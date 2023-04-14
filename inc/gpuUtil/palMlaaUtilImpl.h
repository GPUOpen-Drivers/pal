/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palMlaaUtil.h"
#include "palDeveloperHooks.h"
#include "mlaa/g_mlaaComputePipelineInitImpl.h"

namespace GpuUtil
{

// =====================================================================================================================
template <typename Allocator>
MlaaUtil<Allocator>::MlaaUtil(
    Pal::IDevice* pDevice,
    Allocator*    pAllocator,
    bool          fastPath)
    :
    m_pDevice(pDevice),
    m_pAllocator(pAllocator),
    m_fastPath(fastPath),
    m_width(0),
    m_height(0),
    m_maxSrdSizeInDwords(0)
{
    memset(m_pPipelines, 0, sizeof(m_pPipelines));
    memset(m_pAuxImages, 0, sizeof(m_pAuxImages));
    memset(m_pAuxGpuMem, 0, sizeof(m_pAuxGpuMem));
    memset(&m_deviceProps, 0, sizeof(m_deviceProps));
}

// =====================================================================================================================
template <typename Allocator>
MlaaUtil<Allocator>::~MlaaUtil()
{
    for (Pal::uint32 i = 0; i < static_cast<Pal::uint32>(Mlaa::MlaaComputePipeline::Count); i++)
    {
        if (m_pPipelines[i] != nullptr)
        {
            m_pPipelines[i]->Destroy();
            PAL_SAFE_FREE(m_pPipelines[i], m_pAllocator);
        }
    }

    CleanupAuxImages();
}

// =====================================================================================================================
// Initializes the MlaaUtil class:
//      - Stores the device and GPU memory heap properties for later reference.
//      - Create the pipelines and GPU memory for mlaa resolve.
template <typename Allocator>
Pal::Result MlaaUtil<Allocator>::Init()
{
    Pal::Result result = m_pDevice->GetProperties(&m_deviceProps);

    if (result == Pal::Result::Success)
    {
        Pal::uint32 maxSrdSize = Util::Max(Util::Max(m_deviceProps.gfxipProperties.srdSizes.bufferView,
                                           m_deviceProps.gfxipProperties.srdSizes.imageView),
                                 Util::Max(m_deviceProps.gfxipProperties.srdSizes.fmaskView,
                                           m_deviceProps.gfxipProperties.srdSizes.sampler));

        m_maxSrdSizeInDwords = Util::NumBytesToNumDwords(maxSrdSize);
    }

    if (result == Pal::Result::Success)
    {
        result = Mlaa::CreateMlaaComputePipelines(m_pDevice, m_pAllocator, m_pPipelines);
    }

    return result;
}

// =====================================================================================================================
// Setup auxiliary image objects.
template <typename Allocator>
Pal::Result MlaaUtil<Allocator>::SetupAuxImages(
    Pal::uint32  srcWidth,
    Pal::uint32  srcHeight)
{
    Pal::Result result = Pal::Result::Success;

    // If auxiliary image dimension is not matching with dimension of src image, create new auxiliary images.
    if ((m_width != srcWidth) || (m_height != srcHeight))
    {
        if ((m_width > 0) && (m_height > 0))
        {
            CleanupAuxImages();
        }

        Pal::GpuMemoryRef memRef[static_cast<Pal::uint32>(MlaaAuxImage::Count)] = {};
        Pal::uint32       memRefCount = 0;
        for (Pal::uint32 i = 0; i < static_cast<Pal::uint32>(MlaaAuxImage::Count); i++)
        {
            Pal::ImageCreateInfo imageInfo = {};
            bool createImage = false;
            if (i == static_cast<Pal::uint32>(MlaaAuxImage::SepEdge))
            {
                createImage = true;
                imageInfo.swizzledFormat.format = Pal::ChNumFormat::X8_Uint;
                imageInfo.swizzledFormat.swizzle = { Pal::ChannelSwizzle::X,
                                                     Pal::ChannelSwizzle::Zero,
                                                     Pal::ChannelSwizzle::Zero,
                                                     Pal::ChannelSwizzle::One };
            }
            else if ((m_fastPath == false) && (i < static_cast<Pal::uint32>(MlaaAuxImage::EdgeCountFast)))
            {
                createImage = true;
                imageInfo.swizzledFormat.format = Pal::ChNumFormat::X32_Uint;
                imageInfo.swizzledFormat.swizzle = { Pal::ChannelSwizzle::X,
                                                     Pal::ChannelSwizzle::Zero,
                                                     Pal::ChannelSwizzle::Zero,
                                                     Pal::ChannelSwizzle::One };
            }
            else if (m_fastPath && (i == static_cast<Pal::uint32>(MlaaAuxImage::EdgeCountFast)))
            {
                createImage = true;
                imageInfo.swizzledFormat.format = Pal::ChNumFormat::X8Y8_Uint;
                imageInfo.swizzledFormat.swizzle = { Pal::ChannelSwizzle::X,
                                                     Pal::ChannelSwizzle::Y,
                                                     Pal::ChannelSwizzle::Zero,
                                                     Pal::ChannelSwizzle::One };
            }

            if (createImage)
            {
                imageInfo.arraySize              = 1;
                imageInfo.fragments              = 1;
                imageInfo.samples                = 1;
                imageInfo.tiling                 = Pal::ImageTiling::Optimal;
                imageInfo.extent.width           = srcWidth;
                imageInfo.extent.height          = srcHeight;
                imageInfo.extent.depth           = 1;
                imageInfo.imageType              = Pal::ImageType::Tex2d;
                imageInfo.mipLevels              = 1;
                imageInfo.usageFlags.shaderRead  = 1;
                imageInfo.usageFlags.colorTarget = 1;
                imageInfo.flags.invariant        = 1;
                imageInfo.metadataMode           = Pal::MetadataMode::Disabled;
                imageInfo.metadataTcCompatMode   = Pal::MetadataTcCompatMode::Disabled;

                const size_t objectSize = m_pDevice->GetImageSize(imageInfo, &result);

                if (result == Pal::Result::Success)
                {
                    void* pMemory = PAL_MALLOC(objectSize, m_pAllocator, Util::SystemAllocType::AllocInternal);
                    if (pMemory != nullptr)
                    {
                        result = m_pDevice->CreateImage(imageInfo,
                                                        pMemory,
                                                        &m_pAuxImages[i]);
                        if (result == Pal::Result::Success)
                        {
                            result = CreateImageMemoryObject(m_pAuxImages[i], &m_pAuxGpuMem[i]);
                            if (result == Pal::Result::Success)
                            {
                                memRef[memRefCount].pGpuMemory = m_pAuxGpuMem[i];
                                memRefCount++;
                            }
                        }
                        else
                        {
                            PAL_SAFE_FREE(pMemory, m_pAllocator);
                        }
                    }
                    else
                    {
                        result = Pal::Result::ErrorOutOfMemory;
                    }
                }

                if (result != Pal::Result::Success)
                {
                    break;
                }
            }
        }

        if (result == Pal::Result::Success)
        {
            // Make GPU memories of auxiliary images always resident.
            result = m_pDevice->AddGpuMemoryReferences(memRefCount, memRef, nullptr, Pal::GpuMemoryRefCantTrim);
        }

        if (result == Pal::Result::Success)
        {
            m_width = srcWidth;
            m_height = srcHeight;
        }
    }

    return result;
}

// =====================================================================================================================
// Cleanup auxiliary image objects.
template <typename Allocator>
void MlaaUtil<Allocator>::CleanupAuxImages()
{
    for (Pal::uint32 i = 0; i < static_cast<Pal::uint32>(MlaaAuxImage::Count); i++)
    {
        if (m_pAuxGpuMem[i] != nullptr)
        {
            m_pAuxGpuMem[i]->Destroy();
            PAL_SAFE_FREE(m_pAuxGpuMem[i], m_pAllocator);
            m_pAuxGpuMem[i] = nullptr;
        }

        if (m_pAuxImages[i] != nullptr)
        {
            m_pAuxImages[i]->Destroy();
            PAL_SAFE_FREE(m_pAuxImages[i], m_pAllocator);
            m_pAuxImages[i] = nullptr;
        }
    }
}

// =====================================================================================================================
// Helper function to allocate and bind embedded user data
template <typename Allocator>
Pal::uint32* MlaaUtil<Allocator>::CreateAndBindEmbeddedUserData(
    Pal::ICmdBuffer*  pCmdBuffer,
    Pal::uint32       sizeInDwords)
{
    Pal::gpusize gpuVirtAddr = 0;
    Pal::uint32*const pCmdSpace = pCmdBuffer->CmdAllocateEmbeddedData(sizeInDwords, m_maxSrdSizeInDwords, &gpuVirtAddr);
    PAL_ASSERT(pCmdSpace != nullptr);

    const Pal::uint32 gpuVirtAddrLo = Util::LowPart(gpuVirtAddr);
    pCmdBuffer->CmdSetUserData(Pal::PipelineBindPoint::Compute, 0, 1, &gpuVirtAddrLo);

    return pCmdSpace;
}

// =====================================================================================================================
// Populates an ImageViewInfo that wraps the given range of the provided image object.
template <typename Allocator>
void MlaaUtil<Allocator>::BuildImageViewInfo(
    Pal::ImageViewInfo*   pInfo,
    const Pal::IImage*    pImage,
    const Pal::SubresId&  subresId,
    bool                  isShaderWriteable)
{
    const Pal::ImageType imageType = pImage->GetImageCreateInfo().imageType;

    pInfo->pImage   = pImage;
    pInfo->viewType = static_cast<Pal::ImageViewType>(imageType);
    pInfo->subresRange.startSubres = subresId;
    pInfo->subresRange.numPlanes   = 1;
    pInfo->subresRange.numMips     = 1;
    pInfo->subresRange.numSlices   = 1;
    pInfo->swizzledFormat          = pImage->GetImageCreateInfo().swizzledFormat;

    // MLAA only uses compute shaders
    pInfo->possibleLayouts = { Pal::LayoutShaderRead, Pal::EngineTypeUniversal | Pal::EngineTypeCompute };
    if (isShaderWriteable)
    {
        pInfo->possibleLayouts.usages |= Pal::LayoutShaderWrite;
    }
}

// =====================================================================================================================
// Creates the GPU memory object and binds it to the provided image
template <typename Allocator>
Pal::Result MlaaUtil<Allocator>::CreateImageMemoryObject(
    Pal::IImage*      pImage,
    Pal::IGpuMemory** ppGpuMemory)
{
    Pal::GpuMemoryRequirements memReqs = {};
    pImage->GetGpuMemoryRequirements(&memReqs);

    // Translate the memory requirements into a GpuMemory create info.
    Pal::GpuMemoryCreateInfo createInfo = {};
    createInfo.size      = memReqs.size;
    createInfo.alignment = memReqs.alignment;
    createInfo.vaRange   = Pal::VaRange::Default;
    createInfo.priority  = Pal::GpuMemPriority::Normal;
    createInfo.heapCount = memReqs.heapCount;
    createInfo.pImage    = pImage;

    for (Pal::uint32 i = 0; i < createInfo.heapCount; i++)
    {
        createInfo.heaps[i] = memReqs.heaps[i];
    }

    Pal::Result  result = Pal::Result::Success;
    const size_t objectSize = m_pDevice->GetGpuMemorySize(createInfo, &result);

    if (result == Pal::Result::Success)
    {
        void* pMemory = PAL_MALLOC(objectSize, m_pAllocator, Util::SystemAllocType::AllocInternal);
        if (pMemory != nullptr)
        {
            Pal::IGpuMemory* pGpuMemory = nullptr;
            result = m_pDevice->CreateGpuMemory(createInfo, pMemory, &pGpuMemory);
            if (result == Pal::Result::Success)
            {
                result = pImage->BindGpuMemory(pGpuMemory, 0);
                if (result == Pal::Result::Success)
                {
                    *ppGpuMemory = pGpuMemory;
                }
                else
                {
                    pGpuMemory->Destroy();
                    PAL_SAFE_FREE(pMemory, m_pAllocator);
                }
            }
            else
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
// 1st stage of MLAA resolve: Find the separating edges
template <typename Allocator>
void MlaaUtil<Allocator>::FindSepEdge(
    Pal::ICmdBuffer*     pCmdBuffer,
    const Pal::IImage&   srcImage,
    const Pal::SubresId& srcSubres)
{
    // The shader expects the following layout for the embedded user-data constant.
    // IterationDepth, source X dimension, source Y dimension, unused
    const Pal::uint32 constantData[4] =
    {
        0,
        m_width - 1,
        m_height - 1,
        0
    };

    const Pal::uint32 dataDwords = Util::NumBytesToNumDwords(sizeof(constantData));
    const Pal::uint32 srdDwords  = m_maxSrdSizeInDwords * 2;
    Pal::uint32* pUserData = CreateAndBindEmbeddedUserData(pCmdBuffer, srdDwords + dataDwords);

    const Pal::IImage*  pDstImage = m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::SepEdge)];
    const Pal::SubresId dstSubres = {};

    Pal::ImageViewInfo imageView[2] = {};
    BuildImageViewInfo(&imageView[0], pDstImage, dstSubres, true);
    BuildImageViewInfo(&imageView[1], &srcImage, srcSubres, false);

    m_pDevice->CreateImageViewSrds(2, &imageView[0], pUserData);
    pUserData += srdDwords;

    memcpy(pUserData, constantData, sizeof(constantData));

    pCmdBuffer->CmdBindPipeline({Pal::PipelineBindPoint::Compute,
        m_pPipelines[static_cast<Pal::uint32>(Mlaa::MlaaComputePipeline::MlaaFindSepEdge)], Pal::InternalApiPsoHash});

    const Pal::uint32 threadGroupsX = (m_width  + Mlaa::ThreadsPerGroupX - 1) / Mlaa::ThreadsPerGroupX;
    const Pal::uint32 threadGroupsY = (m_height + Mlaa::ThreadsPerGroupY - 1) / Mlaa::ThreadsPerGroupY;

    pCmdBuffer->CmdDispatch({threadGroupsX, threadGroupsY, 1});
}

// =====================================================================================================================
// 2nd stage of MLAA resolve: Calculate the separating edge length (recursive doubling path)
template <typename Allocator>
void MlaaUtil<Allocator>::CalcSepEdgeLength(
    Pal::ICmdBuffer* pCmdBuffer,
    Pal::uint32      iterationDepth)
{
    // The shader expects the following layout for the embedded user-data constant.
    // IterationDepth, source X dimension, source Y dimension, unused
    const Pal::uint32 constantData[4] =
    {
        iterationDepth,
        m_width - 1,
        m_height - 1,
        0
    };

    const Pal::uint32 dataDwords = Util::NumBytesToNumDwords(sizeof(constantData));
    const Pal::uint32 srdDwords  = m_maxSrdSizeInDwords * 5;
    Pal::uint32* pUserData = CreateAndBindEmbeddedUserData(pCmdBuffer, srdDwords + dataDwords);

    // Select src and dst image
    const Pal::IImage* pSrcImage0 = m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::SepEdge)];
    const Pal::IImage* pSrcImage1 = nullptr;
    const Pal::IImage* pSrcImage2 = nullptr;
    const Pal::IImage* pDstImage0 = nullptr;
    const Pal::IImage* pDstImage1 = nullptr;
    if ((iterationDepth % 2) == 0)
    {
        pSrcImage1 = m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::HorzEdgeCountA)];
        pSrcImage2 = m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::VertEdgeCountA)];
        pDstImage0 = m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::HorzEdgeCountB)];
        pDstImage1 = m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::VertEdgeCountB)];
    }
    else
    {
        pSrcImage1 = m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::HorzEdgeCountB)];
        pSrcImage2 = m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::VertEdgeCountB)];
        pDstImage0 = m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::HorzEdgeCountA)];
        pDstImage1 = m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::VertEdgeCountA)];
    }

    const Pal::SubresId subres = {};

    Pal::ImageViewInfo imageView[5] = {};
    BuildImageViewInfo(&imageView[0], pDstImage0, subres, true);
    BuildImageViewInfo(&imageView[1], pDstImage1, subres, true);
    BuildImageViewInfo(&imageView[2], pSrcImage0, subres, false);
    BuildImageViewInfo(&imageView[3], pSrcImage1, subres, false);
    BuildImageViewInfo(&imageView[4], pSrcImage2, subres, false);

    m_pDevice->CreateImageViewSrds(5, &imageView[0], pUserData);
    pUserData += srdDwords;

    memcpy(pUserData, constantData, sizeof(constantData));

    const Pal::IPipeline* const pPipeline = (iterationDepth == 0) ?
        m_pPipelines[static_cast<Pal::uint32>(Mlaa::MlaaComputePipeline::MlaaCalcSepEdgeLengthInitial)] :
        m_pPipelines[static_cast<Pal::uint32>(Mlaa::MlaaComputePipeline::MlaaCalcSepEdgeLength)];

    pCmdBuffer->CmdBindPipeline({Pal::PipelineBindPoint::Compute, pPipeline, Pal::InternalApiPsoHash});

    const Pal::uint32 threadGroupsX = (m_width  + Mlaa::ThreadsPerGroupX - 1) / Mlaa::ThreadsPerGroupX;
    const Pal::uint32 threadGroupsY = (m_height + Mlaa::ThreadsPerGroupY - 1) / Mlaa::ThreadsPerGroupY;

    pCmdBuffer->CmdDispatch({threadGroupsX, threadGroupsY, 1});
}

