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

#include "core/layers/gpuProfiler/gpuProfilerCmdBuffer.h"
#include "core/layers/gpuProfiler/gpuProfilerPlatform.h"
#include "core/layers/gpuProfiler/gpuProfilerQueue.h"
#include "palDequeImpl.h"
#include "palSysUtil.h"
#include "palLiterals.h"

using namespace Util;
using namespace Util::Literals;

namespace Pal
{
namespace GpuProfiler
{
// =====================================================================================================================
Queue::Queue(
    IQueue*    pNextQueue,
    Device*    pDevice,
    uint32     queueCount,
    uint32     masterQueueId)
    :
    QueueDecorator(pNextQueue, pDevice),
    m_pDevice(pDevice),
    m_queueCount(queueCount),
    m_pQueueInfos(nullptr),
    m_queueId(masterQueueId),
    m_shaderEngineCount(0),
    m_pCmdAllocator(nullptr),
    m_pNestedCmdAllocator(nullptr),
    m_replayAllocator(64_KiB),
    m_availableGpaSessions(static_cast<Platform*>(pDevice->GetPlatform())),
    m_busyGpaSessions(static_cast<Platform*>(pDevice->GetPlatform())),
    m_availPerfExpMem(static_cast<Platform*>(pDevice->GetPlatform())),
    m_numReportedPerfCounters(0),
    m_availableFences(static_cast<Platform*>(pDevice->GetPlatform())),
    m_pendingSubmits(static_cast<Platform*>(pDevice->GetPlatform())),
    m_logItems(static_cast<Platform*>(pDevice->GetPlatform())),
    m_curLogFrame(0),
    m_curLogCmdBufIdx(0),
    m_curLogTraceIdx(0),
    m_isDfSpmTraceEnabled(false)
{
    memset(&m_gpaSessionSampleConfig,    0, sizeof(m_gpaSessionSampleConfig));
    memset(&m_nextSubmitInfo,            0, sizeof(m_nextSubmitInfo));
    memset(&m_perFrameLogItem,           0, sizeof(m_perFrameLogItem));
}

// =====================================================================================================================
Queue::~Queue()
{
    // Ensure all log items are flushed out before we shut down.
    WaitIdle();
    ProcessIdleSubmits();
    m_logFile.Close();

    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());
    if (m_nextSubmitInfo.pCmdBufCount != nullptr)
    {
        PAL_SAFE_DELETE_ARRAY(m_nextSubmitInfo.pCmdBufCount, pPlatform);
    }

    if (m_nextSubmitInfo.pNestedCmdBufCount != nullptr)
    {
        PAL_SAFE_DELETE_ARRAY(m_nextSubmitInfo.pNestedCmdBufCount, pPlatform);
    }

    for (uint32 qIdx = 0; qIdx < m_queueCount; qIdx++)
    {
        PAL_ASSERT(m_pQueueInfos[qIdx].pBusyCmdBufs->NumElements() == 0);
        PAL_ASSERT(m_pQueueInfos[qIdx].pBusyNestedCmdBufs->NumElements() == 0);

        while (m_pQueueInfos[qIdx].pAvailableCmdBufs->NumElements() > 0)
        {
            TargetCmdBuffer* pCmdBuf = nullptr;
            m_pQueueInfos[qIdx].pAvailableCmdBufs->PopFront(&pCmdBuf);

            pCmdBuf->Destroy();
            PAL_SAFE_FREE(pCmdBuf, m_pDevice->GetPlatform());
        }

        while (m_pQueueInfos[qIdx].pAvailableNestedCmdBufs->NumElements() > 0)
        {
            TargetCmdBuffer* pCmdBuf = nullptr;
            m_pQueueInfos[qIdx].pAvailableNestedCmdBufs->PopFront(&pCmdBuf);

            pCmdBuf->Destroy();
            PAL_SAFE_FREE(pCmdBuf, m_pDevice->GetPlatform());
        }

        PAL_SAFE_DELETE(m_pQueueInfos[qIdx].pAvailableCmdBufs, pPlatform);
        PAL_SAFE_DELETE(m_pQueueInfos[qIdx].pBusyCmdBufs, pPlatform);
        PAL_SAFE_DELETE(m_pQueueInfos[qIdx].pAvailableNestedCmdBufs, pPlatform);
        PAL_SAFE_DELETE(m_pQueueInfos[qIdx].pBusyNestedCmdBufs, pPlatform);
    }

    PAL_ASSERT(m_pendingSubmits.NumElements() == 0);
    PAL_ASSERT(m_busyGpaSessions.NumElements() == 0);

    while (m_availableGpaSessions.NumElements() > 0)
    {
        GpuUtil::GpaSession* pGpaSession = nullptr;
        m_availableGpaSessions.PopFront(&pGpaSession);

        PAL_SAFE_DELETE(pGpaSession, m_pDevice->GetPlatform());
    }

    while (m_availPerfExpMem.NumElements() > 0)
    {
        GpuUtil::PerfExperimentMemory memory;
        m_availPerfExpMem.PopFront(&memory);
        PAL_SAFE_FREE(memory.pMemory, m_pDevice->GetPlatform());
    }

    while (m_availableFences.NumElements() > 0)
    {
        IFence* pFence = nullptr;
        m_availableFences.PopFront(&pFence);

        pFence->Destroy();
        PAL_SAFE_FREE(pFence, m_pDevice->GetPlatform());
    }

    if (m_pCmdAllocator != nullptr)
    {
        m_pCmdAllocator->Destroy();
        PAL_SAFE_FREE(m_pCmdAllocator, m_pDevice->GetPlatform());
    }

    if (m_pNestedCmdAllocator != nullptr)
    {
        m_pNestedCmdAllocator->Destroy();
        PAL_SAFE_FREE(m_pNestedCmdAllocator, m_pDevice->GetPlatform());
    }

    DestroyGpaSessionSampleConfig();

    PAL_SAFE_DELETE_ARRAY(m_pQueueInfos, pPlatform);
}

// =====================================================================================================================
Result Queue::Init(
    const QueueCreateInfo* pCreateInfo) // This is an array of QueueCreateInfo. The size of the array should
                                        // be m_queueCount.
{
    Result result = Result::Success;
    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());
    m_pQueueInfos = PAL_NEW_ARRAY(SubQueueInfo, m_queueCount, pPlatform, AllocInternal);
    memset(m_pQueueInfos, 0, sizeof(SubQueueInfo) * m_queueCount);
    if (m_pQueueInfos == nullptr)
    {
        result = Result::ErrorOutOfMemory;
    }

    if (result == Result::Success)
    {
        m_nextSubmitInfo.pCmdBufCount = PAL_NEW_ARRAY(uint32, m_queueCount, pPlatform, AllocInternal);
    }

