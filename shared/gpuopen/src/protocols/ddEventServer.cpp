/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  ddEventServer.cpp
* @brief Implementation for EventServer
***********************************************************************************************************************
*/

#include <protocols/ddEventServer.h>
#include <protocols/ddEventProvider.h>
#include <protocols/ddEventProtocol.h>
#include <protocols/ddEventServerSession.h>
#include <ddTransferManager.h>
#include <ddPlatform.h>
#include <msgChannel.h>

#define EVENT_SERVER_MIN_VERSION EVENT_INDEXING_VERSION
#define EVENT_SERVER_MAX_VERSION EVENT_INDEXING_VERSION

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

EventServer::EventServer(IMsgChannel* pMsgChannel)
    : BaseProtocolServer(pMsgChannel, Protocol::Event, EVENT_SERVER_MIN_VERSION, EVENT_SERVER_MAX_VERSION)
    , m_eventProviders(pMsgChannel->GetAllocCb())
    , m_eventChunkPool(pMsgChannel->GetAllocCb())
    , m_eventChunkQueue(pMsgChannel->GetAllocCb())
    , m_pActiveSession(nullptr)
    , m_nextTrimTime(0)
{
    DD_ASSERT(m_pMsgChannel != nullptr);
}

EventServer::~EventServer()
{
    // All providers should be unregistered before the event server is destroyed.
    // If this is not the case, event chunks may leak because they're still owned by the providers
    // and now they can't be returned to the event server!
    DD_ASSERT(m_eventProviders.IsEmpty());

    // Free any event chunks that are still in the pool before we destroy the server.
    for (EventChunk* pChunk : m_eventChunkPool)
    {
        DD_FREE(pChunk, m_pMsgChannel->GetAllocCb());
    }
}

bool EventServer::AcceptSession(const SharedPointer<ISession>& pSession)
{
    DD_UNUSED(pSession);

    return (m_pActiveSession == nullptr);
}

void EventServer::SessionEstablished(const SharedPointer<ISession>& pSession)
{
    // Allocate session data for the newly established session
    EventServerSession* pEventSession = DD_NEW(EventServerSession, m_pMsgChannel->GetAllocCb())(m_pMsgChannel->GetAllocCb(), pSession, this, &m_pMsgChannel->GetTransferManager());
    pSession->SetUserData(pEventSession);

    m_pActiveSession = pEventSession;
}

void EventServer::UpdateSession(const SharedPointer<ISession>& pSession)
{
    EventServerSession* pEventSession = reinterpret_cast<EventServerSession*>(pSession->GetUserData());
    DD_ASSERT(pEventSession == m_pActiveSession);

    m_eventProvidersMutex.Lock();

    for (auto providerIter : m_eventProviders)
    {
        providerIter.value->Update();
    }

    m_eventProvidersMutex.Unlock();

    pEventSession->UpdateSession();

    // Run a trim operation every once in a while to make sure we give up memory we don't need anymore.
    const uint64 currentTime = Platform::GetCurrentTimeInMs();
    if (currentTime >= m_nextTrimTime)
    {
        m_nextTrimTime = currentTime + kTrimFrequencyInMs;

        TrimEventChunkMemory();
    }
}

void EventServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
{
    DD_UNUSED(terminationReason);
    EventServerSession* pEventSession = reinterpret_cast<EventServerSession*>(pSession->SetUserData(nullptr));
    DD_ASSERT(pEventSession == m_pActiveSession);

    m_eventProvidersMutex.Lock();

    for (auto providerIter : m_eventProviders)
    {
        providerIter.value->Disable();
    }

    m_eventProvidersMutex.Unlock();

    // Free the session data
    if (pEventSession != nullptr)
    {
        DD_DELETE(pEventSession, m_pMsgChannel->GetAllocCb());

        m_pActiveSession = nullptr;
    }
}

Result EventServer::RegisterProvider(BaseEventProvider* pProvider)
{
    Result result = Result::InvalidParameter;

    if (pProvider != nullptr)
    {
        const EventProviderId providerId = pProvider->GetId();

        Platform::LockGuard<Platform::AtomicLock> providersLock(m_eventProvidersMutex);

        if (m_eventProviders.Contains(providerId) == false)
        {
            result = m_eventProviders.Insert(providerId, pProvider);
            if (result == Result::Success)
            {
                pProvider->Register(this);
            }
        }
        else
        {
            result = Result::Error;
        }
    }

    return result;
}

Result EventServer::UnregisterProvider(BaseEventProvider* pProvider)
{
    Result result = Result::InvalidParameter;

    if (pProvider != nullptr)
    {
        const EventProviderId providerId = pProvider->GetId();

        Platform::LockGuard<Platform::AtomicLock> providersLock(m_eventProvidersMutex);

        const auto providerIter = m_eventProviders.Find(providerId);
        if (providerIter != m_eventProviders.End())
        {
            m_eventProviders.Remove(providerIter);
            providerIter->value->Unregister();

            result = Result::Success;
        }
        else
        {
            result = Result::Error;
        }
    }

    return result;
}

