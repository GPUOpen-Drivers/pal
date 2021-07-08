/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/dmaUploadRing.h"
#include "core/g_palSettings.h"
#include "core/platform.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/pipeline.h"
#include "palFile.h"
#include "palEventDefs.h"
#include "palSysUtil.h"

#include "core/devDriverUtil.h"

using namespace Util;

namespace Pal
{

// GPU memory alignment for shader programs.
constexpr size_t GpuMemByteAlign = 256;

constexpr Abi::ApiShaderType PalToAbiShaderType[] =
{
    Abi::ApiShaderType::Cs, // ShaderType::Cs
    Abi::ApiShaderType::Task,
    Abi::ApiShaderType::Vs, // ShaderType::Vs
    Abi::ApiShaderType::Hs, // ShaderType::Hs
    Abi::ApiShaderType::Ds, // ShaderType::Ds
    Abi::ApiShaderType::Gs, // ShaderType::Gs
    Abi::ApiShaderType::Mesh,
    Abi::ApiShaderType::Ps, // ShaderType::Ps
};
static_assert(ArrayLen(PalToAbiShaderType) == NumShaderTypes,
              "PalToAbiShaderType[] array is incorrectly sized!");

// =====================================================================================================================
Pipeline::Pipeline(
    Device* pDevice,
    bool    isInternal)  // True if this is a PAL-owned pipeline (i.e., an RPM pipeline).
    :
    m_pDevice(pDevice),
    m_info{},
    m_shaderMetaData{},
    m_gpuMem(),
    m_gpuMemSize(0),
    m_pPipelineBinary(nullptr),
    m_pipelineBinaryLen(0),
    m_perfDataInfo{},
    m_apiHwMapping{},
    m_uploadFenceToken(0),
    m_pagingFenceVal(0),
    m_flags{},
    m_perfDataMem(),
    m_perfDataGpuMemSize(0)
{
    m_flags.isInternal = isInternal;
}

// =====================================================================================================================
Pipeline::~Pipeline()
{
    if (m_gpuMem.IsBound())
    {
        m_pDevice->MemMgr()->FreeGpuMem(m_gpuMem.Memory(), m_gpuMem.Offset());
        m_gpuMem.Update(nullptr, 0);
    }

    if (m_perfDataMem.IsBound())
    {
        m_pDevice->MemMgr()->FreeGpuMem(m_perfDataMem.Memory(), m_perfDataMem.Offset());
        m_perfDataMem.Update(nullptr, 0);
    }

    ResourceDestroyEventData data = {};
    data.pObj = this;
    m_pDevice->GetPlatform()->GetEventProvider()->LogGpuMemoryResourceDestroyEvent(data);

    PAL_SAFE_FREE(m_pPipelineBinary, m_pDevice->GetPlatform());
}

// =====================================================================================================================
// Destroys a pipeline object allocated via a subclass' CreateInternal()
void Pipeline::DestroyInternal()
{
    PAL_ASSERT(IsInternal());

    Platform*const pPlatform = m_pDevice->GetPlatform();
    Destroy();
    PAL_FREE(this, pPlatform);
}

// =====================================================================================================================
// Allocates GPU memory for this pipeline and uploads the code and data contain in the ELF binary to it.  Any ELF
// relocations are also applied to the memory during this operation.
Result Pipeline::PerformRelocationsAndUploadToGpuMemory(
    const CodeObjectMetadata& metadata,
    const GpuHeap&            clientPreferredHeap,
    PipelineUploader*         pUploader)
{
    PAL_ASSERT(pUploader != nullptr);

    // Compute the total size of all shader stages' performance data buffers.
    gpusize performanceDataOffset = 0;
    for (uint32 s = 0; s < static_cast<uint32>(Abi::HardwareStage::Count); ++s)
    {
        const uint32 performanceDataBytes = metadata.pipeline.hardwareStage[s].perfDataBufferSize;
        if (performanceDataBytes != 0)
        {
            m_perfDataInfo[s].sizeInBytes = performanceDataBytes;
            m_perfDataInfo[s].cpuOffset   = static_cast<size_t>(performanceDataOffset);

            performanceDataOffset += performanceDataBytes;
        }
    } // for each hardware stage

    m_perfDataGpuMemSize = performanceDataOffset;
    Result result        = Result::Success;

    if (m_perfDataGpuMemSize > 0)
    {
        // Allocate gpu memory for the perf data.
        GpuMemoryCreateInfo createInfo = { };
        createInfo.heapCount           = 1;
        createInfo.heaps[0]            = GpuHeap::GpuHeapLocal;
        createInfo.alignment           = GpuMemByteAlign;
        createInfo.vaRange             = VaRange::DescriptorTable;
        createInfo.priority            = GpuMemPriority::High;
        createInfo.size                = m_perfDataGpuMemSize;

        GpuMemoryInternalCreateInfo internalInfo = { };
        internalInfo.flags.alwaysResident        = 1;

        GpuMemory* pGpuMem         = nullptr;
        gpusize    perfDataOffset  = 0;

        result = m_pDevice->MemMgr()->AllocateGpuMem(createInfo,
                                                     internalInfo,
                                                     false,
                                                     &pGpuMem,
                                                     &perfDataOffset);

        if (result == Result::Success)
        {
            m_perfDataMem.Update(pGpuMem, perfDataOffset);

            void* pPerfDataMapped = nullptr;
            result                  = pGpuMem->Map(&pPerfDataMapped);

            if (result == Result::Success)
            {
                memset(VoidPtrInc(pPerfDataMapped, static_cast<size_t>(perfDataOffset)),
                       0,
                       static_cast<size_t>(m_perfDataGpuMemSize));

                // Initialize the performance data buffer for each shader stage and finalize its GPU virtual address.
                for (uint32 s = 0; s < static_cast<uint32>(Abi::HardwareStage::Count); ++s)
                {
                    if (m_perfDataInfo[s].sizeInBytes != 0)
                    {
                        m_perfDataInfo[s].gpuVirtAddr =
                            LowPart(m_perfDataMem.GpuVirtAddr() + m_perfDataInfo[s].cpuOffset);
                    }
                } // for each hardware stage

                pGpuMem->Unmap();
            }
        }
    }

    if (result == Result::Success)
    {
        result = pUploader->Begin(metadata, clientPreferredHeap);

    }

    if (result == Result::Success)
    {
        result = pUploader->ApplyRelocations();
    }

    if (result == Result::Success)
    {
        m_pagingFenceVal = pUploader->PagingFenceVal();
        m_gpuMemSize     = pUploader->GpuMemSize();
        m_gpuMem.Update(pUploader->GpuMem(), pUploader->GpuMemOffset());
    }

    return result;
}

// =====================================================================================================================
// Helper function for extracting the pipeline hash and per-shader hashes from pipeline metadata.
void Pipeline::ExtractPipelineInfo(
    const CodeObjectMetadata& metadata,
    ShaderType                firstShader,
    ShaderType                lastShader)
{
    m_info.internalPipelineHash =
        { metadata.pipeline.internalPipelineHash[0], metadata.pipeline.internalPipelineHash[1] };

    // We don't expect the pipeline ABI to report a hash of zero.
    PAL_ALERT((metadata.pipeline.internalPipelineHash[0] | metadata.pipeline.internalPipelineHash[1]) == 0);

    for (uint32 s = static_cast<uint32>(firstShader); s <= static_cast<uint32>(lastShader); ++s)
    {
        Abi::ApiShaderType shaderType = PalToAbiShaderType[s];

        if (shaderType != Abi::ApiShaderType::Count)
        {
            const uint32 shaderTypeIdx  = static_cast<uint32>(shaderType);
            const auto&  shaderMetadata = metadata.pipeline.shader[shaderTypeIdx];

            m_info.shader[s].hash = { shaderMetadata.apiShaderHash[0], shaderMetadata.apiShaderHash[1] };
            m_apiHwMapping.apiShaders[shaderTypeIdx] = static_cast<uint8>(shaderMetadata.hardwareMapping);
        }
    }
}

// =====================================================================================================================
// Query this pipeline's Bound GPU Memory.
Result Pipeline::QueryAllocationInfo(
    size_t*                   pNumEntries,
    GpuMemSubAllocInfo* const pGpuMemList
    ) const
{
    Result result = Result::ErrorInvalidPointer;

    if (pNumEntries != nullptr)
    {
        (*pNumEntries) = 1;

        if (pGpuMemList != nullptr)
        {
            pGpuMemList[0].offset     = m_gpuMem.Offset();
            pGpuMemList[0].pGpuMemory = m_gpuMem.Memory();
            pGpuMemList[0].size       = m_gpuMemSize;
        }

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Extracts the pipeline's code object ELF binary.
Result Pipeline::GetCodeObject(
    uint32*    pSize,
    void*      pBuffer
    ) const
{
    Result result = Result::ErrorInvalidPointer;

    if (pSize != nullptr)
    {
        if ((m_pPipelineBinary != nullptr) && (m_pipelineBinaryLen != 0))
        {
            if (pBuffer == nullptr)
            {
                (*pSize) = static_cast<uint32>(m_pipelineBinaryLen);
                result = Result::Success;
            }
            else if ((*pSize) >= static_cast<uint32>(m_pipelineBinaryLen))
            {
                memcpy(pBuffer, m_pPipelineBinary, m_pipelineBinaryLen);
                result = Result::Success;
            }
            else
            {
                result = Result::ErrorInvalidMemorySize;
            }
        }
        else
        {
            result = Result::ErrorUnavailable;
        }
    }

    return result;
}

// =====================================================================================================================
// Extracts the binary shader instructions for a specific API shader stage.
Result Pipeline::GetShaderCode(
    ShaderType shaderType,
    size_t*    pSize,
    void*      pBuffer
    ) const
{
    const ShaderStageInfo*const pInfo = GetShaderStageInfo(shaderType);
    PAL_ASSERT(pInfo->codeLength != 0); // How did we get here if there's no shader code?!

    // To extract the shader code, we can re-parse the saved ELF binary and lookup the shader's program
    // instructions by examining the symbol table entry for that shader's entrypoint.
    AbiReader abiReader(m_pDevice->GetPlatform(), m_pPipelineBinary);
    Result result = abiReader.Init();
    if (result == Result::Success)
    {
        const Elf::SymbolTableEntry* pSymbol = abiReader.GetPipelineSymbol(
                Abi::GetSymbolForStage(Abi::PipelineSymbolType::ShaderMainEntry, pInfo->stageId));
        if (pSymbol != nullptr)
        {
            result = abiReader.GetElfReader().CopySymbol(*pSymbol, pSize, pBuffer);
        }
        else
        {
            result = Result::ErrorUnavailable;
        }
    }

    return result;
}

// =====================================================================================================================
// Extracts the performance data from GPU memory and copies it to the specified buffer.
Result Pipeline::GetPerformanceData(
    Util::Abi::HardwareStage hardwareStage,
    size_t*                  pSize,
    void*                    pBuffer)
{
    Result       result       = Result::ErrorUnavailable;
    const uint32 index        = static_cast<uint32>(hardwareStage);
    const auto&  perfDataInfo = m_perfDataInfo[index];

    if (pSize == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (perfDataInfo.sizeInBytes > 0)
    {
        if (pBuffer == nullptr)
        {
            (*pSize) = perfDataInfo.sizeInBytes;
            result   = Result::Success;
        }
        else if ((*pSize) >= perfDataInfo.sizeInBytes)
        {
            auto pPerfDataMem = m_perfDataMem.Memory();
            void* pData       = nullptr;
            result            = pPerfDataMem->Map(&pData);

            if (result == Result::Success)
            {
                memcpy(pBuffer, VoidPtrInc(pData, perfDataInfo.cpuOffset), perfDataInfo.sizeInBytes);
                result = pPerfDataMem->Unmap();
            }
        }
    }

    return result;
}

// =====================================================================================================================
// If pipeline may make indirect function calls, perform any late linking steps required to valid execution
// of the possible function calls.
// (this could include adjusting hardware resources such as GPRs or LDS space for the pipeline).
Result Pipeline::LinkWithLibraries(
    const IShaderLibrary*const* ppLibraryList,
    uint32                      libraryCount)
{
    // To be Implemented in needed Pipeline classes
    return Result::Unsupported;
}

// =====================================================================================================================
// Sets the total stack size for indirect shaders in the pipeline
void Pipeline::SetStackSizeInBytes(
    uint32 stackSizeInBytes)
{
    // To be Implemented in needed Pipeline classes
    PAL_ASSERT_ALWAYS();
}

// =====================================================================================================================
// Helper method which extracts shader statistics from the pipeline ELF binary for a particular hardware stage.
Result Pipeline::GetShaderStatsForStage(
    const ShaderStageInfo& stageInfo,
    const ShaderStageInfo* pStageInfoCopy, // Optional: Non-null if we care about copy shader statistics.
    ShaderStats*           pStats
    ) const
{
    PAL_ASSERT(pStats != nullptr);
    memset(pStats, 0, sizeof(ShaderStats));

    // We can re-parse the saved pipeline ELF binary to extract shader statistics.
    AbiReader abiReader(m_pDevice->GetPlatform(), m_pPipelineBinary);
    Result result = abiReader.Init();

    MsgPackReader      metadataReader;
    CodeObjectMetadata metadata;

    if (result == Result::Success)
    {
        result = abiReader.GetMetadata(&metadataReader, &metadata);
    }

    if (result == Result::Success)
    {
        const auto&  gpuInfo       = m_pDevice->ChipProperties();
        const auto&  stageMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(stageInfo.stageId)];

        pStats->common.numUsedSgprs = stageMetadata.sgprCount;
        pStats->common.numUsedVgprs = stageMetadata.vgprCount;

#if PAL_BUILD_GFX6
        if (gpuInfo.gfxLevel < GfxIpLevel::GfxIp9)
        {
            pStats->numAvailableSgprs = (stageMetadata.hasEntry.sgprLimit != 0) ? stageMetadata.sgprLimit
                                                                                : gpuInfo.gfx6.numShaderVisibleSgprs;
        }
#endif

        if (gpuInfo.gfxLevel >= GfxIpLevel::GfxIp9)
        {
            pStats->numAvailableSgprs = (stageMetadata.hasEntry.sgprLimit != 0) ? stageMetadata.sgprLimit
                                                                                : gpuInfo.gfx9.numShaderVisibleSgprs;
        }
        pStats->numAvailableVgprs = (stageMetadata.hasEntry.vgprLimit != 0) ? stageMetadata.vgprLimit
                                                                            : MaxVgprPerShader;

        pStats->common.ldsUsageSizeInBytes    =
            (stageMetadata.hasEntry.ldsSize           != 0) ? stageMetadata.ldsSize           : 0;
        pStats->common.scratchMemUsageInBytes =
            (stageMetadata.hasEntry.scratchMemorySize != 0) ? stageMetadata.scratchMemorySize : 0;

        pStats->common.flags.isWave32 =
            ((stageMetadata.hasEntry.wavefrontSize != 0) && (stageMetadata.wavefrontSize == 32));

        pStats->isaSizeInBytes = stageInfo.disassemblyLength;

        if (pStageInfoCopy != nullptr)
        {
            const auto& copyStageMetadata =
                metadata.pipeline.hardwareStage[static_cast<uint32>(pStageInfoCopy->stageId)];

            pStats->flags.copyShaderPresent = 1;

            pStats->copyShader.numUsedSgprs = copyStageMetadata.sgprCount;
            pStats->copyShader.numUsedVgprs = copyStageMetadata.vgprCount;

            pStats->copyShader.ldsUsageSizeInBytes    =
                (copyStageMetadata.hasEntry.ldsSize           != 0) ? copyStageMetadata.ldsSize           : 0;
            pStats->copyShader.scratchMemUsageInBytes =
                (copyStageMetadata.hasEntry.scratchMemorySize != 0) ? copyStageMetadata.scratchMemorySize : 0;

            pStats->copyShader.flags.isWave32 =
                (copyStageMetadata.hasEntry.wavefrontSize != 0) && (copyStageMetadata.wavefrontSize == 32);
        }
    }

    return result;
}

// =====================================================================================================================
// Calculates the size, in bytes, of the performance data buffers needed total for the entire pipeline.
size_t Pipeline::PerformanceDataSize(
    const CodeObjectMetadata& metadata
    ) const
{
    size_t dataSize = 0;

    for (uint32 i = 0; i < static_cast<uint32>(Abi::HardwareStage::Count); i++)
    {
        dataSize += metadata.pipeline.hardwareStage[i].perfDataBufferSize;
    }

    return dataSize;
}

// =====================================================================================================================
void Pipeline::DumpPipelineElf(
    const char*         pPrefix,
    const char*         pName         // Optional: Non-null if we want to use a human-readable name for the filename.
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    const PalSettings& settings = m_pDevice->Settings();
    uint64 hashToDump = settings.pipelineElfLogConfig.logHash;
    bool hashMatches = ((hashToDump == 0) || (m_info.internalPipelineHash.stable == hashToDump));

    const bool dumpInternal  = settings.pipelineElfLogConfig.logInternal;
    const bool dumpExternal  = settings.pipelineElfLogConfig.logExternal;
    const bool dumpPipeline  =
        (settings.logPipelineElf && hashMatches && ((dumpExternal && !IsInternal()) || (dumpInternal && IsInternal())));

    if (dumpPipeline)
    {
        const char*const pLogDir = &settings.pipelineElfLogConfig.logDirectory[0];

        // Create the directory. We don't care if it fails (existing is fine, failure is caught when opening the file).
        MkDir(pLogDir);

        char fileName[512] = { };
        if ((pName == nullptr) || (pName[0] == '\0'))
        {
            Snprintf(&fileName[0],
                     sizeof(fileName),
                     "%s/%s_0x%016llX.elf",
                     pLogDir,
                     pPrefix,
                     m_info.internalPipelineHash.stable);
        }
        else
        {
            Snprintf(&fileName[0], sizeof(fileName), "%s/%s_%s.elf", pLogDir, pPrefix, pName);
        }

        File file;
        file.Open(fileName, FileAccessWrite | FileAccessBinary);
        file.Write(m_pPipelineBinary, m_pipelineBinaryLen);
    }
#endif
}

// =====================================================================================================================
void* SectionInfo::GetCpuMappedAddr(
    gpusize offset
    ) const
{
    auto chunkIter = m_chunks.Begin();
    while (offset > chunkIter.Get().size)
    {
        offset -= chunkIter.Get().size;
        chunkIter.Next();
    }
    return Util::VoidPtrInc(chunkIter.Get().pCpuMappedAddr, static_cast<size_t>(offset));
}

// =====================================================================================================================
SectionInfo* SectionMemoryMap::AddSection(
    Util::ElfReader::SectionId sectionId,
    gpusize                    gpuVirtAddr,
    const void*                pCpuLocalAddr)
{
    SectionInfo info(&m_allocator, sectionId, gpuVirtAddr, pCpuLocalAddr);
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
    Util::ElfReader::SectionId sectionId
    ) const
{
    const SectionInfo* pSection = nullptr;
    for (uint32 i = 0; i < m_sections.NumElements(); i++)
    {
        const auto& section = m_sections.At(i);
        if (section.GetSectionId() == sectionId)
        {
            pSection = &section;
            break;
        }
    }
    return pSection;
}

// =====================================================================================================================
Result SectionAddressCalculator::AddSection(
    const Util::ElfReader::Reader& elfReader,
    Util::ElfReader::SectionId     sectionId)
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

    return m_sections.PushBack({sectionId, offset});
}

// =====================================================================================================================
PipelineUploader::PipelineUploader(
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
    m_pipelineHeapType(GpuHeap::GpuHeapCount),
    m_slotId(0),
    m_heapInvisUploadOffset(0)
{
}

// =====================================================================================================================
PipelineUploader::~PipelineUploader()
{
    PAL_ASSERT(m_pMappedPtr == nullptr); // If this fires, the caller forgot to call End()!
}

// =====================================================================================================================
Result PipelineUploader::UploadPipelineSections(
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
void PipelineUploader::PatchPipelineInternalSrdTable(
    ElfReader::SectionId dataSectionId)
{
    // The for loop which follows is entirely non-standard behavior for an ELF loader, but is intended to
    // only be temporary code.
    for (uint32 s = 0; s < static_cast<uint32>(Abi::HardwareStage::Count); ++s)
    {
        const Abi::PipelineSymbolType symbolType =
            Abi::GetSymbolForStage(Abi::PipelineSymbolType::ShaderIntrlTblPtr,
                static_cast<Abi::HardwareStage>(s));

        const Elf::SymbolTableEntry* pSymbol = m_abiReader.GetPipelineSymbol(symbolType);
        if ((pSymbol != nullptr) && (pSymbol->st_shndx == dataSectionId))
        {
            const SectionInfo*const pSectionInfo = m_memoryMap.FindSection(dataSectionId);
            PAL_ASSERT_MSG(pSectionInfo != nullptr, "Data section not uploaded to GPU");
            m_pDevice->GetGfxDevice()->PatchPipelineInternalSrdTable(
                pSectionInfo->GetCpuMappedAddr(pSymbol->st_value), // Dst
                VoidPtrInc(m_abiReader.GetElfReader().GetSectionData(pSymbol->st_shndx),
                           static_cast<size_t>(pSymbol->st_value)), // Src
                static_cast<size_t>(pSymbol->st_size),
                pSectionInfo->GetGpuVirtAddr());
        }
    } // for each hardware stage
}

// =====================================================================================================================
GpuHeap PipelineUploader::SelectUploadHeap(
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
Result PipelineUploader::Begin(
    const CodeObjectMetadata& metadata,
    GpuHeap                   heap)
{
    const PalSettings& settings = m_pDevice->Settings();
    Result result = Result::Success;

    SectionAddressCalculator addressCalculator(m_pDevice->GetPlatform());

    const ElfReader::Reader& elfReader = m_abiReader.GetElfReader();
    for (ElfReader::SectionId i = 0; i < elfReader.GetNumSections(); i++)
    {
        const auto& section = elfReader.GetSection(i);
        if (section.sh_flags & Elf::ShfAlloc)
        {
            result = addressCalculator.AddSection(elfReader, i);
            if (result != Result::Success)
            {
                break;
            }
        }
    }

    if (result == Result::Success)
    {
        m_prefetchSize = addressCalculator.GetSize();

        // The driver must make sure there is a distance of at least gpuInfo.shaderPrefetchBytes
        // that follows the end of the shader to avoid a page fault when the SQ tries to
        // prefetch past the end of a shader
        // shaderPrefetchBytes is set from "SQC_CONFIG.INST_PRF_COUNT" (gfx8-9)
        // defaulting to the hardware supported maximum if necessary
        const gpusize minSafeSize = Pow2Align(m_prefetchSize, ShaderICacheLineSize) +
                                    m_pDevice->ChipProperties().gfxip.shaderPrefetchBytes;

        m_gpuMemSize = Max(m_gpuMemSize, minSafeSize);

        GpuMemoryCreateInfo createInfo = { };
        createInfo.size      = m_gpuMemSize;
        createInfo.alignment = GpuMemByteAlign;
        createInfo.vaRange   = VaRange::DescriptorTable;
        createInfo.heaps[0]  = SelectUploadHeap(heap);
        createInfo.heaps[1]  = GpuHeapGartUswc;
        createInfo.heapCount = 2;
        createInfo.priority  = GpuMemPriority::High;

        GpuMemoryInternalCreateInfo internalInfo = { };
        internalInfo.flags.alwaysResident = 1;
        internalInfo.pPagingFence = &m_pagingFenceVal;

        result = m_pDevice->MemMgr()->AllocateGpuMem(createInfo, internalInfo, false, &m_pGpuMemory, &m_baseOffset);
    }

    void* pMappedPtr = nullptr;
    if (result == Result::Success)
    {
        if (ShouldUploadUsingDma())
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
Result PipelineUploader::UploadUsingDma(
    const SectionAddressCalculator& addressCalc,
    void**                          ppMappedPtr)
{
    Result result = m_pDevice->AcquireRingSlot(&m_slotId);
    if (result == Result::Success)
    {
        const gpusize gpuVirtAddr = (m_pGpuMemory->Desc().gpuVirtAddr + m_baseOffset);

        const ElfReader::Reader&   elfReader     = m_abiReader.GetElfReader();
        const ElfReader::SectionId dataSectionId = elfReader.FindSection(".data");
        for (auto sectionIter = addressCalc.GetSectionsBegin(); sectionIter.IsValid(); sectionIter.Next())
        {
            const SectionAddressCalculator::SectionOffset& section = sectionIter.Get();
            const void* pSectionData = elfReader.GetSectionData(section.sectionId);

            SectionInfo*const pInfo =
                m_memoryMap.AddSection(section.sectionId, (gpuVirtAddr + section.offset), pSectionData);
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

            if (dataSectionId == section.sectionId)
            {
                PatchPipelineInternalSrdTable(section.sectionId);
            }
        }

        if (result == Result::Success)
        {
            // Including potential padding space before data and register section, and potential padding space after
            // end of all sections in case total size of all pipeline sections is less than minSafeSize.
            const size_t dataRegisterAndPadding = static_cast<size_t>(m_gpuMemSize - m_heapInvisUploadOffset);
            if (dataRegisterAndPadding > 0)
            {
                m_pMappedPtr = PAL_CALLOC_ALIGNED(dataRegisterAndPadding,
                                                  GpuMemByteAlign,
                                                  m_pDevice->GetPlatform(),
                                                  AllocInternal);
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
Result PipelineUploader::UploadUsingCpu(
    const SectionAddressCalculator& addressCalc,
    void**                          ppMappedPtr)
{
    Result result = m_pGpuMemory->Map(&m_pMappedPtr);
    if (result == Result::Success)
    {
        m_pMappedPtr = VoidPtrInc(m_pMappedPtr, static_cast<size_t>(m_baseOffset));

        const gpusize gpuVirtAddr = (m_pGpuMemory->Desc().gpuVirtAddr + m_baseOffset);

        const ElfReader::Reader&   elfReader     = m_abiReader.GetElfReader();
        const ElfReader::SectionId dataSectionId = elfReader.FindSection(".data");
        for (auto sectionIter = addressCalc.GetSectionsBegin(); sectionIter.IsValid(); sectionIter.Next())
        {
            const SectionAddressCalculator::SectionOffset& section = sectionIter.Get();

            const void*  pSectionData = elfReader.GetSectionData(section.sectionId);
            const uint64 sectionSize  = elfReader.GetSection(section.sectionId).sh_size;
            void*const   pMappedPtr   = VoidPtrInc(m_pMappedPtr, static_cast<size_t>(section.offset));

            SectionInfo*const pInfo =
                m_memoryMap.AddSection(section.sectionId, (gpuVirtAddr + section.offset), pSectionData);
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

            if (dataSectionId == section.sectionId)
            {
                PatchPipelineInternalSrdTable(section.sectionId);
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
Result PipelineUploader::ApplyRelocations()
{
    Result result = Result::Success;
    // Apply relocations: Iterate through all REL sections
    Util::ElfReader::SectionId numSections = m_abiReader.GetElfReader().GetNumSections();
    for (Util::ElfReader::SectionId i = 0; i < numSections; i++)
    {
        auto type = m_abiReader.GetElfReader().GetSectionType(i);
        if ((type != Elf::SectionHeaderType::Rel) && (type != Elf::SectionHeaderType::Rela))
        {
            continue;
        }

        Util::ElfReader::Relocations relocs(m_abiReader.GetElfReader(), i);
        result = ApplyRelocationSection(relocs);
        if (result != Result::Success)
        {
            break;
        }
    }
    return result;
}

// =====================================================================================================================
// Applies the relocations of one section.
Result PipelineUploader::ApplyRelocationSection(
    const Util::ElfReader::Relocations& relocations)
{
    bool isRela = relocations.IsRela();
    // sh_info contains a reference to the target section where the
    // relocations should be performed.
    const SectionInfo* pMemInfo = m_memoryMap.FindSection(relocations.GetDestSection());
    Result result = Result::Success;

    if (pMemInfo == nullptr)
    {
        result = Result::ErrorInvalidPipelineElf;
    }

    if (result == Result::Success)
    {
        // sh_link contains a reference to the symbol section
        Util::ElfReader::Symbols symbols(m_abiReader.GetElfReader(), relocations.GetSymbolSection());
        Util::ElfReader::SectionId dstSection = relocations.GetDestSection();

        // We have three types of addresses:
        // 1. Virtual GPU addresses, these will be written into the destination
        // 2. The CPU address of the ELF, we read from there because it is fast
        // 3. The CPU mapped address of the destination section on the GPU, we write to that address
        const void* pSecSrcAddr = m_abiReader.GetElfReader().GetSectionData(relocations.GetDestSection());

        for (uint64 i = 0; i < relocations.GetNumRelocations(); i++)
        {
            const Elf::RelTableEntry& relocation = relocations.GetRel(i);
            const Util::Elf::SymbolTableEntry& symbol = symbols.GetSymbol(relocation.r_info.sym);

            // Get address of referenced symbol
            const SectionInfo* pSymSection = m_memoryMap.FindSection(symbol.st_shndx);
            if (pSymSection == nullptr)
            {
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
                uint32 addend32 = 0;
                switch (relType) {
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

            uint32 val32 = 0;
            uint64 val64 = 0;
            switch (relType) {
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
Result PipelineUploader::End(
    UploadFenceToken* pCompletionFence)
{
    Result result = Result::Success;

    if (m_pGpuMemory != nullptr)
    {
        if (ShouldUploadUsingDma())
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
Result PipelineUploader::GetPipelineGpuSymbol(
    Abi::PipelineSymbolType type,
    GpuSymbol*              pSymbol) const
{
    return GetAbsoluteSymbolAddress(m_abiReader.GetPipelineSymbol(type), pSymbol);
}

// =====================================================================================================================
Result PipelineUploader::GetGenericGpuSymbol(
    const char* pName,
    GpuSymbol*  pSymbol) const
{
    return GetAbsoluteSymbolAddress(m_abiReader.GetGenericSymbol(pName), pSymbol);
}

// =====================================================================================================================
Result PipelineUploader::GetAbsoluteSymbolAddress(
    const Util::Elf::SymbolTableEntry* pElfSymbol,
    GpuSymbol*                         pSymbol) const
{
    Result result = Result::Success;
    if (pElfSymbol != nullptr)
    {
        pSymbol->gpuVirtAddr = pElfSymbol->st_value;
        pSymbol->size = pElfSymbol->st_size;
        const SectionInfo* pSection = m_memoryMap.FindSection(pElfSymbol->st_shndx);
        if (pSection != nullptr)
        {
            pSymbol->gpuVirtAddr += pSection->GetGpuVirtAddr();
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

} // Pal
