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
/**
 ***********************************************************************************************************************
 * @file  palFile.h
 * @brief PAL utility collection File class declaration.
 ***********************************************************************************************************************
 */

#pragma once

#include "palUtil.h"
#include "palInlineFuncs.h"
#include <cstdio>

namespace Util
{

/// Enumerates access modes for memory mapped files
enum FileMapAccessMode : uint32
{
    FileMapAllAccess = 0x1,  ///< Enables all access modes.
    FileMapCopy      = 0x2,  ///< Copy access
    FileMapRead      = 0x4,  ///< Read access.
    FileMapWrite     = 0x8   ///< Write access.
};

/**
***********************************************************************************************************************
* @brief  Provides methods for creating and accessing a memory mapped file.
***********************************************************************************************************************
*/
class FileMapping
{
public:
    /// Constructor. Does basic initialization.
    FileMapping();
    /// Destructor. Closes file handle and cleans up.
    ~FileMapping();

    /// Creates a new file mapping for the specified file.
    ///
    /// @param  pFileName      Name of the file to create a mapping for.
    /// @param  allowWrite     Flag that indicates whhether or not to request executeable access.
    /// @param  maximumSize    Maximum size allowed for the lifetime of the file mapping.
    /// @param  pName          System level name for mapping object, used to share the mapping across processes
    ///
    /// @returns Success if the file mapping was successfully created.
    Result Create(
            const char* pFileName,
            bool        allowWrite,
            size_t      maximumSize,
            const char* pName);

    /// Closes the current file memory mapping handle
    void Close();

    /// Closes and reopens the current file memory mapping handle with the new size specified.
    ///
    /// @param  newSize  New size that the file mapping should be opened with.
    ///
    /// @returns Success if the file mapping was successfully reloaded.
    Result ReloadMap(size_t newSize);

#if defined(__unix__)
    /// Returns the file descriptor of the memory mapped file.
    ///
    /// @returns file descriptor.
    int GetHandle() const { return m_fileHandle; }

    /// Determines if the file descriptor is valid
    ///
    /// @returns Turn if the memory mapped file is open.
    bool IsValid() const { return GetHandle() > 0; }
#else
    /// Returns the CPU pointer to the memory mapped file memory.
    ///
    /// @returns CPU pointer to memory mapped file memory.
    void* GetHandle() const { return m_memoryMapping; }

    /// Determines if the file mapping object is valid
    ///
    /// @returns True if the file mapping object is open and has a valid CPU pointer.
    bool IsValid() const { return GetHandle() != nullptr; }
#endif

    /// Flushes the current file ensuring all cached writes are completed out to disk
    ///
    /// @returns Success if the flush completes successfully.
    bool Flush();

private:
#if defined(__unix__)
    int         m_fileHandle;       ///< File descriptor of the file that is opened for mapping
#else
    void*       m_memoryMapping;    ///< CPU pointer to memory mapped file memory.
    void*       m_fileHandle;       ///< Handle to the file that is opened for mapping
#endif
    bool        m_writeable;        ///< Flag that indicates if this mapping is writeable
    const char* m_pFileName;        ///< File name for the file being mapped.
    const char* m_pSystemName;      ///< System name for the file mapping, used to share file mapping object across
                                    ///  processes.

    PAL_DISALLOW_COPY_AND_ASSIGN(FileMapping);
};

/**
***********************************************************************************************************************
* @brief This class owns a view (mapped virtual memory pointer) into a memory mapped file.
*
* @note WARNING ABOUT READING/WRITING TO MEMORY CONTROLLED BY FileView -
*
*   Reading from or writing to a file view of a file other than the page file can cause an
*   EXCEPTION_IN_PAGE_ERROR exception. To guard against exceptions due to input and output
*   (I/O) errors, all attempts to access memory mapped files should be wrapped in
*   structured exception handlers.
*
* SEE: https://github.com/MicrosoftDocs/win32/blob/docs/desktop-src/Memory/reading-and-writing-from-a-file-view.md
*
* Helper macros are provided below.
***********************************************************************************************************************
*/
class FileView
{
public:
    /// Constructor. Initializes the file view.
    FileView();

    /// Maps a view for read or read+write access
    ///
    /// @param [in,out] mappedFile   Memory mapped file object to create a view from.
    /// @param          writeAccess  Flag that indicates if the file view should have write access.
    /// @param          offset       Offset into the file mapping that the file view should start.
    /// @param          size         Size of the file view to create
    ///
    /// @returns CPU pointer to the mapped file view memory.
    void* Map(const FileMapping& mappedFile, bool writeAccess, size_t offset, size_t size);

    /// Unmaps the current view
    ///
    /// @param  flushOnUnmap   Flag that indicates whether a flush should be executed prior to unmapping the file view.
    void  UnMap(bool flushOnUnmap);

    /// Flushes contents of the view to the file on disk
    ///
    /// @param  bytesToFlush   Number of bytes to flush from this view. Can be set to zero to flush the entire view.
    ///
    /// @returns Success if the flush completes successfully.
    Result  Flush(size_t bytesToFlush);

    /// Gets the size of the file view
    ///
    /// @returns  The current size of the file view.
    size_t Size() const { return m_requestedSize; }

    /// Gets a pointer to the mapped memory, please see note above for memory access warnings
    ///
    /// @returns  CPU pointer to the file view mapped memory.
    void* Ptr() const { return VoidPtrInc(m_pMappedMem, m_offestIntoView); }

    /// Determines if the FileView is valid.
    ///
    /// @returns  Boolean indicating whether the FileView is valid or not.
    bool IsValid() const { return Ptr() != nullptr; }

private:
    void*              m_pMappedMem;     ///< pointer to the start of the mapped virtual memory page
    size_t             m_offestIntoView; ///< offset within the memory view of the requested pointer
    size_t             m_requestedSize;  ///< size of mapped memory requested by the user

    PAL_DISALLOW_COPY_AND_ASSIGN(FileView);
};

// For other platforms just define the same SEH symbols to allow use in OS independent code
#define TRY_ACCESS_FILE_VIEW(condition) if(condition)
#define CATCH_ACCESS_FILE_VIEW else

} // Util
