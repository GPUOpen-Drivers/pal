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
* @file  palPlatformKey.h
* @brief PAL platform identification key library declaration.
***********************************************************************************************************************
*/

#pragma once

#include "palUtil.h"
#include "palHashProvider.h"

namespace Util
{
class IPlatformKey;

/// Get the memory size for a platform key object
///
/// @param [in] algorithm   Hashing algorithm to be used
///
/// @return Minimum size of memory buffer needed to pass to CreatePlatformKey()
size_t GetPlatformKeySize(
    HashAlgorithm algorithm);

/// Create a platform key object
///
/// @param [in]     algorithm       Hashing algorithm to be used
/// @param [in]     pInitialData    Optional pointer to initial data used to create the key, may be nullptr
/// @param [in]     initialDataSize Size of initial data buffer. Must be greater than 0 if pInitialData is not nullptr
/// @param [in]     pPlacementAddr  Pointer to the location where the interface should be constructed. There must
///                                 be as much size available here as reported by calling GetPlatformKeySize().
/// @param [out]    ppPlatformKey   Returned platform key object. On failure this value will be set to nullptr.
///
/// @returns Success if the cache layer was created. Otherwise, one of the following errors may be returned:
///         + ErrorUnknown if there is an internal error.
Result CreatePlatformKey(
    HashAlgorithm   algorithm,
    void*           pInitialData,
    size_t          initialDataSize,
    void*           pPlacementAddr,
    IPlatformKey**  ppPlatformKey);

/**
***********************************************************************************************************************
* @brief Platform specific identification key generator. Will contain information about the hardware and driver by
*        by default. Clients may choose to mix in additional data
***********************************************************************************************************************
*/
class IPlatformKey
{
public:
    /// Get the memory size of the platform key
    ///
    /// @return size of buffer returned by IPlatformKey::GetKey()
    virtual size_t GetKeySize() const = 0;

    /// Get the memory size of the platform key
    ///
    /// @return read-only buffer of size returned by IPlatformKey::GetKeySize()
    virtual const uint8* GetKey() const = 0;

    /// Mix the platform key down to a single 64-bit integer
    ///
    /// @return platform key expressed as a 64-bit digest
    virtual uint64 GetKey64() const = 0;

    /// Mix client data into the platform key hash
    ///
    /// @param [in] pData       Data to be hashed.
    /// @param [in] dataSize    Size of the data block to be hashed.
    ///
    /// @return Success if the key data was updated. Otherwise, one of the following may be returned:
    ///         + ErrorInvalidPointer if pData is nullptr.
    ///         + ErrorUnknown if there is an internal error.
    virtual Result AppendClientData(
        const void* pData,
        size_t      dataSize) = 0;

    /// Get the hashing context used to generate the key
    ///
    /// @return pointer to the hashing context suitable for duplication
    virtual const IHashContext* GetKeyContext() const = 0;

    /// Destroy the platform key object
    virtual void Destroy() = 0;

protected:
    IPlatformKey() {}
    virtual ~IPlatformKey() {}

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(IPlatformKey);
};

} //namespace Util
