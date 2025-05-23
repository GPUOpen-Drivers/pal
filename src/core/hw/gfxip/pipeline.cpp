/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace Pal
{

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
    m_gpuMemOffset(0),
    m_pipelineBinary(),
    m_perfDataInfo{},
    m_apiHwMapping{},
    m_uploadFenceToken(0),
    m_pagingFenceVal(0),
    m_flags{},
    m_perfDataMem(),
    m_perfDataGpuMemSize(0),
    m_pSelf(this)
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
    m_pDevice->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceDestroyEvent(data);

    PAL_FREE(m_pipelineBinary.Data(), m_pDevice->GetPlatform());
    m_pipelineBinary = {};
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
    const gpusize             performanceDataOffset,
    const GpuHeap&            clientPreferredHeap,
    CodeObjectUploader*       pUploader)
{
    PAL_ASSERT(pUploader != nullptr);
    m_perfDataGpuMemSize = performanceDataOffset;
    Result result        = Result::Success;

    if (m_perfDataGpuMemSize > 0)
    {
        // Allocate gpu memory for the perf data.
        GpuMemoryCreateInfo createInfo = { };
        createInfo.heapCount           = 1;
        createInfo.heaps[0]            = GpuHeap::GpuHeapLocal;
        createInfo.alignment           = GpuSectionMemByteAlign;
        createInfo.vaRange             = VaRange::DescriptorTable;
        createInfo.priority            = GpuMemPriority::High;
        createInfo.size                = m_perfDataGpuMemSize;

        GpuMemoryInternalCreateInfo internalInfo = { };
        internalInfo.flags.alwaysResident        = 1;
        internalInfo.flags.appRequested          = (IsInternal() == false);

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
            result                = pGpuMem->Map(&pPerfDataMapped);

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
        result = pUploader->Begin(clientPreferredHeap, IsInternal());
    }

    if (result == Result::Success)
    {
        result = pUploader->ApplyRelocations();
    }

    if (result == Result::Success)
    {
        m_pagingFenceVal = pUploader->PagingFenceVal();
        m_gpuMemOffset   = pUploader->SectionOffset();
        m_gpuMemSize     = pUploader->GpuMemSize();
        PAL_ASSERT(m_gpuMemOffset < m_gpuMemSize);
        m_gpuMem.Update(pUploader->GpuMem(), pUploader->GpuMemOffset());
    }

    return result;
}

// =====================================================================================================================
// Allocates GPU memory for this pipeline and uploads the code and data contain in the ELF binary to it.  Any ELF
// relocations are also applied to the memory during this operation.
Result Pipeline::PerformRelocationsAndUploadToGpuMemory(
    const PalAbi::CodeObjectMetadata& metadata,
    const GpuHeap&                    clientPreferredHeap,
    CodeObjectUploader*               pUploader)
{
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

    return PerformRelocationsAndUploadToGpuMemory(performanceDataOffset, clientPreferredHeap, pUploader);
}

