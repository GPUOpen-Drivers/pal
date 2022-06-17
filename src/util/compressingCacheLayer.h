/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "lz4Compressor.h"
#include "palMutex.h"

namespace Util
{

// =====================================================================================================================
// The ICacheLayer implementation that tracks and reports pipeline hashes that were successfully stored or loaded
class CompressingCacheLayer : public ICacheLayer
{
public:
    CompressingCacheLayer(const AllocCallbacks& callbacks, bool useHighCompression, bool decompressOnly);

    virtual ~CompressingCacheLayer();

    virtual Result Init();

    virtual Result Query(
        const Hash128*  pHashId,
        uint32          policy,
        uint32          flags,
        QueryResult*    pQuery) final;

    virtual Result Store(
        Util::StoreFlags storeFlags,
        const Hash128*   pHashId,
        const void*      pData,
        size_t           dataSize,
        size_t           storeSize) final;

    virtual Result WaitForEntry(
        const Hash128* pHashId) final
    {
        return m_pNextLayer->WaitForEntry(pHashId);
    }

    virtual Result Evict(
        const Hash128* pHashId) final
    {
        return m_pNextLayer->Evict(pHashId);
    }

    virtual Result MarkEntryBad(
        const Hash128* pHashId) final
    {
        return m_pNextLayer->MarkEntryBad(pHashId);
    }

    virtual Result Load(
        const QueryResult* pQuery,
        void*              pBuffer) final;

    virtual Result Link(
        ICacheLayer* pNextLayer) final;

    virtual ICacheLayer* GetNextLayer() const final { return m_pNextLayer; }

    // These should never be called.
    virtual Result SetLoadPolicy(uint32 loadPolicy) final { PAL_ASSERT_ALWAYS(); return Result::Unsupported; }
    virtual Result SetStorePolicy(uint32 storePolicy) final { PAL_ASSERT_ALWAYS(); return Result::Unsupported; }
    virtual uint32 GetLoadPolicy() const final { PAL_ASSERT_ALWAYS(); return 0; }
    virtual uint32 GetStorePolicy() const final { PAL_ASSERT_ALWAYS(); return 0; }

    virtual void Destroy() final { this->~CompressingCacheLayer(); }

private:
    PAL_DISALLOW_DEFAULT_CTOR(CompressingCacheLayer);
    PAL_DISALLOW_COPY_AND_ASSIGN(CompressingCacheLayer);

    Lz4Compressor    m_compressor;
    ForwardAllocator m_allocator;
    Mutex            m_compressMutex;
    ICacheLayer*     m_pNextLayer;
    bool             m_decompressOnly;
};

} //namespace Util
