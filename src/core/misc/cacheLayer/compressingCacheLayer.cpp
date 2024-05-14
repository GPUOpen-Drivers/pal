/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "compressingCacheLayer.h"

#include "palSysMemory.h"

#include "core/platform.h"

#include <limits.h>

namespace Util
{

// =====================================================================================================================
CompressingCacheLayer::CompressingCacheLayer(
    const AllocCallbacks& callbacks,
    bool useHighCompression,
    bool decompressOnly)
    : m_compressor(useHighCompression)
    , m_allocator(callbacks)
    , m_pNextLayer(nullptr)
    , m_decompressOnly(decompressOnly)
{
    // Alloc and Free MUST NOT be nullptr
    PAL_ASSERT(callbacks.pfnAlloc != nullptr);
    PAL_ASSERT(callbacks.pfnFree != nullptr);
}

// =====================================================================================================================
CompressingCacheLayer::~CompressingCacheLayer()
{
}

// =====================================================================================================================
// Pass a query to the next layer.
Result CompressingCacheLayer::Query(
    const Hash128* pHashId,
    uint32         policy,
    uint32         flags,
    QueryResult*   pQuery)
{
    Result result = Result::ErrorUnknown;

    PAL_ASSERT(m_pNextLayer != nullptr);

    if (m_pNextLayer == nullptr)
    {
        result = Result::ErrorUnavailable;
    }
    else
    {
        result = m_pNextLayer->Query(pHashId, policy, flags, pQuery);
        // After this layer, any promotion will store the decompressed size.
        pQuery->promotionSize = pQuery->dataSize;
    }

    return result;
}

// =====================================================================================================================
// Validate inputs, then compress and store to the next layer.
Result CompressingCacheLayer::Store(
    Util::StoreFlags    storeFlags,
    const Hash128*      pHashId,
    const void*         pData,
    size_t              dataSize,
    size_t              storeSize)
{
    Result result = Result::ErrorUnknown;

    if ((m_decompressOnly == true)
        || (storeFlags.enableCompression == false)
        )
    {
        // Just pass the store through to the next layer.
        result = m_pNextLayer->Store(
            storeFlags,
            pHashId,
            pData,
            dataSize,
            storeSize);
    }
    else
    {
        if (storeSize == 0)
        {
            storeSize = dataSize;
        }

        PAL_ASSERT(m_pNextLayer != nullptr);
        PAL_ASSERT(storeSize == dataSize);

        if (storeSize != dataSize)
        {
            // This signifies that there is more than one compression layer in the chain.
            // We don't support that.
            result = Result::ErrorInvalidValue;
        }
        else if (m_pNextLayer == nullptr)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            PAL_ASSERT(storeSize <= INT_MAX);
            int neededSize = m_compressor.GetCompressBound(int(storeSize));

            void* compressedBuffer = PAL_MALLOC(neededSize, &m_allocator, AllocInternalTemp);
            if (compressedBuffer == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                int bytesWritten = 0;
                result = m_compressor.Compress(static_cast<const char*>(pData),
                                               static_cast<char*>(compressedBuffer),
                                               int(storeSize),
                                               neededSize,
                                               &bytesWritten);

                if((result == Result::Success) && (bytesWritten > 0) && (size_t(bytesWritten) < dataSize))
                {
                    // Store the compressed version.
                    result = m_pNextLayer->Store(
                        storeFlags,
                        pHashId,
                        compressedBuffer,
                        storeSize,
                        bytesWritten);
                }
                else
                {
                    // There was some sort of problem during compression... just store the uncompressed version.
                    result = m_pNextLayer->Store(
                        storeFlags,
                        pHashId,
                        pData,
                        dataSize,
                        storeSize);
                }

                PAL_SAFE_FREE(compressedBuffer, &m_allocator);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Validate inputs, then load data from our layer
Result CompressingCacheLayer::Load(
    const QueryResult* pQuery,
    void*              pBuffer)
{
    Result result = Result::ErrorUnknown;

    PAL_ASSERT(m_pNextLayer != nullptr);

    if (m_pNextLayer == nullptr)
    {
        result = Result::ErrorUnavailable;
    }
    else
    {
        void* compressedBuffer = PAL_MALLOC(pQuery->storeSize, &m_allocator, AllocInternalTemp);
        if (compressedBuffer == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            result = m_pNextLayer->Load(pQuery, compressedBuffer);

            if (result == Result::Success)
            {
                PAL_ASSERT(pQuery->storeSize <= INT_MAX);
                PAL_ASSERT(pQuery->dataSize  <= INT_MAX);
                int neededSize = m_compressor.GetDecompressedSize(static_cast<const char*>(compressedBuffer),
                                                                  static_cast<int>(pQuery->storeSize));
                if (neededSize > 0)
                {
                    // Decompress the data
                    PAL_ASSERT(static_cast<size_t>(neededSize) == pQuery->dataSize);

                    int bytesWritten = 0;
                    result = m_compressor.Decompress(static_cast<const char*>(compressedBuffer),
                                                     static_cast<char*>(pBuffer),
                                                     static_cast<int>(pQuery->storeSize),
                                                     static_cast<int>(pQuery->dataSize),
                                                     &bytesWritten);
                    PAL_ASSERT(bytesWritten == neededSize);
                }
                else
                {
                    // The data doesn't seem to be compressed- just try to memcpy it from our buffer.
                    memcpy(pBuffer, compressedBuffer, pQuery->dataSize);
                }
            }

            PAL_SAFE_FREE(compressedBuffer, &m_allocator);
        }
    }

    return result;
}

// =====================================================================================================================
// Link another cache layer to ourselves.
Result CompressingCacheLayer::Link(
    ICacheLayer* pNextLayer)
{
    m_pNextLayer = pNextLayer;

    return Result::Success;
}

// =====================================================================================================================
// Get the object size.
size_t GetCompressingCacheLayerSize()
{
    return sizeof(CompressingCacheLayer);
}

// =====================================================================================================================
// Create a compressing cache layer
Result CreateCompressingCacheLayer(
    const CompressingCacheLayerCreateInfo*  pCreateInfo,
    void*                                   pPlacementAddr,
    ICacheLayer**                           ppCacheLayer)
{
    PAL_ASSERT(pCreateInfo        != nullptr);
    PAL_ASSERT(pPlacementAddr     != nullptr);
    PAL_ASSERT(ppCacheLayer       != nullptr);

    Result result                 = Result::Success;
    CompressingCacheLayer* pLayer = nullptr;

    if ((pCreateInfo        == nullptr) ||
        (pPlacementAddr     == nullptr) ||
        (ppCacheLayer       == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }

    if (result == Result::Success)
    {
        AllocCallbacks  callbacks = {};

        if (pCreateInfo->pCallbacks == nullptr)
        {
            GetDefaultAllocCb(&callbacks);
        }

        pLayer = PAL_PLACEMENT_NEW(pPlacementAddr) CompressingCacheLayer(
            (pCreateInfo->pCallbacks == nullptr) ? callbacks : *pCreateInfo->pCallbacks,
             pCreateInfo->useHighCompression,
             pCreateInfo->decompressOnly);

        *ppCacheLayer       = pLayer;
    }

    return result;
}

} //namespace Util
