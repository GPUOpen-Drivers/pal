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

#include "core/device.h"
#include "core/platform.h"
#include "core/hw/gfxip/gfx9/gfx9AbiToPipelineRegisters.h"
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
#if PAL_BUILD_GFX11
    UserDataNotMapped,          // Stream-out control buffer register address
#endif
    UserDataNotMapped,          // UAV export table mapping
    UserDataNotMapped,          // NGG culling data constant buffer
    UserDataNotMapped,          // Vertex offset register address
    UserDataNotMapped,          // Draw ID register address
    UserDataNotMapped,          // Mesh dispatch dimensions register (1st of 3)
    UserDataNotMapped,          // Ring index for the mesh shader.
    UserDataNotMapped,          // Mesh pipeline stats buffer register address
    NoUserDataSpilling,         // Spill threshold
    0,                          // User-data entry limit
    { UserDataNotMapped, },     // Compacted view ID register addresses
    { 0, },                     // User-data mapping hashes per-stage
};
static_assert(UserDataNotMapped == 0, "Unexpected value for indicating unmapped user-data entries!");

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
    m_rbplusRegHash(0),
    m_configRegHash(0),
    m_fastLaunchMode(GsFastLaunchMode::Disabled),
    m_nggSubgroupSize(0),
    m_flags{},
#if PAL_BUILD_GFX11
    m_strmoutVtxStride(),
#endif
    m_primAmpFactor(1),
    m_chunkHs(*pDevice,
              &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Hs)]),
    m_chunkGs(*pDevice,
              &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Gs)]),
    m_chunkVsPs(*pDevice,
                &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Vs)],
                &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Ps)]),
    m_regs{},
    m_prefetch{},
    m_signature{NullGfxSignature}
{
#if PAL_BUILD_GFX11
    m_flags.contextPairsPacketSupported = pDevice->Settings().gfx11EnableContextRegPairOptimization;
    m_flags.shPairsPacketSupported      = pDevice->Settings().gfx11EnableShRegPairOptimization;
#endif
}

// =====================================================================================================================
// Early HWL initialization for the pipeline.  Responsible for determining the number of SH and context registers to be
// loaded using LOAD_SH_REG_INDEX and LOAD_CONTEXT_REG_INDEX, as well as determining things like which shader stages are
// active.
void GraphicsPipeline::EarlyInit(
    const PalAbi::CodeObjectMetadata& metadata,
    GraphicsPipelineLoadInfo*         pInfo)
{
    const Pal::Device&       palDevice = *m_pDevice->Parent();
    const GpuChipProperties& chipProps = palDevice.ChipProperties();
    const GfxIpLevel         gfxLevel  = chipProps.gfxLevel;

    // We must set up which stages are enabled first!
    {
        m_regs.context.vgtShaderStagesEn.u32All = AbiRegisters::VgtShaderStagesEn(metadata, *m_pDevice, gfxLevel);
        m_fastLaunchMode  =
            (IsGfx10Plus(m_gfxLevel)) ?
            static_cast<GsFastLaunchMode>(metadata.pipeline.graphicsRegister.vgtShaderStagesEn.gsFastLaunch) :
            GsFastLaunchMode::Disabled;
    }

    m_nggSubgroupSize = (metadata.pipeline.hasEntry.nggSubgroupSize) ? metadata.pipeline.nggSubgroupSize : 0;

    // Determine whether or not GS is onchip or not.
    if (chipProps.gfxip.supportsHwVs == false)
    {
        // GS is always on chip.
        SetIsGsOnChip(IsGsEnabled());
    }
    else if (IsGsEnabled() && (metadata.pipeline.graphicsRegister.vgtGsMode.onchip != 0))
    {
        SetIsGsOnChip(true);
    }

#if PAL_BUILD_GFX11
    if (metadata.pipeline.hasEntry.streamoutVertexStrides)
    {
        memcpy(m_strmoutVtxStride,
               metadata.pipeline.streamoutVertexStrides,
               sizeof(m_strmoutVtxStride));
    }
#endif

    // Must be called *after* determining active HW stages!
    SetupSignatureFromElf(metadata, &pInfo->esGsLdsSizeRegGs, &pInfo->esGsLdsSizeRegVs);

    pInfo->enableNgg    = IsNgg();
    pInfo->usesOnChipGs = IsGsOnChip();

    if (IsGsEnabled() || pInfo->enableNgg)
    {
        m_chunkGs.EarlyInit(pInfo);
    }
    m_chunkVsPs.EarlyInit(metadata, pInfo);
}

// =====================================================================================================================
// Initializes HW-specific state related to this graphics pipeline (register values, user-data mapping, etc.) using the
// specified Pipeline ABI processor and create info.
Result GraphicsPipeline::HwlInit(
    const GraphicsPipelineCreateInfo& createInfo,
    const AbiReader&                  abiReader,
    const PalAbi::CodeObjectMetadata& metadata,
    MsgPackReader*                    pMetadataReader)
{
    GraphicsPipelineLoadInfo loadInfo = { };
    EarlyInit(metadata, &loadInfo);

    // Next, handle relocations and upload the pipeline code & data to GPU memory.
    PipelineUploader uploader(m_pDevice->Parent(), abiReader);
    Result result = PerformRelocationsAndUploadToGpuMemory(
        metadata,
        IsInternal() ? GpuHeapLocal : m_pDevice->Parent()->GetPublicSettings()->pipelinePreferredHeap,
        &uploader);

    if (result == Result::Success)
    {
        LateInit(createInfo, abiReader, metadata, loadInfo, &uploader);
        PAL_ASSERT(m_uploadFenceToken == 0);
        result = uploader.End(&m_uploadFenceToken);
    }

    return result;
}

