/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "../common.h"

#include <stdint.h>
#include <string.h>

namespace UmdCrashAnalysisEvents
{

constexpr uint32_t VersionMajor = 0;
constexpr uint32_t VersionMinor = 3;
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
    ExecutionMarkerInfo   = DDCommonEventId::FirstEventIdForIndividualProvider + 3
};

/// The source from which execution markers were inserted.
enum class ExecutionMarkerSource : uint8_t
{
    Application = 0,        // Marker issued from the application
    Api         = 1,        // Marker issued from client driver (DX12/Vulkan/...)
    Pal         = 2,        // Marker issued from PAL
    Hardware    = 3,        // Marker issued from Hardware

    CmdBufInfo  = 250,      // Driver internal use, provide info for CmdBuffer
    OpInfo      = 251,      // Driver internal use, provide info for an CmdBuffer event
    SqttEvent   = 252       // Driver internal use, provide SqttEvent type for an CmdBuffer event
};

enum class ExecutionMarkerInfoType : uint8_t
{
    Invalid      = 0,       // Indicate an invalid MarkerInfoType
    CmdBufStart  = 1,       // Indicate that the header precedes a CmdBufferInfo struct
    PipelineBind = 2,       // Indicate that the header precedes a PipelineInfo struct
    Draw         = 3,       // Indicate that the header precedes a DrawInfo struct
    DrawUserData = 4,       // Indicate that the header precedes a DrawUserData struct
    Dispatch     = 5,       // Indicate that the header precedes a DispatchInfo struct
    BarrierBegin = 6,       // Indicate that the header precedes a BarrierBeginInfo struct
    BarrierEnd   = 7        // Indicate that the header precedes a BarrierEndInfo struct
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

