/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_DEVELOPER_BUILD

#include "core/g_palPlatformSettings.h"
#include "core/layers/gpuDebug/gpuDebugCmdBuffer.h"
#include "core/layers/gpuDebug/gpuDebugDevice.h"
#include "core/layers/gpuDebug/gpuDebugPlatform.h"
#include "core/layers/gpuDebug/gpuDebugQueue.h"
#include "palAutoBuffer.h"
#include "palCmdAllocator.h"
#include "palDequeImpl.h"
#include "palSysUtil.h"

using namespace Util;

namespace Pal
{
namespace GpuDebug
{

// Wait for a maximum of 1000 seconds.
constexpr uint64 Timeout = 1000000000000ull;

// =====================================================================================================================
Queue::Queue(
    IQueue*    pNextQueue,
    Device*    pDevice,
    uint32     queueCount)
    :
    QueueDecorator(pNextQueue, pDevice),
    m_pDevice(pDevice),
    m_queueCount(queueCount),
    m_pQueueInfos(nullptr),
    m_timestampingActive(pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.singleStep != 0),
    m_pCmdAllocator(nullptr),
    m_pNestedCmdAllocator(nullptr),
    m_ppCmdBuffer(nullptr),
    m_ppTimestamp(nullptr),
    m_pendingSubmits(static_cast<Platform*>(pDevice->GetPlatform())),
    m_availableFences(static_cast<Platform*>(pDevice->GetPlatform())),
    m_replayAllocator(64 * 1024),
    m_submitOnActionCount(pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.submitOnActionCount),
    m_waitIdleSleepMs(pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.waitIdleSleepMs)
{
}

// =====================================================================================================================
Result Queue::Init(
    const QueueCreateInfo* pCreateInfo)
{
    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());

    m_pQueueInfos = PAL_NEW_ARRAY(SubQueueInfo, m_queueCount, pPlatform, AllocInternal);
    Result result = (m_pQueueInfos != nullptr) ? Result::Success : Result::ErrorOutOfMemory;

    for (uint32 i = 0; (result == Result::Success) && (i < m_queueCount); i++)
    {
        m_pQueueInfos[i].engineType               = pCreateInfo[i].engineType;
        m_pQueueInfos[i].engineIndex              = pCreateInfo[i].engineIndex;
        m_pQueueInfos[i].queueType                = pCreateInfo[i].queueType;
        m_pQueueInfos[i].commentsSupported        = Device::SupportsCommentString(pCreateInfo[i].queueType);
        m_pQueueInfos[i].pAvailableCmdBufs        = PAL_NEW(CmdBufDeque, pPlatform, AllocInternal)(pPlatform);
        m_pQueueInfos[i].pNextSubmitCmdBufs       = PAL_NEW(CmdBufDeque, pPlatform, AllocInternal)(pPlatform);
        m_pQueueInfos[i].pBusyCmdBufs             = PAL_NEW(CmdBufDeque, pPlatform, AllocInternal)(pPlatform);
        m_pQueueInfos[i].pAvailableNestedCmdBufs  = PAL_NEW(CmdBufDeque, pPlatform, AllocInternal)(pPlatform);
        m_pQueueInfos[i].pNextSubmitNestedCmdBufs = PAL_NEW(CmdBufDeque, pPlatform, AllocInternal)(pPlatform);
        m_pQueueInfos[i].pBusyNestedCmdBufs       = PAL_NEW(CmdBufDeque, pPlatform, AllocInternal)(pPlatform);

        if ((m_pQueueInfos[i].pAvailableCmdBufs        == nullptr) ||
            (m_pQueueInfos[i].pNextSubmitCmdBufs       == nullptr) ||
            (m_pQueueInfos[i].pBusyCmdBufs             == nullptr) ||
            (m_pQueueInfos[i].pAvailableNestedCmdBufs  == nullptr) ||
            (m_pQueueInfos[i].pNextSubmitNestedCmdBufs == nullptr) ||
            (m_pQueueInfos[i].pBusyNestedCmdBufs       == nullptr))
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    if (result == Result::Success)
    {
        result = m_replayAllocator.Init();
    }

    if (result == Result::Success)
    {
        result = InitCmdAllocator();
    }

    if (result == Result::Success)
    {
        result = InitNestedCmdAllocator();
    }

    if ((result == Result::Success) && m_timestampingActive)
    {
        DeviceProperties deviceProps = {};
        result = m_pDevice->GetProperties(&deviceProps);

        if (result == Result::Success)
        {
            m_ppTimestamp = static_cast<IGpuMemory**>(PAL_CALLOC(sizeof(IGpuMemory*) * m_queueCount,
                                                                 pPlatform,
                                                                 AllocInternal));

            result = (m_ppTimestamp != nullptr) ? Result::Success : Result::ErrorOutOfMemory;
        }

        if (result == Result::Success)
        {
            GpuMemoryCreateInfo createInfo = {};
            createInfo.size                = sizeof(CmdBufferTimestampData);
            createInfo.alignment           = sizeof(uint64);
            createInfo.vaRange             = VaRange::Default;
            createInfo.priority            = GpuMemPriority::VeryLow;
            createInfo.heapCount           = 1;
            createInfo.heaps[0]            = GpuHeap::GpuHeapInvisible;
            createInfo.flags.cpuInvisible  = 1;

            for (uint32 i = 0; ((result == Result::Success) && (i < m_queueCount)); i++)
            {
                m_ppTimestamp[i] = static_cast<IGpuMemory*>(PAL_MALLOC(m_pDevice->GetGpuMemorySize(createInfo, &result),
                                                                       pPlatform,
                                                                       AllocInternal));

                result = Result::ErrorOutOfMemory;
                if (m_ppTimestamp[i] != nullptr)
                {
                    result = m_pDevice->CreateGpuMemory(createInfo,
                                                        static_cast<void*>(m_ppTimestamp[i]),
                                                        &m_ppTimestamp[i]);

                    if (result != Result::Success)
                    {
                        uint32 j = i;

                        PAL_SAFE_FREE(m_ppTimestamp[j], pPlatform);

                        while (j > 0)
                        {
                            j--;
                            m_ppTimestamp[j]->Destroy();
                            PAL_SAFE_FREE(m_ppTimestamp[j], pPlatform);
                        }

                        PAL_SAFE_FREE(m_ppTimestamp, pPlatform);
                    }
                }

                if (result == Result::Success)
                {
                    GpuMemoryRef memRef = {};
                    memRef.pGpuMemory   = m_ppTimestamp[i];
                    result = m_pDevice->AddGpuMemoryReferences(1, &memRef, this, GpuMemoryRefCantTrim);
                }
            }
        }

        if (result == Result::Success)
        {
            result = InitCmdBuffers(pCreateInfo);
        }
    }

    return result;
}

// =====================================================================================================================
Result Queue::InitCmdAllocator()
{
    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());

