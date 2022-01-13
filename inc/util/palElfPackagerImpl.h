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
/**
 ***********************************************************************************************************************
 * @file  palElfPackagerImpl.h
 * @brief * @brief PAL utility collection ElfReadContext and ElfWriteContext class implementations.
 ***********************************************************************************************************************
 */

#pragma once

#include "palElfPackager.h"
#include "palHashMapImpl.h"
#include "palListImpl.h"

namespace Util
{

// =====================================================================================================================
template <typename Allocator>
ElfWriteContext<Allocator>::ElfWriteContext(
    Allocator*const pAllocator)
    :
    m_header(),
    m_shStrTab(),
    m_pSharedStringTable(nullptr),
    m_sectionList(pAllocator),
    m_pAllocator(pAllocator)
{
    m_header.e_ident32[0] = ElfMagic;
    m_header.e_ident[4]   = 1; // ELF32
    m_header.e_ident[5]   = 1; // little endian
    m_header.e_ident[6]   = 1; // ELF version number

    m_header.e_machine    = ElfAmdMachine;
    m_header.e_version    = 1;
    m_header.e_ehsize     = sizeof(ElfFormatHeader);
    m_header.e_shentsize  = sizeof(ElfSectionHeader);
    m_header.e_shnum      = 2; // NULL and .shstrtab sections
    m_header.e_shstrndx   = 1; // .shstrtab is after the NULL section.

    m_shStrTab.secHead.sh_type   = ElfShtStrTab;
    m_shStrTab.secHead.sh_flags  = ElfShfStrings;
    m_shStrTab.secHead.sh_offset = sizeof(ElfFormatHeader);
}

// =====================================================================================================================
template <typename Allocator>
ElfWriteContext<Allocator>::~ElfWriteContext()
{
    PAL_SAFE_DELETE_ARRAY(m_pSharedStringTable, m_pAllocator);

    auto sectionIterator = m_sectionList.Begin();
    while (sectionIterator.Get() != nullptr)
    {
        ElfWriteSectionBuffer* pBuf = *(sectionIterator.Get());
        pBuf->~ElfWriteSectionBuffer();

        m_sectionList.Erase(&sectionIterator);
        PAL_SAFE_FREE(pBuf, m_pAllocator);
    }
}

// =====================================================================================================================
// Generates a new section header for the binary section, and then add it to the linked list.
template <typename Allocator>
Result ElfWriteContext<Allocator>::AddBinarySection(
    const char* pName,       // [in] Name of the section to add.
    const void* pData,       // [in] Pointer to the binary data to store.
    size_t      dataLength)  // Length of the data buffer.
{
    PAL_ASSERT(pName != nullptr);
    PAL_ASSERT(pData != nullptr);
    PAL_ASSERT(dataLength > 0);

    void* pDstData = nullptr;
    Result result = AddReservedSection(pName, dataLength, &pDstData);
    if (result == Result::Success)
    {
        memcpy(pDstData, pData, dataLength);
    }

    return result;
}

// =====================================================================================================================
// Generates a new section header for the binary section, and then add it to the linked list.
template <typename Allocator>
Result ElfWriteContext<Allocator>::AddReservedSection(
    const char* pName,       // [in] Name of the section to add.
    size_t      dataLength,  // Length of the data buffer.
    void**      ppData)      // [in] Pointer to the binary data to store.
{
    PAL_ASSERT(pName  != nullptr);
    PAL_ASSERT(ppData != nullptr);
    PAL_ASSERT(dataLength > 0);

    Result result = Result::ErrorOutOfMemory;

    // Allocate storage for the section buffer entry, the section name and the data payload.
    const size_t totalBytes = (sizeof(ElfWriteSectionBuffer) + (strlen(pName) + 1) + dataLength);
    void*        pStorage   = PAL_MALLOC(totalBytes, m_pAllocator, AllocInternalTemp);

    if (pStorage != nullptr)
    {
        auto*const pSectionBuf = PAL_PLACEMENT_NEW(pStorage) ElfWriteSectionBuffer;
        PAL_ASSERT(pSectionBuf != nullptr);

        pSectionBuf->pData = reinterpret_cast<uint8*>(pSectionBuf + 1);
        pSectionBuf->pName = reinterpret_cast<char*>(pSectionBuf->pData + dataLength);

        pSectionBuf->secHead.sh_size      = static_cast<uint32>(dataLength);
        pSectionBuf->secHead.sh_type      = ElfShtProgBits;
        pSectionBuf->secHead.sh_addralign = 1;

        if (strcmp(".text", pName) == 0)
        {
            pSectionBuf->secHead.sh_flags = ElfShfAlloc | ElfShfExecInstr;
        }

        strcpy(pSectionBuf->pName, pName);
        (*ppData) = pSectionBuf->pData;

        result = m_sectionList.PushBack(pSectionBuf);

        m_header.e_shnum++;
    }

    return result;
}

// =====================================================================================================================
// Determines the size needed for a memory buffer to store this ELF.
template <typename Allocator>
size_t ElfWriteContext<Allocator>::GetRequiredBufferSizeBytes()
{
    // Update offsets and size values
    AssembleSharedStringTable();
    CalculateSectionHeaderOffset();

    size_t totalBytes = sizeof(ElfFormatHeader) + m_shStrTab.secHead.sh_size;

    // Iterate through the section list.
    auto sectionIterator = m_sectionList.Begin();
    while (sectionIterator.Get() != nullptr)
    {
        const ElfWriteSectionBuffer* pBuf = *(sectionIterator.Get());
        totalBytes += pBuf->secHead.sh_size;
        sectionIterator.Next();
    }

    totalBytes += m_header.e_shentsize * m_header.e_shnum;

    return totalBytes;
}

// =====================================================================================================================
// Assembles the names of sections into a buffer and stores the size in the .shstrtab section header.  Each section
// header stores the offset to its name string into the shared string table in its secHead.sh_name field.
template <typename Allocator>
void ElfWriteContext<Allocator>::AssembleSharedStringTable()
{
    if (m_pSharedStringTable != nullptr)
    {
        PAL_SAFE_DELETE_ARRAY(m_pSharedStringTable, m_pAllocator);
        m_shStrTab.secHead.sh_size = 0;
    }

    size_t totalLen = 1; // The leading null allows for a null name for the null section.

    // Each name in the table is followed by a null.
    totalLen += strlen(ShStrtabName) + 1;

    auto sectionIterator = m_sectionList.Begin();
    while (sectionIterator.Get() != nullptr)
    {
        const ElfWriteSectionBuffer* pBuf = *(sectionIterator.Get());
        totalLen += strlen(pBuf->pName) + 1;

        sectionIterator.Next();
    }
    totalLen += 1; // Final null terminator.

    m_pSharedStringTable = PAL_NEW_ARRAY(char, totalLen, m_pAllocator, AllocInternalTemp);
    PAL_ASSERT(m_pSharedStringTable != nullptr);

    char* pStrtabPtr = &m_pSharedStringTable[0];
    *pStrtabPtr++ = 0; // First byte is null.

    strcpy(pStrtabPtr, ShStrtabName);
    m_shStrTab.secHead.sh_name = static_cast<uint32>(pStrtabPtr  - m_pSharedStringTable);
    pStrtabPtr += strlen(ShStrtabName) + 1;

    sectionIterator = m_sectionList.Begin();
    while (sectionIterator.Get() != nullptr)
    {
        ElfWriteSectionBuffer* const pBuf = *(sectionIterator.Get());
        strcpy(pStrtabPtr, pBuf->pName);
        pBuf->secHead.sh_name = static_cast<uint32>(pStrtabPtr - m_pSharedStringTable);
        pStrtabPtr += strlen(pBuf->pName) + 1;

        sectionIterator.Next();
    }

    *pStrtabPtr++ = 0; // Table ends with a double null terminator.

    m_shStrTab.secHead.sh_size = static_cast<uint32>(pStrtabPtr - m_pSharedStringTable);
}

// =====================================================================================================================
// Determines the offset of the section header table by totaling the sizes of each binary chunk written to the ELF file,
// accounting for alignment.
template <typename Allocator>
void ElfWriteContext<Allocator>::CalculateSectionHeaderOffset()
{
    uint32 sharedHdrOffset = 0;

    constexpr uint32 ElfHdrSize = sizeof(ElfFormatHeader);
    sharedHdrOffset += ElfHdrSize;

    const uint32 stHdrSize = m_shStrTab.secHead.sh_size;
    sharedHdrOffset += stHdrSize;

    auto sectionIterator = m_sectionList.Begin();
    while (sectionIterator.Get() != nullptr)
    {
        const ElfWriteSectionBuffer* pBuf = *(sectionIterator.Get());
        const uint32 secSzBytes = pBuf->secHead.sh_size;
        sharedHdrOffset += secSzBytes;

        sectionIterator.Next();
    }

    m_header.e_shoff = sharedHdrOffset;
}

// =====================================================================================================================
// Writes the data out to the given buffer in ELF format.  Assumes the buffer has been pre-allocated with adequate
// space, which can be determined with a call to GetRequireBufferSizeBytes().
//
// ELF data is stored in the buffer like so:
//
//
// + ELF header
// + String Table for Section Headers
//
// + Section Buffer (b0) [NULL]
// + Section Buffer (b1) [.shstrtab]
// + ...            (b#) [???]
//
// + Section Header (h0) [NULL]
// + Section Header (h1) [.shstrtab]
// + Section Header (h#) [???]
template <typename Allocator>
void ElfWriteContext<Allocator>::WriteToBuffer(
    char*  pBuffer,
    size_t bufSize)
{
    PAL_ASSERT(pBuffer != nullptr);

    // Update offsets and size values
    AssembleSharedStringTable();
    CalculateSectionHeaderOffset();

    const size_t reqSize = GetRequiredBufferSizeBytes();
    PAL_ASSERT(bufSize >= reqSize);

    memset(pBuffer, 0, reqSize);

    char* pWrite = static_cast<char*>(pBuffer);

    // ELF header comes first.
    constexpr uint32 ElfHdrSize = sizeof(ElfFormatHeader);
    memcpy(pWrite, &m_header, ElfHdrSize);
    pWrite += ElfHdrSize;

    // Write the section buffer table.
    const uint32 sstSize = m_shStrTab.secHead.sh_size;
    memcpy(pWrite, m_pSharedStringTable, sstSize);
    pWrite += sstSize;

    // Write each section buffer.
    auto sectionIterator = m_sectionList.Begin();
    while (sectionIterator.Get() != nullptr)
    {
        ElfWriteSectionBuffer* const pBuf = *(sectionIterator.Get());
        pBuf->secHead.sh_offset = static_cast<uint32>(pWrite - pBuffer);
        const uint32 sizeBytes = pBuf->secHead.sh_size;
        memcpy(pWrite, pBuf->pData, sizeBytes);
        pWrite += sizeBytes;

        sectionIterator.Next();
    }

    PAL_ASSERT(m_header.e_shoff == static_cast<uint32>(pWrite - pBuffer));

    // Write the section header table out.
    // Start with a NULL section header.
    constexpr uint32 SecHdrSize = sizeof(ElfSectionHeader);
    pWrite += SecHdrSize;

    // Write .shstrtab section header.
    memcpy(pWrite, &m_shStrTab.secHead, SecHdrSize);
    pWrite += SecHdrSize;

    sectionIterator = m_sectionList.Begin();
    while (sectionIterator.Get() != nullptr)
    {
        const ElfWriteSectionBuffer* pBuf = *(sectionIterator.Get());
        memcpy(pWrite, &pBuf->secHead, SecHdrSize);
        pWrite += SecHdrSize;

        sectionIterator.Next();
    }

    PAL_ASSERT(VoidPtrDiff(pWrite, pBuffer) == reqSize);
}

// =====================================================================================================================
template <typename Allocator>
ElfReadContext<Allocator>::ElfReadContext(
    Allocator*const pAllocator)
    :
    m_header(),
    m_shStrTab(),
    m_pSharedStringTable(nullptr),
    m_map(ElfBucketNum, pAllocator),
    m_pAllocator(pAllocator)
{
}

// =====================================================================================================================
template <typename Allocator>
ElfReadContext<Allocator>::~ElfReadContext()
{
    for (auto iterator = m_map.Begin(); iterator.Get() != nullptr; iterator.Next())
    {
        auto* pEntry = iterator.Get();
        ElfReadSectionBuffer* pBuffer = pEntry->value;
        PAL_SAFE_DELETE(pBuffer, m_pAllocator);
    }
    m_map.Reset();
}

// =====================================================================================================================
// Reads ELF data in from the given buffer into the context.
//
// ELF data is stored in the buffer like so:
//
// + ELF header
// + Section Header String Table
//
// + Section Buffer (b0) [NULL]
// + Section Buffer (b1) [.shstrtab]
// + ...            (b#) [...]
//
// + Section Header (h0) [NULL]
// + Section Header (h1) [.shstrtab]
// + ...            (h#) [...]
template <typename Allocator>
Result ElfReadContext<Allocator>::ReadFromBuffer(
    const void* pBuffer,   // [retained] Input ELF data buffer.
    size_t*     pBufSize)  // [out] Size of the given read buffer.  Determined from the ELF header.
{
    PAL_ASSERT(pBuffer != nullptr);

    const uint8* pData = static_cast<const uint8*>(pBuffer);

    // ELF header is always located at the beginning of the file.
    const ElfFormatHeader* pHeader = static_cast<const ElfFormatHeader*>(pBuffer);

    // If the identification info isn't the magic number, this isn't a valid file.
    Result ret = (pHeader->e_ident32[0] == ElfMagic) ? Result::Success : Result::ErrorInvalidFormat;

    if (ret == Result::Success)
    {
        ret = (pHeader->e_machine == ElfAmdMachine) ? Result::Success : Result::ErrorInvalidFormat;
    }

    // Initialize the section map.
    if (ret == Result::Success)
    {
        ret = m_map.Init();
    }

    if (ret == Result::Success)
    {
        m_header = *pHeader;
        size_t readSize = sizeof(ElfFormatHeader);

        // Section header location information.
        const uint32 sectionHeaderOffset = pHeader->e_shoff;
        const uint32 sectionHeaderNum    = pHeader->e_shnum;
        const uint32 sectionHeaderSize   = pHeader->e_shentsize;

        const uint32 sectionBufferTableOffset = pHeader->e_ehsize;

        for (uint32 section = 0; section < sectionHeaderNum; section++)
        {
            // Where the header is located for this section
            const uint32 sectionOffset = sectionHeaderOffset + (section * sectionHeaderSize);
            const ElfSectionHeader* pSectionHeader = reinterpret_cast<const ElfSectionHeader*>(pData + sectionOffset);
            readSize += sizeof(ElfSectionHeader);

            // Where the name is located for this section
            const uint32 sectionNameOffset = sectionBufferTableOffset + pSectionHeader->sh_name;
            const char* pSectionName = reinterpret_cast<const char*>(pData + sectionNameOffset);

            // Where the data is located for this section
            const uint32 sectionDataOffset = pSectionHeader->sh_offset;
            ElfReadSectionBuffer* pBuf = PAL_NEW(ElfReadSectionBuffer, m_pAllocator, AllocInternalTemp);

            ret = (pBuf != nullptr) ? Result::Success : Result::ErrorOutOfMemory;

            if (ret == Result::Success)
            {
                pBuf->secHead = *pSectionHeader;
                pBuf->pName   = pSectionName;
                pBuf->pData   = (pData + sectionDataOffset);

                readSize += pSectionHeader->sh_size;

                // If this is the section header string table, update the context
                if (strcmp(pSectionName, ShStrtabName) == 0)
                {
                    m_shStrTab = *pBuf;
                    m_pSharedStringTable = reinterpret_cast<const char*>(pBuf->pData);
                }

                m_map.Insert(pSectionName, pBuf);
            }
        }

        *pBufSize = readSize;
    }

    return ret;
}

// =====================================================================================================================
// Retrieves the section data for the specified section name, if it exists.
template <typename Allocator>
Result ElfReadContext<Allocator>::GetSectionData(
    const char*  pName,       // [in] Name of the section to look for.
    const void** pData,       // [out] Pointer to section data.
    size_t*      pDataLength  // [out] Size of the section data.
    ) const
{
    Result ret = Result::ErrorInvalidValue;

    ElfReadSectionBuffer** const ppBuf = m_map.FindKey(pName);

    if (ppBuf != nullptr)
    {
        *pData = (*ppBuf)->pData;
        *pDataLength = (*ppBuf)->secHead.sh_size;

        ret = Result::Success;
    }

    return ret;
}

} // Util