// =====================================================================================================================
void GraphicsPipeline::LateInit(
    const GraphicsPipelineCreateInfo& createInfo,
    const AbiReader&                  abiReader,
    const PalAbi::CodeObjectMetadata& metadata,
    const GraphicsPipelineLoadInfo&   loadInfo,
    PipelineUploader*                 pUploader)
{
    const Gfx9PalSettings&   settings        = m_pDevice->Settings();
    const PalPublicSettings* pPublicSettings = m_pDevice->Parent()->GetPublicSettings();
    MetroHash64              hasher;

    if (IsTessEnabled())
    {
        m_chunkHs.LateInit(abiReader, metadata, pUploader, &hasher);
    }
    if (IsGsEnabled() || IsNgg())
    {
        m_chunkGs.LateInit(abiReader, metadata, loadInfo, createInfo, pUploader, &hasher);
    }
    m_chunkVsPs.LateInit(abiReader, metadata, loadInfo, createInfo, pUploader, &hasher);

    SetupCommonRegisters(createInfo, metadata);
    SetupNonShaderRegisters(createInfo, metadata);
    SetupStereoRegisters();

    if (pPublicSettings->optDepthOnlyExportRate && CanRbPlusOptimizeDepthOnly())
    {
        m_regs.other.sxPsDownconvert.bits.MRT0                    = SX_RT_EXPORT_32_R;
        m_regs.other.sxPsDownconvertDual.bits.MRT0                = SX_RT_EXPORT_32_R;
        m_regs.context.spiShaderColFormat.bits.COL0_EXPORT_FORMAT = SPI_SHADER_32_R;
    }

    MetroHash::Hash hash = {};

    hasher.Update(m_regs.context);
    hasher.Finalize(hash.bytes);
    m_contextRegHash = MetroHash::Compact32(&hash);

    hasher.Initialize();
    hasher.Update(reinterpret_cast<const uint8*>(&m_regs.other.sxPsDownconvert),
                  sizeof(m_regs.other.sxPsDownconvert) +
                  sizeof(m_regs.other.sxBlendOptEpsilon) +
                  sizeof(m_regs.other.sxBlendOptControl));
    hasher.Finalize(hash.bytes);
    m_rbplusRegHash = MetroHash::Compact32(&hash);

    hasher.Initialize();
    hasher.Update(reinterpret_cast<const uint8*>(&m_regs.other.sxPsDownconvertDual),
        sizeof(m_regs.other.sxPsDownconvertDual) +
        sizeof(m_regs.other.sxBlendOptEpsilonDual) +
        sizeof(m_regs.other.sxBlendOptControlDual));
    hasher.Finalize(hash.bytes);
    m_rbplusRegHashDual = MetroHash::Compact32(&hash);

    // We write our config registers in a separate function so they get their own hash.
    // Also, we only set config registers on gfx10+.
    if (IsGfx10Plus(m_gfxLevel))
    {
        hasher.Initialize();
        hasher.Update(m_regs.uconfig);
        hasher.Finalize(hash.bytes);
        m_configRegHash = MetroHash::Compact32(&hash);

        m_primAmpFactor = m_chunkGs.PrimAmpFactor();
    }

    DetermineBinningOnOff();

    if (m_pDevice->CoreSettings().pipelinePrefetchEnable &&
        (settings.shaderPrefetchMethodGfx != PrefetchDisabled))
    {
        m_prefetch.gpuVirtAddr         = pUploader->PrefetchAddr();
        m_prefetch.size                = pUploader->PrefetchSize();
        m_prefetch.usageMask           = CoherShaderRead;
        m_prefetch.addrTranslationOnly = (settings.shaderPrefetchMethodGfx == PrefetchPrimeUtcL2);
    }

    // Updating the ring sizes expects that all of the register state has been setup.
    UpdateRingSizes(metadata);
}

// =====================================================================================================================
void GraphicsPipeline::DetermineBinningOnOff()
{
    const auto* const pPublicSettings = m_pDevice->Parent()->GetPublicSettings();
    bool disableBinning = (pPublicSettings->binningMode == DeferredBatchBinDisabled);

    const regDB_SHADER_CONTROL& dbShaderControl = m_chunkVsPs.DbShaderControl();

    const bool canKill =
        dbShaderControl.bits.KILL_ENABLE             ||
        dbShaderControl.bits.MASK_EXPORT_ENABLE      ||
        dbShaderControl.bits.COVERAGE_TO_MASK_ENABLE ||
        (dbShaderControl.bits.ALPHA_TO_MASK_DISABLE == 0);

    const bool canReject =
        (dbShaderControl.bits.Z_EXPORT_ENABLE == 0) ||
        (dbShaderControl.bits.CONSERVATIVE_Z_EXPORT > 0);
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 749)
    // Disable binning when the pixels can be rejected before the PS and the PS can kill the pixel.
    // This is an optimization for cases where early Z accepts are not allowed (because the shader may kill) and early
    // Z rejects are allowed (PS does not output depth).
    // In such cases the binner orders pixel traffic in a suboptimal way.
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 753)
    disableBinning |= canKill && canReject && (pPublicSettings->disableBinningPsKill
                                               == OverrideMode::Enabled);
#else
    disableBinning |= canKill && canReject && (pPublicSettings->disableBinningPsKill
                                               == DisableBinningPsKill::_True);
#endif
#else
    disableBinning |=
        canKill && canReject;
#endif

    // Disable binning when the PS uses append/consume.
    // In such cases, binning changes the ordering of append/consume opeartions. This re-ordering can be suboptimal.
    disableBinning |= PsUsesAppendConsume() && m_pDevice->Settings().disableBinningAppendConsume;

    disableBinning |= (GetBinningOverride() == BinningOverride::Disable);

    m_flags.binningAllowed =
        ((disableBinning        == false) ||
         (GetBinningOverride()  == BinningOverride::Enable));
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
uint32 GraphicsPipeline::CalcMaxWavesPerSe(
    float maxWavesPerCu1,
    float maxWavesPerCu2
    ) const
{
    // The maximum number of waves per SH in "register units".
    // By default set the WAVE_LIMIT field to be unlimited.
    // Limits given by the ELF will only apply if the caller doesn't set their own limit.
    uint32 wavesPerSe = 0;

    const auto& gfx9ChipProps = m_pDevice->Parent()->ChipProperties().gfx9;
    wavesPerSe = CalcMaxWavesPerSh(maxWavesPerCu1, maxWavesPerCu2) * gfx9ChipProps.numShaderArrays;

    return wavesPerSe;
}

// =====================================================================================================================
// Helper function to compute the max wave per SH.
uint32 GraphicsPipeline::CalcMaxWavesPerSh(
    float maxWavesPerCu1,
    float maxWavesPerCu2) const
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
        const GpuChipProperties& chipProps          = m_pDevice->Parent()->ChipProperties();
        const uint32             numWavefrontsPerCu = chipProps.gfx9.numSimdPerCu * chipProps.gfx9.numWavesPerSimd;

        // We assume no one is trying to use more than 100% of all waves.
        PAL_ASSERT(maxWavesPerCu <= numWavefrontsPerCu);
        const uint32 maxWavesPerSh = static_cast<uint32>(round(maxWavesPerCu * chipProps.gfx9.numCuPerSh));

        // For graphics shaders, the WAVES_PER_SH field is in units of 16 waves and must not exceed 63. We must
        // also clamp to one if maxWavesPerSh rounded down to zero to prevent the limit from being removed.
        wavesPerSh = Min(
            m_pDevice->GetMaxWavesPerSh(chipProps, false), Max(1u, maxWavesPerSh / Gfx9MaxWavesPerShGraphicsUnitSize));
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
    if (IsGfx10Plus(m_gfxLevel))
    {
        pStageInfo->wavesPerSh   = CalcMaxWavesPerSe(shaderInfo.maxWavesPerCu, 0);
    }
    else
    {
        pStageInfo->wavesPerSh   = CalcMaxWavesPerSh(shaderInfo.maxWavesPerCu, 0);
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 789
    pStageInfo->cuEnableMask = shaderInfo.cuEnableMask;
#endif
}

