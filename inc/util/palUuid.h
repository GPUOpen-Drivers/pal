/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palUuid.h
 * @brief Header for namespace Util::UUID. A collection of UUID generation functions
 ***********************************************************************************************************************
 */

#pragma once

#include "palUtil.h"

#include <string.h>

namespace Util
{

/**
 ***********************************************************************************************************************
 * @brief Platform-agnostic UUID generation functions
 ***********************************************************************************************************************
 */
namespace Uuid
{

/// @brief Possible Version/Types of UUID
enum class Version : uint32
{
    Invalid  = 0,   /// Not a recognized UUID
    Version1 = 1,   /// UUID is based on node and timestamp
    Version3 = 3,   /// UUID is a name local to a namespace (MD5)
    Version4 = 4,   /// UUID is random
    Version5 = 5,   /// UUID is a name local to a namespace (SHA1)
};

/// @brief Possible Variants of UUID
enum class Variant : uint32
{
    Invalid,        /// Not a recognized UUID
    Rfc4122,        /// UUID is stored in network byte order
    MsCompatible    /// UUID is stored in host byte order
};

/// @brief UUID 48-bit node sequence
struct Node
{
    uint8 raw[6];
};

// Static asserts
static_assert((sizeof(Node) == 6), "Node must be 6 bytes in length");

/// @brief UUID 60-bit timestamp (stored in 64-bit register)
using Timestamp = uint64;

/// @brief UUID data storage structure
///
/// @note Byte order of data is determined by Variant:
///       * `Variant::Rfc4122` denotes network byte order (big-endian)
///       * `Variant::MsCompatible` denotes host byte order (mixed-endian)
///
/// @note `variantAndSequence` and `node` are always network byte order
struct UuidData
{
    uint32 timeLow;               /// low 32-bits of timestamp
    uint16 timeMid;               /// middle 16-bits of timestamp
    uint16 timeHighAndVersion;    /// 4-bit version and high 12-bits of timestamp
    uint16 variantAndSequence;    /// 1 to 3-bit variant and 13 to 15-bit sequence id
    Node   node;                  /// 48-bit node ID
};

// Static asserts
static_assert((sizeof(UuidData) == 16), "UuidData must be tightly packed");
static_assert((offsetof(UuidData, node) == 10), "UuidData::node must be at 10 byte offset");

/// @brief An accessor union of UUID in both raw bytes and UuidData formats
union Uuid
{
    uint8    raw[sizeof(UuidData)];  /// raw digits of the UUID as byte array
    uint64   raw64[2];               /// raw data as two 64-bit registers
    UuidData data;                   /// helper accessor for UUID components
};

// Static asserts
static_assert((sizeof(Uuid) == 16), "UUID union must be 128-bit to fit in a register");

/// @brief Compare two UUIDs
///
/// @param left UUID to be compared
/// @param right UUID to be compared
///
/// @return integer value similar to ::std::memcmp
PAL_FORCE_INLINE int Compare(
    const Uuid& left,
    const Uuid& right)
{
    return memcmp(left.raw, right.raw, sizeof(Uuid));
}

/// @brief Get the variant type of the UUID
///
/// @param uuid UUID to be checked
///
/// @return Variant type of the given UUID
PAL_FORCE_INLINE Variant GetVariant(
    const Uuid& uuid) noexcept
{
    constexpr int VariantByte             = 8;
    constexpr int VariantMask             = 0b11000000;
    constexpr int VariantShift            = 6;
    constexpr int VariantBitsRfc4122      = 0b10;
    constexpr int VariantBitsMsCompatible = 0b11;

    const int variantBits = (uuid.raw[VariantByte] & VariantMask) >> VariantShift;

    return (variantBits == VariantBitsRfc4122)
        ? Variant::Rfc4122
        : ((variantBits == VariantBitsMsCompatible)
            ? Variant::MsCompatible
            : Variant::Invalid);
}

/// @brief Get the version (creation method) of the UUID
///
/// @param uuid UUID to be checked
///
/// @return Version/Type of the give UUID
PAL_FORCE_INLINE Version GetVersion(
    const Uuid& uuid) noexcept
{
    constexpr int VersionMask             = 0xF0;
    constexpr int VersionShift            = 4;
    constexpr int MaxVersion              = 5;
    constexpr int VersionByteRfc4122      = 6;
    constexpr int VersionByteMsCompatible = 7;

    const int versionByte = (GetVariant(uuid) != Variant::MsCompatible)
        ? VersionByteRfc4122
        : VersionByteMsCompatible;

    const int version = (uuid.raw[versionByte] & VersionMask) >> VersionShift;

    return (version <= MaxVersion)
        ? static_cast<Version>(version)
        : Version::Invalid;
}

/// @brief Get the version (creation method) of the UUID
///
/// @param uuid UUID to be checked
///
/// @return True if the UUID has a valid Version and Variant, False otherwise
PAL_FORCE_INLINE bool IsValid(
    const Uuid& uuid) noexcept
{
    return (GetVariant(uuid) != Variant::Invalid) &&
           (GetVersion(uuid) != Version::Invalid);
}

/// @brief Convert a UUID to a string in 8-4-4-4-12 notation
///
/// @param uuid UUID to be converted
/// @param pOutString A buffer of at least 36 characters
///
/// @note This function will write exactly 36 characters into the buffer. No null terminator
///       will be written.
void ToString(
    const Uuid& uuid,
    char*       pOutString);

/// @brief Convert a string in 8-4-4-4-12 notation to a UUID
///
/// @param pUuidString A buffer of at least 36 characters containing a UUID
///
/// @return A valid UUID on success, UuidNil on failure
///
/// @note This function will read exactly 36 characters from the buffer
Uuid FromString(
    const char* pUuidString);

/// @brief Get the node ID of the machine
///
/// @return The 48-bit sequence identifying the current machine with the multicast bit set.
///
/// @note This ID may be local to the user and may not be portable between users
const Node& GetLocalNode();

/// @brief Get the current UUID timestamp
///
/// @return A 60-bit timestamp of 100-nanoseconds since epoch
Timestamp GetCurrentTimestamp();

/// @brief Get the UUID of the global namespace
///
/// @return A valid Uuid defining the global (portable) namespace
Uuid GetGlobalNamespace();

/// @brief Get the UUID of the local namespace
///
/// @return A valid Uuid defining the local (non-portable) namespace
Uuid GetLocalNamespace();

/// @brief Create a UUID Version 1 (node and time)
///
/// @param node A 48-bit node ID
/// @param timestamp A 60-bit timestamp
///
/// @return A valid Uuid that varies based on node and time
Uuid Uuid1(
    const Node& node      = GetLocalNode(),
    Timestamp   timestamp = GetCurrentTimestamp());

/// @brief Create a UUID Version 3 (MD5)
///
/// @param scope a valid Uuid defining a namespace
/// @param data a data buffer to be used as object name for hashing
/// @param dataSize size of data buffer in bytes
///
/// @return A valid Uuid defining an object within a namepace using MD5
Uuid Uuid3(
    const Uuid& scope,
    const void* data,
    size_t      dataSize);

/// @brief Create a UUID Version 3 from a known-length string (MD5)
///
/// @param scope a valid Uuid defining a namespace
/// @param name an object name with a known length
///
/// @return A valid Uuid defining an object within a namepace using MD5
template<size_t N>
Uuid Uuid3(
    const Uuid& scope,
    const char (&name)[N])
{
    return Uuid3(scope, name, N);
}

/// @brief Create a UUID Version 4 (random)
///
/// @return A valid random Uuid
Uuid Uuid4();

/// @brief Create a UUID Version 5 (SHA1)
///
/// @param scope a valid Uuid defining a namespace
/// @param name a null-terminated string of the object name
/// @param data a data buffer to be used as object name for hashing
/// @param dataSize size of data buffer in bytes
///
/// @return A valid Uuid defining an object within a namepace using SHA1
Uuid Uuid5(
    const Uuid& scope,
    const void* data,
    size_t      dataSize);

/// @brief Create a UUID Version 3 from a known-length string (MD5)
///
/// @param scope a valid Uuid defining a namespace
/// @param name an object name with a known length
///
/// @return A valid Uuid defining an object within a namepace using MD5
template<size_t N>
Uuid Uuid5(
    const Uuid& scope,
    const char (&name)[N])
{
    return Uuid5(scope, name, N);
}

/// @brief Create a UUID Version 5 (SHA1) with HMAC secret
///
/// @param scope a valid Uuid defining a namespace
/// @param data a data buffer to be used as object name for hashing
/// @param dataSize size of data buffer in bytes
/// @param secret a data buffer containing an HMAC shared secret
/// @param secretSize a data buffer containing an HMAC shared secret
///
/// @return A valid Uuid defining an object within a namepace using SHA1
Uuid Uuid5Hmac(
    const Uuid& scope,
    const void* data,
    size_t      dataSize,
    const void* secret,
    size_t      secretSize);

/// {@
/// @note The following operators are provided for code clarity, and use with standard algorithms

/// @brief Equality operator for Node structure
///
/// @param left Node to be compared
/// @param right Node to be compared
///
/// @return true if the Node values are identical
constexpr inline bool operator==(
    const Node& left,
    const Node& right) noexcept
{
    return (
        (left.raw[0] == right.raw[0]) &&
        (left.raw[1] == right.raw[1]) &&
        (left.raw[2] == right.raw[2]) &&
        (left.raw[3] == right.raw[3]) &&
        (left.raw[4] == right.raw[4]) &&
        (left.raw[5] == right.raw[5])
    );
}

/// @brief Inequality operator for Node structure
///
/// @param left Node to be compared
/// @param right Node to be compared
///
/// @return true if the Node values are different
constexpr inline bool operator!=(
    const Node& left,
    const Node& right) noexcept
{
    return (
        (left.raw[0] != right.raw[0]) ||
        (left.raw[1] != right.raw[1]) ||
        (left.raw[2] != right.raw[2]) ||
        (left.raw[3] != right.raw[3]) ||
        (left.raw[4] != right.raw[4]) ||
        (left.raw[5] != right.raw[5])
    );
}

/// @brief Equality operator for Uuid structure
///
/// @param left Uuid to be compared
/// @param right Uuid to be compared
///
/// @return true if the Uuid values are identical
constexpr inline bool operator==(
    const Uuid& left,
    const Uuid& right) noexcept
{
    return (
        (left.raw64[0] == right.raw64[0]) &&
        (left.raw64[1] == right.raw64[1])
    );
}

/// @brief Inequality operator for Uuid structure
///
/// @param left Uuid to be compared
/// @param right Uuid to be compared
///
/// @return true if the Uuid values are different
constexpr inline bool operator!=(
    const Uuid& left,
    const Uuid& right) noexcept
{
    return (
        (left.raw64[0] != right.raw64[0]) ||
        (left.raw64[1] != right.raw64[1])
    );
}

/// @brief less-than operator for Uuid structure
///
/// @param left Uuid to be compared
/// @param right Uuid to be compared
///
/// @return true if the left Uuid is less than the right
inline bool operator<(
    const Uuid& left,
    const Uuid& right) noexcept
{
    return Compare(left, right) < 0;
}
/// @}

} // namespace Uuid
} // namespace Util
