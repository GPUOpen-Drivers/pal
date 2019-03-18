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

#include "core/hw/gfxip/gfx6/gfx6BorderColorPalette.h"
#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6ColorBlendState.h"
#include "core/hw/gfxip/gfx6/gfx6ColorTargetView.h"
#include "core/hw/gfxip/gfx6/gfx6ComputePipeline.h"
#include "core/hw/gfxip/gfx6/gfx6DepthStencilState.h"
#include "core/hw/gfxip/gfx6/gfx6DepthStencilView.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6GraphicsPipeline.h"
#include "core/hw/gfxip/gfx6/gfx6Image.h"
#include "core/hw/gfxip/gfx6/gfx6IndirectCmdGenerator.h"
#include "core/hw/gfxip/gfx6/gfx6MsaaState.h"
#include "core/hw/gfxip/gfx6/gfx6PerfExperiment.h"
#include "core/hw/gfxip/gfx6/gfx6PerfTrace.h"
#include "core/hw/gfxip/gfx6/gfx6UniversalCmdBuffer.h"
#include "core/hw/gfxip/queryPool.h"
#include "core/cmdAllocator.h"
#include "core/g_palPlatformSettings.h"
#include "palMath.h"
#include "palIntervalTreeImpl.h"
#include "palVectorImpl.h"

#include <float.h>
#include <limits.h>
#include <type_traits>

using namespace Util;
using std::is_same;

namespace Pal
{
namespace Gfx6
{

// Lookup table for converting between IndexType and VGT_INDEX_TYPE enums.
constexpr uint32 VgtIndexTypeLookup[] =
{
    VGT_INDEX_8__VI,    // IndexType::Idx8
    VGT_INDEX_16,       // IndexType::Idx16
    VGT_INDEX_32        // IndexType::Idx32
};

// Uint32 versions of the enumeration for hardware stage ID.
constexpr uint32 LsStageId = static_cast<uint32>(HwShaderStage::Ls);
constexpr uint32 HsStageId = static_cast<uint32>(HwShaderStage::Hs);
constexpr uint32 EsStageId = static_cast<uint32>(HwShaderStage::Es);
constexpr uint32 GsStageId = static_cast<uint32>(HwShaderStage::Gs);
constexpr uint32 VsStageId = static_cast<uint32>(HwShaderStage::Vs);
constexpr uint32 PsStageId = static_cast<uint32>(HwShaderStage::Ps);

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

