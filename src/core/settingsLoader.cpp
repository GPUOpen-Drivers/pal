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

#include "core/device.h"
#include "core/platform.h"
#include "core/settingsLoader.h"
#include "palAssert.h"
#include "palInlineFuncs.h"
#include "palSysMemory.h"

#include "core/hw/amdgpu_asic.h"

#include <limits.h>

using namespace Util;

namespace Pal
{

// =====================================================================================================================
// Constructor for the SettingsLoader object.
SettingsLoader::SettingsLoader(
    Device*      pDevice,
    PalSettings* pSettings)
    :
    m_pSettings(pSettings),
    m_pDevice(pDevice),
    m_settingHash()
{

}

#if PAL_ENABLE_PRINTS_ASSERTS
// =====================================================================================================================
// Initialize debug print output mode and enables for each assert level.
void SettingsLoader::InitDpLevelSettings()
{
    bool ret = true;

    struct
    {
        const char*      pRegString;
        DbgPrintCategory palCategory;
    } dbgPrintSettingsTbl[] =
    {
        "Info",  DbgPrintCategory::DbgPrintCatInfoMsg,
        "Warn",  DbgPrintCategory::DbgPrintCatWarnMsg,
        "Error", DbgPrintCategory::DbgPrintCatErrorMsg,
        "ScMsg", DbgPrintCategory::DbgPrintCatScMsg,
        nullptr, DbgPrintCategory::DbgPrintCatCount
    };

    for (uint32 dpTblIdx = 0; dbgPrintSettingsTbl[dpTblIdx].pRegString != nullptr; dpTblIdx++)
    {
        uint32 outputMode;

        // read debug print output mode from registry or config file
        ret = m_pDevice->ReadSetting(dbgPrintSettingsTbl[dpTblIdx].pRegString,
                                     ValueType::Uint,
                                     &outputMode,
                                     InternalSettingScope::PrivatePalKey);

        if (ret == true)
        {
            SetDbgPrintMode(dbgPrintSettingsTbl[dpTblIdx].palCategory,
                            static_cast<DbgPrintMode>(outputMode));
        }
    }

    struct
    {
        const char*    pRegString;
        AssertCategory category;
    } assertSettingsTbl[] =
    {
        "SoftAssert", AssertCategory::AssertCatAlert,
        "HardAssert", AssertCategory::AssertCatAssert,
        nullptr,      AssertCategory::AssertCatCount
    };

    // Read enables from the registry/config file for each AssertLevel...
    for (uint32 idx = 0; assertSettingsTbl[idx].pRegString != nullptr; idx++)
    {
        bool enable;

        ret = m_pDevice->ReadSetting(assertSettingsTbl[idx].pRegString,
                                     ValueType::Boolean,
                                     &enable,
                                     InternalSettingScope::PrivatePalKey);
        if (ret == true)
        {
            EnableAssertMode(assertSettingsTbl[idx].category, (enable == true));
        }
    }
}
#endif // #if PAL_ENABLE_PRINTS_ASSERTS

// =====================================================================================================================
// Initializes the environment settings to their default values
Result SettingsLoader::Init()
{
    Result ret = Result::Success;

#if PAL_ENABLE_PRINTS_ASSERTS
    InitDpLevelSettings();
#endif

    // setup default values for the settings.
    SetupDefaults(m_pSettings);

    // Initialize settings that need to be read early for initialization purposes.
    InitEarlySettings();

    // load the heap performance ratings
    SetupHeapPerfRatings(m_pSettings);

    // initialize HWL settings
    HwlInit();

    // Finally before passing off to the client we need to override the default values
    OverrideDefaults();

    return ret;
}

// =====================================================================================================================
// Initializes settings that need to be read early, necessary for the client before they finalize.
void SettingsLoader::InitEarlySettings()
{
    auto pPublicSettings = m_pDevice->GetPublicSettings();

    m_pDevice->ReadSetting("HwCompositingEnabled",
                           ValueType::Boolean,
                           &m_pSettings->hwCompositingEnabled,
                           InternalSettingScope::PrivatePalKey);

#if PAL_BUILD_DBG_OVERLAY
    m_pDevice->ReadSetting("DebugOverlayEnabled",
                           ValueType::Boolean,
                           &m_pSettings->debugOverlayEnabled,
                           InternalSettingScope::PrivatePalKey);
#endif

#if PAL_BUILD_GPU_PROFILER
    m_pDevice->ReadSetting("GpuProfilerMode",
                           ValueType::Uint,
                           &m_pSettings->gpuProfilerMode,
                           InternalSettingScope::PrivatePalKey);
#endif

#if PAL_BUILD_CMD_BUFFER_LOGGER
    m_pDevice->ReadSetting("CmdBufferLoggerEnabled",
                           ValueType::Boolean,
                           &m_pSettings->cmdBufferLoggerEnabled,
                           InternalSettingScope::PrivatePalKey);
#endif

#if PAL_BUILD_INTERFACE_LOGGER
    m_pDevice->ReadSetting("InterfaceLoggerEnabled",
                           ValueType::Boolean,
                           &m_pSettings->interfaceLoggerEnabled,
                           InternalSettingScope::PrivatePalKey);
#endif
}

// =====================================================================================================================
// Overrides defaults for the settings.
void SettingsLoader::OverrideDefaults()
{
    HwlOverrideDefaults();

    m_pDevice->OverrideDefaultSettings(m_pSettings);

}

// =====================================================================================================================
// Validates that the settings structure has legal values and calls the Hwl method for validation. Variables that
// require complicated initialization can also be initialized here.
void SettingsLoader::ValidateSettings()
{
    // Call the HWL method for validation.
    HwlValidateSettings();

    // If this is set we will use hard-coded heap performance values instead of the usual ASIC-specific values.
    // This setting is intended for bring-up testing as we will return zeros for all performance data on unknown GPUs.
    // This can cause strange behavior (e.g., poor performance) in some Mantle applications.
    if (m_pSettings->forceHeapPerfToFixedValues)
    {
        if (m_pDevice->ChipProperties().gpuType == GpuType::Integrated)
        {
            // If we happen to know we're an APU then use these Carrizo values.
            m_pSettings->cpuWritePerfForLocal         = 3.4f;
            m_pSettings->cpuReadPerfForLocal          = 0.015f;
            m_pSettings->gpuWritePerfForLocal         = 18.f;
            m_pSettings->gpuReadPerfForLocal          = 8.6f;
            m_pSettings->gpuWritePerfForInvisible     = 17.f;
            m_pSettings->gpuReadPerfForInvisible      = 8.5f;
            m_pSettings->cpuWritePerfForGartUswc      = 2.9f;
            m_pSettings->cpuReadPerfForGartUswc       = 0.045f;
            m_pSettings->gpuWritePerfForGartUswc      = 15.f;
            m_pSettings->gpuReadPerfForGartUswc       = 7.5f;
            m_pSettings->cpuWritePerfForGartCacheable = 2.9f;
            m_pSettings->cpuReadPerfForGartCacheable  = 2.f;
            m_pSettings->gpuWritePerfForGartCacheable = 6.5f;
            m_pSettings->gpuReadPerfForGartCacheable  = 3.3f;
        }
        else
        {
            // Otherwise just go with Hawaii data.
            m_pSettings->cpuWritePerfForLocal         = 2.8f;
            m_pSettings->cpuReadPerfForLocal          = 0.0058f;
            m_pSettings->gpuWritePerfForLocal         = 170.f;
            m_pSettings->gpuReadPerfForLocal          = 130.f;
            m_pSettings->gpuWritePerfForInvisible     = 180.f;
            m_pSettings->gpuReadPerfForInvisible      = 130.f;
            m_pSettings->cpuWritePerfForGartUswc      = 3.3f;
            m_pSettings->cpuReadPerfForGartUswc       = 0.1f;
            m_pSettings->gpuWritePerfForGartUswc      = 2.6f;
            m_pSettings->gpuReadPerfForGartUswc       = 2.6f;
            m_pSettings->cpuWritePerfForGartCacheable = 2.9f;
            m_pSettings->cpuReadPerfForGartCacheable  = 3.2f;
            m_pSettings->gpuWritePerfForGartCacheable = 2.6f;
            m_pSettings->gpuReadPerfForGartCacheable  = 2.6f;
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
    else
    {
        // If the number of instances is zero, then the forced spill threshold shouldn't be set at all!
        m_pSettings->forcedUserDataSpillThreshold = USHRT_MAX;
    }
#endif

    // If developer driver profiling is enabled, we should always request the debug vm id and disable mid command
    // buffer preemption support.
    if (m_pDevice->GetPlatform()->IsDevDriverProfilingEnabled())
    {
        m_pSettings->requestDebugVmid = true;
        m_pSettings->cmdBufPreemptionMode = CmdBufPreemptModeDisable;
    }
}

// =====================================================================================================================
// Completes the initialization of the settings by overriding values from the registry and validating the final settings
// struct
void SettingsLoader::FinalizeSettings()
{
    ReadSettings(m_pSettings);
    HwlReadSettings();
    ValidateSettings();
    GenerateSettingHash();
}

} // Pal
