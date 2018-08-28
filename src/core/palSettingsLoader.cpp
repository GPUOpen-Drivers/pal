/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  palSettingsLoader.cpp
* @brief PAL settings loader utility class function definitions.
***********************************************************************************************************************
*/

#include "palSettingsLoader.h"
#include "palHashMapImpl.h"
#include "palUtil.h"

namespace Pal
{

// =====================================================================================================================
// Searches the ISettingsLoader info hash map for the provided hash, if found it will return the setting value data in
// the provided SettingValue pointer.  If the provided value memory is not big enough, this function will return an
// error and only update the valueSize. If the matching setting has provided a function pointer instead of a value
// location then that function will be called instead of copying the value.
DevDriver::Result ISettingsLoader::GetValue(
    SettingNameHash hash,
    SettingValue*   pSettingValue,
    void*           pPrivateData)
{
    DevDriver::Result ret = DevDriver::Result::SettingsUriInvalidSettingName;
    ISettingsLoader* pSettingsLoader = reinterpret_cast<ISettingsLoader*>(pPrivateData);
    auto pInfo = pSettingsLoader->m_settingsInfoMap.FindKey(hash);
    if (pInfo != nullptr)
    {
        if (pInfo->valueSize >= pSettingValue->valueSize)
        {
            pSettingValue->valueSize = pInfo->valueSize;
            ret = DevDriver::Result::SettingsUriInvalidSettingValueSize;
        }
        else
        {
            memcpy(pSettingValue->pValuePtr, pInfo->pValuePtr, pInfo->valueSize);
            pSettingValue->valueSize = pInfo->valueSize;
            pSettingValue->type = pInfo->type;
            ret = DevDriver::Result::Success;
        }
    }

    return ret;
}

// =====================================================================================================================
// Searches the ISettingsLoader info hash map for the provided hash, if found it will set value using the provided data.
// If the matching setting has provided a function pointer instead of a value location then that function will be called
// instead of copying the value.
DevDriver::Result ISettingsLoader::SetValue(
    SettingNameHash     hash,
    const SettingValue& settingValue,
    void*               pPrivateData)
{
    DevDriver::Result ret = DevDriver::Result::Unavailable;

    ISettingsLoader* pSettingsLoader = reinterpret_cast<ISettingsLoader*>(pPrivateData);
    // We currently only allow modifying of settings
    if (pSettingsLoader->IsSetValueAvailable())
    {
        auto pInfo = pSettingsLoader->m_settingsInfoMap.FindKey(hash);
        if (pInfo != nullptr)
        {
            if (pInfo->type == settingValue.type)
            {
                if (pInfo->valueSize >= settingValue.valueSize)
                {
                    memcpy(pInfo->pValuePtr, settingValue.pValuePtr, settingValue.valueSize);
                    ret = DevDriver::Result::Success;
                }
                else
                {
                    ret = DevDriver::Result::SettingsUriInvalidSettingValueSize;
                }
            }
            else
            {
                ret = DevDriver::Result::SettingsUriInvalidSettingValue;
            }
        }
        else
        {
            ret = DevDriver::Result::SettingsUriInvalidSettingName;
        }
    }

    return ret;
}
} // Pal
