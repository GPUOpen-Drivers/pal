/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palElfProcessor.h
 * @brief PAL Elf utility class declarations.
 ***********************************************************************************************************************
 */

#pragma once

#include "palElf.h"
#include "palVectorImpl.h"

namespace Util
{
namespace Elf
{

template <typename Allocator> class Section;
template <typename Allocator> class Segment;
template <typename Allocator> class StringProcessor;

/**
 ***********************************************************************************************************************
 * @brief Creates and stores the ELF sections.
 ***********************************************************************************************************************
 */
template <typename Allocator>
class Sections
{

typedef VectorIterator<Section<Allocator>*, 8, Allocator> SectionVectorIter;
typedef Vector<Section<Allocator>*, 8, Allocator>         SectionVector;

public:
    explicit Sections(Allocator* const pAllocator);
    ~Sections();

    /// Creates a section and returns it with preset values.
    ///
    /// @param [in] type        The type of the section to create from the available standard sections in SectionType.
    /// @param [in] pSectionHdr Non-null for loading from an existing ELF buffer.
    ///
    /// @returns Pointer to newly created section with standard values set for the given SectionType.
    ///          Nullptr is returned if memory allocation fails.
    Section<Allocator>* Add(SectionType type, const SectionHeader* pSectionHdr = nullptr);

    /// Creates a section and returns it with preset values.
    ///
    /// @param [in] type        The type of the section to create from the available standard sections in SectionType.
    /// @param [in] pName       The name of the section to create.
    /// @param [in] pSectionHdr Non-null for loading from an existing ELF buffer.
    ///
    /// @returns Pointer to newly created section with standard values set for the given SectionType
    ///          and custom name.  Nullptr is returned if memory allocation fails.
    Section<Allocator>* Add(SectionType type, const char* pName, const SectionHeader* pSectionHdr = nullptr);

    /// Creates a section and returns it with preset values only if its name is standard.
    ///
    /// @param [in] pName       The name of the section to create.
    /// @param [in] pSectionHdr Non-null for loading from an existing ELF buffer.
    ///
    /// @returns Pointer to newly created section.  Nullptr is returned if memory allocation fails.
    Section<Allocator>* Add(const char* pName, const SectionHeader* pSectionHdr = nullptr);

    /// Gets the Section with the given index.
    ///
    /// @param [in] index The index of the Section.
    ///
    /// @returns Returns the Section with the given index.
    Section<Allocator>* Get(uint32 index)
    {
        PAL_ASSERT(index < m_sectionVector.NumElements());
        return m_sectionVector.At(index);
    }

    /// Gets the const Section with the given index.
    ///
    /// @param [in] index The index of the const Section.
    ///
    /// @returns Returns the const Section with the given index.
    const Section<Allocator>* Get(uint32 index) const
    {
        PAL_ASSERT(index < m_sectionVector.NumElements());
        return m_sectionVector.At(index);
    }

    /// Gets the Section index with the given name.
    ///
    /// @param [in] name Name of the section index to get.
    ///
    /// @returns Returns the Section index with the first matching name.
    uint32 GetSectionIndex(const char* pName) const;

    /// Gets the Section with the given name.  Will return nullptr for the null
    /// section name.
    ///
    /// @param [in] name Name of the section to get.
    ///
    /// @returns Returns the Section with the first matching name.
    Section<Allocator>* Get(const char* pName);

    /// Gets the const Section with the given name.
    ///
    /// @param [in] name Name of the section to get.
    ///
    /// @returns Returns the const Section with the first matching name.
    const Section<Allocator>* Get(const char* pName) const;

    /// Get an iterator at the beginning of the section vector.
    ///
    /// @returns An iterator at the beginning of the section vector.
    SectionVectorIter Begin() const { return m_sectionVector.Begin(); }

    /// Get an iterator at the end of the section vector.
    ///
    /// @returns An iterator at the end of the section vector.
    SectionVectorIter End() const { return m_sectionVector.End(); }

    /// Gets the number of sections that have been added.
    ///
    /// @returns The number of sections added.
    size_t NumSections() const { return m_sectionVector.NumElements(); }

