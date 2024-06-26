/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "g_platformSettings.h"
#include "core/hw/gfxip/gfx9/gfx9Barrier.h"
#include "core/hw/gfxip/gfx9/gfx9BorderColorPalette.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9ColorBlendState.h"
#include "core/hw/gfxip/gfx9/gfx9ColorTargetView.h"
#include "core/hw/gfxip/gfx9/gfx9ComputePipeline.h"
#include "core/hw/gfxip/gfx9/gfx9DepthStencilState.h"
#include "core/hw/gfxip/gfx9/gfx9DepthStencilView.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9FormatInfo.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9HybridGraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9IndirectCmdGenerator.h"
#include "core/hw/gfxip/gfx9/gfx9MsaaState.h"
#include "core/hw/gfxip/gfx9/gfx9PerfExperiment.h"
#include "core/hw/gfxip/gfx9/gfx9UniversalCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineStatsQueryPool.h"
#include "core/imported/hsa/AMDHSAKernelDescriptor.h"
#include "core/imported/hsa/amd_hsa_kernel_code.h"
#include "core/settingsLoader.h"
#include "palHsaAbiMetadata.h"
#include "palMath.h"
#include "palIterator.h"
#include "palIntervalTreeImpl.h"
#include "palVectorImpl.h"

#include <float.h>
#include <limits.h>
#include <type_traits>

using namespace Util;
using namespace Pal::Formats;
using namespace Pal::Formats::Gfx9;

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

// Index into the valid user entry lookup per graphics shader stage. Assuming 32 user SGPRs per stage, each stage
// consecutively reserves 32 entries into the lookup.
constexpr uint8 LookupIndexHs = 0 * NumUserDataRegisters;
constexpr uint8 LookupIndexGs = 1 * NumUserDataRegisters;
constexpr uint8 LookupIndexPs = 2 * NumUserDataRegisters;

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
    DI_PT_LINELOOP,         // LineLoop
    DI_PT_POLYGON,          // Polygon
    DI_PT_2D_RECTANGLE,     // TwoDRectList
};

// The DB_RENDER_OVERRIDE fields owned by the graphics pipeline.
constexpr uint32 PipelineDbRenderOverrideMask = DB_RENDER_OVERRIDE__FORCE_SHADER_Z_ORDER_MASK  |
                                                DB_RENDER_OVERRIDE__DISABLE_VIEWPORT_CLAMP_MASK;

// Use an unused HW bit in these tracked registers to indicate that they are dirty and must and must be written.
// PAL will never set this bit in HW.
constexpr uint32 PaScConsRastCntlDirtyBit = uint32(1 << 31);
constexpr uint32 VgtLsHsConfigDirtyBit    = uint32(1 << 31);

// The DB_SHADER_CONTROL fields owned by the graphics pipeline.
constexpr uint32 Gfx11PipelineDbShaderControlMask = ~(Gfx11::DB_SHADER_CONTROL__OVERRIDE_INTRINSIC_RATE_ENABLE_MASK |
                                                      Gfx11::DB_SHADER_CONTROL__OVERRIDE_INTRINSIC_RATE_MASK);

// Enumerates the semaphore values used for synchronizing the ACE and GFX workloads of a ganged submit.
enum class CmdStreamSyncEvent : uint32
{
    GfxSetValue = 0x1, // The DE is expected to set the event to this value, after which the ACE cmd stream starts.
    AceSetValue = 0x2, // The ACE cmd stream upon finishing its workload will set the event to this value.
};

// This is the expected hardware layout of the Streamout Control Buffer.
// It's only expected to be consumed by the hardware.
#pragma pack(push, 1)
struct SwStreamoutBufLayout
{
    uint32 offset0;
    uint32 offset1;
    uint32 offset2;
    uint32 offset3;
    uint32 filledSize0;
    uint32 filledSize1;
    uint32 filledSize2;
    uint32 filledSize3;
};
#pragma pack(pop)

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

#if PAL_ENABLE_PRINTS_ASSERTS
// =====================================================================================================================
// Helper to check whether a given register offset belongs to a user-SGPR
template <Pm4ShaderType ShaderType>
static bool IsRegUserSgpr(
    uint16 regAddr,
    const uint16 (&baseUserDataRegs)[HwShaderStage::Last])
{
    constexpr uint32 NumUserSgprsPerStage = (ShaderType == ShaderCompute) ? NumUserDataRegistersCompute
                                                                          : NumUserDataRegisters;

    bool isUserSgpr = false;
    for (uint32 stage = 0; stage < HwShaderStage::Last; ++stage)
    {
        if (InRange<uint16>(regAddr, baseUserDataRegs[stage], baseUserDataRegs[stage] + NumUserSgprsPerStage))
        {
            isUserSgpr = true;
            break;
        }
    }

    return isUserSgpr;
}
#endif

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
static uint32 StreamOutNumRecords(
    const GpuChipProperties& chipProps,
    uint32                   sizeInBytes,
    uint32                   strideInBytes)
{
    // NOTE: As mentioned in the SC GFXIP interface, it is SC's responsibility to handle stream output
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
    uint32 numRecords = ((UINT_MAX - chipProps.gfx9.maxWavefrontSize) + 1);

    if (chipProps.gfxip.supportsSwStrmout)
    {
        // Software Streamout does not require the above handling and operates much more like a standard SRD.

        // Only byte-addressed buffers is currently supported, which is indicated by passing Stride=1
        PAL_ASSERT(strideInBytes <= 1);
        numRecords = Device::CalcNumRecords(sizeInBytes, strideInBytes);
    }

    return numRecords;
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
    Pal::Pm4::UniversalCmdBuffer(device,
                                 createInfo,
                                 device.BarrierMgr(),
                                 &m_deCmdStream,
                                 &m_ceCmdStream,
                                 nullptr,
                                 device.Settings().blendOptimizationEnable,
                                 device.Settings().gfx11EnableShRegPairOptimization),
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
    m_pSignatureCs(&(device.GetNullCsSignature())),
    m_pSignatureGfx(&(device.GetNullGfxSignature())),
    m_rbplusRegHash(0),
    m_pipelineCtxRegHash(0),
    m_pipelineCfgRegHash(0),
    m_pipelineDynRegsDirty(false),
#if PAL_ENABLE_PRINTS_ASSERTS
    m_pipelineStateValid(false),
#endif
    m_pfnValidateUserDataGfx(nullptr),
    m_pfnValidateUserDataGfxPipelineSwitch(nullptr),
    m_predGpuAddr(0),
    m_workaroundState(&device, createInfo.flags.nested, m_state, m_cachedSettings),
    m_vertexOffsetReg(UserDataNotMapped),
    m_drawIndexReg(UserDataNotMapped),
    m_log2NumSes(Log2(m_device.Parent()->ChipProperties().gfx9.numShaderEngines)),
    m_log2NumRbPerSe(Log2(m_device.Parent()->ChipProperties().gfx9.maxNumRbPerSe)),
    m_enabledPbb(true),
    m_customBinSizeX(0),
    m_customBinSizeY(0),
    m_leakCbColorInfoRtv(0),
    m_validVrsCopies(m_device.GetPlatform()),
    m_activeOcclusionQueryWriteRanges(m_device.GetPlatform()),
    m_deferredPipelineStatsQueries(m_device.GetPlatform()),
    m_gangedCmdStreamSemAddr(0),
    m_semCountAceWaitDe(0),
    m_semCountDeWaitAce(0),
    m_swStreamoutDataAddr(0),
    m_validUserEntryRegPairs{},
    m_validUserEntryRegPairsCs{},
    m_numValidUserEntries(0),
    m_numValidUserEntriesCs(0),
    m_minValidUserEntryLookupValue(1),
    m_minValidUserEntryLookupValueCs(1),
    m_meshPipeStatsGpuAddr(0),
    m_globalInternalTableAddr(0),
    m_ringSizes{}
{
    const auto&                palDevice        = *(m_device.Parent());
    const PalPlatformSettings& platformSettings = m_device.Parent()->GetPlatform()->PlatformSettings();
    const PalSettings&         coreSettings     = m_device.Parent()->Settings();
    const Gfx9PalSettings&     settings         = m_device.Settings();
    const auto*const           pPublicSettings  = m_device.Parent()->GetPublicSettings();
    const GpuChipProperties&   chipProps        = m_device.Parent()->ChipProperties();
    const auto&                curEngineProps   = palDevice.EngineProperties().perEngine[EngineTypeUniversal];

    memset(&m_vbTable,         0, sizeof(m_vbTable));
    memset(&m_spillTable,      0, sizeof(m_spillTable));
    memset(&m_streamOut,       0, sizeof(m_streamOut));
    memset(&m_nggTable,        0, sizeof(m_nggTable));
    memset(&m_state,           0, sizeof(m_state));
    memset(&m_cachedSettings,  0, sizeof(m_cachedSettings));
    memset(&m_drawTimeHwState, 0, sizeof(m_drawTimeHwState));
    memset(&m_nggState,        0, sizeof(m_nggState));

    memset(&m_pipelinePsHash, 0, sizeof(m_pipelinePsHash));
    memset(&m_pipelineState,  0, sizeof(m_pipelineState));

    // Setup default engine support - Universal Cmd Buffer supports Graphics, Compute and CPDMA.
    m_engineSupport = (CmdBufferEngineSupport::Graphics |
                       CmdBufferEngineSupport::Compute  |
                       CmdBufferEngineSupport::CpDma);

    for (uint32 stage = 0; stage < HwShaderStage::Last; ++stage)
    {
        m_baseUserDataReg[stage] = m_device.GetBaseUserDataReg(static_cast<HwShaderStage>(stage));
    }

    // Setup all of our cached settings checks.
    m_cachedSettings.tossPointMode              = static_cast<uint32>(coreSettings.tossPointMode);
    m_cachedSettings.hiDepthDisabled            = !settings.hiDepthEnable;
    m_cachedSettings.hiStencilDisabled          = !settings.hiStencilEnable;
    m_cachedSettings.ignoreCsBorderColorPalette = settings.disableBorderColorPaletteBinds;
    m_cachedSettings.blendOptimizationsEnable   = settings.blendOptimizationEnable;
    m_cachedSettings.outOfOrderPrimsEnable      = static_cast<uint32>(settings.enableOutOfOrderPrimitives);
    m_cachedSettings.padParamCacheSpace         =
            ((pPublicSettings->contextRollOptimizationFlags & PadParamCacheSpace) != 0);
    m_cachedSettings.disableVertGrouping       = settings.disableGeCntlVtxGrouping;

    m_cachedSettings.prefetchIndexBufferForNgg = settings.waEnableIndexBufferPrefetchForNgg;
    m_cachedSettings.waCeDisableIb2            = settings.waCeDisableIb2;
    m_cachedSettings.supportsMall              = m_device.Parent()->MemoryProperties().flags.supportsMall;
    m_cachedSettings.waDisableInstancePacking  = settings.waDisableInstancePacking;
    m_cachedSettings.rbPlusSupported           = chipProps.gfx9.rbPlus;

    m_cachedSettings.waUtcL0InconsistentBigPage = settings.waUtcL0InconsistentBigPage;
    m_cachedSettings.waClampGeCntlVertGrpSize   = settings.waClampGeCntlVertGrpSize;
    m_cachedSettings.ignoreDepthForBinSize      = settings.ignoreDepthForBinSizeIfColorBound;
    m_cachedSettings.pbbDisableBinMode          = settings.disableBinningMode;

    m_cachedSettings.waLogicOpDisablesOverwriteCombiner        = settings.waLogicOpDisablesOverwriteCombiner;
    m_cachedSettings.waColorCacheControllerInvalidEviction     = settings.waColorCacheControllerInvalidEviction;
    m_cachedSettings.waTessIncorrectRelativeIndex              = settings.waTessIncorrectRelativeIndex;
    m_cachedSettings.waVgtFlushNggToLegacy                     = settings.waVgtFlushNggToLegacy;
    m_cachedSettings.waVgtFlushNggToLegacyGs                   = settings.waVgtFlushNggToLegacyGs;
    m_cachedSettings.waIndexBufferZeroSize                     = settings.waIndexBufferZeroSize;
    m_cachedSettings.waLegacyGsCutModeFlush                    = settings.waLegacyGsCutModeFlush;
    m_cachedSettings.waClampQuadDistributionFactor             = settings.waClampQuadDistributionFactor;
    m_cachedSettings.supportsVrs                               = chipProps.gfxip.supportsVrs;
    m_cachedSettings.vrsForceRateFine                          = settings.vrsForceRateFine;
    m_cachedSettings.supportsSwStrmout                         = chipProps.gfxip.supportsSwStrmout;
    m_cachedSettings.waAddPostambleEvent                       = settings.waAddPostambleEvent;
    m_cachedSettings.useLegacyDbZInfo                          = settings.useLegacyDbZInfo;
    m_cachedSettings.waLineStippleReset                        = settings.waLineStippleReset;
    m_cachedSettings.disableRbPlusWithBlending                 = settings.gfx11DisableRbPlusWithBlending;
    m_cachedSettings.waEnableIntrinsicRateEnable               = settings.waEnableIntrinsicRateEnable;
    m_cachedSettings.supportsShPairsPacket                     = settings.gfx11EnableShRegPairOptimization;
    m_cachedSettings.supportsShPairsPacketCs                   = settings.gfx11EnableShRegPairOptimizationCs;
    m_cachedSettings.supportAceOffload                         = chipProps.gfxip.supportAceOffload;
    m_cachedSettings.useExecuteIndirectPacket                  = coreSettings.useExecuteIndirectPacket;
    m_cachedSettings.disablePreamblePipelineStats              = (settings.enablePreamblePipelineStats == false);

    if (IsGfx11(m_gfxIpLevel))
    {
        m_cachedSettings.primGrpSize   = settings.gfx11GeCntlPrimGrpSize;
        m_cachedSettings.geCntlGcrMode = settings.gfx11GeCntlGcrMode;
    }

    m_cachedSettings.optimizeDepthOnlyFmt    = pPublicSettings->optDepthOnlyExportRate;
    PAL_ASSERT(m_cachedSettings.optimizeDepthOnlyFmt ? m_cachedSettings.rbPlusSupported : true);
    m_cachedSettings.has32bPred              = curEngineProps.flags.memory32bPredicationSupport;
    m_cachedSettings.optimizeNullSourceImage = settings.optimizeNullSourceImage &&
        (m_graphicsState.inheritedState.stateFlags.targetViewState == 0);

    m_cachedSettings.waitAfterCbFlush = TestAnyFlagSet(settings.waitOnFlush, WaitAfterCbFlush);
    m_cachedSettings.waitAfterDbFlush = TestAnyFlagSet(settings.waitOnFlush, WaitAfterDbFlush);
    m_cachedSettings.rbHarvesting     = (chipProps.gfx9.numActiveRbs != chipProps.gfx9.numTotalRbs);

    // Here we pre-calculate constants used in gfx10 PBB bin sizing calculations.
    // The logic is based on formulas that account for the number of RBs and Channels on the ASIC.
    // The bin size is choosen from the minimum size for Depth, Color and Fmask.
    // See usage in Gfx10GetDepthBinSize() and Gfx10GetColorBinSize() for further details.
    m_totalNumRbs        = chipProps.gfx9.numActiveRbs;
    uint32 totalNumPipes = Max(m_totalNumRbs, chipProps.gfx9.numSdpInterfaces);

    if (settings.binningBinSizeRbOverride != 0)
    {
        m_totalNumRbs = settings.binningBinSizeRbOverride;
    }

    if (settings.binningBinSizePipesOverride != 0)
    {
        totalNumPipes = settings.binningBinSizePipesOverride;
    }

    constexpr uint32 ZsTagSize  = 64;
    constexpr uint32 ZsNumTags  = 312;
    constexpr uint32 CcTagSize  = 1024;
    constexpr uint32 CcReadTags = 31;
    constexpr uint32 FcTagSize  = 256;
    constexpr uint32 FcReadTags = 44;

    // The logic given to calculate the Depth bin size is:
    //   depthBinArea = ((ZsReadTags * m_totalNumRbs / totalNumPipes) * (ZsTagSize * totalNumPipes)) / cDepth
    // After we precalculate the constant terms, the formula becomes:
    //   depthBinArea = depthBinSizeTagPart / cDepth;
    m_depthBinSizeTagPart   = ((ZsNumTags * m_totalNumRbs / totalNumPipes) * (ZsTagSize * totalNumPipes));

    // The logic given to calculate the Color bin size is:
    //   colorBinArea = ((CcReadTags * m_totalNumRbs / totalNumPipes) * (CcTagSize * totalNumPipes)) / cColor
    // After we precalculate the constant terms, the formula becomes:
    //   colorBinArea = colorBinSizeTagPart / cColor;
    m_colorBinSizeTagPart   = ((CcReadTags * m_totalNumRbs / totalNumPipes) * (CcTagSize * totalNumPipes));

    // The logic given to calculate the Fmask bin size is:
    //   fmaskBinArea =  ((FcReadTags * m_totalNumRbs / totalNumPipes) * (FcTagSize * totalNumPipes)) / cFmask
    // After we precalculate the constant terms, the formula becomes:
    //   fmaskBinArea = fmaskBinSizeTagPart / cFmask;
    m_fmaskBinSizeTagPart   = ((FcReadTags * m_totalNumRbs / totalNumPipes) * (FcTagSize * totalNumPipes));

    m_minBinSizeX = settings.minBatchBinSize.width;
    m_minBinSizeY = settings.minBatchBinSize.height;

    PAL_ASSERT((m_minBinSizeX != 0) && (m_minBinSizeY != 0));
    PAL_ASSERT(IsPowerOfTwo(m_minBinSizeX) && IsPowerOfTwo(m_minBinSizeY));

    if (pPublicSettings->binningMode == DeferredBatchBinCustom)
    {
        // The custom bin size setting is encoded as two uint16's.
        m_customBinSizeX = pPublicSettings->customBatchBinSize >> 16;
        m_customBinSizeY = pPublicSettings->customBatchBinSize & 0xFFFF;

        PAL_ASSERT(IsPowerOfTwo(m_customBinSizeX) && IsPowerOfTwo(m_customBinSizeY));
    }

    m_cachedSettings.issueSqttMarkerEvent = device.Parent()->IssueSqttMarkerEvents();
    m_cachedSettings.describeDrawDispatch = (m_cachedSettings.issueSqttMarkerEvent ||
                                             platformSettings.cmdBufferLoggerConfig.embedDrawDispatchInfo ||
                                             device.Parent()->IssueCrashAnalysisMarkerEvents());

#if PAL_DEVELOPER_BUILD
    m_cachedSettings.enablePm4Instrumentation = platformSettings.pm4InstrumentorEnabled;
#endif

    // Initialize defaults for some of the fields in PA_SC_BINNER_CNTL_0.
    m_pbbCntlRegs.paScBinnerCntl0.u32All                     = 0;
    m_pbbCntlRegs.paScBinnerCntl0.bits.FPOVS_PER_BATCH       = settings.binningFpovsPerBatch;
    m_pbbCntlRegs.paScBinnerCntl0.bits.OPTIMAL_BIN_SELECTION = settings.binningOptimalBinSelection;
    m_pbbCntlRegs.paScBinnerCntl0.bits.DISABLE_START_OF_PRIM = 1;

    // Hardware detects binning transitions when this is set so SW can hardcode it.
    // This has no effect unless the KMD has also set PA_SC_ENHANCE_1.FLUSH_ON_BINNING_TRANSITION=1
    m_pbbCntlRegs.paScBinnerCntl0.bits.FLUSH_ON_BINNING_TRANSITION = 1;

    m_cachedPbbSettings.maxAllocCountNgg       = settings.binningMaxAllocCountNggOnChip;
    m_cachedPbbSettings.maxAllocCountLegacy    = settings.binningMaxAllocCountLegacy;

    if (IsGfx10(palDevice))
    {
        PAL_ASSERT(m_cachedPbbSettings.maxAllocCountLegacy > 0);
        PAL_ASSERT(m_cachedPbbSettings.maxAllocCountNgg    > 0);
        m_cachedPbbSettings.maxAllocCountNgg     -= 1;
        m_cachedPbbSettings.maxAllocCountLegacy  -= 1;
    }
    m_cachedPbbSettings.maxPrimsPerBatch       = (pPublicSettings->binningMaxPrimPerBatch - 1);
    m_cachedPbbSettings.persistentStatesPerBin = (m_persistentStatesPerBin                - 1);

    PAL_ASSERT((IsGfx10(palDevice) ?
                m_cachedPbbSettings.maxAllocCountNgg    == (0xFFFF & (settings.binningMaxAllocCountNggOnChip - 1)) :
                m_cachedPbbSettings.maxAllocCountNgg    == (0xFFFF & settings.binningMaxAllocCountNggOnChip)));
    PAL_ASSERT((IsGfx10(palDevice) ?
                m_cachedPbbSettings.maxAllocCountLegacy == (0xFFFF & (settings.binningMaxAllocCountLegacy - 1)) :
                m_cachedPbbSettings.maxAllocCountLegacy == (0xFFFF & settings.binningMaxAllocCountLegacy)));
    PAL_ASSERT(m_cachedPbbSettings.maxPrimsPerBatch     == (0xFFFF & (pPublicSettings->binningMaxPrimPerBatch - 1)));

    m_pbbCntlRegs.paScBinnerCntl1.u32All                         = 0;
    m_pbbCntlRegs.paScBinnerCntl1.bits.MAX_PRIM_PER_BATCH        = m_cachedPbbSettings.maxPrimsPerBatch;
    m_pbbCntlRegs.paScBinnerCntl0.bits.PERSISTENT_STATES_PER_BIN = m_cachedPbbSettings.persistentStatesPerBin;

    // Initialize to the common value for most pipelines (no conservative rast).
    m_paScConsRastCntl.u32All                         = 0;
    m_paScConsRastCntl.bits.NULL_SQUAD_AA_MASK_ENABLE = 1;

    m_sxPsDownconvert.u32All      = 0;
    m_sxBlendOptEpsilon.u32All    = 0;
    m_sxBlendOptControl.u32All    = 0;
    m_cbRmiGl2CacheControl.u32All = 0;
    m_dbRenderOverride.u32All     = 0;
    m_prevDbRenderOverride.u32All = 0;
    m_paScAaConfigNew.u32All      = 0;
    m_paScAaConfigLast.u32All     = 0;
    m_paSuLineStippleCntl.u32All  = 0;
    m_paScLineStipple.u32All      = 0;
    m_cbColorControl.u32All       = 0;
    m_paClClipCntl.u32All         = 0;
    m_cbTargetMask.u32All         = 0;
    m_cbShaderMask.u32All         = 0;
    m_vgtTfParam.u32All           = 0;
    m_paScLineCntl.u32All         = 0;
    m_paSuScModeCntl.u32All       = InvalidPaSuScModeCntlVal;
    m_depthClampMode              = DepthClampMode::Viewport;
    m_geMultiPrimIbResetEn.u32All = 0;

    if (IsGfx11(m_gfxIpLevel))
    {
        m_geMultiPrimIbResetEn.u32All = Gfx11::GE_MULTI_PRIM_IB_RESET_EN__DISABLE_FOR_AUTO_INDEX_MASK;

        // Recommended defaults for GFX11
        constexpr uint32 Gfx11DefaultPatchFactor         = 128;
        m_tessDistributionFactors.isoDistributionFactor  = Gfx11DefaultPatchFactor;
        m_tessDistributionFactors.triDistributionFactor  = Gfx11DefaultPatchFactor;
        m_tessDistributionFactors.quadDistributionFactor = Gfx11DefaultPatchFactor;

        memset(&(m_validUserEntryRegPairsLookup[0]), 0, sizeof(m_validUserEntryRegPairsLookup));
        memset(&(m_validUserEntryRegPairsLookupCs[0]), 0, sizeof(m_validUserEntryRegPairsLookupCs));
    }

    // Assume PAL ABI compute pipelines by default.
    SetDispatchFunctions(false);
    SwitchDrawFunctions(false, false, false, false);
}

// =====================================================================================================================
UniversalCmdBuffer::~UniversalCmdBuffer()
{
    PAL_SAFE_DELETE(m_pAceCmdStream, m_device.GetPlatform());
}

// =====================================================================================================================
// Initializes Gfx9-specific functionality.
Result UniversalCmdBuffer::Init(
    const CmdBufferInternalCreateInfo& internalInfo)
{
    const Gfx9PalSettings&   settings  = m_device.Settings();
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    m_spillTable.stateCs.sizeInDwords  = chipProps.gfxip.maxUserDataEntries;
    m_spillTable.stateGfx.sizeInDwords = chipProps.gfxip.maxUserDataEntries;
    m_streamOut.state.sizeInDwords = (sizeof(m_streamOut.srd) / sizeof(uint32));
    m_uavExportTable.state.sizeInDwords = (sizeof(m_uavExportTable.srd) / sizeof(uint32));

    if (settings.nggSupported)
    {
        const uint32 nggTableBytes = Pow2Align<uint32>(sizeof(Abi::PrimShaderCullingCb), 256);
        m_nggTable.state.sizeInDwords = NumBytesToNumDwords(nggTableBytes);
    }

    m_vbTable.pSrds              = static_cast<BufferSrd*>(VoidPtrAlign((this + 1), alignof(BufferSrd)));
    m_vbTable.state.sizeInDwords = ((sizeof(BufferSrd) / sizeof(uint32)) * MaxVertexBuffers);

    Result result = Pal::Pm4::UniversalCmdBuffer::Init(internalInfo);

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
// Sets-up function pointers for the Dispatch entrypoint and all variants using template parameters.
template <bool HsaAbi, bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
void UniversalCmdBuffer::SetDispatchFunctions()
{
    m_funcTable.pfnCmdDispatch       = CmdDispatch<HsaAbi, IssueSqttMarkerEvent, DescribeDrawDispatch>;
    m_funcTable.pfnCmdDispatchOffset = CmdDispatchOffset<HsaAbi, IssueSqttMarkerEvent, DescribeDrawDispatch>;

    if (HsaAbi)
    {
        // Note that CmdDispatchIndirect does not support the HSA ABI.
        m_funcTable.pfnCmdDispatchIndirect = nullptr;
    }
    else
    {
        m_funcTable.pfnCmdDispatchIndirect = CmdDispatchIndirect<IssueSqttMarkerEvent, DescribeDrawDispatch>;
    }
}

// =====================================================================================================================
// Sets-up function pointers for the Dispatch entrypoint and all variants.
void UniversalCmdBuffer::SetDispatchFunctions(
    bool hsaAbi)
{
    if (hsaAbi)
    {
        if (m_cachedSettings.issueSqttMarkerEvent)
        {
            if (m_cachedSettings.describeDrawDispatch)
            {
                SetDispatchFunctions<true, true, true>();
            }
            else
            {
                SetDispatchFunctions<true, true, false>();
            }
        }
        else
        {
            if (m_cachedSettings.describeDrawDispatch)
            {
                SetDispatchFunctions<true, false, true>();
            }
            else
            {
                SetDispatchFunctions<true, false, false>();
            }
        }
    }
    else
    {
        if (m_cachedSettings.issueSqttMarkerEvent)
        {
            if (m_cachedSettings.describeDrawDispatch)
            {
                SetDispatchFunctions<false, true, true>();
            }
            else
            {
                SetDispatchFunctions<false, true, false>();
            }
        }
        else
        {
            if (m_cachedSettings.describeDrawDispatch)
            {
                SetDispatchFunctions<false, false, true>();
            }
            else
            {
                SetDispatchFunctions<false, false, false>();
            }
        }
    }
}

// =====================================================================================================================
// Sets up function pointers for Draw-time validation of graphics user-data entries.
template <bool TessEnabled, bool GsEnabled, bool VsEnabled>
void UniversalCmdBuffer::SetUserDataValidationFunctions()
{
    m_pfnValidateUserDataGfx =
        &UniversalCmdBuffer::ValidateGraphicsUserData<false, TessEnabled, GsEnabled, VsEnabled>;
    m_pfnValidateUserDataGfxPipelineSwitch =
        &UniversalCmdBuffer::ValidateGraphicsUserData<true, TessEnabled, GsEnabled, VsEnabled>;
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
void UniversalCmdBuffer::SetShaderRingSize(
    const ShaderRingItemSizes& ringSizes)
{
    for (uint32 ring = 0; ring < static_cast<uint32>(ShaderRingType::NumUniversal); ++ring)
    {
        if (ringSizes.itemSize[ring] > m_ringSizes.itemSize[ring])
        {
            m_ringSizes.itemSize[ring] = ringSizes.itemSize[ring];
        }
    }
}

// =====================================================================================================================
// Resets all of the state tracked by this command buffer
void UniversalCmdBuffer::ResetState()
{
    Pal::Pm4::UniversalCmdBuffer::ResetState();

    // Assume PAL ABI compute pipelines by default.
    SetDispatchFunctions(false);
    SetUserDataValidationFunctions(false, false, false);
    SwitchDrawFunctions(false, false, false, false);

    m_vgtDmaIndexType.u32All = 0;
    m_vgtDmaIndexType.bits.SWAP_MODE  = VGT_DMA_SWAP_NONE;
    m_vgtDmaIndexType.bits.INDEX_TYPE = VgtIndexTypeLookup[0];

    m_leakCbColorInfoRtv   = 0;
    m_pipelineDynRegsDirty = false;

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
    m_vgtDmaIndexType.bits.RDREQ_POLICY = VGT_POLICY_STREAM;

    const uint32 cbDbCachePolicy = m_device.Settings().cbDbCachePolicy;

    m_cbRmiGl2CacheControl.u32All = 0;
    m_cbRmiGl2CacheControl.bits.DCC_RD_POLICY =
        (cbDbCachePolicy & Gfx10CbDbCachePolicyLruDcc) ? CACHE_LRU_RD : CACHE_NOA;
    m_cbRmiGl2CacheControl.bits.COLOR_RD_POLICY =
        (cbDbCachePolicy & Gfx10CbDbCachePolicyLruColor) ? CACHE_LRU_RD : CACHE_NOA;

    if (IsGfx11(m_gfxIpLevel))
    {
        m_cbRmiGl2CacheControl.gfx11.DCC_WR_POLICY =
            (cbDbCachePolicy & Gfx10CbDbCachePolicyLruDcc) ? CACHE_LRU_WR : CACHE_STREAM;
        m_cbRmiGl2CacheControl.gfx11.COLOR_WR_POLICY =
            (cbDbCachePolicy & Gfx10CbDbCachePolicyLruColor) ? CACHE_LRU_WR : CACHE_STREAM;
    }
    else
    {
        m_cbRmiGl2CacheControl.gfx10.CMASK_WR_POLICY =
            (cbDbCachePolicy & Gfx10CbDbCachePolicyLruCmask) ? CACHE_LRU_WR : CACHE_STREAM;
        m_cbRmiGl2CacheControl.gfx10.FMASK_WR_POLICY =
            (cbDbCachePolicy & Gfx10CbDbCachePolicyLruFmask) ? CACHE_LRU_WR : CACHE_STREAM;
        m_cbRmiGl2CacheControl.gfx10.CMASK_RD_POLICY =
            (cbDbCachePolicy & Gfx10CbDbCachePolicyLruCmask) ? CACHE_LRU_RD : CACHE_NOA;
        m_cbRmiGl2CacheControl.gfx10.FMASK_RD_POLICY =
            (cbDbCachePolicy & Gfx10CbDbCachePolicyLruFmask) ? CACHE_LRU_RD : CACHE_NOA;
        m_cbRmiGl2CacheControl.gfx10.DCC_WR_POLICY =
            (cbDbCachePolicy & Gfx10CbDbCachePolicyLruDcc) ? CACHE_LRU_WR : CACHE_STREAM;
        // If any of the bound color targets are using linear swizzle mode (or 256_S or 256_D, but PAL doesn't
        // utilize those), then COLOR_WR_POLICY can not be CACHE_BYPASS.
        m_cbRmiGl2CacheControl.gfx10.COLOR_WR_POLICY =
            (cbDbCachePolicy & Gfx10CbDbCachePolicyLruColor) ? CACHE_LRU_WR : CACHE_STREAM;
    }

    m_spiVsOutConfig.u32All         = 0;
    m_spiPsInControl.u32All         = 0;
    m_geCntl.u32All                 = 0;
    m_dbShaderControl.u32All        = 0;
    m_paScAaConfigNew.u32All        = 0;
    m_paSuLineStippleCntl.u32All    = 0;
    m_paScLineStipple.u32All        = 0;
    m_paSuScModeCntl.u32All         = InvalidPaSuScModeCntlVal;

    {
        Extent2d binSize = {};
        binSize.width  = m_minBinSizeX;
        binSize.height = m_minBinSizeY;
        m_pbbCntlRegs.paScBinnerCntl0.bits.BINNING_MODE = m_cachedSettings.pbbDisableBinMode;
        if (binSize.width != 0)
        {
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_Y        = 0;
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_Y_EXTEND = Device::GetBinSizeEnum(binSize.height);
            if (binSize.width == 16)
            {
                m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_X        = 1;
                m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_X_EXTEND = 0;
            }
            else
            {
                m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_X        = 0;
                m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_X_EXTEND = Device::GetBinSizeEnum(binSize.width);
            }

            if (binSize.height == 16)
            {
                m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_Y        = 1;
                m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_Y_EXTEND = 0;
            }
            else
            {
                m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_Y        = 0;
                m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_Y_EXTEND = Device::GetBinSizeEnum(binSize.height);
            }
        }
    }

    // Set to true to enable validate of PBB at draw time.
    m_enabledPbb = true;

    m_pbbCntlRegs.paScBinnerCntl0.bits.CONTEXT_STATES_PER_BIN = (m_contextStatesPerBin - 1);
    m_cachedSettings.batchBreakOnNewPs                        = m_device.Settings().batchBreakOnNewPixelShader ||
                                                                (m_contextStatesPerBin > 1) ||
                                                                (m_persistentStatesPerBin > 1);
    m_cachedSettings.pbbMoreThanOneCtxState                   = (m_contextStatesPerBin > 1);
    m_cachedPbbSettings.persistentStatesPerBin                = (m_persistentStatesPerBin - 1);

    Extent2d binSize = {};
    binSize.width  = m_minBinSizeX;
    binSize.height = m_minBinSizeY;
    m_pbbCntlRegs.paScBinnerCntl0.bits.BINNING_MODE = m_cachedSettings.pbbDisableBinMode;
    if (binSize.width != 0)
    {
        if (binSize.width == 16)
        {
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_X        = 1;
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_X_EXTEND = 0;
        }
        else
        {
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_X        = 0;
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_X_EXTEND = Device::GetBinSizeEnum(binSize.width);
        }

        if (binSize.height == 16)
        {
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_Y        = 1;
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_Y_EXTEND = 0;
        }
        else
        {
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_Y        = 0;
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_Y_EXTEND = Device::GetBinSizeEnum(binSize.height);
        }
    }

    // Reset the command buffer's HWL state tracking
    m_state.flags.u32All                                  = 0;
    m_state.pLastDumpCeRam                                = nullptr;
    m_state.lastDumpCeRamOrdinal2.u32All                  = 0;
    m_state.lastDumpCeRamOrdinal2.bits.gfx10.increment_ce = 1;
    m_state.minCounterDiff                                = UINT_MAX;

    // Set to an invalid (unaligned) address to indicate that streamout hasn't been set yet, and initialize the SRDs'
    // NUM_RECORDS fields to indicate a zero stream-out stride.
    memset(&m_streamOut.srd[0], 0, sizeof(m_streamOut.srd));
    m_device.SetBaseAddress(&m_streamOut.srd[0], 1);
    for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
    {
        m_device.SetNumRecords(&m_streamOut.srd[i], StreamOutNumRecords(m_device.Parent()->ChipProperties(), 0, 0));
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

    m_vertexOffsetReg     = UserDataNotMapped;
    m_drawIndexReg        = UserDataNotMapped;
    m_nggState.numSamples = 1;

    m_pSignatureCs         = &m_device.GetNullCsSignature();
    m_pSignatureGfx        = &m_device.GetNullGfxSignature();
    m_rbplusRegHash        = 0;
    m_pipelineCtxRegHash   = 0;
    m_pipelineCfgRegHash   = 0;
    m_pipelinePsHash.lower = 0;
    m_pipelinePsHash.upper = 0;
    memset(&m_pipelineState, 0, sizeof(m_pipelineState));

#if PAL_ENABLE_PRINTS_ASSERTS
    m_pipelineStateValid = false;
#endif

    ResetUserDataTable(&m_spillTable.stateCs);
    ResetUserDataTable(&m_spillTable.stateGfx);
    ResetUserDataTable(&m_vbTable.state);
    m_vbTable.watermark = m_vbTable.state.sizeInDwords;
    m_vbTable.modified  = 0;

    m_activeOcclusionQueryWriteRanges.Clear();
    m_deferredPipelineStatsQueries.Clear();
    m_validVrsCopies.Clear();

    m_predGpuAddr = 0;
    m_gangedCmdStreamSemAddr = 0;
    m_semCountAceWaitDe  = 0;
    m_semCountDeWaitAce  = 0;
    m_swStreamoutDataAddr = 0;

    // All user data entries are invalid upon state reset.  No need to increment this if we don't have anything to
    // invalidate.

    // In order to wrap, we'd need to have 2^32 draws or dispatches occur.
    // So we'd like to always handle the wrapping logic in ResetState(), then we wouldn't need to check it at all
    // during dispatch or draw validation.
    // (We can be pretty sure that no command buffer is going to have that many draws or dispatches
    // in a single command buffer -- GPUs are doing a few dozen millions of draws per second,
    // 2^32 is 4 billion per command buffer.)
    if (m_minValidUserEntryLookupValue > 1)
    {
        memset(&m_validUserEntryRegPairsLookup[0], 0, sizeof(m_validUserEntryRegPairsLookup));
        m_minValidUserEntryLookupValue = 1;
        m_numValidUserEntries = 0;
    }
    else
    {
        PAL_ASSERT(m_numValidUserEntries == 0);
    }

    if (m_minValidUserEntryLookupValueCs > 1)
    {
        memset(&m_validUserEntryRegPairsLookupCs[0], 0, sizeof(m_validUserEntryRegPairsLookupCs));
        m_minValidUserEntryLookupValueCs = 1;
        m_numValidUserEntriesCs = 0;
    }
    else
    {
        PAL_ASSERT(m_numValidUserEntriesCs == 0);
    }

    m_meshPipeStatsGpuAddr = 0;
    m_globalInternalTableAddr = 0;

    memset(const_cast<ShaderRingItemSizes*>(&m_ringSizes), 0, sizeof(m_ringSizes));
}

// =====================================================================================================================
// Binds a graphics or compute pipeline to this command buffer.
void UniversalCmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    if (params.pipelineBindPoint == PipelineBindPoint::Graphics)
    {
        constexpr uint32 DwordsPerSrd = (sizeof(BufferSrd) / sizeof(uint32));

        auto*const pNewPipeline = static_cast<const GraphicsPipeline*>(params.pPipeline);
        auto*const pOldPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

        const bool disableFiltering =
            false;

        if (disableFiltering || (pNewPipeline != pOldPipeline))
        {
            const bool isNgg       = (pNewPipeline != nullptr) && pNewPipeline->IsNgg();
            const bool tessEnabled = (pNewPipeline != nullptr) && pNewPipeline->IsTessEnabled();
            const bool gsEnabled   = (pNewPipeline != nullptr) && pNewPipeline->IsGsEnabled();
            const bool meshEnabled = (pNewPipeline != nullptr) && pNewPipeline->HasMeshShader();
            const bool taskEnabled = (pNewPipeline != nullptr) && pNewPipeline->HasTaskShader();

            SetUserDataValidationFunctions(tessEnabled, gsEnabled, isNgg);

            const bool newUsesViewInstancing         = (pNewPipeline != nullptr) && pNewPipeline->UsesViewInstancing();
            const bool oldUsesViewInstancing         = (pOldPipeline != nullptr) && pOldPipeline->UsesViewInstancing();
            const bool newUsesUavExport              = (pNewPipeline != nullptr) && pNewPipeline->UsesUavExport();
            const bool oldUsesUavExport              = (pOldPipeline != nullptr) && pOldPipeline->UsesUavExport();
            const bool newNeedsUavExportFlush        = (pNewPipeline != nullptr) && pNewPipeline->NeedsUavExportFlush();
            const bool oldNeedsUavExportFlush        = (pOldPipeline != nullptr) && pOldPipeline->NeedsUavExportFlush();
            const GsFastLaunchMode oldFastLaunchMode = static_cast<GsFastLaunchMode>(m_state.flags.fastLaunchMode);
            const GsFastLaunchMode newFastLaunchMode = (pNewPipeline != nullptr) ? pNewPipeline->FastLaunchMode()
                                                                                 : GsFastLaunchMode::Disabled;

            if (disableFiltering ||
                (static_cast<uint32>(meshEnabled) != m_state.flags.meshShaderEnabled))
            {
                // When mesh shader is either being enabled or being disabled, we need to re-write VGT_PRIMITIVE_TYPE:
                // - Enabling mesh shader requires using the point-list VGT topology;
                // - Disabling mesh shader requires using whatever topology the client gave us.
                const PrimitiveTopology topology = (((newFastLaunchMode == GsFastLaunchMode::VertInLane) && meshEnabled) ?
                                                   PrimitiveTopology::PointList :
                                                   m_graphicsState.inputAssemblyState.topology);

                const uint32 idx = static_cast<uint32>(topology);
                PAL_ASSERT(idx < ArrayLen(TopologyToPrimTypeTable));

                regVGT_PRIMITIVE_TYPE vgtPrimitiveType = { };
                vgtPrimitiveType.bits.PRIM_TYPE = TopologyToPrimTypeTable[idx];

                uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
                pDeCmdSpace = m_deCmdStream.WriteSetOneConfigReg(mmVGT_PRIMITIVE_TYPE,
                                                                 vgtPrimitiveType.u32All,
                                                                 pDeCmdSpace);
                m_deCmdStream.CommitCommands(pDeCmdSpace);
                m_state.flags.meshShaderEnabled = meshEnabled;
                m_state.flags.fastLaunchMode    = static_cast<uint32>(newFastLaunchMode);
            }

            m_state.flags.taskShaderEnabled = taskEnabled;

            if (taskEnabled)
            {
                EnableImplicitGangedSubQueueCount(1);
                ReportHybridPipelineBind();
            }

            bool requiresMeshPipeStatsBuf = false;

            // On Navi2x, we emulate the pipeline stats implementation within the Mesh/Task shaders with unconditional
            // buffer_atomics. So long as a Mesh/Task shader is bound, PAL will need to provide a valid 6 DWORDs buffer
            // regardless of whether or not pipeline stats queries are active.
            if (taskEnabled)
            {
                const auto*const pHybridPipeline = static_cast<const HybridGraphicsPipeline*>(pNewPipeline);
                requiresMeshPipeStatsBuf |=
                    (pHybridPipeline->GetTaskSignature().taskPipeStatsBufRegAddr != UserDataNotMapped);
            }

            requiresMeshPipeStatsBuf |=
                meshEnabled && (pNewPipeline->Signature().meshPipeStatsBufRegAddr != UserDataNotMapped);

            if (requiresMeshPipeStatsBuf && (m_meshPipeStatsGpuAddr == 0))
            {
                // Need 6 DWORDs for MsInvocations, MsPrimitives, TsInvocations.
                constexpr uint32 SizeQuerySlotInDwords = sizeof(PipelineStatsResetMemValue64) / sizeof(uint32);
                constexpr uint32 SizeInDwords          = SizeQuerySlotInDwords * PipelineStatsNumMeshCounters;
                m_meshPipeStatsGpuAddr                 = AllocateGpuScratchMem(SizeInDwords, SizeQuerySlotInDwords);

                WriteDataInfo writeData = {};
                writeData.engineType    = EngineTypeUniversal;
                writeData.dstAddr       = m_meshPipeStatsGpuAddr;
                writeData.engineSel     = engine_sel__pfp_write_data__prefetch_parser;
                writeData.dstSel        = dst_sel__pfp_write_data__memory;
                writeData.predicate     = PacketPredicate();

                const uint32 pData[SizeInDwords] = {};

                uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
                pDeCmdSpace += CmdUtil::BuildWriteData(writeData, SizeInDwords, pData, pDeCmdSpace);
                m_deCmdStream.CommitCommands(pDeCmdSpace);
            }

            const bool oldHasTaskShader = (pOldPipeline != nullptr) && pOldPipeline->HasTaskShader();
            const bool changeMsFunction = (newFastLaunchMode != GsFastLaunchMode::Disabled) &&
                                          (newFastLaunchMode != oldFastLaunchMode);

            if (disableFiltering ||
                (oldNeedsUavExportFlush != newNeedsUavExportFlush) ||
                (oldUsesViewInstancing  != newUsesViewInstancing)  ||
                (meshEnabled && changeMsFunction)                  ||
                (oldHasTaskShader != taskEnabled))
            {
                SwitchDrawFunctions(newNeedsUavExportFlush,
                                    newUsesViewInstancing,
                                    (newFastLaunchMode == GsFastLaunchMode::PrimInLane),
                                    taskEnabled);
            }

            const uint32 vbTableDwords =
                ((pNewPipeline == nullptr) ? 0 : pNewPipeline->VertexBufferCount() * DwordsPerSrd);
            PAL_DEBUG_BUILD_ONLY_ASSERT(vbTableDwords <= m_vbTable.state.sizeInDwords);

            if (disableFiltering ||
                (vbTableDwords > m_vbTable.watermark))
            {
                // If the current high watermark is increasing, we need to mark the contents as dirty because data which
                // was previously uploaded to CE RAM wouldn't have been dumped to GPU memory before the previous Draw.
                m_vbTable.state.dirty = 1;
            }

            m_vbTable.watermark = vbTableDwords;

            if (newUsesUavExport)
            {
                const uint32 maxTargets = static_cast<const GraphicsPipeline*>(params.pPipeline)->NumColorTargets();
                m_uavExportTable.maxColorTargets = maxTargets;
                m_uavExportTable.tableSizeDwords = NumBytesToNumDwords(maxTargets * sizeof(ImageSrd));

                if (disableFiltering ||
                    (oldUsesUavExport == false))
                {
                    // Invalidate color caches so upcoming uav exports don't overlap previous normal exports
                    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
                    pDeCmdSpace = WriteWaitEop(HwPipePostPrefetch, false, SyncGlxNone, SyncCbWbInv, pDeCmdSpace);
                    m_deCmdStream.CommitCommands(pDeCmdSpace);
                }
            }

            // Pipeline owns COVERAGE_TO_SHADER_SELECT
            m_paScAaConfigNew.bits.COVERAGE_TO_SHADER_SELECT =
                (pNewPipeline == nullptr) ? 0 : pNewPipeline->PaScAaConfig().bits.COVERAGE_TO_SHADER_SELECT;

            if ((m_cachedSettings.optimizeDepthOnlyFmt != 0) && (IsNested() == false))
            {
                const bool oldPipeIsCbDisable = (pOldPipeline != nullptr) ?
                                                (pOldPipeline->CbColorControl().bits.MODE == CB_DISABLE) : false;
                const bool newPipeIsCbDisable = (pNewPipeline != nullptr) ?
                                                (pNewPipeline->CbColorControl().bits.MODE == CB_DISABLE) : false;
                if (oldPipeIsCbDisable != newPipeIsCbDisable)
                {
                    // Dirty slot0 as we may want to override it's format at draw-time.
                    m_state.flags.cbColorInfoDirtyRtv |= 1;
                }
            }
        }

        if (pNewPipeline != nullptr)
        {
            regVGT_TF_PARAM        vgtTfParam       = pNewPipeline->VgtTfParam();
            regPA_CL_CLIP_CNTL     paClClipCntl     = pNewPipeline->PaClClipCntl();
            regPA_SC_LINE_CNTL     paScLineCntl     = pNewPipeline->PaScLineCntl();
            regCB_SHADER_MASK      cbShaderMask     = pNewPipeline->CbShaderMask();
            regCB_TARGET_MASK      cbTargetMask     = pNewPipeline->CbTargetMask();
            regCB_COLOR_CONTROL    cbColorControl   = pNewPipeline->CbColorControl();
            regDB_SHADER_CONTROL   dbShaderControl  = pNewPipeline->DbShaderControl();
            regDB_RENDER_OVERRIDE  dbRenderOverride = m_dbRenderOverride;
            BitfieldUpdateSubfield(
                &(dbRenderOverride.u32All), pNewPipeline->DbRenderOverride().u32All, PipelineDbRenderOverrideMask);

            if (IsGfx11(m_gfxIpLevel))
            {
                dbShaderControl = m_dbShaderControl;

                uint32 gfx11PipelineDbShaderControlMask = Gfx11PipelineDbShaderControlMask;
                if (pNewPipeline->DbShaderControl().bits.PRIMITIVE_ORDERED_PIXEL_SHADER == 1)
                {
                    // In POPS mode, OVERRIDE_INTRINSIC_RATE_ENABLE and OVERRIDE_INTRINSIC_RATE should be updated.
                    gfx11PipelineDbShaderControlMask |= (Gfx11::DB_SHADER_CONTROL__OVERRIDE_INTRINSIC_RATE_ENABLE_MASK |
                                                         Gfx11::DB_SHADER_CONTROL__OVERRIDE_INTRINSIC_RATE_MASK);
                }

                BitfieldUpdateSubfield(&(dbShaderControl.u32All),
                                       pNewPipeline->DbShaderControl().u32All,
                                       gfx11PipelineDbShaderControlMask);
            }

            // If RB+ is enabled, we must update the PM4 image of RB+ register state with the new pipelines' values.
            // This should be done here instead of inside SwitchGraphicsPipeline() because RPM sometimes overrides
            //  these values for certain blit operations.
            if (m_cachedSettings.rbPlusSupported != 0)
            {
                pNewPipeline->GetRbPlusRegisters(false,
                                                 &m_sxPsDownconvert,
                                                 &m_sxBlendOptEpsilon,
                                                 &m_sxBlendOptControl);
            }

            m_depthClampMode = pNewPipeline->GetDepthClampMode();

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 842
            const DynamicGraphicsState dynamicState = params.graphics.dynamicState;
#else
            const DynamicGraphicsState dynamicState = params.gfxDynState;
#endif

            // Update context registers according dynamic states
            if (dynamicState.enable.u32All != 0)
            {
                if (dynamicState.enable.switchWinding)
                {
                    if (dynamicState.switchWinding)
                    {
                        if (pNewPipeline->VgtTfParam().bits.TOPOLOGY == OUTPUT_TRIANGLE_CW)
                        {
                            vgtTfParam.bits.TOPOLOGY = OUTPUT_TRIANGLE_CCW;
                        }
                        else if (pNewPipeline->VgtTfParam().bits.TOPOLOGY == OUTPUT_TRIANGLE_CCW)
                        {
                            vgtTfParam.bits.TOPOLOGY = OUTPUT_TRIANGLE_CW;
                        }
                    }
                }

                if (dynamicState.enable.logicOp)
                {
                    cbColorControl.bits.ROP3 = Rop3(dynamicState.logicOp);
                }

                if (dynamicState.enable.rasterizerDiscardEnable)
                {
                    paClClipCntl.bits.DX_RASTERIZATION_KILL = dynamicState.rasterizerDiscardEnable;
                }

                if (dynamicState.enable.depthClipMode)
                {
                    paClClipCntl.bits.ZCLIP_NEAR_DISABLE = dynamicState.depthClipNearEnable ? 0 : 1;
                    paClClipCntl.bits.ZCLIP_FAR_DISABLE  = dynamicState.depthClipFarEnable ? 0 : 1;
                }

                if (dynamicState.enable.depthRange)
                {
                    paClClipCntl.bits.DX_CLIP_SPACE_DEF = (dynamicState.depthRange == DepthRange::ZeroToOne);
                }

                if (dynamicState.enable.perpLineEndCapsEnable)
                {
                    paScLineCntl.bits.PERPENDICULAR_ENDCAP_ENA = dynamicState.perpLineEndCapsEnable;
                }

                if (dynamicState.enable.colorWriteMask)
                {
                    cbTargetMask.u32All = (pNewPipeline->CbTargetMask().u32All & dynamicState.colorWriteMask);
                }

                if (dynamicState.enable.alphaToCoverageEnable)
                {
                    dbShaderControl.bits.ALPHA_TO_MASK_DISABLE = dynamicState.alphaToCoverageEnable ? 0 : 1;
                }

                if (dynamicState.enable.depthClampMode)
                {
                    // For internal RPM pipelines, we want to always disable depth clamp based on depthClampMode
                    // without honor setting of depthClampBasedOnZExport.
                    if (m_device.Parent()->GetPublicSettings()->depthClampBasedOnZExport &&
                        (m_gfxCmdBufStateFlags.isGfxStatePushed == 0)) // Indicates binding a non-RPM pipeline
                    {
                        dbRenderOverride.bits.DISABLE_VIEWPORT_CLAMP =
                            ((dynamicState.depthClampMode == DepthClampMode::_None) &&
                             (pNewPipeline->DbShaderControl().bits.Z_EXPORT_ENABLE != 0));
                    }
                    else
                    {
                        dbRenderOverride.bits.DISABLE_VIEWPORT_CLAMP =
                            (dynamicState.depthClampMode == DepthClampMode::_None);
                    }

                    m_depthClampMode = dynamicState.depthClampMode;
                }

                if (dynamicState.enable.dualSourceBlendEnable)
                {
                    if (m_cachedSettings.rbPlusSupported != 0)
                    {
                        cbColorControl.bits.DISABLE_DUAL_QUAD = dynamicState.dualSourceBlendEnable ? 1 : 0;
                        pNewPipeline->GetRbPlusRegisters(dynamicState.dualSourceBlendEnable,
                                                         &m_sxPsDownconvert,
                                                         &m_sxBlendOptEpsilon,
                                                         &m_sxBlendOptControl);
                    }
                }

                if (dynamicState.enable.vertexBufferCount)
                {
                    const uint32 vbTableDwords = dynamicState.vertexBufferCount * DwordsPerSrd;
                    PAL_ASSERT(vbTableDwords <= m_vbTable.state.sizeInDwords);

                    if (vbTableDwords > m_vbTable.watermark)
                    {
                        m_vbTable.state.dirty = 1;
                    }

                    m_vbTable.watermark = vbTableDwords;
                }
            }

            // Update pipeline dynamic state dirty flags
            if ((vgtTfParam.u32All       != m_vgtTfParam.u32All)      ||
                (cbColorControl.u32All   != m_cbColorControl.u32All)  ||
                (paClClipCntl.u32All     != m_paClClipCntl.u32All)    ||
                (paScLineCntl.u32All     != m_paScLineCntl.u32All)    ||
                (cbTargetMask.u32All     != m_cbTargetMask.u32All)    ||
                (cbShaderMask.u32All     != m_cbShaderMask.u32All)    ||
                (dbShaderControl.u32All  != m_dbShaderControl.u32All) ||
                (dbRenderOverride.u32All != m_dbRenderOverride.u32All))
            {
                if (cbTargetMask.u32All != m_cbTargetMask.u32All)
                {
                    m_state.flags.cbTargetMaskChanged = 1;
                }
                m_vgtTfParam       = vgtTfParam;
                m_cbColorControl   = cbColorControl;
                m_paClClipCntl     = paClClipCntl;
                m_paScLineCntl     = paScLineCntl;
                m_cbTargetMask     = cbTargetMask;
                m_cbShaderMask     = cbShaderMask;
                m_dbShaderControl  = dbShaderControl;
                m_dbRenderOverride = dbRenderOverride;

                m_pipelineDynRegsDirty = true;
            }

            SetShaderRingSize(pNewPipeline->GetShaderRingSize());
        }
    }
    else
    {
        auto* const pNewPipeline = static_cast<const ComputePipeline*>(params.pPipeline);
        auto* const pOldPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);

        const bool newUsesHsaAbi = (pNewPipeline != nullptr) && (pNewPipeline->GetInfo().flags.hsaAbi == 1u);
        const bool oldUsesHsaAbi = (pOldPipeline != nullptr) && (pOldPipeline->GetInfo().flags.hsaAbi == 1u);

        if (oldUsesHsaAbi != newUsesHsaAbi)
        {
            // The HSA abi can clobber USER_DATA_0, which holds the global internal table address for PAL ABI,
            // so we must save the address to memory before switching to an HSA ABI
            // or restore it when switching back to PAL ABI
            if (newUsesHsaAbi && (m_globalInternalTableAddr == 0))
            {
                m_globalInternalTableAddr = AllocateGpuScratchMem(1, 1);
                m_device.RsrcProcMgr().EchoGlobalInternalTableAddr(this, m_globalInternalTableAddr);
            }
            else if (newUsesHsaAbi == false)
            {
                uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
                pDeCmdSpace += m_cmdUtil.BuildLoadShRegsIndex(index__pfp_load_sh_reg_index__direct_addr,
                                                              data_format__pfp_load_sh_reg_index__offset_and_size,
                                                              m_globalInternalTableAddr,
                                                              mmCOMPUTE_USER_DATA_0,
                                                              1,
                                                              Pm4ShaderType::ShaderCompute,
                                                              pDeCmdSpace);
                m_deCmdStream.CommitCommands(pDeCmdSpace);
            }

            SetDispatchFunctions(newUsesHsaAbi);
        }

        if (pNewPipeline != nullptr)
        {
            m_ringSizes.itemSize[static_cast<uint32>(ShaderRingType::ComputeScratch)] =
                Max(m_ringSizes.itemSize[static_cast<uint32>(ShaderRingType::ComputeScratch)],
                    pNewPipeline->GetRingSizeComputeScratch());
        }
    }

     Pal::Pm4::UniversalCmdBuffer::CmdBindPipeline(params);
}

// =====================================================================================================================
// Updates the graphics state with a new pipeline and performs any extra work due to the pipeline switch.
uint32* UniversalCmdBuffer::SwitchGraphicsPipeline(
    const GraphicsPipelineSignature* pPrevSignature,
    const GraphicsPipeline*          pCurrPipeline,
    uint32*                          pDeCmdSpace)
{
    PAL_ASSERT(pCurrPipeline != nullptr);

    const auto& cmdUtil             = m_device.CmdUtil();
    const bool  wasPrevPipelineNull = (pPrevSignature == &m_device.GetNullGfxSignature());
    const bool  wasPrevPipelineNgg  = m_pipelineState.flags.isNgg
                                      ;
    const bool  isNgg               = pCurrPipeline->IsNgg();
    const bool  tessEnabled         = pCurrPipeline->IsTessEnabled();
    const bool  gsEnabled           = pCurrPipeline->IsGsEnabled();
    const bool  disableFiltering    = wasPrevPipelineNull
                                      ;

    const uint32 ctxRegHash = pCurrPipeline->GetContextRegHash();
    const bool ctxRegDirty = disableFiltering || (m_pipelineCtxRegHash != ctxRegHash);
    if (ctxRegDirty || m_pipelineDynRegsDirty)
    {
        if (ctxRegDirty)
        {
            pDeCmdSpace = pCurrPipeline->WriteContextCommands(&m_deCmdStream, pDeCmdSpace);
        }

        if (disableFiltering || m_pipelineDynRegsDirty)
        {
            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(
                mmCB_COLOR_CONTROL, m_cbColorControl.u32All, pDeCmdSpace);

            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(
                mmPA_CL_CLIP_CNTL, m_paClClipCntl.u32All, pDeCmdSpace);

            static_assert(CheckSequentialRegs({
                {mmCB_TARGET_MASK, offsetof(UniversalCmdBuffer, m_cbTargetMask)},
                {mmCB_SHADER_MASK, offsetof(UniversalCmdBuffer, m_cbShaderMask)},
            }), "Regs have moved!");
            pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmCB_TARGET_MASK,
                                                               mmCB_SHADER_MASK,
                                                               &m_cbTargetMask.u32All,
                                                               pDeCmdSpace);

            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(
                mmVGT_TF_PARAM, m_vgtTfParam.u32All, pDeCmdSpace);

            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(
                mmPA_SC_LINE_CNTL, m_paScLineCntl.u32All, pDeCmdSpace);

            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(
                mmDB_SHADER_CONTROL, m_dbShaderControl.u32All, pDeCmdSpace);

            pDeCmdSpace = ValidateDbRenderOverride(pDeCmdSpace);
        }

        m_pipelineCtxRegHash = ctxRegHash;
    }

    const uint32 cfgRegHash = pCurrPipeline->GetConfigRegHash();
    if (disableFiltering || (m_pipelineCfgRegHash != cfgRegHash))
    {
        pDeCmdSpace = pCurrPipeline->WriteConfigCommands(&m_deCmdStream, pDeCmdSpace);
        m_pipelineCfgRegHash = cfgRegHash;
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 842
    const DynamicGraphicsState dynamicState = m_graphicsState.dynamicGraphicsInfo.dynamicState;
#else
    const DynamicGraphicsState dynamicState = m_graphicsState.dynamicState;
#endif

    const bool dynamicDualSourceBlendEnabled = dynamicState.dualSourceBlendEnable &&
                                               dynamicState.enable.dualSourceBlendEnable;
    if ((m_cachedSettings.rbPlusSupported != 0) &&
        (disableFiltering || (m_rbplusRegHash != pCurrPipeline->GetRbplusRegHash(dynamicDualSourceBlendEnabled))))
    {
        // m_sxPsDownconvert, m_sxBlendOptEpsilon and m_sxBlendOptControl have been updated in cmdBindPipeline.
        pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmSX_PS_DOWNCONVERT,
                                                           mmSX_BLEND_OPT_CONTROL,
                                                           &m_sxPsDownconvert,
                                                           pDeCmdSpace);
        m_rbplusRegHash = pCurrPipeline->GetRbplusRegHash(dynamicDualSourceBlendEnabled);
    }

    bool breakBatch = ((m_cachedSettings.pbbMoreThanOneCtxState) && (m_state.flags.cbTargetMaskChanged));

    if ((m_cachedSettings.batchBreakOnNewPs) && (breakBatch == false)
        )
    {
        const ShaderHash& psHash = pCurrPipeline->GetInfo().shader[static_cast<uint32>(ShaderType::Pixel)].hash;
        if (disableFiltering || (ShaderHashesEqual(m_pipelinePsHash, psHash) == false))
        {
            m_pipelinePsHash = psHash;
            breakBatch = true;
        }
    }

    if (breakBatch)
    {
        pDeCmdSpace += cmdUtil.BuildNonSampleEventWrite(BREAK_BATCH, EngineTypeUniversal, pDeCmdSpace);
    }

    // Get new pipeline state VS/PS registers
    regSPI_VS_OUT_CONFIG spiVsOutConfig = pCurrPipeline->SpiVsOutConfig();
    regSPI_PS_IN_CONTROL spiPsInControl = pCurrPipeline->SpiPsInControl();

    // To reduce context rolls due to pipeline state switches the command buffer tracks VS export count and
    // the PS interpolant count and only sets these registers when the maximum value increases. This heuristic
    // pads the actual parameter cache space required for VS/PS to avoid context rolls.
    if (m_cachedSettings.padParamCacheSpace)
    {
        if (IsGfx10(m_gfxIpLevel))
        {
            spiVsOutConfig.bits.VS_EXPORT_COUNT =
                Max(m_spiVsOutConfig.bits.VS_EXPORT_COUNT, spiVsOutConfig.bits.VS_EXPORT_COUNT);
        }

        spiPsInControl.bits.NUM_INTERP = Max(m_spiPsInControl.bits.NUM_INTERP, spiPsInControl.bits.NUM_INTERP);

        // On Gfx11, padding PS_IN > VS_OUT+1 triggers a hazard.
        //
        // Long-term plan is to perform max-padding just like we did in Gfx10, but for that we need to also disable
        // wave reuse.
        //
        // The current strategy pads PS_IN up to VS_OUT+1, which avoids the hazard. This results in more context
        // rolls than we would have with the desired/unconstrained max-padding, but it is still effective in
        // reducing the rolls.
        if (IsGfx11(m_gfxIpLevel))
        {
            spiPsInControl.bits.NUM_INTERP =
                Min(spiPsInControl.bits.NUM_INTERP, spiVsOutConfig.bits.VS_EXPORT_COUNT + 1u);
        }
    }

    // Write VS_OUT_CONFIG if the register changed or this is the first pipeline switch
    if (disableFiltering || (m_spiVsOutConfig.u32All != spiVsOutConfig.u32All))
    {
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmSPI_VS_OUT_CONFIG, spiVsOutConfig.u32All, pDeCmdSpace);
        m_spiVsOutConfig = spiVsOutConfig;
    }

    // Write PS_IN_CONTROL if the register changed or this is the first pipeline switch
    if (disableFiltering || (m_spiPsInControl.u32All != spiPsInControl.u32All))
    {
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmSPI_PS_IN_CONTROL, spiPsInControl.u32All, pDeCmdSpace);
        m_spiPsInControl = spiPsInControl;
    }

    UpdateViewportScissorDirty(pCurrPipeline->UsesMultipleViewports(), m_depthClampMode);

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
        // We need to update the primitive shader constant buffer with this new pipeline if any value changes.
        bool dirty = UpdateNggPrimCb(pCurrPipeline, &m_state.primShaderCullingCb);

        // We need to update the primitive shader constant buffer with this new pipeline if previous pipeline is
        // null or culling data register address changes.
        dirty |= (wasPrevPipelineNull || (pPrevSignature->nggCullingDataAddr != m_pSignatureGfx->nggCullingDataAddr));

        m_nggState.flags.dirty |= dirty;

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

    if ((m_pSignatureGfx->streamoutCntlBufRegAddr != UserDataNotMapped) &&
        (pPrevSignature->streamoutCntlBufRegAddr  != m_pSignatureGfx->streamoutCntlBufRegAddr))
    {
        // SwStreamoutDataAddr() may initialize the buffer, so we need to allow it it be outside of a Reserve/Commit
        // pair.
        m_deCmdStream.CommitCommands(pDeCmdSpace);
        SwStreamoutDataAddr();
        pDeCmdSpace = m_deCmdStream.ReserveCommands();
    }

    if (isNgg == false)
    {
        const bool oldPipelineUnknown = (wasPrevPipelineNull && IsNested())
            ;

        pDeCmdSpace = m_workaroundState.SwitchToLegacyPipeline(m_pipelineState.flags.usesGs,
                                                               wasPrevPipelineNgg,
                                                               m_pipelineState.flags.gsCutMode,
                                                               oldPipelineUnknown,
                                                               pCurrPipeline,
                                                               pDeCmdSpace);
    }

    // Save the set of pipeline flags for the next pipeline transition.  This should come last because the previous
    // pipelines' values are used earlier in the function.
    m_pipelineState.flags.isNgg     = isNgg;
    m_pipelineState.flags.usesTess  = tessEnabled;
    m_pipelineState.flags.usesGs    = gsEnabled;
    m_pipelineState.flags.gsCutMode = pCurrPipeline->VgtGsMode().bits.CUT_MODE;

    m_state.flags.cbTargetMaskChanged = false;

    return pDeCmdSpace;
}

// =====================================================================================================================
// Updates the dirty bits for viewport, scissor, and NGG on pipeline change.
// Also update the tracked state of multi-viewport and depth clamp
void UniversalCmdBuffer::UpdateViewportScissorDirty(
    bool            usesMultiViewports,
    DepthClampMode  depthClampMode)
{
    const bool mvDirty                  = (usesMultiViewports != (m_graphicsState.enableMultiViewport != 0));
    const bool depthClampDirty          =
        (depthClampMode != static_cast<DepthClampMode>(m_graphicsState.depthClampMode));
    if (mvDirty || depthClampDirty)
    {
        // If the previously bound pipeline differed in its use of multiple viewports we will need to rewrite the
        // viewport and scissor state on draw.
        if (m_graphicsState.viewportState.count != 0)
        {
            // If viewport is never set, no need to rewrite viewport, this happens in D3D12 nested command list.
            m_graphicsState.dirtyFlags.viewports    |=
                mvDirty ||
                (depthClampDirty && (depthClampMode != DepthClampMode::_None));
            m_nggState.flags.dirty                                 |= mvDirty;
        }

        if (m_graphicsState.scissorRectState.count != 0)
        {
            // If scissor is never set, no need to rewrite scissor, this happens in D3D12 nested command list.
            m_graphicsState.dirtyFlags.scissorRects |= mvDirty;
        }

        m_graphicsState.enableMultiViewport = usesMultiViewports;
        m_graphicsState.depthClampMode      = static_cast<uint32>(depthClampMode);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetMsaaQuadSamplePattern(
    uint32                       numSamplesPerPixel,
    const MsaaQuadSamplePattern& quadSamplePattern)
{
    PAL_ASSERT((numSamplesPerPixel > 0) && (numSamplesPerPixel <= MaxMsaaRasterizerSamples));

    m_graphicsState.quadSamplePatternState = quadSamplePattern;
    m_graphicsState.numSamplesPerPixel     = numSamplesPerPixel;

    const MsaaQuadSamplePattern& defaultSamplePattern = GfxDevice::DefaultSamplePattern[Log2(numSamplesPerPixel)];
    m_graphicsState.useCustomSamplePattern =
        (memcmp(&quadSamplePattern, &defaultSamplePattern, sizeof(MsaaQuadSamplePattern)) != 0);

    m_graphicsState.dirtyFlags.quadSamplePatternState = 1;
    m_nggState.flags.dirty                            = 1;

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
    m_graphicsState.viewportState.depthRange = params.depthRange;

    memcpy(&m_graphicsState.viewportState.viewports[0],     &params.viewports[0],     viewportSize);
    memcpy(&m_graphicsState.viewportState.horzDiscardRatio, &params.horzDiscardRatio, GuardbandSize);

    m_graphicsState.dirtyFlags.viewports = 1;
    m_nggState.flags.dirty               = 1;

    // Also set scissor dirty flag here since we need cross-validation to handle the case of scissor regions
    // being greater than the viewport regions.
    m_graphicsState.dirtyFlags.scissorRects = 1;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetScissorRects(
    const ScissorRectParams& params)
{
    const size_t scissorSize = (sizeof(params.scissors[0]) * params.count);

    m_graphicsState.scissorRectState.count = params.count;
    memcpy(&m_graphicsState.scissorRectState.scissors[0], &params.scissors[0], scissorSize);

    m_graphicsState.dirtyFlags.scissorRects = 1;
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
    Pal::Pm4::UniversalCmdBuffer::CmdBindIndexData(gpuAddr, indexCount, indexType);
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

        // MSAA State owns MSAA_EXPOSED_SAMPLES and AA_MASK_CENTROID_DTMN
        m_paScAaConfigNew.u32All = ((m_paScAaConfigNew.u32All         & (~MsaaState::PcScAaConfigMask)) |
                                    (pNewState->PaScAaConfig().u32All &   MsaaState::PcScAaConfigMask));

        // NGG state updates
        m_nggState.numSamples = pNewState->NumSamples();
        m_state.primShaderCullingCb.enableConservativeRasterization = pNewState->ConservativeRasterizationEnabled();
    }
    else
    {
        m_paScAaConfigNew.u32All = (m_paScAaConfigNew.u32All & (~MsaaState::PcScAaConfigMask));

        // NGG state updates
        m_nggState.numSamples                                       = 1;
        m_state.primShaderCullingCb.enableConservativeRasterization = 0;
    }

    m_graphicsState.pMsaaState           = pNewState;
    m_graphicsState.dirtyFlags.msaaState = 1;
    m_nggState.flags.dirty               = 1;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSaveGraphicsState()
{
    Pal::Pm4::UniversalCmdBuffer::CmdSaveGraphicsState();

    CopyColorTargetViewStorage(m_colorTargetViewRestoreStorage, m_colorTargetViewStorage, &m_graphicsRestoreState);
    CopyDepthStencilViewStorage(&m_depthStencilViewRestoreStorage, &m_depthStencilViewStorage, &m_graphicsRestoreState);

    // We reset the rbplusRegHash in this cmdBuffer to 0, so that we'll definitely set the context roll state true
    // and update the values of rb+ registers through pm4 commands.
    m_rbplusRegHash        = 0;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdRestoreGraphicsStateInternal(
    bool trackBltActiveFlags)
{
    Pal::Pm4::UniversalCmdBuffer::CmdRestoreGraphicsStateInternal(trackBltActiveFlags);

    CopyColorTargetViewStorage(m_colorTargetViewStorage, m_colorTargetViewRestoreStorage, &m_graphicsState);
    CopyDepthStencilViewStorage(&m_depthStencilViewStorage, &m_depthStencilViewRestoreStorage, &m_graphicsState);

    // We reset the rbplusRegHash in this cmdBuffer to 0, so that we'll definitely set the context roll state true
    // and update the values of rb+ registers through pm4 commands.
    // Switching the pipeline during a pop operation will already cause a context roll, so forcing a re - write of the
    // RB + registers won't cause extra rolls.
    m_rbplusRegHash = 0;
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

    m_graphicsState.pColorBlendState           = pNewState;
    m_graphicsState.dirtyFlags.colorBlendState = 1;
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

    m_graphicsState.pDepthStencilState           = pNewState;
    m_graphicsState.dirtyFlags.depthStencilState = 1;
}

// =====================================================================================================================
// updates setting blend consts and manages dirty state
void UniversalCmdBuffer::CmdSetBlendConst(
    const BlendConstParams& params)
{
    m_graphicsState.blendConstState            = params;
    m_graphicsState.dirtyFlags.blendConstState = 1;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmCB_BLEND_RED,
                                                       mmCB_BLEND_ALPHA,
                                                       &params.blendConst[0],
                                                       pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Sets depth bounds to be applied with depth buffer comparisons
void UniversalCmdBuffer::CmdSetDepthBounds(
    const DepthBoundsParams& params)
{
    m_graphicsState.depthBoundsState            = params;
    m_graphicsState.dirtyFlags.depthBoundsState = 1;

    const float depthBounds[2] = { params.min, params.max, };
    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmDB_DEPTH_BOUNDS_MIN,
                                                       mmDB_DEPTH_BOUNDS_MAX,
                                                       &depthBounds[0],
                                                       pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Sets the current input assembly state
void UniversalCmdBuffer::CmdSetInputAssemblyState(
    const InputAssemblyStateParams& params)
{
    const uint32 idx = static_cast<uint32>(params.topology);
    PAL_ASSERT(idx < ArrayLen(TopologyToPrimTypeTable));

    regVGT_PRIMITIVE_TYPE vgtPrimitiveType = { };
    vgtPrimitiveType.bits.PRIM_TYPE = TopologyToPrimTypeTable[idx];

    regVGT_MULTI_PRIM_IB_RESET_INDX vgtMultiPrimIbResetIndx = { };
    vgtMultiPrimIbResetIndx.bits.RESET_INDX = params.primitiveRestartIndex;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    // If a mesh shader pipeline is active, we cannot write VGT_PRIMITIVE_TYPE because mesh shaders require us to
    // always use the POINTLIST topology.  VGT_PRIMITIVE_TYPE is written in CmdBindPipeline() when either enabling
    // or disabling mesh shader pipelines.
    if ((m_state.flags.meshShaderEnabled == 0) ||
        (m_state.flags.fastLaunchMode == static_cast<uint32>(GsFastLaunchMode::PrimInLane)))
    {
        pDeCmdSpace = m_deCmdStream.WriteSetOneConfigReg(mmVGT_PRIMITIVE_TYPE, vgtPrimitiveType.u32All, pDeCmdSpace);
    }

    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmVGT_MULTI_PRIM_IB_RESET_INDX,
                                                      vgtMultiPrimIbResetIndx.u32All,
                                                      pDeCmdSpace);

    m_deCmdStream.CommitCommands(pDeCmdSpace);

    m_geMultiPrimIbResetEn.bits.MATCH_ALL_BITS = params.primitiveRestartMatchAllBits;

    m_graphicsState.inputAssemblyState            = params;
    m_graphicsState.dirtyFlags.inputAssemblyState = 1;
}

// =====================================================================================================================
// Sets bit-masks to be applied to stencil buffer reads and writes.
void UniversalCmdBuffer::CmdSetStencilRefMasks(
    const StencilRefMaskParams& params)
{
    if (params.flags.u8All != 0x0)
    {
        SetStencilRefMasksState(params, &m_graphicsState.stencilRefMaskState);
        m_graphicsState.dirtyFlags.stencilRefMaskState = 1;

        struct
        {
            regDB_STENCILREFMASK     front;
            regDB_STENCILREFMASK_BF  back;
        } dbStencilRefMask = { };

        // NOTE: We are pulling state from m_graphicsState here explicitly so that for any fields
        // that the client didn't update - we will use our last known value. This avoids RMW.
        // The assumption here is that clients must update all these fields at some point during
        // command recording and that native DX12 bundle support is not supported (where we may
        // not know stencil ref values). Vulkan secondaries do not have this issue.
        dbStencilRefMask.front.bits.STENCILOPVAL       = m_graphicsState.stencilRefMaskState.frontOpValue;
        dbStencilRefMask.front.bits.STENCILTESTVAL     = m_graphicsState.stencilRefMaskState.frontRef;
        dbStencilRefMask.front.bits.STENCILMASK        = m_graphicsState.stencilRefMaskState.frontReadMask;
        dbStencilRefMask.front.bits.STENCILWRITEMASK   = m_graphicsState.stencilRefMaskState.frontWriteMask;
        dbStencilRefMask.back.bits.STENCILOPVAL_BF     = m_graphicsState.stencilRefMaskState.backOpValue;
        dbStencilRefMask.back.bits.STENCILTESTVAL_BF   = m_graphicsState.stencilRefMaskState.backRef;
        dbStencilRefMask.back.bits.STENCILMASK_BF      = m_graphicsState.stencilRefMaskState.backReadMask;
        dbStencilRefMask.back.bits.STENCILWRITEMASK_BF = m_graphicsState.stencilRefMaskState.backWriteMask;

        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

        pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmDB_STENCILREFMASK,
                                                           mmDB_STENCILREFMASK_BF,
                                                           &dbStencilRefMask,
                                                           pDeCmdSpace);

        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdAceWaitDe()
{
    if (m_pAceCmdStream != nullptr)
    {
        uint32* pAceCmdSpace = m_pAceCmdStream->ReserveCommands();

        // We need to make sure that the ACE CmdStream properly waits for any barriers that may have occured
        // on the DE CmdStream. We've been incrementing a counter on the DE CmdStream, so all we need to do
        // on the ACE side is perform the wait.
        pAceCmdSpace += CmdUtil::BuildWaitRegMem(EngineTypeCompute,
                                                 mem_space__mec_wait_reg_mem__memory_space,
                                                 function__mec_wait_reg_mem__greater_than_or_equal_reference_value,
                                                 0, // EngineSel enum does not exist in the MEC WAIT_REG_MEM packet.
                                                 GangedCmdStreamSemAddr(),
                                                 m_semCountAceWaitDe,
                                                 0xFFFFFFFF,
                                                 pAceCmdSpace);

        m_pAceCmdStream->CommitCommands(pAceCmdSpace);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdDeWaitAce()
{
    if (m_pAceCmdStream != nullptr)
    {
        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

        // We need to make sure that the DE CmdStream properly waits for any barriers that may have occured
        // on the ACE CmdStream. We've been incrementing a counter on the ACE CmdStream, so all we need to do
        // on the DE side is perform the wait.
        pDeCmdSpace += CmdUtil::BuildWaitRegMem(EngineTypeUniversal,
                                                mem_space__pfp_wait_reg_mem__memory_space,
                                                function__pfp_wait_reg_mem__greater_than_or_equal_reference_value,
                                                engine_sel__pfp_wait_reg_mem__prefetch_parser,
                                                GangedCmdStreamSemAddr() + sizeof(uint32),
                                                m_semCountDeWaitAce,
                                                0xFFFFFFFF,
                                                pDeCmdSpace);

        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBarrier(
    const BarrierInfo& barrierInfo)
{
    Pm4CmdBuffer::CmdBarrier(barrierInfo);

    for (uint32 i = 0; i < barrierInfo.transitionCount; i++)
    {
        if (barrierInfo.pTransitions[i].imageInfo.pImage != nullptr)
        {
            // We could do better here by detecting all layout/cache changes that could signal rate images
            // transitioning from writes to reads but that's pretty tricky. If this results in too many redundant
            // VRS HTile copies we can try to optimize it but we might need additional interface state to be safe.
            BarrierMightDirtyVrsRateImage(barrierInfo.pTransitions[i].imageInfo.pImage);
        }
    }
    IssueGangedBarrierAceWaitDeIncr();
}

// =====================================================================================================================
uint32 UniversalCmdBuffer::CmdRelease(
    const AcquireReleaseInfo& releaseInfo)
{
    const uint32 syncToken = Pm4CmdBuffer::CmdRelease(releaseInfo);

    for (uint32 i = 0; i < releaseInfo.imageBarrierCount; i++)
    {
        if (releaseInfo.pImageBarriers[i].pImage != nullptr)
        {
            // We could do better here by detecting all layout/cache changes that could signal rate images
            // transitioning from writes to reads but that's pretty tricky. If this results in too many redundant
            // VRS HTile copies we can try to optimize it but we might need additional interface state to be safe.
            BarrierMightDirtyVrsRateImage(releaseInfo.pImageBarriers[i].pImage);
        }
    }

    return syncToken;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdAcquire(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    syncTokenCount,
    const uint32*             pSyncTokens)
{
    Pm4CmdBuffer::CmdAcquire(acquireInfo, syncTokenCount, pSyncTokens);

    IssueGangedBarrierAceWaitDeIncr();
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdReleaseEvent(
    const AcquireReleaseInfo& releaseInfo,
    const IGpuEvent*          pGpuEvent)
{
    Pm4CmdBuffer::CmdReleaseEvent(releaseInfo, pGpuEvent);

    for (uint32 i = 0; i < releaseInfo.imageBarrierCount; i++)
    {
        if (releaseInfo.pImageBarriers[i].pImage != nullptr)
        {
            // We could do better here by detecting all layout/cache changes that could signal rate images
            // transitioning from writes to reads but that's pretty tricky. If this results in too many redundant
            // VRS HTile copies we can try to optimize it but we might need additional interface state to be safe.
            BarrierMightDirtyVrsRateImage(releaseInfo.pImageBarriers[i].pImage);
        }
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdAcquireEvent(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    gpuEventCount,
    const IGpuEvent* const*   ppGpuEvents)
{
    Pm4CmdBuffer::CmdAcquireEvent(acquireInfo, gpuEventCount, ppGpuEvents);

    IssueGangedBarrierAceWaitDeIncr();
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdReleaseThenAcquire(
    const AcquireReleaseInfo& barrierInfo)
{
    Pm4CmdBuffer::CmdReleaseThenAcquire(barrierInfo);

    for (uint32 i = 0; i < barrierInfo.imageBarrierCount; i++)
    {
        if (barrierInfo.pImageBarriers[i].pImage != nullptr)
        {
            // We could do better here by detecting all layout/cache changes that could signal rate images
            // transitioning from writes to reads but that's pretty tricky. If this results in too many redundant
            // VRS HTile copies we can try to optimize it but we might need additional interface state to be safe.
            BarrierMightDirtyVrsRateImage(barrierInfo.pImageBarriers[i].pImage);
        }
    }

    IssueGangedBarrierAceWaitDeIncr();
}

// =====================================================================================================================
// For ganged-submit with ACE+GFX, we need to ensure that any stalls that occur on the GFX engine are properly stalled
// on the ACE engine and vice versa. To that end, when we detect when ganged-submit is active, we issue a
// bottom-of-pipe timestamp event which will write the current barrier count. Later, when the ACE engine is used, we'll
// issue a WAIT_REG_MEM to ensure that all prior events on the GFX engine have completed.
void UniversalCmdBuffer::IssueGangedBarrierAceWaitDeIncr()
{
    m_semCountAceWaitDe++;

    if (m_pAceCmdStream != nullptr)
    {
        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

        ReleaseMemGfx releaseInfo = {};
        releaseInfo.vgtEvent = BOTTOM_OF_PIPE_TS;
        releaseInfo.dstAddr  = GangedCmdStreamSemAddr();
        releaseInfo.dataSel  = data_sel__me_release_mem__send_32_bit_low;
        releaseInfo.data     = m_semCountAceWaitDe;

        pDeCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseInfo, pDeCmdSpace);

        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::IssueGangedBarrierDeWaitAceIncr()
{
    m_semCountDeWaitAce++;

    if (m_pAceCmdStream != nullptr)
    {
        uint32* pAceCmdSpace = m_pAceCmdStream->ReserveCommands();

        ReleaseMemGfx releaseInfo = {};
        releaseInfo.vgtEvent = BOTTOM_OF_PIPE_TS;
        releaseInfo.dstAddr  = GangedCmdStreamSemAddr() + sizeof(uint32);
        releaseInfo.dataSel  = data_sel__mec_release_mem__send_32_bit_low;
        releaseInfo.data     = m_semCountDeWaitAce;

        pAceCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseInfo, pAceCmdSpace);

        m_pAceCmdStream->CommitCommands(pAceCmdSpace);
    }
}
// =====================================================================================================================
// Updates the ring size for Task+Mesh pipelines.
void UniversalCmdBuffer::UpdateTaskMeshRingSize()
{
    Device* pDevice = const_cast<Device*>(&m_device);

    m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::PayloadData)] =
        Max<size_t>(m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::PayloadData)], 1);

    m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::TaskMeshCtrlDrawRing)] =
        Max<size_t>(m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::TaskMeshCtrlDrawRing)], 1);

    GetAceCmdStream();
    ReportHybridPipelineBind();
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetVertexBuffers(
    const VertexBufferViews& bufferViews)
{
    PAL_ASSERT(bufferViews.bufferCount > 0);
    PAL_ASSERT((bufferViews.firstBuffer + bufferViews.bufferCount) <= MaxVertexBuffers);
    PAL_ASSERT(bufferViews.pBufferViewInfos != nullptr);

    // The vertex buffer table will be validated at Draw time, so all that is necessary is to
    // update the CPU-side copy of the SRD table and upload the new SRD data into CE RAM.
    if (bufferViews.offsetMode)
    {
        VertexBufferView* pViews = (m_vbTable.pBufferViews + bufferViews.firstBuffer);
        memcpy(pViews, bufferViews.pVertexBufferViews, sizeof(VertexBufferView) * bufferViews.bufferCount);
    }
    else
    {
        BufferSrd* const pSrds = (m_vbTable.pSrds + bufferViews.firstBuffer);
        m_device.Parent()->CreateUntypedBufferViewSrds(bufferViews.bufferCount, bufferViews.pBufferViewInfos, pSrds);
    }

    constexpr uint32 DwordsPerBufferView = Util::NumBytesToNumDwords(sizeof(VertexBufferView));
    constexpr uint32 DwordsPerSrd = Util::NumBytesToNumDwords(sizeof(BufferSrd));
    static_assert(DwordsPerSrd == DwordsPerBufferView);

    if ((DwordsPerSrd * bufferViews.firstBuffer) < m_vbTable.watermark)
    {
        // Only mark the contents as dirty if the updated VB table entries fall within the current high watermark.
        // This will help avoid redundant validation for data which the current pipeline doesn't care about.
        m_vbTable.state.dirty = 1;
    }

    m_vbTable.modified = 1;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindTargets(
    const BindTargetParams& params)
{
    constexpr uint32 AllColorTargetSlotMask = 255; // Mask of all color-target slots.

    bool colorTargetsChanged = false;

    // BIG_PAGE can only be enabled if all render targets are compatible.  Default to true and disable it later if we
    // find an incompatible target.
    bool   colorBigPage  = true;
    bool   fmaskBigPage  = true;
    bool   bypassMall    = true;

    bool validCbViewFound   = false;
    bool validAaCbViewFound = false;

    Extent2d surfaceExtent = { Pm4::MaxScissorExtent, Pm4::MaxScissorExtent }; // Default to fully open

    // Bind all color targets.
    const uint32  colorTargetLimit   = Max(params.colorTargetCount, m_graphicsState.bindTargets.colorTargetCount);
    uint32        newColorTargetMask = 0;
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

            const auto* pImage = pNewView->GetImage();

            if (pImage != nullptr)
            {
                colorBigPage &= pNewView->IsColorBigPage();

                // There is a shared bit to enable the BIG_PAGE optimization for all targets.  If this image doesn't
                // have fmask we should leave the accumulated fmaskBigPage state alone so other render targets that
                // do have fmask can still get the optimization.
                if (pImage->HasFmaskData())
                {
                    fmaskBigPage      &= pNewView->IsFmaskBigPage();
                    validAaCbViewFound = true;
                }
            }
            else
            {
                colorBigPage = false;
                fmaskBigPage = false;
            }

            if (m_cachedSettings.supportsMall != 0)
            {
                bypassMall &= pNewView->BypassMall();
            }

            validCbViewFound = true;
            m_state.flags.cbColorInfoDirtyRtv |= (1 << slot);
        }

        if ((pCurrentView != nullptr) && (pCurrentView->Equals(pNewView) == false))
        {
            colorTargetsChanged = true;
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
        pDeCmdSpace = ColorTargetView::HandleBoundTargetsChanged(m_cmdUtil, pDeCmdSpace);

        // Add a stall if needed after Flush events issued in HandleBoundTargetsChanged.
        if (m_cachedSettings.waitAfterCbFlush)
        {
            pDeCmdSpace = WriteWaitEop(HwPipePreColorTarget, false, SyncGlxNone, SyncRbNone, pDeCmdSpace);
        }
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
                                                   IsNested(),
                                                   &m_dbRenderOverride,
                                                   pDeCmdSpace);

        const Extent2d depthViewExtent = pNewDepthView->GetExtent();
        surfaceExtent.width  = Min(surfaceExtent.width,  depthViewExtent.width);
        surfaceExtent.height = Min(surfaceExtent.height, depthViewExtent.height);
    }
    else
    {
        pDeCmdSpace = WriteNullDepthTarget(pDeCmdSpace);
    }

    const bool depthTargetChanged = ((pCurrentDepthView != nullptr) &&
                                     (pCurrentDepthView->Equals(pNewDepthView) == false));

    if (depthTargetChanged)
    {
        // Handle the case where the depth view is changing.
        pDeCmdSpace = pCurrentDepthView->HandleBoundTargetChanged(this, pDeCmdSpace);

        // Add a stall if needed after Flush events issued in HandleBoundTargetChanged.
        if (m_cachedSettings.waitAfterDbFlush)
        {
            pDeCmdSpace = WriteWaitEop(HwPipePreRasterization, false, SyncGlxNone, SyncRbNone, pDeCmdSpace);
        }
    }

    if (m_cachedSettings.pbbMoreThanOneCtxState & (colorTargetsChanged | depthTargetChanged))
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

    // If next draw(s) that only change D/S targets, don't program CB_RMI_GL2_CACHE_CONTROL and let the state remains.
    // This is especially necessary for following HW bug WA. If client driver disable big page feature completely, then
    // the sync will still be issued for following case without this tweaking:
    // 1. Client draw to RT[0] (color big_page disable)
    // 2. Client clear DS surf (color big_page enable because no MRT is actually bound)
    // 3. Client draw to RT[0] (color big_page disable)
    // By old logic, the sync will be added between both #1/#2 and #2/#3. The sync added for #1/#2 is unnecessary and it
    // will cause minor CPU and CP performance drop; sync added for #2/#3 will do more than that by draining the whole
    // 3D pipeline, and is completely wrong behavior.
    if (validCbViewFound)
    {
        if (m_cachedSettings.waUtcL0InconsistentBigPage &&
            ((static_cast<bool>(m_cbRmiGl2CacheControl.bits.COLOR_BIG_PAGE) != colorBigPage) ||
             ((static_cast<bool>(m_cbRmiGl2CacheControl.gfx10.FMASK_BIG_PAGE) != fmaskBigPage) && validAaCbViewFound)))
        {
            // For following case, BIG_PAGE bit polarity changes between #A/#B and #C/#D, and we will need to add sync
            // A. Draw to RT[0] (big_page enable)
            // B. Draw to RT[0] + RT[1] (big_page disable due to RT[1] is not big page compatible)
            // C. Draw to RT[0] + RT[1] (big_page disable due to RT[1] is not big page compatible)
            // D. Draw to RT[0] (big_page enable)
            // For simplicity, we don't track big page setting polarity change based on MRT usage, but simply adding the
            // sync whenever a different big page setting value is going to be written into command buffer.
            AcquireMemGfxSurfSync acquireInfo = {};
            acquireInfo.flags.cbTargetStall    = 1;
            acquireInfo.flags.dbTargetStall    = 1; // It's not clear why the original workaround waited on DB.
            acquireInfo.flags.gfx10CbDataWbInv = 1;

            // This alert shouldn't be triggered frequently, or otherwise performance penalty will be there.
            // Consider either of following solutions to avoid the performance penalty:
            // - Enable "big page" for RT/MSAA resource, as many as possible
            // - Disable "big page" for RT/MSAA resource, as many as possible
            // Check IsColorBigPage()/IsFmaskBigPage() for the details about how to enable/disable big page
            PAL_ALERT_ALWAYS();

            pDeCmdSpace += m_cmdUtil.BuildAcquireMemGfxSurfSync(acquireInfo, pDeCmdSpace);
        }

        m_cbRmiGl2CacheControl.bits.COLOR_BIG_PAGE = colorBigPage;

        // Similar to "validCbViewFound" check, only update fmaskBigPage setting if next draw(s) really use fmask
        if (validAaCbViewFound)
        {
            m_cbRmiGl2CacheControl.gfx10.FMASK_BIG_PAGE = fmaskBigPage;
        }

        if (m_cachedSettings.supportsMall != 0)
        {
            if (IsNavi2x(*(m_device.Parent())))
            {
                m_cbRmiGl2CacheControl.nv2x.CMASK_L3_BYPASS = bypassMall;
                m_cbRmiGl2CacheControl.nv2x.FMASK_L3_BYPASS = bypassMall;
            }

            m_cbRmiGl2CacheControl.most.DCC_L3_BYPASS   = bypassMall;
            m_cbRmiGl2CacheControl.most.COLOR_L3_BYPASS = bypassMall;
        }

        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmCB_RMI_GL2_CACHE_CONTROL,
                                                          m_cbRmiGl2CacheControl.u32All,
                                                          pDeCmdSpace);
    }

    if ((surfaceExtent.width  != m_graphicsState.targetExtent.width) ||
        (surfaceExtent.height != m_graphicsState.targetExtent.height))
    {
        m_graphicsState.targetExtent = surfaceExtent;

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
            m_graphicsState.bindTargets.colorTargets[slot].imageLayout      = params.colorTargets[slot].imageLayout;
            m_graphicsState.bindTargets.colorTargets[slot].pColorTargetView = StoreColorTargetView(slot, params);
            updatedColorTargetCount = slot + 1;  // track last actual bound slot
        }
        else
        {
            m_graphicsState.bindTargets.colorTargets[slot] = {};
        }
    }
    m_graphicsState.bindTargets.colorTargetCount               = updatedColorTargetCount;
    m_graphicsState.bindTargets.depthTarget.depthLayout        = params.depthTarget.depthLayout;
    m_graphicsState.bindTargets.depthTarget.stencilLayout      = params.depthTarget.stencilLayout;
    m_graphicsState.bindTargets.depthTarget.pDepthStencilView  = StoreDepthStencilView(params);
    m_graphicsState.dirtyFlags.colorTargetView                 = 1;
    m_graphicsState.dirtyFlags.depthStencilView                = 1;
    PAL_ASSERT(m_graphicsState.inheritedState.stateFlags.targetViewState == 0);
}

// =====================================================================================================================
IColorTargetView* UniversalCmdBuffer::StoreColorTargetView(
    uint32 slot,
    const BindTargetParams& params)
{
    IColorTargetView* pColorTargetView = nullptr;

    if (IsGfx10(m_gfxIpLevel))
    {
        pColorTargetView = PAL_PLACEMENT_NEW(&m_colorTargetViewStorage[slot])
            Gfx10ColorTargetView(*static_cast<const Gfx10ColorTargetView*>(params.colorTargets[slot].pColorTargetView));
    }
    else if (IsGfx11(m_gfxIpLevel))
    {
        pColorTargetView = PAL_PLACEMENT_NEW(&m_colorTargetViewStorage[slot])
            Gfx11ColorTargetView(*static_cast<const Gfx11ColorTargetView*>(params.colorTargets[slot].pColorTargetView));
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    return pColorTargetView;
}

// =====================================================================================================================
void UniversalCmdBuffer::CopyColorTargetViewStorage(
    ColorTargetViewStorage*       pColorTargetViewStorageDst,
    const ColorTargetViewStorage* pColorTargetViewStorageSrc,
    Pm4::GraphicsState*           pGraphicsStateDst)
{
    if (pGraphicsStateDst->bindTargets.colorTargetCount > 0)
    {
        memcpy(pColorTargetViewStorageDst, pColorTargetViewStorageSrc,
            sizeof(ColorTargetViewStorage) * pGraphicsStateDst->bindTargets.colorTargetCount);

        for (uint32 slot = 0; slot < pGraphicsStateDst->bindTargets.colorTargetCount; ++slot)
        {
            // if the view pointer wasn't null, overwrite it with the new storage location
            if (pGraphicsStateDst->bindTargets.colorTargets[slot].pColorTargetView != nullptr)
            {
                pGraphicsStateDst->bindTargets.colorTargets[slot].pColorTargetView =
                    reinterpret_cast<IColorTargetView*>(&pColorTargetViewStorageDst[slot]);
            }
        }
    }
}

// =====================================================================================================================
IDepthStencilView* UniversalCmdBuffer::StoreDepthStencilView(
    const BindTargetParams& params)
{
    IDepthStencilView* pDepthStencilView = nullptr;

    if (params.depthTarget.pDepthStencilView != nullptr)
    {
        pDepthStencilView = PAL_PLACEMENT_NEW(&m_depthStencilViewStorage)
            Gfx10DepthStencilView(*static_cast<const Gfx10DepthStencilView*>(params.depthTarget.pDepthStencilView));
    }

    return pDepthStencilView;
}

// =====================================================================================================================
void UniversalCmdBuffer::CopyDepthStencilViewStorage(
    DepthStencilViewStorage*       pDepthStencilViewStorageDst,
    const DepthStencilViewStorage* pDepthStencilViewStorageSrc,
    Pm4::GraphicsState*            pGraphicsStateDst)
{
    if (pGraphicsStateDst->bindTargets.depthTarget.pDepthStencilView != nullptr)
    {
        memcpy(pDepthStencilViewStorageDst, pDepthStencilViewStorageSrc, sizeof(DepthStencilViewStorage));

        pGraphicsStateDst->bindTargets.depthTarget.pDepthStencilView =
                reinterpret_cast<IDepthStencilView*>(pDepthStencilViewStorageDst);
    }
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
            auto*const pBufferSrd = &m_streamOut.srd[idx];

            bufferSize = LowPart(params.target[idx].size) / sizeof(uint32);
            PAL_ASSERT(HighPart(params.target[idx].size) == 0);

            uint32 strideInBytes = ((pPipeline == nullptr) ? 0 : pPipeline->StrmoutVtxStrideDw(idx)) * sizeof(uint32);

            if (SupportsSwStrmout() && (strideInBytes > 0))
            {
                // Software streamout only supports byte-addressed buffers, which is indicated by passing
                //  Stride=1 and buffer size in NumRecords.
                strideInBytes = 1;
            }

            m_device.SetNumRecords(pBufferSrd, StreamOutNumRecords(chipProps,
                                                                   LowPart(params.target[idx].size),
                                                                   strideInBytes));

            m_device.InitBufferSrd(pBufferSrd, params.target[idx].gpuVirtAddr, strideInBytes);

            pBufferSrd->add_tid_enable = 0;
            pBufferSrd->oob_select     = SQ_OOB_INDEX_ONLY;

            if (IsGfx10(m_gfxIpLevel))
            {
                pBufferSrd->gfx10.format = BUF_FMT_32_UINT;
            }
            else
            {
                pBufferSrd->gfx11.format = BUF_FMT_32_UINT;
            }
        }
        else
        {
            static_assert(SQ_SEL_0                == 0, "Unexpected value for SQ_SEL_0!");
            static_assert(BUF_DATA_FORMAT_INVALID == 0, "Unexpected value for BUF_DATA_FORMAT_INVALID!");
            memset(&m_streamOut.srd[idx], 0, sizeof(m_streamOut.srd[0]));
        }

        if (SupportsSwStrmout() == false)
        {
            constexpr uint32 RegStride = (Gfx10::mmVGT_STRMOUT_BUFFER_SIZE_1 -
                                          Gfx10::mmVGT_STRMOUT_BUFFER_SIZE_0);
            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(Gfx10::mmVGT_STRMOUT_BUFFER_SIZE_0 + (RegStride * idx),
                                                              bufferSize,
                                                              pDeCmdSpace);
        }
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);

    // The stream-out table is being managed by the CPU through embedded-data, just mark it dirty since we
    // need to update the whole table at Draw-time anyway.
    m_streamOut.state.dirty = 1;

    m_graphicsState.bindStreamOutTargets        = params;
    m_graphicsState.dirtyFlags.streamOutTargets = 1;
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
    m_state.flags.optimizeLinearGfxCpy             = optimizeLinearDestGfxCopy;
    m_graphicsState.triangleRasterState            = params;
    m_graphicsState.dirtyFlags.triangleRasterState = 1;
    m_nggState.flags.dirty                         = 1;

    if (static_cast<TossPointMode>(m_cachedSettings.tossPointMode) == TossPointWireframe)
    {
        m_graphicsState.triangleRasterState.frontFillMode = FillMode::Wireframe;
        m_graphicsState.triangleRasterState.backFillMode  = FillMode::Wireframe;
    }

    if (static_cast<TossPointMode>(m_cachedSettings.tossPointMode) == TossPointBackFrontFaceCull)
    {
        m_graphicsState.triangleRasterState.cullMode = CullMode::FrontAndBack;
    }
}

// =====================================================================================================================
// Sets parameters controlling point and line rasterization.
void UniversalCmdBuffer::CmdSetPointLineRasterState(
    const PointLineRasterStateParams& params)
{
    m_graphicsState.pointLineRasterState            = params;
    m_graphicsState.dirtyFlags.pointLineRasterState = 1;

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
}

// =====================================================================================================================
// Sets depth bias parameters.
void UniversalCmdBuffer::CmdSetDepthBiasState(
    const DepthBiasParams& params)
{
    m_graphicsState.depthBiasState            = params;
    m_graphicsState.dirtyFlags.depthBiasState = 1;

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
}

// =====================================================================================================================
// Sets global scissor rectangle params.
void UniversalCmdBuffer::CmdSetGlobalScissor(
    const GlobalScissorParams& params)
{
    m_graphicsState.globalScissorState            = params;
    m_graphicsState.dirtyFlags.globalScissorState = 1;

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
}

// =====================================================================================================================
// This function produces a draw developer callback based on current pipeline state.
void UniversalCmdBuffer::DescribeDraw(
    Developer::DrawDispatchType cmdType,
    bool                        includedGangedAce)
{
    // Get the first user data register offset depending on which HW shader stage is running the VS
    const auto*  pPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const uint32 userData0 = pPipeline->GetVsUserDataBaseOffset();

    uint32 firstVertexIdx   = UINT_MAX;
    uint32 startInstanceIdx = UINT_MAX;
    uint32 drawIndexIdx     = UINT_MAX;

    if ((cmdType != Developer::DrawDispatchType::CmdDispatchMesh) &&
        (cmdType != Developer::DrawDispatchType::CmdDispatchMeshIndirectMulti))
    {
        // Compute register offsets of first vertex and start instance user data locations relative to
        // user data 0.
        PAL_ASSERT((GetVertexOffsetRegAddr() != 0) && (GetInstanceOffsetRegAddr() != 0));
        PAL_ASSERT(GetVertexOffsetRegAddr() >= userData0);
        PAL_ASSERT(GetInstanceOffsetRegAddr() >= userData0);

        firstVertexIdx   = GetVertexOffsetRegAddr() - userData0;
        startInstanceIdx = GetInstanceOffsetRegAddr() - userData0;
    }

    if (m_drawIndexReg != UserDataNotMapped)
    {
        drawIndexIdx = m_drawIndexReg - userData0;
    }

    RgpMarkerSubQueueFlags subQueueFlags { };
    subQueueFlags.includeMainSubQueue    = 1;
    subQueueFlags.includeGangedSubQueues = includedGangedAce;

    m_device.DescribeDraw(this, subQueueFlags, cmdType, firstVertexIdx, startInstanceIdx, drawIndexIdx);
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
    uint32      instanceCount,
    uint32      drawId)
{
    auto*        pThis    = static_cast<UniversalCmdBuffer*>(pCmdBuffer);
    const auto&  cmdUtil  = pThis->m_device.CmdUtil();

    Pm4::ValidateDrawInfo drawInfo;
    drawInfo.vtxIdxCount       = vertexCount;
    drawInfo.instanceCount     = instanceCount;
    drawInfo.firstVertex       = firstVertex;
    drawInfo.firstInstance     = firstInstance;
    drawInfo.firstIndex        = 0;
    drawInfo.drawIndex         = drawId;
    drawInfo.useOpaque         = false;
    drawInfo.multiIndirectDraw = false;

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
        pDeCmdSpace += cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                        EngineTypeUniversal,
                                                        pThis->PacketPredicate(),
                                                        pDeCmdSpace);
    }
    if (HasUavExport)
    {
        pDeCmdSpace += cmdUtil.BuildNonSampleEventWrite(PS_PARTIAL_FLUSH, EngineTypeUniversal, pDeCmdSpace);
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

    Pm4::ValidateDrawInfo drawInfo;
    drawInfo.vtxIdxCount       = 0;
    drawInfo.instanceCount     = instanceCount;
    drawInfo.firstVertex       = 0;
    drawInfo.firstInstance     = firstInstance;
    drawInfo.firstIndex        = 0;
    drawInfo.drawIndex         = 0;
    drawInfo.useOpaque         = true;
    drawInfo.multiIndirectDraw = false;

    pThis->ValidateDraw<false, false>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawOpaque);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    // The LOAD_CONTEXT_REG_INDEX packet does the load via PFP while the streamOutFilledSizeVa is written
    // via ME in STRMOUT_BUFFER_UPDATE packet. So there might be race condition issue loading the filled size.
    // Before the load packet was used (to handle state shadowing), COPY_DATA via ME was used to program the
    // register so there was no sync issue.
    // To fix this race condition, a PFP_SYNC_ME packet is required to make it right.
    pDeCmdSpace += pThis->m_cmdUtil.BuildPfpSyncMe(pDeCmdSpace);
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
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                                 EngineTypeUniversal,
                                                                 pThis->PacketPredicate(),
                                                                 pDeCmdSpace);
    }
    if (HasUavExport)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(PS_PARTIAL_FLUSH, EngineTypeUniversal, pDeCmdSpace);
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
    uint32      instanceCount,
    uint32      drawId)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    Pm4::ValidateDrawInfo drawInfo;
    drawInfo.vtxIdxCount       = indexCount;
    drawInfo.instanceCount     = instanceCount;
    drawInfo.firstVertex       = vertexOffset;
    drawInfo.firstInstance     = firstInstance;
    drawInfo.firstIndex        = firstIndex;
    drawInfo.drawIndex         = drawId;
    drawInfo.useOpaque         = false;
    drawInfo.multiIndirectDraw = false;

    pThis->ValidateDraw<true, false>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawIndexed);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    // The "validIndexCount" (set later in the code) will eventually be used to program the max_size
    // field in the draw packet, which is used to clamp how much of the index buffer can be read.
    //
    // For out-of-bounds index buffer fetches cases:
    // - the firstIndex parameter of the draw command is greater than the currently IB's indexCount
    // - Or binding a null IB (IB's indexCount = 0)
    // We consider validIndexCount = 0.
    // When validIndexCount == 0, the workaround HandleZeroIndexBuffer() is active,
    // we bind a one index sized index buffer with value 0 to conform to that requirement.
    uint32 validIndexCount = (firstIndex >= pThis->m_graphicsState.iaState.indexCount)
                             ? 0
                             : pThis->m_graphicsState.iaState.indexCount - firstIndex;

    pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);

    if (ViewInstancingEnable)
    {
        const GraphicsPipeline* pPipeline =
            static_cast<const GraphicsPipeline*>(pThis->m_graphicsState.pipelineState.pPipeline);
        const auto& viewInstancingDesc = pPipeline->GetViewInstancingDesc();
        uint32      mask               = (1 << viewInstancingDesc.viewInstanceCount) - 1;

        if (viewInstancingDesc.enableMasking)
        {
            mask &= pThis->m_graphicsState.viewInstanceMask;
        }

        for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
        {
            if (TestAnyFlagSet(mask, 1))
            {
                pDeCmdSpace = pThis->BuildWriteViewId(viewInstancingDesc.viewId[i], pDeCmdSpace);

                if (pThis->IsNested() && (pThis->m_graphicsState.iaState.indexAddr == 0) && (validIndexCount > 0))
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
                    const uint32 indexSize   = 1 << static_cast<uint32>(pThis->m_graphicsState.iaState.indexType);
                    gpusize      gpuVirtAddr = pThis->m_graphicsState.iaState.indexAddr + (indexSize * firstIndex);

                    pThis->m_workaroundState.HandleZeroIndexBuffer(pThis, &gpuVirtAddr, &validIndexCount);

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
        if (pThis->IsNested() && (pThis->m_graphicsState.iaState.indexAddr == 0) && (validIndexCount > 0))
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
            const uint32 indexSize   = 1 << static_cast<uint32>(pThis->m_graphicsState.iaState.indexType);
            gpusize      gpuVirtAddr = pThis->m_graphicsState.iaState.indexAddr + (indexSize * firstIndex);

            pThis->m_workaroundState.HandleZeroIndexBuffer(pThis, &gpuVirtAddr, &validIndexCount);

            pDeCmdSpace += CmdUtil::BuildDrawIndex2(indexCount,
                                                    validIndexCount,
                                                    gpuVirtAddr,
                                                    pThis->PacketPredicate(),
                                                    pDeCmdSpace);
        }
    }

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                                 EngineTypeUniversal,
                                                                 pThis->PacketPredicate(),
                                                                 pDeCmdSpace);
    }
    if (HasUavExport)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(PS_PARTIAL_FLUSH, EngineTypeUniversal, pDeCmdSpace);
    }

    pDeCmdSpace  = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Issues an indirect non-indexed draw command. We must discard the draw if vertexCount or instanceCount are zero.
// We will rely on the HW to discard the draw for us.
template <bool IssueSqttMarkerEvent, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawIndirectMulti(
    ICmdBuffer*          pCmdBuffer,
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    gpusize gpuVirtAddr = gpuVirtAddrAndStride.gpuVirtAddr;
    uint32  stride      = static_cast<uint32>(gpuVirtAddrAndStride.stride);

    // These are not the true BaseAddr and offset, but in order to preserve the SET_BASE optimization,
    // we set the high 32-bits of the virtual address as the base. The low 32 bits are used as
    // the offset in the DISPATCH_INDIRECT packet.
    gpusize gpuBaseAddr = HighPart64(gpuVirtAddr);
    gpusize offset      = LowPart(gpuVirtAddr);

    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)) && IsPow2Aligned(countGpuAddr, sizeof(uint32)));

    PAL_ASSERT((countGpuAddr != 0uLL) || (maximumCount != 0));

    bool multiPacketUsed = false;

    Pm4::ValidateDrawInfo drawInfo;
    drawInfo.vtxIdxCount       = 0;
    drawInfo.instanceCount     = 0;
    drawInfo.firstVertex       = 0;
    drawInfo.firstInstance     = 0;
    drawInfo.firstIndex        = 0;
    drawInfo.drawIndex         = 0;
    drawInfo.useOpaque         = false;
    drawInfo.multiIndirectDraw = (maximumCount > 1) || (countGpuAddr != 0uLL);

    pThis->ValidateDraw<false, true>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawIndirectMulti);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace = pThis->m_deCmdStream.WriteSetBase(gpuBaseAddr,
                                                    base_index__pfp_set_base__patch_table_base,
                                                    ShaderGraphics,
                                                    pDeCmdSpace);

    const uint16 vtxOffsetReg  = pThis->GetVertexOffsetRegAddr();
    const uint16 instOffsetReg = pThis->GetInstanceOffsetRegAddr();
    const uint16 drawIndexReg  = pThis->GetDrawIndexRegAddr();

    pThis->m_deCmdStream.NotifyIndirectShRegWrite(vtxOffsetReg);
    pThis->m_deCmdStream.NotifyIndirectShRegWrite(instOffsetReg);

    if (drawIndexReg != UserDataNotMapped)
    {
        pThis->m_deCmdStream.NotifyIndirectShRegWrite(drawIndexReg);
    }

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

                if ((maximumCount == 1) && (countGpuAddr == 0uLL))
                {
                    pDeCmdSpace += CmdUtil::BuildDrawIndirect(offset,
                                                              vtxOffsetReg,
                                                              instOffsetReg,
                                                              pThis->PacketPredicate(),
                                                              pDeCmdSpace);
                }
                else
                {
                    multiPacketUsed = true;
                    pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndirectMulti<IssueSqttMarkerEvent>(
                        offset,
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
        if ((maximumCount == 1) && (countGpuAddr == 0uLL))
        {
            pDeCmdSpace += CmdUtil::BuildDrawIndirect(offset,
                                                      vtxOffsetReg,
                                                      instOffsetReg,
                                                      pThis->PacketPredicate(),
                                                      pDeCmdSpace);
        }
        else
        {
            multiPacketUsed = true;
            pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndirectMulti<IssueSqttMarkerEvent>(
                offset,
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

    // SQTT Marker was added as a bit in the DrawIndirectMulti PM4 to be set for Gfx10+.
    if (IssueSqttMarkerEvent
#if (PAL_BUILD_BRANCH >= 2410)
        && (multiPacketUsed == false)
#endif
       )
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                                 EngineTypeUniversal,
                                                                 pThis->PacketPredicate(),
                                                                 pDeCmdSpace);
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
    ICmdBuffer*          pCmdBuffer,
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    gpusize gpuVirtAddr = gpuVirtAddrAndStride.gpuVirtAddr;
    uint32  stride      = static_cast<uint32>(gpuVirtAddrAndStride.stride);

    // These values are not the true BaseAddr and offset, but in order to preserve the SET_BASE optimization,
    // we set the high 32-bits of the virtual address as the base. The low 32 bits are used as
    // the offset in the DISPATCH_INDIRECT packet.
    gpusize gpuBaseAddr = HighPart64(gpuVirtAddr);
    gpusize offset      = LowPart(gpuVirtAddr);

    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)) && IsPow2Aligned(countGpuAddr, sizeof(uint32)));

    PAL_ASSERT((countGpuAddr != 0) || (maximumCount != 0));

    bool multiPacketUsed = false;

    Pm4::ValidateDrawInfo drawInfo;
    drawInfo.vtxIdxCount       = 0;
    drawInfo.instanceCount     = 0;
    drawInfo.firstVertex       = 0;
    drawInfo.firstInstance     = 0;
    drawInfo.firstIndex        = 0;
    drawInfo.drawIndex         = 0;
    drawInfo.useOpaque         = false;
    drawInfo.multiIndirectDraw = (maximumCount > 1) || (countGpuAddr != 0uLL);

    pThis->ValidateDraw<true, true>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawIndexedIndirectMulti);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace = pThis->m_deCmdStream.WriteSetBase(gpuBaseAddr,
                                                    base_index__pfp_set_base__patch_table_base,
                                                    ShaderGraphics,
                                                    pDeCmdSpace);

    const uint16 vtxOffsetReg  = pThis->GetVertexOffsetRegAddr();
    const uint16 instOffsetReg = pThis->GetInstanceOffsetRegAddr();
    const uint16 drawIndexReg  = pThis->GetDrawIndexRegAddr();

    pThis->m_deCmdStream.NotifyIndirectShRegWrite(vtxOffsetReg);
    pThis->m_deCmdStream.NotifyIndirectShRegWrite(instOffsetReg);

    if (drawIndexReg != UserDataNotMapped)
    {
        pThis->m_deCmdStream.NotifyIndirectShRegWrite(drawIndexReg);
    }

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

                if ((maximumCount == 1) && (countGpuAddr == 0uLL))
                {
                    pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexIndirect(offset,
                                                                           vtxOffsetReg,
                                                                           instOffsetReg,
                                                                           pThis->PacketPredicate(),
                                                                           pDeCmdSpace);
                }
                else
                {
                    multiPacketUsed = true;
                    pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexIndirectMulti<IssueSqttMarkerEvent>(
                        offset,
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
        if ((maximumCount == 1) && (countGpuAddr == 0uLL))
        {
            pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexIndirect(offset,
                                                                   vtxOffsetReg,
                                                                   instOffsetReg,
                                                                   pThis->PacketPredicate(),
                                                                   pDeCmdSpace);
        }
        else
        {
            multiPacketUsed = true;
            pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexIndirectMulti<IssueSqttMarkerEvent>(offset,
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

    if (IssueSqttMarkerEvent
#if (PAL_BUILD_BRANCH >= 2410)
        && (multiPacketUsed == false)
#endif
       )
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                                 EngineTypeUniversal,
                                                                 pThis->PacketPredicate(),
                                                                 pDeCmdSpace);
    }

    pDeCmdSpace = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);

    pThis->m_state.flags.containsDrawIndirect = 1;
}

// =====================================================================================================================
// Issues a direct dispatch command. We must discard the dispatch if x, y, or z are zero. To avoid branching, we will
// rely on the HW to discard the dispatch for us.
template <bool HsaAbi, bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatch(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    auto*        pThis   = static_cast<UniversalCmdBuffer*>(pCmdBuffer);
    const auto&  cmdUtil = pThis->m_device.CmdUtil();

    if (DescribeDrawDispatch)
    {
        pThis->DescribeDispatch(Developer::DrawDispatchType::CmdDispatch, size);
    }

    if (HsaAbi)
    {
        pThis->ValidateDispatchHsaAbi(&pThis->m_computeState, &pThis->m_deCmdStream, {}, size);
    }
    else
    {
        pThis->ValidateDispatchPalAbi(&pThis->m_computeState, &pThis->m_deCmdStream, 0uLL, size);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();
    pDeCmdSpace  = pThis->WaitOnCeCounter(pDeCmdSpace);

    pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect<false, true>(size,
                                                                     pThis->PacketPredicate(),
                                                                     pThis->m_pSignatureCs->flags.isWave32,
                                                                     pThis->UsesDispatchTunneling(),
                                                                     false,
                                                                     pDeCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                        EngineTypeUniversal,
                                                        pThis->PacketPredicate(),
                                                        pDeCmdSpace);
    }

    pDeCmdSpace = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Issues an indirect dispatch command. We must discard the dispatch if x, y, or z are zero. We will rely on the HW to
// discard the dispatch for us.
template <bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchIndirect(
    ICmdBuffer*      pCmdBuffer,
    gpusize          gpuVirtAddr)
{
    auto*        pThis   = static_cast<UniversalCmdBuffer*>(pCmdBuffer);
    const auto&  cmdUtil = pThis->m_device.CmdUtil();

    // These values are not the true BaseAddr and offset, but in order to preserve the SET_BASE optimization,
    // we set the high 32-bits of the virtual address as the base. The low 32 bits are used as
    // the offset in the DISPATCH_INDIRECT packet.
    gpusize gpuBaseAddr = HighPart64(gpuVirtAddr);
    gpusize offset      = LowPart(gpuVirtAddr);

    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)));

    if (DescribeDrawDispatch)
    {
        pThis->DescribeDispatchIndirect();
    }

    pThis->ValidateDispatchPalAbi(&pThis->m_computeState, &pThis->m_deCmdStream, gpuVirtAddr, {});

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();
    pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);
    pDeCmdSpace = pThis->m_deCmdStream.WriteSetBase(gpuBaseAddr,
                                                    base_index__pfp_set_base__patch_table_base,
                                                    ShaderCompute,
                                                    pDeCmdSpace);
    pDeCmdSpace += CmdUtil::BuildDispatchIndirectGfx(offset,
                                                     pThis->PacketPredicate(),
                                                     pThis->m_pSignatureCs->flags.isWave32,
                                                     pDeCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                        EngineTypeUniversal,
                                                        pThis->PacketPredicate(),
                                                        pDeCmdSpace);
    }

    pDeCmdSpace = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);

    pThis->m_state.flags.containsDrawIndirect = 1;
}

// =====================================================================================================================
// Issues a direct dispatch command with immediate threadgroup offsets. We must discard the dispatch if x, y, or z are
// zero. To avoid branching, we will rely on the HW to discard the dispatch for us.
template <bool HsaAbi, bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchOffset(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims offset,
    DispatchDims launchSize,
    DispatchDims logicalSize)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    if (DescribeDrawDispatch)
    {
        pThis->DescribeDispatchOffset(offset, launchSize, logicalSize);
    }
    if (HsaAbi)
    {
        pThis->ValidateDispatchHsaAbi(&pThis->m_computeState, &pThis->m_deCmdStream, offset, logicalSize);
    }
    else
    {
        pThis->ValidateDispatchPalAbi(&pThis->m_computeState, &pThis->m_deCmdStream, 0uLL, logicalSize);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace = pThis->m_deCmdStream.WriteSetSeqShRegs(mmCOMPUTE_START_X,
                                                         mmCOMPUTE_START_Z,
                                                         ShaderCompute,
                                                         &offset,
                                                         pDeCmdSpace);

    pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);

    // The dispatch packet's size is an end position instead of the number of threadgroups to execute.
    pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect<false, false>(offset + launchSize,
                                                                      pThis->PacketPredicate(),
                                                                      pThis->m_pSignatureCs->flags.isWave32,
                                                                      pThis->UsesDispatchTunneling(),
                                                                      false,
                                                                      pDeCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                                 EngineTypeUniversal,
                                                                 pThis->PacketPredicate(),
                                                                 pDeCmdSpace);
    }

    pDeCmdSpace = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Generates commands required for execution of a Mesh-only pipeline. This version focuses on the use of
// the VertInLane GsFastLaunchMode which emulates threadgroups by setting the number of verts/prims in a subgroup
// to 1 and uses the primitive amplification factor to "grow" the subgroup to the threadgroup sizes
// required by the shader.
template <bool IssueSqttMarkerEvent, bool HasUavExport, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchMeshAmpFastLaunch(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    auto*const pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    const GraphicsPipelineSignature& graphicsPipelineSig =
                    static_cast<const GraphicsPipeline*>(pThis->m_graphicsState.pipelineState.pPipeline)->Signature();

    const uint16 meshDispatchDimsRegAddr = graphicsPipelineSig.meshDispatchDimsRegAddr;
    if (meshDispatchDimsRegAddr != UserDataNotMapped)
    {
        // We write dispatch dimensions before draw time validation to pack the writes into the valid user-data array
        // with any additional user-data writes in ValidateDraw() on GFX11.
        uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();
        const uint32 dimensions[3] = { size.x, size.y, size.z };
        pDeCmdSpace = pThis->SetSeqUserSgprRegs<ShaderGraphics>(meshDispatchDimsRegAddr,
                                                                meshDispatchDimsRegAddr + 2,
                                                                &dimensions,
                                                                false,
                                                                pDeCmdSpace);
        pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);
    }

    Pm4::ValidateDrawInfo drawInfo;
    drawInfo.vtxIdxCount       = 0;
    drawInfo.instanceCount     = 1;
    drawInfo.firstVertex       = 0;
    drawInfo.firstInstance     = 0;
    drawInfo.firstIndex        = 0;
    drawInfo.drawIndex         = 0;
    drawInfo.useOpaque         = false;
    drawInfo.multiIndirectDraw = false;
    pThis->ValidateDraw<false, false>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDispatchMesh);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);

    // CmdDispatchMesh with no task shader is emulated by using a non-indexed draw where the vertex count equals
    // the total number of mesh workgroups being dispatched.
    const uint32 workgroupCount = size.Flatten();

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
                pDeCmdSpace += CmdUtil::BuildDrawIndexAuto(workgroupCount,
                                                           false,
                                                           pThis->PacketPredicate(),
                                                           pDeCmdSpace);
            }
        }
    }
    else
    {
        pDeCmdSpace += CmdUtil::BuildDrawIndexAuto(workgroupCount, false, pThis->PacketPredicate(), pDeCmdSpace);
    }

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                                 EngineTypeUniversal,
                                                                 pThis->PacketPredicate(),
                                                                 pDeCmdSpace);
    }
    if (HasUavExport)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(PS_PARTIAL_FLUSH, EngineTypeUniversal, pDeCmdSpace);
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
// Generates commands required for execution of a Mesh-only pipeline. This version focuses on the use of
// the PrimInLane GsFastLaunchMode which uses the X, Y, and Z dimensions programmed into registers to appropriately
// size the subgroup explicitly.
template <bool IssueSqttMarkerEvent, bool HasUavExport, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchMeshNative(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    auto* const pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    const GraphicsPipelineSignature& graphicsPipelineSig =
                    static_cast<const GraphicsPipeline*>(pThis->m_graphicsState.pipelineState.pPipeline)->Signature();

    const uint16 meshDispatchDimsRegAddr = graphicsPipelineSig.meshDispatchDimsRegAddr;
    if (meshDispatchDimsRegAddr != UserDataNotMapped)
    {
        // We write dispatch dimensions before draw time validation to pack the writes into the valid user-data array
        // with any additional user-data writes in ValidateDraw() on GFX11.
        uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();
        const uint32 dimensions[3] = { size.x, size.y, size.z };
        pDeCmdSpace = pThis->SetSeqUserSgprRegs<ShaderGraphics>(meshDispatchDimsRegAddr,
                                                                meshDispatchDimsRegAddr + 2,
                                                                &dimensions,
                                                                false,
                                                                pDeCmdSpace);
        pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);
    }

    Pm4::ValidateDrawInfo drawInfo;
    drawInfo.vtxIdxCount       = 0;
    drawInfo.instanceCount     = 1;
    drawInfo.firstVertex       = 0;
    drawInfo.firstInstance     = 0;
    drawInfo.firstIndex        = 0;
    drawInfo.drawIndex         = 0;
    drawInfo.useOpaque         = false;
    drawInfo.multiIndirectDraw = false;
    pThis->ValidateDraw<false, false>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDispatchMesh);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);

    if (ViewInstancingEnable)
    {
        const auto* const pPipeline =
            static_cast<const GraphicsPipeline*>(pThis->m_graphicsState.pipelineState.pPipeline);
        const auto& viewInstancingDesc = pPipeline->GetViewInstancingDesc();
        uint32           mask = (1 << viewInstancingDesc.viewInstanceCount) - 1;

        if (viewInstancingDesc.enableMasking)
        {
            mask &= pThis->m_graphicsState.viewInstanceMask;
        }

        for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
        {
            if (TestAnyFlagSet(mask, 1))
            {
                pDeCmdSpace  = pThis->BuildWriteViewId(viewInstancingDesc.viewId[i], pDeCmdSpace);
                pDeCmdSpace += CmdUtil::BuildDispatchMeshDirect(size, pThis->PacketPredicate(), pDeCmdSpace);
            }
        }
    }
    else
    {
        pDeCmdSpace += CmdUtil::BuildDispatchMeshDirect(size, pThis->PacketPredicate(), pDeCmdSpace);
    }

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                                 EngineTypeUniversal,
                                                                 pThis->PacketPredicate(),
                                                                 pDeCmdSpace);
    }
    if (HasUavExport)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(PS_PARTIAL_FLUSH, EngineTypeUniversal, pDeCmdSpace);
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
template <bool IssueSqttMarkerEvent, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchMeshIndirectMulti(
    ICmdBuffer*          pCmdBuffer,
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    auto*const pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    gpusize gpuVirtAddr = gpuVirtAddrAndStride.gpuVirtAddr;
    uint32  stride      = static_cast<uint32>(gpuVirtAddrAndStride.stride);

    // These are not the true BaseAddr and offset, but in order to preserve the SET_BASE optimization,
    // we set the high 32-bits of the virtual address as the base. The low 32 bits are used as
    // the offset in the DISPATCH_INDIRECT packet.
    gpusize gpuBaseAddr = HighPart64(gpuVirtAddr);
    gpusize offset      = LowPart(gpuVirtAddr);

    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)));

    constexpr Pm4::ValidateDrawInfo DrawInfo = { };
    pThis->ValidateDraw<false, true>(DrawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDispatchMeshIndirectMulti);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace = pThis->m_deCmdStream.WriteSetBase(gpuBaseAddr,
                                                    base_index__pfp_set_base__patch_table_base,
                                                    ShaderGraphics,
                                                    pDeCmdSpace);

    const uint16 xyzOffsetReg = pThis->m_pSignatureGfx->meshDispatchDimsRegAddr;
    pThis->m_deCmdStream.NotifyIndirectShRegWrite(xyzOffsetReg);

    pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);

    const bool usesLegacyMsFastLaunch =
        (static_cast<GsFastLaunchMode>(pThis->m_state.flags.fastLaunchMode) == GsFastLaunchMode::VertInLane);

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
                pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchMeshIndirectMulti<IssueSqttMarkerEvent>(
                    offset,
                    xyzOffsetReg,
                    pThis->m_drawIndexReg,
                    maximumCount,
                    stride,
                    countGpuAddr,
                    pThis->PacketPredicate(),
                    usesLegacyMsFastLaunch,
                    pDeCmdSpace);
            }
        }
    }
    else
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchMeshIndirectMulti<IssueSqttMarkerEvent>(
            offset,
            xyzOffsetReg,
            pThis->m_drawIndexReg,
            maximumCount,
            stride,
            countGpuAddr,
            pThis->PacketPredicate(),
            usesLegacyMsFastLaunch,
            pDeCmdSpace);
    }

    pDeCmdSpace = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);

    pThis->m_state.flags.containsDrawIndirect = 1;

    // On Gfx9, the WD (Work distributor - breaks down draw commands into work groups which are sent to IA
    // units) has changed to having independent DMA and DRAW logic. As a result, DRAW_INDEX_AUTO commands have
    // added a dummy DMA command issued by the CP which overwrites the VGT_INDEX_TYPE register used by GFX. This
    // can cause hangs and rendering corruption with subsequent indexed draw commands. We must invalidate the
    // index type state so that it will be issued before the next indexed draw.
    pThis->m_drawTimeHwState.dirty.indexedIndexType = 1;
}

// =====================================================================================================================
// Generates commands required for execution of pipelines with both Task and Mesh shaders.
template <bool IssueSqttMarkerEvent, bool HasUavExport, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchMeshTask(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    auto* pThis     = static_cast<UniversalCmdBuffer*>(pCmdBuffer);
    Device* pDevice = const_cast<Device*>(&pThis->m_device);

    pThis->UpdateTaskMeshRingSize();

    CmdStream* pAceCmdStream = pThis->GetAceCmdStream();
    PAL_ASSERT(pAceCmdStream != nullptr);

    const gpusize gangedCmdStreamSemAddr = pThis->GangedCmdStreamSemAddr();

    PAL_ASSERT(static_cast<const Pipeline*>(pThis->m_graphicsState.pipelineState.pPipeline)->IsTaskShaderEnabled());
    const auto* pHybridPipeline =
        static_cast<const HybridGraphicsPipeline*>(pThis->m_graphicsState.pipelineState.pPipeline);
    const auto& taskSignature = pHybridPipeline->GetTaskSignature();

    pThis->CmdAceWaitDe();

    pThis->ValidateTaskMeshDispatch(0uLL, size);

    const uint16 taskDispatchDimsReg = taskSignature.taskDispatchDimsAddr;
    const uint16 taskRingIndexReg    = taskSignature.taskRingIndexAddr;
    PAL_ASSERT((taskRingIndexReg != UserDataNotMapped) && (taskDispatchDimsReg != UserDataNotMapped));

    pAceCmdStream->NotifyIndirectShRegWrite(taskRingIndexReg);

    uint32* pAceCmdSpace = pAceCmdStream->ReserveCommands();

    // We cannot pack these register writes into m_validUserEntryRegPairs without potentially overwriting unrelated
    // compute user data (PipelineBindPoint::Compute). The Task Shader employs graphics user data
    // (PipelineBindPoint::Graphics) written into the ACE command stream.
    // See ValidateTaskMeshDispatch() - graphics user data is copied into a temporary compute state and subsequently
    // written to m_validUserEntryRegPairs in ValidateDispatchPalAbi(), which must take precedence over the dispatch
    // dimensions.
    pAceCmdSpace = pAceCmdStream->WriteSetSeqShRegs(taskDispatchDimsReg,
                                                    taskDispatchDimsReg + 2,
                                                    ShaderCompute,
                                                    &size,
                                                    pAceCmdSpace);

    // Build the ACE direct dispatches.
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
                pAceCmdSpace  = pThis->BuildWriteViewId(viewInstancingDesc.viewId[i], pAceCmdSpace);

                if ((pThis->PacketPredicate() == PredEnable) && (pThis->m_predGpuAddr != 0))
                {
                    pAceCmdSpace += pThis->m_cmdUtil.BuildCondExec(pThis->m_predGpuAddr,
                                                                   CmdUtil::DispatchTaskMeshDirectMecSize,
                                                                   pAceCmdSpace);
                }
                pAceCmdSpace += CmdUtil::BuildDispatchTaskMeshDirectAce(size,
                                                                        taskRingIndexReg,
                                                                        pThis->PacketPredicate(),
                                                                        taskSignature.flags.isWave32,
                                                                        pAceCmdSpace);
            }
        }
    }
    else
    {
        if ((pThis->PacketPredicate() == PredEnable) && (pThis->m_predGpuAddr != 0))
        {
            pAceCmdSpace += pThis->m_cmdUtil.BuildCondExec(pThis->m_predGpuAddr,
                                                           CmdUtil::DispatchTaskMeshDirectMecSize,
                                                           pAceCmdSpace);
        }
        pAceCmdSpace += CmdUtil::BuildDispatchTaskMeshDirectAce(size,
                                                                taskRingIndexReg,
                                                                pThis->PacketPredicate(),
                                                                taskSignature.flags.isWave32,
                                                                pAceCmdSpace);
    }

    pAceCmdStream->CommitCommands(pAceCmdSpace);

    // Validate the draw after signaling the semaphore, so that register writes for validation can be overlapped with
    // the ACE engine launching the first task shader waves.
    Pm4::ValidateDrawInfo drawInfo = {};
    drawInfo.vtxIdxCount      = 0;
    drawInfo.instanceCount    = 0;
    drawInfo.firstVertex      = 0;
    drawInfo.firstInstance    = 0;
    drawInfo.firstIndex       = 0;
    drawInfo.useOpaque        = false;

    pThis->ValidateDraw<false, true>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDispatchMesh);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pThis->m_deCmdStream.NotifyIndirectShRegWrite(pThis->m_pSignatureGfx->meshDispatchDimsRegAddr);
    pThis->m_deCmdStream.NotifyIndirectShRegWrite(pThis->m_pSignatureGfx->meshRingIndexAddr);

    pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);

    const bool usesLegacyMsFastLaunch =
        (static_cast<GsFastLaunchMode>(pThis->m_state.flags.fastLaunchMode) == GsFastLaunchMode::VertInLane);

    // Build the GFX dispatches.
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
                pDeCmdSpace = pThis->BuildWriteViewId(viewInstancingDesc.viewId[i], pDeCmdSpace);
                pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchTaskMeshGfx<IssueSqttMarkerEvent>(
                    pThis->m_pSignatureGfx->meshDispatchDimsRegAddr,
                    pThis->m_pSignatureGfx->meshRingIndexAddr,
                    pThis->PacketPredicate(),
                    usesLegacyMsFastLaunch,
                    taskSignature.flags.isLinear,
                    pDeCmdSpace);
            }
        }
    }
    else
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchTaskMeshGfx<IssueSqttMarkerEvent>(
            pThis->m_pSignatureGfx->meshDispatchDimsRegAddr,
            pThis->m_pSignatureGfx->meshRingIndexAddr,
            pThis->PacketPredicate(),
            usesLegacyMsFastLaunch,
            taskSignature.flags.isLinear,
            pDeCmdSpace);
    }

    pDeCmdSpace = pThis->IncrementDeCounter(pDeCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                                 EngineTypeUniversal,
                                                                 pThis->PacketPredicate(),
                                                                 pDeCmdSpace);
    }

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);

    // The task shader workload uses the ImplicitAce. We set this flag here so it ensures proper reporting to the Queue
    // that a MultiQueue Gang submission will be needed for this CmdBuffer.
    pThis->EnableImplicitGangedSubQueueCount(1);

    // On Gfx9, we need to invalidate the index type which was previously programmed because the CP clobbers
    // that state when executing a non-indexed indirect draw.
    // SEE: CmdDraw() for more details about why we do this.
    pThis->m_drawTimeHwState.dirty.indexedIndexType = 1;
}

// =====================================================================================================================
// Indirect version of CmdDispatchMeshTask for execution of pipelines with both Task and Mesh shaders.
template <bool IssueSqttMarkerEvent, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchMeshIndirectMultiTask(
    ICmdBuffer*          pCmdBuffer,
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    auto*   pThis   = static_cast<UniversalCmdBuffer*>(pCmdBuffer);
    Device* pDevice = const_cast<Device*>(&pThis->m_device);

    gpusize gpuVirtAddr = gpuVirtAddrAndStride.gpuVirtAddr;
    uint32  stride      = static_cast<uint32>(gpuVirtAddrAndStride.stride);

    PAL_ASSERT(IsPow2Aligned(gpuVirtAddr, sizeof(uint32)));

    pThis->m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::PayloadData)] =
        Max<size_t>(pThis->m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::PayloadData)], 1);

    pThis->m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::TaskMeshCtrlDrawRing)] =
        Max<size_t>(pThis->m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::TaskMeshCtrlDrawRing)], 1);

    CmdStream* pAceCmdStream = pThis->GetAceCmdStream();
    PAL_ASSERT(pAceCmdStream != nullptr);

    const gpusize gangedCmdStreamSemAddr = pThis->GangedCmdStreamSemAddr();

    PAL_ASSERT(static_cast<const Pipeline*>(pThis->m_graphicsState.pipelineState.pPipeline)->IsTaskShaderEnabled());
    const auto* pHybridPipeline =
        static_cast<const HybridGraphicsPipeline*>(pThis->m_graphicsState.pipelineState.pPipeline);
    const auto& taskSignature = pHybridPipeline->GetTaskSignature();

    pThis->CmdAceWaitDe();

    pThis->ValidateTaskMeshDispatch(gpuVirtAddr, {});

    const uint16 taskDispatchDimsReg = taskSignature.taskDispatchDimsAddr;
    const uint16 taskRingIndexReg    = taskSignature.taskRingIndexAddr;
    const uint16 taskDispatchIdxReg  = taskSignature.dispatchIndexRegAddr;
    PAL_ASSERT((taskRingIndexReg != UserDataNotMapped) && (taskDispatchDimsReg != UserDataNotMapped));

    pAceCmdStream->NotifyIndirectShRegWrite(taskDispatchDimsReg);
    pAceCmdStream->NotifyIndirectShRegWrite(taskRingIndexReg);

    uint32* pAceCmdSpace = pAceCmdStream->ReserveCommands();

    // Build the ACE indirect dispatches.
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
                pAceCmdSpace  = pThis->BuildWriteViewId(viewInstancingDesc.viewId[i], pAceCmdSpace);

                if ((pThis->PacketPredicate() == PredEnable) && (pThis->m_predGpuAddr != 0))
                {
                    pAceCmdSpace += pThis->m_cmdUtil.BuildCondExec(pThis->m_predGpuAddr,
                                                                   CmdUtil::DispatchTaskMeshIndirectMecSize,
                                                                   pAceCmdSpace);
                }
                pAceCmdSpace += pThis->m_cmdUtil.BuildDispatchTaskMeshIndirectMultiAce<IssueSqttMarkerEvent>(
                    gpuVirtAddr,
                    taskRingIndexReg,
                    taskDispatchDimsReg,
                    taskDispatchIdxReg,
                    maximumCount,
                    stride,
                    countGpuAddr,
                    taskSignature.flags.isWave32,
                    pThis->PacketPredicate(),
                    pAceCmdSpace);
            }
        }
    }
    else
    {
        if ((pThis->PacketPredicate() == PredEnable) && (pThis->m_predGpuAddr != 0))
        {
            pAceCmdSpace += pThis->m_cmdUtil.BuildCondExec(pThis->m_predGpuAddr,
                                                            CmdUtil::DispatchTaskMeshIndirectMecSize,
                                                            pAceCmdSpace);
        }
        pAceCmdSpace += pThis->m_cmdUtil.BuildDispatchTaskMeshIndirectMultiAce<IssueSqttMarkerEvent>(
            gpuVirtAddr,
            taskRingIndexReg,
            taskDispatchDimsReg,
            taskDispatchIdxReg,
            maximumCount,
            stride,
            countGpuAddr,
            taskSignature.flags.isWave32,
            pThis->PacketPredicate(),
            pAceCmdSpace);
    }

    pAceCmdStream->CommitCommands(pAceCmdSpace);

    // Validate the draw after signaling the semaphore, so that register writes for validation can be overlapped with
    // the ACE engine launching the first task shader waves.
    Pm4::ValidateDrawInfo drawInfo = {};
    drawInfo.vtxIdxCount      = 0;
    drawInfo.instanceCount    = 0;
    drawInfo.firstVertex      = 0;
    drawInfo.firstInstance    = 0;
    drawInfo.firstIndex       = 0;
    drawInfo.useOpaque        = false;

    pThis->ValidateDraw<false, true>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDispatchMeshIndirectMulti);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pThis->m_deCmdStream.NotifyIndirectShRegWrite(pThis->m_pSignatureGfx->meshDispatchDimsRegAddr);
    pThis->m_deCmdStream.NotifyIndirectShRegWrite(pThis->m_pSignatureGfx->meshRingIndexAddr);

    pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);

    const bool usesLegacyMsFastLaunch =
        (static_cast<GsFastLaunchMode>(pThis->m_state.flags.fastLaunchMode) == GsFastLaunchMode::VertInLane);

    // Build the GFX dispatches.
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
                pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchTaskMeshGfx<IssueSqttMarkerEvent>(
                    pThis->m_pSignatureGfx->meshDispatchDimsRegAddr,
                    pThis->m_pSignatureGfx->meshRingIndexAddr,
                    pThis->PacketPredicate(),
                    usesLegacyMsFastLaunch,
                    taskSignature.flags.isLinear,
                    pDeCmdSpace);
            }
        }
    }
    else
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchTaskMeshGfx<IssueSqttMarkerEvent>(
            pThis->m_pSignatureGfx->meshDispatchDimsRegAddr,
            pThis->m_pSignatureGfx->meshRingIndexAddr,
            pThis->PacketPredicate(),
            usesLegacyMsFastLaunch,
            taskSignature.flags.isLinear,
            pDeCmdSpace);
    }

    pDeCmdSpace = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);

    // The task shader workload uses the ImplicitAce. We set this flag here so it ensures proper reporting to the Queue
    // that a MultiQueue Gang submission will be needed for this CmdBuffer.
    pThis->EnableImplicitGangedSubQueueCount(1);

    // On Gfx9, we need to invalidate the index type which was previously programmed because the CP clobbers
    // that state when executing a non-indexed indirect draw.
    // SEE: CmdDraw() for more details about why we do this.
    pThis->m_drawTimeHwState.dirty.indexedIndexType = 1;
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
void UniversalCmdBuffer::CmdWriteTimestamp(
    uint32            stageMask,    // Bitmask of PipelineStageFlag
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    const gpusize address = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    // If multiple flags are set we must go down the path that is most conservative (writes at the latest point).
    // This is easiest to implement in this order:
    // 1. All non-CP stages must fall back to an EOP timestamp.
    // 2. The CP stages can write the value directly using COPY_DATA in the ME. (PFP doesn't support gpu_clock_count?)
    // Note that passing in a stageMask of zero will get you an ME write. It's not clear if that is even legal but
    // doing an ME write is probably the least impactful thing we could do in that case.
    if (TestAnyFlagSet(stageMask, EopWaitStageMask | VsWaitStageMask | PsWaitStageMask | CsWaitStageMask))
    {
        ReleaseMemGfx releaseInfo = {};
        releaseInfo.vgtEvent = BOTTOM_OF_PIPE_TS;
        releaseInfo.dstAddr  = address;
        releaseInfo.dataSel  = data_sel__me_release_mem__send_gpu_clock_counter;

        pDeCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseInfo, pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace += m_cmdUtil.BuildCopyData(EngineTypeUniversal,
                                               engine_sel__me_copy_data__micro_engine,
                                               dst_sel__me_copy_data__tc_l2_obsolete,
                                               address,
                                               src_sel__me_copy_data__gpu_clock_count,
                                               0,
                                               count_sel__me_copy_data__64_bits_of_data,
                                               wr_confirm__me_copy_data__wait_for_confirmation,
                                               pDeCmdSpace);
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdWriteImmediate(
    uint32             stageMask, // Bitmask of PipelineStageFlag
    uint64             data,
    ImmediateDataWidth dataSize,
    gpusize            address)
{
    const bool is32Bit = (dataSize == ImmediateDataWidth::ImmediateData32Bit);

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    // If multiple flags are set we must go down the path that is most conservative (writes at the latest point).
    // This is easiest to implement in this order:
    // 1. All non-CP stages must fall back to an EOP timestamp.
    // 2. The CP stages can write the value directly using COPY_DATA, taking care to select the PFP or ME.
    // Note that passing in a stageMask of zero will get you an ME write. It's not clear if that is even legal but
    // doing an ME write is probably the least impactful thing we could do in that case.
    if (TestAnyFlagSet(stageMask, EopWaitStageMask | VsWaitStageMask | PsWaitStageMask | CsWaitStageMask))
    {
        ReleaseMemGfx releaseInfo = {};
        releaseInfo.vgtEvent = BOTTOM_OF_PIPE_TS;
        releaseInfo.dstAddr  = address;
        releaseInfo.data     = data;
        releaseInfo.dataSel  = is32Bit ? data_sel__me_release_mem__send_32_bit_low
                                       : data_sel__me_release_mem__send_64_bit_data;

        pDeCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseInfo, pDeCmdSpace);
    }
    else
    {
        const bool pfpWait = TestAnyFlagSet(stageMask, PipelineStageTopOfPipe | PipelineStageFetchIndirectArgs);

        pDeCmdSpace += m_cmdUtil.BuildCopyData(EngineTypeUniversal,
                                               pfpWait ? engine_sel__pfp_copy_data__prefetch_parser
                                                       : engine_sel__me_copy_data__micro_engine,
                                               dst_sel__me_copy_data__tc_l2_obsolete,
                                               address,
                                               src_sel__me_copy_data__immediate_data,
                                               data,
                                               is32Bit ? count_sel__me_copy_data__32_bits_of_data
                                                       : count_sel__me_copy_data__64_bits_of_data,
                                               wr_confirm__me_copy_data__wait_for_confirmation,
                                               pDeCmdSpace);
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
        const auto*const pNewPalette = static_cast<const BorderColorPalette*>(pPalette);

        {
            if (pNewPalette != nullptr)
            {
                uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
                pDeCmdSpace = pNewPalette->WriteCommands(pipelineBindPoint,
                                                         TimestampGpuVirtAddr(),
                                                         &m_deCmdStream,
                                                         pDeCmdSpace);
                m_deCmdStream.CommitCommands(pDeCmdSpace);
            }

            auto*const pPipelineState = (pipelineBindPoint == PipelineBindPoint::Compute)
                    ? &m_computeState.pipelineState
                    : &m_graphicsState.pipelineState;
            pPipelineState->pBorderColorPalette = pNewPalette;
            pPipelineState->dirtyFlags.borderColorPalette = 1;
        }
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdInsertTraceMarker(
    PerfTraceMarkerType markerType,
    uint32              markerData)
{
    const uint32 userDataAddr =
        (markerType == PerfTraceMarkerType::SqttA) ? mmSQ_THREAD_TRACE_USERDATA_2 : mmSQ_THREAD_TRACE_USERDATA_3;

    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
    pCmdSpace = m_deCmdStream.WriteSetOneConfigReg<true>(userDataAddr, markerData, pCmdSpace);
    m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdInsertRgpTraceMarker(
    RgpMarkerSubQueueFlags subQueueFlags,
    uint32                 numDwords,
    const void*            pData)
{
    PAL_ASSERT(subQueueFlags.u32All != 0);

    // The first dword of every RGP trace marker packet is written to SQ_THREAD_TRACE_USERDATA_2.  The second dword
    // is written to SQ_THREAD_TRACE_USERDATA_3.  For packets longer than 64-bits, continue alternating between
    // user data 2 and 3.
    static_assert(mmSQ_THREAD_TRACE_USERDATA_3 == mmSQ_THREAD_TRACE_USERDATA_2 + 1, "Registers not sequential!");

    const uint32* pDwordData = static_cast<const uint32*>(pData);
    while (numDwords > 0)
    {
        const uint32 dwordsToWrite = Min(numDwords, 2u);

        constexpr uint16 Start = mmSQ_THREAD_TRACE_USERDATA_2;
        const uint16 end = (Start + dwordsToWrite - 1);

        // Reserve and commit command space inside this loop.  Some of the RGP packets are unbounded, like adding a
        // comment string, so it's not safe to assume the whole packet will fit under our reserve limit.
        if (subQueueFlags.includeMainSubQueue != 0)
        {
            uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
            pCmdSpace = m_deCmdStream.WriteSetSeqConfigRegs<true>(Start, end, pDwordData, pCmdSpace);
            m_deCmdStream.CommitCommands(pCmdSpace);
        }

        if (subQueueFlags.includeGangedSubQueues != 0)
        {
            PAL_ASSERT(ImplicitGangedSubQueueCount() == 1);

            CmdStream*const pAceCmdStream = GetAceCmdStream();
            uint32* pCmdSpace = pAceCmdStream->ReserveCommands();
            pCmdSpace = pAceCmdStream->WriteSetSeqConfigRegs(Start, end, pDwordData, pCmdSpace);
            pAceCmdStream->CommitCommands(pCmdSpace);
        }

        pDwordData += dwordsToWrite;
        numDwords  -= dwordsToWrite;
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

    regDB_RENDER_CONTROL dbRenderControl = { };

    if (IsGfx11(m_gfxIpLevel) && m_cachedSettings.useLegacyDbZInfo)
    {
        // When the PA_SC_VRS_SURFACE_CNTL_1.DISABLE_SSAA_DETAIL_TO_EXPOSED_RATE_CLAMPING setting is
        // zero -- and it always is since this is a config register / chicken bit -- then the VRS rate
        // is ultimately clamped against the smaller of
        //      a) DB_Z_INFO.NUM_SAMPLES
        //      b) PA_SC_AA_CONFIG.MSAA_EXPOSED_SAMPLES
        //
        // Note that the HW intentionally looks at DB_Z_INFO.NUM_SAMPLES even if there is no bound depth buffer.
        //
        // The latter is properly setup based on the actual MSAA rate, but if there's no depth buffer
        // (i.e., this case), then we need to ensure that the DB_Z_INFO.NUM_SAMPLE is *not* the constraining
        // factor.
        regs2.dbZInfo.bits.NUM_SAMPLES = 3;
    }

    pCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmDB_Z_INFO,
                                                     mmDB_STENCIL_INFO,
                                                     &regs2,
                                                     pCmdSpace);

    if (m_cachedSettings.supportsVrs)
    {
        if (IsGfx10(m_gfxIpLevel))
        {
            // If no depth buffer has been bound yet, then make sure we obey the panel setting.  This has an
            // effect even if depth testing is disabled.
            regs1.dbRenderOverride2.gfx10Vrs.FORCE_VRS_RATE_FINE = (m_cachedSettings.vrsForceRateFine ? 1 : 0);
        }

        if (IsGfx103Plus(m_gfxIpLevel))
        {
            //   For centroid computation you need to set DB_RENDER_OVERRIDE2::CENTROID_COMPUTATION_MODE to pick
            //   correct sample for centroid, which per DX12 spec is defined as the first covered sample. This
            //   means that it should use "2: Choose the sample with the smallest {~pixel_num, sample_id} as
            //   centroid, for all VRS rates"
            regs1.dbRenderOverride2.gfx103Plus.CENTROID_COMPUTATION_MODE = 2;
        }
    }

    if (IsGfx11(m_gfxIpLevel))
    {
        Gfx10DepthStencilView::SetGfx11StaticDbRenderControlFields(m_device, 1, &dbRenderControl);
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
    for (uint32 slot : BitIter32(newNullSlotMask))
    {
        static_assert((COLOR_INVALID == 0), "COLOR_INVALID != 0");

        // Zero out all the RTV owned fields of CB_COLOR_INFO.
        BitfieldUpdateSubfield(&(m_cbColorInfo[slot].u32All), 0u, ColorTargetView::CbColorInfoMask);

    }
    m_state.flags.cbColorInfoDirtyRtv |= newNullSlotMask;
}

// =====================================================================================================================
// Validates and writes tessellation distribution factors
uint32* UniversalCmdBuffer::WriteTessDistributionFactors(
    uint32* pDeCmdSpace)
{
    // Confirm equivalence b/w the two unions assuming each bitfield compared is the same size (8, 8, 8, 5, and 3 bits).
    constexpr regVGT_TESS_DISTRIBUTION RegCheck    = { 255, 255, 255, 31, 7 };
    constexpr TessDistributionFactors  StructCheck = { 255, 255, 255, 31, 7 };
    static_assert((RegCheck.bits.ACCUM_ISOLINE == StructCheck.isoDistributionFactor),
                  "ACCUM_ISOLINE and isoDistributionFactor do not match!");
    static_assert((RegCheck.bits.ACCUM_TRI == StructCheck.triDistributionFactor),
                  "ACCUM_TRI and triDistributionFactor do not match!");
    static_assert((RegCheck.bits.ACCUM_QUAD == StructCheck.quadDistributionFactor),
                  "ACCUM_QUAD and quadDistributionFactor do not match!");
    static_assert((RegCheck.bits.DONUT_SPLIT == StructCheck.donutDistributionFactor),
                  "DONUT_SPLIT and donutDistributionFactor do not match!");
    static_assert((RegCheck.bits.TRAP_SPLIT == StructCheck.trapDistributionFactor),
                  "TRAP_SPLIT and trapDistributionFactor do not match!");
    static_assert((sizeof(RegCheck) == sizeof(StructCheck)),
                  "TessDistributionFactors and regVGT_TESS_DISTRIBUTION sizes do not match!");

    if (m_cachedSettings.waClampQuadDistributionFactor)
    {
        // VGT_TESS_DISTRIBUTION.ACCUM_QUAD should never be allowed to exceed 64
        m_tessDistributionFactors.quadDistributionFactor = Min(m_tessDistributionFactors.quadDistributionFactor, 64u);
    }

    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmVGT_TESS_DISTRIBUTION,
                                                      m_tessDistributionFactors.u32All,
                                                      pDeCmdSpace);

    return pDeCmdSpace;
}

// =====================================================================================================================
// Adds a preamble to the start of a new command buffer.
Result UniversalCmdBuffer::AddPreamble()
{
    const auto& cmdUtil  = m_device.CmdUtil();
    const auto& device   = *(m_device.Parent());
    const bool  isNested = IsNested();

    // If this trips, it means that this isn't really the preamble -- i.e., somebody has inserted something into the
    // command stream before the preamble.  :-(
    PAL_ASSERT(m_ceCmdStream.IsEmpty());
    PAL_ASSERT(m_deCmdStream.IsEmpty());

    // DB_RENDER_OVERRIDE bits are updated via depth-stencil view and at draw time validation based on dirty
    // depth-stencil state.
    m_dbRenderOverride.u32All = 0;
    if (m_cachedSettings.hiDepthDisabled != 0)
    {
        m_dbRenderOverride.bits.FORCE_HIZ_ENABLE = FORCE_DISABLE;
    }
    if (m_cachedSettings.hiStencilDisabled != 0)
    {
        m_dbRenderOverride.bits.FORCE_HIS_ENABLE0 = FORCE_DISABLE;
        m_dbRenderOverride.bits.FORCE_HIS_ENABLE1 = FORCE_DISABLE;
    }

    {
        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

        if (m_cachedSettings.disablePreamblePipelineStats == false)
        {
            pDeCmdSpace += cmdUtil.BuildNonSampleEventWrite(PIPELINESTAT_START, EngineTypeUniversal, pDeCmdSpace);
        }

        if (isNested == false)
        {
            pDeCmdSpace =
                m_deCmdStream.WriteSetOneContextReg(mmDB_RENDER_OVERRIDE, m_dbRenderOverride.u32All, pDeCmdSpace);
            m_prevDbRenderOverride.u32All = m_dbRenderOverride.u32All;
        }

        // The draw-time validation will get confused unless we set PA_SC_AA_CONFIG to a known last value.
        pDeCmdSpace =
            m_deCmdStream.WriteSetOneContextRegNoOpt(mmPA_SC_AA_CONFIG, m_paScAaConfigLast.u32All, pDeCmdSpace);

        // Set patch and donut distribution thresholds for tessellation.
        pDeCmdSpace = WriteTessDistributionFactors(pDeCmdSpace);

        if (isNested)
        {
            // Clear out the blend optimizations explicitly here as the chained command buffers don't have a way to
            // check inherited state and the optimizations won't be cleared unless cleared in this command buffer.
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

        // PA_SC_CONSERVATIVE_RASTERIZATION_CNTL is the same value for most Pipeline objects. Prime it in the Preamble
        // to the disabled state. At draw-time, we check if a new value is needed based on (Pipeline || MSAA) being
        // dirty. It is expected that Pipeline and MSAA is always known even on nested command buffers.
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmPA_SC_CONSERVATIVE_RASTERIZATION_CNTL,
                                                               m_paScConsRastCntl.u32All,
                                                               pDeCmdSpace);

        // Initialize VGT_LS_HS_CONFIG. It will be rewritten at draw-time if its value changes.
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmVGT_LS_HS_CONFIG, m_vgtLsHsConfig.u32All, pDeCmdSpace);

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

        pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmPA_SC_BINNER_CNTL_0,
                                                           mmPA_SC_BINNER_CNTL_1,
                                                           &m_pbbCntlRegs,
                                                           pDeCmdSpace);

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
            PAL_ASSERT(IsGfx11(m_gfxIpLevel) == false);

            // Force off DFSM.
            regDB_DFSM_CONTROL dbDfsmControl = {};
            dbDfsmControl.bits.PUNCHOUT_MODE = DfsmPunchoutModeForceOff;

            // Note that waStalledPopsMode prevents us from setting POPS_DRAIN_PS_ON_OVERLAP.
            PAL_ASSERT((dbDfsmControl.bits.POPS_DRAIN_PS_ON_OVERLAP == 0) ||
                       (m_device.Settings().waStalledPopsMode == false));

            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(m_cmdUtil.GetRegInfo().mmDbDfsmControl,
                                                              dbDfsmControl.u32All,
                                                              pDeCmdSpace);
        }

        // Initialize m_acqRelFenceValGpuVa.
        if (AcqRelFenceValBaseGpuVa() != 0)
        {
            const uint32 data[static_cast<uint32>(AcqRelEventType::Count)] = {};

            WriteDataInfo writeDataInfo = { };
            writeDataInfo.engineType = m_engineType;
            writeDataInfo.engineSel  = engine_sel__pfp_write_data__prefetch_parser;
            writeDataInfo.dstSel     = dst_sel__pfp_write_data__memory;
            writeDataInfo.dstAddr    = AcqRelFenceValBaseGpuVa();

            pDeCmdSpace += CmdUtil::BuildWriteData(writeDataInfo,
                                                   (sizeof(data) / sizeof(uint32)),
                                                   reinterpret_cast<const uint32*>(&data),
                                                   pDeCmdSpace);
        }

        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }

    // Clients may not bind a PointLineRasterState until they intend to do wireframe rendering. This means that the
    // wireframe tosspoint may render a bunch of zero-width lines (i.e. nothing) until that state is bound. When that
    // tosspoint is enabled we should bind some default state to be sure that we will see some lines.
    //
    // This is not desirable for nested command buffers as we can rely on the state from the parent.
    // By skipping this bind on nested, we can maintain any state set by the client.
    if ((static_cast<TossPointMode>(m_cachedSettings.tossPointMode) == TossPointWireframe) &&
        (IsNested() == false))
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

    if (IsOneTimeSubmit() == false)
    {
        // If the memory contains any value, it is possible that with the ACE running ahead, it could get a value
        // for this semaphore which is >= the number it is waiting for and then just continue ahead before GFX has
        // a chance to write it to 0. The vice versa case could happen for "GFX waiting for ACE" semaphore as well.
        // To handle the case where we reuse a command buffer entirely, we'll have to perform a GPU-side write of this
        // memory in the postamble.
        constexpr uint32 SemZero = 0;

        WriteDataInfo writeData { };
        writeData.engineType    = GetEngineType();
        writeData.engineSel     = engine_sel__me_write_data__micro_engine;
        writeData.dstSel        = dst_sel__pfp_write_data__memory;

        if (m_gangedCmdStreamSemAddr != 0)
        {
            writeData.dstAddr = m_gangedCmdStreamSemAddr;
            pDeCmdSpace      += CmdUtil::BuildWriteData(writeData, 1, &SemZero, pDeCmdSpace);

            writeData.dstAddr = m_gangedCmdStreamSemAddr + sizeof(uint32);
            pDeCmdSpace      += CmdUtil::BuildWriteData(writeData, 1, &SemZero, pDeCmdSpace);
        }

    }

    if (m_pm4CmdBufState.flags.cpBltActive && (IsNested() == false))
    {
        // Stalls the CP ME until the CP's DMA engine has finished all previous "CP blts" (DMA_DATA commands
        // without the sync bit set). The ring won't wait for CP DMAs to finish so we need to do this manually.
        pDeCmdSpace += CmdUtil::BuildWaitDmaData(pDeCmdSpace);
        SetCpBltState(false);
    }

    bool didWaitForIdle = false;

    if ((m_pAceCmdStream != nullptr) &&
        (m_pAceCmdStream->GetNumChunks() > 0) &&
        (m_pAceCmdStream->GetFirstChunk()->BusyTrackerGpuAddr() != 0))
    {
        // The following ATOMIC_MEM packet increments the done-count for the ACE command streams, so that we can probe
        // when the command buffer has completed execution on the GPU.
        // NOTE: Normally, we would need to flush the L2 cache to guarantee that this memory operation makes it out
        // to memory.  However, since we're at the end of the command buffer, we can rely on the fact that the KMD
        // inserts an EOP event which flushes and invalidates the caches in between command buffers.
        pDeCmdSpace += CmdUtil::BuildAtomicMem(AtomicOp::AddInt32,
                                               m_pAceCmdStream->GetFirstChunk()->BusyTrackerGpuAddr(),
                                               1,
                                               pDeCmdSpace);
    }

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
        pDeCmdSpace = WriteWaitEop(HwPipePostPrefetch, false, SyncGlxNone, SyncRbNone, pDeCmdSpace);

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
            pDeCmdSpace = WriteWaitEop(HwPipePostPrefetch, false, SyncGlxNone, SyncRbNone, pDeCmdSpace);
        }

        pDeCmdSpace += CmdUtil::BuildAtomicMem(AtomicOp::AddInt32,
                                               m_deCmdStream.GetFirstChunk()->BusyTrackerGpuAddr(),
                                               1,
                                               pDeCmdSpace);
    }

    if (m_cachedSettings.waAddPostambleEvent && (IsNested() == false))
    {
        // If the last draw was a tessellation draw with shader messages enabled on the last threadgroup, then a hang
        // will occur. The conditions for this to happen are seeing a regular threadgroup (with tf factors fetched)
        // proceeded by a threadgroup with the shader message (all_tf0/all_tf1) set and seeing no other threadgroup
        // or event after.
        //
        // The workaround is to add an event (such as SQ_NON_EVENT) at the end of the last command buffer to help flush
        // the pipe.
        pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(SQ_NON_EVENT, GetEngineType(), pDeCmdSpace);
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);

#if PAL_DEVELOPER_BUILD
    if (m_cachedSettings.enablePm4Instrumentation)
    {
        m_deCmdStream.IssueHotRegisterReport(this);
    }
#endif

    return Result::Success;
}

// =====================================================================================================================
// Adds commands necessary to write "data" to the specified memory
void UniversalCmdBuffer::WriteEventCmd(
    const BoundGpuMemory& boundMemObj,
    uint32                stageMask,   // Bitmask of PipelineStageFlag
    uint32                data)
{
    // This will replace PipelineStageBlt with a more specific set of flags if we haven't done any CP DMAs.
    uint32 unusedStageMask = 0;
    m_pBarrierMgr->OptimizeStageMask(this, BarrierType::Global, &stageMask, &unusedStageMask);

    uint32* pDeCmdSpace         = m_deCmdStream.ReserveCommands();
    bool    releaseMemWaitCpDma = false;

    if (TestAnyFlagSet(stageMask, PipelineStageBlt | PipelineStageBottomOfPipe) && m_pm4CmdBufState.flags.cpBltActive)
    {
        // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
        // the CmdSetEvent and CmdResetEvent functions expect that the prior blts have completed by the time the event
        // is written to memory. Given that our CP DMA blts are asynchronous to the pipeline stages the only way to
        // satisfy this requirement is to force the ME to stall until the CP DMAs are completed.
        if (m_device.EnableReleaseMemWaitCpDma() &&
            TestAnyFlagSet(stageMask, EopWaitStageMask | VsWaitStageMask | PsWaitStageMask | CsWaitStageMask))
        {
            releaseMemWaitCpDma = true;
        }
        else
        {
            pDeCmdSpace += CmdUtil::BuildWaitDmaData(pDeCmdSpace);
        }
        SetCpBltState(false);
    }

    // Now pick the packet that actually writes to the event. If multiple flags are set we must go down the path that
    // is most conservative (sets the event at the latest point). This is easiest to implement in this order:
    // 1. The EOS events can wait for one and only one stage. We should check for "only PS" or "only CS" first.
    // 2. Otherwise, all non-CP stages must fall back to an EOP timestamp. We'll go down this path if multiple EOS
    //    stages are specified in the same call and/or any stages that can only be waited on using an EOP timestamp.
    // 3. If no EOS or EOP stages were specified it must be safe to just to a direct write using the PFP or ME.
    // Note that passing in a stageMask of zero will get you an ME write. It's not clear if that is even legal but
    // doing an ME write is probably the least impactful thing we could do in that case.
    if ((stageMask == PipelineStagePs) || (stageMask == PipelineStageCs))
    {
        ReleaseMemGfx releaseInfo = {};
        releaseInfo.dstAddr        = boundMemObj.GpuVirtAddr();
        releaseInfo.dataSel        = data_sel__me_release_mem__send_32_bit_low;
        releaseInfo.data           = data;
        releaseInfo.vgtEvent       = (stageMask == PipelineStagePs) ? PS_DONE : CS_DONE;
        releaseInfo.gfx11WaitCpDma = releaseMemWaitCpDma;

        pDeCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseInfo, pDeCmdSpace);
    }
    else if (TestAnyFlagSet(stageMask, EopWaitStageMask | VsWaitStageMask | PsWaitStageMask | CsWaitStageMask))
    {
        ReleaseMemGfx releaseInfo  = {};
        releaseInfo.dstAddr        = boundMemObj.GpuVirtAddr();
        releaseInfo.dataSel        = data_sel__me_release_mem__send_32_bit_low;
        releaseInfo.data           = data;
        releaseInfo.vgtEvent       = BOTTOM_OF_PIPE_TS;
        releaseInfo.gfx11WaitCpDma = releaseMemWaitCpDma;

        pDeCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseInfo, pDeCmdSpace);
    }
    else
    {
        const bool pfpWait = TestAnyFlagSet(stageMask, PipelineStageTopOfPipe | PipelineStageFetchIndirectArgs);

        WriteDataInfo writeData = {};
        writeData.engineType = GetEngineType();
        writeData.dstAddr    = boundMemObj.GpuVirtAddr();
        writeData.dstSel     = dst_sel__me_write_data__memory;
        writeData.engineSel  = pfpWait ? uint32(engine_sel__pfp_write_data__prefetch_parser)
                                       : uint32(engine_sel__me_write_data__micro_engine);

        pDeCmdSpace += CmdUtil::BuildWriteData(writeData, data, pDeCmdSpace);
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Gets the command stream associated with the specified engine
CmdStream* UniversalCmdBuffer::GetCmdStreamByEngine(
    CmdBufferEngineSupport engineType) // Mask of Engine types as defined in gfxCmdBufer.h
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
        auto*const pDumpCeRam        = reinterpret_cast<PM4_CE_DUMP_CONST_RAM*>(m_state.pLastDumpCeRam);
        pDumpCeRam->ordinal2.u32All  = m_state.lastDumpCeRamOrdinal2.u32All;

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
// Helper function responsible for handling user-SGPR updates during Draw-time validation when the active pipeline has
// changed since the previous Draw operation. It is expected that this will be called only when the pipeline is changing
// and immediately before a call to WriteDirtyUserDataEntriesToSgprsGfx().
// Returns a mask of which hardware shader stages' user-data mappings have changed.
template <bool TessEnabled, bool GsEnabled, bool VsEnabled>
uint8 UniversalCmdBuffer::FixupUserSgprsOnPipelineSwitch(
    const UserDataEntries*           pUserDataEntries,
    const GraphicsPipelineSignature* pPrevSignature,
    uint32**                         ppDeCmdSpace)
{
    PAL_ASSERT(pPrevSignature != nullptr);

    // The WriteDirtyUserDataEntriesToSgprs() method only writes entries which are mapped to user-SGPR's and have
    // been marked dirty.  When the active pipeline is changing, the set of entries mapped to user-SGPR's can change
    // per shader stage, and which entries are mapped to which registers can also change.  The simplest way to handle
    // this is to write all mapped user-SGPR's for any stage whose mappings are changing.  Any stage whose mappings
    // are not changing will be handled through the normal "pipeline not changing" path.
    uint8 changedStageMask = 0; // Mask of all stages whose mappings are changing.

    if (m_cachedSettings.supportsShPairsPacket)
    {
        PAL_ASSERT(IsGfx11(m_gfxIpLevel));
        if (TessEnabled && (m_pSignatureGfx->userDataHash[HsStageId] != pPrevSignature->userDataHash[HsStageId]))
        {
            changedStageMask |= (1 << HsStageId);
            CmdStream::AccumulateUserDataEntriesForSgprs<true>(m_pSignatureGfx->stage[HsStageId],
                                                               *pUserDataEntries,
                                                               m_baseUserDataReg[HwShaderStage::Hs],
                                                               m_validUserEntryRegPairs,
                                                               &m_validUserEntryRegPairsLookup[LookupIndexHs],
                                                               m_minValidUserEntryLookupValue,
                                                               &m_numValidUserEntries);
        }
        if (GsEnabled && (m_pSignatureGfx->userDataHash[GsStageId] != pPrevSignature->userDataHash[GsStageId]))
        {
            changedStageMask |= (1 << GsStageId);
            CmdStream::AccumulateUserDataEntriesForSgprs<true>(m_pSignatureGfx->stage[GsStageId],
                                                               *pUserDataEntries,
                                                               m_baseUserDataReg[HwShaderStage::Gs],
                                                               m_validUserEntryRegPairs,
                                                               &m_validUserEntryRegPairsLookup[LookupIndexGs],
                                                               m_minValidUserEntryLookupValue,
                                                               &m_numValidUserEntries);
        }
        if (m_pSignatureGfx->userDataHash[PsStageId] != pPrevSignature->userDataHash[PsStageId])
        {
            changedStageMask |= (1 << PsStageId);
            CmdStream::AccumulateUserDataEntriesForSgprs<true>(m_pSignatureGfx->stage[PsStageId],
                                                               *pUserDataEntries,
                                                               m_baseUserDataReg[HwShaderStage::Ps],
                                                               m_validUserEntryRegPairs,
                                                               &m_validUserEntryRegPairsLookup[LookupIndexPs],
                                                               m_minValidUserEntryLookupValue,
                                                               &m_numValidUserEntries);
        }
    }
    else
    {
        uint32* pDeCmdSpace = (*ppDeCmdSpace);

        if (TessEnabled && (m_pSignatureGfx->userDataHash[HsStageId] != pPrevSignature->userDataHash[HsStageId]))
        {
            changedStageMask |= (1 << HsStageId);
            pDeCmdSpace =
                m_deCmdStream.WriteUserDataEntriesToSgprs<true, ShaderGraphics>(m_pSignatureGfx->stage[HsStageId],
                                                                                *pUserDataEntries,
                                                                                pDeCmdSpace);
        }
        if (GsEnabled && (m_pSignatureGfx->userDataHash[GsStageId] != pPrevSignature->userDataHash[GsStageId]))
        {
            changedStageMask |= (1 << GsStageId);
            pDeCmdSpace =
                m_deCmdStream.WriteUserDataEntriesToSgprs<true, ShaderGraphics>(m_pSignatureGfx->stage[GsStageId],
                                                                                *pUserDataEntries,
                                                                                pDeCmdSpace);
        }
        if (VsEnabled && (m_pSignatureGfx->userDataHash[VsStageId] != pPrevSignature->userDataHash[VsStageId]))
        {
            changedStageMask |= (1 << VsStageId);
            pDeCmdSpace =
                m_deCmdStream.WriteUserDataEntriesToSgprs<true, ShaderGraphics>(m_pSignatureGfx->stage[VsStageId],
                                                                                *pUserDataEntries,
                                                                                pDeCmdSpace);
        }
        if (m_pSignatureGfx->userDataHash[PsStageId] != pPrevSignature->userDataHash[PsStageId])
        {
            changedStageMask |= (1 << PsStageId);
            pDeCmdSpace =
                m_deCmdStream.WriteUserDataEntriesToSgprs<true, ShaderGraphics>(m_pSignatureGfx->stage[PsStageId],
                                                                                *pUserDataEntries,
                                                                                pDeCmdSpace);
        }

        (*ppDeCmdSpace) = pDeCmdSpace;
    }

    return changedStageMask;
}

// =====================================================================================================================
// Helper function responsible for writing all dirty graphics user-data entries to their respective user-SGPR's. Does
// not do anything with entries which are mapped to the spill table.
template <bool TessEnabled, bool GsEnabled, bool VsEnabled>
uint32* UniversalCmdBuffer::WriteDirtyUserDataEntriesToSgprsGfx(
    const UserDataEntries*           pUserDataEntries,
    const GraphicsPipelineSignature* pPrevSignature,
    uint8                            alreadyWrittenStageMask,
    uint32*                          pDeCmdSpace)
{
    constexpr uint8 ActiveStageMask = ((TessEnabled ? (1 << HsStageId) : 0) |
                                       (GsEnabled   ? (1 << GsStageId) : 0) |
                                       (VsEnabled   ? (1 << VsStageId) : 0) |
                                                      (1 << PsStageId));
    const uint8 dirtyStageMask      = ((~alreadyWrittenStageMask) & ActiveStageMask);

    if (dirtyStageMask)
    {
        if (m_cachedSettings.supportsShPairsPacket)
        {
            PAL_DEBUG_BUILD_ONLY_ASSERT(IsGfx11(m_gfxIpLevel));

            if (TessEnabled && (dirtyStageMask & (1 << HsStageId)))
            {
                CmdStream::AccumulateUserDataEntriesForSgprs<false>(m_pSignatureGfx->stage[HsStageId],
                                                                    *pUserDataEntries,
                                                                    m_baseUserDataReg[HwShaderStage::Hs],
                                                                    m_validUserEntryRegPairs,
                                                                    &m_validUserEntryRegPairsLookup[LookupIndexHs],
                                                                    m_minValidUserEntryLookupValue,
                                                                    &m_numValidUserEntries);
            }
            if (GsEnabled && (dirtyStageMask & (1 << GsStageId)))
            {
                CmdStream::AccumulateUserDataEntriesForSgprs<false>(m_pSignatureGfx->stage[GsStageId],
                                                                    *pUserDataEntries,
                                                                    m_baseUserDataReg[HwShaderStage::Gs],
                                                                    m_validUserEntryRegPairs,
                                                                    &m_validUserEntryRegPairsLookup[LookupIndexGs],
                                                                    m_minValidUserEntryLookupValue,
                                                                    &m_numValidUserEntries);
            }
            PAL_DEBUG_BUILD_ONLY_ASSERT((VsEnabled == false) && ((dirtyStageMask & (1 << VsStageId)) == 0));
            if (dirtyStageMask & (1 << PsStageId))
            {
                CmdStream::AccumulateUserDataEntriesForSgprs<false>(m_pSignatureGfx->stage[PsStageId],
                                                                    *pUserDataEntries,
                                                                    m_baseUserDataReg[HwShaderStage::Ps],
                                                                    m_validUserEntryRegPairs,
                                                                    &m_validUserEntryRegPairsLookup[LookupIndexPs],
                                                                    m_minValidUserEntryLookupValue,
                                                                    &m_numValidUserEntries);
            }
        }
        else
        {
            if (TessEnabled && (dirtyStageMask & (1 << HsStageId)))
            {
                pDeCmdSpace =
                    m_deCmdStream.WriteUserDataEntriesToSgprs<false, ShaderGraphics>(m_pSignatureGfx->stage[HsStageId],
                                                                                    *pUserDataEntries,
                                                                                     pDeCmdSpace);
            }
            if (GsEnabled && (dirtyStageMask & (1 << GsStageId)))
            {
                pDeCmdSpace =
                    m_deCmdStream.WriteUserDataEntriesToSgprs<false, ShaderGraphics>(m_pSignatureGfx->stage[GsStageId],
                                                                                    *pUserDataEntries,
                                                                                     pDeCmdSpace);
            }
            if (VsEnabled && (dirtyStageMask & (1 << VsStageId)))
            {
                pDeCmdSpace =
                    m_deCmdStream.WriteUserDataEntriesToSgprs<false, ShaderGraphics>(m_pSignatureGfx->stage[VsStageId],
                                                                                    *pUserDataEntries,
                                                                                     pDeCmdSpace);
            }
            if (dirtyStageMask & (1 << PsStageId))
            {
                pDeCmdSpace =
                    m_deCmdStream.WriteUserDataEntriesToSgprs<false, ShaderGraphics>(m_pSignatureGfx->stage[PsStageId],
                                                                                    *pUserDataEntries,
                                                                                     pDeCmdSpace);
            }
        }
    } // if any stages still need dirty state processing

    return pDeCmdSpace;
}

// =====================================================================================================================
// Helper function responsible for handling user-SGPR updates during Dispatch-time validation when the active pipeline
// has changed since the previous Dispatch operation.  It is expected that this will be called only when the pipeline
// is changing and immediately before a call to WriteUserDataEntriesToSgprs<false, ...>().
bool UniversalCmdBuffer::FixupUserSgprsOnPipelineSwitchCs(
    const UserDataEntries&          userData,
    const ComputePipelineSignature* pCurrSignature,
    const ComputePipelineSignature* pPrevSignature,
    const bool                      onAce,
    uint32**                        ppDeCmdSpace)
{
    PAL_ASSERT(pPrevSignature != nullptr);

    // The WriteUserDataEntriesToSgprs() method writes all entries which are mapped to user-SGPR's.
    // When the active pipeline is changing, the set of entries mapped to user-SGPR's have been changed
    // and which entries are mapped to which registers can also change.  The simplest way to handle
    // this is to write all mapped user-SGPR's whose mappings are changing.
    // These functions are only called when the pipeline has changed.

    bool written = false;

    if (pCurrSignature->userDataHash != pPrevSignature->userDataHash)
    {
        if (((m_cachedSettings.supportsShPairsPacket && (onAce == false)) || m_cachedSettings.supportsShPairsPacketCs)
            && (m_numValidUserEntriesCs > 0))
        {
            // Even though we ignore dirty flags here, we still need to accumulate user data entries into packed
            // register pairs for each draw/dispatch when the active pipeline has changed and there are pending register
            // writes (so we only need to write a single packed packet for user entries). If there are no pending writes
            // in the valid user entry packed register pair array, it is more performant to write compute user data
            // into SGPRs via the non-packed SET_SH_REG packet as we can guarantee SGPRs are contiguous when
            // IgnoreDirtyFlags = true.
            CmdStream::AccumulateUserDataEntriesForSgprs<true>(pCurrSignature->stage,
                                                               userData,
                                                               m_baseUserDataReg[HwShaderStage::Cs],
                                                               m_validUserEntryRegPairsCs,
                                                               &m_validUserEntryRegPairsLookupCs[0],
                                                               m_minValidUserEntryLookupValueCs,
                                                               &m_numValidUserEntriesCs);
        }
        else
        {
            uint32* pDeCmdSpace = (*ppDeCmdSpace);
            pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprs<true, ShaderCompute>(pCurrSignature->stage,
                                                                                         userData,
                                                                                         pDeCmdSpace);
            (*ppDeCmdSpace) = pDeCmdSpace;
        }

        written = true;
    }

    return written;
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
            const auto* pGfxTargetView = static_cast<const ColorTargetView*>(pTargetView);

            pGfxTargetView->GetImageSrd(m_device, &m_uavExportTable.srd[idx]);
        }
        else
        {
            m_uavExportTable.srd[idx] = {};
        }
    }
    m_uavExportTable.state.dirty = 1;
}

// =====================================================================================================================
// Helper function to validate and write packed user data entries to SGPRs. It is the caller's responsibility to ensure
// that all user data entries are either packed into a PackedRegisterPair array or written into the command stream
// before this function is called.
// Returns next unused DWORD in command space.
template <Pm4ShaderType ShaderType, bool Pm4OptImmediate>
uint32* UniversalCmdBuffer::WritePackedUserDataEntriesToSgprs(
    uint32* pDeCmdSpace)
{
    PackedRegisterPair* pValidRegPairs = (ShaderType == ShaderCompute) ? m_validUserEntryRegPairsCs
                                                                       : m_validUserEntryRegPairs;
    uint32*             pValidNumRegs  = (ShaderType == ShaderCompute) ? &m_numValidUserEntriesCs
                                                                       : &m_numValidUserEntries;

    PAL_DEBUG_BUILD_ONLY_ASSERT(*pValidNumRegs <= ((ShaderType == ShaderCompute) ? (Gfx11MaxPackedUserEntryCountCs  * 2)
                                                                                 : (Gfx11MaxPackedUserEntryCountGfx * 2)));

    pDeCmdSpace = m_deCmdStream.WriteSetShRegPairs<ShaderType, Pm4OptImmediate>(pValidRegPairs,
                                                                                *pValidNumRegs,
                                                                                pDeCmdSpace);

    // All entries are invalid once written to the command stream.
    if (ShaderType == ShaderCompute)
    {
        // incrementing this value invalidates all entries currently in the lookup table
        m_minValidUserEntryLookupValueCs++;
        PAL_ASSERT(m_minValidUserEntryLookupValueCs < MaxUserEntryLookupSetVal);
    }
    else
    {
        m_minValidUserEntryLookupValue++;
        PAL_ASSERT(m_minValidUserEntryLookupValue < MaxUserEntryLookupSetVal);
    }
    *pValidNumRegs = 0;

#if PAL_ENABLE_PRINTS_ASSERTS
    constexpr size_t ValidRegPairsSize = (ShaderType == ShaderCompute) ? sizeof(m_validUserEntryRegPairsCs)
                                                                       : sizeof(m_validUserEntryRegPairs);
    memset(pValidRegPairs, 0, ValidRegPairsSize);
#endif

    return pDeCmdSpace;
}

// =====================================================================================================================
// Wrapper for the real WritePackedUserDataEntriesToSgprs() for when the caller doesn't know if the immediate mode
// pm4 optimizer is enabled.
template <Pm4ShaderType ShaderType>
uint32* UniversalCmdBuffer::WritePackedUserDataEntriesToSgprs(
    uint32* pDeCmdSpace)
{
    if (m_deCmdStream.Pm4OptimizerEnabled())
    {
        pDeCmdSpace = WritePackedUserDataEntriesToSgprs<ShaderType, true>(pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = WritePackedUserDataEntriesToSgprs<ShaderType, false>(pDeCmdSpace);
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Helper function to write a single user-sgpr. This function should always be preferred for user data writes over
// WriteSetOneShReg() if the SGPR is written before or during draw/dispatch validation.
//
// On GFX11, this function will add the register offset and value into the relevant array of packed register pairs to be
// written in WritePackedUserDataEntriesToSgprs().
//
// Returns the next unused DWORD in pDeCmdSpace.
template <Pm4ShaderType ShaderType>
uint32* UniversalCmdBuffer::SetUserSgprReg(
    uint16  regAddr,
    uint32  regValue,
    bool    onAce,
    uint32* pDeCmdSpace)
{
    return SetSeqUserSgprRegs<ShaderType>(regAddr, regAddr, &regValue, onAce, pDeCmdSpace);
}

// =====================================================================================================================
// Helper function to write a sequence of user-sgprs. This function should always be preferred for user data writes over
// WriteSetSeqShRegs() if the SGPRs are written before or during draw/dispatch validation.
//
// On GFX11, this function will add the offsets/values into the relevant array of packed register pairs to be written
// in WritePackedUserDataEntriesToSgprs().
//
// Returns the next unused DWORD in pDeCmdSpace.
template <Pm4ShaderType ShaderType>
uint32* UniversalCmdBuffer::SetSeqUserSgprRegs(
    uint16      startAddr,
    uint16      endAddr,
    const void* pValues,
    bool        onAce,
    uint32*     pDeCmdSpace)
{
#if PAL_ENABLE_PRINTS_ASSERTS
    // This function is exclusively meant for writing user-SGPR regs. Use the regular WriteSetSeqShRegs/OneShReg() for
    // non user-SGPR SH reg writes.
    PAL_ASSERT(IsRegUserSgpr<ShaderType>(startAddr, m_baseUserDataReg));
#endif

    if ((m_cachedSettings.supportsShPairsPacket   && (onAce == false)) ||
        (m_cachedSettings.supportsShPairsPacketCs && (ShaderType == ShaderCompute)))
    {
        PackedRegisterPair* pValidRegPairs          = (ShaderType == ShaderCompute) ? m_validUserEntryRegPairsCs
                                                                                    : m_validUserEntryRegPairs;

        UserDataEntryLookup* pValidRegPairsLookup   = (ShaderType == ShaderCompute) ? m_validUserEntryRegPairsLookupCs
                                                                                    : m_validUserEntryRegPairsLookup;

        uint32*             pValidNumRegs           = (ShaderType == ShaderCompute) ? &m_numValidUserEntriesCs
                                                                                    : &m_numValidUserEntries;

        uint32              minValidRegLookupValue  = (ShaderType == ShaderCompute) ? m_minValidUserEntryLookupValueCs
                                                                                    : m_minValidUserEntryLookupValue;

        uint16 baseUserDataReg  = m_baseUserDataReg[HwShaderStage::Cs];
        uint8  stageLookupIndex = 0;
        if (ShaderType == ShaderGraphics)
        {
            const uint16 baseHsReg = m_baseUserDataReg[HwShaderStage::Hs];
            const uint16 baseGsReg = m_baseUserDataReg[HwShaderStage::Gs];
            const uint16 basePsReg = m_baseUserDataReg[HwShaderStage::Ps];

            const bool isTessReg = InRange<uint16>(startAddr, baseHsReg, baseHsReg + NumUserDataRegisters);
            const bool isGsReg   = InRange<uint16>(startAddr, baseGsReg, baseGsReg + NumUserDataRegisters);
            const bool isPsReg   = InRange<uint16>(startAddr, basePsReg, basePsReg + NumUserDataRegisters);

            baseUserDataReg  = isTessReg ? baseHsReg :
                               isGsReg   ? baseGsReg
                                         : basePsReg;

            stageLookupIndex = isTessReg ? LookupIndexHs :
                               isGsReg   ? LookupIndexGs
                                         : LookupIndexPs;
        }

        SetSeqUserDataEntryPairPackedValues(startAddr,
                                            endAddr,
                                            baseUserDataReg,
                                            pValues,
                                            pValidRegPairs,
                                            &pValidRegPairsLookup[stageLookupIndex],
                                            minValidRegLookupValue,
                                            pValidNumRegs);
    }
    else
    {
        pDeCmdSpace = m_deCmdStream.WriteSetSeqShRegs(startAddr, endAddr, ShaderType, pValues, pDeCmdSpace);
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Helper function which is responsible for making sure all user-data entries are written to either the spill table or
// to user-SGPR's, as well as making sure that all indirect user-data tables are up-to-date in GPU memory.  Part of
// Draw-time validation.  This version uses the CPU & embedded data for user-data table management.
template <bool HasPipelineChanged, bool TessEnabled, bool GsEnabled, bool VsEnabled>
uint32* UniversalCmdBuffer::ValidateGraphicsUserData(
    UserDataTableState*              pSpillTable,
    UserDataEntries*                 pUserDataEntries,
    const GraphicsPipelineSignature* pPrevSignature,
    uint32*                          pDeCmdSpace)
{
    PAL_ASSERT((HasPipelineChanged  && (pPrevSignature != nullptr)) ||
               (!HasPipelineChanged && (pPrevSignature == nullptr)));

    PAL_DEBUG_BUILD_ONLY_ASSERT((IsGfx11(m_gfxIpLevel) == false) || (VsEnabled == false));

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
            pDeCmdSpace = SetUserSgprReg<ShaderGraphics>(vertexBufTblRegAddr,
                                                         LowPart(m_vbTable.state.gpuVirtAddr),
                                                         false,
                                                         pDeCmdSpace);
        }
    } // if vertex buffer table is mapped by current pipeline

    const uint16 streamOutTblRegAddr = m_pSignatureGfx->streamOutTableRegAddr;
    if (streamOutTblRegAddr != UserDataNotMapped)
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
            constexpr uint32 StreamOutTableDwords = (sizeof(m_streamOut.srd) / sizeof(uint32));
            UpdateUserDataTableCpu(&m_streamOut.state,
                                   StreamOutTableDwords,
                                   0,
                                   reinterpret_cast<const uint32*>(&m_streamOut.srd[0]));
            gpuAddrDirty = true;
        }

        if (gpuAddrDirty)
        {
            pDeCmdSpace = SetUserSgprReg<ShaderGraphics>(streamOutTblRegAddr,
                                                         LowPart(m_streamOut.state.gpuVirtAddr),
                                                         false,
                                                         pDeCmdSpace);
        }
    } // if stream-out table is mapped by current pipeline

    const uint16 streamoutCntlBufRegAddr = m_pSignatureGfx->streamoutCntlBufRegAddr;
    if ((streamoutCntlBufRegAddr != UserDataNotMapped) && HasPipelineChanged)
    {
        // The memory for the streamout control buffer should have already been allocated and initialized.
        PAL_ASSERT(m_swStreamoutDataAddr != 0);

        pDeCmdSpace = SetUserSgprReg<ShaderGraphics>(streamoutCntlBufRegAddr,
                                                     LowPart(m_swStreamoutDataAddr),
                                                     false,
                                                     pDeCmdSpace);
    }

    const uint16 meshPipeStatsBufRegAddr = m_pSignatureGfx->meshPipeStatsBufRegAddr;
    if (HasPipelineChanged                             &&
        (meshPipeStatsBufRegAddr != UserDataNotMapped) &&
        (pPrevSignature->meshPipeStatsBufRegAddr != meshPipeStatsBufRegAddr))
    {
        PAL_ASSERT(m_meshPipeStatsGpuAddr != 0);

        // The pipeline stats buffer for Mesh/Task shaders is located in the DescriptorTable range, so we can use a
        // single-dword descriptor.
        pDeCmdSpace = SetUserSgprReg<ShaderGraphics>(meshPipeStatsBufRegAddr,
                                                     LowPart(m_meshPipeStatsGpuAddr),
                                                     false,
                                                     pDeCmdSpace);
    } // if shader pipeline stats buffer is mapped by current pipeline

    const uint16 primsNeededCntAddr = m_pSignatureGfx->primsNeededCntAddr;
    if ((HasPipelineChanged || (m_graphicsState.dirtyFlags.streamoutStatsQuery != 0)) &&
        (primsNeededCntAddr != UserDataNotMapped))
    {
        // The enablement is that bit 0 is set to 1, otherwise the feature is disabled.
        uint32 queryActiveFlag = (IsQueryActive(QueryPoolType::StreamoutStats)) ? 1 : 0;
        pDeCmdSpace = SetUserSgprReg<ShaderGraphics>(primsNeededCntAddr,
                                                     queryActiveFlag,
                                                     false,
                                                     pDeCmdSpace);
    }

    const uint16 sampleInfoAddr = m_pSignatureGfx->sampleInfoRegAddr;
    if (HasPipelineChanged && (sampleInfoAddr != UserDataNotMapped))
    {
        Abi::ApiSampleInfo sampleInfo;
        sampleInfo.numSamples = m_graphicsState.numSamplesPerPixel;
        sampleInfo.samplePatternIdx = Log2(m_graphicsState.numSamplesPerPixel) * MaxMsaaRasterizerSamples;
        pDeCmdSpace = SetUserSgprReg<ShaderGraphics>(sampleInfoAddr,
                                                     sampleInfo.u32All,
                                                     false,
                                                     pDeCmdSpace);
    }

    const uint16 dualSourceBlendInfoAddr = m_pSignatureGfx->dualSourceBlendInfoRegAddr;
    if (HasPipelineChanged && (dualSourceBlendInfoAddr != UserDataNotMapped))
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 842
        const DynamicGraphicsState& dynamicState = m_graphicsState.dynamicGraphicsInfo.dynamicState;
#else
        const DynamicGraphicsState& dynamicState = m_graphicsState.dynamicState;
#endif

        const uint32 dualSourceBlendInfo = dynamicState.enable.dualSourceBlendEnable &&
                                           dynamicState.dualSourceBlendEnable;

        pDeCmdSpace = SetUserSgprReg<ShaderGraphics>(dualSourceBlendInfoAddr,
                                                     dualSourceBlendInfo,
                                                     false,
                                                     pDeCmdSpace);
    }

    if (m_pSignatureGfx->compositeData.packed != 0)
    {
        bool isDirty = m_graphicsState.dirtyFlags.quadSamplePatternState ||
                       m_graphicsState.dirtyFlags.inputAssemblyState ||
                       m_graphicsState.dirtyFlags.colorBlendState;
        if (HasPipelineChanged || isDirty)
        {
            Abi::ApiCompositeDataValue registerVal = {};
            const auto* const pGraPipeline =
                static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
            if (pGraPipeline->GetOutputNumVertices() != 0)
            {
                PAL_ASSERT(pGraPipeline->IsGsEnabled() || pGraPipeline->IsTessEnabled() ||
                    pGraPipeline->HasMeshShader());
                registerVal.primInfo = pGraPipeline->GetOutputNumVertices();
            }
            else
            {
                // only VS
                registerVal.primInfo = GfxDevice::VertsPerPrimitive(m_graphicsState.inputAssemblyState.topology);
            }

            registerVal.numSamples = m_graphicsState.numSamplesPerPixel;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 842
            const DynamicGraphicsState& dynamicState = m_graphicsState.dynamicGraphicsInfo.dynamicState;
#else
            const DynamicGraphicsState& dynamicState = m_graphicsState.dynamicState;
#endif
            registerVal.dynamicSourceBlend = dynamicState.enable.dualSourceBlendEnable &&
                dynamicState.dualSourceBlendEnable;

            for (uint32 s = 0; s < NumHwShaderStagesGfx; ++s)
            {
                const uint16 userSgpr = m_pSignatureGfx->compositeData.addr[s];
                if (userSgpr != UserDataNotMapped)
                {
                    pDeCmdSpace = SetUserSgprReg<ShaderGraphics>(userSgpr,
                        registerVal.u32All,
                        false,
                        pDeCmdSpace);
                }
            }
        }
    }

    // Update uav export srds if enabled
    const uint16 uavExportEntry = m_pSignatureGfx->uavExportTableAddr;
    if (uavExportEntry != UserDataNotMapped)
    {
        const auto dirtyFlags = m_graphicsState.dirtyFlags;
        if (HasPipelineChanged || (dirtyFlags.colorTargetView))
        {
            UpdateUavExportTable();
        }

        if (m_uavExportTable.state.dirty != 0)
        {
            UpdateUserDataTableCpu(&m_uavExportTable.state,
                                   m_uavExportTable.tableSizeDwords,
                                   0,
                                   reinterpret_cast<const uint32*>(&m_uavExportTable.srd));
        }

        // Update the virtual address if the table has been relocated or we have a different sgpr mapping
        if ((HasPipelineChanged && (pPrevSignature->uavExportTableAddr != uavExportEntry)) ||
            (m_uavExportTable.state.dirty != 0))
        {
            // UAV export table is PS-only.
            PAL_ASSERT(InRange<uint16>(uavExportEntry, mmSPI_SHADER_USER_DATA_PS_0, mmSPI_SHADER_USER_DATA_PS_31));
            const uint32 gpuVirtAddrLo = LowPart(m_uavExportTable.state.gpuVirtAddr);

            pDeCmdSpace = SetUserSgprReg<ShaderGraphics>(uavExportEntry,
                                                         gpuVirtAddrLo,
                                                         false,
                                                         pDeCmdSpace);
        }
    }

    // Step #2:
    // Write all dirty user-data entries to their mapped user SGPR's.
    uint8 alreadyWrittenStageMask = 0;
    if (HasPipelineChanged
        )
    {
        alreadyWrittenStageMask = FixupUserSgprsOnPipelineSwitch<TessEnabled, GsEnabled, VsEnabled>(pUserDataEntries,
                                                                                                    pPrevSignature,
                                                                                                    &pDeCmdSpace);
    }

    const uint16 spillThreshold   = m_pSignatureGfx->spillThreshold;
    bool         reUpload         = false;
    const bool   anyUserDataDirty = IsAnyUserDataDirty(pUserDataEntries);

    if (anyUserDataDirty
        )
    {
        pDeCmdSpace = WriteDirtyUserDataEntriesToSgprsGfx<TessEnabled, GsEnabled, VsEnabled>(pUserDataEntries,
                                                                                             pPrevSignature,
                                                                                             alreadyWrittenStageMask,
                                                                                             pDeCmdSpace);
    }

    if ((HasPipelineChanged || anyUserDataDirty)
        )
    {
        if (spillThreshold != NoUserDataSpilling)
        {
            const uint16 userDataLimit = m_pSignatureGfx->userDataLimit;
            PAL_ASSERT(userDataLimit > 0);
            const uint16 lastUserData  = (userDataLimit - 1);

            // Step #3:
            // Because the spill table is managed using CPU writes to embedded data, it must be fully re-uploaded for
            // any Draw/Dispatch whenever *any* contents have changed.
            reUpload = (pSpillTable->dirty != 0);
            if (HasPipelineChanged &&
                ((spillThreshold < pPrevSignature->spillThreshold) || (userDataLimit > pPrevSignature->userDataLimit)))
            {
                // If the pipeline is changing and the spilled region is expanding, we need to re-upload the table
                // because we normally only update the portions usable by the bound pipeline to minimize memory usage.
                reUpload = true;
            }
            else if (anyUserDataDirty)
            {
                // Otherwise, use the following loop to check if any of the spilled user-data entries are dirty.
                const uint32 firstMaskId = (spillThreshold / UserDataEntriesPerMask);
                const uint32 lastMaskId  = (lastUserData   / UserDataEntriesPerMask);
                for (uint32 maskId = firstMaskId; maskId <= lastMaskId; ++maskId)
                {
                    size_t dirtyMask = pUserDataEntries->dirty[maskId];
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
                UpdateUserDataTableCpu(pSpillTable,
                                       (userDataLimit - spillThreshold),
                                       spillThreshold,
                                       &pUserDataEntries->entries[0]);
            }

            // NOTE: If the pipeline is changing, we may need to re-write the spill table address to any shader stage,
            // even if the spill table wasn't re-uploaded because the mapped user-SGPRs for the spill table could have
            // changed (as indicated by 'alreadyWrittenStageMask').
            if ((alreadyWrittenStageMask != 0) || reUpload)
            {
                const uint32 gpuVirtAddrLo = LowPart(m_spillTable.stateGfx.gpuVirtAddr);
                for (uint32 s = 0; s < NumHwShaderStagesGfx; ++s)
                {
                    const uint16 userSgpr = m_pSignatureGfx->stage[s].spillTableRegAddr;
                    if (userSgpr != UserDataNotMapped)
                    {
                        pDeCmdSpace = SetUserSgprReg<ShaderGraphics>(userSgpr,
                                                                    gpuVirtAddrLo,
                                                                    false,
                                                                    pDeCmdSpace);
                    }
                }
            }
        } // if current pipeline spills user-data

        // All dirtied user-data entries have been written to user-SGPR's or to the spill table somewhere in this
        // method, so it is safe to clear these bits.
        size_t* pDirtyMask = &pUserDataEntries->dirty[0];
        for (uint32 i = 0; i < NumUserDataFlagsParts; i++)
        {
            pDirtyMask[i] = 0;
        }
    }// if any user data is dirty or pipeline changed

    return pDeCmdSpace;
}

// =====================================================================================================================
// Helper function which is responsible for making sure all user-data entries are written to either the spill table or
// to user-SGPR's, as well as making sure that all indirect user-data tables are up-to-date in GPU memory.  Part of
// Dispatch-time validation.  This version uses the CPU & embedded data for user-data table management.
template <bool HasPipelineChanged>
uint32* UniversalCmdBuffer::ValidateComputeUserData(
    ICmdBuffer*                     pCmdBuffer,
    UserDataTableState*             pSpillTable,
    UserDataEntries*                pUserData,
    CmdStream*                      pCmdStream,
    const ComputePipelineSignature* pPrevSignature,
    const ComputePipelineSignature* pCurrSignature,
    uint32*                         pCmdSpace)
{
    PAL_ASSERT((HasPipelineChanged  && (pPrevSignature != nullptr)) ||
               (!HasPipelineChanged && (pPrevSignature == nullptr)));

    auto pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    // Step #1:
    // Write all dirty user-data entries to their mapped user SGPR's. If the pipeline has changed we must also fixup
    // the dirty bits because the prior compute pipeline could use fewer fast sgprs than the current pipeline.

    const bool onAce = (pCmdStream == m_pAceCmdStream);
    bool alreadyWritten = false;

    if (HasPipelineChanged)
    {
        alreadyWritten = pThis->FixupUserSgprsOnPipelineSwitchCs(*pUserData,
                                                                 pCurrSignature,
                                                                 pPrevSignature,
                                                                 onAce,
                                                                 &pCmdSpace);
    }

    if (alreadyWritten == false)
    {
        if ((m_cachedSettings.supportsShPairsPacket && (onAce == false)) || m_cachedSettings.supportsShPairsPacketCs)
        {
            CmdStream::AccumulateUserDataEntriesForSgprs<false>(pCurrSignature->stage,
                                                                *pUserData,
                                                                m_device.GetBaseUserDataReg(HwShaderStage::Cs),
                                                                m_validUserEntryRegPairsCs,
                                                                &m_validUserEntryRegPairsLookupCs[0],
                                                                m_minValidUserEntryLookupValueCs,
                                                                &m_numValidUserEntriesCs);
        }
        else
        {
            pCmdSpace = pCmdStream->WriteUserDataEntriesToSgprs<false, ShaderCompute>(pCurrSignature->stage,
                                                                                      *pUserData,
                                                                                      pCmdSpace);
        }
    }

    const uint16 spillThreshold = pCurrSignature->spillThreshold;
    if (spillThreshold != NoUserDataSpilling)
    {
        const uint16 userDataLimit = pCurrSignature->userDataLimit;
        PAL_ASSERT(userDataLimit != 0);
        const uint16 lastUserData  = (userDataLimit - 1);

        // Step #2:
        // Because the spill table is managed using CPU writes to embedded data, it must be fully re-uploaded for any
        // Dispatch whenever *any* contents have changed.
        bool reUpload = (pSpillTable->dirty != 0);
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
                size_t dirtyMask = pUserData->dirty[maskId];
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
            pThis->UpdateUserDataTableCpu(pSpillTable,
                                          (userDataLimit - spillThreshold),
                                          spillThreshold,
                                          &pUserData->entries[0]);
        }

        // Step #4:
        // We need to re-write the spill table GPU address to its user-SGPR if:
        // - the spill table was reuploaded during step #3, or
        // - the pipeline was changed and the previous pipeline either didn't spill or used a different spill reg.
        if (reUpload ||
            (HasPipelineChanged &&
             ((pPrevSignature->spillThreshold == NoUserDataSpilling) ||
              (pPrevSignature->stage.spillTableRegAddr != pCurrSignature->stage.spillTableRegAddr))))
        {
            pCmdSpace = SetUserSgprReg<ShaderCompute>(pCurrSignature->stage.spillTableRegAddr,
                                                      LowPart(pSpillTable->gpuVirtAddr),
                                                      onAce,
                                                      pCmdSpace);
        }
    } // if current pipeline spills user-data

    const uint16 taskPipeStatsBufRegAddr = pCurrSignature->taskPipeStatsBufRegAddr;
    if (HasPipelineChanged                             &&
        (taskPipeStatsBufRegAddr != UserDataNotMapped) &&
        (pPrevSignature->taskPipeStatsBufRegAddr != taskPipeStatsBufRegAddr))
    {
        PAL_ASSERT(m_meshPipeStatsGpuAddr != 0);

        // The pipeline stats buffer for Mesh/Task shaders is located in the DescriptorTable range, so we can use a
        // single-dword descriptor.
        pCmdSpace = SetUserSgprReg<ShaderCompute>(taskPipeStatsBufRegAddr,
                                                  LowPart(m_meshPipeStatsGpuAddr),
                                                  onAce,
                                                  pCmdSpace);
    } // if shader pipeline stats buffer is mapped by current pipeline

    // All dirtied user-data entries have been written to user-SGPR's or to the spill table somewhere in this method,
    // so it is safe to clear these bits.
    for (uint32 i = 0; i < NumUserDataFlagsParts; i++)
    {
        pUserData->dirty[i] = 0;
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace. Wrapper to determine
// if immediate mode pm4 optimization is enabled before calling the real ValidateDraw() function.
template <bool Indexed, bool Indirect>
void UniversalCmdBuffer::ValidateDraw(
    const Pm4::ValidateDrawInfo& drawInfo)      // Draw info
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
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace. Wrapper to determine
// if the pipeline is dirty before calling the real ValidateDraw() function.
template <bool Indexed, bool Indirect, bool Pm4OptImmediate>
void UniversalCmdBuffer::ValidateDraw(
    const Pm4::ValidateDrawInfo & drawInfo)
{
    if (IsVrsStateDirty())
    {
        // This has the potential to write a *LOT* of PM4 so do this outside the "main" reserve / commit commands
        // checks below.  It also has the potential to set new dirty states, so do all this stuff early.
        ValidateVrsState();
    }

#if PAL_DEVELOPER_BUILD
    uint32 startingCmdLen = GetUsedSize(CommandDataAlloc);
    uint32 userDataCmdLen = 0;
#endif

    if (m_graphicsState.pipelineState.dirtyFlags.pipeline || m_pipelineDynRegsDirty)
    {
        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

        const auto*const pNewPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

        if (m_graphicsState.pipelineState.dirtyFlags.pipeline)
        {
            pDeCmdSpace = pNewPipeline->WriteShCommands(&m_deCmdStream, pDeCmdSpace, m_graphicsState.dynamicGraphicsInfo);

            if (m_buildFlags.prefetchShaders)
            {
                pDeCmdSpace = pNewPipeline->Prefetch(pDeCmdSpace);
            }

            m_deCmdStream.CommitCommands(pDeCmdSpace);
            pDeCmdSpace = m_deCmdStream.ReserveCommands();
        }

        const auto*const pPrevSignature = m_pSignatureGfx;
        m_pSignatureGfx                 = &pNewPipeline->Signature();

        pDeCmdSpace = SwitchGraphicsPipeline(pPrevSignature, pNewPipeline, pDeCmdSpace);

#if PAL_ENABLE_PRINTS_ASSERTS
        m_pipelineStateValid = true; ///< Setup in SwitchGraphicsPipeline()
#endif

        // NOTE: Switching a graphics pipeline can result in a large amount of commands being written, so start a new
        // reserve/commit region before proceeding with validation.
        m_deCmdStream.CommitCommands(pDeCmdSpace);

#if PAL_DEVELOPER_BUILD
        if (m_cachedSettings.enablePm4Instrumentation != 0)
        {
            const uint32 pipelineCmdLen = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
            m_device.DescribeBindPipelineValidation(this, pipelineCmdLen);
            startingCmdLen += pipelineCmdLen;
        }
#endif

        pDeCmdSpace = m_deCmdStream.ReserveCommands();

        pDeCmdSpace = (this->*m_pfnValidateUserDataGfxPipelineSwitch)(&m_spillTable.stateGfx,
                                                                      &m_graphicsState.gfxUserDataEntries,
                                                                      pPrevSignature,
                                                                      pDeCmdSpace);

#if PAL_DEVELOPER_BUILD
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
        m_pipelineStateValid = true; ///< Valid for all for draw-time when pipeline isn't dirty.
#endif

        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

        pDeCmdSpace = (this->*m_pfnValidateUserDataGfx)(&m_spillTable.stateGfx,
                                                        &m_graphicsState.gfxUserDataEntries,
                                                        nullptr,
                                                        pDeCmdSpace);

#if PAL_DEVELOPER_BUILD
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

#if PAL_DEVELOPER_BUILD
    if (m_cachedSettings.enablePm4Instrumentation != 0)
    {
        const uint32 miscCmdLen = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
        m_device.DescribeDrawDispatchValidation(this, userDataCmdLen, miscCmdLen);
    }
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    m_pipelineStateValid = false;
#endif
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.  Wrapper to determine
// if any interesting state is dirty before calling the real ValidateDraw() function.
template <bool Indexed, bool Indirect, bool Pm4OptImmediate, bool PipelineDirty>
uint32* UniversalCmdBuffer::ValidateDraw(
    const Pm4::ValidateDrawInfo& drawInfo,
    uint32*                      pDeCmdSpace)
{
    constexpr Pm4::GraphicsStateFlags ValidationDirtyBits = { { .colorBlendState        = 1,
                                                                .depthStencilState      = 1,
                                                                .msaaState              = 1,
                                                                .quadSamplePatternState = 1,
                                                                .viewports              = 1,
                                                                .scissorRects           = 1,
                                                                .inputAssemblyState     = 1,
                                                                .triangleRasterState    = 1,
                                                                .occlusionQueryActive   = 1,
                                                                .lineStippleState       = 1,
                                                                .colorTargetView        = 1,
                                                                .depthStencilView       = 1,
                                                                .vrsRateParams          = 1,
                                                                .vrsCenterState         = 1,
                                                                .vrsImage               = 1 } };

    // Strictly speaking, paScModeCntl1 is not similar dirty bits as tracked in validationBits. However for best CPU
    // performance in <PipelineDirty=false, StateDirty=false> path, manually make it as part of StateDirty path as
    // it is not frequently updated.
     const bool stateDirty = (((m_graphicsState.dirtyFlags.u32All & ValidationDirtyBits.u32All) |
                              (m_drawTimeHwState.valid.paScModeCntl1 == 0)) != 0);

    if (stateDirty)
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
    const Pm4::ValidateDrawInfo& drawInfo,
    uint32*                      pDeCmdSpace)
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

// =====================================================================================================================
static void UpdateMsaaForNggCullingCb(
    uint32                                     viewportCount,
    float                                      multiplier,
    const Abi::PrimShaderCullingCb::Viewports* pInputVportCb,
    Abi::PrimShaderCullingCb::Viewports*       pOutputVportCb)
{
    // Helper structure to convert uint32 to a float.
    union Uint32ToFloat
    {
        uint32 uValue;
        float  fValue;
    };

    // For small-primitive filter culling with NGG, the shader needs the viewport scale to premultiply
    // the number of samples into it.
    Uint32ToFloat uintToFloat = {};
    for (uint32 i = 0; i < viewportCount; i++)
    {
        uintToFloat.uValue  = pInputVportCb[i].paClVportXScale;
        uintToFloat.fValue *= multiplier;
        pOutputVportCb[i].paClVportXScale = uintToFloat.uValue;

        uintToFloat.uValue  = pInputVportCb[i].paClVportXOffset;
        uintToFloat.fValue *= multiplier;
        pOutputVportCb[i].paClVportXOffset = uintToFloat.uValue;

        uintToFloat.uValue  = pInputVportCb[i].paClVportYScale;
        uintToFloat.fValue *= multiplier;
        pOutputVportCb[i].paClVportYScale = uintToFloat.uValue;

        uintToFloat.uValue  = pInputVportCb[i].paClVportYOffset;
        uintToFloat.fValue *= multiplier;
        pOutputVportCb[i].paClVportYOffset = uintToFloat.uValue;
    }
}

// =====================================================================================================================
// This function updates the NGG culling data constant buffer which is needed for NGG culling operations to execute
// correctly.  See the UpdateNggCullingDataBufferWithGpu function for reference code.
// Returns a pointer to the next entry in the DE cmd space.  This function MUST NOT write any context registers!
uint32* UniversalCmdBuffer::UpdateNggCullingDataBufferWithCpu(
    uint32* pDeCmdSpace)
{
    PAL_ASSERT(m_pSignatureGfx->nggCullingDataAddr != UserDataNotMapped);

    constexpr uint32 NggStateDwords = (sizeof(Abi::PrimShaderCullingCb) / sizeof(uint32));
    const     uint16 nggRegAddr     = m_pSignatureGfx->nggCullingDataAddr;

    Abi::PrimShaderCullingCb* pPrimShaderCullingCb = &m_state.primShaderCullingCb;

    // If the clients have specified a default sample layout we can use the number of samples as a multiplier.
    // However, if custom sample positions are in use we need to assume the worst case sample count (16).
    const float multiplier = m_graphicsState.useCustomSamplePattern
                             ? 16.0f : static_cast<float>(m_nggState.numSamples);

    // Make a local copy of the various shader state so that we can modify it as necessary.
    Abi::PrimShaderCullingCb localCb;
    if (multiplier > 1.0f)
    {
        memcpy(&localCb, &m_state.primShaderCullingCb, NggStateDwords * sizeof(uint32));
        pPrimShaderCullingCb = &localCb;

        UpdateMsaaForNggCullingCb(m_graphicsState.viewportState.count,
                                  multiplier,
                                  &m_state.primShaderCullingCb.viewports[0],
                                  &localCb.viewports[0]);
    }

    // The alignment of the user data is dependent on the type of register used to store
    // the address.
    const bool always4ByteAligned = IsGfx11(m_gfxIpLevel);
    const uint32 byteAlignment = ((always4ByteAligned == false) & (nggRegAddr == mmSPI_SHADER_PGM_LO_GS)) ? 256 : 4;

    // Copy all of NGG state into embedded data, which is pointed to by nggTable.gpuVirtAddr
    UpdateUserDataTableCpu(&m_nggTable.state,
                           NggStateDwords, // size
                           0,              // offset
                           reinterpret_cast<const uint32*>(pPrimShaderCullingCb),
                           NumBytesToNumDwords(byteAlignment));

    gpusize gpuVirtAddr = m_nggTable.state.gpuVirtAddr;
    if (byteAlignment == 256)
    {
        // The address of the constant buffer is stored in the GS shader address registers, which require a
        // 256B aligned address.
        gpuVirtAddr = Get256BAddrLo(m_nggTable.state.gpuVirtAddr);
    }

    const uint16 baseGsUserSgpr = m_baseUserDataReg[HwShaderStage::Gs];
    if (InRange<uint16>(nggRegAddr, baseGsUserSgpr, baseGsUserSgpr + NumUserDataRegisters))
    {
        // We only want to write to the NGG reg addrs via the user-SGPR specific path when we are certain these are
        // user-SGPRs to avoid overwriting valid user-entries.
        pDeCmdSpace = SetSeqUserSgprRegs<ShaderGraphics>(nggRegAddr,
                                                         (nggRegAddr + 1),
                                                         &gpuVirtAddr,
                                                         false,
                                                         pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = m_deCmdStream.WriteSetSeqShRegs(nggRegAddr,
                                                      (nggRegAddr + 1),
                                                      ShaderGraphics,
                                                      &gpuVirtAddr,
                                                      pDeCmdSpace);
    }

    m_nggState.flags.dirty = 0;

    return pDeCmdSpace;
}

// =====================================================================================================================
template <bool PipelineDirty, bool StateDirty>
uint32* UniversalCmdBuffer::ValidateTriangleRasterState(
    const GraphicsPipeline* pPipeline,
    uint32*                 pDeCmdSpace)
{
    regPA_SU_SC_MODE_CNTL paSuScModeCntl;
    paSuScModeCntl.u32All  = m_paSuScModeCntl.u32All;
    const auto& params     = m_graphicsState.triangleRasterState;
    const auto  dirtyFlags = m_graphicsState.dirtyFlags;

    if ((StateDirty && dirtyFlags.triangleRasterState) ||
        (m_paSuScModeCntl.u32All == InvalidPaSuScModeCntlVal))
    {
        paSuScModeCntl.bits.POLY_OFFSET_FRONT_ENABLE = params.flags.frontDepthBiasEnable;
        paSuScModeCntl.bits.POLY_OFFSET_BACK_ENABLE  = params.flags.backDepthBiasEnable;
        paSuScModeCntl.bits.MULTI_PRIM_IB_ENA        = 1;

        static_assert((static_cast<uint32>(FillMode::Points)    == 0) &&
                      (static_cast<uint32>(FillMode::Wireframe) == 1) &&
                      (static_cast<uint32>(FillMode::Solid)     == 2),
                      "FillMode vs. PA_SU_SC_MODE_CNTL.POLY_MODE mismatch");

        paSuScModeCntl.bits.POLY_MODE            = ((params.frontFillMode != FillMode::Solid) ||
                                                    (params.backFillMode  != FillMode::Solid));
        paSuScModeCntl.bits.POLYMODE_BACK_PTYPE  = static_cast<uint32>(params.backFillMode);
        paSuScModeCntl.bits.POLYMODE_FRONT_PTYPE = static_cast<uint32>(params.frontFillMode);

        constexpr uint32 FrontCull = static_cast<uint32>(CullMode::Front);
        constexpr uint32 BackCull  = static_cast<uint32>(CullMode::Back);

        static_assert(((FrontCull | BackCull) == static_cast<uint32>(CullMode::FrontAndBack)),
                      "CullMode::FrontAndBack not a strict union of CullMode::Front and CullMode::Back");

        paSuScModeCntl.bits.CULL_FRONT = ((static_cast<uint32>(params.cullMode) & FrontCull) != 0);
        paSuScModeCntl.bits.CULL_BACK  = ((static_cast<uint32>(params.cullMode) & BackCull)  != 0);

        static_assert((static_cast<uint32>(FaceOrientation::Ccw) == 0) &&
                      (static_cast<uint32>(FaceOrientation::Cw)  == 1),
                      "FaceOrientation vs. PA_SU_SC_MODE_CNTL.FACE mismatch");

        paSuScModeCntl.bits.FACE = static_cast<uint32>(params.frontFace);

        static_assert((static_cast<uint32>(ProvokingVertex::First) == 0) &&
                      (static_cast<uint32>(ProvokingVertex::Last)  == 1),
                      "ProvokingVertex vs. PA_SU_SC_MODE_CNTL.PROVOKING_VTX_LAST mismatch");

        paSuScModeCntl.bits.PROVOKING_VTX_LAST = static_cast<uint32>(params.provokingVertex);
    }

    //  The field was added for both polymode and perpendicular endcap lines.
    //  The SC reuses some information from the first primitive for other primitives within a polymode group. The
    //  whole group needs to make it to the SC in the same order it was produced by the PA. When the field is enabled,
    //  the PA will set a keep_together bit on the first and last primitive of each group. This tells the PBB that the
    //  primitives must be kept in order
    //
    //  it should be enabled when POLY_MODE is enabled.  Also, if the driver ever sets PERPENDICULAR_ENDCAP_ENA, that
    //  should follow the same rules. POLY_MODE is handled @ set-time as it is known then.
    paSuScModeCntl.bits.KEEP_TOGETHER_ENABLE =
        ((m_pbbCntlRegs.paScBinnerCntl0.bits.BINNING_MODE != DISABLE_BINNING_USE_LEGACY_SC__GFX10) &&
         (paSuScModeCntl.bits.POLY_MODE || pPipeline->IsPerpEndCapsEnabled()));

    PAL_DEBUG_BUILD_ONLY_ASSERT(paSuScModeCntl.u32All != InvalidPaSuScModeCntlVal);

    if (paSuScModeCntl.u32All != m_paSuScModeCntl.u32All)
    {
        m_paSuScModeCntl.u32All                    = paSuScModeCntl.u32All;
        m_state.primShaderCullingCb.paSuScModeCntl = paSuScModeCntl.u32All;

        pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmPA_SU_SC_MODE_CNTL, paSuScModeCntl.u32All, pDeCmdSpace);
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// If the image we're doing a barrier on is the bound VRS rate image, assume that the rate image source has changed
// and we need to recopy its contents into hTile memory. There's no good way to know that the source VRS image has
// been modified.
void UniversalCmdBuffer::BarrierMightDirtyVrsRateImage(
    const IImage* pRateImage)
{
    PAL_ASSERT(pRateImage != nullptr);

    const auto* pImage = static_cast<const Pal::Image*>(pRateImage);

    // We only need to force VRS state validation if the image is currently bound as a VRS rate image. This covers the
    // case where the app binds a rate image, does a draw, and then modifies the rate image before the next draw.
    m_graphicsState.dirtyFlags.vrsImage |= (m_graphicsState.pVrsImage == pImage);

    // We must dirty all prior VRS copies that read from this image, if any.
    EraseVrsCopiesFromRateImage(pImage);
}

// =====================================================================================================================
// We take care to never overwrite HTile VRS data in universal command buffers (even in InitMaskRam) so only HW
// bugs should overwrite the HTile VRS data. It's OK that DMA command buffers will clobber HTile VRS data on Init
// because we'll redo the HTile update the first time the image is bound in a universal command buffer. Thus we
// only need to call DirtyVrsDepthImage when a certain HW bug is triggered.
void UniversalCmdBuffer::DirtyVrsDepthImage(
    const IImage* pDepthImage)
{
    // We only need to force VRS state validation if the image is currently bound as a depth target. This covers the
    // case where the app binds a depth target and a VRS rate image, does a draw, and then clobbers the HTile VRS data
    // before the next draw.
    const auto* pView = static_cast<const DepthStencilView*>(m_graphicsState.bindTargets.depthTarget.pDepthStencilView);
    const auto* pImage = static_cast<const Pal::Image*>(pDepthImage);

    m_graphicsState.dirtyFlags.vrsImage |= ((pView != nullptr) && (pView->GetImage()->Parent() == pImage));

    // We must dirty all prior VRS copies that wrote to this image, if any.
    EraseVrsCopiesToDepthImage(pImage);
}

// =====================================================================================================================
bool UniversalCmdBuffer::IsVrsStateDirty() const
{
    const auto dirtyFlags = m_graphicsState.dirtyFlags;
    return ((dirtyFlags.vrsRateParams || dirtyFlags.vrsImage || dirtyFlags.depthStencilView) &&
            m_cachedSettings.supportsVrs &&
            IsGfx10(m_gfxIpLevel));
}

// =====================================================================================================================
// Primary purpose of this function is to do draw-time copying of the image data supplied via the
// CmdBindSampleRateImage interface.
void UniversalCmdBuffer::ValidateVrsState()
{
    const auto             dirtyFlags             = m_graphicsState.dirtyFlags;
    const auto&            vrsRate                = m_graphicsState.vrsRateState;
    constexpr uint32       ImageCombinerStage     = static_cast<uint32>(VrsCombinerStage::Image);
    constexpr uint32       PrimitiveCombinerStage = static_cast<uint32>(VrsCombinerStage::Primitive);
    constexpr uint32       VertexCombinerStage    = static_cast<uint32>(VrsCombinerStage::ProvokingVertex);
    const VrsCombiner      imageCombiner          = vrsRate.combinerState[ImageCombinerStage];
    const VrsCombiner      vtxCombiner            = vrsRate.combinerState[VertexCombinerStage];
    const Gfx9PalSettings& settings               = m_device.Settings();
    VrsRateParams          newRateParams          = vrsRate;
    bool                   bindNewRateParams      =
        m_cachedSettings.optimizeNullSourceImage && dirtyFlags.vrsRateParams;

    // Make sure the panel is requesting the optimized path.
    if (m_cachedSettings.optimizeNullSourceImage &&
        // A null source image corresponds to a 1x1 input into the image combiner.  Unless the combiner state is
        // "sum", we can fake a 1x1 input by messing around with the combiner states. Do some relatively easy fixup
        // checks first.
        ((m_graphicsState.pVrsImage == nullptr) && (imageCombiner != VrsCombiner::Sum)))
    {
        // Unless the client has changed either the rate-params or the bound image, then there's nothing to do
        // here.  The state of the depth image doesn't matter as we're not going to change it.
        if (dirtyFlags.vrsRateParams || dirtyFlags.vrsImage)
        {
            switch (imageCombiner)
            {
                case VrsCombiner::Min:
                    // The result of min(A, 1x1) will always be "1x1".  Same as the "override" case;
                    // i.e., previous combiner state will always lose
                    [[fallthrough]];

                case VrsCombiner::Override:
                    // Set register shading rate to 1x1,
                    newRateParams.shadingRate = VrsShadingRate::_1x1;

                    // Set this and all preceding combiners ("provoking", "primitive" and "image") to passthrough.
                    for (uint32 idx = 0; idx <= static_cast<uint32>(VrsCombinerStage::Image); idx++)
                    {
                        newRateParams.combinerState[idx] = VrsCombiner::Passthrough;
                    }

                    bindNewRateParams = true;
                    break;

                case VrsCombiner::Max:
                    // The result of "max(A, 1x1)" will always be "A" so the image combiner can be set to
                    // passthrough (i.e., take the output of the previous combiner, since the image combiner
                    // will never win).
                    newRateParams.combinerState[static_cast<uint32>(VrsCombinerStage::Image)] =
                            VrsCombiner::Passthrough;

                    bindNewRateParams = true;
                    break;

                case VrsCombiner::Passthrough:
                    // The image combiner is going to ignore the image data, so there's nothing to do here.
                    break;

                case VrsCombiner::Sum:
                    // These cases should have been caught above.  What are we doing here?
                    PAL_ASSERT_ALWAYS();
                    break;

                default:
                    // What is this?
                    PAL_NOT_IMPLEMENTED();
                    break;
            }
        } // end dirty checks
    }
    // We don't care about the rate-parameters changing here as we're destined to update the depth buffer
    // and the combiners will take care of themselves.
    else if (dirtyFlags.depthStencilView || dirtyFlags.vrsImage)
    {
        // Ok, we have source image data that's going to be useful in determining the final shading rate.
        const auto& depthTarget   = m_graphicsState.bindTargets.depthTarget;
        const auto* pClientDsView = static_cast<const Gfx10DepthStencilView*>(depthTarget.pDepthStencilView);
        const auto& rpm           = static_cast<const Gfx10RsrcProcMgr&>(m_device.RsrcProcMgr());

        // Ok, we can't cheat our way to binding this image by modifying the combiner state.  Do we have a
        // client-specified depth buffer into which to copy the shading-rate data?
        if ((pClientDsView != nullptr) && (pClientDsView->GetImage() != nullptr))
        {
            if (IsVrsCopyRedundant(pClientDsView, m_graphicsState.pVrsImage) == false)
            {
                AddVrsCopyMapping(pClientDsView, m_graphicsState.pVrsImage);

                const Image*    pDepthImg        = pClientDsView->GetImage();
                const SubresId  viewBaseSubResId = { 0,
                                                     pClientDsView->MipLevel(),
                                                     pClientDsView->BaseArraySlice() };
                const auto*     pSubResInfo      = pDepthImg->Parent()->SubresourceInfo(viewBaseSubResId);

                rpm.CopyVrsIntoHtile(this, pClientDsView, true, pSubResInfo->extentTexels, m_graphicsState.pVrsImage);
            }
        }
        else
        {
            const auto* pDsView = const_cast<Pal::Gfx9::Device&>(m_device).GetVrsDepthStencilView();
            if (pDsView != nullptr)
            {
                // Ok, the client didn't provide a depth buffer :-( and we have source image data (that could be NULL)
                // that's going to modify the final shading rate.  The device created a depth view for just this
                // occassion, so get that pointer and bind it appropriately.
                const auto*      pDepthImg       = pDsView->GetImage();
                const auto&      depthCreateInfo = pDepthImg->Parent()->GetImageCreateInfo();
                BindTargetParams newBindParams   = GetGraphicsState().bindTargets;

                // Worst case is that there are no bound color targets and we have to initialize the full dimensions
                // of our hTile buffer with VRS data.
                Extent3d  depthExtent = depthCreateInfo.extent;

                // However, if there are bound color buffers, then set the depth extent to the dimensions of the last
                // bound color target.  Each color target changed the scissor dimensions, so the last one should be
                // the one that counts.
                for (uint32  colorIdx = 0; colorIdx < newBindParams.colorTargetCount; colorIdx++)
                {
                    const auto&  colorBindInfo = newBindParams.colorTargets[colorIdx];
                    const auto*  pColorView    = static_cast<const ColorTargetView*>(colorBindInfo.pColorTargetView);
                    if (pColorView != nullptr)
                    {
                        const auto*  pColorImg = pColorView->GetImage();
                        if (pColorImg != nullptr)
                        {
                            depthExtent = pColorImg->Parent()->GetImageCreateInfo().extent;
                        } // end check for a valid image bound to this view
                    } // end check for a valid view
                } // end loop through all bound color targets

                // This would be big trouble.  The HW assumes that the depth buffer is at least as big as the color
                // buffer being rendered into...  this tripping means that the color target is larger than the depth
                // buffer.  We're about to page fault.  Only "cure" is to recreate the device's depth buffer with
                // a larger size.
                PAL_ASSERT((depthExtent.width  <= depthCreateInfo.extent.width) &&
                           (depthExtent.height <= depthCreateInfo.extent.height));

                // Point the HW's registers to our new depth buffer.  The layout shouldn't matter much as this
                // buffer only gets used for one thing.
                newBindParams.depthTarget.pDepthStencilView = pDsView;
                newBindParams.depthTarget.depthLayout       = { LayoutCopyDst, LayoutUniversalEngine };
                CmdBindTargets(newBindParams);

                if (IsVrsCopyRedundant(pDsView, m_graphicsState.pVrsImage) == false)
                {
                    AddVrsCopyMapping(pDsView, m_graphicsState.pVrsImage);

                    // And copy our source data into the image associated with this new view.
                    rpm.CopyVrsIntoHtile(this, pDsView, false, depthExtent, m_graphicsState.pVrsImage);
                }
            }
        } // end check for having a client depth buffer
    } // end check on dirty flags

    // If the new rate params haven't been bound and they need to be, then bind them now.
    if (bindNewRateParams)
    {
        WritePerDrawVrsRate(newRateParams);
        m_graphicsState.dirtyFlags.vrsRateParams = 1;
    }
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
    const Pm4::ValidateDrawInfo& drawInfo,      // Draw info
    uint32*                      pDeCmdSpace)   // Write new draw-engine commands here.
{
    const auto*const pBlendState = static_cast<const ColorBlendState*>(m_graphicsState.pColorBlendState);
    const auto*const pDepthState = static_cast<const DepthStencilState*>(m_graphicsState.pDepthStencilState);
    const auto*const pPipeline   = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const auto*const pMsaaState  = static_cast<const MsaaState*>(m_graphicsState.pMsaaState);
    const auto*const pDsView     =
        static_cast<const DepthStencilView*>(m_graphicsState.bindTargets.depthTarget.pDepthStencilView);

    const auto dirtyFlags = m_graphicsState.dirtyFlags;

    // If we're about to launch a draw we better have a pipeline bound.
    PAL_DEBUG_BUILD_ONLY_ASSERT(pPipeline != nullptr);

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

    if (PipelineDirty || (StateDirty && (dirtyFlags.colorBlendState || dirtyFlags.colorTargetView)))
    {
        pDeCmdSpace = ValidateCbColorInfoAndBlendState<Pm4OptImmediate, PipelineDirty, StateDirty>(pDeCmdSpace);
    }

    // Check for GFX11 as these programming requirements are specific to that platform.
    if (IsGfx11(m_gfxIpLevel))
    {
        // Is blending enabled on any target?
        const bool  blendingEnabled = ((pBlendState != nullptr) && (pBlendState->BlendEnableMask() != 0));

        // If there's no MSAA object bound, then assume 1xAA (log2(1) = 0).
        const uint32  log2ExposedSamples = ((pMsaaState != nullptr)
                                            ? pMsaaState->PaScAaConfig().bits.MSAA_EXPOSED_SAMPLES
                                            : 0);

        // If the panel is requesting to disable RB+ (dual quad) and we have either a dirty pipeline
        // or a color blend state then proceed.
        if ((m_cachedSettings.disableRbPlusWithBlending != 0) &&
            (PipelineDirty || (StateDirty && dirtyFlags.colorBlendState)))
        {
            // Is blending enabled on any target and is RB+ enabled?
            if (blendingEnabled && (m_cbColorControl.bits.DISABLE_DUAL_QUAD == 0))
            {
                // Yes, so we need to turn off RB+
                m_cbColorControl.bits.DISABLE_DUAL_QUAD = 1;

                pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmCB_COLOR_CONTROL, m_cbColorControl.u32All, pDeCmdSpace);
            }
        }

        // This isn't a correctness thing; this is a performance thing that's another attempt at resolving the
        // issues that the "disable RB+ with blending" issue didn't adequately resolve (see above).
        if (m_cachedSettings.waEnableIntrinsicRateEnable &&
            // If the pipeline is dirty then dbShaderControl has (potentially) changed.
            (PipelineDirty ||
             // Of we might need to do something if blending or the AA rate has changed as well.
             (StateDirty && (dirtyFlags.colorBlendState || dirtyFlags.msaaState))))
        {
            // If the pipeline set POPS, then the pipeline set its own "intrinsic rate" settings
            // that we want to keep.  If POPS is off in the pipeline is off though, then we need
            // to implement the workaround.
            if (pPipeline->DbShaderControl().bits.PRIMITIVE_ORDERED_PIXEL_SHADER == 0)
            {
                regDB_SHADER_CONTROL newDbShaderControl = m_dbShaderControl;

                // The POPS enabled case was handled when the pipeline was created.
                //      If(POPS)
                //          This is part of the register spec specified programming rules and was
                //          enabled by the pipeline programming.
                //          OVERRIDE_INTRINSIC_RATE_ENABLE = 1
                //          OVERRIDE_INTRINSIC_RATE = 0 (1xaa)
                //      Else if(BLENDING && pipeline in 1xaa)   (this part is a perf concern)
                //          OVERRIDE_INTRINSIC_RATE_ENABLE = 1
                //          OVERRIDE_INTRINSIC_RATE = 2 (4xaa)
                //      else
                //          OVERRIDE_INTRINSIC_RATE_ENABLE = 0
                if (blendingEnabled && (log2ExposedSamples == 0))
                {
                    newDbShaderControl.gfx11.OVERRIDE_INTRINSIC_RATE_ENABLE = 1;
                    newDbShaderControl.gfx11.OVERRIDE_INTRINSIC_RATE        = 2;
                }
                else
                {
                    newDbShaderControl.gfx11.OVERRIDE_INTRINSIC_RATE_ENABLE = 0;
                }

                // If our new value doesn't match the last-written value of this register, then
                // write it again.
                if (newDbShaderControl.u32All != m_dbShaderControl.u32All)
                {
                    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmDB_SHADER_CONTROL,
                                                                      newDbShaderControl.u32All,
                                                                      pDeCmdSpace);

                    m_dbShaderControl = newDbShaderControl;
                }
            }
        }

        // If this is GFX11 and we have a NULL depth buffer bound then we need to adjust the DB sample
        // count to be non-zero so that it doesn't negatively affect the VRS rate.
        if ((m_cachedSettings.useLegacyDbZInfo == false) &&
            ((StateDirty &&
             ((dirtyFlags.depthStencilView != 0) ||   // If the depth buffer has changed
              (dirtyFlags.msaaState != 0)))      &&   // Or if the MSAA rate has changed
             ((pDsView == nullptr)               ||   // and we have either a non-existent depth buffer
              (pDsView->GetImage() == nullptr))))     // or an explicitly bound NULL depth buffer
        {

            pDeCmdSpace = m_deCmdStream.WriteContextRegRmw(mmDB_Z_INFO,
                                                           DB_Z_INFO__NUM_SAMPLES_MASK,
                                                           log2ExposedSamples << DB_Z_INFO__NUM_SAMPLES__SHIFT,
                                                           pDeCmdSpace);
        }
    } // end check for gfx11

    // Writing the viewport and scissor-rect state is deferred until draw-time because they depend on both the
    // viewport/scissor-rect state and the active pipeline.
    if (StateDirty && dirtyFlags.viewports
        )
    {
        pDeCmdSpace = ValidateViewports<Pm4OptImmediate>(pDeCmdSpace);
    }

    regPA_SC_MODE_CNTL_1 paScModeCntl1 = m_drawTimeHwState.paScModeCntl1;

    // Re-calculate paScModeCntl1 value if state contributing to the register has changed.
    if (PipelineDirty ||
        (StateDirty && (dirtyFlags.depthStencilState       ||
                        dirtyFlags.colorBlendState         ||
                        dirtyFlags.msaaState               ||
                        dirtyFlags.depthStencilView        ||
                        dirtyFlags.occlusionQueryActive    ||
                        dirtyFlags.triangleRasterState     ||
                        (IsGfx11(m_gfxIpLevel)      &&
                         (dirtyFlags.vrsImage != 0) &&
                         (m_graphicsState.pVrsImage != nullptr)) ||
                        (m_drawTimeHwState.valid.paScModeCntl1 == 0))))
    {
        paScModeCntl1 = pPipeline->PaScModeCntl1();
        paScModeCntl1.bits.PS_ITER_SAMPLE |= (pMsaaState != nullptr) ? pMsaaState->ForceSampleRateShading() : 0;

        // If VRS surfaces are enabled, then we can not set the "WALK_ALIGNMENT" or the
        // "WALK_ALIGN8_PRIM_FITS_ST" fields of PA_SC_MODE_CNTL_1.
        if (IsGfx11(m_gfxIpLevel) &&
            ((m_graphicsState.pVrsImage != nullptr)
            ))
        {
            paScModeCntl1.bits.WALK_ALIGNMENT           = 0;
            paScModeCntl1.bits.WALK_ALIGN8_PRIM_FITS_ST = 0;
        }

        if ((m_cachedSettings.outOfOrderPrimsEnable != OutOfOrderPrimDisable) &&
            (pPipeline->IsOutOfOrderPrimsEnabled() == false))
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

    if (StateDirty && (dirtyFlags.msaaState || dirtyFlags.occlusionQueryActive))
    {
        // MSAA sample rates are associated with the MSAA state object, but the sample rate affects how queries are
        // processed (via DB_COUNT_CONTROL). We need to update the value of this register at draw-time since it is
        // affected by multiple elements of command-buffer state.
        const uint32 log2OcclusionQuerySamples = (pMsaaState != nullptr) ? pMsaaState->Log2OcclusionQuerySamples() : 0;
        pDeCmdSpace = UpdateDbCountControl<Pm4OptImmediate>(log2OcclusionQuerySamples, pDeCmdSpace);
    }

    if (PipelineDirty || (StateDirty && (dirtyFlags.msaaState || dirtyFlags.inputAssemblyState)))
    {
        // Typically, ForceWdSwitchOnEop only depends on the primitive topology and restart state.  However, when we
        // disable the hardware WD load balancing feature, we do need to some draw time parameters that can
        // change every draw.
        const bool            wdSwitchOnEop      = ForceWdSwitchOnEop(drawInfo);
        regIA_MULTI_VGT_PARAM iaMultiVgtParam    = pPipeline->IaMultiVgtParam(wdSwitchOnEop);
        regVGT_LS_HS_CONFIG   vgtLsHsConfig      = pPipeline->VgtLsHsConfig();
        const uint32          patchControlPoints = m_graphicsState.inputAssemblyState.patchControlPoints;

        PAL_ASSERT(IsTessEnabled() || (vgtLsHsConfig.bits.HS_NUM_INPUT_CP == patchControlPoints));

        const bool   lineStippleEnabled = (pMsaaState != nullptr) ? pMsaaState->UsesLineStipple() : false;
        const uint32 geCntl             = CalcGeCntl<IsNgg>(lineStippleEnabled, iaMultiVgtParam);

        // GE_CNTL tends to be the same so only bother writing it if the value has changed.
        if (geCntl != m_geCntl.u32All)
        {
            m_geCntl.u32All = geCntl;
            pDeCmdSpace = m_deCmdStream.WriteSetOneConfigReg(mmGE_CNTL, geCntl, pDeCmdSpace);
        }

        if (vgtLsHsConfig.u32All != m_vgtLsHsConfig.u32All)
        {
            PAL_ASSERT((vgtLsHsConfig.u32All & VgtLsHsConfigDirtyBit) == 0);
            m_vgtLsHsConfig = vgtLsHsConfig;
            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg<Pm4OptImmediate>(
                                            mmVGT_LS_HS_CONFIG, vgtLsHsConfig.u32All, pDeCmdSpace);
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
            PAL_ASSERT((paScConsRastCntl.u32All & PaScConsRastCntlDirtyBit) == 0);
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
    if ((PipelineDirty || StateDirty) &&
        (m_paScAaConfigNew.u32All != m_paScAaConfigLast.u32All))
    {
        pDeCmdSpace = ValidatePaScAaConfig(pDeCmdSpace);
    }

    // We shouldn't rewrite the PBB bin sizes unless at least one of these state objects has changed
    if (PipelineDirty ||
        (StateDirty && (dirtyFlags.colorTargetView   ||
                        dirtyFlags.depthStencilView  ||
                        dirtyFlags.depthStencilState)))
    {
        bool shouldEnablePbb = m_enabledPbb;
        // Accessing pipeline state in this function is usually a cache miss, so avoid function call
        // when only when pipeline has changed.
        if (PipelineDirty)
        {
            shouldEnablePbb = pPipeline->BinningAllowed();
        }

        // Reset binner state unless it used to be off and remains off.  If it was on and remains on, it is possible
        // the ideal bin sizes will change, so we must revalidate.
        // Optimal gfx10 bin sizes are determined from render targets both when PBB is enabled or disabled
        if (m_enabledPbb || shouldEnablePbb || IsGfx10(m_gfxIpLevel))
        {
            m_enabledPbb = shouldEnablePbb;
            pDeCmdSpace  = ValidateBinSizes<Pm4OptImmediate, IsNgg, Indirect>(pDeCmdSpace);
        }
    }

    if (PipelineDirty || StateDirty)
    {
        m_deCmdStream.CommitCommands(pDeCmdSpace);
        pDeCmdSpace = m_deCmdStream.ReserveCommands();
    }

    if (PipelineDirty || (StateDirty && dirtyFlags.triangleRasterState))
    {
        pDeCmdSpace = ValidateTriangleRasterState<PipelineDirty, StateDirty>(pPipeline, pDeCmdSpace);
    }

    const bool lineStippleStateDirty = StateDirty && (dirtyFlags.lineStippleState || dirtyFlags.inputAssemblyState);
    if (lineStippleStateDirty)
    {
        regPA_SC_LINE_STIPPLE paScLineStipple  = {};
        paScLineStipple.bits.REPEAT_COUNT      = m_graphicsState.lineStippleState.lineStippleScale;
        paScLineStipple.bits.LINE_PATTERN      = m_graphicsState.lineStippleState.lineStippleValue;
#if BIGENDIAN_CPU
        paScLineStipple.bits.PATTERN_BIT_ORDER = 1;
#endif
        // 1: Reset pattern count at each primitive
        // 2: Reset pattern count at each packet
        paScLineStipple.bits.AUTO_RESET_CNTL   =
            (m_graphicsState.inputAssemblyState.topology == PrimitiveTopology::LineList) ? 1 : 2;

        if (paScLineStipple.u32All != m_paScLineStipple.u32All)
        {
            pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmPA_SC_LINE_STIPPLE,
                                                                   paScLineStipple.u32All,
                                                                   pDeCmdSpace);
            m_paScLineStipple = paScLineStipple;
        }
    }

    if (PipelineDirty         ||
        lineStippleStateDirty ||
        // MSAA state is only meaningful for the purposes of a HW workaround that only applies to GFX11 products.
        (StateDirty && (dirtyFlags.msaaState != 0) && m_cachedSettings.waLineStippleReset))
    {
        regPA_SU_LINE_STIPPLE_CNTL paSuLineStippleCntl = {};

        if (pPipeline->IsLineStippleTexEnabled())
        {
            // Line stipple tex is only used by line stipple with wide antialiased line. so we need always
            // enable FRACTIONAL_ACCUM and EXPAND_FULL_LENGT.
            paSuLineStippleCntl.bits.LINE_STIPPLE_RESET =
                (m_graphicsState.inputAssemblyState.topology == PrimitiveTopology::LineList) ? 1 : 2;
            paSuLineStippleCntl.bits.FRACTIONAL_ACCUM = 1;
            paSuLineStippleCntl.bits.EXPAND_FULL_LENGTH = 1;
        }

        if (m_cachedSettings.waLineStippleReset)
        {
            const bool  lineStippleEnabled = ((pMsaaState != nullptr) ? pMsaaState->UsesLineStipple() : false);

            if (lineStippleEnabled == false)
            {
                //    Set the PA_SU_LINE_STIPPLE_CNTL.LINE_STIPPLE_RESET to 2 for all non-stipple draws,
                //    forcing PA to reset the stipple value at eop
                paSuLineStippleCntl.bits.LINE_STIPPLE_RESET = 2;
            }
        }

        if (paSuLineStippleCntl.u32All != m_paSuLineStippleCntl.u32All)
        {
            pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmPA_SU_LINE_STIPPLE_CNTL,
                                                                   paSuLineStippleCntl.u32All,
                                                                   pDeCmdSpace);
            m_paSuLineStippleCntl = paSuLineStippleCntl;
        }
    }

    // Validate primitive restart enable.  Primitive restart should only apply for indexed draws, but on gfx9,
    // VGT also applies it to auto-generated vertex index values.
    m_geMultiPrimIbResetEn.bits.RESET_EN = Indexed && m_graphicsState.inputAssemblyState.primitiveRestartEnable;

    // Validate the per-draw HW state.
    pDeCmdSpace = ValidateDrawTimeHwState<Indexed, Indirect, Pm4OptImmediate>(paScModeCntl1,
                                                                              drawInfo,
                                                                              pDeCmdSpace);

    pDeCmdSpace = m_workaroundState.PreDraw<PipelineDirty, StateDirty, Pm4OptImmediate>(m_graphicsState,
                                                                                        &m_deCmdStream,
                                                                                        this,
                                                                                        pDeCmdSpace);

    if (IsNgg                         &&
        (PipelineDirty || StateDirty) &&
        (m_nggState.flags.dirty)      &&
        (m_pSignatureGfx->nggCullingDataAddr != UserDataNotMapped))
    {
        pDeCmdSpace = UpdateNggCullingDataBufferWithCpu(pDeCmdSpace);
    }

    if (m_numValidUserEntries > 0)
    {
        pDeCmdSpace = WritePackedUserDataEntriesToSgprs<ShaderGraphics, Pm4OptImmediate>(pDeCmdSpace);
    }

    // Clear the dirty-state flags.
    m_graphicsState.dirtyFlags.u32All               = 0;
    m_graphicsState.pipelineState.dirtyFlags.u32All = 0;
    m_pipelineDynRegsDirty                          = false;

    m_state.flags.firstDrawExecuted = 1;

    return pDeCmdSpace;
}

// =====================================================================================================================
// Gfx10 specific function for calculating Color PBB bin size.
void UniversalCmdBuffer::Gfx10GetColorBinSize(
    Extent2d* pBinSize
    ) const
{
    // TODO: This function needs to be updated to look at the pixel shader and determine which outputs are valid in
    //       addition to looking at the bound render targets. Bound render targets may not necessarily get a pixel
    //       shader export. Using the bound render targets means that we may make the bin size smaller than it needs to
    //       be when a render target is bound, but is not written by the PS. With export cull mask enabled. We need only
    //       examine the PS output because it will account for any RTs that are not bound.

    // Calculate cColor
    uint32 cColor   = 0;

    const auto& boundTargets = m_graphicsState.bindTargets;
    const auto* pPipeline    = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const auto* pMsaaState   = static_cast<const MsaaState*>(m_graphicsState.pMsaaState);
    const bool  psIterSample = ((pPipeline != nullptr) && (pPipeline->PaScModeCntl1().bits.PS_ITER_SAMPLE == 1)) ||
                               pMsaaState->ForceSampleRateShading();
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
        }
    }
    cColor = Max(cColor, 1u);  // cColor 0 to 1 uses cColor=1

    // Calculate Color bin sizes
    // The logic for gfx10 bin sizes is based on a formula that accounts for the number of RBs
    // and Channels on the ASIC.  Since this a potentially large amount of combinations,
    // it is not practical to hardcode binning tables into the driver.
    // Note that the final bin size is choosen from minimum between Depth and Color.
    // Also note that there is bin size that corresponds to the bound fmasks. The driver code does not account for
    // this as the cases where it would impact the the suggested bin size are too few.

    // The logic given to calculate the Color bin size is:
    //   colorBinArea = ((CcReadTags * m_totalNumRbs / totalNumPipes) * (CcTagSize * totalNumPipes)) / cColor
    // The numerator has been pre-calculated as m_colorBinSizeTagPart.
    const uint32 colorLog2Pixels = Log2(m_colorBinSizeTagPart / cColor);
    const uint16 colorBinSizeX   = 1 << ((colorLog2Pixels + 1) / 2); // (Y_BIAS=false) round up width
    const uint16 colorBinSizeY   = 1 << (colorLog2Pixels / 2);       // (Y_BIAS=false) round down height

    // Return size adjusted for minimum bin size
    pBinSize->width  = Max(colorBinSizeX, m_minBinSizeX);
    pBinSize->height = Max(colorBinSizeY, m_minBinSizeY);
}

// =====================================================================================================================
// Gfx10 specific function for calculating Depth PBB bin size.
void UniversalCmdBuffer::Gfx10GetDepthBinSize(
    Extent2d* pBinSize
    ) const
{
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
        //   depthBinArea = ((ZsReadTags * m_totalNumRbs / totalNumPipes) * (ZsTagSize * totalNumPipes)) / cDepth
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
// Fills in paScBinnerCntl0/1(PA_SC_BINNER_CNTL_0/1 registers) with values that corresponds to the
// specified binning mode and sizes.
// Returns: True if PA_SC_BINNER_CNTL_0/1 changed value, False otherwise.
template <bool IsNgg>
bool UniversalCmdBuffer::SetPaScBinnerCntl01(
    const Extent2d* pBinSize)
{
    const regPA_SC_BINNER_CNTL_0 prevPaScBinnerCntl0 = m_pbbCntlRegs.paScBinnerCntl0;
    const regPA_SC_BINNER_CNTL_1 prevPaScBinnerCntl1 = m_pbbCntlRegs.paScBinnerCntl1;

    // Binner_cntl1:
    // 16 bits: Maximum amount of parameter storage allowed per batch.
    // - Legacy: param cache lines/2 (groups of 16 vert-attributes) (0 means 1 encoding)
    // - NGG: number of vert-attributes (0 means 1 encoding)
    // - NGG + PC: param cache lines/2 (groups of 16 vert-attributes) (0 means 1 encoding)
    // 16 bits: Max number of primitives in batch
    m_pbbCntlRegs.paScBinnerCntl1.bits.MAX_ALLOC_COUNT    = (IsNgg ? m_cachedPbbSettings.maxAllocCountNgg :
                                                                     m_cachedPbbSettings.maxAllocCountLegacy);

    m_pbbCntlRegs.paScBinnerCntl0.bits.BINNING_MODE =
        m_enabledPbb ? BINNING_ALLOWED : m_cachedSettings.pbbDisableBinMode;

    // Valid bin sizes require width and height to both be zero or both be non-zero.
    PAL_ASSERT(
        ((pBinSize->width == 0) && (pBinSize->height == 0)) ||
        ((pBinSize->width >  0) && (pBinSize->height >  0)));

    // If bin size is non-zero, then set the size properties
    if (pBinSize->width != 0)
    {
        if (pBinSize->width == 16)
        {
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_X        = 1;
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_X_EXTEND = 0;
        }
        else
        {
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_X        = 0;
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_X_EXTEND = Device::GetBinSizeEnum(pBinSize->width);
        }

        if (pBinSize->height == 16)
        {
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_Y        = 1;
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_Y_EXTEND = 0;
        }
        else
        {
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_Y        = 0;
            m_pbbCntlRegs.paScBinnerCntl0.bits.BIN_SIZE_Y_EXTEND = Device::GetBinSizeEnum(pBinSize->height);
        }
    }

    return ((prevPaScBinnerCntl0.u32All != m_pbbCntlRegs.paScBinnerCntl0.u32All) ||
            (prevPaScBinnerCntl1.u32All != m_pbbCntlRegs.paScBinnerCntl1.u32All));
}

// =====================================================================================================================
// Updates the bin sizes and writes to the register.
template <bool Pm4OptImmediate, bool IsNgg, bool Indirect>
uint32* UniversalCmdBuffer::ValidateBinSizes(
    uint32* pDeCmdSpace)
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

            // Final bin size is choosen from minimum between Depth, Color and Fmask.
            Gfx10GetColorBinSize(&colorBinSize); // returns minimum of Color and Fmask
            Gfx10GetDepthBinSize(&depthBinSize);

            const uint32 colorArea = colorBinSize.width * colorBinSize.height;
            const uint32 depthArea = depthBinSize.width * depthBinSize.height;

            binSize = (colorArea < depthArea) ? colorBinSize : depthBinSize;

            // We may calculate a bin size of 0, which means disable PBB
            if (binSize.width == 0)
            {
                // It is okay to do this here and not execute the 'else' below that corresponds to m_enabledPbb==false.
                // Only GFX9 disables binning by calculating a bin size of 0.
                // Only GFX10+ uses the DISABLE_BINNING_USE_NEW_SC mode which requires bin size programming when
                // bin size is disabled.
                m_enabledPbb = false;
            }
        }
    }
    else
    {
        // Set the bin sizes when we have binning disabled.
        // This matters for the DISABLE_BINNING_USE_NEW_SC mode. This mode enables binning with a batch size of
        // one prim per clock.
        binSize.width  = 128;
        binSize.height = 128;
    }

    // Update our copy of m_pbbCntlRegs.paScBinnerCntl0/1 and write it out.
    if (SetPaScBinnerCntl01<IsNgg>(
                                   &binSize))
    {
        pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs<Pm4OptImmediate>(mmPA_SC_BINNER_CNTL_0,
                                                                            mmPA_SC_BINNER_CNTL_1,
                                                                            &m_pbbCntlRegs,
                                                                            pDeCmdSpace);
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Constructs a virtual rectangle that surrounds all viewports in order to find a center point that must be written to
// PA_SU_HARDWARE_SCREEN_OFFSET so that the guardband originates from the rectangle's center rather than its origin.
// Also calculates scale factors, which is the factor by which the center rectangle can be scaled to fill the entire
// guardband region.
VportCenterRect UniversalCmdBuffer::GetViewportsCenterAndScale() const
{
    const ViewportParams& params = m_graphicsState.viewportState;
    const uint32 viewportCount   = (m_graphicsState.enableMultiViewport) ? params.count : 1;

    float rectLeft   = 0;
    float rectRight  = 0;
    float rectTop    = 0;
    float rectBottom = 0;

    VportCenterRect centerRect = {};

    for (uint32 i = 0; i < viewportCount; i++)
    {
        const Viewport& viewport = params.viewports[i];

        // Calculate the left and rightmost coordinates of the surrounding rectangle
        float left  = viewport.originX;
        float right = viewport.originX + viewport.width;
        // Swap left and right to correct negSize and posSize if width is negative
        if (viewport.width < 0)
        {
            Swap(left, right);
        }
        rectLeft    = Min(left, rectLeft);
        rectRight   = Max(right, rectRight);

        // Calculate the top and bottommost coordinates of the surrounding rectangle
        float top    = viewport.originY;
        float bottom = viewport.originY + viewport.height;
        // Swap top and bottom to correct negSize and posSize if height is negative
        if (viewport.height < 0)
        {
            Swap(top, bottom);
        }
        rectTop      = Min(top, rectTop);
        rectBottom   = Max(bottom, rectBottom);
    }

    // Calculate accumulated viewport rectangle center point
    float centerX          = (rectLeft   + rectRight) / 2;
    float centerY          = (rectBottom + rectTop)   / 2;
    // We must clamp the center point coords to 0 in the corner case where viewports are centered in negative space
    centerRect.centerX     = centerX > 0 ? centerX : 0;
    centerRect.centerY     = centerY > 0 ? centerY : 0;

    // Calculate max acceptable X and Y limit for guardband clipping
    float negSize          = (-MinHorzScreenCoord) + rectLeft;
    float posSize          = MaxHorzScreenCoord - rectRight;
    const float clipLimitX = Min(negSize, posSize);

    negSize                = (-MinVertScreenCoord) + rectTop;
    posSize                = MaxVertScreenCoord - rectBottom;
    const float clipLimitY = Min(negSize, posSize);

    // Calculate accumulated viewport rectangle scale factors
    const float xScale     = (rectRight - rectLeft) * 0.5f;
    const float yScale     = (rectBottom - rectTop) * 0.5f;
    centerRect.xClipFactor = (clipLimitX + xScale) / xScale;
    centerRect.yClipFactor = (clipLimitY + yScale) / yScale;

    return centerRect;
}

// =====================================================================================================================
// Writes the latest set of viewports to HW. It is illegal to call this if the viewports aren't dirty.
template <bool Pm4OptImmediate>
uint32* UniversalCmdBuffer::ValidateViewports(
    uint32* pDeCmdSpace)
{
    const ViewportParams& params = m_graphicsState.viewportState;
    PAL_ASSERT(m_graphicsState.dirtyFlags.viewports != 0);

    const uint32 viewportCount = (m_graphicsState.enableMultiViewport) ? params.count : 1;
    VportRegs    viewportRegs  = {};

    PAL_ASSERT((params.horzClipRatio    >= 1.0f) &&
               (params.horzDiscardRatio >= 1.0f) &&
               (params.vertClipRatio    >= 1.0f) &&
               (params.vertDiscardRatio >= 1.0f));

    viewportRegs.guardbandImg.paClGbHorzDiscAdj.f32All = params.horzDiscardRatio;
    viewportRegs.guardbandImg.paClGbVertDiscAdj.f32All = params.vertDiscardRatio;

    for (uint32 i = 0; i < viewportCount; i++)
    {
        const Viewport&         viewport        = params.viewports[i];
        VportScaleOffsetPm4Img* pScaleOffsetImg = &viewportRegs.scaleOffsetImgs[i];
        auto*                   pNggViewports   = &m_state.primShaderCullingCb.viewports[i];

        const float xScale = (viewport.width  * 0.5f);
        const float yScale = (viewport.height * 0.5f);

        pScaleOffsetImg->xScale.f32All  = xScale;
        pScaleOffsetImg->xOffset.f32All = (viewport.originX + xScale);

        pScaleOffsetImg->yScale.f32All  = yScale * (viewport.origin == PointOrigin::UpperLeft ? 1.0f : -1.0f);
        pScaleOffsetImg->yOffset.f32All = (viewport.originY + yScale);

        if (params.depthRange == DepthRange::NegativeOneToOne)
        {
            pScaleOffsetImg->zScale.f32All  = (viewport.maxDepth - viewport.minDepth) * 0.5f;
            pScaleOffsetImg->zOffset.f32All = (viewport.maxDepth + viewport.minDepth) * 0.5f;
        }
        else
        {
            pScaleOffsetImg->zScale.f32All  = (viewport.maxDepth - viewport.minDepth);
            pScaleOffsetImg->zOffset.f32All = viewport.minDepth;
        }

        pNggViewports->paClVportXOffset = pScaleOffsetImg->xOffset.u32All;
        pNggViewports->paClVportYOffset = pScaleOffsetImg->yOffset.u32All;

        pNggViewports->paClVportXScale  = pScaleOffsetImg->xScale.u32All;
        pNggViewports->paClVportYScale  = pScaleOffsetImg->yScale.u32All;
    }

    const VportCenterRect vpCenterRect = GetViewportsCenterAndScale();

    // Clients may pass specific clip ratios for perf/quality that *must* be used over our calculated clip factors as
    // long as they are < our clip factors
    viewportRegs.guardbandImg.paClGbHorzClipAdj.f32All = Min(vpCenterRect.xClipFactor, params.horzClipRatio);
    viewportRegs.guardbandImg.paClGbVertClipAdj.f32All = Min(vpCenterRect.yClipFactor, params.vertClipRatio);

    m_state.primShaderCullingCb.paClGbHorzClipAdj = viewportRegs.guardbandImg.paClGbHorzClipAdj.u32All;
    m_state.primShaderCullingCb.paClGbHorzDiscAdj = viewportRegs.guardbandImg.paClGbHorzDiscAdj.u32All;
    m_state.primShaderCullingCb.paClGbVertClipAdj = viewportRegs.guardbandImg.paClGbVertClipAdj.u32All;
    m_state.primShaderCullingCb.paClGbVertDiscAdj = viewportRegs.guardbandImg.paClGbVertDiscAdj.u32All;

    // Write accumulated rectangle's center coords to PA_SU_HARDWARE_SCREEN_OFFSET to center guardband correctly.
    // Without doing this, there is fewer potential guardband region below and to the right of the viewport than
    // above and to the left.
    viewportRegs.hwScreenOffset.bits.HW_SCREEN_OFFSET_X = static_cast<uint32>(vpCenterRect.centerX / 16.0f);
    viewportRegs.hwScreenOffset.bits.HW_SCREEN_OFFSET_Y = static_cast<uint32>(vpCenterRect.centerY / 16.0f);

    // On GFX11, bit 0 must be 0 if VRS_SURFACE_ENABLE or RATE_HINT_WRITE_BACK_ENABLE are set. Thus, we must ensure that
    // the LSB for both screen offsets is set to 0. We do this globally for GFX11, which will result in a slightly
    // improperly centered guarband, though it should not matter much for performance or correctness.
    if (IsGfx11(m_gfxIpLevel))
    {
        viewportRegs.hwScreenOffset.bits.HW_SCREEN_OFFSET_X &= 0xFFE;
        viewportRegs.hwScreenOffset.bits.HW_SCREEN_OFFSET_Y &= 0xFFE;
    }

    for (uint32 i = 0; i < viewportCount; i++)
    {
        const auto&         viewport    = params.viewports[i];
        VportZMinMaxPm4Img* pZMinMaxImg = reinterpret_cast<VportZMinMaxPm4Img*>(&viewportRegs.zMinMaxImgs[i]);

#if PAL_BUILD_SUPPORT_DEPTHCLAMPMODE_ZERO_TO_ONE
        if (static_cast<DepthClampMode>(m_graphicsState.depthClampMode) == DepthClampMode::ZeroToOne)
        {
            pZMinMaxImg->zMin.f32All = 0.0f;
            pZMinMaxImg->zMax.f32All = 1.0f;
        }
        else
#endif
        {
            pZMinMaxImg->zMin.f32All = Min(viewport.minDepth, viewport.maxDepth);
            pZMinMaxImg->zMax.f32All = Max(viewport.minDepth, viewport.maxDepth);
        }
    }

    const uint32 numScaleRegs   = VportRegs::NumScaleOffsetRegsPerVport * viewportCount;
    const uint32 numZMinMaxRegs = VportRegs::NumZMinMaxRegsPerVport     * viewportCount;

    pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs<Pm4OptImmediate>(mmPA_CL_GB_VERT_CLIP_ADJ,
                                                                        mmPA_CL_GB_HORZ_DISC_ADJ,
                                                                        &viewportRegs.guardbandImg,
                                                                        pDeCmdSpace);
    pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs<Pm4OptImmediate>(mmPA_CL_VPORT_XSCALE,
                                                                        mmPA_CL_VPORT_XSCALE + numScaleRegs - 1,
                                                                        &viewportRegs.scaleOffsetImgs[0],
                                                                        pDeCmdSpace);
    pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs<Pm4OptImmediate>(mmPA_SC_VPORT_ZMIN_0,
                                                                        mmPA_SC_VPORT_ZMIN_0 + numZMinMaxRegs - 1,
                                                                        &viewportRegs.zMinMaxImgs[0],
                                                                        pDeCmdSpace);
    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg<Pm4OptImmediate>(mmPA_SU_HARDWARE_SCREEN_OFFSET,
                                                                       viewportRegs.hwScreenOffset.u32All,
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
uint32* UniversalCmdBuffer::ValidateCbColorInfoAndBlendState(
    uint32* pDeCmdSpace)
{
    const auto dirtyFlags = m_graphicsState.dirtyFlags;

    // Should only be called if pipeline is dirty or blendState/colorTarget is changed.
    PAL_DEBUG_BUILD_ONLY_ASSERT(PipelineDirty || (StateDirty && (dirtyFlags.colorBlendState || dirtyFlags.colorTargetView)));

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
                pPipeline->NumColorTargets(),
                m_cachedSettings.blendOptimizationsEnable,
                &m_blendOpts[0],
                m_cbColorInfo);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 842
            const DynamicGraphicsState dynamicState = m_graphicsState.dynamicGraphicsInfo.dynamicState;
#else
            const DynamicGraphicsState dynamicState = m_graphicsState.dynamicState;
#endif

            const bool alphaToCoverage = dynamicState.enable.alphaToCoverageEnable
                                         ? dynamicState.alphaToCoverageEnable
                                         : pPipeline->AlphaToCoverageEanble();

            // If BlendState is changed, always need to check and modify the blendState when alphaToCoverage is in use.
            if (StateDirty && dirtyFlags.colorBlendState)
            {
                if (alphaToCoverage)
                {
                    pDeCmdSpace = pBlendState->HandleAlphaToCoverage(&m_deCmdStream, true, pDeCmdSpace);
                }
            }
            // If Only pipeline is changed, always adjust the blend state based on alphaToCoverage state.
            else if (PipelineDirty)
            {
                if (m_state.flags.drawTimeAlphaToCoverage != (alphaToCoverage ? 1u : 0u))
                {
                    pDeCmdSpace = pBlendState->HandleAlphaToCoverage(&m_deCmdStream, alphaToCoverage, pDeCmdSpace);
                }
            }

            m_state.flags.drawTimeAlphaToCoverage = alphaToCoverage ? 1u : 0u;
        }
    }

    uint32 cbColorInfoCheckMask = (m_state.flags.cbColorInfoDirtyRtv | cbColorInfoDirtyBlendOpt);

    if ((IsNested() == false) &&
        (m_cachedSettings.optimizeDepthOnlyFmt != 0) &&
        (PipelineDirty || (StateDirty && dirtyFlags.colorTargetView)))
    {
        // Since this only applies for RTV count = 0, we can assume the expected state is UNORM/INVALID
        // when this optimization is not required by the PSO. This is important if CB_TARGET_MASK[0] != 0
        if (m_graphicsState.bindTargets.colorTargetCount == 0)
        {
            const bool isRbPlusOptDepthOnly = (pPipeline != nullptr) ? pPipeline->CanRbPlusOptimizeDepthOnly() : false;
            m_cbColorInfo[0].bits.NUMBER_TYPE = isRbPlusOptDepthOnly ? Chip::NUMBER_FLOAT : Chip::NUMBER_UNORM;

            const Chip::ColorFormat format = isRbPlusOptDepthOnly ? Chip::COLOR_32 : Chip::COLOR_INVALID;

            if (IsGfx10(m_gfxIpLevel))
            {
                m_cbColorInfo[0].gfx10.FORMAT = format;
            }
            else
            {
                m_cbColorInfo[0].gfx11.FORMAT = format;
            }
        }

        // Dirtying of cbColorInfoCheckMask for this is handled by BindTargets and BindPipeline (cbColorInfoDirtyRtv).
    }

    if (cbColorInfoCheckMask != 0)
    {
        for (uint32 x : BitIter32(cbColorInfoCheckMask))
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
        m_leakCbColorInfoRtv |= m_state.flags.cbColorInfoDirtyRtv;

        m_state.flags.cbColorInfoDirtyRtv = 0;
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Validate DB_RENDER_OVERRIDE register. Depends on DSV, DepthClampOverride state and Pipeline.
uint32* UniversalCmdBuffer::ValidateDbRenderOverride(
    uint32* pDeCmdSpace)
{
    if (IsNested() == false)
    {
        // For normal case - we know all state, just write it if it has changed.
        if (m_prevDbRenderOverride.u32All != m_dbRenderOverride.u32All)
        {
            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmDB_RENDER_OVERRIDE,
                                                              m_dbRenderOverride.u32All,
                                                              pDeCmdSpace);
            m_prevDbRenderOverride.u32All = m_dbRenderOverride.u32All;
        }
    }
    else
    {
        // For nested - only update pipeline/depthclampoverride(BLT) and use RMW. DSV
        // dependent portion will be written @ BindTarget-time for DSV on Nested.
        if (m_graphicsState.pipelineState.dirtyFlags.pipeline || m_pipelineDynRegsDirty)
        {
            pDeCmdSpace = m_deCmdStream.WriteContextRegRmw(mmDB_RENDER_OVERRIDE,
                                                           PipelineDbRenderOverrideMask,
                                                           m_dbRenderOverride.u32All,
                                                           pDeCmdSpace);
        }
    }

    return pDeCmdSpace;
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
uint32* UniversalCmdBuffer::ValidatePaScAaConfig(
    uint32* pDeCmdSpace)
{
    {
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmPA_SC_AA_CONFIG,
                                                               m_paScAaConfigNew.u32All,
                                                               pDeCmdSpace);
    }

    m_paScAaConfigLast.u32All = m_paScAaConfigNew.u32All;
    return pDeCmdSpace;
}

// =====================================================================================================================
// Translates the supplied IA_MULTI_VGT_PARAM register to its equivalent GE_CNTL value
// Calculates the GE_CNTL register value.
template <bool IsNgg>
uint32 UniversalCmdBuffer::CalcGeCntl(
    bool                  usesLineStipple,
    regIA_MULTI_VGT_PARAM iaMultiVgtParam
    ) const
{
    const     auto*  pPalPipeline         = m_graphicsState.pipelineState.pPipeline;
    const     auto*  pPipeline            = static_cast<const GraphicsPipeline*>(pPalPipeline);
    const     bool   isTess               = IsTessEnabled();
    const     bool   isNggFastLaunch      = (pPipeline->FastLaunchMode() != GsFastLaunchMode::Disabled);
    const     bool   disableVertGrouping  = (m_cachedSettings.disableVertGrouping &&
                                             (isNggFastLaunch == false)           &&
                                             (pPipeline->NggSubgroupSize() == 0));
    constexpr uint32 VertGroupingDisabled = 256;

    regGE_CNTL  geCntl = {};

    uint32 primsPerSubgroup = 0;
    uint32 vertsPerSubgroup = 0;

    // For legacy GS on gfx10, GE_CNTL.PRIM_GRP_SIZE should match the programming of
    // VGT_GS_ONCHIP_CNTL.GS_PRIMS_PER_SUBGRP.
    const bool useVgtOnchipCntl        = ((IsNgg == false) && (IsGsEnabled() == false));
    const bool useVgtOnchipCntlForTess = (isTess && (IsGfx11(m_gfxIpLevel) == false));

    if (useVgtOnchipCntl || useVgtOnchipCntlForTess)
    {
        // PRIMGROUP_SIZE is zero-based (i.e., zero means one) but PRIM_GRP_SIZE is one based (i.e., one means one).
        primsPerSubgroup = iaMultiVgtParam.bits.PRIMGROUP_SIZE + 1;

        // Recomendation to disable VERT_GRP_SIZE is to set it to 256.
        vertsPerSubgroup = VertGroupingDisabled;
    }
    else if (isNggFastLaunch)
    {
        const regVGT_GS_ONCHIP_CNTL vgtGsOnchipCntl = pPipeline->VgtGsOnchipCntl();

        primsPerSubgroup = vgtGsOnchipCntl.bits.GS_PRIMS_PER_SUBGRP;
        vertsPerSubgroup = vgtGsOnchipCntl.bits.ES_VERTS_PER_SUBGRP;
    }
    else
    {
        const regVGT_GS_ONCHIP_CNTL vgtGsOnchipCntl = pPipeline->VgtGsOnchipCntl();

        primsPerSubgroup = vgtGsOnchipCntl.bits.GS_PRIMS_PER_SUBGRP;
        vertsPerSubgroup =
            (disableVertGrouping)                       ? VertGroupingDisabled :
            (m_cachedSettings.waClampGeCntlVertGrpSize) ? vgtGsOnchipCntl.bits.ES_VERTS_PER_SUBGRP - 5 :
                                                          vgtGsOnchipCntl.bits.ES_VERTS_PER_SUBGRP;

        // Zero is a legal value for VERT_GRP_SIZE. Other low values are illegal.
        if ((vertsPerSubgroup != 0) &&
            // the 29/24 limit was actually from navi2x and to make sure that Within that chunk it cannot break a
            // subgroup based on number of verts.
            (IsGfx11(m_gfxIpLevel) == false))
        {
            //  These numbers below come from the hardware restrictions.
            if (IsGfx103Plus(m_gfxIpLevel))
            {
                if (vertsPerSubgroup < 29)
                {
                    vertsPerSubgroup = 29;
                }
            }
            else if (IsGfx101(m_gfxIpLevel))
            {
                if (vertsPerSubgroup < 24)
                {
                    vertsPerSubgroup = 24;
                }
            }
        }
    }

    geCntl.gfx10.PRIM_GRP_SIZE = primsPerSubgroup;
    geCntl.gfx10.VERT_GRP_SIZE = vertsPerSubgroup;

    // Note that the only real case in production to use packet_to_one_pa = 1 is when using the PA line stipple mode
    // which requires the entire packet to be sent to a single PA.
    geCntl.bits.PACKET_TO_ONE_PA = usesLineStipple;

    // GFX11 renamed Gfx10::PRIM_GRP_SIZE and GFX10::VERT_GRP_SIZE to Gfx11::PRIMS_PER_SUBGRP and
    // Gfx11::VERTS_PER_SUBGRP.
    // A new field, Gfx11::PRIM_GRP_SIZE was added. This requires different programming from GFX10::PRIM_GRP_SIZE
    static_assert(Gfx10::GE_CNTL__PRIM_GRP_SIZE_MASK == Gfx11::GE_CNTL__PRIMS_PER_SUBGRP_MASK,
                  "Prims per subgroup not in the same place on GFX10 and GFX11");
    static_assert(Gfx10::GE_CNTL__VERT_GRP_SIZE_MASK == Gfx11::GE_CNTL__VERTS_PER_SUBGRP_MASK,
                  "Verts per subgroup not in the same place on GFX10 and GFX11");

    if (IsGfx11(m_gfxIpLevel))
    {
        PAL_ASSERT(pPipeline->GetPrimAmpFactor() > 0);

        // The legal range of values for the primGrpSize is [1, 256]
        const uint32 primGrpSize = Clamp(
            static_cast<uint32>(m_cachedSettings.primGrpSize / pPipeline->GetPrimAmpFactor()),
            1u,
            256u);

        geCntl.gfx11.PRIM_GRP_SIZE = primGrpSize;
        geCntl.gfx11.BREAK_PRIMGRP_AT_EOI = isTess;

        // GE's GCR(Graphics Cache Rinse) can impact performance, i.e. delay the eop_done & context_done signals to CP.
        geCntl.gfx11.GCR_DISABLE = (m_cachedSettings.geCntlGcrMode == Gfx11GeCntlGcrModeDisabled) ? 1 : 0;
    }
    else
    {
        //  ... "the only time break_wave_at_eoi is needed, is for primitive_id/patch_id with tessellation."
        //  ... "I think every DS requires a valid PatchId".
        geCntl.gfx10.BREAK_WAVE_AT_EOI = isTess;
    }

    return geCntl.u32All;
}

// =====================================================================================================================
// Update the HW state and write the necessary packets to push any changes to the HW. Returns the next unused DWORD
// in pDeCmdSpace.
template <bool Indexed, bool Indirect, bool Pm4OptImmediate>
uint32* UniversalCmdBuffer::ValidateDrawTimeHwState(
    regPA_SC_MODE_CNTL_1         paScModeCntl1,         // PA_SC_MODE_CNTL_1 register value.
    const Pm4::ValidateDrawInfo& drawInfo,              // Draw info
    uint32*                      pDeCmdSpace)           // Write new draw-engine commands here.
{
    if ((m_drawTimeHwState.geMultiPrimIbResetEn.u32All != m_geMultiPrimIbResetEn.u32All) ||
        (m_drawTimeHwState.valid.geMultiPrimIbResetEn == 0))
    {
        m_drawTimeHwState.geMultiPrimIbResetEn.u32All = m_geMultiPrimIbResetEn.u32All;
        m_drawTimeHwState.valid.geMultiPrimIbResetEn  = 1;

        pDeCmdSpace = m_deCmdStream.WriteSetOneConfigReg(mmGE_MULTI_PRIM_IB_RESET_EN,
                                                         m_geMultiPrimIbResetEn.u32All,
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

    if (m_drawIndexReg != UserDataNotMapped)
    {
        if (Indirect && drawInfo.multiIndirectDraw)
        {
            // If the active pipeline uses the draw index VS input value, then the PM4 draw packet to issue the multi
            // draw will blow-away the SPI user-data register used to pass that value to the shader.
            m_drawTimeHwState.valid.drawIndex = 0;
        }
        else if ((m_drawTimeHwState.drawIndex != drawInfo.drawIndex) || (m_drawTimeHwState.valid.drawIndex == 0))
        {
            m_drawTimeHwState.drawIndex = drawInfo.drawIndex;
            m_drawTimeHwState.valid.drawIndex = 1;
            pDeCmdSpace = SetUserSgprReg<ShaderGraphics>(m_drawIndexReg,
                                                         drawInfo.drawIndex,
                                                         false,
                                                         pDeCmdSpace);
        }
    }
    const uint16 colorExpRegAddr = m_pSignatureGfx->colorExportAddr;
    // Write the color export shader entry user data register.
    if (colorExpRegAddr != UserDataNotMapped)
    {
        auto pPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 842
        const DynamicGraphicsState dynamicState = m_graphicsState.dynamicGraphicsInfo.dynamicState;
#else
        const DynamicGraphicsState dynamicState = m_graphicsState.dynamicState;
#endif
        const bool dynamicDualSourceBlendEnabled = dynamicState.dualSourceBlendEnable &&
                                                   dynamicState.enable.dualSourceBlendEnable;
        pDeCmdSpace = SetUserSgprReg<ShaderGraphics>(colorExpRegAddr,
                                                     pPipeline->GetColorExportAddr(
                                                         dynamicDualSourceBlendEnabled ?
                                                         ColorExportShaderType::DualSourceBlendEnable :
                                                         ColorExportShaderType::Default),
                                                     false,
                                                     pDeCmdSpace);
    }

    const bool disableInstancePacking =
        m_workaroundState.DisableInstancePacking<Indirect>(m_graphicsState.inputAssemblyState.topology,
                                                           drawInfo.instanceCount,
                                                           NumActiveQueries(QueryPoolType::PipelineStats));

    // Write the INDEX_TYPE packet.
    // We might need to write this outside of indexed draws (for instance, on a change of NGG <-> Legacy pipeline).
    if ((m_drawTimeHwState.dirty.indexType != 0)                                                               ||
        (m_vgtDmaIndexType.gfx103Plus.DISABLE_INSTANCE_PACKING != static_cast<uint32>(disableInstancePacking)) ||
        (Indexed && (m_drawTimeHwState.dirty.indexedIndexType != 0)))
    {
        m_drawTimeHwState.dirty.indexType        = 0;
        m_drawTimeHwState.dirty.indexedIndexType = 0;

        if (IsGfx103Plus(*(m_device.Parent())))
        {
            m_vgtDmaIndexType.gfx103Plus.DISABLE_INSTANCE_PACKING = disableInstancePacking;
        }

        pDeCmdSpace += m_cmdUtil.BuildIndexType(m_vgtDmaIndexType.u32All, pDeCmdSpace);
    }

    if (Indexed)
    {
        // Note that leakFlags.iaState implies an IB has been bound.
        if (m_graphicsState.leakFlags.iaState == 1)
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
        const uint16 vertexOffsetRegAddr = GetVertexOffsetRegAddr();
        // Write the vertex offset user data register.
        if (((m_drawTimeHwState.vertexOffset != drawInfo.firstVertex) ||
            (m_drawTimeHwState.valid.vertexOffset == 0)) &&
            (vertexOffsetRegAddr != UserDataNotMapped))
        {
            m_drawTimeHwState.vertexOffset       = drawInfo.firstVertex;
            m_drawTimeHwState.valid.vertexOffset = 1;

            pDeCmdSpace = SetUserSgprReg<ShaderGraphics>(vertexOffsetRegAddr,
                                                         drawInfo.firstVertex,
                                                         false,
                                                         pDeCmdSpace);
        }

        // Write the instance offset user data register.
        if (((m_drawTimeHwState.instanceOffset != drawInfo.firstInstance) ||
            (m_drawTimeHwState.valid.instanceOffset == 0)) &&
            (vertexOffsetRegAddr != UserDataNotMapped))
        {
            m_drawTimeHwState.instanceOffset       = drawInfo.firstInstance;
            m_drawTimeHwState.valid.instanceOffset = 1;

            pDeCmdSpace = SetUserSgprReg<ShaderGraphics>(vertexOffsetRegAddr + 1,
                                                         drawInfo.firstInstance,
                                                         false,
                                                         pDeCmdSpace);
        }

        // Write the NUM_INSTANCES packet.
        if ((m_drawTimeHwState.numInstances != drawInfo.instanceCount) || (m_drawTimeHwState.valid.numInstances == 0))
        {
            m_drawTimeHwState.numInstances       = drawInfo.instanceCount;
            m_drawTimeHwState.valid.numInstances = 1;

            pDeCmdSpace += m_device.CmdUtil().BuildNumInstances(drawInfo.instanceCount, pDeCmdSpace);
        }
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Performs dispatch-time dirty state validation for Task+Mesh pipelines.
void UniversalCmdBuffer::ValidateTaskMeshDispatch(
    gpusize      indirectGpuVirtAddr,
    DispatchDims size)
{
    const auto* pHybridPipeline =
        static_cast<const HybridGraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const auto& taskSignature = pHybridPipeline->GetTaskSignature();

    ComputeState tempComputeState                      = m_computeState;
    tempComputeState.pipelineState.pPipeline           = pHybridPipeline;
    tempComputeState.pipelineState.apiPsoHash          = m_graphicsState.pipelineState.apiPsoHash;
    tempComputeState.pipelineState.dirtyFlags.pipeline = 1;

    // Copy the gfx user-data entries on to this temporary ComputeState.
    memcpy(&tempComputeState.csUserDataEntries.entries,
           &(m_graphicsState.gfxUserDataEntries),
           sizeof(uint32) * taskSignature.userDataLimit);

    // Mark compute user data entries as dirty so that we are guaranteed to write them.
    memset(&tempComputeState.csUserDataEntries.dirty, -1, sizeof(tempComputeState.csUserDataEntries.dirty));

    ValidateDispatchPalAbi(&tempComputeState,
                           static_cast<CmdStream*>(m_pAceCmdStream),
                           indirectGpuVirtAddr,
                           size);
}

// =====================================================================================================================
// Performs PAL ABI dispatch-time dirty state validation.
void UniversalCmdBuffer::ValidateDispatchPalAbi(
    ComputeState* pComputeState,
    CmdStream*    pCmdStream,
    gpusize       indirectGpuVirtAddr,
    DispatchDims  logicalSize)
{
#if PAL_DEVELOPER_BUILD
    uint32 startingCmdLen = 0;
    uint32 userDataCmdLen = 0;
    if (m_cachedSettings.enablePm4Instrumentation != 0)
    {
        // GetUsedSize() is not accurate if called inside a Reserve/Commit block.
        startingCmdLen = GetUsedSize(CommandDataAlloc);
    }
#endif

    const bool onAce = (pCmdStream == m_pAceCmdStream);
    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    UserDataTableState*             pUserDataTable = &m_spillTable.stateCs;
    const ComputePipelineSignature* pNewSignature  = m_pSignatureCs;

    if (pComputeState->pipelineState.dirtyFlags.pipeline)
    {
        const auto*const pPrevSignature = m_pSignatureCs;
        if (pComputeState->pipelineState.pPipeline->IsTaskShaderEnabled())
        {
            // A pipeline that has a task shader bound is a Gfx9::HybridGraphicsPipeline. We need to go through
            // the regular compute dispatch validation path, but using the gfx user-data. We do not update the
            // UniversalCmdBuffer owned CS signature as the caller is expected to pass in a temporary ComputeState.
            const auto*const pNewPipeline =
                static_cast<const HybridGraphicsPipeline*>(pComputeState->pipelineState.pPipeline);

            pCmdSpace = pNewPipeline->WriteTaskCommands(pCmdStream,
                                                        pCmdSpace,
                                                        pComputeState->dynamicCsInfo,
                                                        m_buildFlags.prefetchShaders);

            pNewSignature  = &pNewPipeline->GetTaskSignature();
            pUserDataTable = &m_spillTable.stateGfx;

            const uint16 taskDispatchIdxReg = pNewSignature->dispatchIndexRegAddr;
            if (taskDispatchIdxReg != UserDataNotMapped)
            {
                // Initialize the taskDispatchIdx to 0, especially for direct dispatch
                pCmdSpace = SetUserSgprReg<ShaderCompute>(taskDispatchIdxReg, 0, onAce, pCmdSpace);
            }
        }
        else
        {
            const auto*const pNewPipeline = static_cast<const ComputePipeline*>(pComputeState->pipelineState.pPipeline);

            pCmdSpace = pNewPipeline->WriteCommands(pCmdStream,
                                                    pCmdSpace,
                                                    pComputeState->dynamicCsInfo,
                                                    m_buildFlags.prefetchShaders);

            m_pSignatureCs = &pNewPipeline->Signature();
            pNewSignature  = m_pSignatureCs;
            pUserDataTable = &m_spillTable.stateCs;
        }

#if PAL_DEVELOPER_BUILD
        if (m_cachedSettings.enablePm4Instrumentation != 0)
        {
            // GetUsedSize() is not accurate if called inside a Reserve/Commit block.
            pCmdStream->CommitCommands(pCmdSpace);
            const uint32 pipelineCmdLen = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
            m_device.DescribeBindPipelineValidation(this, pipelineCmdLen);
            startingCmdLen += pipelineCmdLen;
            pCmdSpace       = pCmdStream->ReserveCommands();
        }
#endif

        pCmdSpace = ValidateComputeUserData<true>(this,
                                                  pUserDataTable,
                                                  &pComputeState->csUserDataEntries,
                                                  pCmdStream,
                                                  pPrevSignature,
                                                  pNewSignature,
                                                  pCmdSpace);
    }
    else
    {
        pCmdSpace = ValidateComputeUserData<false>(this,
                                                   pUserDataTable,
                                                   &pComputeState->csUserDataEntries,
                                                   pCmdStream,
                                                   nullptr,
                                                   pNewSignature,
                                                   pCmdSpace);
    }

#if PAL_DEVELOPER_BUILD
    if (m_cachedSettings.enablePm4Instrumentation != 0)
    {
        // GetUsedSize() is not accurate if called inside a Reserve/Commit block.
        pCmdStream->CommitCommands(pCmdSpace);
        userDataCmdLen  = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
        startingCmdLen += userDataCmdLen;
        pCmdSpace       = pCmdStream->ReserveCommands();
    }
#endif

    pComputeState->pipelineState.dirtyFlags.u32All = 0;

    if (pNewSignature->numWorkGroupsRegAddr != UserDataNotMapped)
    {
        // Indirect Dispatches by definition have the number of thread-groups to launch stored in GPU memory at the
        // specified address.  However, for direct Dispatches, we must allocate some embedded memory to store this
        // information.
        if (indirectGpuVirtAddr == 0uLL) // This is a direct Dispatch.
        {
            *reinterpret_cast<DispatchDims*>(CmdAllocateEmbeddedData(3, 4, &indirectGpuVirtAddr)) = logicalSize;
        }

        pCmdSpace = SetSeqUserSgprRegs<ShaderCompute>(pNewSignature->numWorkGroupsRegAddr,
                                                      (pNewSignature->numWorkGroupsRegAddr + 1),
                                                      &indirectGpuVirtAddr,
                                                      onAce,
                                                      pCmdSpace);
    }

    if (m_numValidUserEntriesCs > 0)
    {
        pCmdSpace = WritePackedUserDataEntriesToSgprs<ShaderCompute>(pCmdSpace);
    }

#if PAL_DEVELOPER_BUILD
    if (m_cachedSettings.enablePm4Instrumentation != 0)
    {
        // GetUsedSize() is not accurate if called inside a Reserve/Commit block.
        pCmdStream->CommitCommands(pCmdSpace);
        const uint32 miscCmdLen = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
        pCmdSpace               = pCmdStream->ReserveCommands();

        m_device.DescribeDrawDispatchValidation(this, userDataCmdLen, miscCmdLen);
    }
#endif

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Performs HSA ABI dispatch-time dirty state validation.
void UniversalCmdBuffer::ValidateDispatchHsaAbi(
    ComputeState* pComputeState,
    CmdStream*    pCmdStream,
    DispatchDims  offset,
    DispatchDims  logicalSize)
{
#if PAL_DEVELOPER_BUILD
    uint32 startingCmdLen = 0;
    uint32 userDataCmdLen = 0;
    if (m_cachedSettings.enablePm4Instrumentation != 0)
    {
        // GetUsedSize() is not accurate if called inside a Reserve/Commit block.
        startingCmdLen = GetUsedSize(CommandDataAlloc);
    }
#endif

    uint32*          pCmdSpace = pCmdStream->ReserveCommands();
    const auto*const pPipeline = static_cast<const ComputePipeline*>(pComputeState->pipelineState.pPipeline);

    if (pComputeState->pipelineState.dirtyFlags.pipeline)
    {
        // We don't expect any HSA ABI pipelines to support task shaders.
        PAL_ASSERT(pPipeline->IsTaskShaderEnabled() == false);

        pCmdSpace = pPipeline->WriteCommands(pCmdStream,
                                             pCmdSpace,
                                             pComputeState->dynamicCsInfo,
                                             m_buildFlags.prefetchShaders);

        m_pSignatureCs = &pPipeline->Signature();
    }

#if PAL_DEVELOPER_BUILD
    if (m_cachedSettings.enablePm4Instrumentation != 0)
    {
        // GetUsedSize() is not accurate if called inside a Reserve/Commit block.
        pCmdStream->CommitCommands(pCmdSpace);
        const uint32 pipelineCmdLen = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
        m_device.DescribeBindPipelineValidation(this, pipelineCmdLen);
        startingCmdLen += pipelineCmdLen;
        pCmdSpace       = pCmdStream->ReserveCommands();
    }
#endif

    // PAL thinks in terms of threadgroups but the HSA ABI thinks in terms of global threads, we need to convert.
    const DispatchDims threads = pPipeline->ThreadsPerGroupXyz();

    offset *= threads;
    const DispatchDims logicalSizeInWorkItems = logicalSize * threads;

    // Now we write the required SGPRs. These depend on per-dispatch state so we don't have dirty bit tracking.
    const HsaAbi::CodeObjectMetadata&        metadata    = pPipeline->HsaMetadata();
    const llvm::amdhsa::kernel_descriptor_t& desc        = pPipeline->KernelDescriptor();
    const GpuChipProperties&                 deviceProps = m_device.Parent()->ChipProperties();

    uint32 startReg = mmCOMPUTE_USER_DATA_0;

    // PAL writes COMPUTE_USER_DATA_0 in the queue context preeamble when resuming from MCBP preemption
    // This will clobber the shadowed user_data_0 value which points to the kernel arguments buffer.
    // RS64 ucode does not replay the preamble streams on resume so MCBP works!
    bool disableMcbp = (deviceProps.pfpUcodeVersion < Gfx11Rs64MinPfpUcodeVersion);

    // Many HSA ELFs request private segment buffer registers, but never actually use them. Space is reserved to
    // adhere to initialization order but will be unset as we do not support scratch space in this execution path.
    if (TestAnyFlagSet(desc.kernel_code_properties, AMD_KERNEL_CODE_PROPERTIES_ENABLE_SGPR_PRIVATE_SEGMENT_BUFFER))
    {
        startReg += 4;
        // When a private segment buffer is requested, the kernel argument buffer pointer will not reside in
        // user_data_0, and so preemption can safely occur.
        disableMcbp = false;
    }

    const bool onAce = (pCmdStream == m_pAceCmdStream);

    if (disableMcbp && (onAce == false))
    {
        pCmdStream->DisablePreemption();
    }

    if (TestAnyFlagSet(desc.kernel_code_properties, AMD_KERNEL_CODE_PROPERTIES_ENABLE_SGPR_DISPATCH_PTR))
    {
        // Fake an AQL dispatch packet for the shader to read.
        gpusize aqlPacketGpu  = 0;
        auto*const pAqlPacket = reinterpret_cast<hsa_kernel_dispatch_packet_t*>(
                                CmdAllocateEmbeddedData(sizeof(hsa_kernel_dispatch_packet_t) / sizeof(uint32),
                                                        1,
                                                        &aqlPacketGpu));

        // Zero everything out then fill in certain fields the shader is likely to read.
        memset(pAqlPacket, 0, sizeof(hsa_kernel_dispatch_packet_t));

        pAqlPacket->workgroup_size_x     = static_cast<uint16>(threads.x);
        pAqlPacket->workgroup_size_y     = static_cast<uint16>(threads.y);
        pAqlPacket->workgroup_size_z     = static_cast<uint16>(threads.z);
        pAqlPacket->grid_size_x          = logicalSizeInWorkItems.x;
        pAqlPacket->grid_size_y          = logicalSizeInWorkItems.y;
        pAqlPacket->grid_size_z          = logicalSizeInWorkItems.z;
        pAqlPacket->private_segment_size = metadata.PrivateSegmentFixedSize();
        pAqlPacket->group_segment_size   = ((m_computeState.dynamicCsInfo.ldsBytesPerTg > 0)
                                                ? m_computeState.dynamicCsInfo.ldsBytesPerTg
                                                : metadata.GroupSegmentFixedSize());

        pCmdSpace = SetSeqUserSgprRegs<ShaderCompute>(startReg, startReg + 1, &aqlPacketGpu, onAce, pCmdSpace);
        startReg += 2;
    }

    if (TestAnyFlagSet(desc.kernel_code_properties, AMD_KERNEL_CODE_PROPERTIES_ENABLE_SGPR_KERNARG_SEGMENT_PTR))
    {
        // Copy the kernel argument buffer into GPU memory.
        gpusize      gpuVa          = 0;
        const uint32 allocSize      = NumBytesToNumDwords(metadata.KernargSegmentSize());
        const uint32 allocAlign     = NumBytesToNumDwords(metadata.KernargSegmentAlign());
        uint8*const  pParams        = reinterpret_cast<uint8*>(CmdAllocateEmbeddedData(allocSize, allocAlign, &gpuVa));
        const uint16 threadsX       = static_cast<uint16>(threads.x);
        const uint16 threadsY       = static_cast<uint16>(threads.y);
        const uint16 threadsZ       = static_cast<uint16>(threads.z);
        uint16 remainderSize        = 0; // no incomplete workgroups supported at this time.
        const uint32 dimensionality = (logicalSize.x > 1) + (logicalSize.y > 1) + (logicalSize.z > 1);

        memcpy(pParams, m_computeState.pKernelArguments, metadata.KernargSegmentSize());

        // The global offsets are always zero, except in CmdDispatchOffset where they are dispatch-time values.
        // This could be moved out into CmdDispatchOffset if the overhead is too much but we'd have to return
        // out some extra state to make that work.
        for (uint32 idx = 0; idx < metadata.NumArguments(); ++idx)
        {
            const HsaAbi::KernelArgument& arg = metadata.Arguments()[idx];
            switch (arg.valueKind)
            {
                case HsaAbi::ValueKind::HiddenGlobalOffsetX:
                    memcpy(pParams + arg.offset, &offset.x, Min<size_t>(sizeof(offset.x), arg.size));
                    break;
                case HsaAbi::ValueKind::HiddenGlobalOffsetY:
                    memcpy(pParams + arg.offset, &offset.y, Min<size_t>(sizeof(offset.y), arg.size));
                    break;
                case HsaAbi::ValueKind::HiddenGlobalOffsetZ:
                    memcpy(pParams + arg.offset, &offset.z, Min<size_t>(sizeof(offset.z), arg.size));
                    break;
                case HsaAbi::ValueKind::HiddenBlockCountX:
                    memcpy(pParams + arg.offset, &logicalSize.x, Min<size_t>(sizeof(logicalSize.x), arg.size));
                    break;
                case HsaAbi::ValueKind::HiddenBlockCountY:
                    memcpy(pParams + arg.offset, &logicalSize.y, Min<size_t>(sizeof(logicalSize.y), arg.size));
                    break;
                case HsaAbi::ValueKind::HiddenBlockCountZ:
                    memcpy(pParams + arg.offset, &logicalSize.z, Min<size_t>(sizeof(logicalSize.z), arg.size));
                    break;
                case HsaAbi::ValueKind::HiddenGroupSizeX:
                    memcpy(pParams + arg.offset, &threadsX, Min<size_t>(sizeof(threadsX), arg.size));
                    break;
                case HsaAbi::ValueKind::HiddenGroupSizeY:
                    memcpy(pParams + arg.offset, &threadsY, Min<size_t>(sizeof(threadsY), arg.size));
                    break;
                case HsaAbi::ValueKind::HiddenGroupSizeZ:
                    memcpy(pParams + arg.offset, &threadsZ, Min<size_t>(sizeof(threadsZ), arg.size));
                    break;
                case HsaAbi::ValueKind::HiddenRemainderX:
                    memcpy(pParams + arg.offset, &remainderSize, Min<size_t>(sizeof(remainderSize), arg.size));
                    break;
                case HsaAbi::ValueKind::HiddenRemainderY:
                    memcpy(pParams + arg.offset, &remainderSize, Min<size_t>(sizeof(remainderSize), arg.size));
                    break;
                case HsaAbi::ValueKind::HiddenRemainderZ:
                    memcpy(pParams + arg.offset, &remainderSize, Min<size_t>(sizeof(remainderSize), arg.size));
                    break;
                case HsaAbi::ValueKind::HiddenGridDims:
                    memcpy(pParams + arg.offset, &dimensionality, Min<size_t>(sizeof(dimensionality), arg.size));
                    break;
                case HsaAbi::ValueKind::ByValue:
                case HsaAbi::ValueKind::GlobalBuffer:
                    break; // these are handled by kernargs
                case HsaAbi::ValueKind::HiddenNone:
                    break; // avoid the assert in this case
                default:
                    PAL_ASSERT_ALWAYS();

            }

        }

        pCmdSpace = SetSeqUserSgprRegs<ShaderCompute>(startReg, startReg + 1, &gpuVa, onAce, pCmdSpace);
        startReg += 2;
    }

    if (TestAnyFlagSet(desc.kernel_code_properties, AMD_KERNEL_CODE_PROPERTIES_ENABLE_SGPR_DISPATCH_ID))
    {
        // This feature may be enabled as a side effect of indirect calls.
        // However, the compiler team confirmed that the dispatch id itself is not used,
        // so safe to send 0 for each dispatch.
        constexpr uint32 DispatchId[2] = {};
        pCmdSpace = SetSeqUserSgprRegs<ShaderCompute>(startReg, startReg + 1, &DispatchId, onAce, pCmdSpace);
        startReg += 2;
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    regCOMPUTE_PGM_RSRC2 computePgmRsrc2 = {};
    computePgmRsrc2.u32All = desc.compute_pgm_rsrc2;

    PAL_ASSERT((startReg - mmCOMPUTE_USER_DATA_0) <= computePgmRsrc2.bitfields.USER_SGPR);
#endif

    if (m_numValidUserEntriesCs > 0)
    {
        pCmdSpace = WritePackedUserDataEntriesToSgprs<ShaderCompute>(pCmdSpace);
    }

    pComputeState->pipelineState.dirtyFlags.u32All = 0;

    PAL_ASSERT(m_pSignatureCs->numWorkGroupsRegAddr == UserDataNotMapped);

#if PAL_DEVELOPER_BUILD
    if (m_cachedSettings.enablePm4Instrumentation != 0)
    {
        // GetUsedSize() is not accurate if called inside a Reserve/Commit block.
        pCmdStream->CommitCommands(pCmdSpace);
        const uint32 miscCmdLen = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
        pCmdSpace               = pCmdStream->ReserveCommands();

        m_device.DescribeDrawDispatchValidation(this, userDataCmdLen, miscCmdLen);
    }
#endif

    pCmdStream->CommitCommands(pCmdSpace);
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
        else if ((queryType == QueryPoolType::PipelineStats) ||
                 (queryType == QueryPoolType::StreamoutStats))
        {
            if (m_cachedSettings.disablePreamblePipelineStats)
            {
                // If pipeline stats are disabled in preamble, need to activate first queries of type PipelineStats
                ActivateQueryType(queryType);
            }
            if (queryType == QueryPoolType::StreamoutStats)
            {
                m_graphicsState.dirtyFlags.streamoutStatsQuery = 1;
            }
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
            m_graphicsState.dirtyFlags.streamoutStatsQuery = 1;
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
    const gpusize streamoutDataAddr = SwStreamoutDataAddr();
    uint32*       pDeCmdSpace       = m_deCmdStream.ReserveCommands();

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
                                                  streamoutDataAddr,
                                                  pDeCmdSpace);
        }
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSaveBufferFilledSizes(
    const gpusize (&gpuVirtAddr)[MaxStreamOutTargets])
{
    const gpusize streamoutDataAddr = SwStreamoutDataAddr();
    uint32*       pDeCmdSpace       = m_deCmdStream.ReserveCommands();

    // The VGT's internal stream output state needs to be flushed before writing the buffer filled size counters
    // to memory.
    pDeCmdSpace = FlushStreamOut(pDeCmdSpace);

    for (uint32 idx = 0; idx < MaxStreamOutTargets; ++idx)
    {
        if (gpuVirtAddr[idx] != 0)
        {
            if (SupportsSwStrmout())
            {
                constexpr uint32 SourceSelect =
                    source_select__pfp_strmout_buffer_update__read_strmout_buffer_filled_size;
                pDeCmdSpace += CmdUtil::BuildStrmoutBufferUpdate(idx,
                                                                 SourceSelect,
                                                                 0,
                                                                 0uLL,
                                                                 0uLL,
                                                                 streamoutDataAddr,
                                                                 pDeCmdSpace);
            }

            pDeCmdSpace += CmdUtil::BuildStrmoutBufferUpdate(idx,
                                                             source_select__pfp_strmout_buffer_update__none__GFX10,
                                                             0,
                                                             gpuVirtAddr[idx],
                                                             0uLL,
                                                             streamoutDataAddr,
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
    PAL_ASSERT(bufferId < MaxStreamOutTargets);

    const gpusize streamoutDataAddr = SwStreamoutDataAddr();
    uint32*       pDeCmdSpace       = m_deCmdStream.ReserveCommands();

    pDeCmdSpace += CmdUtil::BuildStrmoutBufferUpdate(bufferId,
                                                     source_select__pfp_strmout_buffer_update__use_buffer_offset,
                                                     offset,
                                                     0uLL,
                                                     0uLL,
                                                     streamoutDataAddr,
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
    const auto& pool = static_cast<const QueryPool&>(queryPool);

    if (pool.RequiresSamplingFromGangedAce() && (m_pAceCmdStream == nullptr))
    {
        // Some types of queries require using the ganged ACE stream _if_ work launched after the query has begun
        // ends up using the ACE.  However, we don't want to create the ganged ACE stream if no "real" work will
        // actually use it.  So track those queries so that the begin operation can be applied if/when the ganged
        // ACE is initialized.
        if (m_deferredPipelineStatsQueries.PushBack(ActiveQueryState{&pool, slot}) != Result::Success)
        {
            NotifyAllocFailure();
        }
    }

    pool.Begin(this, &m_deCmdStream, m_pAceCmdStream, queryType, slot, flags);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdEndQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot)
{
    const auto& pool = static_cast<const QueryPool&>(queryPool);

    if (pool.RequiresSamplingFromGangedAce() && (m_pAceCmdStream == nullptr))
    {
        // If this query pool was tracked so that ganged ACE portions of its Begin() operation can be applied when
        // the ganged ACE was initialized, _and_ the ganged ACE never actually ended up being used, then we must
        // remove the pool from our tracking so that it doesn't get overwritten sometime later if the ACE is needed
        // later on in this command buffer.
        for (uint32 i = 0; i < m_deferredPipelineStatsQueries.NumElements(); ++i)
        {
            const ActiveQueryState& state = m_deferredPipelineStatsQueries.At(i);
            if ((state.pQueryPool == &pool) && (state.slot == slot))
            {
                m_deferredPipelineStatsQueries.Erase(i);
                break;
            }
        }
    }

    pool.End(this, &m_deCmdStream, m_pAceCmdStream, queryType, slot);
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
    case QueryPoolType::PipelineStats:
        {
            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
            pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PIPELINESTAT_STOP, EngineTypeUniversal, pDeCmdSpace);
            m_deCmdStream.CommitCommands(pDeCmdSpace);
        }
        break;

    case QueryPoolType::StreamoutStats:
        // There is no event to stop sampling of streamout stats when streamout is emulated in software.
        if (SupportsSwStrmout() == false)
        {
            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
            pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PIPELINESTAT_STOP, EngineTypeUniversal, pDeCmdSpace);
            m_deCmdStream.CommitCommands(pDeCmdSpace);
        }
        break;

    case QueryPoolType::Occlusion:
        // Due to apps tendencies to do sequences of {BeginQuery, Draw, EndQuery}, query validation
        // is delayed until draw time when we know the the required query state.
        m_graphicsState.dirtyFlags.occlusionQueryActive = m_state.flags.occlusionQueriesActive;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    // Call base function
    Pal::Pm4::UniversalCmdBuffer::DeactivateQueryType(queryPoolType);
}

// =====================================================================================================================
// Enables the specified query type.
void UniversalCmdBuffer::ActivateQueryType(
    QueryPoolType queryPoolType)
{
    switch (queryPoolType)
    {
    case QueryPoolType::PipelineStats:
        {
            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
            pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PIPELINESTAT_START, EngineTypeUniversal, pDeCmdSpace);
            m_deCmdStream.CommitCommands(pDeCmdSpace);
        }
        break;

    case QueryPoolType::StreamoutStats:
        // There is no event to start sampling of streamout stats when streamout is emulated in software.
        if (SupportsSwStrmout() == false)
        {
            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
            pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PIPELINESTAT_START, EngineTypeUniversal, pDeCmdSpace);
            m_deCmdStream.CommitCommands(pDeCmdSpace);
        }
        break;

    case QueryPoolType::Occlusion:
        // Due to apps tendencies to do sequences of {BeginQuery, Draw, EndQuery}, query validation
        // is delayed until draw time when we know the the required query state.
        m_graphicsState.dirtyFlags.occlusionQueryActive = (m_state.flags.occlusionQueriesActive == 0);
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    // Call base class function
    Pal::Pm4::UniversalCmdBuffer::ActivateQueryType(queryPoolType);
}

// =====================================================================================================================
// Updates the DB_COUNT_CONTROL register state based on the current the MSAA and occlusion query state.
template <bool Pm4OptImmediate>
uint32* UniversalCmdBuffer::UpdateDbCountControl(
    uint32  log2SampleRate,  // MSAA sample rate associated with a bound MSAA state object
    uint32* pDeCmdSpace)
{
    const bool hasActiveQuery = IsQueryActive(QueryPoolType::Occlusion) &&
                                (NumActiveQueries(QueryPoolType::Occlusion) != 0);

    regDB_COUNT_CONTROL dbCountControl      = {0};
    dbCountControl.bits.SAMPLE_RATE         = log2SampleRate;
    dbCountControl.bits.SLICE_EVEN_ENABLE   = 1;
    dbCountControl.bits.SLICE_ODD_ENABLE    = 1;

    if (IsNested() &&
        m_graphicsState.inheritedState.stateFlags.occlusionQuery &&
        (hasActiveQuery == false))
    {
        // In a nested command buffer, the number of active queries is unknown because the caller may have some
        // number of active queries when executing the nested command buffer. In this case, we must make sure that
        // update the sample count without disabling occlusion queries.
        pDeCmdSpace = m_deCmdStream.WriteContextRegRmw<Pm4OptImmediate>(mmDB_COUNT_CONTROL,
                                                                        DB_COUNT_CONTROL__SAMPLE_RATE_MASK,
                                                                        dbCountControl.u32All,
                                                                        pDeCmdSpace);
    }
    else
    {
        if (hasActiveQuery)
        {
            // Since 8xx, the ZPass count controls have moved to a separate register call DB_COUNT_CONTROL.
            // PERFECT_ZPASS_COUNTS forces all partially covered tiles to be detail walked, not setting it will count
            // all HiZ passed tiles as 8x#samples worth of zpasses. Therefore in order for vis queries to get the right
            // zpass counts, PERFECT_ZPASS_COUNTS should be set to 1, but this will hurt performance when z passing
            // geometry does not actually write anything (ZFail Shadow volumes for example).

            // Hardware does not enable depth testing when issuing a depth only render pass with depth writes disabled.
            // Unfortunately this corner case prevents depth tiles from being generated and when setting
            // PERFECT_ZPASS_COUNTS = 0, the hardware relies on counting at the tile granularity for binary occlusion
            // queries. With the depth test disabled and PERFECT_ZPASS_COUNTS = 0, there will be 0 tiles generated which
            // will cause the binary occlusion test to always generate depth pass counts of 0.
            // Setting PERFECT_ZPASS_COUNTS = 1 forces tile generation and reliable binary occlusion query results.
            dbCountControl.bits.PERFECT_ZPASS_COUNTS    = 1;
            dbCountControl.bits.ZPASS_ENABLE            = 1;
        }

        // Even if ZPASS_ENABLE = 0, we should set this flag or it will force OREO to use blend mode in the late_z path.
        // There should be no impact on gfx10 so we did the simple thing and made this a general change.
        // This field must be set to match GFX9's PERFECT_ZPASS_COUNTS behavior.
        dbCountControl.bits.DISABLE_CONSERVATIVE_ZPASS_COUNTS = 1;

        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg<Pm4OptImmediate>(mmDB_COUNT_CONTROL,
                                                                           dbCountControl.u32All,
                                                                           pDeCmdSpace);
    }

    m_state.flags.occlusionQueriesActive = hasActiveQuery;

    return pDeCmdSpace;
}

// =====================================================================================================================
// Returns true if the current command buffer state requires WD_SWITCH_ON_EOP=1, or if a HW workaround necessitates it.
bool UniversalCmdBuffer::ForceWdSwitchOnEop(
    const Pm4::ValidateDrawInfo& drawInfo
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

    const PrimitiveTopology primTopology            = m_graphicsState.inputAssemblyState.topology;
    const bool              primitiveRestartEnabled = m_graphicsState.inputAssemblyState.primitiveRestartEnable;
    const bool              restartPrimsCheck       = (primTopology != PrimitiveTopology::PointList) &&
                                                      (primTopology != PrimitiveTopology::LineStrip) &&
                                                      (primTopology != PrimitiveTopology::TriangleStrip);

    bool switchOnEop = ((primTopology == PrimitiveTopology::TriangleStripAdj) ||
                        (primTopology == PrimitiveTopology::TriangleFan) ||
                        (primTopology == PrimitiveTopology::LineLoop) ||
                        (primTopology == PrimitiveTopology::Polygon) ||
                        (primitiveRestartEnabled && restartPrimsCheck) ||
                        drawInfo.useOpaque);

    return switchOnEop;
}

// =====================================================================================================================
// Issues commands to synchronize the VGT's internal stream-out state. This requires writing '1' to CP_STRMOUT_CNTL,
// issuing a VGT streamout-flush event, and waiting for the event to complete using WATIREGMEM.
uint32* UniversalCmdBuffer::FlushStreamOut(
    uint32* pDeCmdSpace)
{
    // Software streamout does not need to perform the flush event as the VS_PARTIAL_FLUSH done implicitly as part of
    // STRMOUT_BUFFER_UPDATE will handle all stalling necessary.
    if (SupportsSwStrmout() == false)
    {
        constexpr uint32 CpStrmoutCntlData = 0;
        WriteDataInfo    writeData         = {};

        writeData.engineType       = m_engineType;
        writeData.dstAddr          = Gfx10::mmCP_STRMOUT_CNTL;
        writeData.engineSel        = engine_sel__me_write_data__micro_engine;
        writeData.dstSel           = dst_sel__me_write_data__mem_mapped_register;
        writeData.dontWriteConfirm = true;

        pDeCmdSpace += CmdUtil::BuildWriteData(writeData, CpStrmoutCntlData, pDeCmdSpace);
        pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(SO_VGTSTREAMOUT_FLUSH, EngineTypeUniversal, pDeCmdSpace);
        pDeCmdSpace += CmdUtil::BuildWaitRegMem(EngineTypeUniversal,
                                                mem_space__pfp_wait_reg_mem__register_space,
                                                function__pfp_wait_reg_mem__equal_to_the_reference_value,
                                                engine_sel__me_wait_reg_mem__micro_engine,
                                                Gfx10::mmCP_STRMOUT_CNTL,
                                                1,
                                                0x00000001,
                                                pDeCmdSpace);
    }
    return pDeCmdSpace;
}

// =====================================================================================================================
// Bind the last state set on the specified command buffer
void UniversalCmdBuffer::InheritStateFromCmdBuf(
    const Pm4CmdBuffer* pCmdBuffer)
{
    SetComputeState(pCmdBuffer->GetComputeState(), ComputeStateAll);

    if (pCmdBuffer->IsGraphicsSupported())
    {
        const auto*const pUniversalCmdBuffer = static_cast<const UniversalCmdBuffer*>(pCmdBuffer);

        SetGraphicsState(pUniversalCmdBuffer->GetGraphicsState());

        // Was "CmdSetVertexBuffers" ever called on the parent command buffer?
        if (pUniversalCmdBuffer->m_vbTable.modified != 0)
        {
            // Yes, so we need to copy all the VB SRDs into this command buffer as well.
            m_vbTable.modified  = 1;
            m_vbTable.watermark = pUniversalCmdBuffer->m_vbTable.watermark;
            memcpy(m_vbTable.pSrds, pUniversalCmdBuffer->m_vbTable.pSrds, (sizeof(BufferSrd) * MaxVertexBuffers));

            // Set the "dirty" flag here to trigger the CPU update path in "ValidateGraphicsUserData".
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
                                              ramOffset,
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
    m_state.pLastDumpCeRam                          = pCeCmdSpace;
    m_state.lastDumpCeRamOrdinal2.bits.gfx10.offset = ramOffset;

    pCeCmdSpace += CmdUtil::BuildDumpConstRam(dstGpuMemory.Desc().gpuVirtAddr + memOffset,
                                              ramOffset,
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
    pCeCmdSpace += CmdUtil::BuildWriteConstRam(pSrcData, ramOffset, dwordSize, pCeCmdSpace);
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
    gpusize     gpuVirtAddr,
    uint32      data,
    uint32      mask,
    CompareFunc compareFunc)
{
    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

    pCmdSpace += CmdUtil::BuildWaitRegMem(EngineTypeUniversal,
                                          mem_space__me_wait_reg_mem__memory_space,
                                          CmdUtil::WaitRegMemFunc(compareFunc),
                                          engine_sel__me_wait_reg_mem__micro_engine,
                                          gpuVirtAddr,
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

// =====================================================================================================================
void UniversalCmdBuffer::CmdUpdateHiSPretests(
    const IImage*      pImage,
    const HiSPretests& pretests,
    uint32             firstMip,
    uint32             numMips)
{
    const Pal::Image* pPalImage  = static_cast<const Pal::Image*>(pImage);
    Image*            pGfx9Image = static_cast<Image*>(pPalImage->GetGfxImage());

    if (pGfx9Image->HasHiSPretestsMetaData())
    {
        SubresRange range = { };
        range.startSubres = { pGfx9Image->GetStencilPlane(), firstMip, 0 };
        range.numPlanes   = 1;
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

    m_gfxCmdBufStateFlags.clientPredicate  = ((pQueryPool != nullptr) || (pGpuMemory != nullptr)) ? 1 : 0;
    m_pm4CmdBufState.flags.packetPredicate = m_gfxCmdBufStateFlags.clientPredicate;

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

    // If the predicate is 32-bits and the engine does not support that width natively, allocate a 64-bit
    // embedded predicate, zero it, emit a ME copy from the original to the lower 32-bits of the embedded
    // predicate, and update `gpuVirtAddr` and `predType`.
    if ((predType == PredicateType::Boolean32) && (m_cachedSettings.has32bPred == 0))
    {
        PAL_ASSERT(gpuVirtAddr != 0);
        constexpr size_t PredicateDwordSize  = sizeof(uint64) / sizeof(uint32);
        constexpr size_t PredicateDwordAlign = 16 / sizeof(uint32);
        gpusize predicateVirtAddr            = 0;
        uint32* pPredicate                   = CmdAllocateEmbeddedData(PredicateDwordSize,
                                                                       PredicateDwordAlign,
                                                                       &predicateVirtAddr);
        pPredicate[0] = 0;
        pPredicate[1] = 0;
        pDeCmdSpace += m_cmdUtil.BuildCopyData(EngineTypeUniversal,
                                               engine_sel__me_copy_data__micro_engine,
                                               dst_sel__me_copy_data__tc_l2_obsolete,
                                               predicateVirtAddr,
                                               src_sel__me_copy_data__tc_l2_obsolete,
                                               gpuVirtAddr,
                                               count_sel__me_copy_data__32_bits_of_data,
                                               wr_confirm__me_copy_data__wait_for_confirmation,
                                               pDeCmdSpace);

        pDeCmdSpace += CmdUtil::BuildPfpSyncMe(pDeCmdSpace);
        gpuVirtAddr = predicateVirtAddr;
        predType    = PredicateType::Boolean64;
    }

    pDeCmdSpace += CmdUtil::BuildSetPredication(gpuVirtAddr,
                                                predPolarity,
                                                waitResults,
                                                predType,
                                                accumulateData,
                                                pDeCmdSpace);

    // For DX12 clients, we need to save the result of the predicate into embedded data to use for predicating
    // indirect command generation.
    // For Vulkan clients, we need to save the result of the predicate into embedded data to use for predicating
    // compute workload discard when doing gang submit
    if ((m_device.GetPlatform()->GetClientApiId() == ClientApi::Dx12) ||
        (m_device.GetPlatform()->GetClientApiId() == ClientApi::Vulkan))
    {
        if (gpuVirtAddr != 0)
        {
            const uint32 predCopyData  = 1;
            uint32       *pPredCpuAddr = CmdAllocateEmbeddedData(1, 1, &m_predGpuAddr);
            (*pPredCpuAddr) = 0;

            WriteDataInfo writeData = {};
            writeData.engineType = EngineTypeUniversal;
            writeData.dstAddr    = m_predGpuAddr;
            writeData.engineSel  = engine_sel__pfp_write_data__prefetch_parser;
            writeData.dstSel     = dst_sel__pfp_write_data__memory;
            writeData.predicate  = PacketPredicate();

            pDeCmdSpace += CmdUtil::BuildWriteData(writeData, predCopyData, pDeCmdSpace);
        }
        else
        {
            m_predGpuAddr = 0;
        }
    }

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
    pCmdSpace += CmdUtil::BuildDmaData<false>(dmaData, pCmdSpace);

    m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
uint32 UniversalCmdBuffer::ComputeSpillTableInstanceCnt(
    uint32 spillTableDwords,
    uint32 vertexBufTableDwords,
    uint32 maxCmdCnt,
    bool*  pUseLargeEmbeddedData
    ) const
{
    // Since the SpillTable/s data needs to be virtually contiguous the way it is referenced later, we do not wish to
    // allocate more memory for it than what can fit in a single chunk of the CmdAllocator::EmbeddedData or
    // CmdAllocator::LargeEmbeddedData. Number of iterations of ExecuteIndirect Ops is determined by
    // Min(MaxCmdCount, ActualCmdCount). We cannot know ActualCmdCount which is stored in a GPU address space buffer at
    // this point. So we choose EmbeddedData when number of SpillTable instances that would fit in an EmbeddedChunk are
    // greater than or equal to the specified maxCmdCount otherwise it's best we choose LargeEmbeddedData which can fit
    // 4x as many instances. The more instances of SpillTable we can maintain at once the better it is for performance
    // as the CP will stall the next DMA_DATA/s and do a sync and flush of K$ when number of instances per iteration is
    // met. SpillTableInstCnt needs to be a Power of 2 per CP requirements.

    const uint32 tableSizeDwords         = spillTableDwords + vertexBufTableDwords;
    const uint32 embeddedDataLimitDwords = GetEmbeddedDataLimit();
    uint32 spillCnt                      = Min((embeddedDataLimitDwords / tableSizeDwords), maxCmdCnt);
    uint32 spillTableInstCnt             = Util::Pow2Pad(spillCnt);

    *pUseLargeEmbeddedData = false;
    if (spillTableInstCnt < maxCmdCnt)
    {
        *pUseLargeEmbeddedData = true;
        const uint32 largeEmbeddedDataLimitDwords = GetLargeEmbeddedDataLimit();
        spillCnt                                  = Min((largeEmbeddedDataLimitDwords / tableSizeDwords), maxCmdCnt);
        spillTableInstCnt                         = Util::Pow2Pad(spillCnt);
    }

    return (spillTableInstCnt > spillCnt) ? spillTableInstCnt >> 1 : spillTableInstCnt;
}

// =====================================================================================================================
// Note: pDeCmdIb2Space can be null! In that case we just do a "dry run" of the packet building logic and return the
// exact command size needed, in DWORDs. If pDeCmdIb2Space is not null, the packets are built into it and the size used
// is returned.
uint32 UniversalCmdBuffer::BuildExecuteIndirectIb2Packets(
    const IndirectCmdGenerator& gfx9Generator,
    ExecuteIndirectPacketInfo*  pPacketInfo,
    const bool                  isGfx,
    const bool                  usesLegacyMsFastLaunch,
    uint32*                     pDeCmdIb2Space)
{
    uint32       sizeDwords     = 0;
    uint32*const pCmdSpaceBegin = pDeCmdIb2Space;

    const uint32                     cmdCount          = gfx9Generator.ParameterCount();
    IndirectParamData*const          pParamData        = gfx9Generator.GetIndirectParamData();

    const uint32 vertexBufTableDwords = isGfx ? gfx9Generator.Properties().vertexBufTableSize : 0;

    const uint32 spillThreshold = isGfx ? m_pSignatureGfx->spillThreshold : m_pSignatureCs->spillThreshold;

    const Pm4ShaderType shaderType = isGfx ? ShaderGraphics : ShaderCompute;

    // We handle all SetUserData ops here. The other kinds of indirect ops will be handled at the end.
    if (WideBitfieldIsAnyBitSet(gfx9Generator.TouchedUserDataEntries()))
    {
        for (uint32 cmdIndex = 0; cmdIndex < cmdCount; )
        {
            // If apps bind multiple user-data elements we expect them to be defined linearly such that their virtual
            // user-data and argument buffer data are contiguous. That means we should be able to scan over the params
            // and build one large SetUserData op which we hope requires fewer packets.
            //
            // Phase 1:
            // The inner loop builds this imaginary combined SetUserData op into these variables. If we're lucky we
            // will loop over all parameters in one pass but if the SetUserData ops are not contiguous the outer loop
            // will simply run this whole process again.
            uint32 argOffset;
            uint32 firstEntry;
            uint32 entryCount = 0;

            for (; cmdIndex < cmdCount; ++cmdIndex)
            {
                if (pParamData[cmdIndex].type == IndirectOpType::SetUserData)
                {
                    const uint32 nextOffset = pParamData[cmdIndex].argBufOffset;
                    const uint32 nextFirst  = pParamData[cmdIndex].data[0];
                    const uint32 nextCount  = pParamData[cmdIndex].data[1];

                    // This op's argument space must exactly fit its user-data values, we assume this below.
                    PAL_ASSERT(pParamData[cmdIndex].argBufSize == nextCount * sizeof(uint32));

                    if (entryCount == 0)
                    {
                        // Begin accumulating virtual user-data into a new contiguous SetUserData range.
                        argOffset  = nextOffset;
                        firstEntry = nextFirst;
                        entryCount = nextCount;
                    }
                    else if ((nextOffset == argOffset + entryCount * sizeof(uint32)) &&
                             (nextFirst == firstEntry + entryCount))
                    {
                        // We can grow the current SetUserData range if the next user-data op picks up exactly where
                        // the current one stopped. Basically, we want both ops to have contiguous virtual user-data
                        // ranges and contiguous argument buffer ranges.
                        entryCount += nextCount;
                    }
                    else
                    {
                        // We've hit a discontinuity in either the virtual user-data range or the argument buffer
                        // range. We need to write all user-data registers for our current combined SetUserData
                        // before trying to process this command parameter.
                        break;
                    }
                }
            }

            if (entryCount > 0)
            {
                // Phase 2:
                // Turn our large combined SetUserData range into the optimal number of LOAD_SH_REG packets.
                // This uses the same sort of nested loop scheme to build an SGPR range, load it, and repeat.
                const uint32 lastEntry = firstEntry + entryCount - 1;

                // Graphics has muliple Shader Stages while Compute has only one.
                uint32 numHwShaderStgs = isGfx ? NumHwShaderStagesGfx : 1;
                for (uint32 stgId = 0; stgId < numHwShaderStgs; stgId++)
                {
                    const UserDataEntryMap& stage = isGfx ? m_pSignatureGfx->stage[stgId] : m_pSignatureCs->stage;

                    for (uint32 sgprIndx = 0; sgprIndx < stage.userSgprCount; )
                    {
                        // Scan over the fast user-data in real USER_DATA order. Each stage has an arbitrary user-data
                        // mapping so we will need to split this up into multiple LOAD_SH_REG packets if the mapping
                        // is reordered or sparse.
                        uint32 loadEntry;
                        uint32 loadSgpr;
                        uint32 loadCount = 0;

                        for (; sgprIndx < stage.userSgprCount; sgprIndx++)
                        {
                            // "entry" can be any virtual user-data index, even one below the spill threshold.
                            // We should only load it if it's within this op's entry range.
                            const uint32 entry = stage.mappedEntry[sgprIndx];

                            if ((entry >= firstEntry) && (entry <= lastEntry))
                            {
                                if (loadCount == 0)
                                {
                                    // Begin accumulating user-data into a new contiguous load range.
                                    loadEntry = entry;
                                    loadSgpr  = stage.firstUserSgprRegAddr + sgprIndx;
                                    loadCount = 1;
                                }
                                else if (entry == loadEntry + loadCount)
                                {
                                    // We can grow the range if this entry is contiguous with the last user-data in the
                                    // current range. Because we're looping over the real SGPR offsets the SGPRs are
                                    // contiguous. We only need to verify that the virtual user-data one step past the
                                    // end of the current range (loadEntry + loadCount) is equal to this entry.
                                    loadCount++;
                                }
                                else
                                {
                                    // We've hit a virtual user-data mapping discontinuity. We need to end the current
                                    // load range, issue its LOAD_SH_REG_INDEX packet, and loop again.
                                    break;
                                }
                            }
                        }

                        if (loadCount > 0)
                        {
                            sizeDwords += CmdUtil::LoadShRegIndexSize;

                            if (pDeCmdIb2Space != nullptr)
                            {
                                // Issue the load packet. If we're lucky this is the only load packet for this stage.
                                pDeCmdIb2Space += m_cmdUtil.BuildLoadShRegsIndex(
                                    index__pfp_load_sh_reg_index__offset,
                                    data_format__pfp_load_sh_reg_index__offset_and_size,
                                    argOffset + (loadEntry - firstEntry) * sizeof(uint32),
                                    loadSgpr,
                                    loadCount,
                                    shaderType,
                                    pDeCmdIb2Space);
                            }
                        }
                    }
                }

                // Phase 3: Issue a DMA_DATA to update the spill table if required.
                if (spillThreshold <= lastEntry)
                {
                    sizeDwords += CmdUtil::DmaDataSizeDwords;

                    if (pDeCmdIb2Space != nullptr)
                    {
                        // spillOffset is the first virtual user-data to spill relative to firstEntry. It will be
                        // non-zero if some of our user-data were loaded by the code above.
                        const uint32 spillOffset = (spillThreshold > firstEntry) ? spillThreshold - firstEntry : 0;
                        const uint32 spillCount  = entryCount - spillOffset;

                        // Every next iteration we are overwriting the buffer at pSpillTableAddress. The CP handles the
                        // work of cache flush and the PFP-ME sync before overwriting this buffer for the next set of
                        // commands.
                        DmaDataInfo copyInfo = {};
                        copyInfo.srcOffset    = argOffset + spillOffset * sizeof(uint32);
                        copyInfo.srcAddrSpace = sas__pfp_dma_data__memory;
                        copyInfo.srcSel       = src_sel__pfp_dma_data__src_addr_using_l2;
                        copyInfo.dstOffset    = (vertexBufTableDwords + firstEntry + spillOffset) * sizeof(uint32);
                        copyInfo.dstAddrSpace = das__pfp_dma_data__memory;
                        copyInfo.dstSel       = dst_sel__pfp_dma_data__dst_addr_using_l2;
                        copyInfo.numBytes     = spillCount * sizeof(uint32);
                        copyInfo.rawWait      = 0;
                        copyInfo.usePfp       = true;
                        copyInfo.sync         = true;
                        copyInfo.predicate    = PredDisable;

                        pDeCmdIb2Space += m_cmdUtil.BuildDmaData<true>(copyInfo, pDeCmdIb2Space);
                    }
                }
            }
        }
    }

    // Now loop over the indirect ops one more time and build the simple packets.
    const uint16 vtxOffsetReg  = GetVertexOffsetRegAddr();
    const uint16 instOffsetReg = GetInstanceOffsetRegAddr();

    for (uint32 cmdIndex = 0; cmdIndex < cmdCount; cmdIndex++)
    {
        switch (pParamData[cmdIndex].type)
        {
        case IndirectOpType::Dispatch:
            sizeDwords += CmdUtil::DispatchIndirectGfxSize;

            if (pDeCmdIb2Space != nullptr)
            {
                pDeCmdIb2Space += m_cmdUtil.BuildDispatchIndirectGfx(
                    pParamData[cmdIndex].argBufOffset,
                    PacketPredicate(),
                    m_pSignatureCs->flags.isWave32,
                    pDeCmdIb2Space);
            }
            break;

        case IndirectOpType::DrawIndexAuto:
            sizeDwords += CmdUtil::DrawIndirectSize;

            if (pDeCmdIb2Space != nullptr)
            {
                m_deCmdStream.NotifyIndirectShRegWrite(vtxOffsetReg);
                m_deCmdStream.NotifyIndirectShRegWrite(instOffsetReg);

                pDeCmdIb2Space += CmdUtil::BuildDrawIndirect(
                    pParamData[cmdIndex].argBufOffset,
                    vtxOffsetReg,
                    instOffsetReg,
                    PacketPredicate(),
                    pDeCmdIb2Space);
            }
            break;

        case IndirectOpType::DrawIndexOffset2:
            sizeDwords += m_cmdUtil.DrawIndexIndirectSize();

            if (pDeCmdIb2Space != nullptr)
            {
                m_deCmdStream.NotifyIndirectShRegWrite(vtxOffsetReg);
                m_deCmdStream.NotifyIndirectShRegWrite(instOffsetReg);

                pDeCmdIb2Space += m_cmdUtil.BuildDrawIndexIndirect(
                    pParamData[cmdIndex].argBufOffset,
                    vtxOffsetReg,
                    instOffsetReg,
                    PacketPredicate(),
                    pDeCmdIb2Space);
            }
            break;

        case IndirectOpType::DrawIndex2:
            sizeDwords += CmdUtil::SetIndexAttributesSize + m_cmdUtil.DrawIndexIndirectSize();
            if (pDeCmdIb2Space != nullptr)
            {
                m_deCmdStream.NotifyIndirectShRegWrite(vtxOffsetReg);
                m_deCmdStream.NotifyIndirectShRegWrite(instOffsetReg);

                // 1. INDEX_ATTRIBUTES_INDIRECT  set the index buffer base, size, Type
                pDeCmdIb2Space += m_cmdUtil.BuildIndexAttributesIndirect(
                    pParamData[cmdIndex].data[0],
                    0,
                    true,
                    pDeCmdIb2Space);

                // 2. Draw Indirect
                pDeCmdIb2Space += m_cmdUtil.BuildDrawIndexIndirect(
                    pParamData[cmdIndex].argBufOffset,
                    vtxOffsetReg,
                    instOffsetReg,
                    PacketPredicate(),
                    pDeCmdIb2Space);
            }
            break;

        case IndirectOpType::VertexBufTableSrd:
            sizeDwords += CmdUtil::BuildUntypedSrdSize;

            if (pDeCmdIb2Space != nullptr)
            {
                BuildUntypedSrdInfo srdInfo = {};
                srdInfo.srcGpuVirtAddressOffset = pParamData[cmdIndex].argBufOffset;
                srdInfo.dstGpuVirtAddressOffset = pParamData[cmdIndex].data[0] * sizeof(uint32);

                // Always set resource_level = 1 because we're in GEN_TWO  mode
                const uint32 resourceLevel = m_device.BufferSrdResourceLevel();
                // Always set oob_select = 2 (allow transaction unless numRecords == 0)
                constexpr uint32 OobSelect = SQ_OOB_NUM_RECORDS_0;
                // Use the LLC for read/write if enabled in Mtype
                constexpr uint32 LlcNoalloc = 0x0;

                srdInfo.srdDword3 = ((SQ_SEL_X        << SqBufRsrcTWord3DstSelXShift)                       |
                                     (SQ_SEL_Y        << SqBufRsrcTWord3DstSelYShift)                       |
                                     (SQ_SEL_Z        << SqBufRsrcTWord3DstSelZShift)                       |
                                     (SQ_SEL_W        << SqBufRsrcTWord3DstSelWShift)                       |
                                     (BUF_FMT_32_UINT << Gfx10SqBufRsrcTWord3FormatShift)                   |
                                     (resourceLevel   << Gfx10SqBufRsrcTWord3ResourceLevelShift)            |
                                     (OobSelect       << SqBufRsrcTWord3OobSelectShift)                     |
                                     (LlcNoalloc      << Gfx103PlusExclusiveSqBufRsrcTWord3LlcNoallocShift) |
                                     (SQ_RSRC_BUF     << SqBufRsrcTWord3TypeShift));

                pDeCmdIb2Space += m_cmdUtil.BuildUntypedSrd(
                                                PacketPredicate(), &srdInfo, ShaderGraphics, pDeCmdIb2Space);
            }
            break;

        case IndirectOpType::DispatchMesh:
            // Only supported for Gfx11Plus and when Task Shader is disabled until we add support for IB2 on Gfx to sync
            // with an ACE queue.
            // We use the PM4_ME_DISPATCH_MESH_INDIRECT_MULTI packet with a "Multi of 1".
            sizeDwords += CmdUtil::DispatchMeshIndirectMulti;

            if (pDeCmdIb2Space != nullptr)
            {
                const uint16 drawIndexReg        = GetDrawIndexRegAddr();
                const uint16 meshDispatchDimsReg = m_pSignatureGfx->meshDispatchDimsRegAddr;
                m_deCmdStream.NotifyIndirectShRegWrite(meshDispatchDimsReg);
                m_deCmdStream.NotifyIndirectShRegWrite(drawIndexReg);

                if (m_cachedSettings.issueSqttMarkerEvent)
                {
                    pDeCmdIb2Space += m_cmdUtil.BuildDispatchMeshIndirectMulti<true>(pParamData[cmdIndex].argBufOffset,
                                                                                     meshDispatchDimsReg,
                                                                                     drawIndexReg,
                                                                                     1,
                                                                                     pParamData[cmdIndex].argBufSize,
                                                                                     0uLL,
                                                                                     PacketPredicate(),
                                                                                     usesLegacyMsFastLaunch,
                                                                                     pDeCmdIb2Space);
                }
                else
                {
                    pDeCmdIb2Space += m_cmdUtil.BuildDispatchMeshIndirectMulti<false>(pParamData[cmdIndex].argBufOffset,
                                                                                      meshDispatchDimsReg,
                                                                                      drawIndexReg,
                                                                                      1,
                                                                                      pParamData[cmdIndex].argBufSize,
                                                                                      0uLL,
                                                                                      PacketPredicate(),
                                                                                      usesLegacyMsFastLaunch,
                                                                                      pDeCmdIb2Space);
                }
            }
            break;

        case IndirectOpType::Skip:
        case IndirectOpType::SetUserData:
            // Nothing to do here.
            break;
        default:
            // What's this?
            PAL_ASSERT_ALWAYS();
            break;
        }
    }

    if (m_cachedSettings.issueSqttMarkerEvent)
    {
        sizeDwords += CmdUtil::WriteNonSampleEventDwords;

        if (pDeCmdIb2Space != nullptr)
        {
            pDeCmdIb2Space += m_cmdUtil.BuildNonSampleEventWrite(
                                            THREAD_TRACE_MARKER, EngineTypeUniversal, pDeCmdIb2Space);
        }
    }

    // Note that the CP has only required DWORD aligned indirect buffers for many years, since at least gfx8.
    // That means we don't need to add a padding NOP at the end here.
    if (pDeCmdIb2Space != nullptr)
    {
        // We better use exactly the amount of memory we ask for!
        const size_t sizeUsed = VoidPtrDiff(pDeCmdIb2Space, pCmdSpaceBegin);

        PAL_ASSERT(sizeUsed == sizeDwords * sizeof(uint32));
    }

    return sizeDwords;
}

// =====================================================================================================================
// The PacketOp stores what operation this ExecuteIndirectV2 PM4 will be programmed to perform and the Meta object
// stores some data to program the PM4 ordinals and other data like the Look-Up Table implemented to help add Spilled
// UserData entries to a buffer in memory.
uint32 UniversalCmdBuffer::PopulateExecuteIndirectV2Params(
    const IndirectCmdGenerator& gfx9Generator,
    const bool                  isGfx,
    ExecuteIndirectPacketInfo*  pPacketInfo,
    ExecuteIndirectV2Op*        pPacketOp,
    ExecuteIndirectV2Meta*      pMeta)
{
    const uint32                     cmdCount   = gfx9Generator.ParameterCount();
    IndirectParamData*const          pParamData = gfx9Generator.GetIndirectParamData();
    const Pm4::GeneratorProperties&  properties = gfx9Generator.Properties();
    ExecuteIndirectV2MetaData*       pMetaData  = pMeta->GetMetaData();

    const uint32 spillThreshold = isGfx ? m_pSignatureGfx->spillThreshold : m_pSignatureCs->spillThreshold;

    // Get the required VBTable AllocSize by early looping through all OpType VBTableSRDs.
    uint32 vertexBufTableDwords = isGfx ? m_vbTable.watermark : 0;

    for (uint32 cmdIndex = 0; cmdIndex < cmdCount; cmdIndex++)
    {
        if (pParamData[cmdIndex].type == IndirectOpType::VertexBufTableSrd)
        {
            // data[0] here indicates offset into table where SRD is written. + D3D12 VertexBufferView SRD is 4 DWORDs.
            vertexBufTableDwords = Max(pParamData[cmdIndex].data[0] + 4, vertexBufTableDwords);
        }
    }
    pPacketInfo->vbTableSizeDwords = vertexBufTableDwords;

    uint32 stageUsageMask = 0;
    uint32 sizeInDwords   = PM4_PFP_EXECUTE_INDIRECT_V2_SIZEDW__GFX103PLUSEXCLUSIVE;

    constexpr uint32 EightBitMask = 0xff;
    constexpr uint32 TenBitMask   = 0x3ff;

    // We handle all SetUserData ops here. The other kinds of indirect ops will be handled after.
    if (WideBitfieldIsAnyBitSet(gfx9Generator.TouchedUserDataEntries()))
    {
        // We will actually go in and check what HwShaderStgs are active for Gfx based on corresponding userSgprCount.
        // If Cs we can assume it's 1.
        uint32 numActiveHwShaderStgs = isGfx ? 0 : 1;
        uint32 stageIndices[NumHwShaderStagesGfx] {};

        if (isGfx)
        {
            for (uint32 stgId = 0; stgId < NumHwShaderStagesGfx; stgId++)
            {
                if (m_pSignatureGfx->stage[stgId].userSgprCount != 0)
                {
                    stageIndices[numActiveHwShaderStgs++] = stgId;
                }
            }
        }
        PAL_ASSERT(numActiveHwShaderStgs <= EIV2MaxStages);

        uint32 argBufOffsetBaseDw = 0;
        uint32 argSizeDw          = 0;
        bool foundUserData        = false;

        // The UserData entries to be modified are laid out linearly in the ArgumentBuffer at an Offset. Typically,
        // this is 2 to 4 contiguous Reg values. We find the smallest Offset here and set it as the "Base" so we can
        // combine all the SetUserData Ops into 1 Op.
        for (uint32 cmdIndex = 0; cmdIndex < cmdCount; cmdIndex++)
        {
            if (pParamData[cmdIndex].type == IndirectOpType::SetUserData)
            {
                const uint32 baseInDw = pParamData[cmdIndex].argBufOffset >> 2;
                if (foundUserData == false)
                {
                    argBufOffsetBaseDw = baseInDw;
                    foundUserData = true;
                }
                else
                {
                    argBufOffsetBaseDw = Min(argBufOffsetBaseDw, baseInDw);
                }
            }
        }

        // Initialize the Look-Up Table for the (VBTable + UserDataSpill) Buffer we create for ExecuteIndirect Op.
        // Since Look-up for modification is only required for Spilled UserData Entries and not the VertexBuffer Table
        // we will exclude the part of the Buffer which contains the VBtable and UserDataEntries not spilled i.e. up to
        // the SpillThreshold.
        pMeta->InitLut(pPacketInfo->vbTableSizeDwords, pPacketInfo->vbTableSizeDwords + spillThreshold);

        for (uint32 cmdIndex = 0; cmdIndex < cmdCount; cmdIndex++)
        {
            if (pParamData[cmdIndex].type != IndirectOpType::SetUserData)
            {
                continue;
            }

            // Offset for the first UserData entry/entries to Set. If the first SetUserData is lowest then offset is 0.
            const uint32 argBufOffsetDw = (pParamData[cmdIndex].argBufOffset >> 2) - argBufOffsetBaseDw;
            const uint32 firstEntry     = pParamData[cmdIndex].data[0];
            const uint32 entryCount     = pParamData[cmdIndex].data[1];

            // This op's argument space must exactly fit its user-data values, we assume this below.
            PAL_ASSERT(pParamData[cmdIndex].argBufSize == entryCount * sizeof(uint32));

            const uint32 lastEntry = firstEntry + entryCount - 1;

            // Step 1: Update UserData Entries that lie in the UserDataRegister range.
            for (uint32 stgId = 0; stgId < numActiveHwShaderStgs; stgId++)
            {
                const uint32 stgIdx = stageIndices[stgId];

                const UserDataEntryMap& stage = isGfx ? m_pSignatureGfx->stage[stgIdx] : m_pSignatureCs->stage;
                const uint32 addrMask         = isGfx ? EightBitMask : TenBitMask;

                for (uint32 sgprIndx = 0; sgprIndx < stage.userSgprCount; sgprIndx++)
                {
                    // "entry" can be any virtual user-data index, even one below the spill threshold.
                    // We should only load it if it's within this op's entry range.
                    const uint32 entry = stage.mappedEntry[sgprIndx];

                    // Is a valid entry.
                    if ((entry >= firstEntry) && (entry <= lastEntry))
                    {
                        // Used to count the active stages later.
                        stageUsageMask |= (1 << stgId);

                        // argBufIdx is the dword at which we can find this UserData Entry to update in the ArgBuffer.
                        const uint32 argBufIdx = argBufOffsetDw + (entry - firstEntry);
                        argSizeDw = Max(argSizeDw, argBufIdx + 1);

                        // Since the argBufOffset is relative to the owning Cmd's (this case SetUserData's) offset. The
                        // argBufIdx and argSizeDw cannot be greater than the API NumUserDataRegisters.
                        PAL_ASSERT(argBufIdx < NumUserDataRegisters);
                        PAL_ASSERT(argSizeDw <= NumUserDataRegisters);

                        // Calculate which UserData Register for this stage needs to be modified with the new value.
                        const uint32 userDataReg = (stage.firstUserSgprRegAddr + sgprIndx) & addrMask;
                        pMetaData->userData[(NumUserDataRegisters * stgId) + argBufIdx] = userDataReg;
                    }
                }
            }

            // Step 2: Issue a MemCopy command to the CP to update the UserDataSpill table. This MemCopy will be done
            // by the CP during execution of the ExecuteIndirectV2 PM4 based on the MemCopy structures.
            if (spillThreshold <= lastEntry)
            {
                // In cases like the DispatchRays Cmd call DXC forces spilling and the spillThreshold can be 0.
                // spillOffset is the offset into the ArgBuffer from which point forward UserData entries would need to
                // be copied into the SpillTable.
                const uint32 spillOffset = (spillThreshold > firstEntry) ? spillThreshold - firstEntry : 0;
                const uint32 spillCount  = entryCount - spillOffset;
                // argBufIdx is the dword at which we can find the first spilling UserData Entry in the ArgBuffer.
                const uint32 argBufIdx   = argBufOffsetDw + spillOffset;
                // (VBTable + UserDataSpill) Buffer saves space for VBTable and also the UserData entries that are
                // copied onto registers before starting with the spilled entries.
                const uint32 spillBufIdx = vertexBufTableDwords + firstEntry + spillOffset;

                pMeta->SetLut(spillBufIdx, argBufIdx, spillCount);
            }
        }

        uint32 initCount   = 0;
        uint32 updateCount = 0;
        pMeta->ComputeMemCopyStructures(pPacketInfo->vbTableSizeDwords + properties.userDataWatermark,
                                        &initCount,
                                        &updateCount);

        pMetaData->initMemCopyCount   = initCount;
        pMetaData->updateMemCopyCount = updateCount;
        pMetaData->userDataOffset     = argBufOffsetBaseDw * sizeof(uint32);
        pMetaData->userDataDwCount    = argSizeDw;
    }

    if (stageUsageMask != 0)
    {
        pMetaData->stageUsageCount = CountSetBits(stageUsageMask);
        pMetaData->userDataScatterMode = static_cast<PFP_EXECUTE_INDIRECT_V2_REG_SCATTER_MODE_function_enum>
                                        (pMetaData->stageUsageCount - 1);
    }
    const uint16 vtxOffsetReg  = GetVertexOffsetRegAddr();
    const uint16 instOffsetReg = GetInstanceOffsetRegAddr();

    // Now loop over the indirect ops. Only one OpType between these Dispatches/Draws is valid over one loop.
    for (uint32 cmdIndex = 0; cmdIndex < cmdCount; cmdIndex++)
    {
        switch (pParamData[cmdIndex].type)
        {
        case IndirectOpType::Dispatch:
        {
            pMetaData->opType = operation__pfp_execute_indirect_v2__dispatch__GFX103PLUSEXCLUSIVE;

            pPacketOp->dispatch = {};

            pPacketOp->dispatch.dataOffset                                = pParamData[cmdIndex].argBufOffset;
            if (m_pSignatureCs->dispatchIndexRegAddr != UserDataNotMapped)
            {
                pMetaData->commandIndexEnable               = true;
                pPacketOp->dispatch.locData.commandIndexLoc = m_pSignatureCs->dispatchIndexRegAddr;
            }
            pPacketOp->dispatch.dispatchInitiator.bits.COMPUTE_SHADER_EN  = 1;
            pPacketOp->dispatch.dispatchInitiator.bits.FORCE_START_AT_000 = 1;
            pPacketOp->dispatch.dispatchInitiator.bits.CS_W32_EN          = m_pSignatureCs->flags.isWave32;
            break;
        }

        case IndirectOpType::DrawIndexAuto:
        {
            pMetaData->opType = operation__pfp_execute_indirect_v2__draw__GFX103PLUSEXCLUSIVE;
            m_deCmdStream.NotifyIndirectShRegWrite(vtxOffsetReg);
            m_deCmdStream.NotifyIndirectShRegWrite(instOffsetReg);

            pPacketOp->draw = {};

            pPacketOp->draw.dataOffset             = pParamData[cmdIndex].argBufOffset;
            pPacketOp->draw.locData.startVertexLoc = vtxOffsetReg  & EightBitMask;
            pPacketOp->draw.locData.startInstLoc   = instOffsetReg & EightBitMask;
            if (m_pSignatureGfx->drawIndexRegAddr != UserDataNotMapped)
            {
                pMetaData->commandIndexEnable           = true;
                pPacketOp->draw.locData.commandIndexLoc = m_pSignatureGfx->drawIndexRegAddr & EightBitMask;
            }
            pPacketOp->draw.drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_AUTO_INDEX;
            pPacketOp->draw.drawInitiator.bits.MAJOR_MODE    = DI_MAJOR_MODE_0;
            break;
        }

        case IndirectOpType::DrawIndex2:
        {
            // location of INDEX_ATTRIBUTES
            pMetaData->fetchIndexAttributes  = true;
            pMetaData->indexAttributesOffset = pParamData[cmdIndex].data[0];
            [[fallthrough]];
        }

        case IndirectOpType::DrawIndexOffset2:
        {
            pMetaData->opType = operation__pfp_execute_indirect_v2__drawindex__GFX103PLUSEXCLUSIVE;
            m_deCmdStream.NotifyIndirectShRegWrite(vtxOffsetReg);
            m_deCmdStream.NotifyIndirectShRegWrite(instOffsetReg);

            pPacketOp->drawIndexed = {};

            pPacketOp->drawIndexed.dataOffset            = pParamData[cmdIndex].argBufOffset;
            pPacketOp->drawIndexed.locData.baseVertexLoc = vtxOffsetReg  & EightBitMask;
            pPacketOp->drawIndexed.locData.startInstLoc  = instOffsetReg & EightBitMask;
            if (m_pSignatureGfx->drawIndexRegAddr != UserDataNotMapped)
            {
                pMetaData->commandIndexEnable                  = true;
                pPacketOp->drawIndexed.locData.commandIndexLoc = m_pSignatureGfx->drawIndexRegAddr & EightBitMask;
            }
            pPacketOp->drawIndexed.drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_DMA;
            pPacketOp->drawIndexed.drawInitiator.bits.MAJOR_MODE    = DI_MAJOR_MODE_0;
            break;
        }

        case IndirectOpType::VertexBufTableSrd:
        {
            const uint32 idx = pMetaData->buildSrdCount++;
            pMetaData->buildSrdSrcOffsets[idx] = pParamData[cmdIndex].argBufOffset;
            pMetaData->buildSrdDstOffsets[idx] = static_cast<uint16>(pParamData[cmdIndex].data[0] * sizeof(uint32));
            break;
        }
        case IndirectOpType::DispatchMesh:
        {
            pMetaData->opType = operation__pfp_execute_indirect_v2__dispatch_mesh__GFX103PLUSEXCLUSIVE;
            const uint16 drawIndexReg        = GetDrawIndexRegAddr();
            const uint16 meshDispatchDimsReg = m_pSignatureGfx->meshDispatchDimsRegAddr;
            m_deCmdStream.NotifyIndirectShRegWrite(meshDispatchDimsReg);

            pPacketOp->dispatchMesh = {};

            pPacketOp->dispatchMesh.dataOffset        = pParamData[cmdIndex].argBufOffset;
            pPacketOp->dispatchMesh.locData.xyzDimLoc = meshDispatchDimsReg & EightBitMask;
            if (drawIndexReg != UserDataNotMapped)
            {
                m_deCmdStream.NotifyIndirectShRegWrite(drawIndexReg);
                pMetaData->commandIndexEnable                   = true;
                pPacketOp->dispatchMesh.locData.commandIndexLoc = drawIndexReg & EightBitMask;
            }
            pPacketOp->dispatchMesh.drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_AUTO_INDEX;
            pPacketOp->dispatchMesh.drawInitiator.bits.MAJOR_MODE    = DI_MAJOR_MODE_0;
            break;
        }
        case IndirectOpType::Skip:
        case IndirectOpType::SetUserData:
            // Nothing to do here.
            break;

        default:
            // What's this?
            PAL_ASSERT_ALWAYS();
            break;
        }
    }

    // GFX supports only PS_USER_DATA and GS_USER_DATA and register adresses are stored using 8 bits.
    // Compute needs to set only CS_USER_DATA and register adresses are stored using 16 bits.
    const uint32 padLimit  = isGfx ? 3 : 1;
    const uint32 shiftBits = isGfx ? 2 : 1;

    // This is just the fixed precalculated DwordSize that is added by these operations as an offset to the base
    // ExecuteIndirectV2 PM4. For reference look at the corresponding ExecuteIndirectV2WritePacked().
    sizeInDwords += ((pMetaData->initMemCopyCount   + 1) >> 1) * 3;
    sizeInDwords += ((pMetaData->updateMemCopyCount + 1) >> 1) * 3;
    sizeInDwords += ((pMetaData->buildSrdCount      + 1) >> 1) * 2;
    sizeInDwords += (pMetaData->userDataDwCount     + padLimit) >> shiftBits;

    // The GlobalSpillTable for EI V2 is only used when there will be updateMemCopy Ops (UserData SpillTable changes
    // between consecutive Draw/Dispatch Ops) or there is a buildSrd Op (VBTable).
    if ((pMetaData->updateMemCopyCount > 0) || (pMetaData->buildSrdCount > 0))
    {
        m_state.flags.needsEiV2GlobalSpill = true;
    }

    if (m_cachedSettings.issueSqttMarkerEvent)
    {
        pMetaData->threadTraceEnable = 1;
    }

    return sizeInDwords;
}

// =====================================================================================================================
gpusize UniversalCmdBuffer::ConstructExecuteIndirectPacket(
    const IndirectCmdGenerator& gfx9Generator,
    PipelineBindPoint           bindPoint,
    const GraphicsPipeline*     pGfxPipeline,
    const ComputePipeline*      pCsPipeline,
    ExecuteIndirectPacketInfo*  pPacketInfo,
    ExecuteIndirectV2Op*        pPacketOp,
    ExecuteIndirectV2Meta*      pMeta)
{
    const Pm4::GeneratorProperties&  properties = gfx9Generator.Properties();
    const bool isGfx = (bindPoint == PipelineBindPoint::Graphics);
    bool useLargeEmbeddedData = false;

    // DataSpace for the Spill+Vertex Table buffer.
    uint32* pUserDataSpace = nullptr;
    const bool useExecuteIndirectV2 = (m_cachedSettings.useExecuteIndirectPacket >= UseExecuteIndirectV2Packet);
    ExecuteIndirectV2MetaData* pMetaData = pMeta->GetMetaData();
    const Pm4ShaderType shaderType = isGfx ? ShaderGraphics : ShaderCompute;
    const bool usesLegacyMsFastLaunch = isGfx ?
                                            (pGfxPipeline->FastLaunchMode() == GsFastLaunchMode::VertInLane) : false;

    // Graphics Pipeline (Indirect Draw)
    if (isGfx)
    {
        const uint32 spillDwords = (m_pSignatureGfx->spillThreshold <= properties.userDataWatermark) ?
                                    properties.maxUserDataEntries : 0;

        // Required VBTable AllocSize in DWORDs
        const uint32 vertexBufTableDwords = properties.vertexBufTableSize;

        pPacketInfo->spillTableStrideBytes = (spillDwords + vertexBufTableDwords) * sizeof(uint32);

        // Set VertexBuffer parameters.
        if (vertexBufTableDwords > 0)
        {
            pPacketInfo->vbTableSizeDwords = vertexBufTableDwords;
            pPacketInfo->vbTableRegOffset = m_pSignatureGfx->vertexBufTableRegAddr;
        }

        // UserData that spills over the assigned SGPRs is also modified by this generator and we will need to create
        // and handle SpillTable/s + VertexBuffer/s. We manage the VertexBuffer/SRD as part of the SpillTable Buffer.
        // Memory layout is [VertexBuffer + SpillTable].
        if (pPacketInfo->spillTableStrideBytes > 0)
        {
            // Number of instances means max number of (1 UserDataSpillTable + VertexBuffer per Command) Spill+VBTables
            // we can fit. If the number of Tables required exceeds the number we can fit in this buffer the CP will
            // replace the UserData entries stored in the current SpillTable buffer with the next set of entries from
            // the Argument Buffer. spillTableInstCnt should always be a power of 2.
            // ExecuteIndirectV2 needs to maintain a single instance of UserData for the copy over to the queue
            // specific reserved memory buffer with the CP InitMemCpy operation. CP UpdateMemCpy operation will then
            // update UserData slots based on data from the Argument Buffer.
            pPacketInfo->spillTableInstanceCnt = useExecuteIndirectV2 ? 1 : ComputeSpillTableInstanceCnt(spillDwords,
                                                                                          vertexBufTableDwords,
                                                                                          pPacketInfo->maxCount,
                                                                                          &useLargeEmbeddedData);

            // Allocate and populate Spill+VBTable Buffer with UserData. Each instance of the SpillTable and
            // VertexBuffer needs to be initialized with UserDataEntries of current context.
            if (useLargeEmbeddedData)
            {
                pUserDataSpace = CmdAllocateLargeEmbeddedData(
                    ((vertexBufTableDwords + spillDwords) * (pPacketInfo->spillTableInstanceCnt)),
                    CacheLineDwords,
                    &(pPacketInfo->spillTableAddr));
            }
            else
            {
                pUserDataSpace = CmdAllocateEmbeddedData(
                    ((vertexBufTableDwords + spillDwords) * (pPacketInfo->spillTableInstanceCnt)),
                    CacheLineDwords,
                    &(pPacketInfo->spillTableAddr));
            }

            PAL_ASSERT(pUserDataSpace != nullptr);
            for (uint32 i = 0; i < pPacketInfo->spillTableInstanceCnt; i++)
            {
                if (vertexBufTableDwords != 0)
                {
                    memcpy(pUserDataSpace, m_vbTable.pSrds, (sizeof(uint32) * vertexBufTableDwords));
                    pUserDataSpace += vertexBufTableDwords;
                }
                if (spillDwords != 0)
                {
                    memcpy(pUserDataSpace, m_graphicsState.gfxUserDataEntries.entries,
                           (sizeof(uint32) * properties.userDataWatermark));
                    pUserDataSpace += spillDwords;
                }
            }
        }
    }

    // Compute Pipeline (Indirect Dispatch)
    else
    {
        const uint32 spillDwords = (m_pSignatureCs->spillThreshold <= properties.userDataWatermark) ?
                                    properties.maxUserDataEntries : 0;

        pPacketInfo->spillTableStrideBytes = (spillDwords) * sizeof(uint32);

        // UserData that spills over the assigned SGPRs.
        if (pPacketInfo->spillTableStrideBytes > 0)
        {
            pPacketInfo->spillTableInstanceCnt = useExecuteIndirectV2 ? 1 : ComputeSpillTableInstanceCnt(spillDwords,
                                                                                          0,
                                                                                          pPacketInfo->maxCount,
                                                                                          &useLargeEmbeddedData);

            // Allocate and populate SpillTable Buffer with UserData. Each instance of the SpillTable needs to be
            // initialized with UserDataEntries of current context.
            if (useLargeEmbeddedData)
            {
                pUserDataSpace = CmdAllocateLargeEmbeddedData(
                    (spillDwords * (pPacketInfo->spillTableInstanceCnt)),
                    CacheLineDwords,
                    &(pPacketInfo->spillTableAddr));
            }
            else
            {
                pUserDataSpace = CmdAllocateEmbeddedData(
                    (spillDwords * (pPacketInfo->spillTableInstanceCnt)),
                    CacheLineDwords,
                    &(pPacketInfo->spillTableAddr));
            }

            PAL_ASSERT(pUserDataSpace != nullptr);
            for (uint32 i = 0; i < pPacketInfo->spillTableInstanceCnt; i++)
            {
                memcpy(pUserDataSpace, m_computeState.csUserDataEntries.entries, (sizeof(uint32) * spillDwords));
                pUserDataSpace += spillDwords;
            }
        }
    }

    uint32* pIb2Space = nullptr;
    gpusize ib2GpuVa = 0;

    if (useExecuteIndirectV2)
    {
        PopulateExecuteIndirectV2Params(gfx9Generator, isGfx, pPacketInfo, pPacketOp, pMeta);
    }
    else
    {
        // Note that we do a "practice run" of our PM4 building routine to compute the exact IB2 size we need.
        // SetUserData is the entire reason we do this. Its worst-case size estimates are just way too large, like 100x
        // larger than reality. If we don't compute the exact size we risk failing to allocate embedded data.
        const uint32  sizeDwords = BuildExecuteIndirectIb2Packets(gfx9Generator,
                                                                  pPacketInfo,
                                                                  isGfx,
                                                                  usesLegacyMsFastLaunch,
                                                                  nullptr);
        pIb2Space  = CmdAllocateEmbeddedData(sizeDwords, 1, &ib2GpuVa);

        pPacketInfo->commandBufferSizeBytes = sizeDwords * sizeof(uint32);

        BuildExecuteIndirectIb2Packets(gfx9Generator, pPacketInfo, isGfx, usesLegacyMsFastLaunch, pIb2Space);

        const Ib2DumpInfo dumpInfo =
        {
            pIb2Space,                                   // CPU address of the commands
            uint32(pPacketInfo->commandBufferSizeBytes), // Length of the dump in bytes
            uint64(ib2GpuVa),                            // GPU virtual address of the commands
            m_deCmdStream.GetEngineType(),               // Engine Type
            m_deCmdStream.GetSubEngineType(),            // Sub Engine Type
        };

        InsertIb2DumpInfo(dumpInfo);
    }

    return ib2GpuVa;
}

// =====================================================================================================================
// This method creates and uses a CP packet to perform the ExecuteIndirect operation.
void UniversalCmdBuffer::ExecuteIndirectPacket(
    const IIndirectCmdGenerator& generator,
    gpusize                      gpuVirtAddr,
    uint32                       maximumCount,
    gpusize                      countGpuAddr)
{
    const auto& gfx9Generator = static_cast<const IndirectCmdGenerator&>(generator);

    // The generation of indirect commands is determined by the currently-bound pipeline.
    const PipelineBindPoint bindPoint    = ((gfx9Generator.Type() == Pm4::GeneratorType::Dispatch) ?
                                           PipelineBindPoint::Compute : PipelineBindPoint::Graphics);

    const auto* pGfxPipeline  = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const auto* pCsPipeline   = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);
    const bool  isGfx         = (bindPoint == PipelineBindPoint::Graphics);
    uint32      mask          = 1;
    const bool  isTaskEnabled = ((gfx9Generator.Type() == Pm4::GeneratorType::DispatchMesh) &&
                                pGfxPipeline->HasTaskShader());
    const Pm4ShaderType shaderType = isGfx ? ShaderGraphics : ShaderCompute;
    const bool useExecuteIndirectV2 = (m_cachedSettings.useExecuteIndirectPacket >= UseExecuteIndirectV2Packet);

    if (isGfx && (pGfxPipeline->HwStereoRenderingEnabled() == false))
    {
        const auto& viewInstancingDesc = pGfxPipeline->GetViewInstancingDesc();
        mask = (1 << viewInstancingDesc.viewInstanceCount) - 1;
        if (viewInstancingDesc.enableMasking)
        {
            mask &= m_graphicsState.viewInstanceMask;
        }
    }

    for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
    {
        if (TestAnyFlagSet(mask, 1) == false)
        {
            continue;
        }

        ExecuteIndirectPacketInfo packetInfo {};
        packetInfo.maxCount                  = maximumCount;
        packetInfo.countBufferAddr           = countGpuAddr;
        packetInfo.argumentBufferAddr        = gpuVirtAddr;
        packetInfo.argumentBufferStrideBytes = gfx9Generator.Properties().argBufStride;

        ExecuteIndirectV2Op packetOp {};
        ExecuteIndirectV2Meta meta {};

        if (isGfx)
        {
            Pm4::ValidateDrawInfo drawInfo;
            drawInfo.vtxIdxCount   = 0;
            drawInfo.instanceCount = 0;
            drawInfo.firstVertex   = 0;
            drawInfo.firstInstance = 0;
            drawInfo.firstIndex    = 0;
            drawInfo.useOpaque     = false;
            if (gfx9Generator.ContainsIndexBufferBind() || (gfx9Generator.Type() == Pm4::GeneratorType::Draw))
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
            // m_pSignatureGfx is this pipeline's signature post validation.
            packetInfo.pipelineSignature.pSignatureGfx = m_pSignatureGfx;
        }
        else
        {
            ValidateDispatchPalAbi(&m_computeState, &m_deCmdStream, 0uLL, {});
            CommandGeneratorTouchedUserData(m_computeState.csUserDataEntries.touched, gfx9Generator, *m_pSignatureCs);
            // m_pSignatureCs is this pipeline's signature post validation.
            packetInfo.pipelineSignature.pSignatureCs = m_pSignatureCs;
        }

        // In the interest of avoiding code duplication kept this function. ib2GpuVa returned here isn't relevant if
        // ExecuteIndirect V2 PM4 is used.
        packetInfo.commandBufferAddr = ConstructExecuteIndirectPacket(gfx9Generator,
                                                                      bindPoint,
                                                                      pGfxPipeline,
                                                                      pCsPipeline,
                                                                      &packetInfo,
                                                                      &packetOp,
                                                                      &meta);

        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

        pDeCmdSpace = WaitOnCeCounter(pDeCmdSpace);

        if (isGfx)
        {
            const auto& viewInstancingDesc = pGfxPipeline->GetViewInstancingDesc();
            pDeCmdSpace = BuildWriteViewId(viewInstancingDesc.viewId[i], pDeCmdSpace);
        }

        // Since the execute indirect packet changes the packet stream from IB1 to IB2 and there are separate CP packet
        // filters for both IB1 and IB2, reset the packet filter here for IB1. For ExecuteIndirect V2 PM4 is processed
        // on IB1 but it still requires the resetPktFilter bit set.
        constexpr bool ResetPktFilter = true;

        if (useExecuteIndirectV2)
        {
            pDeCmdSpace += CmdUtil::BuildExecuteIndirectV2(PacketPredicate(),
                                                           isGfx,
                                                           packetInfo,
                                                           ResetPktFilter,
                                                           &packetOp,
                                                           &meta,
                                                           pDeCmdSpace);
        }
        else
        {
            pDeCmdSpace += CmdUtil::BuildExecuteIndirect(PacketPredicate(),
                                                         isGfx,
                                                         packetInfo,
                                                         ResetPktFilter,
                                                         pDeCmdSpace);
        }

        // We need to issue any post-draw or post-dispatch workarounds after the ExecuteIndirect packet has finished
        // executing.
        if (isGfx)
        {
            if ((gfx9Generator.Type() == Pm4::GeneratorType::Draw) ||
                (gfx9Generator.Type() == Pm4::GeneratorType::DrawIndexed) ||
                ((gfx9Generator.Type() == Pm4::GeneratorType::DispatchMesh) && (isTaskEnabled == false)))
            {
                // Command generators which issue non-indexed draws generate DRAW_INDEX_AUTO packets, which will
                // invalidate some of our draw-time HW state. SEE: CmdDraw() for more details.
                // ExecuteIndirect Command Generator may modify the index buffer element size but PAL's state
                // tracking would fail to recognize this. So the index type may be set to 32 bit when its actually
                // 16 bit or vice versa. Which is why also include 'DrawIndexed' here.
                m_drawTimeHwState.dirty.indexedIndexType = 1;
            }
        }

        pDeCmdSpace = IncrementDeCounter(pDeCmdSpace);
        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }
}

// =====================================================================================================================
// This method uses the CmdGeneration compute shaders to first create the IB2 with indirect commands (PM4 packets) and
// then execute them.
void UniversalCmdBuffer::ExecuteIndirectShader(
    const IIndirectCmdGenerator& generator,
    gpusize                      gpuVirtAddr,
    uint32                       maximumCount,
    gpusize                      countGpuAddr)
{
    // It is only safe to generate indirect commands on a one-time-submit or exclusive-submit command buffer because
    // there is a potential race condition on the memory used to receive the generated commands.
    PAL_ASSERT(IsOneTimeSubmit() || IsExclusiveSubmit());

    const auto& gfx9Generator = static_cast<const IndirectCmdGenerator&>(generator);

    // The generation of indirect commands is determined by the currently-bound pipeline.
    const PipelineBindPoint bindPoint     = ((gfx9Generator.Type() == Pm4::GeneratorType::Dispatch) ?
                                             PipelineBindPoint::Compute : PipelineBindPoint::Graphics);
    const bool              setViewId     = (bindPoint == PipelineBindPoint::Graphics);
    const auto* const        pGfxPipeline =
        static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    uint32                  mask          = 1;

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

    // We are assuming that we will need to generate and execute maximumCount number of indirect commands.
    AutoBuffer<CmdStreamChunk*, 16, Platform> deChunks(maximumCount, m_device.GetPlatform());
    AutoBuffer<CmdStreamChunk*, 16, Platform> aceChunks(maximumCount, m_device.GetPlatform());

    const bool isTaskEnabled = ((gfx9Generator.Type() == Pm4::GeneratorType::DispatchMesh) &&
                                pGfxPipeline->HasTaskShader());

    const bool cmdGenUseAce = (m_cachedSettings.supportAceOffload &&
                              (m_device.Parent()->GetPublicSettings()->disableExecuteIndirectAceOffload != true) &&
                              (isTaskEnabled == false));

    if ((deChunks.Capacity() < maximumCount) || (isTaskEnabled && (aceChunks.Capacity() < maximumCount)))
    {
        NotifyAllocFailure();
    }
    else
    {
        CmdStreamChunk** ppChunkList[] =
        {
            deChunks.Data(),
            aceChunks.Data(),
        };
        uint32 numGenChunks = 0;
        const uint32 numChunkLists = (isTaskEnabled) ? 2 : 1;
        if (isTaskEnabled)
        {
            UpdateTaskMeshRingSize();
        }

        if (cmdGenUseAce)
        {
            EnableImplicitGangedSubQueueCount(1);
            GetAceCmdStream();
            CmdAceWaitDe();
        }

        for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
        {
            if (TestAnyFlagSet(mask, 1) == false)
            {
                continue;
            }

            // Generate the indirect command buffer chunk(s) using RPM. Since we're wrapping the command generation and
            // execution inside a CmdIf, we want to disable normal predication for this blit.
            const uint32 packetPredicate   = PacketPredicate();
            const uint32 numChunksExecuted = numGenChunks;
            m_pm4CmdBufState.flags.packetPredicate = 0;

            const Pm4::GenerateInfo genInfo =
            {
                this,
                (bindPoint == PipelineBindPoint::Graphics)
                    ? pGfxPipeline
                    : m_computeState.pipelineState.pPipeline,
                gfx9Generator,
                m_graphicsState.iaState.indexCount,
                maximumCount,
                gpuVirtAddr,
                countGpuAddr
            };

            bool requiresMeshTaskPipeStatsBuf = (m_pSignatureGfx->meshPipeStatsBufRegAddr != UserDataNotMapped);
            if (isTaskEnabled)
            {
                // The task shader signature is part of the HybridGraphicsPipeline, so we have to check it there
                // instead of inside the compute signature.
                const auto*const pHybridPipeline = static_cast<const HybridGraphicsPipeline*>(pGfxPipeline);
                requiresMeshTaskPipeStatsBuf |=
                    (pHybridPipeline->GetTaskSignature().taskPipeStatsBufRegAddr != UserDataNotMapped);
            }

            if (requiresMeshTaskPipeStatsBuf)
            {
                // If mesh/task shader requests buffer for emulated pipeline stats query, the buffer must be available
                // before launching execute indirect shader.
                PAL_ASSERT(m_meshPipeStatsGpuAddr != 0);
            }

            m_device.RsrcProcMgr().CmdGenerateIndirectCmds(genInfo,
                                                           &ppChunkList[0],
                                                           numChunkLists,
                                                           &numGenChunks);

            m_pm4CmdBufState.flags.packetPredicate = packetPredicate;

            if (cmdGenUseAce)
            {
                // When using the ACE for Indirect CmdGeneration we have to wait for the ACE side to finish that work
                // before the Draw() work can begin on the DE. This part performs a barrier count increment and wait
                // for DE.
                IssueGangedBarrierDeWaitAceIncr();
                CmdDeWaitAce();
            }
            else
            {
                uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

                // Insert a CS_PARTIAL_FLUSH to make sure that the generated commands are written out to L2 before we
                // attempt to execute them. Then, a PFP wait is also required so that the PFP doesn't prefetch the
                // generated commands before they are finished executing.
                AcquireMemGfxSurfSync acquireInfo = {};
                acquireInfo.cacheSync     = SyncGlkInv; // It's not clear why this was written with a K$ inv.
                acquireInfo.flags.pfpWait = 1;

                pDeCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, EngineTypeUniversal, pDeCmdSpace);
                pDeCmdSpace += m_cmdUtil.BuildAcquireMemGfxSurfSync(acquireInfo, pDeCmdSpace);

                m_deCmdStream.CommitCommands(pDeCmdSpace);
            }

            if (isTaskEnabled)
            {
                // In the case of task shaders, we need to make sure that the ACE side waits for the generator
                // shader to finish on the DE side before it attempts to move forward. This will perform the barrier
                // increment and the wait.
                IssueGangedBarrierAceWaitDeIncr();
                CmdAceWaitDe();

                // Just like a normal direct/indirect draw/dispatch, we need to perform state validation before
                // executing the generated command chunks.
                ValidateTaskMeshDispatch(0uLL, {});
            }

            if (bindPoint == PipelineBindPoint::Graphics)
            {
                // NOTE: If we tell ValidateDraw() that this draw call is indexed, it will validate all of the draw
                // time HW state related to the index buffer. However, since some indirect command generators can
                // generate the commands to bind their own index buffer state, our draw-time validation could be
                // redundant. Therefore, pretend this is a non-indexed draw call if the generated command binds
                // its own index buffer(s).
                Pm4::ValidateDrawInfo drawInfo;
                drawInfo.vtxIdxCount   = 0;
                drawInfo.instanceCount = 0;
                drawInfo.firstVertex   = 0;
                drawInfo.firstInstance = 0;
                drawInfo.firstIndex    = 0;
                drawInfo.useOpaque     = false;
                if (gfx9Generator.ContainsIndexBufferBind() || (gfx9Generator.Type() == Pm4::GeneratorType::Draw))
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
                ValidateDispatchPalAbi(&m_computeState, &m_deCmdStream, 0uLL, {});
                CommandGeneratorTouchedUserData(m_computeState.csUserDataEntries.touched,
                                                gfx9Generator,
                                                *m_pSignatureCs);
            }

            if (setViewId)
            {
                const auto& viewInstancingDesc = pGfxPipeline->GetViewInstancingDesc();

                uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
                pDeCmdSpace = BuildWriteViewId(viewInstancingDesc.viewId[i], pDeCmdSpace);
                m_deCmdStream.CommitCommands(pDeCmdSpace);
            }
            m_deCmdStream.ExecuteGeneratedCommands(ppChunkList[0],
                                                   numChunksExecuted,
                                                   numGenChunks);

            if (isTaskEnabled)
            {
                m_pAceCmdStream->ExecuteGeneratedCommands(ppChunkList[1],
                                                          numChunksExecuted,
                                                          numGenChunks);
            }

            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

            // We need to issue any post-draw or post-dispatch workarounds after all of the generated command buffers
            // have finished.
            if (bindPoint == PipelineBindPoint::Graphics)
            {
                if ((gfx9Generator.Type() == Pm4::GeneratorType::Draw) ||
                    (gfx9Generator.Type() == Pm4::GeneratorType::DrawIndexed) ||
                    ((gfx9Generator.Type() == Pm4::GeneratorType::DispatchMesh) && (isTaskEnabled == false)))
                {
                    // Command generators which issue non-indexed draws generate DRAW_INDEX_AUTO packets, which will
                    // invalidate some of our draw-time HW state. SEE: CmdDraw() for more details.
                    // ExecuteIndirect Command Generator may modify the index buffer element size but PAL's state
                    // tracking would fail to recognize this. So the index type may be set to 32 bit when its actually
                    // 16 bit or vice versa. Which is why also include 'DrawIndexed' here.
                    m_drawTimeHwState.dirty.indexedIndexType = 1;
                }
            }

            pDeCmdSpace = IncrementDeCounter(pDeCmdSpace);
            m_deCmdStream.CommitCommands(pDeCmdSpace);
        } // For each set bit in the mask.

    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdExecuteIndirectCmds(
    const IIndirectCmdGenerator& generator,
    gpusize                      gpuVirtAddr,
    uint32                       maximumCount,
    gpusize                      countGpuAddr
)
{
    const auto& gfx9Generator      = static_cast<const IndirectCmdGenerator&>(generator);
    const auto* const pGfxPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const auto& properties         = gfx9Generator.Properties();

    const uint32 gfxSpillDwords  =
        ((m_pSignatureGfx->spillThreshold <= properties.userDataWatermark) ? properties.maxUserDataEntries : 0);

    const bool userDataSpillTableUsedButNotSupported =
        (m_device.Parent()->Settings().useExecuteIndirectPacket < UseExecuteIndirectV1PacketForDrawSpillTable) &&
        (gfxSpillDwords > 0);

    const bool isTaskShaderEnabled = (pGfxPipeline != nullptr) && (pGfxPipeline->IsTaskShaderEnabled());
    PAL_ASSERT((isTaskShaderEnabled == false) || (gfx9Generator.Type() == Pm4::GeneratorType::DispatchMesh));

    if (m_cachedSettings.describeDrawDispatch)
    {
        DescribeExecuteIndirectCmds(this, static_cast<uint32>(gfx9Generator.Type()));
    }

    if (gfx9Generator.UsingExecuteIndirectPacket()       &&
        (userDataSpillTableUsedButNotSupported == false) &&
        (isTaskShaderEnabled == false))
    {
        // The case where countGpuAddr is zero is handled by packet.ordinal4.bitfields.count_indirect_enable in
        // Pal::Gfx9::CmdUtil::BuildExecuteIndirect()
        ExecuteIndirectPacket(generator, gpuVirtAddr, maximumCount, countGpuAddr);
    }
    else
    {
        if (countGpuAddr == 0uLL)
        {
            // If the count GPU address is zero, then we are expected to use the maximumCount value as the actual number
            // of indirect commands to generate and execute.
            // If the count GPU address is not zero we use the actual cmd count = min(*countGpuAddr, maximumCount)
            uint32* pMemory = CmdAllocateEmbeddedData(1, 1, &countGpuAddr);
            *pMemory = maximumCount;
        }
        ExecuteIndirectShader(generator, gpuVirtAddr, maximumCount, countGpuAddr);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdDispatchAce(
    DispatchDims size)
{
    // Calling CmdDispatchAce requires a check whether multi-queue is supported on the Universal engine from which this
    // function was called. The callee should ensure that it's never called when not supported as that case is not
    // handled. We only do an assert here.
#if PAL_ENABLE_PRINTS_ASSERTS
    PAL_ASSERT(m_cachedSettings.supportAceOffload);
#endif
    auto* pAceCmdStream = GetAceCmdStream();

    if (m_cachedSettings.describeDrawDispatch)
    {
        DescribeDispatch(Developer::DrawDispatchType::CmdDispatchAce, size);
    }

    const auto* pComputePipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);
    const ComputePipelineSignature& pSignature = pComputePipeline->Signature();

    // We create a new local compute state and mark all the bits dirty so that we rewrite entries on ValidateDispatch
    // on this CmdStream because state on the ACE stream cannot be relied on here.
    ComputeState tempComputeState                      = m_computeState;
    tempComputeState.pipelineState.pPipeline           = m_computeState.pipelineState.pPipeline;
    tempComputeState.pipelineState.apiPsoHash          = m_computeState.pipelineState.apiPsoHash;
    tempComputeState.pipelineState.dirtyFlags.pipeline = 1;

    // Copy the cs user-data entries on to this temporary ComputeState.
    memcpy(&tempComputeState.csUserDataEntries.entries,
           &(m_computeState.csUserDataEntries),
           sizeof(uint32) * pSignature.userDataLimit);

    memset(&tempComputeState.csUserDataEntries.dirty, -1, sizeof(tempComputeState.csUserDataEntries.dirty));

    ValidateDispatchPalAbi(&tempComputeState, pAceCmdStream, 0uLL, size);

    uint32* pAceCmdSpace = pAceCmdStream->ReserveCommands();

    pAceCmdSpace += m_cmdUtil.BuildDispatchDirect<false, true>(size,
                                                               PacketPredicate(),
                                                               m_pSignatureCs->flags.isWave32,
                                                               UsesDispatchTunneling(),
                                                               false,
                                                               pAceCmdSpace);

    if (m_cachedSettings.issueSqttMarkerEvent)
    {
        pAceCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                           EngineTypeCompute,
                                                           PacketPredicate(),
                                                           pAceCmdSpace);
    }

    pAceCmdStream->CommitCommands(pAceCmdSpace);

    // If this function was called it means we will be using the ImplicitAceCmdStream for Indirect Cmd Generation.
    // So we will set this flag here to ensure gang submission is used when MS HWS is enabled.
    EnableImplicitGangedSubQueueCount(1);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdCommentString(
    const char* pComment)
{
    const struct
    {
        Pal::CmdStream* pStream;
        Pm4ShaderType   shaderType;
    } streams[] =
    {
        { &m_deCmdStream,  ShaderGraphics, },
        { m_pAceCmdStream, ShaderCompute,  },
    };

    for (uint32 i = 0; i < Util::ArrayLen(streams); i++)
    {
        Pal::CmdStream* pStream = streams[i].pStream;
        if (pStream != nullptr)
        {
            uint32* pCmdSpace = pStream->ReserveCommands();
            pCmdSpace += m_cmdUtil.BuildCommentString(pComment, streams[i].shaderType, pCmdSpace);
            pStream->CommitCommands(pCmdSpace);
        }
    }
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
void UniversalCmdBuffer::GetChunkForCmdGeneration(
    const Pm4::IndirectCmdGenerator& generator,
    const Pal::Pipeline&             pipeline,
    uint32                           maxCommands,
    uint32                           numChunkOutputs,
    ChunkOutput*                     pChunkOutputs)
{
    const auto& properties        = generator.Properties();
    const bool  taskShaderEnabled = (numChunkOutputs == 2);
    PAL_ASSERT((taskShaderEnabled == false) || pipeline.IsTaskShaderEnabled());

    PAL_ASSERT(m_pCmdAllocator != nullptr);
    PAL_ASSERT((numChunkOutputs > 0) && (numChunkOutputs <= 2));

    const Pm4::CmdStream* pStreams[] =
    {
        &m_deCmdStream,
        m_pAceCmdStream,
    };

    for (uint32 i = 0; i < numChunkOutputs; i++)
    {
        const Pm4::CmdStream* pStream = pStreams[i];
        ChunkOutput* pOutput          = &pChunkOutputs[i];

        pOutput->pChunk = Pal::Pm4CmdBuffer::GetNextGeneratedChunk();

        const uint32* pUserDataEntries = nullptr;
        bool usesVertexBufTable        = false;
        uint32 spillThreshold          = NoUserDataSpilling;

        const bool generateTaskChunk = ((i == 1) && (pipeline.IsTaskShaderEnabled() == true));

        if (generator.Type() == Pm4::GeneratorType::Dispatch)
        {
            const auto& signature = static_cast<const ComputePipeline&>(pipeline).Signature();
            spillThreshold        = signature.spillThreshold;

            // NOTE: RPM uses a compute shader to generate indirect commands, so we need to use the saved user-data
            // state because RPM will have pushed its own state before calling this method.
            pUserDataEntries = &m_computeRestoreState.csUserDataEntries.entries[0];
        }
        else
        {
            if (generateTaskChunk)
            {
                const auto& signature = static_cast<const HybridGraphicsPipeline&>(pipeline).GetTaskSignature();
                spillThreshold = signature.spillThreshold;

                pUserDataEntries = &m_computeRestoreState.csUserDataEntries.entries[0];
            }
            else
            {

                const auto& signature = static_cast<const GraphicsPipeline&>(pipeline).Signature();
                usesVertexBufTable = (signature.vertexBufTableRegAddr != 0);
                spillThreshold = signature.spillThreshold;

                // NOTE: RPM uses a compute shader to generate indirect commands, which doesn't interfere with the graphics
                // state, so we don't need to look at the pushed state.
                pUserDataEntries = &m_graphicsState.gfxUserDataEntries.entries[0];
            }
        }

        // Total amount of embedded data space needed for each generated command, including indirect user-data tables
        // and user-data spilling.
        uint32 embeddedDwords = 0;
        // Amount of embedded data space needed for each generated command, for the vertex buffer table:
        uint32 vertexBufTableDwords = 0;
        // User-data high watermark for this command Generator. It depends on the command Generator itself, as well as
        // the pipeline signature for the active pipeline. This is due to the fact that if the command Generator
        // modifies the contents of an indirect user-data table, the command Generator must also fix-up the user-data
        // entry used for the table's GPU virtual address.
        uint32 userDataWatermark = properties.userDataWatermark;

        if (usesVertexBufTable && (properties.vertexBufTableSize != 0))
        {
            vertexBufTableDwords = properties.vertexBufTableSize;
            embeddedDwords      += vertexBufTableDwords;
        }

        const uint32 commandDwords = generator.CmdBufStride(&pipeline) / sizeof(uint32);
        // There are three possibilities when determining how much spill-table space a generated command will need:
        //  (1) The active pipeline doesn't spill at all. This requires no spill-table space.
        //  (2) The active pipeline spills, but the generator doesn't update the any user-data entries beyond the
        //      spill threshold. This requires no spill-table space.
        //  (3) The active pipeline spills, and the generator updates user-data entries which are beyond the spill
        //      threshold. This means each generated command needs to relocate the spill table in addition to the other
        //      stuff it would normally do.
        const uint32 spillDwords = (spillThreshold <= userDataWatermark) ? properties.maxUserDataEntries : 0;
        embeddedDwords          += spillDwords;

        pOutput->commandsInChunk = pStream->PrepareChunkForCmdGeneration(pOutput->pChunk,
                                                                         commandDwords,
                                                                         embeddedDwords,
                                                                         maxCommands);
        pOutput->embeddedDataSize = (pOutput->commandsInChunk * embeddedDwords);

        // Populate command buffer chain size required later for an indirect command generation optimization.
        pOutput->chainSizeInDwords = CmdUtil::ChainSizeInDwords(EngineTypeUniversal);

        if (embeddedDwords > 0)
        {
            // If each generated command requires some amount of spill-table space, then we need to allocate embeded
            // data space for all of the generated commands which will go into this chunk.
            // PrepareChunkForCmdGeneration() should have determined a value for commandsInChunk which allows us to
            // allocate the appropriate amount of embeded data space.
            uint32* pDataSpace = pOutput->pChunk->ValidateCmdGenerationDataSpace(pOutput->embeddedDataSize,
                                                                                 &(pOutput->embeddedDataAddr));
            // We also need to seed the embedded data for each generated command with the current indirect user-data
            // table and spill-table contents, because the generator will only update the table entries which get
            // modified.
            for (uint32 cmd = 0; cmd < pOutput->commandsInChunk; ++cmd)
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
    }
}

// =====================================================================================================================
// Helper method for handling the state "leakage" from a nested command buffer back to its caller. Since the callee has
// tracked its own state during the building phase, we can access the final state of the command buffer since its stored
// in the UniversalCmdBuffer object itself.
void UniversalCmdBuffer::LeakNestedCmdBufferState(
    const UniversalCmdBuffer& cmdBuffer)
{
    Pal::Pm4::UniversalCmdBuffer::LeakNestedCmdBufferState(cmdBuffer);

    if (cmdBuffer.m_graphicsState.leakFlags.colorTargetView != 0)
    {
        CopyColorTargetViewStorage(m_colorTargetViewStorage, cmdBuffer.m_colorTargetViewStorage, &m_graphicsState);
    }

    if (cmdBuffer.m_graphicsState.leakFlags.depthStencilView != 0)
    {
        CopyDepthStencilViewStorage(&m_depthStencilViewStorage, &cmdBuffer.m_depthStencilViewStorage, &m_graphicsState);
    }

    if (cmdBuffer.m_graphicsState.pipelineState.pPipeline != nullptr)
    {
        m_vertexOffsetReg     = cmdBuffer.m_vertexOffsetReg;
        m_drawIndexReg        = cmdBuffer.m_drawIndexReg;
        m_nggState.numSamples = cmdBuffer.m_nggState.numSamples;

        // Update the functions that are modified by nested command list
        m_pfnValidateUserDataGfx               = cmdBuffer.m_pfnValidateUserDataGfx;
        m_pfnValidateUserDataGfxPipelineSwitch = cmdBuffer.m_pfnValidateUserDataGfxPipelineSwitch;

        if (m_cachedSettings.rbPlusSupported != 0)
        {
            m_sxPsDownconvert   = cmdBuffer.m_sxPsDownconvert;
            m_sxBlendOptEpsilon = cmdBuffer.m_sxBlendOptEpsilon;
            m_sxBlendOptControl = cmdBuffer.m_sxBlendOptControl;
        }

        m_pipelineDynRegsDirty = cmdBuffer.m_pipelineDynRegsDirty;
    }

    BitfieldUpdateSubfield(
        &(m_dbRenderOverride.u32All), cmdBuffer.m_dbRenderOverride.u32All, PipelineDbRenderOverrideMask);

    m_dbShaderControl = cmdBuffer.m_dbShaderControl;
    m_cbColorControl  = cmdBuffer.m_cbColorControl;
    m_paClClipCntl    = cmdBuffer.m_paClClipCntl;
    m_cbTargetMask    = cmdBuffer.m_cbTargetMask;
    m_cbShaderMask    = cmdBuffer.m_cbShaderMask;
    m_vgtTfParam      = cmdBuffer.m_vgtTfParam;
    m_paScLineCntl    = cmdBuffer.m_paScLineCntl;
    m_depthClampMode  = cmdBuffer.m_depthClampMode;

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

    if (cmdBuffer.m_graphicsState.leakFlags.depthStencilView)
    {
        BitfieldUpdateSubfield(&(m_dbRenderOverride.u32All),
                               cmdBuffer.m_dbRenderOverride.u32All,
                               DepthStencilView::DbRenderOverrideRmwMask);
    }

    // If the nested command buffer updated PA_SC_CONS_RAST_CNTL, leak its state back to the caller.
    if ((cmdBuffer.m_graphicsState.pipelineState.pPipeline != nullptr) ||
        (cmdBuffer.m_graphicsState.leakFlags.msaaState))
    {
        m_paScConsRastCntl.u32All = cmdBuffer.m_paScConsRastCntl.u32All;
    }

    // If the nested command buffer updated PA_SU_SC_MODE_CNTL, leak its state back to the caller.
    if (cmdBuffer.m_graphicsState.leakFlags.triangleRasterState)
    {
        m_paSuScModeCntl.u32All = cmdBuffer.m_paSuScModeCntl.u32All;
    }

    // If the nested command buffer updated color target view (and implicitly big_page settings), leak the state back to
    // caller as the state tracking is needed for correctly making the WA.
    if (cmdBuffer.m_graphicsState.leakFlags.colorTargetView)
    {
        m_cbRmiGl2CacheControl.bits.COLOR_BIG_PAGE = cmdBuffer.m_cbRmiGl2CacheControl.bits.COLOR_BIG_PAGE;

        if (IsGfx10(m_gfxIpLevel))
        {
            m_cbRmiGl2CacheControl.gfx10.FMASK_BIG_PAGE = cmdBuffer.m_cbRmiGl2CacheControl.gfx10.FMASK_BIG_PAGE;
        }
    }

    // This state is also always updated by the nested command buffer and should leak back.
    m_paScAaConfigNew.u32All  = cmdBuffer.m_paScAaConfigNew.u32All;
    m_paScAaConfigLast.u32All = cmdBuffer.m_paScAaConfigLast.u32All;

    if (cmdBuffer.HasStreamOutBeenSet())
    {
        // If the nested command buffer set their own stream-out targets, we can simply copy the SRD's because CE
        // RAM is up-to-date.
        memcpy(&m_streamOut.srd[0], &cmdBuffer.m_streamOut.srd[0], sizeof(m_streamOut.srd));
    }

    if (cmdBuffer.m_swStreamoutDataAddr != 0)
    {
        // If the nested command buffer allocated their own streamout control buffer, we can just start using it
        // because it has the most recent, valid data.
        m_swStreamoutDataAddr = cmdBuffer.m_swStreamoutDataAddr;
    }

    m_drawTimeHwState.valid.u32All = 0;

    //Update vgtDmaIndexType register if the nested command buffer updated the graphics iaStates
    if (m_graphicsState.dirtyFlags.iaState !=0 )
    {
        m_drawTimeHwState.dirty.indexType = 1;
        m_vgtDmaIndexType.bits.INDEX_TYPE = VgtIndexTypeLookup[static_cast<uint32>(m_graphicsState.iaState.indexType)];
    }

    m_vbTable.state.dirty       |= cmdBuffer.m_vbTable.modified;
    m_vbTable.watermark          = cmdBuffer.m_vbTable.watermark;
    m_spillTable.stateCs.dirty  |= cmdBuffer.m_spillTable.stateCs.dirty;
    m_spillTable.stateGfx.dirty |= cmdBuffer.m_spillTable.stateGfx.dirty;

    // Ensure next ValidateDraw writes this register.
    m_prevDbRenderOverride.u32All = ~m_dbRenderOverride.u32All;

    m_rbplusRegHash        = cmdBuffer.m_rbplusRegHash;
    m_pipelineCtxRegHash   = cmdBuffer.m_pipelineCtxRegHash;
    m_pipelineCfgRegHash   = cmdBuffer.m_pipelineCfgRegHash;
    m_pipelinePsHash       = cmdBuffer.m_pipelinePsHash;
    m_pipelineState        = cmdBuffer.m_pipelineState;

    if (cmdBuffer.m_graphicsState.pipelineState.dirtyFlags.pipeline ||
        (cmdBuffer.m_graphicsState.pipelineState.pPipeline != nullptr))
    {
        m_spiPsInControl = cmdBuffer.m_spiPsInControl;
        m_spiVsOutConfig = cmdBuffer.m_spiVsOutConfig;
        m_vgtLsHsConfig  = cmdBuffer.m_vgtLsHsConfig;
        m_geCntl         = cmdBuffer.m_geCntl;
    }

    m_nggState.flags.hasPrimShaderWorkload |= cmdBuffer.m_nggState.flags.hasPrimShaderWorkload;
    m_nggState.flags.dirty                 |= cmdBuffer.m_nggState.flags.dirty;

    if (cmdBuffer.m_deCmdStream.IsPreemptionEnabled() == false)
    {
        m_deCmdStream.DisablePreemption();
    }
    m_pSignatureCs  = cmdBuffer.m_pSignatureCs;
    m_pSignatureGfx = cmdBuffer.m_pSignatureGfx;

    SetShaderRingSize(cmdBuffer.m_ringSizes);

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
        uint32 strideInBytes = (sizeof(uint32) * pPipeline->StrmoutVtxStrideDw(idx));

        if (SupportsSwStrmout() && (strideInBytes > 0))
        {
            // Software streamout only supports byte-addressed buffers, which is indicated by passing
            //  Stride=1 and buffer size in NumRecords.
            strideInBytes = 1;
        }

        const uint32 sizeInBytes = LowPart(m_graphicsState.bindStreamOutTargets.target[idx].size);
        const uint32 numRecords  = StreamOutNumRecords(chipProps, sizeInBytes, strideInBytes);
        auto*const   pBufferSrd  = &m_streamOut.srd[idx];

        if ((pBufferSrd->num_records != numRecords) || (pBufferSrd->stride != strideInBytes))
        {
            pBufferSrd->num_records = numRecords;
            pBufferSrd->stride      = strideInBytes;

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
void UniversalCmdBuffer::CmdPrimeGpuCaches(
    uint32                    rangeCount,
    const PrimeGpuCacheRange* pRanges)
{
    PAL_ASSERT((rangeCount == 0) || (pRanges != nullptr));

    for (uint32 i = 0; i < rangeCount; ++i)
    {
        uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

        pCmdSpace += m_cmdUtil.BuildPrimeGpuCaches(pRanges[i], EngineTypeUniversal, pCmdSpace);

        m_deCmdStream.CommitCommands(pCmdSpace);
    }
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
}

// =====================================================================================================================
// Sets clip rects.
void UniversalCmdBuffer::CmdSetClipRects(
    uint16      clipRule,
    uint32      rectCount,
    const Rect* pRectList)
{
    PAL_ASSERT(rectCount <= Pm4::MaxClipRects);

    m_graphicsState.clipRectsState.clipRule  = clipRule;
    m_graphicsState.clipRectsState.rectCount = rectCount;
    for (uint32 i = 0; i < rectCount; i++)
    {
        m_graphicsState.clipRectsState.rectList[i] = pRectList[i];
    }
    m_graphicsState.dirtyFlags.clipRectsState = 1;

    constexpr uint32 RegStride = (mmPA_SC_CLIPRECT_1_TL - mmPA_SC_CLIPRECT_0_TL);
    const uint32 endRegAddr    = (mmPA_SC_CLIPRECT_RULE + rectCount * RegStride);

    struct
    {
        regPA_SC_CLIPRECT_RULE paScClipRectRule;
        struct
        {
            regPA_SC_CLIPRECT_0_TL tl;
            regPA_SC_CLIPRECT_0_BR br;
        } paScClipRect[Pm4::MaxClipRects];
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
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdXdmaWaitFlipPending()
{
    // XDMA was retired starting in gfx10 so this function has no use anymore. We'll remove it soon.
}

// =====================================================================================================================
// Need to validate some state as it is valid for root CmdBuf to set state, not issue a draw and expect
// that state to inherit into the nested CmdBuf. It might be safest to just ValidateDraw here eventually.
// That would break the assumption that the Pipeline is bound at draw-time.
void UniversalCmdBuffer::ValidateExecuteNestedCmdBuffer()
{
    uint32*    pDeCmdSpace = m_deCmdStream.ReserveCommands();
    const auto dirtyFlags  = m_graphicsState.dirtyFlags;
    if (m_graphicsState.pipelineState.dirtyFlags.pipeline)
    {
        if (dirtyFlags.u32All)
        {
            pDeCmdSpace = ValidateCbColorInfoAndBlendState<false, true, true>(pDeCmdSpace);
        }
        else
        {
            pDeCmdSpace = ValidateCbColorInfoAndBlendState<false, true, false>(pDeCmdSpace);
        }
        pDeCmdSpace = ValidateDbRenderOverride(pDeCmdSpace);
    }
    else
    {
        if (dirtyFlags.colorBlendState || dirtyFlags.colorTargetView)
        {
            pDeCmdSpace = ValidateCbColorInfoAndBlendState<false, false, true>(pDeCmdSpace);
        }
        if (m_pipelineDynRegsDirty || dirtyFlags.depthStencilView)
        {
            pDeCmdSpace = ValidateDbRenderOverride(pDeCmdSpace);
        }
    }

    if (dirtyFlags.occlusionQueryActive)
    {
        const auto*const pMsaaState  = static_cast<const MsaaState*>(m_graphicsState.pMsaaState);
        const uint32 log2OcclusionQuerySamples = (pMsaaState != nullptr) ? pMsaaState->Log2OcclusionQuerySamples() : 0;
        pDeCmdSpace = UpdateDbCountControl<false>(log2OcclusionQuerySamples, pDeCmdSpace);
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32            cmdBufferCount,
    ICmdBuffer*const* ppCmdBuffers)
{
    ValidateExecuteNestedCmdBuffer();

    for (uint32 buf = 0; buf < cmdBufferCount; ++buf)
    {
        auto*const pCallee = static_cast<Gfx9::UniversalCmdBuffer*>(ppCmdBuffers[buf]);
        PAL_ASSERT(pCallee != nullptr);

        CallNestedCmdBuffer(pCallee);

        // Callee command buffers are also able to leak any changes they made to bound user-data entries and any other
        // state back to the caller.
        LeakNestedCmdBufferState(*pCallee);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CallNestedCmdBuffer(
    UniversalCmdBuffer* pCallee)
{
    // Track the most recent OS paging fence value across all nested command buffers called from this one.
    m_lastPagingFence = Max(m_lastPagingFence, pCallee->LastPagingFence());

    // Track the lastest fence token across all nested command buffers called from this one.
    m_maxUploadFenceToken = Max(m_maxUploadFenceToken, pCallee->GetMaxUploadFenceToken());

    // All user-data entries have been uploaded into CE RAM and GPU memory, so we can safely "call" the nested
    // command buffer's command streams.
    PAL_ASSERT(pCallee->IsNested());

    const bool exclusiveSubmit  = pCallee->IsExclusiveSubmit();
    const bool allowIb2Launch   = ((IsNested() == false) && pCallee->AllowLaunchViaIb2());
    const bool allowIb2LaunchCe = (allowIb2Launch && (m_cachedSettings.waCeDisableIb2 == 0));

    m_deCmdStream.TrackNestedEmbeddedData(pCallee->m_embeddedData.chunkList);
    m_deCmdStream.TrackNestedEmbeddedData(pCallee->m_gpuScratchMem.chunkList);

    if ((pCallee->m_pAceCmdStream != nullptr) && (pCallee->m_pAceCmdStream->IsEmpty() == false))
    {
        CmdStream* pAceCmdStream = GetAceCmdStream();
        pAceCmdStream->TrackNestedCommands(*(pCallee->m_pAceCmdStream));
        pAceCmdStream->Call(*(pCallee->m_pAceCmdStream), exclusiveSubmit, false);

        EnableImplicitGangedSubQueueCount(1);
    }

    m_deCmdStream.TrackNestedCommands(pCallee->m_deCmdStream);
    m_ceCmdStream.TrackNestedCommands(pCallee->m_ceCmdStream);

    m_deCmdStream.Call(pCallee->m_deCmdStream, exclusiveSubmit, allowIb2Launch);
    m_ceCmdStream.Call(pCallee->m_ceCmdStream, exclusiveSubmit, allowIb2LaunchCe);

    if (allowIb2Launch)
    {
        TrackIb2DumpInfoFromExecuteNestedCmds(pCallee->m_deCmdStream);

        if ((pCallee->m_pAceCmdStream != nullptr) && (pCallee->m_pAceCmdStream->IsEmpty() == false))
        {
            TrackIb2DumpInfoFromExecuteNestedCmds(*(pCallee->m_pAceCmdStream));
        }
    }
    if (allowIb2LaunchCe)
    {
        TrackIb2DumpInfoFromExecuteNestedCmds(pCallee->m_ceCmdStream);
    }

}

// =====================================================================================================================
// When RB+ is enabled, pipelines are created per shader export format.  However, same export format possibly supports
// several down convert formats. For example, FP16_ABGR supports 8_8_8_8, 5_6_5, 1_5_5_5, 4_4_4_4, etc.  This updates
// the current RB+ PM4 image with the overridden values.
// NOTE: This is expected to be called immediately after RPM binds a graphics pipeline!
void UniversalCmdBuffer::CmdOverwriteColorExportInfoForBlits(
    SwizzledFormat format,
    uint32         targetIndex)
{
    const auto*const pPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    PAL_ASSERT(pPipeline != nullptr);

    // Just update our PM4 image for RB+.  It will be written at draw-time along with the other pipeline registers.
    if (targetIndex != 0)
    {
        pPipeline->OverrideMrtMappingRegistersForRpm(targetIndex,
                                                     &m_cbShaderMask,
                                                     &m_cbTargetMask,
                                                     &m_sxPsDownconvert,
                                                     &m_sxBlendOptEpsilon,
                                                     &m_sxBlendOptControl);
        m_pipelineDynRegsDirty = true;
        m_rbplusRegHash = 0;
    }

    if (m_cachedSettings.rbPlusSupported != 0)
    {
        pPipeline->OverrideRbPlusRegistersForRpm(format,
                                                 targetIndex,
                                                 &m_sxPsDownconvert,
                                                 &m_sxBlendOptEpsilon,
                                                 &m_sxBlendOptControl);
        m_rbplusRegHash = 0;
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
            pCmdSpace = SetUserSgprReg<ShaderGraphics>(viewIdRegAddr, viewId, false, pCmdSpace);
        }
        else
        {
            break;
        }
    }

    // View Instance IDs must be written in order with their associated state, eg. written in a DRAW_INDEX_AUTO /
    // DISPATCH_TASKMESH_DIRECT etc. packet. Because View Instance IDs are always written after a call to validate draw
    // we may simply use the valid user entry array to store and write view instance ID as all other relevant user data
    // will have already been written via ValidateDraw()
    if (m_numValidUserEntries > 0)
    {
        pCmdSpace = WritePackedUserDataEntriesToSgprs<ShaderGraphics>(pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Switch draw functions - the actual assignment
template <bool ViewInstancing, bool HasUavExport, bool IssueSqtt, bool DescribeDrawDispatch>
void UniversalCmdBuffer::SwitchDrawFunctionsInternal(
    bool nativeMsEnable,
    bool hasTaskShader)
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

    if (hasTaskShader)
    {
        // Task + Gfx pipeline.
        m_funcTable.pfnCmdDispatchMesh =
            CmdDispatchMeshTask<IssueSqtt, HasUavExport, ViewInstancing, DescribeDrawDispatch>;
        m_funcTable.pfnCmdDispatchMeshIndirectMulti =
            CmdDispatchMeshIndirectMultiTask<IssueSqtt, ViewInstancing, DescribeDrawDispatch>;
    }
    else
    {
        // Mesh shader only pipeline.
        if (nativeMsEnable)
        {
            m_funcTable.pfnCmdDispatchMesh =
                CmdDispatchMeshNative<IssueSqtt, HasUavExport, ViewInstancing, DescribeDrawDispatch>;
        }
        else
        {
            m_funcTable.pfnCmdDispatchMesh =
                CmdDispatchMeshAmpFastLaunch<IssueSqtt, HasUavExport, ViewInstancing, DescribeDrawDispatch>;
        }

        m_funcTable.pfnCmdDispatchMeshIndirectMulti =
            CmdDispatchMeshIndirectMulti<IssueSqtt, ViewInstancing, DescribeDrawDispatch>;
    }
}

// =====================================================================================================================
// Switch draw functions - overloaded internal implementation for switching function params to template params
template <bool ViewInstancing, bool IssueSqtt, bool DescribeDrawDispatch>
void UniversalCmdBuffer::SwitchDrawFunctionsInternal(
    bool hasUavExport,
    bool nativeMsEnable,
    bool hasTaskShader)
{
    if (hasUavExport)
    {
        SwitchDrawFunctionsInternal<ViewInstancing, true, IssueSqtt, DescribeDrawDispatch>(nativeMsEnable,
                                                                                           hasTaskShader);
    }
    else
    {
        SwitchDrawFunctionsInternal<ViewInstancing, false, IssueSqtt, DescribeDrawDispatch>(nativeMsEnable,
                                                                                            hasTaskShader);
    }
}

// =====================================================================================================================
// Switch draw functions - overloaded internal implementation for switching function params to template params
template <bool IssueSqtt, bool DescribeDrawDispatch>
void UniversalCmdBuffer::SwitchDrawFunctionsInternal(
    bool hasUavExport,
    bool viewInstancingEnable,
    bool nativeMsEnable,
    bool hasTaskShader)
{
    if (viewInstancingEnable)
    {
        SwitchDrawFunctionsInternal<true, IssueSqtt, DescribeDrawDispatch>(hasUavExport,
                                                                           nativeMsEnable,
                                                                           hasTaskShader);
    }
    else
    {
        SwitchDrawFunctionsInternal<false, IssueSqtt, DescribeDrawDispatch>(hasUavExport,
                                                                            nativeMsEnable,
                                                                            hasTaskShader);
    }
}

// =====================================================================================================================
// Switch draw functions.
void UniversalCmdBuffer::SwitchDrawFunctions(
    bool hasUavExport,
    bool viewInstancingEnable,
    bool nativeMsEnable,
    bool hasTaskShader)
{
    if (m_cachedSettings.issueSqttMarkerEvent)
    {
        PAL_ASSERT(m_cachedSettings.describeDrawDispatch == 1);
        SwitchDrawFunctionsInternal<true, true>(hasUavExport,
                                                viewInstancingEnable,
                                                nativeMsEnable,
                                                hasTaskShader);
    }
    else if (m_cachedSettings.describeDrawDispatch)
    {
        SwitchDrawFunctionsInternal<false, true>(hasUavExport,
                                                 viewInstancingEnable,
                                                 nativeMsEnable,
                                                 hasTaskShader);
    }
    else
    {
        SwitchDrawFunctionsInternal<false, false>(hasUavExport,
                                                  viewInstancingEnable,
                                                  nativeMsEnable,
                                                  hasTaskShader);
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
    dmaDataInfo.predicate   = static_cast<Pm4Predicate>(GetPm4CmdBufState().flags.packetPredicate);
    dmaDataInfo.dstAddr     = dstAddr;
    dmaDataInfo.srcAddr     = srcAddr;
    dmaDataInfo.numBytes    = static_cast<uint32>(numBytes);

    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
    pCmdSpace += CmdUtil::BuildDmaData<false>(dmaDataInfo, pCmdSpace);
    m_deCmdStream.CommitCommands(pCmdSpace);

    SetCpBltState(true);
    SetCpBltWriteCacheState(true);

#if PAL_DEVELOPER_BUILD
    Developer::RpmBltData cbData = { .pCmdBuffer = this, .bltType = Developer::RpmBltType::CpDmaCopy };
    m_device.Parent()->DeveloperCb(Developer::CallbackType::RpmBlt, &cbData);
#endif
}

// =====================================================================================================================
// Returns the parent GfxCmdStream's ACE CmdStream as a Gfx9::CmdStream. Creates and initializes the ACE CmdStream if
// it is the first time this is called.
CmdStream* UniversalCmdBuffer::GetAceCmdStream()
{
    if (m_pAceCmdStream == nullptr)
    {
        // This is the first time the ACE CmdStream is being used. So create and initialize the ACE CmdStream
        // and the associated GpuEvent object additionally.
        m_pAceCmdStream = PAL_NEW(CmdStream, m_device.GetPlatform(), AllocInternal)(m_device,
                                                                                    m_pCmdAllocator,
                                                                                    EngineTypeCompute,
                                                                                    SubEngineType::AsyncCompute,
                                                                                    CmdStreamUsage::Workload,
                                                                                    IsNested());

        Result result = Result::Success;
        if (m_pAceCmdStream != nullptr)
        {
            result = m_pAceCmdStream->Init();
        }
        else
        {
            NotifyAllocFailure();
            result = Result::ErrorOutOfMemory;
        }

        if (result == Result::Success)
        {
            const PalSettings& coreSettings = m_device.Parent()->Settings();

            CmdStreamBeginFlags cmdStreamFlags = {};
            cmdStreamFlags.prefetchCommands    = m_buildFlags.prefetchCommands;
            cmdStreamFlags.optimizeCommands    =
                (((coreSettings.cmdBufOptimizePm4 == Pm4OptDefaultEnable) && m_buildFlags.optimizeGpuSmallBatch) ||
                (coreSettings.cmdBufOptimizePm4 == Pm4OptForceEnable));

            result = m_pAceCmdStream->Begin(cmdStreamFlags, m_pMemAllocator);
        }

        if (result == Result::Success)
        {
            result = ComputeCmdBuffer::WritePreambleCommands(m_cmdUtil, static_cast<CmdStream*>(m_pAceCmdStream));
        }

        // Creation of the Ace CmdStream failed.
        PAL_ASSERT(result == Result::Success);

        if (result != Result::Success)
        {
            SetCmdRecordingError(result);
        }
        else
        {
            // We need to properly issue a stall in case we're requesting the ACE CmdStream after a barrier call.
            IssueGangedBarrierAceWaitDeIncr();

            if (m_deferredPipelineStatsQueries.IsEmpty() == false)
            {
                // We must wait for the DE before applying the deferred queries on the ACE queue because this command
                // buffer might have reset the query slot before beginning the query.
                CmdAceWaitDe();

                uint32* pAceCmdSpace = m_pAceCmdStream->ReserveCommands();

                // Apply the deferred Begin() operation on any pipeline-stats queries we've accumulated before the
                // ganged ACE stream was initialized.
                for (const auto& state: m_deferredPipelineStatsQueries)
                {
                    PAL_ASSERT(state.pQueryPool != nullptr);
                    pAceCmdSpace = state.pQueryPool->DeferredBeginOnGangedAce(this, pAceCmdSpace, state.slot);
                }
                m_pAceCmdStream->CommitCommands(pAceCmdSpace);

                m_deferredPipelineStatsQueries.Clear();
            }
        }
    }

    return static_cast<CmdStream*>(m_pAceCmdStream);
}

// =====================================================================================================================
// Allocates memory for the command stream sync semaphore if not already allocated.
gpusize UniversalCmdBuffer::GangedCmdStreamSemAddr()
{
    if (m_gangedCmdStreamSemAddr == 0)
    {
        uint32* pData = CmdAllocateEmbeddedData(2, CacheLineDwords, &m_gangedCmdStreamSemAddr);
        PAL_ASSERT(m_gangedCmdStreamSemAddr != 0);

        // We need to memset this to handle a possible race condition with stale data.
        // If the memory contains any value, it is possible that, with the ACE running ahead, it could get a value
        // for this semaphore which is >= the number it is waiting for and then just continue ahead before GFX has
        // a chance to write it to 0.
        // To fix this, we use EmbeddedData and memset it on the CPU.
        // To handle the case where we reuse a command buffer entirely, we'll have to perform a GPU-side write of this
        // memory in the postamble.
        pData[0] = 0;
        pData[1] = 0;
    }

    return m_gangedCmdStreamSemAddr;
}

// =====================================================================================================================
// Allocates memory for the Streamout Data buffer if not already allocated.
gpusize UniversalCmdBuffer::SwStreamoutDataAddr()
{
    if (SupportsSwStrmout() && (m_swStreamoutDataAddr == 0))
    {
        // This buffer requires DWORD alignment.
        m_swStreamoutDataAddr = AllocateGpuScratchMem(sizeof(SwStreamoutBufLayout) / sizeof(uint32), 1);
        PAL_ASSERT(m_swStreamoutDataAddr != 0);

        // We need to initialize this buffer to all zeros to start.
        WriteDataInfo writeData = {};
        writeData.engineType    = m_engineType;
        writeData.dstAddr       = m_swStreamoutDataAddr;
        writeData.engineSel     = engine_sel__pfp_write_data__prefetch_parser;
        writeData.dstSel        = dst_sel__pfp_write_data__memory;

        constexpr SwStreamoutBufLayout DummyControlBuffer = {};
        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
        pDeCmdSpace        += m_cmdUtil.BuildWriteData(writeData,
                                                       sizeof(SwStreamoutBufLayout) / sizeof(uint32),
                                                       reinterpret_cast<const uint32*>(&DummyControlBuffer),
                                                       pDeCmdSpace);
        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }

    return m_swStreamoutDataAddr;
}

// =====================================================================================================================
// Returns the HW X and Y shading rate values that correspond to the supplied enumeration.
Offset2d UniversalCmdBuffer::GetHwShadingRate(
    VrsShadingRate  shadingRate)
{
    Offset2d  hwShadingRate = {};

    static constexpr Offset2d  HwShadingRateTable[] =
    {
        { -2, -2 }, // VrsShadingRate::_16xSsaa
        { -2, -1 }, // VrsShadingRate::_8xSsaa
        { -2,  0 }, // VrsShadingRate::_4xSsaa
        { -2,  1 }, // VrsShadingRate::_2xSsaa
        {  0,  0 }, // VrsShadingRate::_1x1
        {  0,  1 }, // VrsShadingRate::_1x2
        {  1,  0 }, // VrsShadingRate::_2x1
        {  1,  1 }, // VrsShadingRate::_2x2
    };

    // HW encoding is in 2's complement of the table values
    hwShadingRate.x = HwShadingRateTable[static_cast<uint32>(shadingRate)].x;
    hwShadingRate.y = HwShadingRateTable[static_cast<uint32>(shadingRate)].y;

    return hwShadingRate;
}

// =====================================================================================================================
// Returns the HW combiner value that corresponds to the supplied combinerMode
uint32 UniversalCmdBuffer::GetHwVrsCombinerState(
    VrsCombiner  combinerMode)
{
    constexpr VRSCombinerMode HwCombinerMode[] =
    {
        VRS_COMB_MODE_PASSTHRU,  // Passthrough
        VRS_COMB_MODE_OVERRIDE,  // Override
        VRS_COMB_MODE_MIN,       // Min
        VRS_COMB_MODE_MAX,       // Max
        VRS_COMB_MODE_SATURATE,  // Sum
    };

    return static_cast<uint32>(HwCombinerMode[static_cast<uint32>(combinerMode)]);
}

// =====================================================================================================================
// Returns the HW combiner value that corresponds to rateParams.combinerState[combinerStage]
uint32 UniversalCmdBuffer::GetHwVrsCombinerState(
    const VrsRateParams&  rateParams,
    VrsCombinerStage      combinerStage)
{
    return GetHwVrsCombinerState(rateParams.combinerState[static_cast<uint32>(combinerStage)]);
}

// =====================================================================================================================
void UniversalCmdBuffer::WritePerDrawVrsRate(
    const VrsRateParams&  rateParams)
{
    regGE_VRS_RATE geVrsRate = {};

    // GE_VRS_RATE has an enable bit located in VGT_DRAW__PAYLOAD_CNTL.EN_VRS_RATE.  That register is owned
    // by the pipeline, but the pipeline should be permanently enabling that bit.
    const Offset2d  hwShadingRate = GetHwShadingRate(rateParams.shadingRate);

    geVrsRate.bits.RATE_X = hwShadingRate.x;
    geVrsRate.bits.RATE_Y = hwShadingRate.y;

    regPA_CL_VRS_CNTL paClVrsCntl = {};

    paClVrsCntl.bits.VERTEX_RATE_COMBINER_MODE    = GetHwVrsCombinerState(rateParams,
                                                                          VrsCombinerStage::ProvokingVertex);
    paClVrsCntl.bits.PRIMITIVE_RATE_COMBINER_MODE = GetHwVrsCombinerState(rateParams, VrsCombinerStage::Primitive);
    paClVrsCntl.bits.HTILE_RATE_COMBINER_MODE     = GetHwVrsCombinerState(rateParams, VrsCombinerStage::Image);
    paClVrsCntl.bits.SAMPLE_ITER_COMBINER_MODE    = GetHwVrsCombinerState(rateParams,
                                                                          VrsCombinerStage::PsIterSamples);
    paClVrsCntl.bits.EXPOSE_VRS_PIXELS_MASK       = rateParams.flags.exposeVrsPixelsMask;

    // This field is related to exposing VRS info into cMask buffer as an output.  Not sure if any client is
    // going to require this functionality at this time, so leave this off.
    paClVrsCntl.bits.CMASK_RATE_HINT_FORCE_ZERO   = 0;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WriteSetOneConfigReg(Gfx103Plus::mmGE_VRS_RATE,
                                                     geVrsRate.u32All,
                                                     pDeCmdSpace);
    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(Gfx103Plus::mmPA_CL_VRS_CNTL,
                                                      paClVrsCntl.u32All,
                                                      pDeCmdSpace);

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Setup registers affected by the VrsRateParams struct
void UniversalCmdBuffer::CmdSetPerDrawVrsRate(
    const VrsRateParams&  rateParams)
{
    Pal::Pm4::UniversalCmdBuffer::CmdSetPerDrawVrsRate(rateParams);

    if (m_cachedSettings.supportsVrs)
    {
        // The optimizeNullSourceImage setting requires us to cross-validate the vrs rate with the vrs image.
        // So, when that optimization is enabled, this state is moved to draw time validation instead of write-thru.
        if (m_cachedSettings.optimizeNullSourceImage == false)
        {
            WritePerDrawVrsRate(rateParams);
        }

        if (IsGfx103Plus(m_gfxIpLevel))
        {
            // The VRS rate params own SAMPLE_COVERAGE_ENCODING
            m_paScAaConfigNew.gfx103Plus.SAMPLE_COVERAGE_ENCODING = rateParams.flags.exposeVrsPixelsMask;
        }
    }
}

// =====================================================================================================================
// Setup registers affected by the VrsCenterState struct
void UniversalCmdBuffer::CmdSetVrsCenterState(
    const VrsCenterState&  centerState)
{
    // Record the state so that we can restore it after RPM operations.
    Pal::Pm4::UniversalCmdBuffer::CmdSetVrsCenterState(centerState);

    if (m_cachedSettings.supportsVrs)
    {
        const Offset2d*  pOffset = &centerState.centerOffset[0];
        regDB_SPI_VRS_CENTER_LOCATION  dbSpiVrsCenterLocation;
        regSPI_BARYC_SSAA_CNTL         spiBarycSsaaCntl;

        dbSpiVrsCenterLocation.u32All                   = 0;
        dbSpiVrsCenterLocation.bits.CENTER_X_OFFSET_1X1 = pOffset[static_cast<uint32>(VrsCenterRates::_1x1)].x;
        dbSpiVrsCenterLocation.bits.CENTER_Y_OFFSET_1X1 = pOffset[static_cast<uint32>(VrsCenterRates::_1x1)].y;
        dbSpiVrsCenterLocation.bits.CENTER_X_OFFSET_2X1 = pOffset[static_cast<uint32>(VrsCenterRates::_2x1)].x;
        dbSpiVrsCenterLocation.bits.CENTER_Y_OFFSET_2X1 = pOffset[static_cast<uint32>(VrsCenterRates::_2x1)].y;
        dbSpiVrsCenterLocation.bits.CENTER_X_OFFSET_1X2 = pOffset[static_cast<uint32>(VrsCenterRates::_1x2)].x;
        dbSpiVrsCenterLocation.bits.CENTER_Y_OFFSET_1X2 = pOffset[static_cast<uint32>(VrsCenterRates::_1x2)].y;
        dbSpiVrsCenterLocation.bits.CENTER_X_OFFSET_2X2 = pOffset[static_cast<uint32>(VrsCenterRates::_2x2)].x;
        dbSpiVrsCenterLocation.bits.CENTER_Y_OFFSET_2X2 = pOffset[static_cast<uint32>(VrsCenterRates::_2x2)].y;

        spiBarycSsaaCntl.u32All                  = 0;
        spiBarycSsaaCntl.bits.CENTER_SSAA_MODE   = centerState.flags.overrideCenterSsaa;
        spiBarycSsaaCntl.bits.CENTROID_SSAA_MODE = centerState.flags.overrideCentroidSsaa;

        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(Gfx103Plus::mmDB_SPI_VRS_CENTER_LOCATION,
                                                          dbSpiVrsCenterLocation.u32All,
                                                          pDeCmdSpace);

        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(Gfx103Plus::mmSPI_BARYC_SSAA_CNTL,
                                                          spiBarycSsaaCntl.u32All,
                                                          pDeCmdSpace);

        if (IsGfx103Plus(m_gfxIpLevel))
        {
            // The VRS center state owns COVERED_CENTROID_IS_CENTER
            m_paScAaConfigNew.gfx103Plus.COVERED_CENTROID_IS_CENTER = (centerState.flags.alwaysComputeCentroid ? 0 : 1);
        }

        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }
}

// =====================================================================================================================
// This implementation probably doesn't have to do a whole lot other then record the sample-rate image in use...
// Draw time? will have the unhappy task of copying the shading-rate data in this image into the hTile buffer, or, if
// there isn't a bound hTile buffer, creating one.
void UniversalCmdBuffer::CmdBindSampleRateImage(
    const IImage*  pImage)
{
    // If a source image was provided, verify its creation parameters here
    if (pImage != nullptr)
    {
        const auto&  createInfo = pImage->GetImageCreateInfo();

        PAL_ASSERT(Formats::BitsPerPixel(createInfo.swizzledFormat.format) == 8);
        PAL_ASSERT(createInfo.mipLevels == 1);
        PAL_ASSERT(createInfo.arraySize == 1);
        PAL_ASSERT(createInfo.samples   == 1);
        PAL_ASSERT(createInfo.imageType == ImageType::Tex2d);
    }

    // The optimizeNullSourceImage setings requires us to re-validate the vrs rate params when transitioning
    // between a null and non-null vrsImage
    m_graphicsState.dirtyFlags.vrsRateParams |=
        (m_cachedSettings.optimizeNullSourceImage) &&
        ((pImage == nullptr) != (m_graphicsState.pVrsImage == nullptr));

    // Independent layer records the source image and marks our command buffer state as dirty.
    Pal::Pm4::UniversalCmdBuffer::CmdBindSampleRateImage(pImage);

    // Nothing else to do here; we don't know which depth buffer is going to be bound for the upcoming draw
    // yet, so we don't have a destination for the source image data (yet).

    if (IsGfx11(m_gfxIpLevel))
    {
        const Pal::Image* pSrcImage = static_cast<const Pal::Image*>(pImage);

        struct
        {
            regPA_SC_VRS_RATE_BASE     paScVrsRateBase;
            regPA_SC_VRS_RATE_BASE_EXT paScVrsRateBaseExt;
            regPA_SC_VRS_RATE_SIZE_XY  paScVrsRateSize;
        } regs = { };

        if (pSrcImage != nullptr)
        {
            const auto&    createInfo      = pSrcImage->GetImageCreateInfo();
            const Image*   pGfxImage       = static_cast<Image*>(pSrcImage->GetGfxImage());
            const SubresId baseSubResId    = {};
            const auto*    pBaseSubResInfo = pSrcImage->SubresourceInfo(baseSubResId);
            const auto&    addrSettings    = pGfxImage->GetAddrSettings(pBaseSubResInfo);

            //   The VRS surface is to follow standard addressing (either SW_Z or SW_R)
            PAL_ASSERT(AddrMgr2::IsRotatedSwizzle(addrSettings.swizzleMode) ||
                       AddrMgr2::IsZSwizzle(addrSettings.swizzleMode));

            const gpusize subresource256BAddr      = pGfxImage->GetSubresource256BAddr(baseSubResId);
            regs.paScVrsRateBase.bits.BASE_256B    = LowPart(subresource256BAddr);
            regs.paScVrsRateBaseExt.bits.BASE_256B = HighPart(subresource256BAddr);

            // The units here are in terms of 8x8 tiles, but the rate image is already in terms of 8x8 tiles, so use
            // the size info directly.
            regs.paScVrsRateSize.bits.X_MAX = createInfo.extent.width  - 1;
            regs.paScVrsRateSize.bits.Y_MAX = createInfo.extent.height - 1;
        }

        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
        // PA_SC_VRS_RATE_BASE, PA_SC_VRS_RATE_BASE_EXT and PA_SC_VRS_RATE_SIZE_XY are sequential, so we can write
        // these three registers using a single packet.
        pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(Gfx11::mmPA_SC_VRS_RATE_BASE,
                                                           Gfx11::mmPA_SC_VRS_RATE_SIZE_XY,
                                                           &regs,
                                                           pDeCmdSpace);

        regPA_SC_VRS_OVERRIDE_CNTL  paScVrsOverrideCntl= {};

        paScVrsOverrideCntl.u32All = 0;
        paScVrsOverrideCntl.bits.VRS_OVERRIDE_RATE_COMBINER_MODE = SC_VRS_COMB_MODE_PASSTHRU;
        paScVrsOverrideCntl.bits.VRS_RATE                        = VRS_SHADING_RATE_1X1;

        // This field also exists in the PA_CL_VRS_CNTL register; that version is deprecated as
        // per SW request.
        paScVrsOverrideCntl.bits.VRS_SURFACE_ENABLE = (pSrcImage != nullptr);

        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(Gfx11::mmPA_SC_VRS_OVERRIDE_CNTL,
                                                          paScVrsOverrideCntl.u32All,
                                                          pDeCmdSpace);

        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }
}

// =====================================================================================================================
// If we've copied VRS rate data from pRateImage into pDsView's subresource range and it hasn't been invalidated by
// a copy, metadata init, etc., we can skip the VRS copy operation for this draw.
bool UniversalCmdBuffer::IsVrsCopyRedundant(
    const Gfx10DepthStencilView* pDsView,
    const Pal::Image*            pRateImage)
{
    bool isRedundant = false;

    const Pal::Image* pViewImage    = pDsView->GetImage()->Parent();
    const uint32      viewMipLevel  = pDsView->MipLevel();
    const uint32      viewBaseSlice = pDsView->BaseArraySlice();
    const uint32      viewEndSlice  = viewBaseSlice + pDsView->ArraySize() - 1;

    // For simplicity's sake, we search for a single copy mapping that contains the whole view range. This could
    // be further optimized to OR together ranges across multiple mappings if it becomes a bottleneck.
    for (uint32 idx = 0; idx < m_validVrsCopies.NumElements(); ++idx)
    {
        const VrsCopyMapping& mapping = m_validVrsCopies.At(idx);

        if ((mapping.pRateImage  == pRateImage)    &&
            (mapping.pDepthImage == pViewImage)    &&
            (mapping.mipLevel    == viewMipLevel)  &&
            (mapping.baseSlice   <= viewBaseSlice) &&
            (mapping.endSlice    >= viewEndSlice))
        {
            isRedundant = true;
            break;
        }
    }

    return isRedundant;
}

// =====================================================================================================================
// Adds a new VrsCopyMapping to our list of prior VRS rate data copies.
void UniversalCmdBuffer::AddVrsCopyMapping(
    const Gfx10DepthStencilView* pDsView,
    const Pal::Image*            pRateImage)
{
    VrsCopyMapping newMapping = {};
    newMapping.pRateImage  = pRateImage;
    newMapping.pDepthImage = pDsView->GetImage()->Parent();
    newMapping.mipLevel    = pDsView->MipLevel();
    newMapping.baseSlice   = pDsView->BaseArraySlice();
    newMapping.endSlice    = newMapping.baseSlice + pDsView->ArraySize() - 1;

    // Walk the copy list to:
    // 1. Try to find an empty mapping in the vector that we can reuse.
    // 2. Mark prior copies that overlap with our new copy as invalid.
    //
    // We don't try to merge contiguous slice ranges and nor split ranges when overlap is detected. We could optimize
    // these cases in the future if they become a bottleneck.
    bool searching = true;

    for (uint32 idx = 0; idx < m_validVrsCopies.NumElements(); ++idx)
    {
        VrsCopyMapping*const pMapping = &m_validVrsCopies.At(idx);

        // By convention, setting the rate image pointer to null marks a mapping as invalid.
        if ((pMapping->pRateImage  != nullptr)                &&
            (pMapping->pDepthImage == newMapping.pDepthImage) &&
            (pMapping->mipLevel    == newMapping.mipLevel)    &&
            (pMapping->baseSlice   <= newMapping.endSlice)    &&
            (pMapping->endSlice    >= newMapping.baseSlice))
        {
            // If we have an existing mapping that wrote to the same view and overlaps in at least one subresource
            // we must mark that prior copy invalid or we could fail to recopy to the overlapped subresources.
            pMapping->pRateImage = nullptr;
        }

        if (searching && (pMapping->pRateImage == nullptr))
        {
            // Write our new copy into the first invalid mapping. This might be a mapping we just invalidated above.
            *pMapping = newMapping;
            searching = false;
        }
    }

    // Otherwise we need to extend the vector.
    if (searching)
    {
        const Result result = m_validVrsCopies.PushBack(newMapping);

        // This function should only be called during command recording so we can't return a Result to the client.
        // Instead we should update our command recording status so it can be returned to the caller later on.
        if (result != Result::Success)
        {
            SetCmdRecordingError(result);
        }
    }
}

// =====================================================================================================================
// Erase any mappings that reference the dirty rate image.
void UniversalCmdBuffer::EraseVrsCopiesFromRateImage(
    const Pal::Image* pRateImage)
{
    for (uint32 idx = 0; idx < m_validVrsCopies.NumElements(); ++idx)
    {
        VrsCopyMapping*const pMapping = &m_validVrsCopies.At(idx);

        if (pMapping->pRateImage == pRateImage)
        {
            // By convention, setting the rate image pointer to null marks a mapping as invalid.
            pMapping->pRateImage = nullptr;
        }
    }
}

// =====================================================================================================================
// Erase any mappings that reference the depth image. We could optimize this if this function also took a subresource
// range but that adds a fair bit complexity that probably won't be worth it. We only expect this function to be called
// if the VRS stencil write HW bug is triggered.
void UniversalCmdBuffer::EraseVrsCopiesToDepthImage(
    const Pal::Image* pDepthImage)
{
    for (uint32 idx = 0; idx < m_validVrsCopies.NumElements(); ++idx)
    {
        VrsCopyMapping*const pMapping = &m_validVrsCopies.At(idx);

        if (pMapping->pDepthImage == pDepthImage)
        {
            // By convention, setting the rate image pointer to null marks a mapping as invalid.
            pMapping->pRateImage = nullptr;
        }
    }
}

// =====================================================================================================================
bool UniversalCmdBuffer::IsPreemptable() const
{
    return m_deCmdStream.IsPreemptionEnabled();
}

// =====================================================================================================================
uint32* UniversalCmdBuffer::WriteWaitEop(
    HwPipePoint waitPoint,
    bool        waitCpDma,
    uint32      hwGlxSync,
    uint32      hwRbSync,
    uint32*     pCmdSpace)
{
    SyncGlxFlags glxSync = SyncGlxFlags(hwGlxSync);
    SyncRbFlags  rbSync  = SyncRbFlags(hwRbSync);

    bool waitAtPfpOrMe = true;

    if (m_device.Parent()->UsePws(EngineTypeUniversal))
    {
        // We should always prefer a PWS sync over a wait for EOP timestamp because it avoids all TS memory accesses.
        // It can also push the wait point further down the graphics pipeline in some cases.
        pCmdSpace += m_cmdUtil.BuildWaitEopPws(waitPoint, waitCpDma, glxSync, rbSync, pCmdSpace);

        waitAtPfpOrMe = (waitPoint == HwPipeTop) || (waitPoint == HwPipePostPrefetch);
    }
    else
    {
        // Issue explicit waitCpDma packet if ReleaseMem doesn't support it.
        if (waitCpDma && (m_device.Settings().gfx11EnableReleaseMemWaitCpDma == false))
        {
            pCmdSpace += m_cmdUtil.BuildWaitDmaData(pCmdSpace);
            waitCpDma = false;
        }

        // We prefer to do our GCR in the release_mem if we can. This function always does an EOP wait so we don't have
        // to worry about release_mem not supporting GCRs with EOS events. Any remaining sync flags must be handled in a
        // trailing acquire_mem packet.
        ReleaseMemGfx releaseInfo = {};
        releaseInfo.vgtEvent       = m_cmdUtil.SelectEopEvent(rbSync);
        releaseInfo.cacheSync      = m_cmdUtil.SelectReleaseMemCaches(&glxSync);
        releaseInfo.dstAddr        = AcqRelFenceValGpuVa(AcqRelEventType::Eop);
        releaseInfo.dataSel        = data_sel__me_release_mem__send_32_bit_low;
        releaseInfo.data           = GetNextAcqRelFenceVal(AcqRelEventType::Eop);
        releaseInfo.gfx11WaitCpDma = waitCpDma;

        pCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseInfo, pCmdSpace);
        pCmdSpace += m_cmdUtil.BuildWaitRegMem(EngineTypeUniversal,
                                               mem_space__me_wait_reg_mem__memory_space,
                                               function__me_wait_reg_mem__equal_to_the_reference_value,
                                               engine_sel__me_wait_reg_mem__micro_engine,
                                               releaseInfo.dstAddr,
                                               releaseInfo.data,
                                               UINT32_MAX,
                                               pCmdSpace);

        // If we still have some caches to sync we require a final acquire_mem. It doesn't do any waiting, it just
        // immediately does some full-range cache flush and invalidates. The previous WRM packet is the real wait.
        if (glxSync != SyncGlxNone)
        {
            AcquireMemGfxSurfSync acquireInfo = {};
            acquireInfo.cacheSync = glxSync;

            pCmdSpace += m_cmdUtil.BuildAcquireMemGfxSurfSync(acquireInfo, pCmdSpace);
        }

        if (waitPoint == HwPipeTop)
        {
            pCmdSpace += m_cmdUtil.BuildPfpSyncMe(pCmdSpace);
        }
    }

    if (waitAtPfpOrMe)
    {
        SetGfxBltState(false);
        SetCsBltState(false);

        if (rbSync == SyncRbWbInv)
        {
            SetGfxBltWriteCacheState(false);
        }

        // The previous EOP event and wait mean that anything prior to this point, including previous command
        // buffers on this queue, have completed.
        SetPrevCmdBufInactive();
    }

    if (TestAllFlagsSet(glxSync, SyncGl2WbInv))
    {
        ClearBltWriteMisalignMdState();
    }

    return pCmdSpace;
}

// =====================================================================================================================
uint32* UniversalCmdBuffer::WriteWaitCsIdle(
    uint32* pCmdSpace)
{
    pCmdSpace += m_cmdUtil.BuildWaitCsIdle(GetEngineType(), TimestampGpuVirtAddr(), pCmdSpace);

    SetCsBltState(false);

    return pCmdSpace;
}

// =====================================================================================================================
bool UniversalCmdBuffer::UpdateNggPrimCb(
    const GraphicsPipeline*         pCurrentPipeline,
    Util::Abi::PrimShaderCullingCb* pPrimShaderCb
    ) const
{
    bool dirty = false;

    if ((pPrimShaderCb->paClVteCntl  != pCurrentPipeline->PaClVteCntl().u32All) ||
        (pPrimShaderCb->paSuVtxCntl  != pCurrentPipeline->PaSuVtxCntl().u32All) ||
        (pPrimShaderCb->paClClipCntl != m_paClClipCntl.u32All))
    {
        dirty = true;
        pPrimShaderCb->paClVteCntl  = pCurrentPipeline->PaClVteCntl().u32All;
        pPrimShaderCb->paSuVtxCntl  = pCurrentPipeline->PaSuVtxCntl().u32All;
        pPrimShaderCb->paClClipCntl = m_paClClipCntl.u32All;
    }

    return dirty;
}

} // Gfx9
} // Pal
