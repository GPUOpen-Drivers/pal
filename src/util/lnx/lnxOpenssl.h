/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#pragma once

#include "util/lnx/lnxHashProvider.h"
#include "palHashProvider.h"
#include "palLibrary.h"
#include "palMutex.h"

namespace Util
{
typedef int32 (*Md5Init)(MD5_CTX *pCtx);
typedef int32 (*Md5Update)(MD5_CTX *pCtx, const void *data, size_t len);
typedef int32 (*Md5Final)(uint8 *md, MD5_CTX *pCtx);
typedef int32 (*Sha1Init)(SHA_CTX *pCtx);
typedef int32 (*Sha1Update)(SHA_CTX *pCtx, const void *data, size_t len);
typedef int32 (*Sha1Final)(uint8 *md, SHA_CTX *pCtx);
typedef int32 (*Sha224Init)(SHA256_CTX *pCtx);
typedef int32 (*Sha224Update)(SHA256_CTX *pCtx, const void *data, size_t len);
typedef int32 (*Sha224Final)(uint8 *md, SHA256_CTX *pCTx);
typedef int32 (*Sha256Init)(SHA256_CTX *pCtx);
typedef int32 (*Sha256Update)(SHA256_CTX *pCtx, const void *data, size_t len);
typedef int32 (*Sha256Final)(uint8 *md, SHA256_CTX *c);
typedef int32 (*Sha384Init)(SHA512_CTX *pCtx);
typedef int32 (*Sha384Update)(SHA512_CTX *pCtx, const void *data, size_t len);
typedef int32 (*Sha384Final)(uint8 *md, SHA512_CTX *pCtx);
typedef int32 (*Sha512Init)(SHA512_CTX *pCtx);
typedef int32 (*Sha512Update)(SHA512_CTX *pCtx, const void *data, size_t len);
typedef int32 (*Sha512Final)(uint8 *md, SHA512_CTX *pCtx);

struct HashFuncs
{
    Md5Init         pfnMd5Init;
    Md5Update       pfnMd5Update;
    Md5Final        pfnMd5Final;
    Sha1Init        pfnSha1Init;
    Sha1Update      pfnSha1Update;
    Sha1Final       pfnSha1Final;
    Sha224Init      pfnSha224Init;
    Sha224Update    pfnSha224Update;
    Sha224Final     pfnSha224Final;
    Sha256Init      pfnSha256Init;
    Sha256Update    pfnSha256Update;
    Sha256Final     pfnSha256Final;
    Sha384Init      pfnSha384Init;
    Sha384Update    pfnSha384Update;
    Sha384Final     pfnSha384Final;
    Sha512Init      pfnSha512Init;
    Sha512Update    pfnSha512Update;
    Sha512Final     pfnSha512Final;
};
static HashFuncs hashFuncs;

// =====================================================================================================================
// Interface for openssl library.
class OpenSslLib
{
public:
    struct ProviderInfo
    {
        size_t            objectSize;
        size_t            hashSize;
    };

    static Result OpenLibrary(OpenSslLib** ppLib);

    Result GetProviderInfo(HashAlgorithm algorithmId, ProviderInfo* pInfo);

    static Result CreateHash(
        ShaContext*     pContext,
        HashAlgorithm   algorithm,
        void*           pWorkMem,
        size_t*         pSize);

    static void DestroyHash(ShaHandle* pShaHandle);

    static Result UpdateHash(ShaHandle* pShaHandle, const void* pInput, size_t inputSize);

    static Result FinishHash(ShaHandle* pShaHandle, void* pOutput);

    static Result Reset(ShaHandle* pShaHandle);

    static Result DuplicateHash(const ShaHandle* pShaHandle, void* pWorkBuffer, ShaContext* pDuplicate);

    static size_t GetHashSize(const ShaHandle* pShaHandle);

private:
    OpenSslLib();
    ~OpenSslLib();

    bool    Valid() const { return m_lib.IsLoaded(); }
    Result  Init();

    Mutex    m_mutex;
    Library  m_lib;

    PAL_DISALLOW_COPY_AND_ASSIGN(OpenSslLib);
};

} // namespace Util
