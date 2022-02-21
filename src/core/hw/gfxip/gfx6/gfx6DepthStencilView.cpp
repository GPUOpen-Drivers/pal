/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
    m_pImage(GetGfx6Image(createInfo.pImage))
{
    PAL_ASSERT(createInfo.pImage != nullptr);
    const auto& imageInfo = m_pImage->Parent()->GetImageCreateInfo();
    const auto& parent    = *m_device.Parent();

    m_flags.u32All            = 0;
    m_flags.hTile             = m_pImage->HasHtileData();
    m_flags.hiSPretests       = m_pImage->HasHiSPretestsMetaData();
    m_flags.depth             = (createInfo.flags.stencilOnlyView == 0) &&
                                parent.SupportsDepth(imageInfo.swizzledFormat.format, imageInfo.tiling);
    m_flags.stencil           = (createInfo.flags.depthOnlyView == 0) &&
                                parent.SupportsStencil(imageInfo.swizzledFormat.format, imageInfo.tiling);
    m_flags.readOnlyDepth     = createInfo.flags.readOnlyDepth;
    m_flags.readOnlyStencil   = createInfo.flags.readOnlyStencil;
    m_flags.waDbTcCompatFlush = m_device.WaDbTcCompatFlush();
    m_flags.viewVaLocked      = createInfo.flags.imageVaLocked;
    m_flags.isExpand          = internalInfo.flags.isExpand;

    m_flags.usesLoadRegIndexPkt = m_device.Parent()->ChipProperties().gfx6.supportLoadRegIndexPkt;

    if (m_flags.depth && m_flags.stencil)
    {
        // Depth & Stencil view.
        m_depthSubresource.plane        = 0;
        m_depthSubresource.mipLevel     = createInfo.mipLevel;
        m_depthSubresource.arraySlice   = 0;
        m_stencilSubresource.plane      = 1;
        m_stencilSubresource.mipLevel   = createInfo.mipLevel;
        m_stencilSubresource.arraySlice = 0;
    }
    else if (m_flags.depth)
    {
        // Depth-only view.
        m_depthSubresource.plane      = 0;
        m_depthSubresource.mipLevel   = createInfo.mipLevel;
        m_depthSubresource.arraySlice = 0;
        m_stencilSubresource          = m_depthSubresource;
    }
    else
    {
        // Stencil-only view.
        m_stencilSubresource.plane      =
            parent.SupportsDepth(imageInfo.swizzledFormat.format, imageInfo.tiling) ? 1 : 0;
        m_stencilSubresource.mipLevel   = createInfo.mipLevel;
        m_stencilSubresource.arraySlice = 0;
        m_depthSubresource              = m_stencilSubresource;
    }

    m_depthLayoutToState   = m_pImage->LayoutToDepthCompressionState(m_depthSubresource);
    m_stencilLayoutToState = m_pImage->LayoutToDepthCompressionState(m_stencilSubresource);

    const SubResourceInfo*const pDepthSubResInfo   = m_pImage->Parent()->SubresourceInfo(m_depthSubresource);
    const SubResourceInfo*const pStencilSubResInfo = m_pImage->Parent()->SubresourceInfo(m_stencilSubresource);

    m_flags.depthMetadataTexFetch   = pDepthSubResInfo->flags.supportMetaDataTexFetch;
    m_flags.stencilMetadataTexFetch = pStencilSubResInfo->flags.supportMetaDataTexFetch;

    memset(&m_regs, 0, sizeof(m_regs));
    InitRegisters(createInfo, internalInfo);

    if (m_flags.viewVaLocked != 0)
    {
        UpdateImageVa(&m_regs);
    }
}

