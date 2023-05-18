/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <stdint.h>
#include <string.h>

constexpr uint32_t DDEventMetaVersionMajor = 0;
constexpr uint32_t DDEventMetaVersionMinor = 1;

/// The meta version dictates data definitions in this file. This version data is
/// __always__ at the very beginning of a dd-event data stream.
///
///
/// ```c++
///
/// DDEventMetaVersion metaVersion{};
/// fread(&metaVersion, 1, sizeof(metaVersion), eventFileHandle);
/// if (metaVersion.major == DDEventMetaVersionMajor) {
///     // continue parsing event stream
///     DDEventProviderHeader providerHeader{};
///     fread(&providerHeader, 1, sizeof(providerHeader), eventFileHandle);
/// } else if (metaVersion.major == 1) {
///     // Let's say a breaking change was made to the version 1 of DDEventProviderHeader,
///     // it is necessary that DDEventProviderHeader was renamed to DDEventProviderHeader_v1,
///     // and is present in this file.
///     DDEventProviderHeader_v1 providerHeader_v1{};
///     fread(&providerHeader_v1, 1, sizeof(providerHeader_v1), eventFileHandle);
/// } else {
///     log("Unable to parse dd-devent stream with meta version %u", (uint32_t)metaVersion.major);
///     return -1;
/// }
/// ```
struct DDEventMetaVersion
{
    uint16_t major;
    uint16_t minor;
};
static_assert(sizeof(DDEventMetaVersion) == 4, "DDEventMetaVersion has incorrect size");

/// The header for an event provider. This header immediately follows `DDEventMetaVersion`
/// in a dd-event data stream.
struct DDEventProviderHeader
{
    /// Major version number of the event provider, indicating the events data format.
    uint16_t versionMajor;

    /// Minor version number of the event provider, indicating the events data format.
    uint16_t versionMinor;

    /// reserved
    uint32_t reserved;

    /// Number uniquely identifying an event provider.
    uint32_t providerId;

    /// Time unit indicates the precision of timestamp delta. A timestamp delta
    /// is always a multiple of `timeUnit`. To calculate timestamp:
    /// `currentTimestamp = lastTimestamp + delta * timeUnit`.
    uint32_t timeUnit;

    /// First timestamp counter before any other events. Used to calibrate the
    /// timing of all subquent events.
    uint64_t baseTimestamp;

    /// The frequency of counter, in counts per second. To convert the
    /// difference of two timestamps to duration in seconds:
    /// `seconds = (timestamp2 - timestamp1) / baseTimestampFrequency;`.
    uint64_t baseTimestampFrequency;
};
static_assert(sizeof(DDEventProviderHeader) == 32, "DDEventProviderHeader has incorrect size");

/// Every event from all event providers is prefixed by a `DDEventHeader`
/// object which describes the type and the size of the event. To parse an
/// event, developers are expected to first read `sizeof(DDEventHeader)` bytes
/// before the actual event payload.
///
///
/// ```c++
///
/// DDEventHeader header = {};
/// fread(&header, 1, sizeof(header), dataFileHandle);
///
/// if (header.eventId == DDCommonEventId::TimestampLargeDelta) {
///     // do something
/// } else {
///     switch ((Foo::EvenId)header.eventId) {
///         case Foo::EventId::MySpecialEvent:
///             // read the actual event payload based on `header.eventSize`
///             uint8_t tempBuf[kBigEnoughSize] = {};
///             fread(tempBuf, 1, header.eventSize, dataFileHandle);
///
///             // convert `tempBuf` to the actual event
///             MySpecialEvent event;
///             event.FromBuffer(tempBuf);
///
///             // do something with `event`
///             break;
///         case ... {
///             // other event types
///             break;
///         }
///     }
/// }
/// ```
struct DDEventHeader
{
    /// Id for event type.
    uint8_t eventId;

    /// Time delta since the last timing calibration.
    uint8_t smallDelta;

    /// The size of the actual event immediately following this header object,
    /// not including this header.
    uint16_t eventSize;
};
static_assert(sizeof(DDEventHeader) == 4, "DDEventHeader has incorrect size");

/// Ids for events that are common for all event providers.
enum DDCommonEventId : uint8_t
{
    TimestampLargeDelta = 0,

    /// Individual provider's event id starts at this value.
    FirstEventIdForIndividualProvider = 16,
};

namespace DDCommonEvents
{

/// A separate event representing a timestamp delta since the last timing
/// calibration. This event is emitted if the delta value cannot fit in
/// `DDEventHeader::smallDelta`. The value is subject to
struct TimestampLargeDelta
{
    /// The maximum size required for a buffer to hold the packed data of this event.
    static const uint32_t PackedBufferSize = 8;

    uint64_t delta;

    void FromBuffer(const uint8_t* buffer)
    {
        memcpy(&delta, buffer, sizeof(delta));
    }

    uint32_t ToBuffer(uint8_t* buffer)
    {
        memcpy(buffer, &delta, sizeof(delta));

        return sizeof(delta);
    }
};

} // namespace DDCommonEvents
