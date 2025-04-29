/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

/// Specify a memory usage target for the set of allocated event chunks
/// The implementation will attempt to keep memory usage at or below this level at all times.
/// This level may be exceeded when large events are logged, but memory usage will eventually return to the target
/// level over time.
static constexpr size_t kMemoryUsageTargetInBytes = (4 * 1024 * 1024); // 4 MB
static constexpr size_t kTargetAllocatedChunks = (kMemoryUsageTargetInBytes / sizeof(EventChunk));
static constexpr size_t kTrimFrequencyInMs = 16;
static constexpr size_t kMaxChunksPerTrim = 16;

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
    , m_assignedProviderId(0)
    , m_eventChunkPool(allocCb)
    , m_eventChunkQueue(allocCb)
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

    // Free any event chunks that are still in the pool before we destroy the server.
    for (EventChunk* pChunk : m_eventChunkPool)
    {
        DD_FREE(pChunk, m_allocCb);
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

                case EventMessage::SubscribeToProviderRequest:
                {
                    m_state = HandleSubscribeToProviderRequest(container);
                    break;
                }

                case EventMessage::UnsubscribeFromProviderRequest:
                {
                    m_state = HandleUnsubscribeFromProviderRequest();
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

    // Run a trim operation every once in a while to make sure we give up memory we don't need anymore.
    const uint64 currentTime = Platform::GetCurrentTimeInMs();
    if (currentTime >= m_nextTrimTime)
    {
        m_nextTrimTime = currentTime + kTrimFrequencyInMs;
        TrimEventChunkMemory();
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

SessionState EventServerSession::HandleSubscribeToProviderRequest(SizedPayloadContainer& container)
{
    SubscribeToProviderRequest& request =
        m_payloadContainer.GetPayload<SubscribeToProviderRequest>();
    Result result = m_pServer->AssignSessionToProvider(this, request.providerId);

    container.CreatePayload<SubscribeToProviderResponse>(result);

    return SessionState::SendPayload;
}

SessionState EventServerSession::HandleUnsubscribeFromProviderRequest()
{
    if (m_assignedProviderId > 0)
    {
        m_pServer->UnassignSessionFromProvider(this, m_assignedProviderId);
    }
    return SessionState::ReceivePayload;
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
            m_eventChunkInfo.pChunk    = DequeueEventChunk();
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
                    FreeEventChunk(m_eventChunkInfo.pChunk);

                    // Attempt to acquire a new chunk
                    m_eventChunkInfo.pChunk    = DequeueEventChunk();
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

Result EventServerSession::AllocateEventChunk(EventChunk** ppChunk)
{
    DD_ASSERT(ppChunk != nullptr);

    Platform::LockGuard<Platform::AtomicLock> poolLock(m_eventPoolMutex);

    Result result = Result::Success;

    if (m_eventChunkPool.IsEmpty() == false)
    {
        EventChunk* pChunk = nullptr;
        m_eventChunkPool.PopBack(&pChunk);

        // Reset the chunk before we hand it back to the caller
        pChunk->dataSize = 0;

        *ppChunk = pChunk;
    }
    else
    {
        EventChunk* pChunk = reinterpret_cast<EventChunk*>(DD_CALLOC(sizeof(EventChunk),
                                                                     alignof(EventChunk),
                                                                     m_allocCb));
        if (pChunk != nullptr)
        {
            *ppChunk = pChunk;
        }
        else
        {
            result = Result::InsufficientMemory;
        }
    }

    return result;
}

void EventServerSession::FreeEventChunk(EventChunk* pChunk)
{
    DD_ASSERT(pChunk != nullptr);

    Platform::LockGuard<Platform::AtomicLock> poolLock(m_eventPoolMutex);

    if (IsTargetMemoryUsageExceeded())
    {
        // Free the chunk's memory immediately if we're already past our target memory usage
        DD_FREE(pChunk, m_allocCb);
    }
    else
    {
        // Return the chunk's memory to the pool if we're under our usage budget
        DD_UNHANDLED_RESULT(m_eventChunkPool.PushBack(pChunk) ? Result::Success : Result::InsufficientMemory);
    }
}

void EventServerSession::EnqueueEventChunks(size_t numChunks, EventChunk** ppChunks)
{
    DD_ASSERT(ppChunks != nullptr);

    Platform::LockGuard<Platform::AtomicLock> queueLock(m_eventQueueMutex);

    Result result = Result::Success;

    for (size_t chunkIndex = 0; chunkIndex < numChunks; ++chunkIndex)
    {
        EventChunk* pChunk = ppChunks[chunkIndex];

        // Due to the fact that the event providers never know exactly how much data they'll need, they may over-allocate event
        // chunks in some cases. This can lead to them submitting empty chunks to the server. We just filter them out here since
        // we know they don't contain any useful data.
        if (pChunk->IsEmpty() == false)
        {
            result = (m_eventChunkQueue.PushBack(pChunk) ? Result::Success : Result::InsufficientMemory);
        }
        else
        {
            FreeEventChunk(pChunk);
        }

        if (result != Result::Success)
        {
            // The only way PushBack will fail is if we run out of memory.  We can't do anything useful at that point
            // so just assert and break out of the loop.
            DD_ASSERT_ALWAYS();
            break;
        }
    }
}

void EventServerSession::SetProviderId(EventProviderId providerId)
{
    m_assignedProviderId = providerId;
}

EventChunk* EventServerSession::DequeueEventChunk()
{
    Platform::LockGuard<Platform::AtomicLock> queueLock(m_eventQueueMutex);

    EventChunk* pChunk = nullptr;

    // Attempt to pop the next chunk from the queue.
    // It's okay if this fails since it just means we don't have any chunks available and we should return nullptr.
    m_eventChunkQueue.PopFront(&pChunk);

    return pChunk;
}

bool EventServerSession::IsTargetMemoryUsageExceeded() const
{
    return (m_eventChunkPool.Size() > kTargetAllocatedChunks);
}

void EventServerSession::TrimEventChunkMemory()
{
    // Trimming should only happen in the background if there's no contention for the event chunk pool.
    // When an application is making heavy use of the memory pool, we shouldn't waste time trying to trim it.
    if (m_eventPoolMutex.TryLock())
    {
        // If we have more chunks allocated than we should, we'll attempt to deallocate a few of them here.
        // We limit the amount of chunks we free in a single trim cycle to reduce the runtime overhead of this operation.
        size_t numChunksTrimmed = 0;
        while (IsTargetMemoryUsageExceeded() && (numChunksTrimmed < kMaxChunksPerTrim))
        {
            EventChunk* pChunk = nullptr;
            m_eventChunkPool.PopBack(&pChunk);

            DD_FREE(pChunk, m_allocCb);

            ++numChunksTrimmed;
        }

        m_eventPoolMutex.Unlock();
    }
}

} // namespace EventProtocol
} // namespace DevDriver
