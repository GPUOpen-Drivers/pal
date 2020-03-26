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
* @file  ddEventProvider.h
* @brief Header for BaseEventProvider
***********************************************************************************************************************
*/

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

    void Flush();

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

    // Attempts to write an event and its associated data into the provider's event stream
    // Returns the same results as QueryEventWriteStatus except for a few exceptions:
    // Returns InsufficientMemory if there's an internal memory allocation failure or we run out of event
    // chunk space.
    Result WriteEvent(uint32 eventId, const void* pEventData, size_t eventDataSize);

    // Returns the header associated with this provider
    ProviderDescriptionHeader GetHeader() const;

    // These notification function are intended to be overriden by derived classes to allow them to take action
    // when the event provider is enabled/disabled.
    virtual void OnEnable() {}
    virtual void OnDisable() {}

private:
    void EnableEvent(uint32 eventId) { m_eventState.SetBit(eventId); }
    void DisableEvent(uint32 eventId) { m_eventState.ResetBit(eventId); }

    void Update(uint64 currentTime);

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
            Flush();
            m_isEnabled = false;
            OnDisable();
        }
    }

    void UpdateEventData(const void* pEventData, size_t eventDataSize)
    {
        m_eventState.UpdateBitData(pEventData, eventDataSize);
    }

    Result AcquireEventChunk(size_t numBytesRequired, EventChunk** ppChunk);

    void SetEventServer(EventServer* pServer) { m_pServer = pServer; }

    Result AllocateEventChunk(EventChunk** ppChunk);
    void   FreeEventChunk(EventChunk* pChunk);
    Result BeginEventStream(EventChunk** ppChunk);
    Result WriteStreamPreamble(EventChunk* pChunk);

    // This function generates a small delta time value for use in other event tokens.
    // It requires a pointer to the chunk that's being written to because it may need to write
    // a separate timestamp token as a side effect of generating the small delta value.
    Result GenerateEventTimestamp(EventChunk* pChunk, uint8* pSmallDelta);

    EventServer*         m_pServer = nullptr;
    DynamicBitSet<>      m_eventState;
    bool                 m_isEnabled = false;
    EventTimer           m_eventTimer;
    uint32               m_flushFrequencyInMs;
    Platform::AtomicLock m_chunkMutex;
    uint64               m_nextFlushTime;
    Vector<EventChunk*>  m_eventChunks;
};

} // namespace EventProtocol

} // namespace DevDriver
