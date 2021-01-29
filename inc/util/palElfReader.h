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
 * @file  palElfReader.h
 * @brief PAL Elf utility class declarations.
 * SEE: Section 1 in https://www.uclibc.org/docs/elf-64-gen.pdf for an overview of the ELF format.
 ***********************************************************************************************************************
 */

#pragma once

#include "palAssert.h"
#include "palElf.h"
#include "palHashBaseImpl.h"
#include "palInlineFuncs.h"
#include <iterator>

namespace Util
{
namespace ElfReader
{

using Elf::FileHeader;
using Elf::MachineType;
using Elf::NoteTableEntryHeader;
using Elf::RelTableEntry;
using Elf::RelaTableEntry;
using Elf::SectionHeader;
using Elf::SectionHeaderType;
using Elf::SymbolTableEntry;

// 8 byte alignment for notes according to Elf64 spec.
// However, in practice it is 4 bytes (readelf gets confused by 8)
constexpr uint32 NoteAlignment = 4;

typedef uint16 SectionId;

class Notes;

/**
 ***********************************************************************************************************************
 * @brief A thin wrapper to facilitate access of ELF files.
 ***********************************************************************************************************************
 */
class Reader
{
    friend class Section;

public:
    explicit Reader(const void* pData) : m_pData(pData)
    {
        PAL_ASSERT(pData != nullptr);
        PAL_ASSERT_MSG(VoidPtrIsPow2Aligned(m_pData, alignof(FileHeader)),
            "Invalid alignment, not allowed to cast");
        PAL_ASSERT_MSG(GetHeader().ei_data == Util::Elf::ElfLittleEndian,
            "Elf reader can only read little endian elfs");
    }
    const void* GetData() const { return m_pData; }

    // File information
    const FileHeader& GetHeader() const { return *static_cast<const FileHeader*>(m_pData); }
    MachineType GetTargetMachine() const { return static_cast<MachineType>(GetHeader().e_machine); }

    // Section information
    uint16 GetNumSections() const { return GetHeader().e_shnum; }
    const SectionHeader& GetSection(SectionId i) const;
    SectionHeaderType GetSectionType(SectionId i) const { return static_cast<SectionHeaderType>(GetSection(i).sh_type); }
    const char* GetSectionName(SectionId i) const;

    const void* GetSectionData(SectionId i) const
    {
        return VoidPtrInc(m_pData, static_cast<size_t>(GetSection(i).sh_offset));
    }

    /// Searches a section by its name.
    ///
    /// @returns The found section or 0 if it was not found.
    SectionId FindSection(const char* pName) const;

    /// Copy the data of the given symbol into a buffer.
    ///
    /// @param [out] pSize   The size of the symbol. Is only written if pBuffer is null.
    /// @param [out] pBuffer A buffer for storing the symbol data. If null, pSize will be written.
    ///
    /// @returns Fails if the symbol has no associated section or the ELF is malformed.
    Result CopySymbol(const Elf::SymbolTableEntry& symbol, size_t* pSize, void* pBuffer) const;

private:
    const void* m_pData;
};

/**
 ***********************************************************************************************************************
 * @brief Iterator for traversal of notes in a note section.
 *
 * Supports forward traversal.
 ***********************************************************************************************************************
 */
class NoteIterator
{
public:
    /// Checks if the current note is within bounds of the note section.
    ///
    /// @returns True if the current element this iterator is pointing to is within the permitted range.
    bool IsValid() const;

    /// Returns the note the iterator is currently pointing to as a reference.
    ///
    /// @warning This may cause an access violation if the iterator is not valid.
    ///
    /// @returns The element the iterator is currently pointing to.
    const NoteTableEntryHeader& GetHeader() const
    {
        PAL_ASSERT(IsValid());
        return *m_pData;
    }

    const void* GetName() const;
    const void* GetDescriptor() const;

