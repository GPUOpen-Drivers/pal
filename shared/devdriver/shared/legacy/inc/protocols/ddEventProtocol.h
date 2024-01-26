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

#include <gpuopen.h>
#include <protocols/ddTransferProtocol.h>

/*
***********************************************************************************************************************
* Event Protocol
***********************************************************************************************************************
*/

#define EVENT_PROTOCOL_VERSION 2

#define EVENT_PROTOCOL_MINIMUM_VERSION 2

/*
***********************************************************************************************************************
*| Version | Change Description                                                                                       |
*| ------- | ---------------------------------------------------------------------------------------------------------|
*|  2.0    | Switched to 64bit payload size and added event indices                                                   |
*|  1.0    | Initial version                                                                                          |
***********************************************************************************************************************
*/

#define EVENT_INDEXING_VERSION 2
#define EVENT_INITIAL_VERSION 1

namespace DevDriver
{
    namespace EventProtocol
    {
        using BlockId = TransferProtocol::BlockId;

        ///////////////////////
        // Event Protocol
        enum struct EventMessage : MessageCode
        {
            Unknown = 0,

            // Returns an array of the currently registered event providers, their event description data, and their
            // provider and event enablement status.
            QueryProvidersRequest,
            QueryProvidersResponse,

            // Allocates a push block for the client to write new provider states into
            AllocateProviderUpdatesRequest,
            AllocateProviderUpdatesResponse,

            // Updates the provider states using a previously allocated provider states block
            ApplyProviderUpdatesRequest,
            ApplyProviderUpdatesResponse,

            // Returns new event data from the server
            EventDataUpdate,

            // EventClient requests to subscribe to a provider.
            SubscribeToProviderRequest,
            SubscribeToProviderResponse,

            // EventClient requests to unsubscribe from its previously subscribed provider.
            UnsubscribeFromProviderRequest,

            Count
        };

        typedef uint32 EventProviderId;

        DD_NETWORK_STRUCT(QueryProvidersResponseHeader, 4)
        {
            uint32 numProviders;

            QueryProvidersResponseHeader(uint32 numProviders)
                : numProviders(numProviders)
            {
            }
        };

        DD_CHECK_SIZE(QueryProvidersResponseHeader, 4);

        DD_NETWORK_STRUCT(ProviderDescriptionHeader, 4)
        {
            uint32 providerId;
            uint32 numEvents;
            uint32 eventDescriptionDataSize;
            bool   isEnabled;
            // This is a hack. The original ddEventProtocol didn't have a way
            // to communicate to EventClient the version of EventServer. Here
            // we re-purpose the first uint8 padding to be the version data.
            // Luckily all paddings are zeroed, so for old EventServer,
            // this field is 0.
            uint8  version = 0;
            uint8  padding[2] = {};

            ProviderDescriptionHeader(
                uint32 providerId,
                uint32 numEvents,
                uint32 eventDescriptionDataSize,
                bool isEnabled,
                uint8_t version)
                : providerId(providerId)
                , numEvents(numEvents)
                , eventDescriptionDataSize(eventDescriptionDataSize)
                , isEnabled(isEnabled)
                , version(version)
            {
            }

            size_t GetEventDataOffset() const { return sizeof(ProviderDescriptionHeader); }
            size_t GetEventDataSize() const { return ((Platform::Pow2Align<size_t>(numEvents, 32) / 32) * sizeof(uint32)); }
            size_t GetEventDescriptionOffset() const
            {
                return (GetEventDataOffset() + GetEventDataSize());
            }
            size_t GetNextProviderDescriptionOffset() const
            {
                return (GetEventDescriptionOffset() + eventDescriptionDataSize);
            }
        };

        DD_CHECK_SIZE(ProviderDescriptionHeader, 16);

        DD_NETWORK_STRUCT(ProviderUpdateHeader, 4)
        {
            uint32 providerId;
            uint32 eventDataSize;
            bool   isEnabled;
            uint8  padding[3] = {};

            ProviderUpdateHeader(uint32 providerId, uint32 eventDataSize, bool isEnabled)
                : providerId(providerId)
                , eventDataSize(eventDataSize)
                , isEnabled(isEnabled)
            {
            }

            size_t GetEventDataOffset() const { return sizeof(ProviderUpdateHeader); }
            size_t GetEventDataSize() const { return eventDataSize; }
            size_t GetNextProviderUpdateOffset() const
            {
                return (GetEventDataOffset() + eventDataSize);
            }
        };

        DD_CHECK_SIZE(ProviderUpdateHeader, 12);

