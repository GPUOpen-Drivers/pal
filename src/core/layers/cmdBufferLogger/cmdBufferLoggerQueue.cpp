/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_BUILD_CMD_BUFFER_LOGGER

#include "core/layers/cmdBufferLogger/cmdBufferLoggerCmdBuffer.h"
#include "core/layers/cmdBufferLogger/cmdBufferLoggerDevice.h"
#include "core/layers/cmdBufferLogger/cmdBufferLoggerPlatform.h"
#include "core/layers/cmdBufferLogger/cmdBufferLoggerQueue.h"
#include "palAutoBuffer.h"
#include "palCmdAllocator.h"

using namespace Util;

namespace Pal
{
namespace CmdBufferLogger
{

// =====================================================================================================================
Queue::Queue(
    IQueue*    pNextQueue,
    Device*    pDevice,
    uint32     queueCount)
    :
    QueueDecorator(pNextQueue, pDevice),
    m_pDevice(pDevice),
    m_queueCount(queueCount),
    m_timestampingActive(static_cast<Platform*>(pDevice->GetPlatform())->IsTimestampingEnabled()),
    m_pCmdAllocator(nullptr),
    m_ppCmdBuffer(nullptr),
    m_ppTimestamp(nullptr),
    m_pFence(nullptr)
{
}

// =====================================================================================================================
Result Queue::Init(
    const QueueCreateInfo* pCreateInfo)
{
    Result result = Result::Success;

    if (m_timestampingActive)
    {
        Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());

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
                                                        reinterpret_cast<IGpuMemory**>(m_ppTimestamp[i]));
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

        if (result == Result::Success)
        {
            m_pFence = static_cast<IFence*>(PAL_MALLOC(m_pDevice->GetFenceSize(&result),
                                                       pPlatform,
                                                       AllocInternal));

            if (m_pFence != nullptr)
            {
                FenceCreateInfo fenceCreateInfo = {};
                result = m_pDevice->CreateFence(fenceCreateInfo, m_pFence, &m_pFence);
            }
        }
    }

    return result;
}

