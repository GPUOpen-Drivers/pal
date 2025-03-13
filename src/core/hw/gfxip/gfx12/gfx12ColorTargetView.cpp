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
#include "core/hw/gfxip/gfx12/g_gfx12DataFormats.h"
#include "core/hw/gfxip/gfx12/gfx12CmdStream.h"
#include "core/hw/gfxip/gfx12/gfx12ColorTargetView.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12Image.h"
#include "palFormatInfo.h"

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
// Helper function to modify the base subresource (mip 0, slice 0) extents to handle corner cases where the bits per
// texel doesn't match the bits per addressable element in the hardware.  This occurs with cases like block compressed
// textures and YUV images.
static void SetupExtents(
    const ColorTargetViewCreateInfo& createInfo,
    bool                             useSubresBaseAddr,
    Extent3d*                        pBaseExtent)
{
    const SubresId    baseSubresId = { createInfo.imageInfo.baseSubRes.plane, 0, 0 };
    const ChNumFormat format       = createInfo.swizzledFormat.format;

    const Pal::Image* const      pImage          = static_cast<const Pal::Image*>(createInfo.imageInfo.pImage);
    const SubResourceInfo* const pBaseSubresInfo = pImage->SubresourceInfo(baseSubresId);
    const SubResourceInfo* const pSubresInfo     = pImage->SubresourceInfo(createInfo.imageInfo.baseSubRes);
    const ImageCreateInfo&       imageCreateInfo = pImage->GetImageCreateInfo();
    const bool                   imgIsBc         = Formats::IsBlockCompressed(format);

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
    if (imgIsBc || (pSubresInfo->bitsPerTexel != Formats::BitsPerPixel(format)))
    {
        const uint32 firstMipLevel = createInfo.imageInfo.baseSubRes.mipLevel;

        pBaseExtent->width  = Clamp((pSubresInfo->extentElements.width  << firstMipLevel),
                                    pBaseSubresInfo->extentElements.width,
                                    pBaseSubresInfo->actualExtentElements.width);
        pBaseExtent->height = Clamp((pSubresInfo->extentElements.height << firstMipLevel),
                                    pBaseSubresInfo->extentElements.height,
                                    pBaseSubresInfo->actualExtentElements.height);
    }

    if (Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format) &&
        ((createInfo.imageInfo.arraySize > 1) || (createInfo.imageInfo.baseSubRes.arraySlice != 0)) &&
        (useSubresBaseAddr == false))
    {
        *pBaseExtent = pBaseSubresInfo->actualExtentTexels;

        pImage->GetGfxImage()->PadYuvPlanarViewActualExtent(baseSubresId, pBaseExtent);
    }
}

