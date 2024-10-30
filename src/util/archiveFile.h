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
#include "palArchiveFile.h"
#include "palArchiveFileFmt.h"
#include "palFileMap.h"
#include "palInlineFuncs.h"
#include "palLiterals.h"
#include "palList.h"
#include "palMutex.h"
#include "archiveFileHelper.h"

namespace Util
{

// =====================================================================================================================
// Wrapper around a transaction file written in the format specified in palArchiveFileFmt.h
class ArchiveFile : public IArchiveFile
{
public:
    ArchiveFile(
        const AllocCallbacks&         callbacks,
        ArchiveFileHelper::FileHandle hFile,
        const ArchiveFileHeader*      pArchiveHeader,
        bool                          haveWriteAccess);
    virtual ~ArchiveFile();

    Result Init(const ArchiveFileOpenInfo* pInfo);

    virtual size_t GetEntryCount() const override;
    virtual uint64 GetFileSize() const override;

    virtual Result FillEntryHeaderTable(
        ArchiveEntryHeader* pHeaders,
        size_t              startEntry,
        size_t              maxEntries,
        size_t*             pEntriesFilled) override;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 907
    virtual Result GetEntryByIndex(
        size_t              index,
        ArchiveEntryHeader* pHeader) override;
#endif

    virtual Result Read(
        const ArchiveEntryHeader*   pHeader,
        void*                       pDataBuffer) override;

    virtual Result Write(
        ArchiveEntryHeader* pHeader,
        const void*         pData) override;

    virtual bool   AllowWriteAccess() const final { return m_haveWriteAccess; }

    virtual void   Destroy() override { this->~ArchiveFile(); }

private:
    PAL_DISALLOW_DEFAULT_CTOR(ArchiveFile);
    PAL_DISALLOW_COPY_AND_ASSIGN(ArchiveFile);

    void AlignUpMappedSize();
    template<typename T> T CastOffset(size_t offset)
    {
        PAL_ASSERT(offset <= m_fileView.Size());
        return reinterpret_cast<T>(VoidPtrInc(m_fileView.Ptr(), offset));
    }

    // Allocator
    ForwardAllocator*       Allocator() { return &m_allocator; }
    ForwardAllocator        m_allocator;

    // This could easily be a vector
    // but we went with a list to avoid long copies when resizing big vectors
    // and we don't expect to iterate over it often.
    using HeaderOffsetList = List<size_t, ForwardAllocator>;

    // File information
    const ArchiveFileHelper::FileHandle m_hFile;
    const bool                          m_haveWriteAccess;
    HeaderOffsetList                    m_headerOffsetList;
    size_t                              m_curFooterOffset;
    size_t                              m_eofFooterOffset;

    // Mapping information
    FileMapping m_fileMapping;
    FileView    m_fileView;
    size_t      m_curSize;
    size_t      m_memMapAlignSize;

    // We can only have one thread writing at a time. Other threads can read while a write is in progress.
    Mutex  m_writeMutex;

    // Occasionally, we need to grow our FileMapping during a write.
    // We must stop reads from occurring during these expansions.
    RWLock m_expansionLock;
};
} //namespace Util
