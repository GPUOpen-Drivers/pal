/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/chip/gfx9_plus_merged_enum.h"

namespace Pal
{
class   Device;

namespace AddrMgr2
{

// Unique image tile token.
union TileToken
{
    struct
    {
        uint32 elementSize  :  3; // Log2 of bytes per pixel
        uint32 swizzleMode  :  3; // Basic swizzle mode
        uint32 reserved     : 26; // Reserved for future use
    } bits;

    uint32 u32All;
};

// Tiling info structure
struct TileInfo
{
    gpusize  backingStoreOffset; // Offset to this subresource within the KMD's linear backing store for the Image. This
                                 // is relative to the beginning of the Image.
    uint32   pipeBankXor;        // Pipe/bank XOR value for this subresource
    uint32   ePitch;             // The width or height of the mip chain, whichever is larger, minus 1
    bool     mip0InMipTail;      // flag indicates mip0 is in mip tail
    gpusize  mipTailMask;        // mask for mip tail offset
};

// =====================================================================================================================
// Returns a pointer to the tiling info for the subresource with the given index.
PAL_INLINE const TileInfo* GetTileInfo(
    const Image* pImage,
    uint32       subResIdx)
{
    PAL_ASSERT(pImage != nullptr);
    return static_cast<const TileInfo*>(pImage->SubresourceTileInfo(subResIdx));
}

// =====================================================================================================================
// Returns a pointer to the tiling info for the given subresource.
PAL_INLINE const TileInfo* GetTileInfo(
    const Image*    pImage,
    const SubresId& subRes)
{
    return GetTileInfo(pImage, pImage->CalcSubresourceId(subRes));
}

// =====================================================================================================================
// Returns a non-const pointer to the tiling info for the subresource with the given index, given the non-const pointer
// to the entire tiling info list for the Image.
PAL_INLINE TileInfo* NonConstTileInfo(
    void*  pTileInfoList,
    uint32 subResIdx)
{
    return static_cast<TileInfo*>(Util::VoidPtrInc(pTileInfoList, (subResIdx * sizeof(TileInfo))));
}

// =====================================================================================================================
static bool IsLinearSwizzleMode(
    AddrSwizzleMode  swizzleMode)
{
    return ((swizzleMode == ADDR_SW_LINEAR) ||
            (swizzleMode == ADDR_SW_LINEAR_GENERAL));
}

// =====================================================================================================================
// Returns the HW tiling / swizzle mode that corresponds to the specified subresource.
static SWIZZLE_MODE_ENUM GetHwSwizzleMode(
    AddrSwizzleMode  swizzleMode)
{
    static constexpr SWIZZLE_MODE_ENUM  hwSwizzleMode[]=
    {
        SW_LINEAR,      // ADDR_SW_LINEAR
        SW_256B_S,      // ADDR_SW_256B_S
        SW_256B_D,      // ADDR_SW_256B_D
        SW_256B_R,      // ADDR_SW_256B_R
        SW_4KB_Z,       // ADDR_SW_4KB_Z
        SW_4KB_S,       // ADDR_SW_4KB_S
        SW_4KB_D,       // ADDR_SW_4KB_D
        SW_4KB_R,       // ADDR_SW_4KB_R
        SW_64KB_Z,      // ADDR_SW_64KB_Z
        SW_64KB_S,      // ADDR_SW_64KB_S
        SW_64KB_D,      // ADDR_SW_64KB_D
        SW_64KB_R,      // ADDR_SW_64KB_R
        SW_VAR_Z,       // ADDR_SW_VAR_Z
        SW_VAR_S,       // ADDR_SW_VAR_S
        SW_VAR_D,       // ADDR_SW_VAR_D
        SW_VAR_R,       // ADDR_SW_VAR_R
        SW_64KB_Z_T,    // ADDR_SW_64KB_Z_T
        SW_64KB_S_T,    // ADDR_SW_64KB_S_T
        SW_64KB_D_T,    // ADDR_SW_64KB_D_T
        SW_64KB_R_T,    // ADDR_SW_64KB_R_T
        SW_4KB_Z_X,     // ADDR_SW_4KB_Z_X
        SW_4KB_S_X,     // ADDR_SW_4KB_S_X
        SW_4KB_D_X,     // ADDR_SW_4KB_D_X
        SW_4KB_R_X,     // ADDR_SW_4KB_R_X
        SW_64KB_Z_X,    // ADDR_SW_64KB_Z_X
        SW_64KB_S_X,    // ADDR_SW_64KB_S_X
        SW_64KB_D_X,    // ADDR_SW_64KB_D_X
        SW_64KB_R_X,    // ADDR_SW_64KB_R_X
        SW_VAR_Z_X,     // ADDR_SW_VAR_Z_X
        SW_VAR_S_X,     // ADDR_SW_VAR_S_X
        SW_VAR_D_X,     // ADDR_SW_VAR_D_X
        SW_VAR_R_X,     // ADDR_SW_VAR_R_X
        SW_LINEAR,      // ADDR_SW_LINEAR_GENERAL
    };

    PAL_ASSERT (swizzleMode < (sizeof(hwSwizzleMode) / sizeof(SWIZZLE_MODE_ENUM)));

    return hwSwizzleMode[swizzleMode];
}

// =====================================================================================================================
// Returns true if the associated swizzle mode is PRT capable
static bool IsPrtSwizzle(
    AddrSwizzleMode  swizzleMode)
{
    return ((swizzleMode == ADDR_SW_64KB_Z_T) ||
            (swizzleMode == ADDR_SW_64KB_S_T) ||
            (swizzleMode == ADDR_SW_64KB_D_T) ||
            (swizzleMode == ADDR_SW_64KB_R_T));
}

// =====================================================================================================================
// Returns true for standard (as opposed to depth, displayable, rotated, etc.) swizzle modes
static bool IsStandardSwzzle(
    AddrSwizzleMode  swizzleMode)
{
    return ((swizzleMode == ADDR_SW_256B_S)    ||
            (swizzleMode == ADDR_SW_4KB_S)     ||
            (swizzleMode == ADDR_SW_64KB_S)    ||
            (swizzleMode == ADDR_SW_64KB_S_T)  ||
            (swizzleMode == ADDR_SW_4KB_S_X)   ||
            (swizzleMode == ADDR_SW_64KB_S_X));
}

// =====================================================================================================================
// Returns true if the associated swizzle mode works with Z-buffers
static bool IsZSwizzle(
    AddrSwizzleMode  swizzleMode)
{
    return ((swizzleMode == ADDR_SW_4KB_Z)    ||
            (swizzleMode == ADDR_SW_64KB_Z)   ||
            (swizzleMode == ADDR_SW_64KB_Z_T) ||
            (swizzleMode == ADDR_SW_4KB_Z_X)  ||
            (swizzleMode == ADDR_SW_64KB_Z_X) ||
            (swizzleMode == ADDR_SW_VAR_Z_X));
}

// =====================================================================================================================
// Returns true for displayable (as opposed to depth, rotated, standard, etc.) swizzle modes
static bool IsDisplayableSwizzle(
    AddrSwizzleMode  swizzleMode)
{
    return ((swizzleMode == ADDR_SW_256B_D)   ||
            (swizzleMode == ADDR_SW_4KB_D)    ||
            (swizzleMode == ADDR_SW_4KB_D_X)  ||
            (swizzleMode == ADDR_SW_64KB_D)   ||
            (swizzleMode == ADDR_SW_64KB_D_T) ||
            (swizzleMode == ADDR_SW_64KB_D_X));
}

// =====================================================================================================================
static bool IsRotatedSwizzle(
    AddrSwizzleMode  swizzleMode)
{
    return ((swizzleMode == ADDR_SW_256B_R)   ||
            (swizzleMode == ADDR_SW_4KB_R)    ||
            (swizzleMode == ADDR_SW_4KB_R_X)  ||
            (swizzleMode == ADDR_SW_64KB_R)   ||
            (swizzleMode == ADDR_SW_64KB_R_T) ||
            (swizzleMode == ADDR_SW_64KB_R_X) ||
            (swizzleMode == ADDR_SW_VAR_R_X));
}

// =====================================================================================================================
// Returns true if the associated swizzle mode works with pipe-bank-xor values
static bool IsXorSwizzle(
    AddrSwizzleMode  swizzleMode)
{
    return ((swizzleMode == ADDR_SW_4KB_Z_X)  ||
            (swizzleMode == ADDR_SW_4KB_S_X)  ||
            (swizzleMode == ADDR_SW_4KB_D_X)  ||
            (swizzleMode == ADDR_SW_4KB_R_X)  ||
            (swizzleMode == ADDR_SW_64KB_Z_X) ||
            (swizzleMode == ADDR_SW_64KB_S_X) ||
            (swizzleMode == ADDR_SW_64KB_D_X) ||
            (swizzleMode == ADDR_SW_64KB_R_X) ||
            (swizzleMode == ADDR_SW_VAR_Z_X)  ||
            (swizzleMode == ADDR_SW_VAR_R_X));
}

// =====================================================================================================================
// Returns the swizzle type for a given swizzle modes.
static AddrSwType GetSwizzleType(
    AddrSwizzleMode swizzleMode)
{
    // SW AddrLib will provide public enum ADDR_SW_MAX_SWTYPE/ADDR_SW_L for following private definition soon.
    constexpr uint32 InvalidSwizzleMode = 5;
    constexpr uint32 LinearSwizzleMode  = 4;
    static_assert(LinearSwizzleMode == (static_cast<uint32>(ADDR_SW_R) + 1),
                  "LinearSwizzleMode tile token is unexpected value!");

    uint32 swType = IsZSwizzle(swizzleMode)           ? ADDR_SW_Z         :
                    IsStandardSwzzle(swizzleMode)     ? ADDR_SW_S         :
                    IsDisplayableSwizzle(swizzleMode) ? ADDR_SW_D         :
                    IsRotatedSwizzle(swizzleMode)     ? ADDR_SW_R         :
                    IsLinearSwizzleMode(swizzleMode)  ? LinearSwizzleMode : InvalidSwizzleMode;

    PAL_ASSERT(swType != InvalidSwizzleMode);

    return static_cast<AddrSwType>(swType);
}

// =====================================================================================================================
// Returns the micro swizzle type of one of the non-linear swizzle modes.
static AddrSwType GetMicroSwizzle(
    AddrSwizzleMode swizzleMode)
{
    // It's illegal to call this on linear modes.
    PAL_ASSERT((swizzleMode != ADDR_SW_LINEAR) && (swizzleMode != ADDR_SW_LINEAR_GENERAL));

    return GetSwizzleType(swizzleMode);
}

// =====================================================================================================================
// Returns the HW value of "EPITCH" for the supplied addr-output.
static uint32 CalcEpitch(
    const ADDR2_COMPUTE_SURFACE_INFO_OUTPUT* pAddrOutput)
{
    uint32 ePitch = 0;

    if (pAddrOutput->epitchIsHeight != 0)
    {
        ePitch = pAddrOutput->mipChainHeight - 1;
    }
    else
    {
        ePitch = pAddrOutput->mipChainPitch - 1;
    }

    return ePitch;
}

// =====================================================================================================================
// Responsible for implementing address and tiling code that is specific to "version 1" of the address library
// interface.  Corresponds to ASICs starting with GFX9
class AddrMgr2 : public AddrMgr
{
public:
    explicit AddrMgr2(const Device*  pDevice);
    virtual ~AddrMgr2() {}

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

