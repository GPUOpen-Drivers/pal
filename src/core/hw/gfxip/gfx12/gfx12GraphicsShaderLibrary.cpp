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

#include "core/hw/gfxip/gfx12/gfx12GraphicsShaderLibrary.h"
#include "core/hw/gfxip/gfx12/gfx12HybridGraphicsPipeline.h"

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
// Helper function used to check whether task shader exists in code object.
static bool ContainsTaskShader(
    const PalAbi::CodeObjectMetadata& metadata)
{
    bool exist = false;

    const PalAbi::ShaderMetadata& taskMetadata =
        metadata.pipeline.shader[static_cast<uint32>(Abi::ApiShaderType::Task)];

    if (taskMetadata.hasEntry.apiShaderHash)
    {
        const ShaderHash taskHash = { taskMetadata.apiShaderHash[0], taskMetadata.apiShaderHash[1] };
        exist = ShaderHashIsNonzero(taskHash);
    }

    return exist;
}

// =====================================================================================================================
GraphicsShaderLibrary::GraphicsShaderLibrary(
    Device* pDevice)
    :
    Pal::GraphicsShaderLibrary(pDevice->Parent()),
    m_pDevice(pDevice),
    m_pPartialPipeline(nullptr)
{
}

// =====================================================================================================================
GraphicsShaderLibrary::~GraphicsShaderLibrary()
{
    if (m_pPartialPipeline != nullptr)
    {
        m_pPartialPipeline->~GraphicsPipeline();
    }
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
    PAL_ASSERT(m_pPartialPipeline == nullptr);

    // Partial pipeline should closely follow GraphicsShaderLibrary in memory.
    void* pPartialPipelineMem = this + 1;

    if (ContainsTaskShader(metadata))
    {
        m_pPartialPipeline = PAL_PLACEMENT_NEW(pPartialPipelineMem) HybridGraphicsPipeline(m_pDevice, true);
    }
    else
    {
        m_pPartialPipeline = PAL_PLACEMENT_NEW(pPartialPipelineMem) GraphicsPipeline(m_pDevice, true);
    }

    GraphicsPipelineInternalCreateInfo internalInfo = {};
    internalInfo.flags.isPartialPipeline = true;

    GraphicsPipelineCreateInfo dummyCreateInfo = {};
    // Force numColorTarget is non-zero and avoid spiShaderColFormat is overwritten in m_partialPipeline.
    dummyCreateInfo.cbState.target[0].channelWriteMask = 0xf;
    // Force enable depthClipNearEnable and depthClipFarEnable to avoid overwrite paClClipCntl
    dummyCreateInfo.viewportInfo.depthClipNearEnable   = true;
    dummyCreateInfo.viewportInfo.depthClipFarEnable    = true;

    return m_pPartialPipeline->Init(dummyCreateInfo, internalInfo, &abiReader, &metadata, pMetadataReader);
}

} // namespace Gfx12
} // namespace Pal
