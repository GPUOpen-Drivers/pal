/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/image.h"
#include "core/addrMgr/addrMgr3/addrMgr3.h"
#include "core/hw/gfxip/gfxImage.h"
#include "core/hw/gfxip/gfx12/gfx12CmdStream.h"
#include "core/hw/gfxip/gfx12/gfx12DepthStencilView.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12FormatInfo.h"
#include "core/hw/gfxip/gfx12/gfx12Image.h"
#include "core/hw/gfxip/gfx12/gfx12Metadata.h"
#include "palFormatInfo.h"

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
static ZFormat HwZFormat(
    ChNumFormat format)
{
    ZFormat zFormat = Z_INVALID;

    switch (format)
    {
    case ChNumFormat::X16_Unorm:
    case ChNumFormat::D16_Unorm_S8_Uint:
        zFormat = Z_16;
        break;
    case ChNumFormat::X32_Float:
    case ChNumFormat::D32_Float_S8_Uint:
        zFormat = Z_32_FLOAT;
        break;

    default:
        zFormat = Z_INVALID;
        break;
    }

    return zFormat;
}

// =====================================================================================================================
static StencilFormat HwSFormat(
    ChNumFormat format)
{
    StencilFormat sFormat = STENCIL_INVALID;

    switch (format)
    {
    case ChNumFormat::X8_Uint:
    case ChNumFormat::D16_Unorm_S8_Uint:
    case ChNumFormat::D32_Float_S8_Uint:
        sFormat = STENCIL_8;
        break;

    default:
        break;
    }

    return sFormat;
}

