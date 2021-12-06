/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#ifndef DD_RPC_SERVER_HEADER
#define DD_RPC_SERVER_HEADER

#include <ddRpcServerApi.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Get version of the loaded library to check interface compatibility
DDApiVersion ddRpcServerQueryVersion();

/// Get human-readable representation of the loaded library version
const char* ddRpcServerQueryVersionString();

/// Attempts to create a new server object with the provided creation information
DD_RESULT ddRpcServerCreate(
    const DDRpcServerCreateInfo* pInfo,     /// [in]  Create info
    DDRpcServer*                 phServer); /// [out] Handle to the new server object

/// Destroys an existing server object
void ddRpcServerDestroy(
    DDRpcServer hServer); /// [in] Handle to the existing server object

/// Attempts to register a new RPC service on the provided server
DD_RESULT ddRpcServerRegisterService(
    DDRpcServer                           hServer, /// [in] Handle to an existing RPC server
    const DDRpcServerRegisterServiceInfo* pInfo);  /// [in] Service registration info

/// Unregisters a previously registered RPC service from the provided server if it's currently registered
void ddRpcServerUnregisterService(
    DDRpcServer    hServer, /// [in] Handle to an existing RPC server
    DDRpcServiceId id);     /// Service identifier

/// Attempts to register a new RPC function on the provided server
DD_RESULT ddRpcServerRegisterFunction(
    DDRpcServer                            hServer, /// [in] Handle to an existing RPC server
    const DDRpcServerRegisterFunctionInfo* pInfo);  /// [in] Function registration info

/// Unregisters a previously registered RPC function from the provided server if it's currently registered
void ddRpcServerUnregisterFunction(
    DDRpcServer     hServer,   /// [in] Handle to an existing RPC server
    DDRpcServiceId  serviceId, /// Service identifier
    DDRpcFunctionId id);       /// Function identifier

/// Returns the network client id associated with an existing RPC server or 0 if an invalid handle is provided
DDClientId ddRpcServerQueryClientId(
    DDRpcServer hServer); /// [in] Handle to an existing RPC server

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ! DD_RPC_SERVER_HEADER
