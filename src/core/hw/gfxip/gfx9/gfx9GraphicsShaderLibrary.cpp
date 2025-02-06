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

#include "core/hw/gfxip/gfx9/gfx9GraphicsShaderLibrary.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
GraphicsShaderLibrary::GraphicsShaderLibrary(
    Device* pDevice)
    :
    Pal::GraphicsShaderLibrary(pDevice->Parent()),
    m_pDevice(pDevice),
    m_partialPipeline(pDevice, true),
    m_task(*pDevice, &m_taskStageInfo, nullptr),
    m_taskStageInfo{},
    m_taskSignature{}
{
    m_taskStageInfo.stageId = Util::Abi::HardwareStage::Cs;
}

// =====================================================================================================================
// Initializes HW-specific state related to this shader library object (register values, user-data mapping, etc.)
// using the specified library ABI processor.
Result GraphicsShaderLibrary::HwlInit(
    const ShaderLibraryCreateInfo&    createInfo,
    const AbiReader&                  abiReader,
    const PalAbi::CodeObjectMetadata& metadata,
    Util::MsgPackReader*              pMetadataReader)
{
    GraphicsPipelineCreateInfo dummyCreateInfo = {};
    GraphicsPipelineInternalCreateInfo internalInfo = {};
    internalInfo.flags.isPartialPipeline = true;

    // Force numColorTarget is non-zero and avoid spiShaderColFormat is overwritten in m_partialPipeline.
    dummyCreateInfo.cbState.target[0].channelWriteMask = 0xf;

    // Force enable depthClipNearEnable and depthClipFarEnable to avoid overwrite paClClipCntl
    dummyCreateInfo.viewportInfo.depthClipNearEnable = true;
    dummyCreateInfo.viewportInfo.depthClipFarEnable = true;
    Result result = m_partialPipeline.Init(dummyCreateInfo, internalInfo, &abiReader, &metadata, pMetadataReader);

    if ((result == Result::Success) && m_partialPipeline.IsTaskShaderEnabled())
    {
        m_task.SetupSignatureFromElf(&m_taskSignature, metadata);

        // We opt to pass the graphics pipeline metadata bit to the task shader signature here instead of in the
        // above task shader (or compute shader) function is due to that, task shader is actually a graphics
        // shader that is used by hybrid graphics pipeline.
        // This bit is placed in taskSignature but not in graphicsSignature since linear dispatch is derived
        // from task shader SC output.
        m_taskSignature.flags.isLinear = metadata.pipeline.graphicsRegister.flags.meshLinearDispatchFromTask;

        const uint32 wavefrontSize = m_taskSignature.flags.isWave32 ? 32 : 64;
        DispatchDims threadsPerTg = {};
        m_task.LateInit(metadata,
                        wavefrontSize,
                        &threadsPerTg,
                        DispatchInterleaveSize::Default,
                        nullptr);

        const  Elf::SymbolTableEntry* pElfSymbol = abiReader.GetSymbolHeader(Abi::PipelineSymbolType::CsDisassembly);
        if (pElfSymbol != nullptr)
        {
            m_taskStageInfo.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
        }

        m_task.InitGpuAddrFromMesh(abiReader, m_partialPipeline.GetChunkGs());
    }

    return result;
}

// =====================================================================================================================
// Obtains the shader pre and post compilation stats/params for the specified shader.
// In this path, values for isWave32, ldsSizePerThreadGroup, numAvailableSgprs,
// numAvailableVgprs, and scratchMemUsageInBytes will not be determined, but it can be extended here
// if any varibale will be used.
Result GraphicsShaderLibrary::GetShaderFunctionStats(
    Util::StringView<char> shaderExportName,
    ShaderLibStats* pShaderStats
) const
{
    Result result = Result::Success;

    PAL_ASSERT(pShaderStats != nullptr);
    memset(pShaderStats, 0, sizeof(ShaderLibStats));
    AbiReader abiReader(m_pDevice->GetPlatform(), m_codeObject);
    result = abiReader.Init();
    if (result == Result::Success)
    {
        MsgPackReader              metadataReader;
        PalAbi::CodeObjectMetadata metadata;
        result = abiReader.GetMetadata(&metadataReader, &metadata);
        if (result == Result::Success)
        {
            result = GetShaderFunctionInfos(shaderExportName, pShaderStats, abiReader, &metadataReader, metadata);
        }
    }
    return result;
}

} // namespace Gfx9
} // namespace Pal