    /// @internal Gets the internally managed ShStrTabSection.
    ///
    /// @returns The ShStrTabSection.
    Section<Allocator>* GetShStrTabSection() const { return m_pShStrTabSection; }

    /// @internal Initializes this set of ELF sections.
    ///
    /// @returns Success if successful, or ErrorOutOfMemory if memory allocations fails.
    Result Init();

private:
    SectionVector  m_sectionVector;

    StringProcessor<Allocator>* m_pStringProcessor;
    Section<Allocator>*         m_pNullSection;
    Section<Allocator>*         m_pShStrTabSection;

    Allocator* const            m_pAllocator;

    PAL_DISALLOW_COPY_AND_ASSIGN(Sections);
};

/**
 ***********************************************************************************************************************
 * @brief An ELF Section
 ***********************************************************************************************************************
 */
template <typename Allocator>
class Section
{
public:
    explicit Section(Allocator* const pAllocator);
    ~Section();

    /// Set the section header type (sh_type)
    ///
    /// @param [in] type The SectionHeaderType.
    void SetType(
        SectionHeaderType type) { m_sectionHeader.sh_type = static_cast<uint32>(type); }

    /// Set the section header flags.
    ///
    /// @param [in] sh_flags The section header flags.
    void SetFlags(uint64 sh_flags) { m_sectionHeader.sh_flags = sh_flags; }

    /// Set the section header address.
    ///
    /// @param [in] sh_addr The section header address.
    void SetAddr(uint64 sh_addr) { m_sectionHeader.sh_addr = sh_addr; }

    /// Set the section link section (sh_link).
    ///
    /// @param [in] pLinkSection Pointer to the section being linked.
    void SetLink(Section<Allocator>* pLinkSection);

    /// Set the section info section (sh_info).
    ///
    /// @param [in] pInfoSection Pointer to the section info.
    void SetInfo(Section<Allocator>* pInfoSection);

    /// Set the section alignment.
    ///
    /// @param [in] sh_addralign section alignment.
    void SetAlignment(uint64 sh_addralign) { m_sectionHeader.sh_addralign = sh_addralign; }

    /// Set the section table entry size if the section is a table with fixed entry sizes.
    ///
    /// @param [in] sh_entsize section table entry size.
    void SetEntrySize(uint64 sh_entsize) { m_sectionHeader.sh_entsize = sh_entsize; }

    /// Set the data of the section.
    ///
    /// @param [in] pData    Pointer to the data to set.
    /// @param [in] dataSize Size in bytes of the data being set.
    ///
    /// @returns  Pointer to the saved data if successful, or nullptr if memory allocation fails.
    void* SetData(const void* pData, size_t dataSize);

    /// Append data to the section.
    ///
    /// @param [in] pData    Pointer to the data to append.
    /// @param [in] dataSize Size in bytes of the data being added.
    ///
    /// @returns  Pointer to the appended data if successful, or nullptr if memory allocation fails.
    void* AppendData(const void* pData, size_t dataSize);

    /// Gets the name of the section.
    ///
    /// @returns Returns the name of the section.
    const char* GetName() const { return m_pName; }

    /// Gets the index of the section.
    ///
    /// @returns Returns the index of the section.
    uint32 GetIndex() const { return m_index; }

    /// Gets the section header type (sh_type)
    ///
    /// @return The SectionHeaderType.
    SectionHeaderType GetType() const { return static_cast<SectionHeaderType>(m_sectionHeader.sh_type); }

    /// Gets the link section of the section.
    ///
    /// @returns Returns the link section of the section.
    Section<Allocator>* GetLink() const { return m_pLinkSection; }

    /// Gets the info section of the section.
    ///
    /// @returns Returns the info section of the section.
    Section<Allocator>* GetInfo() const { return m_pInfoSection; }

    /// Gets the offset of the section. This is not set until a call to
    /// Finalize on the ElfProcessor.
    ///
    /// @returns Returns the offset of the section.
    uint64 GetOffset() const { return m_sectionHeader.sh_offset; }

    /// Gets a pointer to the data of the section.
    ///
    /// @returns Returns a pointer to the data of the section.
    const void* GetData() const { return m_pData; }

