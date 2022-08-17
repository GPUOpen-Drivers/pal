/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/colorBlendState.h"
#include "core/hw/gfxip/depthStencilState.h"
#include "core/hw/gfxip/msaaState.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/graphicsPipeline.h"
#include "core/hw/gfxip/universalCmdBuffer.h"
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
    Pal::RsrcProcMgr(pDevice)
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
// Builds commands to copy one or more regions from one image to another using a graphics pipeline.
// This path only supports copies between single-sampled non-compressed 2D, 2D color, and 3D images for now.
void RsrcProcMgr::CopyColorImageGraphics(
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
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    // Get some useful information about the image.
    const auto& dstCreateInfo = dstImage.GetImageCreateInfo();
    const auto& srcCreateInfo = srcImage.GetImageCreateInfo();
    const auto& device        = *m_pDevice->Parent();
    const auto& settings      = device.Settings();

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
    colorViewInfo.flags.bypassMall    = TestAnyFlagSet(settings.rpmViewsBypassMall,
                                                       Gfx10RpmViewsBypassMallOnCbDbWrite);

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
        restoreMask              |= HwlBeginGraphicsCopy(pCmdBuffer, pPipeline, dstImage, bitsPerPixel);

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
    HwlEndGraphicsCopy(pStream, restoreMask);

    // Restore original command buffer state.
    pCmdBuffer->CmdRestoreGraphicsState();
}

// =====================================================================================================================
// Copies multisampled depth-stencil images using a graphics pipeline.
void RsrcProcMgr::CopyDepthStencilImageGraphics(
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
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const auto& device        = *m_pDevice->Parent();
    const auto& settings      = device.Settings();
    const auto& texOptLevel   = device.TexOptLevel();
    const auto& dstCreateInfo = dstImage.GetImageCreateInfo();
    const auto& srcCreateInfo = srcImage.GetImageCreateInfo();

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
    depthViewInfo.flags.bypassMall = TestAnyFlagSet(settings.rpmViewsBypassMall, Gfx10RpmViewsBypassMallOnCbDbWrite);

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

                    viewRange        = { pRegions[secondSurface].srcSubres, 1, 1, 1 };

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
        CopyDepthStencilImageGraphics(pCmdBuffer,
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
        CopyColorImageGraphics(pCmdBuffer,
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

    const auto& dstCreateInfo   = pDstImage->GetImageCreateInfo();
    const auto& srcCreateInfo   = pSrcImage->GetImageCreateInfo();
    const auto& device          = *m_pDevice->Parent();
    const auto& settings        = device.Settings();
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

#if PAL_ENABLE_PRINTS_ASSERTS
    PAL_ASSERT(static_cast<const UniversalCmdBuffer*>(pCmdBuffer)->IsGraphicsStatePushed());
#endif

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

    colorViewInfo.flags.bypassMall = TestAnyFlagSet(settings.rpmViewsBypassMall, Gfx10RpmViewsBypassMallOnCbDbWrite);
    depthViewInfo.flags.bypassMall = TestAnyFlagSet(settings.rpmViewsBypassMall, Gfx10RpmViewsBypassMallOnCbDbWrite);

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
        restoreMask              |= HwlBeginGraphicsCopy(pCmdBuffer, pPipeline, *pDstImage, bitsPerPixel);

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
    HwlEndGraphicsCopy(pStream, restoreMask);
}

} // Pm4
} // Pal
