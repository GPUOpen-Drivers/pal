/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include <ddPlatform.h>
#include <metrohash.h>

namespace DevDriver
{

namespace MetroHash
{
/// 128-bit hash structure
struct Hash
{
    union
    {
        uint32 dwords[4]; ///< Output hash in dwords.
        uint8  bytes[16]; ///< Output hash in bytes.
    };
};

// Compacts a 128-bit hash into a 64-bit one by XOR'ing the low and high 64-bits together.
inline uint64 Compact64(
    const Hash* pHash)
{
    return (static_cast<uint64>(pHash->dwords[3] ^ pHash->dwords[1]) |
           (static_cast<uint64>(pHash->dwords[2] ^ pHash->dwords[0]) << 32));
}

// Compacts a 64-bit hash checksum into a 32-bit one by XOR'ing each 32-bit chunk together.
inline uint32 Compact32(
    const Hash* pHash)
{
    return pHash->dwords[3] ^ pHash->dwords[2] ^ pHash->dwords[1] ^ pHash->dwords[0];
}

// Compacts a 64-bit hash checksum into a 32-bit one by XOR'ing each 32-bit chunk together.
inline uint32 Compact32(
    const uint64 hash)
{
    return static_cast<uint32>(hash) ^ static_cast<uint32>(hash >> 32);
}

inline uint64 MetroHash64(const uint8* pData, const uint64 dataSize)
{
    uint64 hash = 0;
    Util::MetroHash64::Hash(pData, dataSize, reinterpret_cast<uint8*>(&hash));
    return hash;
}

inline uint32 MetroHash32(const uint8* pData, const uint64 dataSize)
{
    return Compact32(MetroHash64(pData, dataSize));
}

inline uint64 HashCStr64(const char* pString)
{
    return MetroHash64(reinterpret_cast<const uint8*>(pString), strlen(pString));
}

} // MetroHash
} // DevDriver
