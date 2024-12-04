/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/platformSettingsLoader.h"
#include "core/devDriverUtil.h"
#include "palDbgPrint.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
// Constructor for the PlatformSettingsLoader object.
PlatformSettingsLoader::PlatformSettingsLoader(
    Platform* pPlatform)
    :
    DevDriver::SettingsBase(&m_settings, sizeof(m_settings)),
    m_pPlatform(pPlatform)
{
}

// =====================================================================================================================
PlatformSettingsLoader::~PlatformSettingsLoader()
{
}

#if PAL_ENABLE_PRINTS_ASSERTS

static struct DebugPrintSettingsTable
{
    DD_SETTINGS_NAME_HASH  hash;
    const char*            pRegString;
    DbgPrintCategory       palCategory;
} DbgPrintSettingsTbl[] =
{
    3336086055, "Info",    DbgPrintCategory::DbgPrintCatInfoMsg,
    3827375483, "Warn",    DbgPrintCategory::DbgPrintCatWarnMsg,
    1444311189, "Error",   DbgPrintCategory::DbgPrintCatErrorMsg,
    695309361,  "ScMsg",   DbgPrintCategory::DbgPrintCatScMsg,
    721345714,  "Event",   DbgPrintCategory::DbgPrintCatEventPrintMsg,
    4220374213, "EventCb", DbgPrintCategory::DbgPrintCatEventPrintCallbackMsg,
    0,           nullptr,  DbgPrintCategory::DbgPrintCatCount
};

static struct
{
    DD_SETTINGS_NAME_HASH hash;
    const char*           pRegString;
    AssertCategory        category;
} AssertSettingsTbl[] =
{
    3333004859, "SoftAssert", AssertCategory::AssertCatAlert,
    1110605001, "HardAssert", AssertCategory::AssertCatAssert,
    0,           nullptr,     AssertCategory::AssertCatCount
};

// =====================================================================================================================
// Initialize debug print output mode and enables for each assert level.
void PlatformSettingsLoader::ReadAssertAndPrintSettings(
    Pal::Device* pDevice)
{
    bool ret = true;

    for (uint32 dpTblIdx = 0; DbgPrintSettingsTbl[dpTblIdx].pRegString != nullptr; dpTblIdx++)
    {
        uint32 outputMode;

        // read debug print output mode from registry or config file
        ret = pDevice->ReadSetting(DbgPrintSettingsTbl[dpTblIdx].pRegString,
                                   ValueType::Uint32,
                                   &outputMode,
                                   InternalSettingScope::PrivatePalKey);

        if (ret == true)
        {
            SetDbgPrintMode(DbgPrintSettingsTbl[dpTblIdx].palCategory,
                            static_cast<DbgPrintMode>(outputMode));
        }
    }

    // Read enables from the registry/config file for each AssertLevel...
    for (uint32 idx = 0; AssertSettingsTbl[idx].pRegString != nullptr; idx++)
    {
        bool enable;

        ret = pDevice->ReadSetting(AssertSettingsTbl[idx].pRegString,
                                   ValueType::Boolean,
                                   &enable,
                                   InternalSettingScope::PrivatePalKey);
        if (ret == true)
        {
            EnableAssertMode(AssertSettingsTbl[idx].category, (enable == true));
        }
    }
}

#endif

// =====================================================================================================================
// Initializes the settings structure, setting default values.
Result PlatformSettingsLoader::Init()
{
    DD_RESULT ddResult = SetupDefaultsAndPopulateMap();
    return DdResultToPalResult(ddResult);
}

// =====================================================================================================================
// Overrides defaults for the settings based on runtime information.
void PlatformSettingsLoader::OverrideDefaults()
{
    // There are no current overrides for platform settings.
}