// =====================================================================================================================
// Helper for setting the dynamic stage info.
void GraphicsPipeline::CalcDynamicStageInfo(
    const DynamicGraphicsShaderInfo& shaderInfo1,
    const DynamicGraphicsShaderInfo& shaderInfo2,
    DynamicStageInfo*                pStageInfo
    ) const
{
    if (IsGfx10Plus(m_gfxLevel))
    {
        pStageInfo->wavesPerSh   = CalcMaxWavesPerSe(shaderInfo1.maxWavesPerCu, shaderInfo2.maxWavesPerCu);
    }
    else
    {
        pStageInfo->wavesPerSh   = CalcMaxWavesPerSh(shaderInfo1.maxWavesPerCu, shaderInfo2.maxWavesPerCu);
    }
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 789
    pStageInfo->cuEnableMask = shaderInfo1.cuEnableMask & shaderInfo2.cuEnableMask;
#endif
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
        if (HasMeshShader())
        {
            // HasMeshShader(): PipelineMesh
            // API Shader -> Hardware Stage
            // PS -> PS
            // MS -> GS

            CalcDynamicStageInfo(graphicsInfo.ms, &pStageInfos->gs);
        }
        else if (IsNgg() || IsGsEnabled())
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

#if PAL_BUILD_GFX11
    if (m_flags.shPairsPacketSupported)
    {
        constexpr uint32 MaxNumRegisters = GfxPipelineRegs::NumShReg +
                                           HsRegs::NumShReg          +
                                           GsRegs::NumShReg          +
                                           VsPsRegs::NumShReg        +
                                           2; // For the fetch shader address.
        constexpr uint32 MaxNumRegPairs  = Pow2Align(MaxNumRegisters, 2) / 2;

        static_assert(MaxNumRegisters <= Gfx11RegPairMaxRegCount, "Requesting too many registers!");

        PackedRegisterPair regPairs[MaxNumRegPairs];
        uint32             numRegs = 0;

        if (IsTessEnabled())
        {
            m_chunkHs.AccumulateShRegs(regPairs, &numRegs);
        }

        m_chunkGs.AccumulateShRegs(regPairs, &numRegs, HasMeshShader());

        m_chunkVsPs.AccumulateShRegs(regPairs, &numRegs);

        PAL_ASSERT(numRegs < MaxNumRegisters);

        pCmdSpace = pCmdStream->WriteSetShRegPairs<ShaderGraphics>(regPairs, numRegs, pCmdSpace);
    }
    else
#endif
    {
        // If NGG is enabled, there is no hardware-VS, so there is no need to write the late-alloc VS limit.
        if (IsNgg() == false)
        {
            pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(HasHwVs::mmSPI_SHADER_LATE_ALLOC_VS,
                                                                     m_regs.sh.spiShaderLateAllocVs.u32All,
                                                                     pCmdSpace);
        }

        if (IsTessEnabled())
        {
            pCmdSpace = m_chunkHs.WriteShCommands(pCmdStream, pCmdSpace);
        }
        if (IsGsEnabled() || IsNgg())
        {
            pCmdSpace = m_chunkGs.WriteShCommands(pCmdStream, pCmdSpace, HasMeshShader());
        }
        pCmdSpace = m_chunkVsPs.WriteShCommands(pCmdStream, pCmdSpace, IsNgg());
    }

    pCmdSpace = WriteDynamicRegisters(pCmdStream, pCmdSpace, graphicsInfo);

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

#if PAL_BUILD_GFX11
    if (m_flags.contextPairsPacketSupported)
    {
        constexpr uint32 MaxNumRegisters = GfxPipelineRegs::NumContextReg +
                                           HsRegs::NumContextReg          +
                                           GsRegs::NumContextReg          +
                                           VsPsRegs::NumContextReg;
        constexpr uint32 MaxNumRegPairs  = Pow2Align(MaxNumRegisters, 2) / 2;

        static_assert(MaxNumRegisters <= Gfx11RegPairMaxRegCount, "Requesting too many registers!");

        PackedRegisterPair regPairs[MaxNumRegPairs];
        uint32             numRegs = 0;

        AccumulateContextRegisters(regPairs, &numRegs);

        if (IsTessEnabled())
        {
            m_chunkHs.AccumulateContextRegs(regPairs, &numRegs);
        }
        if (IsGsEnabled() || IsNgg())
        {
            m_chunkGs.AccumulateContextRegs(regPairs, &numRegs);
        }

        m_chunkVsPs.AccumulateContextRegs(regPairs, &numRegs);

        PAL_ASSERT(numRegs < MaxNumRegisters);

        pCmdSpace = pCmdStream->WriteSetContextRegPairs(regPairs, numRegs, pCmdSpace);
    }
    else
#endif
    {
        pCmdSpace = WriteContextCommandsSetPath(pCmdStream, pCmdSpace);

        if (IsTessEnabled())
        {
            pCmdSpace = m_chunkHs.WriteContextCommands(pCmdStream, pCmdSpace);
        }
        if (IsGsEnabled() || IsNgg())
        {
            pCmdSpace = m_chunkGs.WriteContextCommands(pCmdStream, pCmdSpace);
        }

        pCmdSpace = m_chunkVsPs.WriteContextCommands(pCmdStream, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
uint32* GraphicsPipeline::WriteDynamicRegisters(
    CmdStream*                        pCmdStream,
    uint32*                           pCmdSpace,
    const DynamicGraphicsShaderInfos& graphicsInfo
    ) const
{
    DynamicStageInfos stageInfos = {};
    CalcDynamicStageInfos(graphicsInfo, &stageInfos);

    if (IsTessEnabled())
    {
        pCmdSpace = m_chunkHs.WriteDynamicRegs(pCmdStream, pCmdSpace, stageInfos.hs);
    }
    if (IsGsEnabled() || IsNgg())
    {
        pCmdSpace = m_chunkGs.WriteDynamicRegs(pCmdStream, pCmdSpace, stageInfos.gs);
    }
    pCmdSpace = m_chunkVsPs.WriteDynamicRegs(pCmdStream, pCmdSpace, IsNgg(), stageInfos.vs, stageInfos.ps);

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

#if PAL_BUILD_GFX11
    if (IsGfx11(m_gfxLevel) && (IsGsEnabled() || HasMeshShader() || (m_fastLaunchMode != GsFastLaunchMode::Disabled)))
    {
        // Prim type is implicitly set for API VS without fast-launch, but needs to be set otherwise.
        pCmdSpace = pCmdStream->WriteSetOneConfigReg(Gfx11::mmVGT_GS_OUT_PRIM_TYPE,
                                                    m_regs.uconfig.vgtGsOutPrimType.u32All,
                                                    pCmdSpace);
    }
#endif

    return pCmdSpace;
}

// =====================================================================================================================
// Requests that this pipeline indicates what it would like to prefetch.
uint32* GraphicsPipeline::Prefetch(
    uint32* pCmdSpace
    ) const
{
    if (m_prefetch.gpuVirtAddr != 0)
    {
        pCmdSpace += m_pDevice->CmdUtil().BuildPrimeGpuCaches(m_prefetch, EngineTypeUniversal, pCmdSpace);
    }

    return pCmdSpace;
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
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmVGT_REUSE_OFF, m_regs.context.vgtReuseOff.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmVGT_DRAW_PAYLOAD_CNTL,
                                                  m_regs.context.vgtDrawPayloadCntl.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmCB_SHADER_MASK,
                                                  m_regs.context.cbShaderMask.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmPA_SU_VTX_CNTL, m_regs.context.paSuVtxCntl.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmPA_CL_VTE_CNTL, m_regs.context.paClVteCntl.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmPA_SC_EDGERULE, m_regs.context.paScEdgerule.u32All, pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmSPI_INTERP_CONTROL_0,
                                                  m_regs.context.spiInterpControl0.u32All,
                                                  pCmdSpace);

    if (IsGfx9(m_gfxLevel) || ((IsGsEnabled() == false) && (IsNgg() == false)))
    {
        pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmSPI_SHADER_POS_FORMAT,
                                                       mmSPI_SHADER_COL_FORMAT,
                                                       &m_regs.context.spiShaderPosFormat,
                                                       pCmdSpace);
    }
    else
    {
        pCmdSpace = pCmdStream->WriteSetSeqContextRegs(Gfx10Plus::mmSPI_SHADER_IDX_FORMAT,
                                                       mmSPI_SHADER_COL_FORMAT,
                                                       &m_regs.context.spiShaderIdxFormat,
                                                       pCmdSpace);
    }

    if (m_pDevice->Parent()->ChipProperties().gfxip.supportsHwVs)
    {
        pCmdSpace = pCmdStream->WriteSetOneContextReg(HasHwVs::mmVGT_GS_MODE,
                                                      m_regs.context.vgtGsMode.u32All,
                                                      pCmdSpace);
        pCmdSpace = pCmdStream->WriteSetOneContextReg(HasHwVs::mmVGT_VERTEX_REUSE_BLOCK_CNTL,
                                                      m_regs.context.vgtVertexReuseBlockCntl.u32All,
                                                      pCmdSpace);
    }

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

    if ((IsGsEnabled() || IsNgg() || IsTessEnabled())
#if PAL_BUILD_GFX11
        && (IsGfx11(m_gfxLevel) == false)
#endif
       )
    {
        pCmdSpace = pCmdStream->WriteSetOneContextReg(Gfx09_10::mmVGT_GS_ONCHIP_CNTL,
                                                      m_regs.context.vgtGsOnchipCntl.u32All,
                                                      pCmdSpace);
    }

    return pCmdSpace;
}

#if PAL_BUILD_GFX11
// =====================================================================================================================
void GraphicsPipeline::AccumulateContextRegisters(
    PackedRegisterPair* pRegPairs,
    uint32*             pNumRegs
    ) const
{
    const CmdUtil&      cmdUtil = m_pDevice->CmdUtil();
    const RegisterInfo& regInfo = cmdUtil.GetRegInfo();

    SetOneContextRegValPairPacked(pRegPairs, pNumRegs, mmVGT_SHADER_STAGES_EN, m_regs.context.vgtShaderStagesEn.u32All);
    SetOneContextRegValPairPacked(pRegPairs, pNumRegs, mmVGT_REUSE_OFF,        m_regs.context.vgtReuseOff.u32All);
    SetOneContextRegValPairPacked(pRegPairs,
                                  pNumRegs,
                                  mmVGT_DRAW_PAYLOAD_CNTL,
                                  m_regs.context.vgtDrawPayloadCntl.u32All);

    SetOneContextRegValPairPacked(pRegPairs, pNumRegs, mmPA_SU_VTX_CNTL,       m_regs.context.paSuVtxCntl.u32All);
    SetOneContextRegValPairPacked(pRegPairs, pNumRegs, mmPA_CL_VTE_CNTL,       m_regs.context.paClVteCntl.u32All);
    SetOneContextRegValPairPacked(pRegPairs, pNumRegs, mmPA_SC_EDGERULE,       m_regs.context.paScEdgerule.u32All);
    SetOneContextRegValPairPacked(pRegPairs, pNumRegs, mmSPI_INTERP_CONTROL_0, m_regs.context.spiInterpControl0.u32All);
    SetOneContextRegValPairPacked(pRegPairs, pNumRegs, mmCB_SHADER_MASK,       m_regs.context.cbShaderMask.u32All);

    if (IsGfx9(m_gfxLevel) || ((IsGsEnabled() == false) && (IsNgg() == false)))
    {
        SetSeqContextRegValPairPacked(pRegPairs,
                                      pNumRegs,
                                      mmSPI_SHADER_POS_FORMAT,
                                      mmSPI_SHADER_COL_FORMAT,
                                      &m_regs.context.spiShaderPosFormat);
    }
    else
    {
        SetSeqContextRegValPairPacked(pRegPairs,
                                      pNumRegs,
                                      Gfx10Plus::mmSPI_SHADER_IDX_FORMAT,
                                      mmSPI_SHADER_COL_FORMAT,
                                      &m_regs.context.spiShaderIdxFormat);
    }

    if (m_pDevice->Parent()->ChipProperties().gfxip.supportsHwVs)
    {
        SetOneContextRegValPairPacked(pRegPairs,
                                      pNumRegs,
                                      HasHwVs::mmVGT_GS_MODE,
                                      m_regs.context.vgtGsMode.u32All);
        SetOneContextRegValPairPacked(pRegPairs,
                                      pNumRegs,
                                      HasHwVs::mmVGT_VERTEX_REUSE_BLOCK_CNTL,
                                      m_regs.context.vgtVertexReuseBlockCntl.u32All);
    }

    if (regInfo.mmPaStereoCntl != 0)
    {
        SetOneContextRegValPairPacked(pRegPairs,
                                      pNumRegs,
                                      regInfo.mmPaStereoCntl,
                                      m_regs.context.paStereoCntl.u32All);
    }

    if (IsGfx10Plus(m_gfxLevel))
    {
        SetOneContextRegValPairPacked(pRegPairs,
                                      pNumRegs,
                                      Gfx10Plus::mmCB_COVERAGE_OUT_CONTROL,
                                      m_regs.context.cbCoverageOutCntl.u32All);
    }

    if ((IsGsEnabled() || IsNgg() || IsTessEnabled())
#if PAL_BUILD_GFX11
        && (IsGfx11(m_gfxLevel) == false)
#endif
       )
    {
        SetOneContextRegValPairPacked(pRegPairs,
                                      pNumRegs,
                                      Gfx09_10::mmVGT_GS_ONCHIP_CNTL,
                                      m_regs.context.vgtGsOnchipCntl.u32All);
    }
}
#endif

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

    const SX_DOWNCONVERT_FORMAT downConvertFormat = SxDownConvertFormat(swizzledFormat);
    const uint32                blendOptControl   = Gfx9::SxBlendOptControl(writeMask);

    // A value of 1 in SRGB is 0.0003035, so even the lowest epsilon 0.0003662 will not work.
    // 0 is the only safe value.
    const uint32                blendOptEpsilon   =
        ((downConvertFormat == SX_RT_EXPORT_NO_CONVERSION) ||
         (Formats::IsSrgb(swizzledFormat.format))) ? 0 : Gfx9::SxBlendOptEpsilon(downConvertFormat);

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
    const PalAbi::CodeObjectMetadata& metadata)
{
    const auto&              palDevice    = *(m_pDevice->Parent());
    const GpuChipProperties& chipProps    = palDevice.ChipProperties();
    const Gfx9PalSettings&   settings     = m_pDevice->Settings();
    const PalPublicSettings* pPalSettings = m_pDevice->Parent()->GetPublicSettings();

    m_regs.context.paClVteCntl.u32All        = AbiRegisters::PaClVteCntl(metadata);
    m_regs.context.paSuVtxCntl.u32All        = AbiRegisters::PaSuVtxCntl(metadata);
    m_regs.context.spiShaderIdxFormat.u32All = AbiRegisters::SpiShaderIdxFormat(metadata);
    m_regs.context.spiShaderColFormat.u32All = AbiRegisters::SpiShaderColFormat(metadata);
    m_regs.context.spiShaderPosFormat.u32All = AbiRegisters::SpiShaderPosFormat(metadata, m_gfxLevel);
    m_regs.context.spiShaderZFormat.u32All   = AbiRegisters::SpiShaderZFormat(metadata);
    m_regs.context.spiInterpControl0.u32All  = AbiRegisters::SpiInterpControl0(metadata, createInfo);
    m_regs.context.vgtGsMode.u32All          = AbiRegisters::VgtGsMode(metadata);
    m_regs.context.vgtGsOnchipCntl.u32All    = AbiRegisters::VgtGsOnchipCntl(metadata);
    m_regs.context.vgtReuseOff.u32All        = AbiRegisters::VgtReuseOff(metadata);

    m_regs.context.vgtDrawPayloadCntl.u32All = AbiRegisters::VgtDrawPayloadCntl(metadata, *m_pDevice, m_gfxLevel);

#if PAL_BUILD_GFX11
    m_regs.uconfig.vgtGsOutPrimType.u32All   = AbiRegisters::VgtGsOutPrimType(metadata, m_gfxLevel);
#endif
    m_regs.other.paClClipCntl.u32All   = AbiRegisters::PaClClipCntl(metadata, *m_pDevice, createInfo);
    m_regs.other.vgtTfParam.u32All     = AbiRegisters::VgtTfParam(metadata, m_gfxLevel);
    m_regs.other.spiPsInControl.u32All = AbiRegisters::SpiPsInControl(metadata, m_gfxLevel);
    m_regs.other.spiVsOutConfig.u32All = AbiRegisters::SpiVsOutConfig(metadata, *m_pDevice, m_gfxLevel);
    m_regs.other.vgtLsHsConfig.u32All  = AbiRegisters::VgtLsHsConfig(metadata);
    m_regs.other.paScModeCntl1.u32All  = AbiRegisters::PaScModeCntl1(metadata, createInfo, *m_pDevice);
    m_info.ps.flags.perSampleShading   = m_regs.other.paScModeCntl1.bits.PS_ITER_SAMPLE;

    // DB_RENDER_OVERRIDE
    {
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
                ((createInfo.rsState.depthClampMode == DepthClampMode::_None) &&
                 (m_chunkVsPs.DbShaderControl().bits.Z_EXPORT_ENABLE != 0));
        }
        else
        {
            // Vulkan (only) will take this path by default, unless an app-detect forces the other way.
            m_regs.other.dbRenderOverride.bits.DISABLE_VIEWPORT_CLAMP =
                (createInfo.rsState.depthClampMode == DepthClampMode::_None);
        }
    }

    // PA_STEREO_CNTL/GE_STEREO_CNTL
    {
        // These are really HW enumerations that don't show up in the official HW header files.  Define them
        // here for convenience.
        enum StereoMode
        {
            SHADER_STEREO_X    = 0x00000000,
            STATE_STEREO_X     = 0x00000001,
            SHADER_STEREO_XYZW = 0x00000002,
        };

        if (IsGfx10Plus(m_gfxLevel))
        {
            m_regs.uconfig.geStereoCntl.u32All           = 0;
            m_regs.context.paStereoCntl.most.STEREO_MODE = STATE_STEREO_X;
        }
        else if (IsVega12(palDevice) || IsVega20(palDevice))
        {
            m_regs.context.paStereoCntl.most.STEREO_MODE = STATE_STEREO_X;
        }
    }

    // GE_USER_VGPR_EN
    {
        // We do not support this feature.
        m_regs.uconfig.geUserVgprEn.u32All = 0;
    }

    // GE_PC_ALLOC
    {
        if (IsGfx10Plus(m_gfxLevel))
        {
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
    }

    // VGT_VERTEX_REUSE_BLOCK_CNTL
    {
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
                (IsTessEnabled() && (m_regs.other.vgtTfParam.bits.PARTITIONING != PART_FRAC_ODD)))
            {
                m_regs.context.vgtVertexReuseBlockCntl.bits.VTX_REUSE_DEPTH = 30;
            }
        }
    }

    // SPI_SHADER_LATE_ALLOC_VS
    {
        // If NGG is enabled, there is no hardware-VS, so there is no need to compute the late-alloc VS limit.
        if (IsNgg() == false)
        {
            // Target late-alloc limit uses PAL settings by default. The lateAllocVsLimit member from graphicsPipeline
            // can override this setting if corresponding flag is set. Did the pipeline request to use the pipeline
            // specified late-alloc limit 4 * (gfx9Props.numCuPerSh - 1).
            const uint32 targetLateAllocLimit = IsLateAllocVsLimit()
                                                ? GetLateAllocVsLimit()
                                                : m_pDevice->LateAllocVsLimit() + 1;

            const auto& hwVs = metadata.pipeline.hardwareStage[uint32(Abi::HardwareStage::Vs)];
            const auto& hwPs = metadata.pipeline.hardwareStage[uint32(Abi::HardwareStage::Ps)];

            const uint32 programmedLimit = CalcMaxLateAllocLimit(*m_pDevice,
                                                                 hwVs.vgprCount,
                                                                 hwVs.sgprCount,
                                                                 hwVs.wavefrontSize,
                                                                 hwVs.flags.scratchEn,
                                                                 hwPs.flags.scratchEn,
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
        }
    }
    SetupIaMultiVgtParam(metadata);
}

