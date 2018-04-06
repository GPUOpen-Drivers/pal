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
* @file  g_gfx9PalSettings.h
* @brief auto-generated file.
*        Contains the definition for the PAL settings struct and enums for initialization.
***************************************************************************************************
*/
#pragma once
#include "core/g_palSettings.h"

namespace Pal
{
namespace Gfx9
{

enum CsSimdDestCntlMode : uint32
{
    CsSimdDestCntlDefault = 0,
    CsSimdDestCntlForce1 = 1,
    CsSimdDestCntlForce0 = 2,

};

enum PrefetchMethod : uint32
{
    PrefetchCpDma = 0,
    PrefetchPrimeUtcL2 = 1,

};

enum Gfx9OffchipLdsBufferSize : uint32
{
    Gfx9OffchipLdsBufferSize8192 = 0,
    Gfx9OffchipLdsBufferSize4096 = 1,
    Gfx9OffchipLdsBufferSize2048 = 2,
    Gfx9OffchipLdsBufferSize1024 = 3,

};

enum Gfx9UseDcc : uint32
{
    Gfx9UseDccSingleSample = 0x00000001,
    Gfx9UseDccSrgb = 0x00000002,
    Gfx9UseDccNonTcCompatShaderRead = 0x00000004,
    Gfx9UseDccPrt = 0x00000008,
    Gfx9UseDccMultiSample2x = 0x00000010,
    Gfx9UseDccMultiSample4x = 0x00000020,
    Gfx9UseDccMultiSample8x = 0x00000040,
    Gfx9UseDccEqaa = 0x00000080,
    Gfx10UseDccUav = 0x00000100,

};

enum Gfx9DecompressMask : uint32
{
    Gfx9DecompressDcc = 0x00000001,
    Gfx9DecompressHtile = 0x00000002,
    Gfx9DecompressFmask = 0x00000004,
    Gfx9DecompressFastClear = 0x00000008,

};

enum Gfx9DccOnCompute : uint32
{
    Gfx9DccOnComputeInit = 0x00000001,
    Gfx9DccOnComputeFastClear = 0x00000002,

};

enum Gfx9OutOfOrderPrimMode : uint32
{
    Gfx9OutOfOrderPrimDisable = 0,
    Gfx9OutOfOrderPrimSafe = 1,
    Gfx9OutOfOrderPrimAggressive = 2,
    Gfx9OutOfOrderPrimAlways = 3,

};

enum Gfx9BinningMode : uint32
{
    Gfx9DeferredBatchBinDisabled = 0,
    Gfx9DeferredBatchBinCustom = 1,
    Gfx9DeferredBatchBinAccurate = 2,

};

enum Gfx9InitMetaDataFill : uint32
{
    Gfx9InitMetaDataFillDcc = 0x00000001,
    Gfx9InitMetaDataFillCmask = 0x00000002,
    Gfx9InitMetaDataFillHtile = 0x00000004,

};

enum Gfx9PrintMetaEquationInfo : uint32
{
    Gfx9PrintMetaEquationInfoEquations = 0x00000001,
    Gfx9PrintMetaEquationInfoProcessing = 0x00000002,

};

enum Gfx9NggMode : uint32
{
    Gfx9NggDisabled = 0x00,
    Gfx9NggEnableInternal = 0x01,
    Gfx9NggEnableExternal = 0x02,
    Gfx9NggEnableClient = 0x04,
    Gfx9NggEnableAll = 0x07,

};

enum Gfx9WdLoadBalancingMode : uint32
{
    Gfx9WdLoadBalancingDisabled = 0x0,
    Gfx9WdLoadBalancingBasic = 0x1,
    Gfx9WdLoadBalancingAdvanced = 0x2,

};

enum Gfx9OffChipHsCopyMethod : uint32
{
    Gfx9OffChipHsImmediate = 0x0,
    Gfx9OffChipHsCopyAllAtEnd = 0x1,
    Gfx9OffChipHsCopyMultiElements = 0x2,

};

enum Gfx9OptimizedFastClear : uint32
{
    Gfx9OptimizedFastClearDisabled = 0x00000000,
    Gfx9OptimizedFastClearColorCmask = 0x00000001,
    Gfx9OptimizedFastClearColorDcc = 0x00000002,
    Gfx9OptimizedFastClearDepth = 0x00000004,

};

enum Gfx9PrefetchCommands : uint32
{
    Gfx9PrefetchCommandsDisabled = 0,
    Gfx9PrefetchCommandsBuildInfo = 1,
    Gfx9PrefetchCommandsForceAllDe = 2,
    Gfx9PrefetchCommandsForceAllDeAce = 3,

};

struct Gfx9PalSettings : public PalSettings
{
    bool    disableBorderColorPaletteBinds;
    bool    drainPsOnOverlap;
    uint32    printMetaEquationInfo;
    bool    processMetaEquationViaCpu;
    uint32    optimizedFastClear;
    uint32    alwaysDecompress;
    bool    treat1dAs2d;
    bool    forceGraphicsFillMemoryPath;
    bool    waitOnMetadataMipTail;
    bool    blendOptimizationsEnable;
    bool    fastColorClearEnable;
    bool    fastColorClearOn3dEnable;
    bool    fmaskCompressDisable;
    uint32    dccOnComputeEnable;
    uint32    useDcc;
    uint32    csMaxWavesPerCu;
    uint32    csLockThreshold;
    CsSimdDestCntlMode    csSimdDestCntl;
    bool    htileEnable;
    bool    depthCompressEnable;
    bool    stencilCompressEnable;
    bool    dbPreloadEnable;
    bool    dbPreloadWinEnable;
    bool    dbPerTileExpClearEnable;
    bool    hiDepthEnable;
    bool    hiStencilEnable;
    uint32    dbRequestSize;
    bool    dbDisableColorOnValidation;
    bool    enableOnchipGs;
    Gfx9OutOfOrderPrimMode    enableOutOfOrderPrimitives;
    uint32    outOfOrderWatermark;
    uint32    gfxMaxWavesPerCu;
    bool    gsCuGroupEnabled;
    uint32    gsMaxLdsSize;
    uint32    gsOffChipThreshold;
    uint32    idealGsPrimsPerSubGroup;
    uint32    lateAllocGs;
    float    maxTessFactor;
    uint32    numOffchipLdsBuffers;
    uint32    numTessPatchesPerTg;
    uint32    offchipLdsBufferSize;
    uint32    isolineDistributionFactor;
    uint32    triDistributionFactor;
    uint32    quadDistributionFactor;
    uint32    donutDistributionFactor;
    uint32    trapezoidDistributionFactor;
    uint32    primGroupSize;
    bool    psCuGroupEnabled;
    bool    gfx9RbPlusEnable;
    bool    switchVgtOnDraw;
    uint32    tessFactorBufferSizePerSe;
    uint32    disableTessDonutWalkPattern;
    bool    useMaxOffchipLdsBuffers;
    bool    vsCuGroupEnabled;
    uint32    vsHalfPackThreshold;
    bool    vsForcePartialWave;
    bool    disableCoverageAaMask;
    Gfx9WdLoadBalancingMode    wdLoadBalancingMode;
    bool    batchBreakOnNewPixelShader;
    uint32    gsCuEnLimitMask;
    uint32    vsCuEnLimitMask;
    uint32    psCuEnLimitMask;
    Gfx9OffChipHsCopyMethod    gfx9OffChipHsCopyMethod;
    bool    gfx9OffChipHsSkipDataCopyNullPatch;
    bool    gfx9OptimizeDsDataFetch;
    Gfx9NggMode    nggMode;
    uint32    nggRegLaunchGsPrimsPerSubgrp;
    uint32    idealNggFastLaunchWavesPerSubgrp;
    uint32    nggLateAllocGs;
    bool    nggDisableBackfaceCulling;
    bool    nggEnableFrustumCulling;
    bool    nggEnableSmallPrimFilter;
    bool    nggEnableFasterLaunchRate;
    bool    enableOrderedIdMode;
    uint64    nggFastLaunchPipelineHash;
    uint64    nggVertexReusePipelineHash;
    uint32    nggRingSize;
    Gfx9BinningMode    binningMode;
    uint32    customBatchBinSize;
    bool    disableBinningPsKill;
    bool    disableBinningNoDb;
    bool    disableBinningBlendingOff;
    uint32    binningMaxAllocCountLegacy;
    uint32    binningMaxAllocCountNggOnChip;
    uint32    binningMaxPrimPerBatch;
    uint32    binningContextStatesPerBin;
    uint32    binningPersistentStatesPerBin;
    uint32    binningFpovsPerBatch;
    bool    binningOptimalBinSelection;
    bool    disableBinningAppendConsume;
    bool    disableDfsm;
    bool    disableDfsmPsUav;
    PrefetchMethod    shaderPrefetchMethod;
    Gfx9PrefetchCommands    prefetchCommandBuffers;
    bool    anisoFilterOptEnabled;
    bool    samplerCeilingLogicEnabled;
    bool    samplerPrecisionFixEnabled;
    uint32    samplerPerfMip;
    uint32    samplerAnisoThreshold;
    uint32    samplerAnisoBias;
    uint32    samplerSecAnisoBias;
    bool    waWrite1xAASampleLocationsToZero;
    bool    waColorCacheControllerInvalidEviction;
    bool    waOverwriteCombinerTargetMaskOnly;
    bool    waDisableHtilePrefetch;
    bool    waMiscPopsMissedOverlap;
    bool    waMiscScissorRegisterChange;
    bool    waMiscPsFlushScissorChange;
    bool    waHtilePipeBankXorMustBeZero;
    bool    waDisableDfsmWithEqaa;
    bool    waLegacyTessToNggVgtFlush;
    bool    waNggWdPageFault;
    bool    waLegacyToNggVsPartialFlush;
    bool    waDisable24BitHWFormatForTCCompatibleDepth;
    bool    waDummyZpassDoneBeforeTs;
    bool    waMetaAliasingFixEnabled;

};


static const char* pGfx9DisableBorderColorPaletteBindsStr = "#4032484395";
static const char* pGfx9DrainPsOnOverlapStr = "#152448562";
static const char* pGfx9PrintMetaEquationInfoStr = "#3253866853";
static const char* pGfx9ProcessMetaEquationViaCpuStr = "#2803972389";
static const char* pGfx9OptimizedFastClearStr = "#2912214255";
static const char* pGfx9AlwaysDecompressStr = "#2524848317";
static const char* pTreat1dAs2dStr = "#648332656";
static const char* pGfx9ForceGraphicsFillMemoryPathStr = "#1387491670";
static const char* pGfx9WaitOnMetadataMipTailStr = "#3286240530";
static const char* pGfx9BlendOptimizationEnableStr = "#522975064";
static const char* pGfx9FastColorClearEnableStr = "#2371341558";
static const char* pGfx9FastColorClearOn3DEnableStr = "#2362017720";
static const char* pGfx9FmaskCompressDisableStr = "#3793345169";
static const char* pGfx9DccOnComputeEnableStr = "#536295180";
static const char* pGfx9UseDccStr = "#562025936";
static const char* pGfx9CsMaxWavesPerCuStr = "#2575501128";
static const char* pGfx9CsLockThresholdStr = "#2815557665";
static const char* pGfx9CsSimdDestCntlStr = "#3026220909";
static const char* pGfx9HtileEnableStr = "#307687750";
static const char* pGfx9DepthCompressEnableStr = "#3595964543";
static const char* pGfx9StencilCompressEnableStr = "#1980889386";
static const char* pGfx9DbPreloadEnableStr = "#3211778437";
static const char* pGfx9DbPreloadWinEnableStr = "#1522517871";
static const char* pGfx9DbPerTileExpClearEnableStr = "#3501215503";
static const char* pGfx9HiDepthEnableStr = "#849586198";
static const char* pGfx9HiStencilEnableStr = "#1805524835";
static const char* pGfx9DbRequestSizeStr = "#3547313499";
static const char* pGfx9DbDisableColorOnValidationStr = "#558448268";
static const char* pGfx9EnableOnchipGsStr = "#3294293377";
static const char* pGfx9EnableOutOfOrderPrimitivesStr = "#2065357817";
static const char* pGfx9OutOfOrderWatermarkStr = "#1839281642";
static const char* pGfx9GfxMaxWavesPerCuStr = "#1283469649";
static const char* pGfx9GsCuGroupEnabledStr = "#4156712597";
static const char* pGfx9GsMaxLdsSizeStr = "#1336428223";
static const char* pGfx9GsOffChipThresholdStr = "#2274962091";
static const char* pGfx9GsPrimsPerSubGroupStr = "#622742320";
static const char* pGfx9LateAllocGsStr = "#500094222";
static const char* pGfx9MaxTessFactorStr = "#3454740625";
static const char* pGfx9NumOffchipLdsBuffersStr = "#2896241920";
static const char* pGfx9NumTessPatchesPerThreadGroupStr = "#3807746724";
static const char* pGfx9OffchipLdsBufferSizeStr = "#4265805768";
static const char* pGfx9IsolineDistributionFactorStr = "#4075672123";
static const char* pGfx9TriDistributionFactorStr = "#1693673697";
static const char* pGfx9QuadDistributionFactorStr = "#169905281";
static const char* pGfx9DonutDistributionFactorStr = "#2385212232";
static const char* pGfx9TrapezoidDistributionFactorStr = "#338431624";
static const char* pGfx9PrimgroupSizeStr = "#3662550551";
static const char* pGfx9PsCuGroupEnabledStr = "#1161534418";
static const char* pGfx9RbPlusEnableStr = "#639180784";
static const char* pGfx9SwitchVgtOnDrawStr = "#3454725195";
static const char* pGfx9TessFactorBufferSizePerSeStr = "#400732309";
static const char* pGfx9TessWalkPatternStr = "#409766951";
static const char* pGfx9UseMaxOffchipLdsBuffersStr = "#1604056243";
static const char* pGfx9VsCuGroupEnabledStr = "#1373800440";
static const char* pGfx9VsHalfPackThresholdStr = "#2495259643";
static const char* pGfx9VsForcePartialWaveStr = "#1346961557";
static const char* pGfx9DisableCoverageAaMaskStr = "#3826833505";
static const char* pGfx9WdLoadBalancingModeStr = "#3279973116";
static const char* pBatchBreakOnNewPixelShaderStr = "#3367458304";
static const char* pGfx9GsCuEnLimitMaskStr = "#1209868797";
static const char* pGfx9VsCuEnLimitMaskStr = "#1593704846";
static const char* pGfx9PsCuEnLimitMaskStr = "#1073203284";
static const char* pGfx9OffChipHsCopyMethodStr = "#2447823699";
static const char* pGfx9OffChipHsSkipDataCopyNullPatchStr = "#552599054";
static const char* pGfx9OptimizeDsDataFetchStr = "#4239968103";
static const char* pGfx9NggModeStr = "#3564556450";
static const char* pGfx9NggRegLaunchGsPrimsPerSubgrpStr = "#3876424429";
static const char* pGfx9NggFastLaunchWavesPerSubgrpStr = "#588781428";
static const char* pGfx9NggLateAllocGsStr = "#25680884";
static const char* pGfx9NggDisableBackfaceCullingStr = "#3013509235";
static const char* pGfx9NggEnableFrustumCullingStr = "#2950286264";
static const char* pGfx9NggEnableSmallPrimFilterStr = "#464933711";
static const char* pGfx9NggEnableFasterLaunchRateStr = "#1042489726";
static const char* pGfx9EnableOrderedIdModeStr = "#2734740323";
static const char* pNggFastLaunchPipelineHashStr = "#3629124844";
static const char* pNggVertexReusePipelineHashStr = "#830275381";
static const char* pGfx9NggRingSizeStr = "#3892861530";
static const char* pGfx9DeferredBatchBinModeStr = "#3574331626";
static const char* pGfx9CustomBatchBinSizeStr = "#2849727856";
static const char* pGfx9DisableBinningPsKillStr = "#3256572933";
static const char* pGfx9DisableBinningNoDbStr = "#3837697457";
static const char* pGfx9DisableBinningBlendingOffStr = "#1253898010";
static const char* pGfx9BinningMaxAllocCountLegacyStr = "#3595245251";
static const char* pGfx9BinningMaxAllocCountNggOnChipStr = "#3074921727";
static const char* pGfx9BinningMaxPrimPerBatchStr = "#2052449";
static const char* pGfx9BinningContextStatesPerBinStr = "#608721679";
static const char* pGfx9BinningPersistentStatesPerBinStr = "#1552476445";
static const char* pGfx9BinningFpovsPerBatchStr = "#2355869131";
static const char* pGfx9BinningOptimalBinSelectionStr = "#3624290521";
static const char* pGfx9BinningDisableBinningAppendConsumeStr = "#1364069097";
static const char* pGfx9DisableDfsmStr = "#2613455003";
static const char* pGfx9DisableDfsmPsUavStr = "#2160640426";
static const char* pShaderPrefetchMethodStr = "#3028994822";
static const char* pGfx9PrefetchCommandBuffersStr = "#1463768592";
static const char* pGfx9AnisoFilterOptEnabledStr = "#954244333";
static const char* pGfx9CeilingLogicEnabledStr = "#715773477";
static const char* pGfx9PrecisionFixEnabledStr = "#1572193513";
static const char* pGfx9SamplerPerfMipStr = "#4250234692";
static const char* pGfx9SamplerAnisoThresholdStr = "#3251759486";
static const char* pGfx9SamplerAnisoBiasStr = "#2835153978";
static const char* pGfx9SamplerSecAnisoBiasStr = "#2554508743";
static const char* pWaWrite1xAASampleLocationsToZeroStr = "#2042380720";
static const char* pWaColorCacheControllerInvalidEvictionStr = "#2330368444";
static const char* pWaOverwriteCombinerTargetMaskOnlyStr = "#1670732044";
static const char* pWaDisableHtilePrefetchStr = "#4011209522";
static const char* pWaMiscPopsMissedOverlapStr = "#2170713611";
static const char* pWaMiscScissorRegisterChangeStr = "#3580876344";
static const char* pWaMiscPsFlushScissorChangeStr = "#2611268564";
static const char* pWaHtilePipeBankXorMustBeZeroStr = "#264312760";
static const char* pWaDisableDfsmWithEqaaStr = "#3435751213";
static const char* pWaLegacyTessToNggVgtFlushStr = "#833237955";
static const char* pWaNggWdPageFaultStr = "#677348793";
static const char* pWaLegacyToNggVsPartialFlushStr = "#2643144139";
static const char* pWaDisable24BitHWFormatForTCCompatibleDepthStr = "#1925370123";
static const char* pWaDummyZpassDoneBeforeTsStr = "#814916412";
static const char* pWaMetaAliasingFixEnabledStr = "#3182155668";

} // Gfx9
} // Pal
namespace Pal { class Device; }
extern const Pal::Gfx9::Gfx9PalSettings& GetGfx9Settings(const Pal::Device& device);