// =====================================================================================================================
// Finalizes the PM4 packet image by setting up the register values used to write this View object to hardware.
void DepthStencilView::InitRegisters(
    const DepthStencilViewCreateInfo&         createInfo,
    const DepthStencilViewInternalCreateInfo& internalInfo)
{
    const ImageCreateInfo& imageCreateInfo = m_pImage->Parent()->GetImageCreateInfo();

    const Gfx6PalSettings& settings = m_device.Settings();

    const SubResourceInfo*const pDepthSubResInfo   = m_pImage->Parent()->SubresourceInfo(m_depthSubresource);
    const SubResourceInfo*const pStencilSubResInfo = m_pImage->Parent()->SubresourceInfo(m_stencilSubresource);

    const ChNumFormat zFmt = pDepthSubResInfo->format.format;
    const ChNumFormat sFmt = pStencilSubResInfo->format.format;

    const AddrMgr1::TileInfo*const pTileInfo   = AddrMgr1::GetTileInfo(m_pImage->Parent(), m_depthSubresource);
    const AddrMgr1::TileInfo*const pStTileInfo = AddrMgr1::GetTileInfo(m_pImage->Parent(), m_stencilSubresource);

    if (m_device.Parent()->ChipProperties().gfxLevel == GfxIpLevel::GfxIp6)
    {
        m_regs.dbZInfo.bits.TILE_MODE_INDEX       = pTileInfo->tileIndex;
        m_regs.dbStencilInfo.bits.TILE_MODE_INDEX = pStTileInfo->tileIndex;
    }
    else
    {
        // For non-Gfx6 asics:
        // TILE_MODE_INDEX fields have been removed from DB_Z_INFO and DB_STENCIL_INFO and instead the per surface
        // tiling parameters will need to be programmed directly in DB_Z_INFO, DB_DEPTH_INFO, and DB_STENCIL_INFO
        // registers.
        m_regs.dbDepthInfo.bits.PIPE_CONFIG__CI__VI       = pTileInfo->pipeConfig;
        m_regs.dbDepthInfo.bits.NUM_BANKS__CI__VI         = pTileInfo->banks;
        m_regs.dbDepthInfo.bits.BANK_WIDTH__CI__VI        = pTileInfo->bankWidth;
        m_regs.dbDepthInfo.bits.BANK_HEIGHT__CI__VI       = pTileInfo->bankHeight;
        m_regs.dbDepthInfo.bits.MACRO_TILE_ASPECT__CI__VI = pTileInfo->macroAspectRatio;
        m_regs.dbDepthInfo.bits.ARRAY_MODE__CI__VI        = pTileInfo->tileMode;
        m_regs.dbZInfo.bits.TILE_SPLIT__CI__VI            = pTileInfo->tileSplitBytes;
        m_regs.dbStencilInfo.bits.TILE_SPLIT__CI__VI      = pStTileInfo->tileSplitBytes;
    }

    const bool zReadOnly = (createInfo.flags.readOnlyDepth != 0);
    const bool sReadOnly = (createInfo.flags.readOnlyStencil != 0);
    const bool isResummarize = (createInfo.flags.resummarizeHiZ != 0);

    if (m_flags.hTile != 0)
    {
        const Gfx6Htile*const pHtile = m_pImage->GetHtile(m_depthSubresource);

        // Tell the HW that HTILE metadata is present.
        m_regs.dbZInfo.bits.ZRANGE_PRECISION           = pHtile->ZRangePrecision();
        m_regs.dbZInfo.bits.TILE_SURFACE_ENABLE        = 1;
        m_regs.dbStencilInfo.bits.TILE_STENCIL_DISABLE = pHtile->TileStencilDisabled();

        if (internalInfo.flags.isExpand | internalInfo.flags.isDepthCopy | internalInfo.flags.isStencilCopy)
        {
            m_regs.dbRenderControl.bits.DEPTH_COMPRESS_DISABLE   = !zReadOnly;
            m_regs.dbRenderControl.bits.STENCIL_COMPRESS_DISABLE = !sReadOnly;

            m_flags.dbRenderControlLocked = 1; // This cannot change at bind-time for expands and copies!
        }

        if (isResummarize)
        {
            m_regs.dbRenderControl.bits.RESUMMARIZE_ENABLE = 1;
        }

        // NOTE: From regspec: For 32B tiles, indicates whether the data should be stored in the upper or lower half of
        // a 64B word. if the XOR reduce of ADDR5_SWIZZLE_MASK & {TILE_Y[1:0],TILE_X[1:0]} is set, use the upper half,
        // otherwise, use the lower half. Most likely best value is 0x1.
        //
        // The texture block can't understand the addr5-swizzle stuff, so if this surface might be texture fetched,
        // then don't use addr5-swizzle.
        if (m_flags.depthMetadataTexFetch == 0)
        {
            m_regs.dbDepthInfo.bits.ADDR5_SWIZZLE_MASK = settings.dbAddr5SwizzleMask;
        }
        else
        {
            // This image might get texture-fetched, so setup any register info specific to texture fetches here.
            m_regs.dbZInfo.bits.DECOMPRESS_ON_N_ZPLANES__VI =
                CalcDecompressOnZPlanesValue(m_regs.dbRenderControl.bits.DEPTH_COMPRESS_DISABLE);
        }

        m_regs.dbZInfo.bits.ALLOW_EXPCLEAR       =
                (imageCreateInfo.usageFlags.shaderRead == 1) ? settings.dbPerTileExpClearEnable : 0;
        m_regs.dbStencilInfo.bits.ALLOW_EXPCLEAR =
                (imageCreateInfo.usageFlags.shaderRead == 1) ? settings.dbPerTileExpClearEnable : 0;

        m_regs.dbHtileSurface.u32All   = pHtile->DbHtileSurface().u32All;
        m_regs.dbPreloadControl.u32All = pHtile->DbPreloadControl().u32All;
    }
    else
    {
        // Tell the HW that HTILE metadata is not present.
        m_regs.dbDepthInfo.bits.ADDR5_SWIZZLE_MASK           = 0;
        m_regs.dbZInfo.bits.TILE_SURFACE_ENABLE              = 0;
        m_regs.dbStencilInfo.bits.TILE_STENCIL_DISABLE       = 1;
        m_regs.dbRenderControl.bits.DEPTH_COMPRESS_DISABLE   = 1;
        m_regs.dbRenderControl.bits.STENCIL_COMPRESS_DISABLE = 1;
    }

    const MergedFmtInfo* pChFmtInfo = MergedChannelFmtInfoTbl(m_device.Parent()->ChipProperties().gfxLevel);

    // Setup DB_Z_INFO, DB_DEPTH_INFO, and DB_STENCIL_INFO.
    m_regs.dbZInfo.bits.FORMAT       = HwZFmt(pChFmtInfo, zFmt);
    m_regs.dbZInfo.bits.READ_SIZE    = settings.dbRequestSize;
    m_regs.dbZInfo.bits.NUM_SAMPLES  = Log2(imageCreateInfo.samples);
    m_regs.dbStencilInfo.bits.FORMAT = HwStencilFmt(pChFmtInfo, sFmt);

    //     Z_INFO and STENCIL_INFO CLEAR_DISALLOWED were never reliably working on GFX8 or 9.  Although the
    //     bit is not implemented, it does actually connect into logic.  In block regressions, some tests
    //     worked but many tests did not work using this bit.  Please do not set this bit
    PAL_ASSERT((m_regs.dbZInfo.bits.CLEAR_DISALLOWED__VI == 0) &&
               (m_regs.dbStencilInfo.bits.CLEAR_DISALLOWED__VI == 0));

    const auto& actualExtent = pDepthSubResInfo->actualExtentTexels;

    // Setup DB_DEPTH_SLICE.
    m_regs.dbDepthSlice.bits.SLICE_TILE_MAX = (actualExtent.width * actualExtent.height / TilePixels) - 1;

    // Setup DB_DEPTH_SIZE.
    m_regs.dbDepthSize.bits.PITCH_TILE_MAX  = (actualExtent.width  / TileWidth) - 1;
    m_regs.dbDepthSize.bits.HEIGHT_TILE_MAX = (actualExtent.height / TileWidth) - 1;

    // NOTE: Base addresses of the depth and stencil planes aren't known until bind-time.
    m_regs.dbZReadBase.u32All        = 0;
    m_regs.dbZWriteBase.u32All       = 0;
    m_regs.dbStencilReadBase.u32All  = 0;
    m_regs.dbStencilWriteBase.u32All = 0;
    m_regs.dbHtileDataBase.u32All    = 0;
    m_regs.coherDestBase0.u32All     = 0;

    // Setup DB_DEPTH_VIEW.
    m_regs.dbDepthView.bits.SLICE_START = createInfo.baseArraySlice;
    m_regs.dbDepthView.bits.SLICE_MAX   = (createInfo.arraySize + createInfo.baseArraySlice - 1);
    m_regs.dbDepthView.bits.Z_READ_ONLY       = (zReadOnly ? 1 : 0);
    m_regs.dbDepthView.bits.STENCIL_READ_ONLY = (sReadOnly ? 1 : 0);

    // Set clear enable fields if the create info indicates the view should be a fast clear view
    m_regs.dbRenderControl.bits.DEPTH_CLEAR_ENABLE   = internalInfo.flags.isDepthClear;
    m_regs.dbRenderControl.bits.STENCIL_CLEAR_ENABLE = internalInfo.flags.isStencilClear;
    m_regs.dbRenderControl.bits.DEPTH_COPY           = internalInfo.flags.isDepthCopy;
    m_regs.dbRenderControl.bits.STENCIL_COPY         = internalInfo.flags.isStencilCopy;

    if (internalInfo.flags.isDepthCopy | internalInfo.flags.isStencilCopy)
    {
        m_regs.dbRenderControl.bits.COPY_SAMPLE   = 0;
        m_regs.dbRenderControl.bits.COPY_CENTROID = 1;
    }

    // For 4xAA and 8xAA need to decompress on flush for better performance
    m_regs.dbRenderOverride2.bits.DECOMPRESS_Z_ON_FLUSH = (imageCreateInfo.samples > 2) ? 1 : 0;

    m_regs.dbRenderOverride2.bits.DISABLE_COLOR_ON_VALIDATION = settings.dbDisableColorOnValidation;

    // From the register spec, it seems that for 16 bit unorm DB, we need to write
    // -16 and for 24 bit unorm DB, we need to write - 24 to POLY_OFFSET_NEG_NUM_DB_BITS
    //
    // based on a set of local tests, my observation is:
    // for unorm depth buffer, e.g. 24bit unorm, HW uses rounding after applying float to
    // 24bit unorm convertion  where the fomula should be u = round(f * (2^24 - 1))
    //
    // for the polygon offset unit value, opengl spec says:
    // "It is the smallest difference in window coordinate z values that is guaranteed
    // to remain distinct throughout polygon rasterization and in the depth buffer"
    //
    // so that sounds like the delta is 1/(2^24-1). If we do set register field to be -24,
    // it seems that the HW apply a delta as 1/(2^24), which is a tiny little bit smaller than
    // the other one. So when there is a float Z value f that can be converted by f*(2^24-1) to be x.5
    // if we request a polygon offset unit of 1.0f, the HW will do  (f + 1/2^24)*(2^24-1) ,
    // that will be (x+1).4999...   so when x.5 and (x+1).4999.... are both being rounded,
    // the result are both x+1, that is, to this Z value f, the polygonoffset is not applied.
    // this could be the reason we use -22 for 24 bit and - 15 for 16 bit depth buffer.

    // Setup PA_SU_POLY_OFFSET_DB_FMT_CNTL.
    if (createInfo.flags.absoluteDepthBias == 0)
    {
        // NOTE: The client has indicated this Image has promoted 24bit depth to 32bits, we should set the negative num
        // bit as -24 and use fixed points format
        m_regs.paSuPolyOffsetDbFmtCntl.bits.POLY_OFFSET_NEG_NUM_DB_BITS =
            (imageCreateInfo.usageFlags.depthAsZ24 == 1) ? -22 : ((m_regs.dbZInfo.bits.FORMAT == Z_16) ? -15 : -23);
        m_regs.paSuPolyOffsetDbFmtCntl.bits.POLY_OFFSET_DB_IS_FLOAT_FMT =
            ((m_regs.dbZInfo.bits.FORMAT == Z_32_FLOAT) && (imageCreateInfo.usageFlags.depthAsZ24 == 0)) ? 1 : 0;
    }
    else
    {
        m_regs.paSuPolyOffsetDbFmtCntl.u32All = 0;
    }

    m_extent.width  = pDepthSubResInfo->extentTexels.width;
    m_extent.height = pDepthSubResInfo->extentTexels.height;

    // Setup DB_RENDER_OVERRIDE fields
    // Enable HiZ/HiS based on settings
    m_regs.dbRenderOverride.bits.FORCE_HIZ_ENABLE  = settings.hiDepthEnable   ? FORCE_OFF : FORCE_DISABLE;
    m_regs.dbRenderOverride.bits.FORCE_HIS_ENABLE0 = settings.hiStencilEnable ? FORCE_OFF : FORCE_DISABLE;
    m_regs.dbRenderOverride.bits.FORCE_HIS_ENABLE1 = settings.hiStencilEnable ? FORCE_OFF : FORCE_DISABLE;

    if (internalInfo.flags.u32All != 0)
    {
        // DB_RENDER_OVERRIDE cannot change at bind-time due to compression states for internal blit types.
        m_flags.dbRenderOverrideLocked = 1;
    }

    if (isResummarize)
    {
        m_regs.dbRenderOverride.bits.FORCE_Z_VALID           = !zReadOnly;
        m_regs.dbRenderOverride.bits.FORCE_STENCIL_VALID     = !sReadOnly;
        m_regs.dbRenderOverride.bits.NOOP_CULL_DISABLE       = 1;
        m_regs.dbRenderOverride.bits.DISABLE_TILE_RATE_TILES = 1;
    }
}

