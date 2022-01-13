/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palBuddyAllocatorImpl.h
 * @brief PAL utility BuddyAllocator class implementation.
 ***********************************************************************************************************************
 */

#pragma once

#include "palBuddyAllocator.h"
#include "palInlineFuncs.h"
#include "palListImpl.h"
#include "palSysMemory.h"

namespace Util
{

// =====================================================================================================================
template <typename Allocator>
BuddyAllocator<Allocator>::BuddyAllocator(
    Allocator*      pAllocator,
    Pal::gpusize    baseAllocSize,
    Pal::gpusize    minAllocSize)
    :
    m_pAllocator(pAllocator),
    m_baseAllocKval(SizeToKval(baseAllocSize)),
    m_minKval(SizeToKval(minAllocSize)),
    m_pBlockLists(nullptr),
    m_numSuballocations(0)
{
    // Allocator must be non-null
    PAL_ASSERT(m_pAllocator != nullptr);

    // Base allocation size must be POT
    PAL_ASSERT(KvalToSize(m_baseAllocKval) == baseAllocSize);

    // Minimum allocation size must be POT
    PAL_ASSERT(KvalToSize(m_minKval) == minAllocSize);
}

// =====================================================================================================================
template <typename Allocator>
BuddyAllocator<Allocator>::~BuddyAllocator()
{
    if (m_pBlockLists != nullptr)
    {
        const uint32 numKvals = m_baseAllocKval - m_minKval;

        for (uint32 i = 0; i < numKvals; ++i)
        {
            for (auto it = m_pBlockLists[i].Begin(); it.Get() != nullptr;)
            {
                m_pBlockLists[i].Erase(&it);
            }

            // Call the destructor
            m_pBlockLists[i].~List();
        }

        // Free the block list array
        PAL_SAFE_FREE(m_pBlockLists, m_pAllocator);
    }
}

// =====================================================================================================================
// Gets maximum allocation size supported by this buddy allocator.
template <typename Allocator>
Pal::gpusize BuddyAllocator<Allocator>::MaximumAllocationSize() const
{
    // NOTE: Report one less than our base allocation k-value because there's no sense in suballocating a memory
    // request which is larger than half a chunk
    return KvalToSize(m_baseAllocKval - 1);
}

// =====================================================================================================================
// Initializes the buddy allocator.
template <typename Allocator>
Result BuddyAllocator<Allocator>::Init()
{
    PAL_ASSERT(m_pBlockLists == nullptr);

    Result result = Result::ErrorOutOfMemory;

    const uint32 numKvals = m_baseAllocKval - m_minKval;

    // Allocate the block lists
    m_pBlockLists = static_cast<BlockList*>(PAL_MALLOC(sizeof(BlockList) * numKvals,
                                                       m_pAllocator,
                                                       AllocInternal));

    if (m_pBlockLists != nullptr)
    {
        for (uint32 i = 0; i < numKvals; ++i)
        {
            PAL_PLACEMENT_NEW(&m_pBlockLists[i]) BlockList(m_pAllocator);
        }

        // We need to create the first two largest-size blocks and add them to the last block list
        const uint32        blockKval = (m_baseAllocKval - 1);
        const Pal::gpusize  blockSize = KvalToSize(blockKval);

        BlockList* pBlockList = &m_pBlockLists[blockKval - m_minKval];

        Block block = {};
        block.isFree = true;
        block.offset = 0;

        result = pBlockList->PushBack(block);
        PAL_ALERT(result != Result::Success);

        block.offset = blockSize;

        if (result == Result::Success)
        {
            result = pBlockList->PushBack(block);
            PAL_ALERT(result != Result::Success);
        }
    }

    return result;
}

// =====================================================================================================================
// Suballocates a block from the base allocation that this buddy allocator manages. If no free space is found then an
// appropriate error is returned.
template <typename Allocator>
Result BuddyAllocator<Allocator>::Allocate(
    Pal::gpusize    size,
    Pal::gpusize    alignment,
    Pal::gpusize*   pOffset)
{
    PAL_ASSERT(m_pBlockLists != nullptr);

    PAL_ASSERT(size <= MaximumAllocationSize());

    // Pad the requested allocation size to the nearest POT of the size and alignment
    const uint32 kval = Max(SizeToKval(Pow2Pad(Max(size, alignment))), m_minKval);

    Result result = GetNextFreeBlock(kval, pOffset);

    if (result == Result::Success)
    {
        // Increment the number of suballocations this buddy allocator manages
        m_numSuballocations++;
    }

    return result;
}

// =====================================================================================================================
// Gets the next free block of the provided size and marks it as used.
template <typename Allocator>
Result BuddyAllocator<Allocator>::GetNextFreeBlock(
    uint32          kval,
    Pal::gpusize*   pOffset)
{
    Result result = Result::ErrorOutOfGpuMemory;

    if (kval < m_baseAllocKval)
    {
        BlockList* pBlockList = &m_pBlockLists[kval - m_minKval];

        for (auto it = pBlockList->Begin(); it.Get() != nullptr; it.Next())
        {
            if (it.Get()->isFree)
            {
                // Allocate this block and return it
                it.Get()->isFree = false;
                *pOffset = it.Get()->offset;

                result = Result::Success;
                break;
            }
        }

        if (result != Result::Success)
        {
            // If we didn't find any free blocks in the list then we need to get a block one size bigger and split it
            result = GetNextFreeBlock(kval + 1, pOffset); // Determine the offset for the larger block
            if (result == Result::Success)
            {
                // Now we need to create two blocks in that larger block's space. First, the block we'll return
                Block block = {};
                block.isFree = false;
                block.offset = *pOffset;
                result = pBlockList->PushBack(block);

                if (result == Result::Success)
                {
                    // ... and its buddy which starts out as free
                    block.isFree = true;
                    block.offset = *pOffset + KvalToSize(kval);
                    result = pBlockList->PushBack(block);
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Frees a suballocated block making it available for future re-use.
template <typename Allocator>
void BuddyAllocator<Allocator>::Free(
    Pal::gpusize    offset,
    Pal::gpusize    size,
    Pal::gpusize    alignment)
{
    PAL_ASSERT(m_pBlockLists != nullptr);

    uint32 startKval = Max(SizeToKval(Pow2Pad(Max(size, alignment))), m_minKval);

    Result result = FreeBlock(startKval, offset);

    // Freeing should always succeed unless something went wrong with the allocation scheme
    PAL_ASSERT(result == Result::Success);

    // Decrement the number of suballocations this buddy allocator manages
    m_numSuballocations--;
}

// =====================================================================================================================
// Frees a block with the matching offset.
template <typename Allocator>
Result BuddyAllocator<Allocator>::FreeBlock(
    uint32          kval,
    Pal::gpusize    offset)
{
    Result result = Result::ErrorInvalidValue;

    // If this assert is hit then something went wrong with the allocation patterns
    PAL_ASSERT((kval >= m_minKval) && (kval < m_baseAllocKval));

    for (; (kval < m_baseAllocKval) && (result != Result::Success); ++kval)
    {
        BlockList* pBlockList = &m_pBlockLists[kval - m_minKval];

        for (auto it = pBlockList->Begin(); it.Get() != nullptr; it.Next())
        {
            Block* pBlock = it.Get();
            if (pBlock->offset == offset)
            {
                // At this point this block should not be free
                PAL_ASSERT(pBlock->isFree == false);

                pBlock->isFree = true;

                // Find the buddy block to see if it is free, and if so remove the blocks and free the next size up.
                // Because all offsets are zero relative and aligned to block size, buddy offset can be simply
                // calculated by XOR-ing the block offset with its size.
                Pal::gpusize buddyOffset = (offset ^ KvalToSize(kval));

                // Because buddies are always consecutive in the block list we can find the buddy block next to the
                // current one.
                if (buddyOffset > offset)
                {
                    it.Next();
                }
                else
                {
                    it.Prev();
                }
                PAL_ASSERT(it.Get() != nullptr && it.Get()->offset == buddyOffset);

                // If the buddy is free then remove both blocks from the list, unless we're at the largest block size
                if (it.Get()->isFree && (kval < m_baseAllocKval - 1))
                {
                    pBlockList->Erase(&it);

                    // Erasing a node advances the iterator, if the next node is not our block, then we need to move
                    // back one node
                    if (it.Get() != pBlock)
                    {
                        it.Prev();
                    }
                    pBlockList->Erase(&it);

                    // Recursively call this method again to free the next block size up
                    FreeBlock(kval + 1, Min(offset, buddyOffset));
                }

                // Finished successfully
                result = Result::Success;
                break;
            }
        }
    }

    return result;
}

} // Pal
