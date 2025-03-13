/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx12/g_gfx12DataFormats.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12Image.h"

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
// Returns a mask with the compression related fields in a srd for bufferview
static uint32 GetBufferSrdCompressionBits(
    const Gfx12PalSettings& gfx12Settings,
    CompressionMode         compressionMode)
{
    static_assert(
        (uint32(CompressionMode::Default)                == BufferViewCompressionDefault)                &&
        (uint32(CompressionMode::ReadEnableWriteEnable)  == BufferViewCompressionReadEnableWriteEnable)  &&
        (uint32(CompressionMode::ReadEnableWriteDisable) == BufferViewCompressionReadEnableWriteDisable));

    if (gfx12Settings.bufferViewCompressionMode != BufferViewCompressionDefault)
    {
        compressionMode = static_cast<CompressionMode>(gfx12Settings.bufferViewCompressionMode);
    }
    else if ((compressionMode == CompressionMode::ReadBypassWriteDisable) &&
             (gfx12Settings.enableCompressionReadBypass == 0))
    {
        compressionMode = CompressionMode::ReadEnableWriteDisable;
    }

    // Func to fill out a mask with the compression fields' values.
    constexpr auto MakeMask = [](bool readEn, bool writeEn, uint32 accessMode) -> uint32
    {
        return ((uint32(readEn) << SqBufRsrcTWord3CompressionEnShift)        |
                (uint32(writeEn) << SqBufRsrcTWord3WriteCompressEnableShift) |
                (accessMode << SqBufRsrcTWord3CompressionAccessModeShift));
    };

    constexpr uint32 CompressionValues[] = {
        MakeMask(true, true, 0),    // Default (RW enabled)
        MakeMask(true, true, 0),    // RW enabled
        MakeMask(true, false, 0),   // R enabled, w disabled
        MakeMask(false, false, 0),  // R bypass, w disabled
    };
    static_assert(ArrayLen(CompressionValues) == uint32(CompressionMode::Count));

    return CompressionValues[uint32(compressionMode)];
}

// =====================================================================================================================
// Sets the compression related fields in a srd for imageview
static void SetImageSrdCompression(
    const IDevice*        pDevice,
    const Image&          gfx12Image,
    uint32                plane,
    CompressionMode       compressionMode,
    sq_img_rsrc_t*        pSrd)
{
    switch (compressionMode)
    {
    case CompressionMode::Default:
    case CompressionMode::ReadEnableWriteEnable:
        pSrd->compression_en        = 1;
        pSrd->write_compress_enable = 1;
        break;
    case CompressionMode::ReadEnableWriteDisable:
        pSrd->compression_en        = 1;
        pSrd->write_compress_enable = 0;
        break;
    case CompressionMode::ReadBypassWriteDisable:
        pSrd->write_compress_enable = 0;
        if (GetGfx12Settings(static_cast<const Pal::Device*>(pDevice)).enableCompressionReadBypass)
        {
            pSrd->compression_en    = 0;
        }
        else
        {
            pSrd->compression_en    = 1;
        }
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }
    pSrd->max_compressed_block_size   = gfx12Image.GetMaxCompressedSize(plane);
    pSrd->max_uncompressed_block_size = gfx12Image.GetMaxUncompressedSize(plane);
    pSrd->compression_access_mode = 0;
}

// =====================================================================================================================
void PAL_STDCALL Device::CreateTypedBufferViewSrds(
    const IDevice*        pDevice,
    uint32                count,
    const BufferViewInfo* pViewInfo,
    void*                 pOut)
{
    using namespace Formats::Gfx12;

    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pViewInfo != nullptr) && (count > 0));

    const Gfx12PalSettings& gfx12Settings = GetGfx12Settings(static_cast<const Pal::Device*>(pDevice));
    sq_buf_rsrc_t* pOutSrd = static_cast<sq_buf_rsrc_t*>(pOut);

    for (uint32 i = 0; i < count; i++)
    {
        PAL_ASSERT(IsValidTypedBufferView(*pViewInfo));

        pOutSrd->u32All[0] = LowPart(pViewInfo->gpuAddr);
        pOutSrd->u32All[1] = HighPart(pViewInfo->gpuAddr) |
                             (static_cast<uint32>(pViewInfo->stride) << SqBufRsrcTWord1StrideShift);

        //    We can simplify NUM_RECORDS to be:
        //    Bytes if: Buffer SRD is for raw buffer access (which we define as Undefined format and stride of 1).
        //    Otherwise, in units of "stride".
        // Which can be simplified to divide by stride if the stride is greater than 1
        pOutSrd->u32All[2] = (pViewInfo->stride > 1) ? pViewInfo->range / pViewInfo->stride : pViewInfo->range;

        const SQ_SEL_XYZW01 SqSelX = HwSwizzle(pViewInfo->swizzledFormat.swizzle.r);
        const SQ_SEL_XYZW01 SqSelY = HwSwizzle(pViewInfo->swizzledFormat.swizzle.g);
        const SQ_SEL_XYZW01 SqSelZ = HwSwizzle(pViewInfo->swizzledFormat.swizzle.b);
        const SQ_SEL_XYZW01 SqSelW = HwSwizzle(pViewInfo->swizzledFormat.swizzle.a);

        // Get the HW format enumeration corresponding to the view-specified format.
        const BUF_FMT hwBufFmt = HwBufFmt(pViewInfo->swizzledFormat.format);

        // If we get an invalid format in the buffer SRD, then the memory operation involving this SRD will be dropped
        PAL_ASSERT(hwBufFmt != BUF_FMT_INVALID);
        pOutSrd->u32All[3] = (SqSelX            << SqBufRsrcTWord3DstSelXShift)    |
                             (SqSelY            << SqBufRsrcTWord3DstSelYShift)    |
                             (SqSelZ            << SqBufRsrcTWord3DstSelZShift)    |
                             (SqSelW            << SqBufRsrcTWord3DstSelWShift)    |
                             (hwBufFmt          << SqBufRsrcTWord3FormatShift)     |
                             (SQ_OOB_INDEX_ONLY << SqBufRsrcTWord3OobSelectShift)  |
                             (SQ_RSRC_BUF       << SqBufRsrcTWord3TypeShift)       |
                             GetBufferSrdCompressionBits(gfx12Settings, pViewInfo->compressionMode);

        pOutSrd++;
        pViewInfo++;
    }
}

// =====================================================================================================================
void PAL_STDCALL Device::CreateUntypedBufferViewSrds(
    const IDevice*        pDevice,
    uint32                count,
    const BufferViewInfo* pViewInfo,
    void*                 pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pViewInfo != nullptr) && (count > 0));

    const Gfx12PalSettings& gfx12Settings = GetGfx12Settings(static_cast<const Pal::Device*>(pDevice));
    sq_buf_rsrc_t* pOutSrd = static_cast<sq_buf_rsrc_t*>(pOut);

    for (uint32 i = 0; i < count; i++)
    {
        PAL_ASSERT((pViewInfo->gpuAddr != 0) || (pViewInfo->range == 0));
        PAL_ASSERT(Formats::IsUndefined(pViewInfo->swizzledFormat.format));

        pOutSrd->u32All[0] = LowPart(pViewInfo->gpuAddr);
        pOutSrd->u32All[1] = HighPart(pViewInfo->gpuAddr) |
                             (static_cast<uint32>(pViewInfo->stride) << SqBufRsrcTWord1StrideShift);

        //    We can simplify NUM_RECORDS to be:
        //    Bytes if: Buffer SRD is for raw buffer access (which we define as Undefined format and stride of 1).
        //    Otherwise, in units of "stride".
        // Which can be simplified to divide by stride if the stride is greater than 1
        pOutSrd->u32All[2] = (pViewInfo->stride > 1) ? (pViewInfo->range / pViewInfo->stride) : pViewInfo->range;

        if (pViewInfo->gpuAddr != 0)
        {
            const uint32 oobSelect  = (pViewInfo->stride <= 1) ? SQ_OOB_COMPLETE : SQ_OOB_INDEX_ONLY;

            pOutSrd->u32All[3] = (SQ_SEL_X        << SqBufRsrcTWord3DstSelXShift)    |
                                 (SQ_SEL_Y        << SqBufRsrcTWord3DstSelYShift)    |
                                 (SQ_SEL_Z        << SqBufRsrcTWord3DstSelZShift)    |
                                 (SQ_SEL_W        << SqBufRsrcTWord3DstSelWShift)    |
                                 (BUF_FMT_32_UINT << SqBufRsrcTWord3FormatShift)     |
                                 (oobSelect       << SqBufRsrcTWord3OobSelectShift)  |
                                 (SQ_RSRC_BUF     << SqBufRsrcTWord3TypeShift)       |
                                 GetBufferSrdCompressionBits(gfx12Settings, pViewInfo->compressionMode);
        }
        else
        {
            pOutSrd->u32All[3] = 0;
        }

        pOutSrd++;
        pViewInfo++;
    }
}

