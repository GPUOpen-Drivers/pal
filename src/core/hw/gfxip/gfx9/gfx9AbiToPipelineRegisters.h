/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "palPipelineAbi.h"
#include "g_palPipelineAbiMetadata.h"

namespace Pal
{
namespace Gfx9
{
namespace AbiRegisters
{

// =====================================================================================================================
static uint32 CalcNumVgprs(
    uint32 vgprCount,
    bool   isWave32)
{
    return (vgprCount == 0) ? 0 : ((vgprCount - 1) / ((isWave32) ? 8 : 4));
}

// =====================================================================================================================
static uint32 CalcNumSgprs(
    uint32 sgprCount)
{
    // HW register ranges from 1-128 SGPRs, in units of 8 SGPRs (minus 1 field).
    return ((sgprCount - 1) / 8);
}

// =====================================================================================================================
static uint32 VgtShaderStagesEn(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    GfxIpLevel                              gfxLevel)
{
    const auto& vgtShaderStagesEnMetadata = metadata.pipeline.graphicsRegister.vgtShaderStagesEn;

    const auto& hwHs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Hs)];
    const auto& hwGs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Gs)];
    const auto& hwVs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Vs)];
    const auto& hwPs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Ps)];

    VGT_SHADER_STAGES_EN vgtShaderStagesEn = {};
    vgtShaderStagesEn.bits.ES_EN = vgtShaderStagesEnMetadata.esStageEn;
    vgtShaderStagesEn.bits.GS_EN = vgtShaderStagesEnMetadata.flags.gsStageEn;
    vgtShaderStagesEn.bits.HS_EN = vgtShaderStagesEnMetadata.flags.hsStageEn;
    vgtShaderStagesEn.bits.LS_EN = vgtShaderStagesEnMetadata.flags.lsStageEn;
    vgtShaderStagesEn.bits.VS_EN = vgtShaderStagesEnMetadata.vsStageEn;

    vgtShaderStagesEn.bits.MAX_PRIMGRP_IN_WAVE = vgtShaderStagesEnMetadata.maxPrimgroupInWave;
    vgtShaderStagesEn.bits.ORDERED_ID_MODE     = vgtShaderStagesEnMetadata.flags.orderedIdMode;
    vgtShaderStagesEn.bits.PRIMGEN_EN          = vgtShaderStagesEnMetadata.flags.primgenEn;

    if (IsGfx091xPlus(*device.Parent()))
    {
        vgtShaderStagesEn.gfx09_1xPlus.GS_FAST_LAUNCH = vgtShaderStagesEnMetadata.gsFastLaunch;
    }

    if (IsGfx10Plus(gfxLevel))
    {
        vgtShaderStagesEn.gfx10Plus.DYNAMIC_HS = vgtShaderStagesEnMetadata.flags.dynamicHs;
        vgtShaderStagesEn.gfx10Plus.GS_W32_EN  = (hwGs.hasEntry.wavefrontSize && (hwGs.wavefrontSize == 32));
        vgtShaderStagesEn.gfx10Plus.HS_W32_EN  = (hwHs.hasEntry.wavefrontSize && (hwHs.wavefrontSize == 32));
        vgtShaderStagesEn.gfx10Plus.VS_W32_EN  = (hwVs.hasEntry.wavefrontSize && (hwVs.wavefrontSize == 32));
        vgtShaderStagesEn.gfx10Plus.NGG_WAVE_ID_EN = vgtShaderStagesEnMetadata.flags.nggWaveIdEn;
        vgtShaderStagesEn.gfx10Plus.PRIMGEN_PASSTHRU_EN = vgtShaderStagesEnMetadata.flags.primgenPassthruEn;
    }

#if  PAL_BUILD_GFX11
    if (IsGfx104Plus(gfxLevel))
    {
        vgtShaderStagesEn.gfx104Plus.PRIMGEN_PASSTHRU_NO_MSG = vgtShaderStagesEnMetadata.flags.primgenPassthruNoMsg;
    }
#endif

    return vgtShaderStagesEn.u32All;
}

// =====================================================================================================================
static uint32 PaClClipCntl(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    const GraphicsPipelineCreateInfo&       createInfo)
{
    const Util::PalAbi::PaClClipCntlMetadata& paClClipCntlMetadata = metadata.pipeline.graphicsRegister.paClClipCntl;

    PA_CL_CLIP_CNTL paClClipCntl = {};
    paClClipCntl.bits.UCP_ENA_0               = paClClipCntlMetadata.flags.userClipPlane0Ena;
    paClClipCntl.bits.UCP_ENA_1               = paClClipCntlMetadata.flags.userClipPlane1Ena;
    paClClipCntl.bits.UCP_ENA_2               = paClClipCntlMetadata.flags.userClipPlane2Ena;
    paClClipCntl.bits.UCP_ENA_3               = paClClipCntlMetadata.flags.userClipPlane3Ena;
    paClClipCntl.bits.UCP_ENA_4               = paClClipCntlMetadata.flags.userClipPlane4Ena;
    paClClipCntl.bits.UCP_ENA_5               = paClClipCntlMetadata.flags.userClipPlane5Ena;
    paClClipCntl.bits.DX_LINEAR_ATTR_CLIP_ENA = paClClipCntlMetadata.flags.dxLinearAttrClipEna;
    paClClipCntl.bits.ZCLIP_NEAR_DISABLE      = paClClipCntlMetadata.flags.zclipNearDisable;
    paClClipCntl.bits.ZCLIP_FAR_DISABLE       = paClClipCntlMetadata.flags.zclipFarDisable;
    paClClipCntl.bits.DX_RASTERIZATION_KILL   = paClClipCntlMetadata.flags.rasterizationKill;
    paClClipCntl.bits.CLIP_DISABLE            = paClClipCntlMetadata.flags.clipDisable;

    paClClipCntl.bits.DX_CLIP_SPACE_DEF =
        (createInfo.viewportInfo.depthRange == DepthRange::ZeroToOne);

    if (createInfo.viewportInfo.depthClipNearEnable == false)
    {
        paClClipCntl.bits.ZCLIP_NEAR_DISABLE = 1;
    }
    if (createInfo.viewportInfo.depthClipFarEnable == false)
    {
        paClClipCntl.bits.ZCLIP_FAR_DISABLE = 1;
    }
    if (static_cast<TossPointMode>(device.Parent()->Settings().tossPointMode) == TossPointAfterRaster)
    {
        paClClipCntl.bits.DX_RASTERIZATION_KILL = 1;
    }

    return paClClipCntl.u32All;
}

// =====================================================================================================================
static uint32 PaClVteCntl(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    const Util::PalAbi::PaClVteCntlMetadata& paClVteCntlMetadata = metadata.pipeline.graphicsRegister.paClVteCntl;

    PA_CL_VTE_CNTL paClVteCntl = {};
    paClVteCntl.bits.VTX_XY_FMT         = paClVteCntlMetadata.flags.vtxXyFmt;
    paClVteCntl.bits.VTX_Z_FMT          = paClVteCntlMetadata.flags.vtxZFmt;
    paClVteCntl.bits.VPORT_X_SCALE_ENA  = paClVteCntlMetadata.flags.xScaleEna;
    paClVteCntl.bits.VPORT_X_OFFSET_ENA = paClVteCntlMetadata.flags.xOffsetEna;
    paClVteCntl.bits.VPORT_Y_SCALE_ENA  = paClVteCntlMetadata.flags.yScaleEna;
    paClVteCntl.bits.VPORT_Y_OFFSET_ENA = paClVteCntlMetadata.flags.yOffsetEna;
    paClVteCntl.bits.VPORT_Z_SCALE_ENA  = paClVteCntlMetadata.flags.zScaleEna;
    paClVteCntl.bits.VPORT_Z_OFFSET_ENA = paClVteCntlMetadata.flags.zOffsetEna;
    paClVteCntl.bits.VTX_W0_FMT         = paClVteCntlMetadata.flags.vtxW0Fmt;

    return paClVteCntl.u32All;
}

// =====================================================================================================================
static uint32 PaScModeCntl1(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const GraphicsPipelineCreateInfo&       createInfo,
    const Device&                           device)
{
    const Gfx9PalSettings& settings = device.Settings();

    PA_SC_MODE_CNTL_1 paScModeCntl1 = {};
    paScModeCntl1.bits.WALK_ALIGN8_PRIM_FITS_ST                = 1;
    paScModeCntl1.bits.WALK_FENCE_ENABLE                       = 1;
    paScModeCntl1.bits.TILE_WALK_ORDER_ENABLE                  = 1;
    paScModeCntl1.bits.SUPERTILE_WALK_ORDER_ENABLE             = 1;
    paScModeCntl1.bits.MULTI_SHADER_ENGINE_PRIM_DISCARD_ENABLE = 1;
    paScModeCntl1.bits.FORCE_EOV_CNTDWN_ENABLE                 = 1;
    paScModeCntl1.bits.FORCE_EOV_REZ_ENABLE                    = 1;

    switch (createInfo.rsState.forcedShadingRate)
    {
    case PsShadingRate::SampleRate:
        paScModeCntl1.bits.PS_ITER_SAMPLE = 1;
        break;
    case PsShadingRate::PixelRate:
        paScModeCntl1.bits.PS_ITER_SAMPLE = 0;
        break;
    default:
        paScModeCntl1.bits.PS_ITER_SAMPLE = metadata.pipeline.graphicsRegister.flags.psIterSample;
        break;
    }

    // Overrides some of the fields in PA_SC_MODE_CNTL1 to account for GPU pipe config and features like
    // out-of-order rasterization.

    // The maximum value for OUT_OF_ORDER_WATER_MARK is 7
    constexpr uint32 MaxOutOfOrderWatermark = 7;
    paScModeCntl1.bits.OUT_OF_ORDER_WATER_MARK = Util::Min(MaxOutOfOrderWatermark, settings.outOfOrderWatermark);

    if (createInfo.rsState.outOfOrderPrimsEnable && (settings.enableOutOfOrderPrimitives != OutOfOrderPrimDisable))
    {
        paScModeCntl1.bits.OUT_OF_ORDER_PRIMITIVE_ENABLE = 1;
    }

    // Hardware team recommendation is to set WALK_FENCE_SIZE to 512 pixels for 4/8/16 pipes and 256 pixels
    // for 2 pipes.
    paScModeCntl1.bits.WALK_FENCE_SIZE = ((device.GetNumPipesLog2() <= 1) ? 2 : 3);

    return paScModeCntl1.u32All;
}

