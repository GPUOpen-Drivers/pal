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

#ifndef DD_GPU_DETECTIVE_API_H
#define DD_GPU_DETECTIVE_API_H

#include "dd_common_api.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define DD_GPU_DETECTIVE_API_NAME "DD_GPU_DETECTIVE_API"

#define DD_GPU_DETECTIVE_API_VERSION_MAJOR 0
#define DD_GPU_DETECTIVE_API_VERSION_MINOR 1
#define DD_GPU_DETECTIVE_API_VERSION_PATCH 0

typedef struct DDGpuDetectiveInstance DDGpuDetectiveInstance;

typedef struct DDGpuDetectiveApi
{
    /// An opaque pointer to the internal implementation of the UberTrace API.
    DDGpuDetectiveInstance* pInstance;

    /// Enables tracing for the connection specified. This is idempotent, so calling it
    /// twice for the same connection will only enable tracing once.
    /// This can be called any time after platform init.
    ///
    /// @param pInstance Must be \ref DDGpuDetectiveInstance.pInstance.
    /// @param umdConnectionId The identifier of the connection to start tracing for.
    /// @param processId The process ID.
    /// @return DD_RESULT_SUCCESS Tracing was successfully started.
    /// @return DD_RESULT_DD_GENERIC_NOT_READY If router connection isn't ready.
    /// @return Other errors if starting tracing failed.
    DD_RESULT (*EnableTracing)(DDGpuDetectiveInstance* pInstance, DDConnectionId umdConnectionId, DDProcessId processId);

    /// Disables tracing for the connection specified.
    ///
    /// @param pInstance Must be \ref DDGpuDetectiveApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to stop tracing for.
    void (*DisableTracing)(DDGpuDetectiveInstance* pInstance, DDConnectionId umdConnectionId);

    /// Ends tracing for the connection specified and asynchronously writes out the dump for any crash
    /// that was detected.
    ///
    /// @param pInstance Must be \ref DDGpuDetectiveInstance.pInstance.
    /// @param umdConnectionId The identifier of the connection to stop tracing for.
    /// @param isClientInitialized Should be true if the connection reached the post device init state.
    /// @param[out] pCrashDetected Will be set to true if a crash was detected.
    /// @return DD_RESULT_SUCCESS Tracing was successfully ended.
    /// @return Other errors if ending tracing failed.
    DD_RESULT (*EndTracing)(DDGpuDetectiveInstance* pInstance,
                            DDConnectionId          umdConnectionId,
                            bool                    isClientInitialized,
                            bool*                   didDetectCrash);

    /// Synchronously dumps a crash dump for the connection if one occurred.
    ///
    /// @param pInstance Must be \ref DDMemoryTraceApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to dump the crash for.
    /// @param pRdfFileWriter The file writer to use to write a crash dump if one was detected.
    /// @param pHeartbeat The heartbeat to notify about trace writing.
    /// @return DD_RESULT_SUCCESS Tracing was successfully ended.
    /// @return DD_RESULT_DD_GENERIC_NOT_READY Tracing for the connection was not ended.
    /// @return DD_RESULT_DD_GENERIC_INVALID_PARAMETER The file writer or heartbeat were not valid.
    DD_RESULT (*TransferTraceData)(DDGpuDetectiveInstance*       pInstance,
                                   DDConnectionId                umdConnectionId,
                                   const struct DDRdfFileWriter* pRdfFileWriter,
                                   const struct DDIOHeartbeat*   pHeartbeat);

} DDGpuDetectiveApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