        ///////////////////////
        // Event Types
        DD_NETWORK_STRUCT(EventHeader, 4)
        {
            EventMessage  command;
            uint8         padding;

            // We use two of the padding bytes in the header to store the event data size
            // when working with the EventDataUpdate payload.
            uint16        eventDataSize;

            constexpr EventHeader(EventMessage command)
                : command(command)
                , padding(0)
                , eventDataSize(0)
            {
            }
        };

        DD_CHECK_SIZE(EventHeader, 4);

        // We need to reserve at least 4 bytes of data for the event header when sending event data
        constexpr size_t kMaxEventDataSize = (kMaxPayloadSizeInBytes - sizeof(EventHeader));

        DD_NETWORK_STRUCT(QueryProvidersRequestPayload, 4)
        {
            EventHeader        header;

            QueryProvidersRequestPayload()
                : header(EventMessage::QueryProvidersRequest)
            {
            }
        };

        DD_CHECK_SIZE(QueryProvidersRequestPayload, 4);

        DD_NETWORK_STRUCT(QueryProvidersResponsePayload, 4)
        {
            EventHeader        header;
            Result             result;
            BlockId            blockId;

            QueryProvidersResponsePayload(Result  result,
                                          BlockId block)
                : header(EventMessage::QueryProvidersResponse)
                , result(result)
                , blockId(block)
            {
            }
        };

        DD_CHECK_SIZE(QueryProvidersResponsePayload, 12);

        DD_NETWORK_STRUCT(AllocateProviderUpdatesRequest, 4)
        {
            EventHeader        header;
            uint32             dataSize;

            AllocateProviderUpdatesRequest(uint32 dataSize)
                : header(EventMessage::AllocateProviderUpdatesRequest)
                , dataSize(dataSize)
            {
            }
        };

        DD_CHECK_SIZE(AllocateProviderUpdatesRequest, 8);

        DD_NETWORK_STRUCT(AllocateProviderUpdatesResponse, 4)
        {
            EventHeader        header;
            Result             result;
            BlockId            blockId;

            AllocateProviderUpdatesResponse(Result result,
                                           BlockId block)
                : header(EventMessage::AllocateProviderUpdatesResponse)
                , result(result)
                , blockId(block)
            {
            }
        };

        DD_CHECK_SIZE(AllocateProviderUpdatesResponse, 12);

        DD_NETWORK_STRUCT(ApplyProviderUpdatesRequest, 4)
        {
            EventHeader header;

            ApplyProviderUpdatesRequest()
                : header(EventMessage::ApplyProviderUpdatesRequest)
            {
            }
        };

        DD_CHECK_SIZE(ApplyProviderUpdatesRequest, 4);

        DD_NETWORK_STRUCT(ApplyProviderUpdatesResponse, 4)
        {
            EventHeader header;
            Result      result;

            ApplyProviderUpdatesResponse(Result result)
                : header(EventMessage::ApplyProviderUpdatesResponse)
                , result(result)
            {
            }
        };

        DD_CHECK_SIZE(ApplyProviderUpdatesResponse, 8);

        DD_NETWORK_STRUCT(EventDataUpdatePayload, 4)
        {
            EventHeader header;
            uint8       eventData[kMaxEventDataSize];

            EventDataUpdatePayload(const void* pEventData, size_t eventDataSize)
                : header(EventMessage::EventDataUpdate)
            {
                DD_ASSERT(eventDataSize <= kMaxEventDataSize);

                const size_t clampedEventDataSize = Platform::Min(eventDataSize, kMaxEventDataSize);
                memcpy(eventData, pEventData, clampedEventDataSize);

                DD_ASSERT(clampedEventDataSize <= 0xffff);
                header.eventDataSize = static_cast<uint16>(clampedEventDataSize);
            }

            void* GetEventDataBuffer()
            {
                return eventData;
            }

            const void* GetEventDataBuffer() const
            {
                return eventData;
            }

            size_t GetEventDataBufferSize() const
            {
                return sizeof(eventData);
            }

            size_t GetEventDataSize() const
            {
                return static_cast<size_t>(header.eventDataSize);
            }

            void SetEventDataSize(uint16 eventDataSize)
            {
                header.eventDataSize = eventDataSize;
            }
        };

        DD_CHECK_SIZE(EventDataUpdatePayload, kMaxEventDataSize + sizeof(EventHeader));

        DD_NETWORK_STRUCT(SubscribeToProviderRequest, 4)
        {
            EventHeader header;
            uint32 providerId;

            SubscribeToProviderRequest(EventProviderId id)
                : header(EventMessage::SubscribeToProviderRequest)
                , providerId(id)
            {}

        };
        DD_CHECK_SIZE(SubscribeToProviderRequest, 8);