// =====================================================================================================================
Result Queue::InitCmdBuffers(
    const QueueCreateInfo* pCreateInfo)
{
    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());

    // We need a command allocator for the per-queue command buffer which contains information as to
    // where the timestamp data lives. This command buffer will be used for comments so we can get away
    // with small allocations and suballocations.
    constexpr uint32 CommandDataSuballocSize = (1024 * 4);
    constexpr uint32 EmbeddedDataSuballocSize = (1024 * 4);
    constexpr uint32 GpuScratchMemSuballocSize = (1024 * 4);

    CmdAllocatorCreateInfo cmdAllocCreateInfo = { };
    cmdAllocCreateInfo.flags.threadSafe      = 1;
    cmdAllocCreateInfo.flags.autoMemoryReuse = 1;
    cmdAllocCreateInfo.allocInfo[CommandDataAlloc].allocHeap      = GpuHeapGartCacheable;
    cmdAllocCreateInfo.allocInfo[CommandDataAlloc].suballocSize   = CommandDataSuballocSize;
    cmdAllocCreateInfo.allocInfo[CommandDataAlloc].allocSize      = CommandDataSuballocSize;
    cmdAllocCreateInfo.allocInfo[EmbeddedDataAlloc].allocHeap     = GpuHeapGartCacheable;
    cmdAllocCreateInfo.allocInfo[EmbeddedDataAlloc].suballocSize  = EmbeddedDataSuballocSize;
    cmdAllocCreateInfo.allocInfo[EmbeddedDataAlloc].allocSize     = EmbeddedDataSuballocSize;
    cmdAllocCreateInfo.allocInfo[GpuScratchMemAlloc].allocHeap    = GpuHeapInvisible;
    cmdAllocCreateInfo.allocInfo[GpuScratchMemAlloc].suballocSize = GpuScratchMemSuballocSize;
    cmdAllocCreateInfo.allocInfo[GpuScratchMemAlloc].allocSize    = GpuScratchMemSuballocSize;

    m_pCmdAllocator =
        static_cast<ICmdAllocator*>(PAL_MALLOC(m_pDevice->GetCmdAllocatorSize(cmdAllocCreateInfo, nullptr),
                                               pPlatform,
                                               AllocInternal));

    Result result = Result::ErrorOutOfMemory;
    if (m_pCmdAllocator != nullptr)
    {
        result = m_pDevice->CreateCmdAllocator(cmdAllocCreateInfo, m_pCmdAllocator, &m_pCmdAllocator);
    }

    if (result == Result::Success)
    {
        m_ppCmdBuffer = static_cast<CmdBuffer**>(PAL_CALLOC(sizeof(CmdBuffer*) * m_queueCount,
                                                            pPlatform,
                                                            AllocInternal));

        result = (m_ppCmdBuffer != nullptr) ? Result::Success : Result::ErrorOutOfMemory;
    }

    for (uint32 i = 0; (result == Result::Success) && (i < m_queueCount); ++i)
    {
        if (m_pDevice->SupportsCommentString(pCreateInfo[i].queueType) == false)
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
void Queue::Destroy()
{
    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());
    if (m_timestampingActive)
    {
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

        if (m_pFence != nullptr)
        {
            m_pFence->Destroy();
            PAL_SAFE_FREE(m_pFence, pPlatform);
        }
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
    PAL_ASSERT_MSG((submitInfo.perSubQueueInfoCount <= 1),
                   "Multi-Queue support has not yet been tested in CmdBufferLogger!");

    // Wait for a maximum of 1000 seconds.
    constexpr uint64 Timeout = 1000000000000ull;

    Platform*       pPlatform       = static_cast<Platform*>(m_pDevice->GetPlatform());
    MultiSubmitInfo finalSubmitInfo = submitInfo;
    Result          result          = Result::Success;

    const bool dummySubmit =
        ((submitInfo.pPerSubQueueInfo == nullptr) || (submitInfo.pPerSubQueueInfo[0].cmdBufferCount == 0));

    if ((m_timestampingActive) && (dummySubmit == false))
    {
        // Start by assuming we'll need to add our header CmdBuffer per queue.
        uint32 maxCmdBufferCount = m_queueCount;

        for (uint32 i = 0; i < submitInfo.perSubQueueInfoCount; i++)
        {
            maxCmdBufferCount += submitInfo.pPerSubQueueInfo[i].cmdBufferCount;
        }

        AutoBuffer<PerSubQueueSubmitInfo,   32, Platform> perSubQueueInfoList(m_queueCount, pPlatform);
        AutoBuffer<ICmdBuffer*,             32, Platform> cmdBuffers(maxCmdBufferCount,     pPlatform);
        AutoBuffer<CmdBufInfo,              32, Platform> cmdBufInfoList(maxCmdBufferCount, pPlatform);
        AutoBuffer<VirtualMemoryRemapRange, 32, Platform> ranges(maxCmdBufferCount,         pPlatform);

        if ((perSubQueueInfoList.Capacity() < m_queueCount)      ||
            (cmdBuffers.Capacity()          < maxCmdBufferCount) ||
            (cmdBufInfoList.Capacity()      < maxCmdBufferCount) ||
            (ranges.Capacity()              < maxCmdBufferCount))
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            memset(perSubQueueInfoList.Data(), 0, sizeof(PerSubQueueSubmitInfo) * m_queueCount);
            memset(cmdBufInfoList.Data(),      0, sizeof(CmdBufInfo) * maxCmdBufferCount);

            finalSubmitInfo.pPerSubQueueInfo = perSubQueueInfoList.Data();

            uint32 newCmdBufferIdx  = 0;
            uint32 newCmdBufInfoIdx = 0;
            uint32 newRangesIdx     = 0;

            for (uint32 i = 0; i < submitInfo.perSubQueueInfoCount; ++i)
            {
                const auto& perSubQueueInfo      = submitInfo.pPerSubQueueInfo[i];
                const bool  supportsTimestamping = (m_ppCmdBuffer[i] != nullptr);

                const uint32 startCmdBufferIdx  = newCmdBufferIdx;
                const uint32 startCmdBufInfoIdx = newCmdBufInfoIdx;

                if (supportsTimestamping)
                {
                    cmdBuffers[newCmdBufferIdx++] = m_ppCmdBuffer[i];
                    newCmdBufInfoIdx++;
                    AddRemapRange(i, &ranges[newRangesIdx++], m_ppCmdBuffer[i]);
                }

                const bool hasCmdBufInfo = (perSubQueueInfo.pCmdBufInfoList != nullptr);
                for (uint32 cmdBufIdx = 0; cmdBufIdx < perSubQueueInfo.cmdBufferCount; cmdBufIdx++)
                {
                    CmdBuffer* pCmdBuffer = static_cast<CmdBuffer*>(perSubQueueInfo.ppCmdBuffers[cmdBufIdx]);
                    cmdBuffers[newCmdBufferIdx++] = pCmdBuffer;

                    if (hasCmdBufInfo)
                    {
                        cmdBufInfoList[newCmdBufInfoIdx++] = perSubQueueInfo.pCmdBufInfoList[cmdBufIdx];
                    }
                    AddRemapRange(i, &ranges[newRangesIdx++], pCmdBuffer);
                }

                PAL_ASSERT((hasCmdBufInfo == false) ||
                           (newCmdBufferIdx - startCmdBufferIdx) == (newCmdBufInfoIdx - startCmdBufInfoIdx));

                PerSubQueueSubmitInfo* pNewPerSubQueueInfo = &perSubQueueInfoList[i];
                pNewPerSubQueueInfo->cmdBufferCount  = (newCmdBufferIdx - startCmdBufferIdx);
                pNewPerSubQueueInfo->ppCmdBuffers    = &cmdBuffers[startCmdBufferIdx];
                pNewPerSubQueueInfo->pCmdBufInfoList = &cmdBufInfoList[startCmdBufInfoIdx];
            }

            if (result == Result::Success)
            {
                result = RemapVirtualMemoryPages(newRangesIdx, ranges.Data(), true, nullptr);
            }

            if ((result == Result::Success) && (m_pFence != nullptr))
            {
                result = m_pDevice->ResetFences(1, &m_pFence);
            }

            if (result == Result::Success)
            {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 568
                finalSubmitInfo.ppFences   = (submitInfo.fenceCount > 0) ? submitInfo.ppFences : &m_pFence;
                finalSubmitInfo.fenceCount = (submitInfo.fenceCount > 0) ? submitInfo.fenceCount :
                                             (m_pFence != nullptr);
#else
                finalSubmitInfo.pFence = (submitInfo.pFence != nullptr) ? submitInfo.pFence : m_pFence;
#endif
            }
        }
    }

    if (result == Result::Success)
    {
        result = QueueDecorator::Submit(finalSubmitInfo);
    }

    if ((result == Result::Success) && m_timestampingActive &&
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 568
        (finalSubmitInfo.fenceCount > 0))
    {
        result = m_pDevice->WaitForFences(finalSubmitInfo.fenceCount, finalSubmitInfo.ppFences, true, Timeout);
    }
#else
        (finalSubmitInfo.pFence != nullptr))
    {
        result = m_pDevice->WaitForFences(1, &finalSubmitInfo.pFence, true, Timeout);
    }
#endif

    return result;
}

} // CmdBufferLogger
} // Pal

#endif
