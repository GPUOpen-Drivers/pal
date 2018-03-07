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

#include "core/device.h"
#include "core/platform.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9ColorBlendState.h"
#include "core/hw/gfxip/gfx9/gfx9DepthStencilState.h"
#include "core/hw/gfxip/gfx9/gfx9DepthStencilView.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9PrefetchMgr.h"
#include "core/hw/gfxip/gfx9/gfx9UniversalCmdBuffer.h"
#include "palFormatInfo.h"
#include "palInlineFuncs.h"
#include "palPipelineAbiProcessorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// User-data signature for an unbound graphics pipeline.
const GraphicsPipelineSignature NullGfxSignature =
{
    { UserDataNotMapped, },     // User-data mapping for each shader stage
    { UserDataNotMapped, },     // Indirect user-data table mapping
    UserDataNotMapped,          // Stream-out table mapping
    UserDataNotMapped,          // Vertex offset register address
    UserDataNotMapped,          // Draw ID register address
    UserDataNotMapped,          // Start Index register address
    UserDataNotMapped,          // Log2(sizeof(indexType)) register address
    UserDataNotMapped,          // ES/GS LDS size register address
    UserDataNotMapped,          // ES/GS LDS size register address
    NoUserDataSpilling,         // Spill threshold
    0,                          // User-data entry limit
    { UserDataNotMapped, },     // Compacted view ID register addresses
};
static_assert(UserDataNotMapped == 0, "Unexpected value for indicating unmapped user-data entries!");

static uint8 Rop3(LogicOp logicOp);
static SX_DOWNCONVERT_FORMAT SxDownConvertFormat(ChNumFormat format);
static uint32 SxBlendOptEpsilon(SX_DOWNCONVERT_FORMAT sxDownConvertFormat);
static uint32 SxBlendOptControl(uint32 writeMask);

// =====================================================================================================================
// Determines whether we can allow the hardware to render out-of-order primitives.  This is done by determing the
// effects that this could have on the depth buffer, stencil buffer, and render target.
bool GraphicsPipeline::CanDrawPrimsOutOfOrder(
    const DepthStencilView*  pDsView,
    const DepthStencilState* pDepthStencilState,
    const ColorBlendState*   pBlendState,
    uint32                   hasActiveQueries,
    Gfx9OutOfOrderPrimMode   gfx9EnableOutOfOrderPrimitives
    ) const
{
    bool enableOutOfOrderPrims = true;

    if ((gfx9EnableOutOfOrderPrimitives == Gfx9OutOfOrderPrimSafe) ||
        (gfx9EnableOutOfOrderPrimitives == Gfx9OutOfOrderPrimAggressive))
    {
        if (PsUsesUavs() || pDepthStencilState == nullptr)
        {
            enableOutOfOrderPrims = false;
        }
        else
        {
            bool isDepthStencilWriteEnabled = false;

            if (pDsView != nullptr)
            {
                const bool isDepthWriteEnabled = (pDsView->GetDsViewCreateInfo().flags.readOnlyDepth == 0) &&
                                                 (pDepthStencilState->IsDepthWriteEnabled());

                const bool isStencilWriteEnabled = (pDsView->GetDsViewCreateInfo().flags.readOnlyStencil == 0) &&
                                                   (pDepthStencilState->IsStencilWriteEnabled());

                isDepthStencilWriteEnabled = (isDepthWriteEnabled || isStencilWriteEnabled);
            }

            bool canDepthStencilRunOutOfOrder = false;

            if ((gfx9EnableOutOfOrderPrimitives == Gfx9OutOfOrderPrimSafe) && (hasActiveQueries != 0))
            {
                canDepthStencilRunOutOfOrder = !isDepthStencilWriteEnabled;
            }
            else
            {
                canDepthStencilRunOutOfOrder =
                    (isDepthStencilWriteEnabled == false) ||
                    (pDepthStencilState->CanDepthRunOutOfOrder() && pDepthStencilState->CanStencilRunOutOfOrder());
            }

            // Primitive ordering must be honored when no depth-stencil view is bound.
            if ((canDepthStencilRunOutOfOrder == false) || (pDsView == nullptr))
            {
                enableOutOfOrderPrims = false;
            }
            else
            {
                // Aggressive setting allows render target writes to run out of order if depth testing forces
                // ordering of primitives.
                const bool canRenderTargetRunOutOfOrder =
                    (gfx9EnableOutOfOrderPrimitives == Gfx9OutOfOrderPrimAggressive) &&
                    (pDepthStencilState->DepthForcesOrdering());

                // Depth testing is required for the z-buffer to be correctly constructed with out-of-order primitives.
                // This should already be baked into each of the above flags implicitly.
                PAL_ASSERT((canRenderTargetRunOutOfOrder == false) || pDepthStencilState->IsDepthEnabled());

                if (pBlendState != nullptr)
                {
                    for (uint32 i = 0; i < MaxColorTargets; i++)
                    {
                        if (GetTargetMask(i) > 0)
                        {
                            // There may be precision delta with out-of-order blending, so only allow out-of-order
                            // primitives for commutative blending with aggressive setting.
                            const bool canBlendingRunOutOfOrder =
                                (pBlendState->IsBlendCommutative(i) &&
                                (gfx9EnableOutOfOrderPrimitives == Gfx9OutOfOrderPrimAggressive));

                            // We cannot enable out of order primitives if
                            //   1. If blending is off and depth ordering of the samples is not enforced.
                            //   2. If commutative blending is enabled and depth/stencil writes are disabled.
                            if ((pBlendState->IsBlendEnabled(i) || (canRenderTargetRunOutOfOrder == false)) &&
                                ((canBlendingRunOutOfOrder == false) || isDepthStencilWriteEnabled))
                            {
                                enableOutOfOrderPrims = false;
                                break;
                            }
                        }
                    }
                }
                else
                {
                    enableOutOfOrderPrims = canRenderTargetRunOutOfOrder;
                }
            }
        }
    }
    else if (gfx9EnableOutOfOrderPrimitives != Gfx9OutOfOrderPrimAlways)
    {
        enableOutOfOrderPrims = false;
    }

    return enableOutOfOrderPrims;
}

// =====================================================================================================================
GraphicsPipeline::GraphicsPipeline(
    Device* pDevice,
    bool    isInternal)
    :
    Pal::GraphicsPipeline(pDevice->Parent(), isInternal),
    m_gfxLevel(pDevice->Parent()->ChipProperties().gfxLevel),
    m_pDevice(pDevice),
    m_contextPm4ImgHash(0),
    m_chunkHs(*pDevice),
    m_chunkGs(*pDevice),
    m_chunkVs(*pDevice),
    m_chunkPs(*pDevice)
{
    memset(&m_statePm4CmdsSh,      0, sizeof(m_statePm4CmdsSh));
    memset(&m_statePm4CmdsContext, 0, sizeof(m_statePm4CmdsContext));
    memset(&m_streamoutPm4Cmds,    0, sizeof(m_streamoutPm4Cmds));
    memset(&m_iaMultiVgtParam[0],  0, sizeof(m_iaMultiVgtParam));

    memcpy(&m_signature, &NullGfxSignature, sizeof(m_signature));

    m_vgtLsHsConfig.u32All  = 0;
    m_spiVsOutConfig.u32All = 0;
    m_spiPsInControl.u32All = 0;
    m_paScModeCntl1.u32All  = 0;
}

// =====================================================================================================================
// Initializes HW-specific state related to this graphics pipeline (register values, user-data mapping, etc.) using the
// specified Pipeline ABI processor and create info.
Result GraphicsPipeline::HwlInit(
    const GraphicsPipelineCreateInfo& createInfo,
    const AbiProcessor&               abiProcessor)
{
    const Gfx9PalSettings& settings = m_pDevice->Settings();

    // First, handle relocations and upload the pipeline code & data to GPU memory.
    gpusize codeGpuVirtAddr = 0;
    gpusize dataGpuVirtAddr = 0;
    Result result = PerformRelocationsAndUploadToGpuMemory(abiProcessor, &codeGpuVirtAddr, &dataGpuVirtAddr);
    if (result ==  Result::Success)
    {
        MetroHash64 hasher;

        InitCommonStateRegisters(createInfo, abiProcessor);
        BuildPm4Headers(VgtStrmoutConfig().u32All != 0);

        SetupSignatureFromElf(abiProcessor);

        // SetupStereoRegisters uses signature so it must be called after SetupSignatureFromElf
        SetupStereoRegisters();

        if (IsTessEnabled())
        {
            HsParams params = {};
            params.codeGpuVirtAddr = codeGpuVirtAddr;
            params.dataGpuVirtAddr = dataGpuVirtAddr;
            params.pHsPerfDataInfo = &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Hs)];
            params.pHasher         = &hasher;

            m_chunkHs.Init(abiProcessor, params);
        }
        if (IsGsEnabled() || IsNgg())
        {
            GsParams params = {};
            params.codeGpuVirtAddr   = codeGpuVirtAddr;
            params.dataGpuVirtAddr   = dataGpuVirtAddr;
            params.esGsLdsSizeRegGs  = m_signature.esGsLdsSizeRegAddrGs;
            params.esGsLdsSizeRegVs  = m_signature.esGsLdsSizeRegAddrVs;
            params.isNgg             = IsNgg();
            params.usesOnChipGs      = IsGsOnChip();
            params.pGsPerfDataInfo   = &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Gs)];
            params.pCopyPerfDataInfo = &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Vs)];
            params.pHasher           = &hasher;

            m_chunkGs.Init(abiProcessor, params);
        }
        else
        {
            VsParams params = {};
            params.codeGpuVirtAddr = codeGpuVirtAddr;
            params.dataGpuVirtAddr = dataGpuVirtAddr;
            params.pVsPerfDataInfo = &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Vs)];
            params.pHasher         = &hasher;

            m_chunkVs.Init(abiProcessor, params);
        }

        PsParams params = {};
        params.codeGpuVirtAddr = codeGpuVirtAddr;
        params.dataGpuVirtAddr = dataGpuVirtAddr;
        params.isNgg           = IsNgg();
        params.pPsPerfDataInfo = &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Ps)];
        params.pHasher         = &hasher;

        m_chunkPs.Init(abiProcessor, params);

        hasher.Update(m_statePm4CmdsContext);
        hasher.Update(m_streamoutPm4Cmds);

        hasher.Finalize(reinterpret_cast<uint8* const>(&m_contextPm4ImgHash));

        UpdateRingSizes(abiProcessor);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 387
        m_info.ps.flags.perSampleShading = m_paScModeCntl1.bits.PS_ITER_SAMPLE;
#endif
    }

    return result;
}

// =====================================================================================================================
// Retrieve the appropriate shader-stage-info based on the specifed shader type.
const ShaderStageInfo* GraphicsPipeline::GetShaderStageInfo(
    ShaderType shaderType
    ) const
{
    const ShaderStageInfo* pInfo = nullptr;

    switch (shaderType)
    {
    case ShaderType::Vertex:
        pInfo = (IsTessEnabled() ? &m_chunkHs.StageInfo()
                                 : ((IsGsEnabled() || IsNgg()) ? &m_chunkGs.StageInfo()
                                                               : &m_chunkVs.StageInfo()));
        break;
    case ShaderType::Hull:
        pInfo = (IsTessEnabled() ? &m_chunkHs.StageInfo() : nullptr);
        break;
    case ShaderType::Domain:
        pInfo = (IsTessEnabled() ? ((IsGsEnabled() || IsNgg()) ? &m_chunkGs.StageInfo()
                                                               : &m_chunkVs.StageInfo())
                                 : nullptr);
        break;
    case ShaderType::Geometry:
        pInfo = (IsGsEnabled() ? &m_chunkGs.StageInfo() : nullptr);
        break;
    case ShaderType::Pixel:
        pInfo = &m_chunkPs.StageInfo();
        break;
    default:
        break;
    }

    return pInfo;
}

