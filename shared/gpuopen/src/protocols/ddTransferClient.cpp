/*
 *******************************************************************************
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "protocols/ddTransferClient.h"
#include <cstring>

#define TRANSFER_CLIENT_MIN_MAJOR_VERSION 1
#define TRANSFER_CLIENT_MAX_MAJOR_VERSION 1

namespace DevDriver
{
    namespace TransferProtocol
    {
        // =====================================================================================================================
        TransferClient::TransferClient(IMsgChannel* pMsgChannel)
            : BaseProtocolClient(pMsgChannel, Protocol::Transfer, TRANSFER_CLIENT_MIN_MAJOR_VERSION, TRANSFER_CLIENT_MAX_MAJOR_VERSION)
        {
            memset(&m_transferContext, 0, sizeof(m_transferContext));
        }

        // =====================================================================================================================
        TransferClient::~TransferClient()
        {
        }

        // =====================================================================================================================
        Result TransferClient::RequestTransfer(BlockId blockId, size_t* pTransferSizeInBytes)
        {
            Result result = Result::Error;

            if ((m_transferContext.state == TransferState::Idle) &&
                (pTransferSizeInBytes != nullptr))
            {
                TransferPayload payload = {};
                payload.command = TransferMessage::TransferRequest;
                payload.transferRequest.blockId = blockId;

                result = SendPayload(payload);

                if (result == Result::Success)
                {
                    // We've successfully sent the request to the server.
                    // Attempt to receive the transfer data header.
                    result = ReceivePayload(payload);
                    if ((result == Result::Success) && (payload.command == TransferMessage::TransferDataHeader))
                    {
                        // We've successfully received the transfer data header. Check if the transfer request was successful.
                        result = payload.transferDataHeader.result;
                        if (result == Result::Success)
                        {
                            m_transferContext.state = TransferState::TransferInProgress;
                            m_transferContext.totalBytes = payload.transferDataHeader.sizeInBytes;
                            m_transferContext.numChunks = ((m_transferContext.totalBytes % kMaxTransferDataChunkSize) == 0) ? (m_transferContext.totalBytes / kMaxTransferDataChunkSize)
                                                                                                                            : ((m_transferContext.totalBytes / kMaxTransferDataChunkSize) + 1);
                            m_transferContext.numChunksReceived = 0;
                            m_transferContext.dataChunkSizeInBytes = 0;
                            m_transferContext.dataChunkBytesRead = 0;

                            *pTransferSizeInBytes = payload.transferDataHeader.sizeInBytes;
                        }
                        else
                        {
                            // The transfer failed on the remote server.
                            m_transferContext.state = TransferState::Error;
                        }
                    }
                    else
                    {
                        // We either didn't receive a response, or we received an invalid response.
                        m_transferContext.state = TransferState::Error;
                        result = Result::Error;
                    }
                }
                else
                {
                    // If we fail to send the payload, fail the transfer.
                    m_transferContext.state = TransferState::Error;
                    result = Result::Error;
                }
            }

            return result;
        }

        // =====================================================================================================================
        Result TransferClient::ReadTransferData(uint8* pDstBuffer, size_t bufferSize, size_t* pBytesRead)
        {
            Result result = Result::Error;

            if ((m_transferContext.state == TransferState::TransferInProgress) && (pBytesRead != nullptr))
            {
                result = Result::Success;

                if (m_transferContext.numChunks == 0)
                {
                    // There's no data to transfer, immediately return end of stream.
                    result = Result::EndOfStream;
                    m_transferContext.state = TransferState::Idle;

                    *pBytesRead = 0;
                }
                else if (bufferSize > 0)
                {
                    // There's space available in the caller's buffer, attempt to write data into it.
                    size_t remainingBufferSize = bufferSize;
                    while ((remainingBufferSize > 0) &&
                           (m_transferContext.state == TransferState::TransferInProgress))
                    {
                        // If we have local data, read from that.
                        const size_t dataChunkBytesAvailable = (m_transferContext.dataChunkSizeInBytes - m_transferContext.dataChunkBytesRead);
                        if ((m_transferContext.lastPayload.command == TransferMessage::TransferDataChunk) &&
                            (dataChunkBytesAvailable > 0))
                        {
                            const size_t bytesToRead = Platform::Min(remainingBufferSize, dataChunkBytesAvailable);
                            const uint8* pData = (m_transferContext.lastPayload.transferDataChunk.data + m_transferContext.dataChunkBytesRead);
                            memcpy(pDstBuffer + (bufferSize - remainingBufferSize), pData, bytesToRead);
                            m_transferContext.dataChunkBytesRead += bytesToRead;
                            remainingBufferSize -= bytesToRead;

                            // If this is the last of the data for the transfer, return end of stream and return to the idle state.
                            if ((m_transferContext.dataChunkBytesRead == m_transferContext.dataChunkSizeInBytes) &&
                                (m_transferContext.numChunksReceived == m_transferContext.numChunks))
                            {
                                result = Result::EndOfStream;
                                m_transferContext.state = TransferState::Idle;
                            }
                        }
                        else if (m_transferContext.numChunksReceived < m_transferContext.numChunks)
                        {
                            // Attempt to fetch a new chunk if we're out of data.
                            result = ReceivePayload(m_transferContext.lastPayload, kTransferChunkTimeoutInMs);

                            if (result == Result::Success)
                            {
                                if (m_transferContext.lastPayload.command == TransferMessage::TransferDataChunk)
                                {
                                    ++m_transferContext.numChunksReceived;

                                    // Handle a partial chunk at the end of the stream.
                                    if (m_transferContext.numChunksReceived == m_transferContext.numChunks)
                                    {
                                        m_transferContext.dataChunkSizeInBytes = (m_transferContext.totalBytes % kMaxTransferDataChunkSize == 0) ? kMaxTransferDataChunkSize
                                                                                                                                                 : (m_transferContext.totalBytes % kMaxTransferDataChunkSize);

                                        // Make sure we read the sentinel value before returning. It should always mark the end of the transfer data chunk stream.
                                        TransferPayload sentinelPayload = {};
                                        result = ReceivePayload(sentinelPayload, kTransferChunkTimeoutInMs);

                                        if ((result != Result::Success) || (sentinelPayload.command != TransferMessage::TransferDataSentinel))
                                        {
                                            // Failed to receive the sentinel. Fail the transfer.
                                            m_transferContext.state = TransferState::Error;
                                        }
                                    }
                                    else
                                    {
                                        m_transferContext.dataChunkSizeInBytes = kMaxTransferDataChunkSize;
                                    }

                                    m_transferContext.dataChunkBytesRead = 0;
                                }
                                else
                                {
                                    // Failed to receive a transfer data chunk. Fail the transfer.
                                    m_transferContext.state = TransferState::Error;
                                }
                            }
                            else
                            {
                                // Failed to receive a transfer data chunk. Fail the transfer.
                                m_transferContext.state = TransferState::Error;
                            }
                        }
                        else
                        {
                            // This should never happen.
                            DD_UNREACHABLE();
                        }
                    }

                    *pBytesRead = (bufferSize - remainingBufferSize);
                }
                else
                {
                    // No space available for writing in the caller's buffer.
                    *pBytesRead = 0;
                }
            }

            return result;
        }

        // =====================================================================================================================
        Result TransferClient::AbortTransfer()
        {
            Result result = Result::Error;

            TransferPayload payload = {};

            if (m_transferContext.state == TransferState::TransferInProgress)
            {
                payload.command = TransferMessage::TransferAbort;

                result = SendPayload(payload);

                if (result == Result::Success)
                {
                    // Discard all messages until we find the transfer data sentinel.
                    while ((result == Result::Success) && (payload.command != TransferMessage::TransferDataSentinel))
                    {
                        result = ReceivePayload(payload);
                    }

                    if ((result == Result::Success) &&
                        (payload.command == TransferMessage::TransferDataSentinel))
                    {
                        // We've successfully aborted the transfer.

                        // We've either reached the original sentinel that indicates the end of the transfer or we've
                        // received a sentinel in response to calling abort. Sanity check the results with an assert.
                        DD_ASSERT((payload.transferDataSentinel.result == Result::Aborted) ||
                                  (payload.transferDataSentinel.result == Result::Success));

                        m_transferContext.state = TransferState::Idle;
                    }
                    else
                    {
                        // Fail the transfer if this process does not succeed.
                        m_transferContext.state = TransferState::Error;
                        result = Result::Error;
                    }
                }
                else
                {
                    // If we fail to send the payload, fail the transfer.
                    m_transferContext.state = TransferState::Error;
                    result = Result::Error;
                }
            }

            return result;
        }

        // =====================================================================================================================
        void TransferClient::ResetState()
        {
            memset(&m_transferContext, 0, sizeof(m_transferContext));
        }
    }

} // DevDriver
