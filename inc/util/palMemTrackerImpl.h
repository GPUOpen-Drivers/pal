/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
 ***********************************************************************************************************************
 * @file  palMemTrackerImpl.h
 * @brief PAL utility collection MemTracker class implementations.
 ***********************************************************************************************************************
 */

#pragma once

#if PAL_MEMTRACK

#include "palMemTracker.h"
#include "palSysMemory.h"

#include <cstring>

namespace Util
{

/// Table to convert a blockType to a string. Used by the logging routines.
static const char*const MemBlkTypeStr[] =
{
    "Malloc",       ///< MemBlkType::Malloc
    "New",          ///< MemBlkType::New
    "NewArray",     ///< MemBlkType::NewArray
};

// =====================================================================================================================
template <typename Allocator>
MemTracker<Allocator>::MemTracker(
    Allocator*const pAllocator)
    :
    m_pMemListHead(&m_memListHead),
    m_markerSizeUints(MarkerSizeUints),
    m_markerSizeBytes(MarkerSizeBytes),
    m_pAllocator(pAllocator),
    m_nextAllocNum(1),
    m_breakOnAllocNum(0)
{
    memset(&m_memListHead, 0, sizeof(MemTrackerElem));
}

// =====================================================================================================================
template <typename Allocator>
MemTracker<Allocator>::~MemTracker()
{
    // Clean-up leaked memory if needed
    if (m_pMemListHead->pNext != nullptr)
    {
        // If the dummy head has a valid next pointer, we have a leak.  The leak could either be caused by an
        // internal PAL leak, a client leak, or even the application not destroying API objects.
        PAL_ALERT_ALWAYS();

        // Dump out a list of unfreed blocks.
        MemoryReport();

    }
}

// =====================================================================================================================
// Initializes the MemTracker singleton.
template <typename Allocator>
Result MemTracker<Allocator>::Init()
{
    return m_mutex.Init();
}

// =====================================================================================================================
// Adds the newly allocated memory block to the list of blocks for tracking.
//
// The tracking information includes things like filename, line numbers, and type of block.  Also, given a pointer,
// adds the Underrun/Overrun markers to the memory allocated, and return a pointer to the actual client usable memory.
//
// See MemTracker::Alloc() which is used to allocate memory that is being tracked.
template <typename Allocator>
void* MemTracker<Allocator>::AddMemElement(
    void*       pMem,        // [in,out] Original pointer allocated by MemTracker::Alloc.
    size_t      bytes,       // Client requested allocation size in bytes.
    size_t      align,       // Client-requested alignment in bytes.
    MemBlkType  blockType,   // Block type based on calling allocation routine.
    const char* pFilename,   // Client filename that is requesting the memory.
    uint32      lineNumber)  // Line number in client file that is requesting the memory.
{
    // Increment memory pointer for alloced memory.
    void* pClientMem = VoidPtrAlign(VoidPtrInc(pMem, m_markerSizeBytes), align);

    uint32* pUnderrun = static_cast<uint32*>(VoidPtrDec(pClientMem, m_markerSizeBytes));
    uint32* pOverrun  = static_cast<uint32*>(VoidPtrInc(pClientMem, Pow2Align(bytes, sizeof(uint32))));

    // Mark the memory with the underrun/overrun marker.
    for (uint32 markerUints = 0; markerUints < m_markerSizeUints; ++markerUints)
    {
        *pUnderrun++ = UnderrunSentinel;
        *pOverrun++  = OverrunSentinel;
    }

    MemTrackerElem*const pNewElement = static_cast<MemTrackerElem*>(malloc(sizeof(MemTrackerElem)));

    if (pNewElement != nullptr)
    {
        pNewElement->size       = bytes;
        pNewElement->pFilename  = pFilename;
        pNewElement->lineNumber = lineNumber;
        pNewElement->blockType  = blockType;
        pNewElement->pClientMem = pClientMem;
        pNewElement->pOrigMem   = pMem;

        MutexAuto lock(&m_mutex);

        // Trigger an assert if we're about to allocate the break-on-allocation number.
        if (m_nextAllocNum == m_breakOnAllocNum)
        {
            PAL_ASSERT_ALWAYS();
        }

        pNewElement->allocNum = m_nextAllocNum;
        ++m_nextAllocNum;

        pNewElement->pNext     = m_pMemListHead->pNext;
        pNewElement->pNextNext = m_pMemListHead->pNextNext;

        m_pMemListHead->pNextNext = m_pMemListHead->pNext;
        m_pMemListHead->pNext     = pNewElement;

        // Sanity check to make sure we've updated both pointers properly.
        PAL_ASSERT(m_pMemListHead->pNextNext != m_pMemListHead->pNext);
    }

    return pClientMem;
}

// =====================================================================================================================
// Removes an allocated block from the list of blocks used for tracking.
//
// The routine checks for invalid frees (and duplicate frees). Also, the routine is able to detect mismatched alloc/free
// usage based on the blockType.  The routine is called with the pointer to the client usable memory and returns the
// pointer to the allocated memory.
//
// See MemTracker::Free() which is used to free memory that is being tracked.
template <typename Allocator>
void* MemTracker<Allocator>::RemoveMemElement(
    void*       pClientMem,  // Pointer to client usable memory.
    MemBlkType  blockType)   // Block type based on calling deallocation routine.
{
    bool  badFree  = false;
    void* pOrigPtr = nullptr;

    m_mutex.Lock();

    MemTrackerElem* pCurrent  = m_pMemListHead->pNext;
    MemTrackerElem* pCurNext  = m_pMemListHead->pNextNext;
    MemTrackerElem* pPrevious = m_pMemListHead;
    MemTrackerElem* pPrevPrev = nullptr;

    while ((pCurrent != nullptr) && (pCurrent->pClientMem != pClientMem))
    {
        // We will hit this assert if someone has corrupted the current tracker's pNext.
        // That probably means that someone is writing into random heap memory they don't own.
        PAL_ASSERT(pCurNext == pCurrent->pNext);

        pPrevPrev = pPrevious;
        pPrevious = pCurrent;
        pCurNext  = pCurrent->pNextNext;
        pCurrent  = pCurrent->pNext;
    }

    // We should not be trying to free something twice or trying to free something which has not been allocated.  The
    // assert will catch the invalid free. Current equals previous only for empty lists.

    if (pCurrent == nullptr)
    {
        // A free was attempted on an unrecognized pointer.
        PAL_DPERROR("Invalid Free Attempted with ptr = : (%#x)", pClientMem);
        badFree = true;
    }
    else if (pCurrent->blockType != blockType)
    {
        // We have a mismatch in the alloc/free pair, e.g. PAL_NEW with PAL_FREE etc.  return early here without freeing
        // the memory so it shows up as a leak.
        PAL_DPERROR("Trying to Free %s as %s.",
                  MemBlkTypeStr[static_cast<uint32>(pCurrent->blockType)],
                  MemBlkTypeStr[static_cast<uint32>(blockType)]);
        badFree = true;
    }
    else
    {
        // Update the linked list to no longer contain the element we are removing.
        if (pPrevPrev != nullptr)
        {
            pPrevPrev->pNextNext = pCurrent->pNext;

            // Sanity check to make sure we've updated both pointers properly.
            PAL_ASSERT((pPrevPrev->pNextNext == nullptr) || (pPrevPrev->pNextNext != pPrevPrev->pNext));
        }

        pPrevious->pNextNext = pCurrent->pNextNext;
        pPrevious->pNext     = pCurrent->pNext;
        pOrigPtr             = pCurrent->pOrigMem;

        // Sanity check to make sure we've updated both pointers properly.
        PAL_ASSERT((pPrevious->pNextNext == nullptr) || (pPrevious->pNextNext != pPrevious->pNext));
    }

    m_mutex.Unlock();

    if (badFree == false)
    {
        // We can check for memory corruption at top and bottom since the element was found in our free list.

        uint32* pUnderrun = static_cast<uint32*>(VoidPtrDec(pClientMem, m_markerSizeBytes));
        uint32* pOverrun  = static_cast<uint32*>(VoidPtrInc(pClientMem, Pow2Align(pCurrent->size, sizeof(uint32))));

        for (uint32 markerUints = 0; markerUints < m_markerSizeUints; ++markerUints)
        {
            PAL_ASSERT(*pUnderrun++ == UnderrunSentinel);
            PAL_ASSERT(*pOverrun++  == OverrunSentinel);
        }

        // Free the current memory element
        free(pCurrent);
    }

    // Return a pointer to the actual allocated block.
    return pOrigPtr;
}

// =====================================================================================================================
// Allocates a block of memory and tracks it using the memory tracker.
template <typename Allocator>
void* MemTracker<Allocator>::Alloc(
    const AllocInfo& allocInfo)
{
    // Allocating zero bytes of memory results in undefined behavior.
    PAL_ASSERT(allocInfo.bytes > 0);

    void* pMem = nullptr;

    size_t paddedSizeBytes = allocInfo.bytes;

    // Allocate two "m_markerSizeBytes" elements to detect over/under runs of the memory range.  The overrun marker will
    // actually start at the next aligned point after the allocation.  We need to allocate additional space to re-align
    // returned start pointer so that the address immediately following the underrun marker is properly aligned.
    if (m_markerSizeBytes != 0)
    {
        paddedSizeBytes  = Pow2Align(paddedSizeBytes, sizeof(uint32));
        paddedSizeBytes += m_markerSizeBytes * 2;
        paddedSizeBytes += allocInfo.alignment;
    }

    AllocInfo memTrackerInfo(allocInfo);
    memTrackerInfo.bytes = paddedSizeBytes;

    pMem = m_pAllocator->Alloc(memTrackerInfo);

    if (pMem != nullptr)
    {
        // Don't bother adding a failed allocation to the Memtrack list.
        pMem = AddMemElement(pMem,
                             allocInfo.bytes,
                             allocInfo.alignment,
                             allocInfo.blockType,
                             allocInfo.pFilename,
                             allocInfo.lineNumber);
    }

    return pMem;
}

// =====================================================================================================================
// Frees a block of memory.  The routine is called with the pointer to the client usable memory.
//
// See MemTracker::RemoveMemElement() which is used to validate the free.
template <typename Allocator>
void MemTracker<Allocator>::Free(
    const FreeInfo& freeInfo)
{
    // Don't want to call RemoveMemElement if the ptr is null.
    if (freeInfo.pClientMem != nullptr)
    {
        void* pMem = RemoveMemElement(freeInfo.pClientMem, freeInfo.blockType);

        // If this free call is valid (RemoveMemElement doesn't return nullptr), release the memory.
        if (pMem != nullptr)
        {
            m_pAllocator->Free(FreeInfo(pMem, freeInfo.blockType));
        }
    }
}

// =====================================================================================================================
// Frees all memory that has not been explicitly freed (in other words, memory that has leaked).  This function is only
// expected to be called when the memory tracker is being destroyed.
template <typename Allocator>
void MemTracker<Allocator>::FreeLeakedMemory()
{
    const MemTrackerElem* pCurrent = m_pMemListHead->pNext;

    while (pCurrent != nullptr)
    {
        // Free will release the memory for tracking and the actual element.
        Free(FreeInfo(pCurrent->pClientMem, pCurrent->blockType));
        pCurrent = m_pMemListHead->pNext;
    }
}

// =====================================================================================================================
// Outputs information about leaked memory by traversing the memory tracker list.
template <typename Allocator>
void MemTracker<Allocator>::MemoryReport()
{
    const MemTrackerElem* pCurrent = m_pMemListHead;

    PAL_DPWARN("================ List of Leaked Blocks ================");

    while (pCurrent->pNext != nullptr)
    {
        pCurrent = pCurrent->pNext;
        PAL_DPWARN("AllocSize = %8d, MemBlkType = %s, File = %-15s, LineNumber = %8d, AllocNum = %8d",
                   pCurrent->size,
                   MemBlkTypeStr[static_cast<uint32>(pCurrent->blockType)],
                   pCurrent->pFilename,
                   pCurrent->lineNumber,
                   pCurrent->allocNum);
    }

    PAL_DPWARN("================ End of List ===========================");
}

} // Util

#endif
