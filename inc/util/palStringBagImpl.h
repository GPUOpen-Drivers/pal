/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  palStringBagImpl.h
* @brief PAL utility collection StringBag and StringBagIterator class implementations.
***********************************************************************************************************************
*/

#pragma once

#include "palInlineFuncs.h"
#include "palStringBag.h"
#include "palSysMemory.h"

namespace Util
{

// =====================================================================================================================
// Allocates on the heap new storage for the bag buffer, moves objects from the bag buffer to heap allocation and,
// takes ownership of that allocation.
template<typename T, typename Allocator>
Result StringBag<T, Allocator>::ReserveInternal(
    uint32 newCapacity)
{
    Result result = Result::_Success;

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

        // Free string buffer if it uses storage from heap allocation.
        if (m_pData != nullptr)
        {
            std::memcpy(pNewData, m_pData, sizeof(T) * m_currOffset);

            PAL_FREE(m_pData, m_pAllocator);
        }

        // Take ownership of the heap allocation.
        m_pData       = pNewData;
        m_maxCapacity = newCapacity;
    }

    return result;
}

// =====================================================================================================================
// Pushes a new string to the end of the bag using the specified string view.
template<typename T, typename Allocator>
typename StringBag<T, Allocator>::Handle StringBag<T, Allocator>::PushBack(
    Util::StringView<T> sv,
    Result*             pResult)
{
    // Storage for the string length, the string and the null terminator.
    const StringLengthType stringLength = sv.Length();
    const size_t requiredSize           = RequiredSpace(stringLength);

    if (sv.Data() != nullptr)
    {
        const size_t requiredCapacity = m_currOffset + requiredSize;

        // Alloc more space if push back requested cannot fit in the data buffer.
        if (requiredCapacity > static_cast<size_t>(m_maxCapacity))
        {
            // Allocate an additional amount of memory that is double the required size needed for
            // the current bag plus the incoming string length. Clamp the maximum to the largest
            // possible uint32.
            const size_t newMaxCapacity = Min(requiredCapacity * 2, static_cast<size_t>(~0u));

            // Ensure the new capacity will fit the incoming string.
            if (newMaxCapacity <= requiredCapacity)
            {
                *pResult = Result::ErrorInvalidMemorySize;
            }
            else
            {
                *pResult = ReserveInternal(static_cast<uint32>(newMaxCapacity));
            }
        }
    }
    else
    {
        *pResult = Result::ErrorInvalidPointer;
    }

    Handle handle;
    if (*pResult == Result::_Success)
    {
        // Store the previous offset so that the last string in the bag can be quickly retrieved.
        const uint32 prevOffset = m_currOffset;

        // Store string length.
        auto* pStringLength =
            Util::AssumeAligned<alignof(StringLengthType)>(reinterpret_cast<uint32*>(m_pData + m_currOffset));
        *pStringLength      = stringLength;
        // Copy string.
        std::memcpy((m_pData + m_currOffset + StringLengthSizeof), sv.Data(), sizeof(T) * stringLength);
        // Add terminator.
        m_pData[m_currOffset + StringLengthSizeof + stringLength] = T('\0');

        // Push the current offset one past the end of the copied data, including alignment.
        m_currOffset += uint32(requiredSize);

        handle = GetHandleAt(prevOffset);
    }

    return handle;
}

// =====================================================================================================================
// Pushes a new string to the end of the bag after calculating the length.
template<typename T, typename Allocator>
typename StringBag<T, Allocator>::Handle StringBag<T, Allocator>::PushBack(
    const T* pString,
    Result*  pResult)
{
    return PushBack(Util::StringView<T>{pString}, pResult);
}

// =====================================================================================================================
// Pushes a new string to the end of the bag using the specified length.
template<typename T, typename Allocator>
typename StringBag<T, Allocator>::Handle StringBag<T, Allocator>::PushBack(
    const T* pString,
    uint32   length,
    Result*  pResult)
{
    return PushBack(Util::StringView<T>{pString, length}, pResult);
}

} // Util
