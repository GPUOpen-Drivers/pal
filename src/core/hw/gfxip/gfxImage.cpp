/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/image.h"
#include "core/hw/gfxip/gfxImage.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "palFormatInfo.h"
#include "palHashSetImpl.h"

using namespace Util;
using namespace Pal::Formats;

namespace Pal
{

// =====================================================================================================================
GfxImage::GfxImage(
    Image*        pParentImage,
    ImageInfo*    pImageInfo,
    const Device& device)
    :
    m_pParent(pParentImage),
    m_device(device),
    m_createInfo(m_pParent->GetImageCreateInfo()),
    m_pImageInfo(pImageInfo)
{

}

// =====================================================================================================================
// Updates m_gpuMemLayout to take into account a new block of metadata with the given offset and alignment.
void GfxImage::UpdateMetaDataLayout(
    ImageMemoryLayout* pGpuMemLayout,
    gpusize            offset,
    gpusize            alignment)
{
    // If the layout's metadata information is empty, begin the metadata section at this offset.
    if (pGpuMemLayout->metadataOffset == 0)
    {
        pGpuMemLayout->metadataOffset = offset;
    }

    // The metadata section alignment must be the maximum of all individual metadata alignments.
    if (pGpuMemLayout->metadataAlignment < alignment)
    {
        pGpuMemLayout->metadataAlignment = alignment;
    }
}

// =====================================================================================================================
// By default, the image type does not require any override.
ImageType GfxImage::GetOverrideImageType() const
{
    return Parent()->GetImageCreateInfo().imageType;
}

// =====================================================================================================================
// Helper method to check if the surface is a multimedia surface and have some tile mode restrictions.
bool GfxImage::IsRestrictedTiledMultiMediaSurface() const
{
    return ((m_createInfo.swizzledFormat.format == ChNumFormat::NV12) ||
            (m_createInfo.swizzledFormat.format == ChNumFormat::P010) ||
            (m_createInfo.swizzledFormat.format == ChNumFormat::P016));
}

// =====================================================================================================================
uint32 GfxImage::GetStencilPlane() const
{
    return ((m_pImageInfo->numPlanes == 1) ? 0 : 1);
}

// =====================================================================================================================
// Helper method which adds padding to the actual extent of a subresource so that a view can span all planes of a YUV
// planar Image. It is only legal to call this on Images which have a YUV-planar format and more than one array slice.
void GfxImage::PadYuvPlanarViewActualExtent(
    SubresId  subresource,
    Extent3d* pActualExtent // In: Original actualExtent of subresource. Out: padded actualExtent
    ) const
{
    PAL_ASSERT(Formats::IsYuvPlanar(m_createInfo.swizzledFormat.format) &&
               (m_createInfo.arraySize  > 1)                            &&
               (m_createInfo.mipLevels == 1));

    // We need to compute the difference in start offsets of two consecutive array slices of whichever plane
    // the view is associated with.
    const SubresId slice0SubRes = { subresource.plane, 0, 0 };
    const SubresId slice1SubRes = { subresource.plane, 0, 1 };

    const SubResourceInfo*const pSlice0Info  = Parent()->SubresourceInfo(slice0SubRes);
    const SubResourceInfo*const pSlice1Info  = Parent()->SubresourceInfo(slice1SubRes);

    // Stride between array slices in pixels.
    const gpusize arraySliceStride = (pSlice1Info->offset - pSlice0Info->offset) / (pSlice0Info->bitsPerTexel >> 3);

    // The pseudo actualHeight is the stride between slices in pixels divided by the actualPitch of each row.
    PAL_ASSERT((arraySliceStride % pActualExtent->width) == 0);
    pActualExtent->height = static_cast<uint32>(arraySliceStride / pActualExtent->width);
}

} // Pal