// =====================================================================================================================
static uint32 PaSuVtxCntl(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    const Util::PalAbi::PaSuVtxCntlMetadata& paSuVtxCntlMetadata = metadata.pipeline.graphicsRegister.paSuVtxCntl;

    PA_SU_VTX_CNTL paSuVtxCntl = {};
    paSuVtxCntl.bits.PIX_CENTER = paSuVtxCntlMetadata.flags.pixCenter;
    paSuVtxCntl.bits.ROUND_MODE = paSuVtxCntlMetadata.roundMode;
    paSuVtxCntl.bits.QUANT_MODE = paSuVtxCntlMetadata.quantMode;

    return paSuVtxCntl.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderIdxFormat(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    SPI_SHADER_IDX_FORMAT spiShaderIdxFormat = {};
    spiShaderIdxFormat.bits.IDX0_EXPORT_FORMAT = metadata.pipeline.graphicsRegister.spiShaderIdxFormat;

    return spiShaderIdxFormat.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderColFormat(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    const Util::PalAbi::SpiShaderColFormatMetadata& spiShaderColFormatMetadata =
        metadata.pipeline.graphicsRegister.spiShaderColFormat;

    SPI_SHADER_COL_FORMAT spiShaderColFormat = {};
    spiShaderColFormat.bits.COL0_EXPORT_FORMAT = spiShaderColFormatMetadata.col_0ExportFormat;
    spiShaderColFormat.bits.COL1_EXPORT_FORMAT = spiShaderColFormatMetadata.col_1ExportFormat;
    spiShaderColFormat.bits.COL2_EXPORT_FORMAT = spiShaderColFormatMetadata.col_2ExportFormat;
    spiShaderColFormat.bits.COL3_EXPORT_FORMAT = spiShaderColFormatMetadata.col_3ExportFormat;
    spiShaderColFormat.bits.COL4_EXPORT_FORMAT = spiShaderColFormatMetadata.col_4ExportFormat;
    spiShaderColFormat.bits.COL5_EXPORT_FORMAT = spiShaderColFormatMetadata.col_5ExportFormat;
    spiShaderColFormat.bits.COL6_EXPORT_FORMAT = spiShaderColFormatMetadata.col_6ExportFormat;
    spiShaderColFormat.bits.COL7_EXPORT_FORMAT = spiShaderColFormatMetadata.col_7ExportFormat;

    return spiShaderColFormat.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPosFormat(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    GfxIpLevel                              gfxLevel)
{
    const uint8* pSpiShaderPosFormat = &metadata.pipeline.graphicsRegister.spiShaderPosFormat[0];

    SPI_SHADER_POS_FORMAT spiShaderPosFormat = {};
    spiShaderPosFormat.bits.POS0_EXPORT_FORMAT = pSpiShaderPosFormat[0];
    spiShaderPosFormat.bits.POS1_EXPORT_FORMAT = pSpiShaderPosFormat[1];
    spiShaderPosFormat.bits.POS2_EXPORT_FORMAT = pSpiShaderPosFormat[2];
    spiShaderPosFormat.bits.POS3_EXPORT_FORMAT = pSpiShaderPosFormat[3];

    if (IsGfx10Plus(gfxLevel))
    {
        spiShaderPosFormat.gfx10Plus.POS4_EXPORT_FORMAT = pSpiShaderPosFormat[4];
    }

    return spiShaderPosFormat.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderZFormat(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    SPI_SHADER_Z_FORMAT spiShaderZFormat = {};
    spiShaderZFormat.bits.Z_EXPORT_FORMAT = metadata.pipeline.graphicsRegister.spiShaderZFormat;

    return spiShaderZFormat.u32All;
}

// =====================================================================================================================
static uint32 VgtGsMode(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    const Util::PalAbi::VgtGsModeMetadata& vgtGsModeMetadata = metadata.pipeline.graphicsRegister.vgtGsMode;

    VGT_GS_MODE vgtGsMode = {};
    vgtGsMode.bits.MODE              = vgtGsModeMetadata.mode;
    vgtGsMode.bits.CUT_MODE          = vgtGsModeMetadata.cutMode;
    vgtGsMode.bits.ONCHIP            = vgtGsModeMetadata.onchip;
    vgtGsMode.bits.ES_WRITE_OPTIMIZE = vgtGsModeMetadata.flags.esWriteOptimize;
    vgtGsMode.bits.GS_WRITE_OPTIMIZE = vgtGsModeMetadata.flags.gsWriteOptimize;

    return vgtGsMode.u32All;
}

// =====================================================================================================================
static uint32 VgtGsOnchipCntl(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    const Util::PalAbi::VgtGsOnchipCntlMetadata& vgtGsOnchipCntlMetadata =
        metadata.pipeline.graphicsRegister.vgtGsOnchipCntl;

    VGT_GS_ONCHIP_CNTL vgtGsOnchipCntl = {};
    vgtGsOnchipCntl.bits.ES_VERTS_PER_SUBGRP     = vgtGsOnchipCntlMetadata.esVertsPerSubgroup;
    vgtGsOnchipCntl.bits.GS_PRIMS_PER_SUBGRP     = vgtGsOnchipCntlMetadata.gsPrimsPerSubgroup;
    vgtGsOnchipCntl.bits.GS_INST_PRIMS_IN_SUBGRP = vgtGsOnchipCntlMetadata.gsInstPrimsPerSubgrp;

    return vgtGsOnchipCntl.u32All;
}

// =====================================================================================================================
static uint32 VgtReuseOff(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    VGT_REUSE_OFF vgtReuseOff = {};
    vgtReuseOff.bits.REUSE_OFF = metadata.pipeline.graphicsRegister.flags.vgtReuseOff;

    return vgtReuseOff.u32All;
}

// =====================================================================================================================
static uint32 SpiPsInControl(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    GfxIpLevel                              gfxLevel)
{
    const Util::PalAbi::SpiPsInControlMetadata& spiPsInControlMetadata =
        metadata.pipeline.graphicsRegister.spiPsInControl;

    SPI_PS_IN_CONTROL spiPsInControl = {};
    spiPsInControl.bits.NUM_INTERP          = spiPsInControlMetadata.numInterps;
    spiPsInControl.bits.PARAM_GEN           = spiPsInControlMetadata.flags.paramGen;
    spiPsInControl.bits.OFFCHIP_PARAM_EN    = spiPsInControlMetadata.flags.offchipParamEn;
    spiPsInControl.bits.LATE_PC_DEALLOC     = spiPsInControlMetadata.flags.latePcDealloc;
    spiPsInControl.bits.BC_OPTIMIZE_DISABLE = spiPsInControlMetadata.flags.bcOptimizeDisable;

    if (IsGfx10Plus(gfxLevel))
    {
        spiPsInControl.gfx10Plus.PS_W32_EN =
            (metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Ps)].wavefrontSize == 32);
    }

    if (IsGfx103PlusExclusive(gfxLevel))
    {
        spiPsInControl.gfx103PlusExclusive.NUM_PRIM_INTERP = spiPsInControlMetadata.numPrimInterp;
    }

    return spiPsInControl.u32All;
}

// =====================================================================================================================
static uint32 SpiVsOutConfig(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    GfxIpLevel                              gfxLevel)
{
    const Gfx9PalSettings& settings = device.Settings();

    const Util::PalAbi::SpiVsOutConfigMetadata& spiVsOutConfigMetadata =
        metadata.pipeline.graphicsRegister.spiVsOutConfig;

    SPI_VS_OUT_CONFIG spiVsOutConfig = {};
    spiVsOutConfig.bits.VS_EXPORT_COUNT = spiVsOutConfigMetadata.vsExportCount;

    if (IsGfx10Plus(gfxLevel))
    {
        spiVsOutConfig.gfx10Plus.NO_PC_EXPORT = spiVsOutConfigMetadata.flags.noPcExport;
    }

    if (IsGfx103PlusExclusive(gfxLevel))
    {
        spiVsOutConfig.gfx103PlusExclusive.PRIM_EXPORT_COUNT = spiVsOutConfigMetadata.primExportCount;
    }

    // If the number of VS output semantics exceeds the half-pack threshold, then enable VS half-pack mode.  Keep in
    // mind that the number of VS exports are represented by a -1 field in the HW register!
    if ((spiVsOutConfig.bits.VS_EXPORT_COUNT + 1u) > settings.vsHalfPackThreshold)
    {
        spiVsOutConfig.gfx09_10.VS_HALF_PACK = 1;
    }

    return spiVsOutConfig.u32All;
}

// =====================================================================================================================
static uint32 VgtTfParam(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    GfxIpLevel                              gfxLevel)
{
    const Util::PalAbi::VgtTfParamMetadata& vgtTfParamMetadata = metadata.pipeline.graphicsRegister.vgtTfParam;

    VGT_TF_PARAM vgtTfParam = {};

    // If the type isn't specified, then we don't care.
    if (vgtTfParamMetadata.hasEntry.type)
    {
        vgtTfParam.bits.TYPE              = vgtTfParamMetadata.type;
        vgtTfParam.bits.PARTITIONING      = vgtTfParamMetadata.partitioning;
        vgtTfParam.bits.TOPOLOGY          = vgtTfParamMetadata.topology;
        vgtTfParam.bits.DISABLE_DONUTS    = vgtTfParamMetadata.flags.disableDonuts;
        vgtTfParam.bits.DISTRIBUTION_MODE = vgtTfParamMetadata.distributionMode;

        if (IsGfx10Plus(gfxLevel))
        {
            vgtTfParam.gfx10Plus.NUM_DS_WAVES_PER_SIMD = vgtTfParamMetadata.numDsWavesPerSimd;
        }
    }

    return vgtTfParam.u32All;
}

// =====================================================================================================================
static uint32 VgtLsHsConfig(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    const Util::PalAbi::VgtLsHsConfigMetadata& vgtLsHsConfigMetadata = metadata.pipeline.graphicsRegister.vgtLsHsConfig;

    VGT_LS_HS_CONFIG vgtLsHsConfig = {};
    vgtLsHsConfig.bits.NUM_PATCHES      = vgtLsHsConfigMetadata.numPatches;
    vgtLsHsConfig.bits.HS_NUM_INPUT_CP  = vgtLsHsConfigMetadata.hsNumInputCp;
    vgtLsHsConfig.bits.HS_NUM_OUTPUT_CP = vgtLsHsConfigMetadata.hsNumOutputCp;

    return vgtLsHsConfig.u32All;
}

// =====================================================================================================================
static uint32 SpiInterpControl0(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const GraphicsPipelineCreateInfo&       createInfo)
{
    const Util::PalAbi::SpiInterpControlMetadata& spiInterpControlMetadata =
        metadata.pipeline.graphicsRegister.spiInterpControl;

    SPI_INTERP_CONTROL_0 spiInterpControl0 = {};
    spiInterpControl0.bits.PNT_SPRITE_ENA    = spiInterpControlMetadata.flags.pointSpriteEna;
    spiInterpControl0.bits.PNT_SPRITE_OVRD_X = uint32(spiInterpControlMetadata.pointSpriteOverrideX);
    spiInterpControl0.bits.PNT_SPRITE_OVRD_Y = uint32(spiInterpControlMetadata.pointSpriteOverrideY);
    spiInterpControl0.bits.PNT_SPRITE_OVRD_Z = uint32(spiInterpControlMetadata.pointSpriteOverrideZ);
    spiInterpControl0.bits.PNT_SPRITE_OVRD_W = uint32(spiInterpControlMetadata.pointSpriteOverrideW);

    spiInterpControl0.bits.FLAT_SHADE_ENA = (createInfo.rsState.shadeMode == ShadeMode::Flat);
    if (spiInterpControl0.bits.PNT_SPRITE_ENA != 0) // Point sprite mode is enabled.
    {
        spiInterpControl0.bits.PNT_SPRITE_TOP_1 = (createInfo.rsState.pointCoordOrigin != PointOrigin::UpperLeft);
    }

    return spiInterpControl0.u32All;
}

// =====================================================================================================================
static uint32 VgtDrawPayloadCntl(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    GfxIpLevel                              gfxLevel)
{
    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();

    VGT_DRAW_PAYLOAD_CNTL vgtDrawPayloadCntl = {};

    if (IsGfx10Plus(gfxLevel))
    {
        vgtDrawPayloadCntl.gfx10Plus.EN_PRIM_PAYLOAD = metadata.pipeline.graphicsRegister.flags.vgtDrawPrimPayloadEn;
    }

    if (chipProps.gfxip.supportsVrs)
    {
        // Enable draw call VRS rate from GE_VRS_RATE.
        //    00 - Suppress draw VRS rates
        //    01 - Send draw VRS rates to the PA
        vgtDrawPayloadCntl.gfx103Plus.EN_VRS_RATE = 1;
    }

    return vgtDrawPayloadCntl.u32All;
}

// =====================================================================================================================
static uint32 CbShaderMask(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    const Util::PalAbi::CbShaderMaskMetadata& cbShaderMaskMetadata = metadata.pipeline.graphicsRegister.cbShaderMask;

    CB_SHADER_MASK cbShaderMask = {};
    cbShaderMask.bits.OUTPUT0_ENABLE = cbShaderMaskMetadata.output0Enable;
    cbShaderMask.bits.OUTPUT1_ENABLE = cbShaderMaskMetadata.output1Enable;
    cbShaderMask.bits.OUTPUT2_ENABLE = cbShaderMaskMetadata.output2Enable;
    cbShaderMask.bits.OUTPUT3_ENABLE = cbShaderMaskMetadata.output3Enable;
    cbShaderMask.bits.OUTPUT4_ENABLE = cbShaderMaskMetadata.output4Enable;
    cbShaderMask.bits.OUTPUT5_ENABLE = cbShaderMaskMetadata.output5Enable;
    cbShaderMask.bits.OUTPUT6_ENABLE = cbShaderMaskMetadata.output6Enable;
    cbShaderMask.bits.OUTPUT7_ENABLE = cbShaderMaskMetadata.output7Enable;

    return cbShaderMask.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmRsrc1Gs(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    GfxIpLevel                              gfxLevel)
{
    const Gfx9PalSettings& settings = device.Settings();
    const auto&            hwGs     = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Gs)];

    SPI_SHADER_PGM_RSRC1_GS spiShaderPgmRsrc1Gs = {};

    spiShaderPgmRsrc1Gs.bits.VGPRS            = CalcNumVgprs(hwGs.vgprCount, (hwGs.wavefrontSize == 32));
    spiShaderPgmRsrc1Gs.bits.SGPRS            = CalcNumSgprs(hwGs.sgprCount);
    spiShaderPgmRsrc1Gs.bits.FLOAT_MODE       = hwGs.floatMode;
    spiShaderPgmRsrc1Gs.bits.DX10_CLAMP       = 1;
    spiShaderPgmRsrc1Gs.bits.DEBUG_MODE       = hwGs.flags.debugMode;
    spiShaderPgmRsrc1Gs.bits.IEEE_MODE        = hwGs.flags.ieeeMode;
    spiShaderPgmRsrc1Gs.bits.GS_VGPR_COMP_CNT = metadata.pipeline.graphicsRegister.gsVgprCompCnt;
    spiShaderPgmRsrc1Gs.bits.FP16_OVFL        = hwGs.flags.fp16Overflow;

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_ENABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    spiShaderPgmRsrc1Gs.bits.CU_GROUP_ENABLE = (settings.gsCuGroupEnabled ? 1 : 0);

    if (IsGfx10Plus(gfxLevel))
    {
        spiShaderPgmRsrc1Gs.gfx10Plus.MEM_ORDERED  = hwGs.flags.memOrdered;
        spiShaderPgmRsrc1Gs.gfx10Plus.FWD_PROGRESS = hwGs.flags.forwardProgress;
        spiShaderPgmRsrc1Gs.gfx10Plus.WGP_MODE     = hwGs.flags.wgpMode;
    }

    return spiShaderPgmRsrc1Gs.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmRsrc2Gs(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    GfxIpLevel                              gfxLevel)
{
    const auto& hwGs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Gs)];

    SPI_SHADER_PGM_RSRC2_GS spiShaderPgmRsrc2Gs = {};
    spiShaderPgmRsrc2Gs.bits.SCRATCH_EN       = hwGs.flags.scratchEn;
    spiShaderPgmRsrc2Gs.bits.USER_SGPR        = hwGs.userSgprs;
    spiShaderPgmRsrc2Gs.bits.TRAP_PRESENT     = hwGs.flags.trapPresent;
    spiShaderPgmRsrc2Gs.bits.EXCP_EN          = hwGs.excpEn;
    spiShaderPgmRsrc2Gs.bits.ES_VGPR_COMP_CNT = metadata.pipeline.graphicsRegister.esVgprCompCnt;
    spiShaderPgmRsrc2Gs.bits.OC_LDS_EN        = hwGs.flags.offchipLdsEn;
    spiShaderPgmRsrc2Gs.bits.LDS_SIZE         =
        Util::Pow2Align(hwGs.ldsSize / uint32(sizeof(uint32)), Gfx9LdsDwGranularity) >> Gfx9LdsDwGranularityShift;

    if (IsGfx9(gfxLevel))
    {
        spiShaderPgmRsrc2Gs.gfx09.USER_SGPR_MSB = (hwGs.userSgprs >= 32);
    }
    else if (IsGfx10Plus(gfxLevel))
    {
        spiShaderPgmRsrc2Gs.gfx10Plus.USER_SGPR_MSB   = (hwGs.userSgprs >= 32);
        spiShaderPgmRsrc2Gs.gfx10Plus.SHARED_VGPR_CNT = hwGs.sharedVgprCnt;
    }

    return spiShaderPgmRsrc2Gs.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmRsrc3Gs(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    GfxIpLevel                              gfxLevel,
    bool                                    nggEnabled,
    bool                                    usesOnChipGs)
{
    const Gfx9PalSettings&   settings  = device.Settings();
    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();
    const auto&              hwGs      = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Gs)];

    SPI_SHADER_PGM_RSRC3_GS spiShaderPgmRsrc3Gs = {};

    if (IsGfx9(gfxLevel))
    {
        const uint32 numSaPerSe = chipProps.gfx9.numShaderArrays;
        spiShaderPgmRsrc3Gs.bits.WAVE_LIMIT = hwGs.wavesPerSe / numSaPerSe;
    }
    else if (IsGfx10Plus(chipProps.gfxLevel))
    {
        spiShaderPgmRsrc3Gs.bits.WAVE_LIMIT = hwGs.wavesPerSe;
    }

    // If late-alloc for NGG is enabled, or if we're using on-chip legacy GS path, we need to avoid using CU1
    // for GS waves to avoid a deadlock with the PS. It is impossible to fully disable LateAlloc on Gfx9+, even
    // with LateAlloc = 0.
    // There are two issues:
    //    1. NGG:
    //       The HW-GS can perform exports which require parameter cache space. There are pending PS waves who have
    //       claims on parameter cache space (before the interpolants are moved to LDS). This can cause a deadlock
    //       where the HW-GS waves are waiting for space in the cache, but that space is claimed by pending PS waves
    //       that can't launch on the CU due to lack of space (already existing waves).
    //    2. On-chip legacy GS:
    //       When on-chip is enabled, the HW-VS must run on the same CU as the HW-GS, since all communication
    //       between the waves are done via LDS. This means that wherever the HW-GS launches is where the HW-VS
    //       (copy shader) will launch. Due to the same issues as above (HW-VS waiting for parameter cache space,
    //       pending PS waves), this could also cause a deadlock.
    uint16 gsCuDisableMask = 0;
    if (nggEnabled || usesOnChipGs)
    {
        // It is possible, with an NGG shader, that late-alloc GS waves can deadlock the PS.  To prevent this hang
        // situation, we need to mask off one CU when NGG is enabled.
        if (IsGfx101(chipProps.gfxLevel))
        {
            // Both CU's of a WGP need to be disabled for better performance.
            gsCuDisableMask = 0xC;
        }
        else
        {
            // Disable virtualized CU #1 instead of #0 because thread traces use CU #0 by default.
            gsCuDisableMask = 0x2;
        }

        if ((nggEnabled) && (settings.allowNggOnAllCusWgps))
        {
            gsCuDisableMask = 0x0;
        }
    }

    spiShaderPgmRsrc3Gs.bits.CU_EN = device.GetCuEnableMask(gsCuDisableMask, settings.gsCuEnLimitMask);

