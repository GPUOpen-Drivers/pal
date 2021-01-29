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
* @file  palHashProvider.h
* @brief PAL utility interface for OS provided hashing libraries
***********************************************************************************************************************
*/
#pragma once

#include "palUtil.h"

namespace Util
{

class IHashContext;

/// Ids for commonly supported hashing algorithms provided by the OS
enum class HashAlgorithm : uint32
{
    NoOp   = 0x00, ///< Null/Dummy algorithm
    Md5    = 0x01, ///< Message Digest 5 (128-bit digest)
    Sha1   = 0x10, ///< Secure Hash Algorithm 1 (160-bit digest)
    Sha224 = 0x20, ///< Secure Hash Algorithm 2 (224-bit digest)
    Sha256 = 0x21, ///< Secure Hash Algorithm 2 (256-bit digest)
    Sha384 = 0x22, ///< Secure Hash Algorithm 2 (384-bit digest)
    Sha512 = 0x23, ///< Secure Hash Algorithm 2 (512-bit digest)
};

/// Minimum memory buffer sizes needed to hold data relating to hash algorithms
struct HashContextInfo
{
    size_t contextObjectSize;      ///< size of buffer needed to pass to CreateHashContext and IHashContext::Duplicate
    size_t contextObjectAlignment; ///< alignment of buffer needed to pass to CreateHashContext and IHashContext::Duplicate
    size_t outputBufferSize;       ///< size of buffer needed to pass to IHashContext::Finish
};

/// Get the memory sizes for a hash algorithm
///
/// @param [in]  algorithm  Enumeration id for the desired hashing method
/// @param [out] pInfo      Information regarding necessary memory sizes for a hash algorithm
///
/// @returns Success if the size information was retrieved. Otherwise, one of the following errors may be returned:
///          + NotFound if no provider was found for the requested algorithm.
///          + ErrorInvalidPointer if pInfo is nullptr.
///          + ErrorInvalidValue if algorithm was unrecognized.
Result GetHashContextInfo(
    HashAlgorithm    algorithm,
    HashContextInfo* pInfo);

/// Create a OS context suitable for hashing data if available.
///
/// This function may cause the OS to allocate memory or load additional libraries. If a provider is already loaded
/// that satisfies the request the existing provider may be internally re-used. In this case the provider and any
/// required libraries will remain loaded until the module is unloaded. Due to the potential OS overhead it is
/// not recommended to call this function multiple times upon failure.
///
/// @param [in]     algorithm      Enumeration id for the desired hashing method
/// @param [in]     pPlacementAddr Pointer to the location where the interface should be constructed. There must
///                                be as much size available here as reported by HashSizeInfo::contextObjectSize.
///                                The pointer also must fulfill the reported alignment requirements.
/// @param [out]    ppHashContext  Hash context interface. On failure this value will be set to nullptr.
///
/// @returns Success if the context is available for use. Otherwise, one of the following errors may be returned:
///          + NotFound if no provider was found for the requested algorithm.
///          + ErrorInvalidPointer if pPlacementAddr or ppHashContext is nullptr.
///          + ErrorInvalidValue if algorithm was unrecognized.
///          + ErrorUnavailable if the OS library could not be loaded.
///          + ErrorOutOfMemory when there is not enough system memory to create the OS internal provider object.
///          + ErrorInitializationFailed if provider object failed to initialize.
///          + ErrorUnknown if there is an internal error.
Result CreateHashContext(
    HashAlgorithm   algorithm,
    void*           pPlacementAddr,
    IHashContext**  ppHashContext);

/**
***********************************************************************************************************************
* @brief Interface representing an multi-stage hash calculation. No thread safety is implied.
***********************************************************************************************************************
*/
class IHashContext
{
public:
    /// Hash additional data into the context
    ///
    /// @param [in] pData       Data to be hashed.
    /// @param [in] dataSize    Size of the data block to be hashed.
    ///
    /// @return Success if the hash data was updated. Otherwise, one of the following may be returned:
    ///         + ErrorInvalidPointer if pData is nullptr.
    ///         + ErrorUnavailable if the context object may not be used for additional operations.
    ///         + ErrorUnknown if there is an internal error.
    virtual Result AddData(
        const void* pData,
        size_t      dataSize) = 0;

    /// Get the size of the final hash output for this context
    ///
    /// @return size of the buffer needed to pass to Finish()
    virtual size_t GetOutputBufferSize() const = 0;

    /// Finalize the hash and get the resulting digest
    ///
    /// @param [out] pBuffer    The resulting hash output. There must be as much size available here as reported by
    ///                         calling GetOutputBufferSize().
    ///
    /// @return Success if the hash was returned. Otherwise, one of the following may be returned:
    ///         + ErrorInvalidPointer if pBuffer is nullptr.
    ///         + ErrorUnavailable if the context object may not be used for additional operations.
    ///         + ErrorUnknown if there is an internal error.
    virtual Result Finish(
        void*   pOutput) = 0;

    /// Reset the context for re-use without getting the result buffer
    ///
    /// @return Success if the context was reset. Otherwise, one of the following may be returned:
    ///         + ErrorUnknown if there is an internal error.
    virtual Result Reset() = 0;

    /// Get the memory size needed to duplicate this context
    ///
    /// @return size of buffer needed to pass into Duplicate()
    virtual size_t GetDuplicateObjectSize() const = 0;

    /// Duplicate an existing hash context with its current state
    ///
    /// @param [in]  pPlacementAddr     Pointer to the location where the object should be constructed. There must
    ///                                 be as much size available here as reported by calling GetDuplicateObjectSize().
    /// @param [out] ppDuplicatedObject Constructed hash context object. When successful, the returned address will be
    ///                                 the same as specified in pPlacementAddr.
    ///
    /// @return Success if the hash object was duplicated. Otherwise, one of the following may be returned:
    ///         + Unsuppported if the context object may not be duplicated.
    ///         + ErrorInvalidPointer if pPlacementAddr or ppDuplicatedObject is nullptr.
    ///         + ErrorOutOfMemory when there is not enough system memory to create a hash context.
    ///         + ErrorUnknown if there is an internal error.
    virtual Result Duplicate(
        void*           pPlacementAddr,
        IHashContext**  ppDuplicatedObject) const = 0;

    /// Detroy the hash context object. Object is no longer usable after calling this function.
    virtual void Destroy() = 0;

protected:
    IHashContext() {}
    virtual ~IHashContext() {}

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(IHashContext);
};

} //namespace Util