// =====================================================================================================================
// Helper function to compute the WAVE_LIMIT field of the SPI_SHADER_PGM_RSRC3* registers.
uint32 GraphicsPipeline::CalcMaxWavesPerSh(
    uint32 maxWavesPerCu1,
    uint32 maxWavesPerCu2
    ) const
{
    constexpr uint32 MaxWavesPerShGraphics         = 63u;
    constexpr uint32 MaxWavesPerShGraphicsUnitSize = 16u;

    const auto& gfx9ChipProps = m_pDevice->Parent()->ChipProperties().gfx9;

    // The HW shader stage might a combination of two API shader stages (e.g., for GS copy shaders), so we must apply
    // the minimum wave limit of both API shader stages.  Note that zero is the largest value because it means
    // unlimited.
    const uint32 maxWavesPerCu =
        ((maxWavesPerCu2 == 0) ? maxWavesPerCu1
                               : ((maxWavesPerCu1 == 0) ? maxWavesPerCu2
                                                        : Min(maxWavesPerCu1, maxWavesPerCu2)));

    // The maximum number of waves per SH in "register units".
    // By default set the WAVE_LIMIT field to the maximum possible value.
    uint32 wavesPerSh = MaxWavesPerShGraphics;

    // If the caller would like to override the default maxWavesPerCu
    if (maxWavesPerCu > 0)
    {
        // We assume no one is trying to use more than 100% of all waves.
        const uint32 numWavefrontsPerCu = (NumSimdPerCu * gfx9ChipProps.numWavesPerSimd);
        PAL_ASSERT(maxWavesPerCu <= numWavefrontsPerCu);

        const uint32 maxWavesPerSh = (maxWavesPerCu * gfx9ChipProps.numCuPerSh);

        // For graphics shaders, the WAVES_PER_SH field is in units of 16 waves and must not exceed 63. We must
        // also clamp to one if maxWavesPerSh rounded down to zero to prevent the limit from being removed.
        wavesPerSh = Min(MaxWavesPerShGraphics, Max(1u, maxWavesPerSh / MaxWavesPerShGraphicsUnitSize));
    }

    return wavesPerSh;
}

// =====================================================================================================================
// Helper for setting the dynamic stage info.
void GraphicsPipeline::CalcDynamicStageInfo(
    const DynamicGraphicsShaderInfo& shaderInfo,
    DynamicStageInfo*                pStageInfo
    ) const
{
    pStageInfo->wavesPerSh   = CalcMaxWavesPerSh(shaderInfo.maxWavesPerCu, 0);
    pStageInfo->cuEnableMask = shaderInfo.cuEnableMask;
}

// =====================================================================================================================
// Helper for setting the dynamic stage info.
void GraphicsPipeline::CalcDynamicStageInfo(
    const DynamicGraphicsShaderInfo& shaderInfo1,
    const DynamicGraphicsShaderInfo& shaderInfo2,
    DynamicStageInfo*                pStageInfo
    ) const
{
    pStageInfo->wavesPerSh   = CalcMaxWavesPerSh(shaderInfo1.maxWavesPerCu, shaderInfo2.maxWavesPerCu);
    pStageInfo->cuEnableMask = shaderInfo1.cuEnableMask & shaderInfo2.cuEnableMask;
}

// =====================================================================================================================
// Helper for setting all the dynamic stage infos.
void GraphicsPipeline::CalcDynamicStageInfos(
    const DynamicGraphicsShaderInfos& graphicsInfo,
    DynamicStageInfos*                pStageInfos
    ) const
{
    CalcDynamicStageInfo(graphicsInfo.ps, &pStageInfos->ps);

    if (IsTessEnabled())
    {
        CalcDynamicStageInfo(graphicsInfo.vs, graphicsInfo.hs, &pStageInfos->hs);

        if (IsNgg() || IsGsEnabled())
        {
            // IsNgg(): PipelineNggTess
            // API Shader -> Hardware Stage
            // PS -> PS
            // DS/GS -> GS
            // VS/HS -> HS

            // IsGsEnabled(): PipelineGsTess
            // API Shader -> Hardware Stage
            // PS -> PS
            // DS/GS -> GS
            // VS/HS -> HS

            CalcDynamicStageInfo(graphicsInfo.ds, graphicsInfo.gs, &pStageInfos->gs);
        }
        else
        {
            // PipelineTess
            // API Shader -> Hardware Stage
            // PS -> PS
            // DS -> VS
            // VS/HS -> HS
            CalcDynamicStageInfo(graphicsInfo.ds, &pStageInfos->vs);
        }
    }
    else
    {
        if (IsNgg() || IsGsEnabled())
        {
            // IsNgg(): PipelineNgg
            // API Shader -> Hardware Stage
            // PS -> PS
            // VS/GS -> GS

            // IsGsEnabled(): PipelineGs
            // API Shader -> Hardware Stage
            // PS -> PS
            // VS/GS -> GS

            CalcDynamicStageInfo(graphicsInfo.vs, graphicsInfo.gs, &pStageInfos->gs);
        }
        else
        {
            // PipelineVsPs
            // API Shader -> Hardware Stage
            // PS -> PS
            // VS -> VS

            CalcDynamicStageInfo(graphicsInfo.vs, &pStageInfos->vs);
        }
    }
}