#if PAL_BUILD_GFX11
    if (settings.waForceLockThresholdZero)
    {
        spiShaderPgmRsrc3Gs.bits.LOCK_LOW_THRESHOLD = 0;
    }
#endif

    return spiShaderPgmRsrc3Gs.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmRsrc4Gs(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    GfxIpLevel                              gfxLevel,
    bool                                    nggEnabled,
    size_t                                  codeLength,
    const GraphicsPipelineCreateInfo&       createInfo)
{
    const Gfx9PalSettings&   settings        = device.Settings();
    const PalPublicSettings* pPublicSettings = device.Parent()->GetPublicSettings();
    const auto&              hwGs            = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Gs)];
    const auto&              hwPs            = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Ps)];

    SPI_SHADER_PGM_RSRC4_GS spiShaderPgmRsrc4Gs = {};

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 781
    uint32 nggLateAllocWaves = (createInfo.useLateAllocGsLimit) ? createInfo.lateAllocGsLimit :
                                                                  pPublicSettings->nggLateAllocGs;
#else
    uint32 nggLateAllocWaves = pPublicSettings->nggLateAllocGs;
#endif
    uint32 lateAllocWaves    = (nggEnabled) ? nggLateAllocWaves : settings.lateAllocGs;
    uint32 lateAllocLimit    = 127;

    if (nggEnabled == false)
    {
        lateAllocLimit = GraphicsPipeline::CalcMaxLateAllocLimit(device,
                                                                 hwGs.vgprCount,
                                                                 hwGs.sgprCount,
                                                                 hwGs.wavefrontSize,
                                                                 hwGs.flags.scratchEn,
                                                                 hwPs.flags.scratchEn,
                                                                 lateAllocWaves);
    }
    else if (IsGfx10Plus(gfxLevel))
    {
        if (metadata.pipeline.graphicsRegister.vgtShaderStagesEn.flags.primgenEn && settings.waLimitLateAllocGsNggFifo)
        {
            lateAllocLimit = 64;
        }
    }

    lateAllocWaves = Util::Min(lateAllocWaves, lateAllocLimit);

    if (gfxLevel == GfxIpLevel::GfxIp9)
    {
        spiShaderPgmRsrc4Gs.gfx09.SPI_SHADER_LATE_ALLOC_GS = lateAllocWaves;
    }
    else // Gfx10+
    {
        // Note that SPI_SHADER_PGM_RSRC4_GS has a totally different layout on Gfx10+ vs. Gfx9!
        spiShaderPgmRsrc4Gs.gfx10Plus.SPI_SHADER_LATE_ALLOC_GS = lateAllocWaves;

        constexpr uint16 GsCuDisableMaskHi = 0;

        if (IsGfx10(gfxLevel))
        {
            spiShaderPgmRsrc4Gs.gfx10.CU_EN = device.GetCuEnableMaskHi(GsCuDisableMaskHi, settings.gsCuEnLimitMask);
        }
#if PAL_BUILD_GFX11
        else
        {
            spiShaderPgmRsrc4Gs.gfx11.CU_EN           = 0;
            spiShaderPgmRsrc4Gs.gfx11.PH_THROTTLE_EN  =
                Util::TestAnyFlagSet(settings.rsrc4GsThrottleEn, Rsrc4GsThrottlePhEn);
            spiShaderPgmRsrc4Gs.gfx11.SPI_THROTTLE_EN =
                Util::TestAnyFlagSet(settings.rsrc4GsThrottleEn, Rsrc4GsThrottleSpiEn);

            // PWS+ only support pre-shader waits if the IMAGE_OP bit is set. Theoretically we only set it for
            // shaders that do an image operation. However that would mean that our use of the pre-shader PWS+ wait
            // is dependent on us only waiting on image resources, which we don't know in our interface. For now
            // always set the IMAGE_OP bit for corresponding shaders, making the pre-shader waits global.
            spiShaderPgmRsrc4Gs.gfx11.IMAGE_OP = 1;
        }
#endif
    }

#if  PAL_BUILD_GFX11
    if (IsGfx104Plus(gfxLevel))
    {
        spiShaderPgmRsrc4Gs.gfx104Plus.INST_PREF_SIZE = device.GetShaderPrefetchSize(codeLength);
    }
#endif

    return spiShaderPgmRsrc4Gs.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmChksumGs(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device)
{
    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();
    const auto&              hwGs      = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Gs)];

    SPI_SHADER_PGM_CHKSUM_GS spiShaderPgmChksumGs = {};
    if (chipProps.gfx9.supportSpp != 0)
    {
        spiShaderPgmChksumGs.bits.CHECKSUM = hwGs.checksumValue;
    }

    return spiShaderPgmChksumGs.u32All;
}

