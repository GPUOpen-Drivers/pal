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
#include "core/platform.h"
#include "util/lnx/lnxArchiveFile.h"

#include "palAssert.h"
#include "palInlineFuncs.h"
#include "palIntrusiveListImpl.h"
#include "palMetroHash.h"
#include "palPlatformKey.h"
#include "palSysUtil.h"
#include "palVectorImpl.h"

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

namespace Util
{

// =====================================================================================================================
// Generate a full path from ArchiveFileOpenInfo
static char* GenerateFullPath(
    char*                      pStringBuffer,
    size_t                     stringBufferSize,
    const ArchiveFileOpenInfo* pOpenInfo)
{
    PAL_ASSERT(pStringBuffer    != nullptr);
    PAL_ASSERT(pOpenInfo        != nullptr);
    PAL_ASSERT(stringBufferSize != 0);

    Strncpy(pStringBuffer, pOpenInfo->pFilePath, stringBufferSize);
    Strncat(pStringBuffer, stringBufferSize, "/");
    Strncat(pStringBuffer, stringBufferSize, pOpenInfo->pFileName);

    return pStringBuffer;
}

// =====================================================================================================================
// Convert a FILETIME to uint64
static uint64 UnixTimeToFileTimeToU64(
    const uint64 unixTimeStamp)
{
    // FILETIME starts from 1601-01-01 UTC, epoch from 1970-01-01
    constexpr uint64 EpochDiff = 116444736000000000;
    // 100ns
    constexpr uint64 RateDiff = 10000000;

    return unixTimeStamp * RateDiff + EpochDiff;
}

// =====================================================================================================================
// Get the earliest known good file time for a PAL archive footer: 1 January, 2018
static uint64 EarliestValidFileTime()
{
    struct tm earliestTime = { };
    earliestTime.tm_mday = 1; // Day of the month (1-31)
    earliestTime.tm_wday = 1; // Day of the week (0-6, Sunday = 0), 2018-01-01 is a Monday
    earliestTime.tm_year = 2018 - 1900;

    const uint64 unixTime = mktime(&earliestTime);

    return UnixTimeToFileTimeToU64(unixTime);
}

// =====================================================================================================================
// Helper function to get current time as a 64 bit integer in FILETIME scale
static uint64 GetCurrentFileTime()
{
    const uint64 unixTime = time(nullptr);

    return UnixTimeToFileTimeToU64(unixTime);
}

// =====================================================================================================================
// Helper function around MetroHash64 for easy crc64 hashing
static uint64 Crc64(
    const void* pData,
    size_t      dataSize,
    uint64      seed = 0)
{
    PAL_ASSERT(pData != nullptr);

    union {
        uint64 crc64;
        uint8  raw[8];
    } hashOutput;
    hashOutput.crc64 = 0;
    MetroHash64::Hash(static_cast<const uint8*>(pData), dataSize, hashOutput.raw, seed);

    return hashOutput.crc64;
}

// =====================================================================================================================
// Helper function to read directly from a file using Linux API
static Result ReadDirect(
    int32   fd,
    size_t  fileOffset,
    void*   pBuffer,
    size_t  readSize)
{
    PAL_ASSERT(fd   > 0);
    PAL_ASSERT(pBuffer != nullptr);

    struct stat statBuf;
    Result result    = Result::ErrorUnknown;

    if (fstat(fd, &statBuf) == 0)
    {
        result = Result::Success;
    }

    if (result == Result::Success)
    {
        if (lseek(fd, fileOffset, SEEK_SET) == InvalidSysCall)
        {
            result = ConvertErrno(errno);
        }
    }

    size_t alreadyReadSize = 0;
    size_t exactSize = Min(readSize, (static_cast<size_t>(statBuf.st_size) - fileOffset));

    if (result == Result::Success)
    {
        alreadyReadSize = read(fd, pBuffer, exactSize);
    }

    if (alreadyReadSize == exactSize)
    {
        result = Result::Success;
    }
    else
    {
        result = ConvertErrno(errno);
        PAL_ALERT_ALWAYS();
    }

    return result;
}

// =====================================================================================================================
// Helper function to write directly to a file using Linux API
static Result WriteDirect(
    int32       fd,
    size_t      fileOffset,
    const void* pData,
    size_t      writeSize)
{
    PAL_ASSERT(fd > 0);
    PAL_ASSERT(pData != nullptr);

    Result result       = Result::ErrorUnknown;

    if (lseek(fd, fileOffset, SEEK_SET) == InvalidSysCall)
    {
        result = ConvertErrno(errno);
        PAL_ALERT_ALWAYS();
        return result;
    }

    size_t alreadyWriteSize = write(fd, pData, writeSize);

    if (alreadyWriteSize == writeSize)
    {
        result = Result::Success;
    }
    else
    {
        result = ConvertErrno(errno);
        PAL_ALERT_ALWAYS();
    }

    return result;
}

// =====================================================================================================================
static Result CreateDir(
    const char *pPathName)
{
    Result result = Result::Success;

    char dirName[PathBufferLen];

    Strncpy(dirName, pPathName, sizeof(dirName));
    Strncat(dirName, sizeof(dirName), "/");

    const uint32 len = strlen(dirName);

    for (uint32 i = 1; i < len; i++)
    {
        if (dirName[i]=='/')
        {
            dirName[i] = 0;
            if (access(dirName, 0) != 0)
            {
                if (mkdir(dirName, 0755) == InvalidSysCall)
                {
                    result = ConvertErrno(errno);
                    break;
                }
            }
            dirName[i] = '/';
        }
    }

    return result;
}

// =====================================================================================================================
// Initialize a newly created file
static Result CreateFileInternal(
    const char*                pFileName,
    const ArchiveFileOpenInfo* pOpenInfo)
{
    PAL_ASSERT(pFileName != nullptr);
    PAL_ASSERT(pOpenInfo != nullptr);

    Result result = CreateDir(pOpenInfo->pFilePath);

    if (result == Result::Success)
    {
        if (access(pFileName, F_OK) == 0)
        {
            result = Result::AlreadyExists;
        }
    }
    if (result == Result::Success)
    {
        int32 fd = open(pFileName, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);

        if (fd == InvalidFd)
        {
            result = ConvertErrno(errno);
        }
        // The lock will prevent the file from being opened by multiple instances simultaneously.
        // It will be automatically released when we close the file handle.
        else if (flock(fd, LOCK_EX | LOCK_NB) == 0)
        {
            struct
            {
                ArchiveFileHeader header;
                ArchiveFileFooter footer;
            } data;

            memcpy(data.header.archiveMarker, MagicArchiveMarker, sizeof(data.header.archiveMarker));
            data.header.majorVersion = CurrentMajorVersion;
            data.header.minorVersion = CurrentMinorVersion;
            data.header.firstBlock   = static_cast<uint32>(VoidPtrDiff(&data.footer, &data));
            data.header.archiveType  = pOpenInfo->archiveType;

            memset(data.header.platformKey, 0, sizeof(data.header.platformKey));
            if (pOpenInfo->pPlatformKey)
            {
                memcpy(
                    data.header.platformKey,
                    pOpenInfo->pPlatformKey->GetKey(),
                    Min(sizeof(data.header.platformKey), pOpenInfo->pPlatformKey->GetKeySize()));
            }

            memcpy(data.footer.footerMarker, MagicFooterMarker, sizeof(data.footer.footerMarker));
            data.footer.entryCount         = 0;
            data.footer.lastWriteTimestamp = GetCurrentFileTime();
            memcpy(data.footer.archiveMarker, MagicArchiveMarker, sizeof(data.footer.archiveMarker));

            result = WriteDirect(fd, 0, &data, sizeof(data));

            close(fd);

            if (result != Result::Success)
            {
                remove(pFileName);
            }
        }
        else
        {
            result = ConvertErrno(errno);
            close(fd);
        }
    }

    return result;
}

// =====================================================================================================================
// Convert ArchiveFileOpenInfo flags and make OS calls to open the file
static Result OpenFileInternal(
    int32*                     pFd,
    const char*                pFileName,
    const ArchiveFileOpenInfo* pOpenInfo)
{
    PAL_ASSERT(pFd != nullptr);
    PAL_ASSERT(pFileName  != nullptr);
    PAL_ASSERT(pOpenInfo  != nullptr);

    int32    fd            = InvalidFd;
    int32    flags         = O_RDONLY;
    Result result        = Result::Success;

    if (pOpenInfo->allowWriteAccess)
    {
        flags = O_RDWR;
    }

    fd = open(pFileName, flags);

    if (fd != InvalidFd)
    {
        // In read-only mode, we allow another process to have this open as read/write.
        // In write mode, other processes can only have this open for read.
        // Use an exclusive lock if opening for read-write
        int resultLock = pOpenInfo->allowWriteAccess ? flock(fd, (LOCK_EX | LOCK_NB)) : 0;

        if (resultLock == 0)
        {
            if (pOpenInfo->useBufferedReadMemory)
            {
                posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL | POSIX_FADV_WILLNEED);
            }
            *pFd = fd;
        }
        else
        {
            close(fd);
            result = Result::ErrorUnavailable;
        }
    }
    else
    {
        PAL_ALERT_ALWAYS_MSG("Failed to open file '%s'", pFileName);
        result = ConvertErrno(errno);
    }

