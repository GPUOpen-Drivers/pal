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
#include "trackingCacheLayer.h"

#include "palHashSetImpl.h"
#include "palAssert.h"
#include "palSysUtil.h"

#include "core/platform.h"

namespace Util
{

// =====================================================================================================================
TrackingCacheLayer::TrackingCacheLayer(
    const AllocCallbacks& callbacks)
    :
    m_allocator   { callbacks },
    m_pNextLayer  { nullptr },
    m_loadPolicy  { LinkPolicy::PassData | LinkPolicy::PassCalls },
    m_storePolicy { LinkPolicy::PassData },
    m_entries     { HashTableBucketCount, Allocator() }
{
    // Alloc and Free MUST NOT be nullptr
    PAL_ASSERT(callbacks.pfnAlloc != nullptr);
    PAL_ASSERT(callbacks.pfnFree != nullptr);

    // pClientData SHOULD not be nullptr
    PAL_ALERT(callbacks.pClientData == nullptr);
}

// =====================================================================================================================
TrackingCacheLayer::~TrackingCacheLayer()
{
}

// =====================================================================================================================
Result TrackingCacheLayer::Init()
{
    return m_entries.Init();
}

// =====================================================================================================================
// Validate inputs, then attempt to query our layer. On Result::NotFound attempt to query children
Result TrackingCacheLayer::Query(
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
    }

    if (result == Result::Success)
    {
        m_entries.Insert(*pHashId);
    }
    else
    {
        m_entries.Erase(*pHashId);
    }

    return result;
}

// =====================================================================================================================
// Validate inputs, then store data to our layer. Propagate data down to children if needed.
Result TrackingCacheLayer::Store(
    const Hash128* pHashId,
    const void*    pData,
    size_t         dataSize)
{
    Result result = Result::ErrorUnknown;

    PAL_ASSERT(m_pNextLayer != nullptr);

    if (m_pNextLayer == nullptr)
    {
        result = Result::ErrorUnavailable;
    }
    else
    {
        result = m_pNextLayer->Store(pHashId, pData, dataSize);
    }

    if (pHashId != nullptr)
    {
        if (result == Result::Success)
        {
            m_entries.Insert(*pHashId);
        }
        else
        {
            m_entries.Erase(*pHashId);
        }
    }

    return result;
}

// =====================================================================================================================
// Validate inputs, then load data from our layer
Result TrackingCacheLayer::Load(
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
        result = m_pNextLayer->Load(pQuery, pBuffer);
    }

    if (result != Result::Success)
    {
        m_entries.Erase(pQuery->hashId);
    }

    return result;
}

// =====================================================================================================================
// Link another cache layer to ourselves.
Result TrackingCacheLayer::Link(
    ICacheLayer* pNextLayer)
{
    m_pNextLayer = pNextLayer;

    return Result::Success;
}

// =====================================================================================================================
// Populates the iteration begin point
TrackedHashIter TrackingCacheLayer::GetEntriesBegin(
    const ICacheLayer* pTrackingLayer)
{
    return static_cast<const TrackingCacheLayer*>(pTrackingLayer)->m_entries.Begin();
}

// =====================================================================================================================
// Get the memory size for a pipeline tracking cache layer
size_t GetTrackingCacheLayerSize()
{
    return sizeof(TrackingCacheLayer);
}

// =====================================================================================================================
// Create an pipeline tracking cache layer
Result CreateTrackingCacheLayer(
    const TrackingCacheCreateInfo*  pCreateInfo,
    void*                           pPlacementAddr,
    ICacheLayer**                   ppCacheLayer,
    Util::GetTrackedHashes*         ppGetTrackedHashes)
{
    PAL_ASSERT(pCreateInfo        != nullptr);
    PAL_ASSERT(pPlacementAddr     != nullptr);
    PAL_ASSERT(ppCacheLayer       != nullptr);
    PAL_ASSERT(ppGetTrackedHashes != nullptr);

    Result result              = Result::Success;
    TrackingCacheLayer* pLayer = nullptr;

    if ((pCreateInfo        == nullptr) ||
        (pPlacementAddr     == nullptr) ||
        (ppCacheLayer       == nullptr) ||
        (ppGetTrackedHashes == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }

    if (result == Result::Success)
    {
        AllocCallbacks  callbacks = {};

        if (pCreateInfo->pCallbacks == nullptr)
        {
            Pal::GetDefaultAllocCb(&callbacks);
        }

        pLayer = PAL_PLACEMENT_NEW(pPlacementAddr) TrackingCacheLayer(
            (pCreateInfo->pCallbacks == nullptr) ? callbacks : *pCreateInfo->pCallbacks);

        result = pLayer->Init();

        if (result == Result::Success)
        {
            *ppGetTrackedHashes = *TrackingCacheLayer::GetEntriesBegin;
            *ppCacheLayer       = pLayer;
        }
        else
        {
            *ppGetTrackedHashes = nullptr;
            *ppCacheLayer       = nullptr;
        }
    }

    return result;
}

} //namespace Util
