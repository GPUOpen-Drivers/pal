/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "sessionManager.h"
#include "session.h"
#include "msgChannel.h"
#include "protocolServer.h"
#include "protocolClient.h"

using namespace DevDriver::SessionProtocol;
using namespace DevDriver::Platform;

namespace DevDriver
{
    // We break the SessionId value into two, 16 bit values. These variables make it easier to operate on the bitfield
    DD_STATIC_CONST uint32 kClientSessionIdSize = 16;
    DD_STATIC_CONST uint32 kClientSessionIdMask = (1 << kClientSessionIdSize) - 1;

    // SendReset
    //
    // Sends a reset packet to the specified destination
    inline Result SessionManager::SendReset(
        ClientId remoteClientId,
        uint32 remoteSessionId,
        Result reason,
        Version version)
    {
        return SendCommand(remoteClientId,
                            static_cast<MessageCode>(SessionMessage::Rst),
                            remoteSessionId,
                            static_cast<Sequence>(reason),
                            version);
    }

    // GetProtocolServer
    //
    // Retrieves the specified protocol server
    IProtocolServer* SessionManager::GetProtocolServer(Protocol protocol)
    {
        // Look up the protocol in the hash map
        return m_protocolServers.FindPointer(protocol);
    }

    // HasProtocolServer
    //
    // Checks for the presence of the specified protocol server
    bool SessionManager::HasProtocolServer(Protocol protocol)
    {
        // Look up the protocol in the hash map
        return m_protocolServers.Contains(protocol);
    }

    SessionManager::SessionManager(const AllocCb& allocCb)
        : m_clientId(kBroadcastClientId)
        , m_pMessageChannel(nullptr)
        , m_lastSessionId(kInvalidSessionId)
        , m_sessionMutex()
        , m_sessions(allocCb)
        , m_protocolServers(allocCb)
        , m_allocCb(allocCb)
    {
    }

    SessionManager::~SessionManager()
    {
        Destroy();
    }

    // Init
    //
    // Initializes the SessionManager object and binds it to the message channel specified by pMessageChannel
    Result SessionManager::Init(IMsgChannel* pMessageChannel)
    {
        Result result = Result::Error;

        if (pMessageChannel != nullptr)
        {
            m_pMessageChannel = pMessageChannel;
            m_clientId = m_pMessageChannel->GetClientId();

            // Generate a random initial SessionId to help minimize probability of collision
            Platform::Random rng;
            m_lastSessionId = rng.Generate();

            result = Result::Success;
        }

        return result;
    }

    // Destroy
    //
    // Destroys the session manager object
    void SessionManager::Destroy()
    {
        if (m_pMessageChannel != nullptr)
        {
            ShutDownAllSessions();

            // Clear the list of registered protocol servers after all sessions have been disconnected
            m_protocolServers.Clear();

            m_pMessageChannel = nullptr;
        }
    }

    Result SessionManager::EstablishSessionForClient(SharedPointer<ISession>*    ppSession,
                                                     const EstablishSessionInfo& sessionInfo)
    {
        Result result = Result::Error;

        if (ppSession != nullptr)
        {
            // The shared pointer will automatically clean up the session object if anything below fails
            SharedPointer<Session> pNewSession =
                SharedPointer<Session>::Create(m_allocCb,
                                               m_pMessageChannel,
                                               SessionType::Client,
                                               sessionInfo.protocol);
            if (!pNewSession.IsNull())
            {
                // Create a new sessionRef.
                // Get a new sessionRef id for the sessionRef.
                Platform::LockGuard<Platform::Mutex> sessionLock(m_sessionMutex);

                const SessionId sessionId = GetNewSessionId(kInvalidSessionId);

                result = pNewSession->Connect(sessionInfo.remoteClientId,
                                              sessionId,
                                              sessionInfo.minProtocolVersion,
                                              sessionInfo.maxProtocolVersion);
                if (result == Result::Success)
                {
                    result = m_sessions.Create(sessionId, pNewSession);
                    if (result != Result::Success)
                    {
                        pNewSession->Shutdown(Result::InsufficientMemory);
                    }
                }
                else
                {
                    DD_PRINT(LogLevel::Error, "[DevDriver][SessionManager] Failed to connect session (id: %u).", sessionId);
                }
            }

            // If everything goes well, return the session shared pointer.
            if (result == Result::Success)
            {
                *ppSession = pNewSession;
            }
        }

        return result;
    }

