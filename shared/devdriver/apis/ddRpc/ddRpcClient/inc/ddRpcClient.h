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

/// Checks if a service ID is currently registered to the server and returns the version it is using
DD_RESULT ddRpcClientGetServiceInfo(
    DDRpcClient          hClient,   /// [in]  Handle to the client object
    const DDRpcServiceId serviceId, /// [in]  Service ID to check
    DDApiVersion*        pVersion); /// [out] The service version

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

#endif