// =====================================================================================================================
// Updates the specified PM4 image with the virtual addresses of the image and the image's various metadata addresses.
void DepthStencilView::UpdateImageVa(
    DepthStencilViewRegs* pRegs
    ) const
{
    // The "GetSubresource256BAddrSwizzled" function will crash if no memory has been bound to
    // the associated image yet, so don't do anything if it's not safe
    if (m_pImage->Parent()->GetBoundGpuMemory().IsBound())
    {
        if (m_flags.hTile)
        {
            pRegs->fastClearMetadataGpuVa = m_pImage->FastClearMetaDataAddr(m_depthSubresource);
            PAL_ASSERT((pRegs->fastClearMetadataGpuVa & 0x3) == 0);

            pRegs->dbHtileDataBase.bits.BASE_256B = m_pImage->GetHtile256BAddr(m_depthSubresource);
        }

        if (m_flags.hiSPretests)
        {
            pRegs->hiSPretestMetadataGpuVa = m_pImage->HiSPretestsMetaDataAddr(MipLevel());
            PAL_ASSERT((pRegs->hiSPretestMetadataGpuVa & 0x3) == 0);
        }

        if (m_flags.depth)
        {
            const uint32 gpuVirtAddr = m_pImage->GetSubresource256BAddrSwizzled(m_depthSubresource);

            pRegs->dbZReadBase.u32All  = gpuVirtAddr;
            pRegs->dbZWriteBase.u32All = gpuVirtAddr;
        }

        if (m_flags.stencil)
        {
            const uint32 gpuVirtAddr = m_pImage->GetSubresource256BAddrSwizzled(m_stencilSubresource);

            pRegs->dbStencilReadBase.u32All  = gpuVirtAddr;
            pRegs->dbStencilWriteBase.u32All = gpuVirtAddr;

            pRegs->coherDestBase0.bits.DEST_BASE_256B = pRegs->dbStencilWriteBase.bits.BASE_256B;
        }
    }
}

