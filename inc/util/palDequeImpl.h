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
/**
 ***********************************************************************************************************************
 * @file  palDequeImpl.h
 * @brief PAL utility collection Deque and DequeIterator class implementations.
 ***********************************************************************************************************************
 */

#pragma once

#include <utility>
#include "palDeque.h"
#include "palSysMemory.h"

namespace Util
{

// =====================================================================================================================
// Retrieves the element at position index.
template<typename T, typename Allocator>
T& Deque<T, Allocator>::InternalAt(
    uint32 index) const
{
    PAL_ASSERT(index < m_numElements);

    // Convert the abstract index into a physical index of elements across the chain of blocks. We must account for
    // invalid elements at the beginning of the front block (perhaps made invalid by prior PopFront calls).
    PAL_ASSERT(m_pFront >= static_cast<T*>(m_pFrontHeader->pStart));
    const size_t globalElemIndex = index + m_pFront - static_cast<T*>(m_pFrontHeader->pStart);

    // find the block
    DequeBlockHeader* pCurrentHeader = m_pFrontHeader;
    for (size_t blockIndex = globalElemIndex / m_numElementsPerBlock; blockIndex > 0; --blockIndex)
    {
        pCurrentHeader = pCurrentHeader->pNext;
    }

    // find the element in the block
    const size_t elementIndex = globalElemIndex % m_numElementsPerBlock;
    return *(static_cast<T*>(pCurrentHeader->pStart) + elementIndex);
}

// =====================================================================================================================
// Retrieves the element at position index.
template<typename T, typename Allocator>
const T& Deque<T, Allocator>::At(
    uint32 index) const
{
    return InternalAt(index);
}

// =====================================================================================================================
// Retrieves the element at position index.
template<typename T, typename Allocator>
T& Deque<T, Allocator>::At(
    uint32 index)
{
    return InternalAt(index);
}

// =====================================================================================================================
// Retrieves the element at position index.
template<typename T, typename Allocator>
T& Deque<T, Allocator>::operator[](
    uint32 index)
{
    return At(index);
}

// =====================================================================================================================
// Retrieves the element at position index.
template<typename T, typename Allocator>
const T& Deque<T, Allocator>::operator[](
    uint32 index) const
{
    return At(index);
}

// =====================================================================================================================
// Allocates a new block for storing additional data elements.  If the lazy-free block is present, just use that instead
// of allocating more memory.
template<typename T, typename Allocator>
DequeBlockHeader* Deque<T, Allocator>::AllocateNewBlock()
{
    DequeBlockHeader* pNewBlock = nullptr;

    if (m_pLazyFreeHeader != nullptr)
    {
        pNewBlock         = m_pLazyFreeHeader;
        m_pLazyFreeHeader = nullptr;

        // Fill in the newly allocated header. The caller is responsible for properly attaching the new block's header
        // to the list.
        pNewBlock->pPrev = nullptr;
        pNewBlock->pNext = nullptr;
    }
    else
    {
        const size_t blockSize   = m_numElementsPerBlock * sizeof(T);
        const size_t sizeToAlloc = sizeof(DequeBlockHeader) + blockSize;

        pNewBlock = static_cast<DequeBlockHeader*>(PAL_MALLOC(sizeToAlloc, m_pAllocator, AllocInternal));

        if (pNewBlock != nullptr)
        {
            // Fill in the newly allocated header. The caller is responsible for properly attaching the new block's
            // header to the list.
            pNewBlock->pPrev = nullptr;
            pNewBlock->pNext = nullptr;

            pNewBlock->pStart = (pNewBlock + 1);
            pNewBlock->pEnd   = VoidPtrInc(pNewBlock->pStart, blockSize);

            PAL_ASSERT(pNewBlock->pEnd == VoidPtrInc(pNewBlock, sizeToAlloc));
        }
    }

    return pNewBlock;
}

// =====================================================================================================================
// If there is currently no lazy-free block, cache the given block so that the next block allocation will be faster. If
// the lazy-free block already exists, actually frees the block's memory.
//
// The reason for this is because some use cases might cause us to ping-pong between N and N+1 blocks, which would
// result in excessive calls to PAL_MALLOC & PAL_FREE.
template<typename T, typename Allocator>
void Deque<T, Allocator>::FreeUnusedBlock(
    DequeBlockHeader* pHeader)
{
    if (m_pLazyFreeHeader == nullptr)
    {
        m_pLazyFreeHeader = pHeader;
    }
    else
    {
        PAL_SAFE_FREE(pHeader, m_pAllocator);
    }
}

// =====================================================================================================================
// Allocates space for a new element at the front of the queue.
template<typename T, typename Allocator>
Result Deque<T, Allocator>::AllocateFront(
    T** ppAllocatedSpace)
{
    Result result = Result::ErrorOutOfMemory;

    if ((m_pFrontHeader == nullptr) || (m_pFront == m_pFrontHeader->pStart))
    {
        // The current block has no more room at the front, or there are no blocks yet. In either case, allocate a new
        // front block.
        DequeBlockHeader*const pNewBlock = AllocateNewBlock();
        if (pNewBlock != nullptr)
        {
            // Add the new block to the front of the block linked-list.
            if (m_pFrontHeader != nullptr)
            {
                pNewBlock->pNext      = m_pFrontHeader;
                m_pFrontHeader->pPrev = pNewBlock;
            }

            m_pFrontHeader = pNewBlock;
            // The new front element is the last slot in the new block. We point the front element ptr off the block
            // because it gets decremented right before copying in the data.
            m_pFront = static_cast<T*>(pNewBlock->pEnd);

            if (m_pBackHeader == nullptr)
            {
                m_pBackHeader = pNewBlock;
                // If the deque is presently empty, the front and back element ptrs need to match at the end of this
                // function. Set up m_pBack to point where m_pFront will after the data is copied in.
                m_pBack = (m_pFront - 1);
            }
        }
    }

    if ((m_pFrontHeader != nullptr) && (m_pFront > m_pFrontHeader->pStart))
    {
        // There's room at the beginning of the current block, so we can throw the new element in there.
        ++m_numElements;
        --m_pFront;
        *ppAllocatedSpace = m_pFront;

        result = Result::_Success;
    }

    return result;
}

// =====================================================================================================================
// Allocates space for a new element at the back of the queue.
template<typename T, typename Allocator>
Result Deque<T, Allocator>::AllocateBack(
    T** ppAllocatedSpace)
{
    Result result = Result::ErrorOutOfMemory;

    if ((m_pBackHeader == nullptr) || ((m_pBack + 1) == m_pBackHeader->pEnd))
    {
        // The current block has no more room at the back, or there are no blocks yet. In either case, allocate a new
        // back block.
        DequeBlockHeader*const pNewBlock = AllocateNewBlock();
        if (pNewBlock != nullptr)
        {
            // Add the new block to the back of the block linked-list.
            if (m_pBackHeader != nullptr)
            {
                pNewBlock->pPrev     = m_pBackHeader;
                m_pBackHeader->pNext = pNewBlock;
            }

            m_pBackHeader = pNewBlock;
            // The new back element is the first slot in the new block. We point the back element ptr off the block
            // because it gets incremented right before copying in the data.
            m_pBack = (static_cast<T*>(pNewBlock->pStart) - 1);

            if (m_pFrontHeader == nullptr)
            {
                m_pFrontHeader = pNewBlock;
                // If the deque is presently empty, the front and back element ptrs need to match at the end of this
                // function. Set up m_pFront to point where m_pBack will after the data is copied in.
                m_pFront = static_cast<T*>(pNewBlock->pStart);
            }
        }
    }

    if ((m_pBackHeader != nullptr) && ((m_pBack + 1) < m_pBackHeader->pEnd))
    {
        // There's room at the end of the current block, so we can throw the new element in there.
        ++m_numElements;
        ++m_pBack;
        *ppAllocatedSpace = m_pBack;

        result = Result::_Success;
    }

    return result;
}

// =====================================================================================================================
// Inserts a new data element at the front of the deque.
template<typename T, typename Allocator>
Result Deque<T, Allocator>::PushFront(
    const T& data)
{
    T* pAllocatedSpace = nullptr;
    Result result = AllocateFront(&pAllocatedSpace);
    if (result == Result::_Success)
    {
        PAL_PLACEMENT_NEW(pAllocatedSpace) T(data);
    }
    return result;
}

// =====================================================================================================================
// Inserts a new data element at the front of the deque.
template<typename T, typename Allocator>
template<typename... Args>
Result Deque<T, Allocator>::EmplaceFront(
    Args&&... args)
{
    T* pAllocatedSpace = nullptr;
    Result result = AllocateFront(&pAllocatedSpace);
    if (result == Result::_Success)
    {
        PAL_PLACEMENT_NEW(pAllocatedSpace) T(std::forward<Args>(args)...);
    }
    return result;
}

// =====================================================================================================================
// Inserts a new data element at the back of the deque.
template<typename T, typename Allocator>
Result Deque<T, Allocator>::PushBack(
    const T& data)
{
    T* pAllocatedSpace = nullptr;
    Result result = AllocateBack(&pAllocatedSpace);
    if (result == Result::_Success)
    {
        PAL_PLACEMENT_NEW(pAllocatedSpace) T(data);
    }
    return result;
}

// =====================================================================================================================
// Inserts a new data element at the back of the deque.
template<typename T, typename Allocator>
template<typename... Args>
Result Deque<T, Allocator>::EmplaceBack(
    Args&&... args)
{
    T* pAllocatedSpace = nullptr;
    Result result = AllocateBack(&pAllocatedSpace);
    if (result == Result::_Success)
    {
        PAL_PLACEMENT_NEW(pAllocatedSpace) T(std::forward<Args>(args)...);
    }
    return result;
}

// =====================================================================================================================
// Pops an element off of the front of the deque.
template<typename T, typename Allocator>
Result Deque<T, Allocator>::PopFront(
    T* pOut)
{
    Result result = Result::ErrorUnavailable;

    if (m_numElements > 0)
    {
        PAL_ASSERT((m_pFrontHeader != nullptr) && (m_pFront != nullptr));

        // First, copy the front element into the output buffer and clean up our copy of it.
        if (pOut != nullptr)
        {
            *pOut = *m_pFront;
        }

        // Explicitly destroy the removed value if it's non-trivial.
        if (!std::is_trivial<T>::value)
        {
            m_pFront->~T();
        }
        --m_numElements;

        ++m_pFront; // Advance to the next element in the deque.

        if ((m_pFront == m_pFrontHeader->pEnd) || (m_numElements == 0))
        {
            // We've reached the end of the front block: therefore it is empty, and all other elements reside in other
            // blocks (if there are any).
            DequeBlockHeader*const pOldFrontHeader = m_pFrontHeader;

            if (m_pFrontHeader->pNext != nullptr)
            {
                // Need to fix-up the linked list of blocks.
                m_pFrontHeader        = m_pFrontHeader->pNext;
                m_pFrontHeader->pPrev = nullptr;
                // The new front element is the first element in the new front block.
                m_pFront = static_cast<T*>(m_pFrontHeader->pStart);
            }
            else
            {
                // The deque is now empty... clear our block & element pointers.
                PAL_ASSERT(m_pFrontHeader == m_pBackHeader);
                m_pFrontHeader = nullptr;
                m_pBackHeader  = nullptr;
                m_pFront       = nullptr;
                m_pBack        = nullptr;
            }

            // Need to free the now-unused block.
            FreeUnusedBlock(pOldFrontHeader);
        }

        result = Result::_Success;
    }

    return result;
}

// =====================================================================================================================
// Pops an element off of the back of the deque.
template<typename T, typename Allocator>
Result Deque<T, Allocator>::PopBack(
    T* pOut)
{
    Result result = Result::ErrorUnavailable;

    if (m_numElements > 0)
    {
        PAL_ASSERT((m_pBackHeader != nullptr) && (m_pBack != nullptr));

        // First, copy the back element into the output buffer and clean up our copy of it.
        if (pOut != nullptr)
        {
            *pOut = *m_pBack;
        }

        // Explicitly destroy the removed value if it's non-trivial.
        if (!std::is_trivial<T>::value)
        {
            m_pBack->~T();
        }
        --m_numElements;

        if ((m_pBack == m_pBackHeader->pStart) || (m_numElements == 0))
        {
            // We're currently at the beginning of the back block: therefore it just became empty, and all other
            // elements reside in other blocks (if there are any).
            DequeBlockHeader*const pOldBackHeader = m_pBackHeader;

            if (m_pBackHeader->pPrev != nullptr)
            {
                // Need to fix-up the linked list of blocks.
                m_pBackHeader        = m_pBackHeader->pPrev;
                m_pBackHeader->pNext = nullptr;
                // The new back element is the last element in the new back block.
                m_pBack = (static_cast<T*>(m_pBackHeader->pEnd) - 1);
            }
            else
            {
                // The deque is now empty... clear our block & element pointers.
                PAL_ASSERT(m_pFrontHeader == m_pBackHeader);
                m_pFrontHeader = nullptr;
                m_pBackHeader  = nullptr;
                m_pFront       = nullptr;
                m_pBack        = nullptr;
            }

            // Need to free the now-unused block.
            FreeUnusedBlock(pOldBackHeader);
        }
        else
        {
            --m_pBack; // Simply de-advance to the previous element in the deque.
        }

        result = Result::_Success;
    }

    return result;
}

// =====================================================================================================================
// Constructor.
template<typename T, typename Allocator>
DequeIterator<T, Allocator>::DequeIterator(
    const Deque<T, Allocator>*  pDeque,   // Deque to iterate over.
    DequeBlockHeader*           pHeader,  // Header of the block where the iterator starts.
    T*                          pCurrent) // Current object in the deque.
    :
    m_pDeque(pDeque),
    m_pCurrentHeader(pHeader),
    m_pCurrent(pCurrent)
{
}

// =====================================================================================================================
// Advances the iterator to the next element in the Deque.  If we go past the end, mark the current element pointer as
// invalid.
template<typename T, typename Allocator>
void DequeIterator<T, Allocator>::Next()
{
    if (m_pCurrent != nullptr)
    {
        if (m_pCurrent == m_pDeque->m_pBack)
        {
            // If we've gone past the end, mark the current pointer as invalid.
            m_pCurrent = nullptr;
        }
        else
        {
            ++m_pCurrent;   // Advance to the next element.

            if (m_pCurrent == m_pCurrentHeader->pEnd)
            {
                // Advance to the next block if we've reached the end of the current block.
                m_pCurrentHeader = m_pCurrentHeader->pNext;
                m_pCurrent       = nullptr;

                if (m_pCurrentHeader != nullptr)
                {
                    m_pCurrent = static_cast<T*>(m_pCurrentHeader->pStart);
                }
            }
        }
    }
}

// =====================================================================================================================
// Moves the iterator to the previous element in the Deque.  If there is no previous element, then mark the current
// element pointer as invalid.
template<typename T, typename Allocator>
void DequeIterator<T, Allocator>::Prev()
{
    if (m_pCurrent != nullptr)
    {
        if (m_pCurrent == m_pDeque->m_pFront)
        {
            // If we're pointing to the first element in the entire deque, then there is no previous element, and we're
            // done.  Just set "current" to null to indicate that we're off the end.
            m_pCurrent = nullptr;

            // Verify the deque integrity here...  If there's a previous header, then something has become seriously
            // corrupt.
            PAL_ASSERT(m_pCurrentHeader->pPrev == nullptr);
        }
        else
        {
            // Ok, we're not at the front of the entire deque, so there is something to backup to.  We could still be at
            // the start of this block though.
            if (m_pCurrent == m_pCurrentHeader->pStart)
            {
                // We're currently at the front of this block.  Need to back up to the previous block.
                m_pCurrentHeader = m_pCurrentHeader->pPrev;

                // If we're at the start of this block, but not the front of the entire deque, then there had better be
                // a previous header to backup into.
                PAL_ASSERT(m_pCurrentHeader != nullptr);

                // "pEnd" points to one location past the end of the chunk (i.e., there's nothing there to dereference),
                // so back up one location to start.
                m_pCurrent = static_cast<T*>(m_pCurrentHeader->pEnd) - 1;
            }
            else
            {
                // Still room to go backwards in this chunk, so just go back.
                --m_pCurrent;
            }
        }
    }
}

} // Util