    // We need a command allocator for the per-queue command buffer which contains information as to
    // where the timestamp data lives. This command buffer will be used for comments so we can get away
    // with small allocations and suballocations.
    constexpr uint32 AllocSize      = (2 * 1024 * 1024);
    constexpr uint32 SuballocSize   = (64 * 1024);

    CmdAllocatorCreateInfo createInfo = { };
    createInfo.flags.threadSafe               = 1;
    createInfo.flags.autoMemoryReuse          = 1;
    createInfo.flags.disableBusyChunkTracking = 1;
    createInfo.allocInfo[CommandDataAlloc].allocHeap      = GpuHeapGartCacheable;
    createInfo.allocInfo[CommandDataAlloc].suballocSize   = SuballocSize;
    createInfo.allocInfo[CommandDataAlloc].allocSize      = AllocSize;
    createInfo.allocInfo[EmbeddedDataAlloc].allocHeap     = GpuHeapGartCacheable;
    createInfo.allocInfo[EmbeddedDataAlloc].suballocSize  = SuballocSize;
    createInfo.allocInfo[EmbeddedDataAlloc].allocSize     = AllocSize;
    createInfo.allocInfo[GpuScratchMemAlloc].allocHeap    = GpuHeapInvisible;
    createInfo.allocInfo[GpuScratchMemAlloc].suballocSize = SuballocSize;
    createInfo.allocInfo[GpuScratchMemAlloc].allocSize    = AllocSize;

    Result result = Result::Success;

    m_pCmdAllocator =
        static_cast<ICmdAllocator*>(PAL_MALLOC(m_pDevice->GetCmdAllocatorSize(createInfo, &result),
                                               pPlatform,
                                               AllocInternal));

    if ((result == Result::Success) && (m_pCmdAllocator != nullptr))
    {
        result = m_pDevice->CreateCmdAllocator(createInfo, m_pCmdAllocator, &m_pCmdAllocator);

        if (result != Result::Success)
        {
            PAL_SAFE_FREE(m_pCmdAllocator, pPlatform);
        }
    }

    return result;
}

// =====================================================================================================================
Result Queue::InitNestedCmdAllocator()
{
    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());

    CmdAllocatorCreateInfo createInfo = { };
    createInfo.flags.threadSafe               = 1;
    createInfo.flags.autoMemoryReuse          = 1;
    createInfo.flags.disableBusyChunkTracking = 1;
    // All nested allocations are set the the minimum size (4KB) because applications that submit hundreds of nested
    // command buffers can potentially exhaust the GPU VA range by simply playing back too many nested command buffers.
    // This will have a small performance impact on large nested command buffers but we have little choice for now.
    createInfo.allocInfo[CommandDataAlloc].allocHeap      = GpuHeapGartUswc;
    createInfo.allocInfo[CommandDataAlloc].allocSize      = 4 * 1024;
    createInfo.allocInfo[CommandDataAlloc].suballocSize   = 4 * 1024;
    createInfo.allocInfo[EmbeddedDataAlloc].allocHeap     = GpuHeapGartUswc;
    createInfo.allocInfo[EmbeddedDataAlloc].allocSize     = 4 * 1024;
    createInfo.allocInfo[EmbeddedDataAlloc].suballocSize  = 4 * 1024;
    createInfo.allocInfo[GpuScratchMemAlloc].allocHeap    = GpuHeapGartUswc;
    createInfo.allocInfo[GpuScratchMemAlloc].allocSize    = 4 * 1024;
    createInfo.allocInfo[GpuScratchMemAlloc].suballocSize = 4 * 1024;

    Result result = Result::Success;

    m_pNestedCmdAllocator =
        static_cast<ICmdAllocator*>(PAL_MALLOC(m_pDevice->GetCmdAllocatorSize(createInfo, &result),
                                               pPlatform,
                                               AllocInternal));

    if ((result == Result::Success) && (m_pNestedCmdAllocator != nullptr))
    {
        result = m_pDevice->CreateCmdAllocator(createInfo, m_pNestedCmdAllocator, &m_pNestedCmdAllocator);

        if (result != Result::Success)
        {
            PAL_SAFE_FREE(m_pNestedCmdAllocator, pPlatform);
        }
    }

    return result;
}

