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
#pragma once

#include "cacheLayerBase.h"

#include "palArchiveFileFmt.h"
#include "palArchiveFile.h"
#include "palConditionVariable.h"
#include "palLinearAllocator.h"
#include "palHashProvider.h"
#include "palHashMap.h"

namespace Util
{

// =====================================================================================================================
// An ICacheLayer implementation that interacts closely with a file archive
class FileArchiveCacheLayer : public CacheLayerBase
{
public:
    FileArchiveCacheLayer(
        const AllocCallbacks& callbacks,
        IArchiveFile*         pArchiveFile,
        IHashContext*         pBaseContext);
    virtual ~FileArchiveCacheLayer();

    virtual Result Init() override;

    virtual Result WaitForEntry(const Hash128* pHashId) override;

    Result GetFileCacheSize(uint64* pCurCount, uint64* pCurSize) const
    {
        *pCurCount = static_cast<uint64>(m_entries.GetNumEntries());
        *pCurSize  = static_cast<uint64>(m_pArchivefile->GetFileSize());

        return Result::Success;
    }

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

    virtual Result Reserve(
        const Hash128* pHashId) override;

private:
    PAL_DISALLOW_DEFAULT_CTOR(FileArchiveCacheLayer);
    PAL_DISALLOW_COPY_AND_ASSIGN(FileArchiveCacheLayer);

    // Helper type for ArchiveEntryHeader::entryKey
    struct EntryKey
    {
        uint8 value[sizeof(ArchiveEntryHeader::entryKey)];
    };

    using EntryMap = HashMap<EntryKey,
                             ArchiveEntryHeader,
                             ForwardAllocator,
                             JenkinsHashFunc,
                             DefaultEqualFunc,
                             HashAllocator<ForwardAllocator>,
                             2048>;

    // Hashing Utility functions
    Result ConvertToEntryKey(const Hash128* pHashId, EntryKey* pKey);

    Result AddHeaderToTable(const ArchiveEntryHeader& header);
    Result LoadHeaders();

    // Helper function for constructor
    static size_t GetHashMapNumBuckets(const IArchiveFile* pArchiveFile);

    // Invariants that must be passed in by ctor
    IArchiveFile* const  m_pArchivefile;
    IHashContext* const  m_pBaseContext;

    RWLock               m_entryMapLock;

    Mutex                m_conditionMutex;    // Mutex that will be used with the condition variable
    ConditionVariable    m_conditionVariable; // used for waiting on Entry::ready

    // Data Members
    EntryMap m_entries;
};

} //namespace Util