// =====================================================================================================================
// Buffer-specific Gfx12 state descriptor setup.
void ColorTargetView::BufferViewInit(
    const ColorTargetViewCreateInfo& createInfo,
    const Device*                    pDevice)
{
    const Gfx12PalSettings& settings = pDevice->Settings();
    const ChNumFormat       format   = createInfo.swizzledFormat.format;

    PAL_ASSERT(createInfo.bufferInfo.pGpuMemory != nullptr);

    // The buffer virtual address is simply "offset" pixels from the start of the GPU memory's virtual address.
    const gpusize bufferOffset = createInfo.bufferInfo.offset * Formats::BytesPerPixel(format);
    const gpusize bufferAddr   = createInfo.bufferInfo.pGpuMemory->Desc().gpuVirtAddr + bufferOffset;

    // Convert to a 256-bit aligned base address and a base offset. Note that we don't need to swizzle the base
    // address because buffers aren't macro tiled.
    const gpusize baseOffset = bufferAddr & 0xFF;
    const gpusize baseAddr   = bufferAddr & ~0xFF;

    Regs::Get<mmCB_COLOR0_BASE, CB_COLOR0_BASE>(m_regs)->bits.BASE_256B         = Get256BAddrLo(baseAddr);
    Regs::Get<mmCB_COLOR0_BASE_EXT, CB_COLOR0_BASE_EXT>(m_regs)->bits.BASE_256B = Get256BAddrHi(baseAddr);

    auto* pView = Regs::Get<mmCB_COLOR0_VIEW, CB_COLOR0_VIEW>(m_regs);

    // The view slice_start is overloaded to specify the base offset.
    pView->bits.SLICE_START = baseOffset;

    // VIEW2 is already initialized to 0.

    auto* pAttrib3 = Regs::Get<mmCB_COLOR0_ATTRIB3, CB_COLOR0_ATTRIB3>(m_regs);
    static_assert(uint32(ImageType::Tex1d) == 0, "1D Value does not match expected HW value.");
    pAttrib3->bits.RESOURCE_TYPE    = 0;
    pAttrib3->bits.SPECULATIVE_READ = 0; // Auto mode

    auto* pInfo = Regs::Get<mmCB_COLOR0_INFO, CB_COLOR0_INFO>(m_regs);
    pInfo->bits.LINEAR_GENERAL = 1;

    auto* pFdccControl = Regs::Get<mmCB_COLOR0_FDCC_CONTROL, CB_COLOR0_FDCC_CONTROL>(m_regs);

    // Disable distributed compression for buffer by default.
    pFdccControl->bits.MAX_COMPRESSED_BLOCK_SIZE   = settings.defaultMaxCompressedBlockSize;
    pFdccControl->bits.MAX_UNCOMPRESSED_BLOCK_SIZE = DefaultMaxUncompressedSize;
    pFdccControl->bits.FRAGMENT_COMPRESS_DISABLE   = 1;
    switch (createInfo.compressionMode)
    {
    case CompressionMode::Default:
    case CompressionMode::ReadEnableWriteEnable:
        pFdccControl->bits.COMPRESSION_MODE = uint32(RbCompressionMode::Default);
        break;
    case CompressionMode::ReadEnableWriteDisable:
        pFdccControl->bits.COMPRESSION_MODE = uint32(RbCompressionMode::CompressWriteDisable);
        break;
    case CompressionMode::ReadBypassWriteDisable:
        if (settings.enableCompressionReadBypass)
        {
            pFdccControl->bits.COMPRESSION_MODE = static_cast<uint32>(RbCompressionMode::ReadBypassWriteDisable);
        }
        else
        {
            pFdccControl->bits.COMPRESSION_MODE = static_cast<uint32>(RbCompressionMode::CompressWriteDisable);
        }
        break;
    default:
        PAL_NEVER_CALLED();
    }
    pFdccControl->bits.ENABLE_MAX_COMP_FRAG_OVERRIDE = 0;
    pFdccControl->bits.MAX_COMP_FRAGS                = 0;

    auto* pAttrib2 = Regs::Get<mmCB_COLOR0_ATTRIB2, CB_COLOR0_ATTRIB2>(m_regs);

    // The pixel height/width specified by the client.
    pAttrib2->bits.MIP0_WIDTH  = (createInfo.bufferInfo.extent - 1);
    pAttrib2->bits.MIP0_HEIGHT = 0;
}

