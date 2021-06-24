/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
// When changes are needed, modify the tools generating this module in the tools\generate directory OR
// settings_platform.json
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!  WARNING!  WARNING!
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "core/device.h"
#include "core/settingsLoader.h"

#include "core/hw/gfxip/gfx9/gfx9SettingsLoader.h"
#include "core/hw/gfxip/gfx9/g_gfx9PalSettings.h"
#include "palInlineFuncs.h"
#include "palHashMapImpl.h"

#include "devDriverServer.h"
#include "protocols/ddSettingsService.h"

using namespace DevDriver::SettingsURIService;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
// Initializes the settings structure to default values.
void SettingsLoader::SetupDefaults()
{
    // set setting variables to their default values...
    m_settings.enableLoadIndexForObjectBinds = false;

    m_settings.allowBigPage = 0x3f;
    m_settings.disableBorderColorPaletteBinds = false;
    m_settings.printMetaEquationInfo = 0x0;
    m_settings.optimizedFastClear = 0x7;
    m_settings.alwaysDecompress = 0x0;
    m_settings.treat1dAs2d = true;
    m_settings.forceWaveBreakSize = Gfx10ForceWaveBreakSizeClient;
    m_settings.sdmaPreferCompressedSource = true;
    m_settings.useCompToSingle = 0x1f;
    m_settings.forceRegularClearCode = false;

    m_settings.forceGraphicsFillMemoryPath = false;
    m_settings.waitOnMetadataMipTail = false;
    m_settings.blendOptimizationsEnable = true;
    m_settings.fastColorClearEnable = true;
    m_settings.fastColorClearOn3dEnable = true;
    m_settings.fmaskCompressDisable = false;
    m_settings.fmaskAllowPipeBankXor = false;
    m_settings.dccOnComputeEnable = 0x3;
    m_settings.cbDbCachePolicy = 0x0;
    m_settings.csMaxWavesPerCu = 0;
    m_settings.csLockThreshold = 0;
    m_settings.csSimdDestCntl = CsSimdDestCntlDefault;

    m_settings.htileEnable = true;
    m_settings.disableIterate256Opt = false;
    m_settings.allowDepthCopyResolve = true;
    m_settings.depthCompressEnable = true;
    m_settings.stencilCompressEnable = true;
    m_settings.dbPreloadEnable = true;
    m_settings.dbPreloadWinEnable = false;
    m_settings.dbPerTileExpClearEnable = false;
    m_settings.hiDepthEnable = true;
    m_settings.hiStencilEnable = true;
    m_settings.dbRequestSize = 0x0;
    m_settings.dbDisableColorOnValidation = false;
    m_settings.enableOutOfOrderPrimitives = OutOfOrderPrimSafe;
    m_settings.outOfOrderWatermark = 7;
    m_settings.disableGeCntlVtxGrouping = true;
    m_settings.gsCuGroupEnabled = false;
    m_settings.gsMaxLdsSize = 8192;
    m_settings.lateAllocGs = 16;
    m_settings.lateAllocVs = LateAllocVsBehaviorLegacy;
    m_settings.maxTessFactor = 64.0;
    m_settings.numOffchipLdsBuffers = 508;
    m_settings.numTessPatchesPerTg = 0;
    m_settings.offchipLdsBufferSize = OffchipLdsBufferSize8192;
    m_settings.isolineDistributionFactor = 12;
    m_settings.triDistributionFactor = 30;
    m_settings.quadDistributionFactor = 24;
    m_settings.donutDistributionFactor = 24;
    m_settings.trapezoidDistributionFactor = 6;
    m_settings.primGroupSize = 128;
    m_settings.gfx9RbPlusEnable = true;
    m_settings.gfx10SpiShaderLateAllocVsNumLines = 255;
    m_settings.gfx10GePcAllocNumLinesPerSeLegacyNggPassthru = 0x80;
    m_settings.allowNggOnAllCusWgps = false;
    m_settings.gfx10GePcAllocNumLinesPerSeNggCulling = 0x100;
    m_settings.numPsWavesSoftGroupedPerCu = 4;
    m_settings.numVsWavesSoftGroupedPerCu = 0;
    m_settings.switchVgtOnDraw = false;
    m_settings.tessFactorBufferSizePerSe = 8192;
    m_settings.disableTessDonutWalkPattern = 0;
    m_settings.useMaxOffchipLdsBuffers = true;
    m_settings.vsHalfPackThreshold = 16;
    m_settings.vsForcePartialWave = false;
    m_settings.disableCoverageAaMask = true;
    m_settings.batchBreakOnNewPixelShader = false;
    m_settings.useClearStateToInitialize = true;
    m_settings.gsCuEnLimitMask = 0xffffffff;
    m_settings.vsCuEnLimitMask = 0xffffffff;
    m_settings.psCuEnLimitMask = 0xffffffff;
    m_settings.csCuEnLimitMask = 0xffffffff;

    m_settings.shaderPrefetchSizeBytes = 4294967295;
    m_settings.numTsMsDrawEntriesPerSe = 256;
    m_settings.nggSupported = true;
    m_settings.nggLateAllocGs = 127;
    m_settings.binningMode = Gfx9DeferredBatchBinAccurate;
    m_settings.customBatchBinSize = 0x800080;
    m_settings.minBatchBinSize.width = 0;
    m_settings.minBatchBinSize.height = 0;
    m_settings.ignoreDepthForBinSizeIfColorBound = false;
    m_settings.disableBinningMode = 3;
    m_settings.disableBinningPsKill = true;
    m_settings.binningMaxAllocCountLegacy = 0;
    m_settings.binningMaxAllocCountNggOnChip = 0;
    m_settings.binningMaxPrimPerBatch = 1024;
    m_settings.binningContextStatesPerBin = 1;
    m_settings.binningPersistentStatesPerBin = 1;

    m_settings.binningFpovsPerBatch = 63;
    m_settings.binningOptimalBinSelection = true;

    m_settings.disableBinningAppendConsume = true;

    m_settings.shaderPrefetchMethod = PrefetchCpDma;
    m_settings.prefetchCommandBuffers = Gfx9PrefetchCommandsBuildInfo;
    m_settings.anisoFilterOptEnabled = false;
    m_settings.samplerCeilingLogicEnabled = false;
    m_settings.samplerPrecisionFixEnabled = true;
    m_settings.samplerPerfMip = 0;
    m_settings.samplerAnisoThreshold = 0;
    m_settings.samplerAnisoBias = 0;
    m_settings.samplerSecAnisoBias = 0;
    m_settings.waRestrictMetaDataUseInMipTail = false;
    m_settings.waVrsStencilUav = NoFix;

    m_settings.waLogicOpDisablesOverwriteCombiner = false;
    m_settings.waRotatedSwizzleDisablesOverwriteCombiner = false;
    m_settings.waDisableFmaskNofetchOpOnFmaskCompressionDisable = false;
    m_settings.waDisableVrsWithDsExports = false;

    m_settings.waFixPostZConservativeRasterization = false;
    m_settings.waClampQuadDistributionFactor = false;
    m_settings.waWrite1xAASampleLocationsToZero = false;
    m_settings.waColorCacheControllerInvalidEviction = false;
    m_settings.waForceZonlyHtileForMipmaps = false;
    m_settings.waOverwriteCombinerTargetMaskOnly = false;
    m_settings.waDisableHtilePrefetch = false;
    m_settings.waMiscPopsMissedOverlap = false;
    m_settings.waMiscScissorRegisterChange = false;
    m_settings.waMiscPsFlushScissorChange = false;
    m_settings.waHtilePipeBankXorMustBeZero = false;
    m_settings.waDisableDfsmWithEqaa = false;
    m_settings.waDisable24BitHWFormatForTCCompatibleDepth = false;
    m_settings.waDummyZpassDoneBeforeTs = false;
    m_settings.waMetaAliasingFixEnabled = true;
    m_settings.waForce256bCbFetch = false;
    m_settings.waCmaskImageSyncs = false;
    m_settings.waSdmaPreventCompressedSurfUse = false;
    m_settings.waVgtFlushNggToLegacyGs = false;
    m_settings.waVgtFlushNggToLegacy = false;
    m_settings.waEnableIndexBufferPrefetchForNgg = false;
    m_settings.waZ16Unorm1xAaDecompressUninitialized = false;
    m_settings.waLateAllocGs0 = false;
    m_settings.waIndexBufferZeroSize = false;
    m_settings.waStalledPopsMode = false;
    m_settings.waCeDisableIb2 = false;
    m_settings.waDisableSCompressSOnly = false;
    m_settings.waDisableInstancePacking = false;

    m_settings.vrsHtileEncoding = Gfx10VrsHtileEncodingTwoBit;

    m_settings.vrsForceRateFine = false;

    m_settings.privateDepthIsHtileOnly = true;

    m_settings.optimizeNullSourceImage = true;

    m_settings.vrsImageSize = 0x40004000;

    m_settings.waUtcL0InconsistentBigPage = false;
    m_settings.waTwoPlanesIterate256 = false;
    m_settings.waTessIncorrectRelativeIndex = false;
    m_settings.waLimitLateAllocGsNggFifo = false;
    m_settings.waClampGeCntlVertGrpSize = false;
    m_settings.waLegacyGsCutModeFlush = false;
    m_settings.sdmaBypassMall = 0x3;

    m_settings.enableMallCursorCache = true;

    m_settings.depthStencilFastClearComputeThresholdSingleSampled = 2097152;
    m_settings.depthStencilFastClearComputeThresholdMultiSampled = 4194304;
    m_settings.gfx10MaxFpovsInWave = 0;
    m_settings.disableAceCsPartialFlush = false;
    m_settings.addrLibGbAddrConfigOverride = 0x0;
    m_settings.nonLocalDestPreferCompute = true;

    m_settings.gfx103DisableAsymmetricWgpForPs = false;

    m_settings.numSettings = g_gfx9PalNumSettings;
}

