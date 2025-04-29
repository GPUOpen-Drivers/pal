/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palHashMapImpl.h"
#include "core/device.h"
#include "core/hw/gfxip/gfx12/gfx12Chip.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12SettingsLoader.h"
#include "core/devDriverUtil.h"
#include "devDriverServer.h"

namespace Pal
{
#include "g_gfx12SwWarDetection.h"
}

using namespace DevDriver::SettingsURIService;
using namespace Util;
using namespace Util::Literals;

namespace Pal
{
namespace Gfx12
{

// Minimum ucode version that RELEASE_MEM packet supports waiting CP DMA.
constexpr uint32 MinPfpVersionReleaseMemSupportsWaitCpDma = 2330;

// =====================================================================================================================
// Constructor for the SettingsLoader object.
SettingsLoader::SettingsLoader(
    Pal::Device* pDevice)
    :
    DevDriver::SettingsBase(&m_settings, sizeof(m_settings)),
    m_pDevice(pDevice),
    m_settings{}
{

}

// =====================================================================================================================
SettingsLoader::~SettingsLoader()
{
}

// =====================================================================================================================
// Initializes the environment settings to their default values
Result SettingsLoader::Init()
{
    DD_RESULT ddResult = SetupDefaultsAndPopulateMap();
    return DdResultToPalResult(ddResult);
}

// =====================================================================================================================
// Setup workarounds for Gfx12
static void SetupWorkarounds(
    const Pal::Device&  device,
    Gfx12PalSettings*   pSettings)
{
    const uint32 familyId = device.ChipProperties().familyId;
    const uint32 eRevId   = device.ChipProperties().eRevId;

    Gfx12SwWarDetection workarounds = {};
    const bool waFound = DetectGfx12SoftwareWorkaroundsByChip(familyId, eRevId, &workarounds);
    PAL_ASSERT(waFound);

#if PAL_ENABLE_PRINTS_ASSERTS
    constexpr uint32 HandledWaMask[] = { 0x1113163F, 0x00000312 }; // Workarounds handled by PAL.
    constexpr uint32 OutsideWaMask[] = { 0xCE2069C0, 0x000000ED }; // Workarounds handled by other components.
    constexpr uint32 MissingWaMask[] = { 0x00CC8000, 0x00000000 }; // Workarounds that should be handled by PAL
                                                                   // that are not yet implemented or are unlikey
                                                                   // to be implemented.
    constexpr uint32 InvalidWaMask[] = { 0x20000000, 0x00000000 }; // Workarounds marked invalid, thus not handled.
    static_assert((sizeof(HandledWaMask) == sizeof(Gfx12InactiveMask)) &&
                  (sizeof(OutsideWaMask) == sizeof(Gfx12InactiveMask)) &&
                  (sizeof(MissingWaMask) == sizeof(Gfx12InactiveMask)) &&
                  (sizeof(InvalidWaMask) == sizeof(Gfx12InactiveMask)),
                  "Workaround Masks do not match expected size!");

        constexpr uint32 InactiveMask[] = {~(HandledWaMask[0] | OutsideWaMask[0] | MissingWaMask[0] | InvalidWaMask[0]),
                                           ~(HandledWaMask[1] | OutsideWaMask[1] | MissingWaMask[1] | InvalidWaMask[1])};
        static_assert(((InactiveMask[0] == Gfx12InactiveMask[0]) && (InactiveMask[1] == Gfx12InactiveMask[1])),
                       "Workaround Masks do not match!");
#endif

    static_assert(Gfx12NumWorkarounds == 42, "Workaround count mismatch between PAL and SWD");

    pSettings->waCsGlgDisableOff =
        workarounds.sioSpiBci12_12412_125GLGWhenSpiGrpLaunchGuaranteeEnable_csGlgDisableIsSetAndGSTriggersGLG_UnexpectedSoftlockMaskIsSetOnHSShader_A_;

    pSettings->waWalkAlign64kScreenSpace = workarounds.ppScIssueWithWALKALIGN8PRIMFITSST_1And64KScreenSpace_A_;

    pSettings->waHiSzRoundMode2 = workarounds.ppScDBD16comp2ndDrawFailedWithFastSet_ZFrom0_50__A_;

    pSettings->waNoDistTessPacketToOnePa = workarounds.geoGeTessOnGESPIGsgrpMismatchDueToSEStateBeingOutOfSync_A_;

    pSettings->waPreventSqgTimingRace = workarounds.shaderSqShaderSqcShaderSqgSQ_SQCAndSQGLegacyPerfCounterUsageIsBrokenWithNewGRBMArch__A_;

    pSettings->waNoOpaqueOreo = workarounds.ppDbudbOreoScoreBoard_udbOsbData_udbOsbdMonitor_ostSampleMaskMismatchOREOScoreboardStoresInvalidEWaveIDAndIncorrectlySetsRespectiveValidBit_A_;

    pSettings->waDrawOpaqueSqNonEvents = workarounds.geoGeDRAWOPAQUERegUpdatesWithin5CyclesOnDifferentContextsCausesGEIssue_A_;

    pSettings->waZSurfaceMismatchWithXorSwizzleBits = workarounds.ppDbMEMDIFFTOOLZSurfaceMismatchWithXorSwizzleBits_A_;

    pSettings->waScpcBackPressure = workarounds.geoGeGeoPaPpScSioPcSioSpiBciSioSxBackPressureFromSCPC_SCSPICanCauseDeadlock_A_;

    PAL_ASSERT(workarounds.ppDbShaderSqSioSpiBciPixelWaitSyncPRECOLORModeLeadsToExportHang_A_ != 0);

    // We assume that this workaround is active.
    // Please look at Gfx12::UniversalCmdBuffer::UpdateDbCountControl().
    PAL_ASSERT(workarounds.ppDbECRRTLFixForConservativeZPASSCounts_A_ != 0);

    // We assume this workaround is active, and we already program all of the impacted registers together.
    PAL_ASSERT(workarounds.geoGeGeoPaUpdateToPHMQProgrammingGuideRelatedToTheProgrammingOfPHRingRegisters_A_ != 0);

    pSettings->waDbForceStencilRead = workarounds.ppDbDataCorruptionDBFailedToMarkCacheValidForFastSetsTiles_A_;

    if (workarounds.ppScIncorrectHiStencilUpdateEquationForSResultsOrCanLeadToImageCorruption__A_)
    {
        pSettings->hiStencilEnable = false;
    }

    if (workarounds.ppDbDBStencilCorruptionDueToMSAA_ZFASTNOOP_StencilFASTSET_A_)
    {
        pSettings->waDbForceStencilValid = true;
    }

    if (workarounds.ppDbTSCEvictionTimeoutCanLeadToSCHangDueToHiZSCacheInflightCountCorruption_A_)
    {
        // The default workaround for the A1 HiZ/S bug is to forcibly disable HiZ/S when draws could potentially trigger
        // the hang.
        // The default currently does not enable the force-ReZ optimization, as performance evaluation is still needed.
        // We also disable the statistical workaround while the disablement workaround is enabled.
        pSettings->waHiZsDisableWhenZsWrite   = true;
        pSettings->forceReZWhenHiZsDisabledWa = false;
        pSettings->waHiZsBopTsEventAfterDraw  = false;
        pSettings->hiZsDbSummarizerTimeouts   = 0;
    }
}

// =====================================================================================================================
// Overrides defaults for the settings based on runtime information.
void SettingsLoader::OverrideDefaults(
    PalSettings* pSettings)
{
    Platform* pPlatform                       = m_pDevice->GetPlatform();
    const PalExperimentsSettings& expSettings = pPlatform->GetExpSettings();

    const Pal::GpuChipProperties& chipProperties = m_pDevice->ChipProperties();

    if (expSettings.expSynchronizationOptimizationOreoModeControl.ValueOr(false))
    {
        m_settings.oreoModeControl = OreoModeBlend;
    }

    SetupWorkarounds(*m_pDevice, &m_settings);

    if (m_settings.waHiSzRoundMode2)
    {
        m_settings.hiDepthRound = 2;
    }

    // Only enable for RS64 FW based GFX11 which adds the support.
    if (chipProperties.pfpUcodeVersion < MinPfpVersionReleaseMemSupportsWaitCpDma)
    {
        m_settings.enableReleaseMemWaitCpDma = false;
    }

}

// =====================================================================================================================
// Validates that the settings structure has legal values. Variables that require complicated initialization can also
// be initialized here.
void SettingsLoader::ValidateSettings(
    PalSettings* pSettings)
{
    const auto& gfx9Props    = m_pDevice->ChipProperties().gfx9;
    auto*       pPalSettings = m_pDevice->GetPublicSettings();

    constexpr uint32 MaxHiZRoundVal = 7;
    m_settings.hiDepthRound = Min(m_settings.hiDepthRound, MaxHiZRoundVal);

    // Gfx12+ supports a maximum of 256 buffers per SE.
    constexpr uint32 MaxOffChipLdsBuffersPerSe = 256;
    const     uint32 maxOffchipLdsBuffers      = MaxOffChipLdsBuffersPerSe * gfx9Props.numShaderEngines;

    if (pSettings->numOffchipLdsBuffers > 0)
    {
        if (m_settings.useMaxOffchipLdsBuffers)
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

    // Vertex Attribute ring buffer must be aligned respect the maximum for the chip
    const uint32 maxAttribRingBufferSizePerSe =
        Pow2AlignDown(VertexAttributeRingMaxSizeBytes / gfx9Props.numShaderEngines,
                      VertexAttributeRingAlignmentBytes);

    m_settings.gfx12VertexAttributesRingBufferSizePerSe =
        Pow2Align(m_settings.gfx12VertexAttributesRingBufferSizePerSe, VertexAttributeRingAlignmentBytes);

    m_settings.gfx12VertexAttributesRingBufferSizePerSe =
        Min(maxAttribRingBufferSizePerSe, m_settings.gfx12VertexAttributesRingBufferSizePerSe);

    m_settings.primBufferRingSizing =
        RoundDownToMultiple(Clamp(m_settings.primBufferRingSizing, 1024u, MaxGePrimRingPrims), 2u);
    m_settings.posBufferRingSizing  =
        RoundDownToMultiple(Clamp(m_settings.posBufferRingSizing,  2048u, MaxGePosRingPos), 2u);

    if (pPalSettings->binningContextStatesPerBin == 0)
    {
        pPalSettings->binningContextStatesPerBin = 1;
    }
    if (pPalSettings->binningPersistentStatesPerBin == 0)
    {
        pPalSettings->binningPersistentStatesPerBin = 1;
    }

    // By default allow PBB to be disabled for PS kill.
    if (pPalSettings->disableBinningPsKill == OverrideMode::Default)
    {
        pPalSettings->disableBinningPsKill = OverrideMode::Enabled;
    }

    // The last step is to update the experiment values:
    Platform* pPlatform                                         = m_pDevice->GetPlatform();
    PalExperimentsSettings* pExpSettings                        = pPlatform->GetExpSettingsPtr();
    pExpSettings->expSynchronizationOptimizationOreoModeControl = (m_settings.oreoModeControl == OreoModeBlend);

    // Validate temporal hints settings
    PAL_ASSERT(m_settings.gfx12TemporalHintsMrtReadBlendReadsDst != Gfx12TemporalHintsReadHonorClient);
    m_settings.gfx12TemporalHintsMrtReadBlendReadsDst =
        (m_settings.gfx12TemporalHintsMrtReadBlendReadsDst == Gfx12TemporalHintsReadHonorClient) ?
        Gfx12TemporalHintsReadNtRt : m_settings.gfx12TemporalHintsMrtReadBlendReadsDst;

    PAL_ASSERT(m_settings.gfx12TemporalHintsMrtWriteBlendReadsDst != Gfx12TemporalHintsWriteHonorClient);
    m_settings.gfx12TemporalHintsMrtWriteBlendReadsDst =
        (m_settings.gfx12TemporalHintsMrtWriteBlendReadsDst == Gfx12TemporalHintsWriteHonorClient) ?
        Gfx12TemporalHintsWriteNtRt : m_settings.gfx12TemporalHintsMrtWriteBlendReadsDst;

    PAL_ASSERT(m_settings.gfx12TemporalHintsMrtReadRaw != Gfx12TemporalHintsReadHonorClient);
    m_settings.gfx12TemporalHintsMrtReadRaw =
        (m_settings.gfx12TemporalHintsMrtReadRaw == Gfx12TemporalHintsReadHonorClient) ?
        Gfx12TemporalHintsReadNtRt : m_settings.gfx12TemporalHintsMrtReadRaw;

    PAL_ASSERT(m_settings.gfx12TemporalHintsMrtWriteRaw != Gfx12TemporalHintsWriteHonorClient);
    m_settings.gfx12TemporalHintsMrtWriteRaw =
        (m_settings.gfx12TemporalHintsMrtWriteRaw == Gfx12TemporalHintsWriteHonorClient) ?
        Gfx12TemporalHintsWriteNtRt : m_settings.gfx12TemporalHintsMrtWriteRaw;

    PAL_ASSERT(m_settings.gfx12TemporalHintsZRead != Gfx12TemporalHintsReadHonorClient);
    m_settings.gfx12TemporalHintsZRead =
        (m_settings.gfx12TemporalHintsZRead == Gfx12TemporalHintsReadHonorClient) ?
        Gfx12TemporalHintsReadNtRt : m_settings.gfx12TemporalHintsZRead;

    PAL_ASSERT(m_settings.gfx12TemporalHintsZWrite != Gfx12TemporalHintsWriteHonorClient);
    m_settings.gfx12TemporalHintsZWrite =
        (m_settings.gfx12TemporalHintsZWrite == Gfx12TemporalHintsWriteHonorClient) ?
        Gfx12TemporalHintsWriteNtRt : m_settings.gfx12TemporalHintsZWrite;

    PAL_ASSERT(m_settings.gfx12TemporalHintsSRead != Gfx12TemporalHintsReadHonorClient);
    m_settings.gfx12TemporalHintsSRead =
        (m_settings.gfx12TemporalHintsSRead == Gfx12TemporalHintsReadHonorClient) ?
        Gfx12TemporalHintsReadNtRt : m_settings.gfx12TemporalHintsSRead;

    PAL_ASSERT(m_settings.gfx12TemporalHintsSWrite != Gfx12TemporalHintsWriteHonorClient);
    m_settings.gfx12TemporalHintsSWrite =
        (m_settings.gfx12TemporalHintsSWrite == Gfx12TemporalHintsWriteHonorClient) ?
        Gfx12TemporalHintsWriteNtRt : m_settings.gfx12TemporalHintsSWrite;

    PAL_ASSERT(m_settings.gfx12TemporalHintsPhqRead != Gfx12TemporalHintsReadHonorClient);
    m_settings.gfx12TemporalHintsPhqRead =
        (m_settings.gfx12TemporalHintsPhqRead == Gfx12TemporalHintsReadHonorClient) ?
        Gfx12TemporalHintsReadLu : m_settings.gfx12TemporalHintsPhqRead;

    PAL_ASSERT(m_settings.gfx12TemporalHintsPhqWrite != Gfx12TemporalHintsWriteHonorClient);
    m_settings.gfx12TemporalHintsPhqWrite =
        (m_settings.gfx12TemporalHintsPhqWrite == Gfx12TemporalHintsWriteHonorClient) ?
        Gfx12TemporalHintsWriteWb : m_settings.gfx12TemporalHintsPhqWrite;

    switch (pPalSettings->hiSZWorkaroundBehavior)
    {
    case HiSZWorkaroundBehavior::ForceDisableAllWar:
        m_settings.waHiZsDisableWhenZsWrite   = false;
        m_settings.waHiZsBopTsEventAfterDraw  = false;
        m_settings.forceReZWhenHiZsDisabledWa = false;
        break;
    case HiSZWorkaroundBehavior::ForceHiSZDisableBasedWar:
        m_settings.waHiZsDisableWhenZsWrite   = true;
        m_settings.waHiZsBopTsEventAfterDraw  = false;
        m_settings.forceReZWhenHiZsDisabledWa = false;
        break;
    case HiSZWorkaroundBehavior::ForceHiSZEventBasedWar:
        m_settings.waHiZsDisableWhenZsWrite   = false;
        m_settings.waHiZsBopTsEventAfterDraw  = true;
        m_settings.forceReZWhenHiZsDisabledWa = false;
        break;
    case HiSZWorkaroundBehavior::ForceHiSZDisableBaseWarWithReZ:
        m_settings.waHiZsDisableWhenZsWrite   = true;
        m_settings.waHiZsBopTsEventAfterDraw  = false;
        m_settings.forceReZWhenHiZsDisabledWa = true;
    case HiSZWorkaroundBehavior::Default:
    default:
        // Default behavior is to listen to the settings, whether they are at their defaults or overridden.
        break;
    }

    // Set up the value for DB_SUMMARIZER_TIMEOUTS.
    // It's only relevant to the BOP_TS Event After Draw workaround.
    if (m_settings.waHiZsBopTsEventAfterDraw)
    {
        // If the setting is 0, then fall back to the public setting override.
        if (m_settings.hiZsDbSummarizerTimeouts == 0)
        {
            // By default for the event-based workaround we want a timeout value of 4k (0xfff).
            // If the client has specified a value in the public settings, use that instead.
            m_settings.hiZsDbSummarizerTimeouts =
                (pPalSettings->tileSummarizerTimeout == 0) ? 0xfff : pPalSettings->tileSummarizerTimeout;
        }
    }
    else
    {
        // Don't need to force it to zero here, zero is the default.
        // If a developer has changed it in the panel, use that value instead for their experiments.
    }
}

// =====================================================================================================================
// The settings hashes are used during pipeline loading to verify that the pipeline data is compatible between when it
// was stored and when it was loaded.
void SettingsLoader::GenerateSettingHash()
{
    MetroHash128::Hash(
        reinterpret_cast<const uint8*>(&m_settings),
        sizeof(Pal::Gfx12::Gfx12PalSettings),
        m_settingsHash.bytes);
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

} // Gfx12
} // Pal
