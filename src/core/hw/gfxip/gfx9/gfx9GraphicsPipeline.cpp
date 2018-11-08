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
    { 0, },                     // User-data mapping for each shader stage
    { UserDataNotMapped, },     // Indirect user-data table mapping
    UserDataNotMapped,          // Stream-out table mapping
    UserDataNotMapped,          // Stream-out table user-SGPR address
    UserDataNotMapped,          // Vertex offset register address
    UserDataNotMapped,          // Draw ID register address
    UserDataNotMapped,          // Start Index register address
    UserDataNotMapped,          // Log2(sizeof(indexType)) register address
    UserDataNotMapped,          // ES/GS LDS size register address
    UserDataNotMapped,          // ES/GS LDS size register address
    NoUserDataSpilling,         // Spill threshold
    0,                          // User-data entry limit
    { UserDataNotMapped, },     // Compacted view ID register addresses
    { UserDataNotMapped, },     // Performance data address for each shader stage
    { 0, },                     // User-data mapping hashes per-stage
};
static_assert(UserDataNotMapped == 0, "Unexpected value for indicating unmapped user-data entries!");

static uint8 Rop3(LogicOp logicOp);
static uint32 SxBlendOptEpsilon(SX_DOWNCONVERT_FORMAT sxDownConvertFormat);
static uint32 SxBlendOptControl(uint32 writeMask);

// Base count of SH registers which are loaded using LOAD_SH_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedShRegCount =
    1;  // mmSPI_SHADER_LATE_ALLOC_VS

// Base count of Context registers which are loaded using LOAD_CNTX_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedCntxRegCount =
    1 + // mmVGT_SHADER_STAGES_EN
    1 + // mmVGT_GS_MODE
    1 + // mmVGT_REUSE_OFF
    1 + // mmVGT_TF_PARAM
    1 + // mmCB_COLOR_CONTROL
    1 + // mmCB_TARGET_MASK
    1 + // mmCB_SHADER_MASK
    1 + // mmPA_CL_CLIP_CNTL
    1 + // mmPA_SU_VTX_CNTL
    1 + // mmPA_CL_VTE_CNTL
    1 + // mmPA_SC_LINE_CNTL
    0 + // mmPA_STEREO_CNTL is not included because it is not present on all HW
    0 + // mmVGT_GS_ONCHIP_CNTL is not included because it is not required for all pipeline types.
    1 + // mmSPI_INTERP_CONTROL_0
    1;  // mmVGT_VERTEX_REUSE_BLOCK_CNTL

// =====================================================================================================================
// Determines whether we can allow the hardware to render out-of-order primitives.  This is done by determing the
// effects that this could have on the depth buffer, stencil buffer, and render target.
bool GraphicsPipeline::CanDrawPrimsOutOfOrder(
    const DepthStencilView*  pDsView,
    const DepthStencilState* pDepthStencilState,
    const ColorBlendState*   pBlendState,
    uint32                   hasActiveQueries,
    OutOfOrderPrimMode       gfx9EnableOutOfOrderPrimitives
    ) const
{
    bool enableOutOfOrderPrims = true;

    if ((gfx9EnableOutOfOrderPrimitives == OutOfOrderPrimSafe) ||
        (gfx9EnableOutOfOrderPrimitives == OutOfOrderPrimAggressive))
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
                const bool isDepthWriteEnabled = (pDsView->ReadOnlyDepth() == false) &&
                                                 (pDepthStencilState->IsDepthWriteEnabled());

                const bool isStencilWriteEnabled = (pDsView->ReadOnlyStencil() == false) &&
                                                   (pDepthStencilState->IsStencilWriteEnabled());

                isDepthStencilWriteEnabled = (isDepthWriteEnabled || isStencilWriteEnabled);
            }

            bool canDepthStencilRunOutOfOrder = false;

            if ((gfx9EnableOutOfOrderPrimitives == OutOfOrderPrimSafe) && (hasActiveQueries != 0))
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
                    (gfx9EnableOutOfOrderPrimitives == OutOfOrderPrimAggressive) &&
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
                                (gfx9EnableOutOfOrderPrimitives == OutOfOrderPrimAggressive));

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
    else if (gfx9EnableOutOfOrderPrimitives != OutOfOrderPrimAlways)
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
    m_contextRegHash(0),
    m_chunkHs(*pDevice,
              &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Hs)]),
    m_chunkGs(*pDevice,
              &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Gs)]),
    m_chunkVsPs(*pDevice,
                &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Vs)],
                &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Ps)])
{
    memset(&m_iaMultiVgtParam[0], 0, sizeof(m_iaMultiVgtParam));

    memset(&m_commands, 0, sizeof(m_commands));
    memcpy(&m_signature, &NullGfxSignature, sizeof(m_signature));

    m_sxPsDownconvert.u32All   = 0;
    m_sxBlendOptEpsilon.u32All = 0;
    m_sxBlendOptControl.u32All = 0;
    m_vgtLsHsConfig.u32All     = 0;
    m_spiVsOutConfig.u32All    = 0;
    m_spiPsInControl.u32All    = 0;
    m_paScModeCntl1.u32All     = 0;
}

// =====================================================================================================================
// Early HWL initialization for the pipeline.  Responsible for determining the number of SH and context registers to be
// loaded using LOAD_SH_REG_INDEX and LOAD_CONTEXT_REG_INDEX, as well as determining things like which shader stages are
// active.
void GraphicsPipeline::EarlyInit(
    const CodeObjectMetadata& metadata,
    const RegisterVector&     registers,
    GraphicsPipelineLoadInfo* pInfo)
{
    const RegisterInfo& regInfo = m_pDevice->CmdUtil().GetRegInfo();

    // VGT_SHADER_STAGES_EN and must be read first, since it determines which HW stages are active!
    m_commands.set.context.vgtShaderStagesEn.u32All = registers.At(mmVGT_SHADER_STAGES_EN);

    // Similarly, VGT_GS_MODE should also be read early, since it determines if on-chip GS is enabled.
    registers.HasEntry(mmVGT_GS_MODE, &m_commands.set.context.vgtGsMode.u32All);
    if (IsGsEnabled() && (m_commands.set.context.vgtGsMode.bits.ONCHIP == VgtGsModeOnchip))
    {
        SetIsGsOnChip(true);
    }

    // Must be called *after* determining active HW stages!
    SetupSignatureFromElf(metadata, registers);

    const Gfx9PalSettings& settings = m_pDevice->Settings();
    if (settings.enableLoadIndexForObjectBinds != false)
    {
        pInfo->loadedShRegCount = BaseLoadedShRegCount;

        pInfo->loadedCtxRegCount =
            (regInfo.mmPaStereoCntl != 0)                          + // mmPA_STEREO_CNTL
            (IsGsEnabled() || IsNgg() || IsTessEnabled())          + // mmVGT_GS_ONCHIP_CNTL
            BaseLoadedCntxRegCount;
    }

    pInfo->enableNgg        = IsNgg();
    pInfo->usesOnChipGs     = IsGsOnChip();
    pInfo->esGsLdsSizeRegGs = m_signature.esGsLdsSizeRegAddrGs;
    pInfo->esGsLdsSizeRegVs = m_signature.esGsLdsSizeRegAddrVs;

    if (IsTessEnabled())
    {
        m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Hs)].regOffset =
            m_signature.perfDataAddr[HwShaderStage::Hs];

        m_chunkHs.EarlyInit(pInfo);
    }

    if (IsGsEnabled() || pInfo->enableNgg)
    {
        m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Gs)].regOffset =
            m_signature.perfDataAddr[HwShaderStage::Gs];

        m_chunkGs.EarlyInit(pInfo);
    }

    m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Vs)].regOffset =
            m_signature.perfDataAddr[HwShaderStage::Vs];
    m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Ps)].regOffset =
            m_signature.perfDataAddr[HwShaderStage::Ps];

    m_chunkVsPs.EarlyInit(registers, pInfo);