// =====================================================================================================================
// Computes the image view SRD DEPTH field based on image view parameters
static uint32 ComputeImageViewDepth(
    const ImageViewInfo&   viewInfo,
    const ImageInfo&       imageInfo,
    const SubResourceInfo& subresInfo)
{
    const ImageCreateInfo& imageCreateInfo = viewInfo.pImage->GetImageCreateInfo();

    uint32 depth = 0;

    // From reg spec: Units are "depth - 1", so 0 = 1 slice, 1= 2 slices. If the image type is 3D, then the DEPTH field
    // is the image's depth - 1. Otherwise, the DEPTH field replaces the old "last_array" field.

    // Note that we can't use viewInfo.viewType here since 3D image may be viewed as 2D (array).
    if (imageCreateInfo.imageType == ImageType::Tex3d)
    {
        if (viewInfo.flags.zRangeValid)
        {
            // If the client is specifying a valid Z range, the depth of the SRD must include the range's offset and
            // extent. Furthermore, the Z range is specified in terms of the view's first mip level, not the Image's
            // base mip level. Since it is UAV, the hardware accepts depth in the current mip level.
            depth = (viewInfo.zRange.offset + viewInfo.zRange.extent) - 1;
        }
        else
        {
            depth = subresInfo.extentTexels.depth - 1;
        }
    }
    else
    {
        depth = (viewInfo.subresRange.startSubres.arraySlice + viewInfo.subresRange.numSlices - 1);
    }

    return depth;
}

// =====================================================================================================================
// Returns the value for SQ_IMG_RSRC_WORD4.BC_SWIZZLE
static TEX_BC_SWIZZLE GetBcSwizzle(
    const SwizzledFormat& swizzledFormat)
{
    // GFX9+ applies image view swizzle to border color in hardware.  The only thing we have to do is to apply swizzle
    // to border color, which is specified as image format swizzle relative to RGBA format e.g. RAGB image format has
    // a swizzle of XWYZ relative to RGBA.
    const ChannelMapping& swizzle   = swizzledFormat.swizzle;
    TEX_BC_SWIZZLE        bcSwizzle = TEX_BC_Swizzle_XYZW;

    const uint32 numComponents = Formats::NumComponents(swizzledFormat.format);

    // If the format has 3 or 4 components there is only one possible combination that matches
    if (numComponents >= 3)
    {
        if ((swizzle.r == ChannelSwizzle::X) &&
            (swizzle.g == ChannelSwizzle::Y) &&
            (swizzle.b == ChannelSwizzle::Z))
        {
            // RGBA
            bcSwizzle = TEX_BC_Swizzle_XYZW;
        }
        else if ((swizzle.r == ChannelSwizzle::X) &&
                 (swizzle.a == ChannelSwizzle::Y) &&
                 (swizzle.g == ChannelSwizzle::Z))
        {
            // RAGB
            bcSwizzle = TEX_BC_Swizzle_XWYZ;
        }
        else if ((swizzle.a == ChannelSwizzle::X) &&
                 (swizzle.b == ChannelSwizzle::Y) &&
                 (swizzle.g == ChannelSwizzle::Z))
        {
            // ABGR
            bcSwizzle = TEX_BC_Swizzle_WZYX;
        }
        else if ((swizzle.a == ChannelSwizzle::X) &&
                 (swizzle.r == ChannelSwizzle::Y) &&
                 (swizzle.g == ChannelSwizzle::Z))
        {
            // ARGB
            bcSwizzle = TEX_BC_Swizzle_WXYZ;
        }
        else if ((swizzle.b == ChannelSwizzle::X) &&
                 (swizzle.g == ChannelSwizzle::Y) &&
                 (swizzle.r == ChannelSwizzle::Z))
        {
            // BGRA
            bcSwizzle = TEX_BC_Swizzle_ZYXW;
        }
        else if ((swizzle.g == ChannelSwizzle::X) &&
                 (swizzle.r == ChannelSwizzle::Y) &&
                 (swizzle.a == ChannelSwizzle::Z))
        {
            // GRAB
            bcSwizzle = TEX_BC_Swizzle_YXWZ;
        }
    }
    // If the format has 2 components we have to match them and the remaining 2 can be in any order
    else if (numComponents == 2)
    {
        if ((swizzle.r == ChannelSwizzle::X) &&
            (swizzle.g == ChannelSwizzle::Y))
        {
            // RGBA
            bcSwizzle = TEX_BC_Swizzle_XYZW;
        }
        else if ((swizzle.r == ChannelSwizzle::X) &&
                 (swizzle.a == ChannelSwizzle::Y))
        {
            // RAGB
            bcSwizzle = TEX_BC_Swizzle_XWYZ;
        }
        else if ((swizzle.a == ChannelSwizzle::X) &&
                 (swizzle.b == ChannelSwizzle::Y))
        {
            // ABGR
            bcSwizzle = TEX_BC_Swizzle_WZYX;
        }
        else if ((swizzle.a == ChannelSwizzle::X) &&
                 (swizzle.r == ChannelSwizzle::Y))
        {
            // ARGB
            bcSwizzle = TEX_BC_Swizzle_WXYZ;
        }
        else if ((swizzle.b == ChannelSwizzle::X) &&
                 (swizzle.g == ChannelSwizzle::Y))
        {
            // BGRA
            bcSwizzle = TEX_BC_Swizzle_ZYXW;
        }
        else if ((swizzle.g == ChannelSwizzle::X) &&
                 (swizzle.r == ChannelSwizzle::Y))
        {
            // GRAB
            bcSwizzle = TEX_BC_Swizzle_YXWZ;
        }
    }
    // If the format has 1 component we have to match it and the remaining 3 can be in any order
    else
    {
        if (swizzle.r == ChannelSwizzle::X)
        {
            // RGBA or RAGB
            bcSwizzle = TEX_BC_Swizzle_XYZW;
        }
        else if (swizzle.g == ChannelSwizzle::X)
        {
            // GRAB
            bcSwizzle = TEX_BC_Swizzle_YXWZ;
        }
        else if (swizzle.b == ChannelSwizzle::X)
        {
            // BGRA
            bcSwizzle = TEX_BC_Swizzle_ZYXW;
        }
        else if (swizzle.a == ChannelSwizzle::X)
        {
            // ABGR or ARGB
            bcSwizzle = TEX_BC_Swizzle_WXYZ;
        }
    }

    return bcSwizzle;
}

