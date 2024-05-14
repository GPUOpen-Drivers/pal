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

#include <dd_settings_base.h>
#include <dd_settings_rpc_types.h>
#include <dd_optional.h>

#include <cstring>
#include <limits>

namespace
{

template<typename T>
void SetSetting(void* pSetting, void* pValue)
{
    *(static_cast<T*>(pSetting)) = *(static_cast<T*>(pValue));
}

template<typename T>
void SetOptionalSetting(void* pSetting, void* pValue)
{
    *(static_cast<DevDriver::Optional<T>*>(pSetting)) = *(static_cast<T*>(pValue));
}

inline void SetValueHelper(DDSettingsValueRef* pDestValueRef, const DDSettingsValueRef& pSrcValueRef)
{
    if (pDestValueRef->isOptional)
    {
        switch (pDestValueRef->type)
        {
        case DD_SETTINGS_TYPE_BOOL:   SetOptionalSetting<bool>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_INT8:   SetOptionalSetting<int8_t>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_UINT8:  SetOptionalSetting<uint8_t>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_INT16:  SetOptionalSetting<int16_t>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_UINT16: SetOptionalSetting<uint16_t>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_INT32:  SetOptionalSetting<int32_t>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_UINT32: SetOptionalSetting<uint32_t>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_INT64:  SetOptionalSetting<int64_t>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_UINT64: SetOptionalSetting<uint64_t>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_FLOAT:  SetOptionalSetting<float>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_STRING: memcpy(pDestValueRef->pValue, pSrcValueRef.pValue, pSrcValueRef.size); break;
        default:
            DD_ASSERT(false);
        }
    }
    else
    {
        switch (pDestValueRef->type)
        {
        case DD_SETTINGS_TYPE_BOOL:   SetSetting<bool>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_INT8:   SetSetting<int8_t>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_UINT8:  SetSetting<uint8_t>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_INT16:  SetSetting<int16_t>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_UINT16: SetSetting<uint16_t>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_INT32:  SetSetting<int32_t>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_UINT32: SetSetting<uint32_t>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_INT64:  SetSetting<int64_t>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_UINT64: SetSetting<uint64_t>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_FLOAT:  SetSetting<float>(pDestValueRef->pValue, pSrcValueRef.pValue); break;
        case DD_SETTINGS_TYPE_STRING: memcpy(pDestValueRef->pValue, pSrcValueRef.pValue, pSrcValueRef.size); break;
        default:
            DD_ASSERT(false);
        }
    }
}

template<typename T>
const void* OptionalInnerValueAddrHelper(const DevDriver::Optional<T>* pOptional)
{
    return pOptional->HasValue() ? &(pOptional->Value()) : nullptr;
}

template<typename R, typename T>
R SafeUIntCast(T u)
{
    DD_ASSERT(u <= std::numeric_limits<R>::max());
    return static_cast<R>(u);
}

} // anonymous namespace

