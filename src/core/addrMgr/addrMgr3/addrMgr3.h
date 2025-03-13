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

#include "core/image.h"
#include "core/addrMgr/addrMgr.h"

// Need the HW version of the tiling definitions
#include "core/hw/gfxip/gfx12/chip/gfx12_merged_enum.h"

namespace Pal
{
class   Device;

namespace AddrMgr3
{

// Maximum number of mipmap levels image in gfx12+.
constexpr uint32 MaxImageMipLevels = 17;

// Unique image tile token.
union TileToken
{
    struct
    {
        uint32 elementSize  :  3; // Log2 of bytes per pixel
        uint32 reserved     : 29; // Reserved for future use
    } bits;

    uint32 u32All;
};

// Tiling info structure
struct TileInfo
{
    gpusize           backingStoreOffset; // Offset to this subresource within the KMD's linear backing store for
                                          // the Image. This is relative to the beginning of the Image.
    Addr3SwizzleMode  swizzleMode;        // Swizzle mode associated with this subresource
    uint32            pipeBankXor;        // Pipe/bank XOR value for this subresource
    bool              mip0InMipTail;      // flag indicates mip0 is in mip tail
    gpusize           mipTailMask;        // mask for mip tail offset
};

// =====================================================================================================================
// Returns a pointer to the tiling info for the subresource with the given index.
inline const TileInfo* GetTileInfo(
    const Image* pImage,
    uint32       subResIdx)
{
    PAL_ASSERT(pImage != nullptr);
    return static_cast<const TileInfo*>(pImage->SubresourceTileInfo(subResIdx));
}

// =====================================================================================================================
// Returns a pointer to the tiling info for the given subresource.
inline const TileInfo* GetTileInfo(
    const Image* pImage,
    SubresId     subresId)
{
    return GetTileInfo(pImage, pImage->CalcSubresourceId(subresId));
}

// =====================================================================================================================
// Returns a non-const pointer to the tiling info for the subresource with the given index, given the non-const pointer
// to the entire tiling info list for the Image.
constexpr TileInfo* NonConstTileInfo(
    void*  pTileInfoList,
    uint32 subResIdx)
{
    return static_cast<TileInfo*>(Util::VoidPtrInc(pTileInfoList, (subResIdx * sizeof(TileInfo))));
}

// =====================================================================================================================
constexpr bool IsLinearSwizzleMode(
    Addr3SwizzleMode  swizzleMode)
{
    return (swizzleMode == ADDR3_LINEAR);
}

// =====================================================================================================================
constexpr bool Is3dSwizzleMode(
    Addr3SwizzleMode  swizzleMode)
{
    return ((swizzleMode == ADDR3_4KB_3D) ||
            (swizzleMode == ADDR3_64KB_3D) ||
            (swizzleMode == ADDR3_256KB_3D));
}

// =====================================================================================================================
// Responsible for implementing address and tiling code that is specific to "version 3" of the address library
// interface.  Corresponds to ASICs starting with GFX12
class AddrMgr3 final : public AddrMgr
{
public:
    explicit AddrMgr3(const Device*  pDevice);
    virtual ~AddrMgr3() {}

    virtual Result InitSubresourcesForImage(
        Image*             pImage,
        gpusize*           pGpuMemSize,
        gpusize*           pGpuMemAlignment,
        ImageMemoryLayout* pGpuMemLayout,
        SubResourceInfo*   pSubResInfoList,
        void*              pSubResTileInfoList,
        bool*              pDccUnsupported) const override;

    virtual uint32 GetTileSwizzle(const Image* pImage, SubresId subresource) const override
        { return GetTileInfo(pImage, subresource)->pipeBankXor; }

    // Computes the size (in PRT tiles) of the mip tail for a particular Image plane.
    virtual void ComputeTilesInMipTail(
        const Image&       image,
        uint32             plane,
        ImageMemoryLayout* pGpuMemLayout) const override;

