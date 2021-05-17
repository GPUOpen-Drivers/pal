/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

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

class BaseEventProvider
{
    friend class EventServer;

public:
    uint32      GetNumEvents() const { return static_cast<uint32>(m_eventState.SizeInBits()); }
    const void* GetEventData() const { return m_eventState.Data(); }
    size_t      GetEventDataSize() const { return m_eventState.SizeInBytes(); }

    bool IsProviderEnabled() const { return m_isEnabled; }
    bool IsEventEnabled(uint32 eventId) const { return m_eventState[eventId]; }
    bool IsProviderRegistered() const { return (m_pServer != nullptr); }

    virtual EventProviderId GetId() const = 0;

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
    void EnableEvent(uint32 eventId) { m_eventState.SetBit(eventId); }
    void DisableEvent(uint32 eventId) { m_eventState.ResetBit(eventId); }

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

    void UpdateEventData(const void* pEventData, size_t eventDataSize)
    {
        m_eventState.UpdateBitData(pEventData, eventDataSize);
    }

    Result AcquireEventChunks(size_t numBytesRequired, Vector<EventChunk*>* pChunks);

    void Register(EventServer* pServer);
    void Unregister();

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
    DynamicBitSet<>      m_eventState;
    bool                 m_isEnabled;
    EventTimer           m_eventTimer;
    uint32               m_flushFrequencyInMs;
    uint32               m_eventDataIndex;
    Platform::AtomicLock m_chunkMutex;
    uint64               m_nextFlushTime;
    Vector<EventChunk*>  m_eventChunks;
};

} // namespace EventProtocol

} // namespace DevDriver
