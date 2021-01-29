/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palHashLiteralString.h
 * @brief PAL utility collection implementation of the HashLiteralString constexpr template function.
 ***********************************************************************************************************************
 */

#pragma once

#include "palInlineFuncs.h"

namespace Util
{
/// @internal FNV-1a hash offset used by HashLiteralString().
static constexpr uint32 Fnv1aOffset = 2166136261u;
/// FNV-1a hash prime used by HashLiteralString().
static constexpr uint32 Fnv1aPrime  = 16777619u;

/// @internal Template metafunction which can be used to generate a compile-time hash of a string literal.  This relies
///           on recursively applying the template to smaller and smaller subsections of the original string.
/// @note     MSVC requires index to be provided as a template argument to avoid having the input string appear in the
///           compiled binary.  This cannot safely be implemented as a non-templated constexpr function.
template <size_t N, uint32 I>
class Fnv1aHashHelper
{
public:
    /// @internal Recursively compute a hash value.
    ///
    /// @param [in] string C-style string to be hashed.
    ///
    /// @returns Hash of the specified substring.
    static constexpr uint32 Hash(
        const char (&string)[N])
    {
        return LowPart(static_cast<uint64>(Fnv1aHashHelper<N, I - 1>::Hash(string) ^ string[I - 1]) * Fnv1aPrime);
    }
};

/// @internal Partial specialization of the above template metafunction. This is needed to provide a base case which
///           terminates further recursion.
template <size_t N>
class Fnv1aHashHelper<N, 1>
{
public:
    /// @internal Recursively compute a hash value.
    ///
    /// @param [in] string C-style string to be hashed.
    ///
    /// @returns Hash of the specified substring.
    static constexpr uint32 Hash(
        const char (&string)[N])
    {
        return LowPart(static_cast<uint64>(Fnv1aOffset ^ string[0]) * Fnv1aPrime);
    }
};

/// Generates a compile-time hash of the specified literal C-style string using the FNV-1a hash algorithm.
///
/// @param [in] string C-style string literal to be hashed.
///
/// @returns 32-bit hash value for the specified string.
template <size_t N>
constexpr uint32 HashLiteralString(
    const char (&string)[N])
{
    // The "-1" on the second template parameter is necessary to avoid hashing the string's null-terminator!
    return Fnv1aHashHelper<N, N - 1>::Hash(string);
}

} // Util