// =====================================================================================================================
Result Queue::InitCmdBuffers(
    const QueueCreateInfo* pCreateInfo)
{
    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());
    m_ppCmdBuffer = static_cast<CmdBuffer**>(PAL_CALLOC(sizeof(CmdBuffer*) * m_queueCount,
                                                        pPlatform,
                                                        AllocInternal));

    Result result = (m_ppCmdBuffer != nullptr) ? Result::Success : Result::ErrorOutOfMemory;

    for (uint32 i = 0; (result == Result::Success) && (i < m_queueCount); ++i)
    {
        if (m_pQueueInfos[i].commentsSupported == false)
        {
            continue;
        }

        CmdBufferCreateInfo cmdBufferCreateInfo = {};
        cmdBufferCreateInfo.engineType          = pCreateInfo[i].engineType;
        cmdBufferCreateInfo.queueType           = pCreateInfo[i].queueType;
        cmdBufferCreateInfo.pCmdAllocator       = m_pCmdAllocator;

        m_ppCmdBuffer[i] =
            static_cast<CmdBuffer*>(PAL_MALLOC(m_pDevice->GetCmdBufferSize(cmdBufferCreateInfo, &result),
                                                pPlatform,
                                                AllocInternal));

        result = Result::ErrorOutOfMemory;
        if (m_ppCmdBuffer[i] != nullptr)
        {
            result = m_pDevice->CreateCmdBuffer(cmdBufferCreateInfo,
                                                m_ppCmdBuffer[i],
                                                reinterpret_cast<ICmdBuffer**>(&m_ppCmdBuffer[i]));

            if (result != Result::Success)
            {
                // Any other command buffers will be cleaned up in Destroy().
                PAL_SAFE_FREE(m_ppCmdBuffer[i], pPlatform);
            }
        }

        if (result == Result::Success)
        {
            CmdBufferBuildInfo buildInfo = {};
            buildInfo.flags.optimizeExclusiveSubmit = 1;
            result = m_ppCmdBuffer[i]->Begin(buildInfo);
        }

        if (result == Result::Success)
        {
            char buffer[256] = {};
            Snprintf(&buffer[0], sizeof(buffer),
                     "This submit contains timestamps which are written to the following GPU virtual address:");
            m_ppCmdBuffer[i]->CmdCommentString(&buffer[0]);
            Snprintf(&buffer[0], sizeof(buffer), "    0x%016llX", m_ppTimestamp[i]->Desc().gpuVirtAddr);
            m_ppCmdBuffer[i]->CmdCommentString(&buffer[0]);
            Snprintf(&buffer[0], sizeof(buffer), "The structure of the data at the above address is:");
            m_ppCmdBuffer[i]->CmdCommentString(&buffer[0]);
            Snprintf(&buffer[0], sizeof(buffer), "    uint64 cmdBufferHash; uint32 counter;");
            m_ppCmdBuffer[i]->CmdCommentString(&buffer[0]);

            result = m_ppCmdBuffer[i]->End();
        }
    }

    return result;
}

// =====================================================================================================================
// Acquire methods return corresponding objects for use by a command buffer being replayed from reusable pools
// managed by the Queue.
TargetCmdBuffer* Queue::AcquireCmdBuf(
    const CmdBufInfo* pCmdBufInfo,
    uint32            subQueueIdx,
    bool              nested)
{
    const SubQueueInfo& subQueueInfo = m_pQueueInfos[subQueueIdx];

    CmdBufDeque* pAvailable  = nested ? subQueueInfo.pAvailableNestedCmdBufs  : subQueueInfo.pAvailableCmdBufs;
    CmdBufDeque* pNextSubmit = nested ? subQueueInfo.pNextSubmitNestedCmdBufs : subQueueInfo.pNextSubmitCmdBufs;

    TargetCmdBuffer* pCmdBuffer = nullptr;

    if (pAvailable->NumElements() > 0)
    {
        // Use an idle command buffer from the pool if available.
        pAvailable->PopFront(&pCmdBuffer);

        // Check if the per-acquire state has been reset.
        PAL_ASSERT((pCmdBuffer->GetNestedCmdBufCount() == 0)        &&
                   (pCmdBuffer->GetSubQueueIdx() == BadSubQueueIdx) &&
                   (pCmdBuffer->GetCmdBufInfo() == nullptr));
    }
    else
    {
        // No command buffers are currently idle (or possibly none exist at all) - allocate a new command buffer.  Note
        // that we create a GpuDebug::TargetCmdBuffer here, not a GpuDebug::CmdBuffer which would just record our
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
            Result result = m_pDevice->CreateTargetCmdBuffer(createInfo, pMemory, &pCmdBuffer);

            if (result != Result::Success)
            {
                PAL_SAFE_FREE(pMemory, m_pDevice->GetPlatform());
            }
        }
    }

    PAL_ASSERT(pCmdBuffer != nullptr);

    // Set per-acquire state.
    pCmdBuffer->SetCmdBufInfo(pCmdBufInfo);
    pCmdBuffer->SetSubQueueIdx(subQueueIdx);

    // We always submit command buffers in the order they are acquired, so we can go ahead and add this to the next
    // submit queue immediately.
    pNextSubmit->PushBack(pCmdBuffer);

    return pCmdBuffer;
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

    if (pFence != nullptr)
    {
        m_pDevice->ResetFences(1, &pFence);
    }

    PAL_ASSERT(pFence != nullptr);
    return pFence;
}

