/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/addrMgr/addrMgr1/addrMgr1.h"
#include "palFormatInfo.h"

using namespace Util;

namespace Pal
{
namespace AddrMgr1
{

// =====================================================================================================================
AddrMgr1::AddrMgr1(
    const Device* pDevice)
    :
    // NOTE: Each subresource for AddrMgr1 hardware needs the following tiling information: the tiling caps for itself
    // along with the actual tiling information computed by the AddrLib.
    AddrMgr(pDevice, (sizeof(TileInfo) + sizeof(TilingCaps)))
{
}

// =====================================================================================================================
Result Create(
    const Device*  pDevice,
    void*          pPlacementAddr,
    AddrMgr**      ppAddrMgr)
{
    PAL_ASSERT((pDevice != nullptr) && (pPlacementAddr != nullptr) && (ppAddrMgr != nullptr));

    AddrMgr* pAddrMgr = PAL_PLACEMENT_NEW(pPlacementAddr) AddrMgr1(pDevice);

    Result result = pAddrMgr->Init();

    if (result == Result::Success)
    {
        (*ppAddrMgr) = pAddrMgr;
    }
    else
    {
        pAddrMgr->Destroy();
    }

    return result;
}

// =====================================================================================================================
// Returns the size, in bytes, required to support an AddrMgr1 object
size_t GetSize()
{
    return sizeof(AddrMgr1);
}

// =====================================================================================================================
// Assembles the tile token for the given subresource. The tile token is a generated key which can determine if two
// optimally tiled images are compatible for copying when the supportsMismatchedTileTokenCopy capability flag is false.
void AddrMgr1::BuildTileToken(
    SubResourceInfo* pSubResInfo,
    const TileInfo*  pTileInfo
    ) const
{
    TileToken token = { };

    switch (GetDevice()->ChipProperties().ossLevel)
    {
    case OssIpLevel::OssIp1:
        token.bits.tileMode         = pTileInfo->tileMode;
        token.bits.bankHeight       = pTileInfo->bankHeight;
        token.bits.bankWidth        = pTileInfo->bankWidth;
        token.bits.banks            = pTileInfo->banks;
        token.bits.macroAspectRatio = pTileInfo->macroAspectRatio;
        token.bits.tileSplitBytes   = pTileInfo->tileSplitBytes;
        // Fall-through intentional
    case OssIpLevel::OssIp2:
    case OssIpLevel::OssIp2_4:
        token.bits.tileType    = pTileInfo->tileType;
        token.bits.elementSize = Util::Log2(pSubResInfo->bitsPerTexel >> 3);
        break;
    default:
        PAL_NEVER_CALLED(); // Unsupported OssIp version!
        break;
    }

    pSubResInfo->tileToken = token.u32All;
}

// =====================================================================================================================
// Initializes all subresources for an Image object.
Result AddrMgr1::InitSubresourcesForImage(
    Image*             pImage,
    gpusize*           pGpuMemSize,
    gpusize*           pGpuMemAlignment,
    ImageMemoryLayout* pGpuMemLayout,
    SubResourceInfo*   pSubResInfoList,
    void*              pSubResTileInfoList,
    bool*              pDccUnsupported
    ) const
{
    Result result = Result::Success;

    int32 stencilTileIdx = TileIndexUnused;

    SubResIterator subResIt(*pImage);
    do
    {
        InitTilingCaps(pImage, subResIt.Index(), pSubResTileInfoList);

        result = ComputeSubResourceInfo(pImage,
                                        pSubResInfoList,
                                        pSubResTileInfoList,
                                        subResIt.Index(),
                                        pGpuMemLayout,
                                        pDccUnsupported,
                                        &stencilTileIdx);
        if (result != Result::Success)
        {
            break;
        }

        SubResourceInfo*const pSubResInfo = (pSubResInfoList + subResIt.Index());

        // Update the subresource offset and image total GPU memory size
        pSubResInfo->offset        = Pow2Align(*pGpuMemSize, pSubResInfo->baseAlign);
        pSubResInfo->swizzleOffset = pSubResInfo->offset;
        *pGpuMemSize               = pSubResInfo->offset + pSubResInfo->size;

        // Update the memory layout's swizzle equation information. These propagate down from index 0 to index
        // 1 so this check should skip this logic once we're found both swizzle equations.
        const uint8 eqIdx = static_cast<uint8>(pSubResInfo->swizzleEqIndex);
        if (pGpuMemLayout->swizzleEqIndices[1] != eqIdx)
        {
            if (pGpuMemLayout->swizzleEqIndices[0] == InvalidSwizzleEqIndex)
            {
                // We set both indices because they must both be valid even if the image only uses one.
                pGpuMemLayout->swizzleEqIndices[0] = eqIdx;
                pGpuMemLayout->swizzleEqIndices[1] = eqIdx;
            }
            else if (pGpuMemLayout->swizzleEqIndices[0] == pGpuMemLayout->swizzleEqIndices[1])
            {
                // We've just transitioned to the second swizzle index.
                pGpuMemLayout->swizzleEqIndices[1] = eqIdx;

                // The transition cound happen either between two mip levels, or between two planes.
                const uint32 planeIndex = PlaneIndex(pSubResInfo->subresId.aspect);
                if ((pImage->GetImageInfo().numPlanes > 1) && (planeIndex != 0))
                {
                    pGpuMemLayout->swizzleEqTransitionPlane = static_cast<uint8>(planeIndex);
                }
                else
                {
                    pGpuMemLayout->swizzleEqTransitionMip = static_cast<uint8>(pSubResInfo->subresId.mipLevel);
                }
            }
            else
            {
                // We found an unexpected third swizzle index.
                PAL_ASSERT(pGpuMemLayout->swizzleEqIndices[0] == eqIdx);
            }
        }
    } while (subResIt.Next());

    return result;
}

// =====================================================================================================================
// Computes the size (in PRT tiles) of the mip tail for a particular Image aspect.
void AddrMgr1::ComputeTilesInMipTail(
    const Image&       image,
    ImageAspect        aspect,
    ImageMemoryLayout* pGpuMemLayout
    ) const
{
    const ImageCreateInfo& createInfo = image.GetImageCreateInfo();

    // This function is only supposed to be called for PRT Images which have a mip tail.
    PAL_ASSERT((createInfo.flags.prt != 0) && (pGpuMemLayout->prtMinPackedLod < createInfo.mipLevels));

    // AddrMgr1 only supports GPU's whose tiling has a single mip tail for the entire Image aspect, not one tail
    // per array slice.
    const auto& imageProperties = GetDevice()->ChipProperties().imageProperties;
    PAL_ASSERT((imageProperties.prtFeatures & PrtFeaturePerSliceMipTail) == 0);

    const SubresId startSubresId = { aspect, pGpuMemLayout->prtMinPackedLod, 0 };
    const SubresId endSubresId   = { aspect, (createInfo.mipLevels - 1), (createInfo.arraySize - 1) };

    const gpusize  startOffset = image.SubresourceInfo(startSubresId)->offset;
    const gpusize  endOffset   = (image.SubresourceInfo(endSubresId)->offset +
                                  image.SubresourceInfo(endSubresId)->size);

    pGpuMemLayout->prtMipTailTileCount =
            static_cast<uint32>(RoundUpQuotient((endOffset - startOffset), imageProperties.prtTileSize));
}

// =====================================================================================================================
// Initializes tiling capabilities for a subresource belonging to the specified Image.
void AddrMgr1::InitTilingCaps(
    Image* pImage,
    uint32 subResIdx,
    void*  pTileInfoList
    ) const
{
    TilingCaps*const pTileCaps = NonConstTilingCaps(pTileInfoList, subResIdx);

    const ImageCreateInfo& createInfo          = pImage->GetImageCreateInfo();
    const bool             linearModeRequested = (createInfo.tiling == ImageTiling::Linear);

    // Default to whatever tiling capabilities the settings have selected. This will be overridden for some types
    // of Images.
    // Most YUV-packed formats can be interpreted in a shader as having a different effective bits-per-pixel than YUV
    // format actually has. This requires that we use linear tiling because the tile swizzle pattern depends highly on
    // the bits-per-pixel of the tiled Image. The only exception is the NV12/P010 format. This needs to support tiling
    // because NV12/P010 Images can be presentable for some API's, and the display hardware requires tiling.
    if (linearModeRequested ||
        (Formats::IsYuv(createInfo.swizzledFormat.format)
        ))
    {
        // Linear tiling requested, so init tile caps to all zero
        pTileCaps->value = 0;
    }
    else
    {
        pTileCaps->value = Addr1TilingCaps;
    }

    if (pImage->IsPeer())
    {
        // Peer images must use the same tiling mode as the original image. The easiest way to satisfy that
        // requirement is to set the tiling caps to only support the original tiling mode.
        const auto& origTileInfo = *GetTileInfo(pImage->OriginalImage(), subResIdx);

        pTileCaps->value = 0;
        switch (AddrTileModeFromHwArrayMode(origTileInfo.tileMode))
        {
        case ADDR_TM_1D_TILED_THIN1:
            pTileCaps->tile1DThin1 = 1;
            break;
        case ADDR_TM_1D_TILED_THICK:
            pTileCaps->tile1DThick = 1;
            break;
        case ADDR_TM_2D_TILED_THIN1:
            pTileCaps->tile2DThin1 = 1;
            break;
        case ADDR_TM_2D_TILED_THICK:
            pTileCaps->tile2DThick = 1;
            break;
        case ADDR_TM_3D_TILED_THIN1:
            pTileCaps->tile3DThin1 = 1;
            break;
        case ADDR_TM_3D_TILED_THICK:
            pTileCaps->tile3DThick = 1;
            break;
        case ADDR_TM_2D_TILED_XTHICK:
            pTileCaps->tile2DXThick = 1;
            break;
        case ADDR_TM_3D_TILED_XTHICK:
            pTileCaps->tile3DXThick = 1;
            break;
        case ADDR_TM_PRT_TILED_THIN1:
            pTileCaps->tilePrtThin1 = 1;
            break;
        default:
            break;
        }
    }
}

// =====================================================================================================================
// Helper function to initialize the AddrLib surface infor flags for a subresource.
static ADDR_SURFACE_FLAGS InitSurfaceInfoFlags(
    const Device& device,
    const Image&  image,
    uint32        subResIdx)
{
    const ImageCreateInfo& createInfo = image.GetImageCreateInfo();
    const ImageInfo&       imageInfo  = image.GetImageInfo();
    const SubResourceInfo& subResInfo = *image.SubresourceInfo(subResIdx);

    ADDR_SURFACE_FLAGS flags = { };

    if (image.IsDepthStencil())
    {
        if (subResInfo.subresId.aspect == ImageAspect::Stencil)
        {
            flags.stencil = 1;
        }
        else if (subResInfo.subresId.aspect == ImageAspect::Depth)
        {
            flags.depth     = 1;
            flags.noStencil = (imageInfo.numPlanes == 1) ? 1 : 0;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 306
            if ((device.ChipProperties().gfxLevel > GfxIpLevel::GfxIp6) && flags.noStencil && image.IsResolveDst())
            {
                // Depth-copy resolve is only supported when depth surface is not splitted on Gfx7/Gfx8. So we set
                // nonSplit for depth-only resolveDst on Gfx7/Gfx8. Moreover, to make non-depth-only formats work
                // in db and tc for depth and stencil access simultaneously, we can't set nonSplit for non-depth-only
                // surface.
                flags.nonSplit = 1;
            }
#endif
        }
    }
    else
    {
        // We should always set the color flag for non-Depth/Stencil resources. Color block has more strict surface
        // alignments and a texture may be the destination of an image copy.
        flags.color = 1;
    }

    // We should always set the texture flag since even Color or Depth/Stencil resources could be bound as a shader
    // resource during RPM blts
    flags.texture = 1;

    // The interleaved flag informs the address library that there is extra padding between subresources due to YUV
    // packed and/or YUV planar formats.
    flags.interleaved = Formats::IsYuv(createInfo.swizzledFormat.format);

    flags.volume  = (createInfo.imageType == ImageType::Tex3d);
    flags.cube    = ((createInfo.arraySize % 6) == 0);
    flags.pow2Pad = (createInfo.mipLevels > 1);
    flags.display = createInfo.flags.flippable;

    // The following four flags have the given effects. They are applied to the surface in the order
    // they are listed. We shouldn't set any of them for shared surfaces because the tiling mode is already defined.
    // - opt4space:         If 2D padding is bigger than 1.5x 1D padding, convert tile mode to 1D.
    // - minimizeAlignment: If 2D padding is bigger than 1d padding, convert tile mode to 1D.
    // - maxAlignment64k:   If 2D macro block size is bigger than 64KB, convert tile mode to PRT.
    // - needEquation:      If tile mode is 2D, convert it to PRT tile mode.

    if (image.IsShared() == false)
    {
        flags.needEquation = createInfo.flags.needSwizzleEqs;

        // NV12 or P010 only support 2D THIN1 or linear tile mode, setting the opt4Space or minimizeAlignment flag for
        // those surfaces could change the tile mode to 1D THIN1.
        if (image.GetGfxImage()->IsRestrictedTiledMultiMediaSurface() == false)
        {
            if (createInfo.tilingOptMode == TilingOptMode::Balanced)
            {
                flags.opt4Space = 1;
            }
            else if (createInfo.tilingOptMode == TilingOptMode::OptForSpace)
            {
                flags.minimizeAlignment = 1;
            }
        }
    }

    flags.preferEquation = createInfo.flags.preferSwizzleEqs;

    const SubresId       mipZeroId        = { subResInfo.subresId.aspect, 0, subResInfo.subresId.arraySlice };
    const TileInfo*const pMipZeroTileInfo = GetTileInfo(&image, mipZeroId);

    flags.prt = ((createInfo.flags.prt != 0) || pMipZeroTileInfo->childMipsNeedPrtTileIndex);

    flags.qbStereo = createInfo.flags.stereo;

    return flags;
}

// =====================================================================================================================
// So far we have calculated the independent aligned dimensions of both the Y and the chroma planes.  PAL considers
// each plane to be its own subresource, but the HW considers both planes combined as one array slice.  Due to
// alignment constraints, the alignmed dimensions of one array slice is not the combined height of both planes (at
// least for macro-tiled images).  Compute the dimensions of one slice of a YUV planar surface here.
Result AddrMgr1::AdjustChromaPlane(
    Image*              pImage,
    SubResourceInfo*    pSubResInfoList,
    void*               pSubResTileInfoList,
    uint32              subResIdx,
    ImageMemoryLayout*  pGpuMemLayout
    ) const
{
    const ImageCreateInfo& imageCreateInfo   = pImage->GetImageCreateInfo();
    SubResourceInfo*const  pChromaSubResInfo = (pSubResInfoList + subResIdx);
    const SubresId         chromaSubresId    = pChromaSubResInfo->subresId;
    Result                 result            = Result::Success;

    PAL_ASSERT((chromaSubresId.aspect == ImageAspect::CbCr) ||
               (chromaSubresId.aspect == ImageAspect::Cb)   ||
               (chromaSubresId.aspect == ImageAspect::Cr));

    // Verify that we are not currently processing the last array slice associated with this image...  That one
    // doesn't require any further padding.
    if ((chromaSubresId.arraySlice != (imageCreateInfo.arraySize - 1)))
    {
        const SubresId         ySubresId    = { ImageAspect::Y, chromaSubresId.mipLevel, chromaSubresId.arraySlice };
        const SubResourceInfo* pYsubResInfo = pImage->SubresourceInfo(ySubresId);
        const TileInfo*        pYtileInfo   = GetTileInfo(pImage, ySubresId);
        const AddrTileMode     yTileMode    = AddrTileModeFromHwArrayMode(pYtileInfo->tileMode);

        // We calculate the dimensions of the chroma plane twice -- once to get some info so that we can calc the
        // Y plane dimensions and once "for real".  Make sure this is the "for real" path.  Also, linear and 1D images
        // don't require any additional fixup.
        if ((pYsubResInfo->actualExtentElements.height != 0) && IsMacroTiled(yTileMode))
        {
            // Ok.  At this point, we have calculated the padded dimensions of the Y and UV planes and stored that
            // info in the associated sub-res-info structs for those planes.  The issue here is that to the texture
            // pipe, each slice is the size of the combined Y and UV planes, and that could introduce additional
            // padding. Ugh.  We need to recalculate the size of a slice here and adjust the size and actualHeight
            // of the UV plane accordingly.
            ADDR_COMPUTE_SURFACE_INFO_INPUT  surfInfoIn       = { };
            ADDR_COMPUTE_SURFACE_INFO_OUTPUT surfInfoOut      = { };
            ADDR_TILEINFO                    tileInfo         = { };
            ADDR_QBSTEREOINFO                addrStereoInfo   = { };
            bool                             dccUnsupported   = false; // who cares?
            int32                            stencilTileIdx   = 0;     // who cares?
            const uint32                     yAndChromaHeight = pYsubResInfo->actualExtentElements.height +
                                                                pChromaSubResInfo->actualExtentElements.height;

            surfInfoOut.pTileInfo   = &tileInfo;
            surfInfoOut.pStereoInfo = &addrStereoInfo;

            const ADDR_E_RETURNCODE addrRet = CalcSurfInfoOut(pImage,
                                                              pSubResInfoList,
                                                              pSubResTileInfoList,
                                                              pImage->CalcSubresourceId(ySubresId),
                                                              pYsubResInfo->actualExtentElements.width,
                                                              yAndChromaHeight,
                                                              pGpuMemLayout,
                                                              &dccUnsupported,
                                                              &stencilTileIdx,
                                                              &surfInfoIn,
                                                              &surfInfoOut);

            if (addrRet == ADDR_OK)
            {
                pChromaSubResInfo->actualExtentElements.height = surfInfoOut.pixelHeight -
                                                                 pYsubResInfo->actualExtentElements.height;
                pChromaSubResInfo->actualExtentTexels.height   = pChromaSubResInfo->actualExtentElements.height;
                pChromaSubResInfo->size                        = surfInfoOut.sliceSize - pYsubResInfo->size;
            }
            else
            {
                result = Result::ErrorUnknown;
            }
        }
    }

    return result;
}

// =====================================================================================================================
ADDR_E_RETURNCODE AddrMgr1::CalcSurfInfoOut(
    Image*                             pImage,
    SubResourceInfo*                   pSubResInfoList,
    void*                              pSubResTileInfoList,
    uint32                             subResIdx,
    uint32                             subResWidth,
    uint32                             subResHeight,
    ImageMemoryLayout*                 pGpuMemLayout,
    bool*                              pDccUnsupported,
    int32*                             pStencilTileIdx,
    ADDR_COMPUTE_SURFACE_INFO_INPUT*   pSurfInfoInput,
    ADDR_COMPUTE_SURFACE_INFO_OUTPUT*  pSurfInfoOutput
    ) const
{
    const ImageCreateInfo& imageCreateInfo      = pImage->GetImageCreateInfo();
    const ImageInfo&       imageInfo            = pImage->GetImageInfo();
    SubResourceInfo*const  pSubResInfo          = (pSubResInfoList + subResIdx);
    const auto*const       pBaseSubResInfo      = pImage->SubresourceInfo(0);
    const TileInfo*const   pBaseTileInfo        = GetTileInfo(pImage, 0);
    const bool             isSecondPlaneStencil =
        ((pSubResInfo->subresId.aspect == ImageAspect::Stencil) && (imageInfo.numPlanes > 1));

    pSurfInfoInput->size         = sizeof(*pSurfInfoInput);
    pSurfInfoInput->format       = Image::GetAddrFormat(pSubResInfo->format.format);
    pSurfInfoInput->bpp          = pSubResInfo->bitsPerTexel;
    pSurfInfoInput->mipLevel     = pSubResInfo->subresId.mipLevel;
    pSurfInfoInput->slice        = pSubResInfo->subresId.arraySlice;
    pSurfInfoInput->width        = subResWidth;
    pSurfInfoInput->height       = subResHeight;
    pSurfInfoInput->numSlices    = (imageCreateInfo.imageType == ImageType::Tex3d) ? pSubResInfo->extentTexels.depth
                                                                                   : imageCreateInfo.arraySize;
    pSurfInfoInput->numSamples   = imageCreateInfo.samples;
    pSurfInfoInput->numFrags     = imageCreateInfo.fragments;
    pSurfInfoInput->maxBaseAlign = imageCreateInfo.maxBaseAlign;
    pSurfInfoInput->flags        = InitSurfaceInfoFlags(*GetDevice(), *pImage, subResIdx);

    const bool alignYuvPlanes = (imageCreateInfo.tiling == ImageTiling::Optimal)                ||
                                (imageCreateInfo.swizzledFormat.format == ChNumFormat::YV12)    ||
                                (imageCreateInfo.swizzledFormat.format == ChNumFormat::NV11);

    // NOTE: To handle YUV planar Images, it is required that the actualHeight and pitch of the chroma plane(s)
    // are half (or quarter for NV11) of that of the luma plane. Tiled images, linear YV12 or NV11 images may not meet
    // the request because the planes have different bits-per-pixel, which can result in different tiling modes, etc.
    // To avoid this problem, we need to precompute the subresource info for one of the chroma planes and use its padded
    // dimensions to "lie" to AddrLib about the dimensions of the luma plane.
    const bool isLumaPlane    = (pSubResInfo->subresId.aspect == ImageAspect::Y);
    const bool isYuvPlanar    = Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format);
    if (isYuvPlanar && isLumaPlane && alignYuvPlanes && (pSubResInfo->actualExtentTexels.width == 0))
    {
        SubresId chromaSubRes = pSubResInfo->subresId;
        chromaSubRes.aspect   = (imageInfo.numPlanes == 2) ? ImageAspect::CbCr : ImageAspect::Cb;
        const uint32 chromaSubResId = pImage->CalcSubresourceId(chromaSubRes);

        Result result = ComputeSubResourceInfo(pImage,
                                               pSubResInfoList,
                                               pSubResTileInfoList,
                                               chromaSubResId,
                                               pGpuMemLayout,
                                               pDccUnsupported,
                                               pStencilTileIdx);

        Extent3d log2Ratio = Formats::Log2SubsamplingRatio(imageCreateInfo.swizzledFormat.format, chromaSubRes.aspect);

        pSurfInfoInput->width  = (pSubResInfoList[chromaSubResId].actualExtentTexels.width  << log2Ratio.width);
        pSurfInfoInput->height = (pSubResInfoList[chromaSubResId].actualExtentTexels.height << log2Ratio.height);
    }

