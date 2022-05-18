/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace llvm
{
namespace amdhsa
{
    struct kernel_descriptor_t;
}
}

namespace Pal
{

// =====================================================================================================================
// Hardware independent compute pipeline class.  Implements all details of a compute pipeline that are common across
// all hardware types but distinct from a graphics pipeline.
class ComputePipeline : public Pipeline
{
public:
    virtual ~ComputePipeline();

    virtual Result Init(const ComputePipelineCreateInfo& createInfo);

    uint32 ThreadsPerGroup() const { return m_threadsPerTgX * m_threadsPerTgY * m_threadsPerTgZ; }

    void ThreadsPerGroupXyz(uint32* pNumThreadsX, uint32* pNumThreadsY, uint32* pNumThreadsZ) const
    {
        *pNumThreadsX = m_threadsPerTgX;
        *pNumThreadsY = m_threadsPerTgY;
        *pNumThreadsZ = m_threadsPerTgZ;
    }

    virtual const Util::HsaAbi::KernelArgument* GetKernelArgument(uint32 index) const override;

    const Util::HsaAbi::CodeObjectMetadata& HsaMetadata() const;
    const llvm::amdhsa::kernel_descriptor_t& KernelDescriptor() const;

protected:
    ComputePipeline(Device* pDevice, bool isInternal);

    virtual const ShaderStageInfo* GetShaderStageInfo(ShaderType shaderType) const override
        { return (shaderType == ShaderType::Compute) ? &m_stageInfo : nullptr; }

    virtual Result HwlInit(
        const ComputePipelineCreateInfo&        createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader) = 0;

    virtual Result HwlInit(
        const ComputePipelineCreateInfo&        createInfo,
        const AbiReader&                        abiReader,
        const Util::HsaAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader) { return Result::Unsupported; }

    // We need a copy of the HSA metadata for future reference. We also keep a pointer to the HSA metadata's kernel
    // descriptor object. The descriptor is in the pipeline binary, it's not independently allocated!
    Util::HsaAbi::CodeObjectMetadata*        m_pHsaMeta;
    const llvm::amdhsa::kernel_descriptor_t* m_pKernelDescriptor;

    // Number of threads per threadgroup in each dimension as determined by parsing the input IL.
    uint32  m_threadsPerTgX;
    uint32  m_threadsPerTgY;
    uint32  m_threadsPerTgZ;

    bool    m_useCps;                // Use continuation passing shader
    uint32  m_maxFunctionCallDepth;  // Maximum depth for indirect function calls
    uint32  m_stackSizeInBytes;      // Stack managed by the compiler backend.
    uint32  m_irStackSizeInBytes;    // Stack managed by IR (Intermediate Representation),
                                     // transparent to the compiler backend.

    ShaderStageInfo  m_stageInfo;

private:
    Result InitFromPalAbiBinary(
        const ComputePipelineCreateInfo& createInfo,
        const AbiReader&                 abiReader,
        Util::MsgPackReader*             pMetadataReader);

    Result InitFromHsaAbiBinary(
        const ComputePipelineCreateInfo& createInfo,
        const AbiReader&                 abiReader,
        Util::MsgPackReader*             pMetadataReader);

    PAL_DISALLOW_COPY_AND_ASSIGN(ComputePipeline);
};

} // Pal
