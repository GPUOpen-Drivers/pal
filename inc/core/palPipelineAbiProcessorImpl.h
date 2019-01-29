/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palMetroHash.h"
#include "palMsgPackImpl.h"
#include "palInlineFuncs.h"
#include "palHashLiteralString.h"
#include "g_palPipelineAbiMetadataImpl.h"

namespace Util
{
namespace Abi
{

constexpr size_t AbiAmdGpuVersionNoteSize = sizeof(AbiAmdGpuVersionNote);
constexpr size_t AbiMinorVersionNoteSize  = sizeof(AbiMinorVersionNote);
constexpr size_t PalMetadataNoteEntrySize = sizeof(PalMetadataNoteEntry);

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
    m_flags(),
    m_metadataMajorVer(0),
    m_metadataMinorVer(1),
    m_compatRegisterSize(0),
    m_pCompatRegisterBlob(nullptr),
    m_pMetadata(nullptr),
    m_metadataSize(0),
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 432
    m_gpuVersionNote(),
    m_abiMinorVersionNote(),
    m_registerMap(128, pAllocator),
    m_pipelineMetadataVector(pAllocator),
    m_pipelineMetadataIndices(),
#endif
    m_genericSymbolsMap(16u, pAllocator),
    m_pipelineSymbolsVector(pAllocator),
    m_pipelineSymbolIndices(),
    m_elfProcessor(pAllocator),
    m_pAllocator(pAllocator)
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 432
    for (uint32 i = 0; i < static_cast<uint32>(PipelineMetadataType::Count); i++)
    {
        m_pipelineMetadataIndices[i] = -1;
    }
#endif

