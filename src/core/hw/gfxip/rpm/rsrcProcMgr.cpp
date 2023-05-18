/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/cmdStream.h"
#include "core/platform.h"
#include "g_platformSettings.h"
#include "core/hw/gfxip/colorBlendState.h"
#include "core/hw/gfxip/computePipeline.h"
#include "core/hw/gfxip/depthStencilState.h"
#include "core/hw/gfxip/graphicsPipeline.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfxCmdStream.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/msaaState.h"
#include "core/hw/gfxip/rpm/rpmUtil.h"
#include "core/hw/gfxip/rpm/rsrcProcMgr.h"
#include "core/hw/gfxip/pm4UniversalCmdBuffer.h"
#include "palAutoBuffer.h"
#include "palColorBlendState.h"
#include "palColorTargetView.h"
#include "palDepthStencilState.h"
#include "palDepthStencilView.h"
#include "palFormatInfo.h"
#include "palMsaaState.h"
#include "palInlineFuncs.h"
#include "palLiterals.h"
#include "palGpuMemory.h"

#include <float.h>
#include <math.h>

using namespace Util;
using namespace Util::Literals;

namespace Pal
{

// =====================================================================================================================
// Note that this constructor is invoked before settings have been committed.
RsrcProcMgr::RsrcProcMgr(
    GfxDevice* pDevice)
    :
    m_pBlendDisableState(nullptr),
    m_pColorBlendState(nullptr),
    m_pDepthDisableState(nullptr),
    m_pDepthClearState(nullptr),
    m_pStencilClearState(nullptr),
    m_pDepthStencilClearState(nullptr),
    m_pDepthExpandState(nullptr),
    m_pDepthResummarizeState(nullptr),
    m_pDepthResolveState(nullptr),
    m_pStencilResolveState(nullptr),
    m_pDepthStencilResolveState(nullptr),
    m_pDevice(pDevice),
    m_srdAlignment(0)
{
    memset(&m_pMsaaState[0], 0, sizeof(m_pMsaaState));
    memset(&m_pComputePipelines[0], 0, sizeof(m_pComputePipelines));
    memset(&m_pGraphicsPipelines[0], 0, sizeof(m_pGraphicsPipelines));
}

// =====================================================================================================================
RsrcProcMgr::~RsrcProcMgr()
{
    // These objects must be destroyed in Cleanup().
    for (uint32 idx = 0; idx < static_cast<uint32>(RpmComputePipeline::Count); ++idx)
    {
        PAL_ASSERT(m_pComputePipelines[idx] == nullptr);
    }

    for (uint32 idx = 0; idx < RpmGfxPipelineCount; ++idx)
    {
        PAL_ASSERT(m_pGraphicsPipelines[idx] == nullptr);
    }

    for (uint32 sampleIdx = 0; sampleIdx <= MaxLog2AaSamples; ++sampleIdx)
    {
        for (uint32 fragmentIdx = 0; fragmentIdx <= MaxLog2AaFragments; ++fragmentIdx)
        {
            PAL_ASSERT(m_pMsaaState[sampleIdx][fragmentIdx] == nullptr);
        }
    }

    PAL_ASSERT(m_pBlendDisableState      == nullptr);
    PAL_ASSERT(m_pColorBlendState        == nullptr);
    PAL_ASSERT(m_pDepthDisableState      == nullptr);
    PAL_ASSERT(m_pDepthClearState        == nullptr);
    PAL_ASSERT(m_pStencilClearState      == nullptr);
    PAL_ASSERT(m_pDepthStencilClearState == nullptr);
    PAL_ASSERT(m_pDepthExpandState       == nullptr);
    PAL_ASSERT(m_pDepthResummarizeState  == nullptr);
    PAL_ASSERT(m_pDepthResolveState      == nullptr);
    PAL_ASSERT(m_pStencilResolveState    == nullptr);
}

// =====================================================================================================================
// This must clean up all internal GPU memory allocations and all objects created after EarlyInit. Note that EarlyInit
// is called when the platform creates the device objects so the work it does must be preserved if we are to reuse
// this object.
void RsrcProcMgr::Cleanup()
{
    // Destroy all compute pipeline objects.
    for (uint32 idx = 0; idx < static_cast<uint32>(RpmComputePipeline::Count); ++idx)
    {
        if (m_pComputePipelines[idx] != nullptr)
        {
            m_pComputePipelines[idx]->DestroyInternal();
            m_pComputePipelines[idx] = nullptr;
        }
    }

    // Destroy all graphics pipeline objects.
    for (uint32 idx = 0; idx < RpmGfxPipelineCount; ++idx)
    {
        if (m_pGraphicsPipelines[idx] != nullptr)
        {
            m_pGraphicsPipelines[idx]->DestroyInternal();
            m_pGraphicsPipelines[idx] = nullptr;
        }
    }

    m_pDevice->DestroyColorBlendStateInternal(m_pBlendDisableState);
    m_pBlendDisableState = nullptr;

    m_pDevice->DestroyColorBlendStateInternal(m_pColorBlendState);
    m_pColorBlendState = nullptr;

    DepthStencilState**const ppDepthStates[] =
    {
        &m_pDepthDisableState,
        &m_pDepthClearState,
        &m_pStencilClearState,
        &m_pDepthStencilClearState,
        &m_pDepthExpandState,
        &m_pDepthResummarizeState,
        &m_pDepthResolveState,
        &m_pStencilResolveState,
        &m_pDepthStencilResolveState,
    };

    for (uint32 idx = 0; idx < ArrayLen(ppDepthStates); ++idx)
    {
        m_pDevice->DestroyDepthStencilStateInternal(*ppDepthStates[idx]);
        (*ppDepthStates[idx]) = nullptr;
    }

    for (uint32 sampleIdx = 0; sampleIdx <= MaxLog2AaSamples; ++sampleIdx)
    {
        for (uint32 fragmentIdx = 0; fragmentIdx <= MaxLog2AaFragments; ++fragmentIdx)
        {
            m_pDevice->DestroyMsaaStateInternal(m_pMsaaState[sampleIdx][fragmentIdx]);
            m_pMsaaState[sampleIdx][fragmentIdx] = nullptr;
        }
    }
}

// =====================================================================================================================
// Performs early initialization of this object; this occurs when the device owning is created.
Result RsrcProcMgr::EarlyInit()
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    m_srdAlignment = Max(chipProps.srdSizes.bufferView,
                         chipProps.srdSizes.fmaskView,
                         chipProps.srdSizes.imageView,
                         chipProps.srdSizes.sampler);

    // Round up to the size of a DWORD.
    m_srdAlignment = Util::NumBytesToNumDwords(m_srdAlignment);

    return Result::Success;
}

// =====================================================================================================================
// Performs any late-stage initialization that can only be done after settings have been committed.
Result RsrcProcMgr::LateInit()
{
    Result result = Result::Success;

    if (m_pDevice->Parent()->GetPublicSettings()->disableResourceProcessingManager == false)
    {
        result = CreateRpmComputePipelines(m_pDevice, m_pComputePipelines);

        if (result == Result::Success)
        {
            result = CreateRpmGraphicsPipelines(m_pDevice, m_pGraphicsPipelines);
        }

        if (result == Result::Success)
        {
            result = CreateCommonStateObjects();
        }

    }

    return result;
}

// =====================================================================================================================
// Builds commands to copy one or more regions from one GPU memory location to another with a compute shader.
void RsrcProcMgr::CopyMemoryCs(
    GfxCmdBuffer*           pCmdBuffer,
    const GpuMemory&        srcGpuMemory,
    const GpuMemory&        dstGpuMemory,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions
    ) const
{
    // Local to local copy prefers wide format copy for better performance. Copy to/from nonlocal heap
    // with wide format may result in worse performance.
    const bool preferWideFormatCopy = (srcGpuMemory.IsLocalPreferred() && dstGpuMemory.IsLocalPreferred());

    CopyMemoryCs(pCmdBuffer,
                 srcGpuMemory.Desc().gpuVirtAddr,
                 *srcGpuMemory.GetDevice(),
                 dstGpuMemory.Desc().gpuVirtAddr,
                 *dstGpuMemory.GetDevice(),
                 regionCount,
                 pRegions,
                 preferWideFormatCopy,
                 nullptr);
}

// =====================================================================================================================
// Builds commands to copy one or more regions from one GPU memory location to another with a compute shader.
void RsrcProcMgr::CopyMemoryCs(
    GfxCmdBuffer*           pCmdBuffer,
    gpusize                 srcGpuVirtAddr,
    const Device&           srcDevice,
    gpusize                 dstGpuVirtAddr,
    const Device&           dstDevice,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions,
    bool                    preferWideFormatCopy,
    const gpusize*          pP2pBltInfoChunks
    ) const
{
    constexpr uint32  NumGpuMemory  = 2;        // source & destination.
    constexpr gpusize CopySizeLimit = 16777216; // 16 MB.

    // Save current command buffer state.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    // Now begin processing the list of copy regions.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        if (pP2pBltInfoChunks != nullptr)
        {
            pCmdBuffer->P2pBltWaCopyNextRegion(pP2pBltInfoChunks[idx]);
        }

        const gpusize srcOffset  = pRegions[idx].srcOffset;
        const gpusize dstOffset  = pRegions[idx].dstOffset;
        const gpusize copySize   = pRegions[idx].copySize;

        for(gpusize copyOffset = 0; copyOffset < copySize; copyOffset += CopySizeLimit)
        {
            const uint32 copySectionSize = static_cast<uint32>(Min(CopySizeLimit, copySize - copyOffset));

            // Get the pipeline object and number of thread groups.
            const ComputePipeline* pPipeline = nullptr;
            uint32 numThreadGroups           = 0;

            constexpr uint32 DqwordSize = 4 * sizeof(uint32);
            if (preferWideFormatCopy &&
                IsPow2Aligned(srcOffset + copyOffset, DqwordSize) &&
                IsPow2Aligned(dstOffset + copyOffset, DqwordSize) &&
                IsPow2Aligned(copySectionSize, DqwordSize))
            {
                // Offsets and copySectionSize are DQWORD aligned so we can use the DQWORD copy pipeline.
                pPipeline       = GetPipeline(RpmComputePipeline::CopyBufferDqword);
                numThreadGroups = RpmUtil::MinThreadGroups(copySectionSize / DqwordSize,
                                                           pPipeline->ThreadsPerGroup());
            }
            else if (IsPow2Aligned(srcOffset + copyOffset, sizeof(uint32)) &&
                     IsPow2Aligned(dstOffset + copyOffset, sizeof(uint32)) &&
                     IsPow2Aligned(copySectionSize, sizeof(uint32)))
            {
                // Offsets and copySectionSize are DWORD aligned so we can use the DWORD copy pipeline.
                pPipeline       = GetPipeline(RpmComputePipeline::CopyBufferDword);
                numThreadGroups = RpmUtil::MinThreadGroups(copySectionSize / sizeof(uint32),
                                                           pPipeline->ThreadsPerGroup());
            }
            else
            {
                // Offsets and copySectionSize are not all DWORD aligned so we have to use the byte copy pipeline.
                pPipeline       = GetPipeline(RpmComputePipeline::CopyBufferByte);
                numThreadGroups = RpmUtil::MinThreadGroups(copySectionSize, pPipeline->ThreadsPerGroup());
            }

            pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

            // Create an embedded user-data table and bind it to user data. We need buffer views for the source and
            // destination.
            uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                       SrdDwordAlignment() * NumGpuMemory,
                                                                       SrdDwordAlignment(),
                                                                       PipelineBindPoint::Compute,
                                                                       0);

            // Populate the table with raw buffer views, by convention the destination is placed before the source.
            BufferViewInfo rawBufferView = {};
            RpmUtil::BuildRawBufferViewInfo(&rawBufferView,
                                            dstDevice,
                                            (dstGpuVirtAddr + dstOffset + copyOffset),
                                            copySectionSize);
            m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &rawBufferView, pSrdTable);
            pSrdTable += SrdDwordAlignment();

            RpmUtil::BuildRawBufferViewInfo(&rawBufferView,
                                            srcDevice,
                                            (srcGpuVirtAddr + srcOffset + copyOffset),
                                            copySectionSize);
            m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &rawBufferView, pSrdTable);

            const uint32 regionUserData[3] = { 0, 0, copySectionSize };
            pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 1, 3, regionUserData);
            pCmdBuffer->CmdDispatch({numThreadGroups, 1, 1});
        }
    }

    if (pP2pBltInfoChunks != nullptr)
    {
        pCmdBuffer->P2pBltWaCopyEnd();
    }

    // Restore command buffer state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Builds commands to copy one or more regions from one image to another.
void RsrcProcMgr::CmdCopyImage(
    GfxCmdBuffer*          pCmdBuffer,
    const Image&           srcImage,
    ImageLayout            srcImageLayout,
    const Image&           dstImage,
    ImageLayout            dstImageLayout,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    const Rect*            pScissorRect,
    uint32                 flags
    ) const
{
    // MSAA source and destination images must have the same number of fragments.
    PAL_ASSERT(srcImage.GetImageCreateInfo().fragments == dstImage.GetImageCreateInfo().fragments);

    CopyImageCompute(pCmdBuffer,
                     srcImage,
                     srcImageLayout,
                     dstImage,
                     dstImageLayout,
                     regionCount,
                     pRegions,
                     flags);
}

// =====================================================================================================================
void RsrcProcMgr::CopyImageCs(
    GfxCmdBuffer*          pCmdBuffer,
    const CopyImageCsInfo& copyImageCsInfo
    ) const
{
    const Device&          device        = *m_pDevice->Parent();
    const ImageCreateInfo& dstCreateInfo = copyImageCsInfo.pDstImage->GetImageCreateInfo();
    const ImageCreateInfo& srcCreateInfo = copyImageCsInfo.pSrcImage->GetImageCreateInfo();
    const ImageType        imageType     = copyImageCsInfo.pSrcImage->GetGfxImage()->GetOverrideImageType();
    const bool             viewMatchDim  = (IsGfx8(device) || IsGfx9(device));

    // If the destination format is srgb and we will be doing format conversion copy then we need the shader to
    // perform gamma correction. Note: If both src and dst are srgb then we'll do a raw copy and so no need to change
    // pipelines in that case.
    const bool isSrgbDst = (TestAnyFlagSet(copyImageCsInfo.flags, CopyFormatConversion) &&
                            Formats::IsSrgb(dstCreateInfo.swizzledFormat.format)        &&
                            (Formats::IsSrgb(srcCreateInfo.swizzledFormat.format) == false));

    // Save current command buffer state and bind the pipeline.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, copyImageCsInfo.pPipeline, InternalApiPsoHash, });

    // Now begin processing the list of copy regions.
    for (uint32 idx = 0; idx < copyImageCsInfo.regionCount; ++idx)
    {
        ImageCopyRegion copyRegion = copyImageCsInfo.pRegions[idx];

        // When copying from 3D to 3D, the number of slices should be 1. When copying from
        // 1D to 1D or 2D to 2D, depth should be 1. Therefore when the src image type is identical
        // to the dst image type, either the depth or the number of slices should be equal to 1.
        PAL_ASSERT((srcCreateInfo.imageType != dstCreateInfo.imageType) ||
                   (copyRegion.numSlices == 1) ||
                   (copyRegion.extent.depth == 1));

#if PAL_ENABLE_PRINTS_ASSERTS
        // When copying from 2D to 3D or 3D to 2D, the number of slices should match the depth.
        if (((srcCreateInfo.imageType == ImageType::Tex3d) && (dstCreateInfo.imageType == ImageType::Tex2d)) ||
            ((srcCreateInfo.imageType == ImageType::Tex2d) && (dstCreateInfo.imageType == ImageType::Tex3d)))
        {
            PAL_ASSERT(copyRegion.numSlices == copyRegion.extent.depth);
        }
#endif

        if (copyImageCsInfo.pP2pBltInfoChunks != nullptr)
        {
            pCmdBuffer->P2pBltWaCopyNextRegion(copyImageCsInfo.pP2pBltInfoChunks[idx]);
        }

        // Setup image formats per-region. This is different than the graphics path because the compute path must be
        // able to copy depth-stencil images.
        SwizzledFormat dstFormat    = {};
        SwizzledFormat srcFormat    = {};
        uint32         texelScale   = 1;
        bool           singleSubres = false;

        GetCopyImageFormats(*copyImageCsInfo.pSrcImage,
                            copyImageCsInfo.srcImageLayout,
                            *copyImageCsInfo.pDstImage,
                            copyImageCsInfo.dstImageLayout,
                            copyRegion,
                            copyImageCsInfo.flags,
                            &srcFormat,
                            &dstFormat,
                            &texelScale,
                            &singleSubres);

        // The hardware can't handle UAV stores using SRGB num format.  The resolve shaders already contain a
        // linear-to-gamma conversion, but in order for that to work the output UAV's num format must be patched to be
        // simple unorm.
        if (Formats::IsSrgb(dstFormat.format))
        {
            dstFormat.format = Formats::ConvertToUnorm(dstFormat.format);
            PAL_ASSERT(Formats::IsUndefined(dstFormat.format) == false);
        }

        // Multiply all x-dimension values in our region by the texel scale.
        copyRegion.srcOffset.x  *= texelScale;
        copyRegion.dstOffset.x  *= texelScale;
        copyRegion.extent.width *= texelScale;

        // Create an embedded user-data table and bind it to user data 0. We need image views for the src and dst
        // subresources, as well as some inline constants for the copy offsets and extents.
        const uint32 numSlots = copyImageCsInfo.isFmaskCopy ? 3 : 2;
        uint32* pUserData = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   SrdDwordAlignment() * numSlots,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        // When we treat 3D images as 2D arrays each z-slice must be treated as an array slice.
        const uint32 numSlices = (imageType == ImageType::Tex3d) ? copyRegion.extent.depth : copyRegion.numSlices;

        ImageViewInfo imageView[2] = {};
        SubresRange   viewRange    = { copyRegion.dstSubres, 1, 1, numSlices };

        PAL_ASSERT(TestAnyFlagSet(copyImageCsInfo.dstImageLayout.usages, LayoutCopyDst));
        RpmUtil::BuildImageViewInfo(&imageView[0],
                                    *copyImageCsInfo.pDstImage,
                                    viewRange,
                                    dstFormat,
                                    copyImageCsInfo.dstImageLayout,
                                    device.TexOptLevel(),
                                    true);

        viewRange.startSubres = copyRegion.srcSubres;
        RpmUtil::BuildImageViewInfo(&imageView[1],
                                    *copyImageCsInfo.pSrcImage,
                                    viewRange,
                                    srcFormat,
                                    copyImageCsInfo.srcImageLayout,
                                    device.TexOptLevel(),
                                    false);

        // Image view type matters for HW addrlib. Only override if absolutely necessary.
        // GFX10+: Copying behavior depends on instruction DIM, not image view type
        //    See GetCopyImageComputePipeline for more info on DIM
        // GFX8,9: The original comment asserts that overriding the image view type to 2D is necessary.
        //    "The shader treats all images as 2D arrays which means we need to override the view type to 2D.
        //     We also used to do this for 3D images but that caused test failures when the images used mipmaps
        //     because the HW expected "numSlices" to be constant for all mip levels
        //     (rather than halving at each mip as z-slices do)."
        if (viewMatchDim && (imageType == ImageType::Tex1d))
        {
            imageView[0].viewType = ImageViewType::Tex2d;
            imageView[1].viewType = ImageViewType::Tex2d;
        }

        if (copyImageCsInfo.useMipInSrd == false)
        {
            // The miplevel as specified in the shader instruction is actually an offset from the mip-level
            // as specified in the SRD.
            imageView[0].subresRange.startSubres.mipLevel = 0;  // dst
            imageView[1].subresRange.startSubres.mipLevel = 0;  // src

            // The mip-level from the instruction is also clamped to the "last level" as specified in the SRD.
            imageView[0].subresRange.numMips = copyRegion.dstSubres.mipLevel + viewRange.numMips;
            imageView[1].subresRange.numMips = copyRegion.srcSubres.mipLevel + viewRange.numMips;
        }

        PAL_ASSERT(singleSubres == false);

        // Turn our image views into HW SRDs here
        device.CreateImageViewSrds(2, &imageView[0], pUserData);
        pUserData += SrdDwordAlignment() * 2;

        if (copyImageCsInfo.isFmaskCopy)
        {
            // If this is an Fmask-accelerated Copy, create an image view of the source Image's Fmask surface.
            FmaskViewInfo fmaskView = {};
            fmaskView.pImage         = copyImageCsInfo.pSrcImage;
            fmaskView.baseArraySlice = copyRegion.srcSubres.arraySlice;
            fmaskView.arraySize      = copyRegion.numSlices;

            m_pDevice->Parent()->CreateFmaskViewSrds(1, &fmaskView, pUserData);
        }

        // Embed the constant buffer in the remaining fast user-data entries.
        union
        {
            RpmUtil::CopyImageInfo copyImageInfo;
            uint32                 userData[RpmUtil::CopyImageInfoDwords];
        } cb;

        cb.copyImageInfo.srcOffset                 = copyRegion.srcOffset;
        cb.copyImageInfo.dstOffset                 = copyRegion.dstOffset;
        cb.copyImageInfo.numSamples                = dstCreateInfo.samples;
        cb.copyImageInfo.packedMipData.srcMipLevel = copyRegion.srcSubres.mipLevel;
        cb.copyImageInfo.packedMipData.dstMipLevel = copyRegion.dstSubres.mipLevel;
        cb.copyImageInfo.copyRegion.width          = copyRegion.extent.width;
        cb.copyImageInfo.copyRegion.height         = copyRegion.extent.height;
        cb.copyImageInfo.dstIsSrgb                 = isSrgbDst;

        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 1, RpmUtil::CopyImageInfoDwords, cb.userData);

        // Execute the dispatch. All of our copyImage shaders split the copy window into 8x8x1-texel tiles.
        // Most of them simply define their threadgroup as an 8x8x1 grid and assign one texel to each thread.
        // Some more advanced shaders use abstract threadgroup layouts which do not map one thread to one texel.
        constexpr DispatchDims texelsPerGroup = {8, 8, 1};
        const     DispatchDims texels = {copyRegion.extent.width, copyRegion.extent.height, numSlices};

        pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroupsXyz(texels, texelsPerGroup));
    }

    if (copyImageCsInfo.pP2pBltInfoChunks != nullptr)
    {
        pCmdBuffer->P2pBltWaCopyEnd();
    }

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);

    if (copyImageCsInfo.isFmaskCopyOptimized || (dstCreateInfo.flags.fullCopyDstOnly != 0))
    {
        // If this is MSAA copy optimized we might have to update destination image meta data.
        // If image is created with fullCopyDstOnly=1, there will be no expand when transition to "LayoutCopyDst"; if
        // the copy isn't compressed copy, need fix up dst metadata to uncompressed state.
        const Pal::Image* pSrcImage = copyImageCsInfo.isFmaskCopyOptimized ? copyImageCsInfo.pSrcImage : nullptr;
        AutoBuffer<ImageFixupRegion, 32, Platform> fixupRegions(copyImageCsInfo.regionCount, m_pDevice->GetPlatform());

        if (fixupRegions.Capacity() >= copyImageCsInfo.regionCount)
        {
            for (uint32 i = 0; i < copyImageCsInfo.regionCount; i++)
            {
                fixupRegions[i].subres    = copyImageCsInfo.pRegions[i].dstSubres;
                fixupRegions[i].offset    = copyImageCsInfo.pRegions[i].dstOffset;
                fixupRegions[i].extent    = copyImageCsInfo.pRegions[i].extent;
                fixupRegions[i].numSlices = copyImageCsInfo.pRegions[i].numSlices;
            }
            HwlFixupCopyDstImageMetaData(pCmdBuffer,
                                         pSrcImage,
                                         *copyImageCsInfo.pDstImage,
                                         copyImageCsInfo.dstImageLayout,
                                         &fixupRegions[0],
                                         copyImageCsInfo.regionCount,
                                         copyImageCsInfo.isFmaskCopyOptimized);
        }
        else
        {
            pCmdBuffer->NotifyAllocFailure();
        }
    }
}

