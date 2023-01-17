/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
***********************************************************************************************************************
* @file  palSettingsLoader.h
* @brief PAL settings loader utility class declaration.
***********************************************************************************************************************
*/

#pragma once

#include "pal.h"
#include "palDevice.h"
#include "palInlineFuncs.h"
#include "palPlatform.h"
#include "palHashMap.h"
#include "palMetroHash.h"
#include "palSysMemory.h"
#include "protocols/ddSettingsServiceTypes.h"

namespace Pal
{

class IDevice;

/// Enum defining the initialization state of the settings loader.
enum struct SettingsLoaderState : uint32
{
    PreInit   = 0, ///< The initial state of the settings loader, between creation and the call to Init().
    EarlyInit = 1, ///< The state betwen setting of initial default values and registration with developer mode service.
    LateInit  = 2, ///< The state between developer mode registration and finalization, this is the period where the
                   ///< tool will connect and apply overrides.
    Final     = 3  ///< The state after settings finalization, init time settings may not be modified during this state.
};

/// Base structure for driver settings that will be inherited for child class implementations.
struct DriverSettings
{
    uint32 numSettings;
};

typedef DevDriver::SettingsURIService::SettingNameHash SettingNameHash;
typedef DevDriver::SettingsURIService::SettingValue    SettingValue;
typedef DevDriver::SettingsURIService::SettingType     SettingType;

/**
***********************************************************************************************************************
* @brief Settings Loader class.
*
* This class declares a common interface for loading of driver settings for a sub-component and registering that
* sub-component with the Developer Mode Driver Settings URI Service which makes those settings available for query/edit
* via Developer Mode
***********************************************************************************************************************
*/
class ISettingsLoader
{
public:
    template <typename Allocator>
    ISettingsLoader(Allocator* pAllocator, DriverSettings* pSettings, uint32 numSettings)
        :
        m_pSettingsPtr(pSettings),
        m_settingHash(),
        m_state(SettingsLoaderState::PreInit),
        m_allocator(pAllocator),
        m_settingsInfoMap(numSettings, &m_allocator)
        {}

    virtual ~ISettingsLoader() {}

    virtual Result Init() = 0;

    const void* GetDriverSettingsPtr() const { return m_pSettingsPtr; };
    Util::MetroHash::Hash GetSettingsHash() const { return m_settingHash; };

    // auto-generated functions
    virtual void RereadSettings() {}

protected:
    DriverSettings* m_pSettingsPtr;
    Util::MetroHash::Hash m_settingHash;
    SettingsLoaderState m_state;

    virtual void DevDriverRegister() = 0;

    // Check if a setting is allowed to be updated after driver passed
    // initialization and is in running state. By default, settings are
    // not allowed to update in running state.
    virtual bool IsSetAllowedInDriverRunningState(SettingNameHash hash)
    {
        return false;
    }

    // Determines if a setting can be modified. By default we allow
    // modifying of all settings in EarlyInit or LateInit.
    bool IsSetValueAvailable(SettingNameHash setting)
    {
        const bool initState = ((m_state == SettingsLoaderState::EarlyInit) ||
                                (m_state == SettingsLoaderState::LateInit));
        return (initState || IsSetAllowedInDriverRunningState(setting));
    }

    // This function is called in the static SetValue implementation, it is used to perform any complex processing
    // related to setting the value of a particular setting. If this function returns NotReady it indicates the
    // SetValue request was NOT handled and the default memcpy of the setting value will be performed.  Success
    // indicates the value was successfully updated, other error codes describe failures e.g. invalid parameters.
    virtual DevDriver::Result PerformSetValue(
        SettingNameHash     hash,
        const SettingValue& settingValue)
    {
        // Default implementation assumes no action needed, simply returns NotReady
        return DevDriver::Result::NotReady;
    }

    static DevDriver::Result GetValue(
        SettingNameHash hash,
        SettingValue*   pSettingValue,
        void*           pPrivateData);

    static DevDriver::Result SetValue(
        SettingNameHash     hash,
        const SettingValue& settingValue,
        void*               pPrivateData);

    struct SettingInfo
    {
        SettingType  type;       // Setting value type
        void*        pValuePtr;  // Memory location of the setting value
        uint32       valueSize;  // Size of the setting value
    };

    Util::IndirectAllocator m_allocator;

    typedef Util::HashMap<SettingNameHash,
                  SettingInfo,
                  Util::IndirectAllocator,
                  Util::DefaultHashFunc,
                  Util::DefaultEqualFunc,
                  Util::HashAllocator<Util::IndirectAllocator>,
                  192> SettingsInfoHashMap;

    SettingsInfoHashMap m_settingsInfoMap;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(ISettingsLoader);
    PAL_DISALLOW_DEFAULT_CTOR(ISettingsLoader);

    // This is a wrapper that should be implemented by different platform to
    // read initial Settings user-values from a source on local machine, and
    // is called by Settings generated code.
    virtual bool ReadSetting(
        const char*          pSettingName,
        void*                pValue,
        Util::ValueType      valueType,
        size_t               bufferSize,
        InternalSettingScope settingType)
    {
        return false;
    }

    // auto-generated functions
    virtual void SetupDefaults() = 0;
    virtual void ReadSettings() = 0;
    virtual void InitSettingsInfo() = 0;
};

} // Pal
