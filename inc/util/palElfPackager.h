/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palElfPackager.h
 * @brief PAL utility collection ElfReadContext and ElfWriteContext class declarations.
 ***********************************************************************************************************************
 */

#pragma once

#include "palHashMap.h"
#include "palList.h"

namespace Util
{

// Structs and defines taken from readelf.h.
static constexpr uint32 ElfMagic      = 0x464C457F;   ///< "/177ELF" in little endian.
static constexpr uint32 ElfIdentSize  = 16;           ///< Identification information size.
static constexpr int16  ElfAmdMachine = 0x3FD;        ///< AMD GPU magic number, machine architecture.
static const     char*  ShStrtabName  = ".shstrtab";  ///< Section header name for the string table.
static constexpr uint32 ElfBucketNum  = 32;           ///< Number of buckets for the read HashMap.

#pragma pack (push, 1)
/// ELF file header. This is specified at the very beginning of every ELF file.
struct ElfFormatHeader
{
    union
    {
        char   e_ident[ElfIdentSize];      ///< ELF identification info.
        uint32 e_ident32[ElfIdentSize/4];  ///< Bytes grouped for easy magic number setting.
    };
    int16  e_type;       ///< 1 = relocatable, 3 = shared.
    int16  e_machine;    ///< Machine architecture constant, 0x3fd = AMD GPU.
    uint32 e_version;    ///< ELF format version(1).
    uint32 e_entry;      ///< Entry point if executable(0).
    uint32 e_phoff;      ///< File offset of program header(unused, 0).
    uint32 e_shoff;      ///< File offset of section header.
    uint32 e_flags;      ///< Architecture-specific flags.
    int16  e_ehsize;     ///< Size of this ELF header.
    int16  e_phentsize;  ///< Size of an entry in program header(unused, 0).
    int16  e_phnum;      ///< # of entries in program header(0).
    int16  e_shentsize;  ///< Size of an entry in section header.
    int16  e_shnum;      ///< # of entries in section header.
    int16  e_shstrndx;   ///< Section # that contains section name strings.
};

/// ELF section header. Every data section is located using the section header.
struct ElfSectionHeader
{
    uint32 sh_name;       ///< Name(index into string table).
    uint32 sh_type;       ///< Section type.
    uint32 sh_flags;      ///< Flag bits (SectionHeaderFlags enum).
    uint32 sh_addr;       ///< Base memory address if loadable(0).
    uint32 sh_offset;     ///< File position of start of section.
    uint32 sh_size;       ///< Size of section in bytes.
    uint32 sh_link;       ///< Section # with related info(unused, 0).
    uint32 sh_info;       ///< More section-specific info.
    uint32 sh_addralign;  ///< Alignment granularity in power of 2(1).
    uint32 sh_entsize;    ///< Size of entries if section is array.
};
#pragma pack (pop)

/// ELF Constants from GNU readelf indicating section type.
enum ElfSectionHeaderTypes : uint32
{
    ElfShtProgBits = 1,  ///< Executable data.
    ElfShtStrTab   = 3,  ///< String table.
};

/// ELF Constants from GNU readelf indicating data type.
enum ElfSectionHeaderFlags : uint32
{
    ElfShfAlloc     = 0x02,  ///< Section occupies memory during execution.
    ElfShfExecInstr = 0x04,  ///< Executable data.
    ElfShfStrings   = 0x20,  ///< Readable strings.
};

/// A named buffer to hold section data and metadata.
struct ElfWriteSectionBuffer
{
    uint8*           pData;    ///< Pointer to binary data buffer.
    char*            pName;    ///< Section name.
    ElfSectionHeader secHead;  ///< Section metadata.
};

/// A named buffer to hold constant section data and metadata.
struct ElfReadSectionBuffer
{
    const uint8*     pData;    ///< Pointer to binary data buffer.
    const char*      pName;    ///< Section name.
    ElfSectionHeader secHead;  ///< Section metadata.
};

/// HashMap for storing read ELF sections.
template <typename Allocator>
struct SectionMap
{
    typedef HashMap<const char*, ElfReadSectionBuffer*, Allocator, StringJenkinsHashFunc, StringEqualFunc> Section;
};

/**
 ***********************************************************************************************************************
 * @brief Context for writing data to an [Executable and Linkable Format (ELF)] buffer.
 * The client should call AddBinarySection() as necessary to add one or more named sections to the ELF.  After all
 * sections are added, the client should call GetRequiredBufferSizeBytes(), allocate the specified amount of memory,
 * then call WriteToBuffer() to get the final ELF binary.
 ***********************************************************************************************************************
 */
template <typename Allocator>
class ElfWriteContext
{
public:
    /// Constructor.
    ///
    /// @param [in] pAllocator The allocator that will allocate memory if required.
    ElfWriteContext(Allocator*const pAllocator);
    ~ElfWriteContext();

