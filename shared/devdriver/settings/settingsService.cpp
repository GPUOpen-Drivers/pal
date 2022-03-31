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

#include "settingsService.h"
#include <util/vector.h>
#include <util/ddJsonWriter.h>
#include "settingsTypes.h"

using namespace DevDriver;

namespace SettingsRpcService
{
    // =====================================================================================================================
    SettingsService::SettingsService(
        const AllocCb& allocCb)
        :
        SettingsRpc::ISettingsRpcService(),
        m_registeredComponents(allocCb),
        m_allocCb(allocCb)
    {

    }

    // =====================================================================================================================
    // Registers the component name and setting information.
    // This is called from the generated code for the settings loader and is initialized when a component is created.
    void SettingsService::RegisterComponent(const RegisteredComponent& component)
    {
        Platform::LockGuard<Platform::Mutex> componentsLock(m_componentsMutex);

        // We shouldn't expect there to be collisions in component name, so only check when asserts are enabled
        const uint32_t componentHash = MetroHash::MetroHash32(reinterpret_cast<const uint8_t*>(&component.componentName[0]),
            strlen(component.componentName));
        DD_ASSERT(m_registeredComponents.Contains(componentHash) == false);

        // There's no recourse for the driver if this insert fails (and no harm can come of it), so we just capture the result
        // to be able to assert on failure.
        const Result result = m_registeredComponents.Insert(componentHash, component);
        if (result != Result::Success)
        {
            DD_ASSERT_ALWAYS();
        }
    }

    // =====================================================================================================================
    // Removes the component from the registration hash.
    // This is called when a component is destroyed.
    void SettingsService::UnregisterComponent(
        const char* pComponentName)
    {
        Platform::LockGuard<Platform::Mutex> componentsLock(m_componentsMutex);
        uint32_t componentHash = MetroHash::MetroHash32(reinterpret_cast<const uint8_t*>(pComponentName), strlen(pComponentName));
        auto iter = m_registeredComponents.Find(componentHash);

        if (iter != m_registeredComponents.End())
        {
            m_registeredComponents.Remove(iter);
        }
    }

    // =====================================================================================================================
    // Returns the list of registered settings components and the count
    // The following is the JSON output for one run:
    //  {
    //      "NumComponents" : 3,
    //      "Components" : [
    //          "Pal_Platform",
    //          "Gfx9_Pal",
    //           "Pal"
    //      ]
    //  }
    DD_RESULT SettingsService::GetComponents(
        const DDByteWriter& writer
    )
    {
        Platform::LockGuard<Platform::Mutex> componentsLock(m_componentsMutex);

        Vector<char> buffer(m_allocCb);
        JsonWriter jsonWriter(&buffer);

        jsonWriter.BeginMap();
        jsonWriter.KeyAndBeginList(Components_ComponentsKey);

        for (const auto& entry : m_registeredComponents)
        {
            jsonWriter.Value(entry.value.componentName);
        }

        jsonWriter.EndList();
        jsonWriter.EndMap();
        jsonWriter.End();

        DD_RESULT result = writer.pfnBegin(writer.pUserdata, nullptr);

        if (result == DD_RESULT_SUCCESS)
        {
            result = writer.pfnWriteBytes(writer.pUserdata, buffer.Data(), buffer.Size());
            DD_ASSERT(result == DD_RESULT_SUCCESS);
            writer.pfnEnd(writer.pUserdata, result);
        }

        return result;
    }