// =====================================================================================================================
// 2nd stage of MLAA resolve: Calculate the separating edge length (fast path)
template <typename Allocator>
void MlaaUtil<Allocator>::CalcSepEdgeLengthFast(
    Pal::ICmdBuffer* pCmdBuffer)
{
    // The shader expects the following layout for the embedded user-data constant.
    // IterationDepth, source X dimension, source Y dimension, unused
    const Pal::uint32 constantData[4] =
    {
        0,
        m_width - 1,
        m_height - 1,
        0
    };

    const Pal::uint32 dataDwords = Util::NumBytesToNumDwords(sizeof(constantData));
    const Pal::uint32 srdDwords  = m_maxSrdSizeInDwords * 2;
    Pal::uint32* pUserData = CreateAndBindEmbeddedUserData(pCmdBuffer, srdDwords + dataDwords);

    const Pal::IImage* pSrcImage = m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::SepEdge)];
    const Pal::IImage* pDstImage = m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::EdgeCountFast)];
    const Pal::SubresId subres   = {};

    Pal::ImageViewInfo imageView[2] = {};
    BuildImageViewInfo(&imageView[0], pDstImage, subres, true);
    BuildImageViewInfo(&imageView[1], pSrcImage, subres, false);

    m_pDevice->CreateImageViewSrds(2, &imageView[0], pUserData);
    pUserData += srdDwords;

    memcpy(pUserData, constantData, sizeof(constantData));

    pCmdBuffer->CmdBindPipeline({Pal::PipelineBindPoint::Compute,
        m_pPipelines[static_cast<Pal::uint32>(Mlaa::MlaaComputePipeline::MlaaCalcSepEdgeLengthFast)],
        Pal::InternalApiPsoHash});

    const Pal::uint32 threadGroupsX = (m_width  + Mlaa::ThreadsPerGroupX - 1) / Mlaa::ThreadsPerGroupX;
    const Pal::uint32 threadGroupsY = (m_height + Mlaa::ThreadsPerGroupY - 1) / Mlaa::ThreadsPerGroupY;

    pCmdBuffer->CmdDispatch({threadGroupsX, threadGroupsY, 1});
}

