/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6UniversalCmdBuffer.h"
#include "core/hw/gfxip/queryPool.h"
#include "core/cmdAllocator.h"
#include "g_platformSettings.h"
#include "palMath.h"
#include "palIntervalTreeImpl.h"
#include "palVectorImpl.h"
#include "palIterator.h"

#include <float.h>
#include <limits.h>
#include <type_traits>

using namespace Util;

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

// The DB_RENDER_OVERRIDE fields owned by the graphics pipeline.
constexpr uint32 PipelineDbRenderOverrideMask  = DB_RENDER_OVERRIDE__FORCE_SHADER_Z_ORDER_MASK  |
                                                 DB_RENDER_OVERRIDE__FORCE_STENCIL_READ_MASK    |
                                                 DB_RENDER_OVERRIDE__DISABLE_VIEWPORT_CLAMP_MASK;

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

    uint32 numRecords = ((UINT_MAX - chipProps.gfx6.nativeWavefrontSize) + 1);
    if ((chipProps.gfxLevel >= GfxIpLevel::GfxIp8) && (strideInBytes > 0))
    {
        PAL_ANALYSIS_ASSUME(strideInBytes != 0);
        // On GFX8.x, NUM_RECORDS is in bytes, so we need to take the vertex stride into account when computing
        // the stream-out clamp value expected by SC.
        numRecords = (strideInBytes * (((UINT_MAX / strideInBytes) - chipProps.gfx6.nativeWavefrontSize) + 1));
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
                                 &m_deCmdStream,
                                 &m_ceCmdStream,
                                 nullptr,
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
    m_cachedSettings.rbPlusSupported            = m_device.Parent()->ChipProperties().gfx6.rbPlus;

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

    m_cachedSettings.issueSqttMarkerEvent = device.Parent()->IssueSqttMarkerEvents();
    m_cachedSettings.describeDrawDispatch = (m_cachedSettings.issueSqttMarkerEvent ||
                                             platformSettings.cmdBufferLoggerConfig.embedDrawDispatchInfo);

    m_cachedSettings.has32bPred =
        m_device.Parent()->EngineProperties().perEngine[EngineTypeUniversal].flags.memory32bPredicationSupport;
#if PAL_DEVELOPER_BUILD
    m_cachedSettings.enablePm4Instrumentation = platformSettings.pm4InstrumentorEnabled;
#endif

#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 755)
    // Recommended defaults for GFX8
    m_tessDistributionFactors = { 8, 8, 8, 8, 7 };
#endif

    m_sxPsDownconvert.u32All     = 0;
    m_sxBlendOptEpsilon.u32All   = 0;
    m_sxBlendOptControl.u32All   = 0;
    m_dbRenderOverride.u32All    = 0;
    m_paSuLineStippleCntl.u32All = 0;
    m_paScLineStipple.u32All     = 0;
    m_cbColorControl.u32All      = 0;
    m_paClClipCntl.u32All        = 0;
    m_cbTargetMask.u32All        = 0;
    m_vgtTfParam.u32All          = 0;
    m_paScLineCntl.u32All        = 0;
    m_dbShaderControl.u32All     = 0;
    m_paSuScModeCntl.u32All      = InvalidPaSuScModeCntlVal;
    m_depthClampMode             = DepthClampMode::Viewport;
    SwitchDrawFunctions(false);
}

// =====================================================================================================================
// Initializes Gfx6-specific functionality.
Result UniversalCmdBuffer::Init(
    const CmdBufferInternalCreateInfo& internalInfo)
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    m_spillTable.stateCs.sizeInDwords  = chipProps.gfxip.maxUserDataEntries;
    m_spillTable.stateGfx.sizeInDwords = chipProps.gfxip.maxUserDataEntries;
    m_streamOut.state.sizeInDwords     = (sizeof(m_streamOut.srd) / sizeof(uint32));
    m_vbTable.pSrds                    = static_cast<BufferSrd*>(VoidPtrAlign((this + 1), alignof(BufferSrd)));
    m_vbTable.state.sizeInDwords       = ((sizeof(BufferSrd) / sizeof(uint32)) * MaxVertexBuffers);

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
// Sets-up function pointers for the Dispatch entrypoint and all variants.
template <bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
void UniversalCmdBuffer::SetDispatchFunctions()
{
    m_funcTable.pfnCmdDispatch         = CmdDispatch<IssueSqttMarkerEvent, DescribeDrawDispatch>;
    m_funcTable.pfnCmdDispatchIndirect = CmdDispatchIndirect<IssueSqttMarkerEvent, DescribeDrawDispatch>;
    m_funcTable.pfnCmdDispatchOffset   = CmdDispatchOffset<IssueSqttMarkerEvent, DescribeDrawDispatch>;
}

