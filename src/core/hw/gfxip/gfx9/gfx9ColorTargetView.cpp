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

#include "core/addrMgr/addrMgr2/addrMgr2.h"
#include "core/hw/gfxip/universalCmdBuffer.h"
#include "core/hw/gfxip/gfx9/g_gfx9PalSettings.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9ColorTargetView.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9FormatInfo.h"
#include "palFormatInfo.h"
#include <type_traits>

using namespace Util;
using namespace Pal::Formats::Gfx9;
using std::is_same;

namespace Pal
{
namespace Gfx9
{

// Value for CB_COLOR_DCC_CONTROL when compressed rendering is disabled.
constexpr uint32 CbColorDccControlDecompressed = 0;

// Mask of CB_COLOR_INFO bits to clear when compressed rendering is disabled.
constexpr uint32 CbColorInfoDecompressedMask = (CB_COLOR0_INFO__DCC_ENABLE_MASK                |
                                                CB_COLOR0_INFO__COMPRESSION_MASK               |
                                                CB_COLOR0_INFO__FMASK_COMPRESSION_DISABLE_MASK |
                                                CB_COLOR0_INFO__FMASK_COMPRESS_1FRAG_ONLY_MASK);

// =====================================================================================================================
ColorTargetView::ColorTargetView(
    const Device*                            pDevice,
    const ColorTargetViewCreateInfo&         createInfo,
    const ColorTargetViewInternalCreateInfo& internalInfo)
    :
    m_pDevice(pDevice),
    m_pImage((createInfo.flags.isBufferView == 0) ? GetGfx9Image(createInfo.imageInfo.pImage) : nullptr),
    m_arraySize(0)
{
    const auto&  settings = GetGfx9Settings(*(pDevice->Parent()));

    memset(&m_subresource, 0, sizeof(m_subresource));

    m_flags.u32All = 0;
    // Note that buffew views have their VA ranges locked because they cannot have their memory rebound.
    m_flags.isBufferView = createInfo.flags.isBufferView;
    m_flags.viewVaLocked = (createInfo.flags.imageVaLocked | createInfo.flags.isBufferView);

    if (m_flags.isBufferView == 0)
    {
        PAL_ASSERT(m_pImage != nullptr);

        // If this assert triggers the caller is probably trying to select z slices using the subresource range
        // instead of the zRange as required by the PAL interface.
        PAL_ASSERT((m_pImage->Parent()->GetImageCreateInfo().imageType != ImageType::Tex3d) ||
                   ((createInfo.imageInfo.baseSubRes.arraySlice == 0) && (createInfo.imageInfo.arraySize == 1)));

        // Sets the base subresource for this mip. Note that this is not fixed to the first slice like in gfx6.
        m_subresource = createInfo.imageInfo.baseSubRes;
        m_arraySize   = createInfo.imageInfo.arraySize;

        if ((settings.waRestrictMetaDataUseInMipTail == false) ||
            m_pImage->CanMipSupportMetaData(m_subresource.mipLevel))
        {
            m_flags.hasDcc              = m_pImage->HasDccData();
            m_flags.hasDccStateMetaData = m_pImage->HasDccStateMetaData();
            m_flags.hasCmaskFmask       = m_pImage->HasFmaskData();
        }

        // If this view has DCC data and this is a decompress operation we must set "isDccCompress"
        // to zero. If the view has DCC data and this is a normal render operation we should set
        // "isDccCompress" to one as we expect the CB to write to DCC.
        m_flags.isDccDecompress = internalInfo.flags.dccDecompress;

        if (m_pDevice->Settings().waitOnMetadataMipTail)
        {
            m_flags.waitOnMetadataMipTail = m_pImage->IsInMetadataMipTail(MipLevel());
        }

        m_layoutToState = m_pImage->LayoutToColorCompressionState();
    }
    else
    {
        memset(&m_layoutToState, 0, sizeof(m_layoutToState));
    }
}

// =====================================================================================================================
// Helper function which adds commands into the command stream when the currently-bound color targets are changing.
// Returns the address to where future commands will be written.
uint32* ColorTargetView::HandleBoundTargetsChanged(
    const Device& device,
    uint32*       pCmdSpace)
{
    if (device.Settings().disableDfsm == false)
    {
        // If the slice-index as programmed by the CB is changing, then we have to flush DFSM stuff. This isn't
        // necessary if DFSM is disabled.
        //
        // ("it" refers to the RT-index, the HW perspective of which slice is being rendered to. The RT-index is
        //  a combination of the CB registers and the GS output).
        //
        //  If the GS (HW VS) is changing it, then there is only one view, so no batch break is needed..  If any of the
        //  RT views are changing, the DFSM has no idea about it and there isn't any one single RT_index to keep track
        //  of since each RT may have a different view with different STARTs and SIZEs that can be independently
        //  changing.  The DB and Scan Converter also doesn't know about the CB's views changing.  This is why there
        //  should be a batch break on RT view changes.  The other reason is that binning and deferred shading can't
        //  give any benefit when the bound RT views of consecutive contexts are not intersecting.  There is no way to
        //  increase cache hit ratios if there is no way to generate the same address between draws, so there is no
        //  reason to enable binning.
        pCmdSpace += device.CmdUtil().BuildNonSampleEventWrite(BREAK_BATCH, EngineTypeUniversal, pCmdSpace);
    }

    // If you change the mips of a resource being rendered-to, regardless of which MRT slot it is bound to, we need
    // to flush the CB metadata caches (DCC, Fmask, Cmask). This protects against the case where a DCC, Fmask or Cmask
    // cacheline can contain data from two different mip levels in different RB's.
    pCmdSpace += device.CmdUtil().BuildNonSampleEventWrite(FLUSH_AND_INV_CB_META, EngineTypeUniversal, pCmdSpace);

    // Unfortunately, the FLUSH_AND_INV_CB_META event doesn't actually flush the DCC cache. Instead, it only flushes the
    // Fmask and Cmask caches, along with the overwrite combiner. So we also need to issue another event to flush the CB
    // pixel data caches, which will also flush the DCC cache.
    pCmdSpace += device.CmdUtil().BuildNonSampleEventWrite(FLUSH_AND_INV_CB_PIXEL_DATA, EngineTypeUniversal, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Writes the PM4 commands to dynmaic general Dcc state metadata base on arraySize.
void ColorTargetView::UpdateDccStateMetadata(
    CmdStream*  pCmdStream,
    ImageLayout imageLayout
    ) const
{
    const bool compressed = (m_flags.isBufferView == 0) &&
                            (ImageLayoutToColorCompressionState(m_layoutToState, imageLayout) == ColorCompressed);

    if (compressed && m_flags.hasDccStateMetaData)
    {
        const SubresRange range = {m_subresource, 1, m_arraySize};
        m_pImage->UpdateDccStateMetaData(pCmdStream,
                                         range,
                                         (m_flags.isDccDecompress == 0),
                                         pCmdStream->GetEngineType(),
                                         PredDisable);
    }
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

    return pCmdStream->WriteSetSeqContextRegs(mmCB_COLOR0_CLEAR_WORD0 + slotRegIncr,
                                              mmCB_COLOR0_CLEAR_WORD1 + slotRegIncr,
                                              color,
                                              pCmdSpace);
}

// =====================================================================================================================
// Builds the common PM4 packet headers
template <typename Pm4ImgType>
void ColorTargetView::CommonBuildPm4Headers(
    Pm4ImgType* pPm4Img
    ) const
{
    const CmdUtil& cmdUtil = m_pDevice->CmdUtil();

    size_t extraSpace = cmdUtil.BuildSetSeqContextRegs(mmPA_SC_GENERIC_SCISSOR_TL,
                                                       mmPA_SC_GENERIC_SCISSOR_BR,
                                                       &pPm4Img->hdrPaScGenericScissor);

    pPm4Img->spaceNeeded             += extraSpace;
    pPm4Img->spaceNeededDecompressed += extraSpace;

    if (m_flags.hasDcc != 0)
    {
        // On GFX9, if we have DCC we also have fast clear metadata. This class assumes this will always be true.
        PAL_ASSERT(m_pImage->HasFastClearMetaData());

        // If the parent Image has DCC memory, then we need to add a LOAD_CONTEXT_REG packet to load the image's
        // fast-clear metadata.
        //
        // NOTE: Just because we have DCC data doesn't mean that we're doing fast-clears. Writing this register
        // shouldn't hurt anything though. We do not know the GPU virtual address of the metadata until bind-time.
        pPm4Img->spaceNeeded +=
            cmdUtil.BuildLoadContextRegsIndex<true>(0,
                                                    mmCB_COLOR0_CLEAR_WORD0,
                                                    (mmCB_COLOR0_CLEAR_WORD1 - mmCB_COLOR0_CLEAR_WORD0 + 1),
                                                    &pPm4Img->loadMetaDataIndex);
    }
}

// =====================================================================================================================
template <typename Pm4ImgType, typename CbColorViewType>
void ColorTargetView::InitCommonBufferView(
    const ColorTargetViewCreateInfo& createInfo,
    Pm4ImgType*                      pPm4Img,
    CbColorViewType*                 pCbColorView
    ) const
{
    PAL_ASSERT(createInfo.bufferInfo.pGpuMemory != nullptr);

    // The buffer virtual address is simply "offset" pixels from the start of the GPU memory's virtual address.
    const gpusize bufferOffset = (createInfo.bufferInfo.offset *
                                  Formats::BytesPerPixel(createInfo.swizzledFormat.format));
    const gpusize bufferAddr   = (createInfo.bufferInfo.pGpuMemory->Desc().gpuVirtAddr + bufferOffset);

    // Convert to a 256-bit aligned base address and a base offset. Note that we don't need to swizzle the base
    // address because buffers aren't macro tiled.
    const gpusize baseOffset = bufferAddr & 0xFF;
    const gpusize baseAddr   = bufferAddr & (~0xFF);

    pPm4Img->cbColorBase.bits.BASE_256B = Get256BAddrLo(baseAddr);

    // The view slice_start is overloaded to specify the base offset.
    pCbColorView->SLICE_START = baseOffset;
    pCbColorView->SLICE_MAX   = 0;
    pCbColorView->MIP_LEVEL   = 0;

    // According to the other UMDs, this is the absolute max mip level. For one mip level, the MAX_MIP is mip #0.
    pPm4Img->cbColorAttrib2.bits.MAX_MIP     = 0;

    // From testing this is not the padded mip height/width, but the pixel height/width specified by the client.
    pPm4Img->cbColorAttrib2.bits.MIP0_HEIGHT = 0;
    pPm4Img->cbColorAttrib2.bits.MIP0_WIDTH  = (createInfo.bufferInfo.extent - 1);

    pPm4Img->paScGenericScissorTl.bits.WINDOW_OFFSET_DISABLE = true;
    pPm4Img->paScGenericScissorTl.bits.TL_X = 0;
    pPm4Img->paScGenericScissorTl.bits.TL_Y = 0;
    pPm4Img->paScGenericScissorBr.bits.BR_X = createInfo.bufferInfo.extent;
    pPm4Img->paScGenericScissorBr.bits.BR_Y = 1;

    pPm4Img->cbColorAttrib.bits.FORCE_DST_ALPHA_1 = Formats::HasUnusedAlpha(createInfo.swizzledFormat) ? 1 : 0;
    pPm4Img->cbColorAttrib.bits.NUM_SAMPLES       = 0;
    pPm4Img->cbColorAttrib.bits.NUM_FRAGMENTS     = 0;
}

// =====================================================================================================================
template <typename FmtInfoType>
regCB_COLOR0_INFO ColorTargetView::InitCommonCbColorInfo(
    const ColorTargetViewCreateInfo& createInfo,
    const FmtInfoType*               pFmtInfo
    ) const
{
    const Pal::Device*const pParentDevice = m_pDevice->Parent();

    regCB_COLOR0_INFO cbColorInfo = { };
    cbColorInfo.bits.ENDIAN      = ENDIAN_NONE;
    cbColorInfo.bits.FORMAT      = Formats::Gfx9::HwColorFmt(pFmtInfo, createInfo.swizzledFormat.format);
    cbColorInfo.bits.NUMBER_TYPE = Formats::Gfx9::ColorSurfNum(pFmtInfo, createInfo.swizzledFormat.format);
    cbColorInfo.bits.COMP_SWAP   = Formats::Gfx9::ColorCompSwap(createInfo.swizzledFormat);

    // Set bypass blending for any format that is not blendable. Blend clamp must be cleared if blend_bypass is set.
    // Otherwise, it must be set iff any component is SNORM, UNORM, or SRGB.
    const bool blendBypass  =
        (pParentDevice->SupportsBlend(createInfo.swizzledFormat.format, ImageTiling::Optimal) == false);
    const bool isNormOrSrgb = Formats::IsNormalized(createInfo.swizzledFormat.format) ||
                              Formats::IsSrgb(createInfo.swizzledFormat.format);
    const bool blendClamp   = (blendBypass == false) && isNormOrSrgb;

    // Selects between truncating (standard for floats) and rounding (standard for most other cases) to convert blender
    // results to frame buffer components. Round mode must be set to ROUND_BY_HALF if any component is UNORM, SNORM or
    // SRGB otherwise ROUND_TRUNCATE.
    const RoundMode roundMode = isNormOrSrgb ? ROUND_BY_HALF : ROUND_TRUNCATE;

    cbColorInfo.bits.BLEND_CLAMP  = blendClamp;
    cbColorInfo.bits.BLEND_BYPASS = blendBypass;
    cbColorInfo.bits.SIMPLE_FLOAT = Pal::Device::CbSimpleFloatEnable;
    cbColorInfo.bits.ROUND_MODE   = roundMode;

    return cbColorInfo;
}

// =====================================================================================================================
template <typename Pm4ImgType, typename CbColorViewType>
void ColorTargetView::InitCommonImageView(
    const ColorTargetViewCreateInfo&         createInfo,
    const ColorTargetViewInternalCreateInfo& internalInfo,
    const Extent3d&                          baseExtent,
    const Extent3d&                          extent,
    Pm4ImgType*                              pPm4Img,
    regCB_COLOR0_INFO*                       pCbColorInfo,
    CbColorViewType*                         pCbColorView
    ) const
{
    const Pal::Device*      pParentDevice   = m_pDevice->Parent();
    const ImageCreateInfo&  imageCreateInfo = m_pImage->Parent()->GetImageCreateInfo();
    const ImageType         imageType       = m_pImage->GetOverrideImageType();
    const auto&             settings        = GetGfx9Settings(*pParentDevice);

    pPm4Img->paScGenericScissorTl.bits.WINDOW_OFFSET_DISABLE = 1;
    pPm4Img->paScGenericScissorTl.bits.TL_X = 0;
    pPm4Img->paScGenericScissorTl.bits.TL_Y = 0;
    pPm4Img->paScGenericScissorBr.bits.BR_X = extent.width;
    pPm4Img->paScGenericScissorBr.bits.BR_Y = extent.height;

    pPm4Img->cbColorAttrib.bits.NUM_SAMPLES       = Log2(imageCreateInfo.samples);
    pPm4Img->cbColorAttrib.bits.NUM_FRAGMENTS     = Log2(imageCreateInfo.fragments);
    pPm4Img->cbColorAttrib.bits.FORCE_DST_ALPHA_1 = Formats::HasUnusedAlpha(createInfo.swizzledFormat) ? 1 : 0;

    // According to the other UMDs, this is the absolute max mip level. For one mip level, the MAX_MIP is mip #0.
    pPm4Img->cbColorAttrib2.bits.MAX_MIP = (imageCreateInfo.mipLevels - 1);

    if ((createInfo.flags.zRangeValid == 1) && (imageType == ImageType::Tex3d))
    {
        pCbColorView->SLICE_START = createInfo.zRange.offset;
        pCbColorView->SLICE_MAX   = (createInfo.zRange.offset + createInfo.zRange.extent - 1);
        pCbColorView->MIP_LEVEL   = createInfo.imageInfo.baseSubRes.mipLevel;
    }
    else
    {
        const uint32 baseArraySlice = createInfo.imageInfo.baseSubRes.arraySlice;

        pCbColorView->SLICE_START = baseArraySlice;
        pCbColorView->SLICE_MAX   = (baseArraySlice + createInfo.imageInfo.arraySize - 1);
        pCbColorView->MIP_LEVEL   = createInfo.imageInfo.baseSubRes.mipLevel;
    }

    if (m_flags.hasDcc != 0)
    {
        regCB_COLOR0_DCC_CONTROL dccControl = m_pImage->GetDcc()->GetControlReg();
        if (internalInfo.flags.fastClearElim && IsGfx091xPlus(*(m_pDevice->Parent())))
        {
            // Without this, the CB will not expand the compress-to-register (0x20) keys.
            dccControl.gfx09_1xPlus.DISABLE_CONSTANT_ENCODE_REG = 1;
        }

        pPm4Img->cbColorDccControl = dccControl;

        pCbColorInfo->bits.DCC_ENABLE = 1;
    }

    if (m_flags.hasCmaskFmask != 0)
    {
        // Check if we can keep fmask in a compressed state and avoid corresponding fmask decompression
        const bool fMaskTexFetchAllowed = m_pImage->IsComprFmaskShaderReadable(m_subresource);

        // Setup CB_COLOR*_INFO register fields which depend on CMask or fMask state:
        pCbColorInfo->bits.COMPRESSION               = 1;
        pCbColorInfo->bits.FMASK_COMPRESSION_DISABLE = settings.fmaskCompressDisable;

        if (fMaskTexFetchAllowed                      &&
            (internalInfo.flags.dccDecompress   == 0) &&
            (internalInfo.flags.fmaskDecompress == 0))
        {
            // Setting this bit means two things:
            //    1) The texture block can read fmask data directly without needing
            //       a decompress stage (documented).
            //    2) If this bit is set then the fMask decompress operation will not occur
            //       whether happening explicitly through fmaskdecompress or as a part of
            //       dcc decompress.(not documented)
            pCbColorInfo->bits.FMASK_COMPRESS_1FRAG_ONLY = 1;
        }
    }

    // From testing this is not the padded mip height/width, but the pixel height/width specified by the client.
    pPm4Img->cbColorAttrib2.bits.MIP0_HEIGHT = (baseExtent.height - 1);
    pPm4Img->cbColorAttrib2.bits.MIP0_WIDTH  = (baseExtent.width - 1);
}

// =====================================================================================================================
// Updates the specified PM4 image with the virtual addresses of the image and the image's various metadata addresses.
// This can never be called on buffer views; the buffer view address will be computed elsewhere.
template <typename Pm4ImgType>
void ColorTargetView::UpdateImageVa(
    Pm4ImgType* pPm4Img
    ) const
{
    const BoundGpuMemory& boundMem = m_pImage->Parent()->GetBoundGpuMemory();

    // The "GetSubresource256BAddrSwizzled" function will crash if no memory has been bound to
    // the associated image yet, so don't do anything if it's not safe
    if (boundMem.IsBound())
    {
        // Program Color Buffer base address
        pPm4Img->cbColorBase.bits.BASE_256B = m_pImage->GetSubresource256BAddrSwizzled(m_subresource);
        PAL_ASSERT(m_pImage->GetSubresource256BAddrSwizzledHi(m_subresource) == 0);

        // On GFX9, only DCC can be used for fast clears.  The load-meta-data packet updates the cb color regs to
        // indicate what the clear color is.  (See Gfx9FastColorClearMetaData in gfx9MaskRam.h).
        if (m_flags.hasDcc)
        {
            // Program fast-clear metadata base address.
            gpusize metaDataVirtAddr = m_pImage->FastClearMetaDataAddr(MipLevel());
            PAL_ASSERT((metaDataVirtAddr & 0x3) == 0);

            pPm4Img->loadMetaDataIndex.bitfields2.mem_addr_lo = (LowPart(metaDataVirtAddr) >> 2);
            pPm4Img->loadMetaDataIndex.mem_addr_hi            = HighPart(metaDataVirtAddr);

            // Tell the HW where the DCC surface is
            pPm4Img->cbColorDccBase.bits.BASE_256B = m_pImage->GetDcc256BAddr();
        }

        if (m_flags.hasCmaskFmask)
        {
            pPm4Img->cbColorCmask.bits.BASE_256B = m_pImage->GetCmask256BAddr();
            pPm4Img->cbColorFmask.bits.BASE_256B = m_pImage->GetFmask256BAddr();
        }
    }
}

// =====================================================================================================================
Gfx9ColorTargetView::Gfx9ColorTargetView(
    const Device*                            pDevice,
    const ColorTargetViewCreateInfo&         createInfo,
    const ColorTargetViewInternalCreateInfo& internalInfo)
    :
    ColorTargetView(pDevice, createInfo, internalInfo)
{
    memset(&m_pm4Cmds, 0, sizeof(m_pm4Cmds));

    BuildPm4Headers();
    InitRegisters(createInfo, internalInfo);

    if (m_flags.viewVaLocked && (m_flags.isBufferView == 0))
    {
        UpdateImageVa(&m_pm4Cmds);
    }
}

// =====================================================================================================================
// Builds the PM4 packet headers for an image of PM4 commands used to write this View object to hardware.
void Gfx9ColorTargetView::BuildPm4Headers()
{
    const CmdUtil& cmdUtil = m_pDevice->CmdUtil();

    size_t spaceNeeded = cmdUtil.BuildSetSeqContextRegs(mmCB_COLOR0_BASE, mmCB_COLOR0_VIEW, &m_pm4Cmds.hdrCbColorBase);

    // NOTE: The register offset will be updated at bind-time to reflect the actual slot this View is being bound to.
    spaceNeeded += CmdUtil::ContextRegRmwSizeDwords;

    spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmCB_COLOR0_ATTRIB,
                                                  Gfx09::mmCB_COLOR0_DCC_BASE_EXT,
                                                  &m_pm4Cmds.hdrCbColorAttrib);

    spaceNeeded += cmdUtil.BuildSetOneContextReg(Gfx09::mmCB_MRT0_EPITCH, &m_pm4Cmds.hdrCbMrtEpitch);

    m_pm4Cmds.spaceNeeded             = spaceNeeded;
    m_pm4Cmds.spaceNeededDecompressed = spaceNeeded;

    CommonBuildPm4Headers(&m_pm4Cmds);
}

// =====================================================================================================================
// Finalizes the PM4 packet image by setting up the register values used to write this View object to hardware.
void Gfx9ColorTargetView::InitRegisters(
    const ColorTargetViewCreateInfo&         createInfo,
    const ColorTargetViewInternalCreateInfo& internalInfo)
{
    const MergedFmtInfo*const pFmtInfo = MergedChannelFmtInfoTbl(GfxIpLevel::GfxIp9);

    regCB_COLOR0_INFO cbColorInfo = InitCommonCbColorInfo(createInfo, pFmtInfo);

    // Most register values are simple to compute but vary based on whether or not this is a buffer view. Let's set
    // them all up-front before we get on to the harder register values.
    if (m_flags.isBufferView)
    {
        PAL_ASSERT(createInfo.bufferInfo.offset == 0);

        InitCommonBufferView(createInfo, &m_pm4Cmds, &m_pm4Cmds.cbColorView.gfx09);

        m_pm4Cmds.cbColorAttrib.gfx09.MIP0_DEPTH    = 0; // what is this?
        m_pm4Cmds.cbColorAttrib.gfx09.COLOR_SW_MODE = SW_LINEAR;
        m_pm4Cmds.cbColorAttrib.gfx09.RESOURCE_TYPE = static_cast<uint32>(ImageType::Tex1d); // no HW enums
        m_pm4Cmds.cbColorAttrib.gfx09.RB_ALIGNED    = 0;
        m_pm4Cmds.cbColorAttrib.gfx09.PIPE_ALIGNED  = 0;
        m_pm4Cmds.cbColorAttrib.gfx09.FMASK_SW_MODE = SW_LINEAR; // ignored as there is no fmask
        m_pm4Cmds.cbColorAttrib.gfx09.META_LINEAR   = 0;         // linear meta surfaces not supported on gfx9
        m_pm4Cmds.cbMrtEpitch.bits.EPITCH           = (createInfo.bufferInfo.extent - 1);
    }
    else
    {
        const auto*const            pImage          = m_pImage->Parent();
        const SubresId              baseSubRes      = { m_subresource.aspect, 0, 0 };
        const SubResourceInfo*const pBaseSubResInfo = pImage->SubresourceInfo(baseSubRes);
        const SubResourceInfo*const pSubResInfo     = pImage->SubresourceInfo(m_subresource);
        const auto&                 surfSetting     = m_pImage->GetAddrSettings(pSubResInfo);
        const auto*const            pAddrOutput     = m_pImage->GetAddrOutput(pBaseSubResInfo);
        const ImageCreateInfo&      imageCreateInfo = pImage->GetImageCreateInfo();
        const ImageType             imageType       = m_pImage->GetOverrideImageType();
        const bool                  imgIsBc         = Formats::IsBlockCompressed(imageCreateInfo.swizzledFormat.format);

        // NOTE: The color base address will be determined later, we don't need to do anything here.

        Extent3d baseExtent = pBaseSubResInfo->extentTexels;
        Extent3d extent     = pSubResInfo->extentTexels;

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
            baseExtent = pBaseSubResInfo->extentElements;
            extent     = pSubResInfo->extentElements;
        }

        bool modifiedYuvExtent = false;
        if (Formats::IsYuvPacked(pSubResInfo->format.format)                  &&
            (Formats::IsYuvPacked(createInfo.swizzledFormat.format) == false) &&
            ((pSubResInfo->bitsPerTexel << 1) == Formats::BitsPerPixel(createInfo.swizzledFormat.format)))
        {
            PAL_NOT_TESTED();

            // Changing how we interpret the bits-per-pixel of the subresource wreaks havoc with any tile swizzle
            // pattern used. This will only work for linear-tiled Images.
            PAL_ASSERT(m_pImage->IsSubResourceLinear(baseSubRes));

            baseExtent.width >>= 1;
            extent.width     >>= 1;
            modifiedYuvExtent  = true;
        }
        else if (Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format) &&
                 ((createInfo.imageInfo.arraySize > 1) || (createInfo.imageInfo.baseSubRes.arraySlice != 0)))
        {
            baseExtent = pBaseSubResInfo->actualExtentTexels;
            m_pImage->PadYuvPlanarViewActualExtent(m_subresource, &baseExtent);
            modifiedYuvExtent = true;
        }

        // The view should be in terms of texels except when we're operating in terms of elements. This will only happen
        // when we're copying to an "expanded" format (e.g., R32G32B32). In this case we can't do native format writes
        // so we're going to write each element independently. The trigger for this case is a mismatched bpp.
        if ((pSubResInfo->bitsPerTexel != Formats::BitsPerPixel(createInfo.swizzledFormat.format)) &&
            (modifiedYuvExtent == false))
        {
            baseExtent = pBaseSubResInfo->extentElements;
            extent     = pSubResInfo->extentElements;
        }

        InitCommonImageView(createInfo,
                            internalInfo,
                            baseExtent,
                            extent,
                            &m_pm4Cmds,
                            &cbColorInfo,
                            &m_pm4Cmds.cbColorView.gfx09);

        m_pm4Cmds.cbColorAttrib.gfx09.MIP0_DEPTH    =
            ((imageType == ImageType::Tex3d) ? imageCreateInfo.extent.depth : imageCreateInfo.arraySize) - 1;
        m_pm4Cmds.cbColorAttrib.gfx09.COLOR_SW_MODE = AddrMgr2::GetHwSwizzleMode(surfSetting.swizzleMode);
        m_pm4Cmds.cbColorAttrib.gfx09.RESOURCE_TYPE = static_cast<uint32>(imageType); // no HW enums
        m_pm4Cmds.cbColorAttrib.gfx09.RB_ALIGNED    = m_pImage->IsRbAligned();
        m_pm4Cmds.cbColorAttrib.gfx09.PIPE_ALIGNED  = m_pImage->IsPipeAligned();
        m_pm4Cmds.cbColorAttrib.gfx09.META_LINEAR   = 0;

        const AddrSwizzleMode fMaskSwizzleMode =
            (m_pImage->HasFmaskData() ? m_pImage->GetFmask()->GetSwizzleMode() : ADDR_SW_LINEAR /* ignored */);
        m_pm4Cmds.cbColorAttrib.gfx09.FMASK_SW_MODE = AddrMgr2::GetHwSwizzleMode(fMaskSwizzleMode);

        if (modifiedYuvExtent)
        {
            m_pm4Cmds.cbMrtEpitch.bits.EPITCH =
                ((pAddrOutput->epitchIsHeight ? baseExtent.height : baseExtent.width) - 1);
        }
        else
        {
            m_pm4Cmds.cbMrtEpitch.bits.EPITCH = AddrMgr2::CalcEpitch(pAddrOutput);
        }
    }

