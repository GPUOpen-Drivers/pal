/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

// Handling for Unix ar file format in PAL pipeline ABI.

#pragma once
#include "palArFile.h"
#undef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1 // to get PRIx64 etc
#include <inttypes.h>
#include <stdio.h>

namespace Util
{
namespace Abi
{

// =====================================================================================================================
/// Writer class for PAL ABI ar (Unix archive) file.
/// This is a virtual class; to write such a file, subclass this class and implement the virtual methods
/// here, as well as ones in ArFileWriter that are not implemented here:
/// - GetNumMembers()
/// - GetMember()
class PipelineArFileWriter : public ArFileWriter
{
public:
    /// Get ELF hash for the member with the specified index. This forms part of the member name in the ABI.
    /// To be implemented by subclass.
    ///
    /// @param idx Member index
    ///
    /// @returns ELF hash
    virtual uint64 GetMemberElfHash(uint32 idx) = 0;

    /// Get ELF retention ID for the member with the specified index. This forms part of the member name in the ABI.
    /// To be implemented by subclass.
    ///
    /// @param idx Member index
    ///
    /// @returns ELF retention ID, 0 if none
    virtual uint64 GetMemberElfRetentionId(uint32 idx) { return 0; }

    /// Get the name of the member with the specified index. Implementation of virtual method in ArFileWriter.
    ///
    /// @param idx Member index
    ///
    /// @returns Member name.
    virtual Span<const char> GetMemberName(uint32 idx) override
    {
        // The member name is:
        // - 16 hex digits for the ELF hash; then
        // - if the ELF retention ID is non-zero, a period then 16 hex digits ELF retention ID.
        Util::Snprintf(m_nameBuf, sizeof(m_nameBuf), "%16.16llX", GetMemberElfHash(idx));
        uint64 elfRetentionId = GetMemberElfRetentionId(idx);
        if (elfRetentionId != 0)
        {
            size_t sizeSoFar = strlen(m_nameBuf);
            Util::Snprintf(m_nameBuf + sizeSoFar, sizeof(m_nameBuf) - sizeSoFar, ".%16.16llX", elfRetentionId);
        }
        return Span<const char>(m_nameBuf, strlen(m_nameBuf));
    }

private:
    static constexpr const size_t MaxNameLen = 16 + 1 + 16;
    char m_nameBuf[MaxNameLen + 1];
};

// =====================================================================================================================
/// Reader class for PAL ABI ar (Unix archive) file.
class PipelineArFileReader : public ArFileReader
{
public:
    /// Iterator class. The user of this class can use the GetData(), Next(), IsMalformed(), IsEnd() methods
    /// inherited from ArFileReader.
    class Iterator : public ArFileReader::Iterator
    {
    public:
        /// Construct end iterator.
        Iterator() : ArFileReader::Iterator() {}

        /// Construct iterator from PipelineArFileReader, setting to the beginning.
        Iterator(PipelineArFileReader* pReader) : ArFileReader::Iterator(pReader)
        {
        }

        /// Get the ELF hash from the name of the currently pointed to archive entry.
        /// Returns 0 if the format of the name does not match the PAL ABI ar format.
        /// The name needs to be one 16-digit hex number 123456789ABCDEF0 giving the ELF hash, or
        /// two such numbers separated by a period 123456789ABCDEF0.123456789ABCDEF0 where the first
        /// number gives the ELF hash.
        uint64 GetElfHash() const
        {
            uint64 elfHash = 0;
            Span<const char> name = GetName();
            if (((name.NumElements() == 16) || ((name.NumElements() == 16 + 1 + 16) && (name[16] == '.'))) &&
                (isxdigit(name[0])))
            {
                uint32 numCharsUsed = 0;
                sscanf(name.Data(), "%" SCNx64 "%n", &elfHash, &numCharsUsed);
                if (numCharsUsed != 16)
                {
                    elfHash = 0;
                }
            }
            return elfHash;
        }

        /// Get the retention ID from the name of the currently pointed to archive entry.
        /// Returns 0 if the format of the name does not match the PAL ABI ar format or it
        /// does not have a retention ID.
        /// The name needs to be two 16-digit hex numbers separated by a period
        /// 123456789ABCDEF0.123456789ABCDEF0 where the second number gives the retention ID.
        uint64 GetRetentionId() const
        {
            uint64 retentionId = 0;
            Span<const char> name = GetName();
            if ((name.NumElements() == 16 + 1 + 16) && (name[16] == '.') && (isxdigit(name[17])))
            {
                uint32 numCharsUsed = 0;
                sscanf(&name[17], "%" PRIx64 "%n", &retentionId, &numCharsUsed);
                if (numCharsUsed != 16)
                {
                    retentionId = 0;
                }
            }
            return retentionId;
        }
    };

    /// Construct from binary blob
    PipelineArFileReader(Span<const char> blob) : ArFileReader(blob) {}

    /// Get iterator for archive members.
    Iterator Begin() { return Iterator(this); }
};

} // namespace Abi

} // namespace Util