// =====================================================================================================================
// Reads the setting from the OS adapter and sets the structure value when the setting values are found.
void SettingsLoader::ReadSettings()
{
    // read from the OS adapter for each individual setting
    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pEnableLoadIndexForObjectBindsStr,
                           Util::ValueType::Boolean,
                           &m_settings.enableLoadIndexForObjectBinds,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAllowBigPageStr,
                           Util::ValueType::Uint,
                           &m_settings.allowBigPage,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDisableBorderColorPaletteBindsStr,
                           Util::ValueType::Boolean,
                           &m_settings.disableBorderColorPaletteBinds,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPrintMetaEquationInfoStr,
                           Util::ValueType::Uint,
                           &m_settings.printMetaEquationInfo,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pOptimizedFastClearStr,
                           Util::ValueType::Uint,
                           &m_settings.optimizedFastClear,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAlwaysDecompressStr,
                           Util::ValueType::Uint,
                           &m_settings.alwaysDecompress,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pTreat1dAs2dStr,
                           Util::ValueType::Boolean,
                           &m_settings.treat1dAs2d,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pForceWaveBreakSizeStr,
                           Util::ValueType::Uint,
                           &m_settings.forceWaveBreakSize,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pSdmaPreferCompressedSourceStr,
                           Util::ValueType::Boolean,
                           &m_settings.sdmaPreferCompressedSource,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pUseCompToSingleStr,
                           Util::ValueType::Uint,
                           &m_settings.useCompToSingle,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pForceRegularClearCodeStr,
                           Util::ValueType::Boolean,
                           &m_settings.forceRegularClearCode,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pForceGraphicsFillMemoryPathStr,
                           Util::ValueType::Boolean,
                           &m_settings.forceGraphicsFillMemoryPath,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaitOnMetadataMipTailStr,
                           Util::ValueType::Boolean,
                           &m_settings.waitOnMetadataMipTail,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pBlendOptimizationEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.blendOptimizationsEnable,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pFastColorClearEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.fastColorClearEnable,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pFastColorClearOn3DEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.fastColorClearOn3dEnable,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pFmaskCompressDisableStr,
                           Util::ValueType::Boolean,
                           &m_settings.fmaskCompressDisable,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pFmaskAllowPipeBankXorStr,
                           Util::ValueType::Boolean,
                           &m_settings.fmaskAllowPipeBankXor,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDccOnComputeEnableStr,
                           Util::ValueType::Uint,
                           &m_settings.dccOnComputeEnable,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCbDbCachePolicyStr,
                           Util::ValueType::Uint,
                           &m_settings.cbDbCachePolicy,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCsMaxWavesPerCuStr,
                           Util::ValueType::Uint,
                           &m_settings.csMaxWavesPerCu,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCsLockThresholdStr,
                           Util::ValueType::Uint,
                           &m_settings.csLockThreshold,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCsSimdDestCntlStr,
                           Util::ValueType::Uint,
                           &m_settings.csSimdDestCntl,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pHtileEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.htileEnable,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDisableIterate256OptStr,
                           Util::ValueType::Boolean,
                           &m_settings.disableIterate256Opt,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAllowDepthCopyResolveStr,
                           Util::ValueType::Boolean,
                           &m_settings.allowDepthCopyResolve,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDepthCompressEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.depthCompressEnable,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pStencilCompressEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.stencilCompressEnable,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDbPreloadEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.dbPreloadEnable,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDbPreloadWinEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.dbPreloadWinEnable,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDbPerTileExpClearEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.dbPerTileExpClearEnable,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pHiDepthEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.hiDepthEnable,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pHiStencilEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.hiStencilEnable,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDbRequestSizeStr,
                           Util::ValueType::Uint,
                           &m_settings.dbRequestSize,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDbDisableColorOnValidationStr,
                           Util::ValueType::Boolean,
                           &m_settings.dbDisableColorOnValidation,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pEnableOutOfOrderPrimitivesStr,
                           Util::ValueType::Uint,
                           &m_settings.enableOutOfOrderPrimitives,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pOutOfOrderWatermarkStr,
                           Util::ValueType::Uint,
                           &m_settings.outOfOrderWatermark,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDisableGeCntlVtxGroupingStr,
                           Util::ValueType::Boolean,
                           &m_settings.disableGeCntlVtxGrouping,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGsCuGroupEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.gsCuGroupEnabled,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGsMaxLdsSizeStr,
                           Util::ValueType::Uint,
                           &m_settings.gsMaxLdsSize,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pLateAllocGsStr,
                           Util::ValueType::Uint,
                           &m_settings.lateAllocGs,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pLateAllocVsStr,
                           Util::ValueType::Uint,
                           &m_settings.lateAllocVs,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pMaxTessFactorStr,
                           Util::ValueType::Float,
                           &m_settings.maxTessFactor,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pNumOffchipLdsBuffersStr,
                           Util::ValueType::Uint,
                           &m_settings.numOffchipLdsBuffers,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pNumTessPatchesPerThreadGroupStr,
                           Util::ValueType::Uint,
                           &m_settings.numTessPatchesPerTg,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pOffchipLdsBufferSizeStr,
                           Util::ValueType::Uint,
                           &m_settings.offchipLdsBufferSize,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pIsolineDistributionFactorStr,
                           Util::ValueType::Uint,
                           &m_settings.isolineDistributionFactor,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pTriDistributionFactorStr,
                           Util::ValueType::Uint,
                           &m_settings.triDistributionFactor,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pQuadDistributionFactorStr,
                           Util::ValueType::Uint,
                           &m_settings.quadDistributionFactor,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDonutDistributionFactorStr,
                           Util::ValueType::Uint,
                           &m_settings.donutDistributionFactor,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pTrapezoidDistributionFactorStr,
                           Util::ValueType::Uint,
                           &m_settings.trapezoidDistributionFactor,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPrimgroupSizeStr,
                           Util::ValueType::Uint,
                           &m_settings.primGroupSize,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pRbPlusEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.gfx9RbPlusEnable,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGfx10SpiShaderLateAllocVsNumLinesStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx10SpiShaderLateAllocVsNumLines,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGfx10GePcAllocNumLinesPerSeLegacyNggPassthruStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx10GePcAllocNumLinesPerSeLegacyNggPassthru,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAllowNggOnAllCusWgpsStr,
                           Util::ValueType::Boolean,
                           &m_settings.allowNggOnAllCusWgps,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGfx10GePcAllocNumLinesPerSeNggCullingStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx10GePcAllocNumLinesPerSeNggCulling,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pNumPsWavesSoftGroupedPerCuStr,
                           Util::ValueType::Uint,
                           &m_settings.numPsWavesSoftGroupedPerCu,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pNumVsWavesSoftGroupedPerCuStr,
                           Util::ValueType::Uint,
                           &m_settings.numVsWavesSoftGroupedPerCu,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pSwitchVgtOnDrawStr,
                           Util::ValueType::Boolean,
                           &m_settings.switchVgtOnDraw,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pTessFactorBufferSizePerSeStr,
                           Util::ValueType::Uint,
                           &m_settings.tessFactorBufferSizePerSe,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pTessWalkPatternStr,
                           Util::ValueType::Uint,
                           &m_settings.disableTessDonutWalkPattern,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pUseMaxOffchipLdsBuffersStr,
                           Util::ValueType::Boolean,
                           &m_settings.useMaxOffchipLdsBuffers,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pVsHalfPackThresholdStr,
                           Util::ValueType::Uint,
                           &m_settings.vsHalfPackThreshold,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pVsForcePartialWaveStr,
                           Util::ValueType::Boolean,
                           &m_settings.vsForcePartialWave,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDisableCoverageAaMaskStr,
                           Util::ValueType::Boolean,
                           &m_settings.disableCoverageAaMask,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pBatchBreakOnNewPixelShaderStr,
                           Util::ValueType::Boolean,
                           &m_settings.batchBreakOnNewPixelShader,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pUseClearStateToInitializeStr,
                           Util::ValueType::Boolean,
                           &m_settings.useClearStateToInitialize,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGsCuEnLimitMaskStr,
                           Util::ValueType::Uint,
                           &m_settings.gsCuEnLimitMask,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pVsCuEnLimitMaskStr,
                           Util::ValueType::Uint,
                           &m_settings.vsCuEnLimitMask,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPsCuEnLimitMaskStr,
                           Util::ValueType::Uint,
                           &m_settings.psCuEnLimitMask,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCsCuEnLimitMaskStr,
                           Util::ValueType::Uint,
                           &m_settings.csCuEnLimitMask,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pShaderPrefetchSizeBytesStr,
                           Util::ValueType::Uint,
                           &m_settings.shaderPrefetchSizeBytes,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pNumTsMsDrawEntriesPerSeStr,
                           Util::ValueType::Uint,
                           &m_settings.numTsMsDrawEntriesPerSe,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pNggSupportedStr,
                           Util::ValueType::Boolean,
                           &m_settings.nggSupported,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pNggLateAllocGsStr,
                           Util::ValueType::Uint,
                           &m_settings.nggLateAllocGs,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDeferredBatchBinModeStr,
                           Util::ValueType::Uint,
                           &m_settings.binningMode,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCustomBatchBinSizeStr,
                           Util::ValueType::Uint,
                           &m_settings.customBatchBinSize,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pMinBatchBinSize_WidthStr,
                           Util::ValueType::Uint,
                           &m_settings.minBatchBinSize.width,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pMinBatchBinSize_HeightStr,
                           Util::ValueType::Uint,
                           &m_settings.minBatchBinSize.height,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pIgnoreDepthForBinSizeIfColorBoundStr,
                           Util::ValueType::Boolean,
                           &m_settings.ignoreDepthForBinSizeIfColorBound,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDisableBinningModeStr,
                           Util::ValueType::Uint,
                           &m_settings.disableBinningMode,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDisableBinningPsKillStr,
                           Util::ValueType::Boolean,
                           &m_settings.disableBinningPsKill,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pBinningMaxAllocCountLegacyStr,
                           Util::ValueType::Uint,
                           &m_settings.binningMaxAllocCountLegacy,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pBinningMaxAllocCountNggOnChipStr,
                           Util::ValueType::Uint,
                           &m_settings.binningMaxAllocCountNggOnChip,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pBinningMaxPrimPerBatchStr,
                           Util::ValueType::Uint,
                           &m_settings.binningMaxPrimPerBatch,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pBinningContextStatesPerBinStr,
                           Util::ValueType::Uint,
                           &m_settings.binningContextStatesPerBin,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pBinningPersistentStatesPerBinStr,
                           Util::ValueType::Uint,
                           &m_settings.binningPersistentStatesPerBin,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pBinningFpovsPerBatchStr,
                           Util::ValueType::Uint,
                           &m_settings.binningFpovsPerBatch,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pBinningOptimalBinSelectionStr,
                           Util::ValueType::Boolean,
                           &m_settings.binningOptimalBinSelection,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pBinningDisableBinningAppendConsumeStr,
                           Util::ValueType::Boolean,
                           &m_settings.disableBinningAppendConsume,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pShaderPrefetchMethodStr,
                           Util::ValueType::Uint,
                           &m_settings.shaderPrefetchMethod,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPrefetchCommandBuffersStr,
                           Util::ValueType::Uint,
                           &m_settings.prefetchCommandBuffers,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAnisoFilterOptEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.anisoFilterOptEnabled,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCeilingLogicEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.samplerCeilingLogicEnabled,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPrecisionFixEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.samplerPrecisionFixEnabled,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pSamplerPerfMipStr,
                           Util::ValueType::Uint,
                           &m_settings.samplerPerfMip,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pSamplerAnisoThresholdStr,
                           Util::ValueType::Uint,
                           &m_settings.samplerAnisoThreshold,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pSamplerAnisoBiasStr,
                           Util::ValueType::Uint,
                           &m_settings.samplerAnisoBias,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pSamplerSecAnisoBiasStr,
                           Util::ValueType::Uint,
                           &m_settings.samplerSecAnisoBias,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaRestrictMetaDataUseInMipTailStr,
                           Util::ValueType::Boolean,
                           &m_settings.waRestrictMetaDataUseInMipTail,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaVrsStencilUavStr,
                           Util::ValueType::Uint,
                           &m_settings.waVrsStencilUav,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaLogicOpDisablesOverwriteCombinerStr,
                           Util::ValueType::Boolean,
                           &m_settings.waLogicOpDisablesOverwriteCombiner,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaRotatedSwizzleDisablesOverwriteCombinerStr,
                           Util::ValueType::Boolean,
                           &m_settings.waRotatedSwizzleDisablesOverwriteCombiner,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaDisableFmaskNofetchOpOnFmaskCompressionDisableStr,
                           Util::ValueType::Boolean,
                           &m_settings.waDisableFmaskNofetchOpOnFmaskCompressionDisable,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaDisableVrsWithDsExportsStr,
                           Util::ValueType::Boolean,
                           &m_settings.waDisableVrsWithDsExports,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaFixPostZConservativeRasterizationStr,
                           Util::ValueType::Boolean,
                           &m_settings.waFixPostZConservativeRasterization,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaClampQuadDistributionFactorStr,
                           Util::ValueType::Boolean,
                           &m_settings.waClampQuadDistributionFactor,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaWrite1xAASampleLocationsToZeroStr,
                           Util::ValueType::Boolean,
                           &m_settings.waWrite1xAASampleLocationsToZero,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaColorCacheControllerInvalidEvictionStr,
                           Util::ValueType::Boolean,
                           &m_settings.waColorCacheControllerInvalidEviction,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaForceZonlyHtileForMipmapsStr,
                           Util::ValueType::Boolean,
                           &m_settings.waForceZonlyHtileForMipmaps,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaOverwriteCombinerTargetMaskOnlyStr,
                           Util::ValueType::Boolean,
                           &m_settings.waOverwriteCombinerTargetMaskOnly,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaDisableHtilePrefetchStr,
                           Util::ValueType::Boolean,
                           &m_settings.waDisableHtilePrefetch,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaMiscPopsMissedOverlapStr,
                           Util::ValueType::Boolean,
                           &m_settings.waMiscPopsMissedOverlap,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaMiscScissorRegisterChangeStr,
                           Util::ValueType::Boolean,
                           &m_settings.waMiscScissorRegisterChange,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaMiscPsFlushScissorChangeStr,
                           Util::ValueType::Boolean,
                           &m_settings.waMiscPsFlushScissorChange,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaHtilePipeBankXorMustBeZeroStr,
                           Util::ValueType::Boolean,
                           &m_settings.waHtilePipeBankXorMustBeZero,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaDisableDfsmWithEqaaStr,
                           Util::ValueType::Boolean,
                           &m_settings.waDisableDfsmWithEqaa,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaDisable24BitHWFormatForTCCompatibleDepthStr,
                           Util::ValueType::Boolean,
                           &m_settings.waDisable24BitHWFormatForTCCompatibleDepth,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaDummyZpassDoneBeforeTsStr,
                           Util::ValueType::Boolean,
                           &m_settings.waDummyZpassDoneBeforeTs,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaMetaAliasingFixEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.waMetaAliasingFixEnabled,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaForce256bCbFetchStr,
                           Util::ValueType::Boolean,
                           &m_settings.waForce256bCbFetch,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaCmaskImageSyncsStr,
                           Util::ValueType::Boolean,
                           &m_settings.waCmaskImageSyncs,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaSdmaPreventCompressedSurfUseStr,
                           Util::ValueType::Boolean,
                           &m_settings.waSdmaPreventCompressedSurfUse,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaVgtFlushNggToLegacyGsStr,
                           Util::ValueType::Boolean,
                           &m_settings.waVgtFlushNggToLegacyGs,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaVgtFlushNggToLegacyStr,
                           Util::ValueType::Boolean,
                           &m_settings.waVgtFlushNggToLegacy,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaEnableIndexBufferPrefetchForNggStr,
                           Util::ValueType::Boolean,
                           &m_settings.waEnableIndexBufferPrefetchForNgg,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaZ16Unorm1xAaDecompressUninitializedStr,
                           Util::ValueType::Boolean,
                           &m_settings.waZ16Unorm1xAaDecompressUninitialized,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaLateAllocGs0Str,
                           Util::ValueType::Boolean,
                           &m_settings.waLateAllocGs0,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaIndexBufferZeroSizeStr,
                           Util::ValueType::Boolean,
                           &m_settings.waIndexBufferZeroSize,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaStalledPopsModeStr,
                           Util::ValueType::Boolean,
                           &m_settings.waStalledPopsMode,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaCeDisableIb2Str,
                           Util::ValueType::Boolean,
                           &m_settings.waCeDisableIb2,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaDisableSCompressSOnlyStr,
                           Util::ValueType::Boolean,
                           &m_settings.waDisableSCompressSOnly,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaDisableInstancePackingStr,
                           Util::ValueType::Boolean,
                           &m_settings.waDisableInstancePacking,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pVrsHtileEncodingStr,
                           Util::ValueType::Uint,
                           &m_settings.vrsHtileEncoding,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pVrsForceRateFineStr,
                           Util::ValueType::Boolean,
                           &m_settings.vrsForceRateFine,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPrivateDepthIsHtileOnlyStr,
                           Util::ValueType::Boolean,
                           &m_settings.privateDepthIsHtileOnly,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pOptimizeNullSourceImageStr,
                           Util::ValueType::Boolean,
                           &m_settings.optimizeNullSourceImage,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pVrsImageSizeStr,
                           Util::ValueType::Uint,
                           &m_settings.vrsImageSize,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaUtcL0InconsistentBigPageStr,
                           Util::ValueType::Boolean,
                           &m_settings.waUtcL0InconsistentBigPage,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaTwoPlanesIterate256Str,
                           Util::ValueType::Boolean,
                           &m_settings.waTwoPlanesIterate256,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaTessIncorrectRelativeIndexStr,
                           Util::ValueType::Boolean,
                           &m_settings.waTessIncorrectRelativeIndex,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaLimitLateAllocGsNggFifoStr,
                           Util::ValueType::Boolean,
                           &m_settings.waLimitLateAllocGsNggFifo,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaClampGeCntlVertGrpSizeStr,
                           Util::ValueType::Boolean,
                           &m_settings.waClampGeCntlVertGrpSize,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaLegacyGsCutModeFlushStr,
                           Util::ValueType::Boolean,
                           &m_settings.waLegacyGsCutModeFlush,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pSdmaBypassMallStr,
                           Util::ValueType::Uint,
                           &m_settings.sdmaBypassMall,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pEnableMallCursorCacheStr,
                           Util::ValueType::Boolean,
                           &m_settings.enableMallCursorCache,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDepthStencilFastClearComputeThresholdSingleSampledStr,
                           Util::ValueType::Uint,
                           &m_settings.depthStencilFastClearComputeThresholdSingleSampled,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDepthStencilFastClearComputeThresholdMultiSampledStr,
                           Util::ValueType::Uint,
                           &m_settings.depthStencilFastClearComputeThresholdMultiSampled,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGfx10MaxFpovsInWaveStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx10MaxFpovsInWave,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDisableAceCsPartialFlushStr,
                           Util::ValueType::Boolean,
                           &m_settings.disableAceCsPartialFlush,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAddrLibGbAddrConfigOverrideStr,
                           Util::ValueType::Uint,
                           &m_settings.addrLibGbAddrConfigOverride,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGfx102NonLocalDestPreferComputeStr,
                           Util::ValueType::Boolean,
                           &m_settings.nonLocalDestPreferCompute,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGfx103DisableAsymmetricWgpForPsStr,
                           Util::ValueType::Boolean,
                           &m_settings.gfx103DisableAsymmetricWgpForPs,
                           InternalSettingScope::PrivatePalGfx9Key);

}

