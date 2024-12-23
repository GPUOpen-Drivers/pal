/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9SettingsLoader.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/pm4CmdBuffer.h"
#include "core/hw/amdgpu_asic.h"
#include "core/devDriverUtil.h"
#include "devDriverServer.h"

namespace Pal
{
#include "g_gfx11SwWarDetection.h"
}

using namespace Util;
using namespace Util::Literals;

namespace Pal
{
namespace Gfx9
{

// Minimum ucode version that supports the packed register pairs packet. Temporarily set to UINT_MAX to disable packet
// usage till additional testing and validation is completed.
constexpr uint32 Gfx11MinPfpVersionPackedRegPairsPacket   = 1448;
// Minimum ucode version that supports the packed register pairs packet for compute. Currently not supported.
constexpr uint32 Gfx11MinPfpVersionPackedRegPairsPacketCs = UINT_MAX;
// Minimum ucode version that supports the EVENT_WRITE_ZPASS packet.
constexpr uint32 Gfx11MinPfpVersionEventWriteZpassPacket  = 1458;
// Minimum ucode version that RELEASE_MEM packet supports waiting CP DMA.
constexpr uint32 Gfx11MinPfpVersionReleaseMemSupportsWaitCpDma = 2150;

// =====================================================================================================================
SettingsLoader::SettingsLoader(
    Pal::Device* pDevice)
    :
    DevDriver::SettingsBase(&m_settings, sizeof(m_settings)),
    m_pDevice(pDevice),
    m_settings{},
    m_gfxLevel(pDevice->ChipProperties().gfxLevel)
{
}

// =====================================================================================================================
SettingsLoader::~SettingsLoader()
{
}

// =====================================================================================================================
// Initializes the settings struct and assign default setting values.
Result SettingsLoader::Init()
{
    DD_RESULT ddResult = SetupDefaultsAndPopulateMap();
    return DdResultToPalResult(ddResult);
}

// =====================================================================================================================
// Validates that the settings structure has legal values. Variables that require complicated initialization can also
// be initialized here.
void SettingsLoader::ValidateSettings(
    PalSettings* pSettings)
{
    const auto& chipProps   = m_pDevice->ChipProperties();
    const auto& engineProps = m_pDevice->EngineProperties();
    const auto& gfx9Props   = chipProps.gfx9;

    auto* pPalSettings = m_pDevice->GetPublicSettings();

    if (m_settings.binningMaxAllocCountLegacy == 0)
    {
        if (IsGfx10(*m_pDevice))
        {
            // In Gfx10 there is a single view of the PC rather than a division per SE.
            // The recommended value for this is to allow a single batch to consume at
            // most 1/3 of the parameter cache lines.
            m_settings.binningMaxAllocCountLegacy = gfx9Props.parameterCacheLines / 3;
        }
    }

    if (IsGfx11(*m_pDevice))
    {
        if (m_settings.binningMaxAllocCountNggOnChip == UINT32_MAX)
        {
            // GFX11 eliminates the parameter cache, so the determination needs to come from elsewhere.
            // In addition, binningMaxAllocCountLegacy has no affect on GFX11.
            // The expectation is that a value of ~16, which is a workload size of 1024 prims, assuming 2 verts per
            // prim and 16 attribute groups (256 verts * 16 attr groups / 2 verts per prim / 2 screen tiles = 1024),
            // should be sufficient for building a batch. This value can be tuned further as more apps are running
            // and perf investigations continue to progress.
            m_settings.binningMaxAllocCountNggOnChip = 16;
        }
    }
    else if ((m_settings.binningMaxAllocCountNggOnChip == 0) ||
             (m_settings.binningMaxAllocCountNggOnChip == UINT32_MAX))
    {
        // With NGG + on chip PC there is a single view of the PC rather than a
        // division per SE. The recommended value for this is to allow a single batch to
        // consume at most 1/3 of the parameter cache lines.
        // This applies to all of Gfx10, as the PC only has a single view for both legacy and NGG.
        m_settings.binningMaxAllocCountNggOnChip = gfx9Props.parameterCacheLines / 3;
    }

    // If a specific late-alloc GS value was requested in the panel, we want to supersede a client set value. This may
    // still be overridden to 0 below for sufficiently small GPUs.
    pPalSettings->nggLateAllocGs = m_settings.overrideNggLateAllocGs >= 0 ? m_settings.overrideNggLateAllocGs
                                                                          : pPalSettings->nggLateAllocGs;

    // Some hardware can support 128 offchip buffers per SE, but most support 64.
    const uint32 maxOffchipLdsBuffersPerSe = (gfx9Props.doubleOffchipLdsBuffers ? 128 : 64);

    // Compute the number of offchip LDS buffers for the whole chip.
    uint32 maxOffchipLdsBuffers = (gfx9Props.numShaderEngines * maxOffchipLdsBuffersPerSe);

    if (IsGfx11(*m_pDevice))
    {
        // Gfx11 has more SEs than our previous products, and so the number of maxOffchipLdsBuffers is now a factor
        // of the number of SEs in the chip and there is no minimum.
        constexpr uint32 Gfx11MaxOffchipLdsBufferPerSe = 256;
        maxOffchipLdsBuffers = Gfx11MaxOffchipLdsBufferPerSe * gfx9Props.numShaderEngines;
    }
    else
    {
        // On gfx9, the offchip buffering register has enough space to support the full 512 buffers.
        maxOffchipLdsBuffers = Min(maxOffchipLdsBuffers, 512U);
    }

    // Validate the number of offchip LDS buffers used for tessellation.
    if (pSettings->numOffchipLdsBuffers > 0)
    {
        if (m_settings.useMaxOffchipLdsBuffers == true)
        {
            // Use the maximum amount of offchip-LDS buffers.
            pSettings->numOffchipLdsBuffers = maxOffchipLdsBuffers;
        }
        else
        {
            // Clamp to the maximum amount of offchip LDS buffers.
            pSettings->numOffchipLdsBuffers =
                    Min(maxOffchipLdsBuffers, pSettings->numOffchipLdsBuffers);
        }
    }

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

    pSettings->prefetchClampSize = Pow2Align(pSettings->prefetchClampSize, 4096);

    // By default, gfx9RbPlusEnable is true, and it should be overridden to false
    // if the ASIC doesn't support Rb+.
    if (gfx9Props.rbPlus == 0)
    {
        m_settings.rbPlusEnable = false;
        pPalSettings->optDepthOnlyExportRate = false;
    }

    if (gfx9Props.supportOutOfOrderPrimitives == 0)
    {
        m_settings.enableOutOfOrderPrimitives = OutOfOrderPrimDisable;
    }
    if (pPalSettings->binningContextStatesPerBin == 0)
    {
        pPalSettings->binningContextStatesPerBin = 1;
    }
    if (pPalSettings->binningPersistentStatesPerBin == 0)
    {
        pPalSettings->binningPersistentStatesPerBin = 1;
    }
    if (pPalSettings->disableBinningPsKill == OverrideMode::Default)
    {
        pPalSettings->disableBinningPsKill = OverrideMode::Enabled;
    }

    if (IsGfx10(*m_pDevice))
    {
        // GFX10 doesn't use the convoluted meta-addressing scheme that GFX9 does, so disable
        // the "optimized" algorithm for processing the meta-equations.
        m_settings.optimizedFastClear = 0;

        if ((m_settings.waLateAllocGs0) && m_settings.nggSupported)
        {
            pPalSettings->nggLateAllocGs = 0;

            // This workaround requires that tessellation distribution is enabled and the distribution factors are
            // non-zero.
            if (pPalSettings->distributionTessMode == DistributionTessOff)
            {
                pPalSettings->distributionTessMode = DistributionTessDefault;
            }
        }

        if (m_settings.rbPlusEnable)
        {
            m_settings.useCompToSingle |= (Gfx10UseCompToSingle8bpp | Gfx10UseCompToSingle16bpp);
        }
    }

    // On GFX103+ WGP harvesting asymmetric configuration, for pixel shader waves the extra WGP is not useful as all
    // of GFX103 splits workloads (waves) evenly among the SE. Using Navi2x as an example: For pixel shader workloads,
    // pixels are split evenly among the 2 SA within an SE as well. So for basic large uniform PS workload, pixels are
    // split evenly among all 8 SA of a Navi2x and the work-load will only finish as fast as the SA with the fewest # of
    // WGP. In essence this means that a 72 CU Navi21 behaves like a 64 CU Navi21 for pixel shader workloads.
    // We should mask off the extra WGP from PS waves on WGP harvesting asymmetric configuration.
    // This will reduce power consumption when not needed and allow to the GPU to clock higher.
    if (IsGfx103Plus(*m_pDevice) && m_settings.gfx103PlusDisableAsymmetricWgpForPs)
    {
        m_settings.psCuEnLimitMask = (1 << (gfx9Props.gfx10.minNumWgpPerSa * 2)) - 1;
    }

    uint32 tessFactRingSizeMask = Gfx10::VGT_TF_RING_SIZE__SIZE_MASK;
    uint32 tessFactScalar       = gfx9Props.numShaderEngines;

    if (IsGfx11(*m_pDevice))
    {
        // All GFX11 parts support RB+ which according to HW guarantees that
        // every 256 bytes line up on consistent pixel boundaries.
        m_settings.useCompToSingle |= (Gfx10UseCompToSingle8bpp | Gfx10UseCompToSingle16bpp);

        // Programming this on GFX11 is slightly different. The size has changed but more importantly the value is now
        // treated as per SE by the hardware so we don't need to scale by the number of SE's manually any more.
        tessFactRingSizeMask = Gfx11::VGT_TF_RING_SIZE__SIZE_MASK;
        tessFactScalar       = 1;
    }

    // Default values for Tess Factor buffer are safe. This could have been changed by the panel settings so for a
    // sanity check let's adjust the tess factor buffer size down if it's to big:
    if ((pSettings->tessFactorBufferSizePerSe * tessFactScalar) > tessFactRingSizeMask)
    {
        pSettings->tessFactorBufferSizePerSe = Pow2AlignDown(tessFactRingSizeMask, tessFactScalar) / tessFactScalar;
        static_assert(VGT_TF_RING_SIZE__SIZE__SHIFT == 0, "VGT_TF_RING_SIZE::SIZE shift is no longer zero!");
    }

    if (IsGfx11(*m_pDevice))
    {
        // GFX11 doesn't use the convoluted meta-addressing scheme that GFX9 does either, so disable
        // the "optimized" algorithm for processing the meta-equations.
        m_settings.optimizedFastClear = 0;

        // Vertex Attribute ring buffer must be aligned respect the maximum for the chip
        const uint32 maxAttribRingBufferSizePerSe =
            Pow2AlignDown(Gfx11VertexAttributeRingMaxSizeBytes / gfx9Props.numShaderEngines,
                          Gfx11VertexAttributeRingAlignmentBytes);

        m_settings.gfx11VertexAttributesRingBufferSizePerSe =
            Pow2Align(m_settings.gfx11VertexAttributesRingBufferSizePerSe, Gfx11VertexAttributeRingAlignmentBytes);

        m_settings.gfx11VertexAttributesRingBufferSizePerSe =
            Min(maxAttribRingBufferSizePerSe, m_settings.gfx11VertexAttributesRingBufferSizePerSe);

        if ((m_settings.waForceSpiThrottleModeNonZero) &&
            ((m_settings.defaultSpiGsThrottleCntl2 & Gfx11::SPI_GS_THROTTLE_CNTL2__SPI_THROTTLE_MODE_MASK) == 0))
        {
            // Force mode 1 (expected default) if we haven't already forced it via the setting.
            m_settings.defaultSpiGsThrottleCntl2 |= (1 << Gfx11::SPI_GS_THROTTLE_CNTL2__SPI_THROTTLE_MODE__SHIFT);
        }

        // GFX11 has different VRS programming than GFX10 and does not need this optimization.
        m_settings.optimizeNullSourceImage = false;

        // Clamp the sample-mask-tracker to the HW-legal values of 3-15.
        m_settings.gfx11SampleMaskTrackerWatermark = ((m_settings.gfx11SampleMaskTrackerWatermark == 0) ?
            0 : Clamp(m_settings.gfx11SampleMaskTrackerWatermark, 3u, 15u));

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 818
        // Replace the "PublicSetting" enum with a final value. The rest of PAL can ignore Ac01WaPublicSetting.
        if (m_settings.waDisableAc01 == Ac01WaPublicSetting)
        {
            m_settings.waDisableAc01 = pPalSettings->ac01WaNotNeeded ? Ac01WaAllowAc01 : Ac01WaForbidAc01;
        }
#endif

        if (m_settings.waNoOpaqueOreo && (m_settings.gfx11OreoModeControl == OMODE_O_THEN_B))
        {
            m_settings.gfx11OreoModeControl = OMODE_BLEND;
        }

#if PAL_BUILD_GFX115
        if (IsGfx115(*m_pDevice))
        {
            // The "ONE_PRIM_PER_BATCH" mode will be removed in GFX11.5 and later
            // This means that "BINNING_DISABLED__GFX11" is the only valid value that disables binning.
            m_settings.disableBinningMode = BINNING_DISABLED__GFX11;
        }
#endif

    }
    else
    {
        m_settings.gfx11VertexAttributesRingBufferSizePerSe = 0;
    }

    if ((pPalSettings->distributionTessMode == DistributionTessTrapezoidOnly) ||
        (pPalSettings->distributionTessMode == DistributionTessDefault))
    {
        pPalSettings->distributionTessMode = DistributionTessTrapezoid;
    }

    // When WD load balancing flowchart optimization is enabled, the primgroup size cannot exceed 253.
    m_settings.primGroupSize = Min(253u, m_settings.primGroupSize);

    // Set default value for DCC BPP Threshold unless it was already overriden
    if (pPalSettings->dccBitsPerPixelThreshold == UINT_MAX)
    {
        pPalSettings->dccBitsPerPixelThreshold = 0;
    }

    // For sufficiently small GPUs, we want to disable late-alloc and allow NGG waves access to the whole chip.
    // On gfx10, non-NGG draws may in flight at the same time as NGG draws, and the deadlock may encountered if
    // NGG draws consume all CUs and meanwhile some non-NGG draws cannot drain.
    // On gfx11, we could only have NGG draws. Furthermore, ATM makes it pretty much impossible for PC space
    // to late-alloc, and the late-alloc really only then applies to prim/pos buffer space, which should be more
    // than enough for nearly every workload that can execute on a tiny chip.
    if (IsGfx11(chipProps.gfxLevel) &&
        ((chipProps.gfx9.gfx10.minNumWgpPerSa <= 2) || (chipProps.gfx9.numActiveCus < 4)))
    {
        constexpr uint32 MaskEnableAll  = UINT_MAX;
        m_settings.gsCuEnLimitMask      = MaskEnableAll;
        m_settings.allowNggOnAllCusWgps = true;
        pPalSettings->nggLateAllocGs    = 0;
    }

    // nggLateAllocGs can NOT be greater than 127.
    pPalSettings->nggLateAllocGs = Min(pPalSettings->nggLateAllocGs, 127u);

    // Since XGMI is much faster than PCIe, PAL should not reduce the number of RBs to increase the PCIe throughput
    if (chipProps.p2pSupport.xgmiEnabled != 0)
    {
        pSettings->nonlocalDestGraphicsCopyRbs = UINT_MAX;
    }

    // The last step is to update the experiments:
    Platform* pPlatform                  = m_pDevice->GetPlatform();
    PalExperimentsSettings* pExpSettings = pPlatform->GetExpSettingsPtr();

    pExpSettings->expSynchronizationOptimizationOreoModeControl = (m_settings.gfx11OreoModeControl == Gfx11OreoModeBlend);
    pExpSettings->expDepthStencilTextureCompression             = !m_settings.htileEnable;

    // Copy public setting 'waitOnFlush' to final private setting used in rest of pal
    m_settings.waitOnFlush |= pPalSettings->waitOnFlush;
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
    pSettings->waCmaskImageSyncs = (device.ChipProperties().cpUcodeVersion < 28);

    // We can't use CP_PERFMON_STATE_STOP_COUNTING when using an SQ counters or they can get stuck off until we reboot.
    pSettings->waNeverStopSqCounters = true;

    pSettings->waDccCacheFlushAndInv = true;
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

    // When instance packing is enabled for adjacent prim_types and num_instances >1, pipeline stats generated by GE
    // are incorrect.
    // This workaround is to disable instance packing for adjacent prim_types.
    pSettings->waDisableInstancePacking = true;

    // On Navi2x hw, the polarity of AutoFlushMode is inverted, thus setting this value to true as a Navi2x workaround.
    // The AUTO_FLUSH_MODE flag will be properly inverted as a part of PerfExperiment.
    pSettings->waAutoFlushModePolarityInversed = true;
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
}

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
// Setup workarounds that only apply to Navi12.
static void SetupNavi12Workarounds(
    const Pal::Device&  device,
    Gfx9PalSettings*    pSettings,
    PalSettings*        pCoreSettings)
{
    // Setup any Gfx10 workarounds.
    SetupGfx10Workarounds(device, pSettings);

    // Setup any Gfx10.1 workarounds.
    SetupGfx101Workarounds(device, pSettings, pCoreSettings);

    // Setup any Navi12 workarounds.
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
// Setup workarounds that only apply to Navi22.
static void SetupNavi22Workarounds(
    const Pal::Device&  device,
    Gfx9PalSettings*    pSettings)
{
    // Setup any Gfx10 workarounds.
    SetupGfx10Workarounds(device, pSettings);

    // Setup any Navi2x workarounds.
    SetupNavi2xWorkarounds(device, pSettings);

    // Setup any Navi22 workarounds.

    pSettings->waCeDisableIb2 = true;

    pSettings->waDisableVrsWithDsExports = true;
}

// =====================================================================================================================
// Setup workarounds that only apply to Navi23.
static void SetupNavi23Workarounds(
    const Pal::Device&  device,
    Gfx9PalSettings*    pSettings)
{
    // Setup any Gfx10 workarounds.
    SetupGfx10Workarounds(device, pSettings);

    // Setup any Navi2x workarounds.
    SetupNavi2xWorkarounds(device, pSettings);

    // Setup any Navi23 workarounds.
    pSettings->waBadSqttFinishResults = true;
}

// =====================================================================================================================
// Setup workarounds that only apply to Navi24.
static void SetupNavi24Workarounds(
    const Pal::Device&  device,
    Gfx9PalSettings*    pSettings)
{
    // Setup any Gfx10 workarounds.
    SetupGfx10Workarounds(device, pSettings);

    // Setup any Navi2x workarounds.
    SetupNavi2xWorkarounds(device, pSettings);

    // Setup any Navi24 workarounds.
    pSettings->waBadSqttFinishResults = true;
}

// =====================================================================================================================
// Setup workarounds that only apply to Rembrandt.
static void SetupRembrandtWorkarounds(
    const Pal::Device& device,
    Gfx9PalSettings*   pSettings)
{
    // Setup any Gfx10 workarounds.
    SetupGfx10Workarounds(device, pSettings);

    // Setup any Navi2x workarounds.
    SetupNavi2xWorkarounds(device, pSettings);

    // Setup any Rembrandt workarounds.
    pSettings->waBadSqttFinishResults = true;
}

// =====================================================================================================================
// Setup workarounds that only apply to Raphael.
static void SetupRaphaelWorkarounds(
    const Pal::Device& device,
    Gfx9PalSettings*   pSettings)
{
    // Setup any Gfx10 workarounds.
    SetupGfx10Workarounds(device, pSettings);

    // Setup any Navi2x workarounds.
    SetupNavi2xWorkarounds(device, pSettings);

    // Setup any Raphael workarounds.
}

// =====================================================================================================================
// Setup workarounds that only apply to Mendocino.
static void SetupMendocinoWorkarounds(
    const Pal::Device& device,
    Gfx9PalSettings* pSettings)
{
    // Setup any Gfx10 workarounds.
    SetupGfx10Workarounds(device, pSettings);

    // Setup any Navi2x workarounds.
    SetupNavi2xWorkarounds(device, pSettings);

    // Setup any Mendocino workarounds.
}

// =====================================================================================================================
// Setup workarounds for GFX11
static void SetupGfx11Workarounds(
    const Pal::Device&  device,
    Gfx9PalSettings*    pSettings)
{
    const uint32 familyId = device.ChipProperties().familyId;
    const uint32 eRevId   = device.ChipProperties().eRevId;

    Gfx11SwWarDetection workarounds = {};
    const bool waFound = DetectGfx11SoftwareWorkaroundsByChip(familyId, eRevId, &workarounds);
    PAL_ASSERT(waFound);

#if PAL_ENABLE_PRINTS_ASSERTS
    constexpr uint32 HandledWaMask[] = { 0x1E793001, 0x00284B00 }; // Workarounds handled by PAL.
    constexpr uint32 OutsideWaMask[] = { 0xE0068DFE, 0x000714FC }; // Workarounds handled by other components.
    constexpr uint32 MissingWaMask[] = { 0x00004000, 0x0000A001 }; // Workarounds that should be handled by PAL that
                                                                   // are not yet implemented or are unlikey to be
                                                                   // implemented.
    constexpr uint32 InvalidWaMask[] = { 0x01800200, 0x00102002 }; // Workarounds marked invalid, thus not handled.
    static_assert((sizeof(HandledWaMask) == sizeof(Gfx11InactiveMask)) &&
                  (sizeof(OutsideWaMask) == sizeof(Gfx11InactiveMask)) &&
                  (sizeof(MissingWaMask) == sizeof(Gfx11InactiveMask)) &&
                  (sizeof(InvalidWaMask) == sizeof(Gfx11InactiveMask)),
                  "Workaround Masks do not match expected size!");

    constexpr uint32 InactiveMask[] = { ~(HandledWaMask[0] | OutsideWaMask[0] | MissingWaMask[0] | InvalidWaMask[0] ),
                                        ~(HandledWaMask[1] | OutsideWaMask[1] | MissingWaMask[1] | InvalidWaMask[1] ) };
    static_assert((InactiveMask[0] == Gfx11InactiveMask[0]) && (InactiveMask[1] == Gfx11InactiveMask[1]),
                  "Workaround Masks do not match!");
#endif

    static_assert(Gfx11NumWorkarounds == 54, "Workaround count mismatch between PAL and SWD");

    if (workarounds.ppPbbPBBBreakBatchDifferenceWithPrimLimit_FpovLimit_DeallocLimit_A_)
    {
        if (pSettings->binningFpovsPerBatch == 0)
        {
            pSettings->binningFpovsPerBatch = 255;
        }

        if (pSettings->binningMaxAllocCountNggOnChip == 0)
        {
            pSettings->binningMaxAllocCountNggOnChip = 255;
        }
    }

    pSettings->waForceSpiThrottleModeNonZero =
        workarounds.sioSpiBciSpyGlassRevealedABugInSpiRaRscselGsThrottleModuleWhichIsCausedByGsPsVgprLdsInUsesVariableDroppingMSBInRelevantMathExpression_A_;

    pSettings->waReplaceEventsWithTsEvents = workarounds.ppDbPWSIssueForDepthWrite_TextureRead_A_;
    pSettings->waAddPostambleEvent         = workarounds.geometryGeGEWdTe11ClockCanStayHighAfterShaderMessageThdgrp_A_;
    pSettings->waLineStippleReset          = workarounds.geometryPaPALineStippleResetError_A_;
    pSettings->waCwsrThreadgroupTrap       = workarounds.shaderSqSqgNV3xHWBugCausesHangOnCWSRWhenTGCreatedOnSA1_A_;

    // These two settings are different ways of solving the same problem.  We've experimentally
    // determined that "intrinsic rate enable" has better performance.
    pSettings->gfx11DisableRbPlusWithBlending = false;
    pSettings->waEnableIntrinsicRateEnable    = workarounds.sioSpiBciSPI_TheOverRestrictedExportConflictHQ_HoldingQueue_PtrRuleMayReduceTheTheoreticalExpGrantThroughput_PotentiallyIncreaseOldNewPSWavesInterleavingChances_A_;

    // This workaround requires disabling use of the AC01 clear codees, which is what the "force regular clear code"
    // panel setting is already designed to do.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 818
    if (workarounds.ppCbGFX11DCC31DXXPNeedForSpeedHeat_BlackFlickeringDotCorruption_A_ != 0)
    {
        // Note that the public setting defaults to "enable the workaround".
        pSettings->waDisableAc01 = Ac01WaPublicSetting;
    }
#endif

    pSettings->waSqgTtWptrOffsetFixup = workarounds.shaderSqSqgSQGTTWPTRIssues_A_;

    pSettings->waCbPerfCounterStuckZero =
        workarounds.gcPvPpCbCBPerfcountersStuckAtZeroAfterPerfcounterStopEventReceived_A_;

    pSettings->waForceLockThresholdZero = workarounds.sioSpiBciSoftLockIssue_A_;

    pSettings->waSetVsXyNanToInfZero = workarounds.geometryPaStereoPositionNanCheckBug_A_;

    pSettings->waIncorrectMaxAllowedTilesInWave = workarounds.ppDbPpScSCDBHangNotSendingWaveConflictBackToSPI_A_;

    if (IsNavi31(device) || IsNavi32(device))
    {
        pSettings->shaderPrefetchSizeBytes = 0;
    }

    if (pSettings->gfx11SampleMaskTrackerWatermark > 0)
    {
        pSettings->waitOnFlush |= (WaitAfterCbFlush | WaitBeforeBarrierEopWithCbFlush);
    }

    // Some GFX11 IP included an override for capping the maximum number of fragmants that DCC will try to compress.
    pSettings->waDccMaxCompFrags = workarounds.ppCbFDCCKeysWithFragComp_MSAASettingCauseHangsInCB_A_;

    // This snuck past the SWD refactor and was just hard-coded on in gfx9Device.cpp. It was moved here instead.
    pSettings->waDccCacheFlushAndInv = true;

    pSettings->waNoOpaqueOreo =
        workarounds.ppDbDBOreoOpaqueModeHWBug_UdbOreoScoreBoard_udbOsbData_udbOsbdMonitor_ostSampleMaskMismatchOREOScoreboardStoresInvalidEWaveIDAndIncorrectlySetsRespectiveValidBit_A_;
}

// =====================================================================================================================
// Override Gfx9 layer settings. This also includes setting up the workaround flags stored in the settings structure
// based on chip Family & ID.
//
// The workaround flags setup here can be overridden if the settings are set.
void SettingsLoader::OverrideDefaults(
    PalSettings* pSettings)
{
    const Pal::Device&            device          = *m_pDevice;
    PalPublicSettings*            pPublicSettings = m_pDevice->GetPublicSettings();
    const PalExperimentsSettings& expSettings     = m_pDevice->GetPlatform()->GetExpSettings();

    if (expSettings.expSynchronizationOptimizationOreoModeControl.ValueOr(false))
    {
        m_settings.gfx11OreoModeControl = Gfx11OreoModeBlend;
    }

    if (expSettings.expDepthStencilTextureCompression.ValueOr(false))
    {
        m_settings.htileEnable = false;
    }

    pSettings->tessFactorBufferSizePerSe = 0x3000;

    if (IsGfx10(device))
    {
        if (IsNavi10(device))
        {
            SetupNavi10Workarounds(device, &m_settings, pSettings);
        }
        else if (IsNavi12(device))
        {
            SetupNavi12Workarounds(device, &m_settings, pSettings);
        }
        else if (IsNavi14(device))
        {
            SetupNavi14Workarounds(device, &m_settings, pSettings);
        }
        else if (IsNavi21(device))
        {
            SetupNavi21Workarounds(device, &m_settings);
        }
        else if (IsNavi22(device))
        {
            SetupNavi22Workarounds(device, &m_settings);
        }
        else if (IsNavi23(device))
        {
            SetupNavi23Workarounds(device, &m_settings);
        }
        else if (IsNavi24(device))
        {
            SetupNavi24Workarounds(device, &m_settings);
        }
        else if (IsRembrandt(device))
        {
            SetupRembrandtWorkarounds(device, &m_settings);
        }
        else if (IsRaphael(device))
        {
            SetupRaphaelWorkarounds(device, &m_settings);
        }
        else if (IsMendocino(device))
        {
            SetupMendocinoWorkarounds(device, &m_settings);
        }
    }
    else if (IsGfx11(device))
    {
        SetupGfx11Workarounds(device, &m_settings);

        m_settings.numTsMsDrawEntriesPerSe = 1024;

        // GFX11 supports modifying the group size.  Use the maximum setting.
        m_settings.ldsPsGroupSize = Gfx10LdsPsGroupSize::Gfx10LdsPsGroupSizeDouble;

        // GFX11 doesn't have HW support for DB -> CB copies.
        m_settings.allowDepthCopyResolve = false;

        m_settings.defaultSpiGsThrottleCntl1 = Gfx11::mmSPI_GS_THROTTLE_CNTL1_DEFAULT;
        m_settings.defaultSpiGsThrottleCntl2 = Gfx11::mmSPI_GS_THROTTLE_CNTL2_DEFAULT;

        // Recommended by HW that LATE_ALLOC_GS be set to 63 on GFX11
        pPublicSettings->nggLateAllocGs = 63;

        // Apply this to all Gfx11 APUs
        if (device.ChipProperties().gpuType == GpuType::Integrated)
        {
            if (device.ChipProperties().gfxip.tccSizeInBytes >= 2_MiB)
            {
                // APU tuning with 2MB L2 Cache shows ATM Ring Buffer size 768 KiB yields best performance
                m_settings.gfx11VertexAttributesRingBufferSizePerSe = 768_KiB;
            }
            else
            {
                // For APU's with smaller L2 Cache, limit ATM Ring Buffer size to 512 KiB
                m_settings.gfx11VertexAttributesRingBufferSizePerSe = 512_KiB;
            }
        }
    }

    if (IsGfx103Plus(device))
    {
        m_settings.gfx103PlusDisableAsymmetricWgpForPs = true;
    }

    const uint32 pfpUcodeVersion = m_pDevice->ChipProperties().pfpUcodeVersion;

    m_settings.gfx11EnableContextRegPairOptimization = pfpUcodeVersion >= Gfx11MinPfpVersionPackedRegPairsPacket;
    m_settings.gfx11EnableShRegPairOptimization      = pfpUcodeVersion >= Gfx11MinPfpVersionPackedRegPairsPacket;
    m_settings.gfx11EnableShRegPairOptimizationCs    = pfpUcodeVersion >= Gfx11MinPfpVersionPackedRegPairsPacketCs;
    m_settings.gfx11EnableZpassPacketOptimization    = pfpUcodeVersion >= Gfx11MinPfpVersionEventWriteZpassPacket;

    // Only enable for RS64 FW based GFX11 which adds the support.
    if ((device.ChipProperties().gfxLevel != GfxIpLevel::GfxIp11_0) ||
        (pfpUcodeVersion < Gfx11MinPfpVersionReleaseMemSupportsWaitCpDma))
    {
        m_settings.gfx11EnableReleaseMemWaitCpDma = false;
    }

    // If minimum sizes are 0, then use default size.
    constexpr uint16 MinBatchBinSizeWidth  = 128;
    constexpr uint16 MinBatchBinSizeHeight = 64;

    if (m_settings.minBatchBinSize.width == 0)
    {
        m_settings.minBatchBinSize.width = MinBatchBinSizeWidth;
    }
    if (m_settings.minBatchBinSize.height == 0)
    {
        m_settings.minBatchBinSize.height = MinBatchBinSizeHeight;
    }

    // Use the default minimum DCC block compression size for the device
    if (m_settings.minDccCompressedBlockSize == Gfx9MinDccCompressedBlockSize::DefaultBlockSize)
    {
        if (device.ChipProperties().gpuType == GpuType::Integrated)
        {
            {
                //
                //
                m_settings.minDccCompressedBlockSize = Gfx9MinDccCompressedBlockSize::BlockSize64B;
            }
        }
        else
        {
            m_settings.minDccCompressedBlockSize = Gfx9MinDccCompressedBlockSize::BlockSize32B;
        }
    }

    pPublicSettings->waitOnFlush = m_settings.waitOnFlush;
}

// =====================================================================================================================
bool SettingsLoader::ReadSetting(
    const char*          pSettingName,
    Util::ValueType      valueType,
    void*                pValue,
    InternalSettingScope settingType,
    size_t               bufferSize)
{
    return m_pDevice->ReadSetting(
        pSettingName,
        valueType,
        pValue,
        settingType,
        bufferSize);
}

// =====================================================================================================================
// The settings hash is used during pipeline loading to verify that the pipeline data is compatible between when it was
// stored and when it was loaded.
void SettingsLoader::GenerateSettingHash()
{
    MetroHash128::Hash(
        reinterpret_cast<const uint8*>(&m_settings),
        sizeof(Gfx9PalSettings),
        m_settingsHash.bytes);
}

} // Gfx9
} // Pal