    /// Gets the data size of the section.
    ///
    /// @returns Returns the data size of the section.
    size_t GetDataSize() const { return static_cast<size_t>(m_sectionHeader.sh_size); }

    /// Gets the name offset of the section.
    ///
    /// @returns Returns the name offset of the section.
    uint32 GetNameOffset() const { return m_sectionHeader.sh_name; }

    /// Gets the SectionHeader of the section.
    ///
    /// @returns Returns the SectionHeader of the section.
    const SectionHeader* GetSectionHeader() const { return &m_sectionHeader; }

    /// @internal Sets the name of the section.
    ///
    /// @param [in] pName The name of the section.
    void SetName(const char* pName) { m_pName = pName; }

    /// @internal Sets the name offset of the section.
    ///
    /// @param [in] sh_name The name offset of the section.
    void SetNameOffset(uint32 sh_name) { m_sectionHeader.sh_name = sh_name; }

    /// @internal Sets the offset of the section in the ELF file.
    ///
    /// @param [in] sh_offset The offset of the section in the ELF file.
    void SetOffset(size_t sh_offset)
        { m_sectionHeader.sh_offset = static_cast<uint64>(sh_offset); }

    /// @internal Returns a pointer to a block of data appended to the section.  This is useful as otherwise internal
    /// calls to AppendData would be used requiring two allocations and an additional memcpy.
    ///
    /// @param [in] dataSize The size in bytes of the memory to allocate.
    ///
    /// @returns Returns a pointer to this newly allocated memory, or nullptr if memory allocation fails.
    void* AppendUninitializedData(size_t dataSize);

    /// @internal Set the index of the section.
    ///
    /// @param [in] index The index of the section.
    void SetIndex(uint32 index) { m_index = index; }

private:
    uint32              m_index;

    const char*         m_pName;
    void*               m_pData;

    Section<Allocator>* m_pLinkSection;
    Section<Allocator>* m_pInfoSection;

    SectionHeader       m_sectionHeader;

    Allocator* const    m_pAllocator;

    PAL_DISALLOW_COPY_AND_ASSIGN(Section);
};

/**
 ***********************************************************************************************************************
 * @brief Creates and stores the ELF segments.
 ***********************************************************************************************************************
 */
template <typename Allocator>
class Segments
{

typedef VectorIterator<Segment<Allocator>*, 8, Allocator> SegmentVectorIter;
typedef Vector<Segment<Allocator>*, 8, Allocator>         SegmentVector;

public:
    explicit Segments(Allocator* const pAllocator);
    ~Segments();

    /// Creates a Segment and returns it.
    ///
    /// @returns Pointer to newly created segment, or nullptr if memory allocation fails.
    Segment<Allocator>* Add();

    /// Gets the Segment with the given index.
    ///
    /// @param [in] index The index of the Segment.
    ///
    /// @returns Returns the Segment with the given index.
    Segment<Allocator>* Get(uint32 index)
        { PAL_ASSERT(index < m_segmentVector.NumElements()); return m_segmentVector.At(index); }

    /// Gets the Segment at least mapping to the given Section.
    ///
    /// @param [in] pSection The Section the Segment should map with.
    ///
    /// @returns Returns the Segment at least mapping to the given Section.
    Segment<Allocator>* GetWithSection(Section<Allocator>* pSection);

    /// Get an iterator at the beginning of the segment vector.
    ///
    /// @returns An iterator at the beginning of the segment vector.
    SegmentVectorIter Begin() const { return m_segmentVector.Begin(); }

    /// Get an iterator at the end of the segment vector.
    ///
    /// @returns An iterator at the end of the segment vector.
    SegmentVectorIter End() const { return m_segmentVector.End(); }

    /// Gets the number of segments that have been added.
    ///
    /// @returns The number of segments added.
    size_t NumSegments() const { return m_segmentVector.NumElements(); }

    /// Calls Finalize on each segment
    void Finalize();

private:
    SegmentVector    m_segmentVector;

    Allocator* const m_pAllocator;