// =====================================================================================================================
void Queue::Destroy()
{
    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());

    // Wait idle to make sure everything is done being used.
    m_pNextLayer->WaitIdle();
    ProcessIdleSubmits();

    for (uint32 qIdx = 0; qIdx < m_queueCount; qIdx++)
    {
        PAL_ASSERT(m_pQueueInfos[qIdx].pBusyCmdBufs->NumElements() == 0);
        PAL_ASSERT(m_pQueueInfos[qIdx].pNextSubmitCmdBufs->NumElements() == 0);
        PAL_ASSERT(m_pQueueInfos[qIdx].pBusyNestedCmdBufs->NumElements() == 0);
        PAL_ASSERT(m_pQueueInfos[qIdx].pNextSubmitNestedCmdBufs->NumElements() == 0);

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

        PAL_SAFE_DELETE(m_pQueueInfos[qIdx].pAvailableCmdBufs,        pPlatform);
        PAL_SAFE_DELETE(m_pQueueInfos[qIdx].pBusyCmdBufs,             pPlatform);
        PAL_SAFE_DELETE(m_pQueueInfos[qIdx].pNextSubmitCmdBufs,       pPlatform);
        PAL_SAFE_DELETE(m_pQueueInfos[qIdx].pAvailableNestedCmdBufs,  pPlatform);
        PAL_SAFE_DELETE(m_pQueueInfos[qIdx].pBusyNestedCmdBufs,       pPlatform);
        PAL_SAFE_DELETE(m_pQueueInfos[qIdx].pNextSubmitNestedCmdBufs, pPlatform);
    }
    PAL_SAFE_DELETE_ARRAY(m_pQueueInfos, pPlatform);

    if (m_ppTimestamp != nullptr)
    {
        m_pDevice->RemoveGpuMemoryReferences(m_queueCount, m_ppTimestamp, this);
        for (uint32 i = 0; i < m_queueCount; i++)
        {
            m_ppTimestamp[i]->Destroy();
            PAL_SAFE_FREE(m_ppTimestamp[i], pPlatform);
        }
        PAL_SAFE_FREE(m_ppTimestamp, pPlatform);
    }

    if (m_ppCmdBuffer != nullptr)
    {
        for (uint32 i = 0; i < m_queueCount; i++)
        {
            CmdBuffer* pCmdBuffer = m_ppCmdBuffer[i];
            if (pCmdBuffer != nullptr)
            {
                pCmdBuffer->Destroy();
                PAL_SAFE_FREE(pCmdBuffer, pPlatform);
            }
        }

        PAL_SAFE_FREE(m_ppCmdBuffer, pPlatform);
    }

    if (m_pCmdAllocator != nullptr)
    {
        m_pCmdAllocator->Destroy();
        PAL_SAFE_FREE(m_pCmdAllocator, pPlatform);
    }

    if (m_pNestedCmdAllocator != nullptr)
    {
        m_pNestedCmdAllocator->Destroy();
        PAL_SAFE_FREE(m_pNestedCmdAllocator, pPlatform);
    }

    while (m_availableFences.NumElements() > 0)
    {
        IFence* pFence = nullptr;
        m_availableFences.PopFront(&pFence);

        pFence->Destroy();
        PAL_SAFE_FREE(pFence, m_pDevice->GetPlatform());
    }

    IQueue* pNextLayer = m_pNextLayer;
    this->~Queue();
    pNextLayer->Destroy();
}

// =====================================================================================================================
void Queue::AddRemapRange(
    uint32                   queueId,
    VirtualMemoryRemapRange* pRange,
    CmdBuffer*               pCmdBuffer
    ) const
{
    pRange->pRealGpuMem        = m_ppTimestamp[queueId];
    pRange->realStartOffset    = 0;
    pRange->pVirtualGpuMem     = pCmdBuffer->TimestampMem();
    pRange->virtualStartOffset = 0;
    pRange->size               = m_ppTimestamp[queueId]->Desc().size;
    pRange->virtualAccessMode  = VirtualGpuMemAccessMode::NoAccess;
}

