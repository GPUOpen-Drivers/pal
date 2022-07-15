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

#include "../../base/inc/ddSettingsTypes.h"
#include <ddApi.h>
#include <g_SettingsRpcService.h>
#include <util/hashMap.h>
#include <protocols/ddSettingsServiceTypes.h>

using namespace DevDriver::SettingsURIService;

namespace SettingsRpcService
{
    class SettingsService : public SettingsRpc::ISettingsRpcService
    {
    public:
        SettingsService(const DevDriver::AllocCb& allocCb);

        virtual ~SettingsService() {}

        // Queries the settings components
        virtual DD_RESULT GetComponents(const DDByteWriter& writer);

        // Queries the settings for a component
        virtual DD_RESULT QueryComponentSettings(
            const void*         pParamBuffer,
            size_t              paramBufferSize,
            const DDByteWriter& writer);

        // Queries for the current settings values for a component
        virtual DD_RESULT QueryCurrentValues(
            const void*         pParamBuffer,
            size_t              paramBufferSize,
            const DDByteWriter& writer);

        // Gets the setting data hash of the component
        virtual DD_RESULT QuerySettingsDataHash(
            const void*         pParamBuffer,
            size_t              paramBufferSize,
            const DDByteWriter& writer);

        // Sends a setting to the driver
        virtual DD_RESULT SetData(const void* pParamBuffer, size_t paramBufferSize);

        void RegisterComponent(const RegisteredComponent& component);

        void UnregisterComponent(const char* pComponentName);

        DevDriver::Result GetValue(
            const RegisteredComponent& component,
            SettingNameHash            settingName,
            SettingValue**             ppSettingValue,
            bool*                      pNeedsCleanup);

        inline bool IsSettingNameValid(const RegisteredComponent& component, SettingNameHash name)
        {
            if (component.pSettingsHashes != nullptr)
            {
                for (uint32_t i = 0; i < component.numSettings; i++)
                {
                    if (component.pSettingsHashes[i] == name)
                    {
                        return true;
                    }
                }
            }
            return false;
        }

    private:
        // A hash map of all the components that are currently available to the SettingsService.
        DevDriver::HashMap<uint32_t, RegisteredComponent, 16> m_registeredComponents;

        DevDriver::AllocCb m_allocCb;
        // Mutex to protect access to the registered components hash map which can be accessed asyncronously from separate threads
        // from the URI calls (RegisterComponent vs. URI calls).
        DevDriver::Platform::Mutex m_componentsMutex;

        static constexpr uint32_t kDefaultGetValueMaxDataSize = kSettingValueBufferSize - sizeof(SettingValue);

        // This buffer is used as temporary storage of SettingValue struct to avoid dynamically allocating memory for the common
        // case of basic data types (bool, int, float, etc).
        uint8_t m_pDefaultGetValueBuffer[kSettingValueBufferSize];
    };
}
