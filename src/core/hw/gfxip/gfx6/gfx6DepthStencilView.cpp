/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/image.h"
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6DepthStencilView.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6FormatInfo.h"
#include "core/hw/gfxip/gfx6/gfx6Image.h"
#include "core/addrMgr/addrMgr1/addrMgr1.h"

using namespace Pal::Formats::Gfx6;
using namespace Util;

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
DepthStencilView::DepthStencilView(
    const Device*                             pDevice,
    const DepthStencilViewCreateInfo&         createInfo,
    const DepthStencilViewInternalCreateInfo& internalInfo)
    :
    m_device(*pDevice),
    m_pImage(GetGfx6Image(createInfo.pImage)),
    m_createInfo(createInfo),
    m_internalInfo(internalInfo)
{
    PAL_ASSERT(createInfo.pImage != nullptr);
    const auto& imageInfo = m_pImage->Parent()->GetImageCreateInfo();
    const auto& parent    = *m_device.Parent();
    const auto& settings  = m_device.Settings();

    m_flags.u32All              = 0;
    m_flags.hTile               = m_pImage->HasHtileData();
    m_flags.depth               = parent.SupportsDepth(imageInfo.swizzledFormat.format, imageInfo.tiling);
    m_flags.stencil             = parent.SupportsStencil(imageInfo.swizzledFormat.format, imageInfo.tiling);
    m_flags.waDbTcCompatFlush   = m_device.WaDbTcCompatFlush();
    m_flags.usesLoadRegIndexPkt = m_device.Parent()->ChipProperties().gfx6.supportLoadRegIndexPkt;

    if (m_flags.depth && m_flags.stencil)
    {
        // Depth & Stencil format.
        m_depthSubresource.aspect       = ImageAspect::Depth;
        m_depthSubresource.mipLevel     = m_createInfo.mipLevel;
        m_depthSubresource.arraySlice   = 0;
        m_stencilSubresource.aspect     = ImageAspect::Stencil;
        m_stencilSubresource.mipLevel   = m_createInfo.mipLevel;
        m_stencilSubresource.arraySlice = 0;
    }
    else if (m_flags.depth)
    {
        // Depth-only format.
        m_depthSubresource.aspect     = ImageAspect::Depth;
        m_depthSubresource.mipLevel   = m_createInfo.mipLevel;
        m_depthSubresource.arraySlice = 0;
        m_stencilSubresource          = m_depthSubresource;
    }
    else
    {
        // Stencil-only format.
        m_stencilSubresource.aspect     = ImageAspect::Stencil;
        m_stencilSubresource.mipLevel   = m_createInfo.mipLevel;
        m_stencilSubresource.arraySlice = 0;
        m_depthSubresource              = m_stencilSubresource;
    }

    const SubResourceInfo*const pDepthSubResInfo   = m_pImage->Parent()->SubresourceInfo(m_depthSubresource);
    const SubResourceInfo*const pStencilSubResInfo = m_pImage->Parent()->SubresourceInfo(m_stencilSubresource);
    m_flags.depthMetadataTexFetch   = pDepthSubResInfo->flags.supportMetaDataTexFetch;
    m_flags.stencilMetadataTexFetch = pStencilSubResInfo->flags.supportMetaDataTexFetch;

    // Initialize the register states for the various depth/stencil compression states.
    for (uint32 depthState = 0; depthState < DepthStencilCompressionStateCount; ++depthState)
    {
        for (uint32 stencilState = 0; stencilState < DepthStencilCompressionStateCount; ++stencilState)
        {
            BuildPm4Headers(
                static_cast<DepthStencilCompressionState>(depthState),
                static_cast<DepthStencilCompressionState>(stencilState));

            InitRegisters(
                static_cast<DepthStencilCompressionState>(depthState),
                static_cast<DepthStencilCompressionState>(stencilState));

            if (m_createInfo.flags.imageVaLocked)
            {
                UpdateImageVa(&m_pm4Images[depthState][stencilState]);
            }
        }
    }
}

