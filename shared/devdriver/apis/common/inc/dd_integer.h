/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <dd_common_api.h>
#include <dd_assert.h>
#include <cstdint>

namespace DevDriver
{

template<typename T>
uint32_t SafeCastToU32(T x)
{
    DD_ASSERT((x >= 0) && (x <= UINT32_MAX));
    return static_cast<uint32_t>(x);
}

template<typename T>
uint16_t SafeCastToU16(T x)
{
    DD_ASSERT((x >= 0) && (x <= UINT16_MAX));
    return static_cast<uint16_t>(x);
}

/// Find the smallest power of 2 that's greater than or equal to `x`.
/// Zero is returned if:
/// 1) `x` is 0
/// 2) the operation causes integer overflow
inline uint32_t NextSmallestPow2(uint32_t x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}

/// Align a 32-bit integer to be multiples of `alignment`. `alignment` must be a power of 2.
/// Return 0 if either:
/// 1) \param x is 0
/// 2) \param alignment is 0
/// 3) the operation causes integer overflow.
inline uint32_t AlignU32(uint32_t x, uint32_t alignment)
{
    DD_ASSERT((alignment & (alignment - 1)) == 0);
    uint32_t aligned = (x + (alignment - 1)) & (~(alignment - 1));
    return aligned;
}

/// Similar to `AlignU32` but for 64-bit integers.
inline uint64_t AlignU64(uint64_t x, uint64_t alignment)
{
    DD_ASSERT((alignment & (alignment - 1)) == 0);
    uint64_t aligned = (x + (alignment - 1)) & (~(alignment - 1));
    return aligned;
}

} // namespace DevDriver
