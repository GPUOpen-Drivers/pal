/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palArFile.h"

using namespace Util;

// Magic character sequence at the end of each file header in the archive.
static constexpr const char EndChars[] = "`\n";

// =====================================================================================================================
// Get the overall size in bytes of the archive file to be written.
//
// @returns Size in bytes
size_t ArFileWriter::GetSize()
{
    size_t dataLen = 0;
    uint32 numMembers = GetNumMembers();

    // Determine the ar format by looking at the names.
    uint32 maxNameLen = 0;
    bool haveSpaceInName = false;
    for (uint32 idx = 0; idx != numMembers; ++idx)
    {
        Span<const char> name = GetMemberName(idx);
        maxNameLen = Util::Max(maxNameLen, uint32(name.NumElements()));
        if (haveSpaceInName == false)
        {
            haveSpaceInName = (memchr(name.Data(), ' ', name.NumElements()) != nullptr);
        }
    }
    if ((maxNameLen <= sizeof(FileHeader::name)) && (haveSpaceInName == false))
    {
        m_format = Format::Traditional;
    }
    else if (maxNameLen < sizeof(FileHeader::name))
    {
        m_format = Format::Svr4Short;
    }
    else
    {
        m_format = Format::Svr4Long;
    }

    // Add up size, including size of extended names table.
    m_extendedNamesLen = 0;
    for (uint32 idx = 0; idx != numMembers; ++idx)
    {
        Span<const char> name = GetMemberName(idx);
        if (m_format == Format::Svr4Long)
        {
            m_extendedNamesLen += name.NumElements() + 2;
        }
        // Round up member size to even number of bytes.
        size_t memberDataLen = GetMember(idx, nullptr, 0);
        dataLen += Pow2Align(memberDataLen, 2);
    }
    size_t totalSize = sizeof(GlobalHeader) + dataLen + numMembers * sizeof(FileHeader);
    if (m_extendedNamesLen != 0)
    {
        m_extendedNamesLen = Pow2Align(m_extendedNamesLen, 2);
        totalSize += sizeof(FileHeader) + m_extendedNamesLen;
    }
    return totalSize;
}

// =====================================================================================================================
// Write the archive into the supplied buffer. Must call GetSize() first.
//
// @param [out] pBuffer Buffer to write into
// @param bufferSize    Size of buffer in bytes (as returned by GetSize())
void ArFileWriter::Write(
    char*  pBuffer,
    size_t bufferSize)
{
    PAL_ASSERT(pBuffer != nullptr);
    char* pBufferEnd = pBuffer + bufferSize;
    uint32 numMembers = GetNumMembers();
    char* pWrite = pBuffer + sizeof(GlobalHeader);

    // Write the global header.
    char* pExtendedNamesStart = pWrite;
    char* pExtendedNamesWrite = pWrite;
    if (pWrite <= pBufferEnd)
    {
        memcpy(pBuffer, ArFileMagic, sizeof(GlobalHeader));

        if ((m_extendedNamesLen != 0) && ((pBufferEnd - pWrite) >= (sizeof(FileHeader) + m_extendedNamesLen)))
        {
            // Write header for extended names and leave space for the extended names.
            WriteFileHeader(Span<const char>("//", 2),
                            m_extendedNamesLen,
                            pWrite);
            pWrite += sizeof(FileHeader);
            pExtendedNamesStart = pWrite;
            pExtendedNamesWrite = pWrite;
            pWrite += m_extendedNamesLen;
        }
    }

    // Process each member.
    for (uint32 idx = 0; idx != numMembers; ++idx)
    {
        Span<const char> name = GetMemberName(idx);
        size_t memberLen = GetMember(idx, nullptr, 0);

        if ((pBufferEnd - pWrite) < (sizeof(FileHeader) + memberLen))
        {
            break;
        }

        // Handle extended name.
        char nameBuf[sizeof(FileHeader::name) + 1];
        if (m_format == Format::Svr4Long)
        {
            uint32 nameOffset = uint32(pExtendedNamesWrite - pExtendedNamesStart);
            if ((pExtendedNamesWrite + name.NumElements() + 2) <= pBufferEnd)
            {
                memcpy(pExtendedNamesWrite, name.Data(), name.NumElements());
                pExtendedNamesWrite += name.NumElements();
                memcpy(pExtendedNamesWrite, "/\n", 2);
                pExtendedNamesWrite += 2;
            }
            // Set the standard non-extended name to point to the extended name.
            Snprintf(nameBuf, sizeof(nameBuf), "/%u", nameOffset);
            name = Span<const char>(nameBuf, strlen(nameBuf));
        }

        // Write the file header.
        WriteFileHeader(name, memberLen, pWrite);
        pWrite += sizeof(FileHeader);

        // Write the member data.
        pWrite += GetMember(idx, pWrite, pBufferEnd - pWrite);
        if ((memberLen % 2) != 0)
        {
            // Add padding \n to regain even offset.
            *pWrite++ = '\n';
        }
    }
    const char* pExtendedNamesEnd = pExtendedNamesStart + m_extendedNamesLen;
    PAL_ASSERT((pExtendedNamesWrite == pExtendedNamesEnd) ||
               (pExtendedNamesWrite == pExtendedNamesEnd - 1));
    PAL_ASSERT(pWrite == pBufferEnd);
}

