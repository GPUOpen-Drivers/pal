/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/

#include "core/hw/gfxip/gfx9/gfx9BorderColorPalette.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9ColorBlendState.h"
#include "core/hw/gfxip/gfx9/gfx9ColorTargetView.h"
#include "core/hw/gfxip/gfx9/gfx9ComputePipeline.h"
#include "core/hw/gfxip/gfx9/gfx9DepthStencilState.h"
#include "core/hw/gfxip/gfx9/gfx9DepthStencilView.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9FormatInfo.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9IndirectCmdGenerator.h"
#include "core/hw/gfxip/gfx9/gfx9MsaaState.h"
#include "core/hw/gfxip/gfx9/gfx9PerfExperiment.h"
#include "core/hw/gfxip/gfx9/gfx9UniversalCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9UserDataTableImpl.h"
#include "core/hw/gfxip/queryPool.h"
#include "core/settingsLoader.h"
#include "palMath.h"
#include "palIntervalTreeImpl.h"
#include "palVectorImpl.h"

#include <float.h>
#include <limits.h>
#include <type_traits>

using namespace Util;
using namespace Pal::Formats;
using namespace Pal::Formats::Gfx9;

using std::is_same;

namespace Pal
{
namespace Gfx9
{

// Microcode version for CE dump offset support
static constexpr uint32 UcodeVersionWithDumpOffsetSupport = 30;

// Microcode version for NGG Indexed Indirect Draw support.
constexpr uint32 UcodeVersionNggIndexedIndirectDraw = 34;

// Lookup table for converting between IndexType and VGT_INDEX_TYPE enums.
constexpr uint32 VgtIndexTypeLookup[] =
{
    VGT_INDEX_8,    // IndexType::Idx8
    VGT_INDEX_16,   // IndexType::Idx16
    VGT_INDEX_32    // IndexType::Idx32
};

// Structure used to convert the "c" value (a combination of various states) to the appropriate deferred-batch
// binning sizes for those states.  Two of these structs define one "range" of "c" values.
struct CtoBinSize
{
    uint32  cStart;
    uint32  binSizeX;
    uint32  binSizeY;
};

// =====================================================================================================================
// Returns the entry in the pBinSize table that corresponds to "c".  It is the caller's responsibility to verify that
// "c" can be found in the table.  If not, this routine could get into an infinite loop.
static const CtoBinSize* GetBinSizeValue(
    const CtoBinSize*  pBinSizeTable, // bin-size table for the # SEs that correspond to this asic
    uint32             c)             // see the Deferred batch binning docs, section 8
{
    bool    cRangeFound               = false;
    uint32  idx                       = 0;
    const   CtoBinSize* pBinSizeEntry = nullptr;

    while (cRangeFound == false)
    {
        const auto*  pNextBinSizeEntry = &pBinSizeTable[idx + 1];

        pBinSizeEntry = &pBinSizeTable[idx];

        if ((c >= pBinSizeEntry->cStart) && (c < pNextBinSizeEntry->cStart))
        {
            // Ok, we found the right range,
            cRangeFound = true;
        }
        else
        {
            // Move onto the next entry in the table
            idx++;
        }
    }

    return pBinSizeEntry;
}

// =====================================================================================================================
// Handle CE - DE synchronization before dumping from CE RAM to ring buffer instance.
// Returns true if this ring will wrap on the next dump.
bool HandleCeRinging(
    UniversalCmdBufferState* pState,
    uint32                   currRingPos,
    uint32                   ringInstances,
    uint32                   ringSize)
{
    // Detect when we're about to wrap to the beginning of the ring buffer.
    // Using ((currRingPos + ringInstances) > ringSize) is optimal for performance. However, it has an issue. Assume
    // ringInstances = 1, ringSize = 1024, the sequence of currRingPos from Client should be:
    //     0, 1, 2, ..., 1023, 1024, 1, ...
    // instead of
    //     0, 1, 2, ..., 1023,    0, 1, ...
    // this requirement is against common sense and error prone. It also prohibits a client from directly using a
    // local copy of currRingPos to reference its data structure array.
    const bool isWrapping = ((currRingPos + ringInstances) >= ringSize);

    if (isWrapping)
    {
        pState->flags.ceHasAnyRingWrapped = 1;
    }

    // If *ANY* ring managed by the CE has wrapped inside this command buffer (including the spill table ring,
    // as well as any client-owned rings), we may need to add additional synchronization to prevent the CE from
    // running too far ahead and to prevent the shaders from reading stale user-data entries from the Kcache.
    if (pState->flags.ceHasAnyRingWrapped != 0)
    {
        const uint32 quarterRingSize = (ringSize / 4);

        const uint32 nextRingPos = (currRingPos + ringInstances) % ringSize;

        // UDX and the CE programming guide both recommend that we stall the CE so that it gets no further ahead
        // of the DE than 1/4 the size of the smallest CE-managed ring buffer. Furthermore, we only need to stall
        // the CE each 1/4 of the way through the smallest ring being managed.
        const uint32 currRingQuadrant = RoundUpToMultiple(currRingPos, quarterRingSize);
        const uint32 nextRingQuadrant = RoundUpToMultiple(nextRingPos, quarterRingSize);

        if (currRingQuadrant != nextRingQuadrant)
        {
            pState->flags.ceWaitOnDeCounterDiff = 1;
        }

        pState->minCounterDiff = Min(pState->minCounterDiff, quarterRingSize);

        // Furthermore, we don't want the shader cores reading stale user-data entries from the Kcache. This can
        // happen because the CE RAM dumps to memory go through the L2 cache, but the shaders read the user-data
        // through the Kcache (L1). After the detected ring wrap, when we reach the halfway point or the end
        // of any ring, we must invalidate the Kcache on the DE while waiting for the CE counter.
        if ((nextRingPos % (ringSize / 2)) == 0)
        {
            pState->flags.ceInvalidateKcache = 1;
        }
    }

    return isWrapping;
}

// =====================================================================================================================
// Helper function which computes the NUM_RECORDS field of a buffer SRD used for a stream-output target.
static uint32 PAL_INLINE StreamOutNumRecords(
    const GpuChipProperties& chipProps,
    uint32                   strideInBytes)
{
    // NOTE: As mentioned in the SC interface for GFX6+ hardware, it is SC's responsibility to handle stream output
    // buffer overflow clamping. SC does this by using an invalid write index for the store instruction.
    //
    // Example: if there are 5 threads streaming out to a buffer which can only hold 3 vertices, the VGT will set the
    // number of threads which will stream data out (strmout_vtx_count) to 3. SC adds instructions to clamp the writes
    // as below:
    //
    // if (strmout_vtx_count > thread_id)
    //     write_index = strmout_write_index (starting index in the SO buffer for this wave)
    // else
    //     write_index = 0xFFFFFFC0
    //
    // The TA block adds the thread_id to the write_index during address calculations for the buffer exports. There is a
    // corner case when all threads are streaming out, the write_index may overflow and no clamping occurs. The
    // "workaround" for this, we account for the maximum thread_id in a wavefront when computing  the clamping value in
    // the stream-out SRD.

    return ((UINT_MAX - chipProps.gfx9.wavefrontSize) + 1);
}

// =====================================================================================================================
size_t UniversalCmdBuffer::GetSize(
    const Device& device)
{
    size_t bytes = sizeof(UniversalCmdBuffer);

    // NOTE: Because universal command buffers use embedded data to manage the client's indirect user-data tables
    // during indirect command generation, we need to track their contents along with the command buffer's state.
    // Since the sizes of these tables is dynamic and the client configures them at run-time, we will store them
    // immediately following the command buffer object itself in memory.
    for (uint32 tableId = 0; tableId < MaxIndirectUserDataTables; ++tableId)
    {
        bytes += (sizeof(uint32) * device.Parent()->IndirectUserDataTableSize(tableId));
    }

    return bytes;
}

// =====================================================================================================================
UniversalCmdBuffer::UniversalCmdBuffer(
    const Device&              device,
    const CmdBufferCreateInfo& createInfo)
    :
    Pal::UniversalCmdBuffer(device,
                            createInfo,
                            &m_prefetchMgr,
                            &m_deCmdStream,
                            &m_ceCmdStream,
                            device.Settings().blendOptimizationsEnable),
    m_device(device),
    m_cmdUtil(device.CmdUtil()),
    m_prefetchMgr(device),
    m_deCmdStream(device,
                  createInfo.pCmdAllocator,
                  GetEngineType(),
                  SubQueueType::Primary,
                  IsNested(),
                  false),
    m_ceCmdStream(device,
                  createInfo.pCmdAllocator,
                  GetEngineType(),
                  SubQueueType::ConstantEngine,
                  IsNested(),
                  false),
    m_pSignatureCs(&NullCsSignature),
    m_pSignatureGfx(&NullGfxSignature),
    m_nggCbCeRamOffset(0),
    m_workaroundState(&device, createInfo.flags.nested, m_state),
    m_vertexOffsetReg(UserDataNotMapped),
    m_drawIndexReg(UserDataNotMapped),
    m_log2NumSes(Log2(m_device.Parent()->ChipProperties().gfx9.numShaderEngines)),
    m_log2NumRbPerSe(Log2(m_device.Parent()->ChipProperties().gfx9.maxNumRbPerSe)),
    m_log2NumSamples(0),
    m_binningMode(FORCE_BINNING_ON), // use a value that we would never actually use
    m_pbbStateOverride(BinningOverride::Default),
    m_enabledPbb(false),
    m_customBinSizeX(0),
    m_customBinSizeY(0),
    m_activeOcclusionQueryWriteRanges(m_device.GetPlatform()),
    m_nestedChunkRefList(m_device.GetPlatform()),
    m_supportsDumpOffsetPacket(
        (m_device.Parent()->EngineProperties().cpUcodeVersion >= UcodeVersionWithDumpOffsetSupport))
{
    const Gfx9PalSettings& settings        = m_device.Settings();
    const auto*const       pPublicSettings = m_device.Parent()->GetPublicSettings();

    memset(&m_indirectUserDataInfo[0],  0, sizeof(m_indirectUserDataInfo));
    memset(&m_spillTable,               0, sizeof(m_spillTable));
    memset(&m_streamOut,                0, sizeof(m_streamOut));
    memset(&m_nggTable,                 0, sizeof(m_nggTable));
    memset(&m_state,                    0, sizeof(m_state));
    memset(&m_paScBinnerCntl0,          0, sizeof(m_paScBinnerCntl0));
    memset(&m_cachedSettings,           0, sizeof(m_cachedSettings));
    memset(&m_drawTimeHwState,          0, sizeof(m_drawTimeHwState));
    memset(&m_primGroupOpt,             0, sizeof(m_primGroupOpt));
    memset(&m_nggState,                 0, sizeof(m_nggState));

    // Setup default engine support - Universal Cmd Buffer supports Graphics, Compute and CPDMA.
    m_engineSupport = (CmdBufferEngineSupport::Graphics |
                       CmdBufferEngineSupport::Compute  |
                       CmdBufferEngineSupport::CpDma);

    // Setup all of our cached settings checks.
    m_cachedSettings.tossPointMode              = static_cast<uint32>(settings.tossPointMode);
    m_cachedSettings.hiDepthDisabled            = !settings.hiDepthEnable;
    m_cachedSettings.hiStencilDisabled          = !settings.hiStencilEnable;
    m_cachedSettings.disableDfsm                = settings.disableDfsm;
    m_cachedSettings.disableDfsmPsUav           = settings.disableDfsmPsUav;
    m_cachedSettings.disableBatchBinning        = (settings.binningMode == Gfx9DeferredBatchBinDisabled);
    m_cachedSettings.disablePbbPsKill           = settings.disableBinningPsKill;
    m_cachedSettings.disablePbbNoDb             = settings.disableBinningNoDb;
    m_cachedSettings.disablePbbBlendingOff      = settings.disableBinningBlendingOff;
    m_cachedSettings.disablePbbAppendConsume    = settings.disableBinningAppendConsume;
    m_cachedSettings.disableWdLoadBalancing     = (settings.wdLoadBalancingMode == Gfx9WdLoadBalancingDisabled);
    m_cachedSettings.ignoreCsBorderColorPalette = settings.disableBorderColorPaletteBinds;
    m_cachedSettings.blendOptimizationsEnable   = settings.blendOptimizationsEnable;
    m_cachedSettings.outOfOrderPrimsEnable      = static_cast<uint32>(settings.enableOutOfOrderPrimitives);
    m_cachedSettings.nggWdPageFaultWa           = settings.waNggWdPageFault;
    m_cachedSettings.scissorChangeWa            = settings.waMiscScissorRegisterChange;
    m_cachedSettings.checkDfsmEqaaWa =
                     (settings.waDisableDfsmWithEqaa                  &&   // Is the workaround enabled on this GPU?
                      (m_cachedSettings.disableDfsm         == false) &&   // Is DFSM already forced off?
                      (m_cachedSettings.disableBatchBinning == false));    // Is binning enabled?
    m_cachedSettings.batchBreakOnNewPs        = settings.batchBreakOnNewPixelShader;
    m_cachedSettings.padParamCacheSpace       =
            ((pPublicSettings->contextRollOptimizationFlags & PadParamCacheSpace) != 0);

    if (settings.binningMode == Gfx9DeferredBatchBinCustom)
    {
        // The custom bin size setting is encoded as two uint16's.
        m_customBinSizeX = settings.customBatchBinSize >> 16;
        m_customBinSizeY = settings.customBatchBinSize & 0xFFFF;

        PAL_ASSERT(IsPowerOfTwo(m_customBinSizeX) && IsPowerOfTwo(m_customBinSizeY));
    }

    // Because Compute pipelines use a fixed user-data entry mapping, the CS CmdSetUserData callback never changes.
    SwitchCmdSetUserDataFunc(PipelineBindPoint::Compute, &UniversalCmdBuffer::CmdSetUserDataCs);

    if (settings.dynamicPrimGroupEnable)
    {
        m_primGroupOpt.windowSize = settings.dynamicPrimGroupWindowSize;
        m_primGroupOpt.step       = settings.dynamicPrimGroupStep;
        m_primGroupOpt.minSize    = settings.dynamicPrimGroupMin;
        m_primGroupOpt.maxSize    = settings.dynamicPrimGroupMax;
    }
    else
    {
        memset(&m_primGroupOpt, 0, sizeof(m_primGroupOpt));
    }

    const bool sqttEnabled = (settings.gpuProfilerMode > GpuProfilerSqttOff) &&
                             (Util::TestAnyFlagSet(settings.gpuProfilerTraceModeMask, GpuProfilerTraceSqtt));
    m_cachedSettings.issueSqttMarkerEvent = (sqttEnabled || device.GetPlatform()->IsDevDriverProfilingEnabled());

    if (m_cachedSettings.issueSqttMarkerEvent)
    {
        m_funcTable.pfnCmdDispatch                 = CmdDispatch<true>;
        m_funcTable.pfnCmdDispatchIndirect         = CmdDispatchIndirect<true>;
        m_funcTable.pfnCmdDispatchOffset           = CmdDispatchOffset<true>;
    }
    else
    {
        m_funcTable.pfnCmdDispatch                 = CmdDispatch<false>;
        m_funcTable.pfnCmdDispatchIndirect         = CmdDispatchIndirect<false>;
        m_funcTable.pfnCmdDispatchOffset           = CmdDispatchOffset<false>;
    }

    m_paScBinnerCntl0.u32All = 0;
    // Initialize defaults for some of the fields in PA_SC_BINNER_CNTL_0.
    m_savedPaScBinnerCntl0.bits.CONTEXT_STATES_PER_BIN    = settings.binningContextStatesPerBin - 1;
    m_savedPaScBinnerCntl0.bits.PERSISTENT_STATES_PER_BIN = settings.binningPersistentStatesPerBin - 1;
    m_savedPaScBinnerCntl0.bits.FPOVS_PER_BATCH           = settings.binningFpovsPerBatch;
    m_savedPaScBinnerCntl0.bits.OPTIMAL_BIN_SELECTION     = settings.binningOptimalBinSelection;

    SwitchDrawFunctions(false, false);
}

// =====================================================================================================================
// Initializes Gfx9-specific functionality.
Result UniversalCmdBuffer::Init(
    const CmdBufferInternalCreateInfo& internalInfo)
{
    Result result = Pal::UniversalCmdBuffer::Init(internalInfo);

    if (result == Result::Success)
    {
        result = m_deCmdStream.Init();
    }

    if (result == Result::Success)
    {
        result = m_ceCmdStream.Init();
    }

    // Initialize the states for the constant engine GPU ring buffer(s) and indirect user-data table(s):
    if (result == Result::Success)
    {
        const auto&      settings        = m_device.Settings();
        const auto&      chipProps       = m_device.Parent()->ChipProperties();
        const auto*const pPublicSettings = m_device.Parent()->GetPublicSettings();

        const BoundGpuMemory& ceRingGpuMem = m_device.CeRingBufferGpuMem(IsNested());
        if (ceRingGpuMem.IsBound())
        {
            // Partition the CE ring GPU memory allocation to each of the ring buffer(s):
            gpusize baseGpuVirtAddr = ceRingGpuMem.GpuVirtAddr();

            m_spillTable.ring.instanceBytes   = (sizeof(uint32) * chipProps.gfxip.maxUserDataEntries);
            m_spillTable.ring.numInstances    = pPublicSettings->userDataSpillTableRingSize;
            m_spillTable.ring.baseGpuVirtAddr = baseGpuVirtAddr;
            baseGpuVirtAddr += (m_spillTable.ring.instanceBytes * m_spillTable.ring.numInstances);

            m_streamOut.ring.instanceBytes   = sizeof(m_streamOut.srd);
            m_streamOut.ring.numInstances    = pPublicSettings->streamOutTableRingSize;
            m_streamOut.ring.baseGpuVirtAddr = baseGpuVirtAddr;
            baseGpuVirtAddr += (m_streamOut.ring.instanceBytes * m_streamOut.ring.numInstances);

            if (settings.nggMode != Gfx9NggDisabled)
            {
                // NGG constant buffers are 256B aligned.
                baseGpuVirtAddr = Pow2Align(baseGpuVirtAddr, 256U);

                m_nggTable.ring.instanceBytes   = Pow2Align<uint32>(sizeof(Abi::PrimShaderCbLayout), 256U);
                m_nggTable.ring.numInstances    = settings.nggRingSize;
                m_nggTable.ring.baseGpuVirtAddr = baseGpuVirtAddr;

                baseGpuVirtAddr += (m_nggTable.ring.instanceBytes * m_nggTable.ring.numInstances);
            }

            uint32* pIndirectUserDataTables = reinterpret_cast<uint32*>(this + 1);
            for (uint32 id = 0; id < MaxIndirectUserDataTables; ++id)
            {
                m_indirectUserDataInfo[id].pData = pIndirectUserDataTables;
                pIndirectUserDataTables         += m_device.Parent()->IndirectUserDataTableSize(id);

                m_indirectUserDataInfo[id].state.sizeInDwords =
                        static_cast<uint32>(m_device.Parent()->IndirectUserDataTableSize(id));

                m_indirectUserDataInfo[id].ring.instanceBytes   =
                        (sizeof(uint32) * m_indirectUserDataInfo[id].state.sizeInDwords);
                m_indirectUserDataInfo[id].ring.numInstances    =
                        static_cast<uint32>(m_device.Parent()->IndirectUserDataTableRingSize(id));
                m_indirectUserDataInfo[id].ring.baseGpuVirtAddr = baseGpuVirtAddr;
                baseGpuVirtAddr += (m_indirectUserDataInfo[id].ring.instanceBytes *
                                    m_indirectUserDataInfo[id].ring.numInstances);
            }

            const BoundGpuMemory& nestedCeRingGpuMem = m_device.CeRingBufferGpuMem(true);

            if (nestedCeRingGpuMem.IsBound() && m_supportsDumpOffsetPacket)
            {
                // Entire nested command buffer CE memory is used for indirect dumps
                m_nestedIndirectCeDumpTable.ring.baseGpuVirtAddr = nestedCeRingGpuMem.GpuVirtAddr();
                m_nestedIndirectCeDumpTable.ring.currRingPos     = 0;

                // Nested indirect ring must be able to hold the maximum ring instance size amongst all
                // supported ring buffers
                m_nestedIndirectCeDumpTable.ring.instanceBytes = Max(Max(m_spillTable.ring.instanceBytes,
                                                                         m_streamOut.ring.instanceBytes),
                                                                     m_nggTable.ring.instanceBytes);
                for (uint32 id = 0; id < MaxIndirectUserDataTables; ++id)
                {
                    m_nestedIndirectCeDumpTable.ring.instanceBytes =
                        Max(m_nestedIndirectCeDumpTable.ring.instanceBytes,
                            m_indirectUserDataInfo[id].ring.instanceBytes);
                }

                // Divide memory size into even instances
                const uint32 ceRingSize = static_cast<uint32>(nestedCeRingGpuMem.Memory()->Desc().size);
                m_nestedIndirectCeDumpTable.ring.numInstances =
                    ceRingSize / m_nestedIndirectCeDumpTable.ring.instanceBytes;
            }

            // Partition CE RAM to each of the ring table(s):
            // NOTE: The spill tables and stream-output table are taken from PAL-reserved CE RAM space, while the
            // indirect user-data tables are not.

            uint32 ceRamOffset = 0;
            m_spillTable.stateCs.sizeInDwords = chipProps.gfxip.maxUserDataEntries;
            m_spillTable.stateCs.ceRamOffset  = ceRamOffset;
            ceRamOffset += m_spillTable.ring.instanceBytes;

            m_spillTable.stateGfx.sizeInDwords = chipProps.gfxip.maxUserDataEntries;
            m_spillTable.stateGfx.ceRamOffset  = ceRamOffset;
            ceRamOffset += m_spillTable.ring.instanceBytes;

            m_streamOut.state.sizeInDwords = (sizeof(m_streamOut.srd) / sizeof(uint32));
            m_streamOut.state.ceRamOffset  = ceRamOffset;
            ceRamOffset += m_streamOut.ring.instanceBytes;

            if (m_nggTable.ring.instanceBytes != 0)
            {
                m_nggCbCeRamOffset             = ceRamOffset;
                m_nggTable.state.sizeInDwords  = NumBytesToNumDwords(m_nggTable.ring.instanceBytes);
                m_nggTable.state.ceRamOffset   = m_nggCbCeRamOffset;
                ceRamOffset                   += m_nggTable.ring.instanceBytes;
            }

            PAL_ASSERT(ceRamOffset <= ReservedCeRamBytes);

            ceRamOffset = ReservedCeRamBytes;
            for (uint32 id = 0; id < MaxIndirectUserDataTables; ++id)
            {
                m_indirectUserDataInfo[id].state.ceRamOffset = ceRamOffset;
                ceRamOffset += m_indirectUserDataInfo[id].ring.instanceBytes;
            }
        }
        else
        {
            PAL_ASSERT((pPublicSettings->streamOutTableRingSize == 0) &&
                       (pPublicSettings->userDataSpillTableRingSize == 0));
        }

    }

    return result;
}

// =====================================================================================================================
// Resets all of the state tracked by this command buffer
void UniversalCmdBuffer::ResetState()
{
    Pal::UniversalCmdBuffer::ResetState();

    if (UseEmbeddedDataForCeRamDumps())
    {
        m_pfnValidateUserDataTablesCs  = &ValidateComputeUserDataTables<false>;
        m_pfnValidateUserDataTablesGfx = &ValidateGraphicsUserDataTables<false>;
    }
    else
    {
        m_pfnValidateUserDataTablesCs  = &ValidateComputeUserDataTables<true>;
        m_pfnValidateUserDataTablesGfx = &ValidateGraphicsUserDataTables<true>;
    }

    m_vgtDmaIndexType.u32All = 0;
    m_vgtDmaIndexType.bits.SWAP_MODE = VGT_DMA_SWAP_NONE;

    m_spiVsOutConfig.u32All = 0;
    m_spiPsInControl.u32All = 0;
    m_binningMode           = FORCE_BINNING_ON; // set a value that we would never use

    // Reset the command buffer's HWL state tracking
    m_state.flags.u32All   = 0;
    m_state.minCounterDiff = UINT_MAX;

    // Set to an invalid (unaligned) address to indicate that streamout hasn't been set yet, and initialize the SRDs'
    // NUM_RECORDS fields to indicate a zero stream-out stride.
    memset(&m_streamOut.srd[0], 0, sizeof(m_streamOut.srd));
    m_device.SetBaseAddress(&m_streamOut.srd[0], 1);
    for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
    {
        m_device.SetNumRecords(&m_streamOut.srd[i], StreamOutNumRecords(m_device.Parent()->ChipProperties(), 0));
    }

    ResetUserDataRingBuffer(&m_streamOut.ring);
    ResetUserDataRingBuffer(&m_nggTable.ring);
    ResetUserDataTable(&m_streamOut.state);

    // Reset the command buffer's per-draw state objects.
    memset(&m_drawTimeHwState, 0, sizeof(m_drawTimeHwState));

    // The index buffer state starts out in the dirty state.
    m_drawTimeHwState.dirty.indexType       = 1;
    m_drawTimeHwState.dirty.indexBufferBase = 1;
    m_drawTimeHwState.dirty.indexBufferSize = 1;

    // Draw index is an optional VS input which will only be marked dirty if a pipeline is bound which actually
    // uses it.
    m_drawTimeHwState.valid.drawIndex = 1;

    // DB_COUNT_CONTROL register is always valid on a nested command buffer because only some bits are inherited
    // and will be updated if necessary in UpdateDbCountControl.
    if (IsNested())
    {
        m_drawTimeHwState.valid.dbCountControl = 1;
    }

    m_drawTimeHwState.dbCountControl.bits.ZPASS_ENABLE      = 1;
    m_drawTimeHwState.dbCountControl.bits.SLICE_EVEN_ENABLE = 1;
    m_drawTimeHwState.dbCountControl.bits.SLICE_ODD_ENABLE  = 1;

    m_vertexOffsetReg           = UserDataNotMapped;
    m_drawIndexReg              = UserDataNotMapped;
    m_nggState.startIndexReg    = UserDataNotMapped;
    m_nggState.log2IndexSizeReg = UserDataNotMapped;

    m_pSignatureCs  = &NullCsSignature;
    m_pSignatureGfx = &NullGfxSignature;

    ResetUserDataRingBuffer(&m_spillTable.ring);
    ResetUserDataTable(&m_spillTable.stateCs);
    ResetUserDataTable(&m_spillTable.stateGfx);

    for (uint16 id = 0; id < MaxIndirectUserDataTables; ++id)
    {
        ResetUserDataRingBuffer(&m_indirectUserDataInfo[id].ring);
        ResetUserDataTable(&m_indirectUserDataInfo[id].state);
        m_indirectUserDataInfo[id].watermark          = m_indirectUserDataInfo[id].state.sizeInDwords;
        m_indirectUserDataInfo[id].modified           = 0;
        m_indirectUserDataInfo[id].state.gpuVirtAddr  = m_indirectUserDataInfo[id].ring.baseGpuVirtAddr;
        m_indirectUserDataInfo[id].state.gpuAddrDirty = 1;
    }

    // Reset nested indirect dump states
    m_state.flags.useIndirectAddrForCe = (IsNested() && m_supportsDumpOffsetPacket);

    m_state.nestedIndirectRingInstances = 0;

    ResetUserDataRingBuffer(&m_nestedIndirectCeDumpTable.ring);
    ResetUserDataTable(&m_nestedIndirectCeDumpTable.state);

    m_activeOcclusionQueryWriteRanges.Clear();
    m_nestedChunkRefList.Clear();

}

// =====================================================================================================================
// Binds a graphics or compute pipeline to this command buffer.
void UniversalCmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    const GraphicsPipeline* pOldPipeline =
        static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

    Pal::UniversalCmdBuffer::CmdBindPipeline(params);

    if (params.pPipeline != nullptr)
    {
        if (params.pipelineBindPoint == PipelineBindPoint::Graphics)
        {
            m_graphicsState.dynamicGraphicsInfo = params.graphics;

            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
            const auto* pNewPipeline = static_cast<const GraphicsPipeline*>(params.pPipeline);
            pDeCmdSpace = pNewPipeline->WriteShCommands(&m_deCmdStream, pDeCmdSpace, params.graphics);
            m_deCmdStream.CommitCommands(pDeCmdSpace);

            SwitchGraphicsPipeline(pOldPipeline, pNewPipeline);
        }
        else
        {
            m_computeState.dynamicCsInfo = params.cs;

            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
            const auto* pNewPipeline = static_cast<const ComputePipeline*>(params.pPipeline);
            auto&       signature    = pNewPipeline->Signature();
            pDeCmdSpace = pNewPipeline->WriteCommands(&m_deCmdStream, pDeCmdSpace, params.cs);
            if (signature.spillThreshold != NoUserDataSpilling)
            {
                if ((signature.spillThreshold < m_pSignatureCs->spillThreshold) ||
                    (signature.userDataLimit  > m_pSignatureCs->userDataLimit))
                {
                    // The new pipeline has either a lower spill threshold or higher user-data limit than the previous
                    // one. In either case, we need to upload any user data entries which we skipped uploading while
                    // the other pipeline was still bound. This guarantees that the spill table is up-to-date.
                    uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();

                    pCeCmdSpace = FixupUserDataEntriesInCeRam(m_computeState.csUserDataEntries,
                                                              *m_pSignatureCs,
                                                              signature,
                                                              pCeCmdSpace);

                    m_ceCmdStream.CommitCommands(pCeCmdSpace);

                    // NOTE: Both spill tables share the same ring buffer, so when one gets updated, the other must
                    // also. This is because there may be a large series of Dispatches between Draws (or vice-versa),
                    // so if the buffer wraps we need to make sure that both compute and graphics waves don't clobber
                    // each other's spill tables.
                    m_spillTable.stateGfx.contentsDirty = 1;
                }
                else if (m_pSignatureCs->spillThreshold == NoUserDataSpilling)
                {
                    // Compute pipelines always use the same registers for the spill table address, but if the old
                    // pipeline wasn't spilling anything, then the previous Dispatch would not have written the spill
                    // address to the proper registers.
                    m_spillTable.stateCs.gpuAddrDirty = 1;
                }
            }

            for (uint32 id = 0; id < MaxIndirectUserDataTables; ++id)
            {
                const uint16 entryPlusOne = signature.indirectTableAddr[id];
                if ((entryPlusOne != UserDataNotMapped) && (entryPlusOne != m_pSignatureCs->indirectTableAddr[id]))
                {
                    // If this indirect user-data table's GPU address is mapped to a different user-data entry than it was
                    // with the previous pipeline, we need to rewrite the user-data entries at Dispatch time.
                    m_indirectUserDataInfo[id].state.gpuAddrDirty = 1;
                    // Furthermore, if the user-data entry mapped to the indirect table is spilled, then we also need to
                    // mark the spill table as dirty.
                    if ((entryPlusOne - 1) >= signature.spillThreshold)
                    {
                        m_spillTable.stateCs.contentsDirty = 1;
                    }
                }
            }

            // NOTE: Compute pipelines always use a fixed user-data mapping from virtualized entries to physical SPI
            // registers, so we do not need to rewrite any bound user-data entries to the correct registers. Entries
            // which don't fall beyond the spill threshold are always written to registers in CmdSetUserDataCs().

            m_pSignatureCs = &signature;
            m_deCmdStream.CommitCommands(pDeCmdSpace);
        }

        auto*const pNewPipeline = static_cast<const Pipeline*>(params.pPipeline);

        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
        pDeCmdSpace = pNewPipeline->RequestPrefetch(m_prefetchMgr, pDeCmdSpace);
        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }
    else if (params.pipelineBindPoint == PipelineBindPoint::Compute)
    {
        m_pSignatureCs = &NullCsSignature;
        m_computeState.dynamicCsInfo = params.cs;
    }
    else
    {
        m_pSignatureGfx = &NullGfxSignature;
        m_graphicsState.dynamicGraphicsInfo = params.graphics;
        SwitchCmdSetUserDataFunc(params.pipelineBindPoint, &Pal::UniversalCmdBuffer::CmdSetUserDataGfx);
    }
}

// =====================================================================================================================
// Updates the graphics state with a new pipeline and performs any extra work due to the pipeline switch. This DOES NOT
// write the pipeline's PM4 image to the DE command stream.
void UniversalCmdBuffer::SwitchGraphicsPipeline(
    const GraphicsPipeline* pOldPipeline,
    const GraphicsPipeline* pNewPipeline)
{
    PAL_ASSERT(pNewPipeline != nullptr);

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    if (pNewPipeline->IsNgg())
    {
        pDeCmdSpace = m_workaroundState.SwitchToNggPipeline(pOldPipeline,
                                                            pNewPipeline->UsesOffchipParamCache(),
                                                            &m_deCmdStream,
                                                            pDeCmdSpace);
    }

    if ((pOldPipeline == nullptr) || (pOldPipeline->GetContextPm4ImgHash() != pNewPipeline->GetContextPm4ImgHash()))
    {
        pDeCmdSpace = pNewPipeline->WriteContextCommands(&m_deCmdStream, pDeCmdSpace);
        m_deCmdStream.SetContextRollDetected<true>();
    }

    if (m_cachedSettings.batchBreakOnNewPs &&
        ((pOldPipeline == nullptr) ||
         (ShaderHashesEqual(pOldPipeline->GetInfo().shader[static_cast<uint32>(ShaderType::Pixel)].hash,
                            pNewPipeline->GetInfo().shader[static_cast<uint32>(ShaderType::Pixel)].hash) == false)))
    {
        pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(BREAK_BATCH, EngineTypeUniversal, pDeCmdSpace);
    }

    // Get new pipeline state VS/PS registers
    regSPI_VS_OUT_CONFIG spiVsOutConfig = pNewPipeline->SpiVsOutConfig();
    regSPI_PS_IN_CONTROL spiPsInControl = pNewPipeline->SpiPsInControl();

    // To reduce context rolls due to pipeline state switches the command buffer tracks VS export count and
    // the PS interpolant count and only sets these registers when the maximum value increases. This heuristic
    // pads the actual parameter cache space required for VS/PS to avoid context rolls.
    if (m_cachedSettings.padParamCacheSpace)
    {
        spiVsOutConfig.bits.VS_EXPORT_COUNT =
            Max(m_spiVsOutConfig.bits.VS_EXPORT_COUNT, spiVsOutConfig.bits.VS_EXPORT_COUNT);

        spiPsInControl.bits.NUM_INTERP =
            Max(m_spiPsInControl.bits.NUM_INTERP, spiPsInControl.bits.NUM_INTERP);
    }

    // Write VS_OUT_CONFIG if the register changed or this is the first pipeline switch
    if ((pOldPipeline == nullptr) || (m_spiVsOutConfig.u32All != spiVsOutConfig.u32All))
    {
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmSPI_VS_OUT_CONFIG, spiVsOutConfig.u32All, pDeCmdSpace);
        m_spiVsOutConfig = spiVsOutConfig;
    }

    // Write PS_IN_CONTROL if the register changed or this is the first pipeline switch
    if ((pOldPipeline == nullptr) || (m_spiPsInControl.u32All != spiPsInControl.u32All))
    {
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmSPI_PS_IN_CONTROL, spiPsInControl.u32All, pDeCmdSpace);
        m_spiPsInControl = spiPsInControl;
    }

    auto& signature = pNewPipeline->Signature();
    bool  isNgg     = pNewPipeline->IsNgg();

    if (m_state.flags.firstDrawExecuted == 0)
    {
        m_nggState.flags.state.firstPipelineIsNgg   = (isNgg) ? 1 : 0;
        m_nggState.flags.state.firstPipelineOffchip = pNewPipeline->UsesOffchipParamCache();
    }

    bool updateSpillTableInCeRam = false;
    if (isNgg == false)
    {
        if (signature.spillThreshold == NoUserDataSpilling)
        {
            // The new pipeline does not spill any user-data entries, so disable the spilling logic inside
            // CmdSetUserData.
            if (pNewPipeline->IsTessEnabled() && pNewPipeline->IsGsEnabled())
            {
                // GS/tessellation pipeline.  All shader stages are enabled.
                SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics,
                                         &CmdSetUserDataNoSpillTableGfx<true, true, true, true>);
            }
            else if (pNewPipeline->IsTessEnabled())
            {
                // Tessellation pipeline.
                SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics,
                                         &CmdSetUserDataNoSpillTableGfx<true, true, false, true>);
            }
            else if (pNewPipeline->IsGsEnabled())
            {
                // GS pipeline.
                SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics,
                                         &CmdSetUserDataNoSpillTableGfx<true, false, true, true>);
            }
            else
            {
                // VS/PS pipeline.
                SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics,
                                         &CmdSetUserDataNoSpillTableGfx<true, false, false, true>);
            }
        }
        else
        {
            if (pNewPipeline->IsTessEnabled() && pNewPipeline->IsGsEnabled())
            {
                // GS/tessellation pipeline.  All shader stages are enabled.
                SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics,
                                         &CmdSetUserDataWithSpillTableGfx<true, true, true>);
            }
            else if (pNewPipeline->IsTessEnabled())
            {
                // Tessellation pipeline.
                SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics,
                                         &CmdSetUserDataWithSpillTableGfx<true, true, false>);
            }
            else if (pNewPipeline->IsGsEnabled())
            {
                // GS pipeline.
                SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics,
                                         &CmdSetUserDataWithSpillTableGfx<true, false, true>);
            }
            else
            {
                // VS/PS pipeline.
                SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics,
                                         &CmdSetUserDataWithSpillTableGfx<true, false, false>);
            }

            updateSpillTableInCeRam = ((signature.spillThreshold < m_pSignatureGfx->spillThreshold) ||
                                       (signature.userDataLimit  > m_pSignatureGfx->userDataLimit));
        }
    }
    else
    {
        if (signature.spillThreshold == NoUserDataSpilling)
        {
            // The new pipeline does not spill any user-data entries, so disable the spilling logic inside
            // CmdSetUserData.
            if (pNewPipeline->IsTessEnabled() == false)
            {
                // For NGG, both VsPs and Gs pipelines have only GsPs stages.
                SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics,
                                         &CmdSetUserDataNoSpillTableGfx<false, false, true, true>);
            }
            else
            {
                // For NGG, both Tess and GsTess have surface, primitive, and pixel shader stages.
                SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics,
                                         &CmdSetUserDataNoSpillTableGfx<false, true, true, true>);
            }
        }
        else
        {
            // The new pipeline does not spill any user-data entries, so disable the spilling logic inside
            // CmdSetUserData.
            if (pNewPipeline->IsTessEnabled() == false)
            {
                // For NGG, both VsPs and Gs pipelines have only GsPs stages.
                SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics,
                                         &CmdSetUserDataWithSpillTableGfx<false, false, true>);
            }
            else
            {
                // For NGG, both Tess and GsTess have surface, primitive, and pixel shader stages.
                SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics,
                                         &CmdSetUserDataWithSpillTableGfx<false, true, true>);
            }

            updateSpillTableInCeRam = ((signature.spillThreshold < m_pSignatureGfx->spillThreshold) ||
                                       (signature.userDataLimit  > m_pSignatureGfx->userDataLimit));
        }

        // We need to update the primitive shader constant buffer with this new pipeline.
        // We'll determine if the actual constant buffer needs to be updated at draw time.
        pNewPipeline->UpdateNggPrimCb(&m_state.primShaderCbLayout.pipelineStateCb);

        // This command buffer now contains a prim shader (NGG) workload.
        SetPrimShaderWorkload();
    }

    const bool newViewInstancingEnable =        signature.viewIdRegAddr[0] != UserDataNotMapped;
    const bool oldViewInstancingEnable = m_pSignatureGfx->viewIdRegAddr[0] != UserDataNotMapped;
    const bool isViewIdEnableChanging  = newViewInstancingEnable != oldViewInstancingEnable;

    // NGG Fast Launch pipelines require issuing different packets for indexed draws. We'll need to switch the
    // draw function pointers around to handle this case.
    if ((pOldPipeline == nullptr) ||
        isViewIdEnableChanging    ||
        (pOldPipeline->IsNggFastLaunch() != pNewPipeline->IsNggFastLaunch()))
    {
        SwitchDrawFunctions(newViewInstancingEnable, pNewPipeline->IsNggFastLaunch());
    }

    if ((signature.spillThreshold != NoUserDataSpilling) || pNewPipeline->UsesStreamOut())
    {
        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();

        if (updateSpillTableInCeRam)
        {
            // The new pipeline has either a lower spill threshold or higher user-data limit than the previous
            // one. In either case, we need to upload any user data entries which we skipped uploading while the
            // other pipeline was still bound. This guarantees that the spill table is up-to-date.
            pCeCmdSpace = FixupUserDataEntriesInCeRam(m_graphicsState.gfxUserDataEntries,
                                                      *m_pSignatureGfx,
                                                      signature,
                                                      pCeCmdSpace);

            // NOTE: Both spill tables share the same ring buffer, so when one gets updated, the other must also.
            // This is because there may be a large series of Dispatches between Draws (or vice-versa), so if the
            // buffer wraps we need to make sure that both compute and graphics waves don't clobber each other's
            // spill tables.
            m_spillTable.stateCs.contentsDirty = 1;
        }
        else
        {
            // NOTE: Even if the new pipeline's spill threshold isn't lower than the old pipeline's, it is still
            // possible for some of the pipelines' shader stages to use different registers for the spill table
            // address. Therefore, we must mark the GPU address dirty so it will be validated prior to the next Draw.
            m_spillTable.stateGfx.gpuAddrDirty = 1;
        }

        if (pNewPipeline->UsesStreamOut())
        {
            // When switching to a pipeline which uses stream output, we need to update the SRD table for any
            // bound stream-output buffers because the SRD's depend on the pipeline's per-buffer vertex strides.
            pCeCmdSpace = UploadStreamOutBufferStridesToCeRam(*pNewPipeline, pCeCmdSpace);
        }

        m_ceCmdStream.CommitCommands(pCeCmdSpace);
    }

    const uint16 streamOutEntryPlusOne = signature.streamOutTableAddr;
    if ((streamOutEntryPlusOne != UserDataNotMapped) &&
        (streamOutEntryPlusOne != m_pSignatureGfx->streamOutTableAddr))
    {
        // If this stream-out SRD table's GPU address is mapped to a different user-data entry than it was with the
        // previous pipeline, we need to rewrite the user-data entries at Dispatch time.
        m_streamOut.state.gpuAddrDirty = 1;
        // Furthermore, if the user-data entry mapped to the stream-out table is spilled, then we also need to
        // mark the spill table as dirty.
        if ((streamOutEntryPlusOne - 1) >= signature.spillThreshold)
        {
            m_spillTable.stateGfx.contentsDirty = 1;
        }
    }

    for (uint32 id = 0; id < MaxIndirectUserDataTables; ++id)
    {
        const uint16 entryPlusOne = signature.indirectTableAddr[id];
        if ((entryPlusOne != UserDataNotMapped) && (entryPlusOne != m_pSignatureGfx->indirectTableAddr[id]))
        {
            // If this indirect user-data table's GPU address is mapped to a different user-data entry than it was
            // with the previous pipeline, we need to rewrite the user-data entries at Dispatch time.
            m_indirectUserDataInfo[id].state.gpuAddrDirty = 1;
            // Furthermore, if the user-data entry mapped to the indirect table is spilled, then we also need to
            // mark the spill table as dirty.
            if ((entryPlusOne - 1) >= signature.spillThreshold)
            {
                m_spillTable.stateGfx.contentsDirty = 1;
            }
        }
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);

    // If the user-data entry mapping between the two pipelines is different, then we need to rewrite the user-
    // data entries to hardware because the pipelines may map different entries to hardware registers.
    FixupUserDataEntriesInRegisters(m_graphicsState.gfxUserDataEntries, signature);

    if (pNewPipeline->UsesMultipleViewports() != (m_graphicsState.enableMultiViewport != 0))
    {
        // If the previously bound pipeline differed in its use of multiple viewports we will need to rewrite the
        // viewport and scissor state on draw.
        if (m_graphicsState.viewportState.count != 0)
        {
            // If viewport is never set, no need to rewrite viewport, this happens in D3D12 nested command list.
            m_graphicsState.dirtyFlags.validationBits.viewports    = 1;
            m_nggState.flags.dirty.viewports                       = 1;
        }

        if (m_graphicsState.scissorRectState.count != 0)
        {
            // If scissor is never set, no need to rewrite scissor, this happens in D3D12 nested command list.
            m_graphicsState.dirtyFlags.validationBits.scissorRects = 1;
            m_nggState.flags.dirty.scissorRects                    = 1;
        }

        m_graphicsState.enableMultiViewport    = pNewPipeline->UsesMultipleViewports();
        m_graphicsState.everUsedMultiViewport |= pNewPipeline->UsesMultipleViewports();
    }

    if (m_vertexOffsetReg != signature.vertexOffsetRegAddr)
    {
        m_vertexOffsetReg = signature.vertexOffsetRegAddr;

        // If the vsUserRegBase setting is changing we must invalidate the instance offset and vertex offset state
        // so that the appropriate user data registers are updated.
        m_drawTimeHwState.valid.instanceOffset = 0;
        m_drawTimeHwState.valid.vertexOffset   = 0;
    }

    if (m_nggState.startIndexReg != signature.startIndexRegAddr)
    {
        m_nggState.startIndexReg = signature.startIndexRegAddr;
        m_drawTimeHwState.valid.indexOffset = 0;
    }
    if (m_nggState.log2IndexSizeReg != signature.log2IndexSizeRegAddr)
    {
        m_nggState.log2IndexSizeReg = signature.log2IndexSizeRegAddr;
        m_drawTimeHwState.valid.log2IndexSize = 0;
    }

    if (m_drawIndexReg != signature.drawIndexRegAddr)
    {
        m_drawIndexReg = signature.drawIndexRegAddr;
        if (m_drawIndexReg != UserDataNotMapped)
        {
            m_drawTimeHwState.valid.drawIndex = 0;
        }
    }

    // On a legacy-to-NGG pipeline or vice versa, we need to make sure that VGT_DMA_INDEX_TYPE::PRIMGEN_EN is set
    // appropriately if the client performs an indexed draw.
    if (m_vgtDmaIndexType.bits.PRIMGEN_EN != static_cast<uint32>(isNgg))
    {
        m_drawTimeHwState.dirty.indexType = 1;
        m_vgtDmaIndexType.bits.PRIMGEN_EN = static_cast<uint32>(isNgg);
    }

    if (m_primGroupOpt.windowSize != 0)
    {
        // Reset the primgroup window state so that we can start gathering data on this new pipeline.
        // Note that we will only enable this optimization for VS/PS pipelines.
        m_primGroupOpt.vtxIdxTotal = 0;
        m_primGroupOpt.drawCount   = 0;
        m_primGroupOpt.optimalSize = 0;
        m_primGroupOpt.enabled = ((pNewPipeline->IsGsEnabled() == false)   &&
                                  (pNewPipeline->IsTessEnabled() == false) &&
                                  (pNewPipeline->UsesStreamOut() == false) &&
                                  (isNgg == false));
    }

    m_pSignatureGfx = &signature;
}

// =====================================================================================================================
// Helper function which fixes-up the user-data entries in the CE RAM copy of the spill table during a pipeline switch.
template <typename PipelineSignature>
uint32* UniversalCmdBuffer::FixupUserDataEntriesInCeRam(
    const UserDataEntries&   entries,
    const PipelineSignature& oldSignature,
    const PipelineSignature& newSignature,
    uint32*                  pCeCmdSpace)
{
    PAL_ASSERT(newSignature.spillThreshold < newSignature.userDataLimit);

    static_assert(is_same<PipelineSignature, ComputePipelineSignature>::value ||
                  is_same<PipelineSignature, GraphicsPipelineSignature>::value,
                  "PipelineSignature type must either be GraphicsPipelineSignature or ComputePipelineSignature!");

    auto*const pSpillTable = (is_same<PipelineSignature, ComputePipelineSignature>::value)
                             ? &m_spillTable.stateCs : &m_spillTable.stateGfx;

        uint32 ramDwordOffset = 0;
        uint32 dwordsToUpload = 0;

        if ((oldSignature.spillThreshold == NoUserDataSpilling) ||
            (newSignature.userDataLimit  <= oldSignature.userDataLimit))
        {
            // If either the previous pipeline did not spill anything, or the new pipeline does not have a higher
            // user-data limit than the old one, we can fixup CE RAM with one WRITE_CONST_RAM packet starting at
            // the new pipeline's spill threshold.
            PAL_ASSERT(newSignature.spillThreshold < oldSignature.spillThreshold);

            ramDwordOffset = newSignature.spillThreshold;
            dwordsToUpload = (oldSignature.spillThreshold == NoUserDataSpilling)
                                ? (newSignature.userDataLimit  - ramDwordOffset)
                                : (oldSignature.spillThreshold - ramDwordOffset);
        }
        else if (newSignature.spillThreshold >= oldSignature.spillThreshold)
        {
            // Alternatively, if the previous pipeline had a lower spill threshold than the new one, we can fixup
            // CE RAM with one WRITE_CONST_RAM packet starting at the old pipeline's user-data limit.
            PAL_ASSERT(newSignature.userDataLimit > oldSignature.userDataLimit);

            ramDwordOffset = oldSignature.userDataLimit;
            dwordsToUpload = (newSignature.userDataLimit - ramDwordOffset);
        }
        else
        {
            // Otherwise, the new pipeline must have a lower spill threshold than the old one, and a higher user-data
            // limit than the old one. In this case, the CE RAM fixup uses two WRITE_CONST_RAM packets: one before the
            // old pipeline's spill threshold, and one after the old pipeline's user-data limit.
            PAL_ASSERT((oldSignature.spillThreshold != NoUserDataSpilling) &&
                       (newSignature.spillThreshold  < oldSignature.spillThreshold) &&
                       (newSignature.userDataLimit   > oldSignature.userDataLimit));

            pCeCmdSpace = UploadToUserDataTableCeRam(m_cmdUtil,
                                                     pSpillTable,
                                                     newSignature.spillThreshold,
                                                     (oldSignature.spillThreshold - newSignature.spillThreshold),
                                                     &entries.entries[newSignature.spillThreshold],
                                                     newSignature.userDataLimit,
                                                     pCeCmdSpace);

            ramDwordOffset = oldSignature.userDataLimit;
            dwordsToUpload = (newSignature.userDataLimit - ramDwordOffset);
        }

        PAL_ASSERT(dwordsToUpload > 0);
        pCeCmdSpace = UploadToUserDataTableCeRam(m_cmdUtil,
                                                 pSpillTable,
                                                 ramDwordOffset,
                                                 dwordsToUpload,
                                                 &entries.entries[ramDwordOffset],
                                                 newSignature.userDataLimit,
                                                 pCeCmdSpace);
    return pCeCmdSpace;
}

// =====================================================================================================================
// Helper function which fixes-up the graphics user-data entries mapped to SPI user-data registers during a graphics
// pipeline switch.
void UniversalCmdBuffer::FixupUserDataEntriesInRegisters(
    const UserDataEntries&           entries,
    const GraphicsPipelineSignature& signature)
{
    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
        UserDataArgs userDataArgs;
        userDataArgs.firstEntry   = 0;
        userDataArgs.entryCount   = signature.userDataLimit;
        userDataArgs.pEntryValues = &entries.entries[0];

        for (uint32 i = 0; i < NumHwShaderStagesGfx; ++i)
        {
            if (m_pSignatureGfx->stage[i].userDataHash != signature.stage[i].userDataHash)
            {
                pDeCmdSpace = m_deCmdStream.WriteUserDataRegisters(signature.stage[i],
                                                                   &userDataArgs,
                                                                   ShaderGraphics,
                                                                   pDeCmdSpace);
            }
        }
    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetMsaaQuadSamplePattern(
    uint32                       numSamplesPerPixel,
    const MsaaQuadSamplePattern& quadSamplePattern)
{
    PAL_ASSERT((numSamplesPerPixel > 0) && (numSamplesPerPixel <= MaxMsaaRasterizerSamples));
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 339
    m_graphicsState.samplePatternState.isLoad                        = false;
    m_graphicsState.samplePatternState.immediate                     = quadSamplePattern;
    m_graphicsState.samplePatternState.pGpuMemory                    = nullptr;
    m_graphicsState.samplePatternState.memOffset                     = 0;
#else
    m_graphicsState.quadSamplePatternState                           = quadSamplePattern;
#endif
    m_graphicsState.numSamplesPerPixel                               = numSamplesPerPixel;
    m_graphicsState.dirtyFlags.validationBits.quadSamplePatternState = 1;

    MsaaSamplePositionsPm4Img samplePosPm4Image = {};

    const CmdUtil& cmdUtil = m_device.CmdUtil();
    MsaaState::BuildSamplePosPm4Image(cmdUtil,
                                      &samplePosPm4Image,
                                      numSamplesPerPixel,
                                      quadSamplePattern);

    // Build and write register for MAX_SAMPLE_DIST
    regPA_SC_AA_CONFIG paScAaConfig   = {};
    uint32* pDeCmdSpace               = m_deCmdStream.ReserveCommands();
    paScAaConfig.bits.MAX_SAMPLE_DIST = MsaaState::ComputeMaxSampleDistance(numSamplesPerPixel,
                                                                            quadSamplePattern);
    pDeCmdSpace = m_deCmdStream.WriteContextRegRmw(mmPA_SC_AA_CONFIG,
                                                   static_cast<uint32>(PA_SC_AA_CONFIG__MAX_SAMPLE_DIST_MASK),
                                                   paScAaConfig.u32All,
                                                   pDeCmdSpace);

    // Write MSAA quad sample pattern registers
    pDeCmdSpace = m_deCmdStream.WritePm4Image(samplePosPm4Image.spaceNeeded, &samplePosPm4Image, pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 339
// =====================================================================================================================
void UniversalCmdBuffer::CmdStoreMsaaQuadSamplePattern(
    const IGpuMemory&            dstGpuMemory,
    gpusize                      dstMemOffset,
    uint32                       numSamplesPerPixel,
    const MsaaQuadSamplePattern& quadSamplePattern)
{
    size_t centroidPrioritiesHdrSize = 0;
    size_t quadSamplePatternHdrSize  = 0;

    MsaaSamplePositionsPm4Img samplePosPm4Image = {};

    const CmdUtil& cmdUtil = m_device.CmdUtil();

    MsaaState::BuildSamplePosPm4Image(cmdUtil,
                                      &samplePosPm4Image,
                                      numSamplesPerPixel,
                                      quadSamplePattern,
                                      &centroidPrioritiesHdrSize,
                                      &quadSamplePatternHdrSize);

    gpusize dstGpuMemoryAddr = dstGpuMemory.Desc().gpuVirtAddr + dstMemOffset;
    PAL_ASSERT((dstGpuMemoryAddr != 0) && ((dstGpuMemoryAddr & 0x3) == 0));

    // Only the low 16 bits of addrOffset are honored for the high portion of the GPU virtual address!
    PAL_ASSERT((HighPart(dstGpuMemoryAddr) & 0xFFFF0000) == 0);

    constexpr size_t CentroidPriorityRegsDwords = 2;
    constexpr uint32 NumCentroidPriorityRegs = 2;

    PAL_ASSERT(centroidPrioritiesHdrSize == (CentroidPriorityRegsDwords + CmdUtil::ContextRegSizeDwords));

    uint32 centroidPriorityRegisters[NumCentroidPriorityRegs];
    centroidPriorityRegisters[0] = samplePosPm4Image.paScCentroid.priority0.u32All;
    centroidPriorityRegisters[1] = samplePosPm4Image.paScCentroid.priority1.u32All;

    // Issue a WRITE_DATA command to update the centroidPriority data in memory
    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace += m_cmdUtil.BuildWriteData(GetEngineType(),
                                            dstGpuMemoryAddr,
                                            CentroidPriorityRegsDwords,
                                            engine_sel__pfp_write_data__prefetch_parser,
                                            dst_sel__me_write_data__memory,
                                            wr_confirm__me_write_data__wait_for_write_confirmation,
                                            &centroidPriorityRegisters[0],
                                            PredDisable,
                                            pDeCmdSpace);

    dstGpuMemoryAddr += sizeof(centroidPriorityRegisters);
    PAL_ASSERT((dstGpuMemoryAddr & 0x3) == 0);

    constexpr size_t SampleLocationsRegsDwords = 16;
    constexpr uint32 NumSampleLocationRegs = 16;
    constexpr uint32 NumSampleQuadRegs = 4;

    PAL_ASSERT(quadSamplePatternHdrSize == (SampleLocationsRegsDwords + CmdUtil::ContextRegSizeDwords));

    uint32 sampleLocationRegisters[NumSampleLocationRegs];

    memcpy(&sampleLocationRegisters[0], samplePosPm4Image.paScSampleQuad.X0Y0,
        sizeof(samplePosPm4Image.paScSampleQuad.X0Y0));
    memcpy(&sampleLocationRegisters[NumSampleQuadRegs], samplePosPm4Image.paScSampleQuad.X1Y0,
        sizeof(samplePosPm4Image.paScSampleQuad.X1Y0));
    memcpy(&sampleLocationRegisters[NumSampleQuadRegs * 2], samplePosPm4Image.paScSampleQuad.X0Y1,
        sizeof(samplePosPm4Image.paScSampleQuad.X0Y1));
    memcpy(&sampleLocationRegisters[NumSampleQuadRegs * 3], samplePosPm4Image.paScSampleQuad.X1Y1,
        sizeof(samplePosPm4Image.paScSampleQuad.X1Y1));

    // Issue a WRITE_DATA command to update the sample locations in memory
    pDeCmdSpace += m_cmdUtil.BuildWriteData(GetEngineType(),
                                            dstGpuMemoryAddr,
                                            SampleLocationsRegsDwords,
                                            engine_sel__pfp_write_data__prefetch_parser,
                                            dst_sel__me_write_data__memory,
                                            wr_confirm__me_write_data__wait_for_write_confirmation,
                                            &sampleLocationRegisters[0],
                                            PredDisable,
                                            pDeCmdSpace);

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdLoadMsaaQuadSamplePattern(
    const IGpuMemory* pSrcGpuMemory,
    gpusize           srcMemOffset)
{
    m_graphicsState.samplePatternState.isLoad                        = true;
    m_graphicsState.samplePatternState.pGpuMemory                    = pSrcGpuMemory;
    m_graphicsState.samplePatternState.memOffset                     = srcMemOffset;
    m_graphicsState.dirtyFlags.validationBits.quadSamplePatternState = 1;

    LoadDataIndexPm4Img loadCentroidPriorityPm4Img  = {};
    LoadDataIndexPm4Img loadQuadSamplePatternPm4Img = {};

    const CmdUtil& cmdUtil = m_device.CmdUtil();

    // If we use legacy LOAD_CONTEXT_REG packet to load the centroid priority and sample pattern registers, we need
    // to subtract the register offset for the LOAD packet from the address we specify to account for the fact that
    // the CP uses that register offset for both the register address and to compute the final GPU address to
    // fetch from. The newer LOAD_CONTEXT_REG_INDEX packet does not add the register offset to the GPU address.
    bool usesLoadRegIndexPkt = m_device.Parent()->ChipProperties().gfx9.supportLoadRegIndexPkt != 0;
    gpusize srcGpuMemoryAddr = pSrcGpuMemory->Desc().gpuVirtAddr + srcMemOffset;

    PAL_ASSERT((srcGpuMemoryAddr != 0) && ((srcGpuMemoryAddr & 0x3) == 0));

    // Only the low 16 bits of addrOffset are honored for the high portion of the GPU virtual address!
    PAL_ASSERT((HighPart(srcGpuMemoryAddr) & 0xFFFF0000) == 0);

    uint32 startRegAddr = mmPA_SC_CENTROID_PRIORITY_0;
    uint32 regCount     = (mmPA_SC_CENTROID_PRIORITY_1 - mmPA_SC_CENTROID_PRIORITY_0 + 1);

    gpusize centroidPriorityGpuMemoryAddr = srcGpuMemoryAddr;

    if (usesLoadRegIndexPkt)
    {
        loadCentroidPriorityPm4Img.spaceNeeded = cmdUtil.BuildLoadContextRegsIndex<true>(
            centroidPriorityGpuMemoryAddr,
            startRegAddr,
            regCount,
            &loadCentroidPriorityPm4Img.loadDataIndex);
    }
    else
    {
        centroidPriorityGpuMemoryAddr -= (sizeof(uint32) * (startRegAddr - CONTEXT_SPACE_START));

        loadCentroidPriorityPm4Img.spaceNeeded += cmdUtil.BuildLoadContextRegs(
            centroidPriorityGpuMemoryAddr,
            startRegAddr,
            regCount,
            &loadCentroidPriorityPm4Img.loadData);
    }

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WritePm4Image(loadCentroidPriorityPm4Img.spaceNeeded,
                                              &loadCentroidPriorityPm4Img,
                                              pDeCmdSpace);

    startRegAddr = mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0;
    regCount     = (mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X1Y1_3 - mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0 + 1);

    gpusize quadSamplePatternGpuMemoryAddr = srcGpuMemoryAddr + sizeof(PaScCentroid);

    if (usesLoadRegIndexPkt)
    {
        loadQuadSamplePatternPm4Img.spaceNeeded = cmdUtil.BuildLoadContextRegsIndex<true>(
            quadSamplePatternGpuMemoryAddr,
            startRegAddr,
            regCount,
            &loadQuadSamplePatternPm4Img.loadDataIndex);
    }
    else
    {
        quadSamplePatternGpuMemoryAddr -= (sizeof(uint32) * (startRegAddr - CONTEXT_SPACE_START));

        loadQuadSamplePatternPm4Img.spaceNeeded += cmdUtil.BuildLoadContextRegs(
            quadSamplePatternGpuMemoryAddr,
            startRegAddr,
            regCount,
            &loadQuadSamplePatternPm4Img.loadData);
    }

    pDeCmdSpace = m_deCmdStream.WritePm4Image(loadQuadSamplePatternPm4Img.spaceNeeded,
                                              &loadQuadSamplePatternPm4Img,
                                              pDeCmdSpace);

    // Build and write register for MAX_SAMPLE_DIST
    // PA_SC_AA_CONFIG is partially owned by the MSAA state object, and partially by the MSAA sample positions.
    // In particular, the max sample distance field is owned by the sample pattern, while everything else is
    // owned by the MSAA state. Right now, we handle that with a CONTEXTREGRMW packet. However, there is no RMW
    // version of LOAD_CONTEXT reg, so we will simply set MAX_SAMPLE_DIST to maximun value.
    // For the custom scenario where this function will be used, the application would different sample locations
    // per pixels in the quad. If we assume a uniform distribution is used to pick the sample locations, then the
    // probability of hitting the max distance is 50% for 2x and 100% for 4x and and 200% for 8x.
    regPA_SC_AA_CONFIG paScAaConfig = {};
    paScAaConfig.bits.MAX_SAMPLE_DIST = 8;
    pDeCmdSpace = m_deCmdStream.WriteContextRegRmw(mmPA_SC_AA_CONFIG,
                                                   static_cast<uint32>(PA_SC_AA_CONFIG__MAX_SAMPLE_DIST_MASK),
                                                   paScAaConfig.u32All,
                                                   pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
}
#endif

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetViewports(
    const ViewportParams& params)
{
    const size_t     viewportSize  = (sizeof(params.viewports[0]) * params.count);
    constexpr size_t GuardbandSize = (sizeof(float) * 4);

    m_graphicsState.viewportState.count = params.count;
    memcpy(&m_graphicsState.viewportState.viewports[0],     &params.viewports[0],     viewportSize);
    memcpy(&m_graphicsState.viewportState.horzDiscardRatio, &params.horzDiscardRatio, GuardbandSize);

    m_graphicsState.dirtyFlags.validationBits.viewports = 1;
    m_nggState.flags.dirty.viewports                    = 1;

    // Also set scissor dirty flag here since we need cross-validation to handle the case of scissor regions
    // being greater than the viewport regions.
    m_graphicsState.dirtyFlags.validationBits.scissorRects = 1;
    m_nggState.flags.dirty.scissorRects                    = 1;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetScissorRects(
    const ScissorRectParams& params)
{
    const size_t scissorSize = (sizeof(params.scissors[0]) * params.count);

    m_graphicsState.scissorRectState.count = params.count;
    memcpy(&m_graphicsState.scissorRectState.scissors[0], &params.scissors[0], scissorSize);

    m_graphicsState.dirtyFlags.validationBits.scissorRects = 1;
    m_nggState.flags.dirty.scissorRects                    = 1;
}

// =====================================================================================================================
// Invalidates the HW state of the index base, type and size as necessary. This way, during validation, we don't need
// to check the values, only the valid flag. There is more cost here (less frequent) in order to save cost during
// validation (more frequent).
void UniversalCmdBuffer::CmdBindIndexData(
    gpusize   gpuAddr,
    uint32    indexCount,
    IndexType indexType)
{
    if (m_graphicsState.iaState.indexAddr != gpuAddr)
    {
        m_drawTimeHwState.dirty.indexBufferBase        = 1;
        m_drawTimeHwState.valid.nggIndexBufferBaseAddr = 0;
    }

    if (m_graphicsState.iaState.indexCount != indexCount)
    {
        m_drawTimeHwState.dirty.indexBufferSize = 1;
    }

    if (m_graphicsState.iaState.indexType != indexType)
    {
        m_drawTimeHwState.dirty.indexType     = 1;
        m_drawTimeHwState.valid.log2IndexSize = 0;
        m_vgtDmaIndexType.bits.INDEX_TYPE = VgtIndexTypeLookup[static_cast<uint32>(indexType)];
    }

    // NOTE: This must come last because it updates m_graphicsState.iaState.
    Pal::UniversalCmdBuffer::CmdBindIndexData(gpuAddr, indexCount, indexType);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindMsaaState(
    const IMsaaState* pMsaaState)
{
    const MsaaState*const pNewState = static_cast<const MsaaState*>(pMsaaState);

    if (pNewState != nullptr)
    {
        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
        pDeCmdSpace = pNewState->WriteCommands(&m_deCmdStream, pDeCmdSpace);
        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }

    m_graphicsState.pMsaaState                          = pNewState;
    m_graphicsState.dirtyFlags.validationBits.msaaState = 1;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindColorBlendState(
    const IColorBlendState* pColorBlendState)
{
    const ColorBlendState*const pNewState = static_cast<const ColorBlendState*>(pColorBlendState);

    if (pNewState != nullptr)
    {
        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
        pDeCmdSpace = pNewState->WriteCommands(&m_deCmdStream, pDeCmdSpace);
        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }

    m_graphicsState.pColorBlendState                          = pNewState;
    m_graphicsState.dirtyFlags.validationBits.colorBlendState = 1;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindDepthStencilState(
    const IDepthStencilState* pDepthStencilState)
{
    const DepthStencilState*const pNewState = static_cast<const DepthStencilState*>(pDepthStencilState);

    if (pNewState != nullptr)
    {
        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
        pDeCmdSpace = pNewState->WriteCommands(&m_deCmdStream, pDeCmdSpace);
        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }

    m_graphicsState.pDepthStencilState                          = pNewState;
    m_graphicsState.dirtyFlags.validationBits.depthStencilState = 1;
}

// =====================================================================================================================
// updates setting blend consts and manages dirty state
void UniversalCmdBuffer::CmdSetBlendConst(
    const BlendConstParams& params)
{
    m_graphicsState.blendConstState                              = params;
    m_graphicsState.dirtyFlags.nonValidationBits.blendConstState = 1;

    BlendConstReg pm4Image = {};
    BuildSetBlendConst(params, m_cmdUtil, reinterpret_cast<uint32*>(&pm4Image));

    constexpr size_t Pm4ImageSize = sizeof(BlendConstReg) >> 2;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WritePm4Image(Pm4ImageSize, &pm4Image, pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Builds the packets for setting the blend const in the command space provided.
uint32* UniversalCmdBuffer::BuildSetBlendConst(
    const BlendConstParams& params,
    const CmdUtil&          cmdUtil,
    uint32*                 pCmdSpace)
{
    BlendConstReg* pImage = reinterpret_cast<BlendConstReg*>(pCmdSpace);
    const size_t totalDwords = cmdUtil.BuildSetSeqContextRegs(mmCB_BLEND_RED, mmCB_BLEND_ALPHA, &pImage->header);
    pImage->red.f32All       = params.blendConst[0];
    pImage->green.f32All     = params.blendConst[1];
    pImage->blue.f32All      = params.blendConst[2];
    pImage->alpha.f32All     = params.blendConst[3];

    return pCmdSpace + totalDwords;
}

// =====================================================================================================================
// Sets depth bounds to be applied with depth buffer comparisons
void UniversalCmdBuffer::CmdSetDepthBounds(
    const DepthBoundsParams& params)
{
    m_graphicsState.depthBoundsState                              = params;
    m_graphicsState.dirtyFlags.nonValidationBits.depthBoundsState = 1;

    DepthBoundsStateReg pm4Image = {};
    BuildSetDepthBounds(params, m_cmdUtil, reinterpret_cast<uint32*>(&pm4Image));

    constexpr size_t Pm4ImageSize = sizeof(DepthBoundsStateReg) >> 2;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WritePm4Image(Pm4ImageSize, &pm4Image, pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Builds the packets for setting the depth bounds in the command space provided.
uint32* UniversalCmdBuffer::BuildSetDepthBounds(
    const DepthBoundsParams& params,
    const CmdUtil&           cmdUtil,
    uint32*                  pCmdSpace)
{
    DepthBoundsStateReg* pImage = reinterpret_cast<DepthBoundsStateReg*>(pCmdSpace);
    const size_t totalDwords =
        cmdUtil.BuildSetSeqContextRegs(mmDB_DEPTH_BOUNDS_MIN, mmDB_DEPTH_BOUNDS_MAX, &pImage->header);
    pImage->dbDepthBoundsMin.f32All = params.min;
    pImage->dbDepthBoundsMax.f32All = params.max;

    return pCmdSpace + totalDwords;
}

// =====================================================================================================================
// Sets the current input assembly state
void UniversalCmdBuffer::CmdSetInputAssemblyState(
    const InputAssemblyStateParams& params)
{
    m_graphicsState.inputAssemblyState = params;
    m_graphicsState.dirtyFlags.validationBits.inputAssemblyState = 1;

    InputAssemblyStatePm4Img pm4Image = {};
    BuildSetInputAssemblyState(params, m_device, reinterpret_cast<uint32*>(&pm4Image));
    m_state.primShaderCbLayout.renderStateCb.primitiveRestartIndex = params.primitiveRestartIndex;
    m_state.primShaderCbLayout.pipelineStateCb.vgtPrimitiveType    = pm4Image.primType.u32All;
    m_nggState.flags.dirty.inputAssemblyState                      = 1;

    constexpr size_t Pm4ImageSize = sizeof(InputAssemblyStatePm4Img) >> 2;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WritePm4Image(Pm4ImageSize, &pm4Image, pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Builds the packets for setting the input assembly stae in the command space provided.
uint32* UniversalCmdBuffer::BuildSetInputAssemblyState(
    const InputAssemblyStateParams& params,
    const Device&                   device,
    uint32*                         pCmdSpace)
{
    constexpr VGT_DI_PRIM_TYPE TopologyToPrimTypeTbl[] =
    {
        DI_PT_POINTLIST,        // PointList
        DI_PT_LINELIST,         // LineList
        DI_PT_LINESTRIP,        // LineStrip
        DI_PT_TRILIST,          // TriangleList
        DI_PT_TRISTRIP,         // TriangleStrip
        DI_PT_RECTLIST,         // RectList
        DI_PT_QUADLIST,         // QuadList
        DI_PT_QUADSTRIP,        // QuadStrip
        DI_PT_LINELIST_ADJ,     // LineListAdj
        DI_PT_LINESTRIP_ADJ,    // LineStripAdj
        DI_PT_TRILIST_ADJ,      // TriangleListAdj
        DI_PT_TRISTRIP_ADJ,     // TriangleStripAdj
        DI_PT_PATCH,            // Patch
        DI_PT_TRIFAN,           // TriangleFan
    };

    size_t                          totalDwords   = 0;
    InputAssemblyStatePm4Img*       pImage        = reinterpret_cast<InputAssemblyStatePm4Img*>(pCmdSpace);
    PFP_SET_UCONFIG_REG_index_enum  primTypeIndex = index__pfp_set_uconfig_reg__prim_type;

    totalDwords += device.CmdUtil().BuildSetOneConfigReg(mmVGT_PRIMITIVE_TYPE,
                                                         &pImage->hdrPrimType,
                                                         primTypeIndex);
    totalDwords += device.CmdUtil().BuildSetOneContextReg(mmVGT_MULTI_PRIM_IB_RESET_INDX,
                                                          &pImage->hdrVgtMultiPrimIbResetIndex);

    // Initialise register data
    pImage->primType.u32All = 0;
    pImage->primType.bits.PRIM_TYPE = TopologyToPrimTypeTbl[static_cast<uint32>(params.topology)];

    pImage->vgtMultiPrimIbResetIndex.u32All = 0;
    pImage->vgtMultiPrimIbResetIndex.bits.RESET_INDX = params.primitiveRestartIndex;

    return pCmdSpace + totalDwords;
}

// =====================================================================================================================
// Sets bit-masks to be applied to stencil buffer reads and writes.
void UniversalCmdBuffer::CmdSetStencilRefMasks(
    const StencilRefMaskParams& params)
{
    uint32 pm4Image[MaxStencilSetPm4ImgSize / sizeof(uint32)];

    const uint32*const pPm4ImgEnd = BuildSetStencilRefMasks(params, m_cmdUtil, pm4Image);
    const size_t       pm4ImgSize = pPm4ImgEnd - pm4Image;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WritePm4Image(pm4ImgSize, pm4Image, pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);

    SetStencilRefMasksState(params, &m_graphicsState.stencilRefMaskState);
    m_graphicsState.dirtyFlags.nonValidationBits.stencilRefMaskState = 1;
}

// =====================================================================================================================
// Builds the packets for setting the stencil refs and masks in the command space provided.
uint32* UniversalCmdBuffer::BuildSetStencilRefMasks(
    const StencilRefMaskParams& params,
    const CmdUtil&              cmdUtil,
    uint32*                     pCmdSpace)
{
    uint8 flags = params.flags.u8All;
    if (flags == 0xFF)
    {
        StencilRefMasksReg* pImage = reinterpret_cast<StencilRefMasksReg*>(pCmdSpace);

        pCmdSpace += cmdUtil.BuildSetSeqContextRegs(mmDB_STENCILREFMASK, mmDB_STENCILREFMASK_BF, &pImage->header);

        pImage->dbStencilRefMaskFront.bitfields.STENCILOPVAL     = params.frontOpValue;
        pImage->dbStencilRefMaskFront.bitfields.STENCILTESTVAL   = params.frontRef;
        pImage->dbStencilRefMaskFront.bitfields.STENCILMASK      = params.frontReadMask;
        pImage->dbStencilRefMaskFront.bitfields.STENCILWRITEMASK = params.frontWriteMask;

        pImage->dbStencilRefMaskBack.bitfields.STENCILOPVAL_BF     = params.backOpValue;
        pImage->dbStencilRefMaskBack.bitfields.STENCILTESTVAL_BF   = params.backRef;
        pImage->dbStencilRefMaskBack.bitfields.STENCILMASK_BF      = params.backReadMask;
        pImage->dbStencilRefMaskBack.bitfields.STENCILWRITEMASK_BF = params.backWriteMask;
    }
    else
    {
        uint32 accumFrontMask = 0;
        uint32 accumFrontData = 0;
        uint32 accumBackMask  = 0;
        uint32 accumBackData  = 0;
        StencilRefMaskRmwReg* pImage = reinterpret_cast<StencilRefMaskRmwReg*>(pCmdSpace);

        // Accumulate masks and shifted data based on which flags are set
        // 1. Front-facing primitives
        if (params.flags.updateFrontRef)
        {
            accumFrontMask |= DB_STENCILREFMASK__STENCILTESTVAL_MASK;
            accumFrontData |= (params.frontRef << DB_STENCILREFMASK__STENCILTESTVAL__SHIFT);
        }
        if (params.flags.updateFrontReadMask)
        {
            accumFrontMask |= DB_STENCILREFMASK__STENCILMASK_MASK;
            accumFrontData |= (params.frontReadMask << DB_STENCILREFMASK__STENCILMASK__SHIFT);
        }
        if (params.flags.updateFrontWriteMask)
        {
            accumFrontMask |= DB_STENCILREFMASK__STENCILWRITEMASK_MASK;
            accumFrontData |= (params.frontWriteMask << DB_STENCILREFMASK__STENCILWRITEMASK__SHIFT);
        }
        if (params.flags.updateFrontOpValue)
        {
            accumFrontMask |= DB_STENCILREFMASK__STENCILOPVAL_MASK;
            accumFrontData |= (params.frontOpValue << DB_STENCILREFMASK__STENCILOPVAL__SHIFT);
        }

        // 2. Back-facing primitives
        if (params.flags.updateBackRef)
        {
            accumBackMask |= DB_STENCILREFMASK_BF__STENCILTESTVAL_BF_MASK;
            accumBackData |= (params.backRef << DB_STENCILREFMASK_BF__STENCILTESTVAL_BF__SHIFT);
        }
        if (params.flags.updateBackReadMask)
        {
            accumBackMask |= DB_STENCILREFMASK_BF__STENCILMASK_BF_MASK;
            accumBackData |= (params.backReadMask << DB_STENCILREFMASK_BF__STENCILMASK_BF__SHIFT);
        }
        if (params.flags.updateBackWriteMask)
        {
            accumBackMask |= DB_STENCILREFMASK_BF__STENCILWRITEMASK_BF_MASK;
            accumBackData |= (params.backWriteMask << DB_STENCILREFMASK_BF__STENCILWRITEMASK_BF__SHIFT);
        }
        if (params.flags.updateBackOpValue)
        {
            accumBackMask |= DB_STENCILREFMASK_BF__STENCILOPVAL_BF_MASK;
            accumBackData |= (params.backOpValue << DB_STENCILREFMASK_BF__STENCILOPVAL_BF__SHIFT);
        }

        pCmdSpace += cmdUtil.BuildContextRegRmw(mmDB_STENCILREFMASK,
                                                accumFrontMask,
                                                accumFrontData,
                                                &pImage->dbStencilRefMaskFront);
        pCmdSpace += cmdUtil.BuildContextRegRmw(mmDB_STENCILREFMASK_BF,
                                                accumBackMask,
                                                accumBackData,
                                                &pImage->dbStencilRefMaskBack);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// CmdSetUserData callback which writes user-data registers and updates the spill table contents (for compute).
void PAL_STDCALL UniversalCmdBuffer::CmdSetUserDataCs(
    Pal::ICmdBuffer* pCmdBuffer,
    uint32           firstEntry,
    uint32           entryCount,
    const uint32*    pEntryValues)
{
    Pal::GfxCmdBuffer::CmdSetUserDataCs(pCmdBuffer, firstEntry, entryCount, pEntryValues);

    auto*const pSelf = static_cast<Gfx9::UniversalCmdBuffer*>(pCmdBuffer);

    uint32 lastEntry = (firstEntry + entryCount - 1);
    PAL_ASSERT(lastEntry < pSelf->m_device.Parent()->ChipProperties().gfxip.maxUserDataEntries);

    if (firstEntry < MaxFastUserDataEntriesCompute)
    {
        const uint16 baseRegister = pSelf->m_device.GetFirstUserDataReg(HwShaderStage::Cs);
        const uint32 lastRegister = (Min(lastEntry, (MaxFastUserDataEntriesCompute - 1)) + baseRegister);

        uint32* pDeCmdSpace = pSelf->m_deCmdStream.ReserveCommands();
        pDeCmdSpace = pSelf->m_deCmdStream.WriteSetSeqShRegs((baseRegister + firstEntry),
                                                             lastRegister,
                                                             ShaderCompute,
                                                             pEntryValues,
                                                             pDeCmdSpace);
        pSelf->m_deCmdStream.CommitCommands(pDeCmdSpace);
    }

    const uint32 threshold     = pSelf->m_pSignatureCs->spillThreshold;
    const uint32 userDataLimit = pSelf->m_pSignatureCs->userDataLimit;

    if ((threshold <= lastEntry) && (userDataLimit > firstEntry))
    {
        // If one or more of the entries being set are spilled to memory by the active pipeline, then we need to copy
        // those entries up to CE RAM.

        if (threshold > firstEntry) // Handle the case where the user-data update straddles the spill threshold!
        {
            const uint32 difference = (threshold - firstEntry);
            firstEntry   += difference;
            entryCount   -= difference;
            pEntryValues += difference;
        }
        if (userDataLimit <= lastEntry) // Handle the case where the user-data update straddles the user-data limit!
        {
            const uint32 difference = (lastEntry + 1 - userDataLimit);
            lastEntry  -= difference;
            entryCount -= difference;
        }
        PAL_ASSERT(lastEntry == (firstEntry + entryCount - 1));

        uint32* pCeCmdSpace = pSelf->m_ceCmdStream.ReserveCommands();

        pCeCmdSpace = UploadToUserDataTableCeRam(pSelf->m_cmdUtil,
                                                 &pSelf->m_spillTable.stateCs,
                                                 firstEntry,
                                                 entryCount,
                                                 pEntryValues,
                                                 userDataLimit,
                                                 pCeCmdSpace);

        pSelf->m_ceCmdStream.CommitCommands(pCeCmdSpace);

        // NOTE: Both spill tables share the same ring buffer, so when one gets updated, the other must also. This
        // is because there may be a large series of Dispatches between Draws (or vice-versa), so if the buffer wraps,
        // we need to make sure that both compute and graphics waves don't clobber each other's spill tables.
        pSelf->m_spillTable.stateGfx.contentsDirty = 1;
    }
}

// =====================================================================================================================
// CmdSetUserData callback which writes remapped user-data registers without using a spill table (for graphics). Writes
// PM4 packets to set the requested user data register(s) to the data provided. All active shader stages' are updated.
template <bool vsEnabled, bool tessEnabled, bool gsEnabled, bool filterRedundantSets>
void PAL_STDCALL UniversalCmdBuffer::CmdSetUserDataNoSpillTableGfx(
    Pal::ICmdBuffer* pCmdBuffer,
    uint32           firstEntry,
    uint32           entryCount,
    const uint32*    pEntryValues)
{
    auto*const pSelf = static_cast<Gfx9::UniversalCmdBuffer*>(pCmdBuffer);
    PAL_ASSERT((firstEntry + entryCount - 1) < pSelf->m_device.Parent()->ChipProperties().gfxip.maxUserDataEntries);

    UserDataArgs userDataArgs;
    userDataArgs.firstEntry   = firstEntry;
    userDataArgs.entryCount   = entryCount;
    userDataArgs.pEntryValues = pEntryValues;

    if ((filterRedundantSets == false) || pSelf->FilterSetUserDataGfx(&userDataArgs))
    {
        Pal::UniversalCmdBuffer::CmdSetUserDataGfx(pCmdBuffer, userDataArgs.firstEntry, userDataArgs.entryCount,
                                                   userDataArgs.pEntryValues);

        uint32* pDeCmdSpace = pSelf->m_deCmdStream.ReserveCommands();

        PAL_ASSERT(pSelf->m_pSignatureGfx != nullptr);

        if (tessEnabled)
        {
            pDeCmdSpace = pSelf->m_deCmdStream.WriteUserDataRegisters(
                              pSelf->m_pSignatureGfx->stage[static_cast<uint32>(HwShaderStage::Hs)],
                              &userDataArgs,
                              ShaderGraphics,
                              pDeCmdSpace);
        }

        if (gsEnabled)
        {
            pDeCmdSpace = pSelf->m_deCmdStream.WriteUserDataRegisters(
                              pSelf->m_pSignatureGfx->stage[static_cast<uint32>(HwShaderStage::Gs)],
                              &userDataArgs,
                              ShaderGraphics,
                              pDeCmdSpace);
        }

        if (vsEnabled)
        {
            pDeCmdSpace = pSelf->m_deCmdStream.WriteUserDataRegisters(
                              pSelf->m_pSignatureGfx->stage[static_cast<uint32>(HwShaderStage::Vs)],
                              &userDataArgs,
                              ShaderGraphics,
                              pDeCmdSpace);
        }

        pDeCmdSpace = pSelf->m_deCmdStream.WriteUserDataRegisters(
                          pSelf->m_pSignatureGfx->stage[static_cast<uint32>(HwShaderStage::Ps)],
                          &userDataArgs,
                          ShaderGraphics,
                          pDeCmdSpace);

        pSelf->m_deCmdStream.CommitCommands(pDeCmdSpace);
    }
}

// =====================================================================================================================
// CmdSetUserData callback which writes remapped user-data registers and updates the spill table contents (for
// graphics).
template <bool vsEnabled, bool tessEnabled, bool gsEnabled>
void PAL_STDCALL UniversalCmdBuffer::CmdSetUserDataWithSpillTableGfx(
    Pal::ICmdBuffer*  pCmdBuffer,
    uint32            firstEntry,
    uint32            entryCount,
    const uint32*     pEntryValues)
{
    auto*const pSelf = static_cast<Gfx9::UniversalCmdBuffer*>(pCmdBuffer);

    UserDataArgs userDataArgs;
    userDataArgs.firstEntry   = firstEntry;
    userDataArgs.entryCount   = entryCount;
    userDataArgs.pEntryValues = pEntryValues;

    if (pSelf->FilterSetUserDataGfx(&userDataArgs))
    {
        firstEntry   = userDataArgs.firstEntry;
        entryCount   = userDataArgs.entryCount;
        pEntryValues = userDataArgs.pEntryValues;

        // This will update the tracked user-data entries API state and write any entries which are mapped to physical
        // registers.
        CmdSetUserDataNoSpillTableGfx<vsEnabled, tessEnabled, gsEnabled, false>(pCmdBuffer,
                                                                                firstEntry,
                                                                                entryCount,
                                                                                pEntryValues);

        uint32 lastEntry = (firstEntry + entryCount - 1);

        const uint32 threshold     = pSelf->m_pSignatureGfx->spillThreshold;
        const uint32 userDataLimit = pSelf->m_pSignatureGfx->userDataLimit;

        if ((threshold <= lastEntry) && (userDataLimit > firstEntry))
        {
            // If one or more of the entries being set are spilled to memory by the active pipeline, then we need to
            // copy those entries up to CE RAM.

            if (threshold > firstEntry) // Handle the case where the user-data update straddles the spill threshold!
            {
                const uint32 difference = (threshold - firstEntry);
                firstEntry   += difference;
                entryCount   -= difference;
                pEntryValues += difference;
            }
            if (userDataLimit <= lastEntry) // Handle the case where the user-data update straddles the user-data limit!
            {
                const uint32 difference = (lastEntry + 1 - userDataLimit);
                lastEntry  -= difference;
                entryCount -= difference;
            }
            PAL_ASSERT(lastEntry == (firstEntry + entryCount - 1));

            uint32* pCeCmdSpace = pSelf->m_ceCmdStream.ReserveCommands();

            pCeCmdSpace = UploadToUserDataTableCeRam(pSelf->m_cmdUtil,
                                                     &pSelf->m_spillTable.stateGfx,
                                                     firstEntry,
                                                     entryCount,
                                                     pEntryValues,
                                                     userDataLimit,
                                                     pCeCmdSpace);

            pSelf->m_ceCmdStream.CommitCommands(pCeCmdSpace);

            // NOTE: Both spill tables share the same ring buffer, so when one gets updated, the other must also. This
            // is because there may be a large series of Dispatches between Draws (or vice-versa), so if the buffer
            // wraps we need to make sure that both compute and graphics waves don't clobber each other's spill tables.
            pSelf->m_spillTable.stateCs.contentsDirty = 1;
        }
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBarrier(
    const BarrierInfo& barrierInfo)
{
    // Barriers do not honor predication.
    const uint32 packetPredicate = m_gfxCmdBufState.packetPredicate;
    m_gfxCmdBufState.packetPredicate = 0;

    m_device.Barrier(this, &m_deCmdStream, barrierInfo);

    m_gfxCmdBufState.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetIndirectUserData(
    uint16      tableId,
    uint32      dwordOffset,
    uint32      dwordSize,
    const void* pSrcData)
{
    PAL_ASSERT(dwordSize > 0);
    PAL_ASSERT((dwordOffset + dwordSize) <= m_indirectUserDataInfo[tableId].state.sizeInDwords);

    // All this method needs to do is update the CPU-side copy of the indirect user-data table and upload the new
    // data to CE RAM. It will be validated at Draw- or Dispatch-time
    memcpy((m_indirectUserDataInfo[tableId].pData + dwordOffset), pSrcData, (sizeof(uint32) * dwordSize));

    uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();

    pCeCmdSpace = UploadToUserDataTableCeRam(m_cmdUtil,
                                             &m_indirectUserDataInfo[tableId].state,
                                             dwordOffset,
                                             dwordSize,
                                             static_cast<const uint32*>(pSrcData),
                                             m_indirectUserDataInfo[tableId].watermark,
                                             pCeCmdSpace);
    m_indirectUserDataInfo[tableId].modified = 1;

    m_ceCmdStream.CommitCommands(pCeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetIndirectUserDataWatermark(
    uint16 tableId,
    uint32 dwordLimit)
{
    PAL_ASSERT(tableId < MaxIndirectUserDataTables);

    dwordLimit = Min(dwordLimit, m_indirectUserDataInfo[tableId].state.sizeInDwords);
    if (dwordLimit > m_indirectUserDataInfo[tableId].watermark)
    {
        // If the current high watermark is increasing, we need to mark the contents as dirty because data which was
        // previously uploaded to CE RAM wouldn't have been dumped to GPU memory before the previous draw or dispatch.
        m_indirectUserDataInfo[tableId].state.contentsDirty = 1;
    }

    m_indirectUserDataInfo[tableId].watermark = dwordLimit;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindTargetsMetadata(
    const BindTargetParams& params)
{
    for (uint32 slot = 0; slot < params.colorTargetCount; slot++)
    {
        const auto*const pColorView = static_cast<const ColorTargetView*>(params.colorTargets[slot].pColorTargetView);

        if (pColorView != nullptr)
        {
            pColorView->UpdateDccStateMetadata(&m_deCmdStream, params.colorTargets[slot].imageLayout);
        }
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindTargets(
    const BindTargetParams& params)
{
    constexpr uint32 AllColorTargetSlotMask = 255; // Mask of all color-target slots.

    bool colorTargetsChanged = false;
    // Under gfx9 we need to wait for F/I to finish when targets may share same metadata cache lines.  Because there is
    // no easy formula for determining this conflict, we'll be conservative and wait on all targets within the Metadata
    // tail since they will share the same block.
    bool waitOnMetadataMipTail = false;

    // Bind all color targets.
    const uint32 colorTargetLimit = Max(params.colorTargetCount, m_graphicsState.bindTargets.colorTargetCount);
    uint32 newColorTargetMask = 0;
    for (uint32 slot = 0; slot < colorTargetLimit; slot++)
    {
        const auto*const pCurrentView =
            static_cast<const ColorTargetView*>(m_graphicsState.bindTargets.colorTargets[slot].pColorTargetView);
        const auto*const pNewView = (slot < params.colorTargetCount)
                                    ? static_cast<const ColorTargetView*>(params.colorTargets[slot].pColorTargetView)
                                    : nullptr;

        if (pNewView != nullptr)
        {
            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
            pDeCmdSpace = pNewView->WriteCommands(slot,
                                                  params.colorTargets[slot].imageLayout,
                                                  &m_deCmdStream,
                                                  pDeCmdSpace);
            m_deCmdStream.CommitCommands(pDeCmdSpace);

            pNewView->UpdateDccStateMetadata(&m_deCmdStream, params.colorTargets[slot].imageLayout);

            // Set the bit means this color target slot is not bound to a NULL target.
            newColorTargetMask |= (1 << slot);
        }

        if ((pCurrentView != nullptr) && (pCurrentView != pNewView))  // view1->view2 or view->null
        {
            colorTargetsChanged = true;
            // Record if this depth view we are switching from should trigger a Release_Mem due to being in the
            // MetaData tail region.
            waitOnMetadataMipTail |= pCurrentView->WaitOnMetadataMipTail();
        }
    }

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    // Bind NULL for all remaining color target slots. We must build the PM4 image on the stack because we must call
    // the command stream's WritePm4Image to keep the PM4 optimizer in the loop.
    if (newColorTargetMask != AllColorTargetSlotMask)
    {
        pDeCmdSpace = WriteNullColorTargets(pDeCmdSpace, newColorTargetMask, m_graphicsState.boundColorTargetMask);
    }
    m_graphicsState.boundColorTargetMask = newColorTargetMask;

    if (colorTargetsChanged)
    {
        // Handle the case where at least one color target view is changing.
        pDeCmdSpace = ColorTargetView::HandleBoundTargetsChanged(m_device, &m_deCmdStream, pDeCmdSpace);
    }

    // Check for DepthStencilView changes
    const auto*const pCurrentDepthView =
        static_cast<const DepthStencilView*>(m_graphicsState.bindTargets.depthTarget.pDepthStencilView);
    const auto*const pNewDepthView = static_cast<const DepthStencilView*>(params.depthTarget.pDepthStencilView);

    // Bind the depth target or NULL if it was not provided.
    if (pNewDepthView != nullptr)
    {
        pDeCmdSpace = pNewDepthView->WriteCommands(params.depthTarget.depthLayout,
                                                   params.depthTarget.stencilLayout,
                                                   &m_deCmdStream,
                                                   pDeCmdSpace);

        if (pNewDepthView->GetImage()->HasWaTcCompatZRangeMetaData())
        {
            // Re-write the ZRANGE_PRECISION value for the waTcCompatZRange workaround. We must include the
            // COND_EXEC which checks the metadata because we don't know the last fast clear value here.
            pDeCmdSpace = pNewDepthView->UpdateZRangePrecision(true, &m_deCmdStream, pDeCmdSpace);
        }
    }
    else
    {
        pDeCmdSpace = WriteNullDepthTarget(pDeCmdSpace);
    }

    if ((pCurrentDepthView != nullptr) && (pCurrentDepthView != pNewDepthView))  // view1->view2 or view->null
    {
        // Handle the case where the depth view is changing.
        pDeCmdSpace = DepthStencilView::HandleBoundTargetChanged(m_device, &m_deCmdStream, pDeCmdSpace);

        // Record if this depth view we are switching from should trigger a Release_Mem due to being in the MetaData
        // tail region.
        waitOnMetadataMipTail |= pCurrentDepthView->WaitOnMetadataMipTail();
    }

    if (waitOnMetadataMipTail)
    {
        pDeCmdSpace += m_cmdUtil.BuildWaitOnReleaseMemEvent(EngineTypeUniversal,
                                                            BOTTOM_OF_PIPE_TS,
                                                            TcCacheOp::Nop,
                                                            TimestampGpuVirtAddr(),
                                                            pDeCmdSpace);
    }
    m_deCmdStream.CommitCommands(pDeCmdSpace);

    // Save updated bindTargets state
    //  For consistancy ensure we only save colorTargets within the valid target count specified, and set
    //  unbound target slots as empty/null.  This allows simple slot change comparisons above and elsewhere.
    //  Handle cases where callers may supply input like:
    //     colorTargetCount=4 {view, null, null,null} --> colorTargetCount=1 {view,null,...}
    //     colorTargetCount=0 {view1,view2,null,null} --> colorTargetCount=0 {null,null,...}
    uint32 updatedColorTargetCount = 0;
    for (uint32 slot = 0; slot < colorTargetLimit; slot++)
    {
        if ((slot < params.colorTargetCount) && (params.colorTargets[slot].pColorTargetView != nullptr))
        {
            m_graphicsState.bindTargets.colorTargets[slot] = params.colorTargets[slot];
            updatedColorTargetCount = slot + 1;  // track last actual bound slot
        }
        else
        {
            m_graphicsState.bindTargets.colorTargets[slot] = {};
        }
    }
    m_graphicsState.bindTargets.colorTargetCount               = updatedColorTargetCount;
    m_graphicsState.bindTargets.depthTarget                    = params.depthTarget;
    m_graphicsState.dirtyFlags.validationBits.colorTargetView  = 1;
    m_graphicsState.dirtyFlags.validationBits.depthStencilView = 1;
    PAL_ASSERT(m_graphicsState.inheritedState.stateFlags.targetViewState == 0);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindStreamOutTargets(
    const BindStreamOutTargetParams& params)
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();
    const auto*const         pPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    for (uint32 idx = 0; idx < MaxStreamOutTargets; ++idx)
    {
        uint32 bufferSize = 0;

        if (params.target[idx].gpuVirtAddr != 0uLL)
        {
            auto*const  pBufferSrd = &m_streamOut.srd[idx];

            bufferSize = LowPart(params.target[idx].size) / sizeof(uint32);
            PAL_ASSERT(HighPart(params.target[idx].size) == 0);

            const uint32 strideInBytes =
                ((pPipeline == nullptr) ? 0 : pPipeline->VgtStrmoutVtxStride(idx).u32All) * sizeof(uint32);

            m_device.SetNumRecords(pBufferSrd, StreamOutNumRecords(chipProps, strideInBytes));

            m_device.InitBufferSrd(pBufferSrd, params.target[idx].gpuVirtAddr, strideInBytes);
            if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
            {
                auto*const  pSrd = &pBufferSrd->gfx9;

                // A structured buffer load/store with ADD_TID_ENABLE is an invalid combination for the HW.
                pSrd->word3.bits.ADD_TID_ENABLE  = 0;
                pSrd->word3.bits.DATA_FORMAT     = BUF_DATA_FORMAT_32;
                pSrd->word3.bits.NUM_FORMAT      = BUF_NUM_FORMAT_UINT;
            }
            else
            {
                PAL_ASSERT_ALWAYS();
            }
        }
        else
        {
            static_assert(SQ_SEL_0                == 0, "Unexpected value for SQ_SEL_0!");
            static_assert(BUF_DATA_FORMAT_INVALID == 0, "Unexpected value for BUF_DATA_FORMAT_INVALID!");
            memset(&m_streamOut.srd[idx], 0, sizeof(m_streamOut.srd[0]));
        }

        constexpr uint32 RegStride = (mmVGT_STRMOUT_BUFFER_SIZE_1 - mmVGT_STRMOUT_BUFFER_SIZE_0);
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmVGT_STRMOUT_BUFFER_SIZE_0 + (RegStride * idx),
                                                          bufferSize,
                                                          pDeCmdSpace);
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);

    uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();

    pCeCmdSpace = UploadToUserDataTableCeRam(m_cmdUtil,
                                             &m_streamOut.state,
                                             0,
                                             (sizeof(m_streamOut.srd) / sizeof(uint32)),
                                             reinterpret_cast<const uint32*>(&m_streamOut.srd[0]),
                                             UINT_MAX,
                                             pCeCmdSpace);

    m_ceCmdStream.CommitCommands(pCeCmdSpace);

    m_graphicsState.bindStreamOutTargets                          = params;
    m_graphicsState.dirtyFlags.nonValidationBits.streamOutTargets = 1;
}

// =====================================================================================================================
// Generates PA_SU_SC_MODE_CNTL for the triangle raster state.
static regPA_SU_SC_MODE_CNTL BuildPaSuScModeCntl(
    const TriangleRasterStateParams& params)
{
    regPA_SU_SC_MODE_CNTL paSuScModeCntl;

    paSuScModeCntl.u32All                        = 0;
    paSuScModeCntl.bits.POLY_OFFSET_FRONT_ENABLE = params.flags.depthBiasEnable;
    paSuScModeCntl.bits.POLY_OFFSET_BACK_ENABLE  = params.flags.depthBiasEnable;
    paSuScModeCntl.bits.MULTI_PRIM_IB_ENA        = 1;

    static_assert(
        static_cast<uint32>(FillMode::Points)    == 0 &&
        static_cast<uint32>(FillMode::Wireframe) == 1 &&
        static_cast<uint32>(FillMode::Solid)     == 2,
        "FillMode vs. PA_SU_SC_MODE_CNTL.POLY_MODE mismatch");

    paSuScModeCntl.bits.POLY_MODE            = (params.fillMode != FillMode::Solid);
    paSuScModeCntl.bits.POLYMODE_BACK_PTYPE  = static_cast<uint32>(params.fillMode);
    paSuScModeCntl.bits.POLYMODE_FRONT_PTYPE = static_cast<uint32>(params.fillMode);

    constexpr uint32 FrontCull = static_cast<uint32>(CullMode::Front);
    constexpr uint32 BackCull  = static_cast<uint32>(CullMode::Back);

    static_assert((FrontCull | BackCull) == static_cast<uint32>(CullMode::FrontAndBack),
        "CullMode::FrontAndBack not a strict union of CullMode::Front and CullMode::Back");

    paSuScModeCntl.bits.CULL_FRONT = ((static_cast<uint32>(params.cullMode) & FrontCull) != 0);
    paSuScModeCntl.bits.CULL_BACK  = ((static_cast<uint32>(params.cullMode) & BackCull)  != 0);

    static_assert(
        static_cast<uint32>(FaceOrientation::Ccw) == 0 &&
        static_cast<uint32>(FaceOrientation::Cw)  == 1,
        "FaceOrientation vs. PA_SU_SC_MODE_CNTL.FACE mismatch");

    paSuScModeCntl.bits.FACE = static_cast<uint32>(params.frontFace);

    static_assert(
        static_cast<uint32>(ProvokingVertex::First) == 0 &&
        static_cast<uint32>(ProvokingVertex::Last)  == 1,
        "ProvokingVertex vs. PA_SU_SC_MODE_CNTL.PROVOKING_VTX_LAST mismatch");

    paSuScModeCntl.bits.PROVOKING_VTX_LAST = static_cast<uint32>(params.provokingVertex);

    return paSuScModeCntl;
}

// =====================================================================================================================
// Sets parameters controlling triangle rasterization.
void UniversalCmdBuffer::CmdSetTriangleRasterState(
    const TriangleRasterStateParams& params)
{
    m_state.flags.optimizeLinearGfxCpy                               = 0;

    m_graphicsState.triangleRasterState                              = params;
    m_graphicsState.dirtyFlags.validationBits.triangleRasterState    = 1;
    m_nggState.flags.dirty.triangleRasterState                       = 1;

    const TriangleRasterStateParams* pParams = &params;

    if (static_cast<TossPointMode>(m_cachedSettings.tossPointMode) == TossPointWireframe)
    {
        m_graphicsState.triangleRasterState.fillMode = FillMode::Wireframe;

        pParams = &m_graphicsState.triangleRasterState;
    }

    const regPA_SU_SC_MODE_CNTL paSuScModeCntl = BuildPaSuScModeCntl(*pParams);
    m_state.primShaderCbLayout.pipelineStateCb.paSuScModeCntl = paSuScModeCntl.u32All;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmPA_SU_SC_MODE_CNTL, paSuScModeCntl.u32All, pDeCmdSpace);

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetTriangleRasterStateInternal(
    const TriangleRasterStateParams& params,
    bool                             optimizeLinearDestGfxCopy)
{
    // CmdSetTriangleRasterState always clear the optimizeLinearGfxCpy flag.
    CmdSetTriangleRasterState(params);
    m_state.flags.optimizeLinearGfxCpy = optimizeLinearDestGfxCopy;
}

// =====================================================================================================================
// Sets parameters controlling point and line rasterization.
void UniversalCmdBuffer::CmdSetPointLineRasterState(
    const PointLineRasterStateParams& params)
{
    PointLineRasterStateReg pm4Image = {};
    BuildSetPointLineRasterState(params, m_cmdUtil, reinterpret_cast<uint32*>(&pm4Image));

    constexpr size_t Pm4ImageSize = sizeof(PointLineRasterStateReg) >> 2;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WritePm4Image(Pm4ImageSize, &pm4Image, pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);

    m_graphicsState.pointLineRasterState                              = params;
    m_graphicsState.dirtyFlags.nonValidationBits.pointLineRasterState = 1;
}

// =====================================================================================================================
// Builds the packets for setting the point line raster state in the command space provided.
uint32* UniversalCmdBuffer::BuildSetPointLineRasterState(
    const PointLineRasterStateParams& params,
    const CmdUtil&                    cmdUtil,
    uint32*                           pCmdSpace)
{
    auto*const   pImage      = reinterpret_cast<PointLineRasterStateReg*>(pCmdSpace);
    const size_t totalDwords = cmdUtil.BuildSetSeqContextRegs(mmPA_SU_POINT_SIZE,
                                                              mmPA_SU_LINE_CNTL,
                                                              &pImage->paSuHeader);

    // Point radius and line width are in 4-bit sub-pixel precision
    constexpr float  HalfSizeInSubPixels = 8.0f;
    constexpr uint32 MaxPointRadius      = USHRT_MAX;
    constexpr uint32 MaxLineWidth        = USHRT_MAX;

    const uint32 pointRadius   = Min(static_cast<uint32>(params.pointSize * HalfSizeInSubPixels), MaxPointRadius);
    const uint32 lineWidthHalf = Min(static_cast<uint32>(params.lineWidth * HalfSizeInSubPixels), MaxLineWidth);

    pImage->paSuPointSize.u32All      = 0;
    pImage->paSuPointSize.bits.WIDTH  = pointRadius;
    pImage->paSuPointSize.bits.HEIGHT = pointRadius;

    pImage->paSuLineCntl.u32All     = 0;
    pImage->paSuLineCntl.bits.WIDTH = lineWidthHalf;

    const uint32 pointRadiusMin =
        Util::Min(static_cast<uint32>(params.pointSizeMin * HalfSizeInSubPixels), MaxPointRadius);
    const uint32 pointRadiusMax =
        Util::Min(static_cast<uint32>(params.pointSizeMax * HalfSizeInSubPixels), MaxPointRadius);

    pImage->paSuPointMinMax.u32All        = 0;
    pImage->paSuPointMinMax.bits.MIN_SIZE = pointRadiusMin;
    pImage->paSuPointMinMax.bits.MAX_SIZE = pointRadiusMax;

    return pCmdSpace + totalDwords;
}

// =====================================================================================================================
// Sets depth bias parameters.
void UniversalCmdBuffer::CmdSetDepthBiasState(
    const DepthBiasParams& params)
{
    DepthBiasStateReg pm4Image = {};
    BuildSetDepthBiasState(params, m_cmdUtil, reinterpret_cast<uint32*>(&pm4Image));

    constexpr size_t Pm4ImageSize = sizeof(DepthBiasStateReg) >> 2;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WritePm4Image(Pm4ImageSize, &pm4Image, pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);

    m_graphicsState.depthBiasState                              = params;
    m_graphicsState.dirtyFlags.nonValidationBits.depthBiasState = 1;
}

// =====================================================================================================================
// Builds the packets for setting the depth bias state in the command space provided.
uint32* UniversalCmdBuffer::BuildSetDepthBiasState(
    const DepthBiasParams& params,
    const CmdUtil&         cmdUtil,
    uint32*                pCmdSpace)
{
    DepthBiasStateReg* pImage       = reinterpret_cast<DepthBiasStateReg*>(pCmdSpace);
    const size_t       totalDwords  =
        cmdUtil.BuildSetSeqContextRegs(mmPA_SU_POLY_OFFSET_CLAMP, mmPA_SU_POLY_OFFSET_BACK_OFFSET, &pImage->header);

    // NOTE: HW applies a factor of 1/16th to the Z gradients which we must account for.
    constexpr float HwOffsetScaleMultiplier = 16.0f;

    const float slopeScaleDepthBias = (params.slopeScaledDepthBias * HwOffsetScaleMultiplier);

    pImage->paSuPolyOffsetClamp.f32All       = params.depthBiasClamp;
    pImage->paSuPolyOffsetFrontScale.f32All  = slopeScaleDepthBias;
    pImage->paSuPolyOffsetBackScale.f32All   = slopeScaleDepthBias;
    pImage->paSuPolyOffsetFrontOffset.f32All = static_cast<float>(params.depthBias);
    pImage->paSuPolyOffsetBackOffset.f32All  = static_cast<float>(params.depthBias);

    return pCmdSpace + totalDwords;
}

// =====================================================================================================================
// Sets global scissor rectangle params.
void UniversalCmdBuffer::CmdSetGlobalScissor(
    const GlobalScissorParams& params)
{
    GlobalScissorReg pm4Image = {};
    BuildSetGlobalScissor(params, m_cmdUtil, reinterpret_cast<uint32*>(&pm4Image));

    constexpr size_t Pm4ImageSize = sizeof(GlobalScissorReg) >> 2;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WritePm4Image(Pm4ImageSize, &pm4Image, pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);

    m_graphicsState.globalScissorState                              = params;
    m_graphicsState.dirtyFlags.nonValidationBits.globalScissorState = 1;
}

// =====================================================================================================================
// Builds the packets for setting the global scissor rectangle in the command space provided.
uint32* UniversalCmdBuffer::BuildSetGlobalScissor(
    const GlobalScissorParams& params,
    const CmdUtil&             cmdUtil,
    uint32*                    pCmdSpace)
{
    GlobalScissorReg* pImage      = reinterpret_cast<GlobalScissorReg*>(pCmdSpace);
    const size_t      totalDwords =
        cmdUtil.BuildSetSeqContextRegs(mmPA_SC_WINDOW_SCISSOR_TL, mmPA_SC_WINDOW_SCISSOR_BR, &pImage->header);

    pImage->topLeft.u32All     = 0;
    pImage->bottomRight.u32All = 0;

    const uint32 left   = params.scissorRegion.offset.x;
    const uint32 top    = params.scissorRegion.offset.y;
    const uint32 right  = params.scissorRegion.offset.x + params.scissorRegion.extent.width;
    const uint32 bottom = params.scissorRegion.offset.y + params.scissorRegion.extent.height;

    pImage->topLeft.bits.WINDOW_OFFSET_DISABLE = 1;
    pImage->topLeft.bits.TL_X     = Clamp<uint32>(left, 0, ScissorMaxTL);
    pImage->topLeft.bits.TL_Y     = Clamp<uint32>(top, 0, ScissorMaxTL);
    pImage->bottomRight.bits.BR_X = Clamp<uint32>(right, 0, ScissorMaxBR);
    pImage->bottomRight.bits.BR_Y = Clamp<uint32>(bottom, 0, ScissorMaxBR);

    return pCmdSpace + totalDwords;
}

// =====================================================================================================================
// This function produces a draw developer callback based on current pipeline state.
void UniversalCmdBuffer::DescribeDraw(
    Developer::DrawDispatchType cmdType)
{
    // Get the first user data register offset depending on which HW shader stage is running the VS
    const auto* pPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const uint32 userData0 = pPipeline->GetVsUserDataBaseOffset();

    // Compute register offsets of first vertex and start instance user data locations relative to
    // user data 0.
    PAL_ASSERT((GetVertexOffsetRegAddr() != 0) && (GetInstanceOffsetRegAddr() != 0));
    PAL_ASSERT(GetVertexOffsetRegAddr() >= userData0);
    PAL_ASSERT(GetInstanceOffsetRegAddr() >= userData0);

    uint32 firstVertexIdx = GetVertexOffsetRegAddr() - userData0;
    uint32 startInstanceIdx = GetInstanceOffsetRegAddr() - userData0;
    uint32 drawIndexIdx = UINT_MAX;

    if (m_drawIndexReg != UserDataNotMapped)
    {
        drawIndexIdx = m_drawIndexReg - userData0;
    }

    m_device.DescribeDraw(this, cmdType, firstVertexIdx, startInstanceIdx, drawIndexIdx);
}

// =====================================================================================================================
// Issues a non-indexed draw command. We must discard the draw if vertexCount or instanceCount are zero. To avoid
// branching, we will rely on the HW to discard the draw for us.
template <bool issueSqttMarkerEvent, bool viewInstancingEnable>
void PAL_STDCALL UniversalCmdBuffer::CmdDraw(
    ICmdBuffer* pCmdBuffer,
    uint32      firstVertex,
    uint32      vertexCount,
    uint32      firstInstance,
    uint32      instanceCount)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    if (issueSqttMarkerEvent)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDraw);
    }

    ValidateDrawInfo drawInfo;
    drawInfo.vtxIdxCount   = vertexCount;
    drawInfo.instanceCount = instanceCount;
    drawInfo.firstVertex   = firstVertex;
    drawInfo.firstInstance = firstInstance;
    drawInfo.firstIndex    = 0;

    if (pThis->m_primGroupOpt.enabled)
    {
        pThis->UpdatePrimGroupOpt(vertexCount);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace = pThis->ValidateDraw<false, false>(drawInfo, pDeCmdSpace);

    if (viewInstancingEnable)
    {
        const auto*const pPipeline          =
            static_cast<const GraphicsPipeline*>(pThis->m_graphicsState.pipelineState.pPipeline);
        const auto&      viewInstancingDesc = pPipeline->GetViewInstancingDesc();
        uint32           mask               = (1 << viewInstancingDesc.viewInstanceCount) - 1;

        if (viewInstancingDesc.enableMasking)
        {
            mask &= pThis->m_graphicsState.viewInstanceMask;
        }

        for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
        {
            if (TestAnyFlagSet(mask, 1))
            {
                pDeCmdSpace  = pThis->BuildWriteViewId(viewInstancingDesc.viewId[i], pDeCmdSpace);
                pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexAuto(vertexCount, pThis->PacketPredicate(), pDeCmdSpace);
            }
        }
    }
    else
    {
        const auto*const pPipeline          =
            static_cast<const GraphicsPipeline*>(pThis->m_graphicsState.pipelineState.pPipeline);
        const auto&      viewInstancingDesc = pPipeline->GetViewInstancingDesc();

        pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexAuto(vertexCount, pThis->PacketPredicate(), pDeCmdSpace);
    }

    if (issueSqttMarkerEvent)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeUniversal, pDeCmdSpace);
    }

    pDeCmdSpace = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);

    // On Gfx9, the WD (Work distributor - breaks down draw commands into work groups which are sent to IA
    // units) has changed to having independent DMA and DRAW logic. As a result, DRAW_INDEX_AUTO commands have
    // added a dummy DMA command issued by the CP which overwrites the VGT_INDEX_TYPE register used by GFX. This
    // can cause hangs and rendering corruption with subsequent indexed draw commands. We must invalidate the
    // index type state so that it will be issued before the next indexed draw.
    pThis->m_drawTimeHwState.dirty.indexedIndexType = 1;
}

// =====================================================================================================================
// Issues an indexed draw command. We must discard the draw if indexCount or instanceCount are zero. To avoid branching,
// we will rely on the HW to discard the draw for us.
template <bool issueSqttMarkerEvent, bool isNggFastLaunch, bool viewInstancingEnable>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawIndexed(
    ICmdBuffer* pCmdBuffer,
    uint32      firstIndex,
    uint32      indexCount,
    int32       vertexOffset,
    uint32      firstInstance,
    uint32      instanceCount)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    if (issueSqttMarkerEvent)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawIndexed);
    }

    PAL_ASSERT(firstIndex <= pThis->m_graphicsState.iaState.indexCount);

    ValidateDrawInfo drawInfo;
    drawInfo.vtxIdxCount   = indexCount;
    drawInfo.instanceCount = instanceCount;
    drawInfo.firstVertex   = vertexOffset;
    drawInfo.firstInstance = firstInstance;
    drawInfo.firstIndex    = firstIndex;

    if (pThis->m_primGroupOpt.enabled)
    {
        pThis->UpdatePrimGroupOpt(indexCount);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace = pThis->ValidateDraw<true, false>(drawInfo, pDeCmdSpace);

    const uint32 validIndexCount = pThis->m_graphicsState.iaState.indexCount - firstIndex;

    if (viewInstancingEnable)
    {
        const Pal::PipelineState* pPipelineState     = pThis->PipelineState(PipelineBindPoint::Graphics);
        const GraphicsPipeline*   pPipeline          = static_cast<const GraphicsPipeline*>(pPipelineState->pPipeline);
        const auto&               viewInstancingDesc = pPipeline->GetViewInstancingDesc();
        uint32                    mask               = (1 << viewInstancingDesc.viewInstanceCount) - 1;

        if (viewInstancingDesc.enableMasking)
        {
            mask &= pThis->m_graphicsState.viewInstanceMask;
        }

        for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
        {
            if (TestAnyFlagSet(mask, 1))
            {
                pDeCmdSpace = pThis->BuildWriteViewId(viewInstancingDesc.viewId[i], pDeCmdSpace);

                if (isNggFastLaunch == false)
                {
                    if (pThis->IsNested() && (pThis->m_graphicsState.iaState.indexAddr == 0))
                    {
                        // If IB state is not bound, nested command buffers must use DRAW_INDEX_OFFSET_2 so that
                        // we can inherit th IB base and size from direct command buffer
                        pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexOffset2(indexCount,
                                                                              validIndexCount,
                                                                              firstIndex,
                                                                              pThis->PacketPredicate(),
                                                                              pDeCmdSpace);
                    }
                    else
                    {
                        // Compute the address of the IB. We must add the index offset specified by firstIndex into
                        // our address because DRAW_INDEX_2 doesn't take an offset param.
                        const uint32  indexSize   = 1 << static_cast<uint32>(pThis->m_graphicsState.iaState.indexType);
                        const gpusize gpuVirtAddr = pThis->m_graphicsState.iaState.indexAddr + (indexSize * firstIndex);

                        pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndex2(indexCount,
                                                                        validIndexCount,
                                                                        gpuVirtAddr,
                                                                        pThis->PacketPredicate(),
                                                                        pDeCmdSpace);
                    }
                }
                else
                {
                    // NGG Fast Launch pipelines treat all draws as auto-index draws.
                    pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexAuto(indexCount,
                                                                       pThis->PacketPredicate(),
                                                                       pDeCmdSpace);
                }
            }
        }
    }
    else
    {
        if (isNggFastLaunch == false)
        {
            if (pThis->IsNested() && (pThis->m_graphicsState.iaState.indexAddr == 0))
            {
                // If IB state is not bound, nested command buffers must use DRAW_INDEX_OFFSET_2 so that
                // we can inherit th IB base and size from direct command buffer
                pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexOffset2(indexCount,
                                                                      validIndexCount,
                                                                      firstIndex,
                                                                      pThis->PacketPredicate(),
                                                                      pDeCmdSpace);
            }
            else
            {
                // Compute the address of the IB. We must add the index offset specified by firstIndex into
                // our address because DRAW_INDEX_2 doesn't take an offset param.
                const uint32  indexSize   = 1 << static_cast<uint32>(pThis->m_graphicsState.iaState.indexType);
                const gpusize gpuVirtAddr = pThis->m_graphicsState.iaState.indexAddr + (indexSize * firstIndex);

                pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndex2(indexCount,
                                                                validIndexCount,
                                                                gpuVirtAddr,
                                                                pThis->PacketPredicate(),
                                                                pDeCmdSpace);
            }
        }
        else
        {
            // NGG Fast Launch pipelines treat all draws as auto-index draws.
            pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexAuto(indexCount, pThis->PacketPredicate(), pDeCmdSpace);
        }
    }

    if (issueSqttMarkerEvent)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeUniversal, pDeCmdSpace);
    }

    pDeCmdSpace  = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Issues an indirect non-indexed draw command. We must discard the draw if vertexCount or instanceCount are zero.
// We will rely on the HW to discard the draw for us.
template <bool issueSqttMarkerEvent, bool viewInstancingEnable>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawIndirectMulti(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    if (issueSqttMarkerEvent)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawIndirectMulti);
    }

    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)) && IsPow2Aligned(countGpuAddr, sizeof(uint32)));
    PAL_ASSERT(offset + (SizeDrawIndirectArgs * maximumCount) <= gpuMemory.Desc().size);

    ValidateDrawInfo drawInfo;
    drawInfo.vtxIdxCount   = 0;
    drawInfo.instanceCount = 0;
    drawInfo.firstVertex   = 0;
    drawInfo.firstInstance = 0;
    drawInfo.firstIndex    = 0;

    if (pThis->m_primGroupOpt.enabled)
    {
        // Since we can't compute the number of primitives this draw uses we disable this optimization to be safe.
        pThis->DisablePrimGroupOpt();
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace  = pThis->ValidateDraw<false, true>(drawInfo, pDeCmdSpace);
    pDeCmdSpace += pThis->m_cmdUtil.BuildSetBase(gpuMemory.Desc().gpuVirtAddr,
                                                 base_index__pfp_set_base__patch_table_base,
                                                 ShaderGraphics,
                                                 pDeCmdSpace);

    const uint16 vtxOffsetReg  = pThis->GetVertexOffsetRegAddr();
    const uint16 instOffsetReg = pThis->GetInstanceOffsetRegAddr();

    pThis->m_deCmdStream.NotifyIndirectShRegWrite(vtxOffsetReg);
    pThis->m_deCmdStream.NotifyIndirectShRegWrite(instOffsetReg);

    if (viewInstancingEnable)
    {
        const auto*const pPipeline          =
            static_cast<const GraphicsPipeline*>(pThis->m_graphicsState.pipelineState.pPipeline);
        const auto&      viewInstancingDesc = pPipeline->GetViewInstancingDesc();
        uint32           mask               = (1 << viewInstancingDesc.viewInstanceCount) - 1;

        if (viewInstancingDesc.enableMasking)
        {
            mask &= pThis->m_graphicsState.viewInstanceMask;
        }

        for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
        {
            if (TestAnyFlagSet(mask, 1))
            {
                pDeCmdSpace  = pThis->BuildWriteViewId(viewInstancingDesc.viewId[i], pDeCmdSpace);
                pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndirectMulti(offset,
                                                                       vtxOffsetReg,
                                                                       instOffsetReg,
                                                                       pThis->m_drawIndexReg,
                                                                       stride,
                                                                       maximumCount,
                                                                       countGpuAddr,
                                                                       pThis->PacketPredicate(),
                                                                       pDeCmdSpace);
            }
        }
    }
    else
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndirectMulti(offset,
                                                               vtxOffsetReg,
                                                               instOffsetReg,
                                                               pThis->m_drawIndexReg,
                                                               stride,
                                                               maximumCount,
                                                               countGpuAddr,
                                                               pThis->PacketPredicate(),
                                                               pDeCmdSpace);
    }

    if (issueSqttMarkerEvent)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeUniversal, pDeCmdSpace);
    }

    pDeCmdSpace = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);

    pThis->m_state.flags.containsDrawIndirect = 1;

    // On Gfx9, we need to invalidate the index type which was previously programmed because the CP clobbers
    // that state when executing a non-indexed indirect draw.
    // SEE: CmdDraw() for more details about why we do this.
    pThis->m_drawTimeHwState.dirty.indexedIndexType = 1;
}

// =====================================================================================================================
// Issues an indirect indexed draw command. We must discard the draw if indexCount or instanceCount are zero.
// We will rely on the HW to discard the draw for us.
template <bool issueSqttMarkerEvent, bool isNggFastLaunch, bool viewInstancingEnable>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawIndexedIndirectMulti(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    if (issueSqttMarkerEvent)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawIndexedIndirectMulti);
    }

    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)) && IsPow2Aligned(countGpuAddr, sizeof(uint32)));
    PAL_ASSERT(offset + (SizeDrawIndexedIndirectArgs * maximumCount) <= gpuMemory.Desc().size);

    ValidateDrawInfo drawInfo;
    drawInfo.vtxIdxCount   = 0;
    drawInfo.instanceCount = 0;
    drawInfo.firstVertex   = 0;
    drawInfo.firstInstance = 0;
    drawInfo.firstIndex    = 0;

    if (pThis->m_primGroupOpt.enabled)
    {
        // Since we can't compute the number of primitives this draw uses we disable this optimization to be safe.
        pThis->DisablePrimGroupOpt();
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace  = pThis->ValidateDraw<true, true>(drawInfo, pDeCmdSpace);
    pDeCmdSpace += pThis->m_cmdUtil.BuildSetBase(gpuMemory.Desc().gpuVirtAddr,
                                                 base_index__pfp_set_base__patch_table_base,
                                                 ShaderGraphics,
                                                 pDeCmdSpace);

    const uint16 vtxOffsetReg   = pThis->GetVertexOffsetRegAddr();
    const uint16 instOffsetReg  = pThis->GetInstanceOffsetRegAddr();
    const uint16 indexOffsetReg = pThis->GetStartIndexRegAddr();

    pThis->m_deCmdStream.NotifyIndirectShRegWrite(vtxOffsetReg);
    pThis->m_deCmdStream.NotifyIndirectShRegWrite(instOffsetReg);

    if (isNggFastLaunch)
    {
        pThis->m_deCmdStream.NotifyIndirectShRegWrite(indexOffsetReg);
    }

    if (viewInstancingEnable)
    {
        const Pal::Device*  pParentDev         = pThis->m_device.Parent();
        const auto*const    pPipeline          =
            static_cast<const GraphicsPipeline*>(pThis->m_graphicsState.pipelineState.pPipeline);
        const auto&         viewInstancingDesc = pPipeline->GetViewInstancingDesc();
        uint32              mask               = (1 << viewInstancingDesc.viewInstanceCount) - 1;

        if (viewInstancingDesc.enableMasking)
        {
            mask &= pThis->m_graphicsState.viewInstanceMask;
        }

        for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
        {
            if (TestAnyFlagSet(mask, 1))
            {
                 pDeCmdSpace = pThis->BuildWriteViewId(viewInstancingDesc.viewId[i], pDeCmdSpace);

                {
                    if ((isNggFastLaunch == false) ||
                        (pParentDev->EngineProperties().cpUcodeVersion > UcodeVersionNggIndexedIndirectDraw))
                    {
                        pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexIndirectMulti(offset,
                                                                                    vtxOffsetReg,
                                                                                    instOffsetReg,
                                                                                    pThis->m_drawIndexReg,
                                                                                    indexOffsetReg,
                                                                                    stride,
                                                                                    maximumCount,
                                                                                    countGpuAddr,
                                                                                    pThis->PacketPredicate(),
                                                                                    pDeCmdSpace);
                    }
                }
            }
        }
    }
    else
    {
        {
            if ((isNggFastLaunch == false) ||
                (pThis->m_device.Parent()->EngineProperties().cpUcodeVersion > UcodeVersionNggIndexedIndirectDraw))
            {
                pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexIndirectMulti(offset,
                                                                            vtxOffsetReg,
                                                                            instOffsetReg,
                                                                            pThis->m_drawIndexReg,
                                                                            indexOffsetReg,
                                                                            stride,
                                                                            maximumCount,
                                                                            countGpuAddr,
                                                                            pThis->PacketPredicate(),
                                                                            pDeCmdSpace);
            }
        }
    }

    if (issueSqttMarkerEvent)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeUniversal, pDeCmdSpace);
    }

    pDeCmdSpace = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);

    pThis->m_state.flags.containsDrawIndirect = 1;
}

// =====================================================================================================================
// Issues a direct dispatch command. We must discard the dispatch if x, y, or z are zero. To avoid branching, we will
// rely on the HW to discard the dispatch for us.
template <bool issueSqttMarkerEvent>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatch(
    ICmdBuffer* pCmdBuffer,
    uint32      x,
    uint32      y,
    uint32      z)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    if (issueSqttMarkerEvent)
    {
        pThis->m_device.DescribeDispatch(pThis, Developer::DrawDispatchType::CmdDispatch, 0, 0, 0, x, y, z);
    }

    gpusize gpuVirtAddrNumTgs = 0uLL;
    if (pThis->m_pSignatureCs->numWorkGroupsRegAddr != UserDataNotMapped)
    {
        // Reserve embedded user data for the number of launched thread groups if the active pipeline needs to access
        // the number of thread groups...
        uint32*const pData = pThis->CmdAllocateEmbeddedData(3, 4, &gpuVirtAddrNumTgs);
        pData[0] = x;
        pData[1] = y;
        pData[2] = z;
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace  = pThis->ValidateDispatch(gpuVirtAddrNumTgs, pDeCmdSpace);
    pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect<false, true>(x, y, z,
                                                                     pThis->PacketPredicate(),
                                                                     pDeCmdSpace);

    if (issueSqttMarkerEvent)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeUniversal, pDeCmdSpace);
    }

    pDeCmdSpace  = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Issues an indirect dispatch command. We must discard the dispatch if x, y, or z are zero. We will rely on the HW to
// discard the dispatch for us.
template <bool issueSqttMarkerEvent>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchIndirect(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    if (issueSqttMarkerEvent)
    {
        pThis->m_device.DescribeDispatch(pThis, Developer::DrawDispatchType::CmdDispatchIndirect, 0, 0, 0, 0, 0, 0);
    }

    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)));
    PAL_ASSERT(offset + SizeDispatchIndirectArgs <= gpuMemory.Desc().size);

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace  = pThis->ValidateDispatch((gpuMemory.Desc().gpuVirtAddr + offset), pDeCmdSpace);
    pDeCmdSpace += pThis->m_cmdUtil.BuildSetBase(gpuMemory.Desc().gpuVirtAddr,
                                                 base_index__pfp_set_base__patch_table_base,
                                                 ShaderCompute,
                                                 pDeCmdSpace);
    pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchIndirectGfx(offset,
                                                             pThis->PacketPredicate(),
                                                             pDeCmdSpace);

    if (issueSqttMarkerEvent)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeUniversal, pDeCmdSpace);
    }

    pDeCmdSpace  = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);

    pThis->m_state.flags.containsDrawIndirect = 1;
}

// =====================================================================================================================
// Issues a direct dispatch command with immediate threadgroup offsets. We must discard the dispatch if x, y, or z are
// zero. To avoid branching, we will rely on the HW to discard the dispatch for us.
template <bool issueSqttMarkerEvent>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchOffset(
    ICmdBuffer* pCmdBuffer,
    uint32      xOffset,
    uint32      yOffset,
    uint32      zOffset,
    uint32      xDim,
    uint32      yDim,
    uint32      zDim)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    if (issueSqttMarkerEvent)
    {
        pThis->m_device.DescribeDispatch(pThis, Developer::DrawDispatchType::CmdDispatchOffset,
            xOffset, yOffset, zOffset, xDim, yDim, zDim);
    }

    gpusize gpuVirtAddrNumTgs = 0uLL;
    if (pThis->m_pSignatureCs->numWorkGroupsRegAddr != UserDataNotMapped)
    {
        // Reserve embedded user data for the number of launched thread groups if the active pipeline needs to access
        // the number of thread groups...
        uint32*const pData = pThis->CmdAllocateEmbeddedData(3, 4, &gpuVirtAddrNumTgs);
        pData[0] = xDim;
        pData[1] = yDim;
        pData[2] = zDim;
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    const uint32 starts[3] = {xOffset, yOffset, zOffset};
    pDeCmdSpace  = pThis->m_deCmdStream.WriteSetSeqShRegs(mmCOMPUTE_START_X,
                                                          mmCOMPUTE_START_Z,
                                                          ShaderCompute,
                                                          starts,
                                                          pDeCmdSpace);

    // xDim, yDim, zDim are end positions instead of numbers of threadgroups to execute.
    xDim += xOffset;
    yDim += yOffset;
    zDim += zOffset;

    pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect<false, false>(xDim,
                                                                      yDim,
                                                                      zDim,
                                                                      pThis->PacketPredicate(),
                                                                      pDeCmdSpace);

    if (issueSqttMarkerEvent)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeUniversal, pDeCmdSpace);
    }

    pDeCmdSpace  = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdCloneImageData(
    const IImage& srcImage,
    const IImage& dstImage)
{
    m_device.RsrcProcMgr().CmdCloneImageData(this, GetGfx9Image(srcImage), GetGfx9Image(dstImage));
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdCopyMemory(
    const IGpuMemory&       srcGpuMemory,
    const IGpuMemory&       dstGpuMemory,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions)
{
    m_device.RsrcProcMgr().CmdCopyMemory(this,
                                         static_cast<const GpuMemory&>(srcGpuMemory),
                                         static_cast<const GpuMemory&>(dstGpuMemory),
                                         regionCount,
                                         pRegions);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdUpdateMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           dataSize,
    const uint32*     pData)
{
    PAL_ASSERT(pData != nullptr);
    m_device.RsrcProcMgr().CmdUpdateMemory(this,
                                           static_cast<const GpuMemory&>(dstGpuMemory),
                                           dstOffset,
                                           dataSize,
                                           pData);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdUpdateBusAddressableMemoryMarker(
    const IGpuMemory& dstGpuMemory,
    uint32            value)
{
    const GpuMemory* pGpuMemory = static_cast<const GpuMemory*>(&dstGpuMemory);

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace += m_cmdUtil.BuildWriteData(GetEngineType(),
                                            pGpuMemory->GetBusAddrMarkerVa(),
                                            1,
                                            engine_sel__me_write_data__micro_engine,
                                            dst_sel__me_write_data__memory,
                                            wr_confirm__me_write_data__wait_for_write_confirmation,
                                            &value,
                                            PredDisable,
                                            pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Use the GPU's command processor to execute an atomic memory operation
void UniversalCmdBuffer::CmdMemoryAtomic(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    uint64            srcData,
    AtomicOp          atomicOp)
{
    const gpusize address = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace += m_cmdUtil.BuildAtomicMem(atomicOp, address, srcData, pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Issues either an end-of-pipe timestamp or a start of pipe timestamp event.  Writes the results to the pMemObject +
// destOffset.
void UniversalCmdBuffer::CmdWriteTimestamp(
    HwPipePoint       pipePoint,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    const gpusize address = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    if (pipePoint == HwPipeTop)
    {
        pDeCmdSpace += m_cmdUtil.BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
                                                       dst_sel__me_copy_data__memory__GFX09,
                                                       address,
                                                       src_sel__me_copy_data__gpu_clock_count,
                                                       0,
                                                       count_sel__me_copy_data__64_bits_of_data,
                                                       wr_confirm__me_copy_data__wait_for_confirmation,
                                                       pDeCmdSpace);
    }
    else
    {
        PAL_ASSERT(pipePoint == HwPipeBottom);

        ReleaseMemInfo releaseInfo = {};
        releaseInfo.engineType     = EngineTypeUniversal;
        releaseInfo.vgtEvent       = BOTTOM_OF_PIPE_TS;
        releaseInfo.tcCacheOp      = TcCacheOp::Nop;
        releaseInfo.dstAddr        = address;
        releaseInfo.dataSel        = data_sel__me_release_mem__send_gpu_clock_counter;
        releaseInfo.data           = 0;

        pDeCmdSpace += m_cmdUtil.BuildReleaseMem(releaseInfo, pDeCmdSpace);
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Writes an immediate value either during top-of-pipe or bottom-of-pipe event.
void UniversalCmdBuffer::CmdWriteImmediate(
    HwPipePoint        pipePoint,
    uint64             data,
    ImmediateDataWidth dataSize,
    gpusize            address)
{
    PAL_ASSERT(m_gfxIpLevel == GfxIpLevel::GfxIp9);

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    if (pipePoint == HwPipeTop)
    {
        pDeCmdSpace += m_cmdUtil.BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
                                                       dst_sel__me_copy_data__memory__GFX09,
                                                       address,
                                                       src_sel__me_copy_data__immediate_data,
                                                       data,
                                                       ((dataSize == ImmediateDataWidth::ImmediateData32Bit) ?
                                                           count_sel__me_copy_data__32_bits_of_data :
                                                           count_sel__me_copy_data__64_bits_of_data),
                                                       wr_confirm__me_copy_data__wait_for_confirmation,
                                                       pDeCmdSpace);
    }
    else
    {
        PAL_ASSERT(pipePoint == HwPipeBottom);

        ReleaseMemInfo releaseInfo = {};
        releaseInfo.engineType     = EngineTypeUniversal;
        releaseInfo.vgtEvent       = BOTTOM_OF_PIPE_TS;
        releaseInfo.tcCacheOp      = TcCacheOp::Nop;
        releaseInfo.dstAddr        = address;
        releaseInfo.dataSel        = ((dataSize == ImmediateDataWidth::ImmediateData32Bit) ?
                                         data_sel__me_release_mem__send_32_bit_low :
                                         data_sel__me_release_mem__send_64_bit_data);
        releaseInfo.data           = data;

        pDeCmdSpace += m_cmdUtil.BuildReleaseMem(releaseInfo, pDeCmdSpace);
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindBorderColorPalette(
    PipelineBindPoint          pipelineBindPoint,
    const IBorderColorPalette* pPalette)
{
    // NOTE: The hardware fundamentally does not support multiple border color palettes for compute as the register
    //       which controls the address of the palette is a config register. We need to support this for our clients,
    //       but it should not be considered a correct implementation. As a result we may see arbitrary hangs that
    //       do not reproduce easily. This setting (disableBorderColorPaletteBinds) should be set to TRUE in the event
    //       that one of these hangs is suspected. At that point we will need to come up with a more robust solution
    //       which may involve getting KMD support.
    if ((m_cachedSettings.ignoreCsBorderColorPalette == 0) || (pipelineBindPoint == PipelineBindPoint::Graphics))
    {
        auto*const       pPipelineState = PipelineState(pipelineBindPoint);
        const auto*const pNewPalette    = static_cast<const BorderColorPalette*>(pPalette);
        const auto*const pOldPalette    = static_cast<const BorderColorPalette*>(pPipelineState->pBorderColorPalette);

        if (pNewPalette != nullptr)
        {
            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
            pDeCmdSpace = pNewPalette->WriteCommands(pipelineBindPoint, &m_deCmdStream, pDeCmdSpace);
            m_deCmdStream.CommitCommands(pDeCmdSpace);
        }

        // Update the border-color palette state.
        pPipelineState->pBorderColorPalette                = pNewPalette;
        pPipelineState->dirtyFlags.borderColorPaletteDirty = 1;
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdInsertTraceMarker(
    PerfTraceMarkerType markerType,
    uint32              markerData)
{
    PAL_ASSERT(m_pCurrentExperiment != nullptr);

    const PerfExperiment*const pExperiment = static_cast<const PerfExperiment*>(m_pCurrentExperiment);
    pExperiment->InsertTraceMarker(&m_deCmdStream, markerType, markerData);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdInsertRgpTraceMarker(
    uint32      numDwords,
    const void* pData)
{
    // The first dword of every RGP trace marker packet is written to SQ_THREAD_TRACE_USERDATA_2.  The second dword
    // is written to SQ_THREAD_TRACE_USERDATA_3.  For packets longer than 64-bits, continue alternating between
    // user data 2 and 3.

    const uint32 userDataAddr = m_device.CmdUtil().GetRegInfo().mmSqThreadTraceUserData2;
    PAL_ASSERT(m_device.CmdUtil().IsPrivilegedConfigReg(userDataAddr) == false);
    PAL_ASSERT(m_device.CmdUtil().GetRegInfo().mmSqThreadTraceUserData3 == (userDataAddr + 1));

    const uint32* pDwordData = static_cast<const uint32*>(pData);
    while (numDwords > 0)
    {
        const uint32 dwordsToWrite = Min(numDwords, 2u);

        // Reserve and commit command space inside this loop.  Some of the RGP packets are unbounded, like adding a
        // comment string, so it's not safe to assume the whole packet will fit under our reserve limit.
        uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

        pCmdSpace = m_deCmdStream.WriteSetSeqConfigRegs(userDataAddr,
                                                        userDataAddr + dwordsToWrite - 1,
                                                        pDwordData,
                                                        pCmdSpace);
        pDwordData += dwordsToWrite;
        numDwords  -= dwordsToWrite;

        m_deCmdStream.CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Build the NULL depth-stencil PM4 packets.
uint32* UniversalCmdBuffer::WriteNullDepthTarget(
    uint32* pCmdSpace)
{
    NullDepthStencilPm4Img pm4Commands; // Intentionally left un-initialized; we will fully overwrite the important
                                        // fields below.

    // The screen scissor rect and the HTile base address need to be written regardless of GFXIP version.
    size_t cmdDwords = (m_cmdUtil.BuildSetSeqContextRegs(mmPA_SC_SCREEN_SCISSOR_TL,
                                                         mmPA_SC_SCREEN_SCISSOR_BR,
                                                         &pm4Commands.hdrPaScScreenScissor) +
                        m_cmdUtil.BuildSetOneContextReg(mmDB_HTILE_DATA_BASE, &pm4Commands.hdrDbHtileDataBase));

    pm4Commands.paScScreenScissorTl.bits.TL_X = PaScScreenScissorMin;
    pm4Commands.paScScreenScissorTl.bits.TL_Y = PaScScreenScissorMin;
    pm4Commands.paScScreenScissorBr.bits.BR_X = PaScScreenScissorMax;
    pm4Commands.paScScreenScissorBr.bits.BR_Y = PaScScreenScissorMax;
    pm4Commands.dbHtileDataBase.u32All        = 0;

    // The rest of the PM4 commands depend on which GFXIP version we are.
    const GfxIpLevel gfxLevel = m_device.Parent()->ChipProperties().gfxLevel;
    if (gfxLevel == GfxIpLevel::GfxIp9)
    {
        cmdDwords += m_cmdUtil.BuildSetSeqContextRegs(mmDB_Z_INFO__GFX09,
                                                      mmDB_DFSM_CONTROL__GFX09,
                                                      &pm4Commands.hdrDbInfo);

        pm4Commands.gfx9.dbZInfo.u32All              = 0;
        pm4Commands.gfx9.dbStencilInfo.u32All        = 0;
        pm4Commands.gfx9.dbZReadBase.u32All          = 0;
        pm4Commands.gfx9.dbZReadBaseHi.u32All        = 0;
        pm4Commands.gfx9.dbStencilReadBase.u32All    = 0;
        pm4Commands.gfx9.dbStencilReadBaseHi.u32All  = 0;
        pm4Commands.gfx9.dbZWriteBase.u32All         = 0;
        pm4Commands.gfx9.dbZWriteBaseHi.u32All       = 0;
        pm4Commands.gfx9.dbStencilWriteBase.u32All   = 0;
        pm4Commands.gfx9.dbStencilWriteBaseHi.u32All = 0;
        pm4Commands.gfx9.dbDfsmControl.u32All        = m_device.GetDbDfsmControl();
    }
    else
    {
        // What is this?
        PAL_ASSERT_ALWAYS();
    }

    PAL_ASSERT(cmdDwords != 0);
    return m_deCmdStream.WritePm4Image(cmdDwords, &pm4Commands, pCmdSpace);
}

// =====================================================================================================================
// Build the NULL color targets PM4 packets. It is safe to call this when there are no NULL color targets.
uint32* UniversalCmdBuffer::WriteNullColorTargets(
    uint32* pCmdSpace,
    uint32  newColorTargetMask,
    uint32  oldColorTargetMask)
{
    // Scratch space for building the null color targets PM4 image.
    uint32 pm4Commands[MaxNullColorTargetPm4ImgSize / sizeof(uint32)];
    size_t cmdDwords = 0;

    // Compute a mask of slots which were previously bound to valid targets, but are now being bound to NULL.
    uint32 newNullSlotMask = (oldColorTargetMask & ~newColorTargetMask);
    while (newNullSlotMask != 0)
    {
        uint32 slot = 0;
        BitMaskScanForward(&slot, newNullSlotMask);

        auto*const pPm4Image = reinterpret_cast<ColorInfoReg*>(&pm4Commands[cmdDwords]);

        cmdDwords += m_cmdUtil.BuildSetOneContextReg(mmCB_COLOR0_INFO + (slot * CbRegsPerSlot), &pPm4Image->header);

        pPm4Image->cbColorInfo.u32All = 0;
        pPm4Image->cbColorInfo.bits.FORMAT = COLOR_INVALID;

        // Clear the bit since we've already added it to our PM4 image.
        newNullSlotMask &= ~(1 << slot);
    }

    // If no color targets are active, setup generic scissor registers for NULL color targets.
    if (newColorTargetMask == 0)
    {
        auto*const pPm4Image = reinterpret_cast<GenericScissorReg*>(&pm4Commands[cmdDwords]);

        cmdDwords += m_cmdUtil.BuildSetSeqContextRegs(mmPA_SC_GENERIC_SCISSOR_TL,
                                                      mmPA_SC_GENERIC_SCISSOR_BR,
                                                      &pPm4Image->header);

        pPm4Image->paScGenericScissorTl.u32All    = 0;
        pPm4Image->paScGenericScissorBr.u32All    = 0;
        pPm4Image->paScGenericScissorBr.bits.BR_X = ScissorMaxBR;
        pPm4Image->paScGenericScissorBr.bits.BR_Y = ScissorMaxBR;
    }

    if (cmdDwords != 0)
    {
        pCmdSpace = m_deCmdStream.WritePm4Image(cmdDwords, &pm4Commands[0], pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Adds a preamble to the start of a new command buffer.
Result UniversalCmdBuffer::AddPreamble()
{
    // If this trips, it means that this isn't really the preamble -- i.e., somebody has inserted something into the
    // command stream before the preamble.  :-(
    PAL_ASSERT(m_ceCmdStream.IsEmpty());
    PAL_ASSERT(m_deCmdStream.IsEmpty());

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PIPELINESTAT_START, EngineTypeUniversal, pDeCmdSpace);

    if ((m_device.Parent()->GetPlatform()->IsEmulationEnabled()) && (IsNested() == false))
    {
        PAL_ASSERT(m_device.Parent()->IsPreemptionSupported(EngineType::EngineTypeUniversal) == false);

        PM4PFP_CONTEXT_CONTROL contextControl = {};

        contextControl.bitfields2.load_enable            = 1;
        contextControl.bitfields2.load_per_context_state = 1;
        contextControl.bitfields3.shadow_enable          = 1;

        pDeCmdSpace += m_cmdUtil.BuildContextControl(contextControl, pDeCmdSpace);
    }

    // DB_RENDER_OVERRIDE bits are updated via depth-stencil view and at draw time validation based on dirty
    // depth-stencil state.
    regDB_RENDER_OVERRIDE dbRenderOverride = { };
    if (m_cachedSettings.hiDepthDisabled != 0)
    {
        dbRenderOverride.bits.FORCE_HIZ_ENABLE = FORCE_DISABLE;
    }
    if (m_cachedSettings.hiStencilDisabled != 0)
    {
        dbRenderOverride.bits.FORCE_HIS_ENABLE0 = FORCE_DISABLE;
        dbRenderOverride.bits.FORCE_HIS_ENABLE1 = FORCE_DISABLE;
    }

    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmDB_RENDER_OVERRIDE, dbRenderOverride.u32All, pDeCmdSpace);

    const GfxIpLevel gfxLevel = m_device.Parent()->ChipProperties().gfxLevel;

    if (gfxLevel == GfxIpLevel::GfxIp9)
    {
    }

    // With the PM4 optimizer enabled, certain registers are only updated via RMW packets and not having an initial
    // value causes the optimizer to skip optimizing redundant RMW packets.
    if (m_deCmdStream.Pm4OptimizerEnabled())
    {
        // PA_SC_CONSERVATIVE_RASTERIZATION_CNTL bits are updated based on MSAA state, pipeline state and draw time
        // validation based on dirty query state via RMW packets. All the states are valid on a nested command buffer
        // and will be set when this register gets written. Hence, we can safely set to default value for all
        // command buffers.
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmPA_SC_CONSERVATIVE_RASTERIZATION_CNTL, 0, pDeCmdSpace);

        if (IsNested() == false)
        {
            // Nested command buffers inherit parts of the following registers and hence must not be reset
            // in the preamble.

            // PA_SC_AA_CONFIG bits are updated based on MSAA state, pipeline state, CmdSetMsaaQuadSamplePattern
            // and draw time validation based on dirty query state via RMW packets.
            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmPA_SC_AA_CONFIG, 0, pDeCmdSpace);

            constexpr uint32 ZeroStencilRefMasks[] = { 0, 0 };
            pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmDB_STENCILREFMASK,
                                                               mmDB_STENCILREFMASK_BF,
                                                               &ZeroStencilRefMasks[0],
                                                               pDeCmdSpace);
        }

        m_deCmdStream.SetContextRollDetected<true>();
    }

    // Disable PBB at the start of each command buffer unconditionally. Each draw can set the appropriate
    // PBB state at validate time.
    m_enabledPbb = false;
    m_paScBinnerCntl0.u32All = 0;
    m_paScBinnerCntl0.bits.BINNING_MODE          = DISABLE_BINNING_USE_LEGACY_SC;
    m_paScBinnerCntl0.bits.DISABLE_START_OF_PRIM = (m_cachedSettings.disableDfsm) ? 1 : 0;
    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmPA_SC_BINNER_CNTL_0, m_paScBinnerCntl0.u32All, pDeCmdSpace);

    m_deCmdStream.CommitCommands(pDeCmdSpace);

    // Clients may not bind a PointLineRasterState until they intend to do wireframe rendering. This means that the
    // wireframe tosspoint may render a bunch of zero-width lines (i.e. nothing) until that state is bound. When that
    // tosspoint is enabled we should bind some default state to be sure that we will see some lines.
    if (static_cast<TossPointMode>(m_cachedSettings.tossPointMode) == TossPointWireframe)
    {
        Pal::PointLineRasterStateParams rasterState = {};
        rasterState.lineWidth = 1.0f;
        rasterState.pointSize = 1.0f;

        CmdSetPointLineRasterState(rasterState);
    }

    if (m_device.SupportsCePreamblePerSubmit() == false)
    {
        // This dummy LOAD_CONST_RAM packet is because the KMD expects every submission from a UMD to contain at
        // least one LOAD_CONST_RAM packet (even if it loads nothing) in order to support High-Priority 3D Queues.
        // Not having this causes a CP hang when switching between GPU contexts. Mantle does not need this because
        // it never uses CE RAM for anything.
        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
        pCeCmdSpace += m_cmdUtil.BuildLoadConstRam(0, 0, 0, pCeCmdSpace);
        m_ceCmdStream.CommitCommands(pCeCmdSpace);
    }

    return Result::Success;
}

// =====================================================================================================================
// Adds a postamble to the end of a new command buffer.
Result UniversalCmdBuffer::AddPostamble()
{

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    const auto*const pPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    if ((m_cachedSettings.nggWdPageFaultWa != 0) && (pPipeline != nullptr) && pPipeline->IsNgg())
    {
        // In order to force the hardware (specifically, the WD block) back into legacy mode we must perform a draw.
        // We also need to make sure that all NGG state is disabled. This state is located in VGT_SHADER_STAGES_EN and
        // VGT_INDEX_TYPE.

        const regVGT_SHADER_STAGES_EN vgtShaderStagesEn = { };
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmVGT_SHADER_STAGES_EN,
                                                          vgtShaderStagesEn.u32All,
                                                          pDeCmdSpace);
        const uint32 vgtIndexType = 0;
        pDeCmdSpace += m_cmdUtil.BuildIndexType(vgtIndexType, pDeCmdSpace);
        pDeCmdSpace += m_cmdUtil.BuildDrawIndexAuto(0, Pm4Predicate::PredDisable, pDeCmdSpace);
    }

    if (m_gfxCmdBufState.cpBltActive)
    {
        // Stalls the CP ME until the CP's DMA engine has finished all previous "CP blts" (DMA_DATA commands
        // without the sync bit set). The ring won't wait for CP DMAs to finish so we need to do this manually.
        pDeCmdSpace += m_cmdUtil.BuildWaitDmaData(pDeCmdSpace);
        SetGfxCmdBufCpBltState(false);
    }

    if (m_ceCmdStream.GetNumChunks() > 0)
    {
        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();

        const bool ceTimestampNeeded = (m_ceCmdStream.GetFirstChunk()->BusyTrackerGpuAddr() != 0);
        if (ceTimestampNeeded || (UseEmbeddedDataForCeRamDumps() == false))
        {
            // The timestamps used for reclaiming command stream chunks are written when the DE stream has completed.
            // This ensures the CE stream completes before the DE stream completes, so that the timestamp can't return
            // before CE work is complete.
            pCeCmdSpace += m_cmdUtil.BuildIncrementCeCounter(pCeCmdSpace);
            pDeCmdSpace += m_cmdUtil.BuildWaitOnCeCounter(false, pDeCmdSpace);

            m_state.flags.deCounterDirty = 1;
        }

        if (UseEmbeddedDataForCeRamDumps() == false)
        {
            AcquireMemInfo acquireInfo = {};
            acquireInfo.flags.invSqI$ = 1;
            acquireInfo.flags.invSqK$ = 1;
            acquireInfo.tcCacheOp     = TcCacheOp::InvL1;
            acquireInfo.engineType    = EngineTypeUniversal;
            acquireInfo.baseAddress   = FullSyncBaseAddr;
            acquireInfo.sizeBytes     = FullSyncSize;

            // Additionally, since the GPU memory allocation used for CE-managed ring buffers is the same for every
            // command buffer, we need to make sure the CE doesn't get too far ahead and leap into the next chained
            // command buffer before the DE can catch up. We need to flush the Kcache in-between command buffers
            // because a command buffer transition is effectively the same as wrapping the CE ring buffer since we
            // jump back to the first ring slot.
            pCeCmdSpace += m_cmdUtil.BuildWaitOnDeCounterDiff(1, pCeCmdSpace);

            if (IsComputeSupported())
            {
                pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, EngineTypeUniversal, pDeCmdSpace);
            }

            pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PS_PARTIAL_FLUSH, EngineTypeUniversal, pDeCmdSpace);
            pDeCmdSpace += m_cmdUtil.BuildAcquireMem(acquireInfo, pDeCmdSpace);
        }

        // Finally, increment the DE counter after all of the synchronization is complete (if necessary).
        pDeCmdSpace = IncrementDeCounter(pDeCmdSpace);

        m_ceCmdStream.CommitCommands(pCeCmdSpace);

        // The following ATOMIC_MEM packet increments the done-count for the CE command stream, so that we can probe
        // when the command buffer has completed execution on the GPU.
        // NOTE: Normally, we would need to flush the L2 cache to guarantee that this memory operation makes it out to
        // memory. However, since we're at the end of the command buffer, we can rely on the fact that the KMD inserts
        // an EOP event which flushes and invalidates the caches in between command buffers.
        if (ceTimestampNeeded)
        {
            pDeCmdSpace += m_cmdUtil.BuildAtomicMem(AtomicOp::AddInt32,
                                                    m_ceCmdStream.GetFirstChunk()->BusyTrackerGpuAddr(),
                                                    1,
                                                    pDeCmdSpace);
        }
    }

    // The following ATOMIC_MEM packet increments the done-count for the DE command stream, so that we can probe
    // when the command buffer has completed execution on the GPU.
    // NOTE: Normally, we would need to flush the L2 cache to guarantee that this memory operation makes it out to
    // memory. However, since we're at the end of the command buffer, we can rely on the fact that the KMD inserts
    // an EOP event which flushes and invalidates the caches in between command buffers.
    if (m_deCmdStream.GetFirstChunk()->BusyTrackerGpuAddr() != 0)
    {
        pDeCmdSpace += m_cmdUtil.BuildAtomicMem(AtomicOp::AddInt32,
                                                m_deCmdStream.GetFirstChunk()->BusyTrackerGpuAddr(),
                                                1,
                                                pDeCmdSpace);
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);

    return Result::Success;
}

// =====================================================================================================================
// Adds commands necessary to write "data" to the specified memory
void UniversalCmdBuffer::WriteEventCmd(
    const BoundGpuMemory& boundMemObj,
    HwPipePoint           pipePoint,
    uint32                data)
{
    const EngineType  engineType = GetEngineType();

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    if ((pipePoint >= HwPipePostBlt) && (m_gfxCmdBufState.cpBltActive))
    {
        // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
        // the CmdSetEvent and CmdResetEvent functions expect that the prior blts have reached the post-blt stage by
        // the time the event is written to memory. Given that our CP DMA blts are asynchronous to the pipeline stages
        // the only way to satisfy this requirement is to force the MEC to stall until the CP DMAs are completed.
        pDeCmdSpace += m_cmdUtil.BuildWaitDmaData(pDeCmdSpace);
        SetGfxCmdBufCpBltState(false);
    }

    if (pipePoint == HwPipePostBlt)
    {
        // HwPipePostBlt barrier optimization
        pipePoint = OptimizeHwPipePostBlit();
    }

    // Prepare RELEASE_MEM packet build info.
    ReleaseMemInfo releaseInfo = {};
    releaseInfo.engineType     = engineType;
    releaseInfo.tcCacheOp      = TcCacheOp::Nop;
    releaseInfo.dstAddr        = boundMemObj.GpuVirtAddr();
    releaseInfo.dataSel        = data_sel__me_release_mem__send_32_bit_low;
    releaseInfo.data           = data;

    switch (pipePoint)
    {
    case HwPipeTop:
        // Implement set/reset event with a WRITE_DATA command using either the PFP.
        pDeCmdSpace += m_cmdUtil.BuildWriteData(engineType,
                                                boundMemObj.GpuVirtAddr(),
                                                1,
                                                engine_sel__pfp_write_data__prefetch_parser,
                                                dst_sel__me_write_data__memory,
                                                wr_confirm__me_write_data__wait_for_write_confirmation,
                                                &data,
                                                PredDisable,
                                                pDeCmdSpace);
        break;

    case HwPipePostCs:
        // If this trips, expect a hang.
        PAL_ASSERT(IsComputeSupported());
        // break intentionally left out!

    case HwPipePostIndexFetch:
    case HwPipePreRasterization:
    case HwPipePostPs:
        // Implement set/reset with an EOS event waiting for VS/PS or CS waves to complete.  Unfortunately, there is
        // no VS_DONE event with which to implement HwPipePreRasterization, so it has to conservatively use PS_DONE.
        releaseInfo.vgtEvent = (pipePoint == HwPipePostCs) ? CS_DONE : PS_DONE;
        pDeCmdSpace += m_cmdUtil.BuildReleaseMem(releaseInfo, pDeCmdSpace);
        break;

    case HwPipeBottom:
        // Implement set/reset with an EOP event written when all prior GPU work completes.
        releaseInfo.vgtEvent = BOTTOM_OF_PIPE_TS;
        pDeCmdSpace += m_cmdUtil.BuildReleaseMem(releaseInfo, pDeCmdSpace);
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Gets the command stream associated with the specified engine
CmdStream* UniversalCmdBuffer::GetCmdStreamByEngine(
    uint32 engineType) // Mask of Engine types as defined in gfxCmdBufer.h
{
    return TestAnyFlagSet(m_engineSupport, engineType) ? &m_deCmdStream : nullptr;
}

// =====================================================================================================================
// Helper function to synchronize the CE and DE counters at draw or dispatch time when a CE RAM dump was performed prior
// to the draw/dispatch operation.
void UniversalCmdBuffer::SynchronizeCeDeCounters(
    uint32** ppDeCmdSpace,  // [in,out] Command space write location for the DE stream
    uint32** ppCeCmdSpace)  // [in,out] Command space write location for the CE stream
{
    PAL_ASSERT(m_state.flags.ceStreamDirty != 0);

    *ppCeCmdSpace += m_cmdUtil.BuildIncrementCeCounter(*ppCeCmdSpace);
    *ppDeCmdSpace += m_cmdUtil.BuildWaitOnCeCounter((m_state.flags.ceInvalidateKcache != 0), *ppDeCmdSpace);

    m_state.flags.ceInvalidateKcache    = 0;
    m_state.flags.ceStreamDirty         = 0;
    m_state.flags.deCounterDirty        = 1;
}

// =====================================================================================================================
// Helper function to increment the DE counter.
uint32* UniversalCmdBuffer::IncrementDeCounter(
    uint32* pDeCmdSpace)
{
    if (m_state.flags.deCounterDirty != 0)
    {
        pDeCmdSpace += m_cmdUtil.BuildIncrementDeCounter(pDeCmdSpace);

        m_state.flags.deCounterDirty = 0;
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Helper function which updates the GPU virtual address of a CE ring table for a compute pipeline. The address is
// written to either SPI user-data registers or the compute spill table.
void UniversalCmdBuffer::UpdateCeRingAddressCs(
    UserDataTableState* pTable,
    uint16              firstEntry,
    uint32**            ppCeCmdSpace,
    uint32**            ppDeCmdSpace)
{
    PAL_ASSERT(pTable->gpuAddrDirty != 0);

    const uint32 gpuVirtAddrLo = LowPart(pTable->gpuVirtAddr);

    if (firstEntry >= m_pSignatureCs->spillThreshold)
    {
        // Should never get here when dumping to indirect offset
        PAL_ASSERT(m_state.flags.useIndirectAddrForCe == false);

        (*ppCeCmdSpace) = UploadToUserDataTableCeRam(m_cmdUtil,
                                                     &m_spillTable.stateCs,
                                                     firstEntry,
                                                     1,
                                                     &gpuVirtAddrLo,
                                                     m_pSignatureCs->userDataLimit,
                                                     *ppCeCmdSpace);
        // NOTE: Both spill tables share the same ring memory, so we need to dirty the other one in case there
        // are a long series of Draws in-between Dispatches (or vice-versa).
        m_spillTable.stateGfx.contentsDirty = 1;
    }
    else
    {
        UserDataArgs userDataArgs;
        userDataArgs.firstEntry   = firstEntry;
        userDataArgs.entryCount   = 1;
        userDataArgs.pEntryValues = &gpuVirtAddrLo;

        if (m_state.flags.useIndirectAddrForCe)
        {
            (*ppDeCmdSpace) = m_deCmdStream.WriteUserDataRegisterOffset<ShaderCompute>(m_pSignatureCs->stage,
                                                                                       &userDataArgs,
                                                                                       *ppDeCmdSpace);
        }
        else
        {
            (*ppDeCmdSpace) = m_deCmdStream.WriteUserDataRegisters(m_pSignatureCs->stage,
                                                                   &userDataArgs,
                                                                   ShaderCompute,
                                                                   *ppDeCmdSpace);
        }
    }

    WideBitfieldSetBit(m_computeState.csUserDataEntries.touched, firstEntry);
    m_computeState.csUserDataEntries.entries[firstEntry] = gpuVirtAddrLo;

    pTable->gpuAddrDirty = 0;
}

// =====================================================================================================================
// Helper function which updates the GPU virtual address of a CE ring table for a graphics pipeline. The address is
// written to either SPI user-data registers or the graphics spill table.
void UniversalCmdBuffer::UpdateCeRingAddressGfx(
    UserDataTableState* pTable,
    uint16              firstEntry,
    uint32              firstStage,    // First graphics HW shader stage to write
    uint32              lastStage,     // Last graphics HW shader stage to write
    uint32**            ppCeCmdSpace,
    uint32**            ppDeCmdSpace)
{
    PAL_ASSERT(pTable->gpuAddrDirty != 0);

    const uint32 gpuVirtAddrLo = LowPart(pTable->gpuVirtAddr);

    if (firstEntry >= m_pSignatureGfx->spillThreshold)
    {
        // Should never get here when dumping to indirect offset
        PAL_ASSERT(m_state.flags.useIndirectAddrForCe == false);

        (*ppCeCmdSpace) = UploadToUserDataTableCeRam(m_cmdUtil,
                                                     &m_spillTable.stateGfx,
                                                     firstEntry,
                                                     1,
                                                     &gpuVirtAddrLo,
                                                     m_pSignatureGfx->userDataLimit,
                                                     *ppCeCmdSpace);
        // NOTE: Both spill tables share the same ring memory, so we need to dirty the other one in case there
        // are a long series of Draws in-between Dispatches (or vice-versa).
        m_spillTable.stateCs.contentsDirty = 1;
    }

    UserDataArgs userDataArgs;
    userDataArgs.firstEntry   = firstEntry;
    userDataArgs.entryCount   = 1;
    userDataArgs.pEntryValues = &gpuVirtAddrLo;

    for (uint32 stage = firstStage; stage <= lastStage; ++stage)
    {
        if (m_state.flags.useIndirectAddrForCe)
        {
            (*ppDeCmdSpace) = m_deCmdStream.WriteUserDataRegisterOffset<ShaderGraphics>(
                                                                   m_pSignatureGfx->stage[stage],
                                                                   &userDataArgs,
                                                                   *ppDeCmdSpace);
        }
        else
        {
            (*ppDeCmdSpace) = m_deCmdStream.WriteUserDataRegisters(m_pSignatureGfx->stage[stage],
                                                                   &userDataArgs,
                                                                   ShaderGraphics,
                                                                   *ppDeCmdSpace);
        }
    }

    WideBitfieldSetBit(m_graphicsState.gfxUserDataEntries.touched, firstEntry);
    m_graphicsState.gfxUserDataEntries.entries[firstEntry] = gpuVirtAddrLo;

    pTable->gpuAddrDirty = 0;
}

// =====================================================================================================================
// Helper function to do all Dispatch-time validation for CE RAM user-data tables and for the compute spill-table.
template <bool useRingBufferForCe>
uint32* PAL_STDCALL UniversalCmdBuffer::ValidateComputeUserDataTables(
    UniversalCmdBuffer* pSelf,
    uint32*             pDeCmdSpace)
{
    // Step (1):
    // <> If any of the indirect user-data tables were dirtied since the previous Dispatch, those tables need to
    //    be relocated to a new ring position.
    for (uint16 id = 0; id < MaxIndirectUserDataTables; ++id)
    {
        if ((pSelf->m_pSignatureCs->indirectTableAddr[id] != UserDataNotMapped) &&
            (pSelf->m_indirectUserDataInfo[id].state.contentsDirty != 0) &&
            (pSelf->m_indirectUserDataInfo[id].watermark > 0))
        {
            pSelf->m_state.flags.ceStreamDirty = 1; // We will be dumping CE RAM later-on, so mark this dirty up front.

            if (useRingBufferForCe)
            {
                RelocateUserDataTable<true>(pSelf,
                                            &pSelf->m_state,
                                            &pSelf->m_indirectUserDataInfo[id].state,
                                            &pSelf->m_indirectUserDataInfo[id].ring,
                                            &pSelf->m_nestedIndirectCeDumpTable.ring,
                                            0,
                                            pSelf->m_indirectUserDataInfo[id].watermark);
            }
            else
            {
                RelocateUserDataTable<false>(pSelf,
                                             &pSelf->m_state,
                                             &pSelf->m_indirectUserDataInfo[id].state,
                                             &pSelf->m_indirectUserDataInfo[id].ring,
                                             &pSelf->m_nestedIndirectCeDumpTable.ring,
                                             0,
                                             pSelf->m_indirectUserDataInfo[id].watermark);
            }
        }
    }

    // NOTE: If a CE RAM dump has occurred since the previous Dispatch, (or we know we'll be dumping CE RAM here),
    // we need to do some CE work.
    if ((pSelf->m_state.flags.ceStreamDirty != 0) || (pSelf->m_spillTable.stateCs.contentsDirty != 0))
    {
        uint32* pCeCmdSpace = pSelf->m_ceCmdStream.ReserveCommands();

        // Step (2):
        // <> If any of the indirect user-data tables were relocated above, we'll also need to dump their CE RAM to
        //    GPU memory.
        // <> If that table is either relocated or has a dirty GPU address, we need to re-write the user-data entries
        //    which the pipeline uses to read the table's GPU addresses.
        for (uint16 id = 0; id < MaxIndirectUserDataTables; ++id)
        {
            if (pSelf->m_pSignatureCs->indirectTableAddr[id] != UserDataNotMapped)
            {
                if ((pSelf->m_indirectUserDataInfo[id].state.contentsDirty != 0) &&
                    (pSelf->m_indirectUserDataInfo[id].watermark > 0))
                {
                    pCeCmdSpace = DumpUserDataTableCeRam(pSelf->m_cmdUtil,
                                                         &pSelf->m_state,
                                                         &pSelf->m_indirectUserDataInfo[id].state,
                                                         0,
                                                         pSelf->m_indirectUserDataInfo[id].watermark,
                                                         pCeCmdSpace);
                }

                if (pSelf->m_indirectUserDataInfo[id].state.gpuAddrDirty != 0)
                {
                    pSelf->UpdateCeRingAddressCs(&pSelf->m_indirectUserDataInfo[id].state,
                                                 (pSelf->m_pSignatureCs->indirectTableAddr[id] - 1),
                                                 &pCeCmdSpace,
                                                 &pDeCmdSpace);
                }
            }
        }

        // Step (3):
        // <> The spill table is now completely up-to-date in CE RAM (including any of the CE table addresses which
        //    may have been spilled in step #3!). We need to relocate the spill table to a new ring position, and
        //    dump its contents to GPU memory.
        if ((pSelf->m_pSignatureCs->spillThreshold != NoUserDataSpilling) &&
            (pSelf->m_spillTable.stateCs.contentsDirty != 0))
        {
            const uint32 offsetInDwords = pSelf->m_pSignatureCs->spillThreshold;
            const uint32 sizeInDwords   = (pSelf->m_pSignatureCs->userDataLimit - offsetInDwords);

            if (useRingBufferForCe)
            {
                RelocateUserDataTable<true>(pSelf,
                                            &pSelf->m_state,
                                            &pSelf->m_spillTable.stateCs,
                                            &pSelf->m_spillTable.ring,
                                            &pSelf->m_nestedIndirectCeDumpTable.ring,
                                            offsetInDwords,
                                            sizeInDwords);
            }
            else
            {
                RelocateUserDataTable<false>(pSelf,
                                             &pSelf->m_state,
                                             &pSelf->m_spillTable.stateCs,
                                             &pSelf->m_spillTable.ring,
                                             &pSelf->m_nestedIndirectCeDumpTable.ring,
                                             offsetInDwords,
                                             sizeInDwords);
            }

            pCeCmdSpace = DumpUserDataTableCeRam(pSelf->m_cmdUtil,
                                                 &pSelf->m_state,
                                                 &pSelf->m_spillTable.stateCs,
                                                 offsetInDwords,
                                                 sizeInDwords,
                                                 pCeCmdSpace);
        }

        // Step (4):
        // <> A CE/DE counter sync to make sure that any CE RAM dumps finish before the dispatch begins will be taken
        //    care of at the end of ValidateDispatch(), before the actual dispatch.
        pSelf->m_ceCmdStream.CommitCommands(pCeCmdSpace);
    }
    else
    {
        for (uint16 id = 0; id < MaxIndirectUserDataTables; ++id)
        {
            if ((pSelf->m_pSignatureCs->indirectTableAddr[id] != UserDataNotMapped) &&
                (pSelf->m_indirectUserDataInfo[id].state.gpuAddrDirty != 0))
            {
                pSelf->UpdateCeRingAddressCs(&pSelf->m_indirectUserDataInfo[id].state,
                                             (pSelf->m_pSignatureCs->indirectTableAddr[id] - 1),
                                             nullptr,
                                             &pDeCmdSpace);
            }
        }
    }

    // Step (5):
    // <> If the spill table's GPU address was updated earlier in this function, we need to re-write the SPI user-data
    //    register(s) which contain the table's GPU address.
    if ((pSelf->m_spillTable.stateCs.gpuAddrDirty != 0) &&
        (pSelf->m_pSignatureCs->stage.spillTableRegAddr != UserDataNotMapped))
    {
        if (pSelf->m_state.flags.useIndirectAddrForCe)
        {
            pDeCmdSpace = pSelf->m_deCmdStream.WriteSetShRegDataOffset<ShaderCompute>(
                                                                        pSelf->m_pSignatureCs->stage.spillTableRegAddr,
                                                                        pSelf->m_spillTable.stateCs.gpuVirtAddr,
                                                                        pDeCmdSpace);
        }
        else
        {
            pDeCmdSpace =  pSelf->m_deCmdStream.WriteSetOneShReg<ShaderCompute>(
                                                                 pSelf->m_pSignatureCs->stage.spillTableRegAddr,
                                                                 LowPart(pSelf->m_spillTable.stateCs.gpuVirtAddr),
                                                                 pDeCmdSpace);
        }
        pSelf->m_spillTable.stateCs.gpuAddrDirty = 0;
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Helper function to do all Draw-time validation for CE RAM user-data tables and for the graphics spill-table.
template <bool useRingBufferForCe>
uint32* PAL_STDCALL UniversalCmdBuffer::ValidateGraphicsUserDataTables(
    UniversalCmdBuffer* pSelf,
    uint32*             pDeCmdSpace)
{
    constexpr uint32 StreamOutTableDwords = (sizeof(pSelf->m_streamOut.srd) / sizeof(uint32));

    // Step (1):
    // <> If any of the stream-out or indirect user-data tables were dirtied since the previous Draw, those tables
    //    need to be relocated to a new ring position.
    if ((pSelf->m_pSignatureGfx->streamOutTableAddr != UserDataNotMapped) &&
        (pSelf->m_streamOut.state.contentsDirty != 0))
    {
        if (useRingBufferForCe)
        {
            RelocateUserDataTable<true>(pSelf,
                                        &pSelf->m_state,
                                        &pSelf->m_streamOut.state,
                                        &pSelf->m_streamOut.ring,
                                        &pSelf->m_nestedIndirectCeDumpTable.ring,
                                        0,
                                        StreamOutTableDwords);
        }
        else
        {
            RelocateUserDataTable<false>(pSelf,
                                         &pSelf->m_state,
                                         &pSelf->m_streamOut.state,
                                         &pSelf->m_streamOut.ring,
                                         &pSelf->m_nestedIndirectCeDumpTable.ring,
                                         0,
                                         StreamOutTableDwords);
        }

        pSelf->m_state.flags.ceStreamDirty = 1; // We will be dumping CE RAM later-on, so mark this dirty up front.
    }
    for (uint16 id = 0; id < MaxIndirectUserDataTables; ++id)
    {
        if ((pSelf->m_pSignatureGfx->indirectTableAddr[id] != UserDataNotMapped) &&
            (pSelf->m_indirectUserDataInfo[id].state.contentsDirty != 0) &&
            (pSelf->m_indirectUserDataInfo[id].watermark > 0))
        {
            if (useRingBufferForCe)
            {
                RelocateUserDataTable<true>(pSelf,
                                            &pSelf->m_state,
                                            &pSelf->m_indirectUserDataInfo[id].state,
                                            &pSelf->m_indirectUserDataInfo[id].ring,
                                            &pSelf->m_nestedIndirectCeDumpTable.ring,
                                            0,
                                            pSelf->m_indirectUserDataInfo[id].watermark);
            }
            else
            {
                RelocateUserDataTable<false>(pSelf,
                                             &pSelf->m_state,
                                             &pSelf->m_indirectUserDataInfo[id].state,
                                             &pSelf->m_indirectUserDataInfo[id].ring,
                                             &pSelf->m_nestedIndirectCeDumpTable.ring,
                                             0,
                                             pSelf->m_indirectUserDataInfo[id].watermark);
            }

            pSelf->m_state.flags.ceStreamDirty = 1; // We will be dumping CE RAM later-on, so mark this dirty up front.
        }
    }

    // NOTE: If a CE RAM dump has occurred since the previous Draw, (or we know we'll be dumping CE RAM here), we
    // need to do some CE work.
    if ((pSelf->m_state.flags.ceStreamDirty != 0) || (pSelf->m_spillTable.stateGfx.contentsDirty != 0))
    {
        uint32* pCeCmdSpace = pSelf->m_ceCmdStream.ReserveCommands();

        // Step (2):
        // <> If any of the stream-out or indirect user-data tables were relocated above, we'll also need to dump
        //    their CE RAM to GPU memory.
        // <> If that table is either relocated or has a dirty GPU address, we need to re-write the user-data entries
        //    which the pipeline uses to read the table's GPU addresses.
        if (pSelf->m_pSignatureGfx->streamOutTableAddr != UserDataNotMapped)
        {
            if (pSelf->m_streamOut.state.contentsDirty != 0)
            {
                pCeCmdSpace = DumpUserDataTableCeRam(pSelf->m_cmdUtil,
                                                     &pSelf->m_state,
                                                     &pSelf->m_streamOut.state,
                                                     0,
                                                     StreamOutTableDwords,
                                                     pCeCmdSpace);
            }
            if (pSelf->m_streamOut.state.gpuAddrDirty != 0)
            {
                pSelf->UpdateCeRingAddressGfx(&pSelf->m_streamOut.state,
                                              (pSelf->m_pSignatureGfx->streamOutTableAddr - 1),
                                              static_cast<uint32>(HwShaderStage::Vs),
                                              static_cast<uint32>(HwShaderStage::Vs),
                                              &pCeCmdSpace,
                                              &pDeCmdSpace);
            }
        }
        for (uint16 id = 0; id < MaxIndirectUserDataTables; ++id)
        {
            if ((pSelf->m_pSignatureGfx->indirectTableAddr[id] != UserDataNotMapped) &&
                (pSelf->m_indirectUserDataInfo[id].watermark > 0))
            {
                if (pSelf->m_indirectUserDataInfo[id].state.contentsDirty != 0)
                {
                    pCeCmdSpace = DumpUserDataTableCeRam(pSelf->m_cmdUtil,
                                                         &pSelf->m_state,
                                                         &pSelf->m_indirectUserDataInfo[id].state,
                                                         0,
                                                         pSelf->m_indirectUserDataInfo[id].watermark,
                                                         pCeCmdSpace);
                }
                if (pSelf->m_indirectUserDataInfo[id].state.gpuAddrDirty != 0)
                {
                    pSelf->UpdateCeRingAddressGfx(&pSelf->m_indirectUserDataInfo[id].state,
                                                  (pSelf->m_pSignatureGfx->indirectTableAddr[id] - 1),
                                                  static_cast<uint32>(HwShaderStage::Hs),
                                                  static_cast<uint32>(HwShaderStage::Ps),
                                                  &pCeCmdSpace,
                                                  &pDeCmdSpace);
                }
            }
        }

        // Step (3):
        // <> The spill table is now completely up-to-date in CE RAM (including any of the CE table addresses which
        //    may have been spilled in step #3!). We need to relocate the spill table to a new ring position, and
        //    dump its contents to GPU memory.
        if ((pSelf->m_pSignatureGfx->spillThreshold != NoUserDataSpilling) &&
            (pSelf->m_spillTable.stateGfx.contentsDirty != 0))
        {
            const uint32 offsetInDwords = pSelf->m_pSignatureGfx->spillThreshold;
            const uint32 sizeInDwords   = (pSelf->m_pSignatureGfx->userDataLimit - offsetInDwords);

            if (useRingBufferForCe)
            {
                RelocateUserDataTable<true>(pSelf,
                                            &pSelf->m_state,
                                            &pSelf->m_spillTable.stateGfx,
                                            &pSelf->m_spillTable.ring,
                                            &pSelf->m_nestedIndirectCeDumpTable.ring,
                                            offsetInDwords,
                                            sizeInDwords);
            }
            else
            {
                RelocateUserDataTable<false>(pSelf,
                                             &pSelf->m_state,
                                             &pSelf->m_spillTable.stateGfx,
                                             &pSelf->m_spillTable.ring,
                                             &pSelf->m_nestedIndirectCeDumpTable.ring,
                                             offsetInDwords,
                                             sizeInDwords);
            }

            pCeCmdSpace = DumpUserDataTableCeRam(pSelf->m_cmdUtil,
                                                 &pSelf->m_state,
                                                 &pSelf->m_spillTable.stateGfx,
                                                 offsetInDwords,
                                                 sizeInDwords,
                                                 pCeCmdSpace);
        }

        // Step (4):
        // <> A CE/DE counter sync to make sure that any CE RAM dumps finish before the draw begins will be taken
        //    care of at the end of ValidateDraw(), before the actual draw.
        pSelf->m_ceCmdStream.CommitCommands(pCeCmdSpace);
    }
    else
    {
        if ((pSelf->m_pSignatureGfx->streamOutTableAddr != UserDataNotMapped) &&
            (pSelf->m_streamOut.state.gpuAddrDirty != 0))
        {
            pSelf->UpdateCeRingAddressGfx(&pSelf->m_streamOut.state,
                                          (pSelf->m_pSignatureGfx->streamOutTableAddr - 1),
                                          static_cast<uint32>(HwShaderStage::Vs),
                                          static_cast<uint32>(HwShaderStage::Vs),
                                          nullptr,
                                          &pDeCmdSpace);
        }
        for (uint16 id = 0; id < MaxIndirectUserDataTables; ++id)
        {
            if ((pSelf->m_pSignatureGfx->indirectTableAddr[id] != UserDataNotMapped) &&
                (pSelf->m_indirectUserDataInfo[id].state.gpuAddrDirty != 0))
            {
                pSelf->UpdateCeRingAddressGfx(&pSelf->m_indirectUserDataInfo[id].state,
                                              (pSelf->m_pSignatureGfx->indirectTableAddr[id] - 1),
                                              static_cast<uint32>(HwShaderStage::Hs),
                                              static_cast<uint32>(HwShaderStage::Ps),
                                              nullptr,
                                              &pDeCmdSpace);
            }
        }
    }

    // Step (5):
    // <> If the spill table's GPU address was updated earlier in this function, we need to re-write the SPI user-data
    //    register(s) which contain the table's GPU address.
    if (pSelf->m_spillTable.stateGfx.gpuAddrDirty != 0)
    {
        const uint32 gpuVirtAddrLo = LowPart(pSelf->m_spillTable.stateGfx.gpuVirtAddr);
        for (uint32 i = 0; i < NumHwShaderStagesGfx; ++i)
        {
            if (pSelf->m_pSignatureGfx->stage[i].spillTableRegAddr != UserDataNotMapped)
            {
                if (pSelf->m_state.flags.useIndirectAddrForCe)
                {
                    pDeCmdSpace = pSelf->m_deCmdStream.WriteSetShRegDataOffset<ShaderGraphics>(
                                                                 pSelf->m_pSignatureGfx->stage[i].spillTableRegAddr,
                                                                 gpuVirtAddrLo,
                                                                 pDeCmdSpace);
                }
                else
                {
                    pDeCmdSpace =  pSelf->m_deCmdStream.WriteSetOneShReg<ShaderGraphics>(
                                                                 pSelf->m_pSignatureGfx->stage[i].spillTableRegAddr,
                                                                 gpuVirtAddrLo,
                                                                 pDeCmdSpace);
                }
            }
        }

        pSelf->m_spillTable.stateGfx.gpuAddrDirty = 0;
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.  Wrapper to determine
// if immediate mode pm4 optimization is enabled before calling the real ValidateDraw() function.
template <bool indexed, bool indirect>
uint32* UniversalCmdBuffer::ValidateDraw(
    const ValidateDrawInfo& drawInfo,      // Draw info
    uint32* pDeCmdSpace)                   // Write new draw-engine commands here.
{
    if (m_deCmdStream.Pm4ImmediateOptimizerEnabled())
    {
        pDeCmdSpace = ValidateDraw<indexed, indirect, true>(drawInfo, pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = ValidateDraw<indexed, indirect, false>(drawInfo, pDeCmdSpace);
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.  Wrapper to determine
// if the pipeline is dirty before calling the real ValidateDraw() function.
template <bool indexed, bool indirect, bool pm4OptImmediate>
uint32* UniversalCmdBuffer::ValidateDraw(
    const ValidateDrawInfo& drawInfo,
    uint32*                 pDeCmdSpace)
{
    if (m_graphicsState.pipelineState.dirtyFlags.pipelineDirty)
    {
        pDeCmdSpace = ValidateDraw<indexed, indirect, pm4OptImmediate, true>(drawInfo, pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = ValidateDraw<indexed, indirect, pm4OptImmediate, false>(drawInfo, pDeCmdSpace);
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.  Wrapper to determine
// if any interesting state is dirty before calling the real ValidateDraw() function.
template <bool indexed, bool indirect, bool pm4OptImmediate, bool pipelineDirty>
uint32* UniversalCmdBuffer::ValidateDraw(
    const ValidateDrawInfo& drawInfo,
    uint32*                 pDeCmdSpace)
{
    if (m_graphicsState.dirtyFlags.validationBits.u16All)
    {
        pDeCmdSpace = ValidateDraw<indexed, indirect, pm4OptImmediate, pipelineDirty, true>(drawInfo, pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = ValidateDraw<indexed, indirect, pm4OptImmediate, pipelineDirty, false>(drawInfo, pDeCmdSpace);
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.  Wrapper to determine
// if the pipeline is NGG before calling the real ValidateDraw() function.
template <bool indexed, bool indirect, bool pm4OptImmediate, bool pipelineDirty, bool stateDirty>
uint32* UniversalCmdBuffer::ValidateDraw(
    const ValidateDrawInfo& drawInfo,
    uint32*                 pDeCmdSpace)
{
    if (static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline)->IsNgg())
    {
        pDeCmdSpace = ValidateDraw<indexed,
                                   indirect,
                                   pm4OptImmediate,
                                   pipelineDirty,
                                   stateDirty,
                                   true>(drawInfo, pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = ValidateDraw<indexed,
                                   indirect,
                                   pm4OptImmediate,
                                   pipelineDirty,
                                   stateDirty,
                                   false>(drawInfo, pDeCmdSpace);
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.  Wrapper to determine
// if the pipeline is NGG Fast Launch before calling the real ValidateDraw() function.
template <bool indexed,
          bool indirect,
          bool pm4OptImmediate,
          bool pipelineDirty,
          bool stateDirty,
          bool isNgg>
uint32* UniversalCmdBuffer::ValidateDraw(
    const ValidateDrawInfo& drawInfo,
    uint32*                 pDeCmdSpace)
{
    if (isNgg && static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline)->IsNggFastLaunch())
    {
        pDeCmdSpace = ValidateDraw<indexed,
            indirect,
            pm4OptImmediate,
            pipelineDirty,
            stateDirty,
            isNgg,
            true>(drawInfo, pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = ValidateDraw<indexed,
            indirect,
            pm4OptImmediate,
            pipelineDirty,
            stateDirty,
            isNgg,
            false>(drawInfo, pDeCmdSpace);
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// This function updates the m_nggTable ring buffer that tracks the state necessary for an NGG pipeline to execute
// Returns a pointer to the next entry in the DE cmd space.  This function can not write any context registers!
uint32* UniversalCmdBuffer::UpdateNggRingData(
    uint32*  pDeCmdSpace)
{
    auto* pNggRing  = &m_nggTable.ring;
    auto* pNggState = &m_nggTable.state;

    // If nothing has changed, then there's no need to do anything...
    if (m_nggState.flags.dirty.triangleRasterState ||
        m_nggState.flags.dirty.viewports           ||
        m_nggState.flags.dirty.scissorRects        ||
        m_nggState.flags.dirty.inputAssemblyState  ||
        m_graphicsState.pipelineState.dirtyFlags.pipelineDirty)
    {
        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();

        if (m_nggState.flags.dirty.triangleRasterState)
        {
            const uint32 rasterStateSize = NumBytesToNumDwords(sizeof(m_state.primShaderCbLayout.renderStateCb));
            const uint32 ceRamOffset     = m_nggCbCeRamOffset + offsetof(Abi::PrimShaderCbLayout, renderStateCb);

            pCeCmdSpace += m_cmdUtil.BuildWriteConstRam(&m_state.primShaderCbLayout.renderStateCb,
                                                        ceRamOffset,
                                                        rasterStateSize,
                                                        pCeCmdSpace);
        }

        if (m_nggState.flags.dirty.viewports)
        {
            const uint32 viewportDataSize = NumBytesToNumDwords(sizeof(m_state.primShaderCbLayout.viewportStateCb));
            const uint32 ceRamOffset      = m_nggCbCeRamOffset + offsetof(Abi::PrimShaderCbLayout, viewportStateCb);

            pCeCmdSpace += m_cmdUtil.BuildWriteConstRam(&m_state.primShaderCbLayout.viewportStateCb,
                                                        ceRamOffset,
                                                        viewportDataSize,
                                                        pCeCmdSpace);
        }

        if (m_nggState.flags.dirty.scissorRects)
        {
            const uint32 scissorDataSize = NumBytesToNumDwords(sizeof(m_state.primShaderCbLayout.scissorStateCb));
            const uint32 ceRamOffset     = m_nggCbCeRamOffset + offsetof(Abi::PrimShaderCbLayout, scissorStateCb);

            pCeCmdSpace += m_cmdUtil.BuildWriteConstRam(&m_state.primShaderCbLayout.scissorStateCb,
                                                        ceRamOffset,
                                                        scissorDataSize,
                                                        pCeCmdSpace);
        }

        if (m_graphicsState.pipelineState.dirtyFlags.pipelineDirty ||
            m_nggState.flags.dirty.viewports                       ||
            m_nggState.flags.dirty.triangleRasterState             ||
            m_nggState.flags.dirty.inputAssemblyState)
        {
            const uint32 pipelineDataSize = NumBytesToNumDwords(sizeof(m_state.primShaderCbLayout.pipelineStateCb));
            const uint32 ceRamOffset      = m_nggCbCeRamOffset + offsetof(Abi::PrimShaderCbLayout, pipelineStateCb);

            pCeCmdSpace += m_cmdUtil.BuildWriteConstRam(&m_state.primShaderCbLayout.pipelineStateCb,
                                                        ceRamOffset,
                                                        pipelineDataSize,
                                                        pCeCmdSpace);
        }

        // At this point, the CE RAM is updated...  need to dump into the next entry in m_nggTable ring.
        const uint32 dwordRingSize = NumBytesToNumDwords(sizeof(Abi::PrimShaderCbLayout));

        // There's no such thing as embedded data for the NGG ring data
        RelocateUserDataTable<true>(this,
                                    &m_state,
                                    pNggState,
                                    pNggRing,
                                    &m_nestedIndirectCeDumpTable.ring,
                                    0,
                                    dwordRingSize);

        pCeCmdSpace = DumpUserDataTableCeRam(m_cmdUtil,
                                             &m_state,
                                             pNggState,
                                             0, // offset in dwords, we're dumping everything
                                             dwordRingSize,
                                             pCeCmdSpace);

        m_ceCmdStream.CommitCommands(pCeCmdSpace);

        m_nggState.flags.dirty.u8All = 0;
    }

    if (pNggState->gpuAddrDirty != 0)
    {
#if PAL_DBG_COMMAND_COMMENTS
        pDeCmdSpace += m_cmdUtil.BuildCommentString("NGG: ConstantBufferAddr", pDeCmdSpace);
#endif
        const gpusize  baseAddr = Get256BAddrLo(pNggState->gpuVirtAddr);

        // The address of the constant buffer is stored in the GS shader address registers.
        if (m_state.flags.useIndirectAddrForCe)
        {
            // WriteSetShRegDataOffset writes the base address from SET_BASE added to the value passed into this
            // function to the register location requested. However, this register (SPI_SHADER_PGM_{LO/HI}_GS) is
            // a 256B aligned address. Until CP adds support for this, just assert.
            PAL_NOT_IMPLEMENTED();

            //pDeCmdSpace = m_deCmdStream.WriteSetShRegDataOffset<ShaderGraphics>(mmSPI_SHADER_PGM_LO_GS,
            //                                                                    LowPart(baseAddr),
            //                                                                    pDeCmdSpace);
            //
            //pDeCmdSpace = m_deCmdStream.WriteSetShRegDataOffset<ShaderGraphics>(mmSPI_SHADER_PGM_HI_GS,
            //                                                                    HighPart(baseAddr),
            //                                                                    pDeCmdSpace);
        }
        else
        {
            pDeCmdSpace = m_deCmdStream.WriteSetSeqShRegs(mmSPI_SHADER_PGM_LO_GS,
                                                          mmSPI_SHADER_PGM_HI_GS,
                                                          ShaderGraphics,
                                                          &baseAddr,
                                                          pDeCmdSpace);
        }

        pNggState->gpuAddrDirty = 0;
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.
template <bool indexed,
          bool indirect,
          bool pm4OptImmediate,
          bool pipelineDirty,
          bool stateDirty,
          bool isNgg,
          bool isNggFastLaunch>
uint32* UniversalCmdBuffer::ValidateDraw(
    const ValidateDrawInfo& drawInfo,      // Draw info
    uint32*                 pDeCmdSpace)   // Write new draw-engine commands here.
{
    const auto*const pBlendState = static_cast<const ColorBlendState*>(m_graphicsState.pColorBlendState);
    const auto*const pDepthState = static_cast<const DepthStencilState*>(m_graphicsState.pDepthStencilState);
    const auto*const pPipeline   = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const auto*const pMsaaState  = static_cast<const MsaaState*>(m_graphicsState.pMsaaState);
    const auto*const pDsView =
        static_cast<const DepthStencilView*>(m_graphicsState.bindTargets.depthTarget.pDepthStencilView);

    const auto& dirtyFlags = m_graphicsState.dirtyFlags.validationBits;

    // If we're about to launch a draw we better have a pipeline bound.
    PAL_ASSERT(pPipeline != nullptr);

    // All of our dirty state will leak to the caller.
    m_graphicsState.leakFlags.u32All |= m_graphicsState.dirtyFlags.u32All;

    // Make sure the contents of all graphics user-data tables and the spill-table are up-to-date.
    pDeCmdSpace = (*m_pfnValidateUserDataTablesGfx)(this, pDeCmdSpace);

    if (pipelineDirty || (stateDirty && dirtyFlags.colorBlendState))
    {
        // Blend state optimizations are associated with the Blend state object, but the CB state affects which
        // optimizations are chosen. We need to make sure we have the best optimizations chosen, so we write it at draw
        // time only if it is dirty.
        if (pBlendState != nullptr)
        {
            pDeCmdSpace = pBlendState->WriteBlendOptimizations<pm4OptImmediate>(
                &m_deCmdStream,
                pPipeline->TargetFormats(),
                pPipeline->TargetWriteMasks(),
                m_cachedSettings.blendOptimizationsEnable,
                &m_blendOpts[0],
                pDeCmdSpace);
        }
    }

    // Writing the viewport and scissor-rect state is deferred until draw-time because they depend on both the
    // viewport/scissor-rect state and the active pipeline.
    if (stateDirty && dirtyFlags.viewports)
    {
        pDeCmdSpace = ValidateViewports<pm4OptImmediate>(pDeCmdSpace);
    }

    regPA_SC_MODE_CNTL_1 paScModeCntl1 = m_drawTimeHwState.paScModeCntl1;

    // Re-calculate paScModeCntl1 value if state constributing to the register has changed.
    if ((m_drawTimeHwState.valid.paScModeCntl1 == 0) ||
        pipelineDirty ||
        (stateDirty && (dirtyFlags.depthStencilState || dirtyFlags.colorBlendState || dirtyFlags.depthStencilView)))
    {
        paScModeCntl1 = pPipeline->PaScModeCntl1();

        if (pPipeline->IsOutOfOrderPrimsEnabled() == false)
        {
            paScModeCntl1.bits.OUT_OF_ORDER_PRIMITIVE_ENABLE = pPipeline->CanDrawPrimsOutOfOrder(
                pDsView,
                pDepthState,
                pBlendState,
                MayHaveActiveQueries(),
                static_cast<Gfx9OutOfOrderPrimMode>(m_cachedSettings.outOfOrderPrimsEnable));
        }
        if (m_state.flags.optimizeLinearGfxCpy)
        {
            // UBM performance test shows that if dst image is linear when doing graphics copy, disable super tile
            // walk and fence pattern walk will boost up to 33% performance.
            paScModeCntl1.bits.WALK_SIZE         = 1;
            paScModeCntl1.bits.WALK_FENCE_ENABLE = 0;
        }
    }

    regDB_COUNT_CONTROL dbCountControl = m_drawTimeHwState.dbCountControl;
    if (stateDirty && (dirtyFlags.msaaState || dirtyFlags.queryState))
    {
        // MSAA sample rates are associated with the MSAA state object, but the sample rate affects how queries are
        // processed (via DB_COUNT_CONTROL). We need to update the value of this register at draw-time since it is
        // affected by multiple elements of command-buffer state.
        const uint32 log2OcclusionQuerySamples = (pMsaaState != nullptr) ? pMsaaState->Log2OcclusionQuerySamples() : 0;
        pDeCmdSpace = UpdateDbCountControl<pm4OptImmediate>(log2OcclusionQuerySamples, &dbCountControl, pDeCmdSpace);
    }

    // Before we do per-draw HW state validation we need to get a copy of the current IA_MULTI_VGT_PARAM register. This
    // is also where we do things like force WdSwitchOnEop and optimize the primgroup size.
    const bool            wdSwitchOnEop   = ForceWdSwitchOnEop(*pPipeline, drawInfo);
    regIA_MULTI_VGT_PARAM iaMultiVgtParam = pPipeline->IaMultiVgtParam(wdSwitchOnEop);
    regVGT_LS_HS_CONFIG   vgtLsHsConfig   = pPipeline->VgtLsHsConfig();

    // NGG cannot change IA_MULTI_VGT_PARAM from the already precalculated results in VGT_GS_ONCHIP_CNTL.
    if (isNgg == false)
    {
        if (m_primGroupOpt.optimalSize > 0)
        {
            iaMultiVgtParam.bits.PRIMGROUP_SIZE = m_primGroupOpt.optimalSize - 1;
        }
    }

    // MSAA num samples are associated with the MSAA state object, but inner coverage affects how many samples are
    // required. We need to update the value of this register.
    // When the pixel shader uses inner coverage the rasterizer needs another "sample" to hold the inner coverage
    // result.
    const uint32 log2MsaaStateSamples = (pMsaaState != nullptr) ? pMsaaState->Log2NumSamples() : 0;
    const uint32 log2TotalSamples     = log2MsaaStateSamples + (pPipeline->UsesInnerCoverage() ? 1 : 0);
    const bool   newAaConfigSamples   = (m_log2NumSamples != log2TotalSamples);

    if ((stateDirty && dirtyFlags.msaaState) ||
        newAaConfigSamples                   ||
        (m_state.flags.paScAaConfigUpdated == 0))
    {
        m_state.flags.paScAaConfigUpdated = 1;
        m_log2NumSamples = log2TotalSamples;

        regPA_SC_AA_CONFIG paScAaConfig = {};
        paScAaConfig.bits.MSAA_NUM_SAMPLES = log2TotalSamples;
        pDeCmdSpace = m_deCmdStream.WriteContextRegRmw<pm4OptImmediate>(mmPA_SC_AA_CONFIG,
                                                                        PA_SC_AA_CONFIG__MSAA_NUM_SAMPLES_MASK,
                                                                        paScAaConfig.u32All,
                                                                        pDeCmdSpace);
        m_deCmdStream.SetContextRollDetected<true>();
    }

    bool disableDfsm = m_cachedSettings.disableDfsm;
    if (disableDfsm == false)
    {
        const auto*  pDepthTargetView =
            reinterpret_cast<const DepthStencilView*>(m_graphicsState.bindTargets.depthTarget.pDepthStencilView);
        const auto*  pDepthImage = (pDepthTargetView ? pDepthTargetView->GetImage() : nullptr);

        // Is the setting configured such that this workaround is meaningful, and
        // Do we have a depth image, and
        // If the bound depth image has changed or if the number of samples programmed into PA_SC_AA_CONFIG has changed,
        // then we need to do some further checking.
        const bool checkDfsmEqaaWa = m_cachedSettings.checkDfsmEqaaWa &&
                                     (pDepthImage != nullptr) &&
                                     ((stateDirty && dirtyFlags.depthStencilView) || newAaConfigSamples);

        // Is the setting configured such that we want to disable DFSM when the PS uses UAVs or ROVs, and
        // Has the current bound pipeline changed?
        const bool checkDfsmPsUav  = m_cachedSettings.disableDfsmPsUav && pipelineDirty;

        // If we're in EQAA for the purposes of this JIRA then we have to kill DFSM.
        // Remember that the register is programmed in terms of log2, while the create info struct is in terms of
        // actual samples.
        if (checkDfsmEqaaWa && (1u << m_log2NumSamples) != pDepthImage->Parent()->GetImageCreateInfo().samples)
        {
            disableDfsm = true;
        }

        if (checkDfsmPsUav && (pPipeline->PsUsesUavs() || pPipeline->PsUsesRovs()))
        {
            disableDfsm = true;
        }

        if (disableDfsm)
        {
            pDeCmdSpace += m_cmdUtil.BuildContextRegRmw(
                m_cmdUtil.GetRegInfo().mmDbDfsmControl,
                DB_DFSM_CONTROL__PUNCHOUT_MODE_MASK,
                (DfsmPunchoutModeDisable << DB_DFSM_CONTROL__PUNCHOUT_MODE__SHIFT),
                pDeCmdSpace);
            m_deCmdStream.SetContextRollDetected<true>();
        }
    }

    // We shouldn't rewrite the PBB bin sizes unless at least one of these state objects has changed
    if (pipelineDirty || (stateDirty && (dirtyFlags.colorBlendState  ||
                                         dirtyFlags.colorTargetView  ||
                                         dirtyFlags.depthStencilView ||
                                         dirtyFlags.depthStencilState)))
    {
        // Accessing pipeline state in this function is usually a cache miss, so avoid function call
        // when only when pipeline has changed.
        if (pipelineDirty)
        {
            m_pbbStateOverride = pPipeline->GetBinningOverride();
        }
        bool shouldEnablePbb = (m_pbbStateOverride == BinningOverride::Enable);

        if (m_pbbStateOverride == BinningOverride::Default)
        {
            shouldEnablePbb = ShouldEnablePbb(*pPipeline, pBlendState, pDepthState, pMsaaState);
        }

        if (m_enabledPbb != shouldEnablePbb)
        {
            m_enabledPbb = shouldEnablePbb;
            pDeCmdSpace  = ValidateBinSizes<pm4OptImmediate>(*pPipeline, pBlendState, disableDfsm, pDeCmdSpace);
        }
    }

    // Validate primitive restart enable.  Primitive restart should only apply for indexed draws, but on gfx9,
    // VGT also applies it to auto-generated vertex index values.
    //

    regVGT_MULTI_PRIM_IB_RESET_EN vgtMultiPrimIbResetEn = {};

    vgtMultiPrimIbResetEn.bits.RESET_EN = static_cast<uint32>(
        indexed && m_graphicsState.inputAssemblyState.primitiveRestartEnable);

    m_state.primShaderCbLayout.renderStateCb.primitiveRestartEnable = vgtMultiPrimIbResetEn.bits.RESET_EN;

    // Validate the per-draw HW state.
    pDeCmdSpace = ValidateDrawTimeHwState<indexed,
                                          indirect,
                                          isNgg,
                                          isNggFastLaunch,
                                          pm4OptImmediate>(iaMultiVgtParam,
                                                           vgtLsHsConfig,
                                                           paScModeCntl1,
                                                           dbCountControl,
                                                           vgtMultiPrimIbResetEn,
                                                           drawInfo,
                                                           pDeCmdSpace);

    pDeCmdSpace = m_workaroundState.PreDraw<indirect, stateDirty, pm4OptImmediate>(m_graphicsState,
                                                                                   &m_deCmdStream,
                                                                                   this,
                                                                                   drawInfo,
                                                                                   pDeCmdSpace);

    if (isNgg)
    {
        // This function will not write any context registers
        pDeCmdSpace = UpdateNggRingData(pDeCmdSpace);
    }

    // If any validation about dumped from CE RAM to GPU memory, we need to add commands to synchronize the draw and
    // constant engines before we issue the upcoming Draw.
    if (m_state.flags.ceStreamDirty != 0)
    {
        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
        SynchronizeCeDeCounters(&pDeCmdSpace, &pCeCmdSpace);
        m_ceCmdStream.CommitCommands(pCeCmdSpace);
    }

    // Clear the dirty-state flags.
    m_graphicsState.dirtyFlags.u32All               = 0;
    m_graphicsState.pipelineState.dirtyFlags.u32All = 0;
    m_deCmdStream.ResetDrawTimeState();

    m_state.flags.firstDrawExecuted = 1;

    return pDeCmdSpace;
}

// =====================================================================================================================
// MMRT = (num_frag == 1) ? 1 : (ps_iter == 1) ? num_frag : 2
// CMRT = Bpp * MMRT
Extent2d UniversalCmdBuffer::GetColorBinSize() const
{
    const auto& boundTargets = m_graphicsState.bindTargets;
    const auto* pPipeline    = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const bool  psIterSample = ((pPipeline != nullptr) && (pPipeline->PaScModeCntl1().bits.PS_ITER_SAMPLE == 1));
    uint32      cColor       = 0;

    // TODO: This function needs to be updated to look at the pixel shader and determine which outputs are valid in
    //       addition to looking at the bound render targets. Bound render targets may not necessarily get a pixel
    //       shader export. Using the bound render targets means that we may make the bin size smaller than it needs to
    //       be when a render target is bound, but is not written by the PS. With export cull mask enabled. We need only
    //       examine the PS output because it will account for any RTs that are not bound.

    for (uint32  idx = 0; idx < boundTargets.colorTargetCount; idx++)
    {
        const auto* pColorView = static_cast<const ColorTargetView*>(boundTargets.colorTargets[idx].pColorTargetView);
        const auto* pImage     = ((pColorView != nullptr) ? pColorView->GetImage() : nullptr);

        if (pImage != nullptr)
        {
            const auto&  info = pImage->Parent()->GetImageCreateInfo();
            const uint32 mmrt = (info.fragments == 1) ? 1 : (psIterSample ? info.fragments : 2);

            cColor += BytesPerPixel(info.swizzledFormat.format) * mmrt;
        }
    }

    static constexpr CtoBinSize BinSize[][3][8]=
    {
        {
            // One RB / SE
            {
                // One shader engine
                {        0,  128,  128 },
                {        1,   64,  128 },
                {        2,   32,  128 },
                {        3,   16,  128 },
                {       17,    0,    0 },
                { UINT_MAX,    0,    0 },
            },
            {
                // Two shader engines
                {        0,  128,  128 },
                {        2,   64,  128 },
                {        3,   32,  128 },
                {        5,   16,  128 },
                {       17,    0,    0 },
                { UINT_MAX,    0,    0 },
            },
            {
                // Four shader engines
                {        0,  128,  128 },
                {        3,   64,  128 },
                {        5,   16,  128 },
                {       17,    0,    0 },
                { UINT_MAX,    0,    0 },
            },
        },
        {
            // Two RB / SE
            {
                // One shader engine
                {        0,  128,  128 },
                {        2,   64,  128 },
                {        3,   32,  128 },
                {        5,   16,  128 },
                {       33,    0,    0 },
                { UINT_MAX,    0,    0 },
            },
            {
                // Two shader engines
                {        0,  128,  128 },
                {        3,   64,  128 },
                {        5,   32,  128 },
                {        9,   16,  128 },
                {       33,    0,    0 },
                { UINT_MAX,    0,    0 },
            },
            {
                // Four shader engines
                {        0,  256,  256 },
                {        2,  128,  256 },
                {        3,  128,  128 },
                {        5,   64,  128 },
                {        9,   16,  128 },
                {       33,    0,    0 },
                { UINT_MAX,    0,    0 },
            },
        },
        {
            // Four RB / SE
            {
                // One shader engine
                {        0,  128,  256 },
                {        2,  128,  128 },
                {        3,   64,  128 },
                {        5,   32,  128 },
                {        9,   16,  128 },
                {       33,    0,    0 },
                { UINT_MAX,    0,    0 },
            },
            {
                // Two shader engines
                {        0,  256,  256 },
                {        2,  128,  256 },
                {        3,  128,  128 },
                {        5,   64,  128 },
                {        9,   32,  128 },
                {       17,   16,  128 },
                {       33,    0,    0 },
                { UINT_MAX,    0,    0 },
            },
            {
                // Four shader engines
                {        0,  256,  512 },
                {        2,  256,  256 },
                {        3,  128,  256 },
                {        5,  128,  128 },
                {        9,   64,  128 },
                {       17,   16,  128 },
                {       33,    0,    0 },
                { UINT_MAX,    0,    0 },
            },
        },
    };

    const CtoBinSize* pBinEntry = GetBinSizeValue(&BinSize[m_log2NumRbPerSe][m_log2NumSes][0], cColor);
    const Extent2d    size      = { pBinEntry->binSizeX, pBinEntry->binSizeY };

    return size;
}

// =====================================================================================================================
// C_per_sample = ((z_enabled) ? 5 : 0) + ((stencil_enabled) ? 1 : 0)
// C = 4 * C_per_sample * num_samples
Extent2d UniversalCmdBuffer::GetDepthBinSize() const
{
    // Set to max sizes in case there is no depth image bound
    Extent2d size = { 512, 512 };

    const auto*  pDepthTargetView =
            static_cast<const DepthStencilView*>(m_graphicsState.bindTargets.depthTarget.pDepthStencilView);
    const auto*  pImage           = (pDepthTargetView ? pDepthTargetView->GetImage() : nullptr);

    if (pImage != nullptr)
    {
        const auto* pDepthStencilState = static_cast<const DepthStencilState*>(m_graphicsState.pDepthStencilState);
        const auto& imageCreateInfo    = pImage->Parent()->GetImageCreateInfo();
        const bool  supportsDepth      = m_device.Parent()->SupportsDepth(imageCreateInfo.swizzledFormat.format,
                                                                          imageCreateInfo.tiling);
        const bool  supportsStencil    = m_device.Parent()->SupportsStencil(imageCreateInfo.swizzledFormat.format,
                                                                            imageCreateInfo.tiling);

        const uint32 cPerDepthSample   = (pDepthStencilState->IsDepthEnabled() &&
                                          supportsDepth                        &&
                                          (pDepthTargetView->GetDsViewCreateInfo().flags.readOnlyDepth == 0)) ? 5 : 0;
        const uint32 cPerStencilSample = (pDepthStencilState->IsStencilEnabled() &&
                                          supportsStencil                        &&
                                          (pDepthTargetView->GetDsViewCreateInfo().flags.readOnlyStencil == 0)) ? 1 : 0;
        const uint32 cDepth            = 4 * (cPerDepthSample + cPerStencilSample) * imageCreateInfo.samples;

        static constexpr CtoBinSize BinSize[][3][9]=
        {
            {
                // One RB / SE
                {
                    // One shader engine
                    {        0,  128,  256 },
                    {        2,  128,  128 },
                    {        4,   64,  128 },
                    {        7,   32,  128 },
                    {       13,   16,  128 },
                    {       49,    0,    0 },
                    { UINT_MAX,    0,    0 },
                },
                {
                    // Two shader engines
                    {        0,  256,  256 },
                    {        2,  128,  256 },
                    {        4,  128,  128 },
                    {        7,   64,  128 },
                    {       13,   32,  128 },
                    {       25,   16,  128 },
                    {       49,    0,    0 },
                    { UINT_MAX,    0,    0 },
                },
                {
                    // Four shader engines
                    {        0,  256,  512 },
                    {        2,  256,  256 },
                    {        4,  128,  256 },
                    {        7,  128,  128 },
                    {       13,   64,  128 },
                    {       25,   16,  128 },
                    {       49,    0,    0 },
                    { UINT_MAX,    0,    0 },
                },
            },
            {
                // Two RB / SE
                {
                    // One shader engine
                    {        0,  256,  256 },
                    {        2,  128,  256 },
                    {        4,  128,  128 },
                    {        7,   64,  128 },
                    {       13,   32,  128 },
                    {       25,   16,  128 },
                    {       97,    0,    0 },
                    { UINT_MAX,    0,    0 },
                },
                {
                    // Two shader engines
                    {        0,  256,  512 },
                    {        2,  256,  256 },
                    {        4,  128,  256 },
                    {        7,  128,  128 },
                    {       13,   64,  128 },
                    {       25,   32,  128 },
                    {       49,   16,  128 },
                    {       97,    0,    0 },
                    { UINT_MAX,    0,    0 },
                },
                {
                    // Four shader engines
                    {        0,  512,  512 },
                    {        2,  256,  512 },
                    {        4,  256,  256 },
                    {        7,  128,  256 },
                    {       13,  128,  128 },
                    {       25,   64,  128 },
                    {       49,   16,  128 },
                    {       97,    0,    0 },
                    { UINT_MAX,    0,    0 },
                },
            },
            {
                // Four RB / SE
                {
                    // One shader engine
                    {        0,  256,  512 },
                    {        2,  256,  256 },
                    {        4,  128,  256 },
                    {        7,  128,  128 },
                    {       13,   64,  128 },
                    {       25,   32,  128 },
                    {       49,   16,  128 },
                    { UINT_MAX,    0,    0 },
                },
                {
                    // Two shader engines
                    {        0,  512,  512 },
                    {        2,  256,  512 },
                    {        4,  256,  256 },
                    {        7,  128,  256 },
                    {       13,  128,  128 },
                    {       25,   64,  128 },
                    {       49,   32,  128 },
                    {       97,   16,  128 },
                    { UINT_MAX,    0,    0 },
                },
                {
                    // Four shader engines
                    {        0,  512,  512 },
                    {        4,  256,  512 },
                    {        7,  256,  256 },
                    {       13,  128,  256 },
                    {       25,  128,  128 },
                    {       49,   64,  128 },
                    {       97,   16,  128 },
                    { UINT_MAX,    0,    0 },
                },
            },
        };

        const CtoBinSize*  pBinEntry  = GetBinSizeValue(&BinSize[m_log2NumRbPerSe][m_log2NumSes][0], cDepth);

        size.width  = pBinEntry->binSizeX;
        size.height = pBinEntry->binSizeY;
    }

    return size;
}

// =====================================================================================================================
// Returns the PA_SC_BINNER_CNTL_0 register value that corresponds to the specified bin sizes.
void UniversalCmdBuffer::SetPaScBinnerCntl0(
    const GraphicsPipeline&  pipeline,
    const ColorBlendState*   pColorBlendState,
    Extent2d                 size,
    bool                     disableDfsm)
{
    m_paScBinnerCntl0.u32All = 0;

    // If the reported bin sizes are zero, then disable binning
    if ((size.width == 0) || (size.height == 0))
    {
        m_paScBinnerCntl0.bits.BINNING_MODE = DISABLE_BINNING_USE_LEGACY_SC;
    }
    else
    {
        m_paScBinnerCntl0.bits.BINNING_MODE              = BINNING_ALLOWED;
        m_paScBinnerCntl0.bits.CONTEXT_STATES_PER_BIN    = m_savedPaScBinnerCntl0.bits.CONTEXT_STATES_PER_BIN;
        m_paScBinnerCntl0.bits.PERSISTENT_STATES_PER_BIN = m_savedPaScBinnerCntl0.bits.PERSISTENT_STATES_PER_BIN;
        m_paScBinnerCntl0.bits.FPOVS_PER_BATCH           = m_savedPaScBinnerCntl0.bits.FPOVS_PER_BATCH;
        m_paScBinnerCntl0.bits.OPTIMAL_BIN_SELECTION     = m_savedPaScBinnerCntl0.bits.OPTIMAL_BIN_SELECTION;

        if (size.width == 16)
        {
            m_paScBinnerCntl0.bits.BIN_SIZE_X = 1;
        }
        else
        {
            m_paScBinnerCntl0.bits.BIN_SIZE_X_EXTEND = Device::GetBinSizeEnum(size.width);
        }

        if (size.height == 16)
        {
            m_paScBinnerCntl0.bits.BIN_SIZE_Y = 1;
        }
        else
        {
            m_paScBinnerCntl0.bits.BIN_SIZE_Y_EXTEND = Device::GetBinSizeEnum(size.height);
        }
    }

    m_paScBinnerCntl0.bits.DISABLE_START_OF_PRIM = 1;

    const uint32 colorTargetCount = m_graphicsState.bindTargets.colorTargetCount;

    if ((disableDfsm == false)                                   &&
        (colorTargetCount > 0)                                   &&
        (m_paScBinnerCntl0.bits.BINNING_MODE == BINNING_ALLOWED) &&
        (pipeline.PsAllowsPunchout() == true))
    {
        const bool blendingEnabled = ((pColorBlendState != nullptr)
                                      ? ((pColorBlendState->BlendEnableMask() & ((1 << colorTargetCount) - 1)) != 0)
                                      : false);

        if (blendingEnabled == false)
        {
            m_paScBinnerCntl0.bits.DISABLE_START_OF_PRIM = 0;
        }
    }

}

// =====================================================================================================================
bool UniversalCmdBuffer::ShouldEnablePbb(
    const GraphicsPipeline&  pipeline,
    const ColorBlendState*   pColorBlendState,
    const DepthStencilState* pDepthState,
    const MsaaState*         pMsaaState
    ) const
{
    bool disableBinning = true;

    if (m_cachedSettings.disableBatchBinning == 0)
    {
        // To improve performance we may wish to disable binning depending on the following logic.

        // Whether or not we can disable binning based on the pixel shader, MSAA states, and write-bound.
        // For MSAA, its ALPHA_TO_MASK_ENABLE bit is controlled by ALPHA_TO_MASK_DISABLE field of pipeline state's
        // DB_SHADER_CONTROL register, so we should check that bit instead.
        const auto*  pDepthView =
            static_cast<const DepthStencilView*>(m_graphicsState.bindTargets.depthTarget.pDepthStencilView);
        const bool canKill      = pipeline.PsTexKill() ||
                                  ((pMsaaState != nullptr) && pipeline.IsAlphaToMaskEnable());
        const bool canReject    = pipeline.PsCanTriviallyReject();
        const bool isWriteBound = (pDepthState != nullptr) &&
                                  (pDepthView  != nullptr) &&
                                  ((pDepthState->IsDepthWriteEnabled() &&
                                    (pDepthView->GetDsViewCreateInfo().flags.readOnlyDepth == 0)) ||
                                   (pDepthState->IsStencilWriteEnabled() &&
                                    (pDepthView->GetDsViewCreateInfo().flags.readOnlyStencil == 0)));

        const bool psKill       = (m_cachedSettings.disablePbbPsKill && canKill && canReject && isWriteBound);

        // Whether we can disable binning based on the depth stencil state.
        const bool depthUnbound = ((m_graphicsState.bindTargets.depthTarget.pDepthStencilView == nullptr) &&
                                   (pDepthState == nullptr));
        const bool noDepthWrite = ((pDepthState != nullptr)                      &&
                                   (pDepthState->IsDepthWriteEnabled() == false) &&
                                   (pDepthState->IsStencilWriteEnabled() == false));
        const bool dbDisabled   = (m_cachedSettings.disablePbbNoDb && (depthUnbound || noDepthWrite));

        // Whether we can disable binning based on the color blend state.
        const uint32 boundRenderTargetMask = (1 << m_graphicsState.bindTargets.colorTargetCount) - 1;
        const bool blendOff     = m_cachedSettings.disablePbbBlendingOff &&
                                  ((pColorBlendState == nullptr) ||
                                   ((boundRenderTargetMask & pColorBlendState->BlendEnableMask()) == 0) ||
                                    ((boundRenderTargetMask & pColorBlendState->BlendReadsDestMask()) == 0));

        // Whether we can disable binning based on if PS uses append and consume. Performance investigation has
        // found that the way binning affects the append/consume ordering hurts performance a lot.
        const bool appendConsumeEnabled = m_cachedSettings.disablePbbAppendConsume &&
                                          (pipeline.PsUsesAppendConsume() == 1);

        disableBinning = psKill || dbDisabled || blendOff || appendConsumeEnabled;
    }

    return (disableBinning == false);
}

// =====================================================================================================================
// Updates the bin sizes and writes to the register.
template <bool pm4OptImmediate>
uint32* UniversalCmdBuffer::ValidateBinSizes(
    const GraphicsPipeline&  pipeline,
    const ColorBlendState*   pColorBlendState,
    bool                     disableDfsm,
    uint32*                  pDeCmdSpace)
{
    // Default to a zero-sized bin to disable binning.
    Extent2d binSize = {};

    if (m_enabledPbb)
    {
        if ((m_customBinSizeX != 0) && (m_customBinSizeY != 0))
        {
            // The custom bin size is packed as two shorts.
            binSize.width  = m_customBinSizeX;
            binSize.height = m_customBinSizeY;
        }
        else
        {
            // Go through all the bound color targets and the depth target.
            const Extent2d colorBinSize = GetColorBinSize();
            const Extent2d depthBinSize = GetDepthBinSize();
            const uint32   colorArea    = colorBinSize.width * colorBinSize.height;
            const uint32   depthArea    = depthBinSize.width * depthBinSize.height;

            binSize = (colorArea < depthArea) ? colorBinSize : depthBinSize;
        }
    }

    // Update our copy of m_paScBinnerCntl0 and write it out.
    SetPaScBinnerCntl0(pipeline, pColorBlendState, binSize, disableDfsm);

    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg<pm4OptImmediate>(mmPA_SC_BINNER_CNTL_0,
                                                                       m_paScBinnerCntl0.u32All,
                                                                       pDeCmdSpace);
    m_deCmdStream.SetContextRollDetected<true>();

    return pDeCmdSpace;
}

// =====================================================================================================================
// Writes the latest set of viewports to HW. It is illegal to call this if the viewports aren't dirty.
template <bool pm4OptImmediate>
uint32* UniversalCmdBuffer::ValidateViewports(
    uint32* pDeCmdSpace)
{
    const auto& params = m_graphicsState.viewportState;
    PAL_ASSERT(m_graphicsState.dirtyFlags.validationBits.viewports != 0);

    const uint32 viewportCount       = (m_graphicsState.enableMultiViewport) ? params.count : 1;
    const uint32 numVportScaleRegs   = ((sizeof(VportScaleOffsetPm4Img) >> 2) * viewportCount);
    const uint32 numVportZMinMaxRegs = ((sizeof(VportZMinMaxPm4Img)     >> 2) * viewportCount);

    GuardbandPm4Img guardbandImg = {};
    PAL_ASSERT((params.horzClipRatio    >= 1.0f) &&
               (params.horzDiscardRatio >= 1.0f) &&
               (params.vertClipRatio    >= 1.0f) &&
               (params.vertDiscardRatio >= 1.0f));

    guardbandImg.paClGbHorzClipAdj.f32All = params.horzClipRatio;
    guardbandImg.paClGbHorzDiscAdj.f32All = params.horzDiscardRatio;
    guardbandImg.paClGbVertClipAdj.f32All = params.vertClipRatio;
    guardbandImg.paClGbVertDiscAdj.f32All = params.vertDiscardRatio;

    VportScaleOffsetPm4Img scaleOffsetImg[MaxViewports];
    for (uint32 i = 0; i < viewportCount; i++)
    {
        const auto&             viewport          = params.viewports[i];
        VportScaleOffsetPm4Img* pScaleOffsetImg   = &scaleOffsetImg[i];
        auto*                   pNggViewportCntls = &m_state.primShaderCbLayout.viewportStateCb.vportControls[i];

        float xScale = (viewport.width * 0.5f);
        float yScale = (viewport.height * 0.5f);

        pScaleOffsetImg->xScale.f32All  = xScale;
        pScaleOffsetImg->yScale.f32All  = yScale * (viewport.origin == PointOrigin::UpperLeft ? 1.0f : -1.0f);
        pScaleOffsetImg->zScale.f32All  = (viewport.maxDepth - viewport.minDepth);
        pScaleOffsetImg->xOffset.f32All = (viewport.originX + xScale);
        pScaleOffsetImg->yOffset.f32All = (viewport.originY + yScale);
        pScaleOffsetImg->zOffset.f32All = viewport.minDepth;

        // Calc the max acceptable X limit for guardband clipping.
        float left  = viewport.originX;
        float right = viewport.originX + viewport.width;
        // Swap left and right to correct negSize and posSize if width is negative
        if (viewport.width < 0)
        {
            left  = viewport.originX + viewport.width;
            right = viewport.originX;
            xScale = -xScale;
        }
        float negSize = (-MinHorzScreenCoord) + left;
        float posSize = MaxHorzScreenCoord - right;

        const float xLimit = Min(negSize, posSize);

        // Calc the max acceptable Y limit for guardband clipping.
        float top    = viewport.originY;
        float bottom = viewport.originY + viewport.height;

        // Swap top and bottom to correct negSize and posSize if height is negative
        if (viewport.height < 0)
        {
             top    = viewport.originY + viewport.height;
             bottom = viewport.originY;
             yScale = -yScale;
        }
        negSize = (-MinVertScreenCoord) + top;
        posSize = MaxVertScreenCoord - bottom;

        const float yLimit = Min(negSize, posSize);

        // Calculate this viewport's clip guardband scale factors.
        const float xClip = (xLimit + xScale) / xScale;
        const float yClip = (yLimit + yScale) / yScale;

        // Accumulate the clip guardband scales for all active viewports.
        guardbandImg.paClGbHorzClipAdj.f32All = Min(xClip, guardbandImg.paClGbHorzClipAdj.f32All);
        guardbandImg.paClGbVertClipAdj.f32All = Min(yClip, guardbandImg.paClGbVertClipAdj.f32All);

        static_assert((sizeof(*pNggViewportCntls) == sizeof(*pScaleOffsetImg)),
                      "NGG viewport struct must be same size as PAL viewport struct!");
        memcpy(pNggViewportCntls, pScaleOffsetImg, sizeof(*pNggViewportCntls));
    }

    static_assert((offsetof(GuardbandPm4Img, paClGbVertClipAdj) == 0),
                  "registers are out of order in PrimShaderPsoCb");
    auto*  pNggPipelineStateCb = &m_state.primShaderCbLayout.pipelineStateCb;
    pNggPipelineStateCb->paClGbHorzClipAdj = guardbandImg.paClGbHorzClipAdj.u32All;
    pNggPipelineStateCb->paClGbHorzDiscAdj = guardbandImg.paClGbHorzDiscAdj.u32All;
    pNggPipelineStateCb->paClGbVertClipAdj = guardbandImg.paClGbVertClipAdj.u32All;
    pNggPipelineStateCb->paClGbVertDiscAdj = guardbandImg.paClGbVertDiscAdj.u32All;

    pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs<pm4OptImmediate>(mmPA_CL_GB_VERT_CLIP_ADJ,
                                                                        mmPA_CL_GB_HORZ_DISC_ADJ,
                                                                        &guardbandImg,
                                                                        pDeCmdSpace);

    pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs<pm4OptImmediate>(mmPA_CL_VPORT_XSCALE,
                                                                        mmPA_CL_VPORT_XSCALE + numVportScaleRegs - 1,
                                                                        &scaleOffsetImg[0],
                                                                        pDeCmdSpace);

    VportZMinMaxPm4Img zMinMaxImg[MaxViewports];
    for (uint32 i = 0; i < viewportCount; i++)
    {
        const auto&         viewport    = params.viewports[i];
        VportZMinMaxPm4Img* pZMinMaxImg = reinterpret_cast<VportZMinMaxPm4Img*>(&zMinMaxImg[i]);

        pZMinMaxImg->zMin.f32All = Min(viewport.minDepth, viewport.maxDepth);
        pZMinMaxImg->zMax.f32All = Max(viewport.minDepth, viewport.maxDepth);
    }

    pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs<pm4OptImmediate>(mmPA_SC_VPORT_ZMIN_0,
                                                                        mmPA_SC_VPORT_ZMIN_0 + numVportZMinMaxRegs - 1,
                                                                        &zMinMaxImg[0],
                                                                        pDeCmdSpace);

    return pDeCmdSpace;
}

// =====================================================================================================================
// Wrapper for the real ValidateViewports() for when the caller doesn't know if the immediate mode pm4 optimizer is
// enabled.
uint32* UniversalCmdBuffer::ValidateViewports(
    uint32*    pDeCmdSpace)
{
    if (m_deCmdStream.Pm4ImmediateOptimizerEnabled())
    {
        pDeCmdSpace = ValidateViewports<true>(pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = ValidateViewports<false>(pDeCmdSpace);
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Wrapper for the real NeedsToValidateScissorRects() for when the caller doesn't know if the immediate mode pm4
// optimizer is enabled.
bool UniversalCmdBuffer::NeedsToValidateScissorRects() const
{
    return NeedsToValidateScissorRects(m_deCmdStream.Pm4ImmediateOptimizerEnabled());
}

// =====================================================================================================================
// Returns whether we need to validate scissor rects at draw time.
bool UniversalCmdBuffer::NeedsToValidateScissorRects(
    const bool pm4OptImmediate
    ) const
{
    const auto& dirtyFlags    = m_graphicsState.dirtyFlags;
    const auto& pipelineFlags = m_graphicsState.pipelineState.dirtyFlags;

    bool needsValidation = false;

    if (pm4OptImmediate)
    {
        // When PM4 optimizer is enabled ContextRollDetected() will detect all context rolls through the PM4
        // optimizer.
        needsValidation = (dirtyFlags.validationBits.scissorRects ||
                           (m_cachedSettings.scissorChangeWa && m_deCmdStream.ContextRollDetected()));
    }
    else
    {
        // When PM4 optimizer is disabled ContextRollDetected() represents individual context register writes in the
        // driver. Thus, if any other graphics state is dirtied we must assume a context roll has occurred.
        needsValidation = (dirtyFlags.validationBits.scissorRects ||
                           (m_cachedSettings.scissorChangeWa &&
                            (m_deCmdStream.ContextRollDetected()               ||
                             dirtyFlags.validationBits.colorBlendState         ||
                             dirtyFlags.validationBits.depthStencilState       ||
                             dirtyFlags.validationBits.msaaState               ||
                             dirtyFlags.validationBits.quadSamplePatternState  ||
                             dirtyFlags.validationBits.viewports               ||
                             dirtyFlags.validationBits.depthStencilView        ||
                             dirtyFlags.validationBits.inputAssemblyState      ||
                             dirtyFlags.validationBits.triangleRasterState     ||
                             dirtyFlags.validationBits.colorTargetView         ||
                             dirtyFlags.nonValidationBits.streamOutTargets     ||
                             dirtyFlags.nonValidationBits.globalScissorState   ||
                             dirtyFlags.nonValidationBits.blendConstState      ||
                             dirtyFlags.nonValidationBits.depthBiasState       ||
                             dirtyFlags.nonValidationBits.depthBoundsState     ||
                             dirtyFlags.nonValidationBits.pointLineRasterState ||
                             dirtyFlags.nonValidationBits.stencilRefMaskState  ||
                             pipelineFlags.borderColorPaletteDirty             ||
                             pipelineFlags.pipelineDirty)));
    }

    return needsValidation;
}

// =====================================================================================================================
// Fillout the Scissor Rects Register.
uint32 UniversalCmdBuffer::BuildScissorRectImage(
    ScissorRectPm4Img* pScissorRectImg
    ) const
{
    const auto& viewportState = m_graphicsState.viewportState;
    const auto& scissorState  = m_graphicsState.scissorRectState;

    const uint32 scissorCount       = (m_graphicsState.enableMultiViewport) ? scissorState.count : 1;
    const uint32 numScissorRectRegs = ((sizeof(ScissorRectPm4Img) >> 2) * scissorCount);

    // Number of rects need cross validation
    const uint32 numberCrossValidRects = Min(scissorCount, viewportState.count);

    for (uint32 i = 0; i < scissorCount; ++i)
    {
        const auto&        scissorRect = scissorState.scissors[i];
        ScissorRectPm4Img* pPm4Img     = pScissorRectImg + i;

        int32 left;
        int32 top;
        int32 right;
        int32 bottom;

        if (static_cast<TossPointMode>(m_cachedSettings.tossPointMode) != TossPointAfterSetup)
        {
            left   = scissorRect.offset.x;
            top    = scissorRect.offset.y;
            right  = scissorRect.offset.x + scissorRect.extent.width;
            bottom = scissorRect.offset.y + scissorRect.extent.height;

            // Cross-validation between scissor rects and viewport rects
            if (i < numberCrossValidRects)
            {
                const auto& viewportRect = viewportState.viewports[i];

                // Flush denorm to 0 before rounds to negative infinity.
                int32 viewportLeft   =
                    static_cast<int32>(Math::FlushDenormToZero(viewportRect.originX));
                int32 viewportTop    =
                    static_cast<int32>(Math::FlushDenormToZero(viewportRect.originY));
                int32 viewportRight  =
                    static_cast<int32>(Math::FlushDenormToZero(viewportRect.originX + viewportRect.width));
                int32 viewportBottom =
                    static_cast<int32>(Math::FlushDenormToZero(viewportRect.originY + viewportRect.height));

                left   = Max(viewportLeft, left);
                top    = Max(viewportTop, top);
                right  = Min(viewportRight, right);
                bottom = Min(viewportBottom, bottom);
            }
        }
        else
        {
            left   = 0;
            top    = 0;
            right  = 1;
            bottom = 1;
        }

        pPm4Img->tl.u32All = 0;
        pPm4Img->br.u32All = 0;

        pPm4Img->tl.bits.WINDOW_OFFSET_DISABLE = 1;
        pPm4Img->tl.bits.TL_X = Clamp<int32>(left,   0, ScissorMaxTL);
        pPm4Img->tl.bits.TL_Y = Clamp<int32>(top,    0, ScissorMaxTL);
        pPm4Img->br.bits.BR_X = Clamp<int32>(right,  0, ScissorMaxBR);
        pPm4Img->br.bits.BR_Y = Clamp<int32>(bottom, 0, ScissorMaxBR);
    }

    return numScissorRectRegs;
}

// =====================================================================================================================
// Writes the latest set of scissor-rects to HW. It is illegal to call this if the scissor-rects aren't dirty.
template <bool pm4OptImmediate>
uint32* UniversalCmdBuffer::ValidateScissorRects(
    uint32*    pDeCmdSpace)
{
    ScissorRectPm4Img scissorRectImg[MaxViewports];
    const uint32      numScissorRectRegs = BuildScissorRectImage(scissorRectImg);
    auto*             pNggScissorCntls   = &m_state.primShaderCbLayout.scissorStateCb.scissorControls[0];

    pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs<pm4OptImmediate>(
                                    mmPA_SC_VPORT_SCISSOR_0_TL,
                                    mmPA_SC_VPORT_SCISSOR_0_TL + numScissorRectRegs - 1,
                                    &scissorRectImg[0],
                                    pDeCmdSpace);

    static_assert((sizeof(*pNggScissorCntls) == sizeof(ScissorRectPm4Img)),
                  "NGG scissor structure doesn't match PAL internal structure!");
    memcpy(pNggScissorCntls, &scissorRectImg[0], numScissorRectRegs * sizeof(uint32));

    return pDeCmdSpace;
}

// =====================================================================================================================
// Wrapper for the real ValidateScissorRects() for when the caller doesn't know if the immediate pm4 optimizer is
// enabled.
uint32* UniversalCmdBuffer::ValidateScissorRects(
    uint32*    pDeCmdSpace)
{
    {
        if (m_deCmdStream.Pm4ImmediateOptimizerEnabled())
        {
            pDeCmdSpace = ValidateScissorRects<true>(pDeCmdSpace);
        }
        else
        {
            pDeCmdSpace = ValidateScissorRects<false>(pDeCmdSpace);
        }
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Update the HW state and write the necessary packets to push any changes to the HW. Returns the next unused DWORD
// in pDeCmdSpace.
template <bool indexed, bool indirect, bool isNgg, bool isNggFastLaunch, bool pm4OptImmediate>
uint32* UniversalCmdBuffer::ValidateDrawTimeHwState(
    regIA_MULTI_VGT_PARAM         iaMultiVgtParam,       // The draw preamble's IA_MULTI_VGT_PARAM register value.
    regVGT_LS_HS_CONFIG           vgtLsHsConfig,         // The draw preamble's VGT_LS_HS_CONFIG register value.
    regPA_SC_MODE_CNTL_1          paScModeCntl1,         // PA_SC_MODE_CNTL_1 register value.
    regDB_COUNT_CONTROL           dbCountControl,        // DB_COUNT_CONTROL register value.
    regVGT_MULTI_PRIM_IB_RESET_EN vgtMultiPrimIbResetEn, // VGT_MULTI_PRIM_IB_RESET_EN register value.
    const ValidateDrawInfo&       drawInfo,              // Draw info
    uint32*                       pDeCmdSpace)           // Write new draw-engine commands here.
{
    // Start with the IA_MULTI_VGT_PARAM regsiter.
    if ((m_drawTimeHwState.iaMultiVgtParam.u32All != iaMultiVgtParam.u32All) ||
        (m_drawTimeHwState.valid.iaMultiVgtParam == 0))
    {
        m_drawTimeHwState.iaMultiVgtParam.u32All = iaMultiVgtParam.u32All;
        m_drawTimeHwState.valid.iaMultiVgtParam  = 1;

        if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
        {
            pDeCmdSpace = m_deCmdStream.WriteSetOneConfigReg(mmIA_MULTI_VGT_PARAM__GFX09,
                                                             iaMultiVgtParam.u32All,
                                                             pDeCmdSpace,
                                                             index__pfp_set_uconfig_reg__multi_vgt_param);
        }
    }

    if ((m_drawTimeHwState.vgtMultiPrimIbResetEn.u32All != vgtMultiPrimIbResetEn.u32All) ||
        (m_drawTimeHwState.valid.vgtMultiPrimIbResetEn == 0))
    {
        m_drawTimeHwState.vgtMultiPrimIbResetEn.u32All = vgtMultiPrimIbResetEn.u32All;
        m_drawTimeHwState.valid.vgtMultiPrimIbResetEn  = 1;

        pDeCmdSpace = m_deCmdStream.WriteSetOneConfigReg(mmVGT_MULTI_PRIM_IB_RESET_EN__GFX09,
                                                         vgtMultiPrimIbResetEn.u32All,
                                                         pDeCmdSpace,
                                                         index__pfp_set_uconfig_reg__default);
    }

    if ((m_drawTimeHwState.vgtLsHsConfig.u32All != vgtLsHsConfig.u32All) ||
        (m_drawTimeHwState.valid.vgtLsHsConfig == 0))
    {
        m_drawTimeHwState.vgtLsHsConfig.u32All = vgtLsHsConfig.u32All;
        m_drawTimeHwState.valid.vgtLsHsConfig  = 1;

        pDeCmdSpace = m_deCmdStream.WriteSetVgtLsHsConfig<pm4OptImmediate>(vgtLsHsConfig, pDeCmdSpace);
        m_deCmdStream.SetContextRollDetected<true>();
    }

    if ((m_drawTimeHwState.paScModeCntl1.u32All != paScModeCntl1.u32All) ||
        (m_drawTimeHwState.valid.paScModeCntl1 == 0))
    {
        m_drawTimeHwState.paScModeCntl1.u32All = paScModeCntl1.u32All;
        m_drawTimeHwState.valid.paScModeCntl1  = 1;

        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg<pm4OptImmediate>(mmPA_SC_MODE_CNTL_1,
                                                                           paScModeCntl1.u32All,
                                                                           pDeCmdSpace);
        m_deCmdStream.SetContextRollDetected<true>();
    }

    if ((m_drawTimeHwState.dbCountControl.u32All != dbCountControl.u32All) ||
        (m_drawTimeHwState.valid.dbCountControl == 0))
    {
        m_drawTimeHwState.dbCountControl.u32All = dbCountControl.u32All;
        m_drawTimeHwState.valid.dbCountControl = 1;

        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg<pm4OptImmediate>(mmDB_COUNT_CONTROL,
                                                                           dbCountControl.u32All,
                                                                           pDeCmdSpace);
        m_deCmdStream.SetContextRollDetected<true>();
    }

    if (m_drawIndexReg != UserDataNotMapped)
    {
        if (indirect)
        {
            // If the active pipeline uses the draw index VS input value, then the PM4 draw packet to issue the multi
            // draw will blow-away the SPI user-data register used to pass that value to the shader.
            m_drawTimeHwState.valid.drawIndex = 0;
        }
        else if (m_drawTimeHwState.valid.drawIndex == 0)
        {
            // Otherwise, this SH register write will reset it to zero for us.
            m_drawTimeHwState.valid.drawIndex = 1;
            pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics, pm4OptImmediate>(m_drawIndexReg,
                                                                                          0,
                                                                                          pDeCmdSpace);
        }
    }
    // Write the INDEX_TYPE packet.
    // We might need to write this outside of indexed draws (for instance, on a change of NGG <-> Legacy pipeline).
    if ((m_drawTimeHwState.dirty.indexType != 0) || (indexed && (m_drawTimeHwState.dirty.indexedIndexType != 0)))
    {
        m_drawTimeHwState.dirty.indexType        = 0;
        m_drawTimeHwState.dirty.indexedIndexType = 0;
        {
             pDeCmdSpace += m_cmdUtil.BuildIndexType(m_vgtDmaIndexType.u32All, pDeCmdSpace);
        }
    }

    if (indexed)
    {
        // Note that leakFlags.iaState implies an IB has been bound.
        if (m_graphicsState.leakFlags.nonValidationBits.iaState == 1)
        {
            // Direct indexed draws use DRAW_INDEX_2 which contains the IB base and size. This means that
            // we only have to validate the IB base and size for indirect indexed draws.
            if (indirect)
            {
                // Write the INDEX_BASE packet.
                if (m_drawTimeHwState.dirty.indexBufferBase != 0)
                {
                    m_drawTimeHwState.dirty.indexBufferBase        = 0;
                    m_drawTimeHwState.valid.nggIndexBufferBaseAddr = 0;
                    pDeCmdSpace += m_cmdUtil.BuildIndexBase(m_graphicsState.iaState.indexAddr, pDeCmdSpace);
                }

                // Write the INDEX_BUFFER_SIZE packet.
                if (m_drawTimeHwState.dirty.indexBufferSize != 0)
                {
                    m_drawTimeHwState.dirty.indexBufferSize = 0;
                    pDeCmdSpace += m_cmdUtil.BuildIndexBufferSize(m_graphicsState.iaState.indexCount, pDeCmdSpace);
                }
            }
        }
    }

    if (indirect)
    {
        // The following state will be clobbered by the indirect draw packet.
        m_drawTimeHwState.valid.numInstances   = 0;
        m_drawTimeHwState.valid.instanceOffset = 0;
        m_drawTimeHwState.valid.vertexOffset   = 0;
    }
    else
    {
        // Write the vertex offset user data register.
        if ((m_drawTimeHwState.vertexOffset != drawInfo.firstVertex) || (m_drawTimeHwState.valid.vertexOffset == 0))
        {
            m_drawTimeHwState.vertexOffset       = drawInfo.firstVertex;
            m_drawTimeHwState.valid.vertexOffset = 1;

            pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics, pm4OptImmediate>(GetVertexOffsetRegAddr(),
                                                                                          drawInfo.firstVertex,
                                                                                          pDeCmdSpace);
        }

        // Write the instance offset user data register.
        if ((m_drawTimeHwState.instanceOffset != drawInfo.firstInstance) ||
            (m_drawTimeHwState.valid.instanceOffset == 0))
        {
            m_drawTimeHwState.instanceOffset       = drawInfo.firstInstance;
            m_drawTimeHwState.valid.instanceOffset = 1;

            pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics, pm4OptImmediate>(GetInstanceOffsetRegAddr(),
                                                                                          drawInfo.firstInstance,
                                                                                          pDeCmdSpace);
        }

        // Write the NUM_INSTANCES packet.
        if ((m_drawTimeHwState.numInstances != drawInfo.instanceCount) || (m_drawTimeHwState.valid.numInstances == 0))
        {
            m_drawTimeHwState.numInstances       = drawInfo.instanceCount;
            m_drawTimeHwState.valid.numInstances = 1;

            pDeCmdSpace += m_cmdUtil.BuildNumInstances(drawInfo.instanceCount, pDeCmdSpace);
        }
    }

    if (isNggFastLaunch)
    {
        pDeCmdSpace = ValidateDrawTimeNggFastLaunchState<indexed, indirect, pm4OptImmediate>(drawInfo, pDeCmdSpace);
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
template <bool indexed, bool indirect, bool pm4OptImmediate>
uint32* UniversalCmdBuffer::ValidateDrawTimeNggFastLaunchState(
    const ValidateDrawInfo& drawInfo,
    uint32*                 pDeCmdSpace)
{
    if (indexed)
    {
        {
            if (indirect == false)
            {
                // Write the index buffer address for NGG.
                if ((m_drawTimeHwState.nggIndexBufferBaseAddr != m_graphicsState.iaState.indexAddr) ||
                    (m_drawTimeHwState.valid.nggIndexBufferBaseAddr == 0))
                {
                    // We'll write the index buffer address a little later in this function. For now just update the
                    // current index buffer base address.
                    m_drawTimeHwState.nggIndexBufferBaseAddr       = m_graphicsState.iaState.indexAddr;
                    m_drawTimeHwState.valid.nggIndexBufferBaseAddr = 0;
                }
            }

            // Write the Log2(sizeof(indexType)) user data register.
            if ((m_drawTimeHwState.log2IndexSize != Log2IndexSize[m_vgtDmaIndexType.bits.INDEX_TYPE]) ||
                (m_drawTimeHwState.valid.log2IndexSize == 0))
            {
                // This register should be mapped!
                PAL_ASSERT(m_nggState.log2IndexSizeReg != UserDataNotMapped);

                m_drawTimeHwState.log2IndexSize       = Log2IndexSize[m_vgtDmaIndexType.bits.INDEX_TYPE];
                m_drawTimeHwState.valid.log2IndexSize = 1;

#if PAL_DBG_COMMAND_COMMENTS
                pDeCmdSpace += m_cmdUtil.BuildCommentString("NGG: Log2IndexSize", pDeCmdSpace);
#endif
                pDeCmdSpace =
                    m_deCmdStream.WriteSetOneShReg<ShaderGraphics, pm4OptImmediate>(m_nggState.log2IndexSizeReg,
                                                                                    m_drawTimeHwState.log2IndexSize,
                                                                                    pDeCmdSpace);
            }

        }
    }
    else
    {
        // Non-indexed draws might still need to write the index buffer address, as the shader uses a value of zero
        // to determine whether or not to perform index buffer fetches at runtime.
        if ((m_drawTimeHwState.nggIndexBufferBaseAddr != 0) ||
            (m_drawTimeHwState.valid.nggIndexBufferBaseAddr == 0))
        {
            m_drawTimeHwState.nggIndexBufferBaseAddr       = 0;
            m_drawTimeHwState.valid.nggIndexBufferBaseAddr = 0;
        }
    }

    // Write the index offset user data register.
    if (indirect)
    {
        m_drawTimeHwState.valid.indexOffset = 0;
    }
    else if ((m_drawTimeHwState.startIndex != drawInfo.firstIndex) || (m_drawTimeHwState.valid.indexOffset == 0))
    {
        // This register should be mapped!
        PAL_ASSERT(m_nggState.startIndexReg != UserDataNotMapped);

        m_drawTimeHwState.startIndex        = drawInfo.firstIndex;
        m_drawTimeHwState.valid.indexOffset = 1;
#if PAL_DBG_COMMAND_COMMENTS
        pDeCmdSpace += m_cmdUtil.BuildCommentString("NGG: StartIndex", pDeCmdSpace);
#endif

        pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics, pm4OptImmediate>(m_nggState.startIndexReg,
                                                                                      drawInfo.firstIndex,
                                                                                      pDeCmdSpace);
    }

    // Write the index buffer address to the HW-GS's user-data address lo/hi.
    // These two SGPRs are always loaded by HW-GS's and as such can be repurposed for the index buffer base address.
    // We might need to write this even in auto-index draws as the shader uses a value of zero to determine whether
    // or not to perform index buffer fetches at runtime.
    if (m_drawTimeHwState.valid.nggIndexBufferBaseAddr == 0)
    {
        m_drawTimeHwState.valid.nggIndexBufferBaseAddr = 1;
#if PAL_DBG_COMMAND_COMMENTS
        pDeCmdSpace += m_cmdUtil.BuildCommentString("NGG: IndexBufferBaseAddr", pDeCmdSpace);
#endif

        pDeCmdSpace = m_deCmdStream.WriteSetSeqShRegs(mmSPI_SHADER_USER_DATA_ADDR_LO_GS,
                                                      mmSPI_SHADER_USER_DATA_ADDR_HI_GS,
                                                      ShaderGraphics,
                                                      &m_graphicsState.iaState.indexAddr,
                                                      pDeCmdSpace);
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Performs dispatch-time dirty state validation.
uint32* UniversalCmdBuffer::ValidateDispatch(
    gpusize gpuVirtAddrNumTgs, // GPU virtual address of a buffer containing the number of thread groups to launch in
                               // each dimension (x/y/z)
    uint32* pDeCmdSpace)
{
    // Make sure the contents of all compue user-data tables and the spill-table are up-to-date.
    pDeCmdSpace = (*m_pfnValidateUserDataTablesCs)(this, pDeCmdSpace);

    if (m_pSignatureCs->numWorkGroupsRegAddr != UserDataNotMapped)
    {
        // ... and write the GPU virtual address of the table containing the dispatch dimensions to the appropriate
        // SPI registers.
        pDeCmdSpace = m_deCmdStream.WriteSetSeqShRegs(m_pSignatureCs->numWorkGroupsRegAddr,
                                                      (m_pSignatureCs->numWorkGroupsRegAddr + 1),
                                                      ShaderCompute,
                                                      &gpuVirtAddrNumTgs,
                                                      pDeCmdSpace);
    }

    // If any validation about dumped from CE RAM to GPU memory, we need to add commands to synchronize the draw and
    // constant engines before we issue the upcoming dispatch.
    if (m_state.flags.ceStreamDirty != 0)
    {
        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
        SynchronizeCeDeCounters(&pDeCmdSpace, &pCeCmdSpace);
        m_ceCmdStream.CommitCommands(pCeCmdSpace);
    }

    m_computeState.pipelineState.dirtyFlags.u32All = 0;

    return pDeCmdSpace;
}

// =====================================================================================================================
// Performs dirty-state validation required before executing nested command buffer(s).
void UniversalCmdBuffer::ValidateExecuteNestedCmdBuffers(
    const UniversalCmdBuffer& cmdBuffer)
{
    // Track the most recent OS paging fence value across all nested command buffers called from this one.
    m_lastPagingFence = Max(m_lastPagingFence, cmdBuffer.LastPagingFence());

}

// =====================================================================================================================
// Adds PM4 commands needed to write any registers associated with starting a query
void UniversalCmdBuffer::AddQuery(
    QueryPoolType     queryType, // type of query being added
    QueryControlFlags flags)     // refinements on the query
{
    if (IsFirstQuery(queryType))
    {
        if (queryType == QueryPoolType::Occlusion)
        {
            if (flags.impreciseData == 0)
            {
                m_state.flags.isPrecisionOn = 1;
            }

            // Activate queries on first AddQuery call
            ActivateQueryType(queryType);
        }
        else if (queryType == QueryPoolType::PipelineStats)
        {
            // PIPELINE_START event was issued in the preamble, so no need to do anything here
        }
        else if (queryType == QueryPoolType::StreamoutStats)
        {
            // Nothing needs to do for Streamout stats query
        }
        else
        {
            // What is this?
            PAL_ASSERT_ALWAYS();
        }
    }
    else if ((queryType == QueryPoolType::Occlusion) && (flags.impreciseData == false))
    {
        if (m_state.flags.isPrecisionOn == 0)
        {
            // Dirty query state for draw-time validation
            m_graphicsState.dirtyFlags.validationBits.queryState = 1;
            m_state.flags.isPrecisionOn = 1;
        }
    }
}

// =====================================================================================================================
// Adds PM4 commands needed to write any registers associated with ending the last active query in this command buffer.
void UniversalCmdBuffer::RemoveQuery(
    QueryPoolType queryPoolType) // type of query being removed
{
    if (IsLastActiveQuery(queryPoolType))
    {
        if (queryPoolType == QueryPoolType::Occlusion)
        {
            // Reset the flag now that no queries are activate.
            m_state.flags.isPrecisionOn = 0;

            // Deactivate queries on last RemoveQuery call
            DeactivateQueryType(queryPoolType);
        }
        else if (queryPoolType == QueryPoolType::PipelineStats)
        {
            // We're not bothering with PIPELINE_STOP events, as leaving these counters running doesn't hurt anything
        }
        else if (queryPoolType == QueryPoolType::StreamoutStats)
        {
            // Nothing needs to do for Streamout stats query
        }
        else
        {
            // What is this?
            PAL_ASSERT_ALWAYS();
        }
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdLoadGds(
    HwPipePoint       pipePoint,
    uint32            dstGdsOffset,
    const IGpuMemory& srcGpuMemory,
    gpusize           srcMemOffset,
    uint32            size)
{
    BuildLoadGds(&m_deCmdStream,
                 &m_cmdUtil,
                 pipePoint,
                 dstGdsOffset,
                 srcGpuMemory,
                 srcMemOffset,
                 size);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdStoreGds(
    HwPipePoint       pipePoint,
    uint32            srcGdsOffset,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstMemOffset,
    uint32            size,
    bool              waitForWC)
{
    BuildStoreGds(&m_deCmdStream,
                  &m_cmdUtil,
                  pipePoint,
                  srcGdsOffset,
                  dstGpuMemory,
                  dstMemOffset,
                  size,
                  waitForWC,
                  false,
                  TimestampGpuVirtAddr());
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdUpdateGds(
    HwPipePoint       pipePoint,
    uint32            gdsOffset,
    uint32            dataSize,
    const uint32*     pData)
{
    BuildUpdateGds(&m_deCmdStream,
                   &m_cmdUtil,
                   pipePoint,
                   gdsOffset,
                   dataSize,
                   pData,
                   false);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdFillGds(
    HwPipePoint       pipePoint,
    uint32            gdsOffset,
    uint32            fillSize,
    uint32            data)
{
    BuildFillGds(&m_deCmdStream,
                 &m_cmdUtil,
                 pipePoint,
                 gdsOffset,
                 fillSize,
                 data);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdLoadBufferFilledSizes(
    const gpusize (&gpuVirtAddr)[MaxStreamOutTargets])
{
    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    for (uint32 idx = 0; idx < MaxStreamOutTargets; ++idx)
    {
        if (gpuVirtAddr[idx] != 0)
        {
            pDeCmdSpace +=
                m_cmdUtil.BuildStrmoutBufferUpdate(idx,
                                                   source_select__pfp_strmout_buffer_update__from_src_address,
                                                   0,
                                                   0uLL,
                                                   gpuVirtAddr[idx],
                                                   pDeCmdSpace);
        }
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSaveBufferFilledSizes(
    const gpusize (&gpuVirtAddr)[MaxStreamOutTargets])
{
    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    // The VGT's internal stream output state needs to be flushed before writing the buffer filled size counters
    // to memory.
    pDeCmdSpace = FlushStreamOut(pDeCmdSpace);

    for (uint32 idx = 0; idx < MaxStreamOutTargets; ++idx)
    {
        if (gpuVirtAddr[idx] != 0)
        {
            pDeCmdSpace += m_cmdUtil.BuildStrmoutBufferUpdate(idx,
                                                              source_select__pfp_strmout_buffer_update__none,
                                                              0,
                                                              gpuVirtAddr[idx],
                                                              0uLL,
                                                              pDeCmdSpace);
        }
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBeginQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot,
    QueryControlFlags flags)
{
    static_cast<const QueryPool&>(queryPool).Begin(this, &m_deCmdStream, queryType, slot, flags);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdEndQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot)
{
    static_cast<const QueryPool&>(queryPool).End(this, &m_deCmdStream, queryType, slot);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdResolveQuery(
    const IQueryPool& queryPool,
    QueryResultFlags  flags,
    QueryType         queryType,
    uint32            startQuery,
    uint32            queryCount,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           dstStride)
{
    // Resolving a query is not supposed to honor predication.
    const uint32 packetPredicate = m_gfxCmdBufState.packetPredicate;
    m_gfxCmdBufState.packetPredicate = 0;

    m_device.RsrcProcMgr().CmdResolveQuery(this,
                                           static_cast<const QueryPool&>(queryPool),
                                           flags,
                                           queryType,
                                           startQuery,
                                           queryCount,
                                           static_cast<const GpuMemory&>(dstGpuMemory),
                                           dstOffset,
                                           dstStride);

    m_gfxCmdBufState.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdResetQueryPool(
    const IQueryPool& queryPool,
    uint32            startQuery,
    uint32            queryCount)
{
    static_cast<const QueryPool&>(queryPool).Reset(this, &m_deCmdStream, startQuery, queryCount);
}

// =====================================================================================================================
// Disables the specified query type
void UniversalCmdBuffer::DeactivateQueryType(
    QueryPoolType queryPoolType)
{
    switch (queryPoolType)
    {
    // PIPELINESTAT_STOP works for both pipeline stats and stream out stats
    case QueryPoolType::PipelineStats:
    case QueryPoolType::StreamoutStats:
        {
            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
            pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PIPELINESTAT_STOP, EngineTypeUniversal, pDeCmdSpace);
            m_deCmdStream.CommitCommands(pDeCmdSpace);
        }
        break;

    case QueryPoolType::Occlusion:
        // The value of DB_COUNT_CONTROL depends on both the active occlusion queries and the bound MSAA state
        // object, so we validate it at draw-time.
        m_graphicsState.dirtyFlags.validationBits.queryState = 1;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    // Call base function
    Pal::UniversalCmdBuffer::DeactivateQueryType(queryPoolType);
}

// =====================================================================================================================
// Enables the specified query type.
void UniversalCmdBuffer::ActivateQueryType(
    QueryPoolType queryPoolType)
{
    switch (queryPoolType)
    {
    // PIPELINESTAT_START works for both pipeline stats and stream out stats
    case QueryPoolType::PipelineStats:
    case QueryPoolType::StreamoutStats:
        {
            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
            pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PIPELINESTAT_START, EngineTypeUniversal, pDeCmdSpace);
            m_deCmdStream.CommitCommands(pDeCmdSpace);
        }
        break;

    case QueryPoolType::Occlusion:
        // The value of DB_COUNT_CONTROL depends on both the active occlusion queries and the bound MSAA state
        // object, so we validate it at draw-time.
        m_graphicsState.dirtyFlags.validationBits.queryState = 1;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    // Call base class function
    Pal::UniversalCmdBuffer::ActivateQueryType(queryPoolType);
}

// =====================================================================================================================
// Updates the DB_COUNT_CONTROL register state based on the current the MSAA and occlusion query state.
template <bool pm4OptImmediate>
uint32* UniversalCmdBuffer::UpdateDbCountControl(
    uint32               log2SampleRate,  // MSAA sample rate associated with a bound MSAA state object
    regDB_COUNT_CONTROL* pDbCountControl,
    uint32*              pDeCmdSpace)
{
    if (IsQueryActive(QueryPoolType::Occlusion) && (NumActiveQueries(QueryPoolType::Occlusion) != 0))
    {
        // Only update the value of DB_COUNT_CONTROL if there are active queries. If no queries are active,
        // the new SAMPLE_RATE value is ignored by the HW and the register will be written the next time a query
        // is activated.
        pDbCountControl->bits.SAMPLE_RATE = log2SampleRate;

        //   Since 8xx, the ZPass count controls have moved to a separate register call DB_COUNT_CONTROL.
        //   PERFECT_ZPASS_COUNTS forces all partially covered tiles to be detail walked, and not setting it will count
        //   all HiZ passed tiles as 8x#samples worth of zpasses.  Therefore in order for vis queries to get the right
        //   zpass counts, PERFECT_ZPASS_COUNTS should be set to 1, but this will hurt performance when z passing
        //   geometry does not actually write anything (ZFail Shadow volumes for example).
        pDbCountControl->bits.PERFECT_ZPASS_COUNTS    = m_state.flags.isPrecisionOn;
        pDbCountControl->bits.ZPASS_ENABLE            = 1;
        pDbCountControl->bits.ZPASS_INCREMENT_DISABLE = 0;
    }
    else if (IsNested())
    {
        // Only update DB_COUNT_CONTROL if necessary
        if (pDbCountControl->bits.SAMPLE_RATE != log2SampleRate)
        {
            // MSAA sample rates are associated with the MSAA state object, but the sample rate affects how queries are
            // processed (via DB_COUNT_CONTROL). We need to update the value of this register.
            pDbCountControl->bits.SAMPLE_RATE = log2SampleRate;

            // In a nested command buffer, the number of active queries is unknown because the caller may have some
            // number of active queries when executing the nested command buffer. In this case, the only safe thing
            // to do is to issue a register RMW operation to update the SAMPLE_RATE field of DB_COUNT_CONTROL.
            pDeCmdSpace = m_deCmdStream.WriteContextRegRmw<pm4OptImmediate>(mmDB_COUNT_CONTROL,
                                                                            DB_COUNT_CONTROL__SAMPLE_RATE_MASK,
                                                                            pDbCountControl->u32All,
                                                                            pDeCmdSpace);
            m_deCmdStream.SetContextRollDetected<true>();
        }
    }
    else
    {
        // Disable Z-pass queries
        pDbCountControl->bits.PERFECT_ZPASS_COUNTS    = 0;
        pDbCountControl->bits.ZPASS_ENABLE            = 0;
        pDbCountControl->bits.ZPASS_INCREMENT_DISABLE = 1;
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Returns true if the current command buffer state requires WD_SWITCH_ON_EOP=1, or if a HW workaround necessitates it.
bool UniversalCmdBuffer::ForceWdSwitchOnEop(
    const GraphicsPipeline& pipeline,
    const ValidateDrawInfo& drawInfo
    ) const
{
    // We need switch on EOP if primitive restart is enabled or if our primitive topology cannot be split between IAs.
    // The topologies that meet this requirement are below (currently PAL only supports triangle strip w/ adjacency
    // and triangle fan).
    //    - Polygons (DI_PT_POLYGON)
    //    - Line loop (DI_PT_LINELOOP)
    //    - Triangle fan (DI_PT_TRIFAN)
    //    - Triangle strip w/ adjacency (DI_PT_TRISTRIP_ADJ)
    // The following primitive types support 4x primitive rate with reset index:
    //    - Point list
    //    - Line strip
    //    - Triangle strip

    const PrimitiveTopology primTopology            = m_graphicsState.inputAssemblyState.topology;
    const bool              primitiveRestartEnabled = m_graphicsState.inputAssemblyState.primitiveRestartEnable;

    bool switchOnEop = ((primTopology == PrimitiveTopology::TriangleStripAdj) ||
                        (primTopology == PrimitiveTopology::TriangleFan) ||
                        (primitiveRestartEnabled &&
                         ((primTopology != PrimitiveTopology::PointList) &&
                          (primTopology != PrimitiveTopology::LineStrip) &&
                          (primTopology != PrimitiveTopology::TriangleStrip))));

    // Note the following optimization is not needed on gfx9 and is only here for debug and experimental purpose.
    // TODO: Remove this block of code once we are confident that automatic work distributor load balancing works
    // correctly.
    if ((switchOnEop == false) && m_cachedSettings.disableWdLoadBalancing)
    {
        const uint32 primGroupSize =
            (m_primGroupOpt.optimalSize > 0) ? m_primGroupOpt.optimalSize
                                             : (pipeline.IaMultiVgtParam(false).bits.PRIMGROUP_SIZE + 1);

        PAL_ASSERT(pipeline.VertsPerPrimitive() != 0);
        const uint32 primCount     = drawInfo.vtxIdxCount / pipeline.VertsPerPrimitive();

        const bool   singlePrimGrp = (primCount <= primGroupSize);
        const bool   multiInstance = (drawInfo.instanceCount > 1);
        const bool   isIndirect    = (drawInfo.vtxIdxCount == 0);

        switchOnEop = (isIndirect|| (singlePrimGrp && multiInstance));
    }

    return switchOnEop;
}

// =====================================================================================================================
// Issues commands to synchronize the VGT's internal stream-out state. This requires writing '1' to CP_STRMOUT_CNTL,
// issuing a VGT streamout-flush event, and waiting for the event to complete using WATIREGMEM.
uint32* UniversalCmdBuffer::FlushStreamOut(
    uint32* pDeCmdSpace)
{
    pDeCmdSpace = m_deCmdStream.WriteSetOneConfigReg(mmCP_STRMOUT_CNTL, 0, pDeCmdSpace);

    pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(SO_VGTSTREAMOUT_FLUSH, EngineTypeUniversal, pDeCmdSpace);
    pDeCmdSpace += m_cmdUtil.BuildWaitRegMem(mem_space__pfp_wait_reg_mem__register_space,
                                             function__pfp_wait_reg_mem__equal_to_the_reference_value,
                                             engine_sel__me_wait_reg_mem__micro_engine,
                                             mmCP_STRMOUT_CNTL,
                                             1,
                                             0x00000001,
                                             pDeCmdSpace);

    return pDeCmdSpace;
}

// =====================================================================================================================
// Perform Gfx9-specific functionality regarding pushing of graphics state.
void UniversalCmdBuffer::PushGraphicsState()
{
    Pal::UniversalCmdBuffer::PushGraphicsState();

    if (m_pCurrentExperiment != nullptr)
    {
        // Inform the performance experiment that we're starting some internal operations.
        static_cast<const PerfExperiment*>(m_pCurrentExperiment)->BeginInternalOps(&m_deCmdStream);
    }
}

// =====================================================================================================================
// Restores the last saved to m_graphicsRestoreState, rebinding all objects as necessary.
// TODO: If we determine that it is more performant to only rewrite the non-context registers of a Pipeline, we should
//       add that optimization.
void UniversalCmdBuffer::PopGraphicsState()
{
    Pal::UniversalCmdBuffer::PopGraphicsState();

    if (m_pCurrentExperiment != nullptr)
    {
        // Inform the performance experiment that we've finished some internal operations.
        static_cast<const PerfExperiment*>(m_pCurrentExperiment)->EndInternalOps(&m_deCmdStream);
    }
}

// =====================================================================================================================
// Set all specified state on this command buffer.
void UniversalCmdBuffer::SetGraphicsState(
    const GraphicsState& newGraphicsState)
{
    Pal::UniversalCmdBuffer::SetGraphicsState(newGraphicsState);

    // The target state that we would restore is invalid if this is a nested command buffer that inherits target
    // view state.  The only allowed BLTs in a nested command buffer are CmdClearBoundColorTargets and
    // CmdClearBoundDepthStencilTargets, neither of which will overwrite the bound targets.
    if (m_graphicsState.inheritedState.stateFlags.targetViewState == 0)
    {
        CmdBindTargets(newGraphicsState.bindTargets);
    }

    if ((newGraphicsState.iaState.indexAddr  != m_graphicsState.iaState.indexAddr)  ||
        (newGraphicsState.iaState.indexCount != m_graphicsState.iaState.indexCount) ||
        (newGraphicsState.iaState.indexType  != m_graphicsState.iaState.indexType))
    {
        CmdBindIndexData(newGraphicsState.iaState.indexAddr,
                            newGraphicsState.iaState.indexCount,
                            newGraphicsState.iaState.indexType);
    }

    if (memcmp(&newGraphicsState.inputAssemblyState,
                &m_graphicsState.inputAssemblyState,
                sizeof(m_graphicsState.inputAssemblyState)) != 0)
    {
        CmdSetInputAssemblyState(newGraphicsState.inputAssemblyState);
    }

    if (newGraphicsState.pColorBlendState != m_graphicsState.pColorBlendState)
    {
        CmdBindColorBlendState(newGraphicsState.pColorBlendState);
    }

    if (memcmp(newGraphicsState.blendConstState.blendConst,
                m_graphicsState.blendConstState.blendConst,
                sizeof(m_graphicsState.blendConstState.blendConst)) != 0)
    {
        CmdSetBlendConst(newGraphicsState.blendConstState);
    }

    if (memcmp(&newGraphicsState.stencilRefMaskState,
                &m_graphicsState.stencilRefMaskState,
                sizeof(m_graphicsState.stencilRefMaskState)) != 0)
    {
        // Setting StencilRefMaskState flags to 0xFF so that the faster command is used instead of read-modify-write
        StencilRefMaskParams stencilRefMaskState = newGraphicsState.stencilRefMaskState;
        stencilRefMaskState.flags.u8All = 0xFF;

        CmdSetStencilRefMasks(stencilRefMaskState);
    }

    if (newGraphicsState.pDepthStencilState != m_graphicsState.pDepthStencilState)
    {
        CmdBindDepthStencilState(newGraphicsState.pDepthStencilState);
    }

    if ((newGraphicsState.depthBoundsState.min != m_graphicsState.depthBoundsState.min) ||
        (newGraphicsState.depthBoundsState.max != m_graphicsState.depthBoundsState.max))
    {
        CmdSetDepthBounds(newGraphicsState.depthBoundsState);
    }

    if (newGraphicsState.pMsaaState != m_graphicsState.pMsaaState)
    {
        CmdBindMsaaState(newGraphicsState.pMsaaState);
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 339
    if (memcmp(&newGraphicsState.samplePatternState,
        &m_graphicsState.samplePatternState,
        sizeof(SamplePattern)) != 0)
    {
        // numSamplesPerPixel can be 0 if the client never called CmdSetMsaaQuadSamplePattern.
        if (newGraphicsState.numSamplesPerPixel != 0)
        {
            if (newGraphicsState.samplePatternState.isLoad)
            {
                PAL_ASSERT(newGraphicsState.samplePatternState.pGpuMemory != nullptr);
                CmdLoadMsaaQuadSamplePattern(newGraphicsState.samplePatternState.pGpuMemory,
                    newGraphicsState.samplePatternState.memOffset);
            }
            else
            {
                CmdSetMsaaQuadSamplePattern(newGraphicsState.numSamplesPerPixel,
                    newGraphicsState.samplePatternState.immediate);
            }
        }
    }
#else
    if (memcmp(&newGraphicsState.quadSamplePatternState,
               &m_graphicsState.quadSamplePatternState,
               sizeof(MsaaQuadSamplePattern)) != 0)
    {
        // numSamplesPerPixel can be 0 if the client never called CmdSetMsaaQuadSamplePattern.
        if (newGraphicsState.numSamplesPerPixel != 0)
        {
            CmdSetMsaaQuadSamplePattern(newGraphicsState.numSamplesPerPixel,
                newGraphicsState.quadSamplePatternState);
        }
    }
#endif

    if (memcmp(&newGraphicsState.triangleRasterState,
                &m_graphicsState.triangleRasterState,
                sizeof(m_graphicsState.triangleRasterState)) != 0)
    {
        CmdSetTriangleRasterState(newGraphicsState.triangleRasterState);
    }

    if (memcmp(&newGraphicsState.pointLineRasterState,
               &m_graphicsState.pointLineRasterState,
               sizeof(m_graphicsState.pointLineRasterState)) != 0)
    {
        CmdSetPointLineRasterState(newGraphicsState.pointLineRasterState);
    }

    const auto& restoreDepthBiasState = newGraphicsState.depthBiasState;

    if ((restoreDepthBiasState.depthBias            != m_graphicsState.depthBiasState.depthBias)      ||
        (restoreDepthBiasState.depthBiasClamp       != m_graphicsState.depthBiasState.depthBiasClamp) ||
        (restoreDepthBiasState.slopeScaledDepthBias != m_graphicsState.depthBiasState.slopeScaledDepthBias))
    {
        CmdSetDepthBiasState(newGraphicsState.depthBiasState);
    }

    const auto& restoreViewports = newGraphicsState.viewportState;
    const auto& currentViewports = m_graphicsState.viewportState;

    if ((restoreViewports.count != currentViewports.count) ||
        (memcmp(&restoreViewports.viewports[0],
                &currentViewports.viewports[0],
                restoreViewports.count * sizeof(restoreViewports.viewports[0])) != 0))
    {
        CmdSetViewports(restoreViewports);
    }

    const auto& restoreScissorRects = newGraphicsState.scissorRectState;
    const auto& currentScissorRects = m_graphicsState.scissorRectState;

    if ((restoreScissorRects.count != currentScissorRects.count) ||
        (memcmp(&restoreScissorRects.scissors[0],
                &currentScissorRects.scissors[0],
                restoreScissorRects.count * sizeof(restoreScissorRects.scissors[0])) != 0))
    {
        CmdSetScissorRects(restoreScissorRects);
    }

    const auto& restoreGlobalScissor = newGraphicsState.globalScissorState.scissorRegion;
    const auto& currentGlobalScissor = m_graphicsState.globalScissorState.scissorRegion;

    if ((restoreGlobalScissor.offset.x      != currentGlobalScissor.offset.x)     ||
        (restoreGlobalScissor.offset.y      != currentGlobalScissor.offset.y)     ||
        (restoreGlobalScissor.extent.width  != currentGlobalScissor.extent.width) ||
        (restoreGlobalScissor.extent.height != currentGlobalScissor.extent.height))
    {
        CmdSetGlobalScissor(newGraphicsState.globalScissorState);
    }
}

// =====================================================================================================================
// Bind the last state set on the specified command buffer
void UniversalCmdBuffer::InheritStateFromCmdBuf(
    const GfxCmdBuffer* pCmdBuffer)
{
    const UniversalCmdBuffer* pUniversalCmdBuffer = static_cast<const UniversalCmdBuffer*>(pCmdBuffer);
    SetGraphicsState(pUniversalCmdBuffer->GetGraphicsState());
    SetComputeState(pCmdBuffer->GetComputeState(), ComputeStateAll);

    for (uint16 i = 0; i < MaxIndirectUserDataTables; i++)
    {
        const uint32  numEntries = pUniversalCmdBuffer->m_indirectUserDataInfo[i].watermark;
        const uint32* pData      = pUniversalCmdBuffer->m_indirectUserDataInfo[i].pData;
        if (numEntries > 0)
        {
            CmdSetIndirectUserData(i, 0, numEntries, pData);
        }
    }
}

// =====================================================================================================================
// Perform Gfx9-specific functionality regarding saving of compute state.
void UniversalCmdBuffer::CmdSaveComputeState(
    uint32 stateFlags)
{
    GfxCmdBuffer::CmdSaveComputeState(stateFlags);

    if (m_pCurrentExperiment != nullptr)
    {
        // Inform the performance experiment that we're starting some internal operations.
        static_cast<const PerfExperiment*>(m_pCurrentExperiment)->BeginInternalOps(&m_deCmdStream);
    }
}

// =====================================================================================================================
// Perform Gfx9-specific functionality regarding restoration of compute state.
void UniversalCmdBuffer::CmdRestoreComputeState(
    uint32 stateFlags)
{
    GfxCmdBuffer::CmdRestoreComputeState(stateFlags);

    if (m_pCurrentExperiment != nullptr)
    {
        // Inform the performance experiment that we've finished some internal operations.
        static_cast<const PerfExperiment*>(m_pCurrentExperiment)->EndInternalOps(&m_deCmdStream);
    }
}

// =====================================================================================================================
// Creates a CE command to load data from the specified memory object into the CE RAM offset provided.
void UniversalCmdBuffer::CmdLoadCeRam(
    const IGpuMemory& srcGpuMemory,
    gpusize           memOffset,        // GPU memory offset, must be 32-byte aligned
    uint32            ramOffset,        // CE RAM offset, must be 32-byte aligned
    uint32            dwordSize)        // Number of DWORDs to load, must be a multiple of 8
{
    uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
    pCeCmdSpace += m_cmdUtil.BuildLoadConstRam(srcGpuMemory.Desc().gpuVirtAddr + memOffset,
                                               (ReservedCeRamBytes + ramOffset),
                                               dwordSize,
                                               pCeCmdSpace);
    m_ceCmdStream.CommitCommands(pCeCmdSpace);
}

// =====================================================================================================================
// Creates a CE command to dump data from the specified CE RAM offset to the provided memory object.
void UniversalCmdBuffer::CmdDumpCeRam(
    const IGpuMemory& dstGpuMemory,
    gpusize           memOffset,        // GPU memory offset, must be 4-byte aligned
    uint32            ramOffset,        // CE RAM offset, must be 4-byte aligned
    uint32            dwordSize,
    uint32            currRingPos,
    uint32            ringSize)
{
    uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
    HandleCeRinging(&m_state, currRingPos, 1, ringSize);

    if (m_state.flags.ceWaitOnDeCounterDiff)
    {
        pCeCmdSpace += m_cmdUtil.BuildWaitOnDeCounterDiff(m_state.minCounterDiff, pCeCmdSpace);
        m_state.flags.ceWaitOnDeCounterDiff = 0;
    }

    pCeCmdSpace += m_cmdUtil.BuildDumpConstRam(dstGpuMemory.Desc().gpuVirtAddr + memOffset,
                                               (ReservedCeRamBytes + ramOffset),
                                               dwordSize,
                                               pCeCmdSpace);
    m_ceCmdStream.CommitCommands(pCeCmdSpace);

    m_state.flags.ceStreamDirty = 1;
}

// =====================================================================================================================
// Creates a CE command to write data from the specified CPU memory location into the CE RAM offset provided.
void UniversalCmdBuffer::CmdWriteCeRam(
    const void* pSrcData,
    uint32      ramOffset,      // CE RAM byte offset, must be 4-byte aligned
    uint32      dwordSize)      // Number of DWORDs to write from pSrcData
{
    uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
    pCeCmdSpace += m_cmdUtil.BuildWriteConstRam(pSrcData, (ReservedCeRamBytes + ramOffset), dwordSize, pCeCmdSpace);
    m_ceCmdStream.CommitCommands(pCeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdIf(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    // CE and nested command buffers don't support control flow yet.
    PAL_ASSERT(m_ceCmdStream.IsEmpty() && (IsNested() == false));

    m_deCmdStream.If(compareFunc, gpuMemory.Desc().gpuVirtAddr + offset, data, mask);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdElse()
{
    // CE and nested command buffers don't support control flow yet.
    PAL_ASSERT(m_ceCmdStream.IsEmpty() && (IsNested() == false));

    m_deCmdStream.Else();
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdEndIf()
{
    // CE and nested command buffers don't support control flow yet.
    PAL_ASSERT(m_ceCmdStream.IsEmpty() && (IsNested() == false));

    m_deCmdStream.EndIf();
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdWhile(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    // CE and nested command buffers don't support control flow yet.
    PAL_ASSERT(m_ceCmdStream.IsEmpty() && (IsNested() == false));

    m_deCmdStream.While(compareFunc, gpuMemory.Desc().gpuVirtAddr + offset, data, mask);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdEndWhile()
{
    // CE and nested command buffers don't support control flow yet.
    PAL_ASSERT(m_ceCmdStream.IsEmpty() && (IsNested() == false));

    m_deCmdStream.EndWhile();
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdFlglEnable()
{
    SendFlglSyncCommands(FlglRegSeqSwapreadyReset);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdFlglDisable()
{
    SendFlglSyncCommands(FlglRegSeqSwapreadySet);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdFlglSync()
{
    // make sure (wait that) the swap req line is low
    SendFlglSyncCommands(FlglRegSeqSwaprequestReadLow);
    // pull the swap grant line low as we are done rendering
    SendFlglSyncCommands(FlglRegSeqSwapreadySet);
    // wait for rising edge of SWAPREQUEST (or timeout)
    SendFlglSyncCommands(FlglRegSeqSwaprequestRead);
    // pull the swap grant line high marking the beginning of the next frame
    SendFlglSyncCommands(FlglRegSeqSwapreadyReset);
}

// =====================================================================================================================
void UniversalCmdBuffer::SendFlglSyncCommands(
    FlglRegSeqType syncSequence)
{
    PAL_ASSERT((syncSequence >= 0) && (syncSequence < FlglRegSeqMax));

    const FlglRegSeq* pSeq        = m_device.GetFlglRegisterSequence(syncSequence);
    const uint32      totalNumber = pSeq->regSequenceCount;

    // if there's no GLsync board, num should be 0
    if (totalNumber > 0)
    {
        const bool isReadSequence = (syncSequence == FlglRegSeqSwapreadyRead) ||
                                    (syncSequence == FlglRegSeqSwaprequestRead) ||
                                    (syncSequence == FlglRegSeqSwaprequestReadLow);

        const FlglRegCmd* seq = pSeq->regSequence;

        uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

        for (uint32 i = 0; i < totalNumber; i++)
        {
            // all sequence steps are write operations apart from the last
            // step of the SWAPREADY_READ or SWAPREQUEST_READ sequences
            if ((i == totalNumber - 1) && isReadSequence)
            {
                pCmdSpace += m_device.CmdUtil().BuildWaitRegMem(mem_space__me_wait_reg_mem__register_space,
                                                                CmdUtil::WaitRegMemFunc(CompareFunc::Equal),
                                                                engine_sel__me_wait_reg_mem__micro_engine,
                                                                seq[i].offset,
                                                                seq[i].orMask ? seq[i].andMask : 0,
                                                                seq[i].andMask,
                                                                pCmdSpace);
            }
            else
            {
                pCmdSpace += m_device.CmdUtil().BuildRegRmw(seq[i].offset, seq[i].orMask, seq[i].andMask, pCmdSpace);
                pCmdSpace += m_device.CmdUtil().BuildRegRmw(seq[i].offset, seq[i].orMask, seq[i].andMask, pCmdSpace);
                pCmdSpace += m_device.CmdUtil().BuildRegRmw(seq[i].offset, seq[i].orMask, seq[i].andMask, pCmdSpace);
            }
        }
        m_deCmdStream.CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdWaitRegisterValue(
    uint32      registerOffset,
    uint32      data,
    uint32      mask,
    CompareFunc compareFunc)
{
    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

    pCmdSpace += m_cmdUtil.BuildWaitRegMem(mem_space__me_wait_reg_mem__register_space,
                                           CmdUtil::WaitRegMemFunc(compareFunc),
                                           engine_sel__me_wait_reg_mem__micro_engine,
                                           registerOffset,
                                           data,
                                           mask,
                                           pCmdSpace);

    m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdWaitMemoryValue(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

    pCmdSpace += m_cmdUtil.BuildWaitRegMem(mem_space__me_wait_reg_mem__memory_space,
                                           CmdUtil::WaitRegMemFunc(compareFunc),
                                           engine_sel__me_wait_reg_mem__micro_engine,
                                           gpuMemory.Desc().gpuVirtAddr + offset,
                                           data,
                                           mask,
                                           pCmdSpace);

    m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdWaitBusAddressableMemoryMarker(
    const IGpuMemory& gpuMemory,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    const GpuMemory* pGpuMemory = static_cast<const GpuMemory*>(&gpuMemory);
    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

    pCmdSpace += m_cmdUtil.BuildWaitRegMem(mem_space__me_wait_reg_mem__memory_space,
                                           CmdUtil::WaitRegMemFunc(compareFunc),
                                           engine_sel__me_wait_reg_mem__micro_engine,
                                           pGpuMemory->GetBusAddrMarkerVa(),
                                           data,
                                           mask,
                                           pCmdSpace);

    m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Enables or disables a flexible predication check which the CP uses to determine if a draw or dispatch can be skipped
// based on the results of prior GPU work.
// SEE: CmdUtil::BuildSetPredication(...) for more details on the meaning of this method's parameters.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 311
void UniversalCmdBuffer::CmdSetPredication(
    IQueryPool*         pQueryPool,
    uint32              slot,
    const IGpuMemory*   pGpuMemory,
    gpusize             offset,
    PredicateType       predType,
    bool                predPolarity,
    bool                waitResults,
    bool                accumulateData)
{
    PAL_ASSERT((pQueryPool == nullptr) || (pGpuMemory == nullptr));

    m_gfxCmdBufState.clientPredicate = ((pQueryPool != nullptr) || (pGpuMemory != nullptr)) ? 1 : 0;
    m_gfxCmdBufState.packetPredicate = m_gfxCmdBufState.clientPredicate;

    gpusize gpuVirtAddr = 0;
    if (pGpuMemory != nullptr)
    {
        gpuVirtAddr = pGpuMemory->Desc().gpuVirtAddr + offset;
    }

    if (pQueryPool != nullptr)
    {
        Result result = static_cast<QueryPool*>(pQueryPool)->GetQueryGpuAddress(static_cast<uint32>(slot), &gpuVirtAddr);
        PAL_ASSERT(result == Result::Success);
    }

    // Clear/disable predicate
    if ((pQueryPool == nullptr) && (gpuVirtAddr == 0))
    {
        predType = static_cast<PredicateType>(0);
    }

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    pDeCmdSpace += m_cmdUtil.BuildSetPredication(gpuVirtAddr,
                                                 predPolarity,
                                                 waitResults,
                                                 predType,
                                                 accumulateData,
                                                 pDeCmdSpace);

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}
#else
void UniversalCmdBuffer::CmdSetPredication(
    IQueryPool*   pQueryPool,
    uint32        slot,
    gpusize       gpuVirtAddr,
    PredicateType predType,
    bool          predPolarity,
    bool          waitResults,
    bool          accumulateData)
{
    PAL_ASSERT((pQueryPool == nullptr) || (gpuVirtAddr == 0));

    m_gfxCmdBufState.clientPredicate = ((pQueryPool != nullptr) || (gpuVirtAddr != 0)) ? 1 : 0;
    m_gfxCmdBufState.packetPredicate = m_gfxCmdBufState.clientPredicate;

    if (pQueryPool != nullptr)
    {
        static_cast<QueryPool*>(pQueryPool)->GetQueryGpuAddress(static_cast<uint32>(slot), &gpuVirtAddr);
    }

    // Clear/disable predicate
    if ((pQueryPool == nullptr) && (gpuVirtAddr == 0))
    {
        predType = static_cast<PredicateType>(0);
    }

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    pDeCmdSpace += m_cmdUtil.BuildSetPredication(gpuVirtAddr,
                                                 predPolarity,
                                                 waitResults,
                                                 predType,
                                                 accumulateData,
                                                 pDeCmdSpace);

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}
#endif

// =====================================================================================================================
void UniversalCmdBuffer::CmdCopyRegisterToMemory(
    uint32            srcRegisterOffset,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

    DmaDataInfo dmaData = {};
    dmaData.dstSel       = dst_sel__pfp_dma_data__dst_addr_using_das;
    dmaData.dstAddr      = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;
    dmaData.dstAddrSpace = das__pfp_dma_data__memory;
    dmaData.srcSel       = src_sel__pfp_dma_data__src_addr_using_sas;
    dmaData.srcAddr      = srcRegisterOffset;
    dmaData.srcAddrSpace = sas__pfp_dma_data__register;
    dmaData.sync         = true;
    dmaData.usePfp       = false;
    pCmdSpace += m_cmdUtil.BuildDmaData(dmaData, pCmdSpace);

    m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdExecuteIndirectCmds(
    const IIndirectCmdGenerator& generator,
    const IGpuMemory&            gpuMemory,
    gpusize                      offset,
    uint32                       maximumCount,
    gpusize                      countGpuAddr)
{
    // It is only safe to generate indirect commands on a one-time-submit or exclusive-submit command buffer because
    // there is a potential race condition on the memory used to receive the generated commands.
    PAL_ASSERT(IsOneTimeSubmit() || IsExclusiveSubmit());

    const auto& gfx9Generator = static_cast<const IndirectCmdGenerator&>(generator);

    if (countGpuAddr == 0uLL)
    {
        // If the count GPU address is zero, then we are expected to use the maximumCount value as the actual number
        // of indirect commands to generate and execute.
        uint32* pMemory = CmdAllocateEmbeddedData(1, 1, &countGpuAddr);
        *pMemory = maximumCount;
    }

    // The generation of indirect commands is determined by the currently-bound pipeline.
    const PipelineBindPoint bindPoint = ((gfx9Generator.Type() == GeneratorType::Dispatch)
                                        ? PipelineBindPoint::Compute : PipelineBindPoint::Graphics);
    const bool              setViewId    = (bindPoint == PipelineBindPoint::Graphics);
    const auto*const        pGfxPipeline =
        static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    uint32                  mask         = 1;

    if ((bindPoint == PipelineBindPoint::Graphics) &&
        (pGfxPipeline->HwStereoRenderingEnabled() == false))
    {
        const auto& viewInstancingDesc = pGfxPipeline->GetViewInstancingDesc();

        mask = (1 << viewInstancingDesc.viewInstanceCount) - 1;

        if (viewInstancingDesc.enableMasking)
        {
            mask &= m_graphicsState.viewInstanceMask;
        }
    }

    // There is a lot of logic necessary to support NGG pipelines with indirect command generation that would cause
    // indirect command generation to suffer more of a performance hit.
    PAL_ASSERT(static_cast<const GraphicsPipeline*>(PipelineState(bindPoint)->pPipeline)->IsNggFastLaunch() == false);

    for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
    {
        if (TestAnyFlagSet(mask, 1) == false)
        {
            continue;
        }

        // NOTE: Save an iterator to the current end of the generated-chunk list. Each command buffer chunk generated by
        // the call to RPM below will be added to the end of the list, so we can iterate over the new chunks starting
        // from the first item in the list following this iterator.
        auto chunkIter = m_generatedChunkList.End();

        // Generate the indirect command buffer chunk(s) using RPM. Since we're wrapping the command generation and
        // execution inside a CmdIf, we want to disable normal predication for this blit.
        const uint32 packetPredicate = PacketPredicate();
        m_gfxCmdBufState.packetPredicate = 0;

        m_device.RsrcProcMgr().CmdGenerateIndirectCmds(this,
                                                       PipelineState(bindPoint)->pPipeline,
                                                       gfx9Generator,
                                                       (gpuMemory.Desc().gpuVirtAddr + offset),
                                                       countGpuAddr,
                                                       m_graphicsState.iaState.indexCount,
                                                       maximumCount);

        m_gfxCmdBufState.packetPredicate = packetPredicate;

        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

        // Insert a CS_PARTIAL_FLUSH and invalidate/flush the texture caches to make sure that the generated commands
        // are written out to memory before we attempt to execute them. Then, a PFP_SYNC_ME is also required so that
        // the PFP doesn't prefetch the generated commands before they are finished executing.
        AcquireMemInfo acquireInfo = {};
        acquireInfo.flags.invSqK$ = 1;
        acquireInfo.tcCacheOp     = TcCacheOp::WbInvL1L2;
        acquireInfo.engineType    = EngineTypeUniversal;
        acquireInfo.baseAddress   = FullSyncBaseAddr;
        acquireInfo.sizeBytes     = FullSyncSize;

        pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, EngineTypeUniversal, pDeCmdSpace);
        pDeCmdSpace += m_cmdUtil.BuildAcquireMem(acquireInfo, pDeCmdSpace);
        pDeCmdSpace += m_cmdUtil.BuildPfpSyncMe(pDeCmdSpace);
        m_deCmdStream.SetContextRollDetected<false>();

        // Just like a normal direct/indirect draw/dispatch, we need to perform state validation before executing the
        // generated command chunks.
        if (bindPoint == PipelineBindPoint::Graphics)
        {
            // NOTE: If we tell ValidateDraw() that this draw call is indexed, it will validate all of the draw-time
            // HW state related to the index buffer. However, since some indirect command generators can genrate the
            // commands to bind their own index buffer state, our draw-time validation could be redundant. Therefore,
            // pretend this is a non-indexed draw call if the generated command binds its own index buffer(s).
            ValidateDrawInfo drawInfo;
            drawInfo.vtxIdxCount   = 0;
            drawInfo.instanceCount = 0;
            drawInfo.firstVertex   = 0;
            drawInfo.firstInstance = 0;
            drawInfo.firstIndex    = 0;
            if (gfx9Generator.ContainsIndexBufferBind() || (gfx9Generator.Type() == GeneratorType::Draw))
            {
                pDeCmdSpace = ValidateDraw<false, true>(drawInfo, pDeCmdSpace);
            }
            else
            {
                pDeCmdSpace = ValidateDraw<true, true>(drawInfo, pDeCmdSpace);
            }

            CommandGeneratorTouchedUserData(m_graphicsState.gfxUserDataEntries.touched,
                                            gfx9Generator,
                                            *m_pSignatureGfx);
        }
        else
        {
            pDeCmdSpace = ValidateDispatch(0uLL, pDeCmdSpace);

            CommandGeneratorTouchedUserData(m_computeState.csUserDataEntries.touched, gfx9Generator, *m_pSignatureCs);
        }

        if (setViewId)
        {
            const auto& viewInstancingDesc = pGfxPipeline->GetViewInstancingDesc();

            pDeCmdSpace = BuildWriteViewId(viewInstancingDesc.viewId[i], pDeCmdSpace);
        }

        m_deCmdStream.CommitCommands(pDeCmdSpace);

        // NOTE: The command stream expects an iterator to the first chunk to execute, but this iterator points to the
        // place in the list before the first generated chunk (see comments above).
        chunkIter.Next();
        m_deCmdStream.ExecuteGeneratedCommands(chunkIter);

        pDeCmdSpace = m_deCmdStream.ReserveCommands();

        // We need to issue any post-draw or post-dispatch workarounds after all of the generated command buffers have
        // finished.
        if (bindPoint == PipelineBindPoint::Graphics)
        {
            if (gfx9Generator.Type() == GeneratorType::Draw)
            {
                // Command generators which issue non-indexed draws generate DRAW_INDEX_AUTO packets, which will
                // invalidate some of our draw-time HW state. SEE: CmdDraw() for more details.
                m_drawTimeHwState.dirty.indexedIndexType = 1;
            }
        }

        pDeCmdSpace = IncrementDeCounter(pDeCmdSpace);
        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }

}

// =====================================================================================================================
void UniversalCmdBuffer::CmdCommentString(
    const char* pComment)
{
    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    pDeCmdSpace += m_cmdUtil.BuildCommentString(pComment, pDeCmdSpace);

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
CmdStreamChunk* UniversalCmdBuffer::GetChunkForCmdGeneration(
    const Pal::IndirectCmdGenerator& generator,
    const Pal::Pipeline&             pipeline,
    uint32                           maxCommands,
    uint32*                          pCommandsInChunk, // [out] How many commands can safely fit into the command chunk
    gpusize*                         pEmbeddedDataAddr,
    uint32*                          pEmbeddedDataSize)
{
    const auto& properties = generator.Properties();

    PAL_ASSERT(m_pCmdAllocator != nullptr);

    CmdStreamChunk*const pChunk = Pal::GfxCmdBuffer::GetNextGeneratedChunk();

    const uint32* pUserDataEntries                             = nullptr;
    uint16        indirectTableAddr[MaxIndirectUserDataTables] = { };
    uint32        spillThreshold                               = NoUserDataSpilling;

    if (generator.Type() == GeneratorType::Dispatch)
    {
        const auto& signature = static_cast<const ComputePipeline&>(pipeline).Signature();
        memcpy(&indirectTableAddr[0], &signature.indirectTableAddr[0], sizeof(indirectTableAddr));
        spillThreshold = signature.spillThreshold;

        // NOTE: RPM uses a compute shader to generate indirect commands, so we need to use the saved user-data
        // state because RPM will have pushed its own state before calling this method.
        pUserDataEntries = &m_computeRestoreState.csUserDataEntries.entries[0];
    }
    else
    {
        const auto& signature = static_cast<const GraphicsPipeline&>(pipeline).Signature();
        memcpy(&indirectTableAddr[0], &signature.indirectTableAddr[0], sizeof(indirectTableAddr));
        spillThreshold = signature.spillThreshold;

        // NOTE: RPM uses a compute shader to generate indirect commands, which doesn't interfere with the graphics
        // state, so we don't need to look at the pushed state.
        pUserDataEntries = &m_graphicsState.gfxUserDataEntries.entries[0];
    }

    // Total amount of embedded data space needed for each generated command, including indirect user-data tables and
    // user-data spilling.
    uint32 embeddedDwords = 0;
    // Amount of embedded data space needed for each generated command, per indirect user-data table:
    uint32 indirectTableDwords[MaxIndirectUserDataTables] = { };
    // User-data high watermark for this command Generator. It depends on the command Generator itself, as well as the
    // pipeline signature for the active pipeline. This is due to the fact that if the command Generator modifies the
    // contents of an indirect user-data table, the command Generator must also fix-up the user-data entry used for the
    // table's GPU virtual address.
    uint32 userDataWatermark = properties.userDataWatermark;

    for (uint32 id = 0; id < MaxIndirectUserDataTables; ++id)
    {
        if ((indirectTableAddr[id] != 0) &&
            (properties.indirectUserDataThreshold[id] < m_device.Parent()->IndirectUserDataTableSize(id)))
        {
            userDataWatermark       = Max<uint32>(userDataWatermark, (indirectTableAddr[id] - 1));
            indirectTableDwords[id] = static_cast<uint32>(m_device.Parent()->IndirectUserDataTableSize(id));
            embeddedDwords         += indirectTableDwords[id];
        }
    }

    const uint32 commandDwords = (properties.cmdBufStride / sizeof(uint32));
    // There are three possibilities when determining how much spill-table space a generated command will need:
    //  (1) The active pipeline doesn't spill at all. This requires no spill-table space.
    //  (2) The active pipeline spills, but the generator doesn't update the any user-data entries beyond the
    //      spill threshold. This requires no spill-table space.
    //  (3) The active pipeline spills, and the generator updates user-data entries which are beyond the spill
    //      threshold. This means each generated command needs to relocate the spill table in addition to the other
    //      stuff it would normally do.
    const uint32 spillDwords = (spillThreshold <= userDataWatermark) ? properties.maxUserDataEntries : 0;
    embeddedDwords          += spillDwords;

    // Ask the DE command stream to make sure the command chunk is ready to receive GPU-generated commands (this
    // includes setting up padding for size alignment, allocating command space, etc.
    (*pCommandsInChunk) =
        m_deCmdStream.PrepareChunkForCmdGeneration(pChunk, commandDwords, embeddedDwords, maxCommands);
    (*pEmbeddedDataSize) = ((*pCommandsInChunk) * embeddedDwords);

    if (embeddedDwords > 0)
    {
        // If each generated command requires some amount of spill-table space, then we need to allocate embeded data
        // space for all of the generated commands which will go into this chunk. PrepareChunkForCmdGeneration() should
        // have determined a value for commandsInChunk which allows us to allocate the appropriate amount of embeded
        // data space.
        uint32* pDataSpace = pChunk->ValidateCmdGenerationDataSpace((*pEmbeddedDataSize), pEmbeddedDataAddr);

        // We also need to seed the embedded data for each generated command with the current indirect user-data table
        // and spill-table contents, because the generator will only update the table entries which get modified.
        for (uint32 cmd = 0; cmd < (*pCommandsInChunk); ++cmd)
        {
            for (uint32 id = 0; id < MaxIndirectUserDataTables; ++id)
            {
                memcpy(pDataSpace,
                       m_indirectUserDataInfo[id].pData,
                       (sizeof(uint32) * m_indirectUserDataInfo[id].watermark));
                pDataSpace += indirectTableDwords[id];
            }
            memcpy(pDataSpace, pUserDataEntries, (sizeof(uint32) * spillDwords));
            pDataSpace += spillDwords;
        }
    }

    return pChunk;
}

// =====================================================================================================================
// Helper method for handling the state "leakage" from a nested command buffer back to its caller. Since the callee has
// tracked its own state during the building phase, we can access the final state of the command buffer since its stored
// in the UniversalCmdBuffer object itself.
void UniversalCmdBuffer::LeakNestedCmdBufferState(
    const UniversalCmdBuffer& cmdBuffer)
{
    Pal::UniversalCmdBuffer::LeakNestedCmdBufferState(cmdBuffer);

    if (cmdBuffer.m_computeState.pipelineState.pPipeline != nullptr)
    {
        m_pSignatureCs = cmdBuffer.m_pSignatureCs;
    }

    if (cmdBuffer.m_graphicsState.pipelineState.pPipeline != nullptr)
    {
        m_pSignatureGfx             = cmdBuffer.m_pSignatureGfx;
        m_vertexOffsetReg           = cmdBuffer.m_vertexOffsetReg;
        m_drawIndexReg              = cmdBuffer.m_drawIndexReg;
        m_nggState.startIndexReg    = cmdBuffer.m_nggState.startIndexReg;
        m_nggState.log2IndexSizeReg = cmdBuffer.m_nggState.log2IndexSizeReg;

        const GraphicsPipeline* pPipeline =
            static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

        if (pPipeline->IsNgg())
        {
            pPipeline->UpdateNggPrimCb(&m_state.primShaderCbLayout.pipelineStateCb);
        }

        // Update the functions that are modified by nested command list
        SwitchCmdSetUserDataFunc(
            PipelineBindPoint::Graphics,
            cmdBuffer.m_funcTable.pfnCmdSetUserData[static_cast<uint32>(PipelineBindPoint::Graphics)]);
        m_funcTable.pfnCmdDraw                     = cmdBuffer.m_funcTable.pfnCmdDraw;
        m_funcTable.pfnCmdDrawIndexed              = cmdBuffer.m_funcTable.pfnCmdDrawIndexed;
        m_funcTable.pfnCmdDrawIndirectMulti        = cmdBuffer.m_funcTable.pfnCmdDrawIndirectMulti;
        m_funcTable.pfnCmdDrawIndexedIndirectMulti = cmdBuffer.m_funcTable.pfnCmdDrawIndexedIndirectMulti;
    }

    if (cmdBuffer.HasStreamOutBeenSet())
    {
        // If the nested command buffer set their own stream-out targets, we can simply copy the SRD's because CE
        // RAM is up-to-date.
        memcpy(&m_streamOut.srd[0], &cmdBuffer.m_streamOut.srd[0], sizeof(m_streamOut.srd));
    }
    else if (m_graphicsState.pipelineState.pPipeline != nullptr)
    {
        // Otherwise we need to rebind the stream-out strides based on the *new* current pipeline.
        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
        pCeCmdSpace = UploadStreamOutBufferStridesToCeRam(
                        static_cast<const GraphicsPipeline&>(*m_graphicsState.pipelineState.pPipeline),
                        pCeCmdSpace);
        m_ceCmdStream.CommitCommands(pCeCmdSpace);
    }

    m_drawTimeHwState.valid.u32All = 0;

    for (uint32 id = 0; id < MaxIndirectUserDataTables; ++id)
    {
        m_indirectUserDataInfo[id].state.contentsDirty |= cmdBuffer.m_indirectUserDataInfo[id].modified;
    }

    m_spillTable.stateCs.contentsDirty  |= cmdBuffer.m_spillTable.stateCs.contentsDirty;
    m_spillTable.stateGfx.contentsDirty |= cmdBuffer.m_spillTable.stateGfx.contentsDirty;

    m_spiPsInControl = cmdBuffer.m_spiPsInControl;
    m_spiVsOutConfig = cmdBuffer.m_spiVsOutConfig;

    m_nggState.flags.state.hasPrimShaderWorkload |= cmdBuffer.m_nggState.flags.state.hasPrimShaderWorkload;
    m_nggState.flags.dirty.u8All                 |= cmdBuffer.m_nggState.flags.dirty.u8All;

    // Invalidate PM4 optimizer state on post-execute since the current command buffer state does not reflect
    // state changes from the nested command buffer. We will need to resolve the nested PM4 state onto the
    // current command buffer for this to work correctly.
    m_deCmdStream.NotifyNestedCmdBufferExecute();
}

// =====================================================================================================================
// Helper method to upload the stream-output buffer strides into the CE RAM copy of the stream-out buffer SRD table.
uint32* UniversalCmdBuffer::UploadStreamOutBufferStridesToCeRam(
    const GraphicsPipeline& pipeline,
    uint32*                 pCeCmdSpace)
{
    // Start at word1 of the 0th SRD...
    uint32 ceRamOffset = (m_streamOut.state.ceRamOffset + sizeof(uint32));

    // NOTE: In DX12, nested command buffers have a "gotcha" regarding stream-out SRD management: word1 of each SRD
    // contains the STRIDE and BASE_ADDRESS_HI fields. The STRIDE field is determined by the current pipeline, but
    // BASE_ADDRESS_HI is determined by the stream-output buffer bound. Unfortunately, nested command buffers don't
    // always know the current SO buffer bindings because they may have been inherited from the calling command buffer.
    // If we ask the CE to update word1 (to fixup the STRIDE value), we will blow away the current state of
    // BASE_ADDRESS_HI.
    //
    // The solution to this problem is for nested command buffers to only use the CE to patch word2 (which only has
    // the NUM_RECORDS field), and to fix the STRIDE field at draw-time using CP memory atomics. This will preserve
    // all of the fields in word1 except STRIDE, which is what we need.

    for (uint32 idx = 0; idx < MaxStreamOutTargets; ++idx)
    {
        const uint32 strideInBytes = (sizeof(uint32) * pipeline.VgtStrmoutVtxStride(idx).u32All);
        const uint32 numRecords    = StreamOutNumRecords(m_device.Parent()->ChipProperties(), strideInBytes);

        auto*const pBufferSrd    = &m_streamOut.srd[idx];
        uint32     srdNumRecords = 0;
        uint32     srdStride     = 0;

        if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
        {
            srdNumRecords = pBufferSrd->gfx9.word2.bits.NUM_RECORDS;
            srdStride     = pBufferSrd->gfx9.word1.bits.STRIDE;
        }

        if ((srdNumRecords != numRecords) || (srdStride != strideInBytes))
        {
            if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
            {
                pBufferSrd->gfx9.word2.bits.NUM_RECORDS = numRecords;
                pBufferSrd->gfx9.word1.bits.STRIDE      = strideInBytes;
            }

                // Root command buffers and nested command buffers which have changed the stream-output bindings
                // fully know the complete stream-out SRD so we can use the "normal" path.
                pCeCmdSpace += m_cmdUtil.BuildWriteConstRam(VoidPtrInc(pBufferSrd, sizeof(uint32)),
                                                            ceRamOffset,
                                                            2,
                                                            pCeCmdSpace);
            // CE RAM is now more up-to-date than the stream out table memory is, so remember that we'll need to
            // dump to GPU memory before the next Draw.
            m_streamOut.state.contentsDirty = 1;
        }

        ceRamOffset += sizeof(BufferSrd);
    }

    return pCeCmdSpace;
}

// =====================================================================================================================
// Sets user defined clip planes.
void UniversalCmdBuffer::CmdSetUserClipPlanes(
    uint32               firstPlane,
    uint32               planeCount,
    const UserClipPlane* pPlanes)
{
    UserClipPlaneStatePm4Img pm4Image = {};

    PAL_ASSERT((planeCount > 0) && (planeCount <= 6));

    BuildSetUserClipPlane(firstPlane,
                          planeCount,
                          pPlanes,
                          m_cmdUtil,
                          reinterpret_cast<uint32*>(&pm4Image));

    const size_t Pm4ImageSize = ((planeCount * sizeof(UserClipPlaneStateReg)) >> 2) + CmdUtil::ContextRegSizeDwords;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WritePm4Image(Pm4ImageSize, &pm4Image, pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
    m_deCmdStream.SetContextRollDetected<true>();
}

// =====================================================================================================================
// Builds the packets for setting the user clip plane in the command space provided.
uint32* UniversalCmdBuffer::BuildSetUserClipPlane(
    uint32                     firstPlane,
    uint32                     count,
    const UserClipPlane*       pPlanes,
    const CmdUtil&             cmdUtil,
    uint32*                    pCmdSpace)
{
    UserClipPlaneStatePm4Img* pImage       = reinterpret_cast<UserClipPlaneStatePm4Img*>(pCmdSpace);
    constexpr uint32          RegStride    = mmPA_CL_UCP_1_X - mmPA_CL_UCP_0_X;
    const uint32              regStart     = mmPA_CL_UCP_0_X + firstPlane * RegStride;
    const uint32              regEnd       = mmPA_CL_UCP_0_W + (firstPlane + count - 1) * RegStride;
    const size_t              totalDwords  = cmdUtil.BuildSetSeqContextRegs(regStart, regEnd, &pImage->header);

    for (uint32 i = 0; i < count; i++)
    {
        pImage->plane[i].paClUcpX.f32All = pPlanes[i].x;
        pImage->plane[i].paClUcpY.f32All = pPlanes[i].y;
        pImage->plane[i].paClUcpZ.f32All = pPlanes[i].z;
        pImage->plane[i].paClUcpW.f32All = pPlanes[i].w;
    }

    return pCmdSpace + totalDwords;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdXdmaWaitFlipPending()
{
    CmdWaitRegisterValue(mmXdmaSlvFlipPending, 0, 0x00000001, CompareFunc::Equal);
}

// =====================================================================================================================
// Helper function that tracks back-to-back nested executions referencing embedded data chunks.
// Returns true if the current nested command buffer was previously executed on this command buffer
bool UniversalCmdBuffer::CheckNestedExecuteReference(
    const UniversalCmdBuffer* pCmdBuffer)
{
    // A reference to nested command buffer may exist if it used embedded data for CE dumps
    const bool mayExistRef = pCmdBuffer->UseEmbeddedDataForCeRamDumps()     &&
                             (pCmdBuffer->m_ceCmdStream.IsEmpty() == false) &&
                             (pCmdBuffer->m_embeddedData.chunkList.IsEmpty() == false);

    bool existsRef = false;
    if (mayExistRef)
    {
        // Check for existing reference from nested execute
        CmdStreamChunk* pChunk = pCmdBuffer->m_embeddedData.chunkList.Begin().Get();
        for (auto iter = m_nestedChunkRefList.Begin(); iter.IsValid(); iter.Next())
        {
            if (iter.Get() == pChunk)
            {
                // Found a reference. Clear reference list for next back-to-back execution
                existsRef = true;
                m_nestedChunkRefList.Clear();
            }
        }

        // Add embedded data chunk to reference list
        m_nestedChunkRefList.PushBack(pChunk);
    }

    return existsRef;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32            cmdBufferCount,
    ICmdBuffer*const* ppCmdBuffers)
{
    for (uint32 buf = 0; buf < cmdBufferCount; ++buf)
    {
        auto*const pCmdBuffer = static_cast<Gfx9::UniversalCmdBuffer*>(ppCmdBuffers[buf]);

        ValidateExecuteNestedCmdBuffers(*pCmdBuffer);

        if (pCmdBuffer->m_state.nestedIndirectRingInstances > 0)
        {
            // The nestedIndirectRingInstances reflects the total number of ring instances used by the nested command
            // buffer. Nested command buffers will handle wrapping by inserting CE-DE sync as needed. The required
            // instance count is clamped to maximum indirect CE dump instances in this case.
            const uint32 requiredInstances = Min(pCmdBuffer->m_state.nestedIndirectRingInstances,
                                                 m_nestedIndirectCeDumpTable.ring.numInstances);

            RelocateRingedUserDataTable(&m_state,
                                        &m_nestedIndirectCeDumpTable.ring,
                                        &m_nestedIndirectCeDumpTable.state,
                                        requiredInstances);

            // Set the base address for SET_SH_REG_OFFSET indirect data.
            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
            pDeCmdSpace += m_cmdUtil.BuildSetBase(m_nestedIndirectCeDumpTable.state.gpuVirtAddr,
                                                  base_index__pfp_set_base__indirect_data_base,
                                                  ShaderGraphics,
                                                  pDeCmdSpace);

            // Set the DUMP_CONST_RAM_OFFSET indirect base for constant engine.
            uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
            pCeCmdSpace += m_cmdUtil.BuildSetBaseCe(m_nestedIndirectCeDumpTable.state.gpuVirtAddr,
                                                    base_index__ce_set_base__ce_dst_base_addr,
                                                    ShaderGraphics,
                                                    pCeCmdSpace);

            // Insert necessary CE-DE syncs for handling wrapping rings
            if (m_state.flags.ceWaitOnDeCounterDiff)
            {
                // Must sync CE-DE before executing a nested command buffer that will dump to wrapping ring
                PAL_DPWARN("Absolute CE-DE sync inserted! Increasing the CE ring size may improve performance.");
                pCeCmdSpace += m_cmdUtil.BuildWaitOnDeCounterDiff(0, pCeCmdSpace);
                m_state.flags.ceWaitOnDeCounterDiff = 0;

                // CE-DE are now completely synced up. Reset ceHasAnyRingWrapped bit for re-evaluation
                // on future dumps.
                m_state.flags.ceHasAnyRingWrapped   = 0;
            }

            if (m_state.flags.ceInvalidateKcache)
            {
                pCeCmdSpace += m_cmdUtil.BuildIncrementCeCounter(pCeCmdSpace);
                pDeCmdSpace += m_cmdUtil.BuildWaitOnCeCounter(true, pDeCmdSpace);
                pDeCmdSpace += m_cmdUtil.BuildIncrementDeCounter(pDeCmdSpace);

                m_state.flags.ceInvalidateKcache = 0;
            }

            m_spillTable.stateGfx.gpuAddrDirty = 0;

            m_ceCmdStream.CommitCommands(pCeCmdSpace);
            m_deCmdStream.CommitCommands(pDeCmdSpace);
        }

        // All user-data entries have been uploaded into CE RAM and GPU memory, so we can safely "call" the nested
        // command buffer's command streams.

        const bool exclusiveSubmit = pCmdBuffer->IsExclusiveSubmit();
        const bool allowIb2Launch  = (pCmdBuffer->AllowLaunchViaIb2() &&
                                      (pCmdBuffer->m_state.flags.containsDrawIndirect == 0));

        m_deCmdStream.TrackNestedEmbeddedData(pCmdBuffer->m_embeddedData.chunkList);
        m_deCmdStream.TrackNestedCommands(pCmdBuffer->m_deCmdStream);
        m_ceCmdStream.TrackNestedCommands(pCmdBuffer->m_ceCmdStream);

        const bool existRef = CheckNestedExecuteReference(pCmdBuffer);
        if (existRef)
        {
            // If the nested command buffer has been called by this caller before, and it dumped CE RAM to embedded
            // data instead of to the per-Device CE RAM ring buffer, we need to "throttle" the constant engine to
            // ensure that the previous call of the nested command buffer has time for its DE to catch up to the CE.
            uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
            pCeCmdSpace += m_cmdUtil.BuildWaitOnDeCounterDiff(1, pCeCmdSpace);
            m_ceCmdStream.CommitCommands(pCeCmdSpace);

            AcquireMemInfo acquireInfo = {};
            acquireInfo.flags.invSqI$ = 1;
            acquireInfo.flags.invSqK$ = 1;
            acquireInfo.tcCacheOp     = TcCacheOp::InvL1;
            acquireInfo.engineType    = EngineTypeUniversal;
            acquireInfo.baseAddress   = FullSyncBaseAddr;
            acquireInfo.sizeBytes     = FullSyncSize;

            // We also need to invalidate the Kcache when we do this to make sure that the embedded data contents for
            // the data dumped from CE RAM is actually re-read from memory.
            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
            pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, EngineTypeUniversal, pDeCmdSpace);
            pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PS_PARTIAL_FLUSH, EngineTypeUniversal, pDeCmdSpace);
            pDeCmdSpace += m_cmdUtil.BuildAcquireMem(acquireInfo, pDeCmdSpace);
            m_deCmdStream.CommitCommands(pDeCmdSpace);
            m_deCmdStream.SetContextRollDetected<false>();
        }

        m_deCmdStream.Call(pCmdBuffer->m_deCmdStream, exclusiveSubmit, allowIb2Launch);
        m_ceCmdStream.Call(pCmdBuffer->m_ceCmdStream, exclusiveSubmit, allowIb2Launch);

        // Callee command buffers are also able to leak any changes they made to bound user-data entries and any other
        // state back to the caller.
        LeakNestedCmdBufferState(*pCmdBuffer);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::AddPerPresentCommands(
    gpusize frameCountGpuAddr,
    uint32  frameCntReg)
{
    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    pDeCmdSpace += m_cmdUtil.BuildAtomicMem(AtomicOp::IncUint32,
                                            frameCountGpuAddr,
                                            UINT32_MAX,
                                            pDeCmdSpace);

    pDeCmdSpace += m_cmdUtil.BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
                                                   dst_sel__me_copy_data__perfcounters,
                                                   frameCntReg,
                                                   src_sel__me_copy_data__tc_l2,
                                                   frameCountGpuAddr,
                                                   count_sel__me_copy_data__32_bits_of_data,
                                                   wr_confirm__me_copy_data__do_not_wait_for_confirmation,
                                                   pDeCmdSpace);

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// When Rb+ is enabled, pipelines are created per shader export format, however, same export format possibly supports
// several down convert formats. For example, FP16_ABGR supports 8_8_8_8, 5_6_5, 1_5_5_5, 4_4_4_4, etc. Need to build
// the commands to overwrite the RbPlus related registers according to the format.
// Please note that this method is supposed to be called right after the internal graphic pipelines are bound to command
// buffer.
void UniversalCmdBuffer::CmdOverwriteRbPlusFormatForBlits(
    SwizzledFormat format,
    uint32         targetIndex)
{
    const Pal::PipelineState* pPipelineState = PipelineState(PipelineBindPoint::Graphics);
    const GraphicsPipeline*   pPipeline      = static_cast<const GraphicsPipeline*>(pPipelineState->pPipeline);
    RbPlusPm4Img              pm4Image       = {};

    pPipeline->BuildRbPlusRegistersForRpm(format, targetIndex, &pm4Image);

    if (pm4Image.spaceNeeded)
    {
        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

        pDeCmdSpace = m_deCmdStream.WritePm4Image(pm4Image.spaceNeeded, &pm4Image, pDeCmdSpace);

        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }
}

// =====================================================================================================================
// Applies the primgroup size optimization to a new draw.
void UniversalCmdBuffer::UpdatePrimGroupOpt(
    uint32 vxtIdxCount)
{
    const auto& pipeline = static_cast<const GraphicsPipeline&>(*m_graphicsState.pipelineState.pPipeline);

    // Update the draw counters.
    m_primGroupOpt.vtxIdxTotal += vxtIdxCount;
    m_primGroupOpt.drawCount++;

    // If we've reached the end of the window, determine if we need to update our primgroup size.
    const uint32 windowSize = m_primGroupOpt.windowSize;
    if (m_primGroupOpt.drawCount >= windowSize)
    {
        // Compute the optimal primgroup size. The calculation is simple, compute the average primgroup size over the
        // window, divide by the number of prims per clock, round to a multiple of the step, and clamp to the min/max.
        const uint32 primRate      = m_device.Parent()->ChipProperties().primsPerClock;
        const uint64 primTotal     = m_primGroupOpt.vtxIdxTotal / pipeline.VertsPerPrimitive();
        const uint32 rawGroupSize  = static_cast<uint32>(primTotal / (windowSize * primRate));
        const uint32 roundedSize   = static_cast<uint32>(Pow2AlignDown(rawGroupSize, m_primGroupOpt.step));
        m_primGroupOpt.optimalSize = Min(m_primGroupOpt.maxSize, Max(m_primGroupOpt.minSize, roundedSize));

        // Reset the draw counters.
        m_primGroupOpt.vtxIdxTotal = 0;
        m_primGroupOpt.drawCount   = 0;
    }
}

// =====================================================================================================================
// Disables the primgroup size optimization and zeros the optimial primgroup size.
void UniversalCmdBuffer::DisablePrimGroupOpt()
{
    // Force off the primgroup size optimization and reset our primgroup size.
    // We do this to be sure that any large indirect draws will still run at full speed.
    m_primGroupOpt.enabled     = false;
    m_primGroupOpt.optimalSize = 0;
}

// =====================================================================================================================
// Stream-out target GPU addresses must be DWORD-aligned, so we can use the LSB of the address to know if
// a stream-out target has ever been set for this command buffer.
bool UniversalCmdBuffer::HasStreamOutBeenSet() const
{
    return ((m_device.GetBaseAddress(&m_streamOut.srd[0]) & 1) == 0);
}

// =====================================================================================================================
// Inserts sync commands after each chunk to idle and flush all relevant caches.
void UniversalCmdBuffer::P2pBltWaSync()
{
    constexpr HwPipePoint PipePoint = HwPipePoint::HwPipeBottom;

    BarrierTransition transition   = { };
    transition.dstCacheMask        = CoherMemory;
    transition.srcCacheMask        = CoherColorTarget | CoherShader;

    BarrierInfo barrierInfo        = { };
    barrierInfo.waitPoint          = HwPipePoint::HwPipeTop;
    barrierInfo.pipePointWaitCount = 1;
    barrierInfo.pPipePoints        = &PipePoint;
    barrierInfo.transitionCount    = 1;
    barrierInfo.pTransitions       = &transition;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 360
    barrierInfo.reason             = Developer::BarrierReasonP2PBlitSync;
#endif
    CmdBarrier(barrierInfo);
}

// =====================================================================================================================
// MCBP must be disabled when the P2P BAR workaround is being applied.  This can be done by temporarily disabling
// state shadowing with a CONTEXT_CONTROL packet.  Shadowing will be re-enabled in P2pBltWaCopyEnd().
void UniversalCmdBuffer::P2pBltWaCopyBegin(
    const GpuMemory* pDstMemory,
    uint32           regionCount,
    const gpusize*   pChunkAddrs)
{
    if (m_device.Parent()->IsPreemptionSupported(EngineType::EngineTypeUniversal))
    {
        PM4PFP_CONTEXT_CONTROL contextControl = m_device.GetContextControl();

        contextControl.bitfields3.shadow_per_context_state = 0;
        contextControl.bitfields3.shadow_cs_sh_regs        = 0;
        contextControl.bitfields3.shadow_gfx_sh_regs       = 0;
        contextControl.bitfields3.shadow_global_config     = 0;
        contextControl.bitfields3.shadow_global_uconfig    = 0;

        uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
        pCmdSpace += m_cmdUtil.BuildContextControl(contextControl, pCmdSpace);
        m_deCmdStream.CommitCommands(pCmdSpace);
    }

    Pal::UniversalCmdBuffer::P2pBltWaCopyBegin(pDstMemory, regionCount, pChunkAddrs);
}

// =====================================================================================================================
// Called before each region of a P2P BLT where the P2P PCI BAR workaround is enabled.  Graphics BLTs require a idle
// and cache flush between chunks.
void UniversalCmdBuffer::P2pBltWaCopyNextRegion(
    gpusize chunkAddr)
{
    // An idle is only required if the new chunk address is different than the last chunk entry.  This logic must be
    // mirrored in P2pBltWaCopyBegin().
    if (chunkAddr != m_p2pBltWaLastChunkAddr)
    {
        P2pBltWaSync();
    }

    Pal::UniversalCmdBuffer::P2pBltWaCopyNextRegion(chunkAddr);
}

// =====================================================================================================================
// Re-enabled MCBP if it was disabled in P2pBltWaCopyBegin().
void UniversalCmdBuffer::P2pBltWaCopyEnd()
{
    P2pBltWaSync();

    Pal::UniversalCmdBuffer::P2pBltWaCopyEnd();

    if (m_device.Parent()->IsPreemptionSupported(EngineType::EngineTypeUniversal))
    {
        PM4PFP_CONTEXT_CONTROL contextControl = m_device.GetContextControl();

        uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
        pCmdSpace += m_cmdUtil.BuildContextControl(contextControl, pCmdSpace);
        m_deCmdStream.CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Build write view id commands.
uint32* UniversalCmdBuffer::BuildWriteViewId(
    uint32  viewId,
    uint32* pCmdSpace)
{
    for (uint32 i = 0; i < NumHwShaderStagesGfx; ++i)
    {
        const uint16 viewIdRegAddr = m_pSignatureGfx->viewIdRegAddr[i];
        if (viewIdRegAddr != UserDataNotMapped)
        {
            pCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics>(viewIdRegAddr, viewId, pCmdSpace);
        }
        else
        {
            break;
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Switch draw functions.
void UniversalCmdBuffer::SwitchDrawFunctions(
    bool viewInstancingEnable,
    bool nggFastLuanch)
{
    if (viewInstancingEnable)
    {
        if (m_cachedSettings.issueSqttMarkerEvent)
        {
            m_funcTable.pfnCmdDraw              = CmdDraw<true, true>;
            m_funcTable.pfnCmdDrawIndirectMulti = CmdDrawIndirectMulti<true, true>;

            if (nggFastLuanch)
            {
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<true, true, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti = CmdDrawIndexedIndirectMulti<true, true, true>;
            }
            else
            {
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<true, false, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti = CmdDrawIndexedIndirectMulti<true, false, true>;
            }
        }
        else
        {
            m_funcTable.pfnCmdDraw              = CmdDraw<false, true>;
            m_funcTable.pfnCmdDrawIndirectMulti = CmdDrawIndirectMulti<false, true>;

            if (nggFastLuanch)
            {
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<false, true, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti = CmdDrawIndexedIndirectMulti<false, true, true>;
            }
            else
            {
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<false, false, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti = CmdDrawIndexedIndirectMulti<false, false, true>;
            }
        }
    }
    else
    {
        if (m_cachedSettings.issueSqttMarkerEvent)
        {
            m_funcTable.pfnCmdDraw              = CmdDraw<true, false>;
            m_funcTable.pfnCmdDrawIndirectMulti = CmdDrawIndirectMulti<true, false>;

            if (nggFastLuanch)
            {
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<true, true, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti = CmdDrawIndexedIndirectMulti<true, true, false>;
            }
            else
            {
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<true, false, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti = CmdDrawIndexedIndirectMulti<true, false, false>;
            }
        }
        else
        {
            m_funcTable.pfnCmdDraw              = CmdDraw<false, false>;
            m_funcTable.pfnCmdDrawIndirectMulti = CmdDrawIndirectMulti<false, false>;

            if (nggFastLuanch)
            {
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<false, true, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti = CmdDrawIndexedIndirectMulti<false, true, false>;
            }
            else
            {
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<false, false, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti = CmdDrawIndexedIndirectMulti<false, false, false>;
            }
        }
    }
}

} // Gfx9
} // Pal