    if ((pSubResInfo->subresId.mipLevel > 0) || isSecondPlaneStencil)
    {
        // If we're setting up a nonzero mip level, or setting up the stencil plane of a depth/stencil Image, we must
        // use the base subresource's tile-mode, tile-type and pitch.
        pSurfInfoInput->tileMode  = AddrTileModeFromHwArrayMode(pBaseTileInfo->tileMode);
        pSurfInfoInput->tileType  = AddrTileTypeFromHwMicroTileMode(pBaseTileInfo->tileType);
        pSurfInfoInput->basePitch = pBaseSubResInfo->actualExtentTexels.width;
    }

    // The GfxIp HWL needs to be able to override or initialize some parts of the AddrLib surface info.
    Result result = pImage->GetGfxImage()->Addr1InitSurfaceInfo(subResIdx, pSurfInfoInput);
    PAL_ALERT(result != Result::Success); // This should never happen under normal circumstances.

    // The matchStencilTileCfg flag is only valid for depth/stencil Images!
    PAL_ASSERT(pImage->IsDepthStencil() || (pSurfInfoInput->flags.matchStencilTileCfg == 0));

    if (imageInfo.internalCreateInfo.flags.useSharedTilingOverrides)
    {
        pSurfInfoInput->tileIndex = imageInfo.internalCreateInfo.gfx6.sharedTileIndex;
    }
    else if (isSecondPlaneStencil)
    {
        if (((*pStencilTileIdx == TileIndexUnused) &&
             (pSurfInfoInput->flags.tcCompatible)) ||
            (pSubResInfo->subresId.mipLevel != 0))
        {
            // For this stencil surface to actually work with the texture engine, we need to use the same tile-index
            // between it and the Z-surface.
            const SubresId depthSubResource =
                { ImageAspect::Depth, pSubResInfo->subresId.mipLevel, pSubResInfo->subresId.arraySlice };

            pSurfInfoInput->tileIndex = GetTileInfo(pImage, depthSubResource)->tileIndex;
        }
        else
        {
            // Set Stencil tile index to previously returned matching tile config.
            pSurfInfoInput->tileIndex = *pStencilTileIdx;
        }
    }
    else
    {
        pSurfInfoInput->tileIndex = TileIndexUnused;
    }

