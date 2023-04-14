/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/cmdAllocator.h"
#include "core/cmdBuffer.h"
#include "core/device.h"
#include "core/gpuMemory.h"
#include "core/hw/gfxip/cmdUploadRing.h"
#include "palCmdBuffer.h"
#include "palFence.h"
#include "palInlineFuncs.h"
#include "palQueueSemaphore.h"
#include "palQueue.h"
#include "palVectorImpl.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
size_t CmdUploadRing::GetPlacementSize(
    const Device& device)
{
    QueueSemaphoreCreateInfo signaledSemaphoreCreateInfo = {};
    signaledSemaphoreCreateInfo.maxCount     = device.MaxQueueSemaphoreCount();
    signaledSemaphoreCreateInfo.initialCount = 1;

    QueueSemaphoreCreateInfo unsignaledSemaphoreCreateInfo = {};
    unsignaledSemaphoreCreateInfo.maxCount = device.MaxQueueSemaphoreCount();

    // Note that each raft's GpuMemory is created by the MemMgr so we don't have to allocate space for it.
    const size_t perRaftSize = device.GetQueueSemaphoreSize(signaledSemaphoreCreateInfo,   nullptr) +
                               device.GetQueueSemaphoreSize(unsignaledSemaphoreCreateInfo, nullptr);

    CmdBufferCreateInfo cmdBufferCreateInfo = {};
    cmdBufferCreateInfo.queueType     = QueueTypeDma;
    cmdBufferCreateInfo.engineType    = EngineTypeDma;
    cmdBufferCreateInfo.pCmdAllocator = device.InternalCmdAllocator(EngineTypeDma);

    const size_t perCopySize = device.GetCmdBufferSize(cmdBufferCreateInfo, nullptr) +
                               device.GetFenceSize(nullptr);

    QueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.queueType  = QueueTypeDma;
    queueCreateInfo.engineType = EngineTypeDma;

    return (device.GetQueueSize(queueCreateInfo, nullptr) +
            (RaftRingSize * perRaftSize)                  +
            (CopyRingSize * perCopySize));
}

// =====================================================================================================================
void CmdUploadRing::DestroyInternal()
{
    Platform*const pPlatform = m_pDevice->GetPlatform();
    this->~CmdUploadRing();
    PAL_FREE(this, pPlatform);
}

// =====================================================================================================================
CmdUploadRing::CmdUploadRing(
    const CmdUploadRingCreateInfo& createInfo,
    Device*                        pDevice,
    uint32                         minPostambleBytes,
    gpusize                        maxStreamBytes)
    :
    m_createInfo(createInfo),
    m_trackMemoryRefs(pDevice->MemoryProperties().flags.supportPerSubmitMemRefs),
    m_addrAlignBytes(pDevice->EngineProperties().perEngine[createInfo.engineType].startAlign),
    m_sizeAlignBytes(pDevice->EngineProperties().perEngine[createInfo.engineType].sizeAlignInDwords * sizeof(uint32)),
    m_minPostambleBytes(minPostambleBytes),
    m_maxStreamBytes(maxStreamBytes),
    m_pDevice(pDevice),
    m_pQueue(nullptr),
    m_prevRaft(0),
    m_prevCopy(0),
    m_chunkMemoryRefs(pDevice->GetPlatform())
{
    // If this trips we added a new stream to a command buffer type and MaxUploadedCmdStreams needs to be increased.
    PAL_ASSERT(m_createInfo.numCmdStreams <= MaxUploadedCmdStreams);

    // The alignments are assumed to be powers of two.
    PAL_ASSERT(IsPowerOfTwo(m_addrAlignBytes));
    PAL_ASSERT(IsPowerOfTwo(m_sizeAlignBytes));

    memset(m_raft, 0, sizeof(m_raft));
    memset(m_copy, 0, sizeof(m_copy));
}

