/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "pal.h"
#include "palInlineFuncs.h"
#include "palHashMapImpl.h"
#include "core/device.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9SettingsLoader.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/amdgpu_asic.h"
#include "devDriverServer.h"
#include "protocols/ddSettingsService.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// Minimum microcode feature version that has necessary MCBP fix.
constexpr uint32 MinUcodeFeatureVersionMcbpFix = 36;

// =====================================================================================================================
SettingsLoader::SettingsLoader(
    Util::IndirectAllocator* pAllocator,
    Pal::Device*             pDevice)
    :
    Pal::ISettingsLoader(pAllocator,
                         static_cast<DriverSettings*>(&m_settings),
                         g_gfx9PalNumSettings),
    m_pDevice(pDevice),
    m_settings(),
    m_gfxLevel(pDevice->ChipProperties().gfxLevel),
    m_pComponentName("Gfx9_Pal")
{
    memset(&m_settings, 0, sizeof(Gfx9PalSettings));
}

// =====================================================================================================================
SettingsLoader::~SettingsLoader()
{
    auto* pDevDriverServer = m_pDevice->GetPlatform()->GetDevDriverServer();
    if (pDevDriverServer != nullptr)
    {
        auto* pSettingsService = pDevDriverServer->GetSettingsService();
        if (pSettingsService != nullptr)
        {
            pSettingsService->UnregisterComponent(m_pComponentName);
        }
    }
}

// =====================================================================================================================
// Initializes the HWL environment settings.
Result SettingsLoader::Init()
{
    Result ret = m_settingsInfoMap.Init();

    if (ret == Result::Success)
    {
        // Init Settings Info HashMap
        InitSettingsInfo();

        // setup default values
        SetupDefaults();

        m_state = SettingsLoaderState::EarlyInit;

        // Read the rest of the settings from the registry
        ReadSettings();

        // Register with the DevDriver settings service
        DevDriverRegister();
    }

    return ret;
}

