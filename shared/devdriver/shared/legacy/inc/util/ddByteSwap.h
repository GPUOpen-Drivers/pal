/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

constexpr bool IsHostBE()
{
#if (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)) || defined(_M_PPC)
    return true;
#elif (defined(__BYTE_ORDER__) && (__BYTE_ORDER__== __ORDER_LITTLE_ENDIAN__))
    return false;
#else
    #error Unable to determine host byte order
#endif
}

/// @{
/// @brief Swap the byte order of the value
///
/// @param v Value to be swapped
///
/// @return Value in swapped byte order
///
/// @note Requires compiler intrinsics
#if defined(__GNUC__) || defined(__clang__)
uint16 SwapBytes16(uint16 v) { return __builtin_bswap16(v); }
uint32 SwapBytes32(uint32 v) { return __builtin_bswap32(v); }
uint64 SwapBytes64(uint64 v) { return __builtin_bswap64(v); }
#else
#error Unable to determine compiler intrinsics
#endif
/// @}

/// @{
/// @brief Swap the byte order of the value from host (memory) to Big-Endian (network) byte order if needed
///
/// @param v Value in host byte order
///
/// @return Value in Big-Endian order
///
/// @note Compiler may no-op if host is BE
uint16 HostToBigEndian16(uint16 v) { return IsHostBE() ? v : SwapBytes16(v); }
uint32 HostToBigEndian32(uint32 v) { return IsHostBE() ? v : SwapBytes32(v); }
uint64 HostToBigEndian64(uint64 v) { return IsHostBE() ? v : SwapBytes64(v); }
/// @}

/// @{
/// @brief Swap the byte order of the value from Big-Endian (network) to host (memory) byte order if needed
///
/// @param v Value in Big-Endian order
///
/// @return Value in host byte order
///
/// @note Compiler may no-op if host is BE
uint16 BigEndianToHost16(uint16 v) { return IsHostBE() ? v : SwapBytes16(v); }
uint32 BigEndianToHost32(uint32 v) { return IsHostBE() ? v : SwapBytes32(v); }
uint64 BigEndianToHost64(uint64 v) { return IsHostBE() ? v : SwapBytes64(v); }
/// @}

} // namespace DevDriver