// =====================================================================================================================
// Write a file header
//
// @param name         Name of member
// @param size         Size in bytes of member
// @param [out] pWrite Where to write the header
void ArFileWriter::WriteFileHeader(
    Span<const char> name,
    size_t           size,
    void*            pWrite)
{
    // Write:
    // char name[16];    // Name is /-terminated (Format::Svr4Short) then space padded
    // char modTime[12]; // We write 0
    // char owner[6];    // We write 0
    // char group[6];    // We write 0
    // char mode[8];     // We write 644
    // char size[10];    // Size of member data
    // char endChars[2]; // Separately written to avoid the snprintf's 0 termination overwriting the next
    //                      thing
    FileHeader* pWriteHeader = static_cast<FileHeader*>(pWrite);
    const char* pNamePadding = &"/                "[(m_format != Format::Svr4Short)];
    Snprintf(pWriteHeader->name,
             sizeof(FileHeader),
             "%.*s%.*s0           0     0     644     %-10u",
             int(name.NumElements()),
             name.Data(),
             int(sizeof(pWriteHeader->name) - name.NumElements()),
             pNamePadding,
             size);
    memcpy(pWriteHeader->endChars, EndChars, sizeof(pWriteHeader->endChars));
}

// =====================================================================================================================
// Construct iterator from ArFileReader, setting to the beginning.
ArFileReader::Iterator::Iterator(
    ArFileReader* pReader)
    :
    m_pReader(pReader),
    m_pHeader(nullptr)
{
    // Check global header is well-formed.
    if ((m_pReader->m_blob.NumElements() >= sizeof(GlobalHeader)) &&
        (memcmp(m_pReader->m_blob.Data(), ArFileMagic, sizeof(GlobalHeader)) == 0))
    {
        // Set m_pHeader to the first element if any.
        m_pHeader = reinterpret_cast<const FileHeader*>(m_pReader->m_blob.Data() + sizeof(GlobalHeader));
        // Remember and skip extended filename section. This also checks that the first header is
        // valid.
        SkipExtendedNames();
    }
    else
    {
        PAL_ALERT_ALWAYS_MSG("Malformed archive");
        m_pReader->m_malformed = true;
    }
}

