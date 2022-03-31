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

#include "settingsConfig.h"
#include "settingsBase.h"
#include <ddCommon.h>
#include <stdio.h>
#include <rapidjson/document.h>

using namespace DevDriver;
using RjValue = rapidjson::Value;
using RjDocument = rapidjson::Document;

namespace
{

using SettingNameHash = SettingsURIService::SettingNameHash;
using SettingValue = SettingsURIService::SettingValue;
using SettingType = SettingsURIService::SettingType;

// ============================================================================
/// A helper union for storing setting values. We need an intermeidate place to
/// hold a non-string value copied from a JSON Value.
union SettingValueHelper
{
    bool        bVal;
    int8_t      i8Val;
    uint8_t     u8Val;
    int16_t     i16Val;
    uint16_t    u16Val;
    int32_t     i32Val;
    uint32_t    u32Val;
    int64_t     i64Val;
    uint64_t    u64Val;
    float       fVal;
    const char* sVal;
};

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

// ============================================================================
/// Get the JSON Value of the component by name.
const RjValue* GetComponentByName(
    const RjValue* pRoot,
    const char* pComponentName)
{
    const RjValue* pComponent = nullptr;

    const RjValue& components = (*pRoot)["Data"]["Components"];
    if (components.IsArray())
    {
        for (RjValue::ConstValueIterator compoItr = components.Begin();
             compoItr != components.End();
             ++compoItr)
        {
            if (!compoItr->IsObject())
            {
                // log error
                continue;
            }
            RjValue::ConstMemberIterator nameItr = compoItr->FindMember("Name");
            if (nameItr != compoItr->MemberEnd())
            {
                // log error
                continue;
            }
            if (!nameItr->value.IsString())
            {
                // log error
                continue;
            }
            if (nameItr->value.GetString() == nullptr)
            {
                // log error
                continue;
            }
            if (strcmp(pComponentName, nameItr->value.GetString()) == 0)
            {
                pComponent = compoItr;
                break;
            }
        }
    }
    else
    {
        // log error
    }

    return pComponent;
}

// ============================================================================
/// Get the name of a setting JSON Value.
const char* GetSettingName(const RjValue* pUserValue)
{
    const char* name = nullptr;

    RjValue::ConstMemberIterator itr = pUserValue->FindMember("Name");
    if (itr != pUserValue->MemberEnd())
    {
        if (itr->value.IsString())
        {
            name = itr->value.GetString();
        }
        else
        {
            // log error
        }
    }
    else
    {
        // log error
    }

    return name;
}

// ============================================================================
/// Get the value field of a setting JSON Value.
const RjValue* GetSettingValue(const RjValue* pUserValue)
{
    const RjValue* pValue = nullptr;

    RjValue::ConstMemberIterator itr = pUserValue->FindMember("Value");
    if (itr != pUserValue->MemberEnd())
    {
        pValue = &itr->value;
    }
    else
    {
        // log error
    }

    return pValue;
}

// ============================================================================
DD_RESULT GetSetting(
    const RjValue*      pUserValue,
    SettingType*        pOutType,
    uint32_t*           pOutSize,
    SettingValueHelper* pOutValue)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    RjValue::ConstMemberIterator itr = pUserValue->FindMember("Type");
    if (itr != pUserValue->MemberEnd())
    {
        if (itr->value.IsString())
        {
            const char* pJsonField_Type = itr->value.GetString();
            if (pJsonField_Type != nullptr)
            {
                if (strcmp("Bool", pJsonField_Type) == 0)
                {
                    *pOutType = SettingType::Boolean;
                    *pOutSize = sizeof(bool);
                    const RjValue* pValue = GetSettingValue(pUserValue);
                    if (pValue && pValue->IsBool())
                    {
                        pOutValue->bVal = pValue->GetBool();
                    }
                    else
                    {
                        // log error
                        result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
                    }
                }
                else if (strcmp("Int8", pJsonField_Type) == 0)
                {
                    *pOutType = SettingType::Int8;
                    *pOutSize = sizeof(int8_t);
                    const RjValue* pValue = GetSettingValue(pUserValue);
                    if (pValue && pValue->IsInt())
                    {
                        int val = pValue->GetInt();
                        if ((val < INT8_MIN) || (val > INT8_MAX))
                        {
                            // log warning
                        }
                        pOutValue->i8Val = (int8_t)val;
                    }
                    else
                    {
                        // log error
                        result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
                    }
                }
                else if (strcmp("Uint8", pJsonField_Type) == 0)
                {
                    *pOutType = SettingType::Uint8;
                    *pOutSize = sizeof(uint8_t);
                    const RjValue* pValue = GetSettingValue(pUserValue);
                    if (pValue && pValue->IsUint())
                    {
                        unsigned int val = pValue->GetUint();
                        if (val > UINT8_MAX)
                        {
                            // log warning
                        }
                        pOutValue->u8Val = (uint8)val;
                    }
                    else
                    {
                        // log error
                        result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
                    }
                }
                else if (strcmp("Int16", pJsonField_Type) == 0)
                {
                    *pOutType = SettingType::Int16;
                    *pOutSize = sizeof(int16_t);
                    const RjValue* pValue = GetSettingValue(pUserValue);
                    if (pValue && pValue->IsInt())
                    {
                        int val = pValue->GetInt();
                        if ((val < INT16_MIN) || (val > INT16_MAX))
                        {
                            // log warning
                        }
                        pOutValue->i16Val = (int16_t)val;
                    }
                    else
                    {
                        // log error
                        result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
                    }
                }
                else if (strcmp("Uint16", pJsonField_Type) == 0)
                {
                    *pOutType = SettingType::Uint16;
                    *pOutSize = sizeof(uint16_t);
                    const RjValue* pValue = GetSettingValue(pUserValue);
                    if (pValue && pValue->IsUint())
                    {
                        unsigned int val = pValue->GetUint();
                        if (val > UINT16_MAX)
                        {
                            // log warning
                        }
                        pOutValue->u16Val = (uint16_t)val;
                    }
                    else
                    {
                        // log error
                        result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
                    }
                }
                else if (strcmp("Int32", pJsonField_Type) == 0)
                {
                    *pOutType = SettingType::Int;
                    *pOutSize = sizeof(int32_t);
                    const RjValue* pValue = GetSettingValue(pUserValue);
                    if (pValue && pValue->IsInt())
                    {
                        pOutValue->i32Val = pValue->GetInt();
                    }
                    else
                    {
                        // log error
                        result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
                    }
                }
                else if (strcmp("Uint32", pJsonField_Type) == 0)
                {
                    *pOutType = SettingType::Uint;
                    *pOutSize = sizeof(uint32_t);
                    const RjValue* pValue = GetSettingValue(pUserValue);
                    if (pValue && pValue->IsUint())
                    {
                        pOutValue->u32Val = pValue->GetUint();
                    }
                    else
                    {
                        // log error
                        result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
                    }
                }
                else if (strcmp("Int64", pJsonField_Type) == 0)
                {
                    *pOutType = SettingType::Int64;
                    *pOutSize = sizeof(int64_t);
                    const RjValue* pValue = GetSettingValue(pUserValue);
                    if (pValue && pValue->IsInt64())
                    {
                        pOutValue->i64Val = pValue->GetInt64();
                    }
                    else
                    {
                        // log error
                        result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
                    }
                }
                else if (strcmp("Uint64", pJsonField_Type) == 0)
                {
                    *pOutType = SettingType::Uint64;
                    *pOutSize = sizeof(uint64_t);
                    const RjValue* pValue = GetSettingValue(pUserValue);
                    if (pValue && pValue->IsUint64())
                    {
                        pOutValue->u64Val = pValue->GetUint64();
                    }
                    else
                    {
                        // log error
                        result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
                    }
                }
                else if (strcmp("Float", pJsonField_Type) == 0)
                {
                    *pOutType = SettingType::Float;
                    *pOutSize = sizeof(float);
                    const RjValue* pValue = GetSettingValue(pUserValue);
                    if (pValue && pValue->IsFloat())
                    {
                        pOutValue->fVal = pValue->GetFloat();
                    }
                    else
                    {
                        // log error
                        result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
                    }
                }
                else if (strcmp("String", pJsonField_Type) == 0)
                {
                    *pOutType = SettingType::String;
                    // String length isn't known for now.
                    const RjValue* pValue = GetSettingValue(pUserValue);
                    if (pValue && pValue->IsString())
                    {
                        pOutValue->sVal = pValue->GetString();
                    }
                    size_t len = strlen(pOutValue->sVal) + 1; // plus one for null-terminator
                    DD_ASSERT(len <= UINT32_MAX);
                    *pOutSize = (uint32_t)len;
                }
                else
                {
                    // log error
                    result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
                }
            }
            else
            {
                // log error
                result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
            }
        }
        else
        {
            // log error
            result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
        }
    }
    else
    {
        // log error
        result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
    }