// =====================================================================================================================
// Image-specific Gfx12 state descriptor setup.
void ColorTargetView::ImageViewInit(
    const ColorTargetViewCreateInfo&         createInfo,
    const ColorTargetViewInternalCreateInfo& internalCreateInfo)
{
    const Pal::Image* const   pImage        = static_cast<const Pal::Image*>(createInfo.imageInfo.pImage);
    const GfxImage* const     pGfxImage     = static_cast<const GfxImage*>(pImage->GetGfxImage());
    const ChNumFormat         format        = createInfo.swizzledFormat.format;
    const auto&               gfx12Settings = GetGfx12Settings(pImage->GetDevice());

    m_pImage = pImage;

    // Don't expect to need support for color target views with non-locked VAs on Gfx12 hardware.
    PAL_ASSERT(createInfo.flags.imageVaLocked);
    PAL_ASSERT(pImage->GetBoundGpuMemory().IsBound());

    const SubresId subresId     = createInfo.imageInfo.baseSubRes;
    const SubresId baseSubresId = { subresId.plane, 0, 0 };

    const SubResourceInfo* const pSubresInfo     = pImage->SubresourceInfo(subresId);
    const SubResourceInfo* const pBaseSubresInfo = pImage->SubresourceInfo(baseSubresId);

    const ImageCreateInfo& imageCreateInfo = pImage->GetImageCreateInfo();

    // The Y and UV planes of a YUV-planar Image are interleaved, so we need to include padding when we set up a
    // color-target view so that the HW will correctly span all planes when addressing nonzero array slices. This
    // padding can cause problems because the HW thinks each plane is larger than it actually is.  A better
    // solution for single-slice views is to use the subresource address for the color address instead of the
    // slice0 base address.
    const bool useSubresBaseAddr = Formats::IsYuvPlanar(format)                    &&
                                   (imageCreateInfo.mipLevels == 1)                &&
                                   (imageCreateInfo.imageType == ImageType::Tex2d) &&
                                   (createInfo.imageInfo.arraySize == 1);

    if (useSubresBaseAddr)
    {
        // Get the base address of each array slice.
        const gpusize subresBaseAddr = pImage->GetSubresourceBaseAddr(subresId);

        const auto*   pTileInfo   = AddrMgr3::GetTileInfo(pImage, subresId);
        const gpusize pipeBankXor = pTileInfo->pipeBankXor;
        const gpusize addrWithXor = subresBaseAddr | (pipeBankXor << 8);

        Regs::Get<mmCB_COLOR0_BASE, CB_COLOR0_BASE>(m_regs)->bits.BASE_256B         = Get256BAddrLo(addrWithXor);
        Regs::Get<mmCB_COLOR0_BASE_EXT, CB_COLOR0_BASE_EXT>(m_regs)->bits.BASE_256B = Get256BAddrHi(addrWithXor);
    }
    else
    {
        const gpusize baseAddr256B = pGfxImage->GetSubresource256BAddr(subresId);
        Regs::Get<mmCB_COLOR0_BASE, CB_COLOR0_BASE>(m_regs)->bits.BASE_256B         = LowPart(baseAddr256B);
        Regs::Get<mmCB_COLOR0_BASE_EXT, CB_COLOR0_BASE_EXT>(m_regs)->bits.BASE_256B = HighPart(baseAddr256B);
    }

    auto* pView  = Regs::Get<mmCB_COLOR0_VIEW, CB_COLOR0_VIEW>(m_regs);
    auto* pView2 = Regs::Get<mmCB_COLOR0_VIEW2, CB_COLOR0_VIEW2>(m_regs);

    if ((createInfo.flags.zRangeValid == 1) && (imageCreateInfo.imageType == ImageType::Tex3d))
    {
        pView->bits.SLICE_START = createInfo.zRange.offset;
        pView->bits.SLICE_MAX   = createInfo.zRange.offset + createInfo.zRange.extent - 1;
        pView2->bits.MIP_LEVEL  = subresId.mipLevel;
    }
    else if (useSubresBaseAddr)
    {
        pView->bits.SLICE_START = 0;
    }
    else
    {
        pView->bits.SLICE_START = subresId.arraySlice;
        pView->bits.SLICE_MAX   = subresId.arraySlice + createInfo.imageInfo.arraySize - 1;
        pView2->bits.MIP_LEVEL  = subresId.mipLevel;
    }

    // Potentially modify extents or base subresource (mip0/slice0) extents based on oddball situations where
    // HW addressable elements and texels are not 1:1.
    Extent3d baseExtent = pBaseSubresInfo->extentTexels;
    SetupExtents(createInfo, useSubresBaseAddr, &baseExtent);

    const Image& gfx12Image  = *static_cast<Image*>(pImage->GetGfxImage());
    const uint32 plane       = createInfo.imageInfo.baseSubRes.plane;
    const auto*  gfx12Device = static_cast<const Pal::Gfx12::Device*>(pImage->GetDevice()->GetGfxDevice());

    const bool enableFragmentClientCompression =
        gfx12Image.EnableClientCompression(internalCreateInfo.flags.disableClientCompression);

    // Disable distributed compression by default.
    auto* pFdccControl = Regs::Get<mmCB_COLOR0_FDCC_CONTROL, CB_COLOR0_FDCC_CONTROL>(m_regs);
    pFdccControl->bits.MAX_COMPRESSED_BLOCK_SIZE   = gfx12Image.GetMaxCompressedSize(plane);
    pFdccControl->bits.MAX_UNCOMPRESSED_BLOCK_SIZE = gfx12Image.GetMaxUncompressedSize(plane);
    pFdccControl->bits.FRAGMENT_COMPRESS_DISABLE   = enableFragmentClientCompression ? 0 : 1;

    const CompressionMode imageCompressionMode = createInfo.imageInfo.pImage->GetImageCreateInfo().compressionMode;

    static_assert((uint32(CompressionMode::Default)                == RtvCompressionDefault)                &&
                  (uint32(CompressionMode::ReadEnableWriteEnable)  == RtvCompressionReadEnableWriteEnable)  &&
                  (uint32(CompressionMode::ReadEnableWriteDisable) == RtvCompressionReadEnableWriteDisable));

    CompressionMode finalCompressionMode = static_cast<CompressionMode>(gfx12Settings.rtvCompressionMode);
    if (finalCompressionMode == CompressionMode::Default)
    {
        finalCompressionMode =
            gfx12Device->GetImageViewCompressionMode(createInfo.compressionMode,
                                                     imageCompressionMode,
                                                     pImage->GetBoundGpuMemory().Memory());
    }
    switch (finalCompressionMode)
    {
    case CompressionMode::Default:
    case CompressionMode::ReadEnableWriteEnable:
        pFdccControl->bits.COMPRESSION_MODE = static_cast<uint32>(RbCompressionMode::Default);
        break;
    case CompressionMode::ReadEnableWriteDisable:
        pFdccControl->bits.COMPRESSION_MODE = static_cast<uint32>(RbCompressionMode::CompressWriteDisable);
        break;
    case CompressionMode::ReadBypassWriteDisable:
        pFdccControl->bits.COMPRESSION_MODE = static_cast<uint32>(RbCompressionMode::ReadBypassWriteDisable);
        break;
    default:
        PAL_NEVER_CALLED();
    }
    pFdccControl->bits.ENABLE_MAX_COMP_FRAG_OVERRIDE = 1;
    pFdccControl->bits.MAX_COMP_FRAGS                = (imageCreateInfo.fragments == 8) ? 3 :
                                                       (imageCreateInfo.fragments == 4) ? 2 :
                                                                                          0;

    auto* pAttrib2 = Regs::Get<mmCB_COLOR0_ATTRIB2, CB_COLOR0_ATTRIB2>(m_regs);
    pAttrib2->bits.MIP0_WIDTH  = baseExtent.width - 1;
    pAttrib2->bits.MIP0_HEIGHT = baseExtent.height - 1;

    auto* pAttrib3 = Regs::Get<mmCB_COLOR0_ATTRIB3, CB_COLOR0_ATTRIB3>(m_regs);
    pAttrib3->bits.MAX_MIP         = imageCreateInfo.mipLevels - 1;
    pAttrib3->bits.MIP0_DEPTH      = (imageCreateInfo.imageType == ImageType::Tex3d) ?
                                     (imageCreateInfo.extent.depth - 1)              :
                                     (imageCreateInfo.arraySize - 1);
    pAttrib3->bits.COLOR_SW_MODE   = pGfxImage->GetSwTileMode(pSubresInfo);
    pAttrib3->bits.RESOURCE_TYPE   = uint32(imageCreateInfo.imageType);

    pAttrib3->bits.SPECULATIVE_READ = 0; // Auto mode

    auto* pAttrib = Regs::Get<mmCB_COLOR0_ATTRIB, CB_COLOR0_ATTRIB>(m_regs);
    pAttrib->bits.NUM_FRAGMENTS = Log2(imageCreateInfo.fragments);
}

