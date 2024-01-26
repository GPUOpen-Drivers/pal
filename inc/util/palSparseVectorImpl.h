/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  palSparseVectorImpl.h
* @brief PAL utility collection SparseVector class implementation.
***********************************************************************************************************************
*/

#pragma once

#include "palSparseVector.h"
#include <limits>

namespace Util
{

// =====================================================================================================================
template <typename T, typename CapacityType, CapacityType DefaultCapacity, typename Allocator, uint32... KeyRanges>
Result SparseVector<T, CapacityType, DefaultCapacity, Allocator, KeyRanges...>::Reserve(
    uint32 requiredCapacity)
{
    Result result = Result::Success;

    if (requiredCapacity > m_capacity)
    {
        static constexpr uint32 MaxCapacity = static_cast<uint32>((std::numeric_limits<CapacityType>::max)());
        if (requiredCapacity <= MaxCapacity)
        {
            // Create dynamically-allocated array.
            void*const pBuffer = PAL_MALLOC((sizeof(T) * requiredCapacity), m_pAllocator, AllocInternal);

            if (pBuffer != nullptr)
            {
                memcpy(pBuffer, m_pData, (sizeof(T) * NumElements()));

                if (m_pData != &m_localData[0])
                {
                    // Free data buffer if it uses storage from heap allocation.
                    PAL_FREE(m_pData, m_pAllocator);
                }

                m_pData    = static_cast<T*>(pBuffer);
                m_capacity = static_cast<CapacityType>(requiredCapacity);
            }
            else
            {
                result = Result::ErrorOutOfMemory;
            }
        }
        else
        {
            result = Result::ErrorInvalidValue;
        }
    }

    return result;
}

// =====================================================================================================================
template <typename T, typename CapacityType, CapacityType DefaultCapacity, typename Allocator, uint32... KeyRanges>
Result SparseVector<T, CapacityType, DefaultCapacity, Allocator, KeyRanges...>::Insert(
    uint32  key,
    T       value)
{
    Result result = Result::Success;

    const uint32 keyIndex   = GetKeyIndex<0, KeyRanges...>(key);
    const uint32 chunkIndex = GetChunkIndex(keyIndex);
    const uint64 chunkMask  = GetChunkMask(keyIndex);

    // Count the number of elements preceding where we want to insert.
    const uint32 leftDistance = (chunkIndex > 0 ? m_accumPop[chunkIndex - 1] : 0) +
                                CountSetBits(m_hasEntry[chunkIndex] & (chunkMask - 1));

    if ((m_hasEntry[chunkIndex] & chunkMask) == 0)
    {
        // Alloc more space if insertion requested when current size fills our current capacity.
        if (NumElements() == m_capacity)
        {
            // Allocate an additional amount of memory that is double the old size, up to MaxCapacity.
            static constexpr CapacityType MaxCapacity = (std::numeric_limits<CapacityType>::max)();
            if (m_capacity < MaxCapacity)
            {
                result = Reserve((m_capacity <= (MaxCapacity / 2)) ? (m_capacity * 2) : MaxCapacity);
            }
            else
            {
                result = Result::ErrorInvalidValue;
            }
        }

        if (result == Result::Success)
        {
            m_hasEntry[chunkIndex] |= chunkMask;

            const uint32 rightDistance = NumElements() - leftDistance;

            // Shift all following elements one over to the right to make space for our new element.
            if (rightDistance > 0)
            {
                memmove(&m_pData[leftDistance + 1], &m_pData[leftDistance], (sizeof(T) * rightDistance));
            }
        }
    }

    if (result == Result::Success)
    {
        m_pData[leftDistance] = value;

        for (uint32 i = chunkIndex; i < NumBitsetChunks; ++i)
        {
            ++m_accumPop[i];
        }
    }

    return result;
}

// =====================================================================================================================
template <typename T, typename CapacityType, CapacityType DefaultCapacity, typename Allocator, uint32... KeyRanges>
void SparseVector<T, CapacityType, DefaultCapacity, Allocator, KeyRanges...>::Erase(
    uint32 key)
{
    const uint32 keyIndex   = GetKeyIndex<0, KeyRanges...>(key);
    const uint32 chunkIndex = GetChunkIndex(keyIndex);
    const uint64 chunkMask  = GetChunkMask(keyIndex);

    if ((m_hasEntry[chunkIndex] & chunkMask) != 0)
    {
        const uint32 leftDistance  = (chunkIndex > 0 ? m_accumPop[chunkIndex - 1] : 0) +
                                     CountSetBits(m_hasEntry[chunkIndex] & (chunkMask - 1));

        const uint32 rightDistance = NumElements() - leftDistance;

        // Shift all following elements one over to the left to fill in the gap from deleting the element.
        if (rightDistance > 0)
        {
            memmove(&m_pData[leftDistance], &m_pData[leftDistance + 1], (sizeof(T) * rightDistance));
        }

        for (uint32 i = chunkIndex; i < NumBitsetChunks; ++i)
        {
            --m_accumPop[i];
        }

        m_hasEntry[chunkIndex] &= ~chunkMask;
    }
}

// =====================================================================================================================
template <typename T, typename CapacityType, CapacityType DefaultCapacity, typename Allocator, uint32... KeyRanges>
const T& SparseVector<T, CapacityType, DefaultCapacity, Allocator, KeyRanges...>::At(
    uint32 key
    ) const
{
    const uint32 keyIndex   = GetKeyIndex<0, KeyRanges...>(key);
    const uint32 chunkIndex = GetChunkIndex(keyIndex);
    const uint64 chunkMask  = GetChunkMask(keyIndex);

    PAL_ASSERT(HasEntry(key));

    const uint32 leftDistance = (chunkIndex > 0 ? m_accumPop[chunkIndex - 1] : 0) +
                                CountSetBits(m_hasEntry[chunkIndex] & (chunkMask - 1));
    return m_pData[leftDistance];
}

// =====================================================================================================================
template <typename T, typename CapacityType, CapacityType DefaultCapacity, typename Allocator, uint32... KeyRanges>
const T& SparseVector<T, CapacityType, DefaultCapacity, Allocator, KeyRanges...>::LowerBound(
    uint32 key
    ) const
{
    PAL_ASSERT(NumElements() > 0);

    const uint32 keyIndex   = GetKeyIndex<0, KeyRanges...>(key);
    const uint32 chunkIndex = GetChunkIndex(keyIndex);
    const uint64 chunkMask  = GetChunkMask(keyIndex);

    const uint32 leftDistance = (chunkIndex > 0 ? m_accumPop[chunkIndex - 1] : 0) +
                                CountSetBits(m_hasEntry[chunkIndex] & (chunkMask - 1));

    return (((m_hasEntry[chunkIndex] & chunkMask) != 0) || (leftDistance == 0)) ? m_pData[leftDistance]
                                                                                : m_pData[leftDistance - 1];
}

// =====================================================================================================================
template <typename T, typename CapacityType, CapacityType DefaultCapacity, typename Allocator, uint32... KeyRanges>
const T& SparseVector<T, CapacityType, DefaultCapacity, Allocator, KeyRanges...>::UpperBound(
    uint32 key
    ) const
{
    PAL_ASSERT(NumElements() > 0);

    const uint32 keyIndex   = GetKeyIndex<0, KeyRanges...>(key);
    const uint32 chunkIndex = GetChunkIndex(keyIndex);
    const uint64 chunkMask  = GetChunkMask(keyIndex);

    const uint32 leftDistance = (chunkIndex > 0 ? m_accumPop[chunkIndex - 1] : 0) +
                                CountSetBits(m_hasEntry[chunkIndex] & (chunkMask - 1));

    return ((leftDistance > 0) && (leftDistance < NumElements())) ? m_pData[leftDistance]
                                                                  : m_pData[leftDistance - 1];
}

// =====================================================================================================================
template <typename T, typename CapacityType, CapacityType DefaultCapacity, typename Allocator, uint32... KeyRanges>
bool SparseVector<T, CapacityType, DefaultCapacity, Allocator, KeyRanges...>::HasEntry(
    uint32  key,
    T*      pValue
    ) const
{
    const uint32 keyIndex   = GetKeyIndex<0, KeyRanges...>(key);
    const uint32 chunkIndex = GetChunkIndex(keyIndex);
    const uint64 chunkMask  = GetChunkMask(keyIndex);

    const bool entryExists = (m_hasEntry[chunkIndex] & chunkMask) != 0;

    if (entryExists)
    {
        const uint32 leftDistance = (chunkIndex > 0 ? m_accumPop[chunkIndex - 1] : 0) +
                                    CountSetBits(m_hasEntry[chunkIndex] & (chunkMask - 1));
        *pValue = m_pData[leftDistance];
    }

    return entryExists;
}

} // Util
