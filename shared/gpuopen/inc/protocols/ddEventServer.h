/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

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

class EventServer final : public BaseProtocolServer
{
    friend class BaseEventProvider;
    friend class EventServerSession;

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
    Result AllocateEventChunk(EventChunk** ppChunk);
    void FreeEventChunk(EventChunk* pChunk);
    void EnqueueEventChunks(size_t numChunks, EventChunk** ppChunks);
    EventChunk* DequeueEventChunk();
    Result BuildQueryProvidersResponse(BlockId* pBlockId);
    Result ApplyProviderUpdate(const ProviderUpdateHeader* pUpdate);
    bool IsTargetMemoryUsageExceeded() const;
    void TrimEventChunkMemory();

    HashMap<EventProviderId, BaseEventProvider*, 16u> m_eventProviders;
    Platform::AtomicLock                              m_eventProvidersMutex;
    Platform::AtomicLock                              m_eventPoolMutex;
    Vector<EventChunk*>                               m_eventChunkPool;
    Platform::AtomicLock                              m_eventQueueMutex;
    Vector<EventChunk*>                               m_eventChunkQueue;
    EventServerSession*                               m_pActiveSession;
    uint64                                            m_nextTrimTime;
};

} // EventProtocol

} // DevDriver
