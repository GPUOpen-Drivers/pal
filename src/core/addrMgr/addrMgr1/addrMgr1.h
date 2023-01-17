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

#include "core/addrMgr/addrMgr.h"
#include "core/image.h"
#include "palInlineFuncs.h"

namespace Pal
{
class   Device;
struct  SubResourceInfo;

namespace AddrMgr1
{

// Define a new constant which represents an invalid AddrTileType
constexpr AddrTileType TileTypeInvalid = static_cast<AddrTileType>(0x7);

// Bitfield of caps to control supported tiling modes
union TilingCaps
{
    struct
    {
        uint32   tile1DThin1  : 1;    // support ARRAY_1D_TILED_THIN1/Micro tiling
        uint32   tile1DThick  : 1;    // support ARRAY_1D_TILED_THICK
        uint32   tile2DThin1  : 1;    // support ARRAY_2D_TILED_THIN1
        uint32   tile2DThick  : 1;    // support ARRAY_2D_TILED_THICK
        uint32   tile2DXThick : 1;    // support ARRAY_2D_TILED_XTHICK__NI
        uint32   tile3DThin1  : 1;    // support ARRAY_3D_TILED_THIN1
        uint32   tile3DThick  : 1;    // support ARRAY_3D_TILED_THICK
        uint32   tile3DXThick : 1;    // support ARRAY_3D_TILED_XTHICK__NI
        uint32   tilePrtThin1 : 1;    // support ARRAY_PRT_TILED_THIN1
        uint32   reserved     : 23;
    };
    uint32 value;
};

// Unique image tile token.
union TileToken
{
    struct
    {
        uint32 tileMode         :  5; // Tile mode (ARRAY_MODE)
        uint32 bankHeight       :  2; // Bank Height
        uint32 bankWidth        :  2; // Bank width
        uint32 banks            :  2; // Number of banks
        uint32 macroAspectRatio :  2; // Macro tile aspect ratio
        uint32 tileType         :  3; // Micro tiling type (MICRO_TILE_MODE)
        uint32 tileSplitBytes   :  3; // Tile split size
        uint32 elementSize      :  3; // Log2 of bytes per pixel
        uint32 reserved         : 10; // Reserved for future use
    } bits;

    uint32 u32All;
};

// Tiling info structure
struct TileInfo
{
    int32  tileIndex;                   // Tile mode table index
    int32  macroModeIndex;              // Macro tile mode table index
    bool   childMipsNeedPrtTileIndex;   // Gfx6 only. Child mips for this mip 0 subresource need to specify that the
                                        // returned tile index is for PRT.

    uint32 tileMode;                    // Tile mode (ARRAY_MODE)
    uint32 tileType;                    // Micro tiling type (MICRO_TILE_MODE)

    uint32 banks;                       // Number of banks
    uint32 bankWidth;                   // Number of tiles in the X direction in the same bank
    uint32 bankHeight;                  // Number of tiles in the Y direction in the same bank
    uint32 macroAspectRatio;            // Macro tile aspect ratio
    uint32 tileSplitBytes;              // Tile split size
    uint32 pipeConfig;                  // Pipe Config = HW enum