// =====================================================================================================================
// Builds the PM4 packet headers for an image of PM4 commands used to write this View object to hardware.
void DepthStencilView::BuildPm4Headers(
    DepthStencilCompressionState depthState,   // Depth state - indexes into m_pm4Images to be initialized.
    DepthStencilCompressionState stencilState) // Stencil state - indexes into m_pm4Images to be initialized.
{
    const CmdUtil& cmdUtil          = m_device.CmdUtil();
    DepthStencilViewPm4Img* pPm4Img = &m_pm4Images[depthState][stencilState];

    memset(pPm4Img, 0, sizeof(DepthStencilViewPm4Img));

    // 1st PM4 set data packet: sets the context registers DB_DEPTH_INFO through DB_DEPTH_SLICE.
    pPm4Img->spaceNeeded = cmdUtil.BuildSetSeqContextRegs(mmDB_DEPTH_INFO,
                                                          mmDB_DEPTH_SLICE,
                                                          &pPm4Img->hdrDbDepthInfo);

    // 2nd PM4 set data packet: sets the context register DB_DEPTH_VIEW.
    pPm4Img->spaceNeeded += cmdUtil.BuildSetOneContextReg(mmDB_DEPTH_VIEW, &pPm4Img->hdrDbDepthView);

    // 3rd PM4 set data packet: sets the context registers DB_RENDER_OVERRIDE2 and
    //  DB_HTILE_DATA_BASE.
    pPm4Img->spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmDB_RENDER_OVERRIDE2,
                                                           mmDB_HTILE_DATA_BASE,
                                                           &pPm4Img->hdrDbRenderOverride2);

    // 4th PM4 set data packet: sets the context register DB_HTILE_SURFACE.
    pPm4Img->spaceNeeded += cmdUtil.BuildSetOneContextReg(mmDB_HTILE_SURFACE, &pPm4Img->hdrDbHtileSurface);

    // 5th PM4 set data packet: sets the context register DB_PRELOAD_CONTROL.
    pPm4Img->spaceNeeded += cmdUtil.BuildSetOneContextReg(mmDB_PRELOAD_CONTROL, &pPm4Img->hdrDbPreloadControl);

    // 6th PM4 set data packet: sets the context register DB_RENDER_CONTROL.
    pPm4Img->spaceNeeded += cmdUtil.BuildSetOneContextReg(mmDB_RENDER_CONTROL, &pPm4Img->hdrDbRenderControl);

    // 7th PM4 set data packet: sets the context register PA_SU_POLY_OFFSET_DB_FMT_CNTL.
    pPm4Img->spaceNeeded += cmdUtil.BuildSetOneContextReg(mmPA_SU_POLY_OFFSET_DB_FMT_CNTL,
                                                          &pPm4Img->hdrPaSuPolyOffsetDbFmtCntl);

    // 8th PM4 set data packet: sets the context registers PA_SC_SCREEN_SCISSOR_TL and
    //  PA_SC_SCREEN_SCISSOR_BR.
    pPm4Img->spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmPA_SC_SCREEN_SCISSOR_TL,
                                                           mmPA_SC_SCREEN_SCISSOR_BR,
                                                           &pPm4Img->hdrPaScScreenScissorTlBr);

    // 9th PM4 set data packet: sets the first two generic COHER_DEST_BASE context registers.
    pPm4Img->spaceNeeded += cmdUtil.BuildSetOneContextReg(mmCOHER_DEST_BASE_0, &pPm4Img->hdrCoherDestBase0);

    // 10th PM4 set data packet: RMW set of portions of DB_RENDER_OVERRIDE defined by a depth stencil view (other parts
    // written by graphics pipelines)
    pPm4Img->spaceNeeded += cmdUtil.GetContextRegRmwSize(); // Header and value defined by InitRegisters()

    if (m_flags.hTile &&
        ((depthState == DepthStencilCompressed) || (stencilState == DepthStencilCompressed)))
    {
        // If the parent image has HTile and some aspect is in the compressed state, we need to add a LOAD_CONTEXT_REG
        // packet to load the image's fast-clear metadata.
        // NOTE: We do not know the GPU virtual address of the metadata until bind-time.
        constexpr uint32 StartRegAddr = mmDB_STENCIL_CLEAR;
        constexpr uint32 RegCount     = (mmDB_DEPTH_CLEAR - mmDB_STENCIL_CLEAR + 1);

        if (m_flags.usesLoadRegIndexPkt != 0)
        {
            pPm4Img->spaceNeeded += cmdUtil.BuildLoadContextRegsIndex<true>(0,
                                                                            StartRegAddr,
                                                                            RegCount,
                                                                            &pPm4Img->loadMetaDataIndex);
        }
        else
        {
            pPm4Img->spaceNeeded += cmdUtil.BuildLoadContextRegs(0,
                                                                 StartRegAddr,
                                                                 RegCount,
                                                                 &pPm4Img->loadMetaData);
        }
    }
}

