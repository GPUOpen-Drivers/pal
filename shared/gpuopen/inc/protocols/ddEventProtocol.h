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
* @file  ddEventProtocol.h
* @brief Common interface for Event Protocol.
***********************************************************************************************************************
*/

#pragma once

#include <gpuopen.h>
#include <protocols/ddTransferProtocol.h>

/*
***********************************************************************************************************************
* Event Protocol
***********************************************************************************************************************
*/

#define EVENT_PROTOCOL_VERSION 1

#define EVENT_PROTOCOL_MINIMUM_VERSION 1

/*
***********************************************************************************************************************
*| Version | Change Description                                                                                       |
*| ------- | ---------------------------------------------------------------------------------------------------------|
*|  1.0    | Initial version                                                                                          |
***********************************************************************************************************************
*/

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
            uint8  padding[3];

            ProviderDescriptionHeader(uint32 providerId, uint32 numEvents, uint32 eventDescriptionDataSize, bool isEnabled)
                : providerId(providerId)
                , numEvents(numEvents)
                , eventDescriptionDataSize(eventDescriptionDataSize)
                , isEnabled(isEnabled)
            {
            }

            size_t GetEventDataOffset() const { return sizeof(ProviderDescriptionHeader); }
            size_t GetEventDataSize() const { return ((Platform::Pow2Align<size_t>(numEvents, 32) >> 5) * sizeof(uint32)); }
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
            uint8  padding[3];

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
    }
}
