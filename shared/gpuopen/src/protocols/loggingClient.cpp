/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "protocols/loggingClient.h"
#include "msgChannel.h"

#define LOGGING_CLIENT_MIN_VERSION 2
#define LOGGING_CLIENT_MAX_VERSION 3

namespace DevDriver
{
    namespace LoggingProtocol
    {
        LoggingClient::LoggingClient(IMsgChannel* pMsgChannel)
            : BaseProtocolClient(pMsgChannel, Protocol::Logging, LOGGING_CLIENT_MIN_VERSION, LOGGING_CLIENT_MAX_VERSION)
#if !DD_VERSION_SUPPORTS(GPUOPEN_SIMPLER_LOGGING_VERSION)
#if DD_BUILD_32
            , _padding(0)
#endif
#endif
            , m_isLoggingEnabled(false)
        {
#if !DD_VERSION_SUPPORTS(GPUOPEN_SIMPLER_LOGGING_VERSION)
#if DD_BUILD_32
            DD_UNUSED(_padding);
#endif
            m_pendingMsg.payloadSize = 0;
#endif
        }

        LoggingClient::~LoggingClient()
        {
        }

        Result LoggingClient::EnableLogging(LogLevel priority, LoggingCategory categoryMask)
        {
            Result result = Result::Error;

            if (IsConnected() && IsIdle())
            {
                LoggingFilter filter = {};
                filter.priority = priority;
                filter.category = categoryMask;

                SizedPayloadContainer container = {};
                container.CreatePayload<EnableLoggingRequestPayload>(filter);

                result = TransactLoggingPayload(&container);
                if (result == Result::Success)
                {
                    const EnableLoggingResponsePayload& response = container.GetPayload<EnableLoggingResponsePayload>();
                    if (response.header.command == LoggingProtocol::LoggingMessage::EnableLoggingResponse)
                    {
                        result = response.result;

                        if (result == Result::Success)
                        {
                            m_isLoggingEnabled = true;
                        }
                    }
                    else
                    {
                        // Invalid response payload
                        result = Result::Error;
                    }
                }
            }

            return result;
        }

        Result LoggingClient::DisableLogging(Vector<LogMessage>* pLogMessages)
        {
            Result result = Result::Error;
            if (IsConnected() && IsLoggingEnabled())
            {
                SizedPayloadContainer container = {};
                LoggingHeader* pHeader = reinterpret_cast<LoggingHeader*>(container.payload);
                pHeader->command = LoggingMessage::DisableLogging;
                container.payloadSize = sizeof(LoggingHeader);

                // Send the disable logging request.
                if (SendLoggingPayload(container) == Result::Success)
                {
                    // Process messages until we find the sentinel.
                    bool foundSentinel = false;
                    while (ReceiveLoggingPayload(&container) == Result::Success)
                    {
                        const LogMessagePayload* pPayload = reinterpret_cast<const LogMessagePayload*>(container.payload);
                        if (pPayload->header.command == LoggingMessage::LogMessageSentinel)
                        {
                            DD_PRINT(LogLevel::Debug, "Received Logging Sentinel From Session %d!", m_pSession->GetSessionId());

                            foundSentinel = true;
                            break;
                        }
                        else if (pPayload->header.command == LoggingMessage::LogMessage)
                        {
                            // If the caller provided a log message container, push the messages into it.
                            if (pLogMessages != nullptr)
                            {
                                pLogMessages->PushBack(Platform::Move(pPayload->message));
                            }
                        }
                        else
                        {
                            // We should never get another message type here.
                            DD_UNREACHABLE();
                        }
                    }

                    // Update the logging enabled variable.
                    m_isLoggingEnabled = false;

                    if (foundSentinel)
                    {
                        result = Result::Success;
                    }
                    else
                    {
                        // We should always find the sentinel unless we disconnect.
                        DD_ASSERT(IsConnected() == false);
                    }
                }
            }

            return result;
        }

