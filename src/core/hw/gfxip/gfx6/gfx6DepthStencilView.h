/*
 *******************************************************************************
 *
 * Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include "palDepthStencilView.h"
#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6Image.h"

namespace Pal
{

class CmdStream;
class Device;
class Image;

namespace Gfx6
{

// Represents an "image" of the PM4 commands necessary to write a GcnDepthStencilView to hardware. The required
// register writes are grouped into sets based on sequential register addresses, to minimize the amount of PM4 space
// needed by setting several registers at once.
struct DepthStencilViewPm4Img
{
    PM4CMDSETDATA                    hdrDbDepthInfo;
    regDB_DEPTH_INFO                 dbDepthInfo;
    regDB_Z_INFO                     dbZInfo;
    regDB_STENCIL_INFO               dbStencilInfo;
    regDB_Z_READ_BASE                dbZReadBase;
    regDB_STENCIL_READ_BASE          dbStencilReadBase;
    regDB_Z_WRITE_BASE               dbZWriteBase;
    regDB_STENCIL_WRITE_BASE         dbStencilWriteBase;
    regDB_DEPTH_SIZE                 dbDepthSize;
    regDB_DEPTH_SLICE                dbDepthSlice;

    PM4CMDSETDATA                    hdrDbDepthView;
    regDB_DEPTH_VIEW                 dbDepthView;

    PM4CMDSETDATA                    hdrDbRenderOverride2;
    regDB_RENDER_OVERRIDE2           dbRenderOverride2;
    regDB_HTILE_DATA_BASE            dbHtileDataBase;

    PM4CMDSETDATA                    hdrDbHtileSurface;
    regDB_HTILE_SURFACE              dbHtileSurface;

    PM4CMDSETDATA                    hdrDbPreloadControl;
    regDB_PRELOAD_CONTROL            dbPreloadControl;

    PM4CMDSETDATA                    hdrDbRenderControl;
    regDB_RENDER_CONTROL             dbRenderControl;

    PM4CMDSETDATA                    hdrPaSuPolyOffsetDbFmtCntl;
    regPA_SU_POLY_OFFSET_DB_FMT_CNTL paSuPolyOffsetDbFmtCntl;

    PM4CMDSETDATA                    hdrPaScScreenScissorTlBr;
    regPA_SC_SCREEN_SCISSOR_TL       paScScreenScissorTl;
    regPA_SC_SCREEN_SCISSOR_BR       paScScreenScissorBr;

    PM4CMDSETDATA                    hdrCoherDestBase0;
    regCOHER_DEST_BASE_0             coherDestBase0;

    PM4CMDREGRMW                     dbRenderOverrideRmw;

    // PM4 load context regs packet to load the Image's fast-clear meta-data.
    union
    {
        PM4CMDLOADDATA               loadMetaData;
        PM4CMDLOADDATAINDEX          loadMetaDataIndex;
    };

    // Command space needed, in DWORDs. This field must always be last in the structure to not interfere w/ the actual
    // commands contained within.
    size_t                           spaceNeeded;
};

// =====================================================================================================================
// Gfx6 HW-specific implementation of the Pal::IDepthStencilView interface
class DepthStencilView : public IDepthStencilView
{
public:
    static const uint32 DbRenderOverrideRmwMask = DB_RENDER_OVERRIDE__FORCE_HIZ_ENABLE_MASK |
                                                  DB_RENDER_OVERRIDE__FORCE_HIS_ENABLE0_MASK |
                                                  DB_RENDER_OVERRIDE__FORCE_HIS_ENABLE1_MASK |
                                                  DB_RENDER_OVERRIDE__FORCE_STENCIL_VALID_MASK |
                                                  DB_RENDER_OVERRIDE__FORCE_Z_VALID_MASK |
                                                  DB_RENDER_OVERRIDE__DISABLE_TILE_RATE_TILES_MASK |
                                                  DB_RENDER_OVERRIDE__NOOP_CULL_DISABLE_MASK;

    DepthStencilView(
        const Device*                             pDevice,
        const DepthStencilViewCreateInfo&         createInfo,
        const DepthStencilViewInternalCreateInfo& internalInfo);

    static size_t Pm4ImageSize() { return sizeof(DepthStencilViewPm4Img); }
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

    bool IsVaLocked() const { return m_createInfo.flags.imageVaLocked; }

    const DepthStencilViewCreateInfo& GetDsViewCreateInfo() const { return m_createInfo; }

protected:
    virtual ~DepthStencilView()
    {
        // This destructor, and the destructors of all member and base classes, must always be empty: PAL color target
        // views guarantee to the client that they do not have to be explicitly destroyed.
        PAL_NEVER_CALLED();
    }

private:
    void BuildPm4Headers(DepthStencilCompressionState depthState, DepthStencilCompressionState stencilState);
    void InitRegisters(DepthStencilCompressionState depthState, DepthStencilCompressionState stencilState);
    uint32 CalcDecompressOnZPlanesValue(bool depthCompressDisable) const;

    void UpdateImageVa(DepthStencilViewPm4Img* pPm4Img) const;

    // PM4 command images used to write this view to hardware, depending on the depth/stencil compression states.
    DepthStencilViewPm4Img m_pm4Images[DepthStencilCompressionStateCount][DepthStencilCompressionStateCount];

    SubresId     m_depthSubresource;               // Sub-resource associated with the Depth plane
    SubresId     m_stencilSubresource;             // Sub-resource associated with the Stencil plane

    // Bitfield describing the metadata and settings that are supported by this view.
    union
    {
        struct
        {
            uint32 hTile                   :  1;
            uint32 depth                   :  1;
            uint32 stencil                 :  1;
            uint32 waDbTcCompatFlush       :  2;
            uint32 depthMetadataTexFetch   :  1;
            uint32 stencilMetadataTexFetch :  1;
            uint32 usesLoadRegIndexPkt     :  1; // Set if LOAD_CONTEXT_REG_INDEX is used instead of LOAD_CONTEXT_REG.
            uint32 reserved                : 24;
        };

        uint32 u32All;
    } m_flags;

    const Device&                            m_device;
    const Image*                             m_pImage;
    const DepthStencilViewCreateInfo         m_createInfo;
    const DepthStencilViewInternalCreateInfo m_internalInfo;

    PAL_DISALLOW_DEFAULT_CTOR(DepthStencilView);
    PAL_DISALLOW_COPY_AND_ASSIGN(DepthStencilView);
};

} // Gfx6
} // Pal