// =====================================================================================================================
// Final stage of MLAA resolve: Blend the pixels along the separating edge
template <typename Allocator>
void MlaaUtil<Allocator>::FinalBlend(
    Pal::ICmdBuffer*     pCmdBuffer,
    const Pal::IImage&   srcImage,
    const Pal::SubresId& srcSubres,
    const Pal::IImage&   dstImage,
    const Pal::SubresId& dstSubres,
    Pal::uint32          maxIterationDepth)
{
    // The shader expects the following layout for the embedded user-data constant.
    // IterationDepth, source X dimension, source Y dimension, unused
    const Pal::uint32 constantData[4] =
    {
        0,
        m_width - 1,
        m_height - 1,
        0
    };

    const Pal::uint32 dataDwords = Util::NumBytesToNumDwords(sizeof(constantData));
    const Pal::uint32 srdDwords  = m_maxSrdSizeInDwords * 4;
    Pal::uint32* pUserData = CreateAndBindEmbeddedUserData(pCmdBuffer, srdDwords + dataDwords);

    const Pal::IImage* pSrcImage1 = (maxIterationDepth % 2 != 0) ?
        m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::HorzEdgeCountA)] :
        m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::HorzEdgeCountB)];
    const Pal::IImage* pSrcImage2 = (maxIterationDepth % 2 != 0) ?
        m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::VertEdgeCountA)] :
        m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::VertEdgeCountB)];
    const Pal::SubresId subres = {};

    Pal::ImageViewInfo imageView[4] = {};
    BuildImageViewInfo(&imageView[0], &dstImage, dstSubres, true);
    BuildImageViewInfo(&imageView[1], &srcImage, srcSubres, false);
    BuildImageViewInfo(&imageView[2], pSrcImage1, subres, false);
    BuildImageViewInfo(&imageView[3], pSrcImage2, subres, false);

    m_pDevice->CreateImageViewSrds(4, &imageView[0], pUserData);
    pUserData += srdDwords;

    memcpy(pUserData, constantData, sizeof(constantData));

    pCmdBuffer->CmdBindPipeline({Pal::PipelineBindPoint::Compute,
        m_pPipelines[static_cast<Pal::uint32>(Mlaa::MlaaComputePipeline::MlaaFinalBlend)],
        Pal::InternalApiPsoHash});

    const Pal::uint32 threadGroupsX = (m_width  + Mlaa::ThreadsPerGroupX - 1) / Mlaa::ThreadsPerGroupX;
    const Pal::uint32 threadGroupsY = (m_height + Mlaa::ThreadsPerGroupY - 1) / Mlaa::ThreadsPerGroupY;

    pCmdBuffer->CmdDispatch({threadGroupsX, threadGroupsY, 1});
}

