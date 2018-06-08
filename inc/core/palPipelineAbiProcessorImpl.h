/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palPipelineAbiProcessorImpl.h
 * @brief PAL Pipeline ABI utility class implementations.  The PipelineAbiProcessor is a layer on top of ElfProcessor
 * which creates and loads ELFs compatible with the pipeline ABI.
 ***********************************************************************************************************************
 */

#pragma once

#include "palPipelineAbiProcessor.h"
#include "palAutoBuffer.h"

namespace Util
{
namespace Abi
{

constexpr size_t AbiAmdGpuVersionNoteSize = sizeof(AbiAmdGpuVersionNote);
constexpr size_t AbiMinorVersionNoteSize  = sizeof(AbiMinorVersionNote);
constexpr size_t PalMetadataNoteEntrySize = sizeof(PalMetadataNoteEntry);

constexpr uint32 NumBuckets = 128;

// =====================================================================================================================
template <typename Allocator>
PipelineAbiProcessor<Allocator>::PipelineAbiProcessor(
    Allocator* const pAllocator)
    :
    m_pTextSection(nullptr),
    m_pDataSection(nullptr),
    m_pRoDataSection(nullptr),
    m_pRelTextSection(nullptr),
    m_pRelDataSection(nullptr),
    m_pRelaTextSection(nullptr),
    m_pRelaDataSection(nullptr),
    m_pSymbolSection(nullptr),
    m_pSymbolStrTabSection(nullptr),
    m_pNoteSection(nullptr),
    m_pCommentSection(nullptr),
    m_pDisasmSection(nullptr),
    m_gpuVersionNote(),
    m_abiMinorVersionNote(),
    m_registerMap(NumBuckets, pAllocator),
    m_pipelineMetadataVector(pAllocator),
    m_pipelineMetadataIndices(),
    m_pipelineSymbolsVector(pAllocator),
    m_pipelineSymbolIndices(),
    m_elfProcessor(pAllocator),
    m_pAllocator(pAllocator)
{
    for(uint32 i = 0; i < static_cast<uint32>(PipelineMetadataType::Count); i++)
    {
        m_pipelineMetadataIndices[i] = -1;
    }

    for(uint32 i = 0; i < static_cast<uint32>(PipelineSymbolType::Count); i++)
    {
        m_pipelineSymbolIndices[i] = -1;
    }
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::Init()
{
    Result result = m_registerMap.Init();
    if (result == Result::Success)
    {
        result = m_elfProcessor.Init();
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
void PipelineAbiProcessor<Allocator>::SetGfxIpVersion(
    uint32 gfxipMajorVer,
    uint32 gfxipMinorVer,
    uint32 gfxipStepping)
{
    m_gpuVersionNote.gfxipMajorVer = gfxipMajorVer;
    m_gpuVersionNote.gfxipMinorVer = gfxipMinorVer;
    m_gpuVersionNote.gfxipStepping = gfxipStepping;
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::SetPipelineCode(
    const void* pCode,
    size_t      codeSize)
{
    Result result = Result::Success;
    if (m_pTextSection == nullptr)
    {
        result = CreateTextSection();
    }

    if (result == Result::Success)
    {
        if (nullptr == m_pTextSection->SetData(pCode, codeSize))
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::SetData(
    const void* pData,
    size_t      dataSize,
    gpusize     alignment)
{
    PAL_ASSERT(Pow2Align(alignment, DataMinBaseAddrAlignment) == alignment);

    Result result = Result::Success;
    if (m_pDataSection == nullptr)
    {
        result = CreateDataSection();
    }

    if (result == Result::Success)
    {
        m_pDataSection->SetAlignment(alignment);
        if (nullptr == m_pDataSection->SetData(pData, dataSize))
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::SetReadOnlyData(
    const void* pData,
    size_t      dataSize,
    gpusize     alignment)
{
    PAL_ASSERT(Pow2Align(alignment, RoDataMinBaseAddrAlignment) == alignment);

    Result result = Result::Success;
    if (m_pRoDataSection == nullptr)
    {
        result = CreateRoDataSection();
    }

    if (result == Result::Success)
    {
        m_pRoDataSection->SetAlignment(alignment);
        if (nullptr == m_pRoDataSection->SetData(pData, dataSize))
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::SetComment(
    const char* pComment)
{
    if (m_pCommentSection == nullptr)
    {
        m_pCommentSection = m_elfProcessor.GetSections()->Add(Elf::SectionType::Comment);
    }

    const size_t commentSize = (strlen(pComment) + 1);

    Result result = Result::Success;
    if ((m_pCommentSection == nullptr) || (nullptr == m_pCommentSection->SetData(pComment, commentSize)))
    {
        result = Result::ErrorOutOfMemory;
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::SetDisassembly(
    const void* pData,
    size_t      dataSize)
{
    if (m_pDisasmSection == nullptr)
    {
        m_pDisasmSection = m_elfProcessor.GetSections()->Add(&AmdGpuDisassemblyName[0]);
        if (m_pDisasmSection != nullptr)
        {
            m_pDisasmSection->SetType(Elf::SectionHeaderType::ProgBits);
        }
    }

    Result result = Result::Success;
    if ((m_pDisasmSection == nullptr) || (nullptr == m_pDisasmSection->SetData(pData, dataSize)))
    {
        result = Result::ErrorOutOfMemory;
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
void PipelineAbiProcessor<Allocator>::GetPipelineCode(
    const void** ppCode,
    size_t*      pCodeSize
    ) const
{
    if (m_pTextSection != nullptr)
    {
        *ppCode    = m_pTextSection->GetData();
        *pCodeSize = m_pTextSection->GetDataSize();
    }
    else
    {
        *ppCode = nullptr;
    }
}

// =====================================================================================================================
template <typename Allocator>
void PipelineAbiProcessor<Allocator>::GetData(
    const void** ppData,
    size_t*      pDataSize,
    gpusize*     pAlignment
    ) const
{
    if (m_pDataSection != nullptr)
    {
        *ppData     = m_pDataSection->GetData();
        *pDataSize  = m_pDataSection->GetDataSize();
        *pAlignment = static_cast<gpusize>(m_pDataSection->GetSectionHeader()->sh_addralign);
    }
    else
    {
        *ppData = nullptr;
    }
}

// =====================================================================================================================
template <typename Allocator>
void PipelineAbiProcessor<Allocator>::GetReadOnlyData(
    const void** ppData,
    size_t*      pDataSize,
    gpusize*     pAlignment
    ) const
{
    if (m_pRoDataSection != nullptr)
    {
        *ppData     = m_pRoDataSection->GetData();
        *pDataSize  = m_pRoDataSection->GetDataSize();
        *pAlignment = static_cast<gpusize>(m_pRoDataSection->GetSectionHeader()->sh_addralign);
    }
    else
    {
        *ppData = nullptr;
    }
}

// =====================================================================================================================
template <typename Allocator>
const char* PipelineAbiProcessor<Allocator>::GetComment() const
{
    const char* pComment = (m_pCommentSection == nullptr) ? ""
                                                          : static_cast<const char*>(m_pCommentSection->GetData());

    return pComment;
}

// =====================================================================================================================
template <typename Allocator>
void PipelineAbiProcessor<Allocator>::GetDisassembly(
    const void** ppData,
    size_t*      pDataSize
    ) const
{
    if (m_pDisasmSection != nullptr)
    {
        *ppData    = m_pDisasmSection->GetData();
        *pDataSize = m_pDisasmSection->GetDataSize();
    }
    else
    {
        *ppData    = nullptr;
        *pDataSize = 0;
    }
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::AddRegisterEntry(
    uint32 offset,
    uint32 value)
{
    RegisterEntry entry = { };
    entry.key   = offset;
    entry.value = value;
    return AddRegisterEntry(entry);
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::AddPipelineMetadataEntry(
    PipelineMetadataEntry entry)
{
    Result result = m_pipelineMetadataVector.PushBack(entry);
    if (result == Result::Success)
    {
        m_pipelineMetadataIndices[static_cast<uint32>(entry.key)] = m_pipelineMetadataVector.NumElements() - 1;
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::AddPipelineSymbolEntry(
    PipelineSymbolEntry entry)
{
    Result result = m_pipelineSymbolsVector.PushBack(entry);
    if (result == Result::Success)
    {
        m_pipelineSymbolIndices[static_cast<uint32>(entry.type)] = m_pipelineSymbolsVector.NumElements() - 1;
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
bool PipelineAbiProcessor<Allocator>::HasRegisterEntry(
    uint32  registerOffset,
    uint32* pRegisterValue
    ) const
{
    const RegisterEntry* pRegisterEntry = m_registerMap.FindKey(registerOffset);
    const bool registerExists           = pRegisterEntry != nullptr;

    if (registerExists)
    {
        *pRegisterValue = pRegisterEntry->value;
    }

    return registerExists;
}

// =====================================================================================================================
template <typename Allocator>
uint32 PipelineAbiProcessor<Allocator>::GetRegisterEntry(
    uint32 registerOffset
    ) const
{
    const RegisterEntry* pRegisterEntry = m_registerMap.FindKey(registerOffset);
    PAL_ASSERT(pRegisterEntry != nullptr);
    return pRegisterEntry->value;
}

// =====================================================================================================================
template <typename Allocator>
bool PipelineAbiProcessor<Allocator>::HasPipelineMetadataEntry(
    PipelineMetadataType pipelineMetadataType,
    uint32*              pPipelineMetadataValue
    ) const
{
    const int32 index = m_pipelineMetadataIndices[static_cast<uint32>(pipelineMetadataType)];
    const bool pipelineMetadataEntryExists = (index >= 0);

    if (pipelineMetadataEntryExists)
    {
        *pPipelineMetadataValue = m_pipelineMetadataVector.At(index).value;
    }

    return pipelineMetadataEntryExists;
}

// =====================================================================================================================
template <typename Allocator>
bool PipelineAbiProcessor<Allocator>::HasPipelineMetadataEntries(
    PipelineMetadataType pipelineMetadataTypeHigh,
    PipelineMetadataType pipelineMetadataTypeLow
    ) const
{
    const int32 indexHigh = m_pipelineMetadataIndices[static_cast<uint32>(pipelineMetadataTypeHigh)];
    const int32 indexLow  = m_pipelineMetadataIndices[static_cast<uint32>(pipelineMetadataTypeLow)];

    return (indexHigh >= 0) && (indexLow >= 0);
}

// =====================================================================================================================
template <typename Allocator>
bool PipelineAbiProcessor<Allocator>::HasPipelineMetadataEntries(
    PipelineMetadataType pipelineMetadataTypeHigh,
    PipelineMetadataType pipelineMetadataTypeLow,
    uint64*              pPipelineMetadataValue
    ) const
{
    const int32 indexHigh = m_pipelineMetadataIndices[static_cast<uint32>(pipelineMetadataTypeHigh)];
    const int32 indexLow  = m_pipelineMetadataIndices[static_cast<uint32>(pipelineMetadataTypeLow)];

    const bool pipelineMetadataEntriesExists = (indexHigh >= 0) && (indexLow >= 0);

    if (pipelineMetadataEntriesExists)
    {
        *pPipelineMetadataValue = (static_cast<uint64>(m_pipelineMetadataVector.At(indexHigh).value) << 32) |
                                  m_pipelineMetadataVector.At(indexLow).value;
    }

    return pipelineMetadataEntriesExists;
}

// =====================================================================================================================
template <typename Allocator>
uint64 PipelineAbiProcessor<Allocator>::GetPipelineMetadataEntries(
    PipelineMetadataType pipelineMetadataTypeHigh,
    PipelineMetadataType pipelineMetadataTypeLow
    ) const
{
    const int32 indexHigh = m_pipelineMetadataIndices[static_cast<uint32>(pipelineMetadataTypeHigh)];
    const int32 indexLow  = m_pipelineMetadataIndices[static_cast<uint32>(pipelineMetadataTypeLow)];

    PAL_ASSERT(indexHigh >= 0);
    PAL_ASSERT(indexLow >= 0);

    return (static_cast<uint64>(m_pipelineMetadataVector.At(indexHigh).value) << 32) |
           m_pipelineMetadataVector.At(indexLow).value;
}

// =====================================================================================================================
template <typename Allocator>
bool PipelineAbiProcessor<Allocator>::HasPipelineSymbolEntry(
    PipelineSymbolType   pipelineSymbolType,
    PipelineSymbolEntry* pPipelineSymbolEntry
    ) const
{
    const int32 index = m_pipelineSymbolIndices[static_cast<uint32>(pipelineSymbolType)];
    const bool pipelineSymbolEntryExists = (index >= 0);

    if (pipelineSymbolEntryExists)
    {
        *pPipelineSymbolEntry = m_pipelineSymbolsVector.At(index);
    }

    return pipelineSymbolEntryExists;
}

// =====================================================================================================================
template <typename Allocator>
void PipelineAbiProcessor<Allocator>::GetGfxIpVersion(
    uint32* pGfxipMajorVer,
    uint32* pGfxipMinorVer,
    uint32* pGfxipStepping
    ) const
{
    *pGfxipMajorVer = m_gpuVersionNote.gfxipMajorVer;
    *pGfxipMinorVer = m_gpuVersionNote.gfxipMinorVer;
    *pGfxipStepping = m_gpuVersionNote.gfxipStepping;
}

// =====================================================================================================================
template <typename Allocator>
void PipelineAbiProcessor<Allocator>::GetAbiVersion(
    uint32* pAbiMajorVer,
    uint32* pAbiMinorVer
    ) const
{
    *pAbiMajorVer = m_elfProcessor.GetFileHeader()->ei_abiversion;
    *pAbiMinorVer = m_abiMinorVersionNote.minorVersion;
}

// =====================================================================================================================
template <typename Allocator>
void PipelineAbiProcessor<Allocator>::RelocationHelper(
    void*                    pBuffer,
    uint64                   baseAddress,
    Elf::Section<Allocator>* pRelocationSection
    ) const
{
    Elf::RelocationProcessor<Allocator> relocationProcessor(pRelocationSection);

    const uint32 numRelocations = relocationProcessor.GetNumRelocations();
    for (uint32 index = 0; index < numRelocations; index++)
    {
        uint64 offset      = 0;
        uint32 symbolIndex = 0;
        uint32 type        = 0;
        uint64 addend      = 0;
        relocationProcessor.Get(index, &offset, &symbolIndex, &type, &addend);

        uint64*const pReference = static_cast<uint64* const>(VoidPtrInc(pBuffer, static_cast<size_t>(offset)));

        if (pRelocationSection->GetType() == Elf::SectionHeaderType::Rel)
        {
            addend = *pReference;
        }

    }
}

// =====================================================================================================================
template <typename Allocator>
void PipelineAbiProcessor<Allocator>::ApplyRelocations(
    void*          pBuffer,
    size_t         bufferSize,
    AbiSectionType sectionType,
    uint64         baseAddress
    ) const
{
    Elf::Section<Allocator>* pRelSection  = nullptr;
    Elf::Section<Allocator>* pRelaSection = nullptr;

    switch (sectionType)
    {
    case AbiSectionType::Undefined:
        PAL_ASSERT_ALWAYS();
        break;
    case AbiSectionType::Code:
        pRelSection  = m_pRelTextSection;
        pRelaSection = m_pRelaTextSection;
        break;
    case AbiSectionType::Data:
        pRelSection  = m_pRelDataSection;
        pRelaSection = m_pRelaDataSection;
        break;
    case AbiSectionType::Disassembly:
        PAL_ASSERT_ALWAYS();
        break;
    default:
        PAL_NEVER_CALLED();
        break;
    }

    if (pRelSection != nullptr)
    {
        RelocationHelper(pBuffer, baseAddress, pRelSection);
    }

    if (pRelaSection != nullptr)
    {
        RelocationHelper(pBuffer, baseAddress, pRelaSection);
    }
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::Finalize(
    const char* pPipelineName)
{
    Result result = Result::Success;

    m_elfProcessor.SetOsAbi(ElfOsAbiVersion);

    m_elfProcessor.SetAbiVersion(ElfAbiMajorVersion);
    m_abiMinorVersionNote.minorVersion = ElfAbiMinorVersion;

    m_elfProcessor.SetObjectFileType(Elf::ObjectFileType::Rel);
    m_elfProcessor.SetTargetMachine(Elf::MachineType::AmdGpu);

    m_pSymbolStrTabSection = m_elfProcessor.GetSections()->Add(Elf::SectionType::StrTab);
    m_pSymbolSection       = m_elfProcessor.GetSections()->Add(Elf::SectionType::SymTab);

    // Handle the ELF symbols.
    if ((m_pSymbolSection == nullptr) || (m_pSymbolStrTabSection == nullptr))
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        m_pSymbolSection->SetLink(m_pSymbolStrTabSection);

        Elf::StringProcessor<Allocator> symbolStringProcessor(m_pSymbolStrTabSection, m_pAllocator);
        Elf::SymbolProcessor<Allocator> symbolProcessor(m_pSymbolSection, &symbolStringProcessor, m_pAllocator);

        PipelineSymbolVectorIter iter = PipelineSymbolsBegin();
        // There should at least be one symbol pointing to a shader load address!
        PAL_ASSERT(iter.IsValid());
        do
        {
            Abi::PipelineSymbolEntry pipeSymb = iter.Get();

            uint32 sectionIndex = 0;
            switch (pipeSymb.sectionType)
            {
            case AbiSectionType::Undefined:
                break;
            case AbiSectionType::Code:
                sectionIndex = m_pTextSection->GetIndex();
                break;
            case AbiSectionType::Data:
                sectionIndex = m_pDataSection->GetIndex();
                break;
            case AbiSectionType::Disassembly:
                sectionIndex = m_pDisasmSection->GetIndex();
                break;
            default:
                PAL_NEVER_CALLED();
                break;
            }

            if (symbolProcessor.Add(PipelineAbiSymbolNameStrings[static_cast<uint32>(pipeSymb.type)],
                                    Elf::SymbolTableEntryBinding::Local,
                                    pipeSymb.entryType,
                                    static_cast<uint16>(sectionIndex),
                                    pipeSymb.value,
                                    pipeSymb.size) == UINT_MAX)
            {
                result = Result::ErrorOutOfMemory;
                break;
            }

            iter.Next();
        } while (iter.IsValid());

        // NOTE: This is a bit of a hack.  We're adding the human-readable name for this pipeline (if one exists) to
        // the symbol-name string table as a temporary measure.  Eventually, the pipeline ABI metadata layout will be
        // changed to be more extensible so that arbitrary data (such as strings) can be stored.
        if (pPipelineName != nullptr)
        {
            PipelineMetadataEntry entry = { };
            entry.key   = Abi::PipelineMetadataType::PipelineNameIndex;
            entry.value = symbolStringProcessor.Add(pPipelineName);

            if (entry.value != UINT_MAX)
            {
                this->AddPipelineMetadataEntry(entry);
            }
        }
    }

    // Handle the AMDGPU & PAL ABI note entries:

    if (result == Result::Success)
    {
        m_pNoteSection = m_elfProcessor.GetSections()->Add(Elf::SectionType::Note);
        if (m_pNoteSection == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    if (result == Result::Success)
    {
        Elf::NoteProcessor<Allocator> noteProcessor(m_pNoteSection, m_pAllocator);

        result = noteProcessor.Init();
        if (result == Result::Success)
        {
            if ((noteProcessor.Add(static_cast<uint32>(PipelineAbiNoteType::HsaIsa),
                                   AmdGpuVendorName,
                                   &m_gpuVersionNote,
                                   AbiAmdGpuVersionNoteSize) == UINT_MAX) ||
                (noteProcessor.Add(static_cast<uint32>(PipelineAbiNoteType::AbiMinorVersion),
                                   AmdGpuVendorName,
                                   &m_abiMinorVersionNote,
                                   AbiMinorVersionNoteSize) == UINT_MAX))
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                const size_t noteEntryCount = (m_registerMap.GetNumEntries() + m_pipelineMetadataVector.NumElements());
                AutoBuffer<PalMetadataNoteEntry, 64, Allocator> entries(noteEntryCount, m_pAllocator);

                result = Result::ErrorOutOfMemory;

                if (entries.Capacity() >= noteEntryCount)
                {
                    uint32 index = 0;

                    for (auto iter = RegistersBegin(); iter.Get() != nullptr; iter.Next())
                    {
                        entries[index] = *reinterpret_cast<PalMetadataNoteEntry*>(&((*iter.Get()).value));
                        ++index;
                    }

                    for (auto iter = PipelineMetadataBegin(); iter.IsValid(); iter.Next())
                    {
                        PalMetadataNoteEntry entry = *reinterpret_cast<PalMetadataNoteEntry*>(&iter.Get());
                        entry.key |= PipelineMetadataBase;
                        entries[index] = entry;

                        ++index;
                    }

                    if (noteProcessor.Add(static_cast<uint32>(PipelineAbiNoteType::PalMetadata),
                                          AmdGpuVendorName,
                                          &entries[0],
                                          (noteEntryCount * sizeof(PalMetadataNoteEntry))) != UINT_MAX)
                    {
                        result = Result::Success;
                    }
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
void PipelineAbiProcessor<Allocator>::SaveToBuffer(
    void* pBuffer)
{
    PAL_ASSERT(m_pTextSection         != nullptr);
    PAL_ASSERT(m_pNoteSection         != nullptr);
    PAL_ASSERT(m_pSymbolSection       != nullptr);
    PAL_ASSERT(m_pSymbolStrTabSection != nullptr);

    m_elfProcessor.SaveToBuffer(pBuffer);
}

// =====================================================================================================================
template <typename Allocator>
const char* PipelineAbiProcessor<Allocator>::GetPipelineName() const
{
    const char* pName = nullptr;

    uint32 nameIndex = 0;
    if (this->HasPipelineMetadataEntry(PipelineMetadataType::PipelineNameIndex, &nameIndex))
    {
        Elf::StringProcessor<Allocator> symbolStringProcessor(m_pSymbolStrTabSection, m_pAllocator);
        pName = symbolStringProcessor.Get(nameIndex);
    }

    return pName;
}

// =====================================================================================================================
template <typename Allocator>
PipelineSymbolType PipelineAbiProcessor<Allocator>::GetSymbolTypeFromName(
    const char* pName
    ) const
{
    PipelineSymbolType type = PipelineSymbolType::Unknown;
    for (uint32 i = 0; i < static_cast<uint32>(PipelineSymbolType::Count); i++)
    {
        if (strcmp(PipelineAbiSymbolNameStrings[i], pName) == 0)
        {
            type = static_cast<PipelineSymbolType>(i);
            break;
        }
    }

    return type;
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::LoadFromBuffer(
    const void* pBuffer,
    size_t      bufferSize)
{
    Result result = m_registerMap.Init();

    if (result == Result::Success)
    {
        result = m_elfProcessor.LoadFromBuffer(pBuffer, bufferSize);
    }

    if (result == Result::Success)
    {
        // TODO: Determine what are considered unsupported ABI major and minor versions to
        // figure out when to return Result::ErrorUnsupportedPipelineElfAbiVersion
        if (m_elfProcessor.GetFileHeader()->ei_osabi != ElfOsAbiVersion)
        {
            result = Result::ErrorInvalidPipelineElf;
        }
        else if (m_elfProcessor.GetTargetMachine() != Elf::MachineType::AmdGpu)
        {
            result = Result::ErrorInvalidPipelineElf;
        }
    }

    if (result == Result::Success)
    {
        m_pTextSection         = m_elfProcessor.GetSections()->Get(".text");
        m_pDataSection         = m_elfProcessor.GetSections()->Get(".data");
        m_pRoDataSection       = m_elfProcessor.GetSections()->Get(".rodata");

        m_pRelTextSection      = m_elfProcessor.GetSections()->Get(".rel.text");
        m_pRelDataSection      = m_elfProcessor.GetSections()->Get(".rel.data");

        m_pRelaTextSection     = m_elfProcessor.GetSections()->Get(".rela.text");
        m_pRelaDataSection     = m_elfProcessor.GetSections()->Get(".rela.data");

        m_pSymbolSection       = m_elfProcessor.GetSections()->Get(".symtab");
        if (m_pSymbolSection)
        {
            m_pSymbolStrTabSection = m_pSymbolSection->GetLink();
        }

        m_pNoteSection         = m_elfProcessor.GetSections()->Get(".note");
        m_pCommentSection      = m_elfProcessor.GetSections()->Get(".comment");

        m_pDisasmSection       = m_elfProcessor.GetSections()->Get(AmdGpuDisassemblyName);

        // Check that all required sections are present
        if ((m_pTextSection == nullptr)   ||
            (m_pNoteSection == nullptr)   ||
            (m_pSymbolSection == nullptr) ||
            (m_pSymbolStrTabSection == nullptr))
        {
            result = Result::ErrorInvalidPipelineElf;
        }
    }

    if (result == Result::Success)
    {
        Elf::NoteProcessor<Allocator> noteProcessor(m_pNoteSection, m_pAllocator);

        result = noteProcessor.Init();
        if (result == Result::Success)
        {
            for (uint32 i = 0; i < noteProcessor.GetNumNotes(); i++)
            {
                uint32 type;
                const char* pName;
                const void* pDesc;
                size_t descSize;

                noteProcessor.Get(i, &type, &pName, &pDesc, &descSize);

                switch (static_cast<PipelineAbiNoteType>(type))
                {
                case PipelineAbiNoteType::HsaIsa:
                    PAL_ASSERT(descSize >= AbiAmdGpuVersionNoteSize);
                    m_gpuVersionNote = *static_cast<const AbiAmdGpuVersionNote*>(pDesc);
                    break;
                case PipelineAbiNoteType::AbiMinorVersion:
                    PAL_ASSERT(descSize == AbiMinorVersionNoteSize);
                    m_abiMinorVersionNote = *static_cast<const AbiMinorVersionNote*>(pDesc);
                    break;
                case PipelineAbiNoteType::PalMetadata:
                {
                    PAL_ASSERT(descSize % PalMetadataNoteEntrySize == 0);

                    const PalMetadataNoteEntry* pMetadataEntryReader =
                        static_cast<const PalMetadataNoteEntry*>(pDesc);

                    while ((result == Result::Success) && (VoidPtrDiff(pMetadataEntryReader, pDesc) < descSize))
                    {
                        if (pMetadataEntryReader->key < PipelineMetadataBase)
                        {
                            // Entry is a RegisterEntry
                            result = AddRegisterEntry(*pMetadataEntryReader);
                        }
                        else
                        {
                            // Entry is a PipelineMetadataEntry
                            PipelineMetadataEntry entry;

                            entry.key = static_cast<PipelineMetadataType>(pMetadataEntryReader->key & 0x0FFFFFFF);
                            entry.value = pMetadataEntryReader->value;

                            PAL_ASSERT((entry.key >= PipelineMetadataType::ApiCsHashDword0) &&
                                       (entry.key < PipelineMetadataType::Count));

                            result = AddPipelineMetadataEntry(entry);
                        }

                        pMetadataEntryReader++;
                    }
                    break;
                }
                default:
                    // Unknown note type.
                    break;
                }
            }
        }
    }

    if (result == Result::Success)
    {
        Elf::StringProcessor<Allocator> symbolStringProcessor(m_pSymbolStrTabSection, m_pAllocator);
        Elf::SymbolProcessor<Allocator> symbolProcessor(m_pSymbolSection, &symbolStringProcessor, m_pAllocator);

        for (uint32 i = 0; ((result == Result::Success) && (i < symbolProcessor.GetNumSymbols())); i++)
        {
            const char* pName;
            Elf::SymbolTableEntryBinding binding;
            Elf::SymbolTableEntryType type;
            uint16 sectionIndex;
            uint64 value;
            uint64 size;

            symbolProcessor.Get(i, &pName, &binding, &type, &sectionIndex, &value, &size);

            PipelineSymbolType pipelineSymbolType = GetSymbolTypeFromName(pName);

            AbiSectionType sectionType = AbiSectionType::Undefined;

            if (sectionIndex == m_pTextSection->GetIndex())
            {
                sectionType = AbiSectionType::Code;
            }
            else if ((m_pDataSection != nullptr) && (sectionIndex == m_pDataSection->GetIndex()))
            {
                sectionType = AbiSectionType::Data;
            }
            else if ((m_pDisasmSection != nullptr) && (sectionIndex == m_pDisasmSection->GetIndex()))
            {
                sectionType = AbiSectionType::Disassembly;
            }
            else if (sectionIndex != 0)
            {
                PAL_ASSERT_ALWAYS();
            }

            if (pipelineSymbolType != PipelineSymbolType::Unknown)
            {
                result = AddPipelineSymbolEntry({pipelineSymbolType, type, sectionType, value, size});
            }
        }
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::CreateDataSection()
{
    PAL_ASSERT(m_pDataSection == nullptr);

    Result result = Result::Success;

    m_pDataSection = m_elfProcessor.GetSections()->Add(Elf::SectionType::Data);

    if (m_pDataSection == nullptr)
    {
        // This object gets cleaned-up by the Sections<> helper class, so we can just zero the
        // pointer here without worrying about leaks.
        m_pDataSection = nullptr;

        result = Result::ErrorOutOfMemory;
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::CreateRoDataSection()
{
    PAL_ASSERT(m_pRoDataSection == nullptr);

    Result result = Result::Success;

    m_pRoDataSection     = m_elfProcessor.GetSections()->Add(Elf::SectionType::RoData);

    if (m_pRoDataSection == nullptr)
    {
        // This object gets cleaned-up by the Sections<> helper class, so we can just zero the
        // pointer here without worrying about leaks.
        m_pRoDataSection = nullptr;

        result = Result::ErrorOutOfMemory;
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::CreateTextSection()
{
    PAL_ASSERT(m_pTextSection == nullptr);

    Result result = Result::Success;

    m_pTextSection         = m_elfProcessor.GetSections()->Add(Elf::SectionType::Text);

    if (m_pTextSection == nullptr)
    {
        // This object gets cleaned-up by the Sections<> helper class, so we can just zero the
        // pointer here without worrying about leaks.
        m_pTextSection = nullptr;

        result = Result::ErrorOutOfMemory;
    }
    else
    {
        m_pTextSection->SetAlignment(PipelineShaderBaseAddrAlignment);
    }

    return result;
}

} // Abi
} // Util
