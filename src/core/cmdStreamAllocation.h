/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/fence.h"
#include "core/gpuMemory.h"
#include "core/g_palSettings.h"

#include "palIntrusiveList.h"
#include "palMutex.h"
#include "palVector.h"

namespace Util { class File; }

namespace Pal
{
// Forward declarations
class CmdStreamChunk;
class Device;
class Platform;

// Information required to create a new CmdStreamAllocation.
struct CmdStreamAllocationCreateInfo
{
    GpuMemoryCreateInfo         memObjCreateInfo;    // The public create info for the allocation's GpuMemory object.
    GpuMemoryInternalCreateInfo memObjInternalInfo;  // The private create info for the allocation's GpuMemory object.
    uint32                      chunkSize;           // Ammount of GPU memory each chunk contains (in bytes).
    uint32                      numChunks;           // How many chunks will fit in this allocation.
    struct
    {
        uint32                  enableStagingBuffer :  1;   // True if the allocation should use a staging buffer.
        uint32                  dummyAllocation     :  1;   // True if this is an dummy allocation which means it uses
                                                            // system memory and get dummy GPU memory from Device
        uint32                  cpuAccessible       :  1;   // True if this chunk should be CPU-accessible.  Only valid
                                                            // for "real" GPU memory allocations.
        uint32                  reserved            : 29;
    } flags;
};

// =====================================================================================================================
// A CmdStreamAllocation represents a single GpuMemory allocation that will be owned by a CmdBufferAllocator. It defines
// an array of CmdStreamChunks that divide its GpuMemory allocation into sections which the CmdBufferAllocator will
// manage.
class CmdStreamAllocation
{
    // A useful shorthand for Intrusive list of CmdStreamAllocation(s)
    typedef Util::IntrusiveList<CmdStreamAllocation> AllocList;

public:
    static size_t GetSize(const CmdStreamAllocationCreateInfo& createInfo);

    static Result Create(
        const CmdStreamAllocationCreateInfo& createInfo,
        Device*                              pDevice,
        void*                                pPlacementAddr,
        CmdStreamAllocation**                ppAlloc);

    void Destroy(Device* pDevice);

    AllocList::Node* ListNode() { return &m_parentNode; }
    CmdStreamChunk* Chunks() const { return m_pChunks; }

    // Note: the return type has to be non-const so that we can make GpuMemoryRefs that reference command allocations.
    GpuMemory* GpuMemory() const
    {
        PAL_ASSERT(UsesSystemMemory() == false);
        return m_pGpuMemory;
    }

    uint32 ChunkSize() const { return m_createInfo.chunkSize; }

    bool UsesSystemMemory() const { return (m_createInfo.memObjCreateInfo.heapCount == 0); }
    bool IsDummyAllocation() const { return (m_createInfo.flags.dummyAllocation != 0); }
    bool CpuAccessible() const { return (m_createInfo.flags.cpuAccessible != 0); }

private:
    CmdStreamAllocation(const CmdStreamAllocationCreateInfo& createInfo);
    ~CmdStreamAllocation() {}

    Result Init(Device* pDevice);

    const CmdStreamAllocationCreateInfo m_createInfo; // This allocation was created with this information.

    AllocList::Node      m_parentNode; // This allocation should always be owned by exactly one list using this node.
    CmdStreamChunk*const m_pChunks;    // This allocation has been split into these chunks.
    Pal::GpuMemory*      m_pGpuMemory; // The GPU memory object that backs this allocation.
    uint32*              m_pCpuAddr;   // CPU virtual address of the mapped GPU allocation.
    uint32*              m_pStaging;   // If non-null, commands should be accumulated here until chunks are finalized.

    PAL_DISALLOW_DEFAULT_CTOR(CmdStreamAllocation);
    PAL_DISALLOW_COPY_AND_ASSIGN(CmdStreamAllocation);
};

// =====================================================================================================================
// One CmdStreamChunk is one section of a CmdStreamAllocation. CmdStreamChunks are created by CmdStreamAllocations but
// will be owned by a CmdBufferAllocator or a CmdStream.
//
// A CmdStream may manage a CmdStreamChunk as a set of command blocks which must be contiguous in memory but may be
// executed out of order. If this is the case, the command stream must call EndCommandBlock when it finishes allocating
// command space for a block.
class CmdStreamChunk
{
    typedef Util::IntrusiveList<CmdStreamChunk> ChunkList;

public:
    CmdStreamChunk(const CmdStreamAllocation& allocation, uint32* pCpuAddr, uint32* pWriteAddr, gpusize byteOffset);

