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

#include "settingsBase.h"

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
