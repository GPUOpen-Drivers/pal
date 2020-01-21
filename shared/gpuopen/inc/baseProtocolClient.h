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
/**
***********************************************************************************************************************
* @file  baseProtocolClient.h
* @brief Class declaration for BaseProtocolClient.
***********************************************************************************************************************
*/

#pragma once

#include "protocolClient.h"

namespace DevDriver
{
    class IMsgChannel;

    enum class ClientState : uint32
    {
        Disconnected = 0,
        Connected
    };

    class BaseProtocolClient : public IProtocolClient
    {
    public:
        virtual ~BaseProtocolClient();

        // query constant properties of the protocol client
        Protocol GetProtocol() const override final { return m_protocol; };
        SessionType GetType() const override final { return SessionType::Client; };
        Version GetMinVersion() const override final { return m_minVersion; };
        Version GetMaxVersion() const override final { return m_maxVersion; };

        // connection management/tracking
        Result Connect(ClientId clientId, uint32 timeoutInMs = kDefaultConnectionTimeoutInMs) override final;
        void Disconnect() override final;
        bool IsConnected() const override final;
        bool QueryConnectionStatus() override final;

        // properties that are only valid in a connected session
        ClientId GetRemoteClientId() const override final;
        Version GetSessionVersion() const override final;

        // asychronous callbacks used by the SessionManager - should never be called directly
        virtual void SessionEstablished(const SharedPointer<ISession>& pSession) override;
        virtual void UpdateSession(const SharedPointer<ISession>& pSession) override;
        virtual void SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason) override;

    protected:
        BaseProtocolClient(IMsgChannel* pMsgChannel, Protocol protocol, Version minVersion, Version maxVersion);

        // Default implementation of ResetState that does nothing
        virtual void ResetState() {};

        IMsgChannel* const m_pMsgChannel;
        const Protocol m_protocol;
        const Version m_minVersion;
        const Version m_maxVersion;

        SharedPointer<ISession> m_pSession;

        DD_STATIC_CONST uint32 kDefaultRetryTimeoutInMs = 50;
        DD_STATIC_CONST uint32 kDefaultCommunicationTimeoutInMs = 5000;
        DD_STATIC_CONST uint32 kDefaultConnectionTimeoutInMs = 1000;

        // Attempts to receive a payload into a fixed size buffer.
        // Returns the result and the size of the payload if it was received successfully.
        Result ReceiveSizedPayload(void*   pPayloadBuffer,
                                   uint32  payloadBufferSize,
                                   uint32* pBytesReceived,
                                   uint32  timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                   uint32  retryInMs   = kDefaultRetryTimeoutInMs)
        {
            uint32 timeElapsed = 0;
            // Blocking wait on the message.
            Result result = Result::Error;

            SharedPointer<ISession> pSession = m_pSession;
            if (!pSession.IsNull())
            {
                do
                {
                    result = pSession->Receive(payloadBufferSize, pPayloadBuffer, pBytesReceived, retryInMs);
                    timeElapsed += retryInMs;
                }
                while ((result == Result::NotReady) & (timeElapsed <= timeoutInMs));
            }

            return result;
        }

        // Templated wrapper around ReceiveSizedPayload
        template <typename T>
        Result ReceivePayload(T*     pPayload,
                              uint32 timeoutInMs = kDefaultCommunicationTimeoutInMs,
                              uint32 retryInMs   = kDefaultRetryTimeoutInMs)
        {
            uint32 bytesReceived = 0;
            Result result = ReceiveSizedPayload(pPayload, sizeof(T), &bytesReceived, timeoutInMs, retryInMs);

            // Return an error if we don't get back the size we were expecting.
            if ((result == Result::Success) & (bytesReceived != sizeof(T)))
            {
                result = Result::Error;
            }

            return result;
        }

        // Attempts to send a payload
        Result SendSizedPayload(const void* pPayload,
                                uint32      payloadSize,
                                uint32      timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                uint32      retryInMs   = kDefaultRetryTimeoutInMs)
        {
            Result result = Result::Error;
            uint32 timeElapsed = 0;
            SharedPointer<ISession> pSession = m_pSession;
            if (!pSession.IsNull())
            {
                do
                {
                    result = pSession->Send(payloadSize, pPayload, retryInMs);
                    timeElapsed += retryInMs;
                } while ((result == Result::NotReady) & (timeElapsed <= timeoutInMs));
            }
            return result;
        }

        // Templated wrapper around SendSizedPayload
        template <typename T>
        Result SendPayload(const T* pPayload,
                           uint32   timeoutInMs = kDefaultCommunicationTimeoutInMs,
                           uint32   retryInMs   = kDefaultRetryTimeoutInMs)
        {
            return SendSizedPayload(pPayload, sizeof(T), timeoutInMs, retryInMs);
        }

        // Templated helper for common Send/Receive pattern
        template <typename T>
        Result Transact(T* pPayload,
                        uint32   timeoutInMs = kDefaultCommunicationTimeoutInMs,
                        uint32   retryInMs   = kDefaultRetryTimeoutInMs)
        {
            Result result = Result::Error;
            if (IsConnected())
            {
                result = SendPayload(pPayload, timeoutInMs, retryInMs);
                if (result == Result::Success)
                {
                    result = ReceivePayload(pPayload, timeoutInMs, retryInMs);
                }
            }
            return result;
        }

        Result SendPayloadContainer(
            const SizedPayloadContainer& container,
            uint32                       timeoutInMs = kDefaultCommunicationTimeoutInMs,
            uint32                       retryInMs   = kDefaultRetryTimeoutInMs)
        {
            return SendSizedPayload(container.payload,
                                    container.payloadSize,
                                    timeoutInMs,
                                    retryInMs);
        }

        Result ReceivePayloadContainer(
            SizedPayloadContainer* pContainer,
            uint32                 timeoutInMs = kDefaultCommunicationTimeoutInMs,
            uint32                 retryInMs   = kDefaultRetryTimeoutInMs)
        {

            return ReceiveSizedPayload(pContainer->payload,
                                       sizeof(pContainer->payload),
                                       &pContainer->payloadSize,
                                       timeoutInMs,
                                       retryInMs);
        }

        Result TransactPayloadContainer(
            SizedPayloadContainer* pContainer,
            uint32                 timeoutInMs = kDefaultCommunicationTimeoutInMs,
            uint32                 retryInMs   = kDefaultRetryTimeoutInMs)
        {
            Result result = SendPayloadContainer(*pContainer, timeoutInMs, retryInMs);
            if (result == Result::Success)
            {
                result = ReceivePayloadContainer(pContainer, timeoutInMs, retryInMs);
            }

            return result;
        }

    private:
        ClientState m_state;
    };

} // DevDriver