// =====================================================================================================================
// Update the supplied SRD to instead reflect certain parameters that are different between the "map" image and its
// parent image.
static void UpdateLinkedResourceViewSrd(
    const Pal::Image*    pParentImage, // Can be NULL for read access type
    const Image&         mapImage,
    SubresId             subresId,
    const ImageViewInfo& viewInfo,
    sq_img_rsrc_t*       pSrd)
{
    const auto&                mapCreateInfo = mapImage.Parent()->GetImageCreateInfo();
    sq_img_rsrc_linked_rsrc_t* pLinkedRsrc   = reinterpret_cast<sq_img_rsrc_linked_rsrc_t*>(pSrd);
    const PrtMapAccessType     accessType    = viewInfo.mapAccess;

    // Without this, the other fields setup here have very different meanings.
    pLinkedRsrc->linked_resource = 1;

    // Sanity check that our sq_img_rsrc_linked_rsrc_t and sq_img_rsrc_t definitions line up.
    PAL_ASSERT(pSrd->linked_resource == 1);

    // "linked_resource_type" lines up with the "bc_swizzle" field of the sq_img_rsrc_t structure.
    // There are no enums for these values
    if (mapCreateInfo.prtPlus.mapType == PrtMapType::Residency)
    {
        switch (accessType)
        {
        case PrtMapAccessType::Read:
            pLinkedRsrc->linked_resource_type = 4;
            break;
        case PrtMapAccessType::WriteMin:
            pLinkedRsrc->linked_resource_type = 2;
            break;
        case PrtMapAccessType::WriteMax:
            pLinkedRsrc->linked_resource_type = 3;
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }
    }
    else
    {
        PAL_ASSERT(mapCreateInfo.prtPlus.mapType == PrtMapType::SamplingStatus);
        pLinkedRsrc->linked_resource_type = 1;
    }

    if (pParentImage != nullptr)
    {
        const auto*  pPalDevice       = pParentImage->GetDevice();
        const auto&  parentCreateInfo = pParentImage->GetImageCreateInfo();
        const auto&  parentExtent     = parentCreateInfo.extent;
        const auto&  mapExtent        = mapCreateInfo.extent;
        const auto*  pGfx12Device     = static_cast<Pal::Gfx12::Device*>(pPalDevice->GetGfxDevice());
        const Image& image            = static_cast<const Image&>(*(pParentImage->GetGfxImage()));

        // The "max_mip" field reflects the number of mip levels in the map image
        pLinkedRsrc->max_mip = mapCreateInfo.mipLevels - 1;

        // "xxx_scale" lines up with the "min_lod_warn" field of the sq_img_rsrc_t structure.
        pLinkedRsrc->width_scale  = Log2(parentExtent.width  / mapExtent.width);
        pLinkedRsrc->height_scale = Log2(parentExtent.height / mapExtent.height);
        pLinkedRsrc->depth_scale  = Log2(parentExtent.depth  / mapExtent.depth);

        // Most importantly, the base address points to the map image, not the parent image.
        pLinkedRsrc->base_address = mapImage.GetSubresource256BAddr(subresId);

        // As the linked resource image's memory is the one that is actually being accessed, the swizzle
        // mode needs to reflect that image, not the parent.
        pLinkedRsrc->sw_mode = mapImage.GetFinalSwizzleMode(subresId);

        // Set the compression_en bit according to the parent image's compression setting.
        CompressionMode finalCompressionMode =
            static_cast<CompressionMode>(GetGfx12Settings(pPalDevice).imageViewCompressionMode);
        if (finalCompressionMode == CompressionMode::Default)
        {
            auto* pParentMemory  = pParentImage->GetBoundGpuMemory().Memory();
            finalCompressionMode = pGfx12Device->GetImageViewCompressionMode(viewInfo.compressionMode,
                                                                             parentCreateInfo.compressionMode,
                                                                             pParentMemory);
        }
        SetImageSrdCompression(pPalDevice, image, subresId.plane, finalCompressionMode, pSrd);
    }
}