// =====================================================================================================================
Result Queue::Submit(
    const MultiSubmitInfo& submitInfo)
{
    // First start by processing any idle submits.
    ProcessIdleSubmits();

    // Wait for a maximum of 1000 seconds.
    constexpr uint64 Timeout = 1000000000000ull;

    Platform*       pPlatform       = static_cast<Platform*>(m_pDevice->GetPlatform());
    MultiSubmitInfo finalSubmitInfo = submitInfo;
    Result          result          = Result::Success;

    const bool dummySubmit =
        ((submitInfo.pPerSubQueueInfo == nullptr) || (submitInfo.pPerSubQueueInfo[0].cmdBufferCount == 0));

    // Start by assuming we'll need to add our header CmdBuffer per queue.
    size_t totalCmdBufferCount = m_queueCount;

    if (dummySubmit == false)
    {
        for (uint32 subQueueIdx = 0;
             (result == Result::Success) && (subQueueIdx < submitInfo.perSubQueueInfoCount);
             subQueueIdx++)
        {
            const auto& subQueueInfo = submitInfo.pPerSubQueueInfo[subQueueIdx];
            for (uint32 cmdBufIdx = 0;
                 (result == Result::Success) && (cmdBufIdx < subQueueInfo.cmdBufferCount);
                 cmdBufIdx++)
            {
                CmdBuffer*        pCmdBuffer  = static_cast<CmdBuffer*>(subQueueInfo.ppCmdBuffers[cmdBufIdx]);
                const CmdBufInfo* pCmdBufInfo =
                    (subQueueInfo.pCmdBufInfoList != nullptr) ? &subQueueInfo.pCmdBufInfoList[cmdBufIdx] : nullptr;
                result = pCmdBuffer->Replay(this, pCmdBufInfo, subQueueIdx, nullptr);

                const uint32 sufaceCaptureMemCount = pCmdBuffer->GetSurfaceCaptureGpuMemCount();
                if (sufaceCaptureMemCount > 0)
                {
                    AutoBuffer<GpuMemoryRef, 32, Platform> memRefs(sufaceCaptureMemCount, pPlatform);

                    for(uint32 i = 0; i < sufaceCaptureMemCount; i++)
                    {
                        memRefs[i].pGpuMemory   = pCmdBuffer->GetSurfaceCaptureGpuMems()[i];
                        memRefs[i].flags.u32All = 0;
                    }

                    m_pDevice->AddGpuMemoryReferences(
                        sufaceCaptureMemCount,
                        memRefs.Data(),
                        this,
                        0);
                }
            }

            totalCmdBufferCount += m_pQueueInfos[subQueueIdx].pNextSubmitCmdBufs->NumElements();
        }

        AutoBuffer<ICmdBuffer*, 32, Platform> cmdBuffers(totalCmdBufferCount,     pPlatform);
        AutoBuffer<CmdBufInfo,  32, Platform> cmdBufInfoList(totalCmdBufferCount, pPlatform);

        if ((result == Result::Success) &&
            ((cmdBuffers.Capacity() < totalCmdBufferCount) || (cmdBufInfoList.Capacity() < totalCmdBufferCount)))
        {
            result = Result::ErrorOutOfMemory;
        }

        if (result == Result::Success)
        {
            // Regardless of the submit model, we'll need to process the virtual memory remapping for the timestamp
            // memory, if it is active.
            result = ProcessRemaps(submitInfo, totalCmdBufferCount);
        }

        if (result == Result::Success)
        {
            // If we are using a MultiQueue, or we've disabled the 'submitOnActionCount' setting, we prepare
            // the submit like the client expected us to.
            if ((m_queueCount > 1) || (m_submitOnActionCount == 0))
            {
                AutoBuffer<PerSubQueueSubmitInfo, 32, Platform> perSubQueueInfoList(m_queueCount, pPlatform);

                if (perSubQueueInfoList.Capacity() >= m_queueCount)
                {
                    result = SubmitAll(submitInfo,
                                       perSubQueueInfoList.Data(),
                                       cmdBuffers.Data(),
                                       cmdBufInfoList.Data(),
                                       totalCmdBufferCount);
                }
                else
                {
                    result = Result::ErrorOutOfMemory;
                }
            }
            else
            {
                // If we're here, we're being asked to split the original client's submit into N number of submits,
                // where each submit contains X actions as specified by the 'submitOnActionCount'.
                result = SubmitSplit(submitInfo,
                                     cmdBuffers.Data(),
                                     cmdBufInfoList.Data(),
                                     totalCmdBufferCount);
            }
        }

        if (result == Result::Success)
        {
            bool idle = false;

            for (uint32 subQueueIdx = 0; subQueueIdx < submitInfo.perSubQueueInfoCount; subQueueIdx++)
            {
                const auto& subQueueInfo = submitInfo.pPerSubQueueInfo[subQueueIdx];
                for (uint32 cmdBufIdx = 0; cmdBufIdx < subQueueInfo.cmdBufferCount; cmdBufIdx++)
                {
                    CmdBuffer* pCmdBuffer = static_cast<CmdBuffer*>(subQueueInfo.ppCmdBuffers[cmdBufIdx]);

                    const uint32 sufaceCaptureMemCount = pCmdBuffer->GetSurfaceCaptureGpuMemCount();
                    if (sufaceCaptureMemCount > 0)
                    {
                        if (idle == false)
                        {
                            WaitIdle();
                            idle = true;
                        }
                        pCmdBuffer->OutputSurfaceCapture();

                        if (sufaceCaptureMemCount > 0)
                        {
                            m_pDevice->RemoveGpuMemoryReferences(
                                pCmdBuffer->GetSurfaceCaptureGpuMemCount(),
                                pCmdBuffer->GetSurfaceCaptureGpuMems(),
                                this);
                        }
                    }
                }
            }
        }
    }
    else
    {
        // This is a dummy submit, so we just forward it to the next layer.
        result = QueueDecorator::Submit(submitInfo);
    }

    return result;
}

// =====================================================================================================================
Result Queue::ProcessRemaps(
    const MultiSubmitInfo&   submitInfo,
    size_t                   totalCmdBufferCount)
{
    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());

    Result result = Result::Success;

    if (m_timestampingActive)
    {
        AutoBuffer<VirtualMemoryRemapRange, 32, Platform> ranges(totalCmdBufferCount, pPlatform);
        result = (ranges.Capacity() >= totalCmdBufferCount) ? Result::Success : Result::ErrorOutOfMemory;

        uint32 newRangeIdx = 0;

        if (result == Result::Success)
        {
            for (uint32 queueIdx = 0; queueIdx < submitInfo.perSubQueueInfoCount; queueIdx++)
            {
                const auto& perSubQueueInfo = submitInfo.pPerSubQueueInfo[queueIdx];
                if (m_pQueueInfos[queueIdx].commentsSupported)
                {
                    AddRemapRange(queueIdx, &ranges[newRangeIdx++], m_ppCmdBuffer[queueIdx]);
                }

                for (uint32 cmdBufIdx = 0; cmdBufIdx < perSubQueueInfo.cmdBufferCount; cmdBufIdx++)
                {
                    CmdBuffer* pCmdBuffer = static_cast<CmdBuffer*>(perSubQueueInfo.ppCmdBuffers[cmdBufIdx]);
                    AddRemapRange(queueIdx, &ranges[newRangeIdx++], pCmdBuffer);
                }
            }

            result = RemapVirtualMemoryPages(newRangeIdx, ranges.Data(), true, nullptr);
        }
    }

    return result;
}

