/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palInlineFuncs.h
 * @brief PAL utility collection inline functions.
 ***********************************************************************************************************************
 */

#pragma once

#include "palAssert.h"
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <type_traits>

namespace Util
{

/// Describes a value type, primarily used for loading settings values.
enum class ValueType : uint32
{
    Boolean,  ///< Boolean type.
    Int,      ///< Signed integer type.
    Uint,     ///< Unsigned integer type.
    Uint64,   ///< 64-bit unsigned integer type.
    Float,    ///< Floating point type.
    Str,      ///< String type.
};

/// Determines the length of an array at compile-time.
///
/// @returns The length of the array.
template <typename T, size_t N>
constexpr size_t ArrayLen(
    const T (&array)[N]) ///< The array of arbitrary type T.
{
    return N;
}

/// Increments a const pointer by nBytes by first casting it to a const uint8*.
///
/// @returns Incremented pointer.
constexpr const void* VoidPtrInc(
    const void* p,         ///< [in] Pointer to be incremented.
    size_t      numBytes)  ///< Number of bytes to increment the pointer by.
{
    return (static_cast<const uint8*>(p) + numBytes);
}

/// Increments a pointer by nBytes by first casting it to a uint8*.
///
/// @returns Incremented pointer.
constexpr void* VoidPtrInc(
    void*  p,         ///< [in] Pointer to be incremented.
    size_t numBytes)  ///< Number of bytes to increment the pointer by.
{
    return (static_cast<uint8*>(p) + numBytes);
}

/// Decrements a const pointer by nBytes by first casting it to a const uint8*.
///
/// @returns Decremented pointer.
constexpr const void* VoidPtrDec(
    const void* p,         ///< [in] Pointer to be decremented.
    size_t      numBytes)  ///< Number of bytes to decrement the pointer by.
{
    return (static_cast<const uint8*>(p) - numBytes);
}

/// Decrements a pointer by nBytes by first casting it to a uint8*.
///
/// @returns Decremented pointer.
constexpr void* VoidPtrDec(
    void*  p,         ///< [in] Pointer to be decremented.
    size_t numBytes)  ///< Number of bytes to decrement the pointer by.
{
    return (static_cast<uint8*>(p) - numBytes);
}

/// Finds the number of bytes between two pointers by first casting them to uint8*.
///
/// This function expects the first pointer to not be smaller than the second.
///
/// @returns Number of bytes between the two pointers.
PAL_INLINE size_t VoidPtrDiff(
    const void* p1,  ///< [in] First pointer (higher address).
    const void* p2)  ///< [in] Second pointer (lower address).
{
    PAL_ASSERT(p1 >= p2);
    return (static_cast<const uint8*>(p1) - static_cast<const uint8*>(p2));
}

/// Returns the high 32 bits of a 64-bit integer.
///
/// @returns Returns the high 32 bits of a 64-bit integer.
constexpr uint32 HighPart(
    uint64 value)  ///< 64-bit input value.
{
    return (value & 0xFFFFFFFF00000000) >> 32;
}

/// Returns the low 32 bits of a 64-bit integer.
///
/// @returns Returns the low 32 bits of a 64-bit integer.
constexpr uint32 LowPart(
    uint64 value)  ///< 64-bit input value.
{
    return (value & 0x00000000FFFFFFFF);
}

/// Returns a bitfield from within some value.
///
/// @returns Returns a bitfield from within some value.
template <typename T>
constexpr T BitExtract(
    T      value,    ///< Extract a bitfield from here.
    uint32 firstBit, ///< The zero-based index of the first bit to extract.
    uint32 lastBit)  ///< The zero-based index of the last bit to extract.
{
    return (value >> firstBit) & ((1 << (lastBit - firstBit + 1)) - 1);
}

/// Determines if any of the bits set in "test" are also set in "src".
///
/// @returns True if any bits in "test" are set in "src", false otherwise.
constexpr bool TestAnyFlagSet(
    uint32 src,   ///< Source pattern.
    uint32 test)  ///< Test pattern.
{
    return ((src & test) != 0);
}

/// Determines if all of the bits set in "test" are also set in "src".
///
/// @returns True if all bits set in "test" are also set in "src", false otherwise.
constexpr bool TestAllFlagsSet(
    uint32 src,   ///< Source pattern.
    uint32 test)  ///< Test pattern.
{
    return ((src & test) == test);
}

/// Tests if a single bit in a "wide bitfield" is set. A "wide bifield" is a bitfield which spans an array of
/// integers because there are more flags than bits in one integer.
///
/// @param [in] bitfield  Reference to the bitfield being tested
/// @param [in] bit       Index of the flag to test
///
/// @returns True if the flag is set.
template <typename T, size_t N>
bool WideBitfieldIsSet(
    const T (&bitfield)[N],
    uint32  bit)
{
    const uint32 index = (bit / (sizeof(T) << 3));
    const T      mask  = (static_cast<T>(1) << (bit & ((sizeof(T) << 3) - 1)));

    return (0 != (bitfield[index] & mask));
}

/// Sets a single bit in a "wide bitfield" to one. A "wide bifield" is a bitfield which spans an array of
/// integers because there are more flags than bits in one integer.
///
/// @param [in] bitfield  Reference to the bitfield being modified
/// @param [in] bit       Index of the flag to set
template <typename T, size_t N>
void WideBitfieldSetBit(
    T      (&bitfield)[N],
    uint32 bit)
{
    const uint32 index = (bit / (sizeof(T) << 3));
    const T      mask  = (static_cast<T>(1) << (bit & ((sizeof(T) << 3) - 1)));

    bitfield[index] |= mask;
}

/// Clears a single bit in a "wide bitfield" to zero. A "wide bifield" is a bitfield which spans an array of
/// integers because there are more flags than bits in one integer.
///
/// @param [in] bitfield  Reference to the bitfield being modified
/// @param [in] bit       Index of the flag to set
template <typename T, size_t N>
void WideBitfieldClearBit(
    T      (&bitfield)[N],
    uint32 bit)
{
    const uint32 index = (bit / (sizeof(T) << 3));
    const T      mask  = (static_cast<T>(1) << (bit & ((sizeof(T) << 3) - 1)));

    bitfield[index] &= ~mask;
}

/// XORs all of the bits in two "wide bitfields". A "wide bifield" is a bitfield which spans an array of integers
/// because there are more flags than bits in one integer.
///
/// @param [in] bitfield1 Reference to the first bitfield.
/// @param [in] bitfield2 Reference to the second bitfield.
/// @param [in] pOut      Result of (bitfield1 ^ bitfield2)
template <typename T, size_t N>
void WideBitfieldXorBits(
    T      (&bitfield1)[N],
    T      (&bitfield2)[N],
    T*     pOut)
{
    for (uint32 i = 0; i < N; i++)
    {
        pOut[i] = (bitfield1[i] ^ bitfield2[i]);
    }
}

/// ANDs all of the bits in two "wide bitfields". A "wide bifield" is a bitfield which spans an array of integers
/// because there are more flags than bits in one integer.
///
/// @param [in] bitfield1 Reference to the first bitfield.
/// @param [in] bitfield2 Reference to the second bitfield.
/// @param [in] pOut      Result of (bitfield1 & bitfield2)
template <typename T, size_t N>
void WideBitfieldAndBits(
    T      (&bitfield1)[N],
    T      (&bitfield2)[N],
    T*     pOut)
{
    for (uint32 i = 0; i < N; i++)
    {
        pOut[i] = (bitfield1[i] & bitfield2[i]);
    }
}

/// Scans the specified bit-mask for the least-significant '1' bit.
///
/// @returns True if the input was nonzero; false otherwise.
template <typename T>
bool BitMaskScanForward(
    uint32* pIndex,  ///< [out] Index of least-significant '1' bit.  Undefined if input is zero.
    T       mask)    ///< Bit-mask to scan.
{
    // Bitscan intrinsics may compile to flaky code in certain situations. Discarding bitscan flags avoids this. The key
    // is to forward declare result, and set it in a conditional branch after the bitscan. Be careful if modifying this.
    bool result = false;

#if   defined(__GNUC__)
    *pIndex = (sizeof(T) > 4) ? __builtin_ctzll(mask) : __builtin_ctz(static_cast<uint32>(mask));
#else
    if (mask != 0)
    {
        uint32 index = 0;
        for (; ((mask & 0x1) == 0); mask >>= 1, ++index);
        *pIndex = index;
    }
#endif

    if (mask != 0)
    {
        result = (mask != 0);
    }
    return result;
}

/// Scans the specified bit-mask for the most-significant '1' bit.
///
/// @returns True if the input was nonzero; false otherwise.
template <typename T>
bool BitMaskScanReverse(
    uint32* pIndex,  ///< [out] Index of most-significant '1' bit.  Undefined if input is zero.
    T       mask)    ///< Bit-mask to scan.
{
    // Bitscan intrinsics may compile to flaky code in certain situations. Discarding bitscan flags avoids this. The key
    // is to forward declare result, and set it in a conditional branch after the bitscan. Be careful if modifying this.
    bool result = false;

#if   defined(__GNUC__)
    *pIndex = (sizeof(T) > 4) ? (63u - __builtin_clzll(mask)) : (31u - __builtin_clz(static_cast<uint32>(mask)));
#else
    if (mask != 0)
    {
        uint32 index = 31u;
        for (; (((mask >> index) & 0x1) == 0); --index);
        *pIndex = index;
    }
#endif

    if (mask != 0)
    {
        result = (mask != 0);
    }
    return result;
}

/// Scans the specified wide bit-mask for the least-significant '1' bit.
///
/// @returns True if input was nonzero; false otherwise.
template <typename T, size_t N>
bool WideBitMaskScanForward(
    uint32* pIndex,        ///< [out] Index of least-significant '1' bit.  Undefined if input is zero.
    T       (&mask)[N])    ///< Bit-mask to scan.
{
    uint32 maskIndex = ((*pIndex) / (sizeof(T) << 3));

    // Check to see if the wide bitmask has some bits set.
    uint32 index = 0;
    while ((mask[index] == 0) && (++index < N));
    bool result = (index < N);

    while (result == true)
    {
        result = BitMaskScanForward(pIndex, mask[maskIndex]);

        if (result == false)
        {
            ++maskIndex;
            result = (maskIndex < N);
        }
        else
        {
            (*pIndex) = (*pIndex) + (maskIndex * (sizeof(T) << 3));
            break;
        }
    }

    return result;
}

/// Scans the specified wide bit-mask for the most-significant '1' bit.
///
/// @returns True if input was nonzero; false otherwise.
template <typename T, size_t N>
bool WideBitMaskScanReverse(
    uint32* pIndex,        ///< [out] Index of most-significant '1' bit.  Undefined if input is zero.
    T       (&mask)[N])    ///< Bit-mask to scan.
{
    uint32 maskIndex = ((*pIndex) / (sizeof(T) << 3));

    // Check to see if the wide bitmask has some bits set.
    uint32 index = N - 1;
    while ((mask[index] == 0) && (--index > 0));
    bool result = (mask[index] != 0);

    while (result == true)
    {
        result = BitMaskScanReverse(pIndex, mask[maskIndex]);

        if (result == false)
        {
            const uint32 oldIndex = maskIndex--;
            result = (oldIndex != 0);
        }
        else
        {
            (*pIndex) = (*pIndex) + (maskIndex * (sizeof(T) << 3));
            break;
        }
    }

    return result;
}

/// Determines if a value is a power of two.
///
/// @returns True if it is a power of two, false otherwise.
constexpr bool IsPowerOfTwo(
    uint64 value)  ///< Value to check.
{
    return (value == 0) ? false : ((value & (value - 1)) == 0);
}

/// Determines if 'value' is at least aligned to the specified power-of-2 alignment.
///
/// @returns True if aligned, false otherwise.
PAL_INLINE bool IsPow2Aligned(
    uint64 value,      ///< Value to check.
    uint64 alignment)  ///< Desired alignment.
{
    PAL_ASSERT(IsPowerOfTwo(alignment));
    return ((value & (alignment - 1)) == 0);
}

/// Rounds the specified uint 'value' up to the nearest value meeting the specified 'alignment'.  Only power of 2
/// alignments are supported by this function.
///
/// @returns Aligned value.
template <typename T>
T Pow2Align(
    T      value,      ///< Value to align.
    uint64 alignment)  ///< Desired alignment (must be a power of 2).
{
    PAL_ASSERT(IsPowerOfTwo(alignment));
    return ((value + static_cast<T>(alignment) - 1) & ~(static_cast<T>(alignment) - 1));
}

/// Rounds the specified uint 'value' up to the nearest power of 2
///
/// @param [in] value  The value to pad.
///
/// @returns Power of 2 padded value.
template <typename T>
T Pow2Pad(
    T value)
{
    T ret = value;

    if ((value & (value - 1)) != 0)
    {
        uint32 lastBitIndex = 0;
        BitMaskScanReverse(&lastBitIndex, value);
        ret = (static_cast<T>(0x2) << lastBitIndex);
    }

    return ret;
}

/// Computes the base-2 logarithm of an unsigned integer.
///
/// If the given integer is not a power of 2, this function will not provide an exact answer.
///
/// @param [in] u  Value to compute the logarithm of.
///
/// @returns log_2(u)
template <typename T>
uint32 Log2(
    T u)
{
    uint32 logValue = 0;
    return BitMaskScanReverse(&logValue, u) ? logValue : 0;
}

/// Computes the base-2 logarithm of an unsigned 64-bit integer based on ceiling
///
/// If the given integer is not a power of 2, this function will not provide an exact answer.
///
/// @returns ceilLog_2(u)
template <typename T>
uint32 CeilLog2(
    T u)  ///< Value to compute the ceil logarithm of.
{
    const uint32 logValue = Log2(u);
    return ((static_cast<T>(0x1ul) << logValue) < u) ? (logValue + 1) : logValue;
}

/// Implements an alternative version of integer division in which the quotient is always rounded up instead of down.
///
/// @returns The rounded quotient.
template <typename T>
constexpr T RoundUpQuotient(
    T dividend, ///< Value to divide.
    T divisor)  ///< Value to divide by.
{
    return ((dividend + (divisor - 1)) / divisor);
}

/// Rounds up the specified integer to the nearest multiple of the specified alignment value.
///
/// @returns Rounded value.
template <typename T>
constexpr T RoundUpToMultiple(
    T operand,   ///< Value to be aligned.
    T alignment) ///< Alignment desired.
{
    return (((operand + (alignment - 1)) / alignment) * alignment);
}

/// Rounds down the specified integer to the nearest multiple of the specified alignment value.
///
/// @returns Rounded value.
template <typename T>
constexpr T RoundDownToMultiple(
    T operand,    ///< Value to be aligned.
    T alignment)  ///< Alignment desired.
{
    return ((operand / alignment) * alignment);
}

/// Rounds the specified 'value' down to the nearest value meeting the specified 'alignment'.  Only power of 2
/// alignments are supported by this function.
///
/// @returns Rounded value.
template <typename T>
T Pow2AlignDown(
    T      value,      ///< Value to align.
    uint64 alignment)  ///< Desired alignment (must be a power of 2).
{
    PAL_ASSERT(IsPowerOfTwo(alignment));
    return (value & ~(alignment - 1));
}

/// Determines the maximum of two numbers.
///
/// @returns The larger of the two inputs.
template <typename T>
constexpr T Max(
    T value1,  ///< First value to check.
    T value2)  ///< Second value to check.
{
    return ((value1 > value2) ? value1 : value2);
}

/// Determines the maximum of N numbers.
///
/// @returns The largest of all the inputs.
template <typename T, typename... Ts>
constexpr T Max(
    T     value1,  ///< First value to check.
    T     value2,  ///< Second value to check.
    Ts... values)  ///< Additional values to check.
{
    return Max(((value1 > value2) ? value1 : value2), values...);
}

/// Determines the minimum of two numbers.
///
/// @returns The smaller of the two inputs.
template <typename T>
constexpr T Min(
    T value1,  ///< First value to check.
    T value2)  ///< Second value to check.
{
    return ((value1 < value2) ? value1 : value2);
}

/// Determines the minimum of N numbers.
///
/// @returns The smallest of all the inputs.
template <typename T, typename... Ts>
constexpr T Min(
    T     value1,  ///< First value to check.
    T     value2,  ///< Second value to check.
    Ts... values)  ///< Additional values to check.
{
    return Min(((value1 < value2) ? value1 : value2), values...);
}

/// Clamps the input number so that it falls in-between the lower and upper bounds (inclusive).
///
/// @returns Clamped input number.
template <typename T>
constexpr T Clamp(
    T input,      ///< Input number to clamp.
    T lowBound,   ///< Lower-bound to clamp to.
    T highBound)  ///< Upper-bound to clamp to.
{
    return ((input <= lowBound)  ? lowBound  :
            (input >= highBound) ? highBound : input);
}

/// Converts a byte value to the equivalent number of DWORDs (uint32) rounded up.  I.e., 3 bytes will return 1 dword.
///
/// @returns Number of dwords necessary to cover numBytes.
PAL_INLINE uint32 NumBytesToNumDwords(
    uint32 numBytes)  ///< Byte count to convert.
{
    return Pow2Align(numBytes, static_cast<uint32>(sizeof(uint32))) / sizeof(uint32);
}

/// Compare two strings ignoring case
PAL_INLINE int Strcasecmp(
    const char* pSrc,     ///< [in] The source string to be compared.
    const char* pDst)     ///< [in] The dest string to compare.
{
    PAL_ASSERT(pSrc != nullptr);
    PAL_ASSERT(pDst != nullptr);

    return strcasecmp(pDst, pSrc);
}

/// Performs a safe strcpy by requiring the destination buffer size.
PAL_INLINE void Strncpy(
    char*       pDst,     ///< [out] Destination string.
    const char* pSrc,     ///< [in] Source string to be copied into destination.
    size_t      dstSize)  ///< Size of the destination buffer in bytes.
{
    PAL_ASSERT(pDst != nullptr);
    PAL_ASSERT(pSrc != nullptr);
    PAL_ALERT(strlen(pSrc) >= dstSize);

    strncpy(pDst, pSrc, (dstSize - 1));
    pDst[dstSize - 1] = '\0';
}

/// Simple wrapper for wcscpy_s or wcsncpy, which are available on Windows and Linux, respectively.
PAL_INLINE void Wcsncpy(
    wchar_t*       pDst,    ///< [out] Destination string.
    const wchar_t* pSrc,    ///< [in] Source string to copy.
    size_t         dstSize) ///< Length of the destination buffer, in wchar_t's.
{
    wcsncpy(pDst, pSrc, (dstSize - 1));
    pDst[dstSize - 1] = L'\0';
}

/// Simple wrapper for strncat or strncat_s which provides a safe version of strncat.
PAL_INLINE void Strncat(
    char*       pDst,     ///< [in,out] Destination string.
    size_t      sizeDst,  ///< Length of the destination string, including the null terminator.
    const char* pSrc)     ///< [in] Source string.
{
    PAL_ASSERT((pDst != nullptr) && (pSrc != nullptr));

#if   defined(__unix__)
    // Compute the length of the destination string to prevent buffer overruns.
    const size_t dstLength = strlen(pDst);
    strncat(pDst, pSrc, (sizeDst - dstLength - 1));
#endif
}

/// Simple wrapper for strtok_s or strtok_r which provides a safe version of strtok.
PAL_INLINE char* Strtok(
    char*       str,    ///< [in] Token string.
    const char* delim,  ///< [in] Token delimit.
    char**      buf)    ///< [in,out] Buffer to store the rest of the string.
{
    PAL_ASSERT((delim != nullptr) && (buf != nullptr));

    char* pToken = NULL;

#if   defined(__unix__)
    pToken = strtok_r(str, delim, buf);
#endif

    return pToken;
}

/// Rounds the specified pointer up to the nearest value meeting the specified 'alignment'.  Only power of 2 alignments
/// are supported by this function.
///
/// @returns Aligned pointer.
PAL_INLINE void* VoidPtrAlign(
    void*  ptr,        ///< Pointer to align.
    size_t alignment)  ///< Desired alignment.
{
    // This function only works for POW2 alignment
    PAL_ASSERT(IsPowerOfTwo(alignment));

    return reinterpret_cast<void*>(
               (reinterpret_cast<size_t>(ptr) + (alignment - 1)) & ~(alignment - 1));
}

/// Converts a raw string value to the correct data type.
PAL_INLINE void StringToValueType(
    const char* pStrValue,  ///< [in] Setting value in string form.
    ValueType   type,       ///< Data type of the value being converted.
    size_t      valueSize,  ///< Size of pValue buffer.
    void*       pValue)     ///< [out] Converted setting value buffer.
{
    switch (type)
    {
    case ValueType::Boolean:
        *(static_cast<bool*>(pValue)) = ((atoi(pStrValue)) ? true : false);
        break;
    case ValueType::Int:
        *(static_cast<int32*>(pValue)) = static_cast<int32>(strtol(pStrValue, nullptr, 0));
        break;
    case ValueType::Uint:
        *(static_cast<uint32*>(pValue)) = static_cast<uint32>(strtoul(pStrValue, nullptr, 0));
        break;
    case ValueType::Uint64:
        *(static_cast<uint64*>(pValue)) = static_cast<uint64>(strtoull(pStrValue, nullptr, 0));
        break;
    case ValueType::Float:
        *(static_cast<float*>(pValue)) = static_cast<float>(atof(pStrValue));
        break;
    case ValueType::Str:
        Strncpy(static_cast<char*>(pValue), pStrValue, valueSize);
        break;
    }
}

/// Hashes the provided string using FNV1a hashing (http://www.isthe.com/chongo/tech/comp/fnv/) algorithm.
///
/// @returns 32-bit hash generated from the provided string.
PAL_INLINE uint32 HashString(
    const char* pStr,     ///< [in] String to be hashed.
    size_t      strSize)  ///< Size of the input string.
{
    PAL_ASSERT((pStr != nullptr) && (strSize > 0));

    static constexpr uint32 FnvPrime  = 16777619u;
    static constexpr uint32 FnvOffset = 2166136261u;

    uint32 hash = FnvOffset;

    for (uint32 i = 0; i < strSize; i++)
    {
        hash ^= static_cast<uint32>(pStr[i]);
        hash *= FnvPrime;
    }

    return hash;
}

///@{
/// Counts the number of one bits (population count) in an unsigned integer using some bitwise magic explained in the
/// Software Optimization Guide for AMD64 Processors.
///
/// @param [in] value  The value need to be counted.
///
/// @returns Number of one bits in the input
template <typename T>
uint32 CountSetBits(
    T  value)
{
    uint32 x = static_cast<uint32>(value);

    x = x - ((x >> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x = (((x + (x >> 4)) & 0x0F0F0F0F) * 0x01010101) >> ((sizeof(uint32) - 1) << 3);

    return x;
}

PAL_INLINE uint32 CountSetBits(
    uint64  value)
{
    uint64 x = value;

    x = x - ((x >> 1) & 0x5555555555555555ull);
    x = (x & 0x3333333333333333ull) + ((x >> 2) & 0x3333333333333333ull);
    x = (((x + (x >> 4)) & 0x0F0F0F0F0F0F0F0Full) * 0x0101010101010101ull) >> ((sizeof(uint64) - 1) << 3);

    return static_cast<uint32>(x);
}
///@}

/// Indicates that an object may be moved from.
/// Can be understood as preparation for possible move operation.
///
/// @warning Do not read object after it has been moved from!
///
/// @param [in] object Universal reference to an object that may be moved from.
///
/// @returns Rvalue reference to the parameter object.
template <typename T>
constexpr typename std::remove_reference<T>::type&& Move(T&& object)
{
    // Cast universal reference to rvalue reference.
    return static_cast<typename std::remove_reference<T>::type&&>(object);
}

/// Exchanges values between two variables.
///
/// @param [in] left  First variable used in swap operation.
/// @param [in] right Second variable used in swap operation.
template <typename T>
void Swap(T& left, T& right)
{
    T tmp = Move(left);
    left  = Move(right);
    right = Move(tmp);
}

/// Convenient alias for C style arrays.
template <typename Element, size_t Size>
using Array = Element[Size];

/// Prevent swapping arrays because of the cost of this operation.
template <typename Element, size_t Size>
void Swap(Array<Element, Size>& a, Array<Element, Size>& b);

/// Compacts an array by moving all empty slots to the end of the array.
///         +---+---+---+---+---+---+---+---+---+---+
///  Input: | A |   | C | D |   | E |   | A | X | J |
///         +---+---+---+---+---+---+---+---+---+---+
///         +---+---+---+---+---+---+---+---+---+---+
/// Output: | A | C | D | E | A | X | J |   |   |   |
///         +---+---+---+---+---+---+---+---+---+---+
template <typename Element, size_t Size>
void PackArray(Array<Element, Size>& array, const Element& emptySlot)
{
    int lastOccupiedSlot = -1;

    for (size_t i = 0; i < Size; ++i)
    {
        if (array[i] != emptySlot)
        {
            Swap(array[i], array[lastOccupiedSlot + 1]);
            ++lastOccupiedSlot;
        }
    }
}

/// Performs a safe mbstowcs by requiring the destination buffer size.
PAL_INLINE void Mbstowcs(
    wchar_t*      pDst,           ///< [out] dst string
    const char*   pSrc,           ///< [in] src string
    size_t        dstSizeInWords) ///< size of the destination buffer in words
{
    PAL_ASSERT(pDst != NULL);
    PAL_ASSERT(pSrc != NULL);

    bool result = false;
    // clamp the conversion to the size of the dst buffer (1 char reserved for the NULL terminator)
    size_t retCode = mbstowcs(pDst, pSrc, (dstSizeInWords - 1));

    result = (retCode == static_cast<size_t>(-1)) ? false : true;

    if (result == false)
    {
        // A non-convertible character was encountered.
        PAL_ASSERT_ALWAYS();
        pDst[0] = '\0';
    }

    if (strlen(pSrc) >= dstSizeInWords)
    {
        // Assert to alert the user when the string has been truncated.
        PAL_ASSERT_ALWAYS();

        // NULL terminate the string.
        pDst[dstSizeInWords - 1] = '\0';
    }
}

/// Performs a safe wcstombs by requiring the destination buffer size.
PAL_INLINE void Wcstombs(
    char*          pDst,           ///< [out] dst string
    const wchar_t* pSrc,           ///< [in] src string
    size_t         dstSizeInBytes) ///< size of the destination buffer in bytes
{
    PAL_ASSERT(pDst != NULL);
    PAL_ASSERT(pSrc != NULL);

    bool result = false;
    // clamp the conversion to the size of the dst buffer (1 char reserved for the NULL terminator)
    size_t retCode = wcstombs(pDst, pSrc, (dstSizeInBytes - 1));

    result = (retCode == static_cast<size_t>(-1)) ? false : true;

    if (result == false)
    {
        // A non-convertible character was encountered.
        PAL_ASSERT_ALWAYS();
        pDst[0] = '\0';
    }

    if (wcslen(pSrc) >= dstSizeInBytes)
    {
        // Assert to alert the user when the string has been truncated.
        PAL_ASSERT_ALWAYS();

        // NULL terminate the string.
        pDst[dstSizeInBytes - 1] = '\0';
    }
}

} // Util