// =====================================================================================================================
void PAL_STDCALL Device::CreateImageViewSrds(
    const IDevice*       pDevice,
    uint32               count,
    const ImageViewInfo* pImgViewInfo,
    void*                pOut)
{
    using namespace Formats::Gfx12;

    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pImgViewInfo != nullptr) && (count > 0));
    const auto*const pPalDevice   = static_cast<const Pal::Device*>(pDevice);
    const auto*      pGfx12Device = static_cast<Pal::Gfx12::Device*>(pPalDevice->GetGfxDevice());
    const auto*      pAddrMgr     = static_cast<const AddrMgr3::AddrMgr3*>(pPalDevice->GetAddrMgr());

    sq_img_rsrc_t* pSrds = static_cast<sq_img_rsrc_t*>(pOut);

    for (uint32 i = 0; i < count; ++i)
    {
        const ImageViewInfo& viewInfo = pImgViewInfo[i];
        PAL_ASSERT(viewInfo.subresRange.numPlanes == 1);

        // If the "image" is really a PRT+ mapping image, then we want to set up the majority of this
        // SRD off of the parent image, unless the client is indicating they want raw access to the
        // map image.
        const auto*const       pParent         = ((viewInfo.mapAccess == PrtMapAccessType::Raw)
                                                  ? static_cast<const Pal::Image*>(viewInfo.pImage)
                                                  : static_cast<const Pal::Image*>(viewInfo.pPrtParentImg));
        const Image&           image           = static_cast<const Image&>(*(pParent->GetGfxImage()));
        const ImageInfo&       imageInfo       = pParent->GetImageInfo();
        const ImageCreateInfo& imageCreateInfo = pParent->GetImageCreateInfo();
        const bool             imgIsYuvPlanar  = Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format);
        sq_img_rsrc_t          srd             = { };
        ChNumFormat            format          = viewInfo.swizzledFormat.format;

        SubresId baseSubResId   = { viewInfo.subresRange.startSubres.plane, 0, 0 };
        uint32   baseArraySlice = viewInfo.subresRange.startSubres.arraySlice;
        uint32   firstMipLevel  = viewInfo.subresRange.startSubres.mipLevel;
        uint32   mipLevels      = imageCreateInfo.mipLevels;

        PAL_ASSERT((viewInfo.possibleLayouts.engines != 0) && (viewInfo.possibleLayouts.usages != 0));

        if ((viewInfo.flags.zRangeValid == 1) && (imageCreateInfo.imageType == ImageType::Tex3d))
        {
            baseArraySlice = viewInfo.zRange.offset;
            srd.uav3d = 1;
        }
        else if (imgIsYuvPlanar && (viewInfo.subresRange.numSlices == 1))
        {
            baseSubResId.arraySlice = baseArraySlice;
            baseArraySlice = 0;
        }

        bool overrideBaseResource = false;
        bool overrideZRangeOffset = false;
        bool viewMipAsFullTexture = false;
        bool includePadding       = (viewInfo.flags.includePadding != 0);

        // Validate subresource ranges
        const SubResourceInfo* pBaseSubResInfo = pParent->SubresourceInfo(baseSubResId);

        Extent3d extent       = pBaseSubResInfo->extentTexels;
        Extent3d actualExtent = pBaseSubResInfo->actualExtentTexels;

        // The view should be in terms of texels except in four special cases when we're operating in terms of elements:
        // 1. Viewing a compressed image in terms of blocks. For BC images elements are blocks, so if the caller gave
        //    us an uncompressed view format we assume they want to view blocks.
        // 2. Copying to an "expanded" format (e.g., R32G32B32). In this case we can't do native format writes so we're
        //    going to write each element independently. The trigger for this case is a mismatched bpp.
        // 3. Viewing a YUV-packed image with a non-YUV-packed format when the view format is allowed for view formats
        //    with twice the bpp. In this case, the effective width of the view is half that of the base image.
        // 4. Viewing a YUV-planar Image which has multiple array slices. In this case, the texture hardware has no way
        //    to know about the padding in between array slices of the same plane (due to the other plane's slices being
        //    interleaved). In this case, we pad out the actual height of the view to span all planes (so that the view
        //    can access each array slice).
        //    This has the unfortunate side-effect of making normalized texture coordinates inaccurate.
        //    However, this is required for access to multiple slices.
        const bool imgIsBc = Formats::IsBlockCompressed(imageCreateInfo.swizzledFormat.format);
        if (imgIsBc && (Formats::IsBlockCompressed(format) == false))
        {
            // If we have the following image:
            //              Uncompressed pixels   Compressed block sizes (4x4)
            //      mip0:       22 x 22                   6 x 6
            //      mip1:       11 x 11                   3 x 3
            //      mip2:        5 x  5                   2 x 2
            //      mip3:        2 x  2                   1 x 1
            //      mip4:        1 x  1                   1 x 1
            //
            // On GFX10 the SRD is always programmed with the WIDTH and HEIGHT of the base level and the HW is
            // calculating the degradation of the block sizes down the mip-chain as follows (straight-up
            // divide-by-two integer math):
            //      mip0:  6x6
            //      mip1:  3x3
            //      mip2:  1x1
            //      mip3:  1x1
            //
            // This means that mip2 will be missing texels.

            if (viewInfo.subresRange.numMips <= 1)
            {
                // Fix this by calculating the start mip's ceil(texels/blocks) width and height and then go up the
                // chain to pad the base mip's width and height to account for this.  A result lower than the base
                // mip's indicates a non-power-of-two texture, and the result should be clamped to its extentElements.
                // Otherwise, if the mip is aligned to block multiples, the result will be equal to extentElements.
                // If there is no suitable width or height, the actualExtentElements is chosen.  The application is in
                // charge of making sure the math works out properly if they do this (allowed by Vulkan), otherwise we
                // assume it's an internal view and the copy shaders will prevent accessing out-of-bounds pixels.

                const SubresId         mipSubResId =
                    Subres(viewInfo.subresRange.startSubres.plane, firstMipLevel, baseArraySlice);
                const SubResourceInfo* pMipSubResInfo = pParent->SubresourceInfo(mipSubResId);

                extent.width  = Clamp((pMipSubResInfo->extentElements.width  << firstMipLevel),
                                      pBaseSubResInfo->extentElements.width,
                                      pBaseSubResInfo->actualExtentElements.width);
                extent.height = Clamp((pMipSubResInfo->extentElements.height << firstMipLevel),
                                      pBaseSubResInfo->extentElements.height,
                                      pBaseSubResInfo->actualExtentElements.height);

                // Only 2D images and 3D thin images (view3dAs2dArray == 1) support nonBlockCompressedViews.
                const bool isNonBcViewCompatible = (imageCreateInfo.imageType == ImageType::Tex2d) ||
                    ((imageCreateInfo.imageType == ImageType::Tex3d) && imageCreateInfo.flags.view3dAs2dArray);

                if (isNonBcViewCompatible                                                            &&
                    (viewInfo.subresRange.numSlices == 1)                                            &&
                    ((Max(1u, extent.width >> firstMipLevel) < pMipSubResInfo->extentElements.width) ||
                    (Max(1u, extent.height >> firstMipLevel) < pMipSubResInfo->extentElements.height)))

                {
                    srd.base_address = image.ComputeNonBlockCompressedView(pBaseSubResInfo,
                                                                           pMipSubResInfo,
                                                                           &mipLevels,
                                                                           &firstMipLevel,
                                                                           &extent);
                    baseArraySlice = 0;
                    viewMipAsFullTexture = true;
                }

                actualExtent = pBaseSubResInfo->actualExtentElements;
            }
            else
            {
                // Set no_edge_clamp to avoid missing texels problem for multi-mip views.
                srd.no_edge_clamp = 1;

                // It would appear that HW needs the actual extents to calculate the mip addresses correctly when
                // viewing more than 1 mip especially in the case of non power of two textures.
                includePadding = true;
            }
        }
        else if ((pBaseSubResInfo->bitsPerTexel != Formats::BitsPerPixel(format)) &&
                 // For PRT+ map images, the format of the view is expected to be different
                 // from the format of the image itself.  Don't adjust the extents for PRT+ map images!
                 (viewInfo.pImage->GetImageCreateInfo().prtPlus.mapType == PrtMapType::None))
        {
            if (Formats::IsMacroPixelPacked(imageCreateInfo.swizzledFormat.format))
            {
                // YUV422 formats use 32bpp memory addressing instead of 16bpp. The HW scales the SRD width and
                // x-coordinate accordingly for these formats.
                extent.width       /= 2;
                actualExtent.width /= 2;
            }
            else
            {
                extent       = pBaseSubResInfo->extentElements;
                actualExtent = pBaseSubResInfo->actualExtentElements;

                // For 96 bit bpp formats(X32Y32Z32_Uint/X32Y32Z32_Sint/X32Y32Z32_Float), X32_Uint formated image view
                // srd might be created upon the image for image copy operation. Extent of mipmaped level of X32_Uint
                // and mipmaped level of the original X32Y32Z32_* format might mismatch, especially on the last several
                // mips. Thus, it could be problematic to use 256b address of zero-th mip + mip level mode. Instead we
                // shall adopt 256b address of startsubres's miplevel/arrayLevel.
                if (pBaseSubResInfo->bitsPerTexel == 96)
                {
                    PAL_ASSERT(viewInfo.subresRange.numMips == 1);
                    mipLevels             = 1;
                    baseSubResId.mipLevel = firstMipLevel;
                    firstMipLevel         = 0;

                    // For gfx10 the baseSubResId should point to the baseArraySlice instead of setting the base_array
                    // SRD. When baseSubResId is used to calculate the baseAddress value, the current array slice will
                    // will be included in the equation.
                    PAL_ASSERT(viewInfo.subresRange.numSlices == 1);

                    // For gfx10 3d texture, we need to access per z slice instead subresource.
                    // Z slices are interleaved for mipmapped 3d texture. (each DepthPitch contains all the miplevels)
                    // example: the memory layout for a 3 miplevel WxHxD 3d texture:
                    // baseAddress(mip2) + DepthPitch * 0: subresource(mip2)'s 0 slice
                    // baseAddress(mip1) + DepthPitch * 0: subresource(mip1)'s 0 slice
                    // baseAddress(mip0) + DepthPitch * 0: subresource(mip0)'s 0 slice
                    // baseAddress(mip2) + DepthPitch * 1: subresource(mip2)'s 1 slice
                    // baseAddress(mip1) + DepthPitch * 1: subresource(mip1)'s 1 slice
                    // baseAddress(mip0) + DepthPitch * 1: subresource(mip0)'s 1 slice
                    // ...
                    // baseAddress(mip2) + DepthPitch * (D-1): subresource(mip2)'s D-1 slice
                    // baseAddress(mip1) + DepthPitch * (D-1): subresource(mip1)'s D-1 slice
                    // baseAddress(mip0) + DepthPitch * (D-1): subresource(mip0)'s D-1 slice
                    // When we try to view each subresource as 1 miplevel, we can't use srd.word5.bits.BASE_ARRAY to
                    // access each z slices since the srd for hardware can't compute the correct z slice stride.
                    // Instead we need a view to each slice.
                    if (imageCreateInfo.imageType == ImageType::Tex3d)
                    {
                        PAL_ASSERT((viewInfo.flags.zRangeValid == 1) && (viewInfo.zRange.extent == 1));
                        PAL_ASSERT(image.IsSubResourceLinear(baseSubResId));

                        baseSubResId.arraySlice = 0;
                        overrideZRangeOffset    = viewInfo.flags.zRangeValid;
                    }
                    else
                    {
                        baseSubResId.arraySlice = baseArraySlice;
                    }

                    baseArraySlice       = 0;
                    overrideBaseResource = true;

                    pBaseSubResInfo = pParent->SubresourceInfo(baseSubResId);
                    extent          = pBaseSubResInfo->extentElements;
                    actualExtent    = pBaseSubResInfo->actualExtentElements;
                }
            }

            // When there is mismatched bpp and more than 1 mipLevels, it's possible to have missing texels like it
            // is to block compressed format. To compensate that, we set includePadding to true.
            if (imageCreateInfo.mipLevels > 1)
            {
                includePadding = true;
            }
        }
        else if (Formats::IsYuvPacked(pBaseSubResInfo->format.format) &&
                 (Formats::IsYuvPacked(format) == false)              &&
                 ((pBaseSubResInfo->bitsPerTexel << 1) == Formats::BitsPerPixel(format)))
        {
            // Changing how we interpret the bits-per-pixel of the subresource wreaks havoc with any tile swizzle
            // pattern used. This will only work for linear-tiled Images.
            PAL_ASSERT(image.IsSubResourceLinear(baseSubResId));

            extent.width       >>= 1;
            actualExtent.width >>= 1;
        }
        else if (Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format))
        {
            if (viewInfo.subresRange.numSlices > 1)
            {
                image.PadYuvPlanarViewActualExtent(baseSubResId, &actualExtent);

                includePadding = true;
                // Sampling using this view will not work correctly, but direct image loads will work.
                // This path is only expected to be used by RPM operations.
                PAL_ALERT_ALWAYS();
            }
            else
            {
                // We must use base slice 0 for correct normalized coordinates on a YUV planar surface.
                PAL_ASSERT(baseArraySlice == 0);
            }
        }
        else if (Formats::IsMacroPixelPackedRgbOnly(imageCreateInfo.swizzledFormat.format) &&
                 (Formats::IsMacroPixelPackedRgbOnly(format) == false) &&
                 (imageCreateInfo.mipLevels > 1))
        {
            // If we have view format as X16 for MacroPixelPackedRgbOnly format.
            // We need a padding view for width need to be padding to even.
            //      mip0:  100x800
            //      mip1:  50x400
            //      mip2:  26x200
            //      mip3:  12x100
            //      mip4:  6x50
            //      mip5:  4x25
            //      mip6:  2x12
            //      mip7:  2x6
            //      mip8:  2x3
            //      mip9:  2x1   (may missing pixel if actual base extent.width < 2**10)
            // Set no_edge_clamp to avoid missing pixel copy problem
            srd.no_edge_clamp = 1;
            includePadding = true;
        }

        // MIN_LOD field is u5.8
        constexpr uint32 Gfx12MinLodIntBits  = 5;
        constexpr uint32 Gfx12MinLodFracBits = 8;
        const     uint32 minLod              = Math::FloatToUFixed(viewInfo.minLod,
                                                                   Gfx12MinLodIntBits,
                                                                   Gfx12MinLodFracBits,
                                                                   true);

        srd.min_lod_lo = minLod & ((1 << 6) - 1);
        srd.min_lod_hi = minLod >> 6;
        srd.format = Formats::Gfx12::HwImgFmt(format);

        // GFX10 does not support native 24-bit surfaces...  Clients promote 24-bit depth surfaces to 32-bit depth on
        // image creation.  However, they can request that border color data be clamped appropriately for the original
        // 24-bit depth.  Don't check for explicit depth surfaces here, as that only pertains to bound depth surfaces,
        // not to purely texture surfaces.
        //
        if ((imageCreateInfo.usageFlags.depthAsZ24 != 0) && (Formats::ShareChFmt(format, ChNumFormat::X32_Uint)))
        {
            // This special format indicates to HW that this is a promoted 24-bit surface, so sample_c and border color
            // can be treated differently.
            srd.format = IMG_FMT_32_FLOAT_CLAMP;
        }

        const Extent3d   programmedExtent = includePadding ? actualExtent : extent;
        constexpr uint32 WidthLowSize     = 2;

        srd.width_lo = (programmedExtent.width - 1) & ((1 << WidthLowSize) - 1);
        srd.width_hi = (programmedExtent.width - 1) >> WidthLowSize;
        srd.height   = (programmedExtent.height - 1);

        // Setup CCC filtering optimizations: GCN uses a simple scheme which relies solely on the optimization
        // setting from the CCC rather than checking the render target resolution.
        static_assert(TextureFilterOptimizationsDisabled   == 0, "TextureOptLevel lookup table mismatch");
        static_assert(TextureFilterOptimizationsEnabled    == 1, "TextureOptLevel lookup table mismatch");
        static_assert(TextureFilterOptimizationsAggressive == 2, "TextureOptLevel lookup table mismatch");

        constexpr TexPerfModulation PanelToTexPerfMod[] =
        {
            TexPerfModulation::None,     // TextureFilterOptimizationsDisabled
            TexPerfModulation::Default,  // TextureFilterOptimizationsEnabled
            TexPerfModulation::Max       // TextureFilterOptimizationsAggressive
        };

        PAL_ASSERT(viewInfo.texOptLevel < ImageTexOptLevel::Count);

        uint32 texOptLevel;
        switch (viewInfo.texOptLevel)
        {
        case ImageTexOptLevel::Disabled:
            texOptLevel = TextureFilterOptimizationsDisabled;
            break;
        case ImageTexOptLevel::Enabled:
            texOptLevel = TextureFilterOptimizationsEnabled;
            break;
        case ImageTexOptLevel::Maximum:
            texOptLevel = TextureFilterOptimizationsAggressive;
            break;
        case ImageTexOptLevel::Default:
        default:
            texOptLevel = static_cast<const Pal::Device*>(pDevice)->Settings().tfq;
            break;
        }

        PAL_ASSERT(texOptLevel < ArrayLen(PanelToTexPerfMod));

        TexPerfModulation perfMod = PanelToTexPerfMod[texOptLevel];

        srd.perf_mod = static_cast<uint32>(perfMod);

        // Destination swizzles come from the view creation info, rather than the format of the view.
        srd.dst_sel_x = HwSwizzle(viewInfo.swizzledFormat.swizzle.r);
        srd.dst_sel_y = HwSwizzle(viewInfo.swizzledFormat.swizzle.g);
        srd.dst_sel_z = HwSwizzle(viewInfo.swizzledFormat.swizzle.b);
        srd.dst_sel_w = HwSwizzle(viewInfo.swizzledFormat.swizzle.a);

        // When view3dAs2dArray is enabled for 3d image, we'll use the same mode for writing and viewing
        // according to the doc, so we don't need to change it here.
        srd.sw_mode = image.GetHwSwizzleMode(pBaseSubResInfo);

        const bool isMultiSampled = (imageCreateInfo.samples > 1);

        // NOTE: Where possible, we always assume an array view type because we don't know how the shader will
        // attempt to access the resource.
        switch (viewInfo.viewType)
        {
        case ImageViewType::Tex1d:
            srd.type = ((imageCreateInfo.arraySize == 1) ? SQ_RSRC_IMG_1D : SQ_RSRC_IMG_1D_ARRAY);
            break;
        case ImageViewType::Tex2d:
            // A 3D image with view3dAs2dArray enabled can be accessed via 2D image view too, it needs 2D_ARRAY type.
            srd.type = (((imageCreateInfo.arraySize == 1) && (imageCreateInfo.imageType != ImageType::Tex3d))
                        ? (isMultiSampled ? SQ_RSRC_IMG_2D_MSAA       : SQ_RSRC_IMG_2D)
                        : (isMultiSampled ? SQ_RSRC_IMG_2D_MSAA_ARRAY : SQ_RSRC_IMG_2D_ARRAY));
            break;
        case ImageViewType::Tex3d:
            srd.type = SQ_RSRC_IMG_3D;
            break;
        case ImageViewType::TexCube:
            srd.type = SQ_RSRC_IMG_CUBE;
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }

        uint32 maxMipField = 0;
        if (isMultiSampled)
        {
            // MSAA textures cannot be mipmapped; the LAST_LEVEL and MAX_MIP fields indicate the texture's
            // sample count.  According to the docs, these are samples.  According to reality, this is
            // fragments.  I'm going with reality.
            srd.base_level = 0;
            srd.last_level = Log2(imageCreateInfo.fragments);
            maxMipField    = Log2(imageCreateInfo.fragments);
        }
        else
        {
            srd.base_level = firstMipLevel;
            srd.last_level = firstMipLevel + viewInfo.subresRange.numMips - 1;
            maxMipField    = mipLevels - 1;
        }

        srd.max_mip = maxMipField;

        // For 1D, 2D and MSAA resources, if pitch > width, depth and pitch_msb are used to report pitch
        const uint32  bytesPerPixel = Formats::BytesPerPixel(format);
        const uint32  pitchInPixels = imageCreateInfo.rowPitch / bytesPerPixel;
        if ((pitchInPixels > extent.width) &&
            (viewMipAsFullTexture == false) &&
            ((srd.type == SQ_RSRC_IMG_1D) ||
             (srd.type == SQ_RSRC_IMG_2D) ||
             (srd.type == SQ_RSRC_IMG_2D_MSAA)))
        {
            srd.depth     = pitchInPixels - 1;
            srd.pitch_msb = (pitchInPixels - 1) >> 14;
        }
        else
        {
            srd.depth   = ComputeImageViewDepth(viewInfo, imageInfo, *pBaseSubResInfo);
        }

        srd.bc_swizzle = GetBcSwizzle(imageCreateInfo.swizzledFormat);

        constexpr uint32 BaseArrayLowSize = 13;

        srd.base_array        = baseArraySlice & ((1 << BaseArrayLowSize) - 1);
        srd.base_array_msb    = baseArraySlice >> BaseArrayLowSize;
        srd.corner_samples    = imageCreateInfo.usageFlags.cornerSampling;

        if (pParent->GetBoundGpuMemory().IsBound())
        {
            // When overrideBaseResource = true (96bpp images), compute baseAddress using the mip/slice in
            // baseSubResId.
            if ((imgIsYuvPlanar && (viewInfo.subresRange.numSlices == 1)) || overrideBaseResource)
            {
                const gpusize gpuVirtAddress = pParent->GetSubresourceBaseAddr(baseSubResId);
                const auto*   pTileInfo      = AddrMgr3::GetTileInfo(pParent, baseSubResId);
                const gpusize pipeBankXor    = pTileInfo->pipeBankXor;
                gpusize       addrWithXor    = gpuVirtAddress | (pipeBankXor << 8);

                if (overrideZRangeOffset)
                {
                    addrWithXor += viewInfo.zRange.offset * pBaseSubResInfo->depthPitch;
                }

                srd.base_address = addrWithXor >> 8;
            }
            else if (srd.base_address == 0)
            {
                srd.base_address = image.GetSubresource256BAddr(baseSubResId);
            }

            // make sure the compression setting follows both the view and the image compression mode
            static_assert(
                (uint32(CompressionMode::Default)                == ImageViewCompressionDefault)                &&
                (uint32(CompressionMode::ReadEnableWriteEnable)  == ImageViewCompressionReadEnableWriteEnable)  &&
                (uint32(CompressionMode::ReadEnableWriteDisable) == ImageViewCompressionReadEnableWriteDisable));

            CompressionMode finalCompressionMode =
                static_cast<CompressionMode>(GetGfx12Settings(pPalDevice).imageViewCompressionMode);
            if (finalCompressionMode == CompressionMode::Default)
            {
                finalCompressionMode = pGfx12Device->GetImageViewCompressionMode(viewInfo.compressionMode,
                                                                                 imageCreateInfo.compressionMode,
                                                                                 pParent->GetBoundGpuMemory().Memory());
            }
            SetImageSrdCompression(pDevice, image, baseSubResId.plane, finalCompressionMode, &srd);
        }
        // Fill the unused 4 bits of word6 with sample pattern index
        srd.sample_pattern_offset = viewInfo.samplePatternIdx;

        if (viewInfo.mapAccess != PrtMapAccessType::Raw)
        {
            UpdateLinkedResourceViewSrd(static_cast<const Pal::Image*>(viewInfo.pPrtParentImg),
                                        *GetGfx12Image(viewInfo.pImage),
                                        baseSubResId,
                                        viewInfo,
                                        &srd);
        }

        memcpy(&pSrds[i], &srd, sizeof(srd));
    }
}

