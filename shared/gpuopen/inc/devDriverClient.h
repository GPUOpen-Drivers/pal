/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  devDriverClient.h
* @brief Class declaration for DevDriverClient.
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "ddPlatform.h"
#include "msgTransport.h"
#include "protocols/systemProtocols.h"
#include "protocols/typemap.h"
#include "util/vector.h"
#include "protocolClient.h"

namespace DevDriver
{
    class IMsgChannel;
    class IProtocolClient;

    struct DevDriverClientCreateInfo
    {
        TransportCreateInfo transportCreateInfo;
    };

    class DevDriverClient
    {
    public:
        explicit DevDriverClient(const DevDriverClientCreateInfo& createInfo);
        ~DevDriverClient();

        Result Initialize();
        void Destroy();

        bool IsConnected() const;
        IMsgChannel* GetMessageChannel() const;

        template <Protocol protocol>
        ProtocolClientType<protocol>* AcquireProtocolClient()
        {
            using T = ProtocolClientType<protocol>;
            T* pProtocolClient = nullptr;

            Platform::LockGuard<Platform::AtomicLock> lock(m_clientLock);

            for (size_t index = 0; index < m_pUnusedClients.Size(); index++)
            {
                if (m_pUnusedClients[index]->GetProtocol() == protocol)
                {
                    pProtocolClient = static_cast<T*>(m_pUnusedClients[index]);
                    DD_ASSERT(pProtocolClient->GetProtocol() == protocol);
                    m_pUnusedClients.Remove(index);
                    m_pClients.PushBack(pProtocolClient);
                    return pProtocolClient;
                }
            }

            pProtocolClient = DD_NEW(T, m_createInfo.transportCreateInfo.allocCb)(m_pMsgChannel);
            if (pProtocolClient != nullptr)
            {
                m_pClients.PushBack(pProtocolClient);
            }

            return pProtocolClient;
        }

        void ReleaseProtocolClient(IProtocolClient* pProtocolClient)
        {
            Platform::LockGuard<Platform::AtomicLock> lock(m_clientLock);
            if (pProtocolClient != nullptr && (m_pClients.Remove(pProtocolClient) > 0))
            {
                pProtocolClient->Disconnect();
                m_pUnusedClients.PushBack(pProtocolClient);
            }
        }

    private:
        DD_STATIC_CONST uint32      kRegistrationTimeoutInMs = 1000;
        DevDriverClientCreateInfo   m_createInfo;
        IMsgChannel*                m_pMsgChannel;

        Platform::AtomicLock        m_clientLock;
        Vector<IProtocolClient*, 8> m_pClients;
        Vector<IProtocolClient*, 8> m_pUnusedClients;
    };

} // DevDriver
