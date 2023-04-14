/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palAutoBuffer.h"
#include "palColorBlendState.h"
#include "palColorTargetView.h"
#include "palDepthStencilState.h"
#include "palDepthStencilView.h"
#include "palFormatInfo.h"
#include "palMsaaState.h"
#include "palFormatInfo.h"
#include "palInlineFuncs.h"
#include "palGpuMemory.h"
#include "core/hw/gfxip/colorBlendState.h"
#include "core/hw/gfxip/computePipeline.h"
#include "core/hw/gfxip/depthStencilState.h"
#include "core/hw/gfxip/msaaState.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/graphicsPipeline.h"
#include "core/hw/gfxip/pm4Image.h"
#include "core/hw/gfxip/pm4IndirectCmdGenerator.h"
#include "core/hw/gfxip/pm4UniversalCmdBuffer.h"
#include "core/hw/gfxip/rpm/rpmUtil.h"
#include "core/hw/gfxip/rpm/pm4RsrcProcMgr.h"

#include <float.h>

using namespace Util;

namespace Pal
{

namespace Pm4
{
// =====================================================================================================================
// Note that this constructor is invoked before settings have been committed.
RsrcProcMgr::RsrcProcMgr(
    GfxDevice* pDevice)
    :
    Pal::RsrcProcMgr(pDevice),
    m_releaseAcquireSupported(m_pDevice->Parent()->ChipProperties().gfx9.supportReleaseAcquireInterface)
{

}

// =====================================================================================================================
RsrcProcMgr::~RsrcProcMgr()
{
}

// =====================================================================================================================
// Default implementation of getting the engine to use for image-to-image copies.
ImageCopyEngine RsrcProcMgr::GetImageToImageCopyEngine(
    const GfxCmdBuffer*    pCmdBuffer,
    const Image&           srcImage,
    const Image&           dstImage,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    uint32                 copyFlags
    ) const
{
    const auto&      srcInfo      = srcImage.GetImageCreateInfo();
    const auto&      dstInfo      = dstImage.GetImageCreateInfo();
    const ImageType  srcImageType = srcInfo.imageType;
    const ImageType  dstImageType = dstInfo.imageType;

    const bool bothColor    = ((srcImage.IsDepthStencilTarget() == false) &&
                               (dstImage.IsDepthStencilTarget() == false) &&
                               (Formats::IsDepthStencilOnly(srcInfo.swizzledFormat.format) == false) &&
                               (Formats::IsDepthStencilOnly(dstInfo.swizzledFormat.format) == false));
    const bool isCompressed = (Formats::IsBlockCompressed(srcInfo.swizzledFormat.format) ||
                               Formats::IsBlockCompressed(dstInfo.swizzledFormat.format));
    const bool isYuv        = (Formats::IsYuv(srcInfo.swizzledFormat.format) ||
                               Formats::IsYuv(dstInfo.swizzledFormat.format));

    const bool isSrgbWithFormatConversion = (Formats::IsSrgb(dstInfo.swizzledFormat.format) &&
                                             TestAnyFlagSet(copyFlags, CopyFormatConversion));
    const bool isMacroPixelPackedRgbOnly  = (Formats::IsMacroPixelPackedRgbOnly(srcInfo.swizzledFormat.format) ||
                                             Formats::IsMacroPixelPackedRgbOnly(dstInfo.swizzledFormat.format));

    ImageCopyEngine  engineType = ImageCopyEngine::Compute;

    // We need to decide between the graphics copy path and the compute copy path. The graphics path only supports
    // single-sampled non-compressed, non-YUV , non-MacroPixelPackedRgbOnly 2D or 2D color images for now.
    if ((Image::PreferGraphicsCopy && pCmdBuffer->IsGraphicsSupported()) &&
        (dstImage.IsDepthStencilTarget() ||
         ((srcImageType != ImageType::Tex1d)   &&
          (dstImageType != ImageType::Tex1d)   &&
          (dstInfo.samples == 1)               &&
          (isCompressed == false)              &&
          (isYuv == false)                     &&
          (isMacroPixelPackedRgbOnly == false) &&
          (bothColor == true)                  &&
          (isSrgbWithFormatConversion == false))))
    {
        engineType = ImageCopyEngine::Graphics;
    }

    // Scissor-enabled blit for OGLP is only supported on graphics path.
    PAL_ASSERT((engineType == ImageCopyEngine::Graphics) ||
               (TestAnyFlagSet(copyFlags, CopyEnableScissorTest) == false));

    return engineType;
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
    const auto& srcInfo = srcImage.GetImageCreateInfo();
    const auto& dstInfo = dstImage.GetImageCreateInfo();

    // MSAA source and destination images must have the same number of fragments.  Note that MSAA images always use
    // the compute copy path; the shader instructions are based on fragments, not samples.
    PAL_ASSERT(srcInfo.fragments == dstInfo.fragments);

    const ImageCopyEngine copyEngine =
        GetImageToImageCopyEngine(pCmdBuffer, srcImage, dstImage, regionCount, pRegions, flags);

    if (copyEngine == ImageCopyEngine::Graphics)
    {
        CopyImageGraphics(pCmdBuffer,
                          srcImage,
                          srcImageLayout,
                          dstImage,
                          dstImageLayout,
                          regionCount,
                          pRegions,
                          pScissorRect,
                          flags);
    }
    else
    {
        AutoBuffer<ImageFixupRegion, 32, Platform> fixupRegions(regionCount, m_pDevice->GetPlatform());
        if (fixupRegions.Capacity() >= regionCount)
        {
            for (uint32 i = 0; i < regionCount; i++)
            {
                fixupRegions[i].subres    = pRegions[i].dstSubres;
                fixupRegions[i].offset    = pRegions[i].dstOffset;
                fixupRegions[i].extent    = pRegions[i].extent;
                fixupRegions[i].numSlices = pRegions[i].numSlices;
            }
            FixupMetadataForComputeDst(pCmdBuffer, dstImage, dstImageLayout, regionCount, &fixupRegions[0], true);

            CopyImageCompute(pCmdBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout,
                             regionCount, pRegions, flags);

            FixupMetadataForComputeDst(pCmdBuffer, dstImage, dstImageLayout, regionCount, &fixupRegions[0], false);

            if (((Formats::IsBlockCompressed(srcInfo.swizzledFormat.format) ||
                  Formats::IsMacroPixelPackedRgbOnly(srcInfo.swizzledFormat.format)) && (srcInfo.mipLevels > 1)) ||
                ((Formats::IsBlockCompressed(dstInfo.swizzledFormat.format) ||
                  Formats::IsMacroPixelPackedRgbOnly(dstInfo.swizzledFormat.format)) && (dstInfo.mipLevels > 1)))
            {
                // Insert a generic barrier to prevent write-after-write hazard between CS copy and per-pixel copy
                BarrierInfo barrier = {};
                barrier.waitPoint = HwPipePreCs;
                barrier.reason = Developer::BarrierReasonUnknown;

                constexpr HwPipePoint PostCs = HwPipePostCs;
                barrier.pipePointWaitCount = 1;
                barrier.pPipePoints = &PostCs;

                BarrierTransition transition = {};
                transition.srcCacheMask = CoherShader;
                transition.dstCacheMask = CoherShader;
                barrier.pTransitions = &transition;

                pCmdBuffer->CmdBarrier(barrier);

                for (uint32 regionIdx = 0; regionIdx < regionCount; regionIdx++)
                {
                    HwlImageToImageMissingPixelCopy(pCmdBuffer, srcImage, dstImage, pRegions[regionIdx]);
                }
            }
        }
        else
        {
            pCmdBuffer->NotifyAllocFailure();
        }
    }

    if (copyEngine == ImageCopyEngine::ComputeVrsDirty)
    {
        // This copy destroyed the VRS data associated with the destination image.  Mark it as dirty so the
        // next draw re-issues the VRS copy.
        pCmdBuffer->DirtyVrsDepthImage(&dstImage);
    }
}

// =====================================================================================================================
// Builds commands to copy one or more regions from one image to another using a graphics pipeline.
// This path only supports copies between single-sampled non-compressed 2D, 2D color, and 3D images for now.
void RsrcProcMgr::CopyColorImageGraphics(
    Pm4CmdBuffer*          pCmdBuffer,
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
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    // Get some useful information about the image.
    const auto& dstCreateInfo   = dstImage.GetImageCreateInfo();
    const auto& srcCreateInfo   = srcImage.GetImageCreateInfo();
    const auto& device          = *m_pDevice->Parent();
    const auto* pPublicSettings = device.GetPublicSettings();

    Pal::CmdStream*const pStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Graphics);
    PAL_ASSERT(pStream != nullptr);

    const StencilRefMaskParams stencilRefMasks = { 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0x01, 0xFF };

    ViewportParams viewportInfo = { };
    viewportInfo.count                 = 1;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo = { };
    scissorInfo.count = 1;

    const ColorTargetViewInternalCreateInfo colorViewInfoInternal = { };

    ColorTargetViewCreateInfo colorViewInfo = { };
    colorViewInfo.imageInfo.pImage    = &dstImage;
    colorViewInfo.imageInfo.arraySize = 1;
    colorViewInfo.flags.bypassMall = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnCbDbWrite);

    if (dstCreateInfo.imageType == ImageType::Tex3d)
    {
        colorViewInfo.zRange.extent     = 1;
        colorViewInfo.flags.zRangeValid = true;
    }

    BindTargetParams bindTargetsInfo = { };
    bindTargetsInfo.colorTargets[0].imageLayout      = dstImageLayout;
    bindTargetsInfo.colorTargets[0].pColorTargetView = nullptr;

    // Save current command buffer state.
    pCmdBuffer->CmdSaveGraphicsState();
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindColorBlendState(m_pBlendDisableState);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthDisableState);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(dstCreateInfo.samples, dstCreateInfo.fragments));
    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    SubresRange viewRange = { };
    viewRange.numPlanes   = 1;
    viewRange.numMips     = srcCreateInfo.mipLevels;
    // Use the depth of base subresource as the number of array slices since 3D image is viewed as 2D array
    // later. Src image view is set up as a whole rather than per mip-level, using base subresource's depth
    // to cover the MAX_SLICE of all mip-level.
    viewRange.numSlices   =
        (srcCreateInfo.imageType == ImageType::Tex3d) ? srcCreateInfo.extent.depth : srcCreateInfo.arraySize;

    // Keep track of the previous graphics pipeline to reduce the pipeline switching overhead.
    const GraphicsPipeline* pPreviousPipeline = nullptr;

    // Accumulate the restore mask for each region copied.
    uint32 restoreMask = 0;

    // Each region needs to be copied individually.
    for (uint32 region = 0; region < regionCount; ++region)
    {
        // Multiply all x-dimension values in our region by the texel scale.
        ImageCopyRegion copyRegion = pRegions[region];

        // Determine which image formats to use for the copy.
        SwizzledFormat dstFormat    = { };
        SwizzledFormat srcFormat    = { };
        uint32         texelScale   = 1;
        bool           singleSubres = false;

        GetCopyImageFormats(srcImage,
                            srcImageLayout,
                            dstImage,
                            dstImageLayout,
                            copyRegion,
                            flags,
                            &srcFormat,
                            &dstFormat,
                            &texelScale,
                            &singleSubres);

        // Update the color target view format with the destination format.
        colorViewInfo.swizzledFormat = dstFormat;

        // Only switch to the appropriate graphics pipeline if it differs from the previous region's pipeline.
        const GraphicsPipeline*const pPipeline = GetGfxPipelineByTargetIndexAndFormat(Copy_32ABGR, 0, dstFormat);
        if (pPreviousPipeline != pPipeline)
        {
            pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, pPipeline, InternalApiPsoHash, });
            pCmdBuffer->CmdOverwriteRbPlusFormatForBlits(dstFormat, 0);
            pPreviousPipeline = pPipeline;
        }

        if (singleSubres == false)
        {
            // We'll setup both 2D and 3D src images as a 2D view.
            //
            // Is it legal for the shader to view 3D images as 2D?
            ImageViewInfo imageView = {};
            RpmUtil::BuildImageViewInfo(&imageView,
                                        srcImage,
                                        viewRange,
                                        srcFormat,
                                        srcImageLayout,
                                        device.TexOptLevel(),
                                        false);

            // Create an embedded SRD table and bind it to user data 4 for pixel work.
            uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                       SrdDwordAlignment(),
                                                                       SrdDwordAlignment(),
                                                                       PipelineBindPoint::Graphics,
                                                                       4);

            // Populate the table with an image view of the source image.
            device.CreateImageViewSrds(1, &imageView, pSrdTable);
        }

        // Give the gfxip layer a chance to optimize the hardware before we start copying.
        const uint32 bitsPerPixel = Formats::BitsPerPixel(dstFormat.format);
        restoreMask              |= HwlBeginGraphicsCopy(static_cast<Pm4CmdBuffer*>(pCmdBuffer),
                                                         pPipeline,
                                                         dstImage,
                                                         bitsPerPixel);

        // When copying from 3D to 3D, the number of slices should be 1. When copying from
        // 1D to 1D or 2D to 2D, depth should be 1. Therefore when the src image type is identical
        // to the dst image type, either the depth or the number of slices should be equal to 1.
        PAL_ASSERT((srcCreateInfo.imageType != dstCreateInfo.imageType) ||
                   (copyRegion.numSlices == 1) ||
                   (copyRegion.extent.depth == 1));

        // When copying from 2D to 3D or 3D to 2D, the number of slices should match the depth.
        PAL_ASSERT((srcCreateInfo.imageType == dstCreateInfo.imageType) ||
                   ((((srcCreateInfo.imageType == ImageType::Tex3d) &&
                      (dstCreateInfo.imageType == ImageType::Tex2d)) ||
                     ((srcCreateInfo.imageType == ImageType::Tex2d) &&
                      (dstCreateInfo.imageType == ImageType::Tex3d))) &&
                    (copyRegion.numSlices == copyRegion.extent.depth)));

        copyRegion.srcOffset.x  *= texelScale;
        copyRegion.dstOffset.x  *= texelScale;
        copyRegion.extent.width *= texelScale;

        // Setup the viewport and scissor to restrict rendering to the destination region being copied.
        viewportInfo.viewports[0].originX = static_cast<float>(copyRegion.dstOffset.x);
        viewportInfo.viewports[0].originY = static_cast<float>(copyRegion.dstOffset.y);
        viewportInfo.viewports[0].width   = static_cast<float>(copyRegion.extent.width);
        viewportInfo.viewports[0].height  = static_cast<float>(copyRegion.extent.height);

        if (TestAnyFlagSet(flags, CopyEnableScissorTest))
        {
            scissorInfo.scissors[0].offset.x      = pScissorRect->offset.x;
            scissorInfo.scissors[0].offset.y      = pScissorRect->offset.y;
            scissorInfo.scissors[0].extent.width  = pScissorRect->extent.width;
            scissorInfo.scissors[0].extent.height = pScissorRect->extent.height;
        }
        else
        {
            scissorInfo.scissors[0].offset.x      = copyRegion.dstOffset.x;
            scissorInfo.scissors[0].offset.y      = copyRegion.dstOffset.y;
            scissorInfo.scissors[0].extent.width  = copyRegion.extent.width;
            scissorInfo.scissors[0].extent.height = copyRegion.extent.height;
        }

        pCmdBuffer->CmdSetViewports(viewportInfo);
        pCmdBuffer->CmdSetScissorRects(scissorInfo);

        const float texcoordVs[4] =
        {
            static_cast<float>(copyRegion.srcOffset.x),
            static_cast<float>(copyRegion.srcOffset.y),
            static_cast<float>(copyRegion.srcOffset.x + copyRegion.extent.width),
            static_cast<float>(copyRegion.srcOffset.y + copyRegion.extent.height)
        };

        const uint32* pUserDataVs = reinterpret_cast<const uint32*>(&texcoordVs);
        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 0, 4, pUserDataVs);

        // Copy may happen between the layers of a 2d image and the slices of a 3d image.
        const uint32 numSlices = Max(copyRegion.numSlices, copyRegion.extent.depth);

        // Each slice is copied individually, we can optimize this into fewer draw calls if it becomes a
        // performance bottleneck, but for now this is simpler.
        for (uint32 sliceOffset = 0; sliceOffset < numSlices; ++sliceOffset)
        {
            const uint32 srcSlice    = ((srcCreateInfo.imageType == ImageType::Tex3d)
                                        ? copyRegion.srcOffset.z          + sliceOffset
                                        : copyRegion.srcSubres.arraySlice + sliceOffset);

            if (singleSubres)
            {
                const bool singleArrayAccess  = (srcCreateInfo.imageType != ImageType::Tex3d);
                const bool singlezRangeAccess = (srcCreateInfo.imageType == ImageType::Tex3d) &&
                                                HwlNeedSinglezRangeAccess();

                viewRange.numPlanes   = 1;
                viewRange.numMips     = 1;
                viewRange.numSlices   = 1;
                viewRange.startSubres = copyRegion.srcSubres;

                if (singleArrayAccess)
                {
                    viewRange.startSubres.arraySlice += sliceOffset;
                }

                ImageViewInfo imageView = {};
                RpmUtil::BuildImageViewInfo(&imageView,
                                            srcImage,
                                            viewRange,
                                            srcFormat,
                                            srcImageLayout,
                                            device.TexOptLevel(),
                                            false);

                if (singlezRangeAccess)
                {
                    imageView.zRange.offset     = srcSlice;
                    imageView.zRange.extent     = 1;
                    imageView.flags.zRangeValid = 1;
                }

                // Create an embedded SRD table and bind it to user data 4 for pixel work.
                uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                           SrdDwordAlignment(),
                                                                           SrdDwordAlignment(),
                                                                           PipelineBindPoint::Graphics,
                                                                           4);

                // Populate the table with an image view of the source image.
                device.CreateImageViewSrds(1, &imageView, pSrdTable);

                const uint32 userDataPs[2] =
                {
                    (singleArrayAccess || singlezRangeAccess) ? 0 : sliceOffset,
                    0
                };

                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 5, 2, &userDataPs[0]);
            }
            else
            {
                const uint32 userDataPs[2] =
                {
                    srcSlice,
                    copyRegion.srcSubres.mipLevel
                };
                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 5, 2, &userDataPs[0]);
            }

            colorViewInfo.imageInfo.baseSubRes = copyRegion.dstSubres;

            if (dstCreateInfo.imageType == ImageType::Tex3d)
            {
                colorViewInfo.zRange.offset = copyRegion.dstOffset.z + sliceOffset;
            }
            else
            {
                colorViewInfo.imageInfo.baseSubRes.arraySlice = copyRegion.dstSubres.arraySlice + sliceOffset;
            }

            // Create and bind a color-target view for this slice.
            LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

            IColorTargetView* pColorView = nullptr;
            void* pColorViewMem =
                PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

            if (pColorViewMem == nullptr)
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {
                // Since our color target view can only bind 1 slice at a time, we have to issue a separate draw for
                // each slice in extent.z. We can keep the same src image view since we pass the explicit slice to
                // read from in user data, but we'll need to create a new color target view each time.
                Result result = m_pDevice->CreateColorTargetView(colorViewInfo,
                                                                 colorViewInfoInternal,
                                                                 pColorViewMem,
                                                                 &pColorView);
                PAL_ASSERT(result == Result::Success);

                bindTargetsInfo.colorTargets[0].pColorTargetView = pColorView;
                bindTargetsInfo.colorTargetCount = 1;
                pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                // Draw a fullscreen quad.
                pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                // Unbind the color-target view.
                bindTargetsInfo.colorTargetCount = 0;
                pCmdBuffer->CmdBindTargets(bindTargetsInfo);
                PAL_SAFE_FREE(pColorViewMem, &sliceAlloc);
            }
        }
    }

    // Call back to the gfxip layer so it can restore any state it modified previously.
    HwlEndGraphicsCopy(static_cast<Pm4::CmdStream*>(pStream), restoreMask);

    // Restore original command buffer state.
    pCmdBuffer->CmdRestoreGraphicsState();
}

