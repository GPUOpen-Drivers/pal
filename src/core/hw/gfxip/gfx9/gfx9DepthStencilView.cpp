/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9FormatInfo.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "core/hw/gfxip/gfx9/gfx9MaskRam.h"
#include "core/addrMgr/addrMgr2/addrMgr2.h"
#include <type_traits>

using namespace Util;
using namespace Pal::Formats::Gfx9;
using std::is_same;

namespace Pal
{
namespace Gfx9
{

constexpr uint32 DbRenderOverrideRmwMask = DB_RENDER_OVERRIDE__FORCE_HIZ_ENABLE_MASK        |
                                           DB_RENDER_OVERRIDE__FORCE_HIS_ENABLE0_MASK       |
                                           DB_RENDER_OVERRIDE__FORCE_HIS_ENABLE1_MASK       |
                                           DB_RENDER_OVERRIDE__FORCE_STENCIL_VALID_MASK     |
                                           DB_RENDER_OVERRIDE__FORCE_Z_VALID_MASK           |
                                           DB_RENDER_OVERRIDE__DISABLE_TILE_RATE_TILES_MASK |
                                           DB_RENDER_OVERRIDE__NOOP_CULL_DISABLE_MASK;

// =====================================================================================================================
DepthStencilView::DepthStencilView(
    const Device*                             pDevice,
    const DepthStencilViewCreateInfo&         createInfo,
    const DepthStencilViewInternalCreateInfo& internalInfo)
    :
    m_device(*pDevice),
    m_pImage(GetGfx9Image(createInfo.pImage))
{
    PAL_ASSERT(createInfo.pImage != nullptr);
    const auto& imageInfo = m_pImage->Parent()->GetImageCreateInfo();
    const auto& parent    = *m_device.Parent();
    const auto& settings  = GetGfx9Settings(*(pDevice->Parent()));

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

    m_flags.depth           = parent.SupportsDepth(imageInfo.swizzledFormat.format, imageInfo.tiling);
    m_flags.stencil         = parent.SupportsStencil(imageInfo.swizzledFormat.format, imageInfo.tiling);
    m_flags.readOnlyDepth   = createInfo.flags.readOnlyDepth;
    m_flags.readOnlyStencil = createInfo.flags.readOnlyStencil;
    m_flags.viewVaLocked    = createInfo.flags.imageVaLocked;

    if (m_flags.depth && m_flags.stencil)
    {
        // Depth & Stencil format.
        m_depthSubresource.aspect       = ImageAspect::Depth;
        m_depthSubresource.mipLevel     = createInfo.mipLevel;
        m_depthSubresource.arraySlice   = 0;
        m_stencilSubresource.aspect     = ImageAspect::Stencil;
        m_stencilSubresource.mipLevel   = createInfo.mipLevel;
        m_stencilSubresource.arraySlice = 0;
    }
    else if (m_flags.depth)
    {
        // Depth-only format.
        m_depthSubresource.aspect     = ImageAspect::Depth;
        m_depthSubresource.mipLevel   = createInfo.mipLevel;
        m_depthSubresource.arraySlice = 0;
        m_stencilSubresource          = m_depthSubresource;
    }
    else
    {
        // Stencil-only format.
        m_stencilSubresource.aspect     = ImageAspect::Stencil;
        m_stencilSubresource.mipLevel   = createInfo.mipLevel;
        m_stencilSubresource.arraySlice = 0;
        m_depthSubresource              = m_stencilSubresource;
    }

    m_depthLayoutToState   = m_pImage->LayoutToDepthCompressionState(m_depthSubresource);
    m_stencilLayoutToState = m_pImage->LayoutToDepthCompressionState(m_stencilSubresource);

    const SubResourceInfo*const pDepthSubResInfo   = m_pImage->Parent()->SubresourceInfo(m_depthSubresource);
    const SubResourceInfo*const pStencilSubResInfo = m_pImage->Parent()->SubresourceInfo(m_stencilSubresource);

    m_flags.depthMetadataTexFetch   = pDepthSubResInfo->flags.supportMetaDataTexFetch && m_flags.hTile;
    m_flags.stencilMetadataTexFetch = pStencilSubResInfo->flags.supportMetaDataTexFetch && m_flags.hTile;

    if (m_device.Settings().waitOnMetadataMipTail)
    {
        m_flags.waitOnMetadataMipTail = m_pImage->IsInMetadataMipTail(MipLevel());
    }
}

// =====================================================================================================================
// Builds the PM4 packet headers for an image of PM4 commands used to write this View object to hardware.
template <typename Pm4ImgType>
void DepthStencilView::CommonBuildPm4Headers(
    Pm4ImgType* pPm4Img
    ) const
{
    if (m_hTileUsage.dsMetadata != 0)
    {
        const CmdUtil& cmdUtil = m_device.CmdUtil();

        // If the parent image has HTile and some aspect is in the compressed state, we need to add a LOAD_CONTEXT_REG
        // packet to load the image's fast-clear metadata.
        // NOTE: We do not know the GPU virtual address of the metadata until bind-time.
        pPm4Img->spaceNeeded += cmdUtil.BuildLoadContextRegsIndex<true>(0,
                                                                        mmDB_STENCIL_CLEAR,
                                                                        (mmDB_DEPTH_CLEAR - mmDB_STENCIL_CLEAR + 1),
                                                                        &pPm4Img->loadMetaDataIndex);
    }
}

// =====================================================================================================================
// Updates the specified PM4 image with the virtual addresses of the image and the image's various metadata addresses.
template <typename Pm4ImgType, typename FmtInfoTableType>
void DepthStencilView::InitCommonImageView(
    const DepthStencilViewCreateInfo&         createInfo,
    const DepthStencilViewInternalCreateInfo& internalInfo,
    const FmtInfoTableType*                   pFmtInfo,
    Pm4ImgType*                               pPm4Img,
    regDB_RENDER_OVERRIDE*                    pDbRenderOverride)
{
    const CmdUtil&              cmdUtil              = m_device.CmdUtil();
    const SubresId              baseDepthSubResId    = { m_depthSubresource.aspect, 0 , 0 };
    const SubResourceInfo*const pDepthSubResInfo     = m_pImage->Parent()->SubresourceInfo(m_depthSubresource);
    const SubResourceInfo*const pStencilSubResInfo   = m_pImage->Parent()->SubresourceInfo(m_stencilSubresource);
    const Gfx9PalSettings&      settings             = m_device.Settings();
    const bool                  zReadOnly            = (createInfo.flags.readOnlyDepth != 0);
    const bool                  sReadOnly            = (createInfo.flags.readOnlyStencil != 0);
    const auto&                 imageCreateInfo      = m_pImage->Parent()->GetImageCreateInfo();
    const ChNumFormat           zFmt                 = pDepthSubResInfo->format.format;
    const ChNumFormat           sFmt                 = pStencilSubResInfo->format.format;
    const ZFormat               hwZFmt               = HwZFmt(pFmtInfo, zFmt);

    if (m_flags.hTile)
    {
        const Gfx9Htile*const pHtile = m_pImage->GetHtile();

        // Tell the HW that HTILE metadata is present.
        pPm4Img->dbZInfo.bits.ZRANGE_PRECISION           = pHtile->ZRangePrecision();

        pPm4Img->dbZInfo.bits.TILE_SURFACE_ENABLE        = 1;
        pPm4Img->dbStencilInfo.bits.TILE_STENCIL_DISABLE = pHtile->TileStencilDisabled();

        if (internalInfo.flags.isExpand | internalInfo.flags.isDepthCopy | internalInfo.flags.isStencilCopy)
        {
            pPm4Img->dbRenderControl.bits.DEPTH_COMPRESS_DISABLE   = !zReadOnly;
            pPm4Img->dbRenderControl.bits.STENCIL_COMPRESS_DISABLE = !sReadOnly;

            m_flags.dbRenderControlLocked = 1; // This cannot change at bind-time for expands and copies!
        }

        if (internalInfo.flags.isResummarize)
        {
            pPm4Img->dbRenderControl.bits.RESUMMARIZE_ENABLE = 1;
        }

        pPm4Img->dbZInfo.bits.ALLOW_EXPCLEAR       =
                (imageCreateInfo.usageFlags.shaderRead == 1) ? settings.dbPerTileExpClearEnable : 0;
        pPm4Img->dbStencilInfo.bits.ALLOW_EXPCLEAR =
                (imageCreateInfo.usageFlags.shaderRead == 1) ? settings.dbPerTileExpClearEnable : 0;

        pPm4Img->dbHtileSurface.u32All   = pHtile->DbHtileSurface(m_depthSubresource.mipLevel).u32All;
        pPm4Img->dbPreloadControl.u32All = pHtile->DbPreloadControl(m_depthSubresource.mipLevel).u32All;

        if (m_flags.depthMetadataTexFetch)
        {
            // This image might get texture-fetched, so setup any register info specific to texture fetches here.
            pPm4Img->dbZInfo.bits.DECOMPRESS_ON_N_ZPLANES = CalcDecompressOnZPlanesValue(hwZFmt);
        }
    }
    else
    {
        // Tell the HW that HTILE metadata is not present.
        pPm4Img->dbZInfo.bits.TILE_SURFACE_ENABLE              = 0;
        pPm4Img->dbStencilInfo.bits.TILE_STENCIL_DISABLE       = 1;
        pPm4Img->dbRenderControl.bits.DEPTH_COMPRESS_DISABLE   = 1;
        pPm4Img->dbRenderControl.bits.STENCIL_COMPRESS_DISABLE = 1;
    }

    // Setup DB_DEPTH_VIEW.
    pPm4Img->dbDepthView.bits.SLICE_START       = createInfo.baseArraySlice;
    pPm4Img->dbDepthView.bits.SLICE_MAX         = (createInfo.arraySize + createInfo.baseArraySlice - 1);
    pPm4Img->dbDepthView.bits.Z_READ_ONLY       = (zReadOnly ? 1 : 0);
    pPm4Img->dbDepthView.bits.STENCIL_READ_ONLY = (sReadOnly ? 1 : 0);
    pPm4Img->dbDepthView.bits.MIPID             = createInfo.mipLevel;

    // Set clear enable fields if the create info indicates the view should be a fast clear view
    pPm4Img->dbRenderControl.bits.DEPTH_CLEAR_ENABLE   = internalInfo.flags.isDepthClear;
    pPm4Img->dbRenderControl.bits.STENCIL_CLEAR_ENABLE = internalInfo.flags.isStencilClear;
    pPm4Img->dbRenderControl.bits.DEPTH_COPY           = internalInfo.flags.isDepthCopy;
    pPm4Img->dbRenderControl.bits.STENCIL_COPY         = internalInfo.flags.isStencilCopy;

    if (internalInfo.flags.isDepthCopy | internalInfo.flags.isStencilCopy)
    {
        pPm4Img->dbRenderControl.bits.COPY_SAMPLE   = 0;
        pPm4Img->dbRenderControl.bits.COPY_CENTROID = 1;
    }

    // Enable HiZ/HiS based on settings
    pDbRenderOverride->bits.FORCE_HIZ_ENABLE  = settings.hiDepthEnable   ? FORCE_OFF : FORCE_DISABLE;
    pDbRenderOverride->bits.FORCE_HIS_ENABLE0 = settings.hiStencilEnable ? FORCE_OFF : FORCE_DISABLE;
    pDbRenderOverride->bits.FORCE_HIS_ENABLE1 = settings.hiStencilEnable ? FORCE_OFF : FORCE_DISABLE;

    if (internalInfo.flags.u32All != 0)
    {
        // DB_RENDER_OVERRIDE cannot change at bind-time due to compression states for internal blit types.
        m_flags.dbRenderOverrideLocked = 1;
    }

    if (internalInfo.flags.isResummarize)
    {
        pDbRenderOverride->bits.FORCE_Z_VALID           = !zReadOnly;
        pDbRenderOverride->bits.FORCE_STENCIL_VALID     = !sReadOnly;
        pDbRenderOverride->bits.NOOP_CULL_DISABLE       = 1;
        pDbRenderOverride->bits.DISABLE_TILE_RATE_TILES = 1;
    }

    m_extent.width  = pDepthSubResInfo->extentTexels.width;
    m_extent.height = pDepthSubResInfo->extentTexels.height;

    pPm4Img->dbZInfo.bits.READ_SIZE          = settings.dbRequestSize;
    pPm4Img->dbZInfo.bits.NUM_SAMPLES        = Util::Log2(imageCreateInfo.samples);
    pPm4Img->dbZInfo.bits.MAXMIP             = (imageCreateInfo.mipLevels - 1);
    pPm4Img->dbZInfo.bits.PARTIALLY_RESIDENT = imageCreateInfo.flags.prt;
    pPm4Img->dbZInfo.bits.FORMAT             = hwZFmt;

    pPm4Img->dbStencilInfo.bits.FORMAT             = HwStencilFmt(pFmtInfo, sFmt);
    pPm4Img->dbStencilInfo.bits.PARTIALLY_RESIDENT = pPm4Img->dbZInfo.bits.PARTIALLY_RESIDENT;

    // For 4xAA and 8xAA need to decompress on flush for better performance
    pPm4Img->dbRenderOverride2.bits.DECOMPRESS_Z_ON_FLUSH       = (imageCreateInfo.samples > 2) ? 1 : 0;
    pPm4Img->dbRenderOverride2.bits.DISABLE_COLOR_ON_VALIDATION = settings.dbDisableColorOnValidation;

    // Setup PA_SU_POLY_OFFSET_DB_FMT_CNTL.
    if (createInfo.flags.absoluteDepthBias == 0)
    {
        // NOTE: The client has indicated this Image has promoted 24bit depth to 32bits, we should set the negative num
        //       bit as -24 and use fixed points format
        pPm4Img->paSuPolyOffsetDbFmtCntl.bits.POLY_OFFSET_NEG_NUM_DB_BITS =
            (imageCreateInfo.usageFlags.depthAsZ24 == 1) ? -24 : ((pPm4Img->dbZInfo.bits.FORMAT == Z_16) ? -16 : -23);
        pPm4Img->paSuPolyOffsetDbFmtCntl.bits.POLY_OFFSET_DB_IS_FLOAT_FMT =
            (pPm4Img->dbZInfo.bits.FORMAT == Z_32_FLOAT && (imageCreateInfo.usageFlags.depthAsZ24 == 0)) ? 1 : 0;
    }
    else
    {
        pPm4Img->paSuPolyOffsetDbFmtCntl.u32All = 0;
    }

    // Setup DB_RENDER_OVERRIDE fields
    PAL_ASSERT((pDbRenderOverride->u32All & ~DbRenderOverrideRmwMask) == 0);

    cmdUtil.BuildContextRegRmw(mmDB_RENDER_OVERRIDE,
                               DbRenderOverrideRmwMask,
                               pDbRenderOverride->u32All,
                               &pPm4Img->dbRenderOverrideRmw);
}

// =====================================================================================================================
// Updates the specified PM4 image with the virtual addresses of the image and the image's various metadata addresses.
template <typename Pm4ImgType>
void DepthStencilView::UpdateImageVa(
    Pm4ImgType* pPm4Img
    ) const
{
    const BoundGpuMemory& boundMem = m_pImage->Parent()->GetBoundGpuMemory();

    // The "GetSubresource256BAddrSwizzled" function will crash if no memory has been bound to
    // the associated image yet, so don't do anything if it's not safe
    if (boundMem.IsBound())
    {
        // Setup bits indicating the page size.
        if (is_same<Pm4ImgType, Gfx10DepthStencilViewPm4Img>::value)
        {
            const bool isBigPage = IsImageBigPageCompatible(*m_pImage, Gfx10AllowBigPageDepthStencil);

            Gfx10DepthStencilViewPm4Img* pGfx10Pm4Img = reinterpret_cast<Gfx10DepthStencilViewPm4Img*>(pPm4Img);
            pGfx10Pm4Img->dbRmiL2CacheControl.bits.Z_BIG_PAGE = isBigPage;
            pGfx10Pm4Img->dbRmiL2CacheControl.bits.S_BIG_PAGE = isBigPage;
        }

        uint32  zReadBase        = m_pImage->GetSubresource256BAddrSwizzled(m_depthSubresource);
        uint32  zWriteBase       = zReadBase;
        uint32  stencilReadBase  = m_pImage->GetSubresource256BAddrSwizzled(m_stencilSubresource);
        uint32  stencilWriteBase = stencilReadBase;

        if (m_flags.hTile)
        {
            if (m_hTileUsage.dsMetadata != 0)
            {
                // Program fast-clear metadata base address.
                gpusize metaDataVirtAddr = m_pImage->FastClearMetaDataAddr(MipLevel());
                PAL_ASSERT((metaDataVirtAddr & 0x3) == 0);

                pPm4Img->loadMetaDataIndex.bitfields2.mem_addr_lo = (LowPart(metaDataVirtAddr) >> 2);
                pPm4Img->loadMetaDataIndex.mem_addr_hi            = HighPart(metaDataVirtAddr);
            }

            // Program HTile base address.
            pPm4Img->dbHtileDataBase.bits.BASE_256B = m_pImage->GetHtile256BAddr();

        }

        if (m_flags.depth)
        {
            PAL_ASSERT(m_pImage->GetSubresource256BAddrSwizzledHi(m_depthSubresource) == 0);

            // Program depth read and write bases
            pPm4Img->dbZReadBase.u32All  = zReadBase;
            pPm4Img->dbZWriteBase.u32All = zWriteBase;
        }

        if (m_flags.stencil)
        {
            PAL_ASSERT(m_pImage->GetSubresource256BAddrSwizzledHi(m_stencilSubresource) == 0);

            // Program stencil read and write bases
            pPm4Img->dbStencilReadBase.u32All  = stencilReadBase;
            pPm4Img->dbStencilWriteBase.u32All = stencilWriteBase;

            // Copy the stencil base address into one of the CP's generic sync registers.
            pPm4Img->coherDestBase0.bits.DEST_BASE_256B = pPm4Img->dbStencilWriteBase.bits.BASE_256B;
        }
    }
}

// =====================================================================================================================
// Writes the PM4 commands required to bind to depth/stencil slot. Returns the next unused DWORD in pCmdSpace.
template <typename Pm4ImgType>
uint32* DepthStencilView::WriteCommandsInternal(
    ImageLayout        depthLayout,
    ImageLayout        stencilLayout,
    CmdStream*         pCmdStream,
    uint32*            pCmdSpace,
    const Pm4ImgType&  pm4Img
    ) const
{
    const DepthStencilCompressionState depthState =
            ImageLayoutToDepthCompressionState(m_depthLayoutToState, depthLayout);
    const DepthStencilCompressionState stencilState =
            ImageLayoutToDepthCompressionState(m_stencilLayoutToState, stencilLayout);

    const Pm4ImgType* pPm4Commands = &pm4Img;
    // Spawn a local copy of the PM4 image, since some register values may need to be updated in this method.  For
    // some clients, the base address and Htile address also need to be updated.  The contents of the local copy will
    // depend on which Image state is specified.
    Pm4ImgType patchedPm4Commands;

    if ((depthState != DepthStencilCompressed) | (stencilState != DepthStencilCompressed))
    {
        patchedPm4Commands = *pPm4Commands;
        pPm4Commands = &patchedPm4Commands;

        // For decompressed rendering to an Image, we need to override the values of DB_DEPTH_CONTROL and
        // DB_RENDER_OVERRIDE, depending on the compression states for depth and stencil.
        if ((m_flags.dbRenderControlLocked == 0)
           )
        {
            patchedPm4Commands.dbRenderControl.bits.DEPTH_COMPRESS_DISABLE   = (depthState   != DepthStencilCompressed);
            patchedPm4Commands.dbRenderControl.bits.STENCIL_COMPRESS_DISABLE = (stencilState != DepthStencilCompressed);
        }
        if (m_flags.dbRenderOverrideLocked == 0)
        {
            if (depthState == DepthStencilDecomprNoHiZ)
            {
                patchedPm4Commands.dbRenderOverrideRmw.reg_data &= ~DB_RENDER_OVERRIDE__FORCE_HIZ_ENABLE_MASK;
                patchedPm4Commands.dbRenderOverrideRmw.reg_data |=
                    (FORCE_DISABLE << DB_RENDER_OVERRIDE__FORCE_HIZ_ENABLE__SHIFT);
            }
            if (stencilState == DepthStencilDecomprNoHiZ)
            {
                patchedPm4Commands.dbRenderOverrideRmw.reg_data &= ~(DB_RENDER_OVERRIDE__FORCE_HIS_ENABLE0_MASK |
                                                                     DB_RENDER_OVERRIDE__FORCE_HIS_ENABLE1_MASK);
                patchedPm4Commands.dbRenderOverrideRmw.reg_data |=
                    ((FORCE_DISABLE << DB_RENDER_OVERRIDE__FORCE_HIS_ENABLE0__SHIFT) |
                     (FORCE_DISABLE << DB_RENDER_OVERRIDE__FORCE_HIS_ENABLE1__SHIFT));
            }
        }
    }

    if ((m_flags.viewVaLocked == 0) && m_pImage->Parent()->GetBoundGpuMemory().IsBound())
    {
        if (pPm4Commands != &patchedPm4Commands)
        {
            patchedPm4Commands = *pPm4Commands;
            pPm4Commands = &patchedPm4Commands;
        }
        UpdateImageVa(&patchedPm4Commands);
    }

    const size_t spaceNeeded = ((depthState == DepthStencilCompressed) || (stencilState == DepthStencilCompressed))
        ? pm4Img.spaceNeeded : pm4Img.spaceNeededDecompressed;

    PAL_ASSERT(pCmdStream != nullptr);
    pCmdSpace = pCmdStream->WritePm4Image(spaceNeeded, pPm4Commands, pCmdSpace);

    // During the client is binding depth stencil target, we load the pretests meta data, which we expect is
    // initialized by ClearHiSPretestsMetaData and later set by CmdUpdateHiSPretests, into a paired of DB
    // context registers which are used to store the HiStencil pretests.
    if (m_pImage->HasHiSPretestsMetaData())
    {
        const gpusize metaDataVirtAddr = m_pImage->HiSPretestsMetaDataAddr(MipLevel());
        PAL_ASSERT((metaDataVirtAddr & 0x3) == 0);
        pCmdSpace = pCmdStream->WriteLoadSeqContextRegs(
                                mmDB_SRESULTS_COMPARE_STATE0,
                                (mmDB_SRESULTS_COMPARE_STATE1 - mmDB_SRESULTS_COMPARE_STATE0 + 1),
                                metaDataVirtAddr,
                                pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Determines the proper value of the DB_Z_INFO.DECOMPRESS_ON_N_ZPLANES register value
uint32 DepthStencilView::CalcDecompressOnZPlanesValue(
    ZFormat  hwZFmt
    ) const
{
    const Pal::Image*  pParent    = m_pImage->Parent();
    const auto&        createInfo = pParent->GetImageCreateInfo();

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

    const Gfx9PalSettings& settings = m_device.Settings();

    // Check this first to eliminate products that don't have an ITERATE_256 bit in the first place.  Calling
    // the GetIterate256() function below causes asserts on non-GFX10 products.
    if (settings.waTwoPlanesIterate256)
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

        pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmDB_STENCIL_CLEAR,
                                                       mmDB_DEPTH_CLEAR,
                                                       &clearValueRegs,
                                                       pCmdSpace);
    }
    else if (metaDataClearFlags == HtileAspectDepth)
    {
        pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_DEPTH_CLEAR,
                                                      *reinterpret_cast<const uint32*>(&depth),
                                                      pCmdSpace);
    }
    else
    {
        PAL_ASSERT(metaDataClearFlags == HtileAspectStencil);

        clearValueRegs.dbStencilClear.u32All     = 0;
        clearValueRegs.dbStencilClear.bits.CLEAR = stencil;

        pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_STENCIL_CLEAR,
                                                      clearValueRegs.dbStencilClear.u32All,
                                                      pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Helper function which adds commands into the command stream when the currently-bound depth target is changing.
// Returns the address to where future commands will be written.
uint32* DepthStencilView::HandleBoundTargetChanged(
    const CmdUtil& cmdUtil,
    uint32*        pCmdSpace)
{
    // If you change the mips of a resource being used as a depth/stencil target, we need to flush the DB metadata
    // cache. This protects against the case where an Htile cacheline can contain data from two different mip levels
    // in different RB's.
    return (pCmdSpace + cmdUtil.BuildNonSampleEventWrite(FLUSH_AND_INV_DB_META, EngineTypeUniversal, pCmdSpace));
}

// =====================================================================================================================
Gfx9DepthStencilView::Gfx9DepthStencilView(
    const Device*                             pDevice,
    const DepthStencilViewCreateInfo&         createInfo,
    const DepthStencilViewInternalCreateInfo& internalInfo)
    :
    DepthStencilView(pDevice, createInfo, internalInfo)
{
    memset(&m_pm4Cmds, 0, sizeof(m_pm4Cmds));

    BuildPm4Headers();
    InitRegisters(createInfo, internalInfo);

    if (IsVaLocked())
    {
        UpdateImageVa(&m_pm4Cmds);
    }
}

// =====================================================================================================================
// Builds the PM4 packet headers for an image of PM4 commands used to write this View object to hardware.
void Gfx9DepthStencilView::BuildPm4Headers()
{
    const CmdUtil& cmdUtil = m_device.CmdUtil();

    CommonBuildPm4Headers(&m_pm4Cmds);

    size_t spaceNeeded = cmdUtil.BuildSetSeqContextRegs(Gfx09::mmDB_Z_INFO,
                                                        Gfx09::mmDB_STENCIL_WRITE_BASE_HI,
                                                        &m_pm4Cmds.hdrDbZInfoToDfsmControl);

    spaceNeeded += cmdUtil.BuildSetSeqContextRegs(Gfx09::mmDB_Z_INFO2,
                                                  Gfx09::mmDB_STENCIL_INFO2,
                                                  &m_pm4Cmds.hdrDbZInfo2ToStencilInfo2);

    spaceNeeded += cmdUtil.BuildSetOneContextReg(mmDB_DEPTH_VIEW, &m_pm4Cmds.hdrDbDepthView);
    spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmDB_RENDER_OVERRIDE2,
                                                  Gfx09::mmDB_DEPTH_SIZE,
                                                  &m_pm4Cmds.hdrDbRenderOverride2);

    spaceNeeded += cmdUtil.BuildSetOneContextReg(mmDB_HTILE_SURFACE, &m_pm4Cmds.hdrDbHtileSurface);
    spaceNeeded += cmdUtil.BuildSetOneContextReg(mmDB_PRELOAD_CONTROL, &m_pm4Cmds.hdrDbPreloadControl);
    spaceNeeded += cmdUtil.BuildSetOneContextReg(mmDB_RENDER_CONTROL, &m_pm4Cmds.hdrDbRenderControl);
    spaceNeeded += cmdUtil.BuildSetOneContextReg(mmPA_SU_POLY_OFFSET_DB_FMT_CNTL,
                                                 &m_pm4Cmds.hdrPaSuPolyOffsetDbFmtCntl);
    spaceNeeded += cmdUtil.BuildSetOneContextReg(mmCOHER_DEST_BASE_0, &m_pm4Cmds.hdrCoherDestBase);
    spaceNeeded += CmdUtil::ContextRegRmwSizeDwords; // Header and value defined by InitRegisters()

    m_pm4Cmds.spaceNeeded             += spaceNeeded;
    m_pm4Cmds.spaceNeededDecompressed += spaceNeeded;
}

// =====================================================================================================================
// Finalizes the PM4 packet image by setting up the register values used to write this View object to hardware.
void Gfx9DepthStencilView::InitRegisters(
    const DepthStencilViewCreateInfo&         createInfo,
    const DepthStencilViewInternalCreateInfo& internalInfo)
{
    const MergedFmtInfo*const pFmtInfo =
        MergedChannelFmtInfoTbl(GfxIpLevel::GfxIp9, &m_device.GetPlatform()->PlatformSettings());

    DB_RENDER_OVERRIDE dbRenderOverride = { };
    InitCommonImageView(createInfo, internalInfo, pFmtInfo, &m_pm4Cmds, &dbRenderOverride);

    const auto*const            pDepthSubResInfo     = m_pImage->Parent()->SubresourceInfo(m_depthSubresource);
    const auto*const            pStencilSubResInfo   = m_pImage->Parent()->SubresourceInfo(m_stencilSubresource);
    const SubresId              baseDepthSubResId    = { m_depthSubresource.aspect, 0 , 0 };
    const SubResourceInfo*const pBaseDepthSubResInfo = m_pImage->Parent()->SubresourceInfo(baseDepthSubResId);

    const ADDR2_COMPUTE_SURFACE_INFO_OUTPUT*const pDepthAddrInfo = m_pImage->GetAddrOutput(pDepthSubResInfo);
    const ADDR2_COMPUTE_SURFACE_INFO_OUTPUT*const pStAddrInfo    = m_pImage->GetAddrOutput(pStencilSubResInfo);

    const auto& depthAddrSettings   = m_pImage->GetAddrSettings(pDepthSubResInfo);
    const auto& stencilAddrSettings = m_pImage->GetAddrSettings(pStencilSubResInfo);

    // Setup the size
    m_pm4Cmds.dbDepthSize.gfx09.X_MAX = (pBaseDepthSubResInfo->extentTexels.width  - 1);
    m_pm4Cmds.dbDepthSize.gfx09.Y_MAX = (pBaseDepthSubResInfo->extentTexels.height - 1);

    // From the reg-spec:  Indicates that compressed data must be iterated on flush every pipe interleave bytes in
    //                     order to be readable by TC
    m_pm4Cmds.dbZInfo.gfx09.ITERATE_FLUSH       = m_flags.depthMetadataTexFetch;
    m_pm4Cmds.dbStencilInfo.gfx09.ITERATE_FLUSH = m_flags.stencilMetadataTexFetch;

    m_pm4Cmds.dbZInfo.gfx09.FAULT_BEHAVIOR        = FAULT_ZERO;
    m_pm4Cmds.dbStencilInfo.gfx09.FAULT_BEHAVIOR  = FAULT_ZERO;
    m_pm4Cmds.dbZInfo.bits.SW_MODE                = AddrMgr2::GetHwSwizzleMode(depthAddrSettings.swizzleMode);
    m_pm4Cmds.dbZInfo2.bits.EPITCH                = AddrMgr2::CalcEpitch(pDepthAddrInfo);
    m_pm4Cmds.dbStencilInfo2.bits.EPITCH          = AddrMgr2::CalcEpitch(pStAddrInfo);
    m_pm4Cmds.dbStencilInfo.bits.SW_MODE          = AddrMgr2::GetHwSwizzleMode(stencilAddrSettings.swizzleMode);
}

// =====================================================================================================================
// Writes the PM4 commands required to bind to depth/stencil slot. Returns the next unused DWORD in pCmdSpace.
uint32* Gfx9DepthStencilView::WriteCommands(
    ImageLayout depthLayout,   // Allowed usages/queues for the depth aspect. Implies compression state.
    ImageLayout stencilLayout, // Allowed usages/queues for the stencil aspect. Implies compression state.
    CmdStream*  pCmdStream,
    uint32*     pCmdSpace
    ) const
{
    return WriteCommandsInternal(depthLayout, stencilLayout, pCmdStream, pCmdSpace, m_pm4Cmds);
}

// =====================================================================================================================
// On Gfx9, there is a bug on cleared TC-compatible surfaces where the ZRange is not reset after LateZ kills pixels.
// The workaround for this is to set DB_Z_INFO.ZRANGE_PRECISION to match the last fast clear value. Since
// ZRANGE_PRECISION is currently always set to 1 by default, we only need to re-write it if the last fast clear
// value is 0.0f.
//
//
// This function writes the PM4 to set ZRANGE_PRECISION to 0. There are two cases where this needs to be called:
//      1. After binding a TC-compatible depth target. We need to check the workaroud metadata to know if the last
//         clear value was 0.0f, so requiresCondExec should be true.
//      2. After a compute-based fast clear to 0.0f if this view is currently bound as a depth target. We do not need
//         to look at the metadata in this case, so requiresCondExec should be false.
//
// Returns the next unused DWORD in pCmdSpace.
uint32* Gfx9DepthStencilView::UpdateZRangePrecision(
    bool       requiresCondExec,
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    PAL_ASSERT(m_device.WaTcCompatZRange());

    // This workaround only applies to depth-stencil image that is using "ZRange" format hitle
    PAL_ASSERT(m_pImage->GetHtile()->TileStencilDisabled() == false);

    if (requiresCondExec)
    {
        const CmdUtil&    cmdUtil           = m_device.CmdUtil();
        const gpusize     metaDataVirtAddr  = GetImage()->GetWaTcCompatZRangeMetaDataAddr(MipLevel());
        const Pal::uint32 setContextRegSize = CmdUtil::ContextRegSizeDwords + 1;

        // Build a COND_EXEC to check the workaround metadata. If the last clear value was 0.0f, the metadata will
        // be non-zero and the register will be re-written, otherwise the metadata will be 0 and register write
        // will be skipped.
        pCmdSpace += cmdUtil.BuildCondExec(metaDataVirtAddr, setContextRegSize, pCmdSpace);
    }

    // DB_Z_INFO is the same for all compression states
    regDB_Z_INFO regVal          = m_pm4Cmds.dbZInfo;
    regVal.bits.ZRANGE_PRECISION = 0;

    return pCmdStream->WriteSetOneContextReg(Gfx09::mmDB_Z_INFO, regVal.u32All, pCmdSpace);
}

// =====================================================================================================================
Gfx10DepthStencilView::Gfx10DepthStencilView(
    const Device*                             pDevice,
    const DepthStencilViewCreateInfo&         createInfo,
    const DepthStencilViewInternalCreateInfo& internalInfo)
    :
    DepthStencilView(pDevice, createInfo, internalInfo),
    m_baseArraySlice(createInfo.baseArraySlice),
    m_arraySize(createInfo.arraySize)
{
    memset(&m_pm4Cmds, 0, sizeof(m_pm4Cmds));

    BuildPm4Headers();
    InitRegisters(createInfo, internalInfo);

    if (IsVaLocked())
    {
        UpdateImageVa(&m_pm4Cmds);
    }
}

// =====================================================================================================================
// Builds the PM4 packet headers for an image of PM4 commands used to write this View object to hardware.
void Gfx10DepthStencilView::BuildPm4Headers()
{
    const CmdUtil& cmdUtil = m_device.CmdUtil();

    uint32  dbDepthInfoRegMask = 0;
    uint32  dbDepthInfoRegData = 0;

    CommonBuildPm4Headers(&m_pm4Cmds);

    size_t spaceNeeded = cmdUtil.BuildSetOneContextReg(mmDB_RENDER_CONTROL, &m_pm4Cmds.hdrDbRenderControl);

    spaceNeeded += cmdUtil.BuildSetOneContextReg(Gfx10::mmDB_RMI_L2_CACHE_CONTROL, &m_pm4Cmds.hdrDbRmiL2CacheControl);
    spaceNeeded += cmdUtil.BuildSetOneContextReg(mmDB_DEPTH_VIEW, &m_pm4Cmds.hdrDbDepthView);
    spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmDB_RENDER_OVERRIDE2,
                                                  mmDB_HTILE_DATA_BASE,
                                                  &m_pm4Cmds.hdrDbRenderOverride2);

    spaceNeeded += cmdUtil.BuildSetOneContextReg(Gfx10::mmDB_DEPTH_SIZE_XY, &m_pm4Cmds.hdrDbDepthSize);

    spaceNeeded += cmdUtil.BuildSetSeqContextRegs(Gfx10::mmDB_Z_INFO,
                                                  Gfx10::mmDB_STENCIL_WRITE_BASE,
                                                  &m_pm4Cmds.hdrDbZInfo);
    spaceNeeded += cmdUtil.BuildSetOneContextReg(mmDB_HTILE_SURFACE, &m_pm4Cmds.hdrDbHtileSurface);
    spaceNeeded += cmdUtil.BuildSetOneContextReg(mmDB_PRELOAD_CONTROL, &m_pm4Cmds.hdrDbPreloadControl);
    spaceNeeded += cmdUtil.BuildSetOneContextReg(mmPA_SU_POLY_OFFSET_DB_FMT_CNTL,
                                                 &m_pm4Cmds.hdrPaSuPolyOffsetDbFmtCntl);
    spaceNeeded += cmdUtil.BuildSetOneContextReg(mmCOHER_DEST_BASE_0, &m_pm4Cmds.hdrCoherDestBase);
    spaceNeeded += CmdUtil::ContextRegRmwSizeDwords; // Header and value defined by InitRegisters()

    if (cmdUtil.GetRegInfo().mmDbDepthInfo != 0)
    {
        spaceNeeded += cmdUtil.BuildContextRegRmw(cmdUtil.GetRegInfo().mmDbDepthInfo,
                                                  dbDepthInfoRegMask,
                                                  dbDepthInfoRegData,
                                                  &m_pm4Cmds.dbDepthInfo);
    }
    else
    {
        // If this register doesn't even exist, then just NOP it.
        spaceNeeded += cmdUtil.BuildNop(CmdUtil::RegRmwSizeDwords, &m_pm4Cmds.dbDepthInfo);
    }

    m_pm4Cmds.spaceNeeded             += spaceNeeded;
    m_pm4Cmds.spaceNeededDecompressed += spaceNeeded;
}

// =====================================================================================================================
// Finalizes the PM4 packet image by setting up the register values used to write this View object to hardware.
void Gfx10DepthStencilView::InitRegisters(
    const DepthStencilViewCreateInfo&         createInfo,
    const DepthStencilViewInternalCreateInfo& internalInfo)
{
    const auto*                 pParentImg           = m_pImage->Parent();
    const ImageCreateInfo&      imageCreateInfo      = pParentImg->GetImageCreateInfo();
    const auto&                 palDevice            = *m_device.Parent();
    GfxIpLevel                  gfxip                = palDevice.ChipProperties().gfxLevel;
    const Gfx9PalSettings&      settings             = GetGfx9Settings(palDevice);

    const MergedFlatFmtInfo*const pFmtInfo =
        MergedChannelFlatFmtInfoTbl(gfxip, &m_device.GetPlatform()->PlatformSettings());

    const SubResourceInfo*      pDepthSubResInfo     = pParentImg->SubresourceInfo(m_depthSubresource);
    const SubResourceInfo*      pStencilSubResInfo   = pParentImg->SubresourceInfo(m_stencilSubresource);
    const SubresId              baseDepthSubResId    = { m_depthSubresource.aspect, 0 , 0 };
    const SubResourceInfo*const pBaseDepthSubResInfo = pParentImg->SubresourceInfo(baseDepthSubResId);

    DB_RENDER_OVERRIDE dbRenderOverride = { };
    InitCommonImageView(createInfo, internalInfo, pFmtInfo, &m_pm4Cmds, &dbRenderOverride);

    // GFX10 adds extra bits to the slice selection...  the "hi" bits are in different fields from the "low" bits.
    // The "low" bits were set in "InitCommonImageView", take care of the new high bits here.
    const uint32 sliceMax = (createInfo.arraySize + createInfo.baseArraySlice - 1);

    // Setup the size
    m_pm4Cmds.dbDepthSize.bits.X_MAX = (pBaseDepthSubResInfo->extentTexels.width  - 1);
    m_pm4Cmds.dbDepthSize.bits.Y_MAX = (pBaseDepthSubResInfo->extentTexels.height - 1);

    // From the reg-spec:  Indicates that compressed data must be iterated on flush every pipe interleave bytes in
    //                     order to be readable by TC
    m_pm4Cmds.dbZInfo.gfx10.ITERATE_FLUSH       = m_flags.depthMetadataTexFetch;
    m_pm4Cmds.dbStencilInfo.gfx10.ITERATE_FLUSH = m_flags.stencilMetadataTexFetch;

    const auto& depthAddrSettings   = m_pImage->GetAddrSettings(pDepthSubResInfo);
    const auto& stencilAddrSettings = m_pImage->GetAddrSettings(pStencilSubResInfo);

    if (depthAddrSettings.swizzleMode != ADDR_SW_64KB_Z_X)
    {
        PAL_NEVER_CALLED();
    }

    if (stencilAddrSettings.swizzleMode != ADDR_SW_64KB_Z_X)
    {
        PAL_NEVER_CALLED();
    }

    m_pm4Cmds.dbZInfo.bits.SW_MODE       = AddrMgr2::GetHwSwizzleMode(depthAddrSettings.swizzleMode);
    m_pm4Cmds.dbStencilInfo.bits.SW_MODE = AddrMgr2::GetHwSwizzleMode(stencilAddrSettings.swizzleMode);

    m_pm4Cmds.dbZInfo.gfx10.FAULT_BEHAVIOR       = FAULT_ZERO;
    m_pm4Cmds.dbStencilInfo.gfx10.FAULT_BEHAVIOR = FAULT_ZERO;

    m_pm4Cmds.dbZInfo.gfx10.ITERATE_256          = m_pImage->GetIterate256(pDepthSubResInfo);
    m_pm4Cmds.dbStencilInfo.gfx10.ITERATE_256    = m_pImage->GetIterate256(pStencilSubResInfo);

    PAL_ASSERT(CountSetBits(DB_DEPTH_VIEW__SLICE_START_MASK) == DbDepthViewSliceStartMaskNumBits);
    PAL_ASSERT(CountSetBits(DB_DEPTH_VIEW__SLICE_MAX_MASK)   == DbDepthViewSliceMaxMaskNumBits);

    m_pm4Cmds.dbDepthView.gfx10.SLICE_START_HI = createInfo.baseArraySlice >> DbDepthViewSliceStartMaskNumBits;
    m_pm4Cmds.dbDepthView.gfx10.SLICE_MAX_HI   = sliceMax >> DbDepthViewSliceMaxMaskNumBits;

    const uint32 cbDbCachePolicy = m_device.Settings().cbDbCachePolicy;

    m_pm4Cmds.dbRmiL2CacheControl.u32All                = 0;
    m_pm4Cmds.dbRmiL2CacheControl.bits.Z_WR_POLICY      =
        (cbDbCachePolicy & Gfx10CbDbCachePolicyLruDepth)     ? CACHE_LRU_WR : CACHE_STREAM;
    m_pm4Cmds.dbRmiL2CacheControl.bits.S_WR_POLICY      =
        (cbDbCachePolicy & Gfx10CbDbCachePolicyLruStencil)   ? CACHE_LRU_WR : CACHE_STREAM;
    m_pm4Cmds.dbRmiL2CacheControl.bits.HTILE_WR_POLICY  =
        (cbDbCachePolicy & Gfx10CbDbCachePolicyLruHtile)     ? CACHE_LRU_WR : CACHE_STREAM;
    m_pm4Cmds.dbRmiL2CacheControl.bits.ZPCPSD_WR_POLICY =
        (cbDbCachePolicy & Gfx10CbDbCachePolicyLruOcclusion) ? CACHE_LRU_WR : CACHE_STREAM;
    m_pm4Cmds.dbRmiL2CacheControl.bits.Z_RD_POLICY      =
        (cbDbCachePolicy & Gfx10CbDbCachePolicyLruDepth)     ? CACHE_LRU_RD : CACHE_NOA;
    m_pm4Cmds.dbRmiL2CacheControl.bits.S_RD_POLICY      =
        (cbDbCachePolicy & Gfx10CbDbCachePolicyLruStencil)   ? CACHE_LRU_RD : CACHE_NOA;
    m_pm4Cmds.dbRmiL2CacheControl.bits.HTILE_RD_POLICY  =
        (cbDbCachePolicy & Gfx10CbDbCachePolicyLruHtile)     ? CACHE_LRU_RD : CACHE_NOA;

}

// =====================================================================================================================
// Writes the PM4 commands required to bind to depth/stencil slot. Returns the next unused DWORD in pCmdSpace.
uint32* Gfx10DepthStencilView::WriteCommands(
    ImageLayout depthLayout,   // Allowed usages/queues for the depth aspect. Implies compression state.
    ImageLayout stencilLayout, // Allowed usages/queues for the stencil aspect. Implies compression state.
    CmdStream*  pCmdStream,
    uint32*     pCmdSpace
    ) const
{
    return WriteCommandsInternal(depthLayout, stencilLayout, pCmdStream, pCmdSpace, m_pm4Cmds);
}

} // Gfx9
} // Pal
