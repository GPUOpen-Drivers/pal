/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6SettingsLoader.h"
#include "core/hw/amdgpu_asic.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// Minimum microcode feature version that has necessary MCBP fix.
constexpr uint32 MinUcodeFeatureVersionMcbpFix = 48;

// =====================================================================================================================
SettingsLoader::SettingsLoader(
    Pal::Device* pDevice)
    :
    Pal::SettingsLoader(pDevice, &m_gfx6Settings)
{
    memset(&m_gfx6Settings, 0, sizeof(Gfx6PalSettings));
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
    Gfx6SetupDefaults(&m_gfx6Settings);

    // Override Gfx6 layer settings
    OverrideGfx6Defaults();
}

// =====================================================================================================================
// Validates that the settings structure has legal values. Variables that require complicated initialization can also
// be initialized here.
void SettingsLoader::HwlValidateSettings()
{
    const auto& gfx6Props = m_pDevice->ChipProperties().gfx6;
    // Some hardware can support 128 offchip buffers per SE, but most support 64.
    const uint32 maxOffchipLdsBuffersPerSe =
            (gfx6Props.doubleOffchipLdsBuffers ? 128 : 64);
    // Compute the number of offchip LDS buffers for the whole chip.
    uint32 maxOffchipLdsBuffers =
            (gfx6Props.numShaderEngines * maxOffchipLdsBuffersPerSe);

    auto pGfx6Device = static_cast<Pal::Gfx6::Device*>(m_pDevice->GetGfxDevice());
    auto pPalSettings = m_pDevice->GetPublicSettings();

    if (IsGfx6(*m_pDevice))
    {
        // On Gfx6, the offchip bufferring register only has enough space to support a maximum of 127 buffers. Since
        // this must be evenly distributed across all SE's, we need to clamp to 126 (for two-SE configurations).
        maxOffchipLdsBuffers = Min(maxOffchipLdsBuffers, 126U);

        // Gfx6 hardware only supports an offchip LDS buffer size of 8K DWORDs.
        m_gfx6Settings.gfx7OffchipLdsBufferSize = Gfx7OffchipLdsBufferSize8192;

        // Gfx6 hardware does not support on-chip GS mode.
        m_gfx6Settings.gfx7EnableOnchipGs = false;
    }
    else if (IsGfx7(*m_pDevice))
    {
        // On Gfx7, the offchip bufferring register only has enough space to support a maximum of 511 buffers. Since
        // this must be evenly distributed across all SE's, we need to clamp to 508 (for four-SE configurations).
        maxOffchipLdsBuffers = Min(maxOffchipLdsBuffers, 508U);
    }
    else if(IsGfx8(*m_pDevice))
    {
        // On Gfx8, the offchip bufferring register has enough space to support the full 512 buffers.
        maxOffchipLdsBuffers = Min(maxOffchipLdsBuffers, 512U);
    }
    else
    {
        PAL_NOT_IMPLEMENTED();
    }

    // If the current microcode version doesn't support the "indexed" versions of the LOADDATA PM4 packets, we cannot
    // support MCBP because that feature requires using those packets. Furthermore, we also need the microcode version
    // which includes the fix for preemption within a chained indirect buffer. Otherwise, the CP may hang or page fault
    // upon resuming a preempted command buffer.
    // We also need to make sure any microcode versions which are before the microcode fix disable preemption, even if
    // the user tried to enable it through the panel.
    if ((gfx6Props.supportLoadRegIndexPkt == 0)        ||
        (gfx6Props.supportPreemptionWithChaining == 0) ||
        (m_pDevice->EngineProperties().cpUcodeVersion < MinUcodeFeatureVersionMcbpFix))
    {
        m_gfx6Settings.commandBufferPreemptionFlags &= ~UniversalEnginePreemption;
    }

    if (m_pDevice->GetPublicSettings()->disableCommandBufferPreemption)
    {
        m_gfx6Settings.commandBufferPreemptionFlags = PreemptionDisabled;
    }

    // The maximum GS LDS size must be aligned to the LDS granularity.
    m_gfx6Settings.gfx7GsMaxLdsSize = Pow2Align(m_gfx6Settings.gfx7GsMaxLdsSize, Gfx7LdsDwGranularity);

    // Validate the number of offchip LDS buffers used for tessellation.
    if (m_gfx6Settings.numOffchipLdsBuffers > 0)
    {
        if (m_gfx6Settings.useMaxOffchipLdsBuffers == true)
        {
            // Use the maximum amount of offchip-LDS buffers.
            m_gfx6Settings.numOffchipLdsBuffers = maxOffchipLdsBuffers;
        }
        else
        {
            // Clamp to the maximum amount of offchip LDS buffers.
            m_gfx6Settings.numOffchipLdsBuffers =
                    Min(maxOffchipLdsBuffers, m_gfx6Settings.numOffchipLdsBuffers);
        }
    }

    // Due to a hardware bug, some chips are limited to using smaller offchip LDS buffers or a smaller number of large
    // buffers. For now, prefer a large number of small offchip buffers.
    if ((pGfx6Device->WaMiscOffchipLdsBufferLimit() == true) &&
        (m_gfx6Settings.numOffchipLdsBuffers         > 256)  &&
        (m_gfx6Settings.gfx7OffchipLdsBufferSize    == Gfx7OffchipLdsBufferSize8192))
    {
        m_gfx6Settings.gfx7OffchipLdsBufferSize = Gfx7OffchipLdsBufferSize4096;
    }

    // Distributed tessellation mode is only supported on Gfx8+ hardware with two or more shader engines, and when
    // off-chip tessellation is enabled.
    if ((gfx6Props.numShaderEngines == 1) ||
        (m_gfx6Settings.numOffchipLdsBuffers == 0) ||
        (IsGfx8(*m_pDevice) == false))
    {
        pPalSettings->distributionTessMode        = DistributionTessOff;
        m_gfx6Settings.gfx8PatchDistributionFactor = 0;
        m_gfx6Settings.gfx8DonutDistributionFactor = 0;
    }

    // Validate tessellation distribution settings.
    if (pPalSettings->distributionTessMode != DistributionTessOff)
    {
        // VGT tessellation distribution does not exist for gfxip 6,7. We also turn off distributed tessellation as
        // default for gfxip 6 to 8.
        if (pPalSettings->distributionTessMode == DistributionTessDefault)
        {
            pPalSettings->distributionTessMode = DistributionTessOff;
        }
        else if ((pPalSettings->distributionTessMode == DistributionTessTrapezoid) &&
                 (gfx6Props.supportTrapezoidTessDistribution == 0))
        {
            // Fallback to Donut granularity for gfxip that lack Trapezoid support.
            pPalSettings->distributionTessMode = DistributionTessDonut;
        }
        else if (pPalSettings->distributionTessMode == DistributionTessTrapezoidOnly)
        {
            if (gfx6Props.supportTrapezoidTessDistribution == 0)
            {
                // Disable tessellation distribution if Trapezoid mode is not supported.
                pPalSettings->distributionTessMode = DistributionTessOff;
            }
            else
            {
                pPalSettings->distributionTessMode = DistributionTessTrapezoid;
            }
        }
    }

    // If distributed tessellation is enabled, then tessellation must always go off-chip.
    if (pPalSettings->distributionTessMode != DistributionTessOff)
    {
        PAL_ALERT(m_gfx6Settings.numOffchipLdsBuffers == 0);
        m_gfx6Settings.dsWavesPerSimdOverflow = 0;
    }

    if (m_gfx6Settings.fastColorClearEnable == false)
    {
        // Cannot enable fast color clears on 3D Images if they are disabled globally.
        m_gfx6Settings.fastColorClearOn3dEnable = false;
    }

    // If HTile is disabled, also disable the other settings which depend on it:
    if (m_gfx6Settings.htileEnable == false)
    {
        m_gfx6Settings.hiDepthEnable           = false;
        m_gfx6Settings.hiStencilEnable         = false;
        m_gfx6Settings.dbPreloadEnable         = false;
        m_gfx6Settings.dbPreloadWinEnable      = false;
        m_gfx6Settings.dbPerTileExpClearEnable = false;
        m_gfx6Settings.depthCompressEnable     = false;
        m_gfx6Settings.stencilCompressEnable   = false;
    }

    // Out of Order primitives are only supported on Hawaii and Gfx8 ASICs with more than one VGT.
    // Hawaii has a hardware bug where the hardware can hang when a multi-cycle primitive is processed
    // when out of order is enabled. So we disable out of order prims for that ASIC.
    if ((IsGfx8(*m_pDevice) == false) || (gfx6Props.numShaderEngines < 2))
    {
        m_gfx6Settings.gfx7EnableOutOfOrderPrimitives = Gfx7OutOfOrderPrimDisable;
    }

    // By default, gfx8RbPlusEnable is true, and it should be overriden to false
    // if the ASIC doesn't support Rb+;
    if (gfx6Props.rbPlus == 0)
    {
        m_gfx6Settings.gfx8RbPlusEnable = false;
    }

    if (m_gfx6Settings.nonlocalDestGraphicsCopyRbs >=
        static_cast<int32>(gfx6Props.numActiveRbs))
    {
        m_gfx6Settings.nonlocalDestGraphicsCopyRbs = gfx6Props.numActiveRbs;
    }

    // Apply the "VGT Null Primitive" workaround:
    // This workaround is identical to the Gfx7AvoidVgtNullPrims feature so force it on.
    if (pGfx6Device->WaMiscVgtNullPrim())
    {
        m_gfx6Settings.gfx7AvoidVgtNullPrims = true;
    }

    // It doesn't make sense to enable this feature for ASICs that don't support 4x prim rate and it may actually cause
    // crashes and/or hangs.
    if (m_pDevice->ChipProperties().primsPerClock < 4)
    {
        m_gfx6Settings.gfx7AvoidVgtNullPrims = false;
    }

    // Apply the CP DMA performance workaround: force 32-byte alignment.
    if (pGfx6Device->WaAlignCpDma())
    {
        m_gfx6Settings.cpDmaSrcAlignment = CpDmaAlignmentOptimal;
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
        m_gfx6Settings.dbPerTileExpClearEnable = true;
    }

    // Clamp the number of supported user-data entries between the number of fast-user-data registers available and
    // the maximum number of virtualized user-data entries.
    pPalSettings->maxUserDataEntries = Max(MaxFastUserDataEntries,
                                          Min(pPalSettings->maxUserDataEntries, MaxUserDataEntries));
}

