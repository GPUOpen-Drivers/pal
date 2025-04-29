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

#include <protocols/ddEventProvider.h>
#include <protocols/ddEventServer.h>
#include <protocols/ddEventServerSession.h>

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
    : m_allocCb(allocCb)
    , m_pServer(nullptr)
    , m_pSession(nullptr)
    , m_numEvents(numEvents)
    , m_isEnabled(false)
    , m_flushFrequencyInMs(flushFrequencyInMs)
    , m_eventDataIndex(0)
    , m_nextFlushTime(0)
    , m_eventChunks(allocCb)
    , m_eventState(allocCb)
{
    DD_UNHANDLED_RESULT(m_eventState.Resize(numEvents));
}

BaseEventProvider::~BaseEventProvider()
{
}

Result BaseEventProvider::QueryEventWriteStatus(uint32 eventId) const
{
    DD_UNUSED(eventId);

    Result result = IsProviderRegistered() ? Result::Success
                                           : Result::Unavailable;

    if (result == Result::Success)
    {
        result = IsProviderEnabled() ? Result::Success : Result::Rejected;
    }

    return result;
}

Result BaseEventProvider::WriteEventWithHeader(
    uint32 eventId,
    const void* pHeaderData,
    size_t headerSize,
    const void* pEventData,
    size_t eventDataSize)
{
    Result result = QueryEventWriteStatus(eventId);

    if (result == Result::Success)
    {
        m_chunkMutex.Lock();

        const size_t totalEventSize = (headerSize + eventDataSize);

        const size_t requiredSize = CalculateWorstCaseSize(totalEventSize);

        // Attempt to allocate as many event chunks as we require from the server and write the data into them
        Vector<EventChunk*> chunks(m_allocCb);
        result = AcquireEventChunks(requiredSize, &chunks);

        if (result == Result::Success)
        {
            EventChunkBufferView bufferView(chunks.Data(), chunks.Size());

            // Write the timestamp and the data token into the first chunk unconditionally
            uint8 smallDelta = 0;
            if (result == Result::Success)
            {
                result = GenerateEventTimestamp(&bufferView, &smallDelta);
            }

            if (result == Result::Success)
            {
                result = bufferView.WriteEventDataToken(
                    smallDelta,
                    eventId,
                    m_eventDataIndex,
                    totalEventSize);
            }

            // Write the optional header into the collections of chunks if necessary
            if ((result == Result::Success) && (pHeaderData != nullptr))
            {
                result = bufferView.Write(pHeaderData, headerSize);
            }

            // Write the data payload into the collections of chunks as necessary
            if (result == Result::Success)
            {
                result = bufferView.Write(pEventData, eventDataSize);
            }
        }

        // Update the flush timer after each event write call to make sure events still get flushed
        // under heavy event writing pressure.
        if (result == Result::Success)
        {
            UpdateFlushTimer();
        }

        // Increment the event data index value every time we attempt to write a new event.
        // This value should be incremented even if we fail to write the event data to a chunk.
        ++m_eventDataIndex;

        m_chunkMutex.Unlock();

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
    uint8_t version = 1;
    return ProviderDescriptionHeader(GetId(),
                                     m_numEvents,
                                     static_cast<uint32>(GetEventDescriptionDataSize()),
                                     m_isEnabled,
                                     version);
}

void BaseEventProvider::Update()
{
    // Attempt to lock our chunk mutex so we can update the flush timer
    // Under heavy event logging pressure, we may be unable to do this, but that's fine because the event logging
    // path has built-in flush logic so the data will get flushed eventually by the thread who refuses to give up
    // the chunk lock.
    if (m_chunkMutex.TryLock())
    {
        UpdateFlushTimer();

        m_chunkMutex.Unlock();
    }
}

// This function must only be called while the chunk mutex is held!
void BaseEventProvider::UpdateFlushTimer()
{
    const uint64 currentTime = Platform::GetCurrentTimeInMs();

    if (m_flushFrequencyInMs > 0)
    {
        if (currentTime >= m_nextFlushTime)
        {
            m_nextFlushTime = currentTime + m_flushFrequencyInMs;

            Flush();
        }
    }
}

// This function must only be called while the chunk mutex is held!
void BaseEventProvider::Flush()
{
    if (m_eventChunks.IsEmpty() == false)
    {
        // Flush all chunks in our current stream into the event server's queue
        m_pSession->EnqueueEventChunks(m_eventChunks.Size(), m_eventChunks.Data());
        m_eventChunks.Reset();
    }
}

Result BaseEventProvider::AcquireEventChunks(size_t numBytesRequired, Vector<EventChunk*>* pChunks)
{
    // We should always start with a valid, empty vector.
    DD_ASSERT(pChunks != nullptr);
    DD_ASSERT(pChunks->IsEmpty());

    Vector<EventChunk*>& chunks = *pChunks;

    Result result = Result::Success;

    bool hasExistingData = false;
    EventChunk* pChunk = nullptr;

    // Acquire the current chunk
    // We may have to start a new stream if we have none in our internal buffer.
    if (m_eventChunks.IsEmpty() == false)
    {
        // We have existing chunks, attempt to acquire the most recently used chunk.
        pChunk = m_eventChunks[m_eventChunks.Size() - 1];
        hasExistingData = true;
    }
    else
    {
        // We have no existing chunks, begin a new stream
        result = BeginEventStream(&pChunk);
    }

    if (result == Result::Success)
    {
        result = BoolToResult(chunks.PushBack(pChunk));
    }

    if (result == Result::Success)
    {
        // If the current chunk doesn't have enough space, then we need to allocate additional chunks.
        // Keep allocating chunks until we acquire enough bytes
        size_t bytesAllocated = pChunk->CalculateBytesRemaining();
        while (bytesAllocated < numBytesRequired)
        {
            result = AllocateEventChunk(&pChunk);
            if (result == Result::Success)
            {
                result = BoolToResult(chunks.PushBack(pChunk));
                if (result == Result::Success)
                {
                    bytesAllocated += pChunk->CalculateBytesRemaining();
                }
                else
                {
                    // Free the event chunk if we fail to add it to our list
                    FreeEventChunk(pChunk);
                }
            }

            // Break out of the loop if we encounter errors
            if (result != Result::Success)
            {
                // Free all the chunks we allocated if we fail.
                // In some cases, the first chunk may contained unrelated event data so we need to ensure that we don't
                // free it in those cases.
                const size_t firstAllocatedChunkIndex = (hasExistingData ? 1 : 0);
                for (size_t chunkIndex = firstAllocatedChunkIndex; chunkIndex < chunks.Size(); ++chunkIndex)
                {
                    FreeEventChunk(chunks[chunkIndex]);
                }
                chunks.Clear();

                break;
            }
        }
    }

    return result;
}

void BaseEventProvider::Register(EventServer* pServer)
{
    // Register should only be called on a provider that's currently unregistered
    DD_ASSERT(m_pServer == nullptr);

    m_pServer = pServer;
}

void BaseEventProvider::AcquireSession(EventServerSession* pSession)
{
    DD_ASSERT(m_pSession == nullptr);
    m_pSession = pSession;
}

EventServerSession* BaseEventProvider::GetAcquiredSession()
{
    return m_pSession;
}

EventServerSession* BaseEventProvider::ResetSession()
{
    EventServerSession* pOldEventSession = m_pSession;
    m_pSession = nullptr;
    return pOldEventSession;
}

void BaseEventProvider::Unregister()
{
    // Flush any remaining chunks before the provider is unregistered
    m_chunkMutex.Lock();
    Flush();
    m_chunkMutex.Unlock();

    m_pServer = nullptr;
}

Result BaseEventProvider::AllocateEventChunk(EventChunk** ppChunk)
{
    EventChunk* pChunk = nullptr;
    Result result = m_pSession->AllocateEventChunk(&pChunk);
    if (result == Result::Success)
    {
        if (m_eventChunks.PushBack(pChunk) == false)
        {
            result = Result::InsufficientMemory;
            m_pSession->FreeEventChunk(pChunk);
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
    m_pSession->FreeEventChunk(pChunk);
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
    EventChunkBufferView bufferView(&pChunk);
    return bufferView.WriteEventProviderToken(GetId(), timestamp.full.frequency, timestamp.full.timestamp);
}

Result BaseEventProvider::GenerateEventTimestamp(EventChunkBufferView* pBufferView, uint8* pSmallDelta)
{
    DD_ASSERT(pBufferView != nullptr);
    DD_ASSERT(pSmallDelta != nullptr);

    Result result = Result::Success;

    const EventTimestamp timestamp = m_eventTimer.CreateTimestamp();

    uint8 smallDelta = 0;

    if (timestamp.type == EventTimestampType::Full)
    {
        // Write a full timestamp token
        result = pBufferView->WriteEventTimestampToken(timestamp.full.frequency, timestamp.full.timestamp);
    }
    else if (timestamp.type == EventTimestampType::LargeDelta)
    {
        result = pBufferView->WriteEventTimeDeltaToken(
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
