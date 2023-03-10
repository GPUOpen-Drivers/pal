/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
        result = (version == ElfAbiVersionAmdgpuHsaV3) || (version == ElfAbiVersionAmdgpuHsaV4);
    }

    return result;
}

// =====================================================================================================================
Result PipelineAbiReader::Init()
{
    Result result = Result::Success;

    if ((MatchesAnySupportedAbi(GetOsAbi(), GetAbiVersion()) == false) ||
        (m_elfReader.GetTargetMachine() != Elf::MachineType::AmdGpu))
    {
        result = Result::ErrorInvalidPipelineElf;
    }

    if (result == Result::Success)
    {
        memset(&m_pipelineSymbols, 0, sizeof(m_pipelineSymbols));
        result = m_genericSymbolsMap.Init();
    }

    if (result == Result::Success)
    {
        // Cache symbols so we don't have to search them when looking up
        for (ElfReader::SectionId sectionIndex = 0; sectionIndex < m_elfReader.GetNumSections(); sectionIndex++)
        {
            if (m_elfReader.GetSectionType(sectionIndex) != ElfReader::SectionHeaderType::SymTab)
            {
                continue;
            }

            ElfReader::Symbols symbols(m_elfReader, sectionIndex);
            for (uint32 symbolIndex = 0; symbolIndex < symbols.GetNumSymbols(); symbolIndex++)
            {
                if (symbols.GetSymbol(symbolIndex).st_shndx == 0)
                {
                    continue;
                }

                const char*const   pName = symbols.GetSymbolName(symbolIndex);
                PipelineSymbolType pipelineSymbolType = PipelineSymbolType::Unknown;

                if (GetOsAbi() == ElfOsAbiAmdgpuHsa)
                {
                    // This table assumes the PAl ABI. That's not a big deal though if we assume there's a single
                    // function symbol in each HSA ABI elf that corresonds to the main function.
                    if (symbols.GetSymbolType(symbolIndex) == Elf::SymbolTableEntryType::Func)
                    {
                        pipelineSymbolType = PipelineSymbolType::CsMainEntry;
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

                    m_pipelineSymbols[static_cast<uint32>(pipelineSymbolType)] = {sectionIndex, symbolIndex};
                }
                else
                {
                    result = m_genericSymbolsMap.Insert(pName, {sectionIndex, symbolIndex});

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

    for (ElfReader::SectionId sectionIndex = 0; sectionIndex < m_elfReader.GetNumSections(); sectionIndex++)
    {
        // Only the .note section has the right format
        if ((m_elfReader.GetSectionType(sectionIndex) != Elf::SectionHeaderType::Note) ||
            !StringEqualFunc<const char*>()(m_elfReader.GetSectionName(sectionIndex), ".note"))
        {
            continue;
        }

        uint32 metadataMajorVer = 0;
        uint32 metadataMinorVer = 1;

        const void* pRawMetadata = nullptr;
        uint32      metadataSize = 0;

        ElfReader::Notes notes(m_elfReader, sectionIndex);
        ElfReader::NoteIterator notesEnd = notes.End();
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

                result = PalAbi::GetPalMetadataVersion(pReader, pDesc, descSize, &metadataMajorVer, &metadataMinorVer);

                break;
            }
            default:
                // Unknown note type.
                break;
            }
        }

        if (result != Result::Success)
        {
            break;
        }
        result = PalAbi::DeserializeCodeObjectMetadata(pReader, pMetadata, pRawMetadata, metadataSize,
            metadataMajorVer, metadataMinorVer);
        foundMetadata = true;

        // Quit after the first .note section
        break;
    }

    if (result == Result::Success && !foundMetadata)
    {
        result = Result::ErrorInvalidPipelineElf;
    }

    return result;
}

// =====================================================================================================================
Result PipelineAbiReader::GetMetadata(
    MsgPackReader*              pReader,
    HsaAbi::CodeObjectMetadata* pMetadata
    ) const
{
    Result result = Result::Success;
    bool   foundMetadata = false;

    for (ElfReader::SectionId sectionIndex = 0; sectionIndex < m_elfReader.GetNumSections(); sectionIndex++)
    {
        // Only the .note section has the right format
        if ((m_elfReader.GetSectionType(sectionIndex) != Elf::SectionHeaderType::Note) ||
            !StringEqualFunc<const char*>()(m_elfReader.GetSectionName(sectionIndex), ".note"))
        {
            continue;
        }

        uint32 metadataMajorVer = 0;
        uint32 metadataMinorVer = 0;

        const void* pRawMetadata = nullptr;
        uint32      metadataSize = 0;

        ElfReader::Notes notes(m_elfReader, sectionIndex);

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
            result = pMetadata->DeserializeNote(pReader, pRawMetadata, metadataSize);
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
const Elf::SymbolTableEntry* PipelineAbiReader::GetPipelineSymbol(
    PipelineSymbolType pipelineSymbolType
    ) const
{
    const SymbolEntry index = m_pipelineSymbols[static_cast<uint32>(pipelineSymbolType)];
    const bool pipelineSymbolEntryExists = index.m_section != 0;
    const Elf::SymbolTableEntry* pSymbol = nullptr;
    if (pipelineSymbolEntryExists)
    {
        ElfReader::Symbols symbolSection(m_elfReader, index.m_section);
        pSymbol = &symbolSection.GetSymbol(index.m_index);
    }

    return pSymbol;
}

// =====================================================================================================================
const Elf::SymbolTableEntry* PipelineAbiReader::GetGenericSymbol(
    const char* pName
    ) const
{
    PAL_ASSERT(pName != nullptr);

    const SymbolEntry*const pSymbolEntry = m_genericSymbolsMap.FindKey(pName);

    const Elf::SymbolTableEntry* pSymbol = nullptr;
    if (pSymbolEntry != nullptr)
    {
        ElfReader::Symbols symbolSection(m_elfReader, pSymbolEntry->m_section);
        pSymbol = &symbolSection.GetSymbol(pSymbolEntry->m_index);
    }

    return pSymbol;
}
} // Util::Abi
