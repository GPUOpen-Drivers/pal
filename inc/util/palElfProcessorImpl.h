/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palElfProcessorImpl.h
 * @brief PAL Elf utility class implementations.
 ***********************************************************************************************************************
 */

#pragma once

#include "palElfProcessor.h"
#include "palHashMapImpl.h"
#include "palListImpl.h"

#include <limits.h>

namespace Util
{
namespace Elf
{

constexpr size_t FileHeaderSize           = sizeof(FileHeader);
constexpr size_t ProgramHeaderSize        = sizeof(ProgramHeader);
constexpr size_t SectionHeaderSize        = sizeof(SectionHeader);
constexpr size_t NoteTableEntryHeaderSize = sizeof(NoteTableEntryHeader);
constexpr size_t RelTableEntrySize        = sizeof(RelTableEntry);
constexpr size_t RelaTableEntrySize       = sizeof(RelaTableEntry);

// LLVM's ELF reader requires 4 byte alignment when processing Section Headers.
constexpr size_t SectionHeaderAlignment   = 4;

// 8 byte alignment for notes according to Elf64 spec.
// However, in practice it is 4 bytes (readelf gets confused by 8)
constexpr size_t NoteAlignment = 4;

// According to the Elf64 spec the namesz should not include the null
// terminator. However, in practice it is included. Set
// NoteNameNullTerminatorByte to 1 to match the spec.
constexpr size_t NoteNameNullTerminatorByte = 0;

// =====================================================================================================================
template <typename Allocator>
Sections<Allocator>::Sections(
    Allocator* const pAllocator)
    :
    m_sectionVector(pAllocator),
    m_pStringProcessor(nullptr),
    m_pNullSection(nullptr),
    m_pShStrTabSection(nullptr),
    m_pAllocator(pAllocator)
{
}

// =====================================================================================================================
template <typename Allocator>
Result Sections<Allocator>::Init()
{
    Result result = Result::ErrorOutOfMemory;

    // NOTE: Both sections are not added to the Sections until another section is added!
    m_pNullSection     = PAL_NEW(Section<Allocator>, m_pAllocator, AllocInternalTemp)(m_pAllocator);
    m_pShStrTabSection = PAL_NEW(Section<Allocator>, m_pAllocator, AllocInternalTemp)(m_pAllocator);

    if ((m_pNullSection != nullptr) && (m_pShStrTabSection != nullptr))
    {
        m_pNullSection->SetName("");

        constexpr uint32 SectionTypeIndex = static_cast<uint32>(SectionType::ShStrTab);
        m_pShStrTabSection->SetType(SectionHeaderInfoTable[SectionTypeIndex].type);
        m_pShStrTabSection->SetFlags(SectionHeaderInfoTable[SectionTypeIndex].flags);
        m_pShStrTabSection->SetName(SectionNameStringTable[SectionTypeIndex]);

        m_pStringProcessor =
            PAL_NEW(StringProcessor<Allocator>, m_pAllocator, AllocInternalTemp)(m_pShStrTabSection, m_pAllocator);
        if (m_pStringProcessor != nullptr)
        {
            uint32 nameOffset = m_pStringProcessor->Add("");
            m_pNullSection->SetNameOffset(nameOffset);

            if (nameOffset != UINT_MAX)
            {
                nameOffset = m_pStringProcessor->Add(m_pShStrTabSection->GetName());
                m_pShStrTabSection->SetNameOffset(nameOffset);
            }

            if (nameOffset != UINT_MAX)
            {
                result = Result::Success;
            }
        }
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
Sections<Allocator>::~Sections()
{
    // If no sections were added we need to free these separately as they are not in the vector.
    if (m_sectionVector.IsEmpty())
    {
        PAL_SAFE_DELETE(m_pShStrTabSection, m_pAllocator);
        PAL_SAFE_DELETE(m_pNullSection, m_pAllocator);
    }
    else
    {
        while (!m_sectionVector.IsEmpty())
        {
            Section<Allocator>* pSection = nullptr;
            m_sectionVector.PopBack(&pSection);
            PAL_SAFE_DELETE(pSection, m_pAllocator);
        }
    }

    PAL_SAFE_DELETE(m_pStringProcessor, m_pAllocator);
}

// =====================================================================================================================
template <typename Allocator>
Section<Allocator>* Sections<Allocator>::Add(
    SectionType          type,
    const SectionHeader* pSectionHdr)
{
    return Add(type, SectionNameStringTable[static_cast<uint32>(type)], pSectionHdr);
}

// =====================================================================================================================
template <typename Allocator>
Section<Allocator>* Sections<Allocator>::Add(
    SectionType          type,
    const char*          pName,
    const SectionHeader* pSectionHdr)
{
    PAL_ASSERT((type >= SectionType::Null)  &&
               (type <  SectionType::Count) &&
               (type != SectionType::ShStrTab));

    Result result = Result::Success;
    if (m_sectionVector.NumElements() == 0)
    {
        m_pNullSection->SetIndex(0);
        m_sectionVector.PushBack(m_pNullSection);

        m_pShStrTabSection->SetIndex(1);
        m_sectionVector.PushBack(m_pShStrTabSection);

        if (m_sectionVector.NumElements() != 2)
        {
            m_sectionVector.Clear();
            result = Result::ErrorOutOfMemory;
        }
    }

    Section<Allocator>* pSection = nullptr;

    if (result == Result::Success)
    {
        const uint32 sectionTypeIndex = static_cast<uint32>(type);

        pSection = PAL_NEW(Section<Allocator>, m_pAllocator, AllocInternalTemp)(m_pAllocator);

        if (pSection != nullptr)
        {
            const uint32 nameOffset = (pSectionHdr == nullptr) ? m_pStringProcessor->Add(pName) : pSectionHdr->sh_name;
            if (nameOffset == 0)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                pSection->SetName(pName);
                pSection->SetNameOffset(nameOffset);
                pSection->SetType(SectionHeaderInfoTable[sectionTypeIndex].type);
                pSection->SetFlags(SectionHeaderInfoTable[sectionTypeIndex].flags);

                switch (SectionHeaderInfoTable[sectionTypeIndex].type)
                {
                case SectionHeaderType::SymTab:
                    pSection->SetEntrySize(SymbolTableEntrySize);
                    break;
                case SectionHeaderType::Rel:
                    pSection->SetEntrySize(RelTableEntrySize);
                    break;
                case SectionHeaderType::Rela:
                    pSection->SetEntrySize(RelaTableEntrySize);
                    break;
                default:
                    break;
                }

                pSection->SetIndex(m_sectionVector.NumElements());
                result = m_sectionVector.PushBack(pSection);
            }

            if (result != Result::Success)
            {
                PAL_SAFE_DELETE(pSection, m_pAllocator);
            }
        }
    }

    return pSection;
}

// =====================================================================================================================
template <typename Allocator>
Section<Allocator>* Sections<Allocator>::Add(
    const char*          pName,
    const SectionHeader* pSectionHdr)
{
    Result result = Result::Success;
    if (m_sectionVector.NumElements() == 0)
    {
        m_pNullSection->SetIndex(0);
        m_sectionVector.PushBack(m_pNullSection);

        m_pShStrTabSection->SetIndex(1);
        m_sectionVector.PushBack(m_pShStrTabSection);

        if (m_sectionVector.NumElements() != 2)
        {
            m_sectionVector.Clear();
            result = Result::ErrorOutOfMemory;
        }
    }

    Section<Allocator>* pSection = nullptr;

    if (result == Result::Success)
    {
        for (uint32 i = 0; i < static_cast<uint32>(SectionType::Count); i++)
        {
            if (strcmp(SectionNameStringTable[i], pName) == 0)
            {
                // Match found from standard sections.
                pSection = Add(static_cast<SectionType>(i), pSectionHdr);
                break;
            }
        }

        // No match found, custom Section
        if (pSection == nullptr)
        {
            pSection = PAL_NEW(Section<Allocator>, m_pAllocator, AllocInternalTemp)(m_pAllocator);
            if (pSection != nullptr)
            {
                const uint32 nameOffset = m_pStringProcessor->Add(pName);
                if (nameOffset == 0)
                {
                    result = Result::ErrorOutOfMemory;
                }
                else
                {
                    pSection->SetNameOffset(nameOffset);
                    pSection->SetName(pName);
                    pSection->SetIndex(m_sectionVector.NumElements());
                    result = m_sectionVector.PushBack(pSection);
                }

                if (result != Result::Success)
                {
                    PAL_SAFE_DELETE(pSection, m_pAllocator);
                }
            }
        }
    }

    return pSection;
}

// =====================================================================================================================
template <typename Allocator>
uint32 Sections<Allocator>::GetSectionIndex(
    const char* pName
    ) const
{
    uint32 sectionIndex = 0;

    auto sectionIterator = m_sectionVector.Begin();
    while (sectionIterator.IsValid())
    {
        Section<Allocator>* pSectionToMatch = sectionIterator.Get();

        if (strcmp(pName, pSectionToMatch->GetName()) == 0)
        {
            sectionIndex = pSectionToMatch->GetIndex();
            break;
        }

        sectionIterator.Next();
    }

    return sectionIndex;
}

// =====================================================================================================================
template <typename Allocator>
Section<Allocator>* Sections<Allocator>::Get(
    const char* pName)
{
    Section<Allocator>* pSection = Get(GetSectionIndex(pName));
    return (pSection->GetIndex() == 0) ? nullptr : pSection;
}

// =====================================================================================================================
template <typename Allocator>
const Section<Allocator>* Sections<Allocator>::Get(
    const char* pName
    ) const
{
    const Section<Allocator>* pSection = Get(GetSectionIndex(pName));
    return (pSection->GetIndex() == 0) ? nullptr : pSection;
}

// =====================================================================================================================
template <typename Allocator>
Section<Allocator>::Section(
    Allocator* const pAllocator)
    :
    m_index(0),
    m_pName(nullptr),
    m_pData(nullptr),
    m_pLinkSection(nullptr),
    m_pInfoSection(nullptr),
    m_sectionHeader(),
    m_pAllocator(pAllocator)
{
}

// =====================================================================================================================
template <typename Allocator>
Section<Allocator>::~Section()
{
    PAL_SAFE_FREE(m_pData, m_pAllocator);
}

// =====================================================================================================================
template <typename Allocator>
void Section<Allocator>::SetLink(
    Section<Allocator>* pLinkSection)
{
    PAL_ASSERT(pLinkSection != nullptr);

    m_pLinkSection = pLinkSection;
    m_sectionHeader.sh_link = m_pLinkSection->GetIndex();
}

// =====================================================================================================================
template <typename Allocator>
void Section<Allocator>::SetInfo(
    Section<Allocator>* pInfoSection)
{
    PAL_ASSERT(pInfoSection != nullptr);

    m_pInfoSection = pInfoSection;
    m_sectionHeader.sh_info = m_pInfoSection->GetIndex();
}

// =====================================================================================================================
template <typename Allocator>
void* Section<Allocator>::SetData(
    const void* pData,
    size_t      dataSize)
{
    PAL_ASSERT((pData != nullptr) || ((pData == nullptr) && (dataSize == 0)));

    void* pNewData = PAL_MALLOC(dataSize, m_pAllocator, AllocInternalTemp);
    if (pNewData != nullptr)
    {
        if (m_pData != nullptr)
        {
            PAL_SAFE_FREE(m_pData, m_pAllocator);
        }

        memcpy(pNewData, pData, dataSize);
        m_pData = pNewData;
        m_sectionHeader.sh_size = dataSize;
    }
    // NOTE: If memory allocation fails, no state will be changed, and nullptr is returned.

    return pNewData;
}

// =====================================================================================================================
template <typename Allocator>
void* Section<Allocator>::AppendData(
    const void* pData,
    size_t      dataSize)
{
    PAL_ASSERT((pData != nullptr) && (dataSize != 0));

    void*const pAppendData = this->AppendUninitializedData(dataSize);
    if (pAppendData != nullptr)
    {
        memcpy(pAppendData, pData, dataSize);
    }

    return pAppendData;
}

// =====================================================================================================================
template <typename Allocator>
void* Section<Allocator>::AppendUninitializedData(
    size_t dataSize)
{
    PAL_ASSERT(dataSize != 0);

    const size_t newDataSize = (GetDataSize() + dataSize);
    void*        pNewData    = PAL_MALLOC(newDataSize, m_pAllocator, AllocInternalTemp);
    void*        pAppendData = nullptr;

    if (pNewData != nullptr)
    {
        pAppendData = VoidPtrInc(pNewData, GetDataSize());

        if (m_pData != nullptr)
        {
            memcpy(pNewData, m_pData, GetDataSize());
            PAL_SAFE_FREE(m_pData, m_pAllocator);
        }

        m_pData = pNewData;
        m_sectionHeader.sh_size = newDataSize;
    }
    // NOTE: If memory allocation fails, no state will be changed, and nullptr is returned.

    return pAppendData;
}

// =====================================================================================================================
template <typename Allocator>
Segments<Allocator>::Segments(
    Allocator* const pAllocator)
    :
    m_segmentVector(pAllocator),
    m_pAllocator(pAllocator)
{
}

// =====================================================================================================================
template <typename Allocator>
Segments<Allocator>::~Segments()
{
    while (!m_segmentVector.IsEmpty())
    {
        Segment<Allocator>* pSegment;
        m_segmentVector.PopBack(&pSegment);
        PAL_SAFE_DELETE(pSegment, m_pAllocator);
    }
}

// =====================================================================================================================
template <typename Allocator>
Segment<Allocator>* Segments<Allocator>::Add()
{
    Segment<Allocator>* pSegment = PAL_NEW(Segment<Allocator>, m_pAllocator, AllocInternalTemp)(m_pAllocator);
    if (pSegment != nullptr)
    {
        if (Result::Success != m_segmentVector.PushBack(pSegment))
        {
            PAL_SAFE_DELETE(pSegment, m_pAllocator);
        }
    }

    return pSegment;
}

// =====================================================================================================================
// It is hard to identify segments as segments don't have names.  They only have an index and
// sections that they are mapped with.  This helper returns the first segment (as there can be
// multiple) that maps with the given section.
template <typename Allocator>
Segment<Allocator>* Segments<Allocator>::GetWithSection(
    Section<Allocator>* pSection)
{
    Segment<Allocator>* pSegment = nullptr;

    auto segmentIterator = m_segmentVector.Begin();
    while (segmentIterator.IsValid())
    {
        Segment<Allocator>* pSegmentElement = segmentIterator.Get();

        auto sectionIterator = pSegmentElement->Begin();
        while (sectionIterator.IsValid())
        {
            const Section<Allocator>* pSectionElement = sectionIterator.Get();
            if (pSection == pSectionElement)
            {
                pSegment = pSegmentElement;
                break;
            }

            sectionIterator.Next();
        }

        segmentIterator.Next();
    }

    return pSegment;
}

// =====================================================================================================================
template <typename Allocator>
void Segments<Allocator>::Finalize()
{
    // Iterate through the segment vector to calculate size / offsets.

    auto segmentIterator = m_segmentVector.Begin();
    while (segmentIterator.IsValid())
    {
        Segment<Allocator>* pSegment = segmentIterator.Get();

        pSegment->Finalize();

        segmentIterator.Next();
    }
}

// =====================================================================================================================
template <typename Allocator>
Segment<Allocator>::Segment(
    Allocator* const pAllocator)
    :
    m_sectionVector(pAllocator),
    m_programHeader(),
    m_pAllocator(pAllocator)
{
}

// =====================================================================================================================
template <typename Allocator>
void Segment<Allocator>::AddSection(
    Section<Allocator>* pSection)
{
    PAL_ASSERT(pSection != nullptr);

    // Ensure that the sections are contiguous.
    if (!m_sectionVector.IsEmpty())
    {
        Section<Allocator>* prevSection = m_sectionVector.Back();
        PAL_ASSERT(prevSection->GetIndex() == (pSection->GetIndex() - 1));
    }

    m_sectionVector.PushBack(pSection);
}

// =====================================================================================================================
template <typename Allocator>
void Segment<Allocator>::Finalize()
{
    if (m_sectionVector.NumElements() > 0)
    {
        m_programHeader.p_offset = m_sectionVector.At(0)->GetOffset();

        size_t segmentSize = 0;

        // Iterate through the section vector to determine the size of the segment.
        auto sectionIterator = m_sectionVector.Begin();
        while (sectionIterator.IsValid())
        {
            const Section<Allocator>* pSection = sectionIterator.Get();

            segmentSize +=  pSection->GetDataSize();

            sectionIterator.Next();
        }

        m_programHeader.p_memsz = static_cast<uint64>(segmentSize);
        m_programHeader.p_filesz = static_cast<uint64>(segmentSize);
    }
}

// =====================================================================================================================
template <typename Allocator>
NoteProcessor<Allocator>::NoteProcessor(
    Section<Allocator>* pNoteSection,
    Allocator* const    pAllocator)
    :
    m_pNoteSection(pNoteSection),
    m_noteVector(pAllocator),
    m_pAllocator(pAllocator)
{
}

// =====================================================================================================================
template <typename Allocator>
NoteProcessor<Allocator>::NoteProcessor(
    const Section<Allocator>* pNoteSection,
    Allocator* const          pAllocator)
    :
    m_pNoteSection(const_cast<Section<Allocator>*>(pNoteSection)),
    m_noteVector(pAllocator),
    m_pAllocator(pAllocator)
{
}

// =====================================================================================================================
template <typename Allocator>
Result NoteProcessor<Allocator>::Init()
{
    Result result = Result::Success;

    // NOTE: Note Data can be externally manipulated by working directly with the note section.  This would mean the
    // note indices managed by this note processor would become invalid.  Is that acceptable?

    // Find existing notes and save their offsets.
    const size_t dataSize = m_pNoteSection->GetDataSize();

    if (dataSize > 0)
    {
        const void* pData = m_pNoteSection->GetData();

        const uint8* pNoteReader = static_cast<const uint8*>(pData);

        while (VoidPtrDiff(pNoteReader, pData) < dataSize)
        {
            result = m_noteVector.PushBack(VoidPtrDiff(pNoteReader, pData));
            if (result != Result::Success)
            {
                break;
            }

            const NoteTableEntryHeader*const pNoteHeader =
                reinterpret_cast<const NoteTableEntryHeader*>(pNoteReader);

            size_t noteSize = NoteTableEntryHeaderSize;

            noteSize += pNoteHeader->n_namesz + NoteNameNullTerminatorByte;
            noteSize = RoundUpToMultiple(noteSize, NoteAlignment);

            noteSize += pNoteHeader->n_descsz;
            noteSize = RoundUpToMultiple(noteSize, NoteAlignment);

            pNoteReader += noteSize;
        }
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
uint32 NoteProcessor<Allocator>::Add(
    uint32      type,
    const char* pName,
    const void* pDesc,
    size_t      descSize)
{
    PAL_ASSERT((pName != nullptr) && ((pDesc != nullptr) || (descSize == 0)));

    uint32 index = UINT_MAX; // Assume failure until otherwise knnown.

    // Size for namesz, descsz, and type
    size_t noteSize = NoteTableEntryHeaderSize;

    const size_t nameSize = strlen(pName);
    noteSize += nameSize + 1; // Account for the terminating null.

    const size_t namePaddingSize = RoundUpToMultiple(noteSize, NoteAlignment) - noteSize;
    noteSize += namePaddingSize;

    noteSize += descSize;

    const size_t descPaddingSize = RoundUpToMultiple(noteSize, NoteAlignment) - noteSize;
    noteSize += descPaddingSize;

    NoteTableEntryHeader header = { };
    // The length does not include the terminating null or the padding
    // If NoteNameNullTerminatorByte == 0 then we are not matching the spec (add 1 to nameSize)
    header.n_namesz = static_cast<uint32>(nameSize + (NoteNameNullTerminatorByte ? 0 : 1));
    header.n_descsz = static_cast<uint32>(descSize);
    header.n_type   = type;

    void*const pData = m_pNoteSection->AppendUninitializedData(noteSize);
    if (pData != nullptr)
    {
        memcpy(pData, &header, sizeof(header));
        void* pRunner = VoidPtrInc(pData, sizeof(header));

        // Copy over the name, accounting for the null terminator.
        memcpy(pRunner, pName, (nameSize + 1));
        pRunner = VoidPtrInc(pRunner, (nameSize + 1));

        // Fill in some alignment padding.
        memset(pRunner, 0, namePaddingSize);
        pRunner = VoidPtrInc(pRunner, namePaddingSize);

        // Copy over the desc data.
        memcpy(pRunner, pDesc, descSize);
        pRunner = VoidPtrInc(pRunner, descSize);

        // Fill in some alignment padding for the next note.
        memset(pRunner, 0, descPaddingSize);
        pRunner = VoidPtrInc(pRunner, descPaddingSize);

        PAL_ASSERT(VoidPtrDiff(pRunner, pData) == noteSize); // Guard against overruns.

        if (m_noteVector.PushBack(VoidPtrDiff(pData, m_pNoteSection->GetData())) == Result::Success)
        {
            index = (m_noteVector.NumElements() - 1);
        }
    }

    return index;
}

// =====================================================================================================================
template <typename Allocator>
void NoteProcessor<Allocator>::Get(
    uint32       index,
    uint32*      pType,
    const char** ppName,
    const void** ppDesc,
    size_t*      pDescSize
    ) const
{
    PAL_ASSERT((index >= 0) && (index < m_noteVector.NumElements()));

    const uint8* pNoteReader = static_cast<const uint8*>(m_pNoteSection->GetData())
        + m_noteVector.At(index);

    const NoteTableEntryHeader* pNoteHeader =
        reinterpret_cast<const NoteTableEntryHeader*>(pNoteReader);

    *pDescSize = pNoteHeader->n_descsz;
    *pType     = pNoteHeader->n_type;

    pNoteReader += sizeof(NoteTableEntryHeader);

    *ppName = reinterpret_cast<const char*>(pNoteReader);

    // Add null terminator to name size and account for alignment padding.
    pNoteReader += RoundUpToMultiple(
        static_cast<size_t>(pNoteHeader->n_namesz + NoteNameNullTerminatorByte), NoteAlignment);

    *ppDesc = pNoteReader;
}

// =====================================================================================================================
template <typename Allocator>
SymbolProcessor<Allocator>::SymbolProcessor(
    Section<Allocator>*         pSymbolSection,
    StringProcessor<Allocator>* pStringProcessor,
    Allocator* const            pAllocator)
    :
    m_pSymbolSection(pSymbolSection),
    m_pStringProcessor(pStringProcessor),
    m_pAllocator(pAllocator)
{
    Init();
}

// =====================================================================================================================
template <typename Allocator>
SymbolProcessor<Allocator>::SymbolProcessor(
    const Section<Allocator>*         pSymbolSection,
    const StringProcessor<Allocator>* pStringProcessor,
    Allocator* const                  pAllocator)
    :
    m_pSymbolSection(const_cast<Section<Allocator>*>(pSymbolSection)),
    m_pStringProcessor(const_cast<StringProcessor<Allocator>*>(pStringProcessor)),
    m_pAllocator(pAllocator)
{
    Init();
}

// =====================================================================================================================
template <typename Allocator>
void SymbolProcessor<Allocator>::Init()
{
    if(m_pSymbolSection->GetDataSize() == 0)
    {
        // Create the null symbol entry.
        Add("", SymbolTableEntryBinding::Local, SymbolTableEntryType::None, 0, 0, 0);
    }
}

// =====================================================================================================================
template <typename Allocator>
uint32 SymbolProcessor<Allocator>::Add(
    const char*             pName,
    SymbolTableEntryBinding binding,
    SymbolTableEntryType    type,
    uint16                  sectionIndex,
    uint64                  value,
    uint64                  size)
{
    PAL_ASSERT(pName != nullptr);

    uint32 index = UINT_MAX; // Assume failure until otherwise known.

    SymbolTableEntry entry = { };
    entry.st_name         = m_pStringProcessor->Add(pName);
    entry.st_info.binding = static_cast<uint8>(binding);
    entry.st_info.type    = static_cast<uint8>(type);
    entry.st_shndx        = sectionIndex;
    entry.st_value        = value;
    entry.st_size         = size;

    if ((entry.st_name != UINT_MAX) && (m_pSymbolSection->AppendData(&entry, sizeof(entry)) != nullptr))
    {
        index = static_cast<uint32>(m_pSymbolSection->GetDataSize() / SymbolTableEntrySize);
        PAL_ASSERT((m_pSymbolSection->GetDataSize() % SymbolTableEntrySize) == 0);
    }

    return index;
}

// =====================================================================================================================
template <typename Allocator>
void SymbolProcessor<Allocator>::Get(
    uint32                   index,
    const char**             ppName,
    SymbolTableEntryBinding* pBinding,
    SymbolTableEntryType*    pType,
    uint16*                  pSectionIndex,
    uint64*                  pValue,
    uint64*                  pSize
    ) const
{
    const size_t symbolOffset = index * SymbolTableEntrySize;

    PAL_ASSERT(symbolOffset < m_pSymbolSection->GetDataSize());

    const SymbolTableEntry* pSymbol = reinterpret_cast<const SymbolTableEntry*>(
        static_cast<const uint8*>(m_pSymbolSection->GetData()) + symbolOffset);

    *ppName        = m_pStringProcessor->Get(pSymbol->st_name);
    *pBinding      = static_cast<SymbolTableEntryBinding>(pSymbol->st_info.binding);
    *pType         = static_cast<SymbolTableEntryType>(pSymbol->st_info.type);
    *pSectionIndex = pSymbol->st_shndx;
    *pValue        = pSymbol->st_value;
    *pSize         = pSymbol->st_size;
}

// =====================================================================================================================
template <typename Allocator>
RelocationProcessor<Allocator>::RelocationProcessor(
    Section<Allocator>* pRelocationSection)
    :
    m_pRelocationSection(pRelocationSection)
{
}

// =====================================================================================================================
template <typename Allocator>
RelocationProcessor<Allocator>::RelocationProcessor(
    const Section<Allocator>* pRelocationSection)
    :
    m_pRelocationSection(const_cast<Section<Allocator>*>(pRelocationSection))
{
}

// =====================================================================================================================
template <typename Allocator>
uint32 RelocationProcessor<Allocator>::Add(
    uint64 offset,
    uint32 symbolIndex,
    uint32 type)
{
    PAL_ASSERT(m_pRelocationSection->GetType() == SectionHeaderType::Rel);

    uint32 index = UINT_MAX; // Assume failure until otherwise known.

    RelTableEntry entry = { };
    entry.r_info.sym  = symbolIndex;
    entry.r_info.type = type;
    entry.r_offset    = offset;

    if (m_pRelocationSection->AppendData(&entry, sizeof(entry)) != nullptr)
    {
        PAL_ASSERT((m_pRelocationSection->GetDataSize() % RelTableEntrySize) == 0);
        index = static_cast<uint32>(m_pRelocationSection->GetDataSize() / RelTableEntrySize);
    }

    return index;
}

// =====================================================================================================================
template <typename Allocator>
uint32 RelocationProcessor<Allocator>::Add(
    uint64 offset,
    uint32 symbolIndex,
    uint32 type,
    uint64 addend)
{
    PAL_ASSERT(m_pRelocationSection->GetType() == SectionHeaderType::Rela);

    uint32 index = UINT_MAX; // Assume failure until otherwise known.

    RelaTableEntry entry = { };
    entry.r_info.sym  = symbolIndex;
    entry.r_info.type = type;
    entry.r_offset    = offset;
    entry.r_addend    = addend;

    if (m_pRelocationSection->AppendData(&entry, sizeof(entry)) != nullptr)
    {
        PAL_ASSERT((m_pRelocationSection->GetDataSize() % RelaTableEntrySize) == 0);
        index = static_cast<uint32>(m_pRelocationSection->GetDataSize() / RelaTableEntrySize);
    }

    return index;
}

// =====================================================================================================================
template <typename Allocator>
void RelocationProcessor<Allocator>::GetRelEntry(
    uint32  index,
    uint64* pOffset,
    uint32* pSymbolIndex,
    uint32* pType
    ) const
{
    const size_t relocationOffset = index * RelTableEntrySize;
    PAL_ASSERT(relocationOffset < m_pRelocationSection->GetDataSize());

    const RelTableEntry*const pRelocation =
        static_cast<const RelTableEntry*>(VoidPtrInc(m_pRelocationSection->GetData(), relocationOffset));

    *pOffset      = pRelocation->r_offset;
    *pSymbolIndex = pRelocation->r_info.sym;
    *pType        = pRelocation->r_info.type;
}

// =====================================================================================================================
template <typename Allocator>
void RelocationProcessor<Allocator>::GetRelaEntry(
    uint32  index,
    uint64* pOffset,
    uint32* pSymbolIndex,
    uint32* pType,
    uint64* pAddend
    ) const
{
    const size_t relocationOffset = index * RelaTableEntrySize;
    PAL_ASSERT(relocationOffset < m_pRelocationSection->GetDataSize());

    const RelaTableEntry*const pRelocation =
        static_cast<const RelaTableEntry*>(VoidPtrInc(m_pRelocationSection->GetData(), relocationOffset));

    *pOffset      = pRelocation->r_offset;
    *pSymbolIndex = pRelocation->r_info.sym;
    *pType        = pRelocation->r_info.type;
    *pAddend      = pRelocation->r_addend;
}

// =====================================================================================================================
template <typename Allocator>
void RelocationProcessor<Allocator>::Get(
    uint32  index,
    uint64* pOffset,
    uint32* pSymbolIndex,
    uint32* pType,
    uint64* pAddend
    ) const
{
    const SectionHeaderType sectionType = m_pRelocationSection->GetType();

    if (sectionType == SectionHeaderType::Rel)
    {
        GetRelEntry(index, pOffset, pSymbolIndex, pType);
        *pAddend = 0;
    }
    else if (sectionType == SectionHeaderType::Rela)
    {
        GetRelaEntry(index, pOffset, pSymbolIndex, pType, pAddend);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }
}

// =====================================================================================================================
template <typename Allocator>
const uint32 RelocationProcessor<Allocator>::GetNumRelocations() const
{
    uint32 numRelocations = 0;

    const SectionHeaderType sectionType = m_pRelocationSection->GetType();

    if (sectionType == SectionHeaderType::Rel)
    {
        numRelocations = static_cast<uint32>(m_pRelocationSection->GetDataSize() / RelTableEntrySize);
    }
    else if (sectionType == SectionHeaderType::Rela)
    {
        numRelocations = static_cast<uint32>(m_pRelocationSection->GetDataSize() / RelaTableEntrySize);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    return numRelocations;
}

// =====================================================================================================================
template <typename Allocator>
StringProcessor<Allocator>::StringProcessor(
    Section<Allocator>* pStrTabSection,
    Allocator* const    pAllocator)
    :
    m_pStrTabSection(pStrTabSection),
    m_pAllocator(pAllocator)
{
}

// =====================================================================================================================
template <typename Allocator>
StringProcessor<Allocator>::StringProcessor(
    const Section<Allocator>* pStrTabSection,
    Allocator* const          pAllocator)
    :
    m_pStrTabSection(const_cast<Section<Allocator>*>(pStrTabSection)),
    m_pAllocator(pAllocator)
{
}

// =====================================================================================================================
template <typename Allocator>
uint32 StringProcessor<Allocator>::Add(
    const char* pString)
{
    uint32 offset = UINT_MAX;

    const char*const pData = static_cast<char*>(m_pStrTabSection->AppendData(pString, strlen(pString) + 1));
    if (pData != nullptr)
    {
        offset = static_cast<uint32>(VoidPtrDiff(pData, m_pStrTabSection->GetData()));
    }

    return offset;
}

// =====================================================================================================================
template <typename Allocator>
const char* StringProcessor<Allocator>::Get(
    uint32 offset
    ) const
{
    return static_cast<const char*>(m_pStrTabSection->GetData()) + offset;
}

// =====================================================================================================================
template <typename Allocator>
const uint32 StringProcessor<Allocator>::GetNumStrings() const
{
    const char* ch = static_cast<const char*>(m_pStrTabSection->GetData());

    uint32 count = 0;

    if (m_pStrTabSection->GetDataSize() > 0)
    {
        while (VoidPtrDiff(ch, m_pStrTabSection->GetData()) != m_pStrTabSection->GetDataSize())
        {
            if (*ch == 0)
            {
                count++;
            }
            ch++;
        }
    }

    return count;
}

// =====================================================================================================================
template <typename Allocator>
ElfProcessor<Allocator>::ElfProcessor(
    Allocator* const pAllocator)
    :
    m_fileHeader(),
    m_sections(pAllocator),
    m_segments(pAllocator),
    m_pAllocator(pAllocator)
{
    // Setup
    m_fileHeader.ei_magic    = ElfMagic;
    m_fileHeader.ei_class    = ElfClass64;
    m_fileHeader.ei_data     = ElfLittleEndian;
    m_fileHeader.ei_version  = ElfVersion;

    m_fileHeader.e_version   = ElfVersion;
    m_fileHeader.e_ehsize    = FileHeaderSize;

    m_fileHeader.e_phentsize = 0;
    m_fileHeader.e_phnum     = 0;

    m_fileHeader.e_shentsize = 0;
    m_fileHeader.e_shnum     = 0;

    m_fileHeader.e_shstrndx  = static_cast<uint16>(SectionHeaderIndex::Undef);
}

// =====================================================================================================================
template <typename Allocator>
Result ElfProcessor<Allocator>::Init()
{
    return m_sections.Init();
}

// =====================================================================================================================
template <typename Allocator>
void ElfProcessor<Allocator>::Finalize()
{
    if (m_segments.NumSegments() > 0)
    {
        // Set the program header offset.
        m_fileHeader.e_phoff = static_cast<uint64>(FileHeaderSize);

        // Set the size of the program header.
        m_fileHeader.e_phentsize = ProgramHeaderSize;

        // Set the number of program headers
        m_fileHeader.e_phnum = static_cast<uint16>(m_segments.NumSegments());
    }

    if (m_sections.NumSections() > 0)
    {
        // If NumElements is > 0. NumElements is at least 3.
        // null section + shstrtab + other section.
        m_fileHeader.e_shstrndx = static_cast<uint16>(m_sections.GetShStrTabSection()->GetIndex());

        size_t sectionOffset = FileHeaderSize + (m_segments.NumSegments()  * ProgramHeaderSize);

        // Iterate through the section vector to calculate the section offsets.
        auto sectionIterator = m_sections.Begin();

        // Skip the null section
        sectionIterator.Next();

        while (sectionIterator.IsValid())
        {
            Section<Allocator>* pSection = sectionIterator.Get();

            pSection->SetOffset(sectionOffset);
            sectionOffset += pSection->GetDataSize();

            sectionIterator.Next();
        }

        // Set the section header offset.
        m_fileHeader.e_shoff = static_cast<uint64>(RoundUpToMultiple(sectionOffset, SectionHeaderAlignment));

        // Set the size of the section header.
        m_fileHeader.e_shentsize = SectionHeaderSize;

        // Set the number of section headers
        m_fileHeader.e_shnum = static_cast<uint16>(m_sections.NumSections());
    }

    // This needs to run after the section offsets are calculated to be able
    // to determine the start of the segments.
    m_segments.Finalize();
}

// =====================================================================================================================
template <typename Allocator>
size_t ElfProcessor<Allocator>::GetRequiredBufferSizeBytes() const
{
    size_t bufferSizeBytes = FileHeaderSize;

    bufferSizeBytes += m_segments.NumSegments() * ProgramHeaderSize;

    // Iterate through the section vector.
    auto sectionIterator = m_sections.Begin();
    while (sectionIterator.IsValid())
    {
        const Section<Allocator>* pSection = sectionIterator.Get();
        bufferSizeBytes += pSection->GetDataSize();

        sectionIterator.Next();
    }

    bufferSizeBytes = RoundUpToMultiple(bufferSizeBytes, SectionHeaderAlignment);

    bufferSizeBytes += m_sections.NumSections() * SectionHeaderSize;

    return bufferSizeBytes;
}

// =====================================================================================================================
template <typename Allocator>
void ElfProcessor<Allocator>::SaveToBuffer(
    void* pBuffer)
{
    // Finalize offsets and sizes.
    Finalize();

    uint8* pBufferWriter = static_cast<uint8*>(pBuffer);

    memcpy(pBuffer, &m_fileHeader, FileHeaderSize);
    pBufferWriter += FileHeaderSize;

    if (m_segments.NumSegments() > 0)
    {
        // Iterate through the segment vector to write out the program headers.
        auto segmentIterator = m_segments.Begin();
        while (segmentIterator.IsValid())
        {
            const Segment<Allocator>* pSegment = segmentIterator.Get();

            memcpy(pBufferWriter, pSegment->GetProgramHeader(), ProgramHeaderSize);
            pBufferWriter += ProgramHeaderSize;

            segmentIterator.Next();
        }
    }

    if (m_sections.NumSections() > 0)
    {
        // Iterate through the section vector to write out the section contents.
        auto sectionContentIterator = m_sections.Begin();
        while (sectionContentIterator.IsValid())
        {
            const Section<Allocator>* pSection = sectionContentIterator.Get();

            const size_t dataSize =  pSection->GetDataSize();
            memcpy(pBufferWriter, pSection->GetData(), dataSize);
            pBufferWriter += dataSize;

            sectionContentIterator.Next();
        }

        const size_t sectionHeaderPadding =
            VoidPtrDiff(VoidPtrAlign(pBufferWriter, SectionHeaderAlignment), pBufferWriter);

        memset(pBufferWriter, 0, sectionHeaderPadding);
        pBufferWriter += sectionHeaderPadding;

        // Iterate through the section vector to write out the section headers.
        auto sectionIterator = m_sections.Begin();
        while (sectionIterator.IsValid())
        {
            const Section<Allocator>* pSection = sectionIterator.Get();

            memcpy(pBufferWriter, pSection->GetSectionHeader(), SectionHeaderSize);
            pBufferWriter += SectionHeaderSize;

            sectionIterator.Next();
        }
    }

    PAL_ASSERT(VoidPtrDiff(pBufferWriter, pBuffer) == GetRequiredBufferSizeBytes());
}

// =====================================================================================================================
template <typename Allocator>
Result ElfProcessor<Allocator>::LoadFromBuffer(
    const void*  pBuffer,
    size_t       bufferSize)
{
    const void* pBufferStart = pBuffer;
    PAL_ASSERT(bufferSize >= FileHeaderSize);

    Result result = m_sections.Init();
    if (result == Result::Success)
    {
        // Read in the ELF FileHeader
        memcpy(&m_fileHeader, pBufferStart, FileHeaderSize);

        // Skip the program headers and go straight to the section headers.
        // Once the sections are created we can determine the segment section mappings.
        if (m_fileHeader.e_shnum > 0)
        {
            // In this ElfProcessor the .shstrtab should come after the null section.
            PAL_ASSERT(m_fileHeader.e_shstrndx == 1);

            // If there are any sections there should be at least 3: The null
            // section, .shstrtab, and an additional section using .shstrtab.
            PAL_ASSERT(m_fileHeader.e_shnum >= 3);

            const SectionHeader* pSectionHdrReader =
                static_cast<const SectionHeader*>(VoidPtrInc(pBufferStart, static_cast<size_t>(m_fileHeader.e_shoff)));

            // Skip Null Section
            pSectionHdrReader++;

            // Get a pointer to the section names
            const char* pSectionHeaderNames =
                static_cast<const char*>(VoidPtrInc(pBufferStart, static_cast<size_t>(pSectionHdrReader->sh_offset)));

            for (uint32 i = 1; i < m_fileHeader.e_shnum; i++)
            {
                const char*const pName = (pSectionHeaderNames + pSectionHdrReader->sh_name);

                Section<Allocator>* pSection;
                if (i == 1)
                {
                    // The first section, assumed to be the section name strtab, is handled
                    // magically and we cannot call Add for it.
                    pSection = m_sections.GetShStrTabSection();
                    pSection->SetNameOffset(pSectionHdrReader->sh_name);
                }
                else
                {
                    pSection = m_sections.Add(pName, pSectionHdrReader);
                    if (pSection == nullptr)
                    {
                        result = Result::ErrorOutOfMemory;
                        break;
                    }
                }

                pSection->SetType(static_cast<SectionHeaderType>(pSectionHdrReader->sh_type));
                pSection->SetFlags(pSectionHdrReader->sh_flags);
                pSection->SetAddr(pSectionHdrReader->sh_addr);

                if (pSectionHdrReader->sh_link != 0)
                {
                    PAL_ASSERT(pSectionHdrReader->sh_link < m_sections.NumSections());
                    pSection->SetLink(m_sections.Get(pSectionHdrReader->sh_link));
                }

                switch (static_cast<SectionHeaderType>(pSectionHdrReader->sh_type))
                {
                    case SectionHeaderType::Rel:
                    case SectionHeaderType::Rela:
                        // Only relocation sections have the index of another section in the info field.
                        PAL_ASSERT(pSectionHdrReader->sh_info < m_sections.NumSections());
                        pSection->SetInfo(m_sections.Get(pSectionHdrReader->sh_info));
                        break;
                    default:
                        break;
                }

                pSection->SetAlignment(pSectionHdrReader->sh_addralign);
                pSection->SetEntrySize(pSectionHdrReader->sh_entsize);

                pSection->SetOffset(static_cast<size_t>(pSectionHdrReader->sh_offset));
                const void* pData = VoidPtrInc(pBufferStart, static_cast<size_t>(pSectionHdrReader->sh_offset));
                if ((pSectionHdrReader->sh_size != 0) &&
                    (pSection->SetData(pData, static_cast<size_t>(pSectionHdrReader->sh_size)) == nullptr))
                {
                    result = Result::ErrorOutOfMemory;
                    break;
                }

                pSectionHdrReader++;
            }  // For each section header.
        }
    }

    if ((result == Result::Success) && (m_fileHeader.e_phnum > 0))
    {
        const ProgramHeader* pProgramHdrReader =
            static_cast<const ProgramHeader*>(VoidPtrInc(pBufferStart, static_cast<size_t>(m_fileHeader.e_phoff)));

        for (uint32 i = 0; i < m_fileHeader.e_phnum; i++)
        {
            Segment<Allocator>* pSegment = m_segments.Add();
            if (pSegment == nullptr)
            {
                result = Result::ErrorOutOfMemory;
                break;
            }

            pSegment->SetType(static_cast<SegmentType>(pProgramHdrReader->p_type));
            pSegment->SetFlags(pProgramHdrReader->p_flags);
            pSegment->SetVirtualAddr(pProgramHdrReader->p_vaddr);
            pSegment->SetAlignment(pProgramHdrReader->p_align);

            if (pProgramHdrReader->p_filesz > 0)
            {
                // Figure out sections based off of size and offset.

                bool   offsetFound = false;
                size_t segmentSize = 0;

                // Iterate through the sections to find the section with the same
                // offset as the segment.
                auto sectionIterator = m_sections.Begin();
                while (sectionIterator.IsValid() && (segmentSize < pProgramHdrReader->p_filesz))
                {
                    Section<Allocator>* pSection = sectionIterator.Get();

                    if (!offsetFound && (pProgramHdrReader->p_offset == pSection->GetOffset()))
                    {
                        offsetFound = true;
                    }

                    if(offsetFound)
                    {
                        segmentSize += pSection->GetDataSize();
                        pSegment->AddSection(pSection);
                    }

                    sectionIterator.Next();
                }

                PAL_ASSERT(offsetFound);
                PAL_ASSERT(segmentSize == pProgramHdrReader->p_filesz);

                pSegment->Finalize();

                PAL_ASSERT(pSegment->GetOffset() == pProgramHdrReader->p_offset);
                PAL_ASSERT(pSegment->GetSize() == pProgramHdrReader->p_filesz);
            }

            pProgramHdrReader++;
        }
    }

    return result;
}

} // Elf
} // Util
