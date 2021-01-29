/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palList.h"
#include "palMutex.h"
#include "palSysUtil.h"

namespace Pal
{

// Forward decl's
enum   DebugOverlayLocation : uint32;
struct DebugOverlaySettings;

namespace DbgOverlay
{

struct GpuTimestampPair;
class  Platform;
class  Device;

static constexpr uint32 TimeCount             = 100; // Number of times to average for FPS
static constexpr uint32 NumberOfPixelsToScale = 100; // Number of pixels to scale in the Graph

// =====================================================================================================================
// API-layer implementation of the 'OverlayFpsMgr' object. OverlayFpsMgr is not exposed to the API directly; rather,
// they are accessed via Singleton.
class FpsMgr
{
public:
    FpsMgr(Platform* pPlatform, const Device* pDevice);
    ~FpsMgr();

    Result Init();
    void   UpdateFps();
    void   UpdateGpuFps();
    void   UpdateBenchmark();
    float  GetFramesPerSecond();
    float  GetCpuTime();
    float  GetGpuTime();
    bool   PartialGpuTime();
    void   GetBenchmarkString(char* pString, size_t bufSize);
    void   DumpFrameLogs();
    Result DumpUsageLogs();

    // Increments the total running frame count for this instance of Pal.
    void IncrementFrameCount();

    uint64 FrameCount() const { return m_frameCount; }

    uint32 GetScaledCpuTime(uint32 index);
    uint32 GetScaledGpuTime(uint32 index);

    DebugOverlayLocation GetDebugOverlayLocation();
    DebugOverlayLocation GetTimeGraphLocation();

    void UpdateSubmitTimelist(uint32 queueCount, GpuTimestampPair** ppTimestamp);
    void NotifySubmitWithoutTimestamp();
    void NotifyQueueDestroyed(const IQueue* pQueue);

private:
    float ComputeGpuTimePerFrame();

    Platform* m_pPlatform;

    // Pointer to the device that should be queried for overlay settings
    const Device* m_pDevice;

    // Enumerations which indicate which performance query index is being modified.
    enum QueryTime : uint32
    {
        LastQuery = 0,       // Last performance query index
        CurrentQuery,        // Current performance query index
        NumQuery             // Total number of query indices
    };

    int64  m_performanceCounters[NumQuery];        // Last and current time queries
    float  m_frequency;                            // Frequency of performance counters
    float  m_cpuTimeList[TimeCount];               // List of times between frames
    uint32 m_cpuTimeSamples;                       // Number of valid entried in m_cpuTimeList
    uint32 m_scaledCpuTimeList[TimeCount];         // Scaled cpu time for the Time Graph
    uint32 m_cpuTimeIndex;                         // Current index into list of times
    float  m_cpuTimeSum;                           // Current sum of all times

    bool   m_prevBenchmarkKeyState;
    int64  m_benchmarkCounter[NumQuery];           // Last and current perf counters for F11 benchmark.
    uint32 m_benchmarkFrames;                      // Number of frames in benchmark span.
    bool   m_benchmarkActive;                      // true if a benchmark span is active.
    float* m_pFrameTimeLog;                        // Array of benchmark frame end times in ms.

    uint64 m_frameCount;                           // Total number of frames rendered/presented
    uint64 m_frameTracker;                         // Keeps track of the current frame being evaluated for GPU Time
    int64  m_partialFrameTracker;                  // -1 or the most recent frame whose GPU time was only partially measured.
    uint64 m_prevFrameEnd;                         // The time (in ticks) when the previous frame ended
    uint64 m_gpuTimerFrequency;                    // How many timer ticks pass in a second
    float  m_gpuTimeList[TimeCount];               // List of times between frames
    uint32 m_gpuTimeSamples;                       // Number of valid entreis in m_gpuTimeList
    uint32 m_scaledGpuTimeList[TimeCount];         // Scaled gpu time for the Time Graph
    uint32 m_gpuTimeIndex;                         // Current index into list of times
    float  m_gpuTimeSum;                           // Current sum of all times

    Util::Mutex   m_gpuTimestampWorkLock;                       // Mutex protecting access to SubmitTimelist
    Util::List<GpuTimestampPair*, Platform> m_submitTimeList;   // Contains GpuTimestampPairs of all prior submissions
                                                                // that must be evaluated into GPU frame times

    bool m_prevDebugKeyState;
    bool m_prevGraphKeyState;

    // A length of GPU time as measured by a GpuTimestampPair.
    struct GpuTimeRange
    {
        uint64 begin;
        uint64 end;
    };

    // As GpuTimestampPairs are recycled we copy their times into this array to later compute the total GPU time.
    // It has a fixed size to prevent applications that do not present from endlessly allocating a larger array.
    static constexpr uint32 MaxGpuTimeRanges = 256;

    uint32       m_numGpuTimeRanges;
    GpuTimeRange m_gpuTimeRanges[MaxGpuTimeRanges];

    PAL_DISALLOW_DEFAULT_CTOR(FpsMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(FpsMgr);
};

} // DbgOverlay
} // Pal
