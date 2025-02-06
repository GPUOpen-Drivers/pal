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

// pal
#include "archiveFile.h"
#include "palAssert.h"
#include "palMetroHash.h"
#include "palSysUtil.h"
#include "palPlatformKey.h"
#include "palInlineFuncs.h"
#include "palListImpl.h"
#include "palSysMemory.h"

// stl
#include <cstddef>
#include <cstdio>

namespace Util
{
// =====================================================================================================================
// Global Util functions defined in palArchiveFile.h
// =====================================================================================================================

// =====================================================================================================================
// Get the memory size needed for an archive file object
size_t GetArchiveFileObjectSize(
    const ArchiveFileOpenInfo* pOpenInfo)
{
    return sizeof(ArchiveFile);
}

// =====================================================================================================================
// Opens a file on disk as a "PAL Archive File"
Result OpenArchiveFile(
    const ArchiveFileOpenInfo* pOpenInfo,
    void*                      pPlacementAddr,
    IArchiveFile**             ppArchiveFile)
{
    Result result = Result::Success;

    if ((pOpenInfo      == nullptr) ||
        (pPlacementAddr == nullptr) ||
        (ppArchiveFile  == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }

    ArchiveFileHelper::FileHandle hFile = ArchiveFileHelper::InvalidFileHandle;

    if (result == Result::Success)
    {
        char stringBuffer[PathBufferLen] = {};
        ArchiveFileHelper::GenerateFullPath(stringBuffer, sizeof(stringBuffer), pOpenInfo);
        // Only attempt to create the folder paths if we were going to write the file to begin with
        if (pOpenInfo->allowCreateFile)
        {
            result = ArchiveFileHelper::CreateFileInternal(stringBuffer, pOpenInfo);
        }

        // Result::AlreadyExists may be returned so check for Errors instead of Result::Success
        if (IsErrorResult(result) == false)
        {
            result = ArchiveFileHelper::OpenFileInternal(&hFile, stringBuffer, pOpenInfo);
        }
    }

    ArchiveFileHeader fileHeader = {};

    if (result == Result::Success)
    {
        // Inside here we have to clean up hFile on failure
        PAL_ALERT(hFile == ArchiveFileHelper::InvalidFileHandle);

        result = ArchiveFileHelper::ReadDirect(hFile, 0, &fileHeader, sizeof(fileHeader));

        if (result == Result::Success)
        {
            result = ArchiveFileHelper::ValidateFile(pOpenInfo, &fileHeader);
        }

        if (result != Result::Success)
        {
            ArchiveFileHelper::CloseFileHandle(hFile);
        }
    }

    if (result == Result::Success)
    {
        // Ownership of hFile is given to ArchiveFile in the constructor
        AllocCallbacks  callbacks = {};

        if (pOpenInfo->pMemoryCallbacks == nullptr)
        {
            GetDefaultAllocCb(&callbacks);
        }

        ArchiveFile* pArchiveFile = PAL_PLACEMENT_NEW(pPlacementAddr) ArchiveFile(
            (pOpenInfo->pMemoryCallbacks == nullptr) ? callbacks : *pOpenInfo->pMemoryCallbacks,
            hFile,
            &fileHeader,
            pOpenInfo->allowWriteAccess);

        hFile  = ArchiveFileHelper::InvalidFileHandle;
        result = pArchiveFile->Init(pOpenInfo);

        if (result == Result::Success)
        {
            *ppArchiveFile = pArchiveFile;
        }
        else
        {
            *ppArchiveFile = nullptr;
            pArchiveFile->Destroy();

            // If the result is anything other than out of memory and incompatible library, simplify it to an Init failure
            if ((result != Result::ErrorOutOfMemory) &&
                (result != Result::ErrorIncompatibleLibrary))
            {
                result = Result::ErrorInitializationFailed;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Create a blank archive
Result CreateArchiveFile(
    const ArchiveFileOpenInfo* pOpenInfo)
{
    PAL_ASSERT(pOpenInfo != nullptr);

    Result result = Result::ErrorInvalidPointer;

    if (pOpenInfo != nullptr)
    {
        char stringBuffer[PathBufferLen] = {};
        ArchiveFileHelper::GenerateFullPath(stringBuffer, sizeof(stringBuffer), pOpenInfo);
        result = ArchiveFileHelper::CreateFileInternal(stringBuffer, pOpenInfo);
    }

    return result;
}

// =====================================================================================================================
// Attempt to delete an archive file on disc
Result DeleteArchiveFile(
    const ArchiveFileOpenInfo* pOpenInfo)
{
    PAL_ASSERT(pOpenInfo != nullptr);

    Result result = Result::ErrorInvalidPointer;

    if (pOpenInfo != nullptr)
    {
        result = Result::Success;
        char stringBuffer[PathBufferLen] = {};
        ArchiveFileHelper::GenerateFullPath(stringBuffer, sizeof(stringBuffer), pOpenInfo);
        result = ArchiveFileHelper::DeleteFileInternal(stringBuffer);
    }

    return result;
}

// =====================================================================================================================
// ArchiveFile Class implementation
// =====================================================================================================================

// =====================================================================================================================
ArchiveFile::ArchiveFile(
    const AllocCallbacks&         callbacks,
    ArchiveFileHelper::FileHandle hFile,
    const ArchiveFileHeader*      pArchiveHeader,
    bool                          haveWriteAccess)
    :
    // File Information
    m_allocator                 { callbacks },
    m_hFile                     { hFile },
    m_haveWriteAccess           { haveWriteAccess },
    m_headerOffsetList          { Allocator() },
    m_curFooterOffset           { 0 },
    m_eofFooterOffset           { 0 },
    m_fileMapping               { },
    m_fileView                  { },
    m_curSize                   { 0 },
    m_memMapAlignSize           { 0 },
    m_writeMutex                { },
    m_expansionLock             { }
{
}

// =====================================================================================================================
ArchiveFile::~ArchiveFile()
{
    while (m_headerOffsetList.NumElements() > 0)
    {
        auto it = m_headerOffsetList.Begin();
        m_headerOffsetList.Erase(&it);
    }

    // No need to flush the view here because we're about to close the file which will implicitly perform a flush.
    m_fileView.UnMap(false);
    m_fileMapping.Close();
}

// =====================================================================================================================
// Due to possible failure on object creation, Init() is required to be called before the object is usable
Result ArchiveFile::Init(
    const ArchiveFileOpenInfo* pInfo)
{
    Result result = Result::ErrorUnknown;

    // Due to the TRY_ACCESS_FILE_VIEW macro's use of exceptions on win32, we cannot use MutexAuto.
    m_writeMutex.Lock();

    size_t fileSize = ArchiveFileHelper::GetFileSize(m_hFile);
    if (fileSize > 0)
    {
        m_curSize = fileSize;

        if (m_curSize >= sizeof(ArchiveFileFooter))
        {
            m_eofFooterOffset = m_curSize - sizeof(ArchiveFileFooter);
            result = Result::Success;
        }
    }

    if (result == Result::Success)
    {
        AlignUpMappedSize();
        result = m_fileMapping.CreateFromHandle(m_hFile, m_haveWriteAccess, m_curSize);
    }

    if (result == Result::Success)
    {
        m_fileView.Map(m_fileMapping, m_haveWriteAccess, 0, m_curSize);
        if (m_fileView.IsValid())
        {
            result = Result::ErrorInvalidFormat;

            const ArchiveFileFooter* eofFooter = CastOffset<ArchiveFileFooter*>(m_eofFooterOffset);
            TRY_ACCESS_FILE_VIEW (eofFooter != nullptr)
            {
                if (ArchiveFileHelper::ValidateFooter(eofFooter))
                {
                    if (m_haveWriteAccess)
                    {
                        // There's an edge case where we map the file for the first time, causing it to resize,
                        // and don't write anything. We need to make sure the EOF footer exists.
                        if (m_eofFooterOffset != (m_curSize - sizeof(ArchiveFileFooter)))
                        {
                            m_eofFooterOffset = m_curSize - sizeof(ArchiveFileFooter);
                            ArchiveFileFooter* newEofFooter = CastOffset<ArchiveFileFooter*>(m_eofFooterOffset);
                            memcpy(newEofFooter, eofFooter, sizeof(ArchiveFileFooter));
                        }
                    }

                    result = Result::Success;
                }
            }
            CATCH_ACCESS_FILE_VIEW
            {
                PAL_ASSERT_ALWAYS();
            }
        }
        else
        {
            result = Result::ErrorInitializationFailed;
        }
    }

    if (result == Result::Success)
    {
        ArchiveFileHeader* header = reinterpret_cast<ArchiveFileHeader*>(m_fileView.Ptr());
        size_t curOffset = header->firstBlock;
        while (curOffset < m_curSize)
        {
            result = Result::ErrorUnknown;

            const ArchiveFileFooter* footerCheck = CastOffset<ArchiveFileFooter*>(curOffset);
            TRY_ACCESS_FILE_VIEW (footerCheck != nullptr)
            {
                if (ArchiveFileHelper::ValidateFooter(footerCheck))
                {
                    // We've actually found a footer marker, not an entry marker.
                    m_curFooterOffset = curOffset;
                    if (m_headerOffsetList.NumElements() == footerCheck->entryCount)
                    {
                        result = Result::Success;
                    }
                    else
                    {
                        result = Result::ErrorInvalidFormat;
                    }
                    break;
                }
                else
                {
                    ArchiveEntryHeader* pCurHeader = CastOffset<ArchiveEntryHeader*>(curOffset);
                    if (memcmp(pCurHeader->entryMarker, MagicEntryMarker, sizeof(MagicEntryMarker)) != 0)
                    {
                        result = Result::ErrorInvalidFormat;
                        break;
                    }
                    else
                    {
                        if (m_headerOffsetList.NumElements() == pCurHeader->ordinalId)
                        {
                            m_headerOffsetList.PushBack(curOffset);
                            curOffset = pCurHeader->nextBlock;
                            result = Result::Success;
                        }
                        else
                        {
                            result = Result::ErrorInvalidFormat;
                            break;
                        }
                    }
                }
            }
            CATCH_ACCESS_FILE_VIEW
            {
                PAL_ASSERT_ALWAYS();
                break;
            }
        }
    }

    // Due to the TRY_ACCESS_FILE_VIEW macro's use of exceptions on win32, we cannot use MutexAuto.
    m_writeMutex.Unlock();

    return result;
}

//======================================================================================================================
void ArchiveFile::AlignUpMappedSize()
{
    if (m_haveWriteAccess)
    {
        size_t mapSize = 4096; // Start at 4k (default ntfs disk cluster/min file size on disk).
        constexpr size_t maxGrowthSize = 64 * 1024 * 1024; // Don't grow more than 64MB at at time at most.
        while ((mapSize < m_curSize) && (mapSize < maxGrowthSize))
        {
            mapSize *= 2;
        }
        m_curSize = Util::Pow2Align(m_curSize, mapSize);
    }
}

// =====================================================================================================================
// Returns the number of "good" entries found within the archive
size_t ArchiveFile::GetEntryCount() const
{
    return m_headerOffsetList.NumElements();
}

// =====================================================================================================================
// Returns the size of the archive
uint64 ArchiveFile::GetFileSize() const
{
    return m_fileView.Size();
}

// =====================================================================================================================
// Read the value corresponding to the entry header passed in from the archive
Result ArchiveFile::Read(
    const ArchiveEntryHeader* pHeader,
    void*                     pDataBuffer)
{
    PAL_ASSERT(pHeader != nullptr);
    PAL_ASSERT(pDataBuffer != nullptr);

    Result result = Result::ErrorUnknown;

    if ((pHeader == nullptr) ||
        (pDataBuffer == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }
    else
    {
        // Due to the TRY_ACCESS_FILE_VIEW macro's use of exceptions on win32, we cannot use RWLockAuto.
        m_expansionLock.LockForRead();

        // Sanity check our arguments before attempting the read
        if ((pHeader->dataPosition + pHeader->dataSize) <= m_curFooterOffset)
        {
            void* pFileData = CastOffset<void*>(pHeader->dataPosition);
            TRY_ACCESS_FILE_VIEW (pFileData != nullptr)
            {
                memcpy(pDataBuffer, pFileData, pHeader->dataSize);

                result = Result::Success;
            }
            CATCH_ACCESS_FILE_VIEW
            {
                PAL_ASSERT_ALWAYS();
            }
        }
        else
        {
            result = Result::ErrorInvalidValue;
        }

        // Due to the TRY_ACCESS_FILE_VIEW macro's use of exceptions on win32, we cannot use RWLockAuto.
        m_expansionLock.UnlockForRead();
    }

    // Verify our data was read in as expected. This does not guarantee that the payload is valid, merely that no errors
    // ocurred during the file read
    if (result == Result::Success)
    {
        const uint64 crc = ArchiveFileHelper::Crc64(pDataBuffer, pHeader->dataSize);

        if (crc != pHeader->dataCrc64)
        {
            PAL_ALERT_ALWAYS();

            result = Result::ErrorIncompatibleLibrary;
        }
    }

    return result;
}

// =====================================================================================================================
// Write a header+data pair to the archive
Result ArchiveFile::Write(
    ArchiveEntryHeader* pHeader,
    const void*         pData)
{
    Result result = Result::ErrorUnknown;

    if ((pHeader == nullptr) ||
        (pData == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (m_haveWriteAccess)
    {
        // Only one write can be in progress at a time. Too much state changes otherwise.
        // Due to the TRY_ACCESS_FILE_VIEW macro's use of exceptions on win32, we cannot use MutexAuto.
        m_writeMutex.Lock();

        // cache off the write location
        const size_t curOffset = m_curFooterOffset;

        memcpy(pHeader->entryMarker, MagicEntryMarker, sizeof(MagicEntryMarker));
#if PAL_64BIT_ARCHIVE_FILE_FMT
        pHeader->ordinalId     = m_headerOffsetList.NumElements();
#else
        pHeader->ordinalId     = uint32(m_headerOffsetList.NumElements());
#endif

        // Save the arithmetic in 64-bit types so we can test for overflow when casting to 32-bit.
        const size_t dataPosition = curOffset + sizeof(ArchiveEntryHeader);
        const size_t nextBlock    = dataPosition + pHeader->dataSize;
#if PAL_64BIT_ARCHIVE_FILE_FMT
        pHeader->nextBlock     = nextBlock;
        pHeader->dataPosition  = dataPosition;
#else
        pHeader->nextBlock     = uint32(nextBlock);
        pHeader->dataPosition  = uint32(dataPosition);
        if (nextBlock != size_t(pHeader->nextBlock))
        {
            PAL_ASSERT_ALWAYS_MSG(
                "Overflow! The 32-bit cache has grown too large. Consider setting PAL_64BIT_ARCHIVE_FILE_FMT.");
            result = Result::ErrorOutOfMemory;
        }
#endif
        pHeader->dataCrc64     = ArchiveFileHelper::Crc64(pData, pHeader->dataSize);

        const size_t writeSize = sizeof(ArchiveEntryHeader) + pHeader->dataSize + sizeof(ArchiveFileFooter);

        // We need to pause reads while we expand the mapping.
        // We add an extra sizeof(ArchiveFileFooter) here to make sure we have room for the EOF footer.
        const size_t totalSizeNeeded = (curOffset + writeSize + sizeof(ArchiveFileFooter));
        if (totalSizeNeeded > m_curSize)
        {
            // Due to the TRY_ACCESS_FILE_VIEW macro's use of exceptions on win32, we cannot use RWLockAuto.
            m_expansionLock.LockForWrite();

            m_curSize = totalSizeNeeded;
            AlignUpMappedSize();

            // No need to flush the view here because ReloadMap will implicitly perform a flush.
            m_fileView.UnMap(false);
            m_fileMapping.ReloadMap(m_curSize);
            m_fileView.Map(m_fileMapping, m_haveWriteAccess, 0, m_curSize);

            m_eofFooterOffset = m_curSize - sizeof(ArchiveFileFooter);

            // Due to the TRY_ACCESS_FILE_VIEW macro's use of exceptions on win32, we cannot use RWLockAuto.
            m_expansionLock.UnlockForWrite();
        }

        void* pBuffer = CastOffset<void*>(curOffset);

        TRY_ACCESS_FILE_VIEW (pBuffer != nullptr)
        {
            void* pOutData   = VoidPtrInc(pBuffer, sizeof(ArchiveEntryHeader));
            void* pOutFooter = VoidPtrInc(pOutData, pHeader->dataSize);

            // Copy the current footer to after the new entry, and update it.
            const ArchiveFileFooter* pCurFooter = CastOffset<ArchiveFileFooter*>(curOffset);
            memcpy(pOutFooter, pCurFooter, sizeof(ArchiveFileFooter));
            static_cast<ArchiveFileFooter*>(pOutFooter)->entryCount += 1;
            static_cast<ArchiveFileFooter*>(pOutFooter)->lastWriteTimestamp = ArchiveFileHelper::GetCurrentFileTime();

            // Write the data.
            memcpy(pBuffer, pHeader, sizeof(ArchiveEntryHeader));
            memcpy(pOutData, pData, pHeader->dataSize);

            // Update the EOF footer by copying the new post-entry footer to the end of the file.
            // We need the EOF footer to maintain backwards compatibility with the archive file spec/older versions,
            // as there are external utilities written against the published file spec.
            // We need to write this now as opposed to at shutdown time because many apps exit by killing the process
            // and we never get a proper flush(). The OS will handle flushing any cached writes from the memory map
            // to the physical file.
            ArchiveFileFooter* pEofFooter = CastOffset<ArchiveFileFooter*>(m_eofFooterOffset);
            memcpy(pEofFooter, pOutFooter, sizeof(ArchiveFileFooter));

            result = Result::Success;
        }
        CATCH_ACCESS_FILE_VIEW
        {
            PAL_ASSERT_ALWAYS();
        }

        if (result == Result::Success)
        {
            m_curFooterOffset = pHeader->nextBlock;
            result = m_headerOffsetList.PushBack(curOffset);
        }

        // Due to the TRY_ACCESS_FILE_VIEW macro's use of exceptions on win32, we cannot use MutexAuto.
        m_writeMutex.Unlock();
    }
    else
    {
        result = Result::Unsupported;
    }

    return result;
}

// =====================================================================================================================
// Fills an array with header information
Result ArchiveFile::FillEntryHeaderTable(
    ArchiveEntryHeader* pHeaders,
    size_t              startEntry,
    size_t              maxEntries,
    size_t*             pEntriesFilled)
{
    Result result = Result::ErrorUnknown;

    if ((pHeaders == nullptr) ||
        (pEntriesFilled == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (maxEntries > 0)
    {
        result = Result::ErrorInvalidValue;

        // Due to the TRY_ACCESS_FILE_VIEW macro's use of exceptions on win32, we cannot use MutexAuto.
        // Don't want to write during list operations, but this should be called very sparingly.
        // This also precludes map expansion, so no need to lock the expansion lock too.
        m_writeMutex.Lock();

        if (startEntry < m_headerOffsetList.NumElements())
        {
            size_t curIndex = 0;
            auto listIter = m_headerOffsetList.Begin();
            auto listEnd  = m_headerOffsetList.End();
            while ((listIter != listEnd) && (curIndex < startEntry))
            {
                curIndex++;
                listIter.Next();
            }

            while ((listIter != listEnd) && ((*pEntriesFilled) < maxEntries))
            {
                size_t entryHeaderOffset = *(listIter.Get());
                ArchiveEntryHeader* pHeaderInFile = CastOffset<ArchiveEntryHeader*>(entryHeaderOffset);

                TRY_ACCESS_FILE_VIEW (pHeaderInFile != nullptr)
                {
                    PAL_ASSERT(pHeaderInFile->ordinalId == curIndex);

                    // Copy the header info.
                    memcpy(&(pHeaders[*pEntriesFilled]), pHeaderInFile, sizeof(ArchiveEntryHeader));
                }
                CATCH_ACCESS_FILE_VIEW
                {
                    PAL_ASSERT_ALWAYS();
                    break;
                }

                (*pEntriesFilled)++;
                curIndex++;
                listIter.Next();
            }

            if ((*pEntriesFilled) > 0)
            {
                result = Result::Success;
            }
        }

        // Due to the TRY_ACCESS_FILE_VIEW macro's use of exceptions on win32, we cannot use MutexAuto.
        m_writeMutex.Unlock();
    }

    return result;
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 907
// =====================================================================================================================
// Deprecated and seemingly unused now.
Result ArchiveFile::GetEntryByIndex(
    size_t              index,
    ArchiveEntryHeader* pHeader)
{
    Result result = Result::ErrorUnknown;

    size_t entriesFilled = 0;
    result = FillEntryHeaderTable(pHeader, index, 1, &entriesFilled);
    PAL_ASSERT(entriesFilled == 1);

    return result;
}
#endif

} //namespace Util