    /// Fill the pre-allocated `buffer` with the data in this struct. The size of
    /// the buffer has to be at least `sizeof(ExecutionMarkerTop)` big.
    ///
    /// Return the actual amount of bytes copied into `buffer`.
    uint32_t ToBuffer(uint8_t* buffer) const
    {
        uint32_t copySize = 0;

        memcpy(buffer + copySize, &cmdBufferId, sizeof(cmdBufferId));
        copySize += sizeof(cmdBufferId);

        memcpy(buffer + copySize, &marker, sizeof(marker));
        copySize += sizeof(marker);

        memcpy(buffer + copySize, &markerNameSize, sizeof(markerNameSize));
        copySize += sizeof(markerNameSize);

        memcpy(buffer + copySize, markerName, markerNameSize);
        copySize += markerNameSize;

        return copySize;
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

    /// Fill the pre-allocated `buffer` with the data of this struct. The size of
    /// the buffer has to be at least `sizeof(ExecutionMarkerBottom)` big.
    ///
    /// Return the actual amount of bytes copied into `buffer`.
    uint32_t ToBuffer(uint8_t* buffer) const
    {
        uint32_t copySize = 0;

        memcpy(buffer + copySize, &cmdBufferId, sizeof(cmdBufferId));
        copySize += sizeof(cmdBufferId);

        memcpy(buffer + copySize, &marker, sizeof(marker));
        copySize += sizeof(marker);

        return copySize;
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

    /// Fill the pre-allocated `buffer` with the data of this struct. The size of
    /// the buffer has to be at least `sizeof(CrashDebugMarkerValue)` big.
    ///
    /// Return the actual amount of bytes copied into `buffer`.
    uint32_t ToBuffer(uint8_t* buffer)
    {
        uint32_t copySize = 0;

        memcpy(buffer + copySize, &cmdBufferId, sizeof(cmdBufferId));
        copySize += sizeof(cmdBufferId);

        memcpy(buffer + copySize, &topMarkerValue, sizeof(topMarkerValue));
        copySize += sizeof(topMarkerValue);

        memcpy(buffer + copySize, &bottomMarkerValue, sizeof(bottomMarkerValue));
        copySize += sizeof(bottomMarkerValue);

        return copySize;
    }
};

/// Execution marker that provide additional information
//
//  The most typical use of the event is to describe an already existing ExecutionMarkerTop event.
//  Take 'Draw' as an example, here is what the tool can expect to see
//
//  => ExecutionMarkerTop({marker=0x10000003, makerName="Draw"}
//  => ExecutionMarkerInfo({
//          marker=0x10000003,
//          markerInfo={ExecutionMarkerHeader({typeInfo=Draw}) + DrawInfo({drawType=...})
//  => ExecutionMarkerBottom({marker=0x10000003})
//
//  A couple of things to note
//  1. ExecutionMarkerInfo have the same markerValue as the ExecutionMarkerTop that it is describing.
//  2. ExecutionMarkerInfo is only used inside driver so ExecutionMarkerTop(Application)+ExecutionMarkerInfo
//     is not a possible combination. Currently, tool can expect to see back-to-back Top->Info->Bottom if Info
//     is available. However, this may not be true when we generate timestamps for all internal calls in the future.
//
//  There are situations where ExecutionMarkerTop and ExecutionMarkerInfo does not have 1-to-1 relations.
//  1. When using ExecutionMarkerInfo to provide additional info for a CmdBuffer, there will be timestamp but
//     no ExecutionMarkerTop/ExecutionMarkerBottom events. In this case, ExecutionMarkerInfo.marker is set to
//     0xFFFFAAAA (InitialExecutionMarkerValue).
//  2. There will be an ExecutionMarkerInfo for PipelineBind but not timestamp generated for that because binding
//     a pipeline does not cause any GPU work. Therefore no timestamp is needed.
//  3. Barrier operation will have one timestamp generated but 2 different ExecutionMarkerInfo generated (BarrierBegin
//     and BarrierEnd). Expect MarkerTop + MarkerInfo(BarrierBegin) + MarkerInfo(BarrierEnd) + MarkerBottom in this case.
//
struct ExecutionMarkerInfo
{
    // Unique identifier of the relevant command buffer
    uint32_t cmdBufferId;

    /// Execution marker value (see comment in ExecutionMarkerTop). The ExecutionMarkerInfo generally describes an
    /// existing ExecutionMarkerTop and the marker is how ExecutionMarkerInfo relates to an ExecutionMarkerTop.
    uint32_t marker;

    /// The length of `markerInfo`.
    uint16_t markerInfoSize;

    /// Used as a buffer to host additonal structural data. It should starts with ExecutionMarkerInfoHeader followed
    /// by a data structure that ExecutionMarkerInfoHeader.infoType dictates. All the structure are tightly packed
    /// (no paddings).
    uint8_t markerInfo[64];

    void FromBuffer(const uint8_t* buffer)
    {
        memcpy(&cmdBufferId, buffer, sizeof(cmdBufferId));
        buffer += sizeof(cmdBufferId);

        memcpy(&marker, buffer, sizeof(marker));
        buffer += sizeof(marker);

        memcpy(&markerInfoSize, buffer, sizeof(markerInfoSize));
        buffer += sizeof(markerInfoSize);

        memcpy(markerInfo, buffer, markerInfoSize);
    }

    /// Fill the pre-allocated `buffer` with the data in this struct.
    /// Return the actual amount of bytes copied into `buffer`.
    uint32_t ToBuffer(uint8_t* buffer) const
    {
        uint32_t copySize = 0;

        memcpy(buffer + copySize, &cmdBufferId, sizeof(cmdBufferId));
        copySize += sizeof(cmdBufferId);

        memcpy(buffer + copySize, &marker, sizeof(marker));
        copySize += sizeof(marker);

        memcpy(buffer + copySize, &markerInfoSize, sizeof(markerInfoSize));
        copySize += sizeof(markerInfoSize);

        memcpy(buffer + copySize, markerInfo, markerInfoSize);
        copySize += markerInfoSize;

        return copySize;
    }
};

#pragma pack(push, 1)

// Header information on how to interpret the info struct
struct ExecutionMarkerInfoHeader
{
    ExecutionMarkerInfoType infoType;
};

/// CmdBufferInfo follows header with ExecutionMarkerInfoType::CmdBufStart
struct CmdBufferInfo
{
    uint8_t     queue;          // Api-specific queue family index
    uint64_t    deviceId;       // Device handle in D3D12 & Vulkan
    uint32_t    queueFlags;     // 0 in D3D12. VkQueueFlagBits in Vulkan
};

/// PipelineInfo follows header with ExecutionMarkerInfoType::PipelineBind
struct PipelineInfo
{
    uint32_t    bindPoint;      // Pal::PipelineBindPoint
    uint64_t    apiPsoHash;     // Api Pipeline hash
};

/// DrawUserData follows header with ExecutionMarkerInfoType::DrawUserData
struct DrawUserData
{
    uint32_t     vertexOffset;   // Vertex offset (first vertex) user data register index
    uint32_t     instanceOffset; // Instance offset (start instance) user data register index
    uint32_t     drawId;         // Draw ID SPI user data register index
};

/// DrawInfo follows header with ExecutionMarkerInfoType::Draw
struct DrawInfo
{
    uint32_t     drawType;
    uint32_t     vtxIdxCount;    // Vertex/Index count
    uint32_t     instanceCount;  // Instance count
    uint32_t     startIndex;     // Start index
    DrawUserData userData;
};

/// DispatchInfo follows header with ExecutionMarkerInfoType::Dispatch
struct DispatchInfo
{
    uint32_t    dispatchType;   // Api specific. RgpSqttMarkerApiType(DXCP) or RgpSqttMarkerEventType(Vulkan)
    uint32_t    threadX;        // Number of thread groups in X dimension
    uint32_t    threadY;        // Number of thread groups in Y dimension
    uint32_t    threadZ;        // Number of thread groups in Z dimension
};

/// BarrierBeginInfo follows header with ExecutionMarkerInfoType::BarrierBegin
struct BarrierBeginInfo
{
    uint32_t    type;       // Pal::Developer::BarrierType
    uint32_t    reason;     // Pal::Developer::BarrierReason
};

/// BarrierEndInfo follows header with ExecutionMarkerInfoType::BarrierEnd
struct BarrierEndInfo
{
    uint16_t    pipelineStalls;     // Pal::Developer::BarrierOperations.pipelineStalls
    uint16_t    layoutTransitions;  // Pal::Developer::BarrierOperations.layoutTransitions
    uint16_t    caches;             // Pal::Developer::BarrierOperations.caches
};

#pragma pack(pop)

} // namespace UmdCrashAnalysisEvents
