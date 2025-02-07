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

#include "core/image.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9DepthStencilView.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "core/hw/gfxip/gfx9/gfx9MaskRam.h"
#include "core/addrMgr/addrMgr2/addrMgr2.h"
#include <type_traits>

using namespace Util;
using namespace Pal::Formats::Gfx9;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
DepthStencilView::DepthStencilView(
    const Device*                             pDevice,
    const DepthStencilViewCreateInfo&         createInfo,
    const DepthStencilViewInternalCreateInfo& internalInfo,
    uint32                                    uniqueId)
    :
    m_pImage(GetGfx9Image(createInfo.pImage)),
    m_uniqueId(uniqueId),
    m_baseArraySlice(createInfo.baseArraySlice),
    m_arraySize(createInfo.arraySize),
    m_regs{}
{
    PAL_ASSERT(createInfo.pImage != nullptr);
    const auto& imageInfo  = m_pImage->Parent()->GetImageCreateInfo();
    const auto& parent     = *pDevice->Parent();
    const auto& settings   = GetGfx9Settings(parent);
    const bool  hasDepth   = parent.SupportsDepth(imageInfo.swizzledFormat.format, imageInfo.tiling);
    const bool  hasStencil = parent.SupportsStencil(imageInfo.swizzledFormat.format, imageInfo.tiling);

    m_flags.u32All = 0;

    if ((settings.waRestrictMetaDataUseInMipTail == false) ||
        m_pImage->CanMipSupportMetaData(createInfo.mipLevel))
    {
        m_flags.hTile = m_pImage->HasHtileData();
    }

    m_hTileUsage.value = 0;
    if (m_pImage->HasHtileData())
    {
        m_hTileUsage = m_pImage->GetHtile()->GetHtileUsage();
    }

    m_flags.hiSPretests     = m_pImage->HasHiSPretestsMetaData();
    m_flags.depth           = (createInfo.flags.stencilOnlyView == 0) && hasDepth;
    m_flags.stencil         = (createInfo.flags.depthOnlyView == 0) && hasStencil;
    m_flags.readOnlyDepth   = createInfo.flags.readOnlyDepth;
    m_flags.readOnlyStencil = createInfo.flags.readOnlyStencil;
    m_flags.viewVaLocked    = createInfo.flags.imageVaLocked;
    m_flags.vrsOnlyDepth    = m_pImage->Parent()->GetImageInfo().internalCreateInfo.flags.vrsOnlyDepth;

    //  we need to set both depthSubresource and stencilSubresource correctly when creating:
    //  1. depth-stencil view,
    //  2. stencil-only view with compatiable depth format,
    //  3. depth-only view with compatiable stencil format.
    if (hasDepth && hasStencil)
    {
        // Have both depth and stencil planes.
        m_depthSubresource.plane        = 0;
        m_depthSubresource.mipLevel     = createInfo.mipLevel;
        m_depthSubresource.arraySlice   = 0;
        m_stencilSubresource.plane      = 1;
        m_stencilSubresource.mipLevel   = createInfo.mipLevel;
        m_stencilSubresource.arraySlice = 0;
    }
    else if (hasDepth)
    {
        // Only have depth plane.
        m_depthSubresource.plane      = 0;
        m_depthSubresource.mipLevel   = createInfo.mipLevel;
        m_depthSubresource.arraySlice = 0;
        m_stencilSubresource          = m_depthSubresource;
    }
    else
    {
        // Only have stencil plane.
        m_stencilSubresource.plane      = 0;
        m_stencilSubresource.mipLevel   = createInfo.mipLevel;
        m_stencilSubresource.arraySlice = 0;
        m_depthSubresource              = m_stencilSubresource;
    }

    m_depthLayoutToState   = m_pImage->LayoutToDepthCompressionState(m_depthSubresource);
    m_stencilLayoutToState = m_pImage->LayoutToDepthCompressionState(m_stencilSubresource);

    if (m_flags.hTile != 0)
    {
        const SubResourceInfo*const pDepthSubResInfo   = m_pImage->Parent()->SubresourceInfo(m_depthSubresource);
        const SubResourceInfo*const pStencilSubResInfo = m_pImage->Parent()->SubresourceInfo(m_stencilSubresource);

        m_flags.depthMetadataTexFetch   = pDepthSubResInfo->flags.supportMetaDataTexFetch;
        m_flags.stencilMetadataTexFetch = pStencilSubResInfo->flags.supportMetaDataTexFetch;
    }

    InitRegisters(*pDevice, createInfo, internalInfo);

    if (IsVaLocked())
    {
        UpdateImageVa(&m_regs);
    }
}

