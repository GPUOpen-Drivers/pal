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

#include "core/device.h"
#include "core/platform.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9ColorBlendState.h"
#include "core/hw/gfxip/gfx9/gfx9ComputePipeline.h"
#include "core/hw/gfxip/gfx9/gfx9DepthStencilState.h"
#include "core/hw/gfxip/gfx9/gfx9DepthStencilView.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9UniversalCmdBuffer.h"
#include "palFormatInfo.h"
#include "palInlineFuncs.h"
#include "palMetroHash.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// User-data signature for an unbound graphics pipeline.
const GraphicsPipelineSignature NullGfxSignature =
{
    { 0, },                     // User-data mapping for each shader stage
    UserDataNotMapped,          // Vertex buffer table register address
    UserDataNotMapped,          // Stream-out table register address
    UserDataNotMapped,          // UAV export table mapping
    UserDataNotMapped,          // NGG culling data constant buffer
    UserDataNotMapped,          // Vertex offset register address
    UserDataNotMapped,          // Draw ID register address
    NoUserDataSpilling,         // Spill threshold
    0,                          // User-data entry limit
    { UserDataNotMapped, },     // Compacted view ID register addresses
    { 0, },                     // User-data mapping hashes per-stage
};
static_assert(UserDataNotMapped == 0, "Unexpected value for indicating unmapped user-data entries!");

static uint8 Rop3(LogicOp logicOp);
static uint32 SxBlendOptEpsilon(SX_DOWNCONVERT_FORMAT sxDownConvertFormat);
static uint32 SxBlendOptControl(uint32 writeMask);

// Base count of SH registers which are loaded using LOAD_SH_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedShRegCount =
    0;  // mmSPI_SHADER_LATE_ALLOC_VS is only used for non NGG so add it later if we don't use NGG
