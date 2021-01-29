/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6Image.h"
#include "core/hw/gfxip/universalCmdBuffer.h"

namespace Pal
{

class CmdStream;
class Device;
class Image;

namespace Gfx6
{

// Set of context registers associated with a depth/stencil view object.
struct DepthStencilViewRegs
{
    regDB_DEPTH_INFO                 dbDepthInfo;
    regDB_Z_INFO                     dbZInfo;
    regDB_STENCIL_INFO               dbStencilInfo;
    regDB_Z_READ_BASE                dbZReadBase;
    regDB_STENCIL_READ_BASE          dbStencilReadBase;
    regDB_Z_WRITE_BASE               dbZWriteBase;
    regDB_STENCIL_WRITE_BASE         dbStencilWriteBase;
    regDB_DEPTH_SIZE                 dbDepthSize;
    regDB_DEPTH_SLICE                dbDepthSlice;
    regDB_DEPTH_VIEW                 dbDepthView;
    regDB_RENDER_OVERRIDE2           dbRenderOverride2;
    regDB_HTILE_DATA_BASE            dbHtileDataBase;
    regDB_HTILE_SURFACE              dbHtileSurface;
    regDB_PRELOAD_CONTROL            dbPreloadControl;
    regDB_RENDER_CONTROL             dbRenderControl;
    regPA_SU_POLY_OFFSET_DB_FMT_CNTL paSuPolyOffsetDbFmtCntl;
    regCOHER_DEST_BASE_0             coherDestBase0;
    regDB_RENDER_OVERRIDE            dbRenderOverride;

    gpusize  fastClearMetadataGpuVa;
    gpusize  hiSPretestMetadataGpuVa;
};

// =====================================================================================================================
// Gfx6 HW-specific implementation of the Pal::IDepthStencilView interface
class DepthStencilView final : public IDepthStencilView
{
public:
    static const uint32 DbRenderOverrideRmwMask = (DB_RENDER_OVERRIDE__FORCE_HIZ_ENABLE_MASK        |
                                                   DB_RENDER_OVERRIDE__FORCE_HIS_ENABLE0_MASK       |
                                                   DB_RENDER_OVERRIDE__FORCE_HIS_ENABLE1_MASK       |
                                                   DB_RENDER_OVERRIDE__FORCE_STENCIL_VALID_MASK     |
                                                   DB_RENDER_OVERRIDE__FORCE_Z_VALID_MASK           |
                                                   DB_RENDER_OVERRIDE__DISABLE_TILE_RATE_TILES_MASK |
                                                   DB_RENDER_OVERRIDE__NOOP_CULL_DISABLE_MASK);

    DepthStencilView(
        const Device*                             pDevice,
        const DepthStencilViewCreateInfo&         createInfo,
        const DepthStencilViewInternalCreateInfo& internalInfo);

    uint32* WriteCommands(
        ImageLayout depthLayout,
        ImageLayout stencilLayout,
        CmdStream*  pCmdStream,
        uint32*     pCmdSpace) const;

    static uint32* WriteUpdateFastClearDepthStencilValue(
        uint32     metaDataClearFlags,
        float      depth,
        uint8      stencil,
        CmdStream* pCmdStream,
        uint32*    pCmdSpace);

    static uint32* WriteTcCompatFlush(
        const Device&           device,
        const DepthStencilView* pNewView,
        const DepthStencilView* pOldView,
        uint32*                 pCmdSpace);

    uint32* UpdateZRangePrecision(
        bool       requiresCondExec,
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const;

    const Image* GetImage() const { return m_pImage; }
    uint32 MipLevel() const { return m_depthSubresource.mipLevel; }

    bool IsVaLocked() const { return m_flags.viewVaLocked; }
    bool ReadOnlyDepth() const { return m_flags.readOnlyDepth; }
    bool ReadOnlyStencil() const { return m_flags.readOnlyStencil; }

    TargetExtent2d GetExtent() const { return m_extent; }

private:
    virtual ~DepthStencilView()
    {
        // This destructor, and the destructors of all member and base classes, must always be empty: PAL depth stencil
        // views guarantee to the client that they do not have to be explicitly destroyed.
        PAL_NEVER_CALLED();
    }

    void InitRegisters(const DepthStencilViewCreateInfo&         createInfo,
                       const DepthStencilViewInternalCreateInfo& internalInfo);
    void UpdateImageVa(DepthStencilViewRegs* pRegs) const;

    uint32 CalcDecompressOnZPlanesValue(bool depthCompressDisable) const;

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
            uint32 waDbTcCompatFlush       :  2;
            uint32 depthMetadataTexFetch   :  1;
            uint32 stencilMetadataTexFetch :  1;
            uint32 usesLoadRegIndexPkt     :  1; // Set if LOAD_CONTEXT_REG_INDEX is used instead of LOAD_CONTEXT_REG.
            uint32 viewVaLocked            :  1; // Whether the view's VA range is locked and won't change.
            uint32 isExpand                :  1; // Set if this view is for an expand blit.
            uint32 hiSPretests             :  1; // Set if the image has HiS pretest metadata
            uint32 dbRenderOverrideLocked  :  1; // Set if DB_RENDER_OVERRIDE cannot change due to bind-time
                                                 // compression state.
            uint32 dbRenderControlLocked   :  1; // Set if DB_RENDER_CONTROL cannot change due to bind-time
                                                 // compression state.
            uint32 reserved                : 17;
        };

        uint32 u32All;
    } m_flags;

    const Device&      m_device;
    const Image*const  m_pImage;
    TargetExtent2d     m_extent;

    SubresId  m_depthSubresource;   // Sub-resource associated with the Depth plane
    SubresId  m_stencilSubresource; // Sub-resource associated with the Stencil plane

    DepthStencilLayoutToState  m_depthLayoutToState;
    DepthStencilLayoutToState  m_stencilLayoutToState;
    DepthStencilViewRegs       m_regs;

    PAL_DISALLOW_DEFAULT_CTOR(DepthStencilView);
    PAL_DISALLOW_COPY_AND_ASSIGN(DepthStencilView);
};

} // Gfx6
} // Pal
