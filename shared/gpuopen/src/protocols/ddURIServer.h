/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
***********************************************************************************************************************
* @file  ddURIServer.h
* @brief Class declaration for URIServer.
***********************************************************************************************************************
*/

#pragma once

#include "baseProtocolServer.h"
#include "util/string.h"
#include "util/vector.h"
#include "ddUriInterface.h"

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

            void Finalize() override;

            bool AcceptSession(const SharedPointer<ISession>& pSession) override;
            void SessionEstablished(const SharedPointer<ISession>& pSession) override;
            void UpdateSession(const SharedPointer<ISession>& pSession) override;
            void SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason) override;

            // Adds a service to the list of registered server.
            Result RegisterService(IService* pService);

            // Removes a service from the list of registered server.
            Result UnregisterService(IService* pService);

            // Get the names of all currently registered services
            void GetServiceNames(Vector<FixedString<kMaxUriServiceNameLength>>& pServiceNames);

        private:
            // Returns a pointer to a service that was registered with a name that matches pServiceName.
            // Returns nullptr if there is no service registered with a matching name.
            IService* FindService(const char* pServiceName);

            // Looks up and services the request provided.
            Result ServiceRequest(const char*        pServiceName,
                                  URIRequestContext* pRequestContext);

            // Mutex used for synchronizing the registered services list.
            Platform::Mutex m_mutex;

            // Mutex used for synchronizing the service names cache, which can be held independently of 'm_mutex'.
            Platform::Mutex m_cacheMutex;

            // A list of all the registered services.
            // @todo: Replace this vector with a map.
            Vector<IService*, 8> m_registeredServices;

            // A list of the names of all of the registered services.
            // Any service in m_registeredServices should have its name in here, but they may be reordered.
            // e.g. If service "Foo" is at index 2 in m_registeredServices,
            //      there is no guarentee that m_registeredServiceNamesCache[2] == "Foo";
            Vector<FixedString<kMaxUriServiceNameLength>, 8> m_registeredServiceNamesCache;
        };
    }
} // DevDriver
