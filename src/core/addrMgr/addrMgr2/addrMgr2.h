/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_BUILD_GFX11
static_assert(static_cast<uint32>(ADDR_SW_256KB_Z_X) == static_cast<uint32>(ADDR_SW_VAR_Z_X),
              "mismatched assumption expecting same swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_256KB_R_X) == static_cast<uint32>(ADDR_SW_VAR_R_X),
              "mismatched assumption expecting same swizzle enum value");

// Bitmasks for swizzle mode determination on GFX11
constexpr uint32 Gfx11LinearSwModeMask = (1u << ADDR_SW_LINEAR);

constexpr uint32 Gfx11Blk256BSwModeMask = (1u << ADDR_SW_256B_D);

constexpr uint32 Gfx11Blk4KBSwModeMask = (1u << ADDR_SW_4KB_S)   |
                                         (1u << ADDR_SW_4KB_D)   |
                                         (1u << ADDR_SW_4KB_S_X) |
                                         (1u << ADDR_SW_4KB_D_X);

constexpr uint32 Gfx11Blk64KBSwModeMask = (1u << ADDR_SW_64KB_S)   |
                                          (1u << ADDR_SW_64KB_D)   |
                                          (1u << ADDR_SW_64KB_S_T) |
                                          (1u << ADDR_SW_64KB_D_T) |
                                          (1u << ADDR_SW_64KB_Z_X) |
                                          (1u << ADDR_SW_64KB_S_X) |
                                          (1u << ADDR_SW_64KB_D_X) |
                                          (1u << ADDR_SW_64KB_R_X);

constexpr uint32 Gfx11Blk256KBSwModeMask = (1u << ADDR_SW_256KB_Z_X) |
                                           (1u << ADDR_SW_256KB_S_X) |
                                           (1u << ADDR_SW_256KB_D_X) |
                                           (1u << ADDR_SW_256KB_R_X);

constexpr uint32 Gfx11ZSwModeMask = (1u << ADDR_SW_64KB_Z_X) | (1u << ADDR_SW_256KB_Z_X);

constexpr uint32 Gfx11StandardSwModeMask = (1u << ADDR_SW_4KB_S)    |
                                           (1u << ADDR_SW_64KB_S)   |
                                           (1u << ADDR_SW_64KB_S_T) |
                                           (1u << ADDR_SW_4KB_S_X)  |
                                           (1u << ADDR_SW_64KB_S_X) |
                                           (1u << ADDR_SW_256KB_S_X);

constexpr uint32 Gfx11DisplaySwModeMask = (1u << ADDR_SW_256B_D)   |
                                          (1u << ADDR_SW_4KB_D)    |
                                          (1u << ADDR_SW_64KB_D)   |
                                          (1u << ADDR_SW_64KB_D_T) |
                                          (1u << ADDR_SW_4KB_D_X)  |
                                          (1u << ADDR_SW_64KB_D_X) |
                                          (1u << ADDR_SW_256KB_D_X);

constexpr uint32 Gfx11RenderSwModeMask = (1u << ADDR_SW_64KB_R_X) | (1u << ADDR_SW_256KB_R_X);

constexpr uint32 Gfx11XSwModeMask = (1u << ADDR_SW_4KB_S_X)  |
                                    (1u << ADDR_SW_4KB_D_X)  |
                                    (1u << ADDR_SW_64KB_Z_X) |
                                    (1u << ADDR_SW_64KB_S_X) |
                                    (1u << ADDR_SW_64KB_D_X) |
                                    (1u << ADDR_SW_64KB_R_X) |
                                    Gfx11Blk256KBSwModeMask;

constexpr uint32 Gfx11TSwModeMask = (1u << ADDR_SW_64KB_S_T) | (1u << ADDR_SW_64KB_D_T);

constexpr uint32 Gfx11XorSwModeMask = Gfx11XSwModeMask | Gfx11TSwModeMask;

constexpr uint32 Gfx11Rsrc3dSwModeMask = Gfx11LinearSwModeMask    |
                                         Gfx11StandardSwModeMask  |
                                         Gfx11ZSwModeMask         |
                                         Gfx11RenderSwModeMask    |
                                         (1u << ADDR_SW_64KB_D_X) |
                                         (1u << ADDR_SW_256KB_D_X);

constexpr uint32 Gfx11Rsrc3dThin64KBSwModeMask = (1u << ADDR_SW_64KB_Z_X) | (1u << ADDR_SW_64KB_R_X);

constexpr uint32 Gfx11Rsrc3dThin256KBSwModeMask = (1u << ADDR_SW_256KB_Z_X) | (1u << ADDR_SW_256KB_R_X);

constexpr uint32 Gfx11Rsrc3dThinSwModeMask = Gfx11Rsrc3dThin64KBSwModeMask | Gfx11Rsrc3dThin256KBSwModeMask;

