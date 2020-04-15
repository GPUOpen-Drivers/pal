/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "marker_payload.h"
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

// Lookup table for converting PAL primitive topologies to VGT hardware enums.
constexpr VGT_DI_PRIM_TYPE TopologyToPrimTypeTable[] =
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 557
    DI_PT_LINELOOP,         // LineLoop
    DI_PT_POLYGON,          // Polygon
#endif
};

// The DB_RENDER_OVERRIDE fields owned by the graphics pipeline.
constexpr uint32 PipelineDbRenderOverrideMask = DB_RENDER_OVERRIDE__FORCE_SHADER_Z_ORDER_MASK  |
                                                DB_RENDER_OVERRIDE__DISABLE_VIEWPORT_CLAMP_MASK;

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
    m_pipelineCtxRegHash(0),
    m_pipelineCfgRegHash(0),
    m_pfnValidateUserDataGfx(nullptr),
    m_pfnValidateUserDataGfxPipelineSwitch(nullptr),
    m_workaroundState(&device, createInfo.flags.nested, m_state),
    m_vertexOffsetReg(UserDataNotMapped),
    m_drawIndexReg(UserDataNotMapped),
    m_log2NumSes(Log2(m_device.Parent()->ChipProperties().gfx9.numShaderEngines)),
    m_log2NumRbPerSe(Log2(m_device.Parent()->ChipProperties().gfx9.maxNumRbPerSe)),
    m_pbbStateOverride(BinningOverride::Default),
    m_enabledPbb(false),
    m_customBinSizeX(0),
    m_customBinSizeY(0),
    m_activeOcclusionQueryWriteRanges(m_device.GetPlatform())
{
    const auto&                palDevice        = *(m_device.Parent());
    const PalPlatformSettings& platformSettings = m_device.Parent()->GetPlatform()->PlatformSettings();
    const PalSettings&         coreSettings     = m_device.Parent()->Settings();
    const Gfx9PalSettings&     settings         = m_device.Settings();
    const auto*const           pPublicSettings  = m_device.Parent()->GetPublicSettings();

    memset(&m_vbTable,         0, sizeof(m_vbTable));
    memset(&m_spillTable,      0, sizeof(m_spillTable));
    memset(&m_streamOut,       0, sizeof(m_streamOut));
    memset(&m_nggTable,        0, sizeof(m_nggTable));
    memset(&m_state,           0, sizeof(m_state));
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
    m_cachedSettings.pbbMoreThanOneCtxState    = (settings.binningContextStatesPerBin > 1);
    m_cachedSettings.padParamCacheSpace        =
            ((pPublicSettings->contextRollOptimizationFlags & PadParamCacheSpace) != 0);
    m_cachedSettings.disableVertGrouping       = settings.disableGeCntlVtxGrouping;

    m_cachedSettings.prefetchIndexBufferForNgg = settings.waEnableIndexBufferPrefetchForNgg;
    m_cachedSettings.waCeDisableIb2            = settings.waCeDisableIb2;
    m_cachedSettings.rbPlusSupported           = m_device.Parent()->ChipProperties().gfx9.rbPlus;

    m_cachedSettings.waUtcL0InconsistentBigPage = settings.waUtcL0InconsistentBigPage;
    m_cachedSettings.waClampGeCntlVertGrpSize   = settings.waClampGeCntlVertGrpSize;
    m_cachedSettings.ignoreDepthForBinSize      = settings.ignoreDepthForBinSizeIfColorBound;

    // Here we pre-calculate constants used in gfx10 PBB bin sizing calculations.
    // The logic is based on formulas that account for the number of RBs and Channels on the ASIC.
    // The bin size is choosen from the minimum size for Depth, Color and Fmask.
    // See usage in Gfx10GetDepthBinSize() and Gfx10GetColorBinSize() for further details.
    uint32 totalNumRbs   = m_device.Parent()->ChipProperties().gfx9.numActiveRbs;
    uint32 totalNumPipes = Max(totalNumRbs, m_device.Parent()->ChipProperties().gfx9.numSdpInterfaces);

    constexpr uint32 ZsTagSize  = 64;
    constexpr uint32 ZsNumTags  = 312;
    constexpr uint32 CcTagSize  = 1024;
    constexpr uint32 CcReadTags = 31;
    constexpr uint32 FcTagSize  = 256;
    constexpr uint32 FcReadTags = 44;

    // The logic given to calculate the Depth bin size is:
    //   depthBinArea = ((ZsReadTags * totalNumRbs / totalNumPipes) * (ZsTagSize * totalNumPipes)) / cDepth
    // After we precalculate the constant terms, the formula becomes:
    //   depthBinArea = depthBinSizeTagPart / cDepth;
    m_depthBinSizeTagPart   = ((ZsNumTags * totalNumRbs / totalNumPipes) * (ZsTagSize * totalNumPipes));

    // The logic given to calculate the Color bin size is:
    //   colorBinArea = ((CcReadTags * totalNumRbs / totalNumPipes) * (CcTagSize * totalNumPipes)) / cColor
    // After we precalculate the constant terms, the formula becomes:
    //   colorBinArea = colorBinSizeTagPart / cColor;
    m_colorBinSizeTagPart   = ((CcReadTags * totalNumRbs / totalNumPipes) * (CcTagSize * totalNumPipes));

    // The logic given to calculate the Fmask bin size is:
    //   fmaskBinArea =  ((FcReadTags * totalNumRbs / totalNumPipes) * (FcTagSize * totalNumPipes)) / cFmask
    // After we precalculate the constant terms, the formula becomes:
    //   fmaskBinArea = fmaskBinSizeTagPart / cFmask;
    m_fmaskBinSizeTagPart   = ((FcReadTags * totalNumRbs / totalNumPipes) * (FcTagSize * totalNumPipes));

    m_minBinSizeX = settings.minBatchBinSize.width;
    m_minBinSizeY = settings.minBatchBinSize.height;

    PAL_ASSERT((m_minBinSizeX != 0) && (m_minBinSizeY != 0));
    PAL_ASSERT(IsPowerOfTwo(m_minBinSizeX) && IsPowerOfTwo(m_minBinSizeY));

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
    m_cachedSettings.describeDrawDispatch =
        (m_cachedSettings.issueSqttMarkerEvent ||
         device.GetPlatform()->PlatformSettings().cmdBufferLoggerConfig.embedDrawDispatchInfo);

#if PAL_BUILD_PM4_INSTRUMENTOR
    m_cachedSettings.enablePm4Instrumentation = platformSettings.pm4InstrumentorEnabled;
#endif

    // Initialize defaults for some of the fields in PA_SC_BINNER_CNTL_0.
    m_paScBinnerCntl0.u32All                         = 0;
    m_paScBinnerCntl0.bits.CONTEXT_STATES_PER_BIN    = (settings.binningContextStatesPerBin - 1);
    m_paScBinnerCntl0.bits.PERSISTENT_STATES_PER_BIN = (settings.binningPersistentStatesPerBin - 1);
    m_paScBinnerCntl0.bits.FPOVS_PER_BATCH           = settings.binningFpovsPerBatch;
    m_paScBinnerCntl0.bits.OPTIMAL_BIN_SELECTION     = settings.binningOptimalBinSelection;

    // Hardware detects binning transitions when this is set so SW can hardcode it.
    // This has no effect unless the KMD has also set PA_SC_ENHANCE_1.FLUSH_ON_BINNING_TRANSITION=1
    if (IsGfx091xPlus(palDevice))
    {
        m_paScBinnerCntl0.gfx09_1xPlus.FLUSH_ON_BINNING_TRANSITION = 1;
    }

    // Initialize to the common value for most pipelines (no conservative rast).
    m_paScConsRastCntl.u32All                         = 0;
    m_paScConsRastCntl.bits.NULL_SQUAD_AA_MASK_ENABLE = 1;

    m_sxPsDownconvert.u32All      = 0;
    m_sxBlendOptEpsilon.u32All    = 0;
    m_sxBlendOptControl.u32All    = 0;
    m_cbRmiGl2CacheControl.u32All = 0;
    m_dbRenderOverride.u32All     = 0;
    m_paScAaConfigNew.u32All      = 0;
    m_paScAaConfigLast.u32All     = 0;

    SwitchDrawFunctions(false, false);
}