// =====================================================================================================================
// Writes the PM4 commands required to bind to depth/stencil slot. Returns the next unused DWORD in pCmdSpace.
uint32* DepthStencilView::WriteCommands(
    ImageLayout depthLayout,
    ImageLayout stencilLayout,
    CmdStream*  pCmdStream,
    uint32*     pCmdSpace
    ) const
{
    const DepthStencilCompressionState depthState =
            ImageLayoutToDepthCompressionState(m_depthLayoutToState, depthLayout);
    const DepthStencilCompressionState stencilState =
            ImageLayoutToDepthCompressionState(m_stencilLayoutToState, stencilLayout);

    DepthStencilViewRegs regs = m_regs;

    if ((m_flags.viewVaLocked == 0) && m_pImage->Parent()->GetBoundGpuMemory().IsBound())
    {
        UpdateImageVa(&regs);
    }

    if ((stencilLayout.usages == 0) &&
        ((depthLayout.usages & LayoutDepthStencilTarget) != 0))
    {
        regs.dbStencilInfo.bits.FORMAT = 0;
    }

    if ((depthLayout.usages == 0) &&
        ((stencilLayout.usages & LayoutDepthStencilTarget) != 0))
    {
        regs.dbZInfo.bits.FORMAT = 0;
    }

    if ((depthState != DepthStencilCompressed) || (stencilState != DepthStencilCompressed))
    {
        // For decompressed rendering to an Image, we need to override the values of DB_DEPTH_CONTROL and
        // DB_RENDER_OVERRIDE, depending on the compression states for depth and stencil.
        if (m_flags.dbRenderControlLocked == 0)
        {
            regs.dbRenderControl.bits.DEPTH_COMPRESS_DISABLE   = (depthState   != DepthStencilCompressed);
            regs.dbRenderControl.bits.STENCIL_COMPRESS_DISABLE = (stencilState != DepthStencilCompressed);
        }
        if (m_flags.dbRenderOverrideLocked == 0)
        {
            if (depthState == DepthStencilDecomprNoHiZ)
            {
                regs.dbRenderOverride.bits.FORCE_HIZ_ENABLE = FORCE_DISABLE;
            }
            if (stencilState == DepthStencilDecomprNoHiZ)
            {
                regs.dbRenderOverride.bits.FORCE_HIS_ENABLE0 = FORCE_DISABLE;
                regs.dbRenderOverride.bits.FORCE_HIS_ENABLE1 = FORCE_DISABLE;
            }
        }
    }

    if ((depthState == DepthStencilCompressed) || (stencilState == DepthStencilCompressed))
    {
        if (regs.fastClearMetadataGpuVa != 0)
        {
            // Load the context registers which store the fast-clear value(s) from GPU memory.
            constexpr uint32 RegisterCount = (mmDB_DEPTH_CLEAR - mmDB_STENCIL_CLEAR + 1);
            pCmdSpace = pCmdStream->WriteLoadSeqContextRegs((m_flags.usesLoadRegIndexPkt != 0),
                                                            mmDB_STENCIL_CLEAR,
                                                            RegisterCount,
                                                            regs.fastClearMetadataGpuVa,
                                                            pCmdSpace);
        }

        if (regs.hiSPretestMetadataGpuVa != 0)
        {
            // During the client is binding depth stencil target, we load the pretests meta data, which we expect is
            // initialized by ClearHiSPretestsMetaData and later set by CmdUpdateHiSPretests, into a paired of DB
            // context registers which are used to store the HiStencil pretests.
            constexpr uint32 RegisterCount = (mmDB_SRESULTS_COMPARE_STATE1 - mmDB_SRESULTS_COMPARE_STATE0 + 1);
            pCmdSpace = pCmdStream->WriteLoadSeqContextRegs((m_flags.usesLoadRegIndexPkt != 0),
                                                            mmDB_SRESULTS_COMPARE_STATE0,
                                                            RegisterCount,
                                                            regs.hiSPretestMetadataGpuVa,
                                                            pCmdSpace);
        }
    }

    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmDB_DEPTH_INFO, mmDB_DEPTH_SLICE, &regs.dbDepthInfo, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_DEPTH_VIEW, regs.dbDepthView.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmDB_RENDER_OVERRIDE2,
                                                   mmDB_HTILE_DATA_BASE,
                                                   &regs.dbRenderOverride2,
                                                   pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_HTILE_SURFACE, regs.dbHtileSurface.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_PRELOAD_CONTROL, regs.dbPreloadControl.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_RENDER_CONTROL, regs.dbRenderControl.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmPA_SU_POLY_OFFSET_DB_FMT_CNTL,
                                                  regs.paSuPolyOffsetDbFmtCntl.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmCOHER_DEST_BASE_0, regs.coherDestBase0.u32All, pCmdSpace);
    return pCmdStream->WriteContextRegRmw(mmDB_RENDER_OVERRIDE,
                                          DbRenderOverrideRmwMask,
                                          regs.dbRenderOverride.u32All,
                                          pCmdSpace);
}

