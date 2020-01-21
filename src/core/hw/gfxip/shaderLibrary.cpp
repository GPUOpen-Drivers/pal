/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/shaderLibrary.h"
#include "palPipelineAbiProcessorImpl.h"

using namespace Util;

namespace Pal
{

// GPU memory alignment for shader programs.
constexpr size_t GpuMemByteAlign = 256;

// =====================================================================================================================
ShaderLibrary::ShaderLibrary(
    Device* pDevice)
    :
    Pal::IShaderLibrary(),
    m_pDevice(pDevice),
    m_pCodeObjectBinary(nullptr),
    m_codeObjectBinaryLen(0),
    m_gpuMem(),
    m_gpuMemSize(0),
    m_pClientData(nullptr),
    m_perfDataMem(),
    m_perfDataGpuMemSize(0)
{
    memset(&m_info, 0, sizeof(m_info));
}

// =====================================================================================================================
// Initialize this shader library based on the provided creation info.
Result ShaderLibrary::Initialize(
    const ShaderLibraryCreateInfo& createInfo)
{
    Result result = Result::Success;

    if((createInfo.pCodeObject != nullptr) && (createInfo.codeObjectSize != 0))
    {
        m_codeObjectBinaryLen = createInfo.codeObjectSize;
        m_pCodeObjectBinary   = PAL_MALLOC(m_codeObjectBinaryLen, m_pDevice->GetPlatform(), AllocInternal);

        if (m_pCodeObjectBinary == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            memcpy(m_pCodeObjectBinary, createInfo.pCodeObject, m_codeObjectBinaryLen);
        }
    }
    else
    {
        result = Result::ErrorInvalidPointer;
    }

    if (result == Result::Success)
    {
        PAL_ASSERT(m_pCodeObjectBinary != nullptr);
        result = InitFromCodeObjectBinary(createInfo);
    }

    return result;
}

// =====================================================================================================================
// Initializes this library from the library binary data stored in this object.
Result ShaderLibrary::InitFromCodeObjectBinary(
    const ShaderLibraryCreateInfo& createInfo)
{
    PAL_ASSERT((m_pCodeObjectBinary != nullptr) && (m_codeObjectBinaryLen != 0));

    AbiProcessor abiProcessor(m_pDevice->GetPlatform());
    Result result = abiProcessor.LoadFromBuffer(m_pCodeObjectBinary, m_codeObjectBinaryLen);

    MsgPackReader      metadataReader;
    CodeObjectMetadata metadata;

    if (result == Result::Success)
    {
        result = abiProcessor.GetMetadata(&metadataReader, &metadata);
    }

    if (result == Result::Success)
    {
        ExtractLibraryInfo(metadata);

        result = HwlInit(createInfo,
                         abiProcessor,
                         metadata,
                         &metadataReader);
    }

    return result;
}

// =====================================================================================================================
// Helper function for extracting the pipeline hash and per-shader hashes from pipeline metadata.
void ShaderLibrary::ExtractLibraryInfo(
    const CodeObjectMetadata& metadata)
{
    m_info.internalLibraryHash =
        { metadata.pipeline.internalPipelineHash[0], metadata.pipeline.internalPipelineHash[1] };

    // We don't expect the pipeline ABI to report a hash of zero.
    PAL_ALERT((metadata.pipeline.internalPipelineHash[0] | metadata.pipeline.internalPipelineHash[1]) == 0);

}

// =====================================================================================================================
// Allocates GPU memory for this library and uploads the code and data contain in the ELF binary to it.
// Any ELF relocations are also applied to the memory during this operation.
Result ShaderLibrary::PerformRelocationsAndUploadToGpuMemory(
    const AbiProcessor&       abiProcessor,
    const CodeObjectMetadata& metadata,
    const GpuHeap&            clientPreferredHeap,
    PipelineUploader*         pUploader)
{
    PAL_ASSERT(pUploader != nullptr);

    // Compute the total size of all shader stages' performance data buffers.
    gpusize performanceDataOffset = 0;
    m_perfDataGpuMemSize = performanceDataOffset;
    Result result        = Result::Success;

    result = pUploader->Begin(abiProcessor, metadata, clientPreferredHeap);

    if (result == Result::Success)
    {
        m_gpuMemSize = pUploader->GpuMemSize();
        m_gpuMem.Update(pUploader->GpuMem(), pUploader->GpuMemOffset());
    }

    return result;
}

// =====================================================================================================================
// Extracts the shader library's code object ELF binary.
Result ShaderLibrary::GetCodeObject(
    uint32*    pSize,
    void*      pBuffer
    ) const
{
    Result result = Result::ErrorInvalidPointer;

    if (pSize != nullptr)
    {
        if ((m_pCodeObjectBinary != nullptr) && (m_codeObjectBinaryLen != 0))
        {
            if (pBuffer == nullptr)
            {
                (*pSize) = static_cast<uint32>(m_codeObjectBinaryLen);
                result = Result::Success;
            }
            else if ((*pSize) >= static_cast<uint32>(m_codeObjectBinaryLen))
            {
                memcpy(pBuffer, m_pCodeObjectBinary, m_codeObjectBinaryLen);
                result = Result::Success;
            }
            else
            {
                result = Result::ErrorInvalidMemorySize;
            }
        }
        else
        {
            result = Result::ErrorUnavailable;
        }
    }

    return result;
}

// =====================================================================================================================
// Computes the GPU virtual address of each of the indirect functions specified by the client.
void ShaderLibrary::GetFunctionGpuVirtAddrs(
    const AbiProcessor&             abiProcessor,
    const PipelineUploader&         uploader,
    ShaderLibraryFunctionInfo*      pFuncInfoList,
    uint32                          funcCount)
{
    const gpusize codeGpuVirtAddr = uploader.CodeGpuVirtAddr();
    for (uint32 i = 0; i < funcCount; ++i)
    {
        Abi::GenericSymbolEntry symbol = { };
        if (abiProcessor.HasGenericSymbolEntry(pFuncInfoList[i].pSymbolName, &symbol))
        {
            pFuncInfoList[i].gpuVirtAddr = (codeGpuVirtAddr + symbol.value);
        }
    }
}

// =====================================================================================================================
// Query this shader library's Bound GPU Memory.
Result ShaderLibrary::QueryAllocationInfo(
    size_t*                   pNumEntries,
    GpuMemSubAllocInfo* const pGpuMemList
    ) const
{
    Result result = Result::ErrorInvalidPointer;

    if (pNumEntries != nullptr)
    {
        (*pNumEntries) = 1;

        if (pGpuMemList != nullptr)
        {
            pGpuMemList[0].offset     = m_gpuMem.Offset();
            pGpuMemList[0].pGpuMemory = m_gpuMem.Memory();
            pGpuMemList[0].size       = m_gpuMemSize;
        }

        result = Result::Success;
    }

    return result;
}

}