// =====================================================================================================================
DepthStencilView::DepthStencilView(
    const Device*                      pDevice,
    const DepthStencilViewCreateInfo&  createInfo,
    DepthStencilViewInternalCreateInfo internalCreateInfo,
    uint32                             viewId)
    :
    m_regs{},
    m_flags{},
    m_hizValidLayout{},
    m_hisValidLayout{},
    m_uniqueId(viewId),
    m_pImage(GetGfx12Image(createInfo.pImage))
{
    DsRegs::Init(m_regs);

    const Pal::Device&     palDevice     = *pDevice->Parent();
    const Pal::Image&      palImage      = *static_cast<const Pal::Image*>(createInfo.pImage);
    const Image&           gfx12Image    = *static_cast<const Image*>(palImage.GetGfxImage());
    const ImageCreateInfo& imgCreateInfo = palImage.GetImageCreateInfo();
    const auto*            pAddrMgr      = static_cast<const AddrMgr3::AddrMgr3*>(palDevice.GetAddrMgr());

    // Don't expect to need support for depth stencil views with non-locked VAs on Gfx12 hardware.
    PAL_ASSERT(createInfo.flags.imageVaLocked);
    PAL_ASSERT(palImage.GetBoundGpuMemory().IsBound());

    // We start with simple registers which describe the basic nature of this view.

    // The depth and stencil extents must always be equal so we can program this register using the mip0 extents of
    // whichever plane happens to come first.
    const SubResourceInfo* const pBaseSubresInfo = palImage.SubresourceInfo(BaseSubres(0));

    auto*const pDbDepthSizeXy = DsRegs::Get<mmDB_DEPTH_SIZE_XY, DB_DEPTH_SIZE_XY>(m_regs);
    pDbDepthSizeXy->bits.X_MAX = pBaseSubresInfo->extentElements.width  - 1;
    pDbDepthSizeXy->bits.Y_MAX = pBaseSubresInfo->extentElements.height - 1;

    auto*const pDbDepthView = DsRegs::Get<mmDB_DEPTH_VIEW, DB_DEPTH_VIEW>(m_regs);
    pDbDepthView->bits.SLICE_START = createInfo.baseArraySlice;
    pDbDepthView->bits.SLICE_MAX   = createInfo.arraySize + createInfo.baseArraySlice - 1;

    const bool zReadOnly = (createInfo.flags.readOnlyDepth   != 0);
    const bool sReadOnly = (createInfo.flags.readOnlyStencil != 0);

    auto*const pDbDepthView1 = DsRegs::Get<mmDB_DEPTH_VIEW1, DB_DEPTH_VIEW1>(m_regs);
    pDbDepthView1->bits.Z_READ_ONLY       = zReadOnly;
    pDbDepthView1->bits.STENCIL_READ_ONLY = sReadOnly;
    pDbDepthView1->bits.MIPID             = createInfo.mipLevel;

    // Now let's program resource state for the four planes this view can represent: depth, stencil, HiZ, HiS.

    // If the image physically has depth or stencil.
    const bool hasDepth   = palDevice.SupportsDepth(imgCreateInfo.swizzledFormat.format, imgCreateInfo.tiling);
    const bool hasStencil = palDevice.SupportsStencil(imgCreateInfo.swizzledFormat.format, imgCreateInfo.tiling);

    // If the view wants depth or stencil actually programmed.
    const bool isDepthView   = (createInfo.flags.stencilOnlyView == 0) && hasDepth;
    const bool isStencilView = (createInfo.flags.depthOnlyView   == 0) && hasStencil;

    m_flags.szValid = isDepthView && isStencilView;

    m_viewRange.numMips     = 1;
    m_viewRange.numPlanes   = isDepthView && isStencilView ? 2 : 1;
    m_viewRange.numSlices   = createInfo.arraySize;
    m_viewRange.startSubres = Subres((m_viewRange.numPlanes == 2) ? 0 : (isStencilView ? 1 : 0),
                                     createInfo.mipLevel,
                                     createInfo.baseArraySlice);

    // Some of the fields in DB_Z_INFO apply to stencil too. This is very confusing.
    auto*const pDbZInfo = DsRegs::Get<mmDB_Z_INFO, DB_Z_INFO>(m_regs);
    pDbZInfo->bits.NUM_SAMPLES = Log2(imgCreateInfo.samples);
    pDbZInfo->bits.MAXMIP      = imgCreateInfo.mipLevels - 1;

    // Note that when this isn't a depth view, we're implicitly setting FORMAT = Z_INVALID by zero-initializing
    // the register vector. If the DB sees Z_INVALID it decides "Z doesn't exist" which is exactly what we want.
    static_assert(Z_INVALID == 0);

    if (isDepthView)
    {
        // If depth exists it's always the first plane.
        constexpr SubresId BaseDepthId = BaseSubres(0);
        const gpusize      zBase256b   = gfx12Image.GetSubresource256BAddr(BaseDepthId);

        DsRegs::Get<mmDB_Z_WRITE_BASE, DB_Z_WRITE_BASE>(m_regs)->bits.BASE_256B     = LowPart(zBase256b);
        DsRegs::Get<mmDB_Z_READ_BASE,  DB_Z_READ_BASE>(m_regs)->bits.BASE_256B      = LowPart(zBase256b);
        DsRegs::Get<mmDB_Z_READ_BASE_HI, DB_Z_READ_BASE_HI>(m_regs)->bits.BASE_HI   = HighPart(zBase256b);
        DsRegs::Get<mmDB_Z_WRITE_BASE_HI, DB_Z_WRITE_BASE_HI>(m_regs)->bits.BASE_HI = HighPart(zBase256b);

        const SubResourceInfo*const pBaseDepthInfo = palImage.SubresourceInfo(BaseDepthId);
        const ZFormat               zFmt           = HwZFormat(pBaseDepthInfo->format.format);

        pDbZInfo->bits.TILE_SURFACE_ENABLE = 0;
        pDbZInfo->bits.FORMAT  = zFmt;
        pDbZInfo->bits.SW_MODE = gfx12Image.GetHwSwizzleMode(pBaseDepthInfo);

        pDbZInfo->bits.DECOMPRESS_ON_N_ZPLANES = 0;

        // Based on hardware documentation, it seems that for 16-bit unorm DB, we need to write -16 and for 24-bit
        // unorm DB, we need to write -24 to POLY_OFFSET_NEG_NUM_DB_BITS.
        //
        // Based on local tests, the observation is that for unorm DB (e.g. 24-bit unorm), HW uses rounding after
        // applying float to 24-bit unorm conversion where the formula should be u = round(f * (2^24 - 1)).
        //
        // For the polygon offset unit value, OpenGL spec states:
        //     "It is the smallest difference in window coordinate Z values that is guaranteed to remain distinct
        //      throughout polygon rasterization and in the depth buffer."
        //
        // The above spec makes it sound like the delta is 1/(2^24 - 1). If we do set POLY_OFFSET_NEG_NUM_DB_BITS to
        // -24, it seems that the HW applies a delta as 1/(2^24), which is a tiny bit smaller than the other one.
        // Therefore, when there is a float Z value f that can be converted by f * (2^24 - 1) to be x.5, if we request
        // a polygon offset unit of 1.0f, then the HW will do (f + 1/(2^24))*(2^24 - 1), that will be (x+1).4999...
        // So when x.5 and (x+1).4999... are both being rounded the result are both x+1 to this Z value f, the
        // polgon offset is not applied. This could be the reason we use -22 for 24-bit and -15 for 16-bit DB.
        auto*const pPaSuPolyOffsetDbFmtCntl =
            DsRegs::Get<mmPA_SU_POLY_OFFSET_DB_FMT_CNTL, PA_SU_POLY_OFFSET_DB_FMT_CNTL>(m_regs);

        if (createInfo.flags.absoluteDepthBias == 0)
        {
            const bool depthAsZ24 =
                (createInfo.flags.useHwFmtforDepthOffset == 0) && (imgCreateInfo.usageFlags.depthAsZ24 != 0);

            // NOTE: The client has indicated this Image has promoted 24bit depth to 32bits, we should set the negative
            //       sum bit as -24 and use fixed points format
            const bool reducePrecision = createInfo.flags.lowZplanePolyOffsetBits;

            if (depthAsZ24)
            {
                pPaSuPolyOffsetDbFmtCntl->bits.POLY_OFFSET_NEG_NUM_DB_BITS = uint8(reducePrecision ? -22 : -24);
            }
            else if (pDbZInfo->bits.FORMAT == Z_16)
            {
                pPaSuPolyOffsetDbFmtCntl->bits.POLY_OFFSET_NEG_NUM_DB_BITS = uint8(reducePrecision ? -15 : -16);
            }
            else
            {
                pPaSuPolyOffsetDbFmtCntl->bits.POLY_OFFSET_NEG_NUM_DB_BITS = uint8(-23);
            }

            pPaSuPolyOffsetDbFmtCntl->bits.POLY_OFFSET_DB_IS_FLOAT_FMT =
                ((pDbZInfo->bits.FORMAT == Z_32_FLOAT) && (depthAsZ24 == false)) ? 1 : 0;
        }
        else
        {
            pPaSuPolyOffsetDbFmtCntl->bits.POLY_OFFSET_NEG_NUM_DB_BITS = 0;
            pPaSuPolyOffsetDbFmtCntl->bits.POLY_OFFSET_DB_IS_FLOAT_FMT = 0;
        }
    }

    // Note that when this isn't a stencil view, we're implicitly setting FORMAT = STENCIL_INVALID by zero-initializing
    // the register vector. If the DB sees INVALID it decides "stencil doesn't exist" which is exactly what we want.
    static_assert(STENCIL_INVALID == 0);

    if (isStencilView)
    {
        // Stencil is always the last plane so we need to use plane=1 if the image's format has depth.
        const SubresId baseStencilId = BaseSubres(hasDepth ? 1 : 0);
        const gpusize  sBase256b     = gfx12Image.GetSubresource256BAddr(baseStencilId);

        DsRegs::Get<mmDB_STENCIL_WRITE_BASE, DB_STENCIL_WRITE_BASE>(m_regs)->bits.BASE_256B     = LowPart(sBase256b);
        DsRegs::Get<mmDB_STENCIL_READ_BASE, DB_STENCIL_READ_BASE>(m_regs)->bits.BASE_256B       = LowPart(sBase256b);
        DsRegs::Get<mmDB_STENCIL_READ_BASE_HI, DB_STENCIL_READ_BASE_HI>(m_regs)->bits.BASE_HI   = HighPart(sBase256b);
        DsRegs::Get<mmDB_STENCIL_WRITE_BASE_HI, DB_STENCIL_WRITE_BASE_HI>(m_regs)->bits.BASE_HI = HighPart(sBase256b);

        const SubResourceInfo* pBaseStencilInfo = palImage.SubresourceInfo(baseStencilId);

        auto* pDbStencilInfo = DsRegs::Get<mmDB_STENCIL_INFO, DB_STENCIL_INFO>(m_regs);
        pDbStencilInfo->bits.TILE_STENCIL_DISABLE = 1;
        pDbStencilInfo->bits.FORMAT  = HwSFormat(pBaseStencilInfo->format.format);
        pDbStencilInfo->bits.SW_MODE = gfx12Image.GetHwSwizzleMode(pBaseStencilInfo);
    }

    // Now program the HiZ and HiS state. Like the depth and stencil state we should only program this if the metadata
    // sub-images exist and are enabled in this view.
    const bool   hasHiSZ    = gfx12Image.HasHiSZ();
    const HiSZ*  pHiSZ      = gfx12Image.GetHiSZ();
    const bool   hiZEnabled = hasHiSZ && isDepthView   && pHiSZ->HiZEnabled();
    const bool   hiSEnabled = hasHiSZ && isStencilView && pHiSZ->HiSEnabled();

    if (hasHiSZ)
    {
        m_flags.hiSZEnabled = 1;

        const Extent3d baseExtent = pHiSZ->GetBaseExtent();

        if (hiZEnabled)
        {
            const gpusize hiZBase256b = pHiSZ->Get256BAddrSwizzled(HiSZType::HiZ);
            DsRegs::Get<mmPA_SC_HIZ_BASE, PA_SC_HIZ_BASE>(m_regs)->bits.BASE_256B         = LowPart(hiZBase256b);
            DsRegs::Get<mmPA_SC_HIZ_BASE_EXT, PA_SC_HIZ_BASE_EXT>(m_regs)->bits.BASE_256B = HighPart(hiZBase256b);

            auto*const pPaScHiZInfo = DsRegs::Get<mmPA_SC_HIZ_INFO, PA_SC_HIZ_INFO>(m_regs);
            pPaScHiZInfo->bits.SURFACE_ENABLE = 1;
            pPaScHiZInfo->bits.FORMAT         = 0; // 0 = unorm16
            pPaScHiZInfo->bits.SW_MODE        = pAddrMgr->GetHwSwizzleMode(pHiSZ->GetSwizzleMode(HiSZType::HiZ));
            // Default to 0 based on preferred settings of previous generation HWLs.
            pPaScHiZInfo->bits.DST_OUTSIDE_ZERO_TO_ONE = 0;

            auto*const pPaScHiZSizeXY = DsRegs::Get<mmPA_SC_HIZ_SIZE_XY, PA_SC_HIZ_SIZE_XY>(m_regs);
            pPaScHiZSizeXY->bits.X_MAX = baseExtent.width  - 1;
            pPaScHiZSizeXY->bits.Y_MAX = baseExtent.height - 1;

            // HiZ implementation requires 2-pixel tile surface alignment, thus bit 0 must be 1.
            PAL_ASSERT(TestAnyFlagSet(pPaScHiZSizeXY->bits.X_MAX, 0x1));
            PAL_ASSERT(TestAnyFlagSet(pPaScHiZSizeXY->bits.Y_MAX, 0x1));

            m_hizValidLayout = gfx12Image.GetHiSZValidLayout(0);
        }

        if (hiSEnabled)
        {
            const gpusize hiSBase256b = pHiSZ->Get256BAddrSwizzled(HiSZType::HiS);
            DsRegs::Get<mmPA_SC_HIS_BASE, PA_SC_HIS_BASE>(m_regs)->bits.BASE_256B         = LowPart(hiSBase256b);
            DsRegs::Get<mmPA_SC_HIS_BASE_EXT, PA_SC_HIS_BASE_EXT>(m_regs)->bits.BASE_256B = HighPart(hiSBase256b);

            auto*const pPaScHiSInfo = DsRegs::Get<mmPA_SC_HIS_INFO, PA_SC_HIS_INFO>(m_regs);
            pPaScHiSInfo->bits.SURFACE_ENABLE = 1;
            pPaScHiSInfo->bits.SW_MODE        = pAddrMgr->GetHwSwizzleMode(pHiSZ->GetSwizzleMode(HiSZType::HiS));

            auto*const pPaScHiSSizeXY = DsRegs::Get<mmPA_SC_HIS_SIZE_XY, PA_SC_HIS_SIZE_XY>(m_regs);
            pPaScHiSSizeXY->bits.X_MAX = baseExtent.width  - 1;
            pPaScHiSSizeXY->bits.Y_MAX = baseExtent.height - 1;

            // HiS implementation requires 2-pixel tile surface alignment, thus bit 0 must be 1.
            PAL_ASSERT(TestAnyFlagSet(pPaScHiSSizeXY->bits.X_MAX, 0x1));
            PAL_ASSERT(TestAnyFlagSet(pPaScHiSSizeXY->bits.Y_MAX, 0x1));

            m_hisValidLayout = gfx12Image.GetHiSZValidLayout(gfx12Image.GetStencilPlane());
        }
    }

    // The rest of this function covers the various control/override registers which are harder to categorize and
    // usually pull state from many different locations.
    const bool enableClientCompression =
        gfx12Image.EnableClientCompression(internalCreateInfo.flags.disableClientCompression);

    auto*const pDbRenderControl = DsRegs::Get<mmDB_RENDER_CONTROL, DB_RENDER_CONTROL>(m_regs);
    pDbRenderControl->bits.DEPTH_COMPRESS_DISABLE   = enableClientCompression ? 0 : 1;
    pDbRenderControl->bits.STENCIL_COMPRESS_DISABLE = enableClientCompression ? 0 : 1;
    pDbRenderControl->bits.STENCIL_CLEAR_ENABLE     = internalCreateInfo.flags.isStencilClear;

    // No DB to CB copy support on gfx12.
    PAL_ASSERT((internalCreateInfo.flags.isDepthCopy | internalCreateInfo.flags.isStencilCopy) == 0);

    const Gfx12PalSettings& gfx12Settings = pDevice->Settings();

    // The user mode driver should generally set the OREO_MODE field to OPAQUE_THEN_BLEND for best performance.
    // Setting to BLEND is a fail-safe that should work for all cases
    pDbRenderControl->bits.OREO_MODE = gfx12Settings.oreoModeControl;

    if (gfx12Settings.waNoOpaqueOreo && (pDbRenderControl->bits.OREO_MODE == OMODE_O_THEN_B))
    {
        pDbRenderControl->bits.OREO_MODE = OMODE_BLEND;
    }

    // The FORCE_OREO_MODE is intended only for workarounds and should otherwise be set to 0
    pDbRenderControl->bits.FORCE_OREO_MODE = gfx12Settings.forceOreoMode;

    // If 1, forces DB to make every wave conflict with the prior wave. Use only for debugging.
    pDbRenderControl->bits.FORCE_EXPORT_ORDER = gfx12Settings.forceExportOrderControl;

    auto*const pDbRenderOverride = DsRegs::Get<mmDB_RENDER_OVERRIDE, DB_RENDER_OVERRIDE>(m_regs);
    pDbRenderOverride->bits.FORCE_HIZ_ENABLE  = hiZEnabled ? FORCE_OFF : FORCE_DISABLE;
    pDbRenderOverride->bits.FORCE_HIS_ENABLE0 = hiSEnabled ? FORCE_OFF : FORCE_DISABLE;
    pDbRenderOverride->bits.FORCE_HIS_ENABLE1 = hiSEnabled ? FORCE_OFF : FORCE_DISABLE;

    const bool isResummarize = (createInfo.flags.resummarizeHiZ != 0);

    if (isResummarize)
    {
        pDbRenderOverride->bits.FORCE_Z_VALID           = (zReadOnly == false);
        pDbRenderOverride->bits.FORCE_STENCIL_VALID     = (sReadOnly == false);
        pDbRenderOverride->bits.NOOP_CULL_DISABLE       = 1;
        pDbRenderOverride->bits.DISABLE_TILE_RATE_TILES = 1;
    }

    if (gfx12Settings.waDbForceStencilRead)
    {
        pDbRenderOverride->bits.FORCE_STENCIL_READ = 1;
    }

    auto*const pDbRenderOverride2 = DsRegs::Get<mmDB_RENDER_OVERRIDE2, DB_RENDER_OVERRIDE2>(m_regs);
    // For 4xAA and 8xAA need to decompress on flush for better performance
    pDbRenderOverride2->bits.DECOMPRESS_Z_ON_FLUSH       = (imgCreateInfo.samples > 2) ? 1 : 0;
    pDbRenderOverride2->bits.DISABLE_COLOR_ON_VALIDATION = gfx12Settings.dbDisableColorOnValidation;

    // All gfx12 HW should support VRS so we program CENTROID_COMPUTATION_MODE unconditionally.
    PAL_ASSERT(palDevice.ChipProperties().gfxip.supportsVrs);

    //   For centroid computation you need to set DB_RENDER_OVERRIDE2::CENTROID_COMPUTATION_MODE to pick
    //   correct sample for centroid, which per DX12 spec is defined as the first covered sample. This
    //   means that it should use "2: Choose the sample with the smallest {~pixel_num, sample_id} as
    //   centroid, for all VRS rates"
    pDbRenderOverride2->bits.CENTROID_COMPUTATION_MODE = 2; // SmallestNotPixAll

    auto*const pPaScHiSZRenderOverride = DsRegs::Get<mmPA_SC_HISZ_RENDER_OVERRIDE, PA_SC_HISZ_RENDER_OVERRIDE>(m_regs);
    pPaScHiSZRenderOverride->bits.FORCE_HIZ_ENABLE        = hiZEnabled ? FORCE_OFF : FORCE_DISABLE;
    pPaScHiSZRenderOverride->bits.FORCE_HIS_ENABLE        = hiSEnabled ? FORCE_OFF : FORCE_DISABLE;
    pPaScHiSZRenderOverride->bits.DISABLE_TILE_RATE_TILES = isResummarize;

    // Verify that we can cast dsvCompressionMode to CompressionMode.
    static_assert((uint32(CompressionMode::Default)                == DsvCompressionDefault)                &&
                  (uint32(CompressionMode::ReadEnableWriteEnable)  == DsvCompressionReadEnableWriteEnable)  &&
                  (uint32(CompressionMode::ReadEnableWriteDisable) == DsvCompressionReadEnableWriteDisable));

    CompressionMode finalCompressionMode = static_cast<CompressionMode>(gfx12Settings.dsvCompressionMode);

    if (finalCompressionMode == CompressionMode::Default)
    {
        const GpuMemory*const pGpuMemory = palImage.GetBoundGpuMemory().Memory();

        finalCompressionMode = pDevice->GetImageViewCompressionMode(createInfo.compressionMode,
                                                                    imgCreateInfo.compressionMode,
                                                                    pGpuMemory);
    }

    RbCompressionMode compressionMode = RbCompressionMode::Default;

    switch (finalCompressionMode)
    {
    case CompressionMode::Default:
    case CompressionMode::ReadEnableWriteEnable:
        compressionMode = RbCompressionMode::Default;
        break;
    case CompressionMode::ReadEnableWriteDisable:
        compressionMode = RbCompressionMode::CompressWriteDisable;
        break;
    case CompressionMode::ReadBypassWriteDisable:
        compressionMode = RbCompressionMode::ReadBypassWriteDisable;
        break;
    default:
        PAL_NEVER_CALLED();
    }

    auto*const pDbGl1InterfaceControl = DsRegs::Get<mmDB_GL1_INTERFACE_CONTROL, DB_GL1_INTERFACE_CONTROL>(m_regs);
    pDbGl1InterfaceControl->bits.Z_COMPRESSION_MODE         = uint32(compressionMode);
    pDbGl1InterfaceControl->bits.STENCIL_COMPRESSION_MODE   = uint32(compressionMode);
    pDbGl1InterfaceControl->bits.OCCLUSION_COMPRESSION_MODE = uint32(RbCompressionMode::CompressWriteDisable);

    // CopyRegPairsToCmdSpace won't write the high address registers if they're all zero. This saves a few register
    // writes per bind if the app happens to stick to a small virtual memory space.
    if ((m_regs[DsRegs::Index(mmDB_Z_READ_BASE_HI)].value        != 0) ||
        (m_regs[DsRegs::Index(mmDB_STENCIL_READ_BASE_HI)].value  != 0) ||
        (m_regs[DsRegs::Index(mmDB_Z_WRITE_BASE_HI)].value       != 0) ||
        (m_regs[DsRegs::Index(mmDB_STENCIL_WRITE_BASE_HI)].value != 0) ||
        (m_regs[DsRegs::Index(mmPA_SC_HIS_BASE_EXT)].value       != 0) ||
        (m_regs[DsRegs::Index(mmPA_SC_HIZ_BASE_EXT)].value       != 0))
    {
        m_flags.hasNonZeroHighBaseBits = true;
    }
}

