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

#ifndef DD_RPC_SERVER_API_HEADER
#define DD_RPC_SERVER_API_HEADER

#include <ddApi.h>

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////// Data Definitions (enums, structs, defines) ////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Compile time version information
#define DD_RPC_SERVER_API_MAJOR_VERSION 0
#define DD_RPC_SERVER_API_MINOR_VERSION 3
#define DD_RPC_SERVER_API_PATCH_VERSION 0

#define DD_RPC_SERVER_API_VERSION_STRING DD_API_STRINGIFY_VERSION(DD_RPC_SERVER_API_MAJOR_VERSION, \
                                                                  DD_RPC_SERVER_API_MINOR_VERSION, \
                                                                  DD_RPC_SERVER_API_PATCH_VERSION)

/// Name of the API
#define DD_RPC_SERVER_API_NAME        "ddRpcServer"

/// Description of the API
#define DD_RPC_SERVER_API_DESCRIPTION "Server-side API for remote procedure calls over DevDriver"

/// Identifier for the API
/// This identifier is used to acquire access to the API's interface
// Note: This is "drvrpc_s" in big endian ASCII
#define DD_RPC_SERVER_API_ID 0x6472767270635f73

/// Opaque handle to a developer driver remote procedure call server
typedef struct DDRpcServer_t* DDRpcServer;

/// Structure that contains the information required to create a server
typedef struct DDRpcServerCreateInfo
{
    DDNetConnection  hConnection; /// A handle to an existing connection object
    DDProtocolId     protocolId;  /// The identifier of the protocol to use for connections
                                  //< Specify 0 for the standard default id.
                                  //< This is effectively the "port".
} DDRpcServerCreateInfo;

/// Structure that contains the information required to register an RPC service on a server
typedef struct DDRpcServerRegisterServiceInfo
{
    DDRpcServiceId       id;           /// Unique identifier for the service
                                       //< This is used by clients to remotely call functions
    DDApiVersion         version;      /// Version of the service
                                       //< The implementation will ensure that incompatible requests are not forwarded
                                       //< to user authored service functions. Compatibility is determined using
                                       //< semantic versioning.
    const char*          pName;        /// Name of the service
    const char*          pDescription; /// Description of the service
} DDRpcServerRegisterServiceInfo;

/// Structure that contains the information required to call a user provided function
typedef struct DDRpcServerCallInfo
{
    void*               pUserdata;         /// [in] Userdata pointer
    DDApiVersion        version;           /// Compatibility requirements from the client
    const void*         pParameterData;    /// [in] Pointer to data associated with the function's parameters
    size_t              parameterDataSize; /// Size of the data pointed to by pParameterData
    const DDByteWriter* pWriter;           /// Writer user to return data to the caller
} DDRpcServerCallInfo;

/// Function prototype for a function called via the remote procedure call system
///
/// NOTE: This function will be called from background thread in response to a request from the network.
///       All synchronization requirements MUST be handled within the function's implementation.
typedef DD_RESULT (*PFN_ddRpcServerFunctionCb)(
    const DDRpcServerCallInfo* pCall); /// [in] Information provided by the server as part of a call operation

/// Structure that contains the information required to register an RPC function on a previously registered RPC service
typedef struct DDRpcServerRegisterFunctionInfo
{
    DDRpcServiceId            serviceId;     /// Unique identifier of the service to register the function with
    DDRpcFunctionId           id;            /// Unique identifier for the function
                                             //< This is used by clients to remotely call functions
    const char*               pName;         /// Name of the function
    const char*               pDescription;  /// Description of the function
    void*                     pFuncUserdata; /// Userdata pointer provided to the function callback
                                             //< [Optional]
    PFN_ddRpcServerFunctionCb pfnFuncCb;     /// Callback to invoke when requested via the network
} DDRpcServerRegisterFunctionInfo;

/// Get version of the loaded library to check interface compatibility
typedef DDApiVersion (*PFN_ddRpcServerQueryVersion)(
    void);

/// Get human-readable representation of the loaded library version
typedef const char* (*PFN_ddRpcServerQueryVersionString)(
    void);

/// Attempts to create a new server object with the provided creation information
typedef DD_RESULT (*PFN_ddRpcServerCreate)(
    const DDRpcServerCreateInfo* pInfo,     /// [in]  Create info
    DDRpcServer*                 phServer); /// [out] Handle to the new server object

/// Destroys an existing server object
typedef void (*PFN_ddRpcServerDestroy)(
    DDRpcServer hServer); /// [in] Handle to the existing server object

/// Attempts to register a new RPC service on the provided server
typedef DD_RESULT (*PFN_ddRpcServerRegisterService)(
    DDRpcServer                           hServer, /// [in] Handle to an existing RPC server
    const DDRpcServerRegisterServiceInfo* pInfo);  /// [in] Service registration info

/// Unregisters a previously registered RPC service from the provided server if it's currently registered
typedef void (*PFN_ddRpcServerUnregisterService)(
    DDRpcServer    hServer, /// [in] Handle to an existing RPC server
    DDRpcServiceId id);     /// Service identifier

/// Attempts to register a new RPC function on the provided server
typedef DD_RESULT (*PFN_ddRpcServerRegisterFunction)(
    DDRpcServer                            hServer, /// [in] Handle to an existing RPC server
    const DDRpcServerRegisterFunctionInfo* pInfo);  /// [in] Function registration info

/// Unregisters a previously registered RPC function from the provided server if it's currently registered
typedef void (*PFN_ddRpcServerUnregisterFunction)(
    DDRpcServer     hServer,   /// [in] Handle to an existing RPC server
    DDRpcServiceId  serviceId, /// Service identifier
    DDRpcFunctionId id);       /// Function identifier

/// Returns the network client id associated with an existing RPC server or 0 if an invalid handle is provided
typedef DDClientId (*PFN_ddRpcServerQueryClientId)(
    DDRpcServer hServer); /// [in] Handle to an existing RPC server

/// Returns DD_RESULT_SUCCESS if the service is registered
typedef DD_RESULT (*PFN_ddRpcServerIsServiceRegistered)(
    DDRpcServer    hServer,    /// [in] Handle to an existing RPC server
    DDRpcServiceId serviceId); /// [in] Service identifier

/// API structure
typedef struct DDRpcServerApi
{
    PFN_ddRpcServerQueryVersion        pfnQueryVersion;
    PFN_ddRpcServerQueryVersionString  pfnQueryVersionString;
    PFN_ddRpcServerCreate              pfnCreate;
    PFN_ddRpcServerDestroy             pfnDestroy;
    PFN_ddRpcServerRegisterService     pfnRegisterService;
    PFN_ddRpcServerUnregisterService   pfnUnregisterService;
    PFN_ddRpcServerRegisterFunction    pfnRegisterFunction;
    PFN_ddRpcServerUnregisterFunction  pfnUnregisterFunction;
    PFN_ddRpcServerQueryClientId       pfnQueryClientId;
    PFN_ddRpcServerIsServiceRegistered pfnIsServiceRegistered;
} DDRpcServerApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
