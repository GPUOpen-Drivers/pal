/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "protocols/ddSettingsService.h"
#include "msgChannel.h"
#include "ddTransferManager.h"
#include "util/hashMap.h"
#include "util/string.h"
#include "util/ddMetroHash.h"
#include "ddPlatform.h"

namespace DevDriver
{
namespace SettingsURIService
{

// =====================================================================================================================
SettingsService::SettingsService(
    const AllocCb& allocCb)
    :
    DevDriver::IService(),
    m_allocCb(allocCb),
    m_pDefaultGetValueBuffer(),
    m_componentsMutex(),
    m_registeredComponents(allocCb),
    m_pContext(nullptr)
{
}

// =====================================================================================================================
SettingsService::~SettingsService()
{
}

// =====================================================================================================================
void SettingsService::RegisterComponent(const RegisteredComponent& component)
{
    Platform::LockGuard<Platform::Mutex> componentsLock(m_componentsMutex);

    // We shouldn't expect there to be collisions in component name, so only check when asserts are enabled
    const uint32 componentHash = MetroHash::MetroHash32(reinterpret_cast<const uint8*>(&component.componentName[0]),
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
void SettingsService::UnregisterComponent(
    const char* pComponentName)
{
    Platform::LockGuard<Platform::Mutex> componentsLock(m_componentsMutex);
    uint32 componentHash = MetroHash::MetroHash32(reinterpret_cast<const uint8*>(pComponentName), strlen(pComponentName));
    auto iter = m_registeredComponents.Find(componentHash);
    if (iter != m_registeredComponents.End())
    {
        m_registeredComponents.Remove(iter);
    }
}

#if DD_VERSION_SUPPORTS(GPUOPEN_URIINTERFACE_CLEANUP_VERSION)
// =====================================================================================================================
// Handles settings requests from the developer driver bus
Result SettingsService::HandleRequest(
    IURIRequestContext* pContext)
{
    Result result = Result::UriInvalidParameters;

    // We can safely use a single strtok context here because HandleRequest can only be called on one thread at a time
    // (enforced by the URI server).
    const char* pCommandArg = Platform::Strtok(pContext->GetRequestArguments(), " ", &m_pContext);

    if (pCommandArg != nullptr)
    {
        if (strcmp(pCommandArg, "components") == 0)
        {
            result = HandleGetComponents(pContext);
        }
        else if (strcmp(pCommandArg, "settingsData") == 0)
        {
            result = HandleGetSettingData(pContext);
        }
        else if (strcmp(pCommandArg, "getValue") == 0)
        {
            result = HandleGetValue(pContext);
        }
        else if (strcmp(pCommandArg, "setValue") == 0)
        {
            result = HandleSetValue(pContext);
        }
        else
        {
            // Unsupported request
        }
    }

    return result;
}

// =====================================================================================================================
// Returns the list of registered settings components
Result SettingsService::HandleGetComponents(
    IURIRequestContext* pContext)
{
    Platform::LockGuard<Platform::Mutex> componentsLock(m_componentsMutex);

    IStructuredWriter* pWriter;
    Result result = pContext->BeginJsonResponse(&pWriter);
    if (result == Result::Success)
    {
        pWriter->BeginMap();
        pWriter->KeyAndBeginList("components");

        for (const auto& entry : m_registeredComponents)
        {
            pWriter->Value(entry.value.componentName);
        }

        pWriter->EndList();
        pWriter->EndMap();
        result = pWriter->End();
    }

    return result;
}

// =====================================================================================================================
Result SettingsService::HandleGetSettingData(
    IURIRequestContext* pContext)
{
    Result result = Result::SettingsUriInvalidComponent;

    // This uses the same strtok context started in HandleRequest, which is safe because it can only be called on one thread
    // at a time (enforced by URI Server)
    const char* pComponentName = Platform::Strtok(nullptr, " ", &m_pContext);
    if (pComponentName != nullptr)
    {
        Platform::LockGuard<Platform::Mutex> componentsLock(m_componentsMutex);
        const uint32 componentHash = MetroHash::MetroHash32(reinterpret_cast<const uint8*>(pComponentName), strlen(pComponentName));
        const auto iter = m_registeredComponents.Find(componentHash);
        if (iter != m_registeredComponents.End() && (iter->value.pSettingsData != nullptr))
        {
            const auto& component = iter->value;
            IByteWriter* pWriter = nullptr;
            result = pContext->BeginByteResponse(&pWriter);
            if (result == Result::Success)
            {
                pWriter->WriteBytes(&component.settingsDataHeader, sizeof(SettingsDataHeader));
                pWriter->WriteBytes(component.pSettingsData, component.settingsDataSize);
                result = pWriter->End();
            }
        }
    }

    return result;
}

// =====================================================================================================================
Result SettingsService::HandleGetValue(
    IURIRequestContext* pContext)
{
    Result result = Result::Success;
    // This uses the same strtok context started in HandleRequest, which is safe because it can only be called on one thread
    // at a time (enforced by URI Server)
    const char* pComponentName = Platform::Strtok(nullptr, " ", &m_pContext);
    const char* pSettingNameStr = Platform::Strtok(nullptr, " ", &m_pContext);
    if ((pComponentName != nullptr) && (pSettingNameStr != nullptr))
    {
        Platform::LockGuard<Platform::Mutex> componentsLock(m_componentsMutex);
        const SettingNameHash settingName = strtoul(pSettingNameStr, nullptr, 0);

        // First look for the component
        const uint32 componentHash =
            MetroHash::MetroHash32(reinterpret_cast<const uint8*>(pComponentName), strlen(pComponentName));
        const auto iter = m_registeredComponents.Find(componentHash);
        if (iter != m_registeredComponents.End())
        {
            const auto& component = iter->value;
            // Verify that the setting shows up in the settings set
            if (IsSettingNameValid(component, settingName))
            {
                void* pValueBuffer = m_pDefaultGetValueBuffer;
                SettingValue* pSettingValue = static_cast<SettingValue*>(pValueBuffer);
                pSettingValue->pValuePtr = VoidPtrInc(pValueBuffer, sizeof(SettingValue));
                pSettingValue->valueSize = kDefaultGetValueMaxDataSize;

                // First call to get the value size
                result = component.pfnGetValue(settingName, pSettingValue, component.pPrivateData);
                if (result != Result::Success)
                {
                    DD_ASSERT((result == Result::InsufficientMemory) &&
                              (pSettingValue->valueSize < kMaxSettingValueSize));

                    // The only other result we should expect is insuffient memory, in that case the required memory
                    // size is returned in the valueSize field.  Allocate a buffer big enough to hold the struct and the associated data
                    pValueBuffer = DD_MALLOC((pSettingValue->valueSize + sizeof(SettingValue)), DD_DEFAULT_ALIGNMENT, m_allocCb);
                    if (pValueBuffer != nullptr)
                    {
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

                if (result == Result::Success)
                {
                    if ((pSettingValue->pValuePtr != nullptr) && (pSettingValue->valueSize > 0))
                    {
                        IByteWriter* pWriter = nullptr;
                        result = pContext->BeginByteResponse(&pWriter);
                        if (result == Result::Success)
                        {
                            // We've got the value, now send it back to the client.  We'll send the struct as binary,
                            // with the pointer zeroed out.
                            void* pValueDataPtr = pSettingValue->pValuePtr;
                            pSettingValue->pValuePtr = nullptr;
                            pWriter->WriteBytes(pSettingValue, sizeof(SettingValue));
                            pWriter->WriteBytes(pValueDataPtr, pSettingValue->valueSize);
                            result = pWriter->End();
                        }
                    }
                    else
                    {
                        // The component didn't properly fill in the setting value struct, return an error
                        result = Result::Error;
                    }
                }
                else
                {
                    // We shouldn't ever hit this assert, probably means we're out of memory
                    result = Result::InsufficientMemory;
                    DD_ASSERT_ALWAYS();
                }

                if ((pValueBuffer != nullptr) && (pValueBuffer != m_pDefaultGetValueBuffer))
                {
                    // Free the memory if we allocated a separate buffer to hold the value
                    DD_FREE(pValueBuffer, m_allocCb);
                }
            }
            else
            {
                // Couldn't find the setting name in the set provided by the component
                result = Result::SettingsUriInvalidSettingName;
            }
        }
        else
        {
            result = Result::SettingsUriInvalidComponent;
        }
    }

    return result;
}

// =====================================================================================================================
Result SettingsService::HandleSetValue(
    IURIRequestContext* pContext)
{
    Result result = Result::Success;
    // This continues the same strtok started in HandleRequest, which is safe because it can only be called on one thread
    // at a time (enforced by URI Server)
    const char* pComponentName = Platform::Strtok(nullptr, " ", &m_pContext);
    const char* pSettingNameStr = Platform::Strtok(nullptr, " ", &m_pContext);
    if ((pComponentName != nullptr) && (pSettingNameStr != nullptr))
    {
        Platform::LockGuard<Platform::Mutex> componentsLock(m_componentsMutex);
        const SettingNameHash settingName = strtoul(pSettingNameStr, nullptr, 0);

        // First, look for the component
        const uint32 componentHash =
            MetroHash::MetroHash32(reinterpret_cast<const uint8*>(pComponentName), strlen(pComponentName));
        const auto iter = m_registeredComponents.Find(componentHash);
        if (iter != m_registeredComponents.End())
        {
            const auto& component = iter->value;
            // Verify that the setting shows up in the settings set
            if (IsSettingNameValid(component, settingName))
            {
                // We found component and setting matching the parameters, setup the provided post data as a
                // SettingValue struct
                SettingValue settingValue = {};
                const PostDataInfo& postData = pContext->GetPostData();
                if((postData.pData != nullptr) && (postData.size >= sizeof(SettingValue)))
                {
                    const SettingValue* pSettingValue = static_cast<const SettingValue*>(postData.pData);
                    settingValue =* pSettingValue;

                    if (postData.size >= (sizeof(SettingValue) + settingValue.valueSize))
                    {
                        settingValue.pValuePtr = const_cast<void*>(VoidPtrInc(postData.pData, sizeof(SettingValue)));
                    }
                    else
                    {
                        // We can ignore trailing data, but if the valueSize is going to cause us to read past the
                        // end of the context data return an error.
                        result = Result::SettingsUriInvalidSettingValueSize;
                    }
                }

                // If everything checks out so far then send the data to the component
                if (result == Result::Success)
                {
                    result = component.pfnSetValue(settingName, settingValue, component.pPrivateData);
                }
            }
            else
            {
                // Couldn't find the setting name in the set provided by the component
                result = Result::SettingsUriInvalidSettingName;
            }
        }
        else
        {
            // Couldn't find a component matching the provided name
            result = Result::SettingsUriInvalidComponent;
        }
    }

    return result;
}
#endif

} // SettingsURIService
} // DevDriver
