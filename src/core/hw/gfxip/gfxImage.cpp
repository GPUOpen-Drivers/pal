/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
    m_hasMisalignedMetadata(false),
    m_fastClearMetaDataOffset{},
    m_fastClearMetaDataSizePerMip{},
    m_hiSPretestsMetaDataOffset(0),
    m_hiSPretestsMetaDataSizePerMip(0),
    m_hasSeenNonTcCompatClearColor(false),
    m_pNumSkippedFceCounter(nullptr)
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
// Helper method to check if the surface is a multimedia surface and have some tile mode restrictions.
bool GfxImage::IsRestrictedTiledMultiMediaSurface() const
{
    return ((m_createInfo.swizzledFormat.format == ChNumFormat::NV12) ||
            (m_createInfo.swizzledFormat.format == ChNumFormat::P010) ||
            (m_createInfo.swizzledFormat.format == ChNumFormat::P016));
}

// =====================================================================================================================
// Helper method to check if the surface is a multimedia surface with some tile mode restrictions.
bool GfxImage::IsNv12OrP010FormatSurface() const
{
    return ((m_createInfo.swizzledFormat.format == ChNumFormat::NV12) ||
            (m_createInfo.swizzledFormat.format == ChNumFormat::P010));
}

// =====================================================================================================================
uint8 GfxImage::GetStencilPlane() const
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

// =====================================================================================================================
bool GfxImage::IsSwizzleThin(
    SubresId subresId
    ) const
{
    const SubResourceInfo* pSubResInfo = Parent()->SubresourceInfo(subresId);
    const uint32           swizzleMode = GetSwTileMode(pSubResInfo);

    // If the image is 1D or 2D, then it's automatically thin... 3D images require help from the addrmgr as then the
    // "thin" determination is dependent on the characteristics of the swizzle mode assigned to this subresource.
    return (m_createInfo.imageType != ImageType::Tex3d) || m_device.GetAddrMgr()->IsThin(swizzleMode);
}

// =====================================================================================================================
// Returns an index into the m_fastClearMetaData* arrays.
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

// =====================================================================================================================
bool GfxImage::HasFastClearMetaData(
    const SubresRange& range
    ) const
{
    bool result = false;
    for (uint32 plane = range.startSubres.plane; (plane < (range.startSubres.plane + range.numPlanes)); plane++)
    {
        result |= HasFastClearMetaData(plane);
    }
    return result;
}

// =====================================================================================================================
// Returns the GPU virtual address of the fast-clear metadata for the specified mip level.
gpusize GfxImage::FastClearMetaDataAddr(
    SubresId subresId
    ) const
{
    gpusize  metaDataAddr = 0;

    if (HasFastClearMetaData(subresId.plane))
    {
        const uint32 planeIndex = GetFastClearIndex(subresId.plane);

        metaDataAddr = Parent()->GetBoundGpuMemory().GpuVirtAddr() +
                       m_fastClearMetaDataOffset[planeIndex]       +
                       (m_fastClearMetaDataSizePerMip[planeIndex] * subresId.mipLevel);
    }

    return metaDataAddr;
}

// =====================================================================================================================
// Returns the offset relative to the bound GPU memory of the fast-clear metadata for the specified mip level.
gpusize GfxImage::FastClearMetaDataOffset(
    SubresId subresId
    ) const
{
    gpusize  metaDataOffset = 0;

    if (HasFastClearMetaData(subresId.plane))
    {
        const uint32 planeIndex = GetFastClearIndex(subresId.plane);

        metaDataOffset = Parent()->GetBoundGpuMemory().Offset() +
                         m_fastClearMetaDataOffset[planeIndex] +
                         (m_fastClearMetaDataSizePerMip[planeIndex] * subresId.mipLevel);
    }

    return metaDataOffset;
}

// =====================================================================================================================
// Returns the GPU memory size of the fast-clear metadata for the specified num mips.
gpusize GfxImage::FastClearMetaDataSize(
    uint32 plane,
    uint32 numMips
    ) const
{
    PAL_ASSERT(HasFastClearMetaData(plane));

    return (m_fastClearMetaDataSizePerMip[GetFastClearIndex(plane)] * numMips);
}

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
// Sets the clear method for all subresources associated with the specified miplevel.
void GfxImage::UpdateClearMethod(
    SubResourceInfo* pSubResInfoList,
    uint32           plane,
    uint32           mipLevel,
    ClearMethod      method)
{
    SubresId subRes = Subres(plane, mipLevel, 0);
    for (; subRes.arraySlice < m_createInfo.arraySize; ++subRes.arraySlice)
    {
        const uint32 subresId = Parent()->CalcSubresourceId(subRes);
        pSubResInfoList[subresId].clearMethod = method;
    }
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
        if (format == ChNumFormat::X10Y10Z10W2_Float)
        {
            maxColorValue = Math::Float32ToFloat10_6e4(1.0f);
        }
        // ones isn't calculated properly because Float32ToNumBits
        // does not do anything for a bit-count of 9; even if it did, the
        // 9-bit fractional portion of 1.0f and 0.0f are the same

        // Since we only allow clearing to MAX for this format,
        // clearCodeOne for each component is the max value for that component
        else if (format == ChNumFormat::X9Y9Z9E5_Float)
        {
            // unpacked this value is (0x000001FF, 0x000001FF, 0x000001FF, 0x000001F),
            // which is maxComponentValue for each channel
            maxColorValue = maxComponentValue;
        }
        else if (pBitCounts[cmpIdx] > 0)
        {
            maxColorValue = Math::Float32ToNumBits(1.0f, pBitCounts[cmpIdx]);
        }
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

#if PAL_BUILD_GFX12

// =====================================================================================================================
bool GfxImage::EnableClientCompression(
    bool disableClientCompression
    ) const
{
    // Note that compression status (PTE.D) on virtual GPU memory is unknown, since it depends on if mapped physical
    // GPU memory enables compression or not. Enabling compression for virtual memory with PTE.D=0 may have
    // performance hit as client compressed content will be decompressed by GL2 when writing out.
    const GpuMemory*const  pGpuMemory      = m_pParent->GetBoundGpuMemory().Memory();
    const ImageCreateInfo& imageCreateInfo = m_pParent->GetImageCreateInfo();

    const bool enableClientCompression =
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 876
        (imageCreateInfo.clientCompressionMode != ClientCompressionMode::Disable) &&
#else
        (imageCreateInfo.clientCompressionMode != TriState::Disable) &&
#endif
        (disableClientCompression == false) &&
        pGpuMemory->MaybeCompressed();

    return enableClientCompression;
}

#endif

// =====================================================================================================================
void GfxImage::Destroy()
{
    if (m_pNumSkippedFceCounter != nullptr)
    {
        // Give up the allocation.
        Util::AtomicDecrement(m_pNumSkippedFceCounter);
    }
}

} // Pal