#if PAL_ENABLE_PRINTS_ASSERTS
    if (settings.enableLoadIndexForObjectBinds != false)
    {
        PAL_ASSERT((pInfo->loadedShRegCount != 0) && (pInfo->loadedCtxRegCount != 0));
    }
    else
    {
        PAL_ASSERT((pInfo->loadedShRegCount == 0) && (pInfo->loadedCtxRegCount == 0));
    }
#endif
}

// =====================================================================================================================
// Initializes HW-specific state related to this graphics pipeline (register values, user-data mapping, etc.) using the
// specified Pipeline ABI processor and create info.
Result GraphicsPipeline::HwlInit(
    const GraphicsPipelineCreateInfo& createInfo,
    const AbiProcessor&               abiProcessor,
    const CodeObjectMetadata&         metadata,
    MsgPackReader*                    pMetadataReader)
{
    RegisterVector registers(m_pDevice->GetPlatform());
    Result result = pMetadataReader->Unpack(&registers);

    if (result == Result::Success)
    {
        GraphicsPipelineLoadInfo loadInfo = { };
        EarlyInit(metadata, registers, &loadInfo);

        // Next, handle relocations and upload the pipeline code & data to GPU memory.
        GraphicsPipelineUploader uploader(loadInfo.loadedCtxRegCount, loadInfo.loadedShRegCount);
        result = PerformRelocationsAndUploadToGpuMemory(abiProcessor, metadata, &uploader);

        if (result == Result::Success)
        {
            MetroHash64 hasher;
            BuildPm4Headers(uploader);

            if (IsTessEnabled())
            {
                m_chunkHs.LateInit(abiProcessor, registers, &uploader, &hasher);
            }
            if (IsGsEnabled() || IsNgg())
            {
                m_chunkGs.LateInit(abiProcessor, metadata, registers, loadInfo, &uploader, &hasher);
            }
            m_chunkVsPs.LateInit(abiProcessor, metadata, registers, loadInfo, &uploader, &hasher);

            SetupCommonRegisters(createInfo, registers, &uploader);
            SetupNonShaderRegisters(createInfo, registers, &uploader);
            SetupStereoRegisters();

            uploader.End();

            hasher.Update(m_commands.set.context);
            hasher.Update(m_commands.common);
            hasher.Finalize(reinterpret_cast<uint8* const>(&m_contextRegHash));

            UpdateRingSizes(metadata);
        }
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
                                                               : &m_chunkVsPs.StageInfoVs()));
        break;
    case ShaderType::Hull:
        pInfo = (IsTessEnabled() ? &m_chunkHs.StageInfo() : nullptr);
        break;
    case ShaderType::Domain:
        pInfo = (IsTessEnabled() ? ((IsGsEnabled() || IsNgg()) ? &m_chunkGs.StageInfo()
                                                               : &m_chunkVsPs.StageInfoVs())
                                 : nullptr);
        break;
    case ShaderType::Geometry:
        pInfo = (IsGsEnabled() ? &m_chunkGs.StageInfo() : nullptr);
        break;
    case ShaderType::Pixel:
        pInfo = &m_chunkVsPs.StageInfoPs();
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
    // The HW shader stage might a combination of two API shader stages (e.g., for GS copy shaders), so we must apply
    // the minimum wave limit of both API shader stages.  Note that zero is the largest value because it means
    // unlimited.
    const uint32 maxWavesPerCu =
        ((maxWavesPerCu2 == 0) ? maxWavesPerCu1
                               : ((maxWavesPerCu1 == 0) ? maxWavesPerCu2
                                                        : Min(maxWavesPerCu1, maxWavesPerCu2)));

    // The maximum number of waves per SH in "register units".
    // By default set the WAVE_LIMIT field to be unlimited.
    // Limits given by the ELF will only apply if the caller doesn't set their own limit.
    uint32 wavesPerSh = 0;

    // If the caller would like to override the default maxWavesPerCu
    if (maxWavesPerCu > 0)
    {
        const auto& gfx9ChipProps = m_pDevice->Parent()->ChipProperties().gfx9;

        const     uint32 numWavefrontsPerCu            = gfx9ChipProps.numSimdPerCu * gfx9ChipProps.numWavesPerSimd;
        constexpr uint32 MaxWavesPerShGraphicsUnitSize = 16u;
        const     uint32 maxWavesPerShGraphics         = (numWavefrontsPerCu * gfx9ChipProps.maxNumCuPerSh) /
                                                         MaxWavesPerShGraphicsUnitSize;

        // We assume no one is trying to use more than 100% of all waves.
        PAL_ASSERT(maxWavesPerCu <= numWavefrontsPerCu);
        const uint32 maxWavesPerSh = (maxWavesPerCu * gfx9ChipProps.numCuPerSh);

        // For graphics shaders, the WAVES_PER_SH field is in units of 16 waves and must not exceed 63. We must
        // also clamp to one if maxWavesPerSh rounded down to zero to prevent the limit from being removed.
        wavesPerSh = Min(maxWavesPerShGraphics, Max(1u, maxWavesPerSh / MaxWavesPerShGraphicsUnitSize));
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
        pCmdSpace = m_chunkGs.WriteShCommands(pCmdStream, pCmdSpace, stageInfos.gs);
    }
    pCmdSpace = m_chunkVsPs.WriteShCommands(pCmdStream, pCmdSpace, IsNgg(), stageInfos.vs, stageInfos.ps);

    // NOTE: It is possible for neither of the below branches to be taken.
    if (m_commands.set.sh.hdrSpiShaderLateAllocVs.header.u32All != 0)
    {
        constexpr uint32 SpaceNeededSet = sizeof(m_commands.set.sh) / sizeof(uint32);
        pCmdSpace = pCmdStream->WritePm4Image(SpaceNeededSet, &m_commands.set.sh, pCmdSpace);
    }
    else if (m_commands.loadIndex.sh.loadShRegIndex.header.u32All != 0)
    {
        constexpr uint32 SpaceNeededLoad = sizeof(m_commands.loadIndex.sh) / sizeof(uint32);
        pCmdSpace = pCmdStream->WritePm4Image(SpaceNeededLoad, &m_commands.loadIndex.sh, pCmdSpace);
    }

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

    if (m_commands.set.context.spaceNeeded != 0)
    {
        // The SET path's PM4 size will only be nonzero if the pipeline is using the SET path.
        pCmdSpace = pCmdStream->WritePm4Image(m_commands.set.context.spaceNeeded, &m_commands.set.context, pCmdSpace);

        if (IsTessEnabled())
        {
            pCmdSpace = m_chunkHs.WriteContextCommands(pCmdStream, pCmdSpace);
        }
        if (IsGsEnabled() || IsNgg())
        {
            pCmdSpace = m_chunkGs.WriteContextCommands(pCmdStream, pCmdSpace);
        }
    }
    else
    {
        PAL_ASSERT(m_commands.loadIndex.context.loadCtxRegIndex.header.u32All != 0);

        constexpr uint32 SpaceNeededLoad = sizeof(m_commands.loadIndex.context) / sizeof(uint32);
        pCmdSpace = pCmdStream->WritePm4Image(SpaceNeededLoad, &m_commands.loadIndex.context, pCmdSpace);
    }

    // NOTE: The VsPs chunk gets called for both the LOAD_INDEX and SET paths because it has some common stuff which
    // is written for both paths.
    pCmdSpace = m_chunkVsPs.WriteContextCommands(pCmdStream, pCmdSpace);

    return pCmdStream->WritePm4Image(m_commands.common.spaceNeeded, &m_commands.common, pCmdSpace);
}