// =====================================================================================================================
// Finalizes the PM4 packet image by setting up the register values used to write this View object to hardware.
void DepthStencilView::InitRegisters(
    DepthStencilCompressionState depthState,
    DepthStencilCompressionState stencilState)
{
    const CmdUtil& cmdUtil = m_device.CmdUtil();
    const ImageInfo& imageInfo = m_pImage->Parent()->GetImageInfo();
    const ImageCreateInfo& createInfo = m_pImage->Parent()->GetImageCreateInfo();

    const Gfx6PalSettings& settings = m_device.Settings();

    const SubResourceInfo*const pDepthSubResInfo   = m_pImage->Parent()->SubresourceInfo(m_depthSubresource);
    const SubResourceInfo*const pStencilSubResInfo = m_pImage->Parent()->SubresourceInfo(m_stencilSubresource);

    const ChNumFormat zFmt = pDepthSubResInfo->format.format;
    const ChNumFormat sFmt = pStencilSubResInfo->format.format;

    const AddrMgr1::TileInfo*const pTileInfo   = AddrMgr1::GetTileInfo(m_pImage->Parent(), m_depthSubresource);
    const AddrMgr1::TileInfo*const pStTileInfo = AddrMgr1::GetTileInfo(m_pImage->Parent(), m_stencilSubresource);

    DepthStencilViewPm4Img* pPm4Img = &m_pm4Images[depthState][stencilState];

    if (m_device.Parent()->ChipProperties().gfxLevel == GfxIpLevel::GfxIp6)
    {
        pPm4Img->dbZInfo.bits.TILE_MODE_INDEX       = pTileInfo->tileIndex;
        pPm4Img->dbStencilInfo.bits.TILE_MODE_INDEX = pStTileInfo->tileIndex;
    }
    else
    {
        // For non-Gfx6 asics:
        // TILE_MODE_INDEX fields have been removed from DB_Z_INFO and DB_STENCIL_INFO and instead the per surface
        // tiling parameters will need to be programmed directly in DB_Z_INFO, DB_DEPTH_INFO, and DB_STENCIL_INFO
        // registers.
        pPm4Img->dbDepthInfo.bits.PIPE_CONFIG__CI__VI       = pTileInfo->pipeConfig;
        pPm4Img->dbDepthInfo.bits.NUM_BANKS__CI__VI         = pTileInfo->banks;
        pPm4Img->dbDepthInfo.bits.BANK_WIDTH__CI__VI        = pTileInfo->bankWidth;
        pPm4Img->dbDepthInfo.bits.BANK_HEIGHT__CI__VI       = pTileInfo->bankHeight;
        pPm4Img->dbDepthInfo.bits.MACRO_TILE_ASPECT__CI__VI = pTileInfo->macroAspectRatio;
        pPm4Img->dbDepthInfo.bits.ARRAY_MODE__CI__VI        = pTileInfo->tileMode;
        pPm4Img->dbZInfo.bits.TILE_SPLIT__CI__VI            = pTileInfo->tileSplitBytes;
        pPm4Img->dbStencilInfo.bits.TILE_SPLIT__CI__VI      = pStTileInfo->tileSplitBytes;
    }

    const bool zReadOnly = (m_createInfo.flags.readOnlyDepth != 0);
    const bool sReadOnly = (m_createInfo.flags.readOnlyStencil != 0);

    if (m_flags.hTile)
    {
        const Gfx6Htile*const pHtile = m_pImage->GetHtile(m_depthSubresource);

        // Tell the HW that HTILE metadata is present.
        pPm4Img->dbZInfo.bits.ZRANGE_PRECISION           = pHtile->ZRangePrecision();
        pPm4Img->dbZInfo.bits.TILE_SURFACE_ENABLE        = 1;
        pPm4Img->dbStencilInfo.bits.TILE_STENCIL_DISABLE = pHtile->TileStencilDisabled();

        if (m_internalInfo.flags.isExpand || m_internalInfo.flags.isDepthCopy || m_internalInfo.flags.isStencilCopy)
        {
            pPm4Img->dbRenderControl.bits.DEPTH_COMPRESS_DISABLE   = !zReadOnly;
            pPm4Img->dbRenderControl.bits.STENCIL_COMPRESS_DISABLE = !sReadOnly;
        }
        else
        {
            pPm4Img->dbRenderControl.bits.DEPTH_COMPRESS_DISABLE   = (depthState == DepthStencilCompressed) ? 0 : 1;
            pPm4Img->dbRenderControl.bits.STENCIL_COMPRESS_DISABLE = (stencilState == DepthStencilCompressed) ? 0 : 1;
        }

        if (m_internalInfo.flags.isResummarize)
        {
            pPm4Img->dbRenderControl.bits.RESUMMARIZE_ENABLE = 1;
        }

        // NOTE: From regspec: For 32B tiles, indicates whether the data should be stored in the upper or lower half of
        // a 64B word. if the XOR reduce of ADDR5_SWIZZLE_MASK & {TILE_Y[1:0],TILE_X[1:0]} is set, use the upper half,
        // otherwise, use the lower half. Most likely best value is 0x1.
        //
        // The texture block can't understand the addr5-swizzle stuff, so if this surface might be texture fetched,
        // then don't use addr5-swizzle.
        if (m_flags.depthMetadataTexFetch == false)
        {
            pPm4Img->dbDepthInfo.bits.ADDR5_SWIZZLE_MASK = settings.dbAddr5SwizzleMask;
        }
        else
        {
            // This image might get texture-fetched, so setup any register info specific to texture fetches here.
            pPm4Img->dbZInfo.bits.DECOMPRESS_ON_N_ZPLANES__VI =
                CalcDecompressOnZPlanesValue(pPm4Img->dbRenderControl.bits.DEPTH_COMPRESS_DISABLE);
        }

        pPm4Img->dbZInfo.bits.ALLOW_EXPCLEAR       =
                (createInfo.usageFlags.shaderRead == 1) ? settings.dbPerTileExpClearEnable : 0;
        pPm4Img->dbStencilInfo.bits.ALLOW_EXPCLEAR =
                (createInfo.usageFlags.shaderRead == 1) ? settings.dbPerTileExpClearEnable : 0;

        pPm4Img->dbHtileSurface.u32All   = pHtile->DbHtileSurface().u32All;
        pPm4Img->dbPreloadControl.u32All = pHtile->DbPreloadControl().u32All;
    }
    else
    {
        // Tell the HW that HTILE metadata is not present.
        pPm4Img->dbDepthInfo.bits.ADDR5_SWIZZLE_MASK           = 0;
        pPm4Img->dbZInfo.bits.TILE_SURFACE_ENABLE              = 0;
        pPm4Img->dbStencilInfo.bits.TILE_STENCIL_DISABLE       = 1;
        pPm4Img->dbRenderControl.bits.DEPTH_COMPRESS_DISABLE   = 1;
        pPm4Img->dbRenderControl.bits.STENCIL_COMPRESS_DISABLE = 1;
    }

    const MergedFmtInfo* pChFmtInfo = MergedChannelFmtInfoTbl(m_device.Parent()->ChipProperties().gfxLevel);

    // Setup DB_Z_INFO, DB_DEPTH_INFO, and DB_STENCIL_INFO.
    pPm4Img->dbZInfo.bits.FORMAT       = HwZFmt(pChFmtInfo, zFmt);
    pPm4Img->dbZInfo.bits.READ_SIZE    = settings.dbRequestSize;
    pPm4Img->dbZInfo.bits.NUM_SAMPLES  = Log2(createInfo.samples);
    pPm4Img->dbStencilInfo.bits.FORMAT = HwStencilFmt(pChFmtInfo, sFmt);

    //     Z_INFO and STENCIL_INFO CLEAR_DISALLOWED were never reliably working on GFX8 or 9.  Although the
    //     bit is not implemented, it does actually connect into logic.  In block regressions, some tests
    //     worked but many tests did not work using this bit.  Please do not set this bit
    PAL_ASSERT((pPm4Img->dbZInfo.bits.CLEAR_DISALLOWED__VI == 0) &&
               (pPm4Img->dbStencilInfo.bits.CLEAR_DISALLOWED__VI == 0));

    const auto& actualExtent = pDepthSubResInfo->actualExtentTexels;

    // Setup DB_DEPTH_SLICE.
    pPm4Img->dbDepthSlice.bits.SLICE_TILE_MAX = (actualExtent.width * actualExtent.height / TilePixels) - 1;

    // Setup DB_DEPTH_SIZE.
    pPm4Img->dbDepthSize.bits.PITCH_TILE_MAX  = (actualExtent.width  / TileWidth) - 1;
    pPm4Img->dbDepthSize.bits.HEIGHT_TILE_MAX = (actualExtent.height / TileWidth) - 1;

    // NOTE: Base addresses of the depth and stencil planes aren't known until bind-time.
    pPm4Img->dbZReadBase.u32All        = 0;
    pPm4Img->dbZWriteBase.u32All       = 0;
    pPm4Img->dbStencilReadBase.u32All  = 0;
    pPm4Img->dbStencilWriteBase.u32All = 0;
    pPm4Img->dbHtileDataBase.u32All    = 0;
    pPm4Img->coherDestBase0.u32All     = 0;

    // Setup DB_DEPTH_VIEW.
    pPm4Img->dbDepthView.bits.SLICE_START = m_createInfo.baseArraySlice;
    pPm4Img->dbDepthView.bits.SLICE_MAX   = (m_createInfo.arraySize + m_createInfo.baseArraySlice - 1);
    pPm4Img->dbDepthView.bits.Z_READ_ONLY       = (zReadOnly ? 1 : 0);
    pPm4Img->dbDepthView.bits.STENCIL_READ_ONLY = (sReadOnly ? 1 : 0);

    // Set clear enable fields if the create info indicates the view should be a fast clear view
    pPm4Img->dbRenderControl.bits.DEPTH_CLEAR_ENABLE   = m_internalInfo.flags.isDepthClear;
    pPm4Img->dbRenderControl.bits.STENCIL_CLEAR_ENABLE = m_internalInfo.flags.isStencilClear;
    pPm4Img->dbRenderControl.bits.DEPTH_COPY = m_internalInfo.flags.isDepthCopy;
    pPm4Img->dbRenderControl.bits.STENCIL_COPY = m_internalInfo.flags.isStencilCopy;

    if (m_internalInfo.flags.isDepthCopy || m_internalInfo.flags.isStencilCopy)
    {
        pPm4Img->dbRenderControl.bits.COPY_SAMPLE = 0;
        pPm4Img->dbRenderControl.bits.COPY_CENTROID = 1;
    }

    // For 4xAA and 8xAA need to decompress on flush for better performance
    pPm4Img->dbRenderOverride2.bits.DECOMPRESS_Z_ON_FLUSH = (createInfo.samples > 2) ? 1 : 0;

    pPm4Img->dbRenderOverride2.bits.DISABLE_COLOR_ON_VALIDATION = settings.dbDisableColorOnValidation;

    // Setup PA_SU_POLY_OFFSET_DB_FMT_CNTL.
    if (m_createInfo.flags.absoluteDepthBias == 0)
    {
        // NOTE: The client has indicated this Image has promoted 24bit depth to 32bits, we should set the negative num
        // bit as -24 and use fixed points format
        pPm4Img->paSuPolyOffsetDbFmtCntl.bits.POLY_OFFSET_NEG_NUM_DB_BITS =
            (createInfo.usageFlags.depthAsZ24 == 1) ? -24 : ((pPm4Img->dbZInfo.bits.FORMAT == Z_16) ? -16 : -23);
        pPm4Img->paSuPolyOffsetDbFmtCntl.bits.POLY_OFFSET_DB_IS_FLOAT_FMT =
            (pPm4Img->dbZInfo.bits.FORMAT == Z_32_FLOAT && (createInfo.usageFlags.depthAsZ24 == 0)) ? 1 : 0;    }
    else
    {
        pPm4Img->paSuPolyOffsetDbFmtCntl.u32All = 0;
    }

    // Setup screen scissor registers.
    pPm4Img->paScScreenScissorTl.bits.TL_X = PaScScreenScissorMin;
    pPm4Img->paScScreenScissorTl.bits.TL_Y = PaScScreenScissorMin;
    pPm4Img->paScScreenScissorBr.bits.BR_X = pDepthSubResInfo->extentTexels.width;
    pPm4Img->paScScreenScissorBr.bits.BR_Y = pDepthSubResInfo->extentTexels.height;

    // Setup DB_RENDER_OVERRIDE fields
    DB_RENDER_OVERRIDE dbRenderOverride;

    dbRenderOverride.u32All = 0;

    // Enable HiZ/HiS based on settings
    dbRenderOverride.bits.FORCE_HIZ_ENABLE  = settings.hiDepthEnable   ? FORCE_OFF : FORCE_DISABLE;
    dbRenderOverride.bits.FORCE_HIS_ENABLE0 = settings.hiStencilEnable ? FORCE_OFF : FORCE_DISABLE;
    dbRenderOverride.bits.FORCE_HIS_ENABLE1 = settings.hiStencilEnable ? FORCE_OFF : FORCE_DISABLE;

    // Turn off HiZ/HiS if the current image layout disallows use of the htile
    if (m_internalInfo.flags.u32All == 0)
    {
        if (depthState == DepthStencilDecomprNoHiZ)
        {
            dbRenderOverride.bits.FORCE_HIZ_ENABLE = FORCE_DISABLE;
        }

        if (stencilState == DepthStencilDecomprNoHiZ)
        {
            dbRenderOverride.bits.FORCE_HIS_ENABLE0 = FORCE_DISABLE;
            dbRenderOverride.bits.FORCE_HIS_ENABLE1 = FORCE_DISABLE;
        }
    }

    if (m_internalInfo.flags.isResummarize)
    {
        dbRenderOverride.bits.FORCE_Z_VALID           = !zReadOnly;
        dbRenderOverride.bits.FORCE_STENCIL_VALID     = !sReadOnly;
        dbRenderOverride.bits.NOOP_CULL_DISABLE       = 1;
        dbRenderOverride.bits.DISABLE_TILE_RATE_TILES = 1;
    }

    PAL_ASSERT((dbRenderOverride.u32All & ~DbRenderOverrideRmwMask) == 0);

    cmdUtil.BuildContextRegRmw(
        mmDB_RENDER_OVERRIDE,
        DbRenderOverrideRmwMask,
        dbRenderOverride.u32All,
        &pPm4Img->dbRenderOverrideRmw);
}

