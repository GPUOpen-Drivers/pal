/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <utility>
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

            if (std::is_trivial<T>::value)
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
// Resize vector.  If the new size is smaller, the elements at the end of Vector will be destroyed, though their memory
// will remain.  If the new size is bigger, more space may be reserved.  The amount of memory reserved will be a power
// of 2.
template<typename T, uint32 defaultCapacity, typename Allocator>
Result Vector<T, defaultCapacity, Allocator>::Resize(
    uint32 newSize,
    const T& newVal)
{
    Result result = Result::_Success;

    if (m_numElements > newSize)
    {
        if (std::is_trivial<T>::value)
        {
            // Trivial value, so we don't need to destroy any objects.  Just shrink m_numElements.
            m_numElements = newSize;
        }
        else
        {
            // Explicitly destroy the removed value if it's non-trivial.
            while (m_numElements > newSize)
            {
                m_pData[--m_numElements].~T();
            }
        }
    }
    else if (m_numElements < newSize)
    {
        // Reserve space for new elements.  It's likely the caller knows exactly how many they need, so reserve exact
        // amount.
        result = Reserve(newSize);

        if (result == Result::_Success)
        {
            // Push new elements with default value (newVal).
            while (m_numElements < newSize)
            {
                // Insert new data into the array.
                PAL_PLACEMENT_NEW(m_pData + m_numElements) T(newVal);
                ++(m_numElements);
            }
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
        result = Reserve(m_numElements * GrowthFactor);
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
// Pushes the new element to the end of the vector. If the vector has reached maximum capacity, new space is allocated
// on the heap and the data in the old space is copied over to the new space. The old space is freed if it was also
// allocated on the heap.
template<typename T, uint32 defaultCapacity, typename Allocator>
Result Vector<T, defaultCapacity, Allocator>::PushBack(
    T&& data)
{
    Result result = Result::_Success;

    // Alloc more space if push back requested when current size is at max capacity.
    if (m_numElements == m_maxCapacity)
    {
        result = Reserve(m_numElements * GrowthFactor);
    }

    if (result == Result::_Success)
    {
        // Insert new data into the array.
        PAL_PLACEMENT_NEW(m_pData + m_numElements) T(Move(data));
        ++(m_numElements);
    }

    return result;
}

// =====================================================================================================================
// Constructs a new element at the end of the vector. If the vector has reached maximum capacity, new space is allocated
// on the heap and the data in the old space is copied over to the new space. The old space is freed if it was also
// allocated on the heap.
template<typename T, uint32 defaultCapacity, typename Allocator>
template<typename... Args>
Result Vector<T, defaultCapacity, Allocator>::EmplaceBack(
    Args&&... args)
{
    Result result = Result::_Success;

    // Alloc more space if push back requested when current size is at max capacity.
    if (m_numElements == m_maxCapacity)
    {
        result = Reserve(m_numElements * GrowthFactor);
    }

    if (result == Result::_Success)
    {
        // Insert new data into the array.
        PAL_PLACEMENT_NEW(m_pData + m_numElements) T(std::forward<Args>(args)...);
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
        PAL_PLACEMENT_NEW(pData) T(Move(*(m_pData + m_numElements)));
    }

    // Explicitly destroy the removed value if it's non-trivial.
    if (!std::is_trivial<T>::value)
    {
        m_pData[m_numElements].~T();
    }
}

// =====================================================================================================================
template<typename T, uint32 defaultCapacity, typename Allocator>
void Vector<T, defaultCapacity, Allocator>::Clear()
{
    // Explicitly destroy all non-trivial types.
    if (!std::is_trivial<T>::value)
    {
        for (uint32 idx = 0; idx < m_numElements; ++idx)
        {
            m_pData[idx].~T();
        }
    }

    m_numElements = 0;
}

// =====================================================================================================================
template<typename T, uint32 defaultCapacity, typename Allocator>
void Vector<T, defaultCapacity, Allocator>::Erase(
    Iter it)
{
    PAL_ASSERT(it.IsValid());

    // call the iterator version of the method
    Erase(&it.Get());
}

// =====================================================================================================================
template<typename T, uint32 defaultCapacity, typename Allocator>
void Vector<T, defaultCapacity, Allocator>::Erase(
    iterator it)
{
    PAL_ASSERT(m_pData <= it);
    PAL_ASSERT(it < (m_pData + m_numElements));

    // call the index version of the method
    Erase(it - m_pData);
}

// =====================================================================================================================
template<typename T, uint32 defaultCapacity, typename Allocator>
void Vector<T, defaultCapacity, Allocator>::Erase(
    uint32 index)
{
    PAL_ASSERT(index < m_numElements);

    m_pData[index].~T();
    std::memmove(m_pData + index, m_pData + index + 1, m_numElements - index);
    m_numElements--;
}

} // Util