    return result;
}

// ============================================================================
DD_RESULT SetSettingValue(
    const char*    pSettingName,
    const RjValue* pJsonValue,
    void*          pSettingsBase)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    uint32_t hash = HashString(pSettingName, strlen(pSettingName));

    SettingType type = SettingType::Boolean;
    uint32_t size = 0;
    SettingValueHelper value = {0};
    DD_RESULT found = GetSetting(pJsonValue, &type, &size, &value);
    if (found != DD_RESULT_SUCCESS)
    {
        SettingValue valueRef = { type, nullptr, size };
        switch (type)
        {
        case SettingType::Boolean:
        {
            valueRef.pValuePtr = &value.bVal;
        } break;
        case SettingType::Int8:
        {
            valueRef.pValuePtr = &value.i8Val;
        } break;
        case SettingType::Uint8:
        {
            valueRef.pValuePtr = &value.u8Val;
        } break;
        case SettingType::Int16:
        {
            valueRef.pValuePtr = &value.i16Val;
        } break;
        case SettingType::Uint16:
        {
            valueRef.pValuePtr = &value.u16Val;
        } break;
        case SettingType::Int:
        {
            valueRef.pValuePtr = &value.i32Val;
        } break;
        case SettingType::Uint:
        {
            valueRef.pValuePtr = &value.u32Val;
        } break;
        case SettingType::Int64:
        {
            valueRef.pValuePtr = &value.i64Val;
        } break;
        case SettingType::Uint64:
        {
            valueRef.pValuePtr = &value.u64Val;
        } break;
        case SettingType::Float:
        {
            valueRef.pValuePtr = &value.fVal;
        } break;
        case SettingType::String:
        {
            valueRef.pValuePtr = (void*)value.sVal;
        } break;
        }

        result = DevDriverToDDResult(SettingsBase::SetValue(hash, valueRef, pSettingsBase));
    }
    else
    {
        // log error
        result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
    }

    return result;
}

} // unnamed namespace

