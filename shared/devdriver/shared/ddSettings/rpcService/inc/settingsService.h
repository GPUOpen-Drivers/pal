/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "../../base/inc/ddSettingsTypes.h"
#include <ddApi.h>
#include <g_SettingsRpcService.h>
#include <util/hashMap.h>
#include <util/vector.h>
#include <protocols/ddSettingsServiceTypes.h>

using namespace DevDriver::SettingsURIService;

namespace DevDriver
{
class SettingsBase;
}

namespace SettingsRpcService
{

class SettingsService : public SettingsRpc::ISettingsRpcService
{
public:
    SettingsService(const DevDriver::AllocCb& allocCb);

    virtual ~SettingsService() {}

    // Queries the settings components
    DD_RESULT GetComponents(const DDByteWriter& writer) override;

    // Queries the settings for a component
    DD_RESULT QueryComponentSettings(
        const void*         pParamBuffer,
        size_t              paramBufferSize,
        const DDByteWriter& writer) override;

    // Queries for the current settings values for a component
    DD_RESULT QueryCurrentValues(
        const void*         pParamBuffer,
        size_t              paramBufferSize,
        const DDByteWriter& writer) override;

    // Gets the setting data hash of the component
    DD_RESULT QuerySettingsDataHash(
        const void*         pParamBuffer,
        size_t              paramBufferSize,
        const DDByteWriter& writer) override;

    // Sends a setting to the driver
    DD_RESULT SetData(const void* pParamBuffer, size_t paramBufferSize) override;

    void RegisterComponent(const RegisteredComponent& component);

    void UnregisterComponent(const char* pComponentName);

    DevDriver::Result GetValue(
        const RegisteredComponent& component,
        const SettingNameHash      settingName,
        SettingValue**             ppSettingValue,
        bool*                      pNeedsCleanup);

    inline bool IsSettingNameValid(const RegisteredComponent& component, SettingNameHash name)
    {
        bool isValid = false;
        if (component.pSettingsHashes != nullptr)
        {
            for (uint32_t i = 0; i < component.numSettings; i++)
            {
                if (component.pSettingsHashes[i] == name)
                {
                    isValid = true;
                    break;
                }
            }
        }
        return isValid;
    }

    void RegisterSettings(DevDriver::SettingsBase* pSettingsBase);

    DD_RESULT GetCurrentValues(const DDByteWriter& writer) override;

    DD_RESULT SetValue(const void* pParamBuffer, size_t paramBufferSize) override;

private:

    /// A helper function to writer all values of a Settings component to a
    /// buffer.
    DevDriver::Vector<uint8_t> WriteAllComponentValues(
        const DevDriver::HashMap<uint32_t, DDSettingsValueRef>& settingsMap);

private:
    // A hash map of all the components that are currently available to the SettingsService.
    DevDriver::HashMap<uint32_t, RegisteredComponent, 16> m_registeredComponents;

    // A map of all Settings components registered through `RegisterSettings`.
    // The map key is component name.
    DevDriver::HashMap<const char*, DevDriver::SettingsBase*> m_componentsMap;

    DevDriver::AllocCb m_allocCb;
    // Mutex to protect access to the registered components hash map which can be accessed asyncronously from separate threads
    // from the URI calls (RegisterComponent vs. URI calls).
    DevDriver::Platform::Mutex m_componentsMutex;

    static constexpr uint32_t kDefaultGetValueMaxDataSize = kSettingValueBufferSize - sizeof(SettingValue);

    // This buffer is used as temporary storage of SettingValue struct to avoid dynamically allocating memory for the common
    // case of basic data types (bool, int, float, etc).
    uint8_t m_pDefaultGetValueBuffer[kSettingValueBufferSize];
};

} // namespace SettingsRpcService