    return result;
}

// =====================================================================================================================
// Verify if the opened file satisfies the open request
static Result ValidateFile(
    const ArchiveFileOpenInfo* pOpenInfo,
    const ArchiveFileHeader*   pHeader)
{
    PAL_ASSERT(pOpenInfo != nullptr);
    PAL_ASSERT(pHeader != nullptr);

    bool valid = true;

    if (memcmp(pHeader->archiveMarker, MagicArchiveMarker, sizeof(MagicArchiveMarker)) != 0)
    {
        valid = false;
    }
    else if (pHeader->majorVersion != CurrentMajorVersion)
    {
        valid = false;
    }
    else if ((pOpenInfo->useStrictVersionControl == true) &&
             (pHeader->minorVersion != CurrentMinorVersion))
    {
        valid = false;
    }
    else if (pOpenInfo->pPlatformKey != nullptr)
    {
        const size_t headerKeySize   = sizeof(pHeader->platformKey);
        const size_t platformKeySize = pOpenInfo->pPlatformKey->GetKeySize();

        uint8 tmpKey[headerKeySize];
        memset(tmpKey, 0, headerKeySize);

        memcpy(
            tmpKey,
            pOpenInfo->pPlatformKey->GetKey(),
            Min(headerKeySize, platformKeySize));

        if (memcmp(pHeader->platformKey, tmpKey, headerKeySize) != 0)
        {
            valid = false;
        }
    }
    else if ((pOpenInfo->archiveType != 0) &&
             (pOpenInfo->archiveType != pHeader->archiveType))
    {
        valid = false;
    }

    return valid ? Result::Success : Result::ErrorIncompatibleLibrary;
}