// =====================================================================================================================
// Updates the specified PM4 image with the virtual addresses of the image and the image's various metadata addresses.
void DepthStencilView::InitRegistersCommon(
    const Device&                             device,
    const DepthStencilViewCreateInfo&         createInfo,
    const DepthStencilViewInternalCreateInfo& internalInfo,
    const MergedFlatFmtInfo*                  pFmtInfo,
    DepthStencilViewRegs*                     pRegs)
{
    const SubResourceInfo*const pDepthSubResInfo     = m_pImage->Parent()->SubresourceInfo(m_depthSubresource);
    const SubResourceInfo*const pStencilSubResInfo   = m_pImage->Parent()->SubresourceInfo(m_stencilSubresource);
    const Gfx9PalSettings&      settings             = device.Settings();
    const bool                  zReadOnly            = (createInfo.flags.readOnlyDepth != 0);
    const bool                  sReadOnly            = (createInfo.flags.readOnlyStencil != 0);
    const auto&                 imageCreateInfo      = m_pImage->Parent()->GetImageCreateInfo();
    const ChNumFormat           zFmt                 = pDepthSubResInfo->format.format;
    const ChNumFormat           sFmt                 = pStencilSubResInfo->format.format;
    const ZFormat               hwZFmt               = HwZFmt(pFmtInfo, zFmt);
    const bool                  isResummarize        = (createInfo.flags.resummarizeHiZ != 0);

    if (m_flags.hTile)
    {
        const Gfx9Htile*const pHtile = m_pImage->GetHtile();

        // Tell the HW that HTILE metadata is present.
        pRegs->dbZInfo.bits.ZRANGE_PRECISION = pHtile->ZRangePrecision();

        // Don't tie this to the hTileUsage.dsMetadata flag!  TILE_SURFACE_ENABLE needs to remain set
        // even for VRS-only hTile data as otherwise the HW won't try to actually read the hTile data.
        pRegs->dbZInfo.bits.TILE_SURFACE_ENABLE        = 1;
        pRegs->dbStencilInfo.bits.TILE_STENCIL_DISABLE = pHtile->TileStencilDisabled();

        if (internalInfo.flags.isExpand | internalInfo.flags.isDepthCopy | internalInfo.flags.isStencilCopy)
        {
            pRegs->dbRenderControl.bits.DEPTH_COMPRESS_DISABLE   = !zReadOnly;
            pRegs->dbRenderControl.bits.STENCIL_COMPRESS_DISABLE = !sReadOnly;

            m_flags.dbRenderControlLocked = 1; // This cannot change at bind-time for expands and copies!
        }

        if (isResummarize)
        {
            pRegs->dbRenderControl.bits.RESUMMARIZE_ENABLE = 1;
        }

        pRegs->dbZInfo.bits.ALLOW_EXPCLEAR       =
                (imageCreateInfo.usageFlags.shaderRead == 1) ? settings.dbPerTileExpClearEnable : 0;
        pRegs->dbStencilInfo.bits.ALLOW_EXPCLEAR =
                (imageCreateInfo.usageFlags.shaderRead == 1) ? settings.dbPerTileExpClearEnable : 0;

        pRegs->dbHtileSurface.u32All   = pHtile->DbHtileSurface(m_depthSubresource.mipLevel).u32All;

        if (m_flags.depthMetadataTexFetch)
        {
            // This image might get texture-fetched, so setup any register info specific to texture fetches here.
            pRegs->dbZInfo.bits.DECOMPRESS_ON_N_ZPLANES = CalcDecompressOnZPlanesValue(device, hwZFmt);
        }
    }
    else
    {
        // Tell the HW that HTILE metadata is not present.
        pRegs->dbZInfo.bits.TILE_SURFACE_ENABLE              = 0;
        pRegs->dbStencilInfo.bits.TILE_STENCIL_DISABLE       = 1;
        pRegs->dbRenderControl.bits.DEPTH_COMPRESS_DISABLE   = 1;
        pRegs->dbRenderControl.bits.STENCIL_COMPRESS_DISABLE = 1;
    }

    // Setup DB_DEPTH_VIEW.
    pRegs->dbDepthView.bits.SLICE_START       = createInfo.baseArraySlice;
    pRegs->dbDepthView.bits.SLICE_MAX         = (createInfo.arraySize + createInfo.baseArraySlice - 1);
    pRegs->dbDepthView.bits.Z_READ_ONLY       = (zReadOnly ? 1 : 0);
    pRegs->dbDepthView.bits.STENCIL_READ_ONLY = (sReadOnly ? 1 : 0);
    pRegs->dbDepthView.bits.MIPID             = createInfo.mipLevel;

    // Set clear enable fields if the create info indicates the view should be a fast clear view
    pRegs->dbRenderControl.bits.DEPTH_CLEAR_ENABLE   = internalInfo.flags.isDepthClear;
    pRegs->dbRenderControl.bits.STENCIL_CLEAR_ENABLE = internalInfo.flags.isStencilClear;
    pRegs->dbRenderControl.bits.DEPTH_COPY           = internalInfo.flags.isDepthCopy;
    pRegs->dbRenderControl.bits.STENCIL_COPY         = internalInfo.flags.isStencilCopy;

    if (internalInfo.flags.isDepthCopy | internalInfo.flags.isStencilCopy)
    {
        pRegs->dbRenderControl.bits.COPY_SAMPLE   = 0;
        pRegs->dbRenderControl.bits.COPY_CENTROID = 1;
    }

    // Enable HiZ/HiS based on settings
    pRegs->dbRenderOverride.bits.FORCE_HIZ_ENABLE  = settings.hiDepthEnable   ? FORCE_OFF : FORCE_DISABLE;
    pRegs->dbRenderOverride.bits.FORCE_HIS_ENABLE0 = settings.hiStencilEnable ? FORCE_OFF : FORCE_DISABLE;
    pRegs->dbRenderOverride.bits.FORCE_HIS_ENABLE1 = settings.hiStencilEnable ? FORCE_OFF : FORCE_DISABLE;

    if (internalInfo.flags.u32All != 0)
    {
        // DB_RENDER_OVERRIDE cannot change at bind-time due to compression states for internal blit types.
        m_flags.dbRenderOverrideLocked = 1;
    }

    if (isResummarize)
    {
        pRegs->dbRenderOverride.bits.FORCE_Z_VALID           = !zReadOnly;
        pRegs->dbRenderOverride.bits.FORCE_STENCIL_VALID     = !sReadOnly;
        pRegs->dbRenderOverride.bits.NOOP_CULL_DISABLE       = 1;
        pRegs->dbRenderOverride.bits.DISABLE_TILE_RATE_TILES = 1;
    }

    m_extent.width  = pDepthSubResInfo->extentTexels.width;
    m_extent.height = pDepthSubResInfo->extentTexels.height;

    pRegs->dbZInfo.bits.READ_SIZE          = settings.dbRequestSize;
    pRegs->dbZInfo.bits.NUM_SAMPLES        = Log2(imageCreateInfo.samples);
    pRegs->dbZInfo.bits.MAXMIP             = (imageCreateInfo.mipLevels - 1);
    pRegs->dbZInfo.bits.PARTIALLY_RESIDENT = imageCreateInfo.flags.prt;
    pRegs->dbZInfo.bits.FORMAT             = hwZFmt;

    pRegs->dbStencilInfo.bits.FORMAT             = HwStencilFmt(pFmtInfo, sFmt);
    pRegs->dbStencilInfo.bits.PARTIALLY_RESIDENT = pRegs->dbZInfo.bits.PARTIALLY_RESIDENT;

    // For 4xAA and 8xAA need to decompress on flush for better performance
    pRegs->dbRenderOverride2.bits.DECOMPRESS_Z_ON_FLUSH       = (imageCreateInfo.samples > 2) ? 1 : 0;
    pRegs->dbRenderOverride2.bits.DISABLE_COLOR_ON_VALIDATION = settings.dbDisableColorOnValidation;

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
        const bool depthAsZ24 =
            static_cast<bool>(createInfo.flags.useHwFmtforDepthOffset ? 0 : imageCreateInfo.usageFlags.depthAsZ24);

        // NOTE: The client has indicated this Image has promoted 24bit depth to 32bits, we should set the negative num
        //       bit as -24 and use fixed points format
        const bool reducePrecision = createInfo.flags.lowZplanePolyOffsetBits;

        if (depthAsZ24)
        {
            pRegs->paSuPolyOffsetDbFmtCntl.bits.POLY_OFFSET_NEG_NUM_DB_BITS = uint8(reducePrecision ?  -22 : -24);
        }
        else if (pRegs->dbZInfo.bits.FORMAT == Z_16)
        {
            pRegs->paSuPolyOffsetDbFmtCntl.bits.POLY_OFFSET_NEG_NUM_DB_BITS = uint8(reducePrecision ?  -15 : -16);
        }
        else
        {
            pRegs->paSuPolyOffsetDbFmtCntl.bits.POLY_OFFSET_NEG_NUM_DB_BITS = uint8(-23);
        }

        pRegs->paSuPolyOffsetDbFmtCntl.bits.POLY_OFFSET_DB_IS_FLOAT_FMT =
            ((pRegs->dbZInfo.bits.FORMAT == Z_32_FLOAT) && (depthAsZ24 == false)) ? 1 : 0;
    }
    else
    {
        pRegs->paSuPolyOffsetDbFmtCntl.u32All = 0;
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
        // Setup bits indicating the page size.
        const bool isBigPage = IsImageBigPageCompatible(*m_pImage, Gfx10AllowBigPageDepthStencil);

        auto*const pGfx10 = reinterpret_cast<DepthStencilViewRegs*>(pRegs);
        pGfx10->dbRmiL2CacheControl.bits.Z_BIG_PAGE = isBigPage;
        pGfx10->dbRmiL2CacheControl.bits.S_BIG_PAGE = isBigPage;

        gpusize zReadBase        = m_pImage->GetSubresourceAddr(m_depthSubresource);
        gpusize zWriteBase       = zReadBase;
        gpusize stencilReadBase  = m_pImage->GetSubresourceAddr(m_stencilSubresource);
        gpusize stencilWriteBase = stencilReadBase;

        if (m_flags.hTile)
        {
            if (m_hTileUsage.dsMetadata != 0)
            {
                // Program fast-clear metadata base address.  Plane shouldn't matter here, just the mip level
                pRegs->fastClearMetadataGpuVa = m_pImage->FastClearMetaDataAddr(m_depthSubresource);
                PAL_ASSERT((pRegs->fastClearMetadataGpuVa & 0x3) == 0);
            }
            const gpusize htile256BAddrSwizzled   = m_pImage->GetHtile256BAddrSwizzled();
            pRegs->dbHtileDataBase.bits.BASE_256B = LowPart(htile256BAddrSwizzled);
            pRegs->dbHtileDataBaseHi.bits.BASE_HI = HighPart(htile256BAddrSwizzled);

            // If this image was created without Z or stencil data, then the HW requires that the Z or
            // stencil base addresses are still set.  With depth-testing disabled, the HW should never
            // fetch from these addresses as if it were Z or stencil data.  Ensure that the stencil read
            // base and hTile base are the same thing.
            if (m_flags.vrsOnlyDepth != 0)
            {
                PAL_ASSERT(m_hTileUsage.vrs != 0);

                zReadBase        = pRegs->dbHtileDataBase.bits.BASE_256B;
                stencilReadBase  = zReadBase;

                // As we don't have real image data, make doubly sure nobody tries to write to these
                // non-existent buffers.  Attempting to do so would overwrite VRS data stored in htile.
                zWriteBase       = 0;
                stencilWriteBase = 0;
            }
        }

        if (m_flags.hiSPretests)
        {
            pRegs->hiSPretestMetadataGpuVa = m_pImage->HiSPretestsMetaDataAddr(MipLevel());
            PAL_ASSERT((pRegs->hiSPretestMetadataGpuVa & 0x3) == 0);
        }

        if (m_flags.depth)
        {
            pRegs->dbZReadBase.u32All    = Get256BAddrLo(zReadBase);
            pRegs->dbZWriteBase.u32All   = Get256BAddrLo(zWriteBase);
            pRegs->dbZReadBaseHi.u32All  = Get256BAddrHi(zReadBase);
            pRegs->dbZWriteBaseHi.u32All = Get256BAddrHi(zWriteBase);
        }

        if (m_flags.stencil)
        {
            pRegs->dbStencilReadBase.u32All    = Get256BAddrLo(stencilReadBase);
            pRegs->dbStencilWriteBase.u32All   = Get256BAddrLo(stencilWriteBase);
            pRegs->dbStencilReadBaseHi.u32All  = Get256BAddrHi(stencilReadBase);
            pRegs->dbStencilWriteBaseHi.u32All = Get256BAddrHi(stencilWriteBase);

            pRegs->coherDestBase0.bits.DEST_BASE_256B = pRegs->dbStencilWriteBase.bits.BASE_256B;
        }
    }
}

