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

#pragma once

#include <ddRpcServer.h>
#include <ddSocket.h>
#include <ddPlatform.h>
#include <util/vector.h>
#include <util/hashMap.h>

namespace Rpc
{

class RpcClientHandler;

/// Class responsible for managing the server side implementation of the RPC protocol
///
/// Services can be registered into this object to expose them to remote clients on the network
class RpcServer
{
public:
    RpcServer(DDNetConnection hConnection);
    ~RpcServer();

    // Configures this server object to listen to connections for the provided protocol
    //
    // This function can be called multiple times in order to change the protocol id
    DD_RESULT Listen(DDProtocolId protocolId);

    // Returns the client id associated with the underlying connection
    DDClientId QueryClientId() const;

    // Executes a request based on the parameters provided
    //
    // Any response data is returned through the writer interface
    DD_RESULT ExecuteRequest(
        DDRpcServiceId      serviceId,
        DDApiVersion        serviceVersion,
        DDRpcFunctionId     functionId,
        const void*         pParameterData,
        size_t              parameterDataSize,
        const DDByteWriter& writer
    );

    // Registers a new service on the server
    DD_RESULT RegisterService(const DDRpcServerRegisterServiceInfo& info);

    // Registers a new function on an existing service within the server
    DD_RESULT RegisterFunction(const DDRpcServerRegisterFunctionInfo& info);

    // Unregisters an existing service from the server
    void UnregisterService(DDRpcServiceId id);

    // Unregisters an existing function on an existing service within the server
    void UnregisterFunction(DDRpcServiceId serviceId, DDRpcFunctionId id);

    // Determines if a service is registered.
    DD_RESULT IsServiceRegistered(DDRpcServiceId id);

private:
    // Shuts down all internal operations and threads so the object can be destroyed or used with a new protocol id
    void Cleanup();

    // A function that runs on its own thread and handles any new incoming client connections
    void AcceptThreadFunc();

    // Internal allocator
    // Note: It's important that this allocator is used for ALL internal allocations so that the top level
    //       C functions work correctly across DLL boundaries.
    DevDriver::AllocCb m_allocCb;

    // Underlying network connection
    DDNetConnection m_hConnection;

    // Server socket used to listen for incoming client connections
    DDSocket m_hListenSocket;

    // Indicator used to stop the accept thread from looking for new connections when this object is being destroyed
    bool m_exitRequested;

    // Thread used to handle accept logic for new incoming clients
    DevDriver::Platform::Thread m_acceptThread;

    // Internal list of all currently active client handler objects
    // These are 1:1 with the number of clients we're currently talking to
    DevDriver::Vector<RpcClientHandler*> m_clients;

    // Structure used to represent an internally registered RPC function
    // Used to call into the application code during server side function execution
    struct RegisteredFunction
    {
        void*                     pFuncUserdata;
        PFN_ddRpcServerFunctionCb pfnFuncCb;
    };

    // Structure used to represent an internally registered RPC service
    struct RegisteredService
    {
        // Contains all functions registered on this service
        DevDriver::HashMap<DDRpcFunctionId, RegisteredFunction> functions;

        // Version of the service
        DDApiVersion version;

        RegisteredService(const DevDriver::AllocCb& allocCb, const DDApiVersion& version)
            : functions(allocCb)
            , version(version)
        {
        }
    };

    // Contains the set of all currently registered services
    DevDriver::HashMap<DDRpcServiceId, RegisteredService*> m_services;

    // Protects thread access to the services map
    DevDriver::Platform::AtomicLock m_servicesLock;
};

} // namespace Rpc