    // The CB_COLOR0_INFO RMW packet requires a mask. We want everything but these two bits,
    // so we'll use the inverse of them.
    constexpr uint32 RmwCbColorInfoMask =
        static_cast<const uint32>(~(CB_COLOR0_INFO__BLEND_OPT_DONT_RD_DST_MASK |
                                    CB_COLOR0_INFO__BLEND_OPT_DISCARD_PIXEL_MASK));

    // All relevent register data has now been calculated; create the RMW packet.
    m_pDevice->CmdUtil().BuildContextRegRmw(mmCB_COLOR0_INFO,
                                            RmwCbColorInfoMask,
                                            cbColorInfo.u32All,
                                            &m_pm4Cmds.cbColorInfo);
}

// =====================================================================================================================
// Writes the PM4 commands required to bind to a certain slot. Returns the next unused DWORD in pCmdSpace.
uint32* Gfx9ColorTargetView::WriteCommands(
    uint32        slot,        // Bind slot
    ImageLayout   imageLayout, // Current image layout
    CmdStream*    pCmdStream,
    uint32*       pCmdSpace
    ) const
{
    const bool decompressedImage =
        (m_flags.isBufferView == 0) &&
        (ImageLayoutToColorCompressionState(m_layoutToState, imageLayout) != ColorCompressed);

    const Gfx9ColorTargetViewPm4Img* pPm4Commands = &m_pm4Cmds;
    // Spawn a local copy of the PM4 image, since some register values and/or offsets may need to be updated in this
    // method.  For some clients, the base address, Fmask address and Cmask address also need to be updated.  The
    // contents of the local copy will depend on which Image state is specified.
    Gfx9ColorTargetViewPm4Img patchedPm4Commands;

    if (slot != 0)
    {
        PAL_ASSERT(slot < MaxColorTargets);

        patchedPm4Commands = *pPm4Commands;
        pPm4Commands = &patchedPm4Commands;

        // Offset to add to most PM4 headers' register offset.  Note that all CB_MRT*_EPITCH registers are adjacent
        // to one another, so for that one we can just increment by 'slot'.
        const uint32 slotDelta = (slot * CbRegsPerSlot);

        patchedPm4Commands.hdrCbColorBase.bitfields2.reg_offset    += slotDelta;
        patchedPm4Commands.hdrCbColorAttrib.bitfields2.reg_offset  += slotDelta;
        patchedPm4Commands.hdrCbMrtEpitch.bitfields2.reg_offset    += slot;
        patchedPm4Commands.cbColorInfo.bitfields2.reg_offset       += slotDelta;
        patchedPm4Commands.loadMetaDataIndex.bitfields4.reg_offset += slotDelta;
    }

    size_t spaceNeeded = pPm4Commands->spaceNeeded;
    if (decompressedImage)
    {
        if (pPm4Commands != &patchedPm4Commands)
        {
            patchedPm4Commands = *pPm4Commands;
            pPm4Commands = &patchedPm4Commands;
        }

        // For decompressed rendering to an Image, we need to override the values for CB_COLOR_CONTROL and for
        // CB_COLOR_DCC_CONTROL.
        patchedPm4Commands.cbColorDccControl.u32All = CbColorDccControlDecompressed;
        patchedPm4Commands.cbColorInfo.reg_data    &= ~CbColorInfoDecompressedMask;

        spaceNeeded = pPm4Commands->spaceNeededDecompressed;
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

    PAL_ASSERT(pCmdStream != nullptr);
    return pCmdStream->WritePm4Image(spaceNeeded, pPm4Commands, pCmdSpace);
}

} // Gfx9
} // Pal
