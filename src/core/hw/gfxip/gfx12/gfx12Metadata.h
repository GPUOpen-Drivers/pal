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

#pragma once

namespace Pal
{
namespace Gfx12
{

class Image;

// Flags to indicate if HiZ or HiS is allowed or enabled.
union HiSZUsageFlags
{
    struct
    {
        uint32  hiZ      :  1;
        uint32  hiS      :  1;
        uint32  reserved : 30;
    };
    uint32  value;
};

// Define HiSZ sub type: HiZ or HiS
enum HiSZType : uint32
{
    HiZ = 0,
    HiS,
    Count
};

// =====================================================================================================================
// Manages GFX12 HiZ/HiS states for an Image resource.
//
// HiZ/HiS uses same image type as base image. But when base image is 1D, HiZ/HiS should use 2D instead since there is
// a requirement that its width/height needs to be padded as 2 aligned and then it's a 2D image.
//
// Note that you need to be fairly careful when dealing with MSAA HiZ/HiS because their samples are not typical your
// typical sub-pixel samples. The fundemental addressing unit of these surfaces (one element) maps to a pair of values
// for each scan converter sample tile (a group of 64 samples in the parent image). However, the number of HiZ/HiS
// texels is the same as the number of scan converter pixel tiles (a group of 64 pixels/texels in the parent image).
// If the parent image is single sampled these values are always the same (1 pixel tile = 1 sample tile), however MSAA
// parent images have MSAA HiZ/HiS surfaces. When we address a MSAA HiZ/HiS surface we must compute the (x, y) texel
// coords in units of pixel tiles.
//
// Here's a summary of what units we should use in specific situations:
// 1. When calling SW addrlib: texel extent = pixel tiles, samples/fragments = parent image samples/fragments.
// 2. When creating HiZ/HiS image view SRDs: extent = pixel tiles.
// 3. When clearing/copying HiZ/HiS: offsets & extents = pixel tiles, sample index = ???
//
// So it seems like we only really need values in terms of pixel tiles. We won't be storing offsets or extents in units
// of elements like we typically do for normal MSAA images.
//
// Note that situation #3 has one tricky detail if we ever implement sub-rect/windowed clears or copies: we need to map
// specific pixels within each pixel tile to their specific sample tiles using the sample index. Basically, the scan
// converter must define some sort of mapping between pixels within a 8x8 pixel tile and the sample tiles within that
// pixel tile. This mapping is not currently documented.
class HiSZ
{
public:
    HiSZ(const Image& image, HiSZUsageFlags usageFlags);
    virtual ~HiSZ() {}

    static HiSZUsageFlags UseHiSZForImage(const Image& image);

    Result Init(gpusize* pGpuMemSize);

    bool HiZEnabled() const { return (m_flags.hiZ != 0); }
    bool HiSEnabled() const { return (m_flags.hiS != 0); }

    gpusize MemoryOffset() const { return m_offset[HiZEnabled() ? HiSZType::HiZ : HiSZType::HiS]; }
    gpusize Alignment()    const { return Util::Max(m_alignment[HiSZType::HiZ], m_alignment[HiSZType::HiS]); }

    // Get the unaligned/unpadded extent of the given mip level in units of pixel tiles.
    Extent3d GetUnalignedExtent(uint32 mipLevel) const;

    // Get the base subresource extent in units of pixel tiles. This is not the same as GetExtent(0, true)!
    Extent3d GetBaseExtent() const { return m_baseExtent; }

    gpusize GetOffset(HiSZType hiSZType) const { AssertValid(hiSZType); return m_offset[hiSZType]; }
    gpusize GetSize(HiSZType hiSZType) const { AssertValid(hiSZType); return m_size[hiSZType]; }

    gpusize Get256BAddrSwizzled(HiSZType hiSZType) const;

    Addr3SwizzleMode GetSwizzleMode(HiSZType hiSZType) const { AssertValid(hiSZType); return m_swizzleMode[hiSZType]; }

    uint32 GetHiZInitialValue() const;
    uint32 GetHiZClearValue(float depthValue) const;

    uint16 GetHiSInitialValue() const;
    uint16 GetHiSClearValue(uint8 stencilValue) const;

private:
    void AssertValid(HiSZType hiSZType) const
    {
        PAL_ASSERT(((hiSZType == HiSZType::HiZ) && HiZEnabled()) || ((hiSZType == HiSZType::HiS) && HiSEnabled()));
    }

    // HiZ/HiS format info, used for computing surface swizzle mode, alignment and info.
    // HiS uses X8Y8_UINT format and HiZ uses X16Y16_UNORM.
    ChNumFormat GetFormat(HiSZType hiSZType) const
    {
        AssertValid(hiSZType);
        return (hiSZType == HiSZType::HiZ) ? ChNumFormat::X16Y16_Uint : ChNumFormat::X8Y8_Uint;
    }

    uint32 GetPipeBankXor(HiSZType hiSZType) const;

    const Image&    m_image;
    HiSZUsageFlags  m_flags;

    Addr3SwizzleMode m_swizzleMode[HiSZType::Count];

    gpusize          m_alignment[HiSZType::Count]; // GPU memory alignment for HiZ and HiS.
    gpusize          m_offset[HiSZType::Count];    // GPU memory offset from base Image for HiZ and HiS.
    gpusize          m_size[HiSZType::Count];      // GPU memory size for HiZ and HiS.

    Extent3d         m_baseExtent; // Base subresource extent of HiZ and HiS surfaces in pixel tiles. (not elements!)

    PAL_DISALLOW_DEFAULT_CTOR(HiSZ);
    PAL_DISALLOW_COPY_AND_ASSIGN(HiSZ);
};

} // namespace Gfx12
} // namespace Pal
