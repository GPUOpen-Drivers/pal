/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#include "devDriverClient.h"
#include "messageChannel.h"
#include "protocolClient.h"
#include "socketMsgTransport.h"
#include "protocols/driverControlClient.h"
#include "protocols/rgpClient.h"
#include "protocols/etwClient.h"

#if DD_PLATFORM_WINDOWS_UM
    #include "win/ddWinPipeMsgTransport.h"
#endif

namespace DevDriver
{
    DevDriverClient::DevDriverClient(const AllocCb&          allocCb,
                                     const ClientCreateInfo& createInfo)
        : m_pMsgChannel(nullptr)
        , m_pClients(allocCb)
        , m_pUnusedClients(allocCb)
        , m_allocCb(allocCb)
        , m_createInfo(createInfo)
    {}

    DevDriverClient::~DevDriverClient()
    {
        Destroy();
    }

    Result DevDriverClient::Initialize()
    {
        Result result = Result::Error;
#if defined(DD_PLATFORM_WINDOWS_UM)
        if (m_createInfo.connectionInfo.type == TransportType::Local)
        {
            using MsgChannelPipe = MessageChannel<WinPipeMsgTransport>;
            m_pMsgChannel = DD_NEW(MsgChannelPipe, m_allocCb)(m_allocCb,
                                                              m_createInfo,
                                                              m_createInfo.connectionInfo);
        }
        else if (m_createInfo.connectionInfo.type == TransportType::Remote ||
                 m_createInfo.connectionInfo.type == TransportType::RemoteReliable)
        {
            using MsgChannelSocket = MessageChannel<SocketMsgTransport>;
            m_pMsgChannel = DD_NEW(MsgChannelSocket, m_allocCb)(m_allocCb,
                                                                m_createInfo,
                                                                m_createInfo.connectionInfo);
        }
#else
        if ((m_createInfo.connectionInfo.type == TransportType::Remote) |
            (m_createInfo.connectionInfo.type == TransportType::RemoteReliable) |
            (m_createInfo.connectionInfo.type == TransportType::Local))
        {
            using MsgChannelSocket = MessageChannel<SocketMsgTransport>;
            m_pMsgChannel = DD_NEW(MsgChannelSocket, m_allocCb)(m_allocCb,
                                                                m_createInfo,
                                                                m_createInfo.connectionInfo);
        }
#endif
        else
        {
            // Invalid transport type
            DD_WARN_REASON("Invalid transport type specified");
        }

        if (m_pMsgChannel != nullptr)
        {
            result = m_pMsgChannel->Register(kRegistrationTimeoutInMs);
            if (result != Result::Success)
            {
                // We failed to initialize so we need to destroy the message channel.
                DD_DELETE(m_pMsgChannel, m_allocCb);
                m_pMsgChannel = nullptr;
            }
        }

        return result;
    }

    void DevDriverClient::Destroy()
    {
        if (m_pMsgChannel != nullptr)
        {
            m_pMsgChannel->Unregister();

            for (auto &pClient : m_pClients)
            {
                DD_DELETE(pClient, m_allocCb);
            }
            m_pClients.Clear();

            for (auto &pClient : m_pUnusedClients)
            {
                DD_DELETE(pClient, m_allocCb);
            }
            m_pUnusedClients.Clear();

            DD_DELETE(m_pMsgChannel, m_allocCb);
            m_pMsgChannel = nullptr;
        }
    }

    bool DevDriverClient::IsConnected() const
    {
        if (m_pMsgChannel)
            return m_pMsgChannel->IsConnected();
        return false;
    }

    IMsgChannel* DevDriverClient::GetMessageChannel() const
    {
        return m_pMsgChannel;
    }
} // DevDriver