Result EventServer::AllocateEventChunk(EventChunk** ppChunk)
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
                                                                     m_pMsgChannel->GetAllocCb()));
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

void EventServer::FreeEventChunk(EventChunk* pChunk)
{
    DD_ASSERT(pChunk != nullptr);

    Platform::LockGuard<Platform::AtomicLock> poolLock(m_eventPoolMutex);

    if (IsTargetMemoryUsageExceeded())
    {
        // Free the chunk's memory immediately if we're already past our target memory usage
        DD_FREE(pChunk, m_pMsgChannel->GetAllocCb());
    }
    else
    {
        // Return the chunk's memory to the pool if we're under our usage budget
        DD_UNHANDLED_RESULT(m_eventChunkPool.PushBack(pChunk) ? Result::Success : Result::InsufficientMemory);
    }
}

void EventServer::EnqueueEventChunks(size_t numChunks, EventChunk** ppChunks)
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

EventChunk* EventServer::DequeueEventChunk()
{
    Platform::LockGuard<Platform::AtomicLock> queueLock(m_eventQueueMutex);

    EventChunk* pChunk = nullptr;

    // Attempt to pop the next chunk from the queue.
    // It's okay if this fails since it just means we don't have any chunks available and we should return nullptr.
    m_eventChunkQueue.PopFront(&pChunk);

    return pChunk;
}

Result EventServer::BuildQueryProvidersResponse(BlockId* pBlockId)
{
    Result result = Result::InvalidParameter;

    if (pBlockId != nullptr)
    {
        auto pServerBlock = m_pMsgChannel->GetTransferManager().OpenServerBlock();
        if (pServerBlock.IsNull() == false)
        {
            Platform::LockGuard<Platform::AtomicLock> providersLock(m_eventProvidersMutex);

            // Write the response header
            const QueryProvidersResponseHeader responseHeader(static_cast<uint32>(m_eventProviders.Size()));
            pServerBlock->Write(&responseHeader, sizeof(responseHeader));

            for (const auto& providerPair : m_eventProviders)
            {
                const BaseEventProvider* pProvider = providerPair.value;

                // Write the provider header
                const ProviderDescriptionHeader providerHeader = pProvider->GetHeader();
                pServerBlock->Write(&providerHeader, sizeof(providerHeader));

                // Write the event data
                pServerBlock->Write(pProvider->GetEventData(), pProvider->GetEventDataSize());

                // Write the event description data
                pServerBlock->Write(pProvider->GetEventDescriptionData(), pProvider->GetEventDescriptionDataSize());
            }

            // Close the block to expose it to external clients
            pServerBlock->Close();

            // Return the block id
            *pBlockId = pServerBlock->GetBlockId();

            result = Result::Success;
        }
        else
        {
            result = Result::InsufficientMemory;
        }
    }

    return result;
}

Result EventServer::ApplyProviderUpdate(const ProviderUpdateHeader* pUpdate)
{
    Result result = Result::InvalidParameter;

    if (pUpdate != nullptr)
    {
        const EventProviderId providerId = pUpdate->providerId;

        Platform::LockGuard<Platform::AtomicLock> providersLock(m_eventProvidersMutex);

        const auto providerIter = m_eventProviders.Find(providerId);
        if (providerIter != m_eventProviders.End())
        {
            result = Result::Success;

            BaseEventProvider* pProvider = providerIter->value;
            if (pUpdate->isEnabled)
            {
                pProvider->Enable();
            }
            else
            {
                pProvider->Disable();
            }

            // If the client provides a valid event data update, attempt to apply it.
            if (pUpdate->GetEventDataSize() > 0)
            {
                // Calculate the number of bits in the event data update
                // The client should have sent at least enough bits
                const size_t numEventDataBits = (pUpdate->GetEventDataSize() * 8);
                if (numEventDataBits >= pProvider->GetNumEvents())
                {
                    pProvider->UpdateEventData(VoidPtrInc(pUpdate, pUpdate->GetEventDataOffset()), pUpdate->GetEventDataSize());
                }
                else
                {
                    result = Result::Error;
                }
            }
        }
        else
        {
            result = Result::Error;
        }
    }

    return result;
}

bool EventServer::IsTargetMemoryUsageExceeded() const
{
    return (m_eventChunkPool.Size() > kTargetAllocatedChunks);
}

void EventServer::TrimEventChunkMemory()
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

            DD_FREE(pChunk, m_pMsgChannel->GetAllocCb());

            ++numChunksTrimmed;
        }

        m_eventPoolMutex.Unlock();
    }
}

} // EventProtocol

} // DevDriver
