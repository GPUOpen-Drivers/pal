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

#include "protocols/ddTransferServer.h"
#include "ddTransferManager.h"
#include "msgChannel.h"

#define TRANSFER_SERVER_MIN_MAJOR_VERSION 1
#define TRANSFER_SERVER_MAX_MAJOR_VERSION 1

namespace DevDriver
{
    namespace TransferProtocol
    {
        enum class SessionState
        {
            ReceivePayload = 0,
            ProcessPayload,
            SendPayload,
            StartTransfer,
            TransferData
        };

        struct TransferSession
        {
            SessionState state;
            Version version;
            size_t totalBytes;
            size_t bytesSent;
            SharedPointer<LocalBlock> pBlock;
            TransferPayload payload;

            explicit TransferSession()
                : state(SessionState::ReceivePayload)
                , version(0)
                , totalBytes(0)
                , bytesSent(0)
            {
            }
        };

        // =====================================================================================================================
        TransferServer::TransferServer(IMsgChannel* pMsgChannel)
            : BaseProtocolServer(pMsgChannel, Protocol::Transfer, TRANSFER_SERVER_MIN_MAJOR_VERSION, TRANSFER_SERVER_MAX_MAJOR_VERSION)
            , m_registeredLocalBlocks(pMsgChannel->GetAllocCb())
        {
            DD_ASSERT(m_pMsgChannel != nullptr);
        }

        // =====================================================================================================================
        TransferServer::~TransferServer()
        {
        }