// =====================================================================================================================
// Returns the 2D pixel extents of the depth stencil view.
Extent2d DepthStencilView::Extent() const
{
    const auto& dbDepthSizeXy = DsRegs::GetC<mmDB_DEPTH_SIZE_XY, DB_DEPTH_SIZE_XY>(m_regs);
    const Extent2d extent =
    {
        (dbDepthSizeXy.bits.X_MAX + 1u),
        (dbDepthSizeXy.bits.Y_MAX + 1u)
    };

    return extent;
}

// =====================================================================================================================
uint32 DepthStencilView::OverrideHiZHiSEnable(
    bool              enable,
    DB_SHADER_CONTROL dbShaderControl,
    bool              noForceReZ,
    uint32*           pCmdSpace
    ) const
{
    PA_SC_HIZ_INFO paScHiZInfo = { .u32All = m_regs[DsRegs::Index(mmPA_SC_HIZ_INFO)].value };
    PA_SC_HIS_INFO paScHiSInfo = { .u32All = m_regs[DsRegs::Index(mmPA_SC_HIS_INFO)].value };

    if (enable == false)
    {
        paScHiZInfo.bits.SURFACE_ENABLE = 0;
        paScHiSInfo.bits.SURFACE_ENABLE = 0;
        dbShaderControl.bits.Z_ORDER = EARLY_Z_THEN_RE_Z;
    }
    // Else keep original state designated by creation time.

    const RegisterValuePair regs[] =
    {
        {.offset = mmPA_SC_HIZ_INFO - CONTEXT_SPACE_START, .value = paScHiZInfo.u32All },
        {.offset = mmPA_SC_HIS_INFO - CONTEXT_SPACE_START, .value = paScHiSInfo.u32All },
        {.offset = mmDB_SHADER_CONTROL - CONTEXT_SPACE_START, .value = dbShaderControl.u32All },
    };

    const uint32 numRegs = (noForceReZ) ? ArrayLen32(regs) - 1 : ArrayLen32(regs);

    void* pThrowAway;
    const size_t pktSizeDwords = CmdUtil::BuildSetContextPairsHeader(numRegs, &pThrowAway, pCmdSpace);

    memcpy(&pCmdSpace[1], &regs[0], numRegs * sizeof(RegisterValuePair));

    return uint32(pktSizeDwords);
}

