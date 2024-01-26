/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palMetroHash.h
 * @brief PAL utility collection MetroHash namespace declarations.
 ***********************************************************************************************************************
 */

#pragma once

#include "metrohash.h"
#include "palAssert.h"
#include "palUtil.h"

namespace Util
{

/// Namespace containing functions that provide support for MetroHash.
namespace MetroHash
{

/// 128-bit hash structure
struct Hash
{
    union
    {
        uint32 dwords[4]; ///< Output hash in dwords.
        uint64 qwords[2]; ///< Output hash in qwords.
        uint8  bytes[16]; ///< Output hash in bytes.
    };
};

/// Compacts a 128-bit hash into a 64-bit one by XOR'ing the low and high 64-bits together.
///
/// @param [in] pHash 128-bit hash to be compacted.
///
/// @returns 64-bit hash value based on the inputted 128-bit hash.
constexpr uint64 Compact64(
    const Hash* pHash)
{
    return (static_cast<uint64>(pHash->dwords[3] ^ pHash->dwords[1]) |
           (static_cast<uint64>(pHash->dwords[2] ^ pHash->dwords[0]) << 32));
}

/// Compacts a 64-bit hash checksum into a 32-bit one by XOR'ing each 32-bit chunk together.
///
/// @param [in] pHash 128-bit hash to be compacted.
///
/// @returns 32-bit hash value based on the inputted 128-bit hash.
constexpr uint32 Compact32(
    const Hash* pHash)
{
    return pHash->dwords[3] ^ pHash->dwords[2] ^ pHash->dwords[1] ^ pHash->dwords[0];
}

/// Compacts a 64-bit hash checksum into a 32-bit one by XOR'ing each 32-bit chunk together.
///
/// @param [in] hash 64-bit hash to be compacted.
///
/// @returns 32-bit hash value based on the inputted 64-bit hash.
constexpr uint32 Compact32(
    uint64 hash)
{
    return static_cast<uint32>(hash) ^ static_cast<uint32>(hash >> 32);
}

/// Hash functor.
///
/// Helpful when a 128-bit hash is being used as a key in another container.
template<typename T>
struct HashFunc
{
    /// Hashes the specified 128-bit key value by XOR'ing each 32-bit chunk.
    ///
    /// @param [in] pVoidKey Pointer to the key to be hashed.
    /// @param [in] keyLen   Amount of data at pVoidKey to hash, in bytes.
    ///
    /// @returns 32-bit uint hash value.
    uint32 operator()(const void* pVoidKey, uint32 keyLen) const
    {
        PAL_ASSERT(keyLen == sizeof(Hash));
        return Compact32(static_cast<const Hash*>(pVoidKey));
    }

    /// No init job. Defined to be compatible with default hash func.
    void Init(uint32) const { }
};

} // MetroHash
} // Util
