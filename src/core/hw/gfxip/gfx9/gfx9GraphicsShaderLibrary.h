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

#pragma once

#include "core/hw/gfxip/graphicsShaderLibrary.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkCs.h"
#include "palMsgPack.h"

namespace Pal
{

namespace Gfx9
{
// =====================================================================================================================
// GFX9 Graphics Shader Library class: implements GFX9 specific functionality for the GraphicsShaderLibrary class.
class GraphicsShaderLibrary final : public Pal::GraphicsShaderLibrary
{
public:
    explicit GraphicsShaderLibrary(Device* pDevice);
    virtual ~GraphicsShaderLibrary() {};
    virtual const Pal::GraphicsPipeline* GetPartialPipeline() const override { return &m_partialPipeline; }
    const PipelineChunkCs& GetTaskChunk() const { return m_task; }
    const ShaderStageInfo& GetTaskStageInfo() const { return m_taskStageInfo; }
    const ComputeShaderSignature GetTaskSignature() const { return m_taskSignature; }
    virtual Result GetShaderFunctionStats(
        Util::StringView<char> shaderExportName,
        ShaderLibStats* pShaderStats) const override;
protected:
    virtual Result HwlInit(
        const ShaderLibraryCreateInfo&          createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader) override;

private:
    Device*const           m_pDevice;
    GraphicsPipeline       m_partialPipeline;
    PipelineChunkCs        m_task;
    ShaderStageInfo        m_taskStageInfo;
    ComputeShaderSignature m_taskSignature;
    // Disable the default constructor and assignment operator
    PAL_DISALLOW_DEFAULT_CTOR(GraphicsShaderLibrary);
    PAL_DISALLOW_COPY_AND_ASSIGN(GraphicsShaderLibrary);
};

} // namespace Gfx9
} // namespace Pal