        Result LoggingClient::QueryCategories(Vector<NamedLoggingCategory>& categories)
        {
            Result result = Result::Error;

            if (IsConnected() && IsIdle())
            {
                SizedPayloadContainer container = {};
                LoggingHeader* pHeader = reinterpret_cast<LoggingHeader*>(container.payload);
                pHeader->command = LoggingMessage::QueryCategoriesRequest;
                container.payloadSize = sizeof(LoggingHeader);

                result = TransactLoggingPayload(&container);
                if (result == Result::Success)
                {
                    if (pHeader->command == LoggingMessage::QueryCategoriesNumResponse)
                    {
                        const QueryCategoriesNumResponsePayload* pNumResponse =
                            reinterpret_cast<QueryCategoriesNumResponsePayload*>(container.payload);
                        const uint32 categoriesSent = pNumResponse->numCategories;

                        // TODO: actually validate this instead of just asserting
                        DD_ASSERT(categoriesSent < kMaxCategoryCount);

                        for (uint32 categoryIndex = 0; categoryIndex < categoriesSent; ++categoryIndex)
                        {
                            result = ReceiveLoggingPayload(&container);
                            if (result == Result::Success)
                            {
                                if (pHeader->command == LoggingMessage::QueryCategoriesDataResponse)
                                {
                                    const QueryCategoriesDataResponsePayload* pDataResponse =
                                        reinterpret_cast<QueryCategoriesDataResponsePayload*>(container.payload);
                                    categories.PushBack(pDataResponse->category);
                                }
                                else
                                {
                                    // Invalid response payload
                                    result = Result::Error;
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    else
                    {
                        // Invalid response payload
                        result = Result::Error;
                    }
                }
            }

            return result;
        }

#if DD_VERSION_SUPPORTS(GPUOPEN_SIMPLER_LOGGING_VERSION)
        Result LoggingClient::ReadLogMessage(LogMessage* pLogMessage, uint32 timeoutInMs)
        {
            Result result = Result::Error;

            if (IsConnected() && IsLoggingEnabled() && (pLogMessage != nullptr))
            {
                // Check for a new log message on the message bus.
                SizedPayloadContainer container = {};
                result = ReceiveLoggingPayload(&container, timeoutInMs);
                if (result == Result::Success)
                {
                    const LogMessagePayload* pPayload = reinterpret_cast<const LogMessagePayload*>(container.payload);
                    if (pPayload->header.command == LoggingMessage::LogMessage)
                    {
                        memcpy(pLogMessage, &pPayload->message, sizeof(pPayload->message));
                    }
                    else
                    {
                        result = Result::Error;
                        DD_ASSERT_REASON("Unexpected payload type");
                    }
                }
            }

            return result;
        }
#else
        Result LoggingClient::ReadLogMessages(Vector<LogMessage>& logMessages, uint32 maxMessages)
        {
            Result result = Result::Error;

            if (IsConnected() && IsLoggingEnabled())
            {
                result = Result::NotReady;

                uint32 messageCount = 0;

                // Enqueue the pending message if we have one.
                // A pending message can be created by a call to HasLogMessages(). HasLogMessages() has no way to
                // return messages to the caller, so we inject the message into the regular log stream here.
                if ((m_pendingMsg.payloadSize > 0) && (messageCount < maxMessages))
                {
                    const LogMessagePayload* pPayload =
                        reinterpret_cast<const LogMessagePayload*>(m_pendingMsg.payload);

                    // Should be impossible to see a sentinel here.
                    DD_ASSERT(pPayload->header.command != LoggingMessage::LogMessageSentinel);

                    logMessages.PushBack(Platform::Move(pPayload->message));
                    m_pendingMsg.payloadSize = 0;
                    ++messageCount;

                    result = Result::Success;
                }

                // Check for new log messages on the message bus.
                SizedPayloadContainer container = {};
                uint32 receiveDelayMs = kDefaultCommunicationTimeoutInMs;
                while ((messageCount < maxMessages) &&
                       (ReceiveLoggingPayload(&container, receiveDelayMs) == Result::Success))
                {
                    const LogMessagePayload* pPayload = reinterpret_cast<const LogMessagePayload*>(container.payload);
                    DD_ASSERT(pPayload->header.command == LoggingMessage::LogMessage);

                    DD_PRINT(LogLevel::Debug, "Received Logging Payload From Session %d!", m_pSession->GetSessionId());
                    logMessages.PushBack(Platform::Move(pPayload->message));
                    ++messageCount;

                    result = Result::Success;

                    // We only want the first read to wait for new messages.
                    // After that we only want to get the remaining messages in the receive window.
                    receiveDelayMs = kNoWait;
                }
            }

            return result;
        }

        bool LoggingClient::HasLogMessages()
        {
            bool result = false;

            if (IsConnected() && IsLoggingEnabled())
            {
                // If we don't have any pending messages, check if there's a new one.
                if (m_pendingMsg.payloadSize == 0)
                {
                    result = (ReceiveLoggingPayload(&m_pendingMsg, kNoWait) == Result::Success);
                }
                else
                {
                    // We already have a pending message.
                    result = true;
                }
            }

            return result;
        }
#endif

        void LoggingClient::ResetState()
        {
            m_isLoggingEnabled = false;

#if !DD_VERSION_SUPPORTS(GPUOPEN_SIMPLER_LOGGING_VERSION)
            m_pendingMsg.payloadSize = 0;
#endif
        }

        bool LoggingClient::IsIdle() const
        {
            return (m_isLoggingEnabled == false);
        }

        bool LoggingClient::IsLoggingEnabled() const
        {
            return m_isLoggingEnabled;
        }

        Result LoggingClient::SendLoggingPayload(
            const SizedPayloadContainer& container,
            uint32 timeoutInMs,
            uint32 retryInMs)
        {
            // Use the legacy size for the payload if we're connected to an older client, otherwise use the real size.
            const Version sessionVersion = (m_pSession.IsNull() == false) ? m_pSession->GetVersion() : 0;
            const uint32 payloadSize = (sessionVersion >= LOGGING_LARGE_MESSAGES_VERSION) ? container.payloadSize : kLegacyLoggingPayloadSize;

            return SendSizedPayload(container.payload, payloadSize, timeoutInMs, retryInMs);
        }

        Result LoggingClient::ReceiveLoggingPayload(
            SizedPayloadContainer* pContainer,
            uint32 timeoutInMs,
            uint32 retryInMs)
        {
            return ReceiveSizedPayload(pContainer->payload, sizeof(pContainer->payload), &pContainer->payloadSize, timeoutInMs, retryInMs);
        }

        Result LoggingClient::TransactLoggingPayload(
            SizedPayloadContainer* pContainer,
            uint32 timeoutInMs,
            uint32 retryInMs)
        {
            Result result = SendLoggingPayload(*pContainer, timeoutInMs, retryInMs);
            if (result == Result::Success)
            {
                result = ReceiveLoggingPayload(pContainer, timeoutInMs, retryInMs);
            }

            return result;
        }
    }
}