#if PAL_BUILD_GFX11
// =====================================================================================================================
static uint32 SpiShaderGsMeshletDim(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    const Util::PalAbi::SpiShaderGsMeshletDimMetadata& spiShaderGsMeshletDimMetadata =
        metadata.pipeline.graphicsRegister.spiShaderGsMeshletDim;

    SPI_SHADER_GS_MESHLET_DIM spiShaderGsMeshletDim = {};
    spiShaderGsMeshletDim.bits.MESHLET_NUM_THREAD_X     = spiShaderGsMeshletDimMetadata.numThreadX;
    spiShaderGsMeshletDim.bits.MESHLET_NUM_THREAD_Y     = spiShaderGsMeshletDimMetadata.numThreadY;
    spiShaderGsMeshletDim.bits.MESHLET_NUM_THREAD_Z     = spiShaderGsMeshletDimMetadata.numThreadZ;
    spiShaderGsMeshletDim.bits.MESHLET_THREADGROUP_SIZE = spiShaderGsMeshletDimMetadata.threadgroupSize;

    return spiShaderGsMeshletDim.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderGsMeshletExpAlloc(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    const Util::PalAbi::SpiShaderGsMeshletExpAllocMetadata& spiShaderGsMeshletExpAllocMetadata =
        metadata.pipeline.graphicsRegister.spiShaderGsMeshletExpAlloc;

    SPI_SHADER_GS_MESHLET_EXP_ALLOC spiShaderGsMeshletExpAlloc = {};
    spiShaderGsMeshletExpAlloc.bits.MAX_EXP_VERTS = spiShaderGsMeshletExpAllocMetadata.maxExpVerts;
    spiShaderGsMeshletExpAlloc.bits.MAX_EXP_PRIMS = spiShaderGsMeshletExpAllocMetadata.maxExpPrims;

    return spiShaderGsMeshletExpAlloc.u32All;
}
#endif

// =====================================================================================================================
static uint32 VgtGsInstanceCnt(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    GfxIpLevel                              gfxLevel)
{
    const Util::PalAbi::VgtGsInstanceCntMetadata& vgtGsInstanceCntMetadata =
        metadata.pipeline.graphicsRegister.vgtGsInstanceCnt;

    VGT_GS_INSTANCE_CNT vgtGsInstanceCnt = {};
    vgtGsInstanceCnt.bits.ENABLE = vgtGsInstanceCntMetadata.flags.enable;
    vgtGsInstanceCnt.bits.CNT    = vgtGsInstanceCntMetadata.count;

    if (IsGfx10Plus(gfxLevel))
    {
        vgtGsInstanceCnt.gfx10Plus.EN_MAX_VERT_OUT_PER_GS_INSTANCE =
            vgtGsInstanceCntMetadata.flags.enMaxVertOutPerGsInstance;
    }

    return vgtGsInstanceCnt.u32All;
}

// =====================================================================================================================
static uint32 VgtGsOutPrimType(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    GfxIpLevel                              gfxLevel)
{
    const Util::PalAbi::VgtGsOutPrimTypeMetadata& vgtGsOutPrimTypeMetadata =
        metadata.pipeline.graphicsRegister.vgtGsOutPrimType;

    VGT_GS_OUT_PRIM_TYPE vgtGsOutPrimType = {};
    if (IsGfx9(gfxLevel))
    {
        vgtGsOutPrimType.bits.OUTPRIM_TYPE =
            (vgtGsOutPrimTypeMetadata.outprimType == Util::Abi::GsOutPrimType::RectList)
            ? VGT_GS_OUTPRIM_TYPE::RECTLIST__GFX09
            : uint32(vgtGsOutPrimTypeMetadata.outprimType);
        vgtGsOutPrimType.gfx09_10.OUTPRIM_TYPE_1 =
            (vgtGsOutPrimTypeMetadata.outprimType_1 == Util::Abi::GsOutPrimType::RectList)
            ? VGT_GS_OUTPRIM_TYPE::RECTLIST__GFX09
            : uint32(vgtGsOutPrimTypeMetadata.outprimType_1);
        vgtGsOutPrimType.gfx09_10.OUTPRIM_TYPE_2 =
            (vgtGsOutPrimTypeMetadata.outprimType_2 == Util::Abi::GsOutPrimType::RectList)
            ? VGT_GS_OUTPRIM_TYPE::RECTLIST__GFX09
            : uint32(vgtGsOutPrimTypeMetadata.outprimType_2);
        vgtGsOutPrimType.gfx09_10.OUTPRIM_TYPE_3 =
            (vgtGsOutPrimTypeMetadata.outprimType_3 == Util::Abi::GsOutPrimType::RectList)
            ? VGT_GS_OUTPRIM_TYPE::RECTLIST__GFX09
            : uint32(vgtGsOutPrimTypeMetadata.outprimType_3);

        vgtGsOutPrimType.gfx09_10.UNIQUE_TYPE_PER_STREAM =
            vgtGsOutPrimTypeMetadata.flags.uniqueTypePerStream;
    }
    else
    {
        static_assert((uint32(Util::Abi::GsOutPrimType::PointList) == VGT_GS_OUTPRIM_TYPE::POINTLIST)           &&
                      (uint32(Util::Abi::GsOutPrimType::LineStrip) == VGT_GS_OUTPRIM_TYPE::LINESTRIP)           &&
                      (uint32(Util::Abi::GsOutPrimType::TriStrip)  == VGT_GS_OUTPRIM_TYPE::TRISTRIP)            &&
                      (uint32(Util::Abi::GsOutPrimType::RectList)  == VGT_GS_OUTPRIM_TYPE::RECTLIST__GFX10PLUS) &&
                      (uint32(Util::Abi::GsOutPrimType::Rect2d)    == VGT_GS_OUTPRIM_TYPE::RECT_2D__GFX10PLUS),
                      "Abi::GsOutPrimType does not match HW version!");
        vgtGsOutPrimType.bits.OUTPRIM_TYPE = uint32(vgtGsOutPrimTypeMetadata.outprimType);

        if (IsGfx10(gfxLevel))
        {
            vgtGsOutPrimType.gfx09_10.OUTPRIM_TYPE_1         = uint32(vgtGsOutPrimTypeMetadata.outprimType_1);
            vgtGsOutPrimType.gfx09_10.OUTPRIM_TYPE_2         = uint32(vgtGsOutPrimTypeMetadata.outprimType_2);
            vgtGsOutPrimType.gfx09_10.OUTPRIM_TYPE_3         = uint32(vgtGsOutPrimTypeMetadata.outprimType_3);
            vgtGsOutPrimType.gfx09_10.UNIQUE_TYPE_PER_STREAM = vgtGsOutPrimTypeMetadata.flags.uniqueTypePerStream;
        }
    }

    return vgtGsOutPrimType.u32All;
}

// =====================================================================================================================
static uint32 VgtGsPerVs(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    bool*                                   pAllHere)
{
    VGT_GS_PER_VS vgtGsPerVs = {};
    vgtGsPerVs.bits.GS_PER_VS = metadata.pipeline.graphicsRegister.vgtGsPerVs;
    *pAllHere &= (metadata.pipeline.graphicsRegister.hasEntry.vgtGsPerVs != 0);

    return vgtGsPerVs.u32All;
}

// =====================================================================================================================
static void VgtGsVertItemsizes(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    VGT_GS_VERT_ITEMSIZE*                   pSize0,
    VGT_GS_VERT_ITEMSIZE_1*                 pSize1,
    VGT_GS_VERT_ITEMSIZE_2*                 pSize2,
    VGT_GS_VERT_ITEMSIZE_3*                 pSize3,
    bool*                                   pAllHere)
{
    pSize0->bits.ITEMSIZE = metadata.pipeline.graphicsRegister.vgtGsVertItemsize[0];
    pSize1->bits.ITEMSIZE = metadata.pipeline.graphicsRegister.vgtGsVertItemsize[1];
    pSize2->bits.ITEMSIZE = metadata.pipeline.graphicsRegister.vgtGsVertItemsize[2];
    pSize3->bits.ITEMSIZE = metadata.pipeline.graphicsRegister.vgtGsVertItemsize[3];
    *pAllHere &= metadata.pipeline.graphicsRegister.hasEntry.vgtGsVertItemsize;
}

// =====================================================================================================================
static uint32 VgtGsVsRingItemsize(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    bool*                                   pAllHere)
{
    VGT_GSVS_RING_ITEMSIZE vgtGsVsRingItemSize = {};
    vgtGsVsRingItemSize.bits.ITEMSIZE = metadata.pipeline.graphicsRegister.vgtGsvsRingItemsize;
    *pAllHere &= metadata.pipeline.graphicsRegister.hasEntry.vgtGsvsRingItemsize;

    return vgtGsVsRingItemSize.u32All;
}

// =====================================================================================================================
static void VgtGsVsRingOffsets(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    VGT_GSVS_RING_OFFSET_1*                 pOffset1,
    VGT_GSVS_RING_OFFSET_2*                 pOffset2,
    VGT_GSVS_RING_OFFSET_3*                 pOffset3,
    bool*                                   pAllHere)
{
    pOffset1->bits.OFFSET = metadata.pipeline.graphicsRegister.vgtGsvsRingOffset[0];
    pOffset2->bits.OFFSET = metadata.pipeline.graphicsRegister.vgtGsvsRingOffset[1];
    pOffset3->bits.OFFSET = metadata.pipeline.graphicsRegister.vgtGsvsRingOffset[2];
    *pAllHere &= metadata.pipeline.graphicsRegister.hasEntry.vgtGsvsRingOffset;
}

// =====================================================================================================================
static uint32 VgtEsGsRingItemSize(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    VGT_ESGS_RING_ITEMSIZE vgtEsGsRingItemSize = {};
    vgtEsGsRingItemSize.bits.ITEMSIZE = metadata.pipeline.graphicsRegister.vgtEsgsRingItemsize;

    return vgtEsGsRingItemSize.u32All;
}

// =====================================================================================================================
static uint32 VgtGsMaxVertOut(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    VGT_GS_MAX_VERT_OUT vgtGsMaxVertOut = {};
    vgtGsMaxVertOut.bits.MAX_VERT_OUT = metadata.pipeline.graphicsRegister.vgtGsMaxVertOut;

    return vgtGsMaxVertOut.u32All;
}

// =====================================================================================================================
static void GeMaxOutputPerSubgroup(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    VGT_GS_MAX_PRIMS_PER_SUBGROUP*          pVgtGsMaxPrimsPerSubgroup,
    GE_MAX_OUTPUT_PER_SUBGROUP*             pGeMaxOutputPerSubgroup,
    GfxIpLevel                              gfxLevel)
{
    if (gfxLevel == GfxIpLevel::GfxIp9)
    {
        // While this is called MAX_PRIMS, it really is a calculation of the maximum number of verts per subgroup.
        pVgtGsMaxPrimsPerSubgroup->bits.MAX_PRIMS_PER_SUBGROUP = metadata.pipeline.graphicsRegister.maxVertsPerSubgroup;
    }
    else
    {
        pGeMaxOutputPerSubgroup->bits.MAX_VERTS_PER_SUBGROUP = metadata.pipeline.graphicsRegister.maxVertsPerSubgroup;

    }
}

// =====================================================================================================================
static uint32 GeNggSubgrpCntl(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    const Util::PalAbi::GeNggSubgrpCntlMetadata& geNggSubgrpCntlMetadata =
        metadata.pipeline.graphicsRegister.geNggSubgrpCntl;

    GE_NGG_SUBGRP_CNTL geNggSubgrpCntl = {};
    geNggSubgrpCntl.bits.PRIM_AMP_FACTOR = geNggSubgrpCntlMetadata.primAmpFactor;
    geNggSubgrpCntl.bits.THDS_PER_SUBGRP = geNggSubgrpCntlMetadata.threadsPerSubgroup;

    return geNggSubgrpCntl.u32All;
}

// =====================================================================================================================
static uint32 PaClNggCntl(
    const GraphicsPipelineCreateInfo& createInfo,
    GfxIpLevel                        gfxLevel)
{
    PA_CL_NGG_CNTL paClNggCntl = {};

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 709
    paClNggCntl.bits.INDEX_BUF_EDGE_FLAG_ENA =
        (createInfo.iaState.topologyInfo.topologyIsPolygon ||
         (createInfo.iaState.topologyInfo.primitiveType == Pal::PrimitiveType::Quad));
#else
    paClNggCntl.bits.INDEX_BUF_EDGE_FLAG_ENA =
        (createInfo.iaState.topologyInfo.primitiveType == Pal::PrimitiveType::Quad);
#endif

    if (IsGfx103PlusExclusive(gfxLevel))
    {
        paClNggCntl.gfx103PlusExclusive.VERTEX_REUSE_DEPTH = 30;
    }

    return paClNggCntl.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmRsrc1Hs(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    GfxIpLevel                              gfxLevel)
{
    const auto& hwHs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Hs)];

    SPI_SHADER_PGM_RSRC1_HS spiShaderPgmRsrc1Hs = {};
    spiShaderPgmRsrc1Hs.bits.VGPRS            = CalcNumVgprs(hwHs.vgprCount, (hwHs.wavefrontSize == 32));
    spiShaderPgmRsrc1Hs.bits.SGPRS            = CalcNumSgprs(hwHs.sgprCount);
    spiShaderPgmRsrc1Hs.bits.FLOAT_MODE       = hwHs.floatMode;
    spiShaderPgmRsrc1Hs.bits.DX10_CLAMP       = 1;
    spiShaderPgmRsrc1Hs.bits.DEBUG_MODE       = hwHs.flags.debugMode;
    spiShaderPgmRsrc1Hs.bits.IEEE_MODE        = hwHs.flags.ieeeMode;
    spiShaderPgmRsrc1Hs.bits.LS_VGPR_COMP_CNT = metadata.pipeline.graphicsRegister.lsVgprCompCnt;
    spiShaderPgmRsrc1Hs.bits.FP16_OVFL        = hwHs.flags.fp16Overflow;

    if (IsGfx10Plus(gfxLevel))
    {
        spiShaderPgmRsrc1Hs.gfx10Plus.MEM_ORDERED  = hwHs.flags.memOrdered;
        spiShaderPgmRsrc1Hs.gfx10Plus.FWD_PROGRESS = hwHs.flags.forwardProgress;
        spiShaderPgmRsrc1Hs.gfx10Plus.WGP_MODE     = hwHs.flags.wgpMode;
    }

    return spiShaderPgmRsrc1Hs.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmRsrc2Hs(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    GfxIpLevel                              gfxLevel)
{
    const auto& hwHs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Hs)];

    SPI_SHADER_PGM_RSRC2_HS spiShaderPgmRsrc2Hs = {};
    spiShaderPgmRsrc2Hs.bits.SCRATCH_EN       = hwHs.flags.scratchEn;
    spiShaderPgmRsrc2Hs.bits.USER_SGPR        = hwHs.userSgprs;
    spiShaderPgmRsrc2Hs.bits.TRAP_PRESENT     = hwHs.flags.trapPresent;

    const uint32 ldsSize =
        Util::Pow2Align(hwHs.ldsSize / uint32(sizeof(uint32)), Gfx9LdsDwGranularity) >> Gfx9LdsDwGranularityShift;

    if (IsGfx9(gfxLevel))
    {
        spiShaderPgmRsrc2Hs.gfx09.EXCP_EN       = hwHs.excpEn;
        spiShaderPgmRsrc2Hs.gfx09.LDS_SIZE      = ldsSize;
        spiShaderPgmRsrc2Hs.gfx09.USER_SGPR_MSB = (hwHs.userSgprs >= 32);
    }
    else if (IsGfx10Plus(gfxLevel))
    {
        spiShaderPgmRsrc2Hs.gfx10Plus.EXCP_EN         = hwHs.excpEn;
        spiShaderPgmRsrc2Hs.gfx10Plus.LDS_SIZE        = ldsSize;
        spiShaderPgmRsrc2Hs.gfx10Plus.OC_LDS_EN       = hwHs.flags.offchipLdsEn;
        spiShaderPgmRsrc2Hs.gfx10Plus.TG_SIZE_EN      = metadata.pipeline.graphicsRegister.flags.hsTgSizeEn;
        spiShaderPgmRsrc2Hs.gfx10Plus.USER_SGPR_MSB   = (hwHs.userSgprs >= 32);
        spiShaderPgmRsrc2Hs.gfx10Plus.SHARED_VGPR_CNT = hwHs.sharedVgprCnt;
    }

    return spiShaderPgmRsrc2Hs.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmRsrc3Hs(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    GfxIpLevel                              gfxLevel)
{
    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();
    const auto&              hwHs      = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Hs)];

    SPI_SHADER_PGM_RSRC3_HS spiShaderPgmRsrc3Hs = {};

    if (IsGfx9(gfxLevel))
    {
        const uint32 numSaPerSe = chipProps.gfx9.numShaderArrays;
        spiShaderPgmRsrc3Hs.bits.WAVE_LIMIT = hwHs.wavesPerSe / numSaPerSe;
    }
    else if (IsGfx10Plus(chipProps.gfxLevel))
    {
        spiShaderPgmRsrc3Hs.bits.WAVE_LIMIT = hwHs.wavesPerSe;
    }

    // NOTE: The Pipeline ABI doesn't specify CU enable masks for each shader stage, so it should be safe to
    // always use the ones PAL prefers.
    spiShaderPgmRsrc3Hs.bits.CU_EN = device.GetCuEnableMask(0, UINT_MAX);

#if PAL_BUILD_GFX11
    if (device.Settings().waForceLockThresholdZero)
    {
        spiShaderPgmRsrc3Hs.bits.LOCK_LOW_THRESHOLD = 0;
    }
#endif

    return spiShaderPgmRsrc3Hs.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmRsrc4Hs(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    GfxIpLevel                              gfxLevel,
    size_t                                  codeLength)
{
    SPI_SHADER_PGM_RSRC4_HS spiShaderPgmRsrc4Hs = {};

    if (IsGfx10Plus(gfxLevel))
    {
        spiShaderPgmRsrc4Hs.gfx10Plus.CU_EN = device.GetCuEnableMaskHi(0, UINT_MAX);

#if  PAL_BUILD_GFX11
        if (IsGfx104Plus(gfxLevel))
        {
            spiShaderPgmRsrc4Hs.gfx104Plus.INST_PREF_SIZE = device.GetShaderPrefetchSize(codeLength);
        }
#endif

#if PAL_BUILD_GFX11
        // PWS+ only support pre-shader waits if the IMAGE_OP bit is set. Theoretically we only set it for shaders that
        // do an image operation. However that would mean that our use of the pre-shader PWS+ wait is dependent on us
        // only waiting on image resources, which we don't know in our interface. For now always set the IMAGE_OP bit
        // for corresponding shaders, making the pre-shader waits global.
        if (IsGfx11(gfxLevel))
        {
            spiShaderPgmRsrc4Hs.gfx11.IMAGE_OP = 1;
        }
#endif
    }

    return spiShaderPgmRsrc4Hs.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmChksumHs(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device)
{
    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();
    const auto&              hwHs      = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Hs)];

    SPI_SHADER_PGM_CHKSUM_HS spiShaderPgmChksumHs = {};
    if (chipProps.gfx9.supportSpp != 0)
    {
        spiShaderPgmChksumHs.bits.CHECKSUM = hwHs.checksumValue;
    }

    return spiShaderPgmChksumHs.u32All;
}