// =====================================================================================================================
ColorTargetView::ColorTargetView(
    const Device*                     pDevice,
    const ColorTargetViewCreateInfo&  createInfo,
    ColorTargetViewInternalCreateInfo internalCreateInfo,
    uint32                            viewId)
    :
    m_regs{},
    m_uniqueId(viewId),
    m_pImage(nullptr)
{
    Regs::Init(m_regs);

    const Pal::Device*const pParentDevice = pDevice->Parent();
    const ChNumFormat       format        = createInfo.swizzledFormat.format;

    auto* pInfo = Regs::Get<mmCB_COLOR0_INFO, CB_COLOR0_INFO>(m_regs);
    pInfo->bits.FORMAT      = Formats::Gfx12::HwColorFmt(format);
    pInfo->bits.NUMBER_TYPE = Formats::Gfx12::ColorSurfNum(format);
    pInfo->bits.COMP_SWAP   = Formats::Gfx12::ColorCompSwap(createInfo.swizzledFormat);

    // Set bypass blending for any format that is not blendable. Blend clamp must be cleared if blendBypass is set.
    // Otherwise, it must be set iff any component is Snorm, Unorm, or Srgb.
    const bool blendBypass  = (pParentDevice->SupportsBlend(format, ImageTiling::Optimal) == false);
    const bool isNormOrSrgb = Formats::IsNormalized(format) || Formats::IsSrgb(format);
    const bool blendClamp   = (blendBypass == false) && isNormOrSrgb;

    pInfo->bits.BLEND_CLAMP  = blendClamp;
    pInfo->bits.BLEND_BYPASS = blendBypass;
    pInfo->bits.SIMPLE_FLOAT = Pal::Device::CbSimpleFloatEnable;

    // Selects between truncating (standard for floats) and rounding (standard for most other cases) to convert blender
    // results to frame buffer components. Round mode must be set to ROUND_BY_HALF if any component is UNORM, SNORM or
    // SRGB otherwise ROUND_TRUNCATE.
    pInfo->bits.ROUND_MODE = isNormOrSrgb ? ROUND_BY_HALF : ROUND_TRUNCATE;

    auto* pAttrib = Regs::Get<mmCB_COLOR0_ATTRIB, CB_COLOR0_ATTRIB>(m_regs);
    pAttrib->bits.FORCE_DST_ALPHA_1 = Formats::HasUnusedAlpha(createInfo.swizzledFormat);

    // The rest of the descriptor setup diverges based on whether this is a buffer or image color target view.
    if (createInfo.flags.isBufferView)
    {
        BufferViewInit(createInfo, pDevice);
    }
    else
    {
        ImageViewInit(createInfo, internalCreateInfo);
    }
}

