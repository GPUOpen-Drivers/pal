/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include <stdint.h>
#include <ddApi.h>
#include <ddDefs.h>
#include <settingsService.h>
#include <util/hashMap.h>
#include <protocols/ddSettingsServiceTypes.h>
#include "settingsConfig.h"
#include "settingsTypes.h"

namespace DevDriver
{

/// The base struct for storing settings data. Settings of different components
/// should store their settings in a struct that derives from this one.
struct SettingsData
{
    uint32 numSettings;
};

/// The base class for Settings. Settings of different components should each
/// derive from this class and implement its virtual functions.
class SettingsBase
{
    using SettingValue = SettingsURIService::SettingValue;

public:
    SettingsBase(SettingsData* pSettings, uint32 numSettings, size_t settingsBytes)
        : m_pSettingsData(pSettings)
        , m_settingValueRefsMap(DevDriver::Platform::GenericAllocCb)
    {
        // Zero out the entire SettingsData. This ensure the struct paddings
        // are always zero, and is required for generating deterministic hashing
        // result.
        memset(m_pSettingsData, 0, settingsBytes);
        m_pSettingsData->numSettings = numSettings;
    }

    virtual ~SettingsBase() {}

    virtual DD_RESULT Init(const char* pUserOverridesFilePath) = 0;

    MetroHash::Hash GetSettingsHash() const { return m_settingsHash; }

    static Result GetValue(
        uint32_t        hash,
        SettingValue*   pOutSettingValue,
        void*           pPrivateData);

    static Result SetValue(
        uint32_t            hash,
        const SettingValue& settingValue,
        void*               pPrivateData);

protected:

    /// This function is called in the static SetValue implementation, it is used
    /// to perform any complex processing related to setting the value of a
    /// particular setting. If this function returns NotReady it indicates the
    /// SetValue request was NOT handled and the default memcpy of the setting
    /// value will be performed.  Success indicates the value was successfully
    /// updated, other error codes describe failures e.g. invalid parameters.
    virtual Result PerformSetValue(
        uint32_t            hash,
        const SettingValue& settingValue)
    {
        DD_UNUSED(hash);
        DD_UNUSED(settingValue);
        // Default implementation assumes no action needed, simply returns NotReady
        return DevDriver::Result::NotReady;
    }

    DD_RESULT LoadUserOverridesFile(const char* pFilepath)
    {
        return m_useroverrides.Load(pFilepath);
    }

    /// Apply user-overrides of a specific component.
    /// Return SUCCESS:
    ///     1) The specified component is not found.
    ///     2) The specified component found but doesn't contain any user-overrides.
    ///     3) All user-overrides in the specified component are applied.
    /// Return SUCCESS_WITH_ERROR:
    ///     Some but not all user-overrides fail to be applied.
    /// Return other errors:
    ///     All other cases.
    DD_RESULT ApplyUserOverridesByComponent(const char* pComponentName);

    // auto-generated functions
    virtual void InitSettingsInfo() = 0;
    virtual void SetupDefaults() = 0;
    virtual void DevDriverRegister(SettingsRpcService::SettingsService* pSettingsService) = 0;

// ============================================================================
// Member variables
private:
    SettingsData* m_pSettingsData;
    MetroHash::Hash m_settingsHash;
    SettingsConfig m_useroverrides;
protected:
    HashMap<uint32_t, SettingsValueRef> m_settingValueRefsMap;

private:
    DD_DISALLOW_COPY_AND_ASSIGN(SettingsBase);
    DD_DISALLOW_DEFAULT_CTOR(SettingsBase);
};

} // namespace DevDriver
