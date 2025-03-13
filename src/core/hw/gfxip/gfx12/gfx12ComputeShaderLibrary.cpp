/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx12/gfx12ComputeShaderLibrary.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12PipelineChunkCs.h"

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
ComputeShaderLibrary::ComputeShaderLibrary(
    Device* pDevice)
    :
    Pal::ComputeShaderLibrary(pDevice->Parent()),
    m_hwInfo{},
    m_isWave32(false)
    , m_pUserDataLayout(nullptr)
{
}

// =====================================================================================================================
ComputeShaderLibrary::~ComputeShaderLibrary()
{
    if (m_pUserDataLayout != nullptr)
    {
        m_pUserDataLayout->Destroy();
        m_pUserDataLayout = nullptr;
    }
}

// =====================================================================================================================
// Obtains the compiled shader ISA code for the shader specified.
Result ComputeShaderLibrary::GetShaderFunctionCode(
    Util::StringView<char> shaderExportName,
    size_t*                pSize,
    void*                  pBuffer
    ) const
{
    // To extract the shader code, we can re-parse the saved ELF binary and lookup the shader's program
    // instructions by examining the symbol table entry for that shader's entrypoint.
    AbiReader abiReader(m_pDevice->GetPlatform(), m_codeObject);
    Result    result = abiReader.Init();

    if (result == Result::Success)
    {
        result = abiReader.CopySymbol(shaderExportName, pSize, pBuffer);
    }

    return result;
}

// =====================================================================================================================
// Obtains the shader pre and post compilation stats/params for the specified shader.
Result ComputeShaderLibrary::GetShaderFunctionStats(
    Util::StringView<char> shaderExportName,
    ShaderLibStats*        pShaderStats
    ) const
{
    Result result = Result::Success;

    const GpuChipProperties& chipProps = m_pDevice->ChipProperties();

    PAL_ASSERT(pShaderStats != nullptr);
    memset(pShaderStats, 0, sizeof(ShaderLibStats));

    pShaderStats->palInternalLibraryHash       = m_info.internalLibraryHash;
    pShaderStats->common.ldsSizePerThreadGroup = chipProps.gfxip.ldsSizePerThreadGroup;
    pShaderStats->common.flags.isWave32        = IsWave32();

    // We can re-parse the saved pipeline ELF binary to extract shader statistics.
    AbiReader abiReader(m_pDevice->GetPlatform(), m_codeObject);
    result = abiReader.Init();
    if (result == Result::Success)
    {
        const Elf::SymbolTableEntry* pSymbol = abiReader.GetSymbolHeader(shaderExportName);
        if (pSymbol != nullptr)
        {
            pShaderStats->isaSizeInBytes = static_cast<size_t>(pSymbol->st_size);
        }
    }

    MsgPackReader              metadataReader;
    PalAbi::CodeObjectMetadata metadata;

    if (result == Result::Success)
    {
        result = abiReader.GetMetadata(&metadataReader, &metadata);
    }

    if (result == Result::Success)
    {
        const auto&  stageMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Cs)];

        pShaderStats->numAvailableSgprs = (stageMetadata.hasEntry.sgprLimit != 0)
            ? stageMetadata.sgprLimit
            : chipProps.gfx9.numShaderVisibleSgprs;
        pShaderStats->numAvailableVgprs = (stageMetadata.hasEntry.vgprLimit != 0)
            ? stageMetadata.vgprLimit
            : MaxVgprPerShader;

        pShaderStats->common.scratchMemUsageInBytes = stageMetadata.scratchMemorySize;
    }

    if (result == Result::Success)
    {
        result = UnpackShaderFunctionStats(shaderExportName,
                                           metadata,
                                           &metadataReader,
                                           pShaderStats);
    }

    return result;
}