    // =====================================================================================================================
    // Returns the settings on a component provided.
    // The data layout is:
    //      SettingsDataHeader (containing the magic buffer info)
    //      The settings data
    DD_RESULT SettingsService::QueryComponentSettings(
        const void*         pParamBuffer,
        size_t              paramBufferSize,
        const DDByteWriter& writer
    )
    {
        char* pComponentName = (char*)pParamBuffer;
        DD_ASSERT(strlen(pComponentName) + 1 == paramBufferSize);
        DD_UNUSED(paramBufferSize);

        DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

        if (pComponentName != nullptr)
        {
            Platform::LockGuard<Platform::Mutex> componentsLock(m_componentsMutex);
            const uint32_t componentHash = MetroHash::MetroHash32(reinterpret_cast<const uint8_t*>(pComponentName), strlen(pComponentName));
            const auto iter = m_registeredComponents.Find(componentHash);

            if (iter != m_registeredComponents.End() && (iter->value.pSettingsData != nullptr))
            {
                const auto& component = iter->value;
                result = writer.pfnBegin(writer.pUserdata, nullptr);

                if (result == DD_RESULT_SUCCESS)
                {
                    result = writer.pfnWriteBytes(writer.pUserdata, &component.settingsDataHeader, sizeof(SettingsDataHeader));
                    DD_ASSERT(result == DD_RESULT_SUCCESS);
                    result = writer.pfnWriteBytes(writer.pUserdata, component.pSettingsData, component.settingsDataSize);
                    DD_ASSERT(result == DD_RESULT_SUCCESS);
                    writer.pfnEnd(writer.pUserdata, result);
                }
            }
            else
            {
                result = (iter == m_registeredComponents.End()) ? DD_RESULT_SETTINGS_SERVICE_INVALID_COMPONENT :
                                                                  DD_RESULT_SETTINGS_SERVICE_INVALID_SETTING_DATA;
            }
        }

        return result;
    }

    // =====================================================================================================================
    // Queries the value for the given settingNameHash, allocating memory for the value if required.
    // Sets pNeedsCleanup to true if memory was malloc'd for the setting value, it is up to the caller to free the memory in
    // that case. In the event of an error, the pNeedsCleanup will never be set to true.
    Result SettingsService::GetValue(
        const RegisteredComponent& component,
        SettingNameHash            settingName,
        SettingValue**             ppSettingValue,
        bool*                      pNeedsCleanup)
    {
        DD_ASSERT(ppSettingValue != nullptr);
        DD_ASSERT(pNeedsCleanup != nullptr);

        bool needsCleanup           = false;
        void* pValueBuffer          = m_pDefaultGetValueBuffer;
        SettingValue* pSettingValue = static_cast<SettingValue*>(pValueBuffer);
        pSettingValue->pValuePtr    = VoidPtrInc(pValueBuffer, sizeof(SettingValue));
        pSettingValue->valueSize    = kDefaultGetValueMaxDataSize;

        // Attempt to query the setting value
        Result result = component.pfnGetValue(settingName, pSettingValue, component.pPrivateData);

        if (result == Result::Success)
        {
            // We've successfully acquired the setting value information
        }
        else if ((result == Result::SettingsUriInvalidSettingValueSize) ||
                 (result == Result::SettingsInsufficientValueSize))
        {
            if (pSettingValue->valueSize <= kMaxSettingValueSize)
            {
                // In some cases, we need to allocate memory in order to hold the setting. In those cases, the required memory
                // size is returned in the valueSize field.  Allocate a buffer big enough to hold the struct and the associated data
                pValueBuffer = m_allocCb.Alloc((pSettingValue->valueSize + sizeof(SettingValue)), false);

                if (pValueBuffer != nullptr)
                {
                    needsCleanup = true;

                    memcpy(pValueBuffer, pSettingValue, sizeof(SettingValue));
                    pSettingValue = static_cast<SettingValue*>(pValueBuffer);
                    pSettingValue->pValuePtr = VoidPtrInc(pValueBuffer, sizeof(SettingValue));

                    // Try again with our newly malloc'd buffer size
                    result = component.pfnGetValue(settingName, pSettingValue, component.pPrivateData);
                }
                else
                {
                    // Malloc failed, we're out of memory
                    result = Result::InsufficientMemory;
                }
            }
            else
            {
                // The setting requires more memory than we're allowed to use.
                result = Result::MemoryOverLimit;
            }
        }
        else if (result == Result::SettingsInvalidSettingName)
        {
            // We were unable to get information about the setting
            // This can happen in cases where settings are conditionally compiled out
        }
        else
        {
            // We've encountered an unknown error
        }

        if (result == Result::Success)
        {
            // Do a little sanity check / validation here to make sure we get reasonable data back from the component
            const bool isSettingValueValid = ((pSettingValue->pValuePtr != nullptr) && (pSettingValue->valueSize > 0));
            result = isSettingValueValid ? Result::Success : Result::SettingsInvalidSettingValue;
        }

        if (result == Result::Success)
        {
            (*ppSettingValue) = pSettingValue;
            (*pNeedsCleanup) = needsCleanup;
        }
        else
        {
            // We've encountered a failure so clean up our memory before returning if necessary.
            if (needsCleanup)
            {
                m_allocCb.Free(pValueBuffer);
            }
        }

        return result;
    }

