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

#pragma once

#include "palDepthStencilView.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "core/hw/gfxip/gfx9/gfx9FormatInfo.h"
#include "core/hw/gfxip/gfx9/gfx9MaskRam.h"
#include "core/hw/gfxip/universalCmdBuffer.h"

namespace Pal
{

class CmdStream;
class Device;
class Image;

namespace Gfx9
{

// Set of context registers associated with a depth/stencil view object (Gfx10 version).
struct DepthStencilViewRegs
{
    regDB_RENDER_CONTROL             dbRenderControl;
    regDB_DEPTH_VIEW                 dbDepthView;
    regDB_RENDER_OVERRIDE2           dbRenderOverride2;
    regDB_HTILE_DATA_BASE            dbHtileDataBase;
    regDB_DEPTH_SIZE_XY              dbDepthSizeXy;
    regDB_Z_INFO                     dbZInfo;
    regDB_STENCIL_INFO               dbStencilInfo;
    regDB_Z_READ_BASE                dbZReadBase;
    regDB_STENCIL_READ_BASE          dbStencilReadBase;
    regDB_Z_WRITE_BASE               dbZWriteBase;
    regDB_STENCIL_WRITE_BASE         dbStencilWriteBase;
    regDB_HTILE_SURFACE              dbHtileSurface;
    regPA_SU_POLY_OFFSET_DB_FMT_CNTL paSuPolyOffsetDbFmtCntl;
    regCOHER_DEST_BASE_0             coherDestBase0;
    regDB_RENDER_OVERRIDE            dbRenderOverride;
    regDB_RMI_L2_CACHE_CONTROL       dbRmiL2CacheControl;

    // Add these five registers to support PAL's high-address bits on GFX10 DB
    regDB_Z_READ_BASE_HI             dbZReadBaseHi;
    regDB_Z_WRITE_BASE_HI            dbZWriteBaseHi;
    regDB_STENCIL_READ_BASE_HI       dbStencilReadBaseHi;
    regDB_STENCIL_WRITE_BASE_HI      dbStencilWriteBaseHi;
    regDB_HTILE_DATA_BASE_HI         dbHtileDataBaseHi;

    gpusize  fastClearMetadataGpuVa;
    gpusize  hiSPretestMetadataGpuVa;
};

// =====================================================================================================================
// Gfx9 HW-specific implementation of the Pal::IDepthStencilView interface
class DepthStencilView final : public IDepthStencilView
{
public:
    DepthStencilView(
        const Device*                             pDevice,
        const DepthStencilViewCreateInfo&         createInfo,
        const DepthStencilViewInternalCreateInfo& internalInfo,
        uint32                                    uniqueId);

    DepthStencilView(const DepthStencilView&) = default;
    DepthStencilView& operator=(const DepthStencilView&) = default;
    DepthStencilView(DepthStencilView&&) = default;
    DepthStencilView& operator=(DepthStencilView&&) = default;

    uint32* WriteCommands(
        ImageLayout            depthLayout,
        ImageLayout            stencilLayout,
        CmdStream*             pCmdStream,
        bool                   isNested,
        regDB_RENDER_OVERRIDE* pDbRenderOverride,
        uint32*                pCmdSpace) const;

    const Image* GetImage() const { return m_pImage; }
    uint32 MipLevel() const { return m_depthSubresource.mipLevel; }

    bool ReadOnlyDepth() const { return m_flags.readOnlyDepth; }
    bool ReadOnlyStencil() const { return m_flags.readOnlyStencil; }
    bool VrsImageIncompatible() const { return m_flags.vrsImageIncompatible; }

    uint32* HandleBoundTargetChanged(const Pal::UniversalCmdBuffer* pCmdBuffer, uint32* pCmdSpace) const;

    Extent2d GetExtent() const { return m_extent; }

    bool Equals(const DepthStencilView* pOther) const;

    uint32 BaseArraySlice() const { return m_baseArraySlice; }
    uint32 ArraySize()      const { return m_arraySize; }

    static uint32* WriteUpdateFastClearDepthStencilValue(
        uint32     metaDataClearFlags,
        float      depth,
        uint8      stencil,
        CmdStream* pCmdStream,
        uint32*    pCmdSpace);

    static void SetGfx11StaticDbRenderControlFields(
        const Device&         device,
        const uint8           numFragments,
        regDB_RENDER_CONTROL* pDbRenderControl);

