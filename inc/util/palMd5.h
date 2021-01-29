/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palMd5.h
 * @brief PAL utility collection Md5 namespace declarations.
 ***********************************************************************************************************************
 */

#pragma once

#if PAL_CLIENT_INTERFACE_VERSION < 369

#include "palUtil.h"

namespace Util
{

class File;

/// Namespace containing functions that provide support for MD5 checksums.
///
/// Code adapted from: http://www.fourmilab.ch/md5/
///
/// This code implements the MD5 message-digest algorithm.  The algorithm is due to Ron Rivest.  This code was written
/// by Colin Plumb in 1993, no copyright is claimed.  This code is in the public domain; do with it what you wish.
///
/// Equivalent code is available from RSA Data Security, Inc.  This code has been tested against that, and is
/// equivalent, except that you don't need to include two pages of legalese with every copy.
namespace Md5
{

/// Output hash value generated from the MD5 checksum.
struct Hash
{
    uint32 hashValue[4];  ///< Output hash value.
};

/// Working context for the MD5 checksum algorithm.
struct Context
{
    uint32 buf[4];   ///< Working buffer.
    uint32 bits[2];  ///< Bit count.
    uint8  in[64];   ///< Hash Value.
};

/// Generates an MD5 hash from the specified memory buffer.
///
/// @param [in] pBuffer Buffer in memory to be hashed.
/// @param [in] bufLen  Size of pBuffer, in bytes.
///
/// @returns 128-bit hash value.
extern Hash GenerateHashFromBuffer(const void* pBuffer, size_t bufLen);

/// Initializes an MD5 context to be used for incremental hashing of several buffers via Update().
///
/// Must be called before Update() or Final().
///
/// @param [in] pCtx MD5 context to be initialized.
extern void Init(Context* pCtx);

/// Updates the specified MD5 context based on the data in the specified buffer.
///
/// @param [in] pCtx   MD5 context to be updated.
/// @param [in] pBuf   Buffer in memory to be hashed in the MD5 context.
/// @param [in] bufLen Size of pBuf, in bytes.
extern void Update(Context* pCtx, const uint8* pBuf, size_t bufLen);

/// Updates the specified MD5 context based on the data in the specified object.
///
/// @param [in] pCtx   MD5 context to be updated.
/// @param [in] object Object to be hashed in the MD5 context.
template <typename T>
PAL_INLINE void Update(Context* pCtx, const T& object)
    { Update(pCtx, reinterpret_cast<const uint8*>(&object), sizeof(object)); }

/// Outputs the final MD5 hash after a series of Update() calls.
///
/// @param [in]  pCtx  MD5 context that has been accumulating a hash via calls to Update().
/// @param [out] pHash 128-bit hash value.
extern void Final(Context* pCtx, Hash* pHash);

/// Compacts a 128-bit MD5 checksum into a 64-bit one by XOR'ing the low and high 64-bits together to create a more
/// manageable 64-bit identifier based on the checksum.
///
/// @param [in] pHash 128-bit MD5 hash to be compacted.
///
/// @returns 64-bit hash value based on the inputted 128-bit MD5 hash.
PAL_INLINE uint64 Compact64(
    const Hash* pHash)
{
    return (static_cast<uint64>(pHash->hashValue[3] ^ pHash->hashValue[1]) |
           (static_cast<uint64>(pHash->hashValue[2] ^ pHash->hashValue[0]) << 32));
}

/// Compacts a 128-bit MD5 checksum into a 32-bit one by XOR'ing each 32-bit chunk together to create a more manageable
/// 32-bit identifier based on the checksum.
///
/// @param [in] pHash 128-bit MD5 hash to be compacted.
///
/// @returns 32-bit hash value based on the inputted 128-bit MD5 hash.
PAL_INLINE uint32 Compact32(
    const Hash* pHash)
{
    return pHash->hashValue[3] ^ pHash->hashValue[2] ^ pHash->hashValue[1] ^ pHash->hashValue[0];
}

} // Md5
} // Util

#endif