    // =====================================================================================================================
    // Returns the current values for a specific component.
    // The input is the component name as a string.
    // Each setting hash is written along with its value data.
    DD_RESULT SettingsService::QueryCurrentValues(
        const void*         pParamBuffer,
        size_t              paramBufferSize,
        const DDByteWriter& writer
    )
    {
        DD_RESULT result     = DD_RESULT_COMMON_INVALID_PARAMETER;
        char* pComponentName = (char*)pParamBuffer;

        DD_ASSERT(strlen(pComponentName) + 1 == paramBufferSize);
        DD_UNUSED(paramBufferSize);

        if (pComponentName != nullptr)
        {
            Platform::LockGuard<Platform::Mutex> componentsLock(m_componentsMutex);
            const uint32_t componentHash = MetroHash::MetroHash32(reinterpret_cast<const uint8_t*>(pComponentName), strlen(pComponentName));
            const auto iter = m_registeredComponents.Find(componentHash);

            if (iter != m_registeredComponents.End())
            {
                const auto& component = iter->value;
                result = writer.pfnBegin(writer.pUserdata, nullptr);
                if (result == DD_RESULT_SUCCESS)
                {
                    // For each hash in pSettingsHashes fill out a SettingValue struct and write as a byte response
                    for (uint32_t i = 0; i < component.numSettings; i++)
                    {
                        SettingNameHash settingName = component.pSettingsHashes[i];
                        SettingValue* pSettingValue = nullptr;
                        bool needsCleanup           = false;

                        result = GetValue(component, settingName, &pSettingValue, &needsCleanup) == Result::Success ? DD_RESULT_SUCCESS : DD_RESULT_UNKNOWN;
                        DD_ASSERT(result == DD_RESULT_SUCCESS);
                        if (result == DD_RESULT_SUCCESS)
                        {
                            result = writer.pfnWriteBytes(writer.pUserdata, &settingName, sizeof(SettingNameHash));
                            DD_ASSERT(result == DD_RESULT_SUCCESS);

                            result = writer.pfnWriteBytes(writer.pUserdata, pSettingValue, sizeof(SettingValue));
                            DD_ASSERT(result == DD_RESULT_SUCCESS);

                            result = writer.pfnWriteBytes(writer.pUserdata, pSettingValue->pValuePtr, pSettingValue->valueSize);
                            DD_ASSERT(result == DD_RESULT_SUCCESS);

                            if (needsCleanup)
                            {
                                DD_FREE(pSettingValue, m_allocCb);
                            }
                        }
                    }

                    writer.pfnEnd(writer.pUserdata, result);
                }
            }
        }

        return result;
    }