// =====================================================================================================================
// Copies multisampled depth-stencil images using a graphics pipeline.
void RsrcProcMgr::CopyDepthStencilImageGraphics(
    Pm4CmdBuffer*          pCmdBuffer,
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
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const auto& device          = *m_pDevice->Parent();
    const auto* pPublicSettings = device.GetPublicSettings();
    const auto& texOptLevel     = device.TexOptLevel();
    const auto& dstCreateInfo   = dstImage.GetImageCreateInfo();
    const auto& srcCreateInfo   = srcImage.GetImageCreateInfo();

    const StencilRefMaskParams stencilRefMasks = { 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0x01, 0xFF, };

    // Initialize some structures we will need later on.
    ViewportParams viewportInfo = { };
    viewportInfo.count                 = 1;
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

    const DepthStencilViewInternalCreateInfo noDepthViewInfoInternal = { };
    DepthStencilViewCreateInfo               depthViewInfo           = { };
    depthViewInfo.pImage           = &dstImage;
    depthViewInfo.arraySize        = 1;
    depthViewInfo.flags.bypassMall = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnCbDbWrite);

    // Save current command buffer state and bind graphics state which is common for all regions.
    pCmdBuffer->CmdSaveGraphicsState();
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(dstCreateInfo.samples, dstCreateInfo.fragments));
    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    RpmUtil::WriteVsZOut(pCmdBuffer, 1.0f);

    AutoBuffer<bool, 16, Platform> isRangeProcessed(regionCount, m_pDevice->GetPlatform());
    PAL_ASSERT(isRangeProcessed.Capacity() >= regionCount);

    // Notify the command buffer that the AutoBuffer allocation has failed.
    if (isRangeProcessed.Capacity() < regionCount)
    {
        pCmdBuffer->NotifyAllocFailure();
    }
    else
    {
        memset(&isRangeProcessed[0], false, isRangeProcessed.SizeBytes());

        // Now issue fast or slow clears to all ranges, grouping identical depth/stencil pairs if possible.
        for (uint32 idx = 0; idx < regionCount; idx++)
        {
            // Setup the viewport and scissor to restrict rendering to the destination region being copied.
            viewportInfo.viewports[0].originX = static_cast<float>(pRegions[idx].dstOffset.x);
            viewportInfo.viewports[0].originY = static_cast<float>(pRegions[idx].dstOffset.y);
            viewportInfo.viewports[0].width   = static_cast<float>(pRegions[idx].extent.width);
            viewportInfo.viewports[0].height  = static_cast<float>(pRegions[idx].extent.height);

            if (TestAnyFlagSet(flags, CopyEnableScissorTest))
            {
                scissorInfo.scissors[0].offset.x      = pScissorRect->offset.x;
                scissorInfo.scissors[0].offset.y      = pScissorRect->offset.y;
                scissorInfo.scissors[0].extent.width  = pScissorRect->extent.width;
                scissorInfo.scissors[0].extent.height = pScissorRect->extent.height;
            }
            else
            {
                scissorInfo.scissors[0].offset.x      = pRegions[idx].dstOffset.x;
                scissorInfo.scissors[0].offset.y      = pRegions[idx].dstOffset.y;
                scissorInfo.scissors[0].extent.width  = pRegions[idx].extent.width;
                scissorInfo.scissors[0].extent.height = pRegions[idx].extent.height;
            }

            // The shader will calculate src coordinates by adding a delta to the dst coordinates. The user data should
            // contain those deltas which are (srcOffset-dstOffset) for X & Y.
            const int32  xOffset = (pRegions[idx].srcOffset.x - pRegions[idx].dstOffset.x);
            const int32  yOffset = (pRegions[idx].srcOffset.y - pRegions[idx].dstOffset.y);
            const uint32 userData[2] =
            {
                reinterpret_cast<const uint32&>(xOffset),
                reinterpret_cast<const uint32&>(yOffset)
            };

            pCmdBuffer->CmdSetViewports(viewportInfo);
            pCmdBuffer->CmdSetScissorRects(scissorInfo);
            pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 2, 2, userData);

            // To improve performance, input src coordinates to VS, avoid using screen position in PS.
            const float texcoordVs[4] =
            {
                static_cast<float>(pRegions[idx].srcOffset.x),
                static_cast<float>(pRegions[idx].srcOffset.y),
                static_cast<float>(pRegions[idx].srcOffset.x + pRegions[idx].extent.width),
                static_cast<float>(pRegions[idx].srcOffset.y + pRegions[idx].extent.height)
            };

            const uint32* pUserDataVs = reinterpret_cast<const uint32*>(&texcoordVs);
            pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 6, 4, pUserDataVs);

            // Same sanity checks of the region planes.
            const bool isDepth = dstImage.IsDepthPlane(pRegions[idx].dstSubres.plane);
            bool isDepthStencil = false;

            BindTargetParams bindTargetsInfo = {};

            // It's possible that SRC may be not a depth/stencil resource and it's created with X32_UINT from
            // R32_TYPELESS, use DST's format to setup SRC format correctly.
            const ChNumFormat depthFormat = dstImage.GetImageCreateInfo().swizzledFormat.format;

            if (isDepth)
            {
                bindTargetsInfo.depthTarget.depthLayout = dstImageLayout;
            }

            if (dstImage.IsStencilPlane(pRegions[idx].dstSubres.plane))
            {
                bindTargetsInfo.depthTarget.stencilLayout = dstImageLayout;
            }

            // No need to clear a range twice.
            if (isRangeProcessed[idx])
            {
                continue;
            }

            uint32 secondSurface = 0;

            // Search the range list to see if there is a matching range which span the other plane.
            for (uint32 forwardIdx = idx + 1; forwardIdx < regionCount; ++forwardIdx)
            {
                // TODO: there is unknown corruption issue if grouping depth and stencil copy together for mipmap
                //       image, disallow merging copy for mipmap image as a temp fix.
                if ((dstCreateInfo.mipLevels                   == 1)                                  &&
                    (pRegions[forwardIdx].srcSubres.plane      != pRegions[idx].srcSubres.plane)      &&
                    (pRegions[forwardIdx].dstSubres.plane      != pRegions[idx].dstSubres.plane)      &&
                    (pRegions[forwardIdx].srcSubres.mipLevel   == pRegions[idx].srcSubres.mipLevel)   &&
                    (pRegions[forwardIdx].dstSubres.mipLevel   == pRegions[idx].dstSubres.mipLevel)   &&
                    (pRegions[forwardIdx].srcSubres.arraySlice == pRegions[idx].srcSubres.arraySlice) &&
                    (pRegions[forwardIdx].dstSubres.arraySlice == pRegions[idx].dstSubres.arraySlice) &&
                    (pRegions[forwardIdx].extent.depth         == pRegions[idx].extent.depth)         &&
                    (pRegions[forwardIdx].extent.height        == pRegions[idx].extent.height)        &&
                    (pRegions[forwardIdx].extent.width         == pRegions[idx].extent.width)         &&
                    (pRegions[forwardIdx].numSlices            == pRegions[idx].numSlices))
                {
                    // We found a matching range for the other plane, clear them both at once.
                    isDepthStencil = true;
                    isRangeProcessed[forwardIdx] = true;
                    secondSurface = forwardIdx;
                    bindTargetsInfo.depthTarget.stencilLayout = dstImageLayout;
                    break;
                }
            }
            pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics,
                                          GetCopyDepthStencilPipeline(
                                              isDepth,
                                              isDepthStencil,
                                              srcImage.GetImageCreateInfo().samples),
                                          InternalApiPsoHash, });

            // Determine which format we should use to view the source image.
            SwizzledFormat srcFormat =
            {
                ChNumFormat::Undefined,
                { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
            };

            if (isDepthStencil)
            {
                // We should only be in the depth stencil case when we have a depth stencil format
                PAL_ASSERT((depthFormat == ChNumFormat::D32_Float_S8_Uint) ||
                           (depthFormat == ChNumFormat::D16_Unorm_S8_Uint));
                if (depthFormat == ChNumFormat::D32_Float_S8_Uint)
                {
                    srcFormat.format = ChNumFormat::X32_Float;
                }
                else
                {
                    srcFormat.format = ChNumFormat::X16_Unorm;
                }
                pCmdBuffer->CmdBindDepthStencilState(m_pDepthStencilResolveState);
            }
            else if (isDepth)
            {
                if ((depthFormat == ChNumFormat::D32_Float_S8_Uint) || (depthFormat == ChNumFormat::X32_Float))
                {
                    srcFormat.format = ChNumFormat::X32_Float;
                }
                else
                {
                    srcFormat.format = ChNumFormat::X16_Unorm;
                }
                pCmdBuffer->CmdBindDepthStencilState(m_pDepthResolveState);
            }
            else
            {
                srcFormat.format = ChNumFormat::X8_Uint;
                pCmdBuffer->CmdBindDepthStencilState(m_pStencilResolveState);
            }

            for (uint32 slice = 0; slice < pRegions[idx].numSlices; ++slice)
            {
                LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

                // Create an embedded user-data table and bind it to user data 1. We need an image view for each plane.
                const uint32 numSrds = isDepthStencil ? 2 : 1;
                uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                           SrdDwordAlignment() * numSrds,
                                                                           SrdDwordAlignment(),
                                                                           PipelineBindPoint::Graphics,
                                                                           1);

                if (isDepthStencil)
                {
                    // Populate the table with an image view of the source image.
                    ImageViewInfo imageView[2] = {};
                    SubresRange viewRange      = { pRegions[idx].srcSubres, 1, 1, 1 };

                    viewRange.startSubres.arraySlice += slice;

                    RpmUtil::BuildImageViewInfo(&imageView[0],
                                                srcImage,
                                                viewRange,
                                                srcFormat,
                                                srcImageLayout,
                                                texOptLevel,
                                                false);

                    constexpr SwizzledFormat StencilSrcFormat =
                    {
                        ChNumFormat::X8_Uint,
                        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
                    };

                    viewRange = { pRegions[secondSurface].srcSubres, 1, 1, 1 };

                    viewRange.startSubres.arraySlice += slice;

                    RpmUtil::BuildImageViewInfo(&imageView[1],
                                                srcImage,
                                                viewRange,
                                                StencilSrcFormat,
                                                srcImageLayout,
                                                texOptLevel,
                                                false);
                    device.CreateImageViewSrds(2, &imageView[0], pSrdTable);
                }
                else
                {
                    // Populate the table with an image view of the source image.
                    ImageViewInfo imageView = {};
                    SubresRange   viewRange = { pRegions[idx].srcSubres, 1, 1, 1 };

                    viewRange.startSubres.arraySlice += slice;

                    RpmUtil::BuildImageViewInfo(&imageView,
                                                srcImage,
                                                viewRange,
                                                srcFormat,
                                                srcImageLayout,
                                                texOptLevel,
                                                false);
                    device.CreateImageViewSrds(1, &imageView, pSrdTable);
                }

                // Create and bind a depth stencil view of the destination region.
                depthViewInfo.baseArraySlice = pRegions[idx].dstSubres.arraySlice + slice;
                depthViewInfo.mipLevel       = pRegions[idx].dstSubres.mipLevel;

                void* pDepthStencilViewMem = PAL_MALLOC(m_pDevice->GetDepthStencilViewSize(nullptr),
                                                        &sliceAlloc,
                                                        AllocInternalTemp);
                if (pDepthStencilViewMem == nullptr)
                {
                    pCmdBuffer->NotifyAllocFailure();
                }
                else
                {
                    IDepthStencilView* pDepthView = nullptr;
                    Result result = m_pDevice->CreateDepthStencilView(depthViewInfo,
                                                                      noDepthViewInfoInternal,
                                                                      pDepthStencilViewMem,
                                                                      &pDepthView);
                    PAL_ASSERT(result == Result::Success);

                    bindTargetsInfo.depthTarget.pDepthStencilView = pDepthView;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    // Draw a fullscreen quad.
                    pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                    // Unbind the depth view and destroy it.
                    bindTargetsInfo.depthTarget.pDepthStencilView = nullptr;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    PAL_SAFE_FREE(pDepthStencilViewMem, &sliceAlloc);
                }
            } // End for each slice.
        } // End for each region
    }
    // Restore original command buffer state.
    pCmdBuffer->CmdRestoreGraphicsState();
}

// =====================================================================================================================
void RsrcProcMgr::CopyImageGraphics(
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
    if (dstImage.IsDepthStencilTarget())
    {
        CopyDepthStencilImageGraphics(static_cast<Pm4CmdBuffer*>(pCmdBuffer),
                                      srcImage,
                                      srcImageLayout,
                                      dstImage,
                                      dstImageLayout,
                                      regionCount,
                                      pRegions,
                                      pScissorRect,
                                      flags);
    }
    else
    {
        CopyColorImageGraphics(static_cast<Pm4CmdBuffer*>(pCmdBuffer),
                               srcImage,
                               srcImageLayout,
                               dstImage,
                               dstImageLayout,
                               regionCount,
                               pRegions,
                               pScissorRect,
                               flags);
    }
}

// =====================================================================================================================
bool RsrcProcMgr::ScaledCopyImageUseGraphics(
    GfxCmdBuffer*         pCmdBuffer,
    const ScaledCopyInfo& copyInfo
    ) const
{
    const auto&     srcInfo      = copyInfo.pSrcImage->GetImageCreateInfo();
    const auto&     dstInfo      = copyInfo.pDstImage->GetImageCreateInfo();
    const auto*     pDstImage    = static_cast<const Image*>(copyInfo.pDstImage);
    const ImageType srcImageType = srcInfo.imageType;
    const ImageType dstImageType = dstInfo.imageType;

    const bool isDepth      = ((srcInfo.usageFlags.depthStencil != 0) ||
                               (dstInfo.usageFlags.depthStencil != 0) ||
                               Formats::IsDepthStencilOnly(srcInfo.swizzledFormat.format) ||
                               Formats::IsDepthStencilOnly(dstInfo.swizzledFormat.format));
    const bool isCompressed = (Formats::IsBlockCompressed(srcInfo.swizzledFormat.format) ||
                               Formats::IsBlockCompressed(dstInfo.swizzledFormat.format));
    const bool isYuv        = (Formats::IsYuv(srcInfo.swizzledFormat.format) ||
                               Formats::IsYuv(dstInfo.swizzledFormat.format));

    const bool preferGraphicsCopy = Image::PreferGraphicsCopy &&
                                    (PreferComputeForNonLocalDestCopy(*pDstImage) == false);

    // isDepthOrSingleSampleColorFormatSupported is used for depth or single-sample color format checking.
    // IsGfxPipelineForFormatSupported is only relevant for non depth formats.
    const bool isDepthOrSingleSampleColorFormatSupported = isDepth ||
        ((dstInfo.samples == 1) && IsGfxPipelineForFormatSupported(dstInfo.swizzledFormat));

    // We need to decide between the graphics copy path and the compute copy path. The graphics path only supports
    // single-sampled non-compressed, non-YUV 2D or 2D color images, or depth stencil images.
    const bool useGraphicsCopy = ((preferGraphicsCopy && pCmdBuffer->IsGraphicsSupported()) &&
                                  ((srcImageType != ImageType::Tex1d) &&
                                   (dstImageType != ImageType::Tex1d) &&
                                   (isCompressed == false)            &&
                                   (isYuv == false)                   &&
                                   (isDepthOrSingleSampleColorFormatSupported)));

    // Scissor-enabled blit for OGLP is only supported on graphics path.
    PAL_ASSERT(useGraphicsCopy || (copyInfo.flags.scissorTest == 0));

    return useGraphicsCopy;
}

// =====================================================================================================================
void RsrcProcMgr::ScaledCopyImageGraphics(
    GfxCmdBuffer*           pCmdBuffer,
    const ScaledCopyInfo&   copyInfo
    ) const
{
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    // Get some useful information about the image.
    const auto* pSrcImage                 = static_cast<const Image*>(copyInfo.pSrcImage);
    const auto* pDstImage                 = static_cast<const Image*>(copyInfo.pDstImage);
    ImageLayout srcImageLayout            = copyInfo.srcImageLayout;
    ImageLayout dstImageLayout            = copyInfo.dstImageLayout;
    uint32 regionCount                    = copyInfo.regionCount;
    const ImageScaledCopyRegion* pRegions = copyInfo.pRegions;

    const auto& dstCreateInfo    = pDstImage->GetImageCreateInfo();
    const auto& srcCreateInfo    = pSrcImage->GetImageCreateInfo();
    const auto& device           = *m_pDevice->Parent();
    const auto* pPublicSettings  = device.GetPublicSettings();
    const bool isSrcTex3d = srcCreateInfo.imageType == ImageType::Tex3d;
    const bool isDstTex3d = dstCreateInfo.imageType == ImageType::Tex3d;
    const bool depthStencilCopy = ((srcCreateInfo.usageFlags.depthStencil != 0) ||
                                 (dstCreateInfo.usageFlags.depthStencil != 0) ||
                                 Formats::IsDepthStencilOnly(srcCreateInfo.swizzledFormat.format) ||
                                 Formats::IsDepthStencilOnly(dstCreateInfo.swizzledFormat.format));

    Pal::CmdStream*const pStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Graphics);
    PAL_ASSERT(pStream != nullptr);

    const StencilRefMaskParams stencilRefMasks = { 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0x01, 0xFF };

    ViewportParams viewportInfo        = {};
    viewportInfo.count                 = 1;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo = {};
    scissorInfo.count             = 1;

    PAL_ASSERT(pCmdBuffer->GetGfxCmdBufStateFlags().isGfxStatePushed != 0);

    BindCommonGraphicsState(pCmdBuffer);

    pCmdBuffer->CmdBindMsaaState(GetMsaaState(dstCreateInfo.samples, dstCreateInfo.fragments));
    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    uint32 colorKey[4]        = { 0 };
    uint32 alphaDiffMul       = 0;
    float  threshold          = 0.0f;
    uint32 colorKeyEnableMask = 0;

    const ColorTargetViewInternalCreateInfo colorViewInfoInternal    = {};
    ColorTargetViewCreateInfo colorViewInfo                          = {};
    BindTargetParams bindTargetsInfo                                 = {};
    const DepthStencilViewInternalCreateInfo noDepthViewInfoInternal = {};
    DepthStencilViewCreateInfo depthViewInfo                         = {};

    colorViewInfo.flags.bypassMall = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnCbDbWrite);
    depthViewInfo.flags.bypassMall = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnCbDbWrite);

    if (!depthStencilCopy)
    {
        if (copyInfo.flags.srcColorKey)
        {
            colorKeyEnableMask = 1;
        }
        else if (copyInfo.flags.dstColorKey)
        {
            colorKeyEnableMask = 2;
        }

        if (colorKeyEnableMask > 0)
        {
            const bool srcColorKey = (colorKeyEnableMask == 1);

            PAL_ASSERT(copyInfo.pColorKey != nullptr);
            PAL_ASSERT(srcCreateInfo.imageType == ImageType::Tex2d);
            PAL_ASSERT(dstCreateInfo.imageType == ImageType::Tex2d);
            PAL_ASSERT(srcCreateInfo.samples <= 1);
            PAL_ASSERT(dstCreateInfo.samples <= 1);

            memcpy(&colorKey[0], &copyInfo.pColorKey->u32Color[0], sizeof(colorKey));

            // Convert uint color key to float representation
            SwizzledFormat format = srcColorKey ? srcCreateInfo.swizzledFormat : dstCreateInfo.swizzledFormat;
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

        colorViewInfo.imageInfo.pImage    = copyInfo.pDstImage;
        colorViewInfo.imageInfo.arraySize = 1;

        if (isDstTex3d)
        {
            colorViewInfo.zRange.extent     = 1;
            colorViewInfo.flags.zRangeValid = true;
        }

        bindTargetsInfo.colorTargets[0].imageLayout      = dstImageLayout;
        bindTargetsInfo.colorTargets[0].pColorTargetView = nullptr;

        pCmdBuffer->CmdBindDepthStencilState(m_pDepthDisableState);

        if (copyInfo.flags.srcAlpha)
        {
            pCmdBuffer->CmdBindColorBlendState(m_pColorBlendState);
        }
        else
        {
            pCmdBuffer->CmdBindColorBlendState(m_pBlendDisableState);
        }
    }
    else
    {
        depthViewInfo.pImage    = pDstImage;
        depthViewInfo.arraySize = 1;
        RpmUtil::WriteVsZOut(pCmdBuffer, 1.0f);
    }

    // Keep track of the previous graphics pipeline to reduce the pipeline switching overhead.
    uint64 rangeMask = 0;
    const GraphicsPipeline* pPreviousPipeline = nullptr;

    // Accumulate the restore mask for each region copied.
    uint32 restoreMask = 0;

    // Each region needs to be copied individually.
    for (uint32 region = 0; region < regionCount; ++region)
    {
        // Multiply all x-dimension values in our region by the texel scale.
        ImageScaledCopyRegion copyRegion = pRegions[region];

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

        float src3dScale = 0;
        float src3dOffset = 0;

        if ((absDstExtentW > 0) && (absDstExtentH > 0) && (absDstExtentD > 0))
        {
            // A negative extent means that we should do a reverse the copy.
            // We want to always use the absolute value of dstExtent.
            // If dstExtent is negative in one dimension, then we negate srcExtent in that dimension,
            // and we adjust the offsets as well.
            if (copyInfo.flags.coordsInFloat != 0)
            {
                if (copyRegion.dstExtentFloat.width < 0)
                {
                    copyRegion.dstOffsetFloat.x     = copyRegion.dstOffsetFloat.x + copyRegion.dstExtentFloat.width;
                    copyRegion.srcOffsetFloat.x     = copyRegion.srcOffsetFloat.x + copyRegion.srcExtentFloat.width;
                    copyRegion.srcExtentFloat.width = -copyRegion.srcExtentFloat.width;
                    copyRegion.dstExtentFloat.width = -copyRegion.dstExtentFloat.width;
                }

                if (copyRegion.dstExtentFloat.height < 0)
                {
                    copyRegion.dstOffsetFloat.y      = copyRegion.dstOffsetFloat.y + copyRegion.dstExtentFloat.height;
                    copyRegion.srcOffsetFloat.y      = copyRegion.srcOffsetFloat.y + copyRegion.srcExtentFloat.height;
                    copyRegion.srcExtentFloat.height = -copyRegion.srcExtentFloat.height;
                    copyRegion.dstExtentFloat.height = -copyRegion.dstExtentFloat.height;
                }

                if (copyRegion.dstExtentFloat.depth < 0)
                {
                    copyRegion.dstOffsetFloat.z     = copyRegion.dstOffsetFloat.z + copyRegion.dstExtentFloat.depth;
                    copyRegion.srcOffsetFloat.z     = copyRegion.srcOffsetFloat.z + copyRegion.srcExtentFloat.depth;
                    copyRegion.srcExtentFloat.depth = -copyRegion.srcExtentFloat.depth;
                    copyRegion.dstExtentFloat.depth = -copyRegion.dstExtentFloat.depth;
                }
            }
            else
            {
                if (copyRegion.dstExtent.width < 0)
                {
                    copyRegion.dstOffset.x     = copyRegion.dstOffset.x + copyRegion.dstExtent.width;
                    copyRegion.srcOffset.x     = copyRegion.srcOffset.x + copyRegion.srcExtent.width;
                    copyRegion.srcExtent.width = -copyRegion.srcExtent.width;
                    copyRegion.dstExtent.width = -copyRegion.dstExtent.width;
                }

                if (copyRegion.dstExtent.height < 0)
                {
                    copyRegion.dstOffset.y      = copyRegion.dstOffset.y + copyRegion.dstExtent.height;
                    copyRegion.srcOffset.y      = copyRegion.srcOffset.y + copyRegion.srcExtent.height;
                    copyRegion.srcExtent.height = -copyRegion.srcExtent.height;
                    copyRegion.dstExtent.height = -copyRegion.dstExtent.height;
                }

                if (copyRegion.dstExtent.depth < 0)
                {
                    copyRegion.dstOffset.z     = copyRegion.dstOffset.z + copyRegion.dstExtent.depth;
                    copyRegion.srcOffset.z     = copyRegion.srcOffset.z + copyRegion.srcExtent.depth;
                    copyRegion.srcExtent.depth = -copyRegion.srcExtent.depth;
                    copyRegion.dstExtent.depth = -copyRegion.dstExtent.depth;
                }
            }

            // The shader expects the region data to be arranged as follows for each dispatch:
            // Src Normalized Left,  Src Normalized Top,Src Normalized Right, SrcNormalized Bottom.
            const Extent3d& srcExtent = pSrcImage->SubresourceInfo(copyRegion.srcSubres)->extentTexels;
            float srcLeft   = 0;
            float srcTop    = 0;
            float srcRight  = 0;
            float srcBottom = 0;

            float dstLeft   = 0;
            float dstTop    = 0;
            float dstRight  = 0;
            float dstBottom = 0;

            if (copyInfo.flags.coordsInFloat != 0)
            {
                srcLeft   = copyRegion.srcOffsetFloat.x / srcExtent.width;
                srcTop    = copyRegion.srcOffsetFloat.y / srcExtent.height;
                srcRight  = (copyRegion.srcOffsetFloat.x + copyRegion.srcExtentFloat.width) / srcExtent.width;
                srcBottom = (copyRegion.srcOffsetFloat.y + copyRegion.srcExtentFloat.height) / srcExtent.height;

                dstLeft   = copyRegion.dstOffsetFloat.x;
                dstTop    = copyRegion.dstOffsetFloat.y;
                dstRight  = copyRegion.dstOffsetFloat.x + copyRegion.dstExtentFloat.width;
                dstBottom = copyRegion.dstOffsetFloat.y + copyRegion.dstExtentFloat.height;
            }
            else
            {
                srcLeft   = (1.f * copyRegion.srcOffset.x) / srcExtent.width;
                srcTop    = (1.f * copyRegion.srcOffset.y) / srcExtent.height;
                srcRight  = (1.f * (copyRegion.srcOffset.x + copyRegion.srcExtent.width)) / srcExtent.width;
                srcBottom = (1.f * (copyRegion.srcOffset.y + copyRegion.srcExtent.height)) / srcExtent.height;

                dstLeft   = 1.f * copyRegion.dstOffset.x;
                dstTop    = 1.f * copyRegion.dstOffset.y;
                dstRight  = 1.f * (copyRegion.dstOffset.x + copyRegion.dstExtent.width);
                dstBottom = 1.f * (copyRegion.dstOffset.y + copyRegion.dstExtent.height);
            }

            PAL_ASSERT((srcLeft   >= 0.0f)   && (srcLeft   <= 1.0f) &&
                       (srcTop    >= 0.0f)   && (srcTop    <= 1.0f) &&
                       (srcRight  >= 0.0f)   && (srcRight  <= 1.0f) &&
                       (srcBottom >= 0.0f)   && (srcBottom <= 1.0f));

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

            const uint32 texcoordVs[4] =
            {
                reinterpret_cast<const uint32&>(dstLeft),
                reinterpret_cast<const uint32&>(dstTop),
                reinterpret_cast<const uint32&>(dstRight),
                reinterpret_cast<const uint32&>(dstBottom),
            };

            const uint32 userData[10] =
            {
                reinterpret_cast<const uint32&>(srcLeft),
                reinterpret_cast<const uint32&>(srcTop),
                reinterpret_cast<const uint32&>(srcRight),
                reinterpret_cast<const uint32&>(srcBottom),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][0]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][1]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][2]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][3]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][4]),
                reinterpret_cast<const uint32&>(RotationParams[rotationIndex][5]),
            };

            if (!depthStencilCopy)
            {
                if (isSrcTex3d)
                {
                    // For 3d texture, the cb0 contains the allow data.
                    // cb0[0].xyzw = src   : {  left,    top,  right,  bottom}
                    // cb0[1].xyzw = slice : {scaler, offset, number,    none}
                    const float src3dNumSlice = static_cast<float>(srcExtent.depth);
                    const float dstNumSlice = static_cast<float>(isDstTex3d ? absDstExtentD : copyRegion.numSlices);

                    src3dScale = copyRegion.srcExtent.depth / dstNumSlice;
                    src3dOffset = static_cast<float>(copyRegion.srcOffset.z) + 0.5f * src3dScale;

                    const uint32 userData3d[8] =
                    {
                        reinterpret_cast<const uint32&>(srcLeft),
                        reinterpret_cast<const uint32&>(srcTop),
                        reinterpret_cast<const uint32&>(srcRight),
                        reinterpret_cast<const uint32&>(srcBottom),
                        reinterpret_cast<const uint32&>(src3dScale),
                        reinterpret_cast<const uint32&>(src3dOffset),
                        reinterpret_cast<const uint32&>(src3dNumSlice),
                        0,
                    };
                    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 1, 8, &userData3d[0]);
                }
                else
                {
                    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 1, 4, &texcoordVs[0]);
                    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 5, 10, &userData[0]);
                }
            }
            else
            {
                const uint32 extent[2] = { srcExtent.width, srcExtent.height };
                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 2, 10, &userData[0]);
                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 13, 2, &extent[0]);
            }
        }

        // Determine which image formats to use for the copy.
        SwizzledFormat srcFormat = pSrcImage->SubresourceInfo(copyRegion.srcSubres)->format;
        SwizzledFormat dstFormat = pDstImage->SubresourceInfo(copyRegion.dstSubres)->format;
        if (Formats::IsUndefined(copyRegion.swizzledFormat.format) == false)
        {
            srcFormat = copyRegion.swizzledFormat;
            dstFormat = copyRegion.swizzledFormat;
        }

        // Non-SRGB can be treated as SRGB when copying to non-srgb image
        if (copyInfo.flags.dstAsSrgb)
        {
            dstFormat.format = Formats::ConvertToSrgb(dstFormat.format);
            PAL_ASSERT(Formats::IsUndefined(dstFormat.format) == false);
        }
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 742
        // srgb can be treated as non-srgb when copying to srgb image
        else if (copyInfo.flags.dstAsNorm)
        {
            dstFormat.format = Formats::ConvertToUnorm(dstFormat.format);
            PAL_ASSERT(Formats::IsUndefined(dstFormat.format) == false);
        }