// =====================================================================================================================
// If the current header is for an extended names section, remember and skip it.
// This also spots the case that the iterator has gone off the end, and sets m_pHeader to nullptr.
void ArFileReader::Iterator::SkipExtendedNames()
{
    if (IsValidHeader() && (memcmp(m_pHeader->name, "//              ", sizeof(m_pHeader->name)) == 0))
    {
        // Extended names section.
        if ((m_pReader->m_extendedNames.NumElements() != 0) &&
            (m_pReader->m_extendedNames.Data() != GetData().Data()))
        {
            // Can't have more than one extended names section.
            PAL_ALERT_ALWAYS_MSG("Malformed archive");
            m_pReader->m_malformed = true;
        }
        else
        {
            m_pReader->m_extendedNames = GetData();
            Next();
        }
    }
}

// =====================================================================================================================
// Check if the current header pointer is valid, including that the entry fits into the archive file.
// This also sets m_size, the data size of the current entry, and m_name, the (possibly extended) name
// of the current entry.
bool ArFileReader::Iterator::IsValidHeader()
{
    bool isValid = false;
    // If header pointer is right on the end, or if we have already set m_pHeader to nullptr, that
    // is not a valid header, but it is not an error.
    if ((m_pHeader != nullptr) && (reinterpret_cast<const char*>(m_pHeader) !=
                                   m_pReader->m_blob.Data() + m_pReader->m_blob.NumElements()))
    {
        // Check for the pointed-to header being entirely within the blob, and having the terminating
        // '`\n' chars.
        if ((m_pReader->m_blob.NumElements() > sizeof(FileHeader)) &&
            (reinterpret_cast<const char*>(m_pHeader) - m_pReader->m_blob.Data() <
             m_pReader->m_blob.NumElements() - sizeof(FileHeader)) &&
            (memcmp(m_pHeader->endChars, EndChars, sizeof(m_pHeader->endChars)) == 0))
        {
            // Parse the entry size with strtoul. That is safe as we have already checked that the
            // terminating "`\n" chars are there.
            m_size = strtoul(m_pHeader->size, nullptr, 10);
            // Check for the entry fitting into the archive.
            if (m_size <= m_pReader->m_blob.NumElements() -
                          (reinterpret_cast<const char*>(m_pHeader + 1) - m_pReader->m_blob.Data()))
            {
                // Set the name, checking for extended name.
                // First check for extended name.
                if ((m_pHeader->name[0] == '/') && isdigit(m_pHeader->name[1]))
                {
                    size_t extendedNameOffset = strtoul(&m_pHeader->name[1], nullptr, 10);
                    m_name = m_pReader->m_extendedNames.DropFront(extendedNameOffset);
                    const char* pTerminator = static_cast<const char*>(
                                                  memchr(m_name.Data(), '\n', m_name.NumElements()));
                    if (pTerminator != nullptr)
                    {
                        m_name = m_name.Subspan(0, pTerminator - m_name.Data());
                        if ((m_name.NumElements() >= 2) && (m_name.Back() == '/'))
                        {
                            m_name = m_name.DropBack(1);
                        }
                        isValid = true;
                    }
                }
                else
                {
                    // Then check for non-extended name with / termination (SysV/GNU, which is what our
                    // writer class writes).
                    const char* pTerminator = static_cast<const char*>(
                                                  memchr(m_pHeader->name, '/', sizeof(m_pHeader->name)));
                    if (pTerminator == nullptr)
                    {
                        // Then check for non-extended name with space termination (other ar formats).
                        pTerminator = static_cast<const char*>(
                                         memchr(m_pHeader->name, ' ', sizeof(m_pHeader->name)));
                    }
                    if (pTerminator != nullptr)
                    {
                        m_name = Span<const char>(m_pHeader->name, pTerminator - m_pHeader->name);
                    }
                    else
                    {
                        // No termination at all, so take the whole 16 byte field (other ar formats).
                        m_name = Span<const char>(m_pHeader->name);
                    }
                    isValid = true;
                }
            }
        }
        if (isValid == false)
        {
            PAL_ALERT_ALWAYS_MSG("Malformed archive");
        }
    }
    if (isValid == false)
    {
        m_pHeader = nullptr;
    }
    return isValid;
}