// =====================================================================================================================
// Writes the PM4 commands required to bind to depth/stencil slot (common to both Gfx9 and Gfx10).  Returns the next
// unused DWORD in pCmdSpace.
uint32* DepthStencilView::WriteCommandsCommon(
    ImageLayout           depthLayout,
    ImageLayout           stencilLayout,
    CmdStream*            pCmdStream,
    uint32*               pCmdSpace,
    DepthStencilViewRegs* pRegs
    ) const
{
    const DepthStencilCompressionState depthState =
            ImageLayoutToDepthCompressionState(m_depthLayoutToState, depthLayout);
    const DepthStencilCompressionState stencilState =
            ImageLayoutToDepthCompressionState(m_stencilLayoutToState, stencilLayout);

    if ((m_flags.viewVaLocked == 0) && m_pImage->Parent()->GetBoundGpuMemory().IsBound())
    {
        UpdateImageVa(pRegs);
    }

    if ((stencilLayout.usages == 0) &&
        ((depthLayout.usages & LayoutDepthStencilTarget) != 0))
    {
        pRegs->dbStencilInfo.bits.FORMAT = 0;
    }

    if ((depthLayout.usages == 0) &&
        ((stencilLayout.usages & LayoutDepthStencilTarget) != 0))
    {
        pRegs->dbZInfo.bits.FORMAT = 0;
    }

    if ((depthState != DepthStencilCompressed) || (stencilState != DepthStencilCompressed))
    {
        // For decompressed rendering to an Image, we need to override the values of DB_DEPTH_CONTROL and
        // DB_RENDER_OVERRIDE, depending on the compression states for depth and stencil.
        if ((m_flags.dbRenderControlLocked == 0) &&
            // If this is an hTile only VRS-storage surface then don't set the COMPRESS_DISABLE flags.
            (m_flags.vrsOnlyDepth == 0))
        {
            pRegs->dbRenderControl.bits.DEPTH_COMPRESS_DISABLE   = (depthState   != DepthStencilCompressed);
            pRegs->dbRenderControl.bits.STENCIL_COMPRESS_DISABLE = (stencilState != DepthStencilCompressed);
        }
        if (m_flags.dbRenderOverrideLocked == 0)
        {
            if (depthState == DepthStencilDecomprNoHiZ)
            {
                pRegs->dbRenderOverride.bits.FORCE_HIZ_ENABLE = FORCE_DISABLE;
            }
            if (stencilState == DepthStencilDecomprNoHiZ)
            {
                pRegs->dbRenderOverride.bits.FORCE_HIS_ENABLE0 = FORCE_DISABLE;
                pRegs->dbRenderOverride.bits.FORCE_HIS_ENABLE1 = FORCE_DISABLE;
            }
        }
    }

    if ((depthState == DepthStencilCompressed) || (stencilState == DepthStencilCompressed))
    {
        if (pRegs->fastClearMetadataGpuVa != 0)
        {
            // Load the context registers which store the fast-clear value(s) from GPU memory.
            constexpr uint32 RegisterCount = (mmDB_DEPTH_CLEAR - mmDB_STENCIL_CLEAR + 1);
            pCmdSpace = pCmdStream->WriteLoadSeqContextRegs(mmDB_STENCIL_CLEAR,
                                                            RegisterCount,
                                                            pRegs->fastClearMetadataGpuVa,
                                                            pCmdSpace);
        }
    }

    if (pRegs->hiSPretestMetadataGpuVa != 0)
    {
        // During the client is binding depth stencil target, we load the pretests meta data, which we expect is
        // initialized by ClearHiSPretestsMetaData and later set by CmdUpdateHiSPretests, into a paired of DB
        // context registers which are used to store the HiStencil pretests.
        constexpr uint32 RegisterCount = (mmDB_SRESULTS_COMPARE_STATE1 - mmDB_SRESULTS_COMPARE_STATE0 + 1);
        pCmdSpace = pCmdStream->WriteLoadSeqContextRegs(mmDB_SRESULTS_COMPARE_STATE0,
                                                        RegisterCount,
                                                        pRegs->hiSPretestMetadataGpuVa,
                                                        pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Determines the proper value of the DB_Z_INFO.DECOMPRESS_ON_N_ZPLANES register value
uint32 DepthStencilView::CalcDecompressOnZPlanesValue(
    const Device& device,
    ZFormat       hwZFmt
    ) const
{
    const Pal::Image*const pParent    = m_pImage->Parent();
    const ImageCreateInfo& createInfo = pParent->GetImageCreateInfo();

    //   fmt  1xAA  2xAA   4xAA  8xAA
    //   Z16    4     2      2     2
    //  Z32f    4     4      4     4
    //
    uint32  decompressOnZPlanes = 4;
    switch (hwZFmt)
    {
    case Z_16:
        if (createInfo.samples > 1)
        {
            decompressOnZPlanes = 2;
        }
        break;

    case Z_32_FLOAT:
        // Default value of 4 is correct, don't assert below
        break;

    case Z_INVALID:
        // Must be a stencil-only format.
        break;

    default:
        // What is this?
        PAL_ASSERT_ALWAYS();
        break;
    }

    // Check this first to eliminate products that don't have an ITERATE_256 bit in the first place.  Calling
    // the GetIterate256() function below causes asserts on non-GFX10 products.
    if (device.Settings().waTwoPlanesIterate256)
    {
        const SubResourceInfo*const pDepthSubResInfo   = pParent->SubresourceInfo(m_depthSubresource);
        const SubResourceInfo*const pStencilSubResInfo = pParent->SubresourceInfo(m_stencilSubresource);

        const bool isIterate256 = (m_pImage->GetIterate256(pDepthSubResInfo) == 1) ||
                                  (m_pImage->GetIterate256(pStencilSubResInfo) == 1);

        if (isIterate256                            &&
            (m_pImage->IsHtileDepthOnly() == false) &&
            (createInfo.samples == 4))
        {
            decompressOnZPlanes = 1;
        }
    }

    PAL_ASSERT((decompressOnZPlanes != 0) || (IsGfx10(*(device.Parent())) == false));

    return decompressOnZPlanes + 1;
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
        regDB_STENCIL_CLEAR  dbStencilClear;
        regDB_DEPTH_CLEAR    dbDepthClear;
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
// Helper function which adds commands into the command stream when the currently-bound depth target is changing.
// Returns the address to where future commands will be written.
uint32* DepthStencilView::HandleBoundTargetChanged(
    const Pal::UniversalCmdBuffer* pCmdBuffer,
    uint32*                        pCmdSpace
    ) const
{
    const Pal::Device&  palDevice  = *(m_pImage->Parent()->GetDevice());
    const Device*       pGfxDevice = static_cast<Device*>(palDevice.GetGfxDevice());
    const auto&         cmdUtil    = pGfxDevice->CmdUtil();

    // If you change the mips of a resource being used as a depth/stencil target, we need to flush the DB metadata
    // cache. This protects against the case where an Htile cacheline can contain data from two different mip levels
    // in different RB's.
    const size_t packetSize = cmdUtil.BuildNonSampleEventWrite(FLUSH_AND_INV_DB_META, EngineTypeUniversal, pCmdSpace);

    return (pCmdSpace + packetSize);
}

// =====================================================================================================================
bool DepthStencilView::Equals(
    const DepthStencilView* pOther) const
{
    return ((pOther != nullptr) && (m_uniqueId == pOther->m_uniqueId));
}

// =====================================================================================================================
// Helper to initialize the GFX11 DB_RENDER_CONTROL reg based on panel settings and necessary workarounds.
void DepthStencilView::SetGfx11StaticDbRenderControlFields(
    const Device&         device,
    const uint8           numFragments,
    regDB_RENDER_CONTROL* pDbRenderControl)
{
    const Pal::Device&     palDevice  = *device.Parent();
    const Gfx9PalSettings& settings   = device.Settings();

    // The user mode driver should generally set the OREO_MODE field to OPAQUE_THEN_BLEND for best performance.
    // Setting to BLEND is a fail-safe that should work for all cases

    pDbRenderControl->gfx11.OREO_MODE          = settings.gfx11OreoModeControl;

    // The FORCE_OREO_MODE is intended only for workarounds and should otherwise be set to 0
    pDbRenderControl->gfx11.FORCE_OREO_MODE    = settings.gfx11ForceOreoMode ? 1 : 0;

    pDbRenderControl->gfx11.FORCE_EXPORT_ORDER = settings.gfx11ForceExportOrderControl ? 1 : 0;

    if (IsNavi3x(palDevice))
    {
        switch (numFragments)
        {
        case 8:
            pDbRenderControl->gfx11.MAX_ALLOWED_TILES_IN_WAVE =
                (settings.waIncorrectMaxAllowedTilesInWave) ? 6 : 7;
            break;
        case 4:
            pDbRenderControl->gfx11.MAX_ALLOWED_TILES_IN_WAVE =
                (settings.waIncorrectMaxAllowedTilesInWave) ? 13 : 14;
            break;
        default:
            pDbRenderControl->gfx11.MAX_ALLOWED_TILES_IN_WAVE = 0;
            break;
        }
    }
    else if (IsPhoenixFamily(palDevice)
#if PAL_BUILD_STRIX
             || IsStrixFamily(palDevice)
#endif
        )
    {
        switch (numFragments)
        {
        case 8:
            pDbRenderControl->gfx11.MAX_ALLOWED_TILES_IN_WAVE =
                (settings.waIncorrectMaxAllowedTilesInWave) ? 7 : 8;
            break;
        case 4:
            pDbRenderControl->gfx11.MAX_ALLOWED_TILES_IN_WAVE =
                (settings.waIncorrectMaxAllowedTilesInWave) ? 15 : 0;
            break;
        default:
            pDbRenderControl->gfx11.MAX_ALLOWED_TILES_IN_WAVE = 0;
            break;
        }
    }
}

// =====================================================================================================================
// Finalizes the PM4 packet image by setting up the register values used to write this View object to hardware.
void DepthStencilView::InitRegisters(
    const Device&                             device,
    const DepthStencilViewCreateInfo&         createInfo,
    const DepthStencilViewInternalCreateInfo& internalInfo)
{
    const Pal::Image*const pParentImg = m_pImage->Parent();
    const Pal::Device&     palDevice  = *device.Parent();
    const auto*            pAddrMgr   = static_cast<const AddrMgr2::AddrMgr2*>(palDevice.GetAddrMgr());
    const GfxIpLevel       gfxLevel   = palDevice.ChipProperties().gfxLevel;
    const Gfx9PalSettings& settings   = device.Settings();

    const MergedFlatFmtInfo*const pFmtInfo =
        MergedChannelFlatFmtInfoTbl(gfxLevel, &device.GetPlatform()->PlatformSettings());

    const SubResourceInfo*const pDepthSubResInfo     = pParentImg->SubresourceInfo(m_depthSubresource);
    const SubResourceInfo*const pStencilSubResInfo   = pParentImg->SubresourceInfo(m_stencilSubresource);
    const SubresId              baseDepthSubResId    = { m_depthSubresource.plane, 0 , 0 };
    const SubResourceInfo*const pBaseDepthSubResInfo = pParentImg->SubresourceInfo(baseDepthSubResId);

    InitRegistersCommon(device, createInfo, internalInfo, pFmtInfo, &m_regs);

    // GFX10 adds extra bits to the slice selection...  the "hi" bits are in different fields from the "low" bits.
    // The "low" bits were set in "InitCommonImageView", take care of the new high bits here.
    const uint32 sliceMax = (createInfo.arraySize + createInfo.baseArraySlice - 1);

    // Setup the size
    m_regs.dbDepthSizeXy.bits.X_MAX = (pBaseDepthSubResInfo->extentTexels.width  - 1);
    m_regs.dbDepthSizeXy.bits.Y_MAX = (pBaseDepthSubResInfo->extentTexels.height - 1);

    // From the reg-spec:  Indicates that compressed data must be iterated on flush every pipe interleave bytes in
    //                     order to be readable by TC
    m_regs.dbZInfo.bits.ITERATE_FLUSH       = m_flags.depthMetadataTexFetch;
    m_regs.dbStencilInfo.bits.ITERATE_FLUSH = m_flags.stencilMetadataTexFetch;

    const auto& depthAddrSettings   = m_pImage->GetAddrSettings(pDepthSubResInfo);
    const auto& stencilAddrSettings = m_pImage->GetAddrSettings(pStencilSubResInfo);

    if (depthAddrSettings.swizzleMode != ADDR_SW_64KB_Z_X)
    {
        PAL_ASSERT(IsGfx103Plus(palDevice) && (depthAddrSettings.swizzleMode == ADDR_SW_VAR_Z_X));
    }

    if (stencilAddrSettings.swizzleMode != ADDR_SW_64KB_Z_X)
    {
        PAL_ASSERT(IsGfx103Plus(palDevice) && (stencilAddrSettings.swizzleMode == ADDR_SW_VAR_Z_X));
    }

    m_regs.dbZInfo.bits.SW_MODE       = m_pImage->GetHwSwizzleMode(pDepthSubResInfo);
    m_regs.dbStencilInfo.bits.SW_MODE = m_pImage->GetHwSwizzleMode(pStencilSubResInfo);

    m_regs.dbZInfo.bits.FAULT_BEHAVIOR       = FAULT_ZERO;
    m_regs.dbStencilInfo.bits.FAULT_BEHAVIOR = FAULT_ZERO;

    m_regs.dbZInfo.bits.ITERATE_256          = m_pImage->GetIterate256(pDepthSubResInfo);
    m_regs.dbStencilInfo.bits.ITERATE_256    = m_pImage->GetIterate256(pStencilSubResInfo);

    PAL_ASSERT(CountSetBits(DB_DEPTH_VIEW__SLICE_START_MASK) == DbDepthViewSliceStartMaskNumBits);
    PAL_ASSERT(CountSetBits(DB_DEPTH_VIEW__SLICE_MAX_MASK)   == DbDepthViewSliceMaxMaskNumBits);

    m_regs.dbDepthView.bits.SLICE_START_HI = createInfo.baseArraySlice >> DbDepthViewSliceStartMaskNumBits;
    m_regs.dbDepthView.bits.SLICE_MAX_HI   = sliceMax >> DbDepthViewSliceMaxMaskNumBits;

    const uint32 cbDbCachePolicy = settings.cbDbCachePolicy;

    m_regs.dbRmiL2CacheControl.bits.Z_WR_POLICY      =
        (cbDbCachePolicy & Gfx10CbDbCachePolicyLruDepth)     ? CACHE_LRU_WR : CACHE_STREAM;
    m_regs.dbRmiL2CacheControl.bits.S_WR_POLICY      =
        (cbDbCachePolicy & Gfx10CbDbCachePolicyLruStencil)   ? CACHE_LRU_WR : CACHE_STREAM;
    m_regs.dbRmiL2CacheControl.bits.HTILE_WR_POLICY  =
        (cbDbCachePolicy & Gfx10CbDbCachePolicyLruHtile)     ? CACHE_LRU_WR : CACHE_STREAM;
    m_regs.dbRmiL2CacheControl.bits.ZPCPSD_WR_POLICY =
        (cbDbCachePolicy & Gfx10CbDbCachePolicyLruOcclusion) ? CACHE_LRU_WR : CACHE_STREAM;
    m_regs.dbRmiL2CacheControl.bits.Z_RD_POLICY      =
        (cbDbCachePolicy & Gfx10CbDbCachePolicyLruDepth)     ? CACHE_LRU_RD : CACHE_NOA;
    m_regs.dbRmiL2CacheControl.bits.S_RD_POLICY      =
        (cbDbCachePolicy & Gfx10CbDbCachePolicyLruStencil)   ? CACHE_LRU_RD : CACHE_NOA;
    m_regs.dbRmiL2CacheControl.bits.HTILE_RD_POLICY  =
        (cbDbCachePolicy & Gfx10CbDbCachePolicyLruHtile)     ? CACHE_LRU_RD : CACHE_NOA;

    if (palDevice.MemoryProperties().flags.supportsMall != 0)
    {
        m_regs.dbRmiL2CacheControl.most.HTILE_NOALLOC  = createInfo.flags.bypassMall;
        m_regs.dbRmiL2CacheControl.most.Z_NOALLOC      = createInfo.flags.bypassMall;
        m_regs.dbRmiL2CacheControl.most.S_NOALLOC      = createInfo.flags.bypassMall;
        m_regs.dbRmiL2CacheControl.most.ZPCPSD_NOALLOC = createInfo.flags.bypassMall; // Zpass / Pstat dump surface
    }

    if (palDevice.ChipProperties().gfxip.supportsVrs)
    {
        if (IsGfx10(palDevice))
        {
            // 0: Use the VRS rate generated by VRS rate combiners as shading rate
            // 1: Forces VRS rate to 1x1 if the rate generated by rate combiners is coarse rate
            m_regs.dbRenderOverride2.gfx10Vrs.FORCE_VRS_RATE_FINE = (settings.vrsForceRateFine ? 1 : 0);

            if (m_flags.hTile && (m_pImage->CanMipSupportMetaData(createInfo.mipLevel) == false))
            {
                // If the target mip does not support metadata in htile, the hardware will force the VRS image
                // into passthrough mode. This does not apply on Gfx11+.
                m_flags.vrsImageIncompatible = 1;
            }
        }

        if (IsGfx103Plus(palDevice))
        {
            //   For centroid computation you need to set DB_RENDER_OVERRIDE2::CENTROID_COMPUTATION_MODE to pick
            //   correct sample for centroid, which per DX12 spec is defined as the first covered sample. This
            //   means that it should use "2: Choose the sample with the smallest {~pixel_num, sample_id} as
            //   centroid, for all VRS rates"
            m_regs.dbRenderOverride2.gfx103Plus.CENTROID_COMPUTATION_MODE = 2;
        }
    }

    if (IsGfx11(palDevice))
    {
        const ImageCreateInfo& imageCreateInfo = pParentImg->GetImageCreateInfo();

        SetGfx11StaticDbRenderControlFields(device, imageCreateInfo.fragments, &m_regs.dbRenderControl);
    }
}

// =====================================================================================================================
// Writes the PM4 commands required to bind to depth/stencil slot. Returns the next unused DWORD in pCmdSpace.
uint32* DepthStencilView::WriteCommands(
    ImageLayout            depthLayout,   // Allowed usages/queues for the depth plane. Implies compression state.
    ImageLayout            stencilLayout, // Allowed usages/queues for the stencil plane. Implies compression state.
    CmdStream*             pCmdStream,
    bool                   isNested,
    regDB_RENDER_OVERRIDE* pDbRenderOverride,
    uint32*                pCmdSpace
    ) const
{
    DepthStencilViewRegs regs = m_regs;
    pCmdSpace = WriteCommandsCommon(depthLayout, stencilLayout, pCmdStream, pCmdSpace, &regs);

    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_RENDER_CONTROL, regs.dbRenderControl.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_RMI_L2_CACHE_CONTROL,
                                                  regs.dbRmiL2CacheControl.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_DEPTH_VIEW, regs.dbDepthView.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmDB_RENDER_OVERRIDE2,
                                                   mmDB_HTILE_DATA_BASE,
                                                   &regs.dbRenderOverride2,
                                                   pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_DEPTH_SIZE_XY, regs.dbDepthSizeXy.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmDB_Z_INFO,
                                                   mmDB_STENCIL_WRITE_BASE,
                                                   &regs.dbZInfo,
                                                   pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_HTILE_SURFACE, regs.dbHtileSurface.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmPA_SU_POLY_OFFSET_DB_FMT_CNTL,
                                                  regs.paSuPolyOffsetDbFmtCntl.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmCOHER_DEST_BASE_0, regs.coherDestBase0.u32All, pCmdSpace);

    // We need to write all the five hi-registers to support PAL's hi-address bit on gfx10
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_Z_READ_BASE_HI, regs.dbZReadBaseHi.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_Z_WRITE_BASE_HI,
                                                  regs.dbZWriteBaseHi.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_STENCIL_READ_BASE_HI,
                                                  regs.dbStencilReadBaseHi.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_STENCIL_WRITE_BASE_HI,
                                                  regs.dbStencilWriteBaseHi.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_HTILE_DATA_BASE_HI,
                                                  regs.dbHtileDataBaseHi.u32All,
                                                  pCmdSpace);

    // Update just the portion owned by DSV.
    BitfieldUpdateSubfield(&(pDbRenderOverride->u32All), regs.dbRenderOverride.u32All, DbRenderOverrideRmwMask);

    if (isNested)
    {
        pCmdSpace = pCmdStream->WriteContextRegRmw(mmDB_RENDER_OVERRIDE,
                                                   DbRenderOverrideRmwMask,
                                                   regs.dbRenderOverride.u32All,
                                                   pCmdSpace);
    }

    return pCmdSpace;
}

} // Gfx9
} // Pal
