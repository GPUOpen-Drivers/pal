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
* @file  messageChannel.h
* @brief Class declaration for BaseMsgChannel
***********************************************************************************************************************
*/

#pragma once

#include "msgChannel.h"
#include "msgTransport.h"
#include "sessionManager.h"
#include "util/queue.h"
#include "ddPlatform.h"
#include "protocols/systemProtocols.h"
#include "ddTransferManager.h"
#include "protocols/ddURIServer.h"
#include "ddClientURIService.h"

namespace DevDriver
{
    template <class T>
    class MessageChannel : public IMsgChannel
    {
        static void MsgChannelReceiveFunc(void* pThreadParam);
        DD_STATIC_CONST uint32 kMaxBufferedMessages = 64;

    public:
        template <class ...Args>
        MessageChannel(const TransportCreateInfo& createInfo, Args&&... args);

        ~MessageChannel();

        void Update(uint32 timeoutInMs = kDefaultUpdateTimeoutInMs) override;

        Result Register(uint32 timeoutInMs = kInfiniteTimeout) override;
        Result Unregister() override;
        bool IsConnected() override;

        Result SetStatusFlags(StatusFlags flags) override;
        StatusFlags GetStatusFlags() const override;

        Result Send(ClientId dstClientId, Protocol protocol, MessageCode message,
                    const ClientMetadata& metadata,
                    uint32 payloadSizeInBytes,
                    const void* pPayload) override;

        Result Receive(MessageBuffer& message, uint32 timeoutInMs) override;
        Result Forward(const MessageBuffer& messageBuffer) override;

        Result EstablishSession(ClientId dstClientId, IProtocolClient* pClient) override;
        Result RegisterProtocolServer(IProtocolServer* pServer) override;
        Result UnregisterProtocolServer(IProtocolServer* pServer) override;
        IProtocolServer* GetProtocolServer(Protocol protocol) override;

        ClientId GetClientId() const override;

        const ClientInfoStruct& GetClientInfo() const override { return m_clientInfoResponse; }

        Result FindFirstClient(const ClientMetadata& filter,
                               ClientId*             pClientId,
                               uint32                timeoutInMs,
                               ClientMetadata*       pClientMetadata) override;

        const AllocCb& GetAllocCb() const override { return m_createInfo.allocCb; }

        TransferProtocol::TransferManager& GetTransferManager() override { return m_transferManager; }

        Result RegisterService(URIProtocol::URIService* pService) override
        {
            DD_ASSERT(pService != nullptr);
            DD_ASSERT(m_pURIServer != nullptr);

            return m_pURIServer->RegisterService(pService);
        }

        Result UnregisterService(URIProtocol::URIService* pService) override
        {
            DD_ASSERT(pService != nullptr);
            DD_ASSERT(m_pURIServer != nullptr);

            return m_pURIServer->UnregisterService(pService);
        }

    protected:
        struct MsgThreadInfo
        {
            volatile bool active;
        };

        struct ReceiveQueue
        {
            Queue<MessageBuffer, kMaxBufferedMessages>  queue;
            Platform::Semaphore                         semaphore;
            Platform::AtomicLock                        lock;

            explicit ReceiveQueue(const AllocCb& allocCb)
                : queue(allocCb)
                , semaphore(0, kMaxBufferedMessages) {}
        };

        Result CreateMsgThread();
        Result DestroyMsgThread();

        Result Disconnect();
        bool HandleMessageReceived(const MessageBuffer &messageBuffer);

        Result SendSystem(ClientId dstClientId, SystemProtocol::SystemMessage message, const ClientMetadata &metadata);

#ifdef DEVDRIVER_ENABLE_PACKET_LOSS
        // Returns true if a packet should be dropped. Used for testing.
        bool ShouldDropPacket()
        {
            // Generate a random value between 0.0 and 1.0.
            const float dropValue = (static_cast<float>(m_packetLossRng.Generate()) /
                                     static_cast<float>(m_packetLossRng.Max()));

            // Return true to drop the packet if the random value is below the packet loss ratio/threshold.
            return (dropValue < static_cast<float>(DEVDRIVER_PACKET_LOSS_RATIO));
        }

        // Write a message into the internal transport
        Result WriteTransportMessage(const MessageBuffer& messageBuffer)
        {
            // If we're testing packet loss and we want to drop a packet, return Success without
            // actually writing the message into the transport.
            if (ShouldDropPacket())
            {
                return Result::Success;
            }

            return m_msgTransport.WriteMessage(messageBuffer);
        }

        // Reads a message from the internal transport
        Result ReadTransportMessage(MessageBuffer& messageBuffer, uint32 timeoutInMs)
        {
            Result result = m_msgTransport.ReadMessage(messageBuffer, timeoutInMs);

            // If we're testing packet loss and we want to drop a packet, read the message out of the
            // transport but tell the caller that we didn't get anything.
            if ((result == Result::Success) && ShouldDropPacket())
            {
                result = Result::NotReady;
            }

            return result;
        }
#else
        // Write a message into the internal transport
        Result WriteTransportMessage(const MessageBuffer& messageBuffer)
        {
            return m_msgTransport.WriteMessage(messageBuffer);
        }

        // Reads a message from the internal transport
        Result ReadTransportMessage(MessageBuffer& messageBuffer, uint32 timeoutInMs)
        {
            return m_msgTransport.ReadMessage(messageBuffer, timeoutInMs);
        }
#endif

#ifdef DEVDRIVER_ENABLE_PACKET_LOSS
        // Random number generator for packet loss testing.
        Platform::Random                  m_packetLossRng;
#endif

        ClientId                          m_clientId;
        SessionManager                    m_sessionManager;
        Platform::Thread                  m_msgThread;
        MsgThreadInfo                     m_msgThreadParams;
        T                                 m_msgTransport;
        const TransportCreateInfo         m_createInfo;
        ClientInfoStruct                  m_clientInfoResponse;

        ReceiveQueue                      m_receiveQueue;

        DD_STATIC_CONST uint64            kKeepAliveTimeout      = 2000;
        DD_STATIC_CONST uint64            kKeepAliveThreshold    = 5;
        DD_STATIC_CONST uint64            kRetransmitTimeoutInMs = 50;

        volatile uint64                   m_lastActivityTimeMs;
        SessionId                         m_lastKeepaliveTransmitted;
        SessionId                         m_lastKeepaliveReceived;
        Platform::Semaphore               m_updateSemaphore;

        TransferProtocol::TransferManager m_transferManager;
        URIProtocol::URIServer*           m_pURIServer;
        ClientURIService                  m_clientURIService;
    };

} // DevDriver
#include "messageChannel.inl"