// =====================================================================================================================
// Returns the 2D pixel extents of the color target view.
Extent2d ColorTargetView::Extent() const
{
    const CB_COLOR0_ATTRIB2& attrib2 = Regs::GetC<mmCB_COLOR0_ATTRIB2, CB_COLOR0_ATTRIB2>(m_regs);
    const CB_COLOR0_VIEW2&   view2   = Regs::GetC<mmCB_COLOR0_VIEW2, CB_COLOR0_VIEW2>(m_regs);

    const Extent2d extent =
    {
        Max((attrib2.bits.MIP0_WIDTH  + 1u) >> view2.bits.MIP_LEVEL, 1u),
        Max((attrib2.bits.MIP0_HEIGHT + 1u) >> view2.bits.MIP_LEVEL, 1u)
    };

    return extent;
}

// =====================================================================================================================
uint32* ColorTargetView::CopyRegPairsToCmdSpace(
    uint32             index,
    uint32*            pCmdSpace,
    bool*              pWriteCbDbHighBaseRegs,
    const Pal::Device& device
    ) const
{
    static_assert(Regs::Size() == Regs::NumContext(), "Unexpected additional registers!");

    constexpr uint32 CbRegsPerSlot = (mmCB_COLOR1_BASE - mmCB_COLOR0_BASE);
    static constexpr uint32 OffsetTable[] =
    {
        CbRegsPerSlot,  // mmCB_COLOR0_BASE
        CbRegsPerSlot,  // mmCB_COLOR0_VIEW
        CbRegsPerSlot,  // mmCB_COLOR0_VIEW2
        CbRegsPerSlot,  // mmCB_COLOR0_ATTRIB
        CbRegsPerSlot,  // mmCB_COLOR0_FDCC_CONTROL
        1,              // mmCB_COLOR0_INFO
        CbRegsPerSlot,  // mmCB_COLOR0_ATTRIB2
        CbRegsPerSlot,  // mmCB_COLOR0_ATTRIB3
        1,              // mmCB_COLOR0_BASE_EXT
    };
    static_assert(Util::ArrayLen(OffsetTable) == Regs::Size(), "Unexpected mismatch of size.");
    static_assert(Regs::Index(mmCB_COLOR0_INFO) == 5, "Unexpected index for CB_COLOR0_INFO.");
    static_assert(OffsetTable[Regs::Index(mmCB_COLOR0_INFO)] == 1, "Unexpected offset for CB_COLOR0_INFO.");
    static_assert(Regs::Index(mmCB_COLOR0_BASE_EXT) == Regs::Size() - 1,
                  "Unexpected index for CB_COLOR0_BASE_EXT.");
    static_assert(Util::CheckSequential({
        mmCB_COLOR0_BASE_EXT, mmCB_COLOR1_BASE_EXT, mmCB_COLOR2_BASE_EXT, mmCB_COLOR3_BASE_EXT,
        mmCB_COLOR4_BASE_EXT, mmCB_COLOR5_BASE_EXT, mmCB_COLOR6_BASE_EXT, mmCB_COLOR7_BASE_EXT }),
        "The ordering of the CB_COLOR#_BASE_EXT regs changed!");

    RegisterValuePair regs[Regs::Size()];
    memcpy(regs, m_regs, sizeof(regs));

    if (regs[Regs::Index(mmCB_COLOR0_BASE_EXT)].value != 0)
    {
        *pWriteCbDbHighBaseRegs = true;
    }

    const uint32 numRegPairs = *pWriteCbDbHighBaseRegs ? Regs::Size() : Regs::Size() - 1;

    for (uint32 i = 0; i < numRegPairs; i++)
    {
        regs[i].offset += (index * OffsetTable[i]);
    }

    memcpy(pCmdSpace, regs, sizeof(RegisterValuePair) * numRegPairs);

#if PAL_DEVELOPER_BUILD
    Developer::SurfRegDataInfo data = {};
    data.type    = Developer::SurfRegDataType::RenderTargetView;
    data.regData = Regs::GetC<mmCB_COLOR0_BASE, CB_COLOR0_BASE>(m_regs).u32All;
    device.DeveloperCb(Developer::CallbackType::SurfRegData, &data);
#endif

    return pCmdSpace + (numRegPairs * sizeof(RegisterValuePair) / sizeof(uint32));
}

// =====================================================================================================================
bool ColorTargetView::Equals(
    const ColorTargetView* pOther
    ) const
{
    return ((pOther != nullptr) && (m_uniqueId == pOther->m_uniqueId));
}

}
}
