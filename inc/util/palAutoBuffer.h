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
 * @file  palAutoBuffer.h
 * @brief PAL utility collection AutoBuffer class definition.
 ***********************************************************************************************************************
 */

#pragma once

#include "palSysMemory.h"

namespace Util
{

/**
 ***********************************************************************************************************************
 * @brief Safe version of C99's variable-length arrays.
 *
 * The general idea is that this class encapsulates a variable-length array where we expect the size required to not
 * exceed the 'defaultCapacity' template parameter most of the time.  In those "normal" cases, this buffer will
 * reference a static array of size 'defaultCapacity', but if the constructor's parameter exceeds defaultCapacity, then
 * a dynamic array will be allocated from the heap to satisfy the space requirements.  The destructor will clean-up any
 * dynamic allocation made by the constructor.
 *
 * This class violates several PAL coding conventions, but for good reason:
 *
 * - We have overloaded the [] (array-element-accessor) operator to make using this class just like using a regular
 *   array, which it semantically represents.
 * - In order to return array elements by-reference instead of by-value, we need to use C++ references in the
 *   overloaded operators because this is required by C++.
 *
 * This class __does not__ clear the contents of the static or dynamic arrays, for performance reasons.  If a client
 * needs the buffer to be cleared, it must do the memset itself.  (However, if 'Item' is a class type rather than
 * plain-old-data, the default c'tor will be invoked.)
 ***********************************************************************************************************************
 */
template<typename Item, size_t defaultCapacity, typename Allocator>
class AutoBuffer
{
public:
    /// Constructor.
    ///
    /// The object is initialized to use the static array of items if the required capacity is less than or equal to the
    /// default capacity.  Otherwise, a larger array is allocated on the heap.
    ///
    /// @param [in] requiredCapacity Number of items actually required (unknown until runtime).
    /// @param [in] pAllocator       The allocator that will allocate memory if required.
    AutoBuffer(
        size_t           requiredCapacity,
        Allocator*const  pAllocator)
        :
        m_capacity(defaultCapacity),
        m_pBuffer(reinterpret_cast<Item*>(m_localBuffer)),
        m_pAllocator(pAllocator)
    {
        if (requiredCapacity > defaultCapacity)
        {
            PAL_DPWARN("AutoBuffer needs to malloc. Consider increasing the default size. Default: %u, Required: %u",
                       static_cast<uint32>(defaultCapacity), static_cast<uint32>(requiredCapacity));

            // Create dynamically allocated array, by allocating memory and construcing its objects.
            Item*const pBuffer = PAL_NEW_ARRAY(Item, requiredCapacity, pAllocator, AllocInternalTemp);

            if (pBuffer != nullptr)
            {
                m_pBuffer  = pBuffer;
                m_capacity = requiredCapacity;
            }
        }
        else if (!std::is_trivial<Item>::value)
        {
            // Explicitly construct all objects of non-trivial type in the local buffer.
            for (uint32 idx = 0; idx < defaultCapacity; ++idx)
            {
                PAL_PLACEMENT_NEW(m_pBuffer + idx) Item();
            }
        }

        // Alert if the local buffer's capacity was insufficient and the dynamic buffer's allocation failed.
        PAL_ALERT(m_capacity < requiredCapacity);
    }

    /// Destructor.
    ///
    /// Cleans up the dynamically allocated buffer if we allocated one.
    ~AutoBuffer()
    {
        if (m_pBuffer != reinterpret_cast<Item*>(m_localBuffer))
        {
            // Destory dynamically allocated array, by destroying its objects and freeing memory.
            PAL_SAFE_DELETE_ARRAY(m_pBuffer, m_pAllocator);
        }
        else if (!std::is_trivial<Item>::value)
        {
            // Explicitly destroy all objects of non-trivial type from the local buffer.
            for (uint32 idx = 0; idx < defaultCapacity; ++idx)
            {
                m_pBuffer[idx].~Item();
            }
        }
    }

    /// Getter for the current capacity of the buffer.
    ///
    /// Clients can use this function to determine if the constuctor's allocation succeeded.
    ///
    /// @returns Size of the array in bytes.  Should match the requiredCapacity parameter passed to the constructor
    ///          unless a dynamic memory allocation failed.
    size_t Capacity() const { return m_capacity; }

    /// Getter for the current size of this buffer, in bytes.
    size_t SizeBytes() const { return (sizeof(Item) * m_capacity); }

    /// Accessor for the nth element of this buffer.
    const Item& operator[](size_t n) const
    {
        PAL_ASSERT(n < m_capacity);
        return m_pBuffer[n];
    }

    /// Non-const accessor for the nth element of this buffer.
    Item& operator[](size_t n)
    {
        PAL_ASSERT(n < m_capacity);
        return m_pBuffer[n];
    }

    /// Returns pointer to the underlying buffer serving as data storage.
    /// The returned pointer defines always valid range [Data(), Data() + Capacity()).
    ///
    /// @returns Pointer to the underlying data storage for read & write access.
    ///          The returned pointer contains address of the first element.
    Item* Data() { return m_pBuffer; }

    /// Returns pointer to the underlying buffer serving as data storage.
    /// The returned pointer defines always valid range [Data(), Data() + Capacity()),
    /// even if the container is empty (Data() is not dereferenceable in that case).
    ///
    /// @returns Pointer to the underlying data storage for read only access.
    ///          The returned pointer contains address of the first element.
    const Item* Data() const { return m_pBuffer; }

private:
    // This is a POD-type that exactly fits one Item value.
    using ValueStorage = typename std::aligned_storage<sizeof(Item), alignof(Item)>::type;

    // Current capacity of this buffer (in Items).
    size_t m_capacity;

    // Buffer pointer this object uses to access the buffer's elements: if the required capacity exceeds the default
    // capacity, this points to a dynamic array of Items. Otherwise, this points to m_localBuffer.
    Item*  m_pBuffer;

    // Static array providing storage for Items which we expect most objects of this type to end up using.
    ValueStorage m_localBuffer[defaultCapacity];

    // Allocator for this AutoBuffer.
    Allocator*const m_pAllocator;

    PAL_DISALLOW_DEFAULT_CTOR(AutoBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(AutoBuffer);
};

} // Util
