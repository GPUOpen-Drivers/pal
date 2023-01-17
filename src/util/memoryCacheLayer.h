/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palConditionVariable.h"
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 711
        uint32                expectedEntries,
#endif
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

    virtual Result AcquireCacheRef(const QueryResult* pQuery) override;
    virtual Result ReleaseCacheRef(const QueryResult* pQuery) override;
    virtual Result GetCacheData(const QueryResult* pQuery, const void** ppData) override;
    virtual Result WaitForEntry(const Hash128* pHashId) override;
    virtual Result Evict(const Hash128* pHashId) override;
    virtual Result MarkEntryBad(const Hash128* pHashId) override;

protected:
    virtual Result QueryInternal(
        const Hash128*  pHashId,
        QueryResult*    pQuery) override;

    virtual Result StoreInternal(
        Util::StoreFlags    storeFlags,
        const Hash128*      pHashId,
        const void*         pData,
        size_t              dataSize,
        size_t              storeSize) override;

    virtual Result LoadInternal(
        const QueryResult* pQuery,
        void*              pBuffer) override;

    virtual Result PromoteData(
        ICacheLayer* pNextLayer,
        const void*  pBuffer,
        QueryResult* pQuery) override;

    virtual Result Reserve(
        const Hash128* pHashId) override;
private:
    PAL_DISALLOW_COPY_AND_ASSIGN(MemoryCacheLayer);
    PAL_DISALLOW_DEFAULT_CTOR(MemoryCacheLayer);
    class Entry;

    Result SetDataToEntry(Entry* pEntry, const void* pData, size_t dataSize, size_t storeSize);
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
        using Map  = HashMap<Hash128,
                             Entry*,
                             ForwardAllocator,
                             JenkinsHashFunc,
                             DefaultEqualFunc,
                             HashAllocator<ForwardAllocator>,
                             256>;

        static Entry* Create(
            ForwardAllocator* pAllocator,
            const Hash128*    pHashId,
            const void*       pInitialData,
            size_t            dataSize,
            size_t            storeSize);

        Result SetData(const void* pData, size_t dataSize, size_t storeSize);
        const Hash128* HashId() const { return &m_hashId; }
        void* Data() const { return m_pData; }
        size_t DataSize() const { return m_dataSize; }
        size_t StoreSize() const { return m_storeSize; }
        void IncreaseRef() { AtomicIncrement(&m_zeroCopyCount); }
        void DecreaseRef()
        {
            PAL_ASSERT(m_zeroCopyCount > 0);
            AtomicDecrement(&m_zeroCopyCount);
        }
        bool CanEvict() { return m_zeroCopyCount == 0; }
        void SetIsBad(bool isBad) { m_isBad = isBad; }
        bool IsBad() { return m_isBad; }

        Node* ListNode() { return &m_node; }

        void Destroy();

    private:
        PAL_DISALLOW_COPY_AND_ASSIGN(Entry);
        PAL_DISALLOW_DEFAULT_CTOR(Entry);

        Entry(ForwardAllocator* pAllocator)
            :
            m_pAllocator { pAllocator },
            m_node       { this },
            m_hashId     {},
            m_pData      { nullptr },
            m_dataSize   { 0 },
            m_isBad      { false }
        {
            PAL_ASSERT(m_pAllocator != nullptr);
        }

        ~Entry() { PAL_ASSERT(m_node.InList() == false); }

        ForwardAllocator* const m_pAllocator;
        Node                    m_node;
        Hash128                 m_hashId;
        void*                   m_pData;
        size_t                  m_dataSize;
        size_t                  m_storeSize;
        volatile uint32         m_zeroCopyCount;
        bool                    m_isBad;
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

    Mutex              m_conditionMutex;      // Mutex that will be used with the condition variable
    ConditionVariable  m_conditionVariable;   // used for waiting on Entry::ready
};

} //namespace Util
