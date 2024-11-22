/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "multiElfCacheLayer.h"
#include <cinttypes>
#include <cstdio>

namespace Util
{

// =====================================================================================================================
MultiElfCacheLayer::MultiElfCacheLayer(
    const AllocCallbacks& callbacks)
    :
    CacheLayerBase(callbacks),
    m_statistics({})
{
}

// =====================================================================================================================
MultiElfCacheLayer::~MultiElfCacheLayer()
{
}

// =====================================================================================================================
// Initialize the cache layer
Result MultiElfCacheLayer::Init()
{
    Result result = CacheLayerBase::Init();

    // We explicitly call next layer to store, so don't use the default LinkPolicy::PassData.
    SetStorePolicy(0);

    return result;
}

// =====================================================================================================================
// Check if a requested id is present
Result MultiElfCacheLayer::QueryInternal(
    const Hash128* pHashId,
    QueryResult*   pQuery)
{
    // Query lower layers for match
    QueryResult localQueryResult = {};
    Result result = m_pNextLayer->Query(pHashId, 0, 0, &localQueryResult);
    UpdateStatistics(Query, Result::Reserved);

    // Adjust query results for hit if it is probably an archive of ELFs
    if ((result == Result::Success) && (localQueryResult.dataSize == sizeof(MultiElfEntry)))
    {
        MultiElfEntry entry = {};
        Result loadResult = m_pNextLayer->Load(&localQueryResult, &entry);
        if (loadResult == Result::Success)
        {
            PAL_ASSERT(pHashId->qwords[1] == entry.hash.qwords[1]);
            PAL_ASSERT(pHashId->qwords[0] == entry.hash.qwords[0]);

            // Sanity check just in case this is somehow not an archive of ELFs entry
            if (memcmp(pHashId, &entry.hash, sizeof(entry.hash)) == 0)
            {
                localQueryResult.pLayer         = this;
                localQueryResult.dataSize       = entry.size;
                localQueryResult.storeSize      = entry.size;
                localQueryResult.promotionSize  = entry.size;
                UpdateStatistics(Query, Result::Success);
            }
            else
            {
                result = Result::NotFound; // reset result so next layer can handle this doppelganger
            }
        }
        else
        {
            // Even if the query was a success, we cannot verify entry integrity and composition if it is not loadable
            UpdateStatistics(Query, loadResult);
            result = loadResult;
        }
    }

    *pQuery = localQueryResult;

    PAL_ALERT(IsErrorResult(result));
    return result;
}

// =====================================================================================================================
// Add data passed in to the cache
Result MultiElfCacheLayer::StoreInternal(
    Util::StoreFlags    storeFlags,
    const Hash128*      pHashId,
    const void*         pData,
    size_t              dataSize,
    size_t              storeSize)
{
    Result result = Result::Success;

    // Check for bad inputs and state
    if ((pHashId == nullptr) || (pData == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (dataSize == 0)
    {
        result = Result::ErrorInvalidValue;
    }
    else if (m_pNextLayer == nullptr)
    {
        result = Result::ErrorUnavailable;
    }
    UpdateStatistics(Store, Result::Reserved);

    // Process store
    if (result == Result::Success)
    {
        const char* pBuffer = static_cast<const char*>(pData);
        Span<const char> bufferSpan(pBuffer, dataSize);
        if (memcmp(pBuffer, ArFileMagic, sizeof(ArFileMagic)) == 0) // archive of ELFs
        {
            // Quickly iterate through archive to count ELFs
            Util::ArFileReader archiveReader(bufferSpan);
            auto counterIterator = archiveReader.Begin();
            uint32 numElfs       = 0;
            while (counterIterator.IsEnd() == false)
            {
                numElfs++;
                counterIterator.Next();
            }

            // Store ELFs based on count, either with commonly fixed size of 3 or variable size (potentially hundreds)
            if (numElfs == MultiElfSimpleEntry::MaxElfsPerArchive)
            {
                result = StoreMultiElfSimple(storeFlags, pHashId, bufferSpan);
            }
            else
            {
                // NPRT can have hundreds of ELFs, so chain them together.
                result = StoreMultiElfChain(storeFlags, pHashId, bufferSpan, numElfs);
            }
        }
        else // something else (i.e. single ELF)
        {
            result = m_pNextLayer->Store(storeFlags, pHashId, pData, dataSize, storeSize);
        }
    }

    PAL_ALERT(IsErrorResult(result));
    return result;
}

// =====================================================================================================================
// Copy data from cache to the provided buffer
Result MultiElfCacheLayer::LoadInternal(
    const QueryResult* pQuery,
    void*              pBuffer)
{
    PAL_ASSERT(pQuery != nullptr);
    PAL_ASSERT(pQuery->pLayer != nullptr);
    PAL_ASSERT(pBuffer != nullptr);

    // The next layer will handle the load if this is not actually an archive of ELFs
    Result result = Result::NotFound;

    // Check for bad inputs and state
    if ((pQuery == nullptr) || (pBuffer == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (pQuery->pLayer != this)
    {
        result = Result::ErrorInvalidValue;
    }
    UpdateStatistics(Load, Result::Reserved);

    // Reassemble the archive of ELFs
    if (pQuery->pLayer == this)
    {
        // Re-query since we lost track of which later layer handled the initial query
        QueryResult localQueryResult = {};
        result = m_pNextLayer->Query(&(pQuery->hashId), 0, 0, &localQueryResult);
        PAL_ASSERT(localQueryResult.dataSize == sizeof(MultiElfEntry));

        // Load the multi-elf entry
        MultiElfEntry entry = {};
        result = m_pNextLayer->Load(&localQueryResult, &entry);
        UpdateStatistics(Load, result, localQueryResult.dataSize);
        PAL_ASSERT(result == Result::Success);
        PAL_ASSERT(pQuery->hashId.qwords[1] == entry.hash.qwords[1]);
        PAL_ASSERT(pQuery->hashId.qwords[0] == entry.hash.qwords[0]);

        // Load ELFs based on count, either with commonly fixed size of 3 or variable size (potentially hundreds)
        if (entry.numElfs == MultiElfSimpleEntry::MaxElfsPerArchive)
        {
            result = LoadMultiElfSimple(pBuffer, &entry);
        }
        else
        {
            result = LoadMultiElfChain(pBuffer, &entry);
        }
    }

    PAL_ALERT(IsErrorResult(result));
    return result;
}

// =====================================================================================================================
// Reserve a empty entry in cache
Result MultiElfCacheLayer::Reserve(
    const Hash128* pHashId)
{
    Result result = Result::Unsupported;

    if (m_pNextLayer != nullptr)
    {
        // Do a Query() with reserve flag on next layer, similar to how we got here
        QueryResult localQueryResult = {};
        result = m_pNextLayer->Query(pHashId, 0, QueryFlags::ReserveEntryOnMiss, &localQueryResult);
    }

    return result;
}

// =====================================================================================================================
// Split and store a simple 3-ELF archive
Result MultiElfCacheLayer::StoreMultiElfSimple(
    Util::StoreFlags       storeFlags,
    const Util::Hash128*   pHashId,
    Util::Span<const char> bufferSpan)
{
    Result result = Result::Success;

    // We store a description of archive contents so we can look up child ELFs later
    MultiElfEntry entry = {
        .hash       = *pHashId,
        .numElfs    = MultiElfSimpleEntry::MaxElfsPerArchive,
        .size       = sizeof(ArFileMagic),
    };

    // Begin stepping through ELFs in the archive
    Util::ArFileReader archiveReader(bufferSpan);
    auto archiveIterator = archiveReader.Begin();

    // Call next layer to store each ELF in archive individually
    for (uint32 elfIndex = 0; elfIndex < MultiElfSimpleEntry::MaxElfsPerArchive; ++elfIndex)
    {
        // Track file header for this ELF
        entry.simpleEntry.elfHeaders[elfIndex] = archiveIterator.GetHeader();
        entry.size                            += sizeof(ArFileFormat::FileHeader);

        // ELF identifier is a 16-character hex string from the Pipeline Compiler
        Hash128 elfHash                        = {};
        const Span<const char> name            = archiveIterator.GetName();
        const Span<const char> data            = archiveIterator.GetData();
        char buffer[]                          = "0xFFFFFFFFFFFFFFFF";
        memcpy(buffer+2, name.Begin(), 16);
        sscanf(buffer, "%" SCNx64, &elfHash.qwords[0]);

        // Track and store this child ELF
        entry.simpleEntry.elfHashes[elfIndex]  = elfHash;
        entry.simpleEntry.elfSizes[elfIndex]   = data.size();
        entry.size                            += data.size();
        result = m_pNextLayer->Store(storeFlags, &elfHash, data.begin(), data.size());
        UpdateStatistics(ElfStore, result, data.size());

        archiveIterator.Next();
        if (IsErrorResult(result))
        {
            break;
        }
    }

    if (IsErrorResult(result) == false)
    {
        // Sanity checks
        PAL_ASSERT(entry.size == bufferSpan.size());
        PAL_ASSERT(archiveIterator.IsEnd() == true);
        MetroHash128::Hash(reinterpret_cast<const uint8*>(bufferSpan.Data()), bufferSpan.size(), entry.checksum.bytes);

        // Store the entry describing this pipeline's composition
        result = m_pNextLayer->Store(storeFlags, pHashId, &entry, sizeof(MultiElfEntry));
        UpdateStatistics(Store, result, sizeof(MultiElfEntry));
        UpdateStatistics(ArchiveMaxLength, Result::Success, MultiElfSimpleEntry::MaxElfsPerArchive);
    }

    PAL_ALERT(IsErrorResult(result));
    return result;
}

// =====================================================================================================================
// Load and assemble a simple 3-ELF archive
Result MultiElfCacheLayer::LoadMultiElfSimple(
    void*                pBuffer,
    const MultiElfEntry* pEntry)
{
    Result result = Result::Success;

    // Write identifying character sequence
    void* pWrite = pBuffer;
    memcpy(pWrite, ArFileMagic, sizeof(ArFileMagic));
    pWrite = VoidPtrInc(pWrite, sizeof(ArFileMagic));

    // Write ELFs sequentially into buffer
    for (uint32 elfIndex = 0; elfIndex < MultiElfSimpleEntry::MaxElfsPerArchive; ++elfIndex)
    {
        // File header
        const ArFileFormat::FileHeader* pHeader = &(pEntry->simpleEntry.elfHeaders[elfIndex]);
        memcpy(pWrite, pHeader, sizeof(ArFileFormat::FileHeader));
        pWrite = VoidPtrInc(pWrite, sizeof(ArFileFormat::FileHeader));

        // ELF binary
        result = QueryAndLoadElf(&pWrite, &pEntry->simpleEntry.elfHashes[elfIndex]);
        if (result != Result::Success)
        {
            break;
        }
    }

    if (IsErrorResult(result) == false)
    {
        // Sanity checks
        PAL_ASSERT(VoidPtrDiff(pWrite, pBuffer) == pEntry->size);
        Hash128 assembledHash = {};
        MetroHash128::Hash(static_cast<const uint8*>(pBuffer), pEntry->size, assembledHash.bytes);
        PAL_ASSERT(memcmp(assembledHash.bytes, pEntry->checksum.bytes, sizeof(Hash128)) == 0);
    }

    PAL_ALERT(IsErrorResult(result));
    return result;
}

// =====================================================================================================================
// Split and store an archive with many ELFs
Result MultiElfCacheLayer::StoreMultiElfChain(
    Util::StoreFlags       storeFlags,
    const Util::Hash128*   pHashId,
    Util::Span<const char> bufferSpan,
    uint32                 numElfs)
{
    Result result = Result::Success;
    UpdateStatistics(ArchiveMaxLength, Result::Success, numElfs);
    PAL_ALERT(numElfs <= MultiElfSimpleEntry::MaxElfsPerArchive); // We shouldn't be chaining if it fits in simple

    // Begin parsing the archive
    Util::ArFileReader archiveReader(bufferSpan);
    auto archiveIterator = archiveReader.Begin();
    MultiElfEntry entry = {
        .hash       = *pHashId,
        .numElfs    = numElfs,
        .size       = sizeof(ArFileMagic),
        .chainBegin = {},
    };

    // Split archive into chunks of N ELFs and chain the chunks together into a linked list
    const uint32 numChainEntries          = ((numElfs-1) / MultiElfChainEntry::MaxElfsPerChainEntry) + 1;
    const uint32 numElfsInLast            = numElfs % MultiElfChainEntry::MaxElfsPerChainEntry;
    MultiElfChainEntry previousChainEntry = {};
    Hash128 previousChainHash             = {};
    uint32 chainIndex                     = 0;
    uint32 elfIndex                       = 0;
    while((archiveIterator.IsMalformed() == false) && (archiveIterator.IsEnd() == false))
    {
        // Populate chain entry with data on a sequence of ELFs, while storing said ELFs
        MultiElfChainEntry currentChainEntry = {};
        for (uint32 elfInLink = 0; elfInLink < MultiElfChainEntry::MaxElfsPerChainEntry; ++elfInLink)
        {
            PAL_ASSERT(archiveIterator.IsEnd() == false);

            // ELF identifier is a 16-character hex string from the Pipeline Compiler
            Hash128 elfHash                 = {};
            const Span<const char> name     = archiveIterator.GetName();
            const Span<const char> data     = archiveIterator.GetData();
            char buffer[]                   = "0xFFFFFFFFFFFFFFFF";
            memcpy(buffer+2, name.Begin(), 16);
            sscanf(buffer, "%" SCNx64, &elfHash.qwords[0]);

            // Track and store this child ELF
            currentChainEntry.numElfs++;
            currentChainEntry.elfHeaders[elfInLink] = archiveIterator.GetHeader();
            currentChainEntry.elfHashes[elfInLink]  = elfHash;
            currentChainEntry.elfSizes[elfInLink]   = data.size();
            entry.size                              += sizeof(ArFileFormat::FileHeader);
            entry.size                              += data.size();
            result = m_pNextLayer->Store(storeFlags, &elfHash, data.begin(), data.size());
            UpdateStatistics(ElfStore, result, data.size());

            if (IsErrorResult(result))
            {
                break;
            }
            archiveIterator.Next();
            if (archiveIterator.IsEnd())
            {
                break;
            }
            elfIndex++;
        }

        if (IsErrorResult(result))
        {
            break;
        }

        // Determine hash for current entry, then have previous entry point to current, then store previous entry
        Hash128 currentChainHash = {};
        currentChainEntry.nextChainEntryHash = *pHashId; // pipeline hash to prevent cross-pipeline tangling
        MetroHash128::Hash(reinterpret_cast<const uint8*>(&currentChainEntry),
                           sizeof(MultiElfChainEntry),
                           currentChainHash.bytes);
        if (chainIndex == 0) // first link in chain, so have MultiElfEntry point to it
        {
            entry.chainBegin = currentChainHash;
        }
        else // all other links are pointed to by previous link
        {
            previousChainEntry.nextChainEntryHash = currentChainHash;
            result = m_pNextLayer->Store(storeFlags,
                                         &previousChainHash,
                                         &previousChainEntry,
                                         sizeof(MultiElfChainEntry));
            UpdateStatistics(Overhead, result, sizeof(MultiElfChainEntry));
        }
        if (archiveIterator.IsEnd()) // current link terminates chain
        {
            result = m_pNextLayer->Store(storeFlags,
                                         &currentChainHash,
                                         &currentChainEntry,
                                         sizeof(MultiElfChainEntry));
            UpdateStatistics(Overhead, result, sizeof(MultiElfChainEntry));
        }

        previousChainHash  = currentChainHash;
        previousChainEntry = currentChainEntry;
        chainIndex++;
    }

    if (IsErrorResult(result) == false)
    {
        // Sanity checks
        PAL_ASSERT(entry.size == bufferSpan.size());
        MetroHash128::Hash(reinterpret_cast<const uint8*>(bufferSpan.Data()), bufferSpan.size(), entry.checksum.bytes);

        // Store the entry describing this pipeline's composition
        result = m_pNextLayer->Store(storeFlags, pHashId, &entry, sizeof(MultiElfEntry));
        UpdateStatistics(Store, result, sizeof(MultiElfEntry));
    }

    PAL_ALERT(IsErrorResult(result));
    return result;
}

// =====================================================================================================================
// Load and assemble an archive with many ELFs
Result MultiElfCacheLayer::LoadMultiElfChain(
    void*                pBuffer,
    const MultiElfEntry* pEntry)
{
    Result result = Result::Success;
    PAL_ASSERT((pEntry->hash.qwords[0] != 0) || (pEntry->hash.qwords[1] != 0)); // chain start hash must be non-zero

    // Write identifying character sequence
    void* pWrite = pBuffer;
    memcpy(pWrite, ArFileMagic, sizeof(ArFileMagic));
    pWrite = VoidPtrInc(pWrite, sizeof(ArFileMagic));

    // Track current position in entry chain
    const Hash128 firstChainHash            = pEntry->chainBegin;
    Hash128 currentChainHash                = firstChainHash;
    MultiElfChainEntry currentChainEntry    = {};
    bool shouldKeepLoading                  = true;
    uint32 chainIndex                       = 0;
    uint32 elfIndex                         = 0;

    // Load each chunk of N ELF descriptions, then load each of the N ELFs
    while (((currentChainHash.qwords[0] != 0) || (currentChainHash.qwords[1] != 0)) && shouldKeepLoading)
    {
        if (elfIndex >= pEntry->numElfs)
        {
            break;
        }

        // Load next link in chain
        QueryResult chainQueryResult = {};
        Result queryResult = m_pNextLayer->Query(&currentChainHash, 0, 0, &chainQueryResult);
        Result loadResult  = m_pNextLayer->Load(&chainQueryResult, &currentChainEntry);
        UpdateStatistics(Overhead, loadResult, chainQueryResult.dataSize);
        if (IsErrorResult(queryResult) || IsErrorResult(loadResult))
        {
            result = IsErrorResult(queryResult) ? queryResult : loadResult;
            break;
        }

        // Load and process each ELF in this chain link
        for (uint32 elfInChain = 0; elfInChain < currentChainEntry.numElfs; ++elfInChain)
        {
            // File header
            const ArFileFormat::FileHeader* pHeader = &(currentChainEntry.elfHeaders[elfInChain]);
            memcpy(pWrite, pHeader, sizeof(ArFileFormat::FileHeader));
            pWrite = VoidPtrInc(pWrite, sizeof(ArFileFormat::FileHeader));

            // ELF binary
            result = QueryAndLoadElf(&pWrite, &currentChainEntry.elfHashes[elfInChain]);
            if (result != Result::Success)
            {
                shouldKeepLoading = false;
                break;
            }

            const Hash128 elfHash = pEntry->simpleEntry.elfHashes[elfIndex];
            elfIndex++;
        }

        currentChainHash = currentChainEntry.nextChainEntryHash;
        chainIndex++;
    }

    if (IsErrorResult(result) == false)
    {
        // Sanity checks
        PAL_ASSERT(VoidPtrDiff(pWrite, pBuffer) == pEntry->size);
        Hash128 assembledHash = {};
        MetroHash128::Hash(static_cast<const uint8*>(pBuffer), pEntry->size, assembledHash.bytes);
        PAL_ASSERT(memcmp(assembledHash.bytes, pEntry->checksum.bytes, sizeof(Hash128)) == 0);
        if (memcmp(assembledHash.bytes, pEntry->checksum.bytes, sizeof(Hash128)) != 0)
        {
            result = Result::ErrorIncompleteResults;
        }
    }

    PAL_ALERT(IsErrorResult(result));
    return result;
}

// =====================================================================================================================
// Query for an ELF and load if it exists
Result MultiElfCacheLayer::QueryAndLoadElf(
    void**               ppWrite,
    const Util::Hash128* pElfHash)
{
    Result result = Result::NotFound;

    // Query for existence of ELF
    QueryResult elfQueryResult = {};
    result = m_pNextLayer->Query(pElfHash, 0, 0, &elfQueryResult);

    // Attempt to load ELF
    if (result == Result::Success)
    {
        result = m_pNextLayer->Load(&elfQueryResult, *ppWrite);

        // Update statistics and write pointer
        if (result == Result::Success)
        {
            *ppWrite = VoidPtrInc(*ppWrite, elfQueryResult.dataSize);

            // Count load performed, even if it gets discarded later
            UpdateStatistics(ElfLoad, result, elfQueryResult.dataSize);

            // If this ELF was provided by memory layer, count it towards memory savings
            if (elfQueryResult.pLayer == m_pNextStorageLayer)
            {
                UpdateStatistics(ElfLoadSavings, result, elfQueryResult.dataSize);
            }
        }
        else // query success but load failed
        {
            PAL_DPWARN("ELF load failed (%d): ELF %016" PRIX64 ".%016" PRIX64, result, pElfHash->qwords[1], pElfHash->qwords[0]);
        }
    }
    else // query failed
    {
        PAL_DPWARN("ELF query failed (%d): ELF %016" PRIX64 ".%016" PRIX64, result, pElfHash->qwords[1], pElfHash->qwords[0]);
    }

    return result;
}

// =====================================================================================================================
// Have later layers wait for entry availability
Result MultiElfCacheLayer::WaitForEntry(
    const Hash128* pHashId)
{
    return m_pNextLayer->WaitForEntry(pHashId);
}

// =====================================================================================================================
// Increments statistics counters based on attempted operation and result
void MultiElfCacheLayer::UpdateStatistics(
    MultiElfCacheStatisticsTypes stat,
    Result                       result,
    size_t                       size)
{
    // Drop all statistics updates unless we really want them, as collecting can impact performance.
#if PAL_DEVELOPER_BUILD
    // While the prospect of savings greater than 100% is exciting, it defies many mathematical/logical principles.
    MutexAuto autoLock(&m_statisticsMutex);

    if (IsErrorResult(result))
    {
        PAL_ALERT_ALWAYS_MSG("Something went wrong with MultiElf cache: %d", result);
    }

    switch (stat)
    {
    case Query:
        if (result == Result::Reserved)             // layer saw a query
        {
            m_statistics.queries++;
        }
        else if (result == Result::Success)         // layer handled a query
        {
            m_statistics.archiveQueries++;
        }
        else
        {
            PAL_ALERT_ALWAYS();
        }
        break;
    case Store:
        if (result == Result::Reserved)             // layer saw a store
        {
            m_statistics.stores++;
        }
        else if (result == Result::Success)         // layer handled a store
        {
            m_statistics.archiveStores++;
            m_statistics.overhead += size;
        }
        else if (result == Result::AlreadyExists)   // duplicate store of archive, meaning query mechanism is broken
        {
            PAL_ALERT_ALWAYS();
        }
        else
        {
            PAL_ALERT_ALWAYS();
        }
        break;
    case Load:
        if (result == Result::Reserved)             // layer saw a load
        {
            m_statistics.loads++;
        }
        else if (result == Result::Success)         // layer handled a load
        {
            m_statistics.archiveLoads++;
            m_statistics.overhead += size;
        }
        else
        {
            PAL_ALERT_ALWAYS();
        }
        break;
    case ElfStore:
        m_statistics.elfStores++;
        if (result == Result::Success)              // ELF store was unique
        {
            m_statistics.elfStoresUnique++;
            m_statistics.elfStoreSize += size;
        }
        else if (result == Result::AlreadyExists)   // ELF store hash collision
        {
            m_statistics.elfStoresExists++;
            m_statistics.elfStoreSize    += size;
            m_statistics.elfStoreSavings += size;
        }
        else if (IsErrorResult(result))             // ELF store error
        {
            m_statistics.elfErrors++;
            PAL_DPWARN("Error storing ELF from archive into cache");
        }
        else
        {
            PAL_ALERT_ALWAYS();
        }
        break;
    case ElfLoad:
        if (result == Result::Success)              // ELF load was successful, meaning it was found in/by lower layer
        {
            m_statistics.elfLoads++;
            m_statistics.elfLoadSize += size;
        }
        else if (IsErrorResult(result))             // ELF store error
        {
            m_statistics.elfErrors++;
            PAL_DPWARN("Error loading archive ELF from cache");
        }
        else
        {
            PAL_ALERT_ALWAYS();
        }
        break;

    // memory metrics
    case Overhead:
        m_statistics.overhead += size;
        break;
    case ElfStoreSize:
        m_statistics.elfStoreSize += size;
        break;
    case ElfStoreSavings:
        m_statistics.elfStoreSavings += size;
        break;
    case ElfLoadSize:
        m_statistics.elfLoadSize += size;
        break;
    case ElfLoadSavings:
        m_statistics.elfLoadSavings += size;
        break;

    // other
    case ArchiveMaxLength:
        m_statistics.archiveMaxLength = Util::Max(m_statistics.archiveMaxLength, static_cast<uint32>(size));
        break;

    default:
        PAL_ALERT_ALWAYS();
        break;
    }
#endif
}

// =====================================================================================================================
// Get the memory size for a multi-ELF archive splitting cache layer
size_t GetMultiElfCacheLayerSize()
{
    return sizeof(MultiElfCacheLayer);
}

// =====================================================================================================================
// Create a multi-ELF archive splitting cache layer
Result CreateMultiElfCacheLayer(
    const MultiElfCacheLayerCreateInfo* pCreateInfo,
    void*                               pPlacementAddr,
    ICacheLayer**                       ppCacheLayer)
{
    PAL_ASSERT(pCreateInfo        != nullptr);
    PAL_ASSERT(pPlacementAddr     != nullptr);
    PAL_ASSERT(ppCacheLayer       != nullptr);

    Result result              = Result::Success;
    MultiElfCacheLayer* pLayer = nullptr;

    if ((pCreateInfo        == nullptr) ||
        (pPlacementAddr     == nullptr) ||
        (ppCacheLayer       == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }

    if (result == Result::Success)
    {
        AllocCallbacks  callbacks = {};

        if (pCreateInfo->baseInfo.pCallbacks == nullptr)
        {
            GetDefaultAllocCb(&callbacks);
        }

        pLayer = PAL_PLACEMENT_NEW(pPlacementAddr) MultiElfCacheLayer(
            (pCreateInfo->baseInfo.pCallbacks == nullptr) ? callbacks : *pCreateInfo->baseInfo.pCallbacks);

        result = pLayer->Init();

        if (result == Result::Success)
        {
            *ppCacheLayer       = pLayer;
        }
        else
        {
            pLayer->Destroy();
            *ppCacheLayer       = nullptr;
        }
    }

    return result;
}

// =====================================================================================================================
// Get detailed statistics for the lifetime of this cache layer
Result GetMultiElfCacheLayerStatistics(
    ICacheLayer*                  pCacheLayer,
    MultiElfCacheLayerStatistics* pStats)
{
    auto pElfLayer = static_cast<MultiElfCacheLayer*>(pCacheLayer);
    *pStats = *pElfLayer->GetMultiElfCacheStatistics();

    return Result::Success;
}

// =====================================================================================================================
// Inform a multi-ELF cache layer which other layer will handle it's modified payloads, to better track hit/miss.
Result SetMultiElfCacheLayerNextDataLayer(
    ICacheLayer* pCacheLayer,
    ICacheLayer* pOtherLayer)
{
    auto pElfLayer = static_cast<MultiElfCacheLayer*>(pCacheLayer);
    pElfLayer->SetNextStorageLayer(pOtherLayer);

    return Result::Success;
}

} //namespace Util
