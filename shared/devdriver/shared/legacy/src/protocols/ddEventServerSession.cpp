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

#include <protocols/ddEventServerSession.h>
#include <protocols/ddEventServer.h>
#include <util/ddByteReader.h>

namespace DevDriver
{
    namespace EventProtocol
    {
        EventServerSession::EventServerSession(
            const AllocCb&                     allocCb,
            SharedPointer<ISession>            pSession,
            EventServer*                       pServer,
            TransferProtocol::TransferManager* pTransferManager)
            : m_pServer(pServer)
            , m_pSession(pSession)
            , m_allocCb(allocCb)
            , m_state(SessionState::ReceivePayload)
            , m_pTransferManager(pTransferManager)
            , m_eventPayloadPending(false)
            , m_eventChunkInfo({ nullptr, 0 })

        {
            DD_ASSERT(m_pTransferManager != nullptr);
            DD_UNUSED(m_allocCb);
        }

        EventServerSession::~EventServerSession()
        {
            if (m_pUpdateBlock.IsNull() == false)
            {
                m_pTransferManager->CloseServerBlock(m_pUpdateBlock);
            }
        }

        void EventServerSession::UpdateSession()
        {
            DD_ASSERT(this == reinterpret_cast<EventServerSession*>(m_pSession->GetUserData()));

            switch (m_state)
            {
                case SessionState::ReceivePayload:
                {
                    Result result = m_pSession->ReceivePayload(&m_payloadContainer, kNoWait);

                    if (result == Result::Success)
                    {
                        m_state = SessionState::ProcessPayload;
                    }
                    else
                    {
                        // We should only receive specific error codes here.
                        // Assert if we see an unexpected error code.
                        DD_ASSERT((result == Result::Error)    ||
                                  (result == Result::NotReady) ||
                                  (result == Result::EndOfStream));

                        if (result == Result::NotReady)
                        {
                            SendEventData();
                        }
                    }
                    break;
                }

                case SessionState::ProcessPayload:
                {
                    SizedPayloadContainer& container = m_payloadContainer;
                    switch (container.GetPayload<EventHeader>().command)
                    {
                        case EventMessage::QueryProvidersRequest:
                        {
                            m_state = HandleQueryProvidersRequest(container);
                            break;
                        }

                        case EventMessage::AllocateProviderUpdatesRequest:
                        {
                            m_state = HandleAllocateProviderUpdatesRequest(container);
                            break;
                        }

                        case EventMessage::ApplyProviderUpdatesRequest:
                        {
                            m_state = HandleApplyProviderUpdatesRequest(container);
                            break;
                        }

                        default:
                            DD_UNREACHABLE();
                            break;
                    }
                    break;
                }

                case SessionState::SendPayload:
                {
                    const Result result = m_pSession->Send(m_payloadContainer.payloadSize, &m_payloadContainer.payload, kNoWait);
                    if (result == Result::Success)
                    {
                        m_state = SessionState::ReceivePayload;
                    }
                    break;
                }

                default:
                {
                    DD_ASSERT_ALWAYS();
                    break;
                }
            }
        }

        SessionState EventServerSession::HandleQueryProvidersRequest(SizedPayloadContainer& container)
        {
            BlockId blockId = TransferProtocol::kInvalidBlockId;
            const Result result = m_pServer->BuildQueryProvidersResponse(&blockId);

            container.CreatePayload<QueryProvidersResponsePayload>(result, blockId);

            return SessionState::SendPayload;
        }

        SessionState EventServerSession::HandleAllocateProviderUpdatesRequest(SizedPayloadContainer& container)
        {
            Result result = Result::Error;

            TransferProtocol::BlockId blockId = TransferProtocol::kInvalidBlockId;

            // Lazily allocate a new server block if we don't already have one.
            if (m_pUpdateBlock.IsNull())
            {
                m_pUpdateBlock = m_pTransferManager->OpenServerBlock();
            }

            if (m_pUpdateBlock.IsNull() == false)
            {
                blockId = m_pUpdateBlock->GetBlockId();

                result = Result::Success;
            }

            container.CreatePayload<AllocateProviderUpdatesResponse>(result, blockId);

            return SessionState::SendPayload;
        }