namespace DevDriver
{

SettingsBase::SettingsBase(void* pSettingsData, size_t settingsDataSize)
    : m_pSettingsData(pSettingsData)
    , m_settingsMap(Platform::GenericAllocCb),
      m_unsupportedExperiments(Platform::GenericAllocCb)
{
    // Zero out the entire SettingsData. This ensure the struct paddings
    // are always zero, and is required for generating deterministic hashing
    // result.
    memset(m_pSettingsData, 0, settingsDataSize);
}

SettingsBase::~SettingsBase()
{
}

DD_RESULT SettingsBase::SetValue(const DDSettingsValueRef& srcValueRef)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    DDSettingsValueRef* pDestValueRef = m_settingsMap.FindValue(srcValueRef.hash);
    if (pDestValueRef != nullptr)
    {
        if (pDestValueRef->type == srcValueRef.type)
        {
            bool set = CustomSetValue(srcValueRef);
            if (!set)
            {
                // For settings of static char array, its size is the size of the array and could be
                // bigger than that of the length of the source string.
                if (pDestValueRef->size >= srcValueRef.size)
                {
                    SetValueHelper(pDestValueRef, srcValueRef);
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

    DDSettingsValueRef* pSrcValueRef = m_settingsMap.FindValue(pValueRef->hash);
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

DD_RESULT SettingsBase::GetAllValues(DynamicBuffer& recvBuffer, size_t* pOutNumValues)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    uint32_t numValues = 0;

    for (const auto& entry : m_settingsMap)
    {
        DDSettingsValueHeader valueHeader{};
        valueHeader.hash = entry.key;
        valueHeader.type = entry.value.type;

        const void* pSrcValueBuf = nullptr;

        if (entry.value.type == DD_SETTINGS_TYPE_STRING)
        {
            const char* pStrValue = static_cast<const char*>(entry.value.pValue);
            if (pStrValue[0] != '\0')
            {
                pSrcValueBuf          = pStrValue;
                valueHeader.valueSize = SafeUIntCast<uint16_t, size_t>(strnlen(pStrValue, entry.value.size) + 1);

                DD_ASSERT(entry.value.size >= valueHeader.valueSize);
            }
            else
            {
                pSrcValueBuf           = nullptr;
                valueHeader.valueSize  = 0;
            }
        }
        else
        {
            if (entry.value.isOptional)
            {
                pSrcValueBuf = OptionalInnerValueAddr(entry.value);
            }
            else
            {
                pSrcValueBuf = entry.value.pValue;
            }
            valueHeader.valueSize = entry.value.size;
        }

        if (pSrcValueBuf)
        {
            recvBuffer.Copy(&valueHeader, sizeof(valueHeader));
            recvBuffer.Copy(pSrcValueBuf, valueHeader.valueSize);

            result = recvBuffer.Error();
            if (result == DD_RESULT_SUCCESS)
            {
                numValues += 1;
            }
            else
            {
                break;
            }
        }
    }

    if (result == DD_RESULT_SUCCESS)
    {
        *pOutNumValues = numValues;
    }

    return result;
}

DD_RESULT SettingsBase::GetUnsupportedExperiments(DynamicBuffer& recvBuffer, size_t* pOutNumValues)
{
    DD_RESULT result     = DD_RESULT_COMMON_INVALID_PARAMETER;
    size_t    sizeNeeded = sizeof(DD_SETTINGS_NAME_HASH) * m_unsupportedExperiments.Size();
    if ((pOutNumValues != nullptr) && (recvBuffer.Capacity() >= sizeNeeded))
    {
        *pOutNumValues = m_unsupportedExperiments.Size();
        recvBuffer.Copy(m_unsupportedExperiments.Data(), sizeNeeded);
        result = DD_RESULT_SUCCESS;
    }

    return result;
}

const void* SettingsBase::OptionalInnerValueAddr(const DDSettingsValueRef& valueRef)
{
    switch (valueRef.type)
    {
    case DD_SETTINGS_TYPE_BOOL:   return OptionalInnerValueAddrHelper(static_cast<const Optional<bool>*>(valueRef.pValue));
    case DD_SETTINGS_TYPE_INT8:   return OptionalInnerValueAddrHelper(static_cast<const Optional<int8_t>*>(valueRef.pValue));
    case DD_SETTINGS_TYPE_UINT8:  return OptionalInnerValueAddrHelper(static_cast<const Optional<uint8_t>*>(valueRef.pValue));
    case DD_SETTINGS_TYPE_INT16:  return OptionalInnerValueAddrHelper(static_cast<const Optional<int16_t>*>(valueRef.pValue));
    case DD_SETTINGS_TYPE_UINT16: return OptionalInnerValueAddrHelper(static_cast<const Optional<uint16_t>*>(valueRef.pValue));
    case DD_SETTINGS_TYPE_INT32:  return OptionalInnerValueAddrHelper(static_cast<const Optional<int32_t>*>(valueRef.pValue));
    case DD_SETTINGS_TYPE_UINT32: return OptionalInnerValueAddrHelper(static_cast<const Optional<uint32_t>*>(valueRef.pValue));
    case DD_SETTINGS_TYPE_INT64:  return OptionalInnerValueAddrHelper(static_cast<const Optional<int64_t>*>(valueRef.pValue));
    case DD_SETTINGS_TYPE_UINT64: return OptionalInnerValueAddrHelper(static_cast<const Optional<uint64_t>*>(valueRef.pValue));
    case DD_SETTINGS_TYPE_FLOAT:  return OptionalInnerValueAddrHelper(static_cast<const Optional<float>*>(valueRef.pValue));
    case DD_SETTINGS_TYPE_STRING: return (static_cast<const char*>(valueRef.pValue)[0] != '\0') ? valueRef.pValue : nullptr;
    default:                      DD_ASSERT(false); return nullptr;
    }
}

} // namespace DevDriver