// =====================================================================================================================
Result Queue::WaitForFence(
    const IFence* pFence
    ) const
{
    uint32 counter = 0;
    while ((pFence->GetStatus() != Result::Success) && (counter < m_waitIdleSleepMs))
    {
        SleepMs(1u);
        counter += 1;
    }

    return pFence->GetStatus();
}

// =====================================================================================================================
Result Queue::SubmitAll(
    const MultiSubmitInfo& submitInfo,
    PerSubQueueSubmitInfo* pPerSubQueueInfos,
    ICmdBuffer**           ppCmdBuffers,
    CmdBufInfo*            pCmdBufInfos,
    size_t                 totalCmdBufferCount)
{
    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());

    PendingSubmitInfo pendingInfo = {};
    pendingInfo.pFence = AcquireFence();

    Result result  = (pendingInfo.pFence != nullptr) ? Result::Success : Result::ErrorOutOfMemory;

    if (result == Result::Success)
    {
        pendingInfo.pCmdBufCount = PAL_NEW_ARRAY(uint32, m_queueCount, pPlatform, AllocInternal);
        result = (pendingInfo.pCmdBufCount != nullptr) ? Result::Success : Result::ErrorOutOfMemory;
    }

    if (result == Result::Success)
    {
        memset(pendingInfo.pCmdBufCount, 0, sizeof(uint32) * m_queueCount);
    }

    if (result == Result::Success)
    {
        pendingInfo.pNestedCmdBufCount = PAL_NEW_ARRAY(uint32, m_queueCount, pPlatform, AllocInternal);
        result = (pendingInfo.pNestedCmdBufCount != nullptr) ? Result::Success : Result::ErrorOutOfMemory;
    }

    if (result == Result::Success)
    {
        memset(pendingInfo.pNestedCmdBufCount, 0, sizeof(uint32) * m_queueCount);
    }

    MultiSubmitInfo newSubmitInfo      = submitInfo;
    newSubmitInfo.perSubQueueInfoCount = 0;
    newSubmitInfo.pPerSubQueueInfo     = pPerSubQueueInfos;

    uint32 newCmdBufIdx     = 0;
    uint32 newCmdBufInfoIdx = 0;

    for (uint32 subQueueIdx = 0;
         (result == Result::Success) && (subQueueIdx < submitInfo.perSubQueueInfoCount);
         subQueueIdx++)
    {
        const auto& subQueueInfo       = m_pQueueInfos[subQueueIdx];
        auto*       pNewPerSubmitInfo  = &pPerSubQueueInfos[subQueueIdx];
        const auto& oldPerSubmitInfo   = submitInfo.pPerSubQueueInfo[subQueueIdx];
        const bool  containsCmdBufInfo = (oldPerSubmitInfo.pCmdBufInfoList != nullptr);

        memset(pNewPerSubmitInfo, 0, sizeof(PerSubQueueSubmitInfo));
        pNewPerSubmitInfo->cmdBufferCount = static_cast<uint32>(subQueueInfo.pNextSubmitCmdBufs->NumElements());

        if (pNewPerSubmitInfo->cmdBufferCount > 0)
        {
            pNewPerSubmitInfo->ppCmdBuffers = &ppCmdBuffers[newCmdBufIdx];

            if (containsCmdBufInfo)
            {
                pNewPerSubmitInfo->pCmdBufInfoList = &pCmdBufInfos[newCmdBufInfoIdx];
            }

            if (m_timestampingActive && subQueueInfo.commentsSupported)
            {
                ppCmdBuffers[newCmdBufIdx++]       = m_ppCmdBuffer[subQueueIdx];
                pCmdBufInfos[newCmdBufInfoIdx++]   = CmdBufInfo{};
                pNewPerSubmitInfo->cmdBufferCount += 1;
            }
        }

        while ((result == Result::Success) && (subQueueInfo.pNextSubmitCmdBufs->NumElements() > 0))
        {
            TargetCmdBuffer* pCmdBuffer = nullptr;
            result = subQueueInfo.pNextSubmitCmdBufs->PopFront(&pCmdBuffer);

            if (result == Result::Success)
            {
                ppCmdBuffers[newCmdBufIdx++] = pCmdBuffer;
                if (containsCmdBufInfo)
                {
                    pCmdBufInfos[newCmdBufInfoIdx++] = (pCmdBuffer->GetCmdBufInfo() != nullptr) ?
                                                        (*pCmdBuffer->GetCmdBufInfo()) : CmdBufInfo{};
                }

                // Add it to the list of busy command buffers for tracking.
                subQueueInfo.pBusyCmdBufs->PushBack(pCmdBuffer);

                pendingInfo.pCmdBufCount[subQueueIdx]++;
            }

            // Add the current CmdBuffer's tracked nested CmdBuffers to the nested CmdBuffer busy list.
            const uint32 currentCmdBufferNestedCount =
                (pCmdBuffer != nullptr) ? pCmdBuffer->GetNestedCmdBufCount() : 0;

            // All of the CmdBuffers' nested CmdBuffers are tracked in pNextSubmitNestedCmdBufs - we're only
            // interested in those tracked by the current command buffer.

            // We're using a for loop here because we need to only pull as many items off of the
            // pNextSubmitNestedCmdBufs list as there are currently tracked in the current CmdBuffer.
            for (uint32 nestedCount = 0; (nestedCount < currentCmdBufferNestedCount); ++nestedCount)
            {
                TargetCmdBuffer* pNextNestedCmdBuffer = nullptr;
                result = subQueueInfo.pNextSubmitNestedCmdBufs->PopFront(&pNextNestedCmdBuffer);
                if (result == Result::Success)
                {
                    // Add it to the list of busy nested command buffers for tracking.
                    subQueueInfo.pBusyNestedCmdBufs->PushBack(pNextNestedCmdBuffer);

                    pendingInfo.pNestedCmdBufCount[subQueueIdx]++;
                }
                else
                {
                    // If popping a command buffer off of the "next submit" deque fails then there is probably
                    // an issue with how primary command buffers track nested executes.
                    PAL_ASSERT_ALWAYS();
                    break;
                }
            }
        }

        // All nested command buffers should have been tracked as busy with their parents.
        PAL_ASSERT(subQueueInfo.pNextSubmitNestedCmdBufs->NumElements() == 0);

        newSubmitInfo.perSubQueueInfoCount++;
    }

    PAL_ASSERT(newCmdBufIdx     <= totalCmdBufferCount);
    PAL_ASSERT(newCmdBufInfoIdx <= totalCmdBufferCount);
    PAL_ASSERT((newSubmitInfo.perSubQueueInfoCount == submitInfo.perSubQueueInfoCount) ||
               (result != Result::Success));

    if (result == Result::Success)
    {
        result = QueueDecorator::Submit(newSubmitInfo);
    }

    if (result == Result::Success)
    {
        result = AssociateFenceWithLastSubmit(pendingInfo.pFence);
    }

    // When submitting all of the command buffers together as one submit, we only need to wait for idle on the submit
    // if timestamping is active globally.
    if ((result == Result::Success) && m_timestampingActive)
    {
        Result fenceResult = WaitForFence(pendingInfo.pFence);
        PAL_ASSERT(fenceResult == Result::Success);
        ProcessIdleSubmits();
    }

    if (result == Result::Success)
    {
        m_pendingSubmits.PushBack(pendingInfo);
    }

    return result;
}