#endif

        uint32 sizeInDwords                 = 0;
        constexpr uint32 ColorKeyDataDwords = 7;
        const GraphicsPipeline* pPipeline   = nullptr;

        const bool isDepth = pDstImage->IsDepthPlane(copyRegion.dstSubres.plane);
        bool isDepthStencil  = false;
        uint32 secondSurface = 0;

        if (!depthStencilCopy)
        {
            // Update the color target view format with the destination format.
            colorViewInfo.swizzledFormat = dstFormat;

            if (isSrcTex3d == false)
            {
                if (colorKeyEnableMask)
                {
                    // There is no UINT/SINT formats in DX9 and only legacy formats <= 32 bpp can be used in color key blit.
                    const uint32 bpp = Formats::BytesPerPixel(srcFormat.format);
                    PAL_ASSERT(bpp <= 32);
                    pPipeline = GetGfxPipeline(ScaledCopyImageColorKey);
                }
                else
                {
                    pPipeline = GetGfxPipelineByTargetIndexAndFormat(ScaledCopy2d_32ABGR, 0, dstFormat);
                }
            }
            else
            {
                pPipeline = GetGfxPipelineByTargetIndexAndFormat(ScaledCopy3d_32ABGR, 0, dstFormat);
            }

            if (colorKeyEnableMask)
            {
                // Create an embedded SRD table and bind it to user data 0. We need image views and
                // a sampler for the src and dest subresource, as well as some inline constants for src and dest
                // color key for 2d texture copy. Only need image view and a sampler for the src subresource
                // as not support color key for 3d texture copy.
                sizeInDwords = SrdDwordAlignment() * 3 + ColorKeyDataDwords;
            }
            else
            {
                // If color Key is not enabled, the ps shader don't need to allocate memory for copydata.
                sizeInDwords = SrdDwordAlignment() * 2;
            }
        }
        else
        {
            if (isDepth)
            {
                bindTargetsInfo.depthTarget.depthLayout = dstImageLayout;
            }

            if (pDstImage->IsStencilPlane(copyRegion.dstSubres.plane))
            {
                bindTargetsInfo.depthTarget.stencilLayout = dstImageLayout;
            }

            // No need to copy a range twice.
            if (BitfieldIsSet(rangeMask, region))
            {
                continue;
            }

            // Search the range list to see if there is a matching range which span the other plane.
            for (uint32 forwardIdx = region + 1; forwardIdx < regionCount; ++forwardIdx)
            {
                // TODO: there is unknown corruption issue if grouping depth and stencil copy together for mipmap
                //       image, disallow merging copy for mipmap image as a temp fix.
                if ((dstCreateInfo.mipLevels                  == 1)                                &&
                    (pRegions[forwardIdx].srcSubres.plane     != copyRegion.srcSubres.plane)       &&
                    (pRegions[forwardIdx].dstSubres.plane     != copyRegion.dstSubres.plane)       &&
                    (pRegions[forwardIdx].srcSubres.mipLevel   == copyRegion.srcSubres.mipLevel)   &&
                    (pRegions[forwardIdx].dstSubres.mipLevel   == copyRegion.dstSubres.mipLevel)   &&
                    (pRegions[forwardIdx].srcSubres.arraySlice == copyRegion.srcSubres.arraySlice) &&
                    (pRegions[forwardIdx].dstSubres.arraySlice == copyRegion.dstSubres.arraySlice) &&
                    (pRegions[forwardIdx].dstExtent.depth      == copyRegion.dstExtent.depth)      &&
                    (pRegions[forwardIdx].dstExtent.height     == copyRegion.dstExtent.height)     &&
                    (pRegions[forwardIdx].dstExtent.width      == copyRegion.dstExtent.width)      &&
                    (pRegions[forwardIdx].numSlices            == copyRegion.numSlices))
                {
                    // We found a matching range for the other plane, copy them both at once.
                    isDepthStencil = true;
                    secondSurface  = forwardIdx;
                    BitfieldUpdateSubfield<uint64>(&rangeMask, UINT64_MAX, 1ULL);
                    break;
                }
            }

            if (isDepthStencil)
            {
                pCmdBuffer->CmdBindDepthStencilState(m_pDepthStencilResolveState);
            }
            else if (isDepth)
            {
                pCmdBuffer->CmdBindDepthStencilState(m_pDepthResolveState);
            }
            else
            {
                pCmdBuffer->CmdBindDepthStencilState(m_pStencilResolveState);
            }

            pPipeline = GetScaledCopyDepthStencilPipeline(isDepth, isDepthStencil, pSrcImage->GetImageCreateInfo().samples);

            sizeInDwords = isDepthStencil ? SrdDwordAlignment() * 3 : SrdDwordAlignment() * 2;

            if (pSrcImage->GetImageCreateInfo().samples > 1)
            {
                // HW doesn't support image Opcode for msaa image with sampler, needn't sampler srd for msaa image sampler.
                sizeInDwords = isDepthStencil ? SrdDwordAlignment() * 2 : SrdDwordAlignment() * 1;
            }
            else
            {
                sizeInDwords = isDepthStencil ? SrdDwordAlignment() * 3 : SrdDwordAlignment() * 2;
            }
        }

        // Only switch to the appropriate graphics pipeline if it differs from the previous region's pipeline.
        if (pPreviousPipeline != pPipeline)
        {
            pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, pPipeline, InternalApiPsoHash, });

            if (!depthStencilCopy)
            {
                pCmdBuffer->CmdOverwriteRbPlusFormatForBlits(dstFormat, 0);
            }

            pPreviousPipeline = pPipeline;
        }

        // Give the gfxip layer a chance to optimize the hardware before we start copying.
        const uint32 bitsPerPixel = Formats::BitsPerPixel(dstFormat.format);
        restoreMask              |= HwlBeginGraphicsCopy(static_cast<Pm4CmdBuffer*>(pCmdBuffer),
                                                         pPipeline,
                                                         *pDstImage,
                                                         bitsPerPixel);

        // When copying from 3D to 3D, the number of slices should be 1. When copying from
        // 1D to 1D or 2D to 2D, depth should be 1. Therefore when the src image type is identical
        // to the dst image type, either the depth or the number of slices should be equal to 1.
        PAL_ASSERT((srcCreateInfo.imageType != dstCreateInfo.imageType) ||
                   (copyRegion.numSlices == 1)                          ||
                   (copyRegion.srcExtent.depth == 1));

        // When copying from 2D to 3D or 3D to 2D, the number of slices should match the depth.
        PAL_ASSERT((srcCreateInfo.imageType == dstCreateInfo.imageType) ||
                   ((((srcCreateInfo.imageType == ImageType::Tex3d)     &&
                      (dstCreateInfo.imageType == ImageType::Tex2d))    ||
                     ((srcCreateInfo.imageType == ImageType::Tex2d)     &&
                      (dstCreateInfo.imageType == ImageType::Tex3d)))   &&
                    (copyRegion.numSlices == static_cast<uint32>(copyRegion.dstExtent.depth))));

        // Setup the viewport and scissor to restrict rendering to the destination region being copied.
        if (copyInfo.flags.coordsInFloat != 0)
        {
            viewportInfo.viewports[0].originX = copyRegion.dstOffsetFloat.x;
            viewportInfo.viewports[0].originY = copyRegion.dstOffsetFloat.y;
            viewportInfo.viewports[0].width   = copyRegion.dstExtentFloat.width;
            viewportInfo.viewports[0].height  = copyRegion.dstExtentFloat.height;
        }
        else
        {
            viewportInfo.viewports[0].originX = static_cast<float>(copyRegion.dstOffset.x);
            viewportInfo.viewports[0].originY = static_cast<float>(copyRegion.dstOffset.y);
            viewportInfo.viewports[0].width   = static_cast<float>(copyRegion.dstExtent.width);
            viewportInfo.viewports[0].height  = static_cast<float>(copyRegion.dstExtent.height);
        }

        if (copyInfo.flags.scissorTest != 0)
        {
            scissorInfo.scissors[0].offset.x      = copyInfo.pScissorRect->offset.x;
            scissorInfo.scissors[0].offset.y      = copyInfo.pScissorRect->offset.y;
            scissorInfo.scissors[0].extent.width  = copyInfo.pScissorRect->extent.width;
            scissorInfo.scissors[0].extent.height = copyInfo.pScissorRect->extent.height;
        }
        else
        {
            if (copyInfo.flags.coordsInFloat != 0)
            {
                scissorInfo.scissors[0].offset.x      = static_cast<int32>(copyRegion.dstOffsetFloat.x + 0.5f);
                scissorInfo.scissors[0].offset.y      = static_cast<int32>(copyRegion.dstOffsetFloat.y + 0.5f);
                scissorInfo.scissors[0].extent.width  = static_cast<int32>(copyRegion.dstExtentFloat.width + 0.5f);
                scissorInfo.scissors[0].extent.height = static_cast<int32>(copyRegion.dstExtentFloat.height + 0.5f);
            }
            else
            {
                scissorInfo.scissors[0].offset.x      = copyRegion.dstOffset.x;
                scissorInfo.scissors[0].offset.y      = copyRegion.dstOffset.y;
                scissorInfo.scissors[0].extent.width  = copyRegion.dstExtent.width;
                scissorInfo.scissors[0].extent.height = copyRegion.dstExtent.height;
            }
        }

        pCmdBuffer->CmdSetViewports(viewportInfo);
        pCmdBuffer->CmdSetScissorRects(scissorInfo);

        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   sizeInDwords,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Graphics,
                                                                   !depthStencilCopy ? 0 : 1);

        ImageViewInfo imageView[2] = {};
        SubresRange   viewRange = { copyRegion.srcSubres, 1, 1, copyRegion.numSlices };

        RpmUtil::BuildImageViewInfo(&imageView[0],
                                    *pSrcImage,
                                    viewRange,
                                    srcFormat,
                                    srcImageLayout,
                                    device.TexOptLevel(),
                                    false);

        if (!depthStencilCopy)
        {
            if (colorKeyEnableMask)
            {
                // Note that this is a read-only view of the destination.
                viewRange.startSubres = copyRegion.dstSubres;
                RpmUtil::BuildImageViewInfo(&imageView[1],
                                            *pDstImage,
                                            viewRange,
                                            dstFormat,
                                            dstImageLayout,
                                            device.TexOptLevel(),
                                            true);
                PAL_ASSERT(imageView[1].viewType == ImageViewType::Tex2d);
            }

            // Populate the table with image views of the source and dest image for 2d texture.
            // Only populate the table with an image view of the source image for 3d texutre.
            const uint32 imageCount = colorKeyEnableMask ? 2 : 1;
            device.CreateImageViewSrds(imageCount, &imageView[0], pSrdTable);
            pSrdTable += SrdDwordAlignment() * imageCount;

            SamplerInfo samplerInfo = {};
            samplerInfo.filter      = copyInfo.filter;
            samplerInfo.addressU    = TexAddressMode::Clamp;
            samplerInfo.addressV    = TexAddressMode::Clamp;
            samplerInfo.addressW    = TexAddressMode::Clamp;
            samplerInfo.compareFunc = CompareFunc::Always;
            device.CreateSamplerSrds(1, &samplerInfo, pSrdTable);
            pSrdTable += SrdDwordAlignment();

            // Copy the copy parameters into the embedded user-data space for 2d texture copy.
            if (colorKeyEnableMask)
            {
                PAL_ASSERT(isSrcTex3d == false);
                uint32 copyData[ColorKeyDataDwords] =
                {
                    colorKeyEnableMask,
                    alphaDiffMul,
                    Util::Math::FloatToBits(threshold),
                    colorKey[0],
                    colorKey[1],
                    colorKey[2],
                    colorKey[3],
                };

                memcpy(pSrdTable, &copyData[0], sizeof(copyData));
            }
        }
        else
        {
            if (isDepthStencil)
            {
                constexpr SwizzledFormat StencilSrcFormat =
                {
                    ChNumFormat::X8_Uint,
                    { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
                };

                viewRange = { pRegions[secondSurface].srcSubres, 1, 1, copyRegion.numSlices };

                RpmUtil::BuildImageViewInfo(&imageView[1],
                                            *pSrcImage,
                                            viewRange,
                                            StencilSrcFormat,
                                            srcImageLayout,
                                            device.TexOptLevel(),
                                            false);
                device.CreateImageViewSrds(2, &imageView[0], pSrdTable);
                pSrdTable += SrdDwordAlignment() * 2;
            }
            else
            {
                device.CreateImageViewSrds(1, &imageView[0], pSrdTable);
                pSrdTable += SrdDwordAlignment();
            }

            if (pSrcImage->GetImageCreateInfo().samples == 1)
            {
                SamplerInfo samplerInfo = {};
                samplerInfo.filter      = copyInfo.filter;
                samplerInfo.addressU    = TexAddressMode::Clamp;
                samplerInfo.addressV    = TexAddressMode::Clamp;
                samplerInfo.addressW    = TexAddressMode::Clamp;
                samplerInfo.compareFunc = CompareFunc::Always;
                device.CreateSamplerSrds(1, &samplerInfo, pSrdTable);
                pSrdTable += SrdDwordAlignment();
            }
        }

        // Copy may happen between the layers of a 2d image and the slices of a 3d image.
        uint32 numSlices = Max(copyRegion.numSlices, absDstExtentD);

        // In default case, each slice is copied individually.
        uint32 vertexCnt = 3;

        // The multi-slice draw will be used only when the copy happends between two 3d textures.
        if (isSrcTex3d && isDstTex3d)
        {
            colorViewInfo.zRange.extent = numSlices;
            vertexCnt *= numSlices;
            numSlices = 1;
        }

        // Each slice is copied individually, we can optimize this into fewer draw calls if it becomes a
        // performance bottleneck, but for now this is simpler.
        for (uint32 sliceOffset = 0; sliceOffset < numSlices; ++sliceOffset)
        {
            const float src3dSlice    = src3dScale * static_cast<float>(sliceOffset) + src3dOffset;
            const float src2dSlice    = static_cast<const float>(sliceOffset);
            const uint32 srcSlice     = isSrcTex3d
                                        ? reinterpret_cast<const uint32&>(src3dSlice)
                                        : reinterpret_cast<const uint32&>(src2dSlice);

            const uint32 userData[1] =
            {
                srcSlice
            };

            // Create and bind a color-target view or depth stencil view for this slice.
            LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

            if (!depthStencilCopy)
            {
                if (isSrcTex3d)
                {
                    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 6, 1, &userData[0]);
                }
                else
                {
                    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 15, 1, &userData[0]);
                }

                colorViewInfo.imageInfo.baseSubRes = copyRegion.dstSubres;

                if (isDstTex3d)
                {
                    colorViewInfo.zRange.offset = copyRegion.dstOffset.z + sliceOffset;
                }
                else
                {
                    colorViewInfo.imageInfo.baseSubRes.arraySlice = copyRegion.dstSubres.arraySlice + sliceOffset;
                }

                IColorTargetView* pColorView = nullptr;
                void* pColorViewMem =
                    PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

                if (pColorViewMem == nullptr)
                {
                    pCmdBuffer->NotifyAllocFailure();
                }
                else
                {
                    // Since our color target view can only bind 1 slice at a time, we have to issue a separate draw for
                    // each slice in extent.z. We can keep the same src image view since we pass the explicit slice to
                    // read from in user data, but we'll need to create a new color target view each time.
                    Result result = m_pDevice->CreateColorTargetView(colorViewInfo,
                                                                     colorViewInfoInternal,
                                                                     pColorViewMem,
                                                                     &pColorView);
                    PAL_ASSERT(result == Result::Success);

                    bindTargetsInfo.colorTargets[0].pColorTargetView = pColorView;
                    bindTargetsInfo.colorTargetCount                 = 1;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    // Draw a fullscreen quad.
                    pCmdBuffer->CmdDraw(0, vertexCnt, 0, 1, 0);

                    // Unbind the color-target view.
                    bindTargetsInfo.colorTargetCount = 0;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);
                    PAL_SAFE_FREE(pColorViewMem, &sliceAlloc);
                }
            }
            else
            {
                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 12, 1, &userData[0]);

                // Create and bind a depth stencil view of the destination region.
                depthViewInfo.baseArraySlice = copyRegion.dstSubres.arraySlice + sliceOffset;
                depthViewInfo.mipLevel       = copyRegion.dstSubres.mipLevel;

                void* pDepthStencilViewMem = PAL_MALLOC(m_pDevice->GetDepthStencilViewSize(nullptr),
                                                        &sliceAlloc,
                                                        AllocInternalTemp);
                if (pDepthStencilViewMem == nullptr)
                {
                    pCmdBuffer->NotifyAllocFailure();
                }
                else
                {
                    IDepthStencilView* pDepthView = nullptr;
                    Result result = m_pDevice->CreateDepthStencilView(depthViewInfo,
                                                                      noDepthViewInfoInternal,
                                                                      pDepthStencilViewMem,
                                                                      &pDepthView);
                    PAL_ASSERT(result == Result::Success);

                    bindTargetsInfo.depthTarget.pDepthStencilView = pDepthView;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    // Draw a fullscreen quad.
                    pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                    // Unbind the depth view and destroy it.
                    bindTargetsInfo.depthTarget.pDepthStencilView = nullptr;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    PAL_SAFE_FREE(pDepthStencilViewMem, &sliceAlloc);
                }
            }
        }
    }
    // Call back to the gfxip layer so it can restore any state it modified previously.
    HwlEndGraphicsCopy(static_cast<Pm4::CmdStream*>(pStream), restoreMask);
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
    Pm4CmdBuffer*               pPm4CmdBuffer = static_cast<Pm4CmdBuffer*>(pCmdBuffer);
    Pm4Image*                   pPm4Image     = static_cast<Pm4Image*>(dstImage.GetGfxImage());
    const auto&                 createInfo    = dstImage.GetImageCreateInfo();
    const SubResourceInfo*const pStartSubRes  = dstImage.SubresourceInfo(pRanges[0].startSubres);
    const bool                  hasBoxes      = (boxCount > 0);

    // If a clearFormat has been specified, assert that it is compatible with the image's selected DCC encoding. This
    // should guard against compression-related corruption, and should always be true if the clearFormat is one of the
    // pViewFormat's specified at image-creation time.
    PAL_ASSERT((clearFormat.format == ChNumFormat::Undefined) ||
               (m_pDevice->ComputeDccFormatEncoding(createInfo.swizzledFormat, &clearFormat, 1) >=
                dstImage.GetImageInfo().dccFormatEncoding));

    const bool clearBoxCoversWholeImage = ((hasBoxes == false)                                    ||
                                           ((boxCount                 == 1)                       &&
                                            (pBoxes[0].offset.x       == 0)                       &&
                                            (pBoxes[0].offset.y       == 0)                       &&
                                            (pBoxes[0].offset.z       == 0)                       &&
                                            (createInfo.extent.width  == pBoxes[0].extent.width)  &&
                                            (createInfo.extent.height == pBoxes[0].extent.height) &&
                                            (createInfo.extent.depth  == pBoxes[0].extent.depth)));

    const bool skipIfSlow          = TestAnyFlagSet(flags, ColorClearSkipIfSlow);
    const bool needPreComputeSync  = TestAnyFlagSet(flags, ColorClearAutoSync);
    bool       needPostComputeSync = false;
    bool       csFastClear         = false;

    for (uint32 rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx)
    {
        PAL_ASSERT(pRanges[rangeIdx].numPlanes == 1);

        SubresRange minSlowClearRange = {};
        const auto* pSlowClearRange   = &minSlowClearRange;
        const auto& clearRange        = pRanges[rangeIdx];
        ClearMethod slowClearMethod   = Image::DefaultSlowClearMethod;

        uint32 convertedColor[4] = { 0 };
        if (color.type == ClearColorType::Float)
        {
            const SwizzledFormat& baseFormat = dstImage.SubresourceInfo(clearRange.startSubres)->format;
            Formats::ConvertColor(baseFormat, &color.f32Color[0], &convertedColor[0]);
        }
        else
        {
            memcpy(&convertedColor[0], &color.u32Color[0], sizeof(convertedColor));
        }

        // Note that fast clears don't support sub-rect clears so we skip them if we have any boxes.  Futher, we only
        // can store one fast clear color per mip level, and therefore can only support fast clears when a range covers
        // all slices.
        // Fast clear is only usable when all channels of the color are being written.
        if ((color.disabledChannelMask == 0) &&
            clearBoxCoversWholeImage         &&
            // If the client is requesting slow clears, then we don't want to do a fast clear here.
            (TestAnyFlagSet(flags, ClearColorImageFlags::ColorClearForceSlow) == false) &&
            pPm4Image->IsFastColorClearSupported(pPm4CmdBuffer, dstImageLayout, &convertedColor[0], clearRange))
        {
            // Assume that all portions of the original range can be fast cleared.
            SubresRange fastClearRange = clearRange;

            // Assume that no portion of the original range needs to be slow cleared.
            minSlowClearRange.startSubres = clearRange.startSubres;
            minSlowClearRange.numPlanes   = clearRange.numPlanes;
            minSlowClearRange.numSlices   = clearRange.numSlices;
            minSlowClearRange.numMips     = 0;

            for (uint32 mipIdx = 0; mipIdx < clearRange.numMips; ++mipIdx)
            {
                const SubresId    subres      = { clearRange.startSubres.plane,
                                                  clearRange.startSubres.mipLevel + mipIdx,
                                                  0 };
                const ClearMethod clearMethod = dstImage.SubresourceInfo(subres)->clearMethod;

                if (clearMethod != ClearMethod::Fast)
                {
                    fastClearRange.numMips = mipIdx;

                    minSlowClearRange.startSubres.mipLevel = subres.mipLevel;
                    minSlowClearRange.numMips              = clearRange.numMips - mipIdx;
                    slowClearMethod                        = clearMethod;
                    break;
                }
            }

            if (fastClearRange.numMips != 0)
            {
                if (needPreComputeSync)
                {
                    PreComputeColorClearSync(pPm4CmdBuffer,
                                             &dstImage,
                                             pRanges[rangeIdx],
                                             dstImageLayout);

                    needPostComputeSync = true;
                    csFastClear         = true;
                }

                // Hand off to the HWL to perform the fast-clear.
                PAL_ASSERT(dstImage.IsRenderTarget());

                HwlFastColorClear(pPm4CmdBuffer,
                                  *pPm4Image,
                                  &convertedColor[0],
                                  clearFormat,
                                  fastClearRange);
            }
        }
        else
        {
            // Since fast clears aren't available, the slow-clear range is everything the caller asked for.
            pSlowClearRange = &clearRange;
        }

        // If we couldn't fast clear every range, then we need to slow clear whatever is left over.
        if ((pSlowClearRange->numMips != 0) && (skipIfSlow == false))
        {
            if (SlowClearUseGraphics(pPm4CmdBuffer,
                                     dstImage,
                                     *pSlowClearRange,
                                     slowClearMethod))
            {
                SlowClearGraphics(pPm4CmdBuffer,
                                  dstImage,
                                  dstImageLayout,
                                  &color,
                                  clearFormat,
                                  *pSlowClearRange,
                                  boxCount,
                                  pBoxes);
            }
            else
            {
                if (needPreComputeSync)
                {
                    PreComputeColorClearSync(pPm4CmdBuffer,
                                             &dstImage,
                                             pRanges[rangeIdx],
                                             dstImageLayout);

                    needPostComputeSync = true;
                }

                // Raw format clears are ok on the compute engine because these won't affect the state of DCC memory.
                SlowClearCompute(pPm4CmdBuffer,
                                 dstImage,
                                 dstImageLayout,
                                 &color,
                                 clearFormat,
                                 *pSlowClearRange,
                                 boxCount,
                                 pBoxes);
            }
        }

        if (needPostComputeSync)
        {
            PostComputeColorClearSync(pPm4CmdBuffer, &dstImage, pRanges[rangeIdx], dstImageLayout, csFastClear);

            needPostComputeSync = false;
        }
    }
}

