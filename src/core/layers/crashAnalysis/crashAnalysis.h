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

#include "pal.h"
#include "palPlatform.h"
#include "palVector.h"
#include "palStringBag.h"

#include <dd_events/gpu_detective/umd_crash_analysis.h>

namespace Pal
{
namespace CrashAnalysis
{

/// The default value of an initialized Crash Analysis marker
constexpr uint32 InitialMarkerValue = UmdCrashAnalysisEvents::InitialExecutionMarkerValue;

/// The final value of a Crash Analysis marker
constexpr uint32 FinalMarkerValue   = UmdCrashAnalysisEvents::FinalExecutionMarkerValue;

using MarkerSource                  = UmdCrashAnalysisEvents::ExecutionMarkerSource;

static_assert(static_cast<uint32>(MarkerSource::Application) == 0, "");
static_assert(static_cast<uint32>(MarkerSource::Api)         == 1, "");
static_assert(static_cast<uint32>(MarkerSource::Pal)         == 2, "");

constexpr uint32 MarkerStackCount = 0x10; // Corresponds to 4 bits of unique source IDs

/// Structure inserted into a GPU memory region to track CmdBuffer progression
/// and state.
struct MarkerState
{
    uint32 cmdBufferId; ///< Unique ID representing a command buffer
    uint32 markerBegin; ///< Top-of-pipe marker execution counter
    uint32 markerEnd;   ///< Bottom-of-pipe marker execution counter
};

// If each member doesn't start at a DWORD offset this won't work.
static_assert(offsetof(MarkerState, cmdBufferId) == 0,                  "");
static_assert(offsetof(MarkerState, markerBegin) == sizeof(uint32),     "");
static_assert(offsetof(MarkerState, markerEnd)   == sizeof(uint32) * 2, "");

// Basic reference counting structure
class RefCounter
{
public:
    explicit RefCounter(IPlatform* pPlatform);

    virtual void Destroy() = 0;

    void TakeReference();
    void ReleaseReference();

    IPlatform* GetAllocator() const { return m_pPlatform; }

    RefCounter() = delete;

    IPlatform*      m_pPlatform; // Allocator for this object
    volatile uint32 m_refCount; // Reference counter for memory lifetime
};

class Device; // Forward declaration: used in 'MemoryChunk' below

// A structure managing a 'MarkerState' allocation, mapping, and lifetime
class MemoryChunk : public RefCounter
{
public:
    gpusize         gpuVirtAddr; // GPU address of embedded 'MarkerState'
    MarkerState*    pCpuAddr;    // System memory address of embedded 'MarkerState'
    uint32          raftIndex;   // Index of the memory raft owner
    Device*         pDevice;     // Device that owns this memory chunk

    MemoryChunk(
        Device* pDevice);

    // Called when the last reference to this object is released
    virtual void Destroy() override;
};

class EventCache : public RefCounter
{
public:
    EventCache(
        IPlatform* pPlatform);

    Result CacheExecutionMarkerBegin(
        uint32      cmdBufferId,
        uint32      markerValue,
        const char* pMarkerName,
        uint32      markerNameSize);

    Result CacheExecutionMarkerEnd(
        uint32      cmdBufferId,
        uint32      markerValue);

    uint32 Count() const { return m_eventCache.NumElements(); }

    Result GetEventAt(
        uint32                           index,          // [in]  Index of event to retrieve
        UmdCrashAnalysisEvents::EventId* pEventId,       // [out] Event ID
        uint32*                          pCmdBufferId,   // [out] Command buffer ID
        uint32*                          pMarkerValue,   // [out] Marker value
        const char**                     ppMarkerName,   // [out] Marker name
        uint32*                          pMarkerNameSize // [out] Marker name size
    ) const;

    // Called when the last reference to this object is released
    virtual void Destroy() override;

private:
    struct EventData
    {
        UmdCrashAnalysisEvents::EventId    eventId;
        uint32                             cmdBufferId;
        uint32                             markerValue;
        Util::StringBagHandle<char>        markerNameHandle;
    };
    Util::Vector<EventData, 20, IPlatform> m_eventCache;
    Util::StringBag<char, IPlatform>       m_markerNameBag;
};

} // CrashAnalysis
} // Pal
