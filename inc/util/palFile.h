/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

/// Enumerates access modes that may be required on an opened file.  Can be bitwise ORed together to specify multiple
/// simultaneous modes.
enum FileAccessMode : uint32
{
    FileAccessRead      = 0x1,  ///< Read access.
    FileAccessWrite     = 0x2,  ///< Write access.
    FileAccessAppend    = 0x4,  ///< Append access.
    FileAccessBinary    = 0x8,  ///< Binary access.
    FileAccessNoDiscard = 0x10, ///< Don't discard existing file.
};

/**
 ***********************************************************************************************************************
 * @brief Exposes simple file I/O functionality by encapsulating standard C runtime file I/O functions like fopen,
 *        fwrite, etc.
 ***********************************************************************************************************************
 */
class File
{
public:
    File() : m_pFileHandle(nullptr) { }

    /// Closes the file if it is still open.
    ~File() { Close(); }

    /// Opens a file stream for read, write or append access.
    ///
    /// @param [in] pFilename   Name of file to open.
    /// @param [in] accessFlags Bitmask of FileAccessMode values indicating the usage of the file.
    ///
    /// @returns Success if successful, otherwise an appropriate error.
    Result Open(const char* pFilename, uint32 accessFlags);

    /// Closes the file handle.
    void Close();

    /// Writes a stream of bytes to the file.
    ///
    /// @param [in] pBuffer    Byte stream to be written to the file.
    /// @param [in] bufferSize Number of bytes to write.
    ///
    /// @returns Success if successful, otherwise an appropriate error.
    Result Write(const void* pBuffer, size_t bufferSize);

    /// Reads a stream of bytes from the file.
    ///
    /// @param [out] pBuffer    Buffer to be written with data read from file.
    /// @param [in]  bufferSize Size of the output buffer.
    /// @param [out] pBytesRead Number of bytes actually read (can be null).
    ///
    /// @returns Success if successful, otherwise an appropriate error.
    Result Read(void* pBuffer, size_t bufferSize, size_t* pBytesRead);

    /// Reads a single line of bytes from the file.
    ///
    /// @param [out] pBuffer    Buffer to be written with data read from file.
    /// @param [in]  bufferSize Size of the output buffer.
    /// @param [out] pBytesRead Number of bytes actually read (can be null).
    ///
    /// @returns Success if successful, otherwise an appropriate error.
    Result ReadLine(void* pBuffer, size_t bufferSize, size_t* pBytesRead);

    /// Prints a formatted string to the file.
    ///
    /// @param [in] pFormatStr Printf-style format string.
    ///
    /// @returns Success if successful, otherwise an appropriate error.
    Result Printf(const char* pFormatStr, ...) const;

    /// Prints a formatted string to the file.
    ///
    /// @param [in] pFormatStr Printf-style format string.
    /// @param [in] argList    Variable argument list.
    ///
    /// @returns Success if successful, otherwise an appropriate error.
    Result VPrintf(const char* pFormatStr, va_list argList);

    /// Flushes pending I/O to the file.
    ///
    /// @returns Success if successful, otherwise an appropriate error.
    Result Flush() const;

    /// Sets the file position to the beginning of the file.
    void Rewind();

    /// Sets the position indicator to a new position.
    ///
    /// @param  offset      Number of bytes to offset
    /// @param  fromOrigin  If true the seek will be relative to the file origin, if false it will
    ///                     be from the current position
    void Seek(int32 offset, bool fromOrigin);

    /// Returns true if the file is presently open.
    bool IsOpen() const { return (m_pFileHandle != nullptr); }

    /// Gets the size of the file contents in bytes
    ///
    /// @param [in] pFilename Name of the file to check.
    ///
    /// @returns Size of the file in bytes, or -1 on failure.
    static size_t GetFileSize(const char* pFilename);

    /// Checks if a file with the specified name exists.
    ///
    /// @param [in] pFilename Name of the file to check.
    ///
    /// @returns True if the specified file exists.
    static bool Exists(const char* pFilename);

    /// Gets the handle associated with this file.
    ///
    /// @returns A pointer to the file handle
    const std::FILE* GetHandle() const { return m_pFileHandle; }

private:
    std::FILE* m_pFileHandle;

    PAL_DISALLOW_COPY_AND_ASSIGN(File);
};

} // Util
