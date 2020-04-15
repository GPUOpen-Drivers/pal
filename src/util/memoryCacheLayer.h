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
#pragma once

#include "cacheLayerBase.h"
#include "palHashMap.h"
#include "palIntrusiveList.h"
#include "palVector.h"

namespace Util
{

// =====================================================================================================================
// An ICacheLayer implementation that operates on fixed memory limits but not a fixed memory space
class MemoryCacheLayer : public CacheLayerBase
{
public:
    MemoryCacheLayer(
        const AllocCallbacks& callbacks,
        size_t                maxMemorySize,
        size_t                maxObjectCount,
        bool                  evictOnFull,
        bool                  evictDuplicates);
    virtual ~MemoryCacheLayer();

    virtual Result Init() override;

    Result GetMemoryCacheSize(size_t* pCurCount, size_t* pCurSize) const
    {
        *pCurCount = m_curCount;
        *pCurSize  = m_curSize;

        return Result::Success;
    }

    Result GetMemoryCacheHashIds(size_t curCount, Hash128* pHashIds);

protected:
    virtual Result QueryInternal(
        const Hash128*  pHashId,
        QueryResult*    pQuery) override;

    virtual Result StoreInternal(
        const Hash128*  pHashId,
        const void*     pData,
        size_t          dataSize) override;

    virtual Result LoadInternal(
        const QueryResult* pQuery,
        void*              pBuffer) override;

    virtual Result PromoteData(
        uint32       loadPolicy,
        ICacheLayer* pNextLayer,
        QueryResult* pQuery) override;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(MemoryCacheLayer);
    PAL_DISALLOW_DEFAULT_CTOR(MemoryCacheLayer);
    class Entry;

    Result AddEntryToCache(Entry* pEntry);
    Result EvictEntryFromCache(Entry* pEntry);

    Result EnsureAvailableSpace(size_t entrySize, size_t entryCount);
    Result EvictEntryByCount(size_t numToEvict = 1);
    Result EvictEntryBySize(size_t minSizeToEvict);

    // IntrusiveList capable cache entry data structure
    class Entry
    {
    public:
        using List = IntrusiveList<Entry>;
        using Node = IntrusiveListNode<Entry>;
        using Iter = IntrusiveListIterator<Entry>;
        using Map  = HashMap<Hash128, Entry*, ForwardAllocator, JenkinsHashFunc>;

        static Entry* Create(
            ForwardAllocator* pAllocator,
            const Hash128*    pHashId,
            const void*       pInitialData,
            size_t            dataSize)
        {
            PAL_ASSERT(pAllocator != nullptr);
            PAL_ASSERT(pHashId != nullptr);

            Entry* pEntry = nullptr;
            void*  pMem   = PAL_MALLOC(sizeof(Entry) + dataSize, pAllocator, AllocInternal);

            if (pMem != nullptr)
            {
                pEntry = PAL_PLACEMENT_NEW(pMem) Entry(pAllocator);

                void* pData = VoidPtrInc(pMem, sizeof(Entry));

                if (pInitialData != nullptr)
                {
                    memcpy(pData, pInitialData, dataSize);
                }

                pEntry->m_hashId   = *pHashId;
                pEntry->m_pData    = pData;
                pEntry->m_dataSize = dataSize;
            }

            return pEntry;
        }

        const Hash128* HashId() const { return &m_hashId; }
        void* Data() const { return m_pData; }
        size_t DataSize() const { return m_dataSize; }

        Node* ListNode() { return &m_node; }

        void Destroy()
        {
            ForwardAllocator* pAllocator = m_pAllocator;
            this->~Entry();
            PAL_FREE(this, pAllocator);
        }

    private:
        PAL_DISALLOW_COPY_AND_ASSIGN(Entry);
        PAL_DISALLOW_DEFAULT_CTOR(Entry);

        Entry(ForwardAllocator* pAllocator)
            :
            m_pAllocator { pAllocator },
            m_node       { this },
            m_hashId     {},
            m_pData      { nullptr },
            m_dataSize   { 0 }
        {
            PAL_ASSERT(m_pAllocator != nullptr);
        }

        ~Entry() { PAL_ASSERT(m_node.InList() == false); }

        ForwardAllocator* const m_pAllocator;
        Node                    m_node;
        Hash128                 m_hashId;
        void*                   m_pData;
        size_t                  m_dataSize;
    };

    const size_t m_maxSize;
    const size_t m_maxCount;
    const bool   m_evictOnFull;
    const bool   m_evictDuplicates;

    RWLock       m_lock;

    size_t       m_curSize;
    size_t       m_curCount;

    Entry::List  m_recentEntryList;
    Entry::Map   m_entryLookup;
};

} //namespace Util
