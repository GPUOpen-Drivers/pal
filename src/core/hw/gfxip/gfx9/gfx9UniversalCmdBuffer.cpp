/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/queryPool.h"
#include "core/g_palPlatformSettings.h"
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

// Microcode version for NGG Indexed Indirect Draw support.
constexpr uint32 UcodeVersionNggIndexedIndirectDraw  = 34;

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

// Uint32 versions of the enumeration values for hardware stage ID.
constexpr uint32 HsStageId = static_cast<uint32>(HwShaderStage::Hs);
constexpr uint32 GsStageId = static_cast<uint32>(HwShaderStage::Gs);
constexpr uint32 VsStageId = static_cast<uint32>(HwShaderStage::Vs);
constexpr uint32 PsStageId = static_cast<uint32>(HwShaderStage::Ps);

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
PAL_INLINE static uint32 StreamOutNumRecords(
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

    return ((UINT_MAX - chipProps.gfx9.maxWavefrontSize) + 1);
}

// =====================================================================================================================
size_t UniversalCmdBuffer::GetSize(
    const Device& device)
{
    // Space enough for the object and vertex buffer SRD table.
    constexpr size_t Alignment = alignof(BufferSrd);
    return (Pow2Align(sizeof(UniversalCmdBuffer), Alignment) + (sizeof(BufferSrd) * MaxVertexBuffers));
}

// =====================================================================================================================
UniversalCmdBuffer::UniversalCmdBuffer(
    const Device&              device,
    const CmdBufferCreateInfo& createInfo)
    :
    Pal::UniversalCmdBuffer(device,
                            createInfo,
                            &m_deCmdStream,
                            &m_ceCmdStream,
                            device.Settings().blendOptimizationsEnable),
    m_device(device),
    m_cmdUtil(device.CmdUtil()),
    m_deCmdStream(device,
                  createInfo.pCmdAllocator,
                  EngineTypeUniversal,
                  SubEngineType::Primary,
                  CmdStreamUsage::Workload,
                  IsNested()),
    m_ceCmdStream(device,
                  createInfo.pCmdAllocator,
                  EngineTypeUniversal,
                  SubEngineType::ConstantEngine,
                  CmdStreamUsage::Workload,
                  IsNested()),
    m_pSignatureCs(&NullCsSignature),
    m_pSignatureGfx(&NullGfxSignature),
    m_pipelineCtxPm4Hash(0),
    m_pfnValidateUserDataGfx(nullptr),
    m_pfnValidateUserDataGfxPipelineSwitch(nullptr),
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
    m_activeOcclusionQueryWriteRanges(m_device.GetPlatform())
{
    const PalPlatformSettings& platformSettings = m_device.Parent()->GetPlatform()->PlatformSettings();
    const PalSettings&         coreSettings     = m_device.Parent()->Settings();
    const Gfx9PalSettings&     settings         = m_device.Settings();
    const auto*const           pPublicSettings  = m_device.Parent()->GetPublicSettings();

    memset(&m_vbTable,         0, sizeof(m_vbTable));
    memset(&m_spillTable,      0, sizeof(m_spillTable));
    memset(&m_streamOut,       0, sizeof(m_streamOut));
    memset(&m_nggTable,        0, sizeof(m_nggTable));
    memset(&m_state,           0, sizeof(m_state));
    memset(&m_paScBinnerCntl0, 0, sizeof(m_paScBinnerCntl0));
    memset(&m_cachedSettings,  0, sizeof(m_cachedSettings));
    memset(&m_drawTimeHwState, 0, sizeof(m_drawTimeHwState));
    memset(&m_nggState,        0, sizeof(m_nggState));
    memset(&m_currentBinSize,  0, sizeof(m_currentBinSize));

    memset(&m_pipelinePsHash, 0, sizeof(m_pipelinePsHash));
    m_pipelineFlags.u32All = 0;

    // Setup default engine support - Universal Cmd Buffer supports Graphics, Compute and CPDMA.
    m_engineSupport = (CmdBufferEngineSupport::Graphics |
                       CmdBufferEngineSupport::Compute  |
                       CmdBufferEngineSupport::CpDma);

    // Setup all of our cached settings checks.
    m_cachedSettings.tossPointMode              = static_cast<uint32>(coreSettings.tossPointMode);
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
    m_cachedSettings.scissorChangeWa            = settings.waMiscScissorRegisterChange;
    m_cachedSettings.checkDfsmEqaaWa =
                     (settings.waDisableDfsmWithEqaa                  &&   // Is the workaround enabled on this GPU?
                      (m_cachedSettings.disableDfsm         == false) &&   // Is DFSM already forced off?
                      (m_cachedSettings.disableBatchBinning == false));    // Is binning enabled?
    m_cachedSettings.batchBreakOnNewPs         = settings.batchBreakOnNewPixelShader;
    m_cachedSettings.padParamCacheSpace        =
            ((pPublicSettings->contextRollOptimizationFlags & PadParamCacheSpace) != 0);

    if (settings.binningMode == Gfx9DeferredBatchBinCustom)
    {
        // The custom bin size setting is encoded as two uint16's.
        m_customBinSizeX = settings.customBatchBinSize >> 16;
        m_customBinSizeY = settings.customBatchBinSize & 0xFFFF;

        PAL_ASSERT(IsPowerOfTwo(m_customBinSizeX) && IsPowerOfTwo(m_customBinSizeY));
    }

    const bool sqttEnabled = (platformSettings.gpuProfilerMode > GpuProfilerCounterAndTimingOnly) &&
                             (TestAnyFlagSet(platformSettings.gpuProfilerConfig.traceModeMask, GpuProfilerTraceSqtt));
    m_cachedSettings.issueSqttMarkerEvent = (sqttEnabled || device.GetPlatform()->IsDevDriverProfilingEnabled());

    m_paScBinnerCntl0.u32All = 0;
    // Initialize defaults for some of the fields in PA_SC_BINNER_CNTL_0.
    m_savedPaScBinnerCntl0.bits.CONTEXT_STATES_PER_BIN    = settings.binningContextStatesPerBin - 1;
    m_savedPaScBinnerCntl0.bits.PERSISTENT_STATES_PER_BIN = settings.binningPersistentStatesPerBin - 1;
    m_savedPaScBinnerCntl0.bits.FPOVS_PER_BATCH           = settings.binningFpovsPerBatch;
    m_savedPaScBinnerCntl0.bits.OPTIMAL_BIN_SELECTION     = settings.binningOptimalBinSelection;

    // Initialize to the common value for most pipelines (no conservative rast).
    m_paScConsRastCntl.u32All                         = 0;
    m_paScConsRastCntl.bits.NULL_SQUAD_AA_MASK_ENABLE = 1;

    memset(&m_rbPlusPm4Img, 0, sizeof(m_rbPlusPm4Img));
    if (m_device.Parent()->ChipProperties().gfx9.rbPlus != 0)
    {
        m_rbPlusPm4Img.spaceNeeded = m_device.CmdUtil().BuildSetSeqContextRegs(mmSX_PS_DOWNCONVERT,
                                                                               mmSX_BLEND_OPT_CONTROL,
                                                                               &m_rbPlusPm4Img.header);
    }

    SwitchDrawFunctions(
        false,
        false);
}

// =====================================================================================================================
// Initializes Gfx9-specific functionality.
Result UniversalCmdBuffer::Init(
    const CmdBufferInternalCreateInfo& internalInfo)
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    uint32 ceRamOffset = 0;
    m_spillTable.stateCs.sizeInDwords = chipProps.gfxip.maxUserDataEntries;
    m_spillTable.stateCs.ceRamOffset  = ceRamOffset;
    ceRamOffset += (sizeof(uint32) * m_spillTable.stateCs.sizeInDwords);

    m_spillTable.stateGfx.sizeInDwords = chipProps.gfxip.maxUserDataEntries;
    m_spillTable.stateGfx.ceRamOffset  = ceRamOffset;
    ceRamOffset += (sizeof(uint32) * m_spillTable.stateGfx.sizeInDwords);

    m_streamOut.state.sizeInDwords = (sizeof(m_streamOut.srd) / sizeof(uint32));
    m_streamOut.state.ceRamOffset  = ceRamOffset;
    ceRamOffset += sizeof(m_streamOut.srd);

    if (m_device.Settings().nggEnableMode != NggPipelineTypeDisabled)
    {
        const uint32 nggTableBytes = Pow2Align<uint32>(sizeof(Abi::PrimShaderCbLayout), 256);

        m_nggTable.state.sizeInDwords = NumBytesToNumDwords(nggTableBytes);
        m_nggTable.state.ceRamOffset  = ceRamOffset;
        ceRamOffset                  += nggTableBytes;
    }

    m_vbTable.pSrds              = static_cast<BufferSrd*>(VoidPtrAlign((this + 1), alignof(BufferSrd)));
    m_vbTable.state.sizeInDwords = ((sizeof(BufferSrd) / sizeof(uint32)) * MaxVertexBuffers);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 469
    m_vbTable.state.ceRamOffset  = ceRamOffset;
    ceRamOffset += (sizeof(uint32) * m_vbTable.state.sizeInDwords);
#endif

    PAL_ASSERT(ceRamOffset <= ReservedCeRamBytes);
    ceRamOffset = ReservedCeRamBytes;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 469
    m_vbTable.state.ceRamOffset  =
        (static_cast<uint32>(m_device.Parent()->IndirectUserDataTableCeRamOffset()) + ceRamOffset);
    ceRamOffset += (sizeof(uint32) * m_vbTable.state.sizeInDwords);
#endif

    Result result = Pal::UniversalCmdBuffer::Init(internalInfo);

    if (result == Result::Success)
    {
        result = m_deCmdStream.Init();
    }

    if (result == Result::Success)
    {
        result = m_ceCmdStream.Init();
    }

    return result;
}

// =====================================================================================================================
// Sets-up function pointers for the Dispatch entrypoint and all variants.
template <bool IssueSqttMarkerEvent>
void UniversalCmdBuffer::SetDispatchFunctions()
{
    if (UseCpuPathInsteadOfCeRam())
    {
        m_funcTable.pfnCmdDispatch         = CmdDispatch<IssueSqttMarkerEvent, true>;
        m_funcTable.pfnCmdDispatchIndirect = CmdDispatchIndirect<IssueSqttMarkerEvent, true>;
        m_funcTable.pfnCmdDispatchOffset   = CmdDispatchOffset<IssueSqttMarkerEvent, true>;
    }
    else
    {
        m_funcTable.pfnCmdDispatch         = CmdDispatch<IssueSqttMarkerEvent, false>;
        m_funcTable.pfnCmdDispatchIndirect = CmdDispatchIndirect<IssueSqttMarkerEvent, false>;
        m_funcTable.pfnCmdDispatchOffset   = CmdDispatchOffset<IssueSqttMarkerEvent, false>;
    }
}

// =====================================================================================================================
// Sets up function pointers for Draw-time validation of graphics user-data entries.
template <bool TessEnabled, bool GsEnabled, bool VsEnabled>
void UniversalCmdBuffer::SetUserDataValidationFunctions()
{
    if (UseCpuPathInsteadOfCeRam())
    {
        m_pfnValidateUserDataGfx =
            &UniversalCmdBuffer::ValidateGraphicsUserDataCpu<false, TessEnabled, GsEnabled, VsEnabled>;
        m_pfnValidateUserDataGfxPipelineSwitch =
            &UniversalCmdBuffer::ValidateGraphicsUserDataCpu<true, TessEnabled, GsEnabled, VsEnabled>;
    }
    else
    {
        m_pfnValidateUserDataGfx =
            &UniversalCmdBuffer::ValidateGraphicsUserDataCeRam<false, TessEnabled, GsEnabled, VsEnabled>;
        m_pfnValidateUserDataGfxPipelineSwitch =
            &UniversalCmdBuffer::ValidateGraphicsUserDataCeRam<true, TessEnabled, GsEnabled, VsEnabled>;
    }
}

// =====================================================================================================================
// Sets up function pointers for Draw-time validation of graphics user-data entries.
void UniversalCmdBuffer::SetUserDataValidationFunctions(
    bool tessEnabled,
    bool gsEnabled,
    bool isNgg)
{
    if (isNgg)
    {
        if (tessEnabled)
        {
            SetUserDataValidationFunctions<true, true, false>();
        }
        else
        {
            SetUserDataValidationFunctions<false, true, false>();
        }
    }
    else if (tessEnabled)
    {
        if (gsEnabled)
        {
            SetUserDataValidationFunctions<true, true, true>();
        }
        else
        {
            SetUserDataValidationFunctions<true, false, true>();
        }
    }
    else
    {
        if (gsEnabled)
        {
            SetUserDataValidationFunctions<false, true, true>();
        }
        else
        {
            SetUserDataValidationFunctions<false, false, true>();
        }
    }
}

// =====================================================================================================================
// Resets all of the state tracked by this command buffer
void UniversalCmdBuffer::ResetState()
{
    Pal::UniversalCmdBuffer::ResetState();

    if (m_cachedSettings.issueSqttMarkerEvent)
    {
        SetDispatchFunctions<true>();
    }
    else
    {
        SetDispatchFunctions<false>();
    }

    SetUserDataValidationFunctions(false, false, false);
    SwitchDrawFunctions(
        false,
        false);

    m_vgtDmaIndexType.u32All = 0;
    m_vgtDmaIndexType.bits.SWAP_MODE = VGT_DMA_SWAP_NONE;

    // For IndexBuffers - default to STREAM cache policy so that they get evicted from L2 as soon as possible.
    {
        PAL_ASSERT(IsGfx9(*m_device.Parent()));
        m_vgtDmaIndexType.gfx09.RDREQ_POLICY = VGT_POLICY_STREAM;
    }

    m_spiVsOutConfig.u32All    = 0;
    m_spiPsInControl.u32All    = 0;
    m_paScShaderControl.u32All = 0xFFFFFFFF; ///< Initialize to a known bad value to ensure it is flushed on 1st draw
    m_binningMode              = FORCE_BINNING_ON; // set a value that we would never use

    // Reset the command buffer's HWL state tracking
    m_state.flags.u32All                            = 0;
    m_state.pLastDumpCeRam                          = nullptr;
    m_state.lastDumpCeRamOrdinal2.u32All            = 0;
    m_state.lastDumpCeRamOrdinal2.bits.increment_ce = 1;
    m_state.minCounterDiff                          = UINT_MAX;

    // Set to an invalid (unaligned) address to indicate that streamout hasn't been set yet, and initialize the SRDs'
    // NUM_RECORDS fields to indicate a zero stream-out stride.
    memset(&m_streamOut.srd[0], 0, sizeof(m_streamOut.srd));
    m_device.SetBaseAddress(&m_streamOut.srd[0], 1);
    for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
    {
        m_device.SetNumRecords(&m_streamOut.srd[i], StreamOutNumRecords(m_device.Parent()->ChipProperties(), 0));
    }

    ResetUserDataTable(&m_streamOut.state);
    ResetUserDataTable(&m_nggTable.state);

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
    m_nggState.numSamples       = 1;

    m_pSignatureCs         = &NullCsSignature;
    m_pSignatureGfx        = &NullGfxSignature;
    m_pipelineCtxPm4Hash   = 0;
    m_pipelinePsHash.lower = 0;
    m_pipelinePsHash.upper = 0;
    m_pipelineFlags.u32All = 0;

    ResetUserDataTable(&m_spillTable.stateCs);
    ResetUserDataTable(&m_spillTable.stateGfx);
    ResetUserDataTable(&m_vbTable.state);
    m_vbTable.watermark = m_vbTable.state.sizeInDwords;
    m_vbTable.modified  = 0;

    m_activeOcclusionQueryWriteRanges.Clear();
}

// =====================================================================================================================
// Binds a graphics or compute pipeline to this command buffer.
void UniversalCmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    if (params.pipelineBindPoint == PipelineBindPoint::Graphics)
    {
        auto*const pNewPipeline = static_cast<const GraphicsPipeline*>(params.pPipeline);
        auto*const pOldPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

        const bool isNgg       = (pNewPipeline != nullptr) && pNewPipeline->IsNgg();
        const bool tessEnabled = (pNewPipeline != nullptr) && pNewPipeline->IsTessEnabled();
        const bool gsEnabled   = (pNewPipeline != nullptr) && pNewPipeline->IsGsEnabled();

        SetUserDataValidationFunctions(tessEnabled, gsEnabled, isNgg);

        const bool newIsNggFastLaunch    = (pNewPipeline != nullptr) && pNewPipeline->IsNggFastLaunch();
        const bool oldIsNggFastLaunch    = (pOldPipeline != nullptr) && pOldPipeline->IsNggFastLaunch();
        const bool newUsesViewInstancing = (pNewPipeline != nullptr) && pNewPipeline->UsesViewInstancing();
        const bool oldUsesViewInstancing = (pOldPipeline != nullptr) && pOldPipeline->UsesViewInstancing();

        // NGG Fast Launch pipelines require issuing different packets for indexed draws. We'll need to switch the
        // draw function pointers around to handle this case.
        if ((oldIsNggFastLaunch     != newIsNggFastLaunch)     ||
            (oldUsesViewInstancing  != newUsesViewInstancing))
        {
            SwitchDrawFunctions(
                newUsesViewInstancing,
                newIsNggFastLaunch);
        }

        // If RB+ is enabled, we must update the PM4 image of RB+ register state with the new pipelines' values.  This
        // should be done here instead of inside SwitchGraphicsPipeline() because RPM sometimes overrides these values
        // for certain blit operations.
        if ((m_rbPlusPm4Img.spaceNeeded != 0) && (pNewPipeline != nullptr))
        {
            m_rbPlusPm4Img.sxPsDownconvert   = pNewPipeline->SxPsDownconvert();
            m_rbPlusPm4Img.sxBlendOptEpsilon = pNewPipeline->SxBlendOptEpsilon();
            m_rbPlusPm4Img.sxBlendOptControl = pNewPipeline->SxBlendOptControl();
        }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 473
        constexpr uint32 DwordsPerSrd = (sizeof(BufferSrd) / sizeof(uint32));
        const uint32 vbTableDwords =
            ((pNewPipeline == nullptr) ? 0 : pNewPipeline->VertexBufferCount() * DwordsPerSrd);
        PAL_ASSERT(vbTableDwords <= m_vbTable.state.sizeInDwords);

        if (vbTableDwords > m_vbTable.watermark)
        {
            // If the current high watermark is increasing, we need to mark the contents as dirty because data which
            // was previously uploaded to CE RAM wouldn't have been dumped to GPU memory before the previous Draw.
            m_vbTable.state.dirty = 1;
        }

        m_vbTable.watermark = vbTableDwords;
#endif
    }

     Pal::UniversalCmdBuffer::CmdBindPipeline(params);
}