    PAL_DISALLOW_COPY_AND_ASSIGN(Segments);
};

/**
 ***********************************************************************************************************************
 * @brief An ELF Segment
 ***********************************************************************************************************************
 */
template <typename Allocator>
class Segment
{

typedef VectorIterator<Section<Allocator>*, 8, Allocator> SectionVectorIter;
typedef Vector<Section<Allocator>*, 8, Allocator>         SectionVector;

public:
    explicit Segment(Allocator* const pAllocator);

    /// All Sections added MUST be contigous.
    void AddSection(Section<Allocator>* pSection);

    /// Set the segment type (p_type)
    ///
    /// @param [in] type The SegmentType.
    void SetType(SegmentType type) { m_programHeader.p_type = static_cast<uint32>(type); }

    /// Set the segment flags.
    ///
    /// @param [in] p_flags The segment flags.
    void SetFlags(uint32 p_flags) { m_programHeader.p_flags = p_flags; }

    /// Set the segment virtual address.
    ///
    /// @param [in] p_vaddr The segment virtual address.
    void SetVirtualAddr(uint64 p_vaddr) { m_programHeader.p_vaddr = p_vaddr; }

    /// Set the segment alignment.
    ///
    /// @param [in] p_align The segment alignment.
    void SetAlignment(uint64 p_align) { m_programHeader.p_align = p_align; }

    /// Gets the offset of the segment.
    ///
    /// @returns Returns the offset of the segment.
    uint64 GetOffset() const { return m_programHeader.p_offset; }

    /// Gets the size of the segment.
    ///
    /// @returns Returns the size of the segment.
    uint64 GetSize() const { return m_programHeader.p_filesz; }

    /// Gets the ProgramHeader of the segment.
    ///
    /// @returns Returns the ProgramHeader of the segment.
    const ProgramHeader* GetProgramHeader() const { return &m_programHeader; }

    /// Get an iterator at the beginning of the section vector which contains
    /// all the sections this segment maps to.
    ///
    /// @returns An iterator at the beginning of the section vector
    SectionVectorIter Begin() const { return m_sectionVector.Begin(); }

    /// Get an iterator at the end of the section vector which contains
    /// all the sections this segment maps to.
    ///
    /// @returns An iterator at the end of the section vector.
    SectionVectorIter End() const { return m_sectionVector.End(); }

    /// Finalizes the segment, calculating its offset and size
    void Finalize();

private:
    SectionVector    m_sectionVector;

    ProgramHeader    m_programHeader;

    Allocator* const m_pAllocator;

    PAL_DISALLOW_COPY_AND_ASSIGN(Segment);
};

/**
 ***********************************************************************************************************************
 * @brief Given a note section the NoteProcessor handles adding and getting notes.
 ***********************************************************************************************************************
 */
template <typename Allocator>
class NoteProcessor
{
public:
    NoteProcessor(
        Section<Allocator>* pNoteSection,
        Allocator* const    pAllocator);
    NoteProcessor(
        const Section<Allocator>* pNoteSection,
        Allocator* const          pAllocator);

    /// Initializes the note processor.
    ///
    /// @returns Returns success if initialization was successful, or ErrorOutOfMemory if memory allocation failed.
    Result Init();

    /// Add a note to the NoteSection managed by this NoteProcessor
    ///
    /// @param [in] type     The type of note.
    /// @param [in] pName    The name of the note.
    /// @param [in] pDesc    The description (contents) of the note.
    /// @param [in] descSize The size of the description in bytes.
    ///
    /// @returns Returns the index of the note in the NoteSection, or UINT_MAX if memory allocation failed.
    uint32 Add(
        uint32      type,
        const char* pName,
        const void* pDesc,
        size_t      descSize);

    /// Get a note from the NoteSection.
    ///
    /// @param [in]  index    The index of the note in the NoteSection.
    /// @param [out] pType     The type of note.
    /// @param [out] ppName    The name of the note.
    /// @param [out] ppDesc    The description (contents) of the note.
    /// @param [out] pDescSize The size of the description in bytes.
    void Get(
        uint32       index,
        uint32*      pType,
        const char** ppName,
        const void** ppDesc,
        size_t*      pDescSize) const;

