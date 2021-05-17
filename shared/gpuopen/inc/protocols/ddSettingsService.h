/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

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
