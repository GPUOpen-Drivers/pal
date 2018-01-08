/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/universalCmdBuffer.h"
#include "core/hw/gfxip/gfx6/g_gfx6PalSettings.h"
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6ColorBlendState.h"
#include "core/hw/gfxip/gfx6/gfx6ColorTargetView.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6FormatInfo.h"
#include "core/hw/gfxip/gfx6/gfx6Image.h"
#include "palFormatInfo.h"

using namespace Util;
using namespace Pal::Formats::Gfx6;

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
ColorTargetView::ColorTargetView()
    :
    m_pImage(nullptr)
{
    m_flags.u32All = 0;

    memset(&m_subresource, 0, sizeof(m_subresource));
    memset(&m_pm4CmdsCompressed, 0, sizeof(m_pm4CmdsCompressed));
    memset(&m_pm4CmdsDecompressed, 0, sizeof(m_pm4CmdsDecompressed));
}

// =====================================================================================================================
// Builds the PM4 packet headers for an image of PM4 commands used to write this View object to hardware.
void ColorTargetView::BuildPm4Headers(
    const Device&          device,         // Gfx6 device
    bool                   useCompression, // If true, enables color compression if available
    ColorTargetViewPm4Img* pPm4Img         // Output PM4 image to initialize packet headers for
    ) const
{
    PAL_ASSERT(pPm4Img != nullptr);

    const CmdUtil& cmdUtil = device.CmdUtil();

    // 1st PM4 set data packet: sets the context registers CB_COLOR*_BASE through
    // CB_COLOR*_VIEW.
    //
    // NOTE: The register offset will be updated at bind-time to reflect the actual slot this
    // View is being bound to.
    pPm4Img->spaceNeeded = cmdUtil.BuildSetSeqContextRegs(mmCB_COLOR0_BASE,
                                                          mmCB_COLOR0_VIEW,
                                                          &pPm4Img->hdrCbColorBaseToView);

    // 2nd PM4 packet, a register read/modify/write of CB_COLOR*_INFO.
    // The real packet will be created later, we just need to get the size here.
    //
    // NOTE: The register offset will be updated at bind-time to reflect the actual slot this
    // View is being bound to.
    pPm4Img->spaceNeeded += CmdUtil::GetContextRegRmwSize();

    // 3rd PM4 set data packet: sets the context registers CB_COLOR*_ATTRIB through
    // CB_COLOR*_FMASK_SLICE.
    //
    // NOTE: The register offset will be updated at bind-time to reflect the actual slot this
    // view is being bound to.
    pPm4Img->spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmCB_COLOR0_ATTRIB,
                                                           mmCB_COLOR0_FMASK_SLICE,
                                                           &pPm4Img->hdrCbColorAttribToFmaskSlice);

    // 4th PM4 set data packet: sets the context registers PA_SC_GENERIC_SCISSOR_TL and
    // PA_SC_GENERIC_SCISSOR_BR.
    pPm4Img->spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmPA_SC_GENERIC_SCISSOR_TL,
                                                           mmPA_SC_GENERIC_SCISSOR_BR,
                                                           &pPm4Img->hdrPaScGenericScissorTlBr);

    // 5th PM4 set data packet:  sets the DCC base register
    //
    // NOTE: The register offset will be updated at bind-time to reflect the actual slot
    //       this view is being bound to.
    //
    // NOTE: This register is an unused location on pre-VI ASICs; writing to it doesn't
    //       do anything.
    pPm4Img->spaceNeeded += cmdUtil.BuildSetOneContextReg(mmCB_COLOR0_DCC_BASE__VI,
                                                          &pPm4Img->hdrCbColorDccBase);

    if (useCompression)
    {
        if (m_flags.fastClearSupported)
        {
            // If the parent Image supports fast clears, then we need to add a LOAD_CONTEXT_REG packet to load the
            // image's fast-clear metadata.
            //
            // NOTE: We do not know the GPU virtual address of the metadata until bind-time.
            constexpr uint32 RegCount = (mmCB_COLOR0_CLEAR_WORD1 - mmCB_COLOR0_CLEAR_WORD0 + 1);

            if (m_flags.usesLoadRegIndexPkt != 0)
            {
                pPm4Img->spaceNeeded += cmdUtil.BuildLoadContextRegsIndex<true>(0,
                                                                                mmCB_COLOR0_CLEAR_WORD0,
                                                                                RegCount,
                                                                                &pPm4Img->loadMetaDataIndex);
            }
            else
            {
                pPm4Img->spaceNeeded += cmdUtil.BuildLoadContextRegs(0,
                                                                     mmCB_COLOR0_CLEAR_WORD0,
                                                                     RegCount,
                                                                     &pPm4Img->loadMetaData);
            }
        }

        if (m_flags.dccCompressionEnabled && m_flags.hasDccStateMetaData)
        {
            // Our PM4 image layout assumes that the loadMetaData packet is always used in this case.
            PAL_ASSERT(m_flags.fastClearSupported);

            // We also need to update the DCC state metadata when compression is enabled.
            //
            // NOTE: We do not know the GPU virtual address of the metadata until bind-time.
            constexpr gpusize NumDwords = sizeof(pPm4Img->mipMetaData) / sizeof(uint32);

            pPm4Img->spaceNeeded += cmdUtil.BuildWriteData(0,
                                                           NumDwords,
                                                           WRITE_DATA_ENGINE_PFP,
                                                           WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                                           true,
                                                           nullptr,
                                                           PredDisable,
                                                           &pPm4Img->hdrMipMetaData);
        }
    }
}

