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

#pragma once

#include "core/cmdStreamAllocation.h"
#include "palCmdAllocator.h"
#include "palIntrusiveList.h"
#include "palLinearAllocator.h"
#include "palVector.h"

namespace Util { class Mutex; }

namespace Pal
{

class Device;
class Platform;

// =====================================================================================================================
// The CmdAllocator class is responsible for allocating CmdStreamAllocations and managing their CmdStreamChunks.
class CmdAllocator : public ICmdAllocator
{
    typedef Util::IntrusiveList<CmdStreamAllocation>                  AllocList;
    typedef Util::IntrusiveList<CmdStreamChunk>                       ChunkList;
    typedef Util::IntrusiveList<Util::VirtualLinearAllocatorWithNode> LinearAllocList;
    typedef Util::VectorIterator<CmdStreamChunk*, 16, Platform>       VectorIter;

public:
    static size_t GetSize(const CmdAllocatorCreateInfo& createInfo, Result* pResult);

    CmdAllocator(Device* pDevice, const CmdAllocatorCreateInfo& createInfo);
    virtual ~CmdAllocator();

    virtual Result Init(const CmdAllocatorCreateInfo& createInfo, void* pPlacementAddr);

    virtual void Destroy() override { this->~CmdAllocator(); }
    void DestroyInternal();

    virtual Result Reset(bool freeMemory) override;
    virtual Result Trim(uint32 allocTypeMask, uint32 dynamicThreshold) override;

    // CmdBuffers and CmdStreams will use these public functions to interact with the CmdAllocator.
    Result GetNewChunk(CmdAllocType allocType, bool systemMemory, CmdStreamChunk** ppChunk);

    // Returns the dummy chunk.
    // The dummy chunk allocation always has exactly one chunk so we just return the Chunks() pointer here.
    CmdStreamChunk* GetDummyChunk() const { return m_pDummyChunkAllocation->Chunks(); }

    // CmdStreamChunk(s) are returned back to the allocator for use with the reuse-list.
    void ReuseChunks(CmdAllocType allocType, bool systemMemory, VectorIter iter);

    // CmdBuffers will call this to get an internal linear allocator at Begin time. Null will be returned if a new
    // linear allocator could not be created.
    Util::VirtualLinearAllocator* GetNewLinearAllocator();

    // Once a CmdBuffer that called GetNewLinearAllocator is done with its allocator, it must call this to return the
    // internal linear allocator. Depending on how the CmdBuffer is being used this might be done at End or Reset time
    // but may be as late as Destroy time.
    void ReuseLinearAllocator(Util::VirtualLinearAllocator* pReuseAllocator);

    // The size of chunk it returns is in byte unit.
    uint32 ChunkSize(CmdAllocType allocType) const { return m_gpuAllocInfo[allocType].allocCreateInfo.chunkSize; }

#if PAL_ENABLE_PRINTS_ASSERTS
    void LogCommit(EngineType engineType, bool isConstantEngine, uint32 numDwords);
#endif

    bool AutomaticMemoryReuse() const { return (m_flags.autoMemoryReuse != 0); }
    bool TrackBusyChunks() const      { return (m_flags.trackBusyChunks != 0); }
    bool LocalCommandData() const     { return (m_flags.localCmdData    != 0); }
    bool AutoTrimMemory() const       { return (m_flags.autoTrimMemory  != 0); }

    uint64 LastPagingFence() const { return m_lastPagingFence; }

    virtual Result QueryUtilizationInfo(
        CmdAllocType type, CmdAllocatorUtilizationInfo* pUtilizationInfo) const override;

    static size_t GetPlacementSize(const CmdAllocatorCreateInfo& createInfo);

protected:
    Device*const    m_pDevice;

private:
    // Helper structure for managing a particular type of command allocator memory.
    struct CmdAllocInfo
    {
        AllocList allocList; // Unordered list of allocations owned by the allocator.
        ChunkList freeList;  // Unordered list of chunks that are not in use (busy-tracker indicates idle).
        ChunkList busyList;  // Unordered list of chunks that might be waiting for their busy-tracker to indicate
                             // that the GPU has finished processing them.
        ChunkList reuseList; // Unordered list of chunks that have been 'returned' to the allocator for reuse.

