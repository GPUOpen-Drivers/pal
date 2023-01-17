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

#pragma once

#include "core/layers/gpuProfiler/gpuProfilerDevice.h"
#include "core/layers/decorators.h"
#include "core/layers/functionIds.h"
#include "palDeque.h"
#include "palFile.h"
#include "palAutoBuffer.h"
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

            uint32 subQueueIdx;               // Identifies which subQueue this cmdBuf call is submitted to.

            struct
            {
                uint32 draw        :  1;      // Draw call (should track graphics shaders, vertex/instance count, etc.).
                uint32 dispatch    :  1;      // Dispatch call (should track compute shader, thread group count, etc.).
                uint32 taskmesh    :  1;      // Indicates that this dispatch has a task/mesh shader workload.
                uint32 barrier     :  1;      // Barrier (should track before state, after state, etc.).
                uint32 comment     :  1;      // A CmdCommentString call (should track the comment text).
                uint32 reserved1   :  1;
                uint32 reserved    : 26;
            } flags;

            union
            {
                // Log data only interesting for draw calls (i.e., flags.draw == 1).
                struct
                {
                    PipelineInfo pipelineInfo;  // Bound pipeline info.
                    uint64       apiPsoHash;    // ApiPsoHash of the bound pipeline provided by client.
                    uint32       vertexCount;   // Num vertices drawn, if known (i.e., non-indirect calls).
                    uint32       instanceCount; // Num instances drawn, if known (i.e., non-indirect calls).
                } draw;

                // Log data only interesting for dispatch calls (i.e., flags.dispatch == 1).
                struct
                {
                    PipelineInfo pipelineInfo;     // Bound pipeline info.
                    uint64       apiPsoHash;       // ApiPsoHash of the bound pipeline provided by client.
                    uint32       threadGroupCount; // Threadgroups launched, if known (i.e., non-indirect calls).
                } dispatch;

                struct
                {
                    PipelineInfo pipelineInfo;     // Bound pipeline info.
                    uint64       apiPsoHash;       // ApiPsoHash of the bound pipeline provided by client.
                    uint32       threadGroupCount; // Threadgroups launched, if known (i.e., non-indirect calls).
                } taskmesh;

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

typedef Util::Deque<TargetCmdBuffer*, Platform> CmdBufDeque;

// This struct tracks per subQueue info when we do gang submission.
struct SubQueueInfo
{
    QueueType  queueType;
    EngineType engineType;
    uint32     engineIndex;
    // For each subQueue, track 2 lists of various objects.  Objects that may still be queued for hardware access are
    // in the busy list, others are in the available list.
    CmdBufDeque* pAvailableCmdBufs;
    CmdBufDeque* pBusyCmdBufs;

    CmdBufDeque* pAvailableNestedCmdBufs;
    CmdBufDeque* pBusyNestedCmdBufs;
};

// =====================================================================================================================
// GpuProfiler implementation of the IQueue interface.  Resposible for generating instrumented versions of the
// recorded ICmdBuffer objects the client submits and gathering/reporting performance data.
class Queue final : public QueueDecorator
{
public:
    Queue(IQueue*    pNextQueue,
          Device*    pDevice,
          uint32     queueCount,
          uint32     masterQueueId);

    Result Init(const QueueCreateInfo* pCreateInfo);

    // Acquire methods return corresponding objects for use by a command buffer being replayed from reusable pools
    // managed by the Queue.
    TargetCmdBuffer* AcquireCmdBuf(uint32 subQueueIdx, bool nested);
    Result AcquireGpaSession(GpuUtil::GpaSession** ppGpaSession);

    void AddLogItem(const LogItem& logItem);

    Util::VirtualLinearAllocator* ReplayAllocator() { return &m_replayAllocator; }

    // Public IQueue interface methods:
    virtual Result Submit(
        const MultiSubmitInfo& submitInfo) override;
    virtual Result WaitIdle() override;
    virtual Result SignalQueueSemaphore(
        IQueueSemaphore* pQueueSemaphore,
        uint64           value) override;
    virtual Result WaitQueueSemaphore(
        IQueueSemaphore* pQueueSemaphore,
        uint64           value) override;

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
    void AddDfSpmEndCmdBuffer(
        Util::AutoBuffer<ICmdBuffer*, 64, PlatformDecorator>* pNextCmdBuffers,
        Util::AutoBuffer<CmdBufInfo, 64, PlatformDecorator>*  pNextCmdBufferInfos,
        uint32                                                subQueueIdx,
        uint32                                                globalCmdBufIdx,
        uint32*                                               pLocalCmdBufIdx,
        uint32                                                globalCmdBufInfoIdx,
        uint32*                                               pLocalCmdBufInfoIdx,
        const LogItem&                                        logItem);

    Result EndDfSpm();
    void   RecordDfSpmEndCmdBufInfo(CmdBufInfo* cmdBufInfo);

    void RecordDfSpmEndCmdBuffer(
        TargetCmdBuffer* pEndDfSpmCmdBuf,
        const LogItem&   logItem);

    Result SubmitDfSpmEndCmdBuffer(TargetCmdBuffer* pEndDfSpmCmdBuf);

    Result InternalSubmit(
        const MultiSubmitInfo& submitInfo,
        bool                   releaseObjects);

    Result BeginNextFrame(bool samplingEnabled);

    void LogQueueCall(QueueCallId callId);

    void OutputLogItemsToFile(size_t count, bool hasDrawsDispatches);
    void OpenLogFile(uint32 frameId);
    void OpenSqttFile(
        uint32         shaderEngineId,
        uint32         computeUnitId,
        uint32         traceId,
        Util::File*    pFile,
        const LogItem& logItem);
    void OpenSpmFile(Util::File* pFile, uint32 traceId, const LogItem& logItem, bool isDataFabric);
    void OutputRgpFile(const GpuUtil::GpaSession& gpaSession, uint32 gpaSampleId);
    void OutputQueueCallToFile(const LogItem& logItem);
    void OutputCmdBufCallToFile(const LogItem& logItem, const char* pNestedCmdBufPrefix);
    void OutputFrameToFile(const LogItem& logItem);

    void OutputTimestampsToFile(const LogItem& logItem);
    void OutputPipelineStatsToFile(const LogItem& logItem);
    void OutputGlobalPerfCountersToFile(const LogItem& logItem);
    void OutputTraceDataToFile(const LogItem& logItem);
    void OutputDfSpmData(
        const LogItem&     logItem,
        void*              pResult,
        size_t             offset,
        size_t             dataSize);
    void OutputRlcSpmData(
        const LogItem&     logItem,
        void*              pResult,
        size_t             offset,
        size_t             dataSize);

    Result FillOutSpmGpaSessionSampleConfig(
        uint32                           numSpmCountersRequested,
        const GpuProfiler::PerfCounter*  pStreamingCounters,
        bool                             isDataFabric);

    Device*const     m_pDevice;

    uint32           m_queueCount;

    SubQueueInfo*    m_pQueueInfos;

    uint32           m_queueId;

    uint32           m_shaderEngineCount;

    ICmdAllocator*   m_pCmdAllocator;        // Allocator for the instrumented version of the non-nested command
                                             // buffers this queue will generate at submit time.
    ICmdAllocator*   m_pNestedCmdAllocator;  // Allocator for the instrumented version of the nested command
                                             // buffers this queue will generate at submit time.

    Util::VirtualLinearAllocator m_replayAllocator; // Used to allocate temporary memory during command buffer replay.

    // GpaSession config info for the queue
    GpuUtil::GpaSampleConfig                    m_gpaSessionSampleConfig;

    Util::Deque<GpuUtil::GpaSession*, Platform> m_availableGpaSessions;
    Util::Deque<GpuUtil::GpaSession*, Platform> m_busyGpaSessions;
    GpuUtil::GpaSession::PerfExpMemDeque        m_availPerfExpMem;

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
        IFence*  pFence;
        uint32*  pCmdBufCount;
        uint32*  pNestedCmdBufCount;
        uint32   gpuMemCount;
        uint32   logItemCount;
        uint32   gpaSessionCount;
        bool     hasDrawOrDispatch;
    };
    Util::Deque<PendingSubmitInfo, Platform> m_pendingSubmits;

    // Tracks resources that have been acquired and log items that have been added since the last tracked submit.  This
    // structure will be pushed onto the back of m_pendingSubmits on the next tracked submit.
    PendingSubmitInfo                 m_nextSubmitInfo;

    Util::Deque<LogItem, Platform>    m_logItems;         // List of outstanding calls waiting to be logged.
    Util::File                        m_logFile;          // File logging is currently outputted to (changes per frame).
    uint32                            m_curLogFrame;      // Used to determine when a new frame is started and a new log
                                                          // file should be opened.
    uint32                            m_curLogCmdBufIdx;  // Current command buffer index for the frame being logged.
    uint32                            m_curLogTraceIdx;   // Current SQTT/SPM index for the cmdbuf being logged.

    LogItem                           m_perFrameLogItem;  // Log item used when the profiling granularity is per frame.
    bool                              m_isDfSpmTraceEnabled;

    PAL_DISALLOW_DEFAULT_CTOR(Queue);
    PAL_DISALLOW_COPY_AND_ASSIGN(Queue);
};

} // GpuProfiler
} // Pal