// ============================================================================
SettingsConfig::SettingsConfig()
    : m_buffer(Platform::GenericAllocCb)
    , m_validData(false)
{
    m_pJson = new RjDocument();
}

// ============================================================================
SettingsConfig::~SettingsConfig()
{
    delete (RjDocument*)m_pJson;
}

// ============================================================================
/// Load and store the content of a Settings config file. The Settings
/// config file must conform to the second version of "Settings User Values
/// Export/Import Schema" described in settings_uservalues_schema.json.
DD_RESULT SettingsConfig::Load(const char* pJsonPath)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    FILE* pConfigFile = fopen(pJsonPath, "rb");
    if (pConfigFile)
    {
        if (fseek(pConfigFile, 0L, SEEK_END) == 0)
        {
            long fileSize = ftell(pConfigFile);
            if (fileSize == -1L)
            {
                result = DD_RESULT_FS_UNKNOWN;
            }
            else
            {
                m_buffer.Resize(fileSize + 1); // +1 for null-terminator
            }

            if (result == DD_RESULT_SUCCESS)
            {
                if (fseek(pConfigFile, 0L, SEEK_SET) == 0)
                {
                    size_t readLen = fread(
                        m_buffer.Data(),
                        sizeof(char),
                        m_buffer.Size(),
                        pConfigFile
                    );

                    if (ferror(pConfigFile) == 0)
                    {
                        m_buffer[readLen] = '\0';
                    }
                    else
                    {
                        // log error
                        result = DD_RESULT_FS_UNKNOWN;
                    }
                }
                else
                {
                    // log error
                    result = DD_RESULT_FS_UNKNOWN;
                }
            }
        }
        else
        {
            result = DD_RESULT_FS_UNKNOWN;
        }

        fclose(pConfigFile);

        RjDocument* pJsonDoc = (RjDocument*)m_pJson;

        if (result == DD_RESULT_SUCCESS)
        {
            pJsonDoc->ParseInsitu(m_buffer.Data());
            if (pJsonDoc->HasParseError())
            {
                // log error
                result = DD_RESULT_PARSING_INVALID_JSON;
            }

            // Check basic validity against Settings user-value export JSON schema.

            if (!pJsonDoc->IsObject())
            {
                // The root must be an object.
                // log error
                result = DD_RESULT_FS_INVALID_DATA;
            }
            else
            {
                RjValue::MemberIterator data = pJsonDoc->FindMember("Data");
                if (data == pJsonDoc->MemberEnd())
                {
                    // log error
                    result = DD_RESULT_FS_INVALID_DATA;
                }
                else
                {
                    if (!data->value.IsObject())
                    {
                        // log error
                        result = DD_RESULT_FS_INVALID_DATA;
                    }
                    else
                    {
                        RjValue::MemberIterator components = data->value.FindMember("Components");
                        if (components == pJsonDoc->MemberEnd())
                        {
                            // log error
                            result = DD_RESULT_FS_INVALID_DATA;
                        }
                        else
                        {
                            if (!components->value.IsArray())
                            {
                                // log error
                                result = DD_RESULT_FS_INVALID_DATA;
                            }
                        }
                    }
                }
            }
        }
    }
    else
    {
        result = DD_RESULT_FS_NOT_FOUND;
    }

    m_validData = (result == DD_RESULT_SUCCESS);

    return result;
}