// =====================================================================================================================
static uint32 VgtHosMinTessLevel(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    VGT_HOS_MIN_TESS_LEVEL vgtHosMinTessLevel = {};
    vgtHosMinTessLevel.f32All = metadata.pipeline.graphicsRegister.vgtHosMinTessLevel;

    return vgtHosMinTessLevel.u32All;
}

// =====================================================================================================================
static uint32 VgtHosMaxTessLevel(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    VGT_HOS_MAX_TESS_LEVEL vgtHosMaxTessLevel = {};
    vgtHosMaxTessLevel.f32All = metadata.pipeline.graphicsRegister.vgtHosMaxTessLevel;

    return vgtHosMaxTessLevel.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmRsrc1Ps(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    GfxIpLevel                              gfxLevel)
{
    const Gfx9PalSettings& settings = device.Settings();
    const auto&            hwPs     = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Ps)];

    SPI_SHADER_PGM_RSRC1_PS spiShaderPgmRsrc1Ps = {};
    spiShaderPgmRsrc1Ps.bits.VGPRS       = CalcNumVgprs(hwPs.vgprCount, (hwPs.wavefrontSize == 32));
    spiShaderPgmRsrc1Ps.bits.SGPRS       = CalcNumSgprs(hwPs.sgprCount);
    spiShaderPgmRsrc1Ps.bits.FLOAT_MODE  = hwPs.floatMode;
    spiShaderPgmRsrc1Ps.bits.DX10_CLAMP  = 1;
    spiShaderPgmRsrc1Ps.bits.DEBUG_MODE  = hwPs.flags.debugMode;
    spiShaderPgmRsrc1Ps.bits.IEEE_MODE   = hwPs.flags.ieeeMode;
    spiShaderPgmRsrc1Ps.bits.FP16_OVFL   = hwPs.flags.fp16Overflow;

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_DISABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    spiShaderPgmRsrc1Ps.bits.CU_GROUP_DISABLE = (settings.numPsWavesSoftGroupedPerCu > 0 ? 0 : 1);

    if (IsGfx10Plus(gfxLevel))
    {
        spiShaderPgmRsrc1Ps.gfx10Plus.MEM_ORDERED  = hwPs.flags.memOrdered;
        spiShaderPgmRsrc1Ps.gfx10Plus.FWD_PROGRESS = hwPs.flags.forwardProgress;
    }

    if (IsGfx103PlusExclusive(gfxLevel))
    {
        spiShaderPgmRsrc1Ps.gfx103PlusExclusive.LOAD_PROVOKING_VTX =
            metadata.pipeline.graphicsRegister.flags.psLoadProvokingVtx;
    }

    return spiShaderPgmRsrc1Ps.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmRsrc2Ps(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    GfxIpLevel                              gfxLevel)
{
    const auto&            hwPs     = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Ps)];

    SPI_SHADER_PGM_RSRC2_PS spiShaderPgmRsrc2Ps = {};
    spiShaderPgmRsrc2Ps.bits.SCRATCH_EN       = hwPs.flags.scratchEn;
    spiShaderPgmRsrc2Ps.bits.USER_SGPR        = hwPs.userSgprs;
    spiShaderPgmRsrc2Ps.bits.TRAP_PRESENT     = hwPs.flags.trapPresent;
    spiShaderPgmRsrc2Ps.bits.WAVE_CNT_EN      = metadata.pipeline.graphicsRegister.flags.psWaveCntEn;

#if PAL_BUILD_GFX11
    const uint32 psExtraLdsDwGranularityShift = IsGfx11(gfxLevel) ? Gfx11PsExtraLdsDwGranularityShift :
                                                                    Gfx9PsExtraLdsDwGranularityShift;
#else
    const uint32 psExtraLdsDwGranularityShift = Gfx9PsExtraLdsDwGranularityShift;
#endif

    spiShaderPgmRsrc2Ps.bits.EXTRA_LDS_SIZE   =
        (metadata.pipeline.graphicsRegister.psExtraLdsSize / sizeof(uint32)) >> psExtraLdsDwGranularityShift;
    spiShaderPgmRsrc2Ps.bits.EXCP_EN          = hwPs.excpEn;

    // These two bits are duplicated in PA_SC_SHADER_CONTROL.
    const Util::PalAbi::PaScShaderControlMetadata& paScShaderControl =
        metadata.pipeline.graphicsRegister.paScShaderControl;
    spiShaderPgmRsrc2Ps.bits.LOAD_COLLISION_WAVEID    = paScShaderControl.flags.loadCollisionWaveid;
    spiShaderPgmRsrc2Ps.bits.LOAD_INTRAWAVE_COLLISION = paScShaderControl.flags.loadIntrawaveCollision;

    if (IsGfx9(gfxLevel))
    {
        spiShaderPgmRsrc2Ps.gfx09.USER_SGPR_MSB = (hwPs.userSgprs >= 32);
    }
    else if (IsGfx10Plus(gfxLevel))
    {
        spiShaderPgmRsrc2Ps.gfx10Plus.USER_SGPR_MSB   = (hwPs.userSgprs >= 32);
        spiShaderPgmRsrc2Ps.gfx10Plus.SHARED_VGPR_CNT = hwPs.sharedVgprCnt;
    }

    return spiShaderPgmRsrc2Ps.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmRsrc3Ps(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const GraphicsPipelineCreateInfo&       createInfo,
    const Device&                           device,
    GfxIpLevel                              gfxLevel)
{
    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();
    const Gfx9PalSettings&   settings  = device.Settings();

    const auto& hwPs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Ps)];

    SPI_SHADER_PGM_RSRC3_PS spiShaderPgmRsrc3Ps = {};

    if (IsGfx9(gfxLevel))
    {
        const uint32 numSaPerSe = chipProps.gfx9.numShaderArrays;
        spiShaderPgmRsrc3Ps.bits.WAVE_LIMIT = hwPs.wavesPerSe / numSaPerSe;
    }
    else if (IsGfx10Plus(gfxLevel))
    {
        spiShaderPgmRsrc3Ps.bits.WAVE_LIMIT = hwPs.wavesPerSe;
    }

    spiShaderPgmRsrc3Ps.bits.CU_EN = device.GetCuEnableMask(0, settings.psCuEnLimitMask);

#if  PAL_BUILD_GFX11
    if (IsGfx104Plus(gfxLevel))
    {
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 744)
        if (createInfo.ldsPsGroupSizeOverride != LdsPsGroupSizeOverride::Default)
        {
            spiShaderPgmRsrc3Ps.gfx104Plus.LDS_GROUP_SIZE =
                (static_cast<uint32>(createInfo.ldsPsGroupSizeOverride) - 1U);
        }
        else
#endif
        {
            spiShaderPgmRsrc3Ps.gfx104Plus.LDS_GROUP_SIZE = static_cast<uint32>(settings.ldsPsGroupSize);
        }
    }
#endif

    return spiShaderPgmRsrc3Ps.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmRsrc4Ps(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    GfxIpLevel                              gfxLevel,
    size_t                                  codeLength)
{
    const Gfx9PalSettings&   settings  = device.Settings();

    const auto& hwPs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Ps)];

    SPI_SHADER_PGM_RSRC4_PS spiShaderPgmRsrc4Ps = {};
    if (IsGfx10Plus(gfxLevel))
    {
        spiShaderPgmRsrc4Ps.bits.CU_EN = device.GetCuEnableMaskHi(0, settings.psCuEnLimitMask);

#if  PAL_BUILD_GFX11
        if (IsGfx104Plus(gfxLevel))
        {
            spiShaderPgmRsrc4Ps.gfx104Plus.INST_PREF_SIZE = device.GetShaderPrefetchSize(codeLength);
        }
#endif

#if PAL_BUILD_GFX11
        // PWS+ only support pre-shader waits if the IMAGE_OP bit is set. Theoretically we only set it for shaders
        // that do an image operation. However that would mean that our use of the pre-shader PWS+ wait is dependent
        // on us only waiting on image resources, which we don't know in our interface. For now always set the
        // IMAGE_OP bit for corresponding shaders, making the pre-shader waits global.
        if (IsGfx11(gfxLevel))
        {
            spiShaderPgmRsrc4Ps.gfx11.IMAGE_OP = 1;
        }
#endif
    }

    return spiShaderPgmRsrc4Ps.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmChksumPs(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device)
{
    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();
    const auto&              hwPs      = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Ps)];

    SPI_SHADER_PGM_CHKSUM_PS spiShaderPgmChksumPs = {};
    if (chipProps.gfx9.supportSpp != 0)
    {
        spiShaderPgmChksumPs.bits.CHECKSUM = hwPs.checksumValue;
    }

    return spiShaderPgmChksumPs.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmRsrc1Vs(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    GfxIpLevel                              gfxLevel)
{
    const Gfx9PalSettings& settings = device.Settings();

    const auto& hwVs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Vs)];

    SPI_SHADER_PGM_RSRC1_VS spiShaderPgmRsrc1Vs = {};
    spiShaderPgmRsrc1Vs.bits.VGPRS         = CalcNumVgprs(hwVs.vgprCount, (hwVs.wavefrontSize == 32));
    spiShaderPgmRsrc1Vs.bits.SGPRS         = CalcNumSgprs(hwVs.sgprCount);
    spiShaderPgmRsrc1Vs.bits.FLOAT_MODE    = hwVs.floatMode;
    spiShaderPgmRsrc1Vs.bits.DX10_CLAMP    = 1;
    spiShaderPgmRsrc1Vs.bits.DEBUG_MODE    = hwVs.flags.debugMode;
    spiShaderPgmRsrc1Vs.bits.IEEE_MODE     = hwVs.flags.ieeeMode;
    spiShaderPgmRsrc1Vs.bits.VGPR_COMP_CNT = metadata.pipeline.graphicsRegister.vsVgprCompCnt;
    spiShaderPgmRsrc1Vs.bits.FP16_OVFL     = hwVs.flags.fp16Overflow;

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_ENABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    spiShaderPgmRsrc1Vs.bits.CU_GROUP_ENABLE = (settings.numVsWavesSoftGroupedPerCu > 0 ? 1 : 0);

    if (IsGfx10(gfxLevel))
    {
        spiShaderPgmRsrc1Vs.gfx10.MEM_ORDERED  = hwVs.flags.memOrdered;
        spiShaderPgmRsrc1Vs.gfx10.FWD_PROGRESS = hwVs.flags.forwardProgress;
    }

    return spiShaderPgmRsrc1Vs.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmRsrc2Vs(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    GfxIpLevel                              gfxLevel)
{
    const auto& hwVs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Vs)];

    SPI_SHADER_PGM_RSRC2_VS spiShaderPgmRsrc2Vs = {};
    spiShaderPgmRsrc2Vs.bits.SCRATCH_EN   = hwVs.flags.scratchEn;
    spiShaderPgmRsrc2Vs.bits.USER_SGPR    = hwVs.userSgprs;
    spiShaderPgmRsrc2Vs.bits.TRAP_PRESENT = hwVs.flags.trapPresent;
    spiShaderPgmRsrc2Vs.bits.OC_LDS_EN    = hwVs.flags.offchipLdsEn;
    spiShaderPgmRsrc2Vs.bits.SO_BASE0_EN  = metadata.pipeline.graphicsRegister.flags.vsSoBase0En;
    spiShaderPgmRsrc2Vs.bits.SO_BASE1_EN  = metadata.pipeline.graphicsRegister.flags.vsSoBase1En;
    spiShaderPgmRsrc2Vs.bits.SO_BASE2_EN  = metadata.pipeline.graphicsRegister.flags.vsSoBase2En;
    spiShaderPgmRsrc2Vs.bits.SO_BASE3_EN  = metadata.pipeline.graphicsRegister.flags.vsSoBase3En;
    spiShaderPgmRsrc2Vs.bits.SO_EN        = metadata.pipeline.graphicsRegister.flags.vsStreamoutEn;
    spiShaderPgmRsrc2Vs.bits.EXCP_EN      = hwVs.excpEn;
    spiShaderPgmRsrc2Vs.bits.PC_BASE_EN   = metadata.pipeline.graphicsRegister.flags.vsPcBaseEn;

    if (IsGfx9(gfxLevel))
    {
        spiShaderPgmRsrc2Vs.gfx09.USER_SGPR_MSB = (hwVs.userSgprs >= 32);
    }
    else if (IsGfx10(gfxLevel))
    {
        spiShaderPgmRsrc2Vs.gfx10.USER_SGPR_MSB   = (hwVs.userSgprs >= 32);
        spiShaderPgmRsrc2Vs.gfx10.SHARED_VGPR_CNT = hwVs.sharedVgprCnt;
    }

    return spiShaderPgmRsrc2Vs.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmRsrc3Vs(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    GfxIpLevel                              gfxLevel)
{
    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();
    const Gfx9PalSettings&   settings  = device.Settings();

    const auto& hwVs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Vs)];

    SPI_SHADER_PGM_RSRC3_VS spiShaderPgmRsrc3Vs = {};

    if (IsGfx9(gfxLevel))
    {
        const uint32 numSaPerSe = chipProps.gfx9.numShaderArrays;
        spiShaderPgmRsrc3Vs.bits.WAVE_LIMIT = hwVs.wavesPerSe / numSaPerSe;
    }
    else if (IsGfx10(gfxLevel))
    {
        spiShaderPgmRsrc3Vs.bits.WAVE_LIMIT = hwVs.wavesPerSe;
    }

    uint16 vsCuDisableMask = 0;
    if (IsGfx101(gfxLevel))
    {
        // Both CU's of a WGP need to be disabled for better performance.
        vsCuDisableMask = 0xC;
    }
    else
    {
        // Disable virtualized CU #1 instead of #0 because thread traces use CU #0 by default.
        vsCuDisableMask = 0x2;
    }

    // NOTE: The Pipeline ABI doesn't specify CU enable masks for each shader stage, so it should be safe to
    // always use the ones PAL prefers.
    spiShaderPgmRsrc3Vs.bits.CU_EN = device.GetCuEnableMask(vsCuDisableMask, settings.vsCuEnLimitMask);

    return spiShaderPgmRsrc3Vs.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmRsrc4Vs(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    GfxIpLevel                              gfxLevel,
    size_t                                  codeLength)
{
    const Gfx9PalSettings&   settings  = device.Settings();

    const auto& hwVs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Vs)];

    SPI_SHADER_PGM_RSRC4_VS spiShaderPgmRsrc4Vs = {};

    if (IsGfx10Plus(gfxLevel))
    {
        const uint16 vsCuDisableMaskHi = 0;
        spiShaderPgmRsrc4Vs.bits.CU_EN = device.GetCuEnableMaskHi(vsCuDisableMaskHi, settings.vsCuEnLimitMask);

    }

    return spiShaderPgmRsrc4Vs.u32All;
}