// =====================================================================================================================
// Updates the graphics state with a new pipeline and performs any extra work due to the pipeline switch.
uint32* UniversalCmdBuffer::SwitchGraphicsPipeline(
    const GraphicsPipelineSignature* pPrevSignature,
    const GraphicsPipeline*          pCurrPipeline,
    uint32*                          pDeCmdSpace)
{
    PAL_ASSERT(pCurrPipeline != nullptr);

    const bool isFirstDrawInCmdBuf = (m_state.flags.firstDrawExecuted == 0);
    const bool wasPrevPipelineNull = (pPrevSignature == &NullGfxSignature);
    const bool isNgg               = pCurrPipeline->IsNgg();
    const bool tessEnabled         = pCurrPipeline->IsTessEnabled();
    const bool gsEnabled           = pCurrPipeline->IsGsEnabled();

    const uint64 ctxPm4Hash = pCurrPipeline->GetContextPm4ImgHash();
    if (wasPrevPipelineNull || (m_pipelineCtxPm4Hash != ctxPm4Hash))
    {
        pDeCmdSpace = pCurrPipeline->WriteContextCommands(&m_deCmdStream, pDeCmdSpace);
        m_deCmdStream.SetContextRollDetected<true>();

        m_pipelineCtxPm4Hash = ctxPm4Hash;
    }

    if (m_rbPlusPm4Img.spaceNeeded != 0)
    {
        pDeCmdSpace = m_deCmdStream.WritePm4Image(m_rbPlusPm4Img.spaceNeeded, &m_rbPlusPm4Img, pDeCmdSpace);
        m_deCmdStream.SetContextRollDetected<true>();
    }

    if (m_cachedSettings.batchBreakOnNewPs)
    {
        const ShaderHash& psHash = pCurrPipeline->GetInfo().shader[static_cast<uint32>(ShaderType::Pixel)].hash;
        if (wasPrevPipelineNull || (ShaderHashesEqual(m_pipelinePsHash, psHash) == false))
        {
            pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(BREAK_BATCH, EngineTypeUniversal, pDeCmdSpace);
            m_pipelinePsHash = psHash;
        }
    }

    // Get new pipeline state VS/PS registers
    regSPI_VS_OUT_CONFIG spiVsOutConfig = pCurrPipeline->SpiVsOutConfig();
    regSPI_PS_IN_CONTROL spiPsInControl = pCurrPipeline->SpiPsInControl();

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
    if (wasPrevPipelineNull || (m_spiVsOutConfig.u32All != spiVsOutConfig.u32All))
    {
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmSPI_VS_OUT_CONFIG, spiVsOutConfig.u32All, pDeCmdSpace);
        m_spiVsOutConfig = spiVsOutConfig;
    }

    // Write PS_IN_CONTROL if the register changed or this is the first pipeline switch
    if (wasPrevPipelineNull || (m_spiPsInControl.u32All != spiPsInControl.u32All))
    {
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmSPI_PS_IN_CONTROL, spiPsInControl.u32All, pDeCmdSpace);
        m_spiPsInControl = spiPsInControl;
    }

    const bool usesMultiViewports = pCurrPipeline->UsesMultipleViewports();
    if (usesMultiViewports != (m_graphicsState.enableMultiViewport != 0))
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
        }

        m_graphicsState.enableMultiViewport    = usesMultiViewports;
        m_graphicsState.everUsedMultiViewport |= usesMultiViewports;
    }

    if (m_vertexOffsetReg != m_pSignatureGfx->vertexOffsetRegAddr)
    {
        m_vertexOffsetReg = m_pSignatureGfx->vertexOffsetRegAddr;

        // If the vsUserRegBase setting is changing we must invalidate the instance offset and vertex offset state
        // so that the appropriate user data registers are updated.
        m_drawTimeHwState.valid.instanceOffset = 0;
        m_drawTimeHwState.valid.vertexOffset   = 0;
    }

    if (isNgg)
    {
        // We need to update the primitive shader constant buffer with this new pipeline.
        pCurrPipeline->UpdateNggPrimCb(&m_state.primShaderCbLayout.pipelineStateCb);
        SetPrimShaderWorkload();

        if (m_nggState.startIndexReg != m_pSignatureGfx->startIndexRegAddr)
        {
            m_nggState.startIndexReg = m_pSignatureGfx->startIndexRegAddr;
            m_drawTimeHwState.valid.indexOffset = 0;
        }
        if (m_nggState.log2IndexSizeReg != m_pSignatureGfx->log2IndexSizeRegAddr)
        {
            m_nggState.log2IndexSizeReg = m_pSignatureGfx->log2IndexSizeRegAddr;
            m_drawTimeHwState.valid.log2IndexSize = 0;
        }
    }

    if (m_drawIndexReg != m_pSignatureGfx->drawIndexRegAddr)
    {
        m_drawIndexReg = m_pSignatureGfx->drawIndexRegAddr;
        if (m_drawIndexReg != UserDataNotMapped)
        {
            m_drawTimeHwState.valid.drawIndex = 0;
        }
    }

    // Save the set of pipeline flags for the next pipeline transition.  This should come last because the previous
    // pipelines' values are used earlier in the function.
    m_pipelineFlags.isNgg    = isNgg;
    m_pipelineFlags.usesTess = tessEnabled;
    m_pipelineFlags.usesGs   = gsEnabled;

    return pDeCmdSpace;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetMsaaQuadSamplePattern(
    uint32                       numSamplesPerPixel,
    const MsaaQuadSamplePattern& quadSamplePattern)
{
    PAL_ASSERT((numSamplesPerPixel > 0) && (numSamplesPerPixel <= MaxMsaaRasterizerSamples));
    m_graphicsState.quadSamplePatternState                           = quadSamplePattern;
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
    pDeCmdSpace = m_deCmdStream.WritePm4Image(SizeOfMsaaSamplePositionsPm4ImageInDwords,
                                              &samplePosPm4Image,
                                              pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

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
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetScissorRects(
    const ScissorRectParams& params)
{
    const size_t scissorSize = (sizeof(params.scissors[0]) * params.count);

    m_graphicsState.scissorRectState.count = params.count;
    memcpy(&m_graphicsState.scissorRectState.scissors[0], &params.scissors[0], scissorSize);

    m_graphicsState.dirtyFlags.validationBits.scissorRects = 1;
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
        m_drawTimeHwState.nggIndexBufferPfStartAddr    = 0;
        m_drawTimeHwState.nggIndexBufferPfEndAddr      = 0;
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

        m_nggState.numSamples = pNewState->NumSamples();
    }
    else
    {
        m_nggState.numSamples = 1;
    }

    m_graphicsState.pMsaaState                          = pNewState;
    m_graphicsState.dirtyFlags.validationBits.msaaState = 1;
    m_nggState.flags.dirty.msaaState                    = 1;
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

    size_t                                totalDwords   = 0;
    InputAssemblyStatePm4Img*             pImage        = reinterpret_cast<InputAssemblyStatePm4Img*>(pCmdSpace);
    PFP_SET_UCONFIG_REG_INDEX_index_enum  primTypeIndex = index__pfp_set_uconfig_reg_index__prim_type__GFX09;

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
void UniversalCmdBuffer::CmdBarrier(
    const BarrierInfo& barrierInfo)
{
    CmdBuffer::CmdBarrier(barrierInfo);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_gfxCmdBufState.flags.packetPredicate;
    m_gfxCmdBufState.flags.packetPredicate = 0;

    m_device.Barrier(this, &m_deCmdStream, barrierInfo);

    m_gfxCmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdRelease(
    const AcquireReleaseInfo& releaseInfo,
    const IGpuEvent*          pGpuEvent)
{
    CmdBuffer::CmdRelease(releaseInfo, pGpuEvent);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_gfxCmdBufState.flags.packetPredicate;
    m_gfxCmdBufState.flags.packetPredicate = 0;

    m_device.BarrierRelease(this, &m_deCmdStream, releaseInfo, pGpuEvent);

    m_gfxCmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdAcquire(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    gpuEventCount,
    const IGpuEvent*const*    ppGpuEvents)
{
    CmdBuffer::CmdAcquire(acquireInfo, gpuEventCount, ppGpuEvents);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_gfxCmdBufState.flags.packetPredicate;
    m_gfxCmdBufState.flags.packetPredicate = 0;

    m_device.BarrierAcquire(this, &m_deCmdStream, acquireInfo, gpuEventCount, ppGpuEvents);

    m_gfxCmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdReleaseThenAcquire(
    const AcquireReleaseInfo& barrierInfo)
{
    CmdBuffer::CmdReleaseThenAcquire(barrierInfo);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_gfxCmdBufState.flags.packetPredicate;
    m_gfxCmdBufState.flags.packetPredicate = 0;

    m_device.BarrierReleaseThenAcquire(this, &m_deCmdStream, barrierInfo);

    m_gfxCmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetVertexBuffers(
    uint32                firstBuffer,
    uint32                bufferCount,
    const BufferViewInfo* pBuffers)
{
    PAL_ASSERT(bufferCount > 0);
    PAL_ASSERT((firstBuffer + bufferCount) <= MaxVertexBuffers);
    PAL_ASSERT(pBuffers != nullptr);

    // The vertex buffer table will be validated at Draw time, so all that is necessary is to update the CPU-side copy
    // of the SRD table and upload the new SRD data into CE RAM.

    BufferSrd*const pSrds = (m_vbTable.pSrds + firstBuffer);
    m_device.Parent()->CreateUntypedBufferViewSrds(bufferCount, pBuffers, pSrds);

    constexpr uint32 DwordsPerSrd = (sizeof(BufferSrd) / sizeof(uint32));
    if (UseCpuPathInsteadOfCeRam() == false)
    {
        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
        pCeCmdSpace = UploadToUserDataTable(&m_vbTable.state,
                                            (DwordsPerSrd * firstBuffer),
                                            (DwordsPerSrd * bufferCount),
                                            reinterpret_cast<const uint32*>(pSrds),
                                            m_vbTable.watermark,
                                            pCeCmdSpace);
        m_ceCmdStream.CommitCommands(pCeCmdSpace);
    }
    else if ((DwordsPerSrd * firstBuffer) < m_vbTable.watermark)
    {
        // Only mark the contents as dirty if the updated VB table entries fall within the current high watermark.
        // This will help avoid redundant validation for data which the current pipeline doesn't care about.
        m_vbTable.state.dirty = 1;
    }

    m_vbTable.modified = 1;
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 473
// =====================================================================================================================
void UniversalCmdBuffer::CmdSetIndirectUserData(
    uint16      tableId,        // Deprecated, must be zero!
    uint32      dwordOffset,
    uint32      dwordSize,
    const void* pSrcData)
{
    PAL_ASSERT(tableId == 0);
    PAL_ASSERT(dwordSize > 0);
    PAL_ASSERT((dwordOffset + dwordSize) <= m_vbTable.state.sizeInDwords);

    // All this method needs to do is update the CPU-side copy of the indirect user-data table and upload the new
    // data to CE RAM. It will be validated at Draw-time.

    uint32* pDst = (reinterpret_cast<uint32*>(m_vbTable.pSrds) + dwordOffset);
    auto*   pSrc = static_cast<const uint32*>(pSrcData);
    for (uint32 i = 0; i < dwordSize; ++i)
    {
        *pDst = *pSrc;
        ++pDst;
        ++pSrc;
    }

    if (UseCpuPathInsteadOfCeRam() == false)
    {
        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
        pCeCmdSpace = UploadToUserDataTable(&m_vbTable.state,
                                            dwordOffset,
                                            dwordSize,
                                            static_cast<const uint32*>(pSrcData),
                                            m_vbTable.watermark,
                                            pCeCmdSpace);
        m_ceCmdStream.CommitCommands(pCeCmdSpace);
    }
    else if (dwordOffset < m_vbTable.watermark)
    {
        // Only mark the contents as dirty if the updated VB table entries fall within the current high watermark.
        // This will help avoid redundant validation for data which the current pipeline doesn't care about.
        m_vbTable.state.dirty = 1;
    }

    m_vbTable.modified = 1;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetIndirectUserDataWatermark(
    uint16 tableId,     // Deprecated, must be zero!
    uint32 dwordLimit)
{
    PAL_ASSERT(tableId == 0);

    dwordLimit = Min(dwordLimit, m_vbTable.state.sizeInDwords);
    if (dwordLimit > m_vbTable.watermark)
    {
        // If the current high watermark is increasing, we need to mark the contents as dirty because data which was
        // previously uploaded to CE RAM wouldn't have been dumped to GPU memory before the previous Draw.
        m_vbTable.state.dirty = 1;
    }

    m_vbTable.watermark = dwordLimit;
}
#endif

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

    uint32 bppMoreThan64 = 0;
    TargetExtent2d surfaceExtent = { MaxScissorExtent, MaxScissorExtent }; // Default to fully open

    // Bind all color targets.
    const uint32 colorTargetLimit   = Max(params.colorTargetCount, m_graphicsState.bindTargets.colorTargetCount);
    uint32       newColorTargetMask = 0;
    for (uint32 slot = 0; slot < colorTargetLimit; slot++)
    {
        const auto*const pCurrentView =
            static_cast<const ColorTargetView*>(m_graphicsState.bindTargets.colorTargets[slot].pColorTargetView);
        const auto*const pNewView = (slot < params.colorTargetCount)
                                    ? static_cast<const ColorTargetView*>(params.colorTargets[slot].pColorTargetView)
                                    : nullptr;
        bool             validViewFound = false;

        if (pNewView != nullptr)
        {
            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
            pDeCmdSpace = pNewView->WriteCommands(slot,
                                                  params.colorTargets[slot].imageLayout,
                                                  &m_deCmdStream,
                                                  pDeCmdSpace);
            m_deCmdStream.CommitCommands(pDeCmdSpace);

            if (validViewFound == false)
            {
                // For MRT case, extents must match across all MRTs.
                surfaceExtent = pNewView->GetExtent();
            }

            pNewView->UpdateDccStateMetadata(&m_deCmdStream, params.colorTargets[slot].imageLayout);

            // Set the bit means this color target slot is not bound to a NULL target.
            newColorTargetMask |= (1 << slot);

            validViewFound = true;
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
        pDeCmdSpace = ColorTargetView::HandleBoundTargetsChanged(m_device, pDeCmdSpace);
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

        TargetExtent2d depthViewExtent = pNewDepthView->GetExtent();
        surfaceExtent.width  = Util::Min(surfaceExtent.width,  depthViewExtent.width);
        surfaceExtent.height = Util::Min(surfaceExtent.height, depthViewExtent.height);

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
        pDeCmdSpace = DepthStencilView::HandleBoundTargetChanged(m_cmdUtil, pDeCmdSpace);

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

    if (surfaceExtent.value != m_graphicsState.targetExtent.value)
    {
        // Set scissor owned by the target.
        ScreenScissorReg* pScreenScissors = reinterpret_cast<ScreenScissorReg*>(pDeCmdSpace);
        m_cmdUtil.BuildSetSeqContextRegs(mmPA_SC_SCREEN_SCISSOR_TL,
                                         mmPA_SC_SCREEN_SCISSOR_BR,
                                         pScreenScissors);
        pScreenScissors->paScScreenScissorTl.u32All    = 0;
        pScreenScissors->paScScreenScissorBr.u32All    = 0;
        pScreenScissors->paScScreenScissorBr.bits.BR_X = surfaceExtent.width;
        pScreenScissors->paScScreenScissorBr.bits.BR_Y = surfaceExtent.height;

        pDeCmdSpace += Util::NumBytesToNumDwords(sizeof(ScreenScissorReg));

        m_graphicsState.targetExtent.value = surfaceExtent.value;
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
    const auto&              palDevice = *(m_device.Parent());
    const GpuChipProperties& chipProps = palDevice.ChipProperties();
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

    if (UseCpuPathInsteadOfCeRam())
    {
        // If the stream-out table is being managed by the CPU through embedded-data, just mark it dirty since we
        // need to update the whole table at Draw-time anyway.
        m_streamOut.state.dirty = 1;
    }
    else
    {
        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
        pCeCmdSpace = UploadToUserDataTable(&m_streamOut.state,
                                            0,
                                            (sizeof(m_streamOut.srd) / sizeof(uint32)),
                                            reinterpret_cast<const uint32*>(&m_streamOut.srd[0]),
                                            UINT_MAX,
                                            pCeCmdSpace);
        m_ceCmdStream.CommitCommands(pCeCmdSpace);
    }

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
    else if (static_cast<TossPointMode>(m_cachedSettings.tossPointMode) == TossPointBackFrontFaceCull)
    {
        m_graphicsState.triangleRasterState.cullMode = CullMode::FrontAndBack;

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
    const auto*  pPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const uint32 userData0 = pPipeline->GetVsUserDataBaseOffset();

    // Compute register offsets of first vertex and start instance user data locations relative to
    // user data 0.
    PAL_ASSERT((GetVertexOffsetRegAddr() != 0) && (GetInstanceOffsetRegAddr() != 0));
    PAL_ASSERT(GetVertexOffsetRegAddr() >= userData0);
    PAL_ASSERT(GetInstanceOffsetRegAddr() >= userData0);

    uint32 firstVertexIdx   = GetVertexOffsetRegAddr() - userData0;
    uint32 startInstanceIdx = GetInstanceOffsetRegAddr() - userData0;
    uint32 drawIndexIdx     = UINT_MAX;

    if (m_drawIndexReg != UserDataNotMapped)
    {
        drawIndexIdx = m_drawIndexReg - userData0;
    }

    m_device.DescribeDraw(this, cmdType, firstVertexIdx, startInstanceIdx, drawIndexIdx);
}

// =====================================================================================================================
// Issues a non-indexed draw command. We must discard the draw if vertexCount or instanceCount are zero. To avoid
// branching, we will rely on the HW to discard the draw for us.
template <bool IssueSqttMarkerEvent,
          bool ViewInstancingEnable>
void PAL_STDCALL UniversalCmdBuffer::CmdDraw(
    ICmdBuffer* pCmdBuffer,
    uint32      firstVertex,
    uint32      vertexCount,
    uint32      firstInstance,
    uint32      instanceCount)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    ValidateDrawInfo drawInfo;
    drawInfo.vtxIdxCount   = vertexCount;
    drawInfo.instanceCount = instanceCount;
    drawInfo.firstVertex   = firstVertex;
    drawInfo.firstInstance = firstInstance;
    drawInfo.firstIndex    = 0;
    drawInfo.useOpaque     = false;

    pThis->ValidateDraw<false, false>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (IssueSqttMarkerEvent)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDraw);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);

    if (ViewInstancingEnable)
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
                pDeCmdSpace += CmdUtil::BuildDrawIndexAuto(vertexCount,
                                                           false,
                                                           pThis->PacketPredicate(),
                                                           pDeCmdSpace);
            }
        }
    }
    else
    {
        pDeCmdSpace += CmdUtil::BuildDrawIndexAuto(vertexCount, false, pThis->PacketPredicate(), pDeCmdSpace);
    }

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeUniversal, pDeCmdSpace);
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
// Issues a draw opaque command.
template <bool IssueSqttMarkerEvent,
          bool ViewInstancingEnable>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawOpaque(
    ICmdBuffer* pCmdBuffer,
    gpusize     streamOutFilledSizeVa,
    uint32      streamOutOffset,
    uint32      stride,
    uint32      firstInstance,
    uint32      instanceCount)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    ValidateDrawInfo drawInfo;
    drawInfo.vtxIdxCount   = 0;
    drawInfo.instanceCount = instanceCount;
    drawInfo.firstVertex   = 0;
    drawInfo.firstInstance = firstInstance;
    drawInfo.firstIndex    = 0;
    drawInfo.useOpaque     = true;

    pThis->ValidateDraw<false, false>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (IssueSqttMarkerEvent)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawOpaque);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    // Streamout filled is saved in gpuMemory, we use a me_copy to set mmVGT_STRMOUT_DRAW_OPAQUE_BUFFER_FILLED_SIZE.
    pDeCmdSpace += pThis->m_cmdUtil.BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
                                                          dst_sel__me_copy_data__mem_mapped_register,
                                                          mmVGT_STRMOUT_DRAW_OPAQUE_BUFFER_FILLED_SIZE,
                                                          src_sel__me_copy_data__memory__GFX09,
                                                          streamOutFilledSizeVa,
                                                          count_sel__me_copy_data__32_bits_of_data,
                                                          wr_confirm__me_copy_data__wait_for_confirmation,
                                                          pDeCmdSpace);

    // For now, this method is only invoked by DXXP and Vulkan clients, they both prefer to use the size/offset in
    // bytes.
    // Hardware will calc to indices by (mmVGT_STRMOUT_DRAW_OPAQUE_BUFFER_FILLED_SIZE -
    // mmVGT_STRMOUT_DRAW_OPAQUE_OFFSET) / mmVGT_STRMOUT_DRAW_OPAQUE_VERTEX_STRIDE
    pDeCmdSpace = pThis->m_deCmdStream.WriteSetOneContextReg(mmVGT_STRMOUT_DRAW_OPAQUE_OFFSET,
                                                             streamOutOffset,
                                                             pDeCmdSpace);
    pDeCmdSpace = pThis->m_deCmdStream.WriteSetOneContextReg(mmVGT_STRMOUT_DRAW_OPAQUE_VERTEX_STRIDE,
                                                             stride,
                                                             pDeCmdSpace);

    pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);

    if (ViewInstancingEnable)
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
                pDeCmdSpace += CmdUtil::BuildDrawIndexAuto(0, true, pThis->PacketPredicate(), pDeCmdSpace);
            }
        }
    }
    else
    {
        pDeCmdSpace += CmdUtil::BuildDrawIndexAuto(0, true, pThis->PacketPredicate(), pDeCmdSpace);
    }

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeUniversal, pDeCmdSpace);
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
template <bool IssueSqttMarkerEvent,
          bool IsNggFastLaunch,
          bool ViewInstancingEnable>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawIndexed(
    ICmdBuffer* pCmdBuffer,
    uint32      firstIndex,
    uint32      indexCount,
    int32       vertexOffset,
    uint32      firstInstance,
    uint32      instanceCount)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    PAL_ASSERT(firstIndex <= pThis->m_graphicsState.iaState.indexCount);

    ValidateDrawInfo drawInfo;
    drawInfo.vtxIdxCount   = indexCount;
    drawInfo.instanceCount = instanceCount;
    drawInfo.firstVertex   = vertexOffset;
    drawInfo.firstInstance = firstInstance;
    drawInfo.firstIndex    = firstIndex;
    drawInfo.useOpaque     = false;

    pThis->ValidateDraw<true, false>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (IssueSqttMarkerEvent)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawIndexed);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    const uint32 validIndexCount = pThis->m_graphicsState.iaState.indexCount - firstIndex;

    pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);

    if (ViewInstancingEnable)
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

                if (IsNggFastLaunch == false)
                {
                    if (pThis->IsNested() && (pThis->m_graphicsState.iaState.indexAddr == 0))
                    {
                        // If IB state is not bound, nested command buffers must use DRAW_INDEX_OFFSET_2 so that
                        // we can inherit th IB base and size from direct command buffer
                        pDeCmdSpace += CmdUtil::BuildDrawIndexOffset2(indexCount,
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

                        pDeCmdSpace += CmdUtil::BuildDrawIndex2(indexCount,
                                                                validIndexCount,
                                                                gpuVirtAddr,
                                                                pThis->PacketPredicate(),
                                                                pDeCmdSpace);
                    }
                }
                else
                {
                    // NGG Fast Launch pipelines treat all draws as auto-index draws.
                    pDeCmdSpace += CmdUtil::BuildDrawIndexAuto(indexCount,
                                                               false,
                                                               pThis->PacketPredicate(),
                                                               pDeCmdSpace);
                }
            }
        }
    }
    else
    {
        if (IsNggFastLaunch == false)
        {
            if (pThis->IsNested() && (pThis->m_graphicsState.iaState.indexAddr == 0))
            {
                // If IB state is not bound, nested command buffers must use DRAW_INDEX_OFFSET_2 so that
                // we can inherit th IB base and size from direct command buffer
                pDeCmdSpace += CmdUtil::BuildDrawIndexOffset2(indexCount,
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

                pDeCmdSpace += CmdUtil::BuildDrawIndex2(indexCount,
                                                        validIndexCount,
                                                        gpuVirtAddr,
                                                        pThis->PacketPredicate(),
                                                        pDeCmdSpace);
            }
        }
        else
        {
            // NGG Fast Launch pipelines treat all draws as auto-index draws.
            pDeCmdSpace += CmdUtil::BuildDrawIndexAuto(indexCount,
                                                       false,
                                                       pThis->PacketPredicate(),
                                                       pDeCmdSpace);
        }
    }

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeUniversal, pDeCmdSpace);
    }

    pDeCmdSpace  = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Issues an indirect non-indexed draw command. We must discard the draw if vertexCount or instanceCount are zero.
