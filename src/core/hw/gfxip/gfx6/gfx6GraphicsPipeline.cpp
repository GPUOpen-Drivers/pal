/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6ColorBlendState.h"
#include "core/hw/gfxip/gfx6/gfx6DepthStencilState.h"
#include "core/hw/gfxip/gfx6/gfx6DepthStencilView.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6GraphicsPipeline.h"
#include "palFormatInfo.h"
#include "palInlineFuncs.h"
#include "palPipelineAbiProcessorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
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
    NoUserDataSpilling,         // Spill threshold
    0,                          // User-data entry limit
    { UserDataNotMapped, },     // Compacted view ID register addresses
    { 0, },                     // User-data mapping hashes per-stage
};
static_assert(UserDataNotMapped == 0, "Unexpected value for indicating unmapped user-data entries!");

static uint8 Rop3(LogicOp logicOp);
static SX_DOWNCONVERT_FORMAT SxDownConvertFormat(ChNumFormat format);
static uint32 SxBlendOptEpsilon(SX_DOWNCONVERT_FORMAT sxDownConvertFormat);
static uint32 SxBlendOptControl(uint32 writeMask);

// Base count of SH registers which are loaded using LOAD_SH_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedShRegCount =
    1;  // mmSPI_SHADER_LATE_ALLOC_VS (only present on Gfx7+, but only Gfx8 supports LOAD_INDEX)

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
    1 + // mmSPI_INTERP_CONTROL_0
    1 + // mmVGT_VERTEX_REUSE_BLOCK_CNTL
    1;  // mmDB_SHADER_CONTROL (only Gfx7+ write it at bind-time, but only Gfx8+ supports LOAD_INDEX)

// Mask of DB_RENDER_OVERRIDE fields written during pipeline bind.
static constexpr uint32 DbRenderOverrideRmwMask = (DB_RENDER_OVERRIDE__FORCE_SHADER_Z_ORDER_MASK |
                                                   DB_RENDER_OVERRIDE__FORCE_STENCIL_READ_MASK   |
                                                   DB_RENDER_OVERRIDE__DISABLE_VIEWPORT_CLAMP_MASK);

static_assert((DbRenderOverrideRmwMask & DepthStencilView::DbRenderOverrideRmwMask) == 0,
              "GraphicsPipeline and DepthStencilView DB_RENDER_OVERRIDE fields intersect.  This would require"
              "delayed validation");

// =====================================================================================================================
// The workaround for the "DB Over-Rasterization" hardware bug requires us to write the DB_SHADER_CONTROL register at
// draw-time. This function writes the PM4 commands necessary and returns the next unused DWORD in pCmdSpace.
template <bool pm4OptImmediate>
uint32* GraphicsPipeline::WriteDbShaderControl(
    bool       isDepthEnabled,
    bool       usesOverRasterization,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // NOTE: If the PM4 header for writing DB_SHADER_CONTROL is not initialized and the LOAD_INDEX path's PM4 headers
    // are also not initialized, then the register must be written at draw-time due to a hardware workaround.
    if ((m_commands.set.context.hdrDbShaderControl.header.u32All == 0) &&
        (m_commands.loadIndex.context.loadCtxRegIndex.header.u32All == 0))
    {
        PAL_ASSERT(m_pDevice->WaDbOverRasterization());

        regDB_SHADER_CONTROL dbShaderControl = m_commands.set.context.dbShaderControl;
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

        pCmdSpace = pCmdStream->WriteSetOneContextReg<pm4OptImmediate>(mmDB_SHADER_CONTROL,
                                                                       dbShaderControl.u32All,
                                                                       pCmdSpace);
    }
#if PAL_ENABLE_PRINTS_ASSERTS
    else
    {
        PAL_ASSERT(m_pDevice->WaDbOverRasterization() == false);
    }
#endif

    return pCmdSpace;
}

