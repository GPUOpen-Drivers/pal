/* Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved. */

#ifndef DD_EVENT_STREAMER_HEADER
#define DD_EVENT_STREAMER_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#include <ddEventStreamerApi.h>

/// Attempts to create a new streamer object with the provided creation information
DD_RESULT ddEventStreamerCreate(
    const DDEventStreamerCreateInfo* pInfo,       /// [in]  Create info
    DDEventStreamer*                 phStreamer); /// [out] Handle to the new streamer object

/// Destroys an existing streamer object
/// An event streamer should only be destroyed after it has been signaled to stop streaming
void ddEventStreamerDestroy(
    DDEventStreamer hStreamer); /// [in] Handle to the existing streamer object

/// Updates the callback function trigged when an event is received.
/// To remove an active callback, this method should be called with the callback parameter
/// set to a null pointer. This will also allow events to be silently discarded.
DD_RESULT ddEventStreamerSetEventCallback(
    DDEventStreamer                hStreamer,  /// [in] Handle to the existing streamer object
    const DDEventStreamerCallback* pCallback); /// [in] Callback registration for event handling

/// Returns true if the streamer is actively listening for events
bool ddEventStreamerIsStreaming(
    DDEventStreamer hStreamer); /// [in] Handle to an existing streamer object

/// Signals the event streamer to cease from receiving events and begin
/// shutdown procedures. Must be called before the event streamer is destroyed.
DD_RESULT ddEventStreamerEndStreaming(
    DDEventStreamer hStreamer); /// [in] Handle to an existing streamer object

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ! DD_EVENT_STREAMER_HEADER