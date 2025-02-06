/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/cmdStreamAllocation.h"
#include "core/device.h"
#include "core/internalMemMgr.h"
#include "core/platform.h"
#include "core/cmdBuffer.h"
#include "palFile.h"
#include "palIntrusiveListImpl.h"
#include "palSysMemory.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
// We need enough space for our class and its array of chunks.
size_t CmdStreamAllocation::GetSize(
    const CmdStreamAllocationCreateInfo& createInfo)
{
    return sizeof(CmdStreamAllocation) + sizeof(CmdStreamChunk) * createInfo.numChunks;
}

// =====================================================================================================================
// Constructs a new CmdStreamAllocation object in preallocated memory and initializes it.
Result CmdStreamAllocation::Create(
    const CmdStreamAllocationCreateInfo& createInfo,
    Device*                              pDevice,
    void*                                pPlacementAddr,
    CmdStreamAllocation**                ppAlloc)
{
    auto*  pAlloc = PAL_PLACEMENT_NEW(pPlacementAddr) CmdStreamAllocation(createInfo);
    Result result = pAlloc->Init(pDevice);

    if (result != Result::Success)
    {
        pAlloc->Destroy(pDevice);
    }
    else
    {
        *ppAlloc = pAlloc;
    }

    return result;
}

// =====================================================================================================================
// The array of chunks is placed directly after this object in memory; those chunks will not be constructed until Init.
CmdStreamAllocation::CmdStreamAllocation(
    const CmdStreamAllocationCreateInfo& createInfo)
    :
    m_createInfo(createInfo),
    m_parentNode(this),
    m_pChunks(reinterpret_cast<CmdStreamChunk*>(this + 1)),
    m_pGpuMemory(nullptr),
    m_pCpuAddr(nullptr),
    m_pStaging(nullptr)
{
}