// =====================================================================================================================
// Helper function for writing common sh images which are shared by all graphics pipelines.
// Returns a command buffer pointer incremented to the end of the commands we just wrote.
uint32* GraphicsPipeline::WriteShCommands(
    CmdStream*                        pCmdStream,
    uint32*                           pCmdSpace,
    const DynamicGraphicsShaderInfos& graphicsInfo
    ) const
{
    PAL_ASSERT(pCmdStream != nullptr);

    DynamicStageInfos stageInfos = {};
    CalcDynamicStageInfos(graphicsInfo, &stageInfos);

    if (IsTessEnabled())
    {
        pCmdSpace = m_chunkHs.WriteShCommands(pCmdStream, pCmdSpace, stageInfos.hs);
    }

    if (IsGsEnabled() || IsNgg())
    {
        pCmdSpace = m_chunkGs.WriteShCommands(pCmdStream, pCmdSpace, stageInfos.gs, stageInfos.vs, IsNgg());
    }
    else
    {
        pCmdSpace = m_chunkVs.WriteShCommands(pCmdStream, pCmdSpace, stageInfos.vs);
    }

    pCmdSpace = m_chunkPs.WriteShCommands(pCmdStream, pCmdSpace, stageInfos.ps);

    pCmdSpace = pCmdStream->WritePm4Image(m_statePm4CmdsSh.spaceNeeded, &m_statePm4CmdsSh, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Helper function for writing common context images which are shared by all graphics pipelines.
// Returns a command buffer pointer incremented to the end of the commands we just wrote.
uint32* GraphicsPipeline::WriteContextCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    PAL_ASSERT(pCmdStream != nullptr);

    if (IsTessEnabled())
    {
        pCmdSpace = m_chunkHs.WriteContextCommands(pCmdStream, pCmdSpace);
    }
    if (IsGsEnabled() || IsNgg())
    {
        pCmdSpace = m_chunkGs.WriteContextCommands(pCmdStream, pCmdSpace);
    }
    else
    {
        pCmdSpace = m_chunkVs.WriteContextCommands(pCmdStream, pCmdSpace);
    }
    pCmdSpace = m_chunkPs.WriteContextCommands(pCmdStream, pCmdSpace);

    pCmdSpace = pCmdStream->WritePm4Image(m_statePm4CmdsContext.spaceNeeded, &m_statePm4CmdsContext, pCmdSpace);
    pCmdSpace = pCmdStream->WritePm4Image(m_streamoutPm4Cmds.spaceNeeded, &m_streamoutPm4Cmds, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Requests that this pipeline indicates what it would like to prefetch.
uint32* GraphicsPipeline::RequestPrefetch(
    const Pal::PrefetchMgr& prefetchMgr,
    uint32*                 pCmdSpace
    ) const
{
    const auto& gfx6PrefetchMgr = static_cast<const PrefetchMgr&>(prefetchMgr);

    PrefetchType hwEsPrefetch = PrefetchVs;
    PrefetchType hwVsPrefetch = PrefetchVs;

    if (IsTessEnabled())
    {
        pCmdSpace = gfx6PrefetchMgr.RequestPrefetch(PrefetchHs,
                                                    m_chunkHs.LsProgramGpuVa(),
                                                    m_chunkHs.StageInfo().codeLength,
                                                    pCmdSpace);

        hwVsPrefetch = PrefetchDs;
    }

    if (IsGsEnabled() || IsNgg())
    {
        pCmdSpace = gfx6PrefetchMgr.RequestPrefetch(PrefetchGs,
                                                    m_chunkGs.EsProgramGpuVa(),
                                                    m_chunkGs.StageInfo().codeLength,
                                                    pCmdSpace);
        if (IsNgg() == false)
        {
            pCmdSpace = gfx6PrefetchMgr.RequestPrefetch(PrefetchCopyShader,
                                                        m_chunkGs.VsProgramGpuVa(),
                                                        m_chunkGs.StageInfoCopy().codeLength,
                                                        pCmdSpace);
        }
    }
    else
    {
        pCmdSpace = gfx6PrefetchMgr.RequestPrefetch(hwVsPrefetch,
                                                    m_chunkVs.VsProgramGpuVa(),
                                                    m_chunkVs.StageInfo().codeLength,
                                                    pCmdSpace);
    }

    pCmdSpace = gfx6PrefetchMgr.RequestPrefetch(PrefetchPs,
                                                m_chunkPs.PsProgramGpuVa(),
                                                m_chunkPs.StageInfo().codeLength,
                                                pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Builds the packet headers for the various PM4 images associated with this pipeline.  Register values and packet
// payloads are computed elsewhere.
void GraphicsPipeline::BuildPm4Headers(
    bool useStreamOutput)
{
    const CmdUtil& cmdUtil = m_pDevice->CmdUtil();

    if (m_gfxLevel == GfxIpLevel::GfxIp9)
    {
        // Sets the following SH register: SPI_SHADER_LATE_ALLOC_VS.
        m_statePm4CmdsSh.spaceNeeded =
            cmdUtil.BuildSetOneShReg(mmSPI_SHADER_LATE_ALLOC_VS,
                                     ShaderGraphics,
                                     &m_statePm4CmdsSh.lateAlloc.gfx9.hdrSpiShaderLateAllocVs);
    }

    // Sets the following context register: VGT_SHADER_STAGES_EN.
    // We use += instead of = as the spaceNeeded for when RB+ is supported is calculated earlier on.
    m_statePm4CmdsContext.spaceNeeded +=
        cmdUtil.BuildSetOneContextReg(mmVGT_SHADER_STAGES_EN, &m_statePm4CmdsContext.hdrVgtShaderStagesEn);

    // Sets the following context register: VGT_GS_MODE.
    m_statePm4CmdsContext.spaceNeeded +=
        cmdUtil.BuildSetOneContextReg(mmVGT_GS_MODE, &m_statePm4CmdsContext.hdrVgtGsMode);

    // Sets the following context register: VGT_REUSE_OFF.
    m_statePm4CmdsContext.spaceNeeded +=
        cmdUtil.BuildSetOneContextReg(mmVGT_REUSE_OFF, &m_statePm4CmdsContext.hdrVgtReuseOff);

    // Sets the following context register: VGT_TF_PARAM.
    m_statePm4CmdsContext.spaceNeeded +=
        cmdUtil.BuildSetOneContextReg(mmVGT_TF_PARAM, &m_statePm4CmdsContext.hdrVgtTfParam);

    // Sets the following context register: CB_COLOR_CONTROL.
    m_statePm4CmdsContext.spaceNeeded +=
        cmdUtil.BuildSetOneContextReg(mmCB_COLOR_CONTROL, &m_statePm4CmdsContext.hdrCbColorControl);

    // Sets the following context registers: CB_TARGET_MASK and CB_SHADER_MASK.
    m_statePm4CmdsContext.spaceNeeded +=
        cmdUtil.BuildSetSeqContextRegs(mmCB_TARGET_MASK, mmCB_SHADER_MASK,
                                       &m_statePm4CmdsContext.hdrCbShaderTargetMask);

    // Sets the following context register: PA_CL_CLIP_CNTL.
    m_statePm4CmdsContext.spaceNeeded +=
        cmdUtil.BuildSetOneContextReg(mmPA_CL_CLIP_CNTL, &m_statePm4CmdsContext.hdrPaClClipCntl);

    // PM4 packet: sets the following context register: mmPA_SU_VTX_CNTL.
    m_statePm4CmdsContext.spaceNeeded +=
        cmdUtil.BuildSetOneContextReg(mmPA_SU_VTX_CNTL, &m_statePm4CmdsContext.hdrPaSuVtxCntl);

    // PM4 packet: sets the following context register: PA_CL_VTE_CNTL.
    m_statePm4CmdsContext.spaceNeeded +=
        cmdUtil.BuildSetOneContextReg(mmPA_CL_VTE_CNTL, &m_statePm4CmdsContext.hdrPaClVteCntl);

    // PM4 packet: sets the following context register: PA_SC_LINE_CNTL.
    m_statePm4CmdsContext.spaceNeeded +=
        cmdUtil.BuildSetOneContextReg(mmPA_SC_LINE_CNTL, &m_statePm4CmdsContext.hdrPaScLineCntl);

    if (m_gfxLevel == GfxIpLevel::GfxIp9)
    {
    }

    // PM4 packet: sets the following context register: mmSPI_INTERP_CONTROL_0.
    m_statePm4CmdsContext.spaceNeeded +=
        cmdUtil.BuildSetOneContextReg(mmSPI_INTERP_CONTROL_0, &m_statePm4CmdsContext.hdrSpiInterpControl0);

    // PM4 packet does a read/modify/write to DB_RENDER_OVERRIDE. The real packet will be created later, we just need
    // to get the size.
    m_statePm4CmdsContext.spaceNeeded += CmdUtil::ContextRegRmwSizeDwords;

    // PM4 packet: sets the following context register: mmVGT_VERTEX_REUSE_BLOCK_CNTL.
    m_statePm4CmdsContext.spaceNeeded +=
        cmdUtil.BuildSetOneContextReg(mmVGT_VERTEX_REUSE_BLOCK_CNTL, &m_statePm4CmdsContext.hdrVgtVertexReuseBlockCntl);

    //        Driver must insert FLUSH_DFSM event whenever the ... channel mask changes (ARGB to RGB)
    //
    // Channel-mask changes refer to the CB_TARGET_MASK register
    m_statePm4CmdsContext.spaceNeeded +=
        cmdUtil.BuildNonSampleEventWrite(FLUSH_DFSM,
                                         EngineTypeUniversal,
                                         &m_statePm4CmdsContext.flushDfsm);

    // 1st PM4 packet for stream-out: sets the following context registers: VGT_STRMOUT_CONFIG and
    // VGT_STRMOUT_BUFFER_CONFIG.
    m_streamoutPm4Cmds.spaceNeeded = cmdUtil.BuildSetSeqContextRegs(mmVGT_STRMOUT_CONFIG,
                                                                    mmVGT_STRMOUT_BUFFER_CONFIG,
                                                                    &m_streamoutPm4Cmds.headerStrmoutCfg);

    if (useStreamOutput)
    {
        // 2nd-5th PM4 packet for stream-out: sets the following context registers: VGT_STRMOUT_VTX_STRIDE_*
        // NOTE: These register writes are unnecessary if stream-out is not active.
        constexpr uint16 VgtStrmoutVtxStride[] = { mmVGT_STRMOUT_VTX_STRIDE_0, mmVGT_STRMOUT_VTX_STRIDE_1,
                                                   mmVGT_STRMOUT_VTX_STRIDE_2, mmVGT_STRMOUT_VTX_STRIDE_3, };

        for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
        {
            m_streamoutPm4Cmds.spaceNeeded +=
                cmdUtil.BuildSetOneContextReg(VgtStrmoutVtxStride[i], &m_streamoutPm4Cmds.stride[i].header);
        }
    }
}

// =====================================================================================================================
// Help method to sets-up RbPlus registers. Return true indicates the registers of Rb+ is set, so the the caller could
// set DISABLE_DUAL_QUAD accordingly during initialization.
bool GraphicsPipeline::SetupRbPlusShaderRegisters(
    const bool            dualBlendEnabled,
    const uint8*          pWriteMask,
    const SwizzledFormat* pSwizzledFormats,
    const uint32*         pTargetIndices,
    const uint32          targetIndexCount,
    RbPlusPm4Img*         pPm4Image
    ) const
{
    uint32 downConvert     = 0;
    uint32 blendOptEpsilon = 0;
    uint32 blendOptControl = 0;
    bool   result          = false;

    PAL_ASSERT((targetIndexCount > 0) &&
               (pSwizzledFormats != nullptr) &&
               (pTargetIndices != nullptr) &&
               (pPm4Image != nullptr));

    if (m_pDevice->Settings().gfx9RbPlusEnable &&
        (dualBlendEnabled == false) &&
        (m_statePm4CmdsContext.cbColorControl.bits.MODE != CB_RESOLVE))
    {
        downConvert     = m_statePm4CmdsContext.sxPsDownconvert.u32All;
        blendOptEpsilon = m_statePm4CmdsContext.sxBlendOptEpsilon.u32All;
        blendOptControl = m_statePm4CmdsContext.sxBlendOptControl.u32All;

        for (uint32 i = 0; i < targetIndexCount; ++i)
        {
            const uint32                bitShift          = pTargetIndices[i] * 4;
            const uint32                numComponents     = Formats::NumComponents(pSwizzledFormats[i].format);
            const uint32                componentMask     = Formats::ComponentMask(pSwizzledFormats[i].format);
            const uint8                 writeMask         = (pWriteMask != nullptr) ?
                                                            pWriteMask[i] : static_cast<uint8>(componentMask);
            const SX_DOWNCONVERT_FORMAT downConvertFormat = SxDownConvertFormat(pSwizzledFormats[i].format);
            const uint32                sxBlendOptControl = SxBlendOptControl(writeMask);

            uint32 sxBlendOptEpsilon = 0;

            if (downConvertFormat != SX_RT_EXPORT_NO_CONVERSION)
            {
                sxBlendOptEpsilon = SxBlendOptEpsilon(downConvertFormat);
            }

            const uint32 blendOptControlMask = SX_BLEND_OPT_CONTROL__MRT0_COLOR_OPT_DISABLE_MASK |
                                               SX_BLEND_OPT_CONTROL__MRT0_ALPHA_OPT_DISABLE_MASK;

            downConvert = downConvert & (~(SX_PS_DOWNCONVERT__MRT0_MASK << bitShift));
            downConvert = downConvert | (downConvertFormat << bitShift);

            blendOptEpsilon = blendOptEpsilon & (~(SX_BLEND_OPT_EPSILON__MRT0_EPSILON_MASK << bitShift));
            blendOptEpsilon = blendOptEpsilon | (sxBlendOptEpsilon << bitShift);

            blendOptControl = blendOptControl & (~(blendOptControlMask << bitShift));
            blendOptControl = blendOptControl | (sxBlendOptControl << bitShift);
        }

        result = true;
    }

    pPm4Image->sxPsDownconvert.u32All   = downConvert;
    pPm4Image->sxBlendOptEpsilon.u32All = blendOptEpsilon;
    pPm4Image->sxBlendOptControl.u32All = blendOptControl;

    pPm4Image->spaceNeeded = m_pDevice->CmdUtil().BuildSetSeqContextRegs(mmSX_PS_DOWNCONVERT,
                                                                         mmSX_BLEND_OPT_CONTROL,
                                                                         &pPm4Image->header);

    return result;
}

// =====================================================================================================================
// Initializes some render-state registers based on the provided HW pixel shader and VS output semantic declarations.
//
// The registers set up in this helper method are generally done the same way for all types of graphics pipelines.
void GraphicsPipeline::InitCommonStateRegisters(
    const GraphicsPipelineCreateInfo& createInfo,
    const AbiProcessor&               abiProcessor)
{
    const Gfx9PalSettings& settings = m_pDevice->Settings();

    m_statePm4CmdsContext.paClClipCntl.u32All = abiProcessor.GetRegisterEntry(mmPA_CL_CLIP_CNTL);
    m_statePm4CmdsContext.paClVteCntl.u32All  = abiProcessor.GetRegisterEntry(mmPA_CL_VTE_CNTL);
    m_statePm4CmdsContext.paSuVtxCntl.u32All  = abiProcessor.GetRegisterEntry(mmPA_SU_VTX_CNTL);
    m_paScModeCntl1.u32All                    = abiProcessor.GetRegisterEntry(mmPA_SC_MODE_CNTL_1);

    if (m_gfxLevel == GfxIpLevel::GfxIp9)
    {
    }

    m_statePm4CmdsContext.vgtShaderStagesEn.u32All = abiProcessor.GetRegisterEntry(mmVGT_SHADER_STAGES_EN);
    m_statePm4CmdsContext.vgtReuseOff.u32All       = abiProcessor.GetRegisterEntry(mmVGT_REUSE_OFF);
    m_spiPsInControl.u32All                        = abiProcessor.GetRegisterEntry(mmSPI_PS_IN_CONTROL);
    m_spiVsOutConfig.u32All                        = abiProcessor.GetRegisterEntry(mmSPI_VS_OUT_CONFIG);

    // NOTE: The following registers are assumed to have the value zero if the pipeline ELF does not specify values.
    abiProcessor.HasRegisterEntry(mmVGT_GS_MODE,      &m_statePm4CmdsContext.vgtGsMode.u32All);
    abiProcessor.HasRegisterEntry(mmVGT_TF_PARAM,     &m_statePm4CmdsContext.vgtTfParam.u32All);
    abiProcessor.HasRegisterEntry(mmVGT_LS_HS_CONFIG, &m_vgtLsHsConfig.u32All);

    // If the number of VS output semantics exceeds the half-pack threshold, then enable VS half-pack mode.  Keep in
    // mind that the number of VS exports are represented by a -1 field in the HW register!
    if ((m_spiVsOutConfig.bits.VS_EXPORT_COUNT + 1u) > settings.vsHalfPackThreshold)
    {
        m_spiVsOutConfig.bits.VS_HALF_PACK = 1;
    }

    if (IsGsEnabled() && (m_statePm4CmdsContext.vgtGsMode.bits.ONCHIP == VgtGsModeOnchip))
    {
        SetIsGsOnChip(true);
    }

    // For Gfx9+, default VTX_REUSE_DEPTH to 14
    m_statePm4CmdsContext.vgtVertexReuseBlockCntl.u32All = 0;
    m_statePm4CmdsContext.vgtVertexReuseBlockCntl.bits.VTX_REUSE_DEPTH = 14;

    if ((settings.vsHalfPackThreshold >= MaxVsExportSemantics) &&
        (m_gfxLevel == GfxIpLevel::GfxIp9))
    {
        // Degenerate primitive filtering with fractional odd tessellation requires a VTX_REUSE_DEPTH of 14. Only
        // override to 30 if we aren't using that feature.
        //
        // VGT_TF_PARAM depends solely on the compiled HS when on-chip GS is disabled, in the future when Tess with
        // on-chip GS is supported, the 2nd condition may need to be revisited.
        if ((m_pDevice->DegeneratePrimFilter() == false) ||
            (IsTessEnabled() && (m_statePm4CmdsContext.vgtTfParam.bits.PARTITIONING != PART_FRAC_ODD)))
        {
            m_statePm4CmdsContext.vgtVertexReuseBlockCntl.bits.VTX_REUSE_DEPTH = 30;
        }
    }

    if (abiProcessor.HasRegisterEntry(mmVGT_STRMOUT_CONFIG, &m_streamoutPm4Cmds.vgtStrmoutConfig.u32All) &&
        (m_streamoutPm4Cmds.vgtStrmoutConfig.u32All != 0))
    {
        for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
        {
            m_streamoutPm4Cmds.stride[i].vgtStrmoutVtxStride.u32All =
                abiProcessor.GetRegisterEntry(mmVGT_STRMOUT_VTX_STRIDE_0 + i);
        }

        m_streamoutPm4Cmds.vgtStrmoutBufferConfig.u32All = abiProcessor.GetRegisterEntry(mmVGT_STRMOUT_BUFFER_CONFIG);
    }

    m_statePm4CmdsContext.cbShaderMask.u32All = abiProcessor.GetRegisterEntry(mmCB_SHADER_MASK);

    m_statePm4CmdsContext.spiInterpControl0.u32All = 0;
    abiProcessor.HasRegisterEntry(mmSPI_INTERP_CONTROL_0, &m_statePm4CmdsContext.spiInterpControl0.u32All);

    m_statePm4CmdsContext.spiInterpControl0.bits.FLAT_SHADE_ENA = (createInfo.rsState.shadeMode == ShadeMode::Flat);
    if (m_statePm4CmdsContext.spiInterpControl0.bits.PNT_SPRITE_ENA != 0) // Point sprite mode is enabled.
    {
        m_statePm4CmdsContext.spiInterpControl0.bits.PNT_SPRITE_TOP_1  =
            (createInfo.rsState.pointCoordOrigin != PointOrigin::UpperLeft);
    }

    // If NGG is enabled, there is no hardware-VS, so there is no need to compute the late-alloc VS limit.
    if (IsNgg() == false)
    {
        SetupLateAllocVs(abiProcessor);
    }

    SetupNonShaderRegisters(createInfo);
    SetupIaMultiVgtParam(abiProcessor);
}

// =====================================================================================================================
// The pipeline binary is allowed to partially specify the value for IA_MULTI_VGT_PARAM.  PAL will finish initializing
// this register based on GPU properties, pipeline create info, and the values of other registers.
void GraphicsPipeline::SetupIaMultiVgtParam(
    const AbiProcessor& abiProcessor)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    const Gfx9PalSettings&   settings  = m_pDevice->Settings();

    regIA_MULTI_VGT_PARAM iaMultiVgtParam = { };
    abiProcessor.HasRegisterEntry(mmIA_MULTI_VGT_PARAM__GFX09, &iaMultiVgtParam.u32All);

    if (IsTessEnabled())
    {
        // The hardware requires that the primgroup size matches the number of HS patches-per-thread-group when
        // tessellation is enabled.
        iaMultiVgtParam.bits.PRIMGROUP_SIZE = m_pDevice->ComputeTessPrimGroupSize(m_vgtLsHsConfig.bits.NUM_PATCHES);
    }
    else if (IsGsEnabled() && (m_vgtLsHsConfig.bits.HS_NUM_INPUT_CP != 0))
    {
        // The hardware requires that the primgroup size must not exceed (256/ number of HS input control points) when
        // a GS shader accepts patch primitives as input.
        iaMultiVgtParam.bits.PRIMGROUP_SIZE =
                m_pDevice->ComputeNoTessPatchPrimGroupSize(m_vgtLsHsConfig.bits.HS_NUM_INPUT_CP);
    }
    else
    {
        // Just use the primitive group size specified by the pipeline binary.  Zero is a valid value here in case
        // the binary didn't specify a value for PRIMGROUP_SIZE.
    }

    if (IsGsEnabled() || IsNgg())
    {
        // NOTE: The hardware will automatically set PARTIAL_ES_WAVE_ON when a user-GS or NGG is active, so we should
        // do the same to track what the chip really sees.
        iaMultiVgtParam.bits.PARTIAL_ES_WAVE_ON = 1;
    }

    for (uint32 idx = 0; idx < NumIaMultiVgtParam; ++idx)
    {
        m_iaMultiVgtParam[idx] = iaMultiVgtParam;

        // Additional setup for this register is required based on whether or not WD_SWITCH_ON_EOP is forced to 1.
        FixupIaMultiVgtParam((idx != 0), &m_iaMultiVgtParam[idx]);

        // NOTE: The PRIMGROUP_SIZE field IA_MULTI_VGT_PARAM must be less than 256 if stream output and
        // PARTIAL_ES_WAVE_ON are both enabled on 2-SE hardware.
        if ((VgtStrmoutConfig().u32All != 0) && (chipProps.gfx9.numShaderEngines == 2))
        {
            if (m_iaMultiVgtParam[idx].bits.PARTIAL_ES_WAVE_ON == 0)
            {
                PAL_ASSERT(m_iaMultiVgtParam[idx].bits.PRIMGROUP_SIZE < 256);
            }

            if ((m_iaMultiVgtParam[idx].bits.EN_INST_OPT_BASIC == 1) ||
                (m_iaMultiVgtParam[idx].bits.EN_INST_OPT_ADV   == 1))
            {
                // The maximum supported setting for IA_MULTI_VGT_PARAM.PRIMGROUP_SIZE with the instancing optimization
                // flowchart enabled is 253.
                PAL_ASSERT(m_iaMultiVgtParam[idx].bits.PRIMGROUP_SIZE < 253);
            }
        }
    }
}

// =====================================================================================================================
// Performs additional validation and setup for IA_MULTI_VGT_PARAM for Gfx7 and newer GPUs.
void GraphicsPipeline::FixupIaMultiVgtParam(
    bool                   forceWdSwitchOnEop,
    regIA_MULTI_VGT_PARAM* pIaMultiVgtParam
    ) const
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    const Gfx9PalSettings&   settings  = m_pDevice->Settings();

    if (IsGsEnabled() || IsNgg())
    {
        // NOTE: The GS table is a storage structure in the hardware.  It keeps track of all outstanding GS waves from
        // creation to dealloc.  When Partial ES Wave is off the VGT combines ES waves across primgroups.  In this case
        // more GS table entries may be needed.  This reserved space ensures the worst case is handled as recommended by
        // VGT HW engineers.
        constexpr uint32 GsTableDepthReservedForEsWave = 3;

        // Preferred number of GS primitives per ES thread.
        constexpr uint32 GsPrimsPerEsThread = 256;

        if ((GsPrimsPerEsThread / (pIaMultiVgtParam->bits.PRIMGROUP_SIZE + 1)) >=
            (chipProps.gfx9.gsVgtTableDepth - GsTableDepthReservedForEsWave))
        {
            // Typically, this case will be hit when tessellation is on because PRIMGROUP_SIZE is set to the number of
            // patches per TG, optimally around 8.  For non-tessellated draws PRIMGROUP_SIZE is set larger.
            pIaMultiVgtParam->bits.PARTIAL_ES_WAVE_ON = 1;
        }
    }

    if (m_statePm4CmdsContext.vgtTfParam.bits.DISTRIBUTION_MODE != NO_DIST)
    {
        // Verify a few assumptions given that distributed tessellation is enabled:
        //     - Tessellation itself is enabled;
        PAL_ASSERT(IsTessEnabled());

        // When distributed tessellation is active, hardware requires PARTIAL_ES_WAVE_ON if the GS is present,
        // and PARTIAL_VS_WAVE_ON when the GS is absent.
        if (IsGsEnabled() || IsNgg())
        {
            pIaMultiVgtParam->bits.PARTIAL_ES_WAVE_ON = 1;
        }
        else
        {
            pIaMultiVgtParam->bits.PARTIAL_VS_WAVE_ON = 1;
        }
    }

    // TODO Pipeline: Revisit this, as this programming can be relaxed due to GFX_DV.27.
    // According to the VGT folks, WD_SWITCH_ON_EOP needs to be set whenever any of the following conditions are met.
    // Furthermore, the hardware will automatically set the bit for any part which has <= 2 shader engines.  Note:
    // PAL does not currently support setting DrawAuto, when implemented this condition must also trigger
    // WD_SWITCH_ON_EOP.

    if ((pIaMultiVgtParam->bits.SWITCH_ON_EOP == 1) || // Illegal to have IA switch VGTs on EOP without WD switch IAs
                                                       // on EOP also.
        (chipProps.gfx9.numShaderEngines <= 2)      || // For 2SE systems, WD_SWITCH_ON_EOP = 1 implicitly
        forceWdSwitchOnEop)                            // External condition (e.g. incompatible prim topology) are
                                                       // requiring WD_SWITCH_ON_EOP.
    {
        pIaMultiVgtParam->bits.WD_SWITCH_ON_EOP = 1;
    }
    else
    {
        pIaMultiVgtParam->bits.WD_SWITCH_ON_EOP = 0;

        // Hardware requires SWITCH_ON_EOI (and therefore PARTIAL_ES_WAVE_ON) to be set whenever WD_SWITCH_ON_EOP is
        // zero.
        pIaMultiVgtParam->bits.SWITCH_ON_EOI            = 1;
        pIaMultiVgtParam->bits.PARTIAL_ES_WAVE_ON       = 1;
    }

    // When SWITCH_ON_EOI is enabled, PARTIAL_VS_WAVE_ON should always be set for certain hardware, and only set for
    // instanced draws on others.
    //
    // TODO: Implement the check for instancing.  This could be done by parsing IL.
    // TODO Pipeline: Add support for VS Partial Wave with EOI Enabled.

    if (VgtStrmoutConfig().u32All != 0)
    {
        pIaMultiVgtParam->bits.PARTIAL_VS_WAVE_ON = 1;
    }

    // Enable WD flowchart optimization.  It is not available if NGG fast-launch is enabled.
    //
    // With basic optimization enabled, the work distributor automatically updates register setting for
    // instanced draws (WD_SWITCH_ON_EOP, SWITCH_ON_EOP and SWITCH_ON_EOI) based on an algorithm. Any draw
    // that has the following will automatically bypass this algorithm.
    //
    //  1. WD_SWITCH_ON_EOP = 1
    //  2. Is using patches (DI_PT_PATCH)
    //  3. Enables dispatch draw with NOT_EOP = 1
    //  4. Is using Opaque draw (i.e., DX10's DrawAuto). PAL currently does not support these.

    //  Hardware WD Load Balancing Algorithm :
    //
    //  if (NumPrimitivesPerInstance > 2 * IA_MULTI_VGT_PARAM.PRIMGROUP_SIZE)
    //  {
    //      if (NumPrimitivesPerInstance < NumShaderEngine * PRIMGROUP_SIZE)
    //      {
    //          IA_MULTI_VGT_PARAM.PRIMGROUP_SIZE = ceil(NumPrimitivesPerInstance / NumShaderEngine);
    //      }
    //      else if ((NumPrimitivesPerInstance < 8 * IA_MULTI_VGT_PARAM.PRIMGROUP_SIZE) &&
    //               (NumPrimitivesPerInstance != 4 * IA_MULTI_VGT_PARAM.PRIMGROUP_SIZE))
    //      {
    //          IA_MULTI_VGT_PARAM.PRIMGROUP_SIZE = ceil(NumPrimitivesPerInstance / 8);
    //      }
    //
    //      // Distribute entire call to All shader engines (4xPrimRate)
    //      IA_MULTI_VGT_PARAM.WD_SWITCH_ON_EOP = 0;
    //      IA_MULTI_VGT_PARAM.SWITCH_ON_EOP    = 0;
    //      IA_MULTI_VGT_PARAM.SWITCH_ON_EOI    = 1;
    //  }
    //  else
    //  {
    //      PRIMGROUP_SIZE = ceil(NumPrimitivesPerInstance / 2);
    //
    //      if (PRIMGROUP_SIZE < VGT_CACHE_INVALIDATION.OPT_FLOW_CNTL_1)
    //      {
    //          if ((NumPrimitivesPerInstance * NumInstances > VGT_CACHE_INVALIDATION.OPT_FLOW_CNTL_2) &&
    //              (NumInstances > 1)                                                                 &&
    //              (IA_MULTI_VGT_PARAM.EN_INST_OPT_ADV))
    //          {
    //              // Split into multiple draw calls
    //              NumInstancesPerSubDraw = floor(2 * IA_MULTI_VGT_PARAM.PRIMGROUP_SIZE /
    //                                             NumPrimitivesPerInstance);
    //
    //              IA_MULTI_VGT_PARAM.WD_SWITCH_ON_EOP = 1;
    //              IA_MULTI_VGT_PARAM.SWITCH_ON_EOP    = 0;
    //              IA_MULTI_VGT_PARAM.SWITCH_ON_EOI    = 0;
    //
    //              // Unroll NumInstances into NumInstancesPerSubDraw units
    //          }
    //          else
    //          {
    //              // Distribute entire draw call to 2 SE (2xPrimRate)
    //              IA_MULTI_VGT_PARAM.WD_SWITCH_ON_EOP = 1;
    //              IA_MULTI_VGT_PARAM.SWITCH_ON_EOP    = 0;
    //              IA_MULTI_VGT_PARAM.SWITCH_ON_EOI    = 0;
    //          }
    //      }
    //      else
    //      {
    //          IA_MULTI_VGT_PARAM.PRIMGROUP_SIZE = PRIMGROUP_SIZE;
    //
    //          // Distribute entire call to All shader engines (4xPrimRate)
    //          IA_MULTI_VGT_PARAM.WD_SWITCH_ON_EOP = 0;
    //          IA_MULTI_VGT_PARAM.SWITCH_ON_EOP    = 0;
    //          IA_MULTI_VGT_PARAM.SWITCH_ON_EOI    = 1;
    //      }
    //  }
    if (IsNggFastLaunch() == 0)
    {
        if (settings.wdLoadBalancingMode == Gfx9WdLoadBalancingBasic)
        {
            // Basic optimization enables small instanced draw optimizations. HW optimally distributes workload
            // across shader engines automatically.
            pIaMultiVgtParam->bits.EN_INST_OPT_BASIC = 1;
        }
        else if (settings.wdLoadBalancingMode == Gfx9WdLoadBalancingAdvanced)
        {
            // Advanced optimization enables basic optimization and additional sub-draw call distribution algorithm
            // which splits batch into smaller instanced draws.
            pIaMultiVgtParam->bits.EN_INST_OPT_ADV = 1;
        }
    }
}

// =====================================================================================================================
// Sets-up some render-state register values which don't depend on the shader portions of the graphics pipeline.
void GraphicsPipeline::SetupNonShaderRegisters(
    const GraphicsPipelineCreateInfo& createInfo)
{
    const Gfx9PalSettings& settings = m_pDevice->Settings();

    m_statePm4CmdsContext.paScLineCntl.u32All                        = 0;
    m_statePm4CmdsContext.paScLineCntl.bits.EXPAND_LINE_WIDTH        = createInfo.rsState.expandLineWidth;
    m_statePm4CmdsContext.paScLineCntl.bits.DX10_DIAMOND_TEST_ENA    = 1;
    m_statePm4CmdsContext.paScLineCntl.bits.LAST_PIXEL               = createInfo.rsState.rasterizeLastLinePixel;
    m_statePm4CmdsContext.paScLineCntl.bits.PERPENDICULAR_ENDCAP_ENA = createInfo.rsState.perpLineEndCapsEnable;

    // CB_TARGET_MASK comes from the RT write masks in the pipeline CB state structure.
    m_statePm4CmdsContext.cbTargetMask.u32All = 0;

    for (uint32 rt = 0; rt < MaxColorTargets; ++rt)
    {
        const uint32 rtShift = (rt * 4); // Each RT uses four bits of CB_TARGET_MASK.
        m_statePm4CmdsContext.cbTargetMask.u32All |=
            ((createInfo.cbState.target[rt].channelWriteMask & 0xF) << rtShift);
    }

    //      The bug manifests itself when an MRT is not enabled in the shader mask but is enabled in the target
    //      mask. It will work fine if the target mask is always a subset of the shader mask
    if (settings.waOverwriteCombinerTargetMaskOnly &&
        (TestAllFlagsSet(m_statePm4CmdsContext.cbShaderMask.u32All,
                         m_statePm4CmdsContext.cbTargetMask.u32All) == false))
    {
        //     What would happen if there was a case like:
        //         Target #    : 3 2 1 0
        //         shader_mask : 0 F 0 F
        //         Target_mask : F 0 0 F
        //
        //     Does the HW have the capability to remap shader output #2 to target #3, or is this an invalid case?
        //
        //     There's what the HW is supposed to do, and what the HW does do.   Due to bugs from long ago that
        //     may have created behavior that people didn't want to move away from, the driver was forced to
        //     reconcile this situation itself and set the two masks the same.
        //
        //     What it was supposed to do (and some HW works this way, but we're not really sure if all of the
        //     HW does), is that the shader mask describes what the shader actually exports and assigns MRT#s to
        //     each of the enabled exports. Any channel that is not exported, but is written is supposed to
        //     default to 1.0 for Alpha, and 0.0 for RGB. The Target Mask is then supposed to suppress writing
        //     anything that is not enabled.  The SX is supposed to look at the shader_mask, set default data,
        //     and assign MRT#s, while the CB is supposed to robustly handle any MRT# it gets and mask them off.
        //
        //     Practically speaking, the CB can't handle having a "target" enabled that there is no export so it
        //     doesn't write anything for them, and I believe it maps shader exports to MRT# based on
        //     TARGET_MASK&SHADER_MASK which then drops any extra exports on the floor, but I could be wrong, so
        //     this may lead to a different export#->MRT# than you're expecting.
        //
        //     I believe the HW will currently write the first shader export to MRT0 and drop the second entirely
        //     instead of dropping MRT2 and blending 1.0,0.0,0.0,0.0 into MRT3.
        PAL_ALERT_ALWAYS();
    }

    m_statePm4CmdsContext.cbColorControl.u32All = 0;

    if (IsFastClearEliminate())
    {
        m_statePm4CmdsContext.cbColorControl.bits.MODE = CB_ELIMINATE_FAST_CLEAR;
        m_statePm4CmdsContext.cbColorControl.bits.ROP3 = Rop3(LogicOp::Copy);

        // NOTE: the CB spec states that for fast-clear eliminate, these registers should be set to enable writes to all
        // four channels of RT #0.
        m_statePm4CmdsContext.cbShaderMask.u32All = 0xF;
        m_statePm4CmdsContext.cbTargetMask.u32All = 0xF;
    }
    else if (IsFmaskDecompress())
    {
        m_statePm4CmdsContext.cbColorControl.bits.MODE = CB_FMASK_DECOMPRESS;
        m_statePm4CmdsContext.cbColorControl.bits.ROP3 = Rop3(LogicOp::Copy);

        // NOTE: the CB spec states that for fmask-decompress, these registers should be set to enable writes to all
        // four channels of RT #0.
        m_statePm4CmdsContext.cbShaderMask.u32All = 0xF;
        m_statePm4CmdsContext.cbTargetMask.u32All = 0xF;
    }
    else if (IsDccDecompress())
    {
        m_statePm4CmdsContext.cbColorControl.bits.MODE = CB_DCC_DECOMPRESS;

        // According to the reg-spec, DCC decompress ops imply fmask decompress and fast-clear eliminate operations as
        // well, so set these registers as they would be set above.
        m_statePm4CmdsContext.cbColorControl.bits.ROP3 = Rop3(LogicOp::Copy);
        m_statePm4CmdsContext.cbShaderMask.u32All      = 0xF;
        m_statePm4CmdsContext.cbTargetMask.u32All      = 0xF;
    }
    else if (IsResolveFixedFunc())
    {
        m_statePm4CmdsContext.cbColorControl.bits.MODE = CB_RESOLVE;

        m_statePm4CmdsContext.cbColorControl.bits.ROP3         = Rop3(LogicOp::Copy);
        m_statePm4CmdsContext.cbShaderMask.bits.OUTPUT0_ENABLE = 0xF;
        m_statePm4CmdsContext.cbTargetMask.bits.TARGET0_ENABLE = 0xF;
    }
    else if ((m_statePm4CmdsContext.cbShaderMask.u32All == 0) || (m_statePm4CmdsContext.cbTargetMask.u32All == 0))
    {
        m_statePm4CmdsContext.cbColorControl.bits.MODE = CB_DISABLE;
    }
    else
    {
        m_statePm4CmdsContext.cbColorControl.bits.MODE = CB_NORMAL;
        m_statePm4CmdsContext.cbColorControl.bits.ROP3 = Rop3(createInfo.cbState.logicOp);
    }

    if (createInfo.cbState.dualSourceBlendEnable)
    {
        // If dual-source blending is enabled and the PS doesn't export to both RT0 and RT1, the hardware might hang.
        // To avoid the hang, just disable CB writes.
        if (((m_statePm4CmdsContext.cbShaderMask.u32All & 0x0F) == 0) ||
            ((m_statePm4CmdsContext.cbShaderMask.u32All & 0xF0) == 0))
        {
            PAL_ALERT_ALWAYS();
            m_statePm4CmdsContext.cbColorControl.bits.MODE = CB_DISABLE;
        }
    }

    const CmdUtil& cmdUtil = m_pDevice->CmdUtil();

    // NOTE: On recommendation from h/ware team FORCE_SHADER_Z_ORDER will be set whenever Re-Z is being used.
    regDB_RENDER_OVERRIDE dbRenderOverride = { };
    dbRenderOverride.bits.FORCE_SHADER_Z_ORDER = (m_chunkPs.DbShaderControl().bits.Z_ORDER == RE_Z);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 374
    // Configure depth clamping
    dbRenderOverride.bits.DISABLE_VIEWPORT_CLAMP = (createInfo.rsState.depthClampDisable == true);

    // Write the PM4 packet to set DB_RENDER_OVERRIDE. Note: both the bitfields FORCE_SHADER_Z_ORDER or
    // FORCE_STENCIL_READ have a default 0 value in the preamble, thus we only need to update these three bitfields.
    constexpr uint32 DbRenderOverrideRmwMask = (DB_RENDER_OVERRIDE__FORCE_SHADER_Z_ORDER_MASK |
                                                DB_RENDER_OVERRIDE__FORCE_STENCIL_READ_MASK |
                                                DB_RENDER_OVERRIDE__DISABLE_VIEWPORT_CLAMP_MASK);
#else
    // Write the PM4 packet to set DB_RENDER_OVERRIDE. Note: both the bitfields FORCE_SHADER_Z_ORDER or
    // FORCE_STENCIL_READ have a default 0 value in the preamble, thus we only need to update these two bitfields.
    constexpr uint32 DbRenderOverrideRmwMask = (DB_RENDER_OVERRIDE__FORCE_SHADER_Z_ORDER_MASK |
                                                DB_RENDER_OVERRIDE__FORCE_STENCIL_READ_MASK);
#endif

    cmdUtil.BuildContextRegRmw(mmDB_RENDER_OVERRIDE,
                               DbRenderOverrideRmwMask,
                               dbRenderOverride.u32All,
                               &m_statePm4CmdsContext.dbRenderOverrideRmw);

    // Handling Rb+ registers as long as Rb+ function is supported regardless of enabled/disabled.
    if (m_pDevice->Parent()->ChipProperties().gfx9.rbPlus)
    {
        uint8          writeMask[MaxColorTargets]       = {};
        SwizzledFormat swizzledFormats[MaxColorTargets] = {};
        uint32         targetIndices[MaxColorTargets]   = {};
        RbPlusPm4Img   pm4Image                         = {};

        for (uint32 i = 0; i < MaxColorTargets; ++i)
        {
            const auto& targetState = createInfo.cbState.target[i];

            writeMask[i]       = targetState.channelWriteMask;
            swizzledFormats[i] = targetState.swizzledFormat;
            targetIndices[i]   = i;
        }

        const bool rbPlusIsSet = SetupRbPlusShaderRegisters(createInfo.cbState.dualSourceBlendEnable,
                                                            writeMask,
                                                            swizzledFormats,
                                                            targetIndices,
                                                            MaxColorTargets,
                                                            &pm4Image);

        m_statePm4CmdsContext.cbColorControl.bits.DISABLE_DUAL_QUAD = !rbPlusIsSet;

        m_statePm4CmdsContext.sxPsDownconvert.u32All   = pm4Image.sxPsDownconvert.u32All;
        m_statePm4CmdsContext.sxBlendOptEpsilon.u32All = pm4Image.sxBlendOptEpsilon.u32All;
        m_statePm4CmdsContext.sxBlendOptControl.u32All = pm4Image.sxBlendOptControl.u32All;
        m_statePm4CmdsContext.header                   = pm4Image.header;
        m_statePm4CmdsContext.spaceNeeded             += pm4Image.spaceNeeded;
    }

    // Override some register settings based on toss points.  These toss points cannot be processed in the hardware
    // independent class because they cannot be overridden by altering the pipeline creation info.
    if (IsInternal() == false)
    {
        switch (settings.tossPointMode)
        {
        case TossPointAfterPs:
            // This toss point is used to disable all color buffer writes.
            m_statePm4CmdsContext.cbTargetMask.u32All = 0;
            break;
        default:
            break;
        }
    }

    // Overrides some of the fields in PA_SC_MODE_CNTL1 to account for GPU pipe config and features like out-of-order
    // rasterization.

    // The maximum value for OUT_OF_ORDER_WATER_MARK is 7
    constexpr uint32 MaxOutOfOrderWatermark = 7;
    m_paScModeCntl1.bits.OUT_OF_ORDER_WATER_MARK = Min(MaxOutOfOrderWatermark, settings.outOfOrderWatermark);

    if (createInfo.rsState.outOfOrderPrimsEnable &&
        (settings.enableOutOfOrderPrimitives != Gfx9OutOfOrderPrimDisable))
    {
        m_paScModeCntl1.bits.OUT_OF_ORDER_PRIMITIVE_ENABLE = 1;
    }

    // Hardware team recommendation is to set WALK_FENCE_SIZE to 512 pixels for 4/8/16 pipes and 256 pixels
    // for 2 pipes.
    m_paScModeCntl1.bits.WALK_FENCE_SIZE = ((m_pDevice->GetNumPipesLog2() <= 1) ? 2 : 3);
}

// =====================================================================================================================
// Sets-up the SPI_SHADER_LATE_ALLOC_VS on Gfx9
void GraphicsPipeline::SetupLateAllocVs(
    const AbiProcessor& abiProcessor)
{
    const auto pPalSettings = m_pDevice->Parent()->GetPublicSettings();

    regSPI_SHADER_PGM_RSRC1_VS spiShaderPgmRsrc1Vs = { };
    spiShaderPgmRsrc1Vs.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC1_VS);

    SpiShaderPgmRsrc2Vs spiShaderPgmRsrc2Vs = { };
    spiShaderPgmRsrc2Vs.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC2_VS);

    SpiShaderPgmRsrc2Ps spiShaderPgmRsrc2Ps = { };
    spiShaderPgmRsrc2Ps.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC2_PS);

    // Default to a late-alloc limit of zero.  This will nearly mimic the GFX6 behavior where VS waves don't launch
    // without allocating export space.
    uint32 lateAllocLimit = 0;

    const auto& chipInfo = m_pDevice->Parent()->ChipProperties();
    const auto& gpuInfo = chipInfo.gfx9;

    // Maximum value of the LIMIT field of the SPI_SHADER_LATE_ALLOC_VS register
    // It is the number of wavefronts minus one.
    const uint32 maxLateAllocLimit = chipInfo.gfxip.maxLateAllocVsLimit - 1;

    // Target late-alloc limit uses PAL settings by default. The lateAllocVsLimit member from graphicsPipeline
    // can override this setting if corresponding flag is set.
    uint32 targetLateAllocLimit = IsLateAllocVsLimit() ? GetLateAllocVsLimit() : m_pDevice->LateAllocVsLimit();

    const uint32 vsNumSgpr = (spiShaderPgmRsrc1Vs.bits.SGPRS * 8);
    const uint32 vsNumVgpr = (spiShaderPgmRsrc1Vs.bits.VGPRS * 4);

    if (m_pDevice->UseFixedLateAllocVsLimit())
    {
        lateAllocLimit = m_pDevice->LateAllocVsLimit();
    }
    else if ((targetLateAllocLimit > 0) && (vsNumSgpr > 0) && (vsNumVgpr > 0))
    {
        // Start by assuming the target late-alloc limit will be acceptable.  The limit is per SH and we need to
        // determine the maximum number of HW-VS wavefronts which can be launched per SH based on the shader's
        // resource usage.
        lateAllocLimit = targetLateAllocLimit;

        // NOTE: Late_Alloc_VS as a feature is CI+, and Carrizo is the only asic that we know has issue caused by
        // side effect of LBPG and its setting should be on the "always on" CUs basis. If any GFX9 ASIC has the
        // same issue as Carrizo, we need to add the same control setting LateAllocVsOnCuAlwaysOn and set it true.

        uint32 numCuForLateAllocVs = gpuInfo.numCuPerSh;

        // Compute the maximum number of HW-VS wavefronts that can launch per SH, based on GPR usage.
        const uint32 simdPerSh = (numCuForLateAllocVs * NumSimdPerCu);
        const uint32 maxSgprVsWaves = (gpuInfo.numPhysicalSgprs / vsNumSgpr) * simdPerSh;
        const uint32 maxVgprVsWaves = (gpuInfo.numPhysicalVgprs / vsNumVgpr) * simdPerSh;

        uint32 maxVsWaves = Min(maxSgprVsWaves, maxVgprVsWaves);

        // Find the maximum number of VS waves that can be launched based on scratch usage if both the PS and VS use
        // scratch.
        if ((spiShaderPgmRsrc2Vs.gfx9.bits.SCRATCH_EN != 0) && (spiShaderPgmRsrc2Ps.gfx9.bits.SCRATCH_EN != 0))
        {
            // The maximum number of waves per SH that can launch using scratch is the number of CUs per SH times
            // the setting that clamps the maximum number of in-flight scratch waves.
            const uint32 maxScratchWavesPerSh = numCuForLateAllocVs * pPalSettings->numScratchWavesPerCu;

            maxVsWaves = Min(maxVsWaves, maxScratchWavesPerSh);
        }

        // Clamp the number of waves that are permitted to launch with late alloc to be one less than the maximum
        // possible number of VS waves that can launch.  This is done to prevent the late-alloc VS waves from
        // deadlocking with the PS.
        if (maxVsWaves <= lateAllocLimit)
        {
            lateAllocLimit = ((maxVsWaves > 1) ? (maxVsWaves - 1) : 1);
        }

        // The late alloc setting is the number of wavefronts minus one.  On GFX7+ at least one VS wave always can
        // launch with late alloc enabled.
        lateAllocLimit -= 1;
    }

    const uint32 programmedLimit = Min(lateAllocLimit, maxLateAllocLimit);
    if (m_gfxLevel == GfxIpLevel::GfxIp9)
    {
        m_statePm4CmdsSh.lateAlloc.gfx9.spiShaderLateAllocVs.bits.LIMIT = programmedLimit;
    }
}

