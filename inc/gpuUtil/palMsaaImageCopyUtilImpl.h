/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palMsaaImageCopyUtil.h"
#include "msaaImageCopy/g_msaaImageCopyComputePipelineInitImpl.h"

namespace GpuUtil
{

// =====================================================================================================================
template <typename Allocator>
MsaaImageCopyUtil<Allocator>::MsaaImageCopyUtil(
    Pal::IDevice*  pDevice,
    Allocator*     pAllocator)
    :
    m_pDevice(pDevice),
    m_pAllocator(pAllocator),
    m_maxSrdSizeInDwords(0)
{
    memset(&m_pPipelines[0], 0, sizeof(m_pPipelines));
}

// =====================================================================================================================
template <typename Allocator>
MsaaImageCopyUtil<Allocator>::~MsaaImageCopyUtil()
{
    for (Pal::uint32 i = 0; i < static_cast<Pal::uint32>(MsaaImageCopy::MsaaImageCopyComputePipeline::Count); i++)
    {
        if (m_pPipelines[i] != nullptr)
        {
            m_pPipelines[i]->Destroy();
            PAL_SAFE_FREE(m_pPipelines[i], m_pAllocator);
        }
    }
}

// =====================================================================================================================
// Initializes the MsaaImageCopyUtil class:
//      - Stores relevant device properties for later reference.
//      - Create the pipelines and GPU memory for msaa copy image.
template <typename Allocator>
Pal::Result MsaaImageCopyUtil<Allocator>::Init()
{
    Pal::DeviceProperties deviceProps;
    Pal::Result result = m_pDevice->GetProperties(&deviceProps);

    if (result == Pal::Result::Success)
    {
        const Pal::uint32 maxSrdSize = Util::Max(deviceProps.gfxipProperties.srdSizes.bufferView,
                                                 deviceProps.gfxipProperties.srdSizes.imageView,
                                                 deviceProps.gfxipProperties.srdSizes.fmaskView,
                                                 deviceProps.gfxipProperties.srdSizes.sampler);

        m_maxSrdSizeInDwords = Util::NumBytesToNumDwords(maxSrdSize);

        result = MsaaImageCopy::CreateMsaaImageCopyComputePipelines(m_pDevice, m_pAllocator, &m_pPipelines[0]);
    }

    return result;
}

// =====================================================================================================================
// Helper function to allocate and bind embedded user data
template <typename Allocator>
Pal::uint32* MsaaImageCopyUtil<Allocator>::CreateAndBindEmbeddedUserData(
    Pal::ICmdBuffer*  pCmdBuffer,
    Pal::uint32       sizeInDwords,
    Pal::uint32       entryToBind
    ) const
{
    Pal::gpusize gpuVirtAddr = 0;
    Pal::uint32*const pCmdSpace = pCmdBuffer->CmdAllocateEmbeddedData(sizeInDwords, m_maxSrdSizeInDwords, &gpuVirtAddr);
    PAL_ASSERT(pCmdSpace != nullptr);

    const Pal::uint32 gpuVirtAddrLo = Util::LowPart(gpuVirtAddr);
    pCmdBuffer->CmdSetUserData(Pal::PipelineBindPoint::Compute, entryToBind, 1, &gpuVirtAddrLo);

    return pCmdSpace;
}

// =====================================================================================================================
// Populates an ImageViewInfo that wraps the given range of the provided image object.
template <typename Allocator>
void MsaaImageCopyUtil<Allocator>::BuildImageViewInfo(
    Pal::ImageViewInfo*   pInfo,
    const Pal::IImage*    pImage,
    Pal::SubresId         subresId,
    Pal::SwizzledFormat   swizzledFormat,
    bool                  isShaderWriteable
    ) const
{
    const Pal::ImageType imageType = pImage->GetImageCreateInfo().imageType;

    pInfo->pImage   = pImage;
    pInfo->viewType = static_cast<Pal::ImageViewType>(imageType);
    pInfo->subresRange.startSubres = subresId;
    pInfo->subresRange.numPlanes   = 1;
    pInfo->subresRange.numMips     = 1;
    pInfo->subresRange.numSlices   = 1;
    pInfo->swizzledFormat          = swizzledFormat;

    // Msaa image copy only uses compute shaders, where the write-out surface is assumed to be write-only.
    pInfo->possibleLayouts = { (isShaderWriteable ? Pal::LayoutShaderWrite : Pal::LayoutShaderRead),
                               Pal::EngineTypeUniversal | Pal::EngineTypeCompute };
}

// =====================================================================================================================
// Msaa image copy from source image to destination image using the specified command buffer.
template <typename Allocator>
void MsaaImageCopyUtil<Allocator>::MsaaImageCopy(
    Pal::ICmdBuffer*            pCmdBuffer,
    const Pal::IImage&          srcImage,
    const Pal::IImage&          dstImage,
    Pal::uint32                 regionCount,
    const Pal::ImageCopyRegion* pRegions
    ) const
{
    const auto& srcInfo = srcImage.GetImageCreateInfo();
    const auto& dstInfo = dstImage.GetImageCreateInfo();

    PAL_ASSERT((srcInfo.imageType == dstInfo.imageType) && (srcInfo.imageType == Pal::ImageType::Tex2d) &&
               (srcInfo.samples > 1) && (dstInfo.samples > 1));

    // Save current command buffer state and bind the pipeline.
    pCmdBuffer->CmdSaveComputeState(Pal::ComputeStatePipelineAndUserData);

    MsaaImageCopy::MsaaImageCopyComputePipeline msaaImageCopyPipeline = srcInfo.usageFlags.depthStencil ?
        MsaaImageCopy::MsaaImageCopyComputePipeline::MsaaDSCopy : MsaaImageCopy::MsaaImageCopyComputePipeline::MsaaRTCopy;

    const Pal::IPipeline*const pPipeline = m_pPipelines[static_cast<Pal::uint32>(msaaImageCopyPipeline)];
        PAL_ASSERT(pPipeline != nullptr);

    pCmdBuffer->CmdBindPipeline({ Pal::PipelineBindPoint::Compute, pPipeline, Pal::InternalApiPsoHash, });

     // Now begin processing the list of copy regions.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        Pal::ImageCopyRegion copyRegion = pRegions[idx];

        Pal::SubresLayout layout = {};
        srcImage.GetSubresourceLayout(copyRegion.srcSubres, &layout);
        Pal::SwizzledFormat srcFormat = layout.planeFormat;

        dstImage.GetSubresourceLayout(copyRegion.dstSubres, &layout);
        Pal::SwizzledFormat dstFormat = layout.planeFormat;

        // The hardware can't handle UAV stores using sRGB num format.  The resolve shaders already contain a
        // linear-to-gamma conversion, but in order for that to work the output UAV's num format must be patched to
        // be simple UNORM.
        // For simplicity, MSAA copy also treats src sRGB as UNORM, with the assumption that src format == dst format.
        if (Pal::Formats::IsSrgb(srcFormat.format))
        {
            srcFormat.format = Pal::Formats::ConvertToUnorm(srcFormat.format);
            PAL_ASSERT((Pal::Formats::IsUndefined(srcFormat.format) == false) && Pal::Formats::IsSrgb(srcFormat.format));
        }

        if (Pal::Formats::IsSrgb(dstFormat.format))
        {
            dstFormat.format = Pal::Formats::ConvertToUnorm(dstFormat.format);
            PAL_ASSERT(Pal::Formats::IsUndefined(dstFormat.format) == false);
        }

        // The shader expects the following layout for the embedded user-data constant.
        // cb0[0] = (source X offset, source Y offset, copy width, copy height)
        // cb0[1] = (dest X offset, dest Y offset, src sample count, dst sample count)
        const Pal::uint32 constantData[8] =
        {
            static_cast<Pal::uint32>(copyRegion.srcOffset.x),
            static_cast<Pal::uint32>(copyRegion.srcOffset.y),
            copyRegion.extent.width,
            copyRegion.extent.height,
            static_cast<Pal::uint32>(copyRegion.dstOffset.x),
            static_cast<Pal::uint32>(copyRegion.dstOffset.y),
            srcInfo.samples,
            dstInfo.samples
        };

        const Pal::uint32 dataDwords = Util::NumBytesToNumDwords(sizeof(constantData));
        const Pal::uint32 srdDwords  = m_maxSrdSizeInDwords * 2;
        Pal::uint32* pUserData = CreateAndBindEmbeddedUserData(pCmdBuffer, srdDwords + dataDwords, 0);

        Pal::ImageViewInfo imageView[2] = {};
        BuildImageViewInfo(&imageView[0], &dstImage, copyRegion.dstSubres, dstFormat, true);
        BuildImageViewInfo(&imageView[1], &srcImage, copyRegion.srcSubres, srcFormat, false);

        m_pDevice->CreateImageViewSrds(2, &imageView[0], pUserData);

        pUserData += srdDwords;

        memcpy(pUserData, constantData, sizeof(constantData));

        pCmdBuffer->CmdDispatch({Util::RoundUpQuotient(copyRegion.extent.width,  MsaaImageCopy::ThreadsPerGroupX),
                                 Util::RoundUpQuotient(copyRegion.extent.height, MsaaImageCopy::ThreadsPerGroupY), 1});
    }

    pCmdBuffer->CmdRestoreComputeState(Pal::ComputeStatePipelineAndUserData);
}

} // GpuUtil
