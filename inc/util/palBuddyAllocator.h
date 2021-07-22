/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palBuddyAllocator.h
 * @brief PAL utility BuddyAllocator class declaration.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palList.h"

namespace Util
{

/**
 ***********************************************************************************************************************
 * @brief  Buddy Allocator (see http://en.wikipedia.org/wiki/Buddy_memory_allocation for more info).
 *
 * Responsible for managing small GPU memory requests by allocating a large base allocation and dividing it into
 * appropriately sized suballocation blocks.
 *
 * @warning The buddy allocator is not thread-safe so thread-safety has to be handled on the caller side.
 ***********************************************************************************************************************
 */
template <typename Allocator>
class BuddyAllocator
{
public:
    /// Constructor.
    ///
    /// @param [in]  pAllocator     The allocator that will allocate memory if required.
    /// @param [in]  baseAllocSize  The size of the base allocation this buddy allocator suballocates.
    /// @param [in]  minAllocSize   The size of the smallest block this buddy allocator can allocate.
    BuddyAllocator(
        Allocator*      pAllocator,
        Pal::gpusize    baseAllocSize,
        Pal::gpusize    minAllocSize);
    ~BuddyAllocator();

    /// Initializes the buddy allocator.
    ///
    /// @returns Success if the buddy allocator has been successfully initialized.
    Result Init();

    /// Suballocates a block from the base allocation that this buddy allocator manages.
    ///
    /// @param [in]  size           The size of the requested suballocation.
    /// @param [in]  alignment      The alignment requirements of the requested suballocation.
    /// @param [out] pOffset        The offset the suballocated block starts within the base allocation.
    ///
    /// @returns Success if the allocation succeeded, @ref ErrorOutOfMemory if there isn't enough system memory to
    ///          fulfill the request, or @ref ErrorOutOfGpuMemory if there isn't a large enough block free in the
    ///          base allocation to fulfill the request.
    Result Allocate(
        Pal::gpusize    size,
        Pal::gpusize    alignment,
        Pal::gpusize*   pOffset);

    /// Frees a previously allocated suballocation.
    ///
    /// @param [in]  offset         The offset the suballocated block starts within the base allocation.
    /// @param [in]  size           Optional parameter specifying the size of the original allocation.
    /// @param [in]  alignment      Optional parameter specifying the alignment of the original allocation.
    void Free(
        Pal::gpusize    offset,
        Pal::gpusize    size = 0,
        Pal::gpusize    alignment = 0);

    /// Tells whether the base allocation is completely free. If the returned value is true then the caller is safe
    /// to deallocate the base allocation.
    bool IsEmpty() const
    {
        return (m_numSuballocations == 0);
    }

    /// Returns the size of the largest allocation that can be suballocated with this buddy allocator.
    Pal::gpusize MaximumAllocationSize() const;

private:
    struct Block
    {
        bool            isFree; // Indicates the in-use status of the block
        Pal::gpusize    offset; // Byte offset from the base allocation address where this block begins
    };

    typedef Util::List<Block, Allocator> BlockList;

    Result GetNextFreeBlock(
        uint32              kval,
        Pal::gpusize*       pOffset);

    Result FreeBlock(
        uint32              kval,
        Pal::gpusize        offset);

    static constexpr Pal::gpusize KvalToSize(uint32 kVal) { return (1ull << kVal); }

    static uint32 SizeToKval(Pal::gpusize size) { return Log2(size); }

    Allocator* const    m_pAllocator;

    const uint32        m_baseAllocKval;
    const uint32        m_minKval;

    BlockList*          m_pBlockLists;

    uint32              m_numSuballocations;

    PAL_DISALLOW_COPY_AND_ASSIGN(BuddyAllocator);
    PAL_DISALLOW_DEFAULT_CTOR(BuddyAllocator);
};

} // Util