// =====================================================================================================================
void Device::DisableImageViewSrdEdgeClamp(
    uint32 count,
    void*  pImageSrds
    ) const
{
    sq_img_rsrc_t* pSrds = static_cast<sq_img_rsrc_t*>(pImageSrds);

    for (uint32 i = 0; i < count; ++i)
    {
        pSrds[i].no_edge_clamp = 1;
    }
}

// =====================================================================================================================
// Determine if anisotropic filtering is enabled
static bool IsAnisoEnabled(
    Pal::TexFilter texfilter)
{
    return ((texfilter.magnification == XyFilterAnisotropicPoint)  ||
            (texfilter.magnification == XyFilterAnisotropicLinear) ||
            (texfilter.minification  == XyFilterAnisotropicPoint)  ||
            (texfilter.minification  == XyFilterAnisotropicLinear));
}

// =====================================================================================================================
// Determine the appropriate Anisotropic filtering mode.
// NOTE: For values of anisotropy not natively supported by HW, we clamp to the closest value less than what was
//       requested.
static SQ_TEX_ANISO_RATIO GetAnisoRatio(
    const SamplerInfo& info)
{
    SQ_TEX_ANISO_RATIO anisoRatio = SQ_TEX_ANISO_RATIO_1;

    if (IsAnisoEnabled(info.filter))
    {
        if (info.maxAnisotropy < 2)
        {
            // Nothing to do.
        }
        else if (info.maxAnisotropy < 4)
        {
            anisoRatio = SQ_TEX_ANISO_RATIO_2;
        }
        else if (info.maxAnisotropy < 8)
        {
            anisoRatio = SQ_TEX_ANISO_RATIO_4;
        }
        else if (info.maxAnisotropy < 16)
        {
            anisoRatio = SQ_TEX_ANISO_RATIO_8;
        }
        else if (info.maxAnisotropy == 16)
        {
            anisoRatio = SQ_TEX_ANISO_RATIO_16;
        }
    }

    return anisoRatio;
}