    uint32 numRecords = ((UINT_MAX - chipProps.gfx6.wavefrontSize) + 1);
    if ((chipProps.gfxLevel >= GfxIpLevel::GfxIp8) && (strideInBytes > 0))
    {
        PAL_ANALYSIS_ASSUME(strideInBytes != 0);
        // On GFX8.x, NUM_RECORDS is in bytes, so we need to take the vertex stride into account when computing
        // the stream-out clamp value expected by SC.
        numRecords = (strideInBytes * (((UINT_MAX / strideInBytes) - chipProps.gfx6.wavefrontSize) + 1));
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
    m_vertexOffsetReg(UserDataNotMapped),
    m_drawIndexReg(UserDataNotMapped),
    m_workaroundState(&device, IsNested()),
    m_activeOcclusionQueryWriteRanges(m_device.GetPlatform())
{
    const PalPlatformSettings& platformSettings = m_device.Parent()->GetPlatform()->PlatformSettings();
    const PalSettings&         coreSettings    = m_device.CoreSettings();
    const Gfx6PalSettings&     settings        = m_device.Settings();
    const auto*const           pPublicSettings = m_device.Parent()->GetPublicSettings();

    memset(&m_vbTable,         0, sizeof(m_vbTable));
    memset(&m_spillTable,      0, sizeof(m_spillTable));
    memset(&m_streamOut,       0, sizeof(m_streamOut));
    memset(&m_state,           0, sizeof(m_state));
    memset(&m_drawTimeHwState, 0, sizeof(m_drawTimeHwState));
    memset(&m_cachedSettings,  0, sizeof(m_cachedSettings));
    memset(&m_primGroupOpt,    0, sizeof(m_primGroupOpt));

    m_vgtDmaIndexType.u32All = 0;

    // Setup default engine support - Universal Cmd Buffer supports Graphics, Compute and CPDMA.
    m_engineSupport = (CmdBufferEngineSupport::Graphics |
                       CmdBufferEngineSupport::Compute  |
                       CmdBufferEngineSupport::CpDma);

    // Setup all of our cached settings checks.
    m_cachedSettings.tossPointMode              = static_cast<uint32>(coreSettings.tossPointMode);
    m_cachedSettings.hiDepthDisabled            = !settings.hiDepthEnable;
    m_cachedSettings.hiStencilDisabled          = !settings.hiStencilEnable;
    m_cachedSettings.ignoreCsBorderColorPalette = settings.disableBorderColorPaletteBinds;
    m_cachedSettings.blendOptimizationsEnable   = settings.blendOptimizationsEnable;
    m_cachedSettings.outOfOrderPrimsEnable      = static_cast<uint32>(settings.gfx7EnableOutOfOrderPrimitives);
    m_cachedSettings.padParamCacheSpace         =
            ((pPublicSettings->contextRollOptimizationFlags & PadParamCacheSpace) != 0);
    m_cachedSettings.gfx7AvoidNullPrims         = settings.gfx7AvoidVgtNullPrims;

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

    const bool sqttEnabled = (platformSettings.gpuProfilerMode > GpuProfilerCounterAndTimingOnly) &&
                             (Util::TestAnyFlagSet(platformSettings.gpuProfilerConfig.traceModeMask, GpuProfilerTraceSqtt));

    m_cachedSettings.issueSqttMarkerEvent = (sqttEnabled ||
                                            m_device.Parent()->GetPlatform()->IsDevDriverProfilingEnabled());

    memset(&m_rbPlusPm4Img, 0, sizeof(m_rbPlusPm4Img));
    if (m_device.Parent()->ChipProperties().gfx6.rbPlus != 0)
    {
        m_rbPlusPm4Img.spaceNeeded = m_device.CmdUtil().BuildSetSeqContextRegs(mmSX_PS_DOWNCONVERT__VI,
                                                                               mmSX_BLEND_OPT_CONTROL__VI,
                                                                               &m_rbPlusPm4Img.header);
    }

    SwitchDrawFunctions(false);
}

// =====================================================================================================================
// Initializes Gfx6-specific functionality.
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
template <bool TessEnabled, bool GsEnabled>
void UniversalCmdBuffer::SetUserDataValidationFunctions()
{
    if (UseCpuPathInsteadOfCeRam())
    {
        m_pfnValidateUserDataGfx =
            &UniversalCmdBuffer::ValidateGraphicsUserDataCpu<false, TessEnabled, GsEnabled>;
        m_pfnValidateUserDataGfxPipelineSwitch =
            &UniversalCmdBuffer::ValidateGraphicsUserDataCpu<true, TessEnabled, GsEnabled>;
    }
    else
    {
        m_pfnValidateUserDataGfx =
            &UniversalCmdBuffer::ValidateGraphicsUserDataCeRam<false, TessEnabled, GsEnabled>;
        m_pfnValidateUserDataGfxPipelineSwitch =
            &UniversalCmdBuffer::ValidateGraphicsUserDataCeRam<true, TessEnabled, GsEnabled>;
    }
}

// =====================================================================================================================
// Sets up function pointers for Draw-time validation of graphics user-data entries.
void UniversalCmdBuffer::SetUserDataValidationFunctions(
    bool tessEnabled,
    bool gsEnabled)
{
    if (tessEnabled)
    {
        if (gsEnabled)
        {
            SetUserDataValidationFunctions<true, true>();
        }
        else
        {
            SetUserDataValidationFunctions<true, false>();
        }
    }
    else
    {
        if (gsEnabled)
        {
            SetUserDataValidationFunctions<false, true>();
        }
        else
        {
            SetUserDataValidationFunctions<false, false>();
        }
    }
}

// =====================================================================================================================
// Resets all of the state tracked by this command buffer
void UniversalCmdBuffer::ResetState()
{
    const auto& chipProps = m_device.Parent()->ChipProperties();

    Pal::UniversalCmdBuffer::ResetState();

    if (m_cachedSettings.issueSqttMarkerEvent)
    {
        SetDispatchFunctions<true>();
    }
    else
    {
        SetDispatchFunctions<false>();
    }

    SetUserDataValidationFunctions(false, false);

    m_vgtDmaIndexType.u32All = 0;
    m_vgtDmaIndexType.bits.SWAP_MODE = VGT_DMA_SWAP_NONE;

    if (chipProps.gfxLevel >= GfxIpLevel::GfxIp8)
    {
        // NOTE: On Gfx8 and newer hardware, the VGT_DMA_INDEX_TYPE register has an extra field for specifying the
        // MTYPE of memory accesses to the index buffer. Other than the new field, the register is identical to the
        // SI/CI version.
        m_vgtDmaIndexType.bits.MTYPE        = MTYPE_UC;
        m_vgtDmaIndexType.bits.RDREQ_POLICY = VGT_POLICY_STREAM;
    }
    else if (chipProps.gfxLevel == GfxIpLevel::GfxIp7)
    {
        m_vgtDmaIndexType.bits.RDREQ_POLICY = VGT_POLICY_STREAM;
    }

    m_spiVsOutConfig.u32All = 0;
    m_spiPsInControl.u32All = 0;

    // Reset the command buffer's HWL state tracking
    m_state.flags.u32All   = 0;
    m_state.minCounterDiff = UINT_MAX;

    // Set to an invalid (unaligned) address to indicate that streamout hasn't been set yet, and initialize the SRDs'
    // NUM_RECORDS fields to indicate a zero stream-out stride.
    memset(&m_streamOut.srd[0], 0, sizeof(m_streamOut.srd));
    m_streamOut.srd[0].word0.bits.BASE_ADDRESS = 1;
    for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
    {
        m_streamOut.srd[i].word2.bits.NUM_RECORDS = StreamOutNumRecords(chipProps, 0);
    }

    ResetUserDataTable(&m_streamOut.state);

    // Reset the workaround state object.
    m_workaroundState.Reset();

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

    if (chipProps.gfxLevel != GfxIpLevel::GfxIp6)
    {
        m_drawTimeHwState.dbCountControl.bits.ZPASS_ENABLE__CI__VI      = 1;
        m_drawTimeHwState.dbCountControl.bits.SLICE_EVEN_ENABLE__CI__VI = 1;
        m_drawTimeHwState.dbCountControl.bits.SLICE_ODD_ENABLE__CI__VI  = 1;
    }

    m_vertexOffsetReg = UserDataNotMapped;
    m_drawIndexReg    = UserDataNotMapped;

    m_pSignatureCs       = &NullCsSignature;
    m_pSignatureGfx      = &NullGfxSignature;
    m_pipelineCtxPm4Hash = 0;

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

        const bool tessEnabled = (pNewPipeline != nullptr) && pNewPipeline->IsTessEnabled();
        const bool gsEnabled   = (pNewPipeline != nullptr) && pNewPipeline->IsGsEnabled();

        SetUserDataValidationFunctions(tessEnabled, gsEnabled);

        const bool newUsesViewInstancing = (pNewPipeline != nullptr) && pNewPipeline->UsesViewInstancing();
        const bool oldUsesViewInstancing = (pOldPipeline != nullptr) && pOldPipeline->UsesViewInstancing();

        // NGG Fast Launch pipelines require issuing different packets for indexed draws. We'll need to switch the
        // draw function pointers around to handle this case.
        if (oldUsesViewInstancing != newUsesViewInstancing)
        {
            SwitchDrawFunctions(newUsesViewInstancing);
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
        m_drawTimeHwState.dirty.indexBufferBase = 1;
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
// Updates the graphics state with a new pipeline and performs any extra work due to the pipeline switch.
uint32* UniversalCmdBuffer::SwitchGraphicsPipeline(
    const GraphicsPipelineSignature* pPrevSignature,
    const GraphicsPipeline*          pCurrPipeline,
    uint32*                          pDeCmdSpace)
{
    PAL_ASSERT(pCurrPipeline != nullptr);

    const bool wasPrevPipelineNull = (pPrevSignature == &NullGfxSignature);

    const uint64 ctxPm4Hash = pCurrPipeline->GetContextPm4ImgHash();
    if (wasPrevPipelineNull || (m_pipelineCtxPm4Hash != ctxPm4Hash))
    {
        pDeCmdSpace = pCurrPipeline->WriteContextCommands(&m_deCmdStream, pDeCmdSpace);

        m_pipelineCtxPm4Hash = ctxPm4Hash;
    }

    if (m_rbPlusPm4Img.spaceNeeded != 0)
    {
        pDeCmdSpace = m_deCmdStream.WritePm4Image(m_rbPlusPm4Img.spaceNeeded, &m_rbPlusPm4Img, pDeCmdSpace);
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

    const bool usesViewportArrayIdx = pCurrPipeline->UsesViewportArrayIndex();
    if (usesViewportArrayIdx != (m_graphicsState.enableMultiViewport != 0))
    {
        // If the previously bound pipeline differed in its use of multiple viewports we will need to rewrite the
        // viewport and scissor state on draw.
        if (m_graphicsState.viewportState.count != 0)
        {
            // If viewport is never set, no need to rewrite viewport, this happens in D3D12 nested command list.
            m_graphicsState.dirtyFlags.validationBits.viewports    = 1;
        }
        if (m_graphicsState.scissorRectState.count != 0)
        {
            // If scissor is never set, no need to rewrite scissor, this happens in D3D12 nested command list.
            m_graphicsState.dirtyFlags.validationBits.scissorRects = 1;
        }

        m_graphicsState.enableMultiViewport    = usesViewportArrayIdx;
        m_graphicsState.everUsedMultiViewport |= usesViewportArrayIdx;
    }

    if (m_vertexOffsetReg != m_pSignatureGfx->vertexOffsetRegAddr)
    {
        m_vertexOffsetReg = m_pSignatureGfx->vertexOffsetRegAddr;

        // If the vsUserRegBase setting is changing we must invalidate the instance offset and vertex offset state
        // so that the appropriate user data registers are updated.
        m_drawTimeHwState.valid.instanceOffset = 0;
        m_drawTimeHwState.valid.vertexOffset   = 0;
    }

    if (m_drawIndexReg != m_pSignatureGfx->drawIndexRegAddr)
    {
        m_drawIndexReg = m_pSignatureGfx->drawIndexRegAddr;
        if (m_drawIndexReg != UserDataNotMapped)
        {
            m_drawTimeHwState.valid.drawIndex = 0;
        }
    }

    if (m_primGroupOpt.windowSize != 0)
    {
        // Reset the primgroup window state so that we can start gathering data on this new pipeline.
        // Note that we will only enable this optimization for VS/PS pipelines.
        m_primGroupOpt.vtxIdxTotal = 0;
        m_primGroupOpt.drawCount   = 0;
        m_primGroupOpt.optimalSize = 0;
        m_primGroupOpt.enabled = ((pCurrPipeline->IsGsEnabled() == false)   &&
                                  (pCurrPipeline->IsTessEnabled() == false) &&
                                  (pCurrPipeline->UsesStreamOut() == false));
    }

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
    pDeCmdSpace = m_deCmdStream.WritePm4Image(samplePosPm4Image.spaceNeeded, &samplePosPm4Image, pDeCmdSpace);
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
    pImage->red.f32All   = params.blendConst[0];
    pImage->green.f32All = params.blendConst[1];
    pImage->blue.f32All  = params.blendConst[2];
    pImage->alpha.f32All = params.blendConst[3];

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

    const bool   isGfx7plus = device.Parent()->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp7;
    const uint32 regAddr = isGfx7plus ? mmVGT_PRIMITIVE_TYPE__CI__VI : mmVGT_PRIMITIVE_TYPE__SI;

    InputAssemblyStatePm4Img* pImage = reinterpret_cast<InputAssemblyStatePm4Img*>(pCmdSpace);

    // Initialise PM4 image headers
    size_t totalDwords = 0;
    totalDwords += device.CmdUtil().BuildSetOneConfigReg(regAddr, &pImage->hdrPrimType, SET_UCONFIG_INDEX_PRIM_TYPE);
    totalDwords += device.CmdUtil().BuildSetOneContextReg(mmVGT_MULTI_PRIM_IB_RESET_EN,
                                                          &pImage->hdrVgtMultiPrimIbResetEnable);
    totalDwords += device.CmdUtil().BuildSetOneContextReg(mmVGT_MULTI_PRIM_IB_RESET_INDX,
                                                          &pImage->hdrVgtMultiPrimIbResetIndex);

    // Initialise register data
    pImage->primType.u32All = 0;
    pImage->primType.bits.PRIM_TYPE = TopologyToPrimTypeTbl[static_cast<uint32>(params.topology)];

    pImage->vgtMultiPrimIbResetEnable.u32All = 0;
    pImage->vgtMultiPrimIbResetEnable.bits.RESET_EN  = params.primitiveRestartEnable;

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
    const size_t pm4ImgSize       = pPm4ImgEnd - pm4Image;

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
    const uint32 packetPredicate = PacketPredicate();
    m_gfxCmdBufState.packetPredicate = 0;

    m_device.Barrier(this, &m_deCmdStream, barrierInfo);

    m_gfxCmdBufState.packetPredicate = packetPredicate;
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

    uint32*const pDst = (reinterpret_cast<uint32*>(m_vbTable.pSrds) + dwordOffset);
    memcpy(pDst, pSrcData, dwordSize * sizeof(uint32));

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
    constexpr uint32 AllColorTargetSlotMask = 255; // Mask of all color target slots.

    // Bind all color targets.
    uint32 newColorTargetMask = 0;
    for (uint32 slot = 0; slot < params.colorTargetCount; slot++)
    {
        const auto*const pNewView = static_cast<const ColorTargetView*>(params.colorTargets[slot].pColorTargetView);

        if (pNewView != nullptr)
        {
            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
            pDeCmdSpace = pNewView->WriteCommands(slot,
                                                  params.colorTargets[slot].imageLayout,
                                                  &m_deCmdStream,
                                                  pDeCmdSpace);
            m_deCmdStream.CommitCommands(pDeCmdSpace);

            if (m_device.WaMiscDccOverwriteComb())
            {
                m_workaroundState.ClearDccOverwriteCombinerDisable(slot);
            }

            // Each set bit means the corresponding color target slot is being bound to a valid target.
            newColorTargetMask |= (1 << slot);
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

    const auto*const pOldDepthView =
            static_cast<const DepthStencilView*>(m_graphicsState.bindTargets.depthTarget.pDepthStencilView);

    const auto*const pNewDepthView = static_cast<const DepthStencilView*>(params.depthTarget.pDepthStencilView);

    // Apply the TC compatible flush workaround.
    // This must be done before any DB register writes or hangs might occur.
    pDeCmdSpace = DepthStencilView::WriteTcCompatFlush(m_device, pNewDepthView, pOldDepthView, pDeCmdSpace);

    // Bind the depth target or NULL if it was not provided.
    if (pNewDepthView != nullptr)
    {
        pDeCmdSpace = pNewDepthView->WriteCommands(params.depthTarget.depthLayout,
                                                   params.depthTarget.stencilLayout,
                                                   &m_deCmdStream,
                                                   pDeCmdSpace);

        // Re-write the ZRANGE_PRECISION value for the waTcCompatZRange workaround. We must include the COND_EXEC which
        // checks the metadata because we don't know the last fast clear value here.
        pDeCmdSpace = pNewDepthView->UpdateZRangePrecision(true, &m_deCmdStream, pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = WriteNullDepthTarget(pDeCmdSpace);
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);

    // Save updated bindTargets state
    //  For consistancy ensure we only save colorTargets within the valid target count specified, and set
    //  unbound target slots as empty/null.  This allows simple slot change comparisons above and elsewhere.
    //  Handle cases where callers may supply input like:
    //     colorTargetCount=4 {view, null, null,null} --> colorTargetCount=1 {view,null,...}
    //     colorTargetCount=0 {view1,view2,null,null} --> colorTargetCount=0 {null,null,...}
    uint32 colorTargetLimit = Max(params.colorTargetCount, m_graphicsState.bindTargets.colorTargetCount);
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
            bufferSize = LowPart(params.target[idx].size) / sizeof(uint32);
            PAL_ASSERT(HighPart(params.target[idx].size) == 0);

            const uint32 strideInBytes =
                ((pPipeline == nullptr) ? 0 : pPipeline->VgtStrmoutVtxStride(idx).u32All) * sizeof(uint32);

            m_streamOut.srd[idx].word0.bits.BASE_ADDRESS    = LowPart(params.target[idx].gpuVirtAddr);
            m_streamOut.srd[idx].word1.bits.BASE_ADDRESS_HI = HighPart(params.target[idx].gpuVirtAddr);
            m_streamOut.srd[idx].word1.bits.STRIDE          = strideInBytes;
            m_streamOut.srd[idx].word2.bits.NUM_RECORDS     = StreamOutNumRecords(chipProps, strideInBytes);
            m_streamOut.srd[idx].word3.bits.DST_SEL_X       = SQ_SEL_X;
            m_streamOut.srd[idx].word3.bits.DST_SEL_Y       = SQ_SEL_Y;
            m_streamOut.srd[idx].word3.bits.DST_SEL_Z       = SQ_SEL_Z;
            m_streamOut.srd[idx].word3.bits.DST_SEL_W       = SQ_SEL_W;
            m_streamOut.srd[idx].word3.bits.TYPE            = SQ_RSRC_BUF;
            m_streamOut.srd[idx].word3.bits.ADD_TID_ENABLE  = 1;
            m_streamOut.srd[idx].word3.bits.DATA_FORMAT     = BUF_DATA_FORMAT_32;
            m_streamOut.srd[idx].word3.bits.NUM_FORMAT      = BUF_NUM_FORMAT_UINT;
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

    regPA_SU_SC_MODE_CNTL paSuScModeCntl = BuildPaSuScModeCntl(*pParams);

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
// Builds the packets for setting the triangle raster state in the command space provided.
uint32* UniversalCmdBuffer::BuildSetTriangleRasterState(
    const TriangleRasterStateParams& params,
    const CmdUtil&                   cmdUtil,
    uint32*                          pCmdSpace)
{
    TriangleRasterStateReg* pImage      = reinterpret_cast<TriangleRasterStateReg*>(pCmdSpace);
    const size_t            totalDwords = cmdUtil.BuildSetOneContextReg(mmPA_SU_SC_MODE_CNTL, &pImage->header);

    pImage->paSuScModeCntl = BuildPaSuScModeCntl(params);

    return pCmdSpace + totalDwords;
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
    const auto* pPipeline  = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const uint32 userData0 = pPipeline->GetVsUserDataBaseOffset();

    // Compute register offsets of first vertex and start instance user data locations relative to
    // user data 0.
    PAL_ASSERT((GetVertexOffsetRegAddr() != 0) && (GetInstanceOffsetRegAddr() != 0));
    PAL_ASSERT(GetVertexOffsetRegAddr()   >= userData0);
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
// branching, we will rely on the HW to discard the draw for us with the exception of the zero instanceCount case on
// pre-gfx8 because that HW treats zero instances as one instance.
template <GfxIpLevel gfxLevel, bool issueSqttMarkerEvent, bool viewInstancingEnable>
void PAL_STDCALL UniversalCmdBuffer::CmdDraw(
    ICmdBuffer* pCmdBuffer,
    uint32      firstVertex,
    uint32      vertexCount,
    uint32      firstInstance,
    uint32      instanceCount)
{
    if ((gfxLevel >= GfxIpLevel::GfxIp8) || (instanceCount > 0))
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
        if (issueSqttMarkerEvent)
        {
            pThis->DescribeDraw(Developer::DrawDispatchType::CmdDraw);
        }

        uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

        pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);

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
                    pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexAuto(vertexCount,
                                                                       false,
                                                                       pThis->PacketPredicate(),
                                                                       pDeCmdSpace);
                }
            }
        }
        else
        {
            pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexAuto(vertexCount,
                                                               false,
                                                               pThis->PacketPredicate(),
                                                               pDeCmdSpace);
        }

        if (issueSqttMarkerEvent)
        {
            pDeCmdSpace += pThis->m_cmdUtil.BuildEventWrite(THREAD_TRACE_MARKER, pDeCmdSpace);
        }

        pDeCmdSpace  = pThis->m_workaroundState.PostDraw(pThis->m_graphicsState, pDeCmdSpace);
        pDeCmdSpace  = pThis->IncrementDeCounter(pDeCmdSpace);

        pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);

        if (gfxLevel != GfxIpLevel::GfxIp6)
        {
            // On Gfx7/Gfx8, the WD (Work distributor - breaks down draw commands into work groups which are sent to IA
            // units) has changed to having independent DMA and DRAW logic. As a result, DRAW_INDEX_AUTO commands have
            // added a dummy DMA command issued by the CP which overwrites the VGT_INDEX_TYPE register used by GFX. This
            // can cause hangs and rendering corruption with subsequent indexed draw commands. We must invalidate the
            // index type state so that it will be issued before the next indexed draw.
            pThis->m_drawTimeHwState.dirty.indexType = 1;
        }
    }
}

// =====================================================================================================================
// Issues a draw opaque command. We must discard the draw if instanceCount are zero. To avoid branching,
// we will rely on the HW to discard the draw for us with the exception of the zero instanceCount case on pre-gfx8
// because that HW treats zero instances as one instance.
template <GfxIpLevel gfxLevel, bool issueSqttMarkerEvent, bool viewInstancingEnable>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawOpaque(
    ICmdBuffer* pCmdBuffer,
    gpusize     streamOutFilledSizeVa,
    uint32      streamOutOffset,
    uint32      stride,
    uint32      firstInstance,
    uint32      instanceCount)
{
    if ((gfxLevel >= GfxIpLevel::GfxIp8) || (instanceCount > 0))
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
        if (issueSqttMarkerEvent)
        {
            pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawOpaque);
        }

        uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

        // Streamout filled is saved in gpuMemory, we use a me_copy to set mmVGT_STRMOUT_DRAW_OPAQUE_BUFFER_FILLED_SIZE.
        pDeCmdSpace += pThis->m_cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_MEM_MAPPED_REG_DC,
                                                      mmVGT_STRMOUT_DRAW_OPAQUE_BUFFER_FILLED_SIZE,
                                                      COPY_DATA_SEL_SRC_MEMORY,
                                                      streamOutFilledSizeVa,
                                                      COPY_DATA_SEL_COUNT_1DW,
                                                      COPY_DATA_ENGINE_ME,
                                                      COPY_DATA_WR_CONFIRM_WAIT,
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
                    pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexAuto(0,
                                                                       true,
                                                                       pThis->PacketPredicate(),
                                                                       pDeCmdSpace);
                }
            }
        }
        else
        {
            pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexAuto(0, true, pThis->PacketPredicate(), pDeCmdSpace);
        }