    /// Gets the number of notes in the NoteSection.
    ///
    /// @returns Returns the number of notes added.
    uint32 GetNumNotes() const { return m_noteVector.NumElements(); }

private:
    Section<Allocator>*          m_pNoteSection;
    Vector<size_t, 8, Allocator> m_noteVector;

    Allocator* const             m_pAllocator;

    PAL_DISALLOW_COPY_AND_ASSIGN(NoteProcessor);
};

/// The size of a SymbolTableEntry in bytes.
constexpr size_t SymbolTableEntrySize = sizeof(SymbolTableEntry);

/**
 ***********************************************************************************************************************
 * @brief Given a symbol section the SymbolProcessor handles adding and getting symbols.
 ***********************************************************************************************************************
 */
template <typename Allocator>
class SymbolProcessor
{
public:
    SymbolProcessor(
        Section<Allocator>*         pSymbolSection,
        StringProcessor<Allocator>* pStringProcessor,
        Allocator* const            pAllocator);

    SymbolProcessor(
        const Section<Allocator>*         pSymbolSection,
        const StringProcessor<Allocator>* pStringProcessor,
        Allocator* const                  pAllocator);

    /// Add a symbol to the SymbolSection managed by this SymbolProcessor
    ///
    /// @param [in] pName        The name of the symbol.
    /// @param [in] binding      The binding of the symbol.
    /// @param [in] type         The type of symbol.
    /// @param [in] sectionIndex The section index associated with the symbol.
    /// @param [in] value        The value of the symbol.
    /// @param [in] size         The size associated with the symbol.
    ///
    /// @returns Returns the index of the symbol in the SymbolSection, or UINT_MAX if memory allocation failed.
    uint32 Add(
        const char*             pName,
        SymbolTableEntryBinding binding,
        SymbolTableEntryType    type,
        uint16                  sectionIndex,
        uint64                  value,
        uint64                  size);

    /// Get a symbol from the SymbolSection.
    ///
    /// @param [in]  index        The index of the symbol.
    /// @param [out] ppName         The name of the symbol.
    /// @param [out] pBinding      The binding of the symbol.
    /// @param [out] pType         The type of symbol.
    /// @param [out] pSectionIndex The section index associated with the symbol.
    /// @param [out] pValue        The value of the symbol.
    /// @param [out] pSize         The size associated with the symbol.
    void Get(
        uint32                   index,
        const char**             ppName,
        SymbolTableEntryBinding* pBinding,
        SymbolTableEntryType*    pType,
        uint16*                  pSectionIndex,
        uint64*                  pValue,
        uint64*                  pSize) const;

    /// Gets the number of symbols in the SymbolSection.
    ///
    /// @returns Returns the number of symbols added.
    uint32 GetNumSymbols() const
        { return static_cast<uint32>(m_pSymbolSection->GetDataSize() / SymbolTableEntrySize); }

private:
    void Init();

    Section<Allocator>*         m_pSymbolSection;
    StringProcessor<Allocator>* m_pStringProcessor;

    Allocator* const            m_pAllocator;

    PAL_DISALLOW_COPY_AND_ASSIGN(SymbolProcessor);
};

/**
 ***********************************************************************************************************************
 * @brief Given a relocation section the RelocationProcessor handles adding and getting relocations.
 ***********************************************************************************************************************
 */
template <typename Allocator>
class RelocationProcessor
{
public:
    explicit RelocationProcessor(Section<Allocator>* pReloactionSection);
    explicit RelocationProcessor(const Section<Allocator>* pReloactionSection);
    ~RelocationProcessor() { }

    /// Add a relocation to the RelocationSection managed by this RelocationProcessor
    ///
    /// @param [in] offset      The offset of the relocation in the section.
    /// @param [in] symbolIndex The index of the associated symbol.
    /// @param [in] type        The type of relocation.
    ///
    /// @returns Returns the index of the relocation in the RelocationSection.  UINT_MAX is returned if memory
    ///          allocation fails.
    uint32 Add(
        uint64 offset,
        uint32 symbolIndex,
        uint32 type);