// =====================================================================================================================
// Inserts barrier needed before issuing a compute clear when the target image is currently bound as a color target.
// Only necessary when the client specifies the ColorClearAutoSync flag for a color clear.
void RsrcProcMgr::PreComputeColorClearSync(
    ICmdBuffer*        pCmdBuffer,
    const IImage*      pImage,
    const SubresRange& subres,
    ImageLayout        layout
    ) const
{
    if (m_releaseAcquireSupported)
    {
        ImgBarrier imgBarrier = {};

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        imgBarrier.srcStageMask  = PipelineStageColorTarget;
        // Fast clear path may have CP to update metadata state/values, wait at BLT/ME stage for safe.
        imgBarrier.dstStageMask  = PipelineStageBlt;
#endif
        imgBarrier.srcAccessMask = CoherColorTarget;
        imgBarrier.dstAccessMask = CoherShader;
        imgBarrier.subresRange   = subres;
        imgBarrier.pImage        = pImage;
        imgBarrier.oldLayout     = layout;
        imgBarrier.newLayout     = layout;

        AcquireReleaseInfo acqRelInfo = {};

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 767
        acqRelInfo.srcStageMask      = PipelineStageColorTarget;
        // Fast clear path may have CP to update metadata state/values, wait at BLT/ME stage for safe.
        acqRelInfo.dstStageMask      = PipelineStageBlt;
#endif
        acqRelInfo.imageBarrierCount = 1;
        acqRelInfo.pImageBarriers    = &imgBarrier;
        acqRelInfo.reason            = Developer::BarrierReasonPreComputeColorClear;

        pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
    }
    else
    {
        BarrierInfo preBarrier           = { };
        preBarrier.waitPoint             = HwPipePreCs;

        constexpr HwPipePoint Eop        = HwPipeBottom;
        preBarrier.pipePointWaitCount    = 1;
        preBarrier.pPipePoints           = &Eop;

        BarrierTransition transition     = { };
        transition.srcCacheMask          = CoherColorTarget;
        transition.dstCacheMask          = CoherShader;
        transition.imageInfo.pImage      = pImage;
        transition.imageInfo.subresRange = subres;
        transition.imageInfo.oldLayout   = layout;
        transition.imageInfo.newLayout   = layout;

        preBarrier.transitionCount       = 1;
        preBarrier.pTransitions          = &transition;
        preBarrier.reason                = Developer::BarrierReasonPreComputeColorClear;

        pCmdBuffer->CmdBarrier(preBarrier);
    }
}

// =====================================================================================================================
// Inserts barrier needed after issuing a compute clear when the target image will be immediately re-bound as a
// color target.  Only necessary when the client specifies the ColorClearAutoSync flag for a color clear.
void RsrcProcMgr::PostComputeColorClearSync(
    ICmdBuffer*        pCmdBuffer,
    const IImage*      pImage,
    const SubresRange& subres,
    ImageLayout        layout,
    bool               csFastClear
    ) const
{
    if (m_releaseAcquireSupported)
    {
        ImgBarrier imgBarrier = {};

        // Optimization: For post CS fast Clear to ColorTarget transition, no need flush DST caches and invalidate
        //               SRC caches. Both cs fast clear and ColorTarget access metadata in direct mode, so no need
        //               L2 flush/inv even if the metadata is misaligned. See WaRefreshTccOnMetadataMisalignment()
        //               for more details. Safe to pass 0 here, so no cache operation and PWS can wait at PreColor.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        imgBarrier.srcStageMask  = PipelineStageCs;
        imgBarrier.dstStageMask  = PipelineStageColorTarget;
#endif
        imgBarrier.srcAccessMask = csFastClear ? 0 : CoherShader;
        imgBarrier.dstAccessMask = csFastClear ? 0 : CoherColorTarget;
        imgBarrier.subresRange   = subres;
        imgBarrier.pImage        = pImage;
        imgBarrier.oldLayout     = layout;
        imgBarrier.newLayout     = layout;

        AcquireReleaseInfo acqRelInfo = {};

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 767
        acqRelInfo.srcStageMask      = PipelineStageCs;
        acqRelInfo.dstStageMask      = PipelineStageColorTarget;
#endif
        acqRelInfo.imageBarrierCount = 1;
        acqRelInfo.pImageBarriers    = &imgBarrier;
        acqRelInfo.reason            = Developer::BarrierReasonPostComputeColorClear;

        pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
    }
    else
    {
        BarrierInfo postBarrier          = { };
        postBarrier.waitPoint            = HwPipePreColorTarget;
        constexpr HwPipePoint PostCs     = HwPipePostCs;
        postBarrier.pipePointWaitCount   = 1;
        postBarrier.pPipePoints          = &PostCs;

        BarrierTransition transition     = { };
        transition.srcCacheMask          = CoherShader;
        transition.dstCacheMask          = CoherColorTarget;
        transition.imageInfo.pImage      = pImage;
        transition.imageInfo.subresRange = subres;
        transition.imageInfo.oldLayout   = layout;
        transition.imageInfo.newLayout   = layout;

        postBarrier.transitionCount      = 1;
        postBarrier.pTransitions         = &transition;
        postBarrier.reason               = Developer::BarrierReasonPostComputeColorClear;

        pCmdBuffer->CmdBarrier(postBarrier);
    }
}

// =====================================================================================================================
// Inserts barrier needed before issuing a compute clear when the target image is currently bound as a depth/stencil
// target.  Only necessary when the client specifies the DsClearAutoSync flag for a depth/stencil clear.
void RsrcProcMgr::PreComputeDepthStencilClearSync(
    ICmdBuffer*        pCmdBuffer,
    const GfxImage&    gfxImage,
    const SubresRange& subres,
    ImageLayout        layout
    ) const
{
    PAL_ASSERT(subres.numPlanes == 1);

    BarrierInfo preBarrier                 = { };
    preBarrier.waitPoint                   = HwPipePreCs;

    const IImage* pImage                   = gfxImage.Parent();

    // The most efficient way to wait for DB-idle and flush and invalidate the DB caches is an acquire_mem.
    // Acquire release doesn't support ranged stall and cache F/I via acquire_mem.
    preBarrier.rangeCheckedTargetWaitCount = 1;
    preBarrier.ppTargets                   = &pImage;

    BarrierTransition transition           = { };
    transition.srcCacheMask                = CoherDepthStencilTarget;
    transition.dstCacheMask                = CoherShader;
    transition.imageInfo.pImage            = pImage;
    transition.imageInfo.subresRange       = subres;
    transition.imageInfo.oldLayout         = layout;
    transition.imageInfo.newLayout         = layout;

    preBarrier.transitionCount             = 1;
    preBarrier.pTransitions                = &transition;
    preBarrier.reason                      = Developer::BarrierReasonPreComputeDepthStencilClear;

    pCmdBuffer->CmdBarrier(preBarrier);
}

// =====================================================================================================================
// Inserts barrier needed after issuing a compute clear when the target image will be immediately re-bound as a
// depth/stencil target.  Only necessary when the client specifies the DsClearAutoSync flag for a depth/stencil clear.
void RsrcProcMgr::PostComputeDepthStencilClearSync(
    ICmdBuffer*        pCmdBuffer,
    const GfxImage&    gfxImage,
    const SubresRange& subres,
    ImageLayout        layout,
    bool               csFastClear
    ) const
{
    const IImage* pImage = gfxImage.Parent();

    if (m_releaseAcquireSupported)
    {
        ImgBarrier imgBarrier = {};

        // Optimization: For post CS fast Clear to DepthStencilTarget transition, no need flush DST caches and
        //               invalidate SRC caches. Both cs fast clear and DepthStencilTarget access metadata in direct
        //               mode, so no need L2 flush/inv even if the metadata is misaligned. See
        //               WaRefreshTccOnMetadataMisalignment() for more details. Safe to pass 0 here, so no cache
        //               operation and PWS can wait at PreDepth.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        imgBarrier.srcStageMask  = PipelineStageCs;
        imgBarrier.dstStageMask  = PipelineStageEarlyDsTarget | PipelineStageLateDsTarget;
#endif
        imgBarrier.srcAccessMask = csFastClear ? 0 : CoherShader;
        imgBarrier.dstAccessMask = csFastClear ? 0 : CoherDepthStencilTarget;
        imgBarrier.subresRange   = subres;
        imgBarrier.pImage        = pImage;
        imgBarrier.oldLayout     = layout;
        imgBarrier.newLayout     = layout;

        AcquireReleaseInfo acqRelInfo = {};

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 767
        acqRelInfo.srcStageMask      = PipelineStageCs;
        acqRelInfo.dstStageMask      = PipelineStageEarlyDsTarget | PipelineStageLateDsTarget;
#endif
        acqRelInfo.imageBarrierCount = 1;
        acqRelInfo.pImageBarriers    = &imgBarrier;
        acqRelInfo.reason            = Developer::BarrierReasonPostComputeDepthStencilClear;

        pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
    }
    else
    {
        BarrierInfo postBarrier          = { };
        postBarrier.waitPoint            = HwPipePreRasterization;

        constexpr HwPipePoint PostCs     = HwPipePostCs;
        postBarrier.pipePointWaitCount   = 1;
        postBarrier.pPipePoints          = &PostCs;

        BarrierTransition transition     = { };
        transition.srcCacheMask          = CoherShader;
        transition.dstCacheMask          = CoherDepthStencilTarget;
        transition.imageInfo.pImage      = pImage;
        transition.imageInfo.subresRange = subres;
        transition.imageInfo.oldLayout   = layout;
        transition.imageInfo.newLayout   = layout;

        postBarrier.transitionCount      = 1;
        postBarrier.pTransitions         = &transition;
        postBarrier.reason               = Developer::BarrierReasonPostComputeDepthStencilClear;

        pCmdBuffer->CmdBarrier(postBarrier);
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
    const Pm4Image& pm4Image   = static_cast<Pm4Image&>(*dstImage.GetGfxImage());
    const bool      hasRects   = (rectCount > 0);
    const auto&     createInfo = dstImage.GetImageCreateInfo();

    PAL_ASSERT((hasRects == false) || (pRects != nullptr));

    // Clear groups of ranges on "this group is fast clearable = true/false" boundaries
    uint32 rangesCleared = 0;

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
        for (uint32 i = 0; i < rectCount; i++)
        {
            boxes[i].offset.x      = pRects[i].offset.x;
            boxes[i].offset.y      = pRects[i].offset.y;
            boxes[i].offset.z      = 0;
            boxes[i].extent.width  = pRects[i].extent.width;
            boxes[i].extent.height = pRects[i].extent.height;
            boxes[i].extent.depth  = 1;
        }

        const bool clearRectCoversWholeImage = ((hasRects                  == false)                  ||
                                                ((rectCount                == 1)                      &&
                                                 (pRects[0].offset.x       == 0)                      &&
                                                 (pRects[0].offset.y       == 0)                      &&
                                                 (createInfo.extent.width  == pRects[0].extent.width) &&
                                                 (createInfo.extent.height == pRects[0].extent.height)));

        while (rangesCleared < rangeCount)
        {
            const uint32 groupBegin = rangesCleared;

            // Note that fast clears don't support sub-rect clears so we skip them if we have any boxes. Further,
            // we only can store one fast clear color per mip level, and therefore can only support fast clears
            // when a range covers all slices.
            const bool groupFastClearable = (clearRectCoversWholeImage &&
                                             pm4Image.IsFastDepthStencilClearSupported(
                                                 depthLayout,
                                                 stencilLayout,
                                                 depth,
                                                 stencil,
                                                 stencilWriteMask,
                                                 pRanges[groupBegin]));

            // Find as many other ranges that also support/don't support fast clearing so that they can be grouped
            // together into a single clear operation.
            uint32 groupEnd = groupBegin + 1;

            while ((groupEnd < rangeCount)     &&
                   ((clearRectCoversWholeImage &&
                     pm4Image.IsFastDepthStencilClearSupported(depthLayout,
                                                               stencilLayout,
                                                               depth,
                                                               stencil,
                                                               stencilWriteMask,
                                                               pRanges[groupEnd]))
                    == groupFastClearable))
            {
                ++groupEnd;
            }

            // Either fast clear or slow clear this group of ranges.
            rangesCleared = groupEnd;
            const uint32 clearRangeCount = groupEnd - groupBegin; // NOTE: end equals one past the last range in group.

            HwlDepthStencilClear(pCmdBuffer,
                                 pm4Image,
                                 depthLayout,
                                 stencilLayout,
                                 depth,
                                 stencil,
                                 stencilWriteMask,
                                 clearRangeCount,
                                 &pRanges[groupBegin],
                                 groupFastClearable,
                                 TestAnyFlagSet(flags, DsClearAutoSync),
                                 rectCount,
                                 &boxes[0]);
        }
    }
}