        if (issueSqttMarkerEvent)
        {
            pDeCmdSpace += pThis->m_cmdUtil.BuildEventWrite(THREAD_TRACE_MARKER, pDeCmdSpace);
        }

        pDeCmdSpace  = pThis->m_workaroundState.PostDraw(pThis->m_graphicsState, pDeCmdSpace);
        pDeCmdSpace  = pThis->IncrementDeCounter(pDeCmdSpace);

        pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);

        if (gfxLevel != GfxIpLevel::GfxIp6)
        {
            // On Gfx7/Gfx8, the WD (Work distributor - breaks down draw commands into work groups which are sent to IA
            // units) has changed to having independent DMA and DRAW logic. As a result, DRAW_INDEX_AUTO commands have
            // added a dummy DMA command issued by the CP which overwrites the VGT_INDEX_TYPE register used by GFX. This
            // can cause hangs and rendering corruption with subsequent indexed draw commands. We must invalidate the
            // index type state so that it will be issued before the next indexed draw.
            pThis->m_drawTimeHwState.dirty.indexType = 1;
        }
    }
}

// =====================================================================================================================
// Issues an indexed draw command. We must discard the draw if indexCount or instanceCount are zero. To avoid branching,
// we will rely on the HW to discard the draw for us with the exception of the zero instanceCount case on pre-gfx8
// because that HW treats zero instances as one instance.
template <GfxIpLevel gfxLevel, bool issueSqttMarkerEvent, bool viewInstancingEnable>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawIndexed(
    ICmdBuffer* pCmdBuffer,
    uint32      firstIndex,
    uint32      indexCount,
    int32       vertexOffset,
    uint32      firstInstance,
    uint32      instanceCount)
{
    if ((gfxLevel >= GfxIpLevel::GfxIp8) || (instanceCount > 0))
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
        if (issueSqttMarkerEvent)
        {
            pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawIndexed);
        }

        uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

        pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);

        const uint32 validIndexCount = pThis->m_graphicsState.iaState.indexCount - firstIndex;

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
                    pDeCmdSpace = pThis->BuildWriteViewId(viewInstancingDesc.viewId[i], pDeCmdSpace);

                    if (pThis->IsNested() && (pThis->m_graphicsState.iaState.indexAddr == 0))
                    {
                        // If IB state is not bound, nested command buffers must use DRAW_INDEX_OFFSET_2 so that
                        // we can inherit the IB base and size from direct command buffer
                        pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexOffset2(indexCount,
                                                                              validIndexCount,
                                                                              firstIndex,
                                                                              pThis->PacketPredicate(),
                                                                              pDeCmdSpace);
                    }
                    else
                    {
                        // Compute the address of the IB. We must add the index offset specified by firstIndex
                        // into our address because DRAW_INDEX_2 doesn't take an offset param.
                        const uint32  indexSize   = 1 << static_cast<uint32>(pThis->m_graphicsState.iaState.indexType);
                        const gpusize gpuVirtAddr = pThis->m_graphicsState.iaState.indexAddr + (indexSize * firstIndex);

                        pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndex2(indexCount,
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
                // we can inherit the IB base and size from direct command buffer
                pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndexOffset2(indexCount,
                                                                      validIndexCount,
                                                                      firstIndex,
                                                                      pThis->PacketPredicate(),
                                                                      pDeCmdSpace);
            }
            else
            {
                // Compute the address of the IB. We must add the index offset specified by firstIndex
                // into our address because DRAW_INDEX_2 doesn't take an offset param.
                const uint32  indexSize   = 1 << static_cast<uint32>(pThis->m_graphicsState.iaState.indexType);
                const gpusize gpuVirtAddr = pThis->m_graphicsState.iaState.indexAddr + (indexSize * firstIndex);

                pDeCmdSpace += pThis->m_cmdUtil.BuildDrawIndex2(indexCount,
                                                                validIndexCount,
                                                                gpuVirtAddr,
                                                                pThis->PacketPredicate(),
                                                                pDeCmdSpace);
            }
        }

        if (issueSqttMarkerEvent)
        {
            pDeCmdSpace += pThis->m_cmdUtil.BuildEventWrite(THREAD_TRACE_MARKER, pDeCmdSpace);
        }

        pDeCmdSpace  = pThis->m_workaroundState.PostDraw(pThis->m_graphicsState, pDeCmdSpace);
        pDeCmdSpace  = pThis->IncrementDeCounter(pDeCmdSpace);

        pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);
    }
}

// =====================================================================================================================
// Issues an indirect non-indexed draw command. We must discard the draw if vertexCount or instanceCount are zero.
// We will rely on the HW to discard the draw for us.
template <GfxIpLevel gfxLevel, bool issueSqttMarkerEvent, bool viewInstancingEnable>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawIndirectMulti(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)) && IsPow2Aligned(countGpuAddr, sizeof(uint32)));
    PAL_ASSERT(offset + (sizeof(DrawIndirectArgs) * maximumCount) <= gpuMemory.Desc().size);

    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

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
    if (issueSqttMarkerEvent)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawIndirectMulti);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace += pThis->m_cmdUtil.BuildSetBase(
        ShaderGraphics, BASE_INDEX_DRAW_INDIRECT, gpuMemory.Desc().gpuVirtAddr, pDeCmdSpace);

    const uint16 vtxOffsetReg  = pThis->GetVertexOffsetRegAddr();
    const uint16 instOffsetReg = pThis->GetInstanceOffsetRegAddr();

    pThis->m_deCmdStream.NotifyIndirectShRegWrite(vtxOffsetReg);
    pThis->m_deCmdStream.NotifyIndirectShRegWrite(instOffsetReg);

    pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);

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
        pDeCmdSpace += pThis->m_cmdUtil.BuildEventWrite(THREAD_TRACE_MARKER, pDeCmdSpace);
    }

    pDeCmdSpace = pThis->m_workaroundState.PostDraw(pThis->m_graphicsState, pDeCmdSpace);
    pDeCmdSpace = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);

    pThis->m_state.flags.containsDrawIndirect = 1;

    if (gfxLevel != GfxIpLevel::GfxIp6)
    {
        // On Gfx7/Gfx8, we need to invalidate the index type which was previously programmed because the CP clobbers
        // that state when executing a non-indexed indirect draw.
        // SEE: CmdDraw() for more details about why we do this.
        pThis->m_drawTimeHwState.dirty.indexType = 1;
    }
}

