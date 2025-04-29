/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/devDriverUtil.h"
#include "core/device.h"
#include "core/dmaUploadRing.h"
#include "g_coreSettings.h"
#include "core/platform.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/pipeline.h"
#include "core/hw/gfxip/codeObjectUploader.h"
#include "core/hw/gfxip/graphicsShaderLibrary.h"
#include "palFile.h"
#include "palEventDefs.h"
#include "palSysUtil.h"

using namespace Util;
using SectionId = ElfReader::SectionId;

namespace Pal
{

// =====================================================================================================================
void* SectionInfo::GetCpuMappedAddr(
    gpusize offset
    ) const
{
    auto chunkIter = m_chunks.Begin();
    while (offset >= chunkIter.Get().size)
    {
        offset -= chunkIter.Get().size;
        chunkIter.Next();
    }
    return Util::VoidPtrInc(chunkIter.Get().pCpuMappedAddr, static_cast<size_t>(offset));
}

// =====================================================================================================================
SectionInfo* SectionMemoryMap::AddSection(
    uint32      elfIndex,
    SectionId   sectionId,
    gpusize     gpuVirtAddr,
    gpusize     offset,
    const void* pCpuLocalAddr)
{
    SectionInfo info(&m_allocator, elfIndex, sectionId, gpuVirtAddr, offset, pCpuLocalAddr);
    Result result = m_sections.PushBack(info);
    SectionInfo* pSection = nullptr;
    if (result == Result::Success)
    {
        pSection = &m_sections.Back();
    }
    return pSection;
}

// =====================================================================================================================
const SectionInfo* SectionMemoryMap::FindSection(
    uint32    elfIdx,
    SectionId sectionId
    ) const
{
    // Even in multi-ELF, we don't expect there to be very many sections total today, so this simple lookup is fine.
    // If that changes in the future, then we should consider optimizing this.
    const SectionInfo* pSection = nullptr;
    for (uint32 i = 0; i < m_sections.NumElements(); i++)
    {
        const auto& section = m_sections.At(i);
        if ((section.GetElfIndex() == elfIdx) && (section.GetSectionId() == sectionId))
        {
            pSection = &section;
            break;
        }
    }
    return pSection;
}

// =====================================================================================================================
Result SectionAddressCalculator::AddSection(
    const ElfReader::Reader& elfReader,
    uint32                   elfIndex,
    SectionId                sectionId)
{
    gpusize alignment = elfReader.GetSection(sectionId).sh_addralign;
    // According to the elf spec, 0 and 1 mean everything is allowed
    if (alignment == 0)
    {
        alignment = 1;
    }
    const gpusize offset = Util::Pow2Align(m_size, alignment);
    m_size = offset + elfReader.GetSection(sectionId).sh_size;

    if (alignment > m_alignment)
    {
        m_alignment = alignment;
    }

    return m_sections.PushBack({elfIndex, sectionId, offset});
}

// =====================================================================================================================
CodeObjectUploader::CodeObjectUploader(
    Device*          pDevice,
    const AbiReader& abiReader)
    :
    m_pDevice(pDevice),
    m_abiReader(abiReader),
    m_pGpuMemory(nullptr),
    m_baseOffset(0),
    m_gpuMemSize(0),
    m_memoryMap(pDevice->GetPlatform()),
    m_pMappedPtr(nullptr),
    m_pagingFenceVal(0),
    m_slotId(0),
    m_heapInvisUploadOffset(0),
    m_prefetchGpuVirtAddr(0),
    m_prefetchSize(0)
{
}

// =====================================================================================================================
CodeObjectUploader::~CodeObjectUploader()
{
    PAL_ASSERT(m_pMappedPtr == nullptr); // If this fires, the caller forgot to call End()!
}

// =====================================================================================================================
Result CodeObjectUploader::UploadPipelineSections(
    const void*  pSectionBuffer,
    size_t       sectionBufferSize,
    SectionInfo* pChunks)
{
    Result result = Result::Success;

    size_t bytesRemaining = sectionBufferSize;
    size_t localOffset    = 0;
    while (bytesRemaining > 0)
    {
        void* pEmbeddedData = nullptr;
        size_t bytesCopied = m_pDevice->UploadUsingEmbeddedData(
                                                        m_slotId,
                                                        m_pGpuMemory,
                                                        m_baseOffset + m_heapInvisUploadOffset,
                                                        bytesRemaining,
                                                        &pEmbeddedData);

        if (pChunks != nullptr)
        {
            result = pChunks->AddCpuMappedChunk({pEmbeddedData, bytesCopied});
            if (result != Result::Success)
            {
                break;
            }
        }

        memcpy(pEmbeddedData, VoidPtrInc(pSectionBuffer, localOffset), bytesCopied);
        localOffset             += bytesCopied;
        m_heapInvisUploadOffset += bytesCopied;
        bytesRemaining          -= bytesCopied;
    }
    return result;
}

// =====================================================================================================================
void CodeObjectUploader::PatchPipelineInternalSrdTable(
    uint32    elfIndex,
    SectionId dataSectionId)
{
    // The for loop which follows is entirely non-standard behavior for an ELF loader, but is intended to
    // only be temporary code.
    for (uint32 s = 0; s < static_cast<uint32>(Abi::HardwareStage::Count); ++s)
    {
        const Abi::PipelineSymbolType symbolType =
            Abi::GetSymbolForStage(Abi::PipelineSymbolType::ShaderIntrlTblPtr, static_cast<Abi::HardwareStage>(s));

        const Elf::SymbolTableEntry* pSymbol    = nullptr;
        const ElfReader::Reader*     pElfReader = nullptr;

        const Abi::SymbolEntry* pSymbolEntry = m_abiReader.FindSymbol(symbolType);
        if ((pSymbolEntry != nullptr) && (pSymbolEntry->m_elfIndex == elfIndex))
        {
            pElfReader = &m_abiReader.GetElfReader(pSymbolEntry->m_elfIndex);
            ElfReader::Symbols symbolSection(*pElfReader, pSymbolEntry->m_section);
            pSymbol = &symbolSection.GetSymbol(pSymbolEntry->m_index);
        }

        if ((pSymbol != nullptr) && (pSymbol->st_shndx == dataSectionId))
        {
            const SectionInfo*const pSectionInfo = m_memoryMap.FindSection(pSymbolEntry->m_elfIndex, dataSectionId);
            PAL_ASSERT_MSG(pSectionInfo != nullptr, "Data section not uploaded to GPU");
            m_pDevice->GetGfxDevice()->PatchPipelineInternalSrdTable(
                pSectionInfo->GetCpuMappedAddr(pSymbol->st_value), // Dst
                VoidPtrInc(pElfReader->GetSectionData(pSymbol->st_shndx),
                           static_cast<size_t>(pSymbol->st_value)), // Src
                static_cast<size_t>(pSymbol->st_size),
                pSectionInfo->GetGpuVirtAddr());
        }
    } // for each hardware stage
}

// =====================================================================================================================
GpuHeap CodeObjectUploader::SelectUploadHeap(
    GpuHeap heap) // Client-preferred heap
{
    static_assert(static_cast<uint32>(PreferredPipelineUploadHeap::PipelineHeapLocal)         ==
                  static_cast<uint32>(GpuHeap::GpuHeapLocal),         "Pipeline heap enumeration needs to be updated!");
    static_assert(static_cast<uint32>(PreferredPipelineUploadHeap::PipelineHeapInvisible)     ==
                  static_cast<uint32>(GpuHeap::GpuHeapInvisible),     "Pipeline heap enumeration needs to be updated!");
    static_assert(static_cast<uint32>(PreferredPipelineUploadHeap::PipelineHeapGartUswc)      ==
                  static_cast<uint32>(GpuHeap::GpuHeapGartUswc),      "Pipeline heap enumeration needs to be updated!");
    static_assert(static_cast<uint32>(PreferredPipelineUploadHeap::PipelineHeapGartCacheable) ==
                  static_cast<uint32>(GpuHeap::GpuHeapGartCacheable), "Pipeline heap enumeration needs to be updated!");
    static_assert(static_cast<uint32>(PreferredPipelineUploadHeap::PipelineHeapDeferToClient) ==
                  static_cast<uint32>(GpuHeap::GpuHeapCount),         "Pipeline heap enumeration needs to be updated!");

    const PalSettings& settings = m_pDevice->Settings();
    if (settings.preferredPipelineUploadHeap == PreferredPipelineUploadHeap::PipelineHeapDeferToClient)
    {
        m_pipelineHeapType = heap;
    }
    else
    {
        m_pipelineHeapType = static_cast<GpuHeap>(settings.preferredPipelineUploadHeap);
    }

    if (m_pDevice->ValidatePipelineUploadHeap(m_pipelineHeapType) == false)
    {
        m_pipelineHeapType = GpuHeap::GpuHeapLocal;

        // Cannot upload to this heap for this device.  Fall back to using the optimal heap instead.
        PAL_ALERT(m_pDevice->ValidatePipelineUploadHeap(heap));
    }

    return m_pipelineHeapType;
}

// =====================================================================================================================
// Allocates GPU memory for the current pipeline.  Also, maps the memory for CPU access and uploads the pipeline code
// and data.  The GPU virtual addresses for the code, data, and register segments are also computed.  The caller is
// responsible for calling End() which unmaps the GPU memory.
Result CodeObjectUploader::Begin(
    GpuHeap heap,
    bool    isInternal)
{
    const PalSettings& settings = m_pDevice->Settings();
    Result result = Result::Success;

    SectionAddressCalculator addressCalculator(m_pDevice->GetPlatform());

    uint32 elfIdx = 0;
    for (const auto& [elfHash, elfReader] : m_abiReader.GetElfs())
    {
        const SectionId numSections = elfReader.GetNumSections();
        for (SectionId i = 0; i < numSections; i++)
        {
            const auto& section = elfReader.GetSection(i);
            if (section.sh_flags & Elf::ShfAlloc)
            {
                result = addressCalculator.AddSection(elfReader, elfIdx, i);
                if (result != Result::Success)
                {
                    break;
                }
            }
        }
        ++elfIdx;
    }

    if (result == Result::Success)
    {
        m_prefetchSize = addressCalculator.GetSize();

        // The driver must make sure there is a distance of at least gpuInfo.shaderPrefetchBytes
        // that follows the end of the shader to avoid a page fault when the SQ tries to
        // prefetch past the end of a shader
        // shaderPrefetchBytes is set from SH_MEM_CONFIG.INITIAL_INST_PREFETCH
        // defaulting to the hardware supported maximum if necessary
        const gpusize minSafeSize = Pow2Align(m_prefetchSize, ShaderICacheLineSize) +
                                    m_pDevice->ChipProperties().gfxip.shaderPrefetchBytes;

        m_gpuMemSize = Max(m_gpuMemSize, minSafeSize);

        GpuMemoryCreateInfo createInfo = { };
        createInfo.size      = m_gpuMemSize;
        createInfo.alignment = Max(GpuSectionMemByteAlign, addressCalculator.GetAlignment());
        createInfo.vaRange   = VaRange::DescriptorTable;
        createInfo.heaps[0]  = SelectUploadHeap(heap);
        createInfo.heaps[1]  = GpuHeapGartUswc;
        createInfo.heapCount = 2;
        createInfo.priority  = GpuMemPriority::High;

        GpuMemoryInternalCreateInfo internalInfo = { };
        internalInfo.flags.alwaysResident = 1;
        internalInfo.flags.appRequested   = (isInternal == false);
        internalInfo.pPagingFence = &m_pagingFenceVal;

        result = m_pDevice->MemMgr()->AllocateGpuMem(createInfo, internalInfo, false, &m_pGpuMemory, &m_baseOffset);
    }

    void* pMappedPtr = nullptr;
    if (result == Result::Success)
    {
        if (m_pDevice->ShouldUploadUsingDma(m_pipelineHeapType))
        {
            result = UploadUsingDma(addressCalculator, &pMappedPtr);
        }
        else
        {
            result = UploadUsingCpu(addressCalculator, &pMappedPtr);
        }
    }

    if (result == Result::Success)
    {
        m_prefetchGpuVirtAddr = (m_pGpuMemory->Desc().gpuVirtAddr + m_baseOffset);
    }

    return result;
}

// =====================================================================================================================
Result CodeObjectUploader::UploadUsingDma(
    const SectionAddressCalculator& addressCalc,
    void**                          ppMappedPtr)
{
    Result result = m_pDevice->AcquireRingSlot(&m_slotId);
    if (result == Result::Success)
    {
        const gpusize gpuVirtAddr = (m_pGpuMemory->Desc().gpuVirtAddr + m_baseOffset);

        uint32    lastElfIdx    = UINT32_MAX;
        SectionId dataSectionId = 0;

        for (auto sectionIter = addressCalc.GetSectionsBegin(); sectionIter.IsValid(); sectionIter.Next())
        {
            const SectionAddressCalculator::SectionOffset& section = sectionIter.Get();
            const ElfReader::Reader& elfReader = m_abiReader.GetElfReader(section.elfIndex);

            const void* pSectionData = elfReader.GetSectionData(section.sectionId);
            PAL_ASSERT(section.offset >= elfReader.GetSection(section.sectionId).sh_addr);
            uint64 offset = section.offset - elfReader.GetSection(section.sectionId).sh_addr;

            SectionInfo*const pInfo = m_memoryMap.AddSection(
                section.elfIndex, section.sectionId, (gpuVirtAddr + section.offset), offset, pSectionData);

            if (pInfo == nullptr)
            {
                result = Result::ErrorOutOfMemory;
                break;
            }

            // Insert into DMA queue
            m_heapInvisUploadOffset = section.offset;
            result = UploadPipelineSections(pSectionData,
                                            static_cast<size_t>(elfReader.GetSection(section.sectionId).sh_size),
                                            pInfo);
            if (result != Result::Success)
            {
                break;
            }

            if (lastElfIdx != section.elfIndex)
            {
                dataSectionId = elfReader.FindSection(".data");
                lastElfIdx    = section.elfIndex;
            }

            if (dataSectionId == section.sectionId)
            {
                PatchPipelineInternalSrdTable(section.elfIndex, section.sectionId);
            }
        }

        if (result == Result::Success)
        {
            // Including potential padding space before data and register section, and potential padding space after
            // end of all sections in case total size of all pipeline sections is less than minSafeSize.
            const size_t dataRegisterAndPadding = static_cast<size_t>(m_gpuMemSize - m_heapInvisUploadOffset);
            if (dataRegisterAndPadding > 0)
            {
                const size_t align = Max(GpuSectionMemByteAlign, addressCalc.GetAlignment());

                m_pMappedPtr = PAL_CALLOC_ALIGNED(
                    dataRegisterAndPadding, align, m_pDevice->GetPlatform(), AllocInternal);
                if (m_pMappedPtr == nullptr)
                {
                    result = Result::ErrorOutOfMemory;
                }
                else
                {
                    *ppMappedPtr = m_pMappedPtr;
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
Result CodeObjectUploader::UploadUsingCpu(
    const SectionAddressCalculator& addressCalc,
    void**                          ppMappedPtr)
{
    Result result = m_pGpuMemory->Map(&m_pMappedPtr);
    if (result == Result::Success)
    {
        m_pMappedPtr = VoidPtrInc(m_pMappedPtr, static_cast<size_t>(m_baseOffset));

        const gpusize gpuVirtAddr = (m_pGpuMemory->Desc().gpuVirtAddr + m_baseOffset);

        uint32    lastElfIdx    = UINT32_MAX;
        SectionId dataSectionId = 0;

        for (auto sectionIter = addressCalc.GetSectionsBegin(); sectionIter.IsValid(); sectionIter.Next())
        {
            const SectionAddressCalculator::SectionOffset& section = sectionIter.Get();
            const ElfReader::Reader& elfReader = m_abiReader.GetElfReader(section.elfIndex);

            const void*  pSectionData = elfReader.GetSectionData(section.sectionId);
            const uint64 sectionSize  = elfReader.GetSection(section.sectionId).sh_size;
            void*const   pMappedPtr   = VoidPtrInc(m_pMappedPtr, static_cast<size_t>(section.offset));
            PAL_ASSERT(section.offset >= elfReader.GetSection(section.sectionId).sh_addr);
            uint64 offset = section.offset - elfReader.GetSection(section.sectionId).sh_addr;

            SectionInfo*const pInfo = m_memoryMap.AddSection(
                section.elfIndex, section.sectionId, (gpuVirtAddr + section.offset), offset, pSectionData);
            if (pInfo == nullptr)
            {
                result = Result::ErrorOutOfMemory;
                break;
            }

            result = pInfo->AddCpuMappedChunk({pMappedPtr, sectionSize});
            if (result != Result::Success)
            {
                break;
            }

            // Copy onto GPU
            memcpy(pMappedPtr, pSectionData, static_cast<size_t>(sectionSize));

            if (lastElfIdx != section.elfIndex)
            {
                dataSectionId = elfReader.FindSection(".data");
                lastElfIdx    = section.elfIndex;
            }

            if (dataSectionId == section.sectionId)
            {
                PatchPipelineInternalSrdTable(section.elfIndex, section.sectionId);
            }
        }
    }

    if (result == Result::Success)
    {
        *ppMappedPtr = VoidPtrInc(m_pMappedPtr, static_cast<size_t>(addressCalc.GetSize()));
    }

    return result;
}

// =====================================================================================================================
// Apply relocations
Result CodeObjectUploader::ApplyRelocations()
{
    Result result = Result::Success;

    // Apply relocations: For each ELF, iterate through all REL sections
    uint32 elfIdx = 0;
    for (const auto& [elfHash, elfReader] : m_abiReader.GetElfs())
    {
        const SectionId numSections = elfReader.GetNumSections();
        for (SectionId i = 0; (result == Result::Success) && (i < numSections); i++)
        {
            auto type = elfReader.GetSectionType(i);
            if ((type != Elf::SectionHeaderType::Rel) && (type != Elf::SectionHeaderType::Rela))
            {
                continue;
            }

            ElfReader::Relocations relocs(elfReader, i);
            result = ApplyRelocationSection(elfIdx, relocs);
        }

        if (result != Result::Success)
        {
            break;
        }

        ++elfIdx;
    }
    return result;
}

// =====================================================================================================================
// Applies the relocations of one section.
Result CodeObjectUploader::ApplyRelocationSection(
    uint32                        elfIndex,
    const ElfReader::Relocations& relocations)
{
    bool isRela = relocations.IsRela();
    // sh_info contains a reference to the target section where the
    // relocations should be performed.
    const SectionInfo* pMemInfo = m_memoryMap.FindSection(elfIndex, relocations.GetDestSection());
    Result result = Result::Success;

    // If the section is mapped
    if (pMemInfo != nullptr)
    {
        const ElfReader::Reader& elfReader = m_abiReader.GetElfReader(elfIndex);

        // sh_link contains a reference to the symbol section
        ElfReader::Symbols symbols(elfReader, relocations.GetSymbolSection());
        SectionId dstSection = relocations.GetDestSection();

        // We have three types of addresses:
        // 1. Virtual GPU addresses, these will be written into the destination
        // 2. The CPU address of the ELF, we read from there because it is fast
        // 3. The CPU mapped address of the destination section on the GPU, we write to that address
        const void* pSecSrcAddr = elfReader.GetSectionData(relocations.GetDestSection());

        for (uint64 i = 0; i < relocations.GetNumRelocations(); i++)
        {
            const Elf::RelTableEntry& relocation = relocations.GetRel(i);
            const Util::Elf::SymbolTableEntry& symbol = symbols.GetSymbol(relocation.r_info.sym);

            // Get address of referenced symbol
            const SectionInfo* pSymSection = m_memoryMap.FindSection(elfIndex, symbol.st_shndx);
            if (pSymSection == nullptr)
            {
                PAL_ASSERT_ALWAYS_MSG("Relocation symbol not found: %s", symbols.GetSymbolName(relocation.r_info.sym));
                result = Result::ErrorInvalidPipelineElf;
                break;
            }

            // Address where to write the relocation
            const uint64* pSrcAddr = static_cast<const uint64*>(
                VoidPtrInc(pSecSrcAddr, static_cast<size_t>(relocation.r_offset)));
            void* pDstAddr = pMemInfo->GetCpuMappedAddr(relocation.r_offset);
            gpusize gpuVirtAddr = pMemInfo->GetGpuVirtAddr() + relocation.r_offset;

            Util::Abi::RelocationType relType = static_cast<Util::Abi::RelocationType>(relocation.r_info.type);
            uint64 addend = 0;
            if (isRela)
            {
                addend = relocations.GetRela(i).r_addend;
            }
            else
            {
                // Add original value for .rel sections.
                // .rela sections explicitely contain the addend.
                uint16 addend16 = 0;
                uint32 addend32 = 0;
                switch (relType) {
                case Util::Abi::RelocationType::Rel16:
                    memcpy(&addend16, pSrcAddr, sizeof(addend16));
                    addend = addend16;
                    break;
                case Util::Abi::RelocationType::Abs32:
                case Util::Abi::RelocationType::Abs32Lo:
                case Util::Abi::RelocationType::Abs32Hi:
                case Util::Abi::RelocationType::Rel32:
                case Util::Abi::RelocationType::Rel32Lo:
                case Util::Abi::RelocationType::Rel32Hi:
                    memcpy(&addend32, pSrcAddr, sizeof(addend32));
                    addend = addend32;
                    break;
                case Util::Abi::RelocationType::Abs64:
                case Util::Abi::RelocationType::Rel64:
                    memcpy(&addend, pSrcAddr, sizeof(addend));
                    break;
                default:
                    PAL_ASSERT_ALWAYS();
                }
            }

            // The virtual GPU address of the symbol
            gpusize symbolAddr = pSymSection->GetGpuVirtAddr() + symbol.st_value;
            uint64 abs = symbolAddr + addend;

            uint16 val16 = 0;
            uint32 val32 = 0;
            uint64 val64 = 0;
            switch (relType) {
            case Util::Abi::RelocationType::Rel16:
                val16 = static_cast<uint16>(((abs - gpuVirtAddr) - 4) / 4);
                memcpy(pDstAddr, &val16, sizeof(val16));
                break;
            case Util::Abi::RelocationType::Abs32:
                PAL_ASSERT(static_cast<uint64>(static_cast<uint32>(abs)) == abs);
            case Util::Abi::RelocationType::Abs32Lo:
                val32 = static_cast<uint32>(abs);
                memcpy(pDstAddr, &val32, sizeof(val32));
                break;
            case Util::Abi::RelocationType::Abs32Hi:
                val32 = static_cast<uint32>(abs >> 32);
                memcpy(pDstAddr, &val32, sizeof(val32));
                break;
            case Util::Abi::RelocationType::Abs64:
                val64 = abs;
                memcpy(pDstAddr, &val64, sizeof(val64));
                break;
            case Util::Abi::RelocationType::Rel32:
                PAL_ASSERT(static_cast<uint64>(static_cast<uint32>(abs - gpuVirtAddr)) == (abs - gpuVirtAddr));
            case Util::Abi::RelocationType::Rel32Lo:
                val32 = static_cast<uint32>(abs - gpuVirtAddr);
                memcpy(pDstAddr, &val32, sizeof(val32));
                break;
            case Util::Abi::RelocationType::Rel32Hi:
                val32 = static_cast<uint32>((abs - gpuVirtAddr) >> 32);
                memcpy(pDstAddr, &val32, sizeof(val32));
                break;
            case Util::Abi::RelocationType::Rel64:
                val64 = abs - gpuVirtAddr;
                memcpy(pDstAddr, &val64, sizeof(val64));
                break;
            default:
                PAL_ASSERT_ALWAYS();
            }
        }
    }

    return result;
}

// =====================================================================================================================
// "Finishes" uploading a pipeline to GPU memory by requesting the device to submit a DMA copy of the pipeline from
// its initial heap to the local invisible heap. The temporary CPU visible heap is freed.
Result CodeObjectUploader::End(
    UploadFenceToken* pCompletionFence)
{
    Result result = Result::Success;

    if (m_pGpuMemory != nullptr)
    {
        if (m_pDevice->ShouldUploadUsingDma(m_pipelineHeapType))
        {
            const size_t dataRegisterAndPadding = static_cast<size_t>(m_gpuMemSize - m_heapInvisUploadOffset);
            if (dataRegisterAndPadding > 0)
            {
                PAL_ASSERT(m_pMappedPtr != nullptr);
                result = UploadPipelineSections(m_pMappedPtr, dataRegisterAndPadding, nullptr);
            }
            if (result == Result::Success)
            {
                result = m_pDevice->SubmitDmaUploadRing(m_slotId, pCompletionFence, m_pagingFenceVal);
                PAL_ASSERT(*pCompletionFence > 0);
                PAL_SAFE_FREE(m_pMappedPtr, m_pDevice->GetPlatform());
            }
        }
        else
        {
            PAL_ASSERT(m_pMappedPtr != nullptr);
            result = m_pGpuMemory->Unmap();
        }

        m_pMappedPtr = nullptr;
    }

    return result;
}

// =====================================================================================================================
Result CodeObjectUploader::GetEntryPointGpuSymbol(
    Abi::HardwareStage                stage,
    const PalAbi::CodeObjectMetadata& metadata,
    GpuSymbol*                        pSymbol
    ) const
{
    const PalAbi::HardwareStageMetadata& stageMetadata = metadata.pipeline.hardwareStage[uint32(stage)];
    const Abi::PipelineSymbolType defaultSym = Abi::GetSymbolForStage(Abi::PipelineSymbolType::ShaderMainEntry, stage);
    const StringView<char> defaultSymName    = Abi::PipelineAbiSymbolNameStrings[uint32(defaultSym)];

    const bool isDefaultEntryPoint = (PipelineSupportsGenericEntryPoint(metadata) == false) ||
        (stageMetadata.hasEntry.entryPointSymbol == 0) || (stageMetadata.entryPointSymbol == defaultSymName);

    return isDefaultEntryPoint ? GetGpuSymbol(defaultSym, pSymbol)
                               : GetGpuSymbol(stageMetadata.entryPointSymbol, pSymbol);
}

// =====================================================================================================================
Result CodeObjectUploader::GetAbsoluteSymbolAddress(
    const Abi::SymbolEntry* pCoSymbol,
    GpuSymbol*              pGpuSymbol
    ) const
{
    Result result = Result::Success;
    if (pCoSymbol != nullptr)
    {
        ElfReader::Symbols symbolSection(m_abiReader.GetElfReader(pCoSymbol->m_elfIndex), pCoSymbol->m_section);
        const Util::Elf::SymbolTableEntry* pElfSymbol = &symbolSection.GetSymbol(pCoSymbol->m_index);

        pGpuSymbol->gpuVirtAddr = pElfSymbol->st_value;
        pGpuSymbol->size        = pElfSymbol->st_size;
        const SectionInfo* pSection = m_memoryMap.FindSection(pCoSymbol->m_elfIndex, pElfSymbol->st_shndx);
        if (pSection != nullptr)
        {
            pGpuSymbol->gpuVirtAddr += pSection->GetGpuVirtAddr();
        }
        else
        {
            result = Result::ErrorGpuMemoryNotBound;
        }
    }
    else
    {
        result = Result::NotFound;
    }

    return result;
}

// =====================================================================================================================
gpusize CodeObjectUploader::SectionOffset() const
{
    gpusize offset = 0;
    // HSA pipeline binary may be unlinked, and it has multiple .text section, we need adjust the offset according to
    // the section offset in CS entry symbol. For graphics pipeline or PAL ABI based pipeline, it is always 0.
    const Abi::SymbolEntry* pCoSymbol = m_abiReader.FindSymbol(Abi::PipelineSymbolType::CsMainEntry);
    if (pCoSymbol != nullptr)
    {
        ElfReader::Symbols symbolSection(m_abiReader.GetElfReader(pCoSymbol->m_elfIndex), pCoSymbol->m_section);
        const Util::Elf::SymbolTableEntry* pElfSymbol = &symbolSection.GetSymbol(pCoSymbol->m_index);

        const SectionInfo* pSection = m_memoryMap.FindSection(pCoSymbol->m_elfIndex, pElfSymbol->st_shndx);
        if (pSection != nullptr)
        {
            offset = pSection->GetOffset();
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }
    }
    return offset;
}

} // Pal