// =====================================================================================================================
// Validates that the settings structure has legal values. Variables that require complicated initialization can also
// be initialized here.
void SettingsLoader::ValidateSettings(
    PalSettings* pSettings)
{
    const auto& chipProps = m_pDevice->ChipProperties();
    const auto& gfx9Props = chipProps.gfx9;
    // Some hardware can support 128 offchip buffers per SE, but most support 64.
    const uint32 maxOffchipLdsBuffersPerSe = (gfx9Props.doubleOffchipLdsBuffers ? 128 : 64);
    // Compute the number of offchip LDS buffers for the whole chip.
    uint32 maxOffchipLdsBuffers = (gfx9Props.numShaderEngines * maxOffchipLdsBuffersPerSe);

    auto pPalSettings = m_pDevice->GetPublicSettings();

    if (IsVega10(*m_pDevice))
    {
        // Vega10 has a HW bug where during Tessellation, the SPI can load incorrect SDATA terms for offchip LDS.
        // We must limit the number of offchip buffers to 508 (127 offchip buffers per SE).
        maxOffchipLdsBuffers = Min(maxOffchipLdsBuffers, 508U);
    }
    else
    {
        // On gfx9, the offchip buffering register has enough space to support the full 512 buffers.
        maxOffchipLdsBuffers = Min(maxOffchipLdsBuffers, 512U);
    }

    // If the current microcode version doesn't support the "indexed" versions of the LOADDATA PM4 packets, we cannot
    // support MCBP because that feature requires using those packets.
    // We also need to make sure any microcode versions which are before the microcode fix disable preemption, even if
    // the user tried to enable it through the panel.
    if ((m_gfxLevel == GfxIpLevel::GfxIp9) &&
        (m_pDevice->EngineProperties().cpUcodeVersion < MinUcodeFeatureVersionMcbpFix))
    {
        // We don't have a fully correct path to enable in this case. The KMD needs us to respect their MCBP enablement
        // but we can't support state shadowing without these features.
        pSettings->cmdBufPreemptionMode = CmdBufPreemptModeFullDisableUnsafe;
    }
    else if (m_pDevice->GetPublicSettings()->disableCommandBufferPreemption)
    {
        pSettings->cmdBufPreemptionMode = CmdBufPreemptModeDisable;
    }

    // Validate the number of offchip LDS buffers used for tessellation.
    if (m_settings.numOffchipLdsBuffers > 0)
    {
        if (m_settings.useMaxOffchipLdsBuffers == true)
        {
            // Use the maximum amount of offchip-LDS buffers.
            m_settings.numOffchipLdsBuffers = maxOffchipLdsBuffers;
        }
        else
        {
            // Clamp to the maximum amount of offchip LDS buffers.
            m_settings.numOffchipLdsBuffers =
                    Min(maxOffchipLdsBuffers, m_settings.numOffchipLdsBuffers);
        }
    }

    // Clamp the number of supported user-data entries between the number of fast-user-data registers available and
    pPalSettings->maxUserDataEntries = Min(pPalSettings->maxUserDataEntries, MaxUserDataEntries);

    // If HTile is disabled, also disable the other settings whic
    // If HTile is disabled, also disable the other settings which depend on it:
    if (m_settings.htileEnable == false)
    {
        m_settings.hiDepthEnable           = false;
        m_settings.hiStencilEnable         = false;
        m_settings.dbPreloadEnable         = false;
        m_settings.dbPreloadWinEnable      = false;
        m_settings.dbPerTileExpClearEnable = false;
        m_settings.depthCompressEnable     = false;
        m_settings.stencilCompressEnable   = false;
    }

    // This can't be enabled by default in PAL because enabling the feature requires doing an expand on any clear
    // that changes the depth/stencil clear value. In that case, tiles marked as EXPCLEAR no longer match the new clear
    // value. In PAL, we don't always have visibility into what the last clear value was(if the clear was done in a
    // different command buffer or thread), so we'd have to do the expand conditionally on the GPU which may have
    // Perf Implications. Hence, enable it only if client is sure about depth stencil surfaces never changing the
    // clear values which means PAL doesn't have to worry about any clear-time expand operation to remove
    // the exp-clear tiles.
    if (pPalSettings->hintInvariantDepthStencilClearValues)
    {
        m_settings.dbPerTileExpClearEnable = true;
    }

    pSettings->shaderPrefetchClampSize = Pow2Align(pSettings->shaderPrefetchClampSize, 4096);

    // By default, gfx9RbPlusEnable is true, and it should be overridden to false
    // if the ASIC doesn't support Rb+.
    if (gfx9Props.rbPlus == 0)
    {
        m_settings.gfx9RbPlusEnable = false;
    }

    if ((pPalSettings->distributionTessMode == DistributionTessTrapezoidOnly) ||
        (pPalSettings->distributionTessMode == DistributionTessDefault))
    {
        pPalSettings->distributionTessMode = DistributionTessTrapezoid;
    }

    if (m_settings.wdLoadBalancingMode != Gfx9WdLoadBalancingDisabled)
    {
        // When WD load balancing flowchart optimization is enabled, the primgroup size cannot exceed 253.
        m_settings.primGroupSize = Min(253u, m_settings.primGroupSize);
    }

    m_settings.nggPrimsPerSubgroup = Min(m_settings.nggPrimsPerSubgroup, MaxGsThreadsPerSubgroup);
    m_settings.nggVertsPerSubgroup = Min(m_settings.nggVertsPerSubgroup, MaxGsThreadsPerSubgroup);

    if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
    {
        m_settings.nggEnableMode = NggPipelineTypeDisabled;
    }

    // Set default value for DCC BPP Threshold unless it was already overriden
    if (pPalSettings->dccBitsPerPixelThreshold == UINT_MAX)
    {
        // Performance testing on Vega20 has shown that it generally performs better when it's restricted
        // to use DCC at >=64BPP, we thus set it's default DCC threshold to 64BPP unless otherwise overriden.
        if (IsVega20(*m_pDevice))
        {
            pPalSettings->dccBitsPerPixelThreshold = 64;
        }
        else
        {
            pPalSettings->dccBitsPerPixelThreshold = 0;
        }

    }

    m_state = SettingsLoaderState::Final;
}