// =====================================================================================================================
// Updates the specified PM4 image with the virtual addresses of the image and the image's various metadata addresses.
void DepthStencilView::UpdateImageVa(
    DepthStencilViewPm4Img* pPm4Img
    ) const
{
    // The "GetSubresource256BAddrSwizzled" function will crash if no memory has been bound to
    // the associated image yet, so don't do anything if it's not safe
    if (m_pImage->Parent()->GetBoundGpuMemory().IsBound())
    {
        if (m_flags.hTile)
        {
            // Program fast-clear metadata base address.
            gpusize metaDataVirtAddr = m_pImage->FastClearMetaDataAddr(MipLevel());
            PAL_ASSERT((metaDataVirtAddr & 0x3) == 0);

            // If this view uses the legacy LOAD_CONTEXT_REG packet to load the fast-clear registers, we need to
            // subtract the register offset for the LOAD packet from the address we specify to account for the fact that
            // the CP uses that register offset for both the register address and to compute the final GPU address to
            // fetch from. The newer LOAD_CONTEXT_REG_INDEX packet does not add the register offset to the GPU address.
            if (m_flags.usesLoadRegIndexPkt == 0)
            {
                metaDataVirtAddr -= (sizeof(uint32) * pPm4Img->loadMetaData.regOffset);

                pPm4Img->loadMetaData.addrLo         = LowPart(metaDataVirtAddr);
                pPm4Img->loadMetaData.addrHi.ADDR_HI = HighPart(metaDataVirtAddr);
            }
            else
            {
                // Note that the packet header doesn't provide a proper addr_hi alias (it goes into the addrOffset).
                pPm4Img->loadMetaDataIndex.addrLo.ADDR_LO = (LowPart(metaDataVirtAddr) >> 2);
                pPm4Img->loadMetaDataIndex.addrOffset     = HighPart(metaDataVirtAddr);
            }

            // Otherwise, program HTile base address.
            pPm4Img->dbHtileDataBase.bits.BASE_256B = m_pImage->GetHtile256BAddr(m_depthSubresource);
        }

        if (m_flags.depth)
        {
            const uint32 gpuVirtAddr = m_pImage->GetSubresource256BAddrSwizzled(m_depthSubresource);

            // Program depth read and write bases
            pPm4Img->dbZReadBase.u32All  = gpuVirtAddr;
            pPm4Img->dbZWriteBase.u32All = gpuVirtAddr;
        }

        if (m_flags.stencil)
        {
            const uint32 gpuVirtAddr = m_pImage->GetSubresource256BAddrSwizzled(m_stencilSubresource);

            // Program stencil read and write bases
            pPm4Img->dbStencilReadBase.u32All  = gpuVirtAddr;
            pPm4Img->dbStencilWriteBase.u32All = gpuVirtAddr;

            // Copy the stencil base address into one of the CP's generic sync registers.
            pPm4Img->coherDestBase0.bits.DEST_BASE_256B = pPm4Img->dbStencilWriteBase.bits.BASE_256B;
        }
    }
}