    /// Add a relocation to the RelocationSection managed by this RelocationProcessor
    ///
    /// @param [in] offset      The offset of the relocation in the section.
    /// @param [in] symbolIndex The index of the associated symbol.
    /// @param [in] type        The type of relocation.
    /// @param [in] addend      The addend for the relocation.
    ///
    /// @returns Returns the index of the relocation in the RelocationSection.  UINT_MAX is returned if memory
    ///          allocation fails.
    uint32 Add(
        uint64 offset,
        uint32 symbolIndex,
        uint32 type,
        uint64 addend);

    /// Get a relocation from the RelocationSection managed by this RelocationProcessor
    ///
    /// @param [in]  index        The index of the relocation in the section.
    /// @param [out] pOffset      The offset of the relocation in the section.
    /// @param [out] pSymbolIndex The index of the associated symbol.
    /// @param [out] pType        The type of relocation.
    /// @param [out] pAddend      The addend for the relocation.
    void Get(
        uint32  index,
        uint64* pOffset,
        uint32* pSymbolIndex,
        uint32* pType,
        uint64* pAddend) const;

    /// Gets the number of relocations in the RelocationSection.
    ///
    /// @returns Returns the number of relocations.
    uint32 GetNumRelocations() const;

private:
    void GetRelEntry(
        uint32  index,
        uint64* pOffset,
        uint32* pSymbolIndex,
        uint32* pType) const;

    void GetRelaEntry(
        uint32  index,
        uint64* pOffset,
        uint32* pSymbolIndex,
        uint32* pType,
        uint64* pAddend) const;

    Section<Allocator>*const  m_pRelocationSection;

    PAL_DISALLOW_COPY_AND_ASSIGN(RelocationProcessor);
};

/**
 ***********************************************************************************************************************
 * @brief Given a string table section the StringProcessor handles adding and getting strings.
 ***********************************************************************************************************************
 */
template <typename Allocator>
class StringProcessor
{
public:
    StringProcessor(
        Section<Allocator>* pStrTabSection,
        Allocator* const    pAllocator);
    StringProcessor(
        const Section<Allocator>* pStrTabSection,
        Allocator* const          pAllocator);

    /// Add a string to the StrTabSection managed by this StringProcessor
    ///
    /// @param [in] pString String to add to the StrTabSection.
    ///
    /// @returns Returns the offset of the string in the StrTabSection.  If memory allocation fails, the index of the
    ///          dummy empty string is returned.
    uint32 Add(const char* pString);

    /// Get a string from the StrTabSection managed by this StringProcessor
    ///
    /// @param [in] offset Offset of the string to get from the StrTabSection.
    ///
    /// @returns Returns the string found at the given offset.
    const char* Get(uint32 offset) const;

    /// Gets the number of strings in the StrTabSection.
    ///
    /// @returns Returns the number of strings added.
    uint32 GetNumStrings() const;

private:
    Section<Allocator>*const  m_pStrTabSection;
    Allocator* const          m_pAllocator;

    PAL_DISALLOW_COPY_AND_ASSIGN(StringProcessor);
};

/**
 ***********************************************************************************************************************
 * @brief The ElfProcessor manages the elf header and loads and saves the ELF Object to a buffer.
 ***********************************************************************************************************************
 */
template <typename Allocator>
class ElfProcessor
{
public:
    explicit ElfProcessor(Allocator* const pAllocator);

    /// Set the ELF class.  This ELF processor only supports 64bit ELF files.
    ///
    /// @param [in] ei_class The Elf::IdentClass specifying a 32 or 64 bit ELF.
    void SetClass(Elf::IdentClass ei_class)
    {
        PAL_ASSERT(ei_class == Elf::ElfClass64);
        m_fileHeader.ei_class = static_cast<uint8>(ei_class);
    }

    /// Set the ELF endianness.  This ELF processor only supports little endian.
    ///
    /// @param [in] endianness Elf::IdentEndianness specifying little or big endian.
    void SetEndianness(Elf::IdentEndianness endianness)
    {
        PAL_ASSERT(endianness == Elf::ElfLittleEndian);
        m_fileHeader.ei_data = static_cast<uint8>(endianness);
    }

    /// Set the version.
    ///
    /// @param [in] version ELF version.
    void SetVersion(uint8 version) { m_fileHeader.ei_version = version; }