    void Destroy() { this->~CmdStreamChunk(); }

    uint32* GetSpace(uint32 sizeInDwords);
    uint32* GetSpace(uint32 sizeInDwords, gpusize* pGpuVirtAddr);
    void    ReclaimSpace(uint32 sizeInDwords);
    uint32  ComputeSpaceSize(uint32 sizeInDwords, uint32 alignmentInDwords = 1) const;
    uint32* ValidateCmdGenerationDataSpace(uint32 sizeInDwords, gpusize* pGpuVirtAddr);

    void EndCommandBlock(uint32 postambleDwords);
    void FinalizeCommands();
    void Reset(bool resetRefCount);

    void InitRootBusyTracker();
    void UpdateRootInfo(CmdStreamChunk* pRootChunk);

    void AddCommandStreamReference();
    void AddNestedCommandStreamReference();
    void RemoveCommandStreamReference();

    void IncrementSubmitCount(uint32 count = 1) { Util::AtomicAdd(&m_busyTracker.submitCount, count); }

    ChunkList::Node* ListNode() { return &m_parentNode; }

    const uint32* PeekNextCommandAddr() const { return &m_pWriteAddr[m_usedDataSizeDwords]; }

    GpuMemory* GpuMemory() const { return m_allocation.GpuMemory(); }
    gpusize GpuMemoryOffset() const { return m_offset; }
    gpusize GpuVirtAddr() const { return m_allocation.GpuMemory()->Desc().gpuVirtAddr + m_offset; }

    // Gets a read-only pointer and a read-write pointer to this chunk's mapped buffer. If staging buffers are enabled
    // these won't point to valid data until Finalize is called.
    const uint32* CpuAddr() const { return m_pCpuAddr; }
    uint32* GetRmwCpuAddr() { return m_pCpuAddr; }

    // Gets a read-only pointer to this chunk's write buffer. This can be different than the CpuAddr of the actual
    // allocation if staging buffers are enabled.
    const uint32* WriteAddr() const { return m_pWriteAddr; }

    // These return the total size of the command chunk memory allocation.
    uint32 Size()       const { return m_allocation.ChunkSize(); }
    uint32 SizeDwords() const { return m_allocation.ChunkSize() / sizeof(uint32); }

    // Returns the total ammount of chunk space that can still be allocated.
    uint32 DwordsRemaining() const { return m_reservedDataOffset - m_usedDataSizeDwords; }

    // Returns the total ammount of command space allocated. It may be illegal to execute this space sequentially.
    uint32 DwordsAllocated() const { return m_usedDataSizeDwords; }

    // Returns the range of command space (from offset zero) that can be directly executed by an external class.
    uint32 CmdDwordsToExecute() const { return m_cmdDwordsToExecute; }
    uint32 CmdDwordsToExecuteNoPostamble() const { return m_cmdDwordsToExecuteNoPostamble; }

    // Returns true if the chunk is idle on the GPU.
    // NOTE: Outside of this class, this can only be called by the thread-safe logic within a command allocator.
    bool IsIdleOnGpu() const;

    // Returns true if the chunk is idle on the GPU and is not referenced by any command streams.
    // NOTE: Outside of this class, this can only be called by the thread-safe logic within a command allocator.
    bool IsIdle() const { return (m_referenceCount == 0) && IsIdleOnGpu(); }

    bool UsesSystemMemory() const { return m_allocation.UsesSystemMemory(); }

    uint32 GetGeneration() const { return m_generation; }

    bool ContainsAddress(const uint32* pAddress) const;

#if PAL_ENABLE_PRINTS_ASSERTS
    Result WriteCommandsToFile(Util::File* pFile, uint32 subEngineId, CmdBufDumpFormat mode) const;
#endif

    // We need these intrusive getters so that we can apply the PM4 optimizer during finalization (and other things).
    // They cannot be called after the chunk is finalized.
    uint32* GetRmwWriteAddr() { return m_pWriteAddr; }
    uint32* GetRmwUsedDwords() { return &m_usedDataSizeDwords; }

    // Returns the GPU virtual address of this chunk's busy tracker (or null if there is no busy tracker).
    gpusize BusyTrackerGpuAddr() const { return m_busyTracker.doneCountGpuAddr; }

private:
    ~CmdStreamChunk() {}