// =====================================================================================================================
// Finalizes the PM4 packet image by setting up the register values used to write this View object to hardware.
void ColorTargetView::InitRegisters(
    const Device&                            device,         // Gfx6 device
    const ColorTargetViewCreateInfo&         createInfo,     // Creation parameters
    const ColorTargetViewInternalCreateInfo& internalInfo,   // Internal view creation parameters
    bool                                     useCompression, // If true, enables color compression if available
    ColorTargetViewPm4Img*                   pPm4Img         // Output PM4 image to initialize packet headers for
    ) const
{
    const auto& settings = device.Settings();

    // By default, assume linear general tiling and no Fmask texture fetches.
    int32  baseTileIndex        = TileIndexLinearGeneral;
    uint32 baseBankHeight       = 0;
    bool   fMaskTexFetchAllowed = false;

    // Most register values are simple to compute but vary based on whether or not this is a buffer view. Let's set
    // them all up-front before we get on to the harder register values.
    if (m_flags.isBufferView)
    {
        PAL_ASSERT(createInfo.bufferInfo.pGpuMemory != nullptr);

        // The buffer virtual address is simply "offset" pixels from the start of the GPU memory's virtual address.
        const gpusize bufferOffset = createInfo.bufferInfo.offset *
                                     Formats::BytesPerPixel(createInfo.swizzledFormat.format);
        const gpusize bufferAddr   = createInfo.bufferInfo.pGpuMemory->Desc().gpuVirtAddr + bufferOffset;

        // Convert to a 256-bit aligned base address and a base offset. Note that we don't need to swizzle the base
        // address because buffers aren't macro tiled.
        const gpusize baseOffset = bufferAddr & 0xFF;
        const gpusize baseAddr   = bufferAddr & (~0xFF);

        pPm4Img->cbColorBase.bits.BASE_256B = Get256BAddrLo(baseAddr);

        // The CI addressing doc states that the CB requires linear general surfaces pitches to be 8-element aligned.
        const uint32 alignedExtent = Pow2Align(createInfo.bufferInfo.extent, 8);

        pPm4Img->cbColorPitch.bits.TILE_MAX = (alignedExtent / TileWidth)  - 1;
        pPm4Img->cbColorSlice.bits.TILE_MAX = (alignedExtent / TilePixels) - 1;

        // The view slice_start is overloaded to specify the base offset.
        pPm4Img->cbColorView.bits.SLICE_START = baseOffset;
        pPm4Img->cbColorView.bits.SLICE_MAX   = 0;

        pPm4Img->cbColorAttrib.bits.TILE_MODE_INDEX   = baseTileIndex;
        pPm4Img->cbColorAttrib.bits.FORCE_DST_ALPHA_1 = Formats::HasUnusedAlpha(createInfo.swizzledFormat) ? 1 : 0;
        pPm4Img->cbColorAttrib.bits.NUM_SAMPLES       = 0;
        pPm4Img->cbColorAttrib.bits.NUM_FRAGMENTS     = 0;

        pPm4Img->paScGenericScissorTl.bits.WINDOW_OFFSET_DISABLE = true;
        pPm4Img->paScGenericScissorTl.bits.TL_X = 0;
        pPm4Img->paScGenericScissorTl.bits.TL_Y = 0;
        pPm4Img->paScGenericScissorBr.bits.BR_X = createInfo.bufferInfo.extent;
        pPm4Img->paScGenericScissorBr.bits.BR_Y = 1;
    }
    else
    {
        // Override the three variables defined above.
        const SubResourceInfo*const    pSubResInfo     = m_pImage->Parent()->SubresourceInfo(m_subresource);
        const AddrMgr1::TileInfo*const pTileInfo       = AddrMgr1::GetTileInfo(m_pImage->Parent(), m_subresource);
        const ImageCreateInfo&         imageCreateInfo = m_pImage->Parent()->GetImageCreateInfo();
        const bool                     imgIsBc         = Formats::IsBlockCompressed(imageCreateInfo.swizzledFormat.format);

        // Check if we can keep fmask in a compressed state and avoid corresponding fmask decompression
        fMaskTexFetchAllowed = m_pImage->IsComprFmaskShaderReadable(pSubResInfo);

        const Gfx6::Device* pGfxDevice = static_cast<Gfx6::Device*>(m_pImage->Parent()->GetDevice()->GetGfxDevice());

        baseTileIndex  = (internalInfo.flags.depthStencilCopy == 1) ?
            pGfxDevice->OverridedTileIndexForDepthStencilCopy(pTileInfo->tileIndex) : pTileInfo->tileIndex;

        baseBankHeight = pTileInfo->bankHeight;

        // NOTE: The color base address will be determined later, we don't need to do anything here.

        Extent3d extent       = pSubResInfo->extentTexels;
        Extent3d actualExtent = pSubResInfo->actualExtentTexels;

        // The view should be in terms of texels except in the below cases when we're operating in terms of elements:
        // 1. Viewing a compressed image in terms of blocks. For BC images elements are blocks, so if the caller gave
        //    us an uncompressed view format we assume they want to view blocks.
        // 2. Copying to an "expanded" format (e.g., R32G32B32). In this case we can't do native format writes so we're
        //    going to write each element independently. The trigger for this case is a mismatched bpp.
        // 3. Viewing a YUV-packed image with a non-YUV-packed format when the view format is allowed for view formats
        //    with twice the bpp. In this case, the effective width of the view is half that of the base image.
        // 4. Viewing a YUV planar Image. The view must be associated with a single plane. Since all planes of an array
        //    slice are packed together for YUV formats, we need to tell the CB hardware to "skip" the other planes if
        //    the view either spans multiple array slices or starts at a nonzero array slice.
        if (imgIsBc || (pSubResInfo->bitsPerTexel != Formats::BitsPerPixel(createInfo.swizzledFormat.format)))
        {
            extent       = pSubResInfo->extentElements;
            actualExtent = pSubResInfo->actualExtentElements;
        }

        if (Formats::IsYuvPacked(pSubResInfo->format.format)                  &&
            (Formats::IsYuvPacked(createInfo.swizzledFormat.format) == false) &&
            ((pSubResInfo->bitsPerTexel << 1) == Formats::BitsPerPixel(createInfo.swizzledFormat.format)))
        {
            // Changing how we interpret the bits-per-pixel of the subresource wreaks havoc with any tile swizzle pattern
            // used. This will only work for linear-tiled Images.
            PAL_ASSERT(m_pImage->IsSubResourceLinear(m_subresource));

            extent.width       >>= 1;
            actualExtent.width >>= 1;
        }
        else if (Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format) &&
                 ((createInfo.imageInfo.arraySize > 1) || (createInfo.imageInfo.baseSubRes.arraySlice != 0)))
        {
            m_pImage->PadYuvPlanarViewActualExtent(m_subresource, &actualExtent);
        }

        pPm4Img->cbColorPitch.bits.TILE_MAX = (actualExtent.width / TileWidth) - 1;
        pPm4Img->cbColorSlice.bits.TILE_MAX = (actualExtent.width * actualExtent.height / TilePixels) - 1;

        if ((createInfo.flags.zRangeValid == 1) && (imageCreateInfo.imageType == ImageType::Tex3d))
        {
            pPm4Img->cbColorView.bits.SLICE_START = createInfo.zRange.offset;
            pPm4Img->cbColorView.bits.SLICE_MAX   = (createInfo.zRange.offset + createInfo.zRange.extent - 1);
        }
        else
        {
            const uint32 baseArraySlice = createInfo.imageInfo.baseSubRes.arraySlice;
            pPm4Img->cbColorView.bits.SLICE_START = baseArraySlice;
            pPm4Img->cbColorView.bits.SLICE_MAX   = (baseArraySlice + createInfo.imageInfo.arraySize - 1);
        }
        pPm4Img->cbColorAttrib.bits.TILE_MODE_INDEX   = baseTileIndex;
        pPm4Img->cbColorAttrib.bits.FORCE_DST_ALPHA_1 = Formats::HasUnusedAlpha(createInfo.swizzledFormat) ? 1 : 0;
        pPm4Img->cbColorAttrib.bits.NUM_SAMPLES       = Log2(imageCreateInfo.samples);
        pPm4Img->cbColorAttrib.bits.NUM_FRAGMENTS     = Log2(imageCreateInfo.fragments);

        pPm4Img->paScGenericScissorTl.bits.WINDOW_OFFSET_DISABLE = true;
        pPm4Img->paScGenericScissorTl.bits.TL_X = 0;
        pPm4Img->paScGenericScissorTl.bits.TL_Y = 0;
        pPm4Img->paScGenericScissorBr.bits.BR_X = extent.width;
        pPm4Img->paScGenericScissorBr.bits.BR_Y = extent.height;
    }

    const auto&       parent      = *device.Parent();
    const auto        gfxLevel    = parent.ChipProperties().gfxLevel;
    regCB_COLOR0_INFO cbColorInfo = {};

    cbColorInfo.bits.ENDIAN       = ENDIAN_NONE;
    cbColorInfo.bits.FORMAT       = Formats::Gfx6::HwColorFmt(MergedChannelFmtInfoTbl(gfxLevel),
                                                              createInfo.swizzledFormat.format);
    cbColorInfo.bits.NUMBER_TYPE  = Formats::Gfx6::ColorSurfNum(MergedChannelFmtInfoTbl(gfxLevel),
                                                                createInfo.swizzledFormat.format);
    cbColorInfo.bits.COMP_SWAP    = Formats::Gfx6::ColorCompSwap(createInfo.swizzledFormat);

    // Set bypass blending for any format that is not blendable. Blend clamp must be cleared if blend_bypass is set.
    // Otherwise, it must be set iff any component is SNORM, UNORM, or SRGB.
    const bool blendBypass  = (parent.SupportsBlend(createInfo.swizzledFormat.format, ImageTiling::Optimal) == false);
    const bool isNormOrSrgb = Formats::IsNormalized(createInfo.swizzledFormat.format) ||
                              Formats::IsSrgb(createInfo.swizzledFormat.format);
    const bool blendClamp   = (blendBypass == false) && isNormOrSrgb;

    // Selects between truncating (standard for floats) and rounding (standard for most other cases) to convert blender
    // results to frame buffer components. Round mode must be set to ROUND_BY_HALF if any component is UNORM, SNORM or
    // SRGB otherwise ROUND_TRUNCATE.
    const RoundMode roundMode = isNormOrSrgb ? ROUND_BY_HALF : ROUND_TRUNCATE;

    cbColorInfo.bits.BLEND_CLAMP    = blendClamp;
    cbColorInfo.bits.BLEND_BYPASS   = blendBypass;
    cbColorInfo.bits.SIMPLE_FLOAT   = Pal::Device::CbSimpleFloatEnable;
    cbColorInfo.bits.ROUND_MODE     = roundMode;
    cbColorInfo.bits.LINEAR_GENERAL = (baseTileIndex == TileIndexLinearGeneral);

    if (useCompression && m_flags.hasDcc)
    {
        const Gfx6Dcc* const pDcc = m_pImage->GetDcc(m_subresource);

        pPm4Img->cbColorDccControl = pDcc->GetControlReg();

        // We have DCC memory for this surface, but if it's not available for use by the HW, then
        // we can't actually use it.
        cbColorInfo.bits.DCC_ENABLE__VI = m_flags.dccCompressionEnabled;

        // If this view has DCC data and this is a decompress operation we must set "isCompressed"
        // to zero. If the view has DCC data and this is a normal render operation we should set
        // "isCompressed" to one as we expect the CB to write to DCC.
        if (pDcc->IsCompressionEnabled())
        {
            pPm4Img->mipMetaData.isCompressed = (internalInfo.flags.dccDecompress == 0);
        }
    }

    if (useCompression && m_flags.hasCmask)
    {
        const Gfx6Cmask* const pCmask = m_pImage->GetCmask(m_subresource);

        // Setup CB_COLOR*_INFO register fields which depend on CMask state:
        cbColorInfo.bits.COMPRESSION = 1;

        // If the workaround isn't enabled or if there's no DCC data, then set the bit as
        // normal, otherwise, always keep FAST_CLEAR disabled except for CB operations fast-clear
        // eliminate operations.
        if ((device.WaNoFastClearWithDcc() == false) || !m_flags.hasDcc)
        {
            // No DCC data or the workaround isn't needed, so just set the FAST_CLEAR bit as
            // always done on previous ASICs
            cbColorInfo.bits.FAST_CLEAR = pCmask->UseFastClear();
        }
        else if (m_pImage->HasDccData() && internalInfo.flags.fastClearElim)
        {
            cbColorInfo.bits.FAST_CLEAR = 1;
        }

        if (gfxLevel == GfxIpLevel::GfxIp6 || gfxLevel == GfxIpLevel::GfxIp7)
        {
            // This bit is obsolete on gfxip 8 (VI), although it still exists in the reg spec (therefore,
            // there's no __SI__CI extension on its name).
            cbColorInfo.bits.CMASK_IS_LINEAR = pCmask->IsLinear();
        }
        else
        {
            // If the fMask is going to be texture-fetched, then the fMask SRD will contain a
            // pointer to the cMask, which also needs to be in a tiling mode that the texture
            // block can understand.
            if (!fMaskTexFetchAllowed)
            {
                cbColorInfo.bits.CMASK_ADDR_TYPE__VI = pCmask->IsLinear() ? CMASK_ADDR_LINEAR : CMASK_ADDR_TILED;
            }
            else
            {
                // Put the cmask into a tiling format that allows the texture block to read
                // it directly.
                cbColorInfo.bits.CMASK_ADDR_TYPE__VI = CMASK_ADDR_COMPATIBLE;
            }
        }

        pPm4Img->cbColorCmaskSlice.u32All = pCmask->CbColorCmaskSlice().u32All;
    }

    if (useCompression && m_flags.hasFmask)
    {
        const Gfx6Fmask* const pFmask = m_pImage->GetFmask(m_subresource);

        // Setup CB_COLOR*_INFO, CB_COLOR*_ATTRIB and CB_COLOR*_PITCH register fields which
        // depend on FMask state:
        pPm4Img->cbColorAttrib.bits.FMASK_TILE_MODE_INDEX = pFmask->TileIndex();
        pPm4Img->cbColorAttrib.bits.FMASK_BANK_HEIGHT     = pFmask->BankHeight();

        if (gfxLevel != GfxIpLevel::GfxIp6)
        {
            pPm4Img->cbColorPitch.bits.FMASK_TILE_MAX__CI__VI  = (pFmask->Pitch() / TileWidth) - 1;

            cbColorInfo.bits.FMASK_COMPRESSION_DISABLE__CI__VI = !pFmask->UseCompression();

            if (fMaskTexFetchAllowed && !internalInfo.flags.dccDecompress && !internalInfo.flags.fmaskDecompess)
            {
                // Setting this bit means two things:
                //    1) The texture block can read fmask data directly without needing
                //       a decompress stage (documented).
                //    2) If this bit is set then the fMask decompress operation will not occur
                //       whether happening explicitly through fmaskdecompress or as a part of
                //       dcc decompress.(not documented)
                cbColorInfo.bits.FMASK_COMPRESS_1FRAG_ONLY__VI = 1;
            }
        }

        pPm4Img->cbColorFmaskSlice.u32All = pFmask->CbColorFmaskSlice().u32All;
    }
    else
    {
        // NOTE: Due to a quirk in the hardware when FMask is not in-use, we need to set some
        // FMask-specific register fields to match the attributes of the base subResource.
        pPm4Img->cbColorAttrib.bits.FMASK_TILE_MODE_INDEX = baseTileIndex;
        pPm4Img->cbColorAttrib.bits.FMASK_BANK_HEIGHT     = baseBankHeight;

        if (gfxLevel != GfxIpLevel::GfxIp6)
        {
            pPm4Img->cbColorPitch.bits.FMASK_TILE_MAX__CI__VI = pPm4Img->cbColorPitch.bits.TILE_MAX;
        }

        pPm4Img->cbColorFmaskSlice.bits.TILE_MAX = pPm4Img->cbColorSlice.bits.TILE_MAX;
    }

    // Initialize blend optimization register bits. The blend optimizer will override these bits at draw time
    // based on bound blend state. See ColorBlendState::WriteBlendOptimizations.
    if (device.Settings().blendOptimizationsEnable)
    {
        cbColorInfo.bits.BLEND_OPT_DONT_RD_DST   = FORCE_OPT_AUTO;
        cbColorInfo.bits.BLEND_OPT_DISCARD_PIXEL = FORCE_OPT_AUTO;
    }
    else
    {
        cbColorInfo.bits.BLEND_OPT_DONT_RD_DST   = FORCE_OPT_DISABLE;
        cbColorInfo.bits.BLEND_OPT_DISCARD_PIXEL = FORCE_OPT_DISABLE;
    }

    // The CB_COLOR0_INFO RMW packet requires a mask. We want everything but these two bits,
    // so we'll use the inverse of them.
    constexpr uint32 RmwCbColorInfoMask =
        static_cast<const uint32>(~(CB_COLOR0_INFO__BLEND_OPT_DONT_RD_DST_MASK |
                                    CB_COLOR0_INFO__BLEND_OPT_DISCARD_PIXEL_MASK));

    // All relevent register data has now been calculated; create the RMW packet.
    device.CmdUtil().BuildContextRegRmw(mmCB_COLOR0_INFO,
                                        RmwCbColorInfoMask,
                                        cbColorInfo.u32All,
                                        &pPm4Img->colorBufferInfo);
}

