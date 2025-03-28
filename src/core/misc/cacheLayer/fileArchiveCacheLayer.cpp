/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "fileArchiveCacheLayer.h"
#include "palMutex.h"
#include "palAssert.h"
#include "palPlatformKey.h"
#include "palHashMapImpl.h"
#include "palAutoBuffer.h"
#include "core/platform.h"

using namespace std::chrono_literals;

namespace Util
{

// =====================================================================================================================
// Class requires and will take ownership of fully initialzed objects for pArchiveFile, pHashProvider, and pBaseContext
FileArchiveCacheLayer::FileArchiveCacheLayer(
    const AllocCallbacks& callbacks,
    IArchiveFile*         pArchiveFile,
    IHashContext*         pBaseContext)
    :
    CacheLayerBase     { callbacks },
    m_pArchivefile     { pArchiveFile },
    m_pBaseContext     { pBaseContext },
    m_entryMapLock     {},
    m_entries          { uint32(GetHashMapNumBuckets(pArchiveFile)), Allocator() }
{
    PAL_ASSERT(m_pArchivefile != nullptr);
    PAL_ASSERT(m_pBaseContext != nullptr);
    PAL_ASSERT(m_pBaseContext->GetOutputBufferSize() <= sizeof(EntryKey));
}

// =====================================================================================================================
FileArchiveCacheLayer::~FileArchiveCacheLayer()
{
    m_pBaseContext->Destroy();
}

// =====================================================================================================================
size_t FileArchiveCacheLayer::GetHashMapNumBuckets(
    const IArchiveFile* pArchiveFile)
{
    constexpr size_t MinExpectedHeaders = 1024;
    size_t numBuckets = MinExpectedHeaders;

    const size_t entryCount = pArchiveFile->GetEntryCount();

    // Generally, if we're opening a file for read only, we don't expect any more headers to be added.
    // We limit the number of buckets here because many files can be open at a time and we don't want to waste memory.
    // However, there is the case of multiple processes (on windows only as of now) where one process will open the
    // file for write, and another will have it open for read. In that specific case, it's possible the parameter
    // chosen here may slow hash map operations down. That's an extreme edge case, but something to be aware of.
    // Even then, the hash map operations should be orders of magnitude faster than the file i/o operations.
    if (entryCount > 0)
    {
        if ((pArchiveFile->AllowWriteAccess() == false) || (entryCount > numBuckets))
        {
            numBuckets = entryCount;
        }
    }

    return numBuckets;
}

// =====================================================================================================================
// Initialize the cache layer
Result FileArchiveCacheLayer::Init()
{
    Result result = CacheLayerBase::Init();

    if (result == Result::Success)
    {
        result = m_entries.Init();
    }

    if (result == Result::Success)
    {
        result = LoadHeaders();
    }

    // Collapse all results other than success
    if (result != Result::Success)
    {
        PAL_ALERT_ALWAYS_MSG("FileArchiveCacheLayer failed to initialize.");
        result = Result::ErrorInitializationFailed;
    }

    return result;
}

// =====================================================================================================================
// Wait for the specified entry ready
Result FileArchiveCacheLayer::WaitForEntry(
    const Hash128* pHashId)
{
    Result result = Result::ErrorInvalidPointer;

    if (pHashId != nullptr)
    {
        EntryKey key;
        result = ConvertToEntryKey(pHashId, &key);

        if (result == Result::Success)
        {
            m_conditionMutex.Lock();
            for (;;)
            {
                {
                    RWLockAuto<RWLock::ReadOnly> lock{ &m_entryMapLock };
                    ArchiveEntryHeader* pHeader = m_entries.FindKey(key);
                    if (pHeader == nullptr)
                    {
                        result = Result::NotFound;
                        break;
                    }
                    else if (pHeader->dataSize > 0)
                    {
                        result = Result::Success;
                        break;
                    }
                }
                m_conditionVariable.Wait(&m_conditionMutex, 500ms);
            }
            m_conditionMutex.Unlock();
        }
    }

    return result;
}

// =====================================================================================================================
// Check if a requested id is present
Result FileArchiveCacheLayer::QueryInternal(
    const Hash128* pHashId,
    QueryResult*   pQuery)
{
    Result result = Result::ErrorUnknown;

    if ((pHashId == nullptr) ||
        (pQuery == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }
    else
    {
        EntryKey     key;
        result = ConvertToEntryKey(pHashId, &key);

        if (result == Result::Success)
        {
            RWLockAuto<RWLock::ReadOnly> entryMapLock { &m_entryMapLock };
            const ArchiveEntryHeader* pHeader = m_entries.FindKey(key);

            if (pHeader != nullptr)
            {
                const size_t storeSize = pHeader->dataSize;

                pQuery->pLayer          = this;
                pQuery->hashId          = *pHashId;
                pQuery->dataSize        = pHeader->metaValue;
                pQuery->storeSize       = storeSize;
                pQuery->promotionSize   = storeSize;
                pQuery->context.entryId = pHeader->ordinalId;

                if (storeSize == 0)
                {
                    result = Result::NotReady;
                }
                else
                {
                    result = Result::Success;
                }
            }
            else
            {
                result = Result::NotFound;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Add data passed in to the cache
Result FileArchiveCacheLayer::StoreInternal(
    Util::StoreFlags    storeFlags,
    const Hash128*      pHashId,
    const void*         pData,
    size_t              dataSize,
    size_t              storeSize)
{
    Result result = Result::Success;
    if (storeFlags.enableFileCache == true)
    {
        result = Result::NotFound;

        PAL_ASSERT(dataSize > 0);
        PAL_ASSERT(storeSize > 0);

        if ((pHashId == nullptr) ||
            (pData == nullptr))
        {
            result = Result::ErrorInvalidPointer;
        }
        else if (m_pArchivefile->AllowWriteAccess() == false)
        {
            result = Result::Unsupported;
        }
        else
        {
            EntryKey key;
            result = ConvertToEntryKey(pHashId, &key);

            if (result == Result::Success)
            {
                ArchiveEntryHeader header         = {};
                const size_t       writeDataSize  = storeSize;
                void* const        pMem           = PAL_MALLOC(writeDataSize, Allocator(), AllocInternalTemp);

                PAL_ALERT(pMem == nullptr);

                if (pMem != nullptr)
                {
                    result = Result::Success;
                }
                else
                {
                    result = Result::ErrorOutOfMemory;
                }

                // Write the scratch buffer to the file
                if (result == Result::Success)
                {
                    void* const pDataMem = pMem;

#if PAL_64BIT_ARCHIVE_FILE_FMT
                    header.dataSize      = writeDataSize;
                    header.metaValue     = dataSize;
#else
                    header.dataSize      = uint32(writeDataSize);
                    header.metaValue     = uint32(dataSize);
#endif

                    memcpy(pDataMem, pData, storeSize);
                    memcpy(header.entryKey, key.value, sizeof(header.entryKey));

                    RWLockAuto<RWLock::ReadWrite> entryMapLock { &m_entryMapLock };
                    ArchiveEntryHeader* pHeader = m_entries.FindKey(key);
                    if ((pHeader != nullptr) && (pHeader->dataSize > 0))
                    {
                        result = Result::AlreadyExists;
                    }
                    else
                    {
                        result = m_pArchivefile->Write(&header, pMem);
                    }

                    // Only insert this entry into our lookup table if everything succeeded
                    if (result == Result::Success)
                    {
                        if (pHeader == nullptr)
                        {
                            result = AddHeaderToTable(header);
                        }
                        else
                        {
                            *pHeader = header;
                        }
                        m_conditionVariable.WakeAll();
                    }
                }

                if (pMem != nullptr)
                {
                    PAL_FREE(pMem, Allocator());
                }
            }
        }

        PAL_ALERT(IsErrorResult(result));
    }
    return result;
}

// =====================================================================================================================
// Copy data from cache to the provided buffer
Result FileArchiveCacheLayer::LoadInternal(
    const QueryResult* pQuery,
    void*              pBuffer)
{
    Result result = Result::Success;

    if ((pQuery == nullptr) || (pBuffer == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }

    if ((result == Result::Success) && (pQuery->pLayer != this))
    {
        result = Result::ErrorInvalidValue;
    }

    EntryKey key;
    result = ConvertToEntryKey(&pQuery->hashId, &key);

    // Copy into a local header so we can release the lock.
    ArchiveEntryHeader header;
    if (result == Result::Success)
    {
        RWLockAuto<RWLock::ReadOnly> entryMapLock { &m_entryMapLock };
        const ArchiveEntryHeader* pHeader = m_entries.FindKey(key);

        if (pHeader == nullptr)
        {
            result = Result::ErrorUnknown;
        }
        else
        {
            header = *pHeader;
        }
    }

    if (result == Result::Success)
    {
        PAL_ALERT(header.ordinalId != pQuery->context.entryId);
        PAL_ALERT(header.metaValue > pQuery->dataSize);

        const size_t readSize      = header.dataSize;
        const size_t dataSize      = header.metaValue;

        const size_t storeSize     = readSize;

        PAL_ASSERT(storeSize == pQuery->storeSize);

        void* const pReadMem = PAL_MALLOC(readSize, Allocator(), AllocInternalTemp);
        void* const pDataMem = pReadMem;

        if (pReadMem == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }

        if (result == Result::Success)
        {
            result = m_pArchivefile->Read(&header, pReadMem);

            // In the case that AsyncIO is not ready, signal Result::NotFound
            if (result == Result::NotReady)
            {
                result = Result::NotFound;
            }

            PAL_ALERT(IsErrorResult(result));
        }

        if (result == Result::Success)
        {
            memcpy(pBuffer, pDataMem, storeSize);
        }

        if (pReadMem != nullptr)
        {
            PAL_FREE(pReadMem, Allocator());
        }
    }

    PAL_ALERT(IsErrorResult(result));

    return result;
}

// =====================================================================================================================
// Reserve an empty Header
Result FileArchiveCacheLayer::Reserve(
    const Hash128* pHashId)
{
    Result result = Result::ErrorInvalidPointer;

    if (pHashId != nullptr)
    {
        if (m_pArchivefile->AllowWriteAccess())
        {
            EntryKey key;
            result = ConvertToEntryKey(pHashId, &key);

            if (result == Result::Success)
            {
                RWLockAuto<RWLock::ReadWrite> lock { &m_entryMapLock };

                ArchiveEntryHeader* pHeader = m_entries.FindKey(key);
                if (pHeader != nullptr)
                {
                    result = Result::AlreadyExists;
                }
                else
                {
                    result = m_entries.Insert(key, {});
                }
            }
        }
        else
        {
            result = Result::NotFound;
        }
    }

    return result;
}

// =====================================================================================================================
// Get the size needed to construct the base context for the layer depending on if an existing platform key is passed
static size_t GetBaseContextSizeFromCreateInfo(
    const ArchiveFileCacheCreateInfo* pCreateInfo)
{
    size_t contextSize = 0;

    if (pCreateInfo->pPlatformKey)
    {
        contextSize = pCreateInfo->pPlatformKey->GetKeyContext()->GetDuplicateObjectSize();
    }
    else
    {
        HashContextInfo info   = {};
        Result          result = GetHashContextInfo(HashAlgorithm::Sha1, &info);

        PAL_ALERT(IsErrorResult(result));

        contextSize = info.contextObjectSize;
    }

    return contextSize;
}

// =====================================================================================================================
// Get the memory size for a archive file backed cache layer
size_t GetArchiveFileCacheLayerSize(
    const ArchiveFileCacheCreateInfo* pCreateInfo)
{
    return sizeof(FileArchiveCacheLayer) + (GetBaseContextSizeFromCreateInfo(pCreateInfo));
}

// =====================================================================================================================
// Create an in-memory key-value caching layer
Result CreateArchiveFileCacheLayer(
    const ArchiveFileCacheCreateInfo* pCreateInfo,
    void*                             pPlacementAddr,
    ICacheLayer**                     ppCacheLayer)
{
    Result                 result          = Result::Success;
    FileArchiveCacheLayer* pLayer          = nullptr;
    IHashContext*          pBaseContext    = nullptr;
    const size_t           hashContextSize = GetBaseContextSizeFromCreateInfo(pCreateInfo);

    if ((pCreateInfo == nullptr) || (pPlacementAddr == nullptr) || (ppCacheLayer == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }

    if (result == Result::Success)
    {
        void* pBaseContextMem = VoidPtrInc(pPlacementAddr, sizeof(FileArchiveCacheLayer));

        if (pCreateInfo->pPlatformKey != nullptr)
        {
            result = pCreateInfo->pPlatformKey->GetKeyContext()->Duplicate(pBaseContextMem, &pBaseContext);
        }
        else
        {
            result = CreateHashContext(HashAlgorithm::Sha1, pBaseContextMem, &pBaseContext);
        }
    }

    if (result == Result::Success)
    {
        AllocCallbacks  callbacks = {};

        if (pCreateInfo->baseInfo.pCallbacks == nullptr)
        {
            GetDefaultAllocCb(&callbacks);
        }

        pLayer = PAL_PLACEMENT_NEW(pPlacementAddr) FileArchiveCacheLayer(
            (pCreateInfo->baseInfo.pCallbacks == nullptr) ? callbacks : *pCreateInfo->baseInfo.pCallbacks,
            pCreateInfo->pFile,
            pBaseContext);

        result = pLayer->Init();

        if (result == Result::Success)
        {
            *ppCacheLayer = pLayer;
        }
        else
        {
            pLayer->Destroy();
        }
    }
    else
    {
        if (pBaseContext != nullptr)
        {
            pBaseContext->Destroy();
        }
    }

    return result;
}

// =====================================================================================================================
Result GetArchiveFileCacheLayerCurSize(
    ICacheLayer*    pCacheLayer,
    uint64*         pCurCount,    // [out] number of entries in cache
    uint64*         pCurSize)     // [out] size of backing archive file
{
    auto pFileArchiveCache = static_cast<FileArchiveCacheLayer*>(pCacheLayer);

    // Note that the current file size does not include un-flushed data or pending writes.
    return pFileArchiveCache->GetFileCacheSize(pCurCount, pCurSize);
}

// =====================================================================================================================
// Attempt to add an entry header to our table
Result FileArchiveCacheLayer::AddHeaderToTable(
    const ArchiveEntryHeader& header)
{
    EntryKey key;

    memcpy(key.value, header.entryKey, sizeof(header.entryKey));

    // Note in this case the "dataSize" in the file is how much is stored.
    // The *actual* data size, we store as metadata.
    return m_entries.Insert(key, header);
}

// =====================================================================================================================
// Reload entry headers from the archive file
Result FileArchiveCacheLayer::LoadHeaders()
{
    RWLockAuto<RWLock::ReadWrite> entryMapLock { &m_entryMapLock };

    Result       result        = Result::Success;
    const size_t curFileCount  = m_pArchivefile->GetEntryCount();
    size_t       curEntryCount = m_entries.GetNumEntries();
    const size_t numNewEntries = curFileCount - curEntryCount;

    if (numNewEntries > 0)
    {
        AutoBuffer<ArchiveEntryHeader, 8, ForwardAllocator> headerTable { numNewEntries, Allocator() };

        size_t entriesFilled = 0;
        result = m_pArchivefile->FillEntryHeaderTable(headerTable.Data(),
                                                      curEntryCount,
                                                      headerTable.Capacity(),
                                                      &entriesFilled);

        if (result == Result::Success)
        {
            for (size_t i = 0; i < entriesFilled; i++)
            {
                PAL_ALERT(headerTable[i].ordinalId != curEntryCount);

                result = AddHeaderToTable(headerTable[i]);

                if (IsErrorResult(result))
                {
                    PAL_ALERT_ALWAYS();
                    break;
                }

                curEntryCount++;
            }
        }
    }

    PAL_ASSERT(curEntryCount == curFileCount);

    return result;
}

// =====================================================================================================================
// Convert a 128-bit hash to a SHA1 entry id
Result FileArchiveCacheLayer::ConvertToEntryKey(
    const Hash128* pHashId,
    EntryKey*      pKey)
{
    PAL_ASSERT(pHashId != nullptr);
    PAL_ASSERT(pKey != nullptr);

    memset(pKey, 0, sizeof(EntryKey));

    Result result = Result::Success;

    // Generally the duplicate object size is < 1K, and if so then put it on the stack.
    // But allow larger sizes if necessary.
    const size_t objectSize = m_pBaseContext->GetDuplicateObjectSize();
    AutoBuffer<uint8, 1024, ForwardAllocator> contextMem { objectSize, Allocator() };
    if (contextMem.Capacity() < objectSize)
    {
        result = Result::ErrorOutOfMemory;
    }

    IHashContext* pContext = nullptr;

    if (result == Result::Success)
    {
        result = m_pBaseContext->Duplicate(contextMem.Data(), &pContext);
    }

    if (result == Result::Success)
    {
        result = pContext->AddData(pHashId, sizeof(Hash128));
    }

    if (result == Result::Success)
    {
        result = pContext->Finish(pKey->value);
    }

    if (pContext)
    {
        pContext->Destroy();
    }

    PAL_ALERT(IsErrorResult(result));

    return result;
}

} //namespace Util
