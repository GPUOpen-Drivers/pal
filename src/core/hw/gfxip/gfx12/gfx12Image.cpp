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
#include "core/addrMgr/addrMgr3/addrMgr3.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx12/gfx12Chip.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12Image.h"
#include "core/hw/gfxip/gfx12/gfx12Metadata.h"
#include "util/palInlineFuncs.h"

using namespace Pal::AddrMgr3;
using namespace Util;

namespace Pal
{
namespace Gfx12
{

uint32 Image::s_cbSwizzleIdx = 0;
uint32 Image::s_txSwizzleIdx = 0;

// =====================================================================================================================
Image::Image(
    Pal::Image*        pParentImage,
    ImageInfo*         pImageInfo,
    const Pal::Device& device)
    :
    GfxImage(pParentImage, pImageInfo, device),
    m_gfxDevice(static_cast<const Device&>(*device.GetGfxDevice())),
    m_gpuMemSyncSize(0),
    m_addrSurfOutput{},
    m_addrMipOutput{},
    m_planeOffset{},
    m_totalPlaneSize(0),
    m_pHiSZ(nullptr),
    m_hiSZValidLayout{},
    m_hiSZStateMetaDataOffset(0)
{
    for (uint32 planeIdx = 0; planeIdx < MaxNumPlanes; planeIdx++)
    {
        m_addrSurfOutput[planeIdx].size     = sizeof(ADDR3_COMPUTE_SURFACE_INFO_OUTPUT);
        m_addrSurfOutput[planeIdx].pMipInfo = &m_addrMipOutput[planeIdx][0];
    }

    const DisplayDccCaps& displayDcc = pImageInfo->internalCreateInfo.displayDcc;

    if (pImageInfo->internalCreateInfo.flags.useSharedDccState)
    {
        m_dccControl = pImageInfo->internalCreateInfo.gfx12.sharedDccControl;
    }
    else if (displayDcc.enabled)
    {
        if (displayDcc.dcc_256_256_plane0 || displayDcc.dcc_256_128_plane0 || displayDcc.dcc_256_64_plane0)
        {
            m_dccControl.maxUncompressedBlockSizePlane0 = MaxUncompressSize256B;

            if (displayDcc.dcc_256_256_plane0)
            {
                m_dccControl.maxCompressedBlockSizePlane0 = MaxCompressSize256B;
            }
            else if (displayDcc.dcc_256_128_plane0)
            {
                m_dccControl.maxCompressedBlockSizePlane0 = MaxCompressSize128B;
            }
            else if (displayDcc.dcc_256_64_plane0)
            {
                m_dccControl.maxCompressedBlockSizePlane0 = MaxCompressSize64B;
            }
        }

        if (displayDcc.dcc_256_256_plane1 || displayDcc.dcc_256_128_plane1 || displayDcc.dcc_256_64_plane1)
        {
            m_dccControl.maxUncompressedBlockSizePlane1 = MaxUncompressSize256B;

            if (displayDcc.dcc_256_256_plane1)
            {
                m_dccControl.maxCompressedBlockSizePlane1 = MaxCompressSize256B;
            }
            else if (displayDcc.dcc_256_128_plane1)
            {
                m_dccControl.maxCompressedBlockSizePlane1 = MaxCompressSize128B;
            }
            else if (displayDcc.dcc_256_64_plane1)
            {
                m_dccControl.maxCompressedBlockSizePlane1 = MaxCompressSize64B;
            }
        }
    }
    else
    {
        const MaxCompressSize defMaxCompressedSize = m_gfxDevice.Settings().defaultMaxCompressedBlockSize;

        m_dccControl.maxUncompressedBlockSizePlane0 = DefaultMaxUncompressedSize;
        m_dccControl.maxCompressedBlockSizePlane0   = defMaxCompressedSize;
        m_dccControl.maxUncompressedBlockSizePlane1 = DefaultMaxUncompressedSize;
        m_dccControl.maxCompressedBlockSizePlane1   = defMaxCompressedSize;
    }
}

// =====================================================================================================================
Image::~Image()
{
    Pal::GfxImage::Destroy();

     PAL_SAFE_FREE(m_pHiSZ, m_device.GetPlatform());
}

// =====================================================================================================================
// Function for updating the subResInfo offset to reflect each sub-resources position in the final image.  On input,
// the subres offset reflects the offset of that subresource within a generic slice, but not that slice's position
// in the overall image.
void Image::Addr3InitSubResInfo(
    const SubResIterator& subResIt,
    SubResourceInfo*      pSubResInfoList,
    void*                 pSubResTileInfoList,
    gpusize*              pGpuMemSize)
{
    const Pal::Image*      pParent     = Parent();
    const ImageCreateInfo& createInfo  = pParent->GetImageCreateInfo();
    const uint32           numPlanes   = pParent->GetImageInfo().numPlanes;
    const bool             isYuvPlanar = Formats::IsYuvPlanar(createInfo.swizzledFormat.format);

    SetupPlaneOffsets(numPlanes, isYuvPlanar);

    const SubresId         subresId  = subResIt.GetSubresId();
    SubResourceInfo*const  pSubRes   = pSubResInfoList + subResIt.Index();
    TileInfo*const         pTileInfo = NonConstTileInfo(pSubResTileInfoList, subResIt.Index());

    if (isYuvPlanar == false)
    {
        const ADDR3_COMPUTE_SURFACE_INFO_OUTPUT& addrOutput = GetAddrOutput(subresId);

        // For non-YUV planar surfaces, each plane is stored contiguously.  i.e., all of plane 0 data is stored
        // prior to plane 1 data starting.
        pSubRes->offset = pSubRes->offset      + // existing offset to this miplevel within the slice
                          m_planeOffset[subresId.plane] + // offset of any previous planes
                          (subresId.arraySlice * addrOutput.sliceSize); // offset of any previous slices
    }
    else
    {
        // YUV planar surfaces are stored in [y] [uv] order for each slice.  i.e., the Y data across various
        // slices is non-contiguous.  YUV surfaces can't have multiple mip levels.
        pSubRes->offset = m_planeOffset[subresId.plane] +           // offset within this slice
                          (subresId.arraySlice * m_totalPlaneSize); // all previous slices

        // Because the padding of these surfaces may overlap, recalculate the size from the next offset.
        const uint32  nextPlane       = subresId.plane + 1;
        const gpusize nextPlaneOffset = (nextPlane >= numPlanes) ? m_totalPlaneSize : m_planeOffset[nextPlane];

        pSubRes->size = nextPlaneOffset - m_planeOffset[subresId.plane];
    }

    if (subresId.mipLevel == 0)
    {
        // In AddrMgr3, each subresource's size represents the size of the full mip-chain it belongs to. By
        // adding the size of mip-level zero to the running GPU memory size, we can keep a running total of
        // the entire Image's size.
        *pGpuMemSize += pSubRes->size;
        pTileInfo->backingStoreOffset += *pGpuMemSize;
    }
    else
    {
        const TileInfo*const pBaseTileInfo = NonConstTileInfo(pSubResTileInfoList, subResIt.BaseIndex());

        pTileInfo->backingStoreOffset += pBaseTileInfo->backingStoreOffset;
    }
}

// =====================================================================================================================
// Saves state from the AddrMgr about a particular plane for this Image and computes the bank/pipe XOR value for
// the plane.
Result Image::Addr3FinalizePlane(
    SubResourceInfo*                         pBaseSubRes,
    void*                                    pBaseTileInfo,
    Addr3SwizzleMode                         swizzleMode,
    const ADDR3_COMPUTE_SURFACE_INFO_OUTPUT& surfaceInfo)
{
    const uint32 plane = pBaseSubRes->subresId.plane;

    memcpy(&m_addrSurfOutput[plane], &surfaceInfo, sizeof(m_addrSurfOutput[0]));
    m_finalSwizzleModes[plane] = swizzleMode;

    m_addrSurfOutput[plane].pMipInfo = &m_addrMipOutput[plane][0];

    for (uint32 mip = 0; mip < m_createInfo.mipLevels; ++mip)
    {
        memcpy(&m_addrMipOutput[plane][mip], (surfaceInfo.pMipInfo + mip), sizeof(m_addrMipOutput[0][0]));
    }

    auto*const pTileInfo = static_cast<AddrMgr3::TileInfo*>(pBaseTileInfo);

    static constexpr SwizzleMode ConversionTable[] =
    {
        SwizzleModeLinear,         //  ADDR3_LINEAR
        SwizzleMode256B2D,         //  ADDR3_256B_2D
        SwizzleMode4Kb2D,          //  ADDR3_4KB_2D
        SwizzleMode64Kb2D,         //  ADDR3_64KB_2D
        SwizzleMode256Kb2D,        //  ADDR3_256KB_2D
        SwizzleMode4Kb3D,          //  ADDR3_4KB_3D
        SwizzleMode64Kb3D,         //  ADDR3_64KB_3D
        SwizzleMode256Kb3D         //  ADDR3_256KB_3D
    };

    PAL_ASSERT(swizzleMode < ADDR3_MAX_TYPE);
    pBaseSubRes->swizzleMode = ConversionTable[swizzleMode];

    // Compute the pipe/bank XOR value for the subresource.
    return ComputePipeBankXor(plane, swizzleMode, &pTileInfo->pipeBankXor);
}

// =====================================================================================================================
// Calculate the tile swizzle (pipe/bank XOR value).
Result Image::ComputePipeBankXor(
    uint32            plane,
    Addr3SwizzleMode  swizzleMode,
    uint32*           pPipeBankXor
    ) const
{
    Result result = Result::Success;

    // A pipe/bank xor setting of zero is always valid.
    *pPipeBankXor = 0;

    // Note that if OptForSpeed is selected, we will use PipeBankXor algorithm for performance.
    if (m_createInfo.tilingOptMode == TilingOptMode::OptForSpeed)
    {
        const PalSettings& coreSettings = m_device.Settings();
        const bool isDepthStencil       = Parent()->IsDepthStencilTarget();
        const bool isColorPlane         = Parent()->IsColorPlane(plane);

        PAL_ASSERT((isDepthStencil && (plane < 2)) || (isDepthStencil == false));

        // Also need to make sure that mip0 is not in miptail. In this case, tile swizzle cannot be supported. With current
        // design, when mip0 is in the miptail, swizzleOffset would be negative. This is a problem because the offset in MS
        // interface is a UINT.
        const bool mipChainInTail = m_addrSurfOutput[plane].mipChainInTail;

        // There is no longer SW_*_X mode so the check is simplified.
        if (mipChainInTail == false)
        {
            if (m_pImageInfo->internalCreateInfo.flags.useSharedTilingOverrides)
            {
                if (isColorPlane || isDepthStencil)
                {
                    // If this is a shared image, then the pipe/bank xor value has been given to us. Just take that.
                    *pPipeBankXor = m_pImageInfo->internalCreateInfo.sharedPipeBankXor[plane];
                }
                else if (Formats::IsYuv(m_createInfo.swizzledFormat.format))
                {
                    // If this is a shared Yuv image, then the pipe/bank xor value has been given to us. Just take that.
                    *pPipeBankXor = m_pImageInfo->internalCreateInfo.sharedPipeBankXor[plane];
                    PAL_ALERT_ALWAYS_MSG("Shared YUV image with PipeBankXor enabled may result in unexpected behavior.");
                }
                else
                {
                    PAL_NOT_IMPLEMENTED();
                }
            }
            else if (Parent()->IsPeer())
            {
                // Peer images must have the same pipe/bank xor value as the original image.  The pipe/bank xor
                // value is constant across all mips / slices associated with a given plane.
                *pPipeBankXor = AddrMgr3::GetTileInfo(Parent()->OriginalImage(), BaseSubres(plane))->pipeBankXor;
            }
            else if (m_createInfo.flags.fixedTileSwizzle != 0)
            {
                // Our XOR value was specified by the client using the "tileSwizzle" property. Note that we only support
                // this for single-sampled color images, otherwise we'd need more inputs to cover the other planes.
                //
                // It's possible for us to hang the HW if we use an XOR value computed for a different planes so we must
                // return a safe value like the default of zero if the client breaks these rules.
                if (isColorPlane && (m_createInfo.fragments == 1))
                {
                    *pPipeBankXor = m_createInfo.tileSwizzle;

                    // PipebankXor should be zero for ADDR3_LINEAR and ADDR3_256B_2D modes (both has 256B alignment).
                    PAL_ASSERT((m_createInfo.tileSwizzle == 0) || (m_addrSurfOutput[plane].baseAlign > 256));
                }
                else
                {
                    // Otherwise for other cases, tileSwizzle specified by clients can only be 0.
                    PAL_ASSERT(m_createInfo.tileSwizzle == 0);
                }
            }
            else
            {
                // Presentable/flippable images cannot use tile swizzle because the display engine doesn't support it.
                const bool supportSwizzle = ((Parent()->IsPresentable()          == false) &&
                                                (Parent()->IsFlippable()            == false) &&
                                                (Parent()->IsPrivateScreenPresent() == false));

                // This surface can conceivably use swizzling...  make sure the settings allow swizzling for this surface
                // type as well.
                if (supportSwizzle &&
                    ((TestAnyFlagSet(coreSettings.tileSwizzleMode, TileSwizzleColor) && Parent()->IsRenderTarget()) ||
                    (TestAnyFlagSet(coreSettings.tileSwizzleMode, TileSwizzleDepth) && isDepthStencil) ||
                    (TestAnyFlagSet(coreSettings.tileSwizzleMode, TileSwizzleShaderRes))))
                {
                    uint32 surfaceIndex = 0;

                    if (isDepthStencil)
                    {
                        // The depth-stencil index is fixed to the plane index so it's safe to use it in all cases.
                        surfaceIndex = plane;
                    }
                    else if (Parent()->IsDataInvariant() || Parent()->IsCloneable())
                    {
                        // Data invariant and cloneable images must generate identical swizzles given identical create info.
                        // This means we can hash the public create info struct to get half-way decent swizzling.
                        //
                        // Note that one client is not able to guarantee that they consistently set the perSubresInit flag
                        // for all images that must be identical so we need to skip over the ImageCreateFlags.
                        constexpr size_t HashOffset = offsetof(ImageCreateInfo, usageFlags);
                        constexpr uint64 HashSize   = sizeof(ImageCreateInfo) - HashOffset;
                        const uint8*     pHashStart = reinterpret_cast<const uint8*>(&m_createInfo) + HashOffset;

                        uint64 hash = 0;
                        MetroHash64::Hash(pHashStart, HashSize, reinterpret_cast<uint8* const>(&hash));

                        surfaceIndex = MetroHash::Compact32(hash);
                    }
                    else if (Parent()->IsRenderTarget())
                    {
                        surfaceIndex = s_cbSwizzleIdx++;
                    }
                    else
                    {
                        surfaceIndex = s_txSwizzleIdx++;
                    }

                    const auto*      pBaseSubResInfo = Parent()->SubresourceInfo(0);
                    const auto*const pAddrMgr        = static_cast<const AddrMgr3::AddrMgr3*>(m_device.GetAddrMgr());

                    ADDR3_COMPUTE_PIPEBANKXOR_INPUT pipeBankXorInput = { };
                    pipeBankXorInput.size         = sizeof(pipeBankXorInput);
                    pipeBankXorInput.surfIndex    = surfaceIndex;
                    pipeBankXorInput.swizzleMode  = swizzleMode;

                    ADDR3_COMPUTE_PIPEBANKXOR_OUTPUT pipeBankXorOutput = { };
                    pipeBankXorOutput.size = sizeof(pipeBankXorOutput);

                    ADDR_E_RETURNCODE addrRetCode = Addr3ComputePipeBankXor(m_device.AddrLibHandle(),
                                                                            &pipeBankXorInput,
                                                                            &pipeBankXorOutput);
                    if (addrRetCode == ADDR_OK)
                    {
                        // Further limit the PBX value to the number of "known zeroes" in the low portion of the base
                        // address.
                        // Note that the PBX value is stored starting at bit "8" because the low eight bits of the
                        // address are never programmed.
                        *pPipeBankXor = pipeBankXorOutput.pipeBankXor &
                                        ((1 << (Log2(m_addrSurfOutput[plane].baseAlign) - 8)) - 1);
                    }
                    else
                    {
                        result = Result::ErrorUnknown;
                    }
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
void Image::InitMetadataFill(
    Pal::CmdBuffer*    pCmdBuffer,
    const SubresRange& range,
    ImageLayout        layout
    ) const
{
    PAL_ASSERT(Parent()->IsRangeFullPlane(range));

    if (m_pHiSZ != nullptr)
    {
        const auto& gpuMemObj         = *Parent()->GetBoundGpuMemory().Memory();
        const auto  boundGpuMemOffset = Parent()->GetBoundGpuMemory().Offset();

        if (m_pHiSZ->HiZEnabled())
        {
            const uint32 hiZInitValue = m_pHiSZ->GetHiZInitialValue();

            pCmdBuffer->CmdFillMemory(gpuMemObj,
                                      m_pHiSZ->GetOffset(HiSZType::HiZ) + boundGpuMemOffset,
                                      m_pHiSZ->GetSize(HiSZType::HiZ),
                                      hiZInitValue);
        }

        if (m_pHiSZ->HiSEnabled())
        {
            const uint16 hiSInitValue = m_pHiSZ->GetHiSInitialValue();

            pCmdBuffer->CmdFillMemory(gpuMemObj,
                                      m_pHiSZ->GetOffset(HiSZType::HiS) + boundGpuMemOffset,
                                      m_pHiSZ->GetSize(HiSZType::HiS),
                                      (hiSInitValue | (hiSInitValue << 16)));
        }

        if (HasHiSZStateMetaData())
        {
            pCmdBuffer->CmdFillMemory(gpuMemObj,
                                      HiSZStateMetaDataOffset(range.startSubres.mipLevel),
                                      HiSZStateMetaDataSizePerMip * range.numMips,
                                      1);
        }
    }
}

// =====================================================================================================================
// Fillout shared metadata information.
void Image::GetSharedMetadataInfo(
    SharedMetadataInfo* pMetadataInfo
    ) const
{
    memset(pMetadataInfo, 0, sizeof(SharedMetadataInfo));

    if (m_pHiSZ != nullptr)
    {
        if (m_pHiSZ->HiZEnabled())
        {
            pMetadataInfo->hiZOffset      = m_pHiSZ->GetOffset(HiSZType::HiZ);
            pMetadataInfo->hiZSwizzleMode = m_pHiSZ->GetSwizzleMode(HiSZType::HiZ);
        }

        if (m_pHiSZ->HiSEnabled())
        {
            pMetadataInfo->hiSOffset      = m_pHiSZ->GetOffset(HiSZType::HiS);
            pMetadataInfo->hiSSwizzleMode = m_pHiSZ->GetSwizzleMode(HiSZType::HiS);
        }
    }
}

// =====================================================================================================================
// Initializes the m_htileValidLayout which are used barrier calls to determine which operations are needed
// when transitioning between different Image layouts.
void Image::InitLayoutStateMasks()
{
    if (m_pHiSZ != nullptr)
    {
        // Initialize HiZ/HiS valid layout mask.
        constexpr uint32 DbUsages         = LayoutDepthStencilTarget;
        constexpr uint32 ShaderReadUsages = LayoutCopySrc | LayoutResolveSrc | LayoutShaderRead | LayoutSampleRate;
        ImageLayout      hiSZValidLayout  = {};

        // Layouts that are HiZ/HiS valid support both depth rendering and shader reads (not though shader writes)
        // in the universal queue and compute queue.
        hiSZValidLayout.usages  = DbUsages | ShaderReadUsages;
        hiSZValidLayout.engines = LayoutUniversalEngine | LayoutComputeEngine;

        if (m_pHiSZ->HiZEnabled())
        {
            m_hiSZValidLayout[0] = hiSZValidLayout;
        }
        if (m_pHiSZ->HiSEnabled())
        {
            m_hiSZValidLayout[GetStencilPlane()] = hiSZValidLayout;
        }
    }
}

// =====================================================================================================================
// "Finalizes" this Image object: this includes determining what metadata surfaces need to be used for this Image, and
// initializing the data structures for them.
Result Image::Finalize(
    bool               dccUnsupported,
    SubResourceInfo*   pSubResInfoList,
    ImageMemoryLayout* pGpuMemLayout,
    gpusize*           pGpuMemSize,
    gpusize*           pGpuMemAlignment)
{
    const SharedMetadataInfo& sharedMetadata    = m_pImageInfo->internalCreateInfo.sharedMetadata;
    const bool                useSharedMetadata = m_pImageInfo->internalCreateInfo.flags.useSharedMetadata;

    m_pImageInfo->resolveMethod.depthStencilCopy = 0; // Unsupported on gfx12.

    HiSZUsageFlags hiSZUsage = {};
    Result         result    = Result::Success;

    if (useSharedMetadata)
    {
        hiSZUsage.hiZ = (sharedMetadata.hiZOffset != 0);
        hiSZUsage.hiS = (sharedMetadata.hiSOffset != 0);
    }
    else
    {
        hiSZUsage = HiSZ::UseHiSZForImage(*this);
    }

    // Only depth stencil image may have metadata.
    PAL_ASSERT((hiSZUsage.value == 0) || Parent()->IsDepthStencilTarget());

    // Initialize HiZ/HiS surface.
    if (hiSZUsage.value != 0)
    {
        void* pHiSZMem = PAL_CALLOC(sizeof(HiSZ), m_device.GetPlatform(), SystemAllocType::AllocObject);

        result = (pHiSZMem != nullptr) ? Result::Success : Result::ErrorOutOfMemory;

        if (result == Result::Success)
        {
            m_pHiSZ = PAL_PLACEMENT_NEW(pHiSZMem) HiSZ(*this, hiSZUsage);

            result = m_pHiSZ->Init(pGpuMemSize);
        }

        if (result == Result::Success)
        {
            // It's possible for the HiZ/HiS allocation to require more alignment than the base allocation. Bump
            // up the required alignment of the app-provided allocation if necessary.
            *pGpuMemAlignment = Max(*pGpuMemAlignment, m_pHiSZ->Alignment());

            UpdateMetaDataLayout(pGpuMemLayout, m_pHiSZ->MemoryOffset(), m_pHiSZ->Alignment());

            // If we have a valid metadata offset we also need a metadata size.
            if (pGpuMemLayout->metadataOffset != 0)
            {
                pGpuMemLayout->metadataSize = (*pGpuMemSize - pGpuMemLayout->metadataOffset);
            }

            // Allocate HiSZ state metadata for image with both depth and stencil planes.
            if (m_gfxDevice.Settings().waHiZsDisableWhenZsWrite && (m_pImageInfo->numPlanes == 2))
            {
                InitHiSZStateMetaData(pGpuMemLayout, pGpuMemSize);

                // If we have a valid metadata header offset we also need a metadata header size.
                if (pGpuMemLayout->metadataHeaderOffset != 0)
                {
                    pGpuMemLayout->metadataHeaderSize = (*pGpuMemSize - pGpuMemLayout->metadataHeaderOffset);
                }
            }
        }

    } // End check for needing hTile data

    m_gpuMemSyncSize = *pGpuMemSize;

    // Force its size 16 bytes aligned so it's able to go through the fastest CopyBufferDword in CopyMemoryCs (e.g.
    // called by CmdCopyMemory or CmdCloneImageData or clone copy in CmdCopyImage).
    *pGpuMemSize = Pow2Align(*pGpuMemSize, 16);

    InitLayoutStateMasks();

    if (m_createInfo.flags.prt != 0)
    {
        m_device.GetAddrMgr()->ComputePackedMipInfo(*Parent(), pGpuMemLayout);
    }

#if PAL_DEVELOPER_BUILD
    if (m_pParent != nullptr)
    {
        constexpr SubresId     BaseSubresId = SubresId{};
        const SubResourceInfo* pSubResInfo  = m_pParent->SubresourceInfo(BaseSubresId);

        if (pSubResInfo != nullptr)
        {
            Developer::ImageDataAddrMgrSurfInfo data = {};

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 888
            data.tiling.gfx9.swizzle            = GetFinalSwizzleMode(BaseSubresId);
#endif

            data.flags.properties.color         = m_createInfo.usageFlags.colorTarget;
            data.flags.properties.depth         = m_createInfo.usageFlags.depthStencil;
            data.flags.properties.stencil       = (m_createInfo.usageFlags.noStencilShaderRead == 0);
            data.flags.properties.texture       = m_createInfo.usageFlags.shaderRead;
            data.flags.properties.volume        = (m_createInfo.imageType == ImageType::Tex3d);
            data.flags.properties.cube          = m_createInfo.flags.cubemap;
            data.flags.properties.fmask         = HasFmaskData();
            data.flags.properties.display       = m_createInfo.flags.flippable;
            data.flags.properties.prt           = m_createInfo.flags.prt;
            data.flags.properties.tcCompatible  = pSubResInfo->flags.supportMetaDataTexFetch;
            data.flags.properties.dccCompatible = 0;

            // Note that images with multiple planes can have multiple swizzles so this is incomplete...
            data.swizzle = GetFinalSwizzleMode(BaseSubresId);
            data.size    = m_pParent->GetGpuMemSize();
            data.bpp     = pSubResInfo->bitsPerTexel;
            data.width   = m_createInfo.extent.width;
            data.height  = m_createInfo.extent.height;
            data.depth   = m_createInfo.extent.depth;

            m_device.DeveloperCb(Developer::CallbackType::CreateImage, &data);
        }
    }
#endif

    return Result::Success;
}

// =====================================================================================================================
// Returns the virtual address used for HW programming of the given mip.  Returned value includes any pipe-bank-xor
// value associated with this subresource id.
gpusize Image::GetMipAddr(
    SubresId subresId,
    bool     includeXor
    ) const
{
    const Pal::Image* pParent          = Parent();
    const auto*       pBaseSubResInfo  = pParent->SubresourceInfo(subresId);
    const bool        isYuvPlanarArray = pParent->IsYuvPlanarArray();

    // On GFX12, programming is based on the logical starting address of the plane.  Mips are stored in reverse order
    // (i.e., mip 0 is *last* and the last mip level isn't necessarily at offset zero either), so we need to figure out
    // where this plane begins.
    const gpusize planeOffset = isYuvPlanarArray ? pBaseSubResInfo->offset : m_planeOffset[subresId.plane];

    gpusize imageBaseAddr = pParent->GetBoundGpuMemory().GpuVirtAddr() + planeOffset;

    if (includeXor)
    {
        imageBaseAddr |= gpusize(GetTileSwizzle(subresId)) << 8;

        if (pParent->IsDepthStencilTarget() &&
            ((pBaseSubResInfo->format.format == ChNumFormat::X32_Float) ||
             (pBaseSubResInfo->format.format == ChNumFormat::X8_Unorm)))
        {
            // Depth images require a minimum 64kB alignment which means the low 16 bits (log2(64kb)) of the address
            // must be zero. The PBX value of depth surfaces -- in the PAL implementation -- is tied to the plane
            // index, and PAL happened to assign "depth" to be plane zero. Therefore, at minimum, the low 16 bits of
            // the address are always zero, although we are getting lucky due to having arbitrarily assigned "depth"
            // a PBX value of zero.
            //
            // In the worst case, the workaround requires that bits [11:8] of the address to be zero. However, that
            // will always be the case. So we assert here to ensure that our arbitrary assignment of depth PBX to zero
            // doesn't change.
            PAL_ASSERT((m_gfxDevice.Settings().waZSurfaceMismatchWithXorSwizzleBits == false) ||
                       ((imageBaseAddr & (0xf << 8)) == 0));
        }
    }

    return imageBaseAddr;
}

// =====================================================================================================================
uint32 Image::GetTileSwizzle(
    SubresId subresId
    ) const
{
    return AddrMgr3::GetTileInfo(m_pParent, subresId)->pipeBankXor;
}

// =====================================================================================================================
uint32 Image::GetHwSwizzleMode(
    const SubResourceInfo* pSubResInfo
    ) const
{
    static_assert(
        (EnumSameVal(Addr3SwizzleMode::ADDR3_256B_2D, SWIZZLE_MODE_ENUM::SW_256B_2D)   &&
         EnumSameVal(Addr3SwizzleMode::ADDR3_4KB_2D, SWIZZLE_MODE_ENUM::SW_4KB_2D)     &&
         EnumSameVal(Addr3SwizzleMode::ADDR3_64KB_2D, SWIZZLE_MODE_ENUM::SW_64KB_2D)   &&
         EnumSameVal(Addr3SwizzleMode::ADDR3_256KB_2D, SWIZZLE_MODE_ENUM::SW_256KB_2D) &&
         EnumSameVal(Addr3SwizzleMode::ADDR3_4KB_3D, SWIZZLE_MODE_ENUM::SW_4KB_3D)     &&
         EnumSameVal(Addr3SwizzleMode::ADDR3_64KB_3D, SWIZZLE_MODE_ENUM::SW_64KB_3D)   &&
         EnumSameVal(Addr3SwizzleMode::ADDR3_256KB_3D, SWIZZLE_MODE_ENUM::SW_256KB_3D) &&
         EnumSameVal(Addr3SwizzleMode::ADDR3_LINEAR, SWIZZLE_MODE_ENUM::SW_LINEAR)),
        "Swizzle mode enumerations don't match between HW and SW!");

    const uint32 swTileMode = GetSwTileMode(pSubResInfo);
    const auto*  pAddrMgr   = static_cast<const AddrMgr3::AddrMgr3*>(m_device.GetAddrMgr());

    return pAddrMgr->GetHwSwizzleMode(static_cast<Addr3SwizzleMode>(swTileMode));
}

// =====================================================================================================================
// If depth or stencil plane has non zero m_hiSZValidLayout values, return it; otherwise return zero layout.
ImageLayout Image::GetHiSZValidLayout(
    const SubresRange& subresRange
    ) const
{
    ImageLayout validLayout = {};

    for (uint32 i = 0; i < subresRange.numPlanes; i++)
    {
        const uint32 plane = subresRange.startSubres.plane + i;

        PAL_ASSERT(plane < MaxNumPlanes);
        if (m_hiSZValidLayout[plane].usages != 0)
        {
            validLayout = m_hiSZValidLayout[plane];
            break;
        }
    }

    return validLayout;
}

// =====================================================================================================================
gpusize Image::GetSubresourceAddr(
    SubresId  subresId
    ) const
{
    return GetPlaneBaseAddr(subresId.plane);
}

// =====================================================================================================================
// Determines if this image supports being cleared or copied with format replacement.
bool Image::IsFormatReplaceable(
    SubresId    subresId,
    ImageLayout layout,
    bool        isDst,
    uint8       disabledChannelMask
    ) const
{
    // The image can only be cleared or copied with format replacement when all channels of the color are being written.
    return (disabledChannelMask == 0);
}

// =====================================================================================================================
// We may need to reset the base level when the block size is larger than the mip chain, e.g:
//              Uncompressed pixels       Compressed block sizes (astc8x8)
//  mip0:       604 x 604                 80 x 80
//  mip1:       302 x 302                 40 x 40
//  mip2:       151 x 151                 20 x 20
//  mip3:        75 x  75                 10 x 10
//  mip4:        37 x  37                  5 x 5
//  mip5:        18 x  18                  2 x 2
// For mip5, if we don't compute the non-BC view, HW will get 2 according to the mip chain.  To fix it, we need call
// Addr3ComputeNonBlockCompressedView for it.
gpusize Image::ComputeNonBlockCompressedView(
    const SubResourceInfo* pBaseSubResInfo,
    const SubResourceInfo* pMipSubResInfo,
    uint32*                pMipLevels,      // Out: Number of mips in the view
    uint32*                pMipId,          // Out: First mip in the view
    Extent3d*              pExtent          // Out: width/height of the first mip in the view
    ) const
{
    gpusize addrWithXor = 0;

    const auto*const       pParent         = Parent();
    const ImageCreateInfo& imageCreateInfo = pParent->GetImageCreateInfo();
    Pal::Device*           pDevice         = pParent->GetDevice();
    const auto*            pTileInfo       = Pal::AddrMgr3::GetTileInfo(pParent, pBaseSubResInfo->subresId);

    ADDR3_COMPUTE_NONBLOCKCOMPRESSEDVIEW_INPUT nbcIn = {};

    nbcIn.size                  = sizeof(nbcIn);
    nbcIn.swizzleMode           = m_finalSwizzleModes[pBaseSubResInfo->subresId.plane];
    nbcIn.resourceType          = Pal::AddrMgr3::AddrMgr3::GetAddrResourceType(imageCreateInfo.imageType);
    nbcIn.format                = Pal::Image::GetAddrFormat(imageCreateInfo.swizzledFormat.format);
    nbcIn.unAlignedDims.width   = pBaseSubResInfo->extentTexels.width;
    nbcIn.unAlignedDims.height  = pBaseSubResInfo->extentTexels.height;
    nbcIn.unAlignedDims.depth   = imageCreateInfo.arraySize;
    nbcIn.numMipLevels          = imageCreateInfo.mipLevels;
    nbcIn.slice                 = pMipSubResInfo->subresId.arraySlice;
    nbcIn.mipId                 = pMipSubResInfo->subresId.mipLevel;
    nbcIn.pipeBankXor           = pTileInfo->pipeBankXor;

    ADDR3_COMPUTE_NONBLOCKCOMPRESSEDVIEW_OUTPUT nbcOut = {};
    nbcOut.size = sizeof(nbcOut);

    ADDR_E_RETURNCODE addrResult = Addr3ComputeNonBlockCompressedView(pDevice->AddrLibHandle(), &nbcIn, &nbcOut);
    PAL_ASSERT(addrResult == ADDR_OK);

    pExtent->width  = nbcOut.unAlignedDims.width;
    pExtent->height = nbcOut.unAlignedDims.height;
    *pMipLevels     = nbcOut.numMipLevels;
    *pMipId         = nbcOut.mipId;

    const gpusize gpuVirtAddress = pParent->GetGpuVirtualAddr() + nbcOut.offset;
    const gpusize pipeBankXor    = nbcOut.pipeBankXor;
    addrWithXor                  = (gpuVirtAddress | (pipeBankXor<< 8)) >> 8;

    return addrWithXor;
}

// =====================================================================================================================
void Image::PadYuvPlanarViewActualExtent(
    SubresId  subresource,
    Extent3d* pActualExtent // In: Original actualExtent of subresource. Out: padded actualExtent
    ) const
{
    PAL_ASSERT(Formats::IsYuvPlanar(m_createInfo.swizzledFormat.format) &&
               (m_createInfo.arraySize  > 1)                            &&
               (m_createInfo.mipLevels == 1));

    // We need to compute the difference in start offsets of two consecutive array slices of whichever plane
    // the view is associated with.
    const SubresId slice0SubRes = { subresource.plane, 0, 0 };
    const SubresId slice1SubRes = { subresource.plane, 0, 1 };

    const SubResourceInfo*const pSlice0Info = Parent()->SubresourceInfo(slice0SubRes);
    const SubResourceInfo*const pSlice1Info = Parent()->SubresourceInfo(slice1SubRes);

    if (AddrMgr3::IsLinearSwizzleMode(GetFinalSwizzleMode(slice0SubRes)))
    {
        const ADDR3_MIP_INFO& mipOutput = GetAddrMipOutput(slice0SubRes);

        // Pad out the height so that the total size of one slice equals m_totalPlaneSize.
        // Because we're affecting the mip/slice padding, we have to use pitchForSlice, not the data pitch.

        // Padding dimensions like this has the side effect of breaking normalized coordinates, so we're
        // only safe because RPM blits (unnormalized) are the only thing that use this path.
        pActualExtent->height =
            static_cast<uint32>(m_totalPlaneSize / mipOutput.pitchForSlice / (pSlice0Info->bitsPerTexel >> 3));
    }
    else
    {
        // Stride between array slices in pixels.
        const gpusize arraySliceStride = (pSlice1Info->offset - pSlice0Info->offset) / (pSlice0Info->bitsPerTexel >> 3);

        // The pseudo actualHeight is the stride between slices in pixels divided by the actualPitch of each row.
        PAL_ASSERT((arraySliceStride % pActualExtent->width) == 0);
        pActualExtent->height = static_cast<uint32>(arraySliceStride / pActualExtent->width);
    }
}

// =====================================================================================================================
// Calculates the byte offset from the start of bound image memory to where each plane physically begins.
void Image::SetupPlaneOffsets(
    uint32 numPlanes,
    bool   isYuvPlanar)
{
    gpusize planeOffset         = 0;
    gpusize maxSliceSize        = 0;
    gpusize sliceAlignFromPitch = 1;

    // Loop through all the planes associated with this surface
    for (uint32  planeIdx = 0; planeIdx < numPlanes; planeIdx++)
    {
        // Record where this plane starts
        m_planeOffset[planeIdx] = planeOffset;

        // Address library output is on a per-plane basis, so the mip / slice info in the sub-res is a don't care.
        const SubresId                           baseSubresId = BaseSubres(planeIdx);
        const ADDR3_COMPUTE_SURFACE_INFO_OUTPUT& addrOutput   = GetAddrOutput(baseSubresId);

        if (isYuvPlanar)
        {
            gpusize sliceDataSize = addrOutput.sliceSize;

            if (AddrMgr3::IsLinearSwizzleMode(GetFinalSwizzleMode(baseSubresId)))
            {
                // Addrlib 9.7 can differentiate between 'pitch for data' and 'pitch for slice', plus also where
                // where the trailing padding is on gfx12. We want to use those values to put the other plane(s) in
                // that padding and (if needed) add extra to ensure that we get a multiple of all pitches.
                sliceDataSize = addrOutput.sliceSizeDensePacked;

                const gpusize pitchForSlice = addrOutput.pitchForSlice;

                // Confusingly, the data layout of linear images on gfx12 is calculated differently from the
                // size between slices (relaxed alignment for pitch). This essentially means there's a big
                // chunk of padding at the end of each slice which makes up the difference for the less aligned data.
                // We can take advantage of this to more densely pack YUV planes together by putting the UV data there
                // and there are other places that assume we do so.
                maxSliceSize = Max(maxSliceSize, static_cast<gpusize>(addrOutput.sliceSize));

                // We make custom slice pitches by increasing the height. Therefore, any final custom slice pitch must
                // be a multiple of all pitches (can't have a height of eg. '10.5').
                // Because we should only ever get different plane pitches from downsampling and bpp differences,
                // the pitches should **always** be equal to the largest. If this assumption fails, it's not a bug
                // but means we might start padding allocations to ridiculous numbers and should reevaluate how we
                // do this.
                sliceAlignFromPitch = Lcm(sliceAlignFromPitch, pitchForSlice);

                PAL_ASSERT(sliceAlignFromPitch == Max(sliceAlignFromPitch, pitchForSlice));
            }

            planeOffset += sliceDataSize;
        }
        else
        {
            // For depth/stencil surfaces, the HW assumes that each plane is stored contiguously, so store the
            // plane-offset to correspond to the size of the entire plane.
            planeOffset += addrOutput.surfSize;
        }
    }

    // Record the address where m_planeOffset starts repeating.
    m_totalPlaneSize = RoundUpToMultiple(Max(planeOffset, maxSliceSize), sliceAlignFromPitch);
}

// =====================================================================================================================
void Image::Addr3FinalizeSubresource(
    SubResourceInfo*  pSubResInfo,
    Addr3SwizzleMode  swizzleMode
    ) const
{
    // In all likelihood, everything does since DCC / compression is no longer something we control directly.
    pSubResInfo->flags.supportMetaDataTexFetch = 1;
}

// =====================================================================================================================
const Image* GetGfx12Image(
    const IImage* pImage)
{
    return static_cast<Pal::Gfx12::Image*>(static_cast<const Pal::Image*>(pImage)->GetGfxImage());
}

// =====================================================================================================================
// Returns the GPU virtual address of the HiSPretests metadata for the specified mip level.
gpusize Image::HiSZStateMetaDataAddr(
    uint32 mipLevel
    ) const
{
    PAL_ASSERT(HasHiSZStateMetaData());

    return Parent()->GetBoundGpuMemory().GpuVirtAddr() +
           m_hiSZStateMetaDataOffset +
           (HiSZStateMetaDataSizePerMip * mipLevel);
}

// =====================================================================================================================
// Returns the offset relative to the bound GPU memory of the HiSPretests metadata for the specified mip level.
gpusize Image::HiSZStateMetaDataOffset(
    uint32 mipLevel
    ) const
{
    PAL_ASSERT(HasHiSZStateMetaData());

    return Parent()->GetBoundGpuMemory().Offset() +
           m_hiSZStateMetaDataOffset +
           (HiSZStateMetaDataSizePerMip * mipLevel);
}

// =====================================================================================================================
// Initializes the size and GPU offset for this Image's HiSPretests metadata.
void Image::InitHiSZStateMetaData(
    ImageMemoryLayout* pGpuMemLayout,
    gpusize*           pGpuMemSize)
{
    constexpr uint32 Alignment = HiSZStateMetaDataSizePerMip;

    m_hiSZStateMetaDataOffset = Pow2Align(*pGpuMemSize, Alignment);

    *pGpuMemSize = (m_hiSZStateMetaDataOffset +
                   (HiSZStateMetaDataSizePerMip * m_createInfo.mipLevels));

    // Update the layout information against the HiSZ state metadata.
    UpdateMetaDataHeaderLayout(pGpuMemLayout, m_hiSZStateMetaDataOffset, Alignment);
}

// =====================================================================================================================
// Builds PM4 commands into the command buffer which will update this Image's meta-data to reflect the updated
// HiSZ state values. Returns the next unused DWORD in pCmdSpace.
uint32* Image::UpdateHiSZStateMetaData(
    const SubresRange& range,
    bool               enable,
    Pm4Predicate       predicate,
    EngineType         engineType,
    uint32*            pCmdSpace
    ) const
{
    PAL_ASSERT(HasHiSZStateMetaData());

    WriteDataInfo writeData = { };
    writeData.engineType = engineType;
    writeData.engineSel  = engine_sel__pfp_write_data__prefetch_parser;
    writeData.dstSel     = dst_sel__pfp_write_data__memory;
    writeData.dstAddr    = HiSZStateMetaDataAddr(range.startSubres.mipLevel);
    writeData.predicate  = predicate;

    uint32 values[MaxImageMipLevels];
    for (uint32 i = 0; i < range.numMips; i++)
    {
        values[i] = enable;
    }

    pCmdSpace += CmdUtil::BuildWriteData(writeData, range.numMips, values, pCmdSpace);

    return pCmdSpace;
}

} // Gfx12
} // Pal
