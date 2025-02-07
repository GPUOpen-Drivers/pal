/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palAssert.h"
#include "palHashLiteralString.h"
#include "palHashMapImpl.h"
#include "palHsaAbiMetadata.h"
#include "palMsgPackImpl.h"
#include "palPipelineAbiReader.h"
#include "palPipelineAbiUtils.h"
#include "palPipelineArFile.h"
#include "palVectorImpl.h"
#include "palElf.h"

#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace Util::Abi
{

// =====================================================================================================================
static bool MatchesAnySupportedAbi(
    uint8 osAbi,
    uint8 version)
{
    bool result = false;

    if (osAbi == ElfOsAbiAmdgpuPal)
    {
        result = (version == ElfAbiVersionAmdgpuPal);
    }
    else if (osAbi == ElfOsAbiAmdgpuHsa)
    {
        result = (version == ElfAbiVersionAmdgpuHsaV3) ||
                 (version == ElfAbiVersionAmdgpuHsaV4) ||
                 (version == ElfAbiVersionAmdgpuHsaV5);
    }

    return result;
}

// =====================================================================================================================
Result PipelineAbiReader::Init(
    StringView<char> kernelName)
{
    Result result = m_genericSymbolsMap.Init();
    memset(&m_pipelineSymbols[0], 0, sizeof(m_pipelineSymbols));

    if (result == Result::Success)
    {
        result = InitCodeObject();
    }

    if (result == Result::Success)
    {
        result = InitSymbolCache(kernelName);
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    InitDebugValidate();
#endif

    return result;
}

// =====================================================================================================================
Result PipelineAbiReader::InitCodeObject()
{
    Result result = Result::Success;

    // Handle single ELF vs archive-of-ELF case
    if (Elf::IsElf(m_binary))
    {
        result = m_elfReaders.PushBack({0, ElfReader::Reader(m_binary.Data())});
    }
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 906
    else if (m_binary.SizeInBytes() == size_t(-1))
    {
        // PipelineArFileReader requires an actual size!  (See back-compat PipelineAbiReader(const void*) constructor)
        result = Result::ErrorInvalidPipelineElf;
    }
#endif
    else if (Util::IsArFile(m_binary))
    {
        PipelineArFileReader reader(m_binary);
        for (auto member = reader.Begin(); (result == Result::Success) && (member.IsEnd() == false); member.Next())
        {
            const bool isElf = (member.IsMalformed() == false) && Elf::IsElf(member.GetData());
            result =   isElf ? m_elfReaders.PushBack({member.GetElfHash(), ElfReader::Reader(member.GetData())}) :
                               Result::ErrorInvalidPipelineElf;
        }
    }
    else
    {
        result = Result::ErrorInvalidPipelineElf;
    }

    // Validate ELF header OS and "ABI" versions.  These do not necessarily correspond with metadata version!
    if (result == Result::Success)
    {
        for (const auto& [elfHash, elfReader] : m_elfReaders)
        {
            const uint32 osAbi      = elfReader.GetHeader().ei_osabi;
            const uint32 abiVersion = elfReader.GetHeader().ei_abiversion;

            if ((MatchesAnySupportedAbi(osAbi, abiVersion) == false) ||
                (elfReader.GetTargetMachine() != Elf::MachineType::AmdGpu))
            {
                result = Result::ErrorInvalidPipelineElf;
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
void PipelineAbiReader::InitDebugValidate() const
{
    // Extra slow path, debug-only asserts for sanity checking that the file format is correct.
    bool hasNote   = false;
    bool hasSymbol = false;
    bool hasText   = false;

    for (const auto& [elfHash, elfReader] : m_elfReaders)
    {
        for (ElfReader::SectionId sectionIndex = 0; sectionIndex < elfReader.GetNumSections(); sectionIndex++)
        {
            const char* pSectionName = elfReader.GetSectionName(sectionIndex);
            ElfReader::SectionHeaderType sectionType = elfReader.GetSectionType(sectionIndex);

            if (StringEqualFunc<const char*>()(pSectionName, ".text"))
            {
                hasText = true;
            }
            else if (StringEqualFunc<const char*>()(pSectionName, ".note") ||
                     (sectionType == ElfReader::SectionHeaderType::Note))
            {
                hasNote = true;
            }
            else if ((sectionType == ElfReader::SectionHeaderType::SymTab) &&
                     (elfReader.GetSection(sectionIndex).sh_link != 0))
            {
                hasSymbol = true;
            }
        }
    }

    PAL_ASSERT_MSG(hasNote,   "Missing .note section");
    PAL_ASSERT_MSG(hasSymbol, "Missing .symtab section");
    PAL_ASSERT_MSG(hasText,   "Missing .text section");
}

// =====================================================================================================================
Result PipelineAbiReader::InitSymbolCache(
    StringView<char> kernelName)
{
    Result result = Result::Success;  // Assume m_genericSymbolsMap.Init() was already Success during early this->Init()

    // Cache symbols so we don't have to search them when looking up
    uint32 elfIdx = 0;
    for (const auto& [elfHash, elfReader] : m_elfReaders)
    {
        for (ElfReader::SectionId sectionIndex = 0; sectionIndex < elfReader.GetNumSections(); sectionIndex++)
        {
            if (elfReader.GetSectionType(sectionIndex) != ElfReader::SectionHeaderType::SymTab)
            {
                continue;
            }

            ElfReader::Symbols symbols(elfReader, sectionIndex);
            for (uint32 symbolIndex = 0; symbolIndex < symbols.GetNumSymbols(); symbolIndex++)
            {
                // We are not interested in symbol table entries of type SymbolTableEntryType::Section, since we use
                // m_genericSymbolsMap to look up function addresses. Moreover, they have no name in the symbol
                // table itself, so we cannot insert them in m_genericSymbolsMap (HashString asserts that its
                // argument is not the empty string).
                if ((symbols.GetSymbol(symbolIndex).st_shndx == 0) ||
                    (symbols.GetSymbolType(symbolIndex) == Elf::SymbolTableEntryType::Section))
                {
                    continue;
                }

                const char*const   pName              = symbols.GetSymbolName(symbolIndex);
                PipelineSymbolType pipelineSymbolType = PipelineSymbolType::Unknown;

                if (GetOsAbi() == ElfOsAbiAmdgpuHsa)
                {
                    // This table assumes the PAl ABI. That's not a big deal though if we assume there's a single
                    // function symbol in each HSA ABI elf that corresonds to the main function.
                    if (symbols.GetSymbolType(symbolIndex) == Elf::SymbolTableEntryType::Func)
                    {
                        // When there is only one kernel, kernelName can be empty
                        if (kernelName.IsEmpty() || (StringView<char>(pName) == kernelName))
                        {
                            pipelineSymbolType = PipelineSymbolType::CsMainEntry;
                        }
                        else
                        {
                            // skip not expected kernels
                            continue;
                        }
                    }
                }
                else
                {
                    pipelineSymbolType = GetSymbolTypeFromName(pName);
                }

                if (pipelineSymbolType != PipelineSymbolType::Unknown)
                {
                    // This will trigger if we try to map more than one symbol to the same spot in this table.
                    PAL_ASSERT(m_pipelineSymbols[static_cast<uint32>(pipelineSymbolType)].m_index == 0);

                    m_pipelineSymbols[static_cast<uint32>(pipelineSymbolType)] =
                        {sectionIndex, symbolIndex, elfIdx};
                }
                else
                {
                    result = m_genericSymbolsMap.Insert(
                        HashString(pName, strlen(pName)), {sectionIndex, symbolIndex, elfIdx});

                    if (result != Result::Success)
                    {
                        break;
                    }
                }
            }

            if (result != Result::Success)
            {
                break;
            }
        }

        ++elfIdx;
    }

    return result;
}

// =====================================================================================================================
Result PipelineAbiReader::GetMetadata(
    MsgPackReader*              pReader,
    PalAbi::CodeObjectMetadata* pMetadata
    ) const
{
    Result result = Result::Success;
    bool foundMetadata = false;

    memset(pMetadata, 0, sizeof(PalAbi::CodeObjectMetadata));

    //  Mark initial userEntries as not mapped
    for (uint32 stage = 0; stage < static_cast<uint32>(Abi::HardwareStage::Count); ++stage)
    {
        auto& stageMetadata = pMetadata->pipeline.hardwareStage[stage];

        memset(stageMetadata.userDataRegMap,
               uint32(Abi::UserDataMapping::NotMapped),
               sizeof(stageMetadata.userDataRegMap));
    }

    for (const auto& [elfHash, elfReader] : m_elfReaders)
    {
        for (ElfReader::SectionId sectionIndex = 0; sectionIndex < elfReader.GetNumSections(); sectionIndex++)
        {
            // Only the .note section has the right format.  Only one metadata .note section per ELF is valid.
            if ((elfReader.GetSectionType(sectionIndex) != Elf::SectionHeaderType::Note) ||
                !StringEqualFunc<const char*>()(elfReader.GetSectionName(sectionIndex), ".note"))
            {
                continue;
            }

            uint32 metadataMajorVer = 0;
            uint32 metadataMinorVer = 1;

            const void* pRawMetadata = nullptr;
            uint32      metadataSize = 0;

            ElfReader::Notes notes(elfReader, sectionIndex);
            ElfReader::NoteIterator notesEnd = notes.End();
            for (ElfReader::NoteIterator note = notes.Begin(); note.IsValid() && (pRawMetadata == nullptr); note.Next())
            {
                const void* pDesc    = note.GetDescriptor();
                uint32      descSize = note.GetHeader().n_descsz;

                switch (note.GetHeader().n_type)
                {
                case MetadataNoteType:
                {
                    pRawMetadata = pDesc;
                    metadataSize = descSize;

                    result =
                        PalAbi::GetPalMetadataVersion(pReader, pDesc, descSize, &metadataMajorVer, &metadataMinorVer);

                    break;
                }
                default:
                    // Unknown note type.
                    break;
                }
            }

            if ((result != Result::Success) || (pRawMetadata == nullptr))
            {
                break;
            }

            // Note: this may be called multiple times for multi-ELF code objects
            result = PalAbi::DeserializeCodeObjectMetadata(
                pReader, pMetadata, pRawMetadata, metadataSize, metadataMajorVer, metadataMinorVer);
            foundMetadata = true;
        }

        if (result != Result::Success)
        {
            break;
        }
    }

    if ((result == Result::Success) && (foundMetadata == false))
    {
        result = Result::ErrorInvalidPipelineElf;
    }

    return result;
}

// =====================================================================================================================
Result PipelineAbiReader::GetMetadata(
    MsgPackReader*              pReader,
    HsaAbi::CodeObjectMetadata* pMetadata,
    const StringView<char>      kernelName
    ) const
{
    Result result = Result::Success;
    bool   foundMetadata = false;

    const ElfReader::Reader& elfReader = m_elfReaders[0].reader;

    // We currently expect HSA code objects to only ever be a single ELF, never archives-of-elves.
    for (ElfReader::SectionId sectionIndex = 0; sectionIndex < elfReader.GetNumSections(); sectionIndex++)
    {
        // Only the .note section has the right format
        if ((elfReader.GetSectionType(sectionIndex) != Elf::SectionHeaderType::Note) ||
            !StringEqualFunc<const char*>()(elfReader.GetSectionName(sectionIndex), ".note"))
        {
            continue;
        }

        uint32 metadataMajorVer = 0;
        uint32 metadataMinorVer = 0;

        const void* pRawMetadata = nullptr;
        uint32      metadataSize = 0;

        ElfReader::Notes notes(elfReader, sectionIndex);

        for (ElfReader::NoteIterator note = notes.Begin(); note.IsValid(); note.Next())
        {
            const void* pDesc    = note.GetDescriptor();
            uint32      descSize = note.GetHeader().n_descsz;

            switch (note.GetHeader().n_type)
            {
            case MetadataNoteType:
            {
                pRawMetadata = pDesc;
                metadataSize = descSize;

                result = GetMetadataVersion(pReader, pDesc, descSize,
                                            HashLiteralString(HsaAbi::PipelineMetadataKey::Version),
                                            &metadataMajorVer, &metadataMinorVer);
                break;
            }
            default:
                // Unknown note type.
                break;
            }
        }

        if (result == Result::Success)
        {
            result = pMetadata->SetVersion(metadataMajorVer, metadataMinorVer);
        }

        if (result == Result::Success)
        {
            result = pMetadata->DeserializeNote(pReader, pRawMetadata, metadataSize, kernelName);
        }

        // Quit after the first .note section.
        foundMetadata = true;
        break;
    }

    if ((result == Result::Success) && (foundMetadata == false))
    {
        result = Result::ErrorInvalidPipelineElf;
    }

    return result;
}

// =====================================================================================================================
void PipelineAbiReader::GetGfxIpVersion(
    uint32* pGfxIpMajorVer,
    uint32* pGfxIpMinorVer,
    uint32* pGfxIpStepping
    ) const
{
    AmdGpuMachineType machineType = static_cast<AmdGpuMachineType>(GetElfReader().GetHeader().e_flags);
    MachineTypeToGfxIpVersion(machineType, pGfxIpMajorVer, pGfxIpMinorVer, pGfxIpStepping);
}

// =====================================================================================================================
const SymbolEntry* PipelineAbiReader::FindSymbol(
    PipelineSymbolType pipelineSymbolType
    ) const
{
    const SymbolEntry* pSymbolEntry = nullptr;

    if (pipelineSymbolType < PipelineSymbolType::Count)
    {
        const SymbolEntry& entry = m_pipelineSymbols[static_cast<uint32>(pipelineSymbolType)];
        if (entry.m_section != 0)
        {
            pSymbolEntry = &entry;
        }
    }

    return pSymbolEntry;
}

// =====================================================================================================================
const SymbolEntry* PipelineAbiReader::FindSymbol(
    StringView<char> name
    ) const
{
    const SymbolEntry*const pSymbolEntry = m_genericSymbolsMap.FindKey(HashString(name));
    return ((pSymbolEntry != nullptr) && (pSymbolEntry->m_section != 0)) ? pSymbolEntry : nullptr;
}

// =====================================================================================================================
Span<const void> PipelineAbiReader::GetSymbol(
    const SymbolEntry* pSymbolEntry
    ) const
{
    const void* pData = nullptr;
    size_t      size  = 0;

    const Elf::SymbolTableEntry*const pElfSymbol = GetSymbolHeader(pSymbolEntry);

    if (pElfSymbol != nullptr)
    {
        const ElfReader::Reader& elfReader = GetElfReader(pSymbolEntry->m_elfIndex);
        const Result result = elfReader.GetSymbol(*pElfSymbol, &pData);
        size = (result == Result::Success) ? pElfSymbol->st_size : 0;
        PAL_ASSERT_MSG(result == Result::Success,  "How did we get here if pSymbolEntry != nullptr?!");
    }

    return Span<const void>(pData, size);
}

// =====================================================================================================================
Result PipelineAbiReader::CopySymbol(
    const SymbolEntry* pSymbolEntry,
    size_t*            pSize,
    void*              pBuffer
    ) const
{
    Result result = (pSymbolEntry != nullptr) ? Result::Success : Result::NotFound;

    if (result == Result::Success)
    {
        const auto& elfReader = m_elfReaders[pSymbolEntry->m_elfIndex].reader;
        result = elfReader.CopySymbol(*GetSymbolHeader(pSymbolEntry), pSize, pBuffer);
    }

    return result;
}

// =====================================================================================================================
const Elf::SymbolTableEntry* PipelineAbiReader::GetSymbolHeader(
    const SymbolEntry* pSymbolEntry
    ) const
{
    const Elf::SymbolTableEntry* pElfSymbolHeader = nullptr;

    if (pSymbolEntry != nullptr)
    {
        const ElfReader::Reader& elfReader = GetElfReader(pSymbolEntry->m_elfIndex);
        ElfReader::Symbols symbolSection(elfReader, pSymbolEntry->m_section);
        pElfSymbolHeader = &symbolSection.GetSymbol(pSymbolEntry->m_index);
    }

    return pElfSymbolHeader;
}

} // Util::Abi