// =====================================================================================================================
// Requests that this pipeline indicates what it would like to prefetch.
uint32* GraphicsPipeline::RequestPrefetch(
    const Pal::PrefetchMgr& prefetchMgr,
    uint32*                 pCmdSpace
    ) const
{
    const auto& gfx6PrefetchMgr = static_cast<const PrefetchMgr&>(prefetchMgr);

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
                                                        m_chunkVsPs.VsProgramGpuVa(),
                                                        m_chunkVsPs.StageInfoVs().codeLength,
                                                        pCmdSpace);
        }
    }
    else
    {
        pCmdSpace = gfx6PrefetchMgr.RequestPrefetch(hwVsPrefetch,
                                                    m_chunkVsPs.VsProgramGpuVa(),
                                                    m_chunkVsPs.StageInfoVs().codeLength,
                                                    pCmdSpace);
    }

    pCmdSpace = gfx6PrefetchMgr.RequestPrefetch(PrefetchPs,
                                                m_chunkVsPs.PsProgramGpuVa(),
                                                m_chunkVsPs.StageInfoPs().codeLength,
                                                pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Builds the packet headers for the various PM4 images associated with this pipeline.  Register values and packet
// payloads are computed elsewhere.
void GraphicsPipeline::BuildPm4Headers(
    const GraphicsPipelineUploader& uploader)
{
    const CmdUtil&      cmdUtil = m_pDevice->CmdUtil();
    const RegisterInfo& regInfo = cmdUtil.GetRegInfo();

    m_commands.common.spaceNeeded = cmdUtil.BuildContextRegRmw(mmDB_RENDER_OVERRIDE,
                                                               (DB_RENDER_OVERRIDE__FORCE_SHADER_Z_ORDER_MASK |
                                                                DB_RENDER_OVERRIDE__FORCE_STENCIL_READ_MASK |
                                                                DB_RENDER_OVERRIDE__DISABLE_VIEWPORT_CLAMP_MASK),
                                                               0,
                                                               &m_commands.common.dbRenderOverride);

    // - Driver must insert FLUSH_DFSM event whenever the ... channel mask changes (ARGB to RGB)
    //
    // Channel-mask changes refer to the CB_TARGET_MASK register
    m_commands.common.spaceNeeded +=
        cmdUtil.BuildNonSampleEventWrite(FLUSH_DFSM, EngineTypeUniversal, &m_commands.common.flushDfsm);

    if (uploader.EnableLoadIndexPath())
    {
        PAL_ASSERT((uploader.CtxRegGpuVirtAddr() != 0) && (uploader.ShRegGpuVirtAddr() != 0));

        cmdUtil.BuildLoadShRegsIndex(uploader.ShRegGpuVirtAddr(),
                                     uploader.ShRegisterCount(),
                                     ShaderGraphics,
                                     &m_commands.loadIndex.sh.loadShRegIndex);
        cmdUtil.BuildLoadContextRegsIndex(uploader.CtxRegGpuVirtAddr(),
                                          uploader.CtxRegisterCount(),
                                          &m_commands.loadIndex.context.loadCtxRegIndex);
    }
    else
    {
        if (m_gfxLevel == GfxIpLevel::GfxIp9)
        {
            cmdUtil.BuildSetOneShReg(mmSPI_SHADER_LATE_ALLOC_VS,
                                     ShaderGraphics,
                                     &m_commands.set.sh.hdrSpiShaderLateAllocVs);
        }

        m_commands.set.context.spaceNeeded =
            cmdUtil.BuildSetOneContextReg(mmVGT_SHADER_STAGES_EN, &m_commands.set.context.hdrVgtShaderStagesEn);

        m_commands.set.context.spaceNeeded +=
            cmdUtil.BuildSetOneContextReg(mmVGT_GS_MODE, &m_commands.set.context.hdrVgtGsMode);

        m_commands.set.context.spaceNeeded +=
            cmdUtil.BuildSetOneContextReg(mmVGT_REUSE_OFF, &m_commands.set.context.hdrVgtReuseOff);

        m_commands.set.context.spaceNeeded +=
            cmdUtil.BuildSetOneContextReg(mmVGT_TF_PARAM, &m_commands.set.context.hdrVgtTfParam);

        m_commands.set.context.spaceNeeded +=
            cmdUtil.BuildSetOneContextReg(mmCB_COLOR_CONTROL, &m_commands.set.context.hdrCbColorControl);

        m_commands.set.context.spaceNeeded +=
            cmdUtil.BuildSetSeqContextRegs(mmCB_TARGET_MASK, mmCB_SHADER_MASK,
                                           &m_commands.set.context.hdrCbShaderTargetMask);

        m_commands.set.context.spaceNeeded +=
            cmdUtil.BuildSetOneContextReg(mmPA_CL_CLIP_CNTL, &m_commands.set.context.hdrPaClClipCntl);

        m_commands.set.context.spaceNeeded +=
            cmdUtil.BuildSetOneContextReg(mmPA_SU_VTX_CNTL, &m_commands.set.context.hdrPaSuVtxCntl);

        m_commands.set.context.spaceNeeded +=
            cmdUtil.BuildSetOneContextReg(mmPA_CL_VTE_CNTL, &m_commands.set.context.hdrPaClVteCntl);

        m_commands.set.context.spaceNeeded +=
            cmdUtil.BuildSetOneContextReg(mmPA_SC_LINE_CNTL, &m_commands.set.context.hdrPaScLineCntl);

        if (regInfo.mmPaStereoCntl != 0)
        {
            m_commands.set.context.spaceNeeded +=
                cmdUtil.BuildSetOneContextReg(regInfo.mmPaStereoCntl, &m_commands.set.context.hdrPaStereoCntl);
        }
        else
        {
            // Use a NOP to fill the gap for hardware which doesn't have mmPA_STEREO_CNTL.
            m_commands.set.context.spaceNeeded +=
                cmdUtil.BuildNop(CmdUtil::ContextRegSizeDwords + 1, &m_commands.set.context.hdrPaStereoCntl);
        }

        m_commands.set.context.spaceNeeded +=
            cmdUtil.BuildSetOneContextReg(mmSPI_INTERP_CONTROL_0, &m_commands.set.context.hdrSpiInterpControl0);

        m_commands.set.context.spaceNeeded +=
            cmdUtil.BuildSetOneContextReg(mmVGT_VERTEX_REUSE_BLOCK_CNTL,
                                          &m_commands.set.context.hdrVgtVertexReuseBlockCntl);

        if (IsGsEnabled() || IsNgg() || IsTessEnabled())
        {
            m_commands.set.context.spaceNeeded +=
                cmdUtil.BuildSetOneContextReg(mmVGT_GS_ONCHIP_CNTL,
                                              &m_commands.set.context.hdrVgtGsOnchipCntl);
        }
        else
        {
            m_commands.set.context.spaceNeeded +=
                cmdUtil.BuildNop(CmdUtil::ContextRegSizeDwords + 1, &m_commands.set.context.hdrVgtGsOnchipCntl);
        }
    } // if EnableLoadIndexPath == false
}

// =====================================================================================================================
// Updates the RB+ register values for a single render target slot.  It is only expected that this will be called for
// pipelines with RB+ enabled.
void GraphicsPipeline::SetupRbPlusRegistersForSlot(
    uint32                   slot,
    uint8                    writeMask,
    SwizzledFormat           swizzledFormat,
    regSX_PS_DOWNCONVERT*    pSxPsDownconvert,
    regSX_BLEND_OPT_EPSILON* pSxBlendOptEpsilon,
    regSX_BLEND_OPT_CONTROL* pSxBlendOptControl
    ) const
{
    const uint32 bitShift = (4 * slot);

    const SX_DOWNCONVERT_FORMAT downConvertFormat = SxDownConvertFormat(swizzledFormat.format);
    const uint32                blendOptControl   = Gfx9::SxBlendOptControl(writeMask);
    const uint32                blendOptEpsilon   =
        (downConvertFormat == SX_RT_EXPORT_NO_CONVERSION) ? 0 : Gfx9::SxBlendOptEpsilon(downConvertFormat);

    pSxPsDownconvert->u32All &= ~(SX_PS_DOWNCONVERT__MRT0_MASK << bitShift);
    pSxPsDownconvert->u32All |= (downConvertFormat << bitShift);

    pSxBlendOptEpsilon->u32All &= ~(SX_BLEND_OPT_EPSILON__MRT0_EPSILON_MASK << bitShift);
    pSxBlendOptEpsilon->u32All |= (blendOptEpsilon << bitShift);

    pSxBlendOptControl->u32All &= ~((SX_BLEND_OPT_CONTROL__MRT0_COLOR_OPT_DISABLE_MASK |
                                     SX_BLEND_OPT_CONTROL__MRT0_ALPHA_OPT_DISABLE_MASK) << bitShift);
    pSxBlendOptControl->u32All |= (blendOptControl << bitShift);
}

// =====================================================================================================================
// Initializes render-state registers which are associated with multiple hardware shader stages.
void GraphicsPipeline::SetupCommonRegisters(
    const GraphicsPipelineCreateInfo& createInfo,
    const RegisterVector&             registers,
    GraphicsPipelineUploader*         pUploader)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    const RegisterInfo&      regInfo   = m_pDevice->CmdUtil().GetRegInfo();
    const Gfx9PalSettings&   settings  = m_pDevice->Settings();

    m_commands.set.context.paClClipCntl.u32All    = registers.At(mmPA_CL_CLIP_CNTL);
    m_commands.set.context.paClVteCntl.u32All     = registers.At(mmPA_CL_VTE_CNTL);
    m_commands.set.context.paSuVtxCntl.u32All     = registers.At(mmPA_SU_VTX_CNTL);
    m_paScModeCntl1.u32All                        = registers.At(mmPA_SC_MODE_CNTL_1);
    m_commands.set.context.vgtGsOnchipCntl.u32All = registers.At(mmVGT_GS_ONCHIP_CNTL);

    // Overrides some of the fields in PA_SC_MODE_CNTL1 to account for GPU pipe config and features like out-of-order
    // rasterization.

    // The maximum value for OUT_OF_ORDER_WATER_MARK is 7
    constexpr uint32 MaxOutOfOrderWatermark = 7;
    m_paScModeCntl1.bits.OUT_OF_ORDER_WATER_MARK = Min(MaxOutOfOrderWatermark, settings.outOfOrderWatermark);

    if (createInfo.rsState.outOfOrderPrimsEnable &&
        (settings.enableOutOfOrderPrimitives != OutOfOrderPrimDisable))
    {
        m_paScModeCntl1.bits.OUT_OF_ORDER_PRIMITIVE_ENABLE = 1;
    }

    // Hardware team recommendation is to set WALK_FENCE_SIZE to 512 pixels for 4/8/16 pipes and 256 pixels
    // for 2 pipes.
    m_paScModeCntl1.bits.WALK_FENCE_SIZE = ((m_pDevice->GetNumPipesLog2() <= 1) ? 2 : 3);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 387
    m_info.ps.flags.perSampleShading = m_paScModeCntl1.bits.PS_ITER_SAMPLE;
#endif

    // NOTE: On recommendation from h/ware team FORCE_SHADER_Z_ORDER will be set whenever Re-Z is being used.
    regDB_RENDER_OVERRIDE dbRenderOverride = { };
    dbRenderOverride.bits.FORCE_SHADER_Z_ORDER = (m_chunkVsPs.DbShaderControl().bits.Z_ORDER == RE_Z);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 381
    // Configure depth clamping
    dbRenderOverride.bits.DISABLE_VIEWPORT_CLAMP = ((createInfo.rsState.depthClampDisable != false) &&
                                                    (m_chunkVsPs.DbShaderControl().bits.Z_EXPORT_ENABLE != 0));
#else
    // Configure depth clamping
    dbRenderOverride.bits.DISABLE_VIEWPORT_CLAMP = ((createInfo.rsState.depthClampEnable == false) &&
                                                    (m_chunkVsPs.DbShaderControl().bits.Z_EXPORT_ENABLE != 0));
#endif
    m_commands.common.dbRenderOverride.reg_data = dbRenderOverride.u32All;

    if (regInfo.mmPaStereoCntl != 0)
    {
        registers.HasEntry(regInfo.mmPaStereoCntl, &m_commands.set.context.paStereoCntl.u32All);
    }

    m_commands.set.context.vgtReuseOff.u32All = registers.At(mmVGT_REUSE_OFF);
    m_spiPsInControl.u32All                   = registers.At(mmSPI_PS_IN_CONTROL);
    m_spiVsOutConfig.u32All                   = registers.At(mmSPI_VS_OUT_CONFIG);

    // NOTE: The following registers are assumed to have the value zero if the pipeline ELF does not specify values.
    registers.HasEntry(mmVGT_TF_PARAM,     &m_commands.set.context.vgtTfParam.u32All);
    registers.HasEntry(mmVGT_LS_HS_CONFIG, &m_vgtLsHsConfig.u32All);

    // If the number of VS output semantics exceeds the half-pack threshold, then enable VS half-pack mode.  Keep in
    // mind that the number of VS exports are represented by a -1 field in the HW register!
    if ((m_spiVsOutConfig.bits.VS_EXPORT_COUNT + 1u) > settings.vsHalfPackThreshold)
    {
        m_spiVsOutConfig.bits.VS_HALF_PACK = 1;
    }

    // For Gfx9+, default VTX_REUSE_DEPTH to 14
    m_commands.set.context.vgtVertexReuseBlockCntl.bits.VTX_REUSE_DEPTH = 14;

    if ((settings.vsHalfPackThreshold >= MaxVsExportSemantics) &&
        (m_gfxLevel == GfxIpLevel::GfxIp9))
    {
        // Degenerate primitive filtering with fractional odd tessellation requires a VTX_REUSE_DEPTH of 14. Only
        // override to 30 if we aren't using that feature.
        //
        // VGT_TF_PARAM depends solely on the compiled HS when on-chip GS is disabled, in the future when Tess with
        // on-chip GS is supported, the 2nd condition may need to be revisited.
        if ((m_pDevice->DegeneratePrimFilter() == false) ||
            (IsTessEnabled() && (m_commands.set.context.vgtTfParam.bits.PARTITIONING != PART_FRAC_ODD)))
        {
            m_commands.set.context.vgtVertexReuseBlockCntl.bits.VTX_REUSE_DEPTH = 30;
        }
    }

    m_commands.set.context.spiInterpControl0.u32All = 0;
    registers.HasEntry(mmSPI_INTERP_CONTROL_0, &m_commands.set.context.spiInterpControl0.u32All);

    m_commands.set.context.spiInterpControl0.bits.FLAT_SHADE_ENA = (createInfo.rsState.shadeMode == ShadeMode::Flat);
    if (m_commands.set.context.spiInterpControl0.bits.PNT_SPRITE_ENA != 0) // Point sprite mode is enabled.
    {
        m_commands.set.context.spiInterpControl0.bits.PNT_SPRITE_TOP_1  =
            (createInfo.rsState.pointCoordOrigin != PointOrigin::UpperLeft);
    }

    if (pUploader->EnableLoadIndexPath())
    {
        pUploader->AddCtxReg(mmVGT_SHADER_STAGES_EN,        m_commands.set.context.vgtShaderStagesEn);
        pUploader->AddCtxReg(mmVGT_GS_MODE,                 m_commands.set.context.vgtGsMode);
        pUploader->AddCtxReg(mmVGT_REUSE_OFF,               m_commands.set.context.vgtReuseOff);
        pUploader->AddCtxReg(mmVGT_TF_PARAM,                m_commands.set.context.vgtTfParam);
        pUploader->AddCtxReg(mmPA_CL_CLIP_CNTL,             m_commands.set.context.paClClipCntl);
        pUploader->AddCtxReg(mmPA_SU_VTX_CNTL,              m_commands.set.context.paSuVtxCntl);
        pUploader->AddCtxReg(mmPA_CL_VTE_CNTL,              m_commands.set.context.paClVteCntl);
        pUploader->AddCtxReg(mmSPI_INTERP_CONTROL_0,        m_commands.set.context.spiInterpControl0);
        pUploader->AddCtxReg(mmVGT_VERTEX_REUSE_BLOCK_CNTL, m_commands.set.context.vgtVertexReuseBlockCntl);

        if (regInfo.mmPaStereoCntl != 0)
        {
            pUploader->AddCtxReg(regInfo.mmPaStereoCntl, m_commands.set.context.paStereoCntl);
        }

    }

    // If NGG is enabled, there is no hardware-VS, so there is no need to compute the late-alloc VS limit.
    if (IsNgg() == false)
    {
        SetupLateAllocVs(registers, pUploader);
    }
    SetupIaMultiVgtParam(registers);
}

