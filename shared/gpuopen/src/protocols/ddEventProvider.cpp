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
* @file  ddEventProvider.cpp
* @brief Implementation for BaseEventProvider
***********************************************************************************************************************
*/

#include <protocols/ddEventProvider.h>
#include <protocols/ddEventServer.h>

namespace DevDriver
{

namespace EventProtocol
{

size_t CalculateWorstCaseSize(size_t eventDataSize)
{
    // Event payload storage
    size_t bytesRequired = eventDataSize;

    // Largest timestamp token that might be required
    bytesRequired += sizeof(EventTokenHeader) + sizeof(EventTimestampToken);

    // Header token for the event data payload
    bytesRequired += sizeof(EventTokenHeader) + sizeof(EventDataToken);

    return bytesRequired;
}

BaseEventProvider::BaseEventProvider(const AllocCb& allocCb, uint32 numEvents, uint32 flushFrequencyInMs)
    : m_eventState(allocCb)
    , m_flushFrequencyInMs(flushFrequencyInMs)
    , m_nextFlushTime(0)
    , m_eventChunks(allocCb)
{
    DD_UNHANDLED_RESULT(m_eventState.Resize(numEvents));
}

BaseEventProvider::~BaseEventProvider()
{
}

void BaseEventProvider::Flush()
{
    Platform::LockGuard<Platform::AtomicLock> chunkLock(m_chunkMutex);

    if (m_eventChunks.IsEmpty() == false)
    {
        // Flush all chunks in our current stream into the event server's queue
        m_pServer->EnqueueEventChunks(m_eventChunks.Size(), m_eventChunks.Data());
        m_eventChunks.Reset();
    }
}

Result BaseEventProvider::QueryEventWriteStatus(uint32 eventId) const
{
    Result result = IsProviderRegistered() ? Result::Success
                                           : Result::Unavailable;

    if (result == Result::Success)
    {
        result = (IsProviderEnabled() && IsEventEnabled(eventId)) ? Result::Success
                                                                  : Result::Rejected;
    }

    return result;
}

Result BaseEventProvider::WriteEvent(uint32 eventId, const void* pEventData, size_t eventDataSize)
{
    Result result = QueryEventWriteStatus(eventId);

    if (result == Result::Success)
    {
        const size_t requiredSize = CalculateWorstCaseSize(eventDataSize);
        if (requiredSize <= kEventChunkMaxDataSize)
        {
            Platform::LockGuard<Platform::AtomicLock> chunkLock(m_chunkMutex);

            // Attempt to allocate a chunk from the server and write the data into it
            EventChunk* pChunk = nullptr;
            result = AcquireEventChunk(requiredSize, &pChunk);
            if (result == Result::Success)
            {
                uint8 smallDelta = 0;
                result = GenerateEventTimestamp(pChunk, &smallDelta);

                if (result == Result::Success)
                {
                    result = pChunk->WriteEventDataToken(
                        smallDelta,
                        eventId,
                        pEventData,
                        eventDataSize);
                }
            }
        }
        else
        {
            // @TODO: Support large event payloads by linking together multiple chunks
            result = Result::InsufficientMemory;
        }

        if (result != Result::Success)
        {
            DD_PRINT(LogLevel::Warn,
                "Provider 0x%x failed with result \"%s\" when attempting to write event with id %u and size %u!",
                GetId(),
                ResultToString(result),
                eventId,
                static_cast<uint32>(eventDataSize));
        }
    }

    return result;
}

ProviderDescriptionHeader BaseEventProvider::GetHeader() const
{
    return ProviderDescriptionHeader(GetId(),
                                     static_cast<uint32>(m_eventState.SizeInBits()),
                                     static_cast<uint32>(GetEventDescriptionDataSize()),
                                     m_isEnabled);
}

void BaseEventProvider::Update(uint64 currentTime)
{
    if (m_flushFrequencyInMs > 0)
    {
        if (currentTime >= m_nextFlushTime)
        {
            m_nextFlushTime = currentTime + m_flushFrequencyInMs;

            Flush();
        }
    }
}

Result BaseEventProvider::AcquireEventChunk(size_t numBytesRequired, EventChunk** ppChunk)
{
    Result result = Result::Success;

    EventChunk* pChunk = nullptr;

    // Acquire the current chunk
    // We may have to start a new stream if we have none in our internal buffer.
    if (m_eventChunks.IsEmpty() == false)
    {
        pChunk = m_eventChunks[m_eventChunks.Size() - 1];
    }
    else
    {
        result = BeginEventStream(&pChunk);
    }

    if (result == Result::Success)
    {
        const size_t bytesRemaining = (sizeof(pChunk->data) - pChunk->dataSize);
        if (bytesRemaining < numBytesRequired)
        {
            // Make sure the caller isn't asking for too much space.
            // This should already be handled by the calling code, but we assert here again just in case.
            DD_ASSERT(numBytesRequired <= sizeof(pChunk->data));

            // Allocate a new chunk if this one can't hold all of our data.
            result = AllocateEventChunk(&pChunk);
        }
    }

    if (result == Result::Success)
    {
        *ppChunk = pChunk;
    }

    return result;
}

Result BaseEventProvider::AllocateEventChunk(EventChunk** ppChunk)
{
    EventChunk* pChunk = nullptr;
    Result result = m_pServer->AllocateEventChunk(&pChunk);
    if (result == Result::Success)
    {
        if (m_eventChunks.PushBack(pChunk) == false)
        {
            result = Result::InsufficientMemory;
            m_pServer->FreeEventChunk(pChunk);
        }
        else
        {
            *ppChunk = pChunk;
        }
    }

    return result;
}

void BaseEventProvider::FreeEventChunk(EventChunk* pChunk)
{
    m_eventChunks.Remove(pChunk);
    m_pServer->FreeEventChunk(pChunk);
}

Result BaseEventProvider::BeginEventStream(EventChunk** ppChunk)
{
    // We should always have an empty chunk list if a new stream is being started
    DD_ASSERT(m_eventChunks.IsEmpty());

    EventChunk* pChunk = nullptr;
    Result result = AllocateEventChunk(&pChunk);
    if (result == Result::Success)
    {
        result = WriteStreamPreamble(pChunk);

        if (result != Result::Success)
        {
            FreeEventChunk(pChunk);
            pChunk = nullptr;
        }
    }

    if (result == Result::Success)
    {
        *ppChunk = pChunk;
    }

    return result;
}

Result BaseEventProvider::WriteStreamPreamble(EventChunk* pChunk)
{
    // Write the stream preamble data
    // This only needs to be included once per provider event stream

    // Reset the timer since we're starting a new stream and generate a timestamp.
    m_eventTimer.Reset();
    const EventTimestamp timestamp = m_eventTimer.CreateTimestamp();

    // We should always get a full timestamp since we just reset the event timer above.
    DD_ASSERT(timestamp.type == EventTimestampType::Full);

    // Write the provider token
    return pChunk->WriteEventProviderToken(GetId(), timestamp.full.frequency, timestamp.full.timestamp);
}

Result BaseEventProvider::GenerateEventTimestamp(EventChunk* pChunk, uint8* pSmallDelta)
{
    DD_ASSERT(pChunk      != nullptr);
    DD_ASSERT(pSmallDelta != nullptr);

    Result result = Result::Success;

    const EventTimestamp timestamp = m_eventTimer.CreateTimestamp();

    uint8 smallDelta = 0;

    if (timestamp.type == EventTimestampType::Full)
    {
        // Write a full timestamp token
        result = pChunk->WriteEventTimestampToken(timestamp.full.frequency, timestamp.full.timestamp);
    }
    else if (timestamp.type == EventTimestampType::LargeDelta)
    {
        result = pChunk->WriteEventTimeDeltaToken(
            timestamp.largeDelta.numBytes,
            timestamp.largeDelta.delta);
    }
    else if (timestamp.type == EventTimestampType::SmallDelta)
    {
        smallDelta = timestamp.smallDelta.delta;
    }
    else
    {
        DD_ASSERT_REASON("Invalid timestamp type!");
    }

    if (result == Result::Success)
    {
        *pSmallDelta = smallDelta;
    }

    return result;
}

} // EventProtocol

} // DevDriver
