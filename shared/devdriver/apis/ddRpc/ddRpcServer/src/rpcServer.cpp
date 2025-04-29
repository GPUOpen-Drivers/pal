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

#include <rpcServer.h>
#include <ddPlatform.h>
#include <ddCommon.h>
#include <rpcClientHandler.h>
#include <ddNet.h>

using namespace DevDriver;

namespace Rpc
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RpcServer::RpcServer(
    DDNetConnection hConnection)
    : m_allocCb(Platform::GenericAllocCb)
    , m_hConnection(hConnection)
    , m_hListenSocket(DD_API_INVALID_HANDLE)
    , m_exitRequested(false)
    , m_clients(m_allocCb)
    , m_services(m_allocCb)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RpcServer::~RpcServer()
{
    Cleanup();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RpcServer::Listen(
    DDProtocolId protocolId)
{
    if (m_hListenSocket != DD_API_INVALID_HANDLE)
    {
        Cleanup();
    }

    DD_ASSERT(m_hListenSocket == DD_API_INVALID_HANDLE);

    DDSocketListenInfo info = {};
    info.hConnection = m_hConnection;
    info.protocolId = protocolId;

    DD_RESULT result = ddSocketListen(&info, &m_hListenSocket);
    if (result == DD_RESULT_SUCCESS)
    {
        m_exitRequested = false;

        result = DevDriverToDDResult(m_acceptThread.Start([](void* pUserdata) {
            RpcServer* pThis = reinterpret_cast<RpcServer*>(pUserdata);
            pThis->AcceptThreadFunc();
        },
        this));

        if (result != DD_RESULT_SUCCESS)
        {
            Cleanup();
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DDClientId RpcServer::QueryClientId() const
{
    return ddNetQueryClientId(m_hConnection);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RpcServer::ExecuteRequest(
    DDRpcServiceId      serviceId,
    DDApiVersion        serviceVersion,
    DDRpcFunctionId     functionId,
    const void*         pParameterData,
    size_t              parameterDataSize,
    const DDByteWriter& writer)
{
    DD_RESULT result = DD_RESULT_UNKNOWN;

    Platform::LockGuard<Platform::AtomicLock> lock(m_servicesLock);

    RegisteredService* pService = m_services.FindPointer(serviceId);
    if (pService != nullptr)
    {
        if (ddIsMajorVersionCompatible(pService->version, serviceVersion))
        {
            RegisteredFunction* pFunction = pService->functions.FindValue(functionId);
            if (pFunction != nullptr)
            {
                DDRpcServerCallInfo call = {};

                call.pUserdata         = pFunction->pFuncUserdata;
                call.version           = serviceVersion;
                call.pParameterData    = pParameterData;
                call.parameterDataSize = parameterDataSize;
                call.pWriter           = &writer;

                result = pFunction->pfnFuncCb(&call);
            }
            else
            {
                result = DD_RESULT_DD_RPC_FUNC_NOT_REGISTERED;
            }
        }
        else
        {
            result = DD_RESULT_COMMON_VERSION_MISMATCH;

            DD_PRINT(
                LogLevel::Warn,
                "RPC call (service: 0x%x | function: 0x%x) routed to service with version %u.%u.%u which is incompatible with requested version %u.%u.%u",
                serviceId,
                functionId,
                pService->version.major,
                pService->version.minor,
                pService->version.patch,
                serviceVersion.major,
                serviceVersion.minor,
                serviceVersion.patch);
        }
    }
    else if (kServicesQueryRpcServiceId == serviceId)
    {
        // Handle the special case where the client is checking
        // if the service is connected or not:
        DD_ASSERT(parameterDataSize == sizeof(DDRpcServiceId));
        DD_ASSERT(pParameterData != nullptr);

        // Note: the services query has a separate versioning system from the RPC service itself:
        DDApiVersion servicesQueryVersion = RpcServicesQueryVersion();

        if (ddIsVersionCompatible(servicesQueryVersion, serviceVersion))
        {
            pService = m_services.FindPointer(*reinterpret_cast<const DDRpcServiceId*>(pParameterData));

            if (pService != nullptr)
            {
                result = writer.pfnBegin(writer.pUserdata, nullptr);

                if (result == DD_RESULT_SUCCESS)
                {
                    result = writer.pfnWriteBytes(writer.pUserdata, &pService->version, sizeof(DDApiVersion));
                    DD_ASSERT(result == DD_RESULT_SUCCESS);

                    writer.pfnEnd(writer.pUserdata, result);
                }
            }
            else
            {
                result = DD_RESULT_DD_RPC_SERVICE_NOT_REGISTERED;
            }
        }
        else
        {
            result = DD_RESULT_COMMON_VERSION_MISMATCH;

            DD_PRINT(
                LogLevel::Warn,
                "RPC call to reserved service 0x%x routed to service query version %u.%u.%u which is "
                "incompatible with requested version %u.%u.%u",
                serviceId,
                servicesQueryVersion.major,
                servicesQueryVersion.minor,
                servicesQueryVersion.patch,
                serviceVersion.major,
                serviceVersion.minor,
                serviceVersion.patch);
        }
    }
    else
    {
        result = DD_RESULT_DD_RPC_SERVICE_NOT_REGISTERED;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RpcServer::RegisterService(
    const DDRpcServerRegisterServiceInfo& info)
{
    DD_RESULT result = DD_RESULT_COMMON_ALREADY_EXISTS;

    // Many service IDs are ASCII encoded strings, so display them to aid readability.
    char idAsAscii[sizeof(DDRpcServiceId) + 1] = "";
    memcpy(&idAsAscii, &info.id, sizeof(info.id));

    DD_PRINT(LogLevel::Info, "Registering service: id=0x%x ('%s'), name=\"%s\"", info.id, idAsAscii, info.pName);

    Platform::LockGuard<Platform::AtomicLock> lock(m_servicesLock);

    if (m_services.Contains(info.id) == false)
    {
        RegisteredService* pService = DD_NEW(RegisteredService, m_allocCb)(m_allocCb, info.version);
        if (pService != nullptr)
        {
            result = DevDriverToDDResult(m_services.Create(info.id, pService));
            if (result != DD_RESULT_SUCCESS)
            {
                DD_DELETE(pService, m_allocCb);
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
DD_RESULT RpcServer::RegisterFunction(
    const DDRpcServerRegisterFunctionInfo& info)
{
    DD_RESULT result = DD_RESULT_COMMON_DOES_NOT_EXIST;

    Platform::LockGuard<Platform::AtomicLock> lock(m_servicesLock);

    RegisteredService* pService = m_services.FindPointer(info.serviceId);
    if (pService != nullptr)
    {
        RegisteredFunction function = {};
        function.pFuncUserdata = info.pFuncUserdata;
        function.pfnFuncCb     = info.pfnFuncCb;
        result = DevDriverToDDResult(pService->functions.Create(info.id, function));
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RpcServer::UnregisterService(
    DDRpcServiceId id)
{
    Platform::LockGuard<Platform::AtomicLock> lock(m_servicesLock);

    auto iter = m_services.Find(id);
    if (iter != m_services.End())
    {
        DD_DELETE(iter->value, m_allocCb);
        m_services.Remove(iter);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RpcServer::UnregisterFunction(
    DDRpcServiceId  serviceId,
    DDRpcFunctionId id)
{
    Platform::LockGuard<Platform::AtomicLock> lock(m_servicesLock);

    RegisteredService* pService = m_services.FindPointer(serviceId);
    if (pService != nullptr)
    {
        pService->functions.Erase(id);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RpcServer::IsServiceRegistered(DDRpcServiceId id)
{
    DD_RESULT result = DD_RESULT_DD_RPC_SERVICE_NOT_REGISTERED;

    Platform::LockGuard<Platform::AtomicLock> lock(m_servicesLock);

    RegisteredService* pService = m_services.FindPointer(id);
    if (pService != nullptr)
    {
        result = DD_RESULT_SUCCESS;
    }

    return result;
}
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RpcServer::Cleanup()
{
    // Shut down the accept thread so no new clients will be added while we're trying to clean up
    if (m_acceptThread.IsJoinable())
    {
        m_exitRequested = true;
        DD_UNHANDLED_RESULT(m_acceptThread.Join(1000));
    }

    // Close the listen socket which will disconnect all clients spawned from it.
    if (m_hListenSocket != DD_API_INVALID_HANDLE)
    {
        ddSocketClose(m_hListenSocket);
        m_hListenSocket = DD_API_INVALID_HANDLE;
    }

    // Destroy any remaining clients now that they've been disconnected
    for (auto pClient : m_clients)
    {
        DD_DELETE(pClient, m_allocCb);
    }
    m_clients.Clear();

    // Destroy all registered services
    for (auto serviceIter : m_services)
    {
        DD_DELETE(serviceIter.value, m_allocCb);
    }
    m_services.Clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RpcServer::AcceptThreadFunc()
{
    while (m_exitRequested == false)
    {
        DDSocket hClientSocket = DD_API_INVALID_HANDLE;
        DD_RESULT result = ddSocketAccept(m_hListenSocket, 250, &hClientSocket);
        if (result == DD_RESULT_SUCCESS)
        {
            // A new client connected so we need to spin off a new thread to handle them
            RpcClientHandler* pClient = DD_NEW(RpcClientHandler, m_allocCb)(this, hClientSocket);
            if (pClient != nullptr)
            {
                result = pClient->Initialize();

                if (result == DD_RESULT_SUCCESS)
                {
                    result = m_clients.PushBack(pClient) ? DD_RESULT_SUCCESS : DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
                }

                if (result != DD_RESULT_SUCCESS)
                {
                    DD_DELETE(pClient, m_allocCb);
                }
            }
            else
            {
                result = DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
                ddSocketClose(hClientSocket);
            }
        }
        else if (result == DD_RESULT_DD_GENERIC_NOT_READY)
        {
            // We timed out
            // Try again next loop after checking for an exit request
        }
        else
        {
            // Unexpected error
            // Exit the loop
            break;
        }

        // Clean up any inactive clients
        auto clientIter = m_clients.Begin();
        for (; clientIter != m_clients.End(); ++clientIter)
        {
            RpcClientHandler* pClient = (*clientIter);
            if (pClient->IsActive() == false)
            {
                DD_DELETE(pClient, m_allocCb);

                clientIter = m_clients.Remove(clientIter);
            }
        }
    }
}

} // namespace Rpc