        SessionState EventServerSession::HandleApplyProviderUpdatesRequest(SizedPayloadContainer& container)
        {
            Result result = Result::Error;

            if (m_pUpdateBlock.IsNull() == false)
            {
                result = Result::Success;

                ByteReader reader(m_pUpdateBlock->GetBlockData(), m_pUpdateBlock->GetBlockDataSize());

                while ((result == Result::Success) && (reader.Remaining() > 0))
                {
                    // Attempt to extract an update header
                    const ProviderUpdateHeader* pProviderUpdate = nullptr;
                    result = reader.Get(&pProviderUpdate);

                    if (result == Result::Success)
                    {
                        // Make sure there's enough data behind the header for the event data payload
                        result = reader.Skip(pProviderUpdate->GetEventDataSize());
                    }

                    if (result == Result::Success)
                    {
                        // All of the data is available. Apply the provider update.
                        result = m_pServer->ApplyProviderUpdate(pProviderUpdate);
                    }
                }

                // Reset the block back to its initial state now that we're finished with it.
                // This will allow us to use it again during later transactions.
                m_pUpdateBlock->Reset();
            }

            container.CreatePayload<ApplyProviderUpdatesResponse>(result);

            return SessionState::SendPayload;
        }

        void EventServerSession::SendEventData()
        {
            Result result = Result::Success;

            if (m_eventPayloadPending)
            {
                result = m_pSession->Send(m_eventPayloadContainer.payloadSize, &m_eventPayloadContainer.payload, kNoWait);
                if (result == Result::Success)
                {
                    m_eventPayloadPending = false;
                }
            }

            if (result == Result::Success)
            {
                // If we don't currently have a chunk, attempt to acquire one
                if (m_eventChunkInfo.pChunk == nullptr)
                {
                    m_eventChunkInfo.pChunk    = m_pServer->DequeueEventChunk();
                    m_eventChunkInfo.bytesSent = 0;
                }

                // While we have a valid chunk, attempt to send its data to the client
                while((m_eventChunkInfo.pChunk != nullptr) && (m_eventPayloadPending == false))
                {
                    size_t bytesRemaining = (m_eventChunkInfo.pChunk->dataSize - m_eventChunkInfo.bytesSent);

                    // We should never end up with 0 bytes to send or it means this chunk wasn't properly removed from the queue
                    // after sending data.
                    DD_ASSERT(bytesRemaining > 0);

                    // Write as much of the chunk into packets as we can
                    while (bytesRemaining > 0)
                    {
                        const size_t bytesToSend = Platform::Min(bytesRemaining, kMaxEventDataSize);
                        const uint8* pDataPtr = (m_eventChunkInfo.pChunk->data + m_eventChunkInfo.bytesSent);

                        m_eventPayloadContainer.CreatePayload<EventDataUpdatePayload>(pDataPtr, bytesToSend);

                        m_eventChunkInfo.bytesSent += bytesToSend;

                        bytesRemaining = (m_eventChunkInfo.pChunk->dataSize - m_eventChunkInfo.bytesSent);

                        result = m_pSession->Send(m_eventPayloadContainer.payloadSize, &m_eventPayloadContainer.payload, kNoWait);

                        if (result != Result::Success)
                        {
                            m_eventPayloadPending = true;

                            break;
                        }
                    }

                    if ((result == Result::Success) || (result == Result::NotReady))
                    {
                        // We should never have a successful result with leftover bytes
                        DD_ASSERT((result == Result::NotReady) || (bytesRemaining == 0));

                        // If we sent all the remaining bytes in the chunk, remove it from the queue
                        // and attempt to acquire a new one.
                        if (bytesRemaining == 0)
                        {
                            // Return the chunk to the chunk pool
                            m_pServer->FreeEventChunk(m_eventChunkInfo.pChunk);

                            // Attempt to acquire a new chunk
                            m_eventChunkInfo.pChunk    = m_pServer->DequeueEventChunk();
                            m_eventChunkInfo.bytesSent = 0;
                        }
                    }
                    else
                    {
                        // We've encountered an error, stop sending chunks
                        break;
                    }
                }
            }
        }
    }
}
