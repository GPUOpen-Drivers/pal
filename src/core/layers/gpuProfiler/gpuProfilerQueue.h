/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/decorators.h"
#include "core/layers/functionIds.h"
#include "palDeque.h"
#include "palFile.h"
#include "palGpaSession.h"
#include "palLinearAllocator.h"

namespace Pal
{
namespace GpuProfiler
{

class CmdBuffer;
class Device;
class Platform;
class TargetCmdBuffer;

static constexpr size_t MaxCommentLength = 512;

// Identifies whether a specific LogItem corresponds to a queue call (Submit(), Present(), etc.), a command buffer
// call (CmdDrawIndexed(), CmdCopyImage(), etc.), or a full frame.
enum LogItemType : uint32
{
    QueueCall,
    CmdBufferCall,
    Frame
};

// Specifies various information describing a single queue or command buffer call to be logged.
struct LogItem
{
    LogItemType type;    // Either a command buffer call or queue call.
    uint32      frameId; // Which frame this log item captures.

    struct
    {
        uint32 perfExpOutOfMemory   :  1; // Perf experiment ran out of memory and could not be executed for this
                                          // command buffer call.
        uint32 perfExpUnsupported   :  1; // Perf experiment is unsupported on this command buffer.
        uint32 pipeStatsUnsupported :  1; // Pipeline stats query is unsupported on this command buffer.
        uint32 reserved             : 29;
    } errors;

    union
    {
        // Command buffer call information.
        struct
        {
            CmdBufCallId callId;              // Identifies exactly which call is logged (e.g., CmdDrawIndexed).

            struct
            {
                uint32 draw     :  1;         // Draw call (should track graphics shaders, vertex/instance count, etc.).
                uint32 dispatch :  1;         // Dispatch call (should track compute shader, thread group count, etc.).
                uint32 barrier  :  1;         // Barrier (should track before state, after state, etc.).
                uint32 comment  :  1;         // A CmdCommentString call (should track the comment text).
                uint32 reserved : 28;
            } flags;

            union
            {
                // Log data only interesting for draw calls (i.e., flags.draw == 1).
                struct
                {
                    PipelineInfo pipelineInfo;  // Bound pipeline info.
                    uint32       vertexCount;   // Num vertices drawn, if known (i.e., non-indirect calls).
                    uint32       instanceCount; // Num instances drawn, if known (i.e., non-indirect calls).
                } draw;

                // Log data only interesting for dispatch calls (i.e., flags.dispatch == 1).
                struct
                {
                    PipelineInfo pipelineInfo;     // Bound pipeline info.
                    uint32       threadGroupCount; // Threadgroups launched, if known (i.e., non-indirect calls).
                } dispatch;

                // Log data only interesting for barrier calls (i.e., flags.barrier == 1).
                struct
                {
                    const char* pComment;     // This string is dynamically allocated by the target CmdBuffer.
                } barrier;

                // Log data only interesting for CmdCommentString calls (i.e., flags.comment == 1).
                struct
                {
                    char string[MaxCommentLength]; // A fixed-length, truncated comment string.
                } comment;
            };
        } cmdBufCall;

        // Queue call information.
        struct
        {
            QueueCallId callId;               // Identifies exactly which call is logged (e.g., Submit(), Present()).
        } queueCall;
    };

    // Pointer to the corresponding GPA session and sample IDs to track this logItem's perfExperiment and/or timestamp
    // and relavant info. Only valid if this logItem contains performance profiling.
    GpuUtil::GpaSession* pGpaSession;
    uint32               gpaSampleId;
    uint32               gpaSampleIdTs;
    uint32               gpaSampleIdQuery;
};

// Tracking structure for a single IGpuMemory allocation owned by a GpuProfiler::Queue.  In particular, it tracks the
// associated CPU pointer since these allocations remain mapped for CPU access for their lifetime.
struct GpuMemoryInfo
{
    IGpuMemory* pGpuMemory;
    void*       pCpuAddr;
};

// Each nested command buffer execution must be played back with its own command allocator because one client cannot
// support automatic memory reuse of nested command memory.
struct NestedInfo
{
    TargetCmdBuffer* pCmdBuffer;
    ICmdAllocator*   pCmdAllocator; // This is a GpuProfiler CmdAllocator.
};

// =====================================================================================================================
// GpuProfiler implementation of the IQueue interface.  Resposible for generating instrumented versions of the
// recorded ICmdBuffer objects the client submits and gathering/reporting performance data.
class Queue : public QueueDecorator
{
public:
    Queue(IQueue*    pNextQueue,
          Device*    pDevice,
          QueueType  queueType,
          EngineType engineType,
          uint32     engineId,
          uint32     queueId);

