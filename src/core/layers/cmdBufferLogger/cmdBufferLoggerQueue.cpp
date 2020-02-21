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
    Device*    pDevice)
    :
    QueueDecorator(pNextQueue, pDevice),
    m_pDevice(pDevice),
    m_timestampingActive(static_cast<Platform*>(pDevice->GetPlatform())->IsTimestampingEnabled()),
    m_pCmdAllocator(nullptr),
    m_pCmdBuffer(nullptr),
    m_pTimestamp(nullptr),
    m_pFence(nullptr)
{
}

// =====================================================================================================================
Result Queue::Init(
    EngineType engineType,
    QueueType  queueType)
{
    Result result = Result::Success;

    if (m_timestampingActive)
    {
        Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());

        DeviceProperties deviceProps = {};
        result = m_pDevice->GetProperties(&deviceProps);

        GpuMemoryCreateInfo createInfo = {};
        if (result == Result::Success)
        {
            result = Result::ErrorOutOfMemory;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 516
            createInfo.size               = sizeof(CmdBufferTimestampData);
            createInfo.alignment          = sizeof(uint64);
#else
            const Pal::gpusize allocGranularity = deviceProps.gpuMemoryProperties.realMemAllocGranularity;
            createInfo.size               = Util::Pow2Align(sizeof(CmdBufferTimestampData), allocGranularity);
            createInfo.alignment          = Util::Pow2Align(sizeof(uint64), allocGranularity);
#endif
            createInfo.vaRange            = VaRange::Default;
            createInfo.priority           = GpuMemPriority::VeryLow;
            createInfo.heapCount          = 1;
            createInfo.heaps[0]           = GpuHeap::GpuHeapInvisible;
            createInfo.flags.cpuInvisible = 1;

            m_pTimestamp = static_cast<IGpuMemory*>(PAL_MALLOC(m_pDevice->GetGpuMemorySize(createInfo, &result),
                                                    pPlatform,
                                                    AllocInternal));

            if (m_pTimestamp != nullptr)
            {
                result = m_pDevice->CreateGpuMemory(createInfo, static_cast<void*>(m_pTimestamp), &m_pTimestamp);
            }
        }

        if (result == Result::Success)
        {
            GpuMemoryRef memRef = {};
            memRef.pGpuMemory   = m_pTimestamp;
            result = m_pDevice->AddGpuMemoryReferences(1, &memRef, this, GpuMemoryRefCantTrim);
        }

        if (result == Result::Success)
        {
            result = InitCmdBuffer(engineType, queueType);
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
Result Queue::InitCmdBuffer(
    EngineType engineType,
    QueueType  queueType)
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
        CmdBufferCreateInfo cmdBufferCreateInfo = {};
        cmdBufferCreateInfo.engineType          = engineType;
        cmdBufferCreateInfo.queueType           = queueType;
        cmdBufferCreateInfo.pCmdAllocator       = m_pCmdAllocator;

        m_pCmdBuffer =
            static_cast<CmdBuffer*>(PAL_MALLOC(m_pDevice->GetCmdBufferSize(cmdBufferCreateInfo, &result),
                                                pPlatform,
                                                AllocInternal));

        result = Result::ErrorOutOfMemory;
        if (m_pCmdBuffer != nullptr)
        {
            result = m_pDevice->CreateCmdBuffer(cmdBufferCreateInfo,
                                                m_pCmdBuffer,
                                                reinterpret_cast<ICmdBuffer**>(&m_pCmdBuffer));
        }
    }

    if (result == Result::Success)
    {
        CmdBufferBuildInfo buildInfo = {};
        buildInfo.flags.optimizeExclusiveSubmit = 1;
        result = m_pCmdBuffer->Begin(buildInfo);
    }

    if (result == Result::Success)
    {
        char buffer[256] = {};
        Snprintf(&buffer[0], sizeof(buffer),
                 "This submit contains timestamps which are written to the following GPU virtual address:");
        m_pCmdBuffer->CmdCommentString(&buffer[0]);
        Snprintf(&buffer[0], sizeof(buffer), "    0x%016llX", m_pTimestamp->Desc().gpuVirtAddr);
        m_pCmdBuffer->CmdCommentString(&buffer[0]);
        Snprintf(&buffer[0], sizeof(buffer), "The structure of the data at the above address is:");
        m_pCmdBuffer->CmdCommentString(&buffer[0]);
        Snprintf(&buffer[0], sizeof(buffer), "    uint64 cmdBufferHash; uint32 counter;");
        m_pCmdBuffer->CmdCommentString(&buffer[0]);

        result = m_pCmdBuffer->End();
    }

    return result;
}

