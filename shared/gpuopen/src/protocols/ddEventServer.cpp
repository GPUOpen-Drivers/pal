/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#define EVENT_SERVER_MIN_VERSION EVENT_INITIAL_VERSION
#define EVENT_SERVER_MAX_VERSION EVENT_INITIAL_VERSION

namespace DevDriver
{

namespace EventProtocol
{

EventServer::EventServer(IMsgChannel* pMsgChannel)
    : BaseProtocolServer(pMsgChannel, Protocol::Event, EVENT_SERVER_MIN_VERSION, EVENT_SERVER_MAX_VERSION)
    , m_eventProviders(pMsgChannel->GetAllocCb())
    , m_eventChunkPool(pMsgChannel->GetAllocCb())
    , m_eventChunkAllocList(pMsgChannel->GetAllocCb())
    , m_eventChunkQueue(pMsgChannel->GetAllocCb())
    , m_pActiveSession(nullptr)
{
    DD_ASSERT(m_pMsgChannel != nullptr);
}

EventServer::~EventServer()
{
    for (EventChunk* pChunk : m_eventChunkAllocList)
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

    const uint64 currentTime = Platform::GetCurrentTimeInMs();
    for (auto providerIter : m_eventProviders)
    {
        providerIter.value->Update(currentTime);
    }

    m_eventProvidersMutex.Unlock();

    Platform::LockGuard<Platform::Mutex> queueLock(m_eventQueueMutex);

    pEventSession->UpdateSession();
}

void EventServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
{
    DD_UNUSED(terminationReason);
    EventServerSession* pEventSession = reinterpret_cast<EventServerSession*>(pSession->SetUserData(nullptr));
    DD_ASSERT(pEventSession == m_pActiveSession);

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

        Platform::LockGuard<Platform::Mutex> providersLock(m_eventProvidersMutex);

        if (m_eventProviders.Contains(providerId) == false)
        {
            result = m_eventProviders.Insert(providerId, pProvider);
            if (result == Result::Success)
            {
                pProvider->SetEventServer(this);
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

        Platform::LockGuard<Platform::Mutex> providersLock(m_eventProvidersMutex);

        const auto providerIter = m_eventProviders.Find(providerId);
        if (providerIter != m_eventProviders.End())
        {
            m_eventProviders.Remove(providerIter);
            providerIter->value->SetEventServer(nullptr);
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

    Platform::LockGuard<Platform::Mutex> poolLock(m_eventPoolMutex);

    Result result = Result::Success;

    if (m_eventChunkPool.IsEmpty() == false)
    {
        EventChunk* pChunk;
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
            if (m_eventChunkAllocList.PushBack(pChunk))
            {
                *ppChunk = pChunk;
            }
            else
            {
                DD_FREE(pChunk, m_pMsgChannel->GetAllocCb());

                result = Result::InsufficientMemory;
            }
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

    Platform::LockGuard<Platform::Mutex> poolLock(m_eventPoolMutex);

    DD_UNHANDLED_RESULT(m_eventChunkPool.PushBack(pChunk) ? Result::Success : Result::InsufficientMemory);
}

Result EventServer::EnqueueEventChunks(size_t numChunks, EventChunk** ppChunks)
{
    DD_ASSERT(ppChunks != nullptr);

    Platform::LockGuard<Platform::Mutex> queueLock(m_eventQueueMutex);

    Result result = Result::Success;

    for (size_t chunkIndex = 0; chunkIndex < numChunks; ++chunkIndex)
    {
        EventChunkInfo chunkInfo = {};
        chunkInfo.pChunk         = ppChunks[chunkIndex];
        chunkInfo.bytesSent      = 0;

        result = (m_eventChunkQueue.PushBack(chunkInfo) ? Result::Success : Result::InsufficientMemory);

        if (result != Result::Success)
        {
            break;
        }
    }

    return result;
}

Result EventServer::BuildQueryProvidersResponse(BlockId* pBlockId)
{
    Result result = Result::InvalidParameter;

    if (pBlockId != nullptr)
    {
        auto pServerBlock = m_pMsgChannel->GetTransferManager().OpenServerBlock();
        if (pServerBlock.IsNull() == false)
        {
            Platform::LockGuard<Platform::Mutex> providersLock(m_eventProvidersMutex);

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

        Platform::LockGuard<Platform::Mutex> providersLock(m_eventProvidersMutex);

        const auto providerIter = m_eventProviders.Find(providerId);
        if (providerIter != m_eventProviders.End())
        {
            BaseEventProvider* pProvider = providerIter->value;
            if (pUpdate->isEnabled)
            {
                pProvider->Enable();
            }
            else
            {
                pProvider->Disable();
            }

            pProvider->UpdateEventData(VoidPtrInc(pUpdate, pUpdate->GetEventDataOffset()), pUpdate->GetEventDataSize());

            result = Result::Success;
        }
        else
        {
            result = Result::Error;
        }
    }

    return result;
}

} // EventProtocol

} // DevDriver
