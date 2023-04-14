/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palDevice.h"
#include "palPipeline.h"
#include "palSysMemory.h"

#include "g_msaaImageCopyComputePipelineInit.h"
#include "g_msaaImageCopyComputePipelineBinaries.h"

namespace GpuUtil
{
namespace MsaaImageCopy
{

// =====================================================================================================================
// Creates all compute pipeline objects required by MsaaImageCopyUtil.
template <typename Allocator>
Pal::Result CreateMsaaImageCopyComputePipeline(
    Pal::IDevice*                pDevice,
    Allocator*                   pAllocator,
    Pal::IPipeline**             pPipelineMem,
    const PipelineBinary*        pTable,
    MsaaImageCopyComputePipeline pipelineType)
{
    Pal::Result result = Pal::Result::Success;

    const PipelineBinary& pipeline = pTable[static_cast<size_t>(pipelineType)];

    if (pipeline.pBuffer != nullptr)
    {
        Pal::ComputePipelineCreateInfo pipeInfo = { };

        pipeInfo.pPipelineBinary      = pipeline.pBuffer;
        pipeInfo.pipelineBinarySize   = pipeline.size;
        pipeInfo.flags.clientInternal = 1;

        PAL_ASSERT(pipeline.size != 0);

        void* pMemory = PAL_MALLOC(pDevice->GetComputePipelineSize(pipeInfo, nullptr),
                                   pAllocator,
                                   Util::SystemAllocType::AllocInternal);
        if (pMemory != nullptr)
        {
            result = pDevice->CreateComputePipeline(
                pipeInfo,
                pMemory,
                &pPipelineMem[static_cast<size_t>(pipelineType)]);

            if (result != Pal::Result::Success)
            {
                // We need to explicitly free pMemory if an error occured because m_pPipeline won't be valid.
                PAL_SAFE_FREE(pMemory, pAllocator);
            }
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
// Creates all compute pipeline objects required by MsaaImageCopyUtil.
template <typename Allocator>
Pal::Result CreateMsaaImageCopyComputePipelines(
    Pal::IDevice*    pDevice,
    Allocator*       pAllocator,
    Pal::IPipeline** pPipelineMem)
{
    Pal::Result result = Pal::Result::Success;

    Pal::DeviceProperties properties = {};
    pDevice->GetProperties(&properties);

    const PipelineBinary* pTable = nullptr;

    switch (properties.revision)
    {
$SelectElfTable
    default:
        result = Pal::Result::ErrorUnknown;
        PAL_NOT_IMPLEMENTED();
        break;
    }

$EnvOverrides
    for (uint32 i = 0; ((result == Pal::Result::Success) && (i < static_cast<uint32>(MsaaImageCopyComputePipeline::Count))); i++)
    {
        result = CreateMsaaImageCopyComputePipeline(pDevice,
                                                    pAllocator,
                                                    pPipelineMem,
                                                    pTable,
                                                    static_cast<MsaaImageCopyComputePipeline>(i));
    }

    return result;
}

} // MsaaImageCopy
} // GpuUtil