    uint32 tileSwizzle;                 // Bank/Pipe swizzle bits for macro-tiling modes
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
inline TileInfo* NonConstTileInfo(
    void*  pTileInfoList,
    uint32 subResIdx)
{
    constexpr uint32 BytesPerSubRes = (sizeof(TileInfo) + sizeof(TilingCaps));
    return static_cast<TileInfo*>(Util::VoidPtrInc(pTileInfoList, (subResIdx * BytesPerSubRes)));
}

// =====================================================================================================================
// Returns a pointer to the tiling capabilities for the subresource with the given index. For each subresource, the
// tiling caps immediately follows the tile info.
inline const TilingCaps* GetTilingCaps(
    const Image* pImage,
    uint32       subResIdx)
{
    return reinterpret_cast<const TilingCaps*>(GetTileInfo(pImage, subResIdx) + 1);
}

// =====================================================================================================================
// Returns a non-const pointer to the tiling capabilities for the subresource with the given index, given the non-const
// pointer to the entire tiling info list for the Image.
inline TilingCaps* NonConstTilingCaps(
    void*  pTileInfoList,
    uint32 subResIdx)
{
    return reinterpret_cast<TilingCaps*>(NonConstTileInfo(pTileInfoList, subResIdx) + 1);
}

// =====================================================================================================================
// Determines if the specified tile mode is linear
constexpr bool IsLinearTiled(
    AddrTileMode tileMode)
{
    return ((tileMode == ADDR_TM_LINEAR_GENERAL) ||
            (tileMode == ADDR_TM_LINEAR_ALIGNED));
}

// =====================================================================================================================
// Determines if the specified tile mode is a PRT tile mode
constexpr bool IsPrtTiled(
    AddrTileMode tileMode)
{
    return ((tileMode == ADDR_TM_PRT_TILED_THIN1)    ||
            (tileMode == ADDR_TM_PRT_2D_TILED_THIN1) ||
            (tileMode == ADDR_TM_PRT_3D_TILED_THIN1) ||
            (tileMode == ADDR_TM_PRT_TILED_THICK)    ||
            (tileMode == ADDR_TM_PRT_2D_TILED_THICK) ||
            (tileMode == ADDR_TM_PRT_3D_TILED_THICK));
}

// =====================================================================================================================
// Converts a h/w ARRAY_MODE value to an AddrTileMode enum
inline AddrTileMode AddrTileModeFromHwArrayMode(
    uint32 hwArrayMode)            // H/W value as programmed in GB_TILE_MODE#.ARRAY_MODE
{
    constexpr AddrTileMode AddrTileFromHwTile[] =
    {
        ADDR_TM_LINEAR_GENERAL,     // ARRAY_LINEAR_GENERAL
        ADDR_TM_LINEAR_ALIGNED,     // ARRAY_LINEAR_ALIGNED
        ADDR_TM_1D_TILED_THIN1,     // ARRAY_1D_TILED_THIN1
        ADDR_TM_1D_TILED_THICK,     // ARRAY_1D_TILED_THICK
        ADDR_TM_2D_TILED_THIN1,     // ARRAY_2D_TILED_THIN1
        ADDR_TM_PRT_TILED_THIN1,    // ARRAY_2D_TILED_THIN2__SI, ARRAY_PRT_TILED_THIN1__CI__VI
        ADDR_TM_PRT_2D_TILED_THIN1, // ARRAY_2D_TILED_THIN4__SI, ARRAY_PRT_2D_TILED_THIN1__CI__VI
        ADDR_TM_2D_TILED_THICK,     // ARRAY_2D_TILED_THICK
        ADDR_TM_2D_TILED_XTHICK,    // ARRAY_2D_TILED_XTHICK
        ADDR_TM_PRT_TILED_THICK,    // ARRAY_2B_TILED_THIN2__SI, ARRAY_PRT_TILED_THICK__CI__VI
        ADDR_TM_PRT_2D_TILED_THICK, // ARRAY_2B_TILED_THIN4__SI, ARRAY_PRT_2D_TILED_THICK__CI__VI
        ADDR_TM_PRT_3D_TILED_THIN1, // ARRAY_2B_TILED_THICK__SI, ARRAY_PRT_3D_TILED_THIN1__CI__VI
        ADDR_TM_3D_TILED_THIN1,     // ARRAY_3D_TILED_THIN1
        ADDR_TM_3D_TILED_THICK,     // ARRAY_3D_TILED_THICK
        ADDR_TM_3D_TILED_XTHICK,    // ARRAY_3D_TILED_XTHICK
        ADDR_TM_PRT_3D_TILED_THICK, // ARRAY_POWER_SAVE__SI    , ARRAY_PRT_3D_TILED_THICK__CI__VI
    };

    PAL_ASSERT (hwArrayMode < (sizeof(AddrTileFromHwTile) / sizeof(AddrTileMode)));

    return AddrTileFromHwTile[hwArrayMode];
}

// =====================================================================================================================
// Converts a h/w MICRO_TILE_MODE value to an AddrTileType enum
inline AddrTileType AddrTileTypeFromHwMicroTileMode(
    uint32 hwTileMode)            // H/W value as programmed in GB_TILE_MODE#.MICRO_TILE_MODE
{
    // Note that this table is missing ADDR_SURF_THICK_MICRO_TILING__SI but it shouldn't actually be used.
    constexpr AddrTileType AddrTileFromHwTile[] =
    {
        ADDR_DISPLAYABLE,        // ADDR_SURF_DISPLAY_MICRO_TILING
        ADDR_NON_DISPLAYABLE,    // ADDR_SURF_THIN_MICRO_TILING
        ADDR_DEPTH_SAMPLE_ORDER, // ADDR_SURF_DEPTH_MICRO_TILING
        ADDR_ROTATED,            // ADDR_SURF_ROTATED_MICRO_TILING__CI__VI
        ADDR_THICK,              // ADDR_SURF_THICK_MICRO_TILING__CI__VI
    };

    PAL_ASSERT (hwTileMode < (sizeof(AddrTileFromHwTile) / sizeof(AddrTileType)));

    return AddrTileFromHwTile[hwTileMode];
}

// =====================================================================================================================
// Determines if the specified tile mode is a macro tile mode
inline bool IsMacroTiled(
    AddrTileMode  tileMode)
{
    bool ret = true;

    // Excludes linear and 1D tiling modes
    switch (tileMode)
    {
    case ADDR_TM_LINEAR_GENERAL:
    case ADDR_TM_LINEAR_ALIGNED:
    case ADDR_TM_1D_TILED_THIN1:
    case ADDR_TM_1D_TILED_THICK:
        ret = false;
        break;
    default:
        break;
    }

    return ret;
}

// =====================================================================================================================
// Responsible for implementing address and tiling code that is specific to "version 1" of the address library
// interface.  Corresponds to ASICs prior to GFX9.
class AddrMgr1 final : public AddrMgr
{
public:
    explicit AddrMgr1(const Device* pDevice);
    virtual ~AddrMgr1() {}

