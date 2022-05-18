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
#include "settingsParsingUtils.h"
#include "libyamlUtils.h"
#include <stdio.h>
#include <ddCommon.h>
#include <ddPlatform.h>
#include <protocols/ddSettingsServiceTypes.h>
#include <yaml.h>

namespace
{

using SettingValue = DevDriver::SettingsURIService::SettingValue;
using SettingsType = DevDriver::SettingsURIService::SettingType;

template<typename T>
SettingsType SettingsTypeSelector();

template<>
SettingsType SettingsTypeSelector<bool>()
{
    return SettingsType::Boolean;
}

template<>
SettingsType SettingsTypeSelector<int8_t>()
{
    return SettingsType::Int8;
}

template<>
SettingsType SettingsTypeSelector<uint8_t>()
{
    return SettingsType::Uint8;
}

template<>
SettingsType SettingsTypeSelector<int16_t>()
{
    return SettingsType::Int16;
}

template<>
SettingsType SettingsTypeSelector<uint16_t>()
{
    return SettingsType::Uint16;
}

template<>
SettingsType SettingsTypeSelector<int32_t>()
{
    return SettingsType::Int;
}

template<>
SettingsType SettingsTypeSelector<uint32_t>()
{
    return SettingsType::Uint;
}

template<>
SettingsType SettingsTypeSelector<int64_t>()
{
    return SettingsType::Int64;
}

template<>
SettingsType SettingsTypeSelector<uint64_t>()
{
    return SettingsType::Uint64;
}

template<>
SettingsType SettingsTypeSelector<float>()
{
    return SettingsType::Float;
}

// ============================================================================
/// Fill `pOutUserOverride` based on a YAML node.
template<typename T>
bool SetUserOverrideValueFromYamlNode(
    yaml_node_t* pValNode,
    DevDriver::SettingsUserOverride* pOutUserOverride)
{
    using namespace DevDriver;

    pOutUserOverride->type = SettingsTypeSelector<T>();
    pOutUserOverride->size = sizeof(T);

    bool result = false;
    T value = 0;
    if (YamlNodeGetScalar(pValNode, &value))
    {
        pOutUserOverride->SetVal(value);
        result = true;
    }
    else
    {
        result = false;
        // TODO: log error
    }

    return result;
}

// ============================================================================
/// Get the YAML node that represents a sequence of user-overrides by component name.
yaml_node_t* GetUserOverridesNodeByComponentName(
    yaml_document_t* pDoc,
    const char* pComponentName)
{
    using namespace DevDriver;

    yaml_node_t* pUserOverridesNode = nullptr;

    yaml_node_t* pRoot = yaml_document_get_root_node(pDoc);
    if (pRoot)
    {
        yaml_node_t* pComponentsNode = YamlDocumentFindNodeByKey(pDoc, pRoot, "Components");
        if (pComponentsNode && (pComponentsNode->type == YAML_SEQUENCE_NODE))
        {
            for (yaml_node_item_t* pItem = pComponentsNode->data.sequence.items.start;
                pItem < pComponentsNode->data.sequence.items.top;
                pItem++)
            {
                yaml_node_t* pCompNode = yaml_document_get_node(pDoc, *pItem);
                DD_ASSERT(pCompNode != nullptr);

                if (pCompNode->type != YAML_MAPPING_NODE)
                {
                    // log error
                    continue;
                }

                yaml_node_t* pNameNode = YamlDocumentFindNodeByKey(pDoc, pCompNode, "Name");
                if (!pNameNode || (pNameNode->type == YAML_SCALAR_NODE))
                {
                    // log error
                    continue;
                }

                if (strncmp((const char*)pNameNode->data.scalar.value,
                    pComponentName, pNameNode->data.scalar.length) != 0)
                {
                    continue;
                }

                pUserOverridesNode =
                    YamlDocumentFindNodeByKey(pDoc, pCompNode, "UserOverrides");
            }
        }
    }

    return pUserOverridesNode;
}

// ============================================================================
DD_RESULT GetUserOverride(
    yaml_document_t* pDoc,
    yaml_node_t* pUserOverrideNode,
    DevDriver::SettingsUserOverride* pOutUserOverride)
{
    using namespace DevDriver;

    DD_RESULT result = DD_RESULT_SUCCESS;

    yaml_node_t* pNameNode = YamlDocumentFindNodeByKey(pDoc, pUserOverrideNode, "Name");
    if (pNameNode)
    {
        if (pNameNode->type == YAML_SCALAR_NODE)
        {
            if (pNameNode->data.scalar.length > 0)
            {
                pOutUserOverride->pName = (const char*)pNameNode->data.scalar.value;
                pOutUserOverride->nameLength = pNameNode->data.scalar.length;
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
    }
    else
    {
        result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
    }

    const char* pTypeStr = nullptr;
    size_t typeStrLen = 0;
    if (result == DD_RESULT_SUCCESS)
    {
        yaml_node_t* pTypeNode = YamlDocumentFindNodeByKey(pDoc, pUserOverrideNode, "Type");
        if (pTypeNode && (pTypeNode->type == YAML_SCALAR_NODE))
        {
            pTypeStr = (const char*)pTypeNode->data.scalar.value;
            typeStrLen = pTypeNode->data.scalar.length;
        }
        else
        {
            result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
        }
    }

    yaml_node_t* pValueNode = nullptr;
    if (result == DD_RESULT_SUCCESS)
    {
        pValueNode = YamlDocumentFindNodeByKey(pDoc, pUserOverrideNode, "Value");
        if (!pValueNode || pValueNode->type != YAML_SCALAR_NODE)
        {
            result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
        }
    }

    if (result == DD_RESULT_SUCCESS)
    {
        if (strncmp(pTypeStr, "Bool", typeStrLen) == 0)
        {
            if (!SetUserOverrideValueFromYamlNode<bool>(pValueNode, pOutUserOverride))
            {
                // TODO: log error
                result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
            }
        }
        else if (strncmp(pTypeStr, "Int8", typeStrLen) == 0)
        {
            if (!SetUserOverrideValueFromYamlNode<int8_t>(pValueNode, pOutUserOverride))
            {
                // TODO: log error
                result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
            }
        }
        else if (strncmp(pTypeStr, "Uint8", typeStrLen) == 0)
        {
            if (!SetUserOverrideValueFromYamlNode<uint8_t>(pValueNode, pOutUserOverride))
            {
                // TODO: log error
                result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
            }
        }
        else if (strncmp(pTypeStr, "Int16", typeStrLen) == 0)
        {
            if (!SetUserOverrideValueFromYamlNode<int16_t>(pValueNode, pOutUserOverride))
            {
                // TODO: log error
                result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
            }
        }
        else if (strncmp(pTypeStr, "Uint16", typeStrLen) == 0)
        {
            if (!SetUserOverrideValueFromYamlNode<uint16_t>(pValueNode, pOutUserOverride))
            {
                // TODO: log error
                result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
            }
        }
        else if (strncmp(pTypeStr, "Int32", typeStrLen) == 0)
        {
            if (!SetUserOverrideValueFromYamlNode<int32_t>(pValueNode, pOutUserOverride))
            {
                // TODO: log error
                result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
            }
        }
        else if (strncmp(pTypeStr, "Uint32", typeStrLen) == 0)
        {
            if (!SetUserOverrideValueFromYamlNode<uint32_t>(pValueNode, pOutUserOverride))
            {
                // TODO: log error
                result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
            }
        }
        else if (strncmp(pTypeStr, "Int64", typeStrLen) == 0)
        {
            if (!SetUserOverrideValueFromYamlNode<int64_t>(pValueNode, pOutUserOverride))
            {
                // TODO: log error
                result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
            }
        }
        else if (strncmp(pTypeStr, "Uint64", typeStrLen) == 0)
        {
            if (!SetUserOverrideValueFromYamlNode<uint64_t>(pValueNode, pOutUserOverride))
            {
                // TODO: log error
                result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
            }
        }
        else if (strncmp(pTypeStr, "Float", typeStrLen) == 0)
        {
            if (!SetUserOverrideValueFromYamlNode<float>(pValueNode, pOutUserOverride))
            {
                // TODO: log error
                result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
            }
        }
        else if (strncmp(pTypeStr, "String", typeStrLen) == 0)
        {
            pOutUserOverride->type = SettingsType::String;
            pOutUserOverride->size = pValueNode->data.scalar.length;
            pOutUserOverride->val.s = (const char*)pValueNode->data.scalar.value;
        }
        else
        {
            // TODO: log error
            result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
        }
    }
    else
    {
        // TODO: log error
        result = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
    }

    pOutUserOverride->isValid = (result == DD_RESULT_SUCCESS);

    return result;
}

} // unnamed namespace

// ============================================================================
namespace DevDriver
{

// ============================================================================
SettingsUserOverride SettingsUserOverrideIter::Next()
{
    SettingsUserOverride useroverride = {};
    useroverride.isValid = false;

    if (IsValid())
    {
        yaml_document_t* pDoc = (yaml_document_t*)m_pDoc;
        yaml_node_t* pUserOverridesNode = (yaml_node_t*)m_pUserOverridesNode;
        yaml_node_item_t* pUserOverrideNodeIndex = (yaml_node_item_t*)m_pUserOverrideNodeIndex;

        if (pUserOverrideNodeIndex < pUserOverridesNode->data.sequence.items.top)
        {
            yaml_node_t* pUserOverrideNode =
                yaml_document_get_node(pDoc, *pUserOverrideNodeIndex);

            if (pUserOverrideNode && pUserOverrideNode->type == YAML_MAPPING_NODE)
            {
                DD_RESULT result =
                    GetUserOverride(pDoc, pUserOverrideNode, &useroverride);
                if (result == DD_RESULT_SUCCESS)
                {
                    useroverride.isValid = true;
                    pUserOverrideNodeIndex++;
                }
            }
        }
    }

    return useroverride;
}

// ============================================================================
SettingsConfig::SettingsConfig()
    : m_buffer(Platform::GenericAllocCb)
    , m_pParser(nullptr)
    , m_pDocument(nullptr)
    , m_valid(false)
{
    m_pParser = new yaml_parser_t({});
    m_pDocument = new yaml_document_t({});
}

// ============================================================================
SettingsConfig::~SettingsConfig()
{
    yaml_document_delete((yaml_document_t*)m_pDocument);
    yaml_parser_delete((yaml_parser_t*)m_pParser);

    delete (yaml_document_t*)m_pDocument;
    delete (yaml_parser_t*)m_pParser;
}

// ============================================================================
DD_RESULT SettingsConfig::Load(const char* pUseroverridesFilePath)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    FILE* pConfigFile = fopen(pUseroverridesFilePath, "rb");
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
                m_buffer.Resize(fileSize);
            }

            if (result == DD_RESULT_SUCCESS)
            {
                if (fseek(pConfigFile, 0L, SEEK_SET) == 0)
                {
                    fread(
                        m_buffer.Data(),
                        sizeof(char),
                        m_buffer.Size(),
                        pConfigFile
                    );

                    if (ferror(pConfigFile) != 0)
                    {
                        // TODO: log error
                        result = DD_RESULT_FS_UNKNOWN;
                    }
                }
                else
                {
                    // TODO: log error
                    result = DD_RESULT_FS_UNKNOWN;
                }
            }
        }
        else
        {
            result = DD_RESULT_FS_UNKNOWN;
        }

