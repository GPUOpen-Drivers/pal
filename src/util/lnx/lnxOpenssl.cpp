/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "util/lnx/lnxOpenssl.h"
#include "palAssert.h"
#include "palHashProvider.h"
#include "palMutex.h"
#include "palUtil.h"

#include <limits.h>

namespace Util
{
// =====================================================================================================================
// Open Openssl.so if needed from the OS and report the availability of our functions
//
// Note: Once loaded we do not unload Openssl until the driver itself is unloaded
Result OpenSslLib::OpenLibrary(
    OpenSslLib**    ppLib)
{
    // Local static: will be initialized on first call and destroyed when driver is unloaded. Manages the lifetime of
    //               Openssl handle so we don't leak every time we get reloaded.
    static OpenSslLib s_opensslLib;

    Result result = Result::ErrorUnknown;

    if (s_opensslLib.Valid())
    {
        *ppLib = &s_opensslLib;
        result = Result::Success;
    }
    else
    {
        result = s_opensslLib.Init();
        PAL_ALERT(IsErrorResult(result));

        if (result == Result::Success)
        {
            *ppLib = &s_opensslLib;
        }
    }

    return result;
}

// =====================================================================================================================
OpenSslLib::OpenSslLib()
    :
    m_mutex {}
{
}

// =====================================================================================================================
OpenSslLib::~OpenSslLib()
{
}

// =====================================================================================================================
// Thread-safe Init function
Result OpenSslLib::Init()
{
    Result result = Result::Success;

    MutexAuto lock { &m_mutex };

    if (Valid() == false)
    {
        Library library;
        // Note: If OS doesn't install libssl-dev, there could be no libssl.so link.
        // Try and open versioned ones directly.
        constexpr const char* libSslNames[] =
        {
            "libssl.so",
            "libssl.so.1.1", // SONAME for OpenSSL v1.1.1
            "libssl.so.3",   // SONAME for OpenSSL v3.0.2
                             // Default version installed on Ubuntu 22.04
            "libssl.so.10",  // SONAME for Fedora/Redhat/CentOS OpenSSL v1.0.x
        };

        for (const char* libSslName : libSslNames)
        {
            if (library.Load(libSslName) == Result::Success)
            {
                break;
            }
        }

        if (library.IsLoaded())
        {
            // WARNING: When adding new functions, please double check the API compatiblity for all our supported
            // OpenSSL versions
            if (library.GetFunction("MD5_Init",      &hashFuncs.pfnMd5Init)      &&
                library.GetFunction("MD5_Update",    &hashFuncs.pfnMd5Update)    &&
                library.GetFunction("MD5_Final",     &hashFuncs.pfnMd5Final)     &&
                library.GetFunction("SHA1_Init",     &hashFuncs.pfnSha1Init)     &&
                library.GetFunction("SHA1_Update",   &hashFuncs.pfnSha1Update)   &&
                library.GetFunction("SHA1_Final",    &hashFuncs.pfnSha1Final)    &&
                library.GetFunction("SHA224_Init",   &hashFuncs.pfnSha224Init)   &&
                library.GetFunction("SHA224_Update", &hashFuncs.pfnSha224Update) &&
                library.GetFunction("SHA224_Final",  &hashFuncs.pfnSha224Final)  &&
                library.GetFunction("SHA256_Init",   &hashFuncs.pfnSha256Init)   &&
                library.GetFunction("SHA256_Update", &hashFuncs.pfnSha256Update) &&
                library.GetFunction("SHA256_Final",  &hashFuncs.pfnSha256Final)  &&
                library.GetFunction("SHA384_Init",   &hashFuncs.pfnSha384Init)   &&
                library.GetFunction("SHA384_Update", &hashFuncs.pfnSha384Update) &&
                library.GetFunction("SHA384_Final",  &hashFuncs.pfnSha384Final)  &&
                library.GetFunction("SHA512_Init",   &hashFuncs.pfnSha512Init)   &&
                library.GetFunction("SHA512_Update", &hashFuncs.pfnSha512Update) &&
                library.GetFunction("SHA512_Final",  &hashFuncs.pfnSha512Final))
            {
                m_lib.Swap(&library);
            }
            else
            {
                PAL_ALERT_ALWAYS_MSG("One or more function lookups in libssl.so failed");
                result = Result::ErrorInitializationFailed;
            }
        }
        else
        {
            result =  Result::ErrorUnavailable;
        }
    }

    return result;
}

// =====================================================================================================================
Result OpenSslLib::GetProviderInfo(
    HashAlgorithm  algorithmId,
    ProviderInfo*  pInfo)
{
    PAL_ASSERT(pInfo != nullptr);

    size_t     size    = 0;
    Result result = Result::Success;

    switch (algorithmId)
    {
        case HashAlgorithm::Md5:
            pInfo->objectSize = sizeof(MD5_CTX);
            pInfo->hashSize   = MD5_DIGEST_LENGTH;
            break;
        case HashAlgorithm::Sha1:
            pInfo->objectSize = sizeof(SHA_CTX);
            pInfo->hashSize   = SHA_DIGEST_LENGTH;
            break;
        case HashAlgorithm::Sha224:
            pInfo->objectSize = sizeof(SHA256_CTX);
            pInfo->hashSize   = SHA224_DIGEST_LENGTH;
            break;
        case HashAlgorithm::Sha256:
            pInfo->objectSize = sizeof(SHA256_CTX);
            pInfo->hashSize   = SHA256_DIGEST_LENGTH;
            break;
        case HashAlgorithm::Sha384:
            pInfo->objectSize = sizeof(SHA512_CTX);
            pInfo->hashSize   = SHA384_DIGEST_LENGTH;
            break;
        case HashAlgorithm::Sha512:
            pInfo->objectSize = sizeof(SHA512_CTX);
            pInfo->hashSize   = SHA512_DIGEST_LENGTH;
            break;
        default:
            result = Result::ErrorInvalidValue;
    }

    return result;
}

// =====================================================================================================================
// create hash context
Result OpenSslLib::CreateHash(
    ShaContext*     pContext,
    HashAlgorithm   algorithm,
    void*           pWorkMem,
    size_t*         pObjectSize)
{
    Result result = Result::Success;

    switch (algorithm)
    {
        case HashAlgorithm::Md5:
            pContext->pMd5 = static_cast<MD5_CTX *>(pWorkMem);
            hashFuncs.pfnMd5Init(pContext->pMd5);
            *pObjectSize = sizeof(MD5_CTX);
            break;
        case HashAlgorithm::Sha1:
            pContext->pSha = static_cast<SHA_CTX *>(pWorkMem);
            hashFuncs.pfnSha1Init(pContext->pSha);
            *pObjectSize = sizeof(SHA_CTX);
            break;
        case HashAlgorithm::Sha224:
            pContext->pSha256 = static_cast<SHA256_CTX *>(pWorkMem);
            hashFuncs.pfnSha224Init(pContext->pSha256);
            *pObjectSize = sizeof(SHA256_CTX);
            break;
        case HashAlgorithm::Sha256:
            pContext->pSha256 = static_cast<SHA256_CTX *>(pWorkMem);
            hashFuncs.pfnSha256Init(pContext->pSha256);
            *pObjectSize = sizeof(SHA256_CTX);
            break;
        case HashAlgorithm::Sha384:
            pContext->pSha512 = static_cast<SHA512_CTX *>(pWorkMem);
            hashFuncs.pfnSha384Init(pContext->pSha512);
            *pObjectSize = sizeof(SHA512_CTX);
            break;
        case HashAlgorithm::Sha512:
            pContext->pSha512 = static_cast<SHA512_CTX *>(pWorkMem);
            hashFuncs.pfnSha512Init(pContext->pSha512);
            *pObjectSize = sizeof(SHA512_CTX);
            break;
        default:
            result = Result::ErrorInvalidValue;
            break;
    }

    return result;
}

// =====================================================================================================================
void OpenSslLib::DestroyHash(
    ShaHandle* pShaHandle)
{
    PAL_ASSERT(pShaHandle         != nullptr);
    switch (pShaHandle->algorithm)
    {
        case HashAlgorithm::Md5:
            memset(pShaHandle->context.pMd5, 0, sizeof(MD5_CTX));
            break;
        case HashAlgorithm::Sha1:
            memset(pShaHandle->context.pSha, 0, sizeof(SHA_CTX));
            break;
        case HashAlgorithm::Sha224:
        case HashAlgorithm::Sha256:
            memset(pShaHandle->context.pSha256, 0, sizeof(SHA256_CTX));
            break;
        case HashAlgorithm::Sha384:
        case HashAlgorithm::Sha512:
            memset(pShaHandle->context.pSha512, 0, sizeof(SHA512_CTX));
            break;
        default:
            break;
    }

}

// =====================================================================================================================
// Wrapper for Openssl update HashData
Result OpenSslLib::UpdateHash(
    ShaHandle*          pShaHandle,
    const void*         pData,
    size_t              dataSize)
{
    Result result = Result::Success;

    switch (pShaHandle->algorithm)
    {
        case HashAlgorithm::Md5:
            hashFuncs.pfnMd5Update(pShaHandle->context.pMd5, pData, dataSize);
            break;
        case HashAlgorithm::Sha1:
            hashFuncs.pfnSha1Update(pShaHandle->context.pSha, pData, dataSize);
            break;
        case HashAlgorithm::Sha224:
            hashFuncs.pfnSha224Update(pShaHandle->context.pSha256, pData, dataSize);
            break;
        case HashAlgorithm::Sha256:
            hashFuncs.pfnSha256Update(pShaHandle->context.pSha256, pData, dataSize);
            break;
        case HashAlgorithm::Sha384:
            hashFuncs.pfnSha384Update(pShaHandle->context.pSha512, pData, dataSize);
            break;
        case HashAlgorithm::Sha512:
            hashFuncs.pfnSha512Update(pShaHandle->context.pSha512, pData, dataSize);
            break;
        default:
                result = Result::ErrorInvalidValue;
    }

    return result;
}

// =====================================================================================================================
// Wrapper for Openssl finish Hash
Result OpenSslLib::FinishHash(
    ShaHandle*          pShaHandle,
    void*               pOutput)
{
    uint8 *pAsUint8 = static_cast<uint8*>(pOutput);
    Result result = Result::Success;

    switch (pShaHandle->algorithm)
    {
        case HashAlgorithm::Md5:
            hashFuncs.pfnMd5Final(pAsUint8, pShaHandle->context.pMd5);
            break;
        case HashAlgorithm::Sha1:
            hashFuncs.pfnSha1Final(pAsUint8, pShaHandle->context.pSha);
            break;
        case HashAlgorithm::Sha224:
            hashFuncs.pfnSha224Final(pAsUint8, pShaHandle->context.pSha256);
            break;
        case HashAlgorithm::Sha256:
            hashFuncs.pfnSha256Final(pAsUint8, pShaHandle->context.pSha256);
            break;
        case HashAlgorithm::Sha384:
            hashFuncs.pfnSha384Final(pAsUint8, pShaHandle->context.pSha512);
            break;
        case HashAlgorithm::Sha512:
            hashFuncs.pfnSha512Final(pAsUint8, pShaHandle->context.pSha512);
            break;
        default:
            result = Result::ErrorInvalidValue;
    }

    return result;
}

// =====================================================================================================================
// context reset
Result OpenSslLib::Reset(
    ShaHandle*          pShaHandle)
{
    Result result = Result::Success;

    switch (pShaHandle->algorithm)
    {
        case HashAlgorithm::Md5:
            memset(pShaHandle->context.pMd5, 0, sizeof(MD5_CTX));
            hashFuncs.pfnMd5Init(pShaHandle->context.pMd5);
            break;
        case HashAlgorithm::Sha1:
            memset(pShaHandle->context.pSha, 0, sizeof(SHA_CTX));
            hashFuncs.pfnSha1Init(pShaHandle->context.pSha);
            break;
        case HashAlgorithm::Sha224:
            memset(pShaHandle->context.pSha256, 0, sizeof(SHA256_CTX));
            hashFuncs.pfnSha224Init(pShaHandle->context.pSha256);
            break;
        case HashAlgorithm::Sha256:
            memset(pShaHandle->context.pSha256, 0, sizeof(SHA256_CTX));
            hashFuncs.pfnSha256Init(pShaHandle->context.pSha256);
            break;
        case HashAlgorithm::Sha384:
            memset(pShaHandle->context.pSha512, 0, sizeof(SHA512_CTX));
            hashFuncs.pfnSha384Init(pShaHandle->context.pSha512);
            break;
        case HashAlgorithm::Sha512:
            memset(pShaHandle->context.pSha512, 0, sizeof(SHA512_CTX));
            hashFuncs.pfnSha512Init(pShaHandle->context.pSha512);
            break;
        default:
                result = Result::ErrorInvalidValue;
    }

    return result;
}

// =====================================================================================================================
// Duplicate Hash
Result OpenSslLib::DuplicateHash(
    const ShaHandle*      pShaHandle,
    void*                 pWorkBuffer,
    ShaContext*           pDuplicate
    )
{
    Result result = Result::Success;

    switch (pShaHandle->algorithm)
    {
        case HashAlgorithm::Md5:
            pDuplicate->pMd5 = static_cast<MD5_CTX *>(pWorkBuffer);
            memcpy(pDuplicate->pMd5, pShaHandle->context.pMd5, sizeof(MD5_CTX));
            break;
        case HashAlgorithm::Sha1:
            pDuplicate->pSha = static_cast<SHA_CTX *>(pWorkBuffer);
            memcpy(pDuplicate->pSha, pShaHandle->context.pSha, sizeof(SHA_CTX));
            break;
        case HashAlgorithm::Sha224:
        case HashAlgorithm::Sha256:
            pDuplicate->pSha256 = static_cast<SHA256_CTX *>(pWorkBuffer);
            memcpy(pDuplicate->pSha256, pShaHandle->context.pSha256, sizeof(SHA256_CTX));
            break;
        case HashAlgorithm::Sha384:
        case HashAlgorithm::Sha512:
            pDuplicate->pSha512 = static_cast<SHA512_CTX *>(pWorkBuffer);
            memcpy(pDuplicate->pSha512, pShaHandle->context.pSha512, sizeof(SHA512_CTX));
            break;
        default:
            result = Result::ErrorInvalidValue;
            break;
    }

    return result;
}

// =====================================================================================================================
// Get Hash size
size_t OpenSslLib::GetHashSize(
    const ShaHandle*      pShaHandle)
{
    size_t size = 0;

    switch (pShaHandle->algorithm)
    {
        case HashAlgorithm::Md5:
            size  = MD5_DIGEST_LENGTH;
            break;
        case HashAlgorithm::Sha1:
            size  = SHA_DIGEST_LENGTH;
            break;
        case HashAlgorithm::Sha224:
            size  = SHA224_DIGEST_LENGTH;
            break;
        case HashAlgorithm::Sha256:
            size  = SHA256_DIGEST_LENGTH;
            break;
        case HashAlgorithm::Sha384:
            size  = SHA384_DIGEST_LENGTH;
            break;
        case HashAlgorithm::Sha512:
            size  = SHA512_DIGEST_LENGTH;
            break;
        default:
            size = 0;
    }

    return size;
}

} //namespace Util
