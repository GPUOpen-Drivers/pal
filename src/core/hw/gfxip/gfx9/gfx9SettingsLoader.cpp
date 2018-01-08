/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9SettingsLoader.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/amdgpu_asic.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// Minimum microcode feature version that has necessary MCBP fix.
constexpr uint32 MinUcodeFeatureVersionMcbpFix = 36;

// =====================================================================================================================
SettingsLoader::SettingsLoader(
    Pal::Device* pDevice)
    :
    Pal::SettingsLoader(pDevice, &m_gfx9Settings),
    m_gfxLevel(pDevice->ChipProperties().gfxLevel)
{
    memset(&m_gfx9Settings, 0, sizeof(Gfx9PalSettings));
}

// =====================================================================================================================
SettingsLoader::~SettingsLoader()
{
}

// =====================================================================================================================
// Initializes the HWL environment settings.
void SettingsLoader::HwlInit()
{
    // setup default values
    Gfx9SetupDefaults(&m_gfx9Settings);

    // Override Gfx9 layer settings
    OverrideGfx9Defaults();
}

// =====================================================================================================================
// Validates that the settings structure has legal values. Variables that require complicated initialization can also
// be initialized here.
void SettingsLoader::HwlValidateSettings()
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
    if ((gfx9Props.supportLoadRegIndexPkt == 0) ||
        ((m_gfxLevel == GfxIpLevel::GfxIp9)     &&
         (m_pDevice->EngineProperties().cpUcodeVersion < MinUcodeFeatureVersionMcbpFix)))
    {
        m_gfx9Settings.commandBufferPreemptionFlags &= ~UniversalEnginePreemption;
    }

    // Validate the number of offchip LDS buffers used for tessellation.
    if (m_gfx9Settings.numOffchipLdsBuffers > 0)
    {
        if (m_gfx9Settings.useMaxOffchipLdsBuffers == true)
        {
            // Use the maximum amount of offchip-LDS buffers.
            m_gfx9Settings.numOffchipLdsBuffers = maxOffchipLdsBuffers;
        }
        else
        {
            // Clamp to the maximum amount of offchip LDS buffers.
            m_gfx9Settings.numOffchipLdsBuffers =
                    Min(maxOffchipLdsBuffers, m_gfx9Settings.numOffchipLdsBuffers);
        }
    }

    // Clamp the number of supported user-data entries between the number of fast-user-data registers available and
    pPalSettings->maxUserDataEntries = Min(pPalSettings->maxUserDataEntries, MaxUserDataEntries);

    // If HTile is disabled, also disable the other settings whic
    // If HTile is disabled, also disable the other settings which depend on it:
    if (m_gfx9Settings.htileEnable == false)
    {
        m_gfx9Settings.hiDepthEnable           = false;
        m_gfx9Settings.hiStencilEnable         = false;
        m_gfx9Settings.dbPreloadEnable         = false;
        m_gfx9Settings.dbPreloadWinEnable      = false;
        m_gfx9Settings.dbPerTileExpClearEnable = false;
        m_gfx9Settings.depthCompressEnable     = false;
        m_gfx9Settings.stencilCompressEnable   = false;
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
        m_gfx9Settings.dbPerTileExpClearEnable = true;
    }

    m_pSettings->shaderPrefetchClampSize = Pow2Align(m_pSettings->shaderPrefetchClampSize, 4096);

    // By default, gfx9RbPlusEnable is true, and it should be overridden to false
    // if the ASIC doesn't support Rb+.
    if (gfx9Props.rbPlus == 0)
    {
        m_gfx9Settings.gfx9RbPlusEnable = false;
    }

    if ((pPalSettings->distributionTessMode == DistributionTessTrapezoidOnly) ||
        (pPalSettings->distributionTessMode == DistributionTessDefault))
    {
        pPalSettings->distributionTessMode = DistributionTessTrapezoid;
    }

    if (m_gfx9Settings.wdLoadBalancingMode != Gfx9WdLoadBalancingDisabled)
    {
        // When WD load balancing flowchart optimization is enabled, the primgroup size cannot exceed 253.
        m_gfx9Settings.primGroupSize = Min(253u, m_gfx9Settings.primGroupSize);

        // Disable dynamic prim-group software optimization if WD load balancing is enabled.
        m_gfx9Settings.dynamicPrimGroupEnable = false;
    }

    m_gfx9Settings.nggRegLaunchGsPrimsPerSubgrp     = Min(m_gfx9Settings.nggRegLaunchGsPrimsPerSubgrp,
                                                          OnChipGsMaxPrimPerSubgrp);
    m_gfx9Settings.idealNggFastLaunchWavesPerSubgrp = Min(m_gfx9Settings.idealNggFastLaunchWavesPerSubgrp, 4U);

    if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
    {
        m_gfx9Settings.nggMode = Gfx9NggDisabled;
    }
}

