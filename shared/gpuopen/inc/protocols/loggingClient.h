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

#pragma once

#include "baseProtocolClient.h"
#include "loggingProtocol.h"
#include "util/queue.h"
#include "util/vector.h"

namespace DevDriver
{
    class IMsgChannel;

    namespace LoggingProtocol
    {
        // Default value for the maximum number of messages to return from a single call to ReadLogMessages
        constexpr uint32 kDefaultMaxLogMessages = 4096;

        class LoggingClient : public BaseProtocolClient
        {
        public:
            explicit LoggingClient(IMsgChannel* pMsgChannel);
            ~LoggingClient();

            // Sends an enable logging message to the UMD
            Result EnableLogging(LogLevel priority, LoggingCategory categoryMask);

            // Sends a disable logging message to the UMD
            // Messages that are received during the time the disable message is sent and the time that the response
            // comes back from the server are stored in pLogMessages if it's provided. Otherwise they're discarded.
            Result DisableLogging(Vector<LogMessage>* pLogMessages = nullptr);

            // Queries available categories and populates the provided vector
            Result QueryCategories(Vector<NamedLoggingCategory>& categories);

#if DD_VERSION_SUPPORTS(GPUOPEN_SIMPLER_LOGGING_VERSION)
            // Attempts to read a log message from the bus using the specified timeout value
            // Returns NotReady if the timeout expires before a message becomes available
            // Returns Success if a log message was successfully written to pLogMessage and an error otherwise
            // Can only be called while logging is enabled
            Result ReadLogMessage(LogMessage* pLogMessage, uint32 timeoutInMs = kDefaultCommunicationTimeoutInMs);
#else
            // Reads the log messages stored on the remote server into the provided vector
            // The maximum number of messages returned is limited to maxMessages
            // Can only be called while logging is enabled
            Result ReadLogMessages(Vector<LogMessage>& logMessages, uint32 maxMessages = kDefaultMaxLogMessages);

            // Returns true if the client currently contains log messages that have not been received.
            // Can only be called while logging is enabled
            bool HasLogMessages();
#endif

            // Returns true if logging is currently enabled.
            bool IsLoggingEnabled() const;

        private:
            void ResetState() override;

            bool IsIdle() const;

            Result SendLoggingPayload(const SizedPayloadContainer& container,
                                      uint32                         timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                      uint32                         retryInMs   = kDefaultRetryTimeoutInMs);
            Result ReceiveLoggingPayload(SizedPayloadContainer* pContainer,
                                         uint32                   timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                         uint32                   retryInMs   = kDefaultRetryTimeoutInMs);
            Result TransactLoggingPayload(SizedPayloadContainer* pContainer,
                                         uint32                    timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                         uint32                    retryInMs   = kDefaultRetryTimeoutInMs);

#if !DD_VERSION_SUPPORTS(GPUOPEN_SIMPLER_LOGGING_VERSION)
#if DD_BUILD_32
            // Add padding to make sure the SizedPayloadContainer starts at an 8 byte boundary.
            size_t _padding;
#endif

            // A payload container used to store messages that are read during calls to HasLogMessage()
            SizedPayloadContainer m_pendingMsg;
#endif

            // True if logging is currently enabled.
            bool m_isLoggingEnabled;
        };
    }
}