// =====================================================================================================================
CmdUploadRing::~CmdUploadRing()
{
    if (m_pQueue != nullptr)
    {
        // We must wait for our queue to be idle before destroying it or any other objects.
        const Result result = m_pQueue->WaitIdle();
        PAL_ASSERT(result == Result::Success);

        m_pQueue->Destroy();
    }

    for (uint32 idx = 0; idx < RaftRingSize; ++idx)
    {
        for (uint32 memIdx = 0; memIdx < MaxUploadedCmdStreams; ++memIdx)
        {
            if (m_raft[idx].pGpuMemory[memIdx] != nullptr)
            {
                IGpuMemory*const pGpuMemory = m_raft[idx].pGpuMemory[memIdx];
                const Result     result     = m_pDevice->RemoveGpuMemoryReferences(1, &pGpuMemory, nullptr);
                PAL_ASSERT(result == Result::Success);

                m_raft[idx].pGpuMemory[memIdx]->DestroyInternal();
            }
        }

        if (m_raft[idx].pStartCopy != nullptr)
        {
            m_raft[idx].pStartCopy->Destroy();
        }

        if (m_raft[idx].pEndCopy != nullptr)
        {
            m_raft[idx].pEndCopy->Destroy();
        }
    }

    for (uint32 idx = 0; idx < CopyRingSize; ++idx)
    {
        if (m_copy[idx].pCmdBuffer != nullptr)
        {
            m_copy[idx].pCmdBuffer->Destroy();
        }

        if (m_copy[idx].pFence != nullptr)
        {
            m_copy[idx].pFence->Destroy();
        }
    }
}

// =====================================================================================================================
Result CmdUploadRing::Init(
    void* pPlacementAddr)
{
    // We must fail immediately if we can't create enough GPU memory objects for our command streams.
    Result result = (m_createInfo.numCmdStreams > MaxUploadedCmdStreams) ? Result::ErrorInitializationFailed
                                                                         : Result::Success;

    if (result == Result::Success)
    {
        QueueCreateInfo createInfo = {};
        createInfo.queueType  = QueueTypeDma;
        createInfo.engineType = EngineTypeDma;

        result         = m_pDevice->CreateQueue(createInfo, pPlacementAddr, &m_pQueue);
        pPlacementAddr = VoidPtrInc(pPlacementAddr, m_pDevice->GetQueueSize(createInfo, nullptr));
    }

    GpuMemoryRef memRefs[RaftRingSize * MaxUploadedCmdStreams] = {};
    uint32       numMemRefs = 0;

    for (uint32 idx = 0; idx < RaftRingSize; ++idx)
    {
        if (result == Result::Success)
        {
            GpuMemoryCreateInfo createInfo = {};
            createInfo.size      = RaftMemBytes;
            createInfo.alignment = m_addrAlignBytes;
            createInfo.vaRange   = VaRange::Default;
            createInfo.priority  = GpuMemPriority::High;
            createInfo.heapCount = 2;
            createInfo.heaps[0]  = (m_pDevice->HeapLogicalSize(GpuHeapInvisible) != 0)
                                    ? GpuHeapInvisible : GpuHeapLocal;
            createInfo.heaps[1]  = GpuHeapGartUswc;

            GpuMemoryInternalCreateInfo internalInfo = {};
            internalInfo.flags.udmaBuffer = 1;

            for (uint32 memIdx = 0; memIdx < m_createInfo.numCmdStreams; ++memIdx)
            {
                result = m_pDevice->CreateInternalGpuMemory(createInfo, internalInfo, &m_raft[idx].pGpuMemory[memIdx]);
                memRefs[numMemRefs++].pGpuMemory = m_raft[idx].pGpuMemory[memIdx];
            }
        }

        if (result == Result::Success)
        {
            QueueSemaphoreCreateInfo createInfo = {};
            createInfo.maxCount     = m_pDevice->MaxQueueSemaphoreCount();
            createInfo.initialCount = 1;

            result         = m_pDevice->CreateQueueSemaphore(createInfo, pPlacementAddr, &m_raft[idx].pStartCopy);
            pPlacementAddr = VoidPtrInc(pPlacementAddr, m_pDevice->GetQueueSemaphoreSize(createInfo, nullptr));
        }

        if (result == Result::Success)
        {
            QueueSemaphoreCreateInfo createInfo = {};
            createInfo.maxCount = m_pDevice->MaxQueueSemaphoreCount();

            result         = m_pDevice->CreateQueueSemaphore(createInfo, pPlacementAddr, &m_raft[idx].pEndCopy);
            pPlacementAddr = VoidPtrInc(pPlacementAddr, m_pDevice->GetQueueSemaphoreSize(createInfo, nullptr));
        }
    }

    if (result == Result::Success)
    {
        PAL_ASSERT(numMemRefs == RaftRingSize * m_createInfo.numCmdStreams);
        result = m_pDevice->AddGpuMemoryReferences(numMemRefs, memRefs, nullptr, GpuMemoryRefCantTrim);
    }

    for (uint32 idx = 0; idx < CopyRingSize; ++idx)
    {
        if (result == Result::Success)
        {
            CmdBufferCreateInfo createInfo = {};
            createInfo.queueType     = QueueTypeDma;
            createInfo.engineType    = EngineTypeDma;
            createInfo.pCmdAllocator = m_pDevice->InternalCmdAllocator(EngineTypeDma);

            result         = m_pDevice->CreateCmdBuffer(createInfo, pPlacementAddr, &m_copy[idx].pCmdBuffer);
            pPlacementAddr = VoidPtrInc(pPlacementAddr, m_pDevice->GetCmdBufferSize(createInfo, nullptr));
        }

        if (result == Result::Success)
        {
            Pal::FenceCreateInfo createInfo = {};
            createInfo.flags.signaled       = 1;
            result = m_pDevice->CreateFence(createInfo, pPlacementAddr, &m_copy[idx].pFence);
            pPlacementAddr = VoidPtrInc(pPlacementAddr, m_pDevice->GetFenceSize(nullptr));
        }
    }

    return result;
}

