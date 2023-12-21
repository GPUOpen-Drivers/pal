/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "../inc/ddSettingsBase.h"
#include <ddCommon.h>

using namespace DevDriver;

// =============================================================================
// Searches the settings info hash map for the provided hash, if found it will
// return the setting value data in the provided SettingValue pointer.  If the
// provided value memory is not big enough, this function will return an error
// and only update the valueSize.
Result SettingsBase::GetValue(
    uint32_t      hash,
    SettingValue* pOutSettingValue,
    void*         pPrivateData)
{
    DD_ASSERT(pOutSettingValue != nullptr);

    Result ret = DevDriver::Result::SettingsInvalidSettingName;
    SettingsBase* pSettings = reinterpret_cast<SettingsBase*>(pPrivateData);
    SettingsValueRef* pCurrSettingValRef = pSettings->m_settingValueRefsMap.FindValue(hash);
    if (pCurrSettingValRef != nullptr)
    {
        if (pCurrSettingValRef->size > pOutSettingValue->valueSize)
        {
            pOutSettingValue->valueSize = pCurrSettingValRef->size;
            ret = DevDriver::Result::SettingsInvalidSettingValueSize;
        }
        else
        {
            memcpy(pOutSettingValue->pValuePtr, pCurrSettingValRef->pValue, pCurrSettingValRef->size);
            pOutSettingValue->valueSize = pCurrSettingValRef->size;
            pOutSettingValue->type = pCurrSettingValRef->type;
            ret = DevDriver::Result::Success;
        }
    }

    return ret;
}

// =============================================================================
// Searches the settings info hash map for the provided hash, if found it
// will set value using the provided data. If the matching setting has provided
// a function pointer instead of a value location then that function will be
// called instead of copying the value.
Result SettingsBase::SetValue(
    uint32_t            hash,
    const SettingValue& settingValue,
    void*               pPrivateData)
{
    DevDriver::Result ret = DevDriver::Result::Unavailable;

    SettingsBase* pSettings = reinterpret_cast<SettingsBase*>(pPrivateData);
    SettingsValueRef* pCurrSettingValRef = pSettings->m_settingValueRefsMap.FindValue(hash);
    if (pCurrSettingValRef != nullptr)
    {
        if (pCurrSettingValRef->type == settingValue.type)
        {
            // Give the derived class a chance to update the value in case
            // they need to do something more complex than a simple memcpy
            ret = pSettings->PerformSetValue(hash, settingValue);

            // NotReady indicates that PerformSetValue did not handle the
            // SetValue request, so fall back to the simple memcpy
            if (ret == DevDriver::Result::NotReady)
            {
                if (pCurrSettingValRef->size >= settingValue.valueSize)
                {
                    memcpy(pCurrSettingValRef->pValue, settingValue.pValuePtr, settingValue.valueSize);
                    ret = DevDriver::Result::Success;
                }
                else
                {
                    ret = DevDriver::Result::SettingsInvalidSettingValueSize;
                }
            }
        }
        else
        {
            ret = DevDriver::Result::SettingsInvalidSettingValue;
        }
    }
    else
    {
        ret = DevDriver::Result::SettingsInvalidSettingName;
    }

    return ret;
}

// =======================================================================================
DD_RESULT SettingsBase::SetValue(
    DD_SETTINGS_NAME_HASH nameHash,
    const DDSettingsValueRef& srcValPtr)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    DDSettingsValueRef* pDestValPtr = m_settingsMap.FindValue(nameHash);
    if (pDestValPtr != nullptr)
    {
        if (pDestValPtr->type == srcValPtr.type)
        {
            bool set = CustomSetValue(nameHash, srcValPtr);
            if (set == false)
            {
                if (pDestValPtr->size >= srcValPtr.size)
                {
                    memcpy(pDestValPtr->pValue, srcValPtr.pValue, pDestValPtr->size);
                }
                else
                {
                    result = DD_RESULT_COMMON_BUFFER_TOO_SMALL;
                }
            }
        }
        else
        {
            result = DD_RESULT_COMMON_INVALID_PARAMETER;
        }
    }
    else
    {
        result = DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    return result;
}

