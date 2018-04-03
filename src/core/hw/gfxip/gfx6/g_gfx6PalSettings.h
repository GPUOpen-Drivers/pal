/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!
//
// This code has been generated automatically. Do not hand-modify this code.
//
// When changes are needed, modify the tools generating this module in the tools\generate directory OR settings.cfg
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!  WARNING!  WARNING!
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/**
***************************************************************************************************
* @file  g_gfx6PalSettings.h
* @brief auto-generated file.
*        Contains the definition for the PAL settings struct and enums for initialization.
***************************************************************************************************
*/
#pragma once
#include "core/g_palSettings.h"

namespace Pal
{
namespace Gfx6
{

enum CsSimdDestCntlMode : uint32
{
    CsSimdDestCntlDefault = 0,
    CsSimdDestCntlForce1 = 1,
    CsSimdDestCntlForce0 = 2,

};

enum CpDmaAlignmentMode : uint32
{
    CpDmaAlignmentDefault = sizeof(uint32),
    CpDmaAlignmentOptimal = 32,

};

enum PrefetchMethod : uint32
{
    PrefetchCpDma = 0,

};

enum Gfx7OffchipLdsBufferSize : uint32
{
    Gfx7OffchipLdsBufferSize8192 = 0,
    Gfx7OffchipLdsBufferSize4096 = 1,
    Gfx7OffchipLdsBufferSize2048 = 2,
    Gfx7OffchipLdsBufferSize1024 = 3,

};

enum Gfx8DecompressMask : uint32
{
    Gfx8DecompressDcc = 0x00000001,
    Gfx8DecompressHtile = 0x00000002,
    Gfx8DecompressFmask = 0x00000004,
    Gfx8DecompressFastClear = 0x00000008,

};

enum Gfx8TcComaptDbFlushWorkaround : uint32
{
    Gfx8TcCompatDbFlushWaNever = 0x00000000,
    Gfx8TcCompatDbFlushWaNormal = 0x00000001,
    Gfx8TcCompatDbFlushWaAlways = 0x00000002,

};

enum Gfx8FastClearTcCompatSurfs : uint32
{
    Gfx8FastClearAllTcCompatColorSurfsNever = 0x00000000,
    Gfx8FastClearAllTcCompatColorSurfsNoAa = 0x00000001,
    Gfx8FastClearAllTcCompatColorSurfsMsaa = 0x00000002,

};

enum Gfx8TcCompatibleResolveDst : uint32
{
    Gfx8TcCompatibleResolveDstDepthOnly = 0x00000001,
    Gfx8TcCompatibleResolveDstStencilOnly = 0x00000002,
    Gfx8TcCompatibleResolveDstDepthAndStencil = 0x00000004,

};

enum Gfx8CheckMetaDataFetchFromStartMip : uint32
{
    Gfx8CheckMetaDataFetchFromStartMipDepthStencil = 0x00000001,
    Gfx8CheckMetaDataFetchFromStartMipColorTarget = 0x00000002,

};

enum Gfx8UseDcc : uint32
{
    Gfx8UseDccSingleSample = 0x00000001,
    Gfx8UseDccSrgb = 0x00000002,
    Gfx8UseDccNonTcCompatShaderRead = 0x00000004,
    Gfx8UseDccPrt = 0x00000008,
    Gfx8UseDccMultiSample2x = 0x00000010,
    Gfx8UseDccMultiSample4x = 0x00000020,
    Gfx8UseDccMultiSample8x = 0x00000040,
    Gfx8UseDccEqaa = 0x00000080,

};

enum Gfx7OutOfOrderPrimMode : uint32
{
    Gfx7OutOfOrderPrimDisable = 0,
    Gfx7OutOfOrderPrimSafe = 1,
    Gfx7OutOfOrderPrimAggressive = 2,
    Gfx7OutOfOrderPrimAlways = 3,

};

struct Gfx6PalSettings : public PalSettings
{
    uint32    cpDmaSrcAlignment;
    bool    disableBorderColorPaletteBinds;
    bool    forceOcclusionQueryResult;
    uint32    forceOcclusionQueryResultValue;
    uint32    primGroupSize;
    bool    dynamicPrimGroupEnable;
    uint32    dynamicPrimGroupMin;
    uint32    dynamicPrimGroupMax;
    uint32    dynamicPrimGroupStep;
    uint32    dynamicPrimGroupWindowSize;
    bool    switchVgtOnDraw;
    uint32    esGsRatio;
    bool    vsForcePartialWave;
    uint32    vsHalfPackThreshold;
    bool    esCuGroupEnabled;
    bool    gsCuGroupEnabled;
    bool    vsCuGroupEnabled;
    bool    psCuGroupEnabled;
    uint32    tessFactorBufferSizePerSe;
    uint32    disableTessDonutWalkPattern;
    float    maxTessFactor;
    uint32    numTessPatchesPerTg;
    uint32    numOffchipLdsBuffers;
    bool    useMaxOffchipLdsBuffers;
    uint32    dsWavesPerSimdOverflow;
    float    offchipTfDegree;
    bool    gfx6OffChipHsSkipDataCopyNullPatch;
    uint32    gfxMaxWavesPerCu;
    uint32    lsCuEnLimitMask;
    uint32    esCuEnLimitMask;
    uint32    gsCuEnLimitMask;
    uint32    vsCuEnLimitMask;
    uint32    psCuEnLimitMask;
    uint32    csMaxWavesPerCu;
    uint32    csLockThreshold;
    CsSimdDestCntlMode    csSimdDestCntl;
    PrefetchMethod    shaderPrefetchMethod;
    bool    anisoFilterOptEnabled;
    bool    samplerCeilingLogicEnabled;
    bool    samplerPrecisionFixEnabled;
    uint32    samplerPerfMip;
    uint32    samplerAnisoThreshold;
    uint32    samplerAnisoBias;
    uint32    samplerSecAnisoBias;
    bool    fastColorClearEnable;
    bool    fastColorClearOn3dEnable;
    bool    fmaskCompressEnable;
    bool    blendOptimizationsEnable;
    bool    htileEnable;
    bool    depthCompressEnable;
    bool    stencilCompressEnable;
    bool    linearHtileEnable;
    bool    dbPreloadEnable;
    bool    dbPreloadWinEnable;
    bool    dbPerTileExpClearEnable;
    bool    hiDepthEnable;
    bool    hiStencilEnable;
    uint32    dbRequestSize;
    uint32    dbAddr5SwizzleMask;
    bool    dbDisableColorOnValidation;
    bool    enableSeparateAspectMetadataInit;
    bool    gfx7VsPartialWaveWithEoiEnabled;
    uint32    gfx7OffchipLdsBufferSize;
    bool    gfx7LateAllocVsOnCuAlwaysOn;
    Gfx7OutOfOrderPrimMode    gfx7EnableOutOfOrderPrimitives;
    uint32    gfx7OutOfOrderWatermark;
    uint32    gfx7GsMaxLdsSize;
    bool    gfx7EnableOnchipGs;
    uint32    gfx7GsOffChipThreshold;
    uint32    gfx7IdealGsPrimsPerSubGroup;
    bool    gfx7AvoidVgtNullPrims;
    uint32    gfx8PatchDistributionFactor;
    uint32    gfx8DonutDistributionFactor;
    uint32    gfx8TrapezoidDistributionFactor;
    uint32    gfx8UseDcc;
    uint32    gfx8AlwaysDecompress;
    bool    gfx8RbPlusEnable;
    uint32    gfx8FastClearAllTcCompatColorSurfs;
    uint32    gfx8CheckMetaDataFetchFromStartMip;
    bool    gfx8IgnoreMipInterleave;
    bool    waMiscGsNullPrim;

};


static const char* pGfx6CpDmaSrcAlignmentStr = "#1913440786";
static const char* pGfx6DisableBorderColorPaletteBindsStr = "#3604775650";
static const char* pGfx6ForceOcclusionQueryResultStr = "#538889227";
static const char* pGfx6ForceOcclusionQueryResultValueStr = "#3045821480";
static const char* pGfx6PrimgroupSizeStr = "#3004585776";
static const char* pGfx6DynamicPrimgroupEnableStr = "#4111244369";
static const char* pGfx6DynamicPrimgroupMinStr = "#2539030230";
static const char* pGfx6DynamicPrimgroupMaxStr = "#2305526660";
static const char* pGfx6DynamicPrimgroupStepStr = "#353841984";
static const char* pGfx6DynamicPrimgroupWindowSizeStr = "#4022580943";
static const char* pGfx6SwitchVgtOnDrawStr = "#2280046068";
static const char* pGfx6EsGsRatioStr = "#95530903";
static const char* pGfx6VsForcePartialWaveStr = "#1484263188";
static const char* pGfx6VsHalfPackThresholdStr = "#2052187816";
static const char* pGfx6EsCuGroupEnabledStr = "#4100775526";
static const char* pGfx6GsCuGroupEnabledStr = "#859002704";
static const char* pGfx6VsCuGroupEnabledStr = "#744439277";
static const char* pGfx6PsCuGroupEnabledStr = "#2267496463";
static const char* pGfx6TessFactorBufferSizePerSeStr = "#2243302998";
static const char* pGfx6TessWalkPatternStr = "#2545179316";
static const char* pGfx6MaxTessFactorStr = "#1519012702";
static const char* pGfx6NumTessPatchesPerThreadGroupStr = "#2788271741";
static const char* pGfx6NumOffchipLdsBuffersStr = "#2954846885";
static const char* pGfx6UseMaxOffchipLdsBuffersStr = "#1557810800";
static const char* pGfx6TessDsWavesPerSimdOverflowStr = "#3652930604";
static const char* pGfx6TessOffchipTfDegreeStr = "#1679590436";
static const char* pGfx6OffChipHsSkipDataCopyNullPatchStr = "#3034493711";
static const char* pGfx6GfxMaxWavesPerCuStr = "#654922048";
static const char* pGfx6LsCuEnLimitMaskStr = "#3426572391";
static const char* pGfx6EsCuEnLimitMaskStr = "#1487696080";
static const char* pGfx6GsCuEnLimitMaskStr = "#4057258198";
static const char* pGfx6VsCuEnLimitMaskStr = "#1202383653";
static const char* pGfx6PsCuEnLimitMaskStr = "#3198408779";
static const char* pGfx6CsMaxWavesPerCuStr = "#993695991";
static const char* pGfx6CsLockThresholdStr = "#2168282538";
static const char* pGfx6CsSimdDestCntlStr = "#3624499520";
static const char* pShaderPrefetchMethodStr = "#3028994822";
static const char* pGfx6AnisoFilterOptEnabledStr = "#1617765794";
static const char* pGfx6CeilingLogicEnabledStr = "#427286750";
static const char* pGfx6PrecisionFixEnabledStr = "#2158975290";
static const char* pGfx6SamplerPerfMipStr = "#3308054525";
static const char* pGfx6SamplerAnisoThresholdStr = "#1445871077";
static const char* pGfx6SamplerAnisoBiasStr = "#1639630575";
static const char* pGfx6SamplerSecAnisoBiasStr = "#1351672360";
static const char* pGfx6FastColorClearEnableStr = "#3488495751";
static const char* pGfx6FastColorClearOn3DEnableStr = "#3907531045";
static const char* pGfx6FmaskCompressEnableStr = "#1207214711";
static const char* pGfx6BlendOptimizationEnableStr = "#3541917647";
static const char* pGfx6HtileEnableStr = "#1627997729";
static const char* pGfx6DepthCompressEnableStr = "#2537616448";
static const char* pGfx6StencilCompressEnableStr = "#12125549";
static const char* pGfx6LinearHtileEnableStr = "#3520186222";
static const char* pGfx6DbPreloadEnableStr = "#2450768758";
static const char* pGfx6DbPreloadWinEnableStr = "#3594057978";
static const char* pGfx6DbPerTileExpClearEnableStr = "#4197650500";
static const char* pGfx6HiDepthEnableStr = "#477226497";
static const char* pGfx6HiStencilEnableStr = "#1955039940";
static const char* pGfx6DbRequestSizeStr = "#3226287240";
static const char* pGfx6DbAddr5SwizzleMaskStr = "#581553452";
static const char* pGfx6DbDisableColorOnValidationStr = "#1904824389";
static const char* pGfx6EnableSeparateAspectMetadataInitStr = "#906697807";
static const char* pGfx7VsPartialWaveWithEoiEnabledStr = "#3482032926";
static const char* pGfx7OffchipLdsBufferSizeStr = "#2523963198";
static const char* pGfx7LateAllocVsOnCuAlwaysOnStr = "#129018316";
static const char* pGfx7EnableOutOfOrderPrimitivesStr = "#3860809287";
static const char* pGfx7OutOfOrderWatermarkStr = "#813361688";
static const char* pGfx7GsMaxLdsSizeStr = "#447582277";
static const char* pGfx7EnableOnchipGsStr = "#635082831";
static const char* pGfx7GsOffChipThresholdStr = "#3003734953";
static const char* pGfx7GsPrimsPerSubGroupStr = "#801607194";
static const char* pGfx7AvoidVgtNullPrimsStr = "#3286306707";
static const char* pGfx8PatchDistributionFactorStr = "#2684519855";
static const char* pGfx8DonutDistributionFactorStr = "#2931378985";
static const char* pGfx8TrapezoidDistributionFactorStr = "#3726792085";
static const char* pGfx8UseDccStr = "#3691235539";
static const char* pGfx8AlwaysDecompressStr = "#1324897786";
static const char* pGfx8RbPlusEnableStr = "#3682844315";
static const char* pGfx8FastClearAllTcCompatColorSurfsStr = "#3275131005";
static const char* pGfx8CheckMetaDataFetchFromStartMipStr = "#3191609681";
static const char* pGfx8IgnoreMipInterleaveStr = "#302504781";
static const char* pWaMiscGsNullPrimStr = "#203570314";

} // Gfx6
} // Pal
namespace Pal { class Device; }
extern const Pal::Gfx6::Gfx6PalSettings& GetGfx6Settings(const Pal::Device& device);