// =====================================================================================================================
// The pipeline binary is allowed to partially specify the value for IA_MULTI_VGT_PARAM.  PAL will finish initializing
// this register based on GPU properties, pipeline create info, and the values of other registers.
void GraphicsPipeline::SetupIaMultiVgtParam(
    const RegisterVector& registers)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    regIA_MULTI_VGT_PARAM iaMultiVgtParam = { };
    registers.HasEntry(Gfx09::mmIA_MULTI_VGT_PARAM, &iaMultiVgtParam.u32All);

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

            if ((m_iaMultiVgtParam[idx].gfx09.EN_INST_OPT_BASIC == 1) ||
                (m_iaMultiVgtParam[idx].gfx09.EN_INST_OPT_ADV   == 1))
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

    if (m_commands.set.context.vgtTfParam.bits.DISTRIBUTION_MODE != NO_DIST)
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
            pIaMultiVgtParam->gfx09.EN_INST_OPT_BASIC = 1;
        }
        else if (settings.wdLoadBalancingMode == Gfx9WdLoadBalancingAdvanced)
        {
            // Advanced optimization enables basic optimization and additional sub-draw call distribution algorithm
            // which splits batch into smaller instanced draws.
            pIaMultiVgtParam->gfx09.EN_INST_OPT_ADV = 1;
        }
    }

}