// =====================================================================================================================
void Queue::Destroy()
{
    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());
    if (m_timestampingActive)
    {
        if (m_pTimestamp != nullptr)
        {
            m_pDevice->RemoveGpuMemoryReferences(1, &m_pTimestamp, this);
            m_pTimestamp->Destroy();
            PAL_SAFE_FREE(m_pTimestamp, pPlatform);
        }

        if (m_pCmdBuffer != nullptr)
        {
            m_pCmdBuffer->Destroy();
            PAL_SAFE_FREE(m_pCmdBuffer, pPlatform);
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
    VirtualMemoryRemapRange* pRange,
    CmdBuffer*               pCmdBuffer
    ) const
{
    pRange->pRealGpuMem        = m_pTimestamp;
    pRange->realStartOffset    = 0;
    pRange->pVirtualGpuMem     = pCmdBuffer->TimestampMem();
    pRange->virtualStartOffset = 0;
    pRange->size               = m_pTimestamp->Desc().size;
    pRange->virtualAccessMode  = VirtualGpuMemAccessMode::NoAccess;
}

// =====================================================================================================================
Result Queue::Submit(
    const MultiSubmitInfo& submitInfo)
{
    // Wait for a maximum of 1000 seconds.
    constexpr uint64 Timeout = 1000000000000ull;
    PAL_ASSERT(submitInfo.perSubQueueInfoCount == 1);

    Platform*       pPlatform       = static_cast<Platform*>(m_pDevice->GetPlatform());
    MultiSubmitInfo finalSubmitInfo = submitInfo;
    Result          result          = Result::Success;

    PerSubQueueSubmitInfo perSubQueueInfo = {};

    if (m_timestampingActive)
    {
        const uint32 maxCmdBufferCount = submitInfo.pPerSubQueueInfo[0].cmdBufferCount + 1;
        AutoBuffer<ICmdBuffer*, 32, Platform> cmdBuffers(maxCmdBufferCount, pPlatform);
        AutoBuffer<CmdBufInfo, 32, Platform> cmdBufInfoList(maxCmdBufferCount, pPlatform);
        AutoBuffer<VirtualMemoryRemapRange, 16, Platform> ranges(maxCmdBufferCount, pPlatform);

        if ((cmdBuffers.Capacity() < submitInfo.pPerSubQueueInfo[0].cmdBufferCount)     ||
            (cmdBufInfoList.Capacity() < submitInfo.pPerSubQueueInfo[0].cmdBufferCount) ||
            (ranges.Capacity() < submitInfo.pPerSubQueueInfo[0].cmdBufferCount))
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            memset(cmdBufInfoList.Data(), 0, sizeof(CmdBufInfo) * maxCmdBufferCount);

            const bool  hasCmdBufInfo   = (submitInfo.pPerSubQueueInfo[0].pCmdBufInfoList != nullptr);

            // Our informative command buffer goes first.
            cmdBuffers[0] = m_pCmdBuffer;
            AddRemapRange(&ranges[0], m_pCmdBuffer);

            if (hasCmdBufInfo)
            {
                // Only the first one of these has any real data in it.
                cmdBufInfoList[0] = submitInfo.pPerSubQueueInfo[0].pCmdBufInfoList[0];
            }

            for (uint32 i = 0; i < submitInfo.pPerSubQueueInfo[0].cmdBufferCount; i++)
            {
                CmdBuffer* pCmdBuffer = static_cast<CmdBuffer*>(submitInfo.pPerSubQueueInfo[0].ppCmdBuffers[i]);
                AddRemapRange(&ranges[i + 1], pCmdBuffer);
                cmdBuffers[i + 1] = pCmdBuffer;
            }

            if (result == Result::Success)
            {
                result = RemapVirtualMemoryPages(maxCmdBufferCount, ranges.Data(), true, nullptr);
            }

            if ((result == Result::Success) && (m_pFence != nullptr))
            {
                result = m_pDevice->ResetFences(1, &m_pFence);
            }

            if (result == Result::Success)
            {
                perSubQueueInfo.cmdBufferCount = maxCmdBufferCount;
                perSubQueueInfo.ppCmdBuffers = &cmdBuffers[0];
                perSubQueueInfo.pCmdBufInfoList = (hasCmdBufInfo) ? &cmdBufInfoList[0] : nullptr;
                finalSubmitInfo.pPerSubQueueInfo = &perSubQueueInfo;
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

    if(result == Result::Success)
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
