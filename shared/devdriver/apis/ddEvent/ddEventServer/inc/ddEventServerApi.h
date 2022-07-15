/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef DD_EVENT_SERVER_API_HEADER
#define DD_EVENT_SERVER_API_HEADER

#include <ddApi.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Compile time version information
#define DD_EVENT_SERVER_API_MAJOR_VERSION 0
#define DD_EVENT_SERVER_API_MINOR_VERSION 3
#define DD_EVENT_SERVER_API_PATCH_VERSION 0

#define DD_EVENT_SERVER_API_VERSION_STRING DD_API_STRINGIFY_VERSION(DD_EVENT_SERVER_API_MAJOR_VERSION, \
                                                                    DD_EVENT_SERVER_API_MINOR_VERSION, \
                                                                    DD_EVENT_SERVER_API_PATCH_VERSION)

/// Opaque handle to an event server
typedef struct DDEventServer_t* DDEventServer;

/// Opaque handle to an event provider
typedef struct DDEventProvider_t* DDEventProvider;

/// Structure that contains the information required to create a server
typedef struct DDEventServerCreateInfo
{
    DDNetConnection  hConnection; /// A handle to an existing connection object
} DDEventServerCreateInfo;

/// Notifies the user that the associated event provider has been enabled
///
/// Note: This is called just after the actual state change occurs
typedef void (*PFN_ddEventProviderEnabled)(
    void* pUserdata); /// [in] Userdata pointer

/// Notifies the user that the associated event provider has been disabled
///
/// Note: This is called just after the actual state change occurs
typedef void (*PFN_ddEventProviderDisabled)(
    void* pUserdata); /// [in] Userdata pointer

/// A structure that contains all data required to notify an application when an event provider changes state
typedef struct DDEventProviderStateCb
{
    PFN_ddEventProviderEnabled pfnEnabled;
    PFN_ddEventProviderEnabled pfnDisabled;
    void*                      pUserdata;
} DDEventProviderStateCb;

/// Structure that contains the information required to create a provider
typedef struct DDEventProviderCreateInfo
{
    DDEventServer          hServer;                /// Server associated with the provider
    uint32_t               id;                     /// Unique identifier for the provider
    uint32_t               numEvents;              /// Number of valid events within the provider
    DDEventProviderStateCb stateChangeCb;          /// [Optional]
                                                   /// If valid functions are specified, they will be called when
                                                   /// the state of this provider changes as a result of a remote
                                                   /// client's request.
    char                   name[DD_API_PATH_SIZE]; /// Name of the provider
} DDEventProviderCreateInfo;

/// Get version of the loaded library to check interface compatibility
typedef DDApiVersion (*PFN_ddEventServerQueryVersion)(
    void);

/// Get human-readable representation of the loaded library version
typedef const char* (*PFN_ddEventServerQueryVersionString)(
    void);

/// Attempts to create a new server object with the provided creation information
typedef DD_RESULT (*PFN_ddEventServerCreate)(
    const DDEventServerCreateInfo* pInfo,     /// [in]  Create info
    DDEventServer*                 phServer); /// [out] Handle to the new server object

/// Destroys an existing server object
typedef void (*PFN_ddEventServerDestroy)(
    DDEventServer hServer); /// [in] Handle to the existing server object

/// Attempts to create a new server object with the provided creation information
typedef DD_RESULT (*PFN_ddEventServerCreateProvider)(
    const DDEventProviderCreateInfo* pInfo,       /// [in]  Create info
    DDEventProvider*                 phProvider); /// [out] Handle to the new provider object

/// Destroys an existing provider object
typedef void (*PFN_ddEventServerDestroyProvider)(
    DDEventProvider hProvider); /// [in] Handle to the existing provider object

/// Attempts to emit an event using the specified provider
///
/// This function allows the caller to specify an optional header blob to insert before the event payload data.
/// This can be useful in situations where you have a large binary blob payload and you need to insert a header
/// in front of it, but want to avoid duplicating it in memory just to make it a contiguous allocation.
typedef DD_RESULT (*PFN_ddEventServerEmitWithHeader)(
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
typedef DD_RESULT (*PFN_ddEventServerEmit)(
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
typedef DD_RESULT (*PFN_ddEventServerTestEmit)(
    DDEventProvider hProvider, /// [in] Handle to the existing provider object
    uint32_t        eventId);  /// Identifier of the event being emitted

/// API structure
typedef struct DDEventServerApi
{
    PFN_ddEventServerQueryVersion       pfnQueryVersion;
    PFN_ddEventServerQueryVersionString pfnQueryVersionString;
    PFN_ddEventServerCreate             pfnCreateServer;
    PFN_ddEventServerDestroy            pfnDestroyServer;
    PFN_ddEventServerCreateProvider     pfnCreateProvider;
    PFN_ddEventServerDestroyProvider    pfnDestroyProvider;
    PFN_ddEventServerEmitWithHeader     pfnEmitWithHeader;
    PFN_ddEventServerEmit               pfnEmit;
    PFN_ddEventServerTestEmit           pfnTestEmit;
} DDEventServerApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