    Result Init();

    // Acquire methods return corresponding objects for use by a command buffer being replayed from reusable pools
    // managed by the Queue.
    TargetCmdBuffer* AcquireCmdBuf();
    TargetCmdBuffer* AcquireNestedCmdBuf();
    Result AcquireGpaSession(GpuUtil::GpaSession** ppGpaSession);

    void AddLogItem(const LogItem& logItem);

    Util::VirtualLinearAllocator* ReplayAllocator() { return &m_replayAllocator; }

    // Public IQueue interface methods:
    virtual Result Submit(
        const SubmitInfo& submitInfo) override;
    virtual Result WaitIdle() override;
    virtual Result SignalQueueSemaphore(
        IQueueSemaphore* pQueueSemaphore) override;
    virtual Result WaitQueueSemaphore(
        IQueueSemaphore* pQueueSemaphore) override;

    virtual Result PresentDirect(
        const PresentDirectInfo& presentInfo) override;
    virtual Result PresentSwapChain(
        const PresentSwapChainInfo& presentInfo) override;
    virtual Result Delay(
        float delay) override;
    virtual Result RemapVirtualMemoryPages(
        uint32                         rangeCount,
        const VirtualMemoryRemapRange* pRanges,
        bool                           doNotWait,
        IFence*                        pFence) override;
    virtual Result CopyVirtualMemoryPageMappings(
        uint32                                    rangeCount,
        const VirtualMemoryCopyPageMappingsRange* pRanges,
        bool                                      doNotWait) override;

    Device* GetDevice() const { return m_pDevice; }

    const GpuUtil::GpaSampleConfig& GetGpaSessionSampleConfig() const { return m_gpaSessionSampleConfig; }
    GpuUtil::GpaSession* GetPerFrameGpaSession() { return m_perFrameLogItem.pGpaSession; }

    // Check if the logItem contains a valid GPA sample.
    bool HasValidGpaSample(const LogItem* pLogItem, GpuUtil::GpaSampleType type) const;

private:
    virtual ~Queue();

    IFence* AcquireFence();
    void ProcessIdleSubmits();

    Result InternalSubmit(
        const SubmitInfo& submitInfo,
        bool              releaseObjects);

    void BeginNextFrame(bool samplingEnabled);

    void LogQueueCall(QueueCallId callId);

    void OutputLogItemsToFile(size_t count);
    void OpenLogFile(uint32 frameId);
    void OpenSqttFile(
        uint32 shaderEngineId,
        uint32 computeUnitId,
        uint32 drawId,
        Util::File* pFile,
        const LogItem& logItem);
    void OpenSpmFile(Util::File* pFile, const LogItem& logItem);
    void OutputRgpFile(const GpuUtil::GpaSession& gpaSession, uint32 gpaSampleId);
    void OutputQueueCallToFile(const LogItem& logItem);
    void OutputCmdBufCallToFile(const LogItem& logItem, const char* pNestedCmdBufPrefix);
    void OutputFrameToFile(const LogItem& logItem);

    void OutputTimestampsToFile(const LogItem& logItem);
    void OutputPipelineStatsToFile(const LogItem& logItem);
    void OutputGlobalPerfCountersToFile(const LogItem& logItem);
    void OutputTraceDataToFile(const LogItem& logItem);

