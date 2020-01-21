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
/**
 ***********************************************************************************************************************
 * @file  palVectorImpl.h
 * @brief PAL utility collection Vector and VectorIterator class implementations.
 ***********************************************************************************************************************
 */

#pragma once

#include "palVector.h"
#include "palSysMemory.h"

namespace Util
{

// =====================================================================================================================
// If new capacity exceeds maximum capacity, allocates on the heap new storage for data buffer,
// moves objects from data buffer to heap allocation and takes ownership of that allocation.
// When maximum capacity is large enough to accommodate new capacity, this method does nothing.
template<typename T, uint32 defaultCapacity, typename Allocator>
Result Vector<T, defaultCapacity, Allocator>::Reserve(
    uint32 newCapacity)
{
    Result result = Result::_Success;

    // Not enough storage.
    if (m_maxCapacity < newCapacity)
    {
        // Allocate storage on the heap.
        void* const pNewMemory = PAL_MALLOC(sizeof(T) * newCapacity, m_pAllocator, AllocInternal);

        if (pNewMemory == nullptr)
        {
            // MALLOC has failed.
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            T* const pNewData = static_cast<T*>(pNewMemory);

            if (std::is_pod<T>::value)
            {
                // Optimize trivial types by copying local buffer.
                std::memcpy(pNewData, m_pData, sizeof(T) * m_numElements);
            }
            else
            {
                // Move objects from data buffer to heap alloation.
                // Destory corpses of objects in data buffer after moving.
                for (uint32 idx = 0; idx < m_numElements; ++idx)
                {
                    PAL_PLACEMENT_NEW(pNewData + idx) T(Move(m_pData[idx]));
                    m_pData[idx].~T();
                }
            }

            // Free data buffer if it uses storage from heap allocation.
            if (m_pData != reinterpret_cast<T*>(m_data))
            {
                PAL_FREE(m_pData, m_pAllocator);
            }

            // Take ownership of the heap allocation.
            m_pData       = pNewData;
            m_maxCapacity = newCapacity;
        }
    }

    return result;
}

// =====================================================================================================================
// Pushes the new element to the end of the vector. If the vector has reached maximum capacity, new space is allocated
// on the heap and the data in the old space is copied over to the new space. The old space is freed if it was also
// allocated on the heap.
template<typename T, uint32 defaultCapacity, typename Allocator>
Result Vector<T, defaultCapacity, Allocator>::PushBack(
    const T& data)
{
    Result result = Result::_Success;

    // Alloc more space if push back requested when current size is at max capacity.
    if (m_numElements == m_maxCapacity)
    {
        // Allocate an additional amount of memory that is double the old size.
        result = Reserve(m_numElements * 2);
    }

    if (result == Result::_Success)
    {
        // Insert new data into the array.
        PAL_PLACEMENT_NEW(m_pData + m_numElements) T(data);
        ++(m_numElements);
    }

    return result;
}

// =====================================================================================================================
template<typename T, uint32 defaultCapacity, typename Allocator>
void Vector<T, defaultCapacity, Allocator>::PopBack(
    T* pData)
{
    PAL_ASSERT(IsEmpty() == false);
    --m_numElements;

    if (pData != nullptr)
    {
        *pData = *(m_pData + m_numElements);
    }

    // Explicitly destroy the removed value if it's non-trivial.
    if (!std::is_pod<T>::value)
    {
        m_pData[m_numElements].~T();
    }
}

// =====================================================================================================================
template<typename T, uint32 defaultCapacity, typename Allocator>
void Vector<T, defaultCapacity, Allocator>::Clear()
{
    // Explicitly destroy all non-trivial types.
    if (!std::is_pod<T>::value)
    {
        for (uint32 idx = 0; idx < m_numElements; ++idx)
        {
            m_pData[idx].~T();
        }
    }

    m_numElements = 0;
}

} // Util
