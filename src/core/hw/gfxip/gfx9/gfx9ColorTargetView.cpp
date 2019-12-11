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
    const Device*                     pDevice,
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalInfo)
    :
    m_pDevice(pDevice),
    m_pImage((createInfo.flags.isBufferView == 0) ? GetGfx9Image(createInfo.imageInfo.pImage) : nullptr),
    m_arraySize(0)
{
    const auto&  settings = GetGfx9Settings(*(pDevice->Parent()));

    memset(&m_subresource, 0, sizeof(m_subresource));

    m_flags.u32All = 0;
    // Note that buffer views have their VA ranges locked because they cannot have their memory rebound.
    m_flags.isBufferView = createInfo.flags.isBufferView;
    m_flags.viewVaLocked = (createInfo.flags.imageVaLocked | createInfo.flags.isBufferView);
    m_swizzledFormat     = createInfo.swizzledFormat;

    if (m_flags.isBufferView == 0)
    {
        PAL_ASSERT(m_pImage != nullptr);

        const ImageCreateInfo& imageCreateInfo = m_pImage->Parent()->GetImageCreateInfo();

        // If this assert triggers the caller is probably trying to select z slices using the subresource range
        // instead of the zRange as required by the PAL interface.
        PAL_ASSERT((imageCreateInfo.imageType != ImageType::Tex3d) ||
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

        // The Y and UV planes of a YUV-planar Image are interleaved, so we need to include padding
        // when we set up a color-target view so that the HW will correctly span all planes when
        // addressin nonzero array slices. This padding can cause problems with because
        // the HW thinks each plane is larger than it actually is.
        // A better solution for single-slice views is to use the subresource address for
        // the color address instead of the slice0 base address.
        if (Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format) &&
            (imageCreateInfo.mipLevels == 1)                            &&
            (imageCreateInfo.imageType == ImageType::Tex2d)             &&
            (createInfo.imageInfo.arraySize == 1))
        {
            PAL_ASSERT(m_flags.hasDcc == 0);
            PAL_ASSERT(m_flags.hasCmaskFmask == 0);
            m_flags.useSubresBaseAddr = 1;
        }

        // Determine whether Overwrite Combiner (OC) should be to be disabled or not
        if (m_pDevice->Settings().waRotatedSwizzleDisablesOverwriteCombiner)
        {
            const auto* pPalImage = m_pImage->Parent();
            const SubresId  subResId = { m_subresource.aspect, MipLevel(), 0 };
            const auto*     pSubResInfo = pPalImage->SubresourceInfo(subResId);
            const auto&     surfSettings = m_pImage->GetAddrSettings(pSubResInfo);

            // Disable overwrite-combiner for rotated swizzle modes
            if (AddrMgr2::IsRotatedSwizzle(surfSettings.swizzleMode))
            {
                m_flags.disableRotateSwizzleOC = 1;
            }
        }
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
    if (m_flags.hasDcc != 0)
    {
        // On GFX9, if we have DCC we also have fast clear metadata. This class assumes this will always be true.
        PAL_ASSERT(m_pImage->HasFastClearMetaData());

        // If the parent Image has DCC memory, then we need to add a LOAD_CONTEXT_REG packet to load the image's
        // fast-clear metadata.
        //
        // NOTE: Just because we have DCC data doesn't mean that we're doing fast-clears. Writing this register
        // shouldn't hurt anything though. We do not know the GPU virtual address of the metadata until bind-time.
        const CmdUtil& cmdUtil = m_pDevice->CmdUtil();
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
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalInfo,
    const Extent3d&                   baseExtent,
    Pm4ImgType*                       pPm4Img,
    regCB_COLOR0_INFO*                pCbColorInfo,
    CbColorViewType*                  pCbColorView
    ) const
{
    const Pal::Device*      pParentDevice   = m_pDevice->Parent();
    const ImageCreateInfo&  imageCreateInfo = m_pImage->Parent()->GetImageCreateInfo();
    const ImageType         imageType       = m_pImage->GetOverrideImageType();
    const auto&             settings        = GetGfx9Settings(*pParentDevice);

    // According to the other UMDs, this is the absolute max mip level. For one mip level, the MAX_MIP is mip #0.
    pPm4Img->cbColorAttrib2.bits.MAX_MIP = (imageCreateInfo.mipLevels - 1);

    if ((createInfo.flags.zRangeValid == 1) && (imageType == ImageType::Tex3d))
    {
        pCbColorView->SLICE_START = createInfo.zRange.offset;
        pCbColorView->SLICE_MAX   = (createInfo.zRange.offset + createInfo.zRange.extent - 1);
        pCbColorView->MIP_LEVEL   = createInfo.imageInfo.baseSubRes.mipLevel;
    }
    else if (m_flags.useSubresBaseAddr != 0)
    {
        pCbColorView->SLICE_START = 0;
        pCbColorView->SLICE_MAX   = 0;
        pCbColorView->MIP_LEVEL   = 0;
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
        if (m_flags.useSubresBaseAddr != 0)
        {
            // Get the base address of each array slice.
            const gpusize subresBaseAddr = m_pImage->Parent()->GetSubresourceBaseAddr(m_subresource);

            const auto*   pTileInfo   = AddrMgr2::GetTileInfo(m_pImage->Parent(), m_subresource);
            const gpusize pipeBankXor = pTileInfo->pipeBankXor;
            const gpusize addrWithXor = subresBaseAddr | (pipeBankXor << 8);

            pPm4Img->cbColorBase.bits.BASE_256B = Get256BAddrLo(addrWithXor);
            PAL_ASSERT(Get256BAddrHi(addrWithXor) == 0);
        }
        else
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
}

// =====================================================================================================================
Gfx9ColorTargetView::Gfx9ColorTargetView(
    const Device*                     pDevice,
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalInfo)
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
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalInfo)
{
    const MergedFmtInfo*const pFmtInfo =
        MergedChannelFmtInfoTbl(GfxIpLevel::GfxIp9, &m_pDevice->GetPlatform()->PlatformSettings());

    regCB_COLOR0_INFO cbColorInfo = InitCommonCbColorInfo(createInfo, pFmtInfo);

    // Most register values are simple to compute but vary based on whether or not this is a buffer view. Let's set
    // them all up-front before we get on to the harder register values.
    if (m_flags.isBufferView)
    {
        PAL_ASSERT(createInfo.bufferInfo.offset == 0);

        InitCommonBufferView(createInfo, &m_pm4Cmds, &m_pm4Cmds.cbColorView.gfx09);

        m_extent.width  = createInfo.bufferInfo.extent;
        m_extent.height = 1;

        m_pm4Cmds.cbColorAttrib.core.FORCE_DST_ALPHA_1 = Formats::HasUnusedAlpha(createInfo.swizzledFormat) ? 1 : 0;
        m_pm4Cmds.cbColorAttrib.core.NUM_SAMPLES       = 0;
        m_pm4Cmds.cbColorAttrib.core.NUM_FRAGMENTS     = 0;
        m_pm4Cmds.cbColorAttrib.gfx09.MIP0_DEPTH       = 0; // what is this?
        m_pm4Cmds.cbColorAttrib.gfx09.COLOR_SW_MODE    = SW_LINEAR;
        m_pm4Cmds.cbColorAttrib.gfx09.RESOURCE_TYPE    = static_cast<uint32>(ImageType::Tex1d); // no HW enums
        m_pm4Cmds.cbColorAttrib.gfx09.RB_ALIGNED       = 0;
        m_pm4Cmds.cbColorAttrib.gfx09.PIPE_ALIGNED     = 0;
        m_pm4Cmds.cbColorAttrib.gfx09.FMASK_SW_MODE    = SW_LINEAR; // ignored as there is no fmask
        m_pm4Cmds.cbColorAttrib.gfx09.META_LINEAR      = 0;         // linear meta surfaces not supported on gfx9
        m_pm4Cmds.cbMrtEpitch.bits.EPITCH              = (createInfo.bufferInfo.extent - 1);
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
        else if ((m_flags.useSubresBaseAddr == 0)                            &&
                 Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format) &&
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
                            &m_pm4Cmds,
                            &cbColorInfo,
                            &m_pm4Cmds.cbColorView.gfx09);

        m_extent.width  = extent.width;
        m_extent.height = extent.height;

        m_pm4Cmds.cbColorAttrib.core.NUM_SAMPLES       = Log2(imageCreateInfo.samples);
        m_pm4Cmds.cbColorAttrib.core.NUM_FRAGMENTS     = Log2(imageCreateInfo.fragments);
        m_pm4Cmds.cbColorAttrib.core.FORCE_DST_ALPHA_1 = Formats::HasUnusedAlpha(createInfo.swizzledFormat) ? 1 : 0;
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

// =====================================================================================================================
Gfx10ColorTargetView::Gfx10ColorTargetView(
    const Device*                     pDevice,
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalInfo)
    :
    ColorTargetView(pDevice, createInfo, internalInfo)
{
    memset(&m_pm4Cmds, 0, sizeof(m_pm4Cmds));
    memset(&m_uavExportSrd, 0, sizeof(m_uavExportSrd));
    m_gfx10Flags.u32All = 0;

    BuildPm4Headers();
    InitRegisters(createInfo, internalInfo);

    if (m_flags.viewVaLocked && (m_flags.isBufferView == 0))
    {
        UpdateImageVa(&m_pm4Cmds);
        if (m_pImage->Parent()->GetImageCreateInfo().imageType == ImageType::Tex2d)
        {
            UpdateImageSrd(&m_uavExportSrd);
        }
    }

    if (m_flags.viewVaLocked && (m_flags.isBufferView == 0))
    {
        m_gfx10Flags.colorBigPage = IsImageBigPageCompatible(*m_pImage, Gfx10AllowBigPageRenderTarget);
        m_gfx10Flags.fmaskBigPage = IsFmaskBigPageCompatible(*m_pImage, Gfx10AllowBigPageRenderTarget);
    }
    else if (m_flags.isBufferView)
    {
        m_gfx10Flags.colorBigPage = IsBufferBigPageCompatible(
                                        *static_cast<const GpuMemory*>(createInfo.bufferInfo.pGpuMemory),
                                        createInfo.bufferInfo.offset,
                                        createInfo.bufferInfo.extent,
                                        Gfx10AllowBigPageBuffers);
    }
}

// =====================================================================================================================
// Builds the PM4 packet headers for an image of PM4 commands used to write this View object to hardware.
void Gfx10ColorTargetView::BuildPm4Headers()
{
    const CmdUtil& cmdUtil = m_pDevice->CmdUtil();

    // NOTE: The register offset will be updated at bind-time to reflect the actual slot this View is being bound to.
    size_t spaceNeeded = CmdUtil::ContextRegRmwSizeDwords;

    spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmCB_COLOR0_BASE, mmCB_COLOR0_VIEW, &m_pm4Cmds.hdrCbColorBase);
    spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmCB_COLOR0_ATTRIB, mmCB_COLOR0_FMASK, &m_pm4Cmds.hdrCbColorAttrib);
    spaceNeeded += cmdUtil.BuildSetOneContextReg(mmCB_COLOR0_DCC_BASE, &m_pm4Cmds.hdrCbColorDccBase);

    // Registers prior to this in the ordering are grouped in terms of the target index (i.e., there's all of the
    // MRT0 regs, then the MRT1 regs, etc.)  Therefore, registers of a given "type" (i.e., COLOR0_ATTRIB) are
    // separated by CbRegsPerSlot.  Starting with mmCB_COLOR0_ATTRIB2 though, the registers are grouped by "type"...
    // i.e., mmCB_COLOR0_ATTRIB2 immediately preceeds mmCB_COLOR1_ATTRIB2, etc.
    //
    // All following registers have to be written one at a time.
    //
    // This is part of the reason why PAL doesn't write the various BASE_EXT registers; all of these should be zero
    // anyway (assert protection added in the image-address-getter functions).
    spaceNeeded += cmdUtil.BuildSetOneContextReg(Gfx10::mmCB_COLOR0_ATTRIB2, &m_pm4Cmds.hdrCbColorAttrib2);
    spaceNeeded += cmdUtil.BuildSetOneContextReg(Gfx10::mmCB_COLOR0_ATTRIB3, &m_pm4Cmds.hdrCbColorAttrib3);

    m_pm4Cmds.spaceNeeded             = spaceNeeded;
    m_pm4Cmds.spaceNeededDecompressed = spaceNeeded;

    CommonBuildPm4Headers(&m_pm4Cmds);
}

// =====================================================================================================================
// Finalizes the PM4 packet image by setting up the register values used to write this View object to hardware.
void Gfx10ColorTargetView::InitRegisters(
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalInfo)
{
    const auto&            palDevice   = *m_pDevice->Parent();
    const Gfx9PalSettings& settings    = GetGfx9Settings(palDevice);

    const MergedFlatFmtInfo*const pFmtInfoTbl =
        MergedChannelFlatFmtInfoTbl(palDevice.ChipProperties().gfxLevel, &m_pDevice->GetPlatform()->PlatformSettings());

    regCB_COLOR0_INFO cbColorInfo    = InitCommonCbColorInfo(createInfo, pFmtInfoTbl);

    // Most register values are simple to compute but vary based on whether or not this is a buffer view. Let's set
    // them all up-front before we get on to the harder register values.
    if (m_flags.isBufferView)
    {
        InitCommonBufferView(createInfo, &m_pm4Cmds, &m_pm4Cmds.cbColorView.gfx10);

        m_extent.width  = createInfo.bufferInfo.extent;
        m_extent.height = 1;

        // Setup GFX10-specific registers here
        m_pm4Cmds.cbColorAttrib3.bits.MIP0_DEPTH    = 0;
        m_pm4Cmds.cbColorAttrib3.bits.COLOR_SW_MODE = SW_LINEAR;
        m_pm4Cmds.cbColorAttrib3.bits.RESOURCE_TYPE = static_cast<uint32>(ImageType::Tex1d); // no HW enums

        m_pm4Cmds.cbColorAttrib3.bits.FMASK_SW_MODE = SW_LINEAR; // ignored as there is no fmask
        m_pm4Cmds.cbColorAttrib3.bits.META_LINEAR   = 1;         // no meta-data, but should be set for linear surfaces

        // Specifying a non-zero buffer offset only works with linear-general surfaces
        cbColorInfo.gfx10.LINEAR_GENERAL            = 1;
        {
            m_pm4Cmds.cbColorAttrib.core.FORCE_DST_ALPHA_1 =
                Formats::HasUnusedAlpha(createInfo.swizzledFormat) ? 1 : 0;
            m_pm4Cmds.cbColorAttrib.core.NUM_SAMPLES       = 0;
            m_pm4Cmds.cbColorAttrib.core.NUM_FRAGMENTS     = 0;
        }
    }
    else
    {
        const auto*const            pImage          = m_pImage->Parent();
        const SubresId              baseSubRes      = { m_subresource.aspect, 0, 0 };
        const SubResourceInfo*const pBaseSubResInfo = pImage->SubresourceInfo(baseSubRes);
        const SubResourceInfo*const pSubResInfo     = pImage->SubresourceInfo(m_subresource);
        const auto*                 pAddrOutput     = m_pImage->GetAddrOutput(pSubResInfo);
        const auto&                 surfSetting     = m_pImage->GetAddrSettings(pSubResInfo);
        const auto&                 imageCreateInfo = pImage->GetImageCreateInfo();
        const ImageType             imageType       = m_pImage->GetOverrideImageType();
        const bool                  imgIsBc         = Formats::IsBlockCompressed(imageCreateInfo.swizzledFormat.format);
        const bool                  hasFmask        = m_pImage->HasFmaskData();

        // Extents are one of the things that could be changing on GFX10 with respect to certain surface formats,
        // so go with the simple approach here for now.
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
            const uint32  firstMipLevel = this->m_subresource.mipLevel;

            baseExtent.width  = Util::Clamp((pSubResInfo->extentElements.width  << firstMipLevel),
                                            pBaseSubResInfo->extentElements.width,
                                            pBaseSubResInfo->actualExtentElements.width);
            baseExtent.height = Util::Clamp((pSubResInfo->extentElements.height << firstMipLevel),
                                            pBaseSubResInfo->extentElements.height,
                                            pBaseSubResInfo->actualExtentElements.height);

            extent = pSubResInfo->extentElements;
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
        else if ((m_flags.useSubresBaseAddr == 0)                            &&
                 Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format) &&
                 ((createInfo.imageInfo.arraySize > 1) || (createInfo.imageInfo.baseSubRes.arraySlice != 0)))
        {
            baseExtent = pBaseSubResInfo->actualExtentTexels;
            m_pImage->PadYuvPlanarViewActualExtent(m_subresource, &baseExtent);
            modifiedYuvExtent = true;
        }

        // The view should be in terms of texels except when we're operating in terms of elements. This will only happen
        // when we're copying to an "expanded" format (e.g., R32G32B32). In this case we can't do native format writes
        // so we're going to write each element independently. The trigger for this case is a mismatched bpp.
        if (pSubResInfo->bitsPerTexel != Formats::BitsPerPixel(createInfo.swizzledFormat.format))
        {
            baseExtent = pBaseSubResInfo->extentElements;
            extent     = pSubResInfo->extentElements;
        }

        InitCommonImageView(createInfo,
                            internalInfo,
                            baseExtent,
                            &m_pm4Cmds,
                            &cbColorInfo,
                            &m_pm4Cmds.cbColorView.gfx10);

        m_extent.width  = extent.width;
        m_extent.height = extent.height;

        //  Not setting this can lead to functional issues.   It's not a performance measure.   Due to multiple mip
        //  levels possibly being within same 1K address space, CB can get confused
        {
            m_pm4Cmds.cbColorAttrib.core.NUM_SAMPLES       = Log2(imageCreateInfo.samples);
            m_pm4Cmds.cbColorAttrib.core.NUM_FRAGMENTS     = Log2(imageCreateInfo.fragments);
            m_pm4Cmds.cbColorAttrib.core.FORCE_DST_ALPHA_1 =
                Formats::HasUnusedAlpha(createInfo.swizzledFormat) ? 1 : 0;
            m_pm4Cmds.cbColorAttrib.gfx10Core.LIMIT_COLOR_FETCH_TO_256B_MAX =
                settings.waForce256bCbFetch && (m_subresource.mipLevel >= pAddrOutput->firstMipIdInTail);
        }

        m_pm4Cmds.cbColorAttrib3.bits.MIP0_DEPTH    =
            ((imageType == ImageType::Tex3d) ? imageCreateInfo.extent.depth : imageCreateInfo.arraySize) - 1;
        m_pm4Cmds.cbColorAttrib3.bits.COLOR_SW_MODE = AddrMgr2::GetHwSwizzleMode(surfSetting.swizzleMode);
        m_pm4Cmds.cbColorAttrib3.bits.RESOURCE_TYPE = static_cast<uint32>(imageType); // no HW enums
        m_pm4Cmds.cbColorAttrib3.bits.META_LINEAR   = m_pImage->IsSubResourceLinear(createInfo.imageInfo.baseSubRes);

        m_pm4Cmds.cbColorAttrib3.bits.CMASK_PIPE_ALIGNED = m_pImage->IsPipeAligned();
        m_pm4Cmds.cbColorAttrib3.bits.DCC_PIPE_ALIGNED   = m_pImage->IsPipeAligned();

        const AddrSwizzleMode fMaskSwizzleMode      = (hasFmask
                                                       ? m_pImage->GetFmask()->GetSwizzleMode()
                                                       : ADDR_SW_LINEAR /* ignored */);
        m_pm4Cmds.cbColorAttrib3.bits.FMASK_SW_MODE = AddrMgr2::GetHwSwizzleMode(fMaskSwizzleMode);

        // From the reg-spec:
        //  this bit or the CONFIG equivalent must be set if parts of FMask surface may be unmapped (such as in PRT`s).
        if (hasFmask &&
            (imageCreateInfo.flags.prt ||
             (settings.waDisableFmaskNofetchOpOnFmaskCompressionDisable &&
              cbColorInfo.bits.FMASK_COMPRESSION_DISABLE)))
        {
            {
                m_pm4Cmds.cbColorAttrib.gfx10Core.DISABLE_FMASK_NOFETCH_OPT = 1;
            }
        }
    }

    {
        m_pm4Cmds.cbColorAttrib3.gfx10Core.RESOURCE_LEVEL = 1;
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
// Writes the PM4 commands required to bind to a certain slot.  Returns the next unused DWORD in pCmdSpace.
uint32* Gfx10ColorTargetView::WriteCommands(
    uint32        slot,        // Bind slot
    ImageLayout   imageLayout, // Current image layout
    CmdStream*    pCmdStream,
    uint32*       pCmdSpace
    ) const
{
    const bool decompressedImage =
        (m_flags.isBufferView == 0) &&
        (ImageLayoutToColorCompressionState(m_layoutToState, imageLayout) != ColorCompressed);

    const Gfx10ColorTargetViewPm4Img* pPm4Commands = &m_pm4Cmds;
    // Spawn a local copy of the PM4 image, since some register values and offsets may need to be updated in this
    // method.  For some clients, the base address, Fmask address and Cmask address also need to be updated.  The
    // contents of the local copy will depend on which Image state is specified.
    Gfx10ColorTargetViewPm4Img patchedPm4Commands;

    if (slot != 0)
    {
        PAL_ASSERT(slot < MaxColorTargets);

        patchedPm4Commands = *pPm4Commands;
        pPm4Commands = &patchedPm4Commands;

        // Offset to add to most PM4 headers' register offset.  Note that all CB_COLOR*_ATTRIB[2|3] registers are
        // adjacent to one another, so for those we can just increment by 'slot'.
        const uint32 slotDelta = (slot * CbRegsPerSlot);

        patchedPm4Commands.hdrCbColorBase.bitfields2.reg_offset         += slotDelta;
        patchedPm4Commands.hdrCbColorDccBase.bitfields2.reg_offset      += slotDelta;
        patchedPm4Commands.hdrCbColorAttrib.bitfields2.reg_offset       += slotDelta;
        patchedPm4Commands.hdrCbColorAttrib2.bitfields2.reg_offset      += slot;
        patchedPm4Commands.hdrCbColorAttrib3.bitfields2.reg_offset      += slot;
        patchedPm4Commands.cbColorInfo.bitfields2.reg_offset            += slotDelta;
        patchedPm4Commands.loadMetaDataIndex.bitfields4.reg_offset      += slotDelta;
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

// =====================================================================================================================
// Writes an image SRD (for UAV exports) to the given memory location
void Gfx10ColorTargetView::GetImageSrd(
    void* pOut
    ) const
{
    if (m_flags.viewVaLocked == false)
    {
        UpdateImageSrd(pOut);
    }
    else
    {
        memcpy(pOut, &m_uavExportSrd, sizeof(m_uavExportSrd));
    }
}

// =====================================================================================================================
// Updates the cached image SRD (for UAV exports). This may need to get called at draw-time if viewVaLocked is false
void Gfx10ColorTargetView::UpdateImageSrd(
    void* pOut
    ) const
{
    PAL_ASSERT(m_flags.isBufferView == 0);
    PAL_ASSERT(m_pImage->Parent()->GetImageCreateInfo().imageType == ImageType::Tex2d);

    ImageViewInfo viewInfo = {};
    viewInfo.pImage          = GetImage()->Parent();
    viewInfo.viewType        = ImageViewType::Tex2d;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 478
    viewInfo.possibleLayouts =
    {
        Pal::LayoutShaderWrite | Pal::LayoutColorTarget,
        Pal::LayoutUniversalEngine
    };
#else
    viewInfo.flags.shaderWritable = true;
#endif
    viewInfo.swizzledFormat       = m_swizzledFormat;
    viewInfo.subresRange          = { m_subresource, 1, m_arraySize };

    m_pDevice->Parent()->CreateImageViewSrds(1, &viewInfo, pOut);
}

// =====================================================================================================================
// Reports if the color target view can support setting COLOR_BIG_PAGE in CB_RMI_GLC2_CACHE_CONTROL.
bool Gfx10ColorTargetView::IsColorBigPage() const
{
    // Buffer views and viewVaLocked image views have already computed whether they can support BIG_PAGE or not.  Other
    // cases have to check now in case the bound memory has changed.
    return (m_flags.viewVaLocked || m_flags.isBufferView) ?
               m_gfx10Flags.colorBigPage :
               IsImageBigPageCompatible(*m_pImage, Gfx10AllowBigPageRenderTarget);
}

// =====================================================================================================================
// Reports if the color target view can support setting FMASK_BIG_PAGE in CB_RMI_GLC2_CACHE_CONTROL.
bool Gfx10ColorTargetView::IsFmaskBigPage() const
{
    // viewVaLocked image views have already computed whether they can support BIG_PAGE or not.  Other cases have to
    // check now in case the bound memory has changed.
    return m_flags.viewVaLocked ? m_gfx10Flags.fmaskBigPage :
                                  IsFmaskBigPageCompatible(*m_pImage, Gfx10AllowBigPageRenderTarget);
}

} // Gfx9
} // Pal