// =====================================================================================================================
// Conservatively estimates how many command buffers can be uploaded when calling UploadCmdBuffers.
uint32 CmdUploadRing::PredictBatchSize(
    uint32                  cmdBufferCount,
    const ICmdBuffer*const* ppCmdBuffers
    ) const
{
    PAL_ASSERT(ppCmdBuffers != nullptr);

    const uint32 maxBatchSize = Min(cmdBufferCount, m_pDevice->GetPublicSettings()->cmdBufBatchedSubmitChainLimit);
    gpusize      totalSize[MaxUploadedCmdStreams] = {};
    bool         uploadMoreCmdBuffers = true;
    uint32       batchSize = 0;

    for (uint32 cmdBufIdx = 0; (cmdBufIdx < maxBatchSize) && uploadMoreCmdBuffers; ++cmdBufIdx)
    {
        const CmdBuffer*const pCmdBuffer = static_cast<const CmdBuffer*>(ppCmdBuffers[cmdBufIdx]);
        PAL_ASSERT(pCmdBuffer != nullptr);

        if ((pCmdBuffer->NumCmdStreams() != m_createInfo.numCmdStreams) || pCmdBuffer->HasAddressDependentCmdStream())
        {
            // UploadCommandStreams requires num streams to match and that the streams be address independent.
            uploadMoreCmdBuffers = false;
        }
        else
        {
            // Our upload code guarantees that we can include this command buffer in our batch (chaining if necessary).
            batchSize++;

            for (uint32 streamIdx = 0; (streamIdx < m_createInfo.numCmdStreams); ++streamIdx)
            {
                const CmdStream*const pCmdStream = pCmdBuffer->GetCmdStream(streamIdx);
                if (pCmdStream != nullptr)
                {
                    totalSize[streamIdx] += pCmdStream->TotalChunkDwords() * sizeof(uint32);

                    // Check if we have any space left for the next command buffer's stream. We don't need to track
                    // where the postambles will go because TotalChunkDwords includes all command stream postambles
                    // which in the worst case will be just as large as what we will upload.
                    if (totalSize[streamIdx] >= RaftMemBytes)
                    {
                        uploadMoreCmdBuffers = false;
                    }
                }
            }
        }
    }

    return batchSize;
}