// =====================================================================================================================
uint32* DepthStencilView::CopyRegPairsToCmdSpace(
    ImageLayout depthLayout,
    ImageLayout stencilLayout,
    uint32*     pCmdSpace,
    bool*       pWriteCbDbHighBaseRegs
    ) const
{
    static_assert(DsRegs::Size() == DsRegs::NumContext(), "Only expecting context registers.");
    static_assert((DsRegs::Index(mmDB_Z_READ_BASE_HI)        == DsRegs::Size() - 6) &&
                  (DsRegs::Index(mmDB_STENCIL_READ_BASE_HI)  == DsRegs::Size() - 5) &&
                  (DsRegs::Index(mmDB_Z_WRITE_BASE_HI)       == DsRegs::Size() - 4) &&
                  (DsRegs::Index(mmDB_STENCIL_WRITE_BASE_HI) == DsRegs::Size() - 3) &&
                  (DsRegs::Index(mmPA_SC_HIS_BASE_EXT)       == DsRegs::Size() - 2) &&
                  (DsRegs::Index(mmPA_SC_HIZ_BASE_EXT)       == DsRegs::Size() - 1),
                  "Unexpected indices for DSV High Base regs.");

    RegisterValuePair regs[DsRegs::Size()];
    memcpy(&regs, &m_regs, sizeof(m_regs));

    const DepthStencilHiSZState hizState = ImageLayoutToDepthStencilHiSZState(m_hizValidLayout, depthLayout);
    const DepthStencilHiSZState hisState = ImageLayoutToDepthStencilHiSZState(m_hisValidLayout, stencilLayout);

    auto*const pDbRenderOverride       = DsRegs::Get<mmDB_RENDER_OVERRIDE, DB_RENDER_OVERRIDE>(regs);
    auto*const pPaScHiSZRenderOverride = DsRegs::Get<mmPA_SC_HISZ_RENDER_OVERRIDE, PA_SC_HISZ_RENDER_OVERRIDE>(regs);

    if (hizState == DepthStencilNoHiSZ)
    {
        pDbRenderOverride->bits.FORCE_HIZ_ENABLE       = FORCE_DISABLE;
        pPaScHiSZRenderOverride->bits.FORCE_HIZ_ENABLE = FORCE_DISABLE;
    }
    if (hisState == DepthStencilNoHiSZ)
    {
        pDbRenderOverride->bits.FORCE_HIS_ENABLE0      = FORCE_DISABLE;
        pDbRenderOverride->bits.FORCE_HIS_ENABLE1      = FORCE_DISABLE;
        pPaScHiSZRenderOverride->bits.FORCE_HIS_ENABLE = FORCE_DISABLE;
    }

    // Despite not having a stencil/depth attachment, the case enable the stencil/depth test in a way
    // that will not preserve the stencil/depth clear value no matter if the test passes or not.
    // here we set stencil/depth format to 0 to invalid stencil/depth test for such case.
    if ((stencilLayout.usages == 0) && TestAnyFlagSet(depthLayout.usages, LayoutDepthStencilTarget))
    {
        DsRegs::Get<mmDB_STENCIL_INFO, DB_STENCIL_INFO>(regs)->bits.FORMAT = STENCIL_INVALID;
    }
    if ((depthLayout.usages == 0) && TestAnyFlagSet(stencilLayout.usages, LayoutDepthStencilTarget))
    {
        DsRegs::Get<mmDB_Z_INFO, DB_Z_INFO>(regs)->bits.FORMAT = Z_INVALID;
    }

    // Note that we must never set *pWriteCbDbHighBaseRegs to false! This flag must be "sticky" because we need to
    // write the high registers back to zero if some unrelated past DSV had non-zero high bits.
    if (m_flags.hasNonZeroHighBaseBits)
    {
        *pWriteCbDbHighBaseRegs = true;
    }

    const uint32 numRegPairs = *pWriteCbDbHighBaseRegs ? DsRegs::Size() : DsRegs::Size() - 6;

    memcpy(pCmdSpace, regs, sizeof(RegisterValuePair) * numRegPairs);

    return pCmdSpace + (numRegPairs * sizeof(RegisterValuePair) / sizeof(uint32));
}