    void ResetBusyTracker();

    const CmdStreamAllocation& m_allocation; // This chunk is a section of this allocation.

    ChunkList::Node m_parentNode;  // This chunk should always be owned by exactly one list using this node.
    uint32*const    m_pCpuAddr;    // CPU virtual address of the allocation.
    uint32*const    m_pWriteAddr;  // All commands and embedded data must be written to this buffer. If this pointer
                                   // isn't equal to m_pCpuAddr then it points to a system memory staging buffer.
    const gpusize   m_offset;      // Byte offset within the parent allocation's GpuMemory where this chunk starts.

    // Counts the number of active references on this chunk: each command stream which references this chunk adds one
    // to this count, as do any command buffers which execute a nested command buffer containing this chunk. All of
    // the non-master chunks in this chunk's command buffer also add one to this count. A chunk is considered "free"
    // from the CPU's perspective if this count is zero.
    volatile uint32  m_referenceCount;

    // Each time a chunk is reset its generation is incremented. The busy tracker looks at its root chunk's generation
    // to determine if it has been reset, implying that the root (and thus the local chunk) was idle on the GPU. This
    // counter doesn't need to be volatile because it is only accessed within the allocator's thread-safe logic.
    uint32 m_generation;

    struct
    {
        // The "root" chunk in any command buffer is the first chunk in that buffer. The root chunk contains the GPU
        // semaphore which is used to evaluate whether or not a chunk is still in-use on the GPU.
        CmdStreamChunk*   pRootChunk;

        // The root chunk's generation at the time this chunk was associated with it. If it doesn't match the root
        // chunk's current generation then it root chunk must have been idle on the GPU and reset.
        uint32            rootGeneration;

        // These two counters together represent a sort of "semaphore" which can be used to track whether or not a
        // chunk is busy from the GPU's perspective. Each time this chunk's command buffer will be executed on the
        // GPU, the submit counter is incremented. Each time the GPU finishes executing the command buffer, it
        // increments the done conter. A chunk is considered "idle" from the GPU's perspective if these counters are
        // equal.
        volatile uint32   submitCount;
        uint32 volatile*  pDoneCount;       // Mapped CPU address of the semaphore's done count.
        gpusize           doneCountGpuAddr; // GPU virtual address of the semaphore's done count.

    } m_busyTracker;

    // This state tracks how much of this chunk has been allocated by a command buffer (for commands or embedded data)
    // and how much of the end of this chunk has been reserved for internal chunk management.
    uint32 m_usedDataSizeDwords; // From the beginning of the chunk, this many DWORDs have been allocated.
    uint32 m_cmdDwordsToExecute; // DWORDs of commands that can be directly executed by an external class.
    uint32 m_cmdDwordsToExecuteNoPostamble; // Excludes the postamble commands which may make this unsafe to execute.
    uint32 m_reservedDataOffset; // Offset in DWORDs to the beginning of any reserved space. It will be equal to the
                                 // size of the chunk if no space has been reserved.

    PAL_DISALLOW_COPY_AND_ASSIGN(CmdStreamChunk);
};

// =====================================================================================================================
// A variation of Vector() that favors Back() being as fast as possible. Used for ChunkRefList which has Back() called
// very often through ReserveCommands() and CommitCommands().
template<typename T, uint32 defaultCapacity, typename Allocator>
class ChunkVector : public Util::Vector<T, defaultCapacity, Allocator>
{
    typedef Util::Vector<T, defaultCapacity, Allocator> Vector;

public:

    ChunkVector(Allocator*const pAllocator) : Vector(pAllocator), m_back(nullptr) {}

    virtual ~ChunkVector() {}

    Result PushBack(const T& data)
    {
        Result result = Vector::PushBack(data);
        SetBack();
        return result;
    }

    void PopBack(T* pData)
    {
        Vector::PopBack(pData);
        SetBack();
    }

    void Clear()
    {
        Vector::Clear();
        SetBack();
    }

    // Note that this has a different return type than Vector::Back() - T vs. T&.
    T Back() const
    {
        PAL_ASSERT(Vector::IsEmpty() == false);
        return m_back;
    }

private:

    void SetBack()
    {
        m_back = (Vector::NumElements() == 0) ? nullptr : Vector::Back();
    }

    T m_back;   // The contents of the last data entry

    PAL_DISALLOW_COPY_AND_ASSIGN(ChunkVector);
};

} // Pal
