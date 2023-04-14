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

#include <dd_event.h>

#include <stdint.h>
#include <string.h>

namespace UmdCrashAnalysisEvents
{

constexpr uint32_t VersionMajor = 0;
constexpr uint32_t VersionMinor = 2;
constexpr uint32_t ProviderId   = 0x50434145;

/// A marker that matches this value indicates the associated command buffer hasn't started.
constexpr uint32_t InitialExecutionMarkerValue = 0xFFFFAAAA;

/// A marker that matches this value indicates the associated command buffer has completed.
constexpr uint32_t FinalExecutionMarkerValue   = 0xFFFFBBBB;

/// Unique id represeting each event. Each variable name of the enum value corresponds to the
/// struct with the same name.
enum class EventId : uint8_t
{
    ExecutionMarkerTop    = DDCommonEventId::FirstEventIdForIndividualProvider + 0,
    ExecutionMarkerBottom = DDCommonEventId::FirstEventIdForIndividualProvider + 1,
    CrashDebugMarkerValue = DDCommonEventId::FirstEventIdForIndividualProvider + 2,
    CmdBufferReset        = DDCommonEventId::FirstEventIdForIndividualProvider + 3
};

/// The source from which execution markers were inserted.
enum class ExecutionMarkerSource : uint8_t
{
    Application = 0,
    Api         = 1,
    Pal         = 2,
    Hardware    = 3
};

/// Execution marker inserted at the top of pipe.
struct ExecutionMarkerTop
{
    /// An integer uniquely identifying a command buffer.
    uint32_t cmdBufferId;

    /// Execution marker value. The first 4 most significant bits represent the
    /// source from which the marker was inserted, and should be one of the
    /// values of `ExecutionMarkerSource`. The last 28 bits represent a
    /// timestamp counter.
    uint32_t marker;

    /// The length of `markerName`.
    uint16_t markerNameSize;

    /// A user-defined name for the marker, encoded in UTF-8. Note, this string is not
    /// necessarily null-terminated.
    uint8_t markerName[150];

    void FromBuffer(const uint8_t* buffer)
    {
        memcpy(&cmdBufferId, buffer, sizeof(cmdBufferId));
        buffer += sizeof(cmdBufferId);

        memcpy(&marker, buffer, sizeof(marker));
        buffer += sizeof(marker);

        memcpy(&markerNameSize, buffer, sizeof(markerNameSize));
        buffer += sizeof(markerNameSize);

        memcpy(markerName, buffer, markerNameSize);
    }

    /// Fill the pre-allocated `buffer` with the data in this struct. The size of
    /// the buffer has to be at least `sizeof(ExecutionMarkerTop)` big.
    ///
    /// Return the actual amount of bytes copied into `buffer`.
    uint32_t ToBuffer(uint8_t* buffer) const
    {
        memcpy(buffer, &cmdBufferId, sizeof(cmdBufferId));
        buffer += sizeof(cmdBufferId);

        memcpy(buffer, &marker, sizeof(marker));
        buffer += sizeof(marker);

        memcpy(buffer, &markerNameSize, sizeof(markerNameSize));
        buffer += sizeof(markerNameSize);

        memcpy(buffer, markerName, markerNameSize);

        return sizeof(cmdBufferId) + sizeof(marker) + sizeof(markerNameSize) + markerNameSize;
    }
};

/// Execution marker inserted at the bottom of pipe.
struct ExecutionMarkerBottom
{
    /// An integer uniquely identifying a command buffer.
    uint32_t cmdBufferId;

    /// Execution marker value. The first 4 most significant bits represent the
    /// source from which the marker was inserted, and should be one of the
    /// values of `ExecutionMarkerSource`. The last 28 bits represent a counter
    /// value.
    uint32_t marker;

    void FromBuffer(const uint8_t* buffer)
    {
        memcpy(&cmdBufferId, buffer, sizeof(cmdBufferId));
        buffer += sizeof(cmdBufferId);

        memcpy(&marker, buffer, sizeof(marker));
    }

    /// Fill the pre-allocated `buffer` with the data of this struct. The size of
    /// the buffer has to be at least `sizeof(ExecutionMarkerBottom)` big.
    ///
    /// Return the actual amount of bytes copied into `buffer`.
    uint32_t ToBuffer(uint8_t* buffer) const
    {
        memcpy(buffer, &cmdBufferId, sizeof(cmdBufferId));
        buffer += sizeof(cmdBufferId);

        memcpy(buffer, &marker, sizeof(marker));

        return sizeof(cmdBufferId) + sizeof(marker);
    }
};

/// This struct helps identify commands that may have caused crashes.
struct CrashDebugMarkerValue
{
    /// The id of the commond buffer that may have caused the crash.
    uint32_t cmdBufferId;

    /// The marker value that helps identify which commands have started
    /// execution. Should be equal to one of `ExecutionMarkerTop::marker`s.
    uint32_t topMarkerValue;

    /// The marker value that helps identify which commands' executiion have
    /// ended. Should be equal to one of `ExecutionMarkerBottom::marker`s.
    uint32_t bottomMarkerValue;

    void FromBuffer(const uint8_t* buffer)
    {
        memcpy(&cmdBufferId, buffer, sizeof(cmdBufferId));
        buffer += sizeof(cmdBufferId);

        memcpy(&topMarkerValue, buffer, sizeof(topMarkerValue));
        buffer += sizeof(topMarkerValue);

        memcpy(&bottomMarkerValue, buffer, sizeof(bottomMarkerValue));
    }

    /// Fill the pre-allocated `buffer` with the data of this struct. The size of
    /// the buffer has to be at least `sizeof(CrashDebugMarkerValue)` big.
    ///
    /// Return the actual amount of bytes copied into `buffer`.
    uint32_t ToBuffer(uint8_t* buffer)
    {
        memcpy(buffer, &cmdBufferId, sizeof(cmdBufferId));
        buffer += sizeof(cmdBufferId);

        memcpy(buffer, &topMarkerValue, sizeof(topMarkerValue));
        buffer += sizeof(topMarkerValue);

        memcpy(buffer, &bottomMarkerValue, sizeof(bottomMarkerValue));

        return sizeof(cmdBufferId) + sizeof(topMarkerValue) + sizeof(bottomMarkerValue);
    }
};

/// A command buffer has been reset to an initial state.
struct CmdBufferReset
{
    /// An integer uniquely identifying a command buffer.
    uint32_t cmdBufferId;

    /// Deserializes a buffer into this event object
    void FromBuffer(const uint8_t* buffer)
    {
        memcpy(&cmdBufferId, buffer, sizeof(cmdBufferId));
    }

    /// Fill the pre-allocated `buffer` with the data of this struct. The size of
    /// the buffer has to be at least `sizeof(CmdBufferReset)` big.
    ///
    /// Return the actual amount of bytes copied into `buffer`.
    uint32_t ToBuffer(uint8_t* buffer) const
    {
        memcpy(buffer, &cmdBufferId, sizeof(cmdBufferId));
        return sizeof(cmdBufferId);
    }
};

} // namespace UmdCrashAnalysisEvents