// Base count of Context registers which are loaded using LOAD_CNTX_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedCntxRegCount =
    1 + // mmVGT_SHADER_STAGES_EN
    0 + // mmVGT_DRAW_PAYLOAD_CNTL is not included because it is not needed on all HW
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
    0 + // mmCB_COVERAGE_OUT_CONTROL is not included because it is not present on all HW
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
    m_pDevice(pDevice),
    m_gfxLevel(pDevice->Parent()->ChipProperties().gfxLevel),
    m_contextRegHash(0),
    m_configRegHash(0),
    m_isNggFastLaunch(false),
    m_nggSubgroupSize(0),
    m_uavExportRequiresFlush(false),
    m_fetchShaderRegAddr(UserDataNotMapped),
    m_chunkHs(*pDevice,
              &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Hs)]),
    m_chunkGs(*pDevice,
              &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Gs)]),
    m_chunkVsPs(*pDevice,
                &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Vs)],
                &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Ps)])
{
    memset(&m_regs, 0, sizeof(m_regs));
    memset(&m_loadPath, 0, sizeof(m_loadPath));
    memset(&m_prefetch, 0, sizeof(m_prefetch));
    memcpy(&m_signature, &NullGfxSignature, sizeof(m_signature));
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
    m_regs.context.vgtShaderStagesEn.u32All = registers.At(mmVGT_SHADER_STAGES_EN);

    m_isNggFastLaunch = (IsGfx091xPlus(*(m_pDevice->Parent())) ?
                         (m_regs.context.vgtShaderStagesEn.gfx09_1xPlus.GS_FAST_LAUNCH != 0) :
                         (m_regs.context.vgtShaderStagesEn.gfx09_0.GS_FAST_LAUNCH != 0));
    m_nggSubgroupSize = (metadata.pipeline.hasEntry.nggSubgroupSize) ? metadata.pipeline.nggSubgroupSize : 0;

    // Similarly, VGT_GS_MODE should also be read early, since it determines if on-chip GS is enabled.
    registers.HasEntry(mmVGT_GS_MODE, &m_regs.context.vgtGsMode.u32All);
    if (IsGsEnabled() && (m_regs.context.vgtGsMode.bits.ONCHIP == VgtGsModeOnchip))
    {
        SetIsGsOnChip(true);
    }

    // Must be called *after* determining active HW stages!
    SetupSignatureFromElf(metadata, registers, &pInfo->esGsLdsSizeRegGs, &pInfo->esGsLdsSizeRegVs);

    const Gfx9PalSettings& settings = m_pDevice->Settings();
    if (settings.enableLoadIndexForObjectBinds != false)
    {
        // Add mmSPI_SHADER_LATE_ALLOC_VS if we don't use NGG
        pInfo->loadedShRegCount  = BaseLoadedShRegCount + (IsNgg() == false);

        pInfo->loadedCtxRegCount =
            // This mimics the definition in BaseLoadedCntxRegCount
            IsGfx10Plus(m_gfxLevel)                                + // mmVGT_DRAW_PAYLOAD_CNTL
            IsGfx10Plus(m_gfxLevel)                                + // mmCB_COVERAGE_OUT_CONTROL
            (regInfo.mmPaStereoCntl != 0)                          + // mmPA_STEREO_CNTL
            (IsGsEnabled() || IsNgg() || IsTessEnabled())          + // mmVGT_GS_ONCHIP_CNTL
            BaseLoadedCntxRegCount;
    }

    pInfo->enableNgg    = IsNgg();
    pInfo->usesOnChipGs = IsGsOnChip();

    if (IsTessEnabled())
    {
        m_chunkHs.EarlyInit(pInfo);
    }
    if (IsGsEnabled() || pInfo->enableNgg)
    {
        m_chunkGs.EarlyInit(pInfo);
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
    const AbiReader&                  abiReader,
    const CodeObjectMetadata&         metadata,
    MsgPackReader*                    pMetadataReader)
{
    RegisterVector registers(m_pDevice->GetPlatform());
    Result result = pMetadataReader->Seek(metadata.pipeline.registers);

    if (result == Result::Success)
    {
        result = pMetadataReader->Unpack(&registers);
    }

    if (result == Result::Success)
    {
        GraphicsPipelineLoadInfo loadInfo = { };
        EarlyInit(metadata, registers, &loadInfo);

        // Next, handle relocations and upload the pipeline code & data to GPU memory.
        GraphicsPipelineUploader uploader(m_pDevice,
                                          abiReader,
                                          loadInfo.loadedCtxRegCount,
                                          loadInfo.loadedShRegCount);
        result = PerformRelocationsAndUploadToGpuMemory(
            metadata,
            (createInfo.flags.overrideGpuHeap == 1) ? createInfo.preferredHeapType : GpuHeapInvisible,
            &uploader);

        if (result == Result::Success)
        {
            LateInit(createInfo, abiReader, metadata, registers, loadInfo, &uploader);
            PAL_ASSERT(m_uploadFenceToken == 0);
            result = uploader.End(&m_uploadFenceToken);
        }
    }

    if (result == Result::Success)
    {
        ResourceDescriptionPipeline desc = {};
        desc.pPipelineInfo = &GetInfo();
        desc.pCreateFlags = &createInfo.flags;
        ResourceCreateEventData data = {};
        data.type = ResourceType::Pipeline;
        data.pResourceDescData = &desc;
        data.resourceDescSize = sizeof(ResourceDescriptionPipeline);
        data.pObj = this;
        m_pDevice->GetPlatform()->GetEventProvider()->LogGpuMemoryResourceCreateEvent(data);

        GpuMemoryResourceBindEventData bindData = {};
        bindData.pObj = this;
        bindData.pGpuMemory = m_gpuMem.Memory();
        bindData.requiredGpuMemSize = m_gpuMemSize;
        bindData.offset = m_gpuMem.Offset();
        m_pDevice->GetPlatform()->GetEventProvider()->LogGpuMemoryResourceBindEvent(bindData);
    }

    return result;
}

// =====================================================================================================================
void GraphicsPipeline::LateInit(
    const GraphicsPipelineCreateInfo& createInfo,
    const AbiReader&                  abiReader,
    const CodeObjectMetadata&         metadata,
    const RegisterVector&             registers,
    const GraphicsPipelineLoadInfo&   loadInfo,
    GraphicsPipelineUploader*         pUploader)
{
    MetroHash64 hasher;

    if (IsTessEnabled())
    {
        m_chunkHs.LateInit(abiReader, registers, pUploader, &hasher);
    }
    if (IsGsEnabled() || IsNgg())
    {
        m_chunkGs.LateInit(abiReader, metadata, registers, loadInfo, pUploader, &hasher);
    }
    m_chunkVsPs.LateInit(abiReader, metadata, registers, loadInfo, createInfo, pUploader, &hasher);

    SetupCommonRegisters(createInfo, registers, pUploader);
    SetupNonShaderRegisters(createInfo, registers, pUploader);
    SetupStereoRegisters();
    SetupFetchShaderInfo(pUploader);

    if (pUploader->EnableLoadIndexPath())
    {
        m_loadPath.gpuVirtAddrCtx = pUploader->CtxRegGpuVirtAddr();
        m_loadPath.countCtx       = pUploader->CtxRegisterCount();
        m_loadPath.gpuVirtAddrSh  = pUploader->ShRegGpuVirtAddr();
        m_loadPath.countSh        = pUploader->ShRegisterCount();
    }

    MetroHash::Hash hash = {};

    hasher.Update(m_regs.context);
    hasher.Finalize(hash.bytes);
    m_contextRegHash = MetroHash::Compact32(&hash);

    // We write our config registers in a separate function so they get their own hash.
    // Also, we only set config registers on gfx10+.
    if (IsGfx10Plus(m_gfxLevel))
    {
        hasher.Initialize();
        hasher.Update(m_regs.uconfig);
        hasher.Finalize(hash.bytes);
        m_configRegHash = MetroHash::Compact32(&hash);
    }

    m_pDevice->CmdUtil().BuildPipelinePrefetchPm4(*pUploader, &m_prefetch);

    // Updating the ring sizes expects that all of the register state has been setup.
    UpdateRingSizes(metadata);
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
    float maxWavesPerCu1,
    float maxWavesPerCu2
    ) const
{
    // The HW shader stage might a combination of two API shader stages (e.g., for GS copy shaders), so we must apply
    // the minimum wave limit of both API shader stages.  Note that zero is the largest value because it means
    // unlimited.
    const float maxWavesPerCu =
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
        const uint32 maxWavesPerSh = static_cast<uint32>(round(maxWavesPerCu * gfx9ChipProps.numCuPerSh));

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

    // Disable the LOAD_INDEX path if the PM4 optimizer is enabled.  The optimizer cannot optimize these load packets
    // because the register values are in GPU memory.  Additionally, any client requesting PM4 optimization is trading
    // CPU cycles for GPU performance, so the savings of using LOAD_INDEX is not important.
    if ((m_loadPath.countSh == 0) || pCmdStream->Pm4OptimizerEnabled())
    {
        // If NGG is enabled, there is no hardware-VS, so there is no need to write the late-alloc VS limit.
        if (IsNgg() == false)
        {
            pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(Gfx09_10::mmSPI_SHADER_LATE_ALLOC_VS,
                                                                     m_regs.sh.spiShaderLateAllocVs.u32All,
                                                                     pCmdSpace);
        }

        if (IsTessEnabled())
        {
            pCmdSpace = m_chunkHs.WriteShCommands<false>(pCmdStream, pCmdSpace, stageInfos.hs);
        }
        if (IsGsEnabled() || IsNgg())
        {
            pCmdSpace = m_chunkGs.WriteShCommands<false>(pCmdStream, pCmdSpace, stageInfos.gs);
        }
        pCmdSpace = m_chunkVsPs.WriteShCommands<false>(pCmdStream, pCmdSpace, IsNgg(), stageInfos.vs, stageInfos.ps);
    }
    else
    {
        // This will load SH register state for this object and all pipeline chunks!
        pCmdSpace += m_pDevice->CmdUtil().BuildLoadShRegsIndex(m_loadPath.gpuVirtAddrSh,
                                                               m_loadPath.countSh,
                                                               ShaderGraphics,
                                                               pCmdSpace);

        // The below calls will end up only writing SET packets for "dynamic" state.
        if (IsTessEnabled())
        {
            pCmdSpace = m_chunkHs.WriteShCommands<true>(pCmdStream, pCmdSpace, stageInfos.hs);
        }
        if (IsGsEnabled() || IsNgg())
        {
            pCmdSpace = m_chunkGs.WriteShCommands<true>(pCmdStream, pCmdSpace, stageInfos.gs);
        }
        pCmdSpace = m_chunkVsPs.WriteShCommands<true>(pCmdStream, pCmdSpace, IsNgg(), stageInfos.vs, stageInfos.ps);
    }

    pCmdSpace = WriteFsShCommands(pCmdStream, pCmdSpace, m_fetchShaderRegAddr);

    return pCmdSpace;
}

// =====================================================================================================================
// Helper function for writing common context registers which are shared by all graphics pipelines.
// Returns a command buffer pointer incremented to the end of the commands we just wrote.
uint32* GraphicsPipeline::WriteContextCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    PAL_ASSERT(pCmdStream != nullptr);

    // Disable the LOAD_INDEX path if the PM4 optimizer is enabled.  The optimizer cannot optimize these load packets
    // because the register values are in GPU memory.  Additionally, any client requesting PM4 optimization is trading
    // CPU cycles for GPU performance, so the savings of using LOAD_INDEX is not important.
    if ((m_loadPath.countCtx == 0) || pCmdStream->Pm4OptimizerEnabled())
    {
        pCmdSpace = WriteContextCommandsSetPath(pCmdStream, pCmdSpace);

        if (IsTessEnabled())
        {
            pCmdSpace = m_chunkHs.WriteContextCommands<false>(pCmdStream, pCmdSpace);
        }
        if (IsGsEnabled() || IsNgg())
        {
            pCmdSpace = m_chunkGs.WriteContextCommands<false>(pCmdStream, pCmdSpace);
        }

        pCmdSpace = m_chunkVsPs.WriteContextCommands<false>(pCmdStream, pCmdSpace);
    }
    else
    {
        // This will load context register state for this object and all pipeline chunks!
        pCmdSpace += CmdUtil::BuildLoadContextRegsIndex(m_loadPath.gpuVirtAddrCtx, m_loadPath.countCtx, pCmdSpace);

        // NOTE: The Hs and Gs chunks don't expect us to call WriteContextCommands() when using the LOAD_INDEX path.
        pCmdSpace = m_chunkVsPs.WriteContextCommands<true>(pCmdStream, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Helper function for writing common GFX10 config registers which are shared by all graphics pipelines.
// Returns a command buffer pointer incremented to the end of the commands we just wrote.
uint32* GraphicsPipeline::WriteConfigCommandsGfx10(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // The caller is required to check if this is gfx10 before calling. We'd probably just do it in here if we
    // weren't worried about increasing our draw-time validation CPU overhead.
    PAL_ASSERT(IsGfx10Plus(m_gfxLevel));

    pCmdSpace = pCmdStream->WriteSetSeqConfigRegs(Gfx10Plus::mmGE_STEREO_CNTL,
                                                  Gfx10Plus::mmGE_PC_ALLOC,
                                                  &m_regs.uconfig.geStereoCntl,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneConfigReg(Gfx10Plus::mmGE_USER_VGPR_EN,
                                                 m_regs.uconfig.geUserVgprEn.u32All,
                                                 pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Requests that this pipeline indicates what it would like to prefetch.
uint32* GraphicsPipeline::Prefetch(
    uint32* pCmdSpace
    ) const
{
    memcpy(pCmdSpace, &m_prefetch, m_prefetch.spaceNeeded * sizeof(uint32));
    return (pCmdSpace + m_prefetch.spaceNeeded);
}

// =====================================================================================================================
// Writes PM4 SET commands to the specified command stream.  This is only expected to be called when the LOAD path is
// not in use and we need to use the SET path fallback.
uint32* GraphicsPipeline::WriteContextCommandsSetPath(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const CmdUtil&      cmdUtil = m_pDevice->CmdUtil();
    const RegisterInfo& regInfo = cmdUtil.GetRegInfo();

    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmVGT_SHADER_STAGES_EN,
                                                  m_regs.context.vgtShaderStagesEn.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmVGT_GS_MODE, m_regs.context.vgtGsMode.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmVGT_REUSE_OFF, m_regs.context.vgtReuseOff.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmVGT_TF_PARAM, m_regs.context.vgtTfParam.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmVGT_DRAW_PAYLOAD_CNTL,
                                                  m_regs.context.vgtDrawPayloadCntl.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmCB_COLOR_CONTROL, m_regs.context.cbColorControl.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmCB_TARGET_MASK, mmCB_SHADER_MASK,
                                                   &m_regs.context.cbTargetMask,
                                                   pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmPA_CL_CLIP_CNTL, m_regs.context.paClClipCntl.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmPA_SU_VTX_CNTL, m_regs.context.paSuVtxCntl.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmPA_CL_VTE_CNTL, m_regs.context.paClVteCntl.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmPA_SC_LINE_CNTL, m_regs.context.paScLineCntl.u32All, pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmSPI_INTERP_CONTROL_0,
                                                  m_regs.context.spiInterpControl0.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmVGT_VERTEX_REUSE_BLOCK_CNTL,
                                                  m_regs.context.vgtVertexReuseBlockCntl.u32All,
                                                  pCmdSpace);

    if (regInfo.mmPaStereoCntl != 0)
    {
        pCmdSpace = pCmdStream->WriteSetOneContextReg(regInfo.mmPaStereoCntl,
                                                      m_regs.context.paStereoCntl.u32All,
                                                      pCmdSpace);
    }

    if (IsGfx10Plus(m_gfxLevel))
    {
        pCmdSpace = pCmdStream->WriteSetOneContextReg(Gfx10Plus::mmCB_COVERAGE_OUT_CONTROL,
                                                      m_regs.context.cbCoverageOutCntl.u32All,
                                                      pCmdSpace);
    }

    if (IsGsEnabled() || IsNgg() || IsTessEnabled())
    {
        pCmdSpace = pCmdStream->WriteSetOneContextReg(mmVGT_GS_ONCHIP_CNTL,
                                                      m_regs.context.vgtGsOnchipCntl.u32All,
                                                      pCmdSpace);
    }

    return pCmdSpace;
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
    const auto&              palDevice = *(m_pDevice->Parent());
    const GpuChipProperties& chipProps = palDevice.ChipProperties();
    const RegisterInfo&      regInfo   = m_pDevice->CmdUtil().GetRegInfo();
    const Gfx9PalSettings&   settings  = m_pDevice->Settings();
    const PalPublicSettings* pPalSettings = m_pDevice->Parent()->GetPublicSettings();

    m_regs.context.paClClipCntl.u32All = registers.At(mmPA_CL_CLIP_CNTL);
    m_regs.context.paClVteCntl.u32All  = registers.At(mmPA_CL_VTE_CNTL);
    m_regs.context.paSuVtxCntl.u32All  = registers.At(mmPA_SU_VTX_CNTL);
    m_regs.other.paScModeCntl1.u32All  = registers.At(mmPA_SC_MODE_CNTL_1);

    registers.HasEntry(mmVGT_GS_ONCHIP_CNTL, &m_regs.context.vgtGsOnchipCntl.u32All);

    // Overrides some of the fields in PA_SC_MODE_CNTL1 to account for GPU pipe config and features like out-of-order
    // rasterization.

    // The maximum value for OUT_OF_ORDER_WATER_MARK is 7
    constexpr uint32 MaxOutOfOrderWatermark = 7;
    m_regs.other.paScModeCntl1.bits.OUT_OF_ORDER_WATER_MARK = Min(MaxOutOfOrderWatermark,
                                                                  settings.outOfOrderWatermark);

    if (createInfo.rsState.outOfOrderPrimsEnable &&
        (settings.enableOutOfOrderPrimitives != OutOfOrderPrimDisable))
    {
        m_regs.other.paScModeCntl1.bits.OUT_OF_ORDER_PRIMITIVE_ENABLE = 1;
    }

    // Hardware team recommendation is to set WALK_FENCE_SIZE to 512 pixels for 4/8/16 pipes and 256 pixels
    // for 2 pipes.
    m_regs.other.paScModeCntl1.bits.WALK_FENCE_SIZE = ((m_pDevice->GetNumPipesLog2() <= 1) ? 2 : 3);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 598
    switch (createInfo.rsState.forcedShadingRate)
    {
    case PsShadingRate::SampleRate:
        m_regs.other.paScModeCntl1.bits.PS_ITER_SAMPLE = 1;
        break;
    case PsShadingRate::PixelRate:
        m_regs.other.paScModeCntl1.bits.PS_ITER_SAMPLE = 0;
        break;
    default:
        break;
    }
#else
    m_regs.other.paScModeCntl1.bits.PS_ITER_SAMPLE |= createInfo.rsState.forceSampleRateShading;
#endif

    m_info.ps.flags.perSampleShading = m_regs.other.paScModeCntl1.bits.PS_ITER_SAMPLE;

    // NOTE: On recommendation from h/ware team FORCE_SHADER_Z_ORDER will be set whenever Re-Z is being used.
    m_regs.other.dbRenderOverride.bits.FORCE_SHADER_Z_ORDER = (m_chunkVsPs.DbShaderControl().bits.Z_ORDER == RE_Z);

    // Configure depth clamping
    // Register specification does not specify dependence of DISABLE_VIEWPORT_CLAMP on Z_EXPORT_ENABLE, but
    // removing the dependence leads to perf regressions in some applications for Vulkan, DX and OGL.
    // The reason for perf drop can be narrowed down to the DepthExpand RPM pipeline. Disabling viewport clamping
    // (DISABLE_VIEWPORT_CLAMP = 1) for this pipeline results in heavy perf drops.
    // It's also important to note that this issue is caused by the graphics depth fast clear not the depth expand
    // itself.  It simply reuses the same RPM pipeline from the depth expand.

    if (pPalSettings->depthClampBasedOnZExport == true)
    {
        m_regs.other.dbRenderOverride.bits.DISABLE_VIEWPORT_CLAMP =
            ((createInfo.rsState.depthClampDisable != false) &&
             (m_chunkVsPs.DbShaderControl().bits.Z_EXPORT_ENABLE != 0));
    }
    else
    {
        // Vulkan (only) will take this path by default, unless an app-detect forces the other way.
        m_regs.other.dbRenderOverride.bits.DISABLE_VIEWPORT_CLAMP = (createInfo.rsState.depthClampDisable != false);
    }

    if (regInfo.mmPaStereoCntl != 0)
    {
        registers.HasEntry(regInfo.mmPaStereoCntl, &m_regs.context.paStereoCntl.u32All);
    }

    if (IsGfx10Plus(m_gfxLevel))
    {
        registers.HasEntry(Gfx10Plus::mmGE_STEREO_CNTL,  &m_regs.uconfig.geStereoCntl.u32All);
        registers.HasEntry(Gfx10Plus::mmGE_USER_VGPR_EN, &m_regs.uconfig.geUserVgprEn.u32All);

        if ((IsNgg() == false) || (m_regs.context.vgtShaderStagesEn.gfx10Plus.PRIMGEN_PASSTHRU_EN == 1))
        {
            if (settings.gfx10GePcAllocNumLinesPerSeLegacyNggPassthru > 0)
            {
                m_regs.uconfig.gePcAlloc.bits.OVERSUB_EN   = 1;
                m_regs.uconfig.gePcAlloc.bits.NUM_PC_LINES =
                    ((settings.gfx10GePcAllocNumLinesPerSeLegacyNggPassthru * chipProps.gfx9.numShaderEngines) - 1);
            }
        }
        else
        {
            PAL_ASSERT(m_regs.context.vgtShaderStagesEn.bits.PRIMGEN_EN == 1);
            if (settings.gfx10GePcAllocNumLinesPerSeNggCulling > 0)
            {
                m_regs.uconfig.gePcAlloc.bits.OVERSUB_EN   = 1;
                m_regs.uconfig.gePcAlloc.bits.NUM_PC_LINES =
                    ((settings.gfx10GePcAllocNumLinesPerSeNggCulling * chipProps.gfx9.numShaderEngines) - 1);
            }
        }
    }

    m_regs.context.vgtReuseOff.u32All  = registers.At(mmVGT_REUSE_OFF);
    m_regs.other.spiPsInControl.u32All = registers.At(mmSPI_PS_IN_CONTROL);
    m_regs.other.spiVsOutConfig.u32All = registers.At(mmSPI_VS_OUT_CONFIG);

    // NOTE: The following registers are assumed to have the value zero if the pipeline ELF does not specify values.
    registers.HasEntry(mmVGT_TF_PARAM,     &m_regs.context.vgtTfParam.u32All);
    registers.HasEntry(mmVGT_LS_HS_CONFIG, &m_regs.other.vgtLsHsConfig.u32All);

    // If the number of VS output semantics exceeds the half-pack threshold, then enable VS half-pack mode.  Keep in
    // mind that the number of VS exports are represented by a -1 field in the HW register!
    if ((m_regs.other.spiVsOutConfig.bits.VS_EXPORT_COUNT + 1u) > settings.vsHalfPackThreshold)
    {
        m_regs.other.spiVsOutConfig.gfx09_10.VS_HALF_PACK = 1;
    }

    // For Gfx9+, default VTX_REUSE_DEPTH to 14
    m_regs.context.vgtVertexReuseBlockCntl.bits.VTX_REUSE_DEPTH = 14;

    if ((settings.vsHalfPackThreshold >= MaxVsExportSemantics) &&
        (m_gfxLevel == GfxIpLevel::GfxIp9))
    {
        // Degenerate primitive filtering with fractional odd tessellation requires a VTX_REUSE_DEPTH of 14. Only
        // override to 30 if we aren't using that feature.
        //
        // VGT_TF_PARAM depends solely on the compiled HS when on-chip GS is disabled, in the future when Tess with
        // on-chip GS is supported, the 2nd condition may need to be revisited.
        if ((m_pDevice->DegeneratePrimFilter() == false) ||
            (IsTessEnabled() && (m_regs.context.vgtTfParam.bits.PARTITIONING != PART_FRAC_ODD)))
        {
            m_regs.context.vgtVertexReuseBlockCntl.bits.VTX_REUSE_DEPTH = 30;
        }
    }

    registers.HasEntry(mmSPI_INTERP_CONTROL_0, &m_regs.context.spiInterpControl0.u32All);

    m_regs.context.spiInterpControl0.bits.FLAT_SHADE_ENA = (createInfo.rsState.shadeMode == ShadeMode::Flat);
    if (m_regs.context.spiInterpControl0.bits.PNT_SPRITE_ENA != 0) // Point sprite mode is enabled.
    {
        m_regs.context.spiInterpControl0.bits.PNT_SPRITE_TOP_1  =
            (createInfo.rsState.pointCoordOrigin != PointOrigin::UpperLeft);
    }

    registers.HasEntry(mmVGT_DRAW_PAYLOAD_CNTL, &m_regs.context.vgtDrawPayloadCntl.u32All);

    if (pUploader->EnableLoadIndexPath())
    {
        pUploader->AddCtxReg(mmVGT_SHADER_STAGES_EN,        m_regs.context.vgtShaderStagesEn);
        pUploader->AddCtxReg(mmVGT_GS_MODE,                 m_regs.context.vgtGsMode);
        pUploader->AddCtxReg(mmVGT_REUSE_OFF,               m_regs.context.vgtReuseOff);
        pUploader->AddCtxReg(mmVGT_TF_PARAM,                m_regs.context.vgtTfParam);
        pUploader->AddCtxReg(mmPA_CL_CLIP_CNTL,             m_regs.context.paClClipCntl);
        pUploader->AddCtxReg(mmPA_SU_VTX_CNTL,              m_regs.context.paSuVtxCntl);
        pUploader->AddCtxReg(mmPA_CL_VTE_CNTL,              m_regs.context.paClVteCntl);
        pUploader->AddCtxReg(mmSPI_INTERP_CONTROL_0,        m_regs.context.spiInterpControl0);
        pUploader->AddCtxReg(mmVGT_VERTEX_REUSE_BLOCK_CNTL, m_regs.context.vgtVertexReuseBlockCntl);

        if (regInfo.mmPaStereoCntl != 0)
        {
            pUploader->AddCtxReg(regInfo.mmPaStereoCntl, m_regs.context.paStereoCntl);
        }

        if (IsGfx10Plus(m_gfxLevel))
        {
            pUploader->AddCtxReg(mmVGT_DRAW_PAYLOAD_CNTL, m_regs.context.vgtDrawPayloadCntl);
        }
    }

    // If NGG is enabled, there is no hardware-VS, so there is no need to compute the late-alloc VS limit.
    if (IsNgg() == false)
    {
        // Target late-alloc limit uses PAL settings by default. The lateAllocVsLimit member from graphicsPipeline
        // can override this setting if corresponding flag is set. Did the pipeline request to use the pipeline
        // specified late-alloc limit 4 * (gfx9Props.numCuPerSh - 1).
        const uint32 targetLateAllocLimit = IsLateAllocVsLimit()
                                            ? GetLateAllocVsLimit()
                                            : m_pDevice->LateAllocVsLimit() + 1;

        regSPI_SHADER_PGM_RSRC1_VS spiShaderPgmRsrc1Vs = { };
        spiShaderPgmRsrc1Vs.u32All = registers.At(Gfx09_10::mmSPI_SHADER_PGM_RSRC1_VS);

        regSPI_SHADER_PGM_RSRC2_VS spiShaderPgmRsrc2Vs = { };
        spiShaderPgmRsrc2Vs.u32All = registers.At(Gfx09_10::mmSPI_SHADER_PGM_RSRC2_VS);
        const uint32 programmedLimit = CalcMaxLateAllocLimit(*m_pDevice,
                                                             registers,
                                                             spiShaderPgmRsrc1Vs.bits.VGPRS,
                                                             spiShaderPgmRsrc1Vs.bits.SGPRS,
                                                             spiShaderPgmRsrc2Vs.bits.SCRATCH_EN,
                                                             targetLateAllocLimit);

        if (m_gfxLevel == GfxIpLevel::GfxIp9)
        {
            m_regs.sh.spiShaderLateAllocVs.bits.LIMIT = programmedLimit;
        }
        else if (IsGfx10Plus(m_gfxLevel))
        {
            // Always use the (forced) experimental setting if specified, or use a fixed limit if
            // enabled, otherwise check the VS/PS resource usage to compute the limit.
            if (settings.gfx10SpiShaderLateAllocVsNumLines < 64)
            {
                m_regs.sh.spiShaderLateAllocVs.bits.LIMIT = settings.gfx10SpiShaderLateAllocVsNumLines;
            }
            else
            {
                m_regs.sh.spiShaderLateAllocVs.bits.LIMIT = programmedLimit;
            }
        }
        if (pUploader->EnableLoadIndexPath())
        {
            pUploader->AddShReg(Gfx09_10::mmSPI_SHADER_LATE_ALLOC_VS, m_regs.sh.spiShaderLateAllocVs);
        }
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
        iaMultiVgtParam.bits.PRIMGROUP_SIZE =
            m_pDevice->ComputeTessPrimGroupSize(m_regs.other.vgtLsHsConfig.bits.NUM_PATCHES);
    }
    else if (IsGsEnabled() && (m_regs.other.vgtLsHsConfig.bits.HS_NUM_INPUT_CP != 0))
    {
        // The hardware requires that the primgroup size must not exceed (256/ number of HS input control points) when
        // a GS shader accepts patch primitives as input.
        iaMultiVgtParam.bits.PRIMGROUP_SIZE =
                m_pDevice->ComputeNoTessPatchPrimGroupSize(m_regs.other.vgtLsHsConfig.bits.HS_NUM_INPUT_CP);
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
        m_regs.other.iaMultiVgtParam[idx] = iaMultiVgtParam;

        // Additional setup for this register is required based on whether or not WD_SWITCH_ON_EOP is forced to 1.
        FixupIaMultiVgtParam((idx != 0), &m_regs.other.iaMultiVgtParam[idx]);

        // NOTE: The PRIMGROUP_SIZE field IA_MULTI_VGT_PARAM must be less than 256 if stream output and
        // PARTIAL_ES_WAVE_ON are both enabled on 2-SE hardware.
        if ((VgtStrmoutConfig().u32All != 0) && (chipProps.gfx9.numShaderEngines == 2))
        {
            if (m_regs.other.iaMultiVgtParam[idx].bits.PARTIAL_ES_WAVE_ON == 0)
            {
                PAL_ASSERT(m_regs.other.iaMultiVgtParam[idx].bits.PRIMGROUP_SIZE < 256);
            }

            if ((m_regs.other.iaMultiVgtParam[idx].gfx09.EN_INST_OPT_BASIC == 1) ||
                (m_regs.other.iaMultiVgtParam[idx].gfx09.EN_INST_OPT_ADV   == 1))
            {
                // The maximum supported setting for IA_MULTI_VGT_PARAM.PRIMGROUP_SIZE with the instancing optimization
                // flowchart enabled is 253.
                PAL_ASSERT(m_regs.other.iaMultiVgtParam[idx].bits.PRIMGROUP_SIZE < 253);
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

    if (m_regs.context.vgtTfParam.bits.DISTRIBUTION_MODE != NO_DIST)
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
    if (IsNggFastLaunch() == false)
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

    m_regs.context.paScLineCntl.bits.EXPAND_LINE_WIDTH        = createInfo.rsState.expandLineWidth;
    m_regs.context.paScLineCntl.bits.DX10_DIAMOND_TEST_ENA    = 1;
    m_regs.context.paScLineCntl.bits.LAST_PIXEL               = createInfo.rsState.rasterizeLastLinePixel;
    m_regs.context.paScLineCntl.bits.PERPENDICULAR_ENDCAP_ENA = createInfo.rsState.perpLineEndCapsEnable;

    m_regs.context.cbShaderMask.u32All = registers.At(mmCB_SHADER_MASK);

    // CB_TARGET_MASK comes from the RT write masks in the pipeline CB state structure.
    for (uint32 rt = 0; rt < MaxColorTargets; ++rt)
    {
        const auto&  cbTarget = createInfo.cbState.target[rt];
        const uint32 rtShift  = (rt * 4); // Each RT uses four bits of CB_TARGET_MASK.

        m_regs.context.cbTargetMask.u32All |= ((cbTarget.channelWriteMask & 0xF) << rtShift);

    }

    //      The bug manifests itself when an MRT is not enabled in the shader mask but is enabled in the target
    //      mask. It will work fine if the target mask is always a subset of the shader mask
    if (settings.waOverwriteCombinerTargetMaskOnly &&
        (TestAllFlagsSet(m_regs.context.cbShaderMask.u32All, m_regs.context.cbTargetMask.u32All) == false))
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
        m_regs.context.cbColorControl.bits.MODE = CB_ELIMINATE_FAST_CLEAR;
        m_regs.context.cbColorControl.bits.ROP3 = Rop3(LogicOp::Copy);

        // NOTE: the CB spec states that for fast-clear eliminate, these registers should be set to enable writes to all
        // four channels of RT #0.
        m_regs.context.cbShaderMask.u32All = 0xF;
        m_regs.context.cbTargetMask.u32All = 0xF;
    }
    else if (IsFmaskDecompress())
    {
        m_regs.context.cbColorControl.bits.MODE = CB_FMASK_DECOMPRESS;
        m_regs.context.cbColorControl.bits.ROP3 = Rop3(LogicOp::Copy);

        // NOTE: the CB spec states that for fmask-decompress, these registers should be set to enable writes to all
        // four channels of RT #0.
        m_regs.context.cbShaderMask.u32All = 0xF;
        m_regs.context.cbTargetMask.u32All = 0xF;
    }
    else if (IsDccDecompress())
    {
        m_regs.context.cbColorControl.bits.MODE = CB_DCC_DECOMPRESS;
        m_regs.context.cbColorControl.bits.ROP3 = Rop3(LogicOp::Copy);

        // According to the reg-spec, DCC decompress ops imply fmask decompress and fast-clear eliminate operations as
        // well, so set these registers as they would be set above.
        m_regs.context.cbShaderMask.u32All = 0xF;
        m_regs.context.cbTargetMask.u32All = 0xF;
    }
    else if (IsResolveFixedFunc())
    {
        m_regs.context.cbColorControl.bits.MODE = CB_RESOLVE;
        m_regs.context.cbColorControl.bits.ROP3 = Rop3(LogicOp::Copy);

        m_regs.context.cbShaderMask.u32All = 0xF;
        m_regs.context.cbTargetMask.u32All = 0xF;
    }
    else if ((m_regs.context.cbShaderMask.u32All == 0) || (m_regs.context.cbTargetMask.u32All == 0))
    {
        m_regs.context.cbColorControl.bits.MODE = CB_DISABLE;
    }
    else
    {
        m_regs.context.cbColorControl.bits.MODE = CB_NORMAL;
        m_regs.context.cbColorControl.bits.ROP3 = Rop3(createInfo.cbState.logicOp);
    }

    if (createInfo.cbState.dualSourceBlendEnable)
    {
        // If dual-source blending is enabled and the PS doesn't export to both RT0 and RT1, the hardware might hang.
        // To avoid the hang, just disable CB writes.
        if (((m_regs.context.cbShaderMask.u32All & 0x0F) == 0) ||
            ((m_regs.context.cbShaderMask.u32All & 0xF0) == 0))
        {
            PAL_ALERT_ALWAYS();
            m_regs.context.cbColorControl.bits.MODE = CB_DISABLE;
        }
    }

    // Initialize RB+ registers for pipelines which are able to use the feature.
    if (settings.gfx9RbPlusEnable &&
        (createInfo.cbState.dualSourceBlendEnable == false) &&
        (m_regs.context.cbColorControl.bits.MODE != CB_RESOLVE))
    {
        PAL_ASSERT(chipProps.gfx9.rbPlus);

        m_regs.context.cbColorControl.bits.DISABLE_DUAL_QUAD = 0;

        for (uint32 slot = 0; slot < MaxColorTargets; ++slot)
        {
            SetupRbPlusRegistersForSlot(slot,
                                        createInfo.cbState.target[slot].channelWriteMask,
                                        createInfo.cbState.target[slot].swizzledFormat,
                                        &m_regs.other.sxPsDownconvert,
                                        &m_regs.other.sxBlendOptEpsilon,
                                        &m_regs.other.sxBlendOptControl);
        }
    }
    else if (chipProps.gfx9.rbPlus != 0)
    {
        // If RB+ is supported but not enabled, we need to set DISABLE_DUAL_QUAD.
        m_regs.context.cbColorControl.bits.DISABLE_DUAL_QUAD = 1;
    }

    if (chipProps.gfx9.supportMsaaCoverageOut && createInfo.coverageOutDesc.flags.enable)
    {
        const auto& coverageInfo = createInfo.coverageOutDesc;

        m_regs.context.cbCoverageOutCntl.bits.COVERAGE_OUT_ENABLE  = 1;
        m_regs.context.cbCoverageOutCntl.bits.COVERAGE_OUT_MRT     = coverageInfo.flags.mrt;
        m_regs.context.cbCoverageOutCntl.bits.COVERAGE_OUT_SAMPLES = Log2(coverageInfo.flags.numSamples);
        m_regs.context.cbCoverageOutCntl.bits.COVERAGE_OUT_CHANNEL = coverageInfo.flags.channel;

        // The target and the shader mask need to be modified to ensure the coverage-out channel / MRT
        // combination is enabled.
        const uint32 mask = 1 << (coverageInfo.flags.channel +
                                  CB_SHADER_MASK__OUTPUT1_ENABLE__SHIFT * coverageInfo.flags.mrt);

        m_regs.context.cbShaderMask.u32All |= mask;
        m_regs.context.cbTargetMask.u32All |= mask;
    }

    if (m_signature.uavExportTableAddr != UserDataNotMapped)
    {
        m_uavExportRequiresFlush = (createInfo.cbState.uavExportSingleDraw == false);
    }

    // Override some register settings based on toss points.  These toss points cannot be processed in the hardware
    // independent class because they cannot be overridden by altering the pipeline creation info.
    if ((IsInternal() == false) && (m_pDevice->Parent()->Settings().tossPointMode == TossPointAfterPs))
    {
        // This toss point is used to disable all color buffer writes.
        m_regs.context.cbTargetMask.u32All = 0;
    }

    if (pUploader->EnableLoadIndexPath())
    {
        pUploader->AddCtxReg(mmPA_SC_LINE_CNTL,  m_regs.context.paScLineCntl);
        pUploader->AddCtxReg(mmCB_COLOR_CONTROL, m_regs.context.cbColorControl);
        pUploader->AddCtxReg(mmCB_SHADER_MASK,   m_regs.context.cbShaderMask);
        pUploader->AddCtxReg(mmCB_TARGET_MASK,   m_regs.context.cbTargetMask);
        if (IsGsEnabled() || IsNgg() || IsTessEnabled())
        {
            pUploader->AddCtxReg(mmVGT_GS_ONCHIP_CNTL, m_regs.context.vgtGsOnchipCntl);
        }

        if (IsGfx10Plus(m_gfxLevel))
        {
            pUploader->AddCtxReg(Gfx10Plus::mmCB_COVERAGE_OUT_CONTROL, m_regs.context.cbCoverageOutCntl);
        }
    }
}

// =====================================================================================================================
// Sets-up the late alloc limit. VS and GS will both use this function.
uint32 GraphicsPipeline::CalcMaxLateAllocLimit(
    const Device&          device,
    const RegisterVector&  registers,
    uint32                 numVgprs,
    uint32                 numSgprs,
    uint32                 scratchEn,
    uint32                 targetLateAllocLimit)
{
    const auto* pPalSettings = device.Parent()->GetPublicSettings();
    const auto& gfx9Settings = device.Settings();

    regSPI_SHADER_PGM_RSRC2_PS spiShaderPgmRsrc2Ps = { };
    spiShaderPgmRsrc2Ps.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_PS);

    // Default to a late-alloc limit of zero.  This will nearly mimic the GFX6 behavior where VS waves don't launch
    // without allocating export space.
    uint32 lateAllocLimit = 0;

    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();

    // Maximum value of the LIMIT field of the SPI_SHADER_LATE_ALLOC_VS register
    // It is the number of wavefronts minus one.
    const uint32 maxLateAllocLimit = chipProps.gfxip.maxLateAllocVsLimit - 1;

    const uint32 vsNumSgpr = (numSgprs * 8);
    const uint32 vsNumVgpr = (numVgprs * 4);

    if (gfx9Settings.lateAllocVs == LateAllocVsBehaviorDisabled)
    {
        // Disable late alloc vs entirely
        lateAllocLimit = 0;
    }
    else if (device.UseFixedLateAllocVsLimit())
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
        if ((scratchEn != 0) && (spiShaderPgmRsrc2Ps.bits.SCRATCH_EN != 0))
        {
            // The maximum number of waves per SH that can launch using scratch is the number of CUs per SH times
            // the hardware limit on scratch waves per CU.
            const uint32 maxScratchWavesPerSh = numCuForLateAllocVs * MaxScratchWavesPerCu;

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
    return programmedLimit;
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
    const bool isGfx10Plus   = IsGfx10Plus(m_gfxLevel);
    const bool isWave32Tbl[] = {
        (isGfx10Plus && (m_regs.context.vgtShaderStagesEn.gfx10Plus.HS_W32_EN != 0)),
        (isGfx10Plus && (m_regs.context.vgtShaderStagesEn.gfx10Plus.HS_W32_EN != 0)),
        (isGfx10Plus && (m_regs.context.vgtShaderStagesEn.gfx10Plus.GS_W32_EN != 0)),
        (isGfx10Plus && (m_regs.context.vgtShaderStagesEn.gfx10Plus.GS_W32_EN != 0)),
        (isGfx10Plus && (m_regs.context.vgtShaderStagesEn.gfx10Plus.VS_W32_EN != 0)),
        (isGfx10Plus && (m_regs.other.spiPsInControl.gfx10Plus.PS_W32_EN != 0)),
        false,
    };
    static_assert(ArrayLen(isWave32Tbl) == static_cast<size_t>(Abi::HardwareStage::Count),
                  "IsWave32Tbl is no longer appropriately sized!");

    uint32 scratchMemorySizeBytes = 0;
    for (uint32 i = 0; i < static_cast<uint32>(Abi::HardwareStage::Count); ++i)
    {

        const auto& stageMetadata = metadata.pipeline.hardwareStage[i];
        if (stageMetadata.hasEntry.scratchMemorySize != 0)
        {
            uint32 stageScratchMemorySize = stageMetadata.scratchMemorySize;

            if (isWave32Tbl[i] == false)
            {
                // We allocate scratch memory based on the minimum wave size for the chip, which for Gfx10+ ASICs will
                // be Wave32. In order to appropriately size the scratch memory (reported in the ELF as per-thread) for
                // a Wave64, we need to multiply by 2.
                stageScratchMemorySize *= 2;
            }

            scratchMemorySizeBytes = Max(scratchMemorySizeBytes, stageScratchMemorySize);
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
                pShaderStats->shaderStageMask       = (IsTessEnabled() ? ApiShaderStageDomain : ApiShaderStageVertex);
                pShaderStats->common.gpuVirtAddress = m_chunkGs.EsProgramGpuVa();
                if (IsGsEnabled())
                {
                    pShaderStats->shaderStageMask  |= ApiShaderStageGeometry;
                }
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
        regBase = Gfx09_10::mmSPI_SHADER_USER_DATA_VS_0;
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
    const uint16 baseRegAddr = m_pDevice->GetBaseUserDataReg(stage);
    const uint16 lastRegAddr = (baseRegAddr + 31);

    const uint32 stageId = static_cast<uint32>(stage);
    auto*const   pStage  = &m_signature.stage[stageId];

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
            else if (value == static_cast<uint32>(Abi::UserDataMapping::VertexBufferTable))
            {
                // There can be only one vertex buffer table per pipeline.
                PAL_ASSERT((m_signature.vertexBufTableRegAddr == offset) ||
                           (m_signature.vertexBufTableRegAddr == UserDataNotMapped));
                m_signature.vertexBufTableRegAddr = offset;
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::StreamOutTable))
            {
                // There can be only one stream output table per pipeline.
                PAL_ASSERT((m_signature.streamOutTableRegAddr == offset) ||
                           (m_signature.streamOutTableRegAddr == UserDataNotMapped));
                m_signature.streamOutTableRegAddr = offset;
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
            else if (value == static_cast<uint32>(Abi::UserDataMapping::ViewId))
            {
                m_signature.viewIdRegAddr[stageId] = offset;
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::PerShaderPerfData))
            {
                constexpr uint32 PalToAbiHwShaderStage[] =
                {
                    static_cast<uint32>(Abi::HardwareStage::Hs),
                    static_cast<uint32>(Abi::HardwareStage::Gs),
                    static_cast<uint32>(Abi::HardwareStage::Vs),
                    static_cast<uint32>(Abi::HardwareStage::Ps),
                };

                m_perfDataInfo[PalToAbiHwShaderStage[stageId]].regOffset = offset;
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::UavExportTable))
            {
                // There can be only one uav export table per pipeline
                PAL_ASSERT((m_signature.uavExportTableAddr == offset) ||
                           (m_signature.uavExportTableAddr == UserDataNotMapped));
                // This will still work on older gfxips but provides no perf benefits
                PAL_ASSERT(IsGfx10Plus(m_gfxLevel));
                m_signature.uavExportTableAddr = offset;
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::NggCullingData))
            {
                // There can be only one NGG culling data buffer per pipeline, and it must be used by the
                // primitive shader.
                PAL_ASSERT((m_signature.nggCullingDataAddr == offset) ||
                           (m_signature.nggCullingDataAddr == UserDataNotMapped));
                PAL_ASSERT(stage == HwShaderStage::Gs);
                m_signature.nggCullingDataAddr = offset;
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::FetchShaderPtr))
            {
                 PAL_ASSERT((m_fetchShaderRegAddr == offset) ||
                            (m_fetchShaderRegAddr == UserDataNotMapped));
                 m_fetchShaderRegAddr = offset;
            }
            else
            {
                // This appears to be an illegally-specified user-data register!
                PAL_NEVER_CALLED();
            }
        } // If HasEntry()
    } // For each user-SGPR

    if ((stage == HwShaderStage::Gs) && (m_signature.nggCullingDataAddr == UserDataNotMapped))
    {
        // It is also supported to use the LO/HI GS program registers as the NGG culling data constant buffer. Check
        // that register if we haven't seen an address for the culling data buffer yet.
        uint32 value = 0;
        if (registers.HasEntry(mmSPI_SHADER_PGM_LO_GS, &value) &&
            (value == static_cast<uint32>(Abi::UserDataMapping::NggCullingData)))
        {
            m_signature.nggCullingDataAddr = mmSPI_SHADER_PGM_LO_GS;
        }
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
        SetupSignatureForStageFromElf(metadata, registers, HwShaderStage::Hs, nullptr);
    }
    if (IsGsEnabled() || IsNgg())
    {
        SetupSignatureForStageFromElf(metadata, registers, HwShaderStage::Gs, pEsGsLdsSizeRegGs);
    }
    if (IsNgg() == false)
    {
        SetupSignatureForStageFromElf(metadata, registers, HwShaderStage::Vs, pEsGsLdsSizeRegVs);
    }
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
    case ChNumFormat::P8_Unorm:
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
    uint32 sxBlendOptControl = 0;

    // In order to determine if alpha or color channels are meaningful to the blender, the blend equations and
    // coefficients need to be examined for any interdependency. Instead, rely on the SX optimization result except for
    // the trivial cases: write disabled here and blend disabled using COMB_FCN of SX_MRTx_BLEND_OPT.
    if (writeMask == 0)
    {
        sxBlendOptControl = SX_BLEND_OPT_CONTROL__MRT0_COLOR_OPT_DISABLE_MASK |
                            SX_BLEND_OPT_CONTROL__MRT0_ALPHA_OPT_DISABLE_MASK;
    }

    return sxBlendOptControl;
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

    return (m_regs.context.cbShaderMask.u32All != 0)     &&
           (dbShaderControl.bits.KILL_ENABLE == 0)       &&
           (dbShaderControl.bits.EXEC_ON_HIER_FAIL == 0) &&
           (dbShaderControl.bits.EXEC_ON_NOOP == 0)      &&
           (dbShaderControl.bits.Z_ORDER == EARLY_Z_THEN_LATE_Z);
}

// =====================================================================================================================
// Updates the NGG Primitive Constant Buffer with the values from this pipeline.
void GraphicsPipeline::UpdateNggPrimCb(
    Abi::PrimShaderCullingCb* pPrimShaderCb
    ) const
{
    pPrimShaderCb->paClVteCntl  = m_regs.context.paClVteCntl.u32All;
    pPrimShaderCb->paSuVtxCntl  = m_regs.context.paSuVtxCntl.u32All;
    pPrimShaderCb->paClClipCntl = m_regs.context.paClClipCntl.u32All;
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
        (m_regs.context.cbColorControl.bits.DISABLE_DUAL_QUAD == 0))
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
            enStereo = m_regs.context.paStereoCntl.vg12.EN_STEREO;
        }
        else if (IsVega20(device))
        {
            enStereo = m_regs.context.paStereoCntl.vg20.EN_STEREO;
        }
    }
    else
    {

        enStereo = m_regs.uconfig.geStereoCntl.bits.EN_STEREO;

    }

    return (enStereo != 0);
}