// =====================================================================================================================
// Executes a compute shader which generates a PM4 command buffer which can later be executed. If the number of indirect
// commands being generated will not fit into a single command-stream chunk, this will issue multiple dispatches, one
// for each command chunk to generate.
void RsrcProcMgr::CmdGenerateIndirectCmds(
    const GenerateInfo& genInfo,
    CmdStreamChunk**    ppChunkLists[],
    uint32              NumChunkLists,
    uint32*             pNumGenChunks
    ) const
{
    const auto*                 pPublicSettings = m_pDevice->Parent()->GetPublicSettings();
    const auto&                 chipProps       = m_pDevice->Parent()->ChipProperties();
    const gpusize               argsGpuAddr     = genInfo.argsGpuAddr;
    const gpusize               countGpuAddr    = genInfo.countGpuAddr;
    const Pipeline*             pPipeline       = genInfo.pPipeline;
    const Pm4::IndirectCmdGenerator& generator  = genInfo.generator;
    Pm4CmdBuffer*               pCmdBuffer      = static_cast<Pm4CmdBuffer*>(genInfo.pCmdBuffer);
    uint32                      indexBufSize    = genInfo.indexBufSize;
    uint32                      maximumCount    = genInfo.maximumCount;

    const ComputePipeline* pGenerationPipeline = GetCmdGenerationPipeline(generator, *pCmdBuffer);
    const DispatchDims     threadsPerGroup     = pGenerationPipeline->ThreadsPerGroupXyz();

    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pGenerationPipeline, InternalApiPsoHash, });

    // The command-generation pipelines expect the following descriptor-table layout for the resources which are the
    // same for each command-stream chunk being generated:
    //  + Raw-buffer SRD for the indirect argument data (4 DW)
    //  + Structured-buffer SRD for the command parameter data (4 DW)
    //  + Typed buffer SRD for the user-data entry mapping table for each shader stage (4 DW)
    //  + Structured-buffer SRD for the pipeline signature (4 DW)
    //  + Structured-buffer SRD for the second pipeline signature (4 DW)
    //  + Raw-buffer SRD pointing to return-to-caller INDIRECT_BUFFER packet location for the main chunk. (4 DW)
    //  + Raw-buffer SRD pointing to return-to-caller INDIRECT_BUFFER packet location for the task chunk. (4 DW)
    //  + Constant buffer SRD for the command-generator properties (4 DW)
    //  + Constant buffer SRD for the properties of the ExecuteIndirect() invocation (4 DW)
    //  + GPU address of the memory containing the count of commands to generate (2 DW)
    //  + Issue THREAD_TRACE_MARKER after draw or dispatch (1 DW)
    //  + Task Shader Enabled flag (1 DW)

    constexpr uint32 SrdDwords = 4;
    PAL_ASSERT(chipProps.srdSizes.bufferView == (sizeof(uint32) * SrdDwords));

    const bool taskShaderEnabled = ((generator.Type() == Pm4::GeneratorType::DispatchMesh) &&
                                    (static_cast<const GraphicsPipeline*>(pPipeline)->HasTaskShader()));

    // The generation pipelines expect the descriptor table's GPU address to be written to user-data #0-1.
    gpusize tableGpuAddr = 0uLL;

    uint32* pTableMem = pCmdBuffer->CmdAllocateEmbeddedData(((9 * SrdDwords) + 4), 1, &tableGpuAddr);

    PAL_ASSERT(pTableMem != nullptr);

    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 0, 2, reinterpret_cast<uint32*>(&tableGpuAddr));

    // Raw-buffer SRD for the indirect-argument data:
    BufferViewInfo viewInfo = { };
    viewInfo.gpuAddr        = argsGpuAddr;
    viewInfo.swizzledFormat = UndefinedSwizzledFormat;
    viewInfo.range          = (generator.Properties().argBufStride * maximumCount);
    viewInfo.stride         = 1;
    viewInfo.flags.bypassMallRead  = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnRead);
    viewInfo.flags.bypassMallWrite = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnWrite);
    m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pTableMem);
    pTableMem += SrdDwords;

    // Structured-buffer SRD for the command parameter data:
    generator.PopulateParameterBuffer(pCmdBuffer, pPipeline, pTableMem);
    pTableMem += SrdDwords;

    // Typed-buffer SRD for the user-data entry mappings:
    generator.PopulateUserDataMappingBuffer(pCmdBuffer, pPipeline, pTableMem);
    pTableMem += SrdDwords;

    // Structured buffer SRD for the pipeline signature:
    generator.PopulateSignatureBuffer(pCmdBuffer, pPipeline, pTableMem);
    if (generator.Type() == Pm4::GeneratorType::DispatchMesh)
    {
        // In the case of DispatchMesh, PopulateSignatureBuffer will allocate an additional SRD hence the increment.
        pTableMem += SrdDwords;
    }
    pTableMem += SrdDwords;

    // Raw-buffer SRD pointing to return-to-caller INDIRECT_BUFFER packet location for the main chunk.
    uint32* pReturnIbAddrTableMem = pTableMem;
    memset(pTableMem, 0, (sizeof(uint32) * SrdDwords));
    pTableMem += SrdDwords;

    // Raw-buffer SRD pointing to return-to-caller INDIRECT_BUFFER packet location for the task chunk.
    uint32* pReturnTaskIbAddrTableMem = pTableMem;
    if (generator.Type() == Pm4::GeneratorType::DispatchMesh)
    {
        memset(pTableMem, 0, (sizeof(uint32) * SrdDwords));
        pTableMem += SrdDwords;
    }

    // Constant buffer SRD for the command-generator properties:
    generator.PopulatePropertyBuffer(pCmdBuffer, pPipeline, pTableMem);
    pTableMem += SrdDwords;

    // Constant buffer SRD for the properties of the ExecuteIndirect() invocation:
    generator.PopulateInvocationBuffer(pCmdBuffer,
                                       pPipeline,
                                       taskShaderEnabled,
                                       argsGpuAddr,
                                       maximumCount,
                                       indexBufSize,
                                       pTableMem);
    pTableMem += SrdDwords;

    // GPU address of the memory containing the actual command count to generate:
    memcpy(pTableMem, &countGpuAddr, sizeof(countGpuAddr));
    pTableMem += 2;

    // Flag to decide whether to issue THREAD_TRACE_MARKER following generated draw/dispatch commands.
    pTableMem[0] = m_pDevice->Parent()->IssueSqttMarkerEvents();
    pTableMem[1] = taskShaderEnabled;

    // These will be used for tracking the postamble size of the main and task chunks respectively.
    uint32 postambleDwords    = 0;
    uint32 postambleDwordsAce = 0;

    uint32 commandIdOffset = 0;
    while (commandIdOffset < maximumCount)
    {
        // Obtain a command-stream chunk for generating commands into. This also sets-up the padding requirements
        // for the chunk and determines the number of commands which will safely fit. We'll need to build a raw-
        // buffer SRD so the shader can access the command buffer as a UAV.
        ChunkOutput output[2]  = {};
        const uint32 numChunks = (taskShaderEnabled) ? 2 : 1;
        pCmdBuffer->GetChunkForCmdGeneration(generator,
                                             *pPipeline,
                                             (maximumCount - commandIdOffset),
                                             numChunks,
                                             output);

        ChunkOutput& mainChunk          = output[0];
        ppChunkLists[0][*pNumGenChunks] = mainChunk.pChunk;

        postambleDwords = mainChunk.chainSizeInDwords;

        // The command generation pipeline also expects the following descriptor-table layout for the resources
        // which change between each command-stream chunk being generated:
        //  + Raw buffer UAV SRD for the command-stream chunk to generate (4 DW)
        //  + Raw buffer UAV SRD for the embedded data segment to use for the spill table (4 DW)
        //  + Raw buffer UAV SRD pointing to current chunk's INDIRECT_BUFFER packet that chains to the next chunk (4 DW)
        //  + Command ID offset for the current command-stream-chunk (1 DW)
        //  + Low half of the GPU virtual address of the spill table's embedded data segment (1 DW)

        // The generation pipelines expect the descriptor table's GPU address to be written to user-data #2-3.
        pTableMem = pCmdBuffer->CmdAllocateEmbeddedData(((3 * SrdDwords) + 2), 1, &tableGpuAddr);
        PAL_ASSERT(pTableMem != nullptr);

        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 2, 2, reinterpret_cast<uint32*>(&tableGpuAddr));

        // UAV buffer SRD for the command-stream-chunk to generate:
        viewInfo.gpuAddr        = mainChunk.pChunk->GpuVirtAddr();
        viewInfo.swizzledFormat = UndefinedSwizzledFormat;
        viewInfo.range          = (mainChunk.commandsInChunk * generator.CmdBufStride(pPipeline));
        viewInfo.stride         = 1;
        m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pTableMem);
        pTableMem += SrdDwords;

        // UAV buffer SRD for the embedded-data spill table:
        if (mainChunk.embeddedDataSize != 0)
        {
            viewInfo.gpuAddr        = mainChunk.embeddedDataAddr;
            viewInfo.swizzledFormat = UndefinedSwizzledFormat;
            viewInfo.range          = (sizeof(uint32) * mainChunk.embeddedDataSize);
            viewInfo.stride         = 1;
            m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pTableMem);
        }
        else
        {
            // If we're not using the embedded-data spill table, we still need to clear the srd to 0.
            // This prevents hangs on older hardware caused by the shader attempting to read an invalid srd.
            memset(pTableMem, 0, (sizeof(uint32) * SrdDwords));
        }

        pTableMem += SrdDwords;

        // UAV buffer SRD pointing to current chunk's INDIRECT_BUFFER packet that chains to the next chunk.
        const gpusize chainIbAddress = mainChunk.pChunk->GpuVirtAddr() +
                                       ((mainChunk.pChunk->CmdDwordsToExecute() - postambleDwords) * sizeof(uint32));

        viewInfo.gpuAddr        = chainIbAddress;
        viewInfo.swizzledFormat = UndefinedSwizzledFormat;
        viewInfo.range          = postambleDwords * sizeof(uint32);
        viewInfo.stride         = 1;
        // Value stored for this chunk's "commandBufChainIb" in the shader.
        m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pTableMem);
        pTableMem += SrdDwords;

        // Command ID offset for the current command stream-chunk
        pTableMem[0] = commandIdOffset;
        // Low portion of the spill table's GPU virtual address
        pTableMem[1] = LowPart(mainChunk.embeddedDataAddr);

        // The command generation pipeline also expects the following descriptor-table layout for the resources
        // which change between each command-stream chunk being generated:
        // + Raw buffer UAV SRD for the command-stream chunk to generate (4 DW)
        // + Raw buffer UAV SRD for the embedded data segment to use for the spill table (4 DW)
        // + Raw buffer UAV SRD pointing to current task chunk's INDIRECT_BUFFER packet that chains to the next chunk
        // + (4 DW)
        if (taskShaderEnabled)
        {
            ChunkOutput& taskChunk          = output[1];
            ppChunkLists[1][*pNumGenChunks] = taskChunk.pChunk;

            postambleDwordsAce = taskChunk.chainSizeInDwords;
            // This assert validates that the following dispatch contains equivalent commands for both the DE and ACE
            // engines for this DispatchMesh pipeline.
            PAL_ASSERT(taskChunk.commandsInChunk == mainChunk.commandsInChunk);

            pTableMem = pCmdBuffer->CmdAllocateEmbeddedData((3 * SrdDwords), 1, &tableGpuAddr);
            PAL_ASSERT(pTableMem != nullptr);

            // UAV buffer SRD for the command-stream-chunk to generate:
            viewInfo.gpuAddr        = taskChunk.pChunk->GpuVirtAddr();
            viewInfo.swizzledFormat = UndefinedSwizzledFormat;
            viewInfo.range          = (taskChunk.commandsInChunk * generator.CmdBufStride(pPipeline));
            viewInfo.stride         = 1;
            m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pTableMem);
            pTableMem += SrdDwords;

            // UAV buffer SRD for the embedded-data spill table:
            viewInfo.gpuAddr        = taskChunk.embeddedDataAddr;
            viewInfo.swizzledFormat = UndefinedSwizzledFormat;
            viewInfo.range          = (sizeof(uint32) * taskChunk.embeddedDataSize);
            viewInfo.stride         = 1;
            m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pTableMem);
            pTableMem += SrdDwords;

            // UAV buffer SRD pointing to current task chunk's INDIRECT_BUFFER packet that chains to the next task
            // chunk:
            const gpusize taskChainIbAddress = taskChunk.pChunk->GpuVirtAddr() +
                                               ((taskChunk.pChunk->CmdDwordsToExecute() - postambleDwordsAce) *
                                                sizeof(uint32));

            viewInfo.gpuAddr        = taskChainIbAddress;
            viewInfo.swizzledFormat = UndefinedSwizzledFormat;
            viewInfo.range          = postambleDwordsAce * sizeof(uint32);
            viewInfo.stride         = 1;
            // Value stored for this chunk's "taskCommandBufChainIb" in the shader.
            m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pTableMem);
        }

        // We use the ACE for IndirectCmdGeneration only for this very special case. It has to be a UniversalCmdBuffer,
        // ganged ACE is supported, and we are not using the ACE for Task Shader work.
        bool cmdGenUseAce = pCmdBuffer->IsGraphicsSupported() &&
                            (chipProps.gfxip.supportAceOffload != 0) &&
                            (pPublicSettings->disableExecuteIndirectAceOffload != true) &&
                            (taskShaderEnabled == false);

        if (cmdGenUseAce)
        {
            pCmdBuffer->CmdDispatchAce({RpmUtil::MinThreadGroups(generator.ParameterCount(), threadsPerGroup.x),
                                        RpmUtil::MinThreadGroups(mainChunk.commandsInChunk,  threadsPerGroup.y), 1});
        }
        else
        {
            pCmdBuffer->CmdDispatch({RpmUtil::MinThreadGroups(generator.ParameterCount(), threadsPerGroup.x),
                                     RpmUtil::MinThreadGroups(mainChunk.commandsInChunk,  threadsPerGroup.y), 1});
        }

        (*pNumGenChunks)++;
        commandIdOffset += mainChunk.commandsInChunk;
    }

    // This will calculate the IB's return addresses that will be helpful for the CP jump/ short-circuit over possibly
    // executing long chains of NOPs.
    if (*pNumGenChunks > 0)
    {
        const CmdStreamChunk* pLastChunk = ppChunkLists[0][(*pNumGenChunks) - 1];
        const gpusize pReturnChainIbAddress = pLastChunk->GpuVirtAddr() +
                                              ((pLastChunk->CmdDwordsToExecute() - postambleDwords) * sizeof(uint32));
        viewInfo.gpuAddr               = pReturnChainIbAddress;
        viewInfo.swizzledFormat        = UndefinedSwizzledFormat;
        viewInfo.range                 = postambleDwords * sizeof(uint32);
        viewInfo.stride                = 1;
        // Value stored in "cmdBufReturningChainIb" in the shader.
        m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pReturnIbAddrTableMem);

        if (taskShaderEnabled)
        {
            const CmdStreamChunk* pLastTaskChunk = ppChunkLists[1][(*pNumGenChunks) - 1];
            const gpusize pReturnTaskChainIbAddress = pLastTaskChunk->GpuVirtAddr() +
                                                      ((pLastTaskChunk->CmdDwordsToExecute() - postambleDwordsAce) *
                                                       sizeof(uint32));
            viewInfo.gpuAddr               = pReturnTaskChainIbAddress;
            viewInfo.swizzledFormat        = UndefinedSwizzledFormat;
            viewInfo.range                 = postambleDwordsAce * sizeof(uint32);
            viewInfo.stride                = 1;
            // Value stored in "taskCmdBufReturningChainIb" in the shader.
            m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pReturnTaskIbAddrTableMem);
        }
    }

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Adds commands to pCmdBuffer to copy data between srcGpuMemory and dstGpuMemory. Note that this function requires a
// command buffer that supports CP DMA workloads.
void RsrcProcMgr::CmdCopyMemory(
    Pm4CmdBuffer*           pCmdBuffer,
    const GpuMemory&        srcGpuMemory,
    const GpuMemory&        dstGpuMemory,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions
    ) const
{
    // Force the compute shader copy path if any regions couldn't be executed by the CPDMA copy path:
    //
    //     - Size exceeds the maximum supported by CPDMA.
    //     - Source or destination are virtual resources (CP would halt).
    bool useCsCopy = srcGpuMemory.IsVirtual() || dstGpuMemory.IsVirtual();

    for (uint32 i = 0; !useCsCopy && (i < regionCount); i++)
    {
        if (pRegions[i].copySize > m_pDevice->Parent()->GetPublicSettings()->cpDmaCmdCopyMemoryMaxBytes)
        {
            // We will copy this region later on.
            useCsCopy = true;
        }
    }

    if (useCsCopy)
    {
        CopyMemoryCs(pCmdBuffer, srcGpuMemory, dstGpuMemory, regionCount, pRegions);
    }
    else
    {
        for (uint32 i = 0; i < regionCount; i++)
        {
            const gpusize dstAddr = dstGpuMemory.Desc().gpuVirtAddr + pRegions[i].dstOffset;
            const gpusize srcAddr = srcGpuMemory.Desc().gpuVirtAddr + pRegions[i].srcOffset;

            pCmdBuffer->CpCopyMemory(dstAddr, srcAddr, pRegions[i].copySize);
        }
    }
}

// =====================================================================================================================
bool RsrcProcMgr::SlowClearUseGraphics(
    Pm4CmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& clearRange,
    ClearMethod        method
    ) const
{
    const SwizzledFormat& baseFormat = dstImage.SubresourceInfo(clearRange.startSubres)->format;
    uint32                texelScale = 1;
    RpmUtil::GetRawFormat(baseFormat.format, &texelScale, nullptr);

    return (pCmdBuffer->IsGraphicsSupported() &&
            // Force clears of scaled formats to the compute engine
            (texelScale == 1)                 &&
            (method == ClearMethod::NormalGraphics));
}

// =====================================================================================================================
// Builds commands to slow clear a range of an image to the given raw color data using a pixel shader. Note that this
// function can only clear color planes.
void RsrcProcMgr::SlowClearGraphics(
    Pm4CmdBuffer*         pCmdBuffer,
    const Image&          dstImage,
    ImageLayout           dstImageLayout,
    const ClearColor*     pColor,
    const SwizzledFormat& clearFormat,
    const SubresRange&    clearRange,
    uint32                boxCount,
    const Box*            pBoxes
    ) const
{
    // Graphics slow clears only work on color planes.
    PAL_ASSERT(dstImage.IsDepthStencilTarget() == false);

    const auto& createInfo      = dstImage.GetImageCreateInfo();
    const auto* pPublicSettings = m_pDevice->Parent()->GetPublicSettings();

    for (SubresId subresId = clearRange.startSubres;
         subresId.plane < (clearRange.startSubres.plane + clearRange.numPlanes);
         subresId.plane++)
    {
        // Get some useful information about the image.
        bool rawFmtOk = dstImage.GetGfxImage()->IsFormatReplaceable(subresId,
                                                                    dstImageLayout,
                                                                    true,
                                                                    pColor->disabledChannelMask);

        // Query the format of the image and determine which format to use for the color target view. If rawFmtOk is
        // set the caller has allowed us to use a slightly more efficient raw format.
        const SwizzledFormat baseFormat   = clearFormat.format == ChNumFormat::Undefined ?
                                            dstImage.SubresourceInfo(subresId)->format :
                                            clearFormat;
        SwizzledFormat       viewFormat   = (rawFmtOk ? RpmUtil::GetRawFormat(baseFormat.format, nullptr, nullptr)
                                                      : baseFormat);
        uint32               xRightShift  = 0;
        uint32               vpRightShift = 0;
        // For packed YUV image use X32_Uint instead of X16_Uint to fill with YUYV.
        if ((viewFormat.format == ChNumFormat::X16_Uint) && Formats::IsYuvPacked(baseFormat.format))
        {
            viewFormat.format  = ChNumFormat::X32_Uint;
            viewFormat.swizzle = { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };
            rawFmtOk           = false;
            // If clear color type isn't Yuv then the client is responsible for offset/extent adjustments.
            xRightShift        = (pColor->type == ClearColorType::Yuv) ? 1 : 0;
            // The viewport should always be adjusted regardless the clear color type, (however, since this is just clear,
            // all pixels are the same and the scissor rect will clamp the rendering area, the result is still correct
            // without this adjustment).
            vpRightShift       = 1;
        }

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

        const bool  is3dImage  = (createInfo.imageType == ImageType::Tex3d);
        ColorTargetViewCreateInfo colorViewInfo         = { };
        colorViewInfo.swizzledFormat                    = viewFormat;
        colorViewInfo.imageInfo.pImage                  = &dstImage;
        colorViewInfo.imageInfo.arraySize               = (is3dImage ? 1 : clearRange.numSlices);
        colorViewInfo.imageInfo.baseSubRes.plane        = subresId.plane;
        colorViewInfo.imageInfo.baseSubRes.arraySlice   = subresId.arraySlice;
        colorViewInfo.flags.bypassMall = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                        RpmViewsBypassMallOnCbDbWrite);

        BindTargetParams bindTargetsInfo = { };
        bindTargetsInfo.colorTargets[0].imageLayout      = dstImageLayout;
        bindTargetsInfo.colorTargets[0].pColorTargetView = nullptr;

        PipelineBindParams bindPipelineInfo = { };
        bindPipelineInfo.pipelineBindPoint = PipelineBindPoint::Graphics;
        bindPipelineInfo.pPipeline = GetGfxPipelineByTargetIndexAndFormat(SlowColorClear0_32ABGR, 0, viewFormat);
        bindPipelineInfo.apiPsoHash = InternalApiPsoHash;

        if (pColor->disabledChannelMask != 0)
        {
            // Overwrite CbTargetMask for different writeMasks.
            bindPipelineInfo.graphics.dynamicState.enable.colorWriteMask = 1;
            bindPipelineInfo.graphics.dynamicState.colorWriteMask = ~pColor->disabledChannelMask;
        }

        // Save current command buffer state and bind graphics state which is common for all mipmap levels.
        pCmdBuffer->CmdSaveGraphicsState();
        pCmdBuffer->CmdBindPipeline(bindPipelineInfo);
        BindCommonGraphicsState(pCmdBuffer);

        pCmdBuffer->CmdOverwriteRbPlusFormatForBlits(viewFormat, 0);
        pCmdBuffer->CmdBindColorBlendState(m_pBlendDisableState);
        pCmdBuffer->CmdBindDepthStencilState(m_pDepthDisableState);
        pCmdBuffer->CmdBindMsaaState(GetMsaaState(createInfo.samples, createInfo.fragments));

        RpmUtil::WriteVsZOut(pCmdBuffer, 1.0f);
        RpmUtil::WriteVsFirstSliceOffset(pCmdBuffer, 0);

        uint32 packedColor[4] = {0};

        if (pColor->type == ClearColorType::Yuv)
        {
            // If clear color type is Yuv, the image format should used to determine the clear color swizzling and packing
            // for planar YUV formats since the baseFormat is subresource's format which is not a YUV format.
            // NOTE: if clear color type is Uint, the client is responsible for:
            //       1. packing and swizzling clear color for packed YUV formats (e.g. packing in YUYV order for YUY2).
            //       2. passing correct clear color for this plane for planar YUV formats (e.g. two uint32s for U and V if
            //          current plane is CbCr).
            const SwizzledFormat imgFormat = createInfo.swizzledFormat;
            Formats::ConvertYuvColor(imgFormat, subresId.plane, &pColor->u32Color[0], &packedColor[0]);
        }
        else
        {
            uint32 convertedColor[4] = {0};

            if (pColor->type == ClearColorType::Float)
            {
                Formats::ConvertColor(baseFormat, &pColor->f32Color[0], &convertedColor[0]);
            }
            else
            {
                memcpy(&convertedColor[0], &pColor->u32Color[0], sizeof(convertedColor));
            }

            RpmUtil::ConvertClearColorToNativeFormat(baseFormat, viewFormat, &convertedColor[0]);

            // If we can clear with raw format replacement which is more efficient, swizzle it into the order
            // required and then pack it.
            if (rawFmtOk)
            {
                uint32 swizzledColor[4] = {0};
                Formats::SwizzleColor(baseFormat, &convertedColor[0], &swizzledColor[0]);
                Formats::PackRawClearColor(baseFormat, &swizzledColor[0], &packedColor[0]);
            }
            else
            {
                memcpy(&packedColor[0], &convertedColor[0], sizeof(packedColor));
            }
        }

        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, RpmPsClearFirstUserData, 4, &packedColor[0]);

        // Each mipmap needs to be cleared individually.
        const uint32 lastMip = (subresId.mipLevel + clearRange.numMips - 1);

        // Boxes are only meaningful if we're clearing a single mip.
        PAL_ASSERT((boxCount == 0) || ((pBoxes != nullptr) && (clearRange.numMips == 1)));

        for (uint32 mip = subresId.mipLevel; mip <= lastMip; ++mip)
        {
            const SubresId mipSubres  = { subresId.plane, mip, 0 };
            const auto&    subResInfo = *dstImage.SubresourceInfo(mipSubres);

            // All slices of the same mipmap level can re-use the same viewport state.
            viewportInfo.viewports[0].width  = static_cast<float>(subResInfo.extentTexels.width >> vpRightShift);
            viewportInfo.viewports[0].height = static_cast<float>(subResInfo.extentTexels.height);

            pCmdBuffer->CmdSetViewports(viewportInfo);

            colorViewInfo.imageInfo.baseSubRes.mipLevel = mip;
            SlowClearGraphicsOneMip(static_cast<Pm4CmdBuffer*>(pCmdBuffer),
                                    dstImage,
                                    mipSubres,
                                    boxCount,
                                    pBoxes,
                                    &colorViewInfo,
                                    &bindTargetsInfo,
                                    xRightShift);
        }

        // Restore original command buffer state.
        pCmdBuffer->CmdRestoreGraphicsState();
    }
}

