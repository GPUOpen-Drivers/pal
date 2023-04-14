/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

static constexpr size_t kMaxSessionNum = 4;

EventServer::EventServer(IMsgChannel* pMsgChannel)
    : BaseProtocolServer(pMsgChannel, Protocol::Event, EVENT_SERVER_MIN_VERSION, EVENT_SERVER_MAX_VERSION)
    , m_eventProviders(pMsgChannel->GetAllocCb())
    , m_pendingSessions(pMsgChannel->GetAllocCb())
{
    DD_ASSERT(m_pMsgChannel != nullptr);

    m_pendingSessions.Reserve(kMaxSessionNum);
}

EventServer::~EventServer()
{
    // delete any remaining `EventServerSession`s.
    for (auto sessionIter : m_pendingSessions)
    {
        DD_DELETE(sessionIter, m_pMsgChannel->GetAllocCb());
    }

    // All providers should be unregistered before the event server is destroyed.
    // If this is not the case, event chunks may leak because they're still owned by the providers
    // and now they can't be returned to the event server!
    DD_ASSERT(m_eventProviders.IsEmpty());
}

bool EventServer::AcceptSession(const SharedPointer<ISession>& pSession)
{
    DD_UNUSED(pSession);

    bool acceptable = false;

    Platform::LockGuard<Platform::AtomicLock> providersLock(m_updateMutex);

    // Only accept fixed amount (`kMaxSessionNum`) of connections.
    if (m_pendingSessions.Size() < m_pendingSessions.Capacity())
    {
        for (auto providerIter : m_eventProviders)
        {
            // If there is a provider who hasn't acquired a session.
            if (providerIter.value->IsSessionAcquired() == false)
            {
                acceptable = true;
                DD_PRINT(LogLevel::Verbose, "[DevDriver][EventServer] accepted a session (%u).", pSession->GetSessionId());
                break;
            }
        }
    }

    return acceptable;
}

void EventServer::SessionEstablished(const SharedPointer<ISession>& pSession)
{
    Platform::LockGuard<Platform::AtomicLock> providersLock(m_updateMutex);

    // Allocate session data for the newly established session
    EventServerSession* pEventSession = DD_NEW(EventServerSession, m_pMsgChannel->GetAllocCb())(
        m_pMsgChannel->GetAllocCb(),
        pSession,
        this,
        &m_pMsgChannel->GetTransferManager());

    pSession->SetUserData(pEventSession);

    m_pendingSessions.PushBack(pEventSession);
}

void EventServer::UpdateSession(const SharedPointer<ISession>& pSession)
{
    Platform::LockGuard<Platform::AtomicLock> providersLock(m_updateMutex);

    SessionMapIterator providerIter = FindProviderBySessionId(pSession->GetSessionId());
    if (providerIter != m_eventProviders.End())
    {
        // If this session has already been acquired by a provider, update the
        // provider and the session.

        providerIter->value->Update();
        providerIter->value->GetAcquiredSession()->UpdateSession();
    }
    else
    {
        // If this session hasn't been acquired by any provider, it's either in
        // the pending list, or its original provider unregistered.

        auto sessionIter = FindPendingSessionById(pSession->GetSessionId());
        if (sessionIter != m_pendingSessions.End())
        {
            (*sessionIter)->UpdateSession();
        }
        else
        {
            // The requested provider unregistered itself.
            SubscribeToProviderResponse resp(Result::Unavailable);
            Result result = pSession->Send(sizeof(resp), &resp, kNoWait);
            DD_UNHANDLED_RESULT(result);
        }
    }
}

void EventServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
{
    DD_UNUSED(terminationReason);

    bool eventSessionDeleted = false;

    Platform::LockGuard<Platform::AtomicLock> providersLock(m_updateMutex);

    // Find the session by id.
    auto sessionIter = FindPendingSessionById(pSession->GetSessionId());
    if (sessionIter != m_pendingSessions.End())
    {
        // Delete the EventServerSession object referencing this `pSession`.
        DD_DELETE(*sessionIter, m_pMsgChannel->GetAllocCb());
        m_pendingSessions.Remove(sessionIter);
        eventSessionDeleted = true;
    }

#if defined(DD_OPT_ASSERTS_ENABLE)
    // If the session is in the pending list, it couldn't have been acquired by
    // a provider.
    if (eventSessionDeleted)
    {
        DD_ASSERT(FindProviderBySessionId(pSession->GetSessionId()) == m_eventProviders.End());
    }
#endif

    if (eventSessionDeleted == false)
    {
        SessionMapIterator providerIter = FindProviderBySessionId(pSession->GetSessionId());
        if (providerIter != m_eventProviders.End())
        {
            // Disable the provider who acquired this session.
            providerIter->value->Disable();

            EventServerSession* pEventSession = providerIter->value->GetAcquiredSession();
            // Delete the EventServerSession object referencing this `pSession`.
            DD_DELETE(pEventSession, m_pMsgChannel->GetAllocCb());

            providerIter->value->ResetSession();
        }
    }
}

