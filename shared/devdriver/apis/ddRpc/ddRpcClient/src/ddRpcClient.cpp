/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

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
