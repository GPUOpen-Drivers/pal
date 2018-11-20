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

#include "core/device.h"
#include "core/settingsLoader.h"

#include "core/hw/gfxip/gfx6/gfx6SettingsLoader.h"
#include "core/hw/gfxip/gfx6/g_gfx6PalSettings.h"
#include "palInlineFuncs.h"
#include "palHashMapImpl.h"

#include "devDriverServer.h"
#include "protocols/ddSettingsService.h"

using namespace DevDriver::SettingsURIService;

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
// Initializes the settings structure to default values.
void SettingsLoader::SetupDefaults()
{
    // set setting variables to their default values...
    m_settings.enableLoadIndexForObjectBinds = true;
    m_settings.cpDmaSrcAlignment = CpDmaAlignmentDefault;
    m_settings.disableBorderColorPaletteBinds = false;
    m_settings.forceOcclusionQueryResult = false;
    m_settings.forceOcclusionQueryResultValue = 1;
    m_settings.primGroupSize = 128;
    m_settings.dynamicPrimGroupEnable = true;
    m_settings.dynamicPrimGroupMin = 32;
    m_settings.dynamicPrimGroupMax = 128;
    m_settings.dynamicPrimGroupStep = 32;
    m_settings.dynamicPrimGroupWindowSize = 32;
    m_settings.switchVgtOnDraw = false;
    m_settings.esGsRatio = 4;
    m_settings.vsForcePartialWave = false;
    m_settings.vsHalfPackThreshold = 16;
    m_settings.esCuGroupEnabled = false;
    m_settings.gsCuGroupEnabled = false;
    m_settings.vsCuGroupEnabled = false;
    m_settings.psCuGroupEnabled = true;
    m_settings.tessFactorBufferSizePerSe = 4096;
    m_settings.disableTessDonutWalkPattern = 0;
    m_settings.maxTessFactor = 64.0;
    m_settings.numTessPatchesPerTg = 0;
    m_settings.numOffchipLdsBuffers = 508;
    m_settings.useMaxOffchipLdsBuffers = true;
    m_settings.dsWavesPerSimdOverflow = 4;
    m_settings.offchipTfDegree = 4.0;
    m_settings.gfx6OffChipHsSkipDataCopyNullPatch = false;
    m_settings.gfxMaxWavesPerCu = 0;
    m_settings.lsCuEnLimitMask = 0xffffffffL;
    m_settings.esCuEnLimitMask = 0xffffffffL;
    m_settings.gsCuEnLimitMask = 0xffffffffL;
    m_settings.vsCuEnLimitMask = 0xffffffffL;
    m_settings.psCuEnLimitMask = 0xffffffffL;
    m_settings.csCuEnLimitMask = 0xffffffffL;
    m_settings.csMaxWavesPerCu = 0;
    m_settings.csLockThreshold = 0;
    m_settings.csSimdDestCntl = CsSimdDestCntlDefault;
    m_settings.anisoFilterOptEnabled = false;
    m_settings.samplerCeilingLogicEnabled = false;
    m_settings.samplerPrecisionFixEnabled = true;
    m_settings.samplerPerfMip = 0;
    m_settings.samplerAnisoThreshold = 0;
    m_settings.samplerAnisoBias = 0;
    m_settings.samplerSecAnisoBias = 0;
    m_settings.fastColorClearEnable = true;
    m_settings.fastColorClearOn3dEnable = true;
    m_settings.fmaskCompressEnable = true;
    m_settings.blendOptimizationsEnable = true;
    m_settings.htileEnable = true;
    m_settings.depthCompressEnable = true;
    m_settings.stencilCompressEnable = true;
    m_settings.linearHtileEnable = false;
    m_settings.dbPreloadEnable = false;
    m_settings.dbPreloadWinEnable = false;
    m_settings.dbPerTileExpClearEnable = false;
    m_settings.hiDepthEnable = true;
    m_settings.hiStencilEnable = true;
    m_settings.dbRequestSize = 0x0;
    m_settings.dbAddr5SwizzleMask = 0x1;
    m_settings.dbDisableColorOnValidation = false;
    m_settings.enableSeparateAspectMetadataInit = true;
    m_settings.gfx7VsPartialWaveWithEoiEnabled = false;
    m_settings.gfx7OffchipLdsBufferSize = OffchipLdsBufferSize8192;
    m_settings.gfx7LateAllocVsOnCuAlwaysOn = false;
    m_settings.gfx7EnableOutOfOrderPrimitives = OutOfOrderPrimSafe;
    m_settings.gfx7OutOfOrderWatermark = 7;
    m_settings.gfx7GsMaxLdsSize = 8192;
    m_settings.gfx7EnableOnchipGs = true;
    m_settings.gfx7GsOffChipThreshold = 64;
    m_settings.gfx7IdealGsPrimsPerSubGroup = 64;
    m_settings.gfx7AvoidVgtNullPrims = false;
    m_settings.gfx8PatchDistributionFactor = 8;
    m_settings.gfx8DonutDistributionFactor = 8;
    m_settings.gfx8TrapezoidDistributionFactor = 8;
    m_settings.gfx8UseDcc = 0xf3;
    m_settings.gfx8AlwaysDecompress = 0x0;
    m_settings.gfx8RbPlusEnable = true;
    m_settings.gfx8FastClearAllTcCompatColorSurfs = 3;
    m_settings.gfx8CheckMetaDataFetchFromStartMip = 0x3;
    m_settings.gfx8IgnoreMipInterleave = false;
    m_settings.waMiscGsNullPrim = false;
    m_settings.numSettings = g_gfx6PalNumSettings;
}