// =====================================================================================================================
const ComputePipeline* RsrcProcMgr::GetCopyImageComputePipeline(
    const Image&           srcImage,
    ImageLayout            srcImageLayout,
    const Image&           dstImage,
    ImageLayout            dstImageLayout,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    uint32                 flags,
    bool                   useMipInSrd,
    bool*                  pIsFmaskCopy,
    bool*                  pIsFmaskCopyOptimized
    ) const
{
    const auto&     dstCreateInfo = dstImage.GetImageCreateInfo();
    const auto&     srcCreateInfo = srcImage.GetImageCreateInfo();
    const GfxImage* pSrcGfxImage  = srcImage.GetGfxImage();
    const bool      isEqaaSrc     = (srcCreateInfo.samples != srcCreateInfo.fragments);

    // Get the appropriate pipeline object.
    RpmComputePipeline pipeline   = RpmComputePipeline::Count;
    bool pipelineHasSrgbCoversion = false;

    if (pSrcGfxImage->HasFmaskData())
    {
        // MSAA copies that use FMask.
        PAL_ASSERT(srcCreateInfo.fragments > 1);
        PAL_ASSERT((srcImage.IsDepthStencilTarget() == false) && (dstImage.IsDepthStencilTarget() == false));

        // Optimized image copies require a call to HwlFixupCopyDstImageMetaData...
        // Verify that any "update" operation performed is legal for the source and dest images.
        if (HwlUseOptimizedImageCopy(srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions))
        {
            pipeline = RpmComputePipeline::MsaaFmaskCopyImageOptimized;
            *pIsFmaskCopyOptimized = true;
        }
        else
        {
            if (isEqaaSrc)
            {
                // The normal (non-optimized) Image Copy path does not support EQAA.
                // It would require a separate fixup pass on the Fmask surface.
                // This has not been implemented yet, but can be if required later.
                PAL_NOT_IMPLEMENTED();
            }

            pipeline = RpmComputePipeline::MsaaFmaskCopyImage;
        }

        *pIsFmaskCopy = true;
    }
    else if (srcCreateInfo.fragments > 1)
    {
        // MSAA copies that don't use FMask.
        //
        // We have two different copy algorithms which read and write the fragments of an 8x8 pixel tile in different
        // orders. The simple one assigns each thread to a single pixel and iterates over the fragment index; this
        // works well if the image treats the fragment index like a slice index and stores samples in planes.
        // The more complex Morton/Z order algorithm assigns sequential threads to sequential fragment indices and
        // walks the memory requests around the 8x8 pixel tile in Morton/Z order; this works well if the image stores
        // each pixel's samples sequentially in memory (and also stores tiles in Morton/Z order).
        const bool useMorton = CopyImageCsUseMsaaMorton(dstImage);

        // The Morton shaders have built-in support for SRGB conversions.
        pipelineHasSrgbCoversion = useMorton;

        switch (srcCreateInfo.fragments)
        {
        case 2:
            pipeline = useMorton ? RpmComputePipeline::CopyImage2dMorton2x : RpmComputePipeline::CopyImage2dms2x;
            break;

        case 4:
            pipeline = useMorton ? RpmComputePipeline::CopyImage2dMorton4x : RpmComputePipeline::CopyImage2dms4x;
            break;

        case 8:
            pipeline = useMorton ? RpmComputePipeline::CopyImage2dMorton8x : RpmComputePipeline::CopyImage2dms8x;
            break;

        default:
            PAL_ASSERT_ALWAYS();
            break;
        };
    }
    else if (useMipInSrd)
    {
        // GFX10+: The types declared in the IL source are encoded into the DIM field of the instructions.
        //    DIM determines the max number of texture parameters [S,R,T,Q] to allocate.
        //    TA ignores unused parameters for a resource if the image view defines them as size 1.
        //    [S,R,T] can be generalized (3D, 2D array) for non-sampler operations like copies.
        //        [Q] TA's interpretation of Q depends on DIM. MIP unless DIM is MSAA
        //    Image Copies with a Q component need their own copy shaders.
        //    Simpler copies (non-msaa, non-mip) can all share a single 3-dimensional (2d array) copy shader.
        pipeline = RpmComputePipeline::CopyImage2d;
    }
    else
    {
        pipeline = RpmComputePipeline::CopyImage2dShaderMipLevel;
    }

    // If the destination format is srgb and we will be doing format conversion copy then we need to use the pipeline
    // that will properly perform gamma correction. Note: If both src and dst are srgb then we'll do a raw copy and so
    // no need to change pipelines in that case.
    const bool needSrgbConversion = (TestAnyFlagSet(flags, CopyFormatConversion) &&
                                     Formats::IsSrgb(dstCreateInfo.swizzledFormat.format) &&
                                     (Formats::IsSrgb(srcCreateInfo.swizzledFormat.format) == false));

    if (needSrgbConversion && (pipelineHasSrgbCoversion == false))
    {
        pipeline = RpmComputePipeline::CopyImageGammaCorrect2d;

        // We need to clear these out just in case we went down the FMask path above. This fallback shader has no
        // FMask acceleration support so we need to fully decompress/expand the color information.
        *pIsFmaskCopy          = false;
        *pIsFmaskCopyOptimized = false;
    }

    return GetPipeline(pipeline);
}

// =====================================================================================================================
// Gives the hardare layers some influence over GetCopyImageComputePipeline.
bool RsrcProcMgr::CopyImageCsUseMsaaMorton(
    const Image& dstImage
    ) const
{
    // Our HW has stored depth/stencil samples sequentially for many generations and gfx10+ explicitly stores pixels
    // within a micro-tile in Morton/Z order. The Morton shaders were written with gfx10 in mind but performance
    // profiling showed they help on all GPUs. This makes sense as reading and writing samples sequentially is the
    // primary benefit to using the Morton path over the old path (Morton is just a snazzier name than Sequential).
    return dstImage.IsDepthStencilTarget();
}

// =====================================================================================================================
// Builds commands to copy one or more regions from one image to another using a compute shader.
// The caller should assert that the source and destination images have the same image types and sample counts.
void RsrcProcMgr::CopyImageCompute(
    GfxCmdBuffer*          pCmdBuffer,
    const Image&           srcImage,
    ImageLayout            srcImageLayout,
    const Image&           dstImage,
    ImageLayout            dstImageLayout,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    uint32                 flags
    ) const
{
    PAL_ASSERT(TestAnyFlagSet(flags, CopyEnableScissorTest) == false);

    const bool  isCompressed  = (Formats::IsBlockCompressed(srcImage.GetImageCreateInfo().swizzledFormat.format) ||
                                 Formats::IsBlockCompressed(dstImage.GetImageCreateInfo().swizzledFormat.format));
    const bool  useMipInSrd   = CopyImageUseMipLevelInSrd(isCompressed);

    bool isFmaskCopy          = false;
    bool isFmaskCopyOptimized = false;

    // Get the appropriate pipeline object.
    const ComputePipeline* pPipeline = GetCopyImageComputePipeline(srcImage,
                                                                   srcImageLayout,
                                                                   dstImage,
                                                                   dstImageLayout,
                                                                   regionCount,
                                                                   pRegions,
                                                                   flags,
                                                                   useMipInSrd,
                                                                   &isFmaskCopy,
                                                                   &isFmaskCopyOptimized);

    CopyImageCsInfo copyImageCsInfo;
    copyImageCsInfo.pPipeline            = pPipeline;
    copyImageCsInfo.pSrcImage            = &srcImage;
    copyImageCsInfo.srcImageLayout       = srcImageLayout;
    copyImageCsInfo.pDstImage            = &dstImage;
    copyImageCsInfo.dstImageLayout       = dstImageLayout;
    copyImageCsInfo.regionCount          = regionCount;
    copyImageCsInfo.pRegions             = pRegions;
    copyImageCsInfo.flags                = flags;
    copyImageCsInfo.isFmaskCopy          = isFmaskCopy;
    copyImageCsInfo.isFmaskCopyOptimized = isFmaskCopyOptimized;
    copyImageCsInfo.useMipInSrd          = useMipInSrd;
    copyImageCsInfo.pP2pBltInfoChunks    = nullptr;

    CopyImageCs(pCmdBuffer, copyImageCsInfo);
}

// =====================================================================================================================
// Picks a source format and a destination format for an image-to-image copy.
template <typename CopyRegion>
void RsrcProcMgr::GetCopyImageFormats(
    const Image&      srcImage,
    ImageLayout       srcImageLayout,
    const Image&      dstImage,
    ImageLayout       dstImageLayout,
    const CopyRegion& copyRegion,
    uint32            copyFlags,
    SwizzledFormat*   pSrcFormat,     // [out] Read from the source image using this format.
    SwizzledFormat*   pDstFormat,     // [out] Read from the destination image using this format.
    uint32*           pTexelScale,    // [out] Each texel requires this many raw format texels in the X dimension.
    bool*             pSingleSubres   // [out] Format requires that you access each subres independantly.
    ) const
{
    const auto& device        = *m_pDevice->Parent();
    const auto& srcCreateInfo = srcImage.GetImageCreateInfo();
    const auto& dstCreateInfo = dstImage.GetImageCreateInfo();

    // Begin with each subresource's native format.
    SwizzledFormat srcFormat = srcImage.SubresourceInfo(copyRegion.srcSubres)->format;
    SwizzledFormat dstFormat = dstImage.SubresourceInfo(copyRegion.dstSubres)->format;

    const bool isSrcFormatReplaceable = srcImage.GetGfxImage()->IsFormatReplaceable(copyRegion.srcSubres,
                                                                                    srcImageLayout,
                                                                                    false);
    const bool isDstFormatReplaceable = dstImage.GetGfxImage()->IsFormatReplaceable(copyRegion.dstSubres,
                                                                                    dstImageLayout,
                                                                                    true);

    const bool isDccFormatEncodingMatch = m_pDevice->ComputeDccFormatEncoding(srcFormat,
                                                                              &dstFormat,
                                                                              1) == DccFormatEncoding::Optimal;

    const bool chFmtsMatch    = Formats::ShareChFmt(srcFormat.format, dstFormat.format);
    const bool formatsMatch   = (srcFormat.format == dstFormat.format) &&
                                (srcFormat.swizzle.swizzleValue == dstFormat.swizzle.swizzleValue);
    const bool isMmFormatUsed = (Formats::IsMmFormat(srcFormat.format) || Formats::IsMmFormat(dstFormat.format));

    // Both formats must have the same pixel size.
    PAL_ASSERT(Formats::BitsPerPixel(srcFormat.format) == Formats::BitsPerPixel(dstFormat.format));

    // Initialize the texel scale to 1, it will be modified later if necessary.
    *pTexelScale = 1;

    // First, determine if we must follow conversion copy rules.
    if (TestAnyFlagSet(copyFlags, CopyFormatConversion)                            &&
        device.SupportsFormatConversionSrc(srcFormat.format, srcCreateInfo.tiling) &&
        device.SupportsFormatConversionDst(dstFormat.format, dstCreateInfo.tiling))
    {
        // Eventhough we're supposed to do a conversion copy, it will be faster if we can get away with a raw copy.
        // It will be safe to do a raw copy if the formats match and the target subresources support format replacement.
        if (formatsMatch && isSrcFormatReplaceable && isDstFormatReplaceable)
        {
            srcFormat = RpmUtil::GetRawFormat(srcFormat.format, pTexelScale, pSingleSubres);
            dstFormat = srcFormat;
        }
    }
    else
    {
        // We will be doing some sort of raw copy.
        //
        // Our copy shaders and hardware treat sRGB and UNORM nearly identically, the only difference being that the
        // hardware modifies sRGB data when reading it and can't write it, which will make it hard to do a raw copy.
        // We can avoid that problem by simply forcing sRGB to UNORM.
        if (Formats::IsSrgb(srcFormat.format))
        {
            srcFormat.format = Formats::ConvertToUnorm(srcFormat.format);
        }

        if (Formats::IsSrgb(dstFormat.format))
        {
            dstFormat.format = Formats::ConvertToUnorm(dstFormat.format);
        }

        // Due to hardware-specific compression modes, some image subresources might not support format replacement.
        // Note that the code above can force sRGB to UNORM even if format replacement is not supported because sRGB
        // values use the same bit representation as UNORM values, they just use a different color space.
        if (isSrcFormatReplaceable && isDstFormatReplaceable)
        {
            // We should do a raw copy that respects channel swizzling if the flag is set and the channel formats
            // don't match. The process is simple: keep the channel formats and try to find a single numeric format
            // that fits both of them.
            bool foundSwizzleFormats = false;

            if (TestAnyFlagSet(copyFlags, CopyRawSwizzle) && (chFmtsMatch == false))
            {
                typedef ChNumFormat(PAL_STDCALL *FormatConversion)(ChNumFormat);

                constexpr uint32 NumNumericFormats = 3;
                constexpr FormatConversion FormatConversionFuncs[NumNumericFormats] =
                {
                    &Formats::ConvertToUint,
                    &Formats::ConvertToUnorm,
                    &Formats::ConvertToFloat,
                };

                for (uint32 idx = 0; idx < NumNumericFormats; ++idx)
                {
                    ChNumFormat tempSrcFmt = srcFormat.format;
                    ChNumFormat tempDstFmt = dstFormat.format;

                    tempSrcFmt = FormatConversionFuncs[idx](tempSrcFmt);
                    tempDstFmt = FormatConversionFuncs[idx](tempDstFmt);

                    if ((Formats::IsUndefined(tempSrcFmt) == false)           &&
                        (Formats::IsUndefined(tempDstFmt) == false)           &&
                        device.SupportsCopy(tempSrcFmt, srcCreateInfo.tiling) &&
                        device.SupportsCopy(tempDstFmt, dstCreateInfo.tiling))
                    {
                        foundSwizzleFormats = true;
                        srcFormat.format    = tempSrcFmt;
                        dstFormat.format    = tempDstFmt;
                        break;
                    }
                }
            }

            // If we either didn't try to find swizzling formats or weren't able to do so, execute a true raw copy.
            if (foundSwizzleFormats == false)
            {
                srcFormat = RpmUtil::GetRawFormat(srcFormat.format, pTexelScale, pSingleSubres);
                dstFormat = srcFormat;
            }
        }
        // If one format is deemed "not replaceable" that means it may possibly be compressed. However,
        // if it is compressed, it doesn't necessarily mean it's not replaceable. If we don't do a replacement,
        // copying from one format to another may cause corruption, so we will arbitrarily choose to replace
        // the source if DCC format encoding is compatible and it is not an MM format. MM formats cannot be
        // replaced or HW will convert the data to the format's black or white which is different for MM formats.
        else if ((isSrcFormatReplaceable && (isDstFormatReplaceable == false)) ||
                 (isDccFormatEncodingMatch && (isMmFormatUsed == false)))
        {
            // We can replace the source format but not the destination format. This means that we must interpret
            // the source subresource using the destination numeric format. We should keep the original source
            // channel format if a swizzle copy was requested and is possible.
            srcFormat.format = Formats::ConvertToDstNumFmt(srcFormat.format, dstFormat.format);

            if ((TestAnyFlagSet(copyFlags, CopyRawSwizzle) == false) ||
                (device.SupportsCopy(srcFormat.format, srcCreateInfo.tiling) == false))
            {
                srcFormat = dstFormat;
            }
        }
        else if ((isSrcFormatReplaceable == false) && isDstFormatReplaceable)
        {
            // We can replace the destination format but not the source format. This means that we must interpret
            // the destination subresource using the source numeric format. We should keep the original destination
            // channel format if a swizzle copy was requested and is possible.
            dstFormat.format = Formats::ConvertToDstNumFmt(dstFormat.format, srcFormat.format);

            if ((TestAnyFlagSet(copyFlags, CopyRawSwizzle) == false) ||
                (device.SupportsCopy(dstFormat.format, dstCreateInfo.tiling) == false))
            {
                dstFormat = srcFormat;
            }
        }
        else
        {
            // We can't replace either format, both formats must match. Or the channels must match in the case of
            // an MM copy.
            PAL_ASSERT(formatsMatch || (chFmtsMatch && isMmFormatUsed));
        }
    }

    // We've settled on a pair of formats, make sure that we can actually use them.
    PAL_ASSERT(device.SupportsImageRead(srcFormat.format, srcCreateInfo.tiling));
    // We have specific code to handle srgb destination by treating it as unorm and handling gamma correction
    // manually. So it's ok to ignore SRGB for this assert.
    PAL_ASSERT(Formats::IsSrgb(dstFormat.format) ||
        (device.SupportsImageWrite(dstFormat.format, dstCreateInfo.tiling)));

    *pSrcFormat = srcFormat;
    *pDstFormat = dstFormat;
}

// =====================================================================================================================
// Builds commands to copy one or more regions from a GPU memory location to an image.
void RsrcProcMgr::CmdCopyMemoryToImage(
    GfxCmdBuffer*                pCmdBuffer,
    const GpuMemory&             srcGpuMemory,
    const Image&                 dstImage,
    ImageLayout                  dstImageLayout,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions,
    bool                         includePadding
    ) const
{
    // Select the appropriate pipeline for this copy based on the destination image's properties.
    const auto& createInfo = dstImage.GetImageCreateInfo();
    const ComputePipeline* pPipeline = nullptr;

    switch (dstImage.GetGfxImage()->GetOverrideImageType())
    {
    case ImageType::Tex1d:
        pPipeline = GetPipeline(RpmComputePipeline::CopyMemToImg1d);
        break;

    case ImageType::Tex2d:
        switch (createInfo.fragments)
        {
        case 2:
            pPipeline = GetPipeline(RpmComputePipeline::CopyMemToImg2dms2x);
            break;

        case 4:
            pPipeline = GetPipeline(RpmComputePipeline::CopyMemToImg2dms4x);
            break;

        case 8:
            pPipeline = GetPipeline(RpmComputePipeline::CopyMemToImg2dms8x);
            break;

        default:
            pPipeline = GetPipeline(RpmComputePipeline::CopyMemToImg2d);
            break;
        }
        break;

    default:
        pPipeline = GetPipeline(RpmComputePipeline::CopyMemToImg3d);
        break;
    }

    // Note that we must call this helper function before and after our compute blit to fix up our image's metadata
    // if the copy isn't compatible with our layout's metadata compression level.
    AutoBuffer<ImageFixupRegion, 32, Platform> fixupRegions(regionCount, m_pDevice->GetPlatform());
    if (fixupRegions.Capacity() >= regionCount)
    {
        for (uint32 i = 0; i < regionCount; i++)
        {
            fixupRegions[i].subres    = pRegions[i].imageSubres;
            fixupRegions[i].offset    = pRegions[i].imageOffset;
            fixupRegions[i].extent    = pRegions[i].imageExtent;
            fixupRegions[i].numSlices = pRegions[i].numSlices;
        }
        FixupMetadataForComputeDst(pCmdBuffer, dstImage, dstImageLayout, regionCount, &fixupRegions[0], true);

        CopyBetweenMemoryAndImage(pCmdBuffer, pPipeline, srcGpuMemory, dstImage, dstImageLayout, true, false,
                                  regionCount, pRegions, includePadding);

        FixupMetadataForComputeDst(pCmdBuffer, dstImage, dstImageLayout, regionCount, &fixupRegions[0], false);

        // If image is created with fullCopyDstOnly=1, there will be no expand when transition to "LayoutCopyDst"; if
        // the copy isn't compressed copy, need fix up dst metadata to uncompressed state.
        if (dstImage.GetImageCreateInfo().flags.fullCopyDstOnly != 0)
        {
            HwlFixupCopyDstImageMetaData(pCmdBuffer, nullptr, dstImage, dstImageLayout,
                                         &fixupRegions[0], regionCount, false);
        }
    }
    else
    {
        pCmdBuffer->NotifyAllocFailure();
    }
}

// =====================================================================================================================
// Builds commands to copy one or more regions from an image to a GPU memory location.
void RsrcProcMgr::CmdCopyImageToMemory(
    GfxCmdBuffer*                pCmdBuffer,
    const Image&                 srcImage,
    ImageLayout                  srcImageLayout,
    const GpuMemory&             dstGpuMemory,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions,
    bool                         includePadding
    ) const
{
    // Select the appropriate pipeline for this copy based on the source image's properties.
    const auto&            createInfo = srcImage.GetImageCreateInfo();
    const bool             isEqaaSrc  = (createInfo.samples != createInfo.fragments);
    const GfxImage*        pGfxImage  = srcImage.GetGfxImage();
    const ComputePipeline* pPipeline  = nullptr;

    bool isFmaskCopy = false;

    switch (pGfxImage->GetOverrideImageType())
    {
    case ImageType::Tex1d:
        pPipeline = GetPipeline(RpmComputePipeline::CopyImgToMem1d);
        break;

    case ImageType::Tex2d:
        // The Fmask accelerated copy should be used in all non-EQAA cases where Fmask is enabled. There is no use case
        // Fmask accelerated EQAA copy and it would require several new shaders. It can be implemented at a future
        // point if required.
        if (pGfxImage->HasFmaskData() && isEqaaSrc)
        {
            PAL_NOT_IMPLEMENTED();
        }
        if (pGfxImage->HasFmaskData() && (isEqaaSrc == false))
        {
            PAL_ASSERT((srcImage.IsDepthStencilTarget() == false) && (createInfo.fragments > 1));
            pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskCopyImgToMem);
            isFmaskCopy = true;
        }
        else
        {
            switch (createInfo.fragments)
            {
            case 2:
                pPipeline = GetPipeline(RpmComputePipeline::CopyImgToMem2dms2x);
                break;

            case 4:
                pPipeline = GetPipeline(RpmComputePipeline::CopyImgToMem2dms4x);
                break;

            case 8:
                pPipeline = GetPipeline(RpmComputePipeline::CopyImgToMem2dms8x);
                break;

            default:
                pPipeline = GetPipeline(RpmComputePipeline::CopyImgToMem2d);
                break;
            }
        }
        break;

    default:
        pPipeline = GetPipeline(RpmComputePipeline::CopyImgToMem3d);
        break;
    }

    CopyBetweenMemoryAndImage(pCmdBuffer,
                              pPipeline,
                              dstGpuMemory,
                              srcImage,
                              srcImageLayout,
                              false,
                              isFmaskCopy,
                              regionCount,
                              pRegions,
                              includePadding);
}

