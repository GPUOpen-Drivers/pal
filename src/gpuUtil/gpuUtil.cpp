/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palGpuUtil.h"
#include "palFormatInfo.h"
#include "palInlineFuncs.h"
#include "palDevice.h"
#include "palImage.h"
#include "palGpuMemory.h"
#include "palCmdBuffer.h"

using namespace Pal;
using Util::Max;
using Util::IsPow2Aligned;

namespace GpuUtil
{

// =====================================================================================================================
// Validate image copy region
bool ValidateImageCopyRegion(
    const DeviceProperties& properties,
    EngineType              engineType,
    const IImage&           src,
    const IImage&           dst,
    const ImageCopyRegion&  region)
{
    bool copySupported = false;

    SubresLayout srcSubresLayout = {};
    SubresLayout dstSubresLayout = {};
    if ((src.GetImageCreateInfo().swizzledFormat.format == dst.GetImageCreateInfo().swizzledFormat.format) &&
        (src.GetSubresourceLayout(region.srcSubres, &srcSubresLayout) == Result::Success)                  &&
        (dst.GetSubresourceLayout(region.dstSubres, &dstSubresLayout) == Result::Success)                  &&
        ((properties.engineProperties[engineType].flags.supportsMismatchedTileTokenCopy != 0)              ||
         (srcSubresLayout.tileToken == dstSubresLayout.tileToken)))
    {
        // Image-Image copy alignment is in bytes while region extents are in texels
        const Extent3d& alignment = properties.engineProperties[engineType].minTiledImageCopyAlignment;
        const uint32 bytesPerPixel = Formats::BytesPerPixel(src.GetImageCreateInfo().swizzledFormat.format);

        const Extent3d imageCopyAlign =
        {
            Max(1U, alignment.width  / bytesPerPixel),
            alignment.height,
            alignment.depth,
        };

        // Single step copy is supported if image offsets and extents are aligned properly
        copySupported = IsPow2Aligned(region.srcOffset.x,   imageCopyAlign.width)  &&
                        IsPow2Aligned(region.srcOffset.y,   imageCopyAlign.height) &&
                        IsPow2Aligned(region.dstOffset.x,   imageCopyAlign.width)  &&
                        IsPow2Aligned(region.dstOffset.y,   imageCopyAlign.height) &&
                        IsPow2Aligned(region.extent.width,  imageCopyAlign.width)  &&
                        IsPow2Aligned(region.extent.height, imageCopyAlign.height);

        if (src.GetImageCreateInfo().extent.depth > 1)
        {
            copySupported &= IsPow2Aligned(region.srcOffset.z,  imageCopyAlign.depth) &&
                             IsPow2Aligned(region.dstOffset.z,  imageCopyAlign.depth) &&
                             IsPow2Aligned(region.extent.depth, imageCopyAlign.depth);
        }
    }

    return copySupported;
}

// =====================================================================================================================
// Validate typed buffer copy region
bool ValidateTypedBufferCopyRegion(
    const DeviceProperties&      properties,
    EngineType                   engineType,
    const TypedBufferCopyRegion& region)
{
    bool copySupported = false;

    if (region.srcBuffer.swizzledFormat.format == region.dstBuffer.swizzledFormat.format)
    {
        // Copy alignment is in bytes while region extents are in texels
        const Extent3d& alignment = properties.engineProperties[engineType].minTiledImageMemCopyAlignment;
        const uint32 bytesPerPixel = Formats::BytesPerPixel(region.srcBuffer.swizzledFormat.format);

        const Extent3d copyAlignment =
        {
            Max(1U, alignment.width  / bytesPerPixel),
            alignment.height,
            alignment.depth,
        };

        // Single step copy is supported if typed memory extents are aligned properly
        copySupported = IsPow2Aligned(region.extent.width,  copyAlignment.width)  &&
                        IsPow2Aligned(region.extent.height, copyAlignment.height) &&
                        IsPow2Aligned(region.extent.depth,  copyAlignment.depth);
    }

    return copySupported;
}

// =====================================================================================================================
// Validate image-memory copy region and align extents as necessary
bool ValidateMemoryImageRegion(
    const DeviceProperties&      properties,
    EngineType                   engineType,
    const IImage&                image,
    const IGpuMemory&            memory,
    const MemoryImageCopyRegion& region)
{
    bool copySupported = false;

    // Image-memory copy alignment is in bytes while region extents are in texels
    const Extent3d& alignment = properties.engineProperties[engineType].minTiledImageMemCopyAlignment;
    const uint32 bytesPerPixel = Formats::BytesPerPixel(image.GetImageCreateInfo().swizzledFormat.format);

    const Extent3d imageMemAlign =
    {
        Max(1U, alignment.width  / bytesPerPixel),
        Max(1U, alignment.height / bytesPerPixel),
        Max(1U, alignment.depth  / bytesPerPixel),
    };

    // Single step copy is supported if image offsets and extents are aligned properly
    copySupported = IsPow2Aligned(region.imageOffset.x,      imageMemAlign.width)  &&
                    IsPow2Aligned(region.imageOffset.y,      imageMemAlign.height) &&
                    IsPow2Aligned(region.imageExtent.width,  imageMemAlign.width)  &&
                    IsPow2Aligned(region.imageExtent.height, imageMemAlign.height);

    if (copySupported && (image.GetImageCreateInfo().extent.depth > 1))
    {
        copySupported &= IsPow2Aligned(region.imageOffset.z,     imageMemAlign.depth) &&
                         IsPow2Aligned(region.imageExtent.depth, imageMemAlign.depth);
    }

    // Single step copy is supported if linear memory begin/end/pitch are aligned properly
    if (copySupported)
    {
        const uint32  linearMemAlign = Max(1U, properties.engineProperties[engineType].minLinearMemCopyAlignment.width);
        const gpusize beginVa        = memory.Desc().gpuVirtAddr + region.gpuMemoryOffset;
        const gpusize endVa          = beginVa + region.imageExtent.width * bytesPerPixel;

        copySupported &= IsPow2Aligned(beginVa,                    linearMemAlign) &&
                         IsPow2Aligned(endVa,                      linearMemAlign) &&
                         IsPow2Aligned(region.gpuMemoryRowPitch,   linearMemAlign) &&
                         IsPow2Aligned(region.gpuMemoryDepthPitch, linearMemAlign);
    }

    return copySupported;
}

} //GpuUtil
