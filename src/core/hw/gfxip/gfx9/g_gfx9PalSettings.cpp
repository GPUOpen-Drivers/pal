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


#include "core/hw/gfxip/gfx9/gfx9SettingsLoader.h"
#include "core/hw/gfxip/gfx9/g_gfx9PalSettings.h"
#include "palInlineFuncs.h"
#include "core/device.h"

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
// Initializes the settings structure to default values.
void SettingsLoader::Gfx9SetupDefaults(
    Gfx9PalSettings* pSettings)
{
    // set setting variables to their default values...
    pSettings->disableBorderColorPaletteBinds = false;
    pSettings->drainPsOnOverlap = false;
    pSettings->printMetaEquationInfo = 0;
    pSettings->processMetaEquationViaCpu = false;
    pSettings->optimizedFastClear = Gfx9OptimizedFastClearColorCmask | Gfx9OptimizedFastClearColorDcc | Gfx9OptimizedFastClearDepth;
    pSettings->alwaysDecompress = 0;
    pSettings->treat1dAs2d = true;
    pSettings->forceGraphicsFillMemoryPath = false;
    pSettings->waitOnMetadataMipTail = true;
    pSettings->blendOptimizationsEnable = true;
    pSettings->fastColorClearEnable = true;
    pSettings->fastColorClearOn3dEnable = true;
    pSettings->fmaskCompressDisable = false;
    pSettings->dccOnComputeEnable = 3;
    pSettings->useDcc = 0x0000001ff;
    pSettings->csMaxWavesPerCu = 0;
    pSettings->csLockThreshold = 0;
    pSettings->csSimdDestCntl = CsSimdDestCntlDefault;
    pSettings->htileEnable = true;
    pSettings->depthCompressEnable = true;
    pSettings->stencilCompressEnable = true;
    pSettings->dbPreloadEnable = true;
    pSettings->dbPreloadWinEnable = false;
    pSettings->dbPerTileExpClearEnable = false;
    pSettings->hiDepthEnable = true;
    pSettings->hiStencilEnable = true;
    pSettings->dbRequestSize = 0;
    pSettings->dbDisableColorOnValidation = false;
    pSettings->enableOnchipGs = true;
    pSettings->enableOutOfOrderPrimitives = Gfx9OutOfOrderPrimSafe;
    pSettings->outOfOrderWatermark = 7;
    pSettings->gfxMaxWavesPerCu = 0;
    pSettings->gsCuGroupEnabled = false;
    pSettings->gsMaxLdsSize = 8 * 1024;
    pSettings->gsOffChipThreshold = 64;
    pSettings->idealGsPrimsPerSubGroup = 64;
    pSettings->lateAllocGs = 16;
    pSettings->maxTessFactor = 64.0f;
    pSettings->numOffchipLdsBuffers = 508;
    pSettings->numTessPatchesPerTg = 0;
    pSettings->offchipLdsBufferSize = Gfx9OffchipLdsBufferSize8192;
    pSettings->isolineDistributionFactor = 40;
    pSettings->triDistributionFactor = 30;
    pSettings->quadDistributionFactor = 24;
    pSettings->donutDistributionFactor = 24;
    pSettings->trapezoidDistributionFactor = 6;
    pSettings->primGroupSize = 128;
    pSettings->psCuGroupEnabled = true;
    pSettings->gfx9RbPlusEnable = true;
    pSettings->switchVgtOnDraw = false;
    pSettings->tessFactorBufferSizePerSe = 0x2000;
    pSettings->disableTessDonutWalkPattern = 0;
    pSettings->useMaxOffchipLdsBuffers = true;
    pSettings->vsCuGroupEnabled = false;
    pSettings->vsHalfPackThreshold = 16;
    pSettings->vsForcePartialWave = false;
    pSettings->disableCoverageAaMask = true;
    pSettings->wdLoadBalancingMode = Gfx9WdLoadBalancingAdvanced;
    pSettings->batchBreakOnNewPixelShader = false;
    pSettings->gsCuEnLimitMask = 0xffffffff;
    pSettings->vsCuEnLimitMask = 0xffffffff;
    pSettings->psCuEnLimitMask = 0xffffffff;
    pSettings->gfx9OffChipHsCopyMethod = Gfx9OffChipHsCopyAllAtEnd;
    pSettings->gfx9OffChipHsSkipDataCopyNullPatch = true;
    pSettings->gfx9OptimizeDsDataFetch = false;
    pSettings->nggMode = Gfx9NggDisabled;
    pSettings->nggRegLaunchGsPrimsPerSubgrp = 63;
    pSettings->idealNggFastLaunchWavesPerSubgrp = 4;
    pSettings->nggLateAllocGs = 127;
    pSettings->nggDisableBackfaceCulling = false;
    pSettings->nggEnableFrustumCulling = false;
    pSettings->nggEnableSmallPrimFilter = false;
    pSettings->nggEnableFasterLaunchRate = false;
    pSettings->enableOrderedIdMode = true;
    pSettings->nggFastLaunchPipelineHash = 0;
    pSettings->nggVertexReusePipelineHash = 0;
    pSettings->nggRingSize = 32;
    pSettings->binningMode = Gfx9DeferredBatchBinAccurate;
    pSettings->customBatchBinSize = 0x00800080;
    pSettings->disableBinningPsKill = true;
    pSettings->disableBinningNoDb = false;
    pSettings->disableBinningBlendingOff = false;
    pSettings->binningMaxAllocCountLegacy = 0;
    pSettings->binningMaxAllocCountNggOnChip = 0;
    pSettings->binningMaxPrimPerBatch = 1024;
    pSettings->binningContextStatesPerBin = 1;
    pSettings->binningPersistentStatesPerBin = 1;
    pSettings->binningFpovsPerBatch = 63;
    pSettings->binningOptimalBinSelection = true;
    pSettings->disableBinningAppendConsume = true;
    pSettings->disableDfsm = true;
    pSettings->disableDfsmPsUav = true;
    pSettings->shaderPrefetchMethod = PrefetchCpDma;
    pSettings->prefetchCommandBuffers = Gfx9PrefetchCommandsBuildInfo;
    pSettings->anisoFilterOptEnabled = false;
    pSettings->samplerCeilingLogicEnabled = false;
    pSettings->samplerPrecisionFixEnabled = true;
    pSettings->samplerPerfMip = 0;
    pSettings->samplerAnisoThreshold = 0;
    pSettings->samplerAnisoBias = 0;
    pSettings->samplerSecAnisoBias = 0;
    pSettings->waWrite1xAASampleLocationsToZero = false;
    pSettings->waColorCacheControllerInvalidEviction = false;
    pSettings->waOverwriteCombinerTargetMaskOnly = false;
    pSettings->waDisableHtilePrefetch = false;
    pSettings->waMiscPopsMissedOverlap = false;
    pSettings->waMiscScissorRegisterChange = false;
    pSettings->waMiscPsFlushScissorChange = false;
    pSettings->waHtilePipeBankXorMustBeZero = false;
    pSettings->waDisableDfsmWithEqaa = false;
    pSettings->waLegacyTessToNggVgtFlush = false;
    pSettings->waNggWdPageFault = false;
    pSettings->waLegacyToNggVsPartialFlush = false;
    pSettings->waDisable24BitHWFormatForTCCompatibleDepth = false;
    pSettings->waDummyZpassDoneBeforeTs = false;
    pSettings->waMetaAliasingFixEnabled = true;

}

