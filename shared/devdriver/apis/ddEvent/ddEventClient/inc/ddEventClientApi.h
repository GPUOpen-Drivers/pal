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

#ifndef DD_EVENT_CLIENT_API_HEADER
#define DD_EVENT_CLIENT_API_HEADER

#include <ddApi.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Compile time version information
#define DD_EVENT_CLIENT_API_MAJOR_VERSION 0
#define DD_EVENT_CLIENT_API_MINOR_VERSION 2
#define DD_EVENT_CLIENT_API_PATCH_VERSION 0

#define DD_EVENT_CLIENT_API_VERSION_STRING DD_API_STRINGIFY_VERSION(DD_EVENT_CLIENT_API_MAJOR_VERSION, \
                                                                    DD_EVENT_CLIENT_API_MINOR_VERSION, \
                                                                    DD_EVENT_CLIENT_API_PATCH_VERSION)

/// Opaque handle to an event client
typedef struct DDEventClient_t* DDEventClient;

/// Used by "ReadEventData()" to return event data from the network to the application
///
/// NOTE: This callback may also be triggered indirectly during the execution of functions that interact with providers.
///       This is a consequence of the asynchronous nature of the event protocol. It's entirely possible that the server
///       could emit events while a remote client is attempting to interact with providers. In this case, the client may
///       end up receiving event data interleaved with its provider configuration messages. When this occurs, the
///       implementation is forced to immediately return the data to the application since there is no way to robustly
///       buffer the data until the next time the read function is called.
typedef void (*PFN_ddEventDataCallback)(
    void*         pUserdata, /// [in] Userdata pointer
    const void*   pData,     /// [in] Pointer to a buffer that contains event data
    size_t        dataSize); /// Size of the buffer pointed to by pData

/// Helper structure for PFN_ddEventDataCallback
typedef struct DDEventDataCallback
{
    void*                   pUserdata;   /// [in] Userdata pointer
    PFN_ddEventDataCallback pfnCallback; /// [in] Pointer to a data callback function
} DDEventDataCallback;

/// Structure that contains the information required to create a client
typedef struct DDEventClientCreateInfo
{
    DDNetConnection     hConnection; /// A handle to an existing connection object
    DDClientId          clientId;    /// The ClientId on the network to connect to
    DDEventDataCallback dataCb;      /// Callback used to return event data to the application
    uint32_t            timeoutInMs; /// The maximum time that will be spent attempting to connect to the remote server
                                     //< Connection occurs at creation time and creation will fail if a timeout is
                                     //< encountered.
                                     //< [Optional] Specify 0 to use a reasonable but implementation defined default.
} DDEventClientCreateInfo;

/// Get version of the loaded library to check interface compatibility
typedef DDApiVersion (*PFN_ddEventClientQueryVersion)(
    void);

/// Get human-readable representation of the loaded library version
typedef const char* (*PFN_ddEventClientQueryVersionString)(
    void);

/// Attempts to create a new client object with the provided creation information
typedef DD_RESULT(*PFN_ddEventClientCreate)(
    const DDEventClientCreateInfo* pInfo,     /// [in]  Create info
    DDEventClient*                 phClient); /// [out] Handle to the new client object

/// Destroys an existing client object
typedef void (*PFN_ddEventClientDestroy)(
    DDEventClient hClient); /// [in] Handle to the existing client object

/// Attempts to read incoming event from the server
///
/// Any data received through the read operation will be returned via the event data callback that was provided during
/// client creation.
typedef DD_RESULT (*PFN_ddEventClientReadEventData)(
    DDEventClient hClient,      /// [in] Handle to an existing client object
    uint32_t      timeoutInMs); /// Timeout in milliseconds

/// Structure that generically describes enablement status
typedef struct DDEventEnabledStatus
{
    uint8_t isEnabled : 1; /// Non-zero if enabled

    uint8_t reserved  : 7; /// Reserved for future use
} DDEventEnabledStatus;

/// Structure that describes a remote event provider
typedef struct DDEventProviderDesc
{
    uint32_t                    providerId;     /// Unique identifier
    DDEventEnabledStatus        providerStatus; /// Overall enablement status
    uint32_t                    numEvents;      /// Number of items in pEventStatus array
    const DDEventEnabledStatus* pEventStatus;   /// Enablement status for each event in the provider
} DDEventProviderDesc;

/// Used by "QueryProviders()" to return data about an individual provider to the caller
///
/// This callback will be called once per provider returned by the server
/// If this function returns non-success, iteration will be aborted
typedef DD_RESULT(*PFN_ddEventVisitProvider)(
    void*                      pUserdata,  /// [in] Userdata pointer
    const DDEventProviderDesc* pProvider); /// [in] Pointer to a provider description

/// Helper structure for PFN_ddEventVisitProvider
typedef struct DDEventProviderVisitor
{
    void*                    pUserdata; /// [in] Userdata pointer
    PFN_ddEventVisitProvider pfnVisit;  /// [in] Pointer to a visitor function that will be called once per provider
} DDEventProviderVisitor;

/// Attempts to query and return all known providers from the remote server
typedef DD_RESULT (*PFN_ddEventClientQueryProviders)(
    DDEventClient                 hClient,   /// [in] Handle to an existing client object
    const DDEventProviderVisitor* pVisitor); /// [in] Provider visitor to return provider data through

/// Attempts to configure the state of the providers on the remote server
///
/// Providers on the remote server will be updated to reflect the new configuration
typedef DD_RESULT (*PFN_ddEventClientConfigureProviders)(
    DDEventClient                 hClient,      /// [in] Handle to an existing client object
    size_t                        numProviders, /// Number of items in the pProviders array
    const DDEventProviderDesc*    pProviders);  /// [in] Array of provider descriptions to send to the server

/// Attempts to fully enable all specified providers on the remote server
///
/// This will enable the providers themselves and all individual events supported by them
typedef DD_RESULT (*PFN_ddEventClientEnableProviders)(
    DDEventClient   hClient,        /// [in] Handle to an existing client object
    size_t          numProviderIds, /// Number of items in the pProviderIds array
    const uint32_t* pProviderIds);  /// [in] Array of provider ids to send to the server

/// Attempts to fully disable all specified providers on the remote server
///
/// This will disable the providers themselves and all individual events supported by them
typedef DD_RESULT (*PFN_ddEventClientDisableProviders)(
    DDEventClient   hClient,        /// [in] Handle to an existing client object
    size_t          numProviderIds, /// Number of items in the pProviderIds array
    const uint32_t* pProviderIds);  /// [in] Array of provider ids to send to the server

/// API structure
typedef struct DDEventClientApi
{
    PFN_ddEventClientQueryVersion       pfnQueryVersion;
    PFN_ddEventClientQueryVersionString pfnQueryVersionString;
    PFN_ddEventClientCreate             pfnCreateClient;
    PFN_ddEventClientDestroy            pfnDestroyClient;
    PFN_ddEventClientReadEventData      pfnReadEventData;
    PFN_ddEventClientQueryProviders     pfnQueryProviders;
    PFN_ddEventClientConfigureProviders pfnConfigureProviders;
    PFN_ddEventClientEnableProviders    pfnEnableProviders;
    PFN_ddEventClientDisableProviders   pfnDisableProviders;
} DDEventClientApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