// =====================================================================================================================
// Initializes render-state registers which aren't part of any hardware shader stage.
void GraphicsPipeline::SetupNonShaderRegisters(
    const GraphicsPipelineCreateInfo& createInfo,
    const RegisterVector&             registers,
    GraphicsPipelineUploader*         pUploader)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    const Gfx9PalSettings&   settings  = m_pDevice->Settings();

    m_commands.set.context.paScLineCntl.bits.EXPAND_LINE_WIDTH        = createInfo.rsState.expandLineWidth;
    m_commands.set.context.paScLineCntl.bits.DX10_DIAMOND_TEST_ENA    = 1;
    m_commands.set.context.paScLineCntl.bits.LAST_PIXEL               = createInfo.rsState.rasterizeLastLinePixel;
    m_commands.set.context.paScLineCntl.bits.PERPENDICULAR_ENDCAP_ENA = createInfo.rsState.perpLineEndCapsEnable;

    m_commands.set.context.cbShaderMask.u32All = registers.At(mmCB_SHADER_MASK);

    // CB_TARGET_MASK comes from the RT write masks in the pipeline CB state structure.
    for (uint32 rt = 0; rt < MaxColorTargets; ++rt)
    {
        const uint32 rtShift = (rt * 4); // Each RT uses four bits of CB_TARGET_MASK.
        m_commands.set.context.cbTargetMask.u32All |=
                ((createInfo.cbState.target[rt].channelWriteMask & 0xF) << rtShift);
    }

    //      The bug manifests itself when an MRT is not enabled in the shader mask but is enabled in the target
    //      mask. It will work fine if the target mask is always a subset of the shader mask
    if (settings.waOverwriteCombinerTargetMaskOnly &&
        (TestAllFlagsSet(m_commands.set.context.cbShaderMask.u32All,
                         m_commands.set.context.cbTargetMask.u32All) == false))
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

    if (IsFastClearEliminate())
    {
        m_commands.set.context.cbColorControl.bits.MODE = CB_ELIMINATE_FAST_CLEAR;
        m_commands.set.context.cbColorControl.bits.ROP3 = Rop3(LogicOp::Copy);

        // NOTE: the CB spec states that for fast-clear eliminate, these registers should be set to enable writes to all
        // four channels of RT #0.
        m_commands.set.context.cbShaderMask.u32All = 0xF;
        m_commands.set.context.cbTargetMask.u32All = 0xF;
    }
    else if (IsFmaskDecompress())
    {
        m_commands.set.context.cbColorControl.bits.MODE = CB_FMASK_DECOMPRESS;
        m_commands.set.context.cbColorControl.bits.ROP3 = Rop3(LogicOp::Copy);

        // NOTE: the CB spec states that for fmask-decompress, these registers should be set to enable writes to all
        // four channels of RT #0.
        m_commands.set.context.cbShaderMask.u32All = 0xF;
        m_commands.set.context.cbTargetMask.u32All = 0xF;
    }
    else if (IsDccDecompress())
    {
        m_commands.set.context.cbColorControl.bits.MODE = CB_DCC_DECOMPRESS;
        m_commands.set.context.cbColorControl.bits.ROP3 = Rop3(LogicOp::Copy);

        // According to the reg-spec, DCC decompress ops imply fmask decompress and fast-clear eliminate operations as
        // well, so set these registers as they would be set above.
        m_commands.set.context.cbShaderMask.u32All = 0xF;
        m_commands.set.context.cbTargetMask.u32All = 0xF;
    }
    else if (IsResolveFixedFunc())
    {
        m_commands.set.context.cbColorControl.bits.MODE = CB_RESOLVE;
        m_commands.set.context.cbColorControl.bits.ROP3 = Rop3(LogicOp::Copy);

        m_commands.set.context.cbShaderMask.u32All = 0xF;
        m_commands.set.context.cbTargetMask.u32All = 0xF;
    }
    else if ((m_commands.set.context.cbShaderMask.u32All == 0) || (m_commands.set.context.cbTargetMask.u32All == 0))
    {
        m_commands.set.context.cbColorControl.bits.MODE = CB_DISABLE;
    }
    else
    {
        m_commands.set.context.cbColorControl.bits.MODE = CB_NORMAL;
        m_commands.set.context.cbColorControl.bits.ROP3 = Rop3(createInfo.cbState.logicOp);
    }

    if (createInfo.cbState.dualSourceBlendEnable)
    {
        // If dual-source blending is enabled and the PS doesn't export to both RT0 and RT1, the hardware might hang.
        // To avoid the hang, just disable CB writes.
        if (((m_commands.set.context.cbShaderMask.u32All & 0x0F) == 0) ||
            ((m_commands.set.context.cbShaderMask.u32All & 0xF0) == 0))
        {
            PAL_ALERT_ALWAYS();
            m_commands.set.context.cbColorControl.bits.MODE = CB_DISABLE;
        }
    }

    // Initialize RB+ registers for pipelines which are able to use the feature.
    if (settings.gfx9RbPlusEnable &&
        (createInfo.cbState.dualSourceBlendEnable == false) &&
        (m_commands.set.context.cbColorControl.bits.MODE != CB_RESOLVE))
    {
        PAL_ASSERT(chipProps.gfx9.rbPlus);

        m_commands.set.context.cbColorControl.bits.DISABLE_DUAL_QUAD = 0;

        for (uint32 slot = 0; slot < MaxColorTargets; ++slot)
        {
            SetupRbPlusRegistersForSlot(slot,
                                        createInfo.cbState.target[slot].channelWriteMask,
                                        createInfo.cbState.target[slot].swizzledFormat,
                                        &m_sxPsDownconvert,
                                        &m_sxBlendOptEpsilon,
                                        &m_sxBlendOptControl);
        }
    }
    else if (chipProps.gfx9.rbPlus != 0)
    {
        // If RB+ is supported but not enabled, we need to set DISABLE_DUAL_QUAD.
        m_commands.set.context.cbColorControl.bits.DISABLE_DUAL_QUAD = 1;
    }

    // Override some register settings based on toss points.  These toss points cannot be processed in the hardware
    // independent class because they cannot be overridden by altering the pipeline creation info.
    if ((IsInternal() == false) && (m_pDevice->Parent()->Settings().tossPointMode == TossPointAfterPs))
    {
        // This toss point is used to disable all color buffer writes.
        m_commands.set.context.cbTargetMask.u32All = 0;
    }

    if (pUploader->EnableLoadIndexPath())
    {
        pUploader->AddCtxReg(mmPA_SC_LINE_CNTL,  m_commands.set.context.paScLineCntl);
        pUploader->AddCtxReg(mmCB_COLOR_CONTROL, m_commands.set.context.cbColorControl);
        pUploader->AddCtxReg(mmCB_SHADER_MASK,   m_commands.set.context.cbShaderMask);
        pUploader->AddCtxReg(mmCB_TARGET_MASK,   m_commands.set.context.cbTargetMask);
        if (IsGsEnabled() || IsNgg() || IsTessEnabled())
        {
            pUploader->AddCtxReg(mmVGT_GS_ONCHIP_CNTL, m_commands.set.context.vgtGsOnchipCntl);
        }

    }
}