    void ProfilingClockMode(bool enable);

    Device*const     m_pDevice;
    const QueueType  m_queueType;
    const EngineType m_engineType;
    const uint32     m_engineIndex;
    const uint32     m_queueId;            // Unique ID for the queue to differentiate cases where the client creates
                                           // multiple queues for the same engine.
    uint32           m_shaderEngineCount;

    ICmdAllocator*   m_pCmdAllocator;  // Allocator for the instrumented version of the non-nested command buffers this
                                       // queue will generate at submit time.

    Util::VirtualLinearAllocator m_replayAllocator; // Used to allocate temporary memory during command buffer replay.

    // These pointers track some helper heap allocations which are used to log perf counters and thread traces.
    // They aren't that big (not worth a VirtualLinearAllocator) but we don't know their size at compile time.
    uint64*                m_pGlobalPerfCounterValues;

    // Each replayed nested command buffer needs its own allocator which will be created from this create info.
    CmdAllocatorCreateInfo m_nestedAllocatorCreateInfo;

    // Track 2 lists of various objects owned by this queue.  Objects that may still be queued for hardware access are
    // in the busy list, others are in the available list.
    Util::Deque<TargetCmdBuffer*, Platform>     m_availableCmdBufs;
    Util::Deque<TargetCmdBuffer*, Platform>     m_busyCmdBufs;

    Util::Deque<NestedInfo, Platform>           m_availableNestedCmdBufs;
    Util::Deque<NestedInfo, Platform>           m_busyNestedCmdBufs;

    // GpaSession config info for the queue
    GpuUtil::GpaSampleConfig                    m_gpaSessionSampleConfig;

    Util::Deque<GpuUtil::GpaSession*, Platform> m_availableGpaSessions;
    Util::Deque<GpuUtil::GpaSession*, Platform> m_busyGpaSessions;

    // Create/delete the GpaSession-style config info based on Panel settings
    Result BuildGpaSessionSampleConfig();
    void   DestroyGpaSessionSampleConfig();

    uint32                                      m_numReportedPerfCounters;

    // Tracks a list of fence objects owned by this queue that are ready for reuse.
    Util::Deque<IFence*, Platform>              m_availableFences;

    // Tracks a list of pending (not retired yet) submits on this queue.  When the corresponding pFence object is
    // signaled, we know we can:
    //     - Process logItemCount items in m_logItems - all timestamps, queries, etc. are idle and ready to be logged.
    //     - Reclaim the first cmdBufCount/gpuMemCount/etc. entries in each of the "m_busyFoo" deques.
    //     - Reclaim that fence as available.
    struct PendingSubmitInfo
    {
        IFence* pFence;
        uint32  cmdBufCount;
        uint32  nestedCmdBufCount;
        uint32  gpuMemCount;
        uint32  logItemCount;
        uint32  gpaSessionCount;
    };
    Util::Deque<PendingSubmitInfo, Platform> m_pendingSubmits;

    // Tracks resources that have been acquired and log items that have been added since the last tracked submit.  This
    // structure will be pushed onto the back of m_pendingSubmits on the next tracked submit.
    PendingSubmitInfo                 m_nextSubmitInfo;

    bool                              m_profilingModeEnabled;

    Util::Deque<LogItem, Platform>    m_logItems;         // List of outstanding calls waiting to be logged.
    Util::File                        m_logFile;          // File logging is currently outputted to (changes per frame).
    uint32                            m_curLogFrame;      // Used to determine when a new frame is started and a new log
                                                          // file should be opened.
    uint32                            m_curLogCmdBufIdx;  // Current command buffer index for the frame being logged.
    uint32                            m_curLogSqttIdx;    // Current SQ thread trace index for the cmdbuf being logged.

    LogItem                           m_perFrameLogItem;  // Log item used when the profiling granularity is per frame.

    PAL_DISALLOW_DEFAULT_CTOR(Queue);
    PAL_DISALLOW_COPY_AND_ASSIGN(Queue);
};

} // GpuProfiler
} // Pal
