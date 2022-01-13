/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  lnxHashProvider.h
* @brief Linux header file for HashProvider and HashContext class declarations
***********************************************************************************************************************
*/

#pragma once

#include "palHashProvider.h"

#include <openssl/sha.h>
#include <openssl/md5.h>
#include <openssl/crypto.h>

namespace Util
{

union ShaContext {
    MD5_CTX     *pMd5;
    SHA_CTX     *pSha;
    SHA256_CTX  *pSha256;
    SHA512_CTX  *pSha512;
};

struct ShaHandle
{
    ShaContext      context;
    HashAlgorithm   algorithm;
};

// =====================================================================================================================
// Implementation of wrapped hashing context
class HashContext : public IHashContext
{
public:
    HashContext(
        ShaContext           hContext,
        HashAlgorithm        algorithm,
        size_t               objectSize);
    ~HashContext();

    virtual Result AddData(
        const void* pData,
        size_t      dataSize);

    virtual size_t GetOutputBufferSize() const;

    virtual Result Finish(
        void*   pOutput);

    virtual Result Reset();

    virtual size_t GetDuplicateObjectSize() const { return m_contextObjectSize + sizeof(HashContext); }

    virtual Result Duplicate(
        void*           pPlacementAddr,
        IHashContext**  ppDuplicatedObject) const;

    virtual void Destroy() { this->~HashContext(); }

private:
    PAL_DISALLOW_DEFAULT_CTOR(HashContext);
    PAL_DISALLOW_COPY_AND_ASSIGN(HashContext);

    ShaHandle             m_handle;
    size_t                m_contextObjectSize;
};

} //namespace Util