// =====================================================================================================================
void RsrcProcMgr::CopyBetweenMemoryAndImageCs(
    GfxCmdBuffer*                pCmdBuffer,
    const ComputePipeline*       pPipeline,
    const GpuMemory&             gpuMemory,
    const Image&                 image,
    ImageLayout                  imageLayout,
    bool                         isImageDst,
    bool                         isFmaskCopy,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions,
    bool                         includePadding,
    const gpusize*               pP2pBltInfoChunks
    ) const
{
    const auto& imgCreateInfo   = image.GetImageCreateInfo();
    const auto& device          = *m_pDevice->Parent();
    const auto* pPublicSettings = device.GetPublicSettings();
    const bool  is3d            = (imgCreateInfo.imageType == ImageType::Tex3d);

    // Get number of threads per groups in each dimension, we will need this data later.
    const DispatchDims threadsPerGroup = pPipeline->ThreadsPerGroupXyz();

    // Save current command buffer state and bind the pipeline.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    // Now begin processing the list of copy regions.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        MemoryImageCopyRegion copyRegion = pRegions[idx];

        // 3D images can't have slices and non-3D images shouldn't specify depth > 1 so we expect at least one
        // of them to be set to 1.
        PAL_ASSERT((copyRegion.numSlices == 1) || (copyRegion.imageExtent.depth == 1));

        if (pP2pBltInfoChunks != nullptr)
        {
            pCmdBuffer->P2pBltWaCopyNextRegion(pP2pBltInfoChunks[idx]);
        }

        // It will be faster to use a raw format, but we must stick with the base format if replacement isn't an option.
        SwizzledFormat    viewFormat = image.SubresourceInfo(copyRegion.imageSubres)->format;

        if (Formats::IsUndefined(copyRegion.swizzledFormat.format) == false)
        {
            viewFormat = copyRegion.swizzledFormat;
        }

        const ImageTiling srcTiling  = (isImageDst) ? ImageTiling::Linear : imgCreateInfo.tiling;

        // Our copy shaders and hardware treat sRGB and UNORM nearly identically, the only difference being that the
        // hardware modifies sRGB data when reading it and can't write it, which will make it hard to do a raw copy.
        // We can avoid that problem by simply forcing sRGB to UNORM.
        if (Formats::IsSrgb(viewFormat.format))
        {
            viewFormat.format = Formats::ConvertToUnorm(viewFormat.format);
            PAL_ASSERT(Formats::IsUndefined(viewFormat.format) == false);
        }

        bool singleSubres = false;
        if (image.GetGfxImage()->IsFormatReplaceable(copyRegion.imageSubres, imageLayout, isImageDst) ||
            (m_pDevice->Parent()->SupportsMemoryViewRead(viewFormat.format, srcTiling) == false))
        {
            uint32 texelScale     = 1;
            uint32 pixelsPerBlock = 1;
            if (m_pDevice->IsImageFormatOverrideNeeded(imgCreateInfo, &viewFormat.format, &pixelsPerBlock))
            {
                copyRegion.imageOffset.x     /= pixelsPerBlock;
                copyRegion.imageExtent.width /= pixelsPerBlock;
            }
            else
            {
                viewFormat = RpmUtil::GetRawFormat(viewFormat.format, &texelScale, &singleSubres);
                copyRegion.imageOffset.x     *= texelScale;
                copyRegion.imageExtent.width *= texelScale;
            }
            // If the format is not supported by the buffer SRD (checked with SupportsMemoryViewRead() above)
            // and the compression state check above (i.e., IsFormatReplaceable()) returns false, the
            // format is still replaced but a corruption may occur. The corruption can occur if the format
            // replacement results in a change in the color channel width and the resource is compressed.
            // This should not trigger because DoesImageSupportCopyCompression() limits the LayoutCopyDst
            // compressed usage in InitLayoutStateMasks().
            PAL_ASSERT(image.GetGfxImage()->IsFormatReplaceable(copyRegion.imageSubres, imageLayout, isImageDst)
                       == true);
        }

        // Make sure our view format supports reads and writes.
        PAL_ASSERT(device.SupportsImageWrite(viewFormat.format, imgCreateInfo.tiling) &&
                   device.SupportsImageRead(viewFormat.format, imgCreateInfo.tiling));

        // The row and depth pitches need to be expressed in terms of view format texels.
        const uint32 viewBpp    = Formats::BytesPerPixel(viewFormat.format);
        const uint32 rowPitch   = static_cast<uint32>(copyRegion.gpuMemoryRowPitch   / viewBpp);
        const uint32 depthPitch = static_cast<uint32>(copyRegion.gpuMemoryDepthPitch / viewBpp);

        // Generally the pipeline expects the user data to be arranged as follows for each dispatch:
        // Img X offset, Img Y offset, Img Z offset (3D), row pitch
        // Copy width, Copy height, Copy depth, slice pitch
        uint32 copyData[8] =
        {
            static_cast<uint32>(copyRegion.imageOffset.x),
            static_cast<uint32>(copyRegion.imageOffset.y),
            static_cast<uint32>(copyRegion.imageOffset.z),
            rowPitch,
            copyRegion.imageExtent.width,
            copyRegion.imageExtent.height,
            copyRegion.imageExtent.depth,
            depthPitch
        };

        // For fmask accelerated copy, the pipeline expects the user data to be arranged as below,
        // Img X offset, Img Y offset, samples, row pitch
        // Copy width, Copy height, Copy depth, slice pitch
        if (isFmaskCopy)
        {
            // Img Z offset doesn't make sense for msaa image; store numSamples instead.
            copyData[2] = imgCreateInfo.samples;
        }

        // User-data entry 0 is for the per-dispatch user-data table pointer. Embed the unchanging constant buffer
        // in the fast user-data entries after that table.
        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 1, ArrayLen32(copyData), copyData);

        const uint32 firstMipLevel  = copyRegion.imageSubres.mipLevel;
        const uint32 lastArraySlice = copyRegion.imageSubres.arraySlice + copyRegion.numSlices - 1;

        // If single subres is requested for the format, iterate slice-by-slice and mip-by-mip.
        if (singleSubres)
        {
            copyRegion.numSlices = 1;
        }

        if (isImageDst)
        {
            PAL_ASSERT(TestAnyFlagSet(imageLayout.usages, LayoutCopyDst));
        }

        const Extent3d bufferBox =
        {
            copyRegion.imageExtent.width,
            copyRegion.imageExtent.height,
            is3d ? copyRegion.imageExtent.depth : copyRegion.numSlices
        };

        BufferViewInfo bufferView = {};
        bufferView.gpuAddr        = gpuMemory.Desc().gpuVirtAddr + copyRegion.gpuMemoryOffset;
        bufferView.swizzledFormat = viewFormat;
        bufferView.stride         = viewBpp;
        bufferView.range          = ComputeTypedBufferRange(bufferBox,
                                                            viewBpp * imgCreateInfo.fragments,
                                                            copyRegion.gpuMemoryRowPitch,
                                                            copyRegion.gpuMemoryDepthPitch);
        bufferView.flags.bypassMallRead  = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                          RpmViewsBypassMallOnRead);
        bufferView.flags.bypassMallWrite = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                          RpmViewsBypassMallOnWrite);

        for (;
            copyRegion.imageSubres.arraySlice <= lastArraySlice;
            copyRegion.imageSubres.arraySlice += copyRegion.numSlices)
        {
            copyRegion.imageSubres.mipLevel = firstMipLevel;

            // Create an embedded user-data table to contain the Image SRD's. It will be bound to entry 0.
            uint32* pUserData = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                       SrdDwordAlignment() * 2,
                                                                       SrdDwordAlignment(),
                                                                       PipelineBindPoint::Compute,
                                                                       0);

            device.CreateTypedBufferViewSrds(1, &bufferView, pUserData);
            pUserData += SrdDwordAlignment();

            const SubresRange viewRange = { copyRegion.imageSubres, 1, 1, copyRegion.numSlices };
            ImageViewInfo     imageView = {};

            RpmUtil::BuildImageViewInfo(&imageView,
                                        image,
                                        viewRange,
                                        viewFormat,
                                        imageLayout,
                                        device.TexOptLevel(),
                                        isImageDst);
            imageView.flags.includePadding = includePadding;

            device.CreateImageViewSrds(1, &imageView, pUserData);
            pUserData += SrdDwordAlignment();

            if (isFmaskCopy)
            {
                // If this is an Fmask-accelerated Copy, create an image view of the source Image's Fmask surface.
                FmaskViewInfo fmaskView = {};
                fmaskView.pImage         = &image;
                fmaskView.baseArraySlice = copyRegion.imageSubres.arraySlice;
                fmaskView.arraySize      = copyRegion.numSlices;

                m_pDevice->Parent()->CreateFmaskViewSrds(1, &fmaskView, pUserData);
            }

            // Execute the dispatch, we need one thread per texel.
            const DispatchDims threads = {bufferBox.width, bufferBox.height, bufferBox.depth};

            pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroupsXyz(threads, threadsPerGroup));

            // Offset the buffer view to the next iteration's starting slice.
            bufferView.gpuAddr += copyRegion.gpuMemoryDepthPitch;
        }
    }

    if (pP2pBltInfoChunks != nullptr)
    {
        pCmdBuffer->P2pBltWaCopyEnd();
    }

    // Restore command buffer state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Builds commands to copy one or more regions between an image and a GPU memory location. Which object is the source
// and which object is the destination is determined by the given pipeline. This works because the image <-> memory
// pipelines all have the same input layouts.
void RsrcProcMgr::CopyBetweenMemoryAndImage(
    GfxCmdBuffer*                pCmdBuffer,
    const ComputePipeline*       pPipeline,
    const GpuMemory&             gpuMemory,
    const Image&                 image,
    ImageLayout                  imageLayout,
    bool                         isImageDst,
    bool                         isFmaskCopy,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions,
    bool                         includePadding
    ) const
{
    CopyBetweenMemoryAndImageCs(pCmdBuffer,
                                pPipeline,
                                gpuMemory,
                                image,
                                imageLayout,
                                isImageDst,
                                isFmaskCopy,
                                regionCount,
                                pRegions,
                                includePadding,
                                nullptr);
}

// =====================================================================================================================
// Builds commands to copy multiple regions directly (without format conversion) from one typed buffer to another.
void RsrcProcMgr::CmdCopyTypedBuffer(
    GfxCmdBuffer*                pCmdBuffer,
    const GpuMemory&             srcGpuMemory,
    const GpuMemory&             dstGpuMemory,
    uint32                       regionCount,
    const TypedBufferCopyRegion* pRegions
    ) const
{
    const auto& device          = *m_pDevice->Parent();
    const auto* pPublicSettings = device.GetPublicSettings();

    // Save current command buffer state.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    // We may have to bind a new pipeline for each region, we can optimize out redundant binds by tracking the previous
    // pipeline and only updating the pipeline binding when it must change.
    const ComputePipeline* pPipeline       = nullptr;
    const ComputePipeline* pPrevPipeline   = nullptr;
    DispatchDims           threadsPerGroup = {};

    // Now begin processing the list of copy regions.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        const TypedBufferInfo& srcInfo = pRegions[idx].srcBuffer;
        const TypedBufferInfo& dstInfo = pRegions[idx].dstBuffer;

        // Both buffers must have the same pixel size.
        PAL_ASSERT(Formats::BitsPerPixel(srcInfo.swizzledFormat.format) ==
                   Formats::BitsPerPixel(dstInfo.swizzledFormat.format));

        // Pick a raw format for the copy.
        uint32               texelScale = 1;
        const SwizzledFormat rawFormat  = RpmUtil::GetRawFormat(srcInfo.swizzledFormat.format, &texelScale, nullptr);

        // Multiply 'texelScale' into our extent to make sure we dispatch enough threads to copy the whole region.
        const Extent3d copyExtent =
        {
            pRegions[idx].extent.width * texelScale,
            pRegions[idx].extent.height,
            pRegions[idx].extent.depth
        };

        // The row and depth pitches need to be expressed in terms of raw format texels.
        const uint32 rawBpp        = Formats::BytesPerPixel(rawFormat.format);
        const uint32 dstRowPitch   = static_cast<uint32>(dstInfo.rowPitch   / rawBpp);
        const uint32 dstDepthPitch = static_cast<uint32>(dstInfo.depthPitch / rawBpp);
        const uint32 srcRowPitch   = static_cast<uint32>(srcInfo.rowPitch   / rawBpp);
        const uint32 srcDepthPitch = static_cast<uint32>(srcInfo.depthPitch / rawBpp);

        // Get the appropriate pipeline and user data based on the copy extents.
        uint32 userData[7] = {};
        uint32 numUserData = 0;

        if (copyExtent.depth > 1)
        {
            pPipeline   = GetPipeline(RpmComputePipeline::CopyTypedBuffer3d);
            userData[0] = dstRowPitch;
            userData[1] = dstDepthPitch;
            userData[2] = srcRowPitch;
            userData[3] = srcDepthPitch;
            userData[4] = copyExtent.width;
            userData[5] = copyExtent.height;
            userData[6] = copyExtent.depth;
            numUserData = 7;
        }
        else if (copyExtent.height > 1)
        {
            pPipeline   = GetPipeline(RpmComputePipeline::CopyTypedBuffer2d);
            userData[0] = dstRowPitch;
            userData[1] = srcRowPitch;
            userData[2] = copyExtent.width;
            userData[3] = copyExtent.height;
            numUserData = 4;
        }
        else
        {
            pPipeline   = GetPipeline(RpmComputePipeline::CopyTypedBuffer1d);
            userData[0] = copyExtent.width;
            numUserData = 1;
        }

        PAL_ASSERT(pPipeline != nullptr);

        // Change pipeline bindings if necessary.
        if (pPrevPipeline != pPipeline)
        {
            pPrevPipeline   = pPipeline;
            threadsPerGroup = pPipeline->ThreadsPerGroupXyz();
            pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });
        }

        // Create an embedded user-data table and bind it to user data 0. We need buffer views for the src and dst.
        uint32* pUserDataTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                        SrdDwordAlignment() * 2,
                                                                        SrdDwordAlignment(),
                                                                        PipelineBindPoint::Compute,
                                                                        0);

        BufferViewInfo bufferView = {};
        bufferView.gpuAddr        = dstGpuMemory.Desc().gpuVirtAddr + dstInfo.offset;
        bufferView.range          = ComputeTypedBufferRange(copyExtent, rawBpp, dstInfo.rowPitch, dstInfo.depthPitch);
        bufferView.stride         = rawBpp;
        bufferView.swizzledFormat = rawFormat;
        bufferView.flags.bypassMallRead  = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                          RpmViewsBypassMallOnRead);
        bufferView.flags.bypassMallWrite = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                          RpmViewsBypassMallOnWrite);

        device.CreateTypedBufferViewSrds(1, &bufferView, pUserDataTable);
        pUserDataTable += SrdDwordAlignment();

        bufferView.gpuAddr        = srcGpuMemory.Desc().gpuVirtAddr + srcInfo.offset;
        bufferView.range          = ComputeTypedBufferRange(copyExtent, rawBpp, srcInfo.rowPitch, srcInfo.depthPitch);

        device.CreateTypedBufferViewSrds(1, &bufferView, pUserDataTable);

        // Embed the constant buffer in the remaining fast user-data entries.
        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 1, numUserData, userData);

        // Execute the dispatch, we need one thread per texel.
        const DispatchDims threads = {copyExtent.width, copyExtent.height, copyExtent.depth};

        pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroupsXyz(threads, threadsPerGroup));
    }

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
void RsrcProcMgr::CmdScaledCopyImage(
    GfxCmdBuffer*           pCmdBuffer,
    const ScaledCopyInfo&   copyInfo
    ) const
{
    const bool useGraphicsCopy = ScaledCopyImageUseGraphics(pCmdBuffer, copyInfo);

    if (useGraphicsCopy)
    {
        // Save current command buffer state.
        pCmdBuffer->CmdSaveGraphicsState();
        ScaledCopyImageGraphics(pCmdBuffer, copyInfo);
        // Restore original command buffer state.
        pCmdBuffer->CmdRestoreGraphicsState();
    }
    else
    {
        // Note that we must call this helper function before and after our compute blit to fix up our image's
        // metadata if the copy isn't compatible with our layout's metadata compression level.
        const Image& dstImage = *static_cast<const Image*>(copyInfo.pDstImage);
        AutoBuffer<ImageFixupRegion, 32, Platform> fixupRegions(copyInfo.regionCount, m_pDevice->GetPlatform());
        if (fixupRegions.Capacity() >= copyInfo.regionCount)
        {
            for (uint32 i = 0; i < copyInfo.regionCount; i++)
            {
                fixupRegions[i].subres        = copyInfo.pRegions[i].dstSubres;
                fixupRegions[i].offset        = copyInfo.pRegions[i].dstOffset;
                fixupRegions[i].extent.width  = Math::Absu(copyInfo.pRegions[i].dstExtent.width);
                fixupRegions[i].extent.height = Math::Absu(copyInfo.pRegions[i].dstExtent.height);
                fixupRegions[i].extent.depth  = Math::Absu(copyInfo.pRegions[i].dstExtent.depth);
                fixupRegions[i].numSlices     = copyInfo.pRegions[i].numSlices;
            }
            FixupMetadataForComputeDst(pCmdBuffer, dstImage, copyInfo.dstImageLayout,
                                       copyInfo.regionCount, &fixupRegions[0], true);

            // Save current command buffer state and bind the pipeline.
            pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
            ScaledCopyImageCompute(pCmdBuffer, copyInfo);
            pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);

            FixupMetadataForComputeDst(pCmdBuffer, dstImage, copyInfo.dstImageLayout,
                                       copyInfo.regionCount, &fixupRegions[0], false);

            // If image is created with fullCopyDstOnly=1, there will be no expand when transition to
            // "LayoutCopyDst"; if the copy isn't compressed copy, need fix up dst metadata to uncompressed state.
            if (copyInfo.pDstImage->GetImageCreateInfo().flags.fullCopyDstOnly != 0)
            {
                HwlFixupCopyDstImageMetaData(pCmdBuffer, nullptr, dstImage, copyInfo.dstImageLayout,
                                             &fixupRegions[0], copyInfo.regionCount, false);
            }
        }
        else
        {
            pCmdBuffer->NotifyAllocFailure();
        }
    }
}

// =====================================================================================================================
void RsrcProcMgr::CmdGenerateMipmaps(
    GfxCmdBuffer*         pCmdBuffer,
    const GenMipmapsInfo& genInfo
    ) const
{
    // The range cannot start at mip zero and cannot extend past the last mip level.
    PAL_ASSERT((genInfo.range.startSubres.mipLevel >= 1) &&
               ((genInfo.range.startSubres.mipLevel + genInfo.range.numMips) <=
                genInfo.pImage->GetImageCreateInfo().mipLevels));

    if (m_pDevice->Parent()->Settings().mipGenUseFastPath &&
        (genInfo.pImage->GetImageCreateInfo().imageType == ImageType::Tex2d))
    {
        // Use compute shader-based path that can generate up to 12 mipmaps/array slice per pass.
        GenerateMipmapsFast(pCmdBuffer, genInfo);
    }
    else
    {
        // Use multi-pass scaled copy image-based path.
        GenerateMipmapsSlow(pCmdBuffer, genInfo);
    }
}

// =====================================================================================================================
void RsrcProcMgr::GenerateMipmapsFast(
    GfxCmdBuffer*         pCmdBuffer,
    const GenMipmapsInfo& genInfo
    ) const
{
    const auto& device          = *m_pDevice->Parent();
    const auto& settings        = device.Settings();
    const auto* pPublicSettings = device.GetPublicSettings();
    const auto& image           = *static_cast<const Image*>(genInfo.pImage);
    const auto& imageInfo       = image.GetImageCreateInfo();

    // The shader can only generate up to 12 mips in one pass.
    constexpr uint32 MaxNumMips = 12;

    const ComputePipeline*const pPipeline = (settings.useFp16GenMips == false) ?
                                            GetPipeline(RpmComputePipeline::GenerateMipmaps) :
                                            GetPipeline(RpmComputePipeline::GenerateMipmapsLowp);

    // Save current command buffer state and bind the pipeline.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    BarrierInfo barrier = { };
    barrier.waitPoint   = HwPipePreCs;

    constexpr HwPipePoint PostCs = HwPipePostCs;
    barrier.pipePointWaitCount   = 1;
    barrier.pPipePoints          = &PostCs;

    // If we need to generate more than MaxNumMips mip levels, then we will need to issue multiple dispatches with
    // internal barriers in between, because the src mip of a subsequent pass is the last dst mip of the previous pass.
    // Note that we don't need any barriers between per-array slice dispatches.
    BarrierTransition transition = { };
    transition.srcCacheMask = CoherShader;
    transition.dstCacheMask = CoherShaderRead;

    // We will specify the base subresource later on.
    transition.imageInfo.pImage                = genInfo.pImage;
    transition.imageInfo.subresRange.numPlanes = 1;
    transition.imageInfo.subresRange.numMips   = 1;
    transition.imageInfo.subresRange.numSlices = genInfo.range.numSlices;
    transition.imageInfo.oldLayout             = genInfo.genMipLayout;
    transition.imageInfo.newLayout             = genInfo.genMipLayout;

    barrier.transitionCount = 1;
    barrier.pTransitions    = &transition;

    barrier.reason = Developer::BarrierReasonUnknown;

    uint32 samplerType = 0; // 0 = linearSampler, 1 = pointSampler

    if ((genInfo.filter.magnification == Pal::XyFilterLinear) && (genInfo.filter.minification == Pal::XyFilterLinear))
    {
        PAL_ASSERT(genInfo.filter.mipFilter == Pal::MipFilterNone);
        samplerType = 0;
    }
    else if ((genInfo.filter.magnification == Pal::XyFilterPoint)
        && (genInfo.filter.minification == Pal::XyFilterPoint))
    {
        PAL_ASSERT(genInfo.filter.mipFilter == Pal::MipFilterNone);
        samplerType = 1;
    }
    else
    {
        PAL_NOT_IMPLEMENTED();
    }

    for (SubresId srcSubres = genInfo.range.startSubres;
         srcSubres.plane < (genInfo.range.startSubres.plane + genInfo.range.numPlanes);
         srcSubres.plane++)
    {
        srcSubres.mipLevel   = genInfo.range.startSubres.mipLevel - 1;
        srcSubres.arraySlice = genInfo.range.startSubres.arraySlice;

        for (uint32 start = 0; start < genInfo.range.numMips; start += MaxNumMips, srcSubres.mipLevel += MaxNumMips)
        {
            const uint32 numMipsToGenerate = Min((genInfo.range.numMips - start), MaxNumMips);

            // The shader can only handle one array slice per pass.
            for (uint32 slice = 0; slice < genInfo.range.numSlices; ++slice, ++srcSubres.arraySlice)
            {
                const SubResourceInfo& subresInfo = *image.SubresourceInfo(srcSubres);

                const SwizzledFormat srcFormat = (genInfo.swizzledFormat.format != ChNumFormat::Undefined)
                                                 ? genInfo.swizzledFormat : subresInfo.format;
                SwizzledFormat dstFormat = srcFormat;

                DispatchDims numWorkGroupsPerDim =
                {
                    RpmUtil::MinThreadGroups(subresInfo.extentTexels.width,  64),
                    RpmUtil::MinThreadGroups(subresInfo.extentTexels.height, 64),
                    1
                };

                const float invInputDims[] =
                {
                    (1.0f / subresInfo.extentTexels.width),
                    (1.0f / subresInfo.extentTexels.height),
                };

                // Bind inline constants to user data 0+.
                const uint32 copyData[] =
                {
                    numMipsToGenerate,                                               // numMips
                    numWorkGroupsPerDim.x * numWorkGroupsPerDim.y * numWorkGroupsPerDim.z,
                    reinterpret_cast<const uint32&>(invInputDims[0]),
                    reinterpret_cast<const uint32&>(invInputDims[1]),
                    samplerType,
                };
                const uint32 copyDataDwords = Util::NumBytesToNumDwords(sizeof(copyData));

                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 0, copyDataDwords, &copyData[0]);

                // Create an embedded user-data table and bind it.  We need an image view and a sampler for the src
                // subresource, image views for MaxNumMips dst subresources, and a buffer SRD pointing to the atomic
                // counter.
                constexpr uint8  NumSlots   = 2 + MaxNumMips + 1;
                uint32*          pUserData  = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                                     SrdDwordAlignment() * NumSlots,
                                                                                     SrdDwordAlignment(),
                                                                                     PipelineBindPoint::Compute,
                                                                                     copyDataDwords);

                // The hardware can't handle UAV stores using sRGB num format.  The resolve shaders already contain a
                // linear-to-gamma conversion, but in order for that to work the output UAV's num format must be
                // patched to be simple UNORM.
                if (Formats::IsSrgb(dstFormat.format))
                {
                    dstFormat.format = Formats::ConvertToUnorm(dstFormat.format);
                    PAL_ASSERT(Formats::IsUndefined(dstFormat.format) == false);

                    PAL_NOT_IMPLEMENTED_MSG("%s",
                        "Gamma correction for sRGB image writes is not yet implemented in the mipgen shader.");
                }

                SubresRange viewRange = { srcSubres, 1, 1, 1 };

                ImageViewInfo srcImageView = { };
                RpmUtil::BuildImageViewInfo(&srcImageView,
                                            image,
                                            viewRange,
                                            srcFormat,
                                            genInfo.baseMipLayout,
                                            device.TexOptLevel(),
                                            false);

                device.CreateImageViewSrds(1, &srcImageView, pUserData);
                pUserData += SrdDwordAlignment();

                SamplerInfo samplerInfo = { };
                samplerInfo.filter      = genInfo.filter;
                samplerInfo.addressU    = TexAddressMode::Clamp;
                samplerInfo.addressV    = TexAddressMode::Clamp;
                samplerInfo.addressW    = TexAddressMode::Clamp;
                samplerInfo.compareFunc = CompareFunc::Always;
                device.CreateSamplerSrds(1, &samplerInfo, pUserData);
                pUserData += SrdDwordAlignment();

                ImageViewInfo dstImageView[MaxNumMips] = { };
                for (uint32 mip = 0; mip < MaxNumMips; ++mip)
                {
                    if (mip < numMipsToGenerate)
                    {
                        ++viewRange.startSubres.mipLevel;
                    }

                    RpmUtil::BuildImageViewInfo(&dstImageView[mip],
                                                image,
                                                viewRange,
                                                dstFormat,
                                                genInfo.genMipLayout,
                                                device.TexOptLevel(),
                                                true);
                }

                device.CreateImageViewSrds(MaxNumMips, &dstImageView[0], pUserData);
                pUserData += (SrdDwordAlignment() * MaxNumMips);

                // Allocate scratch memory for the global atomic counter and initialize it to 0.
                const gpusize counterVa = pCmdBuffer->AllocateGpuScratchMem(1, Util::NumBytesToNumDwords(128));
                pCmdBuffer->CmdWriteImmediate(HwPipePoint::HwPipeTop,
                                              0,
                                              ImmediateDataWidth::ImmediateData32Bit,
                                              counterVa);

                BufferViewInfo bufferView = { };
                bufferView.gpuAddr        = counterVa;
                bufferView.stride         = 0;
                bufferView.range          = sizeof(uint32);
                bufferView.swizzledFormat = UndefinedSwizzledFormat;
                bufferView.flags.bypassMallRead  = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                                  RpmViewsBypassMallOnRead);
                bufferView.flags.bypassMallWrite = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                                  RpmViewsBypassMallOnWrite);
                device.CreateUntypedBufferViewSrds(1, &bufferView, pUserData);

                // Execute the dispatch.
                pCmdBuffer->CmdDispatch(numWorkGroupsPerDim);
            }

            srcSubres.arraySlice = genInfo.range.startSubres.arraySlice;

            if ((start + MaxNumMips) < genInfo.range.numMips)
            {
                // If we need to do additional dispatches to handle more mip levels, issue a barrier between each pass.
                transition.imageInfo.subresRange.startSubres          = srcSubres;
                transition.imageInfo.subresRange.startSubres.mipLevel = (start + numMipsToGenerate);

                pCmdBuffer->CmdBarrier(barrier);
            }
        }
    }

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
void RsrcProcMgr::GenerateMipmapsSlow(
    GfxCmdBuffer*         pCmdBuffer,
    const GenMipmapsInfo& genInfo
    ) const
{
    const Pal::Image*      pImage     = static_cast<const Pal::Image*>(genInfo.pImage);
    const ImageCreateInfo& createInfo = pImage->GetImageCreateInfo();

    // We will use scaled image copies to generate each mip. Most of the copy state is identical but we must adjust the
    // copy region for each generated subresource.
    ImageScaledCopyRegion region = {};
    region.srcSubres.arraySlice = genInfo.range.startSubres.arraySlice;
    region.dstSubres.arraySlice = genInfo.range.startSubres.arraySlice;
    region.numSlices            = genInfo.range.numSlices;
    region.swizzledFormat       = genInfo.swizzledFormat;

    ScaledCopyInfo copyInfo = {};
    copyInfo.pSrcImage      = pImage;
    copyInfo.srcImageLayout = genInfo.baseMipLayout;
    copyInfo.pDstImage      = pImage;
    copyInfo.dstImageLayout = genInfo.genMipLayout;
    copyInfo.regionCount    = 1;
    copyInfo.pRegions       = &region;
    copyInfo.filter         = genInfo.filter;
    copyInfo.rotation       = ImageRotation::Ccw0;

    const bool useGraphicsCopy = ScaledCopyImageUseGraphics(pCmdBuffer, copyInfo);

    // We need an internal barrier between each mip-level's scaled copy because the destination of the prior copy is
    // the source of the next copy. Note that we can't use CoherCopy here because we optimize it away in the barrier
    // code but that optimization requires that we pop all state before calling CmdBarrier. That's very slow so instead
    // we use implementation dependent cache masks.
    BarrierTransition transition = {};
    transition.srcCacheMask = useGraphicsCopy ? CoherColorTarget : CoherShader;
    transition.dstCacheMask = CoherShaderRead;

    // We will specify the base subresource later on.
    transition.imageInfo.pImage                = pImage;
    transition.imageInfo.subresRange.numPlanes = 1;
    transition.imageInfo.subresRange.numMips   = 1;
    transition.imageInfo.subresRange.numSlices = genInfo.range.numSlices;
    transition.imageInfo.oldLayout             = genInfo.genMipLayout;
    transition.imageInfo.newLayout             = genInfo.genMipLayout;

    const HwPipePoint postBlt = useGraphicsCopy ? HwPipeBottom : HwPipePostCs;
    BarrierInfo       barrier = {};

    barrier.waitPoint          = HwPipePostPrefetch;
    barrier.pipePointWaitCount = 1;
    barrier.pPipePoints        = &postBlt;
    barrier.transitionCount    = 1;
    barrier.pTransitions       = &transition;

    // Save current command buffer state.
    if (useGraphicsCopy)
    {
        pCmdBuffer->CmdSaveGraphicsState();
    }
    else
    {
        pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    }

    // Issue one CmdScaledCopyImage for each mip, and plane in the generation range.
    const uint32 lastMip   = genInfo.range.startSubres.mipLevel + genInfo.range.numMips - 1;

    for (uint32 plane = genInfo.range.startSubres.plane;
         plane < (genInfo.range.startSubres.plane + genInfo.range.numPlanes);
         plane++)
    {
        region.srcSubres.plane = plane;
        region.dstSubres.plane = plane;

        uint32 destMip = genInfo.range.startSubres.mipLevel;

        while (destMip <= lastMip)
        {
            region.srcSubres.mipLevel = destMip - 1;
            region.dstSubres.mipLevel = destMip;

            // We want to generate all texels in the target subresource so copy the full extent from the first array
            // slice in the current source and destination mips.
            const SubResourceInfo& srcSubresInfo = *pImage->SubresourceInfo(region.srcSubres);
            const SubResourceInfo& dstSubresInfo = *pImage->SubresourceInfo(region.dstSubres);

            region.srcExtent.width  = srcSubresInfo.extentTexels.width;
            region.srcExtent.height = srcSubresInfo.extentTexels.height;
            region.srcExtent.depth  = srcSubresInfo.extentTexels.depth;
            region.dstExtent.width  = dstSubresInfo.extentTexels.width;
            region.dstExtent.height = dstSubresInfo.extentTexels.height;
            region.dstExtent.depth  = dstSubresInfo.extentTexels.depth;

            if (useGraphicsCopy)
            {
                ScaledCopyImageGraphics(pCmdBuffer, copyInfo);
            }
            else
            {
                ScaledCopyImageCompute(pCmdBuffer, copyInfo);
            }

            // If we're going to loop again...
            if (++destMip <= lastMip)
            {
                // Update the copy's source layout.
                copyInfo.srcImageLayout = genInfo.genMipLayout;

                // Issue the barrier between this iteration's writes and the next iteration's reads.
                transition.imageInfo.subresRange.startSubres = region.dstSubres;

                pCmdBuffer->CmdBarrier(barrier);
            }
        }
    }

    // Restore original command buffer state.
    if (useGraphicsCopy)
    {
        pCmdBuffer->CmdRestoreGraphicsState();
    }
    else
    {
        pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
    }
}

