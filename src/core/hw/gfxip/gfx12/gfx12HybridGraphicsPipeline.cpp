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

#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12GraphicsShaderLibrary.h"
#include "core/hw/gfxip/gfx12/gfx12HybridGraphicsPipeline.h"

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
HybridGraphicsPipeline::HybridGraphicsPipeline(
    Device* pDevice,
    bool    isInternal)
    :
    GraphicsPipeline(pDevice, isInternal),
    m_taskShader(pDevice),
    m_taskStageInfo{ Abi::HardwareStage::Cs, 0, 0 },
    m_meshLinearDispatch(false)
{
}

// =====================================================================================================================
Result HybridGraphicsPipeline::InitDerivedState(
    const GraphicsPipelineCreateInfo& createInfo,
    const PalAbi::CodeObjectMetadata& metadata,
    const CodeObjectUploader&         uploader,
    const AbiReader&                  abiReader)
{
    // Task shader is launched by ACE but ACE doesn't support dispatch interleave on Gfx12.
    Result result = m_taskShader.HwlInit(uploader,
                                         metadata,
                                         DispatchInterleaveSize::Disable,
                                         createInfo.groupLaunchGuarantee != TriState::Disable);

    if (result == Result::Success)
    {
        GpuSymbol symbol = {};
        if (uploader.GetGpuSymbol(Abi::PipelineSymbolType::CsMainEntry, &symbol) == Result::Success)
        {
            m_taskStageInfo.codeLength = static_cast<size_t>(symbol.size);
            PAL_ASSERT(IsPow2Aligned(symbol.gpuVirtAddr, 256u));
        }

        const auto* pElfSymbol = abiReader.GetSymbolHeader(Abi::PipelineSymbolType::CsDisassembly);
        if (pElfSymbol != nullptr)
        {
            m_taskStageInfo.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
        }

        // Fixes were made in ME version 2540 for this feature.
        constexpr uint32 MeLinearDispatchVersion = 2540;
        if (m_pDevice->ChipProperties().meUcodeVersion >= MeLinearDispatchVersion)
        {
            // We opt to pass the graphics pipeline metadata bit to the task shader signature here instead of in the
            // above task shader (or compute shader) function is due to that, task shader is actually a graphics
            // shader that is used by hybrid graphics pipeline.
            m_meshLinearDispatch = (metadata.pipeline.graphicsRegister.flags.meshLinearDispatchFromTask != 0);
        }
    }

    return result;
}

// =====================================================================================================================
Result HybridGraphicsPipeline::GetShaderStats(
    ShaderType   shaderType,
    ShaderStats* pShaderStats,
    bool         getDisassemblySize
    ) const
{
    Result result = GraphicsPipeline::GetShaderStats(shaderType, pShaderStats, getDisassemblySize);

    if (shaderType == ShaderType::Task)
    {
        pShaderStats->common.gpuVirtAddress = GetOriginalAddress(
            m_taskShader.GetHwReg<mmCOMPUTE_PGM_LO, COMPUTE_PGM_LO>().bits.DATA, 0);
        pShaderStats->shaderStageMask       = ApiShaderStageTask;
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
// Link graphics pipeline from graphics shader libraries.
Result HybridGraphicsPipeline::LinkGraphicsLibraries(
    const GraphicsPipelineCreateInfo& createInfo)
{
    Result result = GraphicsPipeline::LinkGraphicsLibraries(createInfo);

    if (result == Result::Success)
    {
        const HybridGraphicsPipeline* pTaskLib = nullptr;
        for (uint32 i = 0; i < NumGfxShaderLibraries(); i++)
        {
            const GraphicsShaderLibrary* pLib =
                reinterpret_cast<const GraphicsShaderLibrary*>(GetGraphicsShaderLibrary(i));
            uint32 apiShaderMask = pLib->GetApiShaderMask();
            if (Util::TestAnyFlagSet(apiShaderMask, ApiShaderStageTask))
            {
                pTaskLib = static_cast<const HybridGraphicsPipeline*>(pLib->GetPartialPipeline());
                break;
            }
        }

        PAL_ASSERT((pTaskLib != nullptr) && pTaskLib->HasTaskShader());
        m_taskShader.Clone(pTaskLib->m_taskShader);
        m_taskStageInfo      = pTaskLib->m_taskStageInfo;
        m_meshLinearDispatch = pTaskLib->m_meshLinearDispatch;
    }
    return result;
}

} // namespace Gfx12
} // namespace Pal
