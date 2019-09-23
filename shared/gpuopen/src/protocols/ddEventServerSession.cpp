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
/**
***********************************************************************************************************************
* @file  ddEventServerSession.cpp
* @brief Implementation for EventServerSession
***********************************************************************************************************************
*/

#include <protocols/ddEventServerSession.h>
#include <protocols/ddEventServer.h>

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

        Result EventServerSession::SendEventData(const void* pEventData, size_t eventDataSize)
        {
            Result result = Result::InvalidParameter;

            // @TODO: Support blocking when larger packets need to be sent
            //        Also, this should be writing into a larger ring buffer that gets flushed
            //        periodically.
            if (eventDataSize <= kMaxEventDataSize)
            {
                SizedPayloadContainer container;
                container.CreatePayload<EventDataUpdatePayload>(pEventData, eventDataSize);

                result = m_pSession->Send(container.payloadSize, &container.payload, kNoWait);
            }

            return result;
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

            if (m_pUpdateBlock.IsNull())
            {
                m_pUpdateBlock = m_pTransferManager->OpenServerBlock();
                if (m_pUpdateBlock.IsNull() == false)
                {
                    blockId = m_pUpdateBlock->GetBlockId();

                    result = Result::Success;
                }
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

                size_t byteOffset = 0;
                while ((byteOffset < m_pUpdateBlock->GetBlockDataSize()) && (result == Result::Success))
                {
                    const ProviderUpdateHeader* pProviderUpdate =
                        reinterpret_cast<const ProviderUpdateHeader*>(VoidPtrInc(m_pUpdateBlock->GetBlockData(), byteOffset));

                    result = m_pServer->ApplyProviderUpdate(pProviderUpdate);

                    byteOffset += pProviderUpdate->GetNextProviderUpdateOffset();
                }

                if (result == Result::Success)
                {
                    // These should match exactly for a successful update
                    DD_ASSERT(byteOffset == m_pUpdateBlock->GetBlockDataSize());
                }
            }

            container.CreatePayload<ApplyProviderUpdatesResponse>(result);

            return SessionState::SendPayload;
        }
    }
}