// =====================================================================================================================
// Reads the setting from the OS adapter and sets the structure value when the setting values are found.
void SettingsLoader::Gfx9ReadSettings(
    Gfx9PalSettings* pSettings)
{
    // read from the OS adapter for each individual setting
    m_pDevice->ReadSetting(pGfx9DisableBorderColorPaletteBindsStr,
                             Util::ValueType::Boolean,
                             &pSettings->disableBorderColorPaletteBinds,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9DrainPsOnOverlapStr,
                             Util::ValueType::Boolean,
                             &pSettings->drainPsOnOverlap,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9PrintMetaEquationInfoStr,
                             Util::ValueType::Uint,
                             &pSettings->printMetaEquationInfo,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9ProcessMetaEquationViaCpuStr,
                             Util::ValueType::Boolean,
                             &pSettings->processMetaEquationViaCpu,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9OptimizedFastClearStr,
                             Util::ValueType::Uint,
                             &pSettings->optimizedFastClear,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9AlwaysDecompressStr,
                             Util::ValueType::Uint,
                             &pSettings->alwaysDecompress,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pTreat1dAs2dStr,
                             Util::ValueType::Boolean,
                             &pSettings->treat1dAs2d,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9ForceGraphicsFillMemoryPathStr,
                             Util::ValueType::Boolean,
                             &pSettings->forceGraphicsFillMemoryPath,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9WaitOnMetadataMipTailStr,
                             Util::ValueType::Boolean,
                             &pSettings->waitOnMetadataMipTail,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9BlendOptimizationEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->blendOptimizationsEnable,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9FastColorClearEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->fastColorClearEnable,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9FastColorClearOn3DEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->fastColorClearOn3dEnable,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9FmaskCompressDisableStr,
                             Util::ValueType::Boolean,
                             &pSettings->fmaskCompressDisable,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9DccOnComputeEnableStr,
                             Util::ValueType::Uint,
                             &pSettings->dccOnComputeEnable,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9UseDccStr,
                             Util::ValueType::Uint,
                             &pSettings->useDcc,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9CsMaxWavesPerCuStr,
                             Util::ValueType::Uint,
                             &pSettings->csMaxWavesPerCu,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9CsLockThresholdStr,
                             Util::ValueType::Uint,
                             &pSettings->csLockThreshold,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9CsSimdDestCntlStr,
                             Util::ValueType::Uint,
                             &pSettings->csSimdDestCntl,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9HtileEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->htileEnable,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9DepthCompressEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->depthCompressEnable,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9StencilCompressEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->stencilCompressEnable,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9DbPreloadEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->dbPreloadEnable,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9DbPreloadWinEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->dbPreloadWinEnable,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9DbPerTileExpClearEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->dbPerTileExpClearEnable,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9HiDepthEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->hiDepthEnable,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9HiStencilEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->hiStencilEnable,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9DbRequestSizeStr,
                             Util::ValueType::Uint,
                             &pSettings->dbRequestSize,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9DbDisableColorOnValidationStr,
                             Util::ValueType::Boolean,
                             &pSettings->dbDisableColorOnValidation,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9EnableOnchipGsStr,
                             Util::ValueType::Boolean,
                             &pSettings->enableOnchipGs,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9EnableOutOfOrderPrimitivesStr,
                             Util::ValueType::Uint,
                             &pSettings->enableOutOfOrderPrimitives,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9OutOfOrderWatermarkStr,
                             Util::ValueType::Uint,
                             &pSettings->outOfOrderWatermark,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9GfxMaxWavesPerCuStr,
                             Util::ValueType::Uint,
                             &pSettings->gfxMaxWavesPerCu,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9GsCuGroupEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->gsCuGroupEnabled,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9GsMaxLdsSizeStr,
                             Util::ValueType::Uint,
                             &pSettings->gsMaxLdsSize,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9GsOffChipThresholdStr,
                             Util::ValueType::Uint,
                             &pSettings->gsOffChipThreshold,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9GsPrimsPerSubGroupStr,
                             Util::ValueType::Uint,
                             &pSettings->idealGsPrimsPerSubGroup,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9LateAllocGsStr,
                             Util::ValueType::Uint,
                             &pSettings->lateAllocGs,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9MaxTessFactorStr,
                             Util::ValueType::Float,
                             &pSettings->maxTessFactor,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9NumOffchipLdsBuffersStr,
                             Util::ValueType::Uint,
                             &pSettings->numOffchipLdsBuffers,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9NumTessPatchesPerThreadGroupStr,
                             Util::ValueType::Uint,
                             &pSettings->numTessPatchesPerTg,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9OffchipLdsBufferSizeStr,
                             Util::ValueType::Uint,
                             &pSettings->offchipLdsBufferSize,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9IsolineDistributionFactorStr,
                             Util::ValueType::Uint,
                             &pSettings->isolineDistributionFactor,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9TriDistributionFactorStr,
                             Util::ValueType::Uint,
                             &pSettings->triDistributionFactor,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9QuadDistributionFactorStr,
                             Util::ValueType::Uint,
                             &pSettings->quadDistributionFactor,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9DonutDistributionFactorStr,
                             Util::ValueType::Uint,
                             &pSettings->donutDistributionFactor,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9TrapezoidDistributionFactorStr,
                             Util::ValueType::Uint,
                             &pSettings->trapezoidDistributionFactor,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9PrimgroupSizeStr,
                             Util::ValueType::Uint,
                             &pSettings->primGroupSize,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9PsCuGroupEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->psCuGroupEnabled,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9RbPlusEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->gfx9RbPlusEnable,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9SwitchVgtOnDrawStr,
                             Util::ValueType::Boolean,
                             &pSettings->switchVgtOnDraw,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9TessFactorBufferSizePerSeStr,
                             Util::ValueType::Uint,
                             &pSettings->tessFactorBufferSizePerSe,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9TessWalkPatternStr,
                             Util::ValueType::Uint,
                             &pSettings->disableTessDonutWalkPattern,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9UseMaxOffchipLdsBuffersStr,
                             Util::ValueType::Boolean,
                             &pSettings->useMaxOffchipLdsBuffers,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9VsCuGroupEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->vsCuGroupEnabled,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9VsHalfPackThresholdStr,
                             Util::ValueType::Uint,
                             &pSettings->vsHalfPackThreshold,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9VsForcePartialWaveStr,
                             Util::ValueType::Boolean,
                             &pSettings->vsForcePartialWave,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9DisableCoverageAaMaskStr,
                             Util::ValueType::Boolean,
                             &pSettings->disableCoverageAaMask,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9WdLoadBalancingModeStr,
                             Util::ValueType::Uint,
                             &pSettings->wdLoadBalancingMode,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pBatchBreakOnNewPixelShaderStr,
                             Util::ValueType::Boolean,
                             &pSettings->batchBreakOnNewPixelShader,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9GsCuEnLimitMaskStr,
                             Util::ValueType::Uint,
                             &pSettings->gsCuEnLimitMask,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9VsCuEnLimitMaskStr,
                             Util::ValueType::Uint,
                             &pSettings->vsCuEnLimitMask,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9PsCuEnLimitMaskStr,
                             Util::ValueType::Uint,
                             &pSettings->psCuEnLimitMask,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9OffChipHsCopyMethodStr,
                             Util::ValueType::Uint,
                             &pSettings->gfx9OffChipHsCopyMethod,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9OffChipHsSkipDataCopyNullPatchStr,
                             Util::ValueType::Boolean,
                             &pSettings->gfx9OffChipHsSkipDataCopyNullPatch,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9OptimizeDsDataFetchStr,
                             Util::ValueType::Boolean,
                             &pSettings->gfx9OptimizeDsDataFetch,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9NggModeStr,
                             Util::ValueType::Uint,
                             &pSettings->nggMode,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9NggRegLaunchGsPrimsPerSubgrpStr,
                             Util::ValueType::Uint,
                             &pSettings->nggRegLaunchGsPrimsPerSubgrp,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9NggFastLaunchWavesPerSubgrpStr,
                             Util::ValueType::Uint,
                             &pSettings->idealNggFastLaunchWavesPerSubgrp,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9NggLateAllocGsStr,
                             Util::ValueType::Uint,
                             &pSettings->nggLateAllocGs,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9NggDisableBackfaceCullingStr,
                             Util::ValueType::Boolean,
                             &pSettings->nggDisableBackfaceCulling,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9NggEnableFrustumCullingStr,
                             Util::ValueType::Boolean,
                             &pSettings->nggEnableFrustumCulling,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9NggEnableSmallPrimFilterStr,
                             Util::ValueType::Boolean,
                             &pSettings->nggEnableSmallPrimFilter,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9NggEnableFasterLaunchRateStr,
                             Util::ValueType::Boolean,
                             &pSettings->nggEnableFasterLaunchRate,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9EnableOrderedIdModeStr,
                             Util::ValueType::Boolean,
                             &pSettings->enableOrderedIdMode,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pNggFastLaunchPipelineHashStr,
                             Util::ValueType::Uint64,
                             &pSettings->nggFastLaunchPipelineHash,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pNggVertexReusePipelineHashStr,
                             Util::ValueType::Uint64,
                             &pSettings->nggVertexReusePipelineHash,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9NggRingSizeStr,
                             Util::ValueType::Uint,
                             &pSettings->nggRingSize,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9DeferredBatchBinModeStr,
                             Util::ValueType::Uint,
                             &pSettings->binningMode,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9CustomBatchBinSizeStr,
                             Util::ValueType::Uint,
                             &pSettings->customBatchBinSize,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9DisableBinningPsKillStr,
                             Util::ValueType::Boolean,
                             &pSettings->disableBinningPsKill,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9DisableBinningNoDbStr,
                             Util::ValueType::Boolean,
                             &pSettings->disableBinningNoDb,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9DisableBinningBlendingOffStr,
                             Util::ValueType::Boolean,
                             &pSettings->disableBinningBlendingOff,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9BinningMaxAllocCountLegacyStr,
                             Util::ValueType::Uint,
                             &pSettings->binningMaxAllocCountLegacy,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9BinningMaxAllocCountNggOnChipStr,
                             Util::ValueType::Uint,
                             &pSettings->binningMaxAllocCountNggOnChip,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9BinningMaxPrimPerBatchStr,
                             Util::ValueType::Uint,
                             &pSettings->binningMaxPrimPerBatch,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9BinningContextStatesPerBinStr,
                             Util::ValueType::Uint,
                             &pSettings->binningContextStatesPerBin,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9BinningPersistentStatesPerBinStr,
                             Util::ValueType::Uint,
                             &pSettings->binningPersistentStatesPerBin,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9BinningFpovsPerBatchStr,
                             Util::ValueType::Uint,
                             &pSettings->binningFpovsPerBatch,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9BinningOptimalBinSelectionStr,
                             Util::ValueType::Boolean,
                             &pSettings->binningOptimalBinSelection,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9BinningDisableBinningAppendConsumeStr,
                             Util::ValueType::Boolean,
                             &pSettings->disableBinningAppendConsume,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9DisableDfsmStr,
                             Util::ValueType::Boolean,
                             &pSettings->disableDfsm,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9DisableDfsmPsUavStr,
                             Util::ValueType::Boolean,
                             &pSettings->disableDfsmPsUav,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pShaderPrefetchMethodStr,
                             Util::ValueType::Uint,
                             &pSettings->shaderPrefetchMethod,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9PrefetchCommandBuffersStr,
                             Util::ValueType::Uint,
                             &pSettings->prefetchCommandBuffers,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9AnisoFilterOptEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->anisoFilterOptEnabled,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9CeilingLogicEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->samplerCeilingLogicEnabled,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9PrecisionFixEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->samplerPrecisionFixEnabled,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9SamplerPerfMipStr,
                             Util::ValueType::Uint,
                             &pSettings->samplerPerfMip,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9SamplerAnisoThresholdStr,
                             Util::ValueType::Uint,
                             &pSettings->samplerAnisoThreshold,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9SamplerAnisoBiasStr,
                             Util::ValueType::Uint,
                             &pSettings->samplerAnisoBias,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pGfx9SamplerSecAnisoBiasStr,
                             Util::ValueType::Uint,
                             &pSettings->samplerSecAnisoBias,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pWaWrite1xAASampleLocationsToZeroStr,
                             Util::ValueType::Boolean,
                             &pSettings->waWrite1xAASampleLocationsToZero,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pWaColorCacheControllerInvalidEvictionStr,
                             Util::ValueType::Boolean,
                             &pSettings->waColorCacheControllerInvalidEviction,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pWaOverwriteCombinerTargetMaskOnlyStr,
                             Util::ValueType::Boolean,
                             &pSettings->waOverwriteCombinerTargetMaskOnly,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pWaDisableHtilePrefetchStr,
                             Util::ValueType::Boolean,
                             &pSettings->waDisableHtilePrefetch,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pWaMiscPopsMissedOverlapStr,
                             Util::ValueType::Boolean,
                             &pSettings->waMiscPopsMissedOverlap,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pWaMiscScissorRegisterChangeStr,
                             Util::ValueType::Boolean,
                             &pSettings->waMiscScissorRegisterChange,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pWaMiscPsFlushScissorChangeStr,
                             Util::ValueType::Boolean,
                             &pSettings->waMiscPsFlushScissorChange,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pWaHtilePipeBankXorMustBeZeroStr,
                             Util::ValueType::Boolean,
                             &pSettings->waHtilePipeBankXorMustBeZero,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pWaDisableDfsmWithEqaaStr,
                             Util::ValueType::Boolean,
                             &pSettings->waDisableDfsmWithEqaa,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pWaLegacyTessToNggVgtFlushStr,
                             Util::ValueType::Boolean,
                             &pSettings->waLegacyTessToNggVgtFlush,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pWaNggWdPageFaultStr,
                             Util::ValueType::Boolean,
                             &pSettings->waNggWdPageFault,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pWaLegacyToNggVsPartialFlushStr,
                             Util::ValueType::Boolean,
                             &pSettings->waLegacyToNggVsPartialFlush,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pWaDisable24BitHWFormatForTCCompatibleDepthStr,
                             Util::ValueType::Boolean,
                             &pSettings->waDisable24BitHWFormatForTCCompatibleDepth,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pWaDummyZpassDoneBeforeTsStr,
                             Util::ValueType::Boolean,
                             &pSettings->waDummyZpassDoneBeforeTs,
                             InternalSettingScope::PrivatePalGfx9Key);

    m_pDevice->ReadSetting(pWaMetaAliasingFixEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->waMetaAliasingFixEnabled,
                             InternalSettingScope::PrivatePalGfx9Key);


}

} // Gfx9
} // Pal

using namespace Pal::Gfx9;

// =====================================================================================================================
// Get Gfx9 settings pointer via device object.
const Pal::Gfx9::Gfx9PalSettings& GetGfx9Settings(
    const Pal::Device& device)
{
    return static_cast<const Pal::Gfx9::Gfx9PalSettings&>(device.Settings());
}

