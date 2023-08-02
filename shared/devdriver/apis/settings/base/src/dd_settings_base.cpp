/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <dd_settings_base.h>
#include <string.h>
#include <util/hashMap.h>
#include <ddPlatform.h>

namespace DevDriver
{

using SettingsHashMap = HashMap<DD_SETTINGS_NAME_HASH, DDSettingsValueRef>;

SettingsBase::SettingsBase(SettingsDataBase* pSettingsData, uint32_t numSettings, size_t totalSettingsBytes)
    : m_pSettingsData(pSettingsData)
{
    // Zero out the entire SettingsData. This ensure the struct paddings
    // are always zero, and is required for generating deterministic hashing
    // result.
    memset(m_pSettingsData, 0, totalSettingsBytes);
    m_pSettingsData->numSettings = numSettings;

    auto pSettingsMap = new SettingsHashMap(DevDriver::Platform::GenericAllocCb);

    m_pSettingsMap = pSettingsMap;
}

SettingsBase::~SettingsBase()
{
    delete static_cast<SettingsHashMap*>(m_pSettingsMap);
}

DD_RESULT SettingsBase::SetValue(const DDSettingsValueRef& srcValueRef)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    auto pSettingsMap = static_cast<SettingsHashMap*>(m_pSettingsMap);
    DDSettingsValueRef* pDestValueRef = pSettingsMap->FindValue(srcValueRef.hash);
    if (pDestValueRef != nullptr)
    {
        if (pDestValueRef->type == srcValueRef.type)
        {
            bool set = CustomSetValue(srcValueRef);
            if (!set)
            {
                if (pDestValueRef->size >= srcValueRef.size)
                {
                    memcpy(pDestValueRef->pValue, srcValueRef.pValue, srcValueRef.size);
                }
                else
                {
                    result = DD_RESULT_COMMON_BUFFER_TOO_SMALL;
                }
            }
        }
        else
        {
            result = DD_RESULT_SETTINGS_TYPE_MISMATCH;
        }
    }
    else
    {
        result = DD_RESULT_SETTINGS_NOT_FOUND;
    }

    return result;
}

DD_RESULT SettingsBase::GetValue(DDSettingsValueRef* pValueRef)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    auto pSettingsMap = static_cast<SettingsHashMap*>(m_pSettingsMap);
    DDSettingsValueRef* pSrcValueRef = pSettingsMap->FindValue(pValueRef->hash);
    if (pSrcValueRef != nullptr)
    {
        if (pValueRef->size >= pSrcValueRef->size)
        {
            if (pValueRef->pValue != nullptr)
            {
                memcpy(pValueRef->pValue, pSrcValueRef->pValue, pSrcValueRef->size);
                pValueRef->type = pSrcValueRef->type;
            }
            else
            {
                result = DD_RESULT_COMMON_INVALID_PARAMETER;
            }
        }
        else
        {
            result = DD_RESULT_COMMON_BUFFER_TOO_SMALL;
        }
    }
    else
    {
        result = DD_RESULT_SETTINGS_NOT_FOUND;
    }

    return result;
}

} // namespace DevDriver
