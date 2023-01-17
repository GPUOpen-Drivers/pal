/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/decorators.h"
#include "palPipelineAbi.h"
#include "g_palPipelineAbiMetadataImpl.h"

namespace Util { class File;  }

namespace Pal
{
namespace GpuProfiler
{

class Device;
class Platform;

// =====================================================================================================================
class Pipeline final : public PipelineDecorator
{
public:
    Pipeline(IPipeline* pNextPipeline, const Device* pDevice);

    Result InitGfx(const GraphicsPipelineCreateInfo& createInfo);
    Result InitCompute(const ComputePipelineCreateInfo& createInfo);

    // Public IDestroyable interface methods:
    virtual void Destroy() override;

private:
    virtual ~Pipeline() { }

    struct ShaderDumpInfo
    {
        Util::Abi::ApiShaderType type;
        Util::Abi::HardwareStage hwStage;
        ShaderHash               hash;
        uint64                   compilerHash;
        Util::File*              pFile;
    };

    bool OpenUniqueDumpFile(
        const ShaderDumpInfo& dumpInfo) const;

    size_t DumpShaderPerfData(
        const ShaderDumpInfo& dumpInfo,
        void*                 pPerfData,
        size_t                perfDataSize) const;

    const Device*                    m_pDevice;
    Platform*                        m_pPlatform;
    bool                             m_hasPerformanceData;
    Util::Abi::ApiHwShaderMapping    m_apiHwMapping;

    PAL_DISALLOW_DEFAULT_CTOR(Pipeline);
    PAL_DISALLOW_COPY_AND_ASSIGN(Pipeline);
};

} // GpuProfiler
} // Pal
