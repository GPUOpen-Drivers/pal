/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  palSparseVector.h
* @brief PAL utility collection SparseVector class declaration.
***********************************************************************************************************************
*/

#pragma once

#include "palSysMemory.h"
#include "palInlineFuncs.h"

namespace Util
{

/**
 ***********************************************************************************************************************
 * @brief SparseVector container.
 *
 * SparseVector is a templated array-based storage optimized for memory usage, where keys are expected to fall within a
 * specific range or set of ranges. It starts with an internal default-size allocation, resorting to dynamic allocation
 * if insertion of a new element exceeds the default capacity.
 *
 * State about whether a specific key's entry exists is stored in a bitset, and the values are stored in an array that
 * is sorted such that the first enabled bit in the key bitset is tied to the first element of the value array, second
 * enabled key bit is tied to the second element, etc.
 *
 * In addition to providing constant-time random access, you can efficiently associate a range of keys with the same
 * value via use of LowerBound() or UpperBound() to access nearest neighbor elements in the container.
 *
 * To guarantee best-case performance when inserting into the container, keys should be inserted in sorted order.
 *
 * This container's operations would be suboptimal for non-POD types, and thus they are not supported.
 *
 * Operations which this class supports are:
 *
 * - Random insertion.        (O(n) average, O(1) best case)
 * - Random access.           (O(1))
 * - Nearest neighbor access. (O(1))
 *
 * @warning This class is not thread-safe.
 ***********************************************************************************************************************
*/
template <typename      T,
          typename      CapacityType,
          CapacityType  DefaultCapacity,
          typename      Allocator,
          uint32...     KeyRanges>       ///< This variadic template argument must come in [begin, end] pairs.
class SparseVector
{
static_assert(std::is_trivial_v<T>, "SparseVector only supports trivial types.");
static_assert(((sizeof...(KeyRanges) >= 2) && ((sizeof...(KeyRanges) & 1) == 0)), "KeyRanges must come in pairs.");

public:
    /// Constructor.
    explicit SparseVector(Allocator*const pAllocator)
        :
        m_pData(&m_localData[0]),
        m_pAllocator(pAllocator),
        m_capacity(DefaultCapacity)
    {
        memset(&m_hasEntry[0], 0, sizeof(m_hasEntry));
        memset(&m_accumPop[0], 0, sizeof(m_accumPop));
    }

    /// Destructor.
    ~SparseVector()
    {
        if (m_pData != &m_localData[0])
        {
            // Destroy dynamically-allocated array.
            PAL_FREE(m_pData, m_pAllocator);
        }
    }

    /// Reserves space in the container.
    ///
    /// @param [in] requiredCapacity  Required capacity for the container given at runtime. If this exceeds
    ///                               @ref DefaultCapacity, a larger buffer will by dynamically allocated.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if dynamic allocation failed, ErrorInvalidValue if
    ///          @ref requestedCapacity exceeds @ref MaxCapacity.
    Result Reserve(uint32 requiredCapacity);

    /// Associates a key with the given value.
    ///
    /// @param [in] key    Key of the new entry to insert.
    /// @param [in] value  Value of the new entry to insert.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if insertion requires reserving more space and dynamic
    ///          allocation failed, ErrorInvalidValue if insertion requires reserving more space over @ref MaxCapacity.
    Result Insert(uint32 key, T value);

    /// Removes an entry from the container.
    void Erase(uint32 key);

    /// Empty the container.
    void Clear()
    {
        memset(&m_hasEntry[0], 0, sizeof(m_hasEntry));
        memset(&m_accumPop[0], 0, sizeof(m_accumPop));
    }

    /// Returns the element associated with the given key.
    ///
    /// @param [in] key  Key to query.
    const T& At(uint32 key) const;

    /// Returns the element associated with either the given key, or if it's inactive, the nearest active key before it.
    /// If the queried key is lesser than that of the first element, returns the first element.
    ///
    /// @warning Calling this function when the container is empty is undefined behavior.
    ///
    /// @param [in] key  Key to query.
    const T& LowerBound(uint32 key) const;

    /// Returns the element associated with either the given key, or if it's inactive, the next active key after it.
    /// If the queried key is greater than that of the last element, returns the last element.
    ///
    /// @warning Calling this function when the container is empty is undefined behavior.
    ///
    /// @param [in] key  Key to query.
    const T& UpperBound(uint32 key) const;

    /// Returns the number of elements currently present in the container.
    CapacityType NumElements() const { return m_accumPop[NumBitsetChunks - 1]; }

    /// Returns if the specified key is active in the container.
    ///
    /// @param [in] key  Key to query.
    bool HasEntry(uint32 key) const { return WideBitfieldIsSet(m_hasEntry, GetKeyIndex<0, KeyRanges...>(key)); }

    /// Returns if the specified key is active in the container, and if so, returns its associated element through an
    /// output parameter.
    ///
    /// @param [in]  key     Key to query.
    /// @param [out] pValue  Pointer to where to store the extracted value.
    bool HasEntry(uint32 key, T* pValue) const;

private:
    ///@{
    /// Helper functions to index into the data buffer and the has entry bitset.
    template <uint32 Offset>
    static uint32 GetKeyIndex(uint32 key)
    {
        PAL_ASSERT_ALWAYS();
        return Offset;
    }

    template <uint32 Offset, uint32 Begin, uint32 End, uint32... MoreRanges>
    static uint32 GetKeyIndex(uint32 key)
    {
        return ((key >= Begin) && (key <= End)) ? ((key - Begin) + Offset)
                                                : GetKeyIndex<(Offset + (End - Begin) + 1), MoreRanges...>(key);
    }

    static uint32 GetChunkIndex(uint32 keyIndex) { return (keyIndex / (sizeof(m_hasEntry[0]) << 3)); }
    static uint64 GetChunkMask(uint32 keyIndex)  { return (1ull << (keyIndex & ((sizeof(m_hasEntry[0]) << 3) - 1))); }
    ///@}

    ///@{
    /// Helper function to calculate the number of elements a set of ranges encompasses.
    static constexpr size_t CalcNumKeys() { return 0; }

    template <typename... Ts>
    static constexpr size_t CalcNumKeys(uint32 begin, uint32 end, Ts... moreRanges)
        { return ((end - begin) + 1 + CalcNumKeys(moreRanges...)); }
    ///@}

    static constexpr size_t NumBitsetChunks = RoundUpQuotient(CalcNumKeys(KeyRanges...), (sizeof(uint64) * 8));

    T                m_localData[DefaultCapacity];  // The initial data buffer stored within the SparseVector object.
    T*               m_pData;                       // Pointer to the current data buffer.
    Allocator*const  m_pAllocator;                  // Allocator for this SparseVector.
    uint64           m_hasEntry[NumBitsetChunks];   // Bitset indicating which keys are present.
    CapacityType     m_accumPop[NumBitsetChunks];   // Accumulated population counts of the m_hasEntry bitset.
                                                    // [0] = bits 0..63, [1] = 0..127, [2] = 0..191, [3] = 0..255, etc.
    CapacityType     m_capacity;                    // Number of elements the current data buffer can hold.

    PAL_DISALLOW_COPY_AND_ASSIGN(SparseVector);
};

} // Util
