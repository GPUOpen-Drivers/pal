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
* @file  ddEventServer.h
* @brief Header for EventServer
***********************************************************************************************************************
*/

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