// =====================================================================================================================
// Reads the HWL related settings.
void SettingsLoader::HwlReadSettings()
{
    // read HWL settings from the registry or configure file
    Gfx9ReadSettings(&m_gfx9Settings);
}

// =====================================================================================================================
// Override Gfx9 layer settings. This also includes setting up the workaround flags stored in the settings structure
// based on chip Family & ID.
//
// The workaround flags setup here can be overridden if the settings are set.
void SettingsLoader::OverrideGfx9Defaults()
{
    // Enable workarounds which are common to all Gfx9/Gfx9+ hardware.
    if (IsGfx9(*m_pDevice))
    {
        m_gfx9Settings.waColorCacheControllerInvalidEviction = true;

        m_gfx9Settings.waDisableHtilePrefetch = true;

        m_gfx9Settings.waOverwriteCombinerTargetMaskOnly = true;

        // There is a bug where the WD will page fault when it writes VGT_EVENTs into the NGG offchip control sideband
        // (CSB) because there is no page table mapped for the VMID that was left in an NGG pipeline state.
        // Since page tables are allocated by the kernel driver per-process, when the process is terminated the page
        // table mapping will be invalidated-and-erased. This will leave no page tables mapped for the current VMID, and
        // the WD request for a virtual memory address translation of the CSB buffer will consequently fail.
        // NOTE: This is not an issue for mid-command buffer preemption nor when another process immediately follows
        //       this one with rendering work, as the kernel performs an invalidate-and-swap with the page tables,
        //       instead of invalidate-and-erase. Since the NGG buffers are mapped into every page table, these cases
        //       will not cause the same page fault.
        m_gfx9Settings.waNggWdPageFault = true;

        m_gfx9Settings.waLegacyToNggVsPartialFlush = true;

        m_gfx9Settings.waDummyZpassDoneBeforeTs = true;
    }

    if (IsVega10(*m_pDevice)
#if PAL_BUILD_RAVEN1
        || IsRaven(*m_pDevice)
#endif
        )
    {
        m_gfx9Settings.waHtilePipeBankXorMustBeZero = true;

        m_gfx9Settings.waWrite1xAASampleLocationsToZero = true;

        m_gfx9Settings.waMiscPopsMissedOverlap = true;

        m_gfx9Settings.waMiscScissorRegisterChange = true;

        m_gfx9Settings.waDisableDfsmWithEqaa = true;

        m_gfx9Settings.waDisable24BitHWFormatForTCCompatibleDepth = true;

        m_gfx9Settings.waMetaAliasingFixEnabled = false;
    }

    const auto& gfx9Props = m_pDevice->ChipProperties().gfx9;

    if (m_gfx9Settings.binningMaxAllocCountLegacy == 0)
    {
        // The recommended value for MAX_ALLOC_COUNT is min(128, PC size in the number of cache lines/(2*2*NUM_SE)).
        // The first 2 is to account for the register doubling the value and second 2 is to allow for at least 2
        // batches to ping-pong.
        m_gfx9Settings.binningMaxAllocCountLegacy =
            Min(128u, gfx9Props.parameterCacheLines / (4u * gfx9Props.numShaderEngines));
    }

    if (m_gfx9Settings.binningMaxAllocCountNggOnChip == 0)
    {
        // With NGG + on chip PC there is a single view of the PC rather than a division per SE. The recommended value
        // for MAX_ALLOC_COUNT is TotalParamCacheCacheLines / 2 / 3. There's a divide by 2 because the register units
        // are "2 cache lines" and a divide by 3 is because the HW team estimates it is best to only have one third of
        // the PC used by a single batch.
        m_gfx9Settings.binningMaxAllocCountNggOnChip = gfx9Props.parameterCacheLines / 6;
    }

    m_gfx9Settings.gfx9OffChipHsCopyMethod = Gfx9OffChipHsImmediate;
}

// =====================================================================================================================
// Overrides defaults for the independent layer settings, and applies application optimizations which consist of simple
// settings changes.
void SettingsLoader::HwlOverrideDefaults()
{
}

// =====================================================================================================================
// The settings hash is used during pipeline loading to verify that the pipeline data is compatible between when it was
// stored and when it was loaded.  The CCC controls some of the settings though, and the CCC doesn't set it identically
// across all GPUs in an MGPU configuration.  Since the CCC keys don't affect pipeline generation, just ignore those
// values when it comes to hash generation.
void SettingsLoader::GenerateSettingHash()
{
    Gfx9PalSettings tempSettings = m_gfx9Settings;

    // Ignore these CCC settings when computing a settings hash as described in the function header.
    tempSettings.textureOptLevel = 0;
    tempSettings.catalystAI      = 0;

    MetroHash128::Hash(
        reinterpret_cast<const uint8*>(&tempSettings),
        sizeof(Gfx9PalSettings),
        m_settingHash.bytes);
}

} // Gfx9
} // Pal
