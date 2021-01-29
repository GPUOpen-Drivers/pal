/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palBestFitAllocatorImpl.h
 * @brief PAL utility BestFitAllocator class implementation.
 ***********************************************************************************************************************
 */

#pragma once

#include "palBestFitAllocator.h"
#include "palInlineFuncs.h"
#include "palListImpl.h"

namespace Util
{

// =====================================================================================================================
template<typename Allocator>
BestFitAllocator<Allocator>::BestFitAllocator(
    Allocator*   pAllocator,
    Pal::gpusize baseAllocSize,
    Pal::gpusize minAllocSize)
    :
    m_pAllocator(pAllocator),
    m_totalBytes(baseAllocSize),
    m_minBlockSize(minAllocSize),
    m_freeBytes(baseAllocSize),
    m_blockList(pAllocator)
{
    // Allocator must be non-null
    PAL_ASSERT(m_pAllocator != nullptr);

    // baseAllocSize and minAllocSize must be POT
    PAL_ASSERT(IsPowerOfTwo(baseAllocSize) && IsPowerOfTwo(minAllocSize));

    // baseAllocSize must be aligned to minAllocsize
    PAL_ASSERT((baseAllocSize % minAllocSize) == 0);
}

// =====================================================================================================================
template<typename Allocator>
BestFitAllocator<Allocator>::~BestFitAllocator()
{
    // Prevent access violation in the case that init is failed
    if (m_blockList.NumElements() > 0)
    {
        SanityCheck();

        auto block = m_blockList.Begin();
        // If we don't have a single block that isn't busy, then the user didn't free all of the memory
        PAL_ALERT(!((m_blockList.NumElements() == 1) && (block.Get()->isBusy == false)));

        // Clear out the list so that Util::List doesn't complain
        while (block.Get() != nullptr)
        {
            m_blockList.Erase(&block);
        }
    }
}

// =====================================================================================================================
// Initializes the buddy allocator.
template <typename Allocator>
Result BestFitAllocator<Allocator>::Init()
{
    return m_blockList.PushFront({0, m_freeBytes, false});
}

// =====================================================================================================================
// Suballocates a block from the base allocation that this allocator manages. If no free space is found then an
// appropriate error is returned.
template<typename Allocator>
Result BestFitAllocator<Allocator>::Allocate(
    Pal::gpusize  size,
    Pal::gpusize  alignment,
    Pal::gpusize* pOffset)
{
    PAL_ASSERT(m_blockList.NumElements() > 0);

    Result result = Result::Success;
    auto bestBlock = m_blockList.End();

    size = Pow2Align(size, m_minBlockSize);
    alignment = Pow2Align(alignment, m_minBlockSize);

    if (size > MaximumAllocationSize())
    {
        result = Result::ErrorOutOfGpuMemory;
    }

    if (result == Result::Success)
    {
        for (auto it = m_blockList.Begin(); it != m_blockList.End(); it.Next())
        {
            if ((it.Get()->isBusy == false) &&
                IsPow2Aligned(it.Get()->offset, alignment) &&
                (it.Get()->size >= size) &&
                ((bestBlock == m_blockList.End()) || (it.Get()->size < bestBlock.Get()->size)))
            {
                bestBlock = it;
            }
        }

        // There's no block that could hold the allocation
        if (bestBlock == m_blockList.End())
        {
            result = Result::ErrorOutOfGpuMemory;
        }
    }

    if (result == Result::Success)
    {
        // Need to split block
        if (bestBlock.Get()->size != size)
        {
            result = m_blockList.InsertBefore(&bestBlock, {bestBlock.Get()->offset, size, true});

            if (result == Result::Success)
            {
                bestBlock.Get()->size -= size;
                bestBlock.Get()->offset += size;
                bestBlock.Prev();
            }
        }
    }

    if (result == Result::Success)
    {
        m_freeBytes -= size;
        bestBlock.Get()->isBusy = true;
        *pOffset = bestBlock.Get()->offset;
    }

    SanityCheck();

    return result;
}

// =====================================================================================================================
// Frees a suballocated block making it available for future re-use.
template<typename Allocator>
void BestFitAllocator<Allocator>::Free(
    Pal::gpusize offset,
    Pal::gpusize size,
    Pal::gpusize alignment)
{
    PAL_ASSERT(m_blockList.NumElements() > 0);

    PAL_ALERT(!((offset % m_minBlockSize) == 0));

    auto block = m_blockList.End();

    for (auto it = m_blockList.Begin(); it != m_blockList.End(); it.Next())
    {
        if (it.Get()->offset == offset)
        {
            // The block has to be busy
            PAL_ALERT(!(it.Get()->isBusy == true));

            block = it;
            break;
        }
    }

    // The block was never allocated?
    PAL_ASSERT(block != m_blockList.End());

    block.Get()->isBusy = false;
    m_freeBytes += block.Get()->size;

    // try to merge with next block
    auto nextBlock = block;
    nextBlock.Next();
    if ((nextBlock != m_blockList.End()) &&
        (nextBlock.Get()->isBusy == false))
    {
        block.Get()->size += nextBlock.Get()->size;
        m_blockList.Erase(&nextBlock);
    }

    // try to merge with previous block
    auto prevBlock = block;
    prevBlock.Prev();
    if ((prevBlock != block) &&
        (prevBlock.Get()->isBusy == false))
    {
        prevBlock.Get()->size += block.Get()->size;
        m_blockList.Erase(&block);
    }

    SanityCheck();
}

// =====================================================================================================================
template<typename Allocator>
bool BestFitAllocator<Allocator>::IsEmpty() const
{
    return m_freeBytes == 0u;
}

// =====================================================================================================================
// Gets maximum allocation size supported by this buddy allocator.
template<typename Allocator>
Pal::gpusize BestFitAllocator<Allocator>::MaximumAllocationSize() const
{
    return m_totalBytes;
}

// =====================================================================================================================
template<typename Allocator>
void BestFitAllocator<Allocator>::SanityCheck()
{
#if DEBUG
    PAL_ASSERT(m_blockList.NumElements() > 0);

    auto prevBlock = m_blockList.Begin();
    gpusize totalBytes = prevBlock.Get()->size;
    gpusize freeBytes = prevBlock.Get()->isBusy ? 0u : prevBlock.Get()->size;
    auto it = m_blockList.Begin();
    it.Next();
    for (; it != m_blockList.End(); it.Next(), prevBlock.Next())
    {
        // There should never be neighbour blocks that are both free
        PAL_ASSERT((prevBlock.Get()->isBusy == true) || (it.Get()->isBusy == true));

        // The next block should start off where the previous one finished
        PAL_ASSERT((prevBlock.Get()->offset + prevBlock.Get()->size) == it.Get()->offset);

        totalBytes += it.Get()->size;
        freeBytes += it.Get()->isBusy ? 0u : it.Get()->size;
    }

    // should be the same
    PAL_ASSERT(totalBytes == m_totalBytes);
    PAL_ASSERT(freeBytes == m_freeBytes);
#endif
}

} // Util
