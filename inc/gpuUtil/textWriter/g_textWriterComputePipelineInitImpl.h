/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "g_textWriterComputePipelineInit.h"
#include "g_textWriterComputePipelineBinaries.h"

namespace GpuUtil
{
namespace TextWriterFont
{

// =====================================================================================================================
// Creates all compute pipeline objects required by TextWriter.
template <typename Allocator>
Pal::Result CreateTextWriterComputePipelines(
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
    case Pal::AsicRevision::Navi10:
    case Pal::AsicRevision::Navi12:
    case Pal::AsicRevision::Navi14:
        pTable = textWriterComputeBinaryTableNavi10;
        break;

    case Pal::AsicRevision::Navi21:
    case Pal::AsicRevision::Navi22:
    case Pal::AsicRevision::Navi23:
    case Pal::AsicRevision::Navi24:
    case Pal::AsicRevision::Rembrandt:
    case Pal::AsicRevision::Raphael:
        pTable = textWriterComputeBinaryTableNavi21;
        break;

    case Pal::AsicRevision::Navi31:
    case Pal::AsicRevision::Navi32:
    case Pal::AsicRevision::Navi33:
        pTable = textWriterComputeBinaryTableNavi31;
        break;

    case Pal::AsicRevision::Phoenix1:
        pTable = textWriterComputeBinaryTablePhoenix1;
        break;

    case Pal::AsicRevision::Phoenix2:
        pTable = textWriterComputeBinaryTablePhoenix2;
        break;

    default:
        result = Pal::Result::ErrorUnknown;
        PAL_NOT_IMPLEMENTED();
        break;
    }

    if (result == Pal::Result::Success)
    {
        Pal::ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary      = pTable[static_cast<size_t>(TextWriterComputePipeline::TextWriter)].pBuffer;
        pipeInfo.pipelineBinarySize   = pTable[static_cast<size_t>(TextWriterComputePipeline::TextWriter)].size;
        pipeInfo.flags.clientInternal = 1;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        void* pMemory = PAL_MALLOC(pDevice->GetComputePipelineSize(pipeInfo, nullptr),
                                   pAllocator,
                                   Util::SystemAllocType::AllocInternal);
        if (pMemory != nullptr)
        {
            result = pDevice->CreateComputePipeline(
                pipeInfo,
                pMemory,
                &pPipelineMem[static_cast<size_t>(TextWriterComputePipeline::TextWriter)]);

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

} // TextWriterFont
} // GpuUtil
