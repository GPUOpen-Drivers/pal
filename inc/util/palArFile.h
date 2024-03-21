/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

// Handling for Unix ar file format with SysV/GNU exetended names, but none of the symbol table stuff.

#pragma once
#include "palAssert.h"
#include "palInlineFuncs.h"
#include "palSpan.h"
#include "palUtil.h"

namespace Util
{

/// Definitions for Unix ar file format.
struct ArFileFormat
{
    struct GlobalHeader
    {
        char magic[8];
    };

    struct FileHeader
    {
        char name[16];
        char modTime[12];
        char owner[6];
        char group[6];
        char mode[8];
        char size[10];
        char endChars[2];
    };
};

// =====================================================================================================================
/// Class for writing a Unix ar (archive) file.
/// This is a virtual class; you write an archive by subclassing it and implementing the virtual
/// methods to get information on the archive members.
class ArFileWriter : private ArFileFormat
{
public:
    ArFileWriter() : m_extendedNamesLen(0), m_format(Format::Traditional) {}

    /// Get the overall size in bytes of the archive file to be written.
    ///
    /// @returns Size in bytes
    size_t GetSize();

    /// Write the archive into the supplied buffer. Must call GetSize() first.
    ///
    /// @param [out] pBuffer Buffer to write into
    /// @param bufferSize    Size of buffer in bytes (as returned by GetSize())
    void Write(char* pBuffer, size_t bufferSize);

    /// Get the number of members. To be implemented by subclass.
    ///
    /// @returns Number of members
    virtual uint32 GetNumMembers() = 0;

    /// Get the name of the member with the specified index. To be implemented by subclass.
    ///
    /// @param idx Member index
    ///
    /// @returns Member name, a span pointing to data owned by the implementing subclass. This class will
    ///         only refer to it up to the next GetName() call it makes, or up to when the calling GetSize()
    ///         or Write() returns, meaning the subclass can have a single shared buffer for names if it wants.
    virtual Span<const char> GetMemberName(uint32 idx) = 0;

    /// Get the contents of the member with the specified index. To be implemented by subclass.
    ///
    /// @param idx           Member index
    /// @param [out] pBuffer Buffer to write the contents, or nullptr to just get the size
    /// @param bufferSize    Size of buffer, must be either 0 (when just getting size) or no smaller than the
    ///                      actual size as returned by previous GetMember() for the same index
    ///
    /// @returns Size written; no more than bufferSize. Or size that would be written if pBuffer==nullptr.
    virtual size_t GetMember(uint32 idx, void* pBuffer, size_t bufferSize) = 0;

private:
    /// Write a file header
    ///
    /// @param name         Name of member
    /// @param size         Size in bytes of member
    /// @param [out] pWrite Where to write the header
    void WriteFileHeader(Span<const char> name, size_t size, void* pWrite);

    enum class Format : uint32
    {
        Traditional, // Traditional ar format with names <= 16 bytes and no spaces
        Svr4Short,   // SVR4 ar format with names <= 15 bytes (we add '/' terminator)
        Svr4Long,    // SVR4 ar format with extended names
    };

    size_t m_extendedNamesLen;
    Format m_format;
};

// =====================================================================================================================
/// Class for reading a Unix ar (archive) file.
/// The obvious way to write a loop to read all members of an archive is
///   for (auto it = arFileReader.Begin(); it.IsEnd() == false; it.Next())
/// If the archive is malformed, this asserts (or just terminates on a release build). If you want to
/// programmatically diagnose a malformed archive yourself and avoid the assert, you need to call
/// IsMalformed() before calling IsEnd(), Next() or any member accessor method.
class ArFileReader : private ArFileFormat
{
public:

    /// Iterator class
    class Iterator
    {
    public:
        /// Construct end iterator.
        Iterator() : m_pReader(nullptr), m_pHeader(nullptr) {}

        /// Construct iterator from ArFileReader, setting to the beginning.
        Iterator(ArFileReader* pReader);

        /// Get the name for the currently pointed to archive entry.
        Span<const char> GetName() const
        {
            PAL_ASSERT((m_pReader->m_malformed == false) && (m_pHeader != nullptr));
            return m_name;
        }

        /// Get the data for the currently pointed to archive entry.
        Span<const char> GetData() const
        {
            PAL_ASSERT((m_pReader->m_malformed == false) && (m_pHeader != nullptr));
            return Span<const char>(reinterpret_cast<const char*>(m_pHeader + 1), m_size);
        }

        /// Move to the next entry. This can change the archive to malformed state.
        void Next()
        {
            PAL_ASSERT((m_pReader->m_malformed == false) && (m_pHeader != nullptr));
            size_t sizeToSkip = sizeof(FileHeader) + m_size;
            sizeToSkip = Pow2Align(sizeToSkip, 2);
            m_pHeader = reinterpret_cast<const FileHeader*>(VoidPtrInc(m_pHeader, sizeToSkip));
            SkipExtendedNames();
        }

        /// Check if the archive is malformed. This should be checked before any IsEnd(), Next(), GetName(),
        /// GetData() on the iterator to avoid an assert (or undefined behavior on a non-asserting build).
        bool IsMalformed() const { return m_pReader->m_malformed; }

        /// Check if the iterator is at the end.
        bool IsEnd() const
        {
            PAL_ASSERT(m_pReader->m_malformed == false);
            return m_pHeader == nullptr;
        }

    private:
        /// If the current header is for an extended names section, remember and skip it.
        /// This also spots the case that the iterator has gone off the end, and sets m_pHeader to nullptr.
        void SkipExtendedNames();

        /// Check if the current header pointer is valid, including that the entry fits into the archive file.
        /// This also sets m_size, the data size of the current entry, and m_name, the (possibly extended) name
        /// of the current entry.
        bool IsValidHeader();

        ArFileReader*     m_pReader; // The parent reader
        const FileHeader* m_pHeader; // Header for current member
        size_t            m_size;    // Size of the current member (if not at end and not malformed)
        Span<const char>  m_name;    // Name of the current member (if not at end and not malformed)
    };

    /// Construct from binary blob
    ArFileReader(Span<const char> blob) : m_blob(blob), m_malformed(false) {}

    /// Get iterator for archive members.
    Iterator Begin() { return Iterator(this); }

private:
    Span<const char> m_blob;          // The archive file
    bool             m_malformed;     // Flag set if the archive is found to be malformed
    Span<const char> m_extendedNames; // Extended names section when found
};

} // namespace Util