// =====================================================================================================================
// Helper function for extracting the pipeline hash and per-shader hashes from pipeline metadata.
void Pipeline::ExtractPipelineInfo(
    const PalAbi::CodeObjectMetadata& metadata,
    ShaderType                        firstShader,
    ShaderType                        lastShader)
{
    m_info.internalPipelineHash =
        { metadata.pipeline.internalPipelineHash[0], metadata.pipeline.internalPipelineHash[1] };
    m_info.resourceMappingHash  = metadata.pipeline.resourceHash;

    // We don't expect the pipeline ABI to report a hash of zero.
    PAL_ALERT((metadata.pipeline.internalPipelineHash[0] | metadata.pipeline.internalPipelineHash[1]) == 0);

    for (uint32 s = static_cast<uint32>(firstShader); s <= static_cast<uint32>(lastShader); ++s)
    {
        Abi::ApiShaderType shaderType = PalShaderTypeToAbiShaderType(static_cast<ShaderType>(s));

        if (shaderType != Abi::ApiShaderType::Count)
        {
            const uint32 shaderTypeIdx  = static_cast<uint32>(shaderType);
            const auto&  shaderMetadata = metadata.pipeline.shader[shaderTypeIdx];

            m_info.shader[s].hash = { shaderMetadata.apiShaderHash[0], shaderMetadata.apiShaderHash[1] };
            m_apiHwMapping.apiShaders[shaderTypeIdx] = static_cast<uint8>(shaderMetadata.hardwareMapping);
        }
    }

    if (metadata.pipeline.hasEntry.usesCps != 0)
    {
        m_info.flags.usesCps = metadata.pipeline.flags.usesCps;
    }
    if (metadata.pipeline.hasEntry.cpsGlobal != 0)
    {
        m_info.flags.cpsGlobal = metadata.pipeline.flags.cpsGlobal;
    }

    m_info.ps.flags.usesSampleMask = metadata.pipeline.flags.psSampleMask;
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
        if (m_gpuMem.Memory() != nullptr)
        {
            (*pNumEntries) = 1;

            if (pGpuMemList != nullptr)
            {
                pGpuMemList[0].address = m_gpuMem.Memory()->Desc().gpuVirtAddr;
                pGpuMemList[0].offset  = m_gpuMem.Offset() + m_gpuMemOffset;
                pGpuMemList[0].size    = m_gpuMemSize      - m_gpuMemOffset;
            }
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
        if ((m_pipelineBinary.Data() != nullptr) && (m_pipelineBinary.SizeInBytes() != 0))
        {
            if (pBuffer == nullptr)
            {
                (*pSize) = static_cast<uint32>(m_pipelineBinary.SizeInBytes());
                result = Result::Success;
            }
            else if ((*pSize) >= static_cast<uint32>(m_pipelineBinary.SizeInBytes()))
            {
                memcpy(pBuffer, m_pipelineBinary.Data(), m_pipelineBinary.SizeInBytes());
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
// Gets the code object pointer according to shader type.
const void* Pipeline::GetCodeObjectWithShaderType(
    ShaderType shaderType,
    size_t*    pSize
    ) const
{
    const void* pBinary = nullptr;

    pBinary = m_pipelineBinary.Data();
    if (pSize != nullptr)
    {
        *pSize = m_pipelineBinary.SizeInBytes();
    }

    return pBinary;
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
    size_t      size    = 0;
    const void* pBinary = GetCodeObjectWithShaderType(shaderType, &size);
    AbiReader   abiReader(m_pDevice->GetPlatform(), {pBinary, size});
    Result      result = abiReader.Init();

    if (result == Result::Success)
    {
        MsgPackReader reader;
        PalAbi::CodeObjectMetadata metadata = { };

        result = abiReader.GetMetadata(&reader, &metadata);

        if (result == Result::Success)
        {
            const PalAbi::HardwareStageMetadata& stageMetadata =
                metadata.pipeline.hardwareStage[uint32(pInfo->stageId)];
            const Abi::PipelineSymbolType defaultSym =
                Abi::GetSymbolForStage(Abi::PipelineSymbolType::ShaderMainEntry, pInfo->stageId);
            const StringView<char> defaultSymName    = Abi::PipelineAbiSymbolNameStrings[uint32(defaultSym)];

            const bool isDefaultEntryPoint = (PipelineSupportsGenericEntryPoint(metadata) == false) ||
                (stageMetadata.hasEntry.entryPointSymbol == 0) || (stageMetadata.entryPointSymbol == defaultSymName);

            result = ((pInfo->stageId == Abi::HardwareStage::Cs) && (isDefaultEntryPoint == false)) ?
                abiReader.CopySymbol(stageMetadata.entryPointSymbol, pSize, pBuffer) :
                abiReader.CopySymbol(defaultSym, pSize, pBuffer);
        }

        if (result == Result::NotFound)
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
// Get the frontend and backend stack sizes
Result Pipeline::GetStackSizes(
    CompilerStackSizes* pSizes
    ) const
{
    // To be Implemented in needed Pipeline classes
    return Result::Unsupported;
}

// =====================================================================================================================
// Helper method which extracts shader statistics from the pipeline ELF binary for a particular hardware stage.
Result Pipeline::GetShaderStatsForStage(
    ShaderType             shaderType,
    const ShaderStageInfo& stageInfo,
    const ShaderStageInfo* pStageInfoCopy, // Optional: Non-null if we care about copy shader statistics.
    ShaderStats*           pStats
    ) const
{
    PAL_ASSERT(pStats != nullptr);
    memset(pStats, 0, sizeof(ShaderStats));

    // We can re-parse the saved pipeline ELF binary to extract shader statistics.
    size_t      size            = 0;
    const void* pPipelineBinary = GetCodeObjectWithShaderType(shaderType, &size);

    PAL_ASSERT(pPipelineBinary != nullptr);
    AbiReader abiReader(m_pDevice->GetPlatform(), Span<const void>{pPipelineBinary, size});
    Result result = abiReader.Init();

    PalAbi::CodeObjectMetadata metadata;

    if (result == Result::Success)
    {
        MsgPackReader metadataReader;
        result = abiReader.GetMetadata(&metadataReader, &metadata);
    }

    if (result == Result::Success)
    {
        const auto&  gpuInfo       = m_pDevice->ChipProperties();
        const auto&  stageMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(stageInfo.stageId)];

        pStats->common.numUsedSgprs = stageMetadata.sgprCount;
        pStats->common.numUsedVgprs = stageMetadata.vgprCount;

        pStats->numAvailableSgprs = (stageMetadata.hasEntry.sgprLimit != 0) ? stageMetadata.sgprLimit
                                                                            : gpuInfo.gfx9.numShaderVisibleSgprs;
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
    const PalAbi::CodeObjectMetadata& metadata
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
    Util::StringView<char> prefix,
    Util::StringView<char> name     // Optional: Can be the empty string if a human-readable filename is not desired.
    ) const
{
    m_pDevice->LogCodeObjectToDisk(
        prefix,
        name,
        m_info.internalPipelineHash,
        IsInternal(),
        m_pipelineBinary.Data(),
        m_pipelineBinary.SizeInBytes());
}

// =====================================================================================================================
bool Pipeline::DispatchInterleaveSizeIsValid(
    DispatchInterleaveSize   interleave,
    const GpuChipProperties& chipProps)
{
    bool is1D = false;
#if PAL_BUILD_GFX12
    bool is2D = false;
#endif

    switch (interleave)
    {
    case DispatchInterleaveSize::Default:
    case DispatchInterleaveSize::Disable:
        break;
    case DispatchInterleaveSize::_1D_64_Threads:
    case DispatchInterleaveSize::_1D_128_Threads:
    case DispatchInterleaveSize::_1D_256_Threads:
    case DispatchInterleaveSize::_1D_512_Threads:
        is1D = true;
        break;
#if PAL_BUILD_GFX12
    case DispatchInterleaveSize::_2D_1x1_ThreadGroups:
    case DispatchInterleaveSize::_2D_1x2_ThreadGroups:
    case DispatchInterleaveSize::_2D_1x4_ThreadGroups:
    case DispatchInterleaveSize::_2D_1x8_ThreadGroups:
    case DispatchInterleaveSize::_2D_1x16_ThreadGroups:
    case DispatchInterleaveSize::_2D_2x1_ThreadGroups:
    case DispatchInterleaveSize::_2D_2x2_ThreadGroups:
    case DispatchInterleaveSize::_2D_2x4_ThreadGroups:
    case DispatchInterleaveSize::_2D_2x8_ThreadGroups:
    case DispatchInterleaveSize::_2D_4x1_ThreadGroups:
    case DispatchInterleaveSize::_2D_4x2_ThreadGroups:
    case DispatchInterleaveSize::_2D_4x4_ThreadGroups:
    case DispatchInterleaveSize::_2D_8x1_ThreadGroups:
    case DispatchInterleaveSize::_2D_8x2_ThreadGroups:
    case DispatchInterleaveSize::_2D_16x1_ThreadGroups:
        is2D = true;
        break;
#endif
    default:
        PAL_ASSERT_ALWAYS();
        break;
    };

    bool isValid = true;

#if PAL_BUILD_GFX12
    if ((is1D && (chipProps.gfxip.support1dDispatchInterleave == false)) ||
        (is2D && (chipProps.gfxip.support2dDispatchInterleave == false)))
#else
    if (is1D && (chipProps.gfxip.support1dDispatchInterleave == false))
#endif
    {
        isValid = false;
    }

    return isValid;
};

// =====================================================================================================================
void Pipeline::MergePagingAndUploadFences(
    Util::Span<const IShaderLibrary* const> libraries)
{
    for (auto library : libraries)
    {
        const auto* const pLibObj = static_cast<const ShaderLibrary* const>(library);
        m_uploadFenceToken        = Max(m_uploadFenceToken, pLibObj->GetUploadFenceToken());
        m_pagingFenceVal          = Max(m_pagingFenceVal, pLibObj->GetPagingFenceVal());
    }
}

} // Pal
