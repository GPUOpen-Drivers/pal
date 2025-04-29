/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef DD_UBER_TRACE_API_H
#define DD_UBER_TRACE_API_H

#include <stdint.h>

#include "dd_common_api.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define DD_UBER_TRACE_API_NAME "DD_UBER_TRACE_API"

#define DD_UBER_TRACE_API_VERSION_MAJOR 0
#define DD_UBER_TRACE_API_VERSION_MINOR 2
#define DD_UBER_TRACE_API_VERSION_PATCH 0

typedef struct DDUberTraceInstance DDUberTraceInstance;

typedef struct DDUberTraceApi
{
    /// An opaque pointer to the internal implementation of the UberTrace API.
    DDUberTraceInstance* pInstance;

    /// Connects the UberTrace RPC service to the connection specified.
    ///
    /// This is idempotent, so calling it twice for the same connection will only connect once.
    /// This can be called any time after the driver is connected, so long as the driver
    /// is still connected.
    ///
    /// @param pInstance Must be \ref DDUberTraceApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to start tracing for.
    /// @return DD_RESULT_SUCCESS Tracing was successfully started.
    /// @return DD_RESULT_DD_GENERIC_NOT_READY If router connection isn't ready.
    /// @return Other errors if connecting failed.
    DD_RESULT (*Connect)(DDUberTraceInstance* pInstance, DDConnectionId umdConnectionId);

    /// Disconnects the UberTrace RPC service from the connection specified.
    ///
    /// @param pInstance Must be \ref DDUberTraceApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to disconnect from.
    void (*Disconnect)(DDUberTraceInstance* pInstance, DDConnectionId umdConnectionId);

    /// Activates UberTrace-based tracing within client drivers for the connection specified.
    ///
    /// This has a specific meaning in the client driver and is not required to use UberTrace,
    /// only to use certain trace sources that live within the driver.
    ///
    /// This call signals to the client driver's "DevDriverMgr" layer to disable legacy RGP
    /// tracing and use UberTrace instead.
    ///
    /// @note This *must* be called during the Platform Init driver state.
    ///
    /// @param pInstance       Must be \ref DDUberTraceApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to start tracing for.
    /// @return DD_RESULT_SUCCESS Tracing was successfully started.
    /// @return DD_RESULT_DD_GENERIC_NOT_READY If the router connection isn't ready.
    /// @return Other errors if starting tracing failed.
    DD_RESULT (*EnableTracing)(DDUberTraceInstance* pInstance, DDConnectionId umdConnectionId);

    /// Updates the run-time trace parameters associated with the provided client.
    ///
    /// @param pInstance Must be \ref DDUberTraceApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to update the trace params for.
    /// @param pData Pointer to parameter buffer.
    /// @param dataSize Size of the parameter buffer.
    /// @return DD_RESULT_SUCCESS Trace params were updated.
    /// @return DD_RESULT_DD_GENERIC_NOT_READY If tracing was not enabled for the client.
    /// @return DD_RESULT_COMMON_INVALID_PARAMETER If the buffer isn't valid.
    /// @return Other errors if updating params failed.
    DD_RESULT(*ConfigureTraceParams)(DDUberTraceInstance* pInstance,
                                     DDConnectionId       umdConnectionId,
                                     const char*          pData,
                                     size_t               dataSize);

    /// Attempts to request a trace operation using the current configured parameters.
    ///
    /// This cannot be called until driver initialization is complete (Running state or after PostDeviceInit)
    /// @param pInstance Must be \ref DDUberTraceApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to request a trace for.
    /// @return DD_RESULT_SUCCESS Capture was successfully requested.
    /// @return DD_RESULT_DD_GENERIC_NOT_READY If tracing was not enabled for the client.
    /// @return Other errors if requesting capture failed.
    DD_RESULT (*RequestTrace)(DDUberTraceInstance* pInstance, DDConnectionId umdConnectionId);

    /// Attempts to cancel a trace operation.
    ///
    /// This cannot be called until driver initialization is complete (Running state or after PostDeviceInit)
    /// @param pInstance Must be \ref DDUberTraceApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to cancel a trace for.
    /// @return DD_RESULT_SUCCESS Capture was successfully cancelled.
    /// @return DD_RESULT_DD_GENERIC_NOT_READY If tracing was not enabled for the client.
    /// @return Other errors if cancelling capture failed.
    DD_RESULT (*CancelTrace)(DDUberTraceInstance* pInstance, DDConnectionId umdConnectionId);

    /// Attempts to collect the result a trace operation.
    ///
    /// This cannot be called until driver initialization is complete (Running state or after PostDeviceInit)
    /// @param pInstance Must be \ref DDUberTraceApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to request a trace for.
    /// @param timeoutInMs Number of milliseconds to wait for the trace data to become available.
    /// @param pWriter A writer that will be used to return the trace data.
    /// @return DD_RESULT_SUCCESS Capture was successfully collected.
    /// @return DD_RESULT_DD_GENERIC_NOT_READY If tracing was not enabled for the client.
    /// @return Other errors if collecting capture failed.
    DD_RESULT (*CollectTrace)(DDUberTraceInstance*       pInstance,
                              DDConnectionId             umdConnectionId,
                              uint32_t                   timeoutInMs,
                              const struct DDByteWriter* pWriter);

} DDUberTraceApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
