/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

    virtual uint32* WriteCommands(
        ImageLayout depthLayout,
        ImageLayout stencilLayout,
        CmdStream*  pCmdStream,
        uint32*     pCmdSpace) const = 0;

    virtual uint32* UpdateZRangePrecision(
        bool       requiresCondExec,
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const
    {
        PAL_NEVER_CALLED();
        return pCmdSpace;
    }

    const Image* GetImage() const { return m_pImage; }
    uint32 MipLevel() const { return m_depthSubresource.mipLevel; }

    bool IsVaLocked() const { return m_createInfo.flags.imageVaLocked; }
    bool WaitOnMetadataMipTail() const { return m_flags.waitOnMetadataMipTail; }

    static uint32* WriteUpdateFastClearDepthStencilValue(
        uint32     metaDataClearFlags,
        float      depth,
        uint8      stencil,
        CmdStream* pCmdStream,
        uint32*    pCmdSpace);

    static uint32* HandleBoundTargetChanged(
        const Device& device,
        CmdStream*    pCmdStream,
        uint32*       pCmdSpace);

    const DepthStencilViewCreateInfo& GetDsViewCreateInfo() const { return m_createInfo; }

protected:
    virtual ~DepthStencilView()
    {
        // This destructor, and the destructors of all member and base classes, must always be empty: PAL depth stencil
        // views guarantee to the client that they do not have to be explicitly destroyed.
        PAL_NEVER_CALLED();
    }

    template <typename Pm4ImgType>
    void CommonBuildPm4Headers(
        DepthStencilCompressionState depthState,
        DepthStencilCompressionState stencilState,
        Pm4ImgType*                  pPm4Img) const;

    template <typename Pm4ImgType, typename FmtInfoType>
    void InitCommonImageView(
        const FmtInfoType*            pFmtInfo,
        DepthStencilCompressionState  depthState,
        DepthStencilCompressionState  stencilState,
        Pm4ImgType*                   pPm4Img,
        regDB_RENDER_OVERRIDE*        pDbRenderOverride) const;

    template <typename Pm4ImgType>
    void UpdateImageVa(Pm4ImgType* pPm4Img) const;

    template <typename Pm4ImgType>
    uint32* WriteCommandsInternal(
        CmdStream*         pCmdStream,
        uint32*            pCmdSpace,
        const Pm4ImgType&  pm4Img) const;

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
            uint32 depthMetadataTexFetch   :  1;
            uint32 stencilMetadataTexFetch :  1;
            uint32 usesLoadRegIndexPkt     :  1; // Set if LOAD_CONTEXT_REG_INDEX is used instead of LOAD_CONTEXT_REG.
            uint32 waitOnMetadataMipTail   :  1; // Set if the CmdBindTargets should insert a stall when binding this
                                                 // view object.
            uint32 reserved                : 23;
        };

        uint32 u32All;
    } m_flags;

    const Device&                            m_device;
    const Image*                             m_pImage;
    const DepthStencilViewCreateInfo         m_createInfo;
    const DepthStencilViewInternalCreateInfo m_internalInfo;

private:
    PAL_DISALLOW_DEFAULT_CTOR(DepthStencilView);
    PAL_DISALLOW_COPY_AND_ASSIGN(DepthStencilView);

    uint32 CalcDecompressOnZPlanesValue(ZFormat  hwZFmt) const;
};

// Represents an "image" of the PM4 commands necessary to write a DepthStencilView to GFX9 hardware. The required
// register writes are grouped into sets based on sequential register addresses, to minimize the amount of PM4 space
// needed by setting several registers at once.
struct Gfx9DepthStencilViewPm4Img
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 301
    PM4PFP_SET_CONTEXT_REG           hdrDbZInfoToDfsmControl;
    regDB_Z_INFO__GFX09              dbZInfo;
    regDB_STENCIL_INFO__GFX09        dbStencilInfo;
    regDB_Z_READ_BASE                dbZReadBase;
    regDB_Z_READ_BASE_HI             dbZReadBaseHi;
    regDB_STENCIL_READ_BASE          dbStencilReadBase;
    regDB_STENCIL_READ_BASE_HI       dbStencilReadBaseHi;
    regDB_Z_WRITE_BASE               dbZWriteBase;
    regDB_Z_WRITE_BASE_HI            dbZWriteBaseHi;
    regDB_STENCIL_WRITE_BASE         dbStencilWriteBase;
    regDB_STENCIL_WRITE_BASE_HI      dbStencilWriteBaseHi;
    regDB_DFSM_CONTROL               dbDfsmControl;

    PM4PFP_SET_CONTEXT_REG           hdrDbZInfo2ToStencilInfo2;
    regDB_Z_INFO2__GFX09             dbZInfo2;
    regDB_STENCIL_INFO2__GFX09       dbStencilInfo2;