// =====================================================================================================================
// Final stage of MLAA resolve: Blend the pixels along the separating edge (fast path)
template <typename Allocator>
void MlaaUtil<Allocator>::FinalBlendFast(
    Pal::ICmdBuffer*     pCmdBuffer,
    const Pal::IImage&   srcImage,
    const Pal::SubresId& srcSubres,
    const Pal::IImage&   dstImage,
    const Pal::SubresId& dstSubres)
{
    // The shader expects the following layout for the embedded user-data constant.
    // IterationDepth, source X dimension, source Y dimension, unused
    const Pal::uint32 constantData[4] =
    {
        0,
        m_width - 1,
        m_height - 1,
        0
    };

    const Pal::uint32 dataDwords = Util::NumBytesToNumDwords(sizeof(constantData));
    const Pal::uint32 srdDwords  = m_maxSrdSizeInDwords * 3;
    Pal::uint32* pUserData = CreateAndBindEmbeddedUserData(pCmdBuffer, srdDwords + dataDwords);

    const Pal::IImage* pSrcImage1 = m_pAuxImages[static_cast<Pal::uint32>(MlaaAuxImage::EdgeCountFast)];
    const Pal::SubresId subres = {};

    Pal::ImageViewInfo imageView[3] = {};
    BuildImageViewInfo(&imageView[0], &dstImage, dstSubres, true);
    BuildImageViewInfo(&imageView[1], &srcImage, srcSubres, false);
    BuildImageViewInfo(&imageView[2], pSrcImage1, subres, false);

    m_pDevice->CreateImageViewSrds(3, &imageView[0], pUserData);
    pUserData += srdDwords;

    memcpy(pUserData, constantData, sizeof(constantData));

    pCmdBuffer->CmdBindPipeline({Pal::PipelineBindPoint::Compute,
        m_pPipelines[static_cast<Pal::uint32>(Mlaa::MlaaComputePipeline::MlaaFinalBlendFast)],
        Pal::InternalApiPsoHash});

    const Pal::uint32 threadGroupsX = (m_width  + Mlaa::ThreadsPerGroupX - 1) / Mlaa::ThreadsPerGroupX;
    const Pal::uint32 threadGroupsY = (m_height + Mlaa::ThreadsPerGroupY - 1) / Mlaa::ThreadsPerGroupY;

    pCmdBuffer->CmdDispatch({threadGroupsX, threadGroupsY, 1});
}

