/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/platform.h"
#include "core/platformSettingsLoader.h"
#include "palInlineFuncs.h"
#include "palSysMemory.h"

#include "devDriverServer.h"
#include "protocols/ddSettingsService.h"

using namespace DevDriver::SettingsURIService;

#include <limits.h>

using namespace Util;

namespace Pal
{

// =====================================================================================================================
// Constructor for the PlatformSettingsLoader object.
PlatformSettingsLoader::PlatformSettingsLoader(
    Platform* pPlatform)
    :
    ISettingsLoader(pPlatform, static_cast<DriverSettings*>(&m_settings), g_palPlatformNumSettings),
    m_pPlatform(pPlatform),
    m_settings(),
    m_pComponentName("Pal_Platform")
{
    memset(&m_settings, 0, sizeof(PalPlatformSettings));
}

// =====================================================================================================================
PlatformSettingsLoader::~PlatformSettingsLoader()
{
    auto* pDevDriverServer = m_pPlatform->GetDevDriverServer();
    if (pDevDriverServer != nullptr)
    {
        auto* pSettingsService = pDevDriverServer->GetSettingsService();
        if (pSettingsService != nullptr)
        {
            pSettingsService->UnregisterComponent(m_pComponentName);
        }
    }
}

#if PAL_ENABLE_PRINTS_ASSERTS

static struct DebugPrintSettingsTable
{
    SettingNameHash  hash;
    const char*      pRegString;
    DbgPrintCategory palCategory;
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
    SettingNameHash  hash;
    const char*      pRegString;
    AssertCategory   category;
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
                                   ValueType::Uint,
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
#endif // #if PAL_ENABLE_PRINTS_ASSERTS

// =====================================================================================================================
// Performs required processing to set updated print and assert setting values.
DevDriver::Result PlatformSettingsLoader::PerformSetValue(
    SettingNameHash     hash,
    const SettingValue& settingValue)
{
    DevDriver::Result ret = DevDriver::Result::NotReady;

#if PAL_ENABLE_PRINTS_ASSERTS
    for (uint32 dpTblIdx = 0; DbgPrintSettingsTbl[dpTblIdx].hash != 0; dpTblIdx++)
    {
        if (DbgPrintSettingsTbl[dpTblIdx].hash == hash)
        {
            PAL_ASSERT(settingValue.pValuePtr != nullptr);
            DbgPrintMode* pNewMode = reinterpret_cast<DbgPrintMode*>(settingValue.pValuePtr);
            SetDbgPrintMode(DbgPrintSettingsTbl[dpTblIdx].palCategory, *pNewMode);
            ret = DevDriver::Result::Success;
        }
    }

    for (uint32 idx = 0; AssertSettingsTbl[idx].hash != 0; idx++)
    {
        if (AssertSettingsTbl[idx].hash == hash)
        {
            PAL_ASSERT(settingValue.pValuePtr != nullptr);
            bool* pEnable = reinterpret_cast<bool*>(settingValue.pValuePtr);
            EnableAssertMode(AssertSettingsTbl[idx].category, *pEnable);
            ret = DevDriver::Result::Success;
        }
    }
#endif

#if PAL_BUILD_CMD_BUFFER_LOGGER
    constexpr uint32 CmdBufferLoggerSingleStepHash = 1570291248;
    if (hash == CmdBufferLoggerSingleStepHash)
    {
        auto* pInfo = m_settingsInfoMap.FindKey(hash);
        if (pInfo != nullptr)
        {
            // If the user has requested any of the WaitIdle flags for the CmdBufferLogger,
            // we must enable the corresponding timestamp value as well.
            constexpr uint32 WaitIdleMask   = 0x3E0;
            constexpr uint32 WaitIdleOffset = 5;
            PAL_ASSERT(settingValue.pValuePtr != nullptr);
            uint32 value = *static_cast<uint32*>(settingValue.pValuePtr);

            if (TestAnyFlagSet(value, WaitIdleMask))
            {
                value |= (value >> WaitIdleOffset);
                memcpy(pInfo->pValuePtr, &value, settingValue.valueSize);
                ret = DevDriver::Result::Success;
            }
        }
    }
#endif

    return ret;
}

// =====================================================================================================================
// Initializes the settings structure, setting default values, reading overrides and registering with the developer mode
// (if available).
Result PlatformSettingsLoader::Init()
{
    Result ret = m_settingsInfoMap.Init();

    if (ret == Result::Success)
    {
        // Init Settings Info HashMap
        InitSettingsInfo();

        // setup default values for the settings.
        SetupDefaults();

        m_state = SettingsLoaderState::EarlyInit;

        // Register with the DevDriver settings service
        DevDriverRegister();
    }

    return ret;
}

// =====================================================================================================================
// Overrides defaults for the settings based on runtime information.
void PlatformSettingsLoader::OverrideDefaults()
{
    // There are no current overrides for platform settings, just update our state.

    m_state = SettingsLoaderState::LateInit;
}

// =====================================================================================================================
// Validates that the settings structure has legal values. Variables that require complicated initialization can also be
// initialized here.
void PlatformSettingsLoader::ValidateSettings()
{
#if PAL_BUILD_CMD_BUFFER_LOGGER
    if (m_settings.cmdBufferLoggerConfig.cmdBufferLoggerSingleStep != 0)
    {
        constexpr uint32 WaitIdleMask   = 0x3E0;
        constexpr uint32 WaitIdleOffset = 5;

        uint32* pSetting = &m_settings.cmdBufferLoggerConfig.cmdBufferLoggerSingleStep;
        if (TestAnyFlagSet(*pSetting, WaitIdleMask))
        {
            *pSetting |= (*pSetting >> WaitIdleOffset);
        }
    }
#endif

    m_state = SettingsLoaderState::Final;
}

} // Pal
