/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palPipelineAbiUtils.h"
#include "g_palPipelineAbiMetadataImpl.h"

namespace Util
{
namespace Abi
{

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
    m_pAmdIlSection(nullptr),
    m_pLlvmIrSection(nullptr),
    m_flags(),
    m_metadataMajorVer(0),
    m_metadataMinorVer(1),
    m_compatRegisterSize(0),
    m_pCompatRegisterBlob(nullptr),
    m_pMetadata(nullptr),
    m_metadataSize(0),
    m_genericSymbolsMap(16u, pAllocator),
    m_genericSymbolNames(pAllocator),
    m_pipelineSymbolsVector(pAllocator),
    m_pipelineSymbolIndices(),
    m_elfProcessor(pAllocator),
    m_pAllocator(pAllocator)
{
    for (uint32 i = 0; i < static_cast<uint32>(PipelineSymbolType::Count); i++)
    {
        m_pipelineSymbolIndices[i] = -1;
    }
}

// =====================================================================================================================
template <typename Allocator>
PipelineAbiProcessor<Allocator>::~PipelineAbiProcessor()
{
    if (m_pAllocator != nullptr)
    {
        PAL_FREE(m_pCompatRegisterBlob, m_pAllocator);
    }

    for (uint32_t i = 0, numElements = m_genericSymbolNames.NumElements(); i < numElements; ++i)
    {
        PAL_FREE(m_genericSymbolNames.At(i), m_pAllocator);
    }
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::Init()
{
    Result result = m_elfProcessor.Init();

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
        switch (gfxIpStepping)
        {
        case GfxIpSteppingOland:
            m_flags.machineType = AmdGpuMachineType::Gfx602;
            break;
        default:
            m_flags.machineType = static_cast<AmdGpuMachineType>(static_cast<uint32>(AmdGpuMachineType::Gfx600) +
                                                                 gfxIpStepping);
            break;
        }
        break;
    case 7:
        switch (gfxIpStepping)
        {
        case GfxIpSteppingGodavari:
            m_flags.machineType = AmdGpuMachineType::Gfx705;
            break;
        default:
            m_flags.machineType = static_cast<AmdGpuMachineType>(static_cast<uint32>(AmdGpuMachineType::Gfx700) +
                                                                 gfxIpStepping);
            break;
        }
        break;
    case 8:
        switch (gfxIpMinorVer)
        {
        case 0:
            switch (gfxIpStepping)
            {
            case GfxIpSteppingTongaPro:
                m_flags.machineType = AmdGpuMachineType::Gfx805;
                break;
            default:
                m_flags.machineType =
                    static_cast<AmdGpuMachineType>(static_cast<uint32>(AmdGpuMachineType::Gfx801) + gfxIpStepping - 1);
                break;
            }
            break;
        case 1:
            m_flags.machineType = AmdGpuMachineType::Gfx810;
            break;
        default:
            PAL_ASSERT_ALWAYS();
        }
        break;
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
        case GfxIpSteppingRaven2:
            m_flags.machineType = AmdGpuMachineType::Gfx909;
            break;
        case GfxIpSteppingRenoir:
            m_flags.machineType = AmdGpuMachineType::Gfx90C;
            break;
        }
        break;
    case 10:
        switch (gfxIpMinorVer)
        {
        case 1:
            switch (gfxIpStepping)
            {
            case GfxIpSteppingNavi10:
                m_flags.machineType = AmdGpuMachineType::Gfx1010;
                break;
            case GfxIpSteppingNavi14:
                m_flags.machineType = AmdGpuMachineType::Gfx1012;
                break;
            }
            break;
        case 3:
            switch (gfxIpStepping)
            {
            case GfxIpSteppingNavi21:
                m_flags.machineType = AmdGpuMachineType::Gfx1030;
                break;
            default:
                PAL_ASSERT_ALWAYS();
            }
            break;
        }
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }
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
Result PipelineAbiProcessor<Allocator>::SetAmdIl(
    const void* pData,
    size_t      dataSize)
{
    if (m_pAmdIlSection == nullptr)
    {
        m_pAmdIlSection = m_elfProcessor.GetSections()->Add(&AmdGpuCommentAmdIlName[0]);
        if (m_pAmdIlSection != nullptr)
        {
            m_pAmdIlSection->SetType(Elf::SectionHeaderType::ProgBits);
        }
    }

    Result result = Result::Success;
    if ((m_pAmdIlSection == nullptr) || (nullptr == m_pAmdIlSection->SetData(pData, dataSize)))
    {
        result = Result::ErrorOutOfMemory;
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::SetLlvmIr(
    const void* pData,
    size_t      dataSize)
{
    if (m_pLlvmIrSection == nullptr)
    {
        m_pLlvmIrSection = m_elfProcessor.GetSections()->Add(&AmdGpuCommentLlvmIrName[0]);
        if (m_pLlvmIrSection != nullptr)
        {
            m_pLlvmIrSection->SetType(Elf::SectionHeaderType::ProgBits);
        }
    }

    Result result = Result::Success;
    if ((m_pLlvmIrSection == nullptr) || (nullptr == m_pLlvmIrSection->SetData(pData, dataSize)))
    {
        result = Result::ErrorOutOfMemory;
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::SetGenericSection(
    const char* pName,
    const void* pData,
    size_t      dataSize)
{
#if PAL_ENABLE_PRINTS_ASSERTS
    AssertNotStandardSection(pName);
#endif

    Elf::Section<Allocator>* pSection = m_elfProcessor.GetSections()->Get(pName);

    if (pSection == nullptr)
    {
        pSection = m_elfProcessor.GetSections()->Add(pName);
        if (pSection != nullptr)
        {
            pSection->SetType(Elf::SectionHeaderType::ProgBits);
        }
    }

    Result result = Result::Success;
    if ((pSection == nullptr) || (nullptr == pSection->SetData(pData, dataSize)))
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

        if (m_metadataMajorVer != 0)
        {
            result = DeserializePalCodeObjectMetadata(pReader, pMetadata, m_pMetadata,
                static_cast<uint32>(m_metadataSize), m_metadataMajorVer, m_metadataMinorVer);
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
void PipelineAbiProcessor<Allocator>::GetAmdIl(
    const void** ppData,
    size_t*      pDataSize
    ) const
{
    if (m_pAmdIlSection != nullptr)
    {
        *ppData    = m_pAmdIlSection->GetData();
        *pDataSize = m_pAmdIlSection->GetDataSize();
    }
    else
    {
        *ppData    = nullptr;
        *pDataSize = 0;
    }
}

// =====================================================================================================================
template <typename Allocator>
void PipelineAbiProcessor<Allocator>::GetLlvmIr(
    const void** ppData,
    size_t*      pDataSize
    ) const
{
    if (m_pLlvmIrSection != nullptr)
    {
        *ppData    = m_pLlvmIrSection->GetData();
        *pDataSize = m_pLlvmIrSection->GetDataSize();
    }
    else
    {
        *ppData    = nullptr;
        *pDataSize = 0;
    }
}

// =====================================================================================================================
template <typename Allocator>
void PipelineAbiProcessor<Allocator>::GetGenericSection(
    const char*  pName,
    const void** ppData,
    size_t*      pDataSize
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    AssertNotStandardSection(pName);
#endif

    const Elf::Section<Allocator>* pSection = m_elfProcessor.GetSections()->Get(pName);

    if (pSection != nullptr)
    {
        *ppData    = pSection->GetData();
        *pDataSize = pSection->GetDataSize();
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

    auto nameLength = strlen(entry.pName) + 1;
    char* pName = static_cast<char*>(PAL_MALLOC(nameLength, m_pAllocator, AllocInternal));
    Result result = Result::Success;
    if (pName != nullptr)
    {
        Strncpy(pName, entry.pName, nameLength);
        entry.pName = pName;

        result = m_genericSymbolNames.PushBack(pName);
        if (result == Result::Success)
        {
            result = m_genericSymbolsMap.Insert(entry.pName, entry);
        }
        else
        {
            PAL_FREE(pName, m_pAllocator);
        }
    }
    else
    {
        result = Result::ErrorOutOfMemory;
    }

    return result;
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
    MachineTypeToGfxIpVersion(m_flags.machineType, pGfxIpMajorVer, pGfxIpMinorVer, pGfxIpStepping);
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
template <typename Allocator>
Result PipelineAbiProcessor<Allocator>::Finalize(
    const MsgPackWriter& pipelineMetadataWriter)
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
            case AbiSectionType::AmdIl:
                sectionIndex = m_pAmdIlSection->GetIndex();
                break;
            case AbiSectionType::LlvmIr:
                sectionIndex = m_pLlvmIrSection->GetIndex();
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
            case AbiSectionType::AmdIl:
                sectionIndex = m_pAmdIlSection->GetIndex();
                break;
            case AbiSectionType::LlvmIr:
                sectionIndex = m_pLlvmIrSection->GetIndex();
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

        MsgPackWriter codeObjectMetadataWriter(m_pAllocator);

        if (result == Result::Success)
        {
            result = codeObjectMetadataWriter.Reserve(pipelineMetadataWriter.GetSize());
        }

        if (result == Result::Success)
        {
            codeObjectMetadataWriter.DeclareMap(2);

            codeObjectMetadataWriter.Pack(PalCodeObjectMetadataKey::Version);
            codeObjectMetadataWriter.DeclareArray(2);
            codeObjectMetadataWriter.PackPair(m_metadataMajorVer, m_metadataMinorVer);

            codeObjectMetadataWriter.Pack(PalCodeObjectMetadataKey::Pipelines);
            codeObjectMetadataWriter.DeclareArray(1);
            codeObjectMetadataWriter.DeclareMap(pipelineMetadataWriter.NumItems() / 2);
            result = codeObjectMetadataWriter.Append(pipelineMetadataWriter);
        }

        if ((result == Result::Success) &&
            (noteProcessor.Add(MetadataNoteType,
                                AmdGpuArchName,
                                codeObjectMetadataWriter.GetBuffer(),
                                codeObjectMetadataWriter.GetSize()) == UINT_MAX))
        {
            result = Result::ErrorOutOfMemory;
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
Result PipelineAbiProcessor<Allocator>::LoadFromBuffer(
    const void* pBuffer,
    size_t      bufferSize)
{
    Result result = m_elfProcessor.LoadFromBuffer(pBuffer, bufferSize);

    if (result == Result::Success)
    {
         result = m_genericSymbolsMap.Init();
    }

    if (result == Result::Success)
    {
        m_flags.u32All = m_elfProcessor.GetFileHeader()->e_flags;

        if ((m_elfProcessor.GetFileHeader()->ei_osabi != ElfOsAbiVersion)                  ||
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 634
            (GetXnackFeatureV4()                      == AmdGpuFeatureV4Type::Unsupported) ||
            (GetSramEccFeatureV4()                    == AmdGpuFeatureV4Type::Unsupported) ||
#endif
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
        m_pAmdIlSection        = m_elfProcessor.GetSections()->Get(AmdGpuCommentAmdIlName);
        m_pLlvmIrSection       = m_elfProcessor.GetSections()->Get(AmdGpuCommentLlvmIrName);

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
        for (uint32 i = 0; ((result == Result::Success) && (i < noteProcessor.GetNumNotes())); ++i)
        {
            uint32 type;
            const char* pName;
            const void* pDesc;
            size_t descSize;

            noteProcessor.Get(i, &type, &pName, &pDesc, &descSize);

            switch (type)
            {
            case MetadataNoteType:
            {
                m_pMetadata    = pDesc;
                m_metadataSize = descSize;

                MsgPackReader reader;
                result = GetPalMetadataVersion(&reader, pDesc, static_cast<uint32>(descSize),
                    &m_metadataMajorVer, &m_metadataMinorVer);

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
            else if ((m_pAmdIlSection != nullptr) && (sectionIndex == m_pAmdIlSection->GetIndex()))
            {
                sectionType = AbiSectionType::AmdIl;
            }
            else if ((m_pLlvmIrSection != nullptr) && (sectionIndex == m_pLlvmIrSection->GetIndex()))
            {
                sectionType = AbiSectionType::LlvmIr;
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
void PipelineAbiProcessor<Allocator>::AssertNotStandardSection(
    const char* pName)
{
    // Ensure pName does not match a standard section.
    for (uint32 i = 0; i < static_cast<uint32>(Elf::SectionType::Count); ++i)
    {
        PAL_ASSERT(strcmp(Elf::SectionNameStringTable[i], pName) != 0);
    }
    static constexpr char const* UsedSectionNames[] = { ".rel.text", ".rel.data", ".rela.text", ".rela.data",
        AmdGpuDisassemblyName, AmdGpuCommentName, AmdGpuCommentAmdIlName, AmdGpuCommentLlvmIrName };
    for (uint32 i = 0; i < static_cast<uint32>(Util::ArrayLen(UsedSectionNames)); ++i)
    {
        PAL_ASSERT(strcmp(UsedSectionNames[i], pName) != 0);
    }
}

} // Abi
} // Util
