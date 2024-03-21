/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9MaskRam.h"
#include "core/hw/gfxip/pm4UniversalCmdBuffer.h"

namespace Pal
{

class CmdStream;
class Device;
class Image;

namespace Gfx9
{

// =====================================================================================================================
// Gfx9 HW-specific implementation of the Pal::IDepthStencilView interface
class DepthStencilView : public IDepthStencilView
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

    virtual uint32* WriteCommands(
        ImageLayout            depthLayout,
        ImageLayout            stencilLayout,
        CmdStream*             pCmdStream,
        bool                   isNested,
        regDB_RENDER_OVERRIDE* pDbRenderOverride,
        uint32*                pCmdSpace) const = 0;

    virtual uint32* UpdateZRangePrecision(
        bool       requiresCondExec,
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const
    {
        return pCmdSpace;
    }

    const Image* GetImage() const { return m_pImage; }
    uint32 MipLevel() const { return m_depthSubresource.mipLevel; }

    bool IsVaLocked() const { return m_flags.viewVaLocked; }
    bool WaitOnMetadataMipTail() const { return m_flags.waitOnMetadataMipTail; }
    bool ReadOnlyDepth() const { return m_flags.readOnlyDepth; }
    bool ReadOnlyStencil() const { return m_flags.readOnlyStencil; }

    static uint32* WriteUpdateFastClearDepthStencilValue(
        uint32     metaDataClearFlags,
        float      depth,
        uint8      stencil,
        CmdStream* pCmdStream,
        uint32*    pCmdSpace);

    uint32* HandleBoundTargetChanged(const Pm4::UniversalCmdBuffer*  pCmdBuffer, uint32* pCmdSpace) const;

    Extent2d GetExtent() const { return m_extent; }

    bool Equals(const DepthStencilView* pOther) const;

    static const uint32 DbRenderOverrideRmwMask = DB_RENDER_OVERRIDE__FORCE_HIZ_ENABLE_MASK        |
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

    template <typename RegistersType, typename FmtInfoType>
    void InitRegistersCommon(
        const Device&                             device,
        const DepthStencilViewCreateInfo&         createInfo,
        const DepthStencilViewInternalCreateInfo& internalInfo,
        const FmtInfoType*                        pFmtInfo,
        RegistersType*                            pRegs);

    template <typename RegistersType>
    void UpdateImageVa(RegistersType* pRegs) const;

    template <typename RegistersType>
    uint32* WriteCommandsCommon(
        ImageLayout    depthLayout,
        ImageLayout    stencilLayout,
        CmdStream*     pCmdStream,
        uint32*        pCmdSpace,
        RegistersType* pRegs) const;

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
            uint32 waitOnMetadataMipTail   :  1; // Set if the CmdBindTargets should insert a stall when binding this
                                                 // view object.
            uint32 viewVaLocked            :  1; // Whether the view's VA range is locked and won't change.
            uint32 hiSPretests             :  1; // Set if the image has HiS pretest metadata
            uint32 dbRenderOverrideLocked  :  1; // Set if DB_RENDER_OVERRIDE cannot change due to bind-time
                                                 // compression state.
            uint32 dbRenderControlLocked   :  1; // Set if DB_RENDER_CONTROL cannot change due to bind-time
                                                 // compression state.
            uint32 waTcCompatZRange        :  1; // Set if the TC-compatible ZRange precision workaround is active
            uint32 reserved                : 18;
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
    uint32 CalcDecompressOnZPlanesValue(const Device& device, ZFormat hwZFmt) const;

    HtileUsageFlags  m_hTileUsage;

    PAL_DISALLOW_DEFAULT_CTOR(DepthStencilView);
};

// Set of context registers associated with a depth/stencil view object (Gfx10 version).
struct Gfx10DepthStencilViewRegs
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
// Gfx10 HW-specific implementation of the Pal::IDepthStencilView interface
class Gfx10DepthStencilView final : public DepthStencilView
{
public:
    Gfx10DepthStencilView(
        const Device*                             pDevice,
        const DepthStencilViewCreateInfo&         createInfo,
        const DepthStencilViewInternalCreateInfo& internalInfo,
        uint32                                    uniqueId);

    Gfx10DepthStencilView(const Gfx10DepthStencilView&) = default;
    Gfx10DepthStencilView& operator=(const Gfx10DepthStencilView&) = default;
    Gfx10DepthStencilView(Gfx10DepthStencilView&&) = default;
    Gfx10DepthStencilView& operator=(Gfx10DepthStencilView&&) = default;

    static void SetGfx11StaticDbRenderControlFields(
        const Device&         device,
        const uint8           numFragments,
        regDB_RENDER_CONTROL* pDbRenderControl);

    uint32* WriteCommands(
        ImageLayout            depthLayout,
        ImageLayout            stencilLayout,
        CmdStream*             pCmdStream,
        bool                   isNested,
        regDB_RENDER_OVERRIDE* pDbRenderOverride,
        uint32*                pCmdSpace) const override;

    uint32 BaseArraySlice() const { return m_baseArraySlice; }
    uint32 ArraySize() const { return m_arraySize; }

protected:
    virtual ~Gfx10DepthStencilView()
    {
        // This destructor, and the destructors of all member and base classes, must always be empty: PAL depth stencil
        // views guarantee to the client that they do not have to be explicitly destroyed.
        PAL_NEVER_CALLED();
    }

private:
    void InitRegisters(
        const Device&                             device,
        const DepthStencilViewCreateInfo&         createInfo,
        const DepthStencilViewInternalCreateInfo& internalInfo);

    Gfx10DepthStencilViewRegs  m_regs;

    static constexpr uint32 DbDepthViewSliceStartMaskNumBits = 11;
    static constexpr uint32 DbDepthViewSliceMaxMaskNumBits   = 11;

    uint32 m_baseArraySlice;
    uint32 m_arraySize;

    PAL_DISALLOW_DEFAULT_CTOR(Gfx10DepthStencilView);
};

} // Gfx9
} // Pal
