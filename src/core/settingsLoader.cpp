/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

using namespace DevDriver::SettingsURIService;

#include <limits.h>

using namespace Util;

namespace Pal
{

// =====================================================================================================================
// Constructor for the SettingsLoader object.
SettingsLoader::SettingsLoader(
    IndirectAllocator* pAllocator,
    Device*            pDevice)
    :
    ISettingsLoader(pAllocator, static_cast<DriverSettings*>(&m_settings), g_palNumSettings),
    m_pDevice(pDevice),
    m_settings(),
    m_pComponentName("Pal")
{
    memset(&m_settings, 0, sizeof(PalSettings));
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

        // load the heap performance ratings
        SetupHeapPerfRatings(&m_settings);

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
// Overrides defaults for the settings based on runtime information.
void SettingsLoader::OverrideDefaults()
{
    m_pDevice->OverrideDefaultSettings(&m_settings);

    m_state = SettingsLoaderState::LateInit;
}

// =====================================================================================================================
// Validates that the settings structure has legal values and calls the Hwl method for validation. Variables that
// require complicated initialization can also be initialized here.
void SettingsLoader::ValidateSettings()
{
    m_pDevice->GetGfxDevice()->HwlValidateSettings(&m_settings);
    // If this is set we will use hard-coded heap performance values instead of the usual ASIC-specific values.
    // This setting is intended for bring-up testing as we will return zeros for all performance data on unknown GPUs.
    // This can cause strange behavior (e.g., poor performance) in some Mantle applications.
    if (m_settings.forceHeapPerfToFixedValues)
    {
        if (m_pDevice->ChipProperties().gpuType == GpuType::Integrated)
        {
            // If we happen to know we're an APU then use these Carrizo values.
            m_settings.cpuWritePerfForLocal         = 3.4f;
            m_settings.cpuReadPerfForLocal          = 0.015f;
            m_settings.gpuWritePerfForLocal         = 18.f;
            m_settings.gpuReadPerfForLocal          = 8.6f;
            m_settings.gpuWritePerfForInvisible     = 17.f;
            m_settings.gpuReadPerfForInvisible      = 8.5f;
            m_settings.cpuWritePerfForGartUswc      = 2.9f;
            m_settings.cpuReadPerfForGartUswc       = 0.045f;
            m_settings.gpuWritePerfForGartUswc      = 15.f;
            m_settings.gpuReadPerfForGartUswc       = 7.5f;
            m_settings.cpuWritePerfForGartCacheable = 2.9f;
            m_settings.cpuReadPerfForGartCacheable  = 2.f;
            m_settings.gpuWritePerfForGartCacheable = 6.5f;
            m_settings.gpuReadPerfForGartCacheable  = 3.3f;
        }
        else
        {
            // Otherwise just go with Hawaii data.
            m_settings.cpuWritePerfForLocal         = 2.8f;
            m_settings.cpuReadPerfForLocal          = 0.0058f;
            m_settings.gpuWritePerfForLocal         = 170.f;
            m_settings.gpuReadPerfForLocal          = 130.f;
            m_settings.gpuWritePerfForInvisible     = 180.f;
            m_settings.gpuReadPerfForInvisible      = 130.f;
            m_settings.cpuWritePerfForGartUswc      = 3.3f;
            m_settings.cpuReadPerfForGartUswc       = 0.1f;
            m_settings.gpuWritePerfForGartUswc      = 2.6f;
            m_settings.gpuReadPerfForGartUswc       = 2.6f;
            m_settings.cpuWritePerfForGartCacheable = 2.9f;
            m_settings.cpuReadPerfForGartCacheable  = 3.2f;
            m_settings.gpuWritePerfForGartCacheable = 2.6f;
            m_settings.gpuReadPerfForGartCacheable  = 2.6f;
        }
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 403
    const auto pPalSettings = m_pDevice->GetPublicSettings();
    if (pPalSettings->userDataSpillTableRingSize != 0)
    {
        // The number of instances of the user-data spill table in each universal command buffer's ring must be
        // either zero, or a multiple of four.
        pPalSettings->userDataSpillTableRingSize = RoundUpToMultiple(pPalSettings->userDataSpillTableRingSize, 4u);
    }
#endif

    // If developer driver profiling is enabled, we should always request the debug vm id and disable mid command
    // buffer preemption support.
    if (m_pDevice->GetPlatform()->IsDevDriverProfilingEnabled())
    {
        m_settings.requestDebugVmid = true;
        m_settings.cmdBufPreemptionMode = CmdBufPreemptModeDisable;
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

        Strncpy(subDir, m_settings.pipelineLogConfig.pipelineLogDirectory, sizeof(subDir));
        Snprintf(m_settings.pipelineLogConfig.pipelineLogDirectory,
                 sizeof(m_settings.pipelineLogConfig.pipelineLogDirectory),
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
    uint32 catalystAI = m_settings.catalystAI;
    m_settings.catalystAI = 0;

    MetroHash128::Hash(
        reinterpret_cast<const uint8*>(&m_settings),
        sizeof(PalSettings),
        m_settingHash.bytes);

    m_settings.textureOptLevel = textureOptLevel;
    m_settings.catalystAI = catalystAI;
}

} // Pal
