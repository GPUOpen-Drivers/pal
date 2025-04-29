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

#pragma once

#include <ddPlatform.h>
#include <util/vector.h>

namespace DevDriver
{

// Class that simplifies operations on a collection of bit values with configurable storage
template <typename BitSetStorage>
class BitSet
{
public:
    BitSet(const AllocCb& allocCb)
        : m_bits(allocCb)
    {
        ResetAllBits();
    }

    ~BitSet()
    {
    }

    // Returns a pointer to the internal bit data
    const void* Data() const { return m_bits.Dwords(); }

    // Returns the size of the internal bit data in bytes.
    // This is always a multiple of 4.
    size_t SizeInBytes() const { return (m_bits.NumDwords() * sizeof(uint32)); }

    // Returns the number of bits in the set
    size_t SizeInBits() const { return m_bits.NumBits(); }

    // Changes the number of bits in the set
    // NOTE: This may or may not be available depending on the underlying storage type in use.
    Result Resize(size_t numBits) { return m_bits.Resize(numBits); }

    // Updates the internal bit data using the data provided by the caller
    // This effectively just copies the caller's data over the internal data and discards all unnecessary data.
    void UpdateBitData(const void* pBitData, size_t bitDataSize)
    {
        const size_t copySize = Platform::Min(SizeInBytes(), bitDataSize);

        memcpy(m_bits.Dwords(), pBitData, copySize);
    }

    // Queries the value of a bit at the specified index
    // Returns false if the index is out of bounds
    bool QueryBit(size_t bitIndex) const
    {
        bool bitValue = false;

        AbsoluteIndex index;
        const Result result = CalculateIndex(bitIndex, &index);

        if (result == Result::Success)
        {
            bitValue = ((*GetBitDword(index.DwordIndex)) & (1 << index.BitIndex));
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
            (*GetBitDword(index.DwordIndex)) |= (1 << index.BitIndex);
        }
        else
        {
            DD_ASSERT_REASON("Invalid bit index");
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
            (*GetBitDword(index.DwordIndex)) &= (~(1 << index.BitIndex));
        }
        else
        {
            DD_ASSERT_REASON("Invalid bit index");
        }
    }

    // Sets all bits to 1
    void SetAllBits()
    {
        // Un-used bits in our bytes may be used later
        // We want to make sure they stay zeroed
        uint8* pBytes = reinterpret_cast<uint8*>(m_bits.Dwords());

        const size_t fullBytes = SizeInBits() / 8;
        if (fullBytes > 0)
        {
            memset(pBytes, 0xFF, fullBytes);
        }

        const size_t bits = SizeInBits() % 8;
        if (bits > 0)
        {
            pBytes[fullBytes] = (1 << bits) - 1;
        }
    }

    // Sets all bits to 0
    void ResetAllBits()
    {
        // Un-used bits in our bytes may be used later - it's okay if we zero them redundantly here.
        memset(m_bits.Dwords(), 0, SizeInBytes());
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
        if (dwordIndex < m_bits.NumDwords())
        {
            *pIndex = { dwordIndex, localBitIndex };

            result = Result::Success;
        }

        return result;
    }

    uint32* GetBitDword(size_t dwordIndex)
    {
        return &m_bits.Dwords()[dwordIndex];
    }

    const uint32* GetBitDword(size_t dwordIndex) const
    {
        return &m_bits.Dwords()[dwordIndex];
    }

    BitSetStorage m_bits;
};

// Bit storage class that allows an application to manage a compile-time-sized collection of bit values.
template <size_t NumStorageBits>
class FixedBitStorage
{
public:
    FixedBitStorage(const AllocCb& allocCb)
    {
        // We don't use this, but it has to be in the parameter list in order to support dynamic bit sets
        DD_UNUSED(allocCb);
    }

    // Returns a pointer to the internal bit data
    uint32* Dwords() { return m_bitDwords; }

    // Returns a pointer to the internal bit data
    const uint32* Dwords() const { return m_bitDwords; }

    // Returns the size of the internal bit data in dwords
    size_t NumDwords() const { return Platform::ArraySize(m_bitDwords); }

    // Returns the number of bits in the set
    size_t NumBits() const { return NumStorageBits; }

    // Changes the number of bits in the set
    Result Resize(size_t numBits)
    {
        DD_UNUSED(numBits);
        DD_ASSERT_REASON("Resize called on a fixed-sized storage. If you need this, use DynamicBitSet instead");

        // Fixed bitsets cannot change the number of bits at runtime
        return Result::Unavailable;
    }

private:
    uint32 m_bitDwords[(Platform::Pow2Align<size_t>(NumStorageBits, 32) / 32)];
};

// We purposely prevent people from building a bit set with zero bits in it.
template <>
class FixedBitStorage<0>
{
public:
    FixedBitStorage(const AllocCb& allocCb) = delete;
};

// Bit storage class that allows an application to manage a runtime-sized collection of bit values.
template <size_t NumStorageBits = 256>
class DynamicBitStorage
{
public:
    DynamicBitStorage(const AllocCb& allocCb)
        : m_allocCb(allocCb)
        , m_bitDwords(allocCb)
        , m_numBits(NumStorageBits)
    {
        // Resize our storage to the requested number of bits
        Resize(NumStorageBits);
    }

    ~DynamicBitStorage() {}

    // Returns a pointer to the internal bit data
    uint32* Dwords() { return m_bitDwords.Data(); }

    // Returns a pointer to the internal bit data
    const uint32* Dwords() const { return m_bitDwords.Data(); }

    // Returns the size of the internal bit data in dwords
    size_t NumDwords() const { return m_bitDwords.Size(); }

    // Returns the number of bits in the set
    size_t NumBits() const { return m_numBits; }

    // Changes the number of bits in the set
    Result Resize(size_t numBits)
    {
        const size_t numDwords = (Platform::Pow2Align<size_t>(numBits, 32) / 32);

        m_bitDwords.ResizeAndZero(numDwords);
        m_numBits = numBits;

        // @TODO: Propagate the correct result from Vector::Resize() when it's updated to return results
        return Result::Success;
    }

private:
    DD_DISALLOW_COPY_AND_ASSIGN(DynamicBitStorage);

    using DwordsVector = Vector<uint32, (Platform::Pow2Align<size_t>(NumStorageBits, 32) / 32)>;

    AllocCb      m_allocCb;
    DwordsVector m_bitDwords;
    size_t       m_numBits;
};

// We purposely prevent people from building a bit set with zero bits in it.
template <>
class DynamicBitStorage<0>
{
public:
    DynamicBitStorage(const AllocCb& allocCb) = delete;
};

// Bit set that allows an application to manage a compile-time-sized collection of bit values.
template<size_t NumOfBits>
using FixedBitSet = BitSet<FixedBitStorage<NumOfBits>>;

// Bit set that allows an application to manage a runtime-sized collection of bit values.
template <size_t InitBitCapacity = 256>
using DynamicBitSet = BitSet<DynamicBitStorage<InitBitCapacity>>;

} // DevDriver
