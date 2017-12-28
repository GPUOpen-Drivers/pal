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

#include "core/addrMgr/addrMgr2/addrMgr2.h"
#include "core/hw/gfxip/universalCmdBuffer.h"
#include "core/hw/gfxip/gfx9/g_gfx9PalSettings.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9ColorTargetView.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9FormatInfo.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "palFormatInfo.h"

using namespace Util;
using namespace Pal::Formats::Gfx9;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
ColorTargetView::ColorTargetView(
    const Device*                            pDevice,
    const ColorTargetViewCreateInfo&         createInfo,
    const ColorTargetViewInternalCreateInfo& internalInfo)
    :
    m_pDevice(pDevice),
    m_pImage((createInfo.flags.isBufferView == 0) ? GetGfx9Image(createInfo.imageInfo.pImage) : nullptr),
    m_createInfo(createInfo),
    m_internalInfo(internalInfo)
{
    memset(&m_subresource, 0, sizeof(m_subresource));

    m_flags.u32All = 0;
    // Note that buffew views have their VA ranges locked because they cannot have their memory rebound.
    m_flags.isBufferView = createInfo.flags.isBufferView;
    m_flags.viewVaLocked = createInfo.flags.imageVaLocked || createInfo.flags.isBufferView;

    if (m_flags.isBufferView == 0)
    {
        PAL_ASSERT(createInfo.imageInfo.pImage != nullptr);

        m_flags.usesLoadRegIndexPkt = m_pImage->Parent()->GetDevice()->ChipProperties().gfx9.supportLoadRegIndexPkt;

        // Sets the base subresource for this mip.
        m_subresource.aspect      = createInfo.imageInfo.baseSubRes.aspect;
        m_subresource.mipLevel    = createInfo.imageInfo.baseSubRes.mipLevel;
        const ImageType imageType = m_pImage->GetOverrideImageType();
        if (imageType == Pal::ImageType::Tex3d)
        {
            if (createInfo.flags.zRangeValid)
            {
                m_zRange = createInfo.zRange;
            }
            else
            {
                PAL_ASSERT(createInfo.imageInfo.arraySize != 0);
                m_zRange.offset = createInfo.imageInfo.baseSubRes.arraySlice;
                m_zRange.extent = createInfo.imageInfo.arraySize;
            }
        }
        else
        {
            PAL_ASSERT(createInfo.imageInfo.arraySize != 0);
            m_subresource.arraySlice = createInfo.imageInfo.baseSubRes.arraySlice;
            m_arraySize              = createInfo.imageInfo.arraySize;
        }

        m_flags.hasDcc        = m_pImage->HasDccData();
        m_flags.hasCmaskFmask = m_pImage->HasFmaskData();

        // If this view has DCC data and this is a decompress operation we must set "isDccCompress"
        // to zero. If the view has DCC data and this is a normal render operation we should set
        // "isDccCompress" to one as we expect the CB to write to DCC.
        m_flags.isDccDecompress = internalInfo.flags.dccDecompress;

        if (m_pDevice->Settings().waitOnMetadataMipTail)
        {
            m_flags.waitOnMetadataMipTail = m_pImage->IsInMetadataMipTail(MipLevel());
        }
    }
}