    // We must convert our byte pitches into units of elements. For most formats (including BC formats) the subresource
    // bitsPerTexel is already the size of an element. The exception is 96-bit formats which have three 32-bit element
    // per texel.
    const uint32 bytesPerElement = AddrMgr::CalcBytesPerElement(pSubResInfo);

    // For nonzero mip level, let the AddrLib calculate the rowPitch/depthPitch according to the base pitch.
    if ((pSubResInfo->subresId.mipLevel == 0) && (imageCreateInfo.rowPitch > 0) && (imageCreateInfo.depthPitch > 0))
    {
        PAL_ASSERT((imageCreateInfo.rowPitch   % bytesPerElement)          == 0);
        PAL_ASSERT((imageCreateInfo.depthPitch % imageCreateInfo.rowPitch) == 0);

        pSurfInfoInput->pitchAlign = imageCreateInfo.rowPitch / bytesPerElement;

        gpusize planeSize = imageCreateInfo.depthPitch;
        if (isYuvPlanar)
        {
            if (pSubResInfo->subresId.aspect == ImageAspect::Y)
            {
                planeSize = imageInfo.internalCreateInfo.chromaPlaneOffset[0];
            }
            else if (pSubResInfo->subresId.aspect == ImageAspect::CbCr)
            {
                planeSize -= imageInfo.internalCreateInfo.chromaPlaneOffset[0];
            }
            else if (pSubResInfo->subresId.aspect == ImageAspect::Cb)
            {
                planeSize = (imageInfo.internalCreateInfo.chromaPlaneOffset[1] -
                             imageInfo.internalCreateInfo.chromaPlaneOffset[0]);
            }
            else if (pSubResInfo->subresId.aspect == ImageAspect::Cr)
            {
                planeSize -= imageInfo.internalCreateInfo.chromaPlaneOffset[1];
            }

            PAL_ASSERT(imageInfo.internalCreateInfo.chromaPlaneOffset[0] != 0);
            PAL_ASSERT((imageInfo.numPlanes != 3) || (imageInfo.internalCreateInfo.chromaPlaneOffset[1] != 0));
        }

        pSurfInfoInput->heightAlign = static_cast<uint32>(planeSize / imageCreateInfo.rowPitch);
    }