// =====================================================================================================================
// Return if hardware stereo rendering uses multiple viewports.
bool GraphicsPipeline::HwStereoRenderingUsesMultipleViewports() const
{
    const auto&  palDevice  = *(m_pDevice->Parent());
    uint32       vpIdOffset = 0;

    if (IsGfx10Plus(m_gfxLevel))
    {
        vpIdOffset = m_regs.context.paStereoCntl.gfx10Plus.VP_ID_OFFSET;
    }
    else
    {
        if (IsVega12(palDevice))
        {
            vpIdOffset = m_regs.context.paStereoCntl.vg12.VP_ID_OFFSET;
        }
        else if (IsVega20(palDevice))
        {
            vpIdOffset = m_regs.context.paStereoCntl.vg20.VP_ID_OFFSET;
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
                    SetPaStereoCntl(rtSliceOffset, vpIdOffset, &m_regs.context.paStereoCntl.vg12);
                }
                else if (IsVega20(device))
                {
                    SetPaStereoCntl(rtSliceOffset, vpIdOffset, &m_regs.context.paStereoCntl.vg20);
                }
            }
            else
            {
                const uint32  vpIdOffset    = viewInstancingDesc.viewportArrayIdx[1] -
                                              viewInstancingDesc.viewportArrayIdx[0];
                const uint32  rtSliceOffset = viewInstancingDesc.renderTargetArrayIdx[1] -
                                              viewInstancingDesc.renderTargetArrayIdx[0];

                m_regs.context.paStereoCntl.gfx10Plus.VP_ID_OFFSET    = vpIdOffset;
                m_regs.context.paStereoCntl.gfx10Plus.RT_SLICE_OFFSET = rtSliceOffset;

                if ((vpIdOffset != 0) || (rtSliceOffset != 0))
                {
                    m_regs.uconfig.geStereoCntl.bits.EN_STEREO = 1;
                }

                m_regs.uconfig.geStereoCntl.bits.VIEWPORT = viewInstancingDesc.viewportArrayIdx[0];
                m_regs.uconfig.geStereoCntl.bits.RT_SLICE = viewInstancingDesc.renderTargetArrayIdx[0];

                if (m_regs.uconfig.geStereoCntl.bits.VIEWPORT != 0)
                {
                    m_regs.context.vgtDrawPayloadCntl.gfx10Plus.EN_DRAW_VP = 1;
                }

                if (m_regs.uconfig.geStereoCntl.bits.RT_SLICE != 0)
                {
                    m_regs.context.vgtDrawPayloadCntl.bits.EN_REG_RT_INDEX = 1;
                }
            }
        }
    }
}

// =====================================================================================================================
// Setup fetch shader info.
void GraphicsPipeline::SetupFetchShaderInfo(
    const GraphicsPipelineUploader* pUploader)
{
    GpuSymbol symbol = { };

    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::FsMainEntry, &symbol) == Result::Success)
    {
        m_fetchShaderPgm = symbol.gpuVirtAddr;
    }
}

// =====================================================================================================================
// Writes PM4 commands to program the fetch shader addr to user data registers.
uint32* GraphicsPipeline::WriteFsShCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace,
    uint32     fetchShaderRegAddr
    )const
{
    if (m_fetchShaderRegAddr != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetSeqShRegs(fetchShaderRegAddr,
                                                  (fetchShaderRegAddr + 1),
                                                  ShaderGraphics,
                                                  &m_fetchShaderPgm,
                                                  pCmdSpace);
    }

    return pCmdSpace;
}

} // Gfx9
} // Pal