    Result ComputeFmaskSwizzleMode(
        const Image&                             image,
        ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT* pOut) const;

    ADDR2_SURFACE_FLAGS DetermineSurfaceFlags(
        const Image& image,
        ImageAspect  aspect) const;

    static bool IsValidToOverride(AddrSwizzleMode primarySwMode, ADDR2_SWMODE_SET validSwModeSet);

    virtual uint32 GetBlockSize(AddrSwizzleMode swizzleMode) const;

protected:
    virtual void ComputeTilesInMipTail(
        const Image&       image,
        ImageAspect        aspect,
        ImageMemoryLayout* pGpuMemLayout) const override;

private:
    static uint32 GetNumAddrLib3dSlices(
        const Pal::Image*                               pImage,
        const ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT&  surfSetting,
        const ADDR2_COMPUTE_SURFACE_INFO_OUTPUT&        surfInfoOut);

    void InitTilingCaps(
        const Image*        pImage,
        ADDR2_SURFACE_FLAGS surfaceFlags,
        ADDR2_BLOCK_SET*    pBlockSettings) const;

    Result ComputePlaneSwizzleMode(
        const Image*                             pImage,
        const SubResourceInfo*                   pBaseSubRes,   // Base subresource for the plane
        bool                                     forFmask,
        ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT* pOut) const;

