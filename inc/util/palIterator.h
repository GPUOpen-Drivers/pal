/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palDequeImpl.h
 * @brief Unspecific helper iterators in PAL utility collection.
 ***********************************************************************************************************************
 */

#pragma once

#include "palInlineFuncs.h"
#include <iterator>

namespace Util
{

/// A helper class to scan through bits in an integer.
///
/// This is used as an iterator and returns the index of each set bit from LSB to MSB.
///
/// Eg:
/// ```cpp
/// for (uint32 bit : BitIter32(0x28)) // 3rd and 5th bits set
/// {
///     somevec.push(bit);
/// }
/// ASSERT_EQ(somevec, {3, 5});
/// ```
///
/// @note ResultShift returns each value in N bits (no deduping) and is considered an advanced usage,
///       mainly used with bitmasks generated from subdividing ints in a SIMD-like fashion.
template <typename T, uint32 ResultShift=0>
class BitIter
{
public:
    static_assert(std::is_unsigned<T>::value, "Must use unsigned ints for BitIter");

    /// Constructs a BitIter from the bits in the given value.
    BitIter(T val) : m_val(val) {}

    /// Gets the full integer backing this BitIter.
    T BackingValue() const { return m_val; }

    /// Gets the current bit value.
    uint32 Get() const
    {
        PAL_ASSERT(m_val != 0);
        uint32 idx = 0;
        BitMaskScanForward(&idx, m_val);
        return idx >> ResultShift;
    }

    /// Increments the iterator
    void Next() { m_val = UnsetLeastBit(m_val); }

    /// Returns whether the current iterator is valid (bits remaining)
    bool IsValid() const { return m_val != 0; }

    /// Returns the number of elements (set bits) in this iterator
    uint32 Size() const { return CountSetBits(m_val); }

    // Boilerplate to fit C++'s definition of "InputIterator" (eg, for for-range loops and <algorithm>)
    using iterator = BitIter;
    using const_iterator = BitIter;
    using iterator_category = std::input_iterator_tag;
    using difference_type = int32;
    using value_type = uint32;
    using pointer = void;
    using reference = uint32;

    BitIter& operator++()
    {
        // Pre-increment
        Next();
        return *this;
    }
    BitIter operator++(int)
    {
        // Post-increment
        BitIter other(this);
        Next();
        return other;
    }
    uint32 operator*() const { return Get(); }

    BitIter            begin() const { return *this; }
    BitIter            end()   const { return BitIter(0); }
    [[nodiscard]] bool empty() const { return m_val == 0; }
    uint32             size()  const { return CountSetBits(m_val); }

    friend bool operator==(const BitIter& a, const BitIter& b) { return a.m_val == b.m_val; }
    friend bool operator!=(const BitIter& a, const BitIter& b) { return a.m_val != b.m_val; }
private:
    T m_val;
};

using BitIter32 = BitIter<uint32>;
using BitIter64 = BitIter<uint64>;
using BitIterSizeT = BitIter<size_t>;

/// A helper class to scan through bits in an wide integer.
///
/// This is used as an iterator and returns the index of each set bit from LSB to MSB.
///
/// Eg:
/// ```cpp
/// for (uint32 bit : BitIter32(0x28)) // 3rd and 5th bits set
/// {
///     somevec.push(bit);
/// }
/// ASSERT_EQ(somevec, {3, 5});
/// ```
///
/// @note ResultShift returns each value in N bits (no deduping) and is considered an advanced usage,
///       mainly used with bitmasks generated from subdividing ints in a SIMD-like fashion.
template <typename T, size_t N, uint32 ResultShift=0>
class WideBitIter
{
public:
    static_assert(std::is_unsigned<T>::value, "Must use unsigned ints for WideBitIter");

    /// Constructs a WideBitIter from the bits in the given value.
    WideBitIter(T (&val)[N]) : m_index(0) { memcpy(&m_val, val, sizeof(m_val)); }

    /// Gets the current bit value.
    uint32 Get()
    {
        PAL_ASSERT(IsValid());
        WideBitMaskScanForward<T, N>(&m_index, m_val);
        return m_index >> ResultShift;
    }

    /// Increments the iterator
    void Next() { WideBitfieldClearBit(m_val, m_index); }

    /// Returns whether the current iterator is valid (bits remaining)
    bool IsValid() const { return WideBitfieldIsAnyBitSet(m_val); }

    /// Returns the number of elements (set bits) in this iterator
    uint32 Size() const { return WideBitfieldCountSetBits(m_val); }

    // Boilerplate to fit C++'s definition of "InputIterator" (eg, for for-range loops and <algorithm>)
    using iterator          = WideBitIter;
    using const_iterator    = WideBitIter;
    using iterator_category = std::input_iterator_tag;
    using difference_type   = int32;
    using value_type        = uint32;
    using pointer           = void;
    using reference         = uint32;

    WideBitIter& operator++()
    {
        // Pre-increment
        Next();
        return *this;
    }
    WideBitIter operator++(int)
    {
        // Post-increment
        WideBitIter other(this);
        Next();
        return other;
    }
    uint32 operator*() const { return Get(); }

    WideBitIter        begin() const { return *this; }
    WideBitIter        end()   const { return WideBitIter(0); }
    [[nodiscard]] bool empty() const { return m_val == 0; }
    WideBitIter        size()  const { return WideBitfieldCountSetBits(m_val); }

    friend bool operator == (const WideBitIter& a, const WideBitIter& b) { return a.m_val == b.m_val; }
    friend bool operator != (const WideBitIter& a, const WideBitIter& b) { return a.m_val != b.m_val; }
private:
    uint32 m_index;
    T      m_val[N];
};

} // Util
