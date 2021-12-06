/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

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
#define DD_RPC_CLIENT_API_MAJOR_VERSION 0
#define DD_RPC_CLIENT_API_MINOR_VERSION 2
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

/// API structure
typedef struct DDRpcClientApi
{
    PFN_ddRpcClientQueryVersion       pfnQueryVersion;
    PFN_ddRpcClientQueryVersionString pfnQueryVersionString;
    PFN_ddRpcClientCreate             pfnCreate;
    PFN_ddRpcClientDestroy            pfnDestroy;
    PFN_ddRpcClientCall               pfnCall;
} DDRpcClientApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ! DD_RPC_CLIENT_API_HEADER
