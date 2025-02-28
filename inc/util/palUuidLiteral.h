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

/**
 ***********************************************************************************************************************
 * @file  palUuidLiteral.h
 * @brief Implemtation of MakeUuid() for compile time constants and Known UUID literals
 ***********************************************************************************************************************
 */

#pragma once

#include "palUuid.h"

#if defined(__cpp_exceptions)
#include <stdexcept>
#endif

namespace Util
{
namespace Uuid
{
namespace Literals
{

///@{
/// @internal Implementation namepace for MakeUuid()
///
/// @note These are a intended to be used by the compiler and should not be called directly as they
///       do not adhere to PAL coding standards.
namespace _detail
{
/// @brief lenght of an 8-4-4-4-12 string plus null
static constexpr size_t UuidStringLength = 37;

// Get integer value for a lowercase hexadecimal text digit [0-16)
inline constexpr unsigned char UuidDigitValue(
    char digit)
{
#if defined(__cpp_exceptions)
    if (((digit >= 'a' && digit <= 'f') ||
         (digit >= '0' && digit <= '9')) == false)
    {
        throw ::std::logic_error("UUID strings must contain only lowercase hex digits");
    }
#endif

    return (digit >= 'a')
        ? ((digit - 'a' + 10) & 0xf)
        : ((digit - '0') & 0xf);
}

// Convert a pair of hexadecimal text digits into a byte
inline constexpr unsigned char UuidConvertDigits(
    char digit1,
    char digit2)
{
    return ((UuidDigitValue(digit1) << 4) | UuidDigitValue(digit2));
}

// Convert an 8-4-4-4-12 UUID string into a `Uuid`
template<size_t N>
inline constexpr Uuid UuidStringConvert(
    const char (&str)[N])
{
#if defined(__cpp_exceptions)
    if ((str[8]  != '-') ||
        (str[13] != '-') ||
        (str[18] != '-') ||
        (str[23] != '-'))
    {
        throw ::std::logic_error("UUID strings must be in 8-4-4-4-12 format");
    }
#endif

    // Explicitly unrolled loop to ensure no constexpr issues under C++11
    return
    {
        UuidConvertDigits(str[0], str[1]),
        UuidConvertDigits(str[2], str[3]),
        UuidConvertDigits(str[4], str[5]),
        UuidConvertDigits(str[6], str[7]),
        // str[8] == '-'
        UuidConvertDigits(str[9], str[10]),
        UuidConvertDigits(str[11], str[12]),
        // str[13] == '-'
        UuidConvertDigits(str[14], str[15]),
        UuidConvertDigits(str[16], str[17]),
        // str[18] == '-'
        UuidConvertDigits(str[19], str[20]),
        UuidConvertDigits(str[21], str[22]),
        // str[23] == '-'
        UuidConvertDigits(str[24], str[25]),
        UuidConvertDigits(str[26], str[27]),
        UuidConvertDigits(str[28], str[29]),
        UuidConvertDigits(str[30], str[31]),
        UuidConvertDigits(str[32], str[33]),
        UuidConvertDigits(str[34], str[35])
    };
}
} // namespace _detail
///@}

/// @brief Allows for UUIDs to be declared as literal
///
/// @param str A char[37] literal representing a UUID in 8-4-4-4-12 format
///
/// @return A value formatted in Uuid data layout
///
/// @note Should only ever be used at compile time for constant expressions. For runtime string
///       conversion use UuidFromString().
///
/// @note If the format is invalid a compiler error will be given for the declaration line. No
///       validity checking is performed on the UUID other than formatting.
template<size_t N>
inline constexpr Uuid MakeUuid(
    const char (&str)[N])
{
#if defined(__cpp_exceptions)
    if (N != _detail::UuidStringLength)
    {
        throw ::std::logic_error("UUID strings must be 36 characters long (32 digits, 4 hyphens)");
    }
#endif

    return _detail::UuidStringConvert<N>(str);
}

/// @brief An empty, all zero Uuid
static constexpr Uuid UuidNil                = MakeUuid("00000000-0000-0000-0000-000000000000");

/// @brief Reserved Values from RFC4122
static constexpr Uuid UuidNamespaceDns       = MakeUuid("6ba7b810-9dad-11d1-80b4-00c04fd430c8");
static constexpr Uuid UuidNamespaceUrl       = MakeUuid("6ba7b811-9dad-11d1-80b4-00c04fd430c8");
static constexpr Uuid UuidNamespaceOid       = MakeUuid("6ba7b812-9dad-11d1-80b4-00c04fd430c8");
static constexpr Uuid UuidNamespaceX500      = MakeUuid("6ba7b814-9dad-11d1-80b4-00c04fd430c8");

/// @brief Starting namespace for the AMD driver Uuid5(UuidNamespaceDns, "driver.amd.com")
static constexpr Uuid UuidNamespaceAmdDriver = MakeUuid("2a263b6b-b7f2-56b3-a94c-c497a9069f4b");

} // namespace Literals
} // namespace Uuid
} // namespace Util
