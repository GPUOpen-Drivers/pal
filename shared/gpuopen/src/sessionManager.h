/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include "gpuopen.h"
#include "msgChannel.h"
#include "protocolServer.h"
#include "protocolClient.h"
#include "session.h"
#include "util/vector.h"
#include "util/sharedptr.h"
#include "util/hashMap.h"

namespace DevDriver
{
    class SessionManager
    {
    public:
        // Constructor
        explicit SessionManager(const AllocCb& allocCb);

        // Destructor
        ~SessionManager();

        // Initialize the session manager.
        Result Init(IMsgChannel* pMessageChannel);

        // Destroy the session manager, closing all sessions in the process.
        void Destroy();

        // Creates a session with the specified remote client, using the provided protocol client.
        Result EstablishSessionForClient(SharedPointer<ISession>* ppSession, const EstablishSessionInfo& sessionInfo);

        // Process a session message.
        void HandleReceivedSessionMessage(const MessageBuffer& messageBuffer);

        // Updates all active sessions.
        void UpdateSessions();

        // Registers the protocol server provided.
        Result RegisterProtocolServer(IProtocolServer* pServer);

        // Unregisters the protocol server provided and closes all associated connections.
        Result UnregisterProtocolServer(IProtocolServer* pServer);

        // Get the pointer to the protocol server associated with the provided protocol, or nullptr.
        IProtocolServer* GetProtocolServer(Protocol protocol);

        // Returns true if the protocol associated with the provided protocol exists, false otherwise.
        bool HasProtocolServer(Protocol protocol);

        // Notify the session manager that the destination client has disconnected.
        void HandleClientDisconnection(ClientId dstClientId);

        // Notify the session manager that the underlying transport has disconnected
        void HandleTransportDisconnect();

        // Returns the currently associated ClientId, or kBroadcastClientId if not connected.
        ClientId GetClientId() const { return m_clientId; };
    private:
        // Server hash map goes from Protocol -> IProtocolServer*, with 8 buckets
        using ServerHashMap = HashMap<Protocol, IProtocolServer*>;
        // Session hash map goes from SessionId -> SharedPointer<Session> with a default of 16 buckets
        using SessionHashMap = HashMap<SessionId, SharedPointer<Session>, 16>;

        // Convenience method to send a command packet (e.g., one with no payload) with the given parameters
        Result SendCommand(
            ClientId    remoteClientId,
            MessageCode command,
            SessionId   sessionId,
            Sequence    sequenceNumber,
            WindowSize  windowSize)
        {
            MessageBuffer messageBuffer = {};
            messageBuffer.header.dstClientId = remoteClientId;
            messageBuffer.header.srcClientId = m_clientId;
            messageBuffer.header.protocolId  = Protocol::Session;
            messageBuffer.header.messageId   = command;
            messageBuffer.header.sessionId   = sessionId;
            messageBuffer.header.sequence    = sequenceNumber;
            messageBuffer.header.payloadSize = 0;
            messageBuffer.header.windowSize  = windowSize;
            return m_pMessageChannel->Forward(messageBuffer);
        }

        // Convenience method to send a reset packet
        Result SendReset(ClientId remoteClientId, uint32 remoteSessionId, Result reason, Version version);

        SessionId GetNewSessionId(SessionId remoteSessionId);
        SharedPointer<Session> FindOpenSession(SessionId sessionId);

        // Shuts down all active sessions
        void ShutDownAllSessions();

        ClientId         m_clientId;        // Client Id associated with the session manager.
        IMsgChannel*     m_pMessageChannel; // Message Channel object.
        Platform::Atomic m_lastSessionId;   // Counter used to generate unique session IDs.
        Platform::Mutex  m_sessionMutex;    // Mutex to synchronize session object access.
        SessionHashMap   m_sessions;        // Hash map containing currently active sessions.

        ServerHashMap    m_protocolServers; // Hash map containing protocol servers.

        AllocCb          m_allocCb;         // Allocator callbacks.
    };

} // DevDriver