// We will rely on the HW to discard the draw for us.
template <bool IssueSqttMarkerEvent, bool ViewInstancingEnable>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawIndirectMulti(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)) && IsPow2Aligned(countGpuAddr, sizeof(uint32)));
    PAL_ASSERT(offset + (sizeof(DrawIndirectArgs) * maximumCount) <= gpuMemory.Desc().size);

    ValidateDrawInfo drawInfo;
    drawInfo.vtxIdxCount   = 0;
    drawInfo.instanceCount = 0;
    drawInfo.firstVertex   = 0;
    drawInfo.firstInstance = 0;
    drawInfo.firstIndex    = 0;
    drawInfo.useOpaque     = false;

    pThis->ValidateDraw<false, true>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (IssueSqttMarkerEvent)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawIndirectMulti);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace += CmdUtil::BuildSetBase(gpuMemory.Desc().gpuVirtAddr,
                                         base_index__pfp_set_base__patch_table_base,
                                         ShaderGraphics,
                                         pDeCmdSpace);

    const uint16 vtxOffsetReg  = pThis->GetVertexOffsetRegAddr();
    const uint16 instOffsetReg = pThis->GetInstanceOffsetRegAddr();

    pThis->m_deCmdStream.NotifyIndirectShRegWrite(vtxOffsetReg);
    pThis->m_deCmdStream.NotifyIndirectShRegWrite(instOffsetReg);

    pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);

    if (ViewInstancingEnable)
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
                pDeCmdSpace += CmdUtil::BuildDrawIndirectMulti(offset,
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
        pDeCmdSpace += CmdUtil::BuildDrawIndirectMulti(offset,
                                                       vtxOffsetReg,
                                                       instOffsetReg,
                                                       pThis->m_drawIndexReg,
                                                       stride,
                                                       maximumCount,
                                                       countGpuAddr,
                                                       pThis->PacketPredicate(),
                                                       pDeCmdSpace);
    }

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeUniversal, pDeCmdSpace);
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
template <bool IssueSqttMarkerEvent, bool IsNggFastLaunch, bool ViewInstancingEnable>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawIndexedIndirectMulti(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)) && IsPow2Aligned(countGpuAddr, sizeof(uint32)));
    PAL_ASSERT(offset + (sizeof(DrawIndexedIndirectArgs) * maximumCount) <= gpuMemory.Desc().size);

    ValidateDrawInfo drawInfo;
    drawInfo.vtxIdxCount   = 0;
    drawInfo.instanceCount = 0;
    drawInfo.firstVertex   = 0;
    drawInfo.firstInstance = 0;
    drawInfo.firstIndex    = 0;
    drawInfo.useOpaque     = false;

    pThis->ValidateDraw<true, true>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (IssueSqttMarkerEvent)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawIndexedIndirectMulti);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace += CmdUtil::BuildSetBase(gpuMemory.Desc().gpuVirtAddr,
                                         base_index__pfp_set_base__patch_table_base,
                                         ShaderGraphics,
                                         pDeCmdSpace);

    const uint16 vtxOffsetReg   = pThis->GetVertexOffsetRegAddr();
    const uint16 instOffsetReg  = pThis->GetInstanceOffsetRegAddr();
    const uint16 indexOffsetReg = pThis->GetStartIndexRegAddr();

    pThis->m_deCmdStream.NotifyIndirectShRegWrite(vtxOffsetReg);
    pThis->m_deCmdStream.NotifyIndirectShRegWrite(instOffsetReg);

    if (IsNggFastLaunch)
    {
        pThis->m_deCmdStream.NotifyIndirectShRegWrite(indexOffsetReg);
    }

    pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);

    if (ViewInstancingEnable)
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
                    if ((IsNggFastLaunch == false) ||
                        (pParentDev->EngineProperties().cpUcodeVersion > UcodeVersionNggIndexedIndirectDraw))
                    {
                        pDeCmdSpace += CmdUtil::BuildDrawIndexIndirectMulti(offset,
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
            if ((IsNggFastLaunch == false) ||
                (pThis->m_device.Parent()->EngineProperties().cpUcodeVersion > UcodeVersionNggIndexedIndirectDraw))
            {
                pDeCmdSpace += CmdUtil::BuildDrawIndexIndirectMulti(offset,
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

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeUniversal, pDeCmdSpace);
    }

    pDeCmdSpace = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);

    pThis->m_state.flags.containsDrawIndirect = 1;
}

// =====================================================================================================================
// Issues a direct dispatch command. We must discard the dispatch if x, y, or z are zero. To avoid branching, we will
// rely on the HW to discard the dispatch for us.
template <bool IssueSqttMarkerEvent, bool UseCpuPathForUserDataTables>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatch(
    ICmdBuffer* pCmdBuffer,
    uint32      x,
    uint32      y,
    uint32      z)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    if (IssueSqttMarkerEvent)
    {
        pThis->m_device.DescribeDispatch(pThis, Developer::DrawDispatchType::CmdDispatch, 0, 0, 0, x, y, z);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace  = pThis->ValidateDispatch<UseCpuPathForUserDataTables>(0uLL, x, y, z, pDeCmdSpace);
    pDeCmdSpace  = pThis->WaitOnCeCounter(pDeCmdSpace);

    pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect<false, true>(x, y, z,
                                                                     pThis->PacketPredicate(),
                                                                     pDeCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeUniversal, pDeCmdSpace);
    }

    pDeCmdSpace = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Issues an indirect dispatch command. We must discard the dispatch if x, y, or z are zero. We will rely on the HW to
// discard the dispatch for us.
template <bool IssueSqttMarkerEvent, bool UseCpuPathForUserDataTables>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchIndirect(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    if (IssueSqttMarkerEvent)
    {
        pThis->m_device.DescribeDispatch(pThis, Developer::DrawDispatchType::CmdDispatchIndirect, 0, 0, 0, 0, 0, 0);
    }

    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)));
    PAL_ASSERT(offset + sizeof(DispatchIndirectArgs) <= gpuMemory.Desc().size);

    const gpusize gpuMemBaseAddr = gpuMemory.Desc().gpuVirtAddr;

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace  = pThis->ValidateDispatch<UseCpuPathForUserDataTables>((gpuMemBaseAddr + offset), 0, 0, 0, pDeCmdSpace);
    pDeCmdSpace  = pThis->WaitOnCeCounter(pDeCmdSpace);
    pDeCmdSpace += CmdUtil::BuildSetBase(gpuMemBaseAddr,
                                         base_index__pfp_set_base__patch_table_base,
                                         ShaderCompute,
                                         pDeCmdSpace);
    pDeCmdSpace += CmdUtil::BuildDispatchIndirectGfx(offset,
                                                     pThis->PacketPredicate(),
                                                     pDeCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeUniversal, pDeCmdSpace);
    }

    pDeCmdSpace = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);

    pThis->m_state.flags.containsDrawIndirect = 1;
}