        DD_NETWORK_STRUCT(SubscribeToProviderResponse, 4)
        {
            EventHeader header;
            Result      result;

            SubscribeToProviderResponse(Result result)
                : header(EventMessage::SubscribeToProviderResponse)
                , result(result)
            {}
        };
        DD_CHECK_SIZE(SubscribeToProviderResponse, 8);

        DD_NETWORK_STRUCT(UnsubscribeFromProviderRequest, 4)
        {
            EventHeader header;

            UnsubscribeFromProviderRequest()
                : header(EventMessage::UnsubscribeFromProviderRequest)
            {}

        };
        DD_CHECK_SIZE(UnsubscribeFromProviderRequest, 4);

        enum class EventTokenType : uint8
        {
            Provider  = 0,
            Data      = 1,
            Timestamp = 2,
            TimeDelta = 3,

            Count
        };

        // We have to be able to fit the token type in the first 4 bits of an event header
        static_assert(static_cast<uint8>(EventTokenType::Count) < 16, "Event token type no longer fits in 4 bits!");

        struct EventTokenHeader
        {
            struct
            {
                uint8 id    : 4;
                uint8 delta : 4;
            };
        };

        DD_CHECK_SIZE(EventTokenHeader, 1);

        // @TODO: This struct has some extra padding that could be removed
        // Token used to mark the beginning of a new event stream from an event provider
        struct EventProviderToken
        {
            EventProviderId id;        // Identifier for the event provider
            uint32          padding;   // Padding bytes
            uint64          frequency; // Frequency of "timestamp"
            uint64          timestamp; // Timestamp associated with the start of the event stream
        };

        DD_CHECK_SIZE(EventProviderToken, 24);

        // Token used to wrap event data for the event specified by "id"
        struct EventDataToken
        {
            uint32 id;    /// Event identifier
            uint32 index; /// Event data index
                          /// This value is generated by the associated event provider. It is incremented every time
                          /// the provider attempts to write a new event into the stream. If the provider fails to
                          /// write the event due to memory conditions, this value will still be incremented even
                          /// though the event associated with it will never be seen. This allows readers of the event
                          /// stream to use this value to detect gaps between events that appear to be contiguous.
            uint64 size;  /// Size in bytes of the event data that follows this token
        };

        DD_CHECK_SIZE(EventDataToken, 16);

        // Token that contains complete timestamp information, including the frequency
        struct EventTimestampToken
        {
            uint64 frequency;
            uint64 timestamp;
        };

        DD_CHECK_SIZE(EventTimestampToken, 16);

        // Token that contains a variable size delta from the last timestamp value in the stream
        struct EventTimeDeltaToken
        {
            uint8 numBytes; // Number of bytes used to encode the time delta (Maximum of 6)
        };

        DD_CHECK_SIZE(EventTimeDeltaToken, 1);

        // Maximum number of bytes required by a single event token
        DD_STATIC_CONST size_t kMaxEventTokenSize =
            sizeof(EventTokenHeader) +
            Platform::Max(sizeof(EventProviderToken),
                Platform::Max(sizeof(EventDataToken),
                    Platform::Max(sizeof(EventTimestampToken), sizeof(EventTimeDeltaToken) + 6)));

        // Maximum number of bytes contained within an event chunk
        // We subtract the data size metadata here to make sure the total struct size lands on
        // a nice power of two. This should help us avoid extra memory overhead per chunk allocation.
        // This is checked with a static_assert after the EventChunk definition.
        DD_STATIC_CONST size_t kEventChunkMaxDataSize = ((64 * 1024) - sizeof(uint32));

        struct EventChunk
        {
            uint32 dataSize;
            uint8  data[kEventChunkMaxDataSize];

            // Returns the number of available bytes remaining in the chunk
            size_t CalculateBytesRemaining() const { return (sizeof(data) - dataSize); }

            // Returns true if the chunk has no data in it
            bool IsEmpty() const { return (dataSize == 0); }

            // Returns true if the chunk is completely filled with data
            bool IsFull() const { return (dataSize == sizeof(data)); }

            // Writes the provided event data into the event chunk
            // Returns InsufficientMemory if the data won't fit
            Result Write(const void* pEventData, size_t eventDataSize)
            {
                Result result = Result::Success;

                if (eventDataSize <= CalculateBytesRemaining())
                {
                    memcpy(data + dataSize, pEventData, eventDataSize);
                    dataSize += static_cast<uint32>(eventDataSize);
                }
                else
                {
                    result = Result::InsufficientMemory;
                }

                return result;
            }
        };

        // Utility class that abstracts the logic required to write data across multiple event chunks
        class EventChunkBufferView
        {
        public:
            EventChunkBufferView(EventChunk** ppChunkList, size_t numChunks)
                : m_ppChunkList(ppChunkList)
                , m_numChunks(numChunks)
                , m_currentChunkIndex(0)
            {
            }

