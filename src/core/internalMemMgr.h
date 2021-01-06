/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/gpuMemory.h"
#include "palBuddyAllocator.h"
#include "palMutex.h"

namespace Pal
{

class IGpuMemoryBindable;
class Platform;

// Contains the information describing a piece of GPU memory managed by the internal memory manager.
struct GpuMemoryInfo
{
    GpuMemory*      pGpuMemory;
    bool            readOnly;
};

// Contains the information describing a GPU memory chunk pool
struct GpuMemoryPool
{
    GpuMemory*                      pGpuMemory;             // GPU memory object that the allocator suballocates from

    bool                            readOnly;               // Tells whether the allocation is read-only
    GpuMemoryFlags                  memFlags;               // Properties of the GPU memory object
    size_t                          heapCount;              // Number of heaps in the heap preference array
    GpuHeap                         heaps[GpuHeapCount];    // Heap preference array
    VaRange                         vaRange;                // Virtual address range
    MType                           mtype;                  // The mtype of the GPU memory object.
    uint64                          pagingFenceVal;         // Paging fence value

    Util::BuddyAllocator<Platform>* pBuddyAllocator;        // Buddy allocator used for the suballocation
};

// =====================================================================================================================
// InternalMemMgr is responsible for managing internal memory allocations (either PAL-internal or
// client-driver-internal) and tracks the list of the memory objects which need to be referenced by each command buffer
// submitted. Additionally, it also handles sub-allocating from large allocations to provide tiny allocations when
// possible.
//
// The AllocateGpuMem function skips the suballocation scheme if the caller passes a null pOffset.  It is expected that
// this behavior will only be used in special circumstances (e.g., UDMA buffers); generic GPU memory allocations should
// provide a non-null pOffset to leverage the suballocation scheme.
//
// Note that AllocateGpuMem's internalInfo must have the alwaysResident flag set because all memory managed by the
// InternalMemMgr must be always resident.
class InternalMemMgr
{
public:
    typedef Util::List<GpuMemoryInfo, Platform>         GpuMemoryList;
    typedef Util::ListIterator<GpuMemoryInfo, Platform> GpuMemoryListIterator;

    typedef Util::List<GpuMemoryPool, Platform>         GpuMemoryPoolList;

    explicit InternalMemMgr(Device* pDevice);
    ~InternalMemMgr() { FreeAllocations(); }

    void FreeAllocations();

    Result AllocateGpuMem(
        const GpuMemoryCreateInfo&          createInfo,
        const GpuMemoryInternalCreateInfo&  internalInfo,
        bool                                readOnly,
        GpuMemory**                         ppGpuMemory,
        gpusize*                            pOffset);

    Result AllocateGpuMemNoAllocLock(
        const GpuMemoryCreateInfo&          createInfo,
        const GpuMemoryInternalCreateInfo&  internalInfo,
        bool                                readOnly,
        GpuMemory**                         ppGpuMemory,
        gpusize*                            pOffset);

    Result AllocateAndBindGpuMem(
        IGpuMemoryBindable* pBindable,
        bool                readOnly);

    Result FreeGpuMem(
        GpuMemory*  pGpuMemory,
        gpusize     offset);

    GpuMemoryListIterator GetRefListIter() const { return m_references.Begin(); }
    Util::RWLock* GetRefListLock() { return &m_referenceLock; }
    Util::Mutex* GetAllocatorLock() { return &m_allocatorLock; }

    // It is assumed that the caller will take the references lock before calling this if necessary.
    uint32 ReferenceWatermark() { return m_referenceWatermark; }

    // Number of all allocations in the reference list. Note that this function takes the reference list lock.
    uint32 GetReferencesCount();

private:
    Result AllocateBaseGpuMem(
        const GpuMemoryCreateInfo&          createInfo,
        const GpuMemoryInternalCreateInfo&  internalInfo,
        bool                                readOnly,
        GpuMemory**                         ppGpuMemory);

    Result FreeBaseGpuMem(
        GpuMemory*  pGpuMemory);

    Device*const        m_pDevice;

    // Serialize access to the memory manager to ensure thread-safety
    Util::Mutex         m_allocatorLock;

    // Maintain a list of GPU memory objects that are sub-allocated
    GpuMemoryPoolList   m_poolList;

    // Maintain a list of internal GPU memory references
    GpuMemoryList       m_references;

    // Serialize access to the reference list
    Util::RWLock        m_referenceLock;

    // Ever-incrementing watermark to signal changes to the internal memory reference list
    uint32              m_referenceWatermark;

    PAL_DISALLOW_COPY_AND_ASSIGN(InternalMemMgr);
    PAL_DISALLOW_DEFAULT_CTOR(InternalMemMgr);
};

} // Pal