// =====================================================================================================================
// The pipeline binary is allowed to partially specify the value for IA_MULTI_VGT_PARAM.  PAL will finish initializing
// this register based on GPU properties, pipeline create info, and the values of other registers.
void GraphicsPipeline::SetupIaMultiVgtParam(
    const PalAbi::CodeObjectMetadata& metadata)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    const PalAbi::IaMultiVgtParamMetadata& iaMultiVgtParamMetadata = metadata.pipeline.graphicsRegister.iaMultiVgtParam;

    regIA_MULTI_VGT_PARAM iaMultiVgtParam = { };
    iaMultiVgtParam.bits.PRIMGROUP_SIZE     = iaMultiVgtParamMetadata.primgroupSize;
    iaMultiVgtParam.bits.PARTIAL_VS_WAVE_ON = iaMultiVgtParamMetadata.flags.partialVsWaveOn;
    iaMultiVgtParam.bits.PARTIAL_ES_WAVE_ON = iaMultiVgtParamMetadata.flags.partialEsWaveOn;
    iaMultiVgtParam.bits.SWITCH_ON_EOP      = iaMultiVgtParamMetadata.flags.switchOnEop;
    iaMultiVgtParam.bits.SWITCH_ON_EOI      = iaMultiVgtParamMetadata.flags.switchOnEoi;

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
        if (UsesHwStreamout() && (chipProps.gfx9.numShaderEngines == 2))
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

    if (m_regs.other.vgtTfParam.bits.DISTRIBUTION_MODE != NO_DIST)
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

    if (UsesHwStreamout())
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
    if (m_fastLaunchMode == GsFastLaunchMode::Disabled)
    {
        // Advanced optimization enables basic optimization and additional sub-draw call distribution algorithm
        // which splits batch into smaller instanced draws.
        pIaMultiVgtParam->gfx09.EN_INST_OPT_ADV = 1;
    }

}

