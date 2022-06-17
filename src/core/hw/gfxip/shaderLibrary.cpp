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

#include "core/hw/gfxip/shaderLibrary.h"
#include "palMsgPackImpl.h"
#include "palVectorImpl.h"
#include "g_palPipelineAbiMetadataImpl.h"

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
    m_info{},
    m_pCodeObjectBinary(nullptr),
    m_codeObjectBinaryLen(0),
    m_gpuMem(),
    m_gpuMemSize(0),
    m_maxStackSizeInBytes(0),
    m_maxIrStackSizeInBytes(0),
    m_uploadFenceToken(0),
    m_pagingFenceVal(0),
    m_perfDataMem(),
    m_perfDataGpuMemSize(0)
{
}

// =====================================================================================================================
// Initialize this shader library based on the provided creation info.
Result ShaderLibrary::Initialize(
    const ShaderLibraryCreateInfo& createInfo)
{
    Result result = Result::Success;

    if ((createInfo.pCodeObject != nullptr) && (createInfo.codeObjectSize != 0))
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

    AbiReader abiReader(m_pDevice->GetPlatform(), m_pCodeObjectBinary);
    Result result = abiReader.Init();

    MsgPackReader              metadataReader;
    PalAbi::CodeObjectMetadata metadata;

    if (result == Result::Success)
    {
        result = abiReader.GetMetadata(&metadataReader, &metadata);
    }

    if (result == Result::Success)
    {
        ExtractLibraryInfo(metadata);

        result = metadataReader.Seek(metadata.pipeline.shaderFunctions);

        if (result == Result::Success)
        {
            result = ExtractShaderFunctions(&metadataReader);
        }

        result = HwlInit(createInfo,
                abiReader,
                metadata,
                &metadataReader);
    }

    return result;
}

// =====================================================================================================================
// Helper function for extracting the pipeline hash and per-shader hashes from pipeline metadata.
void ShaderLibrary::ExtractLibraryInfo(
    const PalAbi::CodeObjectMetadata& metadata)
{
    m_info.internalLibraryHash =
        { metadata.pipeline.internalPipelineHash[0], metadata.pipeline.internalPipelineHash[1] };

    // We don't expect the pipeline ABI to report a hash of zero.
    PAL_ALERT((metadata.pipeline.internalPipelineHash[0] | metadata.pipeline.internalPipelineHash[1]) == 0);
}

// =====================================================================================================================
// Helper function for extracting shader function metadata.
Result ShaderLibrary::ExtractShaderFunctions(
    Util::MsgPackReader* pReader)
{
    Result result    = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;
    const auto& item = pReader->Get().as;

    for (uint32 i = item.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        ShaderFuncStats stats = {};

        result = pReader->Next(CWP_ITEM_STR);
        if (result == Result::Success)
        {
            stats.symbolNameLength = item.str.length;
            stats.pSymbolName      = static_cast<const char*>(item.str.start);
        }

        if (result == Result::Success)
        {
            result = pReader->Next(CWP_ITEM_MAP);
        }

        if (result == Result::Success)
        {
            for (uint32 j = item.map.size; ((result == Result::Success) && (j > 0)); --j)
            {
                result = pReader->Next(CWP_ITEM_STR);

                if (result == Result::Success)
                {
                    switch (HashString(static_cast<const char*>(item.str.start), item.str.length))
                    {
                    case HashLiteralString(".stack_frame_size_in_bytes"):
                    {
                        result = pReader->UnpackNext(&stats.stackFrameSizeInBytes);
                        m_maxStackSizeInBytes = Max(m_maxStackSizeInBytes, stats.stackFrameSizeInBytes);
                        break;
                    }
                    case HashLiteralString(".ir_stack_frame_size_in_bytes"):
                    {
                        result = pReader->UnpackNext(&stats.irStackFrameSizeInBytes);
                        m_maxIrStackSizeInBytes = Max(m_maxIrStackSizeInBytes, stats.irStackFrameSizeInBytes);
                        break;
                    }
                    case HashLiteralString(".shader_subtype"):
                    {
                        Util::Abi::ApiShaderSubType shaderSubType;
                        Util::PalAbi::Metadata::DeserializeEnum(pReader, &shaderSubType);
                        stats.shaderSubType = static_cast<Pal::ShaderSubType>(shaderSubType);
                        break;
                    }

                    default:
                        result = pReader->Skip(1);
                       break;
                    }
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Allocates GPU memory for this library and uploads the code and data contain in the ELF binary to it.
// Any ELF relocations are also applied to the memory during this operation.
Result ShaderLibrary::PerformRelocationsAndUploadToGpuMemory(
    const PalAbi::CodeObjectMetadata& metadata,
    const GpuHeap&                    clientPreferredHeap,
    PipelineUploader*                 pUploader)
{
    PAL_ASSERT(pUploader != nullptr);

    // Compute the total size of all shader stages' performance data buffers.
    gpusize performanceDataOffset = 0;
    m_perfDataGpuMemSize = performanceDataOffset;
    Result result        = Result::Success;

    result = pUploader->Begin(clientPreferredHeap);

    if (result == Result::Success)
    {
        m_pagingFenceVal = pUploader->PagingFenceVal();
        m_gpuMemSize     = pUploader->GpuMemSize();
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
// Obtains the compiled shader ISA code for the shader specified.
Result ShaderLibrary::GetShaderFunctionCode(
    const char*  pShaderExportName,
    size_t*      pSize,
    void*        pBuffer) const
{
    // This function should be implemented in gfx6 / gfx9 if needed.
    return Result::ErrorUnavailable;;
}

// =====================================================================================================================
// Obtains the shader pre and post compilation stats/params for the specified shader.
Result ShaderLibrary::GetShaderFunctionStats(
    const char*      pShaderExportName,
    ShaderLibStats*  pStats) const
{
    // This function should be implemented in gfx6 / gfx9 if needed.
    return Result::ErrorUnavailable;;
}

// =====================================================================================================================
// Computes the GPU virtual address of each of the indirect functions specified by the client.
void ShaderLibrary::GetFunctionGpuVirtAddrs(
    const PipelineUploader&         uploader,
    ShaderLibraryFunctionInfo*      pFuncInfoList,
    uint32                          funcCount)
{
    for (uint32 i = 0; i < funcCount; ++i)
    {
        GpuSymbol symbol = { };
        if (uploader.GetGenericGpuSymbol(pFuncInfoList[i].pSymbolName, &symbol) == Result::Success)
        {
            pFuncInfoList[i].gpuVirtAddr = symbol.gpuVirtAddr;
            PAL_ASSERT(pFuncInfoList[i].gpuVirtAddr != 0);
        }
        else
        {
            PAL_ASSERT_ALWAYS();
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 731
            pGpuMemList[0].pGpuMemory  = m_gpuMem.Memory();
#endif
            pGpuMemList[0].address     = m_gpuMem.Memory()->Desc().gpuVirtAddr;
            pGpuMemList[0].offset      = m_gpuMem.Offset();
            pGpuMemList[0].size        = m_gpuMemSize;
        }

        result = Result::Success;
    }

    return result;
}

} // Pal
