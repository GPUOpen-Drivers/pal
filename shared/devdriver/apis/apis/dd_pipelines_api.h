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

#ifndef DD_PIPELINES_API_H
#define DD_PIPELINES_API_H

#include "dd_common_api.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define DD_PIPELINES_API_NAME "DD_PIPELINES_API"

#define DD_PIPELINES_API_VERSION_MAJOR 1
#define DD_PIPELINES_API_VERSION_MINOR 0
#define DD_PIPELINES_API_VERSION_PATCH 0

typedef struct DDPipelinesInstance DDPipelinesInstance;

/// 128-bit Hash identifying a pipeline.  This hash is used by the driver to uniquely idenfity a pipeline at the API level.
typedef struct DDPipelinesApiHash
{
    uint64_t pipelineHashHi; /// Higher 64 bits (MSB) of the 128-bit hash
    uint64_t pipelineHashLo; /// Lower 64 bits (LSB) of the 128-bit hash
} DDPipelinesApiHash;

/// This structure tracks the binary contents of a code object
typedef struct DDPipelinesCodeObjectData
{
    const void*        pData; /// Pointer to the binary data of the code object
    uint64_t           size;  /// Size of the data pointed to by pData
    DDPipelinesApiHash hash;  /// API hash
} DDPipelinesCodeObjectData;

/// Callback for when a code object is received from the driver.
/// @param pData The code object data received from the driver.
/// @param pUserdata Userdata pointer.
typedef void (*DDPipelineRecordCallback)(DDPipelinesCodeObjectData* pData, void* pUserdata);

typedef struct DDPipelinesApi
{
    /// An opaque pointer to the internal implementation of the Pipelines API.
    DDPipelinesInstance* pInstance;

    /// Connects the pipeline client for the connection specified. This is idempotent, so calling it
    /// twice for the same connection will only connect once.
    /// This can be called any time after platform init.
    ///
    /// @param pInstance Must be \ref DDPipelinesApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to connect.
    /// @return DD_RESULT_SUCCESS The pipelines client was successfully connected.
    /// @return DD_RESULT_DD_GENERIC_NOT_READY If router connection isn't ready.
    /// @return Other errors if connecting failed.
    DD_RESULT (*Connect)(DDPipelinesInstance* pInstance, DDConnectionId umdConnectionId);

    /// Disconnects the pipeline client for the connection specified.
    ///
    /// @param pInstance Must be \ref DDPipelinesApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to disconnect.
    void (*Disconnect)(DDPipelinesInstance* pInstance, DDConnectionId umdConnectionId);

    /// Dumps all of the pipeline binaries for the client.
    ///
    /// @param pInstance Must be \ref DDPipelinesApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to dump pipelines for.
    /// @param pCallback The callback to call for each pipeline.
    /// @param pUserdata The userdata to call the callback with.
    DD_RESULT (*DumpDriverPipelines)(DDPipelinesInstance*    pInstance,
                                    DDConnectionId           umdConnectionId,
                                    DDPipelineRecordCallback pCallback,
                                    void*                    pUserdata);

    /// Injects a pipeline for the client.
    ///
    /// @param pInstance Must be \ref DDPipelinesApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to inject the pipeline to.
    /// @param pObjects The objects to inject.
    /// @param numObjects The number of objects in pObjects.
    DD_RESULT (*InjectPipelines)(DDPipelinesInstance*             pInstance,
                                 DDConnectionId                   umdConnectionId,
                                 const DDPipelinesCodeObjectData* pObjects,
                                 size_t                           numObjects);
} DDPipelinesApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