// =====================================================================================================================
// Override Gfx9 layer settings. This also includes setting up the workaround flags stored in the settings structure
// based on chip Family & ID.
//
// The workaround flags setup here can be overridden if the settings are set.
void SettingsLoader::OverrideDefaults(
    PalSettings* pSettings)
{
    // Enable workarounds which are common to all Gfx9/Gfx9+ hardware.
    if (IsGfx9(*m_pDevice))
    {
        m_settings.nggEnableMode = NggPipelineTypeDisabled;

        m_settings.waColorCacheControllerInvalidEviction = true;

        m_settings.waDisableHtilePrefetch = true;

        m_settings.waOverwriteCombinerTargetMaskOnly = true;

        m_settings.waDummyZpassDoneBeforeTs = true;

        // The driver assumes that all meta-data surfaces are pipe-aligned, but there are cases where the
        // HW does not actually pipe-align the data.  In these cases, the L2 cache needs to be flushed prior
        // to the metadata being read by a shader.
        m_settings.waDepthStencilTargetMetadataNeedsTccFlush = true;

        // Metadata is not pipe aligned once we get down to the mip chain within the tail
        m_settings.waitOnMetadataMipTail = true;
    }

    if (IsVega10(*m_pDevice) || IsRaven(*m_pDevice))
    {
        m_settings.waHtilePipeBankXorMustBeZero = true;

        m_settings.waWrite1xAASampleLocationsToZero = true;

        m_settings.waMiscPopsMissedOverlap = true;

        m_settings.waMiscScissorRegisterChange = true;

        m_settings.waDisableDfsmWithEqaa = true;

        m_settings.waDisable24BitHWFormatForTCCompatibleDepth = true;
    }

    if (IsVega20(*m_pDevice))
    {
        m_settings.waDisableDfsmWithEqaa = true;
    }

    if (IsVega10(*m_pDevice) || IsRaven(*m_pDevice)
        || IsRaven2(*m_pDevice)
        )
    {
        m_settings.waMetaAliasingFixEnabled = false;
    }

    const auto& gfx9Props = m_pDevice->ChipProperties().gfx9;

    if (m_settings.binningMaxAllocCountLegacy == 0)
    {
        // The recommended value for MAX_ALLOC_COUNT is min(128, PC size in the number of cache lines/(2*2*NUM_SE)).
        // The first 2 is to account for the register doubling the value and second 2 is to allow for at least 2
        // batches to ping-pong.
        m_settings.binningMaxAllocCountLegacy =
            Min(128u, gfx9Props.parameterCacheLines / (4u * gfx9Props.numShaderEngines));
    }

    if (m_settings.binningMaxAllocCountNggOnChip == 0)
    {
        // With NGG + on chip PC there is a single view of the PC rather than a division per SE. The recommended value
        // for MAX_ALLOC_COUNT is TotalParamCacheCacheLines / 2 / 3. There's a divide by 2 because the register units
        // are "2 cache lines" and a divide by 3 is because the HW team estimates it is best to only have one third of
        // the PC used by a single batch.
        m_settings.binningMaxAllocCountNggOnChip = gfx9Props.parameterCacheLines / 6;
    }

    m_state = SettingsLoaderState::LateInit;
}

// =====================================================================================================================
// The settings hash is used during pipeline loading to verify that the pipeline data is compatible between when it was
// stored and when it was loaded.
void SettingsLoader::GenerateSettingHash()
{
    MetroHash128::Hash(
        reinterpret_cast<const uint8*>(&m_settings),
        sizeof(Gfx9PalSettings),
        m_settingHash.bytes);
}

} // Gfx9
} // Pal
