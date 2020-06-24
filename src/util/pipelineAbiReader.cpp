/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palMsgPackImpl.h"
#include "palPipelineAbiReader.h"
#include "palPipelineAbiUtils.h"
#include "palHashMapImpl.h"

#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace Util
{
namespace Abi
{

// =====================================================================================================================
// Reverses the bytes on a big-endian machine. This converts an u32 from big to little endian or the other way around.
static uint32 ChangeHostDeviceOrder32(
    uint32 value)
{
#if defined(LITTLEENDIAN_CPU)
    // No reversal is necessary on little-endian architecture.
    return value;
#else
    return ((value & 0xff) << 24)
        | (((value >> 8) & 0xff) << 16)
        | (((value >> 16) & 0xff) << 8)
        | ((value >> 24) & 0xff);
#endif
}

// =====================================================================================================================
// Reverses the bytes on a big-endian machine. This converts an u64 from big to little endian or the other way around.
static uint64 ChangeHostDeviceOrder64(
    uint64 value)
{
#if defined(LITTLEENDIAN_CPU)
    // No reversal is necessary on little-endian architecture.
    return value;
#else
    return ((value & 0xff) << 56)
        | (((value >> 8) & 0xff) << 48)
        | (((value >> 16) & 0xff) << 40)
        | (((value >> 24) & 0xff) << 32)
        | (((value >> 32) & 0xff) << 24)
        | (((value >> 40) & 0xff) << 16)
        | (((value >> 48) & 0xff) << 8)
        | ((value >> 56) & 0xff);
#endif
}

// =====================================================================================================================
Result PipelineAbiReader::Init()
{
    Result result = Result::Success;

    if ((m_elfReader.GetHeader().ei_osabi != ElfOsAbiVersion) ||
        (m_elfReader.GetTargetMachine() != Elf::MachineType::AmdGpu) ||
        (m_elfReader.GetHeader().ei_abiversion != ElfAbiVersion))
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

                const char* pName = symbols.GetSymbolName(symbolIndex);
                PipelineSymbolType pipelineSymbolType = GetSymbolTypeFromName(pName);

                if (pipelineSymbolType != PipelineSymbolType::Unknown)
                {
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
    MsgPackReader*         pReader,
    PalCodeObjectMetadata* pMetadata
    ) const
{
    Result result = Result::Success;
    bool foundMetadata = false;

    memset(pMetadata, 0, sizeof(PalCodeObjectMetadata));

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

            switch (static_cast<PipelineAbiNoteType>(note.GetHeader().n_type))
            {
            case PipelineAbiNoteType::PalMetadata:
            {
                pRawMetadata = pDesc;
                metadataSize = descSize;

                result = GetPalMetadataVersion(pReader, pDesc, descSize, &metadataMajorVer, &metadataMinorVer);

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

        result = DeserializePalCodeObjectMetadata(pReader, pMetadata, pRawMetadata, metadataSize,
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

} // Abi
} // Util