// =====================================================================================================================
// Reads the setting from the OS adapter and sets the structure value when the setting values are found.
// This is expected to be done after the component has perform overrides of any defaults.
void SettingsLoader::RereadSettings()
{
    // read from the OS adapter for each individual setting

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCbDbCachePolicyStr,
                           Util::ValueType::Uint,
                           &m_settings.cbDbCachePolicy,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAllowDepthCopyResolveStr,
                           Util::ValueType::Boolean,
                           &m_settings.allowDepthCopyResolve,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGfx10GePcAllocNumLinesPerSeLegacyNggPassthruStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx10GePcAllocNumLinesPerSeLegacyNggPassthru,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAllowNggOnAllCusWgpsStr,
                           Util::ValueType::Boolean,
                           &m_settings.allowNggOnAllCusWgps,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGfx10GePcAllocNumLinesPerSeNggCullingStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx10GePcAllocNumLinesPerSeNggCulling,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pBatchBreakOnNewPixelShaderStr,
                           Util::ValueType::Boolean,
                           &m_settings.batchBreakOnNewPixelShader,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pUseClearStateToInitializeStr,
                           Util::ValueType::Boolean,
                           &m_settings.useClearStateToInitialize,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pNggSupportedStr,
                           Util::ValueType::Boolean,
                           &m_settings.nggSupported,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pNggLateAllocGsStr,
                           Util::ValueType::Uint,
                           &m_settings.nggLateAllocGs,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pIgnoreDepthForBinSizeIfColorBoundStr,
                           Util::ValueType::Boolean,
                           &m_settings.ignoreDepthForBinSizeIfColorBound,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDisableBinningModeStr,
                           Util::ValueType::Uint,
                           &m_settings.disableBinningMode,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDisableBinningPsKillStr,
                           Util::ValueType::Boolean,
                           &m_settings.disableBinningPsKill,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pBinningContextStatesPerBinStr,
                           Util::ValueType::Uint,
                           &m_settings.binningContextStatesPerBin,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pBinningPersistentStatesPerBinStr,
                           Util::ValueType::Uint,
                           &m_settings.binningPersistentStatesPerBin,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaRestrictMetaDataUseInMipTailStr,
                           Util::ValueType::Boolean,
                           &m_settings.waRestrictMetaDataUseInMipTail,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaVrsStencilUavStr,
                           Util::ValueType::Uint,
                           &m_settings.waVrsStencilUav,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaLogicOpDisablesOverwriteCombinerStr,
                           Util::ValueType::Boolean,
                           &m_settings.waLogicOpDisablesOverwriteCombiner,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaRotatedSwizzleDisablesOverwriteCombinerStr,
                           Util::ValueType::Boolean,
                           &m_settings.waRotatedSwizzleDisablesOverwriteCombiner,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaDisableFmaskNofetchOpOnFmaskCompressionDisableStr,
                           Util::ValueType::Boolean,
                           &m_settings.waDisableFmaskNofetchOpOnFmaskCompressionDisable,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaDisableVrsWithDsExportsStr,
                           Util::ValueType::Boolean,
                           &m_settings.waDisableVrsWithDsExports,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaFixPostZConservativeRasterizationStr,
                           Util::ValueType::Boolean,
                           &m_settings.waFixPostZConservativeRasterization,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaClampQuadDistributionFactorStr,
                           Util::ValueType::Boolean,
                           &m_settings.waClampQuadDistributionFactor,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaWrite1xAASampleLocationsToZeroStr,
                           Util::ValueType::Boolean,
                           &m_settings.waWrite1xAASampleLocationsToZero,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaColorCacheControllerInvalidEvictionStr,
                           Util::ValueType::Boolean,
                           &m_settings.waColorCacheControllerInvalidEviction,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaForceZonlyHtileForMipmapsStr,
                           Util::ValueType::Boolean,
                           &m_settings.waForceZonlyHtileForMipmaps,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaOverwriteCombinerTargetMaskOnlyStr,
                           Util::ValueType::Boolean,
                           &m_settings.waOverwriteCombinerTargetMaskOnly,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaDisableHtilePrefetchStr,
                           Util::ValueType::Boolean,
                           &m_settings.waDisableHtilePrefetch,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaMiscPopsMissedOverlapStr,
                           Util::ValueType::Boolean,
                           &m_settings.waMiscPopsMissedOverlap,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaMiscScissorRegisterChangeStr,
                           Util::ValueType::Boolean,
                           &m_settings.waMiscScissorRegisterChange,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaMiscPsFlushScissorChangeStr,
                           Util::ValueType::Boolean,
                           &m_settings.waMiscPsFlushScissorChange,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaHtilePipeBankXorMustBeZeroStr,
                           Util::ValueType::Boolean,
                           &m_settings.waHtilePipeBankXorMustBeZero,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaDisableDfsmWithEqaaStr,
                           Util::ValueType::Boolean,
                           &m_settings.waDisableDfsmWithEqaa,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaDisable24BitHWFormatForTCCompatibleDepthStr,
                           Util::ValueType::Boolean,
                           &m_settings.waDisable24BitHWFormatForTCCompatibleDepth,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaDummyZpassDoneBeforeTsStr,
                           Util::ValueType::Boolean,
                           &m_settings.waDummyZpassDoneBeforeTs,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaMetaAliasingFixEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.waMetaAliasingFixEnabled,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaForce256bCbFetchStr,
                           Util::ValueType::Boolean,
                           &m_settings.waForce256bCbFetch,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaCmaskImageSyncsStr,
                           Util::ValueType::Boolean,
                           &m_settings.waCmaskImageSyncs,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaVgtFlushNggToLegacyGsStr,
                           Util::ValueType::Boolean,
                           &m_settings.waVgtFlushNggToLegacyGs,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaVgtFlushNggToLegacyStr,
                           Util::ValueType::Boolean,
                           &m_settings.waVgtFlushNggToLegacy,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaEnableIndexBufferPrefetchForNggStr,
                           Util::ValueType::Boolean,
                           &m_settings.waEnableIndexBufferPrefetchForNgg,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaZ16Unorm1xAaDecompressUninitializedStr,
                           Util::ValueType::Boolean,
                           &m_settings.waZ16Unorm1xAaDecompressUninitialized,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaLateAllocGs0Str,
                           Util::ValueType::Boolean,
                           &m_settings.waLateAllocGs0,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaIndexBufferZeroSizeStr,
                           Util::ValueType::Boolean,
                           &m_settings.waIndexBufferZeroSize,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaStalledPopsModeStr,
                           Util::ValueType::Boolean,
                           &m_settings.waStalledPopsMode,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaCeDisableIb2Str,
                           Util::ValueType::Boolean,
                           &m_settings.waCeDisableIb2,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaDisableSCompressSOnlyStr,
                           Util::ValueType::Boolean,
                           &m_settings.waDisableSCompressSOnly,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaDisableInstancePackingStr,
                           Util::ValueType::Boolean,
                           &m_settings.waDisableInstancePacking,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaUtcL0InconsistentBigPageStr,
                           Util::ValueType::Boolean,
                           &m_settings.waUtcL0InconsistentBigPage,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaTwoPlanesIterate256Str,
                           Util::ValueType::Boolean,
                           &m_settings.waTwoPlanesIterate256,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaTessIncorrectRelativeIndexStr,
                           Util::ValueType::Boolean,
                           &m_settings.waTessIncorrectRelativeIndex,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaLimitLateAllocGsNggFifoStr,
                           Util::ValueType::Boolean,
                           &m_settings.waLimitLateAllocGsNggFifo,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaClampGeCntlVertGrpSizeStr,
                           Util::ValueType::Boolean,
                           &m_settings.waClampGeCntlVertGrpSize,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaLegacyGsCutModeFlushStr,
                           Util::ValueType::Boolean,
                           &m_settings.waLegacyGsCutModeFlush,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDepthStencilFastClearComputeThresholdSingleSampledStr,
                           Util::ValueType::Uint,
                           &m_settings.depthStencilFastClearComputeThresholdSingleSampled,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDepthStencilFastClearComputeThresholdMultiSampledStr,
                           Util::ValueType::Uint,
                           &m_settings.depthStencilFastClearComputeThresholdMultiSampled,
                           InternalSettingScope::PrivatePalGfx9Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGfx10MaxFpovsInWaveStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx10MaxFpovsInWave,
                           InternalSettingScope::PrivatePalGfx9Key);

}

