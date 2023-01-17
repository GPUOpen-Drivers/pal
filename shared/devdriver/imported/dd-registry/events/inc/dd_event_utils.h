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

#pragma once

#include <stdint.h>

#ifndef DDEVENT_ASSERT
    #ifdef DDEVENT_ASSERT_ENABLED
        #include <assert.h>
        #define DDEVENT_ASSERT(condition) assert(condition)
    #else
        #define DDEVENT_ASSERT(condition)
    #endif
#endif

/// Copy the bits from `srcValue` into the bits delineated by `startBit` and
/// `endBit` of the buffer pointed to by `pBuffer`. The bits copied from
/// `srcValue` ranges from 0 to `endBits - startBits - 1`.
///
/// `startBit` is the starting bit of the buffer for copying. \
/// `endBit` is the one bit pass the last bit for copying.
///
/// Caller must make ensure buffer is big enough to store bits from `startBit`
/// to `endBit - 1`.
void DDEventSetBits(uint8_t* pBuffer, uint32_t startBit, uint32_t endBit, uint64_t srcValue)
{
    DDEVENT_ASSERT(pBuffer != nullptr);

    const uint32_t startByte = startBit / 8;
    const uint32_t startByteBits = 8 - (startBit % 8);
    const uint64_t startByteMask = ((1 << startByteBits) - 1);

    const uint32_t endByte = endBit / 8;
    const uint32_t endByteBits = (endBit % 8) + 1;
    const uint64_t endByteMask = (1 << endByteBits) - 1;

    const int32_t numBytes = endByte - startByte + 1;

    const uint32_t numBits = endBit - startBit + 1;
    const uint64_t maxSrcVal = (1ull << numBits) - 1;
    DDEVENT_ASSERT(numBits <= 64);
    DDEVENT_ASSERT((numBits == 64) || (srcVal <= maxSrcVal));
    (void)maxSrcVal;

    uint32_t totalBitsCopied = 0;
    for (int i = 0; i < numBytes; ++i)
    {
        int dstIdx = (startByte + i);
        uint64_t srcMask = 0xFF;
        uint32_t srcShift = 0;
        uint32_t bits = 8;
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

        uint8_t srcByte = static_cast<uint8_t>((srcValue >> totalBitsCopied) & srcMask) << srcShift;

        uint8_t dstByte = pBuffer[dstIdx];

        uint64_t dstMask = ~(srcMask << srcShift);
        dstByte &= dstMask;

        dstByte = dstByte | srcByte;

        pBuffer[dstIdx] = dstByte;

        totalBitsCopied += bits;
    }
}

/// Copy the bits ranging from `startBit` to `endBit` of the buffer pointed
/// to by `pBuffer`, to a 64-bit value pointed to by `dstValue`.
///
/// `startBit` is the starting bit of the buffer for copying. \
/// `endBit` is the one bit pass the last bit for copying.
///
/// Caller must make ensure buffer is big enough to access bits from `startBit`
/// to `endBit - 1`.
void DDEventGetBits(uint8_t* pBuffer, uint32_t startBit, uint32_t endBit, uint64_t* pDstValue)
{
    DDEVENT_ASSERT(pDstVal != nullptr);
    DDEVENT_ASSERT(pByteData != nullptr);

    const uint32_t startByte = startBit / 8;
    const uint32_t startByteBits = 8 - (startBit % 8);
    const uint64_t startByteMask = ((1 << startByteBits) - 1);

    const uint32_t endByte = endBit / 8;
    const uint32_t endByteBits = (endBit % 8) + 1;
    const uint64_t endByteMask = (1 << endByteBits) - 1;

    const int32_t numBytes = endByte - startByte + 1;
    const uint32_t numBits = endBit - startBit + 1;
    DDEVENT_ASSERT(numBits <= 64);
    (void)numBits;

    uint64_t dstVal = 0;

    uint32_t totalBitsCopied = 0;
    for (int32_t i = 0; i < numBytes; ++i)
    {
        int32_t srcIdx = (startByte + i);
        uint64_t srcMask = 0xFF;
        uint32_t srcShift = 0;
        uint32_t bits = 8;
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
        uint32_t srcByte = pBuffer[srcIdx];

        // Mask off the target bits, in most cases this will be all of them but for the first
        // or last byte it may be less
        srcByte = (srcByte >> srcShift) & srcMask;

        dstVal |= (static_cast<uint64_t>(srcByte) << totalBitsCopied);

        totalBitsCopied += bits;
    }

    *pDstValue = dstVal;
}
