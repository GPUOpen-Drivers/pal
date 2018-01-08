/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @brief PAL utility collection implementation of the HashLiteralString template function.
 ***********************************************************************************************************************
 */

#include "palUtil.h"

namespace Util
{
/// @internal FNV-1a hash offset used by HashLiteralString().
static constexpr uint32 Fnv1aOffset = 2166136261u;
/// FNV-1a hash prime used by HashLiteralString().
static constexpr uint32 Fnv1aPrime  = 16777619u;

/// @internal Template metafunction which can be used to generate a compile-time hash of a string literal.  This relies
///           on recursively applying the template to smaller and smaller subsections of the original string.
template <size_t N, uint32 I>
class Fnv1aHashHelper
{
public:
    /// @internal Recursively compute a hash value.
    ///
    /// @param [in] string C-style string to be hashed.
    ///
    /// @returns Hash of the specified substring.
    static uint32 Hash(
        const char (&string)[N])
    {
        return ((Fnv1aHashHelper<N, I - 1>::Hash(string) ^ string[I - 1]) * Fnv1aPrime);
    }
};

/// @internal Partial specialization of the above template metafunction. This is needed to provide a base case which
/// terminates further recursion.
template <size_t N>
class Fnv1aHashHelper<N, 1>
{
public:
    /// @internal Recursively compute a hash value.
    ///
    /// @param [in] string C-style string to be hashed.
    ///
    /// @returns Hash of the specified substring.
    static uint32 Hash(
        const char (&string)[N])
    {
        return ((Fnv1aOffset ^ string[0]) * Fnv1aPrime);
    }
};

/// Hashes the specified literal C-style string using the FNV-1a hash algorithm.
///
/// This function generates compile-time hashes of string literals because it leverages function inlining combined with
/// the template metafunction Fnv1aHashHelper.
///
/// @note This only generates a compile-time hash in release builds! The MS compiler doesn't inline anything for debug
///       builds so in debug builds, this will compute the hash at runtime (it still computes it correctly, however).
///
/// @param [in] string C-style string literal to be hashed.
///
/// @returns 32-bit hash value for the specified string.
template <size_t N>
PAL_INLINE uint32 HashLiteralString(
    const char (&string)[N])
{
    // The "-1" on the second template parameter is necessary to avoid hashing the string's null-terminator!
    return Fnv1aHashHelper<N, N - 1>::Hash(string);
}

} // Util