    virtual Result InitSubresourcesForImage(
        Image*             pImage,
        gpusize*           pGpuMemSize,
        gpusize*           pGpuMemAlignment,
        ImageMemoryLayout* pGpuMemLayout,
        SubResourceInfo*   pSubResInfoList,
        void*              pSubResTileInfoList,
        bool*              pDccUnsupported) const override;

    virtual uint32 GetTileSwizzle(const Image* pImage, SubresId subresource) const override
        { return GetTileInfo(pImage, subresource)->tileSwizzle; }

protected:
    virtual void ComputeTilesInMipTail(
        const Image&       image,
        uint32             plane,
        ImageMemoryLayout* pGpuMemLayout) const override;

private:
    Result AdjustChromaPlane(
        Image*              pImage,
        SubResourceInfo*    pSubResInfoList,
        void*               pSubResTileInfoList,
        uint32              subResIdx,
        ImageMemoryLayout*  pGpuMemLayout) const;

    ADDR_E_RETURNCODE CalcSurfInfoOut(
        Image*                             pImage,
        SubResourceInfo*                   pSubResInfoList,
        void*                              pSubResTileInfoList,
        uint32                             subResIdx,
        uint32                             subResWidth,
        uint32                             subResHeight,
        ImageMemoryLayout*                 pGpuMemLayout,
        bool*                              pDccUnsupported,
        int32*                             pStencilTileIdx,
        ADDR_COMPUTE_SURFACE_INFO_INPUT*   pSurfInfoInput,
        ADDR_COMPUTE_SURFACE_INFO_OUTPUT*  pSurfInfoOutput) const;

    void InitTilingCaps(
        Image* pImage,
        uint32 subResIdx,
        void*  pTileInfoList) const;

    Result ComputeSubResourceInfo(
        Image*             pImage,
        SubResourceInfo*   pSubResInfoList,
        void*              pSubResTileInfoList,
        uint32             subResIdx,
        ImageMemoryLayout* pGpuMemLayout,
        bool*              pDccUnsupported,
        int32*             pStencilTileIdx) const;

    void BuildTileToken(
        SubResourceInfo* pSubResInfo,
        const TileInfo*  pTileInfo) const;

    static constexpr uint32 Addr1TilingCaps = 0x1FF;

    PAL_DISALLOW_DEFAULT_CTOR(AddrMgr1);
    PAL_DISALLOW_COPY_AND_ASSIGN(AddrMgr1);
};

} // AddrMgr1
} // Pal
