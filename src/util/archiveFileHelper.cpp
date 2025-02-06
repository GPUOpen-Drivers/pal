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
#include "archiveFileHelper.h"
#include "palAssert.h"
#include "palMetroHash.h"
#include "palSysUtil.h"
#include "palPlatformKey.h"
#include "palInlineFuncs.h"
#include "palSysMemory.h"

// linux
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

// stl
#include <cstddef>
#include <cstdio>

namespace Util
{

// =====================================================================================================================
// Generate a full path from ArchiveFileOpenInfo
char* ArchiveFileHelper::GenerateFullPath(
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
// Convert a unixTimeStamp to uint64
uint64 ArchiveFileHelper::FileTimeToU64(
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
uint64 ArchiveFileHelper::EarliestValidFileTime()
{
    struct tm earliestTime = { };
    earliestTime.tm_mday = 1; // Day of the month (1-31)
    earliestTime.tm_wday = 1; // Day of the week (0-6, Sunday = 0), 2018-01-01 is a Monday
    earliestTime.tm_year = 2018 - 1900;

    const uint64 unixTime = mktime(&earliestTime);
    return ArchiveFileHelper::FileTimeToU64(unixTime);
}

// =====================================================================================================================
// Helper function to get current time as a 64 bit integer in FILETIME scale
uint64 ArchiveFileHelper::GetCurrentFileTime()
{
    const uint64 unixTime = time(nullptr);
    return ArchiveFileHelper::FileTimeToU64(unixTime);
}

// =====================================================================================================================
// Helper function around MetroHash64 for easy crc64 hashing
uint64 ArchiveFileHelper::Crc64(
    const void* pData,
    size_t      dataSize)
{
    PAL_ASSERT(pData != nullptr);

    union {
        uint64 crc64;
        uint8  raw[8];
    } hashOutput;
    hashOutput.crc64 = 0;

    // For our purposes, we always use a 0 seed.
    constexpr uint64 Seed = 0;
    MetroHash64::Hash(static_cast<const uint8*>(pData), dataSize, hashOutput.raw, Seed);

    return hashOutput.crc64;
}

// =====================================================================================================================
// Helper function to read directly from a file using OS-specific API.
Result ArchiveFileHelper::ReadDirect(
    FileHandle hFile,
    size_t     fileOffset,
    void*      pBuffer,
    size_t     readSize)
{
    PAL_ASSERT(hFile   != ArchiveFileHelper::InvalidFileHandle);
    PAL_ASSERT(pBuffer != nullptr);

    Result result    = Result::ErrorUnknown;

    struct stat statBuf;

    if (fstat(hFile, &statBuf) == 0)
    {
        result = Result::Success;
    }

    if (result == Result::Success)
    {
        if (lseek(hFile, fileOffset, SEEK_SET) == InvalidSysCall)
        {
            result = ConvertErrno(errno);
        }
    }

    size_t alreadyReadSize = 0;
    size_t exactSize = Min(readSize, (static_cast<size_t>(statBuf.st_size) - fileOffset));

    if (result == Result::Success)
    {
        alreadyReadSize = read(hFile, pBuffer, exactSize);
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
// Helper function to write directly to a file using OS-specific API
Result ArchiveFileHelper::WriteDirect(
    FileHandle  hFile,
    size_t      fileOffset,
    const void* pData,
    size_t      writeSize)
{
    PAL_ASSERT(hFile != ArchiveFileHelper::InvalidFileHandle);
    PAL_ASSERT(pData != nullptr);

    Result result       = Result::ErrorUnknown;

    if (lseek(hFile, fileOffset, SEEK_SET) == InvalidSysCall)
    {
        result = ConvertErrno(errno);
        PAL_ALERT_ALWAYS();
        return result;
    }

    size_t alreadyWriteSize = write(hFile, pData, writeSize);

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
// Create the directory
Result ArchiveFileHelper::CreateDir(
    const char* pPathName)
{
    Result result = Result::Success;

    // TODO - Consider using MkDirRecursively() (implemented in lnxSysUtil.cpp) on linux as well.
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
Result ArchiveFileHelper::CreateFileInternal(
    const char*                pFileName,
    const ArchiveFileOpenInfo* pOpenInfo)
{
    PAL_ASSERT(pFileName != nullptr);
    PAL_ASSERT(pOpenInfo != nullptr);

    Result result = ArchiveFileHelper::CreateDir(pOpenInfo->pFilePath);

    if (IsErrorResult(result) == false) // Result::AlreadyExists is okay
    {
        ArchiveFileHelper::FileHandle hFile = ArchiveFileHelper::InvalidFileHandle;

        if (access(pFileName, F_OK) == 0)
        {
            result = Result::AlreadyExists;
        }
        else
        {
            if (result == Result::Success)
            {
                hFile = open(pFileName, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
            }

            if (hFile == ArchiveFileHelper::InvalidFileHandle)
            {
                result = ConvertErrno(errno);
            }
            // The lock will prevent the file from being opened by multiple instances simultaneously.
            // It will be automatically released when we close the file handle.
            else if (flock(hFile, LOCK_EX | LOCK_NB) == 0)
            {
                result = WriteDirectBlankArchiveFile(hFile, pOpenInfo);

                ArchiveFileHelper::CloseFileHandle(hFile);

                if (result != Result::Success)
                {
                    ArchiveFileHelper::DeleteFileInternal(pFileName);
                }
            }
            else
            {
                result = ConvertErrno(errno);
                ArchiveFileHelper::CloseFileHandle(hFile);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Handle the OS-agnostic part of generating and writing a blank archive file.
Result ArchiveFileHelper::WriteDirectBlankArchiveFile(
    FileHandle                 hFile,
    const ArchiveFileOpenInfo* pOpenInfo)
{
    struct
    {
        ArchiveFileHeader header;
        ArchiveFileFooter footer;
    } data;

    memcpy(data.header.archiveMarker, MagicArchiveMarker, sizeof(data.header.archiveMarker));
    data.header.majorVersion = CurrentMajorVersion;
    data.header.minorVersion = CurrentMinorVersion;
#if PAL_64BIT_ARCHIVE_FILE_FMT
    data.header.firstBlock   = VoidPtrDiff(&data.footer, &data);
#else
    data.header.firstBlock   = uint32(VoidPtrDiff(&data.footer, &data));
#endif
    data.header.archiveType  = pOpenInfo->archiveType;

    memset(data.header.platformKey, 0, sizeof(data.header.platformKey));
    if (pOpenInfo->pPlatformKey)
    {
        memcpy(
            data.header.platformKey,
            pOpenInfo->pPlatformKey->GetKey(),
            Util::Min(sizeof(data.header.platformKey), pOpenInfo->pPlatformKey->GetKeySize()));
    }

    memcpy(data.footer.footerMarker, MagicFooterMarker, sizeof(data.footer.footerMarker));
    data.footer.entryCount         = 0;
    data.footer.lastWriteTimestamp = ArchiveFileHelper::GetCurrentFileTime();
    memcpy(data.footer.archiveMarker, MagicArchiveMarker, sizeof(data.footer.archiveMarker));

    return ArchiveFileHelper::WriteDirect(hFile, 0, &data, sizeof(data));
}

// =====================================================================================================================
// Convert ArchiveFileOpenInfo flags and make OS calls to open the file
Result ArchiveFileHelper::OpenFileInternal(
    ArchiveFileHelper::FileHandle* pOutHandle,
    const char*                    pFileName,
    const ArchiveFileOpenInfo*     pOpenInfo)
{
    PAL_ASSERT(pOutHandle != nullptr);
    PAL_ASSERT(pFileName  != nullptr);
    PAL_ASSERT(pOpenInfo  != nullptr);

    ArchiveFileHelper::FileHandle hFile = ArchiveFileHelper::InvalidFileHandle;

    int32  flags  = O_RDONLY;
    Result result = Result::Success;

    if (pOpenInfo->allowWriteAccess)
    {
        flags = O_RDWR;
    }

    hFile = open(pFileName, flags);

    if (hFile != ArchiveFileHelper::InvalidFileHandle)
    {
        // In read-only mode, we allow another process to have this open as read/write.
        // In write mode, other processes can only have this open for read.
        // Use an exclusive lock if opening for read-write
        int resultLock = pOpenInfo->allowWriteAccess ? flock(hFile, (LOCK_EX | LOCK_NB)) : 0;

        if (resultLock == 0)
        {
            *pOutHandle = hFile;
        }
        else
        {
            close(hFile);
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
Result ArchiveFileHelper::ValidateFile(
    const ArchiveFileOpenInfo* pOpenInfo,
    const ArchiveFileHeader*   pHeader)
{
    PAL_ASSERT(pOpenInfo != nullptr);
    PAL_ASSERT(pHeader != nullptr);

    Result result = Result::Success;

    if (memcmp(pHeader->archiveMarker, MagicArchiveMarker, sizeof(MagicArchiveMarker)) != 0)
    {
        result = Result::ErrorIncompatibleLibrary;
    }
    else if (pHeader->majorVersion != CurrentMajorVersion)
    {
        result = Result::ErrorIncompatibleLibrary;
    }
    else if ((pOpenInfo->useStrictVersionControl == true) &&
             (pHeader->minorVersion != CurrentMinorVersion))
    {
        result = Result::ErrorIncompatibleLibrary;
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
            result = Result::ErrorIncompatibleLibrary;
        }
    }
    else if ((pOpenInfo->archiveType != 0) &&
             (pOpenInfo->archiveType != pHeader->archiveType))
    {
        result = Result::ErrorIncompatibleLibrary;
    }

    return result;
}

// =====================================================================================================================
// Check that an archive footer is valid
bool ArchiveFileHelper::ValidateFooter(
    const ArchiveFileFooter* pFooter)
{
    PAL_ASSERT(pFooter != nullptr);

    static const uint64 earliestFileTime = ArchiveFileHelper::EarliestValidFileTime();

    bool valid = true;

    // Validate footer and archive markers
    if ((memcmp(pFooter->archiveMarker, MagicArchiveMarker, sizeof(MagicArchiveMarker)) != 0) ||
        (memcmp(pFooter->footerMarker, MagicFooterMarker, sizeof(MagicFooterMarker)) != 0))
    {
        valid = false;
    }
    // Ensure the filetime value makes sense
    else if ((pFooter->lastWriteTimestamp < earliestFileTime) ||
             (pFooter->lastWriteTimestamp > ArchiveFileHelper::GetCurrentFileTime()))
    {
        valid = false;
    }

    return valid;
}

// =====================================================================================================================
// Close a file handle in an OS-specific manner.
void ArchiveFileHelper::CloseFileHandle(
    FileHandle hFile)
{
    close(hFile);
}

// =====================================================================================================================
// Delete a file using OS-specific API
Result ArchiveFileHelper::DeleteFileInternal(
    const char* pFileName)
{
    Result result = Result::Success;

    if (remove(pFileName) == InvalidSysCall)
    {
        result = ConvertErrno(errno);
    }

    return result;
}

// =====================================================================================================================
// Get the size of the file opened by hFile.
size_t ArchiveFileHelper::GetFileSize(
    FileHandle hFile)
{
    size_t size = 0;

    struct stat statBuf;
    if (fstat(hFile, &statBuf) == 0)
    {
        size = static_cast<uint64>(statBuf.st_size);
    }

    return size;
}

} //namespace Util