// =====================================================================================================================
// Determine the appropriate SQ clamp mode based on the given TexAddressMode enum value.
static SQ_TEX_CLAMP GetAddressClamp(
    TexAddressMode texAddress)
{
    constexpr SQ_TEX_CLAMP PalTexAddrToHwTbl[] =
    {
        SQ_TEX_WRAP,                    // TexAddressMode::Wrap
        SQ_TEX_MIRROR,                  // TexAddressMode::Mirror
        SQ_TEX_CLAMP_LAST_TEXEL,        // TexAddressMode::Clamp
        SQ_TEX_MIRROR_ONCE_LAST_TEXEL,  // TexAddressMode::MirrorOnce
        SQ_TEX_CLAMP_BORDER,            // TexAddressMode::ClampBorder
        SQ_TEX_MIRROR_ONCE_HALF_BORDER, // TexAddressMode::MirrorClampHalfBorder
        SQ_TEX_CLAMP_HALF_BORDER,       // TexAddressMode::ClampHalfBorder
        SQ_TEX_MIRROR_ONCE_BORDER,      // TexAddressMode::MirrorClampBorder
    };

    static_assert((ArrayLen(PalTexAddrToHwTbl) == static_cast<size_t>(TexAddressMode::Count)),
                  "Hardware table for Texture Address Mode does not match Pal::TexAddressMode enum.");

    return PalTexAddrToHwTbl[static_cast<uint32>(texAddress)];
}