// =====================================================================================================================
// Sets-up the SPI_SHADER_LATE_ALLOC_VS on Gfx9
void GraphicsPipeline::SetupLateAllocVs(
    const RegisterVector&     registers,
    GraphicsPipelineUploader* pUploader)
{
    const auto pPalSettings = m_pDevice->Parent()->GetPublicSettings();
    const auto gfx9Settings = m_pDevice->Settings();

    regSPI_SHADER_PGM_RSRC1_VS spiShaderPgmRsrc1Vs = { };
    spiShaderPgmRsrc1Vs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_VS);

    regSPI_SHADER_PGM_RSRC2_VS spiShaderPgmRsrc2Vs = { };
    spiShaderPgmRsrc2Vs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_VS);

    regSPI_SHADER_PGM_RSRC2_PS spiShaderPgmRsrc2Ps = { };
    spiShaderPgmRsrc2Ps.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_PS);

    // Default to a late-alloc limit of zero.  This will nearly mimic the GFX6 behavior where VS waves don't launch
    // without allocating export space.
    uint32 lateAllocLimit = 0;

    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    // Maximum value of the LIMIT field of the SPI_SHADER_LATE_ALLOC_VS register
    // It is the number of wavefronts minus one.
    const uint32 maxLateAllocLimit = chipProps.gfxip.maxLateAllocVsLimit - 1;

    // Target late-alloc limit uses PAL settings by default. The lateAllocVsLimit member from graphicsPipeline
    // can override this setting if corresponding flag is set.
    uint32 targetLateAllocLimit = IsLateAllocVsLimit() ? GetLateAllocVsLimit() : m_pDevice->LateAllocVsLimit();

    const uint32 vsNumSgpr = (spiShaderPgmRsrc1Vs.bits.SGPRS * 8);
    const uint32 vsNumVgpr = (spiShaderPgmRsrc1Vs.bits.VGPRS * 4);

    if (gfx9Settings.lateAllocVs == LateAllocVsBehaviorDisabled)
    {
        // Disable late alloc vs entirely
        lateAllocLimit = 0;
    }
    else if (m_pDevice->UseFixedLateAllocVsLimit())
    {
        // When using the fixed wave limit scheme, just accept the client or device specified target value.  The
        // fixed scheme mandates that we are disabling a CU from running VS work, so any limit the client may
        // have specified is safe.
        lateAllocLimit = targetLateAllocLimit;
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

        uint32 numCuForLateAllocVs = chipProps.gfx9.numCuPerSh;

        // Compute the maximum number of HW-VS wavefronts that can launch per SH, based on GPR usage.
        const uint32 simdPerSh = (numCuForLateAllocVs * chipProps.gfx9.numSimdPerCu);
        const uint32 maxSgprVsWaves = (chipProps.gfx9.numPhysicalSgprs / vsNumSgpr) * simdPerSh;
        const uint32 maxVgprVsWaves = (chipProps.gfx9.numPhysicalVgprs / vsNumVgpr) * simdPerSh;

        uint32 maxVsWaves = Min(maxSgprVsWaves, maxVgprVsWaves);

        // Find the maximum number of VS waves that can be launched based on scratch usage if both the PS and VS use
        // scratch.
        if ((spiShaderPgmRsrc2Vs.bits.SCRATCH_EN != 0) && (spiShaderPgmRsrc2Ps.bits.SCRATCH_EN != 0))
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
    }

    // The late alloc setting is the number of wavefronts minus one.  On GFX7+ at least one VS wave always can
    // launch with late alloc enabled.
    lateAllocLimit = (lateAllocLimit > 0) ? (lateAllocLimit - 1) : 0;

    const uint32 programmedLimit = Min(lateAllocLimit, maxLateAllocLimit);
    if (m_gfxLevel == GfxIpLevel::GfxIp9)
    {
        m_commands.set.sh.spiShaderLateAllocVs.bits.LIMIT = programmedLimit;

        if (pUploader->EnableLoadIndexPath())
        {
            pUploader->AddShReg(mmSPI_SHADER_LATE_ALLOC_VS, m_commands.set.sh.spiShaderLateAllocVs);
        }
    }
}