// =====================================================================================================================
// MLAA resolve from source image to destination image using the specific command buffer.
template <typename Allocator>
void MlaaUtil<Allocator>::ResolveImage(
    Pal::ICmdBuffer*      pCmdBuffer,
    const Pal::IImage&    srcImage,
    const Pal::SubresId&  srcSubres,
    const Pal::IImage&    dstImage,
    const Pal::SubresId&  dstSubres)
{
    const Pal::uint32 width  = srcImage.GetImageCreateInfo().extent.width;
    const Pal::uint32 height = srcImage.GetImageCreateInfo().extent.height;

    Pal::Result result = SetupAuxImages(width, height);
    PAL_ASSERT(result == Pal::Result::Success);

    // 1st stage: Find the separating edges
    FindSepEdge(pCmdBuffer, srcImage, srcSubres);

    Pal::BarrierInfo barrier = {};
    barrier.waitPoint = Pal::HwPipePoint::HwPipePreCs;

    const Pal::HwPipePoint postCs = Pal::HwPipePoint::HwPipePostCs;
    barrier.pipePointWaitCount = 1;
    barrier.pPipePoints        = &postCs;

    Pal::BarrierTransition transition = {};
    transition.srcCacheMask = Pal::CacheCoherencyUsageFlags::CoherShader;
    transition.dstCacheMask = Pal::CacheCoherencyUsageFlags::CoherShader;

    barrier.transitionCount = 1;
    barrier.pTransitions    = &transition;
    barrier.reason          = Pal::Developer::BarrierReasonMlaaResolveEdgeSync;

    Pal::uint32 iterationDepth = 0;

    // 2nd stage: Calculate the separating edge length
    if (m_fastPath)
    {
        // Issue a barrier to ensure the previous stage is complete
        pCmdBuffer->CmdBarrier(barrier);

        CalcSepEdgeLengthFast(pCmdBuffer);
    }
    else
    {
        // Looking for a max blend size of 128
        for (Pal::uint32 size = 128; size > 0; size >>= 1)
        {
            // Issue a barrier to ensure the previous stage is complete
            pCmdBuffer->CmdBarrier(barrier);

            CalcSepEdgeLength(pCmdBuffer, iterationDepth);

            iterationDepth++;
        }
    }

    // Issue a barrier to ensure the previous stage is complete
    pCmdBuffer->CmdBarrier(barrier);

    // Final blend stage
    if (m_fastPath)
    {
        FinalBlendFast(pCmdBuffer, srcImage, srcSubres, dstImage, dstSubres);
    }
    else
    {
        FinalBlend(pCmdBuffer, srcImage, srcSubres, dstImage, dstSubres, iterationDepth - 1);
    }
}

} // GpuUtil
