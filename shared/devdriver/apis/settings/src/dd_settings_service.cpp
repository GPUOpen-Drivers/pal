/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <dd_settings_service.h>
#include <dd_dynamic_buffer.h>
#include <dd_settings_iterator.h>
#include <dd_integer.h>

#include <ddCommon.h>

#include <cstdlib>
#include <limits>
#include <utility>

namespace DevDriver
{

SettingsRpcService::SettingsRpcService()
    : m_allocCb{Platform::GenericAllocCb}
    , m_settingsComponents{m_allocCb}
    , m_pAllUserOverridesData{nullptr}
    , m_allUserOverrides{m_allocCb}
{
}

SettingsRpcService::~SettingsRpcService()
{
    if (m_pAllUserOverridesData != nullptr)
    {
        m_allocCb.Free(m_pAllUserOverridesData);
    }
}

void SettingsRpcService::RegisterSettingsComponent(SettingsBase* pSettingsComponent)
{
    LockGuard lock(m_settingsComponentsMutex);

    Result result = m_settingsComponents.Insert(pSettingsComponent->GetComponentName(), pSettingsComponent);
    DD_ASSERT(result == Result::Success);

    ApplyComponentUserOverrides(pSettingsComponent);
}

void SettingsRpcService::ApplyComponentUserOverrides(SettingsBase* pSettingsComponent)
{
    Vector<DDSettingsValueRef>* pOverrides = m_allUserOverrides.FindValue(pSettingsComponent->GetComponentName());
    if (pOverrides)
    {
        for (const auto& override : *pOverrides)
        {
            DD_RESULT setResult = pSettingsComponent->SetValue(override);
            if (setResult != DD_RESULT_SUCCESS)
            {
                // TODO: log error
            }
        }
    }
    else
    {
        // TODO: log errors
    }
}

bool SettingsRpcService::ApplyUserOverride(
    SettingsBase*         pSettingsComponent,
    DD_SETTINGS_NAME_HASH nameHash,
    void*                 pSetting,
    size_t                settingSize)
{
    bool applied = false;

    Vector<DDSettingsValueRef>* pOverrides = m_allUserOverrides.FindValue(pSettingsComponent->GetComponentName());
    if (pOverrides != nullptr)
    {
        for (const DDSettingsValueRef& override : *pOverrides)
        {
            if (override.hash == nameHash)
            {
                if (settingSize >= override.size)
                {
                    memcpy(pSetting, override.pValue, override.size);
                    applied = true;
                }
                break;
            }
        }
    }

    return applied;
}

size_t SettingsRpcService::TotalUserOverrideCount() const
{
    size_t count = 0;
    for (const auto& component : m_allUserOverrides)
    {
        count += component.value.Size();
    }
    return count;
}

DD_RESULT SettingsRpcService::SendAllUserOverrides(const void* pParamBuf, size_t paramBufSize)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    // User-overrides data are sent at the earliest point during driver initialization. We need to keep
    // them around to initialize settings components later.
    m_pAllUserOverridesData = (uint8_t*)m_allocCb.Alloc(paramBufSize, false);
    memcpy(m_pAllUserOverridesData, pParamBuf, paramBufSize);

    // Parse user-overrides.

    SettingsIterator iter(m_pAllUserOverridesData, paramBufSize);

    SettingsIterator::Component component {};
    while (iter.NextComponent(&component))
    {
        Vector<DDSettingsValueRef> overrides(m_allocCb);
        overrides.Reserve(component.numValues);

        SettingsIterator::Value value {};
        uint16_t numValues = 0;
        while(iter.NextValue(component, &value))
        {
            overrides.PushBack(value.valueRef);
            numValues += 1;
        }

        if (numValues != component.numValues)
        {
            // TODO(linwang4): log error.
        }

        Result hashmapResult = m_allUserOverrides.Create(component.pName, std::move(overrides));
        result = DevDriverToDDResult(hashmapResult);
        if (result != DD_RESULT_SUCCESS)
        {
            // TODO(linwang4): log error.
            break;
        }
    }

    return result;
}