// =====================================================================================================================
// Initializes render-state registers which aren't part of any hardware shader stage.
void GraphicsPipeline::SetupNonShaderRegisters(
    const GraphicsPipelineCreateInfo& createInfo,
    const PalAbi::CodeObjectMetadata& metadata)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    const Gfx9PalSettings&   settings  = m_pDevice->Settings();

    m_regs.context.cbShaderMask.u32All = AbiRegisters::CbShaderMask(metadata);

    m_regs.other.paScLineCntl.bits.EXPAND_LINE_WIDTH        = createInfo.rsState.expandLineWidth;
    m_regs.other.paScLineCntl.bits.DX10_DIAMOND_TEST_ENA    = createInfo.rsState.dx10DiamondTestDisable ? 0 : 1;
    m_regs.other.paScLineCntl.bits.LAST_PIXEL               = createInfo.rsState.rasterizeLastLinePixel;
    m_regs.other.paScLineCntl.bits.PERPENDICULAR_ENDCAP_ENA = createInfo.rsState.perpLineEndCapsEnable;

    if (createInfo.rsState.pointCoordOrigin == Pal::PointOrigin::UpperLeft)
    {
        m_regs.context.paScEdgerule.bits.ER_TRI     = 0xa;
        m_regs.context.paScEdgerule.bits.ER_POINT   = 0xa;
        m_regs.context.paScEdgerule.bits.ER_RECT    = 0xa;
        m_regs.context.paScEdgerule.bits.ER_LINE_LR = 0x1a;
        m_regs.context.paScEdgerule.bits.ER_LINE_RL = 0x26;
        m_regs.context.paScEdgerule.bits.ER_LINE_TB = 0xa;
        m_regs.context.paScEdgerule.bits.ER_LINE_BT = 0xa;
    }
    else
    {
        m_regs.context.paScEdgerule.bits.ER_TRI     = 0xa;
        m_regs.context.paScEdgerule.bits.ER_POINT   = 0x5;
        m_regs.context.paScEdgerule.bits.ER_RECT    = 0x9;
        m_regs.context.paScEdgerule.bits.ER_LINE_LR = 0x29;
        m_regs.context.paScEdgerule.bits.ER_LINE_RL = 0x29;
        m_regs.context.paScEdgerule.bits.ER_LINE_TB = 0xa;
        m_regs.context.paScEdgerule.bits.ER_LINE_BT = 0xa;
    }

    // CB_TARGET_MASK comes from the RT write masks in the pipeline CB state structure.
    for (uint32 rt = 0; rt < MaxColorTargets; ++rt)
    {
        const auto&  cbTarget = createInfo.cbState.target[rt];
        const uint32 rtShift  = (rt * 4); // Each RT uses four bits of CB_TARGET_MASK.

        m_regs.other.cbTargetMask.u32All |= ((cbTarget.channelWriteMask & 0xF) << rtShift);

    }

    //      The bug manifests itself when an MRT is not enabled in the shader mask but is enabled in the target
    //      mask. It will work fine if the target mask is always a subset of the shader mask
    if (settings.waOverwriteCombinerTargetMaskOnly &&
        (TestAllFlagsSet(m_regs.context.cbShaderMask.u32All, m_regs.other.cbTargetMask.u32All) == false))
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
        m_regs.other.cbColorControl.bits.MODE = CB_ELIMINATE_FAST_CLEAR;
        m_regs.other.cbColorControl.bits.ROP3 = Rop3(LogicOp::Copy);

        // NOTE: the CB spec states that for fast-clear eliminate, these registers should be set to enable writes to all
        // four channels of RT #0.
        m_regs.context.cbShaderMask.u32All = 0xF;
        m_regs.other.cbTargetMask.u32All   = 0xF;
    }
    else if (IsFmaskDecompress())
    {
        m_regs.other.cbColorControl.bits.MODE = CB_FMASK_DECOMPRESS__GFX09_10;
        m_regs.other.cbColorControl.bits.ROP3 = Rop3(LogicOp::Copy);

        // NOTE: the CB spec states that for fmask-decompress, these registers should be set to enable writes to all
        // four channels of RT #0.
        m_regs.context.cbShaderMask.u32All = 0xF;
        m_regs.other.cbTargetMask.u32All   = 0xF;
    }
    else if (IsDccDecompress())
    {
#if PAL_BUILD_GFX11
        if (IsGfx11(*m_pDevice->Parent()))
        {
            m_regs.other.cbColorControl.bits.MODE = CB_DCC_DECOMPRESS__GFX11;
        }
        else
#endif
        {
            m_regs.other.cbColorControl.bits.MODE = CB_DCC_DECOMPRESS__GFX09_10;
        }

        m_regs.other.cbColorControl.bits.ROP3 = Rop3(LogicOp::Copy);

        // According to the reg-spec, DCC decompress ops imply fmask decompress and fast-clear eliminate operations as
        // well, so set these registers as they would be set above.
        m_regs.context.cbShaderMask.u32All = 0xF;
        m_regs.other.cbTargetMask.u32All   = 0xF;
    }
    else if (IsResolveFixedFunc())
    {
        m_regs.other.cbColorControl.bits.MODE = CB_RESOLVE__GFX09_10;
        m_regs.other.cbColorControl.bits.ROP3 = Rop3(LogicOp::Copy);

        m_regs.context.cbShaderMask.u32All = 0xF;
        m_regs.other.cbTargetMask.u32All   = 0xF;
    }
    else if ((m_regs.context.cbShaderMask.u32All == 0) || (m_regs.other.cbTargetMask.u32All == 0))
    {
        m_regs.other.cbColorControl.bits.MODE = CB_DISABLE;
    }
    else
    {
        m_regs.other.cbColorControl.bits.MODE = CB_NORMAL;
        m_regs.other.cbColorControl.bits.ROP3 = Rop3(createInfo.cbState.logicOp);
    }

    if (createInfo.cbState.dualSourceBlendEnable)
    {
        // If dual-source blending is enabled and the PS doesn't export to both RT0 and RT1, the hardware might hang.
        // To avoid the hang, just disable CB writes.
        if (((m_regs.context.cbShaderMask.u32All & 0x0F) == 0) ||
            ((m_regs.context.cbShaderMask.u32All & 0xF0) == 0))
        {
            PAL_ALERT_ALWAYS();
            m_regs.other.cbColorControl.bits.MODE = CB_DISABLE;
        }
    }

    // Copy RbPlus registers sets which compatible with dual source blend enable
    m_regs.other.sxPsDownconvertDual   = m_regs.other.sxPsDownconvert;
    m_regs.other.sxBlendOptEpsilonDual = m_regs.other.sxBlendOptEpsilon;
    m_regs.other.sxBlendOptControlDual = m_regs.other.sxBlendOptControl;

    // Initialize RB+ registers for pipelines which are able to use the feature.
    if (settings.gfx9RbPlusEnable &&
        (createInfo.cbState.dualSourceBlendEnable == false) &&
        (m_regs.other.cbColorControl.bits.MODE != CB_RESOLVE__GFX09_10))
    {
        PAL_ASSERT(chipProps.gfx9.rbPlus);

        m_regs.other.cbColorControl.bits.DISABLE_DUAL_QUAD = 0;

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
        m_regs.other.cbColorControl.bits.DISABLE_DUAL_QUAD = 1;
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
        m_regs.other.cbTargetMask.u32All   |= mask;
    }

    if (m_signature.uavExportTableAddr != UserDataNotMapped)
    {
        m_flags.uavExportRequiresFlush = (createInfo.cbState.uavExportSingleDraw == false);
    }

    // Override some register settings based on toss points.  These toss points cannot be processed in the hardware
    // independent class because they cannot be overridden by altering the pipeline creation info.
    if ((IsInternal() == false) && (m_pDevice->Parent()->Settings().tossPointMode == TossPointAfterPs))
    {
        // This toss point is used to disable all color buffer writes.
        m_regs.other.cbTargetMask.u32All = 0;
    }
}