// =====================================================================================================================
// Allocates some GPU memory, maps it, and constructs a series of CmdStreamChunks to split up that GPU memory.
Result CmdStreamAllocation::Init(
    Device* pDevice)
{
    Result result = Result::ErrorOutOfMemory;

    if (IsDummyAllocation())
    {
        // Dummy allocations should always be created with system memory.
        PAL_ASSERT(UsesSystemMemory());

        // Dummy allocations all use the same memory which is preallocated by the device.
        m_pGpuMemory = pDevice->GetDummyChunkMem().Memory();
        PAL_ASSERT(m_pGpuMemory != nullptr);

        // m_createInfo should be modified to DummyChunk's actually used heap.
        // Otherwise, UsesSystemMemory() would no longer return correct information.
        CmdStreamAllocationCreateInfo* pDummyAllocationCreateInfo =
            const_cast<CmdStreamAllocationCreateInfo*>(&m_createInfo);

        pDummyAllocationCreateInfo->memObjCreateInfo.heapCount = 1;
        pDummyAllocationCreateInfo->memObjCreateInfo.heaps[0]  = m_pGpuMemory->PreferredHeap();

        result = m_pGpuMemory->Map(reinterpret_cast<void**>(&m_pCpuAddr));
    }
    else if (UsesSystemMemory())
    {
        PAL_ASSERT(IsPow2Aligned(ChunkSize(), VirtualPageSize()));

        result = VirtualReserve(static_cast<size_t>(m_createInfo.memObjCreateInfo.size),
                                reinterpret_cast<void**>(&m_pCpuAddr));
        if (result == Result::Success)
        {
            result = VirtualCommit(m_pCpuAddr, static_cast<size_t>(m_createInfo.memObjCreateInfo.size));
        }
    }
    else
    {
        result = pDevice->MemMgr()->AllocateGpuMem(m_createInfo.memObjCreateInfo,
                                                   m_createInfo.memObjInternalInfo,
                                                   false,
                                                   &m_pGpuMemory,
                                                   nullptr);

        if ((result == Result::Success) && CpuAccessible())
        {
            result = m_pGpuMemory->Map(reinterpret_cast<void**>(&m_pCpuAddr));
        }

        if ((result == Result::Success) && m_createInfo.flags.enableStagingBuffer)
        {
            // For simplicitly this (usually disabled) large buffer is not put in the placement buffer.
            m_pStaging = static_cast<uint32*>(PAL_MALLOC(static_cast<size_t>(m_createInfo.memObjCreateInfo.size),
                                                         pDevice->GetPlatform(),
                                                         AllocInternal));

            if (m_pStaging == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
        }
    }

    // We construct the chunks even if we've encountered an error because Destroy() requires us to have done so.
    uint32* pChunkCpuAddr   = m_pCpuAddr;
    uint32* pChunkWriteAddr = (m_pStaging != nullptr) ? m_pStaging : m_pCpuAddr;
    size_t  byteOffset      = 0;

    for (uint32 idx = 0; idx < m_createInfo.numChunks; ++idx)
    {
        PAL_PLACEMENT_NEW(m_pChunks + idx) CmdStreamChunk(this, pChunkCpuAddr, pChunkWriteAddr, byteOffset);

        if (CpuAccessible())
        {
            pChunkCpuAddr   += m_createInfo.chunkSize / sizeof(uint32);
            pChunkWriteAddr += m_createInfo.chunkSize / sizeof(uint32);
        }
        byteOffset += m_createInfo.chunkSize;
    }

    return result;
}

// =====================================================================================================================
// Destroys all CmdStreamChunks within this CmdStreamAllocation and frees the backing GpuMemory object.
void CmdStreamAllocation::Destroy(
    Device* pDevice)
{
    for (uint32 idx = 0; idx < m_createInfo.numChunks; ++idx)
    {
        m_pChunks[idx].Destroy();
    }

    if (m_pGpuMemory != nullptr)
    {
        if (m_pCpuAddr != nullptr)
        {
            Result result = m_pGpuMemory->Unmap();
            PAL_ASSERT(result == Result::Success);

            m_pCpuAddr = nullptr;
        }

        // Only free the GPU memory for real allocations. The device manages the gpu memory for dummy allocations.
        if (IsDummyAllocation() == false)
        {
            pDevice->MemMgr()->FreeGpuMem(m_pGpuMemory, 0);
        }

        m_pGpuMemory = nullptr;
    }
    else if (m_pCpuAddr != nullptr)
    {
        PAL_ASSERT(UsesSystemMemory());

        // Note: VirtualRelease() will decommit and free the memory simultaneously.
        Result result = VirtualRelease(m_pCpuAddr, static_cast<size_t>(m_createInfo.memObjCreateInfo.size));
        PAL_ASSERT(result == Result::Success);

        m_pCpuAddr = nullptr;
    }

    PAL_SAFE_FREE(m_pStaging, pDevice->GetPlatform());
}

// =====================================================================================================================
CmdStreamChunk::CmdStreamChunk(
    CmdStreamAllocation* pAllocation,
    uint32*              pCpuAddr,
    uint32*              pWriteAddr,
    gpusize              byteOffset)
    :
    m_pAllocation(pAllocation),
    m_parentNode(this),
    m_pCpuAddr(pCpuAddr),
    m_pWriteAddr(pWriteAddr),
    m_offset(byteOffset),
    m_generation(0),
    m_usedDataSizeDwords(0),
    m_cmdDwordsToExecute(0),
    m_cmdDwordsToExecuteNoPostamble(0),
    m_reservedDataOffset(SizeDwords())
{
    PAL_ASSERT(m_pAllocation != nullptr);

    ResetBusyTracker();
}

// =====================================================================================================================
// Marks the next "sizeInDwords" DWORDs and return a pointer to them.
// The caller must make sure that this chunk has enough space.
uint32* CmdStreamChunk::GetSpace(
    uint32 sizeInDwords)
{
    uint32*const pSpace = (m_pWriteAddr + m_usedDataSizeDwords);

    m_usedDataSizeDwords += sizeInDwords;

    return pSpace;
}

// =====================================================================================================================
// Marks the next "sizeInDwords" DWORDs and return a pointer to them.
// The caller must make sure that this chunk has enough space.
uint32* CmdStreamChunk::GetSpace(
    uint32   sizeInDwords,
    gpusize* pGpuVirtAddr)  // [out] GPU virtual address of the allocated space. Must not be null!
{
    // It is impossible to retrieve the GPU virtual address of the allocated space when the chunk is located in system
    // memory!
    PAL_ASSERT(m_pAllocation->UsesSystemMemory() == false);

    uint32*const pSpace = (m_pWriteAddr + m_usedDataSizeDwords);

    PAL_ASSERT(pGpuVirtAddr != nullptr);
    (*pGpuVirtAddr) = GpuVirtAddr() + (m_usedDataSizeDwords * sizeof(uint32));

    m_usedDataSizeDwords += sizeInDwords;

    return pSpace;
}

// =====================================================================================================================
// Marks the next "sizeInDwords" DWORDs and return a pointer to them.
// The caller must make sure that this chunk has enough space.
uint32* CmdStreamChunk::GetSpace(
    uint32           sizeInDwords,
    Pal::GpuMemory** ppGpuMem,     // [out] Pointer to the GPU memory object of the allocated space. Must not be null!
    gpusize*         pOffset)      // [out] GPU virtual address offset of the allocated space. Must not be null!
{
    // It is impossible to retrieve the GPU virtual address of the allocated space when the chunk is located in system
    // memory!
    PAL_DEBUG_BUILD_ONLY_ASSERT(m_pAllocation->UsesSystemMemory() == false);

    uint32*const pSpace = (m_pWriteAddr + m_usedDataSizeDwords);

    PAL_DEBUG_BUILD_ONLY_ASSERT((ppGpuMem != nullptr) && (pOffset != nullptr));
    (*ppGpuMem) = GpuMemory();
    (*pOffset)  = GpuMemoryOffset() + (m_usedDataSizeDwords * sizeof(uint32));

    m_usedDataSizeDwords += sizeInDwords;

    return pSpace;
}

// =====================================================================================================================
// The caller asked for too much command space and wishes to return the most recent "sizeInDwords" DWORDs of command
// space back to the chunk. In other words, we must rewind our command space pointer by sizeInDwords.
void CmdStreamChunk::ReclaimSpace(
    uint32 sizeInDwords)
{
    m_usedDataSizeDwords -= sizeInDwords;
}

// =====================================================================================================================
// If data alignment greater than a DWORD is required, the exact size the caller must request depends on the state of
// the chunk. This method computes that size based on the given ideal size and alignment.
uint32 CmdStreamChunk::ComputeSpaceSize(
    uint32 sizeInDwords,
    uint32 alignmentInDwords
    ) const
{
    // Determine how far we would have to move the data space offset to get the required alignment.
    const uint32 newSpaceUsed = Pow2Align(m_usedDataSizeDwords, alignmentInDwords) + sizeInDwords;

    return newSpaceUsed - m_usedDataSizeDwords;
}

// =====================================================================================================================
// Command generation puts command data and embedded data in the same chunk for simplicity of the shader. This function
// validates that the space needed for the extra data is within the limits of this chunk, and returns the CPU and GPU
// pointer for writing the data.
// NOTE: This function does not add to the number of DWORDs used, as the extra data is not considered command data.
uint32* CmdStreamChunk::ValidateCmdGenerationDataSpace(
    uint32   sizeInDwords,
    gpusize* pGpuVirtAddr)
{
    PAL_ASSERT(m_pAllocation->UsesSystemMemory() == false);

    PAL_ASSERT(sizeInDwords <= DwordsRemaining());
    (*pGpuVirtAddr) = GpuVirtAddr() + (m_usedDataSizeDwords * sizeof(uint32));
    return m_pWriteAddr + m_usedDataSizeDwords;
}

// =====================================================================================================================
// Signals that the command stream has finished a command block. This only needs to be called by command streams that
// are subdividing command chunks into command blocks.
void CmdStreamChunk::EndCommandBlock(
    uint32 postambleDwords)
{
    // Set the execution size to the used command space size when the command stream ends its first block. This makes it
    // possible for us to launch the beginning of this chunk without tracking any information about subsequent blocks.
    if (m_cmdDwordsToExecute == 0)
    {
        m_cmdDwordsToExecute            = m_usedDataSizeDwords;
        m_cmdDwordsToExecuteNoPostamble = m_usedDataSizeDwords - postambleDwords;
    }
}

// =====================================================================================================================
// Signals that the command stream is done building this chunk and its data can be made ready for submission.
void CmdStreamChunk::FinalizeCommands()
{
    // Sanity check that we didn't blow past the end of our chunk or reserved space.
    PAL_ASSERT(m_usedDataSizeDwords <= m_reservedDataOffset);

    // If our command stream isn't employing command blocks this will still be zero. We should set it to the entire
    // used command space size so that we have a valid block size in case some class tries to execute this chunk.
    if (m_cmdDwordsToExecute == 0)
    {
        m_cmdDwordsToExecute            = m_usedDataSizeDwords;
        m_cmdDwordsToExecuteNoPostamble = m_usedDataSizeDwords;
    }

    if (m_pWriteAddr != m_pCpuAddr)
    {
        // If the data wasn't directly written to the mapped CPU pointer we need to copy them now.
        memcpy(m_pCpuAddr, m_pWriteAddr, m_usedDataSizeDwords * sizeof(uint32));

        const uint32 reservedSize = Size() - m_reservedDataOffset * sizeof(uint32);

        if (reservedSize > 0)
        {
            memcpy(m_pCpuAddr + m_reservedDataOffset, m_pWriteAddr + m_reservedDataOffset, reservedSize);
        }
    }
}

// =====================================================================================================================
// Reset the chunk so that we can use it again as part of a new command stream. This increments the chunk's generation
// and resets its busy tracker.
void CmdStreamChunk::Reset()
{
    m_usedDataSizeDwords = 0;
    m_cmdDwordsToExecute = 0;
    m_cmdDwordsToExecuteNoPostamble = 0;
    m_reservedDataOffset = SizeDwords();

    m_generation++;

    ResetBusyTracker();
}

// =====================================================================================================================
// Initialize the busy tracker to represent a state indicating that:
// <> This chunk is the "root" of whatever command stream owns it;
// <> This chunk has not allocated a busy tracker.
void CmdStreamChunk::ResetBusyTracker()
{
    m_busyTracker.doneCountGpuAddr = 0;
    m_busyTracker.pDoneCount       = &m_busyTracker.submitCount;
    m_busyTracker.submitCount      = 0;
    m_busyTracker.rootGeneration   = m_generation;
    m_busyTracker.pRootChunk       = this;
}

// =====================================================================================================================
// Initializes the busy tracker attributes. This should only be called on root chunks.
Result CmdStreamChunk::InitRootBusyTracker(
    CmdAllocator* pAllocator
)
{
    Result result = Result::Success;
    if (m_pAllocation->UsesSystemMemory() == false)
    {
        constexpr uint32 TrackerAlignDwords = 2;
        constexpr uint32 TrackerAlignBytes  = TrackerAlignDwords * sizeof(uint32);
        uint32* pWriteAddr = nullptr;
        if (m_pAllocation->GpuMemory()->GetDevice()->Settings().cmdStreamReadOnly == false)
        {
            // This chunk will become the root chunk for a CmdStream. Allocate a busy tracker for this and future chunks
            // to share. Note that we allocate a 64-bit tracker but access it as a 32-bit counter because some engines
            // only support 32-bit counters while others only support 64-bit counters. This means we assume a 32-bit
            // counter will never wrap so that the high 32-bits can be ignored.
            m_reservedDataOffset = Pow2AlignDown((m_reservedDataOffset - TrackerAlignDwords), TrackerAlignDwords);

            // Store the final GPU and CPU addresses for the busy tracker.
            m_busyTracker.doneCountGpuAddr = (GpuVirtAddr() + m_reservedDataOffset * sizeof(uint32));
            m_busyTracker.pDoneCount = (m_pCpuAddr + m_reservedDataOffset);
            pWriteAddr = m_pWriteAddr + m_reservedDataOffset;
        }
        else
        {
            // The root chunk is read-only, so we need a separate RW allocation to track the status of this one.
            CmdStreamChunk* pTrackerChunk = nullptr;
            result = pAllocator->GetNewChunk(EmbeddedDataAlloc, false, &pTrackerChunk);
            if (result != Result::Success)
            {
                PAL_ASSERT_ALWAYS();
                // Use a dummy chunk to still mostly work in an OOM situation.
                pTrackerChunk = pAllocator->GetDummyChunk();
            }
            PAL_ASSERT(pTrackerChunk != nullptr);
            PAL_ASSERT(IsPow2Aligned(pTrackerChunk->GpuVirtAddr(), TrackerAlignBytes));

            m_busyTracker.doneCountGpuAddr = pTrackerChunk->GpuVirtAddr();
            m_busyTracker.pDoneCount       = pTrackerChunk->GetRmwCpuAddr();
            pWriteAddr                     = pTrackerChunk->GetRmwWriteAddr();
        }

        // Initialize both CPU addresses (mapped CPU address and the staging buffer). The value in the staging
        // buffer will be copied into the mapped buffer on Finalize but it's a good idea to initialize both so that
        // we can call IsIdleOnGpu before calling Finalize without invoking undefined behavior.
        *m_busyTracker.pDoneCount = 0;
        *pWriteAddr = 0;
    }
    return result;
}

// =====================================================================================================================
// Update the busy tracker's root pointer and root generation.
void CmdStreamChunk::UpdateRootInfo(
    CmdStreamChunk* pRootChunk)
{
    PAL_ASSERT(pRootChunk != nullptr);

    m_busyTracker.pRootChunk     = pRootChunk;
    m_busyTracker.rootGeneration = pRootChunk->m_generation;
}

// =====================================================================================================================
// Returns true if the chunk is idle from the GPU's perspective. If busy tracking is not used by this chunk, this
// function will always return true (because we'd be relying on the client to be responsible for not reusing chunks
// before they are really idle).
bool CmdStreamChunk::IsIdleOnGpu() const
{
    PAL_ASSERT(m_busyTracker.pRootChunk != nullptr);
    const auto& root = *m_busyTracker.pRootChunk;

    // This chunk is idle if its root (which may be itself) meets any of the following conditions.
    // - It has moved on to a new generation, indicating it was idle and was reset.
    // - Its submit count matches the GPU's done count (no pending or active submissions).
    return (root.m_generation != m_busyTracker.rootGeneration) ||
           (root.m_busyTracker.submitCount == (*root.m_busyTracker.pDoneCount));
}

// =====================================================================================================================
// Returns true if the given CPU address is within the given chunk.
bool CmdStreamChunk::ContainsAddress(
    const uint32* pAddress
    ) const
{
    const uint32*const pBaseAddr = m_pWriteAddr;
    const uint32*const pEndAddr  = pBaseAddr + SizeDwords();

    return ((pAddress >= pBaseAddr) && (pAddress < pEndAddr));
}

// =====================================================================================================================
// Writes the commands in this chunk to the given file. Each DWORD is expressed in hex and printed on its own line.
Result CmdStreamChunk::WriteCommandsToFile(
    File*          pFile,
    uint32         subEngineId,
    CmdBufDumpFormat mode
    ) const
{
    Result result = Result::Success;

    if ((mode == CmdBufDumpFormat::CmdBufDumpFormatBinary) ||
        (mode == CmdBufDumpFormat::CmdBufDumpFormatBinaryHeaders))
    {
        if (mode == CmdBufDumpFormat::CmdBufDumpFormatBinaryHeaders)
        {
            const CmdBufferDumpHeader chunkheader =
            {
                static_cast<uint32>(sizeof(CmdBufferDumpHeader)),
                m_usedDataSizeDwords * static_cast<uint32>(sizeof(uint32)),
                subEngineId
            };
            pFile->Write(&chunkheader, sizeof(chunkheader));
        }
        pFile->Write(m_pWriteAddr, m_usedDataSizeDwords * sizeof(uint32));
    }
    else
    {
        constexpr uint32 MaxLineSize = 16;
        char line[MaxLineSize];

        PAL_ASSERT(mode == CmdBufDumpFormat::CmdBufDumpFormatText);

        for (uint32 idx = 0; (idx < m_usedDataSizeDwords) && (result == Result::Success); ++idx)
        {
            Snprintf(line, MaxLineSize, "0x%08x\n", m_pWriteAddr[idx]);
            result = pFile->Write(line, strlen(line));
        }
    }

    return result;
}

} // Pal
