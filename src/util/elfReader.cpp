/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palElfReader.h"

namespace Util
{
namespace ElfReader
{

// =====================================================================================================================
const SectionHeader& Reader::GetSection(SectionId i) const
{
    const void* pAddr = VoidPtrInc(m_pData, static_cast<size_t>(GetHeader().e_shoff));
    PAL_ASSERT_MSG(VoidPtrIsPow2Aligned(pAddr, alignof(SectionHeader)),
        "Invalid alignment, not allowed to cast");
    PAL_ASSERT_MSG(i < GetNumSections(), "Section index is out of range");
    return static_cast<const SectionHeader*>(pAddr)[i];
}

// =====================================================================================================================
const char* Reader::GetSectionName(SectionId i) const
{
    SectionId index = GetHeader().e_shstrndx;
    const char* pName = nullptr;
    if (index)
    {
        pName = static_cast<const char*>(GetSectionData(index)) + GetSection(i).sh_name;
    }
    return pName;
}

// =====================================================================================================================
SectionId Reader::FindSection(const char* pName) const
{
    SectionId i = 1;
    for (; i < GetNumSections(); i++)
    {
        if (StringEqualFunc<const char*>()(GetSectionName(i), pName))
        {
            break;
        }
    }
    if (i == GetNumSections())
    {
        i = 0;
    }
    return i;
}

// =====================================================================================================================
Result Reader::GetSymbol(
    const Elf::SymbolTableEntry& symbol,
    const void**                 ppData
    ) const
{
    Result result = Result::ErrorInvalidPipelineElf;

    if (symbol.st_shndx != 0)
    {
        const void* pSection = GetSectionData(symbol.st_shndx);

        if ((symbol.st_size + symbol.st_value) <= GetSection(symbol.st_shndx).sh_size)
        {
            *ppData = VoidPtrInc(pSection, static_cast<size_t>(symbol.st_value));
            result  = Result::Success;
        }
    }

    return result;
}

// =====================================================================================================================
Result Reader::CopySymbol(
    const Elf::SymbolTableEntry& symbol,
    size_t*                      pSize,
    void*                        pBuffer
    ) const
{
    Result result = Result::ErrorInvalidPipelineElf;

    if (pSize == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else
    {
        if (pBuffer == nullptr)
        {
            *pSize = static_cast<size_t>(symbol.st_size);
            result = Result::Success;
        }
        else if (symbol.st_shndx != 0)
        {
            const void* pSection = GetSectionData(symbol.st_shndx);
            if ((symbol.st_size + symbol.st_value) <= GetSection(symbol.st_shndx).sh_size)
            {
                memcpy(pBuffer,
                       VoidPtrInc(pSection, static_cast<size_t>(symbol.st_value)),
                       static_cast<size_t>(symbol.st_size));
                result = Result::Success;
            }
        }
    }

    return result;
}

// =====================================================================================================================
bool NoteIterator::IsValid() const { return m_pData < m_notes.End().m_pData; }

// =====================================================================================================================
const void* NoteIterator::GetName() const
{
    const void* pName = nullptr;
    if (m_pData->n_namesz != 0)
    {
        pName = VoidPtrInc(static_cast<const void*>(m_pData), sizeof(NoteTableEntryHeader));
    }
    return pName;
}

// =====================================================================================================================
const void* NoteIterator::GetDescriptor() const
{
    const void* pDescriptor = nullptr;
    if (m_pData->n_descsz != 0)
    {
        pDescriptor = VoidPtrInc(static_cast<const void*>(m_pData), sizeof(NoteTableEntryHeader)
            + static_cast<size_t>(RoundUpToMultiple(m_pData->n_namesz, NoteAlignment)));
    }
    return pDescriptor;
}

// =====================================================================================================================
void NoteIterator::Next()
{
    const void* pNext = VoidPtrInc(static_cast<const void*>(m_pData), sizeof(NoteTableEntryHeader)
        + static_cast<size_t>(RoundUpToMultiple(m_pData->n_namesz, NoteAlignment)
            + RoundUpToMultiple(m_pData->n_descsz, NoteAlignment)));
    PAL_ASSERT_MSG(VoidPtrIsPow2Aligned(pNext, alignof(NoteTableEntryHeader)),
        "Invalid alignment, not allowed to cast");
    m_pData = static_cast<const NoteTableEntryHeader*>(pNext);
}

// =====================================================================================================================
NoteIterator Notes::Begin() const
{
    PAL_ASSERT_MSG(VoidPtrIsPow2Aligned(m_reader.GetSectionData(m_section), alignof(NoteTableEntryHeader)),
        "Invalid alignment, not allowed to cast");
    return NoteIterator(*this, static_cast<const NoteTableEntryHeader*>(m_reader.GetSectionData(m_section)));
}

// =====================================================================================================================
NoteIterator Notes::End() const
{
    const void* pAddr = VoidPtrInc(m_reader.GetSectionData(m_section),
        static_cast<size_t>(RoundUpToMultiple(GetHeader().sh_size,
            static_cast<uint64>(NoteAlignment))));
    PAL_ASSERT_MSG(VoidPtrIsPow2Aligned(pAddr, alignof(NoteTableEntryHeader)),
        "Invalid alignment, not allowed to cast");
    return NoteIterator(*this, static_cast<const NoteTableEntryHeader*>(pAddr));
}

// =====================================================================================================================
Symbols::Symbols(const Reader& reader, SectionId section) : m_reader(reader), m_section(section)
{
    PAL_ASSERT_MSG(m_reader.GetSectionType(m_section) == SectionHeaderType::SymTab ||
        m_reader.GetSectionType(m_section) == SectionHeaderType::DynSym,
        "Expected a symbol section but got something else");
}

// =====================================================================================================================
const SymbolTableEntry& Symbols::GetSymbol(uint64 i) const
{
    PAL_ASSERT_MSG(VoidPtrIsPow2Aligned(m_reader.GetSectionData(m_section), alignof(SymbolTableEntry)),
        "Invalid alignment, not allowed to cast");
    return static_cast<const SymbolTableEntry*>(m_reader.GetSectionData(m_section))[i];
}

// =====================================================================================================================
Relocations::Relocations(const Reader& reader, SectionId section) : m_reader(reader), m_section(section)
{
    PAL_ASSERT_MSG(m_reader.GetSectionType(m_section) == SectionHeaderType::Rel ||
        m_reader.GetSectionType(m_section) == SectionHeaderType::Rela,
        "Expected a relocation section but got something else");
}

// =====================================================================================================================
const RelTableEntry& Relocations::GetRel(uint64 i) const
{
    const void* pData = VoidPtrInc(m_reader.GetSectionData(m_section),
        GetEntrySize() * static_cast<size_t>(i));
    PAL_ASSERT_MSG(VoidPtrIsPow2Aligned(pData, alignof(RelTableEntry)),
        "Invalid alignment, not allowed to cast");
    return *static_cast<const RelTableEntry*>(pData);
}

// =====================================================================================================================
const RelaTableEntry& Relocations::GetRela(uint64 i) const
{
    PAL_ASSERT_MSG(VoidPtrIsPow2Aligned(m_reader.GetSectionData(m_section), alignof(RelaTableEntry)),
        "Invalid alignment, not allowed to cast");
    return static_cast<const RelaTableEntry*>(m_reader.GetSectionData(m_section))[i];
}

// =====================================================================================================================
size_t Relocations::GetEntrySize() const
{
    size_t entrySize = sizeof(RelTableEntry);
    if (IsRela())
    {
        entrySize = sizeof(RelaTableEntry);
    }
    return entrySize;
}

} // Elf
} // Util