// =====================================================================================================================
// Uploads a batch of commands buffers to a large GPU memory raft. If no error occurs pUploadInfo is populated with
// enough information to launch the uploaded command streams and contains semaphores the caller must wait on and signal.
Result CmdUploadRing::UploadCmdBuffers(
    uint32                  cmdBufferCount,
    const ICmdBuffer*const* ppCmdBuffers,
    UploadedCmdBufferInfo*  pUploadInfo)
{
    PAL_ASSERT(ppCmdBuffers != nullptr);

    // Uploading nothing doesn't make sense, we assume we always have at least one command buffer.
    PAL_ASSERT(cmdBufferCount > 0);

    // Get the next set of state from our two rings.
    Raft*const pRaft = NextRaft();
    Copy*const pCopy = NextCopy();

    // Wait for the prior use of this command copy command buffer to be idle.
    // If the alert trips we should increase the size of the copy ring.
    PAL_ALERT(pCopy->pFence->GetStatus() == Result::NotReady);

    constexpr uint64 TwoSeconds = 2000000000ull;
    Result           result     = m_pDevice->WaitForFences(1, &pCopy->pFence, true, TwoSeconds);

    if (result == Result::Success)
    {
        result = m_pDevice->ResetFences(1, &pCopy->pFence);
    }

    if (result == Result::Success)
    {
        CmdBufferBuildInfo buildInfo = {};
        buildInfo.flags.optimizeOneTimeSubmit = 1;

        result = pCopy->pCmdBuffer->Begin(buildInfo);
    }

    // Initialize all uploaded streams. Zero is a natural default for most values, but we must:
    // - explicitly reserve enough space for a final chain postamble
    // - set up per-stream flags like whether or not we can be preempted.
    UploadState streamState[MaxUploadedCmdStreams] = {};

    for (uint32 idx = 0; idx < m_createInfo.numCmdStreams; ++idx)
    {
        const CmdStream* pFirstStream = nullptr;
        for (uint32 cmdBufIdx = 0; (cmdBufIdx < cmdBufferCount) && (pFirstStream == nullptr); cmdBufIdx++)
        {
            pFirstStream = static_cast<const CmdBuffer*>(ppCmdBuffers[cmdBufIdx])->GetCmdStream(idx);
        }

        if (pFirstStream != nullptr)
        {
            streamState[idx].curIbFreeBytes                 = Min(m_maxStreamBytes, RaftMemBytes) - m_minPostambleBytes;
            streamState[idx].engineType                     = pFirstStream->GetEngineType();
            streamState[idx].subEngineType                  = pFirstStream->GetSubEngineType();
            streamState[idx].flags.isPreemptionEnabled      = pFirstStream->IsPreemptionEnabled();
            streamState[idx].flags.dropIfSameContext        = pFirstStream->DropIfSameContext();
        }
    }

    const PalSettings& settings = m_pDevice->Settings();
    const uint32 maxBatchSize = Min(cmdBufferCount, m_pDevice->GetPublicSettings()->cmdBufBatchedSubmitChainLimit);

    uint32 uploadedCmdBuffers   = 0;
    bool   uploadMoreCmdBuffers = true;

    for (uint32 cmdBufIdx = 0;
         (cmdBufIdx < maxBatchSize) && (result == Result::Success) && uploadMoreCmdBuffers;
         ++cmdBufIdx)
    {
        const CmdBuffer*const pCmdBuffer = static_cast<const CmdBuffer*>(ppCmdBuffers[cmdBufIdx]);
        PAL_ASSERT(pCmdBuffer != nullptr);

        if ((pCmdBuffer->GetEngineType() != m_createInfo.engineType) ||
            (pCmdBuffer->NumCmdStreams() != m_createInfo.numCmdStreams))
        {
            // This probably means we did something illegal like launch a graphics command buffer on a compute queue.
            result = Result::ErrorInvalidValue;
        }
        else if (pCmdBuffer->HasAddressDependentCmdStream())
        {
            // We can't upload this command buffer and must exit.
            uploadMoreCmdBuffers = false;

            // The caller is required to only call this function if at least one command buffer can be uploaded. If
            // this triggers we shouldn't hang or crash but will waste CPU/GPU time and might deadlock in the caller.
            PAL_ASSERT(uploadedCmdBuffers > 0);
        }
        else
        {
            // The following loop is written so that we will always be able to upload the current command buffer.
            uploadedCmdBuffers++;

            for (uint32 streamIdx = 0;
                 (streamIdx < m_createInfo.numCmdStreams) && (result == Result::Success);
                 ++streamIdx)
            {
                UploadState*const     pState     = &streamState[streamIdx];
                const CmdStream*const pCmdStream = pCmdBuffer->GetCmdStream(streamIdx);

                if ((pCmdStream != nullptr) && (pCmdStream->IsEmpty() == false))
                {
                    for (auto chunkIter = pCmdStream->GetFwdIterator();
                         chunkIter.IsValid() && (result == Result::Success);
                         chunkIter.Next())
                    {
                        const CmdStreamChunk*const pChunk = chunkIter.Get();
                        const gpusize chunkBytes = pChunk->CmdDwordsToExecuteNoPostamble() * sizeof(uint32);

                        if (chunkBytes > pState->curIbFreeBytes)
                        {
                            // If this triggers we are uploading a chunk bigger than the whole raft.
                            // We should tune the driver to avoid this.
                            PAL_ALERT(pState->curIbSizeBytes == 0);

                            // If the current IB can't fit the next chunk we must end the IB.
                            EndCurrentIb(*pRaft->pGpuMemory[streamIdx], pCopy->pCmdBuffer, pState);

                            // Set up a new current IB if we have space for it. If not, curIbFreeBytes == 0 will signal
                            // that we can't fit anymore data in the raft.
                            const gpusize remainingBytes =
                                (RaftMemBytes > pState->raftFreeOffset) ? (RaftMemBytes - pState->raftFreeOffset) : 0;

                            if (remainingBytes > Pow2Align(m_minPostambleBytes, m_sizeAlignBytes))
                            {
                                pState->curIbOffset    = pState->raftFreeOffset;
                                pState->curIbFreeBytes = Min(m_maxStreamBytes, remainingBytes) - m_minPostambleBytes;
                            }
                        }

                        if (chunkBytes > pState->curIbFreeBytes)
                        {
                            // If we still don't have enough space we can't upload anything else into this stream. We
                            // must chain to the remaining chunks in the source command buffer and tell the command
                            // buffer loop that no other streams should consider further command buffer chunks.

                            // This must be true because we just called EndCurrentIb.
                            PAL_ASSERT(pState->prevIbPostambleSize > 0);

                            UploadChainPostamble(*pRaft->pGpuMemory[streamIdx],
                                                 pCopy->pCmdBuffer,
                                                 pState->prevIbPostambleOffset,
                                                 pState->prevIbPostambleSize,
                                                 pChunk->GpuVirtAddr(),
                                                 pChunk->CmdDwordsToExecute() * sizeof(uint32),
                                                 (pState->subEngineType == SubEngineType::ConstantEngine),
                                                 pState->flags.isPreemptionEnabled);

                            uploadMoreCmdBuffers = false;
                            break;
                        }
                        else
                        {
                            // Append the chunk to the end of the raft.
                            MemoryCopyRegion region = {};
                            region.srcOffset = pChunk->GpuMemoryOffset();
                            region.dstOffset = pState->raftFreeOffset;
                            region.copySize  = chunkBytes;

                            pCopy->pCmdBuffer->CmdCopyMemory(*pChunk->GpuMemory(),
                                                             *pRaft->pGpuMemory[streamIdx],
                                                             1,
                                                             &region);

                            pState->raftFreeOffset += chunkBytes;
                            pState->curIbSizeBytes += chunkBytes;
                            pState->curIbFreeBytes -= chunkBytes;

                            if (m_trackMemoryRefs)
                            {
                                // Remember this chunk's command allocation for later.
                                GpuMemoryRef memRef = {};
                                memRef.flags.readOnly = 1;
                                memRef.pGpuMemory     = pChunk->GpuMemory();

                                result = m_chunkMemoryRefs.PushBack(memRef);
                            }
                        }
                    }
                }
            }
        }
    }

    if (result == Result::Success)
    {
        for (uint32 idx = 0; idx < m_createInfo.numCmdStreams; ++idx)
        {
            UploadState*const pState = &streamState[idx];

            // We've uploaded as much as we can, we need to end the current IB if it's not empty.
            if (pState->curIbSizeBytes > 0)
            {
                EndCurrentIb(*pRaft->pGpuMemory[idx], pCopy->pCmdBuffer, pState);

                // Write a NOP-filled postamble to prevent the CP from hanging.
                UploadChainPostamble(*pRaft->pGpuMemory[idx],
                                     pCopy->pCmdBuffer,
                                     pState->prevIbPostambleOffset,
                                     pState->prevIbPostambleSize,
                                     0,
                                     0,
                                     (pState->subEngineType == SubEngineType::ConstantEngine),
                                     pState->flags.isPreemptionEnabled);
            }
        }
    }

    if (result == Result::Success)
    {
        result = pCopy->pCmdBuffer->End();
    }

    if (result == Result::Success)
    {
        result = m_pQueue->WaitQueueSemaphore(pRaft->pStartCopy);
    }

    if (result == Result::Success)
    {
        PerSubQueueSubmitInfo perSubQueueInfo = {};
        perSubQueueInfo.cmdBufferCount        = 1;
        perSubQueueInfo.ppCmdBuffers          = &pCopy->pCmdBuffer;
        MultiSubmitInfo submitInfo            = {};
        submitInfo.perSubQueueInfoCount       = 1;
        submitInfo.pPerSubQueueInfo           = &perSubQueueInfo;
        submitInfo.ppFences                   = &pCopy->pFence;
        submitInfo.fenceCount                 = 1;

        // Note that we're responsible for adding all command memory read by the upload queue to the per-submit memory
        // reference list. On platforms that do not have this feature the caller must guarantee residency. It is
        // difficult to uniquely identify each referenced command allocation so instead we track each chunk's base
        // allocation, including all duplicates.
        if (m_trackMemoryRefs)
        {
            submitInfo.gpuMemRefCount = m_chunkMemoryRefs.NumElements();
            submitInfo.pGpuMemoryRefs = &m_chunkMemoryRefs.At(0);
        }

        result = m_pQueue->Submit(submitInfo);

        m_chunkMemoryRefs.Clear();
    }

    if (result == Result::Success)
    {
        result = m_pQueue->SignalQueueSemaphore(pRaft->pEndCopy);
    }

    if (result == Result::Success)
    {
        pUploadInfo->uploadedCmdBuffers = uploadedCmdBuffers;
        pUploadInfo->uploadedCmdStreams = m_createInfo.numCmdStreams;
        pUploadInfo->pUploadComplete    = pRaft->pEndCopy;
        pUploadInfo->pExecutionComplete = pRaft->pStartCopy;

        for (uint32 idx = 0; idx < m_createInfo.numCmdStreams; ++idx)
        {
            // In theory all command buffers could have empty streams of the same type (e.g., no CE commands). In that
            // case we can just leave a hole in the stream array.
            if (streamState[idx].launchBytes > 0)
            {
                pUploadInfo->streamInfo[idx].flags         = streamState[idx].flags;
                pUploadInfo->streamInfo[idx].engineType    = streamState[idx].engineType;
                pUploadInfo->streamInfo[idx].subEngineType = streamState[idx].subEngineType;
                pUploadInfo->streamInfo[idx].pGpuMemory    = pRaft->pGpuMemory[idx];
                pUploadInfo->streamInfo[idx].launchSize    = streamState[idx].launchBytes;
            }
            else
            {
                memset(&pUploadInfo->streamInfo[idx], 0, sizeof(pUploadInfo->streamInfo[idx]));
            }
        }
    }

    return result;
}

