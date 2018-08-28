/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
    ISettingsLoader(Util::IndirectAllocator* pAllocator, DriverSettings* pSettings, uint32 numSettings)
        :
        m_pSettingsPtr(pSettings),
        m_settingHash(),
        m_state(SettingsLoaderState::PreInit),
        m_settingsInfoMap(numSettings, pAllocator)
        {}

    virtual ~ISettingsLoader() {}

    virtual Result Init() = 0;

    const void* GetDriverSettingsPtr() const { return m_pSettingsPtr; };
    Util::MetroHash::Hash GetSettingsHash() const { return m_settingHash; };

protected:
    DriverSettings* m_pSettingsPtr;
    Util::MetroHash::Hash m_settingHash;
    SettingsLoaderState m_state;

    virtual void DevDriverRegister() = 0;
    // Determines if settings can be modified in the runtime state.  By default we only allow modifying of settings
    // in EarlyInit or LateInit.
    virtual bool IsSetValueAvailable()
    {
        return ((m_state  == SettingsLoaderState::EarlyInit) || (m_state == SettingsLoaderState::LateInit));
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

    Util::HashMap<SettingNameHash, SettingInfo, Util::IndirectAllocator> m_settingsInfoMap;
    static constexpr uint32 NumSettingBuckets = 256;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(ISettingsLoader);
    PAL_DISALLOW_DEFAULT_CTOR(ISettingsLoader);

    // auto-generated functions
    virtual void SetupDefaults() = 0;
    virtual void ReadSettings() = 0;
    virtual void InitSettingsInfo() = 0;
};

} // Pal