    /// Adds a named section of binary data to the ELF.
    ///
    /// @param [in] pName      Null-terminated string specifying the name of the section to be added.
    /// @param [in] pData      Pointer to the data to be copied into the ELF.
    /// @param [in] dataLength Amount of data to be copied from pData into the ELF, in bytes.
    ///
    /// @returns @ref Success if the section was successfully added or @ref ErrorOutOfMemory if the operation failed
    ///          because of an internal failure to allocate system memory.
    Result AddBinarySection(const char* pName, const void* pData, size_t dataLength);

    /// Adds a named section of data to the ELF.  Unlike @ref AddBinarySection(), this method adds an empty section
    /// of the specified size, and returns a pointer to the reserved space so that the caller can fill it in later.
    ///
    /// @param [in]  pName      Null-terminated string specifying the name of the section to be added.
    /// @param [in]  dataLength Amount of data to be stored in the new section, in bytes.
    /// @param [out] ppData     Pointer to the reserved space in the ELF where the caller can write its data.
    ///
    /// @returns @ref Success if the section was successfully added or @ref ErrorOutOfMemory if the operation failed
    ///          because of an internal failure to allocate system memory.
    Result AddReservedSection(const char* pName, size_t dataLength, void** ppData);

    /// Returns the amount of storage that will be needed by WriteToBuffer().
    size_t GetRequiredBufferSizeBytes();

    /// Writes the completed ELF to the specified memory.
    ///
    /// @param [out] pBuffer Pointer to memory where the ELF should be written.
    /// @param [in]  bufSize Number of bytes available at pBuffer.
    void WriteToBuffer(char* pBuffer, size_t bufSize);

private:
    void AssembleSharedStringTable();
    void CalculateSectionHeaderOffset();

    ElfFormatHeader                         m_header;              // ELF header.
    ElfWriteSectionBuffer                   m_shStrTab;            // Section header for string table.
    char*                                   m_pSharedStringTable;  // String table of section names.
    List<ElfWriteSectionBuffer*, Allocator> m_sectionList;         // List of section data and headers.
    Allocator*const                         m_pAllocator;           // Allocator for this ElfWriteContext.

    PAL_DISALLOW_COPY_AND_ASSIGN(ElfWriteContext<Allocator>);
};

/**
 ***********************************************************************************************************************
 * @brief Context for reading data from an [Executable and Linkable Format (ELF)] buffer.
 * The client should call ReadFromBuffer() to initialize the context with the contents of an ELF, then GetSectionData()
 * to retrieve the contents of a particular named section.
 ***********************************************************************************************************************
 */
template <typename Allocator>
class ElfReadContext
{
public:
    /// Constructor.
    ///
    /// @param [in] pAllocator The allocator that will allocate memory if required.
    ElfReadContext(Allocator*const pAllocator);
    ~ElfReadContext();

    /// Initializes the context with the contents of a binary ELF.
    ///
    /// Must be called before GetSectionData().
    ///
    /// @param [in]  pBuffer  Pointer to the beginning of an ELF.
    /// @param [out] pBufSize Number of bytes read from the ELF (determined via the header of the ELF specified in
    ///                       pBuffer.
    ///
    /// @returns @ref Success if the ELF was successfully read.  Otherwise, one of the following errors may be returned:
    ///          + @ref ErrorInvalidFormat if the ELF is malformed or corrupt.
    ///          + @ref ErrorOutOfMemory if the operation failed because of an internal failure to allocate system
    ///            memory.
    Result ReadFromBuffer(const void* pBuffer, size_t* pBufSize);

    /// Retrieves a pointer to the data of a specific named section of an ELF.
    ///
    /// @param [in]  pName       Null-terminated string specifying the name of the section to retrieve.
    /// @param [out] ppData      Pointer into the ELF where the specified section starts.
    /// @param [out] pDataLength Size of the specified section in bytes.
    ///
    /// @returns @ref Success if the section was found and returned in ppData.  @ref ErrorInvalidValue will be returned
    ///          if the specified section name was not found in the ELF.
    Result GetSectionData(const char* pName, const void** ppData, size_t* pDataLength) const;

    /// Helper method to determine if a section with the specified name is present in this ELF.
    ///
    /// @param [in] pName Null-terminated string specifying the name of the section to check for.
    ///
    /// @returns True if the specified section is present in this ELF.
    bool IsSectionPresent(const char* pName) const { return (m_map.FindKey(pName) != nullptr); }

private:
    ElfFormatHeader                         m_header;              // ELF header.
    ElfReadSectionBuffer                    m_shStrTab;            // Section header for string table.
    const char*                             m_pSharedStringTable;  // String table of section names.
    typename SectionMap<Allocator>::Section m_map;                 // Hashmap for storing section data/headers.
    Allocator*const                         m_pAllocator;          // Allocator for this ElfReadContext.

    PAL_DISALLOW_COPY_AND_ASSIGN(ElfReadContext<Allocator>);
};

} // Util
