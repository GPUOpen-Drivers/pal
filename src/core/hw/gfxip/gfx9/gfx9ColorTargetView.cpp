/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/pm4UniversalCmdBuffer.h"
#include "g_gfx9Settings.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9ColorTargetView.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9FormatInfo.h"
#include "palFormatInfo.h"
#include <type_traits>

using namespace Util;
using namespace Pal::Formats::Gfx9;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
ColorTargetView::ColorTargetView(
    const Device*                     pDevice,
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalInfo,
    uint32                            uniqueId)
    :
    m_pImage((createInfo.flags.isBufferView == 0) ? GetGfx9Image(createInfo.imageInfo.pImage) : nullptr),
    m_gfxLevel(pDevice->Parent()->ChipProperties().gfxLevel),
    m_arraySize(0),
    m_uniqueId(uniqueId)
{
    const Gfx9PalSettings& settings = pDevice->Settings();

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

        m_flags.hasMultipleFragments = (imageCreateInfo.fragments > 1);

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
            m_flags.hasDcc        = m_pImage->HasDccData();
            m_flags.hasCmaskFmask = m_pImage->HasFmaskData();
        }

        // If this view has DCC data and this is a decompress operation we must set "isDccCompress"
        // to zero. If the view has DCC data and this is a normal render operation we should set
        // "isDccCompress" to one as we expect the CB to write to DCC.
        m_flags.isDccDecompress = internalInfo.flags.dccDecompress;

        if (settings.waitOnMetadataMipTail)
        {
            m_flags.waitOnMetadataMipTail = m_pImage->IsInMetadataMipTail(m_subresource);
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
            // YUV planar surfaces can have DCC on GFX10+ as the slices are individually addressable
            // on those platforms.
            PAL_ASSERT((IsGfx10Plus(m_gfxLevel)) || (m_flags.hasDcc == 0));

            // There's no reason to ever have MSAA YUV, so there won't ever be cMask or fMask surfaces.
            PAL_ASSERT(m_flags.hasCmaskFmask == 0);

            m_flags.useSubresBaseAddr = 1;
        }

        // Determine whether Overwrite Combiner (OC) should be to be disabled or not
        if (settings.waRotatedSwizzleDisablesOverwriteCombiner)
        {
            const SubresId  subResId = { m_subresource.plane, MipLevel(), 0 };
            const SubResourceInfo*const pSubResInfo  = m_pImage->Parent()->SubresourceInfo(subResId);
            const auto&                 surfSettings = m_pImage->GetAddrSettings(pSubResInfo);

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
    const CmdUtil&  cmdUtil,
    uint32*         pCmdSpace)
{
    // If you change the mips of a resource being rendered-to, regardless of which MRT slot it is bound to, we need
    // to flush the CB metadata caches (DCC, Fmask, Cmask). This protects against the case where a DCC, Fmask or Cmask
    // cacheline can contain data from two different mip levels in different RB's.
    pCmdSpace += cmdUtil.BuildNonSampleEventWrite(FLUSH_AND_INV_CB_META, EngineTypeUniversal, pCmdSpace);

    // Unfortunately, the FLUSH_AND_INV_CB_META event doesn't actually flush the DCC cache. Instead, it only flushes the
    // Fmask and Cmask caches, along with the overwrite combiner. So we also need to issue another event to flush the CB
    // pixel data caches, which will also flush the DCC cache.
    pCmdSpace += cmdUtil.BuildNonSampleEventWrite(FLUSH_AND_INV_CB_PIXEL_DATA, EngineTypeUniversal, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Writes the fast clear color register only to a new value.  This function is sometimes called after a fast clear
// when it is detected that the cleared image is already bound with the old fast clear value loaded.
uint32* ColorTargetView::WriteUpdateFastClearColor(
    uint32       slot,
    const uint32 color[4],
    CmdStream*   pCmdStream,
    uint32*      pCmdSpace
    ) const
{
#if PAL_BUILD_GFX11
    // These registers physically exist on GFX11 (for now...) but don't do anything.
    if (IsGfx11(m_gfxLevel) == false)
#endif
    {
        const uint32 slotOffset = (slot * CbRegsPerSlot);

        pCmdSpace = pCmdStream->WriteSetSeqContextRegs((Gfx09_10::mmCB_COLOR0_CLEAR_WORD0 + slotOffset),
                                                       (Gfx09_10::mmCB_COLOR0_CLEAR_WORD1 + slotOffset),
                                                       color,
                                                       pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
template <typename RegistersType, typename CbColorView>
void ColorTargetView::InitCommonBufferView(
    const Device&                    device,
    const ColorTargetViewCreateInfo& createInfo,
    RegistersType*                   pRegs,
    CbColorView*                     pCbColorView
    ) const
{
    PAL_ASSERT(createInfo.bufferInfo.pGpuMemory != nullptr);

    // The buffer virtual address is simply "offset" pixels from the start of the GPU memory's virtual address.
    const gpusize bufferOffset = (createInfo.bufferInfo.offset * Formats::BytesPerPixel(m_swizzledFormat.format));
    const gpusize bufferAddr   = (createInfo.bufferInfo.pGpuMemory->Desc().gpuVirtAddr + bufferOffset);

    // Convert to a 256-bit aligned base address and a base offset. Note that we don't need to swizzle the base
    // address because buffers aren't macro tiled.
    const gpusize baseOffset = bufferAddr & 0xFF;
    const gpusize baseAddr   = bufferAddr & (~0xFF);

    pRegs->cbColorBase.bits.BASE_256B    = Get256BAddrLo(baseAddr);
    pRegs->cbColorBaseExt.bits.BASE_256B = Get256BAddrHi(baseAddr);

    // The view slice_start is overloaded to specify the base offset.
    pCbColorView->SLICE_START = baseOffset;
    pCbColorView->SLICE_MAX   = 0;
    pCbColorView->MIP_LEVEL   = 0;

    // According to the other UMDs, this is the absolute max mip level. For one mip level, the MAX_MIP is mip #0.
    pRegs->cbColorAttrib2.bits.MAX_MIP = 0;

    // From testing this is not the padded mip height/width, but the pixel height/width specified by the client.
    pRegs->cbColorAttrib2.bits.MIP0_HEIGHT = 0;
    pRegs->cbColorAttrib2.bits.MIP0_WIDTH  = (createInfo.bufferInfo.extent - 1);
}

// =====================================================================================================================
template <typename FmtInfoType>
regCB_COLOR0_INFO ColorTargetView::InitCbColorInfo(
    const Device&      device,
    const FmtInfoType* pFmtInfo
    ) const
{
    const Pal::Device*const pParentDevice = device.Parent();

    regCB_COLOR0_INFO cbColorInfo = { };
    if (IsGfx9(*pParentDevice) || IsGfx10(*pParentDevice))
    {
        cbColorInfo.gfx09_10.ENDIAN  = ENDIAN_NONE;
        cbColorInfo.gfx09_10.FORMAT  = Formats::Gfx9::HwColorFmt(pFmtInfo, m_swizzledFormat.format);
    }
#if PAL_BUILD_GFX11
    else
    {
        cbColorInfo.gfx11.FORMAT     = Formats::Gfx9::HwColorFmt(pFmtInfo, m_swizzledFormat.format);
    }
#endif

    cbColorInfo.bits.NUMBER_TYPE = Formats::Gfx9::ColorSurfNum(pFmtInfo, m_swizzledFormat.format);
    cbColorInfo.bits.COMP_SWAP   = Formats::Gfx9::ColorCompSwap(m_swizzledFormat);

    // Set bypass blending for any format that is not blendable. Blend clamp must be cleared if blend_bypass is set.
    // Otherwise, it must be set iff any component is SNORM, UNORM, or SRGB.
    const bool blendBypass  = (pParentDevice->SupportsBlend(m_swizzledFormat.format, ImageTiling::Optimal) == false);
    const bool isNormOrSrgb = Formats::IsNormalized(m_swizzledFormat.format) ||
                              Formats::IsSrgb(m_swizzledFormat.format);
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
template <typename RegistersType, typename CbColorViewType>
void ColorTargetView::InitCommonImageView(
    const Device&                     device,
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalInfo,
    const Extent3d&                   baseExtent,
    RegistersType*                    pRegs,
    CbColorViewType*                  pCbColorView
    ) const
{
    const Pal::Device&      palDevice       = *(device.Parent());
    const ImageCreateInfo&  imageCreateInfo = m_pImage->Parent()->GetImageCreateInfo();
    const ImageType         imageType       = m_pImage->GetOverrideImageType();
    const Gfx9PalSettings&  settings        = device.Settings();

    // According to the other UMDs, this is the absolute max mip level. For one mip level, the MAX_MIP is mip #0.
    pRegs->cbColorAttrib2.bits.MAX_MIP = (imageCreateInfo.mipLevels - 1);

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
        regCB_COLOR0_DCC_CONTROL dccControl = m_pImage->GetDcc(m_subresource.plane)->GetControlReg();
        const SubResourceInfo*const pSubResInfo = m_pImage->Parent()->SubresourceInfo(m_subresource);
        if (IsGfx091xPlus(palDevice)      &&
#if PAL_BUILD_GFX11
            (IsGfx11(palDevice) == false) &&
#endif
            (internalInfo.flags.fastClearElim || pSubResInfo->flags.supportMetaDataTexFetch))
        {
            // Without this, the CB will not expand the compress-to-register (0x20) keys.
            dccControl.gfx09_1xPlus.DISABLE_CONSTANT_ENCODE_REG = 1;
        }

        pRegs->cbColorDccControl.u32All = dccControl.u32All;

        if (IsGfx9(palDevice) || IsGfx10(palDevice))
        {
            pRegs->cbColorInfo.gfx09_10.DCC_ENABLE = 1;
        }
    }

    if (m_flags.hasCmaskFmask != 0)
    {
        // Check if we can keep fmask in a compressed state and avoid corresponding fmask decompression
        const bool fMaskTexFetchAllowed = m_pImage->IsComprFmaskShaderReadable(m_subresource);

        // Setup CB_COLOR*_INFO register fields which depend on CMask or fMask state:
        pRegs->cbColorInfo.gfx09_10.COMPRESSION               = 1;
        pRegs->cbColorInfo.gfx09_10.FMASK_COMPRESSION_DISABLE = settings.fmaskCompressDisable;

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
            pRegs->cbColorInfo.gfx09_10.FMASK_COMPRESS_1FRAG_ONLY = 1;
        }
    }

    // From testing this is not the padded mip height/width, but the pixel height/width specified by the client.
    pRegs->cbColorAttrib2.bits.MIP0_HEIGHT = (baseExtent.height - 1);
    pRegs->cbColorAttrib2.bits.MIP0_WIDTH  = (baseExtent.width - 1);
}

// =====================================================================================================================
// Updates color-target view registers with the virtual addresses of the image and the image's various metadata
// addresses.  This can never be called on buffer views; the buffer view address will be computed elsewhere.
template <typename RegistersType>
void ColorTargetView::UpdateImageVa(
    RegistersType* pRegs
    ) const
{
    const Pal::Device&  palDevice = *(m_pImage->Parent()->GetDevice());

    // The "GetSubresource256BAddrSwizzled" function will crash if no memory has been bound to
    // the associated image yet, so don't do anything if it's not safe
    if (m_pImage->Parent()->GetBoundGpuMemory().IsBound())
    {
        if (m_flags.useSubresBaseAddr != 0)
        {
            // Get the base address of each array slice.
            const gpusize subresBaseAddr = m_pImage->Parent()->GetSubresourceBaseAddr(m_subresource);

            const auto*   pTileInfo   = AddrMgr2::GetTileInfo(m_pImage->Parent(), m_subresource);
            const gpusize pipeBankXor = pTileInfo->pipeBankXor;
            const gpusize addrWithXor = subresBaseAddr | (pipeBankXor << 8);

            pRegs->cbColorBase.bits.BASE_256B    = Get256BAddrLo(addrWithXor);
            pRegs->cbColorBaseExt.bits.BASE_256B = Get256BAddrHi(addrWithXor);

            // On GFX9, only DCC can be used for fast clears.  The load-meta-data packet updates the cb color regs to
            // indicate what the clear color is.  (See Gfx9FastColorClearMetaData in gfx9MaskRam.h).
            if (m_flags.hasDcc)
            {
                PAL_ASSERT(IsGfx10Plus(palDevice));

                if (m_pImage->HasFastClearMetaData(m_subresource.plane))
                {
#if PAL_BUILD_GFX11
                    PAL_ASSERT(IsGfx11(palDevice) == false);
#endif

                    // Invariant: On Gfx10 (and gfx9), if we have DCC we also have fast clear metadata.
                    pRegs->fastClearMetadataGpuVa = m_pImage->FastClearMetaDataAddr(m_subresource);
                    PAL_ASSERT((pRegs->fastClearMetadataGpuVa & 0x3) == 0);
                }

                // We want the DCC address of the exact mip level and slice that we're looking at.
                const gpusize dcc256BAddrSwizzled       = m_pImage->GetDcc256BAddrSwizzled(m_subresource);
                pRegs->cbColorDccBase.bits.BASE_256B    = LowPart(dcc256BAddrSwizzled);
                pRegs->cbColorDccBaseExt.bits.BASE_256B = HighPart(dcc256BAddrSwizzled);
            }
        }
        else
        {
            // The GetSubresource256BAddrSwizzled* functions only care about the plane.
            const gpusize subresource256BAddr    = m_pImage->GetSubresource256BAddr(m_subresource);
            pRegs->cbColorBase.bits.BASE_256B    = LowPart(subresource256BAddr);
            pRegs->cbColorBaseExt.bits.BASE_256B = HighPart(subresource256BAddr);

            // On GFX9, only DCC can be used for fast clears.  The load-meta-data packet updates the cb color regs to
            // indicate what the clear color is.  (See Gfx9FastColorClearMetaData in gfx9MaskRam.h).
            if (m_flags.hasDcc)
            {
                if (m_pImage->HasFastClearMetaData(m_subresource.plane))
                {
#if PAL_BUILD_GFX11
                    PAL_ASSERT(IsGfx11(palDevice) == false);
#endif

                    // Invariant: On Gfx10 (and gfx9), if we have DCC we also have fast clear metadata.
                    pRegs->fastClearMetadataGpuVa = m_pImage->FastClearMetaDataAddr(m_subresource);
                    PAL_ASSERT((pRegs->fastClearMetadataGpuVa & 0x3) == 0);
                }

                // The m_subresource variable includes the mip level and slice that we're viewing.  However, because
                // the CB registers are programmed with that info already, we want the address of mip 0 / slice 0 so
                // the HW can find the proper subresource on its own.
                const SubresId  baseSubResId = { m_subresource.plane, 0, 0 };
                const gpusize dcc256BAddrSwizzled       = m_pImage->GetDcc256BAddrSwizzled(baseSubResId);
                pRegs->cbColorDccBase.bits.BASE_256B    = LowPart(dcc256BAddrSwizzled);
                pRegs->cbColorDccBaseExt.bits.BASE_256B = HighPart(dcc256BAddrSwizzled);
            }
        }
    }
}

// =====================================================================================================================
// Writes the PM4 commands required to bind to depth/stencil slot (common to both Gfx9 and Gfx10).  Returns the next
// unused DWORD in pCmdSpace.
template <typename RegistersType>
uint32* ColorTargetView::WriteCommandsCommon(
    uint32         slot,
    ImageLayout    imageLayout,
    CmdStream*     pCmdStream,
    uint32*        pCmdSpace,
    RegistersType* pRegs
    ) const
{
    const uint32 slotOffset = (slot * CbRegsPerSlot);

    if (m_flags.isBufferView == 0)
    {
        if ((m_flags.viewVaLocked == 0) && m_pImage->Parent()->GetBoundGpuMemory().IsBound())
        {
            UpdateImageVa(pRegs);
        }

        if (ImageLayoutToColorCompressionState(m_layoutToState, imageLayout) == ColorCompressed)
        {
            if (pRegs->fastClearMetadataGpuVa != 0)
            {
                // Load the context registers which store the fast-clear color from GPU memory.
                constexpr uint32 RegisterCount = (Gfx09_10::mmCB_COLOR0_CLEAR_WORD1 -
                                                  Gfx09_10::mmCB_COLOR0_CLEAR_WORD0 + 1);

                pCmdSpace = pCmdStream->WriteLoadSeqContextRegs((Gfx09_10::mmCB_COLOR0_CLEAR_WORD0 + slotOffset),
                                                                RegisterCount,
                                                                pRegs->fastClearMetadataGpuVa,
                                                                pCmdSpace);
            }
        }
        else
        {
            // Value for CB_COLOR_DCC_CONTROL when compressed rendering is disabled.
            constexpr uint32 CbColorDccControlDecompressed = 0;

            const Pal::Device&  parentDev = *(m_pImage->Parent()->GetDevice());

            pRegs->cbColorDccControl.u32All = CbColorDccControlDecompressed;

            if (IsGfx9(parentDev) || IsGfx10(parentDev))
            {
                // Mask of CB_COLOR_INFO bits to clear when compressed rendering is disabled.
                constexpr uint32 CbColorInfoDecompressedMask =
                                        (Gfx09_10::CB_COLOR0_INFO__DCC_ENABLE_MASK                |
                                         Gfx09_10::CB_COLOR0_INFO__COMPRESSION_MASK               |
                                         Gfx09_10::CB_COLOR0_INFO__FMASK_COMPRESSION_DISABLE_MASK |
                                         Gfx09_10::CB_COLOR0_INFO__FMASK_COMPRESS_1FRAG_ONLY_MASK);

                // For decompressed rendering to an Image, we need to override the values for CB_COLOR_CONTROL and for
                // CB_COLOR_DCC_CONTROL.
                pRegs->cbColorInfo.u32All      &= ~CbColorInfoDecompressedMask;
            }
#if PAL_BUILD_GFX11
            else
            {
                // GFX11 doesn't have fmask or a "compression" field; DCC_ENABLE has moved to CB_COLOR_FDCC_CONTROL.
            }
#endif
        }
    } // if isBufferView == 0

    return pCmdSpace;
}

// =====================================================================================================================
// Calculates what the extents should be for this color target view.
// baseExtent, extent and modifiedYuvExtent are outputs.
void ColorTargetView::SetupExtents(
    const SubresId                   baseSubRes,
    const ColorTargetViewCreateInfo& createInfo,
    Extent3d*                        pBaseExtent,
    Extent3d*                        pExtent,
    bool*                            pModifiedYuvExtent
    ) const
{
    const auto* const            pImage          = m_pImage->Parent();
    const auto*                  pPalDevice      = pImage->GetDevice();
    const SubResourceInfo* const pBaseSubResInfo = pImage->SubresourceInfo(baseSubRes);
    const SubResourceInfo* const pSubResInfo     = pImage->SubresourceInfo(m_subresource);
    const ImageCreateInfo&       imageCreateInfo = pImage->GetImageCreateInfo();
    const bool                   imgIsBc         = Formats::IsBlockCompressed(imageCreateInfo.swizzledFormat.format);

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
    if (imgIsBc || (pSubResInfo->bitsPerTexel != Formats::BitsPerPixel(m_swizzledFormat.format)))
    {
        if (IsGfx9(*pPalDevice))
        {
            *pBaseExtent = pBaseSubResInfo->extentElements;
        }
        else
        {
            const uint32  firstMipLevel = this->m_subresource.mipLevel;

            pBaseExtent->width  = Clamp((pSubResInfo->extentElements.width  << firstMipLevel),
                pBaseSubResInfo->extentElements.width,
                pBaseSubResInfo->actualExtentElements.width);
            pBaseExtent->height = Clamp((pSubResInfo->extentElements.height << firstMipLevel),
                pBaseSubResInfo->extentElements.height,
                pBaseSubResInfo->actualExtentElements.height);
        }
        *pExtent = pSubResInfo->extentElements;
    }

    if (Formats::IsYuvPacked(pSubResInfo->format.format)         &&
        (Formats::IsYuvPacked(m_swizzledFormat.format) == false) &&
        ((pSubResInfo->bitsPerTexel << 1) == Formats::BitsPerPixel(m_swizzledFormat.format)))
    {
        // Changing how we interpret the bits-per-pixel of the subresource wreaks havoc with any tile swizzle
        // pattern used. This will only work for linear-tiled Images.
        PAL_ASSERT(m_pImage->IsSubResourceLinear(baseSubRes));

        if (IsGfx9(*pPalDevice))
        {
            // For GFX9, EPITCH field needs to be programmed to the exact pitch instead of width.
            *pBaseExtent = pBaseSubResInfo->actualExtentTexels;

            pBaseExtent->width >>= 1;
            pExtent->width     >>= 1;
        }
        else
        {
            // The width may be odd..., so the assertion below needs to round it up to even.
            const uint32 evenExtentWidthInTexels = Util::RoundUpToMultiple(pBaseSubResInfo->extentTexels.width, 2u);
            // Assert that the extentElements must have been adjusted, since use32bppFor422Fmt is 1 for AddrLib.
            PAL_ASSERT((pBaseSubResInfo->extentElements.width << 1) == evenExtentWidthInTexels);
            // Nothing is needed, just set modifiedYuvExtent=true to skip copy from extentElements again below.
        }
        *pModifiedYuvExtent = true;
    }
    else if ((m_flags.useSubresBaseAddr == 0)                            &&
             Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format) &&
             ((createInfo.imageInfo.arraySize > 1) || (createInfo.imageInfo.baseSubRes.arraySlice != 0)))
    {
        *pBaseExtent = pBaseSubResInfo->actualExtentTexels;
        m_pImage->PadYuvPlanarViewActualExtent(m_subresource, pBaseExtent);
        *pModifiedYuvExtent = true;
    }

    // The view should be in terms of texels except when we're operating in terms of elements. This will only happen
    // when we're copying to an "expanded" format (e.g., R32G32B32). In this case we can't do native format writes
    // so we're going to write each element independently. The trigger for this case is a mismatched bpp.
    if ((pSubResInfo->bitsPerTexel != Formats::BitsPerPixel(m_swizzledFormat.format)) &&
        (*pModifiedYuvExtent == false))
    {
        *pBaseExtent = pBaseSubResInfo->extentElements;
        *pExtent     = pSubResInfo->extentElements;
    }
}

// =====================================================================================================================
bool ColorTargetView::Equals(
    const ColorTargetView* pOther) const
{
    return ((pOther != nullptr) && (m_uniqueId == pOther->m_uniqueId));
}

// =====================================================================================================================
Gfx9ColorTargetView::Gfx9ColorTargetView(
    const Device*                     pDevice,
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalInfo,
    uint32                            viewId)
    :
    ColorTargetView(pDevice, createInfo, internalInfo, viewId)
{
    memset(&m_regs, 0, sizeof(m_regs));
    InitRegisters(*pDevice, createInfo, internalInfo);

    if (m_flags.viewVaLocked && (m_flags.isBufferView == 0))
    {
        UpdateImageVa(&m_regs);
        if (m_pImage->Parent()->GetBoundGpuMemory().IsBound() &&
            m_flags.hasCmaskFmask)
        {
            const gpusize cmask256BAddrSwizzled       = m_pImage->GetCmask256BAddrSwizzled();
            const gpusize fmask256BAddrSwizzled       = m_pImage->GetFmask256BAddrSwizzled();

            m_regs.cbColorCmask.bits.BASE_256B        = LowPart(cmask256BAddrSwizzled);
            m_regs.cbColorFmask.bits.BASE_256B        = LowPart(fmask256BAddrSwizzled);
            m_regs.cbColorCmaskBaseExt.bits.BASE_256B = HighPart(cmask256BAddrSwizzled);
            m_regs.cbColorFmaskBaseExt.bits.BASE_256B = HighPart(fmask256BAddrSwizzled);
        }
    }
}

// =====================================================================================================================
// Finalizes the PM4 packet image by setting up the register values used to write this View object to hardware.
void Gfx9ColorTargetView::InitRegisters(
    const Device&                     device,
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalInfo)
{
    const MergedFmtInfo*const pFmtInfo =
        MergedChannelFmtInfoTbl(GfxIpLevel::GfxIp9, &device.GetPlatform()->PlatformSettings());

    m_regs.cbColorInfo = InitCbColorInfo(device, pFmtInfo);

    // Most register values are simple to compute but vary based on whether or not this is a buffer view. Let's set
    // them all up-front before we get on to the harder register values.
    if (m_flags.isBufferView)
    {
        PAL_ASSERT(createInfo.bufferInfo.offset == 0);

        InitCommonBufferView(device, createInfo, &m_regs, &m_regs.cbColorView.gfx09);

        m_extent.width  = createInfo.bufferInfo.extent;
        m_extent.height = 1;

        m_regs.cbColorAttrib.most.FORCE_DST_ALPHA_1 = Formats::HasUnusedAlpha(m_swizzledFormat) ? 1 : 0;
        m_regs.cbColorAttrib.most.NUM_SAMPLES       = 0;
        m_regs.cbColorAttrib.most.NUM_FRAGMENTS     = 0;
        m_regs.cbColorAttrib.gfx09.MIP0_DEPTH       = 0; // what is this?
        m_regs.cbColorAttrib.gfx09.COLOR_SW_MODE    = SW_LINEAR;
        m_regs.cbColorAttrib.gfx09.RESOURCE_TYPE    = static_cast<uint32>(ImageType::Tex1d); // no HW enums
        m_regs.cbColorAttrib.gfx09.RB_ALIGNED       = 0;
        m_regs.cbColorAttrib.gfx09.PIPE_ALIGNED     = 0;
        m_regs.cbColorAttrib.gfx09.FMASK_SW_MODE    = SW_LINEAR; // ignored as there is no fmask
        m_regs.cbColorAttrib.gfx09.META_LINEAR      = 0;         // linear meta surfaces not supported on gfx9
        m_regs.cbMrtEpitch.bits.EPITCH              = (createInfo.bufferInfo.extent - 1);
    }
    else
    {
        const auto*const            pImage          = m_pImage->Parent();
        const auto*                 pPalDevice      = pImage->GetDevice();
        const auto*                 pAddrMgr        = static_cast<const AddrMgr2::AddrMgr2*>(pPalDevice->GetAddrMgr());
        const SubresId              baseSubRes      = { m_subresource.plane, 0, 0 };
        const Gfx9MaskRam*          pMaskRam        = m_pImage->GetColorMaskRam(m_subresource.plane);
        const SubResourceInfo*const pBaseSubResInfo = pImage->SubresourceInfo(baseSubRes);
        const SubResourceInfo*const pSubResInfo     = pImage->SubresourceInfo(m_subresource);
        const auto&                 surfSetting     = m_pImage->GetAddrSettings(pSubResInfo);
        const auto*const            pAddrOutput     = m_pImage->GetAddrOutput(pBaseSubResInfo);
        const ImageCreateInfo&      imageCreateInfo = pImage->GetImageCreateInfo();
        const ImageType             imageType       = m_pImage->GetOverrideImageType();

        // NOTE: The color base address will be determined later, we don't need to do anything here.

        Extent3d baseExtent        = pBaseSubResInfo->extentTexels;
        Extent3d extent            = pSubResInfo->extentTexels;
        bool     modifiedYuvExtent = false;

        // baseExtent, extent and modifiedYuvExtent are outputs.
        SetupExtents(baseSubRes,
                     createInfo,
                     &baseExtent,
                     &extent,
                     &modifiedYuvExtent);

        InitCommonImageView(device,
                            createInfo,
                            internalInfo,
                            baseExtent,
                            &m_regs,
                            &m_regs.cbColorView.gfx09);

        m_extent.width  = extent.width;
        m_extent.height = extent.height;

        m_regs.cbColorAttrib.most.NUM_SAMPLES       = Log2(imageCreateInfo.samples);
        m_regs.cbColorAttrib.most.NUM_FRAGMENTS     = Log2(imageCreateInfo.fragments);
        m_regs.cbColorAttrib.most.FORCE_DST_ALPHA_1 = Formats::HasUnusedAlpha(m_swizzledFormat);
        m_regs.cbColorAttrib.gfx09.MIP0_DEPTH    =
            ((imageType == ImageType::Tex3d) ? imageCreateInfo.extent.depth : imageCreateInfo.arraySize) - 1;
        m_regs.cbColorAttrib.gfx09.COLOR_SW_MODE = pAddrMgr->GetHwSwizzleMode(surfSetting.swizzleMode);
        m_regs.cbColorAttrib.gfx09.RESOURCE_TYPE = static_cast<uint32>(imageType); // no HW enums
        m_regs.cbColorAttrib.gfx09.RB_ALIGNED    = device.IsRbAligned();
        m_regs.cbColorAttrib.gfx09.PIPE_ALIGNED  = ((pMaskRam != nullptr) ? pMaskRam->PipeAligned() : 0);
        m_regs.cbColorAttrib.gfx09.META_LINEAR   = 0;

        const AddrSwizzleMode fMaskSwizzleMode =
            (m_pImage->HasFmaskData() ? m_pImage->GetFmask()->GetSwizzleMode() : ADDR_SW_LINEAR /* ignored */);
        m_regs.cbColorAttrib.gfx09.FMASK_SW_MODE = pAddrMgr->GetHwSwizzleMode(fMaskSwizzleMode);

        if (modifiedYuvExtent)
        {
            m_regs.cbMrtEpitch.bits.EPITCH = ((pAddrOutput->epitchIsHeight ? baseExtent.height : baseExtent.width) - 1);
        }
        else
        {
            m_regs.cbMrtEpitch.bits.EPITCH = AddrMgr2::CalcEpitch(pAddrOutput);
        }
    }
}

// =====================================================================================================================
// Writes the PM4 commands required to bind to a certain slot. Returns the next unused DWORD in pCmdSpace.
uint32* Gfx9ColorTargetView::WriteCommands(
    uint32             slot,        // Bind slot
    ImageLayout        imageLayout, // Current image layout
    CmdStream*         pCmdStream,
    uint32*            pCmdSpace,
    regCB_COLOR0_INFO* pCbColorInfo // Device's copy of CB_COLORn_INFO to update
    ) const
{
    Gfx9ColorTargetViewRegs regs = m_regs;
    pCmdSpace = WriteCommandsCommon(slot, imageLayout, pCmdStream, pCmdSpace, &regs);
    if ((m_flags.viewVaLocked == 0)                       &&
        m_pImage->Parent()->GetBoundGpuMemory().IsBound() &&
        m_flags.hasCmaskFmask)
    {
        const gpusize cmask256BAddrSwizzled     = m_pImage->GetCmask256BAddrSwizzled();
        const gpusize fmask256BAddrSwizzled     = m_pImage->GetFmask256BAddrSwizzled();

        regs.cbColorCmask.bits.BASE_256B        = LowPart(cmask256BAddrSwizzled);
        regs.cbColorFmask.bits.BASE_256B        = LowPart(fmask256BAddrSwizzled);
        regs.cbColorCmaskBaseExt.bits.BASE_256B = HighPart(cmask256BAddrSwizzled);
        regs.cbColorFmaskBaseExt.bits.BASE_256B = HighPart(fmask256BAddrSwizzled);
    }

    const uint32 slotOffset = (slot * CbRegsPerSlot);

    pCmdSpace = pCmdStream->WriteSetSeqContextRegs((mmCB_COLOR0_BASE + slotOffset),
                                                   (mmCB_COLOR0_VIEW + slotOffset),
                                                   &regs.cbColorBase,
                                                   pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs((mmCB_COLOR0_ATTRIB                + slotOffset),
                                                   (Gfx09::mmCB_COLOR0_FMASK_BASE_EXT + slotOffset),
                                                   &regs.cbColorAttrib,
                                                   pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs((mmCB_COLOR0_DCC_BASE            + slotOffset),
                                                   (Gfx09::mmCB_COLOR0_DCC_BASE_EXT + slotOffset),
                                                   &regs.cbColorDccBase,
                                                   pCmdSpace);

    // Registers above this point are grouped by slot index (e.g., all of slot0 then all of slot1, etc.).  Registers
    // below this point are grouped by register (e.g., all of CB_MRT*_EPITCH, and so on).

    pCmdSpace = pCmdStream->WriteSetOneContextReg((Gfx09::mmCB_MRT0_EPITCH + slot),
                                                  regs.cbMrtEpitch.u32All,
                                                  pCmdSpace);

    // Update just the portion owned by RTV.
    BitfieldUpdateSubfield(&(pCbColorInfo->u32All), regs.cbColorInfo.u32All, CbColorInfoMask);

#if PAL_DEVELOPER_BUILD
    if (m_pImage != nullptr)
    {
        Developer::SurfRegDataInfo data = {};
        data.type    = Developer::SurfRegDataType::RenderTargetView;
        data.regData = regs.cbColorBase.u32All;
        m_pImage->Parent()->GetDevice()->DeveloperCb(Developer::CallbackType::SurfRegData, &data);
    }
#endif

    return pCmdSpace;
}

// =====================================================================================================================
Gfx10ColorTargetView::Gfx10ColorTargetView(
    const Device*                     pDevice,
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalInfo,
    uint32                            viewId)
    :
    ColorTargetView(pDevice, createInfo, internalInfo, viewId)
{
    memset(&m_regs, 0, sizeof(m_regs));
    memset(&m_uavExportSrd, 0, sizeof(m_uavExportSrd));
    InitRegisters(*pDevice, createInfo, internalInfo);

    m_flags.bypassMall = createInfo.flags.bypassMall;

    if (m_flags.isBufferView != 0)
    {
        const auto& bufferInfo = createInfo.bufferInfo;
        m_flags.colorBigPage = IsBufferBigPageCompatible(*static_cast<const GpuMemory*>(bufferInfo.pGpuMemory),
                                                         bufferInfo.offset,
                                                         bufferInfo.extent,
                                                         Gfx10AllowBigPageBuffers);
    }
    else if (IsVaLocked())
    {
        m_flags.colorBigPage = IsImageBigPageCompatible(*m_pImage, Gfx10AllowBigPageRenderTarget);
        m_flags.fmaskBigPage = IsFmaskBigPageCompatible(*m_pImage, Gfx10AllowBigPageRenderTarget);

        UpdateImageVa(&m_regs);
        if (m_pImage->Parent()->GetBoundGpuMemory().IsBound() &&
            m_flags.hasCmaskFmask)
        {
            const gpusize cmask256BAddrSwizzled       = m_pImage->GetCmask256BAddrSwizzled();
            const gpusize fmask256BAddrSwizzled       = m_pImage->GetFmask256BAddrSwizzled();

            m_regs.cbColorCmask.bits.BASE_256B        = LowPart(cmask256BAddrSwizzled);
            m_regs.cbColorFmask.bits.BASE_256B        = LowPart(fmask256BAddrSwizzled);
            m_regs.cbColorCmaskBaseExt.bits.BASE_256B = HighPart(cmask256BAddrSwizzled);
            m_regs.cbColorFmaskBaseExt.bits.BASE_256B = HighPart(fmask256BAddrSwizzled);
        }
        if (m_pImage->Parent()->GetImageCreateInfo().imageType == ImageType::Tex2d)
        {
            UpdateImageSrd(*pDevice, &m_uavExportSrd);
        }
    }
}

// =====================================================================================================================
// Finalizes the PM4 packet image by setting up the register values used to write this View object to hardware.
void Gfx10ColorTargetView::InitRegisters(
    const Device&                     device,
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalInfo)
{
    const Pal::Device&       palDevice = *device.Parent();
    const GpuChipProperties& chipProps = palDevice.ChipProperties();
    const Gfx9PalSettings&   settings  = device.Settings();

    const MergedFlatFmtInfo*const pFmtInfoTbl =
        MergedChannelFlatFmtInfoTbl(chipProps.gfxLevel, &device.GetPlatform()->PlatformSettings());

    m_regs.cbColorInfo = InitCbColorInfo(device, pFmtInfoTbl);

    // Most register values are simple to compute but vary based on whether or not this is a buffer view. Let's set
    // them all up-front before we get on to the harder register values.
    if (m_flags.isBufferView)
    {
        InitCommonBufferView(device, createInfo, &m_regs, &m_regs.cbColorView.gfx10Plus);

        m_extent.width  = createInfo.bufferInfo.extent;
        m_extent.height = 1;

        // Setup GFX10-specific registers here
        m_regs.cbColorAttrib3.bits.MIP0_DEPTH    = 0;
        m_regs.cbColorAttrib3.bits.COLOR_SW_MODE = SW_LINEAR;
        m_regs.cbColorAttrib3.bits.RESOURCE_TYPE = static_cast<uint32>(ImageType::Tex1d); // no HW enums

        m_regs.cbColorAttrib3.gfx10.FMASK_SW_MODE = SW_LINEAR; // ignored as there is no fmask
        m_regs.cbColorAttrib3.bits.META_LINEAR    = 1;         // no meta-data, but should be set for linear surfaces

        // Specifying a non-zero buffer offset only works with linear-general surfaces
        m_regs.cbColorInfo.gfx10Plus.LINEAR_GENERAL  = 1;
        {
            m_regs.cbColorAttrib.most.FORCE_DST_ALPHA_1 = Formats::HasUnusedAlpha(m_swizzledFormat);
            m_regs.cbColorAttrib.most.NUM_SAMPLES       = 0;
            m_regs.cbColorAttrib.most.NUM_FRAGMENTS     = 0;
        }
    }
    else
    {
        const auto*const            pImage          = m_pImage->Parent();
        const auto*                 pAddrMgr        = static_cast<const AddrMgr2::AddrMgr2*>(palDevice.GetAddrMgr());
        const SubresId              baseSubRes      = { m_subresource.plane, 0, 0 };
        const Gfx9Dcc*              pDcc            = m_pImage->GetDcc(m_subresource.plane);
        const Gfx9Cmask*            pCmask          = m_pImage->GetCmask();
        const SubResourceInfo*const pBaseSubResInfo = pImage->SubresourceInfo(baseSubRes);
        const SubResourceInfo*const pSubResInfo     = pImage->SubresourceInfo(m_subresource);
        const auto*                 pAddrOutput     = m_pImage->GetAddrOutput(pSubResInfo);
        const auto&                 surfSetting     = m_pImage->GetAddrSettings(pSubResInfo);
        const auto&                 imageCreateInfo = pImage->GetImageCreateInfo();
        const ImageType             imageType       = m_pImage->GetOverrideImageType();
        const bool                  hasFmask        = m_pImage->HasFmaskData();

        // Extents are one of the things that could be changing on GFX10 with respect to certain surface formats,
        // so go with the simple approach here for now.
        Extent3d baseExtent        = pBaseSubResInfo->extentTexels;
        Extent3d extent            = pSubResInfo->extentTexels;
        bool     modifiedYuvExtent = false;

        // baseExtent, extent and modifiedYuvExtent are outputs.
        SetupExtents(baseSubRes,
                     createInfo,
                     &baseExtent,
                     &extent,
                     &modifiedYuvExtent);

        InitCommonImageView(device,
                            createInfo,
                            internalInfo,
                            baseExtent,
                            &m_regs,
                            &m_regs.cbColorView.gfx10Plus);

        m_extent.width  = extent.width;
        m_extent.height = extent.height;

        // Note that we set CB_COLORn_ATTRIB.LIMIT_COLOR_FETCH_TO_256B_MAX to 0 below for clarity, because
        // there have been some conflicting notes in reg specs.
        // Prior to gfx10.1 it was necessary to set LIMIT_COLOR_FETCH_TO_256B_MAX if in miptail, to workaround
        // possible corruptions when multiple mip levels in same 1k address space.
        // A fix for this was applied to gfx10.1 and newer asics, so the workaround is no longer needed.
        {
            m_regs.cbColorAttrib.most.NUM_SAMPLES       = Log2(imageCreateInfo.samples);
            m_regs.cbColorAttrib.most.NUM_FRAGMENTS     = Log2(imageCreateInfo.fragments);
            m_regs.cbColorAttrib.most.FORCE_DST_ALPHA_1 = Formats::HasUnusedAlpha(m_swizzledFormat);
            m_regs.cbColorAttrib.gfx10Core.LIMIT_COLOR_FETCH_TO_256B_MAX = 0;
        }

        m_regs.cbColorAttrib3.bits.MIP0_DEPTH    =
            ((imageType == ImageType::Tex3d) ? imageCreateInfo.extent.depth : imageCreateInfo.arraySize) - 1;
        m_regs.cbColorAttrib3.bits.COLOR_SW_MODE = pAddrMgr->GetHwSwizzleMode(surfSetting.swizzleMode);
        m_regs.cbColorAttrib3.bits.RESOURCE_TYPE = static_cast<uint32>(imageType); // no HW enums
        m_regs.cbColorAttrib3.bits.META_LINEAR   = m_pImage->IsSubResourceLinear(createInfo.imageInfo.baseSubRes);

        const uint32 dccPipeAligned = ((pDcc != nullptr) ? pDcc->PipeAligned() : 0);

        m_regs.cbColorAttrib3.bits.DCC_PIPE_ALIGNED   = dccPipeAligned;
        m_regs.cbColorAttrib3.gfx10.CMASK_PIPE_ALIGNED = (dccPipeAligned |
                                                         ((pCmask != nullptr) ? pCmask->PipeAligned() : 0));

        const AddrSwizzleMode fMaskSwizzleMode =
            (hasFmask ? m_pImage->GetFmask()->GetSwizzleMode() : ADDR_SW_LINEAR /* ignored */);
        m_regs.cbColorAttrib3.gfx10.FMASK_SW_MODE = pAddrMgr->GetHwSwizzleMode(fMaskSwizzleMode);

        // From the reg-spec:
        //  this bit or the CONFIG equivalent must be set if parts of FMask surface may be unmapped (such as in PRT`s).
        if (hasFmask &&
            (imageCreateInfo.flags.prt ||
             (settings.waDisableFmaskNofetchOpOnFmaskCompressionDisable &&
              m_regs.cbColorInfo.gfx09_10.FMASK_COMPRESSION_DISABLE)))
        {
            {
                m_regs.cbColorAttrib.gfx10Core.DISABLE_FMASK_NOFETCH_OPT = 1;
            }
        }
    }

    {
        m_regs.cbColorAttrib3.gfx10Core.RESOURCE_LEVEL = 1;
    }
}

// =====================================================================================================================
// Writes the PM4 commands required to bind to a certain slot.  Returns the next unused DWORD in pCmdSpace.
uint32* Gfx10ColorTargetView::WriteCommands(
    uint32             slot,        // Bind slot
    ImageLayout        imageLayout, // Current image layout
    CmdStream*         pCmdStream,
    uint32*            pCmdSpace,
    regCB_COLOR0_INFO* pCbColorInfo // Device's copy of CB_COLORn_INFO to update
    ) const
{
    Gfx10ColorTargetViewRegs regs = m_regs;
    pCmdSpace = WriteCommandsCommon(slot, imageLayout, pCmdStream, pCmdSpace, &regs);
    if ((m_flags.viewVaLocked == 0)                       &&
        m_pImage->Parent()->GetBoundGpuMemory().IsBound() &&
        m_flags.hasCmaskFmask)
    {
        const gpusize cmask256BAddrSwizzled     = m_pImage->GetCmask256BAddrSwizzled();
        const gpusize fmask256BAddrSwizzled     = m_pImage->GetFmask256BAddrSwizzled();

        regs.cbColorCmask.bits.BASE_256B        = LowPart(cmask256BAddrSwizzled);
        regs.cbColorFmask.bits.BASE_256B        = LowPart(fmask256BAddrSwizzled);
        regs.cbColorCmaskBaseExt.bits.BASE_256B = HighPart(cmask256BAddrSwizzled);
        regs.cbColorFmaskBaseExt.bits.BASE_256B = HighPart(fmask256BAddrSwizzled);
    }

    const uint32 slotOffset = (slot * CbRegsPerSlot);

    pCmdSpace = pCmdStream->WriteSetSeqContextRegs((mmCB_COLOR0_BASE + slotOffset),
                                                   (mmCB_COLOR0_VIEW + slotOffset),
                                                   &regs.cbColorBase,
                                                   pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs((mmCB_COLOR0_ATTRIB + slotOffset),
                                                   (Gfx09_10::mmCB_COLOR0_FMASK  + slotOffset),
                                                   &regs.cbColorAttrib,
                                                   pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg((mmCB_COLOR0_DCC_BASE + slotOffset),
                                                  regs.cbColorDccBase.u32All,
                                                  pCmdSpace);

    // Registers above this point are grouped by slot index (e.g., all of slot0 then all of slot1, etc.).  Registers
    // below this point are grouped by register (e.g., all of CB_COLOR*_ATTRIB2, and so on).

    pCmdSpace = pCmdStream->WriteSetOneContextReg((Gfx10Plus::mmCB_COLOR0_BASE_EXT + slot),
                                                  regs.cbColorBaseExt.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg((Gfx10Plus::mmCB_COLOR0_DCC_BASE_EXT + slot),
                                                  regs.cbColorDccBaseExt.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg((Gfx10::mmCB_COLOR0_FMASK_BASE_EXT + slot),
                                                  regs.cbColorFmaskBaseExt.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg((Gfx10::mmCB_COLOR0_CMASK_BASE_EXT + slot),
                                                  regs.cbColorCmaskBaseExt.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg((Gfx10Plus::mmCB_COLOR0_ATTRIB2 + slot),
                                                  regs.cbColorAttrib2.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg((Gfx10Plus::mmCB_COLOR0_ATTRIB3 + slot),
                                                  regs.cbColorAttrib3.u32All,
                                                  pCmdSpace);

    // Update just the portion owned by RTV.
    BitfieldUpdateSubfield(&(pCbColorInfo->u32All), regs.cbColorInfo.u32All, CbColorInfoMask);

#if PAL_DEVELOPER_BUILD
    if (m_pImage != nullptr)
    {
        Developer::SurfRegDataInfo data = {};
        data.type    = Developer::SurfRegDataType::RenderTargetView;
        data.regData = regs.cbColorBase.u32All;
        m_pImage->Parent()->GetDevice()->DeveloperCb(Developer::CallbackType::SurfRegData, &data);
    }
#endif

    return pCmdSpace;
}

// =====================================================================================================================
// Writes an image SRD (for UAV exports) to the given memory location
void Gfx10ColorTargetView::GetImageSrd(
    const Device& device,
    void*         pOut
    ) const
{
    if (m_flags.viewVaLocked == 0)
    {
        UpdateImageSrd(device, pOut);
    }
    else
    {
        memcpy(pOut, &m_uavExportSrd, sizeof(m_uavExportSrd));
    }
}

// =====================================================================================================================
// Updates the cached image SRD (for UAV exports). This may need to get called at draw-time if viewVaLocked is false
void Gfx10ColorTargetView::UpdateImageSrd(
    const Device& device,
    void*         pOut
    ) const
{
    PAL_ASSERT(m_flags.isBufferView == 0);
    PAL_ASSERT(m_pImage->Parent()->GetImageCreateInfo().imageType == ImageType::Tex2d);

    ImageViewInfo viewInfo = {};
    viewInfo.pImage          = GetImage()->Parent();
    viewInfo.viewType        = ImageViewType::Tex2d;
    viewInfo.possibleLayouts =
    {
        Pal::LayoutShaderWrite | Pal::LayoutColorTarget,
        Pal::LayoutUniversalEngine
    };
    viewInfo.swizzledFormat       = m_swizzledFormat;
    viewInfo.subresRange          = { m_subresource, 1, 1, m_arraySize };

    device.Parent()->CreateImageViewSrds(1, &viewInfo, pOut);
}

// =====================================================================================================================
// Reports if the color target view can support setting COLOR_BIG_PAGE in CB_RMI_GLC2_CACHE_CONTROL.
bool Gfx10ColorTargetView::IsColorBigPage() const
{
    // Buffer views and viewVaLocked image views have already computed whether they can support BIG_PAGE or not.  Other
    // cases have to check now in case the bound memory has changed.
    return ((m_flags.viewVaLocked | m_flags.isBufferView) != 0)
                ? m_flags.colorBigPage
                : IsImageBigPageCompatible(*m_pImage, Gfx10AllowBigPageRenderTarget);
}

// =====================================================================================================================
// Reports if the color target view can support setting FMASK_BIG_PAGE in CB_RMI_GLC2_CACHE_CONTROL.
bool Gfx10ColorTargetView::IsFmaskBigPage() const
{
    // viewVaLocked image views have already computed whether they can support BIG_PAGE or not.  Other cases have to
    // check now in case the bound memory has changed.
    return IsVaLocked() ? (m_flags.fmaskBigPage != 0)
                        : IsFmaskBigPageCompatible(*m_pImage, Gfx10AllowBigPageRenderTarget);
}

#if PAL_BUILD_GFX11
// =====================================================================================================================
Gfx11ColorTargetView::Gfx11ColorTargetView(
    const Device*                     pDevice,
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalInfo,
    uint32                            viewId)
    :
    ColorTargetView(pDevice, createInfo, internalInfo, viewId)
{
    memset(&m_regs, 0, sizeof(m_regs));
    memset(&m_uavExportSrd, 0, sizeof(m_uavExportSrd));
    InitRegisters(*pDevice, createInfo, internalInfo);

    m_flags.bypassMall = createInfo.flags.bypassMall;

    if (m_flags.isBufferView != 0)
    {
        const auto& bufferInfo = createInfo.bufferInfo;
        m_flags.colorBigPage = IsBufferBigPageCompatible(*static_cast<const GpuMemory*>(bufferInfo.pGpuMemory),
                                                         bufferInfo.offset,
                                                         bufferInfo.extent,
                                                         Gfx10AllowBigPageBuffers);
    }
    else if (IsVaLocked())
    {
        m_flags.colorBigPage = IsImageBigPageCompatible(*m_pImage, Gfx10AllowBigPageRenderTarget);

        UpdateImageVa(&m_regs);
        if (m_pImage->Parent()->GetImageCreateInfo().imageType == ImageType::Tex2d)
        {
            UpdateImageSrd(*pDevice, &m_uavExportSrd);
        }
    }
}

// =====================================================================================================================
// Finalizes the PM4 packet image by setting up the register values used to write this View object to hardware.
void Gfx11ColorTargetView::InitRegisters(
    const Device&                     device,
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalInfo)
{
    const Pal::Device&       palDevice       = *device.Parent();
    const GpuChipProperties& chipProps       = palDevice.ChipProperties();
    const Gfx9PalSettings&   settings        = device.Settings();
    const PalPublicSettings* pPublicSettings = palDevice.GetPublicSettings();

    const MergedFlatFmtInfo*const pFmtInfoTbl =
        MergedChannelFlatFmtInfoTbl(chipProps.gfxLevel, &device.GetPlatform()->PlatformSettings());

    m_regs.cbColorInfo = InitCbColorInfo(device, pFmtInfoTbl);

    // Most register values are simple to compute but vary based on whether or not this is a buffer view. Let's set
    // them all up-front before we get on to the harder register values.
    if (m_flags.isBufferView)
    {
        InitCommonBufferView(device, createInfo, &m_regs, &m_regs.cbColorView.gfx10Plus);

        m_extent.width  = createInfo.bufferInfo.extent;
        m_extent.height = 1;

        // Setup GFX10-specific registers here
        m_regs.cbColorAttrib3.bits.MIP0_DEPTH    = 0;
        m_regs.cbColorAttrib3.bits.COLOR_SW_MODE = SW_LINEAR;
        m_regs.cbColorAttrib3.bits.RESOURCE_TYPE = static_cast<uint32>(ImageType::Tex1d); // no HW enums

        m_regs.cbColorAttrib3.bits.META_LINEAR   = 1;         // no meta-data, but should be set for linear surfaces

        // Specifying a non-zero buffer offset only works with linear-general surfaces
        m_regs.cbColorInfo.gfx10Plus.LINEAR_GENERAL  = 1;
        m_regs.cbColorAttrib.gfx11.FORCE_DST_ALPHA_1 = Formats::HasUnusedAlpha(m_swizzledFormat);
        m_regs.cbColorAttrib.gfx11.NUM_FRAGMENTS     = 0;
    }
    else
    {
        const auto*const            pImage          = m_pImage->Parent();
        const auto*                 pAddrMgr        = static_cast<const AddrMgr2::AddrMgr2*>(palDevice.GetAddrMgr());
        const SubresId              baseSubRes      = { m_subresource.plane, 0, 0 };
        const Gfx9Dcc*              pDcc            = m_pImage->GetDcc(m_subresource.plane);
        const SubResourceInfo*const pBaseSubResInfo = pImage->SubresourceInfo(baseSubRes);
        const SubResourceInfo*const pSubResInfo     = pImage->SubresourceInfo(m_subresource);
        const auto*                 pAddrOutput     = m_pImage->GetAddrOutput(pSubResInfo);
        const auto&                 surfSetting     = m_pImage->GetAddrSettings(pSubResInfo);
        const auto&                 imageCreateInfo = pImage->GetImageCreateInfo();
        const ImageType             imageType       = m_pImage->GetOverrideImageType();

        PAL_ASSERT((m_pImage->GetCmask() == nullptr) && (m_pImage->GetFmask() == nullptr));

        // Extents are one of the things that could be changing on GFX10 with respect to certain surface formats,
        // so go with the simple approach here for now.
        Extent3d baseExtent        = pBaseSubResInfo->extentTexels;
        Extent3d extent            = pSubResInfo->extentTexels;
        bool     modifiedYuvExtent = false;

        // baseExtent, extent and modifiedYuvExtent are outputs.
        SetupExtents(baseSubRes,
                     createInfo,
                     &baseExtent,
                     &extent,
                     &modifiedYuvExtent);

        InitCommonImageView(device,
                            createInfo,
                            internalInfo,
                            baseExtent,
                            &m_regs,
                            &m_regs.cbColorView.gfx10Plus);

        if (m_flags.hasDcc != 0)
        {
            m_regs.cbColorDccControl.gfx11.FDCC_ENABLE = 1;

            // We should never see a fast-clear-elimiante request on GFX11
            PAL_ASSERT(internalInfo.flags.fastClearElim == 0);

            // If this surface can't be read by the texture pipe (and that's always a possibility given that any
            // surface can be the source of a CmdCopyXXX operation), then don't allow this surface to be compressed.
            if (pSubResInfo->flags.supportMetaDataTexFetch == 0)
            {
                m_regs.cbColorDccControl.gfx11.DCC_COMPRESS_DISABLE = 1;
            }
        }

        m_extent.width  = extent.width;
        m_extent.height = extent.height;

        m_regs.cbColorAttrib.gfx11.NUM_FRAGMENTS     = Log2(imageCreateInfo.fragments);
        m_regs.cbColorAttrib.gfx11.FORCE_DST_ALPHA_1 = Formats::HasUnusedAlpha(m_swizzledFormat);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 790
        m_regs.cbColorAttrib.gfx11.LIMIT_COLOR_FETCH_TO_256B_MAX = pPublicSettings->limitCbFetch256B;
#endif

        m_regs.cbColorAttrib3.bits.MIP0_DEPTH    =
            ((imageType == ImageType::Tex3d) ? imageCreateInfo.extent.depth : imageCreateInfo.arraySize) - 1;
        m_regs.cbColorAttrib3.bits.COLOR_SW_MODE = pAddrMgr->GetHwSwizzleMode(surfSetting.swizzleMode);
        m_regs.cbColorAttrib3.bits.RESOURCE_TYPE = static_cast<uint32>(imageType); // no HW enums
        m_regs.cbColorAttrib3.bits.META_LINEAR   = m_pImage->IsSubResourceLinear(createInfo.imageInfo.baseSubRes);

        m_regs.cbColorAttrib3.bits.DCC_PIPE_ALIGNED   = ((pDcc != nullptr) ? pDcc->PipeAligned() : 0);

    }
}

// =====================================================================================================================
// Writes the PM4 commands required to bind to a certain slot.  Returns the next unused DWORD in pCmdSpace.
uint32* Gfx11ColorTargetView::WriteCommands(
    uint32             slot,        // Bind slot
    ImageLayout        imageLayout, // Current image layout
    CmdStream*         pCmdStream,
    uint32*            pCmdSpace,
    regCB_COLOR0_INFO* pCbColorInfo // Device's copy of CB_COLORn_INFO to update
    ) const
{
    Gfx11ColorTargetViewRegs regs = m_regs;
    pCmdSpace = WriteCommandsCommon(slot, imageLayout, pCmdStream, pCmdSpace, &regs);

    static_assert(mmCB_COLOR0_BASE + 15 == mmCB_COLOR1_BASE, "CbRegsPerSlot has changed!");
    const uint32 slotOffset = (slot * CbRegsPerSlot);

    pCmdSpace = pCmdStream->WriteSetOneContextReg((mmCB_COLOR0_BASE + slotOffset),
                                                  regs.cbColorBase.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs((mmCB_COLOR0_VIEW + slotOffset),
                                                   mmCB_COLOR0_DCC_CONTROL + slotOffset,
                                                   &regs.cbColorView,
                                                   pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg((mmCB_COLOR0_DCC_BASE + slotOffset),
                                                  regs.cbColorDccBase.u32All,
                                                  pCmdSpace);

    // Registers above this point are grouped by slot index (e.g., all of slot0 then all of slot1, etc.).  Registers
    // below this point are grouped by register (e.g., all of CB_COLOR*_ATTRIB2, and so on).

    pCmdSpace = pCmdStream->WriteSetOneContextReg((Gfx10Plus::mmCB_COLOR0_ATTRIB2 + slot),
                                                  regs.cbColorAttrib2.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg((Gfx10Plus::mmCB_COLOR0_ATTRIB3 + slot),
                                                  regs.cbColorAttrib3.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg((Gfx10Plus::mmCB_COLOR0_BASE_EXT + slot),
                                                  regs.cbColorBaseExt.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg((Gfx10Plus::mmCB_COLOR0_DCC_BASE_EXT + slot),
                                                  regs.cbColorDccBaseExt.u32All,
                                                  pCmdSpace);

    // Update just the portion owned by RTV.
    BitfieldUpdateSubfield(&(pCbColorInfo->u32All), regs.cbColorInfo.u32All, CbColorInfoMask);

#if PAL_DEVELOPER_BUILD
    if (m_pImage != nullptr)
    {
        Developer::SurfRegDataInfo data = {};
        data.type    = Developer::SurfRegDataType::RenderTargetView;
        data.regData = regs.cbColorBase.u32All;
        m_pImage->Parent()->GetDevice()->DeveloperCb(Developer::CallbackType::SurfRegData, &data);
    }
#endif

    return pCmdSpace;
}

// =====================================================================================================================
// Writes an image SRD (for UAV exports) to the given memory location
void Gfx11ColorTargetView::GetImageSrd(
    const Device& device,
    void*         pOut
    ) const
{
    if (m_flags.viewVaLocked == 0)
    {
        UpdateImageSrd(device, pOut);
    }
    else
    {
        memcpy(pOut, &m_uavExportSrd, sizeof(m_uavExportSrd));
    }
}

// =====================================================================================================================
// Updates the cached image SRD (for UAV exports). This may need to get called at draw-time if viewVaLocked is false
void Gfx11ColorTargetView::UpdateImageSrd(
    const Device& device,
    void*         pOut
    ) const
{
    PAL_ASSERT(m_flags.isBufferView == 0);
    PAL_ASSERT(m_pImage->Parent()->GetImageCreateInfo().imageType == ImageType::Tex2d);

    ImageViewInfo viewInfo = {};
    viewInfo.pImage          = GetImage()->Parent();
    viewInfo.viewType        = ImageViewType::Tex2d;
    viewInfo.possibleLayouts =
    {
        Pal::LayoutShaderWrite | Pal::LayoutColorTarget,
        Pal::LayoutUniversalEngine
    };
    viewInfo.swizzledFormat       = m_swizzledFormat;
    viewInfo.subresRange          = { m_subresource, 1, 1, m_arraySize };

    device.Parent()->CreateImageViewSrds(1, &viewInfo, pOut);
}

// =====================================================================================================================
// Reports if the color target view can support setting COLOR_BIG_PAGE in CB_RMI_GLC2_CACHE_CONTROL.
bool Gfx11ColorTargetView::IsColorBigPage() const
{
    // Buffer views and viewVaLocked image views have already computed whether they can support BIG_PAGE or not.  Other
    // cases have to check now in case the bound memory has changed.
    return ((m_flags.viewVaLocked | m_flags.isBufferView) != 0)
                ? m_flags.colorBigPage
                : IsImageBigPageCompatible(*m_pImage, Gfx10AllowBigPageRenderTarget);
}
#endif

} // Gfx9
} // Pal
