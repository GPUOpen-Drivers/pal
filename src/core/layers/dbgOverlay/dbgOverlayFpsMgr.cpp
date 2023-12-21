/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/platform.h"
#include "core/layers/dbgOverlay/dbgOverlayFpsMgr.h"
#include "core/layers/dbgOverlay/dbgOverlayPlatform.h"
#include "core/layers/dbgOverlay/dbgOverlayQueue.h"
#include "core/layers/dbgOverlay/dbgOverlayDevice.h"
#include "palAutoBuffer.h"
#include "palDbgPrint.h"
#include "palDevice.h"
#include "palFile.h"
#include "palListImpl.h"
#include "palMutex.h"
#include "palSysUtil.h"
#include <ctime>

using namespace Util;

namespace Pal
{
namespace DbgOverlay
{

// =====================================================================================================================
FpsMgr::FpsMgr(
    Platform*     pPlatform,
    const Device* pDevice,
    bool          isKeyed)
    :
    m_pPlatform(pPlatform),
    m_pDevice(pDevice),
    m_pDefaultFpsMgr(isKeyed ? pPlatform->GetFpsMgr() : nullptr),
    m_frequency(0.f),
    m_cpuTimeSamples(0),
    m_cpuTimeIndex(0),
    m_cpuTimeSum(0.f),
    m_prevBenchmarkKeyState(false),
    m_benchmarkFrames(0),
    m_benchmarkActive(false),
    m_pFrameTimeLog(nullptr),
    m_frameCount(0),
    m_frameTracker(0),
    m_partialFrameTracker(-1),
    m_prevFrameEnd(0),
    m_gpuTimerFrequency(0),
    m_gpuTimeSamples(0),
    m_gpuTimeIndex(0),
    m_gpuTimeSum(0.f),
    m_submitTimeList(pPlatform),
    m_prevGraphKeyState(false),
    m_numGpuTimeRanges(0)
{
    memset(&m_cpuTimeList[0],         0, sizeof(m_cpuTimeList));
    memset(&m_scaledCpuTimeList[0],   0, sizeof(m_scaledCpuTimeList));
    memset(&m_gpuTimeList[0],         0, sizeof(m_gpuTimeList));
    memset(&m_scaledGpuTimeList[0],   0, sizeof(m_scaledGpuTimeList));
    memset(&m_performanceCounters[0], 0, sizeof(m_performanceCounters));
    memset(&m_benchmarkCounter[0],    0, sizeof(m_benchmarkCounter));
    memset(&m_gpuTimeRanges[0],       0, sizeof(m_gpuTimeRanges));

    // Frequency cannot change while the system is running, so it needs to only be queried once
    m_frequency = static_cast<float>(GetPerfFrequency());
}

// =====================================================================================================================
FpsMgr::~FpsMgr()
{
    for (auto iter = m_submitTimeList.Begin(); iter.Get() != nullptr;)
    {
        m_submitTimeList.Erase(&iter);
    }

    if ((m_pDevice != nullptr) && (m_pDevice->GetPlatform()->PlatformSettings().overlayBenchmarkConfig.usageLogEnable))
    {
        Result result = DumpUsageLogs();
        PAL_ASSERT(result == Result::Success);
    }

    PAL_SAFE_DELETE_ARRAY(m_pFrameTimeLog, m_pPlatform);
}

// =====================================================================================================================
Result FpsMgr::Init()
{
    return Result::Success;
}

// =====================================================================================================================
// Dumps the usage logs of this instance of Pal to a designated file.
Result FpsMgr::DumpUsageLogs()
{
    Result result = Result::Success;

    const auto& settings = m_pDevice->GetPlatform()->PlatformSettings();

    if (m_frameCount > 0)
    {
        PAL_ASSERT(m_pDevice != nullptr);

        char fileNameAndPath[1024] = {};
        Util::Snprintf(&fileNameAndPath[0],
                       sizeof(fileNameAndPath),
                       "%s/%s",
                       &settings.overlayBenchmarkConfig.usageLogDirectory[0],
                       &settings.overlayBenchmarkConfig.usageLogFilename);

        File usageLogFile;
        result = usageLogFile.Open(&fileNameAndPath[0], FileAccessAppend);

        if (result == Result::Success)
        {
            char* pAppName = nullptr;
            char  appNameAndPath[1024] = { };
            GetExecutableName(&appNameAndPath[0], &pAppName, sizeof(appNameAndPath));

            time_t rawTime;
            time(&rawTime);
            struct tm* pTimeInfo = localtime(&rawTime);

            char timeString[64];
            strftime(&timeString[0], sizeof(timeString), "%c", pTimeInfo);

            char buffer[1024];
            Util::Snprintf(buffer, sizeof(buffer), "%s : %s : %u frames\n", &timeString[0], pAppName, m_frameCount);

            // Create FPS log file
            result = usageLogFile.Write(&buffer[0], strlen(buffer));
        }

        if (result == Result::Success)
        {
            usageLogFile.Close();
        }
    }

    return result;
}

// =====================================================================================================================
// Retrieves the frames per second of present calls.
float FpsMgr::GetFramesPerSecond()
{
    // FPS is 1 divided by the average time for a single frame.
    return (m_cpuTimeSum > 0) ? (static_cast<float>(m_cpuTimeSamples) / m_cpuTimeSum) : 0.0f;
}

// =====================================================================================================================
// Updates the moving average of frame time between present calls.
void FpsMgr::UpdateFps()
{
    // Also keep the default FpsMgr tracking the CPU frame time.
    // Thus APIs not yet using the keyed FpsMgr will still get the same fps as before.
    if (m_pDefaultFpsMgr != nullptr)
    {
        m_pDefaultFpsMgr->UpdateFps();
    }

    // Set the last time query.
    m_performanceCounters[LastQuery] = m_performanceCounters[CurrentQuery];

    // Find the current time.
    m_performanceCounters[CurrentQuery] = GetPerfCpuTime();

    if (m_performanceCounters[LastQuery] != 0)
    {
        // Time since last frame is the difference between the queries divided by the frequency of the performance counter.
        float time = (m_performanceCounters[CurrentQuery] - m_performanceCounters[LastQuery]) / m_frequency;

        // Simple Moving Average: Subtract the oldest time on the list, add in the newest time, and update the list of
        // times.
        m_cpuTimeSum -= m_cpuTimeList[m_cpuTimeIndex];
        m_cpuTimeSum += time;
        m_cpuTimeList[m_cpuTimeIndex] = time;

        // Calculating value for the time graph
        // scaledTime = time * NumberOfPixelsToScale / (1/60)fps
        double scaledCpuTimePerFrame = time * NumberOfPixelsToScale * 60.0;
        m_scaledCpuTimeList[m_cpuTimeIndex] = static_cast<uint32>(scaledCpuTimePerFrame);

        // Loop the list.
        if (++m_cpuTimeIndex == TimeCount)
        {
            m_cpuTimeIndex = 0;
        }

        // Increase m_cpuTimeSamples put don't go above TimeCount
        m_cpuTimeSamples = Min(m_cpuTimeSamples + 1, TimeCount);
    }
}

// =====================================================================================================================
// Retrieves the cpu time between present calls.
float FpsMgr::GetCpuTime()
{
    return (m_cpuTimeSamples > 0) ? (m_cpuTimeSum / m_cpuTimeSamples) : 0.0f;
}

// =====================================================================================================================
// Retrieves the gpu time between present calls.
float FpsMgr::GetGpuTime()
{
    return (m_pDefaultFpsMgr != nullptr) ?
        m_pDefaultFpsMgr->GetGpuTime() : (m_gpuTimeSamples > 0) ? (m_gpuTimeSum / m_gpuTimeSamples) : 0.0f;
}

// =====================================================================================================================
// Returns true if one of the recorded GPU times only measured part of its frame.
bool FpsMgr::PartialGpuTime()
{
    // Compute the value of the frame tracker at the time we measured the oldest GPU time in our list.
    const uint64 oldestFrame = (m_frameTracker > TimeCount) ? (m_frameTracker - TimeCount) : 0;

    return ((m_partialFrameTracker >= 0) && (static_cast<uint64>(m_partialFrameTracker) >= oldestFrame));
}

// =====================================================================================================================
void FpsMgr::IncrementFrameCount()
{
    // The default FpsMgr gets used when we have to window/swap chain/ .. specific key.
    // This is the case for the submissions where we measure the GPU time.
    // That default FpsMgr also needs to know about the frame counter increments.
    if (m_pDefaultFpsMgr != nullptr)
    {
        m_pDefaultFpsMgr->IncrementFrameCount();
    }

    // We need to take the GPU timestamp lock here because the submitted time list must be ordered by frame number.
    MutexAuto lock(&m_gpuTimestampWorkLock);
    m_frameCount++;
}

// =====================================================================================================================
// Updates the moving average of frame time between present calls.
void FpsMgr::UpdateGpuFps()
{
    // The default FpsMgr keeps track of the GPU times.
    if (m_pDefaultFpsMgr != nullptr)
    {
        m_pDefaultFpsMgr->UpdateGpuFps();
    }
    else
    {
        MutexAuto lock(&m_gpuTimestampWorkLock);

        for (auto iter = m_submitTimeList.Begin(); iter.Get() != nullptr;)
        {
            GpuTimestampPair* const pTimestamp = *iter.Get();

            //Check for a completed frame
            if (pTimestamp->frameNumber > m_frameTracker)
            {
                //Evaluate gpu time per frame
                const float gpuTimePerFrame = ComputeGpuTimePerFrame();

                //Update m_frameTracker
                m_frameTracker = pTimestamp->frameNumber;

                // Simple Moving Average: Subtract the oldest time on the list,
                // add in the newest time, and update the list of times.
                m_gpuTimeSum -= m_gpuTimeList[m_gpuTimeIndex];
                m_gpuTimeSum += gpuTimePerFrame;
                m_gpuTimeList[m_gpuTimeIndex] = gpuTimePerFrame;

                // Calculating value for the time graph
                // scaledTime = time * numberOfPixelsToScale / (1/60) fps
                double scaledGpuTimePerFrame = gpuTimePerFrame * NumberOfPixelsToScale * 60.0;
                m_scaledGpuTimeList[m_gpuTimeIndex] = static_cast<uint32>(scaledGpuTimePerFrame);

                // Loop the list.
                if (++m_gpuTimeIndex == TimeCount)
                {
                    m_gpuTimeIndex = 0;
                }

                // Increase m_gpuTimeSamples put don't go above TimeCount
                m_gpuTimeSamples = Min(m_gpuTimeSamples + 1, TimeCount);
            }

            // Update Evaluate Time List
            if (pTimestamp->pFence->GetStatus() == Result::Success)
            {
                // If this triggers, this timestamp was added to the list out of frame order.
                PAL_ASSERT(pTimestamp->frameNumber == m_frameTracker);

                if (m_numGpuTimeRanges < MaxGpuTimeRanges)
                {
                    m_gpuTimeRanges[m_numGpuTimeRanges].begin = *pTimestamp->pBeginTimestamp;
                    m_gpuTimeRanges[m_numGpuTimeRanges].end = *pTimestamp->pEndTimestamp;
                    m_numGpuTimeRanges++;
                }
                else
                {
                    // If we can't fit anything else in the array we have to report a partial frame time.
                    m_partialFrameTracker = m_frameTracker;
                }

                // Remove the timestamp pair from the submit list and release this submission.
                m_submitTimeList.Erase(&iter);
                AtomicDecrement(&pTimestamp->numActiveSubmissions);
            }
            else
            {
                // We must evaluate all of the timestamps for the current frame before any others. If we were to continue
                // looping through the list it's possible that we would attempt to evaluate a timestamp for the next frame
                // before this timestamp. We will restart at this timestamp when this function is called again.
                break;
            }
        }
    }
}

// =====================================================================================================================
// Computes the Gpu Time Per Frame
float FpsMgr::ComputeGpuTimePerFrame()
{
    float gpuTime = 0.f;

    // Use insertion sort to sort this array from earliest begin time to latest begin time. This should be fast enough
    // for well-behaved applications that use few submits per frame.
    for (uint32 next = 1; next < m_numGpuTimeRanges; ++next)
    {
        for (uint32 swap = next; (swap > 0) && (m_gpuTimeRanges[swap - 1].begin > m_gpuTimeRanges[swap].begin); --swap)
        {
            const GpuTimeRange temp   = m_gpuTimeRanges[swap];
            m_gpuTimeRanges[swap]     = m_gpuTimeRanges[swap - 1];
            m_gpuTimeRanges[swap - 1] = temp;
        }
    }

    // Each range must be clamped to the end of the previous frame. If we don't do that, multi-queue, multi-frame
    // overlapping work will be double-counted.
    for (uint32 idx = 0; idx < m_numGpuTimeRanges; ++idx)
    {
        m_gpuTimeRanges[idx].begin = Max(m_gpuTimeRanges[idx].begin, m_prevFrameEnd);
        m_gpuTimeRanges[idx].end   = Max(m_gpuTimeRanges[idx].end,   m_prevFrameEnd);
    }

    // Now we can loop once over the array and accumulate the GPU time of all ranges.
    uint32 rangeIdx = 0;
    while (rangeIdx < m_numGpuTimeRanges)
    {
        GpuTimeRange mergedRange = m_gpuTimeRanges[rangeIdx++];

        // If this triggers our sort is buggy or a previous loop iteration messed something up.
        PAL_ASSERT(mergedRange.begin >= m_prevFrameEnd);

        // Merge all timestamp ranges that intersect. We're iterating from the earliest beginning time to the latest
        // so once we find a gap between the two ranges there cannot be a later range that does intersect.
        while ((rangeIdx < m_numGpuTimeRanges) && (mergedRange.end >= m_gpuTimeRanges[rangeIdx].begin))
        {
            mergedRange.end = Max(mergedRange.end, m_gpuTimeRanges[rangeIdx].end);
            rangeIdx++;
        }

        // The merged range is complete, update the elapsed GPU time.
        const uint64 rangeSize = mergedRange.end - mergedRange.begin;
        const float  rangeTime = static_cast<float>(rangeSize) / m_gpuTimerFrequency;

        gpuTime += rangeTime;

        // Update the previous frame's ending time.
        m_prevFrameEnd = mergedRange.end;
    }

    // Drop the contents of the array so we can reuse it for the next frame.
    m_numGpuTimeRanges = 0;

    return gpuTime;
}

// =====================================================================================================================
// Updates the submission tracking info for the timestamp and adds it to the submitted time list.
void FpsMgr::UpdateSubmitTimelist(
    uint32             queueCount,
    GpuTimestampPair** ppTimestamp)
{
    PAL_ASSERT(ppTimestamp != nullptr);
    PAL_ASSERT(queueCount > 0);

    MutexAuto lock(&m_gpuTimestampWorkLock);

    // This class is owned by the platform so it has no way to query the timer frequency from any device. We rely on
    // the timestamp pairs reporting their frequency which must be constant across all devices.
    for (uint32 i = 0; i < queueCount; i++)
    {
        if (ppTimestamp[i] == nullptr)
        {
            continue;
        }

        PAL_ASSERT((m_gpuTimerFrequency == 0) || (m_gpuTimerFrequency == ppTimestamp[i]->timestampFrequency));

        m_gpuTimerFrequency = ppTimestamp[i]->timestampFrequency;

        ppTimestamp[i]->frameNumber = m_frameCount;
        AtomicIncrement(&ppTimestamp[i]->numActiveSubmissions);

        m_submitTimeList.PushBack(ppTimestamp[i]);
    }
}

// =====================================================================================================================
// This is called when a queue failed to timestamp one of its submissions.
void FpsMgr::NotifySubmitWithoutTimestamp()
{
    MutexAuto lock(&m_gpuTimestampWorkLock);
    m_partialFrameTracker = m_frameTracker;
}

// =====================================================================================================================
// The FPS manager must release any references to the given queue's GpuTimestampPairs.
void FpsMgr::NotifyQueueDestroyed(
    const IQueue* pQueue)
{
    MutexAuto lock(&m_gpuTimestampWorkLock);
    for (auto iter = m_submitTimeList.Begin(); iter.Get() != nullptr;)
    {
        if ((*iter.Get())->pOwner == pQueue)
        {
            m_submitTimeList.Erase(&iter);

            // If we removed any timestamps we should treat the current frame as a partial frame.
            m_partialFrameTracker = m_frameTracker;
        }
        else
        {
            iter.Next();
        }
    }
}

// =====================================================================================================================
uint32 FpsMgr::GetScaledCpuTime(uint32 index)
{
    uint32 cpuIndex = (m_cpuTimeIndex + index) % TimeCount;

    return m_scaledCpuTimeList[cpuIndex];
}

// =====================================================================================================================
uint32 FpsMgr::GetScaledGpuTime(uint32 index)
{
    uint32 gpuIndex = (m_gpuTimeIndex + index) % TimeCount;

    return m_scaledGpuTimeList[gpuIndex];
}

// =====================================================================================================================
// Composes output string for overlay.
void FpsMgr::GetBenchmarkString(
    char*  pString,   // Overlay string written here.
    size_t bufSize)   // Size of pString buffer.
{
    // Compose overlay string.
    if (m_benchmarkCounter[LastQuery] == 0)
    {
        // Haven't ever started a benchmark.
        Util::Snprintf(pString, bufSize, "Benchmark (F11):      -.-- FPS");
    }
    else
    {
        const float time = static_cast<float>((m_benchmarkCounter[CurrentQuery] -
                           m_benchmarkCounter[LastQuery]) / m_frequency);

        const float fps  = 1.0f / (time / m_benchmarkFrames);

        if (m_benchmarkActive)
        {
            PAL_ASSERT(m_pDevice != nullptr);
            // Check if the benchmark should be ended due to a settings-imposed maximum time.
            const uint32 maxBenchmarkTime =
                m_pDevice->GetPlatform()->PlatformSettings().overlayBenchmarkConfig.maxBenchmarkTime;
            if ((maxBenchmarkTime != 0) && (time >= maxBenchmarkTime))
            {
                m_benchmarkActive = false;

                if ((m_pFrameTimeLog != nullptr) &&
                    m_pDevice->GetPlatform()->PlatformSettings().overlayBenchmarkConfig.logFrameStats)
                {
                    DumpFrameLogs();
                }
            }

            if (maxBenchmarkTime == 0)
            {
                Util::Snprintf(pString, bufSize, "Benchmark Active:  %7.2f FPS", fps);
            }
            else
            {
                const uint32 timeLeft = maxBenchmarkTime - static_cast<uint32>(time);
                Util::Snprintf(pString, bufSize, "Benchmark (%3ds):  %7.2f FPS", timeLeft, fps);
            }
        }
        else
        {
            Util::Snprintf(pString, bufSize, "Benchmark Done:    %7.2f FPS", fps);
        }
    }
}

// =====================================================================================================================
// Updates benchmark status and composes output string for overlay.
void FpsMgr::UpdateBenchmark()
{
    // Also keep the default FpsMgr tracking the CPU frame time.
    // Thus APIs not yet using the keyed FpsMgr will still get the same fps as before.
    if (m_pDefaultFpsMgr != nullptr)
    {
        m_pDefaultFpsMgr->UpdateBenchmark();
    }

    PAL_ASSERT(m_pDevice != nullptr);
    const auto& settings = m_pDevice->GetPlatform()->PlatformSettings();
    const bool logFrameStats = settings.overlayBenchmarkConfig.logFrameStats;

    if (m_benchmarkActive)
    {
        // If the benchmark is active, extend the current benchmark span to now.
        m_benchmarkCounter[CurrentQuery] = GetPerfCpuTime();

        // If logging frame times and FPS, save off current frame end time (in ms).
        if (logFrameStats                &&
            (m_pFrameTimeLog != nullptr) &&
            (m_benchmarkFrames < settings.overlayBenchmarkConfig.maxLoggedFrames))
        {
            const float timeMs = static_cast<float>((m_benchmarkCounter[CurrentQuery] -
                                 m_benchmarkCounter[LastQuery]) /
                                 m_frequency * 1000.0f);
            m_pFrameTimeLog[m_benchmarkFrames] = timeMs;
        }

        m_benchmarkFrames++;
    }

    // Toggle benchmark state when the F11 key is pressed.
    if (IsKeyPressed(KeyCode::F11, &m_prevBenchmarkKeyState))
    {
        if (m_benchmarkActive)
        {
            m_benchmarkActive = false;

            if ((m_pFrameTimeLog != nullptr) && logFrameStats)
            {
                DumpFrameLogs();
            }
        }
        else
        {
            // Beginning a new benchmark span.
            m_benchmarkCounter[LastQuery] = GetPerfCpuTime();

            m_benchmarkActive = true;
            m_benchmarkFrames = 0;

            if ((m_pFrameTimeLog == nullptr) && logFrameStats)
            {
                m_pFrameTimeLog =
                    PAL_NEW_ARRAY(float,
                                  settings.overlayBenchmarkConfig.maxLoggedFrames,
                                  m_pPlatform,
                                  SystemAllocType::AllocInternal);
                PAL_ASSERT(m_pFrameTimeLog != nullptr);
            }
        }
    }
}

// =====================================================================================================================
// Dumps frame statistics logs collected during benchmark.
void FpsMgr::DumpFrameLogs()
{
    // Unique log ID during an application run
    static uint32 logId = 0;

    constexpr uint32 BufferSize = 640;
    char buf[BufferSize];

    PAL_ASSERT(m_pDevice != nullptr);
    const auto& settings = m_pDevice->GetPlatform()->PlatformSettings();
    // Create timeline log file
    Util::Snprintf(buf,
                   BufferSize,
                   "%s/timelog_%05d.csv",
                   settings.overlayBenchmarkConfig.frameStatsLogDirectory,
                   logId);

    File timeLogFile;
    timeLogFile.Open(buf, FileAccessWrite);

    constexpr char FrameTimesHeader[] = "Frame, Time(ms)\n";
    timeLogFile.Write(FrameTimesHeader, strlen(FrameTimesHeader));

    // Create FPS log file
    Util::Snprintf(buf,
                   BufferSize,
                   "%s/fpslog_%05d.csv",
                   settings.overlayBenchmarkConfig.frameStatsLogDirectory,
                   logId);

    File fpsLogFile;
    fpsLogFile.Open(buf, FileAccessWrite);

    constexpr char FpsHeader[] = "FPS\n";
    fpsLogFile.Write(FpsHeader, strlen(FpsHeader));

    // Sample FPS on average once per 1000 milliseconds
    constexpr float FpsSampleTime = 1000.0f;

    float  prevFpsSampleTime = 0.0f;
    float  fpsSampleEndTime  = FpsSampleTime;
    uint32 framesInSample    = 0;

    const uint32 loggedFrames = Min(m_benchmarkFrames, settings.overlayBenchmarkConfig.maxLoggedFrames);
    for (uint32 i = 0; i < loggedFrames; i++)
    {
        Util::Snprintf(buf, BufferSize, "%d, %.3f\n", i, m_pFrameTimeLog[i]);
        timeLogFile.Write(buf, strlen(buf));

        framesInSample++;

        if (m_pFrameTimeLog[i] >= fpsSampleEndTime)
        {
            // Sample time here is in milliseconds
            float sampleTime = m_pFrameTimeLog[i] - prevFpsSampleTime;

            float fps = (sampleTime > 0.0f) ? (1000.0f / (sampleTime / framesInSample)) : 0.0f;

            Util::Snprintf(buf, BufferSize, "%.3f\n", fps);
            fpsLogFile.Write(buf, strlen(buf));

            prevFpsSampleTime = m_pFrameTimeLog[i];
            fpsSampleEndTime += FpsSampleTime;
            framesInSample    = 0;
        }
    }

    // Advance log ID for the next time
    logId++;
}

// =====================================================================================================================
DebugOverlayLocation FpsMgr::GetDebugOverlayLocation()
{
    PAL_ASSERT(m_pDevice != nullptr);
    DebugOverlayLocation overlayLocation =
        m_pDevice->GetPlatform()->PlatformSettings().debugOverlayConfig.debugOverlayLocation;

    // If F10 is held then shift the overlay
    if (Util::IsKeyPressed(Util::KeyCode::F10))
    {
        overlayLocation = static_cast<DebugOverlayLocation>((overlayLocation + 1) % DebugOverlayCount);
    }

    return overlayLocation;
}

// =====================================================================================================================
DebugOverlayLocation FpsMgr::GetTimeGraphLocation()
{
    PAL_ASSERT(m_pDevice != nullptr);
    const DebugOverlayLocation overlayLocation =
        m_pDevice->GetPlatform()->PlatformSettings().debugOverlayConfig.debugOverlayLocation;

    DebugOverlayLocation timeGraphLocation = DebugOverlayLowerRight;

    if ((overlayLocation == DebugOverlayLowerRight) || (overlayLocation == DebugOverlayUpperRight))
    {
        timeGraphLocation = DebugOverlayLowerLeft;
    }
    else if ((overlayLocation == DebugOverlayLowerLeft) || (overlayLocation == DebugOverlayUpperLeft))
    {
        timeGraphLocation = DebugOverlayLowerRight;
    }
    return timeGraphLocation;
}

} // DbgOverlay
} // Pal
