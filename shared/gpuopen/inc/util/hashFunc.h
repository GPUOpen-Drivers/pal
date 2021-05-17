/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include <ddPlatform.h>
#include <util/ddMetroHash.h>

namespace DevDriver
{

/// ====================================================================================================================
/// Hashes the bytes of a Key using MetroHash
template<typename Key>
struct DefaultHashFunc
{
    uint32 operator()(const Key& key) const
    {
        return MetroHash::MetroHash32(reinterpret_cast<const uint8*>(&key), sizeof(Key));
    }
};

/// ====================================================================================================================
/// Hashes a const char* CString Key using Metrohash
template<>
struct DefaultHashFunc<const char*>
{
    uint32 operator()(const char* pKey) const
    {
        // We cannot pass NULL strings to strlen() and friends, so guard against it anyway.
        uint32 hash = 0;
        DD_ASSERT(pKey != nullptr);
        if (pKey != nullptr)
        {
            hash = MetroHash::MetroHash32(reinterpret_cast<const uint8*>(pKey), strlen(pKey));
        }

        return hash;
    }
};

/// Pointer keys are usually a mistake, so this version is explicitly 'delete'd
/// Overload this template if you're sure you need this. (See: const char* above)
template<typename T>
struct DefaultHashFunc<T*>
{
    uint32 operator()(const T* pKey) const = delete;
};

/// Generic compare functor for types that have defined the comparison operator
///
/// Used by @ref HashBase to prevent defining compare functions for each type.
template<typename Key>
struct DefaultEqualFunc
{
    bool operator()(const Key& key1, const Key& key2) const { return (key1 == key2); }
};

/// String compare functor for use with C-style strings
template<>
struct DefaultEqualFunc<const char*>
{
    bool operator()(const char* pKey1, const char* pKey2) const
    {
        DD_ASSERT(pKey1 != nullptr);
        DD_ASSERT(pKey2 != nullptr);
        return (strcmp(pKey1, pKey2) == 0);
    }
};

/// Generic compare functor for types with arbitrary size
///
/// Used by @ref HashBase to prevent defining compare functions for each type.
template<typename Key>
struct BitwiseEqualFunc
{
    bool operator()(const Key& key1, const Key& key2) const { return (memcmp(&key1, &key2, sizeof(Key)) == 0); }
};

} // namespace DevDriver