#else
    PM4PFP_SET_CONTEXT_REG           hdrDbZInfoToStencilInfo2;
    regDB_Z_INFO__GFX09              dbZInfo;
    regDB_STENCIL_INFO__GFX09        dbStencilInfo;
    regDB_Z_READ_BASE                dbZReadBase;
    regDB_Z_READ_BASE_HI             dbZReadBaseHi;
    regDB_STENCIL_READ_BASE          dbStencilReadBase;
    regDB_STENCIL_READ_BASE_HI       dbStencilReadBaseHi;
    regDB_Z_WRITE_BASE               dbZWriteBase;
    regDB_Z_WRITE_BASE_HI            dbZWriteBaseHi;
    regDB_STENCIL_WRITE_BASE         dbStencilWriteBase;
    regDB_STENCIL_WRITE_BASE_HI      dbStencilWriteBaseHi;
    regDB_DFSM_CONTROL               dbDfsmControl;
    regDB_Z_INFO2__GFX09             dbZInfo2;
    regDB_STENCIL_INFO2__GFX09       dbStencilInfo2;
#endif

    PM4PFP_SET_CONTEXT_REG           hdrDbDepthView;
    regDB_DEPTH_VIEW                 dbDepthView;

    PM4PFP_SET_CONTEXT_REG           hdrDbRenderOverride2;
    regDB_RENDER_OVERRIDE2           dbRenderOverride2;
    regDB_HTILE_DATA_BASE            dbHtileDataBase;
    regDB_HTILE_DATA_BASE_HI         dbHtileDataBaseHi;
    regDB_DEPTH_SIZE__GFX09          dbDepthSize;

    PM4PFP_SET_CONTEXT_REG           hdrDbHtileSurface;
    regDB_HTILE_SURFACE              dbHtileSurface;

    PM4PFP_SET_CONTEXT_REG           hdrDbPreloadControl;
    regDB_PRELOAD_CONTROL            dbPreloadControl;

    PM4PFP_SET_CONTEXT_REG           hdrDbRenderControl;
    regDB_RENDER_CONTROL             dbRenderControl;

    PM4PFP_SET_CONTEXT_REG           hdrPaSuPolyOffsetDbFmtCntl;
    regPA_SU_POLY_OFFSET_DB_FMT_CNTL paSuPolyOffsetDbFmtCntl;

    PM4PFP_SET_CONTEXT_REG           hdrPaScScreenScissor;
    regPA_SC_SCREEN_SCISSOR_TL       paScScreenScissorTl;
    regPA_SC_SCREEN_SCISSOR_BR       paScScreenScissorBr;

    PM4PFP_SET_CONTEXT_REG           hdrCoherDestBase;
    regCOHER_DEST_BASE_0             coherDestBase0;

    PM4ME_CONTEXT_REG_RMW            dbRenderOverrideRmw;

    // PM4 load context regs packet to load the Image's fast-clear meta-data.
    union
    {
        PM4PFP_LOAD_CONFIG_REG           loadMetaData;
        PM4PFP_LOAD_CONTEXT_REG_INDEX    loadMetaDataIndex;
    };

    // Command space needed, in DWORDs. This field must always be last in the structure to not interfere w/ the actual
    // commands contained within.
    size_t                           spaceNeeded;
};

// =====================================================================================================================
// Gfx9 HW-specific implementation of the Pal::IDepthStencilView interface
class Gfx9DepthStencilView : public DepthStencilView
{
public:
    Gfx9DepthStencilView(
        const Device*                             pDevice,
        const DepthStencilViewCreateInfo&         createInfo,
        const DepthStencilViewInternalCreateInfo& internalInfo);

    uint32* WriteCommands(
        ImageLayout depthLayout,
        ImageLayout stencilLayout,
        CmdStream*  pCmdStream,
        uint32*     pCmdSpace) const override;

    virtual uint32* UpdateZRangePrecision(
        bool       requiresCondExec,
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const override;

protected:
    virtual ~Gfx9DepthStencilView()
    {
        // This destructor, and the destructors of all member and base classes, must always be empty: PAL depth stencil
        // views guarantee to the client that they do not have to be explicitly destroyed.
        PAL_NEVER_CALLED();
    }

private:
    void BuildPm4Headers(DepthStencilCompressionState depthState, DepthStencilCompressionState stencilState);
    void InitRegisters(DepthStencilCompressionState depthState, DepthStencilCompressionState stencilState);

    // PM4 command images used to write this view to hardware, depending on the depth/stencil compression states.
    Gfx9DepthStencilViewPm4Img m_pm4Images[DepthStencilCompressionStateCount][DepthStencilCompressionStateCount];

    PAL_DISALLOW_DEFAULT_CTOR(Gfx9DepthStencilView);
    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9DepthStencilView);
};

} // Gfx9
} // Pal
