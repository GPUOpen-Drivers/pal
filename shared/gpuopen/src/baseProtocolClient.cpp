/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "baseProtocolClient.h"
#include "msgChannel.h"

namespace DevDriver
{
    BaseProtocolClient::BaseProtocolClient(IMsgChannel* pMsgChannel, Protocol protocol, Version minVersion, Version maxVersion)
        : m_pMsgChannel(pMsgChannel)
        , m_protocol(protocol)
        , m_minVersion(minVersion)
        , m_maxVersion(maxVersion)
        , m_pSession()
        , m_state(ClientState::Disconnected)
    {
        DD_ASSERT(m_pMsgChannel != nullptr);
    }

    bool BaseProtocolClient::IsConnected() const
    {
        return (m_state == ClientState::Connected);
    }

    bool BaseProtocolClient::QueryConnectionStatus()
    {
        bool isConnected = false;

        if (!m_pSession.IsNull())
        {
            // We should always be in the connected state if we have a valid session pointer
            DD_ASSERT(m_state == ClientState::Connected);

            isConnected = (m_pSession->IsClosed() == false);

            // If our underlying session object has closed while we were connected to it, then
            // invoke the normal disconnect logic.
            if (isConnected == false)
            {
                Disconnect();
            }
        }

        return isConnected;
    }

    ClientId BaseProtocolClient::GetRemoteClientId() const
    {
        if (!m_pSession.IsNull())
        {
            return m_pSession->GetDestinationClientId();
        }
        return 0;
    }

    BaseProtocolClient::~BaseProtocolClient()
    {
        Disconnect();
    }

    Version BaseProtocolClient::GetSessionVersion() const
    {
        Version version = 0;
        if (!m_pSession.IsNull())
        {
            version = m_pSession->GetVersion();
        }
        else
        {
            DD_WARN_REASON("Session version queried without a valid session. Did your session disconnect?");
        }
        return version;
    }

    void BaseProtocolClient::SessionEstablished(const SharedPointer<ISession>& pSession)
    {
        DD_UNUSED(pSession);

        // This should never be called
        DD_ASSERT_ALWAYS();
    }

    void BaseProtocolClient::UpdateSession(const SharedPointer<ISession>& pSession)
    {
        DD_UNUSED(pSession);

        // This should never be called
        DD_ASSERT_ALWAYS();
    }

    void BaseProtocolClient::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
    {
        DD_UNUSED(pSession);
        DD_UNUSED(terminationReason);

        // This should never be called
        DD_ASSERT_ALWAYS();
    }

    Result BaseProtocolClient::Connect(ClientId clientId, uint32 timeoutInMs)
    {
        Result result = Result::Error;

        // Disconnect first in case we're currently connected to something.
        Disconnect();

        if (m_pMsgChannel != nullptr)
        {
            EstablishSessionInfo sessionInfo = {};
            sessionInfo.protocol = m_protocol;
            sessionInfo.minProtocolVersion = m_minVersion;
            sessionInfo.maxProtocolVersion = m_maxVersion;
            sessionInfo.remoteClientId = clientId;

            SharedPointer<ISession> pSession;
            result = m_pMsgChannel->EstablishSessionForClient(&pSession, sessionInfo);
            if (result == Result::Success)
            {
                // Wait for the connection to complete
                result = pSession->WaitForConnection(timeoutInMs);
            }

            // If we successfully connect, store the pointer to the session so it doesn't get deleted.
            if (result == Result::Success)
            {
                m_pSession = pSession;

                m_state = ClientState::Connected;
            }
        }

        return result;
    }

    void BaseProtocolClient::Disconnect()
    {
        if (IsConnected())
        {
            // Drop the shared pointer to the current session. This will allow the session manager to clean up
            // the session object.
            m_pSession.Clear();

            m_state = ClientState::Disconnected;
        }

        ResetState();
    }

} // DevDriver