// =====================================================================================================================
Result Queue::SubmitSplit(
    const MultiSubmitInfo& submitInfo,
    ICmdBuffer**           ppCmdBuffers,
    CmdBufInfo*            pCmdBufInfos,
    size_t                 totalCmdBufferCount)
{
    PAL_ASSERT(m_queueCount == 1);
    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());

    Result          result          = Result::Success;
    MultiSubmitInfo splitSubmitInfo = submitInfo;

    const uint32 fenceCount = submitInfo.fenceCount;
    IFence**     ppFences   = submitInfo.ppFences;

    splitSubmitInfo.fenceCount = 0;
    splitSubmitInfo.ppFences   = nullptr;

    const auto& subQueueInfo       = m_pQueueInfos[0];
    const auto& oldPerSubmitInfo   = submitInfo.pPerSubQueueInfo[0];
    const bool  containsCmdBufInfo = (oldPerSubmitInfo.pCmdBufInfoList != nullptr);

    while ((result == Result::Success) && (subQueueInfo.pNextSubmitCmdBufs->NumElements() > 0))
    {
        uint32 newCmdBufIdx     = 0;
        uint32 newCmdBufInfoIdx = 0;

        PerSubQueueSubmitInfo perSubQueueSubmitInfo = {};
        splitSubmitInfo.pPerSubQueueInfo = &perSubQueueSubmitInfo;
        PAL_ASSERT(splitSubmitInfo.perSubQueueInfoCount == 1);

        if (containsCmdBufInfo)
        {
            perSubQueueSubmitInfo.pCmdBufInfoList = &pCmdBufInfos[newCmdBufInfoIdx];
        }
        perSubQueueSubmitInfo.ppCmdBuffers   = &ppCmdBuffers[newCmdBufIdx];
        perSubQueueSubmitInfo.cmdBufferCount = 0; // Reset to 0 each submit iteration.

        if (m_timestampingActive && subQueueInfo.commentsSupported)
        {
            ppCmdBuffers[newCmdBufIdx++]     = m_ppCmdBuffer[0];
            pCmdBufInfos[newCmdBufInfoIdx++] = CmdBufInfo{};
            perSubQueueSubmitInfo.cmdBufferCount++;
        }

        PendingSubmitInfo pendingInfo = {};
        pendingInfo.pFence = AcquireFence();

        result = (pendingInfo.pFence != nullptr) ? Result::Success : Result::ErrorOutOfMemory;

        if (result == Result::Success)
        {
            pendingInfo.pCmdBufCount = PAL_NEW_ARRAY(uint32, m_queueCount, pPlatform, AllocInternal);
            result = (pendingInfo.pCmdBufCount != nullptr) ? Result::Success : Result::ErrorOutOfMemory;
        }

        if (result == Result::Success)
        {
            memset(pendingInfo.pCmdBufCount, 0, sizeof(uint32) * m_queueCount);
        }

        if (result == Result::Success)
        {
            pendingInfo.pNestedCmdBufCount = PAL_NEW_ARRAY(uint32, m_queueCount, pPlatform, AllocInternal);
            result = (pendingInfo.pNestedCmdBufCount != nullptr) ? Result::Success : Result::ErrorOutOfMemory;
        }

        if (result == Result::Success)
        {
            memset(pendingInfo.pNestedCmdBufCount, 0, sizeof(uint32) * m_queueCount);
        }

        for (uint32 cmdBufIdx = 0;
            ((result == Result::Success) &&
                (cmdBufIdx < m_submitOnActionCount) &&
                (subQueueInfo.pNextSubmitCmdBufs->NumElements() > 0));
            cmdBufIdx++)
        {
            TargetCmdBuffer* pCmdBuffer = nullptr;
            result = subQueueInfo.pNextSubmitCmdBufs->PopFront(&pCmdBuffer);

            if (result == Result::Success)
            {
                ppCmdBuffers[newCmdBufIdx++] = pCmdBuffer;
                if (containsCmdBufInfo)
                {
                    pCmdBufInfos[newCmdBufInfoIdx++] = (pCmdBuffer->GetCmdBufInfo() != nullptr) ?
                                                        (*pCmdBuffer->GetCmdBufInfo()) : CmdBufInfo{};
                }

                // Add it to the list of busy command buffers for tracking.
                subQueueInfo.pBusyCmdBufs->PushBack(pCmdBuffer);

                // Increment the number of command buffers for this submit.
                perSubQueueSubmitInfo.cmdBufferCount++;

                pendingInfo.pCmdBufCount[0]++;
            }

            // Add the current CmdBuffer's tracked nested CmdBuffers to the nested CmdBuffer busy list.
            const uint32 currentCmdBufferNestedCount =
                (pCmdBuffer != nullptr) ? pCmdBuffer->GetNestedCmdBufCount() : 0;

            // All of the CmdBuffers' nested CmdBuffers are tracked in pNextSubmitNestedCmdBufs - we're only
            // interested in those tracked by the current command buffer.

            // We're using a for loop here because we need to only pull as many items off of the
            // pNextSubmitNestedCmdBufs list as there are currently tracked in the current CmdBuffer.
            for (uint32 nestedCount = 0; (nestedCount < currentCmdBufferNestedCount); ++nestedCount)
            {
                TargetCmdBuffer* pNextNestedCmdBuffer = nullptr;
                result = subQueueInfo.pNextSubmitNestedCmdBufs->PopFront(&pNextNestedCmdBuffer);
                if (result == Result::Success)
                {
                    // Add it to the list of busy nested command buffers for tracking.
                    subQueueInfo.pBusyNestedCmdBufs->PushBack(pNextNestedCmdBuffer);

                    pendingInfo.pNestedCmdBufCount[0]++;
                }
                else
                {
                    // If popping a command buffer off of the "next submit" deque fails then there is probably
                    // an issue with how primary command buffers track nested executes.
                    PAL_ASSERT_ALWAYS();
                    break;
                }
            }
        }

        // Only use the client's fences for the last submit we issue.
        if ((result == Result::Success) && (subQueueInfo.pNextSubmitCmdBufs->NumElements() == 0))
        {
            splitSubmitInfo.fenceCount = fenceCount;
            splitSubmitInfo.ppFences   = ppFences;
        }

        if (result == Result::Success)
        {
            result = QueueDecorator::Submit(splitSubmitInfo);
        }

        if (result == Result::Success)
        {
            result = AssociateFenceWithLastSubmit(pendingInfo.pFence);
        }

        if (result == Result::Success)
        {
            result = m_pendingSubmits.PushBack(pendingInfo);
        }

        // Since we're splitting the submit, we need to wait for each submit because we're possibly reusing the
        // first CmdBuffer in the list.
        if ((result == Result::Success) && m_timestampingActive && subQueueInfo.commentsSupported)
        {
            Result fenceResult = WaitForFence(pendingInfo.pFence);
            PAL_ASSERT(fenceResult == Result::Success);
            ProcessIdleSubmits();
        }
    }

    // All nested command buffers should have been tracked as busy with their parents.
    PAL_ASSERT(subQueueInfo.pNextSubmitNestedCmdBufs->NumElements() == 0);

    return result;
}