// =====================================================================================================================
// Issues an indirect indexed draw command. We must discard the draw if indexCount or instanceCount are zero.
// We will rely on the HW to discard the draw for us.
template <GfxIpLevel gfxLevel, bool issueSqttMarkerEvent, bool viewInstancingEnable>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawIndexedIndirectMulti(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)) && IsPow2Aligned(countGpuAddr, sizeof(uint32)));
    PAL_ASSERT(offset + (sizeof(DrawIndexedIndirectArgs) * maximumCount) <= gpuMemory.Desc().size);

    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

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
    if (issueSqttMarkerEvent)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawIndexedIndirectMulti);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace += pThis->m_cmdUtil.BuildSetBase(
        ShaderGraphics, BASE_INDEX_DRAW_INDIRECT, gpuMemory.Desc().gpuVirtAddr, pDeCmdSpace);

    const uint16 vtxOffsetReg  = pThis->GetVertexOffsetRegAddr();
    const uint16 instOffsetReg = pThis->GetInstanceOffsetRegAddr();

    pThis->m_deCmdStream.NotifyIndirectShRegWrite(vtxOffsetReg);
    pThis->m_deCmdStream.NotifyIndirectShRegWrite(instOffsetReg);

    pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);

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

    if (issueSqttMarkerEvent)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildEventWrite(THREAD_TRACE_MARKER, pDeCmdSpace);
    }

    pDeCmdSpace = pThis->m_workaroundState.PostDraw(pThis->m_graphicsState, pDeCmdSpace);
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
    pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect(x, y, z, false, true, pThis->PacketPredicate(), pDeCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildEventWrite(THREAD_TRACE_MARKER, pDeCmdSpace);
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
    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)));
    PAL_ASSERT(offset + sizeof(DispatchIndirectArgs) <= gpuMemory.Desc().size);

    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    if (IssueSqttMarkerEvent)
    {
        pThis->m_device.DescribeDispatch(pThis, Developer::DrawDispatchType::CmdDispatchIndirect, 0, 0, 0, 0, 0, 0);
    }

    const gpusize gpuMemBaseAddr = gpuMemory.Desc().gpuVirtAddr;

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace  = pThis->ValidateDispatch<UseCpuPathForUserDataTables>((gpuMemBaseAddr + offset), 0, 0, 0, pDeCmdSpace);
    pDeCmdSpace += pThis->m_cmdUtil.BuildSetBase(
        ShaderCompute, BASE_INDEX_DISPATCH_INDIRECT, gpuMemBaseAddr, pDeCmdSpace);
    pDeCmdSpace  = pThis->WaitOnCeCounter(pDeCmdSpace);
    pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchIndirect(offset, pThis->PacketPredicate(), pDeCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildEventWrite(THREAD_TRACE_MARKER, pDeCmdSpace);
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
    pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect(xDim,
                                                        yDim,
                                                        zDim,
                                                        false,
                                                        false,
                                                        pThis->PacketPredicate(),
                                                        pDeCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildEventWrite(THREAD_TRACE_MARKER, pDeCmdSpace);
    }

    pDeCmdSpace  = pThis->IncrementDeCounter(pDeCmdSpace);

    pThis->m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdCloneImageData(
    const IImage& srcImage,
    const IImage& dstImage)
{
    m_device.RsrcProcMgr().CmdCloneImageData(this, GetGfx6Image(srcImage), GetGfx6Image(dstImage));
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

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace += m_cmdUtil.BuildWriteData(pGpuMemory->GetBusAddrMarkerVa() + offset,
                                            1,
                                            WRITE_DATA_ENGINE_ME,
                                            WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                            true,
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
        pDeCmdSpace += m_cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_ASYNC_MEMORY,
                                               address,
                                               COPY_DATA_SEL_SRC_GPU_CLOCK_COUNT,
                                               0,
                                               COPY_DATA_SEL_COUNT_2DW,
                                               COPY_DATA_ENGINE_ME,
                                               COPY_DATA_WR_CONFIRM_WAIT,
                                               pDeCmdSpace);
    }
    else
    {
        PAL_ASSERT(pipePoint == HwPipeBottom);

        pDeCmdSpace += m_cmdUtil.BuildEventWriteEop(BOTTOM_OF_PIPE_TS,
                                                    address,
                                                    EVENTWRITEEOP_DATA_SEL_SEND_GPU_CLOCK,
                                                    0,
                                                    false,
                                                    pDeCmdSpace);
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
        pDeCmdSpace += m_cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_ASYNC_MEMORY,
                                               address,
                                               COPY_DATA_SEL_SRC_IMME_DATA,
                                               data,
                                               ((dataSize == ImmediateDataWidth::ImmediateData32Bit) ?
                                                   COPY_DATA_SEL_COUNT_1DW :
                                                   COPY_DATA_SEL_COUNT_2DW),
                                               COPY_DATA_ENGINE_ME,
                                               COPY_DATA_WR_CONFIRM_WAIT,
                                               pDeCmdSpace);
    }
    else
    {
        PAL_ASSERT(pipePoint == HwPipeBottom);

        // CmdUtil will properly route to EventWriteEop/ReleaseMem as appropriate.
        pDeCmdSpace += m_cmdUtil.BuildEventWriteEop(BOTTOM_OF_PIPE_TS,
                                                    address,
                                                    ((dataSize == ImmediateDataWidth::ImmediateData32Bit) ?
                                                        EVENTWRITEEOP_DATA_SEL_SEND_DATA32 :
                                                        EVENTWRITEEOP_DATA_SEL_SEND_DATA64),
                                                    data,
                                                    false,
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
        auto*const       pPipelineState = PipelineState(pipelineBindPoint);
        const auto*const pNewPalette    = static_cast<const BorderColorPalette*>(pPalette);

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
    const uint32 userDataAddr = (markerType == PerfTraceMarkerType::A) ?
                                m_device.CmdUtil().GetRegInfo().mmSqThreadTraceUserData2 :
                                m_device.CmdUtil().GetRegInfo().mmSqThreadTraceUserData3;
    PAL_ASSERT(m_device.CmdUtil().IsPrivilegedConfigReg(userDataAddr) == false);

    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
    pCmdSpace = m_deCmdStream.WriteSetOneConfigReg(userDataAddr, markerData, pCmdSpace);
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

    const size_t cmdDwords = (m_cmdUtil.BuildSetSeqContextRegs(mmDB_Z_INFO,
                                                               mmDB_STENCIL_WRITE_BASE,
                                                               &pm4Commands.hdrDbZInfo) +
                              m_cmdUtil.BuildSetSeqContextRegs(mmPA_SC_SCREEN_SCISSOR_TL,
                                                               mmPA_SC_SCREEN_SCISSOR_BR,
                                                               &pm4Commands.hdrPaScScreenScissorTlBr) +
                              m_cmdUtil.BuildSetOneContextReg(mmDB_HTILE_DATA_BASE,
                                                              &pm4Commands.hdrDbHtileDataBase));

    pm4Commands.dbZInfo.u32All            = 0;
    pm4Commands.dbStencilInfo.u32All      = 0;
    pm4Commands.dbZReadBase.u32All        = 0;
    pm4Commands.dbStencilReadBase.u32All  = 0;
    pm4Commands.dbZWriteBase.u32All       = 0;
    pm4Commands.dbStencilWriteBase.u32All = 0;

    pm4Commands.paScScreenScissorTl.bits.TL_X = PaScScreenScissorMin;
    pm4Commands.paScScreenScissorTl.bits.TL_Y = PaScScreenScissorMin;
    pm4Commands.paScScreenScissorBr.bits.BR_X = PaScScreenScissorMax;
    pm4Commands.paScScreenScissorBr.bits.BR_Y = PaScScreenScissorMax;

    pm4Commands.dbHtileDataBase.u32All = 0;

    PAL_ASSERT(cmdDwords != 0);
    return m_deCmdStream.WritePm4Image(cmdDwords, &pm4Commands, pCmdSpace);
}

// =====================================================================================================================
// Build the NULL color targets PM4 packets. It is not safe to call this when there are no NULL color targets.
uint32* UniversalCmdBuffer::WriteNullColorTargets(
    uint32* pCmdSpace,
    uint32  newColorTargetMask, // Each bit set in this mask indicates a valid color-target is being bound to the
                                // corresponding slot.
    uint32  oldColorTargetMask) // Each bit set in this mask indicates a valid color-target was previously bound to
                                // the corresponding slot.
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
    pDeCmdSpace += m_cmdUtil.BuildEventWrite(PIPELINESTAT_START, pDeCmdSpace);

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

    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    // PA_SC_RASTER_CONFIG and PA_SC_RASTER_CONFIG_1 values are given to us by the KMD.
    regPA_SC_RASTER_CONFIG           paScRasterConfig  = {};
    regPA_SC_RASTER_CONFIG_1__CI__VI paScRasterConfig1 = {};
    paScRasterConfig.u32All  = chipProps.gfx6.paScRasterCfg;
    paScRasterConfig1.u32All = chipProps.gfx6.paScRasterCfg1;

    if (m_device.Parent()->ChipProperties().gfxLevel == GfxIpLevel::GfxIp6)
    {
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmPA_SC_RASTER_CONFIG,
                                                          paScRasterConfig.u32All,
                                                          pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = m_deCmdStream.WriteSetPaScRasterConfig(paScRasterConfig, pDeCmdSpace);

        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmPA_SC_RASTER_CONFIG_1__CI__VI,
                                                          paScRasterConfig1.u32All,
                                                          pDeCmdSpace);
    }

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

    // With the PM4 optimizer enabled, certain registers are only updated via RMW packets and not having an initial
    // value causes the optimizer to skip optimizing redundant RMW packets.
    if (m_deCmdStream.Pm4OptimizerEnabled())
    {
        // Initialise registers that are only updated via RMW packets
        // DB_ALPHA_TO_MASK register gets updated by MSAA state and pipeline state via RMW.
        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmDB_ALPHA_TO_MASK, 0, pDeCmdSpace);

        if (IsNested() == false)
        {
            // Nested command buffers inherit parts of the following registers and hence must not be reset in
            // the preamble.

            // PA_SC_AA_CONFIG.bits are updated based on MSAA state and CmdSetMsaaQuadSamplePattern via RMW packets.
            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmPA_SC_AA_CONFIG, 0, pDeCmdSpace);

            constexpr uint32 ZeroStencilRefMasks[] = { 0, 0 };
            pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmDB_STENCILREFMASK,
                                                               mmDB_STENCILREFMASK_BF,
                                                               &ZeroStencilRefMasks[0],
                                                               pDeCmdSpace);
        }

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

    if (m_gfxCmdBufState.cpBltActive)
    {
        // Stalls the CP ME until the CP's DMA engine has finished all previous "CP blts" (CP_DMA/DMA_DATA commands
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
    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    // GFX6-8 should always have supportReleaseAcquireInterface=0, so GpuEvent is always single slot (one dword).
    PAL_ASSERT(m_device.Parent()->ChipProperties().gfxip.numSlotsPerEvent == 1);

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

    switch (pipePoint)
    {
    case HwPipeTop:
        // Implement set/reset event with a WRITE_DATA command using PFP engine.
        pDeCmdSpace += m_cmdUtil.BuildWriteData(boundMemObj.GpuVirtAddr(),
                                                1,
                                                WRITE_DATA_ENGINE_PFP,
                                                WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                                true,
                                                &data,
                                                PredDisable,
                                                pDeCmdSpace);
        break;

    case HwPipePostIndexFetch:
        // Implement set/reset event with a WRITE_DATA command using ME engine.
        pDeCmdSpace += m_cmdUtil.BuildWriteData(boundMemObj.GpuVirtAddr(),
                                                1,
                                                WRITE_DATA_ENGINE_ME,
                                                WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                                true,
                                                &data,
                                                PredDisable,
                                                pDeCmdSpace);
        break;

    case HwPipePreRasterization:
    case HwPipePostPs:
    case HwPipePostCs:
        // Implement set/reset with an EOS event waiting for VS/PS or CS waves to complete.  Unfortunately, there is
        // no VS_DONE event with which to implement HwPipePreRasterization, so it has to conservatively use PS_DONE.
        pDeCmdSpace += m_cmdUtil.BuildEventWriteEos((pipePoint == HwPipePostCs) ? CS_DONE : PS_DONE,
                                                    boundMemObj.GpuVirtAddr(),
                                                    EVENT_WRITE_EOS_CMD_STORE_32BIT_DATA_TO_MEMORY,
                                                    data,
                                                    0,
                                                    0,
                                                    pDeCmdSpace);
        break;

    case HwPipeBottom:
        // Implement set/reset with an EOP event written when all prior GPU work completes.
        pDeCmdSpace += m_cmdUtil.BuildEventWriteEop(BOTTOM_OF_PIPE_TS,
                                                    boundMemObj.GpuVirtAddr(),
                                                    EVENTWRITEEOP_DATA_SEL_SEND_DATA32,
                                                    data,
                                                    false,
                                                    pDeCmdSpace);
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
// Helper function to instruct the DE to wait on the CE counter at draw or dispatch time if a CE RAM dump was performed
// prior to the draw or dispatch operation or during validation.
uint32* UniversalCmdBuffer::WaitOnCeCounter(
    uint32* pDeCmdSpace)
{
    if (m_state.flags.ceStreamDirty != 0)
    {
        pDeCmdSpace += m_cmdUtil.BuildWaitOnCeCounter((m_state.flags.ceInvalidateKcache != 0), pDeCmdSpace);

        m_state.flags.ceInvalidateKcache = 0;
        m_state.flags.ceStreamDirty      = 0;
        m_state.flags.deCounterDirty     = 1;
    }

    return pDeCmdSpace;
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
// Helper function for relocating a user-data table
void UniversalCmdBuffer::RelocateUserDataTable(
    UserDataTableState* pTable,
    uint32              offsetInDwords, // Offset into the table where the GPU will actually read from
    uint32              dwordsNeeded)
{
    PAL_ASSERT((dwordsNeeded + offsetInDwords) <= pTable->sizeInDwords);

    // CE RAM dumps go straight through to the L2, but the shaders which will access these data read through the L1
    // and Kcache.  In order to prevent false-sharing between CE RAM dumps for consecutive draws, we need to either
    // invalidate the Kcache before each draw (awful!) or just make sure our dumps are at least cacheline-aligned.

    const uint32 offsetInBytes = (sizeof(uint32) * offsetInDwords);
    pTable->gpuVirtAddr = (AllocateGpuScratchMem(dwordsNeeded, CacheLineDwords) - offsetInBytes);
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
    pCeCmdSpace += m_cmdUtil.BuildDumpConstRam((pTable->gpuVirtAddr + offsetInBytes),
                                               (pTable->ceRamOffset + offsetInBytes),
                                               dwordsNeeded,
                                               pCeCmdSpace);

    // Mark that the CE data chunk in GPU memory is now fully up-to-date with CE RAM and that a CE RAM dump has
    // occurred since the previous Draw or Dispatch.
    pTable->dirty = 0;
    m_state.flags.ceStreamDirty = 1;

    return pCeCmdSpace;
}

// =====================================================================================================================
// Helper function responsible for handling user-SGPR updates during Draw-time validation when the active pipeline has
// changed since the previous Draw operation.  It is expected that this will be called only when the pipeline is
// changing and immediately before a call to WriteDirtyUserDataEntriesToSgprsGfx().
// Returns a mask of which hardware shader stages' user-data mappings have changed.
template <bool TessEnabled, bool GsEnabled>
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

    if (TessEnabled)
    {
        if (m_pSignatureGfx->userDataHash[LsStageId] != pPrevSignature->userDataHash[LsStageId])
        {
            changedStageMask |= (1 << LsStageId);
            pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprsGfx<true>(m_pSignatureGfx->stage[LsStageId],
                                                                             m_graphicsState.gfxUserDataEntries,
                                                                             pDeCmdSpace);
        }
        if (m_pSignatureGfx->userDataHash[HsStageId] != pPrevSignature->userDataHash[HsStageId])
        {
            changedStageMask |= (1 << HsStageId);
            pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprsGfx<true>(m_pSignatureGfx->stage[HsStageId],
                                                                             m_graphicsState.gfxUserDataEntries,
                                                                             pDeCmdSpace);
        }
    }
    if (GsEnabled)
    {
        if (m_pSignatureGfx->userDataHash[EsStageId] != pPrevSignature->userDataHash[EsStageId])
        {
            changedStageMask |= (1 << EsStageId);
            pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprsGfx<true>(m_pSignatureGfx->stage[EsStageId],
                                                                             m_graphicsState.gfxUserDataEntries,
                                                                             pDeCmdSpace);
        }
        if (m_pSignatureGfx->userDataHash[GsStageId] != pPrevSignature->userDataHash[GsStageId])
        {
            changedStageMask |= (1 << GsStageId);
            pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprsGfx<true>(m_pSignatureGfx->stage[GsStageId],
                                                                             m_graphicsState.gfxUserDataEntries,
                                                                             pDeCmdSpace);
        }
    }
    if (m_pSignatureGfx->userDataHash[VsStageId] != pPrevSignature->userDataHash[VsStageId])
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
template <bool TessEnabled, bool GsEnabled>
uint32* UniversalCmdBuffer::WriteDirtyUserDataEntriesToSgprsGfx(
    const GraphicsPipelineSignature* pPrevSignature,
    uint8                            alreadyWrittenStageMask,
    uint32*                          pDeCmdSpace)
{
    const uint8 activeStageMask = ((TessEnabled ? ((1 << LsStageId) | (1 << HsStageId)) : 0) |
                                   (GsEnabled   ? ((1 << EsStageId) | (1 << GsStageId)) : 0) |
                                   (1 << VsStageId) | (1 << PsStageId));
    const uint8 dirtyStageMask  = ((~alreadyWrittenStageMask) & activeStageMask);
    if (dirtyStageMask)
    {
        if (TessEnabled)
        {
            if (dirtyStageMask & (1 << LsStageId))
            {
                pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprsGfx<false>(m_pSignatureGfx->stage[LsStageId],
                                                                                  m_graphicsState.gfxUserDataEntries,
                                                                                  pDeCmdSpace);
            }
            if (dirtyStageMask & (1 << HsStageId))
            {
                pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprsGfx<false>(m_pSignatureGfx->stage[HsStageId],
                                                                                  m_graphicsState.gfxUserDataEntries,
                                                                                  pDeCmdSpace);
            }
        }
        if (GsEnabled)
        {
            if (dirtyStageMask & (1 << EsStageId))
            {
                pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprsGfx<false>(m_pSignatureGfx->stage[EsStageId],
                                                                                  m_graphicsState.gfxUserDataEntries,
                                                                                  pDeCmdSpace);
            }
            if (dirtyStageMask & (1 << GsStageId))
            {
                pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprsGfx<false>(m_pSignatureGfx->stage[GsStageId],
                                                                                  m_graphicsState.gfxUserDataEntries,
                                                                                  pDeCmdSpace);
            }
        }
        if (dirtyStageMask & (1 << VsStageId))
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
    static_assert(MaxFastUserDataEntriesCs <= UserDataEntriesPerMask,
                  "The CS user-data entries mapped to user-SGPR's spans multiple wide-bitfield elements!");
    constexpr uint32 AllFastUserDataEntriesMask = ((1 << MaxFastUserDataEntriesCs) - 1);
    uint16 userSgprDirtyMask = (m_computeState.csUserDataEntries.dirty[0] & AllFastUserDataEntriesMask);

    // Additionally, dirty compute user-data is always written to user-SGPR's if it could be mapped by a pipeline,
    // which lets us avoid any complex logic when switching pipelines.
    const uint16 baseUserSgpr = FirstUserDataRegAddr[static_cast<uint32>(HwShaderStage::Cs)];

    for (uint16 e = 0; e < MaxFastUserDataEntriesCs; ++e)
    {
        const uint16 firstEntry = e;
        uint16       entryCount = 0;

        while ((e < MaxFastUserDataEntriesCs) && ((userSgprDirtyMask & (1 << e)) != 0))
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
// Dispatch-time validation.  This version uses CE RAM for spill table management.
template <bool HasPipelineChanged, bool TessEnabled, bool GsEnabled>
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
            RelocateUserDataTable(&m_vbTable.state, 0, m_vbTable.watermark);
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
            RelocateUserDataTable(&m_streamOut.state, 0, StreamOutTableDwords);
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
        alreadyWrittenStageMask = FixupUserSgprsOnPipelineSwitch<TessEnabled, GsEnabled>(pPrevSignature, &pDeCmdSpace);
    }
    pDeCmdSpace = WriteDirtyUserDataEntriesToSgprsGfx<TessEnabled, GsEnabled>(pPrevSignature,
                                                                              alreadyWrittenStageMask,
                                                                              pDeCmdSpace);

    const uint16 spillThreshold = m_pSignatureGfx->spillThreshold;
    const uint16 spillsUserData = (spillThreshold != NoUserDataSpilling);
    // NOTE: Use of bitwise operators here is to reduce branchiness.
    if (((srdTableDumpMask | spillsUserData | m_state.flags.ceStreamDirty) != 0))
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
                RelocateUserDataTable(&m_spillTable.stateGfx, spillThreshold, sizeInDwords);
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

        // Step #7:
        // If any of the above validation dumped CE RAM, or if a client dumped CE RAM since the previous Draw, the
        // CE and DE counters must be synchronized before the Draw is issued.
        if (m_state.flags.ceStreamDirty)
        {
            pCeCmdSpace += m_cmdUtil.BuildIncrementCeCounter(pCeCmdSpace);
        }

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
template <bool HasPipelineChanged, bool TessEnabled, bool GsEnabled>
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
        alreadyWrittenStageMask = FixupUserSgprsOnPipelineSwitch<TessEnabled, GsEnabled>(pPrevSignature, &pDeCmdSpace);
    }
    pDeCmdSpace = WriteDirtyUserDataEntriesToSgprsGfx<TessEnabled, GsEnabled>(pPrevSignature,
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

    // Step #5:
    // Even though the spill table is not being managed using CE RAM, it is possible for the client to use CE RAM for
    // its own purposes.  In this case, we still need to increment the CE RAM counter.
    if (m_state.flags.ceStreamDirty)
    {
        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
        pCeCmdSpace += m_cmdUtil.BuildIncrementCeCounter(pCeCmdSpace);
        m_ceCmdStream.CommitCommands(pCeCmdSpace);
    }

    // All dirtied user-data entries have been written to user-SGPR's or to the spill table somewhere in this method,
    // so it is safe to clear these bits.
    memset(&m_graphicsState.gfxUserDataEntries.dirty[0], 0, sizeof(m_graphicsState.gfxUserDataEntries.dirty));

    return pDeCmdSpace;
}

// =====================================================================================================================
// Helper function which is responsible for making sure all user-data entries are written to either the spill table or
// to user-SGPR's, as well as making sure that all indirect user-data tables are up-to-date in GPU memory.  Part of
// Dispatch-time validation.  This version uses CE RAM for spill table management.
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
    // NOTE: Use of bitwise operators here is to reduce branchiness.
    if (((spillsUserData | m_state.flags.ceStreamDirty) != 0))
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
                RelocateUserDataTable(&m_spillTable.stateCs, spillThreshold, sizeInDwords);
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

        // Step #5:
        // If any of the above validation dumped CE RAM, or if a client dumped CE RAM since the previous Dispatch,
        // the CE and DE must be synchronized before the Dispatch is issued.
        if (m_state.flags.ceStreamDirty)
        {
            pCeCmdSpace += m_cmdUtil.BuildIncrementCeCounter(pCeCmdSpace);
        }

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

    // Step #4
    // Even though the spill table is not being managed using CE RAM, it is possible for the client to use CE RAM for
    // its own purposes.  In this case, we still need to increment the CE RAM counter.
    if (m_state.flags.ceStreamDirty)
    {
        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
        pCeCmdSpace += m_cmdUtil.BuildIncrementCeCounter(pCeCmdSpace);
        m_ceCmdStream.CommitCommands(pCeCmdSpace);
    }

    // All dirtied user-data entries have been written to user-SGPR's or to the spill table somewhere in this method,
    // so it is safe to clear these bits.
    memset(&m_computeState.csUserDataEntries.dirty[0], 0, sizeof(m_computeState.csUserDataEntries.dirty));

    return pDeCmdSpace;
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.  Wrapper to determine
// if immediate mode pm4 optimization is enabled before calling the real ValidateDraw() function.
template <bool indexed, bool indirect>
void UniversalCmdBuffer::ValidateDraw(
    const ValidateDrawInfo& drawInfo)
{
    if (m_deCmdStream.Pm4ImmediateOptimizerEnabled())
    {
        ValidateDraw<indexed, indirect, true>(drawInfo);
    }
    else
    {
        ValidateDraw<indexed, indirect, false>(drawInfo);
    }
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.  Wrapper to determine
// if the pipeline is dirty before calling the real ValidateDraw() function.
template <bool indexed, bool indirect, bool pm4OptImmediate>
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
        pDeCmdSpace = ValidateDraw<indexed, indirect, pm4OptImmediate, true>(drawInfo, pDeCmdSpace);

        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }
    else
    {
        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

        pDeCmdSpace = (this->*m_pfnValidateUserDataGfx)(nullptr, pDeCmdSpace);
        pDeCmdSpace = ValidateDraw<indexed, indirect, pm4OptImmediate, false>(drawInfo, pDeCmdSpace);

        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.  Wrapper to determine
// if any interesting state is dirty before calling the real ValidateDraw() function.
template <bool indexed, bool indirect, bool pm4OptImmediate, bool pipelineDirty>
uint32* UniversalCmdBuffer::ValidateDraw(
    const ValidateDrawInfo& drawInfo,
    uint32*                 pDeCmdSpace)
{
    if (m_primGroupOpt.enabled)
    {
        if (indirect)
        {
            // Since we can't compute the number of primitives this draw uses we disable this optimization to be safe.
            DisablePrimGroupOpt();
        }
        else
        {
            UpdatePrimGroupOpt(drawInfo.vtxIdxCount);
        }
    }

    if (m_graphicsState.dirtyFlags.validationBits.u16All != 0)
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
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.
template <bool indexed, bool indirect, bool pm4OptImmediate, bool pipelineDirty, bool stateDirty>
uint32* UniversalCmdBuffer::ValidateDraw(
    const ValidateDrawInfo& drawInfo,
    uint32*                 pDeCmdSpace)
{
    const auto*const pBlendState = static_cast<const ColorBlendState*>(m_graphicsState.pColorBlendState);
    const auto*const pDepthState = static_cast<const DepthStencilState*>(m_graphicsState.pDepthStencilState);
    const auto*const pMsaaState  = static_cast<const MsaaState*>(m_graphicsState.pMsaaState);
    const auto*const pPipeline   = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const auto*const pDsView =
        static_cast<const DepthStencilView*>(m_graphicsState.bindTargets.depthTarget.pDepthStencilView);

    const auto dirtyFlags = m_graphicsState.dirtyFlags.validationBits;

    // If we're about to launch a draw we better have a pipeline bound.
    PAL_ASSERT(pPipeline != nullptr);

    // All of our dirty state will leak to the caller.
    m_graphicsState.leakFlags.u32All |= m_graphicsState.dirtyFlags.u32All;

    if (pipelineDirty ||
        (stateDirty && (dirtyFlags.depthStencilState || dirtyFlags.msaaState)))
    {
        // NOTE: Due to a hardware workaround, we need to defer writing DB_SHADER_CONTROL until draw-time.
        const bool depthEnabled          = ((pDepthState != nullptr) && pDepthState->IsDepthEnabled());
        const bool usesOverRasterization = ((pMsaaState  != nullptr) && pMsaaState->UsesOverRasterization());

        pDeCmdSpace = pPipeline->WriteDbShaderControl<pm4OptImmediate>(depthEnabled,
                                                                       usesOverRasterization,
                                                                       &m_deCmdStream,
                                                                       pDeCmdSpace);
    }

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
    if (stateDirty && dirtyFlags.scissorRects)
    {
        pDeCmdSpace = ValidateScissorRects<pm4OptImmediate>(pDeCmdSpace);
    }

    regPA_SC_MODE_CNTL_1 paScModeCntl1 = m_drawTimeHwState.paScModeCntl1;

    // Re-calculate paScModeCntl1 value if state constributing to the register has changed.
    if ((m_drawTimeHwState.valid.paScModeCntl1 == 0) ||
        pipelineDirty ||
        (stateDirty && (dirtyFlags.depthStencilState || dirtyFlags.colorBlendState || dirtyFlags.depthStencilView ||
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
            // UBM performance test shows that if dst image is linear when doing graphics copy, disable super tile walk and
            // fence pattern walk will boost up to 33% performance.
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

    if (m_primGroupOpt.optimalSize > 0)
    {
        iaMultiVgtParam.bits.PRIMGROUP_SIZE = m_primGroupOpt.optimalSize - 1;
    }

    // Validate the per-draw HW state.
    pDeCmdSpace = ValidateDrawTimeHwState<indexed, indirect, pm4OptImmediate>(iaMultiVgtParam,
                                                                              vgtLsHsConfig,
                                                                              paScModeCntl1,
                                                                              dbCountControl,
                                                                              drawInfo,
                                                                              pDeCmdSpace);

    // Now that we've validated and written all per-draw state we can apply the pre-draw workarounds.
    pDeCmdSpace = m_workaroundState.PreDraw<indirect, stateDirty>
                                           (m_graphicsState,
                                            m_deCmdStream,
                                            iaMultiVgtParam,
                                            drawInfo,
                                            pDeCmdSpace);

    // Clear the dirty-state flags.
    m_graphicsState.dirtyFlags.u32All = 0;
    m_graphicsState.pipelineState.dirtyFlags.u32All = 0;

    return pDeCmdSpace;
}

// =====================================================================================================================
// Writes the latest set of viewports to HW. It is illegal to call this if the viewports aren't dirty.
template <bool pm4OptImmediate>
uint32* UniversalCmdBuffer::ValidateViewports(
    uint32*    pDeCmdSpace)
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
        const auto&             viewport        = params.viewports[i];
        VportScaleOffsetPm4Img* pScaleOffsetImg = &scaleOffsetImg[i];

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
    }

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
// Writes the latest set of scissor-rects to HW. It is illegal to call this if the scissor-rects aren't dirty.
template <bool pm4OptImmediate>
uint32* UniversalCmdBuffer::ValidateScissorRects(
    uint32*    pDeCmdSpace)
{
    const auto& viewportState = m_graphicsState.viewportState;
    const auto& scissorState  = m_graphicsState.scissorRectState;

    PAL_ASSERT(m_graphicsState.dirtyFlags.validationBits.scissorRects != 0);

    const uint32 scissorCount       = (m_graphicsState.enableMultiViewport) ? scissorState.count : 1;
    const uint32 numScissorRectRegs = ((sizeof(ScissorRectPm4Img) >> 2) * scissorCount);

    // Number of rects need cross validation
    const uint32 numberCrossValidRects = Min(scissorCount, viewportState.count);

    ScissorRectPm4Img scissorRectImg[MaxViewports];
    for (uint32 i = 0; i < scissorCount; ++i)
    {
        const auto&        scissorRect     = scissorState.scissors[i];
        ScissorRectPm4Img* pScissorRectImg = reinterpret_cast<ScissorRectPm4Img*>(&scissorRectImg[i]);

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

        pScissorRectImg->tl.u32All = 0;
        pScissorRectImg->br.u32All = 0;

        pScissorRectImg->tl.bits.WINDOW_OFFSET_DISABLE = 1;
        pScissorRectImg->tl.bits.TL_X = Clamp<int32>(left,   0, ScissorMaxTL);
        pScissorRectImg->tl.bits.TL_Y = Clamp<int32>(top,    0, ScissorMaxTL);
        pScissorRectImg->br.bits.BR_X = Clamp<int32>(right,  0, ScissorMaxBR);
        pScissorRectImg->br.bits.BR_Y = Clamp<int32>(bottom, 0, ScissorMaxBR);
    }

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
    uint32*    pDeCmdSpace)
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
template <bool indexed, bool indirect, bool pm4OptImmediate>
uint32* UniversalCmdBuffer::ValidateDrawTimeHwState(
    regIA_MULTI_VGT_PARAM   iaMultiVgtParam, // The value of the draw preamble's IA_MULTI_VGT_PARAM register.
    regVGT_LS_HS_CONFIG     vgtLsHsConfig,   // The value of the draw preamble's VGT_LS_HS_CONFIG register.
    regPA_SC_MODE_CNTL_1    paScModeCntl1,   // The value of PA_SC_MODE_CNTL_1 register.
    regDB_COUNT_CONTROL     dbCountControl,  // The value of DB_COUNT_CONTROL register
    const ValidateDrawInfo& drawInfo,        // Draw info
    uint32*                 pDeCmdSpace)     // Write new draw-engine commands here.
{
    // Start with the IA_MULTI_VGT_PARAM regsiter.
    if ((m_drawTimeHwState.iaMultiVgtParam.u32All != iaMultiVgtParam.u32All) ||
        (m_drawTimeHwState.valid.iaMultiVgtParam == 0))
    {
        m_drawTimeHwState.iaMultiVgtParam.u32All = iaMultiVgtParam.u32All;
        m_drawTimeHwState.valid.iaMultiVgtParam  = 1;

        pDeCmdSpace = m_deCmdStream.WriteSetIaMultiVgtParam<pm4OptImmediate>(iaMultiVgtParam, pDeCmdSpace);
    }

    if ((m_drawTimeHwState.vgtLsHsConfig.u32All != vgtLsHsConfig.u32All) ||
        (m_drawTimeHwState.valid.vgtLsHsConfig == 0))
    {
        m_drawTimeHwState.vgtLsHsConfig.u32All = vgtLsHsConfig.u32All;
        m_drawTimeHwState.valid.vgtLsHsConfig  = 1;

        pDeCmdSpace = m_deCmdStream.WriteSetVgtLsHsConfig<pm4OptImmediate>(vgtLsHsConfig, pDeCmdSpace);
    }

    if ((m_drawTimeHwState.paScModeCntl1.u32All != paScModeCntl1.u32All) ||
        (m_drawTimeHwState.valid.paScModeCntl1 == 0))
    {
        m_drawTimeHwState.paScModeCntl1.u32All = paScModeCntl1.u32All;
        m_drawTimeHwState.valid.paScModeCntl1  = 1;

        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg<pm4OptImmediate>(mmPA_SC_MODE_CNTL_1,
                                                                           paScModeCntl1.u32All,
                                                                           pDeCmdSpace);
    }

    if ((m_drawTimeHwState.dbCountControl.u32All != dbCountControl.u32All) ||
        (m_drawTimeHwState.valid.dbCountControl == 0))
    {
        m_drawTimeHwState.dbCountControl.u32All = dbCountControl.u32All;
        m_drawTimeHwState.valid.dbCountControl = 1;

        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg<pm4OptImmediate>(mmDB_COUNT_CONTROL,
                                                                           dbCountControl.u32All,
                                                                           pDeCmdSpace);
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
            pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics>(m_drawIndexReg, 0, pDeCmdSpace);
        }
    }

    if (indexed)
    {
        // Note that leakFlags.iaState implies an IB has been bound.
        if (m_graphicsState.leakFlags.nonValidationBits.iaState == 1)
        {
            // Write the INDEX_TYPE packet.
            if (m_drawTimeHwState.dirty.indexType != 0)
            {
                m_drawTimeHwState.dirty.indexType = 0;
                pDeCmdSpace += m_cmdUtil.BuildIndexType(m_vgtDmaIndexType, pDeCmdSpace);
            }

            // Direct indexed draws use DRAW_INDEX_2 which contains the IB base and size. This means that
            // we only have to validate the IB base and size for indirect indexed draws.
            if (indirect)
            {
                // Write the INDEX_BASE packet.
                if (m_drawTimeHwState.dirty.indexBufferBase != 0)
                {
                    m_drawTimeHwState.dirty.indexBufferBase = 0;
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

        // Write the index offset user data register.
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
                   pData);
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
            pDeCmdSpace += m_cmdUtil.BuildStrmoutBufferUpdate(idx,
                                                              STRMOUT_CNTL_OFFSET_SEL_READ_SRC_ADDRESS,
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
                                                              STRMOUT_CNTL_OFFSET_SEL_NONE,
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
                                                      STRMOUT_CNTL_OFFSET_SEL_EXPLICT_OFFSET,
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
    const uint32 packetPredicate = PacketPredicate();
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
            pDeCmdSpace += m_cmdUtil.BuildEventWrite(PIPELINESTAT_STOP, pDeCmdSpace);
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
            pDeCmdSpace += m_cmdUtil.BuildEventWrite(PIPELINESTAT_START, pDeCmdSpace);
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

        // Gfx6 and Gfx7/8 ASICs have different master enable flags
        pDbCountControl->bits.ZPASS_ENABLE__CI__VI    = 1;
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
        pDbCountControl->bits.ZPASS_ENABLE__CI__VI    = 0;
        pDbCountControl->bits.ZPASS_INCREMENT_DISABLE = 1;
    }

    return pDeCmdSpace;
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
// Returns true if the current command buffer state requires WD_SWITCH_ON_EOP=1, or if a HW workaround necessitates it.
bool UniversalCmdBuffer::ForceWdSwitchOnEop(
    const GraphicsPipeline& pipeline,
    const ValidateDrawInfo& drawInfo
    ) const
{
    // We need switch on EOP if primitive restart is enabled or if our primitive topology cannot be split between IAs.
    // The topologies that meet this requirement are below (currently PAL only supports triangle strip w/ adjacency).
    //    - Polygons (DI_PT_POLYGON)
    //    - Line loop (DI_PT_LINELOOP)
    //    - Triangle fan (DI_PT_TRIFAN)
    //    - Triangle strip w/ adjacency (DI_PT_TRISTRIP_ADJ)
    // The following primitive types support 4x primitive rate with reset index enabled for Polaris10:
    //    - Point list
    //    - Line strip
    //    - Triangle strip
    // We need to switch on EOP for opaque draws (i.e., DX10's DrawAuto) also.

    const PrimitiveTopology primTopology = m_graphicsState.inputAssemblyState.topology;
    const bool primitiveRestartEnabled   = m_graphicsState.inputAssemblyState.primitiveRestartEnable;

    bool switchOnEop = ((primTopology == PrimitiveTopology::TriangleStripAdj) ||
                        (primTopology == PrimitiveTopology::TriangleFan) ||
                        (primitiveRestartEnabled &&
                         ((m_device.Support4VgtWithResetIdx() == false) ||
                          ((primTopology != PrimitiveTopology::PointList) &&
                           (primTopology != PrimitiveTopology::LineStrip) &&
                           (primTopology != PrimitiveTopology::TriangleStrip)))) ||
                        drawInfo.useOpaque);

    if ((switchOnEop == false) && m_cachedSettings.gfx7AvoidNullPrims)
    {
        // The logic here only works properly on Gfx7+ hardware.
        //
        // Note to future developers: this optimization is not needed on gfx9!
        PAL_ASSERT(m_device.Parent()->ChipProperties().gfxLevel != GfxIpLevel::GfxIp6);

        // In a multi-instanced draw where each instance has fewer primitives than (IA_MULTI_VGT_PARAM.PRIMGROUP_SIZE+1)
        // occurs, each IA doesn't have enough work per instance to split work between their two VGT's so one VGT per IA
        // gets a series of null primitives. Eventually these null primitives back up a FIFO, causing a performance hit
        // (or a hang on Hawaii). We can avoid this by disabling 4x prim rate for any draw which meets the
        // aforementioned conditions.
        //
        // Just to be safe, we should assume that indirect draws will cause null primitives.
        // ucode will handle indirect draws on Hawaii but we should still do it ourselves to get as much performance as
        // we can on GFX8 ASICs.
        const uint32 primGroupSize =
            (m_primGroupOpt.optimalSize > 0) ? m_primGroupOpt.optimalSize
                                             : (pipeline.IaMultiVgtParam(false).bits.PRIMGROUP_SIZE + 1);

        PAL_ASSERT(pipeline.VertsPerPrimitive() != 0);
        const uint32 primCount     = drawInfo.vtxIdxCount / pipeline.VertsPerPrimitive();

        const bool   singlePrimGrp = (primCount <= primGroupSize);
        const bool   multiInstance = (drawInfo.instanceCount > 1);
        const bool   isIndirect    = (drawInfo.vtxIdxCount == 0);

        switchOnEop = (isIndirect || (singlePrimGrp && multiInstance));
    }

    return switchOnEop;
}

// =====================================================================================================================
// Issues commands to synchronize the VGT's internal stream-out state. This requires writing '1' to CP_STRMOUT_CNTL,
// issuing a VGT streamout-flush event, and waiting for the event to complete using WATIREGMEM.
uint32* UniversalCmdBuffer::FlushStreamOut(
    uint32* pDeCmdSpace)
{
    pDeCmdSpace = m_deCmdStream.WriteSetOneConfigReg(m_cmdUtil.GetRegInfo().mmCpStrmoutCntl, 0, pDeCmdSpace);

    pDeCmdSpace += m_cmdUtil.BuildEventWrite(SO_VGTSTREAMOUT_FLUSH, pDeCmdSpace);
    pDeCmdSpace += m_cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_REGISTER,
                                             WAIT_REG_MEM_FUNC_EQUAL,
                                             WAIT_REG_MEM_ENGINE_ME,
                                             m_cmdUtil.GetRegInfo().mmCpStrmoutCntl,
                                             1,
                                             0x00000001,
                                             false,
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
    // view state. The only allowed BLTs in a nested command buffer are CmdClearBoundColorTargets and
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
    const UniversalCmdBuffer* pUniversalCmdBuffer = static_cast<const UniversalCmdBuffer*>(pCmdBuffer);
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
    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

    pCmdSpace = Gfx6::ThreadTrace::WriteUpdateSqttTokenMask(&m_deCmdStream,
        pCmdSpace,
        sqttTokenConfig,
        m_device);

    m_deCmdStream.CommitCommands(pCmdSpace);
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

    const FlglRegSeq* pSeq = m_device.GetFlglRegisterSequence(syncSequence);
    const uint32 totalNumber = pSeq->regSequenceCount;

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
                pCmdSpace += m_device.CmdUtil().BuildWaitRegMem(WAIT_REG_MEM_SPACE_REGISTER,
                                                                WAIT_REG_MEM_FUNC_EQUAL,
                                                                WAIT_REG_MEM_ENGINE_ME,
                                                                seq[i].offset,
                                                                seq[i].orMask ? seq[i].andMask : 0,
                                                                seq[i].andMask,
                                                                false,
                                                                pCmdSpace);
            }
            else
            {
                // repeat 3 times to prevent dropping of command
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

    pCmdSpace += m_cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_REGISTER,
                                           CmdUtil::WaitRegMemFuncFromCompareType(compareFunc),
                                           WAIT_REG_MEM_ENGINE_ME,
                                           registerOffset,
                                           data,
                                           mask,
                                           false,
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
    const GpuMemory* pGpuMemory = static_cast<const GpuMemory*>(&gpuMemory);

    pCmdSpace += m_cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_MEMORY,
                                           CmdUtil::WaitRegMemFuncFromCompareType(compareFunc),
                                           WAIT_REG_MEM_ENGINE_ME,
                                           gpuMemory.Desc().gpuVirtAddr + offset,
                                           data,
                                           mask,
                                           pGpuMemory->IsBusAddressable(),
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
    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
    const GpuMemory* pGpuMemory = static_cast<const GpuMemory*>(&gpuMemory);

    pCmdSpace += m_cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_MEMORY,
                                           CmdUtil::WaitRegMemFuncFromCompareType(compareFunc),
                                           WAIT_REG_MEM_ENGINE_ME,
                                           pGpuMemory->GetBusAddrMarkerVa(),
                                           data,
                                           mask,
                                           pGpuMemory->IsBusAddressable(),
                                           pCmdSpace);

    m_deCmdStream.CommitCommands(pCmdSpace);
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

    m_gfxCmdBufState.clientPredicate = ((pQueryPool != nullptr) || (pGpuMemory != nullptr)) ? 1 : 0;
    m_gfxCmdBufState.packetPredicate = m_gfxCmdBufState.clientPredicate;

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
    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    DmaDataInfo dmaData = {};
    dmaData.dstSel       = CPDMA_DST_SEL_DST_ADDR;
    dmaData.dstAddr      = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;
    dmaData.dstAddrSpace = CPDMA_ADDR_SPACE_MEM;
    dmaData.srcSel       = CPDMA_SRC_SEL_SRC_ADDR;
    dmaData.srcAddr      = srcRegisterOffset;
    dmaData.srcAddrSpace = CPDMA_ADDR_SPACE_REG;
    dmaData.sync         = true;
    dmaData.usePfp       = false;
    pDeCmdSpace += m_cmdUtil.BuildDmaData(dmaData, pDeCmdSpace);

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32            cmdBufferCount,
    ICmdBuffer*const* ppCmdBuffers)
{
    for (uint32 buf = 0; buf < cmdBufferCount; ++buf)
    {
        auto*const pCallee = static_cast<Gfx6::UniversalCmdBuffer*>(ppCmdBuffers[buf]);
        PAL_ASSERT(pCallee != nullptr);

        // Track the most recent OS paging fence value across all nested command buffers called from this one.
        m_lastPagingFence = Max(m_lastPagingFence, pCallee->LastPagingFence());

        // All user-data entries have been uploaded into CE RAM and GPU memory, so we can safely "call" the nested
        // command buffer's command streams.

        const bool exclusiveSubmit = pCallee->IsExclusiveSubmit();
        const bool allowIb2Launch  = (pCallee->AllowLaunchViaIb2() &&
                                      (pCallee->m_state.flags.containsDrawIndirect == 0));

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

    const auto& gfx6Generator = static_cast<const IndirectCmdGenerator&>(generator);

    if (countGpuAddr == 0uLL)
    {
        // If the count GPU address is zero, then we are expected to use the maximumCount value as the actual number
        // of indirect commands to generate and execute.
        uint32* pMemory = CmdAllocateEmbeddedData(1, 1, &countGpuAddr);
        *pMemory = maximumCount;
    }

    // The generation of indirect commands is determined by the currently-bound pipeline.
    const PipelineBindPoint bindPoint    = ((gfx6Generator.Type() == GeneratorType::Dispatch)
                                           ? PipelineBindPoint::Compute : PipelineBindPoint::Graphics);
    const bool              setViewId    = (bindPoint == PipelineBindPoint::Graphics);
    const auto*const        pGfxPipeline =
        static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    uint32                  mask         = 1;

    if (bindPoint == PipelineBindPoint::Graphics)
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
                                                       gfx6Generator,
                                                       (gpuMemory.Desc().gpuVirtAddr + offset),
                                                       countGpuAddr,
                                                       m_graphicsState.iaState.indexCount,
                                                       maximumCount);

        m_gfxCmdBufState.packetPredicate = packetPredicate;

        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

        // Insert a CS_PARTIAL_FLUSH and invalidate/flush the texture caches to make sure that the generated commands
        // are written out to memory before we attempt to execute them. Then, a PFP_SYNC_ME is also required so that
        // the PFP doesn't prefetch the generated commands before they are finished executing.
        regCP_COHER_CNTL cpCoherCntl = { };
        cpCoherCntl.u32All = CpCoherCntlTexCacheMask;

        pDeCmdSpace += m_cmdUtil.BuildEventWrite(CS_PARTIAL_FLUSH, pDeCmdSpace);
        pDeCmdSpace += m_cmdUtil.BuildGenericSync(cpCoherCntl,
                                                  SURFACE_SYNC_ENGINE_ME,
                                                  FullSyncBaseAddr,
                                                  FullSyncSize,
                                                  false,
                                                  pDeCmdSpace);
        pDeCmdSpace += m_cmdUtil.BuildPfpSyncMe(pDeCmdSpace);

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
            if (gfx6Generator.ContainsIndexBufferBind() || (gfx6Generator.Type() == GeneratorType::Draw))
            {
                ValidateDraw<false, true>(drawInfo);
            }
            else
            {
                ValidateDraw<true, true>(drawInfo);
            }

            CommandGeneratorTouchedUserData(m_graphicsState.gfxUserDataEntries.touched, gfx6Generator, *m_pSignatureGfx);
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

            CommandGeneratorTouchedUserData(m_computeState.csUserDataEntries.touched, gfx6Generator, *m_pSignatureCs);
        }

        if (setViewId)
        {
            const auto& viewInstancingDesc = pGfxPipeline->GetViewInstancingDesc();

            pDeCmdSpace = m_deCmdStream.ReserveCommands();
            pDeCmdSpace = BuildWriteViewId(viewInstancingDesc.viewId[i], pDeCmdSpace);
            m_deCmdStream.CommitCommands(pDeCmdSpace);
        }

        pDeCmdSpace = m_deCmdStream.ReserveCommands();
        pDeCmdSpace = WaitOnCeCounter(pDeCmdSpace);
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
            pDeCmdSpace = m_workaroundState.PostDraw(m_graphicsState, pDeCmdSpace);

            if (gfx6Generator.Type() == GeneratorType::Draw)
            {
                // Command generators which issue non-indexed draws generate DRAW_INDEX_AUTO packets, which will
                // invalidate some of our draw-time HW state. SEE: CmdDraw() for more details.
                m_drawTimeHwState.dirty.indexType = 1;
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
    ///     stuff it would normally do.
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
        m_vertexOffsetReg = cmdBuffer.m_vertexOffsetReg;
        m_drawIndexReg    = cmdBuffer.m_drawIndexReg;

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
    }

    if (cmdBuffer.HasStreamOutBeenSet())
    {
        // If the nested command buffer set their own stream-out targets, we can simply copy the SRD's because CE
        // RAM is up-to-date.
        memcpy(&m_streamOut.srd[0], &cmdBuffer.m_streamOut.srd[0], sizeof(m_streamOut.srd));
    }

    m_drawTimeHwState.valid.u32All = 0;

    m_workaroundState.LeakNestedCmdBufferState(cmdBuffer.m_workaroundState);

    m_vbTable.state.dirty       |= cmdBuffer.m_vbTable.modified;
    m_spillTable.stateCs.dirty  |= cmdBuffer.m_spillTable.stateCs.dirty;
    m_spillTable.stateGfx.dirty |= cmdBuffer.m_spillTable.stateGfx.dirty;

    m_pipelineCtxPm4Hash = cmdBuffer.m_pipelineCtxPm4Hash;
    m_spiPsInControl     = cmdBuffer.m_spiPsInControl;
    m_spiVsOutConfig     = cmdBuffer.m_spiVsOutConfig;

    // It is possible that nested command buffer execute operation which affect the data in the primary buffer
    m_gfxCmdBufState.gfxBltActive              = cmdBuffer.m_gfxCmdBufState.gfxBltActive;
    m_gfxCmdBufState.csBltActive               = cmdBuffer.m_gfxCmdBufState.csBltActive;
    m_gfxCmdBufState.gfxWriteCachesDirty       = cmdBuffer.m_gfxCmdBufState.gfxWriteCachesDirty;
    m_gfxCmdBufState.csWriteCachesDirty        = cmdBuffer.m_gfxCmdBufState.csWriteCachesDirty;
    m_gfxCmdBufState.cpWriteCachesDirty        = cmdBuffer.m_gfxCmdBufState.cpWriteCachesDirty;
    m_gfxCmdBufState.cpMemoryWriteL2CacheStale = cmdBuffer.m_gfxCmdBufState.cpMemoryWriteL2CacheStale;

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

        if ((m_streamOut.srd[idx].word2.bits.NUM_RECORDS != numRecords) ||
            (m_streamOut.srd[idx].word1.bits.STRIDE      != strideInBytes))
        {
            m_streamOut.srd[idx].word2.bits.NUM_RECORDS = numRecords;
            m_streamOut.srd[idx].word1.bits.STRIDE      = strideInBytes;

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

    for (uint32 idx = 0; idx < MaxStreamOutTargets; ++idx)
    {
        if (dirtyStrideMask & (1 << idx))
        {
            // Root command buffers and nested command buffers which have changed the stream-output bindings
            // fully know the complete stream-out SRD so we can use the "normal" path.
            pCeCmdSpace += m_cmdUtil.BuildWriteConstRam(&m_streamOut.srd[idx].word1,
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

    const size_t Pm4ImageSize = ((planeCount * sizeof(UserClipPlaneStateReg)) >> 2) + CmdUtil::GetSetDataHeaderSize();

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WritePm4Image(Pm4ImageSize, &pm4Image, pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
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
    const bool isGfx7plus = m_device.Parent()->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp7;

    if (isGfx7plus)
    {
        CmdWaitRegisterValue(mmXDMA_SLV_FLIP_PENDING__CI__VI, 0, 0x00000001, CompareFunc::Equal);
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

    pDeCmdSpace += m_cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_SYS_PERF_COUNTER,
                                           frameCntReg,
                                           COPY_DATA_SEL_SRC_TC_L2,
                                           frameCountGpuAddr,
                                           COPY_DATA_SEL_COUNT_1DW,
                                           COPY_DATA_ENGINE_ME,
                                           COPY_DATA_WR_CONFIRM_NO_WAIT,
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
    bool viewInstancingEnable)
{
    if (viewInstancingEnable)
    {
        if (m_cachedSettings.issueSqttMarkerEvent)
        {
            switch (m_device.Parent()->ChipProperties().gfxLevel)
            {
            case GfxIpLevel::GfxIp6:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp6, true, true>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp6, true, true>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp6, true, true>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp6, true, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp6, true, true>;
                break;
            case GfxIpLevel::GfxIp7:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp7, true, true>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp7, true, true>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp7, true, true>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp7, true, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp7, true, true>;
                break;
            case GfxIpLevel::GfxIp8:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp8, true, true>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp8, true, true>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp8, true, true>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8, true, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8, true, true>;
                break;
            case GfxIpLevel::GfxIp8_1:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp8_1, true, true>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp8_1, true, true>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp8_1, true, true>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8_1, true, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8_1, true, true>;
                break;
            default:
                PAL_ASSERT_ALWAYS();
                break;
            }
        }
        else
        {
            switch (m_device.Parent()->ChipProperties().gfxLevel)
            {
            case GfxIpLevel::GfxIp6:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp6, false, true>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp6, false, true>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp6, false, true>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp6, false, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp6, false, true>;
                break;
            case GfxIpLevel::GfxIp7:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp7, false, true>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp7, false, true>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp7, false, true>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp7, false, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp7, false, true>;
                break;
            case GfxIpLevel::GfxIp8:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp8, false, true>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp8, false, true>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp8, false, true>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8, false, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8, false, true>;
                break;
            case GfxIpLevel::GfxIp8_1:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp8_1, false, true>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp8_1, false, true>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp8_1, false, true>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8_1, false, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8_1, false, true>;
                break;
            default:
                PAL_ASSERT_ALWAYS();
                break;
            }
        }
    }
    else
    {
        if (m_cachedSettings.issueSqttMarkerEvent)
        {
            switch (m_device.Parent()->ChipProperties().gfxLevel)
            {
            case GfxIpLevel::GfxIp6:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp6, true, false>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp6, true, false>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp6, true, false>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp6, true, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp6, true, false>;
                break;
            case GfxIpLevel::GfxIp7:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp7, true, false>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp7, true, false>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp7, true, false>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp7, true, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp7, true, false>;
                break;
            case GfxIpLevel::GfxIp8:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp8, true, false>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp8, true, false>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp8, true, false>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8, true, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8, true, false>;
                break;
            case GfxIpLevel::GfxIp8_1:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp8_1, true, false>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp8_1, true, false>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp8_1, true, false>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8_1, true, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8_1, true, false>;
                break;
            default:
                PAL_ASSERT_ALWAYS();
                break;
            }
        }
        else
        {
            switch (m_device.Parent()->ChipProperties().gfxLevel)
            {
            case GfxIpLevel::GfxIp6:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp6, false, false>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp6, false, false>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp6, false, false>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp6, false, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp6, false, false>;
                break;
            case GfxIpLevel::GfxIp7:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp7, false, false>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp7, false, false>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp7, false, false>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp7, false, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp7, false, false>;
                break;
            case GfxIpLevel::GfxIp8:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp8, false, false>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp8, false, false>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp8, false, false>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8, false, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8, false, false>;
                break;
            case GfxIpLevel::GfxIp8_1:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp8_1, false, false>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp8_1, false, false>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp8_1, false, false>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8_1, false, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8_1, false, false>;
                break;
            default:
                PAL_ASSERT_ALWAYS();
                break;
            }
        }
    }
}

// =====================================================================================================================
// Copy memory using the CP's DMA engine
void UniversalCmdBuffer::CpCopyMemory(
    gpusize dstAddr,
    gpusize srcAddr,
    gpusize numBytes)
{
    // We want to read and write through L2 because it's faster and expected by CoherCopy but if it isn't supported
    // we need to fall back to a memory-to-memory copy.
    const bool supportsL2 = (m_device.Parent()->ChipProperties().gfxLevel > GfxIpLevel::GfxIp6);

    PAL_ASSERT(numBytes < (1ull << 32));

    DmaDataInfo dmaDataInfo = {};
    dmaDataInfo.dstSel      = supportsL2 ? CPDMA_DST_SEL_DST_ADDR_USING_L2 : CPDMA_DST_SEL_DST_ADDR;
    dmaDataInfo.srcSel      = supportsL2 ? CPDMA_SRC_SEL_SRC_ADDR_USING_L2 : CPDMA_SRC_SEL_SRC_ADDR;
    dmaDataInfo.sync        = false;
    dmaDataInfo.usePfp      = false;
    dmaDataInfo.predicate   = static_cast<PM4Predicate>(GetGfxCmdBufState().packetPredicate);
    dmaDataInfo.dstAddr     = dstAddr;
    dmaDataInfo.srcAddr     = srcAddr;
    dmaDataInfo.numBytes    = static_cast<uint32>(numBytes);

    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
    pCmdSpace += m_cmdUtil.BuildDmaData(dmaDataInfo, pCmdSpace);
    m_deCmdStream.CommitCommands(pCmdSpace);

    SetGfxCmdBufCpBltState(true);

    if (supportsL2)
    {
        SetGfxCmdBufCpBltWriteCacheState(true);
    }
    else
    {
        SetGfxCmdBufCpMemoryWriteL2CacheStaleState(true);
    }
}

} // Gfx6
} // Pal