// =====================================================================================================================
// Updates the device that this pipeline has some new ring-size requirements.
void GraphicsPipeline::UpdateRingSizes(
    const AbiProcessor& abiProcessor)
{
    const Gfx9PalSettings& settings = m_pDevice->Settings();

    ShaderRingItemSizes ringSizes = { };

    if (IsGsEnabled())
    {
        ringSizes.itemSize[static_cast<size_t>(ShaderRingType::GsVs)] = m_chunkGs.GsVsRingItemSize();
    }

    if (IsTessEnabled())
    {
        // NOTE: the TF buffer is special: we only need to specify any nonzero item-size because its a fixed-size ring
        // whose size doesn't depend on the item-size at all.
        ringSizes.itemSize[static_cast<size_t>(ShaderRingType::TfBuffer)] = 1;

        // NOTE: the off-chip LDS buffer's item-size refers to the "number of buffers" that the hardware uses (i.e.,
        // VGT_HS_OFFCHIP_PARAM::OFFCHIP_BUFFERING).
        ringSizes.itemSize[static_cast<size_t>(ShaderRingType::OffChipLds)] = settings.numOffchipLdsBuffers;
    }

    ringSizes.itemSize[static_cast<size_t>(ShaderRingType::GfxScratch)] = ComputeScratchMemorySize(abiProcessor);

    // Inform the device that this pipeline has some new ring-size requirements.
    m_pDevice->UpdateLargestRingSizes(&ringSizes);
}