// =====================================================================================================================
// Builds commands to slow clear a range of an image for a given mip level.
void RsrcProcMgr::SlowClearGraphicsOneMip(
    Pm4CmdBuffer*              pCmdBuffer,
    const Image&               dstImage,
    const SubresId&            mipSubres,
    uint32                     boxCount,
    const Box*                 pBoxes,
    ColorTargetViewCreateInfo* pColorViewInfo,
    BindTargetParams*          pBindTargetsInfo,
    uint32                     xRightShift
    ) const
{
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<Pm4::UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const auto& createInfo = dstImage.GetImageCreateInfo();
    const bool  is3dImage  = (createInfo.imageType == ImageType::Tex3d);
    ColorTargetViewInternalCreateInfo colorViewInfoInternal = {};

    const auto&    subResInfo = *dstImage.SubresourceInfo(mipSubres);

    // If rects were specified, then we'll create scissors to match the rects and do a Draw for each one. Otherwise
    // we'll use the full image scissor and a single draw.
    const bool   hasBoxes     = (boxCount > 0);
    const uint32 scissorCount = hasBoxes ? boxCount : 1;

    if (is3dImage == false)
    {
        LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

        // Create and bind a color-target view for this mipmap level and slice.
        IColorTargetView* pColorView = nullptr;
        void* pColorViewMem =
            PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

        if (pColorViewMem == nullptr)
        {
            pCmdBuffer->NotifyAllocFailure();
        }
        else
        {
            Result result = m_pDevice->CreateColorTargetView(*pColorViewInfo,
                                                             colorViewInfoInternal,
                                                             pColorViewMem,
                                                             &pColorView);
            PAL_ASSERT(result == Result::Success);

            pBindTargetsInfo->colorTargets[0].pColorTargetView = pColorView;
            pBindTargetsInfo->colorTargetCount = 1;
            pCmdBuffer->CmdBindTargets(*pBindTargetsInfo);

            for (uint32 i = 0; i < scissorCount; i++)
            {
                ClearImageOneBox(pCmdBuffer, subResInfo, &pBoxes[i], hasBoxes, xRightShift,
                    pColorViewInfo->imageInfo.arraySize);
            }

            // Unbind the color-target view and destroy it.
            pBindTargetsInfo->colorTargetCount = 0;
            pCmdBuffer->CmdBindTargets(*pBindTargetsInfo);

            PAL_SAFE_FREE(pColorViewMem, &sliceAlloc);
        }
    }
    else
    {
        // For 3d image, the start and end slice is based on the z offset and depth extend of the boxes.
        // The slices must be specified using the zRange because the imageInfo "slice" refers to image subresources.
        pColorViewInfo->flags.zRangeValid = 1;

        for (uint32 i = 0; i < scissorCount; i++)
        {
            LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

            // Create and bind a color-target view for this mipmap level and z offset.
            IColorTargetView* pColorView = nullptr;
            void* pColorViewMem =
                PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

            if (pColorViewMem == nullptr)
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {

                const Box*   pBox     = hasBoxes ? &pBoxes[i]     : nullptr;
                const uint32 maxDepth = subResInfo.extentTexels.depth;

                pColorViewInfo->zRange.extent  = hasBoxes ? pBox->extent.depth : maxDepth;
                pColorViewInfo->zRange.offset  = hasBoxes ? pBox->offset.z : 0;

                PAL_ASSERT((hasBoxes == false) || (pBox->extent.depth <= maxDepth));

                Result result = m_pDevice->CreateColorTargetView(*pColorViewInfo,
                                                                 colorViewInfoInternal,
                                                                 pColorViewMem,
                                                                 &pColorView);
                PAL_ASSERT(result == Result::Success);

                pBindTargetsInfo->colorTargets[0].pColorTargetView = pColorView;
                pBindTargetsInfo->colorTargetCount = 1;
                pCmdBuffer->CmdBindTargets(*pBindTargetsInfo);

                ClearImageOneBox(pCmdBuffer, subResInfo, pBox, hasBoxes, xRightShift, pColorViewInfo->zRange.extent);

                // Unbind the color-target view and destroy it.
                pBindTargetsInfo->colorTargetCount = 0;
                pCmdBuffer->CmdBindTargets(*pBindTargetsInfo);

                PAL_SAFE_FREE(pColorViewMem, &sliceAlloc);
            }
        }
    }
}

// =====================================================================================================================
// Builds commands to clear a range of an image for a given box.
void RsrcProcMgr::ClearImageOneBox(
    Pm4CmdBuffer*          pCmdBuffer,
    const SubResourceInfo& subResInfo,
    const Box*             pBox,
    bool                   hasBoxes,
    uint32                 xRightShift,
    uint32                 numInstances
    ) const
{
    // Create a scissor state for this mipmap level, slice, and current scissor.
    ScissorRectParams scissorInfo = {};
    scissorInfo.count = 1;

    if (hasBoxes)
    {
        scissorInfo.scissors[0].offset.x      = pBox->offset.x >> xRightShift;
        scissorInfo.scissors[0].offset.y      = pBox->offset.y;
        scissorInfo.scissors[0].extent.width  = pBox->extent.width >> xRightShift;
        scissorInfo.scissors[0].extent.height = pBox->extent.height;
    }
    else
    {
        scissorInfo.scissors[0].extent.width  = subResInfo.extentTexels.width >> xRightShift;
        scissorInfo.scissors[0].extent.height = subResInfo.extentTexels.height;
    }

    pCmdBuffer->CmdSetScissorRects(scissorInfo);

    // Draw a fullscreen quad.
    pCmdBuffer->CmdDraw(0, 3, 0, numInstances, 0);
}

// =====================================================================================================================
// Resolves a multisampled source Image into the single-sampled destination Image using the Image's resolve method.
void RsrcProcMgr::CmdResolveImage(
    GfxCmdBuffer*             pCmdBuffer,
    const Image&              srcImage,
    ImageLayout               srcImageLayout,
    const Image&              dstImage,
    ImageLayout               dstImageLayout,
    ResolveMode               resolveMode,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    uint32                    flags
    ) const
{
    Pm4CmdBuffer* pPm4CmdBuffer = static_cast<Pm4CmdBuffer*>(pCmdBuffer);

    const ResolveMethod srcMethod = srcImage.GetImageInfo().resolveMethod;
    const ResolveMethod dstMethod = dstImage.GetImageInfo().resolveMethod;

    if (pPm4CmdBuffer->GetEngineType() == EngineTypeCompute)
    {
        PAL_ASSERT((srcMethod.shaderCsFmask == 1) || (srcMethod.shaderCs == 1));
        ResolveImageCompute(pPm4CmdBuffer,
                            srcImage,
                            srcImageLayout,
                            dstImage,
                            dstImageLayout,
                            resolveMode,
                            regionCount,
                            pRegions,
                            srcMethod,
                            flags);

        HwlFixupResolveDstImage(pPm4CmdBuffer,
                                *dstImage.GetGfxImage(),
                                dstImageLayout,
                                pRegions,
                                regionCount,
                                true);
    }
    else
    {
        if ((srcMethod.fixedFunc == 1) && HwlCanDoFixedFuncResolve(srcImage,
                                                                   dstImage,
                                                                   resolveMode,
                                                                   regionCount,
                                                                   pRegions))
        {
            PAL_ASSERT(resolveMode == ResolveMode::Average);
            // this only support color resolves.
            ResolveImageFixedFunc(pPm4CmdBuffer,
                                  srcImage,
                                  srcImageLayout,
                                  dstImage,
                                  dstImageLayout,
                                  regionCount,
                                  pRegions,
                                  flags);

            HwlFixupResolveDstImage(pPm4CmdBuffer,
                                    *dstImage.GetGfxImage(),
                                    dstImageLayout,
                                    pRegions,
                                    regionCount,
                                    false);
        }
        else if ((srcMethod.depthStencilCopy == 1) && (dstMethod.depthStencilCopy == 1) &&
                 (resolveMode == ResolveMode::Average) &&
                 (TestAnyFlagSet(flags, ImageResolveInvertY) == false) &&
                  HwlCanDoDepthStencilCopyResolve(srcImage, dstImage, regionCount, pRegions))
        {
            ResolveImageDepthStencilCopy(pPm4CmdBuffer,
                                         srcImage,
                                         srcImageLayout,
                                         dstImage,
                                         dstImageLayout,
                                         regionCount,
                                         pRegions,
                                         flags);

            HwlHtileCopyAndFixUp(pPm4CmdBuffer, srcImage, dstImage, dstImageLayout, regionCount, pRegions, false);
        }
        else if (dstMethod.shaderPs && (resolveMode == ResolveMode::Average))
        {
            if (dstImage.IsDepthStencilTarget())
            {
                // this only supports Depth/Stencil resolves.
                ResolveImageDepthStencilGraphics(pPm4CmdBuffer,
                                                 srcImage,
                                                 srcImageLayout,
                                                 dstImage,
                                                 dstImageLayout,
                                                 regionCount,
                                                 pRegions,
                                                 flags);
            }
#if PAL_BUILD_GFX11
            else if (IsGfx11(*m_pDevice->Parent()))
            {
                HwlResolveImageGraphics(pPm4CmdBuffer,
                                        srcImage,
                                        srcImageLayout,
                                        dstImage,
                                        dstImageLayout,
                                        regionCount,
                                        pRegions,
                                        flags);
            }
#endif
            else
            {
                PAL_NOT_IMPLEMENTED();
            }
        }
        else if (pPm4CmdBuffer->IsComputeSupported() &&
                 ((srcMethod.shaderCsFmask == 1) ||
                  (srcMethod.shaderCs == 1)))
        {
            ResolveImageCompute(pPm4CmdBuffer,
                                srcImage,
                                srcImageLayout,
                                dstImage,
                                dstImageLayout,
                                resolveMode,
                                regionCount,
                                pRegions,
                                srcMethod,
                                flags);

            HwlFixupResolveDstImage(pPm4CmdBuffer,
                                    *dstImage.GetGfxImage(),
                                    dstImageLayout,
                                    pRegions,
                                    regionCount,
                                    true);
        }
        else
        {
            PAL_NOT_IMPLEMENTED();
        }
    }
}

// =====================================================================================================================
// Executes a CB fixed function resolve.
void RsrcProcMgr::ResolveImageFixedFunc(
    Pm4CmdBuffer*             pCmdBuffer,
    const Image&              srcImage,
    ImageLayout               srcImageLayout,
    const Image&              dstImage,
    ImageLayout               dstImageLayout,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    uint32                    flags
    ) const
{
    const auto* pPublicSettings = m_pDevice->Parent()->GetPublicSettings();

    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<Pm4::UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const auto& srcCreateInfo = srcImage.GetImageCreateInfo();
    const auto& dstCreateInfo = dstImage.GetImageCreateInfo();

    ViewportParams viewportInfo = { };
    viewportInfo.count                 = 1;
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

    const ColorTargetViewInternalCreateInfo colorViewInfoInternal = { };

    ColorTargetViewCreateInfo srcColorViewInfo = { };
    srcColorViewInfo.imageInfo.pImage    = &srcImage;
    srcColorViewInfo.imageInfo.arraySize = 1;
    srcColorViewInfo.flags.bypassMall = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                       RpmViewsBypassMallOnCbDbWrite);

    ColorTargetViewCreateInfo dstColorViewInfo = { };
    dstColorViewInfo.imageInfo.pImage    = &dstImage;
    dstColorViewInfo.imageInfo.arraySize = 1;
    dstColorViewInfo.flags.bypassMall = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                       RpmViewsBypassMallOnCbDbWrite);

    BindTargetParams bindTargetsInfo = {};
    bindTargetsInfo.colorTargetCount                    = 2;
    bindTargetsInfo.colorTargets[0].pColorTargetView    = nullptr;
    bindTargetsInfo.colorTargets[0].imageLayout.usages  = LayoutColorTarget;
    bindTargetsInfo.colorTargets[0].imageLayout.engines = LayoutUniversalEngine;
    bindTargetsInfo.colorTargets[1].pColorTargetView    = nullptr;
    bindTargetsInfo.colorTargets[1].imageLayout.usages  = LayoutColorTarget;
    bindTargetsInfo.colorTargets[1].imageLayout.engines = LayoutUniversalEngine;

    // Save current command buffer state and bind graphics state which is common for all regions.
    pCmdBuffer->CmdSaveGraphicsState();
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(srcCreateInfo.samples, srcCreateInfo.fragments));
    pCmdBuffer->CmdBindColorBlendState(m_pBlendDisableState);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthDisableState);

    const GraphicsPipeline* pPipelinePrevious      = nullptr;
    const GraphicsPipeline* pPipelineByImageFormat =
        GetGfxPipelineByTargetIndexAndFormat(ResolveFixedFunc_32ABGR, 0, srcCreateInfo.swizzledFormat);

    // Put ImageResolveInvertY value in user data 0 used by VS.
    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 0, 1, &flags);

    // Each region needs to be resolved individually.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        LinearAllocatorAuto<VirtualLinearAllocator> regionAlloc(pCmdBuffer->Allocator(), false);

        srcColorViewInfo.swizzledFormat                = srcCreateInfo.swizzledFormat;
        dstColorViewInfo.swizzledFormat                = dstCreateInfo.swizzledFormat;
        dstColorViewInfo.imageInfo.baseSubRes.mipLevel = pRegions[idx].dstMipLevel;

        // Override the formats with the caller's "reinterpret" format:
        if (Formats::IsUndefined(pRegions[idx].swizzledFormat.format) == false)
        {
            // We require that the channel formats match.
            PAL_ASSERT(Formats::ShareChFmt(srcColorViewInfo.swizzledFormat.format,
                                           pRegions[idx].swizzledFormat.format));
            PAL_ASSERT(Formats::ShareChFmt(dstColorViewInfo.swizzledFormat.format,
                                           pRegions[idx].swizzledFormat.format));

            const SubresId srcSubres = { pRegions[idx].srcPlane, 0, pRegions[idx].srcSlice };
            const SubresId dstSubres = { pRegions[idx].dstPlane, pRegions[idx].dstMipLevel, pRegions[idx].dstSlice };

            // If the specified format exactly matches the image formats the resolve will always work. Otherwise, the
            // images must support format replacement.
            PAL_ASSERT(Formats::HaveSameNumFmt(srcColorViewInfo.swizzledFormat.format,
                                               pRegions[idx].swizzledFormat.format) ||
                       srcImage.GetGfxImage()->IsFormatReplaceable(srcSubres, srcImageLayout, false));

            PAL_ASSERT(Formats::HaveSameNumFmt(dstColorViewInfo.swizzledFormat.format,
                                               pRegions[idx].swizzledFormat.format) ||
                       dstImage.GetGfxImage()->IsFormatReplaceable(dstSubres, dstImageLayout, true));

            srcColorViewInfo.swizzledFormat.format = pRegions[idx].swizzledFormat.format;
            dstColorViewInfo.swizzledFormat.format = pRegions[idx].swizzledFormat.format;
        }

        // Setup the viewport and scissor to restrict rendering to the destination region being copied.
        viewportInfo.viewports[0].originX = static_cast<float>(pRegions[idx].dstOffset.x);
        viewportInfo.viewports[0].originY = static_cast<float>(pRegions[idx].dstOffset.y);
        viewportInfo.viewports[0].width   = static_cast<float>(pRegions[idx].extent.width);
        viewportInfo.viewports[0].height  = static_cast<float>(pRegions[idx].extent.height);

        scissorInfo.scissors[0].offset.x      = pRegions[idx].dstOffset.x;
        scissorInfo.scissors[0].offset.y      = pRegions[idx].dstOffset.y;
        scissorInfo.scissors[0].extent.width  = pRegions[idx].extent.width;
        scissorInfo.scissors[0].extent.height = pRegions[idx].extent.height;

        const GraphicsPipeline* pPipeline =
            Formats::IsUndefined(pRegions[idx].swizzledFormat.format)
            ? pPipelineByImageFormat
            : GetGfxPipelineByTargetIndexAndFormat(ResolveFixedFunc_32ABGR, 0, pRegions[idx].swizzledFormat);

        if (pPipelinePrevious != pPipeline)
        {
            pPipelinePrevious = pPipeline;
            pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, pPipeline, InternalApiPsoHash, });
        }

        pCmdBuffer->CmdSetViewports(viewportInfo);
        pCmdBuffer->CmdSetScissorRects(scissorInfo);

        for (uint32 slice = 0; slice < pRegions[idx].numSlices; ++slice)
        {
            srcColorViewInfo.imageInfo.baseSubRes.arraySlice = (pRegions[idx].srcSlice + slice);
            dstColorViewInfo.imageInfo.baseSubRes.arraySlice = (pRegions[idx].dstSlice + slice);

            LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

            IColorTargetView* pSrcColorView = nullptr;
            IColorTargetView* pDstColorView = nullptr;

            void* pSrcColorViewMem =
                PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);
            void* pDstColorViewMem =
                PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

            if ((pDstColorViewMem == nullptr) || (pSrcColorViewMem == nullptr))
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {
                Result result = m_pDevice->CreateColorTargetView(srcColorViewInfo,
                                                                 colorViewInfoInternal,
                                                                 pSrcColorViewMem,
                                                                 &pSrcColorView);
                PAL_ASSERT(result == Result::Success);
                if (result == Result::Success)
                {
                    result = m_pDevice->CreateColorTargetView(dstColorViewInfo,
                                                              colorViewInfoInternal,
                                                              pDstColorViewMem,
                                                              &pDstColorView);
                    PAL_ASSERT(result == Result::Success);
                }

                if (result == Result::Success)
                {
                    bindTargetsInfo.colorTargets[0].pColorTargetView = pSrcColorView;
                    bindTargetsInfo.colorTargets[1].pColorTargetView = pDstColorView;
                    bindTargetsInfo.colorTargetCount = 2;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    // Draw a fullscreen quad.
                    pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                    // Unbind the color-target view and destroy it.
                    bindTargetsInfo.colorTargetCount = 0;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);
                }

            }

            PAL_SAFE_FREE(pSrcColorViewMem, &sliceAlloc);
            PAL_SAFE_FREE(pDstColorViewMem, &sliceAlloc);
        } // End for each slice.
    } // End for each region.

    // Restore original command buffer state.
    pCmdBuffer->CmdRestoreGraphicsState();
}

