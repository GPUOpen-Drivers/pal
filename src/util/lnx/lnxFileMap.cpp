/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palFileMap.h"
#include "palAssert.h"
#include "palFile.h"

#include <cstdio>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace Util
{

// =====================================================================================================================
FileMapping::FileMapping()
    :
    m_fileHandle(InvalidFd),
    m_writeable(false),
    m_pSystemName(nullptr)
{

}

// =====================================================================================================================
FileMapping::~FileMapping()
{
    Close();
}

// =====================================================================================================================
// Creates a new file memory mapping object. Note: For values to pass into attributeFlags see MSDN CreateFileMapping()
Result FileMapping::Create(
    const char* pFileName,    // Name of the file to create a mapping for.
    bool        allowWrite,   // whether or not to request executeable access
    size_t      mapSize,      // Initial Mapping Size.
    const char* pName)        // [In][Opt] System level name for mapping object
{
    Result result = Result::ErrorUnknown;

    // Write access is required to create a new file
    const bool fileExists = File::Exists(pFileName);
    PAL_ASSERT(fileExists || allowWrite);

    m_writeable = allowWrite;
    m_pFileName = pFileName;
    m_pSystemName = pName;

    int flags = fileExists ? 0 : O_CREAT;
    flags |= allowWrite ? O_RDWR : O_RDONLY;
    m_fileHandle = open(pFileName, flags, 0600);

    if (m_fileHandle != -1)
    {
        if ((m_writeable == false) || (ftruncate(m_fileHandle, mapSize) == 0))
        {
            result = Result::Success;
        }
    }

    return result;
}

// =====================================================================================================================
// Creates a new file memory mapping object from an existing file handle. This class assumes ownership of the handle.
Result FileMapping::CreateFromHandle(
    int         fileHandle,   // Existing Handle to map.
    bool        allowWrite,   // whether or not to request executeable access
    size_t      mapSize,      // Initial Mapping Size.
    const char* pName)        // [In][Opt] System level name for mapping object
{
    Result result = Result::ErrorUnknown;

    m_writeable = allowWrite;
    m_pFileName = nullptr;
    m_pSystemName = pName;
    m_fileHandle = fileHandle;

    if (m_fileHandle != -1)
    {
        if ((m_writeable == false) || (ftruncate(m_fileHandle, mapSize) == 0))
        {
            result = Result::Success;
        }
    }

    return result;
}

// =====================================================================================================================
// Nothing needs to be done for Linux
Result FileMapping::ReloadMap(
    size_t newSize)  // new size that the mapping should be reopened with.
{
    Result result = Result::ErrorUnknown;

    if ((m_writeable == false) || (ftruncate(m_fileHandle, newSize) == 0))
    {
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Closes the current file handle
void FileMapping::Close()
{
    if (IsValid())
    {
        close(m_fileHandle);
    }
}

// =====================================================================================================================
// Flushes the current file handle
bool FileMapping::Flush()
{
    fsync(m_fileHandle);
    return true;
}

// =====================================================================================================================
FileView::FileView()
    :
    m_pMappedMem(nullptr),
    m_offsetIntoView(0),
    m_requestedSize(0)
{
}

// =====================================================================================================================
// Maps a view of a file for read or read+write access. Returns a pointer to requested memory or nullptr on failure.
// NOTE: Please see notes in header file about memory access warnings.
void* FileView::Map(
    const FileMapping& mappedFile,    // [In] pointer to valid file mapping object
    bool               writeAccess,    // whether to allow write access
    size_t             offset,         // position in file to map
    size_t             size)           // size of view to map
{
    // mappedFile should hold a valid file descriptor
    PAL_ASSERT(mappedFile.GetHandle() > 0);
    // offset should be aligned to page
    const int pageSize = sysconf(_SC_PAGE_SIZE);
    m_offsetIntoView = offset - offset / pageSize * pageSize;
    m_requestedSize = (size + m_offsetIntoView - 1);

    const int prot = writeAccess ? (PROT_READ|PROT_WRITE) : PROT_READ;
    m_pMappedMem = mmap(nullptr, m_requestedSize, prot, MAP_SHARED,
                        mappedFile.GetHandle(), offset / pageSize * pageSize );
    if (m_pMappedMem == MAP_FAILED)
    {
        m_pMappedMem = nullptr;
        m_requestedSize = 0;
        m_offsetIntoView = 0;
    }
    void* pMem = Ptr();

    return pMem;
}

// =====================================================================================================================
// Unmaps the current file view.
void FileView::UnMap(
    bool flushOnUnmap)
{
    if (m_pMappedMem != nullptr)
    {
        munmap(m_pMappedMem, m_requestedSize);
    }

    m_pMappedMem = nullptr;
    m_offsetIntoView = 0;
    m_requestedSize = 0;
}

// =====================================================================================================================
// Flush the contents of the file view to the file on disk. Note: Passing in a value of zero will cause the entire
// view to be flushed.
Result FileView::Flush(
    size_t bytesToFlush)    // number of bytes to flush to disk
{
    PAL_ALERT(bytesToFlush > m_requestedSize);

    Result result = Result::ErrorUnknown;
    if (msync(m_pMappedMem, bytesToFlush, MS_SYNC) == 0)
    {
        result = Result::Success;
    }

    return result;
}

} // Util
