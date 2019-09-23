/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
  * @file  ddBitSet.h
  * @brief Class to encapsulate bit operations on an array of bits.
  ***********************************************************************************************************************
  */

#pragma once

#include <ddPlatform.h>

namespace DevDriver
{

// Class that simplifies operations on a collection of bit values
template <size_t NumBits>
class BitSet
{
public:
    BitSet() { ResetBits(); }
    ~BitSet() {}

    // Returns a pointer to the internal bit data
    const void* GetBitData() const { return m_bitDwords; }

    // Returns the size of the internal bit data in bytes
    size_t GetBitDataSize() const { return sizeof(m_bitDwords); }

    // Updates the internal bit data using the data provided by the caller
    // This effectively just copies the caller's data over the internal data and discards all unnecessary data.
    void UpdateBitData(const void* pBitData, size_t bitDataSize)
    {
        const size_t copySize = Platform::Min(sizeof(m_bitDwords), bitDataSize);

        memcpy(m_bitDwords, pBitData, copySize);
    }

    // Returns the number of bits in the set
    size_t GetNumBits() const { return NumBits; }

    // Queries the value of a bit at the specified index
    // Returns false if the index is out of bounds
    bool QueryBit(size_t bitIndex) const
    {
        bool bitValue = false;

        AbsoluteIndex index;
        const Result result = CalculateIndex(bitIndex, &index);

        if (result == Result::Success)
        {
            bitValue = (m_bitDwords[index.DwordIndex] & (1 << index.BitIndex));
        }

        return bitValue;
    }

    // Convenience operator that forwards to QueryBit
    bool operator[](size_t bitIndex) const
    {
        return QueryBit(bitIndex);
    }

    // Sets a bit at the specified index to 1
    // Does nothing if the index is out of bounds
    void SetBit(size_t bitIndex)
    {
        AbsoluteIndex index;
        const Result result = CalculateIndex(bitIndex, &index);

        if (result == Result::Success)
        {
            m_bitDwords[index.DwordIndex] |= (1 << index.BitIndex);
        }
    }

    // Sets a bit at the specified index to 0
    // Does nothing if the index is out of bounds
    void ResetBit(size_t bitIndex)
    {
        AbsoluteIndex index;
        const Result result = CalculateIndex(bitIndex, &index);

        if (result == Result::Success)
        {
            m_bitDwords[index.DwordIndex] &= (~(1 << index.BitIndex));
        }
    }

    // Sets all bits to 1
    void SetBits()
    {
        memset(m_bitDwords, 0xFFFFFFFF, sizeof(m_bitDwords));
    }

    // Sets all bits to 0
    void ResetBits()
    {
        memset(m_bitDwords, 0, sizeof(m_bitDwords));
    }

private:
    DD_DISALLOW_COPY_AND_ASSIGN(BitSet);

    struct AbsoluteIndex
    {
        size_t DwordIndex;
        uint32 BitIndex;
    };

    // Helper function to locate the correct data in the bit set
    Result CalculateIndex(size_t bitIndex, AbsoluteIndex* pIndex) const
    {
        DD_ASSERT(pIndex != nullptr);

        Result result = Result::InvalidParameter;

        const size_t dwordIndex = (bitIndex >> 5);
        const uint32 localBitIndex = (bitIndex & 31);

        // Make sure we're in bounds
        if (dwordIndex < Platform::ArraySize(m_bitDwords))
        {
            *pIndex = { dwordIndex, localBitIndex };

            result = Result::Success;
        }

        return result;
    }

    uint32 m_bitDwords[(Platform::Pow2Align<size_t>(NumBits, 32) >> 5)];
};

// We purposely prevent people from building a bit set with zero bits in it.
template <>
class BitSet<0>
{
public:
    BitSet() = delete;
};

} // DevDriver
