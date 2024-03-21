/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkCs.h"

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
// GFX9 hybrid graphics pipeline class: implements common GFX9-specific funcionality for the GraphicsPipeline class and
// adds support for a supplemental task shader that will launch the graphics workload.  Details specific to a particular
// pipeline configuration (GS-enabled, tessellation-enabled, etc) are offloaded to appropriate subclasses.
class HybridGraphicsPipeline final : public GraphicsPipeline
{
public:
    explicit HybridGraphicsPipeline(Device* pDevice);

    virtual Result GetShaderStats(
        ShaderType   shaderType,
        ShaderStats* pShaderStats,
        bool         getDisassemblySize) const override;

    uint32* WriteTaskCommands(
        CmdStream*                      pCmdStream,
        uint32*                         pCmdSpace,
        const DynamicComputeShaderInfo& info,
        bool                            prefetch) const;

    const ComputeShaderSignature& GetTaskSignature() const { return m_taskSignature; }

protected:
    virtual ~HybridGraphicsPipeline() { }

    virtual Result HwlInit(
        const GraphicsPipelineCreateInfo&       createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader) override;

    virtual const ShaderStageInfo* GetShaderStageInfo(ShaderType shaderType) const override;

    Result LinkGraphicsLibraries(
        const GraphicsPipelineCreateInfo& createInfo) override;
private:
    PipelineChunkCs        m_task;
    ShaderStageInfo        m_taskStageInfo;
    ComputeShaderSignature m_taskSignature;

    const bool m_shPairsPacketSupportedCs;

    PAL_DISALLOW_DEFAULT_CTOR(HybridGraphicsPipeline);
    PAL_DISALLOW_COPY_AND_ASSIGN(HybridGraphicsPipeline);
};

} // Gfx9
} // Pal
