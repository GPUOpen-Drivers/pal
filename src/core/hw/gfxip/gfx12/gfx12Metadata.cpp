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

#include "core/device.h"
#include "core/addrMgr/addrMgr3/addrMgr3.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12Image.h"
#include "core/hw/gfxip/gfx12/gfx12Metadata.h"
#include "palLiterals.h"

using namespace Util;
using namespace Literals;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
static const Device& GetGfx12Device(
    const Image& image)
{
    return *static_cast<const Device*>(image.Parent()->GetDevice()->GetGfxDevice());
}

// =====================================================================================================================
// Constructor for the HiSZ class
HiSZ::HiSZ(
    const Image&   image,
    HiSZUsageFlags usageFlags)
    :
    m_image(image),
    m_flags(usageFlags),
    m_swizzleMode{},
    m_offset{},
    m_size{},
    m_alignment()
{
    const ImageCreateInfo& createInfo = m_image.Parent()->GetImageCreateInfo();

    // Compute required base Extent for covering all pixels in mipmap levels.
    //
    // Note that the HiZ/HiS surfaces can have a larger pixel area or even a smaller pixel area than the parent image.
    // For example, if the HiZ/HiS surfaces smaller than the parent image then the SC automatically disables The HiS/HiZ
    // optimizations on the border pixels that lack metadata coverage.
    m_baseExtent = GetUnalignedExtent(0);

    // HiZ/HiS implementation requires 2-pixel tile surface alignment for base extent which is used to program
    // PA_SC_HIZ_INFO and PA_SC_HIS_INFO.
    m_baseExtent.width  = Pow2Align(m_baseExtent.width, 2u);
    m_baseExtent.height = Pow2Align(m_baseExtent.height, 2u);

    for (uint32 mip = 1; mip < createInfo.mipLevels; mip++)
    {
        const Extent3d& extent = GetUnalignedExtent(mip);

        // Required base extent to cover the pixels in current mip level
        if (extent.width > 1)
        {
            m_baseExtent.width = Max(m_baseExtent.width, (extent.width << mip));
        }

        if (extent.height > 1)
        {
            m_baseExtent.height = Max(m_baseExtent.height, (extent.height << mip));
        }
    }
}

// =====================================================================================================================
// Determine if the given Image object should use any metadata.
HiSZUsageFlags HiSZ::UseHiSZForImage(
    const Image& image)
{
    const Pal::Device&     device        = *image.Parent()->GetDevice();
    const Device&          gfx12Device   = GetGfx12Device(image);
    const auto&            gfx12Settings = gfx12Device.Settings();
    const Pal::Image*const pPalImage     = image.Parent();
    const auto&            createInfo    = pPalImage->GetImageCreateInfo();
    HiSZUsageFlags         usageFlags    = {};

    // If this isn't a depth buffer, then no need any metadata.
    if (pPalImage->IsDepthStencilTarget()                   &&
        (createInfo.metadataMode != MetadataMode::Disabled) &&
        (pPalImage->IsShared()                   == false)  &&
        (pPalImage->IsMetadataDisabledByClient() == false)  &&
        (pPalImage->IsTmz()                      == false)  &&
        ((createInfo.extent.width * createInfo.extent.height) >=
         (gfx12Settings.enableHiDepthHiStencilMinSize * gfx12Settings.enableHiDepthHiStencilMinSize)))
    {
        usageFlags.hiZ = gfx12Settings.hiDepthEnable   &&
                         device.SupportsDepth(createInfo.swizzledFormat.format, createInfo.tiling);
        usageFlags.hiS = gfx12Settings.hiStencilEnable &&
                         device.SupportsStencil(createInfo.swizzledFormat.format, createInfo.tiling);
    }

    return usageFlags;
}