    pSurfInfoOutput->size = sizeof(*pSurfInfoOutput);

    return AddrComputeSurfaceInfo(AddrLibHandle(), pSurfInfoInput, pSurfInfoOutput);
}

// =====================================================================================================================
// Uses AddrLib to compute the complete information describing a single subresource.
Result AddrMgr1::ComputeSubResourceInfo(
    Image*             pImage,
    SubResourceInfo*   pSubResInfoList,
    void*              pSubResTileInfoList,
    uint32             subResIdx,
    ImageMemoryLayout* pGpuMemLayout,
    bool*              pDccUnsupported,
    int32*             pStencilTileIdx
    ) const
{
    const ImageCreateInfo&            imageCreateInfo = pImage->GetImageCreateInfo();
    SubResourceInfo*const             pSubResInfo     = (pSubResInfoList + subResIdx);
    const uint32                      bytesPerElement = CalcBytesPerElement(pSubResInfo);
    ADDR_COMPUTE_SURFACE_INFO_INPUT   surfInfoIn      = {};
    ADDR_COMPUTE_SURFACE_INFO_OUTPUT  surfInfoOut     = {};
    ADDR_TILEINFO                     tileInfo        = {};
    ADDR_QBSTEREOINFO                 addrStereoInfo  = {};
    TileInfo*const                    pTileInfo       = NonConstTileInfo(pSubResTileInfoList, subResIdx);
    const bool                        isYuvPlanar     = Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format);
    Result                            result          = Result::Success;

