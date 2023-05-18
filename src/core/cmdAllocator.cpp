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

#include "core/cmdAllocator.h"
#include "core/device.h"
#include "core/platform.h"
#include "palFile.h"
#include "palHashMap.h"
#include "palHashMapImpl.h"
#include "palIntrusiveListImpl.h"
#include "palMutex.h"
#include "palVectorImpl.h"
#include "palLiterals.h"
#include "eventDefs.h"

#include <limits.h>

using namespace Util;
using namespace Util::Literals;

namespace Pal
{

// =====================================================================================================================
// Determines how much space is required to hold a CmdAllocator and its optional Mutex.
size_t CmdAllocator::GetSize(
    const CmdAllocatorCreateInfo& createInfo,
    Result*                       pResult)    // [optional] The additional validation result is stored here.
{
    // We need extra space for two Mutex objects if the allocator is thread safe.
    size_t size = sizeof(CmdAllocator) + GetPlacementSize(createInfo);

    // Validate the createInfo if requested.
    if (pResult != nullptr)
    {
        for (uint32 i = 0; i < CmdAllocatorTypeCount; ++i)
        {
            const auto& allocInfo = createInfo.allocInfo[i];

            // Check for the following requirements:
            // - The suballocation size is a multiple of 4k; this is a simple way to meet engine alignment requirements.
            // - The allocation size is an integer multiple of the suballocation size.
            // - The allocation heap is CPU-mappable (in other words, not invisible).
            if (IsPow2Aligned(allocInfo.suballocSize, PAL_PAGE_BYTES)                       &&
                (allocInfo.allocSize % allocInfo.suballocSize == 0)                         &&
                ((i == GpuScratchMemAlloc) || (allocInfo.allocHeap != GpuHeapInvisible)))
            {
                *pResult = Result::Success;
            }
            else
            {
                *pResult = Result::ErrorInvalidValue;
                size     = 0;
                break;
            }
        }
    }

    return size;
}

// =====================================================================================================================
// Returns the ammount of additional placement memory required by this class.
size_t CmdAllocator::GetPlacementSize(
    const CmdAllocatorCreateInfo& createInfo)
{
    // We need extra space for two Mutex objects if the allocator is thread safe.
    return createInfo.flags.threadSafe ? (2 * sizeof(Mutex)) : 0;
}

// =====================================================================================================================
CmdAllocator::CmdAllocator(
    Device*                       pDevice,
    const CmdAllocatorCreateInfo& createInfo)
    :
    m_pDevice(pDevice),
    m_pChunkLock(nullptr),
    m_lastPagingFence(0),
    m_pLinearAllocLock(nullptr),
    m_pDummyChunkAllocation(nullptr),
    m_pPlatform(pDevice->GetPlatform())
{
#if PAL_ENABLE_PRINTS_ASSERTS
    memset(m_pHistograms, 0, sizeof(m_pHistograms));
    m_numHistogramBins = 0;
#endif

    m_flags.u32All          = 0;
    m_flags.autoMemoryReuse = createInfo.flags.autoMemoryReuse;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 707
    m_flags.autoTrimMemory  = createInfo.flags.autoTrimMemory;
#endif

    if (createInfo.flags.disableBusyChunkTracking == 0)
    {
        m_flags.trackBusyChunks = m_flags.autoMemoryReuse;
    }

    const uint32 residencyFlags = m_pDevice->GetPublicSettings()->cmdAllocResidency;
    const bool   noInvisibleMem = (m_pDevice->HeapLogicalSize(GpuHeapInvisible) == 0);

    for (uint32 i = 0; i < CmdAllocatorTypeCount; ++i)
    {
        // It's legal to use the local heap but it might not work as expected. We keep all chunk allocations mapped
        // forever so local allocations might be migrated to system memory by the OS. If local chunks are strongly
        // desired then we should rework the chunk management logic and internal memory logic so that command chunks
        // are not mapped while they are referenced on the GPU.  This does not apply when large BAR support is
        // available.
        if (m_pDevice->HasLargeLocalHeap() == false)
        {
            PAL_ALERT(createInfo.allocInfo[i].allocHeap == GpuHeapLocal);
        }

        memset(&m_gpuAllocInfo[i].allocCreateInfo, 0, sizeof(m_gpuAllocInfo[i].allocCreateInfo));

        if (createInfo.allocInfo[i].allocSize == 0)
        {
            continue;
        }

        m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.priority  = GpuMemPriority::Normal;
        m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.vaRange   = VaRange::Default;

        if (i != GpuScratchMemAlloc)
        {
            m_gpuAllocInfo[i].allocCreateInfo.flags.cpuAccessible = 1;

            m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.heapCount = 2;
            m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.heaps[0]  = createInfo.allocInfo[i].allocHeap;
            m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.heaps[1]  = GpuHeapGartCacheable;
        }
        else
        {
            if (m_pDevice->ChipProperties().gpuType == GpuType::Integrated)
            {
                m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.heapCount = 2;
                m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.heaps[0]  = GpuHeapGartUswc;
                m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.heaps[1]  = GpuHeapGartCacheable;
            }
            else
            {
                if (noInvisibleMem)
                {
                    m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.heapCount = 1;
                    m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.heaps[0] = GpuHeapLocal;
                }
                else
                {
                    m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.heapCount = 2;
                    m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.heaps[0] = GpuHeapInvisible;
                    m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.heaps[1] = GpuHeapLocal;

                }
            }
        }

        m_gpuAllocInfo[i].allocCreateInfo.memObjInternalInfo.flags.alwaysResident = 1;
        m_gpuAllocInfo[i].allocCreateInfo.memObjInternalInfo.flags.isCmdAllocator = 1;

        // If wait-on-submit residency is enabled we must request a paging fence for each allocation. Otherwise we will
        // implicitly wait for each allocation to be resident at create-time.
        if (TestAnyFlagSet(residencyFlags, (1 << i)))
        {
            m_gpuAllocInfo[i].allocCreateInfo.flags.optimizePaging = 1;
        }

        constexpr gpusize CmdAllocatorAlignment = 4096;
        m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.alignment = CmdAllocatorAlignment;
        m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.size      = createInfo.allocInfo[i].allocSize;

        // We assume that chunks are no larger than 4GB.
        PAL_ASSERT(HighPart(createInfo.allocInfo[i].suballocSize) == 0);

        m_gpuAllocInfo[i].allocCreateInfo.chunkSize = LowPart(createInfo.allocInfo[i].suballocSize);
        m_gpuAllocInfo[i].allocCreateInfo.numChunks = LowPart(createInfo.allocInfo[i].allocSize /
                                                              createInfo.allocInfo[i].suballocSize);

        // Only enable staging buffers for command allocations.
        m_gpuAllocInfo[i].allocCreateInfo.flags.enableStagingBuffer =
            (i == CommandDataAlloc) && pDevice->Settings().cmdBufChunkEnableStagingBuffer;

        if (i == CommandDataAlloc)
        {
            // There are a couple of places where we'd like to know if command data is in local memory.
            const GpuHeap preferredHeap = m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.heaps[0];

            m_flags.localCmdData = ((preferredHeap == GpuHeapLocal) || (preferredHeap == GpuHeapInvisible));

            // Command data is by definition resident in UDMA buffers.
            m_gpuAllocInfo[i].allocCreateInfo.memObjInternalInfo.flags.udmaBuffer = 1;

            // Command chunks are never written from the gpu, except for the busy tracker fence.
            // We can set read-only if the busy-tracker is disabled or forced read-only (moves the tracker to a RW page)
            m_gpuAllocInfo[i].allocCreateInfo.memObjInternalInfo.flags.gpuReadOnly =
                m_pDevice->Settings().cmdStreamReadOnly;

            // Local memory is low latency and high bandwidth so we don't need to pollute the L2 cache with commands.
            // We still need to use the cache if the commands are in system memory, it's too slow without it.
            //
            // Note that we must also use the cache if we require busy tracking because some queues use L2 atomics
            // to increment the busy tracker counters.
            if (((m_flags.localCmdData == 1) || (m_pDevice->Settings().cmdBufForceUc == 1)) &&
                (m_flags.trackBusyChunks == 0))
            {
                m_gpuAllocInfo[i].allocCreateInfo.memObjInternalInfo.mtype = MType::Uncached;
            }
        }
        else if ((i == EmbeddedDataAlloc) || (i == GpuScratchMemAlloc))
        {
            m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.vaRange = VaRange::DescriptorTable;
        }
        m_gpuAllocInfo[i].allocFreeThreshold = createInfo.allocInfo[i].allocFreeThreshold;
    }

    // The system-memory command allocation info should be a duplicate of the GPU memory ones, but with zero GPU
    // memory heaps selected.
    m_sysAllocInfo.allocCreateInfo = m_gpuAllocInfo[CommandDataAlloc].allocCreateInfo;
    m_sysAllocInfo.allocCreateInfo.memObjCreateInfo.heapCount = 0;

    ResourceDescriptionCmdAllocator desc = {};
    desc.pCreateInfo = &createInfo;
    ResourceCreateEventData data = {};
    data.type = ResourceType::CmdAllocator;
    data.pResourceDescData = static_cast<void*>(&desc);
    data.resourceDescSize = sizeof(ResourceDescriptionCmdAllocator);
    data.pObj = this;
    m_pPlatform->GetGpuMemoryEventProvider()->LogGpuMemoryResourceCreateEvent(data);
}

// =====================================================================================================================
// Destroys all command stream allocations and thus all command stream chunks. All command streams should have returned
// all chunks by now; if not they will suddenly find themselves without any valid chunks.
CmdAllocator::~CmdAllocator()
{
    ResourceDestroyEventData data = {};
    data.pObj = this;
    m_pPlatform->GetGpuMemoryEventProvider()->LogGpuMemoryResourceDestroyEvent(data);

    // We must explicitly invoke the mutexes' destructors because we created them using placement new.
    if (m_pChunkLock != nullptr)
    {
        m_pChunkLock->~Mutex();
        m_pChunkLock = nullptr;
    }

    if (m_pLinearAllocLock != nullptr)
    {
        m_pLinearAllocLock->~Mutex();
        m_pLinearAllocLock = nullptr;
    }

    FreeAllChunks(m_pPlatform->IsSubAllocTrackingEnabled());
    FreeAllLinearAllocators();

    // Free the dummy chunk.
    if (m_pDummyChunkAllocation != nullptr)
    {
        m_pDummyChunkAllocation->Destroy(m_pDevice);
        PAL_SAFE_FREE(m_pDummyChunkAllocation, m_pPlatform);
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    if (m_pDevice->Settings().logCmdBufCommitSizes)
    {
        PrintCommitLog();

        // Print and free the histograms.
        for (uint32 histIdx = 0; histIdx < HistogramCount; ++histIdx)
        {
            PAL_SAFE_FREE(m_pHistograms[histIdx], m_pPlatform);
        }
    }
#endif
}

// =====================================================================================================================
// Transfers all chunks from the src list to the dst list.
void CmdAllocator::TransferChunks(
    ChunkList* pDstList,
    ChunkList* pSrcList)
{
    if (pSrcList->IsEmpty() == false)
    {
#if PAL_ENABLE_PRINTS_ASSERTS
        for (auto iter = pSrcList->Begin(); iter.IsValid(); iter.Next())
        {
            // The caller must guarantee that all of these chunks have expired so we should never have to check
            // the busy trackers. That being said, we should protect ourselves and validate the chunk busy
            // trackers in builds with asserts enabled.
            PAL_ASSERT((TrackBusyChunks() == false) || iter.Get()->IsIdleOnGpu());
        }
#endif

        pDstList->PushFrontList(pSrcList);
    }
}

// =====================================================================================================================
void CmdAllocator::FreeAllChunks(
    const bool trackSuballocations)
{
    CmdAllocInfo*const pAllocInfo[] =
    {
        &m_gpuAllocInfo[CommandDataAlloc],
        &m_gpuAllocInfo[EmbeddedDataAlloc],
        &m_gpuAllocInfo[GpuScratchMemAlloc],
        &m_sysAllocInfo,
    };
    static_assert(ArrayLen(pAllocInfo) == (CmdAllocatorTypeCount + 1),
                  "Unexpected number of command allocation memory types!");

#if PAL_ENABLE_PRINTS_ASSERTS
    // The caller must guarantee that all of these chunks have expired so we should never have to check the busy
    // trackers. That being said, we should protect ourselves and validate the chunk busy-trackers in builds with
    // asserts enabled.
    if (TrackBusyChunks())
    {
        for (uint32 i = 0; i < (CmdAllocatorTypeCount + 1); ++i)
        {
            for (auto iter = pAllocInfo[i]->busyList.Begin(); iter.IsValid(); iter.Next())
            {
                PAL_ASSERT(iter.Get()->IsIdleOnGpu());
            }

            for (auto iter = pAllocInfo[i]->reuseList.Begin(); iter.IsValid(); iter.Next())
            {
                PAL_ASSERT(iter.Get()->IsIdleOnGpu());
            }
        }
    }
#endif

    // Note that as soon as we start destroying allocations our command chunk's head chunks become invalid. Nothing
    // called in this loop can access those head chunks.
    for (uint32 i = 0; i < (CmdAllocatorTypeCount + 1); ++i)
    {
        if ((i < CmdAllocatorTypeCount) && trackSuballocations)
        {
            // Only report suballocations for gpu memory types, not system memory
            for (auto iter = pAllocInfo[i]->busyList.Begin(); iter.IsValid(); iter.Next())
            {
                ReportSuballocationEvent(Developer::CallbackType::SubFreeGpuMemory, iter.Get());
            }
        }

        // Empty out the chunk lists so we can destroy the chunks.
        pAllocInfo[i]->freeList.EraseAll();
        pAllocInfo[i]->busyList.EraseAll();
        pAllocInfo[i]->reuseList.EraseAll();

        // Destroy all allocations (which also destroys all chunks).
        for (auto iter = pAllocInfo[i]->allocList.Begin(); iter.IsValid();)
        {
            CmdStreamAllocation* pAlloc = iter.Get();

            // Remove an allocation from the list and destroy it.
            pAllocInfo[i]->allocList.Erase(&iter);

            pAlloc->Destroy(m_pDevice);
            PAL_SAFE_FREE(pAlloc, m_pPlatform);
        }
    }
}

// =====================================================================================================================
// Removes all linear allocators from our lists and deletes them.
void CmdAllocator::FreeAllLinearAllocators()
{
    for (auto iter = m_linearAllocFreeList.Begin(); iter.IsValid();)
    {
        VirtualLinearAllocatorWithNode*const pAllocator = iter.Get();
        m_linearAllocFreeList.Erase(&iter);
        PAL_DELETE(pAllocator, m_pPlatform);
    }

    for (auto iter = m_linearAllocBusyList.Begin(); iter.IsValid();)
    {
        VirtualLinearAllocatorWithNode*const pAllocator = iter.Get();
        m_linearAllocBusyList.Erase(&iter);
        PAL_DELETE(pAllocator, m_pPlatform);
    }
}

// =====================================================================================================================
Result CmdAllocator::Init(
    const CmdAllocatorCreateInfo& createInfo,
    void* pPlacementAddr)
{
    Result result = Result::Success;

    // Initialize the allocator's mutexes if they are necessary
    if (createInfo.flags.threadSafe)
    {
        m_pChunkLock = PAL_PLACEMENT_NEW(pPlacementAddr) Mutex();
        m_pLinearAllocLock = PAL_PLACEMENT_NEW(m_pChunkLock + 1) Mutex();
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    const auto& settings = m_pDevice->Settings();

    if (settings.logCmdBufCommitSizes && (result == Result::Success))
    {
        const uint32 reserveLimit = Device::CmdStreamReserveLimit;

        // If these are both powers of two (as required) then we can just divide them to get the correct bin count.
        // Note that we have to add one to that count to get the "zero" bin.
        PAL_ASSERT(IsPowerOfTwo(Device::CmdStreamReserveLimit) && IsPowerOfTwo(HistogramStep));

        m_numHistogramBins = 1 + Device::CmdStreamReserveLimit / HistogramStep;

        for (uint32 histIdx = 0; histIdx < HistogramCount; ++histIdx)
        {
            // We must use CALLOC to make sure that all bins start out zeroed.
            m_pHistograms[histIdx] = static_cast<uint64*>(PAL_CALLOC(m_numHistogramBins * sizeof(uint64),
                                                                     m_pPlatform,
                                                                     AllocInternal));
            if (m_pHistograms[histIdx] == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
        }
    }
#endif

    // Initialize the dummy chunk
    if (result == Result::Success)
    {
        result = CreateDummyChunkAllocation();
    }

    return result;
}

// =====================================================================================================================
// Destroys this object assuming it was allocated via CreateInternalCmdAllocator().
void CmdAllocator::DestroyInternal()
{
    Platform*const pPlatform = m_pPlatform;
    Destroy();
    PAL_FREE(this, pPlatform);
}

// =====================================================================================================================
// Informs the command allocator that all of its CmdStreamChunks are no longer being referenced by the GPU.
Result CmdAllocator::Reset(
    bool freeMemory)
{
    const bool trackSuballocations = m_pPlatform->IsSubAllocTrackingEnabled();

    if (m_pChunkLock != nullptr)
    {
        m_pChunkLock->Lock();
    }

    if (freeMemory)
    {
        // We've been asked to simply destroy all of our allocations on each reset.
        FreeAllChunks(trackSuballocations);
    }
    else
    {
        for (uint32 i = 0; i < CmdAllocatorTypeCount; ++i)
        {
            if (trackSuballocations)
            {
                for (auto iter = m_gpuAllocInfo[i].busyList.Begin(); iter.IsValid(); iter.Next())
                {
                    ReportSuballocationEvent(Developer::CallbackType::SubFreeGpuMemory, iter.Get());
                }
            }

            TransferChunks(&m_gpuAllocInfo[i].freeList, &m_gpuAllocInfo[i].busyList);
            TransferChunks(&m_gpuAllocInfo[i].freeList, &m_gpuAllocInfo[i].reuseList);
        }

        TransferChunks(&m_sysAllocInfo.freeList, &m_sysAllocInfo.busyList);
        TransferChunks(&m_sysAllocInfo.freeList, &m_sysAllocInfo.reuseList);
    }

    if (m_pChunkLock != nullptr)
    {
        m_pChunkLock->Unlock();
    }

    // Apply the same logic to our lists of linear allocators.
    if (m_pLinearAllocLock != nullptr)
    {
        m_pLinearAllocLock->Lock();
    }

    if (freeMemory)
    {
        FreeAllLinearAllocators();
    }
    else if (m_linearAllocBusyList.IsEmpty() == false)
    {
        m_linearAllocFreeList.PushFrontList(&m_linearAllocBusyList);
    }

    if (m_pLinearAllocLock != nullptr)
    {
        m_pLinearAllocLock->Unlock();
    }

    return Result::Success;
}

// =====================================================================================================================
// Informs the command allocator to trim down its allocations.
Result CmdAllocator::Trim(
    uint32 allocTypeMask,
    uint32 dynamicThreshold)
{
    Result result = Result::Success;

    if (m_pChunkLock != nullptr)
    {
        m_pChunkLock->Lock();
    }

    for (uint32 type = 0; type < CmdAllocatorTypeCount; ++type)
    {
        if (allocTypeMask & (1 << type))
        {
            CmdAllocInfo* const pAllocInfo = &m_gpuAllocInfo[type];
            const uint32 threshold = Max(dynamicThreshold, pAllocInfo->allocFreeThreshold);

            // Avoid trim overhead when there are not enough free chunks
            if (pAllocInfo->freeList.NumElements() > threshold * pAllocInfo->allocCreateInfo.numChunks)
            {
                result = TrimMemory(pAllocInfo, threshold);

                if (result != Result::Success)
                {
                    break;
                }
            }
        }
    }

    if (m_pChunkLock != nullptr)
    {
        m_pChunkLock->Unlock();
    }

    return result;
}

// =====================================================================================================================
// Helper function to destroy allocations of the given type where its chunks are all idle.
// A minimum of allocFreeThreshold free allocation will be kept around for fast reuse.
Result CmdAllocator::TrimMemory(
    CmdAllocInfo* const pAllocInfo,
    uint32              allocFreeThreshold)
{
    // m_pChunkLock should be handled by the caller

    // First, build a hash map of command allocations with at least one free chunk to their number of free chunks.
    // Keep track of how many allocations we see that only have free chunks.
    //
    // We use a temporary hash map for this because the trim code is the only place in PAL where we need to correlate
    // free chunks with their parent allocations. If we didn't build this state on demand we'd need to do something
    // like persistently track which chunks are free in each allocation and update the tracking state each time we
    // move chunks between the freeList and the reuseList and busyList. That would unnecessary overhead to clients/apps
    // that never trim their allocations. The trim code is expected to be somewhat expensive so we went with a solution
    // that offloads all free allocation overhead to the trim function.
    const uint32 chunksPerAlloc = pAllocInfo->allocCreateInfo.numChunks;
    uint32       numFreeAllocs  = 0;

    // The PAL hash map is really hard to use... 4 entries per bucket should roughly work out right on 64-bit builds.
    const uint32 numBuckets = RoundUpQuotient(static_cast<uint32>(pAllocInfo->allocList.NumElements()), 4u);
    HashMap<CmdStreamAllocation*, uint32, Platform> allocMap(numBuckets, m_pPlatform);

    Result result = allocMap.Init();

    if (result == Result::Success)
    {
        for (auto iter = pAllocInfo->freeList.Begin(); iter.IsValid(); iter.Next())
        {
            CmdStreamAllocation*const pAlloc     = iter.Get()->Allocation();
            bool                      existed    = false;
            uint32*                   pFreeCount = nullptr;

            result = allocMap.FindAllocate(pAlloc, &existed, &pFreeCount);

            if (result != Result::Success)
            {
                break;
            }

            const uint32 newCount = existed ? (*pFreeCount + 1) : 1;

            // It should be impossible to count more chunks than exist in an allocation.
            PAL_ASSERT(newCount <= chunksPerAlloc);

            *pFreeCount = newCount;

            if (newCount == chunksPerAlloc)
            {
                numFreeAllocs++;
            }
        }
    }

    if ((result == Result::Success) && (numFreeAllocs > allocFreeThreshold))
    {
        // Destroy free allocations until we have no more than allocFreeThreshold remaining.
        for (auto iter = allocMap.Begin(); iter.Get() != nullptr; iter.Next())
        {
            if (iter.Get()->value == chunksPerAlloc)
            {
                CmdStreamAllocation*const pAlloc = iter.Get()->key;

                // Make sure the chunks are removed from all the lists. Because we know all chunks are free we only
                // need to look at the free list.
                for (uint32 idx = 0; idx < chunksPerAlloc; ++idx)
                {
                    pAllocInfo->freeList.Erase(pAlloc->Chunks()[idx].ListNode());
                }

                // Remove the allocation from the list and destroy it. We don't bother to remove the now invalid key
                // from the hash map because we'll never iterate over it again.
                pAllocInfo->allocList.Erase(pAlloc->ListNode());

                pAlloc->Destroy(m_pDevice);
                PAL_FREE(pAlloc, m_pPlatform);

                // Bail out once we've freed enough allocations to fit in the threshold.
                if (--numFreeAllocs <= allocFreeThreshold)
                {
                    break;
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
Result CmdAllocator::QueryUtilizationInfo(
    CmdAllocType                 type,
    CmdAllocatorUtilizationInfo* pUtilizationInfo
    ) const
{
    Result result = Result::Success;

    if (pUtilizationInfo == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else
    {
        if (m_pChunkLock != nullptr)
        {
            m_pChunkLock->Lock();
        }

        const CmdAllocInfo* pAllocInfo = &m_gpuAllocInfo[type];
        pUtilizationInfo->numAllocations = static_cast<uint32>(pAllocInfo->allocList.NumElements());
        pUtilizationInfo->numFreeChunks  = static_cast<uint32>(pAllocInfo->freeList.NumElements());
        pUtilizationInfo->numBusyChunks  = static_cast<uint32>(pAllocInfo->busyList.NumElements());
        pUtilizationInfo->numReuseChunks = static_cast<uint32>(pAllocInfo->reuseList.NumElements());

        if (m_pChunkLock != nullptr)
        {
            m_pChunkLock->Unlock();
        }
    }

    return result;
}

// =====================================================================================================================
// Takes an iterator to a list of CmdStreamChunk(s) and moves them to the reuse list for use later.
void CmdAllocator::ReuseChunks(
    CmdAllocType   allocType,
    bool           systemMemory,
    VectorIter     iter)         // [in] A valid iterator to the first element in the vector of chunk refs.
{
    // System memory allocations are only allowed for command data!
    PAL_ASSERT((systemMemory == false) || (allocType == CommandDataAlloc));

    if (AutomaticMemoryReuse())
    {
        // If necessary, engage the chunk lock.
        if (m_pChunkLock != nullptr)
        {
            m_pChunkLock->Lock();
        }

        auto*const pAllocInfo = (systemMemory ? &m_sysAllocInfo : &m_gpuAllocInfo[allocType]);
        const bool trackSuballocations = (systemMemory == false) && m_pPlatform->IsSubAllocTrackingEnabled();

        // If the root chunk is idle, we can push all the chunks to the free list.
        if (iter.Get()->IsIdleOnGpu())
        {
            while (iter.IsValid())
            {
                // Move this chunk from the busy list to the front of the free list.
                auto*const pNode = iter.Get()->ListNode();

                if (trackSuballocations)
                {
                    ReportSuballocationEvent(Developer::CallbackType::SubFreeGpuMemory, iter.Get());
                }

                pAllocInfo->busyList.Erase(pNode);
                pAllocInfo->freeList.PushFront(pNode);

                iter.Next();
            }

            // Only trim when:
            // - enabled by the PAL client
            // - there are enough free chunks (that implies that there are more than allocFreeThreshold allocations)
            if (AutoTrimMemory() && (pAllocInfo->freeList.NumElements() >
                pAllocInfo->allocFreeThreshold * pAllocInfo->allocCreateInfo.numChunks))
            {
                // The core functionality of ReuseChunks always succeeds even if trimming fails. If we followed the
                // usual requirement of passing this result up to the caller it will quickly turn into a cascade of
                // failure throughout our command stream/buffer code. A trimming failure won't actually break anything,
                // it just won't fully trim unused memory, so it seems better to just assert and hide the failure.
                const Result result = TrimMemory(pAllocInfo, pAllocInfo->allocFreeThreshold);
                PAL_ASSERT(result == Result::Success);
            }
        }
        else
        {
            while (iter.IsValid())
            {
                // Move this chunk from the busy list to the front of the reuse list.
                auto*const pNode = iter.Get()->ListNode();

                if (trackSuballocations)
                {
                    ReportSuballocationEvent(Developer::CallbackType::SubFreeGpuMemory, iter.Get());
                }

                pAllocInfo->busyList.Erase(pNode);
                pAllocInfo->reuseList.PushFront(pNode);

                iter.Next();
            }
        }

        if (m_pChunkLock != nullptr)
        {
            m_pChunkLock->Unlock();
        }
    }
}

// =====================================================================================================================
// Obtains the next available CmdStreamChunk and returns a pointer to it.
Result CmdAllocator::GetNewChunk(
    CmdAllocType     allocType,
    bool             systemMemory,
    CmdStreamChunk** ppChunk)
{
    // System memory allocations are only allowed for command data!
    PAL_ASSERT((systemMemory == false) || (allocType == CommandDataAlloc));

    // If necessary, engage the chunk lock while we search for a free chunk.
    if (m_pChunkLock != nullptr)
    {
        m_pChunkLock->Lock();
    }

    Result result = FindFreeChunk(systemMemory, systemMemory ? &m_sysAllocInfo : &m_gpuAllocInfo[allocType], ppChunk);

    if (m_pChunkLock != nullptr)
    {
        m_pChunkLock->Unlock();
    }

    return result;
}

// =====================================================================================================================
// Searches the free and busy lists for a free chunk. A new CmdStreamAllocation will be created if needed.
Result CmdAllocator::FindFreeChunk(
    const bool       systemMemory,
    CmdAllocInfo*    pAllocInfo,
    CmdStreamChunk** ppChunk)
{
    Result result = Result::Success;
    CmdStreamChunk* pChunk = nullptr;
    const bool trackSuballocations = (systemMemory == false) && m_pPlatform->IsSubAllocTrackingEnabled();

    // Search the free-list first.
    if (pAllocInfo->freeList.IsEmpty() == false)
    {
        // Pop a chunk off of the free list because free chunks, by definition, are no longer in use by the GPU.
        pChunk = pAllocInfo->freeList.Back();
        PAL_ASSERT(pChunk->IsIdleOnGpu());

        pChunk->Reset();

        if (trackSuballocations)
        {
            ReportSuballocationEvent(Developer::CallbackType::SubAllocGpuMemory, pChunk);
        }

        // Move the chunk from the free list to the front of the busy list.
        auto*const pNode = pChunk->ListNode();
        pAllocInfo->freeList.Erase(pNode);
        pAllocInfo->busyList.PushFront(pNode);
    }
    else
    {
        if (AutomaticMemoryReuse())
        {
            // Search the reuse list for a chunk that expired after it was returned to us. Start at the end because
            // those chunks have been on the list the longest and are most likely to be idle.
            for (auto reuseIter = pAllocInfo->reuseList.End(); reuseIter.IsValid(); reuseIter.Prev())
            {
                if (reuseIter.Get()->IsIdleOnGpu())
                {
                    pChunk = reuseIter.Get();
                    pChunk->Reset();

                    if (trackSuballocations)
                    {
                        ReportSuballocationEvent(Developer::CallbackType::SubAllocGpuMemory, pChunk);
                    }

                    // Move this chunk from the reuse list to the front of the busy list.
                    auto*const pNode = pChunk->ListNode();
                    pAllocInfo->reuseList.Erase(pNode);
                    pAllocInfo->busyList.PushFront(pNode);
                    break;
                }
            }
        }

        if (pChunk == nullptr)
        {
            // All busy chunks were still in-use so we must create a new ChunkAllocation. It is possible for this call
            // to fail in rare circumstances (e.g., out of GPU memory) but we do not expect it to occur.
            result = CreateAllocation(pAllocInfo, false, &pChunk);
        }
    }

    *ppChunk = pChunk;
    return result;
}

// =====================================================================================================================
// Creates a new command stream allocation and returns one of its chunks for immediate use. If the allocation contains
// more than one chunk the rest will be pushed onto the free chunk list.
Result CmdAllocator::CreateAllocation(
    CmdAllocInfo*    pAllocInfo,
    bool             dummyAlloc,
    CmdStreamChunk** ppChunk)
{
    Result result = Result::ErrorOutOfMemory;

    CmdStreamAllocation* pAlloc = nullptr;
    CmdStreamChunk*      pChunk = nullptr;

    uint64 pagingFence = 0;
    CmdStreamAllocationCreateInfo allocCreateInfo = pAllocInfo->allocCreateInfo;
    // dummyAlloc indicates that the new CmdStreamAllocation will get its GPU memory from device and will not own
    // that piece of memory
    allocCreateInfo.flags.dummyAllocation = dummyAlloc;
    // If wait-on-submit residency is enabled we must request a paging fence for each allocation. Otherwise we will
    // implicitly wait for each allocation to be resident at create-time.
    if (allocCreateInfo.flags.optimizePaging == 1)
    {
        allocCreateInfo.memObjInternalInfo.pPagingFence = &pagingFence;
    }

    void*const pPlacementAddr = PAL_MALLOC(CmdStreamAllocation::GetSize(allocCreateInfo),
                                           m_pPlatform,
                                           AllocInternal);

    if (pPlacementAddr != nullptr)
    {
        result = CmdStreamAllocation::Create(allocCreateInfo, m_pDevice, pPlacementAddr, &pAlloc);

        if (result != Result::Success)
        {
            // Free the memory we allocated for the command stream since it failed to initialize.
            PAL_FREE(pPlacementAddr, m_pPlatform);
        }
    }

    if (pAlloc != nullptr)
    {
        PAL_ASSERT(result == Result::Success);
        pAllocInfo->allocList.PushBack(pAlloc->ListNode());

        pChunk = pAlloc->Chunks();
        for (uint32 idx = 1; idx < allocCreateInfo.numChunks; ++idx)
        {
            pAllocInfo->freeList.PushBack(pChunk[idx].ListNode());
        }

        const bool usesSystemMemory = pAlloc->UsesSystemMemory();

        if ((usesSystemMemory == false) && m_pPlatform->IsSubAllocTrackingEnabled())
        {
            ReportSuballocationEvent(Developer::CallbackType::SubAllocGpuMemory, pChunk);
        }

        GpuMemoryResourceBindEventData eventData = {};
        eventData.pObj = this;
        if (usesSystemMemory)
        {
            eventData.isSystemMemory = true;
        }
        else
        {
            eventData.pGpuMemory = pAlloc->GpuMemory();
        }
        eventData.requiredGpuMemSize = pAllocInfo->allocCreateInfo.memObjCreateInfo.size;
        m_pPlatform->GetGpuMemoryEventProvider()->LogGpuMemoryResourceBindEvent(eventData);

        Developer::BindGpuMemoryData callbackData = {};
        callbackData.pObj               = eventData.pObj;
        callbackData.requiredGpuMemSize = eventData.requiredGpuMemSize;
        callbackData.pGpuMemory         = eventData.pGpuMemory;
        callbackData.offset             = eventData.offset;
        callbackData.isSystemMemory     = eventData.isSystemMemory;
        m_pDevice->DeveloperCb(Developer::CallbackType::BindGpuMemory, &callbackData);

        // Move the first newly created chunk to the busy list.
        pAllocInfo->busyList.PushBack(pChunk->ListNode());
    }

    if ((result == Result::Success) &&
        (allocCreateInfo.flags.optimizePaging == 1))
    {
        // Update the last paging fence if the current paging fence from this allocation is larger.
        m_lastPagingFence = Max(m_lastPagingFence, *allocCreateInfo.memObjInternalInfo.pPagingFence);
    }

    *ppChunk = pChunk;
    return result;
}

// =====================================================================================================================
// Creates a new command stream allocation used to handle the dummy chunk. This chunk is used to prevent crashes in
// cases where we run out of GPU memory.
Result CmdAllocator::CreateDummyChunkAllocation()
{
    Result result = Result::ErrorOutOfMemory;

    uint64 pagingFence = 0;
    CmdStreamAllocationCreateInfo createInfo = {};
    createInfo.memObjCreateInfo.priority     = GpuMemPriority::Normal;
    createInfo.memObjCreateInfo.vaRange      = VaRange::Default;
    createInfo.memObjCreateInfo.alignment    = 4096;
    createInfo.memObjCreateInfo.size         = 4096;

    createInfo.memObjInternalInfo.pPagingFence         = &pagingFence;
    createInfo.memObjInternalInfo.flags.isCmdAllocator = 1;

    createInfo.chunkSize             = 4096;
    createInfo.numChunks             = 1;
    createInfo.flags.dummyAllocation = true;

    void*const pPlacementAddr = PAL_MALLOC(CmdStreamAllocation::GetSize(createInfo),
                                           m_pPlatform,
                                           AllocInternal);

    if (pPlacementAddr != nullptr)
    {
        result = CmdStreamAllocation::Create(createInfo, m_pDevice, pPlacementAddr, &m_pDummyChunkAllocation);

        if (result != Result::Success)
        {
            // Free the memory we allocated for the command stream since it failed to initialize.
            PAL_FREE(pPlacementAddr, m_pPlatform);
        }
        else
        {
            // update the last paging fence if the current paging fence from this dummy allocation is larger.
            m_lastPagingFence = Max(m_lastPagingFence, pagingFence);
        }
    }

    return result;
}

// =====================================================================================================================
VirtualLinearAllocator* CmdAllocator::GetNewLinearAllocator()
{
    VirtualLinearAllocatorWithNode* pAllocator = nullptr;

    // If necessary, engage the linear allocator lock.
    if (m_pLinearAllocLock != nullptr)
    {
        m_pLinearAllocLock->Lock();
    }

    if (m_linearAllocFreeList.IsEmpty() == false)
    {
        // Just pop the first free allocator off of the list.
        pAllocator = m_linearAllocFreeList.Back();

        // Move the allocator from the free list to the front of the busy list.
        auto*const pNode = pAllocator->GetNode();
        m_linearAllocFreeList.Erase(pNode);
        m_linearAllocBusyList.PushFront(pNode);
    }
    else
    {
        // Try to create a new linear allocator, we will return null if this fails.
        pAllocator = PAL_NEW(VirtualLinearAllocatorWithNode, m_pPlatform, AllocInternal) (64_KiB);

        if (pAllocator != nullptr)
        {
            const Result result = pAllocator->Init();

            if (result != Result::Success)
            {
                PAL_SAFE_DELETE(pAllocator, m_pPlatform);
            }
            else
            {
                // It worked, put the new allocator on the busy list.
                m_linearAllocBusyList.PushFront(pAllocator->GetNode());
            }
        }
    }

    if (m_pLinearAllocLock != nullptr)
    {
        m_pLinearAllocLock->Unlock();
    }

    return pAllocator;
}

// =====================================================================================================================
void CmdAllocator::ReuseLinearAllocator(
    VirtualLinearAllocator* pReuseAllocator)
{
    if (AutomaticMemoryReuse())
    {
        auto*const pAllocator = static_cast<VirtualLinearAllocatorWithNode*>(pReuseAllocator);
        auto*const pNode      = pAllocator->GetNode();

        // If necessary, engage the linear allocator lock.
        if (m_pLinearAllocLock != nullptr)
        {
            m_pLinearAllocLock->Lock();
        }

        // Remove our allocator from the busy list and add it to the front of the free list.
        m_linearAllocBusyList.Erase(pNode);
        m_linearAllocFreeList.PushFront(pNode);

        if (m_pLinearAllocLock != nullptr)
        {
            m_pLinearAllocLock->Unlock();
        }
    }
}

#if PAL_ENABLE_PRINTS_ASSERTS
// =====================================================================================================================
// Updates the histogram for the given queue type. This can only be called when logCmdBufCommitSizes is true.
void CmdAllocator::LogCommit(
    EngineType engineType,
    bool       isConstantEngine,
    uint32     numDwords)
{
    PAL_ASSERT((engineType != EngineTypeTimer) &&
               ((isConstantEngine == false) || (engineType == EngineTypeUniversal)));
    PAL_ASSERT(m_pDevice->Settings().logCmdBufCommitSizes);

    if (m_pChunkLock != nullptr)
    {
        m_pChunkLock->Lock();
    }

    constexpr uint32 HistogramIndex[] =
    {
        0,        // EngineTypeUniversal
        2,        // EngineTypeCompute
        3,        // EngineTypeDma,
        UINT_MAX, // EngineTypeTimer
    };

    // Put the DE and CE first followed by the other queues.
    const uint32 histIdx = HistogramIndex[engineType] + ((isConstantEngine) ?  1 : 0);
    const uint32 binIdx  = Pow2Align(numDwords, HistogramStep) / HistogramStep;

    m_pHistograms[histIdx][binIdx]++;

    if (m_pChunkLock != nullptr)
    {
        m_pChunkLock->Unlock();
    }
}

// =====================================================================================================================
// Write the commit histograms out to the commit log.
void CmdAllocator::PrintCommitLog() const
{
    File   commitLog;
    Result result = OpenLogFile(&commitLog, "commitLog.csv", FileAccessMode::FileAccessAppend);

    if (result == Result::Success)
    {
        // Write one row with labels for each bin.
        result = commitLog.Printf("Bin Labels");

        for (uint32 binIdx = 0; (binIdx < m_numHistogramBins) && (result == Result::Success); ++binIdx)
        {
            result = commitLog.Printf(",%u", binIdx * HistogramStep);
        }

        if (result == Result::Success)
        {
            result = commitLog.Printf("\n");
        }

        // Now print the histograms out, one per row.
        const char* headers[HistogramCount] =
        {
            "Universal DE",
            "Universal CE",
            "Compute",
            "DMA",
            "VideoEncode",
            "VideoDecode",
        };

        for (uint32 histIdx = 0; (histIdx < HistogramCount) && (result == Result::Success); ++histIdx)
        {
            result = commitLog.Write(headers[histIdx], strlen(headers[histIdx]));

            for (uint32 binIdx = 0; (binIdx < m_numHistogramBins) && (result == Result::Success); ++binIdx)
            {
                result = commitLog.Printf(",%llu", m_pHistograms[histIdx][binIdx]);
            }

            if (result == Result::Success)
            {
                result = commitLog.Printf("\n");
            }
        }
    }

    if (result == Result::Success)
    {
        // Put a divider at the end to make it easier to distinguish multiple data sets.
        result = commitLog.Printf("==================================================\n");
    }

    PAL_ASSERT(result == Result::Success);
}
#endif

// =====================================================================================================================
// Reports allocation and free of suballocations to the DeveloperCb
void CmdAllocator::ReportSuballocationEvent(
    const Developer::CallbackType type,
    CmdStreamChunk* const         pChunk
    ) const
{
    PAL_ASSERT((type == Developer::CallbackType::SubAllocGpuMemory) ||
               (type == Developer::CallbackType::SubFreeGpuMemory));

    const GpuMemory* pGpuMemory = pChunk->Allocation()->GpuMemory();

    Developer::GpuMemoryData data = {};
    data.size                     = pChunk->Size();
    data.heap                     = pGpuMemory->Desc().heaps[0];
    data.flags.isClient           = pGpuMemory->IsClient();
    data.flags.isFlippable        = pGpuMemory->IsFlippable();
    data.flags.isUdmaBuffer       = pGpuMemory->IsUdmaBuffer();
    data.flags.isCmdAllocator     = pGpuMemory->IsCmdAllocator();
    data.flags.isVirtual          = pGpuMemory->IsVirtual();
    data.flags.isExternal         = pGpuMemory->IsExternal();
    data.flags.buddyAllocated     = pGpuMemory->WasBuddyAllocated();
    data.allocMethod              = Developer::GpuMemoryAllocationMethod::Normal;
    data.pGpuMemory               = pGpuMemory;
    data.offset                   = pChunk->GpuMemoryOffset();
    m_pDevice->DeveloperCb(type, &data);
}

} // Pal
