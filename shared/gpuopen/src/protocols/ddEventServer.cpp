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
* @file  ddEventServer.cpp
* @brief Implementation for EventServer
***********************************************************************************************************************
*/

#include <protocols/ddEventServer.h>
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
{
    DD_ASSERT(m_pMsgChannel != nullptr);
}

bool EventServer::AcceptSession(const SharedPointer<ISession>& pSession)
{
    DD_UNUSED(pSession);

    return (m_pActiveSession == nullptr);
}

void EventServer::SessionEstablished(const SharedPointer<ISession>& pSession)
{
    Platform::LockGuard<Platform::Mutex> sessionLock(m_activeSessionMutex);

    // Allocate session data for the newly established session
    EventServerSession* pEventSession = DD_NEW(EventServerSession, m_pMsgChannel->GetAllocCb())(m_pMsgChannel->GetAllocCb(), pSession, this, &m_pMsgChannel->GetTransferManager());
    pSession->SetUserData(pEventSession);

    m_pActiveSession = pEventSession;
}

void EventServer::UpdateSession(const SharedPointer<ISession>& pSession)
{
    EventServerSession* pEventSession = reinterpret_cast<EventServerSession*>(pSession->GetUserData());
    DD_ASSERT(pEventSession == m_pActiveSession);

    pEventSession->UpdateSession();
}

void EventServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
{
    Platform::LockGuard<Platform::Mutex> sessionLock(m_activeSessionMutex);

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

Result EventServer::RegisterProvider(IEventProvider* pProvider)
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

Result EventServer::UnregisterProvider(IEventProvider* pProvider)
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

Result EventServer::LogEvent(const void* pEventData, size_t eventDataSize)
{
    DD_PRINT(LogLevel::Verbose, "Logging event with size %zu", eventDataSize);

    Result result = Result::Unavailable;

    Platform::LockGuard<Platform::Mutex> sessionLock(m_activeSessionMutex);
    if (m_pActiveSession != nullptr)
    {
        result = m_pActiveSession->SendEventData(pEventData, eventDataSize);
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
                const IEventProvider* pProvider = providerPair.value;

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
            IEventProvider* pProvider = providerIter->value;
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
