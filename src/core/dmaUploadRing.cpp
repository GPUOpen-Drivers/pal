/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/cmdBuffer.h"
#include "core/device.h"
#include "core/dmaUploadRing.h"
#include "core/gpuMemory.h"
#include "core/fence.h"
#include "core/queue.h"
#include "core/platform.h"
#include "palDequeImpl.h"
#include "palSysUtil.h"
#include "palSysMemory.h"

namespace Pal
{

constexpr EngineType UploadEngine = EngineTypeDma;
constexpr QueueType  UploadQueue  = QueueTypeDma;

// =====================================================================================================================
DmaUploadRing::DmaUploadRing(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_pDmaQueue(nullptr),
    m_pRing(nullptr),
    m_ringCapacity(RingInitEntries),
    m_firstEntryInUse(0),
    m_firstEntryFree(0),
    m_numEntriesInUse(0)
{

}

// =====================================================================================================================
Result DmaUploadRing::InitRingItem(
    uint32 slotIdx)
{
    PAL_ASSERT((m_pRing[slotIdx].pCmdBuf == nullptr) && (m_pRing[slotIdx].pFence == nullptr));

    CmdBuffer* pCmdBuf = nullptr;
    IFence* pFence     = nullptr;
    Result result      = CreateInternalCopyCmdBuffer(&pCmdBuf);
    if (result == Result::Success)
    {
        m_pRing[slotIdx].pCmdBuf = pCmdBuf;
    }
    if (result == Result::Success)
    {
        result = CreateInternalFence(&pFence);
    }
    if (result == Result::Success)
    {
        result = m_pDevice->ResetFences(1, &pFence);
    }
    if (result == Result::Success)
    {
        m_pRing[slotIdx].pFence = pFence;
    }

    return result;
}

// =====================================================================================================================
Result DmaUploadRing::Init()
{
    Result result = Result::Success;
    Platform* pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());

    m_pRing = PAL_NEW_ARRAY(Entry, m_ringCapacity, m_pDevice->GetPlatform(), Util::SystemAllocType::AllocInternal);

    if(m_pRing == nullptr)
    {
        result = Result::ErrorOutOfMemory;
    }

    if (result == Result::Success)
    {
        memset(m_pRing, 0, sizeof(Entry)*m_ringCapacity);
        result = CreateInternalCopyQueue();
    }

    return result;
}

// =====================================================================================================================
Result DmaUploadRing::ResizeRing()
{
    Result result = Result::Success;
    Entry* pNewRing = PAL_NEW_ARRAY(
                            Entry,
                            m_ringCapacity * 2,
                            m_pDevice->GetPlatform(),
                            Util::SystemAllocType::AllocInternal);
    if (pNewRing == nullptr)
    {
        result = Result::ErrorOutOfMemory;
    }

    if (result == Result::Success)
    {
        // clear the second half of the new ring to 0.
        memset(Util::VoidPtrInc(pNewRing, sizeof(Entry)*m_ringCapacity), 0, sizeof(Entry)*m_ringCapacity);
        memcpy(pNewRing, m_pRing, sizeof(Entry)*m_ringCapacity);
        m_firstEntryInUse = 0;
        m_firstEntryFree  = m_ringCapacity;
        // m_numEntriesInUse does not change when resizing the ring.
        m_ringCapacity    = m_ringCapacity * 2;
        PAL_SAFE_DELETE_ARRAY(m_pRing, m_pDevice->GetPlatform());
        m_pRing           = pNewRing;
    }

    return result;
}

// =====================================================================================================================
Result DmaUploadRing::FreeFinishedSlots()
{
    Result result = Result::Success;

    while ((result == Result::Success) && (m_numEntriesInUse > 0))
    {
        PAL_ASSERT((m_pRing[m_firstEntryInUse].pCmdBuf != nullptr) && (m_pRing[m_firstEntryInUse].pFence != nullptr));
        Entry* pEntry = &m_pRing[m_firstEntryInUse];
        if (pEntry->pFence->GetStatus() == Result::Success)
        {
            result = m_pDevice->ResetFences(1, &pEntry->pFence);
        }
        else
        {
            break;
        }
        if (result == Result::Success)
        {
            m_numEntriesInUse--;
            m_firstEntryInUse = (m_firstEntryInUse + 1) % m_ringCapacity;
        }
    }

    return result;
}

// =====================================================================================================================
Result DmaUploadRing::AcquireRingSlot(
    UploadRingSlot* pSlotId)
{
    Result result = FreeFinishedSlots();
    PAL_ASSERT(result == Result::Success);

    if (result == Result::Success)
    {
        if (m_numEntriesInUse >= m_ringCapacity)
        {
            result = ResizeRing();
        }
    }

    // In case we fail to enlarge the ring, we wait from CPU until m_pDmaQueue finishes all pending works.
    if (result == Result::ErrorOutOfMemory)
    {
        result = m_pDmaQueue->WaitIdle();
        PAL_ASSERT(result == Result::Success);
        if (result == Result::Success)
        {
            result = FreeFinishedSlots();
        }
    }

    if ((result == Result::Success) &&
        ((m_pRing[m_firstEntryFree].pCmdBuf == nullptr) ||
        (m_pRing[m_firstEntryFree].pFence == nullptr)))
    {
        // We will make sure both pCmdBuf and pFence are nullptr in InitRingItem.
        result = InitRingItem(m_firstEntryFree);
    }

    if (result == Result::Success)
    {
        CmdBufferBuildFlags flags     = { };
        flags.optimizeExclusiveSubmit = true;
        flags.optimizeOneTimeSubmit   = true;

        CmdBufferBuildInfo buildInfo = { };
        buildInfo.flags              = flags;
        result = m_pRing[m_firstEntryFree].pCmdBuf->Begin(buildInfo);
        PAL_ASSERT(result == Result::Success);
    }

    if (result == Result::Success)
    {
        (*pSlotId) = m_firstEntryFree;
        m_firstEntryFree = (m_firstEntryFree + 1) % m_ringCapacity;
        m_numEntriesInUse++;
    }

    return result;
}

// =====================================================================================================================
size_t DmaUploadRing::UploadUsingEmbeddedData(
    UploadRingSlot  slotId,
    Pal::GpuMemory* pDst,
    gpusize         dstOffset,
    size_t          bytes,
    void**          ppEmbeddedData)
{
    size_t embeddedDataLimit = m_pRing[slotId].pCmdBuf->GetEmbeddedDataLimit() * sizeof(uint32);

    const size_t allocSize = (embeddedDataLimit >= bytes) ? bytes : embeddedDataLimit;

    GpuMemory* pGpuMem = nullptr;
    gpusize gpuMemOffset = 0;

    void*const pEmbeddedData = static_cast<CmdBuffer*>(m_pRing[slotId].pCmdBuf)->CmdAllocateEmbeddedData(
                                                       Util::NumBytesToNumDwords(static_cast<uint32>(allocSize)),
                                                       1,
                                                       &pGpuMem,
                                                       &gpuMemOffset);

    PAL_ASSERT(pEmbeddedData != nullptr);
    *ppEmbeddedData             = pEmbeddedData;
    MemoryCopyRegion copyRegion = { };
    copyRegion.copySize         = allocSize;
    copyRegion.dstOffset        = dstOffset;
    copyRegion.srcOffset        = gpuMemOffset;

    m_pRing[slotId].pCmdBuf->CmdCopyMemory(*pGpuMem, *pDst, 1, &copyRegion);

    return allocSize;
}

// =====================================================================================================================
Result DmaUploadRing::Submit(
    UploadRingSlot    slotId,
    UploadFenceToken* pCompletionFence,
    uint64            pagingFenceVal)
{
    Result result = m_pRing[slotId].pCmdBuf->End();
    if(result == Result::Success)
    {
        static_cast<CmdBuffer*>(m_pRing[slotId].pCmdBuf)->UpdateLastPagingFence(pagingFenceVal);

        PerSubQueueSubmitInfo perSubQueueInfo = {};
        perSubQueueInfo.cmdBufferCount        = 1;
        perSubQueueInfo.ppCmdBuffers          = &m_pRing[slotId].pCmdBuf;

        MultiSubmitInfo submitInfo      = {};
        submitInfo.perSubQueueInfoCount = 1;
        submitInfo.pPerSubQueueInfo     = &perSubQueueInfo;
        submitInfo.fenceCount           = 1;
        submitInfo.ppFences             = &m_pRing[slotId].pFence;

        result = m_pDmaQueue->SubmitInternal(submitInfo, false);
        *pCompletionFence = m_pDmaQueue->GetSubmissionContext()->LastTimestamp();
        PAL_ASSERT(*pCompletionFence > 0);
        PAL_ASSERT(result == Result::Success);
        static_cast<CmdBuffer*>(m_pRing[slotId].pCmdBuf)->Reset(m_pDevice->InternalCmdAllocator(UploadEngine), true);
    }

    return result;
}

// =====================================================================================================================
// Creates internal fence for tracking previous submission on the internal dma upload queue.
Result DmaUploadRing::CreateInternalFence(
    IFence** ppFence)
{
    Result result = Result::Success;
    const size_t fenceSize = m_pDevice->GetFenceSize(nullptr);

    void* pMemory = PAL_MALLOC(fenceSize, m_pDevice->GetPlatform(), Util::SystemAllocType::AllocInternal);

    if (pMemory != nullptr)
    {
        result = m_pDevice->CreateFence(FenceCreateInfo(), pMemory, ppFence);

        if (result != Result::Success)
        {
            PAL_SAFE_FREE(pMemory, m_pDevice->GetPlatform());
        }
    }
    else
    {
        result = Result::ErrorOutOfMemory;
    }
    return result;
}

// =====================================================================================================================
// Creates internal copy command buffer for serialized internal DMA operations.
Result DmaUploadRing::CreateInternalCopyCmdBuffer(
    CmdBuffer** ppCmdBuffer)
{
    Result result = Result::Success;

    CmdBufferCreateInfo cmdBufCreateInfo = { };
    cmdBufCreateInfo.engineType    = UploadEngine;
    cmdBufCreateInfo.queueType     = UploadQueue;
    cmdBufCreateInfo.pCmdAllocator = m_pDevice->InternalCmdAllocator(UploadEngine);

    CmdBufferInternalCreateInfo cmdBufInternalCreateInfo = { };
    cmdBufInternalCreateInfo.flags.isInternal = true;

    return m_pDevice->CreateInternalCmdBuffer(cmdBufCreateInfo, cmdBufInternalCreateInfo, ppCmdBuffer);
}

// =====================================================================================================================
// Creates a DMA queue which is meant for uploading pipeline binaries to local invisible heap.
Result DmaUploadRing::CreateInternalCopyQueue()
{
    const uint32 numEnginesAvailable = m_pDevice->EngineProperties().perEngine[UploadEngine].numAvailable;
    PAL_ASSERT(numEnginesAvailable > 0);

    Result result = Result::Success;
    QueueCreateInfo queueCreateInfo = { };
    queueCreateInfo.queueType       = UploadQueue;
    queueCreateInfo.engineType      = UploadEngine;
    queueCreateInfo.priority        = QueuePriority::Normal;
    queueCreateInfo.engineIndex     = (numEnginesAvailable - 1);

    size_t queueSize = m_pDevice->GetQueueSize(queueCreateInfo, &result);

    if (result == Result::Success)
    {
        void* pMemory = PAL_MALLOC(queueSize, m_pDevice->GetPlatform(), Util::SystemAllocType::AllocInternal);

        if (pMemory != nullptr)
        {
            IQueue* pQueue = nullptr;
            result = m_pDevice->CreateQueue(queueCreateInfo, pMemory, &pQueue);

            if (result != Result::Success)
            {
                PAL_SAFE_FREE(pMemory, m_pDevice->GetPlatform());
            }
            else
            {
                m_pDmaQueue = static_cast<Queue*>(pQueue);
            }
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    PAL_ASSERT(result == Result::Success);

    return result;
}

// =====================================================================================================================
DmaUploadRing::~DmaUploadRing()
{
    // Cleanup the internal device-owned queues.
    if (m_pDmaQueue != nullptr)
    {
        Result result = m_pDmaQueue->WaitIdle();
        PAL_ASSERT(result == Result::Success);
        m_pDmaQueue->Destroy();
        PAL_SAFE_FREE(m_pDmaQueue, m_pDevice->GetPlatform());
    }

    // Delete each cmdBuf and fence in entries
    if (m_pRing != nullptr)
    {
        for (uint32 i = 0; i < m_ringCapacity; i++)
        {
            if (m_pRing[i].pCmdBuf != nullptr)
            {
                static_cast<Pal::CmdBuffer*>(m_pRing[i].pCmdBuf)->DestroyInternal();
                m_pRing[i].pCmdBuf = nullptr;
            }

            if (m_pRing[i].pFence != nullptr)
            {
                static_cast<Pal::Fence*>(m_pRing[i].pFence)->DestroyInternal(m_pDevice->GetPlatform());
                m_pRing[i].pFence = nullptr;
            }
        }

        PAL_SAFE_DELETE_ARRAY(m_pRing, m_pDevice->GetPlatform());
    }
}

} // Pal