// =====================================================================================================================
// Check that an archive footer is valid
static bool ValidateFooter(
    const ArchiveFileFooter* pFooter)
{
    PAL_ASSERT(pFooter != nullptr);

    const uint64 earliestFileTime = EarliestValidFileTime();

    bool valid = true;

    // Validate footer and archive markers
    if ((memcmp(pFooter->archiveMarker, MagicArchiveMarker, sizeof(MagicArchiveMarker)) != 0) ||
        (memcmp(pFooter->footerMarker, MagicFooterMarker, sizeof(MagicFooterMarker)) != 0))
    {
        valid = false;
    }
    // Value stored in file is unsigned, ensure that it wasn't written with a negative value
    else if (pFooter->entryCount > INT_MAX)
    {
        valid = false;
    }
    // Ensure the filetime value makes sense
    else if ((pFooter->lastWriteTimestamp < earliestFileTime) ||
             (pFooter->lastWriteTimestamp > GetCurrentFileTime()))
    {
        valid = false;
    }

    return valid;
}

// =====================================================================================================================
ArchiveFile::ArchiveFile(
    const AllocCallbacks&    callbacks,
    int32                    hFile,
    const ArchiveFileHeader* pArchiveHeader,
    bool                     haveWriteAccess,
    size_t                   memoryBufferMax)
    :
    // File Information
    m_allocator         (callbacks),
    m_hFile             (hFile),
    m_archiveHeader     (*pArchiveHeader),
    m_fileSize          (0),
    m_cachedFooter      (),
    m_curFooterOffset   (0),
    m_entries           (Allocator()),
    // Write Access
    m_haveWriteAccess   (haveWriteAccess),
    m_refreshedSinceLastWrite(false),
    // Read memory buffering
    m_useBufferedMemory (false),
    m_bufferMemory      (memoryBufferMax),
    m_recentList        (),
    m_pages             (),
    m_pageCount         (0),
    m_pageSize          (MinPageSize)
{
}

// =====================================================================================================================
ArchiveFile::~ArchiveFile()
{
    close(m_hFile);
}

// =====================================================================================================================
// Due to possible failure on object creation, Init() is required to be called before the object is usable
Result ArchiveFile::Init(
    const ArchiveFileOpenInfo* pInfo)
{
    Result result = Result::Success;

    // Init internal memory buffers
    if ((result == Result::Success) &&
        (pInfo->useBufferedReadMemory))
    {
        m_useBufferedMemory = true;
        result              = InitPages();
    }

    // Read the footer of the file directly
    if (result == Result::Success)
    {
        result = RefreshFile(true);
        if ((result != Result::ErrorIncompatibleLibrary) &&
            (IsErrorResult(result)))
        {
            result = Result::ErrorInitializationFailed;
        }
    }

    return result;
}

