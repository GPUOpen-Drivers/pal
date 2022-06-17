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
#pragma once

#include "palCacheLayer.h"

#include "palSysMemory.h"
#include "palLinearAllocator.h"
#include "palMutex.h"
#include "palVector.h"

namespace Util
{

// =====================================================================================================================
// Common functionality of most cache layers including thread-safety and layering
class CacheLayerBase : public ICacheLayer
{
public:
    virtual Result Init() { return Result::Success; }

    virtual Result Query(
        const Hash128*  pHashId,
        uint32          policy,
        uint32          flags,
        QueryResult*    pQuery) final;

    virtual Result Store(
        Util::StoreFlags    storeFlags,
        const Hash128*      pHashId,
        const void*         pData,
        size_t              dataSize,
        size_t              storeSize = 0) final;

    virtual Result Load(
        const QueryResult* pQuery,
        void*              pBuffer) final;

    virtual Result Link(
        ICacheLayer* pNextLayer) final;

    virtual Result SetLoadPolicy(
        uint32 loadPolicy) final;

    virtual Result SetStorePolicy(
        uint32 storePolicy) final;

    virtual ICacheLayer* GetNextLayer() const final { return m_pNextLayer; }

    virtual uint32 GetLoadPolicy() const final { return m_loadPolicy; }

    virtual uint32 GetStorePolicy() const final { return m_storePolicy; }

    virtual void Destroy() final { this->~CacheLayerBase(); }

protected:
    PAL_DISALLOW_DEFAULT_CTOR(CacheLayerBase);
    PAL_DISALLOW_COPY_AND_ASSIGN(CacheLayerBase);

    explicit CacheLayerBase(const AllocCallbacks& callbacks);
    virtual ~CacheLayerBase();

    // Access to a generic allocator suitable for long-term storage
    ForwardAllocator* Allocator() { return &m_allocator; }

    // Internal, single layer operation functions
    virtual Result QueryInternal(
        const Hash128*  pHashId,
        QueryResult*    pQuery) = 0;
    virtual Result StoreInternal(
        Util::StoreFlags    storeFlags,
        const Hash128*      pHashId,
        const void*         pData,
        size_t              dataSize,
        size_t              storeSize) = 0;
    virtual Result LoadInternal(
        const QueryResult* pQuery,
        void*              pBuffer) = 0;

    // Promote data from a lower cache layer into our own
    // On successful promotion query may be re-written to reflect the newly promoted data rather than the original
    // pBuffer is optional; if it is nullptr we'll load it from pNextLayer.
    virtual Result PromoteData(
        ICacheLayer* pNextLayer,
        const void*  pBuffer,
        QueryResult* pQuery) { return Result::Unsupported; }

    // Reserve a empty entry in cache
    virtual Result Reserve(
        const Hash128* pHashId) { return Result::Unsupported; }

    // Batch data to be submitted to the next cache layer at a later time
    virtual Result BatchData(
        uint32         storePolicy,
        ICacheLayer*   pNextLayer,
        const Hash128* pHashId,
        const void*    pData,
        size_t         dataSize,
        size_t         storeSize) { return Result::Unsupported; }

private:

    ForwardAllocator m_allocator;
    ICacheLayer*     m_pNextLayer;
    uint32           m_loadPolicy;
    uint32           m_storePolicy;
};

} //namespace Util