        fclose(pConfigFile);

        if (result == DD_RESULT_SUCCESS)
        {
            yaml_parser_t* pParser = (yaml_parser_t*)m_pParser;
            yaml_document_t* pDoc = (yaml_document_t*)m_pDocument;

            if (yaml_parser_initialize(pParser))
            {
                yaml_parser_set_input_string(pParser, (const unsigned char*)m_buffer.Data(), m_buffer.Size());
                if (yaml_parser_load(pParser, pDoc))
                {
                    yaml_node_t* pRoot = yaml_document_get_root_node(pDoc);
                    if (pRoot && pRoot->type == YAML_MAPPING_NODE)
                    {
                        yaml_node_t* pVersionNode = YamlDocumentFindNodeByKey(pDoc, pRoot, "Version");
                        if (!pVersionNode)
                        {
                            result = DD_RESULT_PARSING_INVALID_JSON;
                        }
                    }
                    else
                    {
                        result = DD_RESULT_PARSING_INVALID_JSON;
                    }
                }
                else
                {
                    result = DD_RESULT_PARSING_INVALID_JSON;
                }
            }
            else
            {
                result = DD_RESULT_PARSING_INVALID_JSON;
            }

        }
    }
    else
    {
        result = DD_RESULT_FS_NOT_FOUND;
    }

    m_valid = (result == DD_RESULT_SUCCESS);

    return result;
}

SettingsUserOverrideIter SettingsConfig::GetUserOverridesIter(const char* pComponentName)
{
    SettingsUserOverrideIter iter = {};

    if (m_valid)
    {
        yaml_document_t* pDoc = (yaml_document_t*)m_pDocument;

        yaml_node_t* pUserOverridesNode =
            GetUserOverridesNodeByComponentName(pDoc, pComponentName);

        if (pUserOverridesNode && pUserOverridesNode->type == YAML_SEQUENCE_NODE)
        {
            iter.m_pDoc = pDoc;
            iter.m_pUserOverridesNode = pUserOverridesNode;
            iter.m_pUserOverrideNodeIndex = pUserOverridesNode->data.sequence.items.start;
        }
    }

    return iter;
}

} // namespace DevDriver