// =====================================================================================================================
// Validates that the settings structure has legal values. Variables that require complicated initialization can also be
// initialized here.
void PlatformSettingsLoader::ValidateSettings(
    bool hasDdUserOverride)
{
#if PAL_ENABLE_PRINTS_ASSERTS
    if (hasDdUserOverride)
    {
        // For DXCPanel, VkPanel, etc, the print-assert related settings are not part of any settings component, instead
        // they are hard-coded in the panel code and are read directly in ReadAssertAndPrintSettings() by names. For
        // DevDriver settings to achieve parity, the following settings are added to Platform settings component. They
        // can only be updated via DevDriver network.
        //
        // To avoid accidentally override settings read in ReadAssertAndPrintSettings(), we only set print and assert
        // here when we know developers are using DevDriver settings panel. `hasDdUserOverride` check should be removed
        // once DXCPanel and the like are deprecated and removed.
        EnableAssertMode(AssertCatAlert, m_settings.enableSoftAssert);
        EnableAssertMode(AssertCatAssert, m_settings.enableHardAssert);

        SetDbgPrintMode(DbgPrintCatInfoMsg, static_cast<DbgPrintMode>(m_settings.dbgPrintInfoMode));
        SetDbgPrintMode(DbgPrintCatWarnMsg, static_cast<DbgPrintMode>(m_settings.dbgPrintWarnMode));
        SetDbgPrintMode(DbgPrintCatErrorMsg, static_cast<DbgPrintMode>(m_settings.dbgPrintErrorMode));
        SetDbgPrintMode(DbgPrintCatScMsg, static_cast<DbgPrintMode>(m_settings.dbgPrintScMsgMode));
        SetDbgPrintMode(DbgPrintCatEventPrintMsg, static_cast<DbgPrintMode>(m_settings.dbgPrintEventMode));
        SetDbgPrintMode(DbgPrintCatEventPrintCallbackMsg, static_cast<DbgPrintMode>(m_settings.dbgPrintEventCallbackMode));
    }
#endif

#if PAL_ENABLE_LOGGING
    // Overrides debug log directory path to expected value.
    // Now those directories in setting are all *relative*:
    // Relative to the path in the AMD_DEBUG_DIR environment variable, and if that env var isn't set, the location is
    // platform dependent. So we need to query the root path from device and then concatenate two strings (of the root
    // path and relative path of specific file) to the final usable absolute path.
    PAL_ASSERT(m_pPlatform != nullptr);
    Device* pDevice = m_pPlatform->GetDevice(0);
    PAL_ASSERT(pDevice != nullptr);

    const char* pRootPath = pDevice->GetDebugFilePath();
    char subDir[MaxPathStrLen];

    Strncpy(subDir, m_settings.dbgLoggerFileConfig.logDirectory, sizeof(subDir));
    Snprintf(m_settings.dbgLoggerFileConfig.logDirectory, sizeof(m_settings.dbgLoggerFileConfig.logDirectory),
             "%s/%s", pRootPath, subDir);
#endif

#if PAL_DEVELOPER_BUILD
    if (m_settings.cmdBufferLoggerConfig.embedDrawDispatchInfo)
    {
        // Annotations is unsupported while embedding the draw/dispatch info for external tooling.
        m_settings.cmdBufferLoggerConfig.cmdBufferLoggerAnnotations = 0x0;
    }
#endif

    // Early evaluation of target application to ensure downstream effects are global
    if (strlen(m_settings.gpuProfilerConfig.targetApplication) > 0)
    {
        char  executableNameBuffer[256] = {};
        char* pExecutableName = nullptr;

        if (GetExecutableName(executableNameBuffer, &pExecutableName, sizeof(executableNameBuffer)) == Result::Success)
        {
            if (strcmp(pExecutableName, m_settings.gpuProfilerConfig.targetApplication) != 0)
            {
                m_settings.gpuProfilerMode = GpuProfilerDisabled;
            }
        }
        else
        {
            PAL_ASSERT_ALWAYS_MSG("Unable to retrieve executable name to match against the Gpu Profiler target "
                "application name.");
        }
    }
}

// =====================================================================================================================
bool PlatformSettingsLoader::ReadSetting(
    const char*          pSettingName,
    ValueType            valueType,
    void*                pValue,
    InternalSettingScope settingType,
    size_t               bufferSize)
{
    PAL_ASSERT(m_pPlatform != nullptr);
    Device* pDevice = m_pPlatform->GetDevice(0);
    PAL_ASSERT(pDevice != nullptr);

    return pDevice->ReadSetting(
        pSettingName,
        valueType,
        pValue,
        settingType,
        bufferSize);
}

} // Pal