// =====================================================================================================================
// Performs Gfx6 hardware-specific initialization for a color target view object, including:
//  - Initialize the PM4 commands used to write the pipeline to HW.
void ColorTargetView::Init(
    const Device&                            device,       // Associated Gfx6 Device object
    const ColorTargetViewCreateInfo&         createInfo,   // ColorTargetView create info
    const ColorTargetViewInternalCreateInfo& internalInfo) // Internal create info
{
    // Note that buffew views have their VA ranges locked because they cannot have their memory rebound.
    m_flags.isBufferView = createInfo.flags.isBufferView;
    m_flags.viewVaLocked = createInfo.flags.imageVaLocked || createInfo.flags.isBufferView;

    m_flags.usesLoadRegIndexPkt = device.Parent()->ChipProperties().gfx6.supportLoadRegIndexPkt;

    if (m_flags.isBufferView == 0)
    {
        PAL_ASSERT(createInfo.imageInfo.pImage != nullptr);

        // Retain a pointer to the attached image.
        m_pImage = GetGfx6Image(createInfo.imageInfo.pImage);

        // Sets the base subresource for this mip.
        m_subresource.aspect     = createInfo.imageInfo.baseSubRes.aspect;
        m_subresource.mipLevel   = createInfo.imageInfo.baseSubRes.mipLevel;
        m_subresource.arraySlice = 0;

        // Set all of the metadata flags.
        m_flags.hasCmask              = m_pImage->HasCmaskData();
        m_flags.hasFmask              = m_pImage->HasFmaskData();
        m_flags.hasDcc                = m_pImage->HasDccData();
        m_flags.hasDccStateMetaData   = m_pImage->HasDccStateMetaData();
        m_flags.fastClearSupported    = (m_pImage->HasFastClearMetaData() && !internalInfo.flags.depthStencilCopy);
        m_flags.dccCompressionEnabled = (m_flags.hasDcc && m_pImage->GetDcc(m_subresource)->IsCompressionEnabled());
    }

    BuildPm4Headers(device, true,  &m_pm4CmdsCompressed);
    BuildPm4Headers(device, false, &m_pm4CmdsDecompressed);

    InitRegisters(device, createInfo, internalInfo, true,  &m_pm4CmdsCompressed);
    InitRegisters(device, createInfo, internalInfo, false, &m_pm4CmdsDecompressed);

    if (m_flags.viewVaLocked && (m_flags.isBufferView == 0))
    {
        UpdateImageVa(&m_pm4CmdsCompressed);
        UpdateImageVa(&m_pm4CmdsDecompressed);
    }
}

