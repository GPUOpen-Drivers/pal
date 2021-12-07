/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "protocols/ddURIClient.h"
#include "protocols/ddURIProtocol.h"
#include "protocols/ddTransferProtocol.h"
#include "msgChannel.h"
#include "ddTransferManager.h"

#define URI_CLIENT_MIN_VERSION URI_INITIAL_VERSION
#define URI_CLIENT_MAX_VERSION URI_POST_PROTOCOL_VERSION

namespace DevDriver
{
namespace URIProtocol
{

static constexpr URIDataFormat ResponseFormatToUriFormat(ResponseDataFormat format)
{
    static_assert(static_cast<uint32>(ResponseDataFormat::Unknown) == static_cast<uint32>(URIDataFormat::Unknown),
                    "ResponseDataFormat and URIDataFormat no longer match");
    static_assert(static_cast<uint32>(ResponseDataFormat::Text) == static_cast<uint32>(URIDataFormat::Text),
                    "ResponseDataFormat and URIDataFormat no longer match");
    static_assert(static_cast<uint32>(ResponseDataFormat::Binary) == static_cast<uint32>(URIDataFormat::Binary),
                    "ResponseDataFormat and URIDataFormat no longer match");
    static_assert(static_cast<uint32>(ResponseDataFormat::Count) == static_cast<uint32>(URIDataFormat::Count),
                    "ResponseDataFormat and URIDataFormat no longer match");
    return static_cast<URIDataFormat>(format);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Result URIClient::TransactURIRequest(
    const void*     pPostDataBuffer, /// [in] Post data to send to the remote client
                                     ///      Can be set to nullptr if there's no post data to send
    uint32          postDataSize,    /// [in] Size of the data pointed to by pPostDataBuffer
    Vector<uint8>*  pResponseBuffer, /// [in/out] Buffer to write the response data into
                                     ///          Can be set to nullptr if there's no output data expected
                                     ///          for the provided request
    const char*     pFormatString,   /// A format string used to generate the request string
    ...)                             /// Variable length argument list associated with pFormatString
{
    va_list formatArgs;
    va_start(formatArgs, pFormatString);

    const Result result = TransactURIRequestV(
        pPostDataBuffer,
        postDataSize,
        pResponseBuffer,
        pFormatString,
        formatArgs
    );

    va_end(formatArgs);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Result URIClient::TransactURIRequestV(
    const void*     pPostDataBuffer, /// [in] Post data to send to the remote client
                                     ///      Can be set to nullptr if there's no post data to send
    uint32          postDataSize,    /// [in] Size of the data pointed to by pPostDataBuffer
    Vector<uint8>*  pResponseBuffer, /// [in/out] Buffer to write the response data into
                                     ///          Can be set to nullptr if there's no output data expected
                                     ///          for the provided request
    const char*     pFormatString,   /// A format string used to generate the request string
    va_list         formatArgs)      /// Variable length argument list associated with pFormatString
{
    size_t formatSize = 0;
    {
        va_list localArgs;
        va_copy(localArgs, formatArgs);

        const int32 ret = Platform::Vsnprintf(
            m_requestStringBuffer.Data(),
            m_requestStringBuffer.Size(),
            pFormatString,
            localArgs);

        va_end(localArgs);
        formatSize = static_cast<size_t>(ret);
    }

    Result result = Result::Success;

    // Check that we had enough space for the formatting.
    // We may need to adjust our buffer, and re-run the format for it to fit.
    if (formatSize > m_requestStringBuffer.Size())
    {
        m_requestStringBuffer.Resize(formatSize);

        const int32 ret = Platform::Vsnprintf(m_requestStringBuffer.Data(),
            m_requestStringBuffer.Size(),
            pFormatString,
            formatArgs);

        // If we still failed to print the string properly after resizing, return an error.
        if (static_cast<size_t>(ret) != m_requestStringBuffer.Size())
        {
            result = Result::Error;
        }
    }

    if (result == Result::Success)
    {
        URIProtocol::ResponseHeader responseHeader = {};
        result = RequestURI(m_requestStringBuffer.Data(), &responseHeader, pPostDataBuffer, postDataSize);

        // Receive a response if necessary
        if ((result == Result::Success) && (pResponseBuffer != nullptr))
        {
            // Ensure we have enough buffer space to read the whole response.
            pResponseBuffer->Resize(responseHeader.responseDataSizeInBytes);

            // Not all requests return data, so only read the full response if there is data to read.
            if (responseHeader.responseDataSizeInBytes != 0)
            {
                result = ReadFullResponse(pResponseBuffer->Data(), pResponseBuffer->Size());
            }
        }
    }

    memset(m_requestStringBuffer.Data(), 0, m_requestStringBuffer.Size());

    return result;
}

// =====================================================================================================================
URIClient::URIClient(IMsgChannel* pMsgChannel)
    : LegacyProtocolClient(pMsgChannel, Protocol::URI, URI_CLIENT_MIN_VERSION, URI_CLIENT_MAX_VERSION)
    , m_requestStringBuffer(pMsgChannel->GetAllocCb())
{
    // Make sure our buffer always has some space
    m_requestStringBuffer.Resize(m_requestStringBuffer.Capacity());
    memset(&m_context, 0, sizeof(m_context));
}

// =====================================================================================================================
URIClient::~URIClient()
{
}

// =====================================================================================================================
Result URIClient::RequestURI(
    const char*      pRequestString,
    ResponseHeader*  pResponseHeader,
    const void*      pPostData,
    size_t           postDataSize)
{
    Result result = Result::UriInvalidParameters;

    if ((m_context.state == State::Idle) &&
        (pRequestString != nullptr))
    {
        // Setup some sensible defaults in the response header
        if (pResponseHeader != nullptr)
        {
            pResponseHeader->responseDataSizeInBytes = 0;
            pResponseHeader->responseDataFormat = URIDataFormat::Unknown;
        }

        // Set up the request payload.
        SizedPayloadContainer container = {};
        // If there's no post data just create the container with the request string directly
        if ((pPostData == nullptr) || (postDataSize == 0))
        {
            container.CreatePayload<URIRequestPayload>(pRequestString);
            result = Result::Success;
        }
        else if (pPostData != nullptr)
        {
            // Try to fit the post data into a single message packet
            if (postDataSize <= kMaxInlineDataSize)
            {
                // If the data fits into a single packet, setup the URI payload struct first
                container.CreatePayload<URIRequestPayload>(pRequestString,
                                                            TransferProtocol::kInvalidBlockId,
                                                            TransferDataFormat::Binary,
                                                            static_cast<uint32>(postDataSize));
                // Then copy the data into the payload right after the struct
                void* pPostDataLocation = GetInlineDataPtr(&container);
                memcpy(pPostDataLocation, pPostData, postDataSize);
                // And then update the payload size so the post data doesn't get trimmed off
                container.payloadSize = static_cast<uint32>(sizeof(URIRequestPayload) + postDataSize);
                result = Result::Success;
            }
            else
            {
                // If the data won't fit in a single packet we need to request a block from the server.
                // First send the post request, the response will tell us the block ID we should open to
                // push our data into.
                SizedPayloadContainer blockRequest = {};
                blockRequest.CreatePayload<URIPostRequestPayload>(pRequestString, static_cast<uint32>(postDataSize));
                result = TransactURIPayload(&blockRequest);
                if (result == Result::Success)
                {
                    // Read the response and get the block ID to use for our post data
                    const URIPostResponsePayload& response = blockRequest.GetPayload<URIPostResponsePayload>();
                    const uint32 pushBlockId = response.blockId;
                    result = response.result;
                    if (result == Result::Success)
                    {
                        // Open the indicated block and send our data
                        TransferProtocol::PushBlock* pPostBlock = m_pMsgChannel->GetTransferManager().OpenPushBlock(
                            GetRemoteClientId(),
                            pushBlockId,
                            postDataSize);

                        if (pPostBlock != nullptr)
                        {
                            result = pPostBlock->Write(static_cast<const uint8*>(pPostData), postDataSize);
                            if (result == Result::Success)
                            {
                                result = pPostBlock->Finalize();
                            }

                            m_pMsgChannel->GetTransferManager().ClosePushBlock(&pPostBlock);
                        }
                        else
                        {
                            result = Result::UriFailedToAcquirePostBlock;
                        }
                    }

                    // Finally setup the container to send the URI request, this time with the block ID that contains
                    // our post data
                    if (result == Result::Success)
                    {
                        container.CreatePayload<URIRequestPayload>(pRequestString,
                                                                    pushBlockId,
                                                                    TransferDataFormat::Binary,
                                                                    static_cast<uint32>(postDataSize));
                    }
                }
            }
        }

        // Issue a transaction.
        if (result == Result::Success)
        {
            result = TransactURIPayload(&container);
        }

        if (result == Result::Success)
        {
            const URIResponsePayload& responsePayload = container.GetPayload<URIResponsePayload>();

            if (responsePayload.header.command == URIMessage::URIResponse)
            {
                result = responsePayload.result;
                if (result == Result::Success)
                {
                    // We've successfully received the response. Extract the relevant fields from the response.
                    if (responsePayload.blockId != TransferProtocol::kInvalidBlockId)
                    {
                        const TransferProtocol::BlockId& remoteBlockId = responsePayload.blockId;

                        // Attempt to open the pull block containing the response data.
                        // @Todo: Detect if the service returns the invalid block ID and treat that as a success.
                        //        It will require a new protocol version because existing clients will fail if
                        //        the invalid block ID is returned in lieu of a block of size 0.
                        TransferProtocol::PullBlock* pPullBlock =
                            m_pMsgChannel->GetTransferManager().OpenPullBlock(GetRemoteClientId(), remoteBlockId);

                        if (pPullBlock != nullptr)
                        {
                            m_context.pBlock = pPullBlock;
                            const size_t blockSize = m_context.pBlock->GetBlockDataSize();

                            // We successfully opened the block. Return the block data size and format via the header.
                            // The header is optional so check for nullptr first.
                            if (pResponseHeader != nullptr)
                            {
                                // Set up some defaults for the response fields.
                                URIDataFormat responseDataFormat = URIDataFormat::Text;
                                if (GetSessionVersion() >= URI_RESPONSE_FORMATS_VERSION)
                                {
                                    responseDataFormat = ResponseFormatToUriFormat(responsePayload.format);
                                }

                                pResponseHeader->responseDataSizeInBytes = blockSize;
                                pResponseHeader->responseDataFormat = responseDataFormat;
                            }

                            // If the block size is non-zero we move to the read state
                            if (blockSize > 0)
                            {
                                // Set up internal state.
                                m_context.state = State::ReadResponse;
                            }
                            else // If the block size is zero we automatically close it and move back to idle
                            {
                                m_pMsgChannel->GetTransferManager().ClosePullBlock(&m_context.pBlock);
                            }
                        }
                        else
                        {
                            // Failed to open the response block.
                            result = Result::UriFailedToOpenResponseBlock;
                        }
                    }
                }
            }
        }
    }
    return result;
}

// =====================================================================================================================
Result URIClient::ReadResponse(uint8* pDstBuffer, size_t bufferSize, size_t* pBytesRead)
{
    Result result = Result::UriInvalidParameters;

    if (m_context.state == State::ReadResponse)
    {
        result = m_context.pBlock->Read(pDstBuffer, bufferSize, pBytesRead);

        // If we reach the end of the stream or we encounter an error, we should transition back to the idle state.
        if ((result == Result::EndOfStream) ||
            (result == Result::Error))
        {
            m_context.state = State::Idle;
            m_pMsgChannel->GetTransferManager().ClosePullBlock(&m_context.pBlock);
        }
    }

    return result;
}

// =====================================================================================================================
Result URIClient::AbortRequest()
{
    Result result = Result::UriInvalidParameters;

    if (m_context.state == State::ReadResponse)
    {
        m_context.state = State::Idle;
        m_pMsgChannel->GetTransferManager().ClosePullBlock(&m_context.pBlock);

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
Result URIClient::ReadFullResponse(void* pDstBuffer, const size_t bufferSize)
{
    Result result = Result::Error;

    // Ensure that the buffer to copy the response to is valid.
    if (pDstBuffer != nullptr)
    {
        // Read all of the response bytes.
        // We should expect to see an "EndOfStream" result if all response data was read successfully.
        size_t totalBytesRead = 0;
        do
        {
            size_t bytesRead = 0;
            const size_t bytesRemaining = (bufferSize - totalBytesRead);
            const size_t bytesToRead = bytesRemaining;
            result = ReadResponse(reinterpret_cast<DevDriver::uint8*>(pDstBuffer) + totalBytesRead, bytesToRead, &bytesRead);
            totalBytesRead += bytesRead;
        } while (result == DevDriver::Result::Success);

        if (result == Result::EndOfStream)
        {
            result = Result::Success;
        }
    }
    else
    {
        // The response buffer was invalid.
        result = DevDriver::Result::InvalidParameter;
    }

    return result;
}

// =====================================================================================================================
void URIClient::ResetState()
{
    // Close the pull block if it's still valid.
    if (m_context.pBlock != nullptr)
    {
        m_pMsgChannel->GetTransferManager().ClosePullBlock(&m_context.pBlock);
    }

    memset(&m_context, 0, sizeof(m_context));
}

// ============================================================================================================
// Helper method to send a payload, handling backwards compatibility and retrying.
Result URIClient::SendURIPayload(
    const SizedPayloadContainer& container,
    uint32 timeoutInMs,
    uint32 retryInMs)
{
    // Use the legacy size for the container if we're connected to an older client, otherwise use the real size.
    const uint32 payloadSize = (GetSessionVersion() >= URI_POST_PROTOCOL_VERSION) ? container.payloadSize
                                                                                  : kLegacyMaxSize;

    return SendSizedPayload(container.payload,
                            payloadSize,
                            timeoutInMs,
                            retryInMs);
}

// ============================================================================================================
// Helper method to handle receiving a payload from a SizedPayloadContainer, including retrying if busy.
Result URIClient::ReceiveURIPayload(
    SizedPayloadContainer* pContainer,
    uint32 timeoutInMs,
    uint32 retryInMs)
{
    return ReceiveSizedPayload(pContainer->payload,
                                sizeof(pContainer->payload),
                                &pContainer->payloadSize,
                                timeoutInMs,
                                retryInMs);
}

// ============================================================================================================
// Helper method to send and then receive using a SizedPayloadContainer object.
Result URIClient::TransactURIPayload(
    SizedPayloadContainer* pContainer,
    uint32 timeoutInMs,
    uint32 retryInMs)
{
    Result result = SendURIPayload(*pContainer, timeoutInMs, retryInMs);
    if (result == Result::Success)
    {
        result = ReceiveURIPayload(pContainer, timeoutInMs, retryInMs);
    }
    return result;
}
}

} // DevDriver