// =====================================================================================================================
void RsrcProcMgr::ScaledCopyImageCompute(
    GfxCmdBuffer*         pCmdBuffer,
    const ScaledCopyInfo& copyInfo
    ) const
{
    PAL_ASSERT(copyInfo.flags.scissorTest == 0);

    const auto& device       = *m_pDevice->Parent();
    const auto* pSrcImage    = static_cast<const Image*>(copyInfo.pSrcImage);
    const auto* pSrcGfxImage = pSrcImage->GetGfxImage();
    const auto* pDstImage    = static_cast<const Image*>(copyInfo.pDstImage);
    const auto* pDstGfxImage = pDstImage->GetGfxImage();
    const auto& srcInfo      = pSrcImage->GetImageCreateInfo();
    const auto& dstInfo      = pDstImage->GetImageCreateInfo();

    const bool imageTypeMatch = (pSrcGfxImage->GetOverrideImageType() == pDstGfxImage->GetOverrideImageType());
    const bool is3d           = (imageTypeMatch && (pSrcGfxImage->GetOverrideImageType() == ImageType::Tex3d));
    const bool viewMatchDim   = (IsGfx8(device) || IsGfx9(device));
    bool       isFmaskCopy    = false;

    // Get the appropriate pipeline object.
    // Scaling textures relies on sampler instructions.
    // GFX10+: IL type declarations set DIM, which controls the parameters [S,R,T,Q] to alloc.
    //    [S,R] can be generalized for sampler operations. 2D array also works
    //      [T] is interpreted differently by samplers if DIM is 3D.
    const ComputePipeline* pPipeline = nullptr;
    if (is3d)
    {
        pPipeline = GetPipeline(RpmComputePipeline::ScaledCopyImage3d);
    }
    else
    {
        const bool isDepth = (pSrcImage->IsDepthStencilTarget() || pDstImage->IsDepthStencilTarget());

        if (srcInfo.fragments > 1)
        {
            // HW doesn't support UAV writes to depth/stencil MSAA surfaces.
            PAL_ASSERT(isDepth == false);

            // EQAA images with FMask disabled are unsupported for scaled copy. There is no use case for
            // EQAA and it would require several new shaders. It can be implemented if needed at a future point.
            PAL_ASSERT(srcInfo.samples == srcInfo.fragments);

            // Sampling msaa image with linear filter for scaled copy are unsupported, It should be simulated in
            // shader if needed at a future point.
            if (copyInfo.filter.magnification != Pal::XyFilterPoint)
            {
                PAL_ASSERT_MSG(0, "HW doesn't support image Opcode for msaa image with sampler");
            }

            if (pSrcGfxImage->HasFmaskData())
            {
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskScaledCopy);
                isFmaskCopy = true;
            }
            else
            {
                pPipeline = GetPipeline(RpmComputePipeline::MsaaScaledCopyImage2d);
            }
        }
        else
        {
            pPipeline = GetPipeline(RpmComputePipeline::ScaledCopyImage2d);
        }
    }

    // Get number of threads per groups in each dimension, we will need this data later.
    const DispatchDims threadsPerGroup = pPipeline->ThreadsPerGroupXyz();

    PAL_ASSERT(pCmdBuffer->IsComputeStateSaved());

    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    uint32 colorKey[4]          = { 0 };
    uint32 alphaDiffMul         = 0;
    float  threshold            = 0.0f;
    uint32 colorKeyEnableMask   = 0;
    uint32 alphaBlendEnableMask = 0;

    if (copyInfo.flags.srcColorKey)
    {
        colorKeyEnableMask = 1;
    }
    else if (copyInfo.flags.dstColorKey)
    {
        colorKeyEnableMask = 2;
    }
    else if (copyInfo.flags.srcAlpha)
    {
        alphaBlendEnableMask = 4;
    }

    if (colorKeyEnableMask > 0)
    {
        const bool srcColorKey = (colorKeyEnableMask == 1);

        PAL_ASSERT(copyInfo.pColorKey != nullptr);
        PAL_ASSERT(srcInfo.imageType == ImageType::Tex2d);
        PAL_ASSERT(dstInfo.imageType == ImageType::Tex2d);
        PAL_ASSERT(srcInfo.samples <= 1);
        PAL_ASSERT(dstInfo.samples <= 1);
        PAL_ASSERT(pPipeline == GetPipeline(RpmComputePipeline::ScaledCopyImage2d));

        memcpy(&colorKey[0], &copyInfo.pColorKey->u32Color[0], sizeof(colorKey));

        // Convert uint color key to float representation
        SwizzledFormat format = srcColorKey ? srcInfo.swizzledFormat : dstInfo.swizzledFormat;
        RpmUtil::ConvertClearColorToNativeFormat(format, format, colorKey);
        // Only GenerateMips uses swizzledFormat in regions, color key is not available in this case.
        PAL_ASSERT(Formats::IsUndefined(copyInfo.pRegions[0].swizzledFormat.format));

        // Set constant to respect or ignore alpha channel color diff
        constexpr uint32 FloatOne = 0x3f800000;
        alphaDiffMul = Formats::HasUnusedAlpha(format) ? 0 : FloatOne;

        // Compute the threshold for comparing 2 float value
        const uint32 bitCount = Formats::MaxComponentBitCount(format.format);
        threshold = static_cast<float>(pow(2, -2.0f * bitCount) - pow(2, -2.0f * bitCount - 24.0f));
    }

    // Now begin processing the list of copy regions.
    for (uint32 idx = 0; idx < copyInfo.regionCount; ++idx)
    {
        ImageScaledCopyRegion copyRegion = copyInfo.pRegions[idx];

        // Calculate the absolute value of dstExtent, which will get fed to the shader.
        const int32 dstExtentW = (copyInfo.flags.coordsInFloat != 0) ?
            static_cast<int32>(copyRegion.dstExtentFloat.width + 0.5f) : copyRegion.dstExtent.width;
        const int32 dstExtentH = (copyInfo.flags.coordsInFloat != 0) ?
            static_cast<int32>(copyRegion.dstExtentFloat.height + 0.5f) : copyRegion.dstExtent.height;
        const int32 dstExtentD = (copyInfo.flags.coordsInFloat != 0) ?
            static_cast<int32>(copyRegion.dstExtentFloat.depth + 0.5f) : copyRegion.dstExtent.depth;

        const uint32 absDstExtentW = Math::Absu(dstExtentW);
        const uint32 absDstExtentH = Math::Absu(dstExtentH);
        const uint32 absDstExtentD = Math::Absu(dstExtentD);

        if ((absDstExtentW > 0) && (absDstExtentH > 0) && (absDstExtentD > 0))
        {
            // A negative extent means that we should do a reverse the copy.
            // We want to always use the absolute value of dstExtent.
            // otherwise the compute shader can't handle it. If dstExtent is negative in one
            // dimension, then we negate srcExtent in that dimension, and we adjust the offsets
            // as well.
            if (copyInfo.flags.coordsInFloat != 0)
            {
                if (copyRegion.dstExtentFloat.width < 0)
                {
                    copyRegion.dstOffsetFloat.x = copyRegion.dstOffsetFloat.x + copyRegion.dstExtentFloat.width;
                    copyRegion.srcOffsetFloat.x = copyRegion.srcOffsetFloat.x + copyRegion.srcExtentFloat.width;
                    copyRegion.srcExtentFloat.width = -copyRegion.srcExtentFloat.width;
                    copyRegion.dstExtentFloat.width = -copyRegion.dstExtentFloat.width;
                }

                if (copyRegion.dstExtentFloat.height < 0)
                {
                    copyRegion.dstOffsetFloat.y = copyRegion.dstOffsetFloat.y + copyRegion.dstExtentFloat.height;
                    copyRegion.srcOffsetFloat.y = copyRegion.srcOffsetFloat.y + copyRegion.srcExtentFloat.height;
                    copyRegion.srcExtentFloat.height = -copyRegion.srcExtentFloat.height;
                    copyRegion.dstExtentFloat.height = -copyRegion.dstExtentFloat.height;
                }

                if (copyRegion.dstExtentFloat.depth < 0)
                {
                    copyRegion.dstOffsetFloat.z = copyRegion.dstOffsetFloat.z + copyRegion.dstExtentFloat.depth;
                    copyRegion.srcOffsetFloat.z = copyRegion.srcOffsetFloat.z + copyRegion.srcExtentFloat.depth;
                    copyRegion.srcExtentFloat.depth = -copyRegion.srcExtentFloat.depth;
                    copyRegion.dstExtentFloat.depth = -copyRegion.dstExtentFloat.depth;
                }
            }
            else
            {
                if (copyRegion.dstExtent.width < 0)
                {
                    copyRegion.dstOffset.x = copyRegion.dstOffset.x + copyRegion.dstExtent.width;
                    copyRegion.srcOffset.x = copyRegion.srcOffset.x + copyRegion.srcExtent.width;
                    copyRegion.srcExtent.width = -copyRegion.srcExtent.width;
                    copyRegion.dstExtent.width = -copyRegion.dstExtent.width;
                }

                if (copyRegion.dstExtent.height < 0)
                {
                    copyRegion.dstOffset.y = copyRegion.dstOffset.y + copyRegion.dstExtent.height;
                    copyRegion.srcOffset.y = copyRegion.srcOffset.y + copyRegion.srcExtent.height;
                    copyRegion.srcExtent.height = -copyRegion.srcExtent.height;
                    copyRegion.dstExtent.height = -copyRegion.dstExtent.height;
                }

                if (copyRegion.dstExtent.depth < 0)
                {
                    copyRegion.dstOffset.z = copyRegion.dstOffset.z + copyRegion.dstExtent.depth;
                    copyRegion.srcOffset.z = copyRegion.srcOffset.z + copyRegion.srcExtent.depth;
                    copyRegion.srcExtent.depth = -copyRegion.srcExtent.depth;
                    copyRegion.dstExtent.depth = -copyRegion.dstExtent.depth;
                }
            }

            // The shader expects the region data to be arranged as follows for each dispatch:
            // Src Normalized Left,  Src Normalized Top,   Src Normalized Start-Z (3D) or slice (1D/2D), extent width
            // Dst Pixel X offset,   Dst Pixel Y offset,   Dst Z offset (3D) or slice (1D/2D),           extent height
            // Src Normalized Right, SrcNormalized Bottom, Src Normalized End-Z   (3D),                  extent depth

            // For 3D blts, the source Z-values are normalized as the X and Y values are for 1D, 2D, and 3D.

            const Extent3d& srcExtent = pSrcImage->SubresourceInfo(copyRegion.srcSubres)->extentTexels;
            float srcLeft    = 0.0f;
            float srcTop     = 0.0f;
            float srcRight   = 0.0f;
            float srcBottom  = 0.0f;
            float srcSlice   = 0.0f;
            float srcDepth   = 0.0f;
            float dstOffsetX = 0.0f;
            float dstOffsetY = 0.0f;
            float dstOffsetZ = 0.0f;

            if (copyInfo.flags.coordsInFloat != 0)
            {
                srcLeft   = copyRegion.srcOffsetFloat.x / srcExtent.width;
                srcTop    = copyRegion.srcOffsetFloat.y / srcExtent.height;
                srcRight  = (copyRegion.srcOffsetFloat.x + copyRegion.srcExtentFloat.width) / srcExtent.width;
                srcBottom = (copyRegion.srcOffsetFloat.y + copyRegion.srcExtentFloat.height) / srcExtent.height;
                srcSlice  = (1.f * copyRegion.srcOffsetFloat.z) / srcExtent.depth;
                srcDepth  = (1.f * (copyRegion.srcOffsetFloat.z + copyRegion.srcExtentFloat.depth)) / srcExtent.depth;

                dstOffsetX = copyRegion.dstOffsetFloat.x;
                dstOffsetY = copyRegion.dstOffsetFloat.y;
                dstOffsetZ = copyRegion.dstOffsetFloat.z;

            }
            else
            {
                srcLeft   = (1.f * copyRegion.srcOffset.x) / srcExtent.width;
                srcTop    = (1.f * copyRegion.srcOffset.y) / srcExtent.height;
                srcRight  = (1.f * (copyRegion.srcOffset.x + copyRegion.srcExtent.width)) / srcExtent.width;
                srcBottom = (1.f * (copyRegion.srcOffset.y + copyRegion.srcExtent.height)) / srcExtent.height;
                srcSlice  = (1.f * copyRegion.srcOffset.z) / srcExtent.depth;
                srcDepth  = (1.f * (copyRegion.srcOffset.z + copyRegion.srcExtent.depth)) / srcExtent.depth;

                dstOffsetX = 1.f * copyRegion.dstOffset.x;
                dstOffsetY = 1.f * copyRegion.dstOffset.y;
                dstOffsetZ = 1.f * copyRegion.dstOffset.z;
            }

            PAL_ASSERT((srcLeft >= 0.0f) && (srcLeft <= 1.0f) &&
                (srcTop >= 0.0f) && (srcTop <= 1.0f)          &&
                (srcSlice >= 0.0f) && (srcSlice <= 1.0f)      &&
                (srcRight >= 0.0f) && (srcRight <= 1.0f)      &&
                (srcBottom >= 0.0f) && (srcBottom <= 1.0f)    &&
                (srcDepth >= 0.0f) && (srcDepth <= 1.0f));

            SwizzledFormat dstFormat = pDstImage->SubresourceInfo(copyRegion.dstSubres)->format;
            SwizzledFormat srcFormat = pSrcImage->SubresourceInfo(copyRegion.srcSubres)->format;
            if (Formats::IsUndefined(copyRegion.swizzledFormat.format) == false)
            {
                srcFormat = copyRegion.swizzledFormat;
                dstFormat = copyRegion.swizzledFormat;
            }

            const uint32 zfilter   = copyInfo.filter.zFilter;
            const uint32 magfilter = copyInfo.filter.magnification;
            const uint32 minfilter = copyInfo.filter.minification;

            float zOffset = 0.0f;
            if (is3d)
            {
                zOffset = 0.5f;
            }
            else if (zfilter == ZFilterNone)
            {
                if ((magfilter != XyFilterPoint) || (minfilter != XyFilterPoint))
                {
                    zOffset = 0.5f;
                }
            }
            else if (zfilter != ZFilterPoint)
            {
                zOffset = 0.5f;
            }

            // RotationParams contains the parameters to rotate 2d texture cooridnates.
            // Given 2d texture coordinates (u, v), we use following equations to compute rotated coordinates (u', v'):
            // u' = RotationParams[0] * u + RotationParams[1] * v + RotationParams[4]
            // v' = RotationParams[2] * u + RotationParams[3] * v + RotationParams[5]
            constexpr float RotationParams[static_cast<uint32>(ImageRotation::Count)][6] =
            {
                { 1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f},
                { 0.0f, -1.0f,  1.0f,  0.0f, 1.0f, 0.0f},
                {-1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f},
                { 0.0f,  1.0f, -1.0f,  0.0f, 0.0f, 1.0f},
            };

            const uint32 rotationIndex = static_cast<const uint32>(copyInfo.rotation);

            // Enable gamma conversion when dstFormat is Srgb or copyInfo.flags.dstAsSrgb
            const uint32 enableGammaConversion =
                (Formats::IsSrgb(dstFormat.format) || copyInfo.flags.dstAsSrgb) ? 1 : 0;

            uint32 copyData[] =
            {
                reinterpret_cast<const uint32&>(srcLeft),
                reinterpret_cast<const uint32&>(srcTop),
                reinterpret_cast<const uint32&>(srcSlice),
                absDstExtentW,
                static_cast<uint32>(dstOffsetX),
                static_cast<uint32>(dstOffsetY),
                static_cast<uint32>(dstOffsetZ),
                absDstExtentH,
                reinterpret_cast<const uint32&>(srcRight),
                reinterpret_cast<const uint32&>(srcBottom),
                reinterpret_cast<const uint32&>(srcDepth),
                absDstExtentD,
                enableGammaConversion,
                reinterpret_cast<const uint32&>(zOffset),
                srcInfo.samples,
                (colorKeyEnableMask | alphaBlendEnableMask),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][0]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][1]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][2]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][3]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][4]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][5]),
                alphaDiffMul,
                Util::Math::FloatToBits(threshold),
                colorKey[0],
                colorKey[1],
                colorKey[2],
                colorKey[3],
            };

            // Create an embedded user-data table and bind it to user data 0. We need image views for the src and dst
            // subresources, a sampler for the src subresource, as well as some inline constants for the copy offsets
            // and extents.
            const uint32 DataDwords = NumBytesToNumDwords(sizeof(copyData));
            const uint8  numSlots   = ((srcInfo.samples > 1) && !isFmaskCopy) ? 2 : 3;
            uint32* pUserData       = RpmUtil::CreateAndBindEmbeddedUserData(
                                                    pCmdBuffer,
                                                    SrdDwordAlignment() * numSlots + DataDwords,
                                                    SrdDwordAlignment(),
                                                    PipelineBindPoint::Compute,
                                                    0);

            // The hardware can't handle UAV stores using SRGB num format.  The resolve shaders already contain a
            // linear-to-gamma conversion, but in order for that to work the output UAV's num format must be patched to
            // be simple unorm.
            if (Formats::IsSrgb(dstFormat.format))
            {
                dstFormat.format = Formats::ConvertToUnorm(dstFormat.format);
                PAL_ASSERT(Formats::IsUndefined(dstFormat.format) == false);
            }

            ImageViewInfo imageView[2] = {};
            SubresRange   viewRange    = { copyRegion.dstSubres, 1, 1, copyRegion.numSlices };

            PAL_ASSERT(TestAnyFlagSet(copyInfo.dstImageLayout.usages, LayoutCopyDst) == true);
            RpmUtil::BuildImageViewInfo(&imageView[0],
                                        *pDstImage,
                                        viewRange,
                                        dstFormat,
                                        copyInfo.dstImageLayout,
                                        device.TexOptLevel(),
                                        true);
            viewRange.startSubres = copyRegion.srcSubres;
            RpmUtil::BuildImageViewInfo(&imageView[1],
                                        *pSrcImage,
                                        viewRange,
                                        srcFormat,
                                        copyInfo.srcImageLayout,
                                        device.TexOptLevel(),
                                        false);

            // Image view type matters for HW addrlib. Only override if absolutely necessary.
            // GFX10+: Sample instruction limitations depend on DIM, not the image view type.
            //    See comments around the initialization of pPipeline for more details.
            // GFX8,9: See similar behavior in copyImageCS. Unclear why this is necessary.
            if (viewMatchDim && (is3d == false))
            {
                imageView[0].viewType = ImageViewType::Tex2d;
                imageView[1].viewType = ImageViewType::Tex2d;
            }

            device.CreateImageViewSrds(2, &imageView[0], pUserData);
            pUserData += SrdDwordAlignment() * 2;

            if (srcInfo.samples > 1)
            {
                if (isFmaskCopy)
                {
                    // If this is an Fmask-accelerated Copy, create an image view of the source Image's Fmask surface.
                    FmaskViewInfo fmaskView  = {};
                    fmaskView.pImage         = pSrcImage;
                    fmaskView.baseArraySlice = copyRegion.srcSubres.arraySlice;
                    fmaskView.arraySize      = copyRegion.numSlices;

                    m_pDevice->Parent()->CreateFmaskViewSrds(1, &fmaskView, pUserData);
                    pUserData += SrdDwordAlignment();
                }

                // HW doesn't support sample_resource instruction for msaa image, we need use load_resource to fetch
                // data for msaa image, should use src image extent to convert floating point texture coordinate values
                // referencing normalized space to signed integer values in IL shader.
                copyData[10] = srcExtent.width;
                copyData[11] = srcExtent.height;
            }
            // HW doesn't support image Opcode for msaa image with sampler.
            else
            {
                SamplerInfo samplerInfo = {};
                samplerInfo.filter      = copyInfo.filter;
                samplerInfo.addressU    = TexAddressMode::Clamp;
                samplerInfo.addressV    = TexAddressMode::Clamp;
                samplerInfo.addressW    = TexAddressMode::Clamp;
                samplerInfo.compareFunc = CompareFunc::Always;
                device.CreateSamplerSrds(1, &samplerInfo, pUserData);
                pUserData += SrdDwordAlignment();
            }

            // Copy the copy parameters into the embedded user-data space
            memcpy(pUserData, &copyData[0], sizeof(copyData));

            const uint32 zGroups = is3d ? absDstExtentD : copyRegion.numSlices;

            // Execute the dispatch, we need one thread per texel.
            const DispatchDims threads = {absDstExtentW, absDstExtentH, zGroups};

            pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroupsXyz(threads, threadsPerGroup));
        }
    }

    if (CopyDstBoundStencilNeedsWa(pCmdBuffer, *pDstImage))
    {
        for (uint32 regionIdx = 0; regionIdx < copyInfo.regionCount; regionIdx++)
        {
            if (pDstImage->IsStencilPlane(copyInfo.pRegions[regionIdx].dstSubres.plane))
            {
                // Mark the VRS dest image as dirty to force an update of Htile on the next draw.
                pCmdBuffer->DirtyVrsDepthImage(pDstImage);

                // No need to loop through all the regions; they all affect the same image.
                break;
            }
        }
    }
}

