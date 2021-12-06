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
#include "palArchiveFile.h"
#include "palArchiveFileFmt.h"
#include "palIntrusiveList.h"
#include "palLinearAllocator.h"
#include "palVector.h"

namespace Util
{
constexpr int32 InvalidSysCall = -1; // value representing system call happens error for Linux

// =====================================================================================================================
// Wrapper around a transaction file written int the format specified in palArchiveFileFmt.h
class ArchiveFile : public IArchiveFile
{
public:
    ArchiveFile(
            const AllocCallbacks&    callbacks,
            int32                    hFile,
            const ArchiveFileHeader* pArchiveHeader,
            bool                     haveWriteAccess,
            size_t                   memoryBufferMax);
    virtual ~ArchiveFile();

    Result Init(const ArchiveFileOpenInfo* pInfo);

    virtual size_t GetEntryCount() const override;

    virtual Result Preload(
        size_t startLocation,
        size_t maxReadSize) override;

    virtual Result FillEntryHeaderTable(
        ArchiveEntryHeader* pHeaders,
        size_t              startEntry,
        size_t              maxEntries,
        size_t*             pEntriesFilled) override;

    virtual Result GetEntryByIndex(
        size_t              index,
        ArchiveEntryHeader* pHeader) override;

    virtual Result Read(
        const ArchiveEntryHeader*   pHeader,
        void*                       pDataBuffer) override;

    virtual Result Write(
        ArchiveEntryHeader* pHeader,
        const void*         pData) override;

    virtual void   Destroy() override { this->~ArchiveFile(); }

private:
    PAL_DISALLOW_DEFAULT_CTOR(ArchiveFile);
    PAL_DISALLOW_COPY_AND_ASSIGN(ArchiveFile);

    // =================================================================================================================
    // Describes a cached page of memory read from the file
    class PageInfo
    {
    public:
        using List = IntrusiveList<PageInfo>;
        using Node = IntrusiveListNode<PageInfo>;
        using Iter = IntrusiveListIterator<PageInfo>;

        PageInfo() :
            m_beginOffset   { 0 },
            m_pMem          { nullptr },
            m_memSize       { 0 },
            m_node          { this }
            {}
        void Init(void* pMem, size_t memSize) { m_pMem = pMem; m_memSize = memSize; }

        // Page->Memory interface
        void* Contains(size_t offset);

        // I/O Control
        Result Load(int32 fd, size_t fileOffset, bool useAsyncIo);
        Result Reload(int32 fd, bool useAsyncIo) { return Load(fd, m_beginOffset, useAsyncIo); }
        bool   IsLoaded() { return true; }
        void   Wait() {}
        void   Cancel() {}

        // LRU list node
        Node* ListNode() { return &m_node; }

    private:
        PAL_DISALLOW_COPY_AND_ASSIGN(PageInfo);

        size_t       m_beginOffset; // Location in file where page begins
        void*        m_pMem;        // Memory backing this page
        size_t       m_memSize;     // Size of memory page
        Node         m_node;        // Page's position in an LRU chain
    };

    Result RefreshFile(bool forceRefresh);

    Result ReadNextEntry(ArchiveEntryHeader* pCurheader, ArchiveEntryHeader* pNextHeader);

    Result ReadInternal(size_t fileOffset, void* pBuffer, size_t readSize, bool forceCacheReload, bool wait);
    Result WriteInternal(size_t fileOffset, const void* pData, size_t writeSize);

    // "Cached" I/O API
    Result ReadCached(size_t fileOffset, void* pBuffer, size_t readSize, bool forceReload, bool wait);
    Result WriteCached(size_t fileOffset, const void* pData, size_t writeSize);

    // Page management
    Result    InitPages();
    PageInfo* FindPage(size_t fileOffset, bool loadOnMiss, bool forceReload);
    int32     CalcPageIndex(size_t fileOffset) const        { return static_cast<int32>(fileOffset / m_pageSize); }
    size_t    CalcNextPageBoundary(size_t fileOffset) const { return (CalcPageIndex(fileOffset) + 1) * m_pageSize; }

    // Paged memory should total 512 MB max
    static constexpr size_t MaxPageCount = 64;
    static constexpr size_t MaxPageSize  = 8 * 1024 * 1024;
    static constexpr size_t MinPageSize  = 256 * 1024;

    using EntryVector = Vector<ArchiveEntryHeader, 16, ForwardAllocator>;

    // Allocator
    ForwardAllocator*       Allocator() { return &m_allocator; }
    ForwardAllocator        m_allocator;

    // File information
    const int32             m_hFile;
    const ArchiveFileHeader m_archiveHeader;
    uint64                  m_fileSize;
    ArchiveFileFooter       m_cachedFooter;
    uint32                  m_curFooterOffset;
    EntryVector             m_entries;

    // Write components: MAY NOT BE INITIALIZED IF WE DON'T HAVE WRITE ACCESS
    const bool              m_haveWriteAccess;
    bool                    m_refreshedSinceLastWrite;

    // Internal memory buffer: MAY NOT BE INITIALIZED IF WE AREN'T USING A MEMORY BUFFER
    bool                    m_useBufferedMemory;
    VirtualLinearAllocator  m_bufferMemory;
    PageInfo::List          m_recentList;
    PageInfo                m_pages[MaxPageCount];
    size_t                  m_pageCount;
    size_t                  m_pageSize;
};

} //namespace Util