// =====================================================================================================================
void PAL_STDCALL Device::CreateSamplerSrds(
    const IDevice*     pDevice,
    uint32             count,
    const SamplerInfo* pSamplerInfo,
    void*              pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pSamplerInfo != nullptr) && (count > 0));
    const Pal::Device* pPalDevice = static_cast<const Pal::Device*>(pDevice);
    const Device*      pGfxDevice = static_cast<const Device*>(pPalDevice->GetGfxDevice());

    // We don't know where pOut points; it could be pointing into uncached memory where read-modify-writes would be very
    // expensive.  Therefore, we build the SRDs in batches on the stack then memcpy to pOut.
    constexpr uint32 SrdBatchSize = 32;
    sq_img_samp_t srdBatch[SrdBatchSize];

    uint32 srdsBuilt = 0;
    while (srdsBuilt < count)
    {
        void* pSrdOutput = VoidPtrInc(pOut, (srdsBuilt * sizeof(sq_img_samp_t)));

        uint32 batchIdx = 0;
        while ((srdsBuilt < count) && (batchIdx < SrdBatchSize))
        {
            const SamplerInfo* pInfo = &pSamplerInfo[srdsBuilt];
            auto*              pSrd  = &srdBatch[batchIdx];

            // Start with a zeroed out SRD for safety.
            pSrd->u64All[0] = 0;
            pSrd->u64All[1] = 0;

            const SQ_TEX_ANISO_RATIO maxAnisoRatio = GetAnisoRatio(*pInfo);

            pSrd->clamp_x            = GetAddressClamp(pInfo->addressU);
            pSrd->clamp_y            = GetAddressClamp(pInfo->addressV);
            pSrd->clamp_z            = GetAddressClamp(pInfo->addressW);
            pSrd->max_aniso_ratio    = maxAnisoRatio;
            pSrd->depth_compare_func = static_cast<uint32>(pInfo->compareFunc);
            pSrd->force_unnormalized = pInfo->flags.unnormalizedCoords;
            pSrd->trunc_coord        = pInfo->flags.truncateCoords;
            pSrd->disable_cube_wrap  = (pInfo->flags.seamlessCubeMapFiltering == 1) ? 0 : 1;

            constexpr uint32 SamplerLodMinMaxIntBits  = 5;
            constexpr uint32 SamplerLodMinMaxFracBits = 8;
            pSrd->min_lod = Math::FloatToUFixed(pInfo->minLod, SamplerLodMinMaxIntBits, SamplerLodMinMaxFracBits);
            pSrd->max_lod = Math::FloatToUFixed(pInfo->maxLod, SamplerLodMinMaxIntBits, SamplerLodMinMaxFracBits);

            constexpr uint32 SamplerLodBiasIntBits  = 6;
            constexpr uint32 SamplerLodBiasFracBits = 8;

            // Setup XY and Mip filters.  Encoding of the API enumerations is:  xxyyzzww, where:
            //     ww : mag filter bits
            //     zz : min filter bits
            //     yy : z filter bits
            //     xx : mip filter bits
            pSrd->xy_mag_filter      = static_cast<uint32>(pInfo->filter.magnification);
            pSrd->xy_min_filter      = static_cast<uint32>(pInfo->filter.minification);
            pSrd->z_filter           = static_cast<uint32>(pInfo->filter.zFilter);
            pSrd->mip_filter         = static_cast<uint32>(pInfo->filter.mipFilter);
            pSrd->lod_bias           = Math::FloatToSFixed(pInfo->mipLodBias,
                                                           SamplerLodBiasIntBits,
                                                           SamplerLodBiasFracBits);

            // Ensure useAnisoThreshold is only set when preciseAniso is disabled
            PAL_ASSERT((pInfo->flags.preciseAniso == 0) ||
                        ((pInfo->flags.preciseAniso == 1) && (pInfo->flags.useAnisoThreshold == 0)));

            if (pInfo->flags.preciseAniso == 0)
            {
                // Setup filtering optimization levels: these will be modulated by the global filter
                // optimization aggressiveness, which is controlled by the "TFQ" public setting.
                // NOTE: Aggressiveness of optimizations is influenced by the max anisotropy level.
                constexpr uint32 PerfMipOffset = 6;

                pSrd->perf_mip = (pInfo->perfMip > 0) ? pInfo->perfMip : maxAnisoRatio + PerfMipOffset;

                constexpr uint32 NumAnisoThresholdValues = 8;

                if (pInfo->flags.useAnisoThreshold)
                {
                    // ANISO_THRESHOLD is a 3 bit number representing adjustments of 0/8 through 7/8
                    // so we quantize and clamp anisoThreshold into that range here.
                    pSrd->aniso_threshold = Util::Clamp(static_cast<uint32>(
                        static_cast<float>(NumAnisoThresholdValues) * pInfo->anisoThreshold),
                        0U,
                        NumAnisoThresholdValues - 1U);
                }
                else
                {
                    //  The code below does the following calculation.
                    //  if maxAnisotropy < 4   ANISO_THRESHOLD = 0 (0.0 adjust)
                    //  if maxAnisotropy < 16  ANISO_THRESHOLD = 1 (0.125 adjust)
                    //  if maxAnisotropy == 16 ANISO_THRESHOLD = 2 (0.25 adjust)
                    constexpr uint32 Gfx10AnisoRatioShift = 1;
                    pSrd->aniso_threshold = maxAnisoRatio >> Gfx10AnisoRatioShift;
                }

                pSrd->aniso_bias   = maxAnisoRatio;
                pSrd->lod_bias_sec = 0;
            }

            constexpr SQ_IMG_FILTER_TYPE HwFilterMode[]=
            {
                SQ_IMG_FILTER_MODE_BLEND, // TexFilterMode::Blend
                SQ_IMG_FILTER_MODE_MIN,   // TexFilterMode::Min
                SQ_IMG_FILTER_MODE_MAX,   // TexFilterMode::Max
            };

            PAL_ASSERT(static_cast<uint32>(pInfo->filterMode) < (Util::ArrayLen(HwFilterMode)));
            pSrd->filter_mode = HwFilterMode[static_cast<uint32>(pInfo->filterMode)];

            // And setup the HW-supported border colors appropriately
            switch (pInfo->borderColorType)
            {
            case BorderColorType::White:
                pSrd->border_color_type = SQ_TEX_BORDER_COLOR_OPAQUE_WHITE;
                break;
            case BorderColorType::TransparentBlack:
                pSrd->border_color_type = SQ_TEX_BORDER_COLOR_TRANS_BLACK;
                break;
            case BorderColorType::OpaqueBlack:
                pSrd->border_color_type = SQ_TEX_BORDER_COLOR_OPAQUE_BLACK;
                break;
            case BorderColorType::PaletteIndex:
                pSrd->border_color_type = SQ_TEX_BORDER_COLOR_REGISTER;
                pSrd->border_color_ptr  = pInfo->borderColorPaletteIndex;
                break;
            default:
                PAL_ASSERT_ALWAYS();
                break;
            }

            // This allows the sampler to override anisotropic filtering when the resource view contains a single
            // mipmap level.
            pSrd->aniso_override = (pInfo->flags.disableSingleMipAnisoOverride == 0);

            if (pInfo->flags.forResidencyMap)
            {
                // The u/v slope / offset fields are in the same location as the border_color_ptr field
                // used by PaletteIndex.  Verify that both residencymap and palette-index are not set.
                PAL_ASSERT(pInfo->borderColorType != BorderColorType::PaletteIndex);

                sq_img_samp_linked_resource_res_map_t* pLinkedRsrcSrd =
                        reinterpret_cast<sq_img_samp_linked_resource_res_map_t*>(pSrd);

                //  if (T#.linked_resource != 0)
                //      11:9 - v_offset(w_offset for 3D texture) value selector
                //       8:6 - v_slope(w_slope for 3D texture) value selector
                //       5:3 - u_offset value selector
                //       2:0 - u_slope value selector
                //
                // Offset values as specified by the client start at 1 / (1 << 0) = 1.  However,
                // HW considers a programmed value of zero to represent an offset of 1/4th.  Bias
                // the supplied value here.
                constexpr uint32 LowValidOffset = 2; // Log2(4);

                const uint32 biasedOffsetX = pInfo->uvOffset.x - LowValidOffset;
                const uint32 biasedOffsetY = pInfo->uvOffset.y - LowValidOffset;

                pLinkedRsrcSrd->linked_resource_slopes = (((pInfo->uvSlope.x & 0x7) << 0) |
                                                          ((biasedOffsetX    & 0x7) << 3) |
                                                          ((pInfo->uvSlope.y & 0x7) << 6) |
                                                          ((biasedOffsetY    & 0x7) << 9));

                // Verify that the "linked_resource_slopes" lines up with the "border_color_ptr" field.
                PAL_ASSERT(pSrd->border_color_ptr == pLinkedRsrcSrd->linked_resource_slopes);
            }

            srdsBuilt++;
            batchIdx++;
        }

        memcpy(pSrdOutput, &srdBatch[0], (batchIdx * sizeof(sq_img_samp_t)));
    }
}