// =====================================================================================================================
// Reads the HWL related settings.
void SettingsLoader::HwlReadSettings()
{
    // read HWL settings from the registry or configure file
    Gfx6ReadSettings(&m_gfx6Settings);
}

// =====================================================================================================================
// Override Gfx6 layer settings. This also includes setting up the workaround flags stored in the settings structure
// based on chip Family & ID.
//
// The workaround flags setup here can be overridden if the settings are set.
void SettingsLoader::OverrideGfx6Defaults()
{
    const uint32 eRevId = m_pDevice->ChipProperties().eRevId;
    if (IsGfx6(*m_pDevice))
    {
        // Tahiti & Pitcairn workarounds:
        if (IsTahiti(*m_pDevice) || IsPitcairn(*m_pDevice))
        {
            m_gfx6Settings.waMiscGsNullPrim = true;
        }
        // Cape Verde workarounds:
        else if (IsCapeVerde(*m_pDevice))
        {
            // Verde has all of the different powergating types enabled, which is untrue of the rest of the Gfx6
            // family. When powergating is enabled, certain chips are powered down and register states are lost. Some
            // of the registers are write-only, and are not reset when the chip is powered up again. For example,
            // VGT_TF_MEMORY_BASE and VGT_TF_RING_SIZE are two registers that are not reset - these registers are set
            // in the preamble command stream.
            m_gfx6Settings.forcePreambleCmdStream = true;
        }
        // Oland & Hainan workarounds:
        else if (IsOland(*m_pDevice) || IsHainan(*m_pDevice))
        {
            // No additional workarounds beyond the ones common to all Gfx6+.
        }
    }
    else if (IsGfx7(*m_pDevice))
    {
        // Hawaii workarounds:
        if (IsHawaii(*m_pDevice))
        {
            // On Hawaii, thick/thick tiling formats don't suppot fast clears. There are several ways to deal with
            // this: long term, we'd like to add new entries to the tiling table so renderable 3D Images get a
            // thick/thin tiling format, but this requires coordination between multiple teams. For now, we'll just
            // disable fast color clears on 3D Images for Hawaii.
            m_gfx6Settings.fastColorClearOn3dEnable = false;
        }
        // Bonaire workarounds:
        else if (IsBonaire(*m_pDevice))
        {
            m_gfx6Settings.waMiscGsNullPrim = true;
        }
    }
    else if (IsGfx8(*m_pDevice))
    {
        if (IsCarrizo(*m_pDevice))
        {
            m_gfx6Settings.gfx7LateAllocVsOnCuAlwaysOn = true;
        }
    }
    else
    {
        // Unknown chip family!
        PAL_NOT_IMPLEMENTED();
    }

    // When configuring the IA_MULTI_VGT_PARAM register, all Sea Islands hardware with more than two shader engines
    // should set PARTIAL_VS_WAVE_ON whenever SWITCH_ON_EOI is set.
    if ((IsGfx7(*m_pDevice)) &&
        (m_pDevice->ChipProperties().gfx6.numShaderEngines > 2))
    {
        m_gfx6Settings.gfx7VsPartialWaveWithEoiEnabled = true;
    }

    if (IsGfx8(*m_pDevice))
    {
        // Disable VS half-pack mode by default on Gfx8 hardware. The reg-spec recommends more optimal VGT settings
        // which can only be used when half-pack mode is disabled. All Gfx8 parts have enough param cache space for the
        // maximum of 32 VS exports, so VS half-pack mode is never necessary.
        // ( Param cache space: Carrizo:512, Iceland:1024, Tonga:2048 )
        m_gfx6Settings.vsHalfPackThreshold = (MaxVsExportSemantics + 1);
    }

    // Prior-to-Gfx8, the DCC (delta color compression) and texture-fetch-of-meta-data features did not exist. These
    // keys should not do be used without verifying that the installed device is Gfx8 (or newer), but just in case...
    if (IsGfx6(*m_pDevice) || IsGfx7(*m_pDevice))
    {
        m_gfx6Settings.gfx8UseDcc                            = 0;
        m_pDevice->GetPublicSettings()->tcCompatibleMetaData = 0;
    }

    // It's generally faster to use on-chip tess for these ASICs due to their low memory bandwidth.
    if (IsIceland(*m_pDevice) || IsHainan(*m_pDevice))
    {
        m_gfx6Settings.numOffchipLdsBuffers = 0;
    }

    // Null primitives can lead to significant performance losses on 4x prim rate ASICs.
    if (m_pDevice->ChipProperties().primsPerClock >= 4)
    {
        m_gfx6Settings.gfx7AvoidVgtNullPrims = true;
    }
}

// =====================================================================================================================
// The settings hash is used during pipeline loading to verify that the pipeline data is compatible between when it was
// stored and when it was loaded.  The CCC controls some of the settings though, and the CCC doesn't set it identically
// across all GPUs in an MGPU configuration.  Since the CCC keys don't affect pipeline generation, just ignore those
// values when it comes to hash generation.
void SettingsLoader::GenerateSettingHash()
{
    Gfx6PalSettings tempSettings = m_gfx6Settings;

    // Ignore these CCC settings when computing a settings hash as described in the function header.
    tempSettings.textureOptLevel = 0;
    tempSettings.catalystAI      = 0;

    MetroHash128::Hash(
        reinterpret_cast<const uint8*>(&tempSettings),
        sizeof(Gfx6PalSettings),
        m_settingHash.bytes);
}
} // Gfx6
} // Pal
