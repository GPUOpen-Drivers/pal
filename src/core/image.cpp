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

#include "core/addrMgr/addrMgr.h"
#include "core/device.h"
#include "core/g_palSettings.h"
#include "core/image.h"
#include "core/platform.h"
#include "addrinterface.h"
#include "palFormatInfo.h"
#include "palImage.h"

using namespace Util;
using namespace Pal::Formats;

namespace Pal
{

// =====================================================================================================================
// Helper function which computes the total number of planes for an Image.
static PAL_INLINE size_t PlaneCount(
    const Device&          device,
    const ImageCreateInfo& createInfo)
{
    size_t planes = 1;

    if (device.SupportsDepth(createInfo.swizzledFormat.format, ImageTiling::Optimal) &&
        device.SupportsStencil(createInfo.swizzledFormat.format, ImageTiling::Optimal))
    {
        planes = 2;
    }
    else if (IsYuvPlanar(createInfo.swizzledFormat.format))
    {
        planes = (createInfo.swizzledFormat.format == ChNumFormat::YV12) ? 3 : 2;
    }

    return planes;
}

// =====================================================================================================================
// Helper function which computes the total number of subresources for an Image.
static PAL_INLINE size_t TotalSubresourceCount(
    const Device&          device,
    const ImageCreateInfo& createInfo)
{
    return (PlaneCount(device, createInfo) * createInfo.arraySize * createInfo.mipLevels);
}

// =====================================================================================================================
Image::Image(
    Device*                        pDevice,
    void*                          pGfxImagePlacementAddr,
    void*                          pSubresInfoPlacementAddr,
    const ImageCreateInfo&         createInfo,
    const ImageInternalCreateInfo& internalCreateInfo)
    :
    IImage(createInfo),
    m_pDevice(pDevice),
    m_pGfxImage(static_cast<GfxImage*>(pGfxImagePlacementAddr)),
    m_pSubResInfoList(static_cast<SubResourceInfo*>(pSubresInfoPlacementAddr)),
    m_pTileInfoList(m_pSubResInfoList + TotalSubresourceCount(*pDevice, createInfo)),
    m_tileInfoBytes(pDevice->GetAddrMgr()->TileInfoBytes()),
    m_gpuMemSize(0),
    m_gpuMemAlignment(0),
    m_pPrivateScreen(nullptr),
    m_privateScreenImageId(0),
    m_privateScreenIndex(0)
{
    m_imageInfo.internalCreateInfo     = internalCreateInfo;
    m_imageInfo.resolveMethod.u32All   = 0;
    m_imageInfo.flags.u32All           = 0;

    m_imageInfo.flags.dccCompatibleFormatChange = m_pDevice->GetGfxDevice()->AreImageFormatsDccCompatible(createInfo);

    if (IsDepthStencil())
    {
        m_imageInfo.resolveMethod.shaderPs = 1;
        m_imageInfo.resolveMethod.depthStencilCopy = 1;
    }

    m_imageInfo.resolveMethod.shaderCs = 1;

    m_imageInfo.numPlanes       = PlaneCount(*pDevice, createInfo);
    m_imageInfo.numSubresources = TotalSubresourceCount(*pDevice, createInfo);

    memset(m_pSubResInfoList, 0, ((sizeof(SubResourceInfo) + m_tileInfoBytes) * m_imageInfo.numSubresources));

    // Initialize all layout fields to zero except for the alignments which must be one if they are unused and the
    // swizzle equation indices which should be set to invalid.
    memset(&m_gpuMemLayout, 0, sizeof(m_gpuMemLayout));

    m_gpuMemLayout.dataAlignment           = 1;
    m_gpuMemLayout.metadataAlignment       = 1;
    m_gpuMemLayout.metadataHeaderAlignment = 1;
    m_gpuMemLayout.swizzleEqIndices[0]     = InvalidSwizzleEqIndex;
    m_gpuMemLayout.swizzleEqIndices[1]     = InvalidSwizzleEqIndex;
}
static_assert(ADDR_TM_LINEAR_GENERAL == 0,
              "If ADDR_TM_LINEAR_GENERAL does not equal 0, the default in internalCreateInfo must be set to it.");

// =====================================================================================================================
// Creates and initializes a new instance of Image
Result Image::ValidateCreateInfo(
    const Device*                  pDevice,
    const ImageCreateInfo&         imageInfo,
    const ImageInternalCreateInfo& internalCreateInfo)
{
    Result ret = Result::Success;

    const bool shaderReadUsage      = (imageInfo.usageFlags.shaderRead != 0);
    const bool shaderWriteUsage     = (imageInfo.usageFlags.shaderWrite != 0);
    const bool colorUsage           = (imageInfo.usageFlags.colorTarget != 0);
    const bool depthStencilUsage    = (imageInfo.usageFlags.depthStencil != 0);
    const bool windowedPresentUsage = ((internalCreateInfo.flags.presentable != 0) && (imageInfo.flags.flippable == 0));
    const bool isYuvFormat          = IsYuv(imageInfo.swizzledFormat.format);

    // An image's format cannot be undefined.
    if (IsUndefined(imageInfo.swizzledFormat.format))
    {
        ret = Result::ErrorInvalidFormat;
    }

    // Check the image usage flags
    if (colorUsage && depthStencilUsage)
    {
        ret = Result::ErrorInvalidImageTargetUsage;
    }

    // Check MSAA compatibility
    if (ret == Result::Success)
    {
        if (imageInfo.samples > 1)
        {
            // MSAA images can only have 1 mip level
            if (imageInfo.mipLevels != 1)
            {
                ret = Result::ErrorInvalidMsaaMipLevels;
            }
            // Verify the image format to be compatible with MSAA
            else if (pDevice->SupportsMsaa(imageInfo.swizzledFormat.format, imageInfo.tiling) != true)
            {
                ret = Result::ErrorInvalidMsaaFormat;
            }
            // Verify MSAA is enabled only for 2D images.
            else if (imageInfo.imageType != ImageType::Tex2d)
            {
                ret = Result::ErrorInvalidMsaaType;
            }
            else if (depthStencilUsage && (imageInfo.samples != imageInfo.fragments))
            {
                ret = Result::ErrorInvalidSampleCount;
            }
        }
    }

    // Check image type and compressed image dimensions
    if (ret == Result::Success)
    {
        if ((imageInfo.imageType == ImageType::Tex1d) && IsBlockCompressed(imageInfo.swizzledFormat.format))
        {
            // 1D images cannot have a compressed format
            ret = Result::ErrorInvalidCompressedImageType;
        }
        // Check image properties and YUV format usage
        else if ((imageInfo.imageType != ImageType::Tex2d) && isYuvFormat)
        {
            // YUV formats are only supported for 2D Images.
            ret = Result::ErrorInvalidYuvImageType;
        }
    }

    // Check format
    if (ret == Result::Success)
    {
        const uint32 fmtSupport = pDevice->FeatureSupportFlags(imageInfo.swizzledFormat.format, imageInfo.tiling);

        if (TestAnyFlagSet(fmtSupport, (FormatFeatureImageShaderRead  |
                                        FormatFeatureImageShaderWrite |
                                        FormatFeatureCopy             |
                                        FormatFeatureColorTargetWrite |
                                        FormatFeatureDepthTarget      |
                                        FormatFeatureStencilTarget    |
                                        FormatFeatureWindowedPresent)) == false)
        {
            ret = Result::ErrorInvalidFormat;
        }
        // Verify a valid image format is specified for the given access flags.
        else if ((shaderReadUsage      && (TestAnyFlagSet(fmtSupport, FormatFeatureImageShaderRead)  == false)) ||
                 (shaderWriteUsage     && (TestAnyFlagSet(fmtSupport, FormatFeatureImageShaderWrite) == false)) ||
                 (colorUsage           && (TestAnyFlagSet(fmtSupport, FormatFeatureColorTargetWrite) == false)) ||
                 (depthStencilUsage    && (TestAnyFlagSet(fmtSupport, (FormatFeatureDepthTarget |
                                                                       FormatFeatureStencilTarget))  == false)) ||
                 (windowedPresentUsage && (TestAnyFlagSet(fmtSupport, FormatFeatureWindowedPresent) == false)))
        {
            ret = Result::ErrorFormatIncompatibleWithImageUsage;
        }
    }

    // Check array size
    if (ret == Result::Success)
    {
        if (imageInfo.imageType == ImageType::Tex3d)
        {
            // For 3D images, the array size must be 1
            if (imageInfo.arraySize != 1)
            {
                ret = Result::ErrorInvalid3dImageArraySize;
            }
        }
        else
        {
            // For 1D and 2D images, the array size can't be zero or greater than max array size.
            // Client must specify an array size of one for a non-array image.
            if ((imageInfo.arraySize == 0) || (imageInfo.arraySize > MaxArraySlices))
            {
                ret = Result::ErrorInvalidImageArraySize;
            }
        }
    }

    // Check image dimensions and mip levels
    if (ret == Result::Success)
    {
        uint32 maxDim = 0;

        static_assert(((static_cast<uint32>(ImageType::Tex2d) - 1) == static_cast<uint32>(ImageType::Tex1d)) &&
                      ((static_cast<uint32>(ImageType::Tex3d) - 1) == static_cast<uint32>(ImageType::Tex2d)),
                      "Image Type enum values are non-sequential");

        // The enum value will always be >= Tex1d
        if ((imageInfo.extent.width <= 0) || (imageInfo.extent.width > MaxImageDimension))
        {
            // 1D images ignore height and depth parameters
            ret = Result::ErrorInvalidImageWidth;
        }
        else
        {
            maxDim = imageInfo.extent.width;
        }

        if ((ret == Result::Success) &&
            (static_cast<uint32>(imageInfo.imageType) >= static_cast<uint32>(ImageType::Tex2d)))
        {
            if ((imageInfo.extent.height <= 0) || (imageInfo.extent.height > MaxImageDimension))
            {
                // 2D images ignore depth parameter
                ret = Result::ErrorInvalidImageHeight;
            }
            else
            {
                maxDim = Max(maxDim, imageInfo.extent.height);
            }
        }

        if ((ret == Result::Success) && (imageInfo.imageType == ImageType::Tex3d))
        {
            if ((imageInfo.extent.depth <= 0) || (imageInfo.extent.depth > MaxImageDimension))
            {
                // 3D images must have valid width / height / depth parameters
                ret = Result::ErrorInvalidImageDepth;
            }
            else
            {
                maxDim = Max(maxDim, imageInfo.extent.depth);
            }
        }

        // Verify the size of the mip-chain is valid for the given image type and format.
        if (ret == Result::Success)
        {
            if (imageInfo.mipLevels == 0)
            {
                ret = Result::ErrorInvalidMipCount;
            }
            else if ((maxDim >> (imageInfo.mipLevels - 1)) == 0)
            {
                ret = Result::ErrorInvalidMipCount;
            }
            else if ((imageInfo.mipLevels > 1) && isYuvFormat)
            {
                ret = Result::ErrorInvalidMipCount;
            }
        }
    }

    // The row and depth pitches can only be specified for linear images and must be used together.
    if (ret == Result::Success)
    {
        if (imageInfo.tiling == ImageTiling::Linear)
        {
            if ((imageInfo.rowPitch > 0) != (imageInfo.depthPitch > 0))
            {
                ret = Result::ErrorInvalidValue;
            }
        }
        else if ((imageInfo.rowPitch > 0) || (imageInfo.depthPitch > 0))
        {
            ret = Result::ErrorInvalidValue;
        }
    }

    // We can't support 3D depth/stencil images.
    if (ret == Result::Success)
    {
        if (depthStencilUsage && (imageInfo.imageType == ImageType::Tex3d))
        {
            ret = Result::ErrorInvalidValue;
        }
    }

    return ret;
}

// =====================================================================================================================
Result Image::ValidatePrivateCreateInfo(
    const Device*                       pDevice,
    const PrivateScreenImageCreateInfo& createInfo)
{
    Result result = Result::Success;

    PrivateScreen* pPrivateScreen = static_cast<PrivateScreen*>(createInfo.pScreen);

    if (pPrivateScreen == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (pPrivateScreen->FormatSupported(createInfo.swizzledFormat) == false)
    {
        result = Result::ErrorInvalidFormat;
    }

    return result;
}

// =====================================================================================================================
// Computes the size (bytes) of all subresource info and tiling info structures needed for an Image object corresponding
// to the specified creation info.
size_t Image::GetTotalSubresourceSize(
    const Device&          device,
    const ImageCreateInfo& createInfo)
{
    // Each subresource needs information describing its properties and its tiling properties as computed by AddrLib.
    const size_t perSubresourceSize = (sizeof(SubResourceInfo) + device.GetAddrMgr()->TileInfoBytes());

    return (TotalSubresourceCount(device, createInfo) * perSubresourceSize);
}

// =====================================================================================================================
// Helper method which determines the format and aspect for the specified Image plane.
void Image::DetermineFormatAndAspectForPlane(
    SwizzledFormat* pFormat,
    ImageAspect*    pAspect,
    uint32          plane
    ) const
{
    const SwizzledFormat format = m_createInfo.swizzledFormat;
    if (Formats::IsDepthStencilOnly(format.format) || (m_createInfo.usageFlags.depthStencil != 0))
    {
        // Subresource format gets overridden for depth/stencil Images:
        *pAspect         = IsUint(format.format) ? ImageAspect::Stencil : ImageAspect::Depth;
        pFormat->format  = format.format;
        pFormat->swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        //  Subresource planes of depth/stencil formats like D16_S8, D32_8.
        if (m_imageInfo.numPlanes > 1)
        {
            if (plane > 0)
            {
                pFormat->format = ChNumFormat::X8_Uint;
                *pAspect        = ImageAspect::Stencil;
            }
            // Depth plane of formats D16S8, D32S8.
            else if (format.format == ChNumFormat::D16_Unorm_S8_Uint)
            {
                pFormat->format = ChNumFormat::X16_Unorm;
                *pAspect        = ImageAspect::Depth;
            }
            else
            {
                PAL_ASSERT(format.format == ChNumFormat::D32_Float_S8_Uint);
                pFormat->format = ChNumFormat::X32_Float;
                *pAspect        = ImageAspect::Depth;
            }
        }
    }
    else if (IsYuvPacked(format.format))
    {
        *pAspect = ImageAspect::YCbCr;
        *pFormat = m_createInfo.swizzledFormat;
    }
    else if (IsYuvPlanar(format.format))
    {
        if (plane == 0)
        {
            *pAspect         = ImageAspect::Y;
            pFormat->swizzle =
                { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };
            switch (format.format)
            {
            case ChNumFormat::P016:
            case ChNumFormat::P010:
                pFormat->format  = ChNumFormat::X16_Uint;
                break;
            default:
                pFormat->format  = ChNumFormat::X8_Uint;
                break;
            }
        }
        else
        {
            switch (format.format)
            {
            case ChNumFormat::NV11:
            case ChNumFormat::NV12:
            case ChNumFormat::NV21:
                pFormat->format  = ChNumFormat::X8Y8_Uint;
                pFormat->swizzle =
                    { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };
                *pAspect = ImageAspect::CbCr;
                break;
            case ChNumFormat::P016:
            case ChNumFormat::P010:
                pFormat->format  = ChNumFormat::X16Y16_Uint;
                pFormat->swizzle =
                    { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };
                *pAspect = ImageAspect::CbCr;
                break;
            case ChNumFormat::YV12:
                pFormat->format  = ChNumFormat::X8_Uint;
                pFormat->swizzle =
                    { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };
                *pAspect = (plane == 1) ? ImageAspect::Cb : ImageAspect::Cr;
                break;
            default:
                PAL_ASSERT_ALWAYS();
                break;
            }
        }
    }
    else
    {
        *pAspect = ImageAspect::Color;
        *pFormat = m_createInfo.swizzledFormat;
    }
}

// =====================================================================================================================
// Initializes the Image's subresources and any metadata surfaces needed by the GfxIp hardware layer.
Result Image::Init()
{
    // First, initialize some properties of each subresource which we know without delegating to the AddrMgr or to the
    // GfxImage object.
    SubResourceInfo* pSubRes = m_pSubResInfoList;
    for (uint32 plane = 0; plane < m_imageInfo.numPlanes; ++plane)
    {
        SwizzledFormat planeFormat = m_createInfo.swizzledFormat;
        ImageAspect    planeAspect = ImageAspect::Color;
        DetermineFormatAndAspectForPlane(&planeFormat, &planeAspect, plane);

        // For YUV planar formats, the base subresource dimensions vary by plane. We need to determine the ratio of the
        // planes' dimentions.
        const Extent3d log2Ratio = Log2SubsamplingRatio(m_createInfo.swizzledFormat.format, planeAspect);

        uint32 mipWidth  = (m_createInfo.extent.width  >> log2Ratio.width);
        uint32 mipHeight = (m_createInfo.extent.height >> log2Ratio.height);
        uint32 mipDepth  = (m_createInfo.extent.depth  >> log2Ratio.depth);

        for (uint32 mipLevel = 0; mipLevel < m_createInfo.mipLevels; ++mipLevel)
        {
            for (uint32 slice = 0; slice < m_createInfo.arraySize; ++slice, ++pSubRes)
            {
                pSubRes->subresId.aspect     = planeAspect;
                pSubRes->subresId.arraySlice = slice;
                pSubRes->subresId.mipLevel   = mipLevel;
                pSubRes->format              = planeFormat;
                pSubRes->extentTexels.width  = Max(1u, mipWidth);
                pSubRes->extentTexels.height = Max(1u, mipHeight);
                pSubRes->extentTexels.depth  = Max(1u, mipDepth);
                pSubRes->bitsPerTexel        = Formats::BitsPerPixel(pSubRes->format.format);
                pSubRes->clearMethod         = DefaultSlowClearMethod;
            }

            mipWidth  >>= 1;
            mipHeight >>= 1;
            mipDepth  >>= 1;
        }
    }

    // Create the GfxImage object, we've already accounted for the size of the object in GetSize so we can just
    // place the object after this Image object
    m_pDevice->GetGfxDevice()->CreateImage(this, &m_imageInfo, m_pGfxImage, &m_pGfxImage);

    // Initialize all of our subresources using the AddrMgr. We also need to track whether any of the subresources are
    // unable to support DCC, because some hardware needs to disable DCC for an entire Image if any of the subresources
    // cannot use DCC compression.
    bool dccUnsupported = false;
    Result result = Result::Success;
    if (m_imageInfo.internalCreateInfo.pOriginalImage == nullptr)
    {
        // Default: use the local device's AddrMgr.
        result = m_pDevice->GetAddrMgr()->InitSubresourcesForImage(this,
                                                                   &m_gpuMemSize,
                                                                   &m_gpuMemAlignment,
                                                                   &m_gpuMemLayout,
                                                                   m_pSubResInfoList,
                                                                   m_pTileInfoList,
                                                                   &dccUnsupported);
    }
    else
    {
        // Open a peer image: use the remote device's AddrMgr.
        // Example: Apu(Bristol) + dGpu(Polaris11). Polaris11 opens a 2D_THIN1 primary created by Bristol. Polaris11
        // peer-to-peer transfers to this opened allocation via Sdma.
        // If using m_pDevice, the tile info output is
        // banks = 3, bankWidth = 0, bankHeight = 2, macroAspectRatio = 2, tileSplitBytes = 3, pipeConfig = 5
        // Then DmaCmdBuffer::WriteCopyImageTiledToTiledCmd() uses these data to setup the output config for the copy
        // cmd. This is wrong. The correct way is to use Bristol's config which is
        // banks = 2, bankWidth = 0, bankHeight = 0, macroAspectRatio = 1, tileSplitBytes = 3, pipeConfig = 0
        Device* pRemoteDevice = m_imageInfo.internalCreateInfo.pOriginalImage->GetDevice();
        result = pRemoteDevice->GetAddrMgr()->InitSubresourcesForImage(this,
                                                                       &m_gpuMemSize,
                                                                       &m_gpuMemAlignment,
                                                                       &m_gpuMemLayout,
                                                                       m_pSubResInfoList,
                                                                       m_pTileInfoList,
                                                                       &dccUnsupported);
    }

    if (result == Result::Success)
    {
        // We've finished computing the subresource info so we have enough information to validate the
        // swizzle equations.
        if ((m_createInfo.flags.needSwizzleEqs == 1) &&
            ((m_gpuMemLayout.swizzleEqIndices[0] == InvalidSwizzleEqIndex) ||
             (m_gpuMemLayout.swizzleEqIndices[1] == InvalidSwizzleEqIndex)))
        {
            // The client requires valid swizzle equations so this is a failure case.
            result = Result::ErrorInitializationFailed;
        }
        else
        {
            m_gpuMemAlignment = SubresourceInfo(0)->baseAlign;

            // PRT images need to have their data size aligned, otherwise mapping/unmapping the last PRT tile might
            // overrun any Image metadata that follows.
            if (m_createInfo.flags.prt != 0)
            {
                m_gpuMemSize = RoundUpToMultiple(m_gpuMemSize, m_gpuMemAlignment);
            }

            // Save out the data section's size and alignment before continuing. Note that dataAlignment may be less
            // strict than the final value of m_gpuMemAlignment because it is intended to be independent of the
            // metadata requirements.
            m_gpuMemLayout.dataSize      = m_gpuMemSize;
            m_gpuMemLayout.dataAlignment = m_gpuMemAlignment;
        }

        if (result == Result::Success)
        {
            // The extentTexels.height of subresource 0 is different with the extent in the image create info, and this will
            // cause all sorts of problems because there are many places where PAL (and our clients) assume that the extent
            // in the image create info matches the first subresource's extentTexels. So we set the create info's height to
            // the extentTexels.height of subresource zero when we have a stereo image.
            if (m_createInfo.flags.stereo == 1)
            {
                const_cast<ImageCreateInfo&>(m_createInfo).extent.height = m_pSubResInfoList->extentTexels.height;
            }

            // Finalize the GfxIp Image sub-object, which will set up data structures for things like compression metadata,
            // as well as updating the GPU memory size and alignment requirements for this Image.
            result = GetGfxImage()->Finalize(dccUnsupported,
                                             m_pSubResInfoList,
                                             m_pTileInfoList,
                                             &m_gpuMemLayout,
                                             &m_gpuMemSize,
                                             &m_gpuMemAlignment);

            if (result == Result::ErrorNotShareable)
            {
                // This image is going to be re-created without shared metadata info, so the creator needs to be notified
                // that metadata should be fully expanded.
                SetOptimalSharingLevel(MetadataSharingLevel::FullExpand);
            }
        }
    }

    if (result == Result::Success)
    {
        // All three layout sections must fit within the GPU memory size we've calculated. This must be an inequality
        // because inter-section alignment padding may not be included in the layout sizes.
        PAL_ASSERT((m_gpuMemLayout.dataSize     +
                    m_gpuMemLayout.metadataSize +
                    m_gpuMemLayout.metadataHeaderSize) <= m_gpuMemSize);

        if ((m_createInfo.maxBaseAlign > 0) && (m_gpuMemAlignment > m_createInfo.maxBaseAlign))
        {
            // If the client gave us a non-zero maxBaseAlign, they require that our alignment not exceed it.
            result = Result::ErrorInitializationFailed;
        }
    }

    return result;
}

// =====================================================================================================================
void Image::Destroy()
{
    if (m_pPrivateScreen != nullptr)
    {
        m_pPrivateScreen->ReturnImageId(m_privateScreenImageId);
    }

    this->~Image();
}

// =====================================================================================================================
// Destroys an internally created Image object
void Image::DestroyInternal()
{
    Platform*const pPlatform = m_pDevice->GetPlatform();
    Destroy();
    PAL_FREE(this, pPlatform);
}

// =====================================================================================================================
// Determines if the specified image aspect is valid.
bool Image::IsAspectValid(
    ImageAspect  aspect
    ) const
{
    ChNumFormat format = m_createInfo.swizzledFormat.format;
    bool        valid  = false;

    // Determine depth/stencil support
    if (IsDepthStencilOnly(format) ||
        ((aspect == ImageAspect::Depth)   && m_pDevice->SupportsDepth(format, m_createInfo.tiling)) ||
        ((aspect == ImageAspect::Stencil) && m_pDevice->SupportsStencil(format, m_createInfo.tiling)))
    {
        valid = true;
    }
    else if (IsYuv(format))
    {
        switch (aspect)
        {
        case ImageAspect::Y:
            valid = IsYuvPlanar(format);
            break;
        case ImageAspect::CbCr:
            valid = (IsYuvPlanar(format) && (format != ChNumFormat::YV12));
            break;
        case ImageAspect::Cb:
        case ImageAspect::Cr:
            valid = (format == ChNumFormat::YV12);
            break;
        case ImageAspect::YCbCr:
            valid = IsYuvPacked(format);
            break;
        default:
            break;
        }
    }
    // For non YUV, non depth/stencil Images, the only valid aspect we support is Color.
    else if (aspect == ImageAspect::Color)
    {
        valid = true;
    }
    // TODO: Revisit when implementing advanced AA
    else if ((aspect == ImageAspect::Fmask) && (m_createInfo.samples > 1))
    {
        valid = true;
    }
    else if (m_createInfo.tiling == ImageTiling::Linear)
    {
        // Depth and Stencil aspects are valid for linear images that share the same format as images
        // that support DepthTarget.  A better solution would be to introduce an ImageAspect::Default.
        if ((aspect == ImageAspect::Depth) &&
            ((format == ChNumFormat::X16_Unorm) || (format == ChNumFormat::X32_Float)) &&
            m_pDevice->SupportsDepth(format, ImageTiling::Optimal))
        {
            valid = true;
        }
        else if ((aspect == ImageAspect::Stencil)  &&
                 (format == ChNumFormat::X8_Unorm) &&
                 m_pDevice->SupportsStencil(format, ImageTiling::Optimal))
        {
            valid = true;
        }

    }

    return valid;
}

// =====================================================================================================================
// Calculates the subresource id according to array slice, mip level and aspect.
uint32 Image::CalcSubresourceId(
    const SubresId& subresource
    ) const
{
    const uint32 plane = GetPlaneFromAspect(subresource.aspect);

    PAL_ASSERT((subresource.arraySlice < m_createInfo.arraySize) &&
               (subresource.mipLevel   < m_createInfo.mipLevels) &&
               (plane                  < m_imageInfo.numPlanes));

    const uint32 subresInPlane  = ((subresource.mipLevel * m_createInfo.arraySize) + subresource.arraySlice);
    const uint32 subresPerPlane = (m_createInfo.mipLevels * m_createInfo.arraySize);

    // Subresources are placed in subresource-major order, i.e. all subresources of plane N preceed all subresources of
    // plane N+1 in memory.
    return ((plane * subresPerPlane) + subresInPlane);
}

// =====================================================================================================================
// Fills in a subresource range to cover all subresources for the given aspect
void Image::GetFullSubresourceRange(
    ImageAspect  aspect,
    SubresRange* pRange          // [out] Subresource range being returned
    ) const
{
    if ((pRange != nullptr) && (IsAspectValid(aspect)))
    {
        pRange->startSubres.aspect     = aspect;
        pRange->startSubres.mipLevel   = 0;
        pRange->startSubres.arraySlice = 0;
        pRange->numMips                = m_createInfo.mipLevels;
        pRange->numSlices              = m_createInfo.arraySize;
    }
}

// =====================================================================================================================
// Sets up a sub-resource struct for the base mip/array with an appropriate aspect for the image type.  Biased towards
// depth aspect for depth/stencil compatabile images.
SubresId Image::GetBaseSubResource() const
{
    SubresId subRes = { };

    if (IsDepthStencil() ||
        IsDepthStencilOnly(m_createInfo.swizzledFormat.format))
    {
        if (IsAspectValid(ImageAspect::Depth) == true)
        {
            subRes.aspect = ImageAspect::Depth;
        }
        else if (IsAspectValid(ImageAspect::Stencil) == true)
        {
            subRes.aspect = ImageAspect::Stencil;
        }
        else
        {
            // How do we have a depth image that doesn't have either a depth or stencil aspect?
            PAL_ASSERT_ALWAYS();
        }
    }
    else if (IsAspectValid(ImageAspect::YCbCr))
    {
        subRes.aspect = ImageAspect::YCbCr;
    }
    else if (IsAspectValid(ImageAspect::Y))
    {
        subRes.aspect = ImageAspect::Y;
    }
    else
    {
        subRes.aspect = ImageAspect::Color;
    }

    return subRes;
}

// =====================================================================================================================
// Determines the memory requirements for this image.
void Image::GetGpuMemoryRequirements(
    GpuMemoryRequirements* pMemReqs     // [in,out] returns with populated 'heap' info
    ) const
{
    pMemReqs->size      = m_gpuMemSize;
    pMemReqs->alignment = m_gpuMemAlignment;

    if (m_createInfo.flags.shareable)
    {
        pMemReqs->heapCount = 2;
        pMemReqs->heaps[0]  = GpuHeapGartUswc;
        pMemReqs->heaps[1]  = GpuHeapGartCacheable;
    }
    else
    {
        pMemReqs->heapCount = 4;
        pMemReqs->heaps[0]  = GpuHeapInvisible;
        pMemReqs->heaps[1]  = GpuHeapLocal;
        pMemReqs->heaps[2]  = GpuHeapGartUswc;
        pMemReqs->heaps[3]  = GpuHeapGartCacheable;
    }

    GetGfxImage()->OverrideGpuMemHeaps(pMemReqs);
}

// =====================================================================================================================
// Determines which plane in the Image is associated with the specified image aspect.
uint32 Image::GetPlaneFromAspect(
    ImageAspect  aspect
    ) const
{
    PAL_ASSERT(IsAspectValid(aspect));

    // Most of the time, the aspect is tied to plane #0...
    uint32 plane = 0;
    // ...but if we have two planes and are a depth/stencil Image, then the stencil aspect is tied to plane #1. Or,
    // if we are a YUV planar Image, the chroma aspect(s) are tied to plane #1 and #2.
    switch (m_imageInfo.numPlanes)
    {
    case 2:
        if ((aspect == ImageAspect::Stencil) || (aspect == ImageAspect::CbCr))
        {
            plane = 1;
        }
        break;
    case 3:
        if (aspect == ImageAspect::Cb)
        {
            plane = 1;
        }
        else if (aspect == ImageAspect::Cr)
        {
            plane = 2;
        }
        break;
    }

    return plane;
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 365
// =====================================================================================================================
// Fills in the ImageMemoryLayout struct with info for this image.
Result Image::GetMemoryLayout(
    ImageMemoryLayout* pLayout // [out] Memory layout information
    ) const
{
    Result result = Result::ErrorInvalidValue;

    if (pLayout != nullptr)
    {
        *pLayout = m_gpuMemLayout;
        result   = Result::Success;
    }

    return result;
}
#endif
// =====================================================================================================================
// Fills in the SubresLayout struct with info for the subresource specified
Result Image::GetSubresourceLayout(
    SubresId      subresId,
    SubresLayout* pLayout       // [out] Subresource layout information
    ) const
{
    Result ret = Result::ErrorInvalidValue;

    if (pLayout != nullptr)
    {
        const SubResourceInfo*const pSubResInfo = SubresourceInfo(subresId);

        pLayout->offset        = pSubResInfo->offset;
        pLayout->swizzleOffset = pSubResInfo->swizzleOffset;
        pLayout->size          = pSubResInfo->size;
        pLayout->rowPitch      = pSubResInfo->rowPitch;
        pLayout->depthPitch    = pSubResInfo->depthPitch;
        pLayout->tileToken     = pSubResInfo->tileToken;
        pLayout->tileSwizzle   = m_pDevice->GetAddrMgr()->GetTileSwizzle(this, subresId);
        pLayout->blockSize     = pSubResInfo->blockSize;
        pLayout->paddedExtent  = pSubResInfo->actualExtentElements;

        ret = Result::Success;
    }

    return ret;
}

// =====================================================================================================================
Result Image::BindGpuMemory(
    IGpuMemory* pGpuMemory,
    gpusize     offset)
{
    Result ret = m_pDevice->ValidateBindObjectMemoryInput(pGpuMemory,
                                                          offset,
                                                          m_gpuMemSize,
                                                          m_gpuMemAlignment,
                                                          true);

    if (ret == Result::Success)
    {
        // Flippable images should always be bound to flippable memory.  As an exception, it is OK to be bound to a
        // virtual GPU memory object, but it is the clients responsibility to ensure the virtual image is exclusively
        // pointing to flippable memory.
        PAL_ASSERT((pGpuMemory == nullptr) ||
                   (static_cast<GpuMemory*>(pGpuMemory)->IsFlippable() == IsFlippable()) ||
                   static_cast<GpuMemory*>(pGpuMemory)->IsVirtual());

        m_vidMem.Update(pGpuMemory, offset);
    }

    UpdateMetaDataInfo(pGpuMemory);
    return ret;
}

// =====================================================================================================================
// Gets AddrLib format enum from Format
AddrFormat Image::GetAddrFormat(
    const ChNumFormat format)
{
    AddrFormat ret = ADDR_FMT_INVALID;

    switch (format)
    {
        case ChNumFormat::X1_Unorm:
        case ChNumFormat::X1_Uscaled:
            ret = ADDR_FMT_1;
            break;
        case ChNumFormat::X4Y4_Unorm:
        case ChNumFormat::X4Y4_Uscaled:
        case ChNumFormat::L4A4_Unorm:
            ret = ADDR_FMT_4_4;
            break;
        case ChNumFormat::X4Y4Z4W4_Unorm:
        case ChNumFormat::X4Y4Z4W4_Uscaled:
            ret = ADDR_FMT_4_4_4_4;
            break;
        case ChNumFormat::X5Y6Z5_Unorm:
        case ChNumFormat::X5Y6Z5_Uscaled:
            ret = ADDR_FMT_5_6_5;
            break;
        case ChNumFormat::X5Y5Z5W1_Unorm:
        case ChNumFormat::X5Y5Z5W1_Uscaled:
            ret = ADDR_FMT_1_5_5_5;
            break;
        case ChNumFormat::X8_Unorm:
        case ChNumFormat::X8_Snorm:
        case ChNumFormat::X8_Uscaled:
        case ChNumFormat::X8_Sscaled:
        case ChNumFormat::X8_Uint:
        case ChNumFormat::X8_Sint:
        case ChNumFormat::X8_Srgb:
        case ChNumFormat::A8_Unorm:
        case ChNumFormat::L8_Unorm:
        case ChNumFormat::P8_Uint:
            ret = ADDR_FMT_8;
            break;
        case ChNumFormat::X8Y8_Unorm:
        case ChNumFormat::X8Y8_Snorm:
        case ChNumFormat::X8Y8_Uscaled:
        case ChNumFormat::X8Y8_Sscaled:
        case ChNumFormat::X8Y8_Uint:
        case ChNumFormat::X8Y8_Sint:
        case ChNumFormat::X8Y8_Srgb:
        case ChNumFormat::L8A8_Unorm:
            ret = ADDR_FMT_8_8;
            break;
        case ChNumFormat::X8Y8Z8W8_Unorm:
        case ChNumFormat::X8Y8Z8W8_Snorm:
        case ChNumFormat::X8Y8Z8W8_Uscaled:
        case ChNumFormat::X8Y8Z8W8_Sscaled:
        case ChNumFormat::X8Y8Z8W8_Uint:
        case ChNumFormat::X8Y8Z8W8_Sint:
        case ChNumFormat::X8Y8Z8W8_Srgb:
        case ChNumFormat::AYUV:
            ret = ADDR_FMT_8_8_8_8;
            break;
        case ChNumFormat::X8Y8_Z8Y8_Unorm:
        case ChNumFormat::X8Y8_Z8Y8_Uscaled:
        case ChNumFormat::UYVY:
        case ChNumFormat::VYUY:
            ret = ADDR_FMT_GB_GR;
            break;
        case ChNumFormat::Y8X8_Y8Z8_Unorm:
        case ChNumFormat::Y8X8_Y8Z8_Uscaled:
        case ChNumFormat::YUY2:
        case ChNumFormat::YVY2:
            ret = ADDR_FMT_BG_RG;
            break;
        case ChNumFormat::X10Y11Z11_Float:
            ret = ADDR_FMT_11_11_10_FLOAT;
            break;
        case ChNumFormat::X11Y11Z10_Float:
            ret = ADDR_FMT_10_11_11_FLOAT;
            break;
        case ChNumFormat::X10Y10Z10W2_Unorm:
        case ChNumFormat::X10Y10Z10W2_Uscaled:
        case ChNumFormat::X10Y10Z10W2_Uint:
            ret = ADDR_FMT_2_10_10_10;
            break;
        case ChNumFormat::X16_Unorm:
        case ChNumFormat::X16_Snorm:
        case ChNumFormat::X16_Uscaled:
        case ChNumFormat::X16_Sscaled:
        case ChNumFormat::X16_Uint:
        case ChNumFormat::X16_Sint:
        case ChNumFormat::L16_Unorm:
            ret = ADDR_FMT_16;
            break;
        case ChNumFormat::X16_Float:
            ret = ADDR_FMT_16_FLOAT;
            break;
        case ChNumFormat::X16Y16_Unorm:
        case ChNumFormat::X16Y16_Snorm:
        case ChNumFormat::X16Y16_Uscaled:
        case ChNumFormat::X16Y16_Sscaled:
        case ChNumFormat::X16Y16_Uint:
        case ChNumFormat::X16Y16_Sint:
            ret = ADDR_FMT_16_16;
            break;
        case ChNumFormat::X16Y16_Float:
            ret = ADDR_FMT_16_16_FLOAT;
            break;
        case ChNumFormat::X16Y16Z16W16_Unorm:
        case ChNumFormat::X16Y16Z16W16_Snorm:
        case ChNumFormat::X16Y16Z16W16_Uscaled:
        case ChNumFormat::X16Y16Z16W16_Sscaled:
        case ChNumFormat::X16Y16Z16W16_Uint:
        case ChNumFormat::X16Y16Z16W16_Sint:
            ret = ADDR_FMT_16_16_16_16;
            break;
        case ChNumFormat::X16Y16Z16W16_Float:
            ret = ADDR_FMT_16_16_16_16_FLOAT;
            break;
        case ChNumFormat::X32_Uint:
        case ChNumFormat::X32_Sint:
            ret = ADDR_FMT_32;
            break;
        case ChNumFormat::X32_Float:
            ret = ADDR_FMT_32_FLOAT;
            break;
        case ChNumFormat::X32Y32_Uint:
        case ChNumFormat::X32Y32_Sint:
            ret = ADDR_FMT_32_32;
            break;
        case ChNumFormat::X32Y32_Float:
            ret = ADDR_FMT_32_32_FLOAT;
            break;
        case ChNumFormat::X32Y32Z32_Uint:
        case ChNumFormat::X32Y32Z32_Sint:
            ret = ADDR_FMT_32_32_32;
            break;
        case ChNumFormat::X32Y32Z32_Float:
            ret = ADDR_FMT_32_32_32_FLOAT;
            break;
        case ChNumFormat::X32Y32Z32W32_Uint:
        case ChNumFormat::X32Y32Z32W32_Sint:
            ret = ADDR_FMT_32_32_32_32;
            break;
        case ChNumFormat::X32Y32Z32W32_Float:
            ret = ADDR_FMT_32_32_32_32_FLOAT;
            break;
        case ChNumFormat::X9Y9Z9E5_Float:
            ret = ADDR_FMT_5_9_9_9_SHAREDEXP;
            break;
        case ChNumFormat::Bc1_Unorm:
        case ChNumFormat::Bc1_Srgb:
            ret = ADDR_FMT_BC1;
            break;
        case ChNumFormat::Bc2_Unorm:
        case ChNumFormat::Bc2_Srgb:
            ret = ADDR_FMT_BC2;
            break;
        case ChNumFormat::Bc3_Unorm:
        case ChNumFormat::Bc3_Srgb:
            ret = ADDR_FMT_BC3;
            break;
        case ChNumFormat::Bc4_Unorm:
        case ChNumFormat::Bc4_Snorm:
            ret = ADDR_FMT_BC4;
            break;
        case ChNumFormat::Bc5_Unorm:
        case ChNumFormat::Bc5_Snorm:
            ret = ADDR_FMT_BC5;
            break;
        case ChNumFormat::Bc6_Ufloat:
        case ChNumFormat::Bc6_Sfloat:
            ret = ADDR_FMT_BC6;
            break;
        case ChNumFormat::Bc7_Unorm:
        case ChNumFormat::Bc7_Srgb:
            ret = ADDR_FMT_BC7;
            break;

        case ChNumFormat::Etc2X11_Unorm:
        case ChNumFormat::Etc2X11_Snorm:
        case ChNumFormat::Etc2X11Y11_Unorm:
        case ChNumFormat::Etc2X11Y11_Snorm:
        case ChNumFormat::Etc2X8Y8Z8_Unorm:
        case ChNumFormat::Etc2X8Y8Z8_Srgb:
        case ChNumFormat::Etc2X8Y8Z8W1_Unorm:
        case ChNumFormat::Etc2X8Y8Z8W1_Srgb:
        case ChNumFormat::Etc2X8Y8Z8W8_Unorm:
        case ChNumFormat::Etc2X8Y8Z8W8_Srgb:
            ret = ((Formats::BitsPerPixel(format) == 64)
                    ? ADDR_FMT_ETC2_64BPP
                    : ADDR_FMT_ETC2_128BPP);
            break;

        case ChNumFormat::AstcLdr4x4_Unorm:
        case ChNumFormat::AstcLdr4x4_Srgb:
        case ChNumFormat::AstcHdr4x4_Float:
            ret = ADDR_FMT_ASTC_4x4;
            break;

        case ChNumFormat::AstcLdr5x4_Unorm:
        case ChNumFormat::AstcLdr5x4_Srgb:
        case ChNumFormat::AstcHdr5x4_Float:
            ret = ADDR_FMT_ASTC_5x4;
            break;

        case ChNumFormat::AstcLdr5x5_Unorm:
        case ChNumFormat::AstcLdr5x5_Srgb:
        case ChNumFormat::AstcHdr5x5_Float:
            ret = ADDR_FMT_ASTC_5x5;
            break;

        case ChNumFormat::AstcLdr6x5_Unorm:
        case ChNumFormat::AstcLdr6x5_Srgb:
        case ChNumFormat::AstcHdr6x5_Float:
            ret = ADDR_FMT_ASTC_6x5;
            break;

        case ChNumFormat::AstcLdr6x6_Unorm:
        case ChNumFormat::AstcLdr6x6_Srgb:
        case ChNumFormat::AstcHdr6x6_Float:
            ret = ADDR_FMT_ASTC_6x6;
            break;

        case ChNumFormat::AstcLdr8x5_Unorm:
        case ChNumFormat::AstcLdr8x5_Srgb:
        case ChNumFormat::AstcHdr8x5_Float:
            ret = ADDR_FMT_ASTC_8x5;
            break;

        case ChNumFormat::AstcLdr8x6_Unorm:
        case ChNumFormat::AstcLdr8x6_Srgb:
        case ChNumFormat::AstcHdr8x6_Float:
            ret = ADDR_FMT_ASTC_8x6;
            break;

        case ChNumFormat::AstcLdr8x8_Unorm:
        case ChNumFormat::AstcLdr8x8_Srgb:
        case ChNumFormat::AstcHdr8x8_Float:
            ret = ADDR_FMT_ASTC_8x8;
            break;

        case ChNumFormat::AstcLdr10x5_Unorm:
        case ChNumFormat::AstcLdr10x5_Srgb:
        case ChNumFormat::AstcHdr10x5_Float:
            ret = ADDR_FMT_ASTC_10x5;
            break;

        case ChNumFormat::AstcLdr10x6_Unorm:
        case ChNumFormat::AstcLdr10x6_Srgb:
        case ChNumFormat::AstcHdr10x6_Float:
            ret = ADDR_FMT_ASTC_10x6;
            break;

        case ChNumFormat::AstcLdr10x8_Unorm:
        case ChNumFormat::AstcLdr10x8_Srgb:
        case ChNumFormat::AstcHdr10x8_Float:
            ret = ADDR_FMT_ASTC_10x8;
            break;

        case ChNumFormat::AstcLdr10x10_Unorm:
        case ChNumFormat::AstcLdr10x10_Srgb:
        case ChNumFormat::AstcHdr10x10_Float:
            ret = ADDR_FMT_ASTC_10x10;
            break;

        case ChNumFormat::AstcLdr12x10_Unorm:
        case ChNumFormat::AstcLdr12x10_Srgb:
        case ChNumFormat::AstcHdr12x10_Float:
            ret = ADDR_FMT_ASTC_12x10;
            break;

        case ChNumFormat::AstcLdr12x12_Unorm:
        case ChNumFormat::AstcLdr12x12_Srgb:
        case ChNumFormat::AstcHdr12x12_Float:
            ret = ADDR_FMT_ASTC_12x12;
            break;

        default:
            ret = ADDR_FMT_INVALID;
            break;
    }

    return ret;
}

// =====================================================================================================================
// Creates private screen presentable image. A private screen presentable image is similar to a regular presentable
// image but can only be presented on the private screens. It has some implicit properties relative to standard images,
// such as mipLevels=1, arraySize=1, numSamples=1 and etc. It also requires its bound GPU memory to be pinned before
// presenting.
Result Image::CreatePrivateScreenImage(
    Device*                             pDevice,
    const PrivateScreenImageCreateInfo& createInfo,
    void*                               pImagePlacementAddr,
    void*                               pGpuMemoryPlacementAddr,
    IImage**                            ppImage,
    IGpuMemory**                        ppGpuMemory)
{
    ImageCreateInfo imgInfo = {};
    ConvertPrivateScreenImageCreateInfo(createInfo, &imgInfo);

    ImageInternalCreateInfo internalInfo = {};
    internalInfo.flags.privateScreenPresent = 1;

    PrivateScreen* pPrivateScreen = static_cast<PrivateScreen*>(createInfo.pScreen);
    PAL_ASSERT(pPrivateScreen != nullptr);

    uint32 imageId = 0;
    Result result  = pPrivateScreen->ObtainImageId(&imageId);

    if (result == Result::Success)
    {
        Image* pImage = nullptr;
        result = pDevice->CreateInternalImage(imgInfo,
                                              internalInfo,
                                              pImagePlacementAddr,
                                              reinterpret_cast<Pal::Image**>(&pImage));

        if (result == Result::Success)
        {
            PAL_ASSERT(pImage != nullptr);
            pImage->SetPrivateScreen(pPrivateScreen);
            pImage->SetPrivateScreenImageId(imageId);

            result = CreatePrivateScreenImageMemoryObject(pDevice, pImage, pGpuMemoryPlacementAddr, ppGpuMemory);

            if (result != Result::Success)
            {
                // Destroy the image if memory creation failed
                pImage->Destroy();
                pImage = nullptr;
            }
            else
            {
                pPrivateScreen->SetImageSlot(imageId, pImage);
            }

            (*ppImage) = pImage;
        }
    }

    return result;
}

// =====================================================================================================================
// Creates the GPU memory object and binds it to the provided private screen image
Result Image::CreatePrivateScreenImageMemoryObject(
    Device*      pDevice,
    IImage*      pImage,
    void*        pGpuMemoryPlacementAddr,
    IGpuMemory** ppGpuMemOut)
{
    Pal::Image* pImg = static_cast<Pal::Image*>(pImage);

    GpuMemoryRequirements memReqs = { };
    pImg->GetGpuMemoryRequirements(&memReqs);

    const gpusize allocGranularity = pDevice->MemoryProperties().realMemAllocGranularity;

    GpuMemoryCreateInfo createInfo = { };
    createInfo.size      = Pow2Align(memReqs.size, allocGranularity);
    createInfo.alignment = Pow2Align(memReqs.alignment, allocGranularity);
    createInfo.vaRange   = VaRange::Default;
    createInfo.priority  = GpuMemPriority::VeryHigh;
    createInfo.heapCount = memReqs.heapCount;
    createInfo.pImage    = pImage;

    for (uint32 i = 0; i < memReqs.heapCount; i++)
    {
        createInfo.heaps[i] = memReqs.heaps[i];
    }

    GpuMemoryInternalCreateInfo internalInfo = { };
    internalInfo.flags.privateScreen = 1;

    Pal::GpuMemory* pMemObject = nullptr;
    Result result = pDevice->CreateInternalGpuMemory(createInfo, internalInfo, pGpuMemoryPlacementAddr, &pMemObject);

    if (result == Result::Success)
    {
        (*ppGpuMemOut) = pMemObject;
        result = pImg->BindGpuMemory((*ppGpuMemOut), 0);
    }

    return result;
}

// =====================================================================================================================
// Helper function to convert PrivateScreenCreateInfo to ImageCreateInfo.
void ConvertPrivateScreenImageCreateInfo(
    const PrivateScreenImageCreateInfo& privateImageCreateInfo,
    ImageCreateInfo*                    pImageInfo)
{
    PAL_ASSERT(pImageInfo != nullptr);
    pImageInfo->swizzledFormat        = privateImageCreateInfo.swizzledFormat;
    pImageInfo->extent.width          = privateImageCreateInfo.extent.width;
    pImageInfo->extent.height         = privateImageCreateInfo.extent.height;
    pImageInfo->extent.depth          = 1;
    pImageInfo->flags.invariant       = privateImageCreateInfo.flags.invariant;
    pImageInfo->fragments             = 1;
    pImageInfo->samples               = 1;
    pImageInfo->arraySize             = 1;
    pImageInfo->mipLevels             = 1;
    pImageInfo->imageType             = ImageType::Tex2d;
    pImageInfo->tiling                = ImageTiling::Optimal;
    pImageInfo->usageFlags            = privateImageCreateInfo.usage;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 366
    pImageInfo->viewFormatCount       = privateImageCreateInfo.viewFormatCount;
    pImageInfo->pViewFormats          = privateImageCreateInfo.pViewFormats;
#else
    pImageInfo->flags.formatChangeSrd = privateImageCreateInfo.flags.formatChangeSrd;
#endif
}

// =====================================================================================================================
void Image::SetPrivateScreen(
    PrivateScreen* pPrivateScreen)
{
    PAL_ASSERT(pPrivateScreen != nullptr);

    m_pPrivateScreen     = pPrivateScreen;
    m_privateScreenIndex = pPrivateScreen->Index();
}

} // Pal