// =====================================================================================================================
// Initializes HW-specific state related to this shader library object (register values, user-data mapping, etc.)
// using the specified library ABI processor.
Result ComputeShaderLibrary::HwlInit(
    const ShaderLibraryCreateInfo&    createInfo,
    const AbiReader&                  abiReader,
    const PalAbi::CodeObjectMetadata& metadata,
    Util::MsgPackReader*              pMetadataReader)
{
    Device* pDevice = static_cast<Device*>(m_pDevice->GetGfxDevice());

    Result result = Result::Success;

    result = ComputeUserDataLayout::Create(*m_pDevice, metadata.pipeline, &m_pUserDataLayout);

    CodeObjectUploader uploader(m_pDevice, abiReader);

    if (result == Result::Success)
    {
        result = PerformRelocationsAndUploadToGpuMemory(
            metadata, m_pDevice->GetPublicSettings()->pipelinePreferredHeap, &uploader);
    }

    if (result == Result::Success)
    {
        result = InitFunctionListFromMetadata(metadata, pMetadataReader);
    }

    if (result == Result::Success)
    {
        RegisterValuePair regs[Regs::Size()];
        Regs::Init(regs);
        // Indirect calls use the shader function entry.
        ShaderLibStats libStats = {};
        for (uint32 i = 0; i < m_functionList.NumElements(); i++)
        {
            ShaderLibStats currLibStats = {};
            UnpackShaderFunctionStats(m_functionList[i].symbolName, metadata, pMetadataReader, &currLibStats);

            libStats.common.numUsedVgprs = Max(currLibStats.common.numUsedVgprs, libStats.common.numUsedVgprs);
            libStats.common.ldsUsageSizeInBytes =
                Max(currLibStats.common.ldsUsageSizeInBytes, libStats.common.ldsUsageSizeInBytes);
        }

        PipelineChunkCs::SetComputeShaderState<Regs>(
            m_pDevice, metadata, &libStats, uploader, false, regs, &m_isWave32);

        m_hwInfo.libRegs.computePgmRsrc1 = Regs::GetC<mmCOMPUTE_PGM_RSRC1, COMPUTE_PGM_RSRC1>(regs);
        m_hwInfo.libRegs.computePgmRsrc2 = Regs::GetC<mmCOMPUTE_PGM_RSRC2, COMPUTE_PGM_RSRC2>(regs);
        m_hwInfo.libRegs.computePgmRsrc3 = Regs::GetC<mmCOMPUTE_PGM_RSRC3, COMPUTE_PGM_RSRC3>(regs);

        // Must be called after InitFunctionListFromMetadata!
        GetFunctionGpuVirtAddrs(uploader, m_functionList.Data(), m_functionList.NumElements());

        PAL_ASSERT(m_uploadFenceToken == 0);
        result = uploader.End(&m_uploadFenceToken);
    }

    if (result == Result::Success)
    {
        ResourceDescriptionShaderLibrary desc = {};
        desc.pLibrarynfo  = &GetInfo();
        desc.pCreateFlags = &createInfo.flags;
        ResourceCreateEventData data = {};
        data.type              = ResourceType::Pipeline;
        data.pResourceDescData = &desc;
        data.resourceDescSize  = sizeof(ResourceDescriptionShaderLibrary);
        data.pObj              = this;
        m_pDevice->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceCreateEvent(data);

        GpuMemoryResourceBindEventData bindData = {};
        bindData.pObj               = this;
        bindData.pGpuMemory         = m_gpuMem.Memory();
        bindData.requiredGpuMemSize = m_gpuMemSize - m_gpuMemOffset;
        bindData.offset             = m_gpuMem.Offset() + m_gpuMemOffset;
        m_pDevice->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceBindEvent(bindData);

        Developer::BindGpuMemoryData callbackData = {};
        callbackData.pObj               = bindData.pObj;
        callbackData.requiredGpuMemSize = bindData.requiredGpuMemSize;
        callbackData.pGpuMemory         = bindData.pGpuMemory;
        callbackData.offset             = bindData.offset;
        callbackData.isSystemMemory     = bindData.isSystemMemory;
        m_pDevice->DeveloperCb(Developer::CallbackType::BindGpuMemory, &callbackData);
    }

    return result;
}

}
}
