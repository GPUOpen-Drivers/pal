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

#include "palUuid.h"
#include "palUuidLiteral.h"

// PAL::Util support libs
#include "palByteSwap.h"
#include "palMutex.h"

#include "imported/TinySHA1.hpp"
#include "imported/md5.hpp"
#include "imported/libuuid/libuuid.h"

#include <random>

namespace Util
{
namespace Uuid
{

// Hashing functions
namespace Hashing
{

struct Sha1Digest
{
    ::sha1::SHA1::digest8_t bytes;
};

struct Md5Digest
{
    ::websocketpp::md5::md5_byte_t bytes[16];
};

namespace _detail
{
// =====================================================================================================================
template <typename T, size_t N>
inline void BlockXor(
    T       (&out)[N],
    const T (&in)[N],
    T       x)
{
    for (size_t i = 0; i < N; ++i)
    {
        out[i] = in[i] ^ x;
    }
}

// =====================================================================================================================
template<typename context_t, typename... Ts>
inline void Sha1ConcatHash(context_t& context) {};

// =====================================================================================================================
template<typename context_t, typename... Ts>
inline void Sha1ConcatHash(
    context_t&  context,
    const void* pData,
    size_t      dataSize,
    Ts...       additional)
{
    context.processBytes(pData, dataSize);
    Sha1ConcatHash(context, additional...);
};

// =====================================================================================================================
template<typename context_t, typename... Ts>
inline void Md5ConcatHash(context_t& context) {};

// =====================================================================================================================
template<typename context_t, typename... Ts>
inline void Md5ConcatHash(
    context_t&  context,
    const void* pData,
    size_t      dataSize,
    Ts...       additional)
{
    using namespace ::websocketpp::md5;

    md5_append(&context, static_cast<const uint8_t*>(pData), dataSize);
    Md5ConcatHash(context, additional...);
};
} // namespace _detail

// =====================================================================================================================
// Additional values must be passed in pairs of const void*, size_t
template<typename... Ts>
inline Sha1Digest Sha1Hash(
    const void* pData,
    size_t      dataSize,
    Ts...       additional)
{
    sha1::SHA1 context = {};
    Sha1Digest digest;
    _detail::Sha1ConcatHash(context, pData, dataSize, additional...);
    context.getDigestBytes(digest.bytes);
    return digest;
};

// =====================================================================================================================
// HMAC : HASH(o_key_pad + SHA1(i_key_pad + message))
// Additional values must be passed in pairs of const void*, size_t
template<typename... Ts>
inline Sha1Digest Sha1HashHmac(
    const void* pSecret,
    size_t      secretSize,
    const void* pData,
    size_t      dataSize,
    Ts...       additional)
{
    // Values picked by NIST to have a large `Hamming distance` from each other
    static constexpr uint8_t iXor = 0x36;
    static constexpr uint8_t oXor = 0x5c;

    // Values for SHA1
    static constexpr size_t BlockSize = 64;
    static constexpr size_t OutputSize = 20;

    // Ensure buffer is padded to the right with 0s
    uint8_t key[BlockSize] = {};
    if (secretSize > BlockSize)
    {
        // SHA1 secret if too big to fit in key
        memcpy(key, Sha1Hash(pSecret, secretSize).bytes, OutputSize);
    }
    else
    {
        memcpy(key, pSecret, secretSize);
    }

    // temp buffers
    uint8_t keyPad[BlockSize] = {};

    // Inner pass
    _detail::BlockXor(keyPad, key, iXor);
    const Sha1Digest iDigest = Sha1Hash(keyPad, BlockSize, pData, dataSize, additional...);

    // Outer pass
    _detail::BlockXor(keyPad, key, oXor);
    return Sha1Hash(keyPad, BlockSize, iDigest.bytes, OutputSize);
};

// =====================================================================================================================
// Additional values must be passed in pairs of const void*, size_t
template<typename... Ts>
inline Md5Digest Md5Hash(
    const void* pData,
    size_t      dataSize,
    Ts...       additional)
{
    using namespace ::websocketpp::md5;

    md5_state_t context = {};
    Md5Digest digest = {};

    md5_init(&context);
    _detail::Md5ConcatHash(context, pData, dataSize, additional...);
    md5_finish(&context, digest.bytes);

    return digest;
};
} // namespace Hashing

// OS specific function prototypes
namespace Os
{
extern Node GetLocalNode();
extern uint64 GetFixedTimePoint();
extern uint32 GetSequenceStart();
extern Timestamp GetCurrentTimestamp();
} // namespace Os

// private utility functions
namespace
{
// =====================================================================================================================
inline Uuid ForceVersionAndVariant(
    Version     version,
    Variant     variant,
    const Uuid& uuid)
{
    PAL_ASSERT(version != Version::Invalid);
    PAL_ASSERT(variant != Variant::Invalid);

    constexpr int VariantByte = 8;
    const int versionByte = (variant == Variant::Rfc4122) ? 6 : 7;

    Uuid value = uuid;

    // Force the version to the high bits of the version byte
    value.raw[versionByte] = ((static_cast<uint8>(version) << 4) & 0xF0) | (uuid.raw[versionByte] & 0x0F);
    value.raw[VariantByte] = (variant == Variant::Rfc4122)
        ? 0x80 | (uuid.raw[VariantByte] & 0x3F)  // Rfc4122
        : 0xc0 | (uuid.raw[VariantByte] & 0x1F); // MsCompatible

    return value;
}

// =====================================================================================================================
inline Uuid ConstructUuid1FromParts(
    const Node& node,
    Timestamp   timestamp,
    uint32_t    sequenceId)
{
    const Uuid value
    {
        .data
        {
            // Ensure network byte order
            .timeLow = HostToBigEndian32(timestamp & 0xFFFFFFFF),
            .timeMid = HostToBigEndian16((timestamp >> 32) & 0xFFFF),
            .timeHighAndVersion = HostToBigEndian16((timestamp >> 48) & 0xFFFF),
            .variantAndSequence = HostToBigEndian16(sequenceId & 0xFFFF),

            // Copy the node id over
            .node = node,
        }
    };
    return ForceVersionAndVariant(Version::Version1, Variant::Rfc4122, value);
}
} // namespace

// =====================================================================================================================
void ToString(
    const Uuid& uuid,
    char*       pOutString)
{
    char buffer[37] = "";
    uuid_unparse_lower(uuid.raw, buffer);
    memcpy(pOutString, buffer, 36);
}

// =====================================================================================================================
Uuid FromString(
    const char* pUuidString)
{
    using Literals::UuidNil;

    Uuid uuid = {};
    return (uuid_parse_range(&pUuidString[0], &pUuidString[36], uuid.raw) == 0)
        ? uuid
        : UuidNil;
}

// =====================================================================================================================
const Node& GetLocalNode()
{
    static const Node localNode = Os::GetLocalNode();

    return localNode;
}

// =====================================================================================================================
Timestamp GetCurrentTimestamp()
{
    return Os::GetCurrentTimestamp();
}

// =====================================================================================================================
Uuid GetGlobalNamespace()
{
    using Literals::UuidNamespaceAmdDriver;

    static const Uuid GlobalNamespace = Uuid5(UuidNamespaceAmdDriver, "GlobalNamespace");

    return GlobalNamespace;
}

// =====================================================================================================================
Uuid GetLocalNamespace()
{
    static const uint64_t FixedTimePoint = Os::GetFixedTimePoint();
    static const Uuid LocalBase = ConstructUuid1FromParts(GetLocalNode(), FixedTimePoint, 0);
    static const Uuid LocalNamespace = Uuid5(LocalBase, "LocalNamespace");

    return LocalNamespace;
}

// =====================================================================================================================
Uuid Uuid1(
    const Node& node,
    uint64      timestamp)
{
    // Sequence Id is incremented for every call to ensure divergence
    static volatile uint32 sequencId = {Os::GetSequenceStart()};
    Util::AtomicIncrement(&sequencId);

    return ConstructUuid1FromParts(node, timestamp, sequencId);
}

// =====================================================================================================================
Uuid Uuid3(
    const Uuid& scope,
    const void* pData,
    size_t      dataSize)
{
    using namespace Hashing;

    const Md5Digest digest = Md5Hash(scope.raw, sizeof(scope.raw), pData, dataSize);

    // Truncate the digest into our UUID
    Uuid uuid = {};
    memcpy(uuid.raw, digest.bytes, sizeof(uuid.raw));

    return ForceVersionAndVariant(Version::Version3, Variant::Rfc4122, uuid);
}

// =====================================================================================================================
Uuid Uuid4()
{
    static Mutex mutex;
    static ::std::mt19937_64 generator(Os::GetSequenceStart());
    MutexAuto guard(&mutex);

    Uuid uuid { .raw64 = { generator(), generator() } };

    // ensure we don't have our local node id by accident
    if (uuid.data.node == GetLocalNode())
    {
        uuid.raw64[1] ^= generator();
    }

    return ForceVersionAndVariant(Version::Version4, Variant::Rfc4122, uuid);
}

// =====================================================================================================================
Uuid Uuid5(
    const Uuid& scope,
    const void* pData,
    size_t      dataSize)
{
    using namespace Hashing;

    // Generate the hash
    const Sha1Digest digest = Sha1Hash(scope.raw, sizeof(scope.raw), pData, dataSize);

    // Truncate the digest into our UUID
    Uuid uuid = {};
    memcpy(uuid.raw, digest.bytes, sizeof(uuid.raw));

    return ForceVersionAndVariant(Version::Version5, Variant::Rfc4122, uuid);
}

// =====================================================================================================================
Uuid Uuid5Hmac(
    const Uuid& scope,
    const void* pData,
    size_t      dataSize,
    const void* pSecret,
    size_t      secretSize)
{
    using namespace Hashing;

    Uuid uuid = {};

    // If there's no secret, call the normal function
    if ((pSecret == nullptr) ||
        (secretSize == 0))
    {
        uuid = Uuid5(scope, pData, dataSize);
    }
    else
    {
        // Generate the hash
        const Sha1Digest digest = Sha1HashHmac(
            pSecret, secretSize,
            scope.raw, sizeof(scope.raw),
            pData, dataSize);

        // Truncate the digest into our UUID
        memcpy(uuid.raw, digest.bytes, sizeof(uuid.raw));

        // Force the version and variant
        uuid = ForceVersionAndVariant(Version::Version5, Variant::Rfc4122, uuid);
    }

    return uuid;
}

} // namespace Uuid
} // namespace Util
