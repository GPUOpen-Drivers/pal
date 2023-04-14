/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/crashAnalysis/crashAnalysisCmdBuffer.h"
#include "core/layers/crashAnalysis/crashAnalysisDevice.h"
#include "core/layers/crashAnalysis/crashAnalysisPlatform.h"
#include "core/layers/crashAnalysis/crashAnalysisQueue.h"

#include "palVectorImpl.h"
#include "palBuddyAllocatorImpl.h"
#include "palIntrusiveListImpl.h"

using namespace Util;

static const gpusize BaseAllocSize = Pow2Pad<gpusize>(VirtualPageSize());
static const gpusize SubAllocSize  = Pow2Pad<gpusize>(sizeof(Pal::CrashAnalysis::MarkerState));
constexpr    gpusize Alignment     = 0;

namespace Pal
{
namespace CrashAnalysis
{

// =====================================================================================================================
Device::Device(
    PlatformDecorator* pPlatform,
    IDevice*           pNextDevice)
    :
    DeviceDecorator(pPlatform, pNextDevice),
    m_pPublicSettings(nullptr),
    m_deviceProperties{},
    m_queues(),
    m_queueLock(),
    m_memoryLock(),
    m_initialized(false),
    m_memoryRafts(m_pPlatform)
{
}

// =====================================================================================================================
Device::~Device()
{
    PAL_ASSERT(m_queues.IsEmpty());
}

// =====================================================================================================================
// Adds a Queue to an internal tracking list. This should be called whenever a Queue has been created on this Device.
// The tracking list should be an accurate reflection of all queues which are currently owned by this Device.
void Device::TrackQueue(
    Queue* pQueue)
{
    PAL_ASSERT(pQueue != nullptr);
    MutexAuto lock(&m_queueLock);
    m_queues.PushFront(pQueue->DeviceMembershipNode());
}

// =====================================================================================================================
// Remove a Queue from the internal tracking list. This should be called once a Queue has finished execution and is
// preparing to be destroyed.
void Device::UntrackQueue(
    Queue* pQueue)
{
    PAL_ASSERT(pQueue != nullptr);
    MutexAuto lock(&m_queueLock);
    m_queues.Erase(pQueue->DeviceMembershipNode());
}

// =====================================================================================================================
Result Device::GetMemoryChunk(
    MemoryChunk** ppMemChunk)
{
    MutexAuto lock(&m_memoryLock);

    Result result = Result::ErrorOutOfMemory;

    if (ppMemChunk != nullptr)
    {
        MemoryChunk* pChunk = PAL_NEW(MemoryChunk, m_pPlatform, AllocInternal)(this);
        gpusize      offset = 0;

        // Attempt to find a free allocation in one of the Buddy Allocators
        for (int i = 0; ((result != Result::Success) && i < m_memoryRafts.size()); i++)
        {
            // Attempt to claim (take a lock) on a new allocation from the Buddy Allocator
            result = m_memoryRafts[i].pBuddyAllocator->ClaimGpuMemory(SubAllocSize, Alignment);

            if (result == Result::Success)
            {
                // If the claim was successful, attempt to allocate the memory
                result = m_memoryRafts[i].pBuddyAllocator->Allocate(SubAllocSize, Alignment, &offset);

                // Since the memory was already claimed, Allocate() should never fail.
                PAL_ASSERT(result == Result::Success);
            }

            if (result == Result::Success)
            {
                pChunk->raftIndex   = i;
                pChunk->gpuVirtAddr = offset + m_memoryRafts[i].pGpuMemory->Desc().gpuVirtAddr;
                pChunk->pCpuAddr    = static_cast<MarkerState*>(
                    VoidPtrInc(m_memoryRafts[i].pSystemMemory, offset));
            }
        }

        // If no free allocations are available, create a new raft and allocate from that
        if (result != Result::Success)
        {
            result = CreateMemoryRaft();

            if (result == Result::Success)
            {
                RaftAllocator& raft = m_memoryRafts.Back();

                // Attempt to claim (take a lock) on a new allocation from the Buddy Allocator
                result = raft.pBuddyAllocator->ClaimGpuMemory(SubAllocSize, Alignment);

                if (result == Result::Success)
                {
                    // If the claim was successful, attempt to allocate the memory
                    result = raft.pBuddyAllocator->Allocate(SubAllocSize, Alignment, &offset);

                    // Since the memory was already claimed, Allocate() should never fail.
                    PAL_ASSERT(result == Result::Success);
                }

                if (result == Result::Success)
                {
                    pChunk->raftIndex   = m_memoryRafts.size() - 1;
                    pChunk->gpuVirtAddr = (raft.pGpuMemory->Desc().gpuVirtAddr) + offset;
                    pChunk->pCpuAddr    = static_cast<MarkerState*>(VoidPtrInc(raft.pSystemMemory, offset));
                }
            }
        }

        if (result == Result::Success)
        {
            (*ppMemChunk) = pChunk;
        }
    }

    return result;
}

// =====================================================================================================================
void Device::FreeMemoryChunkAllocation(
    uint32  raftIndex,
    gpusize gpuVirtAddr)
{
    if (raftIndex < m_memoryRafts.size())
    {
        RaftAllocator& raft = m_memoryRafts[raftIndex];

        // 'Free' operates on offsets relative to the base allocation, not absolute virtual addresses
        raft.pBuddyAllocator->Free(gpuVirtAddr - raft.pGpuMemory->Desc().gpuVirtAddr);
    }
}

// =====================================================================================================================
Result Device::CreateMemoryRaft()
{
    RaftAllocator raft   = { };
    raft.pGpuMemory      = nullptr;
    raft.pSystemMemory   = nullptr;
    raft.pBuddyAllocator = PAL_NEW(Util::BuddyAllocator<IPlatform>, m_pPlatform, AllocInternal)
                                  (m_pPlatform, BaseAllocSize, SubAllocSize);

    Result result = (raft.pBuddyAllocator != nullptr) ? Result::Success : Result::ErrorOutOfMemory;

    if (result == Result::Success)
    {
        result = raft.pBuddyAllocator->Init();
    }

    // Create an IGpuMemory object, sized to `m_totalRaftSize`
    if (result == Result::Success)
    {
        // Define the memory mapping schema. Critically, this memory must be
        // visible to the CPU, and it must *not* be cacheable.
        GpuMemoryCreateInfo createInfo = { };
        createInfo.size                = BaseAllocSize;
        createInfo.vaRange             = VaRange::Default;
        createInfo.heapCount           = 1;
        createInfo.heaps[0]            = GpuHeap::GpuHeapGartUswc;
        createInfo.priority            = GpuMemPriority::Normal;
        createInfo.priorityOffset      = GpuMemPriorityOffset::Offset0;
        createInfo.flags.gl2Uncached   = 1;
        createInfo.flags.cpuInvisible  = 0;

        raft.pGpuMemory =
            static_cast<IGpuMemory*>(PAL_MALLOC(GetNextLayer()->GetGpuMemorySize(createInfo, &result),
                                                m_pPlatform,
                                                AllocInternal));

        if ((result == Result::Success) && (raft.pGpuMemory != nullptr))
        {
            result = GetNextLayer()->CreateGpuMemory(createInfo,
                                                     raft.pGpuMemory,
                                                     &raft.pGpuMemory);

            if (result != Result::Success)
            {
                PAL_SAFE_FREE(raft.pGpuMemory, m_pPlatform);
            }
        }
    }

    // Map the created IGpuMemory object into CPU-visible memory.
    if (result == Result::Success)
    {
        GpuMemoryRef memRef = { };
        memRef.pGpuMemory   = raft.pGpuMemory;

        result = GetNextLayer()->AddGpuMemoryReferences(
            1, &memRef, nullptr, GpuMemoryRefCantTrim);

        if (result == Result::Success)
        {
            result = raft.pGpuMemory->Map(&raft.pSystemMemory);

            if (result != Result::Success)
            {
                GetNextLayer()->RemoveGpuMemoryReferences(
                    1, &raft.pGpuMemory, nullptr);
            }
        }

        if (result != Result::Success)
        {
            raft.pGpuMemory->Destroy();
            PAL_SAFE_FREE(raft.pGpuMemory, m_pPlatform);
        }
    }

    if (result == Result::Success)
    {
        result = m_memoryRafts.PushBack(raft);
        PAL_ASSERT(result == Result::Success);
    }
    else
    {
        // IGpuMemory should be freed by this point.
        // The only outstanding allocation is the buddy allocator.
        if (raft.pBuddyAllocator != nullptr)
        {
            PAL_SAFE_DELETE(raft.pBuddyAllocator, m_pPlatform);
        }
    }

    return result;
}

// =====================================================================================================================
void Device::FreeMemoryRafts()
{
    for (auto& raft : m_memoryRafts)
    {
        if (raft.pBuddyAllocator != nullptr)
        {
            PAL_ASSERT(raft.pBuddyAllocator->IsEmpty());
            PAL_SAFE_DELETE(raft.pBuddyAllocator, m_pPlatform);
        }

        if (raft.pGpuMemory != nullptr)
        {
            Result result = GetNextLayer()->RemoveGpuMemoryReferences(
                1, &raft.pGpuMemory, nullptr);

            if (result == Result::Success)
            {
                result = raft.pGpuMemory->Unmap();
            }

            PAL_ASSERT(result == Result::Success);

            raft.pGpuMemory->Destroy();

            PAL_SAFE_FREE(raft.pGpuMemory, m_pPlatform);

            raft.pSystemMemory = nullptr;
        }
    }

    m_memoryRafts.Clear();
}

// =====================================================================================================================
// Iterates through the queue list requesting each to log its crash analysis marker data
void Device::LogCrashAnalysisMarkerData()
{
    MutexAuto lock(&m_queueLock);
    for (auto iter = m_queues.Begin(); iter.IsValid(); iter.Next())
    {
        iter.Get()->LogCrashAnalysisMarkerData();
    }
}

// =====================================================================================================================
Result Device::CommitSettingsAndInit()
{
    Result result = DeviceDecorator::CommitSettingsAndInit();

    m_pPublicSettings = GetNextLayer()->GetPublicSettings();

    return result;
}

// =====================================================================================================================
Result Device::Finalize(
    const DeviceFinalizeInfo& finalizeInfo)
{
    Result result = DeviceDecorator::Finalize(finalizeInfo);

    PAL_ASSERT(m_initialized == false);

    if (result == Result::Success)
    {
        result = GetProperties(&m_deviceProperties);
    }

    if (result == Result::Success)
    {
        MutexAuto lock(&m_memoryLock);
        result = CreateMemoryRaft();
        PAL_ASSERT(m_memoryRafts.size() == 1);
    }

    if (result == Result::Success)
    {
        m_initialized = true;
    }

    return result;
}

// =====================================================================================================================
Result Device::Cleanup()
{
    Result result = DeviceDecorator::Cleanup();

    if (m_initialized)
    {
        MutexAuto lock(&m_memoryLock);
        FreeMemoryRafts();
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetCmdBufferSize(
    const CmdBufferCreateInfo& createInfo,
    Result*                    pResult
) const
{
    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    return GetNextLayer()->GetCmdBufferSize(nextCreateInfo, pResult) + sizeof(CrashAnalysis::CmdBuffer);
}

// =====================================================================================================================
Result Device::CreateCmdBuffer(
    const CmdBufferCreateInfo& createInfo,
    void*                      pPlacementAddr,
    ICmdBuffer**               ppCmdBuffer)
{
    ICmdBuffer* pNextCmdBuffer = nullptr;

    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    Result result = m_pNextLayer->CreateCmdBuffer(nextCreateInfo,
                                                  NextObjectAddr<CmdBuffer>(pPlacementAddr),
                                                  &pNextCmdBuffer);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextCmdBuffer != nullptr);
        pNextCmdBuffer->SetClientData(pPlacementAddr);

        (*ppCmdBuffer) = PAL_PLACEMENT_NEW(pPlacementAddr) CmdBuffer(pNextCmdBuffer, this, createInfo);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetQueueSize(
    const QueueCreateInfo& createInfo,
    Result*                pResult
) const
{
    return GetNextLayer()->GetQueueSize(createInfo, pResult) + sizeof(CrashAnalysis::Queue);
}

// =====================================================================================================================
Result Device::CreateQueue(
    const QueueCreateInfo& createInfo,
    void*                  pPlacementAddr,
    IQueue**               ppQueue)
{
    IQueue* pNextQueue = nullptr;
    Queue*  pQueue = nullptr;

    Result result = m_pNextLayer->CreateQueue(createInfo,
                                              NextObjectAddr<Queue>(pPlacementAddr),
                                              &pNextQueue);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextQueue != nullptr);

        pQueue = PAL_PLACEMENT_NEW(pPlacementAddr) Queue(pNextQueue, this, 1);
        result = static_cast<Queue*>(pQueue)->Init(&createInfo);

        if (result == Result::Success)
        {
            pNextQueue->SetClientData(pPlacementAddr);
            (*ppQueue) = pQueue;
        }
        else
        {
            pQueue->Destroy();
        }
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetMultiQueueSize(
    uint32                 queueCount,
    const QueueCreateInfo* pCreateInfo,
    Result*                pResult
) const
{
    return GetNextLayer()->GetMultiQueueSize(queueCount, pCreateInfo, pResult) + sizeof(CrashAnalysis::Queue);
}

// =====================================================================================================================
Result Device::CreateMultiQueue(
    uint32                 queueCount,
    const QueueCreateInfo* pCreateInfo,
    void*                  pPlacementAddr,
    IQueue**               ppQueue)
{
    IQueue* pNextQueue = nullptr;
    Queue*  pQueue     = nullptr;

    Result result = m_pNextLayer->CreateMultiQueue(queueCount,
                                                   pCreateInfo,
                                                   NextObjectAddr<Queue>(pPlacementAddr),
                                                   &pNextQueue);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextQueue != nullptr);

        pQueue = PAL_PLACEMENT_NEW(pPlacementAddr) Queue(pNextQueue, this, queueCount);
        result = static_cast<Queue*>(pQueue)->Init(pCreateInfo);

        if (result == Result::Success)
        {
            pNextQueue->SetClientData(pPlacementAddr);
            (*ppQueue) = pQueue;
        }
        else
        {
            pQueue->Destroy();
        }
    }

    return result;
}

} // namespace CrashAnalysis
} // namespace Pal