// =====================================================================================================================
static uint32 SpiShaderPgmChksumVs(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device)
{
    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();
    const auto&              hwVs      = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Vs)];

    SPI_SHADER_PGM_CHKSUM_VS spiShaderPgmChksumVs = {};
    if (chipProps.gfx9.supportSpp != 0)
    {
        spiShaderPgmChksumVs.most.CHECKSUM = hwVs.checksumValue;
    }

    return spiShaderPgmChksumVs.u32All;
}

// =====================================================================================================================
static uint32 VgtStrmoutConfig(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    const Util::PalAbi::VgtStrmoutConfigMetadata& vgtStrmoutConfigMetadata =
        metadata.pipeline.graphicsRegister.vgtStrmoutConfig;

    VGT_STRMOUT_CONFIG vgtStrmoutConfig = {};
    vgtStrmoutConfig.bits.STREAMOUT_0_EN       = vgtStrmoutConfigMetadata.flags.streamout_0En;
    vgtStrmoutConfig.bits.STREAMOUT_1_EN       = vgtStrmoutConfigMetadata.flags.streamout_1En;
    vgtStrmoutConfig.bits.STREAMOUT_2_EN       = vgtStrmoutConfigMetadata.flags.streamout_2En;
    vgtStrmoutConfig.bits.STREAMOUT_3_EN       = vgtStrmoutConfigMetadata.flags.streamout_3En;
    vgtStrmoutConfig.bits.RAST_STREAM          = vgtStrmoutConfigMetadata.rastStream;
    vgtStrmoutConfig.bits.EN_PRIMS_NEEDED_CNT  = vgtStrmoutConfigMetadata.flags.primsNeededCntEn;
    vgtStrmoutConfig.bits.RAST_STREAM_MASK     = vgtStrmoutConfigMetadata.rastStreamMask;
    vgtStrmoutConfig.bits.USE_RAST_STREAM_MASK = vgtStrmoutConfigMetadata.flags.useRastStreamMask;

    return vgtStrmoutConfig.u32All;
}

// =====================================================================================================================
static void SpiPsInputCntl(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    GfxIpLevel                              gfxLevel,
    SPI_PS_INPUT_CNTL_0*                    pSpiPsInputCntls,
    uint32*                                 pInterpolatorCount)
{
    *pInterpolatorCount = metadata.pipeline.numInterpolants;

    for (uint32 i = 0; i < metadata.pipeline.numInterpolants; ++i)
    {
        const Util::PalAbi::SpiPsInputCntlMetadata& spiPsInputCntl =
            metadata.pipeline.graphicsRegister.spiPsInputCntl[i];

        SPI_PS_INPUT_CNTL_0* pSpiPsInputCntl = &pSpiPsInputCntls[i];

        pSpiPsInputCntl->bits.OFFSET           = spiPsInputCntl.offset;
        pSpiPsInputCntl->bits.DEFAULT_VAL      = spiPsInputCntl.defaultVal;
        pSpiPsInputCntl->bits.FLAT_SHADE       = spiPsInputCntl.flags.flatShade;
        pSpiPsInputCntl->bits.PT_SPRITE_TEX    = spiPsInputCntl.flags.ptSpriteTex;
        pSpiPsInputCntl->bits.FP16_INTERP_MODE = spiPsInputCntl.flags.fp16InterpMode;
        pSpiPsInputCntl->bits.ATTR0_VALID      = spiPsInputCntl.flags.attr0Valid;
        pSpiPsInputCntl->bits.ATTR1_VALID      = spiPsInputCntl.flags.attr1Valid;

        if (IsGfx9(gfxLevel) || IsGfx10(gfxLevel))
        {
            pSpiPsInputCntl->gfx09_10.CYL_WRAP = spiPsInputCntl.cylWrap;
        }

        if (IsGfx103PlusExclusive(gfxLevel))
        {
            pSpiPsInputCntl->gfx103PlusExclusive.ROTATE_PC_PTR = spiPsInputCntl.flags.rotatePcPtr;
        }

#if PAL_BUILD_GFX11
        if (IsGfx11(gfxLevel))
        {
            pSpiPsInputCntl->gfx11.PRIM_ATTR = spiPsInputCntl.flags.primAttr;
        }
#endif
    }
}

// =====================================================================================================================
static uint32 VgtStrmoutBufferConfig(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    const Util::PalAbi::VgtStrmoutBufferConfigMetadata& vgtStrmoutBufferConfigMetadata =
        metadata.pipeline.graphicsRegister.vgtStrmoutBufferConfig;

    VGT_STRMOUT_BUFFER_CONFIG vgtStrmoutBufferConfig = {};

    vgtStrmoutBufferConfig.bits.STREAM_0_BUFFER_EN = vgtStrmoutBufferConfigMetadata.stream_0BufferEn;
    vgtStrmoutBufferConfig.bits.STREAM_1_BUFFER_EN = vgtStrmoutBufferConfigMetadata.stream_1BufferEn;
    vgtStrmoutBufferConfig.bits.STREAM_2_BUFFER_EN = vgtStrmoutBufferConfigMetadata.stream_2BufferEn;
    vgtStrmoutBufferConfig.bits.STREAM_3_BUFFER_EN = vgtStrmoutBufferConfigMetadata.stream_3BufferEn;

    return vgtStrmoutBufferConfig.u32All;
}

// =====================================================================================================================
static void VgtStrmoutVtxStrides(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    VGT_STRMOUT_VTX_STRIDE_0*               pVtxStrides)
{
    for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
    {
        pVtxStrides[i].bits.STRIDE = metadata.pipeline.streamoutVertexStrides[i];
    }
}

// =====================================================================================================================
static uint32 DbShaderControl(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    GfxIpLevel                              gfxLevel)
{
    const Util::PalAbi::DbShaderControlMetadata& dbShaderControlMetadata =
        metadata.pipeline.graphicsRegister.dbShaderControl;

    DB_SHADER_CONTROL dbShaderControl = {};
    dbShaderControl.bits.Z_EXPORT_ENABLE                = dbShaderControlMetadata.flags.zExportEnable;
    dbShaderControl.bits.STENCIL_TEST_VAL_EXPORT_ENABLE = dbShaderControlMetadata.flags.stencilTestValExportEnable;
    dbShaderControl.bits.STENCIL_OP_VAL_EXPORT_ENABLE   = dbShaderControlMetadata.flags.stencilOpValExportEnable;
    dbShaderControl.bits.Z_ORDER                        = dbShaderControlMetadata.zOrder;
    dbShaderControl.bits.KILL_ENABLE                    = dbShaderControlMetadata.flags.killEnable;
    dbShaderControl.bits.COVERAGE_TO_MASK_ENABLE        = dbShaderControlMetadata.flags.coverageToMaskEn;
    dbShaderControl.bits.MASK_EXPORT_ENABLE             = dbShaderControlMetadata.flags.maskExportEnable;
    dbShaderControl.bits.EXEC_ON_HIER_FAIL              = dbShaderControlMetadata.flags.execOnHierFail;
    dbShaderControl.bits.EXEC_ON_NOOP                   = dbShaderControlMetadata.flags.execOnNoop;
    dbShaderControl.bits.ALPHA_TO_MASK_DISABLE          = dbShaderControlMetadata.flags.alphaToMaskDisable;
    dbShaderControl.bits.DEPTH_BEFORE_SHADER            = dbShaderControlMetadata.flags.depthBeforeShader;
    dbShaderControl.bits.CONSERVATIVE_Z_EXPORT          = dbShaderControlMetadata.conservativeZExport;
    dbShaderControl.bits.PRIMITIVE_ORDERED_PIXEL_SHADER = dbShaderControlMetadata.flags.primitiveOrderedPixelShader;

    if (static_cast<TossPointMode>(device.Parent()->Settings().tossPointMode) == TossPointAfterPs)
    {
        // Set EXEC_ON_NOOP to 1 to disallow the DB from turning off the PS entirely when TossPointAfterPs is set (i.e.
        // disable all color buffer writes by setting CB_TARGET_MASK = 0). Without this bit set, the DB will check
        // the CB_TARGET_MASK and turn off the PS if no consumers of the shader are present.
        dbShaderControl.bits.EXEC_ON_NOOP = 1;
    }

    if (IsGfx10Plus(gfxLevel))
    {
        dbShaderControl.gfx10Plus.PRE_SHADER_DEPTH_COVERAGE_ENABLE =
            dbShaderControlMetadata.flags.preShaderDepthCoverageEnable;
    }

#if PAL_BUILD_GFX11
    if (IsGfx11(gfxLevel) && metadata.pipeline.graphicsRegister.dbShaderControl.flags.primitiveOrderedPixelShader)
    {
        // From the reg-spec:
        //    This must be enabled and OVERRIDE_INTRINSIC_RATE set to 0 (1xaa) in POPS mode
        //    with super-sampling disabled
        dbShaderControl.gfx11.OVERRIDE_INTRINSIC_RATE_ENABLE = 1;
        dbShaderControl.gfx11.OVERRIDE_INTRINSIC_RATE = 0;
    }
#endif

    return dbShaderControl.u32All;
}

// =====================================================================================================================
static uint32 SpiBarycCntl(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    GfxIpLevel                              gfxLevel)
{
    const Util::PalAbi::SpiBarycCntlMetadata& spiBarycCntlMetadata = metadata.pipeline.graphicsRegister.spiBarycCntl;

    SPI_BARYC_CNTL spiBarycCntl = {};
    spiBarycCntl.bits.POS_FLOAT_LOCATION  = spiBarycCntlMetadata.posFloatLocation;
    spiBarycCntl.bits.FRONT_FACE_ALL_BITS = spiBarycCntlMetadata.flags.frontFaceAllBits;

    return spiBarycCntl.u32All;
}

// =====================================================================================================================
static uint32 SpiPsInputAddr(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    const Util::PalAbi::SpiPsInputAddrMetadata& spiPsInputMetadata = metadata.pipeline.graphicsRegister.spiPsInputAddr;

    SPI_PS_INPUT_ADDR spiPsInputAddr = {};
    spiPsInputAddr.bits.PERSP_SAMPLE_ENA     = spiPsInputMetadata.flags.perspSampleEna;
    spiPsInputAddr.bits.PERSP_CENTER_ENA     = spiPsInputMetadata.flags.perspCenterEna;
    spiPsInputAddr.bits.PERSP_CENTROID_ENA   = spiPsInputMetadata.flags.perspCentroidEna;
    spiPsInputAddr.bits.PERSP_PULL_MODEL_ENA = spiPsInputMetadata.flags.perspPullModelEna;
    spiPsInputAddr.bits.LINEAR_SAMPLE_ENA    = spiPsInputMetadata.flags.linearSampleEna;
    spiPsInputAddr.bits.LINEAR_CENTER_ENA    = spiPsInputMetadata.flags.linearCenterEna;
    spiPsInputAddr.bits.LINEAR_CENTROID_ENA  = spiPsInputMetadata.flags.linearCentroidEna;
    spiPsInputAddr.bits.LINE_STIPPLE_TEX_ENA = spiPsInputMetadata.flags.lineStippleTexEna;
    spiPsInputAddr.bits.POS_X_FLOAT_ENA      = spiPsInputMetadata.flags.posXFloatEna;
    spiPsInputAddr.bits.POS_Y_FLOAT_ENA      = spiPsInputMetadata.flags.posYFloatEna;
    spiPsInputAddr.bits.POS_Z_FLOAT_ENA      = spiPsInputMetadata.flags.posZFloatEna;
    spiPsInputAddr.bits.POS_W_FLOAT_ENA      = spiPsInputMetadata.flags.posWFloatEna;
    spiPsInputAddr.bits.FRONT_FACE_ENA       = spiPsInputMetadata.flags.frontFaceEna;
    spiPsInputAddr.bits.ANCILLARY_ENA        = spiPsInputMetadata.flags.ancillaryEna;
    spiPsInputAddr.bits.SAMPLE_COVERAGE_ENA  = spiPsInputMetadata.flags.sampleCoverageEna;
    spiPsInputAddr.bits.POS_FIXED_PT_ENA     = spiPsInputMetadata.flags.posFixedPtEna;

    return spiPsInputAddr.u32All;
}

