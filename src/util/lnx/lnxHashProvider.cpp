/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  lnxHashProvider.cpp
* @brief Linux implementation of HashProvider and HashContext.
***********************************************************************************************************************
*/

#include "util/lnx/lnxHashProvider.h"
#include "util/lnx/lnxOpenssl.h"

#include "palAssert.h"
#include "palInlineFuncs.h"
#include "palSysMemory.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

namespace Util
{

// =====================================================================================================================
HashContext::HashContext(
    ShaContext           hContext,
    HashAlgorithm        algorithm,
    size_t               objectSize)
    :
    m_contextObjectSize { objectSize }
{
    m_handle.context.pMd5   = hContext.pMd5;
    m_handle.algorithm      = algorithm;
    PAL_ASSERT(hContext.pMd5 != nullptr);
}

// =====================================================================================================================
HashContext::~HashContext()
{
    OpenSslLib::DestroyHash(&m_handle);
}

// =====================================================================================================================
// Append data to the end of the hash state
Result HashContext::AddData(
    const void* pData,
    size_t      dataSize)
{
    return OpenSslLib::UpdateHash(&m_handle, pData, dataSize);
}

// =====================================================================================================================
// Copy the result result hash to the buffer provided
Result HashContext::Finish(
    void *pOutput)
{
    return OpenSslLib::FinishHash(&m_handle, pOutput);
}

// =====================================================================================================================
// Re-initialize context state for reuse
Result HashContext::Reset()
{
    return OpenSslLib::Reset(&m_handle);
}

// =====================================================================================================================
// Clone the current hashing state to a new object
Result HashContext::Duplicate(
    void*           pPlacementAddr,
    IHashContext**  ppDuplicatedObject
    ) const
{
    Result result = Result::Success;

    if ((pPlacementAddr != nullptr) &&
        (ppDuplicatedObject != nullptr))
    {
        ShaContext hDuplicate = { };
        void*      pWorkBuffer = VoidPtrInc(pPlacementAddr, sizeof(HashContext));

        result = OpenSslLib::DuplicateHash(&m_handle, pWorkBuffer, &hDuplicate);

        if (result == Result::Success)
        {
            PAL_ALERT(hDuplicate.pMd5 == nullptr);

            *ppDuplicatedObject = PAL_PLACEMENT_NEW(pPlacementAddr) HashContext(hDuplicate, m_handle.algorithm,
                    m_contextObjectSize);
        }
    }
    else
    {
        PAL_ALERT(pPlacementAddr == nullptr);
        PAL_ALERT(ppDuplicatedObject == nullptr);

        result = Result::ErrorInvalidPointer;
    }

    return result;
}

// =====================================================================================================================
// Return information about the hash size
size_t HashContext::GetOutputBufferSize() const
{
    return OpenSslLib::GetHashSize(&m_handle);
}

// =====================================================================================================================
// Return information about the hash context memory sizes
Result GetHashContextInfo(
    HashAlgorithm    algorithm,
    HashContextInfo* pInfo)
{
    size_t     size    = 0;
    OpenSslLib* pOpenssl = nullptr;
    Result     result  = OpenSslLib::OpenLibrary(&pOpenssl);

    if ((result == Result::Success) &&
        (pOpenssl != nullptr))
    {
        OpenSslLib::ProviderInfo providerInfo = {};

        result = pOpenssl->GetProviderInfo(algorithm, &providerInfo);

        if (IsErrorResult(result) == false)
        {
            pInfo->contextObjectSize = providerInfo.objectSize + sizeof(HashContext);
            pInfo->outputBufferSize  = providerInfo.hashSize;
        }
    }

    return result;

}

// =====================================================================================================================
// Create a OS hashing context
Result CreateHashContext(
    HashAlgorithm   algorithm,
    void*           pPlacementAddr,
    IHashContext**  ppHashContext)
{
    PAL_ASSERT(pPlacementAddr != nullptr);
    PAL_ASSERT(ppHashContext != nullptr);

    Result result = Result::Success;

    if ((pPlacementAddr == nullptr) ||
        (ppHashContext == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }

    OpenSslLib* pOpenssl = nullptr;

    if (result == Result::Success)
    {
        result = OpenSslLib::OpenLibrary(&pOpenssl);
    }

    ShaContext hContext = { };
    size_t     objectSize;

    if (result == Result::Success)
    {
        void* pWorkBuffer = VoidPtrInc(pPlacementAddr, sizeof(HashContext));
        result            = OpenSslLib::CreateHash(
            &hContext,
            algorithm,
            pWorkBuffer,
            &objectSize);
    }

    if (result == Result::Success)
    {
        PAL_ALERT(hContext.pMd5 == nullptr);

        *ppHashContext = PAL_PLACEMENT_NEW(pPlacementAddr) HashContext(hContext, algorithm, objectSize);
    }
    else
    {
        if (ppHashContext != nullptr)
        {
            *ppHashContext = nullptr;
        }
    }

    return result;
}

} // namespace Util
