/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  ddSettingsService.h
* @brief Class declaration for the Settings URI Service.
***********************************************************************************************************************
*/

#pragma once

#include "ddSettingsServiceTypes.h"
#include "ddUriInterface.h"
#include "util/hashMap.h"

namespace DevDriver
{
namespace Platform
{
class Mutex;
}
namespace SettingsURIService
{

static const char* kSettingsServiceName = "settings";
DD_STATIC_CONST Version kSettingsServiceVersion = 4;

/*
***********************************************************************************************************************
*| Version | Change Description                                                                                       |
*| ------- | ---------------------------------------------------------------------------------------------------------|
*|  4.0    | Adds components2, a fused version of the components and settingsDataHash commands                        |
*|  3.0    | Adds settingsDataHash and queryCurrentValues commands                                                    |
*|  2.0    | Adds header to settingsData to indicate if/how data is encoded.                                          |
*|  1.0    | Initial version                                                                                          |
***********************************************************************************************************************
*/

// =====================================================================================================================
// Settings Service
// Used to allow clients (and client subcomponents) to register settings on the developer driver bus to remotely query
// and override the setting values.
class SettingsService : public IService
{
public:
    explicit SettingsService(const AllocCb& allocCb);
    virtual ~SettingsService();

    // Returns the name of the service
    const char* GetName() const override final { return kSettingsServiceName; }
    Version GetVersion() const override final { return kSettingsServiceVersion; }

    // Handles a request from a developer driver client.
    Result HandleRequest(IURIRequestContext* pContext) override final;

    // Registers a component that has settings to be exposed through the settings service.
    // NOTE: The data provided in the RegisteredComponent struct will be retained for the lifefime of the
    //       component in the settings service (i.e. until process end or an UnregisterComponent call). It is
    //       the component's responsibility to ensure the data remains valid for that lifetime.
    void RegisterComponent(const RegisteredComponent& component);
    void UnregisterComponent(const char* pComponentName);

    virtual size_t QueryPostSizeLimit(char* pArguments) const override
    {
        // We can add some more detailed validation later, but for now just return the max size
        DD_UNUSED(pArguments);
        return kMaxSettingValueSize;
    }

private:
    // Command handlers
    Result HandleGetComponents(IURIRequestContext* pContext);
    Result HandleGetComponents2(IURIRequestContext* pContext);
    Result HandleGetSettingDataHash(IURIRequestContext* pContext);
    Result HandleGetSettingData(IURIRequestContext* pContext);
    Result HandleGetValue(IURIRequestContext* pContext);
    Result HandleQueryValues(IURIRequestContext* pContext);
    Result HandleSetValue(IURIRequestContext* pContext);
    Result GetValue(const RegisteredComponent& component, SettingNameHash settingName, SettingValue** ppSettingValue, bool* pNeedsCleanup);

    inline bool IsSettingNameValid(const RegisteredComponent& component, SettingNameHash name)
    {
        if (component.pSettingsHashes != nullptr)
        {
            for (uint32 i=0; i<component.numSettings; i++)
            {
                if (component.pSettingsHashes[i] == name)
                {
                    return true;
                }
            }
        }
        return false;
    }

    AllocCb m_allocCb;
    static constexpr uint32 kDefaultGetValueBufferSize = 256;
    static constexpr uint32 kDefaultGetValueMaxDataSize = kDefaultGetValueBufferSize - sizeof(SettingValue);
    // This buffer is used as temporary storage of SettingValue struct to avoid dynamically allocating memory for the common
    // case of basic data types (bool, int, float, etc).
    uint8 m_pDefaultGetValueBuffer[kDefaultGetValueBufferSize];
    // Mutex to protect access to the registered components hash map which can be accessed asyncronously from separate threads
    // from the URI calls (RegisterComponent vs. URI calls).
    Platform::Mutex m_componentsMutex;

    // A hash map of all the components that are currently available to the SettingsService.
    HashMap<uint32, RegisteredComponent, 16> m_registeredComponents;

    // Context to be used for tokenizing the argument string for settings commands
    char* m_pContext;
};
} // SettingsURIService
} // DevDriver