// =====================================================================================================================
// Calculates the maximum scratch memory in dwords necessary by checking the scratch memory needed for each shader.
uint32 GraphicsPipeline::ComputeScratchMemorySize(
    const AbiProcessor& abiProcessor
    ) const
{
    uint32 scratchMemorySizeBytes = 0;
    abiProcessor.HasPipelineMetadataEntry(Abi::PipelineMetadataType::PsScratchByteSize, &scratchMemorySizeBytes);

    uint32 tempScratchSizeBytes = 0;
    if (abiProcessor.HasPipelineMetadataEntry(Abi::PipelineMetadataType::VsScratchByteSize, &tempScratchSizeBytes))
    {
        scratchMemorySizeBytes = Max(scratchMemorySizeBytes, tempScratchSizeBytes);
    }

    tempScratchSizeBytes = 0;
    if (abiProcessor.HasPipelineMetadataEntry(Abi::PipelineMetadataType::HsScratchByteSize, &tempScratchSizeBytes))
    {
        scratchMemorySizeBytes = Max(scratchMemorySizeBytes, tempScratchSizeBytes);
    }

    tempScratchSizeBytes = 0;
    if (abiProcessor.HasPipelineMetadataEntry(Abi::PipelineMetadataType::GsScratchByteSize, &tempScratchSizeBytes))
    {
        scratchMemorySizeBytes = Max(scratchMemorySizeBytes, tempScratchSizeBytes);
    }

    return scratchMemorySizeBytes / sizeof(uint32);
}

