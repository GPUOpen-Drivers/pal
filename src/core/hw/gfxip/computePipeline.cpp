/*
 *******************************************************************************
 *
 * Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "core/hw/gfxip/computePipeline.h"
#include "core/hw/gfxip/palToScpcWrapper.h"
#include "palElfPackagerImpl.h"
#include "palMetroHash.h"
#include "palPipelineAbiProcessorImpl.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
ComputePipeline::ComputePipeline(
    Device* pDevice,
    bool    isInternal)  // True if this is a PAL-owned pipeline (i.e., an RPM pipeline).
    :
    Pipeline(pDevice, isInternal),
    m_threadsPerTgX(0),
    m_threadsPerTgY(0),
    m_threadsPerTgZ(0)
{
    memset(&m_stageInfo, 0, sizeof(m_stageInfo));
    m_stageInfo.stageId = Abi::HardwareStage::Cs;
}

// =====================================================================================================================
// Initialize this compute pipeline based on the provided creation info.
Result ComputePipeline::Init(
    const ComputePipelineCreateInfo& createInfo)
{
    Result result = Result::Success;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 305
    if ((createInfo.pPipelineBinary != nullptr) && (createInfo.pipelineBinarySize != 0))
    {
        m_pipelineBinaryLen = createInfo.pipelineBinarySize;
        m_pPipelineBinary   = PAL_MALLOC(m_pipelineBinaryLen, m_pDevice->GetPlatform(), AllocInternal);
        if (m_pPipelineBinary == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            memcpy(m_pPipelineBinary, createInfo.pPipelineBinary, m_pipelineBinaryLen);
        }
    }
    else
#endif
    {
        PAL_ASSERT_ALWAYS();
        result = Result::ErrorInvalidPointer;
    }

    if (result == Result::Success)
    {
        PAL_ASSERT(m_pPipelineBinary != nullptr);
        result = InitFromPipelineBinary();
    }

    return result;
}

// =====================================================================================================================
// Initializes this pipeline from the pipeline binary data stored in this object.
Result ComputePipeline::InitFromPipelineBinary()
{
    PAL_ASSERT((m_pPipelineBinary != nullptr) && (m_pipelineBinaryLen != 0));

    AbiProcessor abiProcessor(m_pDevice->GetPlatform());
    Result result = abiProcessor.LoadFromBuffer(m_pPipelineBinary, m_pipelineBinaryLen);

    if (result == Result::Success)
    {
        ExtractPipelineInfo(abiProcessor, ShaderType::Compute, ShaderType::Compute);

        DumpPipelineElf(abiProcessor, "PipelineCs");

        Abi::PipelineSymbolEntry symbol = { };
        if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::CsDisassembly, &symbol))
        {
            m_stageInfo.disassemblyLength = static_cast<size_t>(symbol.size);
        }

        result = HwlInit(abiProcessor);
    }

    return result;
}

// =====================================================================================================================
// Initializes this pipeline based on the contents of an ELF contents created with a previous call to Pipeline::Store.
Result ComputePipeline::LoadInit(
    const ElfReadContext<Platform>& context)
{
    Result result = Pipeline::LoadInit(context);
    if (result == Result::Success)
    {
        // Verify the correct pipeline type.
        const PipelineType* pType = nullptr;
        size_t              size  = 0;
        result = GetLoadedSectionData(context, ".pipelineType", reinterpret_cast<const void**>(&pType), &size);
        if (*pType != PipelineTypeCompute)
        {
            result = Result::ErrorInvalidPipelineElf;
        }
    }

    if (result == Result::Success)
    {
        // NOTE: We cannot break the legacy pipeline serialization path yet because some clients still rely on it.
        // Instead, Serialize() just puts the pipeline binary blob into the ELF.

        const void* pPipelineBinary   = nullptr;
        size_t      pipelineBinaryLen = 0;
        result = GetLoadedSectionData(context, ".pipelineBinary", &pPipelineBinary, &pipelineBinaryLen);
        if (result == Result::Success)
        {
            m_pipelineBinaryLen = pipelineBinaryLen;
            m_pPipelineBinary   = PAL_MALLOC(m_pipelineBinaryLen, m_pDevice->GetPlatform(), AllocInternal);
            if (m_pPipelineBinary == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                memcpy(m_pPipelineBinary, pPipelineBinary, m_pipelineBinaryLen);
            }
        }
    }

    if (result == Result::Success)
    {
        result = InitFromPipelineBinary();
    }

    return result;
}

// =====================================================================================================================
// Write a section to the provided ELF context that can be later used to restore this pipeline from disk.
Result ComputePipeline::Serialize(
    ElfWriteContext<Platform>* pContext)
{
    // NOTE: We cannot break the legacy pipeline serialization path yet because graphics pipelines still rely on it.
    // Instead, Serialize() just puts the pipeline binary blob into the ELF.

    constexpr PipelineType Type = PipelineTypeCompute;
    Result result = pContext->AddBinarySection(".pipelineType", &Type, sizeof(Type));
    if (result == Result::Success)
    {
        result = pContext->AddBinarySection(".pipelineBinary", m_pPipelineBinary, m_pipelineBinaryLen);
    }

    return result;
}

} // Pal