// =====================================================================================================================
void Device::CreateHiSZViewSrds(
    const Image&          image,
    const SubresRange&    subresRange,
    const SwizzledFormat& viewFormat,
    HiSZType              hiSZType,
    void*                 pOut
    ) const
{
    const auto*            pAddrMgr        = static_cast<const AddrMgr3::AddrMgr3*>(Parent()->GetAddrMgr());
    const Pal::Image*      pParent         = image.Parent();
    const ImageCreateInfo& imageCreateInfo = pParent->GetImageCreateInfo();
    const BoundGpuMemory&  boundGpuMem     = pParent->GetBoundGpuMemory();
    const HiSZ*            pHiSZ           = image.GetHiSZ();

    PAL_ASSERT(pHiSZ != nullptr);

    sq_img_rsrc_t srd = {};

    const Extent3d&  baseExtent   = pHiSZ->GetBaseExtent();
    constexpr uint32 WidthLowSize = 2;

    srd.width_lo   = (baseExtent.width - 1) & ((1 << WidthLowSize) - 1);
    srd.width_hi   = (baseExtent.width - 1) >> WidthLowSize;
    srd.height     = (baseExtent.height - 1);
    srd.perf_mod   = uint32(TexPerfModulation::Default);
    srd.format     = Formats::Gfx12::HwImgFmt(viewFormat.format);
    srd.dst_sel_x  = Formats::Gfx12::HwSwizzle(viewFormat.swizzle.r);
    srd.dst_sel_y  = Formats::Gfx12::HwSwizzle(viewFormat.swizzle.g);
    srd.dst_sel_z  = Formats::Gfx12::HwSwizzle(viewFormat.swizzle.b);
    srd.dst_sel_w  = Formats::Gfx12::HwSwizzle(viewFormat.swizzle.a);
    srd.sw_mode    = pAddrMgr->GetHwSwizzleMode(pHiSZ->GetSwizzleMode(hiSZType));
    srd.bc_swizzle = GetBcSwizzle(viewFormat);

    CompressionMode finalCompressionMode = CompressionMode::ReadBypassWriteDisable;
    if (boundGpuMem.IsBound() && boundGpuMem.Memory()->MaybeCompressed())
    {
        finalCompressionMode = imageCreateInfo.compressionMode;
    }
    SetImageSrdCompression(pParent->GetDevice(), image, 0, finalCompressionMode, &srd);

    // SC CSIM uses backdoor memory access and not the GL1 interface, and bypassing GL1 interface is to bypass
    // accessing the data in distributed compression way.
    if (GetPlatform()->IsEmulationEnabled())
    {
        srd.compression_en        = 0;
        srd.write_compress_enable = 0;
    }

    const bool isMultiSampled = (imageCreateInfo.samples > 1);

    srd.type = (imageCreateInfo.arraySize == 1) ? (isMultiSampled ? SQ_RSRC_IMG_2D_MSAA       : SQ_RSRC_IMG_2D)
                                                : (isMultiSampled ? SQ_RSRC_IMG_2D_MSAA_ARRAY : SQ_RSRC_IMG_2D_ARRAY);

    if (isMultiSampled)
    {
        // MSAA textures cannot be mipmapped; the LAST_LEVEL and MAX_MIP fields indicate the texture's
        // sample count.  According to the docs, these are samples.  According to reality, this is
        // fragments.  I'm going with reality.
        const uint32 log2Fragments = Log2(imageCreateInfo.fragments);

        srd.base_level = 0;
        srd.last_level = log2Fragments;
        srd.max_mip    = log2Fragments;
    }
    else
    {
        const uint32 firstMipLevel = subresRange.startSubres.mipLevel;

        srd.base_level = firstMipLevel;
        srd.last_level = firstMipLevel + subresRange.numMips - 1;
        srd.max_mip    = imageCreateInfo.mipLevels - 1;
    }

    const uint32     baseArraySlice   = subresRange.startSubres.arraySlice;
    constexpr uint32 BaseArrayLowSize = 13;

    srd.base_array     = baseArraySlice & ((1 << BaseArrayLowSize) - 1);
    srd.base_array_msb = baseArraySlice >> BaseArrayLowSize;
    srd.depth          = baseArraySlice + subresRange.numSlices - 1;

    srd.base_address = pHiSZ->Get256BAddrSwizzled(hiSZType);

    memcpy(pOut, &srd, sizeof(srd));
}

// =====================================================================================================================
void PAL_STDCALL Device::CreateBvhSrds(
    const IDevice* pDevice,
    uint32         count,
    const BvhInfo* pBvhInfo,
    void*          pOut)
{
    PAL_ASSERT((pDevice != nullptr) && (pOut != nullptr) && (pBvhInfo != nullptr) && (count > 0));

    for (uint32 i = 0; i < count; ++i)
    {
        sq_bvh_rsrc_t    bvhSrd  = {};
        const BvhInfo&   bvhInfo = pBvhInfo[i];

        // Ok, there are two modes of operation here:
        //    1) raw VA.  The node_address is a tagged VA pointer, instead of a relative offset.  However, the
        //                HW still needs a BVH T# to tell it to run in raw VA mode and to configure the
        //                watertightness, box sorting, and cache behavior.
        //    2) BVH addressing:
        if (bvhInfo.flags.useZeroOffset == 0)
        {
            PAL_ASSERT(bvhInfo.pMemory != nullptr);
            const GpuMemoryDesc& memDesc = bvhInfo.pMemory->Desc();

            const gpusize gpuVa = memDesc.gpuVirtAddr + bvhInfo.offset;

            // Make sure the supplied memory pointer is aligned.
            PAL_ASSERT((gpuVa & 0xFF) == 0);

            bvhSrd.base_address = (gpuVa >> 8);
        }
        else
        {
            // Node_pointer comes from the VGPRs when the instruction is issued (vgpr_a[0] for image_bvh*,
            // vgpr_a[0:1] for image_bvh64*)
            bvhSrd.base_address = 0;
        }

        // Setup common srd fields here
        bvhSrd.size = bvhInfo.numNodes - 1;

        //    Number of ULPs to be added during ray-box test, encoded as unsigned integer
        // HW only has eight bits available for this field
        PAL_ASSERT((bvhInfo.boxGrowValue & ~0xFF) == 0);
        bvhSrd.box_grow_value = bvhInfo.boxGrowValue;

        //    0: Return data for triangle tests are
        //    { 0: t_num, 1 : t_denom, 2 : triangle_id, 3 : hit_status}
        //    1: Return data for triangle tests are
        //    { 0: t_num, 1 : t_denom, 2 : I_num, 3 : J_num }
        // This should only be set if HW supports the ray intersection mode that returns triangle barycentrics.
        bvhSrd.triangle_return_mode = bvhInfo.flags.returnBarycentrics;

        bvhSrd.box_sort_en = (bvhInfo.boxSortHeuristic == BoxSortHeuristic::Disabled) ? false : true;

        //    MSB must be set -- 0x8
        bvhSrd.type = 0x8;

        bvhSrd.pointer_flags = bvhInfo.flags.pointerFlags;

        if (bvhInfo.boxSortHeuristic != BoxSortHeuristic::Disabled)
        {
            bvhSrd.box_sorting_heuristic = static_cast<uint32>(bvhInfo.boxSortHeuristic);
        }

        bvhSrd.wide_sort_en         = bvhInfo.flags.wideSort;
        bvhSrd.box_node_64b         = bvhInfo.flags.highPrecisionBoxNode;
        bvhSrd.instance_en          = bvhInfo.flags.hwInstanceNode;
        bvhSrd.sort_triangles_first = bvhInfo.flags.sortTrianglesFirst;

        bvhSrd.compressed_format_en = bvhInfo.flags.compressedFormatEn;

        // HPB64 and compressed formats cannot be enabled simultaneously.
        PAL_ASSERT((bvhInfo.flags.compressedFormatEn == 0) || (bvhInfo.flags.highPrecisionBoxNode == 0));

        memcpy(VoidPtrInc(pOut, i * sizeof(sq_bvh_rsrc_t)), &bvhSrd, sizeof(sq_bvh_rsrc_t));
    }
}

} // namespace Gfx12
} // namespace Pal