// =====================================================================================================================
// Issues a direct dispatch command with immediate threadgroup offsets. We must discard the dispatch if x, y, or z are
// zero. To avoid branching, we will rely on the HW to discard the dispatch for us.
template <bool IssueSqttMarkerEvent, bool UseCpuPathForUserDataTables>
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

    if (IssueSqttMarkerEvent)
    {
        pThis->m_device.DescribeDispatch(pThis, Developer::DrawDispatchType::CmdDispatchOffset,
            xOffset, yOffset, zOffset, xDim, yDim, zDim);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace = pThis->ValidateDispatch<UseCpuPathForUserDataTables>(0uLL, xDim, yDim, zDim, pDeCmdSpace);

    const uint32 starts[3] = {xOffset, yOffset, zOffset};
    pDeCmdSpace = pThis->m_deCmdStream.WriteSetSeqShRegs(mmCOMPUTE_START_X,
                                                         mmCOMPUTE_START_Z,
                                                         ShaderCompute,
                                                         starts,
                                                         pDeCmdSpace);
    // xDim, yDim, zDim are end positions instead of numbers of threadgroups to execute.
    xDim += xOffset;
    yDim += yOffset;
    zDim += zOffset;

    pDeCmdSpace  = pThis->WaitOnCeCounter(pDeCmdSpace);
    pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect<false, false>(xDim,
                                                                      yDim,
                                                                      zDim,
                                                                      pThis->PacketPredicate(),
                                                                      pDeCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeUniversal, pDeCmdSpace);
    }

    pDeCmdSpace = pThis->IncrementDeCounter(pDeCmdSpace);

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
    gpusize           offset,
    uint32            value)
{
    const GpuMemory* pGpuMemory = static_cast<const GpuMemory*>(&dstGpuMemory);
    WriteDataInfo    writeData  = {};

    writeData.engineType = GetEngineType();
    writeData.dstAddr    = pGpuMemory->GetBusAddrMarkerVa() + offset;
    writeData.engineSel  = engine_sel__me_write_data__micro_engine;
    writeData.dstSel     = dst_sel__me_write_data__memory;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace += m_cmdUtil.BuildWriteData(writeData, value, pDeCmdSpace);
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
    const uint32 userDataAddr =
        (markerType == PerfTraceMarkerType::A) ? mmSQ_THREAD_TRACE_USERDATA_2 : mmSQ_THREAD_TRACE_USERDATA_3;

    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
    {
        pCmdSpace = m_deCmdStream.WriteSetOneConfigReg<false>(userDataAddr, markerData, pCmdSpace);
    }
    m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdInsertRgpTraceMarker(
    uint32      numDwords,
    const void* pData)
{
    // The first dword of every RGP trace marker packet is written to SQ_THREAD_TRACE_USERDATA_2.  The second dword
    // is written to SQ_THREAD_TRACE_USERDATA_3.  For packets longer than 64-bits, continue alternating between
    // user data 2 and 3.
    static_assert(mmSQ_THREAD_TRACE_USERDATA_3 == mmSQ_THREAD_TRACE_USERDATA_2 + 1, "Registers not sequential!");

    const uint32* pDwordData = static_cast<const uint32*>(pData);
    while (numDwords > 0)
    {
        const uint32 dwordsToWrite = Min(numDwords, 2u);

        // Reserve and commit command space inside this loop.  Some of the RGP packets are unbounded, like adding a
        // comment string, so it's not safe to assume the whole packet will fit under our reserve limit.
        uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
        {
            pCmdSpace = m_deCmdStream.WriteSetSeqConfigRegs<false>(mmSQ_THREAD_TRACE_USERDATA_2,
                                                                   mmSQ_THREAD_TRACE_USERDATA_2 + dwordsToWrite - 1,
                                                                   pDwordData,
                                                                   pCmdSpace);
        }
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
    size_t cmdDwords = (m_cmdUtil.BuildSetSeqContextRegs(mmDB_RENDER_OVERRIDE2,
                                                         mmDB_HTILE_DATA_BASE,
                                                         &pm4Commands.hdrDbRenderOverride2) +
                        m_cmdUtil.BuildSetOneContextReg(mmDB_RENDER_CONTROL, &pm4Commands.hdrDbRenderControl));

    pm4Commands.dbHtileDataBase.u32All   = 0;
    pm4Commands.dbRenderOverride2.u32All = 0;

    // If the dbRenderControl.DEPTH_CLEAR_ENABLE bit is not reset to 0 after performing a graphics fast depth clear
    // then any following draw call with pixel shader z-imports will have their z components clamped to the clear
    // plane equation which was set in the fast clear.
    //
    //     [dbRenderControl.]DEPTH_CLEAR_ENABLE will modify the zplane of the incoming geometry to the clear plane.
    //     So if the shader uses this z plane (that is, z-imports are enabled), this can affect the color output.
    pm4Commands.dbRenderControl.u32All = 0;

    // The rest of the PM4 commands depend on which GFXIP version we are.
    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        cmdDwords += m_cmdUtil.BuildSetSeqContextRegs(Gfx09::mmDB_Z_INFO,
                                                      Gfx09::mmDB_DFSM_CONTROL,
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

    if (cmdDwords != 0)
    {
        pCmdSpace = m_deCmdStream.WritePm4Image(cmdDwords, &pm4Commands[0], pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Returns the HW enumeration needed for the PA_SC_BINNER_CNTL_0.BINNING_MDOE field when the binning needs to be
// disabled.
// Also returns bin size as non-zero if required to be set under the disabled mode.
BinningMode UniversalCmdBuffer::GetDisableBinningSetting(
    Extent2d* pBinSize
    ) const
{
    BinningMode  binningMode = DISABLE_BINNING_USE_LEGACY_SC;

    return binningMode;
}

// =====================================================================================================================
// Adds a preamble to the start of a new command buffer.
Result UniversalCmdBuffer::AddPreamble()
{
    const auto& device   = *(m_device.Parent());
    const bool  isNested = IsNested();

    // If this trips, it means that this isn't really the preamble -- i.e., somebody has inserted something into the
    // command stream before the preamble.  :-(
    PAL_ASSERT(m_ceCmdStream.IsEmpty());
    PAL_ASSERT(m_deCmdStream.IsEmpty());

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(PIPELINESTAT_START, EngineTypeUniversal, pDeCmdSpace);

    if ((device.GetPlatform()->IsEmulationEnabled()) && (isNested == false))
    {
        PAL_ASSERT(device.IsPreemptionSupported(EngineType::EngineTypeUniversal) == false);

        PM4PFP_CONTEXT_CONTROL contextControl = {};

        contextControl.bitfields2.update_load_enables    = 1;
        contextControl.bitfields2.load_per_context_state = 1;
        contextControl.bitfields3.update_shadow_enables  = 1;

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

    // Clear out the blend optimizations explicitly here as the chained command buffers don't have a way to check
    // inherited state and the optimizations won't be cleared unless cleared in this command buffer.
    BlendOpt dontRdDst    = FORCE_OPT_AUTO;
    BlendOpt discardPixel = FORCE_OPT_AUTO;

    if (m_cachedSettings.blendOptimizationsEnable == false)
    {
        dontRdDst    = FORCE_OPT_DISABLE;
        discardPixel = FORCE_OPT_DISABLE;
    }

    for (uint32 idx = 0; idx < MaxColorTargets; idx++)
    {
        constexpr uint32 BlendOptRegMask = (CB_COLOR0_INFO__BLEND_OPT_DONT_RD_DST_MASK |
                                            CB_COLOR0_INFO__BLEND_OPT_DISCARD_PIXEL_MASK);

        regCB_COLOR0_INFO regValue            = {};
        regValue.bits.BLEND_OPT_DONT_RD_DST   = dontRdDst;
        regValue.bits.BLEND_OPT_DISCARD_PIXEL = discardPixel;

        if (m_deCmdStream.Pm4ImmediateOptimizerEnabled())
        {
            pDeCmdSpace = m_deCmdStream.WriteContextRegRmw<true>(mmCB_COLOR0_INFO + idx * CbRegsPerSlot,
                                                                 BlendOptRegMask,
                                                                 regValue.u32All,
                                                                 pDeCmdSpace);
        }
        else
        {
            pDeCmdSpace = m_deCmdStream.WriteContextRegRmw<false>(mmCB_COLOR0_INFO + idx * CbRegsPerSlot,
                                                                  BlendOptRegMask,
                                                                  regValue.u32All,
                                                                  pDeCmdSpace);
        }
    }

    const uint32  mmPaStateStereoX = m_cmdUtil.GetRegInfo().mmPaStateStereoX;
    if (mmPaStateStereoX != 0)
    {
        {
            pDeCmdSpace = m_deCmdStream.WriteSetOneConfigReg(mmPaStateStereoX, 0, pDeCmdSpace);
        }
    }

    // PA_SC_CONSERVATIVE_RASTERIZATION_CNTL is the same value for most Pipeline objects. Prime it in the Preamble
    // to the disabled state. At draw-time, we check if a new value is needed based on (Pipeline || MSAA) being dirty.
    // It is expected that Pipeline and MSAA is always known even on nested command buffers.
    pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmPA_SC_CONSERVATIVE_RASTERIZATION_CNTL,
                                                           m_paScConsRastCntl.u32All,
                                                           pDeCmdSpace);

    // With the PM4 optimizer enabled, certain registers are only updated via RMW packets and not having an initial
    // value causes the optimizer to skip optimizing redundant RMW packets.
    if (m_deCmdStream.Pm4OptimizerEnabled())
    {
        if (isNested == false)
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
    }

    // Disable PBB at the start of each command buffer unconditionally. Each draw can set the appropriate
    // PBB state at validate time.
    m_enabledPbb = false;
    m_paScBinnerCntl0.u32All = 0;
    Extent2d binSize = {};
    m_paScBinnerCntl0.bits.BINNING_MODE = GetDisableBinningSetting(&binSize);
    if ((binSize.width != 0) && (binSize.height != 0))
    {
        if (binSize.width == 16)
        {
            m_paScBinnerCntl0.bits.BIN_SIZE_X = 1;
        }
        else
        {
            m_paScBinnerCntl0.bits.BIN_SIZE_X_EXTEND = Device::GetBinSizeEnum(binSize.width);
        }

        if (binSize.height == 16)
        {
            m_paScBinnerCntl0.bits.BIN_SIZE_Y = 1;
        }
        else
        {
            m_paScBinnerCntl0.bits.BIN_SIZE_Y_EXTEND = Device::GetBinSizeEnum(binSize.height);
        }
    }
    m_paScBinnerCntl0.bits.DISABLE_START_OF_PRIM = (m_cachedSettings.disableDfsm) ? 1 : 0;
    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmPA_SC_BINNER_CNTL_0, m_paScBinnerCntl0.u32All, pDeCmdSpace);

    if (isNested == false)
    {
        // Initialize screen scissor value.
        ScreenScissorReg* pScreenScissors = reinterpret_cast<ScreenScissorReg*>(pDeCmdSpace);
        m_cmdUtil.BuildSetSeqContextRegs(mmPA_SC_SCREEN_SCISSOR_TL,
                                         mmPA_SC_SCREEN_SCISSOR_BR,
                                         pScreenScissors);
        pScreenScissors->paScScreenScissorTl.u32All    = 0;
        pScreenScissors->paScScreenScissorBr.u32All    = 0;
        pScreenScissors->paScScreenScissorBr.bits.BR_X = m_graphicsState.targetExtent.width;
        pScreenScissors->paScScreenScissorBr.bits.BR_Y = m_graphicsState.targetExtent.height;

        pDeCmdSpace += Util::NumBytesToNumDwords(sizeof(ScreenScissorReg));
    }

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

    return Result::Success;
}

// =====================================================================================================================
// Adds a postamble to the end of a new command buffer.
Result UniversalCmdBuffer::AddPostamble()
{

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    if (m_gfxCmdBufState.flags.cpBltActive)
    {
        // Stalls the CP ME until the CP's DMA engine has finished all previous "CP blts" (DMA_DATA commands
        // without the sync bit set). The ring won't wait for CP DMAs to finish so we need to do this manually.
        pDeCmdSpace += m_cmdUtil.BuildWaitDmaData(pDeCmdSpace);
        SetGfxCmdBufCpBltState(false);
    }

    if ((m_ceCmdStream.GetNumChunks() > 0) &&
        (m_ceCmdStream.GetFirstChunk()->BusyTrackerGpuAddr() != 0))
    {
        // The timestamps used for reclaiming command stream chunks are written when the DE stream has completed.
        // This ensures the CE stream completes before the DE stream completes, so that the timestamp can't return
        // before CE work is complete.
        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
        pCeCmdSpace += m_cmdUtil.BuildIncrementCeCounter(pCeCmdSpace);
        m_ceCmdStream.CommitCommands(pCeCmdSpace);

        pDeCmdSpace += m_cmdUtil.BuildWaitOnCeCounter(false, pDeCmdSpace);
        pDeCmdSpace += m_cmdUtil.BuildIncrementDeCounter(pDeCmdSpace);

        // The following ATOMIC_MEM packet increments the done-count for the CE command stream, so that we can probe
        // when the command buffer has completed execution on the GPU.
        // NOTE: Normally, we would need to flush the L2 cache to guarantee that this memory operation makes it out to
        // memory. However, since we're at the end of the command buffer, we can rely on the fact that the KMD inserts
        // an EOP event which flushes and invalidates the caches in between command buffers.
        pDeCmdSpace += m_cmdUtil.BuildAtomicMem(AtomicOp::AddInt32,
                                                m_ceCmdStream.GetFirstChunk()->BusyTrackerGpuAddr(),
                                                1,
                                                pDeCmdSpace);
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

    if ((pipePoint >= HwPipePostBlt) && (m_gfxCmdBufState.flags.cpBltActive))
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

    // Prepare packet build info structs.
    WriteDataInfo writeData = {};
    writeData.engineType = engineType;
    writeData.dstAddr    = boundMemObj.GpuVirtAddr();
    writeData.dstSel     = dst_sel__me_write_data__memory;

    ReleaseMemInfo releaseInfo = {};
    releaseInfo.engineType     = engineType;
    releaseInfo.tcCacheOp      = TcCacheOp::Nop;
    releaseInfo.dstAddr        = boundMemObj.GpuVirtAddr();
    releaseInfo.dataSel        = data_sel__me_release_mem__send_32_bit_low;
    releaseInfo.data           = data;

    switch (pipePoint)
    {
    case HwPipeTop:
        // Implement set/reset event with a WRITE_DATA command using PFP engine.
        writeData.engineSel = engine_sel__pfp_write_data__prefetch_parser;

        pDeCmdSpace += m_cmdUtil.BuildWriteData(writeData, data, pDeCmdSpace);
        break;

    case HwPipePostIndexFetch:
        // Implement set/reset event with a WRITE_DATA command using the ME engine.
        writeData.engineSel = engine_sel__me_write_data__micro_engine;

        pDeCmdSpace += m_cmdUtil.BuildWriteData(writeData, data, pDeCmdSpace);
        break;

    case HwPipePostCs:
        // If this trips, expect a hang.
        PAL_ASSERT(IsComputeSupported());
        // break intentionally left out!

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

    // Set remaining (unused) event slots as early as possible. GFX9 and above may have supportReleaseAcquireInterface=1
    // which enables multiple slots (one dword per slot) for a GpuEvent. If the interface is not enabled, PAL client can
    // still treat the GpuEvent as one dword, but PAL needs to handle the unused extra dwords internally by setting it
    // as early in the pipeline as possible.
    const uint32 numEventSlots = m_device.Parent()->ChipProperties().gfxip.numSlotsPerEvent;

    for (uint32 i = 1; i < numEventSlots; i++)
    {
        // Implement set/reset event with a WRITE_DATA command using the CP.
        writeData.dstAddr = boundMemObj.GpuVirtAddr() + (i * sizeof(uint32));

        pDeCmdSpace += m_cmdUtil.BuildWriteData(writeData, data, pDeCmdSpace);
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
// Helper function to instruct the DE to wait on the CE counter at draw or dispatch time if a CE RAM dump was performed
// prior to the draw or dispatch operation or during validation.
uint32* UniversalCmdBuffer::WaitOnCeCounter(
    uint32* pDeCmdSpace)
{
    if (m_state.pLastDumpCeRam != nullptr)
    {
        auto*const pDumpCeRam = reinterpret_cast<PM4_CE_DUMP_CONST_RAM*>(m_state.pLastDumpCeRam);
        pDumpCeRam->ordinal2  = m_state.lastDumpCeRamOrdinal2.u32All;

        pDeCmdSpace += m_cmdUtil.BuildWaitOnCeCounter((m_state.flags.ceInvalidateKcache != 0), pDeCmdSpace);

        m_state.flags.ceInvalidateKcache = 0;
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Helper function to increment the DE counter.
uint32* UniversalCmdBuffer::IncrementDeCounter(
    uint32* pDeCmdSpace)
{
    if (m_state.pLastDumpCeRam != nullptr)
    {
        pDeCmdSpace += m_cmdUtil.BuildIncrementDeCounter(pDeCmdSpace);

        m_state.pLastDumpCeRam = nullptr;
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Helper function for relocating a user-data table
template <uint32 AlignmentInDwords>
void UniversalCmdBuffer::RelocateUserDataTable(
    UserDataTableState* pTable,
    uint32              offsetInDwords, // Offset into the table where the GPU will actually read from
    uint32              dwordsNeeded)
{
    PAL_ASSERT((dwordsNeeded + offsetInDwords) <= pTable->sizeInDwords);

    // CE RAM dumps go straight through to the L2, but the shaders which will access these data read through the L1
    // and Kcache.  In order to prevent false-sharing between CE RAM dumps for consecutive draws, we need to either
    // invalidate the Kcache before each draw (awful!) or just make sure our dumps are at least cacheline-aligned.
    static_assert((AlignmentInDwords & (CacheLineDwords - 1)) == 0,
                  "Alignment for CE RAM tables must be cacheline-aligned to prevent false-sharing between draws!");

    const uint32 offsetInBytes = (sizeof(uint32) * offsetInDwords);
    pTable->gpuVirtAddr = (AllocateGpuScratchMem(dwordsNeeded, AlignmentInDwords) - offsetInBytes);
}

// =====================================================================================================================
// Helper function to upload the contents of a user-data table which is being managed by CE RAM. It is an error to call
// this before the table has been relocated to its new embedded data location!
uint32* UniversalCmdBuffer::UploadToUserDataTable(
    UserDataTableState* pTable,
    uint32              offsetInDwords,
    uint32              dwordsNeeded,
    const uint32*       pSrcData,
    uint32              highWatermark,
    uint32*             pCeCmdSpace)
{
    PAL_ASSERT((dwordsNeeded + offsetInDwords) <= pTable->sizeInDwords);

    pCeCmdSpace += m_cmdUtil.BuildWriteConstRam(pSrcData,
                                                (pTable->ceRamOffset + (sizeof(uint32) * offsetInDwords)),
                                                dwordsNeeded,
                                                pCeCmdSpace);

    if (offsetInDwords < highWatermark)
    {
        // CE RAM now has a more up-to-date copy of the ring data than the GPU memory buffer does, so mark that the
        // data needs to be dumped into ring memory prior to the next Draw or Dispatch, provided that some portion of
        // the upload falls within the high watermark.
        pTable->dirty = 1;
    }

    return pCeCmdSpace;
}

// =====================================================================================================================
// Helper function to dump the contents of a user-data table which is being managed by CE RAM. The constant engine will
// be used to dump the table contents into GPU memory. It is an error to call this before the table has been relocated
// to its new GPU memory location!
uint32* UniversalCmdBuffer::DumpUserDataTable(
    UserDataTableState* pTable,
    uint32              offsetInDwords,
    uint32              dwordsNeeded,
    uint32*             pCeCmdSpace)
{
    PAL_ASSERT((dwordsNeeded + offsetInDwords) <= pTable->sizeInDwords);

    if (m_state.flags.ceWaitOnDeCounterDiff)
    {
        pCeCmdSpace += m_cmdUtil.BuildWaitOnDeCounterDiff(m_state.minCounterDiff, pCeCmdSpace);
        m_state.flags.ceWaitOnDeCounterDiff = 0;
    }

    const uint32 offsetInBytes = (sizeof(uint32) * offsetInDwords);

    // Keep track of the latest DUMP_CONST_RAM packet before the upcoming draw or dispatch.  The last one before the
    // draw or dispatch will be updated to set the increment_ce bit at draw-time.
    m_state.pLastDumpCeRam                    = pCeCmdSpace;
    m_state.lastDumpCeRamOrdinal2.bits.offset = (pTable->ceRamOffset + offsetInBytes);

    pCeCmdSpace += m_cmdUtil.BuildDumpConstRam((pTable->gpuVirtAddr + offsetInBytes),
                                               (pTable->ceRamOffset + offsetInBytes),
                                               dwordsNeeded,
                                               pCeCmdSpace);

    pTable->dirty = 0;

    return pCeCmdSpace;
}

// =====================================================================================================================
// Helper function responsible for handling user-SGPR updates during Draw-time validation when the active pipeline has
// changed since the previous Draw operation.  It is expected that this will be called only when the pipeline is
// changing and immediately before a call to WriteDirtyUserDataEntriesToSgprsGfx().
// Returns a mask of which hardware shader stages' user-data mappings have changed.
template <bool TessEnabled, bool GsEnabled, bool VsEnabled>
uint8 UniversalCmdBuffer::FixupUserSgprsOnPipelineSwitch(
    const GraphicsPipelineSignature* pPrevSignature,
    uint32**                         ppDeCmdSpace)
{
    PAL_ASSERT(pPrevSignature != nullptr);

    // The WriteDirtyUserDataEntriesToSgprsGfx() method only writes entries which are mapped to user-SGPR's and have
    // been marked dirty.  When the active pipeline is changing, the set of entries mapped to user-SGPR's can change
    // per shader stage, and which entries are mapped to which registers can also change.  The simplest way to handle
    // this is to write all mapped user-SGPR's for any stage whose mappings are changing.  Any stage whose mappings
    // are not changing will be handled through the normal "pipeline not changing" path.
    uint8 changedStageMask = 0; // Mask of all stages whose mappings are changing.

    uint32* pDeCmdSpace = (*ppDeCmdSpace);

    if (TessEnabled && (m_pSignatureGfx->userDataHash[HsStageId] != pPrevSignature->userDataHash[HsStageId]))
    {
        changedStageMask |= (1 << HsStageId);
        pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprsGfx<true>(m_pSignatureGfx->stage[HsStageId],
                                                                         m_graphicsState.gfxUserDataEntries,
                                                                         pDeCmdSpace);
    }
    if (GsEnabled && (m_pSignatureGfx->userDataHash[GsStageId] != pPrevSignature->userDataHash[GsStageId]))
    {
        changedStageMask |= (1 << GsStageId);
        pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprsGfx<true>(m_pSignatureGfx->stage[GsStageId],
                                                                         m_graphicsState.gfxUserDataEntries,
                                                                         pDeCmdSpace);
    }
    if (VsEnabled && (m_pSignatureGfx->userDataHash[VsStageId] != pPrevSignature->userDataHash[VsStageId]))
    {
        changedStageMask |= (1 << VsStageId);
        pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprsGfx<true>(m_pSignatureGfx->stage[VsStageId],
                                                                         m_graphicsState.gfxUserDataEntries,
                                                                         pDeCmdSpace);
    }
    if (m_pSignatureGfx->userDataHash[PsStageId] != pPrevSignature->userDataHash[PsStageId])
    {
        changedStageMask |= (1 << PsStageId);
        pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprsGfx<true>(m_pSignatureGfx->stage[PsStageId],
                                                                         m_graphicsState.gfxUserDataEntries,
                                                                         pDeCmdSpace);
    }

    (*ppDeCmdSpace) = pDeCmdSpace;

    return changedStageMask;
}

// =====================================================================================================================
// Helper function responsible for handling spill table updates during Draw- or Dispatch-time validation when the active
// pipeline changed since the previous such operation.  It is expected that this will be called only when the pipeline
// is changing and immediately before a call to WriteDirtyUserDataEntriesToCeRam().
template <typename PipelineSignature>
void UniversalCmdBuffer::FixupSpillTableOnPipelineSwitch(
    const PipelineSignature* pPrevSignature,
    const PipelineSignature* pCurrSignature)
{
    PAL_ASSERT(pPrevSignature != nullptr);

    UserDataEntries*const pEntries = (is_same<PipelineSignature, ComputePipelineSignature>::value)
        ? &m_computeState.csUserDataEntries : &m_graphicsState.gfxUserDataEntries;

    const uint16 currSpillThreshold = pCurrSignature->spillThreshold;
    const uint16 currUserDataLimit  = pCurrSignature->userDataLimit;
    const uint16 prevSpillThreshold = pPrevSignature->spillThreshold;
    const uint16 prevUserDataLimit  = pPrevSignature->userDataLimit;

    // The WriteDirtyUserDataEntriesToCeRam() method only writes spilled user-data entries to CE RAM if they have been
    // marked dirty and if they fall between the active pipeline's spill threshold and user-data limit.  When the
    // active pipeline is changing, the region of entries between the spill threshold and user-data limit may expand,
    // and the entries which didn't fall in the old spill region but fall into the new spill region may not be marked
    // dirty.  In this case, we need to make sure that those entries are written to CE RAM before issuing the Dispatch
    // or Draw.
    if ((currSpillThreshold < prevSpillThreshold) || (currUserDataLimit > prevUserDataLimit))
    {
        // The region of spilled user-data entries can either expand by having the current spill threshold be lower
        // than the previous one, or by having the current user-data limit be higher than the previous one, or both.
        // In both cases, this method will simply mark the entries which are being brought into the spilled region
        // as dirty so that a subsequent call to WriteDirtyUserDataEntriesToCeRam() will write them to CE RAM.
        //
        // The small diagram below illustrates the ways in which the current spill region can expand relative to the
        // previous one.
        //
        // Previous spill region:
        //               [===========================]
        // Possible current spill regions:
        // 1)  [======]  |                           |
        // 2)  [=========|=================]         |
        // 3)  [=========|===========================|=========]
        // 4)            |           [===============|=========]
        // 5)            |                           |  [======]
        //
        // Note that the case where the previous pipeline didn't spill at all is identical to case #1 above because
        // the spill threshold for NoUserDataSpilling is encoded as the maximum uint16 value.

        // This first loop will handle cases #1 and #2 above, as well as the part of case #3 which falls below the
        // previous spill threshold.
        const uint16 firstEntry0 = currSpillThreshold;
        const uint16 entryLimit0 = Min(prevSpillThreshold, currUserDataLimit);
        for (uint16 e = firstEntry0; e < entryLimit0; ++e)
        {
            WideBitfieldSetBit(pEntries->dirty, e);
        }

        // This second loop will handle cases #4 and #5 above, as well as the part of case #3 which falls beyond the
        // previous user-data limit.
        const uint16 firstEntry1 = Max(prevUserDataLimit, currSpillThreshold);
        const uint16 entryLimit1 = currUserDataLimit;
        for (uint16 e = firstEntry1; e < entryLimit1; ++e)
        {
            WideBitfieldSetBit(pEntries->dirty, e);
        }
    } // if the spilled region is expanding
}

// =====================================================================================================================
// Helper function responsible for writing all dirty user-data entries into CE RAM if they are spilled to GPU memory for
// the active pipeline.  Also handles additional updates to CE RAM upon a pipeline change.
template <typename PipelineSignature>
uint32* UniversalCmdBuffer::WriteDirtyUserDataEntriesToCeRam(
    const PipelineSignature* pPrevSignature,
    const PipelineSignature* pCurrSignature,
    uint32*                  pCeCmdSpace)
{
    const UserDataEntries& entries = (is_same<PipelineSignature, ComputePipelineSignature>::value)
        ? m_computeState.csUserDataEntries : m_graphicsState.gfxUserDataEntries;

    UserDataTableState*const pSpillTable = (is_same<PipelineSignature, ComputePipelineSignature>::value)
        ? &m_spillTable.stateCs : &m_spillTable.stateGfx;

    const uint16 spillThreshold = pCurrSignature->spillThreshold;
    const uint16 userDataLimit  = pCurrSignature->userDataLimit;

    uint16 lastEntry = 0;
    uint16 count     = 0;
    for (uint16 e = spillThreshold; e < userDataLimit; ++e)
    {
        while ((e < userDataLimit) && WideBitfieldIsSet(entries.dirty, e))
        {
            PAL_ASSERT((lastEntry == 0) || (lastEntry == (e - 1)));
            lastEntry = e;
            ++count;
            ++e;
        }

        if (count > 0)
        {
            const uint16 firstEntry = (lastEntry - count + 1);
            pCeCmdSpace = UploadToUserDataTable(pSpillTable,
                                                firstEntry,
                                                count,
                                                &entries.entries[firstEntry],
                                                userDataLimit,
                                                pCeCmdSpace);

            // Reset accumulators for the next packet.
            lastEntry = 0;
            count     = 0;
        }
    } // for each entry

    // NOTE: Both spill tables share the same ring buffer, so when one gets updated, the other must also. This is
    // because there may be a large series of Dispatches between Draws (or vice-versa), so if the buffer wraps, it
    // is necessary to make sure that both compute and graphics waves don't clobber each other's spill tables.
    UserDataTableState*const pOtherSpillTable = (is_same<PipelineSignature, ComputePipelineSignature>::value)
        ? &m_spillTable.stateGfx : &m_spillTable.stateCs;
    pOtherSpillTable->dirty |= pSpillTable->dirty;

    return pCeCmdSpace;
}

// =====================================================================================================================
// Helper function responsible for writing all dirty graphics user-data entries to their respective user-SGPR's. Does
// not do anything with entries which are mapped to the spill table.
template <bool TessEnabled, bool GsEnabled, bool VsEnabled>
uint32* UniversalCmdBuffer::WriteDirtyUserDataEntriesToSgprsGfx(
    const GraphicsPipelineSignature* pPrevSignature,
    uint8                            alreadyWrittenStageMask,
    uint32*                          pDeCmdSpace)
{
    const uint8 activeStageMask = ((TessEnabled ? (1 << HsStageId) : 0) |
                                   (GsEnabled   ? (1 << GsStageId) : 0) |
                                   (VsEnabled   ? (1 << VsStageId) : 0) |
                                                  (1 << PsStageId));
    const uint8 dirtyStageMask  = ((~alreadyWrittenStageMask) & activeStageMask);
    if (dirtyStageMask)
    {
        if (TessEnabled && (dirtyStageMask & (1 << HsStageId)))
        {
            pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprsGfx<false>(m_pSignatureGfx->stage[HsStageId],
                                                                              m_graphicsState.gfxUserDataEntries,
                                                                              pDeCmdSpace);
        }
        if (GsEnabled && (dirtyStageMask & (1 << GsStageId)))
        {
            pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprsGfx<false>(m_pSignatureGfx->stage[GsStageId],
                                                                              m_graphicsState.gfxUserDataEntries,
                                                                              pDeCmdSpace);
        }
        if (VsEnabled && (dirtyStageMask & (1 << VsStageId)))
        {
            pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprsGfx<false>(m_pSignatureGfx->stage[VsStageId],
                                                                              m_graphicsState.gfxUserDataEntries,
                                                                              pDeCmdSpace);
        }
        if (dirtyStageMask & (1 << PsStageId))
        {
            pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprsGfx<false>(m_pSignatureGfx->stage[PsStageId],
                                                                              m_graphicsState.gfxUserDataEntries,
                                                                              pDeCmdSpace);
        }
    } // if any stages still need dirty state processing

    return pDeCmdSpace;
}

// =====================================================================================================================
// Helper function responsible for writing all dirty compute user-data entries to their respective user-SGPR's. Does not
// do anything with entries which are mapped to the spill table.
uint32* UniversalCmdBuffer::WriteDirtyUserDataEntriesToUserSgprsCs(
    uint32* pDeCmdSpace)
{
    // Compute pipelines all use a fixed user-data mapping of entries to user-SGPR's, because compute command buffers
    // are not able to use LOAD_SH_REG packets, which are used for inheriting user-data entries in a nested command
    // buffer.  The only way to correctly handle user-data inheritance is by using a fixed mapping.  This has the side
    // effect of allowing us to know that only the first few entries ever need to be written to user-SGPR's, which lets
    // us get away with only checking the first sub-mask of the user-data entries' wide-bitfield of dirty flags.
    static_assert(MaxFastUserDataEntriesCompute <= UserDataEntriesPerMask,
                  "The CS user-data entries mapped to user-SGPR's spans multiple wide-bitfield elements!");
    constexpr uint32 AllFastUserDataEntriesMask = ((1 << MaxFastUserDataEntriesCompute) - 1);
    uint16 userSgprDirtyMask = (m_computeState.csUserDataEntries.dirty[0] & AllFastUserDataEntriesMask);

    // Additionally, dirty compute user-data is always written to user-SGPR's if it could be mapped by a pipeline,
    // which lets us avoid any complex logic when switching pipelines.
    const uint16 baseUserSgpr = m_device.GetFirstUserDataReg(HwShaderStage::Cs);

    for (uint16 e = 0; e < MaxFastUserDataEntriesCompute; ++e)
    {
        const uint16 firstEntry = e;
        uint16       entryCount = 0;

        while ((e < MaxFastUserDataEntriesCompute) && ((userSgprDirtyMask & (1 << e)) != 0))
        {
            ++entryCount;
            ++e;
        }

        if (entryCount > 0)
        {
            const uint16 lastEntry = (firstEntry + entryCount - 1);
            pDeCmdSpace = m_deCmdStream.WriteSetSeqShRegs((baseUserSgpr + firstEntry),
                                                          (baseUserSgpr + lastEntry),
                                                          ShaderCompute,
                                                          &m_computeState.csUserDataEntries.entries[firstEntry],
                                                          pDeCmdSpace);
        }
    } // for each entry

    return pDeCmdSpace;
}

// =====================================================================================================================
// Helper function which is responsible for making sure all user-data entries are written to either the spill table or
// to user-SGPR's, as well as making sure that all indirect user-data tables are up-to-date in GPU memory.  Part of
// Dispatch-time validation.
template <bool HasPipelineChanged, bool TessEnabled, bool GsEnabled, bool VsEnabled>
uint32* UniversalCmdBuffer::ValidateGraphicsUserDataCeRam(
    const GraphicsPipelineSignature* pPrevSignature,
    uint32*                          pDeCmdSpace)
{
    PAL_ASSERT((HasPipelineChanged  && (pPrevSignature != nullptr)) ||
               (!HasPipelineChanged && (pPrevSignature == nullptr)));
    constexpr uint32 StreamOutTableDwords = (sizeof(m_streamOut.srd) / sizeof(uint32));
    constexpr uint16 StreamOutMask        = (1 << 1);
    constexpr uint16 VertexBufMask        = (1 << 0);

    uint16 srdTableDumpMask      = 0; // Mask of which stream-out & VB table require a CE RAM dump.
    uint8 dirtyStreamOutSlotMask = 0; // Mask of which stream-out slots need their buffer strides updated in CE RAM.

    // Step #1:
    // If the stream-out table or vertex buffer table were updated since the previous Draw, and are referenced by
    // the current pipeline, they must be relocated to a new location in GPU memory, and re-dumped from CE RAM.
    const uint16 vertexBufTblRegAddr = m_pSignatureGfx->vertexBufTableRegAddr;
    if ((vertexBufTblRegAddr != 0) && (m_vbTable.watermark > 0))
    {
        if (m_vbTable.state.dirty)
        {
            RelocateUserDataTable<CacheLineDwords>(&m_vbTable.state, 0, m_vbTable.watermark);
            srdTableDumpMask |= VertexBufMask;
        }
        // The GPU virtual address for the vertex buffer table needs to be updated if either the table was relocated,
        // or if the pipeline has changed and the previous pipeline's mapping for this table doesn't match the new
        // mapping.
        if ((HasPipelineChanged && (pPrevSignature->vertexBufTableRegAddr != vertexBufTblRegAddr)) ||
            (srdTableDumpMask & VertexBufMask))
        {
            const uint32 gpuVirtAddrLo = LowPart(m_vbTable.state.gpuVirtAddr);
            pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics>(vertexBufTblRegAddr,
                                                                         gpuVirtAddrLo,
                                                                         pDeCmdSpace);
        }
    } // if vertex buffer table is mapped by current pipeline

    const uint16 streamOutTblRegAddr = m_pSignatureGfx->streamOutTableRegAddr;
    if (streamOutTblRegAddr != 0)
    {
        // When switching to a pipeline which uses stream output, we need to update the SRD table for any
        // bound stream-output buffers because the SRD's depend on the pipeline's per-buffer vertex strides.
        if (HasPipelineChanged)
        {
            dirtyStreamOutSlotMask = CheckStreamOutBufferStridesOnPipelineSwitch();
        }

        if (m_streamOut.state.dirty)
        {
            RelocateUserDataTable<CacheLineDwords>(&m_streamOut.state, 0, StreamOutTableDwords);
            srdTableDumpMask |= StreamOutMask;
        }
        // The GPU virtual address for the stream-out table needs to be updated if either the table was relocated, or if
        // the pipeline has changed and the previous pipeline's mapping for this table doesn't match the new mapping.
        if ((HasPipelineChanged && (pPrevSignature->streamOutTableRegAddr != streamOutTblRegAddr)) ||
            (srdTableDumpMask & StreamOutMask))
        {
            const uint32 gpuVirtAddrLo = LowPart(m_streamOut.state.gpuVirtAddr);
            pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics>(streamOutTblRegAddr,
                                                                         gpuVirtAddrLo,
                                                                         pDeCmdSpace);
        }
    } // if stream-out table is mapped by current pipeline

    // Step #2:
    // Write all dirty user-data entries to their mapped user SGPR's.
    uint8 alreadyWrittenStageMask = 0;
    if (HasPipelineChanged)
    {
        alreadyWrittenStageMask = FixupUserSgprsOnPipelineSwitch<TessEnabled, GsEnabled, VsEnabled>(pPrevSignature,
                                                                                                    &pDeCmdSpace);
    }
    pDeCmdSpace = WriteDirtyUserDataEntriesToSgprsGfx<TessEnabled, GsEnabled, VsEnabled>(pPrevSignature,
                                                                                         alreadyWrittenStageMask,
                                                                                         pDeCmdSpace);

    const uint16 spillThreshold = m_pSignatureGfx->spillThreshold;
    const uint16 spillsUserData = (spillThreshold != NoUserDataSpilling);
    // NOTE: Use of bitwise operators here is to reduce branchiness.
    if ((srdTableDumpMask | spillsUserData) != 0)
    {
        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();

        if (spillsUserData)
        {
            const uint16 userDataLimit = m_pSignatureGfx->userDataLimit;
            PAL_ASSERT(userDataLimit > 0);

            // Step #3:
            // For pipelines which spill user-data to memory, we must make sure that the CE RAM copy of the spill table
            // has the latest copy of any dirty user-data entry values.
            if (HasPipelineChanged)
            {
                FixupSpillTableOnPipelineSwitch(pPrevSignature, m_pSignatureGfx);
            }
            pCeCmdSpace = WriteDirtyUserDataEntriesToCeRam(pPrevSignature, m_pSignatureGfx, pCeCmdSpace);

            // Step #4:
            // At this point, all spilled user-data entries have been updated into CE RAM.  The spill table must now be
            // relocated to a new location in GPU memory, and re-dumped from CE RAM.
            bool relocated = false;
            if (m_spillTable.stateGfx.dirty)
            {
                const uint32 sizeInDwords = (userDataLimit - spillThreshold);
                RelocateUserDataTable<CacheLineDwords>(&m_spillTable.stateGfx, spillThreshold, sizeInDwords);
                pCeCmdSpace = DumpUserDataTable(&m_spillTable.stateGfx, spillThreshold, sizeInDwords, pCeCmdSpace);
                relocated   = true;
            }

            // Step #5:
            // If the spill table was relocated during step #4 above, or if the pipeline is changing and any of the
            // previous pipelines' stages had different mappings for the spill table GPU address user-SGPR, we must
            // re-write the spill table GPU address to the appropriate user-SGPR for each stage.
            if (HasPipelineChanged || relocated)
            {
                const uint32 gpuVirtAddrLo = LowPart(m_spillTable.stateGfx.gpuVirtAddr);
                for (uint32 s = 0; s < NumHwShaderStagesGfx; ++s)
                {
                    const uint16 regAddr = m_pSignatureGfx->stage[s].spillTableRegAddr;
                    if (regAddr != UserDataNotMapped)
                    {
                        pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics>(regAddr,
                                                                                     gpuVirtAddrLo,
                                                                                     pDeCmdSpace);
                    }
                }
            }
        } // if current pipeline spills user-data

        // Step #6:
        // If the stream-out and/or vertex buffer tables are dirty, we need to dump the updated contents from CE RAM
        // into GPU memory.
        if (srdTableDumpMask & VertexBufMask)
        {
            pCeCmdSpace = DumpUserDataTable(&m_vbTable.state, 0, m_vbTable.watermark, pCeCmdSpace);
        } // if indirect user-data table needs dumping

        if (srdTableDumpMask & StreamOutMask)
        {
            if (HasPipelineChanged)
            {
                pCeCmdSpace = UploadStreamOutBufferStridesToCeRam(dirtyStreamOutSlotMask, pCeCmdSpace);
            }
            pCeCmdSpace = DumpUserDataTable(&m_streamOut.state, 0, StreamOutTableDwords, pCeCmdSpace);
        } // if stream-out table needs dumping
        m_ceCmdStream.CommitCommands(pCeCmdSpace);
    } // needs CE workload

    // All dirtied user-data entries have been written to user-SGPR's or to the spill table somewhere in this method,
    // so it is safe to clear these bits.
    memset(&m_graphicsState.gfxUserDataEntries.dirty[0], 0, sizeof(m_graphicsState.gfxUserDataEntries.dirty));

    return pDeCmdSpace;
}

// =====================================================================================================================
// Helper function which is responsible for making sure all user-data entries are written to either the spill table or
// to user-SGPR's, as well as making sure that all indirect user-data tables are up-to-date in GPU memory.  Part of
// Draw-time validation.  This version uses the CPU & embedded data for user-data table management.
template <bool HasPipelineChanged, bool TessEnabled, bool GsEnabled, bool VsEnabled>
uint32* UniversalCmdBuffer::ValidateGraphicsUserDataCpu(
    const GraphicsPipelineSignature* pPrevSignature,
    uint32*                          pDeCmdSpace)
{
    PAL_ASSERT((HasPipelineChanged  && (pPrevSignature != nullptr)) ||
               (!HasPipelineChanged && (pPrevSignature == nullptr)));

    constexpr uint32 StreamOutTableDwords = (sizeof(m_streamOut.srd) / sizeof(uint32));

    // Step #1:
    // If the stream-out table or vertex buffer table were updated since the previous Draw, and are referenced by the
    // current pipeline, they must be relocated to a new location in GPU memory and re-uploaded by the CPU.
    const uint16 vertexBufTblRegAddr = m_pSignatureGfx->vertexBufTableRegAddr;
    if ((vertexBufTblRegAddr != 0) && (m_vbTable.watermark > 0))
    {
        // NOTE: If the pipeline is changing and the previous pipeline's mapping for the VB table doesn't match the
        // current pipeline's, we need to re-write the GPU virtual address even if we don't re-upload the table.
        bool gpuAddrDirty = (HasPipelineChanged && (pPrevSignature->vertexBufTableRegAddr != vertexBufTblRegAddr));

        if (m_vbTable.state.dirty)
        {
            UpdateUserDataTableCpu(&m_vbTable.state,
                                   m_vbTable.watermark,
                                   0,
                                   reinterpret_cast<const uint32*>(m_vbTable.pSrds));
            gpuAddrDirty = true;
        }

        if (gpuAddrDirty)
        {
            pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics>(vertexBufTblRegAddr,
                                                                         LowPart(m_vbTable.state.gpuVirtAddr),
                                                                         pDeCmdSpace);
        }
    } // if vertex buffer table is mapped by current pipeline

    const uint16 streamOutTblRegAddr = m_pSignatureGfx->streamOutTableRegAddr;
    if (streamOutTblRegAddr != 0)
    {
        // When switching to a pipeline which uses stream output, we need to update the SRD table for any
        // bound stream-output buffers because the SRD's depend on the pipeline's per-buffer vertex strides.
        if (HasPipelineChanged)
        {
            CheckStreamOutBufferStridesOnPipelineSwitch();
        }

        // NOTE: If the pipeline is changing and the previous pipeline's mapping for the stream-out table doesn't match
        // the current pipeline's, we need to re-write the GPU virtual address even if we don't re-upload the table.
        bool gpuAddrDirty = (HasPipelineChanged && (pPrevSignature->streamOutTableRegAddr != streamOutTblRegAddr));

        if (m_streamOut.state.dirty)
        {
            UpdateUserDataTableCpu(&m_streamOut.state,
                                   StreamOutTableDwords,
                                   0,
                                   reinterpret_cast<const uint32*>(&m_streamOut.srd[0]));
            gpuAddrDirty = true;
        }

        if (gpuAddrDirty)
        {
            pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics>(streamOutTblRegAddr,
                                                                         LowPart(m_streamOut.state.gpuVirtAddr),
                                                                         pDeCmdSpace);
        }
    } // if stream-out table is mapped by current pipeline

    // Step #2:
    // Write all dirty user-data entries to their mapped user SGPR's.
    uint8 alreadyWrittenStageMask = 0;
    if (HasPipelineChanged)
    {
        alreadyWrittenStageMask = FixupUserSgprsOnPipelineSwitch<TessEnabled, GsEnabled, VsEnabled>(pPrevSignature,
                                                                                                    &pDeCmdSpace);
    }
    pDeCmdSpace = WriteDirtyUserDataEntriesToSgprsGfx<TessEnabled, GsEnabled, VsEnabled>(pPrevSignature,
                                                                                         alreadyWrittenStageMask,
                                                                                         pDeCmdSpace);

    const uint16 spillThreshold = m_pSignatureGfx->spillThreshold;
    if (spillThreshold != NoUserDataSpilling)
    {
        const uint16 userDataLimit = m_pSignatureGfx->userDataLimit;
        PAL_ASSERT(userDataLimit > 0);
        const uint16 lastUserData  = (userDataLimit - 1);

        // Step #3:
        // Because the spill table is managed using CPU writes to embedded data, it must be fully re-uploaded for any
        // Dispatch whenever *any* contents have changed.
        bool reUpload = (m_spillTable.stateCs.dirty != 0);
        if (HasPipelineChanged &&
            ((spillThreshold < pPrevSignature->spillThreshold) || (userDataLimit > pPrevSignature->userDataLimit)))
        {
            // If the pipeline is changing and the spilled region is expanding, we need to re-upload the table because
            // we normally only update the portions useable by the bound pipeline to minimize memory usage.
            reUpload = true;
        }
        else
        {
            // Otherwise, use the following loop to check if any of the spilled user-data entries are dirty.
            const uint32 firstMaskId = (spillThreshold / UserDataEntriesPerMask);
            const uint32 lastMaskId  = (lastUserData   / UserDataEntriesPerMask);
            for (uint32 maskId = firstMaskId; maskId <= lastMaskId; ++maskId)
            {
                uint16 dirtyMask = m_graphicsState.gfxUserDataEntries.dirty[maskId];
                if (maskId == firstMaskId)
                {
                    // Ignore the dirty bits for any entries below the spill threshold.
                    const uint16 firstEntryInMask = (spillThreshold & (UserDataEntriesPerMask - 1));
                    dirtyMask &= ~((1 << firstEntryInMask) - 1);
                }
                if (maskId == lastMaskId)
                {
                    // Ignore the dirty bits for any entries beyond the user-data limit.
                    const uint16 lastEntryInMask = (lastUserData & (UserDataEntriesPerMask - 1));
                    dirtyMask &= ((1 << (lastEntryInMask + 1)) - 1);
                }

                if (dirtyMask != 0)
                {
                    reUpload = true;
                    break; // We only care if *any* spill table contents change!
                }
            } // for each wide-bitfield sub-mask
        }

        // Step #4:
        // Re-upload spill table contents if necessary, and write the new GPU virtual address to the user-SGPR(s).
        if (reUpload)
        {
            UpdateUserDataTableCpu(&m_spillTable.stateGfx,
                                   (userDataLimit - spillThreshold),
                                   spillThreshold,
                                   &m_graphicsState.gfxUserDataEntries.entries[0]);
        }

        // NOTE: If the pipeline is changing, we may need to re-write the spill table address to any shader stage, even
        // if the spill table wasn't re-uploaded because the mapped user-SGPRs for the spill table could have changed.
        if (HasPipelineChanged || reUpload)
        {
            const uint32 gpuVirtAddrLo = LowPart(m_spillTable.stateGfx.gpuVirtAddr);
            for (uint32 s = 0; s < NumHwShaderStagesGfx; ++s)
            {
                const uint16 userSgpr = m_pSignatureGfx->stage[s].spillTableRegAddr;
                if (userSgpr != UserDataNotMapped)
                {
                    pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics>(userSgpr, gpuVirtAddrLo, pDeCmdSpace);
                }
            }
        }
    } // if current pipeline spills user-data

    // All dirtied user-data entries have been written to user-SGPR's or to the spill table somewhere in this method,
    // so it is safe to clear these bits.
    memset(&m_graphicsState.gfxUserDataEntries.dirty[0], 0, sizeof(m_graphicsState.gfxUserDataEntries.dirty));

    return pDeCmdSpace;
}

// =====================================================================================================================
// Helper function which is responsible for making sure all user-data entries are written to either the spill table or
// to user-SGPR's, as well as making sure that all indirect user-data tables are up-to-date in GPU memory.  Part of
// Dispatch-time validation.  This version uses CE RAM for user-data table management.
template <bool HasPipelineChanged>
uint32* UniversalCmdBuffer::ValidateComputeUserDataCeRam(
    const ComputePipelineSignature* pPrevSignature,
    uint32*                         pDeCmdSpace)
{
    PAL_ASSERT((HasPipelineChanged  && (pPrevSignature != nullptr)) ||
               (!HasPipelineChanged && (pPrevSignature == nullptr)));

    // Step #1:
    // Write all dirty user-data entries to their mapped user SGPR's.
    pDeCmdSpace = WriteDirtyUserDataEntriesToUserSgprsCs(pDeCmdSpace);

    const uint16 spillThreshold = m_pSignatureCs->spillThreshold;
    const uint16 spillsUserData = (spillThreshold != NoUserDataSpilling);
    if (spillsUserData != 0)
    {
        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();

        if (spillsUserData)
        {
            const uint16 userDataLimit = m_pSignatureCs->userDataLimit;
            PAL_ASSERT(userDataLimit > 0);

            // Step #2:
            // For pipelines which spill user-data to memory, we must make sure that the CE RAM copy of the spill table
            // has the latest copy of any dirty user-data entry values.
            if (HasPipelineChanged)
            {
                FixupSpillTableOnPipelineSwitch(pPrevSignature, m_pSignatureCs);
            }
            pCeCmdSpace = WriteDirtyUserDataEntriesToCeRam(pPrevSignature, m_pSignatureCs, pCeCmdSpace);

            // Step #3:
            // At this point, all spilled user-data entries have been updated into CE RAM.  The spill table must now be
            // relocated to a new location in GPU memory, and re-dumped from CE RAM.
            bool relocated = false;
            if (m_spillTable.stateCs.dirty)
            {
                const uint32 sizeInDwords = (userDataLimit - spillThreshold);
                RelocateUserDataTable<CacheLineDwords>(&m_spillTable.stateCs, spillThreshold, sizeInDwords);
                pCeCmdSpace = DumpUserDataTable(&m_spillTable.stateCs, spillThreshold, sizeInDwords, pCeCmdSpace);
                relocated   = true;
            }

            // Step #4:
            // If the spill table was relocated during step #4 above, or if the pipeline is changing and the previous
            // pipeline did not spill any user-data entries, we must re-write the spill table GPU address to its
            // user-SGPR.
            if ((HasPipelineChanged && (pPrevSignature->spillThreshold == NoUserDataSpilling)) || relocated)
            {
                const uint32 gpuVirtAddrLo = LowPart(m_spillTable.stateCs.gpuVirtAddr);
                pDeCmdSpace =  m_deCmdStream.WriteSetOneShReg<ShaderCompute>(
                                                                     m_pSignatureCs->stage.spillTableRegAddr,
                                                                     gpuVirtAddrLo,
                                                                     pDeCmdSpace);
            }
        } // if current pipeline spills user-data

        m_ceCmdStream.CommitCommands(pCeCmdSpace);
    } // needs CE workload

    // All dirtied user-data entries have been written to user-SGPR's or to the spill table somewhere in this method,
    // so it is safe to clear these bits.
    memset(&m_computeState.csUserDataEntries.dirty[0], 0, sizeof(m_computeState.csUserDataEntries.dirty));

    return pDeCmdSpace;
}

// =====================================================================================================================
// Helper function which is responsible for making sure all user-data entries are written to either the spill table or
// to user-SGPR's, as well as making sure that all indirect user-data tables are up-to-date in GPU memory.  Part of
// Dispatch-time validation.  This version uses the CPU & embedded data for user-data table management.
template <bool HasPipelineChanged>
uint32* UniversalCmdBuffer::ValidateComputeUserDataCpu(
    const ComputePipelineSignature* pPrevSignature,
    uint32*                         pDeCmdSpace)
{
    PAL_ASSERT((HasPipelineChanged  && (pPrevSignature != nullptr)) ||
               (!HasPipelineChanged && (pPrevSignature == nullptr)));

    // Step #1:
    // Write all dirty user-data entries to their mapped user SGPR's.
    pDeCmdSpace = WriteDirtyUserDataEntriesToUserSgprsCs(pDeCmdSpace);

    const uint16 spillThreshold = m_pSignatureCs->spillThreshold;
    if (spillThreshold != NoUserDataSpilling)
    {
        const uint16 userDataLimit = m_pSignatureCs->userDataLimit;
        PAL_ASSERT(userDataLimit != 0);
        const uint16 lastUserData  = (userDataLimit - 1);

        // Step #2:
        // Because the spill table is managed using CPU writes to embedded data, it must be fully re-uploaded for any
        // Dispatch whenever *any* contents have changed.
        bool reUpload = (m_spillTable.stateCs.dirty != 0);
        if (HasPipelineChanged &&
            ((spillThreshold < pPrevSignature->spillThreshold) || (userDataLimit > pPrevSignature->userDataLimit)))
        {
            // If the pipeline is changing and the spilled region is expanding, we need to re-upload the table because
            // we normally only update the portions useable by the bound pipeline to minimize memory usage.
            reUpload = true;
        }
        else
        {
            // Otherwise, use the following loop to check if any of the spilled user-data entries are dirty.
            const uint32 firstMaskId = (spillThreshold / UserDataEntriesPerMask);
            const uint32 lastMaskId  = (lastUserData   / UserDataEntriesPerMask);
            for (uint32 maskId = firstMaskId; maskId <= lastMaskId; ++maskId)
            {
                uint16 dirtyMask = m_computeState.csUserDataEntries.dirty[maskId];
                if (maskId == firstMaskId)
                {
                    // Ignore the dirty bits for any entries below the spill threshold.
                    const uint16 firstEntryInMask = (spillThreshold & (UserDataEntriesPerMask - 1));
                    dirtyMask &= ~((1 << firstEntryInMask) - 1);
                }
                if (maskId == lastMaskId)
                {
                    // Ignore the dirty bits for any entries beyond the user-data limit.
                    const uint16 lastEntryInMask = (lastUserData & (UserDataEntriesPerMask - 1));
                    dirtyMask &= ((1 << (lastEntryInMask + 1)) - 1);
                }

                if (dirtyMask != 0)
                {
                    reUpload = true;
                    break; // We only care if *any* spill table contents change!
                }
            } // for each wide-bitfield sub-mask
        }

        // Step #3:
        // Re-upload spill table contents if necessary, and write the new GPU virtual address to the user-SGPR.
        if (reUpload)
        {
            UpdateUserDataTableCpu(&m_spillTable.stateCs,
                                   (userDataLimit - spillThreshold),
                                   spillThreshold,
                                   &m_computeState.csUserDataEntries.entries[0]);

            pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderCompute>(m_pSignatureCs->stage.spillTableRegAddr,
                                                                        LowPart(m_spillTable.stateCs.gpuVirtAddr),
                                                                        pDeCmdSpace);
        }
    } // if current pipeline spills user-data

    // All dirtied user-data entries have been written to user-SGPR's or to the spill table somewhere in this method,
    // so it is safe to clear these bits.
    memset(&m_computeState.csUserDataEntries.dirty[0], 0, sizeof(m_computeState.csUserDataEntries.dirty));

    return pDeCmdSpace;
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.  Wrapper to determine
// if immediate mode pm4 optimization is enabled before calling the real ValidateDraw() function.
template <bool Indexed, bool Indirect>
void UniversalCmdBuffer::ValidateDraw(
    const ValidateDrawInfo& drawInfo)      // Draw info
{
    if (m_deCmdStream.Pm4ImmediateOptimizerEnabled())
    {
        ValidateDraw<Indexed, Indirect, true>(drawInfo);
    }
    else
    {
        ValidateDraw<Indexed, Indirect, false>(drawInfo);
    }
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.  Wrapper to determine
// if the pipeline is dirty before calling the real ValidateDraw() function.
template <bool Indexed, bool Indirect, bool Pm4OptImmediate>
void UniversalCmdBuffer::ValidateDraw(
    const ValidateDrawInfo& drawInfo)
{

    if (m_graphicsState.pipelineState.dirtyFlags.pipelineDirty)
    {
        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

        const auto*const pNewPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

        pDeCmdSpace = pNewPipeline->WriteShCommands(&m_deCmdStream, pDeCmdSpace, m_graphicsState.dynamicGraphicsInfo);

        if (m_buildFlags.prefetchShaders)
        {
            pDeCmdSpace = pNewPipeline->Prefetch(pDeCmdSpace);
        }

        const auto*const pPrevSignature = m_pSignatureGfx;
        m_pSignatureGfx                 = &pNewPipeline->Signature();

        pDeCmdSpace = SwitchGraphicsPipeline(pPrevSignature, pNewPipeline, pDeCmdSpace);

        // NOTE: Switching a graphics pipeline can result in a large amount of commands being written, so start a new
        // reserve/commit region before proceeding with validation.
        m_deCmdStream.CommitCommands(pDeCmdSpace);
        pDeCmdSpace = m_deCmdStream.ReserveCommands();

        pDeCmdSpace = (this->*m_pfnValidateUserDataGfxPipelineSwitch)(pPrevSignature, pDeCmdSpace);
        pDeCmdSpace = ValidateDraw<Indexed, Indirect, Pm4OptImmediate, true>(drawInfo, pDeCmdSpace);

        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }
    else
    {
        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

        pDeCmdSpace = (this->*m_pfnValidateUserDataGfx)(nullptr, pDeCmdSpace);
        pDeCmdSpace = ValidateDraw<Indexed, Indirect, Pm4OptImmediate, false>(drawInfo, pDeCmdSpace);

        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.  Wrapper to determine
// if any interesting state is dirty before calling the real ValidateDraw() function.
template <bool Indexed, bool Indirect, bool Pm4OptImmediate, bool PipelineDirty>
uint32* UniversalCmdBuffer::ValidateDraw(
    const ValidateDrawInfo& drawInfo,
    uint32*                 pDeCmdSpace)
{
    if (m_graphicsState.dirtyFlags.validationBits.u16All)
    {
        pDeCmdSpace = ValidateDraw<Indexed, Indirect, Pm4OptImmediate, PipelineDirty, true>(drawInfo, pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = ValidateDraw<Indexed, Indirect, Pm4OptImmediate, PipelineDirty, false>(drawInfo, pDeCmdSpace);
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.  Wrapper to determine
// if the pipeline is NGG before calling the real ValidateDraw() function.
template <bool Indexed, bool Indirect, bool Pm4OptImmediate, bool PipelineDirty, bool StateDirty>
uint32* UniversalCmdBuffer::ValidateDraw(
    const ValidateDrawInfo& drawInfo,
    uint32*                 pDeCmdSpace)
{
    if (static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline)->IsNgg())
    {
        pDeCmdSpace = ValidateDraw<Indexed,
                                   Indirect,
                                   Pm4OptImmediate,
                                   PipelineDirty,
                                   StateDirty,
                                   true>(drawInfo, pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = ValidateDraw<Indexed,
                                   Indirect,
                                   Pm4OptImmediate,
                                   PipelineDirty,
                                   StateDirty,
                                   false>(drawInfo, pDeCmdSpace);
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.  Wrapper to determine
// if the pipeline is NGG Fast Launch before calling the real ValidateDraw() function.
template <bool Indexed,
          bool Indirect,
          bool Pm4OptImmediate,
          bool PipelineDirty,
          bool StateDirty,
          bool IsNgg>
uint32* UniversalCmdBuffer::ValidateDraw(
    const ValidateDrawInfo& drawInfo,
    uint32*                 pDeCmdSpace)
{
    if (IsNgg && static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline)->IsNggFastLaunch())
    {
        pDeCmdSpace = ValidateDraw<Indexed,
            Indirect,
            Pm4OptImmediate,
            PipelineDirty,
            StateDirty,
            IsNgg,
            true>(drawInfo, pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = ValidateDraw<Indexed,
            Indirect,
            Pm4OptImmediate,
            PipelineDirty,
            StateDirty,
            IsNgg,
            false>(drawInfo, pDeCmdSpace);
    }

    return pDeCmdSpace;
}

// Helper structure to convert uint32 to a float.
union Uint32ToFloat
{
    uint32 uValue;
    float  fValue;
};

// =====================================================================================================================
// This function updates the NGG culling data constant buffer which is needed for NGG culling operations to execute
// correctly.
// Returns a pointer to the next entry in the DE cmd space.  This function MUST NOT write any context registers!
uint32* UniversalCmdBuffer::UpdateNggCullingDataBuffer(
    uint32* pDeCmdSpace)
{
    PAL_ASSERT(m_pSignatureGfx->nggCullingDataAddr != UserDataNotMapped);

    // If nothing has changed, then there's no need to do anything...
    if ((m_nggState.flags.dirty.u8All != 0) || m_graphicsState.pipelineState.dirtyFlags.pipelineDirty)
    {
        constexpr uint32 NggStateDwords = (sizeof(Abi::PrimShaderCbLayout) / sizeof(uint32));

        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();

        if (m_nggState.flags.dirty.triangleRasterState)
        {
            pCeCmdSpace = UploadToUserDataTable(
                &m_nggTable.state,
                (offsetof(Abi::PrimShaderCbLayout, renderStateCb) / sizeof(uint32)),
                (sizeof(m_state.primShaderCbLayout.renderStateCb) / sizeof(uint32)),
                reinterpret_cast<const uint32*>(&m_state.primShaderCbLayout.renderStateCb),
                NggStateDwords,
                pCeCmdSpace);
        }

        if (m_nggState.flags.dirty.viewports || m_nggState.flags.dirty.msaaState)
        {
            // For small-primitive filter culling with NGG, the shader needs the viewport scale to premultiply
            // the number of samples into it.
            Abi::PrimShaderVportCb vportCb = m_state.primShaderCbLayout.viewportStateCb;
            Uint32ToFloat uintToFloat = {};
            for (uint32 i = 0; i < m_graphicsState.viewportState.count; i++)
            {
                uintToFloat.uValue  = vportCb.vportControls[i].paClVportXscale;
                uintToFloat.fValue *= (m_nggState.numSamples > 1) ? 16.0f : 1.0f;
                vportCb.vportControls[i].paClVportXscale = uintToFloat.uValue;
            }

            pCeCmdSpace = UploadToUserDataTable(
                &m_nggTable.state,
                (offsetof(Abi::PrimShaderCbLayout, viewportStateCb) / sizeof(uint32)),
                (sizeof(vportCb) / sizeof(uint32)),
                reinterpret_cast<const uint32*>(&vportCb),
                NggStateDwords,
                pCeCmdSpace);
        }

        if (m_graphicsState.pipelineState.dirtyFlags.pipelineDirty ||
            m_nggState.flags.dirty.viewports                       ||
            m_nggState.flags.dirty.triangleRasterState             ||
            m_nggState.flags.dirty.inputAssemblyState)
        {
            pCeCmdSpace = UploadToUserDataTable(
                &m_nggTable.state,
                (offsetof(Abi::PrimShaderCbLayout, pipelineStateCb) / sizeof(uint32)),
                (sizeof(m_state.primShaderCbLayout.pipelineStateCb) / sizeof(uint32)),
                reinterpret_cast<const uint32*>(&m_state.primShaderCbLayout.pipelineStateCb),
                NggStateDwords,
                pCeCmdSpace);
        }

        // It is not expected to enter this path unless we will be updating the NGG CE RAM data!
        PAL_ASSERT(m_nggTable.state.dirty != 0);

#if PAL_DBG_COMMAND_COMMENTS
        pDeCmdSpace += m_cmdUtil.BuildCommentString("NGG: ConstantBufferAddr", pDeCmdSpace);
#endif

        gpusize gpuVirtAddr = 0;
        const uint16 nggRegAddr = m_pSignatureGfx->nggCullingDataAddr;
        if (nggRegAddr == mmSPI_SHADER_PGM_LO_GS)
        {
            // The address of the constant buffer is stored in the GS shader address registers, which require a
            // 256B aligned address.
            constexpr uint32 AlignmentInDwords = (256 / sizeof(uint32));
            RelocateUserDataTable<AlignmentInDwords>(&m_nggTable.state, 0, NggStateDwords);
            gpuVirtAddr = Get256BAddrLo(m_nggTable.state.gpuVirtAddr);
        }
        else
        {
            RelocateUserDataTable<CacheLineDwords>(&m_nggTable.state, 0, NggStateDwords);
            gpuVirtAddr = m_nggTable.state.gpuVirtAddr;
        }

        pDeCmdSpace = m_deCmdStream.WriteSetSeqShRegs(nggRegAddr,
                                                      (nggRegAddr + 1),
                                                      ShaderGraphics,
                                                      &gpuVirtAddr,
                                                      pDeCmdSpace);
        pCeCmdSpace = DumpUserDataTable(&m_nggTable.state, 0, NggStateDwords, pCeCmdSpace);

        m_ceCmdStream.CommitCommands(pCeCmdSpace);

        m_nggState.flags.dirty.u8All = 0;
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.
template <bool Indexed,
          bool Indirect,
          bool Pm4OptImmediate,
          bool PipelineDirty,
          bool StateDirty,
          bool IsNgg,
          bool IsNggFastLaunch>
uint32* UniversalCmdBuffer::ValidateDraw(
    const ValidateDrawInfo& drawInfo,      // Draw info
    uint32*                 pDeCmdSpace)   // Write new draw-engine commands here.
{
    const auto*const pBlendState = static_cast<const ColorBlendState*>(m_graphicsState.pColorBlendState);
    const auto*const pDepthState = static_cast<const DepthStencilState*>(m_graphicsState.pDepthStencilState);
    const auto*const pPipeline   = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const auto*const pMsaaState  = static_cast<const MsaaState*>(m_graphicsState.pMsaaState);
    const auto*const pDsView     =
        static_cast<const DepthStencilView*>(m_graphicsState.bindTargets.depthTarget.pDepthStencilView);

    const auto dirtyFlags = m_graphicsState.dirtyFlags.validationBits;

    // If we're about to launch a draw we better have a pipeline bound.
    PAL_ASSERT(pPipeline != nullptr);

    // All of our dirty state will leak to the caller.
    m_graphicsState.leakFlags.u32All |= m_graphicsState.dirtyFlags.u32All;

    if (PipelineDirty || (StateDirty && dirtyFlags.colorBlendState))
    {
        // Blend state optimizations are associated with the Blend state object, but the CB state affects which
        // optimizations are chosen. We need to make sure we have the best optimizations chosen, so we write it at draw
        // time only if it is dirty.
        if (pBlendState != nullptr)
        {
            pDeCmdSpace = pBlendState->WriteBlendOptimizations<Pm4OptImmediate>(
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
    if (StateDirty && dirtyFlags.viewports)
    {
        pDeCmdSpace = ValidateViewports<Pm4OptImmediate>(pDeCmdSpace);
    }

    regPA_SC_MODE_CNTL_1 paScModeCntl1 = m_drawTimeHwState.paScModeCntl1;

    // Re-calculate paScModeCntl1 value if state constributing to the register has changed.
    if ((m_drawTimeHwState.valid.paScModeCntl1 == 0) ||
        PipelineDirty ||
        (StateDirty && (dirtyFlags.depthStencilState || dirtyFlags.colorBlendState || dirtyFlags.depthStencilView ||
                        dirtyFlags.queryState || dirtyFlags.triangleRasterState)))
    {
        paScModeCntl1 = pPipeline->PaScModeCntl1();

        if (pPipeline->IsOutOfOrderPrimsEnabled() == false)
        {
            paScModeCntl1.bits.OUT_OF_ORDER_PRIMITIVE_ENABLE = pPipeline->CanDrawPrimsOutOfOrder(
                pDsView,
                pDepthState,
                pBlendState,
                MayHaveActiveQueries(),
                static_cast<OutOfOrderPrimMode>(m_cachedSettings.outOfOrderPrimsEnable));
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
    if (StateDirty && (dirtyFlags.msaaState || dirtyFlags.queryState))
    {
        // MSAA sample rates are associated with the MSAA state object, but the sample rate affects how queries are
        // processed (via DB_COUNT_CONTROL). We need to update the value of this register at draw-time since it is
        // affected by multiple elements of command-buffer state.
        const uint32 log2OcclusionQuerySamples = (pMsaaState != nullptr) ? pMsaaState->Log2OcclusionQuerySamples() : 0;
        pDeCmdSpace = UpdateDbCountControl<Pm4OptImmediate>(log2OcclusionQuerySamples, &dbCountControl, pDeCmdSpace);
    }

    if (PipelineDirty || (StateDirty && dirtyFlags.inputAssemblyState) || m_cachedSettings.disableWdLoadBalancing)
    {
        // Typically, ForceWdSwitchOnEop only depends on the primitive topology and restart state.  However, when we
        // disable the hardware WD load balancing feature, we do need to some draw time parameters that can
        // change every draw.
        const bool            wdSwitchOnEop   = ForceWdSwitchOnEop(*pPipeline, drawInfo);
        regIA_MULTI_VGT_PARAM iaMultiVgtParam = pPipeline->IaMultiVgtParam(wdSwitchOnEop);
        regVGT_LS_HS_CONFIG   vgtLsHsConfig   = pPipeline->VgtLsHsConfig();

        if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
        {
            pDeCmdSpace = m_deCmdStream.WriteSetOneConfigReg(Gfx09::mmIA_MULTI_VGT_PARAM,
                                                             iaMultiVgtParam.u32All,
                                                             pDeCmdSpace,
                                                             index__pfp_set_uconfig_reg_index__multi_vgt_param__GFX09);
        }

        pDeCmdSpace = m_deCmdStream.WriteSetVgtLsHsConfig<Pm4OptImmediate>(vgtLsHsConfig, pDeCmdSpace);
    }

    // Underestimation may be used alone or as inner coverage.
    bool onlyUnderestimation = false;

    // Set the conservative rasterization register state.
    // The final setting depends on whether inner coverage was used in the PS.
    if ((PipelineDirty || (StateDirty && dirtyFlags.msaaState)) &&
        (pMsaaState != nullptr))
    {
        auto paScConsRastCntl = pMsaaState->PaScConsRastCntl();

        if (pPipeline->UsesInnerCoverage())
        {
            paScConsRastCntl.bits.UNDER_RAST_ENABLE       = 1; // Inner coverage requires underestimating CR.
            paScConsRastCntl.bits.COVERAGE_AA_MASK_ENABLE = 0;
        }
        else
        {
            onlyUnderestimation = ((paScConsRastCntl.bits.UNDER_RAST_ENABLE == 1) &&
                                   (paScConsRastCntl.bits.OVER_RAST_ENABLE  == 0));
        }

        // Since the vast majority of pipelines do not use ConservativeRast, only update if it changed.
        if (m_paScConsRastCntl.u32All != paScConsRastCntl.u32All)
        {
            pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmPA_SC_CONSERVATIVE_RASTERIZATION_CNTL,
                                                                   paScConsRastCntl.u32All,
                                                                   pDeCmdSpace);
            m_paScConsRastCntl.u32All = paScConsRastCntl.u32All;
        }
    }

    // MSAA num samples are associated with the MSAA state object, but inner coverage affects how many samples are
    // required. We need to update the value of this register.
    // When the pixel shader uses inner coverage the rasterizer needs another "sample" to hold the inner coverage
    // result.
    const uint32 log2MsaaStateSamples = (pMsaaState != nullptr) ? pMsaaState->Log2NumSamples() : 0;
    uint32       log2TotalSamples     = 0;

    if (!onlyUnderestimation)
    {
        log2TotalSamples = log2MsaaStateSamples + (pPipeline->UsesInnerCoverage() ? 1 : 0);
    }
    // Else, use the underestimation result directly as the only covered sample.

    const bool newAaConfigSamples = (m_log2NumSamples != log2TotalSamples);

    if ((StateDirty && dirtyFlags.msaaState) ||
        newAaConfigSamples                   ||
        (m_state.flags.paScAaConfigUpdated == 0))
    {
        m_state.flags.paScAaConfigUpdated = 1;
        m_log2NumSamples = log2TotalSamples;

        regPA_SC_AA_CONFIG paScAaConfig = {};
        paScAaConfig.bits.MSAA_NUM_SAMPLES = log2TotalSamples;
        pDeCmdSpace = m_deCmdStream.WriteContextRegRmw<Pm4OptImmediate>(mmPA_SC_AA_CONFIG,
                                                                        PA_SC_AA_CONFIG__MSAA_NUM_SAMPLES_MASK,
                                                                        paScAaConfig.u32All,
                                                                        pDeCmdSpace);
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
                                     ((StateDirty && dirtyFlags.depthStencilView) || newAaConfigSamples);

        // Is the setting configured such that we want to disable DFSM when the PS uses UAVs or ROVs, and
        // Has the current bound pipeline changed?
        const bool checkDfsmPsUav  = m_cachedSettings.disableDfsmPsUav && PipelineDirty;

        // If we're in EQAA for the purposes of this workaround then we have to kill DFSM.
        // Remember that the register is programmed in terms of log2, while the create info struct is in terms of
        // actual samples.
        if (checkDfsmEqaaWa && (1u << m_log2NumSamples) != pDepthImage->Parent()->GetImageCreateInfo().samples)
        {
            disableDfsm = true;
        }

        if (checkDfsmPsUav && (pPipeline->PsWritesUavs() || pPipeline->PsUsesRovs()))
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
    if (PipelineDirty || (StateDirty && (dirtyFlags.colorBlendState   ||
                                         dirtyFlags.colorTargetView   ||
                                         dirtyFlags.depthStencilView  ||
                                         dirtyFlags.depthStencilState ||
                                         dirtyFlags.msaaState)))
    {
        // Accessing pipeline state in this function is usually a cache miss, so avoid function call
        // when only when pipeline has changed.
        if (PipelineDirty)
        {
            m_pbbStateOverride = pPipeline->GetBinningOverride();
        }
        bool shouldEnablePbb = (m_pbbStateOverride == BinningOverride::Enable);

        if (m_pbbStateOverride == BinningOverride::Default)
        {
            shouldEnablePbb = ShouldEnablePbb(*pPipeline, pBlendState, pDepthState, pMsaaState);
        }

        // Reset binner state unless it used to be off and remains off.  If it was on and remains on, it is possible
        // the ideal bin sizes will change, so we must revalidate.
        if (m_enabledPbb || shouldEnablePbb
            )
        {
            m_enabledPbb = shouldEnablePbb;
            pDeCmdSpace  = ValidateBinSizes<Pm4OptImmediate>(*pPipeline, pBlendState, disableDfsm, pDeCmdSpace);

        }
    }

    // Validate primitive restart enable.  Primitive restart should only apply for indexed draws, but on gfx9,
    // VGT also applies it to auto-generated vertex index values.
    //

    const regPA_SC_SHADER_CONTROL  newPaScShaderControl = pPipeline->PaScShaderControl(drawInfo.vtxIdxCount);
    if (newPaScShaderControl.u32All != m_paScShaderControl.u32All)
    {
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmPA_SC_SHADER_CONTROL,
                                                               newPaScShaderControl.u32All,
                                                               pDeCmdSpace);

        m_paScShaderControl.u32All = newPaScShaderControl.u32All;
    }

    regVGT_MULTI_PRIM_IB_RESET_EN vgtMultiPrimIbResetEn = {};

    vgtMultiPrimIbResetEn.bits.RESET_EN = static_cast<uint32>(
        Indexed && m_graphicsState.inputAssemblyState.primitiveRestartEnable);

    m_state.primShaderCbLayout.renderStateCb.primitiveRestartEnable = vgtMultiPrimIbResetEn.bits.RESET_EN;

    // Validate the per-draw HW state.
    pDeCmdSpace = ValidateDrawTimeHwState<Indexed,
                                          Indirect,
                                          IsNggFastLaunch,
                                          Pm4OptImmediate>(paScModeCntl1,
                                                           dbCountControl,
                                                           vgtMultiPrimIbResetEn,
                                                           drawInfo,
                                                           pDeCmdSpace);

    pDeCmdSpace = m_workaroundState.PreDraw<Indirect, StateDirty, Pm4OptImmediate>(m_graphicsState,
                                                                                   &m_deCmdStream,
                                                                                   this,
                                                                                   drawInfo,
                                                                                   pDeCmdSpace);

    if (IsNgg && (m_pSignatureGfx->nggCullingDataAddr != UserDataNotMapped))
    {
        pDeCmdSpace = UpdateNggCullingDataBuffer(pDeCmdSpace);
    }

    // Clear the dirty-state flags.
    m_graphicsState.dirtyFlags.u32All               = 0;
    m_graphicsState.pipelineState.dirtyFlags.u32All = 0;
    m_deCmdStream.ResetDrawTimeState();

    m_state.flags.firstDrawExecuted = 1;

    return pDeCmdSpace;
}

// =====================================================================================================================
// Gfx9 specific function for calculating Color PBB bin size.
void UniversalCmdBuffer::Gfx9GetColorBinSize(
    Extent2d* pBinSize
    ) const
{
    // TODO: This function needs to be updated to look at the pixel shader and determine which outputs are valid in
    //       addition to looking at the bound render targets. Bound render targets may not necessarily get a pixel
    //       shader export. Using the bound render targets means that we may make the bin size smaller than it needs to
    //       be when a render target is bound, but is not written by the PS. With export cull mask enabled. We need only
    //       examine the PS output because it will account for any RTs that are not bound.

    // Calculate cColor
    //   MMRT = (num_frag == 1) ? 1 : (ps_iter == 1) ? num_frag : 2
    //   CMRT = Bpp * MMRT
    uint32 cColor = 0;

    const auto& boundTargets = m_graphicsState.bindTargets;
    const auto* pPipeline    = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const bool  psIterSample = ((pPipeline != nullptr) && (pPipeline->PaScModeCntl1().bits.PS_ITER_SAMPLE == 1));
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

    // Lookup Color bin size
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
                {       17,    0,    0 },
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
                {        2,  128,  512 },
                {        3,   64,  512 },
                {        5,   32,  512 },
                {        9,   32,  256 },
                {       17,   32,  128 },
                {       33,    0,    0 },
                { UINT_MAX,    0,    0 },
            },
        },
    };

    const CtoBinSize* pBinEntry = GetBinSizeValue(&BinSize[m_log2NumRbPerSe][m_log2NumSes][0], cColor);
    pBinSize->width  = pBinEntry->binSizeX;
    pBinSize->height = pBinEntry->binSizeY;
}

// =====================================================================================================================
// Gfx9 specific function for calculating Depth PBB bin size.
void UniversalCmdBuffer::Gfx9GetDepthBinSize(
    Extent2d* pBinSize
    ) const
{
    const auto*  pDepthTargetView =
            static_cast<const DepthStencilView*>(m_graphicsState.bindTargets.depthTarget.pDepthStencilView);
    const auto*  pImage           = (pDepthTargetView ? pDepthTargetView->GetImage() : nullptr);

    if (pImage == nullptr)
    {
        // Set to max sizes when no depth image bound
        pBinSize->width  = 512;
        pBinSize->height = 512;
    }
    else
    {
        const auto* pDepthStencilState = static_cast<const DepthStencilState*>(m_graphicsState.pDepthStencilState);
        const auto& imageCreateInfo    = pImage->Parent()->GetImageCreateInfo();

        // Calculate cDepth
        //   C_per_sample = ((z_enabled) ? 5 : 0) + ((stencil_enabled) ? 1 : 0)
        //   cDepth = 4 * C_per_sample * num_samples
        const uint32 cPerDepthSample   = (pDepthStencilState->IsDepthEnabled() &&
                                          (pDepthTargetView->ReadOnlyDepth() == false)) ? 5 : 0;
        const uint32 cPerStencilSample = (pDepthStencilState->IsStencilEnabled() &&
                                          (pDepthTargetView->ReadOnlyStencil() == false)) ? 1 : 0;
        const uint32 cDepth            = 4 * (cPerDepthSample + cPerStencilSample) * imageCreateInfo.samples;

        // Lookup Depth bin size
        static constexpr CtoBinSize BinSize[][3][10]=
        {
            {
                // One RB / SE
                {
                    // One shader engine
                    {        0,  64,  512 },
                    {        2,  64,  256 },
                    {        4,  64,  128 },
                    {        7,  32,  128 },
                    {       13,  16,  128 },
                    {       49,   0,    0 },
                    { UINT_MAX,   0,    0 },
                },
                {
                    // Two shader engines
                    {        0, 128,  512 },
                    {        2,  64,  512 },
                    {        4,  64,  256 },
                    {        7,  64,  128 },
                    {       13,  32,  128 },
                    {       25,  16,  128 },
                    {       49,   0,    0 },
                    { UINT_MAX,   0,    0 },
                },
                {
                    // Four shader engines
                    {        0, 256,  512 },
                    {        2, 128,  512 },
                    {        4,  64,  512 },
                    {        7,  64,  256 },
                    {       13,  64,  128 },
                    {       25,  16,  128 },
                    {       49,   0,    0 },
                    { UINT_MAX,   0,    0 },
                },
            },
            {
                // Two RB / SE
                {
                    // One shader engine
                    {        0, 128,  512 },
                    {        2,  64,  512 },
                    {        4,  64,  256 },
                    {        7,  64,  128 },
                    {       13,  32,  128 },
                    {       25,  16,  128 },
                    {       97,   0,    0 },
                    { UINT_MAX,   0,    0 },
                },
                {
                    // Two shader engines
                    {        0,  256,  512 },
                    {        2,  128,  512 },
                    {        4,   64,  512 },
                    {        7,   64,  256 },
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
                    {        4,  128,  512 },
                    {        7,   64,  512 },
                    {       13,   64,  256 },
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
                    {        2,  128,  512 },
                    {        4,   64,  512 },
                    {        7,   64,  256 },
                    {       13,   64,  128 },
                    {       25,   32,  128 },
                    {       49,   16,  128 },
                    {      193,    0,    0 },
                    { UINT_MAX,    0,    0 },
                },
                {
                    // Two shader engines
                    {        0,  512,  512 },
                    {        2,  256,  512 },
                    {        4,  128,  512 },
                    {        7,   64,  512 },
                    {       13,   64,  256 },
                    {       25,   64,  128 },
                    {       49,   32,  128 },
                    {       97,   16,  128 },
                    {      193,    0,    0 },
                    { UINT_MAX,    0,    0 },
                },
                {
                    // Four shader engines
                    {        0,  512,  512 },
                    {        4,  256,  512 },
                    {        7,  128,  512 },
                    {       13,   64,  512 },
                    {       25,   32,  512 },
                    {       49,   32,  256 },
                    {       97,   16,  128 },
                    {      193,    0,    0 },
                    { UINT_MAX,    0,    0 },
                },
            },
        };

        const CtoBinSize* pBinEntry = GetBinSizeValue(&BinSize[m_log2NumRbPerSe][m_log2NumSes][0], cDepth);
        pBinSize->width  = pBinEntry->binSizeX;
        pBinSize->height = pBinEntry->binSizeY;
    }
}

// =====================================================================================================================
// Fills in m_paScBinnerCntl0(PA_SC_BINNER_CNTL_0 register) with values that corresponds to the
// specified binning mode and sizes.   For disabled binning the caller should pass a bin size of zero(0x0).
// 'pBinSize' will be updated with the actual bin size configured.
void UniversalCmdBuffer::SetPaScBinnerCntl0(
    const GraphicsPipeline&  pipeline,
    const ColorBlendState*   pColorBlendState,
    Extent2d*                pBinSize,
    bool                     disableDfsm)
{
    m_paScBinnerCntl0.u32All = 0;

    // If the reported bin sizes are zero, then disable binning
    if ((pBinSize->width == 0) || (pBinSize->height == 0))
    {
        // Note, GetDisableBinningSetting() will update pBinSize if required for binning mode
        m_paScBinnerCntl0.bits.BINNING_MODE = GetDisableBinningSetting(pBinSize);
    }
    else
    {
        m_paScBinnerCntl0.bits.BINNING_MODE              = BINNING_ALLOWED;
        m_paScBinnerCntl0.bits.CONTEXT_STATES_PER_BIN    = m_savedPaScBinnerCntl0.bits.CONTEXT_STATES_PER_BIN;
        m_paScBinnerCntl0.bits.PERSISTENT_STATES_PER_BIN = m_savedPaScBinnerCntl0.bits.PERSISTENT_STATES_PER_BIN;
        m_paScBinnerCntl0.bits.FPOVS_PER_BATCH           = m_savedPaScBinnerCntl0.bits.FPOVS_PER_BATCH;
        m_paScBinnerCntl0.bits.OPTIMAL_BIN_SELECTION     = m_savedPaScBinnerCntl0.bits.OPTIMAL_BIN_SELECTION;
    }

    // If bin size is non-zero, then set the size properties
    if ((pBinSize->width != 0) && (pBinSize->height != 0))
    {
        if (pBinSize->width == 16)
        {
            m_paScBinnerCntl0.bits.BIN_SIZE_X = 1;
        }
        else
        {
            m_paScBinnerCntl0.bits.BIN_SIZE_X_EXTEND = Device::GetBinSizeEnum(pBinSize->width);
        }

        if (pBinSize->height == 16)
        {
            m_paScBinnerCntl0.bits.BIN_SIZE_Y = 1;
        }
        else
        {
            m_paScBinnerCntl0.bits.BIN_SIZE_Y_EXTEND = Device::GetBinSizeEnum(pBinSize->height);
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

    // Only really need to set this bit if we're transitioning from mode 0 or 1 to mode 2 or 3.
    // PAL only ever uses modes 0 and 3.
    if (IsGfx091xPlus(*(m_device.Parent())) &&
        (static_cast<uint32>(m_binningMode) != m_paScBinnerCntl0.bits.BINNING_MODE))
    {
        m_paScBinnerCntl0.gfx09_1xPlus.FLUSH_ON_BINNING_TRANSITION = 1;

        m_binningMode = static_cast<BinningMode>(m_paScBinnerCntl0.bits.BINNING_MODE);
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
                                  ((pMsaaState != nullptr) &&
                                   (pMsaaState->Log2NumSamples() != 0) && pipeline.IsAlphaToMaskEnable());
        const bool canReject    = pipeline.PsCanTriviallyReject();
        const bool isWriteBound = (pDepthState != nullptr) &&
                                  (pDepthView  != nullptr) &&
                                  ((pDepthState->IsDepthWriteEnabled() && (pDepthView->ReadOnlyDepth() == false)) ||
                                   (pDepthState->IsStencilWriteEnabled() && (pDepthView->ReadOnlyStencil() == false)));

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
            Extent2d colorBinSize = {};
            Extent2d depthBinSize = {};
            {
                // Final bin size is choosen from minimum between Depth and Color.
                Gfx9GetColorBinSize(&colorBinSize);
                Gfx9GetDepthBinSize(&depthBinSize);
            }
            const uint32 colorArea = colorBinSize.width * colorBinSize.height;
            const uint32 depthArea = depthBinSize.width * depthBinSize.height;

            binSize = (colorArea < depthArea) ? colorBinSize : depthBinSize;
        }
    }

    // Update our copy of m_paScBinnerCntl0 and write it out.
    SetPaScBinnerCntl0(pipeline, pColorBlendState, &binSize, disableDfsm);

    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg<pm4OptImmediate>(mmPA_SC_BINNER_CNTL_0,
                                                                       m_paScBinnerCntl0.u32All,
                                                                       pDeCmdSpace);

    // Update the current bin sizes chosen.
    m_currentBinSize = binSize;

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
    bool               multipleViewports,
    ScissorRectPm4Img* pScissorRectImg
    ) const
{
    const auto& viewportState = m_graphicsState.viewportState;
    const auto& scissorState  = m_graphicsState.scissorRectState;

    const uint32 scissorCount       = (multipleViewports ? scissorState.count : 1);
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
    uint32* pDeCmdSpace)
{
    ScissorRectPm4Img scissorRectImg[MaxViewports];
    const uint32 numScissorRectRegs = BuildScissorRectImage((m_graphicsState.enableMultiViewport != 0), scissorRectImg);

    pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs<pm4OptImmediate>(
                                    mmPA_SC_VPORT_SCISSOR_0_TL,
                                    mmPA_SC_VPORT_SCISSOR_0_TL + numScissorRectRegs - 1,
                                    &scissorRectImg[0],
                                    pDeCmdSpace);

    return pDeCmdSpace;
}

// =====================================================================================================================
// Wrapper for the real ValidateScissorRects() for when the caller doesn't know if the immediate pm4 optimizer is
// enabled.
uint32* UniversalCmdBuffer::ValidateScissorRects(
    uint32* pDeCmdSpace)
{
    if (m_deCmdStream.Pm4ImmediateOptimizerEnabled())
    {
        pDeCmdSpace = ValidateScissorRects<true>(pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = ValidateScissorRects<false>(pDeCmdSpace);
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Update the HW state and write the necessary packets to push any changes to the HW. Returns the next unused DWORD
// in pDeCmdSpace.
template <bool Indexed, bool Indirect, bool IsNggFastLaunch, bool Pm4OptImmediate>
uint32* UniversalCmdBuffer::ValidateDrawTimeHwState(
    regPA_SC_MODE_CNTL_1          paScModeCntl1,         // PA_SC_MODE_CNTL_1 register value.
    regDB_COUNT_CONTROL           dbCountControl,        // DB_COUNT_CONTROL register value.
    regVGT_MULTI_PRIM_IB_RESET_EN vgtMultiPrimIbResetEn, // VGT_MULTI_PRIM_IB_RESET_EN register value.
    const ValidateDrawInfo&       drawInfo,              // Draw info
    uint32*                       pDeCmdSpace)           // Write new draw-engine commands here.
{
    if ((m_drawTimeHwState.vgtMultiPrimIbResetEn.u32All != vgtMultiPrimIbResetEn.u32All) ||
        (m_drawTimeHwState.valid.vgtMultiPrimIbResetEn == 0))
    {
        m_drawTimeHwState.vgtMultiPrimIbResetEn.u32All = vgtMultiPrimIbResetEn.u32All;
        m_drawTimeHwState.valid.vgtMultiPrimIbResetEn  = 1;

        pDeCmdSpace = m_deCmdStream.WriteSetOneConfigReg(Gfx09::mmVGT_MULTI_PRIM_IB_RESET_EN,
                                                         vgtMultiPrimIbResetEn.u32All,
                                                         pDeCmdSpace);
    }

    if ((m_drawTimeHwState.paScModeCntl1.u32All != paScModeCntl1.u32All) ||
        (m_drawTimeHwState.valid.paScModeCntl1 == 0))
    {
        m_drawTimeHwState.paScModeCntl1.u32All = paScModeCntl1.u32All;
        m_drawTimeHwState.valid.paScModeCntl1  = 1;

        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg<Pm4OptImmediate>(mmPA_SC_MODE_CNTL_1,
                                                                           paScModeCntl1.u32All,
                                                                           pDeCmdSpace);
    }

    if ((m_drawTimeHwState.dbCountControl.u32All != dbCountControl.u32All) ||
        (m_drawTimeHwState.valid.dbCountControl == 0))
    {
        m_drawTimeHwState.dbCountControl.u32All = dbCountControl.u32All;
        m_drawTimeHwState.valid.dbCountControl = 1;

        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg<Pm4OptImmediate>(mmDB_COUNT_CONTROL,
                                                                           dbCountControl.u32All,
                                                                           pDeCmdSpace);
    }

    if (m_drawIndexReg != UserDataNotMapped)
    {
        if (Indirect)
        {
            // If the active pipeline uses the draw index VS input value, then the PM4 draw packet to issue the multi
            // draw will blow-away the SPI user-data register used to pass that value to the shader.
            m_drawTimeHwState.valid.drawIndex = 0;
        }
        else if (m_drawTimeHwState.valid.drawIndex == 0)
        {
            // Otherwise, this SH register write will reset it to zero for us.
            m_drawTimeHwState.valid.drawIndex = 1;
            pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics, Pm4OptImmediate>(m_drawIndexReg,
                                                                                          0,
                                                                                          pDeCmdSpace);
        }
    }
    // Write the INDEX_TYPE packet.
    // We might need to write this outside of indexed draws (for instance, on a change of NGG <-> Legacy pipeline).
    if ((m_drawTimeHwState.dirty.indexType != 0) || (Indexed && (m_drawTimeHwState.dirty.indexedIndexType != 0)))
    {
        m_drawTimeHwState.dirty.indexType        = 0;
        m_drawTimeHwState.dirty.indexedIndexType = 0;
        pDeCmdSpace += m_cmdUtil.BuildIndexType(m_vgtDmaIndexType.u32All, pDeCmdSpace);
    }

    if (Indexed)
    {
        // Note that leakFlags.iaState implies an IB has been bound.
        if (m_graphicsState.leakFlags.nonValidationBits.iaState == 1)
        {
            // Direct indexed draws use DRAW_INDEX_2 which contains the IB base and size. This means that
            // we only have to validate the IB base and size for indirect indexed draws.
            if (Indirect)
            {
                // Write the INDEX_BASE packet.
                if (m_drawTimeHwState.dirty.indexBufferBase != 0)
                {
                    m_drawTimeHwState.dirty.indexBufferBase        = 0;
                    m_drawTimeHwState.valid.nggIndexBufferBaseAddr = 0;
                    pDeCmdSpace += CmdUtil::BuildIndexBase(m_graphicsState.iaState.indexAddr, pDeCmdSpace);
                }

                // Write the INDEX_BUFFER_SIZE packet.
                if (m_drawTimeHwState.dirty.indexBufferSize != 0)
                {
                    m_drawTimeHwState.dirty.indexBufferSize = 0;
                    pDeCmdSpace += CmdUtil::BuildIndexBufferSize(m_graphicsState.iaState.indexCount, pDeCmdSpace);
                }
            }
        }
    }

    if (Indirect)
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

            pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics, Pm4OptImmediate>(GetVertexOffsetRegAddr(),
                                                                                          drawInfo.firstVertex,
                                                                                          pDeCmdSpace);
        }

        // Write the instance offset user data register.
        if ((m_drawTimeHwState.instanceOffset != drawInfo.firstInstance) ||
            (m_drawTimeHwState.valid.instanceOffset == 0))
        {
            m_drawTimeHwState.instanceOffset       = drawInfo.firstInstance;
            m_drawTimeHwState.valid.instanceOffset = 1;

            pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics, Pm4OptImmediate>(GetInstanceOffsetRegAddr(),
                                                                                          drawInfo.firstInstance,
                                                                                          pDeCmdSpace);
        }

        // Write the NUM_INSTANCES packet.
        if ((m_drawTimeHwState.numInstances != drawInfo.instanceCount) || (m_drawTimeHwState.valid.numInstances == 0))
        {
            m_drawTimeHwState.numInstances       = drawInfo.instanceCount;
            m_drawTimeHwState.valid.numInstances = 1;

            pDeCmdSpace += CmdUtil::BuildNumInstances(drawInfo.instanceCount, pDeCmdSpace);
        }
    }

    if (IsNggFastLaunch)
    {
        pDeCmdSpace = ValidateDrawTimeNggFastLaunchState<Indexed, Indirect, Pm4OptImmediate>(drawInfo, pDeCmdSpace);
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
template <bool UseCpuPathForUserDataTables>
uint32* UniversalCmdBuffer::ValidateDispatch(
    gpusize indirectGpuVirtAddr,
    uint32  xDim,
    uint32  yDim,
    uint32  zDim,
    uint32* pDeCmdSpace)
{
    if (m_computeState.pipelineState.dirtyFlags.pipelineDirty)
    {
        const auto*const pNewPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);

        pDeCmdSpace = pNewPipeline->WriteCommands(&m_deCmdStream,
                                                  pDeCmdSpace,
                                                  m_computeState.dynamicCsInfo,
                                                  m_buildFlags.prefetchShaders);

        const auto*const pPrevSignature = m_pSignatureCs;
        m_pSignatureCs                  = &pNewPipeline->Signature();

        if (UseCpuPathForUserDataTables)
        {
            pDeCmdSpace = ValidateComputeUserDataCpu<true>(pPrevSignature, pDeCmdSpace);
        }
        else
        {
            pDeCmdSpace = ValidateComputeUserDataCeRam<true>(pPrevSignature, pDeCmdSpace);
        }
    }
    else
    {
        if (UseCpuPathForUserDataTables)
        {
            pDeCmdSpace = ValidateComputeUserDataCpu<false>(nullptr, pDeCmdSpace);
        }
        else
        {
            pDeCmdSpace = ValidateComputeUserDataCeRam<false>(nullptr, pDeCmdSpace);
        }
    }

    m_computeState.pipelineState.dirtyFlags.u32All = 0;

    if (m_pSignatureCs->numWorkGroupsRegAddr != UserDataNotMapped)
    {
        // Indirect Dispatches by definition have the number of thread-groups to launch stored in GPU memory at the
        // specified address.  However, for direct Dispatches, we must allocate some embedded memory to store this
        // information.
        if (indirectGpuVirtAddr == 0uLL) // This is a direct Dispatch.
        {
            uint32*const pData = CmdAllocateEmbeddedData(3, 4, &indirectGpuVirtAddr);
            pData[0] = xDim;
            pData[1] = yDim;
            pData[2] = zDim;
        }

        pDeCmdSpace = m_deCmdStream.WriteSetSeqShRegs(m_pSignatureCs->numWorkGroupsRegAddr,
                                                      (m_pSignatureCs->numWorkGroupsRegAddr + 1),
                                                      ShaderCompute,
                                                      &indirectGpuVirtAddr,
                                                      pDeCmdSpace);
    }

    return pDeCmdSpace;
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
void UniversalCmdBuffer::CmdSetBufferFilledSize(
    uint32  bufferId,
    uint32  offset)
{
    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    PAL_ASSERT(bufferId < MaxStreamOutTargets);

    pDeCmdSpace += m_cmdUtil.BuildStrmoutBufferUpdate(bufferId,
                                                      source_select__pfp_strmout_buffer_update__use_buffer_offset,
                                                      offset,
                                                      0uLL,
                                                      0uLL,
                                                      pDeCmdSpace);

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
    const uint32 packetPredicate = m_gfxCmdBufState.flags.packetPredicate;
    m_gfxCmdBufState.flags.packetPredicate = 0;

    m_device.RsrcProcMgr().CmdResolveQuery(this,
                                           static_cast<const QueryPool&>(queryPool),
                                           flags,
                                           queryType,
                                           startQuery,
                                           queryCount,
                                           static_cast<const GpuMemory&>(dstGpuMemory),
                                           dstOffset,
                                           dstStride);

    m_gfxCmdBufState.flags.packetPredicate = packetPredicate;
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
            pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(PIPELINESTAT_STOP, EngineTypeUniversal, pDeCmdSpace);
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
            pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(PIPELINESTAT_START, EngineTypeUniversal, pDeCmdSpace);
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

        // Hardware does not enable depth testing when issuing a depth only render pass with depth writes disabled.
        // Unfortunately this corner case prevents depth tiles from being generated and when setting
        // PERFECT_ZPASS_COUNTS = 0, the hardware relies on counting at the tile granularity for binary occlusion
        // queries.  With the depth test disabled and PERFECT_ZPASS_COUNTS = 0, there will be 0 tiles generated which
        // will cause the binary occlusion test to always generate depth pass counts of 0.
        // Setting PERFECT_ZPASS_COUNTS = 1 forces tile generation and reliable binary occlusion query results.
        pDbCountControl->bits.PERFECT_ZPASS_COUNTS    = 1;
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
    // add draw opaque.

    const PrimitiveTopology primTopology            = m_graphicsState.inputAssemblyState.topology;
    const bool              primitiveRestartEnabled = m_graphicsState.inputAssemblyState.primitiveRestartEnable;

    bool switchOnEop = ((primTopology == PrimitiveTopology::TriangleStripAdj) ||
                        (primTopology == PrimitiveTopology::TriangleFan) ||
                        (primitiveRestartEnabled &&
                         ((primTopology != PrimitiveTopology::PointList) &&
                          (primTopology != PrimitiveTopology::LineStrip) &&
                          (primTopology != PrimitiveTopology::TriangleStrip))) ||
                        drawInfo.useOpaque);

    if ((switchOnEop == false) && m_cachedSettings.disableWdLoadBalancing)
    {
        const uint32 primGroupSize = pipeline.IaMultiVgtParam(false).bits.PRIMGROUP_SIZE + 1;

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
    constexpr uint32 CpStrmoutCntlData = 0;
    WriteDataInfo    writeData         = {};

    writeData.engineType       = m_engineType;
    writeData.dstAddr          = mmCP_STRMOUT_CNTL;
    writeData.engineSel        = engine_sel__me_write_data__micro_engine;
    writeData.dstSel           = dst_sel__me_write_data__mem_mapped_register;
    writeData.dontWriteConfirm = true;

    pDeCmdSpace += CmdUtil::BuildWriteData(writeData, CpStrmoutCntlData, pDeCmdSpace);
    pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(SO_VGTSTREAMOUT_FLUSH, EngineTypeUniversal, pDeCmdSpace);
    pDeCmdSpace += CmdUtil::BuildWaitRegMem(mem_space__pfp_wait_reg_mem__register_space,
                                            function__pfp_wait_reg_mem__equal_to_the_reference_value,
                                            engine_sel__me_wait_reg_mem__micro_engine,
                                            mmCP_STRMOUT_CNTL,
                                            1,
                                            0x00000001,
                                            pDeCmdSpace);

    return pDeCmdSpace;
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
    const auto*const pUniversalCmdBuffer = static_cast<const UniversalCmdBuffer*>(pCmdBuffer);

    SetGraphicsState(pUniversalCmdBuffer->GetGraphicsState());
    SetComputeState(pCmdBuffer->GetComputeState(), ComputeStateAll);

    if (pUniversalCmdBuffer->m_vbTable.modified != 0)
    {
        m_vbTable.modified  = 1;
        m_vbTable.watermark = pUniversalCmdBuffer->m_vbTable.watermark;
        memcpy(m_vbTable.pSrds, pUniversalCmdBuffer->m_vbTable.pSrds, (sizeof(BufferSrd) * MaxVertexBuffers));

        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
        pCeCmdSpace = UploadToUserDataTable(&m_vbTable.state,
                                            0,
                                            m_vbTable.state.sizeInDwords,
                                            reinterpret_cast<uint32*>(m_vbTable.pSrds),
                                            m_vbTable.watermark,
                                            pCeCmdSpace);
        m_ceCmdStream.CommitCommands(pCeCmdSpace);
    }
}

// =====================================================================================================================
// Updates the SQTT token mask for all SEs outside of a specific PerfExperiment.  Used by GPA Session when targeting
// a single event for instruction level trace during command buffer building.
void UniversalCmdBuffer::CmdUpdateSqttTokenMask(
    const ThreadTraceTokenConfig& sqttTokenConfig)
{
    PerfExperiment::UpdateSqttTokenMaskStatic(&m_deCmdStream, sqttTokenConfig, m_device);
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

    // Keep track of the latest DUMP_CONST_RAM packet before the upcoming draw or dispatch.  The last one before the
    // draw or dispatch will be updated to set the increment_ce bit at draw-time.
    m_state.pLastDumpCeRam                    = pCeCmdSpace;
    m_state.lastDumpCeRamOrdinal2.bits.offset = (ReservedCeRamBytes + ramOffset);

    pCeCmdSpace += m_cmdUtil.BuildDumpConstRam(dstGpuMemory.Desc().gpuVirtAddr + memOffset,
                                               (ReservedCeRamBytes + ramOffset),
                                               dwordSize,
                                               pCeCmdSpace);
    m_ceCmdStream.CommitCommands(pCeCmdSpace);
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
void UniversalCmdBuffer::CmdSetHiSCompareState0(
    CompareFunc compFunc,
    uint32      compMask,
    uint32      compValue,
    bool        enable)
{
    regDB_SRESULTS_COMPARE_STATE0 dbSResultCompare;

    dbSResultCompare.bitfields.COMPAREFUNC0  = DepthStencilState :: HwStencilCompare(compFunc);
    dbSResultCompare.bitfields.COMPAREMASK0  = compMask;
    dbSResultCompare.bitfields.COMPAREVALUE0 = compValue;
    dbSResultCompare.bitfields.ENABLE0       = enable;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    pDeCmdSpace =
        m_deCmdStream.WriteSetOneContextReg(mmDB_SRESULTS_COMPARE_STATE0, dbSResultCompare.u32All, pDeCmdSpace);

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetHiSCompareState1(
    CompareFunc compFunc,
    uint32      compMask,
    uint32      compValue,
    bool        enable)
{
    regDB_SRESULTS_COMPARE_STATE1 dbSResultCompare;

    dbSResultCompare.bitfields.COMPAREFUNC1  = DepthStencilState :: HwStencilCompare(compFunc);
    dbSResultCompare.bitfields.COMPAREMASK1  = compMask;
    dbSResultCompare.bitfields.COMPAREVALUE1 = compValue;
    dbSResultCompare.bitfields.ENABLE1       = enable;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    pDeCmdSpace =
        m_deCmdStream.WriteSetOneContextReg(mmDB_SRESULTS_COMPARE_STATE1, dbSResultCompare.u32All, pDeCmdSpace);

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Enables or disables a flexible predication check which the CP uses to determine if a draw or dispatch can be skipped
// based on the results of prior GPU work.
// SEE: CmdUtil::BuildSetPredication(...) for more details on the meaning of this method's parameters.
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

    m_gfxCmdBufState.flags.clientPredicate = ((pQueryPool != nullptr) || (pGpuMemory != nullptr)) ? 1 : 0;
    m_gfxCmdBufState.flags.packetPredicate = m_gfxCmdBufState.flags.clientPredicate;

    gpusize gpuVirtAddr = 0;
    if (pGpuMemory != nullptr)
    {
        gpuVirtAddr = pGpuMemory->Desc().gpuVirtAddr + offset;
    }

    if (pQueryPool != nullptr)
    {
        Result result = static_cast<QueryPool*>(pQueryPool)->GetQueryGpuAddress(slot, &gpuVirtAddr);
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
    PAL_ASSERT((bindPoint == PipelineBindPoint::Compute) ||
               (static_cast<const GraphicsPipeline*>(PipelineState(bindPoint)->pPipeline)->IsNggFastLaunch() == false));

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
        m_gfxCmdBufState.flags.packetPredicate = 0;

        m_device.RsrcProcMgr().CmdGenerateIndirectCmds(this,
                                                       PipelineState(bindPoint)->pPipeline,
                                                       gfx9Generator,
                                                       (gpuMemory.Desc().gpuVirtAddr + offset),
                                                       countGpuAddr,
                                                       m_graphicsState.iaState.indexCount,
                                                       maximumCount);

        m_gfxCmdBufState.flags.packetPredicate = packetPredicate;

        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

        // Insert a CS_PARTIAL_FLUSH to make sure that the generated commands are written out to L2 before we attempt to
        // execute them. Then, a PFP_SYNC_ME is also required so that the PFP doesn't prefetch the generated commands
        // before they are finished executing.
        AcquireMemInfo acquireInfo = {};
        acquireInfo.flags.invSqK$ = 1;
        acquireInfo.tcCacheOp     = TcCacheOp::Nop;
        acquireInfo.engineType    = EngineTypeUniversal;
        acquireInfo.baseAddress   = FullSyncBaseAddr;
        acquireInfo.sizeBytes     = FullSyncSize;

        pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, EngineTypeUniversal, pDeCmdSpace);
        pDeCmdSpace += m_cmdUtil.BuildAcquireMem(acquireInfo, pDeCmdSpace);
        pDeCmdSpace += CmdUtil::BuildPfpSyncMe(pDeCmdSpace);
        m_deCmdStream.SetContextRollDetected<false>();

        m_deCmdStream.CommitCommands(pDeCmdSpace);

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
            drawInfo.useOpaque     = false;
            if (gfx9Generator.ContainsIndexBufferBind() || (gfx9Generator.Type() == GeneratorType::Draw))
            {
                ValidateDraw<false, true>(drawInfo);
            }
            else
            {
                ValidateDraw<true, true>(drawInfo);
            }

            CommandGeneratorTouchedUserData(m_graphicsState.gfxUserDataEntries.touched,
                                            gfx9Generator,
                                            *m_pSignatureGfx);
        }
        else
        {
            pDeCmdSpace = m_deCmdStream.ReserveCommands();
            if (UseCpuPathInsteadOfCeRam())
            {
                pDeCmdSpace = ValidateDispatch<true>(0uLL, 0, 0, 0, pDeCmdSpace);
            }
            else
            {
                pDeCmdSpace = ValidateDispatch<false>(0uLL, 0, 0, 0, pDeCmdSpace);
            }
            m_deCmdStream.CommitCommands(pDeCmdSpace);

            CommandGeneratorTouchedUserData(m_computeState.csUserDataEntries.touched, gfx9Generator, *m_pSignatureCs);
        }

        if (setViewId)
        {
            const auto& viewInstancingDesc = pGfxPipeline->GetViewInstancingDesc();

            pDeCmdSpace = m_deCmdStream.ReserveCommands();
            pDeCmdSpace = BuildWriteViewId(viewInstancingDesc.viewId[i], pDeCmdSpace);
            m_deCmdStream.CommitCommands(pDeCmdSpace);
        }

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

    const uint32* pUserDataEntries   = nullptr;
    bool          usesVertexBufTable = false;
    uint32        spillThreshold     = NoUserDataSpilling;

    if (generator.Type() == GeneratorType::Dispatch)
    {
        const auto& signature = static_cast<const ComputePipeline&>(pipeline).Signature();
        spillThreshold = signature.spillThreshold;

        // NOTE: RPM uses a compute shader to generate indirect commands, so we need to use the saved user-data
        // state because RPM will have pushed its own state before calling this method.
        pUserDataEntries = &m_computeRestoreState.csUserDataEntries.entries[0];
    }
    else
    {
        const auto& signature = static_cast<const GraphicsPipeline&>(pipeline).Signature();
        usesVertexBufTable = (signature.vertexBufTableRegAddr != 0);
        spillThreshold     = signature.spillThreshold;

        // NOTE: RPM uses a compute shader to generate indirect commands, which doesn't interfere with the graphics
        // state, so we don't need to look at the pushed state.
        pUserDataEntries = &m_graphicsState.gfxUserDataEntries.entries[0];
    }

    // Total amount of embedded data space needed for each generated command, including indirect user-data tables and
    // user-data spilling.
    uint32 embeddedDwords = 0;
    // Amount of embedded data space needed for each generated command, for the vertex buffer table:
    uint32 vertexBufTableDwords = 0;
    // User-data high watermark for this command Generator. It depends on the command Generator itself, as well as the
    // pipeline signature for the active pipeline. This is due to the fact that if the command Generator modifies the
    // contents of an indirect user-data table, the command Generator must also fix-up the user-data entry used for the
    // table's GPU virtual address.
    uint32 userDataWatermark = properties.userDataWatermark;

    if (usesVertexBufTable && (properties.vertexBufTableSize != 0))
    {
        vertexBufTableDwords = properties.vertexBufTableSize;
        embeddedDwords      += vertexBufTableDwords;
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
            if (vertexBufTableDwords != 0)
            {
                memcpy(pDataSpace, m_vbTable.pSrds, (sizeof(uint32) * vertexBufTableDwords));
                pDataSpace += vertexBufTableDwords;
            }

            if (spillDwords != 0)
            {
                memcpy(pDataSpace, pUserDataEntries, (sizeof(uint32) * spillDwords));
                pDataSpace += spillDwords;
            }
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

    if (cmdBuffer.m_graphicsState.pipelineState.pPipeline != nullptr)
    {
        m_vertexOffsetReg           = cmdBuffer.m_vertexOffsetReg;
        m_drawIndexReg              = cmdBuffer.m_drawIndexReg;
        m_nggState.startIndexReg    = cmdBuffer.m_nggState.startIndexReg;
        m_nggState.log2IndexSizeReg = cmdBuffer.m_nggState.log2IndexSizeReg;
        m_nggState.numSamples       = cmdBuffer.m_nggState.numSamples;

        // Update the functions that are modified by nested command list
        m_pfnValidateUserDataGfx                   = cmdBuffer.m_pfnValidateUserDataGfx;
        m_pfnValidateUserDataGfxPipelineSwitch     = cmdBuffer.m_pfnValidateUserDataGfxPipelineSwitch;
        m_funcTable.pfnCmdDraw                     = cmdBuffer.m_funcTable.pfnCmdDraw;
        m_funcTable.pfnCmdDrawOpaque               = cmdBuffer.m_funcTable.pfnCmdDrawOpaque;
        m_funcTable.pfnCmdDrawIndexed              = cmdBuffer.m_funcTable.pfnCmdDrawIndexed;
        m_funcTable.pfnCmdDrawIndirectMulti        = cmdBuffer.m_funcTable.pfnCmdDrawIndirectMulti;
        m_funcTable.pfnCmdDrawIndexedIndirectMulti = cmdBuffer.m_funcTable.pfnCmdDrawIndexedIndirectMulti;

        if (m_rbPlusPm4Img.spaceNeeded != 0)
        {
            m_rbPlusPm4Img.sxPsDownconvert   = cmdBuffer.m_rbPlusPm4Img.sxPsDownconvert;
            m_rbPlusPm4Img.sxBlendOptEpsilon = cmdBuffer.m_rbPlusPm4Img.sxBlendOptEpsilon;
            m_rbPlusPm4Img.sxBlendOptControl = cmdBuffer.m_rbPlusPm4Img.sxBlendOptControl;
        }

        m_spiPsInControl       = cmdBuffer.m_spiPsInControl;
        m_paScShaderControl    = cmdBuffer.m_paScShaderControl;
        m_spiVsOutConfig       = cmdBuffer.m_spiVsOutConfig;
    }

    // If the nested command buffer updated PA_SC_CONS_RAST_CNTL, leak its state back to the caller.
    if ((cmdBuffer.m_graphicsState.pipelineState.pPipeline != nullptr) ||
        (cmdBuffer.m_graphicsState.leakFlags.validationBits.msaaState))
    {
        m_paScConsRastCntl.u32All = cmdBuffer.m_paScConsRastCntl.u32All;
    }

    if (cmdBuffer.HasStreamOutBeenSet())
    {
        // If the nested command buffer set their own stream-out targets, we can simply copy the SRD's because CE
        // RAM is up-to-date.
        memcpy(&m_streamOut.srd[0], &cmdBuffer.m_streamOut.srd[0], sizeof(m_streamOut.srd));
    }

    m_drawTimeHwState.valid.u32All = 0;

    m_vbTable.state.dirty       |= cmdBuffer.m_vbTable.modified;
    m_spillTable.stateCs.dirty  |= cmdBuffer.m_spillTable.stateCs.dirty;
    m_spillTable.stateGfx.dirty |= cmdBuffer.m_spillTable.stateGfx.dirty;

    m_pipelineCtxPm4Hash   = cmdBuffer.m_pipelineCtxPm4Hash;
    m_pipelinePsHash       = cmdBuffer.m_pipelinePsHash;
    m_pipelineFlags.u32All = cmdBuffer.m_pipelineFlags.u32All;

    m_nggState.flags.state.hasPrimShaderWorkload |= cmdBuffer.m_nggState.flags.state.hasPrimShaderWorkload;
    m_nggState.flags.dirty.u8All                 |= cmdBuffer.m_nggState.flags.dirty.u8All;

    // It is possible that nested command buffer execute operation which affect the data in the primary buffer
    m_gfxCmdBufState.flags.gfxBltActive              = cmdBuffer.m_gfxCmdBufState.flags.gfxBltActive;
    m_gfxCmdBufState.flags.csBltActive               = cmdBuffer.m_gfxCmdBufState.flags.csBltActive;
    m_gfxCmdBufState.flags.gfxWriteCachesDirty       = cmdBuffer.m_gfxCmdBufState.flags.gfxWriteCachesDirty;
    m_gfxCmdBufState.flags.csWriteCachesDirty        = cmdBuffer.m_gfxCmdBufState.flags.csWriteCachesDirty;
    m_gfxCmdBufState.flags.cpWriteCachesDirty        = cmdBuffer.m_gfxCmdBufState.flags.cpWriteCachesDirty;
    m_gfxCmdBufState.flags.cpMemoryWriteL2CacheStale = cmdBuffer.m_gfxCmdBufState.flags.cpMemoryWriteL2CacheStale;

    // Invalidate PM4 optimizer state on post-execute since the current command buffer state does not reflect
    // state changes from the nested command buffer. We will need to resolve the nested PM4 state onto the
    // current command buffer for this to work correctly.
    m_deCmdStream.NotifyNestedCmdBufferExecute();
}

// =====================================================================================================================
// Helper method responsible for checking if any of the stream-out buffer strides need to be updated on a pipeline
// switch.
uint8 UniversalCmdBuffer::CheckStreamOutBufferStridesOnPipelineSwitch()
{
    const auto&      chipProps = m_device.Parent()->ChipProperties();
    const auto*const pPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

    uint8 dirtySlotMask = 0;
    for (uint32 idx = 0; idx < MaxStreamOutTargets; ++idx)
    {
        const uint32 strideInBytes = (sizeof(uint32) * pPipeline->VgtStrmoutVtxStride(idx).u32All);
        const uint32 numRecords    = StreamOutNumRecords(chipProps, strideInBytes);

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

            // Mark this stream-out target slot as requiring an update.
            dirtySlotMask |= (1 << idx);

            // CE RAM will shortly be more up-to-date than the stream out table memory is, so remember that we'll
            // need to dump to GPU memory before the next Draw.
            m_streamOut.state.dirty = 1;
        }
    }

    return dirtySlotMask;
}

// =====================================================================================================================
// Helper method to upload the stream-output buffer strides into the CE RAM copy of the stream-out buffer SRD table.
uint32* UniversalCmdBuffer::UploadStreamOutBufferStridesToCeRam(
    uint8   dirtyStrideMask,    // Mask of which stream-out target slots to upload into CE RAM.
    uint32* pCeCmdSpace)
{
    PAL_ASSERT(UseCpuPathInsteadOfCeRam() == false); // This shouldn't be called unless we're using the CE RAM path!

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
        auto*const pBufferSrd = &m_streamOut.srd[idx];

        if (dirtyStrideMask & (1 << idx))
        {
            // Root command buffers and nested command buffers which have changed the stream-output bindings
            // fully know the complete stream-out SRD so we can use the "normal" path.
            pCeCmdSpace += m_cmdUtil.BuildWriteConstRam(VoidPtrInc(pBufferSrd, sizeof(uint32)),
                                                        ceRamOffset,
                                                        2,
                                                        pCeCmdSpace);
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
void UniversalCmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32            cmdBufferCount,
    ICmdBuffer*const* ppCmdBuffers)
{
    for (uint32 buf = 0; buf < cmdBufferCount; ++buf)
    {
        auto*const pCallee = static_cast<Gfx9::UniversalCmdBuffer*>(ppCmdBuffers[buf]);
        PAL_ASSERT(pCallee != nullptr);

        // Track the most recent OS paging fence value across all nested command buffers called from this one.
        m_lastPagingFence = Max(m_lastPagingFence, pCallee->LastPagingFence());

        // All user-data entries have been uploaded into CE RAM and GPU memory, so we can safely "call" the nested
        // command buffer's command streams.

        const bool exclusiveSubmit = pCallee->IsExclusiveSubmit();
        const bool allowIb2Launch = (pCallee->AllowLaunchViaIb2() &&
                                     ((pCallee->m_state.flags.containsDrawIndirect == 0)
                                     ));

        m_deCmdStream.TrackNestedEmbeddedData(pCallee->m_embeddedData.chunkList);
        m_deCmdStream.TrackNestedEmbeddedData(pCallee->m_gpuScratchMem.chunkList);
        m_deCmdStream.TrackNestedCommands(pCallee->m_deCmdStream);
        m_ceCmdStream.TrackNestedCommands(pCallee->m_ceCmdStream);

        m_deCmdStream.Call(pCallee->m_deCmdStream, exclusiveSubmit, allowIb2Launch);
        m_ceCmdStream.Call(pCallee->m_ceCmdStream, exclusiveSubmit, allowIb2Launch);

        // Callee command buffers are also able to leak any changes they made to bound user-data entries and any other
        // state back to the caller.
        LeakNestedCmdBufferState(*pCallee);
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
// When RB+ is enabled, pipelines are created per shader export format.  However, same export format possibly supports
// several down convert formats. For example, FP16_ABGR supports 8_8_8_8, 5_6_5, 1_5_5_5, 4_4_4_4, etc.  This updates
// the current RB+ PM4 image with the overridden values.
// NOTE: This is expected to be called immediately after RPM binds a graphics pipeline!
void UniversalCmdBuffer::CmdOverwriteRbPlusFormatForBlits(
    SwizzledFormat format,
    uint32         targetIndex)
{
    const auto*const pPipeline =
        static_cast<const GraphicsPipeline*>(PipelineState(PipelineBindPoint::Graphics)->pPipeline);
    PAL_ASSERT(pPipeline != nullptr);

    // Just update our PM4 image for RB+.  It will be written at draw-time along with the other pipeline registers.
    if (m_rbPlusPm4Img.spaceNeeded != 0)
    {
        pPipeline->OverrideRbPlusRegistersForRpm(format,
                                                 targetIndex,
                                                 &m_rbPlusPm4Img.sxPsDownconvert,
                                                 &m_rbPlusPm4Img.sxBlendOptEpsilon,
                                                 &m_rbPlusPm4Img.sxBlendOptControl);
    }
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
    barrierInfo.reason             = Developer::BarrierReasonP2PBlitSync;

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
// Switch draw functions - the actual assignment
template <bool ViewInstancing, bool NggFastLaunch, bool IssueSqtt>
void UniversalCmdBuffer::SwitchDrawFunctionsInternal()
{
    m_funcTable.pfnCmdDraw = CmdDraw<IssueSqtt, ViewInstancing>;
    m_funcTable.pfnCmdDrawOpaque = CmdDrawOpaque<IssueSqtt, ViewInstancing>;
    m_funcTable.pfnCmdDrawIndirectMulti = CmdDrawIndirectMulti<IssueSqtt, ViewInstancing>;
    m_funcTable.pfnCmdDrawIndexed = CmdDrawIndexed<IssueSqtt, NggFastLaunch, ViewInstancing>;
    m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<IssueSqtt, NggFastLaunch, ViewInstancing>;
}

// =====================================================================================================================
// Switch draw functions - overloaded internal implementation for switching function params to template params
template <bool NggFastLaunch, bool IssueSqtt>
void UniversalCmdBuffer::SwitchDrawFunctionsInternal(
    bool viewInstancingEnable)
{
    if (viewInstancingEnable)
    {
        SwitchDrawFunctionsInternal<true, NggFastLaunch, IssueSqtt>(
            );
    }
    else
    {
        SwitchDrawFunctionsInternal<false, NggFastLaunch, IssueSqtt>(
            );
    }
}

// =====================================================================================================================
// Switch draw functions - overloaded internal implementation for switching function params to template params
template <bool IssueSqtt>
void UniversalCmdBuffer::SwitchDrawFunctionsInternal(
    bool viewInstancingEnable,
    bool nggFastLaunch)
{
    if (nggFastLaunch)
    {
        SwitchDrawFunctionsInternal<true, IssueSqtt>(
            viewInstancingEnable);
    }
    else
    {
        SwitchDrawFunctionsInternal<false, IssueSqtt>(
            viewInstancingEnable);
    }
}

// =====================================================================================================================
// Switch draw functions.
void UniversalCmdBuffer::SwitchDrawFunctions(
    bool viewInstancingEnable,
    bool nggFastLaunch)
{
    if (m_cachedSettings.issueSqttMarkerEvent)
    {
        SwitchDrawFunctionsInternal<true>(
            viewInstancingEnable,
            nggFastLaunch);
    }
    else
    {
        SwitchDrawFunctionsInternal<false>(
            viewInstancingEnable,
            nggFastLaunch);
    }
}

// =====================================================================================================================
// Copy memory using the CP's DMA engine
void UniversalCmdBuffer::CpCopyMemory(
    gpusize dstAddr,
    gpusize srcAddr,
    gpusize numBytes)
{
    PAL_ASSERT(numBytes < (1ull << 32));

    DmaDataInfo dmaDataInfo = {};
    dmaDataInfo.dstSel      = dst_sel__pfp_dma_data__dst_addr_using_l2;
    dmaDataInfo.srcSel      = src_sel__pfp_dma_data__src_addr_using_l2;
    dmaDataInfo.sync        = false;
    dmaDataInfo.usePfp      = false;
    dmaDataInfo.predicate   = static_cast<Pm4Predicate>(GetGfxCmdBufState().flags.packetPredicate);
    dmaDataInfo.dstAddr     = dstAddr;
    dmaDataInfo.srcAddr     = srcAddr;
    dmaDataInfo.numBytes    = static_cast<uint32>(numBytes);

    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
    pCmdSpace += m_cmdUtil.BuildDmaData(dmaDataInfo, pCmdSpace);
    m_deCmdStream.CommitCommands(pCmdSpace);

    SetGfxCmdBufCpBltState(true);
    SetGfxCmdBufCpBltWriteCacheState(true);
}

} // Gfx9
} // Pal