// ============================================================================
/// Apply user-values of a specific component designated by name.
/// Return SUCCESS:
///     1) The specified component is not found.
///     2) The specified component found but doesn't contain any user-values.
///     3) All user-values in the specified component are applied.
/// Return SUCCESS_WITH_ERROR:
///     Some but not all user-values fail to be applied.
/// Return other errors:
///     All other cases.
DD_RESULT SettingsConfig::ApplyUserValuesByComponent(
    const char* pComponentName,
    void*       pSettingsBase)
{
    DD_RESULT result = DD_RESULT_SUCCESS;
    int64_t uservalueAppliedCount = 0;
    int64_t uservalueCount = 0;

    if (m_validData)
    {
        const RjValue* component =
            GetComponentByName((RjDocument*)m_pJson, pComponentName);

        if (component != nullptr)
        {
            if (component->IsArray())
            {
                for (RjValue::ConstValueIterator userValueItr = component->Begin();
                    userValueItr != component->End();
                    ++userValueItr)
                {
                    const char* pName = GetSettingName(userValueItr);
                    if (pName != nullptr)
                    {
                        uservalueCount += 1;
                        result = SetSettingValue(pName, userValueItr, pSettingsBase);
                        if (result == DD_RESULT_SUCCESS)
                        {
                            uservalueAppliedCount += 1;
                        }
                        else
                        {
                            // log error
                            result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
                        }
                    }
                    else
                    {
                        // log error
                        result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
                    }
                }
            }
            else
            {
                // log error
                result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
            }
        }
        else
        {
            // log info
        }
    }
    else
    {
        // log error
        result = DD_RESULT_UNKNOWN;
    }

    DD_ASSERT(uservalueAppliedCount <= uservalueCount);

    if ((uservalueAppliedCount > 0) && (uservalueAppliedCount < uservalueCount))
    {
        result = DD_RESULT_COMMON_SUCCESS_WITH_ERRORS;
    }

    return result;
}

// ============================================================================
/// Apply a single user-value by its setting name.
/// Return SUCCESS:
///     1) The specified user-value is not found.
///     2) The specified user-value is applied.
/// Otherwise return errors.
DD_RESULT SettingsConfig::ApplyUserValueByName(
    const char*   pSettingName,
    const char*   pComponentName,
    void*         pSettingsBase)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    if (m_validData)
    {
        const RjValue* component =
            GetComponentByName((RjDocument*)m_pJson, pComponentName);
        if (component->IsArray())
        {
            for (RjValue::ConstValueIterator userValueItr = component->Begin();
                 userValueItr != component->End();
                 ++userValueItr)
            {
                const char* pJsonField_Name = GetSettingName(userValueItr);
                if (pJsonField_Name == nullptr)
                {
                    // log error

                    // Something is wrong with the Settings config file. This
                    // uservalue might not be the one we're interested in. So
                    // don't set error result.
                }
                else
                {
                    if (strcmp(pJsonField_Name, pSettingName) == 0)
                    {
                        result = SetSettingValue(pSettingName, userValueItr, pSettingsBase);
                        if (result == DD_RESULT_SUCCESS)
                        {
                            break;
                        }
                    }
                }
            }
        }
        else
        {
            result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
        }
    }
    else
    {
        result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
    }

    return result;
}