// =====================================================================================================================
// Returns the number of "good" entries found within the archive
size_t ArchiveFile::GetEntryCount() const
{
    return m_cachedFooter.entryCount;
}

// =====================================================================================================================
// Attempt to read ahead from the archive into an in-application buffer
Result ArchiveFile::Preload(
    size_t startLocation,
    size_t maxReadSize)
{
    Result result = Result::ErrorUnknown;

    if (m_useBufferedMemory)
    {
        if (startLocation < m_fileSize)
        {
            // Round up the page count division
            size_t readSize = maxReadSize;
            if ((readSize + startLocation) > static_cast<size_t>(m_fileSize))
            {
                readSize = static_cast<size_t>(m_fileSize) - startLocation;
            }

            result = ReadCached(startLocation, nullptr, readSize, false, false);

            if (IsErrorResult(result))
            {
                result = Result::ErrorUnknown;
            }
        }
        else
        {
            result = Result::ErrorInvalidValue;
        }
    }
    else
    {
        result = Result::Unsupported;
    }

    return result;
}

// =====================================================================================================================
// Gather entries from the archive and place them in a client provided array
Result ArchiveFile::FillEntryHeaderTable(
    ArchiveEntryHeader* pHeaders,
    size_t              startEntry,
    size_t              maxEntries,
    size_t*             pEntriesFilled)
{
    Result result = Result::ErrorUnknown;

    PAL_ASSERT(pHeaders != nullptr);
    PAL_ASSERT(pEntriesFilled != nullptr);

    if ((pHeaders == nullptr) ||
        (pEntriesFilled == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }
    else
    {
        const size_t endEntry = Min<size_t>(startEntry + maxEntries, m_entries.NumElements());

        for (size_t i = startEntry; i < endEntry; ++i)
        {
            ArchiveEntryHeader* pCurEntry = &pHeaders[i];
            result = GetEntryByIndex(i, pCurEntry);

            if (result != Result::Success)
            {
                break;
            }

            *pEntriesFilled += 1;
        }
    }

    return result;
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
        Result refreshResult = RefreshFile(false);

        // We can still attempt to read from the file using our cached header
        PAL_ALERT(IsErrorResult(refreshResult));

        // Sanity check our arguments before attempting the read
        if ((pHeader->ordinalId <= GetEntryCount()) &&
            ((pHeader->dataPosition + pHeader->dataSize) <= m_curFooterOffset))
        {
            result = ReadInternal(pHeader->dataPosition,
                                  pDataBuffer,
                                  pHeader->dataSize,
                                  false,
                                  true);
        }
        else
        {
            result = Result::ErrorInvalidValue;
        }
    }

    // Verify our data was read in as expected. This does not guarantee that the payload is valid, merely that no errors
    // ocurred during the file read
    if (result == Result::Success)
    {
        const uint64 crc = Crc64(pDataBuffer, pHeader->dataSize);

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
    PAL_ASSERT(pHeader != nullptr);
    PAL_ASSERT(pData != nullptr);

    Result result = Result::ErrorUnknown;

    if ((pHeader == nullptr) ||
        (pData == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (m_haveWriteAccess)
    {
        // cache off the write location
        uint32 curOffset = m_curFooterOffset;

        memcpy(pHeader->entryMarker, MagicEntryMarker, sizeof(MagicEntryMarker));
        pHeader->ordinalId    = m_cachedFooter.entryCount;
        pHeader->nextBlock    = curOffset + sizeof(ArchiveEntryHeader) + pHeader->dataSize;
        pHeader->dataPosition = curOffset + sizeof(ArchiveEntryHeader);
        pHeader->dataCrc64    = Crc64(pData, pHeader->dataSize);

        size_t writeSize = sizeof(ArchiveEntryHeader) +
                           pHeader->dataSize +
                           sizeof(ArchiveFileFooter);

        void* pBuffer = PAL_MALLOC(writeSize, Allocator(), AllocInternalTemp);

        if (pBuffer != nullptr)
        {
            void* pOutData   = VoidPtrInc(pBuffer, sizeof(ArchiveEntryHeader));
            void* pOutFooter = VoidPtrInc(pOutData, pHeader->dataSize);

            memcpy(pBuffer, pHeader, sizeof(ArchiveEntryHeader));
            memcpy(pOutData, pData, pHeader->dataSize);
            memcpy(pOutFooter, &m_cachedFooter, sizeof(ArchiveFileFooter));

            // Correct the footer we're about to attempt to write
            static_cast<ArchiveFileFooter*>(pOutFooter)->entryCount += 1;

            result = WriteInternal(curOffset, pBuffer, writeSize);

            PAL_SAFE_FREE(pBuffer, Allocator());
            if (result == Result::Success)
            {
                // Update our internal cache to reflect the result of the write
                m_curFooterOffset = pHeader->nextBlock;
                m_cachedFooter.entryCount += 1;

                result = m_entries.PushBack(*pHeader);

                PAL_ALERT(IsErrorResult(result));
            }
        }
        else
        {
            PAL_ALERT_ALWAYS();
            result = Result::ErrorOutOfMemory;
        }
    }
    else
    {
        result = Result::Unsupported;
    }

    return result;
}

// =====================================================================================================================
// Get the memory size needed for an archive file object
size_t GetArchiveFileObjectSize(
    const ArchiveFileOpenInfo* pOpenInfo)
{
    return sizeof(ArchiveFile);
}

// =====================================================================================================================
// Refresh the archive's file status by re-reading the footer.
Result ArchiveFile::RefreshFile(
    bool forceRefresh)
{
    Result result = Result::ErrorUnknown;

    struct stat statBuf;

    const bool needCheckSize = ((forceRefresh == true) ||
                                (m_fileSize == 0) ||
                                (m_haveWriteAccess == false) ||
                                (m_refreshedSinceLastWrite == false));
    if (needCheckSize == false)
    {
        result = Result::Success;
    }
    else
    {
        if (fstat(m_hFile, &statBuf) == 0)
        {
            if (m_fileSize == static_cast<uint64>(statBuf.st_size))
            {
                result = Result::Success;
            }
            else
            {
                ArchiveFileFooter tmpFooter    = {};
                const size_t      footerOffset = static_cast<size_t>(statBuf.st_size - sizeof(tmpFooter));

                if (m_haveWriteAccess &&
                    (footerOffset == m_curFooterOffset) &&
                    (forceRefresh == false))
                {
                    result = Result::Success;
                }
                else
                {
                    result = ReadInternal(footerOffset, &tmpFooter, sizeof(tmpFooter), true, true);

                    // Overwrite our cached copy only if we got a new valid footer
                    if (result == Result::Success)
                    {
                        if (ValidateFooter(&tmpFooter))
                        {
                            m_curFooterOffset = static_cast<uint64>(footerOffset);
                            m_cachedFooter    = tmpFooter;
                        }
                        else
                        {
                            result = Result::ErrorIncompatibleLibrary;
                        }
                    }
                }

                if (result == Result::Success)
                {
                    m_refreshedSinceLastWrite = true;
                    m_fileSize = static_cast<uint64>(statBuf.st_size);
                }
            }
        }
    }

    // Repopulate our headers if we need to
    if (result == Result::Success)
    {
        m_entries.Reserve(m_cachedFooter.entryCount);
        while ((m_entries.NumElements() < m_cachedFooter.entryCount) &&
               (result == Result::Success))
        {
            ArchiveEntryHeader* pLast   = m_entries.IsEmpty() ? nullptr : &m_entries.Back();
            ArchiveEntryHeader  header = {};

            result = ReadNextEntry(pLast, &header);

            if (result == Result::Success)
            {
                PAL_ALERT(header.ordinalId != m_entries.NumElements());
                result = m_entries.PushBack(header);
            }
        }

        PAL_ALERT(IsErrorResult(result));
        if (result == Result::NotFound)
        {
            // This is a recoverable error.
            PAL_ALERT_ALWAYS();
            result = Result::Success;
        }
    }

    return result;
}

// =====================================================================================================================
// Lookup Archive entry header by index
Result ArchiveFile::GetEntryByIndex(
    size_t              index,
    ArchiveEntryHeader* pHeader)
{
    PAL_ASSERT(pHeader != nullptr);

    Result result = Result::ErrorInvalidValue;

    // We can still attempt to read from the file using our cached entries
    Result refreshResult = RefreshFile(false);
    PAL_ALERT(IsErrorResult(refreshResult));

    if (index < m_entries.NumElements())
    {
        result = Result::Success;

        *pHeader = m_entries.At(static_cast<uint32>(index));

        if ((pHeader == nullptr) ||
            (pHeader->ordinalId != index))
        {
            PAL_ALERT_ALWAYS();
            result = Result::ErrorUnknown;
        }
    }

    // Propogate up the "Not Ready" result just in case
    if ((result == Result::ErrorInvalidValue) &&
        (refreshResult == Result::NotReady))
    {
        result = Result::NotReady;
    }

    return result;
}

// =====================================================================================================================
// Attempt to read the next entry in
Result ArchiveFile::ReadNextEntry(
    ArchiveEntryHeader* pCurheader,
    ArchiveEntryHeader* pNextHeader)
{
    Result result = Result::ErrorUnknown;

    size_t headerOffset = (pCurheader != nullptr) ? pCurheader->nextBlock : m_archiveHeader.firstBlock;

    if (headerOffset < m_curFooterOffset)
    {
        result = ReadInternal(headerOffset, pNextHeader, sizeof(ArchiveEntryHeader), false, true);
    }

    if (result == Result::Success)
    {
        const ArchiveFileFooter* footerCheck = reinterpret_cast<const ArchiveFileFooter*>(pNextHeader);
        if (ValidateFooter(footerCheck))
        {
            // We've actually found a footer marker, not an entry marker.
            ArchiveFileFooter tmpFooter = *footerCheck;
            uint32 tmpFooterOffset = static_cast<uint32>(headerOffset);

            // Try skipping the footer to see if there's a valid entry beyond.
            headerOffset += sizeof(ArchiveFileFooter);
            if (headerOffset < m_curFooterOffset)
            {
                result = ReadInternal(headerOffset, pNextHeader, sizeof(ArchiveEntryHeader), false, true);
            }

            if ((result == Result::Success) &&
                (memcmp(pNextHeader->entryMarker, MagicEntryMarker, sizeof(MagicEntryMarker)) == 0))
            {
                if (pCurheader != nullptr)
                {
                    // Update the pointer to skip the footer we found.
                    pCurheader->nextBlock = static_cast<uint32>(headerOffset);
                }
            }
            else
            {
                // We want to cut the file off here and not read any more entries,
                // but what we've read so far is valid.
                if (m_entries.NumElements() == tmpFooter.entryCount)
                {
                    m_curFooterOffset = tmpFooterOffset;
                    m_cachedFooter    = tmpFooter;

                    result = Result::NotFound;
                }
                else
                {
                    result = Result::ErrorUnknown;
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Select and call the appropriate read method for this file
Result ArchiveFile::ReadInternal(
    size_t fileOffset,
    void*  pBuffer,
    size_t readSize,
    bool   forceCacheReload,
    bool   wait)
{
    PAL_ASSERT(pBuffer != nullptr);

    Result result = Result::ErrorUnknown;

    if (m_useBufferedMemory)
    {
        result = ReadCached(fileOffset, pBuffer, readSize, forceCacheReload, wait);
    }

    if (result != Result::Success)
    {
        result = ReadDirect(m_hFile, fileOffset, pBuffer, readSize);
    }

    return result;
}

// =====================================================================================================================
// Select and call the appropriate write method for this file
Result ArchiveFile::WriteInternal(
    size_t           fileOffset,
    const void*      pData,
    size_t           writeSize)
{
    PAL_ASSERT(pData != nullptr);

    Result result = Result::ErrorUnknown;

    m_refreshedSinceLastWrite = false;

    result = WriteDirect(m_hFile, fileOffset, pData, writeSize);

    // Update the cached pages if needed
    if ((m_useBufferedMemory) &&
        (result == Result::Success))
    {
        Result bufferedResult = WriteCached(fileOffset, pData, writeSize);
        PAL_ALERT(IsErrorResult(bufferedResult));
    }

    return result;
}

// =====================================================================================================================
// Copy data from cached memory pages
Result ArchiveFile::ReadCached(
    size_t fileOffset,
    void*  pBuffer,
    size_t readSize,
    bool   forceReload,
    bool   wait)
{
    PAL_ASSERT(m_useBufferedMemory == true);

    Result result = Result::Success;

    // Break the read into cache pages
    size_t       curOffset = fileOffset;
    const size_t endOffset = fileOffset + readSize;

    while (curOffset < endOffset)
    {
        size_t curEnd = endOffset;
        if (CalcPageIndex(curOffset) != CalcPageIndex(curEnd))
        {
            curEnd = CalcNextPageBoundary(curOffset);
        }

        PageInfo* const pPage = FindPage(curOffset, true, forceReload);

        if (pPage != nullptr)
        {
            if (wait == true)
            {
                pPage->Wait();
            }

            // Allow pBuffer to be nullptr. This allows us to reuse this function to preload cache pages
            if (pBuffer != nullptr)
            {
                if (pPage->IsLoaded())
                {
                    const void* const pSrc = pPage->Contains(curOffset);
                    void* const       pDst = VoidPtrInc(pBuffer, curOffset - fileOffset);
                    memcpy(pDst, pSrc, curEnd - curOffset);
                }
                else
                {
                    result = Result::NotReady;
                    break;
                }
            }
        }
        else
        {
            result = Result::NotFound;
            break;
        }

        curOffset = curEnd;
    }

    return result;
}

// =====================================================================================================================
// Update any cached pages in memory
Result ArchiveFile::WriteCached(
    size_t      fileOffset,
    const void* pData,
    size_t      writeSize)
{
    PAL_ASSERT(m_useBufferedMemory == true);
    PAL_ASSERT(pData != nullptr);

    Result result = Result::Success;

    // Break the read into cache pages
    size_t       curOffset = fileOffset;
    const size_t endOffset = fileOffset + writeSize;

    while (curOffset < endOffset)
    {
        // If we're using async IO, just force the page to reload if found
        PageInfo* const pPage = FindPage(curOffset, false, false);

        size_t curEnd = endOffset;
        if (CalcPageIndex(curOffset) != CalcPageIndex(curEnd))
        {
            curEnd = CalcNextPageBoundary(curOffset);
        }

        // If we don't find our page in memory, that's okay our changes will be pulled in next time
        if (pPage != nullptr)
        {
            void* const       pDst = pPage->Contains(curOffset);
            const void* const pSrc = VoidPtrInc(pData, curOffset - fileOffset);
            memcpy(pDst, pSrc, curEnd - curOffset);
        }

        curOffset = curEnd;
    }

    PAL_ALERT(curOffset != endOffset);

    return result;
}

// =====================================================================================================================
// Initial cache pages to an empty state
Result ArchiveFile::InitPages()
{
    Result result = m_bufferMemory.Init();

    if (result == Result::Success)
    {
        const size_t totalMemorySize = m_bufferMemory.Remaining();

        PAL_ALERT(totalMemorySize < MinPageSize);

        m_pageSize = Max(Pow2Pad(totalMemorySize / MaxPageCount), MinPageSize);
    }

    return result;
}

// =====================================================================================================================
// Locate the cache page corresponding to the file offset
ArchiveFile::PageInfo* ArchiveFile::FindPage(
    size_t fileOffset,
    bool   loadOnMiss,
    bool   forceReload)
{
    PageInfo* pFoundPage = nullptr;

    // Find the existing page first.
    for (PageInfo::Iter i = m_recentList.Begin(); i.IsValid(); i.Next())
    {
        PageInfo* const pCurPage = i.Get();
        if (pCurPage->Contains(fileOffset) != nullptr)
        {
            pFoundPage = pCurPage;
            if (forceReload &&
                (pFoundPage->IsLoaded()))
            {
                Result reloadResult = pFoundPage->Reload(m_hFile, false);
                PAL_ALERT(IsErrorResult(reloadResult));
            }
            break;
        }
    }

    if ((pFoundPage == nullptr) &&
        loadOnMiss)
    {
        size_t pageBaseAddress = CalcPageIndex(fileOffset) * m_pageSize;

        // Alloc all pages before recycling
        if (m_pageCount < MaxPageCount)
        {
            void* pMem = nullptr;
            if (m_bufferMemory.Remaining() >= m_pageSize)
            {
                pMem = PAL_MALLOC(m_pageSize, &m_bufferMemory, AllocInternal);
                PAL_ALERT(pMem == nullptr);
            }

            if (pMem != nullptr)
            {
                m_pages[m_pageCount].Init(pMem, m_pageSize);

                if (m_pages[m_pageCount].Load(m_hFile, pageBaseAddress, false) == Result::Success)
                {
                    pFoundPage   = &m_pages[m_pageCount];
                    m_pageCount += 1;
                }
                else
                {
                    PAL_ALERT_ALWAYS();
                    m_bufferMemory.Rewind(pMem, false);
                }
            }
        }

        // Pull the least recently used page
        if ((pFoundPage == nullptr) && (m_recentList.IsEmpty() != true))
        {
            PageInfo* pRecyclePage = m_recentList.Back();
            pRecyclePage->Cancel();
            if (pRecyclePage->Load(m_hFile, pageBaseAddress, false) == Result::Success)
            {
                pFoundPage = pRecyclePage;
            }
            else
            {
                PAL_ALERT_ALWAYS();
            }
        }
    }

    // "Touch" the current page to make it recently used
    if (pFoundPage != nullptr)
    {
        PageInfo::Node* pNode = pFoundPage->ListNode();
        if (pNode->InList())
        {
            m_recentList.Erase(pNode);
        }

        m_recentList.PushFront(pNode);
    }

    return pFoundPage;
}

// =====================================================================================================================
// Determin an a given offset is inside out page and return a pointer to the backing memory
void* ArchiveFile::PageInfo::Contains(
    size_t offset)
{
    PAL_ASSERT(m_pMem != nullptr);

    const size_t endOffset = m_beginOffset + m_memSize;
    void*        pMem      = nullptr;

    if ((offset >= m_beginOffset) &&
        (offset < endOffset))
    {
        pMem = VoidPtrInc(m_pMem, offset - m_beginOffset);
    }

    return pMem;
}

// =====================================================================================================================
// Pull a page in from the disk using the appropriate method
Result ArchiveFile::PageInfo::Load(
    int32  hFile,
    size_t fileOffset,
    bool   useAsyncIo)
{
    m_beginOffset = fileOffset;

    return ReadDirect(hFile, fileOffset, m_pMem, m_memSize);
}

// =====================================================================================================================
// Opens a file on disk as a "PAL Archive File"
Result OpenArchiveFile(
    const ArchiveFileOpenInfo* pOpenInfo,
    void*                      pPlacementAddr,
    IArchiveFile**             ppArchiveFile)
{
    PAL_ASSERT(pOpenInfo != nullptr);
    PAL_ASSERT(pPlacementAddr != nullptr);
    PAL_ASSERT(ppArchiveFile != nullptr);

    Result result = Result::Success;

    if ((pOpenInfo      == nullptr) ||
        (pPlacementAddr == nullptr) ||
        (ppArchiveFile  == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }

    int32  hFile = InvalidFd;
    char   stringBuffer[PathBufferLen] = {};

    GenerateFullPath(stringBuffer, sizeof(stringBuffer), pOpenInfo);
    if (result == Result::Success)
    {
        // Only attempt to create the folder paths if we were going to write the file to begin with
        if (pOpenInfo->allowCreateFile)
        {
            result = CreateFileInternal(stringBuffer, pOpenInfo);
        }

        // Result::AlreadyExists may be returned so check for Errors instead of Result::Success
        if (IsErrorResult(result) == false)
        {
            result = OpenFileInternal(&hFile, stringBuffer, pOpenInfo);
        }
    }

    ArchiveFileHeader fileHeader = {};

    if (result == Result::Success)
    {
        // Inside here we have to clean up hFile on failure
        PAL_ALERT(hFile == InvalidFd);

        result = ReadDirect(hFile, 0, &fileHeader, sizeof(fileHeader));

        if (result == Result::Success)
        {
            result = ValidateFile(pOpenInfo, &fileHeader);
        }

        if (result != Result::Success)
        {
            close(hFile);
        }
    }

    if (result == Result::Success)
    {
        // Ownership of hFile is given to ArchiveFile in the constructor
        AllocCallbacks  callbacks = {};

        if (pOpenInfo->pMemoryCallbacks == nullptr)
        {
            Pal::GetDefaultAllocCb(&callbacks);
        }

        ArchiveFile* pArchiveFile = PAL_PLACEMENT_NEW(pPlacementAddr) ArchiveFile(
            (pOpenInfo->pMemoryCallbacks == nullptr) ? callbacks : *pOpenInfo->pMemoryCallbacks,
            hFile,
            &fileHeader,
            pOpenInfo->allowWriteAccess,
            pOpenInfo->useBufferedReadMemory ? pOpenInfo->maxReadBufferMem : 0);

        result = pArchiveFile->Init(pOpenInfo);

        if (result == Result::Success)
        {
            *ppArchiveFile = pArchiveFile;
        }
        else
        {
            close(hFile);
            *ppArchiveFile = nullptr;;
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
        GenerateFullPath(stringBuffer, sizeof(stringBuffer), pOpenInfo);
        result = CreateFileInternal(stringBuffer, pOpenInfo);
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
        GenerateFullPath(stringBuffer, sizeof(stringBuffer), pOpenInfo);
        if (remove(stringBuffer) == InvalidSysCall)
        {
            result = ConvertErrno(errno);
        }
    }

    return result;
}

} //namespace Util