    Result ComputeAlignedPlaneDimensions(
        Image*                             pImage,
        SubResourceInfo*                   pBaseSubRes,     // Base subresource for the plane
        TileInfo*                          pBaseTileInfo,   // Base subresource tiling info for the plane
        AddrSwizzleMode                    swizzleMode,
        ADDR2_COMPUTE_SURFACE_INFO_OUTPUT* pOut) const;

    static AddrResourceType GetAddrResourceType(const Pal::Image*  pImage);

    Result InitSubresourceInfo(
        Image*                                         pImage,
        SubResourceInfo*                               pSubResInfo,
        TileInfo*                                      pTileInfo,
        const ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT& surfaceSetting,
        const ADDR2_COMPUTE_SURFACE_INFO_OUTPUT&       surfaceInfo) const;

    void Gfx9InitSubresource(
        const SubResIterator&  subResIt,
        SubResourceInfo*       pSubResInfoList,
        void*                  pSubResTileInfoList,
        gpusize*               pGpuMemSize
        ) const;

    void Gfx10InitSubresource(
        const Pal::Image*      pImage,
        const SubResIterator&  subResIt,
        SubResourceInfo*       pSubResInfoList,
        void*                  pSubResTileInfoList,
        gpusize*               pGpuMemSize
        ) const;

    void BuildTileToken(
        SubResourceInfo* pSubResInfo,
        AddrSwizzleMode  swizzleMode) const;

    PAL_DISALLOW_DEFAULT_CTOR(AddrMgr2);
    PAL_DISALLOW_COPY_AND_ASSIGN(AddrMgr2);

    uint32 m_varBlockSize;
};

} // AddrMgr2
} // Pal
