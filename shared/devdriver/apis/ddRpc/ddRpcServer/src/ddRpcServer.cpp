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

#include <ddRpcServer.h>
#include <ddRpcShared.h>

#include <ddCommon.h>

#include <rpcServer.h>

using namespace DevDriver;
using namespace Rpc;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DDApiVersion ddRpcServerQueryVersion()
{
    DDApiVersion version = {};

    version.major = DD_RPC_SERVER_API_MAJOR_VERSION;
    version.minor = DD_RPC_SERVER_API_MINOR_VERSION;
    version.patch = DD_RPC_SERVER_API_PATCH_VERSION;

    return version;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* ddRpcServerQueryVersionString()
{
    return DD_RPC_SERVER_API_VERSION_STRING;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddRpcServerCreate(
    const DDRpcServerCreateInfo* pInfo,
    DDRpcServer*                 phServer)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((phServer != nullptr) &&
        (pInfo != nullptr) &&
        (pInfo->hConnection != nullptr))
    {
        // TODO: Add a common function to extract an AllocCb from a connection
        // See (#48) for more information about memory allocator handling in ddRpc
        RpcServer* pServer = DD_NEW(RpcServer, Platform::GenericAllocCb)(pInfo->hConnection);
        if (pServer != nullptr)
        {
            const DDProtocolId protocolId = (pInfo->protocolId == DD_API_INVALID_PROTOCOL_ID) ? kDefaultRpcProtocolId : pInfo->protocolId;
            result = pServer->Listen(protocolId);

            if (result == DD_RESULT_SUCCESS)
            {
                *phServer = reinterpret_cast<DDRpcServer>(pServer);
            }
            else
            {
                DD_DELETE(pServer, Platform::GenericAllocCb);
            }
        }
        else
        {
            result = DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ddRpcServerDestroy(
    DDRpcServer hServer)
{
    if (hServer != nullptr)
    {
        RpcServer* pServer = reinterpret_cast<RpcServer*>(hServer);
        DD_DELETE(pServer, Platform::GenericAllocCb);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddRpcServerRegisterService(
    DDRpcServer                           hServer,
    const DDRpcServerRegisterServiceInfo* pInfo)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((hServer != nullptr)                     &&
        (pInfo != nullptr)                       &&
        (pInfo->id != DD_RPC_INVALID_SERVICE_ID) &&
        ddIsVersionValid(pInfo->version)         &&
        (pInfo->pName != nullptr)                &&
        (pInfo->pDescription != nullptr))
    {
        RpcServer* pServer = reinterpret_cast<RpcServer*>(hServer);
        result = pServer->RegisterService(*pInfo);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ddRpcServerUnregisterService(
    DDRpcServer    hServer,
    DDRpcServiceId id)
{
    if ((hServer != nullptr) &&
        (id != DD_RPC_INVALID_SERVICE_ID))
    {
        RpcServer* pServer = reinterpret_cast<RpcServer*>(hServer);
        pServer->UnregisterService(id);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddRpcServerRegisterFunction(
    DDRpcServer                            hServer,
    const DDRpcServerRegisterFunctionInfo* pInfo)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((hServer != nullptr)                            &&
        (pInfo != nullptr)                              &&
        (pInfo->serviceId != DD_RPC_INVALID_SERVICE_ID) &&
        (pInfo->id != DD_RPC_INVALID_FUNC_ID)           &&
        (pInfo->pName != nullptr)                       &&
        (pInfo->pDescription != nullptr)                &&
        (pInfo->pfnFuncCb != nullptr))
    {
        RpcServer* pServer = reinterpret_cast<RpcServer*>(hServer);
        result = pServer->RegisterFunction(*pInfo);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ddRpcServerUnregisterFunction(
    DDRpcServer     hServer,
    DDRpcServiceId  serviceId,
    DDRpcFunctionId id)
{
    if ((hServer != nullptr)                     &&
        (serviceId != DD_RPC_INVALID_SERVICE_ID) &&
        (id != DD_RPC_INVALID_FUNC_ID))
    {
        RpcServer* pServer = reinterpret_cast<RpcServer*>(hServer);
        pServer->UnregisterFunction(serviceId, id);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DDClientId ddRpcServerQueryClientId(
    DDRpcServer hServer)
{
    DDClientId clientId = 0;

    if (hServer != DD_API_INVALID_HANDLE)
    {
        RpcServer* pServer = reinterpret_cast<RpcServer*>(hServer);
        clientId = pServer->QueryClientId();
    }

    return clientId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddRpcServerIsServiceRegistered(
    DDRpcServer    hServer,
    DDRpcServiceId serviceId)
{
    DD_RESULT result = DD_RESULT_DD_RPC_SERVICE_NOT_REGISTERED;

    if (hServer != nullptr)
    {
        RpcServer* pServer = reinterpret_cast<RpcServer*>(hServer);

        result = pServer->IsServiceRegistered(serviceId);
    }

    return result;
}