// =====================================================================================================================
// Sets-up the late alloc limit. VS and GS will both use this function.
uint32 GraphicsPipeline::CalcMaxLateAllocLimit(
    const Device& device,
    uint32        vsNumVgpr,
    uint32        vsNumSgpr,
    uint32        vsWaveSize,
    bool          vsScratchEn,
    bool          psScratchEn,
    uint32        targetLateAllocLimit)
{
    const auto* pPalSettings = device.Parent()->GetPublicSettings();
    const auto& gfx9Settings = device.Settings();

    // Default to a late-alloc limit of zero.  This will nearly mimic the GFX6 behavior where VS waves don't launch
    // without allocating export space.
    uint32 lateAllocLimit = 0;

    // To keep this code equivalent to the previous code, we first transform these values into their
    // register counterparts, then perform the multiplication.

    vsNumVgpr = AbiRegisters::CalcNumVgprs(vsNumVgpr, (vsWaveSize == 32));
    vsNumSgpr = AbiRegisters::CalcNumSgprs(vsNumSgpr);

    vsNumVgpr *= 4;
    vsNumSgpr *= 8;

    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();

    // Maximum value of the LIMIT field of the SPI_SHADER_LATE_ALLOC_VS register
    // It is the number of wavefronts minus one.
    const uint32 maxLateAllocLimit = chipProps.gfxip.maxLateAllocVsLimit - 1;

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
        if (vsScratchEn && psScratchEn)
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
    const PalAbi::CodeObjectMetadata& metadata)
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

    ringSizes.itemSize[static_cast<size_t>(ShaderRingType::ComputeScratch)] =
        Gfx9::ComputePipeline::CalcScratchMemSize(m_gfxLevel, metadata);

    if (metadata.pipeline.hasEntry.meshScratchMemorySize != 0)
    {
        ringSizes.itemSize[static_cast<size_t>(ShaderRingType::MeshScratch)] = metadata.pipeline.meshScratchMemorySize;
    }