template
uint32* GraphicsPipeline::WriteDbShaderControl<true>(
    bool       isDepthEnabled,
    bool       usesOverRasterization,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const;
template
uint32* GraphicsPipeline::WriteDbShaderControl<false>(
    bool       isDepthEnabled,
    bool       usesOverRasterization,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const;

// =====================================================================================================================
// Determines whether we can allow the hardware to render out-of-order primitives.  This is done by determing the
// effects that this could have on the depth buffer, stencil buffer, and render target.
bool GraphicsPipeline::CanDrawPrimsOutOfOrder(
    const DepthStencilView*  pDsView,
    const DepthStencilState* pDepthStencilState,
    const ColorBlendState*   pBlendState,
    uint32                   hasActiveQueries,
    OutOfOrderPrimMode       gfx7EnableOutOfOrderPrimitives
    ) const
{
    bool enableOutOfOrderPrims = true;

    if ((gfx7EnableOutOfOrderPrimitives == OutOfOrderPrimSafe) ||
        (gfx7EnableOutOfOrderPrimitives == OutOfOrderPrimAggressive))
    {
        if (PsWritesUavs() || pDepthStencilState == nullptr)
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

            if ((gfx7EnableOutOfOrderPrimitives == OutOfOrderPrimSafe) && (hasActiveQueries != 0))
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
                const bool canRenderTargetRunOutOfOrder =
                    (gfx7EnableOutOfOrderPrimitives == OutOfOrderPrimAggressive) &&
                    (pDepthStencilState->DepthForcesOrdering());

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
                                (gfx7EnableOutOfOrderPrimitives == OutOfOrderPrimAggressive));

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
    else if (gfx7EnableOutOfOrderPrimitives != OutOfOrderPrimAlways)
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
    m_pDevice(pDevice),
    m_contextRegHash(0),
    m_chunkLsHs(*pDevice,
                &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Ls)],
                &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Hs)]),
    m_chunkEsGs(*pDevice,
                &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Es)],
                &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Gs)]),
    m_chunkVsPs(*pDevice,
                &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Vs)],
                &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Ps)])
{
    memset(&m_iaMultiVgtParam[0],  0, sizeof(m_iaMultiVgtParam));

    memset(&m_commands, 0, sizeof(m_commands));
    memcpy(&m_signature, &NullGfxSignature, sizeof(m_signature));

    m_sxPsDownconvert.u32All   = 0;
    m_sxBlendOptEpsilon.u32All = 0;
    m_sxBlendOptControl.u32All = 0;
    m_vgtLsHsConfig.u32All     = 0;
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
    // VGT_SHADER_STAGES_EN and must be read first, since it determines which HW stages are active!
    m_commands.set.context.vgtShaderStagesEn.u32All = registers.At(mmVGT_SHADER_STAGES_EN);

    // Similarly, VGT_GS_MODE should also be read early, since it determines if on-chip GS is enabled.
    registers.HasEntry(mmVGT_GS_MODE, &m_commands.set.context.vgtGsMode.u32All);
    if (IsGsEnabled() && (m_commands.set.context.vgtGsMode.bits.ONCHIP__CI__VI == VgtGsModeOnchip))
    {
        SetIsGsOnChip(true);
    }

    // Must be called *after* determining active HW stages!
    SetupSignatureFromElf(metadata, registers, &pInfo->esGsLdsSizeRegGs, &pInfo->esGsLdsSizeRegVs);

    const Gfx6PalSettings& settings = m_pDevice->Settings();
    if (settings.enableLoadIndexForObjectBinds != false)
    {
        pInfo->loadedShRegCount  = BaseLoadedShRegCount;
        pInfo->loadedCtxRegCount = BaseLoadedCntxRegCount;
    }

    pInfo->usesOnChipGs = IsGsOnChip();

    if (IsTessEnabled())
    {
        m_chunkLsHs.EarlyInit(pInfo);
    }
    if (IsGsEnabled())
    {
        m_chunkEsGs.EarlyInit(pInfo);
    }
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
                m_chunkLsHs.LateInit(abiProcessor, registers, &uploader, &hasher);
            }
            if (IsGsEnabled())
            {
                m_chunkEsGs.LateInit(abiProcessor, metadata, registers, loadInfo, &uploader, &hasher);
            }
            m_chunkVsPs.LateInit(abiProcessor, registers, loadInfo, &uploader, &hasher);

            SetupCommonRegisters(createInfo, registers, &uploader);
            SetupNonShaderRegisters(createInfo, registers, &uploader);

            uploader.End();

            hasher.Update(m_commands.set.context);
            hasher.Update(m_commands.common);
            hasher.Finalize(reinterpret_cast<uint8* const>(&m_contextRegHash));

            m_pDevice->CmdUtil().BuildPipelinePrefetchPm4(uploader, &m_commands.prefetch);

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
        pInfo = (IsTessEnabled() ? &m_chunkLsHs.StageInfoLs()
                                 : (IsGsEnabled() ? &m_chunkEsGs.StageInfoEs()
                                                  : &m_chunkVsPs.StageInfoVs()));
        break;
    case ShaderType::Hull:
        pInfo = (IsTessEnabled() ? &m_chunkLsHs.StageInfoHs() : nullptr);
        break;
    case ShaderType::Domain:
        pInfo = (IsTessEnabled() ? (IsGsEnabled() ? &m_chunkEsGs.StageInfoEs()
                                                  : &m_chunkVsPs.StageInfoVs())
                                 : nullptr);
        break;
    case ShaderType::Geometry:
        pInfo = (IsGsEnabled() ? &m_chunkEsGs.StageInfoGs() : nullptr);
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
// Overrides the RB+ register values for an RPM blit operation.  This is only valid to be called on GPU's which support
// RB+.
void GraphicsPipeline::OverrideRbPlusRegistersForRpm(
    SwizzledFormat               swizzledFormat,
    uint32                       slot,
    regSX_PS_DOWNCONVERT__VI*    pSxPsDownconvert,
    regSX_BLEND_OPT_EPSILON__VI* pSxBlendOptEpsilon,
    regSX_BLEND_OPT_CONTROL__VI* pSxBlendOptControl
    ) const
{
    PAL_ASSERT(m_pDevice->Parent()->ChipProperties().gfx6.rbPlus != 0);

    const SwizzledFormat*const pTargetFormats = TargetFormats();

    if ((pTargetFormats[slot].format != swizzledFormat.format) &&
        (m_commands.set.context.cbColorControl.bits.DISABLE_DUAL_QUAD__VI == 0))
    {
        regSX_PS_DOWNCONVERT__VI    sxPsDownconvert   = { };
        regSX_BLEND_OPT_EPSILON__VI sxBlendOptEpsilon = { };
        regSX_BLEND_OPT_CONTROL__VI sxBlendOptControl = { };
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
// Helper function to compute the WAVE_LIMIT field of the SPI_SHADER_PGM_RSRC3* registers.
uint32 GraphicsPipeline::CalcMaxWavesPerSh(
    uint32 maxWavesPerCu
    ) const
{
    // The maximum number of waves per SH in "register units".
    // By default set the WAVE_LIMIT field to be unlimited.
    // Limits given by the ELF will only apply if the caller doesn't set their own limit.
    uint32 wavesPerSh = 0;

    // If the caller would like to override the default maxWavesPerCu
    if (maxWavesPerCu > 0)
    {
        const auto&      gfx6ChipProps                 = m_pDevice->Parent()->ChipProperties().gfx6;
        const uint32     numWavefrontsPerCu            = gfx6ChipProps.numSimdPerCu * gfx6ChipProps.numWavesPerSimd;
        const uint32     maxWavesPerShGraphics         = gfx6ChipProps.maxNumCuPerSh * numWavefrontsPerCu;
        constexpr uint32 MaxWavesPerShGraphicsUnitSize = 16u;

        // We assume no one is trying to use more than 100% of all waves.
        PAL_ASSERT(maxWavesPerCu <= numWavefrontsPerCu);
        const uint32 maxWavesPerSh = (maxWavesPerCu * gfx6ChipProps.numCuPerSh);

        // For graphics shaders, the WAVE_LIMIT field is in units of 16 waves and must not exceed 63. We must also clamp
        // to one if maxWavesPerSh rounded down to zero to prevent the limit from being removed.
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
    pStageInfo->wavesPerSh   = CalcMaxWavesPerSh(shaderInfo.maxWavesPerCu);
    pStageInfo->cuEnableMask = shaderInfo.cuEnableMask;
}

// =====================================================================================================================
// Helper for setting all the dynamic stage infos.
void GraphicsPipeline::CalcDynamicStageInfos(
    const DynamicGraphicsShaderInfos& graphicsInfo,
    DynamicStageInfos*                pStageInfos
    ) const
{
    if (m_pDevice->Parent()->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp7)
    {
        CalcDynamicStageInfo(graphicsInfo.ps, &pStageInfos->ps);

        if (IsTessEnabled())
        {
            CalcDynamicStageInfo(graphicsInfo.vs, &pStageInfos->ls);
            CalcDynamicStageInfo(graphicsInfo.hs, &pStageInfos->hs);

            if (IsGsEnabled())
            {
                // PipelineGsTess
                // API Shader -> Hardware Stage
                // PS -> PS
                // VS -> LS
                // HS -> HS
                // DS -> ES
                // GS -> GS
                CalcDynamicStageInfo(graphicsInfo.ds, &pStageInfos->es);
                CalcDynamicStageInfo(graphicsInfo.gs, &pStageInfos->gs);
            }
            else
            {
                // PipelineTess
                // API Shader -> Hardware Stage
                // PS -> PS
                // VS -> LS
                // HS -> HS
                // DS -> VS
                CalcDynamicStageInfo(graphicsInfo.ds, &pStageInfos->vs);
            }
        }
        else
        {
            if (IsGsEnabled())
            {
                // PipelineGs
                // API Shader -> Hardware Stage
                // PS -> PS
                // VS -> ES
                // GS -> GS
                CalcDynamicStageInfo(graphicsInfo.vs, &pStageInfos->es);
                CalcDynamicStageInfo(graphicsInfo.gs, &pStageInfos->gs);
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
}

// =====================================================================================================================
// Helper function for writing common PM4 images which are shared by all graphics pipelines.
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
        pCmdSpace = m_chunkLsHs.WriteShCommands(pCmdStream, pCmdSpace, stageInfos.ls, stageInfos.hs);
    }
    if (IsGsEnabled())
    {
        pCmdSpace = m_chunkEsGs.WriteShCommands(pCmdStream, pCmdSpace, stageInfos.es, stageInfos.gs);
    }
    pCmdSpace = m_chunkVsPs.WriteShCommands(pCmdStream, pCmdSpace, stageInfos.vs, stageInfos.ps);

    // NOTE: It is possible for neither of the below branches to be taken for HW which doesn't support Late-Alloc VS
    // (such as Gfx6).
    if (m_commands.set.sh.hdrSpiShaderLateAllocVs.header.u32All != 0)
    {
        // The SET path's PM4 headers will be initialized if the current GPU doesn't support the updated microcode.  In
        // that case, we must fall back to the "legacy" SET path.
        constexpr uint32 SpaceNeededSet = sizeof(m_commands.set.sh) / sizeof(uint32);
        pCmdSpace = pCmdStream->WritePm4Image(SpaceNeededSet, &m_commands.set.sh, pCmdSpace);
    }
    else if (m_commands.loadIndex.sh.loadShRegIndex.header.u32All != 0)
    {
        // The LOAD_INDEX path can only be used if its PM4 header is initialized (this indicates that the LOAD_INDEX
        // path is supported by the current GPU's microcode).
        constexpr uint32 SpaceNeededLoad = sizeof(m_commands.loadIndex.sh) / sizeof(uint32);
        pCmdSpace = pCmdStream->WritePm4Image(SpaceNeededLoad, &m_commands.loadIndex.sh, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Helper function for writing context PM4 images which are shared by all graphics pipelines.
// Returns a command buffer pointer incremented to the end of the commands we just wrote.
uint32* GraphicsPipeline::WriteContextCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    PAL_ASSERT(pCmdStream != nullptr);

    if (m_commands.set.context.spaceNeeded != 0)
    {
        // The SET path's PM4 size will be initialized if the current GPU doesn't support the updated microcode.  In
        // that case, we must fall back to the "legacy" SET path.
        pCmdSpace = pCmdStream->WritePm4Image(m_commands.set.context.spaceNeeded, &m_commands.set.context, pCmdSpace);

        if (IsTessEnabled())
        {
            pCmdSpace = m_chunkLsHs.WriteContextCommands(pCmdStream, pCmdSpace);
        }
        if (IsGsEnabled())
        {
            pCmdSpace = m_chunkEsGs.WriteContextCommands(pCmdStream, pCmdSpace);
        }
        pCmdSpace = m_chunkVsPs.WriteContextCommands(pCmdStream, pCmdSpace);
    }
    else
    {
        PAL_ASSERT(m_commands.loadIndex.context.loadCtxRegIndex.header.u32All != 0);

        // The LOAD_INDEX path can only be used if its PM4 header is initialized (this indicates that the LOAD_INDEX
        // path is supported by the current GPU's microcode).
        constexpr uint32 SpaceNeededLoad = sizeof(m_commands.loadIndex.context) / sizeof(uint32);
        pCmdSpace = pCmdStream->WritePm4Image(SpaceNeededLoad, &m_commands.loadIndex.context, pCmdSpace);
    }

    constexpr uint32 SpaceNeededCommon = sizeof(m_commands.common) / sizeof(uint32);
    return pCmdStream->WritePm4Image(SpaceNeededCommon, &m_commands.common, pCmdSpace);
}

// =====================================================================================================================
// Requests that this pipeline indicates what it would like to prefetch.
uint32* GraphicsPipeline::Prefetch(
    uint32* pCmdSpace
    ) const
{
    memcpy(pCmdSpace, &m_commands.prefetch, m_commands.prefetch.spaceNeeded * sizeof(uint32));

    return pCmdSpace + m_commands.prefetch.spaceNeeded;
}

// =====================================================================================================================
// Builds the packet headers for the various PM4 images associated with this pipeline.  Register values and packet
// payloads are computed elsewhere.
void GraphicsPipeline::BuildPm4Headers(
    const GraphicsPipelineUploader& uploader)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    const CmdUtil&           cmdUtil   = m_pDevice->CmdUtil();

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
        if (chipProps.gfxLevel != GfxIpLevel::GfxIp6)
        {
            cmdUtil.BuildSetOneShReg(mmSPI_SHADER_LATE_ALLOC_VS__CI__VI,
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

        m_commands.set.context.spaceNeeded +=
            cmdUtil.BuildSetOneContextReg(mmSPI_INTERP_CONTROL_0, &m_commands.set.context.hdrSpiInterpControl0);

        m_commands.set.context.spaceNeeded +=
            cmdUtil.BuildSetOneContextReg(mmVGT_VERTEX_REUSE_BLOCK_CNTL,
                                          &m_commands.set.context.hdrVgtVertexReuseBlockCntl);

        if (m_pDevice->WaDbOverRasterization() == false)
        {
            // This hardware workaround requires draw-time validation for DB_SHADER_CONTROL.  If the current GPU is
            // not affected by this HW bug, we can just put it into the pipeline PM4 image.
            m_commands.set.context.spaceNeeded +=
                cmdUtil.BuildSetOneContextReg(mmDB_SHADER_CONTROL, &m_commands.set.context.hdrDbShaderControl);
        }
    }

    cmdUtil.BuildContextRegRmw(mmDB_ALPHA_TO_MASK,
                               DB_ALPHA_TO_MASK__ALPHA_TO_MASK_ENABLE_MASK,
                               0,
                               &m_commands.common.dbAlphaToMask);
    cmdUtil.BuildContextRegRmw(mmDB_RENDER_OVERRIDE,
                               DbRenderOverrideRmwMask,
                               0,
                               &m_commands.common.dbRenderOverride);
}

// =====================================================================================================================
// Updates the RB+ register values for a single render target slot.  It is only expected that this will be called for
// pipelines with RB+ enabled.
void GraphicsPipeline::SetupRbPlusRegistersForSlot(
    uint32                       slot,
    uint8                        writeMask,
    SwizzledFormat               swizzledFormat,
    regSX_PS_DOWNCONVERT__VI*    pSxPsDownconvert,
    regSX_BLEND_OPT_EPSILON__VI* pSxBlendOptEpsilon,
    regSX_BLEND_OPT_CONTROL__VI* pSxBlendOptControl
    ) const
{
    const uint32 bitShift = (4 * slot);

    const SX_DOWNCONVERT_FORMAT downConvertFormat = SxDownConvertFormat(swizzledFormat.format);
    const uint32                blendOptControl   = Gfx6::SxBlendOptControl(writeMask);
    const uint32                blendOptEpsilon   =
        (downConvertFormat == SX_RT_EXPORT_NO_CONVERSION) ? 0 : Gfx6::SxBlendOptEpsilon(downConvertFormat);

    pSxPsDownconvert->u32All &= ~(SX_PS_DOWNCONVERT__MRT0_MASK__VI << bitShift);
    pSxPsDownconvert->u32All |= (downConvertFormat << bitShift);

    pSxBlendOptEpsilon->u32All &= ~(SX_BLEND_OPT_EPSILON__MRT0_EPSILON_MASK__VI << bitShift);
    pSxBlendOptEpsilon->u32All |= (blendOptEpsilon << bitShift);

    pSxBlendOptControl->u32All &= ~((SX_BLEND_OPT_CONTROL__MRT0_COLOR_OPT_DISABLE_MASK__VI |
                                     SX_BLEND_OPT_CONTROL__MRT0_ALPHA_OPT_DISABLE_MASK__VI) << bitShift);
    pSxBlendOptControl->u32All |= (blendOptControl << bitShift);
}

// =====================================================================================================================
// Initializes render-state registers which aren't part of any hardware shader stage.
void GraphicsPipeline::SetupNonShaderRegisters(
    const GraphicsPipelineCreateInfo& createInfo,
    const RegisterVector&             registers,
    GraphicsPipelineUploader*         pUploader)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    const Gfx6PalSettings&   settings  = m_pDevice->Settings();

    m_commands.set.context.paScLineCntl.bits.EXPAND_LINE_WIDTH        = createInfo.rsState.expandLineWidth;
    m_commands.set.context.paScLineCntl.bits.DX10_DIAMOND_TEST_ENA    = 1;
    m_commands.set.context.paScLineCntl.bits.LAST_PIXEL               = createInfo.rsState.rasterizeLastLinePixel;
    m_commands.set.context.paScLineCntl.bits.PERPENDICULAR_ENDCAP_ENA = createInfo.rsState.perpLineEndCapsEnable;

    m_commands.set.context.cbShaderMask.u32All = registers.At(mmCB_SHADER_MASK);
    // CB_TARGET_MASK is determined by the RT write masks in the pipeline create info.
    for (uint32 rt = 0; rt < MaxColorTargets; ++rt)
    {
        const uint32 rtShift = (rt * 4); // Each RT uses four bits of CB_TARGET_MASK.
        m_commands.set.context.cbTargetMask.u32All |=
            ((createInfo.cbState.target[rt].channelWriteMask & 0xF) << rtShift);
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
        m_commands.set.context.cbColorControl.bits.MODE = CB_DCC_DECOMPRESS__VI;
        m_commands.set.context.cbColorControl.bits.ROP3 = Rop3(LogicOp::Copy);

        // According to the reg-spec, DCC decompress ops imply fmask decompress and fast-clear eliminate operations as
        // well, so set these registers as they would be set above.
        m_commands.set.context.cbShaderMask.u32All      = 0xF;
        m_commands.set.context.cbTargetMask.u32All      = 0xF;
    }
    else if (IsResolveFixedFunc())
    {
        m_commands.set.context.cbColorControl.bits.MODE = CB_RESOLVE;
        m_commands.set.context.cbColorControl.bits.ROP3 = Rop3(LogicOp::Copy);

        m_commands.set.context.cbShaderMask.bits.OUTPUT0_ENABLE = 0xF;
        m_commands.set.context.cbTargetMask.bits.TARGET0_ENABLE = 0xF;
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

    // We need to set the enable bit for alpha to mask dithering, but MSAA state also sets some fields of this register
    // so we must use a read/modify/write packet so we only update the _ENABLE field.
    regDB_ALPHA_TO_MASK dbAlphaToMask = { };
    dbAlphaToMask.bits.ALPHA_TO_MASK_ENABLE = createInfo.cbState.alphaToCoverageEnable;
    m_commands.common.dbAlphaToMask.regData = dbAlphaToMask.u32All;

    // Initialize RB+ registers for pipelines which are able to use the feature.
    if (settings.gfx8RbPlusEnable &&
        (createInfo.cbState.dualSourceBlendEnable == false) &&
        (m_commands.set.context.cbColorControl.bits.MODE != CB_RESOLVE))
    {
        PAL_ASSERT(chipProps.gfx6.rbPlus);

        m_commands.set.context.cbColorControl.bits.DISABLE_DUAL_QUAD__VI = 0;

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
    else if (chipProps.gfx6.rbPlus != 0)
    {
        // If RB+ is supported but not enabled, we need to set DISABLE_DUAL_QUAD.
        m_commands.set.context.cbColorControl.bits.DISABLE_DUAL_QUAD__VI = 1;
    }

    // Override some register settings based on toss points.  These toss points cannot be processed in the hardware
    // independent class because they cannot be overridden by altering the pipeline creation info.
    if (IsInternal() == false)
    {
        switch (m_pDevice->Parent()->Settings().tossPointMode)
        {
        case TossPointAfterPs:
            // This toss point is used to disable all color buffer writes.
            m_commands.set.context.cbTargetMask.u32All = 0;
            break;
        default:
            break;
        }
    }

    if (pUploader->EnableLoadIndexPath())
    {
        pUploader->AddCtxReg(mmPA_SC_LINE_CNTL,  m_commands.set.context.paScLineCntl);
        pUploader->AddCtxReg(mmCB_COLOR_CONTROL, m_commands.set.context.cbColorControl);
        pUploader->AddCtxReg(mmCB_SHADER_MASK,   m_commands.set.context.cbShaderMask);
        pUploader->AddCtxReg(mmCB_TARGET_MASK,   m_commands.set.context.cbTargetMask);
    }
}

// =====================================================================================================================
// Initializes render-state registers which are associated with multiple hardware shader stages.
void GraphicsPipeline::SetupCommonRegisters(
    const GraphicsPipelineCreateInfo& createInfo,
    const RegisterVector&             registers,
    GraphicsPipelineUploader*         pUploader)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    const Gfx6PalSettings&   settings  = m_pDevice->Settings();

    m_commands.set.context.paClClipCntl.u32All = registers.At(mmPA_CL_CLIP_CNTL);
    m_commands.set.context.paClVteCntl.u32All  = registers.At(mmPA_CL_VTE_CNTL);
    m_commands.set.context.paSuVtxCntl.u32All  = registers.At(mmPA_SU_VTX_CNTL);
    m_paScModeCntl1.u32All                     = registers.At(mmPA_SC_MODE_CNTL_1);

    // Overrides some of the fields in PA_SC_MODE_CNTL1 to account for GPU pipe config and features like out-of-order
    // rasterization.

    // The maximum value for OUT_OF_ORDER_WATER_MARK is 7.
    constexpr uint32 MaxOutOfOrderWatermark = 7;
    m_paScModeCntl1.bits.OUT_OF_ORDER_WATER_MARK = Min(MaxOutOfOrderWatermark, settings.gfx7OutOfOrderWatermark);

    if (createInfo.rsState.outOfOrderPrimsEnable &&
        (settings.gfx7EnableOutOfOrderPrimitives != OutOfOrderPrimDisable))
    {
        m_paScModeCntl1.bits.OUT_OF_ORDER_PRIMITIVE_ENABLE = 1;
    }

    // Hardware team recommendation is to set WALK_FENCE_SIZE to 512 pixels for 4/8/16 pipes and 256 pixels for 2 pipes.
    // NOTE: the KMD reported quad-pipe number is unreliable so we'll use the PIPE_CONFIG field of GB_TILE_MODE0 to
    // determine this ourselves.
    regGB_TILE_MODE0 gbTileMode0;
    gbTileMode0.u32All = chipProps.gfx6.gbTileMode[0];
    switch (gbTileMode0.bits.PIPE_CONFIG)
    {
        // 2 Pipes (fall-throughs intentional):
    case ADDR_SURF_P2:
    case ADDR_SURF_P2_RESERVED0:
    case ADDR_SURF_P2_RESERVED1:
    case ADDR_SURF_P2_RESERVED2:
        // NOTE: a register field value of 2 means "256 pixels".
        m_paScModeCntl1.bits.WALK_FENCE_SIZE = 2;
        break;
        // 4 Pipes (fall-throughs intentional):
    case ADDR_SURF_P4_8x16:
    case ADDR_SURF_P4_16x16:
    case ADDR_SURF_P4_16x32:
    case ADDR_SURF_P4_32x32:
        // 8 Pipes (fall-throughs intentional):
    case ADDR_SURF_P8_16x16_8x16:
    case ADDR_SURF_P8_16x32_8x16:
    case ADDR_SURF_P8_32x32_8x16:
    case ADDR_SURF_P8_16x32_16x16:
    case ADDR_SURF_P8_32x32_16x16:
    case ADDR_SURF_P8_32x32_16x32:
    case ADDR_SURF_P8_32x64_32x32:
        // 16 Pipes (fall-throughs intentional):
    case ADDR_SURF_P16_32x32_8x16__CI__VI:
    case ADDR_SURF_P16_32x32_16x16__CI__VI:
        // NOTE: a register field value of 3 means "512 pixels".
        m_paScModeCntl1.bits.WALK_FENCE_SIZE = 3;
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 387
    m_info.ps.flags.perSampleShading = m_paScModeCntl1.bits.PS_ITER_SAMPLE;
#endif

    m_commands.set.context.dbShaderControl.u32All = registers.At(mmDB_SHADER_CONTROL);

    regDB_RENDER_OVERRIDE dbRenderOverride = { };
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 381
    if ((createInfo.rsState.depthClampDisable == true) &&
#else
    if ((createInfo.rsState.depthClampEnable == false) &&
#endif
        (m_commands.set.context.dbShaderControl.bits.Z_EXPORT_ENABLE != 0))
    {
        dbRenderOverride.bits.DISABLE_VIEWPORT_CLAMP = 1;
    }

    // NOTE: On recommendation from h/ware team FORCE_SHADER_Z_ORDER will be set whenever Re-Z is being used.
    dbRenderOverride.bits.FORCE_SHADER_Z_ORDER = (m_commands.set.context.dbShaderControl.bits.Z_ORDER == RE_Z);

    // NOTE: The Re-Z Stencil corruption bug workaround requires setting FORCE_STENCIL_READ in DB_RENDER_OVERRIDE
    // whenever Re-Z is active.
    if (m_pDevice->WaDbReZStencilCorruption() &&
        ((m_commands.set.context.dbShaderControl.bits.Z_ORDER == RE_Z) ||
         (m_commands.set.context.dbShaderControl.bits.Z_ORDER == EARLY_Z_THEN_RE_Z)))
    {
        dbRenderOverride.bits.FORCE_STENCIL_READ = 1;
    }

    m_commands.common.dbRenderOverride.regData = dbRenderOverride.u32All;

    m_commands.set.context.vgtReuseOff.u32All = registers.At(mmVGT_REUSE_OFF);

    // NOTE: The following registers are assumed to have the value zero if the pipeline ELF does not specify values.
    registers.HasEntry(mmVGT_TF_PARAM,     &m_commands.set.context.vgtTfParam.u32All);
    registers.HasEntry(mmVGT_LS_HS_CONFIG, &m_vgtLsHsConfig.u32All);

    // If dynamic tessellation mode is enabled (where the shader chooses whether each patch goes to off-chip or to
    // on-chip memory), we should override DS_WAVES_PER_SIMD according to the panel setting.
    if ((m_commands.set.context.vgtTfParam.bits.NUM_DS_WAVES_PER_SIMD != 0) &&
        (m_commands.set.context.vgtShaderStagesEn.bits.DYNAMIC_HS != 0))
    {
        m_commands.set.context.vgtTfParam.bits.NUM_DS_WAVES_PER_SIMD = settings.dsWavesPerSimdOverflow;
    }

    // For Gfx6+, default VTX_REUSE_DEPTH to 14
    m_commands.set.context.vgtVertexReuseBlockCntl.u32All = 0;
    m_commands.set.context.vgtVertexReuseBlockCntl.bits.VTX_REUSE_DEPTH = 14;

    // On Gfx8+, if half-pack mode is disabled we can override the legacy VTX_REUSE_DEPTH with a more optimal value.
    if ((chipProps.gfxLevel >= GfxIpLevel::GfxIp8) && (settings.vsHalfPackThreshold >= MaxVsExportSemantics))
    {
        // Degenerate primitive filtering with fractional odd tessellation requires a VTX_REUSE_DEPTH of 14.  Only
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
        pUploader->AddCtxReg(mmPA_CL_CLIP_CNTL,             m_commands.set.context.paClClipCntl);
        pUploader->AddCtxReg(mmPA_CL_VTE_CNTL,              m_commands.set.context.paClVteCntl);
        pUploader->AddCtxReg(mmPA_SU_VTX_CNTL,              m_commands.set.context.paSuVtxCntl);
        pUploader->AddCtxReg(mmDB_SHADER_CONTROL,           m_commands.set.context.dbShaderControl);
        pUploader->AddCtxReg(mmVGT_SHADER_STAGES_EN,        m_commands.set.context.vgtShaderStagesEn);
        pUploader->AddCtxReg(mmVGT_GS_MODE,                 m_commands.set.context.vgtGsMode);
        pUploader->AddCtxReg(mmVGT_REUSE_OFF,               m_commands.set.context.vgtReuseOff);
        pUploader->AddCtxReg(mmVGT_TF_PARAM,                m_commands.set.context.vgtTfParam);
        pUploader->AddCtxReg(mmVGT_VERTEX_REUSE_BLOCK_CNTL, m_commands.set.context.vgtVertexReuseBlockCntl);
        pUploader->AddCtxReg(mmSPI_INTERP_CONTROL_0,        m_commands.set.context.spiInterpControl0);
    }

    SetupLateAllocVs(registers, pUploader);
    SetupIaMultiVgtParam(registers);
}

// =====================================================================================================================
// The pipeline binary is allowed to partially specify the value for IA_MULTI_VGT_PARAM.  PAL will finish initializing
// this register based on GPU properties, hardware workarounds, pipeline create info, and the values of other registers.
void GraphicsPipeline::SetupIaMultiVgtParam(
    const RegisterVector& registers)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    const Gfx6PalSettings&   settings  = m_pDevice->Settings();

    regIA_MULTI_VGT_PARAM iaMultiVgtParam = { };
    registers.HasEntry(mmIA_MULTI_VGT_PARAM, &iaMultiVgtParam.u32All);

    regVGT_STRMOUT_CONFIG vgtStrmoutConfig = { };
    registers.HasEntry(mmVGT_STRMOUT_CONFIG, &vgtStrmoutConfig.u32All);

    if (IsTessEnabled())
    {
        // The hardware requires that the primgroup size matches the number of HS patches-per-thread-group when
        // tessellation is enabled.
        iaMultiVgtParam.bits.PRIMGROUP_SIZE = (m_vgtLsHsConfig.bits.NUM_PATCHES - 1);
    }
    else if (IsGsEnabled() && (m_vgtLsHsConfig.bits.HS_NUM_INPUT_CP != 0))
    {
        // The hardware requires that the primgroup size must not exceed (256/ number of HS input control points) when
        // a GS shader accepts patch primitives as input.
        iaMultiVgtParam.bits.PRIMGROUP_SIZE = ((256 / m_vgtLsHsConfig.bits.HS_NUM_INPUT_CP) - 1);
    }
    else
    {
        // Just use the primitive group size specified by the pipeline binary.  Zero is a valid value here in case
        // the binary didn't specify a value for PRIMGROUP_SIZE.
    }

    if (IsGsEnabled() && IsGsOnChip())
    {
        // NOTE: The hardware will automatically set PARTIAL_ES_WAVE_ON when on-chip GS is active, so we should do
        // the same to track what the chip really sees.
        iaMultiVgtParam.bits.PARTIAL_ES_WAVE_ON = 1;
    }

    if ((settings.waMiscGsNullPrim != false) && IsTessEnabled() && IsGsEnabled())
    {
        // There is a GS deadlock scenario on some 2-SE parts which is caused when null primitives back up one SE,
        // deadlocking the VGT and PA.  Forcing PARTIAL_VS_WAVE_ON when GS and tessellation are both enabled works
        // around the issue.
        iaMultiVgtParam.bits.PARTIAL_VS_WAVE_ON = 1;
    }

    for (uint32 idx = 0; idx < NumIaMultiVgtParam; ++idx)
    {
        m_iaMultiVgtParam[idx] = iaMultiVgtParam;

        // Additional setup for this register is required on Gfx7+ hardware.
        if (chipProps.gfxLevel > GfxIpLevel::GfxIp6)
        {
            FixupIaMultiVgtParamOnGfx7Plus((idx != 0), &m_iaMultiVgtParam[idx]);
        }

        // NOTE: The PRIMGROUP_SIZE field IA_MULTI_VGT_PARAM must be less than 256 if stream output and
        // PARTIAL_ES_WAVE_ON are both enabled on 2-SE hardware.
        if ((vgtStrmoutConfig.u32All != 0)         &&
            (chipProps.gfx6.numShaderEngines == 2) &&
            (m_iaMultiVgtParam[idx].bits.PARTIAL_ES_WAVE_ON == 0))
        {
            PAL_ASSERT(m_iaMultiVgtParam[idx].bits.PRIMGROUP_SIZE < 256);
        }
    }
}

// =====================================================================================================================
// Performs additional validation and setup for IA_MULTI_VGT_PARAM for Gfx7 and newer GPUs.
void GraphicsPipeline::FixupIaMultiVgtParamOnGfx7Plus(
    bool                   forceWdSwitchOnEop,
    regIA_MULTI_VGT_PARAM* pIaMultiVgtParam
    ) const
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    PAL_ASSERT(chipProps.gfxLevel != GfxIpLevel::GfxIp6);

    if (IsGsEnabled())
    {
        // NOTE: The GS table is a storage structure in the hardware.  It keeps track of all outstanding GS waves from
        // creation to dealloc.  When Partial ES Wave is off the VGT combines ES waves across primgroups.  In this case
        // more GS table entries may be needed.  This reserved space ensures the worst case is handled as recommended by
        // VGT HW engineers.
        constexpr uint32 GsTableDepthReservedForEsWave = 3;

        // Preferred number of GS primitives per ES thread.
        constexpr uint32 GsPrimsPerEsThread = 256;

        if ((GsPrimsPerEsThread / (pIaMultiVgtParam->bits.PRIMGROUP_SIZE + 1)) >=
            (chipProps.gfx6.gsVgtTableDepth - GsTableDepthReservedForEsWave))
        {
            // Typically, this case will be hit when tessellation is on because PRIMGROUP_SIZE is set to the number of
            // patches per TG, optimally around 8.  For non-tessellated draws PRIMGROUP_SIZE is set larger.
            pIaMultiVgtParam->bits.PARTIAL_ES_WAVE_ON = 1;
        }
    }

    if (chipProps.gfxLevel >= GfxIpLevel::GfxIp8)
    {
        // According to the register spec:
        //
        // Max number of primgroups that can be combined into a single ES or VS wave.  This is ignored if
        // PARTIAL_ES_WAVE_ON or PARTIAL_VS_WAVE_ON is set (for ES and VS).  It is also ignored when programmed to 0
        // (should be programmed to 2 by default)
        pIaMultiVgtParam->bits.MAX_PRIMGRP_IN_WAVE__VI = 2;

        if (m_commands.set.context.vgtTfParam.bits.DISTRIBUTION_MODE__VI != NO_DIST)
        {
            // Verify a few assumptions given that distributed tessellation is enabled:
            //     - Tessellation itself is enabled;
            //     - VGT is configured to send all DS wavefronts to off-chip memory.
            PAL_ASSERT(IsTessEnabled() && (m_commands.set.context.vgtTfParam.bits.NUM_DS_WAVES_PER_SIMD == 0));

            // When distributed tessellation is active, VI hardware requires PARTIAL_ES_WAVE_ON if the GS is present,
            // and PARTIAL_VS_WAVE_ON when the GS is absent.
            if (IsGsEnabled())
            {
                pIaMultiVgtParam->bits.PARTIAL_ES_WAVE_ON = 1;
            }
            else
            {
                pIaMultiVgtParam->bits.PARTIAL_VS_WAVE_ON = 1;
            }
        }

        // NOTE: HW engineers suggested that PARTIAL_VS_WAVE_ON should be programmed to 1 for both on-chip
        // and off-chip GS to work around an issue of system hang.
        if (IsGsEnabled() && (m_pDevice->WaShaderOffChipGsHang() != false))
        {
            pIaMultiVgtParam->bits.PARTIAL_VS_WAVE_ON = 1;
        }
    }
    else
    {
        PAL_ASSERT(m_commands.set.context.vgtTfParam.bits.DISTRIBUTION_MODE__VI == NO_DIST);
    }

    // According to the VGT folks, WD_SWITCH_ON_EOP needs to be set whenever any of the following conditions are met.
    // Furthermore, the hardware will automatically set the bit for any part which has <= 2 shader engines.

    if ((pIaMultiVgtParam->bits.SWITCH_ON_EOP == 1) || // Illegal to have IA switch VGTs on EOP without WD switch IAs
                                                       // on EOP also.
        (chipProps.gfx6.numShaderEngines <= 2)      || // For 2-SE systems, WD_SWITCH_ON_EOP = 1 implicitly
        forceWdSwitchOnEop)                            // External condition (e.g. incompatible prim topology or opaque
                                                       // draw) are requiring WD_SWITCH_ON_EOP.
    {
        pIaMultiVgtParam->bits.WD_SWITCH_ON_EOP__CI__VI = 1;
    }
    else
    {
        pIaMultiVgtParam->bits.WD_SWITCH_ON_EOP__CI__VI = 0;

        // Hardware requires SWITCH_ON_EOI (and therefore PARTIAL_ES_WAVE_ON) to be set whenever WD_SWITCH_ON_EOP is
        // zero.
        pIaMultiVgtParam->bits.SWITCH_ON_EOI            = 1;
        pIaMultiVgtParam->bits.PARTIAL_ES_WAVE_ON       = 1;
    }

    // When SWITCH_ON_EOI is enabled, PARTIAL_VS_WAVE_ON should be set for instanced draws on all GPU's.  On Gfx7 GPU's
    // with more than two shader engines, PARTIAL_VS_WAVE_ON should always be set if SWITCH_ON_EOI is enabled.
    const bool requirePartialVsWaveWithEoi =
            ((chipProps.gfxLevel == GfxIpLevel::GfxIp7) && (chipProps.gfx6.numShaderEngines > 2));

    if ((pIaMultiVgtParam->bits.SWITCH_ON_EOI == 1) && requirePartialVsWaveWithEoi)
    {
        pIaMultiVgtParam->bits.PARTIAL_VS_WAVE_ON = 1;
    }
}

// =====================================================================================================================
// Initializes the SPI_SHADER_LATE_ALLOC_VS register for GFX7 and newer hardware.
void GraphicsPipeline::SetupLateAllocVs(
    const RegisterVector&     registers,
    GraphicsPipelineUploader* pUploader)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    if (chipProps.gfxLevel != GfxIpLevel::GfxIp6)
    {
        const Gfx6PalSettings& settings = m_pDevice->Settings();
        const auto pPalSettings = m_pDevice->Parent()->GetPublicSettings();

        regSPI_SHADER_PGM_RSRC1_VS spiShaderPgmRsrc1Vs = { };
        spiShaderPgmRsrc1Vs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_VS);

        regSPI_SHADER_PGM_RSRC2_VS spiShaderPgmRsrc2Vs = { };
        spiShaderPgmRsrc2Vs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_VS);

        regSPI_SHADER_PGM_RSRC2_PS spiShaderPgmRsrc2Ps = { };
        spiShaderPgmRsrc2Ps.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_PS);

        // Default to a late-alloc limit of zero.  This will nearly mimic the GFX6 behavior where VS waves don't
        // launch without allocating export space.
        uint32 lateAllocLimit = 0;

        // Maximum value of the LIMIT field of the SPI_SHADER_LATE_ALLOC_VS register.  It is the number of wavefronts
        // minus one.
        const uint32 maxLateAllocLimit = (chipProps.gfxip.maxLateAllocVsLimit - 1);

        // Target late-alloc limit uses PAL settings by default.  The lateAllocVsLimit member from graphicsPipeline
        // can override this setting if corresponding flag is set.
        const uint32 targetLateAllocLimit = IsLateAllocVsLimit() ? GetLateAllocVsLimit() : m_pDevice->LateAllocVsLimit();

        const uint32 vsNumSgpr = (spiShaderPgmRsrc1Vs.bits.SGPRS * 8);
        const uint32 vsNumVgpr = (spiShaderPgmRsrc1Vs.bits.VGPRS * 4);

        if (m_pDevice->UseFixedLateAllocVsLimit())
        {
            // When using the fixed wave limit scheme, just accept the client or device specified target value.  The
            // fixed scheme mandates that we are disabling a CU from running VS work, so any limit the client may
            // have specified is safe.
            lateAllocLimit = targetLateAllocLimit;
        }
        else if ((targetLateAllocLimit > 0) && (vsNumSgpr > 0) && (vsNumVgpr > 0))
        {
            const auto& gpuInfo = m_pDevice->Parent()->ChipProperties().gfx6;

            // Start by assuming the target late-alloc limit will be acceptable.  The limit is per SH and we need to
            // determine the maximum number of HW-VS wavefronts which can be launched per SH based on the shader's
            // resource usage.
            lateAllocLimit = targetLateAllocLimit;

            // SPI_SHADER_LATE_ALLOC_VS setting should be based on the "always on" CUs instead of all configured CUs
            // for all ASICS, however, this issue is caused by the side effect of LBPG while PG is applied to APU
            // (and Verde as the only dGPU), and Late_Alloc_VS as a feature is CI+ and Carrizo is the only asic that
            // we know has issue, so choose to enable this for Cz (i.e, settings.gfx7LateAllocVsOnCuAlwaysOn is set
            // to true for Carrizo only for now).
            uint32 numCuForLateAllocVs = gpuInfo.numCuPerSh;
            if (settings.gfx7LateAllocVsOnCuAlwaysOn == true)
            {
               numCuForLateAllocVs = gpuInfo.numCuAlwaysOnPerSh;
            }

            // Compute the maximum number of HW-VS wavefronts that can launch per SH, based on GPR usage.
            const uint32 simdPerSh      = (numCuForLateAllocVs * NumSimdPerCu);
            const uint32 maxSgprVsWaves = (gpuInfo.numPhysicalSgprs / vsNumSgpr) * simdPerSh;
            const uint32 maxVgprVsWaves = (gpuInfo.numPhysicalVgprs / vsNumVgpr) * simdPerSh;

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

        m_commands.set.sh.spiShaderLateAllocVs.bits.LIMIT = Min(lateAllocLimit, maxLateAllocLimit);

        if (pUploader->EnableLoadIndexPath())
        {
            pUploader->AddShReg(mmSPI_SHADER_LATE_ALLOC_VS__CI__VI, m_commands.set.sh.spiShaderLateAllocVs);
        }
    }
}

// =====================================================================================================================
// Updates the device that this pipeline has some new ring-size requirements.
void GraphicsPipeline::UpdateRingSizes(
    const CodeObjectMetadata& metadata)
{
    const Gfx6PalSettings& settings = m_pDevice->Settings();

    ShaderRingItemSizes ringSizes = { };

    if (IsGsEnabled())
    {
        ringSizes.itemSize[static_cast<size_t>(ShaderRingType::EsGs)] = m_chunkEsGs.EsGsRingItemSize();
        ringSizes.itemSize[static_cast<size_t>(ShaderRingType::GsVs)] = m_chunkEsGs.GsVsRingItemSize();
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
// Obtains shader compilation stats.
Result GraphicsPipeline::GetShaderStats(
    ShaderType   shaderType,
    ShaderStats* pShaderStats,
    bool         getDisassemblySize
    ) const
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    PAL_ASSERT(pShaderStats != nullptr);
    Result result = Result::ErrorUnavailable;

    const ShaderStageInfo*const pStageInfo = GetShaderStageInfo(shaderType);
    if (pStageInfo != nullptr)
    {
        const ShaderStageInfo*const pStageInfoCopy =
            (shaderType == ShaderType::Geometry) ? &m_chunkVsPs.StageInfoVs() : nullptr;

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
            case Abi::HardwareStage::Ls:
                pShaderStats->common.gpuVirtAddress = m_chunkLsHs.LsProgramGpuVa();
                break;
            case Abi::HardwareStage::Hs:
                pShaderStats->common.gpuVirtAddress = m_chunkLsHs.HsProgramGpuVa();
                break;
            case Abi::HardwareStage::Es:
                pShaderStats->common.gpuVirtAddress = m_chunkEsGs.EsProgramGpuVa();
                break;
            case Abi::HardwareStage::Gs:
                pShaderStats->common.gpuVirtAddress            = m_chunkEsGs.GsProgramGpuVa();
                pShaderStats->copyShader.gpuVirtAddress        = m_chunkVsPs.VsProgramGpuVa();
                pShaderStats->copyShader.ldsSizePerThreadGroup = chipProps.gfxip.ldsSizePerThreadGroup;
                break;
            case Abi::HardwareStage::Vs:
                pShaderStats->common.gpuVirtAddress = m_chunkVsPs.VsProgramGpuVa();
                break;
            case Abi::HardwareStage::Ps:
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
        regBase = mmSPI_SHADER_USER_DATA_LS_0;
    }
    else if (IsGsEnabled())
    {
        regBase = mmSPI_SHADER_USER_DATA_ES_0;
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
    HwShaderStage             stage,
    uint16*                   pEsGsLdsSizeReg)
{
    constexpr uint16 BaseRegAddr[] =
    {
        mmSPI_SHADER_USER_DATA_LS_0,
        mmSPI_SHADER_USER_DATA_HS_0,
        mmSPI_SHADER_USER_DATA_ES_0,
        mmSPI_SHADER_USER_DATA_GS_0,
        mmSPI_SHADER_USER_DATA_VS_0,
        mmSPI_SHADER_USER_DATA_PS_0,
    };

    constexpr uint16 LastRegAddr[] =
    {
        mmSPI_SHADER_USER_DATA_LS_15,
        mmSPI_SHADER_USER_DATA_HS_15,
        mmSPI_SHADER_USER_DATA_ES_15,
        mmSPI_SHADER_USER_DATA_GS_15,
        mmSPI_SHADER_USER_DATA_VS_15,
        mmSPI_SHADER_USER_DATA_PS_15,
    };

    uint16  entryToRegAddr[MaxUserDataEntries] = { };

    const uint32 stageId = static_cast<uint32>(stage);
    auto*const   pStage  = &m_signature.stage[stageId];

    for (uint16 offset = BaseRegAddr[stageId]; offset <= LastRegAddr[stageId]; ++offset)
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
                PAL_ASSERT(offset == (BaseRegAddr[stageId] + InternalTblStartReg));
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::PerShaderTable))
            {
                PAL_ASSERT(offset == (BaseRegAddr[stageId] + ConstBufTblStartReg));
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
#if !PAL_COMPUTE_GDS_OPT
                PAL_ASSERT(offset == (BaseRegAddr[stageId] + GdsRangeReg));
#endif
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
            else if ((value == static_cast<uint32>(Abi::UserDataMapping::EsGsLdsSize)) &&
                     (pEsGsLdsSizeReg != nullptr))
            {
                (*pEsGsLdsSizeReg) = offset;
            }
            else if ((value == static_cast<uint32>(Abi::UserDataMapping::BaseIndex)) ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::Log2IndexSize)))
            {
                PAL_ALERT_ALWAYS(); // These are for Gfx9+ only!
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
    const RegisterVector&     registers,
    uint16*                   pEsGsLdsSizeRegGs,
    uint16*                   pEsGsLdsSizeRegVs)
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
        SetupSignatureForStageFromElf(metadata, registers, HwShaderStage::Ls, nullptr);
        SetupSignatureForStageFromElf(metadata, registers, HwShaderStage::Hs, nullptr);
    }
    if (IsGsEnabled())
    {
        SetupSignatureForStageFromElf(metadata, registers, HwShaderStage::Es, nullptr);
        SetupSignatureForStageFromElf(metadata, registers, HwShaderStage::Gs, pEsGsLdsSizeRegGs);
    }
    SetupSignatureForStageFromElf(metadata, registers, HwShaderStage::Vs, pEsGsLdsSizeRegVs);
    SetupSignatureForStageFromElf(metadata, registers, HwShaderStage::Ps, nullptr);

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
// Returns the SX "downconvert" format with respect to the channel format of the color buffer target.  This method is
// for the RbPlus feature.
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
// Get the sx-blend-opt-epsilon with respect to SX "downconvert" format.  This method is for the RbPlus feature.
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
// Get the SX blend opt control with respect to the specified writemask.  This method is for the RbPlus feature.
static uint32 SxBlendOptControl(
    uint32 writeMask)
{
    constexpr uint32 AlphaMask = 0x8;
    constexpr uint32 ColorMask = 0x7;

    const uint32 colorOptDisable = ((writeMask & ColorMask) != 0) ?
        0 : SX_BLEND_OPT_CONTROL__MRT0_COLOR_OPT_DISABLE_MASK__VI;

    const uint32 alphaOptDisable = ((writeMask & AlphaMask) != 0) ?
        0 : SX_BLEND_OPT_CONTROL__MRT0_ALPHA_OPT_DISABLE_MASK__VI;

    return (colorOptDisable | alphaOptDisable);
}

} // Gfx6
} // Pal
