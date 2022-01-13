/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palAssert.h"
#include "palFile.h"
#include "palSysUtil.h"
#include <cstdio>
#include <sys/stat.h>
#include <fstream>
#include <limits>

namespace Util
{
// =====================================================================================================================
// 64-bit platform-agnostic stat()
Result File::GetStat(
    const char* pFilename,
    Stat*     pStatus)
{
    PAL_ASSERT(pStatus != nullptr);

    // On non-MSVC the function/struct to retrieve file status information is named `stat` (no underscore)
    // There's also no distinction between `stat` and `stat64` outside of the kernel.
    struct stat status{};
    const int32 ret = stat(pFilename, &status);

    // The size is the only critical member which must be 64-bit.
    static_assert(sizeof(Stat::size) == sizeof(status.st_size), "File::Status::size size mismatch");

    // There were compiler errors with getting 64-bit timestamps on some 32-bit linux builds.
    // Otherwise we'd be asserting for them just like st_size.
    // A 32-bit timestamp will last us until 2038, so it's probably okay.

    // The other members we don't particularly care about their sizes.
    // The sizes can vary widely based on platform, library, and architecture.
    // We don't anticipate losing any data if the sizes are off.

    pStatus->size   = static_cast<decltype(Stat::size)>(status.st_size);
    pStatus->ctime  = static_cast<decltype(Stat::ctime)>(status.st_ctime);
    pStatus->atime  = static_cast<decltype(Stat::atime)>(status.st_atime);
    pStatus->mtime  = static_cast<decltype(Stat::mtime)>(status.st_mtime);
    pStatus->mode   = static_cast<decltype(Stat::mode)>(status.st_mode);
    pStatus->nlink  = static_cast<decltype(Stat::nlink)>(status.st_nlink);
    pStatus->dev    = static_cast<decltype(Stat::dev)>(status.st_dev);

    return (ret == 0) ? Result::Success : ConvertErrno(errno);
}

// =====================================================================================================================
// Opens a file stream for read, write or append access.
Result File::Open(
    const char* pFilename,    // Name of file to open; "-" for stdin/stdout (depending on access mode)
    uint32      accessFlags)  // ORed mask of FileAccessMode values describing how the file will be used.
{
    Result result = Result::Success;

    if (m_pFileHandle != nullptr)
    {
        result = Result::ErrorUnavailable;
    }
    else if (pFilename == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (strcmp(pFilename, "-") == 0)
    {
        switch (accessFlags & (FileAccessRead | FileAccessWrite | FileAccessAppend))
        {
        case FileAccessRead:
            m_pFileHandle = stdin;
            break;
        case FileAccessWrite:
        case FileAccessAppend:
            m_pFileHandle = stdout;
            break;
        default:
            PAL_ASSERT_ALWAYS();
            result = Result::ErrorInvalidFlags;
            break;
        }
    }
    else
    {
        char fileMode[5]{};

        switch (accessFlags)
        {
        case FileAccessRead:
            fileMode[0] = 'r';
            break;
        case FileAccessWrite:
            fileMode[0] = 'w';
            break;
        case FileAccessAppend:
            fileMode[0] = 'a';
            break;
        case (FileAccessRead | FileAccessWrite):
            // Both r+ and w+ modes might apply here: r+ requires that the file exists beforehand, while w+ does not. w+
            // will create the file if it doesn't exist, like w,a,a+. w+, like w, will discard existing contents of the
            // file. If we need to expose r+ mode, use FileAccessNoDiscard.
            fileMode[0] = 'w';
            fileMode[1] = '+';
            break;
        case (FileAccessRead | FileAccessWrite | FileAccessNoDiscard):
            fileMode[0] = 'r';
            fileMode[1] = '+';
            break;
        case (FileAccessRead | FileAccessAppend):
            // When a file is opened by using the "a" or "a+" access type, all write operations occur at the end of the
            // file. The file pointer can be repositioned by using fseek or rewind, but it's always moved back to the
            // end of the file before any write operation is carried out so that existing data cannot be overwritten.
            fileMode[0] = 'a';
            fileMode[1] = '+';
            break;
        case (FileAccessRead | FileAccessBinary):
            fileMode[0] = 'r';
            fileMode[1] = 'b';
            break;
        case (FileAccessWrite | FileAccessBinary) :
            fileMode[0] = 'w';
            fileMode[1] = 'b';
            break;
        case (FileAccessRead | FileAccessWrite | FileAccessBinary):
            fileMode[0] = 'w';
            fileMode[1] = 'b';
            fileMode[2] = '+';
            fileMode[3] = 'R';
            break;
        case (FileAccessRead | FileAccessWrite | FileAccessBinary | FileAccessNoDiscard):
            fileMode[0] = 'r';
            fileMode[1] = 'b';
            fileMode[2] = '+';
            fileMode[3] = 'R';
            break;
        case (FileAccessRead | FileAccessAppend | FileAccessBinary):
            fileMode[0] = 'a';
            fileMode[1] = 'b';
            fileMode[2] = '+';
            fileMode[3] = 'R';
            break;
        default:
            PAL_ASSERT_ALWAYS();
            result = Result::ErrorInvalidFlags;
            break;
        }

        if (result == Result::Success)
        {
            // Just use the traditional fopen.
            m_pFileHandle = fopen(pFilename, fileMode);
            if (m_pFileHandle == nullptr)
            {
                result = ConvertErrno(errno);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Closes the file handle if still open.
void File::Close()
{
    if (m_pFileHandle != nullptr)
    {
        if ((m_pFileHandle != stdin) && (m_pFileHandle != stdout))
        {
            fclose(m_pFileHandle);
        }
        m_pFileHandle = nullptr;
    }
}

// =====================================================================================================================
// Erases a file if it exists.
Result File::Remove(
    const char* pFilename)
{
    const int32 ret = remove(pFilename);
    return ret == 0 ? Result::Success : Result::ErrorUnknown;
}

// =====================================================================================================================
// Writes a stream of bytes to the file.
Result File::Write(
    const void* pBuffer,     // [in] Buffer to write to the file.
    size_t      bufferSize)  // Size of the buffer in bytes.
{
    Result result = Result::Success;

    if (m_pFileHandle == nullptr)
    {
        result = Result::ErrorUnavailable;
    }
    else if (pBuffer == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (bufferSize == 0)
    {
        result = Result::ErrorInvalidValue;
    }
    else
    {
        if (fwrite(pBuffer, 1, bufferSize, m_pFileHandle) != bufferSize)
        {
            result = ConvertErrno(errno);
        }
    }

    return result;
}

// =====================================================================================================================
// Reads a stream of bytes from the file.
Result File::Read(
    void*   pBuffer,     // [out] Buffer to read the file into.
    size_t  bufferSize,  // Size of buffer in bytes.
    size_t* pBytesRead)  // [out] Number of bytes actually read (can be nullptr).
{
    Result result = Result::Success;

    if (m_pFileHandle == nullptr)
    {
        result = Result::ErrorUnavailable;
    }
    else if (pBuffer == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (bufferSize == 0)
    {
        result = Result::ErrorInvalidValue;
    }
    else
    {
        const size_t bytesRead = fread(pBuffer, 1, bufferSize, m_pFileHandle);

        if (ferror(m_pFileHandle) != 0)
        {
            result = ConvertErrno(errno);
        }

        if (pBytesRead != nullptr)
        {
            *pBytesRead = bytesRead;
        }
    }

    return result;
}

// =====================================================================================================================
// Reads a single line (until the next newline) of bytes from the file.
Result File::ReadLine(
    void*   pBuffer,     // [out] Buffer to read the file into.
    size_t  bufferSize,  // Size of buffer in bytes.
    size_t* pBytesRead)  // [out] Number of bytes actually read (can be nullptr).
{
    Result result = Result::ErrorInvalidValue;

    if (m_pFileHandle == nullptr)
    {
        result = Result::ErrorUnavailable;
    }
    else if (pBuffer == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (bufferSize == 0)
    {
        result = Result::ErrorInvalidValue;
    }
    else if (feof(m_pFileHandle))
    {
        result = Result::Eof;
    }
    else
    {
        size_t bytesRead = 0;
        char* pcharBuffer = static_cast<char*>(pBuffer);

        while (bytesRead < bufferSize)
        {
            const int32 c = getc(m_pFileHandle);
            if (c == '\n')
            {
                result = Result::Success;
                break;
            }
            if (c == EOF)
            {
                result = Result::Success;
                if (ferror(m_pFileHandle))
                {
                    result = ConvertErrno(errno);
                }
                break;
            }
            pcharBuffer[bytesRead] = static_cast<char>(c);
            bytesRead++;
        }

        const size_t end = ((bytesRead < bufferSize) ? bytesRead : (bufferSize - 1));
        pcharBuffer[end] = '\0';

        if (pBytesRead != nullptr)
        {
            *pBytesRead = bytesRead;
        }
    }

    return result;
}

// =====================================================================================================================
// Prints a formatted string to the file.
Result File::Printf(
    const char* pFormatStr,  // Printf-style format string.
    ...                      // Printf-style argument list.
    ) const

{
    Result result = Result::ErrorUnavailable;

    if (m_pFileHandle != nullptr)
    {
        va_list argList;
        va_start(argList, pFormatStr);

        // Just use the traditional vfprintf.
        if (vfprintf(m_pFileHandle, pFormatStr, argList) >= 0)
        {
            result = Result::Success;
        }
        else
        {
            result = Result::ErrorUnknown;
        }

        va_end(argList);
    }

    return result;
}

// =====================================================================================================================
// Prints a formatted string to the file.
Result File::VPrintf(
    const char* pFormatStr,  // Printf-style format string.
    va_list     argList)     // Pre-started variable argument list.
{
    Result result = Result::ErrorUnavailable;

    if (m_pFileHandle != nullptr)
    {
        // Just use the traditional vfprintf.
        if (vfprintf(m_pFileHandle, pFormatStr, argList) >= 0)
        {
            result = Result::Success;
        }
        else
        {
            result = Result::ErrorUnknown;
        }
    }

    return result;
}

// =====================================================================================================================
// Flushes pending I/O to the file.
Result File::Flush() const
{
    Result result = Result::Success;

    if (m_pFileHandle == nullptr)
    {
        result = Result::ErrorUnavailable;
    }
    else
    {
        fflush(m_pFileHandle);
    }

    return result;
}

// =====================================================================================================================
// Sets the file position to the beginning of the file.
void File::Rewind()
{
    if (m_pFileHandle != nullptr)
    {
        rewind(m_pFileHandle);
    }
}

// =====================================================================================================================
// Sets the position indicator to a new position.
void File::Seek(
    int64        offset,
    SeekPosition pos)
{
    if (m_pFileHandle != nullptr)
    {
        const int32 ret = fseeko64(m_pFileHandle, offset, static_cast<int>(pos));
        PAL_ASSERT(ret == 0);
    }
}

// =====================================================================================================================
// Returns true if a file with the given name exists.
size_t File::GetFileSize(
    const char* pFilename)
{
    Stat fileStatus{};
    const Result result = GetStat(pFilename, &fileStatus);
    return static_cast<size_t>((result == Result::Success) ? fileStatus.size : -1);
}

// =====================================================================================================================
// Returns true if a file with the given name exists.
bool File::Exists(
    const char* pFilename)
{
    Stat fileStatus{};
    const Result result = GetStat(pFilename, &fileStatus);
    return (result == Result::Success);
}

// =====================================================================================================================
// Reads a file into memory.
Result File::ReadFile(
    const char* pFilename,
    void*       pData,
    size_t      dataSize,
    size_t*     pBytesRead,
    bool        binary)
{
    PAL_ASSERT(pFilename != nullptr);
    PAL_ASSERT(pData != nullptr);
    const size_t fileSize = GetFileSize(pFilename);
    Result result = Result::ErrorUnknown;

    if (dataSize < fileSize)
    {
        result = Result::ErrorInvalidMemorySize;
    }
    else
    {
        File file;
        result = file.Open(pFilename, FileAccessMode::FileAccessRead | (binary ? FileAccessMode::FileAccessBinary : 0));
        if (result == Result::Success)
        {
            size_t bytesRead = 0;
            result = file.Read(pData, fileSize, &bytesRead);
            if (result == Util::Result::Success)
            {
                if (binary && (bytesRead != fileSize))
                {
                    result = Result::ErrorUnknown;
                }
                else if (pBytesRead != nullptr)
                {
                    *pBytesRead = bytesRead;
                }
            }
        }
    }

    return result;
}

} // Util