// =====================================================================================================================
// Resolves a multisampled depth-stencil source Image into the single-sampled destination Image using a pixel shader.
void RsrcProcMgr::ResolveImageDepthStencilGraphics(
    Pm4CmdBuffer*             pCmdBuffer,
    const Image&              srcImage,
    ImageLayout               srcImageLayout,
    const Image&              dstImage,
    ImageLayout               dstImageLayout,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    uint32                    flags
    ) const
{
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<Pm4::UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const auto& device          = *m_pDevice->Parent();
    const auto* pPublicSettings = device.GetPublicSettings();
    const auto& dstCreateInfo   = dstImage.GetImageCreateInfo();
    const auto& srcCreateInfo   = srcImage.GetImageCreateInfo();
    const auto& srcImageInfo    = srcImage.GetImageInfo();

    LateExpandShaderResolveSrc(pCmdBuffer,
                               srcImage,
                               srcImageLayout,
                               pRegions,
                               regionCount,
                               srcImageInfo.resolveMethod,
                               false);

    // This path only works on depth-stencil images.
    PAL_ASSERT((srcCreateInfo.usageFlags.depthStencil && dstCreateInfo.usageFlags.depthStencil) ||
               (Formats::IsDepthStencilOnly(srcCreateInfo.swizzledFormat.format) &&
                Formats::IsDepthStencilOnly(dstCreateInfo.swizzledFormat.format)));

    const StencilRefMaskParams stencilRefMasks = { 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0x01, 0xFF, };

    // Initialize some structures we will need later on.
    ViewportParams viewportInfo = { };
    viewportInfo.count                 = 1;
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

    const DepthStencilViewInternalCreateInfo noDepthViewInfoInternal = { };
    DepthStencilViewCreateInfo               depthViewInfo           = { };
    depthViewInfo.pImage           = &dstImage;
    depthViewInfo.arraySize        = 1;
    depthViewInfo.flags.bypassMall = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnCbDbWrite);

    // Save current command buffer state and bind graphics state which is common for all regions.
    pCmdBuffer->CmdSaveGraphicsState();
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(dstCreateInfo.samples, dstCreateInfo.fragments));
    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    // Determine which format we should use to view the source image. The initial value is the stencil format.
    SwizzledFormat srcFormat =
    {
        ChNumFormat::Undefined,
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
    };

    // Each region needs to be resolved individually.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        // Same sanity checks of the region planes.
        const bool isDepth = dstImage.IsDepthPlane(pRegions[idx].dstPlane);
        PAL_ASSERT((srcImage.IsDepthPlane(pRegions[idx].srcPlane) ||
                    srcImage.IsStencilPlane(pRegions[idx].srcPlane)) &&
                   (pRegions[idx].srcPlane == pRegions[idx].dstPlane));

        // This path can't reinterpret the resolve format.
        const SubresId dstStartSubres =
        {
            pRegions[idx].dstPlane,
            pRegions[idx].dstMipLevel,
            pRegions[idx].dstSlice
        };

        PAL_ASSERT(Formats::IsUndefined(pRegions[idx].swizzledFormat.format) ||
                  (dstImage.SubresourceInfo(dstStartSubres)->format.format == pRegions[idx].swizzledFormat.format));

        BindTargetParams bindTargetsInfo = { };

        if (isDepth)
        {
            if ((srcCreateInfo.swizzledFormat.format == ChNumFormat::D32_Float_S8_Uint) ||
                Formats::ShareChFmt(srcCreateInfo.swizzledFormat.format, ChNumFormat::X32_Float))
            {
                srcFormat.format = ChNumFormat::X32_Float;
            }
            else
            {
                srcFormat.format = ChNumFormat::X16_Unorm;
            }

            bindTargetsInfo.depthTarget.depthLayout = dstImageLayout;
            pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics,
                                          GetGfxPipeline(ResolveDepth),
                                          InternalApiPsoHash, });
            pCmdBuffer->CmdBindDepthStencilState(m_pDepthResolveState);
        }
        else
        {
            srcFormat.format                          = ChNumFormat::X8_Uint;
            bindTargetsInfo.depthTarget.stencilLayout = dstImageLayout;
            pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, GetGfxPipeline(ResolveStencil),
                                          InternalApiPsoHash, });
            pCmdBuffer->CmdBindDepthStencilState(m_pStencilResolveState);
        }

        // Setup the viewport and scissor to restrict rendering to the destination region being copied.
        viewportInfo.viewports[0].originX = static_cast<float>(pRegions[idx].dstOffset.x);
        viewportInfo.viewports[0].originY = static_cast<float>(pRegions[idx].dstOffset.y);
        viewportInfo.viewports[0].width   = static_cast<float>(pRegions[idx].extent.width);
        viewportInfo.viewports[0].height  = static_cast<float>(pRegions[idx].extent.height);

        scissorInfo.scissors[0].offset.x      = pRegions[idx].dstOffset.x;
        scissorInfo.scissors[0].offset.y      = pRegions[idx].dstOffset.y;
        scissorInfo.scissors[0].extent.width  = pRegions[idx].extent.width;
        scissorInfo.scissors[0].extent.height = pRegions[idx].extent.height;

        // The shader will calculate src coordinates by adding a delta to the dst coordinates. The user data should
        // contain those deltas which are (srcOffset-dstOffset) for X & Y.
        // The shader also needs data for y inverting - a boolean flag and height of the image, so the integer
        // coords in texture-space can be inverted.
        const int32  xOffset     = (pRegions[idx].srcOffset.x - pRegions[idx].dstOffset.x);
        int32_t yOffset = pRegions[idx].srcOffset.y;
        if (TestAnyFlagSet(flags, ImageResolveInvertY))
        {
            yOffset = srcCreateInfo.extent.height - yOffset - pRegions[idx].extent.height;
        }
        yOffset = (yOffset - pRegions[idx].dstOffset.y);
        const uint32 userData[5] =
        {
            reinterpret_cast<const uint32&>(xOffset),
            reinterpret_cast<const uint32&>(yOffset),
            TestAnyFlagSet(flags, ImageResolveInvertY) ? 1u : 0u,
            srcCreateInfo.extent.height - 1,
        };

        pCmdBuffer->CmdSetViewports(viewportInfo);
        pCmdBuffer->CmdSetScissorRects(scissorInfo);
        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 1, 4, userData);

        for (uint32 slice = 0; slice < pRegions[idx].numSlices; ++slice)
        {
            LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

            const SubresId srcSubres = { pRegions[idx].srcPlane, 0, pRegions[idx].srcSlice + slice };
            const SubresId dstSubres =
            {
                pRegions[idx].dstPlane,
                pRegions[idx].dstMipLevel,
                pRegions[idx].dstSlice + slice
            };

            // Create an embedded user-data table and bind it to user data 1. We only need one image view.
            uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                       SrdDwordAlignment(),
                                                                       SrdDwordAlignment(),
                                                                       PipelineBindPoint::Graphics,
                                                                       0);

            // Populate the table with an image view of the source image.
            ImageViewInfo     imageView = { };
            const SubresRange viewRange = { srcSubres, 1, 1, 1 };
            RpmUtil::BuildImageViewInfo(&imageView,
                                        srcImage,
                                        viewRange,
                                        srcFormat,
                                        srcImageLayout,
                                        device.TexOptLevel(),
                                        false);
            device.CreateImageViewSrds(1, &imageView, pSrdTable);

            // Create and bind a depth stencil view of the destination region.
            depthViewInfo.baseArraySlice = dstSubres.arraySlice;
            depthViewInfo.mipLevel       = dstSubres.mipLevel;

            void* pDepthStencilViewMem = PAL_MALLOC(m_pDevice->GetDepthStencilViewSize(nullptr),
                                                    &sliceAlloc,
                                                    AllocInternalTemp);
            if (pDepthStencilViewMem == nullptr)
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {
                IDepthStencilView* pDepthView = nullptr;
                Result result = m_pDevice->CreateDepthStencilView(depthViewInfo,
                                                                  noDepthViewInfoInternal,
                                                                  pDepthStencilViewMem,
                                                                  &pDepthView);
                PAL_ASSERT(result == Result::Success);

                bindTargetsInfo.depthTarget.pDepthStencilView = pDepthView;
                pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                // Draw a fullscreen quad.
                pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                // Unbind the depth view and destroy it.
                bindTargetsInfo.depthTarget.pDepthStencilView = nullptr;
                pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                PAL_SAFE_FREE(pDepthStencilViewMem, &sliceAlloc);
            }
        } // End for each slice.
    } // End for each region.

    // Restore original command buffer state.
    pCmdBuffer->CmdRestoreGraphicsState();

    FixupLateExpandShaderResolveSrc(pCmdBuffer,
                                    srcImage,
                                    srcImageLayout,
                                    pRegions,
                                    regionCount,
                                    srcImageInfo.resolveMethod,
                                    false);
}

// =====================================================================================================================
// Executes a image resolve by performing fixed-func depth copy or stencil copy
void RsrcProcMgr::ResolveImageDepthStencilCopy(
    Pm4CmdBuffer*             pCmdBuffer,
    const Image&              srcImage,
    ImageLayout               srcImageLayout,
    const Image&              dstImage,
    ImageLayout               dstImageLayout,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    uint32                    flags) const
{
    PAL_ASSERT(srcImage.IsDepthStencilTarget() && dstImage.IsDepthStencilTarget());
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<Pm4::UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const auto* pPublicSettings = m_pDevice->Parent()->GetPublicSettings();
    const auto& srcCreateInfo   = srcImage.GetImageCreateInfo();
    const auto& dstCreateInfo   = dstImage.GetImageCreateInfo();

    ViewportParams viewportInfo = {};
    viewportInfo.count = 1;

    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;

    viewportInfo.horzClipRatio    = FLT_MAX;
    viewportInfo.horzDiscardRatio = 1.0f;
    viewportInfo.vertClipRatio    = FLT_MAX;
    viewportInfo.vertDiscardRatio = 1.0f;
    viewportInfo.depthRange       = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo = {};
    scissorInfo.count = 1;

    DepthStencilViewCreateInfo srcDepthViewInfo = {};
    srcDepthViewInfo.pImage                = &srcImage;
    srcDepthViewInfo.arraySize             = 1;
    srcDepthViewInfo.flags.readOnlyDepth   = 1;
    srcDepthViewInfo.flags.readOnlyStencil = 1;
    srcDepthViewInfo.flags.bypassMall = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                       RpmViewsBypassMallOnCbDbWrite);

    ColorTargetViewCreateInfo dstColorViewInfo = {};
    dstColorViewInfo.imageInfo.pImage    = &dstImage;
    dstColorViewInfo.imageInfo.arraySize = 1;
    dstColorViewInfo.flags.bypassMall = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                       RpmViewsBypassMallOnCbDbWrite);

    BindTargetParams bindTargetsInfo = {};
    bindTargetsInfo.colorTargetCount = 1;
    bindTargetsInfo.colorTargets[0].pColorTargetView = nullptr;
    bindTargetsInfo.colorTargets[0].imageLayout.usages = LayoutColorTarget;
    bindTargetsInfo.colorTargets[0].imageLayout.engines = LayoutUniversalEngine;

    bindTargetsInfo.depthTarget.depthLayout.usages = LayoutDepthStencilTarget;
    bindTargetsInfo.depthTarget.depthLayout.engines = LayoutUniversalEngine;
    bindTargetsInfo.depthTarget.stencilLayout.usages = LayoutDepthStencilTarget;
    bindTargetsInfo.depthTarget.stencilLayout.engines = LayoutUniversalEngine;

    // Save current command buffer state and bind graphics state which is common for all regions.
    pCmdBuffer->CmdSaveGraphicsState();
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(1u, 1u));
    pCmdBuffer->CmdBindColorBlendState(m_pBlendDisableState);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthDisableState);

    // Put ImageResolveInvertY value in user data 0 used by VS.
    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 0, 1, &flags);

    // Each region needs to be resolved individually.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        LinearAllocatorAuto<VirtualLinearAllocator> regionAlloc(pCmdBuffer->Allocator(), false);

        dstColorViewInfo.imageInfo.baseSubRes.mipLevel = pRegions[idx].dstMipLevel;

        // Setup the viewport and scissor to restrict rendering to the destination region being copied.
        // srcOffset and dstOffset have to be exactly same
        PAL_ASSERT((pRegions[idx].srcOffset.x == pRegions[idx].dstOffset.x) &&
                   (pRegions[idx].srcOffset.y == pRegions[idx].dstOffset.y));
        viewportInfo.viewports[0].originX = static_cast<float>(pRegions[idx].srcOffset.x);
        viewportInfo.viewports[0].originY = static_cast<float>(pRegions[idx].srcOffset.y);
        viewportInfo.viewports[0].width = static_cast<float>(pRegions[idx].extent.width);
        viewportInfo.viewports[0].height = static_cast<float>(pRegions[idx].extent.height);

        scissorInfo.scissors[0].offset.x = pRegions[idx].srcOffset.x;
        scissorInfo.scissors[0].offset.y = pRegions[idx].srcOffset.y;
        scissorInfo.scissors[0].extent.width = pRegions[idx].extent.width;
        scissorInfo.scissors[0].extent.height = pRegions[idx].extent.height;

        pCmdBuffer->CmdSetViewports(viewportInfo);
        pCmdBuffer->CmdSetScissorRects(scissorInfo);

        if (srcCreateInfo.flags.sampleLocsAlwaysKnown != 0)
        {
            PAL_ASSERT(pRegions[idx].pQuadSamplePattern != nullptr);
            pCmdBuffer->CmdSetMsaaQuadSamplePattern(srcCreateInfo.samples, *pRegions[idx].pQuadSamplePattern);
        }
        else
        {
            PAL_ASSERT(pRegions[idx].pQuadSamplePattern == nullptr);
        }

        for (uint32 slice = 0; slice < pRegions[idx].numSlices; ++slice)
        {
            DepthStencilViewInternalCreateInfo depthViewInfoInternal = {};
            ColorTargetViewInternalCreateInfo  colorViewInfoInternal = {};
            colorViewInfoInternal.flags.depthStencilCopy = 1;

            srcDepthViewInfo.baseArraySlice = (pRegions[idx].srcSlice + slice);
            dstColorViewInfo.imageInfo.baseSubRes.arraySlice = (pRegions[idx].dstSlice + slice);

            LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

            IDepthStencilView* pSrcDepthView = nullptr;
            IColorTargetView* pDstColorView = nullptr;

            void* pSrcDepthViewMem =
                PAL_MALLOC(m_pDevice->GetDepthStencilViewSize(nullptr), &sliceAlloc, AllocInternalTemp);
            void* pDstColorViewMem =
                PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

            if ((pDstColorViewMem == nullptr) || (pSrcDepthViewMem == nullptr))
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {
                dstColorViewInfo.imageInfo.baseSubRes.plane = pRegions[idx].dstPlane;

                SubresId dstSubresId   = {};
                dstSubresId.mipLevel   = pRegions[idx].dstMipLevel;
                dstSubresId.arraySlice = (pRegions[idx].dstSlice + slice);
                dstSubresId.plane      = pRegions[idx].dstPlane;

                dstColorViewInfo.swizzledFormat.format = dstImage.SubresourceInfo(dstSubresId)->format.format;

                if (dstImage.IsDepthPlane(pRegions[idx].dstPlane))
                {
                    depthViewInfoInternal.flags.isDepthCopy = 1;

                    dstColorViewInfo.swizzledFormat.swizzle =
                        {ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One};
                    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, GetGfxPipeline(ResolveDepthCopy),
                                                  InternalApiPsoHash, });
                }
                else if (dstImage.IsStencilPlane(pRegions[idx].dstPlane))
                {
                    // Fixed-func stencil copies stencil value from db to g chanenl of cb.
                    // Swizzle the stencil plance to 0X00.
                    depthViewInfoInternal.flags.isStencilCopy = 1;

                    dstColorViewInfo.swizzledFormat.swizzle =
                        { ChannelSwizzle::Zero, ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::One };
                    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics,
                                                  GetGfxPipeline(ResolveStencilCopy),
                                                  InternalApiPsoHash, });
                }
                else
                {
                    PAL_ASSERT_ALWAYS();
                }

                Result result = m_pDevice->CreateDepthStencilView(srcDepthViewInfo,
                                                                  depthViewInfoInternal,
                                                                  pSrcDepthViewMem,
                                                                  &pSrcDepthView);
                PAL_ASSERT(result == Result::Success);

                if (result == Result::Success)
                {
                    result = m_pDevice->CreateColorTargetView(dstColorViewInfo,
                                                              colorViewInfoInternal,
                                                              pDstColorViewMem,
                                                              &pDstColorView);
                    PAL_ASSERT(result == Result::Success);
                }

                if (result == Result::Success)
                {
                    bindTargetsInfo.colorTargetCount = 1;
                    bindTargetsInfo.colorTargets[0].pColorTargetView = pDstColorView;
                    bindTargetsInfo.depthTarget.pDepthStencilView = pSrcDepthView;

                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    // Draw a fullscreen quad.
                    pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                    // Unbind the color-target and depth-stencil target view and destroy them.
                    bindTargetsInfo.colorTargetCount = 0;
                    bindTargetsInfo.depthTarget.pDepthStencilView = nullptr;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);
                }
            }

            PAL_SAFE_FREE(pSrcDepthViewMem, &sliceAlloc);
            PAL_SAFE_FREE(pDstColorViewMem, &sliceAlloc);
        } // End for each slice.
    } // End for each region.

      // Restore original command buffer state.
    pCmdBuffer->CmdRestoreGraphicsState();
}

// =====================================================================================================================
// Executes a generic color blit which acts upon the specified color Image. If mipCondDwordsAddr is non-zero, it is the
// GPU virtual address of an array of conditional DWORDs, one for each mip level in the image. RPM will use these
// DWORDs to conditionally execute this blit on a per-mip basis.
void RsrcProcMgr::GenericColorBlit(
    Pm4CmdBuffer*                pCmdBuffer,
    const Image&                 dstImage,
    const SubresRange&           range,
    const MsaaQuadSamplePattern* pQuadSamplePattern,
    RpmGfxPipeline               pipeline,
    const GpuMemory*             pGpuMemory,
    gpusize                      metaDataOffset
    ) const
{
    PAL_ASSERT(range.numPlanes == 1);
    PAL_ASSERT(dstImage.IsRenderTarget());
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<Pm4::UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const auto* pPublicSettings  = m_pDevice->Parent()->GetPublicSettings();
    const auto& imageCreateInfo  = dstImage.GetImageCreateInfo();
    const bool  is3dImage        = (imageCreateInfo.imageType == ImageType::Tex3d);
    const bool  isDecompress     = ((pipeline == RpmGfxPipeline::DccDecompress) ||
                                    (pipeline == RpmGfxPipeline::FastClearElim) ||
                                    (pipeline == RpmGfxPipeline::FmaskDecompress));

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

    ScissorRectParams scissorInfo;
    scissorInfo.count                  = 1;
    scissorInfo.scissors[0].offset.x   = 0;
    scissorInfo.scissors[0].offset.y   = 0;

    ColorTargetViewInternalCreateInfo colorViewInfoInternal = { };
    colorViewInfoInternal.flags.dccDecompress   = (pipeline == RpmGfxPipeline::DccDecompress);
    colorViewInfoInternal.flags.fastClearElim   = (pipeline == RpmGfxPipeline::FastClearElim);
    colorViewInfoInternal.flags.fmaskDecompress = (pipeline == RpmGfxPipeline::FmaskDecompress);

    ColorTargetViewCreateInfo colorViewInfo = { };
    colorViewInfo.swizzledFormat      = imageCreateInfo.swizzledFormat;
    colorViewInfo.imageInfo.pImage    = &dstImage;
    colorViewInfo.imageInfo.arraySize = 1;
    colorViewInfo.imageInfo.baseSubRes.plane = range.startSubres.plane;
    colorViewInfo.flags.bypassMall = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnCbDbWrite);

    if (is3dImage)
    {
        colorViewInfo.zRange.extent     = 1;
        colorViewInfo.flags.zRangeValid = 1;
    }

    BindTargetParams bindTargetsInfo = { };
    bindTargetsInfo.colorTargets[0].pColorTargetView    = nullptr;
    bindTargetsInfo.colorTargets[0].imageLayout.usages  = LayoutColorTarget;
    bindTargetsInfo.colorTargets[0].imageLayout.engines = LayoutUniversalEngine;
    bindTargetsInfo.depthTarget.pDepthStencilView       = nullptr;
    bindTargetsInfo.depthTarget.depthLayout.usages      = LayoutDepthStencilTarget;
    bindTargetsInfo.depthTarget.depthLayout.engines     = LayoutUniversalEngine;
    bindTargetsInfo.depthTarget.stencilLayout.usages    = LayoutDepthStencilTarget;
    bindTargetsInfo.depthTarget.stencilLayout.engines   = LayoutUniversalEngine;

    const StencilRefMaskParams stencilRefMasks = { 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0x01, 0xFF };

    // Save current command buffer state and bind graphics state which is common for all mipmap levels.
    pCmdBuffer->CmdSaveGraphicsState();
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, GetGfxPipeline(pipeline), InternalApiPsoHash, });

    BindCommonGraphicsState(pCmdBuffer);

    SwizzledFormat swizzledFormat = {};

    swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
    swizzledFormat.swizzle = { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

    pCmdBuffer->CmdOverwriteRbPlusFormatForBlits(swizzledFormat, 0);

    pCmdBuffer->CmdBindColorBlendState(m_pBlendDisableState);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthDisableState);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(dstImage.GetImageCreateInfo().samples,
                                              dstImage.GetImageCreateInfo().fragments));

    if (pQuadSamplePattern != nullptr)
    {
        pCmdBuffer->CmdSetMsaaQuadSamplePattern(dstImage.GetImageCreateInfo().samples, *pQuadSamplePattern);
    }

    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    RpmUtil::WriteVsZOut(pCmdBuffer, 1.0f);

    const uint32    lastMip                = (range.startSubres.mipLevel + range.numMips - 1);
    const Pm4Image* pPm4Image              = static_cast<Pm4Image*>(dstImage.GetGfxImage());
    gpusize         mipCondDwordsOffset    = metaDataOffset;
    bool            needDisablePredication = false;

    for (uint32 mip = range.startSubres.mipLevel; mip <= lastMip; ++mip)
    {
        // If this is a decompress operation of some sort, then don't bother continuing unless this
        // subresource supports expansion.
        if ((isDecompress == false) || (pPm4Image->CanMipSupportMetaData(mip)))
        {
            // Use predication to skip this operation based on the image's conditional dwords.
            // We can only perform this optimization if the client is not currently using predication.
            if ((pCmdBuffer->GetGfxCmdBufStateFlags().clientPredicate == 0) && (pGpuMemory != nullptr))
            {
                // Set/Enable predication
                pCmdBuffer->CmdSetPredication(nullptr,
                                              0,
                                              pGpuMemory,
                                              mipCondDwordsOffset,
                                              PredicateType::Boolean64,
                                              true,
                                              false,
                                              false);
                mipCondDwordsOffset += PredicationAlign; // Advance to the next mip's conditional meta-data.

                needDisablePredication = true;
            }

            const SubresId mipSubres  = { range.startSubres.plane, mip, 0 };
            const auto&    subResInfo = *dstImage.SubresourceInfo(mipSubres);

            // All slices of the same mipmap level can re-use the same viewport & scissor states.
            viewportInfo.viewports[0].width       = static_cast<float>(subResInfo.extentTexels.width);
            viewportInfo.viewports[0].height      = static_cast<float>(subResInfo.extentTexels.height);
            scissorInfo.scissors[0].extent.width  = subResInfo.extentTexels.width;
            scissorInfo.scissors[0].extent.height = subResInfo.extentTexels.height;

            pCmdBuffer->CmdSetViewports(viewportInfo);
            pCmdBuffer->CmdSetScissorRects(scissorInfo);

            // We need to draw each array slice individually because we cannot select which array slice to render to
            // without a Geometry Shader. If this is a 3D Image, we need to include all slices for this mipmap level.
            const uint32 baseSlice = (is3dImage ? 0                             : range.startSubres.arraySlice);
            const uint32 numSlices = (is3dImage ? subResInfo.extentTexels.depth : range.numSlices);
            const uint32 lastSlice = baseSlice + numSlices - 1;

            for (uint32 arraySlice = baseSlice; arraySlice <= lastSlice; ++arraySlice)
            {
                LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

                // Create and bind a color-target view for this mipmap level and slice.
                IColorTargetView* pColorView = nullptr;
                void* pColorViewMem =
                    PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

                if (pColorViewMem == nullptr)
                {
                    pCmdBuffer->NotifyAllocFailure();
                }
                else
                {
                    if (is3dImage)
                    {
                        colorViewInfo.zRange.offset = arraySlice;
                    }
                    else
                    {
                        colorViewInfo.imageInfo.baseSubRes.arraySlice = arraySlice;
                    }

                    colorViewInfo.imageInfo.baseSubRes.mipLevel = mip;

                    Result result = m_pDevice->CreateColorTargetView(colorViewInfo,
                                                                     colorViewInfoInternal,
                                                                     pColorViewMem,
                                                                     &pColorView);
                    PAL_ASSERT(result == Result::Success);

                    bindTargetsInfo.colorTargets[0].pColorTargetView = pColorView;
                    bindTargetsInfo.colorTargetCount = 1;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    // Draw a fullscreen quad.
                    pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                    // Unbind the color-target view and destroy it.
                    bindTargetsInfo.colorTargetCount = 0;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    PAL_SAFE_FREE(pColorViewMem, &sliceAlloc);
                }
            } // End for each array slice.
        }
    } // End for each mip level.

    if (needDisablePredication)
    {
        // Disable predication
        pCmdBuffer->CmdSetPredication(nullptr,
                                      0,
                                      nullptr,
                                      0,
                                      static_cast<PredicateType>(0),
                                      false,
                                      false,
                                      false);
    }

    // Restore original command buffer state.
    pCmdBuffer->CmdRestoreGraphicsState();
}