// =====================================================================================================================
// Internal function used to obtain shader stats using the given shader mem image.
Result GraphicsPipeline::GetShaderStats(
    ShaderType   shaderType,
    ShaderStats* pShaderStats,
    bool         getDissassemblySize
    ) const
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    PAL_ASSERT(pShaderStats != nullptr);
    Result result = Result::ErrorUnavailable;

    const ShaderStageInfo*const pStageInfo = GetShaderStageInfo(shaderType);
    if (pStageInfo != nullptr)
    {
        const ShaderStageInfo*const pStageInfoCopy =
            ((shaderType == ShaderType::Geometry) && (IsNgg() == false)) ? &m_chunkGs.StageInfoCopy() : nullptr;

        result = GetShaderStatsForStage(*pStageInfo, pStageInfoCopy, pShaderStats);
        if (result == Result::Success)
        {
            pShaderStats->shaderStageMask = (1 << static_cast<uint32>(shaderType));
            pShaderStats->palShaderHash   = m_info.shader[static_cast<uint32>(shaderType)].hash;
            pShaderStats->shaderOperations.writesUAV =
                m_shaderMetaData.flags[static_cast<uint32>(shaderType)].writesUav;

            pShaderStats->common.ldsSizePerThreadGroup = chipProps.gfxip.ldsSizePerThreadGroup;

            switch (pStageInfo->stageId)
            {
            case Abi::HardwareStage::Hs:
                pShaderStats->shaderStageMask       = (ApiShaderStageHull | ApiShaderStageVertex);
                pShaderStats->common.gpuVirtAddress = m_chunkHs.LsProgramGpuVa();
                break;
            case Abi::HardwareStage::Gs:
                pShaderStats->shaderStageMask =
                    (ApiShaderStageGeometry | (IsTessEnabled() ? ApiShaderStageDomain : ApiShaderStageVertex));
                pShaderStats->common.gpuVirtAddress = m_chunkGs.EsProgramGpuVa();
                if (IsNgg() == false)
                {
                    pShaderStats->copyShader.gpuVirtAddress        = m_chunkGs.VsProgramGpuVa();
                    pShaderStats->copyShader.ldsSizePerThreadGroup = chipProps.gfxip.ldsSizePerThreadGroup;
                }
                break;
            case Abi::HardwareStage::Vs:
                pShaderStats->shaderStageMask       = (IsTessEnabled() ? ApiShaderStageDomain : ApiShaderStageVertex);
                pShaderStats->common.gpuVirtAddress = m_chunkVs.VsProgramGpuVa();
                break;
            case Abi::HardwareStage::Ps:
                pShaderStats->shaderStageMask       = ApiShaderStagePixel;
                pShaderStats->common.gpuVirtAddress = m_chunkPs.PsProgramGpuVa();
                break;
            default:
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// This function returns the SPI_SHADER_USER_DATA_x_0 register offset where 'x' is the HW shader execution stage that
// runs the vertex shader.
uint32 GraphicsPipeline::GetVsUserDataBaseOffset() const
{
    uint32 regBase = 0;

    if (IsTessEnabled())
    {
        regBase = m_pDevice->GetBaseUserDataReg(HwShaderStage::Hs);
    }
    else if (IsNgg() || IsGsEnabled())
    {
        regBase = m_pDevice->GetBaseUserDataReg(HwShaderStage::Gs);
    }
    else
    {
        regBase = mmSPI_SHADER_USER_DATA_VS_0;
    }

    return regBase;
}

// =====================================================================================================================
// Initializes the signature for a single stage within a graphics pipeline using a pipeline ELF.
void GraphicsPipeline::SetupSignatureForStageFromElf(
    const AbiProcessor& abiProcessor,
    HwShaderStage       stage)
{
    const uint16 baseRegAddr = m_pDevice->GetBaseUserDataReg(stage);
    const uint16 lastRegAddr = (baseRegAddr + 31);

    const uint32 stageId = static_cast<uint32>(stage);
    auto*const   pStage  = &m_signature.stage[stageId];

    for (uint16 offset = baseRegAddr; offset <= lastRegAddr; ++offset)
    {
        uint32 value = 0;
        if (abiProcessor.HasRegisterEntry(offset, &value))
        {
            if (value < MaxUserDataEntries)
            {
                pStage->regAddr[value] = offset;
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::GlobalTable))
            {
                PAL_ASSERT(offset == (baseRegAddr + InternalTblStartReg));
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::PerShaderTable))
            {
                PAL_ASSERT(offset == (baseRegAddr + ConstBufTblStartReg));
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::SpillTable))
            {
                pStage->spillTableRegAddr = offset;
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::Workgroup))
            {
                PAL_ALERT_ALWAYS(); // These are for compute pipelines only!
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::GdsRange))
            {
                PAL_ALERT_ALWAYS(); // This is only expected for compute pipelines on Gfx9+!
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::BaseVertex))
            {
                // There can be only base-vertex user-SGPR per pipeline.
                PAL_ASSERT((m_signature.vertexOffsetRegAddr == offset) ||
                           (m_signature.vertexOffsetRegAddr == UserDataNotMapped));
                m_signature.vertexOffsetRegAddr = offset;
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::BaseInstance))
            {
                // There can be only base-vertex user-SGPR per pipeline.  It immediately follows the base vertex
                // user-SGPR.
                PAL_ASSERT((m_signature.vertexOffsetRegAddr == (offset - 1)) ||
                           (m_signature.vertexOffsetRegAddr == UserDataNotMapped));
                m_signature.vertexOffsetRegAddr = (offset - 1);
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::DrawIndex))
            {
                // There can be only draw-index user-SGPR per pipeline.
                PAL_ASSERT((m_signature.drawIndexRegAddr == offset) ||
                           (m_signature.drawIndexRegAddr == UserDataNotMapped));
                m_signature.drawIndexRegAddr = offset;
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::EsGsLdsSize))
            {
                if (stage == HwShaderStage::Gs)
                {
                    m_signature.esGsLdsSizeRegAddrGs = offset;
                }
                else if (stage == HwShaderStage::Vs)
                {
                    m_signature.esGsLdsSizeRegAddrVs = offset;
                }
                else
                {
                    PAL_NEVER_CALLED(); // PS and HS cannot reference the ES/GS LDS ring size!
                }
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::BaseIndex))
            {
                // There can be only start-index user-SGPR per pipeline.
                PAL_ASSERT((m_signature.startIndexRegAddr == offset) ||
                           (m_signature.startIndexRegAddr == UserDataNotMapped));
                m_signature.startIndexRegAddr = offset;
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::Log2IndexSize))
            {
                // There can be only log2-index-size user-SGPR per pipeline.
                PAL_ASSERT((m_signature.log2IndexSizeRegAddr == offset) ||
                           (m_signature.log2IndexSizeRegAddr == UserDataNotMapped));
                m_signature.log2IndexSizeRegAddr = offset;
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::ViewId))
            {
                m_signature.viewIdRegAddr[stageId] = offset;
            }
            else
            {
                // This appears to be an illegally-specified user-data register!
                PAL_NEVER_CALLED();
            }
        } // If HasRegisterEntry()
    } // For each user-SGPR

    // Compute a hash of the regAddr array and spillTableRegAddr for the CS stage.
    constexpr uint64 HashedDataLength = (sizeof(pStage->regAddr) + sizeof(pStage->spillTableRegAddr));

    MetroHash64::Hash(
        reinterpret_cast<const uint8*>(pStage->regAddr),
        HashedDataLength,
        reinterpret_cast<uint8* const>(&pStage->userDataHash));
}

