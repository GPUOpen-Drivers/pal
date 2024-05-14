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

#ifndef DD_API_REGISTRY_API_H
#define DD_API_REGISTRY_API_H

#include "dd_common_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DD_API_REGISTRY_API_VERSION_MAJOR 0
#define DD_API_REGISTRY_API_VERSION_MINOR 1
#define DD_API_REGISTRY_API_VERSION_PATCH 0

typedef struct DDApiRegistryInstance DDApiRegistryInstance;

/// A struct containing functions and data members for ApiRegistry.
typedef struct DDApiRegistry
{
    /// The current version of this API.
    DDVersion version;

    /// A opaque pointer to an internal API registry instance.
    DDApiRegistryInstance* pInstance;

    /// Add an API struct to the registry. This function stores a copy of the API struct in the registry.
    ///
    /// @param pRegistry Must be \ref DDApiRegistry.pInstance.
    /// @param pApiName A pointer to the API name. The registry only stores the pointer, so the caller of this
    /// function needs to make sure the name string data exists throughout the whole time the API is registered.
    /// @param version The version of the API.
    /// @param pApiStruct A pointer to an instantiation of the API struct.
    /// @param apiStructSize The size of the API struct.
    ///
    /// @return DD_RESULT_SUCCESS The API has been registered successfully.
    /// @return DD_RESULT_COMMON_ALREADY_EXISTS Registration failed because the API with the same name already
    /// exists in the registry.
    /// @return DD_RESULT_COMMON_BUFFER_TOO_SMALL Registration failed because the internal API pool is too small to
    /// accept more data.
    DD_RESULT (*Add)(DDApiRegistryInstance* pInstance, const char* pApiName, DDVersion version, void* pApiStruct, size_t apiStructSize);

    /// Get the API by its name.
    ///
    /// @param pRegistry Must be \ref DDApiRegistry.pInstance.
    /// @param pApiName A pointer to the API name string data.
    /// @param version The version of the API to query.
    /// @param[out] ppOutApiStruct On success, it's set to a pointer to the copy of the API struct stored in the
    /// registry. On failure, it's set to NULL.
    ///
    /// @return DD_RESULT_SUCCESS The API with the correct version is returned.
    /// @return DD_RESULT_COMMON_DOES_NOT_EXIST The queried API doesn't exist in the registry.
    /// @return DD_RESULT_COMMON_VERSION_MISMATCH The version of the existing API doesn't satisfy the queried version.
    /// @return DD_RESULT_COMMON_INVALID_PARAMETER If ppOutApiStruct is a null pointer.
    DD_RESULT (*Get)(DDApiRegistryInstance* pInstance, const char* pApiName, DDVersion version, void** ppOutApiStruct);
} DDApiRegistry;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
