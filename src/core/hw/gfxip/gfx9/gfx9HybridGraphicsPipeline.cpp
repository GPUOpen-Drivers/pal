/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9GraphicsShaderLibrary.h"
#include "palPipelineAbi.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
HybridGraphicsPipeline::HybridGraphicsPipeline(
    Device* pDevice)
    :
    GraphicsPipeline(pDevice, false),
    m_task(*pDevice, &m_taskStageInfo, &m_perfDataInfo[static_cast<uint32>(Abi::HardwareStage::Cs)]),
    m_taskStageInfo(),
    m_taskSignature{NullCsSignature}
#if PAL_BUILD_GFX11
    , m_shPairsPacketSupportedCs(pDevice->Settings().gfx11EnableShRegPairOptimizationCs)
#endif
{
}

// =====================================================================================================================
Result HybridGraphicsPipeline::HwlInit(
    const GraphicsPipelineCreateInfo& createInfo,
    const AbiReader&                  abiReader,
    const PalAbi::CodeObjectMetadata& metadata,
    MsgPackReader*                    pMetadataReader)
{
    GraphicsPipelineLoadInfo loadInfo = {};
    GraphicsPipeline::EarlyInit(metadata, &loadInfo);

    PipelineUploader uploader(m_pDevice->Parent(), abiReader);

    Result result = PerformRelocationsAndUploadToGpuMemory(
            metadata,
            IsInternal() ? GpuHeapLocal : m_pDevice->Parent()->GetPublicSettings()->pipelinePreferredHeap,
            &uploader);

    if (result == Result::Success)
    {
        LateInit(createInfo, abiReader, metadata, loadInfo, &uploader);

        m_task.SetupSignatureFromElf(&m_taskSignature, metadata);

        // We opt to pass the graphics pipeline metadata bit to the task shader signature here instead of in the
        // above task shader (or compute shader) function is due to that, task shader is actually a graphics
        // shader that is used by hybrid graphics pipeline.
        // This bit is placed in taskSignature but not in graphicsSignature since linear dispatch is derived
        // from task shader SC output.
        m_taskSignature.flags.isLinear = metadata.pipeline.graphicsRegister.flags.meshLinearDispatchFromTask;

        const uint32 wavefrontSize = m_taskSignature.flags.isWave32 ? 32 : 64;

        // Number of threads per threadgroup in each dimension as determined by parsing the input IL.
        DispatchDims threadsPerTg = {};
        m_task.LateInit(metadata,
                        wavefrontSize,
                        &threadsPerTg,
#if PAL_BUILD_GFX11
                        createInfo.taskInterleaveSize,
#endif
                        & uploader);

        const auto* pElfSymbol = abiReader.GetPipelineSymbol(Abi::PipelineSymbolType::CsDisassembly);

        if (pElfSymbol != nullptr)
        {
            m_taskStageInfo.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
        }

        PAL_ASSERT(m_uploadFenceToken == 0);
        result = uploader.End(&m_uploadFenceToken);
    }

    return result;
}

// =====================================================================================================================
// Link graphics pipeline from graphics shader libraries.
Result HybridGraphicsPipeline::LinkGraphicsLibraries(
    const GraphicsPipelineCreateInfo& createInfo)
{
    Result                       result   = Result::Success;
    const Gfx9PalSettings&       settings = m_pDevice->Settings();
    const GraphicsShaderLibrary* pTaskLib = nullptr;

    result = GraphicsPipeline::LinkGraphicsLibraries(createInfo);

    if (result == Result::Success)
    {
        for (uint32 i = 0; i < NumGfxShaderLibraries(); i++)
        {
            const GraphicsShaderLibrary* pLib =
                reinterpret_cast<const GraphicsShaderLibrary*>(GetGraphicsShaderLibrary(i));
            uint32 apiShaderMask = pLib->GetApiShaderMask();
            if (Util::TestAnyFlagSet(apiShaderMask, 1 << static_cast<uint32>(ShaderType::Task)))
            {
                pTaskLib = pLib;
                break;
            }
        }
        PAL_ASSERT(pTaskLib != nullptr);
        m_task.Clone(pTaskLib->GetTaskChunk());
        m_taskStageInfo = pTaskLib->GetTaskStageInfo();
        m_taskSignature = pTaskLib->GetTaskSignature();
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
    pCmdSpace = m_task.WriteShCommands(pGfx9CmdStream,
                                       pCmdSpace,
#if PAL_BUILD_GFX11
                                       m_shPairsPacketSupportedCs,
#endif
                                       info,
                                       0uLL,
                                       prefetch);

    return pCmdSpace;
}

} // namespace Gfx9
} // namespace Pal
