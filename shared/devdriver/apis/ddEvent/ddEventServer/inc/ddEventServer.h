/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef DD_EVENT_SERVER_HEADER
#define DD_EVENT_SERVER_HEADER

#include <ddEventServerApi.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Attempts to create a new server object with the provided creation information
DD_RESULT ddEventServerCreate(
    const DDEventServerCreateInfo* pInfo,     /// [in]  Create info
    DDEventServer*                 phServer); /// [out] Handle to the new server object

/// Destroys an existing server object
void ddEventServerDestroy(
    DDEventServer hServer); /// [in] Handle to the existing server object

/// Attempts to create a new provider object with the provided creation information
DD_RESULT ddEventServerCreateProvider(
    const DDEventProviderCreateInfo* pInfo,       /// [in]  Create info
    DDEventProvider*                 phProvider); /// [out] Handle to the new provider object

/// Destroys an existing provider object
void ddEventServerDestroyProvider(
    DDEventProvider hProvider); /// [in] Handle to the existing provider object

/// Attempts to emit an event using the specified provider
///
/// This function allows the caller to specify an optional header blob to insert before the event payload data.
/// This can be useful in situations where you have a large binary blob payload and you need to insert a header
/// in front of it, but want to avoid duplicating it in memory just to make it a contiguous allocation.
DD_RESULT ddEventServerEmitWithHeader(
    DDEventProvider hProvider,   /// [in] Handle to the existing provider object
    uint32_t        eventId,     /// Identifier of the event being emitted
    size_t          headerSize,  /// [Optional]
                                 //< Size of the optional event header if one is present
    const void*     pHeader,     /// [Optional]
                                 //< Pointer to the optional event header data if one is present
    size_t          payloadSize, /// [Optional]
                                 //< Size of the optional event payload if one is present
    const void*     pPayload);   /// [Optional]
                                 //< Pointer to the optional event payload data if one is present

/// Attempts to emit an event using the specified provider
DD_RESULT ddEventServerEmit(
    DDEventProvider hProvider,   /// [in] Handle to the existing provider object
    uint32_t        eventId,     /// Identifier of the event being emitted
    size_t          payloadSize, /// [Optional]
                                 //< Size of the optional event payload if one is present
    const void*     pPayload);   /// [Optional]
                                 //< Pointer to the optional event payload data if one is present

/// Tests the result of emitting the provided event on the associated provider
///
/// This will return DD_RESULT_SUCCESS if the call to emit would succeed and a relevant error code otherwise
///
/// Note: The status of providers and events may change at ANY time! This should not be used as a guarantee
/// that a future attempt to emit an event will succeed. This functionality is available for cases where the
/// code to prepare a specific event before calling emit is expensive. When the application knows the call
/// to emit will likely fail anyways, they can avoid the unnecessary preparation work and improve performance.
DD_RESULT ddEventServerTestEmit(
    DDEventProvider hProvider, /// [in] Handle to the existing provider object
    uint32_t        eventId);  /// Identifier of the event being emitted

#ifdef __cplusplus
} // extern "C"
#endif

#endif