// =====================================================================================================================
// Determines the proper value of the DB_Z_INFO.DECOMPRESS_ON_N_ZPLANES register value
uint32 DepthStencilView::CalcDecompressOnZPlanesValue(
    bool depthCompressDisable // DEPTH_COMPRESS_DISABLE of DB_RENDER_CONTROL
    ) const
{
    const ImageCreateInfo&  createInfo = m_pImage->Parent()->GetImageCreateInfo();
    const ChNumFormat       format     = createInfo.swizzledFormat.format;

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
    if (m_flags.isExpand && depthCompressDisable && m_device.WaDbDecompressPerformance())
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
            const SubResourceInfo*const pOldSubResInfo = pOldImage->Parent()->SubresourceInfo(0);
            const bool                  oldIsTcCompat  = pOldSubResInfo->flags.supportMetaDataTexFetch;

            const Image*const           pNewImage      = pNewView->GetImage();
            const SubResourceInfo*const pNewSubResInfo = pNewImage->Parent()->SubresourceInfo(0);
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
    struct
    {
        regDB_STENCIL_CLEAR dbStencilClear;
        regDB_DEPTH_CLEAR   dbDepthClear;
    } regs; // Intentionally not initialized.

    regs.dbDepthClear.f32All       = depth;
    regs.dbStencilClear.u32All     = 0;
    regs.dbStencilClear.bits.CLEAR = stencil;

    if (metaDataClearFlags == (HtilePlaneDepth | HtilePlaneStencil))
    {
        pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmDB_STENCIL_CLEAR, mmDB_DEPTH_CLEAR, &regs, pCmdSpace);
    }
    else if (metaDataClearFlags == HtilePlaneDepth)
    {
        pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_DEPTH_CLEAR, regs.dbDepthClear.u32All, pCmdSpace);
    }
    else
    {
        PAL_ASSERT(metaDataClearFlags == HtilePlaneStencil);

        pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_STENCIL_CLEAR, regs.dbStencilClear.u32All, pCmdSpace);
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

            // Build a COND_EXEC to check the workaround metadata. If the last clear value was 0.0f, the metadata will
            // be non-zero and the register will be re-written, otherwise the metadata will be 0 and the register
            // write will be skipped.
            pCmdSpace += cmdUtil.BuildCondExec(metaDataVirtAddr, setContextRegSize, pCmdSpace);
        }

        // DB_Z_INFO is the same for all compression states
        regDB_Z_INFO dbZInfo = m_regs.dbZInfo;
        dbZInfo.bits.ZRANGE_PRECISION = 0;

        pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_Z_INFO, dbZInfo.u32All, pCmdSpace);
    }

    return pCmdSpace;
}

} // Gfx6
} // Pal