// =====================================================================================================================
CmdUploadRing::Raft* CmdUploadRing::NextRaft()
{
    // Wrap the ring index using bit masking which works if the size is a power of two.
    PAL_ASSERT(IsPowerOfTwo(RaftRingSize));
    m_prevRaft = (m_prevRaft + 1) & (RaftRingSize - 1);

    return &m_raft[m_prevRaft];
}

// =====================================================================================================================
CmdUploadRing::Copy* CmdUploadRing::NextCopy()
{
    // Wrap the ring index using bit masking which works if the size is a power of two.
    PAL_ASSERT(IsPowerOfTwo(CopyRingSize));
    m_prevCopy = (m_prevCopy + 1) & (CopyRingSize - 1);

    return &m_copy[m_prevCopy];
}

// =====================================================================================================================
// A helper function for UploadCmdBuffers which ends the current uploaded IB and chains the previous IB to it.
void CmdUploadRing::EndCurrentIb(
    const IGpuMemory& raftMemory,
    ICmdBuffer*       pCopyCmdBuffer,
    UploadState*      pState)
{
    // Compute the total size of the IB including the chaining postamble.
    const gpusize curIbTotalBytes = Pow2Align(pState->curIbSizeBytes + m_minPostambleBytes, m_sizeAlignBytes);

    // Remember the total size of the first IB so that the caller can launch it.
    if (pState->launchBytes == 0)
    {
        pState->launchBytes = curIbTotalBytes;
    }

    // Write the previous IB's postamble now that we know the current IB's final size.
    if (pState->prevIbPostambleSize > 0)
    {
        UploadChainPostamble(raftMemory,
                             pCopyCmdBuffer,
                             pState->prevIbPostambleOffset,
                             pState->prevIbPostambleSize,
                             pState->curIbOffset,
                             curIbTotalBytes,
                             (pState->subEngineType == SubEngineType::ConstantEngine),
                             pState->flags.isPreemptionEnabled);
    }

    // The current IB is now the previous IB; we will patch its chain later.
    pState->prevIbPostambleOffset = pState->raftFreeOffset;
    pState->prevIbPostambleSize   = curIbTotalBytes - pState->curIbSizeBytes;
    pState->curIbOffset           = 0;
    pState->curIbSizeBytes        = 0;
    pState->curIbFreeBytes        = 0;

    PAL_ASSERT(pState->prevIbPostambleSize >= m_minPostambleBytes);

    // Advance the raft offset assuming we have the space to start a new IB with the proper address alignment and a
    // chain postamble.
    pState->raftFreeOffset = Pow2Align(pState->raftFreeOffset + pState->prevIbPostambleSize, m_addrAlignBytes);
}

} // Pal