    if (m_nextSubmitInfo.pCmdBufCount == nullptr)
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        memset(m_nextSubmitInfo.pCmdBufCount, 0, sizeof(uint32) * m_queueCount);
    }

    if (result == Result::Success)
    {
        m_nextSubmitInfo.pNestedCmdBufCount = PAL_NEW_ARRAY(uint32, m_queueCount, pPlatform, AllocInternal);
    }

    if (m_nextSubmitInfo.pNestedCmdBufCount == nullptr)
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        memset(m_nextSubmitInfo.pNestedCmdBufCount, 0, sizeof(uint32) * m_queueCount);
    }

    if (result == Result::Success)
    {
        for (uint32 i = 0; ((i < m_queueCount) && (result == Result::Success)); i++)
        {
            m_pQueueInfos[i].engineType              = pCreateInfo[i].engineType;
            m_pQueueInfos[i].engineIndex             = pCreateInfo[i].engineIndex;
            m_pQueueInfos[i].queueType               = pCreateInfo[i].queueType;
            m_pQueueInfos[i].pAvailableCmdBufs       = PAL_NEW(CmdBufDeque, pPlatform, AllocInternal)(pPlatform);
            m_pQueueInfos[i].pBusyCmdBufs            = PAL_NEW(CmdBufDeque, pPlatform, AllocInternal)(pPlatform);
            m_pQueueInfos[i].pAvailableNestedCmdBufs = PAL_NEW(CmdBufDeque, pPlatform, AllocInternal)(pPlatform);
            m_pQueueInfos[i].pBusyNestedCmdBufs      = PAL_NEW(CmdBufDeque, pPlatform, AllocInternal)(pPlatform);
            if ((m_pQueueInfos[i].pAvailableCmdBufs == nullptr)       ||
                (m_pQueueInfos[i].pBusyCmdBufs == nullptr)            ||
                (m_pQueueInfos[i].pAvailableNestedCmdBufs == nullptr) ||
                (m_pQueueInfos[i].pBusyNestedCmdBufs == nullptr))
            {
                result = Result::ErrorOutOfMemory;
            }
        }
    }

    if (result == Result::Success)
    {
        result = m_replayAllocator.Init();
    }

    if (result == Result::Success)
    {
        CmdAllocatorCreateInfo createInfo = { };
        createInfo.flags.autoMemoryReuse                      = 1;
        createInfo.flags.disableBusyChunkTracking             = 1;
        createInfo.allocInfo[CommandDataAlloc].allocHeap      = GpuHeapGartUswc;
        createInfo.allocInfo[CommandDataAlloc].allocSize      = 2_MiB;
        createInfo.allocInfo[CommandDataAlloc].suballocSize   = 64_KiB;
        createInfo.allocInfo[EmbeddedDataAlloc].allocHeap     = GpuHeapGartUswc;
        createInfo.allocInfo[EmbeddedDataAlloc].allocSize     = 2_MiB;
        createInfo.allocInfo[EmbeddedDataAlloc].suballocSize  = 64_KiB;
        createInfo.allocInfo[GpuScratchMemAlloc].allocHeap    = GpuHeapGartUswc;
        createInfo.allocInfo[GpuScratchMemAlloc].allocSize    = 2_MiB;
        createInfo.allocInfo[GpuScratchMemAlloc].suballocSize = 64_KiB;

        void* pMemory = PAL_MALLOC(m_pDevice->GetCmdAllocatorSize(createInfo, &result),
                                   m_pDevice->GetPlatform(),
                                   AllocInternal);

        if (result == Result::Success)
        {
            if (pMemory == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                result = m_pDevice->CreateCmdAllocator(createInfo, pMemory, &m_pCmdAllocator);

                if (result != Result::Success)
                {
                    PAL_SAFE_FREE(pMemory, m_pDevice->GetPlatform());
                }
            }
        }
    }

    if (result == Result::Success)
    {
        CmdAllocatorCreateInfo createInfo = { };
        createInfo.flags.autoMemoryReuse                      = 1;
        createInfo.flags.disableBusyChunkTracking             = 1;
        // All nested allocations are set the the minimum size (4KB) because applications that submit hundreds of nested
        // command buffers can potentially exhaust the GPU VA range by simply playing back too many nested command buffers.
        // This will have a small performance impact on large nested command buffers but we have little choice for now.
        createInfo.allocInfo[CommandDataAlloc].allocHeap      = GpuHeapGartUswc;
        createInfo.allocInfo[CommandDataAlloc].allocSize      = 4_KiB;
        createInfo.allocInfo[CommandDataAlloc].suballocSize   = 4_KiB;
        createInfo.allocInfo[EmbeddedDataAlloc].allocHeap     = GpuHeapGartUswc;
        createInfo.allocInfo[EmbeddedDataAlloc].allocSize     = 4_KiB;
        createInfo.allocInfo[EmbeddedDataAlloc].suballocSize  = 4_KiB;
        createInfo.allocInfo[GpuScratchMemAlloc].allocHeap    = GpuHeapGartUswc;
        createInfo.allocInfo[GpuScratchMemAlloc].allocSize    = 4_KiB;
        createInfo.allocInfo[GpuScratchMemAlloc].suballocSize = 4_KiB;

        void* pMemory = PAL_MALLOC(m_pDevice->GetCmdAllocatorSize(createInfo, &result),
                                   m_pDevice->GetPlatform(),
                                   AllocInternal);

        if (result == Result::Success)
        {
            if (pMemory == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                result = m_pDevice->CreateCmdAllocator(createInfo, pMemory, &m_pNestedCmdAllocator);

                if (result != Result::Success)
                {
                    PAL_SAFE_FREE(pMemory, m_pDevice->GetPlatform());
                }
            }
        }
    }

    // Build GpaSession config info based on profiling objectives.
    if (result == Result::Success)
    {
        result = BuildGpaSessionSampleConfig();
    }

    // Note that global perf counters are disabled if this value is zero.
    const uint32 numGlobalPerfCounters = m_pDevice->NumGlobalPerfCounters();
    const PerfCounter* pPerfCounters   = m_pDevice->GlobalPerfCounters();
    if ((result == Result::Success) && (numGlobalPerfCounters > 0))
    {
        m_numReportedPerfCounters = numGlobalPerfCounters;
    }

    return result;
}

