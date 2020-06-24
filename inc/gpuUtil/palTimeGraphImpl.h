/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palCmdBuffer.h"
#include "palDevice.h"
#include "palFormatInfo.h"
#include "palGpuMemory.h"
#include "palInlineFuncs.h"
#include "palPipeline.h"
#include "palSysMemory.h"
#include "palTimeGraph.h"

#include "timeGraph/g_timeGraphComputePipelineInitImpl.h"

namespace GpuUtil
{

// =====================================================================================================================
template <typename Allocator>
TimeGraph<Allocator>::TimeGraph(
    Pal::IDevice* pDevice,
    Allocator*    pAllocator)
    :
    m_pDevice(pDevice),
    m_pAllocator(pAllocator),
    m_pPipeline(nullptr),
    m_maxSrdSize(0)
{
    memset(&m_deviceProps, 0, sizeof(m_deviceProps));
    memset(m_memHeapProps, 0, sizeof(m_memHeapProps));
}

// =====================================================================================================================
template <typename Allocator>
TimeGraph<Allocator>::~TimeGraph()
{
    if (m_pPipeline != nullptr)
    {
        m_pPipeline->Destroy();
        PAL_SAFE_FREE(m_pPipeline, m_pAllocator);
    }
}

// =====================================================================================================================
// Initializes the TimeGraph class:
//      - Stores the device and GPU memory heap properties for later reference.
//      - Create the pipeline and GPU memory for the draw text pipeline.
//      - Create the GPU memory for the constant binary font data.
//      - Add all GPU memory references to the device and mark them as always resident.
template <typename Allocator>
Pal::Result TimeGraph<Allocator>::Init()
{
    Pal::Result result = m_pDevice->GetProperties(&m_deviceProps);

    if (result == Pal::Result::Success)
    {
        m_maxSrdSize = Util::Max(Util::Max(m_deviceProps.gfxipProperties.srdSizes.bufferView,
                                           m_deviceProps.gfxipProperties.srdSizes.imageView),
                                 Util::Max(m_deviceProps.gfxipProperties.srdSizes.fmaskView,
                                           m_deviceProps.gfxipProperties.srdSizes.sampler));

        result = m_pDevice->GetGpuMemoryHeapProperties(m_memHeapProps);
    }

    if (result == Pal::Result::Success)
    {
        result = TimeGraphDraw::CreateTimeGraphComputePipelines(m_pDevice, m_pAllocator, &m_pPipeline);
    }

    return result;
}

// =====================================================================================================================
// Creates a GpuMemory object using the given memory requirements.
template <typename Allocator>
Pal::Result TimeGraph<Allocator>::CreateGpuMemory(
    Pal::GpuMemoryRequirements* pMemReqs,
    Pal::IGpuMemory**           ppGpuMemory,
    Pal::gpusize*               pOffset)
{
    // Translate the memory requirements into a GpuMemory create info.
    Pal::GpuMemoryCreateInfo createInfo = {};
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 516
    createInfo.size      = pMemReqs->size;
    createInfo.alignment = pMemReqs->alignment;
#else
    const Pal::gpusize allocGranularity = m_deviceProps.gpuMemoryProperties.realMemAllocGranularity;
    createInfo.size      = Util::Pow2Align(pMemReqs->size, allocGranularity);
    createInfo.alignment = Util::Pow2Align(pMemReqs->alignment, allocGranularity);
#endif
    createInfo.vaRange   = Pal::VaRange::Default;
    createInfo.priority  = Pal::GpuMemPriority::VeryLow;
    createInfo.heapCount = pMemReqs->heapCount;

    memcpy(createInfo.heaps, pMemReqs->heaps, createInfo.heapCount);

    Pal::Result  result = Pal::Result::Success;
    const size_t objectSize = m_pDevice->GetGpuMemorySize(createInfo, &result);

    if (result == Pal::Result::Success)
    {
        void* pMemory = PAL_MALLOC(objectSize, m_pAllocator, Util::SystemAllocType::AllocInternal);
        if (pMemory != nullptr)
        {
            result = m_pDevice->CreateGpuMemory(createInfo, pMemory, reinterpret_cast<Pal::IGpuMemory**>(ppGpuMemory));
            if (result != Pal::Result::Success)
            {
                PAL_SAFE_FREE(pMemory, m_pAllocator);
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
// Executes a line draw (using a dispatch) onto the destination image.
template <typename Allocator>
void TimeGraph<Allocator>::DrawGraphLine(
    const Pal::IImage& dstImage,     // Desintation image.
    Pal::ICmdBuffer*   pCmdBuffer,   // Command buffer for drawing graph
    const Pal::uint32* pTimeData,    // Scaled Time Values
    Pal::uint32        xPosition,    // X drawing offset
    Pal::uint32        yPosition,    // Y drawing offset
    const Pal::uint32* pLineColor,   // Color of the line
    Pal::uint32        numDataPoints // Number of data points
    ) const
{
    ColorInfo colorInfo = {};

    // Pack the raw draw colors into the destination format.
    const Pal::SwizzledFormat imgFormat = dstImage.GetImageCreateInfo().swizzledFormat;

    Pal::uint32 swizzledLineColor[4] = {};
    Pal::Formats::SwizzleColor(imgFormat, pLineColor, swizzledLineColor);
    Pal::Formats::PackRawClearColor(imgFormat, swizzledLineColor, &colorInfo.lineColor[0]);

    Pal::gpusize dataAddr = 0;
    Pal::uint32* pData    = pCmdBuffer->CmdAllocateEmbeddedData(numDataPoints * sizeof(float), 1, &dataAddr);

    memcpy(pData, pTimeData, numDataPoints * sizeof(float));

    Pal::BufferViewInfo bufferViewInfo = {};

    // Create an SRD for the Time data.
    bufferViewInfo.gpuAddr        = dataAddr;
    bufferViewInfo.range          = numDataPoints * sizeof(float);
    bufferViewInfo.stride         = sizeof(float);
    bufferViewInfo.swizzledFormat = Pal::UndefinedSwizzledFormat;

    Pal::uint32 bufferViewSrd[4] = {};
    m_pDevice->CreateUntypedBufferViewSrds(1, &bufferViewInfo, &bufferViewSrd[0]);

    // Bind a buffer view for the Scaled Time Data in user data #0-3.
    pCmdBuffer->CmdSetUserData(Pal::PipelineBindPoint::Compute, 0, 4, &bufferViewSrd[0]);

    // Construct an embedded descriptor table. The first entry is an image view for the target image and the second is
    // for the color data .
    const Pal::uint32 srdDwords = m_maxSrdSize / sizeof(Pal::uint32);

    Pal::gpusize tableGpuAddr = 0;
    Pal::uint32* pTable       = pCmdBuffer->CmdAllocateEmbeddedData(srdDwords + (sizeof(ColorInfo) / sizeof(Pal::uint32)),
                                                                    1,
                                                                    &tableGpuAddr);

    CreateImageView(&dstImage, pTable);

    pTable += srdDwords;
    memcpy(pTable, &colorInfo, sizeof(ColorInfo));

    const Pal::uint32 tableGpuAddrLo = Util::LowPart(tableGpuAddr);

    // Bind that descriptor table to user data #4.
    pCmdBuffer->CmdSetUserData(Pal::PipelineBindPoint::Compute, 4, 1, &tableGpuAddrLo);

    const Pal::uint32 constantInfo[3] =
    {
        xPosition,
        yPosition,
        numDataPoints
    };

    pCmdBuffer->CmdSetUserData(Pal::PipelineBindPoint::Compute, 5, 3, &constantInfo[0]);

    // Bind the pipeline and issue one thread group.
    pCmdBuffer->CmdBindPipeline({ Pal::PipelineBindPoint::Compute, m_pPipeline, Pal::InternalApiPsoHash, });
    pCmdBuffer->CmdDispatch(32, 1, 1);
}

// =====================================================================================================================
// Creates an internal image view for the provided pImage.
template <typename Allocator>
void TimeGraph<Allocator>::CreateImageView(
    const Pal::IImage* pImage,
    void*              pOut
    ) const
{
    const auto& createInfo = pImage->GetImageCreateInfo();

    Pal::ImageViewInfo imgViewInfo = {};
    imgViewInfo.pImage   = pImage;
    imgViewInfo.viewType = Pal::ImageViewType::Tex2d;

    imgViewInfo.swizzledFormat = GetRawFormat(createInfo.swizzledFormat.format);

    // This is only used in a compute shader write, but will probably be immediately followed by a present
    imgViewInfo.possibleLayouts.engines = Pal::EngineTypeUniversal   | Pal::EngineTypeCompute;
    imgViewInfo.possibleLayouts.usages  = Pal::LayoutShaderWrite     | Pal::LayoutShaderRead |
                                          Pal::LayoutPresentWindowed | Pal::LayoutPresentFullscreen;

    imgViewInfo.subresRange.startSubres.aspect     = Pal::ImageAspect::Color;
    imgViewInfo.subresRange.startSubres.arraySlice = 0;
    imgViewInfo.subresRange.startSubres.mipLevel   = 0;
    imgViewInfo.subresRange.numSlices              = createInfo.arraySize;
    imgViewInfo.subresRange.numMips                = createInfo.mipLevels;

    m_pDevice->CreateImageViewSrds(1, &imgViewInfo, pOut);
}

// =====================================================================================================================
// Gets a raw Uint format that matches the bit depth of the provided format.
template <typename Allocator>
Pal::SwizzledFormat TimeGraph<Allocator>::GetRawFormat(
    Pal::ChNumFormat oldFmt)
{
    Pal::SwizzledFormat retFmt = Pal::UndefinedSwizzledFormat;

    switch (Pal::Formats::BitsPerPixel(oldFmt))
    {
    case 8:
        retFmt.format = Pal::ChNumFormat::X8_Uint;
        retFmt.swizzle =
        { Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::One };
        break;
    case 16:
        retFmt.format = Pal::ChNumFormat::X16_Uint;
        retFmt.swizzle =
        { Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::One };
        break;
    case 32:
        retFmt.format = Pal::ChNumFormat::X32_Uint;
        retFmt.swizzle =
        { Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::One };
        break;
    case 64:
        retFmt.format = Pal::ChNumFormat::X32Y32_Uint;
        retFmt.swizzle =
        { Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Y,    Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::One };
        break;
    case 128:
        retFmt.format = Pal::ChNumFormat::X32Y32Z32W32_Uint;
        retFmt.swizzle =
        { Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Y,    Pal::ChannelSwizzle::Z,    Pal::ChannelSwizzle::W };
        break;
    default:
        retFmt = Pal::UndefinedSwizzledFormat;
        break;
    }

    return retFmt;
}

} // GpuUtil