        // All allocations for each alloc type are identical, so we can build the create info up-front.
        CmdStreamAllocationCreateInfo allocCreateInfo;

        uint32 allocFreeThreshold; // Minimum number of free allocations to keep around.
    };

    // These internal functions are used to manage all types of chunks.
    Result FindFreeChunk(const bool systemMemory, CmdAllocInfo* pAllocInfo, CmdStreamChunk** ppChunk);
    Result CreateAllocation(CmdAllocInfo* pAllocInfo, bool dummyAlloc, CmdStreamChunk** ppChunk);
    Result CreateDummyChunkAllocation();

    void TransferChunks(ChunkList* pFreeList, ChunkList* pSrcList);
    void FreeAllChunks(const bool trackSuballocations);
    void FreeAllLinearAllocators();

    // Free allocations where all chunks are idle. Keep at least allocFreeThreshold allocations.
    Result TrimMemory(CmdAllocInfo* const pAllocInfo, uint32 allocFreeThreshold);

    void ReportSuballocationEvent(const Developer::CallbackType type, CmdStreamChunk* const pChunk) const;

#if PAL_ENABLE_PRINTS_ASSERTS
    void PrintCommitLog() const;
#endif

    union
    {
        struct
        {
            uint32 autoMemoryReuse :  1; // Indicates that the allocator will automatically recycle idle chunks.
            uint32 trackBusyChunks :  1; // Indicates that the allocator will track which chunks are idle (for debugging
                                         // purposes, or for supporting 'autoMemoryReuse').
            uint32 localCmdData    :  1; // If CommandDataAlloc memory is allocated from the CPU-visible local heap.
            uint32 autoTrimMemory  :  1; // Indicates that the allocator will automatically trim down the allocations
                                         // where all chunks are idle. A minimum of allocFreeThreshold is kept.
            uint32 reserved        : 28;
        };
        uint32 u32All;
    }  m_flags;

    Util::Mutex*    m_pChunkLock;          // If non-null, this protects the allocator's command-chunk state.
    CmdAllocInfo    m_gpuAllocInfo[CmdAllocatorTypeCount];
    CmdAllocInfo    m_sysAllocInfo;

    // Most-recent paging fence value returned from the OS when allocating command-chunk allocations
    uint64          m_lastPagingFence;

    Util::Mutex*    m_pLinearAllocLock;    // If non-null, this protects the allocator's linear allocator state.
    LinearAllocList m_linearAllocFreeList; // Unordered list of allocators that are reset and not in use.
    LinearAllocList m_linearAllocBusyList; // Unordered list of allocators that are being used by command buffers.

#if PAL_ENABLE_PRINTS_ASSERTS
    // To help us make informed decisions about command stream use, the allocator can build histograms of commit sizes
    // and log them to a csv file on destruction. If we exclude the timer queue (no packets) and include the Constant
    // Engine we need exactly 8 histograms.
    static const uint32 HistogramCount = 6;
    static const uint32 HistogramStep  = 8;              // The bins are split by multiples of this power of two.

    // Each histogram is stored as an array of counters, one for each bin. The first bin counts commits with a size of
    // zero. The subsequent bins are increasing multiples of HistogramStep up to the reserve limit. All values should
    // be rounded up when selecting a bin as this will guarantee that the "zero" bin only holds commits of size zero.
    uint64* m_pHistograms[HistogramCount];
    uint32  m_numHistogramBins;
#endif

    // Dummy chunk used to handle cases where we've run out of GPU memory.
    CmdStreamAllocation* m_pDummyChunkAllocation;

    Platform*const       m_pPlatform;

    PAL_DISALLOW_DEFAULT_CTOR(CmdAllocator);
    PAL_DISALLOW_COPY_AND_ASSIGN(CmdAllocator);
};

} // Pal
