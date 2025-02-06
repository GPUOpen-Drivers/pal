/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palArFile.h"

namespace Util
{

// =====================================================================================================================
// An ICacheLayer implementation that extracts ELFs from archives on stores and reassembles them into archive on loads.
//
// This layer is dependent on later layers to perform actual caching of payloads. The expected usage is to be after any
// logging/shim/replacement layers and before any compression/memory/disk layers.
//
// Use of the Archive file format is primarily for structuring data, and not to explicitly write UNIX .ar files to disk,
// even if other layers or utilities may do so.
class MultiElfCacheLayer : public CacheLayerBase
{
public:
    MultiElfCacheLayer(const AllocCallbacks& callbacks);
    virtual ~MultiElfCacheLayer();

    virtual Result Init() override;

    virtual Result WaitForEntry(const Hash128* pHashId) override;

    void SetNextStorageLayer(ICacheLayer* pNextStorageLayer) {m_pNextStorageLayer = pNextStorageLayer;}

    virtual size_t GetEntryCount() const final { return 0; }
    virtual size_t GetEntrySize()  const final { return 0; }

    const MultiElfCacheLayerStatistics* const GetMultiElfCacheStatistics() const {return &m_statistics;}

protected:
    virtual Result QueryInternal(
        const Hash128*      pHashId,
        QueryResult*        pQuery) override;

    virtual Result StoreInternal(
        StoreFlags          storeFlags,
        const Hash128*      pHashId,
        const void*         pData,
        size_t              dataSize,
        size_t              storeSize) override;

    virtual Result LoadInternal(
        const QueryResult*  pQuery,
        void*               pBuffer) override;

    virtual Result Reserve(const Hash128* pHashId) override;

private:
    PAL_DISALLOW_DEFAULT_CTOR(MultiElfCacheLayer);
    PAL_DISALLOW_COPY_AND_ASSIGN(MultiElfCacheLayer);

    // Enums for statistics updates
    enum MultiElfCacheStatisticsTypes
    {
        // operation counters
        Query = 0,          // this layer saw a query
        Store,              // this layer was given a payload to store
        Load,               // this layer was requested to load an entry
        ElfStore,           // an ELF was stored
        ElfLoad,            // an ELF was loaded

        // memory metrics
        Overhead,           // cache memory overhead from AoE indirection and bookkeeping
        ElfStoreSize,       // ELF bytes stored by this layer
        ElfStoreSavings,    // ELF bytes saved by de-duplication
        ElfLoadSize,        // ELF bytes loaded by this layer
        ElfLoadSavings,     // ELF bytes saved by de-duplication

        // other
        ArchiveMaxLength,   // Max count of ELFs seen in an archive
    };

    ICacheLayer*                 m_pNextStorageLayer; // next layer which can handle a query, so we can check load hit
    MultiElfCacheLayerStatistics m_statistics;        // collection of event counts and sizes
    Mutex                        m_statisticsMutex;   // mutex to protect above statistics

    // Simple entries for the primary metadata/pre-raster/pixel 3-ELF case
    struct MultiElfSimpleEntry
    {
        // The common scenario is the 3-ELF composition of metadata, pre-raster, and pixel
        static constexpr uint32     MaxElfsPerArchive = 3;

        // Data for ELFs contained in this archive
        ArFileFormat::FileHeader    elfHeaders[MaxElfsPerArchive];
        Hash128                     elfHashes[MaxElfsPerArchive];
        size_t                      elfSizes[MaxElfsPerArchive];
    };

    // Entries chained together to represent an archive of arbitrary length in ELFs.
    //
    // Since these essentially form a list of all the ELFs in an archive, the sequence is unique per-pipeline and they
    // will not benefit from de-duplication. We could add another level of indirection, but the gain would be very
    // small compared to the added complexity. More ELFs per entry will reduce lookup/assembly overhead, at the cost of
    // wasting some space.
    struct MultiElfChainEntry
    {
        // RayTracing and aggressive splitting can result in numerous ELFs in an archive
        static constexpr uint32     MaxElfsPerChainEntry = 16;

        uint32                      numElfs;
        ArFileFormat::FileHeader    elfHeaders[MaxElfsPerChainEntry];
        Hash128                     elfHashes[MaxElfsPerChainEntry];
        size_t                      elfSizes[MaxElfsPerChainEntry];

        Hash128                     nextChainEntryHash;
    };

    // Used in lieu of original payload when payload is an archive of ELFs, to track said ELFs
    struct MultiElfEntry
    {
        // Archive data, for reconstruction and sanity checking
        Hash128                     hash;
        uint32                      numElfs;
        size_t                      size;
        Hash128                     checksum;

        // We expect the vast majority of entries to be 3-ELF graphics pipelines
        union
        {
            MultiElfSimpleEntry     simpleEntry; // All 3 ELFs, if only 3 ELFs in archive
            Hash128                 chainBegin;  // First MultiElfChainEntry in chain, if not simple entry
        };
    };

    // Store simple 3-ELF archive
    Result StoreMultiElfSimple(
        StoreFlags              storeFlags,
        const Hash128*          pHashId,
        Span<const char>        bufferSpan);

    // Load simple 3-ELF archive
    Result LoadMultiElfSimple(
        void*                   pBuffer,
        const MultiElfEntry*    pEntry);

    // Store long (variable length) chain of ELFs from an archive, probably from NPRT
    Result StoreMultiElfChain(
        StoreFlags              storeFlags,
        const Hash128*          pHashId,
        Span<const char>        bufferSpan,
        uint32                  numElfs);

    // Load long (variable length) chain of ELFs from an archive, probably from NPRT
    Result LoadMultiElfChain(
        void*                   pBuffer,
        const MultiElfEntry*    pEntry);

    // Query for an ELF and load if it exists
    Result QueryAndLoadElf(
        void**                  ppWrite,
        const Hash128*          pElfHash);

    // Update statistics based on attempted operation and its result
    void UpdateStatistics(
        MultiElfCacheStatisticsTypes stats,
        Result                       result,
        size_t                       size = 0);
};

} //namespace Util