// =====================================================================================================================
// Writes the PM4 commands required to bind to depth/stencil slot. Returns the next unused DWORD in pCmdSpace.
uint32* DepthStencilView::WriteCommands(
    ImageLayout             depthLayout,   // Allowed usages/queues for the depth aspect. Implies compression state.
    ImageLayout             stencilLayout, // Allowed usages/queues for the stencil aspect. Implies compression state.
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace
    ) const
{
    const DepthStencilCompressionState depthCompressionState =
        m_pImage->LayoutToDepthCompressionState(m_depthSubresource, depthLayout);
    const DepthStencilCompressionState stencilCompressionState =
        m_pImage->LayoutToDepthCompressionState(m_stencilSubresource, stencilLayout);

    PAL_ASSERT(pCmdStream != nullptr);

    // The "GetSubresource256BAddrSwizzled" function will crash if no memory has been bound to the associated image
    // yet, so don't do anything if it's not safe
    if (m_createInfo.flags.imageVaLocked)
    {
        const DepthStencilViewPm4Img*const pPm4Img = &m_pm4Images[depthCompressionState][stencilCompressionState];

        pCmdSpace = pCmdStream->WritePm4Image(pPm4Img->spaceNeeded, pPm4Img, pCmdSpace);
    }
    else if (m_pImage->Parent()->GetBoundGpuMemory().IsBound())
    {
        // Spawn a local copy of the PM4 image, since the Base address and HTile address need to be updated in this
        // method. The contents of the local copy will depend on which Image state is specified.
        DepthStencilViewPm4Img pm4Commands = m_pm4Images[depthCompressionState][stencilCompressionState];

        UpdateImageVa(&pm4Commands);

        pCmdSpace = pCmdStream->WritePm4Image(pm4Commands.spaceNeeded, &pm4Commands, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Determines the proper value of the DB_Z_INFO.DECOMPRESS_ON_N_ZPLANES register value
uint32 DepthStencilView::CalcDecompressOnZPlanesValue(
    bool depthCompressDisable // DEPTH_COMPRESS_DISABLE of DB_RENDER_CONTROL
    ) const
{
    const ImageCreateInfo&  createInfo = m_pImage->Parent()->GetImageCreateInfo();
    const ChNumFormat       format     = createInfo.swizzledFormat.format;
    const Gfx6PalSettings&  settings   = m_device.Settings();

    uint32 decompressOnZPlanes = 0;

    // Limit the Z plane compression to allow for TC reads.  Up to 16 Z planes can be compressed, however TC is limited
    // to 1-4 based on sample count.
    // NOTE: DECOMPRESS_ON_N_ZPLANES = 0 means 16 Z plane compression (default)
    if ((format == ChNumFormat::X16_Unorm) || (format == ChNumFormat::D16_Unorm_S8_Uint))
    {
        // For Gfx8, TC can read either fast cleared or uncompressed Z_16 depth resource. There is no support for TC
        // to read from compressed shader resource directly. So in order to support shader compatibility, set Z plane
        // compression to 1 so no Z plane will be compressed.
        decompressOnZPlanes = 1;
    }
    else
    {
        switch (createInfo.samples)
        {
        case 2:
            // 2x set 2 Z plane compression
            decompressOnZPlanes = 3;
            break;

        case 4:
            if (m_device.WaDbDecompressOnPlanesFor4xMsaa() == true)
            {
                // In 4xAA mode, when surfaces are compressed to two planes, the Z decompress stall logic may cause a
                // hang. UMD driver WA part is to restrict DB_Z_INFO.DECOMPRESS_ON_N_ZPLANES to no more than 2 when
                // used with 4xAA.
                decompressOnZPlanes = 2;
            }
            else
            {
                // 4x set 2 Z plane compression
                decompressOnZPlanes = 3;
            }
            break;

        case 8:
            // 8x set 1 Z plane compression
            decompressOnZPlanes = 2;
            break;

        default:
            // 1x, set 4 Z plane compression
            decompressOnZPlanes = 5;
            break;
        }
    }

    // Decompress BLT performance is poor but on Fiji and Gfx8.1 variants, this can be avoided by setting
    // DB_RENDER_CONTROL.DECOMPRESS_ON_N_ZPLANES to 0.
    if (m_internalInfo.flags.isExpand && depthCompressDisable && m_device.WaDbDecompressPerformance())
    {
        decompressOnZPlanes = 0;
    }

    return decompressOnZPlanes;
}

// =====================================================================================================================
// The TC compatibility bin in db_tcp_tag_calc_pipe is not stalled properly. Having multiple concunrrent contexts with
// different TC compatibility settings may cause an address calculation error. Effects are varied, depending upon what
// is being read or written and how the returned or written data is used.  It is possible, but not guaranteed, that a
// chip-hang could occur.  A software workaround for this issue is to issue a surface sync to the HTILE (or to
// everything) when switching between tc_compatible and non-tc compatible mode.
//
// Returns the next unused DWORD in pCmdSpace.
//

// NOTE: The DB has to be synced along with the HTILE sync and the HTILE sync has to occur after AFTER the DB sync.
//       This is because the CP doesn't wait for the HTILE's context to be done before starting the sync as it does for
//       the depth surface's context since the CP doesn't track the HTILE base address, only the Z one.
uint32* DepthStencilView::WriteTcCompatFlush(
    const Device&           device,
    const DepthStencilView* pNewView, // New Depth Stencil View
    const DepthStencilView* pOldView, // Previous Depth Stencil View
    uint32*                 pCmdSpace)
{
    const auto& settings = device.Settings();
    if (device.WaDbTcCompatFlush() != Gfx8TcCompatDbFlushWaNever)
    {
        // Workaround bit only makes sense for Gfx8+ ASICs.
        PAL_ASSERT(device.Parent()->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp8);

        const CmdUtil& cmdUtil = device.CmdUtil();

        if ((pOldView != nullptr) && (pNewView != nullptr))
        {
            // If the previously bound DB and the new DB have different TC-compatability states, then we need
            // to do a flush
            const Image*const           pOldImage      = pOldView->GetImage();
            const SubresId              oldBaseSubRes  = pOldImage->Parent()->GetBaseSubResource();
            const SubResourceInfo*const pOldSubResInfo = pOldImage->Parent()->SubresourceInfo(oldBaseSubRes);
            const bool                  oldIsTcCompat  = pOldSubResInfo->flags.supportMetaDataTexFetch;

            const Image*const           pNewImage      = pNewView->GetImage();
            const SubresId              newBaseSubRes  = pNewImage->Parent()->GetBaseSubResource();
            const SubResourceInfo*const pNewSubResInfo = pNewImage->Parent()->SubresourceInfo(newBaseSubRes);
            const bool                  newIsTcCompat  = pNewSubResInfo->flags.supportMetaDataTexFetch;

            if (oldIsTcCompat != newIsTcCompat)
            {
                regCP_COHER_CNTL  coherCntl = {};
                coherCntl.bits.DB_ACTION_ENA    = 1;
                coherCntl.bits.DB_DEST_BASE_ENA = 1;

                // We need to do a flush here.  Since we have the old image data, we can force a sync on its data.
                // The sync range needs to include any hTile data so we just sync the entire surface.
                pCmdSpace += cmdUtil.BuildSurfaceSync(coherCntl,
                                                      SURFACE_SYNC_ENGINE_ME,
                                                      pOldImage->Parent()->GetGpuVirtualAddr(),
                                                      pOldImage->Parent()->GetGpuMemSize(),
                                                      pCmdSpace);
            }
            else
            {
                // The DB state is not changing from a TC-compatability perspective, so there's no need to issue a
                // surface sync here.
            }
        }
        else if (device.WaDbTcCompatFlush() == Gfx8TcCompatDbFlushWaAlways)
        {
            // Writes a full range surface sync to the specified stream that invalidates the DB.
            regCP_COHER_CNTL coherCntl = {};
            coherCntl.bits.DB_ACTION_ENA    = 1;
            coherCntl.bits.DB_DEST_BASE_ENA = 1;

            // Write a full range surface sync that invalidates the DB.
            pCmdSpace += cmdUtil.BuildSurfaceSync(coherCntl,
                                                  SURFACE_SYNC_ENGINE_ME,
                                                  FullSyncBaseAddr,
                                                  FullSyncSize,
                                                  pCmdSpace);
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Writes a new fast clear depth and/or stencil register value.  This function is sometimes called after a fast clear
// when it is detected that the cleared image is already bound with the old fast clear values loaded.
uint32* DepthStencilView::WriteUpdateFastClearDepthStencilValue(
    uint32     metaDataClearFlags,
    float      depth,
    uint8      stencil,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace)
{
    struct ClearValueRegs
    {
        regDB_STENCIL_CLEAR dbStencilClear;
        regDB_DEPTH_CLEAR   dbDepthClear;
    };

    ClearValueRegs clearValueRegs;

    if (metaDataClearFlags == (HtileAspectDepth | HtileAspectStencil))
    {
        clearValueRegs.dbDepthClear.f32All       = depth;
        clearValueRegs.dbStencilClear.u32All     = 0;
        clearValueRegs.dbStencilClear.bits.CLEAR = stencil;

        pCmdSpace = pCmdStream->WriteSetSeqContextRegs(
            mmDB_STENCIL_CLEAR,
            mmDB_DEPTH_CLEAR,
            &clearValueRegs,
            pCmdSpace);
    }
    else if (metaDataClearFlags == HtileAspectDepth)
    {
        pCmdSpace = pCmdStream->WriteSetOneContextReg(
            mmDB_DEPTH_CLEAR,
            *reinterpret_cast<const uint32*>(&depth),
            pCmdSpace);
    }
    else
    {
        PAL_ASSERT(metaDataClearFlags == HtileAspectStencil);

        clearValueRegs.dbStencilClear.u32All     = 0;
        clearValueRegs.dbStencilClear.bits.CLEAR = stencil;

        pCmdSpace = pCmdStream->WriteSetOneContextReg(
            mmDB_STENCIL_CLEAR,
            clearValueRegs.dbStencilClear.u32All,
            pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// On Gfx8, there is a bug on cleared TC-compatible surfaces where the ZRange is not reset after LateZ kills pixels.
// The workaround for this is to always set DB_STENCIL_INFO.TILE_STENCIL_DISABLE = 0 (even with no stencil) and set
// DB_Z_INFO.ZRANGE_PRECISION to match the last fast clear value. Since ZRANGE_PRECISION is currently always set to 1
// by default, we only need to re-write it if the last fast clear value is 0.0f.
//
//
// This function writes the PM4 to set ZRANGE_PRECISION to 0. There are two cases where this needs to be called:
//      1. After binding a TC-compatible depth target. We need to check the workaroud metadata to know if the last
//         clear value was 0.0f, so requiresCondExec should be true.
//      2. After a compute-based fast clear to 0.0f if this view is currently bound as a depth target. We do not need
//         to look at the metadata in this case, so requiresCondExec should be false.
//
// Returns the next unused DWORD in pCmdSpace.
uint32* DepthStencilView::UpdateZRangePrecision(
    bool       requiresCondExec,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    if (m_device.WaTcCompatZRange() && m_flags.depth && m_flags.depthMetadataTexFetch)
    {
        PAL_ASSERT(GetImage()->HasWaTcCompatZRangeMetaData());

        if (requiresCondExec)
        {
            const CmdUtil&    cmdUtil           = m_device.CmdUtil();
            const gpusize     metaDataVirtAddr  = GetImage()->GetWaTcCompatZRangeMetaDataAddr(MipLevel());
            const Pal::uint32 setContextRegSize = cmdUtil.GetSetDataHeaderSize() + 1;

            // Build a COND_EXEC to check the workaround metadata. If the last clear value was 0.0f, the metadata will be
            // non-zero and the register will be re-written, otherwise the metadata will be 0 and the register write will
            // be skipped.
            pCmdSpace += cmdUtil.BuildCondExec(metaDataVirtAddr, setContextRegSize, pCmdSpace);
        }

        // DB_Z_INFO is the same for all compression states
        regDB_Z_INFO regVal = m_pm4Images[DepthStencilCompressed][DepthStencilCompressed].dbZInfo;
        regVal.bits.ZRANGE_PRECISION = 0;

        pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_Z_INFO, regVal.u32All, pCmdSpace);
    }

    return pCmdSpace;
}

} // Gfx6
} // Pal
