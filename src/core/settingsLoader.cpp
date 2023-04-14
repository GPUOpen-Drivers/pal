/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/platform.h"
#include "core/device.h"
#include "core/settingsLoader.h"
#include "palAssert.h"
#include "palInlineFuncs.h"
#include "palSysMemory.h"

#include "core/hw/amdgpu_asic.h"
#include "devDriverServer.h"
#include "protocols/ddSettingsService.h"
#include "settingsService.h"

using namespace DevDriver::SettingsURIService;

#include <limits.h>

using namespace Util;

namespace Pal
{

// =====================================================================================================================
// Constructor for the SettingsLoader object.
SettingsLoader::SettingsLoader(
    Device* pDevice)
    :
    ISettingsLoader(pDevice->GetPlatform(), static_cast<DriverSettings*>(&m_settings), g_palNumSettings),
    m_pDevice(pDevice),
    m_settings(),
    m_pComponentName("Pal")
{
    memset(&m_settings, 0, sizeof(PalSettings));
}

// =====================================================================================================================
SettingsLoader::~SettingsLoader()
{
    SettingsRpcService::SettingsService* pRpcSettingsService = m_pDevice->GetPlatform()->GetSettingsService();
    if (pRpcSettingsService != nullptr)
    {
        pRpcSettingsService->UnregisterComponent(m_pComponentName);
    }

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
// Initializes the environment settings to their default values
Result SettingsLoader::Init()
{
    Result ret = m_settingsInfoMap.Init();

    if (ret == Result::Success)
    {
        // Init Settings Info HashMap
        InitSettingsInfo();

        // setup default values for the settings.
        SetupDefaults();

        m_state = SettingsLoaderState::EarlyInit;

        // Read the rest of the settings from the registry
        ReadSettings();

        // Register with the DevDriver settings service
        DevDriverRegister();

        // We want to override the default values for any platform specific reasons
        OverrideDefaults();

        // Before we pass the settings to the client, perform a reread of any settings that need rereading
        RereadSettings();
    }

    return ret;
}

// =====================================================================================================================
bool SettingsLoader::ReadSetting(
    const char*          pSettingName,
    void*                pValue,
    Util::ValueType      valueType,
    size_t               bufferSize,
    InternalSettingScope settingType)
{
    return m_pDevice->ReadSetting(
        pSettingName,
        valueType,
        pValue,
        InternalSettingScope::PrivatePalKey,
        bufferSize);
}

// =====================================================================================================================
// Helper function for checking the CP microcode version for Gfx9.
static bool CheckGfx9CpUcodeVersion(
    const Device& device,
    uint32        pfpUcodeVersion)
{
    return (IsGfx9(device) && (device.ChipProperties().pfpUcodeVersion >= pfpUcodeVersion));
}

// =====================================================================================================================
// Helper function for checking the CP microcode version for Gfx10.1.
static bool CheckGfx101CpUcodeVersion(
    const Device& device,
    uint32        pfpUcodeVersion)
{
    return (IsGfx101(device) && (device.ChipProperties().pfpUcodeVersion >= pfpUcodeVersion));
}

// =====================================================================================================================
// Helper function for checking the CP microcode version for Gfx10.3.
static bool CheckGfx103CpUcodeVersion(
    const Device& device,
    uint32        pfpUcodeVersion)
{
    return (IsGfx103(device) && (device.ChipProperties().pfpUcodeVersion >= pfpUcodeVersion));
}

#if PAL_BUILD_GFX11
// =====================================================================================================================
// Helper function for checking the CP microcode version for Gfx11.
static bool CheckGfx11CpUcodeVersion(
    const Device& device,
    uint32        pfpUcodeVersionF32,
    uint32        pfpUcodeVersionRs64)
{
    constexpr uint32 Rs64VersionStart   = 300;
    uint32 deviceUcodeVersion = device.ChipProperties().pfpUcodeVersion;

    PAL_ASSERT((pfpUcodeVersionF32 < Rs64VersionStart) && (pfpUcodeVersionRs64 >= Rs64VersionStart));

    bool featureSupported = false;

    if (deviceUcodeVersion > 1000000)
    {
        deviceUcodeVersion = 413;
    }

    if (IsGfx11(device))
    {
        featureSupported = (deviceUcodeVersion < Rs64VersionStart) ? (deviceUcodeVersion >= pfpUcodeVersionF32) :
                                                                     (deviceUcodeVersion >= pfpUcodeVersionRs64);
    }

    return featureSupported;
}
#endif

// =====================================================================================================================
// Overrides defaults for the settings based on runtime information.
void SettingsLoader::OverrideDefaults()
{
    m_pDevice->OverrideDefaultSettings(&m_settings);

    const GfxIpLevel gfxLevel = m_pDevice->ChipProperties().gfxLevel;

    if (gfxLevel >= GfxIpLevel::GfxIp9)
    {
        // Setup default DCC enables.
        constexpr uint32 DccEnables = UseDccSingleSample          |
                                      UseDccSrgb                  |
                                      UseDccNonTcCompatShaderRead |
                                      UseDccPrt                   |
                                      UseDccMultiSample2x         |
                                      UseDccMultiSample4x         |
                                      UseDccMultiSample8x         |
                                      UseDccEqaa                  |
                                      UseDccAllowForceEnable      |
                                      UseDccMipMappedArrays;
        m_settings.useDcc = DccEnables;

#if PAL_BUILD_GFX11
        if (IsGfx11(gfxLevel))
        {
            m_settings.useDcc |= UseDccForAllCompatibleFormats;
        }
#endif
    }
    else if (gfxLevel >= GfxIpLevel::GfxIp8)
    {
        // Setup default DCC enables.
        constexpr uint32 DccEnables = UseDccSingleSample          |
                                      UseDccSrgb                  |
                                      UseDccMultiSample2x         |
                                      UseDccMultiSample4x         |
                                      UseDccMultiSample8x         |
                                      UseDccEqaa;
        m_settings.useDcc = DccEnables;
    }

    if (gfxLevel >= GfxIpLevel::GfxIp9)
    {
        constexpr uint32 PfpUcodeVersionNativeExecuteIndirectGfx9    = 192;
        constexpr uint32 PfpUcodeVersionNativeExecuteIndirectGfx10_1 = 151;
        constexpr uint32 PfpUcodeVersionNativeExecuteIndirectGfx10_3 = 88;

        if (IsRaphael(*m_pDevice) == false)
        {
            if (CheckGfx9CpUcodeVersion(*m_pDevice,   PfpUcodeVersionNativeExecuteIndirectGfx9)    ||
                CheckGfx101CpUcodeVersion(*m_pDevice, PfpUcodeVersionNativeExecuteIndirectGfx10_1) ||
                CheckGfx103CpUcodeVersion(*m_pDevice, PfpUcodeVersionNativeExecuteIndirectGfx10_3))
            {
                m_settings.useExecuteIndirectPacket = UseExecuteIndirectPacketForDraw;
            }

            constexpr uint32 PfpUcodeVersionSpillTableSupportedExecuteIndirectGfx10_1 = 155;
            constexpr uint32 PfpUcodeVersionSpillTableSupportedExecuteIndirectGfx10_3 = 91;

            if (CheckGfx101CpUcodeVersion(*m_pDevice, PfpUcodeVersionSpillTableSupportedExecuteIndirectGfx10_1) ||
                CheckGfx103CpUcodeVersion(*m_pDevice, PfpUcodeVersionSpillTableSupportedExecuteIndirectGfx10_3))
            {
                m_settings.useExecuteIndirectPacket = UseExecuteIndirectPacketForDrawSpillTable;
            }

            constexpr uint32 PfpUcodeVersionVbTableSupportedExecuteIndirectGfx10_1   = 155;
            constexpr uint32 PfpUcodeVersionVbTableSupportedExecuteIndirectGfx10_3   = 95;

#if PAL_BUILD_GFX11
            constexpr uint32 PfpUcodeVersionVbTableSupportedExecuteIndirectGfx11F32  = 84;
            constexpr uint32 PfpUcodeVersionVbTableSupportedExecuteIndirectGfx11Rs64 = 413;
#endif

            if (CheckGfx101CpUcodeVersion(*m_pDevice, PfpUcodeVersionVbTableSupportedExecuteIndirectGfx10_1) ||
                CheckGfx103CpUcodeVersion(*m_pDevice, PfpUcodeVersionVbTableSupportedExecuteIndirectGfx10_3)
#if PAL_BUILD_GFX11
                || CheckGfx11CpUcodeVersion(
                    *m_pDevice,
                    PfpUcodeVersionVbTableSupportedExecuteIndirectGfx11F32,
                    PfpUcodeVersionVbTableSupportedExecuteIndirectGfx11Rs64)
#endif
                )
            {
                m_settings.useExecuteIndirectPacket = UseExecuteIndirectPacketForDrawSpillAndVbTable;
            }
        }
    }

    if (m_pDevice->PhysicalEnginesAvailable())
    {
        // prevent exhausting invisible video memory due to excessive physical alignment for small allocations
        m_settings.enableUswcHeapAllAllocations = true;
    }

    // Since APUs don't have real local memory it's better to use a GART heap instead
    // of allocating out of the limited carveout space.
    if (m_pDevice->ChipProperties().gpuType == GpuType::Integrated)
    {
        m_settings.preferredPipelineUploadHeap = PipelineHeapGartUswc;
    }

    if (IsNavi2x(*m_pDevice)
       )
    {
        m_settings.addr2UseVarSwizzleMode = Addr2UseVarSwizzle::Addr2UseVarSwizzleDisable;
    }

    if ((m_pDevice->ChipProperties().vcnLevel > VcnIpLevel::VcnIp1)
        )
    {

        m_settings.waForceLinearHeight16Alignment = true;
    }

    if (m_pDevice->IsSpoofed())
    {
        // Sending commands intended for a spoofed GPU model to the different physical device is
        // almost certain to hang the device.
        m_settings.ifh = IfhModeKmd;
    }

    m_state = SettingsLoaderState::LateInit;
}

// =====================================================================================================================
// Validates that the settings structure has legal values and calls the Hwl method for validation. Variables that
// require complicated initialization can also be initialized here.
void SettingsLoader::ValidateSettings()
{
    m_pDevice->GetGfxDevice()->HwlValidateSettings(&m_settings);

    // If developer driver profiling is enabled, we should always request the debug-vmid/static-vmid and disable mid
    // command buffer preemption support.
    //
    // CrashAnalysis feature requires disablement of command buffer preemption, as well as DebugVmid.
    if (m_pDevice->GetPlatform()->IsDevDriverProfilingEnabled() ||
        m_pDevice->GetPlatform()->IsCrashAnalysisModeEnabled())
    {
        m_settings.requestDebugVmid     = true;
        m_settings.cmdBufPreemptionMode = CmdBufPreemptModeDisable;
    }

    // When tracing is enabled, we need to request debug/static VMID. This can be enabled via the DriverUtilsService.
    if (m_pDevice->GetPlatform()->IsTracingEnabled())
    {
        m_settings.requestDebugVmid = true;
    }

    // Emulated devices may not be visible to the host OS, so use CPU paths to send data to the window system.
    if (m_pDevice->GetPlatform()->IsEmulationEnabled())
    {
        m_settings.forcePresentViaCpuBlt = true;
    }

    // Overrides all paths for debug files to expected values.
    // Now those directories in setting are all *relative*:
    // Relative to the path in the AMD_DEBUG_DIR environment variable, and if that env var isn't set, the location is
    // platform dependent. So we need to query the root path from device and then concatenate two strings (of the root
    // path and relative path of specific file) to the final usable absolute path
    const char* pRootPath = m_pDevice->GetDebugFilePath();
    auto* pPlatformSettings = m_pDevice->GetPlatform()->PlatformSettingsPtr();
    if (pRootPath != nullptr)
    {
        char subDir[MaxPathStrLen];

        Strncpy(subDir, m_settings.cmdBufDumpDirectory, sizeof(subDir));
        Snprintf(m_settings.cmdBufDumpDirectory, sizeof(m_settings.cmdBufDumpDirectory),
                 "%s/%s", pRootPath, subDir);

        Strncpy(subDir, m_settings.pipelineElfLogConfig.logDirectory, sizeof(subDir));
        Snprintf(m_settings.pipelineElfLogConfig.logDirectory,
                 sizeof(m_settings.pipelineElfLogConfig.logDirectory),
                 "%s/%s", pRootPath, subDir);

        Strncpy(subDir, pPlatformSettings->overlayBenchmarkConfig.usageLogDirectory, sizeof(subDir));
        Snprintf(pPlatformSettings->overlayBenchmarkConfig.usageLogDirectory,
                 sizeof(pPlatformSettings->overlayBenchmarkConfig.usageLogDirectory),
                 "%s/%s", pRootPath, subDir);

        Strncpy(subDir, pPlatformSettings->overlayBenchmarkConfig.frameStatsLogDirectory, sizeof(subDir));
        Snprintf(pPlatformSettings->overlayBenchmarkConfig.frameStatsLogDirectory,
                 sizeof(pPlatformSettings->overlayBenchmarkConfig.frameStatsLogDirectory),
                 "%s/%s", pRootPath, subDir);

        Strncpy(subDir, pPlatformSettings->gpuProfilerConfig.logDirectory, sizeof(subDir));
        Snprintf(pPlatformSettings->gpuProfilerConfig.logDirectory,
                 sizeof(pPlatformSettings->gpuProfilerConfig.logDirectory),
                 "%s/%s", pRootPath, subDir);

        Strncpy(subDir, pPlatformSettings->interfaceLoggerConfig.logDirectory, sizeof(subDir));
        Snprintf(pPlatformSettings->interfaceLoggerConfig.logDirectory,
                 sizeof(pPlatformSettings->interfaceLoggerConfig.logDirectory),
                 "%s/%s", pRootPath, subDir);

        Strncpy(subDir, pPlatformSettings->pm4InstrumentorConfig.logDirectory, sizeof(subDir));
        Snprintf(pPlatformSettings->pm4InstrumentorConfig.logDirectory,
                 sizeof(pPlatformSettings->pm4InstrumentorConfig.logDirectory),
                 "%s/%s", pRootPath, subDir);

#if PAL_DEVELOPER_BUILD
        Strncpy(subDir, pPlatformSettings->gpuDebugConfig.surfaceCaptureLogDirectory, sizeof(subDir));
        Snprintf(pPlatformSettings->gpuDebugConfig.surfaceCaptureLogDirectory,
                 sizeof(pPlatformSettings->gpuDebugConfig.surfaceCaptureLogDirectory),
                 "%s/%s", pRootPath, subDir);
#endif

    }

    m_state = SettingsLoaderState::Final;
}

// =====================================================================================================================
// Completes the initialization of the settings by overriding values from the registry and validating the final settings
// struct
void SettingsLoader::FinalizeSettings()
{
    ValidateSettings();
    GenerateSettingHash();
}

// =====================================================================================================================
// The settings hashes are used during pipeline loading to verify that the pipeline data is compatible between when it
// was stored and when it was loaded.  The CCC controls some of the settings though, and the CCC doesn't set it
// identically across all GPUs in an MGPU configuration.  Since the CCC keys don't affect pipeline generation, just
// ignore those values when it comes to hash generation.
void SettingsLoader::GenerateSettingHash()
{
    // Temporarily ignore these CCC settings when computing a settings hash as described in the function header.
    uint32 textureOptLevel = m_settings.textureOptLevel;
    m_settings.textureOptLevel = 0;

    MetroHash128::Hash(
        reinterpret_cast<const uint8*>(&m_settings),
        sizeof(PalSettings),
        m_settingHash.bytes);

    m_settings.textureOptLevel = textureOptLevel;
}
} // Pal