// =====================================================================================================================
uint32* DepthStencilView::CopyNullRegPairsToCmdSpace(
    uint32* pCmdSpace,
    bool    writeMinimumRegSet)
{
    if (writeMinimumRegSet)
    {
        return CopyNullRegPairsToCmdSpaceInternal<NullDsRegs>(pCmdSpace);
    }
    else
    {
        return CopyNullRegPairsToCmdSpaceInternal<DsRegs>(pCmdSpace);
    }
}

// =====================================================================================================================
template<class RegType>
uint32* DepthStencilView::CopyNullRegPairsToCmdSpaceInternal(
    uint32* pCmdSpace)
{
    static_assert(RegType::Size() == RegType::NumContext(), "Only expecting context registers.");

    constexpr uint32 NumRegPairs = RegType::Size();

    RegisterValuePair regs[NumRegPairs];
    RegType::Init(regs);

    //   For centroid computation you need to set DB_RENDER_OVERRIDE2::CENTROID_COMPUTATION_MODE to pick
    //   correct sample for centroid, which per DX12 spec is defined as the first covered sample. This
    //   means that it should use "2: Choose the sample with the smallest {~pixel_num, sample_id} as
    //   centroid, for all VRS rates"
    // Copied from pal\src\core\hw\gfxip\gfx9\gfx9UniversalCmdBuffer.cpp
    (RegType::template Get<mmDB_RENDER_OVERRIDE2, DB_RENDER_OVERRIDE2>(regs))->bits.CENTROID_COMPUTATION_MODE = 2;

    auto* pPaSuPolyOffsetDbFmtCntl =
        (RegType::template Get<mmPA_SU_POLY_OFFSET_DB_FMT_CNTL, PA_SU_POLY_OFFSET_DB_FMT_CNTL>(regs));

    // As DX spec, the default format for depth bias with no depth buffer bound is UNORM24.
    pPaSuPolyOffsetDbFmtCntl->bits.POLY_OFFSET_DB_IS_FLOAT_FMT = 0;
    pPaSuPolyOffsetDbFmtCntl->bits.POLY_OFFSET_NEG_NUM_DB_BITS = uint8(-24);

    memcpy(pCmdSpace, regs, sizeof(regs));

    return pCmdSpace + (NumRegPairs * sizeof(RegisterValuePair) / sizeof(uint32));
}

// =====================================================================================================================
bool DepthStencilView::Equals(
    const DepthStencilView* pOther
    ) const
{
    return ((pOther != nullptr) && (m_uniqueId == pOther->m_uniqueId));
}

} // namespace Gfx12
} // namespace Pal