// =======================================================================================
DD_RESULT SettingsBase::GetValue(
    DD_SETTINGS_NAME_HASH nameHash,
    DDSettingsValueRef* pOutValPtr) const
{
    DD_ASSERT(pOutValPtr != nullptr);

    DD_RESULT result = DD_RESULT_SUCCESS;

    DDSettingsValueRef* pFoundValPtr = m_settingsMap.FindValue(nameHash);
    if (pFoundValPtr != nullptr)
    {
        pOutValPtr->size = pFoundValPtr->size;
        pOutValPtr->type = pFoundValPtr->type;

        if (pOutValPtr->size >= pFoundValPtr->size)
        {
            if (pOutValPtr->pValue != nullptr)
            {
                memcpy(pOutValPtr->pValue, pFoundValPtr->pValue, pOutValPtr->size);
            }
            else
            {
                result = DD_RESULT_COMMON_BUFFER_TOO_SMALL;
            }
        }
        else
        {
            result = DD_RESULT_COMMON_BUFFER_TOO_SMALL;
        }
    }
    else
    {
        result = DD_RESULT_COMMON_INTERFACE_NOT_FOUND;
    }

    return result;
}

// =======================================================================================
DD_RESULT SettingsBase::ApplyAllUserOverrides()
{
    DD_RESULT result = DD_RESULT_SUCCESS;
    int64_t uservalueAppliedCount = 0;
    int64_t uservalueCount = 0;

    SettingsUserOverrideIter userOverrideIter =
        m_useroverrides.GetUserOverridesIter(GetComponentName());
    if (userOverrideIter.IsValid())
    {
        for (;;)
        {
            SettingsUserOverride useroverride = userOverrideIter.Next();
            if (!useroverride.isValid)
            {
                break;
            }
            uservalueCount += 1;

            DD_RESULT setResult = ApplyUserOverrideImpl(useroverride);
            if (setResult == DD_RESULT_SUCCESS)
            {
                uservalueAppliedCount += 1;
            }
        }
    }

    if ((uservalueAppliedCount > 0) && (uservalueAppliedCount < uservalueCount))
    {
        result = DD_RESULT_COMMON_SUCCESS_WITH_ERRORS;
    }
    return result;
}

// =======================================================================================
DD_RESULT SettingsBase::ApplyUserOverrideByNameHash(
    DD_SETTINGS_NAME_HASH nameHash)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    SettingsUserOverride useroverride =
        m_useroverrides.GetUserOverrideByNameHash(GetComponentName(), nameHash);

    if (useroverride.isValid)
    {
        result = ApplyUserOverrideImpl(useroverride);
    }
    else
    {
        result = DD_RESULT_COMMON_DOES_NOT_EXIST;
    }

    return result;
}

// =======================================================================================
DD_RESULT SettingsBase::ApplyUserOverrideImpl(
    const SettingsUserOverride& useroverride)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    DDSettingsValueRef valueRef = {};
    valueRef.hash = useroverride.nameHash;
    valueRef.type = useroverride.type;
    valueRef.size = static_cast<uint16_t>(useroverride.size);

    switch (useroverride.type)
    {
    case DD_SETTINGS_TYPE_BOOL:   valueRef.pValue = (void*)&useroverride.value.b; break;
    case DD_SETTINGS_TYPE_INT8:   valueRef.pValue = (void*)&useroverride.value.i8; break;
    case DD_SETTINGS_TYPE_UINT8:  valueRef.pValue = (void*)&useroverride.value.u8; break;
    case DD_SETTINGS_TYPE_INT16:  valueRef.pValue = (void*)&useroverride.value.i16; break;
    case DD_SETTINGS_TYPE_UINT16: valueRef.pValue = (void*)&useroverride.value.u16; break;
    case DD_SETTINGS_TYPE_INT32:  valueRef.pValue = (void*)&useroverride.value.i32; break;
    case DD_SETTINGS_TYPE_UINT32: valueRef.pValue = (void*)&useroverride.value.u32; break;
    case DD_SETTINGS_TYPE_INT64:  valueRef.pValue = (void*)&useroverride.value.i64; break;
    case DD_SETTINGS_TYPE_UINT64: valueRef.pValue = (void*)&useroverride.value.u64; break;
    case DD_SETTINGS_TYPE_FLOAT:  valueRef.pValue = (void*)&useroverride.value.f; break;
    case DD_SETTINGS_TYPE_STRING: valueRef.pValue = (void*)useroverride.value.s; break;
    }

    result = SetValue(valueRef.hash, valueRef);

    return result;
}