// =====================================================================================================================
// Initializes the signature of a graphics pipeline using a pipeline ELF.
void GraphicsPipeline::SetupSignatureFromElf(
    const AbiProcessor& abiProcessor)
{
    if (IsTessEnabled())
    {
        SetupSignatureForStageFromElf(abiProcessor, HwShaderStage::Hs);
    }
    if (IsGsEnabled() || IsNgg())
    {
        SetupSignatureForStageFromElf(abiProcessor, HwShaderStage::Gs);
    }
    if (IsNgg() == false)
    {
        SetupSignatureForStageFromElf(abiProcessor, HwShaderStage::Vs);
    }
    SetupSignatureForStageFromElf(abiProcessor, HwShaderStage::Ps);

    uint32 value = 0;
    if (abiProcessor.HasPipelineMetadataEntry(Abi::PipelineMetadataType::StreamOutTableEntry, &value))
    {
        m_signature.streamOutTableAddr = static_cast<uint16>(value);
    }

    // Indirect user-data table(s):
    for (uint32 i = 0; i < MaxIndirectUserDataTables; ++i)
    {
        const auto entryType = static_cast<Abi::PipelineMetadataType>(
                static_cast<uint32>(Abi::PipelineMetadataType::IndirectTableEntryLow) + i);

        if (abiProcessor.HasPipelineMetadataEntry(entryType, &value))
        {
            m_signature.indirectTableAddr[i] = static_cast<uint16>(value);
        }
    }

    if (abiProcessor.HasPipelineMetadataEntry(Abi::PipelineMetadataType::SpillThreshold, &value))
    {
        m_signature.spillThreshold = static_cast<uint16>(value);
    }

    if (abiProcessor.HasPipelineMetadataEntry(Abi::PipelineMetadataType::UserDataLimit, &value))
    {
        m_signature.userDataLimit = static_cast<uint16>(value);
    }

    // Finally, compact the array of view ID register addresses
    // so that all of the mapped ones are at the front of the array.
    PackArray(m_signature.viewIdRegAddr, UserDataNotMapped);
}

// =====================================================================================================================
// Converts the specified logic op enum into a ROP3 code (for programming CB_COLOR_CONTROL).
static uint8 Rop3(
    LogicOp logicOp)
{
    uint8 rop3 = 0xCC;

    constexpr uint8 Rop3Codes[] =
    {
        0xCC, // Copy (S)
        0x00, // Clear (clear to 0)
        0x88, // And (S & D)
        0x44, // AndReverse (S & (~D))
        0x22, // AndInverted ((~S) & D)
        0xAA, // Noop (D)
        0x66, // Xor (S ^ D)
        0xEE, // Or (S | D)
        0x11, // Nor (~(S | D))
        0x99, // Equiv (~(S ^ D))
        0x55, // Invert (~D)
        0xDD, // OrReverse (S | (~D))
        0x33, // CopyInverted (~S)
        0xBB, // OrInverted ((~S) | D)
        0x77, // Nand (~(S & D))
        0xFF  // Set (set to 1)
    };

    return Rop3Codes[static_cast<uint32>(logicOp)];
}

