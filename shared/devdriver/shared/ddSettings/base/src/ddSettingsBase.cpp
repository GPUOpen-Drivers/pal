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

#include "../inc/ddSettingsBase.h"
#include <ddCommon.h>

using namespace DevDriver;

namespace {

// ============================================================================
/// Generate 32-bit hash from the provided string.
///
/// The hash must match exactly what settings codegen script produces, because
/// that's what's used as key in settings map.
///
/// FNV1a hashing (http://www.isthe.com/chongo/tech/comp/fnv/) algorithm.
uint32_t HashString(const char* pStr, size_t strSize)
{
    DD_ASSERT((pStr != nullptr) && (strSize > 0));

    static constexpr uint32_t FnvPrime  = 16777619u;
    static constexpr uint32_t FnvOffset = 2166136261u;

    uint32_t hash = FnvOffset;

    for (uint32_t i = 0; i < strSize; i++)
    {
        hash ^= static_cast<uint32_t>(pStr[i]);
        hash *= FnvPrime;
    }

    return hash;
}

} // unnamed namespace

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

// ============================================================================
DD_RESULT SettingsBase::ApplyUserOverridesByComponent(
    const char* pComponentName)
{
    DD_RESULT result = DD_RESULT_SUCCESS;
    int64_t uservalueAppliedCount = 0;
    int64_t uservalueCount = 0;

    SettingsUserOverrideIter userOverrideIter =
        m_useroverrides.GetUserOverridesIter(pComponentName);
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

            SettingValue valueRef = { useroverride.type, nullptr, useroverride.size };
            switch (valueRef.type)
            {
            case SettingsType::Boolean:
            {
                valueRef.pValuePtr = &useroverride.val.b;
            } break;
            case SettingsType::Int8:
            {
                valueRef.pValuePtr = &useroverride.val.i8;
            } break;
            case SettingsType::Uint8:
            {
                valueRef.pValuePtr = &useroverride.val.u8;
            } break;
            case SettingsType::Int16:
            {
                valueRef.pValuePtr = &useroverride.val.i16;
            } break;
            case SettingsType::Uint16:
            {
                valueRef.pValuePtr = &useroverride.val.u16;
            } break;
            case SettingsType::Int:
            {
                valueRef.pValuePtr = &useroverride.val.i32;
            } break;
            case SettingsType::Uint:
            {
                valueRef.pValuePtr = &useroverride.val.u32;
            } break;
            case SettingsType::Int64:
            {
                valueRef.pValuePtr = &useroverride.val.i64;
            } break;
            case SettingsType::Uint64:
            {
                valueRef.pValuePtr = &useroverride.val.u64;
            } break;
            case SettingsType::Float:
            {
                valueRef.pValuePtr = &useroverride.val.f;
            } break;
            case SettingsType::String:
            {
                valueRef.pValuePtr = (void*)useroverride.val.s;
            } break;
            }

            uint32_t hash = HashString(useroverride.pName, useroverride.nameLength);
            DD_RESULT setResult = DevDriverToDDResult(SetValue(hash, valueRef, this));
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