// =====================================================================================================================
// Builds commands to perform an out-of-place conversion between a YUV and an RGB image.
void RsrcProcMgr::CmdColorSpaceConversionCopy(
    GfxCmdBuffer*                     pCmdBuffer,
    const Image&                      srcImage,
    ImageLayout                       srcImageLayout,
    const Image&                      dstImage,
    ImageLayout                       dstImageLayout,
    uint32                            regionCount,
    const ColorSpaceConversionRegion* pRegions,
    TexFilter                         filter,
    const ColorSpaceConversionTable&  cscTable
    ) const
{
    const auto& srcImageInfo = srcImage.GetImageCreateInfo();
    const auto& dstImageInfo = dstImage.GetImageCreateInfo();
    PAL_ASSERT((srcImageInfo.imageType == ImageType::Tex2d) && (dstImageInfo.imageType == ImageType::Tex2d));

    const bool srcIsYuv = Formats::IsYuv(srcImageInfo.swizzledFormat.format);
    const bool dstIsYuv = Formats::IsYuv(dstImageInfo.swizzledFormat.format);

    SamplerInfo samplerInfo = { };
    samplerInfo.filter      = filter;
    samplerInfo.addressU    = TexAddressMode::Clamp;
    samplerInfo.addressV    = TexAddressMode::Clamp;
    samplerInfo.addressW    = TexAddressMode::Clamp;
    samplerInfo.compareFunc = CompareFunc::Always;

    if ((dstIsYuv == false) && srcIsYuv)
    {
        ConvertYuvToRgb(pCmdBuffer, srcImage, dstImage, regionCount, pRegions, samplerInfo, cscTable);
    }
    else if ((srcIsYuv == false) && dstIsYuv)
    {
        ConvertRgbToYuv(pCmdBuffer, srcImage, dstImage, regionCount, pRegions, samplerInfo, cscTable);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }
}

// =====================================================================================================================
// Builds commands to execute a color-space-conversion copy from a YUV source to an RGB destination.
void RsrcProcMgr::ConvertYuvToRgb(
    GfxCmdBuffer*                     pCmdBuffer,
    const Image&                      srcImage,
    const Image&                      dstImage,
    uint32                            regionCount,
    const ColorSpaceConversionRegion* pRegions,
    const SamplerInfo&                sampler,
    const ColorSpaceConversionTable&  cscTable
    ) const
{
    const auto& device       = *m_pDevice->Parent();
    const auto& srcImageInfo = srcImage.GetImageCreateInfo();
    const auto& dstImageInfo = dstImage.GetImageCreateInfo();

    // Build YUV to RGB color-space-conversion table constant buffer.
    RpmUtil::YuvRgbConversionInfo copyInfo = { };
    memcpy(copyInfo.cscTable, &cscTable, sizeof(cscTable));
    const RpmUtil::ColorSpaceConversionInfo& cscInfo =
        RpmUtil::CscInfoTable[static_cast<uint32>(srcImageInfo.swizzledFormat.format) -
        static_cast<uint32>(ChNumFormat::AYUV)];

    PAL_ASSERT(static_cast<uint32>(cscInfo.pipelineYuvToRgb) != 0);

    // NOTE: Each of the YUV --> RGB conversion shaders expects the following user-data layout:
    //  o RGB destination Image
    //  o YUV source Image's Y plane (or YCbCr plane for RGB --> YUV-packed conversions)
    //  o YUV source Image's Cb or CbCr plane (unused for RGB --> YUV-packed conversions)
    //  o YUV source Image's Cr plane (unused unless converting between YV12 and RGB)
    //  o Image sampler for scaled copies
    //  o Copy Info constant buffer
    //  o Color-space Conversion Table constant buffer

    constexpr uint32 MaxImageSrds = 4;
    constexpr uint32 MaxTotalSrds = (MaxImageSrds + 1);

    const uint32 viewCount =
        (cscInfo.pipelineYuvToRgb == RpmComputePipeline::YuvToRgb) ? MaxImageSrds : (MaxImageSrds - 1);

    ImageViewInfo viewInfo[MaxImageSrds] = { };

    // Override the RGB image format to skip gamma-correction if it is required.
    SwizzledFormat dstFormat = dstImageInfo.swizzledFormat;

    if (Formats::IsSrgb(dstFormat.format))
    {
        dstFormat.format = Formats::ConvertToUnorm(dstFormat.format);
    }

    const ComputePipeline*const pPipeline       = GetPipeline(cscInfo.pipelineYuvToRgb);
    const DispatchDims          threadsPerGroup = pPipeline->ThreadsPerGroupXyz();

    pCmdBuffer->CmdSaveComputeState(ComputeStateFlags::ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        ColorSpaceConversionRegion region = pRegions[idx];
        if ((region.dstExtent.width == 0) || (region.dstExtent.height == 0))
        {
            continue;   // Skip empty regions.
        }

        const SubresRange dstRange = { region.rgbSubres, 1, 1, region.sliceCount };
        RpmUtil::BuildImageViewInfo(&viewInfo[0],
                                    dstImage,
                                    dstRange,
                                    dstFormat,
                                    RpmUtil::DefaultRpmLayoutShaderWrite,
                                    device.TexOptLevel(),
                                    true);

        for (uint32 view = 1; view < viewCount; ++view)
        {
            const auto&       cscViewInfo         = cscInfo.viewInfoYuvToRgb[view - 1];
            SwizzledFormat    imageViewInfoFormat = cscViewInfo.swizzledFormat;
            const SubresRange srcRange            =
                { { cscViewInfo.plane, 0, region.yuvStartSlice }, 1, 1, region.sliceCount };
            // Fall back if we can't use MM formats for YUV planes
            RpmUtil::SwapIncompatibleMMFormat(srcImage.GetDevice(), &imageViewInfoFormat);
            RpmUtil::BuildImageViewInfo(&viewInfo[view],
                                        srcImage,
                                        srcRange,
                                        imageViewInfoFormat,
                                        RpmUtil::DefaultRpmLayoutRead,
                                        device.TexOptLevel(),
                                        false);
        }

        // Calculate the absolute value of dstExtent, which will get fed to the shader.
        copyInfo.dstExtent.width  = Math::Absu(region.dstExtent.width);
        copyInfo.dstExtent.height = Math::Absu(region.dstExtent.height);
        copyInfo.dstOffset.x      = region.dstOffset.x;
        copyInfo.dstOffset.y      = region.dstOffset.y;

        // A negative extent means that we should reverse the copy direction. We want to always use the absolute
        // value of dstExtent, otherwise the compute shader can't handle it. If dstExtent is negative in one
        // dimension, then we negate srcExtent in that dimension, and we adjust the offsets as well.
        if (region.dstExtent.width < 0)
        {
            copyInfo.dstOffset.x   = (region.dstOffset.x + region.dstExtent.width);
            region.srcOffset.x     = (region.srcOffset.x + region.srcExtent.width);
            region.srcExtent.width = -region.srcExtent.width;
        }

        if (region.dstExtent.height < 0)
        {
            copyInfo.dstOffset.y    = (region.dstOffset.y + region.dstExtent.height);
            region.srcOffset.y      = (region.srcOffset.y + region.srcExtent.height);
            region.srcExtent.height = -region.srcExtent.height;
        }

        // The shaders expect the source copy region to be specified in normalized texture coordinates.
        const Extent3d& srcExtent = srcImage.SubresourceInfo(0)->extentTexels;

        copyInfo.srcLeft   = (static_cast<float>(region.srcOffset.x) / srcExtent.width);
        copyInfo.srcTop    = (static_cast<float>(region.srcOffset.y) / srcExtent.height);
        copyInfo.srcRight  = (static_cast<float>(region.srcOffset.x + region.srcExtent.width) / srcExtent.width);
        copyInfo.srcBottom = (static_cast<float>(region.srcOffset.y + region.srcExtent.height) / srcExtent.height);

        PAL_ASSERT((copyInfo.srcLeft   >= 0.0f) && (copyInfo.srcLeft   <= 1.0f) &&
                   (copyInfo.srcTop    >= 0.0f) && (copyInfo.srcTop    <= 1.0f) &&
                   (copyInfo.srcRight  >= 0.0f) && (copyInfo.srcRight  <= 1.0f) &&
                   (copyInfo.srcBottom >= 0.0f) && (copyInfo.srcBottom <= 1.0f));

        // Each conversion shader requires:
        //  o Four image SRD's: one for the RGB image, one each for the Y, U and V "planes" of the YUV image
        //  o One sampler SRD
        //  o Inline constant space for copyInfo
        const uint32 sizeInDwords = (SrdDwordAlignment() * MaxTotalSrds) + RpmUtil::YuvRgbConversionInfoDwords;
        uint32* pUserData = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   sizeInDwords,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        device.CreateImageViewSrds(viewCount, &viewInfo[0], pUserData);
        pUserData += (SrdDwordAlignment() * MaxImageSrds);

        device.CreateSamplerSrds(1, &sampler, pUserData);
        pUserData += SrdDwordAlignment();

        memcpy(pUserData, &copyInfo, sizeof(copyInfo));

        // Finally, issue the dispatch. The shaders need one thread per texel.
        const DispatchDims threads = {copyInfo.dstExtent.width, copyInfo.dstExtent.height, region.sliceCount};

        pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroupsXyz(threads, threadsPerGroup));
    } // End loop over regions

    pCmdBuffer->CmdRestoreComputeState(ComputeStateFlags::ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Builds commands to execute a color-space-conversion copy from a RGB source to an YUV destination.
void RsrcProcMgr::ConvertRgbToYuv(
    GfxCmdBuffer*                     pCmdBuffer,
    const Image&                      srcImage,
    const Image&                      dstImage,
    uint32                            regionCount,
    const ColorSpaceConversionRegion* pRegions,
    const SamplerInfo&                sampler,
    const ColorSpaceConversionTable&  cscTable
    ) const
{
    const auto& device       = *m_pDevice->Parent();
    const auto& srcImageInfo = srcImage.GetImageCreateInfo();
    const auto& dstImageInfo = dstImage.GetImageCreateInfo();

    const RpmUtil::ColorSpaceConversionInfo& cscInfo =
        RpmUtil::CscInfoTable[static_cast<uint32>(dstImageInfo.swizzledFormat.format) -
                              static_cast<uint32>(ChNumFormat::AYUV)];
    PAL_ASSERT(static_cast<uint32>(cscInfo.pipelineRgbToYuv) != 0);

    // NOTE: Each of the RGB --> YUV conversion shaders expects the following user-data layout:
    //  o RGB source Image
    //  o YUV destination Image plane
    //  o Image sampler for scaled copies
    //  o Copy Info constant buffer
    //  o Color-space Conversion Table constant buffer
    //
    // The conversion is done in multiple passes for YUV planar destinations, one pass per plane. This is done so that
    // the planes can sample the source Image at different rates (because planes often have differing dimensions).
    const uint32 passCount = static_cast<uint32>(dstImage.GetImageInfo().numPlanes);

    const ComputePipeline*const pPipeline       = GetPipeline(cscInfo.pipelineRgbToYuv);
    const DispatchDims          threadsPerGroup = pPipeline->ThreadsPerGroupXyz();

    pCmdBuffer->CmdSaveComputeState(ComputeStateFlags::ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        ColorSpaceConversionRegion region = pRegions[idx];
        if ((region.dstExtent.width == 0) || (region.dstExtent.height == 0))
        {
            continue;   // Skip empty regions.
        }

        constexpr uint32 MaxImageSrds = 2;
        constexpr uint32 MaxTotalSrds = (MaxImageSrds + 1);

        ImageViewInfo viewInfo[MaxImageSrds] = { };

        // Override the RGB image format to skip degamma.
        SwizzledFormat srcFormat = srcImageInfo.swizzledFormat;

        if (Formats::IsSrgb(srcFormat.format))
        {
            srcFormat.format = Formats::ConvertToUnorm(srcFormat.format);
        }

        const SubresRange srcRange = { region.rgbSubres, 1, 1, region.sliceCount };
        RpmUtil::BuildImageViewInfo(&viewInfo[0],
                                    srcImage,
                                    srcRange,
                                    srcFormat,
                                    RpmUtil::DefaultRpmLayoutRead,
                                    device.TexOptLevel(),
                                    false);

        RpmUtil::RgbYuvConversionInfo copyInfo = { };

        // Calculate the absolute value of dstExtent, which will get fed to the shader.
        const Extent2d dstExtent = { Math::Absu(region.dstExtent.width), Math::Absu(region.dstExtent.height) };
        Offset2d dstOffset = region.dstOffset;

        // A negative extent means that we should reverse the copy direction. We want to always use the absolute
        // value of dstExtent, otherwise the compute shader can't handle it. If dstExtent is negative in one
        // dimension, then we negate srcExtent in that dimension, and we adjust the offsets as well.
        if (region.dstExtent.width < 0)
        {
            dstOffset.x            = (region.dstOffset.x + region.dstExtent.width);
            region.srcOffset.x     = (region.srcOffset.x + region.srcExtent.width);
            region.srcExtent.width = -region.srcExtent.width;
        }

        if (region.dstExtent.height < 0)
        {
            dstOffset.y             = (region.dstOffset.y + region.dstExtent.height);
            region.srcOffset.y      = (region.srcOffset.y + region.srcExtent.height);
            region.srcExtent.height = -region.srcExtent.height;
        }

        // The shaders expect the source copy region to be specified in normalized texture coordinates.
        const Extent3d& srcExtent = srcImage.SubresourceInfo(0)->extentTexels;

        copyInfo.srcLeft   = (static_cast<float>(region.srcOffset.x) / srcExtent.width);
        copyInfo.srcTop    = (static_cast<float>(region.srcOffset.y) / srcExtent.height);
        copyInfo.srcRight  = (static_cast<float>(region.srcOffset.x + region.srcExtent.width) / srcExtent.width);
        copyInfo.srcBottom = (static_cast<float>(region.srcOffset.y + region.srcExtent.height) / srcExtent.height);

        // Writing to macro-pixel YUV destinations requires the distance between the two source pixels which form
        // the destination macro-pixel (in normalized texture coordinates).
        copyInfo.srcWidthEpsilon = (1.f / srcExtent.width);

        PAL_ASSERT((copyInfo.srcLeft   >= 0.0f) && (copyInfo.srcLeft   <= 1.0f) &&
                   (copyInfo.srcTop    >= 0.0f) && (copyInfo.srcTop    <= 1.0f) &&
                   (copyInfo.srcRight  >= 0.0f) && (copyInfo.srcRight  <= 1.0f) &&
                   (copyInfo.srcBottom >= 0.0f) && (copyInfo.srcBottom <= 1.0f));

        if (cscInfo.pipelineRgbToYuv == RpmComputePipeline::RgbToYuvPacked)
        {
            // The YUY2 and YVY2 formats have the packing of components in a macro-pixel reversed compared to the
            // UYVY and VYUY formats.
            copyInfo.reversePacking = ((dstImageInfo.swizzledFormat.format == ChNumFormat::YUY2) ||
                                       (dstImageInfo.swizzledFormat.format == ChNumFormat::YVY2));
        }

        // Perform one conversion pass per plane of the YUV destination.
        for (uint32 pass = 0; pass < passCount; ++pass)
        {
            const auto&       cscViewInfo         = cscInfo.viewInfoRgbToYuv[pass];
            SwizzledFormat    imageViewInfoFormat = cscViewInfo.swizzledFormat;
            const SubresRange dstRange            =
                { { cscViewInfo.plane, 0, region.yuvStartSlice }, 1, 1, region.sliceCount };
            // Fall back if we can't use MM formats for YUV planes
            RpmUtil::SwapIncompatibleMMFormat(dstImage.GetDevice(), &imageViewInfoFormat);
            RpmUtil::BuildImageViewInfo(&viewInfo[1],
                                        dstImage,
                                        dstRange,
                                        imageViewInfoFormat,
                                        RpmUtil::DefaultRpmLayoutShaderWrite,
                                        device.TexOptLevel(),
                                        true);

            // Build RGB to YUV color-space-conversion table constant buffer.
            RpmUtil::SetupRgbToYuvCscTable(dstImageInfo.swizzledFormat.format, pass, cscTable, &copyInfo);

            // The destination offset and extent need to be adjusted to account for differences in the dimensions of
            // the YUV image's planes.
            Extent3d log2Ratio = Formats::Log2SubsamplingRatio(dstImageInfo.swizzledFormat.format, cscViewInfo.plane);
            if (cscInfo.pipelineRgbToYuv == RpmComputePipeline::RgbToYuvPacked)
            {
                // For YUV formats which are macro-pixel packed, we run a special shader which outputs two pixels
                // (one macro-pxiel) per thread. Therefore, we must adjust the destination region accordingly, even
                // though the planar subsampling ratio would normally be treated as 1:1.
                log2Ratio.width  = 1;
                log2Ratio.height = 0;
            }

            copyInfo.dstOffset.x      = (dstOffset.x      >> log2Ratio.width);
            copyInfo.dstOffset.y      = (dstOffset.y      >> log2Ratio.height);
            copyInfo.dstExtent.width  = (dstExtent.width  >> log2Ratio.width);
            copyInfo.dstExtent.height = (dstExtent.height >> log2Ratio.height);

            // Each codec(Mpeg-1, Mpeg-2) requires the specific chroma subsampling location.
            copyInfo.sampleLocX = cscViewInfo.sampleLocX;
            copyInfo.sampleLocY = cscViewInfo.sampleLocY;

            // Each conversion shader requires:
            //  o Two image SRD's: one for the RGB image, one for the YUV image
            //  o One sampler SRD
            //  o Inline constant space for copyInfo
            const uint32 sizeInDwords = (SrdDwordAlignment() * MaxTotalSrds) + RpmUtil::YuvRgbConversionInfoDwords;
            uint32* pUserData = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                       sizeInDwords,
                                                                       SrdDwordAlignment(),
                                                                       PipelineBindPoint::Compute,
                                                                       0);

            device.CreateImageViewSrds(MaxImageSrds, &viewInfo[0], pUserData);
            pUserData += (SrdDwordAlignment() * MaxImageSrds);

            device.CreateSamplerSrds(1, &sampler, pUserData);
            pUserData += SrdDwordAlignment();

            memcpy(pUserData, &copyInfo, sizeof(copyInfo));

            // Finally, issue the dispatch. The shaders need one thread per texel.
            const DispatchDims threads = {copyInfo.dstExtent.width, copyInfo.dstExtent.height, region.sliceCount};

            pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroupsXyz(threads, threadsPerGroup));
        } // End loop over per-plane passes
    } // End loop over regions

    pCmdBuffer->CmdRestoreComputeState(ComputeStateFlags::ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Builds commands to fill every DWORD of the memory object with 'data' between dstOffset and (dstOffset + fillSize).
// The offset and fill size must be DWORD aligned.
void RsrcProcMgr::CmdFillMemory(
    GfxCmdBuffer*    pCmdBuffer,
    bool             saveRestoreComputeState,
    const GpuMemory& dstGpuMemory,
    gpusize          dstOffset,
    gpusize          fillSize,
    uint32           data
    ) const
{
    const gpusize dstGpuVirtAddr = (dstGpuMemory.Desc().gpuVirtAddr + dstOffset);
    CmdFillMemory(pCmdBuffer, saveRestoreComputeState, dstGpuVirtAddr, fillSize, data);
}

// =====================================================================================================================
// Builds commands to fill every DWORD of memory with 'data' between dstGpuVirtAddr and (dstOffset + fillSize).
// The offset and fill size must be DWORD aligned.
void RsrcProcMgr::CmdFillMemory(
    GfxCmdBuffer* pCmdBuffer,
    bool          saveRestoreComputeState,
    gpusize       dstGpuVirtAddr,
    gpusize       fillSize,
    uint32        data
    ) const
{
    PAL_ASSERT(IsPow2Aligned(dstGpuVirtAddr, sizeof(uint32)));
    PAL_ASSERT(IsPow2Aligned(fillSize, sizeof(uint32)));

    constexpr gpusize FillSizeLimit = 256_MiB;

    const Device*const pDevice               = m_pDevice->Parent();
    const PalPublicSettings* pPublicSettings = pDevice->GetPublicSettings();

    if (saveRestoreComputeState)
    {
        // Save the command buffer's state.
        pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    }

    for (gpusize fillOffset = 0; fillOffset < fillSize; fillOffset += FillSizeLimit)
    {
        const uint32 numDwords = static_cast<uint32>(Min(FillSizeLimit, (fillSize - fillOffset)) / sizeof(uint32));

        // ((FillSizeLimit % 4) == 0) as the value stands now, ensuring fillSize is 4xOptimized too. If we change it
        // to something that doesn't satisfy this condition we would need to check ((fillSize - fillOffset) % 4) too.
        const bool is4xOptimized = ((numDwords % 4) == 0);

        // There is a specialized pipeline which is more efficient when the fill size is a multiple of 4 DWORDs.
        const ComputePipeline*const pPipeline = is4xOptimized
            ? GetPipeline(RpmComputePipeline::FillMem4xDword)
            : GetPipeline(RpmComputePipeline::FillMemDword);

        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

        uint32 srd[4] = { };
        PAL_ASSERT(pDevice->ChipProperties().srdSizes.bufferView == sizeof(srd));

        BufferViewInfo dstBufferView = {};
        dstBufferView.gpuAddr = dstGpuVirtAddr + fillOffset;
        dstBufferView.range   = numDwords * sizeof(uint32);
        dstBufferView.stride  = (is4xOptimized) ? (sizeof(uint32) * 4) : sizeof(uint32);
        if (is4xOptimized)
        {
            dstBufferView.swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
            dstBufferView.swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };
        }
        else
        {
            dstBufferView.swizzledFormat.format  = ChNumFormat::X32_Uint;
            dstBufferView.swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };
        }
        dstBufferView.flags.bypassMallRead  = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                             RpmViewsBypassMallOnRead);
        dstBufferView.flags.bypassMallWrite = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                             RpmViewsBypassMallOnWrite);
        pDevice->CreateTypedBufferViewSrds(1, &dstBufferView, &srd[0]);

        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 0, 4, &srd[0]);
        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 4, 1, &data);

        // Issue a dispatch with one thread per DWORD.
        const uint32 minThreads   = (is4xOptimized) ? (numDwords / 4) : numDwords;
        const uint32 threadGroups = RpmUtil::MinThreadGroups(minThreads, pPipeline->ThreadsPerGroup());
        pCmdBuffer->CmdDispatch({threadGroups, 1, 1});
    }
    if (saveRestoreComputeState)
    {
        // Restore the command buffer's state.
        pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
    }
}

