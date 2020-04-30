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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 477
            case PipelineAbiNoteType::PalMetadataOld:
#endif
            {
                pRawMetadata = pDesc;
                metadataSize = descSize;

                // We need to retrieve version info from the msgpack blob.
                result = pReader->InitFromBuffer(pDesc, static_cast<uint32>(descSize));

                if ((result == Result::Success) && (pReader->Type() != CWP_ITEM_MAP))
                {
                    result = Result::ErrorInvalidPipelineElf;
                    break;
                }

                for (uint32 j = pReader->Get().as.map.size; ((result == Result::Success) && (j > 0)); --j)
                {
                    result = pReader->Next(CWP_ITEM_STR);

                    if (result == Result::Success)
                    {
                        const auto&  str     = pReader->Get().as.str;
                        const uint32 keyHash = HashString(static_cast<const char*>(str.start), str.length);
                        if (keyHash == HashLiteralString(PalCodeObjectMetadataKey::Version))
                        {
                            result = pReader->Next(CWP_ITEM_ARRAY);
                            if ((result == Result::Success) && (pReader->Get().as.array.size >= 2))
                            {
                                result = pReader->UnpackNext(&metadataMajorVer);
                            }
                            if (result == Result::Success)
                            {
                                result = pReader->UnpackNext(&metadataMinorVer);
                            }
                            break;
                        }
                        else
                        {
                            // Ideally, the version is the first field written so we don't reach here.
                            result = pReader->Skip(1);
                        }
                    }
                    else
                    {
                        break;
                    }
                }

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

        if (metadataMajorVer == PipelineMetadataMajorVersion)
        {
            result = pReader->InitFromBuffer(pRawMetadata, metadataSize);
            uint32 registersOffset = UINT_MAX;

            if (result == Result::Success)
            {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 580
                result = Metadata::DeserializePalCodeObjectMetadata(pReader, pMetadata, &registersOffset);
#else
                result = Metadata::DeserializePalCodeObjectMetadata(pReader, pMetadata);
#endif
            }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 580
            if (result == Result::Success)
            {
                result = pReader->Seek(registersOffset);
            }
#endif
        }
        else
        {
            result = Result::ErrorUnsupportedPipelineElfAbiVersion;
        }
        // Quit after the first .note section
        break;
    }

    return result;
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
