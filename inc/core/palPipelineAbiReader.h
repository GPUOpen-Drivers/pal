/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palPipelineAbiReader.h
 * @brief PAL Pipeline ABI utility class implementations. The PipelineAbiReader is a layer on top of ElfReader which
 * loads ELFs compatible with the pipeline ABI.
 ***********************************************************************************************************************
 */

#pragma once

#include "palElfReader.h"
#include "palPipelineAbi.h"
#include "g_palPipelineAbiMetadataImpl.h"

namespace Util
{

namespace Abi
{

/// A reference to a symbol from a symbol table
struct SymbolEntry
{
    /// The symbol section in the ELf
    uint16 m_section;
    /// The index of this symbol in the symbol section
    uint32 m_index;
};

/// The PipelineAbiReader simplifies loading ELFs compatible with the pipeline ABI.
class PipelineAbiReader
{
typedef HashMap<const char*,
                SymbolEntry,
                IndirectAllocator,
                StringJenkinsHashFunc,
                StringEqualFunc,
                HashAllocator<IndirectAllocator>,
                PAL_CACHE_LINE_BYTES * 2> GenericSymbolMap;

public:
    template <typename Allocator>
    PipelineAbiReader(Allocator* const pAllocator, const void* pData);
    Result Init();

    ElfReader::Reader& GetElfReader() { return m_elfReader; }
    const ElfReader::Reader& GetElfReader() const { return m_elfReader; }

    /// Get the Pipeline Metadata as a deserialized struct using the given MsgPackReader instance. If successful,
    /// the reader's position will then be moved to either the start of the registers map, or to EOF if there are
    /// no registers.
    ///
    /// @param [in/out] pReader    Pointer to the MsgPackReader to use and (re)init with the metadata blob.
    /// @param [out]    pMetadata  Pointer to where to store the deserialized metadata.
    ///
    /// @returns Success if successful, ErrorInvalidValue, ErrorUnknown or ErrorUnsupportedPipelineElfAbiVersion
    ///          if a parser error occurred, ErrorInvalidPipelineElf if there is no metadata.
    Result GetMetadata(MsgPackReader* pReader, PalAbi::CodeObjectMetadata* pMetadata) const;

    /// Get the GFXIP version.
    ///
    /// @param [out] pGfxIpMajorVer The major version.
    /// @param [out] pGfxIpMinorVer The minor version.
    /// @param [out] pGfxIpStepping The stepping.
    void GetGfxIpVersion(uint32* pGfxIpMajorVer, uint32* pGfxIpMinorVer, uint32* pGfxIpStepping) const;

    /// Check if a PipelineSymbolEntry exists and return it.
    ///
    /// @param [in]  pipelineSymbolType The PipelineSymbolType to check.
    ///
    /// @returns The found symbol, or nullptr if it was not found.
    const Elf::SymbolTableEntry* GetPipelineSymbol(PipelineSymbolType pipelineSymbolType) const;

    /// Check if a PipelineSymbolEntry exists and return it.
    ///
    /// @param [in]  pName ELF name of the symbol to search for
    ///
    /// @returns The found symbol, or nullptr if it was not found.
    const Elf::SymbolTableEntry* GetGenericSymbol(const char* pName) const;

private:
    IndirectAllocator m_allocator;
    ElfReader::Reader m_elfReader;

    /// The symbols, cached for lookup.
    ///
    /// If the section index of the symbol is 0, it does not exist.
    SymbolEntry m_pipelineSymbols[static_cast<uint32>(PipelineSymbolType::Count)];

    GenericSymbolMap m_genericSymbolsMap;
};

// =====================================================================================================================
template <typename Allocator>
PipelineAbiReader::PipelineAbiReader(Allocator* const pAllocator, const void* pData)
    :
    m_allocator(pAllocator),
    m_elfReader(pData),
    m_genericSymbolsMap(16u, &m_allocator)
{
    PAL_ASSERT(pAllocator);

#if PAL_ENABLE_PRINTS_ASSERTS
    // Required sections
    bool hasNote = false;
    bool hasSymbol = false;
    bool hasText = false;

    for (ElfReader::SectionId sectionIndex = 0; sectionIndex < m_elfReader.GetNumSections(); sectionIndex++)
    {
        const char* pSectionName = m_elfReader.GetSectionName(sectionIndex);
        ElfReader::SectionHeaderType sectionType = m_elfReader.GetSectionType(sectionIndex);

        if (StringEqualFunc<const char*>()(pSectionName, ".text"))
        {
            hasText = true;
        }
        else if (StringEqualFunc<const char*>()(pSectionName, ".note"))
        {
            hasNote = true;
        }
        else if (sectionType == ElfReader::SectionHeaderType::Note)
        {
            hasNote = true;
        }
        else if ((sectionType == ElfReader::SectionHeaderType::SymTab) &&
            (m_elfReader.GetSection(sectionIndex).sh_link != 0))
        {
            hasSymbol = true;
        }
    }

    PAL_ASSERT_MSG(hasNote, "Missing .note section");
    PAL_ASSERT_MSG(hasSymbol, "Missing .symtab section");
    PAL_ASSERT_MSG(hasText, "Missing .text section");
#endif
}

} // Abi
} // Util
