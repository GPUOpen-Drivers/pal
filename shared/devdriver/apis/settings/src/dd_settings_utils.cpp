/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include <dd_settings_utils.h>
#include <dd_settings_blob.h>
#include <unordered_map>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpragmas"
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#elif defined(__GNUC__)
#pragma GCC   diagnostic push
#pragma GCC   diagnostic ignored "-Wpragmas"
#endif

#include "rapidjson/document.h"
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace DevDriver
{
namespace SettingsUtils
{

static DD_SETTINGS_TYPE SettingTypeFromString(const std::string& typeStr)
{
    static const std::unordered_map<std::string, DD_SETTINGS_TYPE> kSettingsTypeMappings = { {"bool", DD_SETTINGS_TYPE_BOOL},
                                                                                             {"float", DD_SETTINGS_TYPE_FLOAT},
                                                                                             {"int8", DD_SETTINGS_TYPE_INT8},
                                                                                             {"int16", DD_SETTINGS_TYPE_INT16},
                                                                                             {"int32", DD_SETTINGS_TYPE_INT32},
                                                                                             {"int64", DD_SETTINGS_TYPE_INT64},
                                                                                             {"uint8", DD_SETTINGS_TYPE_UINT8},
                                                                                             {"uint16", DD_SETTINGS_TYPE_UINT16},
                                                                                             {"uint32", DD_SETTINGS_TYPE_UINT32},
                                                                                             {"uint64", DD_SETTINGS_TYPE_UINT64},
                                                                                             {"string", DD_SETTINGS_TYPE_STRING},
                                                                                             {"enum", DD_SETTINGS_TYPE_UINT32} };

    if (kSettingsTypeMappings.count(typeStr) == 0)
    {
        printf("Invalid type found = %s\n", typeStr.c_str());
        return DD_SETTINGS_TYPE_BOOL;
    }

    return kSettingsTypeMappings.at(typeStr);
}

void FillSettingsValue(SettingValue* pValue, DD_SETTINGS_TYPE type, rapidjson::Value::ConstValueIterator itr)
{
    if (itr->HasMember("Defaults"))
    {
        const auto defaultsField = itr->FindMember("Defaults");
        const auto defaultValField = defaultsField->value.FindMember("Default");
        switch (type)
        {
            case DD_SETTINGS_TYPE_BOOL:
                pValue->numVal.b = defaultValField->value.GetBool();
                break;
            case DD_SETTINGS_TYPE_INT8:
                pValue->numVal.i8 = static_cast<int8_t>(defaultValField->value.GetInt());
                break;
            case DD_SETTINGS_TYPE_UINT8:
                pValue->numVal.u8 = static_cast<uint8_t>(defaultValField->value.GetUint());
                break;
            case DD_SETTINGS_TYPE_INT16:
                pValue->numVal.i16 = static_cast<int16_t>(defaultValField->value.GetInt());
                break;
            case DD_SETTINGS_TYPE_UINT16:
                pValue->numVal.u16 = static_cast<uint16_t>(defaultValField->value.GetUint());
                break;
            case DD_SETTINGS_TYPE_INT32:
                pValue->numVal.i32 = defaultValField->value.GetInt();
                break;
            case DD_SETTINGS_TYPE_UINT32:
                pValue->numVal.u32 = 0;
                if (defaultValField->value.IsUint())
                {
                    pValue->numVal.u32 = defaultValField->value.GetUint();
                }
                else if (defaultValField->value.IsString())
                {
                    // Todo: Properly handle strings like "0xFFFF"
                    // For example "PrimCompressionFlags" in DXCP
                }
                else if (defaultValField->value.IsInt())
                {
                    // Get as an int to workaround default of -1
                    pValue->numVal.u32 = defaultValField->value.GetInt();
                }
                break;
            case DD_SETTINGS_TYPE_INT64:
                pValue->numVal.i64 = defaultValField->value.GetInt64();
                break;
            case DD_SETTINGS_TYPE_UINT64:
                pValue->numVal.u64 = defaultValField->value.GetUint64();
                break;
            case DD_SETTINGS_TYPE_FLOAT:
                pValue->numVal.f = defaultValField->value.GetFloat();
                break;
            case DD_SETTINGS_TYPE_STRING:
                pValue->strVal = defaultValField->value.GetString();
                break;
            default:
                printf("Invalid Type\n");
        }
    }
    else
    {
        // If no default is present, it is assumed to be an optional
        pValue->isOptional = true;
    }
}

void UpdateSetting(rapidjson::Value::ConstValueIterator itr, SettingsData* pData)
{
    const auto nameField = itr->FindMember("Name");
    pData->name = nameField->value.GetString();

    const auto descriptionField = itr->FindMember("Description");
    pData->description = descriptionField->value.GetString();

    const auto nameHashField = itr->FindMember("NameHash");
    pData->nameHash = nameHashField->value.GetUint();

    const auto typeField = itr->FindMember("Type");
    pData->type = SettingTypeFromString(typeField->value.GetString());

    FillSettingsValue(&pData->value, pData->type, itr);
}

DD_RESULT ParseSettingsBlobs(const char* pBlobBuffer, size_t bufferSize, std::vector<SettingComponent>& output)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((pBlobBuffer != nullptr) && (bufferSize >0))
    {
        // The settings blob all header starts after the settings path size and path:
        const uint16_t    offset                 = *reinterpret_cast<const uint16_t*>(pBlobBuffer);
        const SettingsBlobsAll* pSettingsBlobAllHeader = reinterpret_cast<const SettingsBlobsAll*>(pBlobBuffer + offset + 2);

        if (pSettingsBlobAllHeader->nblobs > 0)
        {
            // Skip past the header to get the first blob:
            const SettingsBlob* pBlob = reinterpret_cast<const SettingsBlob*>(pSettingsBlobAllHeader + 1);
            for (uint32_t blob = 0; blob < pSettingsBlobAllHeader->nblobs; ++blob)
            {
                if (pBlob->blobSize == 0)
                {
                    continue;
                }

                rapidjson::Document document;
                document.Parse((const char*)pBlob->blob, pBlob->blobSize);

                const rapidjson::Value& componentName = document["ComponentName"];
                SettingComponent component = {};
                if (componentName.IsString())
                {
                    component.name = componentName.GetString();
                }

                const rapidjson::Value& settings = document["Settings"];

                rapidjson::Value::ConstValueIterator itr = settings.Begin();
                for (; itr != settings.End(); ++itr)
                {
                    if (itr->HasMember("Structure") == false)
                    {
                        SettingsData setting = {};
                        UpdateSetting(itr, &setting);
                        component.settings.push_back(setting);
                    }
                    else
                    {
                        // For structures, each member has its own hash, value, etc
                        // @ToDo: Should we group these some how?
                        const rapidjson::Value& strct = itr->FindMember("Structure")->value;
                        auto strItr = strct.GetArray().Begin();
                        const auto nameField = itr->FindMember("Name");
                        std::string structName = nameField->value.GetString();
                        for (; strItr != strct.End(); strItr++)
                        {
                            SettingsData setting = {};
                            setting.structName = structName;
                            UpdateSetting(strItr, &setting);
                            component.settings.push_back(setting);
                        }
                    }

                }
                pBlob = reinterpret_cast<const DevDriver::SettingsBlob*>(reinterpret_cast<const uint8_t*>(pBlob) + pBlob->size);
                output.push_back(component);
            }

            result = DD_RESULT_SUCCESS;
        }
        else
        {
            printf("Driver settings were successfully queried but the queried data didn't contain any blobs.");
            result = DD_RESULT_SETTINGS_SERVICE_INVALID_SETTING_DATA;
        }
    }

    return result;
}

} // SettingsUtils namespace

} // DevDriver namespace