    surfInfoOut.pTileInfo   = &tileInfo;
    surfInfoOut.pStereoInfo = &addrStereoInfo;

    ADDR_E_RETURNCODE addrRet = CalcSurfInfoOut(pImage,
                                                pSubResInfoList,
                                                pSubResTileInfoList,
                                                subResIdx,
                                                pSubResInfo->extentTexels.width,
                                                pSubResInfo->extentTexels.height,
                                                pGpuMemLayout,
                                                pDccUnsupported,
                                                pStencilTileIdx,
                                                &surfInfoIn,
                                                &surfInfoOut);

    if (addrRet == ADDR_OK)
    {
        if ((surfInfoIn.flags.matchStencilTileCfg != 0) && (surfInfoOut.stencilTileIdx == TileIndexUnused))
        {
            // AddrLib was unable to match the Z and Stencil tile configurations. We need to downgrade the subresource
            // to use 1D tiling as a last-resort. This can sometimes occur if a PRT tile mode was requested because
            // some HW doesn't have any PRT tiling modes where Z and Stencil match.
            if (imageCreateInfo.samples <= 1)
            {
                PAL_DPWARN("Downgrading Depth/Stencil surface to 1D tiling!");
                surfInfoIn.tileMode = ADDR_TM_1D_TILED_THIN1;

                // Re-call into AddrLib to try again with 1D tiling.
                addrRet = AddrComputeSurfaceInfo(AddrLibHandle(), &surfInfoIn, &surfInfoOut);
            }
            else
            {
                PAL_DPWARN("Unable to match Depth/Stencil tile configurations, but MSAA resource requires 2D tiling");
            }
        }
    }

