/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <ddYaml.h>
#include <ddPlatform.h>
#include <limits>
#include <cstring>
#include <cstdlib>

namespace
{

/// Convert a string to uint64_t value. Return true if conversion succeeded.
bool StrToUll(const char* pStr, uint64_t* pOutVal)
{
    bool success = false;

    char* pEnd = nullptr;
    uint64_t val = strtoull(pStr, &pEnd, 0);
    if (pEnd != pStr)
    {
        *pOutVal = val;
        success = true;
    }

    return success;
}

/// Convert a string to int64_t value. Return true if conversion succeeded.
bool StrToLl(const char* pStr, int64_t* pOutVal)
{
    bool success = false;

    char* pEnd = nullptr;
    int64_t val = strtoll(pStr, &pEnd, 0);
    if (pEnd != pStr)
    {
        *pOutVal = val;
        success = true;
    }

    return success;
}

template<typename IntT>
bool YamlNodeGetInt(yaml_node_t* pValNode, IntT* pOutValue)
{
    *pOutValue = 0;
    bool success = false;
    if (pValNode->type == YAML_SCALAR_NODE)
    {
        int64_t number = 0;
        if (StrToLl((const char*)pValNode->data.scalar.value, &number))
        {
            if ((number >= std::numeric_limits<IntT>::min()) &&
                (number <= std::numeric_limits<IntT>::max()))
            {
                *pOutValue = static_cast<IntT>(number);
                success = true;
            }
        }
    }
    return success;
}

template<typename UintT>
bool YamlNodeGetUint(yaml_node_t* pValNode, UintT* pOutValue)
{
    *pOutValue = 0;
    bool success = false;
    if (pValNode->type == YAML_SCALAR_NODE)
    {
        uint64_t number = 0;
        if (StrToUll((const char*)pValNode->data.scalar.value, &number))
        {
            if (number <= std::numeric_limits<UintT>::max())
            {
                *pOutValue = static_cast<UintT>(number);
                success = true;
            }
        }
    }
    return success;
}

} // unnamed namespace

namespace DevDriver
{

yaml_node_t* YamlDocumentFindNodeByKey(
    yaml_document_t* pDoc,
    yaml_node_t* pParent,
    const char* pKey)
{
    yaml_node_t* pResult = nullptr;

    if (pParent->type == YAML_MAPPING_NODE)
    {
        for (yaml_node_pair_t* pPair = pParent->data.mapping.pairs.start;
            pPair < pParent->data.mapping.pairs.top;
            pPair++)
        {
            yaml_node_t* pKeyNode = yaml_document_get_node(pDoc, pPair->key);
            if (pKeyNode != nullptr)
            {
                DD_ASSERT(pKeyNode->type == YAML_SCALAR_NODE);
                if (strcmp((const char*)pKeyNode->data.scalar.value, pKey) == 0)
                {
                    yaml_node_t* pValNode = yaml_document_get_node(pDoc, pPair->value);
                    DD_ASSERT(pValNode != nullptr);
                    pResult = pValNode;
                    break;
                }
            }
        }
    }
    else
    {
        // log error
    }

    return pResult;
}

bool YamlNodeGetScalar(yaml_node_t* pValNode, bool* pOutValue)
{
    bool success = false;
    if (pValNode->type == YAML_SCALAR_NODE)
    {
        if (strncmp((const char*)pValNode->data.scalar.value,
            "false", pValNode->data.scalar.length) == 0)
        {
            *pOutValue = false;
        }
        else if (strncmp((const char*)pValNode->data.scalar.value,
            "true", pValNode->data.scalar.length) == 0)
        {
            *pOutValue = true;
        }
        else
        {
            success = false;
        }
    }

    return success;
}

bool YamlNodeGetScalar(yaml_node_t* pValNode, int8_t* pOutValue)
{
    return YamlNodeGetInt(pValNode, pOutValue);
}

bool YamlNodeGetScalar(yaml_node_t* pValNode, uint8_t* pOutValue)
{
    return YamlNodeGetUint(pValNode, pOutValue);
}

bool YamlNodeGetScalar(yaml_node_t* pValNode, int16_t* pOutValue)
{
    return YamlNodeGetInt(pValNode, pOutValue);
}

bool YamlNodeGetScalar(yaml_node_t* pValNode, uint16_t* pOutValue)
{
    return YamlNodeGetUint(pValNode, pOutValue);
}

bool YamlNodeGetScalar(yaml_node_t* pValNode, int32_t* pOutValue)
{
    return YamlNodeGetInt(pValNode, pOutValue);
}

bool YamlNodeGetScalar(yaml_node_t* pValNode, uint32_t* pOutValue)
{
    return YamlNodeGetUint(pValNode, pOutValue);
}

bool YamlNodeGetScalar(yaml_node_t* pValNode, int64_t* pOutValue)
{
    return YamlNodeGetInt(pValNode, pOutValue);
}

bool YamlNodeGetScalar(yaml_node_t* pValNode, uint64_t* pOutValue)
{
    return YamlNodeGetUint(pValNode, pOutValue);
}

bool YamlNodeGetScalar(yaml_node_t* pValNode, float* pOutValue)
{
    bool success = false;
    if (pValNode->type == YAML_SCALAR_NODE)
    {
        const char* pStr = (const char*)pValNode->data.scalar.value;
        char* pEnd = nullptr;
        float number = strtof(pStr, &pEnd);
        if ((pEnd != pStr))
        {
            *pOutValue = number;
            success = true;
        }
    }
    return success;;
}

} // namespace DevDriver
