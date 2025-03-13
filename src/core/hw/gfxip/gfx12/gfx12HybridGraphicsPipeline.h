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

#pragma once

#include "core/hw/gfxip/gfx12/gfx12ComputePipeline.h"
#include "core/hw/gfxip/gfx12/gfx12GraphicsPipeline.h"
#include "core/hw/gfxip/gfx12/gfx12PipelineChunkCs.h"

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
// GFX12 hybrid graphics pipeline class: implements common GFX12-specific funcionality for the GraphicsPipeline class
// and adds support for a supplemental task shader that will launch the graphics workload.
class HybridGraphicsPipeline final : public GraphicsPipeline
{
public:
    explicit HybridGraphicsPipeline(
        Device* pDevice,
        bool    isInternal);
    virtual ~HybridGraphicsPipeline() { }

    virtual Result GetShaderStats(
        ShaderType   shaderType,
        ShaderStats* pShaderStats,
        bool         getDisassemblySize) const override;

    uint32* WriteTaskCommands(
        const DynamicComputeShaderInfo& dynamicInfo,
        uint32*                         pCmdSpace,
        CmdStream*                      pCmdStream) const
    {
        return m_taskShader.WriteCommands(
            nullptr, dynamicInfo, m_pDevice->Settings().pipelinePrefetchEnable, pCmdSpace, pCmdStream);
    }

    const ComputeUserDataLayout* TaskUserDataLayout() const { return m_taskShader.UserDataLayout(); }

    bool IsTaskWave32() const { return m_taskShader.IsWave32(); }
    bool IsLinearDispatch() const { return m_meshLinearDispatch; }

    size_t GetDvgprExtraAceScratch() const { return m_taskShader.GetDvgprExtraAceScratch(); }

protected:
    virtual Result InitDerivedState(
        const GraphicsPipelineCreateInfo&       createInfo,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        const CodeObjectUploader&               uploader,
        const AbiReader&                        abiReader) override;

    virtual const ShaderStageInfo* GetShaderStageInfo(ShaderType shaderType) const override;

    virtual Result LinkGraphicsLibraries(const GraphicsPipelineCreateInfo& createInfo) override;

private:
    PipelineChunkCs m_taskShader;
    ShaderStageInfo m_taskStageInfo;
    bool            m_meshLinearDispatch;

    PAL_DISALLOW_DEFAULT_CTOR(HybridGraphicsPipeline);
    PAL_DISALLOW_COPY_AND_ASSIGN(HybridGraphicsPipeline);
};

} // namespace Gfx12
} // namespace Pal
