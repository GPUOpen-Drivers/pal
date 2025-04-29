/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef DD_RPC_CLIENT_API_HEADER
#define DD_RPC_CLIENT_API_HEADER

#include <ddApi.h>

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////// Data Definitions (enums, structs, defines) ////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Compile time version information
#define DD_RPC_CLIENT_API_MAJOR_VERSION 1
#define DD_RPC_CLIENT_API_MINOR_VERSION 1
#define DD_RPC_CLIENT_API_PATCH_VERSION 0

#define DD_RPC_CLIENT_API_VERSION_STRING DD_API_STRINGIFY_VERSION(DD_RPC_CLIENT_API_MAJOR_VERSION, \
                                                                  DD_RPC_CLIENT_API_MINOR_VERSION, \
                                                                  DD_RPC_CLIENT_API_PATCH_VERSION)

/// Name of the API
#define DD_RPC_CLIENT_API_NAME        "ddRpcClient"

/// Description of the API
#define DD_RPC_CLIENT_API_DESCRIPTION "Client-side API for remote procedure calls over DevDriver"

/// Identifier for the API
/// This identifier is used to acquire access to the API's interface
// Note: This is "drvrpc_c" in big endian ASCII
#define DD_RPC_CLIENT_API_ID 0x6472767270635f63

/// Opaque handle to a developer driver remote procedure call client
typedef struct DDRpcClient_t* DDRpcClient;

/// Structure that contains the information required to create a client
typedef struct DDRpcClientCreateInfo
{
    DDNetConnection  hConnection; /// A handle to an existing connection object
    DDProtocolId     protocolId;  /// The identifier of the protocol to use for connections
                                  //< Specify 0 for the standard default id.
                                  //< This is effectively the "port".
    DDClientId       clientId;    /// The ClientId on the network to connect to
    uint32_t         timeoutInMs; /// The maximum time that the connection will wait until timing out
                                  //< [Optional] Specify 0 to use a reasonable but implementation defined default.
} DDRpcClientCreateInfo;

/// Structure that contains all required information for a function call operation
typedef struct DDRpcClientCallInfo
{
    /// Remote service to execute on
    DDRpcServiceId      service;

    /// Desired version of the remote service
    DDApiVersion        serviceVersion;

    /// Remote service function to execute
    DDRpcFunctionId     function;

    /// Request data that is sent to the function
    size_t              paramBufferSize;
    const void*         pParamBuffer;

    /// ByteWrier that will receive response data if the call is successful.
    const DDByteWriter* pResponseWriter;

    /// Time (in milliseconds) to wait before data-sending operation times out in one try.
    uint32_t            sendTimeoutMillis;
} DDRpcClientCallInfo;

/// Get version of the loaded library to check interface compatibility
typedef DDApiVersion (*PFN_ddRpcClientQueryVersion)(
    void);

/// Get human-readable representation of the loaded library version
typedef const char* (*PFN_ddRpcClientQueryVersionString)(
    void);

/// Attempts to create a new client object with the provided creation information
typedef DD_RESULT (*PFN_ddRpcClientCreate)(
    const DDRpcClientCreateInfo* pInfo,     /// [in]  Create info
    DDRpcClient*                 phClient); /// [out] Handle to the new client object

/// Destroys an existing client object
typedef void (*PFN_ddRpcClientDestroy)(
    DDRpcClient hClient); /// [in] Handle to the existing client object

/// Execute a call
typedef DD_RESULT (*PFN_ddRpcClientCall)(
    DDRpcClient                hClient, /// [in] Handle to the existing client object
    const DDRpcClientCallInfo* pInfo
);

/// Checks if a service ID is currently registered to the server
typedef DD_RESULT (*PFN_ddRpcClientGetServiceInfo)(
    DDRpcClient          hClient,   /// [in] Handle to the client object
    const DDRpcServiceId serviceId, /// [in] Service ID to check
    DDApiVersion*        pVersion   /// [out] The service version
);

/// API structure
typedef struct DDRpcClientApi
{
    PFN_ddRpcClientQueryVersion       pfnQueryVersion;
    PFN_ddRpcClientQueryVersionString pfnQueryVersionString;
    PFN_ddRpcClientCreate             pfnCreate;
    PFN_ddRpcClientDestroy            pfnDestroy;
    PFN_ddRpcClientCall               pfnCall;
    PFN_ddRpcClientGetServiceInfo     pfnGetServiceInfo;
} DDRpcClientApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
