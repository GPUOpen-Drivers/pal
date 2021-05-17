/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include "baseProtocolServer.h"
#include "util/string.h"
#include "util/hashMap.h"
#include "util/vector.h"
#include "ddUriInterface.h"

#include <protocols/ddInternalService.h>

namespace DevDriver
{
    namespace URIProtocol
    {
        // The protocol server implementation for the uri protocol.
        class URIServer : public BaseProtocolServer
        {
        public:
            explicit URIServer(IMsgChannel* pMsgChannel);
            ~URIServer();

            void Finalize() override final;

            bool AcceptSession(const SharedPointer<ISession>& pSession) override final;
            void SessionEstablished(const SharedPointer<ISession>& pSession) override final;
            void UpdateSession(const SharedPointer<ISession>& pSession) override final;
            void SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason) override final;

            // Adds a service to the list of registered server.
            Result RegisterService(IService* pService);

            // Removes a service from the list of registered server.
            Result UnregisterService(IService* pService);

            // Looks up the service to validate the block size requested by a client for a specific URI request.
            Result ValidatePostRequest(const char* pServiceName, char* pRequestArguments, uint32 sizeRequested);

        private:
            // This struct is used to cache information about registered URI services to lookup services and efficiently
            // respond to "services" and "version" queries.
            struct ServiceInfo
            {
                IService*                             pService;
                FixedString<kMaxUriServiceNameLength> name;
                Version                               version;
            };

            // Returns a pointer to a service that was registered with a name that matches pServiceName.
            // Returns nullptr if there is no service registered with a matching name.
            IService* FindService(const char* pServiceName);

            // Looks up and services the request provided.
            Result ServiceRequest(const char*         pServiceName,
                                  IURIRequestContext* pRequestContext);

            // Callback to query registered services for use with RouterInternalService
            // This must only be called when the internal mutex is already owned. It does not lock internally.
            static Result QueryRegisteredServices(void* pUserdata, Vector<const IService*>* pServices);

            // Mutex used for synchronizing the registered services list.
            Platform::Mutex m_mutex;

            // A hashmap of all the registered services.
            HashMap<uint64, ServiceInfo, 8> m_registeredServices;

            // An always-available service for diagnostic and information queries
            InternalService m_internalService;

            class URISession;
        };
    }
} // DevDriver
