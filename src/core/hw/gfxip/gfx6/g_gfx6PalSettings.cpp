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


#include "core/hw/gfxip/gfx6/gfx6SettingsLoader.h"
#include "core/hw/gfxip/gfx6/g_gfx6PalSettings.h"
#include "palInlineFuncs.h"
#include "core/device.h"

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
// Initializes the settings structure to default values.
void SettingsLoader::Gfx6SetupDefaults(
    Gfx6PalSettings* pSettings)
{
    // set setting variables to their default values...
    pSettings->cpDmaSrcAlignment = CpDmaAlignmentDefault;
    pSettings->disableBorderColorPaletteBinds = false;
    pSettings->forceOcclusionQueryResult = false;
    pSettings->forceOcclusionQueryResultValue = 1;
    pSettings->primGroupSize = 128;
    pSettings->dynamicPrimGroupEnable = true;
    pSettings->dynamicPrimGroupMin = 32;
    pSettings->dynamicPrimGroupMax = 128;
    pSettings->dynamicPrimGroupStep = 32;
    pSettings->dynamicPrimGroupWindowSize = 32;
    pSettings->switchVgtOnDraw = false;
    pSettings->esGsRatio = 4;
    pSettings->vsForcePartialWave = false;
    pSettings->vsHalfPackThreshold = 16;
    pSettings->esCuGroupEnabled = false;
    pSettings->gsCuGroupEnabled = false;
    pSettings->vsCuGroupEnabled = false;
    pSettings->psCuGroupEnabled = true;
    pSettings->tessFactorBufferSizePerSe = 0x1000;
    pSettings->disableTessDonutWalkPattern = 0;
    pSettings->maxTessFactor = 64.0f;
    pSettings->numTessPatchesPerTg = 0;
    pSettings->numOffchipLdsBuffers = 508;
    pSettings->useMaxOffchipLdsBuffers = true;
    pSettings->dsWavesPerSimdOverflow = 4;
    pSettings->offchipTfDegree = 4.0f;
    pSettings->gfx6OffChipHsSkipDataCopyNullPatch = false;
    pSettings->gfxMaxWavesPerCu = 0;
    pSettings->lsCuEnLimitMask = 0xffffffff;
    pSettings->esCuEnLimitMask = 0xffffffff;
    pSettings->gsCuEnLimitMask = 0xffffffff;
    pSettings->vsCuEnLimitMask = 0xffffffff;
    pSettings->psCuEnLimitMask = 0xffffffff;
    pSettings->csMaxWavesPerCu = 0;
    pSettings->csLockThreshold = 0;
    pSettings->csSimdDestCntl = CsSimdDestCntlDefault;
    pSettings->shaderPrefetchMethod = PrefetchCpDma;
    pSettings->anisoFilterOptEnabled = false;
    pSettings->samplerCeilingLogicEnabled = false;
    pSettings->samplerPrecisionFixEnabled = true;
    pSettings->samplerPerfMip = 0;
    pSettings->samplerAnisoThreshold = 0;
    pSettings->samplerAnisoBias = 0;
    pSettings->samplerSecAnisoBias = 0;
    pSettings->fastColorClearEnable = true;
    pSettings->fastColorClearOn3dEnable = true;
    pSettings->fmaskCompressEnable = true;
    pSettings->blendOptimizationsEnable = true;
    pSettings->htileEnable = true;
    pSettings->depthCompressEnable = true;
    pSettings->stencilCompressEnable = true;
    pSettings->linearHtileEnable = false;
    pSettings->dbPreloadEnable = false;
    pSettings->dbPreloadWinEnable = false;
    pSettings->dbPerTileExpClearEnable = false;
    pSettings->hiDepthEnable = true;
    pSettings->hiStencilEnable = true;
    pSettings->dbRequestSize = 0;
    pSettings->dbAddr5SwizzleMask = 1;
    pSettings->dbDisableColorOnValidation = false;
    pSettings->enableSeparateAspectMetadataInit = true;
    pSettings->gfx7VsPartialWaveWithEoiEnabled = false;
    pSettings->gfx7OffchipLdsBufferSize = Gfx7OffchipLdsBufferSize8192;
    pSettings->gfx7LateAllocVsOnCuAlwaysOn = false;
    pSettings->gfx7EnableOutOfOrderPrimitives = Gfx7OutOfOrderPrimSafe;
    pSettings->gfx7OutOfOrderWatermark = 7;
    pSettings->gfx7GsMaxLdsSize = 8 * 1024;
    pSettings->gfx7EnableOnchipGs = true;
    pSettings->gfx7GsOffChipThreshold = 64;
    pSettings->gfx7IdealGsPrimsPerSubGroup = 64;
    pSettings->gfx7AvoidVgtNullPrims = false;
    pSettings->gfx8PatchDistributionFactor = 8;
    pSettings->gfx8DonutDistributionFactor = 8;
    pSettings->gfx8TrapezoidDistributionFactor = 8;
    pSettings->gfx8UseDcc = 0x000000f3;
    pSettings->gfx8AlwaysDecompress = 0;
    pSettings->gfx8RbPlusEnable = true;
    pSettings->gfx8FastClearAllTcCompatColorSurfs = 0x3;
    pSettings->gfx8CheckMetaDataFetchFromStartMip = 0x3;
    pSettings->gfx8IgnoreMipInterleave = false;
    pSettings->waMiscGsNullPrim = false;

}

