/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9HybridGraphicsPipeline.h"
#include "palPipelineAbi.h"

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
HybridGraphicsPipeline::HybridGraphicsPipeline(
    Device* pDevice)
    :
    GraphicsPipeline(pDevice, false),
    m_task(*pDevice, &m_taskStageInfo, &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Cs)]),
    m_taskStageInfo(),
    m_taskSignature()
{
}

// =====================================================================================================================
Result HybridGraphicsPipeline::HwlInit(
    const GraphicsPipelineCreateInfo& createInfo,
    const AbiReader&                  abiReader,
    const CodeObjectMetadata&         metadata,
    Util::MsgPackReader*              pMetadataReader)
{
    RegisterVector registers(m_pDevice->GetPlatform());
    Result result = pMetadataReader->Seek(metadata.pipeline.registers);

    if (result == Result::Success)
    {
        result = pMetadataReader->Unpack(&registers);
    }

    if (result == Result::Success)
    {
        GraphicsPipelineLoadInfo loadInfo = {};
        GraphicsPipeline::EarlyInit(metadata, registers, &loadInfo);

        // We ignore the number of SH registers out of early init to avoid adding the Task Shader to the graphics
        // load path.
        m_task.EarlyInit();

        GraphicsPipelineUploader uploader(m_pDevice,
                                          abiReader,
                                          loadInfo.loadedCtxRegCount,
                                          loadInfo.loadedShRegCount);

        result = PerformRelocationsAndUploadToGpuMemory(
            metadata,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 631
            (createInfo.flags.overrideGpuHeap == 1) ? createInfo.preferredHeapType : GpuHeapInvisible,
#else
            IsInternal() ? GpuHeapLocal : m_pDevice->Parent()->GetPublicSettings()->pipelinePreferredHeap,
#endif
            &uploader);

        if (result == Result::Success)
        {
            LateInit(createInfo, abiReader, metadata, registers, loadInfo, &uploader);

            m_task.SetupSignatureFromElf(&m_taskSignature, metadata, registers);

            const uint32 wavefrontSize = m_taskSignature.flags.isWave32 ? 32 : 64;

            m_task.LateInit<GraphicsPipelineUploader>(abiReader,
                                                      registers,
                                                      wavefrontSize,
                                                      &m_threadsPerTgX,
                                                      &m_threadsPerTgY,
                                                      &m_threadsPerTgZ,
                                                      true,
                                                      &uploader);

            const Util::Elf::SymbolTableEntry* pElfSymbol =
                abiReader.GetPipelineSymbol(Util::Abi::PipelineSymbolType::CsDisassembly);
            if (pElfSymbol != nullptr)
            {
                m_taskStageInfo.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
            }

            PAL_ASSERT(m_uploadFenceToken == 0);
            result = uploader.End(&m_uploadFenceToken);
        }
    }

    if (result == Result::Success)
    {
        ResourceDescriptionPipeline desc = {};
        desc.pPipelineInfo               = &GetInfo();
        desc.pCreateFlags                = &createInfo.flags;
        ResourceCreateEventData data     = {};
        data.type                        = ResourceType::Pipeline;
        data.pResourceDescData           = &desc;
        data.resourceDescSize            = sizeof(ResourceDescriptionPipeline);
        data.pObj                        = this;
        m_pDevice->GetPlatform()->GetEventProvider()->LogGpuMemoryResourceCreateEvent(data);

        GpuMemoryResourceBindEventData bindData = {};
        bindData.pObj                           = this;
        bindData.pGpuMemory                     = m_gpuMem.Memory();
        bindData.requiredGpuMemSize             = m_gpuMemSize;
        bindData.offset                         = m_gpuMem.Offset();
        m_pDevice->GetPlatform()->GetEventProvider()->LogGpuMemoryResourceBindEvent(bindData);
    }

    return result;
}

// =====================================================================================================================
const ShaderStageInfo* HybridGraphicsPipeline::GetShaderStageInfo(
    ShaderType shaderType
    ) const
{
    const ShaderStageInfo* pInfo = nullptr;

    if (shaderType == ShaderType::Task)
    {
        pInfo = &m_taskStageInfo;
    }
    else
    {
        pInfo = GraphicsPipeline::GetShaderStageInfo(shaderType);
    }

    return pInfo;
}

// =====================================================================================================================
Result HybridGraphicsPipeline::GetShaderStats(
    ShaderType   shaderType,
    ShaderStats* pShaderStats,
    bool         getDisassemblySize
    ) const
{
    Result result = GraphicsPipeline::GetShaderStats(shaderType, pShaderStats, getDisassemblySize);
    if ((result == Result::Success) && (shaderType == ShaderType::Task))
    {
        pShaderStats->shaderStageMask       = ApiShaderStageTask;
        pShaderStats->common.gpuVirtAddress = m_task.CsProgramGpuVa();
    }
    return result;
}

// =====================================================================================================================
uint32* HybridGraphicsPipeline::WriteTaskCommands(
    CmdStream*                      pCmdStream,
    uint32*                         pCmdSpace,
    const DynamicComputeShaderInfo& info,
    bool                            prefetch
    ) const
{
    auto* pGfx9CmdStream = static_cast<CmdStream*>(pCmdStream);
    pCmdSpace = m_task.WriteShCommands(pGfx9CmdStream, pCmdSpace, info, prefetch);
    return pCmdSpace;
}

} // namespace Gfx9
} // namespace Pal
