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
#pragma once

#include "palCacheLayer.h"

#include "palSysMemory.h"
#include "palLinearAllocator.h"
#include "palMutex.h"
#include "palVector.h"

namespace Util
{

// =====================================================================================================================
// The ICacheLayer implementation that tracks and reports pipeline hashes that were successfully stored or loaded
class TrackingCacheLayer : public ICacheLayer
{
public:
    TrackingCacheLayer(
        const AllocCallbacks& callbacks);

    virtual ~TrackingCacheLayer();

    virtual Result Init();

    virtual Result Query(
        const Hash128*  pHashId,
        uint32          policy,
        uint32          flags,
        QueryResult*    pQuery) final;

    virtual Result Store(
        const Hash128*  pHashId,
        const void*     pData,
        size_t          dataSize,
        size_t          storeSize = 0) final;

    virtual Result Load(
        const QueryResult* pQuery,
        void*              pBuffer) final;

    virtual Result Link(
        ICacheLayer* pNextLayer) final;

    virtual Result SetLoadPolicy(
        uint32 loadPolicy) final { return Result::Unsupported; }

    virtual Result SetStorePolicy(
        uint32 storePolicy) final { return Result::Unsupported; }

    virtual ICacheLayer* GetNextLayer() const final { return m_pNextLayer; }

    virtual uint32 GetLoadPolicy() const final { return m_loadPolicy; }

    virtual uint32 GetStorePolicy() const final { return m_storePolicy; }

    virtual void Destroy() final { this->~TrackingCacheLayer(); }

    static TrackedHashIter GetEntriesBegin(
        const ICacheLayer* pTrackingLayer);

private:
    PAL_DISALLOW_DEFAULT_CTOR(TrackingCacheLayer);
    PAL_DISALLOW_COPY_AND_ASSIGN(TrackingCacheLayer);

    // Access to a generic allocator suitable for long-term storage
    ForwardAllocator* Allocator() { return &m_allocator; }

    // Constants
    static constexpr size_t HashTableBucketCount = 2048;

    ForwardAllocator m_allocator;
    ICacheLayer*     m_pNextLayer;
    const uint32     m_loadPolicy;
    const uint32     m_storePolicy;

    TrackedHashSet   m_entries;
};

} //namespace Util