// =====================================================================================================================
// Updates the device that this pipeline has some new ring-size requirements.
void GraphicsPipeline::UpdateRingSizes(
    const CodeObjectMetadata& metadata)
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

    ringSizes.itemSize[static_cast<size_t>(ShaderRingType::GfxScratch)] = ComputeScratchMemorySize(metadata);

    // Inform the device that this pipeline has some new ring-size requirements.
    m_pDevice->UpdateLargestRingSizes(&ringSizes);
}

// =====================================================================================================================
// Calculates the maximum scratch memory in dwords necessary by checking the scratch memory needed for each shader.
uint32 GraphicsPipeline::ComputeScratchMemorySize(
    const CodeObjectMetadata& metadata
    ) const
{
    uint32 scratchMemorySizeBytes = 0;
    for (uint32 i = 0; i < static_cast<uint32>(Abi::HardwareStage::Count); ++i)
    {
        const auto& stageMetadata = metadata.pipeline.hardwareStage[i];
        if (stageMetadata.hasEntry.scratchMemorySize != 0)
        {
            scratchMemorySizeBytes = Max(scratchMemorySizeBytes, stageMetadata.scratchMemorySize);
        }
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
            ((shaderType == ShaderType::Geometry) && (IsNgg() == false)) ? &m_chunkVsPs.StageInfoVs() : nullptr;

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
                    pShaderStats->copyShader.gpuVirtAddress        = m_chunkVsPs.VsProgramGpuVa();
                    pShaderStats->copyShader.ldsSizePerThreadGroup = chipProps.gfxip.ldsSizePerThreadGroup;
                }
                break;
            case Abi::HardwareStage::Vs:
                pShaderStats->shaderStageMask       = (IsTessEnabled() ? ApiShaderStageDomain : ApiShaderStageVertex);
                pShaderStats->common.gpuVirtAddress = m_chunkVsPs.VsProgramGpuVa();
                break;
            case Abi::HardwareStage::Ps:
                pShaderStats->shaderStageMask       = ApiShaderStagePixel;
                pShaderStats->common.gpuVirtAddress = m_chunkVsPs.PsProgramGpuVa();
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
    const CodeObjectMetadata& metadata,
    const RegisterVector&     registers,
    HwShaderStage             stage)
{
    uint16  entryToRegAddr[MaxUserDataEntries] = { };

    const uint16 baseRegAddr = m_pDevice->GetBaseUserDataReg(stage);
    const uint16 lastRegAddr = (baseRegAddr + 31);

    const uint32 stageId = static_cast<uint32>(stage);
    auto*const   pStage  = &m_signature.stage[stageId];

    constexpr Abi::HardwareStage PalToAbiHwShaderStage[] =
    {
        Abi::HardwareStage::Hs,
        Abi::HardwareStage::Gs,
        Abi::HardwareStage::Vs,
        Abi::HardwareStage::Ps,
    };

    for (uint16 offset = baseRegAddr; offset <= lastRegAddr; ++offset)
    {
        uint32 value = 0;
        if (registers.HasEntry(offset, &value))
        {
            if (value < MaxUserDataEntries)
            {
                if (pStage->firstUserSgprRegAddr == UserDataNotMapped)
                {
                    pStage->firstUserSgprRegAddr = offset;
                }

                PAL_ASSERT(offset >= pStage->firstUserSgprRegAddr);
                const uint8 userSgprId = static_cast<uint8>(offset - pStage->firstUserSgprRegAddr);
                entryToRegAddr[value]  = offset;

                pStage->mappedEntry[userSgprId] = static_cast<uint8>(value);
                pStage->userSgprCount = Max<uint8>(userSgprId + 1, pStage->userSgprCount);
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
            else if (value == static_cast<uint32>(Abi::UserDataMapping::PerShaderPerfData))
            {
                const uint32 abiHwId = static_cast<uint32>(PalToAbiHwShaderStage[stage]);
                m_signature.perfDataAddr[abiHwId] = offset;
            }
            else
            {
                // This appears to be an illegally-specified user-data register!
                PAL_NEVER_CALLED();
            }
        } // If HasEntry()
    } // For each user-SGPR

    for (uint32 i = 0; i < MaxIndirectUserDataTables; ++i)
    {
        if (m_signature.indirectTableAddr[i] != UserDataNotMapped)
        {
            pStage->indirectTableRegAddr[i] = entryToRegAddr[m_signature.indirectTableAddr[i] - 1];
        }
    }

    if ((stage == HwShaderStage::Vs) && (m_signature.streamOutTableAddr != UserDataNotMapped))
    {
        m_signature.streamOutTableRegAddr = entryToRegAddr[m_signature.streamOutTableAddr - 1];
    }

    // Compute a hash of the regAddr array and spillTableRegAddr for the CS stage.
    MetroHash64::Hash(
        reinterpret_cast<const uint8*>(pStage),
        sizeof(UserDataEntryMap),
        reinterpret_cast<uint8* const>(&m_signature.userDataHash[stageId]));
}

// =====================================================================================================================
// Initializes the signature of a graphics pipeline using a pipeline ELF.
void GraphicsPipeline::SetupSignatureFromElf(
    const CodeObjectMetadata& metadata,
    const RegisterVector&     registers)
{
    if (metadata.pipeline.hasEntry.streamOutTableAddress != 0)
    {
        m_signature.streamOutTableAddr = static_cast<uint16>(metadata.pipeline.streamOutTableAddress);
    }

    if (metadata.pipeline.hasEntry.indirectUserDataTableAddresses != 0)
    {
        for (uint32 i = 0; i < MaxIndirectUserDataTables; ++i)
        {
            m_signature.indirectTableAddr[i] = static_cast<uint16>(metadata.pipeline.indirectUserDataTableAddresses[i]);
        }
    }

    if (metadata.pipeline.hasEntry.spillThreshold != 0)
    {
        m_signature.spillThreshold = static_cast<uint16>(metadata.pipeline.spillThreshold);
    }

    if (metadata.pipeline.hasEntry.userDataLimit != 0)
    {
        m_signature.userDataLimit = static_cast<uint16>(metadata.pipeline.userDataLimit);
    }

    if (IsTessEnabled())
    {
        SetupSignatureForStageFromElf(metadata, registers, HwShaderStage::Hs);
    }
    if (IsGsEnabled() || IsNgg())
    {
        SetupSignatureForStageFromElf(metadata, registers, HwShaderStage::Gs);
    }
    if (IsNgg() == false)
    {
        SetupSignatureForStageFromElf(metadata, registers, HwShaderStage::Vs);
    }
    SetupSignatureForStageFromElf(metadata, registers, HwShaderStage::Ps);

    // Finally, compact the array of view ID register addresses
    // so that all of the mapped ones are at the front of the array.
    PackArray(m_signature.viewIdRegAddr, UserDataNotMapped);
}

// =====================================================================================================================
// Converts the specified logic op enum into a ROP3 code (for programming CB_COLOR_CONTROL).
static uint8 Rop3(
    LogicOp logicOp)
{
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
SX_DOWNCONVERT_FORMAT GraphicsPipeline::SxDownConvertFormat(
    ChNumFormat format
    ) const
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
    const auto& dbShaderControl = m_chunkVsPs.DbShaderControl().bits;

    return dbShaderControl.KILL_ENABLE || dbShaderControl.MASK_EXPORT_ENABLE || dbShaderControl.COVERAGE_TO_MASK_ENABLE;
}

// =====================================================================================================================
// Returns true when the alpha to mask is enabled. The DB_SHADER_CONTROL::ALPHA_TO_MASK_DISABLE bit controls whether
// or not the MsaaState's DB_ALPHA_TO_MASK::ALPHA_TO_MASK_ENABLE bit works. When ALPHA_TO_MASK_DISABLE is true, the
// MsaaState's ALPHA_TO_MASK_ENABLE bit is disabled. We need to know this when considering PBB optimizations.
bool GraphicsPipeline::IsAlphaToMaskEnable() const
{
    const auto& dbShaderControl = m_chunkVsPs.DbShaderControl().bits;

    return (dbShaderControl.ALPHA_TO_MASK_DISABLE == 0);
}

// =====================================================================================================================
bool GraphicsPipeline::PsCanTriviallyReject() const
{
    const auto& dbShaderControl = m_chunkVsPs.DbShaderControl();

    return ((dbShaderControl.bits.Z_EXPORT_ENABLE == 0) || (dbShaderControl.bits.CONSERVATIVE_Z_EXPORT > 0));
}

// =====================================================================================================================
bool GraphicsPipeline::PsAllowsPunchout() const
{
    const auto& dbShaderControl = m_chunkVsPs.DbShaderControl();

    return (m_commands.set.context.cbShaderMask.u32All != 0) &&
           (dbShaderControl.bits.KILL_ENABLE == 0)           &&
           (dbShaderControl.bits.EXEC_ON_HIER_FAIL == 0)     &&
           (dbShaderControl.bits.EXEC_ON_NOOP == 0)          &&
           (dbShaderControl.bits.Z_ORDER == EARLY_Z_THEN_LATE_Z);
}

// =====================================================================================================================
// Updates the NGG Primitive Constant Buffer with the values from this pipeline.
void GraphicsPipeline::UpdateNggPrimCb(
    Abi::PrimShaderPsoCb* pPrimShaderCb
    ) const
{
    pPrimShaderCb->paClVteCntl  = m_commands.set.context.paClVteCntl.u32All;
    pPrimShaderCb->paSuVtxCntl  = m_commands.set.context.paSuVtxCntl.u32All;
    pPrimShaderCb->paClClipCntl = m_commands.set.context.paClClipCntl.u32All;
}

// =====================================================================================================================
// Overrides the RB+ register values for an RPM blit operation.  This is only valid to be called on GPU's which support
// RB+.
void GraphicsPipeline::OverrideRbPlusRegistersForRpm(
    SwizzledFormat           swizzledFormat,
    uint32                   slot,
    regSX_PS_DOWNCONVERT*    pSxPsDownconvert,
    regSX_BLEND_OPT_EPSILON* pSxBlendOptEpsilon,
    regSX_BLEND_OPT_CONTROL* pSxBlendOptControl
    ) const
{
    PAL_ASSERT(m_pDevice->Parent()->ChipProperties().gfx9.rbPlus != 0);

    const SwizzledFormat*const pTargetFormats = TargetFormats();

    if ((pTargetFormats[slot].format != swizzledFormat.format) &&
        (m_commands.set.context.cbColorControl.bits.DISABLE_DUAL_QUAD == 0))
    {
        regSX_PS_DOWNCONVERT    sxPsDownconvert   = { };
        regSX_BLEND_OPT_EPSILON sxBlendOptEpsilon = { };
        regSX_BLEND_OPT_CONTROL sxBlendOptControl = { };
        SetupRbPlusRegistersForSlot(slot,
                                    static_cast<uint8>(Formats::ComponentMask(swizzledFormat.format)),
                                    swizzledFormat,
                                    &sxPsDownconvert,
                                    &sxBlendOptEpsilon,
                                    &sxBlendOptControl);

        *pSxPsDownconvert   = sxPsDownconvert;
        *pSxBlendOptEpsilon = sxBlendOptEpsilon;
        *pSxBlendOptControl = sxBlendOptControl;
    }
}

// =====================================================================================================================
// Return if hardware stereo rendering is enabled.
bool GraphicsPipeline::HwStereoRenderingEnabled() const
{
    const auto&  device   = *(m_pDevice->Parent());
    uint32       enStereo = 0;

    if (m_gfxLevel == GfxIpLevel::GfxIp9)
    {
        if (IsVega12(device))
        {
            enStereo = m_commands.set.context.paStereoCntl.vg12.EN_STEREO;
        }
    }

    return (enStereo != 0);
}

// =====================================================================================================================
// Return if hardware stereo rendering uses multiple viewports.
bool GraphicsPipeline::HwStereoRenderingUsesMultipleViewports() const
{
    const auto&  palDevice  = *(m_pDevice->Parent());
    uint32       vpIdOffset = 0;

    {
        if (IsVega12(palDevice))
        {
            vpIdOffset = m_commands.set.context.paStereoCntl.vg12.VP_ID_OFFSET;
        }
    }

    return (vpIdOffset != 0);
}

// =====================================================================================================================
template <typename RegType>
static void SetPaStereoCntl(
    uint32    rtSliceOffset,
    uint32    vpIdOffset,
    RegType*  pPaStereoCntl)
{
    pPaStereoCntl->RT_SLICE_OFFSET = rtSliceOffset;
    pPaStereoCntl->VP_ID_OFFSET    = vpIdOffset;

    if ((rtSliceOffset != 0) || (vpIdOffset != 0))
    {
        pPaStereoCntl->EN_STEREO = 1;
    }
}

// =====================================================================================================================
// Setup hw stereo rendering related registers, this must be done after signature is initialized.
void GraphicsPipeline::SetupStereoRegisters()
{
    const Pal::Device&              device             = *(m_pDevice->Parent());
    const ViewInstancingDescriptor& viewInstancingDesc = GetViewInstancingDesc();
    bool viewInstancingEnable = false;

    if (viewInstancingDesc.viewInstanceCount > 1)
    {
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
                PAL_ASSERT(viewInstancingDesc.viewportArrayIdx[0] == 0);
                PAL_ASSERT(viewInstancingDesc.renderTargetArrayIdx[0] == 0);

                const uint32  vpIdOffset    = viewInstancingDesc.viewportArrayIdx[1];
                const uint32  rtSliceOffset = viewInstancingDesc.renderTargetArrayIdx[1];

                if (IsVega12(device))
                {
                    SetPaStereoCntl(rtSliceOffset, vpIdOffset, &m_commands.set.context.paStereoCntl.vg12);
                }
            }
        }
    }

}

// =====================================================================================================================
bool GraphicsPipeline::IsNggFastLaunch() const
{
    const auto&  device       = *(m_pDevice->Parent());
    const uint32 gsFastLaunch = (IsGfx091xPlus(device)
                                 ? m_commands.set.context.vgtShaderStagesEn.gfx09_1xPlus.GS_FAST_LAUNCH
                                 : m_commands.set.context.vgtShaderStagesEn.gfx09_0.GS_FAST_LAUNCH);

    return (gsFastLaunch != 0);
}

} // Gfx9
} // Pal