// =====================================================================================================================
// Initializes the SettingInfo hash map and array of setting hashes.
void SettingsLoader::InitSettingsInfo()
{
    SettingInfo info = {};

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.enableLoadIndexForObjectBinds;
    info.valueSize = sizeof(m_settings.enableLoadIndexForObjectBinds);
    m_settingsInfoMap.Insert(2416072074, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.allowBigPage;
    info.valueSize = sizeof(m_settings.allowBigPage);
    m_settingsInfoMap.Insert(1926167631, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.disableBorderColorPaletteBinds;
    info.valueSize = sizeof(m_settings.disableBorderColorPaletteBinds);
    m_settingsInfoMap.Insert(3825276041, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.printMetaEquationInfo;
    info.valueSize = sizeof(m_settings.printMetaEquationInfo);
    m_settingsInfoMap.Insert(2137175839, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.optimizedFastClear;
    info.valueSize = sizeof(m_settings.optimizedFastClear);
    m_settingsInfoMap.Insert(1875719625, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.alwaysDecompress;
    info.valueSize = sizeof(m_settings.alwaysDecompress);
    m_settingsInfoMap.Insert(2887583419, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.treat1dAs2d;
    info.valueSize = sizeof(m_settings.treat1dAs2d);
    m_settingsInfoMap.Insert(648332656, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.forceWaveBreakSize;
    info.valueSize = sizeof(m_settings.forceWaveBreakSize);
    m_settingsInfoMap.Insert(3257946177, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.sdmaPreferCompressedSource;
    info.valueSize = sizeof(m_settings.sdmaPreferCompressedSource);
    m_settingsInfoMap.Insert(1884222990, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.useCompToSingle;
    info.valueSize = sizeof(m_settings.useCompToSingle);
    m_settingsInfoMap.Insert(3110040174, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.forceRegularClearCode;
    info.valueSize = sizeof(m_settings.forceRegularClearCode);
    m_settingsInfoMap.Insert(2537383476, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.forceGraphicsFillMemoryPath;
    info.valueSize = sizeof(m_settings.forceGraphicsFillMemoryPath);
    m_settingsInfoMap.Insert(451570688, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waitOnMetadataMipTail;
    info.valueSize = sizeof(m_settings.waitOnMetadataMipTail);
    m_settingsInfoMap.Insert(2328100940, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.blendOptimizationsEnable;
    info.valueSize = sizeof(m_settings.blendOptimizationsEnable);
    m_settingsInfoMap.Insert(3560979294, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.fastColorClearEnable;
    info.valueSize = sizeof(m_settings.fastColorClearEnable);
    m_settingsInfoMap.Insert(1938040824, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.fastColorClearOn3dEnable;
    info.valueSize = sizeof(m_settings.fastColorClearOn3dEnable);
    m_settingsInfoMap.Insert(655987862, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.fmaskCompressDisable;
    info.valueSize = sizeof(m_settings.fmaskCompressDisable);
    m_settingsInfoMap.Insert(2717822859, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.fmaskAllowPipeBankXor;
    info.valueSize = sizeof(m_settings.fmaskAllowPipeBankXor);
    m_settingsInfoMap.Insert(4218731941, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.dccOnComputeEnable;
    info.valueSize = sizeof(m_settings.dccOnComputeEnable);
    m_settingsInfoMap.Insert(950561670, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.cbDbCachePolicy;
    info.valueSize = sizeof(m_settings.cbDbCachePolicy);
    m_settingsInfoMap.Insert(4090628834, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.csMaxWavesPerCu;
    info.valueSize = sizeof(m_settings.csMaxWavesPerCu);
    m_settingsInfoMap.Insert(4216700794, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.csLockThreshold;
    info.valueSize = sizeof(m_settings.csLockThreshold);
    m_settingsInfoMap.Insert(346110079, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.csSimdDestCntl;
    info.valueSize = sizeof(m_settings.csSimdDestCntl);
    m_settingsInfoMap.Insert(3574730191, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.htileEnable;
    info.valueSize = sizeof(m_settings.htileEnable);
    m_settingsInfoMap.Insert(2379988876, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.disableIterate256Opt;
    info.valueSize = sizeof(m_settings.disableIterate256Opt);
    m_settingsInfoMap.Insert(3148646221, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.allowDepthCopyResolve;
    info.valueSize = sizeof(m_settings.allowDepthCopyResolve);
    m_settingsInfoMap.Insert(3933921170, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.depthCompressEnable;
    info.valueSize = sizeof(m_settings.depthCompressEnable);
    m_settingsInfoMap.Insert(3404166969, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.stencilCompressEnable;
    info.valueSize = sizeof(m_settings.stencilCompressEnable);
    m_settingsInfoMap.Insert(3041432192, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dbPreloadEnable;
    info.valueSize = sizeof(m_settings.dbPreloadEnable);
    m_settingsInfoMap.Insert(2946289999, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dbPreloadWinEnable;
    info.valueSize = sizeof(m_settings.dbPreloadWinEnable);
    m_settingsInfoMap.Insert(4030437501, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dbPerTileExpClearEnable;
    info.valueSize = sizeof(m_settings.dbPerTileExpClearEnable);
    m_settingsInfoMap.Insert(399713165, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.hiDepthEnable;
    info.valueSize = sizeof(m_settings.hiDepthEnable);
    m_settingsInfoMap.Insert(950148604, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.hiStencilEnable;
    info.valueSize = sizeof(m_settings.hiStencilEnable);
    m_settingsInfoMap.Insert(37862373, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.dbRequestSize;
    info.valueSize = sizeof(m_settings.dbRequestSize);
    m_settingsInfoMap.Insert(2835145461, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dbDisableColorOnValidation;
    info.valueSize = sizeof(m_settings.dbDisableColorOnValidation);
    m_settingsInfoMap.Insert(4057416918, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.enableOutOfOrderPrimitives;
    info.valueSize = sizeof(m_settings.enableOutOfOrderPrimitives);
    m_settingsInfoMap.Insert(4194624623, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.outOfOrderWatermark;
    info.valueSize = sizeof(m_settings.outOfOrderWatermark);
    m_settingsInfoMap.Insert(2921949520, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.disableGeCntlVtxGrouping;
    info.valueSize = sizeof(m_settings.disableGeCntlVtxGrouping);
    m_settingsInfoMap.Insert(3045229933, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gsCuGroupEnabled;
    info.valueSize = sizeof(m_settings.gsCuGroupEnabled);
    m_settingsInfoMap.Insert(2295262967, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gsMaxLdsSize;
    info.valueSize = sizeof(m_settings.gsMaxLdsSize);
    m_settingsInfoMap.Insert(3033759533, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.lateAllocGs;
    info.valueSize = sizeof(m_settings.lateAllocGs);
    m_settingsInfoMap.Insert(1802508004, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.lateAllocVs;
    info.valueSize = sizeof(m_settings.lateAllocVs);
    m_settingsInfoMap.Insert(1805023933, info);

    info.type      = SettingType::Float;
    info.pValuePtr = &m_settings.maxTessFactor;
    info.valueSize = sizeof(m_settings.maxTessFactor);
    m_settingsInfoMap.Insert(3272504111, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.numOffchipLdsBuffers;
    info.valueSize = sizeof(m_settings.numOffchipLdsBuffers);
    m_settingsInfoMap.Insert(4150915470, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.numTessPatchesPerTg;
    info.valueSize = sizeof(m_settings.numTessPatchesPerTg);
    m_settingsInfoMap.Insert(2699532302, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.offchipLdsBufferSize;
    info.valueSize = sizeof(m_settings.offchipLdsBufferSize);
    m_settingsInfoMap.Insert(4262839798, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.isolineDistributionFactor;
    info.valueSize = sizeof(m_settings.isolineDistributionFactor);
    m_settingsInfoMap.Insert(951961633, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.triDistributionFactor;
    info.valueSize = sizeof(m_settings.triDistributionFactor);
    m_settingsInfoMap.Insert(2867566175, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.quadDistributionFactor;
    info.valueSize = sizeof(m_settings.quadDistributionFactor);
    m_settingsInfoMap.Insert(860015919, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.donutDistributionFactor;
    info.valueSize = sizeof(m_settings.donutDistributionFactor);
    m_settingsInfoMap.Insert(891881186, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.trapezoidDistributionFactor;
    info.valueSize = sizeof(m_settings.trapezoidDistributionFactor);
    m_settingsInfoMap.Insert(674984646, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.primGroupSize;
    info.valueSize = sizeof(m_settings.primGroupSize);
    m_settingsInfoMap.Insert(3402504325, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gfx9RbPlusEnable;
    info.valueSize = sizeof(m_settings.gfx9RbPlusEnable);
    m_settingsInfoMap.Insert(2122164302, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfx10SpiShaderLateAllocVsNumLines;
    info.valueSize = sizeof(m_settings.gfx10SpiShaderLateAllocVsNumLines);
    m_settingsInfoMap.Insert(1830704021, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfx10GePcAllocNumLinesPerSeLegacyNggPassthru;
    info.valueSize = sizeof(m_settings.gfx10GePcAllocNumLinesPerSeLegacyNggPassthru);
    m_settingsInfoMap.Insert(2459292446, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.allowNggOnAllCusWgps;
    info.valueSize = sizeof(m_settings.allowNggOnAllCusWgps);
    m_settingsInfoMap.Insert(101765188, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfx10GePcAllocNumLinesPerSeNggCulling;
    info.valueSize = sizeof(m_settings.gfx10GePcAllocNumLinesPerSeNggCulling);
    m_settingsInfoMap.Insert(665472713, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.numPsWavesSoftGroupedPerCu;
    info.valueSize = sizeof(m_settings.numPsWavesSoftGroupedPerCu);
    m_settingsInfoMap.Insert(1871590621, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.numVsWavesSoftGroupedPerCu;
    info.valueSize = sizeof(m_settings.numVsWavesSoftGroupedPerCu);
    m_settingsInfoMap.Insert(4021132771, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.switchVgtOnDraw;
    info.valueSize = sizeof(m_settings.switchVgtOnDraw);
    m_settingsInfoMap.Insert(1102013901, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.tessFactorBufferSizePerSe;
    info.valueSize = sizeof(m_settings.tessFactorBufferSizePerSe);
    m_settingsInfoMap.Insert(3112016659, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.disableTessDonutWalkPattern;
    info.valueSize = sizeof(m_settings.disableTessDonutWalkPattern);
    m_settingsInfoMap.Insert(2972449453, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.useMaxOffchipLdsBuffers;
    info.valueSize = sizeof(m_settings.useMaxOffchipLdsBuffers);
    m_settingsInfoMap.Insert(3365086421, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.vsHalfPackThreshold;
    info.valueSize = sizeof(m_settings.vsHalfPackThreshold);
    m_settingsInfoMap.Insert(987247393, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.vsForcePartialWave;
    info.valueSize = sizeof(m_settings.vsForcePartialWave);
    m_settingsInfoMap.Insert(1946161867, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.disableCoverageAaMask;
    info.valueSize = sizeof(m_settings.disableCoverageAaMask);
    m_settingsInfoMap.Insert(1829991091, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.batchBreakOnNewPixelShader;
    info.valueSize = sizeof(m_settings.batchBreakOnNewPixelShader);
    m_settingsInfoMap.Insert(3367458304, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.useClearStateToInitialize;
    info.valueSize = sizeof(m_settings.useClearStateToInitialize);
    m_settingsInfoMap.Insert(1269810789, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gsCuEnLimitMask;
    info.valueSize = sizeof(m_settings.gsCuEnLimitMask);
    m_settingsInfoMap.Insert(3021103171, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.vsCuEnLimitMask;
    info.valueSize = sizeof(m_settings.vsCuEnLimitMask);
    m_settingsInfoMap.Insert(989310036, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.psCuEnLimitMask;
    info.valueSize = sizeof(m_settings.psCuEnLimitMask);
    m_settingsInfoMap.Insert(1509811598, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.csCuEnLimitMask;
    info.valueSize = sizeof(m_settings.csCuEnLimitMask);
    m_settingsInfoMap.Insert(3382331351, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.shaderPrefetchSizeBytes;
    info.valueSize = sizeof(m_settings.shaderPrefetchSizeBytes);
    m_settingsInfoMap.Insert(2362905229, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.numTsMsDrawEntriesPerSe;
    info.valueSize = sizeof(m_settings.numTsMsDrawEntriesPerSe);
    m_settingsInfoMap.Insert(1975499791, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.nggSupported;
    info.valueSize = sizeof(m_settings.nggSupported);
    m_settingsInfoMap.Insert(2715700875, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.nggLateAllocGs;
    info.valueSize = sizeof(m_settings.nggLateAllocGs);
    m_settingsInfoMap.Insert(3217915194, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.binningMode;
    info.valueSize = sizeof(m_settings.binningMode);
    m_settingsInfoMap.Insert(4130214844, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.customBatchBinSize;
    info.valueSize = sizeof(m_settings.customBatchBinSize);
    m_settingsInfoMap.Insert(207210078, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.minBatchBinSize.width;
    info.valueSize = sizeof(m_settings.minBatchBinSize.width);
    m_settingsInfoMap.Insert(431998177, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.minBatchBinSize.height;
    info.valueSize = sizeof(m_settings.minBatchBinSize.height);
    m_settingsInfoMap.Insert(286301360, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.ignoreDepthForBinSizeIfColorBound;
    info.valueSize = sizeof(m_settings.ignoreDepthForBinSizeIfColorBound);
    m_settingsInfoMap.Insert(3483607475, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.disableBinningMode;
    info.valueSize = sizeof(m_settings.disableBinningMode);
    m_settingsInfoMap.Insert(636789469, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.disableBinningPsKill;
    info.valueSize = sizeof(m_settings.disableBinningPsKill);
    m_settingsInfoMap.Insert(1197165395, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.binningMaxAllocCountLegacy;
    info.valueSize = sizeof(m_settings.binningMaxAllocCountLegacy);
    m_settingsInfoMap.Insert(681698893, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.binningMaxAllocCountNggOnChip;
    info.valueSize = sizeof(m_settings.binningMaxAllocCountNggOnChip);
    m_settingsInfoMap.Insert(2543003509, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.binningMaxPrimPerBatch;
    info.valueSize = sizeof(m_settings.binningMaxPrimPerBatch);
    m_settingsInfoMap.Insert(1317079767, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.binningContextStatesPerBin;
    info.valueSize = sizeof(m_settings.binningContextStatesPerBin);
    m_settingsInfoMap.Insert(1661639333, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.binningPersistentStatesPerBin;
    info.valueSize = sizeof(m_settings.binningPersistentStatesPerBin);
    m_settingsInfoMap.Insert(850748547, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.binningFpovsPerBatch;
    info.valueSize = sizeof(m_settings.binningFpovsPerBatch);
    m_settingsInfoMap.Insert(2093710317, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.binningOptimalBinSelection;
    info.valueSize = sizeof(m_settings.binningOptimalBinSelection);
    m_settingsInfoMap.Insert(456360427, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.disableBinningAppendConsume;
    info.valueSize = sizeof(m_settings.disableBinningAppendConsume);
    m_settingsInfoMap.Insert(380189375, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.shaderPrefetchMethod;
    info.valueSize = sizeof(m_settings.shaderPrefetchMethod);
    m_settingsInfoMap.Insert(3028994822, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.prefetchCommandBuffers;
    info.valueSize = sizeof(m_settings.prefetchCommandBuffers);
    m_settingsInfoMap.Insert(3867574326, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.anisoFilterOptEnabled;
    info.valueSize = sizeof(m_settings.anisoFilterOptEnabled);
    m_settingsInfoMap.Insert(958470227, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.samplerCeilingLogicEnabled;
    info.valueSize = sizeof(m_settings.samplerCeilingLogicEnabled);
    m_settingsInfoMap.Insert(2986992899, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.samplerPrecisionFixEnabled;
    info.valueSize = sizeof(m_settings.samplerPrecisionFixEnabled);
    m_settingsInfoMap.Insert(286847775, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.samplerPerfMip;
    info.valueSize = sizeof(m_settings.samplerPerfMip);
    m_settingsInfoMap.Insert(3696903510, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.samplerAnisoThreshold;
    info.valueSize = sizeof(m_settings.samplerAnisoThreshold);
    m_settingsInfoMap.Insert(1521283108, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.samplerAnisoBias;
    info.valueSize = sizeof(m_settings.samplerAnisoBias);
    m_settingsInfoMap.Insert(4085812096, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.samplerSecAnisoBias;
    info.valueSize = sizeof(m_settings.samplerSecAnisoBias);
    m_settingsInfoMap.Insert(2654965201, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waRestrictMetaDataUseInMipTail;
    info.valueSize = sizeof(m_settings.waRestrictMetaDataUseInMipTail);
    m_settingsInfoMap.Insert(599120928, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.waVrsStencilUav;
    info.valueSize = sizeof(m_settings.waVrsStencilUav);
    m_settingsInfoMap.Insert(3499388342, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waLogicOpDisablesOverwriteCombiner;
    info.valueSize = sizeof(m_settings.waLogicOpDisablesOverwriteCombiner);
    m_settingsInfoMap.Insert(2566203469, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waRotatedSwizzleDisablesOverwriteCombiner;
    info.valueSize = sizeof(m_settings.waRotatedSwizzleDisablesOverwriteCombiner);
    m_settingsInfoMap.Insert(863498563, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waDisableFmaskNofetchOpOnFmaskCompressionDisable;
    info.valueSize = sizeof(m_settings.waDisableFmaskNofetchOpOnFmaskCompressionDisable);
    m_settingsInfoMap.Insert(1971936918, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waDisableVrsWithDsExports;
    info.valueSize = sizeof(m_settings.waDisableVrsWithDsExports);
    m_settingsInfoMap.Insert(2338588964, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waFixPostZConservativeRasterization;
    info.valueSize = sizeof(m_settings.waFixPostZConservativeRasterization);
    m_settingsInfoMap.Insert(3779046012, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waClampQuadDistributionFactor;
    info.valueSize = sizeof(m_settings.waClampQuadDistributionFactor);
    m_settingsInfoMap.Insert(2022937678, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waWrite1xAASampleLocationsToZero;
    info.valueSize = sizeof(m_settings.waWrite1xAASampleLocationsToZero);
    m_settingsInfoMap.Insert(2042380720, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waColorCacheControllerInvalidEviction;
    info.valueSize = sizeof(m_settings.waColorCacheControllerInvalidEviction);
    m_settingsInfoMap.Insert(2330368444, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waForceZonlyHtileForMipmaps;
    info.valueSize = sizeof(m_settings.waForceZonlyHtileForMipmaps);
    m_settingsInfoMap.Insert(860624612, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waOverwriteCombinerTargetMaskOnly;
    info.valueSize = sizeof(m_settings.waOverwriteCombinerTargetMaskOnly);
    m_settingsInfoMap.Insert(1670732044, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waDisableHtilePrefetch;
    info.valueSize = sizeof(m_settings.waDisableHtilePrefetch);
    m_settingsInfoMap.Insert(4011209522, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waMiscPopsMissedOverlap;
    info.valueSize = sizeof(m_settings.waMiscPopsMissedOverlap);
    m_settingsInfoMap.Insert(2170713611, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waMiscScissorRegisterChange;
    info.valueSize = sizeof(m_settings.waMiscScissorRegisterChange);
    m_settingsInfoMap.Insert(3580876344, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waMiscPsFlushScissorChange;
    info.valueSize = sizeof(m_settings.waMiscPsFlushScissorChange);
    m_settingsInfoMap.Insert(2611268564, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waHtilePipeBankXorMustBeZero;
    info.valueSize = sizeof(m_settings.waHtilePipeBankXorMustBeZero);
    m_settingsInfoMap.Insert(264312760, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waDisableDfsmWithEqaa;
    info.valueSize = sizeof(m_settings.waDisableDfsmWithEqaa);
    m_settingsInfoMap.Insert(3435751213, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waDisable24BitHWFormatForTCCompatibleDepth;
    info.valueSize = sizeof(m_settings.waDisable24BitHWFormatForTCCompatibleDepth);
    m_settingsInfoMap.Insert(1925370123, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waDummyZpassDoneBeforeTs;
    info.valueSize = sizeof(m_settings.waDummyZpassDoneBeforeTs);
    m_settingsInfoMap.Insert(814916412, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waMetaAliasingFixEnabled;
    info.valueSize = sizeof(m_settings.waMetaAliasingFixEnabled);
    m_settingsInfoMap.Insert(3182155668, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waForce256bCbFetch;
    info.valueSize = sizeof(m_settings.waForce256bCbFetch);
    m_settingsInfoMap.Insert(2944333716, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waCmaskImageSyncs;
    info.valueSize = sizeof(m_settings.waCmaskImageSyncs);
    m_settingsInfoMap.Insert(3002384369, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waSdmaPreventCompressedSurfUse;
    info.valueSize = sizeof(m_settings.waSdmaPreventCompressedSurfUse);
    m_settingsInfoMap.Insert(1236556278, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waVgtFlushNggToLegacyGs;
    info.valueSize = sizeof(m_settings.waVgtFlushNggToLegacyGs);
    m_settingsInfoMap.Insert(1821581352, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waVgtFlushNggToLegacy;
    info.valueSize = sizeof(m_settings.waVgtFlushNggToLegacy);
    m_settingsInfoMap.Insert(3701186862, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waEnableIndexBufferPrefetchForNgg;
    info.valueSize = sizeof(m_settings.waEnableIndexBufferPrefetchForNgg);
    m_settingsInfoMap.Insert(816147502, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waZ16Unorm1xAaDecompressUninitialized;
    info.valueSize = sizeof(m_settings.waZ16Unorm1xAaDecompressUninitialized);
    m_settingsInfoMap.Insert(575442222, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waLateAllocGs0;
    info.valueSize = sizeof(m_settings.waLateAllocGs0);
    m_settingsInfoMap.Insert(1163996140, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waIndexBufferZeroSize;
    info.valueSize = sizeof(m_settings.waIndexBufferZeroSize);
    m_settingsInfoMap.Insert(2561313302, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waStalledPopsMode;
    info.valueSize = sizeof(m_settings.waStalledPopsMode);
    m_settingsInfoMap.Insert(54918207, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waCeDisableIb2;
    info.valueSize = sizeof(m_settings.waCeDisableIb2);
    m_settingsInfoMap.Insert(2126259346, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waDisableSCompressSOnly;
    info.valueSize = sizeof(m_settings.waDisableSCompressSOnly);
    m_settingsInfoMap.Insert(1364141327, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waDisableInstancePacking;
    info.valueSize = sizeof(m_settings.waDisableInstancePacking);
    m_settingsInfoMap.Insert(2220232767, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.vrsHtileEncoding;
    info.valueSize = sizeof(m_settings.vrsHtileEncoding);
    m_settingsInfoMap.Insert(4142650735, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.vrsForceRateFine;
    info.valueSize = sizeof(m_settings.vrsForceRateFine);
    m_settingsInfoMap.Insert(490938231, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.privateDepthIsHtileOnly;
    info.valueSize = sizeof(m_settings.privateDepthIsHtileOnly);
    m_settingsInfoMap.Insert(1433516561, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.optimizeNullSourceImage;
    info.valueSize = sizeof(m_settings.optimizeNullSourceImage);
    m_settingsInfoMap.Insert(2743300351, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.vrsImageSize;
    info.valueSize = sizeof(m_settings.vrsImageSize);
    m_settingsInfoMap.Insert(2610878234, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waUtcL0InconsistentBigPage;
    info.valueSize = sizeof(m_settings.waUtcL0InconsistentBigPage);
    m_settingsInfoMap.Insert(1748539367, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waTwoPlanesIterate256;
    info.valueSize = sizeof(m_settings.waTwoPlanesIterate256);
    m_settingsInfoMap.Insert(164975373, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waTessIncorrectRelativeIndex;
    info.valueSize = sizeof(m_settings.waTessIncorrectRelativeIndex);
    m_settingsInfoMap.Insert(3815932601, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waLimitLateAllocGsNggFifo;
    info.valueSize = sizeof(m_settings.waLimitLateAllocGsNggFifo);
    m_settingsInfoMap.Insert(3170186115, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waClampGeCntlVertGrpSize;
    info.valueSize = sizeof(m_settings.waClampGeCntlVertGrpSize);
    m_settingsInfoMap.Insert(4183598840, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waLegacyGsCutModeFlush;
    info.valueSize = sizeof(m_settings.waLegacyGsCutModeFlush);
    m_settingsInfoMap.Insert(1477053247, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.sdmaBypassMall;
    info.valueSize = sizeof(m_settings.sdmaBypassMall);
    m_settingsInfoMap.Insert(881287982, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.enableMallCursorCache;
    info.valueSize = sizeof(m_settings.enableMallCursorCache);
    m_settingsInfoMap.Insert(2467252640, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.depthStencilFastClearComputeThresholdSingleSampled;
    info.valueSize = sizeof(m_settings.depthStencilFastClearComputeThresholdSingleSampled);
    m_settingsInfoMap.Insert(2634603321, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.depthStencilFastClearComputeThresholdMultiSampled;
    info.valueSize = sizeof(m_settings.depthStencilFastClearComputeThresholdMultiSampled);
    m_settingsInfoMap.Insert(2782857680, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfx10MaxFpovsInWave;
    info.valueSize = sizeof(m_settings.gfx10MaxFpovsInWave);
    m_settingsInfoMap.Insert(3399078965, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.disableAceCsPartialFlush;
    info.valueSize = sizeof(m_settings.disableAceCsPartialFlush);
    m_settingsInfoMap.Insert(4181362005, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.addrLibGbAddrConfigOverride;
    info.valueSize = sizeof(m_settings.addrLibGbAddrConfigOverride);
    m_settingsInfoMap.Insert(4156569361, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.nonLocalDestPreferCompute;
    info.valueSize = sizeof(m_settings.nonLocalDestPreferCompute);
    m_settingsInfoMap.Insert(2724905730, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gfx103DisableAsymmetricWgpForPs;
    info.valueSize = sizeof(m_settings.gfx103DisableAsymmetricWgpForPs);
    m_settingsInfoMap.Insert(2671208712, info);

}

// =====================================================================================================================
// Registers the core settings with the Developer Driver settings service.
void SettingsLoader::DevDriverRegister()
{
    auto* pDevDriverServer = static_cast<Pal::Device*>(m_pDevice)->GetPlatform()->GetDevDriverServer();
    if (pDevDriverServer != nullptr)
    {
        auto* pSettingsService = pDevDriverServer->GetSettingsService();
        if (pSettingsService != nullptr)
        {
            RegisteredComponent component = {};
            strncpy(&component.componentName[0], m_pComponentName, kMaxComponentNameStrLen);
            component.pPrivateData = static_cast<void*>(this);
            component.pSettingsHashes = &g_gfx9PalSettingHashList[0];
            component.numSettings = g_gfx9PalNumSettings;
            component.pfnGetValue = ISettingsLoader::GetValue;
            component.pfnSetValue = ISettingsLoader::SetValue;
            component.pSettingsData = &g_gfx9PalJsonData[0];
            component.settingsDataSize = sizeof(g_gfx9PalJsonData);
            component.settingsDataHash = 136698542;
            component.settingsDataHeader.isEncoded = true;
            component.settingsDataHeader.magicBufferId = 402778310;
            component.settingsDataHeader.magicBufferOffset = 0;

            pSettingsService->RegisterComponent(component);
        }
    }
}

} // Gfx9
} // Pal
