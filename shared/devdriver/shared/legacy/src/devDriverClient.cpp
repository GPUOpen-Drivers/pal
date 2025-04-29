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

#include "devDriverClient.h"
#include "messageChannel.h"
#include "protocolClient.h"
#include "socketMsgTransport.h"
#include "protocols/driverControlClient.h"
#include "protocols/rgpClient.h"
#include "protocols/etwClient.h"

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
        if ((m_createInfo.connectionInfo.type == TransportType::Remote) |
            (m_createInfo.connectionInfo.type == TransportType::Local))
        {
            using MsgChannelSocket = MessageChannel<SocketMsgTransport>;
            m_pMsgChannel = DD_NEW(MsgChannelSocket, m_allocCb)(m_allocCb,
                                                                m_createInfo,
                                                                m_createInfo.connectionInfo);
        }
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