// =====================================================================================================================
// Helper function which adds commands into the command stream when the currently-bound color targets are changing.
// Returns the address to where future commands will be written.
uint32* ColorTargetView::HandleBoundTargetsChanged(
    const Device& device,
    CmdStream*    pCmdStream,
    uint32*       pCmdSpace)
{
    const EngineType  engineType = pCmdStream->GetEngineType();

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
        pCmdSpace += device.CmdUtil().BuildNonSampleEventWrite(BREAK_BATCH, engineType, pCmdSpace);
    }

    // If you change the mips of a resource being rendered-to, regardless of which MRT slot it is bound to, we need
    // to flush the CB metadata caches (DCC, Fmask, Cmask). This protects against the case where a DCC, Fmask or Cmask
    // cacheline can contain data from two different mip levels in different RB's.
    pCmdSpace += device.CmdUtil().BuildNonSampleEventWrite(FLUSH_AND_INV_CB_META, engineType, pCmdSpace);

    // Unfortunately, the FLUSH_AND_INV_CB_META event doesn't actually flush the DCC cache. Instead, it only flushes the
    // Fmask and Cmask caches, along with the overwrite combiner. So we also need to issue another event to flush the CB
    // pixel data caches, which will also flush the DCC cache.
    pCmdSpace += device.CmdUtil().BuildNonSampleEventWrite(FLUSH_AND_INV_CB_PIXEL_DATA, engineType, pCmdSpace);

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
                            (m_pImage->LayoutToColorCompressionState(imageLayout) == ColorCompressed);

    if (compressed && m_flags.hasDcc)
    {
        const SubresRange range = {m_subresource, 1, m_arraySize};
        m_pImage->UpdateDccStateMetaData(pCmdStream,
                                         range,
                                         &m_zRange,
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

    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmCB_COLOR0_CLEAR_WORD0 + slotRegIncr,
                                                   mmCB_COLOR0_CLEAR_WORD1 + slotRegIncr,
                                                   color,
                                                   pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Builds the common PM4 packet headers
template <typename Pm4ImgType>
void ColorTargetView::CommonBuildPm4Headers(
    bool        useCompression,
    Pm4ImgType* pPm4Img
    ) const
{
    const CmdUtil& cmdUtil = m_pDevice->CmdUtil();

    pPm4Img->spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmPA_SC_GENERIC_SCISSOR_TL,
                                                           mmPA_SC_GENERIC_SCISSOR_BR,
                                                           &pPm4Img->hdrPaScGenericScissor);

    if (useCompression && m_flags.hasDcc)
    {
        // On GFX9, if we have DCC we also have fast clear metadata. This class assumes this will always be true.
        PAL_ASSERT(m_pImage->HasFastClearMetaData());

        // If the parent Image has DCC memory, then we need to add a LOAD_CONTEXT_REG packet to load the image's
        // fast-clear metadata.
        //
        // NOTE: Just because we have DCC data doesn't mean that we're doing fast-clears. Writing this register
        // shouldn't hurt anything though. We do not know the GPU virtual address of the metadata until bind-time.
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
}

// =====================================================================================================================
template <typename Pm4ImgType>
void ColorTargetView::InitCommonBufferView(
    Pm4ImgType* pPm4Img
    ) const
{
    PAL_ASSERT(m_createInfo.bufferInfo.pGpuMemory != nullptr);

    // The buffer virtual address is simply "offset" pixels from the start of the GPU memory's virtual address.
    const gpusize bufferOffset = m_createInfo.bufferInfo.offset *
                                 Formats::BytesPerPixel(m_createInfo.swizzledFormat.format);
    const gpusize bufferAddr   = m_createInfo.bufferInfo.pGpuMemory->Desc().gpuVirtAddr + bufferOffset;

    // Convert to a 256-bit aligned base address and a base offset. Note that we don't need to swizzle the base
    // address because buffers aren't macro tiled.
    const gpusize baseOffset = bufferAddr & 0xFF;
    const gpusize baseAddr   = bufferAddr & (~0xFF);

    pPm4Img->cbColorBase.bits.BASE_256B = Get256BAddrLo(baseAddr);

    // The view slice_start is overloaded to specify the base offset.
    pPm4Img->cbColorView.bits.SLICE_START = baseOffset;
    pPm4Img->cbColorView.bits.SLICE_MAX   = 0;
    pPm4Img->cbColorView.bits.MIP_LEVEL   = 0;

    // According to the other UMDs, this is the absolute max mip level. For one mip level, the MAX_MIP is mip #0.
    pPm4Img->cbColorAttrib2.bits.MAX_MIP     = 0;

    // From testing this is not the padded mip height/width, but the pixel height/width specified by the client.
    pPm4Img->cbColorAttrib2.bits.MIP0_HEIGHT = 0;
    pPm4Img->cbColorAttrib2.bits.MIP0_WIDTH  = m_createInfo.bufferInfo.extent - 1;

    pPm4Img->paScGenericScissorTl.bits.WINDOW_OFFSET_DISABLE = true;
    pPm4Img->paScGenericScissorTl.bits.TL_X = 0;
    pPm4Img->paScGenericScissorTl.bits.TL_Y = 0;
    pPm4Img->paScGenericScissorBr.bits.BR_X = m_createInfo.bufferInfo.extent;
    pPm4Img->paScGenericScissorBr.bits.BR_Y = 1;

    pPm4Img->cbColorAttrib.bits.FORCE_DST_ALPHA_1 = Formats::HasUnusedAlpha(m_createInfo.swizzledFormat) ? 1 : 0;
    pPm4Img->cbColorAttrib.bits.NUM_SAMPLES       = 0;
    pPm4Img->cbColorAttrib.bits.NUM_FRAGMENTS     = 0;
}

// =====================================================================================================================
template <typename FmtInfoType>
void ColorTargetView::InitCommonCbColorInfo(
    const FmtInfoType*  pFmtInfo,
    regCB_COLOR0_INFO*  pCbColorInfo
    ) const
{
    const Pal::Device* pParentDevice = m_pDevice->Parent();
    const auto&        settings      = GetGfx9Settings(*pParentDevice);

    pCbColorInfo->bits.ENDIAN       = ENDIAN_NONE;
    pCbColorInfo->bits.FORMAT       = Formats::Gfx9::HwColorFmt(pFmtInfo, m_createInfo.swizzledFormat.format);
    pCbColorInfo->bits.NUMBER_TYPE  = Formats::Gfx9::ColorSurfNum(pFmtInfo, m_createInfo.swizzledFormat.format);
    pCbColorInfo->bits.COMP_SWAP    = Formats::Gfx9::ColorCompSwap(m_createInfo.swizzledFormat);

    // Set bypass blending for any format that is not blendable. Blend clamp must be cleared if blend_bypass is set.
    // Otherwise, it must be set iff any component is SNORM, UNORM, or SRGB.
    const bool blendBypass  =
        (pParentDevice->SupportsBlend(m_createInfo.swizzledFormat.format, ImageTiling::Optimal) == false);
    const bool isNormOrSrgb = Formats::IsNormalized(m_createInfo.swizzledFormat.format) ||
                              Formats::IsSrgb(m_createInfo.swizzledFormat.format);
    const bool blendClamp   = (blendBypass == false) && isNormOrSrgb;

    // Selects between truncating (standard for floats) and rounding (standard for most other cases) to convert blender
    // results to frame buffer components. Round mode must be set to ROUND_BY_HALF if any component is UNORM, SNORM or
    // SRGB otherwise ROUND_TRUNCATE.
    const RoundMode roundMode = isNormOrSrgb ? ROUND_BY_HALF : ROUND_TRUNCATE;

    pCbColorInfo->bits.BLEND_CLAMP  = blendClamp;
    pCbColorInfo->bits.BLEND_BYPASS = blendBypass;
    pCbColorInfo->bits.SIMPLE_FLOAT = Pal::Device::CbSimpleFloatEnable;
    pCbColorInfo->bits.ROUND_MODE   = roundMode;
}

// =====================================================================================================================
template <typename Pm4ImgType>
void ColorTargetView::InitCommonImageView(
    bool                useCompression,
    const Extent3d&     baseExtent,
    const Extent3d&     extent,
    Pm4ImgType*         pPm4Img,
    regCB_COLOR0_INFO*  pCbColorInfo
    ) const
{
    const Pal::Device*      pParentDevice   = m_pDevice->Parent();
    const ImageCreateInfo&  imageCreateInfo = m_pImage->Parent()->GetImageCreateInfo();
    const ImageType         imageType       = m_pImage->GetOverrideImageType();
    const auto&             settings        = GetGfx9Settings(*pParentDevice);
    const auto              gfxLevel        = pParentDevice->ChipProperties().gfxLevel;

    pPm4Img->paScGenericScissorTl.bits.WINDOW_OFFSET_DISABLE = true;
    pPm4Img->paScGenericScissorTl.bits.TL_X = 0;
    pPm4Img->paScGenericScissorTl.bits.TL_Y = 0;
    pPm4Img->paScGenericScissorBr.bits.BR_X = extent.width;
    pPm4Img->paScGenericScissorBr.bits.BR_Y = extent.height;

    pPm4Img->cbColorAttrib.bits.NUM_SAMPLES       = Log2(imageCreateInfo.samples);
    pPm4Img->cbColorAttrib.bits.NUM_FRAGMENTS     = Log2(imageCreateInfo.fragments);
    pPm4Img->cbColorAttrib.bits.FORCE_DST_ALPHA_1 = Formats::HasUnusedAlpha(m_createInfo.swizzledFormat) ? 1 : 0;

    // According to the other UMDs, this is the absolute max mip level. For one mip level, the MAX_MIP is mip #0.
    pPm4Img->cbColorAttrib2.bits.MAX_MIP = (imageCreateInfo.mipLevels - 1);

    if ((m_createInfo.flags.zRangeValid == 1) && (imageType == ImageType::Tex3d))
    {
        pPm4Img->cbColorView.bits.SLICE_START = m_createInfo.zRange.offset;
        pPm4Img->cbColorView.bits.SLICE_MAX   = m_createInfo.zRange.offset + m_createInfo.zRange.extent - 1;
        pPm4Img->cbColorView.bits.MIP_LEVEL   = m_createInfo.imageInfo.baseSubRes.mipLevel;
    }
    else
    {
        const uint32 baseArraySlice = m_createInfo.imageInfo.baseSubRes.arraySlice;

        pPm4Img->cbColorView.bits.SLICE_START = baseArraySlice;
        pPm4Img->cbColorView.bits.SLICE_MAX   = baseArraySlice + m_createInfo.imageInfo.arraySize - 1;
        pPm4Img->cbColorView.bits.MIP_LEVEL   = m_createInfo.imageInfo.baseSubRes.mipLevel;
    }

    if (useCompression && m_flags.hasDcc)
    {
        pPm4Img->cbColorDccControl  = m_pImage->GetDcc()->GetControlReg();
        pCbColorInfo->bits.DCC_ENABLE = 1;
    }

    if (useCompression && m_flags.hasCmaskFmask)
    {
        // Check if we can keep fmask in a compressed state and avoid corresponding fmask decompression
        const bool fMaskTexFetchAllowed = m_pImage->IsComprFmaskShaderReadable(m_subresource);

        // Setup CB_COLOR*_INFO register fields which depend on CMask or fMask state:
        pCbColorInfo->bits.COMPRESSION               = 1;
        pCbColorInfo->bits.FMASK_COMPRESSION_DISABLE = settings.fmaskCompressDisable;

        if (fMaskTexFetchAllowed                           &&
            (m_internalInfo.flags.dccDecompress  == false) &&
            (m_internalInfo.flags.fmaskDecompess == false))
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
    // The "GetSubresource256BAddrSwizzled" function will crash if no memory has been bound to
    // the associated image yet, so don't do anything if it's not safe
    if (m_pImage->Parent()->GetBoundGpuMemory().IsBound())
    {
        // Program Color Buffer base address
        pPm4Img->cbColorBase.bits.BASE_256B = m_pImage->GetSubresource256BAddrSwizzled(m_subresource);

        // On GFX9, only DCC can be used for fast clears.  The load-meta-data packet updates the cb color regs to
        // indicate what the clear color is.  (See Gfx9FastColorClearMetaData in gfx9MaskRam.h).
        if (m_flags.hasDcc)
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
                metaDataVirtAddr -= (sizeof(uint32) * pPm4Img->loadMetaData.bitfields4.reg_offset);

                pPm4Img->loadMetaData.bitfields2.base_addr_lo = (LowPart(metaDataVirtAddr) >> 2);
                pPm4Img->loadMetaData.base_addr_hi            = HighPart(metaDataVirtAddr);
            }
            else
            {
                pPm4Img->loadMetaDataIndex.bitfields2.mem_addr_lo = (LowPart(metaDataVirtAddr) >> 2);
                pPm4Img->loadMetaDataIndex.mem_addr_hi            = HighPart(metaDataVirtAddr);
            }

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
template <typename Pm4ImgType>
uint32* ColorTargetView::WriteCommandsInternal(
    CmdStream*        pCmdStream,
    uint32*           pCmdSpace,
    const Pm4ImgType& pm4Img
    ) const
{
    PAL_ASSERT(pCmdStream != nullptr);

    if (m_flags.viewVaLocked)
    {
        pCmdSpace = pCmdStream->WritePm4Image(pm4Img.spaceNeeded, &pm4Img, pCmdSpace);
    }
    // The "GetSubresource256BAddrSwizzled" function will crash if no memory has been bound to
    // the associated image yet, so don't do anything if it's not safe
    else if (m_pImage->Parent()->GetBoundGpuMemory().IsBound())
    {
        // Spawn a local copy of the PM4 image, since the register offsets, Base address, Fmask
        // address and Cmask address need to be updated in this method. The contents of the local
        // copy will depend on which Image state is specified.
        Pm4ImgType pm4Commands = pm4Img;
        UpdateImageVa<Pm4ImgType>(&pm4Commands);

        pCmdSpace = pCmdStream->WritePm4Image(pm4Commands.spaceNeeded, &pm4Commands, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
Gfx9ColorTargetView::Gfx9ColorTargetView(
    const Device*                            pDevice,
    const ColorTargetViewCreateInfo&         createInfo,
    const ColorTargetViewInternalCreateInfo& internalInfo)
    :
    ColorTargetView(pDevice, createInfo, internalInfo)
{
    memset(&m_pm4CmdsCompressed,   0, sizeof(m_pm4CmdsCompressed));
    memset(&m_pm4CmdsDecompressed, 0, sizeof(m_pm4CmdsDecompressed));
}

// =====================================================================================================================
// Builds the PM4 packet headers for an image of PM4 commands used to write this View object to hardware.
void Gfx9ColorTargetView::BuildPm4Headers(
    bool                       useCompression, // If true, enables color compression if available
    Gfx9ColorTargetViewPm4Img* pPm4Img         // Output PM4 image to initialize packet headers for
    ) const
{
    PAL_ASSERT(pPm4Img != nullptr);

    const CmdUtil& cmdUtil = m_pDevice->CmdUtil();

    CommonBuildPm4Headers<Gfx9ColorTargetViewPm4Img>(useCompression, pPm4Img);

    // 1st PM4 set data packet: sets the context registers CB_COLOR*_BASE through
    // CB_COLOR*_VIEW.
    pPm4Img->spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmCB_COLOR0_BASE,
                                                           mmCB_COLOR0_VIEW,
                                                           &pPm4Img->hdrCbColorBase);

    // 2nd PM4 packet, a register read/modify/write of CB_COLOR*_INFO.
    // The real packet will be created later, we just need to get the size here.
    //
    // NOTE: The register offset will be updated at bind-time to reflect the actual slot this
    // View is being bound to.
    pPm4Img->spaceNeeded += CmdUtil::ContextRegRmwSizeDwords;

    // 3rd PM4 set data packet: sets the context registers CB_COLOR*_ATTRIB through
    // CB_COLOR*_DCC_BASE_EXT
    pPm4Img->spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmCB_COLOR0_ATTRIB,
                                                           mmCB_COLOR0_DCC_BASE_EXT__GFX09,
                                                           &pPm4Img->hdrCbColorAttrib);

    // 5th PM4 set data packet sets the CB_MRTx_EPITCH register.  CB_MRT0_EPITCH through CB_MRT7_EPITCH are
    // located consecutively in addressing space
    pPm4Img->spaceNeeded += cmdUtil.BuildSetOneContextReg(mmCB_MRT0_EPITCH__GFX09, &pPm4Img->hdrCbMrtEpitch);
}

// =====================================================================================================================
// Performs Gfx9 hardware-specific initialization for a color target view object, including:
//  - Initialize the PM4 commands used to write the pipeline to HW.
void Gfx9ColorTargetView::Init()
{
    BuildPm4Headers(true,  &m_pm4CmdsCompressed);
    BuildPm4Headers(false, &m_pm4CmdsDecompressed);

    InitRegisters(true,  &m_pm4CmdsCompressed);
    InitRegisters(false, &m_pm4CmdsDecompressed);

    if (m_flags.viewVaLocked && (m_flags.isBufferView == 0))
    {
        UpdateImageVa<Gfx9ColorTargetViewPm4Img>(&m_pm4CmdsCompressed);
        UpdateImageVa<Gfx9ColorTargetViewPm4Img>(&m_pm4CmdsDecompressed);
    }
}

// =====================================================================================================================
// Finalizes the PM4 packet image by setting up the register values used to write this View object to hardware.
void Gfx9ColorTargetView::InitRegisters(
    bool                        useCompression, // If true, enables color compression if available
    Gfx9ColorTargetViewPm4Img*  pPm4Img         // Output PM4 image to initialize packet headers for
    ) const
{
    const Pal::Device*    pPalDevice = m_pDevice->Parent();
    const auto&           settings   = GetGfx9Settings(*pPalDevice);
    const GfxIpLevel      gfxLevel   = pPalDevice->ChipProperties().gfxLevel;
    const MergedFmtInfo*  pFmtInfo   = MergedChannelFmtInfoTbl(gfxLevel);

    regCB_COLOR0_INFO cbColorInfo = {};

    // Most register values are simple to compute but vary based on whether or not this is a buffer view. Let's set
    // them all up-front before we get on to the harder register values.
    if (m_flags.isBufferView)
    {
        PAL_ASSERT(m_createInfo.bufferInfo.offset == 0);

        InitCommonBufferView<Gfx9ColorTargetViewPm4Img>(pPm4Img);

        // Setup GFX9 specific registers here
        pPm4Img->cbColorAttrib.bits.MIP0_DEPTH    = 0; // what is this?
        pPm4Img->cbColorAttrib.bits.COLOR_SW_MODE = SW_LINEAR;
        pPm4Img->cbColorAttrib.bits.RESOURCE_TYPE = static_cast<uint32>(ImageType::Tex1d); // no HW enums
        pPm4Img->cbColorAttrib.bits.RB_ALIGNED    = 0;
        pPm4Img->cbColorAttrib.bits.PIPE_ALIGNED  = 0;
        pPm4Img->cbColorAttrib.bits.FMASK_SW_MODE = SW_LINEAR; // ignored as there is no fmask
        pPm4Img->cbColorAttrib.bits.META_LINEAR   = 0;         // linear meta surfaces not supported on gfx9

        pPm4Img->cbMrtEpitch.bits.EPITCH = m_createInfo.bufferInfo.extent - 1;
    }
    else
    {
        // Override the three variables defined above.
        const SubresId              baseSubRes      = { m_subresource.aspect, 0, 0 };
        const SubResourceInfo*const pBaseSubResInfo = m_pImage->Parent()->SubresourceInfo(baseSubRes);
        const SubResourceInfo*const pSubResInfo     = m_pImage->Parent()->SubresourceInfo(m_subresource);
        const auto&                 surfSetting     = m_pImage->GetAddrSettings(pSubResInfo);
        const auto*const            pAddrOutput     = m_pImage->GetAddrOutput(pBaseSubResInfo);
        const ImageCreateInfo&      imageCreateInfo = m_pImage->Parent()->GetImageCreateInfo();
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
        if (imgIsBc || (pSubResInfo->bitsPerTexel != Formats::BitsPerPixel(m_createInfo.swizzledFormat.format)))
        {
            baseExtent = pBaseSubResInfo->extentElements;
            extent     = pSubResInfo->extentElements;
        }

        bool modifiedYuvExtent = false;
        if (Formats::IsYuvPacked(pSubResInfo->format.format)                    &&
            (Formats::IsYuvPacked(m_createInfo.swizzledFormat.format) == false) &&
            ((pSubResInfo->bitsPerTexel << 1) == Formats::BitsPerPixel(m_createInfo.swizzledFormat.format)))
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
                 ((m_createInfo.imageInfo.arraySize > 1) || (m_createInfo.imageInfo.baseSubRes.arraySlice != 0)))
        {
            baseExtent = pBaseSubResInfo->actualExtentTexels;
            m_pImage->PadYuvPlanarViewActualExtent(m_subresource, &baseExtent);
            modifiedYuvExtent = true;
        }

        // The view should be in terms of texels except when we're operating in terms of elements. This will only happen
        // when we're copying to an "expanded" format (e.g., R32G32B32). In this case we can't do native format writes
        // so we're going to write each element independently. The trigger for this case is a mismatched bpp.
        if (pSubResInfo->bitsPerTexel != Formats::BitsPerPixel(m_createInfo.swizzledFormat.format))
        {
            baseExtent = pBaseSubResInfo->extentElements;
            extent     = pSubResInfo->extentElements;
        }

        InitCommonImageView<Gfx9ColorTargetViewPm4Img>(useCompression,
                                                       baseExtent,
                                                       extent,
                                                       pPm4Img,
                                                       &cbColorInfo);

        pPm4Img->cbColorAttrib.bits.MIP0_DEPTH    = ((imageType == ImageType::Tex3d)
                                                        ? imageCreateInfo.extent.depth
                                                        : imageCreateInfo.arraySize) - 1;
        pPm4Img->cbColorAttrib.bits.COLOR_SW_MODE = AddrMgr2::GetHwSwizzleMode(surfSetting.swizzleMode);
        pPm4Img->cbColorAttrib.bits.RESOURCE_TYPE = static_cast<uint32>(imageType); // no HW enums
        pPm4Img->cbColorAttrib.bits.RB_ALIGNED    = m_pImage->IsRbAligned();
        pPm4Img->cbColorAttrib.bits.PIPE_ALIGNED  = m_pImage->IsPipeAligned();
        pPm4Img->cbColorAttrib.bits.META_LINEAR   = 0;

        const AddrSwizzleMode  fMaskSwizzleMode   = (m_pImage->HasFmaskData()
                                                     ? m_pImage->GetFmask()->GetSwizzleMode()
                                                     : ADDR_SW_LINEAR /* ignored */);
        pPm4Img->cbColorAttrib.bits.FMASK_SW_MODE = AddrMgr2::GetHwSwizzleMode(fMaskSwizzleMode);

        if (modifiedYuvExtent)
        {
            pPm4Img->cbMrtEpitch.bits.EPITCH =
                ((pAddrOutput->epitchIsHeight ? baseExtent.height : baseExtent.width) - 1);
        }
        else
        {
            pPm4Img->cbMrtEpitch.bits.EPITCH = AddrMgr2::CalcEpitch(pAddrOutput);
        }
    }

    InitCommonCbColorInfo<MergedFmtInfo>(pFmtInfo, &cbColorInfo);

    // The CB_COLOR0_INFO RMW packet requires a mask. We want everything but these two bits,
    // so we'll use the inverse of them.
    constexpr uint32 RmwCbColorInfoMask =
        static_cast<const uint32>(~(CB_COLOR0_INFO__BLEND_OPT_DONT_RD_DST_MASK |
                                    CB_COLOR0_INFO__BLEND_OPT_DISCARD_PIXEL_MASK));

    // All relevent register data has now been calculated; create the RMW packet.
    m_pDevice->CmdUtil().BuildContextRegRmw(mmCB_COLOR0_INFO,
                                            RmwCbColorInfoMask,
                                            cbColorInfo.u32All,
                                            &pPm4Img->cbColorInfo);
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
    const bool compressed = (m_flags.isBufferView == 0) &&
                            (m_pImage->LayoutToColorCompressionState(imageLayout) == ColorCompressed);

    const Gfx9ColorTargetViewPm4Img* pPm4Commands = (compressed ? &m_pm4CmdsCompressed : &m_pm4CmdsDecompressed);
    // Spawn a local copy of the PM4 image, since the register offsets need to be updated in this method.  For some
    // clients, the base address, Fmask address and Cmask address also need to be updated.  The contents of the local
    // copy will depend on which Image state is specified.
    Gfx9ColorTargetViewPm4Img patchedPm4Commands;

    if (slot != 0)
    {
        PAL_ASSERT(slot < MaxColorTargets);

        patchedPm4Commands = *pPm4Commands;
        pPm4Commands = &patchedPm4Commands;

        // Offset to add to most PM4 headers' register offset.  Note that all CB_MRT*_EPITCH registers are adjacent
        // to one another, so for that one we can just increment by 'slot'.
        const uint32 slotDelta = (slot * CbRegsPerSlot);

        patchedPm4Commands.hdrCbColorBase.bitfields2.reg_offset   += slotDelta;
        patchedPm4Commands.hdrCbColorAttrib.bitfields2.reg_offset += slotDelta;
        patchedPm4Commands.hdrCbMrtEpitch.bitfields2.reg_offset   += slot;
        patchedPm4Commands.cbColorInfo.bitfields2.reg_offset      += slotDelta;
        patchedPm4Commands.loadMetaData.bitfields4.reg_offset     += slotDelta;

        if ((m_flags.usesLoadRegIndexPkt == 0) && (m_flags.viewVaLocked != 0))
        {
            // If this view uses the legacy LOAD_CONTEXT_REG packet to load fast-clear registers, we need to update
            // the metadata load address along with the register offset.  See UpdateImageVa() for more details.
            const gpusize metaDataVirtAddrX4 =
                ((static_cast<uint64>(patchedPm4Commands.loadMetaData.base_addr_hi) << 32) |
                 patchedPm4Commands.loadMetaData.bitfields2.base_addr_lo) -
                slotDelta;

           patchedPm4Commands.loadMetaData.bitfields2.base_addr_lo = LowPart(metaDataVirtAddrX4);
           patchedPm4Commands.loadMetaData.base_addr_hi            = HighPart(metaDataVirtAddrX4);
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

} // Gfx9
} // Pal
