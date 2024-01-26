/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <util/ddBitSet.h>
#include <util/vector.h>
#include <util/ddEventTimer.h>

#include <protocols/ddEventProtocol.h>

namespace DevDriver
{
namespace EventProtocol
{

class EventServer;
class EventServerSession;

class BaseEventProvider
{
    friend class EventServer;

public:
    uint32 GetNumEvents() const { return m_numEvents; }

    /// @deprecated EventData was used to represent the enablement of events in
    /// a provider. A provider's events can no longer be enabled individually,
    /// so this variable is meaningless now. Now enabling a provider enables
    /// all of its events.
    const void* GetEventData() const { return m_eventState.Data(); }

    /// @deprecated See `GetEventData`.
    size_t      GetEventDataSize() const { return m_eventState.SizeInBytes(); }

    bool IsProviderEnabled() const { return m_isEnabled; }
    bool IsProviderRegistered() const { return (m_pServer != nullptr); }
    bool IsSessionAcquired() const { return (m_pSession != nullptr); }

    virtual EventProviderId GetId() const = 0;

    virtual const char* GetName() const                     = 0;
    virtual const void* GetEventDescriptionData() const     = 0;
    virtual uint32      GetEventDescriptionDataSize() const = 0;

protected:
    BaseEventProvider(const AllocCb& allocCb, uint32 numEvents, uint32 flushFrequencyInMs);
    virtual ~BaseEventProvider();

    // Used to check if a particular event id is currently being accepted
    // This should be used in cases where the event write preparation logic for an event is expensive
    // since this allows us to avoid it when know the event write will be dropped anyways.
    // Returns Success if the write passes all filters and would have been successful
    // Returns Unavailable if the event provider is not currently registered to a server
    // Returns Rejected if the write would have been rejected due to event filtering settings
    Result QueryEventWriteStatus(uint32 eventId) const;

    // Like WriteEvent, but with an optional header blob that will be inserted before the event data in the payload
    // This is useful for cases when you would otherwise have to allocate an intermediate buffer to insert a header structure
    // before the main event data. (This function does not use an intermediate buffer internally)
    Result WriteEventWithHeader(uint32 eventId, const void* pHeaderData, size_t headerSize, const void* pEventData, size_t eventDataSize);

    // Attempts to write an event and its associated data into the provider's event stream
    // Returns the same results as QueryEventWriteStatus except for a few exceptions:
    // Returns InsufficientMemory if there's an internal memory allocation failure or we run out of event
    // chunk space.
    Result WriteEvent(uint32 eventId, const void* pEventData, size_t eventDataSize)
    {
        return WriteEventWithHeader(eventId, nullptr, 0, pEventData, eventDataSize);
    }

    // Returns the header associated with this provider
    ProviderDescriptionHeader GetHeader() const;

    // These notification function are intended to be overriden by derived classes to allow them to take action
    // when the event provider is enabled/disabled.
    virtual void OnEnable() {}
    virtual void OnDisable() {}

private:
    void Update();

    // This function must only be called while the chunk mutex is held!
    void UpdateFlushTimer();

    // This function must only be called while the chunk mutex is held!
    void Flush();

    void Enable()
    {
        if (m_isEnabled == false)
        {
            m_isEnabled = true;
            OnEnable();
        }
    }

    void Disable()
    {
        if (m_isEnabled)
        {
            // We want to flush any remaining queued events when disabling the provider.
            m_chunkMutex.Lock();
            Flush();
            m_chunkMutex.Unlock();

            m_isEnabled = false;
            OnDisable();
        }
    }

    Result AcquireEventChunks(size_t numBytesRequired, Vector<EventChunk*>* pChunks);

    void Register(EventServer* pServer);
    void Unregister();

    void AcquireSession(EventServerSession* pSession);
    EventServerSession* GetAcquiredSession();
    EventServerSession* ResetSession();

    Result AllocateEventChunk(EventChunk** ppChunk);
    void   FreeEventChunk(EventChunk* pChunk);
    Result BeginEventStream(EventChunk** ppChunk);
    Result WriteStreamPreamble(EventChunk* pChunk);

    // This function generates a small delta time value for use in other event tokens.
    // It requires a pointer to the chunk that's being written to because it may need to write
    // a separate timestamp token as a side effect of generating the small delta value.
    Result GenerateEventTimestamp(EventChunkBufferView* pBufferView, uint8* pSmallDelta);

    AllocCb              m_allocCb;
    EventServer*         m_pServer;
    EventServerSession*  m_pSession;
    uint32               m_numEvents;
    bool                 m_isEnabled;
    EventTimer           m_eventTimer;
    uint32               m_flushFrequencyInMs;
    uint32               m_eventDataIndex;
    Platform::AtomicLock m_chunkMutex;
    uint64               m_nextFlushTime;
    Vector<EventChunk*>  m_eventChunks;

    // Deprecated.
    DynamicBitSet<>      m_eventState;
};

} // namespace EventProtocol
} // namespace DevDriver
