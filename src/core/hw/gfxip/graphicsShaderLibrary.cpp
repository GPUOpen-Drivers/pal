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

    if (metadata.pipeline.shaderFunctions != 0)
    {
        // Traverses the shader function section to find color export shader
        const char* pColExpSymbol =
                Abi::PipelineAbiSymbolNameStrings[static_cast<uint32>(Abi::PipelineSymbolType::PsColorExportEntry)];
        Result result = pReader->Seek(metadata.pipeline.shaderFunctions);
        if (result == Result::Success)
        {
            const auto& func = pReader->Get().as;
            result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

            for (uint32 i = func.map.size; ((result == Result::Success) && (i > 0)); --i)
            {
                result = pReader->Next(CWP_ITEM_STR);
                if ((result == Result::Success) &&
                    ((strncmp(pColExpSymbol, static_cast<const char*>(func.str.start), func.str.length) == 0)))
                {
                    m_gfxLibInfo.isColorExport = true;
                    break;
                }
            }
        }

        if (m_gfxLibInfo.isColorExport)
        {
            ShaderLibStats shaderStats = {};
            UnpackShaderFunctionStats(pColExpSymbol, metadata, pReader, &shaderStats);
            const char* pColExpDualSourceSymbol = Abi::PipelineAbiSymbolNameStrings[
                static_cast<uint32>(Abi::PipelineSymbolType::PsColorExportDualSourceEntry)];
            ShaderLibStats shaderDualSourceStats = {};
            // If there is no dual source export shader, shader stats should be 0.
            UnpackShaderFunctionStats(pColExpDualSourceSymbol, metadata, pReader, &shaderDualSourceStats);
            m_gfxLibInfo.colorExportProperty.vgprCount =
                static_cast<uint16>(Max(shaderStats.common.numUsedVgprs, shaderDualSourceStats.common.numUsedVgprs));
            m_gfxLibInfo.colorExportProperty.sgprCount =
                static_cast<uint16>(Max(shaderStats.common.numUsedSgprs, shaderDualSourceStats.common.numUsedSgprs));
            m_gfxLibInfo.colorExportProperty.scratchMemorySize = Max(shaderStats.stackFrameSizeInBytes,
                                                                     shaderDualSourceStats.stackFrameSizeInBytes);
        }
    }

    return Result::Success;
}

// =====================================================================================================================
UploadFenceToken GraphicsShaderLibrary::GetUploadFenceToken() const
{
    return GetPartialPipeline()->GetUploadFenceToken();
}

// =====================================================================================================================
uint64 GraphicsShaderLibrary::GetPagingFenceVal() const
{
    return GetPartialPipeline()->GetPagingFenceVal();
}

} // Pal
