/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palIntrusiveListImpl.h"
#include "palMutex.h"
#include "palVectorImpl.h"
#include "eventDefs.h"

#include <limits.h>

using namespace Util;

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

            // It's legal to use the local heap but it might not work as expected. We keep all chunk allocations mapped
            // forever so local allocations might be migrated to system memory by the OS. If local chunks are strongly
            // desired then we should rework the chunk management logic and internal memory logic so that command chunks
            // are not mapped while they are referenced on the GPU.
            PAL_ALERT(allocInfo.allocHeap == GpuHeapLocal);

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
    m_pDummyChunkAllocation(nullptr)
{
#if PAL_ENABLE_PRINTS_ASSERTS
    memset(m_pHistograms, 0, sizeof(m_pHistograms));
    m_numHistogramBins = 0;
#endif

    m_flags.u32All          = 0;
    m_flags.autoMemoryReuse = createInfo.flags.autoMemoryReuse;
    if (createInfo.flags.disableBusyChunkTracking == 0)
    {
        m_flags.trackBusyChunks = m_flags.autoMemoryReuse;
    }

    const uint32 residencyFlags = m_pDevice->GetPublicSettings()->cmdAllocResidency;
    for (uint32 i = 0; i < CmdAllocatorTypeCount; ++i)
    {
        memset(&m_gpuAllocInfo[i].allocCreateInfo, 0, sizeof(m_gpuAllocInfo[i].allocCreateInfo));

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
                m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.heapCount = 2;
                m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.heaps[0]  = GpuHeapInvisible;
                m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.heaps[1]  = GpuHeapLocal;
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
            m_gpuAllocInfo[i].allocCreateInfo.memObjInternalInfo.flags.udmaBuffer = 1;
            // Command chunks are never written from the gpu, except for the busy tracker fence.
            // We can set read-only if the busy-tracker is disabled or forced read-only (moves the tracker to a RW page)
            m_gpuAllocInfo[i].allocCreateInfo.memObjInternalInfo.flags.gpuReadOnly =
                m_pDevice->Settings().cmdStreamReadOnly;
        }
        else if ((i == EmbeddedDataAlloc) || (i == GpuScratchMemAlloc))
        {
            m_gpuAllocInfo[i].allocCreateInfo.memObjCreateInfo.vaRange = VaRange::DescriptorTable;
        }
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
    m_pDevice->GetPlatform()->GetEventProvider()->LogGpuMemoryResourceCreateEvent(data);
}

// =====================================================================================================================
// Destroys all command stream allocations and thus all command stream chunks. All command streams should have returned
// all chunks by now; if not they will suddenly find themselves without any valid chunks.
CmdAllocator::~CmdAllocator()
{
    ResourceDestroyEventData data = {};
    data.pObj = this;
    m_pDevice->GetPlatform()->GetEventProvider()->LogGpuMemoryResourceDestroyEvent(data);

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

    FreeAllChunks();
    FreeAllLinearAllocators();

    // Free the dummy chunk.
    if (m_pDummyChunkAllocation != nullptr)
    {
        m_pDummyChunkAllocation->Destroy(m_pDevice);
        PAL_SAFE_FREE(m_pDummyChunkAllocation, m_pDevice->GetPlatform());
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    if (m_pDevice->Settings().logCmdBufCommitSizes)
    {
        PrintCommitLog();

        // Print and free the histograms.
        for (uint32 histIdx = 0; histIdx < HistogramCount; ++histIdx)
        {
            PAL_SAFE_FREE(m_pHistograms[histIdx], m_pDevice->GetPlatform());
        }
    }
#endif
}

// =====================================================================================================================
// Transfers all chunks from the src list to the dst list after resetting the chunks in the src list.
void CmdAllocator::TransferChunks(
    ChunkList* pFreeList,
    ChunkList* pSrcList)
{
    if (pSrcList->IsEmpty() == false)
    {
        // Reset the allocator by moving all chunks from src list into the dst list. The chunks are reset prior to the
        // move
        for (auto iter = pSrcList->Begin(); iter.IsValid(); iter.Next())
        {
            // The caller must guarantee that all of these chunks have expired so we should never have to check
            // the busy trackers. That being said, we should protect ourselves and validate the chunk busy
            // trackers in builds with asserts enabled.
            PAL_ASSERT((TrackBusyChunks() == false) || iter.Get()->IsIdleOnGpu());

            iter.Get()->Reset(true);
        }

        pFreeList->PushFrontList(pSrcList);
    }
}

// =====================================================================================================================
void CmdAllocator::FreeAllChunks()
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
            PAL_SAFE_FREE(pAlloc, m_pDevice->GetPlatform());
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
        PAL_DELETE(pAllocator, m_pDevice->GetPlatform());
    }

    for (auto iter = m_linearAllocBusyList.Begin(); iter.IsValid();)
    {
        VirtualLinearAllocatorWithNode*const pAllocator = iter.Get();
        m_linearAllocBusyList.Erase(&iter);
        PAL_DELETE(pAllocator, m_pDevice->GetPlatform());
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
        result       = m_pChunkLock->Init();

        if (result == Result::Success)
        {
            m_pLinearAllocLock = PAL_PLACEMENT_NEW(m_pChunkLock + 1) Mutex();
            result             = m_pLinearAllocLock->Init();
        }
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
                                                                     m_pDevice->GetPlatform(),
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
    Platform*const pPlatform = m_pDevice->GetPlatform();
    Destroy();
    PAL_FREE(this, pPlatform);
}

