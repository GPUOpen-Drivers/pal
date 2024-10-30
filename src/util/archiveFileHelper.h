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
#include "palLiterals.h"

// linux
#include <sys/file.h>

namespace Util
{
// Helper functions to support global util namespace functions defined in palArchiveFile.h
namespace ArchiveFileHelper
{
constexpr int32 InvalidSysCall = -1;
typedef int32  FileHandle;
const FileHandle InvalidFileHandle = InvalidFd;

char* GenerateFullPath(
    char*                      pStringBuffer,
    size_t                     stringBufferSize,
    const ArchiveFileOpenInfo* pOpenInfo);

uint64 FileTimeToU64(
    const uint64 unixTimeStamp);

uint64 EarliestValidFileTime();

uint64 GetCurrentFileTime();

uint64 Crc64(
    const void* pData,
    size_t      dataSize);

Result ReadDirect(
    FileHandle hFile,
    size_t     fileOffset,
    void*      pBuffer,
    size_t     readSize);

Result WriteDirect(
    FileHandle  hFile,
    size_t      fileOffset,
    const void* pData,
    size_t      writeSize);

Result CreateDir(
    const char* pPathName);

Result CreateFileInternal(
    const char*                pFileName,
    const ArchiveFileOpenInfo* pOpenInfo);

Result WriteDirectBlankArchiveFile(
    FileHandle                 hFile,
    const ArchiveFileOpenInfo* pOpenInfo);

Result OpenFileInternal(
    FileHandle*                pOutHandle,
    const char*                pFileName,
    const ArchiveFileOpenInfo* pOpenInfo);

Result ValidateFile(
    const ArchiveFileOpenInfo* pOpenInfo,
    const ArchiveFileHeader*   pHeader);

bool ValidateFooter(
    const ArchiveFileFooter* pFooter);

void CloseFileHandle(FileHandle hFile);

Result DeleteFileInternal(
    const char* pFileName);

size_t GetFileSize(
    FileHandle hFile);

} //namespace ArchiveFileHelper
} //namespace Util