// =====================================================================================================================
// Performs a depth/stencil expand (decompress) on the provided image.
bool RsrcProcMgr::ExpandDepthStencil(
    Pm4CmdBuffer*                pCmdBuffer,
    const Image&                 image,
    const MsaaQuadSamplePattern* pQuadSamplePattern,
    const SubresRange&           range
    ) const
{
    PAL_ASSERT(range.numPlanes == 1);
    PAL_ASSERT(image.IsDepthStencilTarget());
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<Pm4::UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const auto*                pPublicSettings  = m_pDevice->Parent()->GetPublicSettings();
    const StencilRefMaskParams stencilRefMasks  = { 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0x01, 0xFF };

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

    ScissorRectParams scissorInfo      = { };
    scissorInfo.count                  = 1;
    scissorInfo.scissors[0].offset.x   = 0;
    scissorInfo.scissors[0].offset.y   = 0;

    DepthStencilViewInternalCreateInfo depthViewInfoInternal = { };
    depthViewInfoInternal.flags.isExpand = 1;

    DepthStencilViewCreateInfo depthViewInfo = { };
    depthViewInfo.pImage           = &image;
    depthViewInfo.arraySize        = 1;
    depthViewInfo.flags.bypassMall = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnCbDbWrite);

    if (image.IsDepthPlane(range.startSubres.plane))
    {
        depthViewInfo.flags.readOnlyStencil = 1;
    }
    else
    {
        depthViewInfo.flags.readOnlyDepth = 1;
    }

    BindTargetParams bindTargetsInfo = { };
    bindTargetsInfo.depthTarget.pDepthStencilView     = nullptr;
    bindTargetsInfo.depthTarget.depthLayout.usages    = LayoutDepthStencilTarget;
    bindTargetsInfo.depthTarget.depthLayout.engines   = LayoutUniversalEngine;
    bindTargetsInfo.depthTarget.stencilLayout.usages  = LayoutDepthStencilTarget;
    bindTargetsInfo.depthTarget.stencilLayout.engines = LayoutUniversalEngine;

    // Save current command buffer state and bind graphics state which is common for all subresources.
    pCmdBuffer->CmdSaveGraphicsState();
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, GetGfxPipeline(DepthExpand), InternalApiPsoHash, });
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthExpandState);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(image.GetImageCreateInfo().samples,
                                              image.GetImageCreateInfo().fragments));

    if (pQuadSamplePattern != nullptr)
    {
        pCmdBuffer->CmdSetMsaaQuadSamplePattern(image.GetImageCreateInfo().samples, *pQuadSamplePattern);
    }

    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    RpmUtil::WriteVsZOut(pCmdBuffer, 1.0f);

    const Pm4Image* pPm4Image = static_cast<Pm4Image*>(image.GetGfxImage());
    const uint32    lastMip   = (range.startSubres.mipLevel   + range.numMips   - 1);
    const uint32    lastSlice = (range.startSubres.arraySlice + range.numSlices - 1);

    for (depthViewInfo.mipLevel  = range.startSubres.mipLevel;
         depthViewInfo.mipLevel <= lastMip;
         ++depthViewInfo.mipLevel)
    {
        if (pPm4Image->CanMipSupportMetaData(depthViewInfo.mipLevel))
        {
            LinearAllocatorAuto<VirtualLinearAllocator> mipAlloc(pCmdBuffer->Allocator(), false);

            const SubresId mipSubres  = { range.startSubres.plane, depthViewInfo.mipLevel, 0 };
            const auto&    subResInfo = *image.SubresourceInfo(mipSubres);

            // All slices of the same mipmap level can re-use the same viewport/scissor state.
            viewportInfo.viewports[0].width  = static_cast<float>(subResInfo.extentTexels.width);
            viewportInfo.viewports[0].height = static_cast<float>(subResInfo.extentTexels.height);

            scissorInfo.scissors[0].extent.width  = subResInfo.extentTexels.width;
            scissorInfo.scissors[0].extent.height = subResInfo.extentTexels.height;

            pCmdBuffer->CmdSetViewports(viewportInfo);
            pCmdBuffer->CmdSetScissorRects(scissorInfo);

            for (depthViewInfo.baseArraySlice  = range.startSubres.arraySlice;
                 depthViewInfo.baseArraySlice <= lastSlice;
                 ++depthViewInfo.baseArraySlice)
            {
                LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

                // Create and bind a depth stencil view of the current subresource.
                IDepthStencilView* pDepthView = nullptr;
                void* pDepthViewMem =
                    PAL_MALLOC(m_pDevice->GetDepthStencilViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

                if (pDepthViewMem == nullptr)
                {
                    pCmdBuffer->NotifyAllocFailure();
                }
                else
                {
                    Result result = m_pDevice->CreateDepthStencilView(depthViewInfo,
                                                                      depthViewInfoInternal,
                                                                      pDepthViewMem,
                                                                      &pDepthView);
                    PAL_ASSERT(result == Result::Success);

                    bindTargetsInfo.depthTarget.pDepthStencilView = pDepthView;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    // Draw a fullscreen quad.
                    pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                    PAL_SAFE_FREE(pDepthViewMem, &sliceAlloc);

                    // Unbind the depth view and destroy it.
                    bindTargetsInfo.depthTarget.pDepthStencilView = nullptr;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);
                }
            }
        }
    }

    // Restore command buffer state.
    pCmdBuffer->CmdRestoreGraphicsState();

    // Compute path was not used
    return false;
}

// =====================================================================================================================
// Performs a depth/stencil resummarization on the provided image.  This operation recalculates the HiZ range in the
// htile based on the z-buffer values.
void RsrcProcMgr::ResummarizeDepthStencil(
    Pm4CmdBuffer*                pCmdBuffer,
    const Image&                 image,
    ImageLayout                  imageLayout,
    const MsaaQuadSamplePattern* pQuadSamplePattern,
    const SubresRange&           range
    ) const
{
    PAL_ASSERT(range.numPlanes == 1);
    PAL_ASSERT(image.IsDepthStencilTarget());
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<Pm4::UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const auto*                pPublicSettings  = m_pDevice->Parent()->GetPublicSettings();
    const StencilRefMaskParams stencilRefMasks  = { 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0x01, 0xFF };

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

    ScissorRectParams scissorInfo    = { };
    scissorInfo.count                = 1;
    scissorInfo.scissors[0].offset.x = 0;
    scissorInfo.scissors[0].offset.y = 0;

    const DepthStencilViewInternalCreateInfo depthViewInfoInternal = { };

    DepthStencilViewCreateInfo depthViewInfo = { };
    depthViewInfo.pImage    = &image;
    depthViewInfo.arraySize = 1;
    depthViewInfo.flags.resummarizeHiZ = 1;
    depthViewInfo.flags.bypassMall = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnCbDbWrite);

    if (image.IsDepthPlane(range.startSubres.plane))
    {
        depthViewInfo.flags.readOnlyStencil = 1;
    }
    else
    {
        depthViewInfo.flags.readOnlyDepth = 1;
    }

    BindTargetParams bindTargetsInfo = {};
    bindTargetsInfo.depthTarget.pDepthStencilView = nullptr;
    bindTargetsInfo.depthTarget.depthLayout       = imageLayout;
    bindTargetsInfo.depthTarget.stencilLayout     = imageLayout;

    // Save current command buffer state and bind graphics state which is common for all subresources.
    pCmdBuffer->CmdSaveGraphicsState();
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, GetGfxPipeline(DepthResummarize), InternalApiPsoHash, });
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthResummarizeState);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(image.GetImageCreateInfo().samples,
                                              image.GetImageCreateInfo().fragments));

    if (pQuadSamplePattern != nullptr)
    {
        pCmdBuffer->CmdSetMsaaQuadSamplePattern(image.GetImageCreateInfo().samples, *pQuadSamplePattern);
    }

    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    RpmUtil::WriteVsZOut(pCmdBuffer, 1.0f);

    const Pm4Image* pPm4Image = static_cast<Pm4Image*>(image.GetGfxImage());
    const uint32    lastMip   = range.startSubres.mipLevel   + range.numMips   - 1;
    const uint32    lastSlice = range.startSubres.arraySlice + range.numSlices - 1;

    for (depthViewInfo.mipLevel  = range.startSubres.mipLevel;
         depthViewInfo.mipLevel <= lastMip;
         ++depthViewInfo.mipLevel)
    {
        if (pPm4Image->CanMipSupportMetaData(depthViewInfo.mipLevel))
        {
            LinearAllocatorAuto<VirtualLinearAllocator> mipAlloc(pCmdBuffer->Allocator(), false);

            const SubresId mipSubres  = { range.startSubres.plane, depthViewInfo.mipLevel, 0 };
            const auto&    subResInfo = *image.SubresourceInfo(mipSubres);

            // All slices of the same mipmap level can re-use the same viewport/scissor state.
            viewportInfo.viewports[0].width  = static_cast<float>(subResInfo.extentTexels.width);
            viewportInfo.viewports[0].height = static_cast<float>(subResInfo.extentTexels.height);

            scissorInfo.scissors[0].extent.width  = subResInfo.extentTexels.width;
            scissorInfo.scissors[0].extent.height = subResInfo.extentTexels.height;

            pCmdBuffer->CmdSetViewports(viewportInfo);
            pCmdBuffer->CmdSetScissorRects(scissorInfo);

            for (depthViewInfo.baseArraySlice  = range.startSubres.arraySlice;
                 depthViewInfo.baseArraySlice <= lastSlice;
                 ++depthViewInfo.baseArraySlice)
            {
                LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

                // Create and bind a depth stencil view of the current subresource.
                IDepthStencilView* pDepthView = nullptr;
                void* pDepthViewMem =
                    PAL_MALLOC(m_pDevice->GetDepthStencilViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

                if (pDepthViewMem == nullptr)
                {
                    pCmdBuffer->NotifyAllocFailure();
                }
                else
                {
                    Result result = m_pDevice->CreateDepthStencilView(depthViewInfo,
                                                                      depthViewInfoInternal,
                                                                      pDepthViewMem,
                                                                      &pDepthView);
                    PAL_ASSERT(result == Result::Success);

                    bindTargetsInfo.depthTarget.pDepthStencilView = pDepthView;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    // Draw a fullscreen quad.
                    pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                    PAL_SAFE_FREE(pDepthViewMem, &sliceAlloc);

                    // Unbind the depth view and destroy it.
                    bindTargetsInfo.depthTarget.pDepthStencilView = nullptr;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);
                }
            }
        }
    }

    // Restore command buffer state.
    pCmdBuffer->CmdRestoreGraphicsState();
}

// =====================================================================================================================
// Returns a pointer to the compute pipeline used to decompress the supplied image.
const ComputePipeline* RsrcProcMgr::GetComputeMaskRamExpandPipeline(
    const Image& image
    ) const
{
    const auto&  createInfo   = image.GetImageCreateInfo();

    const auto   pipelineEnum = ((createInfo.samples == 1) ? RpmComputePipeline::ExpandMaskRam :
                                 (createInfo.samples == 2) ? RpmComputePipeline::ExpandMaskRamMs2x :
                                 (createInfo.samples == 4) ? RpmComputePipeline::ExpandMaskRamMs4x :
                                 (createInfo.samples == 8) ? RpmComputePipeline::ExpandMaskRamMs8x :
                                 RpmComputePipeline::ExpandMaskRam);

    const ComputePipeline*  pPipeline = GetPipeline(pipelineEnum);

    PAL_ASSERT(pPipeline != nullptr);

    return pPipeline;
}

// =====================================================================================================================
// Returns a pointer to the compute pipeline used for fast-clearing hTile data that is laid out in a linear fashion.
const ComputePipeline* RsrcProcMgr::GetLinearHtileClearPipeline(
    bool   expClearEnable,
    bool   tileStencilDisabled,
    uint32 hTileMask
    ) const
{
    // Determine which pipeline to use for this clear.
    const ComputePipeline* pPipeline = nullptr;
    if (expClearEnable)
    {
        // If Exp/Clear is enabled, fast clears require using a special Exp/Clear shader. One such shader exists for
        // depth/stencil Images and for depth-only Images.
        if (tileStencilDisabled == false)
        {
            pPipeline = GetPipeline(RpmComputePipeline::FastDepthStExpClear);
        }
        else
        {
            pPipeline = GetPipeline(RpmComputePipeline::FastDepthExpClear);
        }
    }
    else if (hTileMask == UINT_MAX)
    {
        // If the HTile mask has all bits set, we can use the standard ClearHtile path.
        // Set the pipeline to null so we don't attempt to use it.
        pPipeline = nullptr;
    }
    else
    {
        // Otherwise use the depth clear read-write shader.
        pPipeline = GetPipeline(RpmComputePipeline::FastDepthClear);
    }

    return pPipeline;
}

// =====================================================================================================================
// This must be called before and after each compute copy. The pre-copy call will insert any required metadata
// decompresses and the post-copy call will fixup any metadata that needs updating. In practice these barriers are
// required in cases where we treat CopyDst as compressed but RPM can't actually write compressed data directly from
// the compute shader.
void RsrcProcMgr::FixupMetadataForComputeDst(
    GfxCmdBuffer*           pCmdBuffer,
    const Image&            dstImage,
    ImageLayout             dstImageLayout,
    uint32                  regionCount,
    const ImageFixupRegion* pRegions,
    bool                    beforeCopy
    ) const
{
    const Pm4Image* pPm4Image = static_cast<Pm4Image*>(dstImage.GetGfxImage());

    // TODO: unify all RPM metadata fixup here; currently only depth image is handled.
    if (pPm4Image->HasHtileData())
    {
        // There is a Hiz issue on gfx10 with compressed depth writes so we need an htile resummarize blt.
        const bool enableCompressedDepthWriteTempWa = IsGfx10(*m_pDevice->Parent());

        // If enable temp workaround for comrpessed depth write, always need barriers for before and after copy.
        bool needBarrier = enableCompressedDepthWriteTempWa;
        for (uint32 i = 0; (needBarrier == false) && (i < regionCount); i++)
        {
            needBarrier = pPm4Image->ShaderWriteIncompatibleWithLayout(pRegions[i].subres, dstImageLayout);
        }

        if (needBarrier)
        {
            AutoBuffer<BarrierTransition, 32, Platform> transitions(regionCount, m_pDevice->GetPlatform());

            if (transitions.Capacity() >= regionCount)
            {
                const uint32 shaderWriteLayout =
                    (enableCompressedDepthWriteTempWa ? (LayoutShaderWrite | LayoutUncompressed) : LayoutShaderWrite);

                for (uint32 i = 0; i < regionCount; i++)
                {
                    transitions[i].imageInfo.pImage                  = &dstImage;
                    transitions[i].imageInfo.subresRange.startSubres = pRegions[i].subres;
                    transitions[i].imageInfo.subresRange.numPlanes   = 1;
                    transitions[i].imageInfo.subresRange.numMips     = 1;
                    transitions[i].imageInfo.subresRange.numSlices   = pRegions[i].numSlices;
                    transitions[i].imageInfo.oldLayout               = dstImageLayout;
                    transitions[i].imageInfo.newLayout               = dstImageLayout;
                    transitions[i].imageInfo.pQuadSamplePattern      = nullptr;

                    // The first barrier must prepare the image for shader writes, perhaps by decompressing metadata.
                    // The second barrier is required to undo those changes, perhaps by resummarizing the metadata.
                    if (beforeCopy)
                    {
                        // Can optimize depth expand to lighter Barrier with UninitializedTarget for full subres copy.
                        const SubResourceInfo* pSubresInfo = dstImage.SubresourceInfo(pRegions[i].subres);
                        const bool fullSubresCopy =
                            ((pRegions[i].offset.x == 0) &&
                             (pRegions[i].offset.y == 0) &&
                             (pRegions[i].offset.z == 0) &&
                             (pRegions[i].extent.width  >= pSubresInfo->extentElements.width) &&
                             (pRegions[i].extent.height >= pSubresInfo->extentElements.height) &&
                             (pRegions[i].extent.depth  >= pSubresInfo->extentElements.depth));

                        if (fullSubresCopy)
                        {
                            transitions[i].imageInfo.oldLayout.usages = LayoutUninitializedTarget;
                        }

                        transitions[i].imageInfo.newLayout.usages |= shaderWriteLayout;
                        transitions[i].srcCacheMask                = CoherCopyDst;
                        transitions[i].dstCacheMask                = CoherShader;
                    }
                    else // After copy
                    {
                        transitions[i].imageInfo.oldLayout.usages |= shaderWriteLayout;
                        transitions[i].srcCacheMask                = CoherShader;
                        transitions[i].dstCacheMask                = CoherCopyDst;
                    }
                }

                // Operations like resummarizes might read the blit's output so we can't optimize the wait point.
                BarrierInfo barrierInfo = {};
                barrierInfo.pTransitions    = &transitions[0];
                barrierInfo.transitionCount = regionCount;
                barrierInfo.waitPoint       = HwPipePreBlt;

                const HwPipePoint releasePipePoint = beforeCopy ? HwPipeBottom : HwPipePostCs;
                barrierInfo.pipePointWaitCount = 1;
                barrierInfo.pPipePoints        = &releasePipePoint;

                pCmdBuffer->CmdBarrier(barrierInfo);
            }
            else
            {
                pCmdBuffer->NotifyAllocFailure();
            }
        }
    }
}

// =====================================================================================================================
// This is called after compute resolve image.
void RsrcProcMgr::FixupComputeResolveDst(
    GfxCmdBuffer*             pCmdBuffer,
    const Image&              dstImage,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions
    ) const
{
    const Pm4Image* pPm4Image = static_cast<Pm4Image*>(dstImage.GetGfxImage());

    if (pPm4Image->HasHtileData())
    {
        PAL_ASSERT((regionCount > 0) && (pRegions != nullptr));

        for (uint32 i = 0; i < regionCount; ++i)
        {
            const ImageResolveRegion& curRegion = pRegions[i];
            SubresRange subresRange = {};
            subresRange.startSubres.plane      = curRegion.dstPlane;
            subresRange.startSubres.mipLevel   = curRegion.dstMipLevel;
            subresRange.startSubres.arraySlice = curRegion.dstSlice;
            subresRange.numPlanes              = 1;
            subresRange.numMips                = 1;
            subresRange.numSlices              = curRegion.numSlices;
            HwlResummarizeHtileCompute(static_cast<Pm4CmdBuffer*>(pCmdBuffer), *dstImage.GetGfxImage(), subresRange);
        }

        // There is a potential problem here because the htile is shared between
        // the depth and stencil planes, but the APIs manage the state of those
        // planes independently.  At this point in the code, we know the depth
        // plane must be in a state that supports being a resolve destination,
        // but the stencil plane may still be in a state that supports stencil
        // target rendering.  Since we are modifying HTILE asynchronously with
        // respect to the DB and through a different data path than the DB, we
        // need to ensure our CS won't overlap with subsequent stencil rendering
        // and that our HTILE updates are immediately visible to the DB.

        BarrierInfo hiZExpandBarrier = {};
        hiZExpandBarrier.waitPoint = HwPipePreCs;

        constexpr HwPipePoint PostCs = HwPipePostCs;
        hiZExpandBarrier.pipePointWaitCount = 1;
        hiZExpandBarrier.pPipePoints = &PostCs;

        BarrierTransition transition = {};
        transition.srcCacheMask = CoherShader;
        transition.dstCacheMask = CoherShader | CoherDepthStencilTarget;
        hiZExpandBarrier.pTransitions = &transition;

        pCmdBuffer->CmdBarrier(hiZExpandBarrier);
    }
}

// =====================================================================================================================
// Selects the appropriate Depth Stencil copy pipeline based on usage and samples
const GraphicsPipeline* RsrcProcMgr::GetCopyDepthStencilPipeline(
    bool   isDepth,
    bool   isDepthStencil,
    uint32 numSamples
    ) const
{
    RpmGfxPipeline pipelineType;

    if (isDepthStencil)
    {
        pipelineType = (numSamples > 1) ? CopyMsaaDepthStencil : CopyDepthStencil;
    }
    else
    {
        if (isDepth)
        {
            pipelineType = (numSamples > 1) ? CopyMsaaDepth : CopyDepth;
        }
        else
        {
            pipelineType = (numSamples > 1) ? CopyMsaaStencil : CopyStencil;
        }
    }

    return GetGfxPipeline(pipelineType);
}

// =====================================================================================================================
// Selects the appropriate scaled Depth Stencil copy pipeline based on usage and samples
const GraphicsPipeline* RsrcProcMgr::GetScaledCopyDepthStencilPipeline(
    bool   isDepth,
    bool   isDepthStencil,
    uint32 numSamples
    ) const
{
    RpmGfxPipeline pipelineType;

    if (isDepthStencil)
    {
        pipelineType = (numSamples > 1) ? ScaledCopyMsaaDepthStencil : ScaledCopyDepthStencil;
    }
    else
    {
        if (isDepth)
        {
            pipelineType = (numSamples > 1) ? ScaledCopyMsaaDepth : ScaledCopyDepth;
        }
        else
        {
            pipelineType = (numSamples > 1) ? ScaledCopyMsaaStencil : ScaledCopyStencil;
        }
    }

    return GetGfxPipeline(pipelineType);
}

} // Pm4
} // Pal
