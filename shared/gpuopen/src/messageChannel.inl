/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "messageChannel.h"
#include "protocolServer.h"
#include "protocolClient.h"

namespace DevDriver
{
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Thread function that calls the message channel update function until the active flag becomes false
    template <class MsgTransport>
    void MessageChannel<MsgTransport>::MsgChannelReceiveFunc(void* pThreadParam)
    {
        MessageChannel* pMessageChannel = reinterpret_cast<MessageChannel*>(pThreadParam);

        while (pMessageChannel->m_msgThreadParams.active)
        {
            if (pMessageChannel->IsConnected())
            {
                // If we're still connected, update the message channel
                pMessageChannel->Update();
            }
            else
            {
                // We're no longer connected so we should terminate this background thread by breaking out of
                // the loop.

                DD_PRINT(LogLevel::Info, "Message channel lost connection, exiting receive thread loop");

                break;
            }
        }

        DD_PRINT(LogLevel::Info, "Exiting receive thread");
    }

    template <class MsgTransport>
    template <class ...Args>
    MessageChannel<MsgTransport>::MessageChannel(const AllocCb&                  allocCb,
                                                 const MessageChannelCreateInfo& createInfo,
                                                 Args&&...                       args) :
        m_msgTransport(Platform::Forward<Args>(args)...),
        m_receiveQueue(allocCb),
        m_clientId(kBroadcastClientId),
        m_allocCb(allocCb),
        m_createInfo(createInfo),
        m_clientInfoResponse(),
        m_lastActivityTimeMs(0),
        m_lastKeepaliveTransmitted(0),
        m_lastKeepaliveReceived(0),
        m_msgThread(),
        m_msgThreadParams(),
        m_updateSemaphore(1, 1),
        m_sessionManager(allocCb),
        m_transferManager(allocCb),
        m_pURIServer(nullptr),
        m_clientURIService()
    {
    }

    template <class MsgTransport>
    MessageChannel<MsgTransport>::~MessageChannel()
    {
        Unregister();
    }