// =====================================================================================================================
// Informs the command allocator that all of its CmdStreamChunks are no longer being referenced by the GPU.
Result CmdAllocator::Reset()
{
    const bool freeOnReset = m_pDevice->Settings().cmdAllocatorFreeOnReset;

    if (m_pChunkLock != nullptr)
    {
        m_pChunkLock->Lock();
    }

    if (freeOnReset)
    {
        // We've been asked to simply destroy all of our allocations on each reset.
        FreeAllChunks();
    }
    else
    {
        for (uint32 i = 0; i < CmdAllocatorTypeCount; ++i)
        {
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

    if (freeOnReset)
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

        // If the root chunk is idle, we can reset and push all the chunks to the free list.
        if (iter.Get()->IsIdle())
        {
            while (iter.IsValid())
            {
                // Move this chunk from the busy list to the front of the free list.
                auto*const pNode = iter.Get()->ListNode();
                pAllocInfo->busyList.Erase(pNode);
                pAllocInfo->freeList.PushFront(pNode);

                // Remember that items on the free list must be reset.
                iter.Get()->Reset(true);
                iter.Next();
            }
        }
        else
        {
            while (iter.IsValid())
            {
                // Move this chunk from the busy list to the front of the reuse list.
                auto*const pNode = iter.Get()->ListNode();
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

    Result result = FindFreeChunk(systemMemory ? &m_sysAllocInfo : &m_gpuAllocInfo[allocType], ppChunk);
    if (result == Result::Success)
    {
        (*ppChunk)->AddCommandStreamReference();
    }

    if (m_pChunkLock != nullptr)
    {
        m_pChunkLock->Unlock();
    }

    return result;
}

// =====================================================================================================================
// Searches the free and busy lists for a free chunk. A new CmdStreamAllocation will be created if needed.
Result CmdAllocator::FindFreeChunk(
    CmdAllocInfo*    pAllocInfo,
    CmdStreamChunk** ppChunk)
{
    Result result = Result::Success;
    CmdStreamChunk* pChunk = nullptr;

    // Search the free-list first.
    if (pAllocInfo->freeList.IsEmpty() == false)
    {
        // Pop a chunk off of the free list because free chunks, by definition, are no longer in use by the CPU or GPU.
        // Checking for IsIdle with automatic memory reuse disabled is undefined. The best we can do is check if it is
        // idle on the GPU.
        pChunk = pAllocInfo->freeList.Back();
        PAL_ASSERT((AutomaticMemoryReuse() && pChunk->IsIdle()) || pChunk->IsIdleOnGpu());

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
                if (reuseIter.Get()->IsIdle())
                {
                    pChunk = reuseIter.Get();
                    pChunk->Reset(true);

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
                                           m_pDevice->GetPlatform(),
                                           AllocInternal);

    Util::MutexAuto allocatorLock(m_pDevice->MemMgr()->GetAllocatorLock());

    if (pPlacementAddr != nullptr)
    {
        result = CmdStreamAllocation::Create(allocCreateInfo, m_pDevice, pPlacementAddr, &pAlloc);

        if (result != Result::Success)
        {
            // Free the memory we allocated for the command stream since it failed to initialize.
            PAL_FREE(pPlacementAddr, m_pDevice->GetPlatform());
        }
        else if (pAlloc != nullptr)
        {
            GpuMemoryResourceBindEventData eventData = {};
            eventData.pObj = this;
            if (pAlloc->UsesSystemMemory())
            {
                eventData.isSystemMemory = true;
            }
            else
            {
                eventData.pGpuMemory = pAlloc->GpuMemory();
            }
            eventData.requiredGpuMemSize = pAllocInfo->allocCreateInfo.memObjCreateInfo.size;
            m_pDevice->GetPlatform()->GetEventProvider()->LogGpuMemoryResourceBindEvent(eventData);
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
                                           m_pDevice->GetPlatform(),
                                           AllocInternal);

    if (pPlacementAddr != nullptr)
    {
        Util::MutexAuto allocatorLock(m_pDevice->MemMgr()->GetAllocatorLock());

        result = CmdStreamAllocation::Create(createInfo, m_pDevice, pPlacementAddr, &m_pDummyChunkAllocation);

        if (result != Result::Success)
        {
            // Free the memory we allocated for the command stream since it failed to initialize.
            PAL_FREE(pPlacementAddr, m_pDevice->GetPlatform());
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
        constexpr uint32 MaxAllocSize = 64 * 1024;
        pAllocator = PAL_NEW(VirtualLinearAllocatorWithNode, m_pDevice->GetPlatform(), AllocInternal) (MaxAllocSize);

        if (pAllocator != nullptr)
        {
            const Result result = pAllocator->Init();

            if (result != Result::Success)
            {
                PAL_SAFE_DELETE(pAllocator, m_pDevice->GetPlatform());
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

} // Pal
