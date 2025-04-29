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

#pragma once

#include <baseProtocolServer.h>
#include <util/hashMap.h>
#include <util/vector.h>
#include <util/queue.h>
#include <protocols/ddEventProtocol.h>

namespace DevDriver
{

namespace EventProtocol
{

class BaseEventProvider;
struct EventChunk;
class EventServerSession;

constexpr size_t kEventProviderMaxNameLen = 256;

struct EventProviderInfo
{
    EventProviderId id;
    char            name[kEventProviderMaxNameLen];
    bool            enabled;
    bool            registered;
};

class EventServer final : public BaseProtocolServer
{
    friend class BaseEventProvider;
    friend class EventServerSession;

public:
    using SessionMapIterator = HashMap<EventProviderId, BaseEventProvider*, 16u>::Iterator;

public:
    explicit EventServer(IMsgChannel* pMsgChannel);
    ~EventServer();

    bool AcceptSession(const SharedPointer<ISession>& pSession) override;
    void SessionEstablished(const SharedPointer<ISession>& pSession) override;
    void UpdateSession(const SharedPointer<ISession>& pSession) override;
    void SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason) override;

    Result RegisterProvider(BaseEventProvider* pProvider);
    Result UnregisterProvider(BaseEventProvider* pProvider);

private:
    struct PendingConnection
    {
        DevDriver::SharedPointer<DevDriver::ISession> pSession;
    };

    Result BuildQueryProvidersResponse(BlockId* pBlockId);
    Result ApplyProviderUpdate(const ProviderUpdateHeader* pUpdate);
    Result AssignSessionToProvider(EventServerSession* pEventSession, EventProviderId providerId);
    void   UnassignSessionFromProvider(EventServerSession* pEventSession, EventProviderId providerId);

    Vector<EventServerSession*, 16u>::Iterator FindPendingSessionById(SessionId id);
    SessionMapIterator FindProviderBySessionId(SessionId sessionId);

    HashMap<EventProviderId, BaseEventProvider*, 16u> m_eventProviders;
    Vector<EventServerSession*, 16u>                  m_pendingSessions;
    Platform::AtomicLock                              m_updateMutex;
};

} // EventProtocol
} // DevDriver