    /// Advances the iterator to point to the next element.
    ///
    /// @warning Does not do bounds checking.
    void Next();

private:
    NoteIterator(const Notes& notes, const NoteTableEntryHeader* pData) : m_notes(notes), m_pData(pData) {}

    const Notes&                m_notes;
    const NoteTableEntryHeader* m_pData;

    // Although this is a transgression of coding standards, it means that Notes does not need to have a public
    // interface specifically to implement this class. The added encapsulation this provides is worthwhile.
    friend class Notes;
};

/**
 ***********************************************************************************************************************
 * @brief An iterable wrapper for notes in an ELF note section.
 ***********************************************************************************************************************
 */
class Notes
{
public:
    Notes(const Reader& reader, SectionId section) : m_reader(reader), m_section(section)
    {
        PAL_ASSERT_MSG(m_reader.GetSectionType(m_section) == SectionHeaderType::Note,
            "Expected a note section but got something else");
    }

    const SectionHeader& GetHeader() const { return m_reader.GetSection(m_section); }
    NoteIterator Begin() const;
    NoteIterator End() const;

private:
    const Reader& m_reader;
    const SectionId m_section;
};

/**
 ***********************************************************************************************************************
 * @brief An iterable wrapper for symbols in an ELF symbol table.
 ***********************************************************************************************************************
 */
class Symbols
{
public:
    Symbols(const Reader& reader, SectionId section);

    const SectionHeader& GetHeader() const { return m_reader.GetSection(m_section); }
    SectionId GetStringSection() const { return static_cast<SectionId>(m_reader.GetSection(m_section).sh_link); }
    uint64 GetNumSymbols() const { return GetHeader().sh_size / sizeof(SymbolTableEntry); }

    const SymbolTableEntry& GetSymbol(uint64 i) const;

    const char* GetSymbolName(uint64 i) const { return GetStringSectionData() + GetSymbol(i).st_name; }
    const void* GetSymbolData(uint64 i) const
    {
        return VoidPtrInc(m_reader.GetSectionData(GetSymbol(i).st_shndx),
            static_cast<size_t>(GetSymbol(i).st_value));
    }

    Elf::SymbolTableEntryType GetSymbolType(uint64 i) const
    {
        return static_cast<Elf::SymbolTableEntryType>(GetSymbol(i).st_info.type);
    }

private:
    const char* GetStringSectionData() const
    {
        return static_cast<const char*>(m_reader.GetSectionData(GetStringSection()));
    }

    const Reader& m_reader;
    const SectionId m_section;
};

/**
 ***********************************************************************************************************************
 * @brief An iterable wrapper for symbols in an ELF relocation table.
 ***********************************************************************************************************************
 */
class Relocations
{
public:
    Relocations(const Reader& reader, SectionId section);

    /// If relocations in thi section have an explicit addend.
    bool IsRela() const { return m_reader.GetSectionType(m_section) == SectionHeaderType::Rela; }

    const SectionHeader& GetHeader() const { return m_reader.GetSection(m_section); }
    /// The section where the relocations from this section should be performed.
    SectionId GetDestSection() const { return static_cast<SectionId>(GetHeader().sh_info); }
    /// The symbols which are referenced in the relocations
    SectionId GetSymbolSection() const { return static_cast<SectionId>(GetHeader().sh_link); }

    /// This can be called even if this section is a Rela section
    const RelTableEntry& GetRel(uint64 i) const;

    /// Must only be called if this is a Rela section.
    const RelaTableEntry& GetRela(uint64 i) const;

    Elf::SymbolTableEntryType GetRelocationType(uint64 i) const
    {
        return static_cast<Elf::SymbolTableEntryType>(GetRel(i).r_info.type);
    }

    uint64 GetNumRelocations() const { return m_reader.GetSection(m_section).sh_size / GetEntrySize(); }

private:
    size_t GetEntrySize() const;

    const Reader& m_reader;
    const SectionId m_section;
};

} // ElfReader
} // Util