// =====================================================================================================================
// Reads the setting from the OS adapter and sets the structure value when the setting values are found.
void SettingsLoader::ReadSettings()
{
    // read from the OS adapter for each individual setting
    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pEnableLoadIndexForObjectBindsStr,
                           Util::ValueType::Boolean,
                           &m_settings.enableLoadIndexForObjectBinds,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCpDmaSrcAlignmentStr,
                           Util::ValueType::Uint,
                           &m_settings.cpDmaSrcAlignment,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDisableBorderColorPaletteBindsStr,
                           Util::ValueType::Boolean,
                           &m_settings.disableBorderColorPaletteBinds,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pForceOcclusionQueryResultStr,
                           Util::ValueType::Boolean,
                           &m_settings.forceOcclusionQueryResult,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pForceOcclusionQueryResultValueStr,
                           Util::ValueType::Uint,
                           &m_settings.forceOcclusionQueryResultValue,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPrimgroupSizeStr,
                           Util::ValueType::Uint,
                           &m_settings.primGroupSize,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDynamicPrimgroupEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.dynamicPrimGroupEnable,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDynamicPrimgroupMinStr,
                           Util::ValueType::Uint,
                           &m_settings.dynamicPrimGroupMin,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDynamicPrimgroupMaxStr,
                           Util::ValueType::Uint,
                           &m_settings.dynamicPrimGroupMax,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDynamicPrimgroupStepStr,
                           Util::ValueType::Uint,
                           &m_settings.dynamicPrimGroupStep,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDynamicPrimgroupWindowSizeStr,
                           Util::ValueType::Uint,
                           &m_settings.dynamicPrimGroupWindowSize,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pSwitchVgtOnDrawStr,
                           Util::ValueType::Boolean,
                           &m_settings.switchVgtOnDraw,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pEsGsRatioStr,
                           Util::ValueType::Uint,
                           &m_settings.esGsRatio,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pVsForcePartialWaveStr,
                           Util::ValueType::Boolean,
                           &m_settings.vsForcePartialWave,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pVsHalfPackThresholdStr,
                           Util::ValueType::Uint,
                           &m_settings.vsHalfPackThreshold,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pEsCuGroupEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.esCuGroupEnabled,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGsCuGroupEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.gsCuGroupEnabled,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pVsCuGroupEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.vsCuGroupEnabled,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPsCuGroupEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.psCuGroupEnabled,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pTessFactorBufferSizePerSeStr,
                           Util::ValueType::Uint,
                           &m_settings.tessFactorBufferSizePerSe,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pTessWalkPatternStr,
                           Util::ValueType::Uint,
                           &m_settings.disableTessDonutWalkPattern,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pMaxTessFactorStr,
                           Util::ValueType::Float,
                           &m_settings.maxTessFactor,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pNumTessPatchesPerThreadGroupStr,
                           Util::ValueType::Uint,
                           &m_settings.numTessPatchesPerTg,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pNumOffchipLdsBuffersStr,
                           Util::ValueType::Uint,
                           &m_settings.numOffchipLdsBuffers,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pUseMaxOffchipLdsBuffersStr,
                           Util::ValueType::Boolean,
                           &m_settings.useMaxOffchipLdsBuffers,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pTessDsWavesPerSimdOverflowStr,
                           Util::ValueType::Uint,
                           &m_settings.dsWavesPerSimdOverflow,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pTessOffchipTfDegreeStr,
                           Util::ValueType::Float,
                           &m_settings.offchipTfDegree,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pOffChipHsSkipDataCopyNullPatchStr,
                           Util::ValueType::Boolean,
                           &m_settings.gfx6OffChipHsSkipDataCopyNullPatch,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGfxMaxWavesPerCuStr,
                           Util::ValueType::Uint,
                           &m_settings.gfxMaxWavesPerCu,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pLsCuEnLimitMaskStr,
                           Util::ValueType::Uint,
                           &m_settings.lsCuEnLimitMask,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pEsCuEnLimitMaskStr,
                           Util::ValueType::Uint,
                           &m_settings.esCuEnLimitMask,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGsCuEnLimitMaskStr,
                           Util::ValueType::Uint,
                           &m_settings.gsCuEnLimitMask,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pVsCuEnLimitMaskStr,
                           Util::ValueType::Uint,
                           &m_settings.vsCuEnLimitMask,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPsCuEnLimitMaskStr,
                           Util::ValueType::Uint,
                           &m_settings.psCuEnLimitMask,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCsCuEnLimitMaskStr,
                           Util::ValueType::Uint,
                           &m_settings.csCuEnLimitMask,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCsMaxWavesPerCuStr,
                           Util::ValueType::Uint,
                           &m_settings.csMaxWavesPerCu,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCsLockThresholdStr,
                           Util::ValueType::Uint,
                           &m_settings.csLockThreshold,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCsSimdDestCntlStr,
                           Util::ValueType::Uint,
                           &m_settings.csSimdDestCntl,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAnisoFilterOptEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.anisoFilterOptEnabled,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCeilingLogicEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.samplerCeilingLogicEnabled,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPrecisionFixEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.samplerPrecisionFixEnabled,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pSamplerPerfMipStr,
                           Util::ValueType::Uint,
                           &m_settings.samplerPerfMip,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pSamplerAnisoThresholdStr,
                           Util::ValueType::Uint,
                           &m_settings.samplerAnisoThreshold,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pSamplerAnisoBiasStr,
                           Util::ValueType::Uint,
                           &m_settings.samplerAnisoBias,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pSamplerSecAnisoBiasStr,
                           Util::ValueType::Uint,
                           &m_settings.samplerSecAnisoBias,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pFastColorClearEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.fastColorClearEnable,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pFastColorClearOn3DEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.fastColorClearOn3dEnable,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pFmaskCompressEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.fmaskCompressEnable,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pBlendOptimizationEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.blendOptimizationsEnable,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pHtileEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.htileEnable,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDepthCompressEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.depthCompressEnable,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pStencilCompressEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.stencilCompressEnable,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pLinearHtileEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.linearHtileEnable,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDbPreloadEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.dbPreloadEnable,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDbPreloadWinEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.dbPreloadWinEnable,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDbPerTileExpClearEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.dbPerTileExpClearEnable,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pHiDepthEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.hiDepthEnable,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pHiStencilEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.hiStencilEnable,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDbRequestSizeStr,
                           Util::ValueType::Uint,
                           &m_settings.dbRequestSize,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDbAddr5SwizzleMaskStr,
                           Util::ValueType::Uint,
                           &m_settings.dbAddr5SwizzleMask,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDbDisableColorOnValidationStr,
                           Util::ValueType::Boolean,
                           &m_settings.dbDisableColorOnValidation,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pEnableSeparateAspectMetadataInitStr,
                           Util::ValueType::Boolean,
                           &m_settings.enableSeparateAspectMetadataInit,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pVsPartialWaveWithEoiEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.gfx7VsPartialWaveWithEoiEnabled,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pOffchipLdsBufferSizeStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx7OffchipLdsBufferSize,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pLateAllocVsOnCuAlwaysOnStr,
                           Util::ValueType::Boolean,
                           &m_settings.gfx7LateAllocVsOnCuAlwaysOn,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pEnableOutOfOrderPrimitivesStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx7EnableOutOfOrderPrimitives,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pOutOfOrderWatermarkStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx7OutOfOrderWatermark,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGsMaxLdsSizeStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx7GsMaxLdsSize,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pEnableOnchipGsStr,
                           Util::ValueType::Boolean,
                           &m_settings.gfx7EnableOnchipGs,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGsOffChipThresholdStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx7GsOffChipThreshold,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pGsPrimsPerSubGroupStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx7IdealGsPrimsPerSubGroup,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAvoidVgtNullPrimsStr,
                           Util::ValueType::Boolean,
                           &m_settings.gfx7AvoidVgtNullPrims,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPatchDistributionFactorStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx8PatchDistributionFactor,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDonutDistributionFactorStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx8DonutDistributionFactor,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pTrapezoidDistributionFactorStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx8TrapezoidDistributionFactor,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pUseDccStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx8UseDcc,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAlwaysDecompressStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx8AlwaysDecompress,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pRbPlusEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.gfx8RbPlusEnable,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pFastClearAllTcCompatColorSurfsStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx8FastClearAllTcCompatColorSurfs,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCheckMetaDataFetchFromStartMipStr,
                           Util::ValueType::Uint,
                           &m_settings.gfx8CheckMetaDataFetchFromStartMip,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pIgnoreMipInterleaveStr,
                           Util::ValueType::Boolean,
                           &m_settings.gfx8IgnoreMipInterleave,
                           InternalSettingScope::PrivatePalGfx6Key);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pWaMiscGsNullPrimStr,
                           Util::ValueType::Boolean,
                           &m_settings.waMiscGsNullPrim,
                           InternalSettingScope::PrivatePalGfx6Key);

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
    info.pValuePtr = &m_settings.cpDmaSrcAlignment;
    info.valueSize = sizeof(m_settings.cpDmaSrcAlignment);
    m_settingsInfoMap.Insert(2945629691, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.disableBorderColorPaletteBinds;
    info.valueSize = sizeof(m_settings.disableBorderColorPaletteBinds);
    m_settingsInfoMap.Insert(3825276041, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.forceOcclusionQueryResult;
    info.valueSize = sizeof(m_settings.forceOcclusionQueryResult);
    m_settingsInfoMap.Insert(1419797586, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.forceOcclusionQueryResultValue;
    info.valueSize = sizeof(m_settings.forceOcclusionQueryResultValue);
    m_settingsInfoMap.Insert(3761151579, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.primGroupSize;
    info.valueSize = sizeof(m_settings.primGroupSize);
    m_settingsInfoMap.Insert(3402504325, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dynamicPrimGroupEnable;
    info.valueSize = sizeof(m_settings.dynamicPrimGroupEnable);
    m_settingsInfoMap.Insert(1334465030, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.dynamicPrimGroupMin;
    info.valueSize = sizeof(m_settings.dynamicPrimGroupMin);
    m_settingsInfoMap.Insert(4066308367, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.dynamicPrimGroupMax;
    info.valueSize = sizeof(m_settings.dynamicPrimGroupMax);
    m_settingsInfoMap.Insert(3763031297, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.dynamicPrimGroupStep;
    info.valueSize = sizeof(m_settings.dynamicPrimGroupStep);
    m_settingsInfoMap.Insert(2488885191, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.dynamicPrimGroupWindowSize;
    info.valueSize = sizeof(m_settings.dynamicPrimGroupWindowSize);
    m_settingsInfoMap.Insert(1070254748, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.switchVgtOnDraw;
    info.valueSize = sizeof(m_settings.switchVgtOnDraw);
    m_settingsInfoMap.Insert(1102013901, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.esGsRatio;
    info.valueSize = sizeof(m_settings.esGsRatio);
    m_settingsInfoMap.Insert(3217804174, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.vsForcePartialWave;
    info.valueSize = sizeof(m_settings.vsForcePartialWave);
    m_settingsInfoMap.Insert(1946161867, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.vsHalfPackThreshold;
    info.valueSize = sizeof(m_settings.vsHalfPackThreshold);
    m_settingsInfoMap.Insert(987247393, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.esCuGroupEnabled;
    info.valueSize = sizeof(m_settings.esCuGroupEnabled);
    m_settingsInfoMap.Insert(1448769209, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gsCuGroupEnabled;
    info.valueSize = sizeof(m_settings.gsCuGroupEnabled);
    m_settingsInfoMap.Insert(2295262967, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.vsCuGroupEnabled;
    info.valueSize = sizeof(m_settings.vsCuGroupEnabled);
    m_settingsInfoMap.Insert(1712293842, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.psCuGroupEnabled;
    info.valueSize = sizeof(m_settings.psCuGroupEnabled);
    m_settingsInfoMap.Insert(1924559864, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.tessFactorBufferSizePerSe;
    info.valueSize = sizeof(m_settings.tessFactorBufferSizePerSe);
    m_settingsInfoMap.Insert(3112016659, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.disableTessDonutWalkPattern;
    info.valueSize = sizeof(m_settings.disableTessDonutWalkPattern);
    m_settingsInfoMap.Insert(2972449453, info);

    info.type      = SettingType::Float;
    info.pValuePtr = &m_settings.maxTessFactor;
    info.valueSize = sizeof(m_settings.maxTessFactor);
    m_settingsInfoMap.Insert(3272504111, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.numTessPatchesPerTg;
    info.valueSize = sizeof(m_settings.numTessPatchesPerTg);
    m_settingsInfoMap.Insert(2699532302, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.numOffchipLdsBuffers;
    info.valueSize = sizeof(m_settings.numOffchipLdsBuffers);
    m_settingsInfoMap.Insert(4150915470, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.useMaxOffchipLdsBuffers;
    info.valueSize = sizeof(m_settings.useMaxOffchipLdsBuffers);
    m_settingsInfoMap.Insert(3365086421, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.dsWavesPerSimdOverflow;
    info.valueSize = sizeof(m_settings.dsWavesPerSimdOverflow);
    m_settingsInfoMap.Insert(3832811323, info);

    info.type      = SettingType::Float;
    info.pValuePtr = &m_settings.offchipTfDegree;
    info.valueSize = sizeof(m_settings.offchipTfDegree);
    m_settingsInfoMap.Insert(3548610473, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gfx6OffChipHsSkipDataCopyNullPatch;
    info.valueSize = sizeof(m_settings.gfx6OffChipHsSkipDataCopyNullPatch);
    m_settingsInfoMap.Insert(1952167388, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfxMaxWavesPerCu;
    info.valueSize = sizeof(m_settings.gfxMaxWavesPerCu);
    m_settingsInfoMap.Insert(4080017031, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.lsCuEnLimitMask;
    info.valueSize = sizeof(m_settings.lsCuEnLimitMask);
    m_settingsInfoMap.Insert(1219961810, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.esCuEnLimitMask;
    info.valueSize = sizeof(m_settings.esCuEnLimitMask);
    m_settingsInfoMap.Insert(1411431225, info);

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
    info.pValuePtr = &m_settings.fastColorClearEnable;
    info.valueSize = sizeof(m_settings.fastColorClearEnable);
    m_settingsInfoMap.Insert(1938040824, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.fastColorClearOn3dEnable;
    info.valueSize = sizeof(m_settings.fastColorClearOn3dEnable);
    m_settingsInfoMap.Insert(655987862, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.fmaskCompressEnable;
    info.valueSize = sizeof(m_settings.fmaskCompressEnable);
    m_settingsInfoMap.Insert(1936153062, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.blendOptimizationsEnable;
    info.valueSize = sizeof(m_settings.blendOptimizationsEnable);
    m_settingsInfoMap.Insert(3560979294, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.htileEnable;
    info.valueSize = sizeof(m_settings.htileEnable);
    m_settingsInfoMap.Insert(2379988876, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.depthCompressEnable;
    info.valueSize = sizeof(m_settings.depthCompressEnable);
    m_settingsInfoMap.Insert(3404166969, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.stencilCompressEnable;
    info.valueSize = sizeof(m_settings.stencilCompressEnable);
    m_settingsInfoMap.Insert(3041432192, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.linearHtileEnable;
    info.valueSize = sizeof(m_settings.linearHtileEnable);
    m_settingsInfoMap.Insert(3857035179, info);

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

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.dbAddr5SwizzleMask;
    info.valueSize = sizeof(m_settings.dbAddr5SwizzleMask);
    m_settingsInfoMap.Insert(1221993759, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dbDisableColorOnValidation;
    info.valueSize = sizeof(m_settings.dbDisableColorOnValidation);
    m_settingsInfoMap.Insert(4057416918, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.enableSeparateAspectMetadataInit;
    info.valueSize = sizeof(m_settings.enableSeparateAspectMetadataInit);
    m_settingsInfoMap.Insert(250077184, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gfx7VsPartialWaveWithEoiEnabled;
    info.valueSize = sizeof(m_settings.gfx7VsPartialWaveWithEoiEnabled);
    m_settingsInfoMap.Insert(392471174, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfx7OffchipLdsBufferSize;
    info.valueSize = sizeof(m_settings.gfx7OffchipLdsBufferSize);
    m_settingsInfoMap.Insert(4262839798, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gfx7LateAllocVsOnCuAlwaysOn;
    info.valueSize = sizeof(m_settings.gfx7LateAllocVsOnCuAlwaysOn);
    m_settingsInfoMap.Insert(1551275668, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfx7EnableOutOfOrderPrimitives;
    info.valueSize = sizeof(m_settings.gfx7EnableOutOfOrderPrimitives);
    m_settingsInfoMap.Insert(4194624623, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfx7OutOfOrderWatermark;
    info.valueSize = sizeof(m_settings.gfx7OutOfOrderWatermark);
    m_settingsInfoMap.Insert(2921949520, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfx7GsMaxLdsSize;
    info.valueSize = sizeof(m_settings.gfx7GsMaxLdsSize);
    m_settingsInfoMap.Insert(3033759533, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gfx7EnableOnchipGs;
    info.valueSize = sizeof(m_settings.gfx7EnableOnchipGs);
    m_settingsInfoMap.Insert(4034461831, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfx7GsOffChipThreshold;
    info.valueSize = sizeof(m_settings.gfx7GsOffChipThreshold);
    m_settingsInfoMap.Insert(1659075697, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfx7IdealGsPrimsPerSubGroup;
    info.valueSize = sizeof(m_settings.gfx7IdealGsPrimsPerSubGroup);
    m_settingsInfoMap.Insert(307437762, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gfx7AvoidVgtNullPrims;
    info.valueSize = sizeof(m_settings.gfx7AvoidVgtNullPrims);
    m_settingsInfoMap.Insert(1901956459, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfx8PatchDistributionFactor;
    info.valueSize = sizeof(m_settings.gfx8PatchDistributionFactor);
    m_settingsInfoMap.Insert(3445840960, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfx8DonutDistributionFactor;
    info.valueSize = sizeof(m_settings.gfx8DonutDistributionFactor);
    m_settingsInfoMap.Insert(891881186, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfx8TrapezoidDistributionFactor;
    info.valueSize = sizeof(m_settings.gfx8TrapezoidDistributionFactor);
    m_settingsInfoMap.Insert(674984646, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfx8UseDcc;
    info.valueSize = sizeof(m_settings.gfx8UseDcc);
    m_settingsInfoMap.Insert(4029518654, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfx8AlwaysDecompress;
    info.valueSize = sizeof(m_settings.gfx8AlwaysDecompress);
    m_settingsInfoMap.Insert(2887583419, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gfx8RbPlusEnable;
    info.valueSize = sizeof(m_settings.gfx8RbPlusEnable);
    m_settingsInfoMap.Insert(2122164302, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfx8FastClearAllTcCompatColorSurfs;
    info.valueSize = sizeof(m_settings.gfx8FastClearAllTcCompatColorSurfs);
    m_settingsInfoMap.Insert(3864495440, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gfx8CheckMetaDataFetchFromStartMip;
    info.valueSize = sizeof(m_settings.gfx8CheckMetaDataFetchFromStartMip);
    m_settingsInfoMap.Insert(999816292, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gfx8IgnoreMipInterleave;
    info.valueSize = sizeof(m_settings.gfx8IgnoreMipInterleave);
    m_settingsInfoMap.Insert(3804096310, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.waMiscGsNullPrim;
    info.valueSize = sizeof(m_settings.waMiscGsNullPrim);
    m_settingsInfoMap.Insert(203570314, info);

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
            component.pSettingsHashes = &g_gfx6PalSettingHashList[0];
            component.numSettings = g_gfx6PalNumSettings;
            component.pfnGetValue = ISettingsLoader::GetValue;
            component.pfnSetValue = ISettingsLoader::SetValue;
            component.pSettingsData = &g_gfx6PalJsonData[0];
            component.settingsDataSize = sizeof(g_gfx6PalJsonData);
            component.settingsDataHeader.isEncoded = true;
            component.settingsDataHeader.magicBufferId = 402778310;
            component.settingsDataHeader.magicBufferOffset = 0;

            pSettingsService->RegisterComponent(component);
        }
    }
}

} // Gfx6
} // Pal
