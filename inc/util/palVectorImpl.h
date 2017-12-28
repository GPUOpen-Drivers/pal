/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
        const uint32 oldNumBytes = m_maxCapacity * sizeof(ValueStorage);
        const uint32 newNumBytes = oldNumBytes << 1;

        const void* pOldMem = m_pData;
        void*       pNewMem = PAL_MALLOC(newNumBytes, m_pAllocator, AllocInternal);

        if (pNewMem == nullptr)
        {
            // MALLOC has failed.
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            // Move the old data to the new space, use memcpy to optimize trivial types.
            if (std::is_pod<T>::value)
            {
                memcpy(pNewMem, pOldMem, oldNumBytes);
            }
            else
            {
                T* pNewData = static_cast<T*>(pNewMem);
                for (uint32 idx = 0; idx < m_numElements; ++idx)
                {
                    PAL_PLACEMENT_NEW(pNewData + idx) T(m_pData[idx]);
                    m_pData[idx].~T();
                }
            }

            m_pData       = static_cast<T*>(pNewMem);
            m_maxCapacity = m_maxCapacity << 1;
        }

        // Free old memory if it was allocated on the heap, i.e. if the current pointer to the data buffer is not the
        // same as the statically allocated buffer.
        if (m_data != pOldMem)
        {
            PAL_FREE(pOldMem, m_pAllocator);
        }
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
