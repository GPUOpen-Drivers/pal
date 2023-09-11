/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/graphicsShaderLibrary.h"
#include "core/hw/gfxip/graphicsPipeline.h"

using namespace Util;

namespace Pal
{
// =====================================================================================================================
Result GraphicsShaderLibrary::QueryAllocationInfo(
    size_t* pNumEntries,
    GpuMemSubAllocInfo* const  pAllocInfoList) const
{
    return GetPartialPipeline()->QueryAllocationInfo(pNumEntries, pAllocInfoList);
}

// =====================================================================================================================
// Helper function for common init operations after HwlInit
Result GraphicsShaderLibrary::PostInit(
    const PalAbi::CodeObjectMetadata& metadata,
    Util::MsgPackReader*              pReader)
{
    const Pal::GraphicsPipeline*  pPartialPipeline = GetPartialPipeline();
    const PipelineInfo&           info             = pPartialPipeline->GetInfo();
    Util::Abi::ApiHwShaderMapping hwMapping        = pPartialPipeline->ApiHwShaderMapping();

    for (uint32 stage = 0; stage < static_cast<uint32>(ShaderType::Count); stage++)
    {
        if ((info.shader[stage].hash.upper != 0) || (info.shader[stage].hash.lower != 0))
        {
            const uint32 shaderTypeIdx = static_cast<uint32>(
                PalShaderTypeToAbiShaderType(static_cast<ShaderType>(stage)));

            m_gfxLibInfo.apiShaderMask |= (1 << stage);
            m_gfxLibInfo.hwShaderMask |= hwMapping.apiShaders[shaderTypeIdx];
        }
    }

    m_gfxLibInfo.isColorExport =
        (metadata.pipeline.hardwareStage[static_cast<uint32_t>(Abi::HardwareStage::Ps)].entryPoint ==
             Abi::PipelineSymbolType::PsColorExportEntry);

    return Result::Success;
}
} // Pal
