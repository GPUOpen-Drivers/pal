/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "legacyProtocolClient.h"
#include "ddUriInterface.h"
#include <util/vector.h>
#include <stdarg.h>

namespace DevDriver
{
    class IMsgChannel;

    namespace TransferProtocol
    {
        class PullBlock;
    }

    namespace URIProtocol
    {
        // We alias these types for backwards compatibility
        using ResponseHeader = DevDriver::URIResponseHeader;

        class URIClient final : public LegacyProtocolClient
        {
        public:
            explicit URIClient(IMsgChannel* pMsgChannel);
            ~URIClient();

            // Sends a URI request to the connected server.
            Result RequestURI(const char*     pRequestString,
                              ResponseHeader* pResponseHeader = nullptr,
                              const void*     pPostData = nullptr,
                              size_t          postDataSize = 0);

            // Reads response data that was returned by a prior request.
            Result ReadResponse(uint8* pDstBuffer, size_t bufferSize, size_t* pBytesRead);

            // Aborts the currently active request.
            Result AbortRequest();

            // Reads a complete URI response into the destination buffer
            // The caller is responsible for ensuring that the buffer is large enough to fit
            // the entire response.
            Result ReadFullResponse(void* pDstBuffer, const size_t bufferSize);

            // Helper function used execute a URI request and read the response back if necessary
            Result TransactURIRequest(
                const void*     pPostDataBuffer,
                uint32          postDataSize,
                Vector<uint8>*  pResponseBuffer,
                const char*     pFormatString,
                ...);

            // Helper function used execute a URI request and read the response back if necessary
            // This variant takes a va_list directly, and can be chained with other methods using var args.
            Result TransactURIRequestV(
                const void*     pPostDataBuffer,
                uint32          postDataSize,
                Vector<uint8>*  pResponseBuffer,
                const char*     pFormatString,
                va_list         formatArgs);

        private:
            void ResetState() override;

            // Helper method to send a payload, handling backwards compatibility and retrying.
            Result SendURIPayload(const SizedPayloadContainer& container,
                                  uint32                       timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                  uint32                       retryInMs = kDefaultRetryTimeoutInMs);
            // Helper method to handle receiving a payload from a SizedPayloadContainer, including retrying if busy.
            Result ReceiveURIPayload(SizedPayloadContainer* pContainer,
                                     uint32                 timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                     uint32                 retryInMs = kDefaultRetryTimeoutInMs);
            // Helper method to send and then receive using a SizedPayloadContainer object.
            Result TransactURIPayload(SizedPayloadContainer* pContainer,
                                      uint32                 timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                      uint32                 retryInMs = kDefaultRetryTimeoutInMs);

            enum class State : uint32
            {
                Idle = 0,
                ReadResponse
            };

            // Context structure for tracking all state specific to a request.
            struct Context
            {
                State                        state;
                TransferProtocol::PullBlock* pBlock;
            };

            Context           m_context;
            Vector<char, 256> m_requestStringBuffer;
        };
    }
} // DevDriver