constexpr uint32 Gfx11Rsrc3dThickSwModeMask = Gfx11Rsrc3dSwModeMask &
                                              ~(Gfx11Rsrc3dThinSwModeMask | Gfx11LinearSwModeMask);

constexpr uint32 Gfx11Rsrc3dThick4KBSwModeMask = Gfx11Rsrc3dThickSwModeMask & Gfx11Blk4KBSwModeMask;

constexpr uint32 Gfx11Rsrc3dThick64KBSwModeMask = Gfx11Rsrc3dThickSwModeMask & Gfx11Blk64KBSwModeMask;

constexpr uint32 Gfx11Rsrc3dThick256KBSwModeMask = Gfx11Rsrc3dThickSwModeMask & Gfx11Blk256KBSwModeMask;

#endif

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
    const Image*    pImage,
    const SubresId& subRes)
{
    return GetTileInfo(pImage, pImage->CalcSubresourceId(subRes));
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
    AddrSwizzleMode  swizzleMode)
{
    return ((swizzleMode == ADDR_SW_LINEAR) ||
            (swizzleMode == ADDR_SW_LINEAR_GENERAL));
}

// =====================================================================================================================
constexpr bool IsSwizzleModeComputeOnly(
    AddrSwizzleMode  swizzleMode)
{
    return false;
}

// =====================================================================================================================
// Returns true if the associated swizzle mode is PRT capable
constexpr bool IsPrtSwizzle(
    AddrSwizzleMode  swizzleMode)
{
    return ((swizzleMode == ADDR_SW_64KB_Z_T)
            || (swizzleMode == ADDR_SW_64KB_S_T)
            || (swizzleMode == ADDR_SW_64KB_D_T)
            || (swizzleMode == ADDR_SW_64KB_R_T)
           );
}

// =====================================================================================================================
// Returns true for standard (as opposed to depth, displayable, rotated, etc.) swizzle modes
constexpr bool IsStandardSwzzle(
    AddrSwizzleMode  swizzleMode)
{
    return ((swizzleMode == ADDR_SW_256B_S)
            || (swizzleMode == ADDR_SW_4KB_S)
            || (swizzleMode == ADDR_SW_64KB_S)
            || (swizzleMode == ADDR_SW_64KB_S_T)
            || (swizzleMode == ADDR_SW_4KB_S_X)
            || (swizzleMode == ADDR_SW_64KB_S_X)
#if PAL_BUILD_GFX11
            || (swizzleMode == ADDR_SW_256KB_S_X)
#endif
           );
}

// =====================================================================================================================
// Returns true if the associated swizzle mode is a 256 mode;
constexpr bool Is256BSwizzle(
    AddrSwizzleMode  swizzleMode)
{
    return ((swizzleMode == ADDR_SW_256B_S)
            || (swizzleMode == ADDR_SW_256B_D)
            || (swizzleMode == ADDR_SW_256B_R)
           );
}

// =====================================================================================================================
// Returns true if the associated swzzle mode works with Z-buffers
constexpr bool IsZSwizzle(
    AddrSwizzleMode  swizzleMode)
{
    return ((swizzleMode == ADDR_SW_4KB_Z)
            || (swizzleMode == ADDR_SW_64KB_Z)
            || (swizzleMode == ADDR_SW_64KB_Z_T)
            || (swizzleMode == ADDR_SW_4KB_Z_X)
            || (swizzleMode == ADDR_SW_64KB_Z_X)
#if PAL_BUILD_GFX11
            || (swizzleMode == ADDR_SW_256KB_Z_X)  // reused enum from ADDR_SW_VAR_Z_X
#else
            || (swizzleMode == ADDR_SW_VAR_Z_X)
#endif
           );
}

// =====================================================================================================================
// Returns true for displayable (as opposed to depth, rotated, standard, etc.) swizzle modes
constexpr bool IsDisplayableSwizzle(
    AddrSwizzleMode  swizzleMode)
{
    return ((swizzleMode == ADDR_SW_256B_D)
            || (swizzleMode == ADDR_SW_4KB_D)
            || (swizzleMode == ADDR_SW_4KB_D_X)
            || (swizzleMode == ADDR_SW_64KB_D)
            || (swizzleMode == ADDR_SW_64KB_D_T)
            || (swizzleMode == ADDR_SW_64KB_D_X)
#if PAL_BUILD_GFX11
            || (swizzleMode == ADDR_SW_256KB_D_X)
#endif
           );
}

// =====================================================================================================================
constexpr bool IsRotatedSwizzle(
    AddrSwizzleMode  swizzleMode)
{
    return ((swizzleMode == ADDR_SW_256B_R)
            || (swizzleMode == ADDR_SW_4KB_R)
            || (swizzleMode == ADDR_SW_4KB_R_X)
            || (swizzleMode == ADDR_SW_64KB_R)
            || (swizzleMode == ADDR_SW_64KB_R_T)
            || (swizzleMode == ADDR_SW_64KB_R_X)
#if PAL_BUILD_GFX11
            || (swizzleMode == ADDR_SW_256KB_R_X)  // reused enum from ADDR_SW_VAR_R_X
#else
            || (swizzleMode == ADDR_SW_VAR_R_X)
#endif
           );
}