// =====================================================================================================================
static uint32 SpiPsInputEna(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    const Util::PalAbi::SpiPsInputEnaMetadata& spiPsInputMetadata = metadata.pipeline.graphicsRegister.spiPsInputEna;

    SPI_PS_INPUT_ENA spiPsInputEna = {};
    spiPsInputEna.bits.PERSP_SAMPLE_ENA     = spiPsInputMetadata.flags.perspSampleEna;
    spiPsInputEna.bits.PERSP_CENTER_ENA     = spiPsInputMetadata.flags.perspCenterEna;
    spiPsInputEna.bits.PERSP_CENTROID_ENA   = spiPsInputMetadata.flags.perspCentroidEna;
    spiPsInputEna.bits.PERSP_PULL_MODEL_ENA = spiPsInputMetadata.flags.perspPullModelEna;
    spiPsInputEna.bits.LINEAR_SAMPLE_ENA    = spiPsInputMetadata.flags.linearSampleEna;
    spiPsInputEna.bits.LINEAR_CENTER_ENA    = spiPsInputMetadata.flags.linearCenterEna;
    spiPsInputEna.bits.LINEAR_CENTROID_ENA  = spiPsInputMetadata.flags.linearCentroidEna;
    spiPsInputEna.bits.LINE_STIPPLE_TEX_ENA = spiPsInputMetadata.flags.lineStippleTexEna;
    spiPsInputEna.bits.POS_X_FLOAT_ENA      = spiPsInputMetadata.flags.posXFloatEna;
    spiPsInputEna.bits.POS_Y_FLOAT_ENA      = spiPsInputMetadata.flags.posYFloatEna;
    spiPsInputEna.bits.POS_Z_FLOAT_ENA      = spiPsInputMetadata.flags.posZFloatEna;
    spiPsInputEna.bits.POS_W_FLOAT_ENA      = spiPsInputMetadata.flags.posWFloatEna;
    spiPsInputEna.bits.FRONT_FACE_ENA       = spiPsInputMetadata.flags.frontFaceEna;
    spiPsInputEna.bits.ANCILLARY_ENA        = spiPsInputMetadata.flags.ancillaryEna;
    spiPsInputEna.bits.SAMPLE_COVERAGE_ENA  = spiPsInputMetadata.flags.sampleCoverageEna;
    spiPsInputEna.bits.POS_FIXED_PT_ENA     = spiPsInputMetadata.flags.posFixedPtEna;

    return spiPsInputEna.u32All;
}

// =====================================================================================================================
static uint32 PaClVsOutCntl(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const GraphicsPipelineCreateInfo&       createInfo,
    GfxIpLevel                              gfxLevel)
{
    const Util::PalAbi::PaClVsOutCntlMetadata& paClVsOutCntlMetadata = metadata.pipeline.graphicsRegister.paClVsOutCntl;

    PA_CL_VS_OUT_CNTL paClVsOutCntl = {};

    paClVsOutCntl.bits.CLIP_DIST_ENA_0            = paClVsOutCntlMetadata.flags.clipDistEna_0;
    paClVsOutCntl.bits.CLIP_DIST_ENA_1            = paClVsOutCntlMetadata.flags.clipDistEna_1;
    paClVsOutCntl.bits.CLIP_DIST_ENA_2            = paClVsOutCntlMetadata.flags.clipDistEna_2;
    paClVsOutCntl.bits.CLIP_DIST_ENA_3            = paClVsOutCntlMetadata.flags.clipDistEna_3;
    paClVsOutCntl.bits.CLIP_DIST_ENA_4            = paClVsOutCntlMetadata.flags.clipDistEna_4;
    paClVsOutCntl.bits.CLIP_DIST_ENA_5            = paClVsOutCntlMetadata.flags.clipDistEna_5;
    paClVsOutCntl.bits.CLIP_DIST_ENA_6            = paClVsOutCntlMetadata.flags.clipDistEna_6;
    paClVsOutCntl.bits.CLIP_DIST_ENA_7            = paClVsOutCntlMetadata.flags.clipDistEna_7;
    paClVsOutCntl.bits.CULL_DIST_ENA_0            = paClVsOutCntlMetadata.flags.cullDistEna_0;
    paClVsOutCntl.bits.CULL_DIST_ENA_1            = paClVsOutCntlMetadata.flags.cullDistEna_1;
    paClVsOutCntl.bits.CULL_DIST_ENA_2            = paClVsOutCntlMetadata.flags.cullDistEna_2;
    paClVsOutCntl.bits.CULL_DIST_ENA_3            = paClVsOutCntlMetadata.flags.cullDistEna_3;
    paClVsOutCntl.bits.CULL_DIST_ENA_4            = paClVsOutCntlMetadata.flags.cullDistEna_4;
    paClVsOutCntl.bits.CULL_DIST_ENA_5            = paClVsOutCntlMetadata.flags.cullDistEna_5;
    paClVsOutCntl.bits.CULL_DIST_ENA_6            = paClVsOutCntlMetadata.flags.cullDistEna_6;
    paClVsOutCntl.bits.CULL_DIST_ENA_7            = paClVsOutCntlMetadata.flags.cullDistEna_7;
    paClVsOutCntl.bits.USE_VTX_POINT_SIZE         = paClVsOutCntlMetadata.flags.useVtxPointSize;
    paClVsOutCntl.bits.USE_VTX_EDGE_FLAG          = paClVsOutCntlMetadata.flags.useVtxEdgeFlag;
    paClVsOutCntl.bits.USE_VTX_RENDER_TARGET_INDX = paClVsOutCntlMetadata.flags.useVtxRenderTargetIndx;
    paClVsOutCntl.bits.USE_VTX_VIEWPORT_INDX      = paClVsOutCntlMetadata.flags.useVtxViewportIndx;
    paClVsOutCntl.bits.USE_VTX_KILL_FLAG          = paClVsOutCntlMetadata.flags.useVtxKillFlag;
    paClVsOutCntl.bits.VS_OUT_MISC_VEC_ENA        = paClVsOutCntlMetadata.flags.vsOutMiscVecEna;
    paClVsOutCntl.bits.VS_OUT_CCDIST0_VEC_ENA     = paClVsOutCntlMetadata.flags.vsOutCcDist0VecEna;
    paClVsOutCntl.bits.VS_OUT_CCDIST1_VEC_ENA     = paClVsOutCntlMetadata.flags.vsOutCcDist1VecEna;
    paClVsOutCntl.bits.VS_OUT_MISC_SIDE_BUS_ENA   = paClVsOutCntlMetadata.flags.vsOutMiscSideBusEna;

    if (IsGfx9(gfxLevel))
    {
        paClVsOutCntl.gfx09.USE_VTX_LINE_WIDTH = paClVsOutCntlMetadata.flags.useVtxLineWidth;
    }

    if (IsGfx9(gfxLevel) || IsGfx10(gfxLevel))
    {
        paClVsOutCntl.gfx09_10.USE_VTX_GS_CUT_FLAG = paClVsOutCntlMetadata.flags.useVtxGsCutFlag;
    }

    if (IsGfx10Plus(gfxLevel))
    {
        paClVsOutCntl.gfx10Plus.USE_VTX_LINE_WIDTH = paClVsOutCntlMetadata.flags.useVtxLineWidth;
    }

    if (IsGfx103Plus(gfxLevel))
    {
        paClVsOutCntl.gfx103Plus.USE_VTX_VRS_RATE          = paClVsOutCntlMetadata.flags.useVtxVrsRate;
        paClVsOutCntl.gfx103Plus.BYPASS_VTX_RATE_COMBINER  = paClVsOutCntlMetadata.flags.bypassVtxRateCombiner;
        paClVsOutCntl.gfx103Plus.BYPASS_PRIM_RATE_COMBINER = paClVsOutCntlMetadata.flags.bypassPrimRateCombiner;
    }

#if PAL_BUILD_GFX11
    if (IsGfx11(gfxLevel)
        )
    {
        paClVsOutCntl.gfx110.USE_VTX_FSR_SELECT = paClVsOutCntlMetadata.flags.useVtxFsrSelect;
    }
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 733
    if (createInfo.rsState.flags.cullDistMaskValid != 0)
    {
        paClVsOutCntl.bitfields.CULL_DIST_ENA_0 &= (createInfo.rsState.cullDistMask & 0x1) != 0;
        paClVsOutCntl.bitfields.CULL_DIST_ENA_1 &= (createInfo.rsState.cullDistMask & 0x2) != 0;
        paClVsOutCntl.bitfields.CULL_DIST_ENA_2 &= (createInfo.rsState.cullDistMask & 0x4) != 0;
        paClVsOutCntl.bitfields.CULL_DIST_ENA_3 &= (createInfo.rsState.cullDistMask & 0x8) != 0;
        paClVsOutCntl.bitfields.CULL_DIST_ENA_4 &= (createInfo.rsState.cullDistMask & 0x10) != 0;
        paClVsOutCntl.bitfields.CULL_DIST_ENA_5 &= (createInfo.rsState.cullDistMask & 0x20) != 0;
        paClVsOutCntl.bitfields.CULL_DIST_ENA_6 &= (createInfo.rsState.cullDistMask & 0x40) != 0;
        paClVsOutCntl.bitfields.CULL_DIST_ENA_7 &= (createInfo.rsState.cullDistMask & 0x80) != 0;
    }

    if (createInfo.rsState.flags.clipDistMaskValid != 0)
#else
    if (createInfo.rsState.clipDistMask != 0)
#endif
    {
        paClVsOutCntl.bitfields.CLIP_DIST_ENA_0 &= (createInfo.rsState.clipDistMask & 0x1) != 0;
        paClVsOutCntl.bitfields.CLIP_DIST_ENA_1 &= (createInfo.rsState.clipDistMask & 0x2) != 0;
        paClVsOutCntl.bitfields.CLIP_DIST_ENA_2 &= (createInfo.rsState.clipDistMask & 0x4) != 0;
        paClVsOutCntl.bitfields.CLIP_DIST_ENA_3 &= (createInfo.rsState.clipDistMask & 0x8) != 0;
        paClVsOutCntl.bitfields.CLIP_DIST_ENA_4 &= (createInfo.rsState.clipDistMask & 0x10) != 0;
        paClVsOutCntl.bitfields.CLIP_DIST_ENA_5 &= (createInfo.rsState.clipDistMask & 0x20) != 0;
        paClVsOutCntl.bitfields.CLIP_DIST_ENA_6 &= (createInfo.rsState.clipDistMask & 0x40) != 0;
        paClVsOutCntl.bitfields.CLIP_DIST_ENA_7 &= (createInfo.rsState.clipDistMask & 0x80) != 0;
}

    // Unlike our hardware, DX12 and Vulkan do not have separate vertex and primitive combiners.
    // A mesh shader is the only shader that can export a primitive rate so if there is
    // no mesh shader then we can bypass the prim rate combiner. Vulkan does not use mesh shaders
    // so BYPASS_PRIM_RATE_COMBINER should always be 1 there.
    if (IsGfx103Plus(gfxLevel))
    {
        if (metadata.pipeline.shader[static_cast<uint32>(Util::Abi::ApiShaderType::Mesh)].hasEntry.uAll != 0)
        {
            paClVsOutCntl.gfx103Plus.BYPASS_VTX_RATE_COMBINER = 1;
        }
        else
        {
            paClVsOutCntl.gfx103Plus.BYPASS_PRIM_RATE_COMBINER = 1;
        }
    }

    return paClVsOutCntl.u32All;
}

// =====================================================================================================================
static uint32 VgtPrimitiveIdEn(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    VGT_PRIMITIVEID_EN vgtPrimitiveIdEn = {};
    vgtPrimitiveIdEn.bits.PRIMITIVEID_EN           = metadata.pipeline.graphicsRegister.flags.vgtPrimitiveIdEn;
    vgtPrimitiveIdEn.bits.NGG_DISABLE_PROVOK_REUSE = metadata.pipeline.graphicsRegister.flags.nggDisableProvokReuse;

    return vgtPrimitiveIdEn.u32All;
}

// =====================================================================================================================
static uint32 PaScShaderControl(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    GfxIpLevel                              gfxLevel)
{
    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();
    const Gfx9PalSettings&   settings  = device.Settings();

    const Util::PalAbi::PaScShaderControlMetadata& paScShaderControlMetadata =
        metadata.pipeline.graphicsRegister.paScShaderControl;

    PA_SC_SHADER_CONTROL paScShaderControl = {};
    paScShaderControl.core.LOAD_COLLISION_WAVEID    = paScShaderControlMetadata.flags.loadCollisionWaveid;
    paScShaderControl.core.LOAD_INTRAWAVE_COLLISION = paScShaderControlMetadata.flags.loadIntrawaveCollision;

    if (IsGfx10Plus(gfxLevel))
    {
        paScShaderControl.gfx10Plus.WAVE_BREAK_REGION_SIZE = paScShaderControlMetadata.waveBreakRegionSize;
    }

    if (chipProps.gfx9.supportCustomWaveBreakSize && (settings.forceWaveBreakSize != Gfx10ForceWaveBreakSizeClient))
    {
        // Override whatever wave-break size was specified by the pipeline binary if the panel is forcing a
        // value for the preferred wave-break size.
        paScShaderControl.gfx10Plus.WAVE_BREAK_REGION_SIZE = static_cast<uint32>(settings.forceWaveBreakSize);
    }

    return paScShaderControl.u32All;
}

// =====================================================================================================================
static uint32 PaScAaConfig(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    static_assert((uint32(Util::Abi::CoverageToShaderSel::InputCoverage)      == INPUT_COVERAGE)       &&
                  (uint32(Util::Abi::CoverageToShaderSel::InputInnerCoverage) == INPUT_INNER_COVERAGE) &&
                  (uint32(Util::Abi::CoverageToShaderSel::InputDepthCoverage) == INPUT_DEPTH_COVERAGE),
                  "ABI and HW enum values do not match!");

    PA_SC_AA_CONFIG paScAaConfig = {};
    paScAaConfig.bits.COVERAGE_TO_SHADER_SELECT = uint32(metadata.pipeline.graphicsRegister.aaCoverageToShaderSelect);

    return paScAaConfig.u32All;
}

// =====================================================================================================================
static uint32 ComputeNumThreadX(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    const auto& hwCs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Cs)];
    COMPUTE_NUM_THREAD_X computeNumThreadX = { };

    computeNumThreadX.bits.NUM_THREAD_FULL = hwCs.threadgroupDimensions[0];
    return computeNumThreadX.u32All;
}

