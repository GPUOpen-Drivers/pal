/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include <ddPlatform.h>

#if (_MSC_VER > 1900)
#include <stdlib.h>
#endif

namespace DevDriver
{

constexpr bool IsHostBE()
{
#if (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)) || defined(_M_PPC)
    return true;
#elif (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)) || defined(_MSC_VER)
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
#elif (_MSC_VER > 1900)
uint16 SwapBytes16(uint16 v) { return _byteswap_ushort(v); }
uint32 SwapBytes32(uint32 v) { return _byteswap_ulong(v); }
uint64 SwapBytes64(uint64 v) { return _byteswap_uint64(v); }
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