#if PAL_BUILD_GFX11
    ringSizes.itemSize[static_cast<size_t>(ShaderRingType::VertexAttributes)] =
        settings.gfx11VertexAttributesRingBufferSizePerSe;
#endif

    // Inform the device that this pipeline has some new ring-size requirements.
    m_pDevice->UpdateLargestRingSizes(&ringSizes);
}

// =====================================================================================================================
// Calculates the maximum scratch memory in dwords necessary by checking the scratch memory needed for each shader.
uint32 GraphicsPipeline::ComputeScratchMemorySize(
    const PalAbi::CodeObjectMetadata& metadata
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
        if (static_cast<Abi::HardwareStage>(i) == Abi::HardwareStage::Cs)
        {
            // We don't handle compute-scratch in this function.
            continue;
        }

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
        PAL_ASSERT(m_pDevice->Parent()->ChipProperties().gfxip.supportsHwVs);
        regBase = HasHwVs::mmSPI_SHADER_USER_DATA_VS_0;
    }

    return regBase;
}

// =====================================================================================================================
// Initializes the signature for a single stage within a graphics pipeline using a pipeline ELF.
void GraphicsPipeline::SetupSignatureForStageFromElf(
    const PalAbi::CodeObjectMetadata& metadata,
    HwShaderStage                     stage,
    uint16*                           pEsGsLdsSizeReg)
{
    const uint16 baseRegAddr = m_pDevice->GetBaseUserDataReg(stage);

    const uint32 stageId = static_cast<uint32>(stage);
    auto*const   pStage  = &m_signature.stage[stageId];
    auto* const  pRegMap = &metadata.pipeline.hardwareStage[uint32(PalToAbiHwShaderStage[stageId])].userDataRegMap[0];

    for (uint16 idx = 0; idx < 32; idx++)
    {
        const uint16 offset = baseRegAddr + idx;
        const uint32 value  = pRegMap[idx];
        if (value != uint32(Abi::UserDataMapping::NotMapped))
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
            else if (value == static_cast<uint32>(Abi::UserDataMapping::MeshTaskDispatchDims))
            {
                // There can only be one set of mesh dispatch user-SGPR per pipeline.
                PAL_ASSERT((m_signature.meshDispatchDimsRegAddr == offset) ||
                           (m_signature.meshDispatchDimsRegAddr == UserDataNotMapped));
                m_signature.meshDispatchDimsRegAddr = offset;
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::MeshTaskRingIndex))
            {
                // There can only be one set of mesh dispatch user-SGPR per pipeline.
                PAL_ASSERT((m_signature.meshRingIndexAddr == offset) ||
                           (m_signature.meshRingIndexAddr == UserDataNotMapped));
                m_signature.meshRingIndexAddr = offset;
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::MeshPipeStatsBuf))
            {
                // There can be only one pipeline stats query buffer per pipeline.
                PAL_ASSERT((m_signature.meshPipeStatsBufRegAddr == offset) ||
                           (m_signature.meshPipeStatsBufRegAddr == UserDataNotMapped));
                m_signature.meshPipeStatsBufRegAddr = offset;
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
#if PAL_BUILD_GFX11
            else if (value == static_cast<uint32>(Abi::UserDataMapping::StreamOutControlBuf))
            {
                // There can be only one instance of the streamoutCntlBufRegAddr per pipeline.
                PAL_ASSERT((m_signature.streamoutCntlBufRegAddr == offset) ||
                    (m_signature.streamoutCntlBufRegAddr == UserDataNotMapped));
                PAL_ASSERT(stage == HwShaderStage::Gs);

                m_signature.streamoutCntlBufRegAddr = offset;
            }
#endif
            else
            {
                // This appears to be an illegally-specified user-data register!
                PAL_NEVER_CALLED();
            }
        } // If value is mapped
    } // For each user-SGPR

    if ((stage == HwShaderStage::Gs) && (m_signature.nggCullingDataAddr == UserDataNotMapped))
    {
        if (metadata.pipeline.graphicsRegister.hasEntry.nggCullingDataReg)
        {
            m_signature.nggCullingDataAddr = metadata.pipeline.graphicsRegister.nggCullingDataReg;
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
    const PalAbi::CodeObjectMetadata& metadata,
    uint16*                           pEsGsLdsSizeRegGs,
    uint16*                           pEsGsLdsSizeRegVs)
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
        SetupSignatureForStageFromElf(metadata, HwShaderStage::Hs, nullptr);
    }
    if (IsGsEnabled() || IsNgg())
    {
        SetupSignatureForStageFromElf(metadata, HwShaderStage::Gs, pEsGsLdsSizeRegGs);
    }
    if (IsNgg() == false)
    {
        SetupSignatureForStageFromElf(metadata, HwShaderStage::Vs, pEsGsLdsSizeRegVs);
    }
    SetupSignatureForStageFromElf(metadata, HwShaderStage::Ps, nullptr);

    // Finally, compact the array of view ID register addresses
    // so that all of the mapped ones are at the front of the array.
    PackArray(m_signature.viewIdRegAddr, UserDataNotMapped);
}

