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

#include <rpcClient.h>

#include <ddCommon.h>
#include <ddRpcShared.h>

/// Api Exports ////////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace Rpc;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DDApiVersion ddRpcClientQueryVersion()
{
    DDApiVersion version = {};

    version.major = DD_RPC_CLIENT_API_MAJOR_VERSION;
    version.minor = DD_RPC_CLIENT_API_MINOR_VERSION;
    version.patch = DD_RPC_CLIENT_API_PATCH_VERSION;

    return version;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* ddRpcClientQueryVersionString()
{
    return DD_RPC_CLIENT_API_VERSION_STRING;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddRpcClientCreate(const DDRpcClientCreateInfo* pInfo, DDRpcClient* phClient)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    // clang-format off
    if ((phClient != nullptr) &&
        (pInfo != nullptr) &&
        (pInfo->hConnection != nullptr) &&
        (pInfo->clientId != DD_API_INVALID_CLIENT_ID))
    // clang-format on
    {
        RpcClient* pClient = new RpcClient();
        result             = pClient->Init(*pInfo);

        if (result == DD_RESULT_SUCCESS)
        {
            *phClient = pClient->Handle();
        }
        else
        {
            // Failed to init, likely a connection issue. Clean up and fail gracefully.
            delete pClient;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ddRpcClientDestroy(DDRpcClient hClient)
{
    if (hClient != DD_API_INVALID_HANDLE)
    {
        auto* pClient = RpcClient::FromHandle(hClient);
        delete pClient;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddRpcClientCall(DDRpcClient hClient, const DDRpcClientCallInfo* pInfo)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    // clang-format off
    if ((hClient != DD_API_INVALID_HANDLE)                                  &&
        (pInfo != nullptr)                                                  &&
        (pInfo->function != DD_RPC_INVALID_FUNC_ID)                         &&
        (pInfo->service != DD_RPC_INVALID_SERVICE_ID)                       &&
        ddIsVersionValid(pInfo->serviceVersion)                             &&
        ValidateOptionalBuffer(pInfo->pParamBuffer, pInfo->paramBufferSize) &&
        ((pInfo->pResponseWriter == nullptr) || IsValidDDByteWriter(pInfo->pResponseWriter)))
    // clang-format on
    {
        auto* pClient = RpcClient::FromHandle(hClient);
        result        = pClient->Call(*pInfo);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddRpcClientGetServiceInfo(DDRpcClient hClient, const DDRpcServiceId serviceId, DDApiVersion* pVersion)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;
    DynamicBufferByteWriter writer;
    DDRpcClientCallInfo info = {};

    info.pParamBuffer    = &serviceId;
    info.paramBufferSize = sizeof(serviceId);
    info.function        = DD_RPC_INVALID_FUNC_ID;
    info.service         = kServicesQueryRpcServiceId;
    info.serviceVersion  = RpcServicesQueryVersion();
    info.pResponseWriter = writer.Writer();

    if ((hClient != DD_API_INVALID_HANDLE) &&
        ValidateOptionalBuffer(info.pParamBuffer, info.paramBufferSize))
    {
        auto* pClient = RpcClient::FromHandle(hClient);
        result        = pClient->Call(info);

        if (result == DD_RESULT_SUCCESS)
        {
            DD_ASSERT(writer.Size() == sizeof(DDApiVersion));
            memcpy(pVersion, writer.Buffer(), sizeof(DDApiVersion));
        }
    }

    return result;
}