// =====================================================================================================================
// Returns the SX "downconvert" format with respect to the channel format of the color buffer target.
// This method is for the RbPlus feature which is identical to the gfx8.1 implementation.
static SX_DOWNCONVERT_FORMAT SxDownConvertFormat(
    ChNumFormat format)
{
    SX_DOWNCONVERT_FORMAT sxDownConvertFormat = SX_RT_EXPORT_NO_CONVERSION;

    switch (format)
    {
    case ChNumFormat::X4Y4Z4W4_Unorm:
    case ChNumFormat::X4Y4Z4W4_Uscaled:
        sxDownConvertFormat = SX_RT_EXPORT_4_4_4_4;
        break;
    case ChNumFormat::X5Y6Z5_Unorm:
    case ChNumFormat::X5Y6Z5_Uscaled:
        sxDownConvertFormat = SX_RT_EXPORT_5_6_5;
        break;
    case ChNumFormat::X5Y5Z5W1_Unorm:
    case ChNumFormat::X5Y5Z5W1_Uscaled:
        sxDownConvertFormat = SX_RT_EXPORT_1_5_5_5;
        break;
    case ChNumFormat::X8_Unorm:
    case ChNumFormat::X8_Snorm:
    case ChNumFormat::X8_Uscaled:
    case ChNumFormat::X8_Sscaled:
    case ChNumFormat::X8_Uint:
    case ChNumFormat::X8_Sint:
    case ChNumFormat::X8_Srgb:
    case ChNumFormat::L8_Unorm:
    case ChNumFormat::P8_Uint:
    case ChNumFormat::X8Y8_Unorm:
    case ChNumFormat::X8Y8_Snorm:
    case ChNumFormat::X8Y8_Uscaled:
    case ChNumFormat::X8Y8_Sscaled:
    case ChNumFormat::X8Y8_Uint:
    case ChNumFormat::X8Y8_Sint:
    case ChNumFormat::X8Y8_Srgb:
    case ChNumFormat::L8A8_Unorm:
    case ChNumFormat::X8Y8Z8W8_Unorm:
    case ChNumFormat::X8Y8Z8W8_Snorm:
    case ChNumFormat::X8Y8Z8W8_Uscaled:
    case ChNumFormat::X8Y8Z8W8_Sscaled:
    case ChNumFormat::X8Y8Z8W8_Uint:
    case ChNumFormat::X8Y8Z8W8_Sint:
    case ChNumFormat::X8Y8Z8W8_Srgb:
        sxDownConvertFormat = SX_RT_EXPORT_8_8_8_8;
        break;
    case ChNumFormat::X11Y11Z10_Float:
        sxDownConvertFormat = SX_RT_EXPORT_10_11_11;
        break;
    case ChNumFormat::X10Y10Z10W2_Unorm:
    case ChNumFormat::X10Y10Z10W2_Uscaled:
        sxDownConvertFormat = SX_RT_EXPORT_2_10_10_10;
        break;
    case ChNumFormat::X16_Unorm:
    case ChNumFormat::X16_Snorm:
    case ChNumFormat::X16_Uscaled:
    case ChNumFormat::X16_Sscaled:
    case ChNumFormat::X16_Uint:
    case ChNumFormat::X16_Sint:
    case ChNumFormat::X16_Float:
    case ChNumFormat::L16_Unorm:
        sxDownConvertFormat = SX_RT_EXPORT_16_16_AR;
        break;
    case ChNumFormat::X16Y16_Unorm:
    case ChNumFormat::X16Y16_Snorm:
    case ChNumFormat::X16Y16_Uscaled:
    case ChNumFormat::X16Y16_Sscaled:
    case ChNumFormat::X16Y16_Uint:
    case ChNumFormat::X16Y16_Sint:
    case ChNumFormat::X16Y16_Float:
        sxDownConvertFormat = SX_RT_EXPORT_16_16_GR;
        break;
    case ChNumFormat::X32_Uint:
    case ChNumFormat::X32_Sint:
    case ChNumFormat::X32_Float:
        sxDownConvertFormat = SX_RT_EXPORT_32_R;
        break;
    default:
        break;
    }

    return sxDownConvertFormat;
}

// =====================================================================================================================
// Get the sx-blend-opt-epsilon with respect to SX "downconvert" format.
// This method is for the RbPlus feature which is identical to the gfx8.1 implementation.
static uint32 SxBlendOptEpsilon(
    SX_DOWNCONVERT_FORMAT sxDownConvertFormat)
{
    uint32 sxBlendOptEpsilon = 0;

    switch (sxDownConvertFormat)
    {
    case SX_RT_EXPORT_32_R:
    case SX_RT_EXPORT_32_A:
    case SX_RT_EXPORT_16_16_GR:
    case SX_RT_EXPORT_16_16_AR:
    case SX_RT_EXPORT_10_11_11: // 1 is recommended, but doesn't provide sufficient precision
        sxBlendOptEpsilon = 0;
        break;
    case SX_RT_EXPORT_2_10_10_10:
        sxBlendOptEpsilon = 3;
        break;
    case SX_RT_EXPORT_8_8_8_8:  // 7 is recommended, but doesn't provide sufficient precision
        sxBlendOptEpsilon = 6;
        break;
    case SX_RT_EXPORT_5_6_5:
        sxBlendOptEpsilon = 11;
        break;
    case SX_RT_EXPORT_1_5_5_5:
        sxBlendOptEpsilon = 13;
        break;
    case SX_RT_EXPORT_4_4_4_4:
        sxBlendOptEpsilon = 15;
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return sxBlendOptEpsilon;
}

// =====================================================================================================================
// Get the SX blend opt control with respect to the specified writemask.
// This method is for the RbPlus feature which is identical to the gfx8.1 implementation.
static uint32 SxBlendOptControl(
    uint32 writeMask)
{
    constexpr uint32 AlphaMask = 0x8;
    constexpr uint32 ColorMask = 0x7;

    const uint32 colorOptDisable = ((writeMask & ColorMask) != 0) ?
        0 : SX_BLEND_OPT_CONTROL__MRT0_COLOR_OPT_DISABLE_MASK;

    const uint32 alphaOptDisable = ((writeMask & AlphaMask) != 0) ?
        0 : SX_BLEND_OPT_CONTROL__MRT0_ALPHA_OPT_DISABLE_MASK;

    return (colorOptDisable | alphaOptDisable);
}

// =====================================================================================================================
// Returns true when the pixel shader culls pixel fragments.
bool GraphicsPipeline::PsTexKill() const
{
    const auto& dbShaderControl = m_chunkPs.DbShaderControl().bits;

    return dbShaderControl.KILL_ENABLE || dbShaderControl.MASK_EXPORT_ENABLE || dbShaderControl.COVERAGE_TO_MASK_ENABLE;
}

// =====================================================================================================================
// Returns true when the alpha to mask is enabled. The DB_SHADER_CONTROL::ALPHA_TO_MASK_DISABLE bit controls whether
// or not the MsaaState's DB_ALPHA_TO_MASK::ALPHA_TO_MASK_ENABLE bit works. When ALPHA_TO_MASK_DISABLE is true, the
// MsaaState's ALPHA_TO_MASK_ENABLE bit is disabled. We need to know this when considering PBB optimizations.
bool GraphicsPipeline::IsAlphaToMaskEnable() const
{
    const auto& dbShaderControl = m_chunkPs.DbShaderControl().bits;

    return (dbShaderControl.ALPHA_TO_MASK_DISABLE == 0);
}

// =====================================================================================================================
bool GraphicsPipeline::PsCanTriviallyReject() const
{
    const auto& dbShaderControl = m_chunkPs.DbShaderControl();

    return ((dbShaderControl.bits.Z_EXPORT_ENABLE == 0) || (dbShaderControl.bits.CONSERVATIVE_Z_EXPORT > 0));
}

// =====================================================================================================================
bool GraphicsPipeline::PsAllowsPunchout() const
{
    const auto& dbShaderControl = m_chunkPs.DbShaderControl();

    return (m_statePm4CmdsContext.cbShaderMask.u32All != 0) &&
           (dbShaderControl.bits.KILL_ENABLE == 0)          &&
           (dbShaderControl.bits.EXEC_ON_HIER_FAIL == 0)    &&
           (dbShaderControl.bits.EXEC_ON_NOOP == 0)         &&
           (dbShaderControl.bits.Z_ORDER == EARLY_Z_THEN_LATE_Z);
}

// =====================================================================================================================
// Updates the NGG Primitive Constant Buffer with the values from this pipeline.
void GraphicsPipeline::UpdateNggPrimCb(
    Abi::PrimShaderPsoCb* pPrimShaderCb
    ) const
{
    pPrimShaderCb->paClVteCntl  = m_statePm4CmdsContext.paClVteCntl.u32All;
    pPrimShaderCb->paSuVtxCntl  = m_statePm4CmdsContext.paSuVtxCntl.u32All;
    pPrimShaderCb->paClClipCntl = m_statePm4CmdsContext.paClClipCntl.u32All;
}

// =====================================================================================================================
// Build the RbPlus related commands for the specified targetIndex target according to the new swizzledFormat.
void GraphicsPipeline::BuildRbPlusRegistersForRpm(
    SwizzledFormat swizzledFormat,
    uint32         targetIndex,
    RbPlusPm4Img*  pPm4Image
    ) const
{
    const SwizzledFormat*const pTargetFormats = TargetFormats();

    if ((pTargetFormats[targetIndex].format != swizzledFormat.format) &&
        (m_statePm4CmdsContext.cbColorControl.bits.DISABLE_DUAL_QUAD == 0) &&
        m_pDevice->Parent()->ChipProperties().gfx9.rbPlus)
    {
        SetupRbPlusShaderRegisters(false,
                                   nullptr,
                                   &swizzledFormat,
                                   &targetIndex,
                                   1,
                                   pPm4Image);

    }
}

// =====================================================================================================================
// Return if hardware stereo rendering is enabled.
bool GraphicsPipeline::HwStereoRenderingEnabled() const
{
    bool hwStereoRenderingEnabled = false;

    if (m_gfxLevel == GfxIpLevel::GfxIp9)
    {
    }
    return hwStereoRenderingEnabled;
}

// =====================================================================================================================
// Return if hardware stereo rendering uses multiple viewports.
bool GraphicsPipeline::HwStereoRenderingUsesMultipleViewports() const
{
    bool usesMultipleViewports = false;

    if (m_gfxLevel == GfxIpLevel::GfxIp9)
    {
    }
    return usesMultipleViewports;
}

// =====================================================================================================================
// Setup hw stereo rendering related registers, this must be done after signature is initialized.
void GraphicsPipeline::SetupStereoRegisters()
{
    const ViewInstancingDescriptor& viewInstancingDesc = GetViewInstancingDesc();

    if (viewInstancingDesc.viewInstanceCount > 1)
    {
        bool viewInstancingEnable = false;

        for (uint32 i = 0; i < NumHwShaderStagesGfx; i++)
        {
            if (m_signature.viewIdRegAddr[i] != UserDataNotMapped)
            {
                viewInstancingEnable = true;
                break;
            }
        }

        if (viewInstancingEnable == false)
        {
            PAL_ASSERT(viewInstancingDesc.viewInstanceCount == 2);
            PAL_ASSERT(viewInstancingDesc.enableMasking == false);

            if (m_gfxLevel == GfxIpLevel::GfxIp9)
            {
            }
        }
    }
}

} // Gfx9
} // Pal
