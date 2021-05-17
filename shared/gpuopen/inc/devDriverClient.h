/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include "gpuopen.h"
#include "ddPlatform.h"
#include "msgChannel.h"
#include "msgTransport.h"
#include "protocols/systemProtocols.h"
#include "protocols/typemap.h"
#include "util/vector.h"
#include "protocolClient.h"

namespace DevDriver
{
    class IMsgChannel;
    class IProtocolClient;

    // Client Creation Info
    // This struct extends the MessageChannelCreateInfo struct and adds information about the destination host
    // the client will connect to. See msgChannel.h for a full list of members.
    struct ClientCreateInfo : public MessageChannelCreateInfo
    {
        HostInfo                 connectionInfo;    // Connection information describing how the Server should connect
                                                    // to the message bus.
    };

    class DevDriverClient
    {
    public:
        explicit DevDriverClient(const AllocCb& allocCb,
                                 const ClientCreateInfo& createInfo);
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

            pProtocolClient = DD_NEW(T, m_allocCb)(m_pMsgChannel);
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
        IMsgChannel*                m_pMsgChannel;
        Platform::AtomicLock        m_clientLock;
        Vector<IProtocolClient*, 8> m_pClients;
        Vector<IProtocolClient*, 8> m_pUnusedClients;

        // Allocator and create info are stored at the end of the struct since they will be used infrequently
        AllocCb                     m_allocCb;
        ClientCreateInfo            m_createInfo;
    };

} // DevDriver