// =====================================================================================================================
// Compute size of HiZ or HiS, which is in unit of defined pixel tile compared to image data surface.
Extent3d HiSZ::GetUnalignedExtent(
    uint32 mipLevel
    ) const
{
    //
    // Pixel tile dimension:          8x8 (for all cases)
    // Sample tile (sTile) dimension: 8x8 - 1xAA; 8x4 - 2xAA; 4x4 - 4xAA; 4x2 - 8xAA
    //
    // Each sample tile maps to one element in HiZ (X16Y16_UNORM) or HiS (X8Y8_UINT). For HiZ, the red channel of each
    // element represents the mini depth value across the s-tile, and the green channel represents the maximum value.
    // For HiS, the red channel of each element represents the AND reduction of all stencil values across the s-tile,
    // and the green channel represents the OR reduction.
    //
    // HiZ/HiS image is addressed in pixel tile (8x8) space. For single sample depth/stencil image, one pixel tile maps
    // to one sample tile; and for MSAA depth/stencil image, one pixel tile maps multiple sample tiles (number of sTile
    // in pixel tile equals to number of base image fragments). For MSAA depth/stencil image, its HiZ/HiS is also viewed
    // as a MSAA image, where sample tiles (in the pixel tile) are multiple-samples alike and organized in morton order.
    constexpr Extent2d PixelTileDim = { .width = 8, .height = 8 };

    const Pal::Image*const pParent     = m_image.Parent();
    const SubResourceInfo* pSubResInfo = pParent->SubresourceInfo(Subres(0, mipLevel, 0));

    const Extent3d& imageBaseExtent = pSubResInfo->extentTexels;

    Extent3d extent;
    extent.width  = RoundUpQuotient(imageBaseExtent.width,  PixelTileDim.width);
    extent.height = RoundUpQuotient(imageBaseExtent.height, PixelTileDim.height);
    extent.depth  = imageBaseExtent.depth;

    return extent;
}

// =====================================================================================================================
uint32 HiSZ::GetPipeBankXor(
    HiSZType hiSZType
    ) const
{
    const uint8    plane        = (hiSZType == HiSZType::HiZ) ? 0 : m_image.GetStencilPlane();
    const SubresId baseSubResId = { plane, 0, 0 };
    // Use data surface's PipeBankXor.
    const uint32   pipeBankXor  = AddrMgr3::GetTileInfo(m_image.Parent(), baseSubResId)->pipeBankXor;

    // Below are copied from gfx9 HWL but should be applicable for GFX12 on HiZ/HiS.
    //
    // HiZ/HiS and the image itself might have different tile block sizes (i.e., usually the
    // image will be 64kB, but the meta data will usually be 4kB).  For a 64kB block image, the low 16 bits
    // will always be zero, but for a 4kB block image, only the low 12 bits will be zero.  The low eight
    // bits are never programmed (i.e., assumed by HW to be zero), so we really have:
    //    64kB = low 16 bits are zero --> 8 bits for pipeBankXor
    //     4kB = low 12 bits are zero --> 4 bits for pipeBankXor
    //
    // The "alignment" parameter of the mask ram essentially defines the block size of the mask-ram.
    // The low eight bits are never programmed and assumed by HW to be zero
    //
    const uint32 numBitsForPipeBankXor = LowPart(Log2(m_alignment[hiSZType])) - 8;
    const uint32 pipeBankXorMask       = ((1 << numBitsForPipeBankXor) - 1);

    // Whack off any bits that we can't use.
    return (pipeBankXor & pipeBankXorMask);
}

// =====================================================================================================================
gpusize HiSZ::Get256BAddrSwizzled(
    HiSZType hiSZType
    ) const
{
    AssertValid(hiSZType);

    const BoundGpuMemory& boundMem     = m_image.Parent()->GetBoundGpuMemory();
    const gpusize         baseOffset   = GetOffset(hiSZType);
    const gpusize         baseAddr256B = (boundMem.GpuVirtAddr() + baseOffset) >> 8;
    const uint32          pipeBankXor  = GetPipeBankXor(hiSZType);

    return baseAddr256B | pipeBankXor;
}

