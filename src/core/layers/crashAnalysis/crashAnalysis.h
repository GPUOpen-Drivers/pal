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

#include <dd_events/gpu_detective/umd_crash_analysis.h>

namespace Pal
{
namespace CrashAnalysis
{

/// The default value of an initialized Crash Analysis marker
constexpr uint32 InitialMarkerValue = UmdCrashAnalysisEvents::InitialExecutionMarkerValue;

/// The final value of a Crash Analysis marker
constexpr uint32 FinalMarkerValue   = UmdCrashAnalysisEvents::FinalExecutionMarkerValue;

using MarkerSource = UmdCrashAnalysisEvents::ExecutionMarkerSource;

static_assert(static_cast<uint32>(MarkerSource::Application) == 0, "");
static_assert(static_cast<uint32>(MarkerSource::Api)         == 1, "");
static_assert(static_cast<uint32>(MarkerSource::Pal)         == 2, "");

constexpr uint32 MarkerStackCount = 0xF; // Corresponds to 4 bits of unique source IDs

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

struct MemoryChunk
{
    gpusize      gpuVirtAddr; // GPU address of embedded 'MarkerState'
    MarkerState* pCpuAddr;    // System memory address of embedded 'MarkerState'
    uint32       raftIndex;   // Index of the memory raft owner
};

} // CrashAnalysis
} // Pal
