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

#ifndef DD_MEMORY_TRACE_API_H
#define DD_MEMORY_TRACE_API_H

#include <stdint.h>

#include "dd_common_api.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define DD_MEMORY_TRACE_API_NAME "DD_MEMORY_TRACE_API"

#define DD_MEMORY_TRACE_API_VERSION_MAJOR 0
#define DD_MEMORY_TRACE_API_VERSION_MINOR 1
#define DD_MEMORY_TRACE_API_VERSION_PATCH 0

// Foward declaraction
struct DDRdfFileWriter;

typedef struct DDMemoryTraceInstance DDMemoryTraceInstance;

/// Enumeration of trace states
typedef enum
{
    DD_MEMORY_TRACE_STATE_UNKNOWN              = 0, /// The trace is in an unknown state
    DD_MEMORY_TRACE_STATE_NOT_STARTED          = 1, /// The trace has not been started
    DD_MEMORY_TRACE_STATE_RUNNING              = 2, /// The trace is currently running
    DD_MEMORY_TRACE_STATE_ENDED_UNKNOWN        = 3, /// The trace has ended for unknown reasons
    DD_MEMORY_TRACE_STATE_ENDED_USER_REQUESTED = 4, /// The trace has ended because it was explicitly requested
                                                    /// through the native api
    DD_MEMORY_TRACE_STATE_ENDED_APP_REQUESTED  = 5, /// The trace has ended because the application being traced
                                                    /// requested it
    DD_MEMORY_TRACE_STATE_ENDED_APP_EXITED     = 6, /// The trace has ended because the application being traced
                                                    /// exited or disconnected
} DD_MEMORY_TRACE_STATE;

/// Struct containing information about the current memory trace
typedef struct DDMemoryTraceStatus
{
    DD_MEMORY_TRACE_STATE state;  /// The current state of the trace
    uint64_t              size;   /// The current size of the trace data in bytes
    DD_RESULT             result; /// The result of the trace operation
} DDMemoryTraceStatus;

typedef struct DDMemoryTraceApi
{
    /// An opaque pointer to the internal implementation of the MemoryTrace API.
    DDMemoryTraceInstance* pInstance;

    /// Enables tracing for the connection specified. This is idempotent, so calling it
    /// twice for the same connection will only enable tracing once.
    /// This can be called any time after platform init.
    ///
    /// @param pInstance Must be \ref DDMemoryTraceApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to start tracing for.
    /// @param processId The process ID.
    /// @return DD_RESULT_SUCCESS Tracing was successfully started.
    /// @return DD_RESULT_DD_GENERIC_NOT_READY If router connection isn't ready.
    /// @return Other errors if starting tracing failed.
    DD_RESULT (*EnableTracing)(DDMemoryTraceInstance* pInstance, DDConnectionId umdConnectionId, DDProcessId processId);

    /// Disables tracing for the connection specified.
    ///
    /// @param pInstance Must be \ref DDMemoryTraceApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to stop tracing for.
    /// @return DD_RESULT_SUCCESS Tracing was successfully disabled.
    /// @return DD_RESULT_DD_GENERIC_NOT_READY Failed to delete internal resource associated with this tracing,
    ///         because it's still being used.
    DD_RESULT (*DisableTracing)(DDMemoryTraceInstance* pInstance, DDConnectionId umdConnectionId);

    /// Ends tracing for the connection specified.
    ///
    /// @param pInstance Must be \ref DDMemoryTraceApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to stop tracing for.
    DD_RESULT (*EndTracing)(DDMemoryTraceInstance* pInstance, DDConnectionId umdConnectionId, bool isClientInitialized);

    /// Dumps a memory trace on the provided client and continues collecting data
    ///
    /// @param pInstance Must be \ref DDMemoryTraceApi.pInstance.
    /// @param umdConnectionId The identifier of the connection.
    DD_RESULT (*DumpTrace)(DDMemoryTraceInstance* pInstance, DDConnectionId umdConnectionId, bool isClientInitialized);

    /// Aborts the current trace
    ///
    /// @param pInstance Must be \ref DDMemoryTraceApi.pInstance.
    /// @param umdConnectionId The identifier of the connection.
    DD_RESULT (*AbortTrace)(DDMemoryTraceInstance* pInstance, DDConnectionId umdConnectionId, bool isClientInitialized);

    /// Attempts to insert a snapshot
    ///
    /// @param pInstance Must be \ref DDMemoryTraceApi.pInstance.
    /// @param umdConnectionId The identifier of the connection.
    /// @param pSnapshotName The snapshot name
    DD_RESULT (*InsertSnapshot)(DDMemoryTraceInstance* pInstance,
                                DDConnectionId         umdConnectionId,
                                const char*            pSnapshotName);

    /// Attempts to clear the trace data
    ///
    /// @param pInstance Must be \ref DDMemoryTraceApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to clear a trace for.
    DD_RESULT (*ClearTrace)(DDMemoryTraceInstance* pInstance, DDConnectionId umdConnectionId);

    /// Queries the status of the trace
    ///
    /// @param pInstance Must be \ref DDMemoryTraceApi.pInstance.
    /// @param umdConnectionId The identifier of the connection.
    /// @param pStatus pointer to the current status
    DD_RESULT (*QueryStatus)(DDMemoryTraceInstance* pInstance,
                             DDConnectionId         umdConnectionId,
                             DDMemoryTraceStatus*   pStatus);

    /// Attempts to TransferTraceData the result a trace operation.
    ///
    /// @param pInstance Must be \ref DDMemoryTraceApi.pInstance.
    /// @param umdConnectionId The identifier of the connection.
    /// @param pFileWriter A pointer to an object of type \ref DDRdfFileWriter that will be used to return the trace data.
    /// @param pIoCb Callback to share status
    /// @param useCompression flag to indicate whether to use RDF compression
    DD_RESULT (*TransferTraceData) (DDMemoryTraceInstance* pInstance,
                                    DDConnectionId         umdConnectionId,
                                    const DDRdfFileWriter* pFileWriter,
                                    const DDIOHeartbeat*   pIoCb,
                                    bool                   useCompression);

} DDMemoryTraceApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