    Result SessionManager::RegisterProtocolServer(IProtocolServer* pServer)
    {
        // Make sure we're passed a valid server
        DD_ASSERT(pServer != nullptr);

        // Make sure we aren't in the middle of session processing
        Platform::LockGuard<Platform::Mutex> lock(m_sessionMutex);

        return m_protocolServers.Create(pServer->GetProtocol(), pServer);
    }

    Result SessionManager::UnregisterProtocolServer(IProtocolServer* pServer)
    {
        // Make sure we're passed a valid server
        DD_ASSERT(pServer != nullptr);

        Result result = Result::Error;

        // Make sure we aren't in the middle of session processing
        Platform::LockGuard<Platform::Mutex> lock(m_sessionMutex);

        const Protocol protocol = pServer->GetProtocol();
        const bool hasServer = (m_protocolServers.FindPointer(protocol) != nullptr);

        // Make sure we previously had a protocol server registered.
        if (hasServer)
        {
            // Notify all server sessions that rely on this protocol server.
            for (auto& pair : m_sessions)
            {
                auto& pSession = pair.value;

                if (pSession->IsServerSession() && (pSession->GetProtocol() == protocol))
                {
                    pSession->HandleUnregisterProtocolServer(pSession, pServer);
                }
            }

            result = m_protocolServers.Erase(protocol);
        }
        else
        {
            DD_WARN_REASON("Attempted to unregister an unknown protocol server");
        }

        return result;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Session protocol state management functions
    //

    // Lookup sessionRef for sessionRef ID, only returning a sessionRef if the sessionRef has not already been closed.
    SharedPointer<Session> SessionManager::FindOpenSession(SessionId sessionId)
    {
        const auto sessionIter = m_sessions.Find(sessionId);
        if (sessionIter != m_sessions.End())
        {
            auto& pSession = sessionIter->value;
            DD_ASSERT(pSession.IsNull() == false);
            DD_ASSERT(pSession->GetSessionId() == sessionId);
            if (pSession->GetSessionState() != SessionState::Closed)
            {
                return pSession;
            }
        }
        return SharedPointer<Session>();
    }

    void SessionManager::ShutDownAllSessions()
    {
        // Check if there's any sessions to shut down
        if (m_sessions.IsEmpty() == false)
        {
            // If the message channel is still connected, attempt to gracefully shut down active sessions.
            if (m_pMessageChannel->IsConnected())
            {
                DD_PRINT(LogLevel::Info, "[SessionManager] Gracefully shutting down active sessions...");

                m_sessionMutex.Lock();

                // Gracefully close all active sessions.
                for (auto& pair : m_sessions)
                {
                    auto& pSession = pair.value;

                    pSession->Shutdown(Result::Success);
                }

                m_sessionMutex.Unlock();

                constexpr uint32 kShutdownTimeoutInMs = 5000;

                const uint64 absTimeoutTime = (Platform::GetCurrentTimeInMs() + kShutdownTimeoutInMs);

                // Wait for all sessions to close.
                while (m_sessions.IsEmpty() == false)
                {
                    m_pMessageChannel->Update();

                    if (m_pMessageChannel->IsConnected() == false)
                    {
                        // We lost our transport connection while updating the message channel.
                        // Exit the loop early since there's no chance we'll receive more bus messages.

                        DD_PRINT(LogLevel::Warn, "[SessionManager] Transport disconnected while shutting down sessions");

                        break;
                    }
                    else if (Platform::GetCurrentTimeInMs() >= absTimeoutTime)
                    {
                        // This generally shouldn't happen so we alert here
                        DD_ALERT_REASON("[SessionManager] Shutdown timeout exceeded!");

                        break;
                    }
                }

                DD_PRINT(LogLevel::Info, "[SessionManager] Graceful shutdown complete");
            }

            // Check if there's still active sessions after the graceful shutdown attempt
            // This also handles the condition where we have a disconnected transport
            if (m_sessions.IsEmpty() == false)
            {
                DD_PRINT(LogLevel::Info, "[SessionManager] Forcefully shutting down active sessions...");

                m_sessionMutex.Lock();

                // Forcefully close all active sessions.
                for (auto& pair : m_sessions)
                {
                    auto& pSession = pair.value;

                    pSession->Shutdown(Result::EndOfStream);
                }

                m_sessionMutex.Unlock();

                // We should only need to update the sessions once after a forced shutdown.
                // All sessions should exit after a single call because they should have been forcefully moved to the
                // closed state earlier.
                UpdateSessions();

                DD_PRINT(LogLevel::Info, "[SessionManager] Forceful shutdown complete");
            }

            // We should definitely have no active sessions by this point or the forceful shutdown failed somehow.
            DD_ASSERT(m_sessions.IsEmpty());
        }
    }

    void SessionManager::HandleClientDisconnection(ClientId dstClientId)
    {
        Platform::LockGuard<Platform::Mutex> sessionLock(m_sessionMutex);
        for (auto& pair : m_sessions)
        {
            auto& pSession = pair.value;
            DD_ASSERT(pSession.IsNull() == false);

            if (pSession->GetDestinationClientId() == dstClientId)
            {
                pSession->Shutdown(Result::NotReady);
            }
        }
    }

    void SessionManager::HandleTransportDisconnect()
    {
        ShutDownAllSessions();
    }

    void SessionManager::HandleReceivedSessionMessage(const MessageBuffer& messageBuffer)
    {
        // Make sure we're the only code manipulating the sessions/protocol servers
        Platform::LockGuard<Platform::Mutex> lock(m_sessionMutex);

        DD_ASSERT(messageBuffer.header.protocolId == Protocol::Session);
        DD_ASSERT(messageBuffer.header.dstClientId == m_clientId);

        const SessionId& remoteSessionId = messageBuffer.header.sessionId;
        const ClientId& sourceClientId = messageBuffer.header.srcClientId;

        SharedPointer<Session> pSession = SharedPointer<Session>();
        Result reason = Result::Unavailable;
        Version version = 0;

        switch (static_cast<SessionMessage>(messageBuffer.header.messageId))
        {
            case SessionMessage::Syn:
            {
                const SynPayload* DD_RESTRICT pRequestPayload =
                    reinterpret_cast<const SynPayload*>(&messageBuffer.payload[0]);

                // Find the associated protocol server
                IProtocolServer* pServer = m_protocolServers.FindPointer(pRequestPayload->protocol);

                // Handle the Syn packet if we have a protocol server registered for the requested protocol
                if (pServer != nullptr)
                {
                    reason = Result::VersionMismatch;

                    // The first step in accepting a connection is checking to see if the version requested is
                    // supported by the protocol server.

                    const Version& minVersion = pRequestPayload->minVersion;

                    // If the Session protocol version is high enough to support ranged tests we use the max value
                    // as the maximum bound for our protocol version range.
                    Version maxVersion;
                    if (pRequestPayload->sessionVersion >= kSessionProtocolRangeVersion)
                    {
                        maxVersion = Platform::Max(pRequestPayload->maxVersion, minVersion);
                    }
                    // Otherwise we just use the minimum version as our range
                    else
                    {
                        maxVersion = minVersion;
                    }

                    // We pass these version into the protocol server and store the resulting version. This version
                    // is automatically added into the Rst packet if there is a version mismatch.
                    if (pServer->GetSupportedVersion(minVersion, maxVersion, &version))
                    {
                        reason = Result::Rejected;

                        // Create a new session object
                        pSession = SharedPointer<Session>::Create(m_allocCb,
                                                                  m_pMessageChannel,
                                                                  SessionType::Server,
                                                                  pServer->GetProtocol());
                        if (!pSession.IsNull())
                        {
                            // Assuming we made it this far, generate a new session ID and bind the session to the
                            // protocol server
                            SessionId sessionId = GetNewSessionId(remoteSessionId);
                            Result result = pSession->BindToServer(*pServer,
                                                                  sourceClientId,
                                                                  pRequestPayload->sessionVersion,
                                                                  version,
                                                                  sessionId);
                            if (result == Result::Success)
                            {
                                result = m_sessions.Create(sessionId, pSession);
                            }

                            // If insertion failed or the server rejects the session we close it and clear the
                            // sessionRef pointer.
                            if ((result != Result::Success) || !pServer->AcceptSession(pSession))
                            {
                                pSession->Shutdown(Result::Rejected);
                                pSession.Clear();
                            }
                        }
                    }
                }
                break;
            }
            case SessionMessage::SynAck:
            {
                // Handle edge case where the Ack for the SynAck was lost. In this situation, we've already moved
                // into the established state but they have not. We do this first because we assume the Ack has
                // dropped, and it's likely that the sessionRef has already retransmitted the SynAck multiple times.
                auto sessionIter = m_sessions.Find(remoteSessionId);

                // If the lookup succeeded, set the sessionRef pointer to the correct sessionRef
                if (sessionIter != m_sessions.End())
                {
                    pSession = sessionIter->value;
                }
                // Otherwise we treat it as the initial transition, and look up the initial sessionRef ID that is
                // in the payload.
                else
                {
                    const SynAckPayload* DD_RESTRICT pPayload =
                        reinterpret_cast<const SynAckPayload*>(&messageBuffer.payload[0]);
                    sessionIter = m_sessions.Find(pPayload->initialSessionId);
                    // If we found it, we need to initialize the sessionRef pointer, then remove the sessionRef
                    // from the hashmap and reinsert it under the final sessionRef id. If this insertion fails
                    // (most likely due to a collision) then we close the sessionRef and clear our pointer.
                    if (sessionIter != m_sessions.End())
                    {
                        pSession = sessionIter->value;
                        m_sessions.Remove(sessionIter);
                        if (m_sessions.Create(remoteSessionId, pSession) != Result::Success)
                        {
                            pSession->Shutdown(Result::Error);
                            pSession.Clear();
                            reason = Result::Error;
                        }
                    }
                }

                break;
            }
            case SessionMessage::Fin:
            case SessionMessage::Data:
            case SessionMessage::Ack:
            case SessionMessage::Rst:
                pSession = FindOpenSession(remoteSessionId);
                break;
            default:
                break;
        }

        // If the sessionRef pointer is non-null, we pass the message on to it. Otherwise we send a reset packet
        // to inform the other side that the connection is invalid.
        if (!pSession.IsNull())
        {
            DD_ASSERT(pSession->GetDestinationClientId() == sourceClientId);
            pSession->HandleMessage(pSession, messageBuffer);
        }
        else
        {
            SendReset(sourceClientId, remoteSessionId, reason, version);
        }
    }

    void SessionManager::UpdateSessions()
    {
        Platform::LockGuard<Platform::Mutex> sessionLock(m_sessionMutex);

        auto it = m_sessions.Begin();
        while (it != m_sessions.End())
        {
            const SharedPointer<Session>& pSession = it->value;
            Session& sessionRef = *pSession.Get();

            sessionRef.Update(pSession);

            // Remove closing sessions.
            if (sessionRef.GetSessionState() == SessionState::Closed)
            {
                it = m_sessions.Remove(it);
                continue;
            }
            ++it;
        }
    }

    SessionId SessionManager::GetNewSessionId(SessionId remoteSessionId)
    {
        const SessionId remoteInput = (remoteSessionId << kClientSessionIdSize);
        SessionId sessionId;
        do
        {
            const uint32 nextId = AtomicIncrement(&m_lastSessionId);
            sessionId = (nextId & kClientSessionIdMask) | remoteInput;
        } while ((sessionId == kInvalidSessionId) || m_sessions.Contains(sessionId));
        return sessionId;
    }
} // DevDriver