// =====================================================================================================================
// Submits the specified command buffers to the next layer.  This same implementation is used for both command buffers
// submitted by the application and any internal command buffers this layer needs to submit.
Result Queue::InternalSubmit(
    const MultiSubmitInfo& submitInfo,
    bool                   releaseObjects)  // If true, all currently acquired objects can be associated with this
                                            // submit and reclaimed once this submit completes.  Otherwise, continue
                                            // building m_nextSubmitInfo pinning more acquired resources to the next
                                            // tracked submit.
{
    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());

    Result result = m_pNextLayer->Submit(submitInfo);

    if ((result == Result::Success) && releaseObjects)
    {
        // Get an available queue-owned fence.
        m_nextSubmitInfo.pFence = AcquireFence();

        // This call will make it so that the profiler fence will show as signaled once all previous work
        // submitted on this queue has submitted, but WaitForFences() will not work.  This is acceptable for
        // this use case, and lets us avoid interfering with app-specified fences in the real submit above.
        AssociateFenceWithLastSubmit(m_nextSubmitInfo.pFence);

        // Track this submission so we know when we can reclaim the queue-owned command buffers and fence.
        result = m_pendingSubmits.PushBack(m_nextSubmitInfo);
        memset(&m_nextSubmitInfo, 0, sizeof(PendingSubmitInfo));

        if (result == Result::Success)
        {
            m_nextSubmitInfo.pCmdBufCount = PAL_NEW_ARRAY(uint32, m_queueCount, pPlatform, AllocInternal);

            if (m_nextSubmitInfo.pCmdBufCount == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                memset(m_nextSubmitInfo.pCmdBufCount, 0, sizeof(uint32) * m_queueCount);
            }
        }

        if (result == Result::Success)
        {
            m_nextSubmitInfo.pNestedCmdBufCount = PAL_NEW_ARRAY(uint32, m_queueCount, pPlatform, AllocInternal);

            if (m_nextSubmitInfo.pNestedCmdBufCount == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                memset(m_nextSubmitInfo.pNestedCmdBufCount, 0, sizeof(uint32) * m_queueCount);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Processes previous submits, sets/resets the device clock mode for all granularity. Inserts cmd buffer to start thread
// trace for per-frame granularity if tracing is enabled. Shared implementation between the DX and normal present paths.
Result Queue::BeginNextFrame(
    bool samplingEnabled)
{
    Result result = Result::Success;

    ProcessIdleSubmits();

    if (samplingEnabled)
    {
        // Change device clock mode to profiling mode if not already enabled.
        // Clock mode is set for the whole frame regardless of the granularity.
        result = m_pDevice->ProfilingClockMode(true);

        if ((result == Result::Success) && m_pDevice->LoggingEnabled(GpuProfilerGranularityFrame))
        {
            // Insert a command buffer that has commands to start the thread trace for this frame.
            TargetCmdBuffer*const pStartFrameTgtCmdBuf = AcquireCmdBuf(0, false);

            CmdBufferBuildInfo buildInfo = {};
            result = pStartFrameTgtCmdBuf->Begin(NextCmdBufferBuildInfo(buildInfo));

            if (result == Result::Success)
            {
                // Clear the per frame LogItem
                memset(&m_perFrameLogItem, 0, sizeof(m_perFrameLogItem));
                m_perFrameLogItem.type    = Frame;
                m_perFrameLogItem.frameId = static_cast<Platform*>(m_pDevice->GetPlatform())->FrameId();

                // Begin a GPA session.
                result = pStartFrameTgtCmdBuf->BeginGpaSession(this);
            }

            if (result == Result::Success)
            {
                const bool perfExp = (m_pDevice->NumGlobalPerfCounters() > 0)      ||
                                     (m_pDevice->NumStreamingPerfCounters() > 0)   ||
                                     (m_pDevice->NumDfStreamingPerfCounters() > 0) ||
                                     (m_pDevice->IsThreadTraceEnabled());

                pStartFrameTgtCmdBuf->BeginSample(this, &m_perFrameLogItem, false, perfExp);

                result = pStartFrameTgtCmdBuf->End();
            }

            if (result == Result::Success)
            {
                ICmdBuffer* pNextCmdBuf = NextCmdBuffer(pStartFrameTgtCmdBuf);
                PerSubQueueSubmitInfo perSubQueueInfo = {};
                perSubQueueInfo.cmdBufferCount        = 1;
                perSubQueueInfo.ppCmdBuffers          = &pNextCmdBuf;
                MultiSubmitInfo nextSubmitInfo        = {};
                nextSubmitInfo.perSubQueueInfoCount   = 1;
                nextSubmitInfo.pPerSubQueueInfo       = &perSubQueueInfo;
                if ((m_pDevice->NumDfStreamingPerfCounters() > 0) && (m_isDfSpmTraceEnabled == false))
                {
                    m_isDfSpmTraceEnabled = true;
                }
                CmdBufInfo cmdBufInfo           = {};
                cmdBufInfo.dfSpmTraceBegin      = m_isDfSpmTraceEnabled ? 1 : 0;
                cmdBufInfo.isValid              = cmdBufInfo.dfSpmTraceBegin;
                perSubQueueInfo.pCmdBufInfoList = &cmdBufInfo;

                result = InternalSubmit(nextSubmitInfo, false);
            }
        }

    }
    else
    {
        // Make sure that all the log items have been logged before resetting the device clock mode. Resetting the clock
        // mode before all gpu workload has been finished results in incorrect perf counter results in gfx-9 and above.
        if (m_logItems.NumElements() == 0)
        {
            result = m_pDevice->ProfilingClockMode(false);
        }
    }

    return result;
}

// =====================================================================================================================
// When the GPU profiler layer is active, the submitted command buffers are really just tokenized ICmdBuffer calls.
// Now, at submit time, we actually generate submittable command buffers (possibly with additional commands to
// gather performance data).
Result Queue::Submit(
    const MultiSubmitInfo& submitInfo)
{
    PAL_ASSERT(submitInfo.perSubQueueInfoCount > 0);

    LogQueueCall(QueueCallId::Submit);

    Result     result        = Result::Success;
    auto*const pPlatform     = m_pDevice->GetPlatform();
    bool       beginNewFrame = false;

    uint32 cmdBufferCount = 0;
    uint32 numPresentCmdBuf = 0;

    for (uint32 i = 0; i < submitInfo.perSubQueueInfoCount; i++)
    {
        cmdBufferCount += submitInfo.pPerSubQueueInfo[i].cmdBufferCount;
        for (uint32 j = 0; j < submitInfo.pPerSubQueueInfo[i].cmdBufferCount; j++)
        {
            auto*const pRecordedCmdBuffer =
                static_cast<CmdBuffer*>(submitInfo.pPerSubQueueInfo[i].ppCmdBuffers[j]);
            // Detect a DX12 app has issues a present that will end a logged frame.
            // We'll add an internal command buffer to end the current frame-long performance experiment.
            if (pRecordedCmdBuffer->ContainsPresent() &&
                m_pDevice->LoggingEnabled(GpuProfilerGranularityFrame) &&
                (m_perFrameLogItem.pGpaSession != nullptr))
            {
                cmdBufferCount += 1;
                numPresentCmdBuf++;
                // We add another one to copy the DF SPM data
                if (m_pDevice->NumDfStreamingPerfCounters() > 0)
                {
                    cmdBufferCount += 1;
                }
            }
            else if (m_pDevice->LoggingEnabled(GpuProfilerGranularityCmdBuf) &&
                     (m_pDevice->NumDfStreamingPerfCounters() > 0) &&
                     (m_pQueueInfos[i].queueType == QueueType::QueueTypeUniversal ||
                      m_pQueueInfos[i].queueType == QueueType::QueueTypeCompute))
            {
                // For every DF SPM trace on a valid queue we will add a cmd buffer
                // to copy the results.
                cmdBufferCount += 1;
            }
        }
    }
    PAL_ASSERT(numPresentCmdBuf <= 1);

    AutoBuffer<PerSubQueueSubmitInfo, 64, PlatformDecorator> nextPerSubQueueInfos(
                                                    submitInfo.perSubQueueInfoCount, pPlatform);
    AutoBuffer<PerSubQueueSubmitInfo, 64, PlatformDecorator> nextPerSubQueueInfosBreakBatch(
                                                    submitInfo.perSubQueueInfoCount, pPlatform);
    AutoBuffer<ICmdBuffer*, 64, PlatformDecorator> nextCmdBuffers(Max(cmdBufferCount, 1u), pPlatform);
    AutoBuffer<CmdBufInfo, 64, PlatformDecorator>  nextCmdBufInfoList(Max(cmdBufferCount, 1u), pPlatform);

    bool breakBatches = m_pDevice->GetPlatform()->PlatformSettings().gpuProfilerConfig.breakSubmitBatches;

    AutoBuffer<GpuMemoryRef, 32, PlatformDecorator> nextGpuMemoryRefs(submitInfo.gpuMemRefCount, pPlatform);
    AutoBuffer<DoppRef,      32, PlatformDecorator> nextDoppRefs(submitInfo.doppRefCount, pPlatform);
    AutoBuffer<IFence*,      32, PlatformDecorator> nextFences(submitInfo.fenceCount, pPlatform);

    if ((nextCmdBuffers.Capacity()     < cmdBufferCount)            ||
        (nextCmdBufInfoList.Capacity() < cmdBufferCount)            ||
        (nextDoppRefs.Capacity()       < submitInfo.doppRefCount)   ||
        (nextGpuMemoryRefs.Capacity()  < submitInfo.gpuMemRefCount) ||
        (nextFences.Capacity()         < submitInfo.fenceCount))
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        for (uint32 i = 0; i < submitInfo.gpuMemRefCount; i++)
        {
            nextGpuMemoryRefs[i].pGpuMemory = NextGpuMemory(submitInfo.pGpuMemoryRefs[i].pGpuMemory);
            nextGpuMemoryRefs[i].flags.u32All = submitInfo.pGpuMemoryRefs[i].flags.u32All;
        }

        for (uint32 i = 0; i < submitInfo.doppRefCount; i++)
        {
            nextDoppRefs[i].pGpuMemory = NextGpuMemory(submitInfo.pDoppRefs[i].pGpuMemory);
            nextDoppRefs[i].flags.u32All = submitInfo.pDoppRefs[i].flags.u32All;
        }

        const IGpuMemory* pNextBlockIfFlipping[MaxBlockIfFlippingCount] = {};
        PAL_ASSERT(submitInfo.blockIfFlippingCount <= MaxBlockIfFlippingCount);

        for (uint32 i = 0; i < submitInfo.blockIfFlippingCount; i++)
        {
            pNextBlockIfFlipping[i] = NextGpuMemory(submitInfo.ppBlockIfFlipping[i]);
        }

        for (uint32 i = 0; i < submitInfo.fenceCount; i++)
        {
            nextFences[i] = NextFence(submitInfo.ppFences[i]);
        }

        uint32          globalCmdBufIdx     = 0;
        uint32          globalCmdBufInfoIdx = 0;

        memset(nextPerSubQueueInfos.Data(), 0, sizeof(PerSubQueueSubmitInfo) * submitInfo.perSubQueueInfoCount);
        memset(nextPerSubQueueInfosBreakBatch.Data(),
               0,
               sizeof(PerSubQueueSubmitInfo) * submitInfo.perSubQueueInfoCount);
        memset(nextCmdBufInfoList.Data(), 0, sizeof(CmdBufInfo) * Max(cmdBufferCount, 1u));

        MultiSubmitInfo nextSubmitInfo      = {};
        nextSubmitInfo.gpuMemRefCount       = submitInfo.gpuMemRefCount;
        nextSubmitInfo.pGpuMemoryRefs       = &nextGpuMemoryRefs[0];
        nextSubmitInfo.doppRefCount         = submitInfo.doppRefCount;
        nextSubmitInfo.pDoppRefs            = &nextDoppRefs[0];
        nextSubmitInfo.blockIfFlippingCount = submitInfo.blockIfFlippingCount;
        nextSubmitInfo.ppBlockIfFlipping    = &pNextBlockIfFlipping[0];
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 764
        nextSubmitInfo.pFreeMuxMemory       = NextGpuMemory(submitInfo.pFreeMuxMemory);
#endif

        // In most cases, we want to release all newly acquired objects with each submit, since they are only used
        // by one command buffer.  However, when doing frame-granularity captures, we can't release resources used
        // for pending experiments until the entire frame is complete.  In that case, we will delay setting release
        // objects until the next present.
        bool releaseObjectsBreakBatch = (m_pDevice->LoggingEnabled(GpuProfilerGranularityFrame) == false);
        bool releaseObjects = (m_pDevice->LoggingEnabled(GpuProfilerGranularityFrame) == false);
        for (uint32 subQueueIdx = 0;
             ((subQueueIdx < submitInfo.perSubQueueInfoCount) && (result == Result::Success));
             subQueueIdx++)
        {
            const PerSubQueueSubmitInfo& origSubQueueInfo  = submitInfo.pPerSubQueueInfo[subQueueIdx];
            PerSubQueueSubmitInfo*       pNextSubQueueInfo = &nextPerSubQueueInfos[subQueueIdx];

            uint32 localCmdBufIdx = 0;
            uint32 localCmdBufInfoIdx = 0;

            if (origSubQueueInfo.cmdBufferCount > 0)
            {
                releaseObjectsBreakBatch        = (m_pDevice->LoggingEnabled(GpuProfilerGranularityFrame) == false);
                pNextSubQueueInfo->ppCmdBuffers = &nextCmdBuffers[globalCmdBufIdx];

                // Need to add CmdBufInfo list if Df Streaming perf counters exist to enable and disable them
                if ((origSubQueueInfo.pCmdBufInfoList != nullptr) ||
                    ((m_pDevice->NumDfStreamingPerfCounters() > 0) && m_pDevice->LoggingEnabled()))
                {
                    pNextSubQueueInfo->pCmdBufInfoList = &nextCmdBufInfoList[globalCmdBufInfoIdx];
                }

                for (uint32 i = 0; ((i < origSubQueueInfo.cmdBufferCount) && (result == Result::Success)); i++)
                {
                    bool needDfSpmFlags = false;
                    bool needPresent    = false;
                    // Get an available queue-owned command buffer for this recorded command buffer.
                    auto*const pRecordedCmdBuffer =
                        static_cast<CmdBuffer*>(submitInfo.pPerSubQueueInfo[subQueueIdx].ppCmdBuffers[i]);

                    // Detect a DX12 app has issues a present that will end a logged frame.
                    if (pRecordedCmdBuffer->ContainsPresent() &&
                        m_pDevice->LoggingEnabled(GpuProfilerGranularityFrame) &&
                        (m_perFrameLogItem.pGpaSession != nullptr))
                    {
                        // Submit an internal command buffer to end the current frame-long performance experiment.
                        TargetCmdBuffer*const pEndFrameTgtCmdBuf = AcquireCmdBuf(0, false);

                        CmdBufferBuildInfo buildInfo = { };
                        pEndFrameTgtCmdBuf->Begin(NextCmdBufferBuildInfo(buildInfo));
                        pEndFrameTgtCmdBuf->EndSample(this, &m_perFrameLogItem);
                        pEndFrameTgtCmdBuf->EndGpaSession(&m_perFrameLogItem);
                        pEndFrameTgtCmdBuf->End();

                        if (m_pDevice->NumDfStreamingPerfCounters() > 0)
                        {
                            nextCmdBufInfoList[globalCmdBufInfoIdx + localCmdBufInfoIdx].isValid           = 1;
                            nextCmdBufInfoList[globalCmdBufInfoIdx + localCmdBufInfoIdx].dfSpmTraceEnd     = 1;
                            localCmdBufInfoIdx++;
                        }
                        else if (origSubQueueInfo.pCmdBufInfoList != nullptr)
                        {
                            // We need to insert a dummy CmdBufInfo if any caller command buffers specify a CmdBufInfo.
                            nextCmdBufInfoList[globalCmdBufInfoIdx + localCmdBufInfoIdx].isValid = 0;
                            localCmdBufInfoIdx++;
                        }

                        nextCmdBuffers[globalCmdBufIdx + localCmdBufIdx] = NextCmdBuffer(pEndFrameTgtCmdBuf);
                        localCmdBufIdx++;

                        // We need a separate command buffer to copy the DF SPM trace data into the GpaSession result
                        // buffer.
                        if (m_pDevice->NumDfStreamingPerfCounters() > 0)
                        {
                            AddDfSpmEndCmdBuffer(&nextCmdBuffers,
                                                 &nextCmdBufInfoList,
                                                 subQueueIdx,
                                                 globalCmdBufIdx,
                                                 &localCmdBufIdx,
                                                 globalCmdBufInfoIdx,
                                                 &localCmdBufInfoIdx,
                                                 m_perFrameLogItem);

                            m_isDfSpmTraceEnabled = false;
                        }

                        AddLogItem(m_perFrameLogItem);
                        releaseObjectsBreakBatch = true;
                        releaseObjects = true;
                        needPresent    = true;
                    }

                    TargetCmdBuffer*const pTargetCmdBuffer = AcquireCmdBuf(subQueueIdx, false);
                    pTargetCmdBuffer->SetClientData(pRecordedCmdBuffer->GetClientData());

                    // For the submit call, we need to make sure this array entry points to the next level ICmdBuffer.
                    nextCmdBuffers[globalCmdBufIdx + localCmdBufIdx] = NextCmdBuffer(pTargetCmdBuffer);
                    localCmdBufIdx++;

                    // Save this index so that we can match the command buffer info that we create after we add
                    // the split command buffers to this command buffer.
                    uint32 saveLocalCmdBufInfoIdx = localCmdBufInfoIdx;
                    localCmdBufInfoIdx++;

                    // Replay the client-specified command buffer commands into the queue-owned command buffer.
                    result = pRecordedCmdBuffer->Replay(this,
                                                        pTargetCmdBuffer,
                                                        static_cast<Platform*>(m_pDevice->GetPlatform())->FrameId());

                    // After we're done replaying we need an extra command buffer to copy the DF SPM trace data
                    // to the GpaSession result buffer
                    if (m_pDevice->LoggingEnabled(GpuProfilerGranularityCmdBuf) &&
                        (m_pDevice->NumDfStreamingPerfCounters() > 0))
                    {
                        LogItem recordedCmdBufLogItem = pRecordedCmdBuffer->GetCmdBufLogItem();
                        AddDfSpmEndCmdBuffer(&nextCmdBuffers,
                                             &nextCmdBufInfoList,
                                             subQueueIdx,
                                             globalCmdBufIdx,
                                             &localCmdBufIdx,
                                             globalCmdBufInfoIdx,
                                             &localCmdBufInfoIdx,
                                             recordedCmdBufLogItem);

                        needDfSpmFlags = true;
                    }

                    nextPerSubQueueInfosBreakBatch[subQueueIdx].cmdBufferCount = needPresent ? 2 : 1;
                    nextPerSubQueueInfosBreakBatch[subQueueIdx].ppCmdBuffers = &nextCmdBuffers[
                        globalCmdBufIdx + localCmdBufIdx - nextPerSubQueueInfosBreakBatch[subQueueIdx].cmdBufferCount];

                    // If there's a DF SPM trace then we need to check if we should add that info to the CmdBufInfo
                    // list as well.
                    if ((origSubQueueInfo.pCmdBufInfoList != nullptr)  ||
                        ((m_pDevice->NumDfStreamingPerfCounters() > 0) &&
                         m_pDevice->LoggingEnabled()))
                    {
                        CmdBufInfo* pNextCmdBufInfoList =
                                &nextCmdBufInfoList[globalCmdBufInfoIdx + saveLocalCmdBufInfoIdx];

                        if (origSubQueueInfo.pCmdBufInfoList != nullptr)
                        {
                            pNextCmdBufInfoList->u32All = origSubQueueInfo.pCmdBufInfoList[i].u32All;

                            if (pNextCmdBufInfoList->isValid)
                            {
                                pNextCmdBufInfoList->pPrimaryMemory =
                                    NextGpuMemory(origSubQueueInfo.pCmdBufInfoList[i].pPrimaryMemory);

                                if ((pNextCmdBufInfoList->captureBegin) || (pNextCmdBufInfoList->captureEnd))
                                {
                                    pNextCmdBufInfoList->pDirectCapMemory =
                                        NextGpuMemory(origSubQueueInfo.pCmdBufInfoList[i].pDirectCapMemory);

                                    if (pNextCmdBufInfoList->privateFlip)
                                    {
                                        pNextCmdBufInfoList->pPrivFlipMemory =
                                            NextGpuMemory(origSubQueueInfo.pCmdBufInfoList[i].pPrivFlipMemory);
                                    }
                                    pNextCmdBufInfoList->frameIndex = origSubQueueInfo.pCmdBufInfoList[i].frameIndex;
                                }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 779
                                pNextCmdBufInfoList->pEarlyPresentEvent =
                                    origSubQueueInfo.pCmdBufInfoList[i].pEarlyPresentEvent;
#endif
                            }
                        }

                        // Check if we need to add DF SPM info as well
                        if (needDfSpmFlags)
                        {
                            nextCmdBufInfoList[globalCmdBufInfoIdx + saveLocalCmdBufInfoIdx].isValid           = 1;
                            nextCmdBufInfoList[globalCmdBufInfoIdx + saveLocalCmdBufInfoIdx].dfSpmTraceBegin   = 1;
                            nextCmdBufInfoList[globalCmdBufInfoIdx + saveLocalCmdBufInfoIdx].dfSpmTraceEnd     = 1;
                        }

                        nextPerSubQueueInfosBreakBatch[subQueueIdx].pCmdBufInfoList =
                            &nextCmdBufInfoList[globalCmdBufIdx +
                                                localCmdBufIdx  -
                                                nextPerSubQueueInfosBreakBatch[subQueueIdx].cmdBufferCount];
                    }

                    // DX12 apps request a present via a command buffer call. If this command buffer includes one,
                    // increment the frame ID. It is expected that only the last command buffer in a submit would
                    // request a present.
                    if (pRecordedCmdBuffer->ContainsPresent())
                    {
                        PAL_ASSERT(i == (submitInfo.pPerSubQueueInfo[subQueueIdx].cmdBufferCount - 1));
                        static_cast<Platform*>(m_pDevice->GetPlatform())->IncrementFrameId();
                        beginNewFrame = true;
                    }

                    if ((result == Result::Success) && breakBatches)
                    {
                        // Only pass the client fence on to the next layer if this is the last batch, so that it
                        // will only be signaled once all work the client specified in this submit has completed.
                        bool passFence = ((subQueueIdx == submitInfo.perSubQueueInfoCount - 1) &&
                                          (i == origSubQueueInfo.cmdBufferCount - 1));
                        // Include all of subQueueInfos preceding current subQueue,
                        // but the contents of preceding subQueue is cleared.
                        nextSubmitInfo.perSubQueueInfoCount = subQueueIdx + 1;
                        nextSubmitInfo.pPerSubQueueInfo     = &nextPerSubQueueInfosBreakBatch[0];
                        nextSubmitInfo.ppFences = passFence ? &nextFences[0] : nullptr;
                        nextSubmitInfo.fenceCount = passFence ? submitInfo.fenceCount : 0;

                        result = InternalSubmit(nextSubmitInfo, releaseObjectsBreakBatch);
                    }
                } //  end of traversing each cmdBuf in a perSubQueueInfo

                pNextSubQueueInfo->cmdBufferCount = localCmdBufIdx;
                globalCmdBufIdx                  += localCmdBufIdx;
                globalCmdBufInfoIdx              += localCmdBufInfoIdx;
            } // end of if branch

            // clear contents of this perSubQueueInfo.
            nextPerSubQueueInfosBreakBatch[subQueueIdx].cmdBufferCount  = 0;
            nextPerSubQueueInfosBreakBatch[subQueueIdx].ppCmdBuffers    = nullptr;
            nextPerSubQueueInfosBreakBatch[subQueueIdx].pCmdBufInfoList = nullptr;
        } // end of traversing each perSubQueueInfo

        if ((result == Result::Success) && (breakBatches == false))
        {
            // Make sure we didn't overflow the next arrays.
            PAL_ASSERT((globalCmdBufIdx == cmdBufferCount) && (globalCmdBufInfoIdx <= cmdBufferCount));
            nextSubmitInfo.perSubQueueInfoCount = submitInfo.perSubQueueInfoCount;
            nextSubmitInfo.pPerSubQueueInfo     = &nextPerSubQueueInfos[0];
            nextSubmitInfo.ppFences   = &nextFences[0];
            nextSubmitInfo.fenceCount = submitInfo.fenceCount;

            result = InternalSubmit(nextSubmitInfo, releaseObjects);
        }
    }

    if (beginNewFrame)
    {
        if (result == Result::Success)
        {
            // Begin sampling setup work for next frame for dx path only.
            result = BeginNextFrame(m_pDevice->LoggingEnabled());
        }
    }
    else if (m_pDevice->LoggingEnabled(GpuProfilerGranularityFrame) == false)
    {
        // Try to reclaim any newly idle allocations on each submit, unless we're doing a per-frame trace, in which
        // case we don't want to let CPU utilization, disk I/O, etc. of this to starve the GPU.
        ProcessIdleSubmits();
    }

    return result;
}

// =====================================================================================================================
// Helper function to add an extra command buffer that copies the DF SPM trace data
void Queue::AddDfSpmEndCmdBuffer(
    AutoBuffer<ICmdBuffer*, 64, PlatformDecorator>* pNextCmdBuffers,
    AutoBuffer<CmdBufInfo, 64, PlatformDecorator>*  pNextCmdBufferInfos,
    uint32                                          subQueueIdx,
    uint32                                          globalCmdBufIdx,
    uint32*                                         pLocalCmdBufIdx,
    uint32                                          globalCmdBufInfoIdx,
    uint32*                                         pLocalCmdBufInfoIdx,
    const LogItem&                                  logItem)
{
    TargetCmdBuffer* const pEndDfSpmCmdBuf = AcquireCmdBuf(subQueueIdx, false);
    RecordDfSpmEndCmdBuffer(pEndDfSpmCmdBuf, logItem);

    (*pNextCmdBuffers)[globalCmdBufIdx + *pLocalCmdBufIdx] = NextCmdBuffer(pEndDfSpmCmdBuf);
    (*pLocalCmdBufIdx)++;

    (*pNextCmdBufferInfos)[globalCmdBufInfoIdx + *pLocalCmdBufInfoIdx].isValid = 0;
    (*pLocalCmdBufInfoIdx)++;
}

// =====================================================================================================================
// Helper function to record commands that copy the DF SPM trace data
void Queue::RecordDfSpmEndCmdBuffer(
    TargetCmdBuffer* pEndDfSpmCmdBuf,
    const LogItem&   logItem)
{
    CmdBufferBuildInfo buildInfo = { };
    pEndDfSpmCmdBuf->Begin(buildInfo);
    pEndDfSpmCmdBuf->EndDfSpmTraceSession(this, &logItem);
    pEndDfSpmCmdBuf->End();
}

// =====================================================================================================================
// Log the WaitIdle call and pass it to the next layer.
Result Queue::WaitIdle()
{
    LogQueueCall(QueueCallId::WaitIdle);

    return QueueDecorator::WaitIdle();
}

// =====================================================================================================================
// Log the SignalQueueSemaphore call and pass it to the next layer.
Result Queue::SignalQueueSemaphore(
    IQueueSemaphore* pQueueSemaphore,
    uint64           value)
{
    LogQueueCall(QueueCallId::SignalQueueSemaphore);

    return QueueDecorator::SignalQueueSemaphore(pQueueSemaphore, value);
}

// =====================================================================================================================
// Log the WaitQueueSemaphore call and pass it to the next layer.
Result Queue::WaitQueueSemaphore(
    IQueueSemaphore* pQueueSemaphore,
    uint64           value)
{
    LogQueueCall(QueueCallId::WaitQueueSemaphore);

    return QueueDecorator::WaitQueueSemaphore(pQueueSemaphore, value);
}

// =====================================================================================================================
// Log the PresentDirect call and pass it to the next layer.
Result Queue::PresentDirect(
    const PresentDirectInfo& presentInfo)
{
    LogQueueCall(QueueCallId::PresentDirect);

    // Do the present before ending any per-frame experiments so that they will capture any present-time GPU work.
    Result result = QueueDecorator::PresentDirect(presentInfo);

    if ((result == Result::Success)                            &&
        m_pDevice->LoggingEnabled(GpuProfilerGranularityFrame) &&
        (m_perFrameLogItem.pGpaSession != nullptr))
    {
        // Submit an internal command buffer to end the current frame-long performance experiment.
        TargetCmdBuffer*const pEndFrameTgtCmdBuf = AcquireCmdBuf(0, false);
        CmdBufferBuildInfo    buildInfo = {};

        result = pEndFrameTgtCmdBuf->Begin(NextCmdBufferBuildInfo(buildInfo));

        if (result == Result::Success)
        {
            pEndFrameTgtCmdBuf->EndSample(this, &m_perFrameLogItem);

            result = pEndFrameTgtCmdBuf->EndGpaSession(&m_perFrameLogItem);
        }

        if (result == Result::Success)
        {
            result = pEndFrameTgtCmdBuf->End();
        }

        if (result == Result::Success)
        {
            ICmdBuffer* pNextCmdBuf = NextCmdBuffer(pEndFrameTgtCmdBuf);
            PerSubQueueSubmitInfo perSubQueueInfo = {};
            perSubQueueInfo.cmdBufferCount        = 1;
            perSubQueueInfo.ppCmdBuffers          = &pNextCmdBuf;
            MultiSubmitInfo nextSubmitInfo        = {};
            nextSubmitInfo.perSubQueueInfoCount   = 1;
            nextSubmitInfo.pPerSubQueueInfo       = &perSubQueueInfo;

            CmdBufInfo cmdBufInfo = {};
            RecordDfSpmEndCmdBufInfo(&cmdBufInfo);
            perSubQueueInfo.pCmdBufInfoList = &cmdBufInfo;

            AddLogItem(m_perFrameLogItem);

            result = InternalSubmit(nextSubmitInfo, true);

            if (result == Result::Success)
            {
                result = EndDfSpm();
            }
        }
    }

    static_cast<Platform*>(m_pDevice->GetPlatform())->IncrementFrameId();

    // Begin sampling setup for next frame.
    if (result == Result::Success)
    {
        result = BeginNextFrame(m_pDevice->LoggingEnabled());
    }

    return result;
}

// =====================================================================================================================
Result Queue::EndDfSpm()
{
    Result result = Result::Success;
    if (m_pDevice->NumDfStreamingPerfCounters() > 0)
    {
        TargetCmdBuffer* pEndDfSpmTgtCmdBuf = AcquireCmdBuf(0, false);
        RecordDfSpmEndCmdBuffer(pEndDfSpmTgtCmdBuf, m_perFrameLogItem);

        result = SubmitDfSpmEndCmdBuffer(pEndDfSpmTgtCmdBuf);
    }
    return result;
}

// =====================================================================================================================
// Helper function to fill out a CmdBufInfo object with info to end a DF SPM trace
void Queue::RecordDfSpmEndCmdBufInfo(
    CmdBufInfo* pCmdBufInfo)
{
    if (m_pDevice->NumDfStreamingPerfCounters() > 0)
    {
        m_isDfSpmTraceEnabled    = false;
        pCmdBufInfo->dfSpmTraceEnd = 1;
        pCmdBufInfo->isValid       = 1;
    }
}

// =====================================================================================================================
// Helper function to submit the end DF SPM command buffer
Result Queue::SubmitDfSpmEndCmdBuffer(
    TargetCmdBuffer* pEndDfSpmTgtCmdBuf)
{
    ICmdBuffer* pEndDfSpmCmdBuf = NextCmdBuffer(pEndDfSpmTgtCmdBuf);
    PerSubQueueSubmitInfo dfSpmPerSubQueueInfo = {};
    dfSpmPerSubQueueInfo.cmdBufferCount        = 1;
    dfSpmPerSubQueueInfo.ppCmdBuffers          = &pEndDfSpmCmdBuf;
    MultiSubmitInfo endDfSpmSubmitInfo         = {};
    endDfSpmSubmitInfo.perSubQueueInfoCount    = 1;
    endDfSpmSubmitInfo.pPerSubQueueInfo        = &dfSpmPerSubQueueInfo;

    return InternalSubmit(endDfSpmSubmitInfo, true);
}

// =====================================================================================================================
// Log the PresentSwapChain call and pass it to the next layer.
Result Queue::PresentSwapChain(
    const PresentSwapChainInfo& presentInfo)
{
    LogQueueCall(QueueCallId::PresentSwapChain);

    // Do the present before ending any per-frame experiments so that they will capture any present-time GPU work.
    // Note: We must always call down to the next layer because we must release ownership of the image index.
    Result result = QueueDecorator::PresentSwapChain(presentInfo);

    if ((result == Result::Success)                            &&
        m_pDevice->LoggingEnabled(GpuProfilerGranularityFrame) &&
        (m_perFrameLogItem.pGpaSession != nullptr))
    {
        // Submit an internal command buffer to end the current frame-long performance experiment.
        TargetCmdBuffer*const pEndFrameTgtCmdBuf = AcquireCmdBuf(0, false);
        CmdBufferBuildInfo    buildInfo = {};

        result = pEndFrameTgtCmdBuf->Begin(NextCmdBufferBuildInfo(buildInfo));

        if (result == Result::Success)
        {
            pEndFrameTgtCmdBuf->EndSample(this, &m_perFrameLogItem);

            result = pEndFrameTgtCmdBuf->EndGpaSession(&m_perFrameLogItem);
        }

        if (result == Result::Success)
        {
            result = pEndFrameTgtCmdBuf->End();
        }

        if (result == Result::Success)
        {
            ICmdBuffer* pNextCmdBuf        = NextCmdBuffer(pEndFrameTgtCmdBuf);

            PerSubQueueSubmitInfo perSubQueueInfo = {};
            perSubQueueInfo.cmdBufferCount        = 1;
            perSubQueueInfo.ppCmdBuffers          = &pNextCmdBuf;
            MultiSubmitInfo nextSubmitInfo        = {};
            nextSubmitInfo.perSubQueueInfoCount   = 1;
            nextSubmitInfo.pPerSubQueueInfo       = &perSubQueueInfo;

            CmdBufInfo cmdBufInfo = {};
            RecordDfSpmEndCmdBufInfo(&cmdBufInfo);
            perSubQueueInfo.pCmdBufInfoList = &cmdBufInfo;

            AddLogItem(m_perFrameLogItem);

            result = InternalSubmit(nextSubmitInfo, true);

            if (result == Result::Success)
            {
                result = EndDfSpm();
            }
        }
    }

    static_cast<Platform*>(m_pDevice->GetPlatform())->IncrementFrameId();

    // Begin sampling setup for next frame.
    if (result == Result::Success)
    {
        result = BeginNextFrame(m_pDevice->LoggingEnabled());
    }

    return result;
}

// =====================================================================================================================
// Log the Delay call and pass it to the next layer.
Result Queue::Delay(
    float delay)
{
    LogQueueCall(QueueCallId::Delay);

    return QueueDecorator::Delay(delay);
}

// =====================================================================================================================
// Log the RemapVirtualMemoryPages call and pass it to the next layer.
Result Queue::RemapVirtualMemoryPages(
    uint32                         rangeCount,
    const VirtualMemoryRemapRange* pRanges,
    bool                           doNotWait,
    IFence*                        pFence)
{
    LogQueueCall(QueueCallId::RemapVirtualMemoryPages);

    return QueueDecorator::RemapVirtualMemoryPages(rangeCount, pRanges, doNotWait, pFence);
}

// =====================================================================================================================
// Log the CopyVirtualMemoryPageMappings call and pass it to the next layer.
Result Queue::CopyVirtualMemoryPageMappings(
    uint32                                    rangeCount,
    const VirtualMemoryCopyPageMappingsRange* pRanges,
    bool                                      doNotWait)
{
    LogQueueCall(QueueCallId::CopyVirtualMemoryPageMappings);

    return QueueDecorator::CopyVirtualMemoryPageMappings(rangeCount, pRanges, doNotWait);
}

// =====================================================================================================================
// Acquires a queue-owned command buffer for submission of a replayed client command buffer.
TargetCmdBuffer* Queue::AcquireCmdBuf(
    uint32 subQueueIdx,
    bool   nested)
{
    const SubQueueInfo& subQueueInfo = m_pQueueInfos[subQueueIdx];

    CmdBufDeque* pAvailable = nested ? subQueueInfo.pAvailableNestedCmdBufs : subQueueInfo.pAvailableCmdBufs;
    CmdBufDeque* pBusy      = nested ? subQueueInfo.pBusyNestedCmdBufs      : subQueueInfo.pBusyCmdBufs;

    TargetCmdBuffer* pCmdBuffer = nullptr;

    if (pAvailable->NumElements() > 0)
    {
        // Use an idle command buffer from the pool if available.
        pAvailable->PopFront(&pCmdBuffer);
    }
    else
    {
        // No command buffers are currently idle (or possibly none exist at all) - allocate a new command buffer.  Note
        // that we create a GpuProfiler::TargetCmdBuffer here, not a GpuProfiler::CmdBuffer which would just record our
        // commands again!
        CmdBufferCreateInfo createInfo = { };
        createInfo.pCmdAllocator = nested ? m_pNestedCmdAllocator : m_pCmdAllocator;
        createInfo.queueType     = subQueueInfo.queueType;
        createInfo.engineType    = subQueueInfo.engineType;
        createInfo.flags.nested  = nested;

        void* pMemory = PAL_MALLOC(m_pDevice->GetTargetCmdBufferSize(createInfo, nullptr),
                                   m_pDevice->GetPlatform(),
                                   AllocInternal);

        if (pMemory != nullptr)
        {
            Result result = m_pDevice->CreateTargetCmdBuffer(createInfo, pMemory, &pCmdBuffer, subQueueIdx);

            if (result != Result::Success)
            {
                PAL_SAFE_FREE(pMemory, m_pDevice->GetPlatform());
            }
        }
    }

    // We always submit command buffers in the order they are acquired, so we can go ahead and add this to the busy
    // queue immediately.
    PAL_ASSERT(pCmdBuffer != nullptr);
    pBusy->PushBack(pCmdBuffer);
    if (nested)
    {
        m_nextSubmitInfo.pNestedCmdBufCount[subQueueIdx]++;
    }
    else
    {
        m_nextSubmitInfo.pCmdBufCount[subQueueIdx]++;
    }

    return pCmdBuffer;
}

// =====================================================================================================================
// Acquires a queue-owned GPA session based on the device's performance experiment requests.
Result Queue::AcquireGpaSession(
    GpuUtil::GpaSession** ppGpaSession)
{
    Result result = Result::Success;

    // A session is acquired from either available list or newly-created
    if (m_availableGpaSessions.NumElements() > 0)
    {
        // Use an idle session if available.
        result = m_availableGpaSessions.PopFront(ppGpaSession);
    }
    else
    {
        const auto& platform = *static_cast<const Platform*>(m_pDevice->GetPlatform());
        // GpuProfiler shouldn't insert rgpInstrumentationVer value, though it's fine set it zero for now.
        // Will need to change later if RGP is uncomfortable with it.
        *ppGpaSession        = PAL_NEW(GpuUtil::GpaSession,
                                       m_pDevice->GetPlatform(),
                                       SystemAllocType::AllocObject)
                                      (m_pDevice->GetPlatform(),
                                       m_pDevice,
                                       platform.ApiMajorVer(),
                                       platform.ApiMinorVer(),
                                       GpuUtil::ApiType::Generic,
                                       0, 0,
                                       &m_availPerfExpMem);
        if (*ppGpaSession != nullptr)
        {
            result = (*ppGpaSession)->Init();
            if (result != Result::Success)
            {
                PAL_SAFE_DELETE(*ppGpaSession, m_pDevice->GetPlatform());
            }
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    if (result == Result::Success)
    {
        result = m_busyGpaSessions.PushBack(*ppGpaSession);
    }

    if (result == Result::Success)
    {
        m_nextSubmitInfo.gpaSessionCount++;
    }

    return result;
}

// =====================================================================================================================
// Acquires a queue-owned fence.
IFence* Queue::AcquireFence()
{
    IFence* pFence = nullptr;

    if (m_availableFences.NumElements() > 0)
    {
        // Use an idle fence from the pool if available.
        m_availableFences.PopFront(&pFence);
    }
    else
    {
        // No fences are currently idle (or possibly none exist at all) - allocate a new fence.
        void* pMemory = PAL_MALLOC(m_pDevice->GetFenceSize(nullptr), m_pDevice->GetPlatform(), AllocInternal);

        if (pMemory != nullptr)
        {
            Pal::FenceCreateInfo createInfo = {};
            Result result = m_pDevice->CreateFence(createInfo, pMemory, &pFence);
            if (result != Result::Success)
            {
                PAL_SAFE_FREE(pMemory, m_pDevice->GetPlatform());
            }
        }
    }

    PAL_ASSERT(pFence != nullptr);
    return pFence;
}

// =====================================================================================================================
// Determine if any pending submits have completed, and perform accounting on busy/idle command buffers and fences.
void Queue::ProcessIdleSubmits()
{
    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());
    while ((m_pendingSubmits.NumElements() > 0) &&
           (m_pendingSubmits.Front().pFence->GetStatus() == Result::Success))
    {
        PendingSubmitInfo submitInfo = { };
        m_pendingSubmits.PopFront(&submitInfo);

        // Output items from the log item queue that are now known to be idle.
        OutputLogItemsToFile(submitInfo.logItemCount, submitInfo.hasDrawOrDispatch);

        PAL_ASSERT((submitInfo.pCmdBufCount != nullptr) && (submitInfo.pNestedCmdBufCount != nullptr));

        for (uint32 qIdx = 0; qIdx < m_queueCount; qIdx++)
        {
            int cmdBufCnt = submitInfo.pCmdBufCount[qIdx];
            for (uint32 i = 0; i < submitInfo.pCmdBufCount[qIdx]; i++)
            {
                TargetCmdBuffer* pCmdBuffer = nullptr;
                m_pQueueInfos[qIdx].pBusyCmdBufs->PopFront(&pCmdBuffer);

                pCmdBuffer->SetClientData(nullptr);
                Result result = pCmdBuffer->Reset(nullptr, true);
                PAL_ASSERT(result == Result::Success);

                m_pQueueInfos[qIdx].pAvailableCmdBufs->PushBack(pCmdBuffer);
            }

            for (uint32 i = 0; i < submitInfo.pNestedCmdBufCount[qIdx]; i++)
            {
                TargetCmdBuffer* pCmdBuffer = nullptr;
                m_pQueueInfos[qIdx].pBusyNestedCmdBufs->PopFront(&pCmdBuffer);

                pCmdBuffer->SetClientData(nullptr);
                Result result = pCmdBuffer->Reset(nullptr, true);
                PAL_ASSERT(result == Result::Success);

                m_pQueueInfos[qIdx].pAvailableNestedCmdBufs->PushBack(pCmdBuffer);
            }
        }
        PAL_SAFE_DELETE_ARRAY(submitInfo.pCmdBufCount, pPlatform);
        PAL_SAFE_DELETE_ARRAY(submitInfo.pNestedCmdBufCount, pPlatform);

        for (uint32 i = 0; i < submitInfo.gpaSessionCount; i++)
        {
            GpuUtil::GpaSession* pGpaSession = nullptr;
            m_busyGpaSessions.PopFront(&pGpaSession);
            pGpaSession->Reset();
            m_availableGpaSessions.PushBack(pGpaSession);
        }

        m_pDevice->ResetFences(1, &submitInfo.pFence);
        m_availableFences.PushBack(submitInfo.pFence);
    }
}

// =====================================================================================================================
// Adds an entry to the queue of logged calls to be processed and outputted.
void Queue::AddLogItem(
    const LogItem& logItem)
{
    m_logItems.PushBack(logItem);
    m_nextSubmitInfo.logItemCount++;

    if ((logItem.type == CmdBufferCall) &&
        (m_nextSubmitInfo.hasDrawOrDispatch == false))
    {
        switch (logItem.cmdBufCall.callId)
        {
        case CmdBufCallId::CmdDraw:
        case CmdBufCallId::CmdDrawOpaque:
        case CmdBufCallId::CmdDrawIndexed:
        case CmdBufCallId::CmdDrawIndirectMulti:
        case CmdBufCallId::CmdDrawIndexedIndirectMulti:
        case CmdBufCallId::CmdDispatch:
        case CmdBufCallId::CmdDispatchIndirect:
        case CmdBufCallId::CmdDispatchOffset:
        case CmdBufCallId::CmdDispatchMesh:
        case CmdBufCallId::CmdDispatchMeshIndirectMulti:
        case CmdBufCallId::CmdExecuteIndirectCmds:
        {
            m_nextSubmitInfo.hasDrawOrDispatch = true;
            break;
        }
        default:
            break;
        }
    }
}

// =====================================================================================================================
// Adds a log entry for the specified queue call.
void Queue::LogQueueCall(
    QueueCallId callId)
{
    if (m_pDevice->LoggingEnabled(GpuProfilerGranularityDraw) ||
        m_pDevice->LoggingEnabled(GpuProfilerGranularityCmdBuf))
    {
        LogItem logItem = { };
        logItem.type = QueueCall;
        logItem.frameId = static_cast<Platform*>(m_pDevice->GetPlatform())->FrameId();
        logItem.queueCall.callId = callId;
        AddLogItem(logItem);
    }
}

// =====================================================================================================================
// Build sample config data for the creation of GPA session per gpuProfilerCmdBuffer's request
Result Queue::BuildGpaSessionSampleConfig()
{
    const auto& settings = m_pDevice->GetPlatform()->PlatformSettings();

    const uint32 numCounters                             = m_pDevice->NumGlobalPerfCounters();
    const GpuProfiler::PerfCounter* pCounters            = m_pDevice->GlobalPerfCounters();

    const uint32 numSpmCountersRequested                 = m_pDevice->NumStreamingPerfCounters();
    const GpuProfiler::PerfCounter* pStreamingCounters   = m_pDevice->StreamingPerfCounters();

    const uint32 numDfSpmCountersRequested               = m_pDevice->NumDfStreamingPerfCounters();
    const GpuProfiler::PerfCounter* pDfStreamingCounters = m_pDevice->DfStreamingPerfCounters();

    m_gpaSessionSampleConfig.type = GpuUtil::GpaSampleType::None;

    if (numCounters > 0)
    {
        m_gpaSessionSampleConfig.type = GpuUtil::GpaSampleType::Cumulative;
    }
    else if (m_pDevice->GetProfilerMode() > GpuProfilerCounterAndTimingOnly)
    {
        m_gpaSessionSampleConfig.type = GpuUtil::GpaSampleType::Trace;
    }
    else
    {
        // GpuProfiler layer can choose Cumulative/Trace/None mode by setting up perfCounter info or SQTT info in the
        // panel. Timestamps will be collected as long as the engine that cmdbuf submits to supports timestamp,
        // no matter which mode is chosen. From GpaSession's perspective, timestamp & query are sample types too.
        // But in gpuProfiler, it's not this config info that controls whether to collect timestamp or query.
        // GpuProfiler has it's own logic to control.
        m_gpaSessionSampleConfig.type = GpuUtil::GpaSampleType::None;
    }

    m_gpaSessionSampleConfig.flags.sampleInternalOperations      = 1;
    m_gpaSessionSampleConfig.flags.cacheFlushOnCounterCollection =
        settings.gpuProfilerPerfCounterConfig.cacheFlushOnCounterCollection;

    m_gpaSessionSampleConfig.flags.sqShaderMask = 1;
    m_gpaSessionSampleConfig.sqShaderMask       = PerfShaderMaskAll;

#if PAL_BUILD_GFX11 && (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 750)
    m_gpaSessionSampleConfig.flags.sqWgpShaderMask = 1;
    m_gpaSessionSampleConfig.sqWgpShaderMask       = PerfShaderMaskAll;
#endif

    PerfExperimentProperties perfExpProps;
    Result result = m_pDevice->GetPerfExperimentProperties(&perfExpProps);

    if (result == Result::Success)
    {
        m_shaderEngineCount = perfExpProps.shaderEngineCount;

        if (m_gpaSessionSampleConfig.type == GpuUtil::GpaSampleType::Cumulative)
        {
            for (uint32 i = 0; i < numCounters; i++)
            {
                m_gpaSessionSampleConfig.perfCounters.numCounters += (pCounters[i].instanceMask == 0)
                                                                      ? pCounters[i].instanceCount
                                                                      : Util::CountSetBits(pCounters[i].instanceMask);
            }
            GpuUtil::PerfCounterId* pIds =
                static_cast<GpuUtil::PerfCounterId*>(
                    PAL_MALLOC(m_gpaSessionSampleConfig.perfCounters.numCounters * sizeof(GpuUtil::PerfCounterId),
                                                                m_pDevice->GetPlatform(),
                                                                AllocInternal));
            if (pIds != nullptr)
            {
                m_gpaSessionSampleConfig.perfCounters.pIds = pIds;
                uint32 counterIdx = 0;
                for (uint32 i = 0; i < numCounters; i++)
                {
                    GpuUtil::PerfCounterId counterInfo = {};
                    counterInfo.block   = pCounters[i].block;
                    counterInfo.eventId = pCounters[i].eventId;

                    if (pCounters[i].hasOptionalData)
                    {
                        if (counterInfo.block == GpuBlock::DfMall)
                        {
                            counterInfo.df.eventQualifier = pCounters[i].optionalData;
                        }
                        else if (counterInfo.block == GpuBlock::Umcch)
                        {
                            // Threshold   [12-bits]
                            counterInfo.umc.eventThreshold   = pCounters[i].optionalData & 0xFFF;
                            // ThresholdEn [2-bits] 0=disabled, 1=less than, 2=greater than
                            counterInfo.umc.eventThresholdEn = (pCounters[i].optionalData >> 12) & 0x3;
                            // Read/Write Mask [2-bits] 0=Read, 1=Write
                            counterInfo.umc.rdWrMask         = (pCounters[i].optionalData >> 14) & 0x3;
                        }
                    }

                    const uint64 instanceMask = pCounters[i].instanceMask;
                    for (uint32 j = 0; j < pCounters[i].instanceCount; j++)
                    {
                        if ((instanceMask == 0) || Util::BitfieldIsSet(instanceMask, j))
                        {
                            counterInfo.instance = pCounters[i].instanceId + j;
                            pIds[counterIdx++]   = counterInfo;
                        }
                    }
                }
                PAL_ASSERT(counterIdx == m_gpaSessionSampleConfig.perfCounters.numCounters);
            }
            else
            {
                result = Result::ErrorOutOfMemory;
            }
        }
        else if (m_gpaSessionSampleConfig.type == GpuUtil::GpaSampleType::Trace)
        {
            // Streaming performance counter trace config.
            if (numSpmCountersRequested > 0)
            {
                FillOutSpmGpaSessionSampleConfig(numSpmCountersRequested, pStreamingCounters, false);
            }

            if (numDfSpmCountersRequested > 0)
            {
                FillOutSpmGpaSessionSampleConfig(numDfSpmCountersRequested, pDfStreamingCounters, true);
            }

            // Thread trace specific config.
            m_gpaSessionSampleConfig.sqtt.flags.enable                   = m_pDevice->IsThreadTraceEnabled();
            m_gpaSessionSampleConfig.sqtt.seMask                         = m_pDevice->GetSeMask();
            m_gpaSessionSampleConfig.sqtt.gpuMemoryLimit                 = settings.gpuProfilerSqttConfig.bufferSize;
            m_gpaSessionSampleConfig.sqtt.flags.stallMode                = m_pDevice->GetSqttStallMode();
            m_gpaSessionSampleConfig.sqtt.flags.supressInstructionTokens = (settings.gpuProfilerSqttConfig.tokenMask !=
                                                                            0xFFFF);
        }
        else
        {
            PAL_ASSERT(m_gpaSessionSampleConfig.type == GpuUtil::GpaSampleType::None);
        }

        // Always set timestamp pipe-point in the config info.
        m_gpaSessionSampleConfig.timing.preSample  = HwPipeBottom;
        m_gpaSessionSampleConfig.timing.postSample = HwPipeBottom;
    }

    return result;
}

Result Queue::FillOutSpmGpaSessionSampleConfig(
    uint32              numSpmCountersRequested,
    const  PerfCounter* pStreamingCounters,
    bool                isDataFabric)
{
    Result result = Result::Success;
    const auto& settings = m_pDevice->GetPlatform()->PlatformSettings();

    uint32 numTotalInstances = 0;
    for (uint32 i = 0; i < numSpmCountersRequested; i++)
    {
        numTotalInstances += (pStreamingCounters[i].instanceMask == 0)
            ? pStreamingCounters[i].instanceCount
            : Util::CountSetBits(pStreamingCounters[i].instanceMask);
    }

    gpusize ringSizeInBytes;
    if (isDataFabric)
    {
        // DF SPM buffer size is specified in MB
        ringSizeInBytes = settings.gpuProfilerDfSpmConfig.dfSpmBufferSize * 0x100000;
    }
    else
    {
        ringSizeInBytes = settings.gpuProfilerSpmConfig.spmBufferSize;
    }

    if (ringSizeInBytes == 0)
    {
        switch (settings.gpuProfilerConfig.granularity)
        {
        case GpuProfilerGranularityDraw:
            ringSizeInBytes = 1_MiB;
            break;
        case GpuProfilerGranularityCmdBuf:
            ringSizeInBytes = 32_MiB;
            break;
        case GpuProfilerGranularityFrame:
            ringSizeInBytes = 128_MiB;
            break;
        default:
            break;
        }
    }

    // Each instance of the requested block is a unique perf counter according to GpaSession.
    if (isDataFabric)
    {
        m_gpaSessionSampleConfig.dfSpmPerfCounters.numCounters    = numTotalInstances;
        m_gpaSessionSampleConfig.dfSpmPerfCounters.sampleInterval = settings.gpuProfilerDfSpmConfig.dfSpmTraceInterval;
        m_gpaSessionSampleConfig.dfSpmPerfCounters.gpuMemoryLimit = ringSizeInBytes;
    }
    else
    {
        m_gpaSessionSampleConfig.perfCounters.numCounters            = numTotalInstances;
        m_gpaSessionSampleConfig.perfCounters.spmTraceSampleInterval =
            settings.gpuProfilerSpmConfig.spmTraceInterval;
        m_gpaSessionSampleConfig.perfCounters.gpuMemoryLimit         = ringSizeInBytes;
    }

    // Create pIds for the counters that were requested in the config file.
    GpuUtil::PerfCounterId* pIds =
        static_cast<GpuUtil::PerfCounterId*>(PAL_CALLOC(numTotalInstances * sizeof(GpuUtil::PerfCounterId),
                                             m_pDevice->GetPlatform(),
                                             AllocInternal));
    if (pIds != nullptr)
    {
        if (isDataFabric)
        {
            m_gpaSessionSampleConfig.dfSpmPerfCounters.pIds = pIds;
        }
        else
        {
            m_gpaSessionSampleConfig.perfCounters.pIds = pIds;
        }

        uint32 pIdIndex = 0;

        // Create PerfCounterIds with same eventId for all instances of the block.
        for (uint32 counter = 0; counter < numSpmCountersRequested; ++counter)
        {
            GpuUtil::PerfCounterId counterInfo = {};
            counterInfo.block   = pStreamingCounters[counter].block;
            counterInfo.eventId = pStreamingCounters[counter].eventId;
            if (counterInfo.block == GpuBlock::DfMall)
            {
                counterInfo.df.eventQualifier = pStreamingCounters[counter].optionalData;
            }
            const uint64 instanceMask = pStreamingCounters[counter].instanceMask;
            for (uint32 j = 0; j < pStreamingCounters[counter].instanceCount; j++)
            {
                if ((instanceMask == 0) || Util::BitfieldIsSet(instanceMask, j))
                {
                    counterInfo.instance = pStreamingCounters[counter].instanceId + j;
                    pIds[pIdIndex++]     = counterInfo;
                }
            }
        }
        PAL_ASSERT(pIdIndex == numTotalInstances);
    }
    else
    {
        result = Result::ErrorOutOfMemory;
    }
    return result;
}

// =====================================================================================================================
// Destruct sample config info
void Queue::DestroyGpaSessionSampleConfig()
{
    if (m_gpaSessionSampleConfig.perfCounters.pIds != nullptr)
    {
        PAL_SAFE_FREE(m_gpaSessionSampleConfig.perfCounters.pIds, m_pDevice->GetPlatform());
    }

    if (m_gpaSessionSampleConfig.dfSpmPerfCounters.pIds != nullptr)
    {
        PAL_SAFE_FREE(m_gpaSessionSampleConfig.dfSpmPerfCounters.pIds, m_pDevice->GetPlatform());
    }

    m_gpaSessionSampleConfig = {};
}

// =====================================================================================================================
// Check if the logItem contains a valid GPA sample.
bool Queue::HasValidGpaSample(
    const LogItem* pLogItem,
    GpuUtil::GpaSampleType type
) const
{
    uint32 sampleId = GpuUtil::InvalidSampleId;

    if (pLogItem->pGpaSession != nullptr)
    {
        switch (type)
        {
        case GpuUtil::GpaSampleType::Cumulative:
        case GpuUtil::GpaSampleType::Trace:
            sampleId = pLogItem->gpaSampleId;
            break;
        case GpuUtil::GpaSampleType::Timing:
            sampleId = pLogItem->gpaSampleIdTs;
            break;
        case GpuUtil::GpaSampleType::Query:
            sampleId = pLogItem->gpaSampleIdQuery;
            break;
        default:
            break;
        }
    }

    return (sampleId != GpuUtil::InvalidSampleId);
}

} // GpuProfiler
} // Pal
