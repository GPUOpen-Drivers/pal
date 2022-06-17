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

#ifndef DD_EVENT_STREAMER_API_HEADER
#define DD_EVENT_STREAMER_API_HEADER

#include <ddEventParserApi.h>
#include <ddApi.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Compile time version information
#define DD_EVENT_STREAMER_API_MAJOR_VERSION 0
#define DD_EVENT_STREAMER_API_MINOR_VERSION 1
#define DD_EVENT_STREAMER_API_PATCH_VERSION 0

#define DD_EVENT_STREAMER_API_VERSION_STRING DD_API_STRINGIFY_VERSION(DD_EVENT_STREAMER_API_MAJOR_VERSION, \
                                                                      DD_EVENT_STREAMER_API_MINOR_VERSION, \
                                                                      DD_EVENT_STREAMER_API_PATCH_VERSION)

/// Opaque handle to an event streamer
typedef struct DDEventStreamer_t* DDEventStreamer;

/// Callback signature for receiving event data
/// This callback will be called once for each event triggered by
/// a provider. Event data given to this callback is fully-formed
/// and is not streamed in chunks.
typedef void (*PFN_ddEventStreamerCallback)(
    void*                         pUserdata,     /// [in] Userdata pointer
    const DDEventParserEventInfo& eventInfo,     /// Structure containing event metadata
    const void*                   eventData,     /// [in] Event data buffer
    const size_t                  eventDataSize, /// Size of event data buffer
    const DD_RESULT               eventResult);  /// Received event status

/// Structure required to register a callback for event handling
/// The event data handed to the callback will be fully-formed events,
/// including the event header and payload. The userdata pointer is an
/// arbitrary pointer and may safely be nullptr if not required by the
/// callback.
typedef struct DDEventStreamerCallback
{
    void*                         pUserdata;   /// [in] Userdata pointer
    PFN_ddEventStreamerCallback   pfnCallback; /// [in] Pointer to an on-event callback function
} DDEventStreamCallback;

/// Construction parameters required for creating an event streamer object
typedef struct DDEventStreamerCreateInfo
{
    DDNetConnection               hConnection; /// A handle to an existing connection object
    DDClientId                    clientId;    /// The client ID on the network to connect to
    uint32_t                      providerId;  /// Provider ID value
    DDEventStreamerCallback       onEventCb;   /// Callback registration for event handling
} DDEventStreamerCreateInfo;

/// Get version of the loaded library to check interface compatibility
typedef DDApiVersion (*PFN_ddEventStreamerQueryVersion)(void);

/// Get human-readable representation of the loaded library version
typedef const char* (*PFN_ddEventStreamerQueryVersionString)(void);

/// Attempts to create a new client object with the provided creation information
typedef DD_RESULT(*PFN_ddEventStreamerCreate)(
    const DDEventStreamerCreateInfo* pInfo,       /// [in]  Create info
    DDEventStreamer*                 phStreamer); /// [out] Handle to the new streamer object

/// Destroys an existing streamer object
typedef void (*PFN_ddEventStreamerDestroy)(
    DDEventStreamer hStreamer); /// [in] Handle to the existing streamer object

/// Updated the callback function triggered when a new event is received
typedef DD_RESULT (*PFN_ddEventStreamerSetEventCallback)(
    DDEventStreamer                hStreamer,  /// [in] Handle to the existing streamer object
    const DDEventStreamerCallback* pCallback); /// [in] Callback registration for event handling

/// Returns true if the streamer is actively listening for events
typedef bool (*PFN_ddEventStreamerIsStreaming)(
    DDEventStreamer hStreamer); /// [in] Handle to an existing streamer object

/// Signals the event streamer to safely shutdown streaming
typedef DD_RESULT (*PFN_ddEventStreamerEndStreaming)(
    DDEventStreamer hStreamer); /// [in] Handle to an existing streamer object

typedef struct DDEventStreamerApi
{
    PFN_ddEventStreamerQueryVersion       pfnQueryVersion;
    PFN_ddEventStreamerQueryVersionString pfnQueryVersionString;
    PFN_ddEventStreamerCreate             pfnCreateStreamer;
    PFN_ddEventStreamerDestroy            pfnDestroyStreamer;
    PFN_ddEventStreamerSetEventCallback   pfnSetEventCallback;
    PFN_ddEventStreamerIsStreaming        pfnIsStreaming;
    PFN_ddEventStreamerEndStreaming       pfnEndStreaming;
} DDEventStreamerApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