    template <class MsgTransport>
    void MessageChannel<MsgTransport>::Update(uint32 timeoutInMs)
    {
        MessageBuffer messageBuffer = {};

        // Attempt to read a message from the queue with a timeout.
        if (m_updateSemaphore.Wait(kLogicFailureTimeout) == Result::Success)
        {
            Result status = ReadTransportMessage(messageBuffer, timeoutInMs);
            while (status == Result::Success)
            {
                // Read any remaining messages in the queue without waiting on a timeout until the queue is empty.
                if (!HandleMessageReceived(messageBuffer))
                {
                    Platform::LockGuard<Platform::AtomicLock> lock(m_receiveQueue.lock);
                    if (m_receiveQueue.queue.PushBack(messageBuffer))
                    {
                        m_receiveQueue.semaphore.Signal();
                    }
                }
                status = ReadTransportMessage(messageBuffer, kNoWait);
            }

            if (status != Result::NotReady)
            {
                Disconnect();
            }
            else if (MsgTransport::RequiresClientRegistration() & MsgTransport::RequiresKeepAlive())
            {
                // if keep alive is enabled and the last message read wasn't an error
                uint64 currentTime = Platform::GetCurrentTimeInMs();

                // only check the keep alive threshold if we haven't had any network traffic in kKeepAliveTimeout time
                if ((currentTime - m_lastActivityTimeMs) > kKeepAliveTimeout)
                {
                    // if we have gone <kKeepAliveThreshold> heartbeats without reponse we disconnect
                    if ((m_lastKeepaliveTransmitted - m_lastKeepaliveReceived) < kKeepAliveThreshold)
                    {
                        // send a heartbeat and increment the last keepalive transmitted variable
                        using namespace DevDriver::ClientManagementProtocol;
                        MessageBuffer heartbeat = kOutOfBandMessage;
                        heartbeat.header.messageId = static_cast<MessageCode>(ManagementMessage::KeepAlive);
                        heartbeat.header.sessionId = ++m_lastKeepaliveTransmitted;
                        Forward(heartbeat);

                        // we need to update the last activity time to make sure it doesn't immediately timeout again
                        m_lastActivityTimeMs = currentTime;
                    }
                    else
                    {
                        DD_PRINT(LogLevel::Info, "Disconnecting transport due to keep alive timeout");

                        // we have sent too many heartbeats without response, so disconnect
                        Disconnect();
                    }
                }
            }

            // Give the session manager a chance to update its sessions.
            m_sessionManager.UpdateSessions();

            m_updateSemaphore.Signal();

#if defined(DD_PLATFORM_LINUX_UM)
            // we yield the thread after processing messages to let other threads grab the lock if the need to
            // this works around an issue where the message processing thread releases the lock then reacquires
            // it before a sleeping thread that is waiting on it can get it.
            Platform::Sleep(0);
#endif
        }
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::EstablishSessionForClient(
        SharedPointer<ISession>*    ppSession,
        const EstablishSessionInfo& sessionInfo)
    {
        return m_sessionManager.EstablishSessionForClient(ppSession, sessionInfo);
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::RegisterProtocolServer(IProtocolServer* pServer)
    {
        Result result = Result::Error;
        if (pServer != nullptr)
        {
            ProtocolFlags enabledProtocols = {};
            switch (pServer->GetProtocol())
            {
            case Protocol::Logging:
            {
                enabledProtocols.logging = true;
                break;
            }

            case Protocol::Settings:
            {
                enabledProtocols.settings = true;
                break;
            }

            case Protocol::DriverControl:
            {
                enabledProtocols.driverControl = true;
                break;
            }

            case Protocol::RGP:
            {
                enabledProtocols.rgp = true;
                break;
            }

            case Protocol::ETW:
            {
                enabledProtocols.etw = true;
                break;
            }

            case Protocol::GpuCrashDump:
            {
                enabledProtocols.gpuCrashDump = true;
                break;
            }

            case Protocol::Event:
            {
                enabledProtocols.event = true;
                break;
            }

            default:
            {
                DD_WARN_REASON("Registered protocol server for unknown protocol");
                break;
            }
            }

            result = m_sessionManager.RegisterProtocolServer(pServer);
            if (result == Result::Success)
            {
                m_clientInfoResponse.metadata.protocols.value |= enabledProtocols.value;
            }
        }
        return result;
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::UnregisterProtocolServer(IProtocolServer* pServer)
    {
        // @todo: Remove enabled protocol metadata flags related to pServer

        return m_sessionManager.UnregisterProtocolServer(pServer);
    }

    template <class MsgTransport>
    ClientId MessageChannel<MsgTransport>::GetClientId() const
    {
        return m_clientId;
    }

    template <class MsgTransport>
    inline Result MessageChannel<MsgTransport>::FindFirstClient(const ClientMetadata& filter,
                                                                ClientId*             pClientId,
                                                                uint32                timeoutInMs,
                                                                ClientMetadata*       pClientMetadata)
    {
        using namespace DevDriver::SystemProtocol;
        Result result = Result::Error;

        if (pClientId != nullptr)
        {
            result = Result::NotReady;
            // acquire the update semaphore. this prevents the update thread from processing messages
            // as it is possible it could process messages the client was looking for
            if (m_updateSemaphore.Wait(kLogicFailureTimeout) == Result::Success)
            {
                const uint64 startTime = Platform::GetCurrentTimeInMs();
                uint64 timeDelta = 0;

                MessageBuffer messageBuffer = {};

                // we're going to loop through sending a ping, receiving/processing any messages, then updating sessions
                do
                {
                    // send a ping every time the outer loop executes
                    result = SendSystem(kBroadcastClientId,
                                        SystemMessage::Ping,
                                        filter);

                    // there are two expected results from SendSystem: Success or NotReady
                    // if it is successful, we need to transition to waiting
                    if (result == Result::Success)
                    {
                        // read any traffic waiting
                        result = ReadTransportMessage(messageBuffer, kDefaultUpdateTimeoutInMs);

                        // we are going to loop until either an error is encountered or there is no data remaining
                        // the expected behavior is that this loop will exit with result == Result::NotReady
                        while (result == Result::Success)
                        {
                            // if the default message handler doesn't care about this message we inspect it
                            if (!HandleMessageReceived(messageBuffer))
                            {
                                // did we receive any pong messages?
                                if ((messageBuffer.header.protocolId == Protocol::System) &
                                    (static_cast<SystemMessage>(messageBuffer.header.messageId) == SystemMessage::Pong))
                                {
                                    // Non-session messages don't have a sequence number. Instead the sequence field is
                                    // aliased to contain ClientMetadata.
                                    const ClientMetadata metadata(messageBuffer.header.sequence);

                                    // check to see if it matches with our initial filter
                                    if (filter.Matches(metadata))
                                    {
                                        // if it does, write out the client ID
                                        *pClientId = messageBuffer.header.srcClientId;

                                        // If the pClientMetadata pointer is valid, fill it with the matching client
                                        // metadata struct data.
                                        if (pClientMetadata != nullptr)
                                        {
                                            memcpy(pClientMetadata, &metadata, sizeof(ClientMetadata));
                                        }

                                        // if we found a matching client we break out of the inner loop.
                                        // the outer loop will exit automatically because result is implied to be Success
                                        // inside the execution of this loop
                                        break;
                                    }
                                }
                                else
                                {
                                    // if this message wasn't one we were looking for, we go ahead and enqueue
                                    // the message in the local receive queue
                                    Platform::LockGuard<Platform::AtomicLock> lock(m_receiveQueue.lock);
                                    if (m_receiveQueue.queue.PushBack(messageBuffer))
                                    {
                                        m_receiveQueue.semaphore.Signal();
                                    }
                                }
                            }

                            // read the next message with no timeout
                            // this ensures that this inner loop will exit immediately when no data is remaining
                            result = ReadTransportMessage(messageBuffer, kNoWait);
                        }

                        // Give the session manager a chance to update its sessions.
                        m_sessionManager.UpdateSessions();
                    }
                    else if (result == Result::NotReady)
                    {
                        // the write failed because the transport was busy, so we sleep and will try again
                        Platform::Sleep(Platform::Min(timeoutInMs, kDefaultUpdateTimeoutInMs));
                    }

                    // calculate how much time has elapsed since we started looping
                    timeDelta = Platform::GetCurrentTimeInMs() - startTime;

                    // we repeat this loop so long as:
                    //  1. result is NotReady, which implies that either the write or last read timed out
                    //  2. The loop haven't exceed the specified timeout
                    // we exit the outer loop if:
                    //  1. The inner loop exited with result equal to Success, indicating we found a client
                    //  2. result indicates an unexpected error
                    //  3. We have timed out, in which case result is already NotReady
                } while ((result == Result::NotReady) &
                         (timeDelta < timeoutInMs));

                // release the update lock
                m_updateSemaphore.Signal();
            }
        }
        return result;
    }

    template <class MsgTransport>
    bool MessageChannel<MsgTransport>::HandleMessageReceived(const MessageBuffer& messageBuffer)
    {
        using namespace DevDriver::SystemProtocol;
        using namespace DevDriver::ClientManagementProtocol;

        bool handled = false;
        bool forThisHost = false;

        // todo: move this out into message reading loop so that it isn't getting done for every message
        if (MsgTransport::RequiresClientRegistration() & MsgTransport::RequiresKeepAlive())
        {
            m_lastActivityTimeMs = Platform::GetCurrentTimeInMs();
        }

        if (messageBuffer.header.protocolId == Protocol::Session)
        {
            // We should never receive a session message that wasn't intended for us. If we do, it means there's a
            // serious problem in one of the router implementations.
            DD_ASSERT(messageBuffer.header.dstClientId == m_clientId);

            m_sessionManager.HandleReceivedSessionMessage(messageBuffer);
            handled = true;
        }
        else if (IsOutOfBandMessage(messageBuffer))
        {
            handled = true; // always filter these out from the client
            if (IsValidOutOfBandMessage(messageBuffer)
                & (static_cast<ManagementMessage>(messageBuffer.header.messageId) == ManagementMessage::KeepAlive))
            {
                DD_PRINT(LogLevel::Debug, "Received keep alive response seq %u", messageBuffer.header.sessionId);
                m_lastKeepaliveReceived = messageBuffer.header.sessionId;
            }
        }
        else
        {
            using namespace DevDriver::SystemProtocol;
            const ClientId &dstClientId = messageBuffer.header.dstClientId;
            const ClientMetadata metadata(messageBuffer.header.sequence);
            forThisHost = !!((((dstClientId == kBroadcastClientId)
                               & (metadata.Matches(m_clientInfoResponse.metadata)))
                              | ((m_clientId != kBroadcastClientId) & (dstClientId == m_clientId))));
            if ((forThisHost) & (messageBuffer.header.protocolId == Protocol::System))
            {
                const ClientId &srcClientId = messageBuffer.header.srcClientId;

                const SystemMessage& message = static_cast<SystemMessage>(messageBuffer.header.messageId);

                switch (message)
                {
                case SystemMessage::Ping:
                {
                    SendSystem(srcClientId,
                               SystemMessage::Pong,
                               m_clientInfoResponse.metadata);

                    handled = true;

                    break;
                }
                case SystemMessage::QueryClientInfo:
                {
                    Send(srcClientId,
                         Protocol::System,
                         static_cast<MessageCode>(SystemMessage::ClientInfo),
                         m_clientInfoResponse.metadata,
                         sizeof(m_clientInfoResponse),
                         &m_clientInfoResponse);

                    handled = true;

                    break;
                }
                case SystemMessage::ClientDisconnected:
                {
                    m_sessionManager.HandleClientDisconnection(srcClientId);
                    break;
                }
                case SystemMessage::Halted:
                {
                    // If the application has an event callback set up, forward the halted event and consider it handled.
                    // If there's no callback handler set up, this event is placed into the receive queue along with any
                    // other unhandled messages.
                    if (m_createInfo.pfnEventCallback != nullptr)
                    {
                        BusEventClientHalted clientHalted = {};
                        clientHalted.clientId = srcClientId;

                        m_createInfo.pfnEventCallback(m_createInfo.pUserdata,
                                                      BusEventType::ClientHalted,
                                                      &clientHalted,
                                                      sizeof(BusEventClientHalted));
                        handled = true;
                    }
                    break;
                }
                default:
                {
                    // Unhandled system message
                    break;
                }
                }
            }
        }
        return (handled | (!forThisHost));
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::Send(ClientId dstClientId, Protocol protocol, MessageCode message, const ClientMetadata &metadata, uint32 payloadSizeInBytes, const void* pPayload)
    {
        MessageBuffer messageBuffer = {};
        messageBuffer.header.dstClientId = dstClientId;
        messageBuffer.header.srcClientId = m_clientId;
        messageBuffer.header.protocolId = protocol;
        messageBuffer.header.messageId = message;
        messageBuffer.header.payloadSize = payloadSizeInBytes;
        // Non-session messages don't have a sequence number.  Instead we alias the sequence field to send the ClientMetadata.
        // If the size of ClientMetadata changes to grow beyond the size of the sequence field, we should fail the build.
        static_assert(sizeof(ClientMetadata) <= sizeof(Sequence), "ClientMetada size changed, can't alias Sequence as ClientMetadata");
        memcpy(&messageBuffer.header.sequence, &metadata, sizeof(Sequence));

        if ((pPayload != nullptr) & (payloadSizeInBytes != 0))
        {
            memcpy(messageBuffer.payload, pPayload, Platform::Min(payloadSizeInBytes, static_cast<uint32>(sizeof(messageBuffer.payload))));
        }
        return Forward(messageBuffer);
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::SendSystem(ClientId dstClientId, SystemProtocol::SystemMessage message, const ClientMetadata &metadata)
    {
        MessageBuffer messageBuffer = {};
        messageBuffer.header.dstClientId = dstClientId;
        messageBuffer.header.srcClientId = m_clientId;
        messageBuffer.header.protocolId = Protocol::System;
        messageBuffer.header.messageId = static_cast<MessageCode>(message);
        // Non-session messages don't have a sequence number.  Instead we alias the sequence field to send the ClientMetadata.
        // If the size of ClientMetadata changes to grow beyond the size of the sequence field, we should fail the build.
        static_assert(sizeof(ClientMetadata) <= sizeof(Sequence), "ClientMetada size changed, can't alias Sequence as ClientMetadata");
        memcpy(&messageBuffer.header.sequence, &metadata, sizeof(Sequence));
        return Forward(messageBuffer);
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::Forward(const MessageBuffer& messageBuffer)
    {
        Result result = Result::Error;
        if (m_clientId != kBroadcastClientId)
        {
            result = WriteTransportMessage(messageBuffer);
            if ((result != Result::Success) & (result != Result::NotReady))
            {
                Disconnect();
            }
        }
        return result;
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::Receive(MessageBuffer& message, uint32 timeoutInMs)
    {
        Result result = Result::Unavailable;
        if ((m_receiveQueue.queue.Size() > 0) | (m_clientId != kBroadcastClientId))
        {
            result = m_receiveQueue.semaphore.Wait(timeoutInMs);
            if (result == Result::Success)
            {
                Platform::LockGuard<Platform::AtomicLock> lock(m_receiveQueue.lock);
                m_receiveQueue.queue.PopFront(message);
            }
        }
        return result;
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::Register(uint32 timeoutInMs)
    {
        Result status = Result::Error;

        if (m_clientId == kBroadcastClientId)
        {
            status = m_msgTransport.Connect(&m_clientId, timeoutInMs);
        }

        if (MsgTransport::RequiresClientRegistration())
        {
            if ((status == Result::Success) & (m_clientId == kBroadcastClientId))
            {
                using namespace DevDriver::ClientManagementProtocol;
                MessageBuffer recvBuffer = {};
                MessageBuffer messageBuffer = kOutOfBandMessage;
                messageBuffer.header.messageId = static_cast<MessageCode>(ManagementMessage::ConnectRequest);
                messageBuffer.header.payloadSize = sizeof(ConnectRequestPayload);

                {
                    ConnectRequestPayload* DD_RESTRICT pConnectionRequest =
                        reinterpret_cast<ConnectRequestPayload*>(&messageBuffer.payload[0]);
                    pConnectionRequest->componentType = m_createInfo.componentType;
                    pConnectionRequest->initialClientFlags = m_createInfo.initialFlags;
                }

                uint64 sendTime = Platform::GetCurrentTimeInMs();
                uint64 currentTime = sendTime;

                Result registerResult;
                do
                {
                    registerResult = WriteTransportMessage(messageBuffer);

                    if (registerResult == Result::Success)
                    {
                        registerResult = ReadTransportMessage(recvBuffer, kRetransmitTimeoutInMs);
                        if (registerResult == Result::Success)
                        {
                            registerResult = Result::NotReady;
                            if (recvBuffer.header.protocolId == Protocol::ClientManagement)
                            {
                                registerResult = Result::VersionMismatch;
                                if (IsOutOfBandMessage(recvBuffer) &
                                    IsValidOutOfBandMessage(recvBuffer) &
                                    (static_cast<ManagementMessage>(recvBuffer.header.messageId) == ManagementMessage::ConnectResponse))
                                {
                                    ConnectResponsePayload* DD_RESTRICT pConnectionResponse =
                                        reinterpret_cast<ConnectResponsePayload*>(&recvBuffer.payload[0]);
                                    registerResult = pConnectionResponse->result;
                                    m_clientId = pConnectionResponse->clientId;
                                }
                            }
                        }
                    }

                    currentTime = Platform::GetCurrentTimeInMs();
                } while ((registerResult == Result::NotReady) & ((currentTime - sendTime) < timeoutInMs));

                status = registerResult;
            }
        }

        if (status == Result::Success)
        {
            memset(&m_clientInfoResponse, 0, sizeof(m_clientInfoResponse));
            Platform::Strncpy(m_clientInfoResponse.clientDescription, m_createInfo.clientDescription, sizeof(m_clientInfoResponse.clientDescription));
            Platform::GetProcessName(m_clientInfoResponse.clientName, sizeof(m_clientInfoResponse.clientName));
            m_clientInfoResponse.processId = Platform::GetProcessId();
            m_clientInfoResponse.metadata.clientType = m_createInfo.componentType;
            m_clientInfoResponse.metadata.status = m_createInfo.initialFlags;

            status = ((m_sessionManager.Init(this) == Result::Success) ? Result::Success : Result::Error);

            // Initialize the transfer manager
            if (status == Result::Success)
            {
                status = ((m_transferManager.Init(this, &m_sessionManager) == Result::Success) ? Result::Success : Result::Error);
            }

            // Initialize the URI server
            if (status == Result::Success)
            {
                m_pURIServer = DD_NEW(URIProtocol::URIServer, m_allocCb)(this);
                status = (m_pURIServer != nullptr) ? Result::Success : Result::Error;
            }

            // Register the URI server
            if (status == Result::Success)
            {
                m_sessionManager.RegisterProtocolServer(m_pURIServer);
            }

            // Set up internal URI services
            if (status == Result::Success)
            {
                m_clientURIService.BindMessageChannel(this);
                m_pURIServer->RegisterService(&m_clientURIService);
            }

            if ((status == Result::Success) & m_createInfo.createUpdateThread)
            {
                status = CreateMsgThread();
            }
        }

        return status;
    }

    template <class MsgTransport>
    void MessageChannel<MsgTransport>::Unregister()
    {
        if (m_createInfo.createUpdateThread)
        {
            DestroyMsgThread();
        }

        if (m_pURIServer != nullptr)
        {
            m_sessionManager.UnregisterProtocolServer(m_pURIServer);

            DD_DELETE(m_pURIServer, m_allocCb);
            m_pURIServer = nullptr;
        }

        m_transferManager.Destroy();

        m_sessionManager.Destroy();

        if (MsgTransport::RequiresClientRegistration())
        {
            if (m_clientId != kBroadcastClientId)
            {
                using namespace DevDriver::ClientManagementProtocol;
                MessageBuffer disconnectMsgBuffer = {};
                disconnectMsgBuffer.header.protocolId = Protocol::ClientManagement;
                disconnectMsgBuffer.header.messageId =
                    static_cast<MessageCode>(ManagementMessage::DisconnectNotification);
                disconnectMsgBuffer.header.srcClientId = m_clientId;
                disconnectMsgBuffer.header.dstClientId = kBroadcastClientId;
                disconnectMsgBuffer.header.payloadSize = 0;

                WriteTransportMessage(disconnectMsgBuffer);
            }
        }

        Disconnect();
    }

    template <class MsgTransport>
    inline bool MessageChannel<MsgTransport>::IsConnected()
    {
        return (m_clientId != kBroadcastClientId);
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::SetStatusFlags(StatusFlags flags)
    {
        Result status = Result::Error;
        if (m_clientId != kBroadcastClientId)
        {
            m_clientInfoResponse.metadata.status = flags;
            status = Result::Success;
        }
        return status;
    }

    template <class MsgTransport>
    StatusFlags MessageChannel<MsgTransport>::GetStatusFlags() const
    {
        return m_clientInfoResponse.metadata.status;
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::CreateMsgThread()
    {
        memset(&m_msgThreadParams, 0, sizeof(m_msgThreadParams));

        m_msgThreadParams.active = true;

        Result result = m_msgThread.Start(MsgChannelReceiveFunc, this);

        if (result == Result::Success)
        {
            // This is for humans, so we ignore a failure to set the name. The code can't do anything about it anyway.
            m_msgThread.SetName("DevDriver MsgChannel Receiver");
        }
        else
        {
            m_msgThreadParams.active = false;
            DD_WARN_REASON("Thread creation failed");
        }

        return DD_SANITIZE_RESULT(result);
    }

    template <class MsgTransport>
    void MessageChannel<MsgTransport>::DestroyMsgThread()
    {
        if (m_msgThread.IsJoinable())
        {
            m_msgThreadParams.active = false;
            DD_UNHANDLED_RESULT(m_msgThread.Join(kLogicFailureTimeout));
        }
    }

    template <class MsgTransport>
    void MessageChannel<MsgTransport>::Disconnect()
    {
        if (m_clientId != kBroadcastClientId)
        {
            m_clientId = kBroadcastClientId;
            DD_UNHANDLED_RESULT(m_msgTransport.Disconnect());

            // Notify the session manager that the transport has been disconnected
            m_sessionManager.HandleTransportDisconnect();
        }
    }

    template <class MsgTransport>
    IProtocolServer *MessageChannel<MsgTransport>::GetProtocolServer(Protocol protocol)
    {
        return m_sessionManager.GetProtocolServer(protocol);
    }

} // DevDriver