// =====================================================================================================================
// Reads the setting from the OS adapter and sets the structure value when the setting values are found.
void SettingsLoader::Gfx6ReadSettings(
    Gfx6PalSettings* pSettings)
{
    // read from the OS adapter for each individual setting
    m_pDevice->ReadSetting(pGfx6CpDmaSrcAlignmentStr,
                             Util::ValueType::Uint,
                             &pSettings->cpDmaSrcAlignment,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6DisableBorderColorPaletteBindsStr,
                             Util::ValueType::Boolean,
                             &pSettings->disableBorderColorPaletteBinds,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6ForceOcclusionQueryResultStr,
                             Util::ValueType::Boolean,
                             &pSettings->forceOcclusionQueryResult,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6ForceOcclusionQueryResultValueStr,
                             Util::ValueType::Uint,
                             &pSettings->forceOcclusionQueryResultValue,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6PrimgroupSizeStr,
                             Util::ValueType::Uint,
                             &pSettings->primGroupSize,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6DynamicPrimgroupEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->dynamicPrimGroupEnable,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6DynamicPrimgroupMinStr,
                             Util::ValueType::Uint,
                             &pSettings->dynamicPrimGroupMin,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6DynamicPrimgroupMaxStr,
                             Util::ValueType::Uint,
                             &pSettings->dynamicPrimGroupMax,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6DynamicPrimgroupStepStr,
                             Util::ValueType::Uint,
                             &pSettings->dynamicPrimGroupStep,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6DynamicPrimgroupWindowSizeStr,
                             Util::ValueType::Uint,
                             &pSettings->dynamicPrimGroupWindowSize,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6SwitchVgtOnDrawStr,
                             Util::ValueType::Boolean,
                             &pSettings->switchVgtOnDraw,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6EsGsRatioStr,
                             Util::ValueType::Uint,
                             &pSettings->esGsRatio,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6VsForcePartialWaveStr,
                             Util::ValueType::Boolean,
                             &pSettings->vsForcePartialWave,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6VsHalfPackThresholdStr,
                             Util::ValueType::Uint,
                             &pSettings->vsHalfPackThreshold,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6EsCuGroupEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->esCuGroupEnabled,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6GsCuGroupEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->gsCuGroupEnabled,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6VsCuGroupEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->vsCuGroupEnabled,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6PsCuGroupEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->psCuGroupEnabled,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6TessFactorBufferSizePerSeStr,
                             Util::ValueType::Uint,
                             &pSettings->tessFactorBufferSizePerSe,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6TessWalkPatternStr,
                             Util::ValueType::Uint,
                             &pSettings->disableTessDonutWalkPattern,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6MaxTessFactorStr,
                             Util::ValueType::Float,
                             &pSettings->maxTessFactor,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6NumTessPatchesPerThreadGroupStr,
                             Util::ValueType::Uint,
                             &pSettings->numTessPatchesPerTg,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6NumOffchipLdsBuffersStr,
                             Util::ValueType::Uint,
                             &pSettings->numOffchipLdsBuffers,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6UseMaxOffchipLdsBuffersStr,
                             Util::ValueType::Boolean,
                             &pSettings->useMaxOffchipLdsBuffers,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6TessDsWavesPerSimdOverflowStr,
                             Util::ValueType::Uint,
                             &pSettings->dsWavesPerSimdOverflow,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6TessOffchipTfDegreeStr,
                             Util::ValueType::Float,
                             &pSettings->offchipTfDegree,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6OffChipHsSkipDataCopyNullPatchStr,
                             Util::ValueType::Boolean,
                             &pSettings->gfx6OffChipHsSkipDataCopyNullPatch,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6GfxMaxWavesPerCuStr,
                             Util::ValueType::Uint,
                             &pSettings->gfxMaxWavesPerCu,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6LsCuEnLimitMaskStr,
                             Util::ValueType::Uint,
                             &pSettings->lsCuEnLimitMask,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6EsCuEnLimitMaskStr,
                             Util::ValueType::Uint,
                             &pSettings->esCuEnLimitMask,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6GsCuEnLimitMaskStr,
                             Util::ValueType::Uint,
                             &pSettings->gsCuEnLimitMask,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6VsCuEnLimitMaskStr,
                             Util::ValueType::Uint,
                             &pSettings->vsCuEnLimitMask,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6PsCuEnLimitMaskStr,
                             Util::ValueType::Uint,
                             &pSettings->psCuEnLimitMask,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6CsMaxWavesPerCuStr,
                             Util::ValueType::Uint,
                             &pSettings->csMaxWavesPerCu,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6CsLockThresholdStr,
                             Util::ValueType::Uint,
                             &pSettings->csLockThreshold,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6CsSimdDestCntlStr,
                             Util::ValueType::Uint,
                             &pSettings->csSimdDestCntl,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pShaderPrefetchMethodStr,
                             Util::ValueType::Uint,
                             &pSettings->shaderPrefetchMethod,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6AnisoFilterOptEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->anisoFilterOptEnabled,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6CeilingLogicEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->samplerCeilingLogicEnabled,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6PrecisionFixEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->samplerPrecisionFixEnabled,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6SamplerPerfMipStr,
                             Util::ValueType::Uint,
                             &pSettings->samplerPerfMip,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6SamplerAnisoThresholdStr,
                             Util::ValueType::Uint,
                             &pSettings->samplerAnisoThreshold,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6SamplerAnisoBiasStr,
                             Util::ValueType::Uint,
                             &pSettings->samplerAnisoBias,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6SamplerSecAnisoBiasStr,
                             Util::ValueType::Uint,
                             &pSettings->samplerSecAnisoBias,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6FastColorClearEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->fastColorClearEnable,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6FastColorClearOn3DEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->fastColorClearOn3dEnable,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6FmaskCompressEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->fmaskCompressEnable,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6BlendOptimizationEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->blendOptimizationsEnable,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6HtileEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->htileEnable,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6DepthCompressEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->depthCompressEnable,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6StencilCompressEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->stencilCompressEnable,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6LinearHtileEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->linearHtileEnable,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6DbPreloadEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->dbPreloadEnable,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6DbPreloadWinEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->dbPreloadWinEnable,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6DbPerTileExpClearEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->dbPerTileExpClearEnable,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6HiDepthEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->hiDepthEnable,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6HiStencilEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->hiStencilEnable,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6DbRequestSizeStr,
                             Util::ValueType::Uint,
                             &pSettings->dbRequestSize,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6DbAddr5SwizzleMaskStr,
                             Util::ValueType::Uint,
                             &pSettings->dbAddr5SwizzleMask,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6DbDisableColorOnValidationStr,
                             Util::ValueType::Boolean,
                             &pSettings->dbDisableColorOnValidation,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx6EnableSeparateAspectMetadataInitStr,
                             Util::ValueType::Boolean,
                             &pSettings->enableSeparateAspectMetadataInit,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx7VsPartialWaveWithEoiEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->gfx7VsPartialWaveWithEoiEnabled,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx7OffchipLdsBufferSizeStr,
                             Util::ValueType::Uint,
                             &pSettings->gfx7OffchipLdsBufferSize,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx7LateAllocVsOnCuAlwaysOnStr,
                             Util::ValueType::Boolean,
                             &pSettings->gfx7LateAllocVsOnCuAlwaysOn,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx7EnableOutOfOrderPrimitivesStr,
                             Util::ValueType::Uint,
                             &pSettings->gfx7EnableOutOfOrderPrimitives,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx7OutOfOrderWatermarkStr,
                             Util::ValueType::Uint,
                             &pSettings->gfx7OutOfOrderWatermark,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx7GsMaxLdsSizeStr,
                             Util::ValueType::Uint,
                             &pSettings->gfx7GsMaxLdsSize,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx7EnableOnchipGsStr,
                             Util::ValueType::Boolean,
                             &pSettings->gfx7EnableOnchipGs,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx7GsOffChipThresholdStr,
                             Util::ValueType::Uint,
                             &pSettings->gfx7GsOffChipThreshold,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx7GsPrimsPerSubGroupStr,
                             Util::ValueType::Uint,
                             &pSettings->gfx7IdealGsPrimsPerSubGroup,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx7AvoidVgtNullPrimsStr,
                             Util::ValueType::Boolean,
                             &pSettings->gfx7AvoidVgtNullPrims,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx8PatchDistributionFactorStr,
                             Util::ValueType::Uint,
                             &pSettings->gfx8PatchDistributionFactor,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx8DonutDistributionFactorStr,
                             Util::ValueType::Uint,
                             &pSettings->gfx8DonutDistributionFactor,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx8TrapezoidDistributionFactorStr,
                             Util::ValueType::Uint,
                             &pSettings->gfx8TrapezoidDistributionFactor,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx8UseDccStr,
                             Util::ValueType::Uint,
                             &pSettings->gfx8UseDcc,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx8AlwaysDecompressStr,
                             Util::ValueType::Uint,
                             &pSettings->gfx8AlwaysDecompress,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx8RbPlusEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->gfx8RbPlusEnable,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx8FastClearAllTcCompatColorSurfsStr,
                             Util::ValueType::Uint,
                             &pSettings->gfx8FastClearAllTcCompatColorSurfs,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx8CheckMetaDataFetchFromStartMipStr,
                             Util::ValueType::Uint,
                             &pSettings->gfx8CheckMetaDataFetchFromStartMip,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pGfx8IgnoreMipInterleaveStr,
                             Util::ValueType::Boolean,
                             &pSettings->gfx8IgnoreMipInterleave,
                             InternalSettingScope::PrivatePalGfx6Key);

    m_pDevice->ReadSetting(pWaMiscGsNullPrimStr,
                             Util::ValueType::Boolean,
                             &pSettings->waMiscGsNullPrim,
                             InternalSettingScope::PrivatePalGfx6Key);


}

} // Gfx6
} // Pal

using namespace Pal::Gfx6;

// =====================================================================================================================
// Get Gfx6 settings pointer via device object.
const Pal::Gfx6::Gfx6PalSettings& GetGfx6Settings(
    const Pal::Device& device)
{
    return static_cast<const Pal::Gfx6::Gfx6PalSettings&>(device.Settings());
}