// =====================================================================================================================
// Updates the specified PM4 image with the virtual addresses of the image and the image's various metadata addresses.
// This can never be called on buffer views; the buffer view address will be computed elsewhere.
void ColorTargetView::UpdateImageVa(
    ColorTargetViewPm4Img*  pPm4Img
    ) const
{
    // The "GetSubresource256BAddrSwizzled" function will crash if no memory has been bound to
    // the associated image yet, so don't do anything if it's not safe
    if (m_pImage->Parent()->GetBoundGpuMemory().IsBound())
    {
        // Program Color Buffer base address
        pPm4Img->cbColorBase.bits.BASE_256B = m_pImage->GetSubresource256BAddrSwizzled(m_subresource);

        // Either cMask or DCC could have been used for a fast-clear.  The load-meta-data packet updates the cb color
        // regs to indicate what the clear color is. (See Gfx6FastColorClearMetaData in gfx6MaskRam.h).
        if (m_flags.fastClearSupported)
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
        }

        if (m_flags.hasDcc)
        {
            pPm4Img->cbColorDccBase.bits.BASE_256B = m_pImage->GetDcc256BAddr(m_subresource);

            if (m_flags.dccCompressionEnabled && m_flags.hasDccStateMetaData)
            {
                // Program the DCC state metadata address.  (See Gfx6DccMipMetaData in gfx6MaskRam.h).
                const gpusize metaDataVirtAddr = m_pImage->GetDccStateMetaDataAddr(MipLevel());

                PAL_ASSERT((metaDataVirtAddr & 0x3) == 0);

                pPm4Img->hdrMipMetaData.dstAddrLo = LowPart(metaDataVirtAddr);
                pPm4Img->hdrMipMetaData.dstAddrHi = HighPart(metaDataVirtAddr);
            }
        }

        if (m_flags.hasCmask)
        {
            // Program CMask base address.
            pPm4Img->cbColorCmask.bits.BASE_256B = m_pImage->GetCmask256BAddr(m_subresource);
        }

        if (m_flags.hasFmask)
        {
            // Program FMask base address.
            pPm4Img->cbColorFmask.bits.BASE_256B = m_pImage->GetFmask256BAddrSwizzled(m_subresource);
        }
        else
        {
            // According to the CB doc, fast-cleared surfaces without Fmask must program the
            // Fmask base address reg to the same value as the base surface address reg.
            pPm4Img->cbColorFmask.bits.BASE_256B = pPm4Img->cbColorBase.bits.BASE_256B;
        }
    }
}

