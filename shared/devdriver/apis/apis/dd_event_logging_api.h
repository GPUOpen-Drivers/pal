/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef DD_EVENT_LOGGING_API_H
#define DD_EVENT_LOGGING_API_H

#include <stdint.h>

#include "dd_common_api.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define DD_EVENT_LOGGING_API_NAME "DD_EVENT_LOGGING_API"

#define DD_EVENT_LOGGING_API_VERSION_MAJOR 0
#define DD_EVENT_LOGGING_API_VERSION_MINOR 1
#define DD_EVENT_LOGGING_API_VERSION_PATCH 0

// Foward declaraction
struct DDRdfFileWriter;

typedef struct DDEventLoggingInstance DDEventLoggingInstance;

/// Enumeration of trace states
typedef enum
{
    DD_EVENT_LOGGING_STATE_UNKNOWN              = 0, /// The trace is in an unknown state
    DD_EVENT_LOGGING_STATE_RUNNING              = 1, /// The trace is currently running
    DD_EVENT_LOGGING_STATE_ENDED_UNKNOWN        = 2, /// The trace has ended for unknown reasons
    DD_EVENT_LOGGING_STATE_ENDED_USER_REQUESTED = 3, /// The trace has ended because it was explicitly requested
                                                     /// through the native api
    DD_EVENT_LOGGING_STATE_ENDED_APP_REQUESTED  = 4, /// The trace has ended because the application being traced
                                                     /// requested it
    DD_EVENT_LOGGING_STATE_ENDED_APP_EXITED     = 5, /// The trace has ended because the application being traced
                                                     /// exited or disconnected
} DD_EVENT_LOGGING_STATE;

/// Struct containing information about the current memory trace
typedef struct DDEventLoggingStatus
{
    DD_EVENT_LOGGING_STATE  state;  /// The current state of the trace
    uint64_t                size;   /// The current size of the trace data in bytes
    DD_RESULT               result; /// The result of the trace operation
} DDEventLoggingStatus;

/// Struct containing information about an incoming event
typedef struct DDEventLoggingEventInfo
{
    uint64_t timestampFrequency; /// Frequency of the timestamp associated with this event (ticks per second)
    uint64_t timestamp;          /// Timestamp recorded when this event was emitted by the provider
    uint32_t providerId;         /// Id of the event provider that emitted this event
    uint32_t eventId;            /// Id of the event within the provider
    uint32_t eventIndex;         /// Index of the event within the provider's event stream
                                 /// This can be used to verify that all events were correctly
                                 /// captured in the data stream.
    uint64_t totalPayloadSize;   /// The total size of the data payload belonging to this event.
} DDEventLoggingEventInfo;

typedef struct DDEventReceiveEventCallbackImpl DDEventReceiveEventCallbackImpl;
typedef struct DDEventReceiveEventCallback
{
    DDEventReceiveEventCallbackImpl* pImpl;

    void (*ReceiveEvent)(
        DDEventReceiveEventCallbackImpl* pImpl,
        DDEventLoggingEventInfo eventInfo,          /// [in] Received event info
        const void* pEventDataPayload);

} DDEventReceiveEventCallback;

/// Notifies the caller of a complete incoming event from the enabled provider
/// All sizes are measured in bytes
typedef void (*PFN_ddReceiveEvent)(
    void*                   pUserdata,          /// [in] Userdata pointer
    DDEventLoggingEventInfo eventInfo,          /// [in] Received event info
    const void*             pEventDataPayload); /// [in] Pointer to event data

/// An interface for receiving a callback for incoming events
typedef struct DDEventLoggingReceiveCb
{
    void*              pUserdata;
    PFN_ddReceiveEvent pfnReceiveEvent;
} DDEventLoggingReceiveCb;

typedef struct DDEventLoggingApi
{
    /// An opaque pointer to the internal implementation of the EventLogging API.
    DDEventLoggingInstance* pInstance;

    /// Enables tracing for the connection specified. This is idempotent, so calling it
    /// twice for the same connection will only enable tracing once.
    /// This can be called any time after platform init.
    ///
    /// @param pInstance Must be \ref DDEventLoggingApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to start tracing for.
    /// @param processId The process ID.
    /// @param providerId The ID of the event provider to enable
    /// @return DD_RESULT_SUCCESS Tracing was successfully started.
    /// @return DD_RESULT_DD_GENERIC_NOT_READY If router connection isn't ready.
    /// @return Other errors if starting tracing failed.
    DD_RESULT (*EnableTracing)(DDEventLoggingInstance* pInstance,
                               DDConnectionId          umdConnectionId,
                               DDProcessId             processId,
                               uint32_t                providerId);

    /// Registers a callback for receiving incoming events. Subsequent calls to this
    /// function will replace the existing callback. Calling with a null structure will
    /// disable incoming event callbacks.
    ///
    /// @param pInstance Must be \ref DDEventLoggingApi.pInstance.
    /// @param pReceiveCallback Event receive callback structure.
    /// @return DD_RESULT_SUCCESS Callback was successfully registered.
    /// @return DD_RESULT_DD_GENERIC_INVALID_PARAMETER The receive callback was not valid.
    DD_RESULT (*RegisterEventReceiveCb)(DDEventLoggingInstance*               pInstance,
                                        const DDEventReceiveEventCallback*    pReceiveCallback);

    /// Disables tracing for the connection specified.
    ///
    /// @param pInstance Must be \ref DDEventLoggingApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to stop tracing for.
    void (*DisableTracing)(DDEventLoggingInstance* pInstance, DDConnectionId umdConnectionId);

    /// Ends tracing for the connection specified and asynchronously writes out the event data.
    ///
    /// @param pInstance Must be \ref DDEventLoggingApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to stop tracing for.
    /// @param isClientInitialized Should be true if the connection reached the post device init state.
    /// @return DD_RESULT_SUCCESS Tracing was successfully ended.
    /// @return Other errors if ending tracing failed.
    DD_RESULT (*EndTracing)(DDEventLoggingInstance* pInstance,
                            DDConnectionId          umdConnectionId,
                            bool                    isClientInitialized);

    /// Synchronously dumps event data for the connection.
    ///
    /// @param pInstance Must be \ref DDEventLoggingApi.pInstance.
    /// @param umdConnectionId The identifier of the connection to dump the crash for.
    /// @param pRdfFileWriter A pointer to an object of type \ref DDRdfFileWriter, used to write a crash
    /// @param pFileWriter A pointer to an object of type \ref DDRdfFileWriter that will be used to dump
    /// the trace data if one was detected.
    /// @param pHeartbeat The heartbeat to notify about trace writing.
    /// @return DD_RESULT_SUCCESS Tracing was successfully ended.
    /// @return DD_RESULT_DD_GENERIC_NOT_READY Tracing for the connection was not ended.
    /// @return DD_RESULT_DD_GENERIC_INVALID_PARAMETER The file writer or heartbeat were not valid.
    DD_RESULT (*TransferTraceData)(DDEventLoggingInstance* pInstance,
                                   DDConnectionId          umdConnectionId,
                                   const DDRdfFileWriter* pFileWriter,
                                   const DDIOHeartbeat*    pHeartbeat);

} DDEventLoggingApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