// =====================================================================================================================
// Builds commands to clear the specified ranges of the current depth stencil attachment views to the specified values.
void RsrcProcMgr::CmdClearBoundDepthStencilTargets(
    GfxCmdBuffer*                 pCmdBuffer,
    float                         depth,
    uint8                         stencil,
    uint8                         stencilWriteMask,
    uint32                        samples,
    uint32                        fragments,
    DepthStencilSelectFlags       flag,
    uint32                        regionCount,
    const ClearBoundTargetRegion* pClearRegions
    ) const
{
    PAL_ASSERT(regionCount > 0);

    StencilRefMaskParams stencilRefMasks = { };
    stencilRefMasks.flags.u8All    = 0xFF;
    stencilRefMasks.frontRef       = stencil;
    stencilRefMasks.frontReadMask  = 0xFF;
    stencilRefMasks.frontWriteMask = stencilWriteMask;
    stencilRefMasks.backRef        = stencil;
    stencilRefMasks.backReadMask   = 0xFF;
    stencilRefMasks.backWriteMask  = stencilWriteMask;

    ViewportParams viewportInfo = { };
    viewportInfo.count = 1;
    viewportInfo.viewports[0].originX  = 0;
    viewportInfo.viewports[0].originY  = 0;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo = { };
    scissorInfo.count = 1;
    scissorInfo.scissors[0].offset.x = 0;
    scissorInfo.scissors[0].offset.y = 0;

    // Save current command buffer state and bind graphics state which is common for all mipmap levels.
    pCmdBuffer->CmdSaveGraphicsState();
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, GetGfxPipeline(DepthSlowDraw), InternalApiPsoHash, });
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(samples, fragments));
    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    if ((flag.depth != 0) && (flag.stencil != 0))
    {
        pCmdBuffer->CmdBindDepthStencilState(m_pDepthStencilClearState);
    }
    else if (flag.depth != 0)
    {
        pCmdBuffer->CmdBindDepthStencilState(m_pDepthClearState);
    }
    else if (flag.stencil != 0)
    {
        pCmdBuffer->CmdBindDepthStencilState(m_pStencilClearState);
    }

    // All mip levels share the same depth export value, so only need to do it once.
    RpmUtil::WriteVsZOut(pCmdBuffer, depth);

    for (uint32 scissorIndex = 0; scissorIndex < regionCount; ++scissorIndex)
    {
        // Note: we should clear the same range of slices for depth and/or stencil attachment. If this
        // requirement needs to be relaxed, we need to separate the draws for depth clear and stencil clear.
        RpmUtil::WriteVsFirstSliceOffset(pCmdBuffer, pClearRegions[scissorIndex].startSlice);

        viewportInfo.viewports[0].originX = static_cast<float>(pClearRegions[scissorIndex].rect.offset.x);
        viewportInfo.viewports[0].originY = static_cast<float>(pClearRegions[scissorIndex].rect.offset.y);
        viewportInfo.viewports[0].width   = static_cast<float>(pClearRegions[scissorIndex].rect.extent.width);
        viewportInfo.viewports[0].height  = static_cast<float>(pClearRegions[scissorIndex].rect.extent.height);

        pCmdBuffer->CmdSetViewports(viewportInfo);

        scissorInfo.scissors[0].offset.x      = pClearRegions[scissorIndex].rect.offset.x;
        scissorInfo.scissors[0].offset.y      = pClearRegions[scissorIndex].rect.offset.y;
        scissorInfo.scissors[0].extent.width  = pClearRegions[scissorIndex].rect.extent.width;
        scissorInfo.scissors[0].extent.height = pClearRegions[scissorIndex].rect.extent.height;

        pCmdBuffer->CmdSetScissorRects(scissorInfo);

        // Draw numSlices fullscreen instanced quads.
        pCmdBuffer->CmdDraw(0, 3, 0, pClearRegions[scissorIndex].numSlices, 0);
    }

    // Restore original command buffer state and destroy the depth/stencil state.
    pCmdBuffer->CmdRestoreGraphicsState();
}

// =====================================================================================================================
// Builds commands to clear the existing color attachment in the command buffer to the given color data.
void RsrcProcMgr::CmdClearBoundColorTargets(
    GfxCmdBuffer*                   pCmdBuffer,
    uint32                          colorTargetCount,
    const BoundColorTarget*         pBoundColorTargets,
    uint32                          regionCount,
    const ClearBoundTargetRegion*   pClearRegions
    ) const
{
    // for attachment, clear region comes from boxes. So regionCount has to be valid
    PAL_ASSERT(regionCount > 0);

    ViewportParams viewportInfo = { };
    viewportInfo.count                 = 1;
    viewportInfo.viewports[0].originX  = 0;
    viewportInfo.viewports[0].originY  = 0;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo = { };
    scissorInfo.count = 1;

    // Save current command buffer state and bind graphics state which is common for all mipmap levels.
    pCmdBuffer->CmdSaveGraphicsState();
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindColorBlendState(m_pBlendDisableState);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthDisableState);

    for (uint32 colorIndex = 0; colorIndex < colorTargetCount; ++colorIndex)
    {
        uint32 convertedColor[4] = {0};

        if (pBoundColorTargets[colorIndex].clearValue.type == ClearColorType::Float)
        {
            Formats::ConvertColor(pBoundColorTargets[colorIndex].swizzledFormat,
                                  &pBoundColorTargets[colorIndex].clearValue.f32Color[0],
                                  &convertedColor[0]);
        }
        else
        {
            memcpy(&convertedColor[0], &pBoundColorTargets[colorIndex].clearValue.u32Color[0], sizeof(convertedColor));
        }

        const GraphicsPipeline* pPipeline =
            GetGfxPipelineByTargetIndexAndFormat(SlowColorClear0_32ABGR,
                                                 pBoundColorTargets[colorIndex].targetIndex,
                                                 pBoundColorTargets[colorIndex].swizzledFormat);

        pCmdBuffer->CmdBindPipelineWithOverrides({ PipelineBindPoint::Graphics, pPipeline, InternalApiPsoHash, },
                                                 pBoundColorTargets[colorIndex].swizzledFormat,
                                                 pBoundColorTargets[colorIndex].targetIndex);

        pCmdBuffer->CmdBindMsaaState(GetMsaaState(pBoundColorTargets[colorIndex].samples,
                                                  pBoundColorTargets[colorIndex].fragments));

        RpmUtil::WriteVsZOut(pCmdBuffer, 1.0f);

        RpmUtil::ConvertClearColorToNativeFormat(pBoundColorTargets[colorIndex].swizzledFormat,
                                                 pBoundColorTargets[colorIndex].swizzledFormat,
                                                 &convertedColor[0]);

        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, RpmPsClearFirstUserData, 4, &convertedColor[0]);

        for (uint32 scissorIndex = 0; scissorIndex < regionCount; ++scissorIndex)
        {
            RpmUtil::WriteVsFirstSliceOffset(pCmdBuffer, pClearRegions[scissorIndex].startSlice);

            viewportInfo.viewports[0].originX   = static_cast<float>(pClearRegions[scissorIndex].rect.offset.x);
            viewportInfo.viewports[0].originY   = static_cast<float>(pClearRegions[scissorIndex].rect.offset.y);
            viewportInfo.viewports[0].width     = static_cast<float>(pClearRegions[scissorIndex].rect.extent.width);
            viewportInfo.viewports[0].height    = static_cast<float>(pClearRegions[scissorIndex].rect.extent.height);

            pCmdBuffer->CmdSetViewports(viewportInfo);

            // Create a scissor state for this mipmap level, slice, and current scissor.
            scissorInfo.scissors[0].offset.x        = pClearRegions[scissorIndex].rect.offset.x;
            scissorInfo.scissors[0].offset.y        = pClearRegions[scissorIndex].rect.offset.y;
            scissorInfo.scissors[0].extent.width    = pClearRegions[scissorIndex].rect.extent.width;
            scissorInfo.scissors[0].extent.height   = pClearRegions[scissorIndex].rect.extent.height;

            pCmdBuffer->CmdSetScissorRects(scissorInfo);

            // Draw numSlices fullscreen instanced quads.
            pCmdBuffer->CmdDraw(0, 3, 0, pClearRegions[scissorIndex].numSlices, 0);
        }
    }

    // Restore original command buffer state.
    pCmdBuffer->CmdRestoreGraphicsState();
}

// =====================================================================================================================
// Builds commands to clear the specified ranges of an image to the given color data.
void RsrcProcMgr::CmdClearColorImage(
    GfxCmdBuffer*         pCmdBuffer,
    const Image&          dstImage,
    ImageLayout           dstImageLayout,
    const ClearColor&     color,
    const SwizzledFormat& clearFormat,
    uint32                rangeCount,
    const SubresRange*    pRanges,
    uint32                boxCount,
    const Box*            pBoxes,
    uint32                flags
    ) const
{
    const bool skipIfSlow           = TestAnyFlagSet(flags, ColorClearSkipIfSlow);
    const bool needComputeClearSync = TestAnyFlagSet(flags, ColorClearAutoSync) && (skipIfSlow == false);

    if (needComputeClearSync)
    {
        AcquireReleaseInfo acqRelInfo = {};
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        acqRelInfo.srcGlobalStageMask  = PipelineStageColorTarget;
        acqRelInfo.dstGlobalStageMask  = PipelineStageCs;
#else
        acqRelInfo.srcStageMask        = PipelineStageColorTarget;
        acqRelInfo.dstStageMask        = PipelineStageCs;
#endif
        acqRelInfo.srcGlobalAccessMask = CoherColorTarget;
        acqRelInfo.dstGlobalAccessMask = CoherShader;
        acqRelInfo.reason              = Developer::BarrierReasonPreComputeColorClear;

        pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
    }

    for (uint32 rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx)
    {
        PAL_ASSERT(pRanges[rangeIdx].numPlanes == 1);

        if ((pRanges[rangeIdx].numMips != 0) && (skipIfSlow == false))
        {
            SlowClearCompute(pCmdBuffer,
                             dstImage,
                             dstImageLayout,
                             &color,
                             clearFormat,
                             pRanges[rangeIdx],
                             boxCount,
                             pBoxes);
        }
    }

    if (needComputeClearSync)
    {
        AcquireReleaseInfo acqRelInfo = {};
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        acqRelInfo.srcGlobalStageMask  = PipelineStageCs;
        acqRelInfo.dstGlobalStageMask  = PipelineStageColorTarget;
#else
        acqRelInfo.srcStageMask        = PipelineStageCs;
        acqRelInfo.dstStageMask        = PipelineStageColorTarget;
#endif
        acqRelInfo.srcGlobalAccessMask = CoherShader;
        acqRelInfo.dstGlobalAccessMask = CoherColorTarget;
        acqRelInfo.reason              = Developer::BarrierReasonPostComputeColorClear;

        pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
    }
}

// =====================================================================================================================
// Builds commands to clear the specified ranges of a depth/stencil image to the specified values.
void RsrcProcMgr::CmdClearDepthStencil(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    ImageLayout        depthLayout,
    ImageLayout        stencilLayout,
    float              depth,
    uint8              stencil,
    uint8              stencilWriteMask,
    uint32             rangeCount,
    const SubresRange* pRanges,
    uint32             rectCount,
    const Rect*        pRects,
    uint32             flags
    ) const
{
    PAL_ASSERT((rectCount == 0) || (pRects != nullptr));

    // Convert the Rects to Boxes. We use an AutoBuffer instead of the virtual linear allocator because
    // we may need to allocate more boxes than will fit in the fixed virtual space.
    AutoBuffer<Box, 16, Platform> boxes(rectCount, m_pDevice->GetPlatform());

    // Notify the command buffer if AutoBuffer allocation has failed.
    if (boxes.Capacity() < rectCount)
    {
        pCmdBuffer->NotifyAllocFailure();
    }
    else
    {
        const bool         needComputeClearSync = TestAnyFlagSet(flags, ColorClearAutoSync);
        const ChNumFormat& imageFormat          = dstImage.GetImageCreateInfo().swizzledFormat.format;
        const bool         supportsDepth        = m_pDevice->Parent()->SupportsDepth(imageFormat, ImageTiling::Optimal);

        if (needComputeClearSync)
        {
            AcquireReleaseInfo acqRelInfo = {};
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
            acqRelInfo.srcGlobalStageMask  = PipelineStageEarlyDsTarget | PipelineStageLateDsTarget;
            acqRelInfo.dstGlobalStageMask  = PipelineStageCs;
#else
            acqRelInfo.srcStageMask        = PipelineStageEarlyDsTarget | PipelineStageLateDsTarget;
            acqRelInfo.dstStageMask        = PipelineStageCs;
#endif
            acqRelInfo.srcGlobalAccessMask = CoherDepthStencilTarget;
            acqRelInfo.dstGlobalAccessMask = CoherShader;
            acqRelInfo.reason              = Developer::BarrierReasonPreComputeDepthStencilClear;

            pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
        }

        for (uint32 i = 0; i < rectCount; i++)
        {
            boxes[i].offset.x      = pRects[i].offset.x;
            boxes[i].offset.y      = pRects[i].offset.y;
            boxes[i].offset.z      = 0;
            boxes[i].extent.width  = pRects[i].extent.width;
            boxes[i].extent.height = pRects[i].extent.height;
            boxes[i].extent.depth  = 1;
        }

        for (uint32 rangeIdx = 0; rangeIdx < rangeCount; rangeIdx++)
        {
            for (uint32 plane = 0; plane < pRanges[rangeIdx].numPlanes; plane++)
            {
                SubresRange range = pRanges[rangeIdx];

                range.startSubres.plane += plane;
                range.numPlanes          = 1;

                const bool            isDepth       = (range.startSubres.plane == 0) && supportsDepth;
                const SwizzledFormat& subresFormat  = dstImage.SubresourceInfo(range.startSubres)->format;

                ClearColor clearColor = {};

                if (isDepth)
                {
                    // For Depth slow clears, we use a float clear color.
                    clearColor.type        = ClearColorType::Float;
                    clearColor.f32Color[0] = depth;
                }
                else
                {
                    PAL_ASSERT(m_pDevice->Parent()->SupportsStencil(imageFormat, ImageTiling::Optimal));

                    // For Stencil plane we use the stencil value directly.
                    clearColor.type        = ClearColorType::Uint;
                    clearColor.u32Color[0] = stencil;
                }

                // This avoids an assert in the generic clear function below.  I think it's safe to add here without
                // a real transition because, by the time we get here, there is no htile.
                depthLayout.usages |= LayoutShaderWrite;

                SlowClearCompute(pCmdBuffer,
                                 dstImage,
                                 isDepth ? depthLayout : stencilLayout,
                                 &clearColor,
                                 subresFormat,
                                 range,
                                 rectCount,
                                 &boxes[0]);
            }
        }

        if (needComputeClearSync)
        {
            AcquireReleaseInfo acqRelInfo = {};
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
            acqRelInfo.srcGlobalStageMask  = PipelineStageCs;
            acqRelInfo.dstGlobalStageMask  = PipelineStageEarlyDsTarget | PipelineStageLateDsTarget;
#else
            acqRelInfo.srcStageMask        = PipelineStageCs;
            acqRelInfo.dstStageMask        = PipelineStageEarlyDsTarget | PipelineStageLateDsTarget;
#endif

            acqRelInfo.srcGlobalAccessMask = CoherShader;
            acqRelInfo.dstGlobalAccessMask = CoherDepthStencilTarget;
            acqRelInfo.reason              = Developer::BarrierReasonPostComputeDepthStencilClear;

            pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
        }
    }
}

// =====================================================================================================================
// Builds commands to slow clear a range of an image to the given raw color data using a compute shader.
void RsrcProcMgr::SlowClearCompute(
    GfxCmdBuffer*         pCmdBuffer,
    const Image&          dstImage,
    ImageLayout           dstImageLayout,
    const ClearColor*     pColor,
    const SwizzledFormat& clearFormat,
    const SubresRange&    clearRange,
    uint32                boxCount,
    const Box*            pBoxes
    ) const
{
    PAL_ASSERT(clearRange.numPlanes == 1);
    // If the image isn't in a layout that allows format replacement this clear path won't work.
    PAL_ASSERT(dstImage.GetGfxImage()->IsFormatReplaceable(clearRange.startSubres, dstImageLayout, true));

    // Get some useful information about the image.
    const auto&     createInfo   = dstImage.GetImageCreateInfo();
    const ImageType imageType    = dstImage.GetGfxImage()->GetOverrideImageType();
    uint32          texelScale   = 1;
    uint32          texelShift   = 0;
    bool            singleSubRes = false;

    const auto&    subresInfo = *dstImage.SubresourceInfo(clearRange.startSubres);
    const SwizzledFormat baseFormat = clearFormat.format == ChNumFormat::Undefined ? subresInfo.format : clearFormat;
    SwizzledFormat viewFormat = RpmUtil::GetRawFormat(baseFormat.format, &texelScale, &singleSubRes);

    // For packed YUV image use X32_Uint instead of X16_Uint to fill with YUYV.
    if ((viewFormat.format == ChNumFormat::X16_Uint) && Formats::IsYuvPacked(baseFormat.format))
    {
        viewFormat.format  = ChNumFormat::X32_Uint;
        viewFormat.swizzle = { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };
        // The extent and offset need to be adjusted to 1/2 size.
        texelShift         = (pColor->type == ClearColorType::Yuv) ? 1 : 0;
    }

    // These are the only two supported texel scales
    PAL_ASSERT((texelScale == 1) || (texelScale == 3));

    // Get the appropriate pipeline.
    auto pipelineEnum = RpmComputePipeline::Count;
    switch (imageType)
    {
    case ImageType::Tex1d:
        pipelineEnum = ((texelScale == 1)
                        ? RpmComputePipeline::ClearImage1d
                        : RpmComputePipeline::ClearImage1dTexelScale);
        break;

    case ImageType::Tex2d:
        pipelineEnum = ((texelScale == 1)
                        ? RpmComputePipeline::ClearImage2d
                        : RpmComputePipeline::ClearImage2dTexelScale);
        break;

    default:
        pipelineEnum = ((texelScale == 1)
                        ? RpmComputePipeline::ClearImage3d
                        : RpmComputePipeline::ClearImage3dTexelScale);
        break;
    }

    const ComputePipeline*  pPipeline       = GetPipeline(pipelineEnum);
    const DispatchDims      threadsPerGroup = pPipeline->ThreadsPerGroupXyz();

    // Save current command buffer state and bind the pipeline.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    // Pack the clear color into the raw format and write it to user data 1-4.
    uint32 packedColor[4] = {0};

    uint32 convertedColor[4] = {0};
    if (pColor->type == ClearColorType::Yuv)
    {
        // If clear color type is Yuv, the image format should used to determine the clear color swizzling and packing
        // for planar YUV formats since the baseFormat is subresource's format which is not a YUV format.
        // NOTE: if clear color type is Uint, the client is responsible for:
        //       1. packing and swizzling clear color for packed YUV formats (e.g. packing in YUYV order for YUY2)
        //       2. passing correct clear color for this plane for planar YUV formats (e.g. two uint32s for U and V if
        //          current plane is CbCr).
        const SwizzledFormat imgFormat = createInfo.swizzledFormat;
        Formats::ConvertYuvColor(imgFormat, clearRange.startSubres.plane, &pColor->u32Color[0], &packedColor[0]);
    }
    else
    {
        if (pColor->type == ClearColorType::Float)
        {
            Formats::ConvertColor(baseFormat, &pColor->f32Color[0], &convertedColor[0]);
        }
        else
        {
            memcpy(&convertedColor[0], &pColor->u32Color[0], sizeof(convertedColor));
        }

        uint32 swizzledColor[4] = {0};
        Formats::SwizzleColor(baseFormat, &convertedColor[0], &swizzledColor[0]);
        Formats::PackRawClearColor(baseFormat, &swizzledColor[0], &packedColor[0]);
    }

    // The color is constant for all dispatches so we can embed it in the fast user-data right now.
    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 1, ArrayLen32(packedColor), packedColor);

    // Split the clear range into sections with constant mip/array levels and loop over them.
    SubresRange  singleMipRange = { clearRange.startSubres, 1, 1, clearRange.numSlices };
    const uint32 firstMipLevel  = clearRange.startSubres.mipLevel;
    const uint32 lastMipLevel   = clearRange.startSubres.mipLevel + clearRange.numMips - 1;
    const uint32 lastArraySlice = clearRange.startSubres.arraySlice + clearRange.numSlices - 1;

    // If single subres is requested for the format, iterate slice-by-slice and mip-by-mip.
    if (singleSubRes)
    {
        singleMipRange.numSlices = 1;
    }

    // We will do a dispatch for every box. If no boxes are specified then we will do a single full image dispatch.
    const bool   hasBoxes      = (boxCount > 0);
    const uint32 dispatchCount = hasBoxes ? boxCount : 1;

    // Boxes are only meaningful if we're clearing a single mip.
    PAL_ASSERT((hasBoxes == false) || ((pBoxes != nullptr) && (clearRange.numMips == 1)));

    const Device& device = *m_pDevice->Parent();

    for (;
         singleMipRange.startSubres.arraySlice <= lastArraySlice;
         singleMipRange.startSubres.arraySlice += singleMipRange.numSlices)
    {
        singleMipRange.startSubres.mipLevel = firstMipLevel;
        for (; singleMipRange.startSubres.mipLevel <= lastMipLevel; ++singleMipRange.startSubres.mipLevel)
        {
            const auto& subResInfo = *dstImage.SubresourceInfo(singleMipRange.startSubres);

            // Create an embedded SRD table and bind it to user data 0. We only need a single image view.
            uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                       SrdDwordAlignment(),
                                                                       SrdDwordAlignment(),
                                                                       PipelineBindPoint::Compute,
                                                                       0);

            // The view should cover this mip's clear range and use a raw format.
            ImageViewInfo imageView = {};
            PAL_ASSERT(dstImage.GetGfxImage()->ShaderWriteIncompatibleWithLayout(
                       singleMipRange.startSubres, dstImageLayout) == false);
            RpmUtil::BuildImageViewInfo(&imageView,
                                        dstImage,
                                        singleMipRange,
                                        viewFormat,
                                        dstImageLayout,
                                        device.TexOptLevel(),
                                        true);

            device.CreateImageViewSrds(1, &imageView, pSrdTable);

            // The default clear box is the entire subresource. This will be changed per-dispatch if boxes are enabled.
            Extent3d clearExtent = subResInfo.extentTexels;
            Offset3d clearOffset = {};

            for (uint32 i = 0; i < dispatchCount; i++)
            {
                if (hasBoxes)
                {
                    clearExtent = pBoxes[i].extent;
                    clearOffset = pBoxes[i].offset;
                }

                if (texelShift != 0)
                {
                    clearExtent.width >>= texelShift;
                    clearOffset.x     >>= texelShift;
                }

                // Compute the minimum number of threads to dispatch and fill out the per-dispatch constants.
                // Note that only 2D images can have multiple samples and 3D images cannot have multiple slices.
                DispatchDims threads = { clearExtent.width, 1, 1 };

                // The remaining virtual user-data contains a 2D offset followed by a 3D extent.
                uint32 userData[7] = {};
                uint32 numUserData = 0;

                switch (imageType)
                {
                case ImageType::Tex1d:
                    // For 1d the shader expects the x offset, an unused dword, then the clear width.
                    // ClearImage1D:dcl_num_thread_per_group 64, 1, 1, Y and Z direction threads are 1
                    userData[0] = clearOffset.x;
                    userData[2] = clearExtent.width;
                    numUserData = 3;

                    // 1D images can only have a single-sample, but they can have multiple slices.
                    threads.z = singleMipRange.numSlices;
                    break;

                case ImageType::Tex2d:
                    threads.y = clearExtent.height;
                    threads.z = singleMipRange.numSlices * createInfo.samples;

                    // For 2d the shader expects x offset, y offset, clear width then clear height.
                    userData[0] = clearOffset.x;
                    userData[1] = clearOffset.y;
                    userData[2] = clearExtent.width;
                    userData[3] = clearExtent.height;
                    numUserData = 4;
                    break;

                default:
                    // 3d image
                    threads.y = clearExtent.height;
                    threads.z = clearExtent.depth;

                    // For 3d the shader expects x, y z offsets, an unused dword then the width, height and depth.
                    userData[0] = clearOffset.x;
                    userData[1] = clearOffset.y;
                    userData[2] = clearOffset.z;

                    userData[4] = clearExtent.width;
                    userData[5] = clearExtent.height;
                    userData[6] = clearExtent.depth;
                    numUserData = 7;
                    break;
                }

                // Embed these constants in the remaining fast user-data entries (after the packedColor).
                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 5, numUserData, userData);

                pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroupsXyz(threads, threadsPerGroup));
            }
        }
    }

    // Restore original command buffer state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Builds commands to clear the contents of the buffer view (or the given ranges) to the given clear color.
// The simplest way to implement this is to decode the SRD's view info and reuse CmdClearColorBuffer.
void RsrcProcMgr::CmdClearBufferView(
    GfxCmdBuffer*     pCmdBuffer,
    const IGpuMemory& dstGpuMemory,
    const ClearColor& color,
    const void*       pBufferViewSrd,
    uint32            rangeCount,
    const Range*      pRanges
    ) const
{
    // Decode the buffer SRD.
    BufferViewInfo viewInfo = {};
    HwlDecodeBufferViewSrd(pBufferViewSrd, &viewInfo);

    // We need the offset and extent of the buffer wrt. the dstGpuMemory in units of texels.
    const uint32 viewStride = Formats::BytesPerPixel(viewInfo.swizzledFormat.format);
    const uint32 viewOffset = static_cast<uint32>(viewInfo.gpuAddr - dstGpuMemory.Desc().gpuVirtAddr);
    const uint32 viewExtent = static_cast<uint32>(viewInfo.range);

    // The view's offset and extent must be multiples of the view's texel stride.
    PAL_ASSERT((viewOffset % viewStride == 0) && (viewExtent % viewStride == 0));

    const uint32 offset = viewOffset / viewStride;
    const uint32 extent = viewExtent / viewStride;
    CmdClearColorBuffer(pCmdBuffer, dstGpuMemory, color, viewInfo.swizzledFormat, offset, extent, rangeCount, pRanges);
}

