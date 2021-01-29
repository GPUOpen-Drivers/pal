/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
    m_pImageInfo(pImageInfo),
    m_hiSPretestsMetaDataOffset(0),
    m_hiSPretestsMetaDataSizePerMip(0),
    m_hasSeenNonTcCompatClearColor(false),
    m_pNumSkippedFceCounter(nullptr)
{
    memset(&m_fastClearMetaDataOffset[0],     0, sizeof(m_fastClearMetaDataOffset));
    memset(&m_fastClearMetaDataSizePerMip[0], 0, sizeof(m_fastClearMetaDataSizePerMip));
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
// Returns an index into the m_fastClearMetaData* arrays.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
uint32 GfxImage::GetFastClearIndex(
    ImageAspect  aspect
    ) const
{
    uint32 aspectIdx = 0;
    switch (aspect)
    {
    case ImageAspect::Depth:
    case ImageAspect::Stencil:
        // Depth / stencil images only have one hTile allocation despite having two aspects.
        aspectIdx = 0;
        break;
    case ImageAspect::CbCr:
    case ImageAspect::Cb:
        aspectIdx = 1;
        break;
    case ImageAspect::Cr:
        aspectIdx = 2;
        break;
    case ImageAspect::YCbCr:
    case ImageAspect::Y:
    case ImageAspect::Color:
        aspectIdx = 0;
        break;
    default:
        PAL_NEVER_CALLED();
        break;
    }

    PAL_ASSERT (aspectIdx < MaxNumPlanes);

    return aspectIdx;
}
#else
uint32 GfxImage::GetFastClearIndex(
    uint32 plane
    ) const
{
    // Depth/stencil images only have one hTile allocation despite having two planes.
    if ((plane == 1) && m_pParent->IsDepthStencilTarget())
    {
        plane = 0;
    }

    PAL_ASSERT (plane < MaxNumPlanes);

    return plane;
}
#endif

// =====================================================================================================================
bool GfxImage::HasFastClearMetaData(
    const SubresRange& range
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    return HasFastClearMetaData(range.startSubres.aspect);
#else
    bool result = false;
    for (uint32 plane = range.startSubres.plane; (plane < (range.startSubres.plane + range.numPlanes)); plane++)
    {
        result |= HasFastClearMetaData(plane);
    }
    return result;
#endif
}

// =====================================================================================================================
// Returns the GPU virtual address of the fast-clear metadata for the specified mip level.
gpusize GfxImage::FastClearMetaDataAddr(
    const SubresId&  subResId
    ) const
{
    gpusize  metaDataAddr = 0;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    if (HasFastClearMetaData(subResId.aspect))
    {
        const uint32  aspectIndex = GetFastClearIndex(subResId.aspect);

        metaDataAddr = Parent()->GetBoundGpuMemory().GpuVirtAddr() +
                       m_fastClearMetaDataOffset[aspectIndex]      +
                       (m_fastClearMetaDataSizePerMip[aspectIndex] * subResId.mipLevel);
    }
#else
    if (HasFastClearMetaData(subResId.plane))
    {
        const uint32 planeIndex = GetFastClearIndex(subResId.plane);

        metaDataAddr = Parent()->GetBoundGpuMemory().GpuVirtAddr() +
                       m_fastClearMetaDataOffset[planeIndex]       +
                       (m_fastClearMetaDataSizePerMip[planeIndex] * subResId.mipLevel);
    }
#endif

    return metaDataAddr;
}

// =====================================================================================================================
// Returns the offset relative to the bound GPU memory of the fast-clear metadata for the specified mip level.
gpusize GfxImage::FastClearMetaDataOffset(
    const SubresId&  subResId
    ) const
{
    gpusize  metaDataOffset = 0;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    if (HasFastClearMetaData(subResId.aspect))
    {
        const uint32  aspectIndex = GetFastClearIndex(subResId.aspect);

        metaDataOffset = Parent()->GetBoundGpuMemory().Offset() +
                         m_fastClearMetaDataOffset[aspectIndex] +
                         (m_fastClearMetaDataSizePerMip[aspectIndex] * subResId.mipLevel);
    }
#else
    if (HasFastClearMetaData(subResId.plane))
    {
        const uint32 planeIndex = GetFastClearIndex(subResId.plane);

        metaDataOffset = Parent()->GetBoundGpuMemory().Offset() +
                         m_fastClearMetaDataOffset[planeIndex] +
                         (m_fastClearMetaDataSizePerMip[planeIndex] * subResId.mipLevel);
    }
#endif

    return metaDataOffset;
}

// =====================================================================================================================
// Returns the GPU memory size of the fast-clear metadata for the specified num mips.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
gpusize GfxImage::FastClearMetaDataSize(
    ImageAspect  aspect,
    uint32       numMips
    ) const
{
    PAL_ASSERT(HasFastClearMetaData(aspect));

    return (m_fastClearMetaDataSizePerMip[GetFastClearIndex(aspect)] * numMips);
}
#else
gpusize GfxImage::FastClearMetaDataSize(
    uint32 plane,
    uint32 numMips
    ) const
{
    PAL_ASSERT(HasFastClearMetaData(plane));

    return (m_fastClearMetaDataSizePerMip[GetFastClearIndex(plane)] * numMips);
}
#endif

// =====================================================================================================================
// Initializes the size and GPU offset for this Image's fast-clear metadata.
void GfxImage::InitFastClearMetaData(
    ImageMemoryLayout* pGpuMemLayout,
    gpusize*           pGpuMemSize,
    size_t             sizePerMipLevel,
    gpusize            alignment,
    uint32             planeIndex)
{
    // Fast-clear metadata must be DWORD aligned so LOAD_CONTEXT_REG commands will function properly.
    static constexpr gpusize Alignment = 4;

    m_fastClearMetaDataOffset[planeIndex]     = Pow2Align(*pGpuMemSize, alignment);
    m_fastClearMetaDataSizePerMip[planeIndex] = sizePerMipLevel;
    *pGpuMemSize                              = (m_fastClearMetaDataOffset[planeIndex] +
                                                 (m_fastClearMetaDataSizePerMip[planeIndex] * m_createInfo.mipLevels));

    // Update the layout information against the fast-clear metadata.
    UpdateMetaDataHeaderLayout(pGpuMemLayout, m_fastClearMetaDataOffset[planeIndex], Alignment);
}

// =====================================================================================================================
// Returns the GPU virtual address of the HiSPretests metadata for the specified mip level.
gpusize GfxImage::HiSPretestsMetaDataAddr(
    uint32 mipLevel
    ) const
{
    PAL_ASSERT(HasHiSPretestsMetaData());

    return Parent()->GetBoundGpuMemory().GpuVirtAddr() +
           m_hiSPretestsMetaDataOffset +
           (m_hiSPretestsMetaDataSizePerMip * mipLevel);
}

// =====================================================================================================================
// Returns the offset relative to the bound GPU memory of the HiSPretests metadata for the specified mip level.
gpusize GfxImage::HiSPretestsMetaDataOffset(
    uint32 mipLevel
    ) const
{
    PAL_ASSERT(HasHiSPretestsMetaData());

    return Parent()->GetBoundGpuMemory().Offset() +
           m_hiSPretestsMetaDataOffset +
           (m_hiSPretestsMetaDataSizePerMip * mipLevel);
}

// =====================================================================================================================
// Returns the GPU memory size of the HiSPretests metadata for the specified num mips.
gpusize GfxImage::HiSPretestsMetaDataSize(
    uint32 numMips
    ) const
{
    PAL_ASSERT(HasHiSPretestsMetaData());

    return (m_hiSPretestsMetaDataSizePerMip * numMips);
}

// =====================================================================================================================
// Initializes the size and GPU offset for this Image's HiSPretests metadata.
void GfxImage::InitHiSPretestsMetaData(
    ImageMemoryLayout* pGpuMemLayout,
    gpusize*           pGpuMemSize,
    size_t             sizePerMipLevel,
    gpusize            alignment)
{
    m_hiSPretestsMetaDataOffset     = Pow2Align(*pGpuMemSize, alignment);
    m_hiSPretestsMetaDataSizePerMip = sizePerMipLevel;

    *pGpuMemSize = (m_hiSPretestsMetaDataOffset +
                   (m_hiSPretestsMetaDataSizePerMip * m_createInfo.mipLevels));

    // Update the layout information against the HiStencil metadata.
    UpdateMetaDataHeaderLayout(pGpuMemLayout, m_hiSPretestsMetaDataOffset, alignment);
}

// =====================================================================================================================
// Sets the clear method for all subresources associated with the specified miplevel.
void GfxImage::UpdateClearMethod(
    SubResourceInfo* pSubResInfoList,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    ImageAspect      aspect,
#else
    uint32           plane,
#endif
    uint32           mipLevel,
    ClearMethod      method)
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    SubresId subRes = { aspect, mipLevel, 0, };
#else
    SubresId subRes = { plane, mipLevel, 0, };
#endif

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

    // This is really a problem on the caller's end, as this function won't work for 9-9-9-5 format.
    // The fractional 9-bit portion of 1.0f is zero...  the same as the fractional 9-bit portion of 0.0f.
    PAL_ASSERT(format != ChNumFormat::X9Y9Z9E5_Float);

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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const SubresId slice0SubRes = { subresource.aspect, 0, 0 };
    const SubresId slice1SubRes = { subresource.aspect, 0, 1 };
#else
    const SubresId slice0SubRes = { subresource.plane, 0, 0 };
    const SubresId slice1SubRes = { subresource.plane, 0, 1 };
#endif

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

// =====================================================================================================================
uint32 GfxImage::GetFceRefCount() const
{
    uint32 refCount = 0;

    if (m_pNumSkippedFceCounter != nullptr)
    {
        refCount = *m_pNumSkippedFceCounter;
    }

    return refCount;
}

// =====================================================================================================================
// Increments the FCE ref count.
void GfxImage::IncrementFceRefCount()
{
    if (m_pNumSkippedFceCounter != nullptr)
    {
        Util::AtomicIncrement(m_pNumSkippedFceCounter);
    }
}

// =====================================================================================================================
void GfxImage::Destroy()
{
    if (m_pNumSkippedFceCounter != nullptr)
    {
        // Give up the allocation.
        Util::AtomicDecrement(m_pNumSkippedFceCounter);
    }
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
// =====================================================================================================================
// Get the index of a specified aspect.
uint32 GfxImage::GetDepthStencilStateIndex(
    ImageAspect dsAspect
    ) const
{
    PAL_ASSERT(dsAspect == ImageAspect::Depth || dsAspect == ImageAspect::Stencil);
    return (m_pImageInfo->numPlanes == 1) ? 0 : static_cast<uint32>(dsAspect == ImageAspect::Stencil);
}
#else
uint32 GfxImage::GetStencilPlane() const
{
    return ((m_pImageInfo->numPlanes == 1) ? 0 : 1);
}
#endif
} // Pal