// =====================================================================================================================
// Returns true if the associated swizzle mode works with pipe-bank-xor values
constexpr bool IsXorSwizzle(
    AddrSwizzleMode  swizzleMode)
{
    return ((swizzleMode == ADDR_SW_4KB_Z_X)
            || (swizzleMode == ADDR_SW_4KB_S_X)
            || (swizzleMode == ADDR_SW_4KB_D_X)
            || (swizzleMode == ADDR_SW_4KB_R_X)
            || (swizzleMode == ADDR_SW_64KB_Z_X)
            || (swizzleMode == ADDR_SW_64KB_S_X)
            || (swizzleMode == ADDR_SW_64KB_D_X)
            || (swizzleMode == ADDR_SW_64KB_R_X)
#if PAL_BUILD_GFX11
            || (swizzleMode == ADDR_SW_256KB_Z_X)  // reused enum from ADDR_SW_VAR_Z_X
            || (swizzleMode == ADDR_SW_256KB_S_X)
            || (swizzleMode == ADDR_SW_256KB_D_X)
            || (swizzleMode == ADDR_SW_256KB_R_X)  // reused enum from ADDR_SW_VAR_R_X
#else
            || (swizzleMode == ADDR_SW_VAR_Z_X)
            || (swizzleMode == ADDR_SW_VAR_R_X)
#endif
           );
}

// =====================================================================================================================
// Returns true if it is non BC view compatible swizzle mode.
constexpr bool IsNonBcViewCompatible(
    AddrSwizzleMode swizzleMode,
    ImageType       imageType)
{
    //2D or 3D with 3dThin swizzle mode.
    return ((imageType == ImageType::Tex2d) ||
            ((imageType == ImageType::Tex3d) &&
             ((swizzleMode == ADDR_SW_64KB_Z_X)
              || (swizzleMode == ADDR_SW_64KB_R_X)
#if PAL_BUILD_GFX11
              || (swizzleMode == ADDR_SW_256KB_Z_X)
              || (swizzleMode == ADDR_SW_256KB_R_X)
#endif
            )));
}

// =====================================================================================================================
// Returns the swizzle type for a given swizzle modes.
inline AddrSwType GetSwizzleType(
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
inline AddrSwType GetMicroSwizzle(
    AddrSwizzleMode swizzleMode)
{
    // It's illegal to call this on linear modes.
    PAL_ASSERT((swizzleMode != ADDR_SW_LINEAR) && (swizzleMode != ADDR_SW_LINEAR_GENERAL));

    return GetSwizzleType(swizzleMode);
}

// =====================================================================================================================
// Returns the HW value of "EPITCH" for the supplied addr-output.
inline uint32 CalcEpitch(
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
class AddrMgr2 final : public AddrMgr
{
public:
    explicit AddrMgr2(const Device*  pDevice);
    virtual ~AddrMgr2() {}

    Pal::Gfx9::SWIZZLE_MODE_ENUM GetHwSwizzleMode(AddrSwizzleMode  swizzleMode) const;

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
        uint32       plane,
        bool         forFmask) const;

    static bool IsValidToOverride(AddrSwizzleMode primarySwMode, ADDR2_SWMODE_SET validSwModeSet);

    static AddrResourceType GetAddrResourceType(const Pal::Image*  pImage);

    virtual uint32 GetBlockSize(AddrSwizzleMode swizzleMode) const override;

protected:
    virtual void ComputeTilesInMipTail(
        const Image&       image,
        uint32             plane,
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

#if PAL_BUILD_GFX11
    ADDR_E_RETURNCODE Gfx11ChooseSwizzleMode(
        const SubResourceInfo*                        pBaseSubRes,
        const ADDR2_GET_PREFERRED_SURF_SETTING_INPUT* pIn,
        ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT*      pOut) const;
#endif

    ADDR_E_RETURNCODE GetPreferredSurfaceSetting(
        const SubResourceInfo*                        pBaseSubRes,
        bool                                          newSwizzleModeDetermination,
        const ADDR2_GET_PREFERRED_SURF_SETTING_INPUT* pIn,
        ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT*      pOut) const;

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

    Result InitSubresourceInfo(
        Image*                                         pImage,
        SubResourceInfo*                               pSubResInfo,
        TileInfo*                                      pTileInfo,
        const ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT& surfaceSetting,
        const ADDR2_COMPUTE_SURFACE_INFO_OUTPUT&       surfaceInfo) const;

    uint32 GetStereoRightEyePipeBankXor(
        const Image&                                   image,
        const SubResourceInfo*                         pSubResInfo,
        const ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT& surfaceSetting,
        uint32                                         basePipeBankXor) const;

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