    // =====================================================================================================================
    // Gets the settings data header for the component.
    // The input is the component name and the output is a struct containing magic buffer details.
    DD_RESULT SettingsService::QuerySettingsDataHash(
        const void*         pParamBuffer,
        size_t              paramBufferSize,
        const DDByteWriter& writer
    )
    {
        DD_RESULT result     = DD_RESULT_COMMON_INVALID_PARAMETER;
        char* pComponentName = (char*)pParamBuffer;

        DD_ASSERT(strlen(pComponentName) + 1 == paramBufferSize);
        DD_UNUSED(paramBufferSize);

        if (pComponentName != nullptr)
        {
            Platform::LockGuard<Platform::Mutex> componentsLock(m_componentsMutex);
            const uint32_t componentHash = MetroHash::MetroHash32(reinterpret_cast<const uint8_t*>(pComponentName), strlen(pComponentName));
            const auto iter = m_registeredComponents.Find(componentHash);

            if (iter != m_registeredComponents.End())
            {
                const auto& component = iter->value;

                size_t hashSize = sizeof(uint64_t);
                result          = writer.pfnBegin(writer.pUserdata, nullptr);

                if (result == DD_RESULT_SUCCESS)
                {
                    result = writer.pfnWriteBytes(writer.pUserdata, &component.settingsDataHash, hashSize);
                    DD_ASSERT(result == DD_RESULT_SUCCESS);
                    writer.pfnEnd(writer.pUserdata, result);
                }
            }
        }

        return result;
    }

    // =====================================================================================================================
    // Function to set a particular setting on a component.
    // The parambuffer contains a struct providing the component name and the setting info.
    DD_RESULT SettingsService::SetData(
        const void* pParamBuffer,
        size_t      paramBufferSize
    )
    {
        DD_ASSERT(sizeof(DDRpcSetDataInfo) == paramBufferSize);
        DD_UNUSED(paramBufferSize);

        DD_RESULT result                 = DD_RESULT_COMMON_INVALID_PARAMETER;
        const DDRpcSetDataInfo* pRpcData = reinterpret_cast<const DDRpcSetDataInfo*>(pParamBuffer);

        if (pRpcData != nullptr)
        {
            const SettingNameHash settingName = pRpcData->nameHash;
            Platform::LockGuard<Platform::Mutex> componentsLock(m_componentsMutex);

            // First, look for the component
            const uint32_t componentHash =
                MetroHash::MetroHash32(reinterpret_cast<const uint8_t*>(pRpcData->componentName), strlen(pRpcData->componentName));
            const auto iter = m_registeredComponents.Find(componentHash);

            if (iter != m_registeredComponents.End())
            {
                const auto& component = iter->value;
                // Verify that the setting shows up in the settings set
                if (IsSettingNameValid(component, settingName))
                {
                    SettingValue settingValue = {};

                    if (pRpcData->dataSize >= sizeof(SettingValue))
                    {
                        const SettingValue* pSettingValue = reinterpret_cast<const SettingValue*>(&pRpcData->dataBuffer);
                        settingValue = *pSettingValue;

                        if (pRpcData->dataSize >= (sizeof(SettingValue) + settingValue.valueSize))
                        {
                            settingValue.pValuePtr = const_cast<void*>(VoidPtrInc(pRpcData->dataBuffer, sizeof(SettingValue)));
                            result                 = DD_RESULT_SUCCESS;
                        }
                        else
                        {
                            // We can ignore trailing data, but if the valueSize is going to cause us to read past the
                            // end of the context data return an error.
                            result = DD_RESULT_FS_INVALID_DATA;
                        }
                    }

                    // If everything checks out so far then send the data to the component
                    if (result == DD_RESULT_SUCCESS)
                    {
                        result = (component.pfnSetValue(settingName, settingValue, component.pPrivateData) == Result::Success) ? DD_RESULT_SUCCESS : DD_RESULT_DD_GENERIC_UNKNOWN;
                    }
                }
                else
                {
                    // Couldn't find the setting name in the set provided by the component
                    result = DD_RESULT_SETTINGS_SERVICE_INVALID_NAME;
                }
            }
            else
            {
                // Couldn't find a component matching the provided name
                result = DD_RESULT_SETTINGS_SERVICE_INVALID_COMPONENT;
            }
        }

        return result;
    }

} // namespace