    if (addrRet == ADDR_OK)
    {
        // Convert the address library's swizzle equation index into Pal's representation. Note that linear tile modes
        // will result in an invalid equation index. To give our clients a way to handle linear modes we set the index
        // to LinearSwizzleEqIndex.
        if ((surfInfoOut.tileMode == ADDR_TM_LINEAR_GENERAL) || (surfInfoOut.tileMode == ADDR_TM_LINEAR_ALIGNED))
        {
            pSubResInfo->swizzleEqIndex = LinearSwizzleEqIndex;
        }
        else if (surfInfoOut.equationIndex == ADDR_INVALID_EQUATION_INDEX)
        {
            pSubResInfo->swizzleEqIndex = InvalidSwizzleEqIndex;
        }
        else
        {
            pSubResInfo->swizzleEqIndex = static_cast<uint8>(surfInfoOut.equationIndex);
        }

        // Verify that we got the element size calculation correct
        PAL_ASSERT(surfInfoOut.bpp == (bytesPerElement << 3));

        // This alert means that we're want this (potentially) compressed surface to be compatible with the texture
        // pipe, but the address library is saying that it can't be done.
        PAL_ALERT(surfInfoIn.flags.tcCompatible != surfInfoOut.tcCompatible);

        pSubResInfo->actualExtentTexels.width    = surfInfoOut.pixelPitch;
        pSubResInfo->actualExtentTexels.height   = surfInfoOut.pixelHeight;
        pSubResInfo->actualExtentTexels.depth    = (surfInfoIn.flags.volume != 0) ? surfInfoOut.depth : 1;
        pSubResInfo->actualExtentElements.width  = surfInfoOut.pitch;
        pSubResInfo->actualExtentElements.height = surfInfoOut.height;
        pSubResInfo->actualExtentElements.depth  = pSubResInfo->actualExtentTexels.depth;

        pSubResInfo->blockSize.width  = surfInfoOut.blockWidth;
        pSubResInfo->blockSize.height = surfInfoOut.blockHeight;
        pSubResInfo->blockSize.depth  = surfInfoOut.blockSlices;

        if (imageCreateInfo.flags.stereo == 1)
        {
            const uint32 tileSwizzleRight = addrStereoInfo.rightSwizzle << 8;

            pGpuMemLayout->stereoLineOffset  = addrStereoInfo.eyeHeight;
            pSubResInfo->extentTexels.height += pGpuMemLayout->stereoLineOffset;
            pSubResInfo->stereoLineOffset    = pGpuMemLayout->stereoLineOffset;
            pSubResInfo->stereoOffset        = (addrStereoInfo.rightOffset | tileSwizzleRight);
        }

        // AddrLib doesn't tell us the values for extentElements so we must compute it ourselves. It also doesn't
        // report the exact ratios between texels and elements but we can compute them from the pitch and height
        // data which is returned in both texels and elements. The depth values are always the same.
        if (surfInfoOut.pixelPitch >= surfInfoOut.pitch)
        {
            // We must round up to the nearest element because the caller is not required to pad the texel extent.
            const uint32 texelsPerElement     = (surfInfoOut.pixelPitch / surfInfoOut.pitch);
            pSubResInfo->extentElements.width = RoundUpQuotient(pSubResInfo->extentTexels.width, texelsPerElement);
        }
        else
        {
            const uint32 elementsPerTexel     = (surfInfoOut.pitch / surfInfoOut.pixelPitch);
            pSubResInfo->extentElements.width = (pSubResInfo->extentTexels.width * elementsPerTexel);
        }

        if (surfInfoOut.pixelHeight >= surfInfoOut.height)
        {
            // We must round up to the nearest element because the caller is not required to pad the texel extent.
            const uint32 texelsPerElement      = (surfInfoOut.pixelHeight / surfInfoOut.height);
            pSubResInfo->extentElements.height = RoundUpQuotient(pSubResInfo->extentTexels.height, texelsPerElement);
        }
        else
        {
            const uint32 elementsPerTexel      = (surfInfoOut.height / surfInfoOut.pixelHeight);
            pSubResInfo->extentElements.height = (pSubResInfo->extentTexels.height * elementsPerTexel);
        }

        pSubResInfo->extentElements.depth = pSubResInfo->extentTexels.depth;

        // Make sure AddrLib gave us a subresource alignment which is compatible with the client's requirements.
        PAL_ASSERT((imageCreateInfo.maxBaseAlign == 0) || (surfInfoOut.baseAlign <= imageCreateInfo.maxBaseAlign));
        pSubResInfo->baseAlign = surfInfoOut.baseAlign;
        pSubResInfo->size      = surfInfoOut.sliceSize;

        // Compute the exact row and depth pitches in bytes. This math must be done in terms of elements instead of
        // texels because some formats (e.g., R32G32B32) have pitches that are not multiples of their texel size.
        pSubResInfo->rowPitch   = (pSubResInfo->actualExtentElements.width  * bytesPerElement);
        pSubResInfo->depthPitch = (pSubResInfo->actualExtentElements.height * pSubResInfo->rowPitch);

        if (surfInfoOut.dccUnsupport != 0)
        {
            // DCC can only be enabled or disabled for the whole Image. If one subresource cannot support it, we need
            // to disable it for all subresources.
            *pDccUnsupported = true;
        }
        else if ((surfInfoIn.flags.matchStencilTileCfg != 0) &&
                 (pSubResInfo->subresId.aspect == ImageAspect::Depth))
        {
            // If the Image requested a matching tile configuration between the depth and stencil aspects, save the
            // tile index for stencil reported by AddrLib.
            *pStencilTileIdx = surfInfoOut.stencilTileIdx;
        }

        pTileInfo->tileIndex      = surfInfoOut.tileIndex;
        pTileInfo->macroModeIndex = surfInfoOut.macroModeIndex;

        if (surfInfoOut.pTileInfo != nullptr)
        {
            ADDR_CONVERT_TILEINFOTOHW_INPUT tileInfoToHwIn = { };
            tileInfoToHwIn.size           = sizeof(tileInfoToHwIn);
            tileInfoToHwIn.tileIndex      = TileIndexUnused;
            tileInfoToHwIn.macroModeIndex = TileIndexUnused;
            tileInfoToHwIn.pTileInfo      = surfInfoOut.pTileInfo;

            ADDR_TILEINFO                    tileInfoOut     = { };
            ADDR_CONVERT_TILEINFOTOHW_OUTPUT tileInfoToHwOut = { };
            tileInfoToHwOut.size      = sizeof(tileInfoToHwOut);
            tileInfoToHwOut.pTileInfo = &tileInfoOut;

            addrRet = AddrConvertTileInfoToHW(AddrLibHandle(), &tileInfoToHwIn, &tileInfoToHwOut);

            pTileInfo->banks            = tileInfoOut.banks;
            pTileInfo->bankWidth        = tileInfoOut.bankWidth;
            pTileInfo->bankHeight       = tileInfoOut.bankHeight;
            pTileInfo->macroAspectRatio = tileInfoOut.macroAspectRatio;
            pTileInfo->tileSplitBytes   = tileInfoOut.tileSplitBytes;
            pTileInfo->pipeConfig       = tileInfoOut.pipeConfig;
        }

        // The GfxIp HWL needs to initialize some tiling properties specific to itself.
        pImage->GetGfxImage()->Addr1FinalizeSubresource(subResIdx, pSubResInfoList, pSubResTileInfoList, surfInfoOut);

        BuildTileToken(pSubResInfo, pTileInfo);

        // Set the PRT tile dimensions: For PRT images, the pitchAlign and heightAlign of the base subresource
        // represent the PRT tile dimensions.
        if ((imageCreateInfo.flags.prt != 0) && (subResIdx == 0))
        {
            pGpuMemLayout->prtTileWidth  = surfInfoOut.pitchAlign;
            pGpuMemLayout->prtTileHeight = surfInfoOut.heightAlign;
            pGpuMemLayout->prtTileDepth  = 1; // 3D PRT's are not supported by AddrMgr1.
        }

        if ((result == Result::Success) && isYuvPlanar && (pSubResInfo->subresId.aspect != ImageAspect::Y))
        {
            result = AdjustChromaPlane(pImage, pSubResInfoList, pSubResTileInfoList, subResIdx, pGpuMemLayout);
        }

#if PAL_DEVELOPER_BUILD
        Developer::ImageDataAddrMgrSurfInfo data = {};

        if ((surfInfoOut.tileMode == ADDR_TM_LINEAR_GENERAL) ||
            (surfInfoOut.tileMode == ADDR_TM_LINEAR_ALIGNED))
        {
            data.tiling.gfx6.mode.dimension = Developer::Gfx6ImageTileModeDimension::Linear;
        }
        else if ((surfInfoOut.tileMode == ADDR_TM_1D_TILED_THIN1) ||
                 (surfInfoOut.tileMode == ADDR_TM_1D_TILED_THICK))
        {
            data.tiling.gfx6.mode.dimension = Developer::Gfx6ImageTileModeDimension::Dim1d;
            if (surfInfoOut.tileMode == ADDR_TM_1D_TILED_THIN1)
            {
                data.tiling.gfx6.mode.properties.thin = true;
            }
            else
            {
                data.tiling.gfx6.mode.properties.thick = true;
            }
        }
        else if ((surfInfoOut.tileMode == ADDR_TM_2D_TILED_THIN1)     ||
                 (surfInfoOut.tileMode == ADDR_TM_2D_TILED_THIN2)     ||
                 (surfInfoOut.tileMode == ADDR_TM_2D_TILED_THIN4)     ||
                 (surfInfoOut.tileMode == ADDR_TM_2D_TILED_THICK)     ||
                 (surfInfoOut.tileMode == ADDR_TM_2D_TILED_XTHICK)    ||
                 (surfInfoOut.tileMode == ADDR_TM_PRT_2D_TILED_THIN1) ||
                 (surfInfoOut.tileMode == ADDR_TM_PRT_2D_TILED_THICK))
        {
            data.tiling.gfx6.mode.dimension = Developer::Gfx6ImageTileModeDimension::Dim2d;
            if ((surfInfoOut.tileMode == ADDR_TM_PRT_2D_TILED_THIN1) ||
                (surfInfoOut.tileMode == ADDR_TM_PRT_2D_TILED_THICK))
            {
                data.tiling.gfx6.mode.properties.prt = true;
            }
            if ((surfInfoOut.tileMode == ADDR_TM_2D_TILED_THIN1) ||
                (surfInfoOut.tileMode == ADDR_TM_2D_TILED_THIN2) ||
                (surfInfoOut.tileMode == ADDR_TM_2D_TILED_THIN4) ||
                (surfInfoOut.tileMode == ADDR_TM_PRT_2D_TILED_THIN1))
            {
                data.tiling.gfx6.mode.properties.thin = true;
            }
            else
            {
                data.tiling.gfx6.mode.properties.thick = true;
            }
        }
        else if ((surfInfoOut.tileMode == ADDR_TM_3D_TILED_THIN1)     ||
                 (surfInfoOut.tileMode == ADDR_TM_3D_TILED_THICK)     ||
                 (surfInfoOut.tileMode == ADDR_TM_3D_TILED_XTHICK)    ||
                 (surfInfoOut.tileMode == ADDR_TM_PRT_3D_TILED_THIN1) ||
                 (surfInfoOut.tileMode == ADDR_TM_PRT_3D_TILED_THICK))
        {
            data.tiling.gfx6.mode.dimension = Developer::Gfx6ImageTileModeDimension::Dim3d;
            if ((surfInfoOut.tileMode == ADDR_TM_PRT_3D_TILED_THIN1) ||
                (surfInfoOut.tileMode == ADDR_TM_PRT_3D_TILED_THICK))
            {
                data.tiling.gfx6.mode.properties.prt = true;
            }
            if ((surfInfoOut.tileMode == ADDR_TM_3D_TILED_THIN1) ||
                (surfInfoOut.tileMode == ADDR_TM_PRT_3D_TILED_THIN1))
            {
                data.tiling.gfx6.mode.properties.thin = true;
            }
            else
            {
                data.tiling.gfx6.mode.properties.thick = true;
            }
        }

        if (surfInfoOut.tileType == ADDR_DISPLAYABLE)
        {
            data.tiling.gfx6.type = Developer::Gfx6ImageTileType::Displayable;
        }
        else if (surfInfoOut.tileType == ADDR_NON_DISPLAYABLE)
        {
            data.tiling.gfx6.type = Developer::Gfx6ImageTileType::NonDisplayable;
        }
        else if (surfInfoOut.tileType == ADDR_DEPTH_SAMPLE_ORDER)
        {
            data.tiling.gfx6.type = Developer::Gfx6ImageTileType::DepthSampleOrder;
        }
        else if (surfInfoOut.tileType == ADDR_ROTATED)
        {
            data.tiling.gfx6.type = Developer::Gfx6ImageTileType::Rotated;
        }
        else if (surfInfoOut.tileType == ADDR_THICK)
        {
            data.tiling.gfx6.type = Developer::Gfx6ImageTileType::Thick;
        }

        data.flags.properties.color             = surfInfoIn.flags.color;
        data.flags.properties.depth             = surfInfoIn.flags.depth;
        data.flags.properties.stencil           = surfInfoIn.flags.stencil;
        data.flags.properties.texture           = surfInfoIn.flags.texture;
        data.flags.properties.cube              = surfInfoIn.flags.cube;
        data.flags.properties.volume            = surfInfoIn.flags.volume;
        data.flags.properties.fmask             = surfInfoIn.flags.fmask;
        data.flags.properties.compressZ         = surfInfoIn.flags.compressZ;
        data.flags.properties.overlay           = surfInfoIn.flags.overlay;
        data.flags.properties.noStencil         = surfInfoIn.flags.noStencil;
        data.flags.properties.display           = surfInfoIn.flags.display;
        data.flags.properties.opt4Space         = surfInfoIn.flags.opt4Space;
        data.flags.properties.prt               = surfInfoIn.flags.prt;
        data.flags.properties.tcCompatible      = surfInfoIn.flags.tcCompatible;
        data.flags.properties.dccCompatible     = surfInfoIn.flags.dccCompatible;
        data.flags.properties.dccPipeWorkaround = surfInfoIn.flags.dccPipeWorkaround;
        data.flags.properties.disableLinearOpt  = surfInfoIn.flags.disableLinearOpt;

        data.size   = surfInfoOut.surfSize;
        data.bpp    = surfInfoOut.bpp;
        data.width  = surfInfoOut.pitch;
        data.height = surfInfoOut.height;
        data.depth  = surfInfoOut.depth;

        GetDevice()->DeveloperCb(Developer::CallbackType::CreateImage, &data);
#endif
    }

    if (addrRet != ADDR_OK)
    {
        result = Result::ErrorUnknown;
    }
    else if (pSubResInfo->subresId.mipLevel == 0)
    {
        // Fail if we didn't satisfy the client's requested row and depth pitches.
        if ((imageCreateInfo.rowPitch != 0) && (pSubResInfo->rowPitch != imageCreateInfo.rowPitch))
        {
            result = Result::ErrorMismatchedImageRowPitch;
        }
        else if (imageCreateInfo.depthPitch != 0)
        {
            // For YUV image, imageCreateInfo.depthPitch includes both the Y and UV planes, while the
            // pSubResInfo->depthPitch is only covering either the Y or UV planes.
            if (((isYuvPlanar == true)  && (pSubResInfo->depthPitch >= imageCreateInfo.depthPitch)) ||
                ((isYuvPlanar == false) && (pSubResInfo->depthPitch != imageCreateInfo.depthPitch)))
            {
                result = Result::ErrorMismatchedImageDepthPitch;
            }
        }
    }

    return result;
}

} // AddrMgr1
} // Pal
