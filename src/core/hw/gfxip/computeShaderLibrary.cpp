/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/computeShaderLibrary.h"
#include "palMsgPackImpl.h"
#include "palVectorImpl.h"
#include "g_palPipelineAbiMetadataImpl.h"

using namespace Util;

namespace Pal
{

// GPU memory alignment for shader programs.
constexpr size_t GpuMemByteAlign = 256;

// =====================================================================================================================
ComputeShaderLibrary::ComputeShaderLibrary(
    Device* pDevice)
    :
    ShaderLibrary(pDevice),
    m_gpuMem(),
    m_gpuMemSize(0),
    m_maxStackSizeInBytes(0),
    m_uploadFenceToken(0),
    m_pagingFenceVal(0),
    m_perfDataMem(),
    m_perfDataGpuMemSize(0),
    m_functionList(pDevice->GetPlatform())
{
}

// =====================================================================================================================
ComputeShaderLibrary::~ComputeShaderLibrary()
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 827
    for (auto info : m_functionList)
    {
        PAL_FREE(info.pSymbolName, m_pDevice->GetPlatform());
    }
#endif
}

// =====================================================================================================================
// Helper function for common init operations after HwlInit
Result ComputeShaderLibrary::PostInit(
    const PalAbi::CodeObjectMetadata& metadata,
    Util::MsgPackReader*              pReader)
{
    Result result = pReader->Seek(metadata.pipeline.shaderFunctions);

    if (result == Result::Success)
    {
        result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;
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
    }
    return result;
}

// =====================================================================================================================
// Allocates GPU memory for this library and uploads the code and data contain in the ELF binary to it.
// Any ELF relocations are also applied to the memory during this operation.
Result ComputeShaderLibrary::PerformRelocationsAndUploadToGpuMemory(
    const PalAbi::CodeObjectMetadata& metadata,
    const GpuHeap&                    clientPreferredHeap,
    PipelineUploader*                 pUploader)
{
    PAL_ASSERT(pUploader != nullptr);

    // Compute the total size of all shader stages' performance data buffers.
    gpusize performanceDataOffset = 0;
    m_perfDataGpuMemSize = performanceDataOffset;
    Result result        = Result::Success;

    result = pUploader->Begin(clientPreferredHeap, IsInternal());

    if (result == Result::Success)
    {
        result = pUploader->ApplyRelocations();
    }

    if (result == Result::Success)
    {
        m_pagingFenceVal = pUploader->PagingFenceVal();
        m_gpuMemSize     = pUploader->GpuMemSize();
        m_gpuMem.Update(pUploader->GpuMem(), pUploader->GpuMemOffset());
    }

    return result;
}

// =====================================================================================================================
// Initializes m_functionList from metadata
Result ComputeShaderLibrary::InitFunctionListFromMetadata(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    Util::MsgPackReader*                    pReader)
{
    Result result = pReader->Seek(metadata.pipeline.shaderFunctions);

    if (result == Result::Success)
    {
        result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;
        const auto& item = pReader->Get().as;

        uint32 funcCount = item.map.size;

        for (uint32 i = 0; ((result == Result::Success) && (i < funcCount)); ++i)
        {
            result = pReader->Next(CWP_ITEM_STR);

            if (result == Result::Success)
            {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 827
                ShaderLibraryFunctionInfo info = { nullptr, 0 };
#else
                StringView<char> symbolName(static_cast<const char*>(item.str.start), item.str.length);
                ShaderLibraryFunctionInfo info = { symbolName, 0 };
#endif
                result = m_functionList.PushBack(info);
            }

            if (result == Result::Success)
            {
                // Skip metadata for this function (we only need its name here).
                // E.g., function1 : {...}(skip), function2 : {...}(skip)
                result = pReader->Skip(1);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Computes the GPU virtual address of each of the indirect functions specified by the client.
void ComputeShaderLibrary::GetFunctionGpuVirtAddrs(
    const PipelineUploader&         uploader,
    ShaderLibraryFunctionInfo*      pFuncInfoList,
    uint32                          funcCount)
{
    for (uint32 i = 0; i < funcCount; ++i)
    {
        GpuSymbol symbol = { };
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 827
        if (uploader.GetGenericGpuSymbol(pFuncInfoList[i].pSymbolName, &symbol) == Result::Success)
#else
        if (uploader.GetGenericGpuSymbol(pFuncInfoList[i].symbolName, &symbol) == Result::Success)
#endif
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
Result ComputeShaderLibrary::QueryAllocationInfo(
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
            pGpuMemList[0].address     = m_gpuMem.Memory()->Desc().gpuVirtAddr;
            pGpuMemList[0].offset      = m_gpuMem.Offset();
            pGpuMemList[0].size        = m_gpuMemSize;
        }

        result = Result::Success;
    }

    return result;
}
} // Pal