        // =====================================================================================================================
        void TransferServer::Finalize()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);
            BaseProtocolServer::Finalize();
        }

        // =====================================================================================================================
        bool TransferServer::AcceptSession(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);
            return true;
        }

        // =====================================================================================================================
        void TransferServer::SessionEstablished(const SharedPointer<ISession>& pSession)
        {
            // Allocate session data for the newly established session
            TransferSession* pSessionData = DD_NEW(TransferSession, m_pMsgChannel->GetAllocCb())();
            pSessionData->state = SessionState::ReceivePayload;
            memset(&pSessionData->payload, 0, sizeof(TransferPayload));

            pSession->SetUserData(pSessionData);
        }

        // =====================================================================================================================
        void TransferServer::UpdateSession(const SharedPointer<ISession>& pSession)
        {
            TransferSession* pSessionData = reinterpret_cast<TransferSession*>(pSession->GetUserData());

            // Process messages in when we're not executing a trace.
            switch (pSessionData->state)
            {
                case SessionState::ReceivePayload:
                {
                    uint32 bytesReceived = 0;
                    const Result result = pSession->Receive(sizeof(pSessionData->payload), &pSessionData->payload, &bytesReceived, kNoWait);

                    if (result == Result::Success)
                    {
                        DD_ASSERT(sizeof(pSessionData->payload) == bytesReceived);
                        pSessionData->state = SessionState::ProcessPayload;
                    }

                    break;
                }

                case SessionState::ProcessPayload:
                {
                    switch (pSessionData->payload.command)
                    {
                        case TransferMessage::TransferRequest:
                        {
                            const BlockId requestedBlockId = pSessionData->payload.transferRequest.blockId;

                            // Determine if the requested block is available.
                            // If the block is available, start the transfer process.
                            // If the block is not available, return an error response.

                            Platform::LockGuard<Platform::Mutex> lock(m_mutex);
                            SharedPointer<LocalBlock> pBlock = SharedPointer<LocalBlock>();

                            const auto blockIter = m_registeredLocalBlocks.Find(requestedBlockId);
                            if (blockIter != m_registeredLocalBlocks.End())
                            {
                                pBlock = blockIter->value;
                            }

                            // A block must be closed in order to be available for transfer.
                            const bool blockIsAvailable = (!pBlock.IsNull() && pBlock->IsClosed());
                            if (blockIsAvailable)
                            {
                                // @todo: > 4GB block size support?
                                const uint32 blockSizeInBytes = static_cast<uint32>(pBlock->GetBlockDataSize());

                                pSessionData->payload.command = TransferMessage::TransferDataHeader;
                                pSessionData->payload.transferDataHeader.result = Result::Success;
                                pSessionData->payload.transferDataHeader.sizeInBytes = blockSizeInBytes;
                                pSessionData->state = SessionState::StartTransfer;

                                // Notify the block that it's starting a new transfer.
                                pBlock->BeginTransfer();

                                pSessionData->pBlock = pBlock;
                                pSessionData->totalBytes = blockSizeInBytes;
                                pSessionData->bytesSent = 0;
                            }
                            else
                            {
                                pSessionData->payload.command = TransferMessage::TransferDataHeader;
                                pSessionData->payload.transferDataHeader.result = Result::Error;
                                pSessionData->payload.transferDataHeader.sizeInBytes = 0;
                                pSessionData->state = SessionState::SendPayload;
                            }

                            break;
                        }

                        case TransferMessage::TransferAbort:
                        {
                            // It's possible that we may receive a transfer abort request after we've already sent
                            // all the transfer data to the remote client successfully. This can happen when the
                            // remaining amount of data for the transfer fits into the entire send window.
                            // In this case, we still need to respond correctly and send the client an abort sentinel.
                            pSessionData->payload.command = TransferMessage::TransferDataSentinel;
                            pSessionData->payload.transferDataSentinel.result = Result::Aborted;
                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }

                        default:
                        {
                            // Invalid command
                            DD_UNREACHABLE();
                            break;
                        }
                    }

                    break;
                }

                case SessionState::TransferData:
                {
                    // Look for an abort request.
                    uint32 bytesReceived = 0;
                    const Result result = pSession->Receive(sizeof(pSessionData->payload), &pSessionData->payload, &bytesReceived, kNoWait);

                    // If we haven't received any messages from the client, then continue transferring data to them.
                    if (result == Result::NotReady)
                    {
                        TransferPayload payload = {};
                        payload.command = TransferMessage::TransferDataChunk;

                        while (pSessionData->bytesSent < pSessionData->totalBytes)
                        {
                            const uint8* pData = (pSessionData->pBlock->GetBlockData() + pSessionData->bytesSent);
                            const size_t bytesRemaining = (pSessionData->totalBytes - pSessionData->bytesSent);
                            const size_t bytesToSend = Platform::Min(sizeof(payload.transferDataChunk.data), bytesRemaining);
                            memcpy(payload.transferDataChunk.data, pData, bytesToSend);

                            const Result sendResult = pSession->Send(sizeof(payload), &payload, kNoWait);
                            if (sendResult == Result::Success)
                            {
                                pSessionData->bytesSent += bytesToSend;
                            }
                            else
                            {
                                break;
                            }
                        }

                        // If we've finished transferring all block data, send the sentinel and free the block.
                        if (pSessionData->bytesSent == pSessionData->totalBytes)
                        {
                            // Notify the block that a transfer is completing.
                            pSessionData->pBlock->EndTransfer();

                            pSessionData->payload.command = TransferMessage::TransferDataSentinel;
                            pSessionData->payload.transferDataSentinel.result = Result::Success;
                            pSessionData->pBlock.Clear();
                            pSessionData->state = SessionState::SendPayload;
                        }
                    }
                    else if (result == Result::Success)
                    {
                        // Make sure the message we receive is the correct size.
                        DD_ASSERT(sizeof(pSessionData->payload) == bytesReceived);

                        if (pSessionData->payload.command == TransferMessage::TransferAbort)
                        {
                            pSessionData->payload.command = TransferMessage::TransferDataSentinel;
                            pSessionData->payload.transferDataSentinel.result = Result::Aborted;
                            pSessionData->state = SessionState::SendPayload;
                        }
                        else
                        {
                            // We should only ever receive abort requests in this state. Send back an error.
                            pSessionData->payload.command = TransferMessage::TransferDataSentinel;
                            pSessionData->payload.transferDataSentinel.result = Result::Error;
                            pSessionData->state = SessionState::SendPayload;

                            DD_ALERT_REASON("Invalid response received");
                        }
                    }
                    else
                    {
                        // We've encountered an error while receiving. Do nothing. The session will close itself soon.
                    }

                    break;
                }

                case SessionState::StartTransfer:
                {
                    const TransferPayload& payload = pSessionData->payload;

                    // We should only be sending the header in this state.
                    DD_ASSERT(payload.command == TransferMessage::TransferDataHeader);

                    if (pSession->Send(sizeof(payload), &payload, kNoWait) == Result::Success)
                    {
                        pSessionData->state = SessionState::TransferData;
                    }

                    break;
                }

                case SessionState::SendPayload:
                {
                    const TransferPayload& payload = pSessionData->payload;
                    if (pSession->Send(sizeof(payload), &payload, kNoWait) == Result::Success)
                    {
                        pSessionData->state = SessionState::ReceivePayload;
                    }

                    break;
                }

                default:
                {
                    DD_UNREACHABLE();
                    break;
                }
            }
        }

        // =====================================================================================================================
        void TransferServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
        {
            DD_UNUSED(terminationReason);
            TransferSession *pTransferSession = reinterpret_cast<TransferSession*>(pSession->SetUserData(nullptr));

            // Free the session data
            if (pTransferSession != nullptr)
            {
                // If we're terminating a session with a valid block, then that means the transfer did not finish properly.
                // Make sure to notify the block that the transfer is now ending so we don't throw off the internal counter.
                if (pTransferSession->pBlock.IsNull() == false)
                {
                    pTransferSession->pBlock->EndTransfer();
                }

                DD_DELETE(pTransferSession, m_pMsgChannel->GetAllocCb());
            }
        }

        // =====================================================================================================================
        void TransferServer::RegisterLocalBlock(const SharedPointer<LocalBlock>& pLocalBlock)
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            m_registeredLocalBlocks.Create(pLocalBlock->GetBlockId(), pLocalBlock);
        }

        // =====================================================================================================================
        void TransferServer::UnregisterLocalBlock(const SharedPointer<LocalBlock>& pLocalBlock)
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            m_registeredLocalBlocks.Erase(pLocalBlock->GetBlockId());
        }
    }
} // DevDriver