    for (uint32 i = 0; i < static_cast<uint32>(PipelineSymbolType::Count); i++)
    {
        m_pipelineSymbolIndices[i] = -1;
    }
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::Init()
{
    Result result = m_elfProcessor.Init();

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 432
    if (result == Result::Success)
    {
        result = m_registerMap.Init();
    }
#endif

    if (result == Result::Success)
    {
        result = m_genericSymbolsMap.Init();
    }

    if (result == Result::Success)
    {
        m_elfProcessor.SetOsAbi(ElfOsAbiVersion);

        m_elfProcessor.SetAbiVersion(ElfAbiVersion);

        m_elfProcessor.SetObjectFileType(Elf::ObjectFileType::Rel);
        m_elfProcessor.SetTargetMachine(Elf::MachineType::AmdGpu);

        m_metadataMajorVer = PipelineMetadataMajorVersion;
        m_metadataMinorVer = PipelineMetadataMinorVersion;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 432
        m_abiMinorVersionNote = { PipelineMetadataMinorVersion };
#endif
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
void PipelineAbiProcessor<Allocator>::SetGfxIpVersion(
    uint32 gfxIpMajorVer,
    uint32 gfxIpMinorVer,
    uint32 gfxIpStepping)
{
    switch (gfxIpMajorVer)
    {
    case 6:
        m_flags.machineType = static_cast<AmdGpuMachineType>(static_cast<uint32>(AmdGpuMachineType::Gfx600) +
                                                             gfxIpStepping);
        break;
    case 7:
        m_flags.machineType = static_cast<AmdGpuMachineType>(static_cast<uint32>(AmdGpuMachineType::Gfx700) +
                                                             gfxIpStepping);
        break;
    case 8:
        m_flags.machineType = (gfxIpMinorVer > 0) ? AmdGpuMachineType::Gfx810 :
            static_cast<AmdGpuMachineType>(static_cast<uint32>(AmdGpuMachineType::Gfx801) + gfxIpStepping - 1);
        break;
#if PAL_BUILD_GFX9
    case 9:
        switch (gfxIpStepping)
        {
        case GfxIpSteppingVega10:
            m_flags.machineType = AmdGpuMachineType::Gfx900;
            break;
        case GfxIpSteppingRaven:
            m_flags.machineType = AmdGpuMachineType::Gfx902;
            break;
        case GfxIpSteppingVega12:
            m_flags.machineType = AmdGpuMachineType::Gfx904;
            break;
        case GfxIpSteppingVega20:
            m_flags.machineType = AmdGpuMachineType::Gfx906;
            break;
        case 9:
            m_flags.machineType = AmdGpuMachineType::Gfx909;
            break;
        }
        break;
#endif
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 432
    m_gpuVersionNote.gfxipMajorVer = gfxIpMajorVer;
    m_gpuVersionNote.gfxipMinorVer = gfxIpMinorVer;
    m_gpuVersionNote.gfxipStepping = gfxIpStepping;
#endif
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
Result PipelineAbiProcessor<Allocator>::GetMetadata(
    MsgPackReader*         pReader,
    PalCodeObjectMetadata* pMetadata
    ) const
{
    Result result = Result::ErrorInvalidPipelineElf;

    if (m_pMetadata != nullptr)
    {
        memset(pMetadata, 0, sizeof(*pMetadata));

        if (m_metadataMajorVer == 0)
        {
            result = TranslateLegacyMetadata(pReader, pMetadata);
        }
        else if (m_metadataMajorVer == PipelineMetadataMajorVersion)
        {
            result = pReader->InitFromBuffer(m_pMetadata, static_cast<uint32>(m_metadataSize));
            uint32 registersOffset = UINT_MAX;

            if (result == Result::Success)
            {
                result = Metadata::DeserializePalCodeObjectMetadata(pReader, pMetadata, &registersOffset);
            }

            if (result == Result::Success)
            {
                result = pReader->Seek(registersOffset);
            }
        }
        else
        {
            result = Result::ErrorUnsupportedPipelineElfAbiVersion;
        }
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
void PipelineAbiProcessor<Allocator>::GetMetadata(
    const void** ppMetadata,
    size_t*      pMetadataSize
    ) const
{
    *ppMetadata    = m_pMetadata;
    *pMetadataSize = m_metadataSize;
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
Result PipelineAbiProcessor<Allocator>::AddGenericSymbolEntry(
    GenericSymbolEntry entry)
{
    PAL_ASSERT(entry.pName != nullptr);
    return m_genericSymbolsMap.Insert(entry.pName, entry);
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 432
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
#endif

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
bool PipelineAbiProcessor<Allocator>::HasGenericSymbolEntry(
    const char*         pName,
    GenericSymbolEntry* pGenericSymbolEntry
    ) const
{
    PAL_ASSERT(pName != nullptr);

    GenericSymbolEntry*const pEntry = m_genericSymbolsMap.FindKey(pName);
    if (pEntry != nullptr)
    {
        (*pGenericSymbolEntry) = (*pEntry);
    }

    return (pEntry != nullptr);
}

// =====================================================================================================================
template <typename Allocator>
void PipelineAbiProcessor<Allocator>::GetGfxIpVersion(
    uint32* pGfxIpMajorVer,
    uint32* pGfxIpMinorVer,
    uint32* pGfxIpStepping
    ) const
{
    switch (m_flags.machineType)
    {
    case AmdGpuMachineType::Gfx600:
        *pGfxIpMajorVer = 6;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 0;
        break;
    case AmdGpuMachineType::Gfx601:
        *pGfxIpMajorVer = 6;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 1;
        break;
    case AmdGpuMachineType::Gfx700:
        *pGfxIpMajorVer = 7;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 0;
        break;
    case AmdGpuMachineType::Gfx701:
        *pGfxIpMajorVer = 7;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 1;
        break;
    case AmdGpuMachineType::Gfx702:
        *pGfxIpMajorVer = 7;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 2;
        break;
    case AmdGpuMachineType::Gfx703:
        *pGfxIpMajorVer = 7;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 3;
        break;
    case AmdGpuMachineType::Gfx704:
        *pGfxIpMajorVer = 7;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 4;
        break;
    case AmdGpuMachineType::Gfx800:
        *pGfxIpMajorVer = 8;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 0;
        break;
    case AmdGpuMachineType::Gfx801:
        *pGfxIpMajorVer = 8;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 1;
        break;
    case AmdGpuMachineType::Gfx802:
        *pGfxIpMajorVer = 8;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 2;
        break;
    case AmdGpuMachineType::Gfx803:
        *pGfxIpMajorVer = 8;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 3;
        break;
    case AmdGpuMachineType::Gfx810:
        *pGfxIpMajorVer = 8;
        *pGfxIpMinorVer = 1;
        *pGfxIpStepping = 0;
        break;
#if PAL_BUILD_GFX9
    case AmdGpuMachineType::Gfx900:
        *pGfxIpMajorVer = 9;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 0;
        break;
    case AmdGpuMachineType::Gfx902:
        *pGfxIpMajorVer = 9;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 2;
        break;
    case AmdGpuMachineType::Gfx904:
        *pGfxIpMajorVer = 9;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 4;
        break;
    case AmdGpuMachineType::Gfx906:
        *pGfxIpMajorVer = 9;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 6;
        break;
    case AmdGpuMachineType::Gfx909:
        *pGfxIpMajorVer = 9;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 9;
        break;
#endif
    default:
        // What is this?
        PAL_ASSERT_ALWAYS();
        *pGfxIpMajorVer = 0;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 0;
        break;
    }

    return;
}

// =====================================================================================================================
template <typename Allocator>
void PipelineAbiProcessor<Allocator>::GetMetadataVersion(
    uint32* pMajorVer,
    uint32* pMinorVer
    ) const
{
    *pMajorVer = m_metadataMajorVer;
    *pMinorVer = m_metadataMinorVer;
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 432
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::Finalize(
    const MsgPackWriter& pipelineMetadataWriter)
#else
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::Finalize(
    const char* pPipelineName)
#endif
{
    Result result = Result::Success;

    m_elfProcessor.SetFlags(m_flags.u32All);

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

        for (auto iter = PipelineSymbolsBegin(); iter.IsValid(); iter.Next())
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
        } // for each pipeline symbol

        for (auto iter = m_genericSymbolsMap.Begin(); iter.Get() != nullptr; iter.Next())
        {
            Abi::GenericSymbolEntry symbol = iter.Get()->value;

            uint32 sectionIndex = 0;
            switch (symbol.sectionType)
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

            if (symbolProcessor.Add(symbol.pName,
                                    Elf::SymbolTableEntryBinding::Local,
                                    symbol.entryType,
                                    static_cast<uint16>(sectionIndex),
                                    symbol.value,
                                    symbol.size) == UINT_MAX)
            {
                result = Result::ErrorOutOfMemory;
                break;
            }
        } // for each generic symbol

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 432
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
#endif
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 432
            MsgPackWriter codeObjectMetadataWriter(m_pAllocator);
            codeObjectMetadataWriter.Reserve(pipelineMetadataWriter.GetSize());
            codeObjectMetadataWriter.DeclareMap(2);

            codeObjectMetadataWriter.Pack(PalCodeObjectMetadataKey::Version);
            codeObjectMetadataWriter.DeclareArray(2);
            codeObjectMetadataWriter.PackPair(m_metadataMajorVer, m_metadataMinorVer);

            codeObjectMetadataWriter.Pack(PalCodeObjectMetadataKey::Pipelines);
            codeObjectMetadataWriter.DeclareArray(1);
            codeObjectMetadataWriter.DeclareMap(pipelineMetadataWriter.NumItems() / 2);
            result = codeObjectMetadataWriter.Append(pipelineMetadataWriter);

            if ((result == Result::Success) &&
                (noteProcessor.Add(MetadataNoteType,
                                   AmdGpuArchName,
                                   codeObjectMetadataWriter.GetBuffer(),
                                   codeObjectMetadataWriter.GetSize()) == UINT_MAX))
            {
                result = Result::ErrorOutOfMemory;
            }
#else
            if ((noteProcessor.Add(static_cast<uint32>(PipelineAbiNoteType::HsaIsa),
                                   AmdGpuArchName,
                                   &m_gpuVersionNote,
                                   AbiAmdGpuVersionNoteSize) == UINT_MAX) ||
                (noteProcessor.Add(static_cast<uint32>(PipelineAbiNoteType::AbiMinorVersion),
                                   AmdGpuArchName,
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

                    if (noteProcessor.Add(static_cast<uint32>(PipelineAbiNoteType::LegacyMetadata),
                                          AmdGpuArchName,
                                          &entries[0],
                                          (noteEntryCount * sizeof(PalMetadataNoteEntry))) != UINT_MAX)
                    {
                        result = Result::Success;
                    }
                }
            }
#endif
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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 432
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
#endif

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
    Result result = m_elfProcessor.LoadFromBuffer(pBuffer, bufferSize);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 432
    if (result == Result::Success)
    {
         result = m_registerMap.Init();
    }
#endif

    if (result == Result::Success)
    {
         result = m_genericSymbolsMap.Init();
    }

    if (result == Result::Success)
    {
        if ((m_elfProcessor.GetFileHeader()->ei_osabi != ElfOsAbiVersion) ||
            (m_elfProcessor.GetTargetMachine()        != Elf::MachineType::AmdGpu))
        {
            result = Result::ErrorInvalidPipelineElf;
        }
        else if (m_elfProcessor.GetFileHeader()->ei_abiversion != ElfAbiVersion)
        {
            result = Result::ErrorUnsupportedPipelineElfAbiVersion;
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

        m_flags.u32All         = m_elfProcessor.GetFileHeader()->e_flags;

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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 432
        Vector<PalMetadataNoteEntry, 16, Allocator> legacyRegisters(m_pAllocator);
#endif

        result = noteProcessor.Init();
        for (uint32 i = 0; ((result == Result::Success) && (i < noteProcessor.GetNumNotes())); ++i)
        {
            uint32 type;
            const char* pName;
            const void* pDesc;
            size_t descSize;

            noteProcessor.Get(i, &type, &pName, &pDesc, &descSize);

            switch (static_cast<PipelineAbiNoteType>(type))
            {
            case PipelineAbiNoteType::PalMetadata:
            {
                m_pMetadata    = pDesc;
                m_metadataSize = descSize;

                // We need to retrieve version info from the msgpack blob.
                MsgPackReader reader;
                result = reader.InitFromBuffer(pDesc, static_cast<uint32>(descSize));

                if ((result == Result::Success) && (reader.Type() != CWP_ITEM_MAP))
                {
                    result = Result::ErrorInvalidPipelineElf;
                }

                for (uint32 j = reader.Get().as.map.size; ((result == Result::Success) && (j > 0)); --j)
                {
                    result = reader.Next(CWP_ITEM_STR);

                    if (result == Result::Success)
                    {
                        const auto&  str     = reader.Get().as.str;
                        const uint32 keyHash = HashString(static_cast<const char*>(str.start), str.length);
                        if (keyHash == HashLiteralString(PalCodeObjectMetadataKey::Version))
                        {
                            result = reader.Next(CWP_ITEM_ARRAY);
                            if ((result == Result::Success) && (reader.Get().as.array.size >= 2))
                            {
                                result = reader.UnpackNext(&m_metadataMajorVer);
                            }
                            if (result == Result::Success)
                            {
                                result = reader.UnpackNext(&m_metadataMinorVer);
                            }
                            break;
                        }
                        else
                        {
                            // Ideally, the version is the first field written so we don't reach here.
                            result = reader.Skip(1);
                        }
                    }
                }

                break;
            }

            // Handle legacy note types:
            case PipelineAbiNoteType::HsaIsa:
            {
                PAL_ASSERT(descSize >= AbiAmdGpuVersionNoteSize);
                const auto*const pNote = static_cast<const AbiAmdGpuVersionNote*>(pDesc);
                SetGfxIpVersion(pNote->gfxipMajorVer, pNote->gfxipMinorVer, pNote->gfxipStepping);
                break;
            }
            case PipelineAbiNoteType::AbiMinorVersion:
            {
                PAL_ASSERT(descSize == AbiMinorVersionNoteSize);
                const auto*const pNote = static_cast<const AbiMinorVersionNote*>(pDesc);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 432
                m_abiMinorVersionNote = *pNote;
#endif
                m_metadataMajorVer = m_elfProcessor.GetFileHeader()->ei_abiversion;
                m_metadataMinorVer = pNote->minorVersion;
                break;
            }
            case PipelineAbiNoteType::LegacyMetadata:
            {
                PAL_ASSERT(descSize % PalMetadataNoteEntrySize == 0);
                m_pMetadata    = pDesc;
                m_metadataSize = descSize;

                const PalMetadataNoteEntry* pMetadataEntryReader =
                    static_cast<const PalMetadataNoteEntry*>(pDesc);

                while ((result == Result::Success) && (VoidPtrDiff(pMetadataEntryReader, pDesc) < descSize))
                {
                    if (pMetadataEntryReader->key < 0x10000000)
                    {
                        // Entry is a RegisterEntry
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 432
                        result = legacyRegisters.PushBack(*pMetadataEntryReader);
#else
                        result = AddRegisterEntry(*pMetadataEntryReader);
                    }
                    else
                    {
                        // Entry is a PipelineMetadataEntry
                        PipelineMetadataEntry entry;

                        entry.key = static_cast<PipelineMetadataType>(pMetadataEntryReader->key & 0x0FFFFFFF);
                        entry.value = pMetadataEntryReader->value;

                        PAL_ASSERT((entry.key >= PipelineMetadataType::ApiCsHashDword0) &&
                                   (entry.key <  PipelineMetadataType::Count));

                        result = AddPipelineMetadataEntry(entry);
#endif
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

        if (m_pCompatRegisterBlob != nullptr)
        {
            PAL_SAFE_FREE(m_pCompatRegisterBlob, m_pAllocator);
        }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 432
        if ((result == Result::Success) && (legacyRegisters.NumElements() > 0))
        {
            // 1-3 bytes for map declaration + 5 bytes per key + 5 bytes per value
            const uint32 allocSize = (3 + (10 * legacyRegisters.NumElements()) + 1);
            m_pCompatRegisterBlob = PAL_MALLOC(allocSize, m_pAllocator, AllocInternal);

            MsgPackWriter registerWriter(m_pCompatRegisterBlob, allocSize);
            result = registerWriter.DeclareMap(legacyRegisters.NumElements());

            for (auto iter = legacyRegisters.Begin(); ((result == Result::Success) && iter.IsValid()); iter.Next())
            {
                const auto& entry = iter.Get();
                result = registerWriter.PackPair(entry.key, entry.value);
            }

            if (result == Result::Success)
            {
                m_compatRegisterSize = registerWriter.GetSize();
            }
        }
#else
        if ((result == Result::Success) && (m_registerMap.GetNumEntries() > 0))
        {
            // 1-3 bytes for map declaration + 5 bytes per key + 5 bytes per value
            const uint32 allocSize = (3 + (10 * m_registerMap.GetNumEntries()) + 1);
            m_pCompatRegisterBlob = PAL_MALLOC(allocSize, m_pAllocator, AllocInternal);

            MsgPackWriter registerWriter(m_pCompatRegisterBlob, allocSize);
            result = registerWriter.DeclareMap(m_registerMap.GetNumEntries());

            for (auto iter = RegistersBegin(); ((result == Result::Success) && (iter.Get() != nullptr)); iter.Next())
            {
                const auto& entry = *reinterpret_cast<PalMetadataNoteEntry*>(&((*iter.Get()).value));
                result = registerWriter.PackPair(entry.key, entry.value);
            }

            if (result == Result::Success)
            {
                m_compatRegisterSize = registerWriter.GetSize();
            }
        }
#endif
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

            const PipelineSymbolType pipelineSymbolType = GetSymbolTypeFromName(pName);
            if (pipelineSymbolType != PipelineSymbolType::Unknown)
            {
                result = AddPipelineSymbolEntry({pipelineSymbolType, type, sectionType, value, size});
            }
            else
            {
                result = AddGenericSymbolEntry({pName, type, sectionType, value, size});
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

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::TranslateLegacyMetadata(
    MsgPackReader*         pReader,
    PalCodeObjectMetadata* pOut
    ) const
{
    PAL_ASSERT(m_metadataMajorVer == 0);

    Result result = Result::Success;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 432
    Vector<PipelineMetadataEntry, 16, Allocator> metadata(m_pAllocator);
    int32 indices[static_cast<uint32>(PipelineMetadataType::Count)];
    for (uint32 i = 0; i < static_cast<uint32>(PipelineMetadataType::Count); i++)
    {
        indices[i] = -1;
    }

    const PalMetadataNoteEntry* pMetadataEntryReader = static_cast<const PalMetadataNoteEntry*>(m_pMetadata);

    while ((result == Result::Success) && (VoidPtrDiff(pMetadataEntryReader, m_pMetadata) < m_metadataSize))
    {
        if (pMetadataEntryReader->key >= 0x10000000)
        {
            // Entry is a PipelineMetadataEntry
            PipelineMetadataEntry entry;

            entry.key = static_cast<PipelineMetadataType>(pMetadataEntryReader->key & 0x0FFFFFFF);
            entry.value = pMetadataEntryReader->value;

            PAL_ASSERT((entry.key >= PipelineMetadataType::ApiCsHashDword0) &&
                       (entry.key <  PipelineMetadataType::Count));

            result = metadata.PushBack(entry);

            if (result == Result::Success)
            {
                indices[static_cast<uint32>(entry.key)] = metadata.NumElements() - 1;
            }
        }

        pMetadataEntryReader++;
    }
#else
    const auto& metadata = m_pipelineMetadataVector;
    const auto& indices  = m_pipelineMetadataIndices;
#endif

    if (result == Result::Success)
    {
        result = pReader->InitFromBuffer(m_pCompatRegisterBlob, m_compatRegisterSize);
    }

    pOut->version[0] = m_metadataMajorVer; // 0
    pOut->version[1] = m_metadataMinorVer; // 1
    pOut->hasEntry.version = 1;

    // Translate pipeline metadata.
    pOut->pipeline.internalPipelineHash[0] = 0;
    pOut->pipeline.internalPipelineHash[1] = 0;
    uint32 type = static_cast<uint32>(PipelineMetadataType::InternalPipelineHashDword0);
    if (indices[type] != -1)
    {
        pOut->pipeline.internalPipelineHash[0] |= static_cast<uint64>(metadata.At(indices[type]).value);
        pOut->pipeline.hasEntry.internalPipelineHash = 1;
    }

    type = static_cast<uint32>(PipelineMetadataType::InternalPipelineHashDword1);
    if (indices[type] != -1)
    {
        pOut->pipeline.internalPipelineHash[0] |= (static_cast<uint64>(metadata.At(indices[type]).value) << 32);
        pOut->pipeline.hasEntry.internalPipelineHash = 1;
    }

    type = static_cast<uint32>(PipelineMetadataType::InternalPipelineHashDword2);
    if (indices[type] != -1)
    {
        pOut->pipeline.internalPipelineHash[1] |= static_cast<uint64>(metadata.At(indices[type]).value);
        pOut->pipeline.hasEntry.internalPipelineHash = 1;
    }

    type = static_cast<uint32>(PipelineMetadataType::InternalPipelineHashDword3);
    if (indices[type] != -1)
    {
        pOut->pipeline.internalPipelineHash[1] |= (static_cast<uint64>(metadata.At(indices[type]).value) << 32);
        pOut->pipeline.hasEntry.internalPipelineHash = 1;
    }

    if (pOut->pipeline.internalPipelineHash[1] == 0)
    {
        // If the hash is a legacy 64-bit pipeline compiler hash, just use the same hash for both halves of the internal
        // pipeline hash - the legacy hash is most like the "stable" hash.
        pOut->pipeline.internalPipelineHash[1] = pOut->pipeline.internalPipelineHash[0];
    }

    type = static_cast<uint32>(PipelineMetadataType::UserDataLimit);
    if (indices[type] != -1)
    {
        pOut->pipeline.userDataLimit = metadata.At(indices[type]).value;
        pOut->pipeline.hasEntry.userDataLimit = 1;
    }

    type = static_cast<uint32>(PipelineMetadataType::SpillThreshold);
    if (indices[type] != -1)
    {
        pOut->pipeline.spillThreshold = metadata.At(indices[type]).value;
        pOut->pipeline.hasEntry.spillThreshold = 1;
    }

    type = static_cast<uint32>(PipelineMetadataType::StreamOutTableEntry);
    if (indices[type] != -1)
    {
        pOut->pipeline.streamOutTableAddress = metadata.At(indices[type]).value;
        pOut->pipeline.hasEntry.streamOutTableAddress = 1;
    }
    else
    {
        pOut->pipeline.streamOutTableAddress = 0;
    }

    type = static_cast<uint32>(PipelineMetadataType::EsGsLdsByteSize);
    if (indices[type] != -1)
    {
        pOut->pipeline.esGsLdsSize = metadata.At(indices[type]).value;
        pOut->pipeline.hasEntry.esGsLdsSize = 1;
    }

    type = static_cast<uint32>(PipelineMetadataType::UsesViewportArrayIndex);
    if (indices[type] != -1)
    {
        pOut->pipeline.flags.usesViewportArrayIndex = (metadata.At(indices[type]).value != 0);
        pOut->pipeline.hasEntry.usesViewportArrayIndex = 1;
    }

    type = static_cast<uint32>(PipelineMetadataType::PipelineNameIndex);
    if (indices[type] != -1)
    {
        Elf::StringProcessor<Allocator> symbolStringProcessor(m_pSymbolStrTabSection, m_pAllocator);
        const char*const pPipelineName = symbolStringProcessor.Get(metadata.At(indices[type]).value);

        Strncpy(&pOut->pipeline.name[0], pPipelineName, sizeof(pOut->pipeline.name));
        pOut->pipeline.hasEntry.name = 1;
    }

    ApiHwShaderMapping apiHwMapping = {};

    type = static_cast<uint32>(PipelineMetadataType::ApiHwShaderMappingLo);
    if (indices[type] != -1)
    {
        apiHwMapping.u32Lo = metadata.At(indices[type]).value;
    }

    type = static_cast<uint32>(PipelineMetadataType::ApiHwShaderMappingHi);
    if (indices[type] != -1)
    {
        apiHwMapping.u32Hi = metadata.At(indices[type]).value;
    }

    for (type  = static_cast<uint32>(PipelineMetadataType::IndirectTableEntryLow);
         type <= static_cast<uint32>(PipelineMetadataType::IndirectTableEntryHigh);
         ++type)
    {
        const uint32 num = type - static_cast<uint32>(PipelineMetadataType::IndirectTableEntryLow);
        if (indices[type] != -1)
        {
            pOut->pipeline.indirectUserDataTableAddresses[num] = metadata.At(indices[type]).value;
            pOut->pipeline.hasEntry.indirectUserDataTableAddresses = 1;
        }
        else
        {
            pOut->pipeline.indirectUserDataTableAddresses[num] = 0;
        }
    }

    // Translate per-API shader metadata.
    for (uint32 s = 0; s < static_cast<uint32>(ApiShaderType::Count); ++s)
    {
        pOut->pipeline.shader[s].hasEntry.uAll = 0;

        auto*const pDwords = reinterpret_cast<uint32*>(&pOut->pipeline.shader[s].apiShaderHash[0]);

        for (uint32 i = 0; i < 4; ++i)
        {
            type = ((static_cast<uint32>(PipelineMetadataType::ApiCsHashDword0) + i) + (s << 2));

            if (indices[type] != -1)
            {
                pDwords[i] = metadata.At(indices[type]).value;
                pOut->pipeline.shader[s].hasEntry.apiShaderHash = 1;
            }
            else
            {
                pDwords[i] = 0;
            }
        }

        if (apiHwMapping.apiShaders[s] != 0)
        {
            pOut->pipeline.shader[s].hardwareMapping = apiHwMapping.apiShaders[s];
            pOut->pipeline.shader[s].hasEntry.hardwareMapping = 1;
        }
    }

    // Translate per-hardware stage metadata.
    static constexpr uint32 Ps = static_cast<uint32>(HardwareStage::Ps);

    type = static_cast<uint32>(PipelineMetadataType::PsUsesUavs);
    if (indices[type] != -1)
    {
        pOut->pipeline.hardwareStage[Ps].flags.usesUavs = (metadata.At(indices[type]).value != 0);
        pOut->pipeline.hardwareStage[Ps].hasEntry.usesUavs = 1;
#if (!PAL_BUILD_SCPC) && (PAL_CLIENT_INTERFACE_MAJOR_VERSION < 456)
        if (indices[static_cast<uint32>(PipelineMetadataType::PsWritesUavs)] == -1)
        {
            pOut->pipeline.hardwareStage[Ps].flags.writesUavs = (metadata.At(indices[type]).value != 0);
            pOut->pipeline.hardwareStage[Ps].hasEntry.writesUavs = 1;
        }
#endif
    }

    type = static_cast<uint32>(PipelineMetadataType::PsUsesRovs);
    if (indices[type] != -1)
    {
        pOut->pipeline.hardwareStage[Ps].flags.usesRovs = (metadata.At(indices[type]).value != 0);
        pOut->pipeline.hardwareStage[Ps].hasEntry.usesRovs = 1;
    }

    type = static_cast<uint32>(PipelineMetadataType::PsWritesUavs);
    if (indices[type] != -1)
    {
        pOut->pipeline.hardwareStage[Ps].flags.writesUavs = (metadata.At(indices[type]).value != 0);
        pOut->pipeline.hardwareStage[Ps].hasEntry.writesUavs = 1;
    }

    type = static_cast<uint32>(PipelineMetadataType::PsWritesDepth);
    if (indices[type] != -1)
    {
        pOut->pipeline.hardwareStage[Ps].flags.writesDepth = (metadata.At(indices[type]).value != 0);
        pOut->pipeline.hardwareStage[Ps].hasEntry.writesDepth = 1;
    }

    type = static_cast<uint32>(PipelineMetadataType::PsUsesAppendConsume);
    if (indices[type] != -1)
    {
        pOut->pipeline.hardwareStage[Ps].flags.usesAppendConsume = (metadata.At(indices[type]).value != 0);
        pOut->pipeline.hardwareStage[Ps].hasEntry.usesAppendConsume = 1;
    }

    for (uint32 h = 0; h < static_cast<uint32>(HardwareStage::Count); ++h)
    {
        pOut->pipeline.hardwareStage[h].hasEntry.uAll = 0;

        type = static_cast<uint32>(PipelineMetadataType::ShaderNumUsedVgprs) + h;
        if (indices[type] != -1)
        {
            pOut->pipeline.hardwareStage[h].vgprCount = metadata.At(indices[type]).value;
            pOut->pipeline.hardwareStage[h].hasEntry.vgprCount = 1;
        }

        type = static_cast<uint32>(PipelineMetadataType::ShaderNumUsedSgprs) + h;
        if (indices[type] != -1)
        {
            pOut->pipeline.hardwareStage[h].sgprCount = metadata.At(indices[type]).value;
            pOut->pipeline.hardwareStage[h].hasEntry.sgprCount = 1;
        }

        type = static_cast<uint32>(PipelineMetadataType::ShaderNumAvailVgprs) + h;
        if (indices[type] != -1)
        {
            pOut->pipeline.hardwareStage[h].vgprLimit = metadata.At(indices[type]).value;
            pOut->pipeline.hardwareStage[h].hasEntry.vgprLimit = 1;
        }

        type = static_cast<uint32>(PipelineMetadataType::ShaderNumAvailSgprs) + h;
        if (indices[type] != -1)
        {
            pOut->pipeline.hardwareStage[h].sgprLimit = metadata.At(indices[type]).value;
            pOut->pipeline.hardwareStage[h].hasEntry.sgprLimit = 1;
        }

        type = static_cast<uint32>(PipelineMetadataType::ShaderLdsByteSize) + h;
        if (indices[type] != -1)
        {
            pOut->pipeline.hardwareStage[h].ldsSize = metadata.At(indices[type]).value;
            pOut->pipeline.hardwareStage[h].hasEntry.ldsSize = 1;
        }

        type = static_cast<uint32>(PipelineMetadataType::ShaderScratchByteSize) + h;
        if (indices[type] != -1)
        {
            pOut->pipeline.hardwareStage[h].scratchMemorySize = metadata.At(indices[type]).value;
            pOut->pipeline.hardwareStage[h].hasEntry.scratchMemorySize = 1;
        }

        type = static_cast<uint32>(PipelineMetadataType::ShaderPerformanceDataBufferSize) + h;
        if (indices[type] != -1)
        {
            pOut->pipeline.hardwareStage[h].perfDataBufferSize = metadata.At(indices[type]).value;
            pOut->pipeline.hardwareStage[h].hasEntry.perfDataBufferSize = 1;
        }
    }

    return result;
}

} // Abi
} // Util