// =====================================================================================================================
static uint32 ComputeNumThreadY(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    const auto& hwCs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Cs)];
    COMPUTE_NUM_THREAD_X computeNumThreadY = { };

    computeNumThreadY.bits.NUM_THREAD_FULL = hwCs.threadgroupDimensions[1];
    return computeNumThreadY.u32All;
}

// =====================================================================================================================
static uint32 ComputeNumThreadZ(
    const Util::PalAbi::CodeObjectMetadata& metadata)
{
    const auto& hwCs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Cs)];
    COMPUTE_NUM_THREAD_Z computeNumThreadZ = { };

    computeNumThreadZ.bits.NUM_THREAD_FULL = hwCs.threadgroupDimensions[2];
    return computeNumThreadZ.u32All;
}

// =====================================================================================================================
static uint32 ComputePgmRsrc1(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    GfxIpLevel                              gfxLevel)
{
    COMPUTE_PGM_RSRC1 computePgmRsrc1 = { };

    const auto& hwCs    = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Cs)];
    const bool isWave32 = hwCs.hasEntry.wavefrontSize && (hwCs.wavefrontSize == 32);

    if (hwCs.hasEntry.vgprCount)
    {
        computePgmRsrc1.bits.VGPRS = CalcNumVgprs(hwCs.vgprCount, isWave32);
    }

    if (hwCs.hasEntry.sgprCount)
    {
        computePgmRsrc1.bits.SGPRS = CalcNumSgprs(hwCs.sgprCount);
    }

    computePgmRsrc1.bits.FLOAT_MODE = hwCs.floatMode;
    computePgmRsrc1.bits.FP16_OVFL  = hwCs.flags.fp16Overflow;
    computePgmRsrc1.bits.IEEE_MODE  = hwCs.flags.ieeeMode;
    computePgmRsrc1.bits.DEBUG_MODE = hwCs.flags.debugMode;
    computePgmRsrc1.bits.DX10_CLAMP = 1;

    if (IsGfx10Plus(gfxLevel))
    {
        computePgmRsrc1.gfx10Plus.WGP_MODE     = hwCs.flags.wgpMode;
        computePgmRsrc1.gfx10Plus.MEM_ORDERED  = hwCs.flags.memOrdered;
        computePgmRsrc1.gfx10Plus.FWD_PROGRESS = hwCs.flags.forwardProgress;
    }

    return computePgmRsrc1.u32All;
}

// =====================================================================================================================
static uint32 ComputePgmRsrc2(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device)
{
    COMPUTE_PGM_RSRC2 computePgmRsrc2 = { };

    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();
    const GfxIpLevel         gfxLevel  = chipProps.gfxLevel;

    const Util::PalAbi::PipelineMetadata& pipeline                 = metadata.pipeline;
    const Util::PalAbi::ComputeRegisterMetadata& pComputeRegisters = pipeline.computeRegister;
    const auto& hwCs = pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Cs)];

    computePgmRsrc2.bits.USER_SGPR      = hwCs.userSgprs;

    computePgmRsrc2.bits.EXCP_EN        = hwCs.excpEn;
    computePgmRsrc2.bits.EXCP_EN_MSB    = (hwCs.excpEn >= COMPUTE_PGM_RSRC2__EXCP_EN_MSB_MASK);

    computePgmRsrc2.bits.SCRATCH_EN     = hwCs.flags.scratchEn;

    computePgmRsrc2.bits.TIDIG_COMP_CNT = pComputeRegisters.tidigCompCnt;

    computePgmRsrc2.bits.TGID_X_EN = pComputeRegisters.flags.tgidXEn;
    computePgmRsrc2.bits.TGID_Y_EN = pComputeRegisters.flags.tgidYEn;
    computePgmRsrc2.bits.TGID_Z_EN = pComputeRegisters.flags.tgidZEn;

    computePgmRsrc2.bits.TG_SIZE_EN = pComputeRegisters.flags.tgSizeEn;

    uint32 allocateLdsSize = hwCs.ldsSize;
    computePgmRsrc2.bits.LDS_SIZE = allocateLdsSize / (sizeof(uint32) * Gfx9LdsDwGranularity);

    computePgmRsrc2.bits.TRAP_PRESENT = hwCs.flags.trapPresent;
    if (device.Parent()->LegacyHwsTrapHandlerPresent() && (chipProps.gfxLevel == GfxIpLevel::GfxIp9))
    {

        // If the legacy HWS's trap handler is present, compute shaders must always set the TRAP_PRESENT
        // flag.

        // TODO: Handle the case where the client enabled a trap handler and the hardware scheduler's trap handler
        // is already active!
        PAL_ASSERT(hwCs.flags.trapPresent == 0);
        computePgmRsrc2.bits.TRAP_PRESENT = 1;
    }

    return computePgmRsrc2.u32All;
}

// =====================================================================================================================
static uint32 ComputePgmRsrc3(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    size_t                                  shaderStageInfoCodeLength)
{
    COMPUTE_PGM_RSRC3 computePgmRsrc3 = {};

    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();
    const GfxIpLevel         gfxLevel  = chipProps.gfxLevel;

    const auto& hwCs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Cs)];
    if (IsGfx10Plus(gfxLevel))
    {
        computePgmRsrc3.bits.SHARED_VGPR_CNT = (hwCs.sharedVgprCnt) / 8;

#if  PAL_BUILD_GFX11
        if (IsGfx104Plus(gfxLevel))
        {
            computePgmRsrc3.gfx104Plus.INST_PREF_SIZE =
                device.GetShaderPrefetchSize(shaderStageInfoCodeLength);
        }
#endif

#if PAL_BUILD_GFX11
        // PWS+ only support pre-shader waits if the IMAGE_OP bit is set. Theoretically we only set it for shaders that
        // do an image operation. However that would mean that our use of the pre-shader PWS+ wait is dependent on us
        // only waiting on image resources, which we don't know in our interface. For now always set the IMAGE_OP bit
        // for corresponding shaders, making the pre-shader waits global.
        if (IsGfx11(gfxLevel))
        {
            computePgmRsrc3.gfx11.IMAGE_OP = 1;
        }
#endif
    }

    return computePgmRsrc3.u32All;
}

// =====================================================================================================================
static uint32 ComputeShaderChkSum(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device)
{
    COMPUTE_SHADER_CHKSUM chkSum = {};

    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();
    const GfxIpLevel         gfxLevel  = chipProps.gfxLevel;

    const auto& hwCs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Cs)];

    if (chipProps.gfx9.supportSpp && hwCs.hasEntry.checksumValue)
    {
        if (IsGfx9(gfxLevel))
        {
            chkSum.bits.CHECKSUM = hwCs.checksumValue;
        }
        else if (IsGfx10Plus(gfxLevel))
        {
            chkSum.bits.CHECKSUM = hwCs.checksumValue;
        }
    }
    return chkSum.u32All;
}

// =====================================================================================================================
static uint32 ComputeResourceLimits(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    const Device&                           device,
    uint32                                  wavefrontSize)
{
    COMPUTE_RESOURCE_LIMITS  computeResourceLimits = {};

    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();
    const GfxIpLevel         gfxLevel  = chipProps.gfxLevel;

    const auto& hwCs = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Cs)];

    if (IsGfx10Plus(gfxLevel))
    {
        computeResourceLimits.bits.WAVES_PER_SH = hwCs.wavesPerSe;
    }
    else
    {
        const uint32 numSaPerSe = chipProps.gfx9.numShaderArrays;
        computeResourceLimits.bits.WAVES_PER_SH = hwCs.wavesPerSe / numSaPerSe;
    }

    const uint32 threadsPerGroup =
        (hwCs.threadgroupDimensions[0] * hwCs.threadgroupDimensions[1] * hwCs.threadgroupDimensions[2]);
    const uint32 wavesPerGroup = Util::RoundUpQuotient(threadsPerGroup, wavefrontSize);

    // SIMD_DEST_CNTL: Controls which SIMDs thread groups get scheduled on.  If the number of
    // waves-per-TG is a multiple of 4, this should be 1, otherwise 0.
    computeResourceLimits.bits.SIMD_DEST_CNTL = ((wavesPerGroup % 4) == 0) ? 1 : 0;

    // Force even distribution on all SIMDs in CU for workgroup size is 64
    // This has shown some good improvements if #CU per SE not a multiple of 4
    if (((chipProps.gfx9.numShaderArrays * chipProps.gfx9.numCuPerSh) & 0x3) && (wavesPerGroup == 1))
    {
        computeResourceLimits.bits.FORCE_SIMD_DIST = 1;
    }

    const auto& settings = device.Settings();

    // LOCK_THRESHOLD: Sets per-SH low threshold for locking.  Set in units of 4, 0 disables locking.
    // LOCK_THRESHOLD's maximum value: (6 bits), in units of 4, so it is max of 252.
    constexpr uint32 Gfx9MaxLockThreshold = 252;
    PAL_ASSERT(settings.csLockThreshold <= Gfx9MaxLockThreshold);

#if PAL_BUILD_GFX11
    if (settings.waForceLockThresholdZero)
    {
        computeResourceLimits.bits.LOCK_THRESHOLD = 0;
    }
    else
#endif
    {
        computeResourceLimits.bits.LOCK_THRESHOLD =
            Util::Min((settings.csLockThreshold >> 2), (Gfx9MaxLockThreshold >> 2));
    }

    // SIMD_DEST_CNTL: Controls whichs SIMDs thread groups get scheduled on.  If no override is set, just keep
    // the existing value in COMPUTE_RESOURCE_LIMITS.
    switch (settings.csSimdDestCntl)
    {
    case CsSimdDestCntlForce1:
        computeResourceLimits.bits.SIMD_DEST_CNTL = 1;
        break;
    case CsSimdDestCntlForce0:
        computeResourceLimits.bits.SIMD_DEST_CNTL = 0;
        break;
    default:
        PAL_ASSERT(settings.csSimdDestCntl == CsSimdDestCntlDefault);
        break;
    }

    return computeResourceLimits.u32All;
}

#if PAL_BUILD_GFX11
constexpr uint32 DispatchInterleaveSizeLookupTable[] =
{
    64u,  // Default
    1u,   // Disable
    128u, // _128
    256u, // _256
    512u, // _512
};
static_assert((Util::ArrayLen32(DispatchInterleaveSizeLookupTable) ==
               static_cast<uint32>(DispatchInterleaveSize::Count)),
              "DispatchInterleaveSizeLookupTable and DispatchInterleaveSize don't have the same number of elements.");
static_assert((DispatchInterleaveSizeLookupTable[static_cast<uint32>(DispatchInterleaveSize::Default)] ==
               Gfx11::mmCOMPUTE_DISPATCH_INTERLEAVE_DEFAULT),
              "DispatchInterleaveSizeLookupTable looks up incorrect value for DispatchInterleaveSize::Default.");
static_assert((DispatchInterleaveSizeLookupTable[static_cast<uint32>(DispatchInterleaveSize::_128)] == 128u),
              "DispatchInterleaveSizeLookupTable looks up incorrect value for DispatchInterleaveSize::_128.");
static_assert((DispatchInterleaveSizeLookupTable[static_cast<uint32>(DispatchInterleaveSize::_256)] == 256u),
              "DispatchInterleaveSizeLookupTable looks up incorrect value for DispatchInterleaveSize::_128.");
static_assert((DispatchInterleaveSizeLookupTable[static_cast<uint32>(DispatchInterleaveSize::_512)] == 512u),
              "DispatchInterleaveSizeLookupTable looks up incorrect value for DispatchInterleaveSize::_128.");
// Panel setting validation for OverrideCsDispatchInterleaveSize
static_assert((static_cast<uint32>(OverrideCsDispatchInterleaveSizeDisabled) ==
               static_cast<uint32>(DispatchInterleaveSize::Disable)),
              "OverrideCsDispatchInterleaveSizeDisabled and DispatchInterleaveSize::Disable do not match.");
static_assert((static_cast<uint32>(OverrideCsDispatchInterleaveSize128) ==
               static_cast<uint32>(DispatchInterleaveSize::_128)),
              "OverrideCsDispatchInterleaveSize128 and DispatchInterleaveSize::_128 do not match.");
static_assert((static_cast<uint32>(OverrideCsDispatchInterleaveSize256) ==
               static_cast<uint32>(DispatchInterleaveSize::_256)),
              "OverrideCsDispatchInterleaveSize256 and DispatchInterleaveSize::_256 do not match.");
static_assert((static_cast<uint32>(OverrideCsDispatchInterleaveSize512) ==
               static_cast<uint32>(DispatchInterleaveSize::_512)),
              "OverrideCsDispatchInterleaveSize512 and DispatchInterleaveSize::_512 do not match.");

// =====================================================================================================================
static uint32 ComputeDispatchInterleave(
    const Device&           device,
    DispatchInterleaveSize  interleaveSize)
{
    COMPUTE_DISPATCH_INTERLEAVE computeDispatchInterleave = {};

    const auto& settings               = device.Settings();
    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();
    const GfxIpLevel         gfxLevel  = chipProps.gfxLevel;

    if (IsGfx11(gfxLevel))
    {
        const uint32 lookup = (settings.overrideCsDispatchInterleaveSize != CsDispatchInterleaveSizeHonorClient)
            ? static_cast<uint32>(settings.overrideCsDispatchInterleaveSize)
            : static_cast<uint32>(interleaveSize);
        computeDispatchInterleave.bits.INTERLEAVE = DispatchInterleaveSizeLookupTable[lookup];
    }

    return computeDispatchInterleave.u32All;
}
#endif
} // namespace AbiRegisters
} // namespace Gfx9
} // namespace Pal