    ADDR3_SURFACE_FLAGS DetermineSurfaceFlags(
        const Image& image,
        uint32       plane) const;

    Result ComputeHiSZSwizzleMode(
        const Image&      image,
        const Extent3d&   hiSZExtent,
        ChNumFormat       hiSZFormat,
        bool              isHiZ,
        Addr3SwizzleMode* pFinalMode) const;

    Result ComputeHiSZInfo(
        const Image&                       image,
        const Extent3d&                    hiSZExtent,
        ChNumFormat                        hiSZFormat,
        Addr3SwizzleMode                   swizzleMode,
        ADDR3_COMPUTE_SURFACE_INFO_OUTPUT* pOut) const;

    static AddrResourceType GetAddrResourceType(ImageType imageType);
    static Addr3SwizzleMode GetAddrSwizzleMode(SwizzleMode swMode);

    uint32 GetHwSwizzleMode(Addr3SwizzleMode swizzleMode) const;

    Result ComputeFmaskSwizzleMode(
        const Image*      pImage,
        Addr3SwizzleMode* pFinalMode) const;

protected:
    static void BuildTileToken(
        SubResourceInfo*  pSubResInfo,
        Addr3SwizzleMode  swizzleMode);

    Addr3SwizzleMode SelectFinalSwizzleMode(
        const Image*                                   pImage,
        const SubResourceInfo*                         pBaseSubRes,
        const ADDR3_GET_POSSIBLE_SWIZZLE_MODE_INPUT*   pIn,
        ADDR3_GET_POSSIBLE_SWIZZLE_MODE_OUTPUT*        pPossibleSwizzles) const;

    uint32 GetNumAddrLib3dSlices(
        const Pal::Image*                         pImage,
        const Addr3SwizzleMode                    swizzleMode,
        const ADDR3_COMPUTE_SURFACE_INFO_OUTPUT&  surfInfoOut) const;

private:
    PAL_DISALLOW_DEFAULT_CTOR(AddrMgr3);
    PAL_DISALLOW_COPY_AND_ASSIGN(AddrMgr3);

    static void BuildTileToken(
        SubResourceInfo* pSubResInfo,
        AddrSwizzleMode  swizzleMode);

    Result GetStereoRightEyePipeBankXor(
        const Image&           image,
        const SubResourceInfo* pSubResInfo,
        Addr3SwizzleMode       swizzleMode,
        uint32                 basePipeBankXor,
        uint32*                pPipeBankXor) const;

    Result ComputeAlignedPlaneDimensions(
        Image*                             pImage,
        SubResourceInfo*                   pBaseSubRes,
        TileInfo*                          pBaseTileInfo,
        Addr3SwizzleMode                   swizzleMode,
        ADDR3_COMPUTE_SURFACE_INFO_OUTPUT* pOut) const;

    Result ComputePlaneSwizzleMode(
        const Image*           pImage,
        const SubResourceInfo* pBaseSubRes,
        bool                   forFmask,
        Addr3SwizzleMode*      pFinalMode) const;

    Result InitSubresourceInfo(
        Image*                                    pImage,
        SubResourceInfo*                          pSubResInfo,
        TileInfo*                                 pTileInfo,
        Addr3SwizzleMode                          swizzleMode,
        const ADDR3_COMPUTE_SURFACE_INFO_OUTPUT&  surfaceInfo) const;

    bool SwizzleTypeWithinMemoryBudget(
        uint32                      minSize,
        uint64                      newSwizzleTypeSize,
        uint32                      ratioLow,
        uint32                      ratioHi,
        float                       memoryBudget         = 0.0f,
        bool                        newSwizzleTypeBigger = true) const;

    Addr3SwizzleMode GetValidSwizzleMode(
        uint32                  possibleSwSet,
        Addr3SwizzleMode        outputSw) const;
    };

} // AddrMgr3
} // Pal