Result EventServer::RegisterProvider(BaseEventProvider* pProvider)
{
    Result result = Result::InvalidParameter;

    if (pProvider != nullptr)
    {
        const EventProviderId providerId = pProvider->GetId();

        Platform::LockGuard<Platform::AtomicLock> providersLock(m_updateMutex);

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

        Platform::LockGuard<Platform::AtomicLock> providersLock(m_updateMutex);

        const auto providerIter = m_eventProviders.Find(providerId);
        if (providerIter != m_eventProviders.End())
        {
            providerIter->value->Unregister();

            EventServerSession* pEventSession = providerIter->value->ResetSession();
            if (pEventSession)
            {
                DD_DELETE(pEventSession, m_pMsgChannel->GetAllocCb());
            }

            m_eventProviders.Remove(providerIter);

            result = Result::Success;
        }
        else
        {
            result = Result::Error;
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
            // Don't need to lock here, because this function is called inside
            // `EventServer::UpdateSession()`;

            // Write the response header
            const QueryProvidersResponseHeader responseHeader(static_cast<uint32>(m_eventProviders.Size()));
            pServerBlock->Write(&responseHeader, sizeof(responseHeader));

            for (const auto& providerPair : m_eventProviders)
            {
                const BaseEventProvider* pProvider = providerPair.value;

                // Write the provider header
                const ProviderDescriptionHeader providerHeader = pProvider->GetHeader();
                pServerBlock->Write(&providerHeader, sizeof(providerHeader));

                // For backwards compatibility, we still need to send EventData
                // because `ProviderDescriptionHeader` exposes APIs that
                // implicitly assume there is always EventData following
                // immediately.
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

        // Don't need to lock here, because this function is called inside
        // `EventServer::UpdateSession()`;

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
        }
        else
        {
            result = Result::Error;
        }
    }

    return result;
}

Result EventServer::AssignSessionToProvider(EventServerSession* pEventSession, EventProviderId providerId)
{
    Result result = Result::Success;

    // Don't need to lock here, because this function is called inside
    // `EventServer::UpdateSession()`;

    auto requestedProviderIter = m_eventProviders.Find(providerId);
    if (requestedProviderIter != m_eventProviders.End())
    {
        if (requestedProviderIter->value->GetAcquiredSession() == nullptr)
        {
            requestedProviderIter->value->AcquireSession(pEventSession);
            requestedProviderIter->value->Enable();
            pEventSession->SetProviderId(providerId);

            auto sessionIter = FindPendingSessionById(pEventSession->GetSessionId());
            DD_ASSERT(sessionIter != m_pendingSessions.End());
            m_pendingSessions.Remove(sessionIter);

            DD_PRINT(
                LogLevel::Info,
                "[DevDriver][EventServer] Provider (%s) acquired session: %u",
                requestedProviderIter->value->GetName(),
                pEventSession->GetSessionId());
        }
        else
        {
            result = Result::Unavailable;
            DD_PRINT(
                LogLevel::Error,
                "[DevDriver][EventServer] The requested provider (%s) has already acquired a session.\n",
                requestedProviderIter->value->GetName());
        }
    }
    else
    {
        // No registered provider matches the requested provider id.
        result = Result::Unavailable;
    }

    return result;
}

void EventServer::UnassignSessionFromProvider(EventServerSession* pEventSession, EventProviderId providerId)
{
    DD_UNUSED(pEventSession);

    auto providerIter = m_eventProviders.Find(providerId);
    DD_ASSERT(providerIter != m_eventProviders.End());
    DD_ASSERT(pEventSession == providerIter->value->GetAcquiredSession());

    providerIter->value->Disable();
    providerIter->value->ResetSession();

    DD_PRINT(
        LogLevel::Info,
        "[DevDriver][EventServer] Unassign session (%u) from the event provider (%s).",
        pEventSession->GetSessionId(),
        providerIter->value->GetName());
}

Vector<EventServerSession*, 16u>::Iterator EventServer::FindPendingSessionById(SessionId id)
{
    auto sessionIter = m_pendingSessions.Begin();
    for (; sessionIter != m_pendingSessions.End(); ++sessionIter)
    {
        if ((*sessionIter)->GetSessionId() == id)
        {
            break;
        }
    }
    return sessionIter;
}

EventServer::SessionMapIterator EventServer::FindProviderBySessionId(SessionId sessionId)
{
    auto providerIter = m_eventProviders.Begin();
    for (; providerIter != m_eventProviders.End(); ++providerIter)
    {
        EventServerSession* pEventSession = providerIter->value->GetAcquiredSession();
        if (pEventSession != nullptr && pEventSession->GetSessionId() == sessionId)
        {
            break;
        }
    }
    return providerIter;
}

} // EventProtocol
} // DevDriver