// =====================================================================================================================
// Writes the PM4 commands required to bind to a certain slot. Returns the next unused DWORD in pCmdSpace.
uint32* ColorTargetView::WriteCommands(
    uint32      slot,
    ImageLayout imageLayout,
    CmdStream*  pCmdStream,
    uint32*     pCmdSpace
    ) const
{
    const bool compressed = (m_flags.isBufferView == 0) &&
                            (m_pImage->LayoutToColorCompressionState(m_subresource, imageLayout) == ColorCompressed);

    const ColorTargetViewPm4Img* pPm4Commands = (compressed ? &m_pm4CmdsCompressed : &m_pm4CmdsDecompressed);
    // Spawn a local copy of the PM4 image, since the register offsets may need to be updated in this method.  For
    // some clients, the base address, Fmask address and Cmask address also need to be updated.
    ColorTargetViewPm4Img patchedPm4Commands;

    if (slot != 0)
    {
        PAL_ASSERT(slot < MaxColorTargets);

        patchedPm4Commands = *pPm4Commands;
        pPm4Commands = &patchedPm4Commands;

        // Offset to add to most PM4 headers' register offset.
        const uint32 slotDelta = (slot * CbRegsPerSlot);

        patchedPm4Commands.hdrCbColorBaseToView.regOffset         += slotDelta;
        patchedPm4Commands.hdrCbColorDccBase.regOffset            += slotDelta;
        patchedPm4Commands.hdrCbColorAttribToFmaskSlice.regOffset += slotDelta;
        patchedPm4Commands.colorBufferInfo.regOffset              += slotDelta;
        patchedPm4Commands.loadMetaData.regOffset                 += slotDelta;

        if ((m_flags.usesLoadRegIndexPkt == 0) && (m_flags.viewVaLocked != 0))
        {
            // If this view uses the legacy LOAD_CONTEXT_REG packet to load fast-clear registers, we need to update
            // the metadata load address along with the register offset.  See UpdateImageVa() for more details.
            const gpusize metaDataVirtAddr =
                ((static_cast<uint64>(patchedPm4Commands.loadMetaData.addrHi.ADDR_HI) << 32) |
                 patchedPm4Commands.loadMetaData.addrLo) -
                (sizeof(uint32) * slotDelta);

            patchedPm4Commands.loadMetaData.addrHi.ADDR_HI = HighPart(metaDataVirtAddr);
            patchedPm4Commands.loadMetaData.addrLo         = LowPart(metaDataVirtAddr);
        }
    }

    if ((m_flags.viewVaLocked == 0) && m_pImage->Parent()->GetBoundGpuMemory().IsBound())
    {
        if (slot == 0) // Note: If slot == 0, then we wouldn't have copied the unpatched PM4 image above!
        {
            patchedPm4Commands = *pPm4Commands;
            pPm4Commands = &patchedPm4Commands;
        }
        UpdateImageVa(&patchedPm4Commands);
    }

    PAL_ASSERT(pCmdStream != nullptr);
    return pCmdStream->WritePm4Image(pPm4Commands->spaceNeeded, pPm4Commands, pCmdSpace);
}