    regDB_Z_INFO DbZInfo() const { return m_regs.dbZInfo; }

    static constexpr uint32 DbRenderOverrideRmwMask = DB_RENDER_OVERRIDE__FORCE_HIZ_ENABLE_MASK        |
                                                      DB_RENDER_OVERRIDE__FORCE_HIS_ENABLE0_MASK       |
                                                      DB_RENDER_OVERRIDE__FORCE_HIS_ENABLE1_MASK       |
                                                      DB_RENDER_OVERRIDE__FORCE_STENCIL_VALID_MASK     |
                                                      DB_RENDER_OVERRIDE__FORCE_STENCIL_VALID_MASK     |
                                                      DB_RENDER_OVERRIDE__FORCE_Z_VALID_MASK           |
                                                      DB_RENDER_OVERRIDE__DISABLE_TILE_RATE_TILES_MASK |
                                                      DB_RENDER_OVERRIDE__NOOP_CULL_DISABLE_MASK;

protected:
    virtual ~DepthStencilView()
    {
        // This destructor, and the destructors of all member and base classes, must always be empty: PAL depth stencil
        // views guarantee to the client that they do not have to be explicitly destroyed.
        PAL_NEVER_CALLED();
    }

    void InitRegistersCommon(
        const Device&                             device,
        const DepthStencilViewCreateInfo&         createInfo,
        const DepthStencilViewInternalCreateInfo& internalInfo,
        const Formats::Gfx9::MergedFlatFmtInfo*   pFmtInfo,
        DepthStencilViewRegs*                     pRegs);

    void InitRegisters(
        const Device&                             device,
        const DepthStencilViewCreateInfo&         createInfo,
        const DepthStencilViewInternalCreateInfo& internalInfo);

    void UpdateImageVa(DepthStencilViewRegs* pRegs) const;

    uint32* WriteCommandsCommon(
        ImageLayout           depthLayout,
        ImageLayout           stencilLayout,
        CmdStream*            pCmdStream,
        uint32*               pCmdSpace,
        DepthStencilViewRegs* pRegs) const;

    // Bitfield describing the metadata and settings that are supported by this view.
    union
    {
        struct
        {
            uint32 hTile                   :  1;
            uint32 depth                   :  1;
            uint32 stencil                 :  1;
            uint32 readOnlyDepth           :  1; // Set if the depth plane is present and is read-only
            uint32 readOnlyStencil         :  1; // Set if the stencil plane is present and is read-only
            uint32 depthMetadataTexFetch   :  1;
            uint32 stencilMetadataTexFetch :  1;
            uint32 vrsOnlyDepth            :  1; // Set if the image is used for VRS-only depth
            uint32 hiSPretests             :  1; // Set if the image has HiS pretest metadata
            uint32 dbRenderOverrideLocked  :  1; // Set if DB_RENDER_OVERRIDE cannot change due to bind-time
                                                 // compression state.
            uint32 dbRenderControlLocked   :  1; // Set if DB_RENDER_CONTROL cannot change due to bind-time
                                                 // compression state.
            uint32 vrsImageIncompatible    :  1; // Set if the view cannot be used with a VRS image (forced passthrough)
            uint32 reserved                : 20;
        };

        uint32 u32All;
    } m_flags;

    const Image* m_pImage;
    Extent2d     m_extent;

    SubresId  m_depthSubresource;   // Sub-resource associated with the Depth plane
    SubresId  m_stencilSubresource; // Sub-resource associated with the Stencil plane

    DepthStencilLayoutToState  m_depthLayoutToState;
    DepthStencilLayoutToState  m_stencilLayoutToState;

    uint32 m_uniqueId;

private:
    static constexpr uint32 DbDepthViewSliceStartMaskNumBits = 11;
    static constexpr uint32 DbDepthViewSliceMaxMaskNumBits   = 11;

    uint32 CalcDecompressOnZPlanesValue(const Device& device, ZFormat hwZFmt) const;

    HtileUsageFlags      m_hTileUsage;
    uint32               m_baseArraySlice;
    uint32               m_arraySize;
    DepthStencilViewRegs m_regs;

    PAL_DISALLOW_DEFAULT_CTOR(DepthStencilView);
};

} // Gfx9
} // Pal
