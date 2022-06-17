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

#endif