// =====================================================================================================================
// Sets up function pointers for Draw-time validation of graphics user-data entries.
template <bool TessEnabled, bool GsEnabled>
void UniversalCmdBuffer::SetUserDataValidationFunctions()
{
    m_pfnValidateUserDataGfx =
        &UniversalCmdBuffer::ValidateGraphicsUserData<false, TessEnabled, GsEnabled>;
    m_pfnValidateUserDataGfxPipelineSwitch =
        &UniversalCmdBuffer::ValidateGraphicsUserData<true, TessEnabled, GsEnabled>;
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

    Pal::Pm4::UniversalCmdBuffer::ResetState();

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

    SetUserDataValidationFunctions(false, false);

    m_vgtDmaIndexType.u32All = 0;
    m_vgtDmaIndexType.bits.SWAP_MODE  = VGT_DMA_SWAP_NONE;
    m_vgtDmaIndexType.bits.INDEX_TYPE = VgtIndexTypeLookup[0];

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

    m_spiVsOutConfig.u32All      = 0;
    m_spiPsInControl.u32All      = 0;
    m_paSuLineStippleCntl.u32All = 0;
    m_paScLineStipple.u32All     = 0;
    m_paSuScModeCntl.u32All      = InvalidPaSuScModeCntlVal;

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

    m_vertexOffsetReg = UserDataNotMapped;
    m_drawIndexReg    = UserDataNotMapped;

    m_pSignatureCs       = &NullCsSignature;
    m_pSignatureGfx      = &NullGfxSignature;
    m_pipelineCtxRegHash = 0;

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

        if (pNewPipeline != pOldPipeline)
        {
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
        }

        if (pNewPipeline != nullptr)
        {
            regVGT_TF_PARAM        vgtTfParam       = pNewPipeline->VgtTfParam();
            regPA_CL_CLIP_CNTL     paClClipCntl     = pNewPipeline->PaClClipCntl();
            regPA_SC_LINE_CNTL     paScLineCntl     = pNewPipeline->PaScLineCntl();
            regCB_TARGET_MASK      cbTargetMask     = pNewPipeline->CbTargetMask();
            regCB_COLOR_CONTROL    cbColorControl   = pNewPipeline->CbColorControl();
            regDB_SHADER_CONTROL   dbShaderControl  = pNewPipeline->DbShaderControl();
            regDB_RENDER_OVERRIDE  dbRenderOverride = m_dbRenderOverride;
            BitfieldUpdateSubfield(
                &(dbRenderOverride.u32All), pNewPipeline->DbRenderOverride().u32All, PipelineDbRenderOverrideMask);

            // If RB+ is enabled, we must update the PM4 image of RB+ register state with the new pipelines' values.  This
            // should be done here instead of inside SwitchGraphicsPipeline() because RPM sometimes overrides these values
            // for certain blit operations.
            if (m_cachedSettings.rbPlusSupported != 0)
            {
                pNewPipeline->GetRbPlusRegisters(false,
                                                 &m_sxPsDownconvert,
                                                 &m_sxBlendOptEpsilon,
                                                 &m_sxBlendOptControl);
            }
            m_depthClampMode = pNewPipeline->GetDepthClampMode();
            // Update context registers according dynamic states
            if (params.graphics.dynamicState.enable.u32All != 0)
            {
                if (params.graphics.dynamicState.enable.switchWinding)
                {
                    if (params.graphics.dynamicState.switchWinding)
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

                if (params.graphics.dynamicState.enable.logicOp)
                {
                    cbColorControl.bits.ROP3 = Rop3(params.graphics.dynamicState.logicOp);
                }

                if (params.graphics.dynamicState.enable.rasterizerDiscardEnable)
                {
                    paClClipCntl.bits.DX_RASTERIZATION_KILL = params.graphics.dynamicState.rasterizerDiscardEnable;
                }

                if (params.graphics.dynamicState.enable.depthClipMode)
                {
                    paClClipCntl.bits.ZCLIP_NEAR_DISABLE = params.graphics.dynamicState.depthClipNearEnable ? 0 : 1;
                    paClClipCntl.bits.ZCLIP_FAR_DISABLE = params.graphics.dynamicState.depthClipFarEnable ? 0 : 1;
                }

                if (params.graphics.dynamicState.enable.depthRange)
                {
                    paClClipCntl.bits.DX_CLIP_SPACE_DEF =
                        (params.graphics.dynamicState.depthRange == DepthRange::ZeroToOne);
                }

                if (params.graphics.dynamicState.enable.perpLineEndCapsEnable)
                {
                    paScLineCntl.bits.PERPENDICULAR_ENDCAP_ENA = params.graphics.dynamicState.perpLineEndCapsEnable;
                }

                if (params.graphics.dynamicState.enable.colorWriteMask)
                {
                    cbTargetMask.u32All =
                        (pNewPipeline->CbTargetMask().u32All & params.graphics.dynamicState.colorWriteMask);
                }

                if (params.graphics.dynamicState.enable.alphaToCoverageEnable)
                {
                    dbShaderControl.bits.ALPHA_TO_MASK_DISABLE =
                        params.graphics.dynamicState.alphaToCoverageEnable ? 0 : 1;
                }

                if (params.graphics.dynamicState.enable.depthClampMode)
                {
                    // For internal RPM pipelines, we want to always disable depth clamp based on depthClampMode
                    // without honor setting of depthClampBasedOnZExport.
                    if (m_device.Parent()->GetPublicSettings()->depthClampBasedOnZExport &&
                        (m_gfxCmdBufStateFlags.isGfxStatePushed == 0)) // Indicates binding a non-RPM pipeline
                    {
                        dbRenderOverride.bits.DISABLE_VIEWPORT_CLAMP =
                            ((params.graphics.dynamicState.depthClampMode == DepthClampMode::_None) &&
                             (pNewPipeline->DbShaderControl().bits.Z_EXPORT_ENABLE != 0));
                    }
                    else
                    {
                        dbRenderOverride.bits.DISABLE_VIEWPORT_CLAMP =
                            (params.graphics.dynamicState.depthClampMode == DepthClampMode::_None);
                    }

                    m_depthClampMode = params.graphics.dynamicState.depthClampMode;
                }

                if (params.graphics.dynamicState.enable.dualSourceBlendEnable)
                {
                    if (m_cachedSettings.rbPlusSupported != 0)
                    {
                        cbColorControl.bits.DISABLE_DUAL_QUAD__VI =
                            params.graphics.dynamicState.dualSourceBlendEnable ? 1 : 0;
                        pNewPipeline->GetRbPlusRegisters(params.graphics.dynamicState.dualSourceBlendEnable,
                                                         &m_sxPsDownconvert,
                                                         &m_sxBlendOptEpsilon,
                                                         &m_sxBlendOptControl);
                    }
                }
            }

            // Update pipeline dynamic state dirty flags
            if ((vgtTfParam.u32All       != m_vgtTfParam.u32All)      ||
                (cbColorControl.u32All   != m_cbColorControl.u32All)  ||
                (paClClipCntl.u32All     != m_paClClipCntl.u32All)    ||
                (paScLineCntl.u32All     != m_paScLineCntl.u32All)    ||
                (cbTargetMask.u32All     != m_cbTargetMask.u32All)    ||
                (dbShaderControl.u32All  != m_dbShaderControl.u32All) ||
                (dbRenderOverride.u32All != m_dbRenderOverride.u32All))
            {
                m_vgtTfParam       = vgtTfParam;
                m_cbColorControl   = cbColorControl;
                m_paClClipCntl     = paClClipCntl;
                m_paScLineCntl     = paScLineCntl;
                m_cbTargetMask     = cbTargetMask;
                m_dbShaderControl  = dbShaderControl;
                m_dbRenderOverride = dbRenderOverride;
                m_graphicsState.pipelineState.dirtyFlags.dynamicState = 1;
            }
        }
    }

    Pal::Pm4::UniversalCmdBuffer::CmdBindPipeline(params);
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
    Pal::Pm4::UniversalCmdBuffer::CmdBindIndexData(gpuAddr, indexCount, indexType);
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

    const uint32 ctxRegHash = pCurrPipeline->GetContextRegHash();
    const bool ctxRegDirty = wasPrevPipelineNull || (m_pipelineCtxRegHash != ctxRegHash);
    if (ctxRegDirty || m_graphicsState.pipelineState.dirtyFlags.dynamicState)
    {
        if (ctxRegDirty)
        {
            pDeCmdSpace = pCurrPipeline->WriteContextCommands(&m_deCmdStream, pDeCmdSpace);
        }

        if (wasPrevPipelineNull || m_graphicsState.pipelineState.dirtyFlags.dynamicState)
        {
            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(
                mmCB_COLOR_CONTROL, m_cbColorControl.u32All, pDeCmdSpace);

            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(
                mmPA_CL_CLIP_CNTL, m_paClClipCntl.u32All, pDeCmdSpace);

            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(
                mmCB_TARGET_MASK, m_cbTargetMask.u32All, pDeCmdSpace);

            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(
                mmVGT_TF_PARAM, m_vgtTfParam.u32All, pDeCmdSpace);

            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(
                mmPA_SC_LINE_CNTL, m_paScLineCntl.u32All, pDeCmdSpace);

            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(
                mmDB_SHADER_CONTROL, m_dbShaderControl.u32All, pDeCmdSpace);

            pDeCmdSpace = m_deCmdStream.WriteContextRegRmw(mmDB_RENDER_OVERRIDE,
                PipelineDbRenderOverrideMask, m_dbRenderOverride.u32All, pDeCmdSpace);
        }
        m_pipelineCtxRegHash = ctxRegHash;
    }

    if (m_cachedSettings.rbPlusSupported != 0)
    {
        pDeCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmSX_PS_DOWNCONVERT__VI,
                                                           mmSX_BLEND_OPT_CONTROL__VI,
                                                           &m_sxPsDownconvert,
                                                           pDeCmdSpace);
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

    const bool usesViewportArrayIdx     = pCurrPipeline->UsesViewportArrayIndex();
    const bool mvDirty                  = (usesViewportArrayIdx != (m_graphicsState.enableMultiViewport != 0));
    const bool depthClampDirty          =
        (m_depthClampMode != static_cast<DepthClampMode>(m_graphicsState.depthClampMode));
    if (mvDirty || depthClampDirty)
    {
        // If the previously bound pipeline differed in its use of multiple viewports we will need to rewrite the
        // viewport and scissor state on draw.
        if (m_graphicsState.viewportState.count != 0)
        {
            // If viewport is never set, no need to rewrite viewport, this happens in D3D12 nested command list.
            m_graphicsState.dirtyFlags.validationBits.viewports    |=
                mvDirty ||
                (depthClampDirty && (m_depthClampMode != DepthClampMode::_None));
        }
        if (m_graphicsState.scissorRectState.count != 0)
        {
            // If scissor is never set, no need to rewrite scissor, this happens in D3D12 nested command list.
            m_graphicsState.dirtyFlags.validationBits.scissorRects |= mvDirty;
        }

        m_graphicsState.enableMultiViewport = usesViewportArrayIdx;
        m_graphicsState.depthClampMode      = static_cast<uint32>(m_depthClampMode);
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

    m_graphicsState.quadSamplePatternState = quadSamplePattern;
    m_graphicsState.numSamplesPerPixel     = numSamplesPerPixel;

    const MsaaQuadSamplePattern& defaultSamplePattern = GfxDevice::DefaultSamplePattern[Log2(numSamplesPerPixel)];
    m_graphicsState.useCustomSamplePattern =
        (memcmp(&quadSamplePattern, &defaultSamplePattern, sizeof(MsaaQuadSamplePattern)) != 0);

    m_graphicsState.dirtyFlags.validationBits.quadSamplePatternState = 1;

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
void UniversalCmdBuffer::CmdSaveGraphicsState()
{
    Pal::Pm4::UniversalCmdBuffer::CmdSaveGraphicsState();

    CopyColorTargetViewStorage(m_colorTargetViewRestoreStorage, m_colorTargetViewStorage, &m_graphicsRestoreState);
    CopyDepthStencilViewStorage(&m_depthStencilViewRestoreStorage, &m_depthStencilViewStorage, &m_graphicsRestoreState);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdRestoreGraphicsState()
{
    Pal::Pm4::UniversalCmdBuffer::CmdRestoreGraphicsState();

    CopyColorTargetViewStorage(m_colorTargetViewStorage, m_colorTargetViewRestoreStorage, &m_graphicsState);
    CopyDepthStencilViewStorage(&m_depthStencilViewStorage, &m_depthStencilViewRestoreStorage, &m_graphicsState);
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
}

// =====================================================================================================================
// Sets the current input assembly state
void UniversalCmdBuffer::CmdSetInputAssemblyState(
    const InputAssemblyStateParams& params)
{
    m_graphicsState.inputAssemblyState = params;
    m_graphicsState.dirtyFlags.validationBits.inputAssemblyState = 1;

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
        DI_PT_LINELOOP,         // LineLoop
        DI_PT_POLYGON,          // Polygon
    };

    const uint32 idx = static_cast<uint32>(params.topology);
    PAL_ASSERT(idx < ArrayLen(TopologyToPrimTypeTbl));

    regVGT_PRIMITIVE_TYPE vgtPrimitiveType = { };
    vgtPrimitiveType.bits.PRIM_TYPE = TopologyToPrimTypeTbl[idx];

    regVGT_MULTI_PRIM_IB_RESET_EN vgtMultiPrimIbResetEn = { };
    vgtMultiPrimIbResetEn.bits.RESET_EN = params.primitiveRestartEnable;

    regVGT_MULTI_PRIM_IB_RESET_INDX vgtMultiPrimIbResetIndx = { };
    vgtMultiPrimIbResetIndx.bits.RESET_INDX = params.primitiveRestartIndex;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
    pDeCmdSpace = m_deCmdStream.WriteSetVgtPrimitiveType(vgtPrimitiveType, pDeCmdSpace);
    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmVGT_MULTI_PRIM_IB_RESET_EN,
                                                      vgtMultiPrimIbResetEn.u32All,
                                                      pDeCmdSpace);
    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmVGT_MULTI_PRIM_IB_RESET_INDX,
                                                      vgtMultiPrimIbResetIndx.u32All,
                                                      pDeCmdSpace);
    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Sets bit-masks to be applied to stencil buffer reads and writes.
void UniversalCmdBuffer::CmdSetStencilRefMasks(
    const StencilRefMaskParams& params)
{
    if (params.flags.u8All != 0x0)
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
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBarrier(
    const BarrierInfo& barrierInfo)
{
    CmdBuffer::CmdBarrier(barrierInfo);

    // Barriers do not honor predication.
    const uint32 packetPredicate = PacketPredicate();
    m_pm4CmdBufState.flags.packetPredicate = 0;

    bool splitMemAllocated;
    BarrierInfo splitBarrierInfo = barrierInfo;
    Result result = Pal::Device::SplitBarrierTransitions(m_device.GetPlatform(), &splitBarrierInfo, &splitMemAllocated);

    if (result == Result::ErrorOutOfMemory)
    {
        NotifyAllocFailure();
    }
    else if (result == Result::Success)
    {
        m_device.Barrier(this, &m_deCmdStream, splitBarrierInfo);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    // Delete memory allocated for splitting the BarrierTransitions if necessary.
    if (splitMemAllocated)
    {
        PAL_SAFE_DELETE_ARRAY(splitBarrierInfo.pTransitions, m_device.GetPlatform());
    }

    m_pm4CmdBufState.flags.packetPredicate = packetPredicate;
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
    if ((DwordsPerSrd * firstBuffer) < m_vbTable.watermark)
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
    constexpr uint32 AllColorTargetSlotMask = 255; // Mask of all color target slots.

    Pm4::TargetExtent2d surfaceExtent = { Pm4::MaxScissorExtent, Pm4::MaxScissorExtent }; // Default to fully open

    // Bind all color targets.
    uint32 newColorTargetMask = 0;
    for (uint32 slot = 0; slot < params.colorTargetCount; slot++)
    {
        const auto*const pNewView = static_cast<const ColorTargetView*>(params.colorTargets[slot].pColorTargetView);
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

            if (m_device.WaMiscDccOverwriteComb())
            {
                m_workaroundState.ClearDccOverwriteCombinerDisable(slot);
            }

            // Each set bit means the corresponding color target slot is being bound to a valid target.
            newColorTargetMask |= (1 << slot);

            validViewFound = true;
        }
    }

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    // Bind NULL for all remaining color target slots.
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

        Pm4::TargetExtent2d depthViewExtent = pNewDepthView->GetExtent();
        surfaceExtent.width  = Util::Min(surfaceExtent.width,  depthViewExtent.width);
        surfaceExtent.height = Util::Min(surfaceExtent.height, depthViewExtent.height);

        // Re-write the ZRANGE_PRECISION value for the waTcCompatZRange workaround. We must include the COND_EXEC which
        // checks the metadata because we don't know the last fast clear value here.
        pDeCmdSpace = pNewDepthView->UpdateZRangePrecision(true, &m_deCmdStream, pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = WriteNullDepthTarget(pDeCmdSpace);
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
    uint32 colorTargetLimit = Max(params.colorTargetCount, m_graphicsState.bindTargets.colorTargetCount);
    uint32 updatedColorTargetCount = 0;
    for (uint32 slot = 0; slot < colorTargetLimit; slot++)
    {
        if ((slot < params.colorTargetCount) && (params.colorTargets[slot].pColorTargetView != nullptr))
        {
            m_graphicsState.bindTargets.colorTargets[slot].imageLayout      = params.colorTargets[slot].imageLayout;
            m_graphicsState.bindTargets.colorTargets[slot].pColorTargetView =
                PAL_PLACEMENT_NEW(&m_colorTargetViewStorage[slot])
                ColorTargetView(*static_cast<const ColorTargetView*>(params.colorTargets[slot].pColorTargetView));

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

    if (pNewDepthView != nullptr)
    {
        m_graphicsState.bindTargets.depthTarget.pDepthStencilView = PAL_PLACEMENT_NEW(&m_depthStencilViewStorage)
            DepthStencilView(*static_cast<const DepthStencilView*>(pNewDepthView));
    }
    else
    {
        m_graphicsState.bindTargets.depthTarget.pDepthStencilView = nullptr;
    }

    m_graphicsState.dirtyFlags.validationBits.colorTargetView  = 1;
    m_graphicsState.dirtyFlags.validationBits.depthStencilView = 1;
    PAL_ASSERT(m_graphicsState.inheritedState.stateFlags.targetViewState == 0);
}

// =====================================================================================================================
void UniversalCmdBuffer::CopyColorTargetViewStorage(
    ViewStorage<ColorTargetView>*       pStorageDst,
    const ViewStorage<ColorTargetView>* pStorageSrc,
    Pm4::GraphicsState*                 pGraphicsStateDst)
{
    for (uint32 slot = 0; slot < pGraphicsStateDst->bindTargets.colorTargetCount; ++slot)
    {
        if (pGraphicsStateDst->bindTargets.colorTargets[slot].pColorTargetView != nullptr)
        {
            pGraphicsStateDst->bindTargets.colorTargets[slot].pColorTargetView =
                PAL_PLACEMENT_NEW(&pStorageDst[slot])
                ColorTargetView(*reinterpret_cast<const ColorTargetView*>(&pStorageSrc[slot]));
        }
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CopyDepthStencilViewStorage(
    ViewStorage<DepthStencilView>*       pStorageDst,
    const ViewStorage<DepthStencilView>* pStorageSrc,
    Pm4::GraphicsState*                  pGraphicsStateDst)
{
    if (pGraphicsStateDst->bindTargets.depthTarget.pDepthStencilView != nullptr)
    {
        pGraphicsStateDst->bindTargets.depthTarget.pDepthStencilView =
            PAL_PLACEMENT_NEW(pStorageDst) DepthStencilView(*reinterpret_cast<const DepthStencilView*>(pStorageSrc));
    }
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

    // The stream-out table is being managed by the CPU through embedded-data, just mark it dirty since we
    // need to update the whole table at Draw-time anyway.
    m_streamOut.state.dirty = 1;

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
    m_state.flags.optimizeLinearGfxCpy                            = optimizeLinearDestGfxCopy;
    m_graphicsState.triangleRasterState                           = params;
    m_graphicsState.dirtyFlags.validationBits.triangleRasterState = 1;

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

    RgpMarkerSubQueueFlags subQueueFlags { };
    subQueueFlags.includeMainSubQueue = 1;

    m_device.DescribeDraw(this, subQueueFlags, cmdType, firstVertexIdx, startInstanceIdx, drawIndexIdx);
}

// =====================================================================================================================
// Issues a non-indexed draw command. We must discard the draw if vertexCount or instanceCount are zero. To avoid
// branching, we will rely on the HW to discard the draw for us with the exception of the zero instanceCount case on
// pre-gfx8 because that HW treats zero instances as one instance.
template <GfxIpLevel gfxLevel, bool issueSqttMarkerEvent, bool viewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDraw(
    ICmdBuffer* pCmdBuffer,
    uint32      firstVertex,
    uint32      vertexCount,
    uint32      firstInstance,
    uint32      instanceCount,
    uint32      drawId)
{
    if ((gfxLevel >= GfxIpLevel::GfxIp8) || (instanceCount > 0))
    {
        auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

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
template <GfxIpLevel gfxLevel, bool issueSqttMarkerEvent, bool viewInstancingEnable, bool DescribeDrawDispatch>
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

        if (pThis->m_device.Parent()->ChipProperties().gfx6.supportLoadRegIndexPkt)
        {
            // COPY_DATA won't store register value to shadow memory. In order to rightly save-restore,
            // BufferFilledSize should be copy to shadow-memory before programming to register.
            // Otherwise wrong register value will be restored once mid-Cmd-preemption(enabled on gfx8+)
            // happened after COPY_DATA to register command. LoadContextRegsIndex can help us copy data
            // into shadow-memory implicitly.

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
        }
        else
        {
            // Streamout filled is saved in gpuMemory, we use a me_copy to set mmVGT_STRMOUT_DRAW_OPAQUE_BUFFER_FILLED_SIZE.
            pDeCmdSpace += pThis->m_cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_MEM_MAPPED_REG_DC,
                                                      mmVGT_STRMOUT_DRAW_OPAQUE_BUFFER_FILLED_SIZE,
                                                      COPY_DATA_SEL_SRC_MEMORY,
                                                      streamOutFilledSizeVa,
                                                      COPY_DATA_SEL_COUNT_1DW,
                                                      COPY_DATA_ENGINE_ME,
                                                      COPY_DATA_WR_CONFIRM_WAIT,
                                                      pDeCmdSpace);
        }

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
template <GfxIpLevel gfxLevel, bool issueSqttMarkerEvent, bool viewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawIndexed(
    ICmdBuffer* pCmdBuffer,
    uint32      firstIndex,
    uint32      indexCount,
    int32       vertexOffset,
    uint32      firstInstance,
    uint32      instanceCount,
    uint32      drawId)
{
    if ((gfxLevel >= GfxIpLevel::GfxIp8) || (instanceCount > 0))
    {
        auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

        // The "validIndexCount" (set later in the code) will eventually be used to program the max_size
        // field in the draw packet, which is used to clamp how much of the index buffer can be read.
        //
        // If the firstIndex parameter of the draw command is greater than the currently IB's indexCount,
        // the validIndexCount will underflow and end up way too big.
        firstIndex = (firstIndex > pThis->m_graphicsState.iaState.indexCount) ?
                     pThis->m_graphicsState.iaState.indexCount : firstIndex;

        PAL_ASSERT(firstIndex <= pThis->m_graphicsState.iaState.indexCount);

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
template <GfxIpLevel gfxLevel, bool issueSqttMarkerEvent, bool viewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawIndirectMulti(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)) && IsPow2Aligned(countGpuAddr, sizeof(uint32)));
    PAL_ASSERT((countGpuAddr != 0) ||
               (offset + (sizeof(DrawIndirectArgs) * maximumCount) <= gpuMemory.Desc().size));

    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

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

    pDeCmdSpace = pThis->m_deCmdStream.WriteSetBase(
        ShaderGraphics, BASE_INDEX_DRAW_INDIRECT, gpuMemory.Desc().gpuVirtAddr, pDeCmdSpace);

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
template <GfxIpLevel gfxLevel, bool issueSqttMarkerEvent, bool viewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawIndexedIndirectMulti(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)) && IsPow2Aligned(countGpuAddr, sizeof(uint32)));
    PAL_ASSERT((countGpuAddr != 0) ||
               (offset + (sizeof(DrawIndexedIndirectArgs) * maximumCount) <= gpuMemory.Desc().size));

    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

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

    pDeCmdSpace = pThis->m_deCmdStream.WriteSetBase(
        ShaderGraphics, BASE_INDEX_DRAW_INDIRECT, gpuMemory.Desc().gpuVirtAddr, pDeCmdSpace);

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
template <bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatch(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    if (DescribeDrawDispatch)
    {
        pThis->DescribeDispatch(Developer::DrawDispatchType::CmdDispatch, size);
    }

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace  = pThis->ValidateDispatch(0uLL, size, pDeCmdSpace);
    pDeCmdSpace  = pThis->WaitOnCeCounter(pDeCmdSpace);
    pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect(size, false, true, pThis->PacketPredicate(), pDeCmdSpace);

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
template <bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchIndirect(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset)
{
    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)));
    PAL_ASSERT(offset + sizeof(DispatchIndirectArgs) <= gpuMemory.Desc().size);

    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    if (DescribeDrawDispatch)
    {
        pThis->DescribeDispatchIndirect();
    }

    const gpusize gpuMemBaseAddr = gpuMemory.Desc().gpuVirtAddr;

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace  = pThis->ValidateDispatch((gpuMemBaseAddr + offset), {}, pDeCmdSpace);
    pDeCmdSpace  = pThis->m_deCmdStream.WriteSetBase(
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
template <bool IssueSqttMarkerEvent, bool DescribeDrawDispatch>
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

    uint32* pDeCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pDeCmdSpace = pThis->ValidateDispatch(0uLL, logicalSize, pDeCmdSpace);
    pDeCmdSpace = pThis->m_deCmdStream.WriteSetSeqShRegs(mmCOMPUTE_START_X,
                                                         mmCOMPUTE_START_Z,
                                                         ShaderCompute,
                                                         &offset,
                                                         pDeCmdSpace);

    pDeCmdSpace = pThis->WaitOnCeCounter(pDeCmdSpace);

    // The dispatch packet's size is an end position instead of the number of threadgroups to execute.
    pDeCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect(offset + launchSize,
                                                        false,
                                                        false,
                                                        pThis->PacketPredicate(),
                                                        pDeCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pDeCmdSpace += pThis->m_cmdUtil.BuildEventWrite(THREAD_TRACE_MARKER, pDeCmdSpace);
    }

    pDeCmdSpace = pThis->IncrementDeCounter(pDeCmdSpace);

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
    WriteDataInfo    writeData  = {};

    writeData.dstAddr   = pGpuMemory->GetBusAddrMarkerVa() + offset;
    writeData.engineSel = WRITE_DATA_ENGINE_ME;
    writeData.dstSel    = WRITE_DATA_DST_SEL_MEMORY_ASYNC;

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
// Issues an end-of-pipe timestamp event or immediately copies the current time at the ME. Writes the results to the
// pMemObject + destOffset.
void UniversalCmdBuffer::CmdWriteTimestamp(
    HwPipePoint       pipePoint,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    const gpusize address = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    if (pipePoint == HwPipePostPrefetch)
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
// Writes an immediate value during top-of-pipe or bottom-of-pipe event or after indirect arguments and index buffer
// data have been fetched.
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
                                               COPY_DATA_ENGINE_PFP,
                                               COPY_DATA_WR_CONFIRM_WAIT,
                                               pDeCmdSpace);
    }
    else if (pipePoint == HwPipePostPrefetch)
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
        PAL_ASSERT((pipelineBindPoint == PipelineBindPoint::Compute) ||
                   (pipelineBindPoint == PipelineBindPoint::Graphics));

        const auto*const pNewPalette = static_cast<const BorderColorPalette*>(pPalette);
        if (pNewPalette != nullptr)
        {
            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
            pDeCmdSpace = pNewPalette->WriteCommands(pipelineBindPoint,
                                                     TimestampGpuVirtAddr(),
                                                     &m_deCmdStream,
                                                     pDeCmdSpace);
            m_deCmdStream.CommitCommands(pDeCmdSpace);
        }

        auto*const pPipelineState = (pipelineBindPoint == PipelineBindPoint::Compute) ? &m_computeState.pipelineState
                                                                                      : &m_graphicsState.pipelineState;
        pPipelineState->pBorderColorPalette = pNewPalette;
        pPipelineState->dirtyFlags.borderColorPalette = 1;
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
    RgpMarkerSubQueueFlags subQueueFlags,
    uint32                 numDwords,
    const void*            pData)
{
    PAL_ASSERT((subQueueFlags.includeMainSubQueue == 1) && (subQueueFlags.includeGangedSubQueues == 0));

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
    const struct
    {
        regDB_Z_INFO               dbZInfo;
        regDB_STENCIL_INFO         dbStencilInfo;
        regDB_Z_READ_BASE          dbZReadBase;
        regDB_STENCIL_READ_BASE    dbStencilReadBase;
        regDB_Z_WRITE_BASE         dbZWriteBase;
        regDB_STENCIL_WRITE_BASE   dbStencilWriteBase;
    } regs = { };

    const regDB_HTILE_DATA_BASE dbHtileDataBase = { };
    const regDB_RENDER_CONTROL  dbRenderControl = { };

    // If the dbRenderControl.DEPTH_CLEAR_ENABLE bit is not reset to 0 after performing a graphics fast depth clear
    // then any following draw call with pixel shader z-imports will have their z components clamped to the clear
    // plane equation which was set in the fast clear.
    //
    //     [dbRenderControl.]DEPTH_CLEAR_ENABLE will modify the zplane of the incoming geometry to the clear plane.
    //     So if the shader uses this z plane (that is, z-imports are enabled), this can affect the color output.

    pCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmDB_Z_INFO, mmDB_STENCIL_WRITE_BASE, &regs, pCmdSpace);
    pCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmDB_HTILE_DATA_BASE, dbHtileDataBase.u32All, pCmdSpace);
    return m_deCmdStream.WriteSetOneContextReg(mmDB_RENDER_CONTROL, dbRenderControl.u32All, pCmdSpace);
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
    regCB_COLOR0_INFO cbColorInfo = { };
    cbColorInfo.bits.FORMAT = COLOR_INVALID;

    // Compute a mask of slots which were previously bound to valid targets, but are now being bound to NULL.
    uint32 newNullSlotMask = (oldColorTargetMask & ~newColorTargetMask);
    for (uint32 slot : BitIter32(newNullSlotMask))
    {
        pCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmCB_COLOR0_INFO + (slot * CbRegsPerSlot),
                                                        cbColorInfo.u32All,
                                                        pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
//  Validates and writes tessellation distribution factors
uint32* UniversalCmdBuffer::WriteTessDistributionFactors(
    uint32*           pDeCmdSpace,
    GpuChipProperties chipProps)
{
    // Confirm equivalence b/w the two unions assuming each bitfield compared is the same size (8, 8, 8, 5, and 3 bits).
    constexpr regVGT_TESS_DISTRIBUTION__VI RegCheck    = { 255, 255, 255, 31, 7 };
    constexpr TessDistributionFactors      StructCheck = { 255, 255, 255, 31, 7 };
    static_assert((RegCheck.bitfields.ACCUM_ISOLINE == StructCheck.isoDistributionFactor),
                  "ACCUM_ISOLINE and isoDistributionFactor do not match!");
    static_assert((RegCheck.bitfields.ACCUM_TRI == StructCheck.triDistributionFactor),
                  "ACCUM_TRI and triDistributionFactor do not match!");
    static_assert((RegCheck.bitfields.ACCUM_QUAD == StructCheck.quadDistributionFactor),
                  "ACCUM_QUAD and quadDistributionFactor do not match!");
    static_assert((RegCheck.bitfields.DONUT_SPLIT == StructCheck.donutDistributionFactor),
                  "DONUT_SPLIT and donutDistributionFactor do not match!");
    static_assert((RegCheck.bitfields.TRAP_SPLIT == StructCheck.trapDistributionFactor),
                  "TRAP_SPLIT and trapDistributionFactor do not match!");
    static_assert((sizeof(RegCheck) == sizeof(StructCheck)),
                  "TessDistributionFactors and regVGT_TESS_DISTRIBUTION sizes do not match!");

    // Distributed tessellation mode is only supported on Gfx8+ hardware with two or more shader engines, and when
    // off-chip tessellation is enabled.
    if ((chipProps.gfx6.numShaderEngines == 1) || (m_device.Settings().numOffchipLdsBuffers == 0))
    {
        m_tessDistributionFactors.isoDistributionFactor   = 0;
        m_tessDistributionFactors.triDistributionFactor   = 0;
        m_tessDistributionFactors.quadDistributionFactor  = 0;
        m_tessDistributionFactors.donutDistributionFactor = 0;
    }

    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmVGT_TESS_DISTRIBUTION__VI,
                                                      m_tessDistributionFactors.u32All,
                                                      pDeCmdSpace);

    return pDeCmdSpace;
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

    // Track the state of the fields owned by the graphics pipeline.
    PAL_ASSERT((dbRenderOverride.u32All & PipelineDbRenderOverrideMask) == 0);
    m_dbRenderOverride.u32All = 0;

    pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmDB_RENDER_OVERRIDE, dbRenderOverride.u32All, pDeCmdSpace);

    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    // PA_SC_RASTER_CONFIG and PA_SC_RASTER_CONFIG_1 values are given to us by the KMD.
    regPA_SC_RASTER_CONFIG           paScRasterConfig  = {};
    regPA_SC_RASTER_CONFIG_1__CI__VI paScRasterConfig1 = {};
    paScRasterConfig.u32All  = chipProps.gfx6.paScRasterCfg;
    paScRasterConfig1.u32All = chipProps.gfx6.paScRasterCfg1;

    if (chipProps.gfxLevel == GfxIpLevel::GfxIp6)
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

    if (chipProps.gfxLevel >= GfxIpLevel::GfxIp8)
    {
        // Set patch and donut distribution thresholds for tessellation.
        pDeCmdSpace = WriteTessDistributionFactors(pDeCmdSpace, chipProps);
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

    if (IsNested() == false)
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

    if (m_pm4CmdBufState.flags.cpBltActive)
    {
        // Stalls the CP ME until the CP's DMA engine has finished all previous "CP blts" (CP_DMA/DMA_DATA commands
        // without the sync bit set). The ring won't wait for CP DMAs to finish so we need to do this manually.
        pDeCmdSpace += m_cmdUtil.BuildWaitDmaData(pDeCmdSpace);
        SetPm4CmdBufCpBltState(false);
    }

    bool didWaitForIdle = false;

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

        // We also need a wait-for-idle before the atomic increment because command memory might be read or written
        // by draws or dispatches. If we don't wait for idle then the driver might reset and write over that memory
        // before the shaders are done executing.
        didWaitForIdle = true;
        pDeCmdSpace += m_cmdUtil.BuildWaitOnGenericEopEvent(BOTTOM_OF_PIPE_TS,
                                                            TimestampGpuVirtAddr(),
                                                            false,
                                                            pDeCmdSpace);

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
        // If we didn't have a CE tracker we still need this wait-for-idle. See the comment above for the reason.
        if (didWaitForIdle == false)
        {
            pDeCmdSpace += m_cmdUtil.BuildWaitOnGenericEopEvent(BOTTOM_OF_PIPE_TS,
                                                                TimestampGpuVirtAddr(),
                                                                false,
                                                                pDeCmdSpace);
        }

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

    if ((pipePoint >= HwPipePostBlt) && (m_pm4CmdBufState.flags.cpBltActive))
    {
        // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
        // the CmdSetEvent and CmdResetEvent functions expect that the prior blts have reached the post-blt stage by
        // the time the event is written to memory. Given that our CP DMA blts are asynchronous to the pipeline stages
        // the only way to satisfy this requirement is to force the MEC to stall until the CP DMAs are completed.
        pDeCmdSpace += m_cmdUtil.BuildWaitDmaData(pDeCmdSpace);
        SetPm4CmdBufCpBltState(false);
    }

    OptimizePipePoint(&pipePoint);

    if ((pipePoint == HwPipeTop) ||
        (pipePoint == HwPipePostPrefetch))
    {
        WriteDataInfo writeData = {};

        // Implement set/reset event with a WRITE_DATA command using PFP or ME engine.
        writeData.dstAddr   = boundMemObj.GpuVirtAddr();
        writeData.engineSel = (pipePoint == HwPipeTop) ? WRITE_DATA_ENGINE_PFP : WRITE_DATA_ENGINE_ME;
        writeData.dstSel    = WRITE_DATA_DST_SEL_MEMORY_ASYNC;

        pDeCmdSpace += m_cmdUtil.BuildWriteData(writeData, data, pDeCmdSpace);
    }
    else if ((pipePoint == HwPipePostCs) || (pipePoint == HwPipePostPs))
    {
        PAL_ASSERT((pipePoint != HwPipePostCs) || IsComputeSupported());

        // Implement set/reset with an EOS event waiting for PS/VS waves to complete.
        pDeCmdSpace += m_cmdUtil.BuildEventWriteEos((pipePoint == HwPipePostCs) ? CS_DONE : PS_DONE,
                                                    boundMemObj.GpuVirtAddr(),
                                                    EVENT_WRITE_EOS_CMD_STORE_32BIT_DATA_TO_MEMORY,
                                                    data,
                                                    0,
                                                    0,
                                                    pDeCmdSpace);
    }
    else if ((pipePoint == HwPipeBottom) || (pipePoint == HwPipePreRasterization))
    {
        // Implement set/reset with an EOP event written when all prior GPU work completes or VS waves to complete
        // since there is no VS_DONE event.
        pDeCmdSpace += m_cmdUtil.BuildEventWriteEop(BOTTOM_OF_PIPE_TS,
                                                    boundMemObj.GpuVirtAddr(),
                                                    EVENTWRITEEOP_DATA_SEL_SEND_DATA32,
                                                    data,
                                                    false,
                                                    pDeCmdSpace);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
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
            pDeCmdSpace =
                m_deCmdStream.WriteUserDataEntriesToSgprs<true, ShaderGraphics>(m_pSignatureGfx->stage[LsStageId],
                                                                                m_graphicsState.gfxUserDataEntries,
                                                                                pDeCmdSpace);
        }
        if (m_pSignatureGfx->userDataHash[HsStageId] != pPrevSignature->userDataHash[HsStageId])
        {
            changedStageMask |= (1 << HsStageId);
            pDeCmdSpace =
                m_deCmdStream.WriteUserDataEntriesToSgprs<true, ShaderGraphics>(m_pSignatureGfx->stage[HsStageId],
                                                                                m_graphicsState.gfxUserDataEntries,
                                                                                pDeCmdSpace);
        }
    }
    if (GsEnabled)
    {
        if (m_pSignatureGfx->userDataHash[EsStageId] != pPrevSignature->userDataHash[EsStageId])
        {
            changedStageMask |= (1 << EsStageId);
            pDeCmdSpace =
                m_deCmdStream.WriteUserDataEntriesToSgprs<true, ShaderGraphics>(m_pSignatureGfx->stage[EsStageId],
                                                                                m_graphicsState.gfxUserDataEntries,
                                                                                pDeCmdSpace);
        }
        if (m_pSignatureGfx->userDataHash[GsStageId] != pPrevSignature->userDataHash[GsStageId])
        {
            changedStageMask |= (1 << GsStageId);
            pDeCmdSpace =
                m_deCmdStream.WriteUserDataEntriesToSgprs<true, ShaderGraphics>(m_pSignatureGfx->stage[GsStageId],
                                                                                m_graphicsState.gfxUserDataEntries,
                                                                                pDeCmdSpace);
        }
    }
    if (m_pSignatureGfx->userDataHash[VsStageId] != pPrevSignature->userDataHash[VsStageId])
    {
        changedStageMask |= (1 << VsStageId);
        pDeCmdSpace =
            m_deCmdStream.WriteUserDataEntriesToSgprs<true, ShaderGraphics>(m_pSignatureGfx->stage[VsStageId],
                                                                            m_graphicsState.gfxUserDataEntries,
                                                                            pDeCmdSpace);
    }
    if (m_pSignatureGfx->userDataHash[PsStageId] != pPrevSignature->userDataHash[PsStageId])
    {
        changedStageMask |= (1 << PsStageId);
        pDeCmdSpace =
            m_deCmdStream.WriteUserDataEntriesToSgprs<true, ShaderGraphics>(m_pSignatureGfx->stage[PsStageId],
                                                                            m_graphicsState.gfxUserDataEntries,
                                                                            pDeCmdSpace);
    }

    (*ppDeCmdSpace) = pDeCmdSpace;

    return changedStageMask;
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
    constexpr uint8 ActiveStageMask = ((TessEnabled ? ((1 << LsStageId) | (1 << HsStageId)) : 0) |
                                       (GsEnabled   ? ((1 << EsStageId) | (1 << GsStageId)) : 0) |
                                       (1 << VsStageId) | (1 << PsStageId));
    const uint8 dirtyStageMask  = ((~alreadyWrittenStageMask) & ActiveStageMask);
    if (dirtyStageMask)
    {
        if (TessEnabled)
        {
            if (dirtyStageMask & (1 << LsStageId))
            {
                pDeCmdSpace =
                    m_deCmdStream.WriteUserDataEntriesToSgprs<false, ShaderGraphics>(m_pSignatureGfx->stage[LsStageId],
                                                                                     m_graphicsState.gfxUserDataEntries,
                                                                                     pDeCmdSpace);
            }
            if (dirtyStageMask & (1 << HsStageId))
            {
                pDeCmdSpace =
                    m_deCmdStream.WriteUserDataEntriesToSgprs<false, ShaderGraphics>(m_pSignatureGfx->stage[HsStageId],
                                                                                     m_graphicsState.gfxUserDataEntries,
                                                                                     pDeCmdSpace);
            }
        }
        if (GsEnabled)
        {
            if (dirtyStageMask & (1 << EsStageId))
            {
                pDeCmdSpace =
                    m_deCmdStream.WriteUserDataEntriesToSgprs<false, ShaderGraphics>(m_pSignatureGfx->stage[EsStageId],
                                                                                     m_graphicsState.gfxUserDataEntries,
                                                                                     pDeCmdSpace);
            }
            if (dirtyStageMask & (1 << GsStageId))
            {
                pDeCmdSpace =
                    m_deCmdStream.WriteUserDataEntriesToSgprs<false, ShaderGraphics>(m_pSignatureGfx->stage[GsStageId],
                                                                                     m_graphicsState.gfxUserDataEntries,
                                                                                     pDeCmdSpace);
            }
        }
        if (dirtyStageMask & (1 << VsStageId))
        {
            pDeCmdSpace =
                m_deCmdStream.WriteUserDataEntriesToSgprs<false, ShaderGraphics>(m_pSignatureGfx->stage[VsStageId],
                                                                                 m_graphicsState.gfxUserDataEntries,
                                                                                 pDeCmdSpace);
        }
        if (dirtyStageMask & (1 << PsStageId))
        {
            pDeCmdSpace =
                m_deCmdStream.WriteUserDataEntriesToSgprs<false, ShaderGraphics>(m_pSignatureGfx->stage[PsStageId],
                                                                                 m_graphicsState.gfxUserDataEntries,
                                                                                 pDeCmdSpace);
        }
    } // if any stages still need dirty state processing

    return pDeCmdSpace;
}

// =====================================================================================================================
// Helper function responsible for handling user-SGPR updates during Dispatch-time validation when the active pipeline
// has changed since the previous Dispatch operation.  It is expected that this will be called only when the pipeline
// is changing and immediately before a call to WriteUserDataEntriesToSgprs<false, ..>().
bool UniversalCmdBuffer::FixupUserSgprsOnPipelineSwitchCs(
    const ComputePipelineSignature* pPrevSignature,
    uint32**                        ppDeCmdSpace)
{
    PAL_ASSERT(pPrevSignature != nullptr);

    // The WriteUserDataEntriesToSgprs() method writes all entries which are mapped to user-SGPR's.
    // When the active pipeline is changing, the set of entries mapped to user-SGPR's have been changed
    // and which entries are mapped to which registers can also change.  The simplest way to handle
    // this is to write all mapped user-SGPR's whose mappings are changing.
    // These functions are only called when the pipeline has changed.

    bool written = false;
    uint32* pDeCmdSpace = (*ppDeCmdSpace);

    if (m_pSignatureCs->userDataHash != pPrevSignature->userDataHash)
    {
        pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprs<true, ShaderCompute>(m_pSignatureCs->stage,
                                                                                     m_computeState.csUserDataEntries,
                                                                                     pDeCmdSpace);

        written = true;
        (*ppDeCmdSpace) = pDeCmdSpace;
    }
    return written;
}

// =====================================================================================================================
// Helper function which is responsible for making sure all user-data entries are written to either the spill table or
// to user-SGPR's, as well as making sure that all indirect user-data tables are up-to-date in GPU memory.  Part of
// Draw-time validation.  This version uses the CPU & embedded data for user-data table management.
template <bool HasPipelineChanged, bool TessEnabled, bool GsEnabled>
uint32* UniversalCmdBuffer::ValidateGraphicsUserData(
    const GraphicsPipelineSignature* pPrevSignature,
    uint32*                          pDeCmdSpace)
{
    PAL_ASSERT((HasPipelineChanged  && (pPrevSignature != nullptr)) ||
               (!HasPipelineChanged && (pPrevSignature == nullptr)));

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
            constexpr uint32 StreamOutTableDwords = (sizeof(m_streamOut.srd) / sizeof(uint32));
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

    bool         reUpload         = false;
    const uint16 spillThreshold   = m_pSignatureGfx->spillThreshold;
    const bool   anyUserDataDirty = IsAnyGfxUserDataDirty();

    if (anyUserDataDirty)
    {
        pDeCmdSpace = WriteDirtyUserDataEntriesToSgprsGfx<TessEnabled, GsEnabled>(pPrevSignature,
                                                                                  alreadyWrittenStageMask,
                                                                                  pDeCmdSpace);
    }

    if (HasPipelineChanged || anyUserDataDirty)
    {
        if (spillThreshold != NoUserDataSpilling)
        {
            const uint16 userDataLimit = m_pSignatureGfx->userDataLimit;
            PAL_ASSERT(userDataLimit > 0);
            const uint16 lastUserData  = (userDataLimit - 1);

            // Step #3:
            // Because the spill table is managed using CPU writes to embedded data, it must be fully re-uploaded for
            // any Draw/Dispatch whenever *any* contents have changed.
            reUpload = (m_spillTable.stateGfx.dirty != 0);
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

            // NOTE: If the pipeline is changing, we may need to re-write the spill table address to any shader stage,
            // even if the spill table wasn't re-uploaded because the mapped user-SGPRs for the spill table could have
            // changed (as indicated by 'alreadyWrittenStageMask')
            if ((alreadyWrittenStageMask != 0) || reUpload)
            {
                const uint32 gpuVirtAddrLo = LowPart(m_spillTable.stateGfx.gpuVirtAddr);
                for (uint32 s = 0; s < NumHwShaderStagesGfx; ++s)
                {
                    const uint16 userSgpr = m_pSignatureGfx->stage[s].spillTableRegAddr;
                    if (userSgpr != UserDataNotMapped)
                    {
                        pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics>(userSgpr,
                            gpuVirtAddrLo,
                            pDeCmdSpace);
                    }
                }
            }

        } // if current pipeline spills user-data

        // All dirtied user-data entries have been written to user-SGPR's or to the spill table somewhere in this
        // method, so it is safe to clear these bits.
        size_t* pDirtyMask = &m_graphicsState.gfxUserDataEntries.dirty[0];
        for (uint32 i = 0; i < NumUserDataFlagsParts; i++)
        {
            pDirtyMask[i] = 0;
        }
    } // if any user data is dirty or pipeline changed

    // Step #5:
    // Even though the spill table is not being managed using CE RAM, it is possible for the client to use CE RAM for
    // its own purposes.  In this case, we still need to increment the CE RAM counter.
    if (m_state.flags.ceStreamDirty)
    {
        uint32* pCeCmdSpace = m_ceCmdStream.ReserveCommands();
        pCeCmdSpace += m_cmdUtil.BuildIncrementCeCounter(pCeCmdSpace);
        m_ceCmdStream.CommitCommands(pCeCmdSpace);
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Helper function which is responsible for making sure all user-data entries are written to either the spill table or
// to user-SGPR's, as well as making sure that all indirect user-data tables are up-to-date in GPU memory.  Part of
// Dispatch-time validation.  This version uses the CPU & embedded data for user-data table management.
template <bool HasPipelineChanged>
uint32* UniversalCmdBuffer::ValidateComputeUserData(
    const ComputePipelineSignature* pPrevSignature,
    uint32*                         pDeCmdSpace)
{
    PAL_ASSERT((HasPipelineChanged  && (pPrevSignature != nullptr)) ||
               (!HasPipelineChanged && (pPrevSignature == nullptr)));

    // Step #1:
    // Write all dirty user-data entries to their mapped user SGPR's. If the pipeline has changed we must also fixup
    // the dirty bits because the prior compute pipeline could use fewer fast sgprs than the current pipeline.
    bool alreadyWritten = false;
    if (HasPipelineChanged)
    {
        alreadyWritten = FixupUserSgprsOnPipelineSwitchCs(pPrevSignature, &pDeCmdSpace);
    }

    if (alreadyWritten == false)
    {
        pDeCmdSpace = m_deCmdStream.WriteUserDataEntriesToSgprs<false, ShaderCompute>(m_pSignatureCs->stage,
                                                                                      m_computeState.csUserDataEntries,
                                                                                      pDeCmdSpace);
    }

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
    const Pm4::ValidateDrawInfo& drawInfo)
{
    if (m_deCmdStream.Pm4OptimizerEnabled())
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
    const Pm4::ValidateDrawInfo& drawInfo)
{
#if PAL_DEVELOPER_BUILD
    uint32 startingCmdLen = GetUsedSize(CommandDataAlloc);
    uint32 pipelineCmdLen = 0;
    uint32 userDataCmdLen = 0;
#endif

    if (m_graphicsState.pipelineState.dirtyFlags.pipeline || m_graphicsState.pipelineState.dirtyFlags.dynamicState)
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
        }

        const auto*const pPrevSignature = m_pSignatureGfx;
        m_pSignatureGfx                 = &pNewPipeline->Signature();

        pDeCmdSpace = SwitchGraphicsPipeline(pPrevSignature, pNewPipeline, pDeCmdSpace);

        // NOTE: Switching a graphics pipeline can result in a large amount of commands being written, so start a new
        // reserve/commit region before proceeding with validation.
        m_deCmdStream.CommitCommands(pDeCmdSpace);

#if PAL_DEVELOPER_BUILD
        if (m_cachedSettings.enablePm4Instrumentation != 0)
        {
            pipelineCmdLen  = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
            startingCmdLen += pipelineCmdLen;
        }
#endif
        pDeCmdSpace = m_deCmdStream.ReserveCommands();

        pDeCmdSpace = (this->*m_pfnValidateUserDataGfxPipelineSwitch)(pPrevSignature, pDeCmdSpace);

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

        pDeCmdSpace = ValidateDraw<indexed, indirect, pm4OptImmediate, true>(drawInfo, pDeCmdSpace);

        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }
    else
    {
        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

        pDeCmdSpace = (this->*m_pfnValidateUserDataGfx)(nullptr, pDeCmdSpace);

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

        pDeCmdSpace = ValidateDraw<indexed, indirect, pm4OptImmediate, false>(drawInfo, pDeCmdSpace);

        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }

#if PAL_DEVELOPER_BUILD
    if (m_cachedSettings.enablePm4Instrumentation != 0)
    {
        const uint32 miscCmdLen = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
        m_device.DescribeDrawDispatchValidation(this, userDataCmdLen, pipelineCmdLen, miscCmdLen);
    }
#endif
}

// =====================================================================================================================
// Performs draw-time dirty state validation. Returns the next unused DWORD in pDeCmdSpace.  Wrapper to determine
// if any interesting state is dirty before calling the real ValidateDraw() function.
template <bool indexed, bool indirect, bool pm4OptImmediate, bool pipelineDirty>
uint32* UniversalCmdBuffer::ValidateDraw(
    const Pm4::ValidateDrawInfo& drawInfo,
    uint32*                      pDeCmdSpace)
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

    // Strictly speaking, paScModeCntl1 is not similar dirty bits as tracked in validationBits. However for best CPU
    // performance in <PipelineDirty=false, StateDirty=false> path, manually make it as part of StateDirty path as
    // it is not frequently updated.
    const bool stateDirty = ((m_graphicsState.dirtyFlags.validationBits.u32All |
                              (m_drawTimeHwState.valid.paScModeCntl1 == 0)) != 0);

    if (stateDirty)
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
    const Pm4::ValidateDrawInfo& drawInfo,
    uint32*                      pDeCmdSpace)
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
    m_graphicsState.leakFlags.u64All |= m_graphicsState.dirtyFlags.u64All;

    if (pipelineDirty ||
        (stateDirty && (dirtyFlags.depthStencilState || dirtyFlags.msaaState)))
    {
        // NOTE: Due to a hardware workaround, we need to defer writing DB_SHADER_CONTROL until draw-time.
        const bool depthEnabled          = ((pDepthState != nullptr) && pDepthState->IsDepthEnabled());
        const bool usesOverRasterization = ((pMsaaState  != nullptr) && pMsaaState->UsesOverRasterization());

        pDeCmdSpace = WriteDbShaderControl(depthEnabled,
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

    if (stateDirty && dirtyFlags.triangleRasterState)
    {
        pDeCmdSpace = ValidateTriangleRasterState(pDeCmdSpace);
    }

    regPA_SC_MODE_CNTL_1 paScModeCntl1 = m_drawTimeHwState.paScModeCntl1;

    // Re-calculate paScModeCntl1 value if state constributing to the register has changed.
    if (pipelineDirty ||
        (stateDirty && (dirtyFlags.depthStencilState || dirtyFlags.colorBlendState || dirtyFlags.depthStencilView ||
                        dirtyFlags.occlusionQueryActive || dirtyFlags.triangleRasterState ||
                        (m_drawTimeHwState.valid.paScModeCntl1 == 0))))
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

    if (stateDirty && (dirtyFlags.msaaState || dirtyFlags.occlusionQueryActive))
    {
        // MSAA sample rates are associated with the MSAA state object, but the sample rate affects how queries are
        // processed (via DB_COUNT_CONTROL). We need to update the value of this register at draw-time since it is
        // affected by multiple elements of command-buffer state.
        const uint32 log2OcclusionQuerySamples = (pMsaaState != nullptr) ? pMsaaState->Log2OcclusionQuerySamples() : 0;
        pDeCmdSpace = UpdateDbCountControl<pm4OptImmediate>(log2OcclusionQuerySamples, pDeCmdSpace);
    }

    // Before we do per-draw HW state validation we need to get a copy of the current IA_MULTI_VGT_PARAM register. This
    // is also where we do things like force WdSwitchOnEop and optimize the primgroup size.
    const bool            wdSwitchOnEop   = ForceWdSwitchOnEop(*pPipeline, drawInfo);
    regIA_MULTI_VGT_PARAM iaMultiVgtParam = pPipeline->IaMultiVgtParam(wdSwitchOnEop);
    regVGT_LS_HS_CONFIG   vgtLsHsConfig   = pPipeline->VgtLsHsConfig();

#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION>= 747)
    PAL_ASSERT(pPipeline->IsTessEnabled() ||
               (vgtLsHsConfig.bits.HS_NUM_INPUT_CP == m_graphicsState.inputAssemblyState.patchControlPoints));
#endif
    if (m_primGroupOpt.optimalSize > 0)
    {
        iaMultiVgtParam.bits.PRIMGROUP_SIZE = m_primGroupOpt.optimalSize - 1;
    }

    const bool lineStippleStateDirty = stateDirty && (dirtyFlags.lineStippleState || dirtyFlags.inputAssemblyState);
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

    if (pipelineDirty || lineStippleStateDirty)
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
        if (paSuLineStippleCntl.u32All != m_paSuLineStippleCntl.u32All)
        {
            pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmPA_SU_LINE_STIPPLE_CNTL,
                                                                   paSuLineStippleCntl.u32All,
                                                                   pDeCmdSpace);
            m_paSuLineStippleCntl = paSuLineStippleCntl;
        }
    }

    // Validate the per-draw HW state.
    pDeCmdSpace = ValidateDrawTimeHwState<indexed, indirect, pm4OptImmediate>(iaMultiVgtParam,
                                                                              vgtLsHsConfig,
                                                                              paScModeCntl1,
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
    m_graphicsState.dirtyFlags.u64All = 0;
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

    return m_deCmdStream.WriteSetSeqContextRegs<pm4OptImmediate>(mmPA_SC_VPORT_ZMIN_0,
                                                                 mmPA_SC_VPORT_ZMIN_0 + numVportZMinMaxRegs - 1,
                                                                 &zMinMaxImg[0],
                                                                 pDeCmdSpace);
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

    return m_deCmdStream.WriteSetSeqContextRegs<pm4OptImmediate>(mmPA_SC_VPORT_SCISSOR_0_TL,
                                                                 mmPA_SC_VPORT_SCISSOR_0_TL + numScissorRectRegs - 1,
                                                                 &scissorRectImg[0],
                                                                 pDeCmdSpace);
}

// =====================================================================================================================
// Wrapper for the real ValidateScissorRects() for when the caller doesn't know if the immediate pm4 optimizer is
// enabled.
uint32* UniversalCmdBuffer::ValidateScissorRects(
    uint32*    pDeCmdSpace)
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
uint32* UniversalCmdBuffer::ValidateTriangleRasterState(
    uint32* pDeCmdSpace)
{
    regPA_SU_SC_MODE_CNTL paSuScModeCntl;
    paSuScModeCntl.u32All = m_paSuScModeCntl.u32All;
    const auto& params    = m_graphicsState.triangleRasterState;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 721
    paSuScModeCntl.bits.POLY_OFFSET_FRONT_ENABLE = params.flags.frontDepthBiasEnable;
    paSuScModeCntl.bits.POLY_OFFSET_BACK_ENABLE  = params.flags.backDepthBiasEnable;
#else
    paSuScModeCntl.bits.POLY_OFFSET_FRONT_ENABLE = params.flags.depthBiasEnable;
    paSuScModeCntl.bits.POLY_OFFSET_BACK_ENABLE  = params.flags.depthBiasEnable;
#endif
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

    paSuScModeCntl.bits.FACE       = static_cast<uint32>(params.frontFace);

    static_assert((static_cast<uint32>(ProvokingVertex::First) == 0) &&
                  (static_cast<uint32>(ProvokingVertex::Last)  == 1),
                  "ProvokingVertex vs. PA_SU_SC_MODE_CNTL.PROVOKING_VTX_LAST mismatch");

    paSuScModeCntl.bits.PROVOKING_VTX_LAST = static_cast<uint32>(params.provokingVertex);

    PAL_ASSERT(paSuScModeCntl.u32All != InvalidPaSuScModeCntlVal);

    if (paSuScModeCntl.u32All != m_paSuScModeCntl.u32All)
    {
        m_paSuScModeCntl.u32All = paSuScModeCntl.u32All;

        pDeCmdSpace = m_deCmdStream.WriteSetOneContextRegNoOpt(mmPA_SU_SC_MODE_CNTL, paSuScModeCntl.u32All, pDeCmdSpace);
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Update the HW state and write the necessary packets to push any changes to the HW. Returns the next unused DWORD
// in pDeCmdSpace.
template <bool indexed, bool indirect, bool pm4OptImmediate>
uint32* UniversalCmdBuffer::ValidateDrawTimeHwState(
    regIA_MULTI_VGT_PARAM        iaMultiVgtParam, // The value of the draw preamble's IA_MULTI_VGT_PARAM register.
    regVGT_LS_HS_CONFIG          vgtLsHsConfig,   // The value of the draw preamble's VGT_LS_HS_CONFIG register.
    regPA_SC_MODE_CNTL_1         paScModeCntl1,   // The value of PA_SC_MODE_CNTL_1 register.
    const Pm4::ValidateDrawInfo& drawInfo,        // Draw info
    uint32*                      pDeCmdSpace)     // Write new draw-engine commands here.
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

    if (m_drawIndexReg != UserDataNotMapped)
    {
        if (indirect && drawInfo.multiIndirectDraw)
        {
            // If the active pipeline uses the draw index VS input value, then the PM4 draw packet to issue the multi
            // draw will blow-away the SPI user-data register used to pass that value to the shader.
            m_drawTimeHwState.valid.drawIndex = 0;
        }
        else if ((m_drawTimeHwState.drawIndex != drawInfo.drawIndex) || (m_drawTimeHwState.valid.drawIndex == 0))
        {
            m_drawTimeHwState.drawIndex = drawInfo.drawIndex;
            m_drawTimeHwState.valid.drawIndex = 1;
            pDeCmdSpace = m_deCmdStream.WriteSetOneShReg<ShaderGraphics>(m_drawIndexReg,
                                                                         drawInfo.drawIndex,
                                                                         pDeCmdSpace);
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
uint32* UniversalCmdBuffer::ValidateDispatch(
    gpusize      indirectGpuVirtAddr,
    DispatchDims logicalSize,
    uint32*      pDeCmdSpace)
{
#if PAL_DEVELOPER_BUILD
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

    if (m_computeState.pipelineState.dirtyFlags.pipeline)
    {
        const auto*const pNewPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);

        pDeCmdSpace = pNewPipeline->WriteCommands(&m_deCmdStream,
                                                  pDeCmdSpace,
                                                  m_computeState.dynamicCsInfo,
                                                  m_buildFlags.prefetchShaders);

#if PAL_DEVELOPER_BUILD
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

        pDeCmdSpace = ValidateComputeUserData<true>(pPrevSignature, pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = ValidateComputeUserData<false>(nullptr, pDeCmdSpace);
    }

#if PAL_DEVELOPER_BUILD
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
            *reinterpret_cast<DispatchDims*>(CmdAllocateEmbeddedData(3, 4, &indirectGpuVirtAddr)) = logicalSize;
        }

        pDeCmdSpace = m_deCmdStream.WriteSetSeqShRegs(m_pSignatureCs->numWorkGroupsRegAddr,
                                                      (m_pSignatureCs->numWorkGroupsRegAddr + 1),
                                                      ShaderCompute,
                                                      &indirectGpuVirtAddr,
                                                      pDeCmdSpace);
    }

#if PAL_DEVELOPER_BUILD
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
    static_cast<const QueryPool&>(queryPool).Begin(this, &m_deCmdStream, nullptr, queryType, slot, flags);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdEndQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot)
{
    static_cast<const QueryPool&>(queryPool).End(this, &m_deCmdStream, nullptr, queryType, slot);
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
    m_pm4CmdBufState.flags.packetPredicate = 0;

    m_device.RsrcProcMgr().CmdResolveQuery(this,
                                           static_cast<const QueryPool&>(queryPool),
                                           flags,
                                           queryType,
                                           startQuery,
                                           queryCount,
                                           static_cast<const GpuMemory&>(dstGpuMemory),
                                           dstOffset,
                                           dstStride);

    m_pm4CmdBufState.flags.packetPredicate = packetPredicate;
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
        m_graphicsState.dirtyFlags.validationBits.occlusionQueryActive = m_state.flags.occlusionQueriesActive;
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
        m_graphicsState.dirtyFlags.validationBits.occlusionQueryActive = (m_state.flags.occlusionQueriesActive == 0);
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
template <bool pm4OptImmediate>
uint32* UniversalCmdBuffer::UpdateDbCountControl(
    uint32               log2SampleRate,  // MSAA sample rate associated with a bound MSAA state object
    uint32*              pDeCmdSpace)
{
    const bool HasActiveQuery = IsQueryActive(QueryPoolType::Occlusion) &&
                                (NumActiveQueries(QueryPoolType::Occlusion) != 0);

    regDB_COUNT_CONTROL dbCountControl              = {0};
    dbCountControl.bits.SAMPLE_RATE                 = log2SampleRate;
    dbCountControl.bits.SLICE_EVEN_ENABLE__CI__VI   = 1;
    dbCountControl.bits.SLICE_ODD_ENABLE__CI__VI    = 1;

    if (IsNested() &&
        m_graphicsState.inheritedState.stateFlags.occlusionQuery &&
        (HasActiveQuery == false))
    {
        // In a nested command buffer, the number of active queries is unknown because the caller may have some
        // number of active queries when executing the nested command buffer. In this case, we must make sure that
        // update the sample count without disabling occlusion queries.
        pDeCmdSpace = m_deCmdStream.WriteContextRegRmw<pm4OptImmediate>(mmDB_COUNT_CONTROL,
                                                                        DB_COUNT_CONTROL__SAMPLE_RATE_MASK,
                                                                        dbCountControl.u32All,
                                                                        pDeCmdSpace);
    }
    else
    {
        if (HasActiveQuery)
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
            dbCountControl.bits.PERFECT_ZPASS_COUNTS    = 1;

            dbCountControl.bits.ZPASS_ENABLE__CI__VI    = 1;
            dbCountControl.bits.ZPASS_INCREMENT_DISABLE = 0;
        }

        pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg<pm4OptImmediate>(mmDB_COUNT_CONTROL,
                                                                           dbCountControl.u32All,
                                                                           pDeCmdSpace);
    }

    m_state.flags.occlusionQueriesActive = HasActiveQuery;

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
        const uint32 patchControlPoints = pipeline.VgtLsHsConfig().bits.HS_NUM_INPUT_CP;
        const uint32 vertsPerPrim  = GfxDevice::VertsPerPrimitive(m_graphicsState.inputAssemblyState.topology,
                                                                  patchControlPoints);
        const uint64 primTotal     = m_primGroupOpt.vtxIdxTotal / vertsPerPrim;
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
    const Pm4::ValidateDrawInfo& drawInfo
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
                        (primTopology == PrimitiveTopology::LineLoop) ||
                        (primTopology == PrimitiveTopology::Polygon) ||
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
        const uint32 patchControlPoints = pipeline.VgtLsHsConfig().bits.HS_NUM_INPUT_CP;
        const uint32 vertsPerPrim = GfxDevice::VertsPerPrimitive(m_graphicsState.inputAssemblyState.topology,
                                                                 patchControlPoints);
        PAL_ASSERT(vertsPerPrim > 0);
        const uint32 primCount  = drawInfo.vtxIdxCount / vertsPerPrim;

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
    const Pm4::GraphicsState& newGraphicsState)
{
    Pal::Pm4::UniversalCmdBuffer::SetGraphicsState(newGraphicsState);

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
        (restoreViewports.depthRange != currentViewports.depthRange) ||
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
    pCeCmdSpace += m_cmdUtil.BuildLoadConstRam(srcGpuMemory.Desc().gpuVirtAddr + memOffset,
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
        pCeCmdSpace += m_cmdUtil.BuildWaitOnDeCounterDiff(m_state.minCounterDiff, pCeCmdSpace);
        m_state.flags.ceWaitOnDeCounterDiff = 0;
    }

    pCeCmdSpace += m_cmdUtil.BuildDumpConstRam(dstGpuMemory.Desc().gpuVirtAddr + memOffset,
                                               ramOffset,
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
    pCeCmdSpace += m_cmdUtil.BuildWriteConstRam(pSrcData, ramOffset, dwordSize, pCeCmdSpace);
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
        pDeCmdSpace += m_cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_ASYNC_MEMORY,
                                               predicateVirtAddr,
                                               COPY_DATA_SEL_SRC_MEMORY,
                                               gpuVirtAddr,
                                               COPY_DATA_SEL_COUNT_1DW,
                                               COPY_DATA_ENGINE_ME,
                                               COPY_DATA_WR_CONFIRM_WAIT,
                                               pDeCmdSpace);
        pDeCmdSpace += m_cmdUtil.BuildPfpSyncMe(pDeCmdSpace);
        gpuVirtAddr = predicateVirtAddr;
        predType    = PredicateType::Boolean64;
    }

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
    // Need to validate some state as it is valid for root CmdBuf to set state, not issue a draw and expect
    // that state to inherit into the nested CmdBuf.
    const auto dirtyFlags  = m_graphicsState.dirtyFlags.validationBits;
    if (dirtyFlags.occlusionQueryActive)
    {
        uint32*    pDeCmdSpace                 = m_deCmdStream.ReserveCommands();
        const auto*const pMsaaState            = static_cast<const MsaaState*>(m_graphicsState.pMsaaState);
        const uint32 log2OcclusionQuerySamples = (pMsaaState != nullptr) ? pMsaaState->Log2OcclusionQuerySamples() : 0;
        pDeCmdSpace                            = UpdateDbCountControl<false>(log2OcclusionQuerySamples, pDeCmdSpace);
        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }

    for (uint32 buf = 0; buf < cmdBufferCount; ++buf)
    {
        auto*const pCallee = static_cast<Gfx6::UniversalCmdBuffer*>(ppCmdBuffers[buf]);
        PAL_ASSERT(pCallee != nullptr);

        // Track the most recent OS paging fence value across all nested command buffers called from this one.
        m_lastPagingFence = Max(m_lastPagingFence, pCallee->LastPagingFence());

        // Track the lastest fence token across all nested command buffers called from this one.
        m_maxUploadFenceToken = Max(m_maxUploadFenceToken, pCallee->GetMaxUploadFenceToken());

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

        if (allowIb2Launch)
        {
            TrackIb2DumpInfoFromExecuteNestedCmds(pCallee->m_deCmdStream);
            TrackIb2DumpInfoFromExecuteNestedCmds(pCallee->m_ceCmdStream);
        }

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
    const PipelineBindPoint bindPoint    = ((gfx6Generator.Type() == Pm4::GeneratorType::Dispatch)
                                           ? PipelineBindPoint::Compute : PipelineBindPoint::Graphics);
    const bool              setViewId    = (bindPoint == PipelineBindPoint::Graphics);
    const auto*const        pGfxPipeline =
        static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    uint32                  mask         = 1;

    AutoBuffer<CmdStreamChunk*, 16, Platform> deChunks(maximumCount, m_device.GetPlatform());

    if (deChunks.Capacity() < maximumCount)
    {
        NotifyAllocFailure();
    }
    else
    {
        CmdStreamChunk** ppChunkList[] =
        {
            deChunks.Data(),
        };
        uint32 numGenChunks = 0;

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
                gfx6Generator,
                m_graphicsState.iaState.indexCount,
                maximumCount,
                (gpuMemory.Desc().gpuVirtAddr + offset),
                countGpuAddr
            };

            m_device.RsrcProcMgr().CmdGenerateIndirectCmds(genInfo, &ppChunkList[0], 1, &numGenChunks);

            m_pm4CmdBufState.flags.packetPredicate = packetPredicate;

            uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

            // Insert a CS_PARTIAL_FLUSH and invalidate/flush the texture caches to make sure that the generated
            // commands are written out to memory before we attempt to execute them. Then, a PFP_SYNC_ME is also
            // required so that the PFP doesn't prefetch the generated commands before they are finished executing.
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

            // Just like a normal direct/indirect draw/dispatch, we need to perform state validation before executing
            // the generated command chunks.
            if (bindPoint == PipelineBindPoint::Graphics)
            {
                // NOTE: If we tell ValidateDraw() that this draw call is indexed, it will validate all of the
                // draw-time HW state related to the index buffer. However, since some indirect command generators
                // can genrate the commands to bind their own index buffer state, our draw-time validation could be
                // redundant. Therefore, pretend this is a non-indexed draw call if the generated command binds its
                // own index buffer(s).
                Pm4::ValidateDrawInfo drawInfo;
                drawInfo.vtxIdxCount   = 0;
                drawInfo.instanceCount = 0;
                drawInfo.firstVertex   = 0;
                drawInfo.firstInstance = 0;
                drawInfo.firstIndex    = 0;
                drawInfo.useOpaque     = false;
                if (gfx6Generator.ContainsIndexBufferBind() || (gfx6Generator.Type() == Pm4::GeneratorType::Draw))
                {
                    ValidateDraw<false, true>(drawInfo);
                }
                else
                {
                    ValidateDraw<true, true>(drawInfo);
                }

                CommandGeneratorTouchedUserData(m_graphicsState.gfxUserDataEntries.touched,
                                                gfx6Generator,
                                                *m_pSignatureGfx);
            }
            else
            {
                pDeCmdSpace = m_deCmdStream.ReserveCommands();
                pDeCmdSpace = ValidateDispatch(0uLL, {}, pDeCmdSpace);
                m_deCmdStream.CommitCommands(pDeCmdSpace);

                CommandGeneratorTouchedUserData(m_computeState.csUserDataEntries.touched,
                                                gfx6Generator,
                                                *m_pSignatureCs);
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

            m_deCmdStream.ExecuteGeneratedCommands(ppChunkList[0], numChunksExecuted, numGenChunks);

            pDeCmdSpace = m_deCmdStream.ReserveCommands();

            // We need to issue any post-draw or post-dispatch workarounds after all of the generated command buffers
            // have finished.
            if (bindPoint == PipelineBindPoint::Graphics)
            {
                pDeCmdSpace = m_workaroundState.PostDraw(m_graphicsState, pDeCmdSpace);

                if ((gfx6Generator.Type() == Pm4::GeneratorType::Draw) ||
                    (gfx6Generator.Type() == Pm4::GeneratorType::DrawIndexed))
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
void UniversalCmdBuffer::GetChunkForCmdGeneration(
    const Pm4::IndirectCmdGenerator& generator,
    const Pal::Pipeline&             pipeline,
    uint32                           maxCommands,
    uint32                           numChunkOutputs,
    ChunkOutput*                     pChunkOutputs)
{
    const auto& properties = generator.Properties();

    PAL_ASSERT(m_pCmdAllocator != nullptr);
    PAL_ASSERT(numChunkOutputs == 1);

    CmdStreamChunk*const pChunk = Pal::Pm4CmdBuffer::GetNextGeneratedChunk();
    pChunkOutputs->pChunk = pChunk;

    const uint32* pUserDataEntries   = nullptr;
    bool          usesVertexBufTable = false;
    uint32        spillThreshold     = NoUserDataSpilling;

    if (generator.Type() == Pm4::GeneratorType::Dispatch)
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
    (pChunkOutputs->commandsInChunk) =
        m_deCmdStream.PrepareChunkForCmdGeneration(pChunk, commandDwords, embeddedDwords, maxCommands);
    (pChunkOutputs->embeddedDataSize) = ((pChunkOutputs->commandsInChunk) * embeddedDwords);

    // Populate command buffer chain size required later for an indirect command generation optimization.
    (pChunkOutputs->chainSizeInDwords) = m_deCmdStream.GetChainSizeInDwords(m_device, IsNested());

    if (embeddedDwords > 0)
    {
        // If each generated command requires some amount of spill-table space, then we need to allocate embeded data
        // space for all of the generated commands which will go into this chunk. PrepareChunkForCmdGeneration() should
        // have determined a value for commandsInChunk which allows us to allocate the appropriate amount of embeded
        // data space.
        uint32* pDataSpace = pChunk->ValidateCmdGenerationDataSpace(pChunkOutputs->embeddedDataSize,
                                                                    &(pChunkOutputs->embeddedDataAddr));

        // We also need to seed the embedded data for each generated command with the current indirect user-data table
        // and spill-table contents, because the generator will only update the table entries which get modified.
        for (uint32 cmd = 0; cmd < (pChunkOutputs->commandsInChunk); ++cmd)
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

// =====================================================================================================================
// Helper method for handling the state "leakage" from a nested command buffer back to its caller. Since the callee has
// tracked its own state during the building phase, we can access the final state of the command buffer since its stored
// in the UniversalCmdBuffer object itself.
void UniversalCmdBuffer::LeakNestedCmdBufferState(
    const UniversalCmdBuffer& cmdBuffer)
{
    Pal::Pm4::UniversalCmdBuffer::LeakNestedCmdBufferState(cmdBuffer);

    if (cmdBuffer.m_graphicsState.leakFlags.validationBits.colorTargetView != 0)
    {
        CopyColorTargetViewStorage(m_colorTargetViewStorage, cmdBuffer.m_colorTargetViewStorage, &m_graphicsState);
    }

    if (cmdBuffer.m_graphicsState.leakFlags.validationBits.depthStencilView != 0)
    {
        CopyDepthStencilViewStorage(&m_depthStencilViewStorage, &cmdBuffer.m_depthStencilViewStorage, &m_graphicsState);
    }

    if (cmdBuffer.m_graphicsState.pipelineState.pPipeline != nullptr)
    {
        m_vertexOffsetReg  = cmdBuffer.m_vertexOffsetReg;
        m_drawIndexReg     = cmdBuffer.m_drawIndexReg;

        // Update the functions that are modified by nested command list
        m_pfnValidateUserDataGfx                   = cmdBuffer.m_pfnValidateUserDataGfx;
        m_pfnValidateUserDataGfxPipelineSwitch     = cmdBuffer.m_pfnValidateUserDataGfxPipelineSwitch;
        m_funcTable.pfnCmdDraw                     = cmdBuffer.m_funcTable.pfnCmdDraw;
        m_funcTable.pfnCmdDrawOpaque               = cmdBuffer.m_funcTable.pfnCmdDrawOpaque;
        m_funcTable.pfnCmdDrawIndexed              = cmdBuffer.m_funcTable.pfnCmdDrawIndexed;
        m_funcTable.pfnCmdDrawIndirectMulti        = cmdBuffer.m_funcTable.pfnCmdDrawIndirectMulti;
        m_funcTable.pfnCmdDrawIndexedIndirectMulti = cmdBuffer.m_funcTable.pfnCmdDrawIndexedIndirectMulti;

        if (m_cachedSettings.rbPlusSupported != 0)
        {
            m_sxPsDownconvert   = cmdBuffer.m_sxPsDownconvert;
            m_sxBlendOptEpsilon = cmdBuffer.m_sxBlendOptEpsilon;
            m_sxBlendOptControl = cmdBuffer.m_sxBlendOptControl;
        }
    }

    m_dbRenderOverride = cmdBuffer.m_dbRenderOverride;
    m_dbShaderControl  = cmdBuffer.m_dbShaderControl;
    m_cbColorControl   = cmdBuffer.m_cbColorControl;
    m_paClClipCntl     = cmdBuffer.m_paClClipCntl;
    m_cbTargetMask     = cmdBuffer.m_cbTargetMask;
    m_vgtTfParam       = cmdBuffer.m_vgtTfParam;
    m_paScLineCntl     = cmdBuffer.m_paScLineCntl;
    m_depthClampMode   = cmdBuffer.m_depthClampMode;

    // If the nested command buffer updated PA_SU_SC_MODE_CNTL, leak its state back to the caller.
    if (cmdBuffer.m_graphicsState.leakFlags.validationBits.triangleRasterState)
    {
        m_paSuScModeCntl.u32All = cmdBuffer.m_paSuScModeCntl.u32All;
    }

    if (cmdBuffer.HasStreamOutBeenSet())
    {
        // If the nested command buffer set their own stream-out targets, we can simply copy the SRD's because CE
        // RAM is up-to-date.
        memcpy(&m_streamOut.srd[0], &cmdBuffer.m_streamOut.srd[0], sizeof(m_streamOut.srd));
    }

    m_drawTimeHwState.valid.u32All = 0;

    //Update vgtDmaIndexType register if the nested command buffer updated the graphics iaStates
    if (m_graphicsState.dirtyFlags.nonValidationBits.iaState !=0 )
    {
        m_drawTimeHwState.dirty.indexType = 1;
        m_vgtDmaIndexType.bits.INDEX_TYPE = VgtIndexTypeLookup[static_cast<uint32>(m_graphicsState.iaState.indexType)];
    }

    m_workaroundState.LeakNestedCmdBufferState(cmdBuffer.m_workaroundState);

    m_vbTable.state.dirty       |= cmdBuffer.m_vbTable.modified;
    m_vbTable.watermark          = cmdBuffer.m_vbTable.watermark;
    m_spillTable.stateCs.dirty  |= cmdBuffer.m_spillTable.stateCs.dirty;
    m_spillTable.stateGfx.dirty |= cmdBuffer.m_spillTable.stateGfx.dirty;

    if (cmdBuffer.m_graphicsState.pipelineState.dirtyFlags.pipeline ||
        (cmdBuffer.m_graphicsState.pipelineState.pPipeline != nullptr))
    {
        m_spiPsInControl = cmdBuffer.m_spiPsInControl;
        m_spiVsOutConfig = cmdBuffer.m_spiVsOutConfig;
    }

    m_pipelineCtxRegHash = cmdBuffer.m_pipelineCtxRegHash;

    // It is possible that nested command buffer execute operation which affect the data in the primary buffer
    m_pm4CmdBufState.flags.gfxBltActive              = cmdBuffer.m_pm4CmdBufState.flags.gfxBltActive;
    m_pm4CmdBufState.flags.csBltActive               = cmdBuffer.m_pm4CmdBufState.flags.csBltActive;
    m_pm4CmdBufState.flags.gfxWriteCachesDirty       = cmdBuffer.m_pm4CmdBufState.flags.gfxWriteCachesDirty;
    m_pm4CmdBufState.flags.csWriteCachesDirty        = cmdBuffer.m_pm4CmdBufState.flags.csWriteCachesDirty;
    m_pm4CmdBufState.flags.cpWriteCachesDirty        = cmdBuffer.m_pm4CmdBufState.flags.cpWriteCachesDirty;
    m_pm4CmdBufState.flags.cpMemoryWriteL2CacheStale = cmdBuffer.m_pm4CmdBufState.flags.cpMemoryWriteL2CacheStale;

    m_pSignatureCs = cmdBuffer.m_pSignatureCs;
    m_pSignatureGfx = cmdBuffer.m_pSignatureGfx;

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
    const auto*const pPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
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
void UniversalCmdBuffer::CmdUpdateHiSPretests(
    const IImage*      pImage,
    const HiSPretests& pretests,
    uint32             firstMip,
    uint32             numMips)
{
    const Image* pGfx6Image = static_cast<const Image*>(static_cast<const Pal::Image*>(pImage)->GetGfxImage());

    if (pGfx6Image->HasHiSPretestsMetaData())
    {
        SubresRange range = { };
        range.startSubres = { pGfx6Image->GetStencilPlane(), firstMip, 0 };
        range.numPlanes   = 1;
        range.numMips     = numMips;
        range.numSlices   = pImage->GetImageCreateInfo().arraySize;

        const PM4Predicate packetPredicate = PacketPredicate();

        uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
        pCmdSpace = pGfx6Image->UpdateHiSPretestsMetaData(range, pretests, packetPredicate, pCmdSpace);

        if (m_graphicsState.bindTargets.depthTarget.pDepthStencilView != nullptr)
        {
            const DepthStencilView* const pView =
                static_cast<const DepthStencilView*>(m_graphicsState.bindTargets.depthTarget.pDepthStencilView);

            // If the bound image matches the cleared image, we update DB_SRESULTS_COMPARE_STATE0/1 immediately.
            if ((pView->GetImage() == pGfx6Image) &&
                (pView->MipLevel() >= range.startSubres.mipLevel) &&
                (pView->MipLevel() <  range.startSubres.mipLevel + range.numMips))
            {
                Gfx6HiSPretestsMetaData pretestsMetaData = {};

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
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp6, true, true, true>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp6, true, true, true>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp6, true, true, true>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp6, true, true, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp6, true, true, true>;
                break;
            case GfxIpLevel::GfxIp7:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp7, true, true, true>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp7, true, true, true>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp7, true, true, true>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp7, true, true, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp7, true, true, true>;
                break;
            case GfxIpLevel::GfxIp8:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp8, true, true, true>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp8, true, true, true>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp8, true, true, true>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8, true, true, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8, true, true, true>;
                break;
            case GfxIpLevel::GfxIp8_1:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp8_1, true, true, true>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp8_1, true, true, true>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp8_1, true, true, true>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8_1, true, true, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8_1, true, true, true>;
                break;
            default:
                PAL_ASSERT_ALWAYS();
                break;
            }
        }
        else if (m_cachedSettings.describeDrawDispatch)
        {
            switch (m_device.Parent()->ChipProperties().gfxLevel)
            {
            case GfxIpLevel::GfxIp6:
                m_funcTable.pfnCmdDraw = CmdDraw<GfxIpLevel::GfxIp6, false, true, true>;
                m_funcTable.pfnCmdDrawOpaque = CmdDrawOpaque<GfxIpLevel::GfxIp6, false, true, true>;
                m_funcTable.pfnCmdDrawIndexed = CmdDrawIndexed<GfxIpLevel::GfxIp6, false, true, true>;
                m_funcTable.pfnCmdDrawIndirectMulti = CmdDrawIndirectMulti<GfxIpLevel::GfxIp6, false, true, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp6, false, true, true>;
                break;
            case GfxIpLevel::GfxIp7:
                m_funcTable.pfnCmdDraw = CmdDraw<GfxIpLevel::GfxIp7, false, true, true>;
                m_funcTable.pfnCmdDrawOpaque = CmdDrawOpaque<GfxIpLevel::GfxIp7, false, true, true>;
                m_funcTable.pfnCmdDrawIndexed = CmdDrawIndexed<GfxIpLevel::GfxIp7, false, true, true>;
                m_funcTable.pfnCmdDrawIndirectMulti = CmdDrawIndirectMulti<GfxIpLevel::GfxIp7, false, true, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp7, false, true, true>;
                break;
            case GfxIpLevel::GfxIp8:
                m_funcTable.pfnCmdDraw = CmdDraw<GfxIpLevel::GfxIp8, false, true, true>;
                m_funcTable.pfnCmdDrawOpaque = CmdDrawOpaque<GfxIpLevel::GfxIp8, false, true, true>;
                m_funcTable.pfnCmdDrawIndexed = CmdDrawIndexed<GfxIpLevel::GfxIp8, false, true, true>;
                m_funcTable.pfnCmdDrawIndirectMulti = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8, false, true, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8, false, true, true>;
                break;
            case GfxIpLevel::GfxIp8_1:
                m_funcTable.pfnCmdDraw = CmdDraw<GfxIpLevel::GfxIp8_1, false, true, true>;
                m_funcTable.pfnCmdDrawOpaque = CmdDrawOpaque<GfxIpLevel::GfxIp8_1, false, true, true>;
                m_funcTable.pfnCmdDrawIndexed = CmdDrawIndexed<GfxIpLevel::GfxIp8_1, false, true, true>;
                m_funcTable.pfnCmdDrawIndirectMulti = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8_1, false, true, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8_1, false, true, true>;
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
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp6, false, true, false>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp6, false, true, false>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp6, false, true, false>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp6, false, true, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp6, false, true, false>;
                break;
            case GfxIpLevel::GfxIp7:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp7, false, true, false>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp7, false, true, false>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp7, false, true, false>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp7, false, true, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp7, false, true, false>;
                break;
            case GfxIpLevel::GfxIp8:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp8, false, true, false>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp8, false, true, false>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp8, false, true, false>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8, false, true, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8, false, true, false>;
                break;
            case GfxIpLevel::GfxIp8_1:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp8_1, false, true, false>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp8_1, false, true, false>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp8_1, false, true, false>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8_1, false, true, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8_1, false, true, false>;
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
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp6, true, false, true>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp6, true, false, true>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp6, true, false, true>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp6, true, false, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp6, true, false, true>;
                break;
            case GfxIpLevel::GfxIp7:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp7, true, false, true>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp7, true, false, true>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp7, true, false, true>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp7, true, false, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp7, true, false, true>;
                break;
            case GfxIpLevel::GfxIp8:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp8, true, false, true>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp8, true, false, true>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp8, true, false, true>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8, true, false, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8, true, false, true>;
                break;
            case GfxIpLevel::GfxIp8_1:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp8_1, true, false, true>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp8_1, true, false, true>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp8_1, true, false, true>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8_1, true, false, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8_1, true, false, true>;
                break;
            default:
                PAL_ASSERT_ALWAYS();
                break;
            }
        }
        else if (m_cachedSettings.describeDrawDispatch)
        {
            switch (m_device.Parent()->ChipProperties().gfxLevel)
            {
            case GfxIpLevel::GfxIp6:
                m_funcTable.pfnCmdDraw = CmdDraw<GfxIpLevel::GfxIp6, false, false, true>;
                m_funcTable.pfnCmdDrawOpaque = CmdDrawOpaque<GfxIpLevel::GfxIp6, false, false, true>;
                m_funcTable.pfnCmdDrawIndexed = CmdDrawIndexed<GfxIpLevel::GfxIp6, false, false, true>;
                m_funcTable.pfnCmdDrawIndirectMulti = CmdDrawIndirectMulti<GfxIpLevel::GfxIp6, false, false, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp6, false, false, true>;
                break;
            case GfxIpLevel::GfxIp7:
                m_funcTable.pfnCmdDraw = CmdDraw<GfxIpLevel::GfxIp7, false, false, true>;
                m_funcTable.pfnCmdDrawOpaque = CmdDrawOpaque<GfxIpLevel::GfxIp7, false, false, true>;
                m_funcTable.pfnCmdDrawIndexed = CmdDrawIndexed<GfxIpLevel::GfxIp7, false, false, true>;
                m_funcTable.pfnCmdDrawIndirectMulti = CmdDrawIndirectMulti<GfxIpLevel::GfxIp7, false, false, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp7, false, false, true>;
                break;
            case GfxIpLevel::GfxIp8:
                m_funcTable.pfnCmdDraw = CmdDraw<GfxIpLevel::GfxIp8, false, false, true>;
                m_funcTable.pfnCmdDrawOpaque = CmdDrawOpaque<GfxIpLevel::GfxIp8, false, false, true>;
                m_funcTable.pfnCmdDrawIndexed = CmdDrawIndexed<GfxIpLevel::GfxIp8, false, false, true>;
                m_funcTable.pfnCmdDrawIndirectMulti = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8, false, false, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8, false, false, true>;
                break;
            case GfxIpLevel::GfxIp8_1:
                m_funcTable.pfnCmdDraw = CmdDraw<GfxIpLevel::GfxIp8_1, false, false, true>;
                m_funcTable.pfnCmdDrawOpaque = CmdDrawOpaque<GfxIpLevel::GfxIp8_1, false, false, true>;
                m_funcTable.pfnCmdDrawIndexed = CmdDrawIndexed<GfxIpLevel::GfxIp8_1, false, false, true>;
                m_funcTable.pfnCmdDrawIndirectMulti = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8_1, false, false, true>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8_1, false, false, true>;
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
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp6, false, false, false>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp6, false, false, false>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp6, false, false, false>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp6, false, false, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp6, false, false, false>;
                break;
            case GfxIpLevel::GfxIp7:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp7, false, false, false>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp7, false, false, false>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp7, false, false, false>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp7, false, false, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp7, false, false, false>;
                break;
            case GfxIpLevel::GfxIp8:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp8, false, false, false>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp8, false, false, false>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp8, false, false, false>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8, false, false, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8, false, false, false>;
                break;
            case GfxIpLevel::GfxIp8_1:
                m_funcTable.pfnCmdDraw                     = CmdDraw<GfxIpLevel::GfxIp8_1, false, false, false>;
                m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque<GfxIpLevel::GfxIp8_1, false, false, false>;
                m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed<GfxIpLevel::GfxIp8_1, false, false, false>;
                m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti<GfxIpLevel::GfxIp8_1, false, false, false>;
                m_funcTable.pfnCmdDrawIndexedIndirectMulti =
                    CmdDrawIndexedIndirectMulti<GfxIpLevel::GfxIp8_1, false, false, false>;
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
    dmaDataInfo.predicate   = static_cast<PM4Predicate>(GetPm4CmdBufState().flags.packetPredicate);
    dmaDataInfo.dstAddr     = dstAddr;
    dmaDataInfo.srcAddr     = srcAddr;
    dmaDataInfo.numBytes    = static_cast<uint32>(numBytes);

    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
    pCmdSpace += m_cmdUtil.BuildDmaData(dmaDataInfo, pCmdSpace);
    m_deCmdStream.CommitCommands(pCmdSpace);

    SetPm4CmdBufCpBltState(true);

    if (supportsL2)
    {
        SetPm4CmdBufCpBltWriteCacheState(true);
    }
    else
    {
        SetPm4CmdBufCpMemoryWriteL2CacheStaleState(true);
    }
}

// =====================================================================================================================
// The workaround for the "DB Over-Rasterization" hardware bug requires us to write the DB_SHADER_CONTROL register at
// draw-time. This function writes the PM4 commands necessary and returns the next unused DWORD in pCmdSpace.
uint32* UniversalCmdBuffer::WriteDbShaderControl(
    bool       isDepthEnabled,
    bool       usesOverRasterization,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace)
{
    // DB_SHADER_CONTROL must be written at draw-time for particular GPU's to work-around a hardware bug.
    if (m_device.WaDbOverRasterization())
    {
        regDB_SHADER_CONTROL dbShaderControl = m_dbShaderControl;
        if ((dbShaderControl.bits.Z_ORDER == EARLY_Z_THEN_LATE_Z) && usesOverRasterization && isDepthEnabled)
        {
            // Apply the "DB Over-Rasterization" workaround: The DB has a bug with early-Z where the DB will kill
            // pixels when over-rasterization is enabled.  Normally the fix would be to force post-Z over-rasterization
            // via DB_EQAA, but that workaround isn't sufficient if depth testing is enabled.  In that case, we need to
            // force late-Z in the pipeline.
            //
            // If the workaround is active, and both depth testing and over-rasterization are enabled, and the pipeline
            // isn't already using late-Z, then we need to force late-Z for the current pipeline.
            dbShaderControl.bits.Z_ORDER = LATE_Z;
        }

        if (m_dbShaderControl.u32All != dbShaderControl.u32All)
        {
            pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_SHADER_CONTROL,
                m_dbShaderControl.u32All,
                pCmdSpace);
            m_dbShaderControl = dbShaderControl;
        }
    }

    return pCmdSpace;
}

} // Gfx6
} // Pal