// =====================================================================================================================
// Builds commands to clear the contents of the buffer (or the given ranges) to the given clear color.
void RsrcProcMgr::CmdClearColorBuffer(
    GfxCmdBuffer*     pCmdBuffer,
    const IGpuMemory& dstGpuMemory,
    const ClearColor& color,
    SwizzledFormat    bufferFormat,
    uint32            bufferOffset,
    uint32            bufferExtent,
    uint32            rangeCount,
    const Range*      pRanges
    ) const
{
    const auto* pPublicSettings = m_pDevice->Parent()->GetPublicSettings();

    ClearColor clearColor = color;

    uint32 convertedColor[4] = {0};

    if (clearColor.type == ClearColorType::Float)
    {
        Formats::ConvertColor(bufferFormat, &clearColor.f32Color[0], &convertedColor[0]);
    }
    else
    {
        memcpy(&convertedColor[0], &clearColor.u32Color[0], sizeof(convertedColor));
    }

    // Pack the clear color into the form it is expected to take in memory.
    constexpr uint32 PackedColorDwords = 4;
    uint32           packedColor[PackedColorDwords] = {0};
    Formats::PackRawClearColor(bufferFormat, &convertedColor[0], &packedColor[0]);

    // This is the raw format that we will be writing.
    uint32               texelScale    = 0;
    const SwizzledFormat rawFormat     = RpmUtil::GetRawFormat(bufferFormat.format, &texelScale, nullptr);
    const uint32         bpp           = Formats::BytesPerPixel(rawFormat.format);
    const bool           texelScaleOne = (texelScale == 1);

    // Get the appropriate pipeline.
    const auto*const pPipeline = GetPipeline(RpmComputePipeline::ClearBuffer);
    const uint32     threadsPerGroup = pPipeline->ThreadsPerGroup();

    // Save current command buffer state and bind the pipeline.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    // some formats (notably RGB32) require multiple passes, e.g. we cannot write 12b texels (see RpmUtil::GetRawFormat)
    // for all other formats this loop will run a single iteration
    // This is pretty confusing, maybe we should have a separate TexelScale version like the clearImage shaders.
    for (uint32 channel = 0; channel < texelScale; channel++)
    {
        // Create an embedded SRD table and bind it to user data 0. We only need a single buffer view.
        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   SrdDwordAlignment(),
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        // Build an SRD we can use to write to any texel within the buffer using our raw format.
        BufferViewInfo dstViewInfo = {};
        dstViewInfo.gpuAddr        = dstGpuMemory.Desc().gpuVirtAddr +
                                     (texelScaleOne ? bpp : 1) * (bufferOffset) + channel * bpp;
        dstViewInfo.range          = bpp * texelScale * bufferExtent;
        dstViewInfo.stride         = bpp * texelScale;
        dstViewInfo.swizzledFormat = texelScaleOne ? rawFormat : UndefinedSwizzledFormat;
        dstViewInfo.flags.bypassMallRead  = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                           RpmViewsBypassMallOnRead);
        dstViewInfo.flags.bypassMallWrite = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                           RpmViewsBypassMallOnWrite);

        if (texelScaleOne)
        {
            m_pDevice->Parent()->CreateTypedBufferViewSrds(1, &dstViewInfo, pSrdTable);
        }
        else
        {
            // we have to use non-standard stride, which is incompatible with TypedBufferViewSrd contract
            m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &dstViewInfo, pSrdTable);
        }

        // Embed the constants in the remaining fast user-data entries. The clear color is constant over all ranges
        // so we can set it here. Note we need to only write one channel at a time if texelScale != 1.
        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 1,
                                   texelScaleOne ? PackedColorDwords : 1, packedColor + channel);

        // We will do a dispatch for every range. If no ranges are given then we will do a single full buffer dispatch.
        const Range  defaultRange    = { 0, bufferExtent };
        const bool   hasRanges       = (rangeCount > 0);
        const uint32 dispatchCount   = hasRanges ? rangeCount : 1;
        const Range* pDispatchRanges = hasRanges ? pRanges    : &defaultRange;

        for (uint32 i = 0; i < dispatchCount; i++)
        {
            // Verify that the range is contained within the view.
            PAL_ASSERT((pDispatchRanges[i].offset >= 0) &&
                       (pDispatchRanges[i].offset + pDispatchRanges[i].extent <= bufferExtent));

            // The final two constant buffer entries are the range offset and range extent.
            const uint32 userData[2] = { static_cast<uint32>(pDispatchRanges[i].offset), pDispatchRanges[i].extent };
            pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 1 + PackedColorDwords, 2, userData);

            // Execute the dispatch.
            const uint32 numThreadGroups = RpmUtil::MinThreadGroups(pDispatchRanges[i].extent, threadsPerGroup);

            pCmdBuffer->CmdDispatch({numThreadGroups, 1, 1});
        }
    }

    // Restore original command buffer state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Builds commands to clear the contents of the image view (or the given boxes) to the given clear color.
// Given that the destination image is in a shader writeable layout we must do this clear using a compute slow clear.
// The simplest way to implement this is to decode the SRD's format and range and reuse SlowClearCompute.
void RsrcProcMgr::CmdClearImageView(
    GfxCmdBuffer*     pCmdBuffer,
    const Image&      dstImage,
    ImageLayout       dstImageLayout,
    const ClearColor& color,
    const void*       pImageViewSrd,
    uint32            rectCount,
    const Rect*       pRects
    ) const
{
    // Get the SRD's format and subresource range.
    SwizzledFormat srdFormat    = {};
    SubresRange    srdRange     = {};

    HwlDecodeImageViewSrd(pImageViewSrd, dstImage, &srdFormat, &srdRange);

    ClearColor  clearColor = color;
    const auto& createInfo = dstImage.GetImageCreateInfo();

    if (rectCount != 0)
    {
        Box* pBoxes = PAL_NEW_ARRAY(Box, rectCount, m_pDevice->GetPlatform(), AllocObject);

        if (pBoxes != nullptr)
        {
            for (uint32 i = 0; i < rectCount; i++)
            {
                pBoxes[i].offset.x = pRects[i].offset.x;
                pBoxes[i].offset.y = pRects[i].offset.y;
                pBoxes[i].offset.z = srdRange.startSubres.arraySlice;

                pBoxes[i].extent.width  = pRects[i].extent.width;
                pBoxes[i].extent.height = pRects[i].extent.height;
                pBoxes[i].extent.depth  = srdRange.numSlices;
            }

            SlowClearCompute(pCmdBuffer,
                             dstImage,
                             dstImageLayout,
                             &clearColor,
                             srdFormat,
                             srdRange,
                             rectCount,
                             pBoxes);
            PAL_DELETE_ARRAY(pBoxes, m_pDevice->GetPlatform());
        }
        else
        {
            // Memory allocation failed.
            PAL_ASSERT_ALWAYS();
        }
    }
    else
    {
        SlowClearCompute(pCmdBuffer,
                         dstImage,
                         dstImageLayout,
                         &clearColor,
                         srdFormat,
                         srdRange,
                         rectCount,
                         nullptr);
    }
}

// =====================================================================================================================
// Expand DCC/Fmask/HTile and sync before shader-based (PS draw/CS dispatch) resolve image.
void RsrcProcMgr::LateExpandShaderResolveSrc(
    GfxCmdBuffer*             pCmdBuffer,
    const Image&              srcImage,
    ImageLayout               srcImageLayout,
    const ImageResolveRegion* pRegions,
    uint32                    regionCount,
    ResolveMethod             method,
    bool                      isCsResolve
    ) const
{
    PAL_ASSERT((method.shaderCsFmask != 0) || (method.shaderCs != 0) || (method.shaderPs != 0));

    const ImageLayoutUsageFlags shaderUsage =
        (method.shaderCsFmask ? Pal::LayoutShaderFmaskBasedRead : Pal::LayoutShaderRead);

    if (TestAnyFlagSet(srcImageLayout.usages, shaderUsage) == false)
    {
        BarrierTransition transition = { };
        transition.imageInfo.pImage            = &srcImage;
        transition.imageInfo.oldLayout.usages  = srcImageLayout.usages;
        transition.imageInfo.oldLayout.engines = srcImageLayout.engines;
        transition.imageInfo.newLayout.usages  = srcImageLayout.usages | shaderUsage;
        transition.imageInfo.newLayout.engines = srcImageLayout.engines;
        transition.srcCacheMask                = Pal::CoherResolveSrc;
        transition.dstCacheMask                = Pal::CoherShaderRead;

        // The destination operation for the image expand is either a CS read or PS read for the upcoming resolve.
        const HwPipePoint waitPoint = isCsResolve ? HwPipePreCs : HwPipePreRasterization;

        LateExpandShaderResolveSrcHelper(pCmdBuffer, pRegions, regionCount, transition, HwPipePostBlt, waitPoint);
    }
}

// =====================================================================================================================
// Inserts a barrier after a shader-based (PS draw/CS dispatch) resolve for the source color/depth-stencil image.
// Returns the image to the ResolveSrc layout after the draw/dispatch.
void RsrcProcMgr::FixupLateExpandShaderResolveSrc(
    GfxCmdBuffer*             pCmdBuffer,
    const Image&              srcImage,
    ImageLayout               srcImageLayout,
    const ImageResolveRegion* pRegions,
    uint32                    regionCount,
    ResolveMethod             method,
    bool                      isCsResolve
    ) const
{
    PAL_ASSERT((method.shaderCsFmask != 0) || (method.shaderCs != 0) || (method.shaderPs != 0));

    const ImageLayoutUsageFlags shaderUsage =
        (method.shaderCsFmask ? Pal::LayoutShaderFmaskBasedRead : Pal::LayoutShaderRead);

    if (TestAnyFlagSet(srcImageLayout.usages, shaderUsage) == false)
    {
        BarrierTransition transition = { };
        transition.imageInfo.pImage             = &srcImage;
        transition.imageInfo.oldLayout.usages   = srcImageLayout.usages | shaderUsage;
        transition.imageInfo.oldLayout.engines  = srcImageLayout.engines;
        transition.imageInfo.newLayout.usages   = srcImageLayout.usages;
        transition.imageInfo.newLayout.engines  = srcImageLayout.engines;

        transition.srcCacheMask = Pal::CoherShaderRead;
        transition.dstCacheMask = Pal::CoherResolveSrc;

        // The source operation for the image expand is either a CS read or PS read for the past resolve.
        const HwPipePoint pipePoint = isCsResolve ? HwPipePostCs : HwPipePostPs;

        LateExpandShaderResolveSrcHelper(pCmdBuffer, pRegions, regionCount, transition, pipePoint, HwPipePreBlt);
    }
}

// =====================================================================================================================
// Helper function for setting up a barrier used before and after a shader-based resolve.
void RsrcProcMgr::LateExpandShaderResolveSrcHelper(
    GfxCmdBuffer*             pCmdBuffer,
    const ImageResolveRegion* pRegions,
    uint32                    regionCount,
    const BarrierTransition&  transition,
    HwPipePoint               pipePoint,
    HwPipePoint               waitPoint
    ) const
{
    const Image& image = *static_cast<const Image*>(transition.imageInfo.pImage);

    AutoBuffer<BarrierTransition, 32, Platform> transitions(regionCount, m_pDevice->GetPlatform());

    if (transitions.Capacity() >= regionCount)
    {
        for (uint32 i = 0; i < regionCount; i++)
        {
            transitions[i].imageInfo.subresRange.startSubres.plane      = pRegions[i].srcPlane;
            transitions[i].imageInfo.subresRange.startSubres.arraySlice = pRegions[i].srcSlice;
            transitions[i].imageInfo.subresRange.startSubres.mipLevel   = 0;
            transitions[i].imageInfo.subresRange.numPlanes              = 1;
            transitions[i].imageInfo.subresRange.numMips                = 1;
            transitions[i].imageInfo.subresRange.numSlices              = pRegions[i].numSlices;

            transitions[i].imageInfo.pImage             = &image;
            transitions[i].imageInfo.oldLayout          = transition.imageInfo.oldLayout;
            transitions[i].imageInfo.newLayout          = transition.imageInfo.newLayout;
            transitions[i].imageInfo.pQuadSamplePattern = pRegions[i].pQuadSamplePattern;

            transitions[i].srcCacheMask = transition.srcCacheMask;
            transitions[i].dstCacheMask = transition.dstCacheMask;

            PAL_ASSERT((image.GetImageCreateInfo().flags.sampleLocsAlwaysKnown != 0) ==
                       (pRegions[i].pQuadSamplePattern != nullptr));
        }

        BarrierInfo barrierInfo = { };
        barrierInfo.pTransitions    = &transitions[0];
        barrierInfo.transitionCount = regionCount;
        barrierInfo.waitPoint       = waitPoint;
        barrierInfo.reason          = Developer::BarrierReasonUnknown;

        const HwPipePoint releasePipePoint = pipePoint;
        barrierInfo.pipePointWaitCount = 1;
        barrierInfo.pPipePoints        = &releasePipePoint;

        pCmdBuffer->CmdBarrier(barrierInfo);
    }
    else
    {
        pCmdBuffer->NotifyAllocFailure();
    }
}

// =====================================================================================================================
// Resolves a multisampled source Image into the single-sampled destination Image using a compute shader.
void RsrcProcMgr::ResolveImageCompute(
    GfxCmdBuffer*             pCmdBuffer,
    const Image&              srcImage,
    ImageLayout               srcImageLayout,
    const Image&              dstImage,
    ImageLayout               dstImageLayout,
    ResolveMode               resolveMode,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    ResolveMethod             method,
    uint32                    flags
    ) const
{
    const auto& device   = *m_pDevice->Parent();

    LateExpandShaderResolveSrc(pCmdBuffer, srcImage, srcImageLayout, pRegions, regionCount, method, true);

    // Save the command buffer's state.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    // Basic resolves need one slot per region per image, FMask resolves need a third slot for the source Image's FMask.
    const bool   isCsFmask = (method.shaderCsFmask == 1);
    const uint32 numSlots  = isCsFmask ? 3 : 2;

    // Execute the Resolve for each region in the specified list.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        // Select a Resolve shader based on the source Image's sample-count and resolve method.
        const ComputePipeline*const pPipeline = GetCsResolvePipeline(srcImage,
                                                                     pRegions[idx].srcPlane,
                                                                     resolveMode,
                                                                     method);

        const DispatchDims threadsPerGroup = pPipeline->ThreadsPerGroupXyz();

        // Bind the pipeline.
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

        // Set both subresources to the first slice of the required mip level
        const SubresId srcSubres = { pRegions[idx].srcPlane, 0, pRegions[idx].srcSlice };
        const SubresId dstSubres = { pRegions[idx].dstPlane, pRegions[idx].dstMipLevel, pRegions[idx].dstSlice };

        SwizzledFormat srcFormat = srcImage.SubresourceInfo(srcSubres)->format;
        SwizzledFormat dstFormat = dstImage.SubresourceInfo(dstSubres)->format;

        // Override the formats with the caller's "reinterpret" format.
        if (Formats::IsUndefined(pRegions[idx].swizzledFormat.format) == false)
        {
            // We require that the channel formats match.
            PAL_ASSERT(Formats::ShareChFmt(srcFormat.format, pRegions[idx].swizzledFormat.format));
            PAL_ASSERT(Formats::ShareChFmt(dstFormat.format, pRegions[idx].swizzledFormat.format));

            // If the specified format exactly matches the image formats the resolve will always work. Otherwise, the
            // images must support format replacement.
            PAL_ASSERT(Formats::HaveSameNumFmt(srcFormat.format, pRegions[idx].swizzledFormat.format) ||
                       srcImage.GetGfxImage()->IsFormatReplaceable(srcSubres, srcImageLayout, false));

            PAL_ASSERT(Formats::HaveSameNumFmt(dstFormat.format, pRegions[idx].swizzledFormat.format) ||
                       dstImage.GetGfxImage()->IsFormatReplaceable(dstSubres, dstImageLayout, true));

            srcFormat.format = pRegions[idx].swizzledFormat.format;
            dstFormat.format = pRegions[idx].swizzledFormat.format;
        }

        // Non-SRGB can be treated as SRGB when copying to non-srgb image
        if (TestAnyFlagSet(flags, ImageResolveDstAsSrgb))
        {
            dstFormat.format = Formats::ConvertToSrgb(dstFormat.format);
            PAL_ASSERT(Formats::IsUndefined(dstFormat.format) == false);
        }
        // SRGB can be treated as Non-SRGB when copying to srgb image
        else if (TestAnyFlagSet(flags, ImageResolveDstAsNorm))
        {
            dstFormat.format = Formats::ConvertToUnorm(dstFormat.format);
            PAL_ASSERT(Formats::IsUndefined(dstFormat.format) == false);
        }

        // All resolve shaders use a 10-dword constant buffer with this layout:
        // cb0[0] = (source X offset, source Y offset, resolve width, resolve height)
        // cb0[1] = (dest X offset, dest Y offset)
        // cb0[2] = (sample count, gamma correction option, copy single sample flag, y invert flag)
        //
        // Gamma correction should only be enabled if the destination format is SRGB. Copy single sample should only
        // be used for integer formats or for DS images in average mode.
        //
        // Everything could fit in 8 DWORDs if someone wants to rewrite the constant logic in all 32 resolve shaders.
        const bool isDepthOrStencil = (srcImage.IsDepthPlane(pRegions[idx].srcPlane) ||
                                       srcImage.IsStencilPlane(pRegions[idx].srcPlane));

        const uint32 userData[10] =
        {
            static_cast<uint32>(pRegions[idx].srcOffset.x),
            static_cast<uint32>(pRegions[idx].srcOffset.y),
            pRegions[idx].extent.width,
            pRegions[idx].extent.height,
            static_cast<uint32>(pRegions[idx].dstOffset.x),
            static_cast<uint32>(pRegions[idx].dstOffset.y),
            srcImage.GetImageCreateInfo().samples,
            Formats::IsSrgb(dstFormat.format),
            (isDepthOrStencil ? (resolveMode == ResolveMode::Average)
                              : (Formats::IsSint(srcFormat.format) || Formats::IsUint(srcFormat.format))),
            TestAnyFlagSet(flags, ImageResolveInvertY)
        };

        // Embed the constant buffer in user-data right after the SRD table.
        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 1, ArrayLen32(userData), userData);

        // The hardware can't handle UAV stores using SRGB num format.  The resolve shaders already contain a
        // linear-to-gamma conversion, but in order for that to work the output UAV's num format must be patched to be
        // simple unorm.
        if (Formats::IsSrgb(dstFormat.format))
        {
            dstFormat.format = Formats::ConvertToUnorm(dstFormat.format);
        }

        // Create an embedded user-data table and bind it to user data 0. We need image views for the src and dst
        // subresources and in some cases an fmask image view.
        uint32* pUserData = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   SrdDwordAlignment() * numSlots,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        ImageViewInfo imageView[2] = {};
        SubresRange   viewRange = { dstSubres, 1, 1, pRegions[idx].numSlices };

        PAL_ASSERT(TestAnyFlagSet(dstImageLayout.usages, LayoutResolveDst) == true);

        // ResolveDst doesn't imply ShaderWrite, but it's safe because it's always uncompressed
        ImageLayout dstLayoutCompute  = dstImageLayout;
        dstLayoutCompute.usages      |= LayoutShaderWrite;

        // Destination image is at the beginning of pUserData.
        RpmUtil::BuildImageViewInfo(&imageView[0],
                                    dstImage,
                                    viewRange,
                                    dstFormat,
                                    dstLayoutCompute,
                                    device.TexOptLevel(),
                                    true);

        viewRange.startSubres = srcSubres;
        RpmUtil::BuildImageViewInfo(&imageView[1],
                                    srcImage,
                                    viewRange,
                                    srcFormat,
                                    srcImageLayout,
                                    device.TexOptLevel(),
                                    false);

        device.CreateImageViewSrds(2, &imageView[0], pUserData);
        pUserData += SrdDwordAlignment() * 2;

        if (isCsFmask)
        {
            // If this is an Fmask-accelerated Resolve, create a third image view of the source Image's Fmask surface.
            FmaskViewInfo fmaskView = {};
            fmaskView.pImage         = &srcImage;
            fmaskView.baseArraySlice = pRegions[idx].srcSlice;
            fmaskView.arraySize      = pRegions[idx].numSlices;

            m_pDevice->Parent()->CreateFmaskViewSrds(1, &fmaskView, pUserData);
        }

        // Execute the dispatch. Resolves can only be done on 2D images so the Z dimension of the dispatch is always 1.
        const DispatchDims threads = {pRegions[idx].extent.width, pRegions[idx].extent.height, pRegions[idx].numSlices};

        pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroupsXyz(threads, threadsPerGroup));
    }

    // Restore the command buffer's state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);

    FixupComputeResolveDst(pCmdBuffer, dstImage, regionCount, pRegions);

    FixupLateExpandShaderResolveSrc(pCmdBuffer, srcImage, srcImageLayout, pRegions, regionCount, method, true);
}

// =====================================================================================================================
// Selects a compute Resolve pipeline based on the properties of the given Image and resolve method.
const ComputePipeline* RsrcProcMgr::GetCsResolvePipeline(
    const Image&  srcImage,
    uint32        plane,
    ResolveMode   mode,
    ResolveMethod method
    ) const
{
    const ComputePipeline* pPipeline = nullptr;
    const auto& createInfo = srcImage.GetImageCreateInfo();
    const bool  isStencil  = srcImage.IsStencilPlane(plane);

    // If the sample and fragment counts are different then this must be an EQAA resolve.
    if (createInfo.samples != createInfo.fragments)
    {
        PAL_ASSERT(method.shaderCsFmask == 1);

        switch (createInfo.fragments)
        {
        case 1:
            pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve1xEqaa);
            break;
        case 2:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve2xEqaa);
                break;
            case ResolveMode::Minimum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve2xEqaaMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve2xEqaaMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve2xEqaa);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        case 4:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve4xEqaa);
                break;
            case ResolveMode::Minimum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve4xEqaaMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve4xEqaaMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve4xEqaa);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        case 8:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve8xEqaa);
                break;
            case ResolveMode::Minimum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve8xEqaaMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve8xEqaaMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve8xEqaa);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        default:
            PAL_NEVER_CALLED();
            break;
        }
    }
    else if ((method.shaderCs == 1) && (method.shaderCsFmask == 0))
    {
        // A regular MSAA color image resolve shader is used for DS resolve as well. By setting the
        // "copy sample zero" flag to 1, we force the shader to simply copy the first sample (sample 0).
        switch (createInfo.samples)
        {
        case 2:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaResolve2x);
                break;
            case ResolveMode::Minimum:
                pPipeline = isStencil ? GetPipeline(RpmComputePipeline::MsaaResolveStencil2xMin)
                                      : GetPipeline(RpmComputePipeline::MsaaResolve2xMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = isStencil ? GetPipeline(RpmComputePipeline::MsaaResolveStencil2xMax)
                                      : GetPipeline(RpmComputePipeline::MsaaResolve2xMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaResolve2x);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        case 4:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaResolve4x);
                break;
            case ResolveMode::Minimum:
                pPipeline = isStencil ? GetPipeline(RpmComputePipeline::MsaaResolveStencil4xMin)
                                      : GetPipeline(RpmComputePipeline::MsaaResolve4xMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = isStencil ? GetPipeline(RpmComputePipeline::MsaaResolveStencil4xMax)
                                      : GetPipeline(RpmComputePipeline::MsaaResolve4xMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaResolve4x);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        case 8:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaResolve8x);
                break;
            case ResolveMode::Minimum:
                pPipeline = isStencil ? GetPipeline(RpmComputePipeline::MsaaResolveStencil8xMin)
                                      : GetPipeline(RpmComputePipeline::MsaaResolve8xMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = isStencil ? GetPipeline(RpmComputePipeline::MsaaResolveStencil8xMax)
                                      : GetPipeline(RpmComputePipeline::MsaaResolve8xMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaResolve8x);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        default:
            PAL_NEVER_CALLED();
            break;
        }
    }
    else
    {
        switch (createInfo.samples)
        {
        case 2:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve2x);
                break;
            case ResolveMode::Minimum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve2xMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve2xMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve2x);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        case 4:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve4x);
                break;
            case ResolveMode::Minimum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve4xMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve4xMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve4x);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        case 8:
            switch (mode)
            {
            case ResolveMode::Average:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve8x);
                break;
            case ResolveMode::Minimum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve8xMin);
                break;
            case ResolveMode::Maximum:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve8xMax);
                break;
            default:
                pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskResolve8x);
                PAL_NEVER_CALLED();
                break;
            }
            break;
        default:
            PAL_NEVER_CALLED();
            break;
        }
    }

    PAL_ASSERT(pPipeline != nullptr);
    return pPipeline;
}

// =====================================================================================================================
// Retrieves a pre-created MSAA state object that represents the requested number of samples.
const MsaaState* RsrcProcMgr::GetMsaaState(
    uint32 samples,
    uint32 fragments
    ) const
{
    const uint32 log2SampleRate = Log2(samples);
    const uint32 log2FragmentRate = Log2(fragments);
    PAL_ASSERT(log2SampleRate <= MaxLog2AaSamples);
    PAL_ASSERT(log2FragmentRate <= MaxLog2AaFragments);

    return m_pMsaaState[log2SampleRate][log2FragmentRate];
}

