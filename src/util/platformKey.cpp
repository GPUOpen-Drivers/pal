/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "platformKey.h"
#include "palHashProvider.h"
#include "palAssert.h"
#include "palMetroHash.h"
#include "palSysMemory.h"

namespace Util
{

// =====================================================================================================================
// Calculate the memory needed for PlatformKey so that no additional memory is needed
size_t GetPlatformKeySize(
    HashAlgorithm algorithm)
{
    HashContextInfo info   = {};
    Result          result = GetHashContextInfo(algorithm, &info);

    PAL_ALERT(IsErrorResult(result));

    // Extra context memory for refreshing our key
    size_t size = sizeof(PlatformKey) + info.outputBufferSize;
    for (uint32 i = 0; i < 2; i++)
    {
        size = Pow2Align(size, info.contextObjectAlignment);
        size += info.contextObjectSize;
    }
    return size;
}

// =====================================================================================================================
// Construct and initialize a PlatformKey object
Result CreatePlatformKey(
    HashAlgorithm   algorithm,
    void*           pInitialData,
    size_t          initialDataSize,
    void*           pPlacementAddr,
    IPlatformKey**  ppPlatformKey)
{
    PAL_ASSERT((pInitialData == nullptr) || (initialDataSize > 0));
    PAL_ASSERT(pPlacementAddr != nullptr);
    PAL_ASSERT(ppPlatformKey != nullptr);

    Result          result          = Result::Success;
    uint8*          pKeyMem         = static_cast<uint8*>(VoidPtrInc(pPlacementAddr, sizeof(PlatformKey)));
    IHashContext*   pContext        = nullptr;
    void*           pTempContextMem = nullptr;
    HashContextInfo info            = {};

    if ((pPlacementAddr == nullptr) ||
        (ppPlatformKey == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }

    if (result == Result::Success)
    {
        if ((pInitialData != nullptr) &&
            (initialDataSize == 0))
        {
            result = Result::ErrorInvalidValue;
        }
    }

    if (result == Result::Success)
    {
        result = GetHashContextInfo(algorithm, &info);
    }

    if (result == Result::Success)
    {
        void* pContextMem = VoidPtrInc(pKeyMem, info.outputBufferSize);
        pContextMem       = VoidPtrAlign(pContextMem, info.contextObjectAlignment);
        pTempContextMem   = VoidPtrInc(pContextMem, info.contextObjectSize);
        pTempContextMem   = VoidPtrAlign(pTempContextMem, info.contextObjectAlignment);

        result = CreateHashContext(algorithm, pContextMem, &pContext);
    }

    if (result == Result::Success)
    {
        PlatformKey* pKeyObj = PAL_PLACEMENT_NEW(pPlacementAddr) PlatformKey(pContext, pKeyMem, pTempContextMem);

        if (pInitialData != nullptr)
        {
            result = pKeyObj->AppendClientData(pInitialData, initialDataSize);
        }

        if (result == Result::Success)
        {
            *ppPlatformKey = pKeyObj;
        }
        else
        {
            pKeyObj->Destroy();
            result         = Result::ErrorInitializationFailed;
            *ppPlatformKey = nullptr;
        }
    }
    else
    {
        if (pContext != nullptr)
        {
            pContext->Destroy();
        }
    }

    return result;
}

// =====================================================================================================================
PlatformKey::PlatformKey(
    IHashContext*  pKeyContext,
    uint8*         pKeyData,
    void*          pTempContextMem)
    :
    m_pKeyContext     { pKeyContext },
    m_pKeyData        { pKeyData },
    m_pTempContextMem { pTempContextMem }
{
    PAL_ASSERT(m_pKeyContext != nullptr);
    PAL_ASSERT(m_pKeyData != nullptr);
    PAL_ASSERT(m_pTempContextMem != nullptr);
}

// =====================================================================================================================
PlatformKey::~PlatformKey()
{
    m_pKeyContext->Destroy();
}

// =====================================================================================================================
// Add the client data to the context and re-hash
Result PlatformKey::AppendClientData(
    const void* pData,
    size_t      dataSize)
{
    Result result = m_pKeyContext->AddData(pData, dataSize);

    if (result == Result::Success)
    {
        result = RecalcKey();
    }

    return result;
}

// =====================================================================================================================
// Refresh our key-buffer
Result PlatformKey::RecalcKey()
{
    IHashContext* pTemp  = nullptr;
    Result        result = m_pKeyContext->Duplicate(m_pTempContextMem, &pTemp);

    if (result == Result::Success)
    {
        result = pTemp->Finish(m_pKeyData);
        pTemp->Destroy();
    }

    if (result == Result::Success)
    {
        MetroHash64::Hash(m_pKeyData, GetKeySize(), reinterpret_cast<uint8*>(&m_keyData64));
    }

    return result;
}

} //namespace Util