// =====================================================================================================================
// Writes the fast clear color register only to a new value.  This function is sometimes called after a fast clear
// when it is detected that the cleared image is already bound with the old fast clear value loaded.
uint32* ColorTargetView::WriteUpdateFastClearColor(
    uint32       slot,
    const uint32 color[4],
    CmdStream*   pCmdStream,
    uint32*      pCmdSpace)
{
    const uint32 slotRegIncr = (slot * CbRegsPerSlot);

    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(
        mmCB_COLOR0_CLEAR_WORD0 + slotRegIncr,
        mmCB_COLOR0_CLEAR_WORD1 + slotRegIncr,
        color,
        pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Helper method which checks if DCC is enabled for a particular slot & image-layout combination. This is useful for a
// hardware workaround for the DCC overwrite combiner.
bool ColorTargetView::IsDccEnabled(
    ImageLayout activeLayout
    ) const
{
    bool enabled = false;

    if ((m_flags.isBufferView == 0) &&
        (m_pImage->LayoutToColorCompressionState(m_subresource, activeLayout) == ColorCompressed))
    {
        constexpr uint32 DccEnableMask = CB_COLOR0_INFO__DCC_ENABLE_MASK__VI;

        enabled = ((m_pm4CmdsCompressed.colorBufferInfo.regData & DccEnableMask) != 0);
    }

    return enabled;
}

} // Gfx6
} // Pal
