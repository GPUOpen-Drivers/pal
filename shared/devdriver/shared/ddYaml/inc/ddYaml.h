/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include <yaml.h>
#include <stdint.h>

namespace DevDriver
{

/// Get the YAML node from a YAML document.
/// `pDoc` is the enclosing YAML document.
/// `pParent` is the parent YAML node. It must be a mapping node.
/// `pKey` is the mapping key to which the desired YAML node is associated with.
yaml_node_t* YamlDocumentFindNodeByKey(
    yaml_document_t* pDoc,
    yaml_node_t* pParent,
    const char* pKey);

/// Get the boolean value from a YAML node. Return true if the data contained
/// in the node can be converted to the desired value, false otherwise.
bool YamlNodeGetScalar(yaml_node_t* pValNode, bool* pOutValue);

/// Get the int8_t value from a YAML node. Return true if the data contained in
/// the node can be converted to the desired value, false otherwise.
bool YamlNodeGetScalar(yaml_node_t* pValNode, int8_t* pOutValue);

/// Get the uint8_t value from a YAML node. Return true the data contained in
/// the node can be converted to the desired value, false otherwise.
bool YamlNodeGetScalar(yaml_node_t* pValNode, uint8_t* pOutValue);

/// Get the int16_t value from a YAML node. Return true if the data contained in
/// the node can be converted to the desired value, false otherwise.
bool YamlNodeGetScalar(yaml_node_t* pValNode, int16_t* pOutValue);

/// Get the uint16_t value from a YAML node. Return true if the data contained
/// in the node can be converted to the desired value, false otherwise.
bool YamlNodeGetScalar(yaml_node_t* pValNode, uint16_t* pOutValue);

/// Get the int32_t value from a YAML node. Return true if the data contained in
/// the node can be converted to the desired value, false otherwise.
bool YamlNodeGetScalar(yaml_node_t* pValNode, int32_t* pOutValue);

/// Get the uint32_t value from a YAML node. Return true if the data contained
/// in the node can be converted to the desired value, false otherwise.
bool YamlNodeGetScalar(yaml_node_t* pValNode, uint32_t* pOutValue);

/// Get the int64_t value from a YAML node. Return true if the data contained in
/// the node can be converted to the desired value, false otherwise.
bool YamlNodeGetScalar(yaml_node_t* pValNode, int64_t* pOutValue);

/// Get the uint64_t value from a YAML node. Return true if the data contained
/// in the node can be converted to the desired value, false otherwise.
bool YamlNodeGetScalar(yaml_node_t* pValNode, uint64_t* pOutValue);

/// Get the float value from a YAML node. Return true if the data contained in
/// the node can be converted to the desired value, false otherwise.
bool YamlNodeGetScalar(yaml_node_t* pValNode, float* pOutValue);

} // namespace DevDriver
