/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#ifndef DD_RPC_CLIENT_HEADER
#define DD_RPC_CLIENT_HEADER

#include <ddRpcClientApi.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Get version of the loaded library to check interface compatibility
DDApiVersion ddRpcClientQueryVersion();

/// Get human-readable representation of the loaded library version
const char* ddRpcClientQueryVersionString();

/// Attempts to create a new client object with the provided creation information
DD_RESULT ddRpcClientCreate(
    const DDRpcClientCreateInfo* pInfo,     /// [in]  Create info
    DDRpcClient*                 phClient); /// [out] Handle to the new client object

/// Destroys an existing client object
void ddRpcClientDestroy(
    DDRpcClient hClient); /// [in] Handle to the existing client object

/// Execute a call
DD_RESULT ddRpcClientCall(
    DDRpcClient                hClient, /// [in] Handle to the existing client object
    const DDRpcClientCallInfo* pInfo
);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ! DD_RPC_CLIENT_HEADER