// =====================================================================================================================
void Queue::ProcessIdleSubmits()
{
    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());
    while ((m_pendingSubmits.NumElements() > 0) && (m_pendingSubmits.Front().pFence->GetStatus() == Result::Success))
    {
        PendingSubmitInfo submitInfo = { };
        m_pendingSubmits.PopFront(&submitInfo);
        PAL_ASSERT(submitInfo.pFence             != nullptr);
        PAL_ASSERT(submitInfo.pCmdBufCount       != nullptr);
        PAL_ASSERT(submitInfo.pNestedCmdBufCount != nullptr);

        for (uint32 subQueueIdx = 0; subQueueIdx < m_queueCount; subQueueIdx++)
        {
            const SubQueueInfo& queueInfo       = m_pQueueInfos[subQueueIdx];
            const uint32        cmdBufCnt       = submitInfo.pCmdBufCount[subQueueIdx];
            const uint32        nestedCmdBufCnt = submitInfo.pNestedCmdBufCount[subQueueIdx];

            for (uint32 i = 0; i < cmdBufCnt; i++)
            {
                TargetCmdBuffer* pCmdBuffer = nullptr;
                queueInfo.pBusyCmdBufs->PopFront(&pCmdBuffer);

                pCmdBuffer->SetClientData(nullptr);
                Result result = pCmdBuffer->Reset(nullptr, true);
                PAL_ASSERT(result == Result::Success);

                queueInfo.pAvailableCmdBufs->PushBack(pCmdBuffer);
            }

            for (uint32 i = 0; i < nestedCmdBufCnt; i++)
            {
                TargetCmdBuffer* pCmdBuffer = nullptr;
                queueInfo.pBusyNestedCmdBufs->PopFront(&pCmdBuffer);

                pCmdBuffer->SetClientData(nullptr);
                Result result = pCmdBuffer->Reset(nullptr, true);
                PAL_ASSERT(result == Result::Success);

                queueInfo.pAvailableNestedCmdBufs->PushBack(pCmdBuffer);
            }
        }

        m_availableFences.PushBack(submitInfo.pFence);

        PAL_SAFE_DELETE_ARRAY(submitInfo.pCmdBufCount, pPlatform);
        PAL_SAFE_DELETE_ARRAY(submitInfo.pNestedCmdBufCount, pPlatform);
    }
}

} // GpuDebug
} // Pal

#endif