// =====================================================================================================================
// Returns true if no color buffers and no PS UAVs and AlphaToCoverage is disabled.
bool GraphicsPipeline::CanRbPlusOptimizeDepthOnly() const
{
    return ((NumColorTargets() == 0) &&
            (m_regs.other.cbColorControl.bits.MODE == CB_DISABLE) &&
            (DbShaderControl().bits.ALPHA_TO_MASK_DISABLE == 1) &&
            (PsUsesUavs() == false) && (PsWritesUavs() == false));
}

// =====================================================================================================================
// Returns the SX "downconvert" format with respect to the channel format of the color buffer target.
// This method is for the RbPlus feature which is identical to the gfx8.1 implementation.
SX_DOWNCONVERT_FORMAT GraphicsPipeline::SxDownConvertFormat(
    SwizzledFormat swizzledFormat
    ) const
{
    SX_DOWNCONVERT_FORMAT sxDownConvertFormat = SX_RT_EXPORT_NO_CONVERSION;

    switch (swizzledFormat.format)
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
        sxDownConvertFormat =
            (swizzledFormat.swizzle.a == ChannelSwizzle::Y) ? SX_RT_EXPORT_16_16_AR : SX_RT_EXPORT_16_16_GR;
        break;
    case ChNumFormat::X32_Uint:
    case ChNumFormat::X32_Sint:
    case ChNumFormat::X32_Float:
        sxDownConvertFormat =
            (swizzledFormat.swizzle.a == ChannelSwizzle::X) ? SX_RT_EXPORT_32_A : SX_RT_EXPORT_32_R;
        break;
    case ChNumFormat::X9Y9Z9E5_Float:
        //  When doing 8 pixels per clock transfers (in RB+ mode) on a render target using the 999e5 format, the
        //  SX must convert the exported data to 999e5
        PAL_ASSERT(IsGfx103PlusExclusive(m_gfxLevel));

        sxDownConvertFormat = SX_RT_EXPORT_9_9_9_E5__GFX103PLUSEXCLUSIVE;
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
    case SX_RT_EXPORT_9_9_9_E5__GFX103PLUSEXCLUSIVE:
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
uint32 GraphicsPipeline::StrmoutVtxStrideDw(
    uint32 idx
    ) const
{
    uint32 strideDw = 0;

#if PAL_BUILD_GFX11
    if (m_pDevice->Parent()->ChipProperties().gfxip.supportsSwStrmout)
    {
        strideDw = m_strmoutVtxStride[idx];
    }
    else
#endif
    {
        strideDw = m_chunkVsPs.VgtStrmoutVtxStride(idx).u32All;
    }

    return strideDw;
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
        (m_regs.other.cbColorControl.bits.DISABLE_DUAL_QUAD == 0))
    {
        // This logic should not clash with the logic for optDepthOnlyExportRate.
        PAL_ASSERT((m_pDevice->Parent()->GetPublicSettings()->optDepthOnlyExportRate &&
                    CanRbPlusOptimizeDepthOnly()) == false);

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
void GraphicsPipeline::GetRbPlusRegisters(
    bool                     dualSourceBlendEnable,
    regSX_PS_DOWNCONVERT*    pSxPsDownconvert,
    regSX_BLEND_OPT_EPSILON* pSxBlendOptEpsilon,
    regSX_BLEND_OPT_CONTROL* pSxBlendOptControl
    ) const
{
    *pSxPsDownconvert   = dualSourceBlendEnable ? m_regs.other.sxPsDownconvertDual   : m_regs.other.sxPsDownconvert;
    *pSxBlendOptEpsilon = dualSourceBlendEnable ? m_regs.other.sxBlendOptEpsilonDual : m_regs.other.sxBlendOptEpsilon;
    *pSxBlendOptControl = dualSourceBlendEnable ? m_regs.other.sxBlendOptControlDual : m_regs.other.sxBlendOptControl;
}

// =====================================================================================================================
// Return if hardware stereo rendering is enabled.
bool GraphicsPipeline::HwStereoRenderingEnabled() const
{
    const auto&  device   = *(m_pDevice->Parent());
    uint32       enStereo = 0;

    if (m_gfxLevel == GfxIpLevel::GfxIp9)
    {
        if (IsVega12(device) || IsVega20(device))
        {
            enStereo = m_regs.context.paStereoCntl.vg12_Vg20.EN_STEREO;
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
    else if (IsVega12(palDevice) || IsVega20(palDevice))
    {
        vpIdOffset = m_regs.context.paStereoCntl.vg12_Vg20.VP_ID_OFFSET;
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

                if (IsVega12(device) || IsVega20(device))
                {
                    SetPaStereoCntl(rtSliceOffset, vpIdOffset, &m_regs.context.paStereoCntl.vg12_Vg20);
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

} // Gfx9
} // Pal
