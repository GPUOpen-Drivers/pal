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
#include "util/hashSet.h"

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

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    template <class MsgTransport>
    bool MessageChannel<MsgTransport>::FindFirstClientDiscoverFunc(void* pUserdata, const DiscoveredClientInfo& info)
    {
        FindFirstClientContext* pContext = reinterpret_cast<FindFirstClientContext*>(pUserdata);

        // The discovery callback should never be called with an invalid client id
        DD_ASSERT(info.id != kBroadcastClientId);

        (*pContext->pClientId) = info.id;

        if (pContext->pClientMetadata != nullptr)
        {
            (*pContext->pClientMetadata) = info.metadata;
        }

        // Find first client always stops discovery after the first client
        return false;
    }

    template <class MsgTransport>
    template <class ...Args>
    MessageChannel<MsgTransport>::MessageChannel(const AllocCb&                  allocCb,
                                                 const MessageChannelCreateInfo& createInfo,
                                                 Args&&...                       args) :
        m_msgTransport(Platform::Forward<Args>(args)...),
        m_discoveredClientsQueue(allocCb),
        m_clientId(kBroadcastClientId),
        m_allocCb(allocCb),
        m_createInfo(createInfo),
        m_clientInfoResponse(),
        m_lastActivityTimeMs(0),
        m_lastKeepaliveTransmitted(0),
        m_lastKeepaliveReceived(0),
        m_msgThread(),
        m_msgThreadParams(),
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
        Result result = ReadTransportMessage(messageBuffer, timeoutInMs);
        while (result == Result::Success)
        {
            // Handle the message
            HandleMessageReceived(messageBuffer);

            // Read any remaining messages in the queue without waiting on a timeout until the queue is empty.
            result = ReadTransportMessage(messageBuffer, kNoWait);
        }

        // Once we finish processing all the available messages, handle client discovery if necessary.
        if ((result == Result::NotReady) && (m_discoveredClientsQueue.active))
        {
            using namespace DevDriver::SystemProtocol;

            // We're in the middle of a client discovery operation, so keep sending out pings until we finish
            // the operation.
            result = Send(kBroadcastClientId,
                          Protocol::System,
                          static_cast<MessageCode>(SystemMessage::Ping),
                          m_discoveredClientsQueue.filter,
                          sizeof(m_clientInfoResponse),
                          &m_clientInfoResponse);

            // Make sure to change the result back to NotReady if we successfully send out the ping.
            // We should still allow errors to propagate out though.
            if (result == Result::Success)
            {
                result = Result::NotReady;
            }
        }

        if (result != Result::NotReady)
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

#if defined(DD_PLATFORM_LINUX_UM)
        // we yield the thread after processing messages to let other threads grab the lock if the need to
        // this works around an issue where the message processing thread releases the lock then reacquires
        // it before a sleeping thread that is waiting on it can get it.
        Platform::Sleep(0);
