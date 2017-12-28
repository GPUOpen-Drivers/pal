/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/pipeline.h"

namespace Pal
{

class ShaderCacheClientData;

// =====================================================================================================================
// Hardware independent compute pipeline class.  Implements all details of a compute pipeline that are common across
// all hardware types but distinct from a graphics pipeline.
class ComputePipeline : public Pipeline
{
public:
    virtual ~ComputePipeline() { }

    virtual Result Init(const ComputePipelineCreateInfo& createInfo);
    virtual Result LoadInit(const Util::ElfReadContext<Platform>& context) override;

    uint32 ThreadsPerGroup() const { return m_threadsPerTgX * m_threadsPerTgY * m_threadsPerTgZ; }

    void ThreadsPerGroupXyz(uint32* pNumThreadsX, uint32* pNumThreadsY, uint32* pNumThreadsZ) const
    {
        *pNumThreadsX = m_threadsPerTgX;
        *pNumThreadsY = m_threadsPerTgY;
        *pNumThreadsZ = m_threadsPerTgZ;
    }

protected:
    ComputePipeline(Device* pDevice, bool isInternal);

    virtual const ShaderStageInfo* GetShaderStageInfo(ShaderType shaderType) const override
        { return (shaderType == ShaderType::Compute) ? &m_stageInfo : nullptr; }

    virtual Result Serialize(Util::ElfWriteContext<Platform>* pContext) override;
    virtual Result HwlInit(const AbiProcessor& abiProcessor) = 0;

    // Number of threads per threadgroup in each dimension as determined by parsing the input IL.
    uint32  m_threadsPerTgX;
    uint32  m_threadsPerTgY;
    uint32  m_threadsPerTgZ;

    ShaderStageInfo  m_stageInfo;

private:
    Result InitFromPipelineBinary();

    PAL_DISALLOW_COPY_AND_ASSIGN(ComputePipeline);
};

} // Pal