// =====================================================================================================================
// Create a number of common state objects used by the various RPM-owned GFX pipelines
Result RsrcProcMgr::CreateCommonStateObjects()
{
    // Setup a "default" depth/stencil state with depth testing: Depth writes and stencil writes all disabled.
    DepthStencilStateCreateInfo depthStencilInfo = { };
    depthStencilInfo.depthFunc                = CompareFunc::Always;
    depthStencilInfo.front.stencilFailOp      = StencilOp::Keep;
    depthStencilInfo.front.stencilPassOp      = StencilOp::Keep;
    depthStencilInfo.front.stencilDepthFailOp = StencilOp::Keep;
    depthStencilInfo.front.stencilFunc        = CompareFunc::Always;
    depthStencilInfo.back                     = depthStencilInfo.front;
    depthStencilInfo.depthEnable              = false;
    depthStencilInfo.depthWriteEnable         = false;
    depthStencilInfo.stencilEnable            = false;

    Result result = m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo, &m_pDepthDisableState, AllocInternal);

    if (result == Result::Success)
    {
        // Setup depth/stencil state with depth testing disabled, depth writes enabled and stencil writes enabled.
        // This is used for depth and stencil expands.
        depthStencilInfo.depthFunc                = CompareFunc::Always;
        depthStencilInfo.front.stencilFailOp      = StencilOp::Keep;
        depthStencilInfo.front.stencilPassOp      = StencilOp::Keep;
        depthStencilInfo.front.stencilDepthFailOp = StencilOp::Keep;
        depthStencilInfo.front.stencilFunc        = CompareFunc::Always;
        depthStencilInfo.back                     = depthStencilInfo.front;
        depthStencilInfo.depthEnable              = false;
        depthStencilInfo.depthWriteEnable         = true;
        depthStencilInfo.stencilEnable            = true;

        result = m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo, &m_pDepthExpandState, AllocInternal);
    }

    if (result == Result::Success)
    {
        // Setup depth/stencil state with depth testing disabled and depth/stencil writes disabled
        // This is used for depth and stencil resummarization.
        depthStencilInfo.depthFunc                = CompareFunc::Always;
        depthStencilInfo.front.stencilFailOp      = StencilOp::Keep;
        depthStencilInfo.front.stencilPassOp      = StencilOp::Keep;
        depthStencilInfo.front.stencilDepthFailOp = StencilOp::Keep;
        depthStencilInfo.front.stencilFunc        = CompareFunc::Always;
        depthStencilInfo.back                     = depthStencilInfo.front;
        depthStencilInfo.depthEnable              = false;
        depthStencilInfo.depthWriteEnable         = false;
        depthStencilInfo.stencilEnable            = false;

        result = m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo, &m_pDepthResummarizeState, AllocInternal);
    }

    // Setup the depth/stencil state for depth and stencil resolves using the graphics engine.
    if (result == Result::Success)
    {
        depthStencilInfo.depthEnable       = true;
        depthStencilInfo.depthFunc         = CompareFunc::Always;
        depthStencilInfo.front.stencilFunc = CompareFunc::Always;

        // State object for depth resolves:
        depthStencilInfo.front.stencilFailOp      = StencilOp::Keep;
        depthStencilInfo.front.stencilPassOp      = StencilOp::Keep;
        depthStencilInfo.front.stencilDepthFailOp = StencilOp::Keep;
        depthStencilInfo.back                     = depthStencilInfo.front;
        depthStencilInfo.depthWriteEnable         = true;
        depthStencilInfo.stencilEnable            = false;

        result = m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo, &m_pDepthResolveState, AllocInternal);

        if (result == Result::Success)
        {
            // State object for stencil resolves:
            depthStencilInfo.front.stencilFailOp      = StencilOp::Replace;
            depthStencilInfo.front.stencilPassOp      = StencilOp::Replace;
            depthStencilInfo.front.stencilDepthFailOp = StencilOp::Replace;
            depthStencilInfo.back                     = depthStencilInfo.front;
            depthStencilInfo.depthWriteEnable         = true;
            depthStencilInfo.stencilEnable            = true;

            result = m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo,
                                                                &m_pDepthStencilResolveState,
                                                                AllocInternal);
        }

        if (result == Result::Success)
        {
            // State object for stencil resolves:
            depthStencilInfo.front.stencilFailOp      = StencilOp::Replace;
            depthStencilInfo.front.stencilPassOp      = StencilOp::Replace;
            depthStencilInfo.front.stencilDepthFailOp = StencilOp::Replace;
            depthStencilInfo.back                     = depthStencilInfo.front;
            depthStencilInfo.depthWriteEnable         = false;
            depthStencilInfo.stencilEnable            = true;

            result = m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo,
                                                                &m_pStencilResolveState,
                                                                AllocInternal);
        }
    }

    // Setup the depth/stencil states for clearing depth and/or stencil.
    if (result == Result::Success)
    {
        depthStencilInfo.depthFunc                = CompareFunc::Always;
        depthStencilInfo.front.stencilFunc        = CompareFunc::Always;
        depthStencilInfo.front.stencilFailOp      = StencilOp::Replace;
        depthStencilInfo.front.stencilPassOp      = StencilOp::Replace;
        depthStencilInfo.front.stencilDepthFailOp = StencilOp::Replace;
        depthStencilInfo.back                     = depthStencilInfo.front;
        depthStencilInfo.depthBoundsEnable        = false;
        depthStencilInfo.depthWriteEnable         = true;
        depthStencilInfo.depthEnable              = true;
        depthStencilInfo.stencilEnable            = true;

        result =
            m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo, &m_pDepthStencilClearState, AllocInternal);

        if (result == Result::Success)
        {
            depthStencilInfo.depthEnable   = true;
            depthStencilInfo.stencilEnable = false;

            result = m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo, &m_pDepthClearState, AllocInternal);
        }

        if (result == Result::Success)
        {
            depthStencilInfo.depthEnable   = false;
            depthStencilInfo.stencilEnable = true;

            result = m_pDevice->CreateDepthStencilStateInternal(depthStencilInfo, &m_pStencilClearState, AllocInternal);
        }
    }

    if (result == Result::Success)
    {
        // Set up a "default" color blend state which disables all blending.
        ColorBlendStateCreateInfo blendInfo = { };
        for (uint32 idx = 0; idx < MaxColorTargets; ++idx)
        {
            blendInfo.targets[idx].srcBlendColor  = Blend::One;
            blendInfo.targets[idx].srcBlendAlpha  = Blend::One;
            blendInfo.targets[idx].dstBlendColor  = Blend::Zero;
            blendInfo.targets[idx].dstBlendAlpha  = Blend::Zero;
            blendInfo.targets[idx].blendFuncColor = BlendFunc::Add;
            blendInfo.targets[idx].blendFuncAlpha = BlendFunc::Add;
        }

        result = m_pDevice->CreateColorBlendStateInternal(blendInfo, &m_pBlendDisableState, AllocInternal);
    }

    if (result == Result::Success)
    {
        // Set up a color blend state which enable rt0 blending.
        ColorBlendStateCreateInfo blendInfo = { };
        blendInfo.targets[0].blendEnable    = 1;
        blendInfo.targets[0].srcBlendColor  = Blend::SrcColor;
        blendInfo.targets[0].srcBlendAlpha  = Blend::SrcAlpha;
        blendInfo.targets[0].dstBlendColor  = Blend::DstColor;
        blendInfo.targets[0].dstBlendAlpha  = Blend::OneMinusSrcAlpha;
        blendInfo.targets[0].blendFuncColor = BlendFunc::Add;
        blendInfo.targets[0].blendFuncAlpha = BlendFunc::Add;

        result = m_pDevice->CreateColorBlendStateInternal(blendInfo, &m_pColorBlendState, AllocInternal);
    }

    // Create all MSAA state objects.
    MsaaStateCreateInfo msaaInfo = { };
    msaaInfo.sampleMask          = USHRT_MAX;

    for (uint32 log2Samples = 0; ((log2Samples <= MaxLog2AaSamples) && (result == Result::Success)); ++log2Samples)
    {
        const uint32 coverageSamples = (1 << log2Samples);
        msaaInfo.coverageSamples         = coverageSamples;
        msaaInfo.alphaToCoverageSamples  = coverageSamples;

        for (uint32 log2Fragments = 0;
            ((log2Fragments <= MaxLog2AaFragments) && (result == Result::Success));
            ++log2Fragments)
        {
            const uint32 fragments = (1 << log2Fragments);

            // The following parameters should never be higher than the max number of msaa fragments (usually 8).
            const uint32 maxFragments        = m_pDevice->Parent()->ChipProperties().imageProperties.maxMsaaFragments;
            const uint32 clampedSamples      = Min(fragments, maxFragments);
            msaaInfo.exposedSamples          = clampedSamples;
            msaaInfo.pixelShaderSamples      = clampedSamples;
            msaaInfo.depthStencilSamples     = clampedSamples;
            msaaInfo.shaderExportMaskSamples = clampedSamples;
            msaaInfo.sampleClusters          = clampedSamples;

            result = m_pDevice->CreateMsaaStateInternal(
                msaaInfo, &m_pMsaaState[log2Samples][log2Fragments], AllocInternal);
        }
    }

    return result;
}

// =====================================================================================================================
// Returns the size of a typed buffer that contains a 3D block of elements with the given size and pitches.
// This is useful for mapping a sub-cube of a linear image into a linear buffer.
gpusize RsrcProcMgr::ComputeTypedBufferRange(
    const Extent3d& extent,
    uint32          elementSize, // The size of each element in bytes.
    gpusize         rowPitch,    // The number of bytes between successive rows.
    gpusize         depthPitch)  // The number of bytes between successive depth slices.
{
    // This function will underflow if the extents aren't fully defined.
    PAL_ASSERT((extent.width > 0) && (extent.height > 0) && (extent.depth > 0));

    // Traversing the buffer from the "top left" to "bottom right" covers (depth - 1) full depth slices, (height - 1)
    // full rows, and (width) elements in the final partial row.
    return (((extent.depth - 1) * depthPitch) + ((extent.height - 1) * rowPitch) + (extent.width * elementSize));
}

// =====================================================================================================================
// Binds common graphics state.
void RsrcProcMgr::BindCommonGraphicsState(
    GfxCmdBuffer* pCmdBuffer
    ) const
{
    const InputAssemblyStateParams   inputAssemblyState   = { PrimitiveTopology::RectList };
    const DepthBiasParams            depthBias            = { 0.0f, 0.0f, 0.0f };
    const PointLineRasterStateParams pointLineRasterState = { 1.0f, 1.0f };

    const TriangleRasterStateParams  triangleRasterState =
    {
        FillMode::Solid,        // frontface fillMode
        FillMode::Solid,        // backface fillMode
        CullMode::_None,        // cullMode
        FaceOrientation::Cw,    // frontFace
        ProvokingVertex::First  // provokingVertex
    };

    GlobalScissorParams scissorParams = { };
    scissorParams.scissorRegion.extent.width  = Pm4::MaxScissorExtent;
    scissorParams.scissorRegion.extent.height = Pm4::MaxScissorExtent;

    pCmdBuffer->CmdSetInputAssemblyState(inputAssemblyState);
    pCmdBuffer->CmdSetDepthBiasState(depthBias);
    pCmdBuffer->CmdSetPointLineRasterState(pointLineRasterState);
    pCmdBuffer->CmdSetTriangleRasterState(triangleRasterState);
    pCmdBuffer->CmdSetClipRects(Pm4::DefaultClipRectsRule, 0, nullptr);
    pCmdBuffer->CmdSetGlobalScissor(scissorParams);

    // Setup register state to put VRS into 1x1 mode (i.e., essentially off).

    VrsCenterState  centerState = {};
    VrsRateParams   rateParams  = {};

    rateParams.shadingRate = VrsShadingRate::_1x1;
    rateParams.combinerState[static_cast<uint32>(VrsCombinerStage::ProvokingVertex)] = VrsCombiner::Passthrough;
    rateParams.combinerState[static_cast<uint32>(VrsCombinerStage::Primitive)]       = VrsCombiner::Passthrough;
    rateParams.combinerState[static_cast<uint32>(VrsCombinerStage::Image)]           = VrsCombiner::Passthrough;
    rateParams.combinerState[static_cast<uint32>(VrsCombinerStage::PsIterSamples)]   = VrsCombiner::Min;

    pCmdBuffer->CmdSetPerDrawVrsRate(rateParams);
    pCmdBuffer->CmdSetVrsCenterState(centerState);

    // Might not have a bound depth buffer here, so don't provide a source image either so the draw-time validator
    // doesn't do an insane amount of work.
    pCmdBuffer->CmdBindSampleRateImage(nullptr);
}

/// BltMonitorDesc defines a parametrized model for monitors supported by the Desktop Composition interface.
struct BltMonitorDesc
{
    uint32      numPixels;          // Number of pixels packed into a single word
    bool        isColorType;        // True if color monitor, False for monochrome
    bool        isSplitType;        // True if the packed pixels are not adjacent (on screen)
    float       scalingParams[4];   // scaling parameters which is used to convert from float to 10-bit uints
    float       grayScalingMap[12]; // Luminance constants which convert color to monochrome
    uint32      packParams[24];     // parametrized packing layout
};

/// PackPixelConstant describes a set of constants which will be passed to PackedPixelComposite shader.
///     c0       desktop sampling scale/offset for left/first pixel
///     c1       desktop sampling scale/offset for right/third pixel
///     c2       shader flow control parameters
///     c3-c5    color to grayscale conversion matrix
///     c6-c7    left pixel pack parameters
///     c8-c9    middle pixel pack parameters
///     c10-c11  right pixel packing parameters
///     c12      scaling parameters which is used to convert from float to 10-bit unsigned integers
///     c13      region.width*1.0, region.height*1.0, region.width, region.height
struct PackPixelConstant
{
    uint32 aluConstant0[4];
    uint32 aluConstant1[4];
    uint32 aluConstant2[4];
    uint32 aluConstant3[4];
    uint32 aluConstant4[4];
    uint32 aluConstant5[4];
    uint32 aluConstant6[4];
    uint32 aluConstant7[4];
    uint32 aluConstant8[4];
    uint32 aluConstant9[4];
    uint32 aluConstant10[4];
    uint32 aluConstant11[4];
    uint32 aluConstant12[4];
    uint32 aluConstant13[4];
};

static const BltMonitorDesc Desc_NotPacked =
{
    1,                                  // Number of packed pixels
    true,                               // isColorType ? (predicate)
    false,                              // isSplitType ? (predicate)
    255.0f, 1/255.0f, 0, 0,            // pixel precision (2^N-1, 1/(2^N-1))

    1.0f, 0.0f, 0.0f, 0.0f,            // grayScaling
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
};

static const BltMonitorDesc Desc_SplitG70B54_R70B10 =
{
    2,                                  // Number of packed pixels
    false,                              // isColorType ? (predicate)
    true,                               // isSplitType ? (predicate)
    1023.0f, 1/1023.0f, 0, 0,           // pixel precision (2^N-1, 1/(2^N-1))

    0.2126f, 0.7152f, 0.0722f, 0.0f,    // grayScaling
    0.2126f, 0.7152f, 0.0722f, 0.0f,
    0.2126f, 0.7152f, 0.0722f, 0.0f,

    0x00, 0xff, 0x00, 2,        // Most significant bits for the first pixel
    0x00, 0x00, 0x30, 4,        // Least significant bits for the first pixel
    0xff, 0x00, 0x00, 2,        // Most significant bits for the second pixel
    0x00, 0x00, 0x03, 0         // Least significant bits for the second pixel

};

static const BltMonitorDesc Desc_SplitB70G10_R70G76 =
{
    2,                                  // Number of packed pixels
    false,                              // isColorType ? (predicate)
    true,                               // isSplitType ? (predicate)
    1023.0f, 1/1023.0f, 0, 0,           // pixel precision (2^N-1, 1/(2^N-1))

    0.2126f, 0.7152f, 0.0722f, 0.0f,    // grayScaling
    0.2126f, 0.7152f, 0.0722f, 0.0f,
    0.2126f, 0.7152f, 0.0722f, 0.0f,

    0x00, 0x00, 0xff, 2,        // Most significant bits for the first pixel
    0x00, 0x03, 0x00, 0,        // Least significant bits for the first pixel
    0xff, 0x00, 0x00, 2,        // Most significant bits for the second pixel
    0x00, 0xc0, 0x00, 6          // Least significant bits for the second pixel

};

static const BltMonitorDesc Desc_G70B54_R70B10 =
{
    2,                                 // Number of packed pixels
    false,                             // isColorType ? (predicate)
    false,                             // isSplitType ? (predicate)
    1023.0f, 1/1023.0f, 0, 0,          // pixel precision (2^N-1, 1/(2^N-1))

    0.2126f, 0.7152f, 0.0722f, 0.0f,    // grayScaling
    0.2126f, 0.7152f, 0.0722f, 0.0f,
    0.2126f, 0.7152f, 0.0722f, 0.0f,

    0x00, 0xff, 0x00, 2,        // Most significant bits for the first pixel
    0x00, 0x00, 0x30, 4,        // Least significant bits for the first pixel
    0xff, 0x00, 0x00, 2,        // Most significant bits for the second pixel
    0x00, 0x00, 0x03, 0          // Least significant bits for the second pixel
};

static const BltMonitorDesc Desc_B70R32_G70R76 =
{
    2,                                 // Number of packed pixels
    false,                             // isColorType ? (predicate)
    false,                             // isSplitType ? (predicate)
    1023.0f, 1/1023.0f, 0, 0,          // pixel precision (2^N-1, 1/(2^N-1))

    0x00, 0x00, 0xff, 2,        // Most significant bits for the first pixel
    0x0c, 0x00, 0x00, 2,        // Least significant bits for the first pixel
    0x00, 0xff, 0x00, 2,        // Most significant bits for the second pixel
    0xc0, 0x00, 0x00, 6         // Least significant bits for the second pixel

};

static const BltMonitorDesc Desc_B70R30_G70R74 =
{
    2,                                 // Number of packed pixels
    false,                             // isColorType ? (predicate)
    false,                             // isSplitType ? (predicate)
    4095.0f, 1/4095.0f, 0, 0,          // pixel precision (2^N-1, 1/(2^N-1))

    0.2126f, 0.7152f, 0.0722f, 0.0f,   // grayScaling
    0.2126f, 0.7152f, 0.0722f, 0.0f,
    0.2126f, 0.7152f, 0.0722f, 0.0f,

    0x00, 0x00, 0xff, 4,        // Most significant bits for the first pixel
    0x0f, 0x00, 0x00, 0,        // Least significant bits for the first pixel
    0x00, 0xff, 0x00, 4,        // Most significant bits for the second pixel
    0xf0, 0x00, 0x00, 4         // Least significant bits for the second pixel
};

static const BltMonitorDesc Desc_B70_G70_R70 =
{
    3,                                // Number of packed pixels
    false,                            // isColorType ? (predicate)
    false,                            // isSplitType ? (predicate)
    255.0f, 1/255.0f, 0, 0,           // pixel precision (2^N-1, 1/(2^N-1))

    0.2126f, 0.7152f, 0.0722f, 0.0f,  // grayScaling
    0.2126f, 0.7152f, 0.0722f, 0.0f,
    0.2126f, 0.7152f, 0.0722f, 0.0f,

    0x00, 0x00, 0xff, 0,        // Most significant bits for the first pixel
    0x00, 0x00, 0x00, 0,        // Least significant bits for the first pixel
    0x00, 0xff, 0x00, 0,        // Most significant bits for the second pixel
    0x00, 0x00, 0x00, 0,        // Least significant bits for the second pixel
    0xff, 0x00, 0x00, 0,        // Most significant bits for the third pixel
    0x00, 0x00, 0x00, 0         // Least significant bits for the third pixel

};

static const BltMonitorDesc Desc_R70G76 =
{
    1,                                // Number of packed pixels
    false,                            // isColorType ? (predicate)
    false,                            // isSplitType ? (predicate)
    1023.0f, 1/1023.0f, 0, 0,         // pixel precision (2^N-1, 1/(2^N-1))

    0.2126f, 0.7152f, 0.0722f, 0.0f,  // grayScaling
    0.2126f, 0.7152f, 0.0722f, 0.0f,
    0.2126f, 0.7152f, 0.0722f, 0.0f,

    0xff, 0x00, 0x00, 2,        // Most significant bits for the first pixel
    0x00, 0xc0, 0x00, 6         // Least significant bits for the first pixel

};

static const BltMonitorDesc Desc_G70B54 =
{
    1,                                  // Number of packed pixels
    false,                              // isColorType ? (predicate)
    false,                              // isSplitType ? (predicate)
    1023.0f, 1/1023.0f, 0, 0,       // pixel precision (2^N-1, 1/(2^N-1))

    0.2126f, 0.7152f, 0.0722f, 0.0f,  // grayScaling
    0.2126f, 0.7152f, 0.0722f, 0.0f,
    0.2126f, 0.7152f, 0.0722f, 0.0f,

    0x00, 0xff, 0x00, 2,        // Most significant bits for the first pixel
    0x00, 0x00, 0x30, 4         // Least significant bits for the first pixel

};

static const BltMonitorDesc Desc_Native =
{
    1,                                  // Number of packed pixels
    true,                               // isColorType ? (predicate)
    false,                              // isSplitType ? (predicate)
    1023.0f, 1/1023.0f, 0.0f, 0.0f,     // pixel precision (2^N-1, 1/(2^N-1))

    1.0f, 0.0f, 0.0f, 0.0f,             // grayScaling
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
};

// =====================================================================================================================
// Return pointer to parametrized monitor description given the specified (input) packed pixel type.
static const BltMonitorDesc* GetMonitorDesc(
    PackedPixelType packedPixelType)  // packed pixel type
{
    const BltMonitorDesc* pDesc = nullptr;
    switch (packedPixelType)
    {
    case PackedPixelType::NotPacked:
        pDesc = &Desc_NotPacked;
        break;

    case PackedPixelType::SplitG70B54_R70B10:
        pDesc = &Desc_SplitG70B54_R70B10;
        break;

    case PackedPixelType::SplitB70G10_R70G76:
        pDesc = &Desc_SplitB70G10_R70G76;
        break;

    case PackedPixelType::G70B54_R70B10:
        pDesc = &Desc_G70B54_R70B10;
        break;

    case PackedPixelType::B70R32_G70R76:
        pDesc = &Desc_B70R32_G70R76;
        break;

    case PackedPixelType::B70R30_G70R74:
        pDesc = &Desc_B70R30_G70R74;
        break;

    case PackedPixelType::B70_G70_R70:
        pDesc = &Desc_B70_G70_R70;
        break;

    case PackedPixelType::R70G76:
        pDesc = &Desc_R70G76;
        break;

    case PackedPixelType::G70B54:
        pDesc = &Desc_G70B54;
        break;

    case PackedPixelType::Native:
        pDesc = &Desc_Native;
        break;

    default:
        break;
    }
    return pDesc;
}

// =====================================================================================================================
// return packed pixel constant scaling and offset constant based on packed pixel state
static const void ProcessPackPixelCopyConstants(
    const BltMonitorDesc&    monDesc,
    uint32                   packFactor,
    const ImageCopyRegion&   regions,
    float*                   pAluConstants)
{
    float leftOffset;
    float rightOffset;
    float scale;

    scale = (monDesc.isSplitType)? 0.5f : 1.0f;

    if (monDesc.isSplitType)
    {
        leftOffset = 0.5f * regions.srcOffset.x;
        rightOffset = 0.5f;
    }
    else
    {
        const float pixelWidth = 1.0f / static_cast<float>(regions.extent.width * monDesc.numPixels);
        const float offset     = (packFactor == 2) ? (pixelWidth / 2.0f) : pixelWidth;

        leftOffset = -offset;
        rightOffset = offset;
    }

    // c13 -> region.width*1.0, region.height*1.0, region.width, region.height
    pAluConstants[52] = 1.0f * regions.extent.width;
    pAluConstants[53] = 1.0f * regions.extent.height;

    pAluConstants[0] = scale;
    pAluConstants[1] = 1.0f;
    pAluConstants[2] = leftOffset;
    pAluConstants[3] = 0.0f;
    pAluConstants[4] = scale;
    pAluConstants[5] = 1.0f;
    pAluConstants[6] = rightOffset;
    pAluConstants[7] = 0.0f;
}

// =====================================================================================================================
void RsrcProcMgr::CmdGfxDccToDisplayDcc(
    GfxCmdBuffer*  pCmdBuffer,
    const IImage&  image
    ) const
{
    HwlGfxDccToDisplayDcc(pCmdBuffer, static_cast<const Pal::Image&>(image));
}

// =====================================================================================================================
// Put displayDCC memory itself back into a "fully decompressed" state.
void RsrcProcMgr::CmdDisplayDccFixUp(
    GfxCmdBuffer*      pCmdBuffer,
    const IImage&      image
    ) const
{
    InitDisplayDcc(pCmdBuffer, static_cast<const Pal::Image&>(image));
}

} // Pal
