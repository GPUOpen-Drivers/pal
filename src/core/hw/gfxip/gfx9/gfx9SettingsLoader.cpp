/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
    Pal::Device* pDevice)
    :
    Pal::ISettingsLoader(pDevice->GetPlatform(),
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

    auto* pPalSettings = m_pDevice->GetPublicSettings();

    if (IsGfx9(*m_pDevice))
    {
        // YUV planar surfaces require the ability to modify the base address to point to individual slices.  Due
        // to DCC addressing that interleaves slices on GFX9 platforms, we can't accurately point to the start
        // of a slice in DCC, which makes supporting DCC for YUV planar surfaces impossible.
        m_settings.useDcc &= ~Gfx10UseDccYuvPlanar;
    }

    if (m_settings.binningMaxAllocCountLegacy == 0)
    {
        if (IsGfx9(*m_pDevice))
        {
            // The recommended value for MAX_ALLOC_COUNT is min(128, PC size in the number of cache lines/(2*2*NUM_SE)).
            // The first 2 is to account for the register doubling the value and second 2 is to allow for at least 2
            // batches to ping-pong.
            m_settings.binningMaxAllocCountLegacy =
                Min(128u, gfx9Props.parameterCacheLines / (4u * gfx9Props.numShaderEngines));
        }
        else
        {
            // In Gfx10 there is a single view of the PC rather than a division per SE.
            // The recommended value for this is to allow a single batch to consume at
            // most 1/3 of the parameter cache lines.
            m_settings.binningMaxAllocCountLegacy = gfx9Props.parameterCacheLines / 3;
        }
    }

    if (m_settings.binningMaxAllocCountNggOnChip == 0)
    {
        // With NGG + on chip PC there is a single view of the PC rather than a
        // division per SE. The recommended value for this is to allow a single batch to
        // consume at most 1/3 of the parameter cache lines.
        m_settings.binningMaxAllocCountNggOnChip = gfx9Props.parameterCacheLines / 3;

        if (IsGfx9(*m_pDevice))
        {
            // On GFX9, the PA_SC_BINNER_CNTL_1::MAX_ALLOC_COUNT value is in units of
            // 2 parameter cache lines. So divide by 2.
            m_settings.binningMaxAllocCountNggOnChip /= 2;
        }

    }

    // Some hardware can support 128 offchip buffers per SE, but most support 64.
    const uint32 maxOffchipLdsBuffersPerSe = (gfx9Props.doubleOffchipLdsBuffers ? 128 : 64);

    // Compute the number of offchip LDS buffers for the whole chip.
    uint32 maxOffchipLdsBuffers = (gfx9Props.numShaderEngines * maxOffchipLdsBuffersPerSe);

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

    if (gfx9Props.supportOutOfOrderPrimitives == 0)
    {
        m_settings.enableOutOfOrderPrimitives = OutOfOrderPrimDisable;
    }

    if (IsGfx10(*m_pDevice))
    {
        // GFX10 doesn't need this workaround as it can natively support 1D depth images.
        m_settings.treat1dAs2d = false;

        // GFX10 doesn't use the convoluted meta-addressing scheme that GFX9 does, so disable
        // the "optimized" algorithm for processing the meta-equations.
        m_settings.optimizedFastClear = 0;

        // The suggested size of the tessellation factor buffer per SE is 0x4000 DWORDs, to account for the
        // multiples SA's per SE.
        // More updates:  It is true that for Navi10 this value should be 0xC000. This translates to 128
        //                threadgroups per SPI for 3 control point patches and 64 patches per threadgroup.
        //                GE has internal FIFO limits and that prevents it from launching more work.
        //                So there is no point in increasing the size of the buffer
        m_settings.tessFactorBufferSizePerSe = Gfx10::mmVGT_TF_RING_SIZE_DEFAULT / gfx9Props.numShaderEngines;

        if ((m_settings.tessFactorBufferSizePerSe * gfx9Props.numShaderEngines) > Gfx09_10::VGT_TF_RING_SIZE__SIZE_MASK)
        {
            m_settings.tessFactorBufferSizePerSe =
                Pow2AlignDown(Gfx09_10::VGT_TF_RING_SIZE__SIZE_MASK,
                              gfx9Props.numShaderEngines) / gfx9Props.numShaderEngines;
            static_assert(VGT_TF_RING_SIZE__SIZE__SHIFT == 0, "VGT_TF_RING_SIZE::SIZE shift is no longer zero!");
        }

        if (m_settings.waClampQuadDistributionFactor)
        {
            // VGT_TESS_DISTRIBUTION.ACCUM_QUAD should never be allowed to exceed 64
            m_settings.quadDistributionFactor = Min(m_settings.quadDistributionFactor, 64u);
        }

        if ((m_settings.waLateAllocGs0) && m_settings.nggSupported)
        {
            m_settings.nggLateAllocGs = 0;

            // This workaround requires that tessellation distribution is enabled and the distribution factors are
            // non-zero.
            if (pPalSettings->distributionTessMode == DistributionTessOff)
            {
                pPalSettings->distributionTessMode = DistributionTessDefault;
            }
            m_settings.donutDistributionFactor     = Max(m_settings.donutDistributionFactor,     1u);
            m_settings.isolineDistributionFactor   = Max(m_settings.isolineDistributionFactor,   1u);
            m_settings.quadDistributionFactor      = Max(m_settings.quadDistributionFactor,      1u);
            m_settings.trapezoidDistributionFactor = Max(m_settings.trapezoidDistributionFactor, 1u);
            m_settings.triDistributionFactor       = Max(m_settings.triDistributionFactor,       1u);
        }

        if (m_settings.gfx9RbPlusEnable)
        {
            m_settings.useCompToSingle |= (Gfx10UseCompToSingle8bpp | Gfx10UseCompToSingle16bpp);
        }

        // On Navi2x WGP harvesting asymmetric configuration, for pixel shader waves the extra WGP is not useful as all
        // of Navi2x splits workloads (waves) evenly among the SE. For pixel shader workloads, the pixels are split
        // evenly among the 2 SA within an SE as well. So for basic large uniform PS workload, the pixels are split
        // evenly among all 8 SA of a Navi2x and the work-load will only finish as fast as the SA with the fewest # of
        // WGP. In essence this means that a 72 CU Navi21 behaves like a 64 CU Navi21 for pixel shader workloads.
        // We should mask off the extra WGP from PS waves on WGP harvesting asymmetric configuration.
        // This will reduce power consumption when not needed and allow to the GPU to clock higher.
        if (IsGfx103(*m_pDevice) && m_settings.gfx103DisableAsymmetricWgpForPs)
        {
            m_settings.psCuEnLimitMask = (1 << (gfx9Props.gfx10.minNumWgpPerSa * 2)) - 1;
        }
    }

    if ((pPalSettings->distributionTessMode == DistributionTessTrapezoidOnly) ||
        (pPalSettings->distributionTessMode == DistributionTessDefault))
    {
        pPalSettings->distributionTessMode = DistributionTessTrapezoid;
    }

    // When WD load balancing flowchart optimization is enabled, the primgroup size cannot exceed 253.
    m_settings.primGroupSize = Min(253u, m_settings.primGroupSize);

    if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
    {
        m_settings.nggSupported = false;
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

    // Since XGMI is much faster than PCIe, PAL should not reduce the number of RBs to increase the PCIe throughput
    if (chipProps.p2pSupport.xgmiEnabled != 0)
    {
        pSettings->nonlocalDestGraphicsCopyRbs = UINT_MAX;
    }

    m_state = SettingsLoaderState::Final;
}

// =====================================================================================================================
// Setup any workarounds that are necessary for all Gfx10 products.
static void SetupGfx10Workarounds(
    const Pal::Device&  device,
    Gfx9PalSettings*    pSettings)
{
    pSettings->waColorCacheControllerInvalidEviction = true;

    // GCR ranged sync operations cause page faults for Cmask without the uCode fix that properly converts the
    // ACQUIRE_MEM packet's COHER_SIZE to the correct GCR_DATA_INDEX.
    pSettings->waCmaskImageSyncs = (device.EngineProperties().cpUcodeVersion < 28);
}

// =====================================================================================================================
// Setup workarounds that are necessary for all Gfx10.1 products.
static void SetupGfx101Workarounds(
    const Pal::Device&  device,
    Gfx9PalSettings*    pSettings,
    PalSettings*        pCoreSettings)
{
    pSettings->waVgtFlushNggToLegacyGs = true;

    pSettings->waVgtFlushNggToLegacy = true;

    pSettings->waDisableFmaskNofetchOpOnFmaskCompressionDisable = true;

    // The GE has a bug where attempting to use an index buffer of size zero can cause a hang.
    // The workaround is to bind an internal index buffer of a single entry and force the index buffer
    // size to one. This applies to all Navi1x products, which are all Gfx10.1 products.
    pSettings->waIndexBufferZeroSize = true;

    // The CB has a bug where blending can be corrupted if the color target is 8bpp and uses an S swizzle mode.
    pCoreSettings->addr2DisableSModes8BppColor = true;

    pSettings->waCeDisableIb2 = true;

    pSettings->waUtcL0InconsistentBigPage = true;

    pSettings->waLimitLateAllocGsNggFifo = true;

    pSettings->waClampGeCntlVertGrpSize = true;

    pSettings->waLegacyGsCutModeFlush = true;

    {
        // The DB has a bug where an attempted depth expand of a Z16_UNORM 1xAA surface that has not had its
        // metadata initialized will cause the DBs to incorrectly calculate the amount of return data from the
        // RMI block, which results in a hang.
        // The workaround is to force a compute resummarize for these surfaces, as we can't guarantee that an
        // expand won't be executed on an uninitialized depth surface.
        // This applies to all Navi1x products, which are all Gfx10.1 products.
        pSettings->waZ16Unorm1xAaDecompressUninitialized = true;

        // Workaround gfx10 Ngg performance issues related to UTCL2 misses with Index Buffers.
        pSettings->waEnableIndexBufferPrefetchForNgg = true;

        // Applies to all Navi1x products.
        pSettings->waClampQuadDistributionFactor = true;

        pSettings->waLogicOpDisablesOverwriteCombiner = true;

        // Applies to all Navi1x products.
        // If Primitive Order Pixel Shader (POPS/ROVs) are enabled and DB_DFSM_CONTROL.POPS_DRAIN_PS_ON_OVERLAP == 1,
        // we must set DB_RENDER_OVERRIDE2.PARTIAL_SQUAD_LAUNCH_CONTROL = PSLC_ON_HANG_ONLY to avoid a hang.
        pSettings->waStalledPopsMode = true;

        // The DB has a bug that when setting the iterate_256 register to 1 causes a hang.
        // More specifically the Flush Sequencer state-machine gets stuck waiting for Z data
        // when Iter256 is set to 1. The software workaround is to set DECOMPRESS_ON_N_ZPLANES
        // register to 2 for 4x MSAA Depth/Stencil surfaces to prevent hangs.
        pSettings->waTwoPlanesIterate256 = true;
    }
}

// =====================================================================================================================
// Setup workarounds that are necessary for all Navi2x products.
static void SetupNavi2xWorkarounds(
    const Pal::Device&  device,
    Gfx9PalSettings*    pSettings)
{
    // This bug is caused by shader UAV writes to stencil surfaces that have associated hTile data that in turn
    // contains VRS data.  The UAV to stencil will corrupt the VRS data.  No API that supports VRS allows for
    // application writes to stencil UAVs; however, PAL does it internally through image-to-image copies.  Force
    // use of graphics copies for affected surfaces.
    pSettings->waVrsStencilUav = WaVrsStencilUav::GraphicsCopies;

    pSettings->waLegacyGsCutModeFlush = true;
}

// =====================================================================================================================
// Setup workarounds that only apply to Navi10.
static void SetupNavi10Workarounds(
    const Pal::Device&  device,
    Gfx9PalSettings*    pSettings,
    PalSettings*        pCoreSettings)
{
    // Setup any Gfx10 workarounds.
    SetupGfx10Workarounds(device, pSettings);

    // Setup any Gfx10.1 workarounds.
    SetupGfx101Workarounds(device, pSettings, pCoreSettings);

    // Setup any Navi10 specific workarounds.

    pSettings->waSdmaPreventCompressedSurfUse = true;

    pSettings->waFixPostZConservativeRasterization = true;

    pSettings->waTessIncorrectRelativeIndex = true;

    pSettings->waForceZonlyHtileForMipmaps = true;
} // PAL_BUILD_NAVI10

// =====================================================================================================================
// Setup workarounds that only apply to Navi14.
static void SetupNavi14Workarounds(
    const Pal::Device&  device,
    Gfx9PalSettings*    pSettings,
    PalSettings*        pCoreSettings)
{
    // Setup any Gfx10 workarounds.
    SetupGfx10Workarounds(device, pSettings);

    // Setup any Gfx10.1 workarounds.
    SetupGfx101Workarounds(device, pSettings, pCoreSettings);

    // Setup any Navi14 specific workarounds.

    pSettings->waLateAllocGs0 = true;
    pSettings->nggSupported   = false;
}

// =====================================================================================================================
// Setup workarounds that only apply to Navi21.
static void SetupNavi21Workarounds(
    const Pal::Device&  device,
    Gfx9PalSettings*    pSettings)
{
    // Setup any Gfx10 workarounds.
    SetupGfx10Workarounds(device, pSettings);

    // Setup any Navi2x workarounds.
    SetupNavi2xWorkarounds(device, pSettings);

    // Setup any Navi21 workarounds.

    pSettings->waCeDisableIb2 = true;

    pSettings->waDisableFmaskNofetchOpOnFmaskCompressionDisable = true;

    pSettings->waVgtFlushNggToLegacy = true;

    pSettings->waDisableVrsWithDsExports = true;
}

// =====================================================================================================================
// Override Gfx9 layer settings. This also includes setting up the workaround flags stored in the settings structure
// based on chip Family & ID.
//
// The workaround flags setup here can be overridden if the settings are set.
void SettingsLoader::OverrideDefaults(
    PalSettings* pSettings)
{
    const Pal::Device& device = *m_pDevice;

    uint16 minBatchBinSizeWidth  = 128;
    uint16 minBatchBinSizeHeight = 64;

    // Enable workarounds which are common to all Gfx9 hardware.
    if (IsGfx9(device))
    {
        m_settings.nggSupported = false;

        m_settings.waColorCacheControllerInvalidEviction = true;

        m_settings.waDisableHtilePrefetch = true;

        m_settings.waOverwriteCombinerTargetMaskOnly = true;

        m_settings.waDummyZpassDoneBeforeTs = true;

        m_settings.waLogicOpDisablesOverwriteCombiner = true;

        // Metadata is not pipe aligned once we get down to the mip chain within the tail
        m_settings.waitOnMetadataMipTail = true;

        // Set this to 1 in Gfx9 to enable CU soft group for PS by default. VS soft group is turned off by default.
        m_settings.numPsWavesSoftGroupedPerCu = 1;

        m_settings.waDisableSCompressSOnly = true;

        if (IsVega10(device) || IsRaven(device))
        {
            m_settings.waHtilePipeBankXorMustBeZero = true;

            m_settings.waWrite1xAASampleLocationsToZero = true;

            m_settings.waMiscPopsMissedOverlap = true;

            m_settings.waMiscScissorRegisterChange = true;

            m_settings.waDisableDfsmWithEqaa = true;

            m_settings.waDisable24BitHWFormatForTCCompatibleDepth = true;
        }

        if (IsVega20(device))
        {
            m_settings.waDisableDfsmWithEqaa = true;
        }

        if (device.ChipProperties().gfx9.rbPlus != 0)
        {
            m_settings.waRotatedSwizzleDisablesOverwriteCombiner = true;
        }

        if (IsVega10(device) || IsRaven(device) || IsRaven2(device) || IsRenoir(device))
        {
            m_settings.waMetaAliasingFixEnabled = false;
        }
    }
    else if (IsGfx10(device))
    {
        if (IsNavi10(device))
        {
            SetupNavi10Workarounds(device, &m_settings, pSettings);
        }
        else if (IsNavi14(device))
        {
            SetupNavi14Workarounds(device, &m_settings, pSettings);
        }
        else if (IsNavi21(device))
        {
            SetupNavi21Workarounds(device, &m_settings);
        }
        // For 4 or less RB parts, we expect some overlap for metadata requests across RBs.
        if (device.ChipProperties().gfx9.numActiveRbs <= 4)
        {
            m_settings.cbDbCachePolicy = (Gfx10CbDbCachePolicyLruCmask | Gfx10CbDbCachePolicyLruDcc |
                                          Gfx10CbDbCachePolicyLruFmask | Gfx10CbDbCachePolicyLruHtile);

            // Additional default settings that are beneficial for smaller ASICs.
            m_settings.disableBinningPsKill = false;
            m_settings.gfx10GePcAllocNumLinesPerSeLegacyNggPassthru = 0;
            m_settings.gfx10GePcAllocNumLinesPerSeNggCulling = 0;
            m_settings.depthStencilFastClearComputeThresholdSingleSampled = (1024 * 1024) - 1;
            m_settings.binningContextStatesPerBin = 3;
            m_settings.binningPersistentStatesPerBin = 8;
            m_settings.allowNggOnAllCusWgps = true;
            m_settings.nggLateAllocGs = 0;
            m_settings.ignoreDepthForBinSizeIfColorBound = true;

            minBatchBinSizeWidth  = 64;
            minBatchBinSizeHeight = 64;
        }

        if (IsGfx103(device))
        {
            m_settings.gfx103DisableAsymmetricWgpForPs = true;
        }

    }

    // If minimum sizes are 0, then use default size.
    if (m_settings.minBatchBinSize.width == 0)
    {
        m_settings.minBatchBinSize.width = minBatchBinSizeWidth;
    }
    if (m_settings.minBatchBinSize.height == 0)
    {
        m_settings.minBatchBinSize.height = minBatchBinSizeHeight;
    }

    // If we allow > 1 Ctx or Persistent state / batch then the driver should BREAK_BATCH on new PS.
    if ((m_settings.binningContextStatesPerBin > 1) || (m_settings.binningPersistentStatesPerBin > 1))
    {
        m_settings.batchBreakOnNewPixelShader = true;
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