#endif
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
    inline Result MessageChannel<MsgTransport>::DiscoverClients(const DiscoverClientsInfo& info)
    {
        // Start the discovery process by setting our client metadata filter and activating
        // the client discovery queue.
        {
            Platform::LockGuard<Platform::AtomicLock> lock(m_discoveredClientsQueue.lock);

            m_discoveredClientsQueue.filter = info.filter;
            m_discoveredClientsQueue.active = true;
        }

        const uint64 startTime = Platform::GetCurrentTimeInMs();

        DevDriver::HashSet<uint32, 16u> clientHashSet(m_allocCb);

        // Wait until we have a new client entry to process
        Result result = m_discoveredClientsQueue.hasDataEvent.Wait(info.timeoutInMs);
        while (result == Result::Success)
        {
            DiscoveredClientInfo clientInfo = {};

            // Retrieve the client info from the queue
            {
                Platform::LockGuard<Platform::AtomicLock> lock(m_discoveredClientsQueue.lock);

                // We should never have an empty queue while the hasDataEvent is signaled.
                DD_ASSERT(m_discoveredClientsQueue.clients.IsEmpty() == false);

                DD_UNHANDLED_RESULT(m_discoveredClientsQueue.clients.PopBack(&clientInfo) ? Result::Success : Result::Error);

                if (m_discoveredClientsQueue.clients.IsEmpty())
                {
                    // Clear the event if the queue is now empty.
                    m_discoveredClientsQueue.hasDataEvent.Clear();
                }
            }

            bool continueDiscovery = false;

            // Automatically filter out duplicate clients
            // This can occur because the implementation may receive multiple responses to the discovery ping
            // from the same client.
            if (clientHashSet.Contains(clientInfo.id) == false)
            {
                // This is a new client, attempt to add it to our hash set
                result = clientHashSet.Insert(clientInfo.id);

                if (result == Result::Success)
                {
                    // Notify the caller as long as we're successful and see if they want to continue discovery.
                    continueDiscovery = info.pfnCallback(info.pUserdata, clientInfo);
                }
                else
                {
                    // We've encountered some sort of memory failure. This will abort the discovery process.
                }
            }
            else
            {
                // We've already seen this client, continue discovery without notifying the caller.
                continueDiscovery = true;
            }

            if (result == Result::Success)
            {
                if (continueDiscovery)
                {
                    // The client requested to continue discovery or we encountered a duplicate client.
                    // Check if we have more time to continue discovery.
                    const uint64 elapsedTime = (Platform::GetCurrentTimeInMs() - startTime);
                    if (elapsedTime < info.timeoutInMs)
                    {
                        // We still have time, wait for more client information to appear in the queue.
                        const uint32 timeoutRemaining = static_cast<uint32>(info.timeoutInMs - elapsedTime);
                        result = m_discoveredClientsQueue.hasDataEvent.Wait(timeoutRemaining);
                    }
                    else
                    {
                        // The timeout has expired, return the to caller.
                        result = Result::NotReady;
                    }
                }
                else
                {
                    // The caller has signaled that they're no longer interested in discovering more clients.
                    // Break out of the loop because the caller indicated that they're done with discovery.
                    break;
                }
            }
        }

        // Stop the discovery process by deactivating the client discovery queue and clearing its contents.
        {
            Platform::LockGuard<Platform::AtomicLock> lock(m_discoveredClientsQueue.lock);

            m_discoveredClientsQueue.active = false;

            m_discoveredClientsQueue.clients.Clear();
            m_discoveredClientsQueue.hasDataEvent.Clear();
        }

        return result;
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
            // Use our special context and function for client discovery to implement FindFirstClient.
            // The specialized discover function returns after first discovered client that matches our
            // client specifications.

            FindFirstClientContext context = {};
            context.pClientId              = pClientId;
            context.pClientMetadata        = pClientMetadata;

            DiscoverClientsInfo discoverInfo = {};
            discoverInfo.pfnCallback         = &FindFirstClientDiscoverFunc;
            discoverInfo.pUserdata           = &context;
            discoverInfo.filter              = filter;
            discoverInfo.timeoutInMs         = timeoutInMs;

            result = DiscoverClients(discoverInfo);
        }

        return result;
    }

    template <class MsgTransport>
    void MessageChannel<MsgTransport>::HandleMessageReceived(const MessageBuffer& messageBuffer)
    {
        using namespace DevDriver::SystemProtocol;
        using namespace DevDriver::ClientManagementProtocol;

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
        }
        else if (IsOutOfBandMessage(messageBuffer))
        {
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

            const ClientId dstClientId = messageBuffer.header.dstClientId;
            const ClientMetadata metadata(messageBuffer.header.sequence);

            const bool isDirectedMessage = (dstClientId == m_clientId);
            const bool isRelevantBroadcastMessage = ((dstClientId == kBroadcastClientId) && metadata.Matches(m_clientInfoResponse.metadata));
            const bool isForThisHost = (isDirectedMessage || isRelevantBroadcastMessage);

            if ((messageBuffer.header.protocolId == Protocol::System) && isForThisHost)
            {
                const ClientId &srcClientId = messageBuffer.header.srcClientId;

                const SystemMessage& message = static_cast<SystemMessage>(messageBuffer.header.messageId);

                switch (message)
                {
                case SystemMessage::Ping:
                {
                    bool shouldRespond = true;

                    // If we have an event handler callback installed, give the application a chance to
                    // decide if we should respond to this message.
                    if (m_createInfo.pfnEventCallback != nullptr)
                    {
                        const ClientInfoStruct* pClientInfo = nullptr;

                        // Older versions of the ping packet didn't include the client info structure so
                        // it may not always be available.
                        if (messageBuffer.header.payloadSize != 0)
                        {
                            pClientInfo = reinterpret_cast<const ClientInfoStruct*>(messageBuffer.payload);
                        }

                        BusEventPongRequest pongRequest = {};

                        pongRequest.clientId       = srcClientId;
                        pongRequest.pClientInfo    = pClientInfo;
                        pongRequest.pShouldRespond = &shouldRespond;

                        m_createInfo.pfnEventCallback(m_createInfo.pUserdata,
                                                      BusEventType::PongRequest,
                                                      &pongRequest,
                                                      sizeof(pongRequest));
                    }

                    // Send a response if necessary
                    if (shouldRespond)
                    {
                        Send(srcClientId,
                            Protocol::System,
                            static_cast<MessageCode>(SystemMessage::Pong),
                            m_clientInfoResponse.metadata,
                            sizeof(m_clientInfoResponse),
                            &m_clientInfoResponse);
                    }

                    break;
                }
                case SystemMessage::Pong:
                {
                    Platform::LockGuard<Platform::AtomicLock> lock(m_discoveredClientsQueue.lock);

                    // If the discovered clients queue is currently in use, add a new entry for this client into it.
                    // We just ignore these messages otherwise.
                    if (m_discoveredClientsQueue.active)
                    {
                        if (m_discoveredClientsQueue.filter.Matches(metadata))
                        {
                            DiscoveredClientInfo clientInfo = {};
                            clientInfo.id                   = srcClientId;
                            clientInfo.metadata             = metadata;

                            if (messageBuffer.header.payloadSize == 0)
                            {
                                // Older versions of the pong packet didn't include the client info structure so
                                // it may not always be available.
                            }
                            else if (messageBuffer.header.payloadSize == sizeof(clientInfo.clientInfo.data))
                            {
                                // Valid, but this new version includes client info to aid discovery.
                                // Copy it out, only if the sizes match exactly.
                                clientInfo.clientInfo.valid = true;
                                memcpy(&clientInfo.clientInfo.data, messageBuffer.payload, sizeof(clientInfo.clientInfo.data));
                            }
                            else
                            {
                                DD_ASSERT_REASON("Pong packet with wrong size");
                            }

                            if (m_discoveredClientsQueue.clients.PushBack(clientInfo))
                            {
                                m_discoveredClientsQueue.hasDataEvent.Signal();
                            }
                            else
                            {
                                DD_ASSERT_REASON("Failed to insert discovered client into queue!");
                            }
                        }
                    }

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

                    break;
                }
                case SystemMessage::ClientDisconnected:
                {
                    m_sessionManager.HandleClientDisconnection(srcClientId);

                    break;
                }
                case SystemMessage::Halted:
                {
                    // Forward this message to the installed event handler if we have one
                    if (m_createInfo.pfnEventCallback != nullptr)
                    {
                        const ClientInfoStruct& clientInfo = *reinterpret_cast<const ClientInfoStruct*>(messageBuffer.payload);

                        BusEventClientHalted clientHalted = {};

                        clientHalted.clientId   = srcClientId;
                        clientHalted.clientInfo = clientInfo;

                        m_createInfo.pfnEventCallback(m_createInfo.pUserdata,
                                                      BusEventType::ClientHalted,
                                                      &clientHalted,
                                                      sizeof(clientHalted));
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
        DD_UNUSED(message);
        DD_UNUSED(timeoutInMs);

        return Result::NotReady;
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

                                // @TODO: If we receive a regular broadcast packet here, we should ignore it instead of assuming that
                                //        we have a version mismatch here.

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
