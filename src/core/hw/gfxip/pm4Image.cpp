/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/pm4Image.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "palFormatInfo.h"
#include "palHashSetImpl.h"

using namespace Util;
using namespace Pal::Formats;

namespace Pal
{

// =====================================================================================================================
Pm4Image::Pm4Image(
    Image*        pParentImage,
    ImageInfo*    pImageInfo,
    const Device& device)
    :
    GfxImage(pParentImage, pImageInfo, device),
    m_fastClearMetaDataOffset{},
    m_fastClearMetaDataSizePerMip{},
    m_hiSPretestsMetaDataOffset(0),
    m_hiSPretestsMetaDataSizePerMip(0),
    m_hasSeenNonTcCompatClearColor(false),
    m_pNumSkippedFceCounter(nullptr)
{

}

// =====================================================================================================================
// Returns an index into the m_fastClearMetaData* arrays.
uint32 Pm4Image::GetFastClearIndex(
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
bool Pm4Image::HasFastClearMetaData(
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
gpusize Pm4Image::FastClearMetaDataAddr(
    const SubresId&  subResId
    ) const
{
    gpusize  metaDataAddr = 0;

    if (HasFastClearMetaData(subResId.plane))
    {
        const uint32 planeIndex = GetFastClearIndex(subResId.plane);

        metaDataAddr = Parent()->GetBoundGpuMemory().GpuVirtAddr() +
                       m_fastClearMetaDataOffset[planeIndex]       +
                       (m_fastClearMetaDataSizePerMip[planeIndex] * subResId.mipLevel);
    }

    return metaDataAddr;
}

// =====================================================================================================================
// Returns the offset relative to the bound GPU memory of the fast-clear metadata for the specified mip level.
gpusize Pm4Image::FastClearMetaDataOffset(
    const SubresId&  subResId
    ) const
{
    gpusize  metaDataOffset = 0;

    if (HasFastClearMetaData(subResId.plane))
    {
        const uint32 planeIndex = GetFastClearIndex(subResId.plane);

        metaDataOffset = Parent()->GetBoundGpuMemory().Offset() +
                         m_fastClearMetaDataOffset[planeIndex] +
                         (m_fastClearMetaDataSizePerMip[planeIndex] * subResId.mipLevel);
    }

    return metaDataOffset;
}

// =====================================================================================================================
// Returns the GPU memory size of the fast-clear metadata for the specified num mips.
gpusize Pm4Image::FastClearMetaDataSize(
    uint32 plane,
    uint32 numMips
    ) const
{
    PAL_ASSERT(HasFastClearMetaData(plane));

    return (m_fastClearMetaDataSizePerMip[GetFastClearIndex(plane)] * numMips);
}

// =====================================================================================================================
// Initializes the size and GPU offset for this Image's fast-clear metadata.
void Pm4Image::InitFastClearMetaData(
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
void Pm4Image::UpdateClearMethod(
    SubResourceInfo* pSubResInfoList,
    uint32           plane,
    uint32           mipLevel,
    ClearMethod      method)
{
    SubresId subRes = { plane, mipLevel, 0, };

    for (subRes.arraySlice = 0; subRes.arraySlice < m_createInfo.arraySize; ++subRes.arraySlice)
    {
        const uint32 subResId = Parent()->CalcSubresourceId(subRes);
        pSubResInfoList[subResId].clearMethod = method;
    }
}

// =====================================================================================================================
// Returns the GPU virtual address of the HiSPretests metadata for the specified mip level.
gpusize Pm4Image::HiSPretestsMetaDataAddr(
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
gpusize Pm4Image::HiSPretestsMetaDataOffset(
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
gpusize Pm4Image::HiSPretestsMetaDataSize(
    uint32 numMips
    ) const
{
    PAL_ASSERT(HasHiSPretestsMetaData());

    return (m_hiSPretestsMetaDataSizePerMip * numMips);
}

// =====================================================================================================================
// Updates m_gpuMemLayout to take into account a new block of header data with the given offset and alignment.
void Pm4Image::UpdateMetaDataHeaderLayout(
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
void Pm4Image::InitHiSPretestsMetaData(
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
uint32 Pm4Image::TranslateClearCodeOneToNativeFmt(
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
        else
        {
            maxColorValue = Math::Float32ToNumBits(1.0f, ComponentBitCounts(format)[cmpIdx]);
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
uint32 Pm4Image::GetFceRefCount() const
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
void Pm4Image::IncrementFceRefCount()
{
    if (m_pNumSkippedFceCounter != nullptr)
    {
        Util::AtomicIncrement(m_pNumSkippedFceCounter);
    }
}

// =====================================================================================================================
void Pm4Image::Destroy()
{
    if (m_pNumSkippedFceCounter != nullptr)
    {
        // Give up the allocation.
        Util::AtomicDecrement(m_pNumSkippedFceCounter);
    }
}

} // Pal