// =====================================================================================================================
// Initializes Gfx9-specific functionality.
Result UniversalCmdBuffer::Init(
    const CmdBufferInternalCreateInfo& internalInfo)
{
    const Gfx9PalSettings&   settings  = m_device.Settings();
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
    m_uavExportTable.state.sizeInDwords = (sizeof(m_uavExportTable.srd) / sizeof(uint32));
    m_uavExportTable.state.ceRamOffset  = ceRamOffset;
    ceRamOffset += sizeof(m_uavExportTable.srd);

    if (settings.nggSupported)
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
template <bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
void UniversalCmdBuffer::SetDispatchFunctions()
{
    if (UseCpuPathInsteadOfCeRam())
    {
        m_funcTable.pfnCmdDispatch         = CmdDispatch<IssueSqttMarkerEvent, true, DescribeDrawDispatch>;
        m_funcTable.pfnCmdDispatchIndirect = CmdDispatchIndirect<IssueSqttMarkerEvent, true, DescribeDrawDispatch>;
        m_funcTable.pfnCmdDispatchOffset   = CmdDispatchOffset<IssueSqttMarkerEvent, true, DescribeDrawDispatch>;
    }
    else
    {
        m_funcTable.pfnCmdDispatch         = CmdDispatch<IssueSqttMarkerEvent, false, DescribeDrawDispatch>;
        m_funcTable.pfnCmdDispatchIndirect = CmdDispatchIndirect<IssueSqttMarkerEvent, false, DescribeDrawDispatch>;
        m_funcTable.pfnCmdDispatchOffset   = CmdDispatchOffset<IssueSqttMarkerEvent, false, DescribeDrawDispatch>;
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
        SetDispatchFunctions<true, true>();
    }
    else if (m_cachedSettings.describeDrawDispatch)
    {
        SetDispatchFunctions<false, true>();
    }
    else
    {
        SetDispatchFunctions<false, false>();
    }

    SetUserDataValidationFunctions(false, false, false);
    SwitchDrawFunctions(false, false);

    m_vgtDmaIndexType.u32All = 0;
    m_vgtDmaIndexType.bits.SWAP_MODE  = VGT_DMA_SWAP_NONE;
    m_vgtDmaIndexType.bits.INDEX_TYPE = VgtIndexTypeLookup[0];

    m_leakCbColorInfoRtv   = 0;

    for (uint32 x = 0; x < MaxColorTargets; x++)
    {
        static_assert(COLOR_INVALID  == 0, "Unexpected value for COLOR_INVALID!");
        static_assert(FORCE_OPT_AUTO == 0, "Unexpected value for FORCE_OPT_AUTO!");
        m_cbColorInfo[x].u32All = 0;

        if (m_cachedSettings.blendOptimizationsEnable == false)
        {
            m_cbColorInfo[x].bits.BLEND_OPT_DONT_RD_DST   = FORCE_OPT_DISABLE;
            m_cbColorInfo[x].bits.BLEND_OPT_DISCARD_PIXEL = FORCE_OPT_DISABLE;
        }
    }

    // For IndexBuffers - default to STREAM cache policy so that they get evicted from L2 as soon as possible.
    if (IsGfx10(m_gfxIpLevel))
    {
        m_vgtDmaIndexType.gfx10.RDREQ_POLICY = VGT_POLICY_STREAM;

        const uint32 cbDbCachePolicy = m_device.Settings().cbDbCachePolicy;

        m_cbRmiGl2CacheControl.u32All               = 0;
        m_cbRmiGl2CacheControl.bits.CMASK_WR_POLICY =
            (cbDbCachePolicy & Gfx10CbDbCachePolicyLruCmask) ? CACHE_LRU_WR : CACHE_STREAM;
        m_cbRmiGl2CacheControl.bits.FMASK_WR_POLICY =
            (cbDbCachePolicy & Gfx10CbDbCachePolicyLruFmask) ? CACHE_LRU_WR : CACHE_STREAM;
        m_cbRmiGl2CacheControl.bits.DCC_WR_POLICY   =
            (cbDbCachePolicy & Gfx10CbDbCachePolicyLruDcc)   ? CACHE_LRU_WR : CACHE_STREAM;
        m_cbRmiGl2CacheControl.bits.CMASK_RD_POLICY =
            (cbDbCachePolicy & Gfx10CbDbCachePolicyLruCmask) ? CACHE_LRU_RD : CACHE_NOA;
        m_cbRmiGl2CacheControl.bits.FMASK_RD_POLICY =
            (cbDbCachePolicy & Gfx10CbDbCachePolicyLruFmask) ? CACHE_LRU_RD : CACHE_NOA;
        m_cbRmiGl2CacheControl.bits.DCC_RD_POLICY   =
            (cbDbCachePolicy & Gfx10CbDbCachePolicyLruDcc)   ? CACHE_LRU_RD : CACHE_NOA;
        m_cbRmiGl2CacheControl.bits.COLOR_RD_POLICY =
            (cbDbCachePolicy & Gfx10CbDbCachePolicyLruColor) ? CACHE_LRU_RD : CACHE_NOA;

        // If any of the bound color targets are using linear swizzle mode (or 256_S or 256_D, but PAL doesn't utilize
        // those), then COLOR_WR_POLICY can not be CACHE_BYPASS.
        m_cbRmiGl2CacheControl.bits.COLOR_WR_POLICY =
            (cbDbCachePolicy & Gfx10CbDbCachePolicyLruColor) ? CACHE_LRU_WR : CACHE_STREAM;
    }
    else
    {
        PAL_ASSERT(IsGfx9(*m_device.Parent()));
        m_vgtDmaIndexType.gfx09.RDREQ_POLICY = VGT_POLICY_STREAM;
    }

    m_spiVsOutConfig.u32All   = 0;
    m_spiPsInControl.u32All   = 0;
    m_vgtLsHsConfig.u32All    = 0;
    m_geCntl.u32All           = 0;
    m_dbDfsmControl.u32All    = m_device.GetDbDfsmControl();
    m_paScAaConfigNew.u32All  = 0;
    m_paScAaConfigLast.u32All = 0;

    // Disable PBB at the start of each command buffer unconditionally. Each draw can set the appropriate
    // PBB state at validate time.
    m_enabledPbb = false;

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
    ResetUserDataTable(&m_uavExportTable.state);

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

    m_vertexOffsetReg     = UserDataNotMapped;
    m_drawIndexReg        = UserDataNotMapped;
    m_nggState.numSamples = 1;

    m_pSignatureCs         = &NullCsSignature;
    m_pSignatureGfx        = &NullGfxSignature;
    m_pipelineCtxRegHash   = 0;
    m_pipelineCfgRegHash   = 0;
    m_pipelinePsHash.lower = 0;
    m_pipelinePsHash.upper = 0;
    m_pipelineFlags.u32All = 0;

#if PAL_ENABLE_PRINTS_ASSERTS
    m_pipelineFlagsValid = false;
#endif

    // Set this flag at command buffer Begin/Reset, in case the last draw of the previous chained command buffer has
    // rasterization killed.
    m_pipelineFlags.noRaster = 1;

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

        const bool newUsesViewInstancing  = (pNewPipeline != nullptr) && pNewPipeline->UsesViewInstancing();
        const bool oldUsesViewInstancing  = (pOldPipeline != nullptr) && pOldPipeline->UsesViewInstancing();
        const bool newUsesUavExport       = (pNewPipeline != nullptr) && pNewPipeline->UsesUavExport();
        const bool oldUsesUavExport       = (pOldPipeline != nullptr) && pOldPipeline->UsesUavExport();
        const bool newNeedsUavExportFlush = (pNewPipeline != nullptr) && pNewPipeline->NeedsUavExportFlush();
        const bool oldNeedsUavExportFlush = (pOldPipeline != nullptr) && pOldPipeline->NeedsUavExportFlush();

        if ((oldNeedsUavExportFlush != newNeedsUavExportFlush) ||
            (oldUsesViewInstancing  != newUsesViewInstancing))
        {
            SwitchDrawFunctions(newNeedsUavExportFlush, newUsesViewInstancing);
        }

        // If RB+ is enabled, we must update the PM4 image of RB+ register state with the new pipelines' values.  This
        // should be done here instead of inside SwitchGraphicsPipeline() because RPM sometimes overrides these values
        // for certain blit operations.
        if ((m_cachedSettings.rbPlusSupported != 0) && (pNewPipeline != nullptr))
        {
            m_sxPsDownconvert   = pNewPipeline->SxPsDownconvert();
            m_sxBlendOptEpsilon = pNewPipeline->SxBlendOptEpsilon();
            m_sxBlendOptControl = pNewPipeline->SxBlendOptControl();
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
        if (newUsesUavExport)
        {
            const uint32 maxTargets = static_cast<const GraphicsPipeline*>(params.pPipeline)->NumColorTargets();
            m_uavExportTable.maxColorTargets = maxTargets;
            m_uavExportTable.tableSizeDwords = NumBytesToNumDwords(maxTargets * sizeof(ImageSrd));

            if (oldUsesUavExport == false)
            {
                // Invalidate color caches so upcoming uav exports don't overlap previous normal exports
                uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
                pDeCmdSpace += m_cmdUtil.BuildWaitOnReleaseMemEvent(EngineTypeUniversal,
                                                                    CACHE_FLUSH_AND_INV_TS_EVENT,
                                                                    TcCacheOp::Nop,
                                                                    TimestampGpuVirtAddr(),
                                                                    pDeCmdSpace);
                m_deCmdStream.CommitCommands(pDeCmdSpace);
            }
        }

        if ((pNewPipeline == nullptr) || (pOldPipeline == nullptr) ||
            (pNewPipeline->CbTargetMask().u32All != pOldPipeline->CbTargetMask().u32All))
        {
            m_state.flags.cbTargetMaskChanged = true;
        }

        // Pipeline owns COVERAGE_TO_SHADER_SELECT
        m_paScAaConfigNew.bits.COVERAGE_TO_SHADER_SELECT =
            (pNewPipeline == nullptr) ? 0 : pNewPipeline->PaScAaConfig().bits.COVERAGE_TO_SHADER_SELECT;
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
    const bool wasPrevPipelineNgg  = m_pipelineFlags.isNgg;
    const bool isNgg               = pCurrPipeline->IsNgg();
    const bool tessEnabled         = pCurrPipeline->IsTessEnabled();
    const bool gsEnabled           = pCurrPipeline->IsGsEnabled();
    const bool isRasterKilled      = pCurrPipeline->IsRasterizationKilled();

    const uint32 ctxRegHash = pCurrPipeline->GetContextRegHash();
    if (wasPrevPipelineNull || (m_pipelineCtxRegHash != ctxRegHash))
    {
        pDeCmdSpace = pCurrPipeline->WriteContextCommands(&m_deCmdStream, pDeCmdSpace);
        m_deCmdStream.SetContextRollDetected<true>();

        m_pipelineCtxRegHash = ctxRegHash;
    }

    // Only gfx10+ pipelines need to set config registers.
    if (IsGfx10(m_gfxIpLevel))
    {
        const uint32 cfgRegHash = pCurrPipeline->GetConfigRegHash();
        if (wasPrevPipelineNull || (m_pipelineCfgRegHash != cfgRegHash))
        {
            pDeCmdSpace = pCurrPipeline->WriteConfigCommandsGfx10(&m_deCmdStream, pDeCmdSpace);
            m_pipelineCfgRegHash = cfgRegHash;
        }
    }

    if (m_cachedSettings.rbPlusSupported != 0)
    {
        pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmSX_PS_DOWNCONVERT,
                                                           mmSX_BLEND_OPT_CONTROL,
                                                           &m_sxPsDownconvert,
                                                           pDeCmdSpace);
        m_deCmdStream.SetContextRollDetected<true>();
    }

    bool breakBatch = ((m_cachedSettings.pbbMoreThanOneCtxState) && (m_state.flags.cbTargetMaskChanged));
    m_state.flags.cbTargetMaskChanged = false;

    if ((m_cachedSettings.batchBreakOnNewPs) && (breakBatch == false))
    {
        const ShaderHash& psHash = pCurrPipeline->GetInfo().shader[static_cast<uint32>(ShaderType::Pixel)].hash;
        if (wasPrevPipelineNull || (ShaderHashesEqual(m_pipelinePsHash, psHash) == false))
        {
            m_pipelinePsHash = psHash;
            breakBatch = true;
        }
    }

    if (breakBatch)
    {
        pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(BREAK_BATCH, EngineTypeUniversal, pDeCmdSpace);
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
    }

    if (m_drawIndexReg != m_pSignatureGfx->drawIndexRegAddr)
    {
        m_drawIndexReg = m_pSignatureGfx->drawIndexRegAddr;
        if (m_drawIndexReg != UserDataNotMapped)
        {
            m_drawTimeHwState.valid.drawIndex = 0;
        }
    }

    if (wasPrevPipelineNgg && (isNgg == false))
    {
        pDeCmdSpace = m_workaroundState.SwitchFromNggPipelineToLegacy(gsEnabled, pDeCmdSpace);
    }

    // Save the set of pipeline flags for the next pipeline transition.  This should come last because the previous
    // pipelines' values are used earlier in the function.
    m_pipelineFlags.isNgg    = isNgg;
    m_pipelineFlags.usesTess = tessEnabled;
    m_pipelineFlags.usesGs   = gsEnabled;
    m_pipelineFlags.noRaster = isRasterKilled;

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

    // MsaaQuadSamplePattern owns MAX_SAMPLE_DIST
    m_paScAaConfigNew.bits.MAX_SAMPLE_DIST = MsaaState::ComputeMaxSampleDistance(numSamplesPerPixel, quadSamplePattern);

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = MsaaState::WriteSamplePositions(quadSamplePattern, numSamplesPerPixel, &m_deCmdStream, pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetViewports(
    const ViewportParams& params)
{
    const size_t     viewportSize  = (sizeof(params.viewports[0]) * params.count);
    constexpr size_t GuardbandSize = (sizeof(float) * 4);

    m_graphicsState.viewportState.count      = params.count;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 524
    m_graphicsState.viewportState.depthRange = params.depthRange;
#endif

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
    m_workaroundState.HandleZeroIndexBuffer(this, &gpuAddr, &indexCount);

    if (m_graphicsState.iaState.indexAddr != gpuAddr)
    {
        m_drawTimeHwState.dirty.indexBufferBase     = 1;
        m_drawTimeHwState.nggIndexBufferPfStartAddr = 0;
        m_drawTimeHwState.nggIndexBufferPfEndAddr   = 0;
    }

    if (m_graphicsState.iaState.indexCount != indexCount)
    {
        m_drawTimeHwState.dirty.indexBufferSize = 1;
    }

    if (m_graphicsState.iaState.indexType != indexType)
    {
        m_drawTimeHwState.dirty.indexType = 1;
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

        // MSAA State owns MSAA_EXPOSED_SAMPLES and AA_MASK_CENTROID_DTMN
        m_paScAaConfigNew.u32All = ((m_paScAaConfigNew.u32All         & (~MsaaState::PcScAaConfigMask)) |
                                    (pNewState->PaScAaConfig().u32All &   MsaaState::PcScAaConfigMask));
    }
    else
    {
        m_nggState.numSamples    = 1;
        m_paScAaConfigNew.u32All = (m_paScAaConfigNew.u32All & (~MsaaState::PcScAaConfigMask));
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

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmCB_BLEND_RED,
                                                       mmCB_BLEND_ALPHA,
                                                       &params.blendConst[0],
                                                       pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
    m_deCmdStream.SetContextRollDetected<true>();
}

// =====================================================================================================================
// Sets depth bounds to be applied with depth buffer comparisons
void UniversalCmdBuffer::CmdSetDepthBounds(
    const DepthBoundsParams& params)
{
    m_graphicsState.depthBoundsState                              = params;
    m_graphicsState.dirtyFlags.nonValidationBits.depthBoundsState = 1;

    const float depthBounds[2] = { params.min, params.max, };
    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmDB_DEPTH_BOUNDS_MIN,
                                                       mmDB_DEPTH_BOUNDS_MAX,
                                                       &depthBounds[0],
                                                       pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
    m_deCmdStream.SetContextRollDetected<true>();
}

// =====================================================================================================================
// Sets the current input assembly state
void UniversalCmdBuffer::CmdSetInputAssemblyState(
    const InputAssemblyStateParams& params)
{
    regVGT_PRIMITIVE_TYPE vgtPrimitiveType = { };
    vgtPrimitiveType.bits.PRIM_TYPE = TopologyToPrimTypeTable[static_cast<uint32>(params.topology)];

    regVGT_MULTI_PRIM_IB_RESET_INDX vgtMultiPrimIbResetIndx = { };
    vgtMultiPrimIbResetIndx.bits.RESET_INDX = params.primitiveRestartIndex;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    {
        pDeCmdSpace = m_deCmdStream.WriteSetOneConfigReg(mmVGT_PRIMITIVE_TYPE,
                                                         vgtPrimitiveType.u32All,
                                                         pDeCmdSpace,
                                                         index__pfp_set_uconfig_reg_index__prim_type__GFX09);
    }

    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmVGT_MULTI_PRIM_IB_RESET_INDX,
                                                      vgtMultiPrimIbResetIndx.u32All,
                                                      pDeCmdSpace);

    m_deCmdStream.CommitCommands(pDeCmdSpace);

    m_graphicsState.inputAssemblyState = params;
    m_graphicsState.dirtyFlags.validationBits.inputAssemblyState   = 1;

    m_state.primShaderCbLayout.renderStateCb.primitiveRestartIndex = params.primitiveRestartIndex;
    m_state.primShaderCbLayout.pipelineStateCb.vgtPrimitiveType    = vgtPrimitiveType.u32All;
    m_nggState.flags.dirty.inputAssemblyState                      = 1;
}

// =====================================================================================================================
// Sets bit-masks to be applied to stencil buffer reads and writes.
void UniversalCmdBuffer::CmdSetStencilRefMasks(
    const StencilRefMaskParams& params)
{
    SetStencilRefMasksState(params, &m_graphicsState.stencilRefMaskState);
    m_graphicsState.dirtyFlags.nonValidationBits.stencilRefMaskState = 1;

    struct
    {
        regDB_STENCILREFMASK     front;
        regDB_STENCILREFMASK_BF  back;
    } dbStencilRefMask = { };

    dbStencilRefMask.front.bits.STENCILOPVAL       = params.frontOpValue;
    dbStencilRefMask.front.bits.STENCILTESTVAL     = params.frontRef;
    dbStencilRefMask.front.bits.STENCILMASK        = params.frontReadMask;
    dbStencilRefMask.front.bits.STENCILWRITEMASK   = params.frontWriteMask;
    dbStencilRefMask.back.bits.STENCILOPVAL_BF     = params.backOpValue;
    dbStencilRefMask.back.bits.STENCILTESTVAL_BF   = params.backRef;
    dbStencilRefMask.back.bits.STENCILMASK_BF      = params.backReadMask;
    dbStencilRefMask.back.bits.STENCILWRITEMASK_BF = params.backWriteMask;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    if (params.flags.u8All == 0xFF)
    {
        pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmDB_STENCILREFMASK,
                                                           mmDB_STENCILREFMASK_BF,
                                                           &dbStencilRefMask,
                                                           pDeCmdSpace);
    }
    else
    {
        // Accumulate masks and shifted data based on which flags are set
        // 1. Front-facing primitives
        uint32 frontMask = 0;
        if (params.flags.updateFrontRef)
        {
            frontMask |= DB_STENCILREFMASK__STENCILTESTVAL_MASK;
        }
        if (params.flags.updateFrontReadMask)
        {
            frontMask |= DB_STENCILREFMASK__STENCILMASK_MASK;
        }
        if (params.flags.updateFrontWriteMask)
        {
            frontMask |= DB_STENCILREFMASK__STENCILWRITEMASK_MASK;
        }
        if (params.flags.updateFrontOpValue)
        {
            frontMask |= DB_STENCILREFMASK__STENCILOPVAL_MASK;
        }

        // 2. Back-facing primitives
        uint32 backMask = 0;
        if (params.flags.updateBackRef)
        {
            backMask |= DB_STENCILREFMASK_BF__STENCILTESTVAL_BF_MASK;
        }
        if (params.flags.updateBackReadMask)
        {
            backMask |= DB_STENCILREFMASK_BF__STENCILMASK_BF_MASK;
        }
        if (params.flags.updateBackWriteMask)
        {
            backMask |= DB_STENCILREFMASK_BF__STENCILWRITEMASK_BF_MASK;
        }
        if (params.flags.updateBackOpValue)
        {
            backMask |= DB_STENCILREFMASK_BF__STENCILOPVAL_BF_MASK;
        }

        pDeCmdSpace = m_deCmdStream.WriteContextRegRmw(mmDB_STENCILREFMASK,
                                                       frontMask,
                                                       dbStencilRefMask.front.u32All,
                                                       pDeCmdSpace);
        pDeCmdSpace = m_deCmdStream.WriteContextRegRmw(mmDB_STENCILREFMASK_BF,
                                                       backMask,
                                                       dbStencilRefMask.back.u32All,
                                                       pDeCmdSpace);
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);
    m_deCmdStream.SetContextRollDetected<true>();
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

    // Mark these as traditional barriers in RGP
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 504
    m_device.DescribeBarrierStart(this, releaseInfo.reason, Developer::BarrierType::Release);
#else
    m_device.DescribeBarrierStart(this, Developer::BarrierReasonUnknown, Developer::BarrierType::Release);
#endif
    Developer::BarrierOperations barrierOps = {};
    m_device.BarrierRelease(this, &m_deCmdStream, releaseInfo, pGpuEvent, &barrierOps);
    m_device.DescribeBarrierEnd(this, &barrierOps);

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

    // Mark these as traditional barriers in RGP
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 504
    m_device.DescribeBarrierStart(this, acquireInfo.reason, Developer::BarrierType::Acquire);
#else
    m_device.DescribeBarrierStart(this, Developer::BarrierReasonUnknown, Developer::BarrierType::Acquire);
#endif
    Developer::BarrierOperations barrierOps = {};
    m_device.BarrierAcquire(this, &m_deCmdStream, acquireInfo, gpuEventCount, ppGpuEvents, &barrierOps);
    m_device.DescribeBarrierEnd(this, &barrierOps);

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

    // Mark these as traditional barriers in RGP
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 504
    m_device.DescribeBarrierStart(this, barrierInfo.reason, Developer::BarrierType::Full);
#else
    m_device.DescribeBarrierStart(this, Developer::BarrierReasonUnknown, Developer::BarrierType::Full);
#endif
    Developer::BarrierOperations barrierOps = {};
    m_device.BarrierReleaseThenAcquire(this, &m_deCmdStream, barrierInfo, &barrierOps);
    m_device.DescribeBarrierEnd(this, &barrierOps);

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
    // BIG_PAGE can only be enabled if all render targets are compatible.  Default to true and disable it later if we
    // find an incompatible target.
    bool   colorBigPage  = true;
    bool   fmaskBigPage  = true;

    bool validCbViewFound   = false;
    bool validAaCbViewFound = false;

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

        if (pNewView != nullptr)
        {
            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
            pDeCmdSpace = pNewView->WriteCommands(slot,
                                                  params.colorTargets[slot].imageLayout,
                                                  &m_deCmdStream,
                                                  pDeCmdSpace,
                                                  &(m_cbColorInfo[slot]));
            m_deCmdStream.CommitCommands(pDeCmdSpace);

            if (validCbViewFound == false)
            {
                // For MRT case, extents must match across all MRTs.
                surfaceExtent = pNewView->GetExtent();
            }

            // Set the bit means this color target slot is not bound to a NULL target.
            newColorTargetMask |= (1 << slot);

            const auto* pImage        = pNewView->GetImage();
            auto*       pGfx10NewView = static_cast<const Gfx10ColorTargetView*>(pNewView);

            if (IsGfx10(m_gfxIpLevel) && (pImage != nullptr))
            {
                colorBigPage &= pGfx10NewView->IsColorBigPage();

                // There is a shared bit to enable the BIG_PAGE optimization for all targets.  If this image doesn't
                // have fmask we should leave the accumulated fmaskBigPage state alone so other render targets that
                // do have fmask can still get the optimization.
                if (pImage->HasFmaskData())
                {
                    fmaskBigPage      &= pGfx10NewView->IsFmaskBigPage();
                    validAaCbViewFound = true;
                }
            }
            else
            {
                colorBigPage = false;
                fmaskBigPage = false;
            }

            validCbViewFound = true;
            m_state.flags.cbColorInfoDirtyRtv |= (1 << slot);
        }

        if (pCurrentView != pNewView)
        {
            if (pCurrentView != nullptr) // view1->view2 or view->null
            {
                colorTargetsChanged = true;
                // Record if this depth view we are switching from should trigger a Release_Mem due to being in the
                // MetaData tail region.
                waitOnMetadataMipTail |= pCurrentView->WaitOnMetadataMipTail();
            }
        }
    }

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    // Bind NULL for all remaining color target slots.
    if (newColorTargetMask != AllColorTargetSlotMask)
    {
        WriteNullColorTargets(newColorTargetMask, m_graphicsState.boundColorTargetMask);
    }
    m_graphicsState.boundColorTargetMask = newColorTargetMask;

    if (colorTargetsChanged)
    {
        // Handle the case where at least one color target view is changing.
        pDeCmdSpace = ColorTargetView::HandleBoundTargetsChanged(pDeCmdSpace);
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

        const TargetExtent2d depthViewExtent = pNewDepthView->GetExtent();
        surfaceExtent.width  = Min(surfaceExtent.width,  depthViewExtent.width);
        surfaceExtent.height = Min(surfaceExtent.height, depthViewExtent.height);

        // Re-write the ZRANGE_PRECISION value for the waTcCompatZRange workaround. We must include the
        // COND_EXEC which checks the metadata because we don't know the last fast clear value here.
        pDeCmdSpace = pNewDepthView->UpdateZRangePrecision(true, &m_deCmdStream, pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = WriteNullDepthTarget(pDeCmdSpace);
    }

    // view1->view2 or view->null
    const bool depthTargetChanged = ((pCurrentDepthView != nullptr) && (pCurrentDepthView != pNewDepthView));

    if (depthTargetChanged)
    {
        // Handle the case where the depth view is changing.
        pDeCmdSpace = DepthStencilView::HandleBoundTargetChanged(pDeCmdSpace);

        // Record if this depth view we are switching from should trigger a Release_Mem due to being in the MetaData
        // tail region.
        waitOnMetadataMipTail |= pCurrentDepthView->WaitOnMetadataMipTail();
    }

    if (((!m_cachedSettings.disableDfsm) & colorTargetsChanged) |
        (m_cachedSettings.pbbMoreThanOneCtxState & (colorTargetsChanged | depthTargetChanged)))
    {
        // If the slice-index as programmed by the CB is changing, then we have to flush DFSM stuff. This isn't
        // necessary if DFSM is disabled.
        //
        // ("it" refers to the RT-index, the HW perspective of which slice is being rendered to. The RT-index is
        //  a combination of the CB registers and the GS output).
        //
        //  If the GS (HW VS) is changing it, then there is only one view, so no batch break is needed..  If any
        //  of the RT views are changing, the DFSM has no idea about it and there isn't any one single RT_index
        //  to keep track of since each RT may have a different view with different STARTs and SIZEs that can be
        //  independently changing.  The DB and Scan Converter also doesn't know about the CB's views changing.
        //  This is why there should be a batch break on RT view changes.  The other reason is that binning and
        //  deferred shading can't give any benefit when the bound RT views of consecutive contexts are not
        //  intersecting.  There is no way to increase cache hit ratios if there is no way to generate the same
        //  address between draws, so there is no reason to enable binning.
        pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(BREAK_BATCH, EngineTypeUniversal, pDeCmdSpace);
    }

    if (waitOnMetadataMipTail)
    {
        pDeCmdSpace += m_cmdUtil.BuildWaitOnReleaseMemEvent(EngineTypeUniversal,
                                                            BOTTOM_OF_PIPE_TS,
                                                            TcCacheOp::Nop,
                                                            TimestampGpuVirtAddr(),
                                                            pDeCmdSpace);
    }

    // If next draw(s) that only change D/S targets, don't program CB_RMI_GL2_CACHE_CONTROL and let the state remains.
    // This is especially necessary for following HW bug WA. If client driver disable big page feature completely, then
    // the sync will still be issued for following case without this tweaking:
    // 1. Client draw to RT[0] (color big_page disable)
    // 2. Client clear DS surf (color big_page enable because no MRT is actually bound)
    // 3. Client draw to RT[0] (color big_page disable)
    // By old logic, the sync will be added between both #1/#2 and #2/#3. The sync added for #1/#2 is unnecessary and it
    // will cause minor CPU and CP performance drop; sync added for #2/#3 will do more than that by draining the whole
    // 3D pipeline, and is completely wrong behavior.
    if (IsGfx10(m_gfxIpLevel) && validCbViewFound)
    {
        if (m_cachedSettings.waUtcL0InconsistentBigPage &&
            ((static_cast<bool>(m_cbRmiGl2CacheControl.bits.COLOR_BIG_PAGE) != colorBigPage) ||
             ((static_cast<bool>(m_cbRmiGl2CacheControl.bits.FMASK_BIG_PAGE) != fmaskBigPage) && validAaCbViewFound)))
        {
            // For following case, BIG_PAGE bit polarity changes between #A/#B and #C/#D, and we will need to add sync
            // A. Draw to RT[0] (big_page enable)
            // B. Draw to RT[0] + RT[1] (big_page disable due to RT[1] is not big page compatible)
            // C. Draw to RT[0] + RT[1] (big_page disable due to RT[1] is not big page compatible)
            // D. Draw to RT[0] (big_page enable)
            // For simplicity, we don't track big page setting polarity change based on MRT usage, but simply adding the
            // sync whenever a different big page setting value is going to be written into command buffer.
            AcquireMemInfo acquireInfo = {};
            acquireInfo.baseAddress          = FullSyncBaseAddr;
            acquireInfo.sizeBytes            = FullSyncSize;
            acquireInfo.engineType           = EngineTypeUniversal;
            acquireInfo.cpMeCoherCntl.u32All = CpMeCoherCntlStallMask;
            acquireInfo.flags.wbInvCbData    = 1;

            // This alert shouldn't be triggered frequently, or otherwise performance penalty will be there.
            // Consider either of following solutions to avoid the performance penalty:
            // - Enable "big page" for RT/MSAA resource, as many as possible
            // - Disable "big page" for RT/MSAA resource, as many as possible
            // Check IsColorBigPage()/IsFmaskBigPage() for the details about how to enable/disable big page
            PAL_ALERT_ALWAYS();

            pDeCmdSpace += m_cmdUtil.BuildAcquireMem(acquireInfo, pDeCmdSpace);
        }

        m_cbRmiGl2CacheControl.bits.COLOR_BIG_PAGE = colorBigPage;

        // Similar to "validCbViewFound" check, only update fmaskBigPage setting if next draw(s) really use fmask
        if (validAaCbViewFound)
        {
            m_cbRmiGl2CacheControl.bits.FMASK_BIG_PAGE = fmaskBigPage;
        }

        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(Gfx10::mmCB_RMI_GL2_CACHE_CONTROL,
                                                          m_cbRmiGl2CacheControl.u32All,
                                                          pDeCmdSpace);
    }

    if (surfaceExtent.value != m_graphicsState.targetExtent.value)
    {
        m_graphicsState.targetExtent.value = surfaceExtent.value;

        struct
        {
            regPA_SC_SCREEN_SCISSOR_TL tl;
            regPA_SC_SCREEN_SCISSOR_BR br;
        } paScScreenScissor = { };

        paScScreenScissor.br.bits.BR_X = surfaceExtent.width;
        paScScreenScissor.br.bits.BR_Y = surfaceExtent.height;

        pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmPA_SC_SCREEN_SCISSOR_TL,
                                                           mmPA_SC_SCREEN_SCISSOR_BR,
                                                           &paScScreenScissor,
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
            else if (IsGfx10(m_gfxIpLevel))
            {
                auto*const  pSrd = &pBufferSrd->gfx10;

                pSrd->add_tid_enable = 0;
                pSrd->most.format    = BUF_FMT_32_UINT;
                pSrd->oob_select     = SQ_OOB_INDEX_ONLY;
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
// Sets parameters controlling triangle rasterization.
void UniversalCmdBuffer::CmdSetTriangleRasterState(
    const TriangleRasterStateParams& params)
{
    CmdSetTriangleRasterStateInternal(params, false);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetTriangleRasterStateInternal(
    const TriangleRasterStateParams& params,
    bool                             optimizeLinearDestGfxCopy)
{
    m_state.flags.optimizeLinearGfxCpy                               = optimizeLinearDestGfxCopy;
    m_graphicsState.triangleRasterState                              = params;
    m_graphicsState.dirtyFlags.validationBits.triangleRasterState    = 1;
    m_nggState.flags.dirty.triangleRasterState                       = 1;

    regPA_SU_SC_MODE_CNTL paSuScModeCntl = { };
    paSuScModeCntl.bits.POLY_OFFSET_FRONT_ENABLE = params.flags.depthBiasEnable;
    paSuScModeCntl.bits.POLY_OFFSET_BACK_ENABLE  = params.flags.depthBiasEnable;
    paSuScModeCntl.bits.MULTI_PRIM_IB_ENA        = 1;

    static_assert(
        static_cast<uint32>(FillMode::Points)    == 0 &&
        static_cast<uint32>(FillMode::Wireframe) == 1 &&
        static_cast<uint32>(FillMode::Solid)     == 2,
        "FillMode vs. PA_SU_SC_MODE_CNTL.POLY_MODE mismatch");

    if (static_cast<TossPointMode>(m_cachedSettings.tossPointMode) == TossPointWireframe)
    {
        m_graphicsState.triangleRasterState.frontFillMode = FillMode::Wireframe;
        m_graphicsState.triangleRasterState.backFillMode  = FillMode::Wireframe;

        paSuScModeCntl.bits.POLY_MODE            = 1;
        paSuScModeCntl.bits.POLYMODE_BACK_PTYPE  = static_cast<uint32>(FillMode::Wireframe);
        paSuScModeCntl.bits.POLYMODE_FRONT_PTYPE = static_cast<uint32>(FillMode::Wireframe);
    }
    else
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 524
        paSuScModeCntl.bits.POLY_MODE            = (params.fillMode != FillMode::Solid);
        paSuScModeCntl.bits.POLYMODE_BACK_PTYPE  = static_cast<uint32>(params.fillMode);
        paSuScModeCntl.bits.POLYMODE_FRONT_PTYPE = static_cast<uint32>(params.fillMode);
#else
        paSuScModeCntl.bits.POLY_MODE            = ((params.frontFillMode != FillMode::Solid) ||
                                                    (params.backFillMode  != FillMode::Solid));
        paSuScModeCntl.bits.POLYMODE_BACK_PTYPE  = static_cast<uint32>(params.backFillMode);
        paSuScModeCntl.bits.POLYMODE_FRONT_PTYPE = static_cast<uint32>(params.frontFillMode);
#endif
    }

    constexpr uint32 FrontCull = static_cast<uint32>(CullMode::Front);
    constexpr uint32 BackCull  = static_cast<uint32>(CullMode::Back);

    static_assert((FrontCull | BackCull) == static_cast<uint32>(CullMode::FrontAndBack),
        "CullMode::FrontAndBack not a strict union of CullMode::Front and CullMode::Back");

    if (static_cast<TossPointMode>(m_cachedSettings.tossPointMode) == TossPointBackFrontFaceCull)
    {
        m_graphicsState.triangleRasterState.cullMode = CullMode::FrontAndBack;

        paSuScModeCntl.bits.CULL_FRONT = 1;
        paSuScModeCntl.bits.CULL_BACK  = 1;
    }
    else
    {
        paSuScModeCntl.bits.CULL_FRONT = ((static_cast<uint32>(params.cullMode) & FrontCull) != 0);
        paSuScModeCntl.bits.CULL_BACK  = ((static_cast<uint32>(params.cullMode) & BackCull)  != 0);
    }

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

    m_state.primShaderCbLayout.pipelineStateCb.paSuScModeCntl = paSuScModeCntl.u32All;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmPA_SU_SC_MODE_CNTL, paSuScModeCntl.u32All, pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Sets parameters controlling point and line rasterization.
void UniversalCmdBuffer::CmdSetPointLineRasterState(
    const PointLineRasterStateParams& params)
{
    m_graphicsState.pointLineRasterState                              = params;
    m_graphicsState.dirtyFlags.nonValidationBits.pointLineRasterState = 1;

    // Point radius and line width are in 4-bit sub-pixel precision
    constexpr float  HalfSizeInSubPixels = 8.0f;
    constexpr uint32 MaxPointRadius      = USHRT_MAX;
    constexpr uint32 MaxLineWidth        = USHRT_MAX;

    const uint32 pointRadius    = Min(static_cast<uint32>(params.pointSize * HalfSizeInSubPixels), MaxPointRadius);
    const uint32 pointRadiusMin = Min(static_cast<uint32>(params.pointSizeMin * HalfSizeInSubPixels), MaxPointRadius);
    const uint32 pointRadiusMax = Min(static_cast<uint32>(params.pointSizeMax * HalfSizeInSubPixels), MaxPointRadius);
    const uint32 lineWidthHalf  = Min(static_cast<uint32>(params.lineWidth * HalfSizeInSubPixels), MaxLineWidth);

    struct
    {
        regPA_SU_POINT_SIZE    paSuPointSize;
        regPA_SU_POINT_MINMAX  paSuPointMinMax;
        regPA_SU_LINE_CNTL     paSuLineCntl;
    } regs = { };

    regs.paSuPointSize.bits.WIDTH      = pointRadius;
    regs.paSuPointSize.bits.HEIGHT     = pointRadius;
    regs.paSuPointMinMax.bits.MIN_SIZE = pointRadiusMin;
    regs.paSuPointMinMax.bits.MAX_SIZE = pointRadiusMax;
    regs.paSuLineCntl.bits.WIDTH       = lineWidthHalf;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmPA_SU_POINT_SIZE,
                                                       mmPA_SU_LINE_CNTL,
                                                       &regs,
                                                       pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
    m_deCmdStream.SetContextRollDetected<true>();
}

// =====================================================================================================================
// Sets depth bias parameters.
void UniversalCmdBuffer::CmdSetDepthBiasState(
    const DepthBiasParams& params)
{
    m_graphicsState.depthBiasState                              = params;
    m_graphicsState.dirtyFlags.nonValidationBits.depthBiasState = 1;

    struct
    {
        regPA_SU_POLY_OFFSET_CLAMP        paSuPolyOffsetClamp;
        regPA_SU_POLY_OFFSET_FRONT_SCALE  paSuPolyOffsetFrontScale;
        regPA_SU_POLY_OFFSET_FRONT_OFFSET paSuPolyOffsetFrontOffset;
        regPA_SU_POLY_OFFSET_BACK_SCALE   paSuPolyOffsetBackScale;
        regPA_SU_POLY_OFFSET_BACK_OFFSET  paSuPolyOffsetBackOffset;
    } regs = { };

    // NOTE: HW applies a factor of 1/16th to the Z gradients which we must account for.
    constexpr float HwOffsetScaleMultiplier = 16.0f;
    const float slopeScaleDepthBias = (params.slopeScaledDepthBias * HwOffsetScaleMultiplier);

    regs.paSuPolyOffsetClamp.f32All       = params.depthBiasClamp;
    regs.paSuPolyOffsetFrontScale.f32All  = slopeScaleDepthBias;
    regs.paSuPolyOffsetBackScale.f32All   = slopeScaleDepthBias;
    regs.paSuPolyOffsetFrontOffset.f32All = static_cast<float>(params.depthBias);
    regs.paSuPolyOffsetBackOffset.f32All  = static_cast<float>(params.depthBias);

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmPA_SU_POLY_OFFSET_CLAMP,
                                                       mmPA_SU_POLY_OFFSET_BACK_OFFSET,
                                                       &regs,
                                                       pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
    m_deCmdStream.SetContextRollDetected<true>();
}

// =====================================================================================================================
// Sets global scissor rectangle params.
void UniversalCmdBuffer::CmdSetGlobalScissor(
    const GlobalScissorParams& params)
{
    m_graphicsState.globalScissorState                              = params;
    m_graphicsState.dirtyFlags.nonValidationBits.globalScissorState = 1;

    struct
    {
        regPA_SC_WINDOW_SCISSOR_TL tl;
        regPA_SC_WINDOW_SCISSOR_BR br;
    } paScWindowScissor = { };

    const uint32 left   = params.scissorRegion.offset.x;
    const uint32 top    = params.scissorRegion.offset.y;
    const uint32 right  = params.scissorRegion.offset.x + params.scissorRegion.extent.width;
    const uint32 bottom = params.scissorRegion.offset.y + params.scissorRegion.extent.height;

    paScWindowScissor.tl.bits.WINDOW_OFFSET_DISABLE = 1;
    paScWindowScissor.tl.bits.TL_X = Clamp<uint32>(left,   0, ScissorMaxTL);
    paScWindowScissor.tl.bits.TL_Y = Clamp<uint32>(top,    0, ScissorMaxTL);
    paScWindowScissor.br.bits.BR_X = Clamp<uint32>(right,  0, ScissorMaxBR);
    paScWindowScissor.br.bits.BR_Y = Clamp<uint32>(bottom, 0, ScissorMaxBR);

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmPA_SC_WINDOW_SCISSOR_TL,
                                                       mmPA_SC_WINDOW_SCISSOR_BR,
                                                       &paScWindowScissor,
                                                       pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
    m_deCmdStream.SetContextRollDetected<true>();
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
          bool HasUavExport,
          bool ViewInstancingEnable,
          bool DescribeDrawDispatch>
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
    if (DescribeDrawDispatch)
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
    if (HasUavExport)
    {
        pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(PS_PARTIAL_FLUSH, EngineTypeUniversal, pDeCmdSpace);
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
          bool HasUavExport,
          bool ViewInstancingEnable,
          bool DescribeDrawDispatch>
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
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawOpaque);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace += pThis->m_cmdUtil.BuildLoadContextRegsIndex<true>(streamOutFilledSizeVa,
                                                                    mmVGT_STRMOUT_DRAW_OPAQUE_BUFFER_FILLED_SIZE,
                                                                    1,
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
    if (HasUavExport)
    {
        pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(PS_PARTIAL_FLUSH, EngineTypeUniversal, pDeCmdSpace);
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
          bool HasUavExport,
          bool ViewInstancingEnable,
          bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawIndexed(
    ICmdBuffer* pCmdBuffer,
    uint32      firstIndex,
    uint32      indexCount,
    int32       vertexOffset,
    uint32      firstInstance,
    uint32      instanceCount)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    // The "validIndexCount" (set later in the code) will eventually be used to program the max_size
    // field in the draw packet, which is used to clamp how much of the index buffer can be read.
    //
    // If the firstIndex parameter of the draw command is greater than the currently IB's indexCount,
    // the validIndexCount will underflow and end up way too big.
    pThis->m_workaroundState.HandleFirstIndexSmallerThanIndexCount(&firstIndex,
                                                                   pThis->m_graphicsState.iaState.indexCount);

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
    if (DescribeDrawDispatch)
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
        }
    }
    else
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

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeUniversal, pDeCmdSpace);
    }
    if (HasUavExport)
    {
        pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(PS_PARTIAL_FLUSH, EngineTypeUniversal, pDeCmdSpace);
    }

    pDeCmdSpace  = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Issues an indirect non-indexed draw command. We must discard the draw if vertexCount or instanceCount are zero.
// We will rely on the HW to discard the draw for us.
template <bool IssueSqttMarkerEvent, bool ViewInstancingEnable, bool DescribeDrawDispatch>
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
    if (DescribeDrawDispatch)
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
template <bool IssueSqttMarkerEvent, bool ViewInstancingEnable, bool DescribeDrawDispatch>
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
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawIndexedIndirectMulti);
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
                    pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexIndirectMulti(offset,
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
    }
    else
    {
        {
            pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexIndirectMulti(offset,
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
template <bool IssueSqttMarkerEvent, bool UseCpuPathForUserDataTables, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatch(
    ICmdBuffer* pCmdBuffer,
    uint32      x,
    uint32      y,
    uint32      z)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    if (DescribeDrawDispatch)
    {
        pThis->m_device.DescribeDispatch(pThis, Developer::DrawDispatchType::CmdDispatch, 0, 0, 0, x, y, z);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace  = pThis->ValidateDispatch<UseCpuPathForUserDataTables>(0uLL, x, y, z, pDeCmdSpace);
    pDeCmdSpace  = pThis->WaitOnCeCounter(pDeCmdSpace);

    pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect<false, true>(x, y, z,
                                                                     pThis->PacketPredicate(),
                                                                     pThis->m_pSignatureCs->flags.isWave32,
                                                                     pThis->UsesDispatchTunneling(),
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
template <bool IssueSqttMarkerEvent, bool UseCpuPathForUserDataTables, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchIndirect(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    if (DescribeDrawDispatch)
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
                                                     pThis->m_pSignatureCs->flags.isWave32,
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
template <bool IssueSqttMarkerEvent, bool UseCpuPathForUserDataTables, bool DescribeDrawDispatch>
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

    if (DescribeDrawDispatch)
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
                                                                      pThis->m_pSignatureCs->flags.isWave32,
                                                                      pThis->UsesDispatchTunneling(),
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
    pDeCmdSpace += CmdUtil::BuildWriteData(writeData, value, pDeCmdSpace);
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
    pDeCmdSpace += CmdUtil::BuildAtomicMem(atomicOp, address, srcData, pDeCmdSpace);
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
        pDeCmdSpace += CmdUtil::BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
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
        pDeCmdSpace += CmdUtil::BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
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
            pDeCmdSpace = pNewPalette->WriteCommands(pipelineBindPoint,
                                                     TimestampGpuVirtAddr(),
                                                     &m_deCmdStream,
                                                     pDeCmdSpace);
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
    if (m_device.Parent()->ChipProperties().gfxLevel != GfxIpLevel::GfxIp9)
    {
        pCmdSpace = m_deCmdStream.WriteSetOneConfigReg<true>(userDataAddr, markerData, pCmdSpace);
    }
    else
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
        if (m_device.Parent()->ChipProperties().gfxLevel != GfxIpLevel::GfxIp9)
        {
            pCmdSpace = m_deCmdStream.WriteSetSeqConfigRegs<true>(mmSQ_THREAD_TRACE_USERDATA_2,
                                                                  mmSQ_THREAD_TRACE_USERDATA_2 + dwordsToWrite - 1,
                                                                  pDwordData,
                                                                  pCmdSpace);
        }
        else
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
    // If the dbRenderControl.DEPTH_CLEAR_ENABLE bit is not reset to 0 after performing a graphics fast depth clear
    // then any following draw call with pixel shader z-imports will have their z components clamped to the clear
    // plane equation which was set in the fast clear.
    //
    //     [dbRenderControl.]DEPTH_CLEAR_ENABLE will modify the zplane of the incoming geometry to the clear plane.
    //     So if the shader uses this z plane (that is, z-imports are enabled), this can affect the color output.

    struct
    {
        regDB_RENDER_OVERRIDE2  dbRenderOverride2;
        regDB_HTILE_DATA_BASE   dbHtileDataBase;
    } regs1 = { };

    struct
    {
        regDB_Z_INFO        dbZInfo;
        regDB_STENCIL_INFO  dbStencilInfo;
    } regs2 = { };

    const regDB_RENDER_CONTROL dbRenderControl = { };

    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        pCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(Gfx09::mmDB_Z_INFO,
                                                         Gfx09::mmDB_STENCIL_INFO,
                                                         &regs2,
                                                         pCmdSpace);
    }
    else if (IsGfx10(m_gfxIpLevel))
    {
        pCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(Gfx10::mmDB_Z_INFO,
                                                         Gfx10::mmDB_STENCIL_INFO,
                                                         &regs2,
                                                         pCmdSpace);

    }
    else
    {
        // What is this?
        PAL_ASSERT_ALWAYS();
    }

    pCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmDB_RENDER_OVERRIDE2, mmDB_HTILE_DATA_BASE, &regs1, pCmdSpace);
    return m_deCmdStream.WriteSetOneContextReg(mmDB_RENDER_CONTROL, dbRenderControl.u32All, pCmdSpace);
}

// =====================================================================================================================
// Build the NULL color targets PM4 packets. It is safe to call this when there are no NULL color targets.
void UniversalCmdBuffer::WriteNullColorTargets(
    uint32  newColorTargetMask,
    uint32  oldColorTargetMask)
{
    // Compute a mask of slots which were previously bound to valid targets, but are now being bound to NULL.
    uint32 newNullSlotMask = (oldColorTargetMask & ~newColorTargetMask);
    while (newNullSlotMask != 0)
    {
        uint32 slot = 0;
        BitMaskScanForward(&slot, newNullSlotMask);

        static_assert((COLOR_INVALID == 0), "COLOR_INVALID != 0");

        // Zero out all the RTV owned fields of CB_COLOR_INFO.
        BitfieldUpdateSubfield(&(m_cbColorInfo[slot].u32All), 0u, ColorTargetView::CbColorInfoMask);

        m_state.flags.cbColorInfoDirtyRtv |= (1 << slot);

        // Clear the bit since we've already added it to our PM4 image.
        newNullSlotMask &= ~(1 << slot);
    }
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

    if (IsGfx10(m_gfxIpLevel))
    {
        //  DISABLE_BINNING_USE_LEGACY_SC: reverts binning completely and uses the old scan converter (along with
        //                                 serpentine walking pattern). It doesn't support FSR in GFX10.
        //  DISABLE_BINNING_USE_NEW_SC   : disables binning but still uses the binner rasterizer (typewriter walking
        //                                 pattern). Supports FSR.
        //
        //  Because we want to maintain the same performance characteristics, we want to use the
        //  second setting on GFX10: DISABLE_BINNING_USE_NEW_SC
        binningMode = DISABLE_BINNING_USE_NEW_SC;

        // when using "New" SC (PA_SC_BINNER_CNTL_0.BINNING_MODE = 2)
        //     If <= 4BPE render-target -> 128x128
        //     Else -> 128x64  (ie 8BPE and 16BPE)
        // If the render targets are a mix of 4BPE and 8/16 BPE, driver is to use 128x128 bin size.
        uint32  minBpe = 0;

        // First check if there is a bound Depth target which is enabled and being written to
        const auto&  boundTargets     = m_graphicsState.bindTargets;
        const auto*  pDepthTargetView =
                static_cast<const DepthStencilView*>(boundTargets.depthTarget.pDepthStencilView);
        const auto*  pDepthImage      = (pDepthTargetView ? pDepthTargetView->GetImage() : nullptr);
        if (pDepthImage != nullptr)
        {
            const auto*  pDepthStencilState = static_cast<const DepthStencilState*>(m_graphicsState.pDepthStencilState);
            const auto&  imageCreateInfo    = pDepthImage->Parent()->GetImageCreateInfo();
            if ((pDepthStencilState->IsDepthEnabled() && (pDepthTargetView->ReadOnlyDepth() == false)) ||
                (pDepthStencilState->IsStencilEnabled() && (pDepthTargetView->ReadOnlyStencil() == false)))
            {
                // Since depth targets can only ever be 16 or 32 bits-per-pixel, we always fall into the 4BPE or
                // less case
                minBpe = 4;
            }
        }

        // Query Color targets if minimum not determined from Depth
        if (minBpe == 0)
        {
            // Loop through all Color targets to find minimum bytes per pixel
            for (uint32  idx = 0; idx < boundTargets.colorTargetCount; idx++)
            {
                const auto* pColorView =
                        static_cast<const ColorTargetView*>(boundTargets.colorTargets[idx].pColorTargetView);
                const auto* pImage     = ((pColorView != nullptr) ? pColorView->GetImage() : nullptr);

                if (pImage != nullptr)
                {
                    const auto&  info = pImage->Parent()->GetImageCreateInfo();
                    const uint32 bpe  = BytesPerPixel(info.swizzledFormat.format);
                    if ((bpe != 0) && ((minBpe == 0) || (bpe < minBpe)))
                    {
                        minBpe = bpe;
                    }
                }
            }
        }

        if (minBpe <= 4) // <= 4 BPE, or mixed <=4 with 8/16 BPE
        {
            pBinSize->width  = 128;
            pBinSize->height = 128;
        }
        else // 8 or 16 BPE
        {
            pBinSize->width  = 128;
            pBinSize->height = 64;
        }
    }

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

        PM4_PFP_CONTEXT_CONTROL contextControl = {};

        contextControl.bitfields2.update_load_enables    = 1;
        contextControl.bitfields2.load_per_context_state = 1;
        contextControl.bitfields3.update_shadow_enables  = 1;

        pDeCmdSpace += CmdUtil::BuildContextControl(contextControl, pDeCmdSpace);
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

    // Track the state of the fields owned by the graphics pipeline.
    PAL_ASSERT((dbRenderOverride.u32All & PipelineDbRenderOverrideMask) == 0);
    m_dbRenderOverride.u32All = 0;

    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmDB_RENDER_OVERRIDE, dbRenderOverride.u32All, pDeCmdSpace);

    // The draw-time validation will get confused unless we set PA_SC_AA_CONFIG to a known last value.
    pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmPA_SC_AA_CONFIG, m_paScAaConfigLast.u32All, pDeCmdSpace);

    if (isNested)
    {
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

            if (m_deCmdStream.Pm4OptimizerEnabled())
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
    }

    const uint32  mmPaStateStereoX = m_cmdUtil.GetRegInfo().mmPaStateStereoX;
    if (mmPaStateStereoX != 0)
    {
        if (IsGfx10(m_gfxIpLevel))
        {
            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmPaStateStereoX, 0, pDeCmdSpace);
        }
        else
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

    // Initialize VGT_LS_HS_CONFIG. It will be rewritten at draw-time if its value changes.
    if (m_deCmdStream.Pm4OptimizerEnabled())
    {
        pDeCmdSpace = m_deCmdStream.WriteSetVgtLsHsConfig<true>(m_vgtLsHsConfig, pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = m_deCmdStream.WriteSetVgtLsHsConfig<false>(m_vgtLsHsConfig, pDeCmdSpace);
    }

    // With the PM4 optimizer enabled, certain registers are only updated via RMW packets and not having an initial
    // value causes the optimizer to skip optimizing redundant RMW packets.
    if (m_deCmdStream.Pm4OptimizerEnabled())
    {
        if (isNested == false)
        {
            // Nested command buffers inherit parts of the following registers and hence must not be reset
            // in the preamble.
            constexpr uint32 ZeroStencilRefMasks[] = { 0, 0 };
            pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmDB_STENCILREFMASK,
                                                               mmDB_STENCILREFMASK_BF,
                                                               &ZeroStencilRefMasks[0],
                                                               pDeCmdSpace);
        }
    }

    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmPA_SC_BINNER_CNTL_0, m_paScBinnerCntl0.u32All, pDeCmdSpace);

    if (isNested == false)
    {
        // Initialize screen scissor value.
        struct
        {
            regPA_SC_SCREEN_SCISSOR_TL tl;
            regPA_SC_SCREEN_SCISSOR_BR br;
        } paScScreenScissor = { };

        paScScreenScissor.br.bits.BR_X = m_graphicsState.targetExtent.width;
        paScScreenScissor.br.bits.BR_Y = m_graphicsState.targetExtent.height;

        pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmPA_SC_SCREEN_SCISSOR_TL,
                                                           mmPA_SC_SCREEN_SCISSOR_BR,
                                                           &paScScreenScissor,
                                                           pDeCmdSpace);
    }

    if (m_cmdUtil.GetRegInfo().mmDbDfsmControl != 0)
    {
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(m_cmdUtil.GetRegInfo().mmDbDfsmControl,
                                                               m_dbDfsmControl.u32All,
                                                               pDeCmdSpace);
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
        pDeCmdSpace += CmdUtil::BuildWaitDmaData(pDeCmdSpace);
        SetGfxCmdBufCpBltState(false);
    }

    bool didWaitForIdle = false;

    if ((m_ceCmdStream.GetNumChunks() > 0) &&
        (m_ceCmdStream.GetFirstChunk()->BusyTrackerGpuAddr() != 0))
    {
        // The timestamps used for reclaiming command stream chunks are written when the DE stream has completed.
        // This ensures the CE stream completes before the DE stream completes, so that the timestamp can't return
        // before CE work is complete.
        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
        pCeCmdSpace += CmdUtil::BuildIncrementCeCounter(pCeCmdSpace);
        m_ceCmdStream.CommitCommands(pCeCmdSpace);

        pDeCmdSpace += CmdUtil::BuildWaitOnCeCounter(false, pDeCmdSpace);
        pDeCmdSpace += CmdUtil::BuildIncrementDeCounter(pDeCmdSpace);

        // We also need a wait-for-idle before the atomic increment because command memory might be read or written
        // by draws or dispatches. If we don't wait for idle then the driver might reset and write over that memory
        // before the shaders are done executing.
        didWaitForIdle = true;
        pDeCmdSpace += m_cmdUtil.BuildWaitOnReleaseMemEvent(GetEngineType(),
                                                            BOTTOM_OF_PIPE_TS,
                                                            TcCacheOp::Nop,
                                                            TimestampGpuVirtAddr(),
                                                            pDeCmdSpace);

        // The following ATOMIC_MEM packet increments the done-count for the CE command stream, so that we can probe
        // when the command buffer has completed execution on the GPU.
        // NOTE: Normally, we would need to flush the L2 cache to guarantee that this memory operation makes it out to
        // memory. However, since we're at the end of the command buffer, we can rely on the fact that the KMD inserts
        // an EOP event which flushes and invalidates the caches in between command buffers.
        pDeCmdSpace += CmdUtil::BuildAtomicMem(AtomicOp::AddInt32,
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
        // If we didn't have a CE tracker we still need this wait-for-idle. See the comment above for the reason.
        if (didWaitForIdle == false)
        {
            pDeCmdSpace += m_cmdUtil.BuildWaitOnReleaseMemEvent(GetEngineType(),
                                                                BOTTOM_OF_PIPE_TS,
                                                                TcCacheOp::Nop,
                                                                TimestampGpuVirtAddr(),
                                                                pDeCmdSpace);
        }

        pDeCmdSpace += CmdUtil::BuildAtomicMem(AtomicOp::AddInt32,
                                               m_deCmdStream.GetFirstChunk()->BusyTrackerGpuAddr(),
                                               1,
                                               pDeCmdSpace);
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);

#if PAL_BUILD_PM4_INSTRUMENTOR
    if (m_cachedSettings.enablePm4Instrumentation)
    {
        m_deCmdStream.IssueHotRegisterReport(this);
    }
#endif

    return Result::Success;
}

// =====================================================================================================================
void UniversalCmdBuffer::BeginExecutionMarker(
    uint64 clientHandle)
{
    CmdBuffer::BeginExecutionMarker(clientHandle);
    PAL_ASSERT(m_executionMarkerAddr != 0);

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace += m_cmdUtil.BuildExecutionMarker(m_executionMarkerAddr,
                                                  m_executionMarkerCount,
                                                  clientHandle,
                                                  RGD_EXECUTION_BEGIN_MARKER_GUARD,
                                                  pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
uint32 UniversalCmdBuffer::CmdInsertExecutionMarker()
{
    uint32 returnVal = UINT_MAX;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 533
    if (m_buildFlags.enableExecutionMarkerSupport == 1)
    {
        PAL_ASSERT(m_executionMarkerAddr != 0);

        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
        pDeCmdSpace += m_cmdUtil.BuildExecutionMarker(m_executionMarkerAddr,
                                                      ++m_executionMarkerCount,
                                                      0,
                                                      RGD_EXECUTION_MARKER_GUARD,
                                                      pDeCmdSpace);
        m_deCmdStream.CommitCommands(pDeCmdSpace);

        returnVal = m_executionMarkerCount;
    }
#endif
    return returnVal;
}

// =====================================================================================================================
void UniversalCmdBuffer::EndExecutionMarker()
{
    PAL_ASSERT(m_executionMarkerAddr != 0);

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace += m_cmdUtil.BuildExecutionMarker(m_executionMarkerAddr,
                                                  ++m_executionMarkerCount,
                                                  0,
                                                  RGD_EXECUTION_MARKER_GUARD,
                                                  pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
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
        pDeCmdSpace += CmdUtil::BuildWaitDmaData(pDeCmdSpace);
        SetGfxCmdBufCpBltState(false);
    }

    if (pipePoint == HwPipePostBlt)
    {
        // HwPipePostBlt barrier optimization
        pipePoint = OptimizeHwPipePostBlit();
    }
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 577
    else if (pipePoint == HwPipePreColorTarget)
    {
        // HwPipePreColorTarget is only valid as wait point. But for the sake of robustness, if it's used as pipe
        // point to wait on, it's equivalent to HwPipePostPs.
        pipePoint = HwPipePostPs;
    }
#endif

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

        pDeCmdSpace += CmdUtil::BuildWriteData(writeData, data, pDeCmdSpace);
        break;

    case HwPipePostIndexFetch:
        // Implement set/reset event with a WRITE_DATA command using the ME engine.
        writeData.engineSel = engine_sel__me_write_data__micro_engine;

        pDeCmdSpace += CmdUtil::BuildWriteData(writeData, data, pDeCmdSpace);
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

        pDeCmdSpace += CmdUtil::BuildWriteData(writeData, data, pDeCmdSpace);
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

        pDeCmdSpace += CmdUtil::BuildWaitOnCeCounter((m_state.flags.ceInvalidateKcache != 0), pDeCmdSpace);

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
        pDeCmdSpace += CmdUtil::BuildIncrementDeCounter(pDeCmdSpace);

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

    pCeCmdSpace += CmdUtil::BuildWriteConstRam(pSrcData,
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
        pCeCmdSpace += CmdUtil::BuildWaitOnDeCounterDiff(m_state.minCounterDiff, pCeCmdSpace);
        m_state.flags.ceWaitOnDeCounterDiff = 0;
    }

    const uint32 offsetInBytes = (sizeof(uint32) * offsetInDwords);

    // Keep track of the latest DUMP_CONST_RAM packet before the upcoming draw or dispatch.  The last one before the
    // draw or dispatch will be updated to set the increment_ce bit at draw-time.
    m_state.pLastDumpCeRam                    = pCeCmdSpace;
    m_state.lastDumpCeRamOrdinal2.bits.offset = (pTable->ceRamOffset + offsetInBytes);

    pCeCmdSpace += CmdUtil::BuildDumpConstRam((pTable->gpuVirtAddr + offsetInBytes),
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
// Helper function responsible for handling user-SGPR updates during Dispatch-time validation when the active pipeline
// has changed since the previous Dispathc operation.  It is expected that this will be called only when the pipeline
// is changing and immediately before a call to WriteDirtyUserDataEntriesToUserSgprsCs().
void UniversalCmdBuffer::FixupUserSgprsOnPipelineSwitchCs(
    const ComputePipelineSignature* pPrevSignature)
{
    // As in WriteDirtyUserDataEntriesToUserSgprsCs, we assume that all fast user data fit in a single dirty bitfield.
    static_assert(NumUserDataRegistersCompute <= UserDataEntriesPerMask,
                  "The CS user-data entries mapped to user-SGPR's spans multiple wide-bitfield elements!");

    const size_t prevFastUserData = pPrevSignature->stage.userSgprCount;
    const size_t nextFastUserData = m_pSignatureCs->stage.userSgprCount;

    if (prevFastUserData < nextFastUserData)
    {
        // Compute the mask of all dirty bits from the end of the previous fast user data range to the end of the
        // next fast user data range. This is required to handle these cases:
        // 1. If the next spillThreshold is higher we need to migrate user data from the table to sgprs.
        // 2. We only write fast user data up to the userDataLimit. If the client bound user data for the next pipeline
        //    (higher limit) before binding the previous pipeline (lower limit) we wouldn't have written it out.
        //
        // This could be wasteful if the client binds a common set of user data and frequently switches between
        // pipelines with different user data limits. We probably can't avoid that overhead without rewriting
        // our compute user data management.
        const size_t rewriteMask = BitfieldGenMask(nextFastUserData) & ~BitfieldGenMask(prevFastUserData);

        m_computeState.csUserDataEntries.dirty[0] |= rewriteMask;
    }
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
    static_assert(NumUserDataRegistersCompute <= UserDataEntriesPerMask,
                  "The CS user-data entries mapped to user-SGPR's spans multiple wide-bitfield elements!");

    const size_t numFastUserDataEntries  = m_pSignatureCs->stage.userSgprCount;
    const size_t fastUserDataEntriesMask = BitfieldGenMask(numFastUserDataEntries);
    const size_t userSgprDirtyMask       = (m_computeState.csUserDataEntries.dirty[0] & fastUserDataEntriesMask);

    // Additionally, dirty compute user-data is always written to user-SGPR's if it could be mapped by a pipeline,
    // which lets us avoid any complex logic when switching pipelines.
    const uint32 baseUserSgpr = m_device.GetFirstUserDataReg(HwShaderStage::Cs);

    for (uint32 e = 0; e < numFastUserDataEntries; ++e)
    {
        const uint32 firstEntry = e;
        uint32       entryCount = 0;

        while ((e < numFastUserDataEntries) && ((userSgprDirtyMask & (static_cast<size_t>(1) << e)) != 0))
        {
            ++entryCount;
            ++e;
        }

        if (entryCount > 0)
        {
            const uint32 lastEntry = (firstEntry + entryCount - 1);
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
// Helper function to create SRDs corresponding to the current render targets
void UniversalCmdBuffer::UpdateUavExportTable()
{
    for (uint32 idx = 0; idx < m_uavExportTable.maxColorTargets; ++idx)
    {
        const auto* pTargetView = m_graphicsState.bindTargets.colorTargets[idx].pColorTargetView;
        if (pTargetView != nullptr)
        {
            PAL_ASSERT(IsGfx10(m_gfxIpLevel));
            const Gfx10ColorTargetView* pGfx10TargetView = static_cast<const Gfx10ColorTargetView*>(pTargetView);
            pGfx10TargetView->GetImageSrd(m_device, &m_uavExportTable.srd[idx]);
        }
        else
        {
            m_uavExportTable.srd[idx] = {};
        }
    }
    m_uavExportTable.state.dirty = 1;
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
    constexpr uint16 UavExportMask        = (1 << 2);
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
    // Update uav export srds if enabled
    const uint16 uavExportEntry = m_pSignatureGfx->uavExportTableAddr;
    if (uavExportEntry != UserDataNotMapped)
    {
        const auto dirtyFlags = m_graphicsState.dirtyFlags.validationBits;
        if (HasPipelineChanged || (dirtyFlags.colorTargetView))
        {
            UpdateUavExportTable();
        }

        if (m_uavExportTable.state.dirty != 0)
        {
            RelocateUserDataTable<CacheLineDwords>(&m_uavExportTable.state, 0, m_uavExportTable.tableSizeDwords);
            srdTableDumpMask |= UavExportMask;
        }

        // Update the virtual address if the table has been relocated or we have a different sgpr mapping
        if ((HasPipelineChanged && (pPrevSignature->uavExportTableAddr != uavExportEntry)) ||
            (m_uavExportTable.state.dirty != 0))
        {
            const uint32 gpuVirtAddrLo = LowPart(m_uavExportTable.state.gpuVirtAddr);
            pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics>(uavExportEntry,
                                                                         gpuVirtAddrLo,
                                                                         pDeCmdSpace);
        }
    }

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
        if (srdTableDumpMask & UavExportMask)
        {
            pCeCmdSpace = UploadToUserDataTable(&m_uavExportTable.state,
                                                0,
                                                m_uavExportTable.tableSizeDwords,
                                                reinterpret_cast<const uint32*>(&m_uavExportTable.srd),
                                                UINT_MAX,
                                                pCeCmdSpace);
            pCeCmdSpace = DumpUserDataTable(&m_uavExportTable.state,
                                            0,
                                            m_uavExportTable.tableSizeDwords,
                                            pCeCmdSpace);
        } // if uav export table needs dumping
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
    // Update uav export srds if enabled
    const uint16 uavExportEntry = m_pSignatureGfx->uavExportTableAddr;
    if (uavExportEntry != UserDataNotMapped)
    {
        const auto dirtyFlags = m_graphicsState.dirtyFlags.validationBits;
        if (HasPipelineChanged || (dirtyFlags.colorTargetView))
        {
            UpdateUavExportTable();
        }

        if (m_uavExportTable.state.dirty != 0)
        {
            UpdateUserDataTableCpu(&m_uavExportTable.state,
                                   0,
                                   m_uavExportTable.tableSizeDwords,
                                   reinterpret_cast<const uint32*>(&m_uavExportTable.srd));
        }

        // Update the virtual address if the table has been relocated or we have a different sgpr mapping
        if ((HasPipelineChanged && (pPrevSignature->uavExportTableAddr != uavExportEntry)) ||
            (m_uavExportTable.state.dirty != 0))
        {
            const uint32 gpuVirtAddrLo = LowPart(m_uavExportTable.state.gpuVirtAddr);
            pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics>(uavExportEntry,
                                                                         gpuVirtAddrLo,
                                                                         pDeCmdSpace);
        }
    }

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
                size_t dirtyMask = m_graphicsState.gfxUserDataEntries.dirty[maskId];
                if (maskId == firstMaskId)
                {
                    // Ignore the dirty bits for any entries below the spill threshold.
                    const uint32 firstEntryInMask = (spillThreshold & (UserDataEntriesPerMask - 1));
                    dirtyMask &= ~BitfieldGenMask(static_cast<size_t>(firstEntryInMask));
                }
                if (maskId == lastMaskId)
                {
                    // Ignore the dirty bits for any entries beyond the user-data limit.
                    const uint32 lastEntryInMask = (lastUserData & (UserDataEntriesPerMask - 1));
                    dirtyMask &= BitfieldGenMask(static_cast<size_t>(lastEntryInMask + 1));
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
    // Write all dirty user-data entries to their mapped user SGPR's. If the pipeline has changed we must also fixup
    // the dirty bits because the prior compute pipeline could use fewer fast sgprs than the current pipeline.
    if (HasPipelineChanged)
    {
        FixupUserSgprsOnPipelineSwitchCs(pPrevSignature);
    }

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
            // We need to re-write the spill table GPU address to its user-SGPR if:
            // - the spill table was relocated during step #3, or
            // - the pipeline was changed and the previous pipeline either didn't spill or used a different spill reg.
            if (relocated ||
                (HasPipelineChanged &&
                 ((pPrevSignature->spillThreshold == NoUserDataSpilling) ||
                  (pPrevSignature->stage.spillTableRegAddr != m_pSignatureCs->stage.spillTableRegAddr))))
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
    // Write all dirty user-data entries to their mapped user SGPR's. If the pipeline has changed we must also fixup
    // the dirty bits because the prior compute pipeline could use fewer fast sgprs than the current pipeline.
    if (HasPipelineChanged)
    {
        FixupUserSgprsOnPipelineSwitchCs(pPrevSignature);
    }

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
                size_t dirtyMask = m_computeState.csUserDataEntries.dirty[maskId];
                if (maskId == firstMaskId)
                {
                    // Ignore the dirty bits for any entries below the spill threshold.
                    const uint32 firstEntryInMask = (spillThreshold & (UserDataEntriesPerMask - 1));
                    dirtyMask &= ~BitfieldGenMask(static_cast<size_t>(firstEntryInMask));
                }
                if (maskId == lastMaskId)
                {
                    // Ignore the dirty bits for any entries beyond the user-data limit.
                    const uint32 lastEntryInMask = (lastUserData & (UserDataEntriesPerMask - 1));
                    dirtyMask &= BitfieldGenMask(static_cast<size_t>(lastEntryInMask + 1));
                }

                if (dirtyMask != 0)
                {
                    reUpload = true;
                    break; // We only care if *any* spill table contents change!
                }
            } // for each wide-bitfield sub-mask
        }

        // Step #3:
        // Re-upload spill table contents if necessary.
        if (reUpload)
        {
            UpdateUserDataTableCpu(&m_spillTable.stateCs,
                                   (userDataLimit - spillThreshold),
                                   spillThreshold,
                                   &m_computeState.csUserDataEntries.entries[0]);
        }

        // Step #4:
        // We need to re-write the spill table GPU address to its user-SGPR if:
        // - the spill table was reuploaded during step #3, or
        // - the pipeline was changed and the previous pipeline either didn't spill or used a different spill reg.
        if (reUpload ||
            (HasPipelineChanged &&
             ((pPrevSignature->spillThreshold == NoUserDataSpilling) ||
              (pPrevSignature->stage.spillTableRegAddr != m_pSignatureCs->stage.spillTableRegAddr))))
        {
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
    if (m_deCmdStream.Pm4OptimizerEnabled())
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

#if PAL_BUILD_PM4_INSTRUMENTOR
    uint32 startingCmdLen = GetUsedSize(CommandDataAlloc);
    uint32 pipelineCmdLen = 0;
    uint32 userDataCmdLen = 0;
#endif

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

#if PAL_ENABLE_PRINTS_ASSERTS
        m_pipelineFlagsValid = true; ///< Setup in SwitchGraphicsPipeline()
#endif

        // NOTE: Switching a graphics pipeline can result in a large amount of commands being written, so start a new
        // reserve/commit region before proceeding with validation.
        m_deCmdStream.CommitCommands(pDeCmdSpace);

#if PAL_BUILD_PM4_INSTRUMENTOR
        if (m_cachedSettings.enablePm4Instrumentation != 0)
        {
            pipelineCmdLen  = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
            startingCmdLen += pipelineCmdLen;
        }
#endif

        pDeCmdSpace = m_deCmdStream.ReserveCommands();

        pDeCmdSpace = (this->*m_pfnValidateUserDataGfxPipelineSwitch)(pPrevSignature, pDeCmdSpace);

#if PAL_BUILD_PM4_INSTRUMENTOR
        if (m_cachedSettings.enablePm4Instrumentation != 0)
        {
            // GetUsedSize() is not accurate if we don't put the user-data validation and miscellaneous validation
            // in separate Reserve/Commit blocks.
            m_deCmdStream.CommitCommands(pDeCmdSpace);
            userDataCmdLen  = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
            startingCmdLen += userDataCmdLen;
            pDeCmdSpace     = m_deCmdStream.ReserveCommands();
        }
#endif

        pDeCmdSpace = ValidateDraw<Indexed, Indirect, Pm4OptImmediate, true>(drawInfo, pDeCmdSpace);

        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }
    else
    {
#if PAL_ENABLE_PRINTS_ASSERTS
        m_pipelineFlagsValid = true; ///< Valid for all for draw-time when pipeline isn't dirty.
#endif

        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

        pDeCmdSpace = (this->*m_pfnValidateUserDataGfx)(nullptr, pDeCmdSpace);

#if PAL_BUILD_PM4_INSTRUMENTOR
        if (m_cachedSettings.enablePm4Instrumentation != 0)
        {
            // GetUsedSize() is not accurate if we don't put the user-data validation and miscellaneous validation
            // in separate Reserve/Commit blocks.
            m_deCmdStream.CommitCommands(pDeCmdSpace);
            userDataCmdLen  = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
            startingCmdLen += userDataCmdLen;
            pDeCmdSpace     = m_deCmdStream.ReserveCommands();
        }
#endif

        pDeCmdSpace = ValidateDraw<Indexed, Indirect, Pm4OptImmediate, false>(drawInfo, pDeCmdSpace);

        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }

#if PAL_BUILD_PM4_INSTRUMENTOR
    if (m_cachedSettings.enablePm4Instrumentation != 0)
    {
        const uint32 miscCmdLen = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
        m_device.DescribeDrawDispatchValidation(this, userDataCmdLen, pipelineCmdLen, miscCmdLen);
    }
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    m_pipelineFlagsValid = false;
#endif
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
    if (IsNggEnabled())
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

// Helper structure to convert uint32 to a float.
union Uint32ToFloat
{
    uint32 uValue;
    float  fValue;
};

// =====================================================================================================================
// This function updates the NGG culling data constant buffer which is needed for NGG culling operations to execute
// correctly.  See the UpdateNggCullingDataBufferWithGpu function for reference code.
// Returns a pointer to the next entry in the DE cmd space.  This function MUST NOT write any context registers!
uint32* UniversalCmdBuffer::UpdateNggCullingDataBufferWithCpu(
    uint32* pDeCmdSpace)
{
    PAL_ASSERT(m_pSignatureGfx->nggCullingDataAddr != UserDataNotMapped);

    // If nothing has changed, then there's no need to do anything...
    if ((m_nggState.flags.dirty.u8All != 0) || m_graphicsState.pipelineState.dirtyFlags.pipelineDirty)
    {
        constexpr uint32 NggStateDwords = (sizeof(Abi::PrimShaderCbLayout) / sizeof(uint32));
        const     uint16 nggRegAddr     = m_pSignatureGfx->nggCullingDataAddr;

        // Make a local copy of the various shader state so that we can modify it as necessary.
        Abi::PrimShaderCbLayout  localShaderLayout;
        memcpy(&localShaderLayout, &m_state.primShaderCbLayout, sizeof(uint32) * NggStateDwords);

        if (m_nggState.flags.dirty.viewports || m_nggState.flags.dirty.msaaState)
        {
            // For small-primitive filter culling with NGG, the shader needs the viewport scale to premultiply
            // the number of samples into it.
            Abi::PrimShaderVportCb*  pVportCb = &localShaderLayout.viewportStateCb;
            Uint32ToFloat uintToFloat = {};
            for (uint32 i = 0; i < m_graphicsState.viewportState.count; i++)
            {
                uintToFloat.uValue  = pVportCb->vportControls[i].paClVportXscale;
                uintToFloat.fValue *= (m_nggState.numSamples > 1) ? 16.0f : 1.0f;

                pVportCb->vportControls[i].paClVportXscale = uintToFloat.uValue;
            }
        }

        // The alignment of the user data is dependent on the type of register used to store
        // the address.
        const uint32 byteAlignment = ((nggRegAddr == mmSPI_SHADER_PGM_LO_GS) ? 256 : 4);

        // Copy all of NGG state into embedded data, which is pointed to by nggTable.gpuVirtAddr
        UpdateUserDataTableCpu(&m_nggTable.state,
                               NggStateDwords, // size
                               0,              // offset
                               reinterpret_cast<const uint32*>(&localShaderLayout),
                               NumBytesToNumDwords(byteAlignment));

        gpusize gpuVirtAddr = m_nggTable.state.gpuVirtAddr;
        if (byteAlignment == 256)
        {
            // The address of the constant buffer is stored in the GS shader address registers, which require a
            // 256B aligned address.
            gpuVirtAddr = Get256BAddrLo(m_nggTable.state.gpuVirtAddr);
        }

        pDeCmdSpace = m_deCmdStream.WriteSetSeqShRegs(nggRegAddr,
                                                      (nggRegAddr + 1),
                                                      ShaderGraphics,
                                                      &gpuVirtAddr,
                                                      pDeCmdSpace);

        m_nggState.flags.dirty.u8All = 0;
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// This function updates the NGG culling data constant buffer which is needed for NGG culling operations to execute
// correctly.  Updates to this should also be made in UpdateNggCullingDataBufferWithCpu as well.
// Returns a pointer to the next entry in the DE cmd space.  This function MUST NOT write any context registers!
uint32* UniversalCmdBuffer::UpdateNggCullingDataBufferWithGpu(
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
uint32* UniversalCmdBuffer::Gfx10ValidateTriangleRasterState(
    const GraphicsPipeline*  pPipeline,
    uint32*                  pDeCmdSpace)
{
    //  The field was added for both polymode and perpendicular endcap lines.
    //  The SC reuses some information from the first primitive for other primitives within a polymode group. The
    //  whole group needs to make it to the SC in the same order it was produced by the PA. When the field is enabled,
    //  the PA will set a keep_together bit on the first and last primitive of each group. This tells the PBB that the
    //  primitives must be kept in order
    //
    //  it should be enabled when POLY_MODE is enabled.  Also, if the driver ever sets PERPENDICULAR_ENDCAP_ENA, that
    //  should follow the same rules
    if ((m_graphicsState.triangleRasterState.frontFillMode != FillMode::Solid) ||
        (m_graphicsState.triangleRasterState.backFillMode  != FillMode::Solid) ||
        pPipeline->IsPerpEndCapsEnabled())
    {
        pDeCmdSpace = m_deCmdStream.WriteContextRegRmw(mmPA_SU_SC_MODE_CNTL,
                                                       Gfx10::PA_SU_SC_MODE_CNTL__KEEP_TOGETHER_ENABLE_MASK,
                                                       Gfx10::PA_SU_SC_MODE_CNTL__KEEP_TOGETHER_ENABLE_MASK,
                                                       pDeCmdSpace);
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
          bool IsNgg>
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
    if (Indexed                                                 &&
        IsNgg                                                   &&
        (Indirect == false)                                     &&
        m_cachedSettings.prefetchIndexBufferForNgg              &&
        (m_graphicsState.iaState.indexType == IndexType::Idx32) &&
        (m_graphicsState.inputAssemblyState.topology == PrimitiveTopology::TriangleList))
    {

        // We'll underflow the numPages calculation if we're priming zero bytes.
        const size_t  offset      = drawInfo.firstIndex  * sizeof(uint32);
        const size_t  sizeInBytes = drawInfo.vtxIdxCount * sizeof(uint32);
        const gpusize gpuAddr     = m_graphicsState.iaState.indexAddr + offset;
        PAL_ASSERT(sizeInBytes > 0);

        const gpusize firstPage   = Pow2AlignDown(gpuAddr, PrimeUtcL2MemAlignment);
        const gpusize lastPage    = Pow2AlignDown(gpuAddr + sizeInBytes - 1, PrimeUtcL2MemAlignment);
        const size_t  numPages    = 1 + static_cast<size_t>((lastPage - firstPage) / PrimeUtcL2MemAlignment);

        // If multiple draws refetch indices from the same page there's no need to refetch that page.
        // Also, if we use 2 MB pages there won't be much benefit from priming.
        if ((firstPage < m_drawTimeHwState.nggIndexBufferPfStartAddr) ||
            (lastPage  > m_drawTimeHwState.nggIndexBufferPfEndAddr))
        {
            m_drawTimeHwState.nggIndexBufferPfStartAddr = firstPage;
            m_drawTimeHwState.nggIndexBufferPfEndAddr   = lastPage;

            pDeCmdSpace += CmdUtil::BuildPrimeUtcL2(firstPage,
                                                    cache_perm__pfp_prime_utcl2__read,
                                                    prime_mode__pfp_prime_utcl2__dont_wait_for_xack,
                                                    engine_sel__pfp_prime_utcl2__prefetch_parser,
                                                    numPages,
                                                    pDeCmdSpace);
        }
    }

    pDeCmdSpace = ValidateCbColorInfo<Pm4OptImmediate, PipelineDirty, StateDirty>(pDeCmdSpace);

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

    if (PipelineDirty ||
        (StateDirty && (dirtyFlags.msaaState || dirtyFlags.inputAssemblyState)) ||
        m_cachedSettings.disableWdLoadBalancing)
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
        else if (IsGfx10(m_gfxIpLevel))
        {
            const bool   lineStippleEnabled = (pMsaaState != nullptr) ? pMsaaState->UsesLineStipple() : false;
            const uint32 geCntl             = CalcGeCntl<IsNgg>(lineStippleEnabled, iaMultiVgtParam);

            // GE_CNTL tends to be the same so only bother writing it if the value has changed.
            if (geCntl != m_geCntl.u32All)
            {
                m_geCntl.u32All = geCntl;
                pDeCmdSpace = m_deCmdStream.WriteSetOneConfigReg(Gfx10::mmGE_CNTL, geCntl, pDeCmdSpace);
            }
        }

        if (vgtLsHsConfig.u32All != m_vgtLsHsConfig.u32All)
        {
            m_vgtLsHsConfig = vgtLsHsConfig;
            pDeCmdSpace = m_deCmdStream.WriteSetVgtLsHsConfig<Pm4OptImmediate>(vgtLsHsConfig, pDeCmdSpace);
        }
    }

    if (PipelineDirty || (StateDirty && dirtyFlags.msaaState))
    {
        // Underestimation may be used alone or as inner coverage.
        bool onlyUnderestimation = false;

        // Set the conservative rasterization register state.
        // The final setting depends on whether inner coverage was used in the PS.
        if (pMsaaState != nullptr)
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

        if (onlyUnderestimation == false)
        {
            log2TotalSamples = log2MsaaStateSamples + pPipeline->UsesInnerCoverage();
        }

        // The draw-time validation code owns MSAA_NUM_SAMPLES
        m_paScAaConfigNew.bits.MSAA_NUM_SAMPLES = log2TotalSamples;
    }

    // Rewrite PA_SC_AA_CONFIG if any of its fields have changed. There are lots of state binds that can cause this
    // in addition to the draw-time validation code above.
    bool newAaConfigSamples = false;

    if ((PipelineDirty || StateDirty) &&
        (m_paScAaConfigNew.u32All != m_paScAaConfigLast.u32All))
    {
        newAaConfigSamples = (m_paScAaConfigNew.bits.MSAA_NUM_SAMPLES != m_paScAaConfigLast.bits.MSAA_NUM_SAMPLES);

        pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmPA_SC_AA_CONFIG,
                                                               m_paScAaConfigNew.u32All,
                                                               pDeCmdSpace);

        m_paScAaConfigLast.u32All = m_paScAaConfigNew.u32All;
    }

    bool disableDfsm = m_cachedSettings.disableDfsm;
    if (disableDfsm == false)
    {
        const auto* pDepthImage = (pDsView ? pDsView->GetImage() : nullptr);

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
        if (checkDfsmEqaaWa &&
            (1u << m_paScAaConfigLast.bits.MSAA_NUM_SAMPLES) != pDepthImage->Parent()->GetImageCreateInfo().samples)
        {
            disableDfsm = true;
        }

        if (checkDfsmPsUav && (pPipeline->PsWritesUavs() || pPipeline->PsUsesRovs()))
        {
            disableDfsm = true;
        }

        regDB_DFSM_CONTROL dbDfsmControl;
        dbDfsmControl.u32All             = m_dbDfsmControl.u32All;
        dbDfsmControl.bits.PUNCHOUT_MODE = (disableDfsm ? DfsmPunchoutModeForceOff : DfsmPunchoutModeAuto);

        if (dbDfsmControl.u32All != m_dbDfsmControl.u32All)
        {
            pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(m_cmdUtil.GetRegInfo().mmDbDfsmControl,
                                                                   dbDfsmControl.u32All,
                                                                   pDeCmdSpace);
            m_dbDfsmControl.u32All = dbDfsmControl.u32All;
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
              // optimal gfx10 bin sizes are determined from render targets both when PBB is enabled or disabled
              || IsGfx10(m_gfxIpLevel)
            )
        {
            m_enabledPbb = shouldEnablePbb;
            pDeCmdSpace  = ValidateBinSizes<Pm4OptImmediate>(*pPipeline, pBlendState, disableDfsm, pDeCmdSpace);
        }
    }

    // Validate primitive restart enable.  Primitive restart should only apply for indexed draws, but on gfx9,
    // VGT also applies it to auto-generated vertex index values.
    //
    // GFX10 moves the RESET_EN functionality to a new register called GE_MULTI_PRIM_IB_RESET_EN.  Verify that
    // the GFX10 register has the exact same layout as the GFX9 register to eliminate the need for run-time "if"
    // statements to verify which Gfx level the active device uses.
    static_assert(VGT_MULTI_PRIM_IB_RESET_EN__MATCH_ALL_BITS_MASK ==
                  Gfx10::GE_MULTI_PRIM_IB_RESET_EN__MATCH_ALL_BITS_MASK,
                  "MATCH_ALL_BITS bits are not in the same place on GFX9 and GFX10!");
    static_assert(VGT_MULTI_PRIM_IB_RESET_EN__RESET_EN_MASK ==
                  Gfx10::GE_MULTI_PRIM_IB_RESET_EN__RESET_EN_MASK,
                  "RESET_EN bits are not in the same place on GFX9 and GFX10!");

    regVGT_MULTI_PRIM_IB_RESET_EN vgtMultiPrimIbResetEn = {};

    vgtMultiPrimIbResetEn.bits.RESET_EN = static_cast<uint32>(
        Indexed && m_graphicsState.inputAssemblyState.primitiveRestartEnable);

    m_state.primShaderCbLayout.renderStateCb.primitiveRestartEnable = vgtMultiPrimIbResetEn.bits.RESET_EN;

    m_deCmdStream.CommitCommands(pDeCmdSpace);
    pDeCmdSpace = m_deCmdStream.ReserveCommands();

    if (IsGfx10(m_gfxIpLevel) &&
        (PipelineDirty || (StateDirty && dirtyFlags.triangleRasterState)))
    {
        pDeCmdSpace = Gfx10ValidateTriangleRasterState(pPipeline, pDeCmdSpace);
    }

    if (StateDirty && (dirtyFlags.lineStippleState || dirtyFlags.inputAssemblyState))
    {
        regPA_SC_LINE_STIPPLE paScLineStipple  = {};
        paScLineStipple.bits.REPEAT_COUNT      = m_graphicsState.lineStippleState.lineStippleScale;
        paScLineStipple.bits.LINE_PATTERN      = m_graphicsState.lineStippleState.lineStippleValue;
#if BIGENDIAN_CPU
        paScLineStipple.bits.PATTERN_BIT_ORDER = 1;
#endif
        paScLineStipple.bits.AUTO_RESET_CNTL   =
            (m_graphicsState.inputAssemblyState.topology == PrimitiveTopology::LineStrip) ? 2 : 1;

        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg<Pm4OptImmediate>(mmPA_SC_LINE_STIPPLE,
                                                                           paScLineStipple.u32All,
                                                                           pDeCmdSpace);
    }

    if (PipelineDirty || (StateDirty && dirtyFlags.depthClampOverride))
    {
        PAL_ASSERT((m_dbRenderOverride.u32All & ~PipelineDbRenderOverrideMask) == 0);
        PAL_ASSERT((pPipeline->DbRenderOverride().u32All & ~PipelineDbRenderOverrideMask) == 0);

        regDB_RENDER_OVERRIDE updatedReg = pPipeline->DbRenderOverride();

        // Depth clamping override used by RPM.
        if (m_graphicsState.depthClampOverride.enabled)
        {
            updatedReg.bits.DISABLE_VIEWPORT_CLAMP = m_graphicsState.depthClampOverride.disableViewportClamp;
        }

        // Is the new state different from the last written state?
        if (updatedReg.u32All != m_dbRenderOverride.u32All)
        {
            pDeCmdSpace = m_deCmdStream.WriteContextRegRmw(mmDB_RENDER_OVERRIDE,
                                                           PipelineDbRenderOverrideMask,
                                                           updatedReg.u32All,
                                                           pDeCmdSpace);
            m_dbRenderOverride = updatedReg;
        }
    }

    // Validate the per-draw HW state.
    pDeCmdSpace = ValidateDrawTimeHwState<Indexed, Indirect, Pm4OptImmediate>(paScModeCntl1,
                                                                              dbCountControl,
                                                                              vgtMultiPrimIbResetEn,
                                                                              drawInfo,
                                                                              pDeCmdSpace);

    pDeCmdSpace = m_workaroundState.PreDraw<StateDirty, Pm4OptImmediate>(m_graphicsState,
                                                                         &m_deCmdStream,
                                                                         this,
                                                                         pDeCmdSpace);

    if (IsNgg && (m_pSignatureGfx->nggCullingDataAddr != UserDataNotMapped))
    {
        if (UseCpuPathInsteadOfCeRam() == false)
        {
            pDeCmdSpace = UpdateNggCullingDataBufferWithGpu(pDeCmdSpace);
        }
        else
        {
            pDeCmdSpace = UpdateNggCullingDataBufferWithCpu(pDeCmdSpace);
        }
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
// Gfx10 specific function for calculating Color PBB bin size.
void UniversalCmdBuffer::Gfx10GetColorBinSize(
    Extent2d* pBinSize
    ) const
{
    PAL_ASSERT(IsGfx10(m_gfxIpLevel));

    // TODO: This function needs to be updated to look at the pixel shader and determine which outputs are valid in
    //       addition to looking at the bound render targets. Bound render targets may not necessarily get a pixel
    //       shader export. Using the bound render targets means that we may make the bin size smaller than it needs to
    //       be when a render target is bound, but is not written by the PS. With export cull mask enabled. We need only
    //       examine the PS output because it will account for any RTs that are not bound.

    // Calculate cColor and cFmask(if applicable)
    uint32 cColor   = 0;
    uint32 cFmask   = 0;
    bool   hasFmask = false;

    const auto& boundTargets = m_graphicsState.bindTargets;
    const auto* pPipeline    = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const bool  psIterSample = ((pPipeline != nullptr) && (pPipeline->PaScModeCntl1().bits.PS_ITER_SAMPLE == 1));
    for (uint32  idx = 0; idx < boundTargets.colorTargetCount; idx++)
    {
        const auto* pColorView = static_cast<const ColorTargetView*>(boundTargets.colorTargets[idx].pColorTargetView);
        const auto* pImage     = ((pColorView != nullptr) ? pColorView->GetImage() : nullptr);

        if (pImage != nullptr)
        {
            // mMRT = (num_frag == 1) ? 1 : (ps_iter == 1) ? num_frag : 2
            // cMRT = Bpp * mMRT
            // cColor = Sum(cMRT)
            const auto&  info = pImage->Parent()->GetImageCreateInfo();
            const uint32 mmrt = (info.fragments == 1) ? 1 : (psIterSample ? info.fragments : 2);

            cColor += BytesPerPixel(info.swizzledFormat.format) * mmrt;
            if (pImage->HasFmaskData())
            {
                PAL_ASSERT((info.fragments > 0) && (info.samples > 0));
                const uint32 fragmentsLog2 = (uint32)Log2(info.fragments);
                const uint32 samplesLog2   = (uint32)Log2(info.samples);
                PAL_ASSERT((fragmentsLog2 < 4) && (samplesLog2 < 5));
                static constexpr uint32 cFmaskMrt[4 /* fragments */][5 /* samples */]=
                {
                    { 0, 1, 1, 1, 2 }, // fragments = 1
                    { 0, 1, 1, 2, 4 }, // fragments = 2
                    { 0, 1, 1, 4, 8 }, // fragments = 4
                    { 0, 1, 2, 4, 8 }  // fragments = 8
                };
                cFmask  += cFmaskMrt[fragmentsLog2][samplesLog2];
                hasFmask = true;
            }
        }
    }
    cColor = Max(cColor, 1u);  // cColor 0 to 1 uses cColor=1

    // Calculate Color and Fmask bin sizes
    // The logic for gfx10 bin sizes is based on a formula that accounts for the number of RBs
    // and Channels on the ASIC.  Since this a potentially large amount of combinations,
    // it is not practical to hardcode binning tables into the driver.
    // Note that the final bin size is choosen from minimum between Depth, Color and Fmask.

    // The logic given to calculate the Color bin size is:
    //   colorBinArea = ((CcReadTags * totalNumRbs / totalNumPipes) * (CcTagSize * totalNumPipes)) / cColor
    // The numerator has been pre-calculated as m_colorBinSizeTagPart.
    const uint32 colorLog2Pixels = Log2(m_colorBinSizeTagPart / cColor);
    const uint16 colorBinSizeX   = 1 << ((colorLog2Pixels + 1) / 2); // (Y_BIAS=false) round up width
    const uint16 colorBinSizeY   = 1 << (colorLog2Pixels / 2);       // (Y_BIAS=false) round down height

    uint16 binSizeX = colorBinSizeX;
    uint16 binSizeY = colorBinSizeY;

    if (hasFmask)
    {
        cFmask = Max(cFmask, 1u);  // cFmask 0 to 1 uses cFmask=1

        // The logic given to calculate the Fmask bin size is:
        //   fmaskBinArea =((FcReadTags * totalNumRbs / totalNumPipes) * (FcTagSize * totalNumPipes)) / cFmask
        // The numerator has been pre-calculated as m_fmaskBinSizeTagPart.
        const uint32 fmaskLog2Pixels = Log2(m_fmaskBinSizeTagPart / cFmask);
        const uint32 fmaskBinSizeX   = 1 << ((fmaskLog2Pixels + 1) / 2); // (Y_BIAS=false) round up width
        const uint32 fmaskBinSizeY   = 1 << (fmaskLog2Pixels / 2);       // (Y_BIAS=false) round down height

        // use the smaller of the Color vs. Fmask bin sizes
        if (fmaskLog2Pixels < colorLog2Pixels)
        {
            binSizeX = static_cast<uint16>(fmaskBinSizeX);
            binSizeY = static_cast<uint16>(fmaskBinSizeY);
        }
    }
    // Return size adjusted for minimum bin size
    pBinSize->width  = Max(binSizeX, m_minBinSizeX);
    pBinSize->height = Max(binSizeY, m_minBinSizeY);
}

// =====================================================================================================================
// Gfx10 specific function for calculating Depth PBB bin size.
void UniversalCmdBuffer::Gfx10GetDepthBinSize(
    Extent2d* pBinSize
    ) const
{
    PAL_ASSERT(IsGfx10(m_gfxIpLevel));

    const auto*  pDepthTargetView =
            static_cast<const DepthStencilView*>(m_graphicsState.bindTargets.depthTarget.pDepthStencilView);
    const auto*  pImage           = (pDepthTargetView ? pDepthTargetView->GetImage() : nullptr);

    if ((pImage == nullptr) ||
        ((m_cachedSettings.ignoreDepthForBinSize == true) && (m_graphicsState.bindTargets.colorTargetCount > 0)))
    {
        // Set to max sizes when no depth image bound
        pBinSize->width  = 512;
        pBinSize->height = 512;
    }
    else
    {
        const auto* pDepthStencilState = static_cast<const DepthStencilState*>(m_graphicsState.pDepthStencilState);
        const auto& imageCreateInfo    = pImage->Parent()->GetImageCreateInfo();

        // C_per_sample = ((z_enabled) ? 5 : 0) + ((stencil_enabled) ? 1 : 0)
        // cDepth = 4 * C_per_sample * num_samples
        const uint32 cPerDepthSample   = (pDepthStencilState->IsDepthEnabled() &&
                                          (pDepthTargetView->ReadOnlyDepth() == false)) ? 5 : 0;
        const uint32 cPerStencilSample = (pDepthStencilState->IsStencilEnabled() &&
                                          (pDepthTargetView->ReadOnlyStencil() == false)) ? 1 : 0;
        const uint32 cDepth            = (cPerDepthSample + cPerStencilSample) * imageCreateInfo.samples;

        // The logic for gfx10 bin sizes is based on a formula that accounts for the number of RBs
        // and Channels on the ASIC.  Since this a potentially large amount of combinations,
        // it is not practical to hardcode binning tables into the driver.
        // Note that final bin size is choosen from the minimum between Depth, Color and FMask.

        // The logic given to calculate the Depth bin size is:
        //   depthBinArea = ((ZsReadTags * totalNumRbs / totalNumPipes) * (ZsTagSize * totalNumPipes)) / cDepth
        // The numerator has been pre-calculated as m_depthBinSizeTagPart.
        // Note that cDepth 0 to 1 falls into cDepth=1 bucket
        const uint32 depthLog2Pixels = Log2(m_depthBinSizeTagPart / Max(cDepth, 1u));
        uint16       depthBinSizeX   = 1 << ((depthLog2Pixels + 1) / 2); // (Y_BIAS=false) round up width
        uint16       depthBinSizeY   = 1 << (depthLog2Pixels / 2);       // (Y_BIAS=false) round down height

        // Return size adjusted for minimum bin size
        pBinSize->width  = Max(depthBinSizeX, m_minBinSizeX);
        pBinSize->height = Max(depthBinSizeY, m_minBinSizeY);
    }
}

// =====================================================================================================================
// Fills in m_paScBinnerCntl0(PA_SC_BINNER_CNTL_0 register) with values that corresponds to the
// specified binning mode and sizes.   For disabled binning the caller should pass a bin size of zero(0x0).
// 'pBinSize' will be updated with the actual bin size configured.
// Returns: True if PA_SC_BINNER_CNTL_0 changed value, False otherwise.
bool UniversalCmdBuffer::SetPaScBinnerCntl0(
    const GraphicsPipeline&  pipeline,
    const ColorBlendState*   pColorBlendState,
    Extent2d*                pBinSize,
    bool                     disableDfsm)
{
    const regPA_SC_BINNER_CNTL_0 prevPaScBinnerCntl0 = m_paScBinnerCntl0;

    // If the reported bin sizes are zero, then disable binning
    if ((pBinSize->width == 0) || (pBinSize->height == 0))
    {
        // Note, GetDisableBinningSetting() will update pBinSize if required for binning mode
        m_paScBinnerCntl0.bits.BINNING_MODE = GetDisableBinningSetting(pBinSize);
    }
    else
    {
        m_paScBinnerCntl0.bits.BINNING_MODE = BINNING_ALLOWED;
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

    return (prevPaScBinnerCntl0.u32All != m_paScBinnerCntl0.u32All);
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
template <bool Pm4OptImmediate>
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
            if (IsGfx10(m_gfxIpLevel))
            {
                // Final bin size is choosen from minimum between Depth, Color and Fmask.
                Gfx10GetColorBinSize(&colorBinSize); // returns minimum of Color and Fmask
                Gfx10GetDepthBinSize(&depthBinSize);
            }
            else
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
    if (SetPaScBinnerCntl0(pipeline, pColorBlendState, &binSize, disableDfsm))
    {
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg<Pm4OptImmediate>(mmPA_SC_BINNER_CNTL_0,
                                                                           m_paScBinnerCntl0.u32All,
                                                                           pDeCmdSpace);
    }

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
        pScaleOffsetImg->xOffset.f32All = (viewport.originX + xScale);

        pScaleOffsetImg->yScale.f32All  = yScale * (viewport.origin == PointOrigin::UpperLeft ? 1.0f : -1.0f);
        pScaleOffsetImg->yOffset.f32All = (viewport.originY + yScale);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 524
        if (params.depthRange == DepthRange::NegativeOneToOne)
        {
            pScaleOffsetImg->zScale.f32All  = (viewport.maxDepth - viewport.minDepth) * 0.5f;
            pScaleOffsetImg->zOffset.f32All = (viewport.maxDepth + viewport.minDepth) * 0.5f;
        }
        else
#endif
        {
            pScaleOffsetImg->zScale.f32All  = (viewport.maxDepth - viewport.minDepth);
            pScaleOffsetImg->zOffset.f32All = viewport.minDepth;
        }

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
    if (m_deCmdStream.Pm4OptimizerEnabled())
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
// Validate CB_COLORx_INFO registers. Depends on RTV state for much of the register and Pipeline | Blend for BlendOpt.
template <bool Pm4OptImmediate, bool PipelineDirty, bool StateDirty>
uint32* UniversalCmdBuffer::ValidateCbColorInfo(
    uint32* pDeCmdSpace)
{
    const auto dirtyFlags = m_graphicsState.dirtyFlags.validationBits;

    // If BlendOpt could have changed or color targets changed.
    if (PipelineDirty || (StateDirty && (dirtyFlags.colorBlendState || dirtyFlags.colorTargetView)))
    {
        const auto*const pPipeline     = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
        const bool       blendOptDirty = (PipelineDirty || (StateDirty && dirtyFlags.colorBlendState));
        const bool       rtvDirty      = (StateDirty && dirtyFlags.colorTargetView);

        uint8 cbColorInfoDirtyBlendOpt = 0;

        if ((pPipeline != nullptr) && blendOptDirty)
        {
            const auto*const pBlendState = static_cast<const ColorBlendState*>(m_graphicsState.pColorBlendState);

            // Blend state optimizations are associated with the Blend state object, but the CB state affects which
            // optimizations are chosen. We need to make sure we have the best optimizations chosen, so we write it
            // at draw time only if it is dirty.
            if (pBlendState != nullptr)
            {
                cbColorInfoDirtyBlendOpt = pBlendState->WriteBlendOptimizations(
                    &m_deCmdStream,
                    pPipeline->TargetFormats(),
                    pPipeline->TargetWriteMasks(),
                    m_cachedSettings.blendOptimizationsEnable,
                    &m_blendOpts[0],
                    m_cbColorInfo);
            }
        }

        if (m_state.flags.cbColorInfoDirtyRtv || cbColorInfoDirtyBlendOpt)
        {
            for (uint32 x = 0; x < MaxColorTargets; x++)
            {
                const bool slotDirtyRtv      = BitfieldIsSet(m_state.flags.cbColorInfoDirtyRtv, x);
                const bool slotDirtyBlendOpt = BitfieldIsSet(cbColorInfoDirtyBlendOpt, x);

                // If root CmdBuf or all state is has been set at some point on Nested, can simply set the register.
                if (IsNested() == false)
                {
                    if (slotDirtyRtv || slotDirtyBlendOpt)
                    {
                        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg<Pm4OptImmediate>(
                            (mmCB_COLOR0_INFO + (x * CbRegsPerSlot)),
                            m_cbColorInfo[x].u32All,
                            pDeCmdSpace);
                    }
                }
                // If on the NestedCmd buf and only partial state known must use RMW
                else
                {
                    if (slotDirtyRtv)
                    {
                        pDeCmdSpace = m_deCmdStream.WriteContextRegRmw((mmCB_COLOR0_INFO + (x * CbRegsPerSlot)),
                                                                       ColorTargetView::CbColorInfoMask,
                                                                       m_cbColorInfo[x].u32All,
                                                                       pDeCmdSpace);
                    }
                    if (slotDirtyBlendOpt)
                    {
                        pDeCmdSpace = m_deCmdStream.WriteContextRegRmw((mmCB_COLOR0_INFO + (x * CbRegsPerSlot)),
                                                                       ~ColorTargetView::CbColorInfoMask,
                                                                       m_cbColorInfo[x].u32All,
                                                                       pDeCmdSpace);
                    }
                }
            }

            // Track state written over the course of the entire CmdBuf. Needed for Nested CmdBufs to know what
            // state to leak back to the root CmdBuf.
            m_leakCbColorInfoRtv   |= m_state.flags.cbColorInfoDirtyRtv;

            m_state.flags.cbColorInfoDirtyRtv = 0;
        }
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Wrapper for the real NeedsToValidateScissorRects() for when the caller doesn't know if the immediate mode pm4
// optimizer is enabled.
bool UniversalCmdBuffer::NeedsToValidateScissorRects() const
{
    return NeedsToValidateScissorRects(m_deCmdStream.Pm4OptimizerEnabled());
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
                             dirtyFlags.validationBits.lineStippleState        ||
                             dirtyFlags.nonValidationBits.streamOutTargets     ||
                             dirtyFlags.nonValidationBits.globalScissorState   ||
                             dirtyFlags.nonValidationBits.blendConstState      ||
                             dirtyFlags.nonValidationBits.depthBiasState       ||
                             dirtyFlags.nonValidationBits.depthBoundsState     ||
                             dirtyFlags.nonValidationBits.pointLineRasterState ||
                             dirtyFlags.nonValidationBits.stencilRefMaskState  ||
                             dirtyFlags.nonValidationBits.clipRectsState       ||
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
    if (m_deCmdStream.Pm4OptimizerEnabled())
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
// Translates the supplied IA_MULTI_VGT_PARAM register to its equivalent GE_CNTL value
template <bool IsNgg>
uint32 UniversalCmdBuffer::CalcGeCntl(
    bool                  usesLineStipple,
    regIA_MULTI_VGT_PARAM iaMultiVgtParam
    ) const
{
    const     auto*  pPalPipeline         = m_graphicsState.pipelineState.pPipeline;
    const     auto*  pPipeline            = static_cast<const GraphicsPipeline*>(pPalPipeline);
    const     bool   isTess               = IsTessEnabled();
    const     bool   isNggFastLaunch      = pPipeline->IsNggFastLaunch();
    const     bool   disableVertGrouping  = (m_cachedSettings.disableVertGrouping &&
                                             (isNggFastLaunch == false)           &&
                                             (pPipeline->NggSubgroupSize() == 0));
    constexpr uint32 VertGroupingDisabled = 256;

    regGE_CNTL  geCntl = {};

    if ((IsNgg == false) || isTess)
    {
        // PRIMGROUP_SIZE is zero-based (i.e., zero means one) but PRIM_GRP_SIZE is one based (i.e., one means one).
        geCntl.bits.PRIM_GRP_SIZE = iaMultiVgtParam.bits.PRIMGROUP_SIZE + 1;

        // Recomendation to disable VERT_GRP_SIZE is to set it to 256.
        geCntl.bits.VERT_GRP_SIZE = VertGroupingDisabled;
    }
    else
    {
        const regVGT_GS_ONCHIP_CNTL vgtGsOnchipCntl = pPipeline->VgtGsOnchipCntl();

        geCntl.bits.PRIM_GRP_SIZE = vgtGsOnchipCntl.bits.GS_PRIMS_PER_SUBGRP;
        geCntl.bits.VERT_GRP_SIZE =
            (disableVertGrouping)                       ? VertGroupingDisabled :
            (m_cachedSettings.waClampGeCntlVertGrpSize) ? vgtGsOnchipCntl.bits.ES_VERTS_PER_SUBGRP - 5 :
                                                          vgtGsOnchipCntl.bits.ES_VERTS_PER_SUBGRP;
    }

    //  These numbers below come from the hardware restrictions.
    PAL_ASSERT((IsGfx101(*m_device.Parent()) == false) || (geCntl.bits.VERT_GRP_SIZE >= 24));

    // Only used for line-stipple
    geCntl.bits.PACKET_TO_ONE_PA = usesLineStipple;

    //  ... "the only time break_wave_at_eoi is needed, is for primitive_id/patch_id with tessellation."
    //  ... "I think every DS requires a valid PatchId".
    geCntl.bits.BREAK_WAVE_AT_EOI = isTess;

    geCntl.bits.PACKET_TO_ONE_PA  = (iaMultiVgtParam.bits.WD_SWITCH_ON_EOP && iaMultiVgtParam.bits.SWITCH_ON_EOP);

    return geCntl.u32All;
}

// =====================================================================================================================
// Update the HW state and write the necessary packets to push any changes to the HW. Returns the next unused DWORD
// in pDeCmdSpace.
template <bool Indexed, bool Indirect, bool Pm4OptImmediate>
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

        // GFX10 moves the RESET_EN functionality into a new register that happens to exist in the same place
        // as the GFX9 register.
        static_assert(Gfx09::mmVGT_MULTI_PRIM_IB_RESET_EN ==
                      Gfx10::mmGE_MULTI_PRIM_IB_RESET_EN,
                      "MULTI_PRIM_IB_RESET_EN has moved from GFX9 to GFX10!");

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
                    m_drawTimeHwState.dirty.indexBufferBase = 0;
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
#if PAL_BUILD_PM4_INSTRUMENTOR
    uint32 startingCmdLen = 0;
    uint32 pipelineCmdLen = 0;
    uint32 userDataCmdLen = 0;
    if (m_cachedSettings.enablePm4Instrumentation != 0)
    {
        // GetUsedSize() is not accurate if called inside a Reserve/Commit block.
        m_deCmdStream.CommitCommands(pDeCmdSpace);
        startingCmdLen = GetUsedSize(CommandDataAlloc);
        pDeCmdSpace    = m_deCmdStream.ReserveCommands();
    }
#endif

    if (m_computeState.pipelineState.dirtyFlags.pipelineDirty)
    {
        const auto*const pNewPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);

        pDeCmdSpace = pNewPipeline->WriteCommands(&m_deCmdStream,
                                                  pDeCmdSpace,
                                                  m_computeState.dynamicCsInfo,
                                                  m_buildFlags.prefetchShaders);

#if PAL_BUILD_PM4_INSTRUMENTOR
        if (m_cachedSettings.enablePm4Instrumentation != 0)
        {
            // GetUsedSize() is not accurate if called inside a Reserve/Commit block.
            m_deCmdStream.CommitCommands(pDeCmdSpace);
            pipelineCmdLen  = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
            startingCmdLen += pipelineCmdLen;
            pDeCmdSpace     = m_deCmdStream.ReserveCommands();
        }
#endif

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

#if PAL_BUILD_PM4_INSTRUMENTOR
    if (m_cachedSettings.enablePm4Instrumentation != 0)
    {
        // GetUsedSize() is not accurate if called inside a Reserve/Commit block.
        m_deCmdStream.CommitCommands(pDeCmdSpace);
        userDataCmdLen  = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
        startingCmdLen += userDataCmdLen;
        pDeCmdSpace     = m_deCmdStream.ReserveCommands();
    }
#endif

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

    if (IsGfx10(m_gfxIpLevel))
    {
        const regCOMPUTE_DISPATCH_TUNNEL dispatchTunnel = { };
        pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderCompute>(Gfx10::mmCOMPUTE_DISPATCH_TUNNEL,
                                                                    dispatchTunnel.u32All,
                                                                    pDeCmdSpace);
    }

#if PAL_BUILD_PM4_INSTRUMENTOR
    if (m_cachedSettings.enablePm4Instrumentation != 0)
    {
        // GetUsedSize() is not accurate if called inside a Reserve/Commit block.
        m_deCmdStream.CommitCommands(pDeCmdSpace);
        const uint32 miscCmdLen = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
        pDeCmdSpace = m_deCmdStream.ReserveCommands();

        m_device.DescribeDrawDispatchValidation(this, userDataCmdLen, pipelineCmdLen, miscCmdLen);
    }
#endif

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
void UniversalCmdBuffer::CmdLoadBufferFilledSizes(
    const gpusize (&gpuVirtAddr)[MaxStreamOutTargets])
{
    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    for (uint32 idx = 0; idx < MaxStreamOutTargets; ++idx)
    {
        if (gpuVirtAddr[idx] != 0)
        {
            pDeCmdSpace +=
                CmdUtil::BuildStrmoutBufferUpdate(idx,
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
            pDeCmdSpace += CmdUtil::BuildStrmoutBufferUpdate(idx,
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

    pDeCmdSpace += CmdUtil::BuildStrmoutBufferUpdate(bufferId,
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
    const bool HasActiveQuery = IsQueryActive(QueryPoolType::Occlusion) &&
                                (NumActiveQueries(QueryPoolType::Occlusion) != 0);

    if (HasActiveQuery)
    {
        // Only update the value of DB_COUNT_CONTROL if there are active queries. If no queries are active,
        // the new SAMPLE_RATE value is ignored by the HW and the register will be written the next time a query
        // is activated.
        pDbCountControl->bits.SAMPLE_RATE = log2SampleRate;
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

    if (HasActiveQuery ||
        (IsNested() && m_graphicsState.inheritedState.stateFlags.occlusionQuery))
    {
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

        if (IsGfx10(m_gfxIpLevel))
        {
            pDbCountControl->gfx10.DISABLE_CONSERVATIVE_ZPASS_COUNTS = 1;
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
    // The following primitive types support 4x primitive rate with reset index (except for gfx9):
    //    - Point list
    //    - Line strip
    //    - Triangle strip
    // add draw opaque.

    const PrimitiveTopology primTopology = m_graphicsState.inputAssemblyState.topology;
    const bool              primitiveRestartEnabled = m_graphicsState.inputAssemblyState.primitiveRestartEnable;
    bool                    restartPrimsCheck = (primTopology != PrimitiveTopology::PointList) &&
                                                (primTopology != PrimitiveTopology::LineStrip) &&
                                                (primTopology != PrimitiveTopology::TriangleStrip);

    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        // Disable 4x primrate for all primitives when reset index is enabled on gfx9 devices.
        restartPrimsCheck = true;
    }

    bool switchOnEop = ((primTopology == PrimitiveTopology::TriangleStripAdj) ||
                        (primTopology == PrimitiveTopology::TriangleFan) ||
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 557
                        (primTopology == PrimitiveTopology::LineLoop) ||
                        (primTopology == PrimitiveTopology::Polygon) ||
#endif
                        (primitiveRestartEnabled && restartPrimsCheck) ||
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
    pDeCmdSpace += CmdUtil::BuildWaitRegMem(EngineTypeUniversal,
                                            mem_space__pfp_wait_reg_mem__register_space,
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

    if (memcmp(&newGraphicsState.lineStippleState,
               &m_graphicsState.lineStippleState,
               sizeof(LineStippleStateParams)) != 0)
    {
        CmdSetLineStippleState(newGraphicsState.lineStippleState);
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 524
        (restoreViewports.depthRange != currentViewports.depthRange) ||
#endif
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

    const auto& restoreClipRects = newGraphicsState.clipRectsState;
    const auto& currentClipRects = m_graphicsState.clipRectsState;

    if ((restoreClipRects.clipRule != currentClipRects.clipRule)   ||
        (restoreClipRects.rectCount != currentClipRects.rectCount) ||
        (memcmp(&restoreClipRects.rectList[0],
                &currentClipRects.rectList[0],
                restoreClipRects.rectCount * sizeof(Rect))))
    {
        CmdSetClipRects(newGraphicsState.clipRectsState.clipRule,
                        newGraphicsState.clipRectsState.rectCount,
                        newGraphicsState.clipRectsState.rectList);
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

    // Was "CmdSetVertexBuffers" ever called on the parent command buffer?
    if (pUniversalCmdBuffer->m_vbTable.modified != 0)
    {
        // Yes, so we need to copy all the VB SRDs into this command buffer as well.
        m_vbTable.modified  = 1;
        m_vbTable.watermark = pUniversalCmdBuffer->m_vbTable.watermark;
        memcpy(m_vbTable.pSrds, pUniversalCmdBuffer->m_vbTable.pSrds, (sizeof(BufferSrd) * MaxVertexBuffers));

        if (UseCpuPathInsteadOfCeRam() == false)
        {
            uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
            pCeCmdSpace = UploadToUserDataTable(&m_vbTable.state,
                                                0,
                                                m_vbTable.state.sizeInDwords,
                                                reinterpret_cast<uint32*>(m_vbTable.pSrds),
                                                m_vbTable.watermark,
                                                pCeCmdSpace);
            m_ceCmdStream.CommitCommands(pCeCmdSpace);
        }
        else
        {
            // If the CPU update path is active, then set the "dirty" flag here to trigger the CPU
            // update path in "ValidateGraphicsUserDataCpu".
            m_vbTable.state.dirty = 1;
        }
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
    pCeCmdSpace += CmdUtil::BuildLoadConstRam(srcGpuMemory.Desc().gpuVirtAddr + memOffset,
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
        pCeCmdSpace += CmdUtil::BuildWaitOnDeCounterDiff(m_state.minCounterDiff, pCeCmdSpace);
        m_state.flags.ceWaitOnDeCounterDiff = 0;
    }

    // Keep track of the latest DUMP_CONST_RAM packet before the upcoming draw or dispatch.  The last one before the
    // draw or dispatch will be updated to set the increment_ce bit at draw-time.
    m_state.pLastDumpCeRam                    = pCeCmdSpace;
    m_state.lastDumpCeRamOrdinal2.bits.offset = (ReservedCeRamBytes + ramOffset);

    pCeCmdSpace += CmdUtil::BuildDumpConstRam(dstGpuMemory.Desc().gpuVirtAddr + memOffset,
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
    pCeCmdSpace += CmdUtil::BuildWriteConstRam(pSrcData, (ReservedCeRamBytes + ramOffset), dwordSize, pCeCmdSpace);
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
                pCmdSpace += m_device.CmdUtil().BuildWaitRegMem(EngineTypeUniversal,
                                                                mem_space__me_wait_reg_mem__register_space,
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

    pCmdSpace += CmdUtil::BuildWaitRegMem(EngineTypeUniversal,
                                          mem_space__me_wait_reg_mem__register_space,
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

    pCmdSpace += CmdUtil::BuildWaitRegMem(EngineTypeUniversal,
                                          mem_space__me_wait_reg_mem__memory_space,
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

    pCmdSpace += CmdUtil::BuildWaitRegMem(EngineTypeUniversal,
                                          mem_space__me_wait_reg_mem__memory_space,
                                          CmdUtil::WaitRegMemFunc(compareFunc),
                                          engine_sel__me_wait_reg_mem__micro_engine,
                                          pGpuMemory->GetBusAddrMarkerVa(),
                                          data,
                                          mask,
                                          pCmdSpace);

    m_deCmdStream.CommitCommands(pCmdSpace);
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 509
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
#endif

// =====================================================================================================================
void UniversalCmdBuffer::CmdUpdateHiSPretests(
    const IImage*      pImage,
    const HiSPretests& pretests,
    uint32             firstMip,
    uint32             numMips)
{
    Image* pGfx9Image = static_cast<Image*>(static_cast<const Pal::Image*>(pImage)->GetGfxImage());

    if (pGfx9Image->HasHiSPretestsMetaData())
    {
        SubresRange range = { };
        range.startSubres = { ImageAspect::Stencil, firstMip, 0 };
        range.numMips     = numMips;
        range.numSlices   = pImage->GetImageCreateInfo().arraySize;

        const Pm4Predicate packetPredicate = PacketPredicate();

        uint32* pCmdSpace                  = m_deCmdStream.ReserveCommands();

        pCmdSpace = pGfx9Image->UpdateHiSPretestsMetaData(range, pretests, packetPredicate, pCmdSpace);

        if (m_graphicsState.bindTargets.depthTarget.pDepthStencilView != nullptr)
        {
            const DepthStencilView* const pView =
                static_cast<const DepthStencilView*>(m_graphicsState.bindTargets.depthTarget.pDepthStencilView);

            // If the bound image matches the cleared image, we update DB_SRESULTS_COMPARE_STATE0/1 immediately.
            if ((pView->GetImage() == pGfx9Image) &&
                (pView->MipLevel() >= range.startSubres.mipLevel) &&
                (pView->MipLevel() < range.startSubres.mipLevel + range.numMips))
            {
                Gfx9HiSPretestsMetaData pretestsMetaData = {};

                pretestsMetaData.dbSResultCompare0.bitfields.COMPAREFUNC0  =
                    DepthStencilState::HwStencilCompare(pretests.test[0].func);
                pretestsMetaData.dbSResultCompare0.bitfields.COMPAREMASK0  = pretests.test[0].mask;
                pretestsMetaData.dbSResultCompare0.bitfields.COMPAREVALUE0 = pretests.test[0].value;
                pretestsMetaData.dbSResultCompare0.bitfields.ENABLE0       = pretests.test[0].isValid;

                pretestsMetaData.dbSResultCompare1.bitfields.COMPAREFUNC1  =
                    DepthStencilState::HwStencilCompare(pretests.test[1].func);
                pretestsMetaData.dbSResultCompare1.bitfields.COMPAREMASK1  = pretests.test[1].mask;
                pretestsMetaData.dbSResultCompare1.bitfields.COMPAREVALUE1 = pretests.test[1].value;
                pretestsMetaData.dbSResultCompare1.bitfields.ENABLE1       = pretests.test[1].isValid;

                pCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmDB_SRESULTS_COMPARE_STATE0,
                                                                 mmDB_SRESULTS_COMPARE_STATE1,
                                                                 &pretestsMetaData,
                                                                 pCmdSpace);

            }
        }

        m_deCmdStream.CommitCommands(pCmdSpace);
    }

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
    PAL_ASSERT(
        (predType != PredicateType::Boolean32) ||
        (m_device.Parent()->EngineProperties().perEngine[EngineTypeUniversal].flags.memory32bPredicationSupport != 0)
    );

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

    pDeCmdSpace += CmdUtil::BuildSetPredication(gpuVirtAddr,
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
    pCmdSpace += CmdUtil::BuildDmaData(dmaData, pCmdSpace);

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
void UniversalCmdBuffer::CmdNop(
    const void* pPayload,
    uint32      payloadSize)
{
    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace += m_cmdUtil.BuildNopPayload(pPayload, payloadSize, pDeCmdSpace);
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
        m_vertexOffsetReg     = cmdBuffer.m_vertexOffsetReg;
        m_drawIndexReg        = cmdBuffer.m_drawIndexReg;
        m_nggState.numSamples = cmdBuffer.m_nggState.numSamples;
        m_dbRenderOverride    = cmdBuffer.m_dbRenderOverride;

        // Update the functions that are modified by nested command list
        m_pfnValidateUserDataGfx                    = cmdBuffer.m_pfnValidateUserDataGfx;
        m_pfnValidateUserDataGfxPipelineSwitch      = cmdBuffer.m_pfnValidateUserDataGfxPipelineSwitch;
        m_funcTable.pfnCmdDraw                      = cmdBuffer.m_funcTable.pfnCmdDraw;
        m_funcTable.pfnCmdDrawOpaque                = cmdBuffer.m_funcTable.pfnCmdDrawOpaque;
        m_funcTable.pfnCmdDrawIndexed               = cmdBuffer.m_funcTable.pfnCmdDrawIndexed;
        m_funcTable.pfnCmdDrawIndirectMulti         = cmdBuffer.m_funcTable.pfnCmdDrawIndirectMulti;
        m_funcTable.pfnCmdDrawIndexedIndirectMulti  = cmdBuffer.m_funcTable.pfnCmdDrawIndexedIndirectMulti;

        if (m_cachedSettings.rbPlusSupported != 0)
        {
            m_sxPsDownconvert   = cmdBuffer.m_sxPsDownconvert;
            m_sxBlendOptEpsilon = cmdBuffer.m_sxBlendOptEpsilon;
            m_sxBlendOptControl = cmdBuffer.m_sxBlendOptControl;
        }
    }

    // Leak back valid CB_COLORx_INFO state.
    for (uint32 x = 0; x < MaxColorTargets; x++)
    {
        if (BitfieldIsSet(cmdBuffer.m_leakCbColorInfoRtv, x))
        {
            BitfieldUpdateSubfield(
                &(m_cbColorInfo[x].u32All), cmdBuffer.m_cbColorInfo[x].u32All, ColorTargetView::CbColorInfoMask);
        }

        // NestCmd buffer always updates BlendOpt.
        BitfieldUpdateSubfield(
            &(m_cbColorInfo[x].u32All), cmdBuffer.m_cbColorInfo[x].u32All, ~ColorTargetView::CbColorInfoMask);
    }

    // If the nested command buffer updated PA_SC_CONS_RAST_CNTL, leak its state back to the caller.
    if ((cmdBuffer.m_graphicsState.pipelineState.pPipeline != nullptr) ||
        (cmdBuffer.m_graphicsState.leakFlags.validationBits.msaaState))
    {
        m_paScConsRastCntl.u32All = cmdBuffer.m_paScConsRastCntl.u32All;
    }

    // If the nested command buffer updated color target view (and implicitly big_page settings), leak the state back to
    // caller as the state tracking is needed for correctly making the WA.
    if (cmdBuffer.m_graphicsState.leakFlags.validationBits.colorTargetView)
    {
        m_cbRmiGl2CacheControl.bits.COLOR_BIG_PAGE = cmdBuffer.m_cbRmiGl2CacheControl.bits.COLOR_BIG_PAGE;
        m_cbRmiGl2CacheControl.bits.FMASK_BIG_PAGE = cmdBuffer.m_cbRmiGl2CacheControl.bits.FMASK_BIG_PAGE;
    }

    // DB_DFSM_CONTROL is written at AddPreamble time for all CmdBuffer states and potentially turned off
    // at draw-time based on Pipeline, MsaaState and DepthStencil Buffer. Always leak back since the nested
    // cmd buffer always updated the register.
    m_dbDfsmControl.u32All = cmdBuffer.m_dbDfsmControl.u32All;

    // This state is also always updated by the nested command buffer and should leak back.
    m_paScAaConfigNew.u32All  = cmdBuffer.m_paScAaConfigNew.u32All;
    m_paScAaConfigLast.u32All = cmdBuffer.m_paScAaConfigLast.u32All;

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

    m_pipelineCtxRegHash   = cmdBuffer.m_pipelineCtxRegHash;
    m_pipelineCfgRegHash   = cmdBuffer.m_pipelineCfgRegHash;
    m_pipelinePsHash       = cmdBuffer.m_pipelinePsHash;
    m_pipelineFlags.u32All = cmdBuffer.m_pipelineFlags.u32All;

    if (cmdBuffer.m_graphicsState.pipelineState.dirtyFlags.pipelineDirty ||
        (cmdBuffer.m_graphicsState.pipelineState.pPipeline != nullptr))
    {
        m_spiPsInControl = cmdBuffer.m_spiPsInControl;
        m_spiVsOutConfig = cmdBuffer.m_spiVsOutConfig;
        m_vgtLsHsConfig  = cmdBuffer.m_vgtLsHsConfig;
        m_geCntl         = cmdBuffer.m_geCntl;
    }

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
        else if (IsGfx10(m_gfxIpLevel))
        {
            srdNumRecords = pBufferSrd->gfx10.num_records;
            srdStride     = pBufferSrd->gfx10.stride;
        }

        if ((srdNumRecords != numRecords) || (srdStride != strideInBytes))
        {
            if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
            {
                pBufferSrd->gfx9.word2.bits.NUM_RECORDS = numRecords;
                pBufferSrd->gfx9.word1.bits.STRIDE      = strideInBytes;
            }
            else if (IsGfx10(m_gfxIpLevel))
            {
                pBufferSrd->gfx10.num_records = numRecords;
                pBufferSrd->gfx10.stride      = strideInBytes;
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
            pCeCmdSpace += CmdUtil::BuildWriteConstRam(VoidPtrInc(pBufferSrd, sizeof(uint32)),
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
    PAL_ASSERT((planeCount > 0) && (planeCount <= 6));

    // Make sure that the layout of Pal::UserClipPlane is equivalent to the layout of the PA_CL_UCP_* registers.  This
    // lets us skip copying the data around an extra time.
    static_assert((offsetof(UserClipPlane, x) == 0) &&
                  (offsetof(UserClipPlane, y) == 4) &&
                  (offsetof(UserClipPlane, z) == 8) &&
                  (offsetof(UserClipPlane, w) == 12),
                  "The layout of Pal::UserClipPlane must match the layout of the PA_CL_UCP* registers!");

    constexpr uint16 RegStride = (mmPA_CL_UCP_1_X - mmPA_CL_UCP_0_X);
    const uint16 startRegAddr  = static_cast<uint16>(mmPA_CL_UCP_0_X + (firstPlane * RegStride));
    const uint16 endRegAddr    = static_cast<uint16>(mmPA_CL_UCP_0_W + ((firstPlane + planeCount - 1) * RegStride));

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(startRegAddr, endRegAddr, pPlanes, pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
    m_deCmdStream.SetContextRollDetected<true>();
}

// =====================================================================================================================
// Sets clip rects.
void UniversalCmdBuffer::CmdSetClipRects(
    uint16      clipRule,
    uint32      rectCount,
    const Rect* pRectList)
{
    PAL_ASSERT(rectCount <= MaxClipRects);

    m_graphicsState.clipRectsState.clipRule  = clipRule;
    m_graphicsState.clipRectsState.rectCount = rectCount;
    for (uint32 i = 0; i < rectCount; i++)
    {
        m_graphicsState.clipRectsState.rectList[i] = pRectList[i];
    }
    m_graphicsState.dirtyFlags.nonValidationBits.clipRectsState = 1;

    constexpr uint32 RegStride = (mmPA_SC_CLIPRECT_1_TL - mmPA_SC_CLIPRECT_0_TL);
    const uint32 endRegAddr    = (mmPA_SC_CLIPRECT_RULE + rectCount * RegStride);

    struct
    {
        regPA_SC_CLIPRECT_RULE paScClipRectRule;
        struct
        {
            regPA_SC_CLIPRECT_0_TL tl;
            regPA_SC_CLIPRECT_0_BR br;
        } paScClipRect[MaxClipRects];
    } regs; // Intentionally not initialized!

    regs.paScClipRectRule.u32All = 0;
    regs.paScClipRectRule.bits.CLIP_RULE = clipRule;

    for (uint32 r = 0; r < rectCount; ++r)
    {
        regs.paScClipRect[r].tl.bits.TL_X = pRectList[r].offset.x;
        regs.paScClipRect[r].tl.bits.TL_Y = pRectList[r].offset.y;
        regs.paScClipRect[r].br.bits.BR_X = pRectList[r].offset.x + pRectList[r].extent.width;
        regs.paScClipRect[r].br.bits.BR_Y = pRectList[r].offset.y + pRectList[r].extent.height;
    }

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmPA_SC_CLIPRECT_RULE, endRegAddr, &regs, pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
    m_deCmdStream.SetContextRollDetected<true>();
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdXdmaWaitFlipPending()
{
    // Note that we only have an auto-generated version of this register for Vega 12 but it should exist on all ASICs.
    CmdWaitRegisterValue(Vg12::mmXDMA_SLV_FLIP_PENDING, 0, 0x00000001, CompareFunc::Equal);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32            cmdBufferCount,
    ICmdBuffer*const* ppCmdBuffers)
{
    // Need to validate some state as it is valid for root CmdBuf to set state, not issue a draw and expect
    // that state to inherit into the nested CmdBuf. It might be safest to just ValidateDraw here eventually.
    // That would break the assumption that the Pipeline is bound at draw-time.
    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    if (m_graphicsState.pipelineState.dirtyFlags.pipelineDirty)
    {
        if (m_graphicsState.dirtyFlags.validationBits.u16All)
        {
            pDeCmdSpace = ValidateCbColorInfo<false, true, true>(pDeCmdSpace);
        }
        else
        {
            pDeCmdSpace = ValidateCbColorInfo<false, true, false>(pDeCmdSpace);
        }
    }
    else
    {
        if (m_graphicsState.dirtyFlags.validationBits.u16All)
        {
            pDeCmdSpace = ValidateCbColorInfo<false, false, true>(pDeCmdSpace);
        }
        else
        {
            pDeCmdSpace = ValidateCbColorInfo<false, false, false>(pDeCmdSpace);
        }
    }
    m_deCmdStream.CommitCommands(pDeCmdSpace);

    for (uint32 buf = 0; buf < cmdBufferCount; ++buf)
    {
        auto*const pCallee = static_cast<Gfx9::UniversalCmdBuffer*>(ppCmdBuffers[buf]);
        PAL_ASSERT(pCallee != nullptr);

        // Track the most recent OS paging fence value across all nested command buffers called from this one.
        m_lastPagingFence = Max(m_lastPagingFence, pCallee->LastPagingFence());

        // All user-data entries have been uploaded into CE RAM and GPU memory, so we can safely "call" the nested
        // command buffer's command streams.

        const bool exclusiveSubmit  = pCallee->IsExclusiveSubmit();
        const bool allowIb2Launch   = (pCallee->AllowLaunchViaIb2() &&
                                       ((pCallee->m_state.flags.containsDrawIndirect == 0) ||
                                       (IsGfx10(m_gfxIpLevel) == true)));
        const bool allowIb2LaunchCe = (allowIb2Launch && (m_cachedSettings.waCeDisableIb2 == 0));

        m_deCmdStream.TrackNestedEmbeddedData(pCallee->m_embeddedData.chunkList);
        m_deCmdStream.TrackNestedEmbeddedData(pCallee->m_gpuScratchMem.chunkList);
        m_deCmdStream.TrackNestedCommands(pCallee->m_deCmdStream);
        m_ceCmdStream.TrackNestedCommands(pCallee->m_ceCmdStream);

        m_deCmdStream.Call(pCallee->m_deCmdStream, exclusiveSubmit, allowIb2Launch);
        m_ceCmdStream.Call(pCallee->m_ceCmdStream, exclusiveSubmit, allowIb2LaunchCe);

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

    pDeCmdSpace += CmdUtil::BuildAtomicMem(AtomicOp::IncUint32,
                                           frameCountGpuAddr,
                                           UINT32_MAX,
                                           pDeCmdSpace);

    pDeCmdSpace += CmdUtil::BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
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
    if (m_cachedSettings.rbPlusSupported != 0)
    {
        pPipeline->OverrideRbPlusRegistersForRpm(format,
                                                 targetIndex,
                                                 &m_sxPsDownconvert,
                                                 &m_sxBlendOptEpsilon,
                                                 &m_sxBlendOptControl);
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
        PM4_PFP_CONTEXT_CONTROL contextControl = m_device.GetContextControl();

        contextControl.bitfields3.shadow_per_context_state = 0;
        contextControl.bitfields3.shadow_cs_sh_regs        = 0;
        contextControl.bitfields3.shadow_gfx_sh_regs       = 0;
        contextControl.bitfields3.shadow_global_config     = 0;
        contextControl.bitfields3.shadow_global_uconfig    = 0;

        uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
        pCmdSpace += CmdUtil::BuildContextControl(contextControl, pCmdSpace);
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
        PM4_PFP_CONTEXT_CONTROL contextControl = m_device.GetContextControl();

        uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
        pCmdSpace += CmdUtil::BuildContextControl(contextControl, pCmdSpace);
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
template <bool ViewInstancing, bool HasUavExport, bool IssueSqtt, bool DescribeDrawDispatch>
void UniversalCmdBuffer::SwitchDrawFunctionsInternal()
{
    m_funcTable.pfnCmdDraw
        = CmdDraw<IssueSqtt, HasUavExport, ViewInstancing, DescribeDrawDispatch>;
    m_funcTable.pfnCmdDrawOpaque
        = CmdDrawOpaque<IssueSqtt, HasUavExport, ViewInstancing, DescribeDrawDispatch>;
    m_funcTable.pfnCmdDrawIndirectMulti
        = CmdDrawIndirectMulti<IssueSqtt, ViewInstancing, DescribeDrawDispatch>;
    m_funcTable.pfnCmdDrawIndexed
        = CmdDrawIndexed<IssueSqtt, HasUavExport, ViewInstancing, DescribeDrawDispatch>;
    m_funcTable.pfnCmdDrawIndexedIndirectMulti
        = CmdDrawIndexedIndirectMulti<IssueSqtt, ViewInstancing, DescribeDrawDispatch>;
}

// =====================================================================================================================
// Switch draw functions - overloaded internal implementation for switching function params to template params
template <bool ViewInstancing, bool IssueSqtt, bool DescribeDrawDispatch>
void UniversalCmdBuffer::SwitchDrawFunctionsInternal(
    bool hasUavExport)
{
    if (hasUavExport)
    {
        SwitchDrawFunctionsInternal<ViewInstancing, true, IssueSqtt, DescribeDrawDispatch>();
    }
    else
    {
        SwitchDrawFunctionsInternal<ViewInstancing, false, IssueSqtt, DescribeDrawDispatch>();
    }
}

// =====================================================================================================================
// Switch draw functions - overloaded internal implementation for switching function params to template params
template <bool IssueSqtt, bool DescribeDrawDispatch>
void UniversalCmdBuffer::SwitchDrawFunctionsInternal(
    bool hasUavExport,
    bool viewInstancingEnable)
{
    if (viewInstancingEnable)
    {
        SwitchDrawFunctionsInternal<true, IssueSqtt, DescribeDrawDispatch>(hasUavExport);
    }
    else
    {
        SwitchDrawFunctionsInternal<false, IssueSqtt, DescribeDrawDispatch>(hasUavExport);
    }
}

// =====================================================================================================================
// Switch draw functions.
void UniversalCmdBuffer::SwitchDrawFunctions(
    bool hasUavExport,
    bool viewInstancingEnable)
{
    if (m_cachedSettings.issueSqttMarkerEvent)
    {
        PAL_ASSERT(m_cachedSettings.describeDrawDispatch == 1);
        SwitchDrawFunctionsInternal<true, true>(hasUavExport, viewInstancingEnable);
    }
    else if (m_cachedSettings.describeDrawDispatch)
    {
        SwitchDrawFunctionsInternal<false, true>(hasUavExport, viewInstancingEnable);
    }
    else
    {
        SwitchDrawFunctionsInternal<false, false>(hasUavExport, viewInstancingEnable);
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
    pCmdSpace += CmdUtil::BuildDmaData(dmaDataInfo, pCmdSpace);
    m_deCmdStream.CommitCommands(pCmdSpace);

    SetGfxCmdBufCpBltState(true);
    SetGfxCmdBufCpBltWriteCacheState(true);
}

} // Gfx9
} // Pal