DD_RESULT SettingsRpcService::QueryAllCurrentValues(const DDByteWriter& writer)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    // Data are laid out in the following format:
    //
    // DDSettingsAllComponentsHeader
    // DDSettingsComponentHeader
    //   DDSettingsValueHeader | variable-sized value data
    //   .. repeat for all the settings in the component
    // .. repeat for all components

    writer.pfnBegin(writer.pUserdata, nullptr);

    DDSettingsAllComponentsHeader allCompsHeader {};
    allCompsHeader.version = 1;
    allCompsHeader.numComponents = SafeCastToU16(m_settingsComponents.Size());

    writer.pfnWriteBytes(writer.pUserdata, &allCompsHeader, sizeof(allCompsHeader));

    DynamicBuffer valuesBuf;
    valuesBuf.Reserve(4 * 1024);
    for (const auto& entry : m_settingsComponents)
    {
        size_t numValues = 0;

        result = entry.value->GetAllValues(valuesBuf, &numValues);
        if (result == DD_RESULT_SUCCESS)
        {
            DDSettingsComponentHeader header {};

            size_t compNameSize = strlen(entry.value->GetComponentName()) + 1; // + 1 for null-terminator
            DD_ASSERT(compNameSize <= DD_SETTINGS_MAX_COMPONENT_NAME_SIZE);
            compNameSize = (compNameSize > DD_SETTINGS_MAX_COMPONENT_NAME_SIZE) ? DD_SETTINGS_MAX_COMPONENT_NAME_SIZE
                                                                                : compNameSize;
            memcpy(header.name, entry.value->GetComponentName(), compNameSize);
            header.name[DD_SETTINGS_MAX_COMPONENT_NAME_SIZE - 1] = '\0';

            header.blobHash = entry.value->GetSettingsBlobHash();

            header.numValues = SafeCastToU16(numValues);

            size_t compSize = valuesBuf.Size() + sizeof(header);
            header.size = SafeCastToU16(compSize);

            result = writer.pfnWriteBytes(writer.pUserdata, &header, sizeof(header));

            // Do not write the values buffer if there is nothing in it:
            if ((result == DD_RESULT_SUCCESS) && (valuesBuf.Size() > 0))
            {
                result = writer.pfnWriteBytes(writer.pUserdata, valuesBuf.Data(), valuesBuf.Size());
            }
        }
        valuesBuf.Clear();

        if (result != DD_RESULT_SUCCESS)
        {
            break;
        }
    }

    writer.pfnEnd(writer.pUserdata, result);

    return result;
}

DD_RESULT SettingsRpcService::GetUnsupportedExperiments(const DDByteWriter& writer)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    // Data are laid out in the following format:
    //
    // DDSettingsAllComponentsHeader
    // DDSettingsComponentHeader
    //   DD_SETTINGS_NAME_HASH
    //   .. repeat for all the experiments in the component
    // .. repeat for all components

    writer.pfnBegin(writer.pUserdata, nullptr);

    DDSettingsAllComponentsHeader allCompsHeader{};
    allCompsHeader.version       = 1;
    allCompsHeader.numComponents = SafeCastToU16(m_settingsComponents.Size());

    writer.pfnWriteBytes(writer.pUserdata, &allCompsHeader, sizeof(allCompsHeader));

    DynamicBuffer valuesBuf;
    valuesBuf.Reserve(4 * 1024);
    for (const auto& entry : m_settingsComponents)
    {
        size_t numValues = 0;

        // Check for unsupported experiments, but skip writing anything if there weren't any for the component
        result = entry.value->GetUnsupportedExperiments(valuesBuf, &numValues);
        if ((result == DD_RESULT_SUCCESS) && (numValues > 0))
        {
            DDSettingsComponentHeader header{};

            size_t compNameSize = strlen(entry.value->GetComponentName()) + 1; // + 1 for null-terminator
            DD_ASSERT(compNameSize <= DD_SETTINGS_MAX_COMPONENT_NAME_SIZE);
            compNameSize = (compNameSize > DD_SETTINGS_MAX_COMPONENT_NAME_SIZE) ? DD_SETTINGS_MAX_COMPONENT_NAME_SIZE :
                                                                                  compNameSize;
            memcpy(header.name, entry.value->GetComponentName(), compNameSize);
            header.name[DD_SETTINGS_MAX_COMPONENT_NAME_SIZE - 1] = '\0';

            header.blobHash = entry.value->GetSettingsBlobHash();

            header.numValues = SafeCastToU16(numValues);

            size_t compSize = valuesBuf.Size() + sizeof(header);
            header.size     = SafeCastToU16(compSize);

            result = writer.pfnWriteBytes(writer.pUserdata, &header, sizeof(header));
            if (result == DD_RESULT_SUCCESS)
            {
                result = writer.pfnWriteBytes(writer.pUserdata, valuesBuf.Data(), valuesBuf.Size());
            }
        }
        valuesBuf.Clear();

        if (result != DD_RESULT_SUCCESS)
        {
            break;
        }
    }

    writer.pfnEnd(writer.pUserdata, result);

    return result;
}

} // namespace DevDriver
