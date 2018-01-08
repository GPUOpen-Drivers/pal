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
#include "core/image.h"
#include "core/hw/gfxip/gfxImage.h"
#include "palFormatInfo.h"

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
    m_pImageInfo(pImageInfo),
    m_fastClearMetaDataOffset(0),
    m_fastClearMetaDataSizePerMip(0)
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
// Updates m_gpuMemLayout to take into account a new block of header data with the given offset and alignment.
void GfxImage::UpdateMetaDataHeaderLayout(
    ImageMemoryLayout* pGpuMemLayout,
    gpusize            offset,
    gpusize            alignment)
{
    // If the layout's metadata header information is empty, begin the metadata header at this offset.
    if (pGpuMemLayout->metadataHeaderOffset == 0)
    {
        pGpuMemLayout->metadataHeaderOffset = offset;
    }

    // The metadata header alignment must be the maximum of all individual metadata header alignments.
    if (pGpuMemLayout->metadataHeaderAlignment < alignment)
    {
        pGpuMemLayout->metadataHeaderAlignment = alignment;
    }
}

// =====================================================================================================================
// Returns the GPU virtual address of the fast-clear metadata for the specified mip level.
gpusize GfxImage::FastClearMetaDataAddr(
    uint32 mipLevel
    ) const
{
    PAL_ASSERT(HasFastClearMetaData());

    return Parent()->GetBoundGpuMemory().GpuVirtAddr() +
           m_fastClearMetaDataOffset                   +
           (m_fastClearMetaDataSizePerMip * mipLevel);
}

// =====================================================================================================================
// Returns the offset relative to the bound GPU memory of the fast-clear metadata for the specified mip level.
gpusize GfxImage::FastClearMetaDataOffset(
    uint32 mipLevel
    ) const
{
    PAL_ASSERT(HasFastClearMetaData());

    return Parent()->GetBoundGpuMemory().Offset() +
           m_fastClearMetaDataOffset +
           (m_fastClearMetaDataSizePerMip * mipLevel);
}

// =====================================================================================================================
// Returns the GPU memory size of the fast-clear metadata for the specified num mips.
gpusize GfxImage::FastClearMetaDataSize(
    uint32 numMips
    ) const
{
    PAL_ASSERT(HasFastClearMetaData());

    return (m_fastClearMetaDataSizePerMip * numMips);
}

// =====================================================================================================================
// Initializes the size and GPU offset for this Image's fast-clear metadata.
void GfxImage::InitFastClearMetaData(
    ImageMemoryLayout* pGpuMemLayout,
    gpusize*           pGpuMemSize,
    size_t             sizePerMipLevel,
    gpusize            alignment)
{
    // Fast-clear metadata must be DWORD aligned so LOAD_CONTEXT_REG commands will function properly.
    static constexpr gpusize Alignment = 4;

    m_fastClearMetaDataOffset     = Pow2Align(*pGpuMemSize, alignment);
    m_fastClearMetaDataSizePerMip = sizePerMipLevel;
    *pGpuMemSize                  = (m_fastClearMetaDataOffset +
                                     (m_fastClearMetaDataSizePerMip * m_createInfo.mipLevels));

    // Update the layout information against the fast-clear metadata.
    UpdateMetaDataHeaderLayout(pGpuMemLayout, m_fastClearMetaDataOffset, Alignment);
}

// =====================================================================================================================
// Sets the clear method for all subresources associated with the specified miplevel.
void GfxImage::UpdateClearMethod(
    SubResourceInfo* pSubResInfoList,
    ImageAspect      aspect,
    uint32           mipLevel,
    ClearMethod      method)
{
    SubresId subRes = { aspect, mipLevel, 0, };

    for (subRes.arraySlice = 0; subRes.arraySlice < m_createInfo.arraySize; ++subRes.arraySlice)
    {
        const uint32 subResId = Parent()->CalcSubresourceId(subRes);
        pSubResInfoList[subResId].clearMethod = method;
    }
}

// =====================================================================================================================
// Calculates the uint representation of clear code 1 in the numeric-format / bit-width that corresponds to the native
// format of this image.
uint32 GfxImage::TranslateClearCodeOneToNativeFmt(
    uint32  cmpIdx
    ) const
{
    const ChNumFormat format            = m_createInfo.swizzledFormat.format;
    const uint32*     pBitCounts        = ComponentBitCounts(format);
    const uint32      maxComponentValue = (1ull << pBitCounts[cmpIdx]) - 1;

    uint32  maxColorValue = 0;

    switch (FormatInfoTable[static_cast<size_t>(format)].numericSupport)
    {
    case NumericSupportFlags::Uint:
        // For integers, 1 means all positive bits are set.
        maxColorValue = maxComponentValue;
        break;

    case NumericSupportFlags::Sint:
        // For integers, 1 means all positive bits are set.
        maxColorValue = maxComponentValue >> 1;
        break;

    case NumericSupportFlags::Unorm:
    case NumericSupportFlags::Srgb:  // should be the same as UNORM
        maxColorValue = maxComponentValue;
        break;

    case NumericSupportFlags::Snorm:
        // The MSB of the "maxComponentValue" is the sign bit, so whack that off
        // here to get the maximum data value.
        maxColorValue = maxComponentValue & ~(1 << (ComponentBitCounts(format)[cmpIdx] - 1));
        break;

    case NumericSupportFlags::Float:
        // Need to get 1.0f in the correct bit-width
        maxColorValue = Math::Float32ToNumBits(1.0f, ComponentBitCounts(format)[cmpIdx]);
        break;

    case NumericSupportFlags::DepthStencil:
    case NumericSupportFlags::Yuv:
    default:
        // Should never see depth surfaces here...
        PAL_ASSERT_ALWAYS();
        break;
    }

    return maxColorValue;
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
    const SubresId slice0SubRes = { subresource.aspect, 0, 0 };
    const SubresId slice1SubRes = { subresource.aspect, 0, 1 };

    const SubResourceInfo*const pSlice0Info  = Parent()->SubresourceInfo(slice0SubRes);
    const SubResourceInfo*const pSlice1Info  = Parent()->SubresourceInfo(slice1SubRes);

    // Stride between array slices in pixels.
    const gpusize arraySliceStride = (pSlice1Info->offset - pSlice0Info->offset) / (pSlice0Info->bitsPerTexel >> 3);

    // The pseudo actualHeight is the stride between slices in pixels divided by the actualPitch of each row.
    PAL_ASSERT((arraySliceStride % pActualExtent->width) == 0);
    pActualExtent->height = static_cast<uint32>(arraySliceStride / pActualExtent->width);
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
            (m_createInfo.swizzledFormat.format == ChNumFormat::P010));
}

} // Pal