            EventChunkBufferView(EventChunk** ppChunk)
                : m_ppChunkList(ppChunk)
                , m_numChunks(1)
                , m_currentChunkIndex(0)
            {
            }

            // Writes data into the buffer and automatically strides over the event chunks as necessary.
            Result Write(const void* pData, size_t dataSize)
            {
                Result result = Result::Success;

                size_t bytesWritten = 0;

                while (result == Result::Success)
                {
                    // If we've filled the current chunk, we need to move on to the next one
                    if (m_ppChunkList[m_currentChunkIndex]->IsFull())
                    {
                        if ((m_currentChunkIndex + 1) < m_numChunks)
                        {
                            // We have another chunk, increment our index to start using it.
                            m_currentChunkIndex++;
                        }
                        else
                        {
                            // We've run out of chunks. Return an out of memory error.
                            result = Result::InsufficientMemory;
                        }
                    }

                    if (result == Result::Success)
                    {
                        EventChunk* pChunk = m_ppChunkList[m_currentChunkIndex];

                        const void* pDataPtr = VoidPtrInc(pData, bytesWritten);

                        const size_t bytesRemaining = (dataSize - bytesWritten);
                        const size_t bytesToWrite = Platform::Min(pChunk->CalculateBytesRemaining(), bytesRemaining);

                        result = pChunk->Write(pDataPtr, bytesToWrite);

                        if (result == Result::Success)
                        {
                            bytesWritten += bytesToWrite;

                            // Break out of the loop if we've finished writing all of our data
                            if (bytesWritten == dataSize)
                            {
                                break;
                            }
                        }
                    }
                }

                return result;
            }

            // Writes the provided event provider token information into the event chunk
            // Returns InsufficientMemory if the data won't fit
            Result WriteEventProviderToken(
                EventProviderId providerId,
                uint64          frequency,
                uint64          timestamp)
            {
                EventTokenHeader header = {};
                header.id               = static_cast<uint8>(EventTokenType::Provider);
                header.delta            = 0;

                Result result = Write(&header, sizeof(header));

                if (result == Result::Success)
                {
                    EventProviderToken token = {};
                    token.id             = providerId;
                    token.frequency      = frequency;
                    token.timestamp      = timestamp;

                    result = Write(&token, sizeof(token));
                }

                return result;
            }

            // Writes the provided event data token information into the event chunk
            // Returns InsufficientMemory if we don't have enough space remaining
            Result WriteEventDataToken(
                uint8  delta,
                uint32 eventId,
                uint32 index,
                size_t eventDataSize)
            {
                EventTokenHeader header = {};
                header.id               = static_cast<uint8>(EventTokenType::Data);
                header.delta            = delta;

                Result result = Write(&header, sizeof(header));

                if (result == Result::Success)
                {
                    EventDataToken token = {};
                    token.id             = eventId;
                    token.index          = index;
                    token.size           = eventDataSize;

                    result = Write(&token, sizeof(token));
                }

                return result;
            }

            // Writes a timestamp token into the event chunk
            // Returns InsufficientMemory if the data won't fit
            Result WriteEventTimestampToken(
                uint64 frequency,
                uint64 timestamp)
            {
                EventTokenHeader header = {};
                header.id               = static_cast<uint8>(EventTokenType::Timestamp);
                header.delta            = 0;

                Result result = Write(&header, sizeof(header));

                if (result == Result::Success)
                {
                    EventTimestampToken token = {};
                    token.frequency           = frequency;
                    token.timestamp           = timestamp;

                    result = Write(&token, sizeof(token));
                }

                return result;
            }

            // Writes a time delta token into the event chunk
            // Returns InsufficientMemory if the data won't fit
            Result WriteEventTimeDeltaToken(
                uint8  numBytes,
                uint64 timeDelta)
            {
                DD_ASSERT(numBytes > 0);

                EventTokenHeader header = {};
                header.id               = static_cast<uint8>(EventTokenType::TimeDelta);
                header.delta            = 0;

                Result result = Write(&header, sizeof(header));

                if (result == Result::Success)
                {
                    EventTimeDeltaToken token = {};
                    token.numBytes            = numBytes;

                    result = Write(&token, sizeof(token));
                }

                if (result == Result::Success)
                {
                    result = Write(&timeDelta, numBytes);
                }

                return result;
            }

        private:
            EventChunk** m_ppChunkList;
            size_t       m_numChunks;
            size_t       m_currentChunkIndex;
        };

        static_assert(Platform::IsPowerOfTwo(sizeof(EventChunk)), "EventChunk should be a power of two to avoid extra memory overhead per chunk allocation.");
    }
}
