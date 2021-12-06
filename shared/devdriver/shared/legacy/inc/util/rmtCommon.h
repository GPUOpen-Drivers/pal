/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace DevDriver
{
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT Types and Helper Functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct RMT_TOKEN_DATA
{
    uint8* pByteData;
    size_t sizeInBytes;

    size_t Size() const { return sizeInBytes; }

    const void* Data() const { return pByteData; }

    // Set the specified bits in the provided destination data buffer, up to 64 bits.
    void SetBits(uint64 srcVal, uint32 endBit, uint32 startBit)
    {
        DD_ASSERT(pByteData != nullptr);

        const uint32 startByte = startBit / 8;
        const uint32 startByteBits = 8 - (startBit % 8);
        const uint64 startByteMask = ((1 << startByteBits) - 1);

        const uint32 endByte = endBit / 8;
        const uint32 endByteBits = (endBit % 8) + 1;
        const uint64 endByteMask = (1 << endByteBits) - 1;

        const int32 numBytes = endByte - startByte + 1;

        const uint32 numBits = endBit - startBit + 1;
        const uint64 maxSrcVal = (1ull << numBits) - 1;
        DD_ASSERT(numBits <= 64);
        DD_ASSERT((numBits == 64) || (srcVal <= maxSrcVal));
        DD_UNUSED(maxSrcVal);

        if ((startByte + numBytes) <= static_cast<uint32>(sizeInBytes))
        {
            uint32 totalBitsCopied = 0;
            for (int i = 0; i < numBytes; ++i)
            {
                int dstIdx = (startByte + i);
                uint64 srcMask = 0xFF;
                uint32 srcShift = 0;
                uint32 bits = 8;
                if (i == 0)
                {
                    srcMask = startByteMask;
                    bits = startByteBits;
                    srcShift = 8 - bits;
                }
                else if (i == (numBytes - 1))
                {
                    srcMask = endByteMask;
                    bits = endByteBits;
                }

                uint8 srcByte = static_cast<uint8>((srcVal >> totalBitsCopied) & srcMask) << srcShift;

                uint8 dstByte = pByteData[dstIdx];

                uint64 dstMask = ~(srcMask << srcShift);
                dstByte &= dstMask;

                dstByte = dstByte | srcByte;

                pByteData[dstIdx] = dstByte;

                totalBitsCopied += bits;
            }
        }
        else
        {
            DD_ASSERT_REASON("SetBits would overrun destination.");
        }
    }

    // Get the specified bits from the provided source data, up to 64 bits.
    // NOTE: pDstVal will be cleared prior to the copy, any existing data will be lost.
    void GetBits(uint64* pDstVal, uint32 endBit, uint32 startBit)
    {
        DD_ASSERT(pDstVal != nullptr);
        DD_ASSERT(pByteData != nullptr);

        const uint32 startByte = startBit / 8;
        const uint32 startByteBits = 8 - (startBit % 8);
        const uint64 startByteMask = ((1 << startByteBits) - 1);

        const uint32 endByte = endBit / 8;
        const uint32 endByteBits = (endBit % 8) + 1;
        const uint64 endByteMask = (1 << endByteBits) - 1;

        const int32 numBytes = endByte - startByte + 1;
        const uint32 numBits = endBit - startBit + 1;
        DD_ASSERT(numBits <= 64);
        DD_UNUSED(numBits);

        if ((startByte + numBytes) <= static_cast<uint32_t>(sizeInBytes))
        {
            uint64 dstVal = 0;

            uint32 totalBitsCopied = 0;
            for (int32 i = 0; i < numBytes; ++i)
            {
                int32 srcIdx = (startByte + i);
                uint64 srcMask = 0xFF;
                uint32 srcShift = 0;
                uint32 bits = 8;
                if (i == 0)
                {
                    srcMask = startByteMask;
                    bits = startByteBits;
                    srcShift = 8 - bits;
                }
                else if (i == (numBytes - 1))
                {
                    srcMask = endByteMask;
                    bits = endByteBits;
                }

                // Get the source byte
                uint32 srcByte = pByteData[srcIdx];

                // Mask off the target bits, in most cases this will be all of them but for the first
                // or last byte it may be less
                srcByte = (srcByte >> srcShift) & srcMask;

                dstVal |= (static_cast<uint64>(srcByte) << totalBitsCopied);

                totalBitsCopied += bits;
            }

            *pDstVal = dstVal;
        }
        else
        {
            DD_ASSERT_REASON("Get would overrun source.");
        }
    }
};

// Enumeration encoding the PAGE_SIZE fields
enum RMT_PAGE_SIZE
{
    RMT_PAGE_SIZE_UNMAPPED = 0,
    RMT_PAGE_SIZE_4KB      = 1,
    RMT_PAGE_SIZE_64KB     = 2,
    RMT_PAGE_SIZE_256KB    = 3,
    RMT_PAGE_SIZE_1MB      = 4,
    RMT_PAGE_SIZE_2MB      = 5,
};

// Enumeration of RMT heap types
enum RMT_HEAP_TYPE
{
    RMT_HEAP_TYPE_LOCAL          = 0,
    RMT_HEAP_TYPE_INVISIBLE      = 1,
    RMT_HEAP_TYPE_SYSTEM         = 2,

    RMT_HEAP_TYPE_GART_USWC      = 2, // Deprecated (Use RMT_HEAP_TYPE_SYSTEM instead)
    RMT_HEAP_TYPE_GART_CACHEABLE = 2, // Deprecated (Use RMT_HEAP_TYPE_SYSTEM instead)
};

inline RMT_PAGE_SIZE GetRmtPageSize(uint64 pageSize)
{
    switch (pageSize)
    {
        case (4 * 1024):        return RMT_PAGE_SIZE_4KB;
        case (64 * 1024):       return RMT_PAGE_SIZE_64KB;
        case (256 * 1024):      return RMT_PAGE_SIZE_256KB;
        case (1024 * 1024):     return RMT_PAGE_SIZE_1MB;
        case (2 * 1024 * 1024): return RMT_PAGE_SIZE_2MB;
        default:
            DD_ASSERT_REASON("Unexpected Page Size\n");
            break;
    }

    return RMT_PAGE_SIZE_UNMAPPED;
}

} // namespace DevDriver