// =====================================================================================================================
Result HiSZ::Init(
    gpusize* pGpuMemSize)
{
    Result result = Result::Success;

    const Pal::Device& device   = *m_image.Parent()->GetDevice();
    const auto* const  pAddrMgr = static_cast<const AddrMgr3::AddrMgr3*>(device.GetAddrMgr());

    const bool computeInfo[HiSZType::Count] = { HiZEnabled(), HiSEnabled() };

    for (uint32 type = HiSZType::HiZ; type < HiSZType::Count; type++)
    {
        if (computeInfo[type])
        {
            ADDR3_COMPUTE_SURFACE_INFO_OUTPUT infoOutput = {};
            infoOutput.size = sizeof(ADDR3_COMPUTE_SURFACE_INFO_OUTPUT);

            // Compute swizzle mode for HiZ/HiS.
            result = pAddrMgr->ComputeHiSZSwizzleMode(*m_image.Parent(),
                                                      GetBaseExtent(),
                                                      GetFormat(HiSZType(type)),
                                                      (type == HiSZType::HiZ),
                                                      &m_swizzleMode[type]);
            if (result != Result::Success)
            {
                break;
            }

            // Compute base alignment and size info for HiZ/HiS.
            result = pAddrMgr->ComputeHiSZInfo(*m_image.Parent(),
                                               GetBaseExtent(),
                                               GetFormat(HiSZType(type)),
                                               m_swizzleMode[type],
                                               &infoOutput);
            if (result != Result::Success)
            {
                break;
            }

            m_alignment[type] = infoOutput.baseAlign;
            m_size[type]      = infoOutput.surfSize;
        }
    }

    if (result == Result::Success)
    {
        const gpusize baseAlign = Alignment();

        // Base address of HiZ and HiS must be 256 bytes aligned.
        PAL_ASSERT((baseAlign & 0xFF) == 0);

        const ImageInternalCreateInfo& internalCrInfo = m_image.Parent()->GetImageInfo().internalCreateInfo;

        const gpusize metadataBaseOffset = Pow2Align(*pGpuMemSize, baseAlign);
        gpusize       metadataEndOffset  = metadataBaseOffset;

        // If both HiZ and HiS are present, HiZ is located before HiS.
        if (internalCrInfo.flags.useSharedMetadata)
        {
            if (HiZEnabled())
            {
                PAL_ASSERT(metadataBaseOffset == internalCrInfo.sharedMetadata.hiZOffset);

                m_offset[HiSZType::HiZ] = internalCrInfo.sharedMetadata.hiZOffset;
                metadataEndOffset       = m_offset[HiSZType::HiZ] + m_size[HiSZType::HiZ];
            }

            if (HiSEnabled())
            {
                PAL_ASSERT((metadataBaseOffset + Pow2Align(m_size[HiSZType::HiZ], baseAlign)) ==
                           internalCrInfo.sharedMetadata.hiSOffset);

                m_offset[HiSZType::HiS] = internalCrInfo.sharedMetadata.hiSOffset;
                metadataEndOffset       = m_offset[HiSZType::HiS] + m_size[HiSZType::HiS];
            }
        }
        else
        {
            if (HiZEnabled())
            {
                m_offset[HiSZType::HiZ] = metadataBaseOffset;
                metadataEndOffset       = m_offset[HiSZType::HiZ] + m_size[HiSZType::HiZ];
            }

            if (HiSEnabled())
            {
                m_offset[HiSZType::HiS] = metadataBaseOffset + Pow2Align(m_size[HiSZType::HiZ], baseAlign);
                metadataEndOffset       = m_offset[HiSZType::HiS] + m_size[HiSZType::HiS];
            }
        }

        *pGpuMemSize = metadataEndOffset;
    }

    return result;
}

// =====================================================================================================================
uint32 HiSZ::GetHiZInitialValue() const
{
    PAL_ASSERT(m_flags.hiZ != 0);

    // For unorm16, min and max values are 0 and 0xFFFF.
    constexpr uint32 ZMin = 0;
    constexpr uint32 ZMax = 0xFFFF;

    // The first component of each element (red channel) represents the minimum value across the s-tile,
    // and the second component (green channel) represents the maximum value.
    return (ZMin << 0) | (ZMax << 16);
}

// =====================================================================================================================
uint32 HiSZ::GetHiZClearValue(
    float depthValue
    ) const
{
    uint16 convertedDepth;

    PAL_ASSERT(m_flags.hiZ != 0);
    PAL_ASSERT((depthValue >= 0.0f) && (depthValue <= 1.0f));
    depthValue = Clamp(depthValue, 0.0f, 1.0f);

    constexpr uint32 MaxZVal = UINT16_MAX;
    const uint32     value   = uint32((depthValue * MaxZVal) + 0.5f);

    convertedDepth = (value & MaxZVal);

    // The first component of each element (red channel) represents the minimum value across the s-tile,
    // and the second component (green channel) represents the maximum value.

    // For clear, zMin = zMax = convertedDepth.
    return (convertedDepth << 0) | (convertedDepth << 16);
}

// =====================================================================================================================
uint16 HiSZ::GetHiSInitialValue() const
{
    constexpr uint8 AndVal = 0;
    constexpr uint8 OrVal  = 0xFF;

    PAL_ASSERT(m_flags.hiS != 0);

    // The first component represents the AND reduction of all stencil values across the s-tile, and the second
    // represents the OR reduction.  Each component may only be 8 bit (16 bits per element).
    return (AndVal << 0) | (OrVal << 8);
}

// =====================================================================================================================
uint16 HiSZ::GetHiSClearValue(
    uint8 stencilValue
    ) const
{
    PAL_ASSERT(m_flags.hiS != 0);

    // The first component represents the AND reduction of all stencil values across the s-tile, and the second
    // represents the OR reduction.  Each component may only be 8 bit (16 bits per element).

    // For clear, AndVal = OrVal = stencilValue.
    return (stencilValue | (stencilValue << 8));
}

} // Gfx12
} // Pal