    /// Set the OS ABI.
    ///
    /// @param [in] osAbi ELF OS ABI.
    void SetOsAbi(uint8 osAbi) { m_fileHeader.ei_osabi = osAbi; }

    /// Set the ABI Version.
    ///
    /// @param [in] abiVersion ELF ABI Version.
    void SetAbiVersion(uint8 abiVersion) { m_fileHeader.ei_abiversion = abiVersion; }

    /// Set the ELF type.
    ///
    /// @param [in] type ELF ObjectFileType.
    void SetObjectFileType(ObjectFileType type)
        { m_fileHeader.e_type = static_cast<uint16>(type); }

    /// Set the ELF machine.
    ///
    /// @param [in] machine ELF MachineType.
    void SetTargetMachine(MachineType machine)
        { m_fileHeader.e_machine = static_cast<uint16>(machine); }

    /// Set the ELF entry.
    ///
    /// @param [in] entry ELF entry point.
    void SetEntryPoint(uint64 entry) { m_fileHeader.e_entry = entry; }

    /// Set the ELF flags.
    ///
    /// @param [in] flags ELF flags.
    void SetFlags(uint32 flags) { m_fileHeader.e_flags = flags; }

    /// Gets the ELF Class.
    ///
    /// @returns Returns the Elf::IdentClass.
    Elf::IdentClass GetClass() const
        { return static_cast<Elf::IdentClass>(m_fileHeader.ei_class); }

    /// Gets the ELF Endianness.
    ///
    /// @returns Returns the Elf::IdentEndianness.
    Elf::IdentEndianness GetEndianness() const
        { return static_cast<Elf::IdentEndianness>(m_fileHeader.ei_data); }

    /// Gets the ELF object file type.
    ///
    /// @returns Returns the ELF ObjectFileType.
    ObjectFileType GetObjectFileType() const
        { return static_cast<ObjectFileType>(m_fileHeader.e_type); }

    /// Gets the ELF target machine.
    ///
    /// @returns Returns the ELF MachineType.
    MachineType GetTargetMachine() const
        { return static_cast<MachineType>(m_fileHeader.e_machine); }

    /// Gets the FileHeader of the ELF file.
    ///
    /// @returns Returns the FileHeader of the ELF file.
    const FileHeader* GetFileHeader() const { return &m_fileHeader; }

    /// Gets the Sections Object to get and add sections.
    ///
    /// @returns Returns the Sections Object.
    Sections<Allocator>* GetSections() { return &m_sections; }

    /// Gets the Sections Object to get sections.
    ///
    /// @returns Returns the Sections Object.
    const Sections<Allocator>* GetSections() const { return &m_sections; }

    /// Gets the Segments Object to get and add segments.
    ///
    /// @returns Returns the Segments Object.
    Segments<Allocator>* GetSegments() { return &m_segments; }

    /// Gets the Segments Object to get segments.
    ///
    /// @returns Returns the Segments Object.
    const Segments<Allocator>* GetSegments() const { return &m_segments; }

    /// Finalize the ELF by calculating sizes and offsets.
    void Finalize();

    /// Gets the number of bytes required to hold a binary blob of the ELF.
    ///
    /// @returns Returns the size of the ELF in bytes.
    size_t GetRequiredBufferSizeBytes() const;

    /// Save the ELF to a buffer.
    ///
    /// @param [in] pBuffer Pointer to the buffer to save to.
    void SaveToBuffer(void* pBuffer);

    /// Initialize the ELF processor before generating a new ELF.
    ///
    /// @returns Success if successful, or ErrorOutOfMemory upon allocation failure.
    Result Init();

    /// Load the ELF from a buffer.
    ///
    /// @param [in] pBuffer    Pointer to the buffer to load from.
    /// @param [in] bufferSize Size of the buffer in bytes to load from.
    ///
    /// @returns Success if successful, or ErrorOutOfMemory upon allocation failure.
    Result LoadFromBuffer(const void* pBuffer, size_t bufferSize);

private:
    FileHeader          m_fileHeader;
    Sections<Allocator> m_sections;
    Segments<Allocator> m_segments;

    Allocator* const    m_pAllocator;
};

} // Elf
} // Util
