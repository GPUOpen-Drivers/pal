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

#include "core/addrMgr/addrMgr1/addrMgr1.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx6/gfx6Image.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6FormatInfo.h"
#include "core/hw/gfxip/gfx6/g_gfx6PalSettings.h"
#include "core/platform.h"
#include "palMath.h"
#include "palMetroHash.h"

using namespace Pal::AddrMgr1;
using namespace Util;
using namespace Pal::Formats;
using namespace Pal::Formats::Gfx6;

namespace Pal
{
namespace Gfx6
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
    m_pHtile(nullptr),
    m_pCmask(nullptr),
    m_pFmask(nullptr),
    m_pDcc(nullptr),
    m_dccStateMetaDataOffset(0),
    m_dccStateMetaDataSize(0),
    m_fastClearEliminateMetaDataOffset(0),
    m_fastClearEliminateMetaDataSize(0),
    m_waTcCompatZRangeMetaDataOffset(0),
    m_waTcCompatZRangeMetaDataSizePerMip(0)
{
    memset(&m_layoutToState, 0, sizeof(m_layoutToState));
}

// =====================================================================================================================
Image::~Image()
{
    Pal::GfxImage::Destroy();

    PAL_SAFE_DELETE_ARRAY(m_pHtile, m_device.GetPlatform());
    PAL_SAFE_DELETE_ARRAY(m_pCmask, m_device.GetPlatform());
    PAL_SAFE_DELETE_ARRAY(m_pFmask, m_device.GetPlatform());
    PAL_SAFE_DELETE_ARRAY(m_pDcc, m_device.GetPlatform());
}

// =====================================================================================================================
// Initializes Gfx6/7/8 surface information for AddrLib.
Result Image::Addr1InitSurfaceInfo(
    uint32                           subResIdx,
    ADDR_COMPUTE_SURFACE_INFO_INPUT* pSurfInfo)
{
    const SubResourceInfo& subResInfo = *Parent()->SubresourceInfo(subResIdx);

    if (m_pParent->IsDepthStencil())
    {
        bool tcCompatibleEnabledForResolveDst = false;

        tcCompatibleEnabledForResolveDst =
            (m_pParent->IsResolveDst()                                       &&
             m_pParent->IsAspectValid(ImageAspect::Depth)                    &&
             m_pParent->IsAspectValid(ImageAspect::Stencil)                  &&
             TestAnyFlagSet(TcCompatibleResolveDst, Gfx8TcCompatibleResolveDstDepthAndStencil));

        if ((subResInfo.subresId.aspect == ImageAspect::Depth)                                               &&
            (subResInfo.subresId.mipLevel == 0)                                                              &&
            (pSurfInfo->flags.noStencil == 0)                                                                &&
            (m_pParent->IsShaderReadable() || m_pParent->IsResolveSrc() || tcCompatibleEnabledForResolveDst) &&
            (m_createInfo.usageFlags.noStencilShaderRead == 0))
        {
            // Request Addrlib to use matching tile configs for Z/Stencil planes. The DB on Gfx7+ uses the depth tiling
            // info for writes while shader read uses tile index from depth or stencil plane. Thus, the tile modes must
            // match to avoid corruption or client performing stencil plane tile conversion blt. This flag is only
            // valid on the base mip level, as Addrlib determines the tile modes for the sub mip levels from the base.
            pSurfInfo->flags.matchStencilTileCfg = 1;
        }

        pSurfInfo->flags.compressZ = GetGfx6Settings(m_device).depthCompressEnable;
    }

    Result result = Result::Success;

    if ((subResInfo.subresId.mipLevel == 0) &&
        ((subResInfo.subresId.aspect != ImageAspect::Stencil) || (m_pImageInfo->numPlanes == 1)))
    {
        // If this is the most detailed mip of the 0th aspect plane, we need to determine the tile mode and tile
        // type we want AddrLib to use for this subresource.
        result = ComputeAddrTileMode(subResIdx, &pSurfInfo->tileMode);

        // Default to DISPLAYABLE, allowing AddrLib to override to NON_DISPLAYABLE if necessary. There is no
        // performance impact to choosing DISPLAYABLE over NON_DISPLAYABLE.
        pSurfInfo->tileType = ADDR_DISPLAYABLE;

        // AddrLib overrides the DISPLAYABLE tile type with the THICK tile type, which is not efficient for 3D render
        // targets and UAVs. Use the NON_DISPLAYABLE tile type instead.
        if (((pSurfInfo->tileMode == ADDR_TM_2D_TILED_THICK) && ApplyXthickDccWorkaround(pSurfInfo->tileMode)) ||
            ((m_createInfo.imageType == ImageType::Tex3d)                   &&
             (m_pParent->IsRenderTarget() || m_pParent->IsShaderWritable()) &&
             (Formats::IsMacroPixelPacked(subResInfo.format.format) == false)))
        {
            pSurfInfo->tileType = ADDR_NON_DISPLAYABLE;
        }
        else if (m_pParent->IsDepthStencil())
        {
            // Override to DEPTH_SAMPLE_ORDER for depth/stencil Images.
            pSurfInfo->tileType = ADDR_DEPTH_SAMPLE_ORDER;
        }

        // If the surface is hardware rotated (as queried from the KMD), then the tile type must be set to ROTATED
        // micro-tile. This should only be true for flippable, presentable images.
        if (m_pParent->IsHwRotated())
        {
            pSurfInfo->tileType = ADDR_ROTATED;
            PAL_ASSERT(m_pParent->IsPresentable() && m_pParent->IsFlippable());
        }

        if (m_pImageInfo->internalCreateInfo.flags.useSharedTilingOverrides)
        {
            pSurfInfo->tileType = m_pImageInfo->internalCreateInfo.gfx6.sharedTileType;
        }
    }

    // If we will use DCC, we must set this flag to add padding for CB HW requirements and for any ASIC workarounds.
    // true param assumes resource can be made TC compat since this isn't known for sure until after calling addrlib.
    pSurfInfo->flags.dccCompatible = Gfx6Dcc::UseDccForImage(m_device,
                                                             (*this),
                                                             pSurfInfo->tileMode,
                                                             pSurfInfo->tileType,
                                                             true);

    // NOTE: Setting this even if this surface is never texture fetched uses a little more memory but it will still
    // produce a perfectly usable surface for all cases.
    pSurfInfo->flags.tcCompatible = SupportsMetaDataTextureFetch(pSurfInfo->tileMode,
                                                                 pSurfInfo->tileType,
                                                                 subResInfo.format.format,
                                                                 subResInfo.subresId);

    return result;
}

// =====================================================================================================================
// Finalizes the subresource info and tiling info for a single subresource, based on the results reported by AddrLib.
void Image::Addr1FinalizeSubresource(
    uint32                                  subResIdx,
    SubResourceInfo*                        pSubResInfoList,
    void*                                   pTileInfoList,
    const ADDR_COMPUTE_SURFACE_INFO_OUTPUT& surfInfo)
{
    SubResourceInfo*const    pSubResInfo = (pSubResInfoList + subResIdx);
    AddrMgr1::TileInfo*const pTileInfo   = AddrMgr1::NonConstTileInfo(pTileInfoList, subResIdx);
    const ImageAspect        rsrcAspect  = pSubResInfo->subresId.aspect;

    pTileInfo->tileMode = HwArrayModeFromAddrTileMode(surfInfo.tileMode);
    pTileInfo->tileType = HwMicroTileModeFromAddrTileType(surfInfo.tileType);

    // Gfx6 shouldn't use any of the Gfx7+ tile types!
    PAL_ASSERT((m_device.ChipProperties().gfxLevel > GfxIpLevel::GfxIp6) ||
               ((pTileInfo->tileType != ADDR_SURF_ROTATED_MICRO_TILING__CI__VI) &&
                (pTileInfo->tileType != ADDR_SURF_THICK_MICRO_TILING__CI__VI)));

    if (surfInfo.tcCompatible != 0)
    {
        pSubResInfo->flags.supportMetaDataTexFetch = 1;

        if (rsrcAspect == ImageAspect::Stencil)
        {
            if (m_pParent->IsAspectValid(ImageAspect::Depth))
            {
                // The tile info of the depth and stencil aspects have to match. They will match on Tonga for 1x/2x
                // MSAA images, but not for 4x/8x or on Iceland. There are several ways around this:
                //   1) The compressed depth-surface remains readable by the texture pipe. If the app tries to read the
                //      stencil aspect (very rare?), then decompress both aspects. This should be do-able, although the
                //      image will be in a state that is different from what the app thinks it's in.
                //   2) Generate a temporary stencil surface, decompress the stencil aspect to the temp surface, and
                //      point the texture pipe at the temp surface. This is the DXX solution, but it's difficult to
                //      efficiently implement in PAL.
                //   3) Don't allow texture compatability of either aspect. This will force a depth expand before any
                //      texture reads (i.e., pre-Gfx8 behavior). This is the current implementation.

                SubresId zPlaneSubResId = pSubResInfo->subresId;
                zPlaneSubResId.aspect   = ImageAspect::Depth;

                // Only enable TC compatibility for this (stencil) aspect if it is enabled for depth and the tile info
                // for both aspects match. Separate depth init requires disabling stencil compression if separate aspect
                // metadata init is not enabled, so TC compatibility does not apply in this situation.
                pSubResInfo->flags.supportMetaDataTexFetch =
                                    (DoesTileInfoMatch(pSubResInfo->subresId) &&
                                     ((m_pParent->GetImageCreateInfo().flags.separateDepthAspectInit == 0) ||
                                      (GetGfx6Settings(m_device).enableSeparateAspectMetadataInit)) &&
                                     (m_pParent->SubresourceInfo(zPlaneSubResId)->flags.supportMetaDataTexFetch != 0));
            }
        }
        else if (rsrcAspect == ImageAspect::Color)
        {
            if (ColorImageSupportsMetaDataTextureFetch(surfInfo.tileMode, surfInfo.tileType) == false)
            {
                // AddrLib may have given us a micro-tiling mode which is incompatible with DCC. If this occurs, we
                // must disable TC compatibility for the subresource, too.
                PAL_ALERT_ALWAYS();
                pSubResInfo->flags.supportMetaDataTexFetch = 0;
            }
        }
    }

    // We must set supportMetaDataTexFetch before calling this function.
    SetupBankAndPipeSwizzle(subResIdx, pTileInfoList, surfInfo);

    if (m_device.ChipProperties().gfxLevel == GfxIpLevel::GfxIp6)
    {
        if (pSubResInfo->subresId.mipLevel == 0)
        {
            pTileInfo->childMipsNeedPrtTileIndex = (surfInfo.prtTileIndex != 0);
        }

        // Apply the workaround for the Swizzled Mip-Map bug: the workaround is to pad the last 2D mip level of the
        // last array slice in any mipmapped 2D image.
        //
        // Normally a workaround like this would be tied to a setting, but this workaround must be enabled
        // before settings have been committed so we simply enable it for all GfxIp6 devices.
        if ((m_createInfo.mipLevels > 1) &&
            (pSubResInfo->subresId.arraySlice == (m_createInfo.arraySize - 1)) &&
            (surfInfo.last2DLevel == 1))
        {
            uint32 baseSwizzle = 0;
            if ((pSubResInfo->subresId.mipLevel == 0) && (pSubResInfo->subresId.arraySlice == 0))
            {
                baseSwizzle = pTileInfo->tileSwizzle;
            }
            else
            {
                const SubresId   baseSubres    = { rsrcAspect, 0, 0 };
                const auto*const pBaseTileInfo = AddrMgr1::GetTileInfo(Parent(), baseSubres);

                baseSwizzle = pBaseTileInfo->tileSwizzle;
            }

            // The number of bytes to pad before the first 1D mip on downgrade:
            pSubResInfo->size += (baseSwizzle * 256);
        }
    }
}

// =====================================================================================================================
// "Finalizes" this Image object: this includes determining what metadata surfaces need to be used for this Image, and
// initializing the data structures for them.
Result Image::Finalize(
    bool               dccUnsupported,
    SubResourceInfo*   pSubResInfoList,
    void*              pTileInfoList, // Not used in Gfx6 version
    ImageMemoryLayout* pGpuMemLayout,
    gpusize*           pGpuMemSize,
    gpusize*           pGpuMemAlignment)
{
    const auto*const               pPublicSettings   = m_device.GetPublicSettings();
    const SubResourceInfo*const    pBaseSubResInfo   = Parent()->SubresourceInfo(0);
    const AddrMgr1::TileInfo*const pBaseTileInfo     = AddrMgr1::GetTileInfo(Parent(), 0);
    const SharedMetadataInfo&      sharedMetadata    = m_pImageInfo->internalCreateInfo.sharedMetadata;
    const bool                     useSharedMetadata = m_pImageInfo->internalCreateInfo.flags.useSharedMetadata;

    bool useDcc   = false;
    bool useHtile = false;
    bool useCmask = false;
    bool useFmask = false;

    Result result = Result::Success;

    if (useSharedMetadata)
    {
        useDcc   = (sharedMetadata.dccOffset != 0);
        useHtile = (sharedMetadata.htileOffset != 0);
        useCmask = (sharedMetadata.cmaskOffset != 0);
        useFmask = (sharedMetadata.fmaskOffset != 0);

        // Fast-clear metadata is a must for shared DCC and HTILE. Sharing is disabled if it is not provided.
        if (useDcc && (sharedMetadata.fastClearMetaDataOffset == 0))
        {
            useDcc = false;
            result = Result::ErrorNotShareable;
        }

        if (useHtile && (sharedMetadata.fastClearMetaDataOffset == 0))
        {
            useHtile = false;
            result = Result::ErrorNotShareable;
        }
    }
    else
    {
        // Determine which Mask RAM objects are required for this Image (if any).
        useDcc   = ((dccUnsupported == false) &&
                    Gfx6Dcc::UseDccForImage(m_device,
                                            *this,
                                            AddrMgr1::AddrTileModeFromHwArrayMode(pBaseTileInfo->tileMode),
                                            AddrMgr1::AddrTileTypeFromHwMicroTileMode(pBaseTileInfo->tileType),
                                            pBaseSubResInfo->flags.supportMetaDataTexFetch));
        useHtile = Gfx6Htile::UseHtileForImage(m_device, *this, pBaseSubResInfo->flags.supportMetaDataTexFetch);
        useCmask = Gfx6Cmask::UseCmaskForImage(m_device, *this, useDcc);
        useFmask = Gfx6Fmask::UseFmaskForImage(m_device, *this);
    }

    // Also determine if we need any metadata for these mask RAM objects.
    bool needsFastColorClearMetaData   = false;
    bool needsFastDepthClearMetaData   = false;
    bool needsDccStateMetaData         = false;
    bool needsWaTcCompatZRangeMetaData = false;

    // Start out by assuming we can decompress any TC-compatible subresource using compute Queues. This may be
    // overridden later.
    bool allowComputeDecompress = (pBaseSubResInfo->flags.supportMetaDataTexFetch != 0);

    // Initialize DCC:
    if (useDcc)
    {
        m_pDcc = PAL_NEW_ARRAY(Gfx6Dcc, m_createInfo.mipLevels, m_device.GetPlatform(), SystemAllocType::AllocObject);
        if (m_pDcc != nullptr)
        {
            gpusize mipMemOffset   = 0;
            gpusize totalMemOffset = 0;

            // Store current memory offset
            if (useSharedMetadata)
            {
                mipMemOffset      = sharedMetadata.dccOffset;
                totalMemOffset    = sharedMetadata.dccOffset;
            }
            else
            {
                mipMemOffset      = *pGpuMemSize;
                totalMemOffset    = *pGpuMemSize;
            }
            gpusize totalDccSizeAvail = 0;

            // First calculate the total DCC memory needed by this mip chain.
            result = Gfx6Dcc::InitTotal(m_device, *this, pGpuMemLayout->dataSize, &totalMemOffset, &totalDccSizeAvail);
            PAL_ASSERT((result == Result::Success) && (totalDccSizeAvail > 0));

            // First mip-level should always use DCC...  all other levels are open for debate.
            bool mipLevelShouldUseDcc = true;

            for (uint32 mip = 0; ((mip < m_createInfo.mipLevels) && (result == Result::Success)); mip++)
            {
                // Check if client is requesting pal to enable dcc on only select mips.
                if ((m_createInfo.usageFlags.shaderWrite) &&
                    (mip >= m_createInfo.usageFlags.firstShaderWritableMip))
                {
                    // if we have a mip chain in which some mips are not going to be used as UAV but some can be
                    // then we enable dcc on those who are not going to be used as UAV and disable dcc on others.
                    mipLevelShouldUseDcc = false;
                }

                result = m_pDcc[mip].Init(m_device,
                                          *this,
                                          mip,
                                          &totalDccSizeAvail,
                                          &mipMemOffset,
                                          &mipLevelShouldUseDcc);

                if ((result == Result::Success) &&
                    // Does this DCC memory support a fast clear?  If the settings has disabled fast-clear support, then
                    // DCC memory would have been disabled as well. i..e., we wouldn't be here.
                    m_pDcc[mip].UseFastClear())
                {
                    UpdateClearMethod(pSubResInfoList, ImageAspect::Color, mip, ClearMethod::Fast);
                }

                // Offset and size calculation is done. All mips left have zero size DCC memory.
                if (totalDccSizeAvail == 0)
                {
                    for (uint32 remainingMip = mip + 1; remainingMip < m_createInfo.mipLevels; remainingMip++)
                    {
                        m_pDcc[remainingMip].SetEnableCompression(0);
                    }
                    break;
                }

                // For the compute-based DCC decompress option to work then all levels which are compressible must
                // also be TC compatible.
                const SubresId localSubResId = { ImageAspect::Color, mip, 0 };
                if (m_pDcc[mip].IsCompressionEnabled() &&
                    (Parent()->SubresourceInfo(localSubResId)->flags.supportMetaDataTexFetch == 0))
                {
                    allowComputeDecompress = false;
                }
            }

            if (result == Result::Success)
            {
                // To support independent initialization of a subresource, that subresource must be contiguous because
                // the initialization operation is simply a memset to an expanded/decompressed state. Thus
                // we must disable DCC for all subresources that do not have a contiguous memory.
                //
                // To make matters worse, if DCC is enabled for some subresources it must be initialized on all
                // subresources even if it always disabled. Otherwise we might end up with corruption when the TC does a
                // sample looking at two mip levels, one with valid DCC keys and one with invalid DCC keys. Given that
                // we cannot even initialize DCC on subresources that are not contiguous in memory, we must disable DCC
                // for the entire image in this case.
                //
                // Note that if some mip level is not contiguous then neither are any smaller mip levels so we can just
                // check the last mip level for contiguous memory.
                if ((m_createInfo.flags.perSubresInit != 0) &&
                    (m_pDcc[m_createInfo.mipLevels - 1].ContiguousSubresMem() == false))
                {
                    // Reset to the default clear method and clear the metadata TC fetch flag.
                    for (uint32 idx = 0; idx < m_pImageInfo->numSubresources; ++idx)
                    {
                        pSubResInfoList[idx].clearMethod                   = Pal::Image::DefaultSlowClearMethod;
                        pSubResInfoList[idx].flags.supportMetaDataTexFetch = 0;
                    }

                    // Clean up the DCC objects.
                    PAL_SAFE_DELETE_ARRAY(m_pDcc, m_device.GetPlatform());
                }
                else
                {
                    // Set up the size & GPU offset for the fast-clear metadata.  Only need to do this once for all mip
                    // levels. The HW will only use this data if fast-clears have been used, but the fast-clear metadata
                    // is used by the driver if DCC memory is present for any reason, so we always need to do this.
                    // SEE: Gfx6ColorTargetView::WriteCommands for details.
                    needsFastColorClearMetaData = true;

                    // We also need the DCC state metadata when DCC is enabled.
                    needsDccStateMetaData = useSharedMetadata ?
                                            (sharedMetadata.dccStateMetaDataOffset != 0) : true;

                    // The total DCC memory offset equals the current size of this image's GPU memory.
                    *pGpuMemSize = useSharedMetadata ? Max(totalMemOffset, *pGpuMemSize) : totalMemOffset;

                    // It's possible for the metadata allocation to require more alignment than the base allocation.
                    // Bump up the required alignment of the app-provided allocation if necessary.
                    *pGpuMemAlignment = Max(*pGpuMemAlignment, m_pDcc[0].Alignment());

                    // Update the layout information against mip 0's DCC offset and alignment requirements.
                    UpdateMetaDataLayout(pGpuMemLayout, m_pDcc[0].MemoryOffset(), m_pDcc[0].Alignment());
                }
            }
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    } // End check for (useDcc != false)

    // Initialize Htile:
    if (useHtile && (result == Result::Success))
    {
        m_pHtile = PAL_NEW_ARRAY(Gfx6Htile,
                                 m_createInfo.mipLevels,
                                 m_device.GetPlatform(),
                                 SystemAllocType::AllocObject);
        if (m_pHtile != nullptr)
        {
            const bool supportsDepth   = m_device.SupportsDepth(m_createInfo.swizzledFormat.format,
                                                                m_createInfo.tiling);
            const bool supportsStencil = m_device.SupportsStencil(m_createInfo.swizzledFormat.format,
                                                                  m_createInfo.tiling);

            gpusize memOffset = useSharedMetadata ? sharedMetadata.htileOffset : *pGpuMemSize;

            uint32 interleavedMipLevel = m_createInfo.mipLevels;
            bool mipSlicesInterleaved  = false;
            for (uint32 mip = 0; ((mip < m_createInfo.mipLevels) && (result == Result::Success)); ++mip)
            {
                if (mip > interleavedMipLevel)
                {
                    // If mipInterleave exists, following mipLevels are not allowed to be tc-compatible, since texture
                    // engine read might reference the htile interleaved in previous mip level while db rendering
                    // references subRes's own htile. And supportMetaDataTexFetch has to be set to 0 before htile of
                    // cur miplevel initialization. Overriding tc-compatible code is going to be moved into addrLib,
                    // but it's required before addrLib ready.
                    SubresId subResId = {};
                    subResId.mipLevel = mip;

                    for (uint32 slice = 0; slice < m_createInfo.arraySize; ++slice)
                    {
                        subResId.arraySlice = slice;
                        if (supportsDepth)
                        {
                            subResId.aspect = ImageAspect::Depth;
                            pSubResInfoList[Parent()->CalcSubresourceId(subResId)].flags.supportMetaDataTexFetch = 0;
                        }

                        if (supportsStencil)
                        {
                            subResId.aspect = ImageAspect::Stencil;
                            pSubResInfoList[Parent()->CalcSubresourceId(subResId)].flags.supportMetaDataTexFetch = 0;
                        }
                    }
                }

                result = m_pHtile[mip].Init(m_device, *this, mip, &memOffset);

                if (result == Result::Success)
                {
                    // For now, if any of the mips have interleaved slices, force clears to graphics for all mips.
                    mipSlicesInterleaved = (mipSlicesInterleaved || m_pHtile[mip].SlicesInterleaved());

                    if (m_pHtile[mip].FirstInterleavedMip())
                    {
                        // Maximum to one 'FirstInterleavedMip' might exsit.
                        PAL_ASSERT(interleavedMipLevel == m_createInfo.mipLevels);
                        interleavedMipLevel = mip;
                    }

                    // Our compute-based hTile expand option can only operate on one aspect (depth or stencil) at a
                    // time, but it will overwrite hTile data for both aspects once it's done.  :-(  So we can only
                    // use the compute path for images with a single aspect.
                    if (supportsDepth ^ supportsStencil)
                    {
                        const auto      aspect   = (supportsDepth ? ImageAspect::Depth : ImageAspect::Stencil);
                        const SubresId  subResId = { aspect, mip, 0 };
                        if (Parent()->SubresourceInfo(subResId)->flags.supportMetaDataTexFetch == false)
                        {
                            allowComputeDecompress = false;
                        }
                    }
                    else
                    {
                        allowComputeDecompress = false;
                    }

                    // Set up the GPU offset for the waTcCompatZRange metadata
                    needsWaTcCompatZRangeMetaData = (m_device.GetGfxDevice()->WaTcCompatZRange()   &&
                                                     (pBaseSubResInfo->flags.supportMetaDataTexFetch != 0));

                    if (useSharedMetadata          &&
                        needsWaTcCompatZRangeMetaData &&
                        (sharedMetadata.flags.hasWaTcCompatZRange == false))
                    {
                        result = Result::ErrorNotShareable;
                    }
                }
            }

            if (result == Result::Success)
            {
                // To support independent initialization of a subresource, that subresource must be contiguous because
                // the initialization operation is simply a memset to an expanded/decompressed state. Thus we must
                // disable HTile for all subresources that do not have a contiguous memory.
                //
                // To make matters worse, if HTile is created for some subresources it must be initialized on all
                // subresources even if it always disabled. Otherwise we might end up with corruption when the TC does a
                // sample looking at two mip levels, one with valid HTile and one with invalid HTile. Given that
                // we cannot even initialize HTile on subresources that are not contiguous in memory, we must disable
                // HTile for the entire image in this case.
                if ((m_createInfo.flags.perSubresInit != 0) && mipSlicesInterleaved)
                {
                    // Clear the metadata TC fetch flag.
                    for (uint32 idx = 0; idx < m_pImageInfo->numSubresources; ++idx)
                    {
                        pSubResInfoList[idx].flags.supportMetaDataTexFetch = 0;
                    }

                    // Clean up the HTile objects.
                    PAL_SAFE_DELETE_ARRAY(m_pHtile, m_device.GetPlatform());
                }
                else
                {
                    // Depth subresources with hTile memory must be fast-cleared either through the compute or graphics
                    // engine. Slow clears won't work as the hTile memory wouldn't get updated. If a mip level has
                    // interleaved slices, graphics engine must be used to clear.
                    const ClearMethod fastClearMethod = (pPublicSettings->useGraphicsFastDepthStencilClear ||
                                                         mipSlicesInterleaved) ?
                                                            ClearMethod::DepthFastGraphics :
                                                            ClearMethod::Fast;

                    for (uint32 mip = 0; mip < m_createInfo.mipLevels; ++mip)
                    {
                        // If mipInterleave exists, the first affected mip is not allowed to perform ClearMethod::Fast,
                        // since ClearMethod::Fast might affect child mip htiles.
                        const ClearMethod curMipFastClearMethod =
                            (mip == interleavedMipLevel) ? ClearMethod::DepthFastGraphics : fastClearMethod;

                        if (supportsDepth)
                        {
                            UpdateClearMethod(pSubResInfoList, ImageAspect::Depth, mip, curMipFastClearMethod);
                        }

                        if (supportsStencil)
                        {
                            UpdateClearMethod(pSubResInfoList, ImageAspect::Stencil, mip, curMipFastClearMethod);
                        }
                    }

                    needsFastDepthClearMetaData = true;

                    *pGpuMemSize = useSharedMetadata ? Max(memOffset, *pGpuMemSize) : memOffset;

                    // It's possible for the metadata allocation to require more alignment than the base allocation.
                    // Bump up the required alignment of the app-provided allocation if necessary.
                    *pGpuMemAlignment = Max(*pGpuMemAlignment, m_pHtile[0].Alignment());

                    // Update the layout information against mip 0's Htile offset and alignment requirements.
                    UpdateMetaDataLayout(pGpuMemLayout, m_pHtile[0].MemoryOffset(), m_pHtile[0].Alignment());
                }
            }
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    } // End check for (useHtile != false)

    // Initialize Cmask:
    if (useCmask && (result == Result::Success))
    {
        m_pCmask = PAL_NEW_ARRAY(Gfx6Cmask,
                                 m_createInfo.mipLevels,
                                 m_device.GetPlatform(),
                                 SystemAllocType::AllocObject);
        if (m_pCmask != nullptr)
        {
            gpusize memOffset = useSharedMetadata ? sharedMetadata.cmaskOffset : *pGpuMemSize;

            for (uint32 mip = 0; ((mip < m_createInfo.mipLevels) && (result == Result::Success)); ++mip)
            {
                result = m_pCmask[mip].Init(m_device, *this, mip, &memOffset);

                if ((result == Result::Success) && m_pCmask[mip].UseFastClear())
                {
                    // NOTE: Fast clear is not completely controlled by the presence of CMask, because MSAA Images
                    // require CMask but might not have fast-clears enabled.
                    UpdateClearMethod(pSubResInfoList, ImageAspect::Color, mip, ClearMethod::Fast);
                }
            }

            if (m_pCmask[0].UseFastClear())
            {
                needsFastColorClearMetaData = true;
            }

            *pGpuMemSize = useSharedMetadata ? Max(memOffset, *pGpuMemSize) : memOffset;

            // It's possible for the metadata allocation to require more alignment than the base allocation. Bump up the
            // required alignment of the app-provided allocation if necessary.
            *pGpuMemAlignment = Max(*pGpuMemAlignment, m_pCmask[0].Alignment());

            // Update the layout information against mip 0's Cmask offset and alignment requirements.
            UpdateMetaDataLayout(pGpuMemLayout, m_pCmask[0].MemoryOffset(), m_pCmask[0].Alignment());
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    } // End check for (useCmask != false)

    // Initialize Fmask:
    if (useFmask && (result == Result::Success))
    {
        m_pFmask = PAL_NEW_ARRAY(Gfx6Fmask,
                                 m_createInfo.mipLevels,
                                 m_device.GetPlatform(),
                                 SystemAllocType::AllocObject);
        if (m_pFmask != nullptr)
        {
            gpusize memOffset = useSharedMetadata ? sharedMetadata.fmaskOffset : *pGpuMemSize;

            for (uint32 mip = 0; mip < m_createInfo.mipLevels; ++mip)
            {
                result = m_pFmask[mip].Init(m_device, *this, mip, &memOffset);
                if (result != Result::Success)
                {
                    break;
                }
            }

            if ((m_createInfo.flags.repetitiveResolve != 0) || (m_device.Settings().forceFixedFuncColorResolve != 0))
            {
                // According to the CB Micro-Architecture Specification, it is illegal to resolve a 1 fragment eqaa
                // surface.
                if ((Parent()->IsEqaa() == false) || (m_createInfo.fragments > 1))
                {
                    m_pImageInfo->resolveMethod.fixedFunc = 1;
                }
            }

            // NOTE: If FMask is present, use the FMask-accelerated resolve path.
            m_pImageInfo->resolveMethod.shaderCsFmask = 1;

            // It's possible for the metadata allocation to require more alignment than the base allocation. Bump up the
            // required alignment of the app-provided allocation if necessary.
            *pGpuMemAlignment = Max(*pGpuMemAlignment, m_pFmask[0].Alignment());

            *pGpuMemSize = useSharedMetadata ? Max(memOffset, *pGpuMemSize) : memOffset;

            // Update the layout information against mip 0's Fmask offset and alignment requirements.
            UpdateMetaDataLayout(pGpuMemLayout, m_pFmask[0].MemoryOffset(), m_pFmask[0].Alignment());
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    } // End check for (useFmask != false)

    if (result == Result::Success)
    {
        // If we have a valid metadata offset we also need a metadata size.
        if (pGpuMemLayout->metadataOffset != 0)
        {
            pGpuMemLayout->metadataSize = (*pGpuMemSize - pGpuMemLayout->metadataOffset);
        }

        // Set up the size & GPU offset for the fast-clear metadata. An image can't have color metadata and depth-
        // stencil metadata.
        if (needsFastColorClearMetaData)
        {
            if (useSharedMetadata)
            {
                gpusize forcedOffset = sharedMetadata.fastClearMetaDataOffset;
                InitFastClearMetaData(pGpuMemLayout, &forcedOffset, sizeof(Gfx6FastColorClearMetaData), sizeof(uint32));
                *pGpuMemSize = Max(forcedOffset, *pGpuMemSize);
            }
            else
            {
                InitFastClearMetaData(pGpuMemLayout, pGpuMemSize, sizeof(Gfx6FastColorClearMetaData), sizeof(uint32));
            }
        }
        else if (needsFastDepthClearMetaData)
        {
            if (useSharedMetadata)
            {
                gpusize forcedOffset = sharedMetadata.fastClearMetaDataOffset;
                InitFastClearMetaData(pGpuMemLayout, &forcedOffset, sizeof(Gfx6FastDepthClearMetaData), sizeof(uint32));
                *pGpuMemSize = Max(forcedOffset, *pGpuMemSize);
            }
            else
            {
                InitFastClearMetaData(pGpuMemLayout, pGpuMemSize, sizeof(Gfx6FastDepthClearMetaData), sizeof(uint32));
            }
        }

        // Set up the GPU offset for the waTcCompatZRange metadata
        if (needsWaTcCompatZRangeMetaData)
        {
            InitWaTcCompatZRangeMetaData(pGpuMemLayout, pGpuMemSize);
        }

        // Set up the GPU offset for the DCC state metadata.
        if (needsDccStateMetaData)
        {
            if (useSharedMetadata)
            {
                gpusize forcedOffset = sharedMetadata.dccStateMetaDataOffset;
                InitDccStateMetaData(pGpuMemLayout, &forcedOffset);
                *pGpuMemSize = Max(*pGpuMemSize, forcedOffset);
            }
            else
            {
                InitDccStateMetaData(pGpuMemLayout, pGpuMemSize);
            }
        }

        // Texture-compatible color images on VI can only be fast-cleared to certain colors; otherwise the TC won't
        // understand the color data.  For non-supported fast-clear colors, we can either
        //    a) do a slow-clear of the image
        //    b) fast-clear the image anyway and do a fast-clear-eliminate pass when the image is bound as a texture.
        //
        // So, if all these conditions are true:
        //    a) This image supports fast-clears in the first place
        //    b) This is a color image
        //    c) We always fast-clear regardless of the clear-color (meaning a fast-clear eliminate will be required)
        //    d) This image is going to be used as a texture
        //
        // Then setup memory to be used to conditionally-execute the fast-clear-eliminate pass based on the clear-color.
        if (needsFastColorClearMetaData           &&
            (Parent()->IsDepthStencil() == false) &&
            ColorImageSupportsAllFastClears()     &&
            (pBaseSubResInfo->flags.supportMetaDataTexFetch != 0))
        {
            if (useSharedMetadata)
            {
                if (sharedMetadata.fastClearEliminateMetaDataOffset != 0)
                {
                    gpusize forcedOffset = sharedMetadata.fastClearEliminateMetaDataOffset;
                    InitFastClearEliminateMetaData(pGpuMemLayout, &forcedOffset);
                    *pGpuMemSize = Max(forcedOffset, *pGpuMemSize);
                }
            }
            else
            {
                InitFastClearEliminateMetaData(pGpuMemLayout, pGpuMemSize);
            }
        }

        // NOTE: We're done adding bits of GPU memory to our image; its GPU memory size is now final.

        // If we have a valid metadata header offset we also need a metadata header size.
        if (pGpuMemLayout->metadataHeaderOffset != 0)
        {
            pGpuMemLayout->metadataHeaderSize = (*pGpuMemSize - pGpuMemLayout->metadataHeaderOffset);
        }

        InitLayoutStateMasks(allowComputeDecompress);

        if (m_createInfo.flags.prt != 0)
        {
            m_device.GetAddrMgr()->ComputePackedMipInfo(*Parent(), pGpuMemLayout);
        }
    }

    return result;
}

// =====================================================================================================================
// Initializes the layout-to-state masks which are used by Device::Barrier() to determine which operations are needed
// when transitioning between different Image layouts.
void Image::InitLayoutStateMasks(
    bool allowComputeDecompress)
{
    const uint32 mipLevel = Parent()->GetImageCreateInfo().mipLevels;
    SubresId     subresId = Parent()->SubresourceInfo(0)->subresId;

    for (uint32 i = 0; i < mipLevel; i++)
    {
        subresId.mipLevel = i;
        InitLayoutStateMasksOneMip(allowComputeDecompress, subresId);
    }
}

// =====================================================================================================================
// Initializes the layout-to-state masks for one mip level.
void Image::InitLayoutStateMasksOneMip(
    bool            allowComputeDecompress,
    const SubresId& subresId)
{
    const SubResourceInfo*const pSubResInfo                = Parent()->SubresourceInfo(subresId);
    const bool                  isMsaa                     = (m_createInfo.samples > 1);
    const bool                  isComprFmaskShaderReadable = IsComprFmaskShaderReadable(pSubResInfo);
    const uint32                mip                        = subresId.mipLevel;

    if (HasColorMetaData())
    {
        PAL_ASSERT(Parent()->IsDepthStencil() == false);

        // Always allow compression for layouts that only support the color target usage.
        m_layoutToState[mip].color.compressed.usages  = LayoutColorTarget;
        m_layoutToState[mip].color.compressed.engines = LayoutUniversalEngine;

        if (allowComputeDecompress &&
            TestAnyFlagSet(UseComputeExpand, (isMsaa ? UseComputeExpandMsaaDcc : UseComputeExpandDcc)))
        {
            m_layoutToState[mip].color.compressed.engines |= LayoutComputeEngine;
        }

        // On GFX8 hardware, additional usages may be allowed for an image in the compressed state.
        if (pSubResInfo->flags.supportMetaDataTexFetch)
        {
            if (isMsaa)
            {
                // Our fmask surface must be in a tc-compatible state
                PAL_ASSERT(isComprFmaskShaderReadable == true);

                // Resolve can take 3 different paths inside pal:-
                // a. FixedFuncHWResolve :- in this case since CB does all the work we can keep everything compressed.
                // b. ShaderBasedResolve (when format match/native resolve):- We can keep entire color compressed.
                // c. ShaderBasedResolve (when format don't match) :- In this case we won't end up here since pal won't
                // allow any DCC surface and hence tc-compatibility flag supportMetaDataTexFetch will be 0.
                // conclusion:- We can keep it compressed in all cases.
                m_layoutToState[mip].color.compressed.usages |= LayoutResolveSrc;

                // As stated above we only land up here if dcc is allocated and we are tc-compatible and also in this
                // case on gfxip8 we will have fmask surface tc-compatible, which means we can keep colorcompressed
                // for fmaskbased msaaread.
                m_layoutToState[mip].color.compressed.usages |= LayoutShaderFmaskBasedRead;
            }
            else
            {
                // Our copy path has been designed to allow compressed copy sources.
                m_layoutToState[mip].color.compressed.usages |= LayoutCopySrc;

                // You can't raw copy to a compressed texture, you can only write to it using the image's format.
                // Add in LayoutCopyDst if the client promises that all copies will only write using the image's
                // format.
                if (m_createInfo.flags.copyFormatsMatch != 0)
                {
                    m_layoutToState[mip].color.compressed.usages |= LayoutCopyDst;
                }

                // We can keep this layout compressed if all view formats are DCC compatible.
                if (Parent()->GetDccFormatEncoding() != DccFormatEncoding::Incompatible)
                {
                    m_layoutToState[mip].color.compressed.usages |= LayoutShaderRead;
                }
            }
        }
        else if (isMsaa && isComprFmaskShaderReadable)
        {
            // We can't be tc-compatible here
            PAL_ASSERT(pSubResInfo->flags.supportMetaDataTexFetch == 0);

            // Also since we can't be tc-compatible we must not have dcc data
            // isComprFmaskShaderReadable flag ensures that.
            PAL_ASSERT(HasDccData() == false);

            // Resolve can take 3 different paths inside pal:-
            // a. FixedFuncHWResolve :- in this case since CB does all the work we can keep everything compressed.
            // b. ShaderBasedResolve (when format match/native resolve):- We can keep entire color compressed.
            // c. ShaderBasedResolve (when format don't match) :- since we have no dcc surface for such resources
            // and fmask itself is in tc-compatible state, it is safe for us to keep it colorcompressed. unless
            // we have a dcc surface but we are not tc-compatible in that case we can't remain color compressed
            // conclusion :- In this case it is safe for us to keep entire color compressed except one case as
            // identified above. We only make fmask tc-compatible when we can keep entire color surface compressed.
            m_layoutToState[mip].color.compressed.usages |= LayoutResolveSrc;

            // The only case it won't work if DCC is allocated and yet this surface is not tc-compatible, if dcc
            // was never allocated then we can keep entire image color compressed (isComprFmaskShaderReadable takes
            // care of it).
            m_layoutToState[mip].color.compressed.usages |= LayoutShaderFmaskBasedRead;
        }

        // The fmask-decompressed state is only valid for MSAA images. This state implies that the base color data
        // is still compressed, but fmask is expanded so that it is readable by the texture unit even if metadata
        // texture fetches are not supported.
        if (isMsaa)
        {
            // Postpone all decompresses for the ResolveSrc state from Barrier-time to Resolve-time.
            m_layoutToState[mip].color.compressed.usages |= LayoutResolveSrc;

            // Our copy path has been designed to allow color compressed MSAA copy sources.
            m_layoutToState[mip].color.fmaskDecompressed.usages = LayoutColorTarget | LayoutCopySrc;

            // Resolve can take 3 different paths inside pal:-
            // a. FixedFuncHWResolve :- in this case since CB does all the work we can keep everything compressed.
            // b. ShaderBasedResolve (when format match/native resolve):- We can keep entire color compressed and
            // hence also in fmaskdecompressed state. If we have a DCC surface but no tc-compatibility even that
            // case is not a problem since at barrier time we will issue a dccdecompress
            // c. ShaderBasedResolve (when format don't match) :- we won't have dcc surface in this case and hence
            //  it is completely fine to keep color into fmaskdecompressed state.
            m_layoutToState[mip].color.fmaskDecompressed.usages |= LayoutResolveSrc;

            // We can keep this resource into Fmaskcompressed state since barrier will handle any corresponding
            // decompress for cases when dcc is present and we are not tc-compatible.
            m_layoutToState[mip].color.fmaskDecompressed.usages |= LayoutShaderFmaskBasedRead;

            m_layoutToState[mip].color.fmaskDecompressed.engines = LayoutUniversalEngine | LayoutComputeEngine;
        }
    } // End check for HasColorMetadata()
    else if (m_pHtile != nullptr)
    {
        PAL_ASSERT(Parent()->IsDepthStencil());

        // Identify usages supporting DB rendering
        constexpr uint32 DbUsages = LayoutDepthStencilTarget;

        // Identify the supported shader readable usages.
        // Depth stencil resolve have two potential paths:
        // 1. Fixed-func depth-stencil copy resolve.
        // 2. Pixel-shader resolve.
        // Path#1 could keep resolveSrc compressed but Path#2 require resolveSrc to be in decompressed state on
        // non-TC-compatible asics. We have no idea which path will be selected, so resolveSrc is also referred
        // as shader read usages.
        constexpr uint32 ShaderReadUsages = LayoutCopySrc | LayoutResolveSrc | LayoutShaderRead;

        // Layouts that are decompressed (with hiz enabled) support both depth rendering and shader reads (though
        // not shader writes) in the universal queue and compute queue.
        // For resolve dst, HiZ is always valid whatever depth-stencil copy resolve or pixel shader resolve performed:
        // 1. Htile copy-and-fix-up will be performed after depth-stencil copy resolve to ensure HiZ to be valid.
        // 2. Htile is valid during pixel shader resolve.
        ImageLayout decomprWithHiZ;

        decomprWithHiZ.usages  = DbUsages | ShaderReadUsages | LayoutResolveDst;
        decomprWithHiZ.engines = LayoutUniversalEngine | LayoutComputeEngine;

        // If the client has given us a hint that this Image never does anything to this Image which would cause
        // the Image data and Hi-Z to become out-of-sync, we can include all layouts in the decomprWithHiZ state
        // because this Image will never need to do a resummarization blit.
        if (m_createInfo.usageFlags.hiZNeverInvalid != 0)
        {
            decomprWithHiZ.usages  = AllDepthImageLayoutFlags;
            decomprWithHiZ.engines = LayoutUniversalEngine | LayoutComputeEngine | LayoutDmaEngine;
        }

        // Layouts that are compressed support all DB compatible usages in the universal queue
        ImageLayout compressedLayouts;

        compressedLayouts.usages  = DbUsages;
        compressedLayouts.engines = LayoutUniversalEngine;

        if (isMsaa)
        {
            if (Formats::BitsPerPixel(m_createInfo.swizzledFormat.format) == 8)
            {
                // Decompress/Resolve stencil only format image does not need sample location information.
                compressedLayouts.usages |= LayoutResolveSrc;
            }
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 406
            else if (m_createInfo.flags.sampleLocsAlwaysKnown)
            {
                // Postpone decompresses for HTILE from Barrier-time to Resolve-time if sample location is always known.
                compressedLayouts.usages |= LayoutResolveSrc;
            }
#endif
        }

        // On Gfxip8 with a TC-compatible htile, even the compressed layout is shader-readable
        // Moreover, either fixed-func depth-stencil copy resolve or pixel shader resolve could keep resolveSrc in
        // compressed state.
        if (pSubResInfo->flags.supportMetaDataTexFetch)
        {
            compressedLayouts.usages |= ShaderReadUsages;
        }

        if (allowComputeDecompress &&
            TestAnyFlagSet(UseComputeExpand, (isMsaa ? UseComputeExpandMsaaDepth : UseComputeExpandDepth)))
        {
            compressedLayouts.engines |= LayoutComputeEngine;
        }

        // Supported depth layouts per compression state
        const uint32 depth   = GetDepthStencilStateIndex(ImageAspect::Depth);
        const uint32 stencil = GetDepthStencilStateIndex(ImageAspect::Stencil);

        m_layoutToState[mip].depthStencil[depth].compressed     = compressedLayouts;
        m_layoutToState[mip].depthStencil[depth].decomprWithHiZ = decomprWithHiZ;

        // Supported stencil layouts per compression state
        if (m_pHtile->TileStencilDisabled() == false)
        {
            m_layoutToState[mip].depthStencil[stencil].compressed     = compressedLayouts;
            m_layoutToState[mip].depthStencil[stencil].decomprWithHiZ = decomprWithHiZ;
        }
        else
        {
            m_layoutToState[mip].depthStencil[stencil].compressed.usages      = 0;
            m_layoutToState[mip].depthStencil[stencil].compressed.engines     = 0;
            m_layoutToState[mip].depthStencil[stencil].decomprWithHiZ.usages  = 0;
            m_layoutToState[mip].depthStencil[stencil].decomprWithHiZ.engines = 0;
        }
    } // End check for (m_pHtile != nullptr)
}

// =====================================================================================================================
// Initializes the GPU offset for this Image's DCC state metadata. It must include an array of Gfx6DccMipMetaData with
// one item for each mip level.
void Image::InitDccStateMetaData(
    ImageMemoryLayout* pGpuMemLayout,
    gpusize*           pGpuMemSize)
{
    m_dccStateMetaDataOffset = Pow2Align(*pGpuMemSize, PredicationAlign);
    m_dccStateMetaDataSize   = m_createInfo.mipLevels * sizeof(MipDccStateMetaData);
    *pGpuMemSize             = (m_dccStateMetaDataOffset + m_dccStateMetaDataSize);

    // Update the layout information against the DCC state metadata.
    UpdateMetaDataHeaderLayout(pGpuMemLayout, m_dccStateMetaDataOffset, PredicationAlign);
}

// =====================================================================================================================
// Initializes the GPU offset for this Image's fast-clear-eliminate metadata. FCE metadata is one DWORD for each mip
// level of the image; if the corresponding DWORD for a miplevel is zero, then a fast-clear-eliminate operation will not
// be required.
void Image::InitFastClearEliminateMetaData(
    ImageMemoryLayout* pGpuMemLayout,
    gpusize*           pGpuMemSize)
{
    m_fastClearEliminateMetaDataOffset = Pow2Align(*pGpuMemSize, PredicationAlign);
    m_fastClearEliminateMetaDataSize   = m_createInfo.mipLevels * sizeof(MipFceStateMetaData);
    *pGpuMemSize                       = (m_fastClearEliminateMetaDataOffset + m_fastClearEliminateMetaDataSize);

    // Update the layout information against the fast-clear eliminate metadata.
    UpdateMetaDataHeaderLayout(pGpuMemLayout, m_fastClearEliminateMetaDataOffset, PredicationAlign);

    // Initialize data structure for fast clear eliminate optimization. The GPU predicates fast clear eliminates
    // when the clear color is TC compatible. So here, we try to not perform fast clear eliminate and save the
    // CPU cycles required to set up the fast clear eliminate.
    m_pNumSkippedFceCounter =  m_device.GetGfxDevice()->AllocateFceRefCount();
}

// =====================================================================================================================
// Initializes the GPU offset for this Image's waTcCompatZRange metadata.
void Image::InitWaTcCompatZRangeMetaData(
    ImageMemoryLayout* pGpuMemLayout,
    gpusize*           pGpuMemSize)
{
    PAL_ASSERT(m_device.GetGfxDevice()->WaTcCompatZRange());
    PAL_ASSERT(m_device.ChipProperties().gfxLevel >= GfxIpLevel::GfxIp8);

    m_waTcCompatZRangeMetaDataOffset       = Pow2Align(*pGpuMemSize, sizeof(uint32));
    m_waTcCompatZRangeMetaDataSizePerMip   = sizeof(uint32);
    *pGpuMemSize                           = (m_waTcCompatZRangeMetaDataOffset +
                                              (m_waTcCompatZRangeMetaDataSizePerMip * m_createInfo.mipLevels));

    // Update the layout information against the waTcCompatZRange metadata.
    UpdateMetaDataHeaderLayout(pGpuMemLayout, m_waTcCompatZRangeMetaDataOffset, sizeof(uint32));
}

// =====================================================================================================================
// Calculates a base_256b address for a subresource HTile metadata.
uint32 Image::GetHtile256BAddr(
    SubresId subresource
    ) const
{
    const SubResourceInfo*const pSubResInfo = m_pParent->SubresourceInfo(subresource);
    if (pSubResInfo->flags.supportMetaDataTexFetch)
    {
        // Theoretically, the Htile address here should have the tile-swizzle OR'd in, but in SetTileSwizzle, the tile
        // swizzle for texture-fetchable depth images is always set to zero, so we should be all set with the base
        // address.
        PAL_ASSERT(AddrMgr1::GetTileInfo(Parent(), subresource)->tileSwizzle == 0);
    }

    return Get256BAddrLo(m_pParent->GetBoundGpuMemory().GpuVirtAddr() + GetHtile(subresource)->MemoryOffset());
}

// =====================================================================================================================
// Populates information to describe the HTile sub-allocation for a particular mipmap level.
void Image::GetHtileBufferInfo(
    uint32           mipLevel,
    uint32           firstSlice,
    uint32           numSlices,
    HtileBufferUsage htileUsage,
    GpuMemory**      ppGpuMemory, // [out] Bound GPU memory
    gpusize*         pOffset,     // [out] Offset to the Htile memory
    gpusize*         pDataSize    // [out] Size of the Htile memory
    ) const
{
    const Gfx6Htile&      htile    = m_pHtile[mipLevel];
    const BoundGpuMemory& boundMem = m_pParent->GetBoundGpuMemory();

    gpusize dataSize    = 0;
    gpusize sliceOffset = 0;

    if (htileUsage == HtileBufferUsage::Init)
    {
        // There could be additional padding due to slice-interleave and mip-interleave. So it is possible that
        // htile.TotalSize() != (htile.SliceSize() * numSlices). The addtional padded htile is required and only
        // required to be set to expanded state at init time. So TotalSize() is required to be returned if there
        // does exsist actual htile padding. Here's possbile cases:
        // 1. perSubresInit == 0, thus numSlices == m_createInfo.arraySize will always be true.
        // 2. perSubresInit == 1 and slice-interleave exsists. Htile will be killed, see gfx6::Image::Finalize.
        // 3. perSubresInit == 1 and sliceSize == 1, numSlices == m_createInfo.arraySize will always be true.
        // 4. perSubresInit == 1 and sliceSize > 1 and no slice-interleave or mip-interleave exsists. Htile
        //    total size will always be multiple of single slice size. Thus there's no actual htile padding and
        //    it's ok to go through the 'else' path in below.
        if (numSlices == m_createInfo.arraySize)
        {
            // If we're initializing all of the HTile slices we can simply set the total size of our HTile buffer.
            // This will work even if we have interleaved slices.
            dataSize = htile.TotalSize();
        }
        else
        {
            // The perSubresInit must be specified to support this case. Note that we do not have to worry about
            // interleaved slice sizes because we will disable HTile in those cases when perSubresInit is set.
            PAL_ASSERT(m_createInfo.flags.perSubresInit == 1);
            PAL_ASSERT((htile.SliceSize() * m_createInfo.arraySize) == htile.TotalSize());

            dataSize    = (htile.SliceSize() * numSlices);
            sliceOffset = (htile.SliceSize() * firstSlice);
        }
    }
    else
    {
        // It's not possible to clear individual HTile slices if they are interleaved because the HTile data is not
        // contiguous. It may be possible to clear all slices at once by clearing the total size, but it's not
        // apparent if it's legal to clear any HTile padding data.
        PAL_ASSERT(htile.SlicesInterleaved() == false);

        dataSize    = (htile.SliceSize() * numSlices);
        sliceOffset = (htile.SliceSize() * firstSlice);
    }

    (*ppGpuMemory) = boundMem.Memory();
    (*pOffset)     = boundMem.Offset() + htile.MemoryOffset() + sliceOffset;
    (*pDataSize)   = dataSize;
}

// =====================================================================================================================
// Returns true if this image requires separate aspect initialization
bool Image::RequiresSeparateAspectInit() const
{
    return (m_createInfo.flags.perSubresInit && m_createInfo.flags.separateDepthAspectInit);
}

// =====================================================================================================================
// Calculates a base_256b address for a subresource CMask metadata.
uint32 Image::GetCmask256BAddr(
    SubresId subresource
    ) const
{
    const gpusize cMaskBaseAddr = (m_pParent->GetGpuVirtualAddr() + GetCmask(subresource)->MemoryOffset());

    const SubResourceInfo*const pSubResInfo = m_pParent->SubresourceInfo(subresource);

    // The cMask address only includes a tile swizzle if the cMask is going to be texture
    // fetched as indicated by fmask's tc-compatibility.
    uint32 tileSwizzle = 0;
    if (IsComprFmaskShaderReadable(pSubResInfo))
    {
        tileSwizzle = AddrMgr1::GetTileInfo(Parent(), subresource)->tileSwizzle;
    }

    return Get256BAddrSwizzled(cMaskBaseAddr, tileSwizzle);
}

// =====================================================================================================================
// Calculates a base_256b address for a subresource DCC metadata.
uint32 Image::GetDcc256BAddr(
    SubresId subresource
    ) const
{
    const gpusize                  dccBaseAddr = (m_pParent->GetBoundGpuMemory().GpuVirtAddr() +
                                                  GetDcc(subresource)->MemoryOffset());
    const AddrMgr1::TileInfo*const pTileInfo   = AddrMgr1::GetTileInfo(Parent(), subresource);

    return Get256BAddrSwizzled(dccBaseAddr, pTileInfo->tileSwizzle);
}

// =====================================================================================================================
// Determines whether a continous range of DCC of array slices can be cleared in one clear.
bool Image::CanMergeClearDccSlices(
    uint32  mipLevel
    ) const
{
    const Gfx6Dcc& dcc = m_pDcc[mipLevel];

    return ((dcc.UseFastClear() == false) || (dcc.SliceSize() == dcc.GetFastClearSize()));
}

// =====================================================================================================================
// Populates information to describe the DCC sub-allocation for one or more array slice of a particular mipmap level.
void Image::GetDccBufferInfo(
    uint32          mipLevel,
    uint32          firstSlice,
    uint32          numSlices,
    DccClearPurpose clearPurpose,
    GpuMemory**     ppGpuMemory,    // [out] Bound GPU memory
    gpusize*        pOffset,        // [out] Offset to the DCC memory
    gpusize*        pDataSize       // [out] Size of the DCC memory
    ) const
{
    const Gfx6Dcc&        dcc      = m_pDcc[mipLevel];
    const BoundGpuMemory& boundMem = Parent()->GetBoundGpuMemory();

    gpusize clearSize   = 0;
    gpusize sliceOffset = 0;

    if (clearPurpose == DccClearPurpose::Init)
    {
        if (numSlices == m_createInfo.arraySize)
        {
            // We need to explictly clear the total size of the DCC buffer in this case because the DCC slice size might
            // not be size-aligned. In other words, (SliceSize() * numSlices) may not equal TotalSize().
            clearSize   = dcc.TotalSize();
        }
        else
        {
            // The perSubresInit must be specified to support this case. Note that we do not have to worry about
            // unaligned slice sizes because we will disable DCC in those cases when perSubresInit is set.
            PAL_ASSERT(m_createInfo.flags.perSubresInit == 1);

            clearSize   = (dcc.SliceSize() * numSlices);
            sliceOffset = (dcc.SliceSize() * firstSlice);
        }
    }
    else
    {
        // If the fast-clear region size does not equal the size of the entire DCC slice, the clear should be split into
        // "numSlices" loops outside. The caller should use CanMergeClearDccSlices() to detect this case. Note that fast
        // clears are disabled when the slice size is not aligned.
        //
        // Either only one slice to clear or the fast-clear size is equal to slice size.
        PAL_ASSERT((numSlices == 1) || (dcc.GetFastClearSize() == dcc.SliceSize()));

        clearSize   = (dcc.GetFastClearSize() * numSlices);
        sliceOffset = (dcc.SliceSize() * firstSlice);
    }

    (*ppGpuMemory) = boundMem.Memory();
    (*pOffset)     = boundMem.Offset() + dcc.MemoryOffset() + sliceOffset;
    (*pDataSize)   = clearSize;
}

// =====================================================================================================================
// Populates information to describe the CMask sub-allocation for one or more array slice of a particular mipmap level.
void Image::GetCmaskBufferInfo(
    uint32      mipLevel,
    uint32      firstSlice,
    uint32      numSlices,
    GpuMemory** ppGpuMemory,    // [out] Bound GPU memory
    gpusize*    pOffset,        // [out] Offset to the Cmask memory
    gpusize*    pDataSize       // [out] Size of the Cmask memory
    ) const
{
    const Gfx6Cmask&      cmask    = m_pCmask[mipLevel];
    const BoundGpuMemory& boundMem = Parent()->GetBoundGpuMemory();

    (*ppGpuMemory) = boundMem.Memory();
    (*pOffset)     = boundMem.Offset() + cmask.MemoryOffset() + (cmask.SliceSize() * firstSlice);
    (*pDataSize)   = cmask.SliceSize() * numSlices;
}

// =====================================================================================================================
// Calculates the GPU virtual address for a subresource FMask metadata.
gpusize Image::GetFmaskBaseAddr(
    SubresId subresource
    ) const
{
    return (Parent()->GetBoundGpuMemory().GpuVirtAddr() + GetFmask(subresource)->MemoryOffset());
}

// =====================================================================================================================
// Calculates the 256-byte-aligned GPU virtual address for a subresource FMask metadata, with the appropriate tile
// swizzle OR'ed-in.
uint32 Image::GetFmask256BAddrSwizzled(
    SubresId subresource
    ) const
{
    const AddrMgr1::TileInfo*const pTileInfo = AddrMgr1::GetTileInfo(Parent(), subresource);

    return Get256BAddrSwizzled(GetFmaskBaseAddr(subresource), pTileInfo->tileSwizzle);
}

// =====================================================================================================================
// Populates information to describe the FMask sub-allocation for the base mipmap level.
void Image::GetFmaskBufferInfo(
    uint32      firstSlice,
    uint32      numSlices,
    GpuMemory** ppGpuMemory,    // [out] Bound GPU memory
    gpusize*    pOffset,        // [out] Offset to the Fmask memory
    gpusize*    pDataSize       // [out] Size of the Fmask memory
    ) const
{
    const Gfx6Fmask&      fmask    = m_pFmask[0];
    const BoundGpuMemory& boundMem = m_pParent->GetBoundGpuMemory();

    (*ppGpuMemory) = boundMem.Memory();
    (*pOffset)     = boundMem.Offset() + fmask.MemoryOffset() + (fmask.SliceSize() * firstSlice);
    (*pDataSize)   = fmask.SliceSize() * numSlices;
}

// =====================================================================================================================
// Get the sub resource's AddrTileMode
AddrTileMode Image::GetSubResourceTileMode(
    SubresId subresource
    ) const
{
    const AddrMgr1::TileInfo*const pTileInfo = AddrMgr1::GetTileInfo(Parent(), subresource);
    return AddrMgr1::AddrTileModeFromHwArrayMode(pTileInfo->tileMode);
}

// =====================================================================================================================
// Get the sub resource's AddrTileType
AddrTileType Image::GetSubResourceTileType(
    SubresId subresource
    ) const
{
    const AddrMgr1::TileInfo*const pTileInfo = AddrMgr1::GetTileInfo(Parent(), subresource);
    return AddrMgr1::AddrTileTypeFromHwMicroTileMode(pTileInfo->tileType);
}

// =====================================================================================================================
// Builds PM4 commands into the command buffer which will update this Image's meta-data to reflect the updated fast
// clear values. Returns the next unused DWORD in pCmdSpace.
uint32* Image::UpdateDepthClearMetaData(
    const SubresRange& range,
    uint32             writeMask,
    float              depthValue,
    uint8              stencilValue,
    PM4Predicate       predicate,
    uint32*            pCmdSpace
    ) const
{
    PAL_ASSERT(HasHtileData());

    PAL_ASSERT((range.startSubres.arraySlice == 0) && (range.numSlices == m_createInfo.arraySize));

    Gfx6FastDepthClearMetaData clearData;
    clearData.dbStencilClear.u32All     = 0;
    clearData.dbStencilClear.bits.CLEAR = stencilValue;
    clearData.dbDepthClear.f32All       = depthValue;

    // Base GPU virtual address of the Image's fast-clear metadata.
    gpusize       gpuVirtAddr  = FastClearMetaDataAddr(range.startSubres.mipLevel);
    const uint32* pSrcData     = nullptr;
    size_t        dwordsToCopy = 0;

    const bool writeDepth   = TestAnyFlagSet(writeMask, HtileAspectDepth);
    const bool writeStencil = TestAnyFlagSet(writeMask, HtileAspectStencil);

    if (writeStencil)
    {
        // Stencil-only or depth/stencil clear: start at the GPU address of the DB_STENCIL_CLEAR register value. Copy
        // one DWORD for stencil-only and two DWORDs for depth/stencil.
        gpuVirtAddr += offsetof(Gfx6FastDepthClearMetaData, dbStencilClear);
        pSrcData     = reinterpret_cast<uint32*>(&clearData.dbStencilClear);
        dwordsToCopy = (writeDepth ? 2 : 1);
    }
    else if (writeDepth)
    {
        // Depth-only clear: write a single DWORD starting at the GPU address of the DB_DEPTH_CLEAR register value.
        gpuVirtAddr += offsetof(Gfx6FastDepthClearMetaData, dbDepthClear);
        pSrcData     = reinterpret_cast<uint32*>(&clearData.dbDepthClear);
        dwordsToCopy = 1;
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    PAL_ASSERT(gpuVirtAddr != 0);

    const CmdUtil& cmdUtil = static_cast<const Device*>(m_device.GetGfxDevice())->CmdUtil();

    // depth stencil meta data storage as the pair, n levels layout is following,
    //
    // S-stencil, D-depth.
    //  ___________________________________________
    // | mipmap0 | mipmap1 | mipmap2  | ... | mipmapn |
    // |________ |_________|_________|___|_________|
    // |  S   |  D  |  S   |  D   |  S   |  D   | ... |  S  |  D   |
    // |___________________________________________|
    // depth-only write or stencil-only wirte should respective skip S/D offset.
    if (writeDepth && writeStencil)
    {
        // update depth-stencil meta data
        PAL_ASSERT(dwordsToCopy == 2);
        return pCmdSpace + cmdUtil.BuildWriteDataPeriodic(gpuVirtAddr,
                                                          dwordsToCopy,
                                                          range.numMips,
                                                          WRITE_DATA_ENGINE_PFP,
                                                          WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                                          true,
                                                          pSrcData,
                                                          predicate,
                                                          pCmdSpace);
    }
    else
    {
        // update depth-only or stencil-only meta data
        PAL_ASSERT(dwordsToCopy == 1);
        size_t strideWriteData = sizeof(Gfx6FastDepthClearMetaData);

        for (size_t levelOffset = 0; levelOffset < range.numMips; levelOffset++)
        {
            pCmdSpace += cmdUtil.BuildWriteData(gpuVirtAddr,
                                                dwordsToCopy,
                                                WRITE_DATA_ENGINE_PFP,
                                                WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                                true,
                                                pSrcData,
                                                predicate,
                                                pCmdSpace);

            gpuVirtAddr += strideWriteData;
        }

        return pCmdSpace;
    }
}

// =====================================================================================================================
// Builds PM4 commands into the command buffer which will update this Image's waTcCompatZRange metadata to reflect the
// most recent depth fast clear value. Returns the next unused DWORD in pCmdSpace.
uint32* Image::UpdateWaTcCompatZRangeMetaData(
    const SubresRange& range,
    float              depthValue,
    PM4Predicate       predicate,
    uint32*            pCmdSpace
    ) const
{
    PAL_ASSERT(m_device.GetGfxDevice()->WaTcCompatZRange());
    PAL_ASSERT(HasWaTcCompatZRangeMetaData());

    // If the last fast clear value was 0.0f, the DB_Z_INFO.ZRANGE_PRECISION register field should be written to 0 when
    // a depth target is bound. The metadata is used as a COND_EXEC condition, so it needs to be set to true when the
    // clear value is 0.0f, and false otherwise.
    const uint32 metaData = (depthValue == 0.0f) ? UINT_MAX : 0;

    // Base GPU virtual address of the Image's waTcCompatZRange metadata.
    const gpusize gpuVirtAddr  = GetWaTcCompatZRangeMetaDataAddr(range.startSubres.mipLevel);
    const size_t  dwordsToCopy = 1;

    PAL_ASSERT(gpuVirtAddr != 0);

    const CmdUtil& cmdUtil = static_cast<const Device*>(m_device.GetGfxDevice())->CmdUtil();

    return pCmdSpace + cmdUtil.BuildWriteDataPeriodic(gpuVirtAddr,
                                                      dwordsToCopy,
                                                      range.numMips,
                                                      WRITE_DATA_ENGINE_PFP,
                                                      WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                                      true,
                                                      &metaData,
                                                      predicate,
                                                      pCmdSpace);
}

// =====================================================================================================================
// Builds PM4 commands into the command buffer which will update this Image's fast-clear metadata to reflect the most
// recent clear color. Returns the next unused DWORD in pCmdSpace.
uint32* Image::UpdateColorClearMetaData(
    uint32       startMip,
    uint32       numMips,
    const uint32 packedColor[4],
    PM4Predicate predicate,
    uint32*      pCmdSpace
    ) const
{
    // Verify that we have some sort of meta-data here that's capable of handling fast clears.
    PAL_ASSERT(HasCmaskData() || HasDccData());

    const CmdUtil& cmdUtil = static_cast<const Device*>(m_device.GetGfxDevice())->CmdUtil();

    // Number of DWORD registers which represent the fast-clear color for a bound color target:
    constexpr size_t MetaDataDwords = sizeof(Gfx6FastColorClearMetaData) / sizeof(uint32);

    const gpusize gpuVirtAddr = FastClearMetaDataAddr(startMip);
    PAL_ASSERT(gpuVirtAddr != 0);

    // Issue a WRITE_DATA command to update the fast-clear metadata.
    return pCmdSpace + cmdUtil.BuildWriteDataPeriodic(gpuVirtAddr,
                                                      MetaDataDwords,
                                                      numMips,
                                                      WRITE_DATA_ENGINE_PFP,
                                                      WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                                      true,
                                                      packedColor,
                                                      predicate,
                                                      pCmdSpace);
}

// =====================================================================================================================
// Builds PM4 commands into the command buffer which will update this Image's DCC state metadata over the given mip
// range to reflect the given compression state. Returns the next unused DWORD in pCmdSpace.
uint32* Image::UpdateDccStateMetaData(
    const SubresRange& range,
    bool               isCompressed,
    PM4Predicate       predicate,
    uint32*            pCmdSpace
    ) const
{
    PAL_ASSERT(HasDccData());

    const CmdUtil& cmdUtil = static_cast<const Device*>(m_device.GetGfxDevice())->CmdUtil();

    // We need to write one item per mip in the range. We can do this most efficiently with a single WRITE_DATA.
    PAL_ASSERT(range.numMips <= MaxImageMipLevels);

    const gpusize gpuVirtAddr = GetDccStateMetaDataAddr(range.startSubres.mipLevel);
    PAL_ASSERT(gpuVirtAddr != 0);

    MipDccStateMetaData metaData = { };
    metaData.isCompressed = (isCompressed ? 1 : 0);

    return pCmdSpace + cmdUtil.BuildWriteDataPeriodic(gpuVirtAddr,
                                                      (sizeof(metaData) / sizeof(uint32)),
                                                      range.numMips,
                                                      WRITE_DATA_ENGINE_PFP,
                                                      WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                                      true,
                                                      reinterpret_cast<uint32*>(&metaData),
                                                      predicate,
                                                      pCmdSpace);
}

// =====================================================================================================================
// Builds PM4 commands into the command buffer which will update this Image's fast-clear-eliminate metadata over the
// given mip range to reflect the given value. Returns the next unused DWORD in pCmdSpace.
uint32* Image::UpdateFastClearEliminateMetaData(
    const SubresRange& range,
    uint32             value,
    PM4Predicate       predicate,
    uint32*            pCmdSpace
    ) const
{
    const CmdUtil& cmdUtil = static_cast<const Device*>(m_device.GetGfxDevice())->CmdUtil();

    // We need to write one DWORD per mip in the range. We can do this most efficiently with a single WRITE_DATA.
    PAL_ASSERT(range.numMips <= MaxImageMipLevels);

    const gpusize gpuVirtAddr = GetFastClearEliminateMetaDataAddr(range.startSubres.mipLevel);
    PAL_ASSERT(gpuVirtAddr != 0);

    MipFceStateMetaData metaData = { };
    metaData.fceRequired = value;

    return pCmdSpace + cmdUtil.BuildWriteDataPeriodic(gpuVirtAddr,
                                                      (sizeof(metaData) / sizeof(uint32)),
                                                      range.numMips,
                                                      WRITE_DATA_ENGINE_PFP,
                                                      WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                                      true,
                                                      reinterpret_cast<uint32*>(&metaData),
                                                      predicate,
                                                      pCmdSpace);
}

// =====================================================================================================================
// Determines if a resource's fmask is TC compatible/shader readable, allowing read access without an fmask expand.
bool Image::IsComprFmaskShaderReadable(
    const SubResourceInfo*  pSubResInfo
    ) const
{
    bool isComprFmaskShaderReadable = false;

    // If this device doesn't allow any tex fetches of fmask meta data, then don't bother continuing
    if (TestAnyFlagSet(m_device.GetPublicSettings()->tcCompatibleMetaData, Pal::TexFetchMetaDataCapsFmask) &&
        // TC compatibility is only important for Gfx8+
        (m_device.ChipProperties().gfxLevel >= GfxIpLevel::GfxIp8) &&
        // Must be multisampled
        (m_createInfo.samples > 1) &&
        // Either image is tc-compatible or if not it has no dcc and hence we can keep famsk surface
        // in tccompatible state
        ((pSubResInfo->flags.supportMetaDataTexFetch == 1) ||
         ((pSubResInfo->flags.supportMetaDataTexFetch == 0) && (HasDccData() == false))) &&
        // If this image isn't readable by a shader then no shader is going to be performing texture fetches from it...
        // Msaa image with resolveSrc usage flag will go through shader based resolve if fixed function resolve is not
        // preferred, the image will be readable by a shader.
        (m_pParent->IsShaderReadable() ||
         (m_pParent->IsResolveSrc() && (m_pParent->PreferCbResolve() == false))) &&
        // Since TC block can't write to compressed images
        (m_pParent->IsShaderWritable() == false) &&
        // Only 2D/3D tiled resources can use shader compatible compression
        IsMacroTiled(pSubResInfo))
    {
        isComprFmaskShaderReadable = true;
    }

    return isComprFmaskShaderReadable;
}

// =====================================================================================================================
// Determines if this tile-mode supports direct texture fetches of its meta data or not
bool Image::SupportsMetaDataTextureFetch(
    AddrTileMode    tileMode,
    AddrTileType    tileType,
    ChNumFormat     format,
    const SubresId& subResource
    ) const
{
    bool  texFetchSupported = false;
    bool  enableTcCompatResolveDst = false;

    // TcCompatible could be enabled for resolveDst depth-stencil in order to enhance opportunity to trigger fixed-func
    // depth-stencil resolve.
    const bool isDepthStencilResolveDst = (m_pParent->IsResolveDst() && m_pParent->IsDepthStencil());
    const bool isDepth                  = m_pParent->IsAspectValid(ImageAspect::Depth);
    const bool isStencil                = m_pParent->IsAspectValid(ImageAspect::Stencil);

    enableTcCompatResolveDst =
        (isDepthStencilResolveDst &&
         ((isDepth  && !isStencil && TestAnyFlagSet(TcCompatibleResolveDst, Gfx8TcCompatibleResolveDstDepthOnly))   ||
          (!isDepth && isStencil  && TestAnyFlagSet(TcCompatibleResolveDst, Gfx8TcCompatibleResolveDstStencilOnly)) ||
          (isDepth  && isStencil  && TestAnyFlagSet(TcCompatibleResolveDst, Gfx8TcCompatibleResolveDstDepthAndStencil))));

    const bool useSharedMetadata = m_pParent->GetInternalCreateInfo().flags.useSharedMetadata;

    // If this device doesn't allow any tex fetches of meta data, then don't bother continuing
    if ((m_device.GetPublicSettings()->tcCompatibleMetaData != 0) &&
        // TC compatibility is only important for Gfx8+
        (m_device.ChipProperties().gfxLevel >= GfxIpLevel::GfxIp8) &&
        // If this image isn't readable by a shader then no shader is going to be performing texture fetches from it...
        // Msaa image with resolveSrc usage flag will go through shader based resolve if fixed function resolve is not
        // preferred, the image will be readable by a shader.
        (m_pParent->IsShaderReadable() ||
         (m_pParent->IsResolveSrc() && (m_pParent->PreferCbResolve() == false)) ||
         enableTcCompatResolveDst ||
         useSharedMetadata) &&
        // Only 2D/3D tiled resources can use shader compatible compression
        IsMacroTiled(tileMode))
    {
        texFetchSupported = (m_pParent->IsDepthStencil())
                                ? DepthImageSupportsMetaDataTextureFetch(format, subResource)
                                : ColorImageSupportsMetaDataTextureFetch(tileMode, tileType);

        if ((subResource.mipLevel > 0) && texFetchSupported)
        {
            // We have prior info that "subRes with mip-i is tcCompatible" is the pre-condition of "subRes with
            // mip-i+1 is tcCompatible". So we check whether last mip is tcCompatible and then decide whether to
            // set tcCompatible to 1 when configing subRes. This saves redundant tcCompatible check in addrLib
            // for following child mips once mip-i is not tcCompatible. Moreover, if depth-and-stencil image is
            // required to be tcCompatible and there's shader access upon the image, Pal will set matchStencilTileCfg
            // to 1 for the depth aspect only for mip-0. As a side effect, addrLib might return tcCompatible to 0 for
            // subRes with mip-0, while addrLib will return tcCompatible to 1 for subRes with mipLevel > 0. Checking
            // whether last mip is tcCompatible could resolve the side effect.
            SubresId lastMipSubResId = {};
            lastMipSubResId.aspect     = subResource.aspect;
            lastMipSubResId.mipLevel   = (subResource.mipLevel - 1);
            lastMipSubResId.arraySlice = subResource.arraySlice;
            const SubResourceInfo& lastMipSubResInfo = *Parent()->SubresourceInfo(lastMipSubResId);

            texFetchSupported = lastMipSubResInfo.flags.supportMetaDataTexFetch;
        }
    }

    return texFetchSupported;
}

// =====================================================================================================================
// Determines if this color surface supports direct texture fetches of its cmask/fmask/dcc data or not. Note that this
// function is more a heurestic then actual fact, so it should be used with care.
bool Image::ColorImageSupportsMetaDataTextureFetch(
    AddrTileMode  tileMode,
    AddrTileType  tileType
    ) const
{
    // Assume texture fetches won't be allowed
    bool  texFetchAllowed = false;

    if (Parent()->GetInternalCreateInfo().flags.useSharedMetadata)
    {
        texFetchAllowed = Parent()->GetInternalCreateInfo().sharedMetadata.flags.shaderFetchable;
    }
    // Does this image have DCC memory?  Note that this function is called from the address library, meaning that DCC
    // memory may not have been allocated yet.
    // true param assumes resource can be made TC compat since this isn't known for sure until after calling addrlib.
    else if (Gfx6Dcc::UseDccForImage(m_device, (*this), tileMode, tileType, true))
    {
        const uint32 tcCompatibleMetaData = m_device.GetPublicSettings()->tcCompatibleMetaData;

        if ((m_createInfo.samples > 1) &&
            // MSAA meta-data surfaces are only texture fetchable if allowed in the caps.
            TestAnyFlagSet(tcCompatibleMetaData, TexFetchMetaDataCapsMsaaColor))
        {
            texFetchAllowed = true;
        }
        else if ((m_createInfo.samples == 1) &&
                 TestAnyFlagSet(tcCompatibleMetaData, TexFetchMetaDataCapsNoAaColor))
        {
            texFetchAllowed = true;
        }
    }

    return texFetchAllowed;
}

// =====================================================================================================================
// Returns true if the format surface's hTile data can be directly fetched by the texture block. The z-specific aspect
// of the surface must be z-32.
bool Image::DepthMetaDataTexFetchIsZValid(
    ChNumFormat  format
    ) const
{
    const ZFormat          zHwFmt   = HwZFmt(MergedChannelFmtInfoTbl(m_device.ChipProperties().gfxLevel), format);

    bool  isZValid = false;

    if (zHwFmt == Z_16)
    {
        isZValid = TestAnyFlagSet(m_device.GetPublicSettings()->tcCompatibleMetaData, TexFetchMetaDataCapsAllowZ16);
    }
    else if (zHwFmt == Z_32_FLOAT)
    {
        isZValid = true;
    }

    return isZValid;
}

// =====================================================================================================================
// Determines if the tile-info is identical between the Z and stencil aspects of this image Caller is responsible for
// determining that both aspects do exist.
bool Image::DoesTileInfoMatch(
    const SubresId& subresId
    ) const
{
    PAL_ASSERT(m_pParent->IsAspectValid(ImageAspect::Stencil));
    PAL_ASSERT(m_pParent->IsAspectValid(ImageAspect::Depth));

    const SubresId                 stencilSubRes = { ImageAspect::Stencil, subresId.mipLevel, subresId.arraySlice };
    const AddrMgr1::TileInfo*const pStencilInfo  = AddrMgr1::GetTileInfo(Parent(), stencilSubRes);

    const SubresId                 depthSubRes = { ImageAspect::Depth, subresId.mipLevel, subresId.arraySlice };
    const AddrMgr1::TileInfo*const pDepthInfo  = AddrMgr1::GetTileInfo(Parent(), depthSubRes);

    bool tileInfoMatches = true;

    // NOTE: Depth and Stencil have register fields for tileSplitBytes so it doesn't have to match.
    if ((pStencilInfo->bankWidth        != pDepthInfo->bankWidth)  ||
        (pStencilInfo->bankHeight       != pDepthInfo->bankHeight) ||
        (pStencilInfo->banks            != pDepthInfo->banks)      ||
        (pStencilInfo->pipeConfig       != pDepthInfo->pipeConfig) ||
        (pStencilInfo->macroAspectRatio != pDepthInfo->macroAspectRatio))
    {
        tileInfoMatches = false;
    }

    return tileInfoMatches;
}

// =====================================================================================================================
// Determines if this depth surface supports direct texture fetches of its htile data
bool Image::DepthImageSupportsMetaDataTextureFetch(
    ChNumFormat     format,
    const SubresId& subResource
    ) const
{
    bool isFmtLegal = true;

    if (m_pParent->IsAspectValid(ImageAspect::Stencil) &&
        (TestAnyFlagSet(m_device.GetPublicSettings()->tcCompatibleMetaData, TexFetchMetaDataCapsAllowStencil) == false))
    {
        // The settings disallows tex fetches of any compressed depth image that contains stencil
        isFmtLegal = false;
    }

    if (isFmtLegal)
    {
        if (subResource.aspect == ImageAspect::Depth)
        {
            isFmtLegal = DepthMetaDataTexFetchIsZValid(format);
        }
        else if (subResource.aspect == ImageAspect::Stencil)
        {
            if (m_pParent->IsAspectValid(ImageAspect::Depth))
            {
                // Verify that the z-aspect of this image is compatible with the texture pipe and compression.
                const SubresId zSubres = { ImageAspect::Depth, subResource.mipLevel, subResource.arraySlice };

                isFmtLegal = DepthMetaDataTexFetchIsZValid(m_pParent->SubresourceInfo(zSubres)->format.format);
            }
        }
    }

    // Assume that texture fetches won't work.
    bool  texFetchAllowed = false;

    // Image must have hTile data for a meta-data texture fetch to make sense.  This function is called from gfx6AddrLib
    // before any hTile memory has been allocated, so we can't look to see if hTile memory actually exists, because it
    // won't yet.
    // An opened image's hTile should be retrieved from internal creation info.
    if (isFmtLegal)
    {
        if (Parent()->GetInternalCreateInfo().flags.useSharedMetadata)
        {
            texFetchAllowed = Parent()->GetInternalCreateInfo().sharedMetadata.flags.shaderFetchable;
        }
        else if (Gfx6Htile::UseHtileForImage(m_device, *this, true))
        {
            if ((m_createInfo.samples > 1) &&
                 // MSAA meta-data surfaces are only texture fetchable if allowed in the caps.
                TestAnyFlagSet(m_device.GetPublicSettings()->tcCompatibleMetaData, TexFetchMetaDataCapsMsaaDepth))
            {
                texFetchAllowed = true;
            }
            else if ((m_createInfo.samples == 1) &&
                     TestAnyFlagSet(m_device.GetPublicSettings()->tcCompatibleMetaData, TexFetchMetaDataCapsNoAaDepth))
            {
                texFetchAllowed = true;
            }
        }
    }

    return texFetchAllowed;
}

// =====================================================================================================================
// Determines if the specified sub-resource of this image supports being fast-cleared to the given color
bool Image::IsFastColorClearSupported(
    GfxCmdBuffer*      pCmdBuffer,  // Command buffer that will be used for the clear
    ImageLayout        colorLayout, // Current layout of the image's color aspect
    const uint32*      pColor,      // 4-component color to use for the clear
    const SubresRange& range)       // The sub-resource range that will be cleared
{
    // This logic for fast-clearable tex-fetch images is only valid for color images; depth images have their own
    // restrictions which are implemented in IsFastDepthStencilClearSupported().
    PAL_ASSERT(m_pParent->IsDepthStencil() == false);

    const SubresId&             subResource   = range.startSubres;
    const SubResourceInfo*const pSubResInfo   = m_pParent->SubresourceInfo(subResource);
    const ColorLayoutToState&   layoutToState = m_layoutToState[subResource.mipLevel].color;

    // Fast clear is only possible if meta surfaces exist, the image is currently in a color-compressible
    // layout, and we are clearing all arrays at once.
    bool isFastClearSupported = (HasDccData() || HasCmaskData()) &&
                                (ImageLayoutToColorCompressionState(layoutToState, colorLayout) == ColorCompressed) &&
                                (subResource.arraySlice == 0) &&
                                (range.numSlices == m_createInfo.arraySize);

    // When the image has DCC memory, the fast-clear (using compute shader) clears a continous block of DCC memory,
    // which is impossible when the DCC memory of this subresource is not properly aligned (the DCC data are interleaved
    // between subresources).
    if (isFastClearSupported && HasDccData())
    {
        isFastClearSupported = UseDccFastClear(subResource);
    }

    if (isFastClearSupported)
    {
        // A count of 1 indicates that no command buffer has skipped a fast clear eliminate and hence holds a reference
        // to this image's ref counter. 0 indicates that the optimization is disabled.
        const bool noSkippedFastClearElim   = (Pal::GfxImage::GetFceRefCount() <= 1);
        const bool isClearColorTcCompatible = IsFastClearColorMetaFetchable(pColor);

        SetNonTcCompatClearFlag(isClearColorTcCompatible == false);

        // Figure out if we can do a cmask- or a non-TC compatible DCC fast clear.  This kind of fast clear works
        // on any clear color, but requires a fast clear eliminate blt.
        const bool nonTcCompatibleFastClearPossible =
            // Non-universal queues can't execute CB fast clear eliminates.  If the image layout declares a non-
            // universal queue type as currently legal, the barrier to execute such a blit may occur on one of those
            // unsupported queues and thus will be ignored.  Because there's a chance the eliminate may be skipped, we
            // must not allow a cmask-based fast clear to happen under such circumstances.
            (colorLayout.engines == LayoutUniversalEngine) &&
            // The image setting must dictate that all fast clear colors are allowed -- not just TC-compatible ones
            // (this is a profile preference in case sometimes the fast clear eliminate becomes too expensive for
            // specific applications)
            ColorImageSupportsAllFastClears() &&
            // Allow non-TC compatible clears only if there are no skipped fast clear eliminates.
            noSkippedFastClearElim;

        // Figure out if we can do a TC-compatible DCC fast clear (one that requires no fast clear eliminate blt)
        const bool tcCompatDccFastClearPossible =
            // Short-circuit the rest of the checks: if we can already do a cmask fast clear, we don't even care about
            // DCC fast clear
            (nonTcCompatibleFastClearPossible == false) &&
            // The image supports TC-compatible reads from DCC-compressed surfaces
            pSubResInfo->flags.supportMetaDataTexFetch &&
            // The clear value is TC-compatible
            isClearColorTcCompatible;

        // Allow fast clear only if either is possible
        isFastClearSupported = (nonTcCompatibleFastClearPossible || tcCompatDccFastClearPossible);
    }

    return isFastClearSupported;
}

// =====================================================================================================================
bool Image::IsFastClearDepthMetaFetchable(
    float depth
    ) const
{
    return ((depth == 0.0f) || (depth == 1.0f));
}

// =====================================================================================================================
bool Image::IsFastClearStencilMetaFetchable(
    uint8 stencil
    ) const
{
    return (stencil == 0);
}

// =====================================================================================================================
// Returns true if fast depth/stencil clears are supported by the image's current layouts.
bool Image::IsFastDepthStencilClearSupported(
    ImageLayout        depthLayout,
    ImageLayout        stencilLayout,
    float              depth,
    uint8              stencil,
    const SubresRange& range
    ) const
{
    const SubresId& subResource = range.startSubres;

    // We can only fast clear all arrays at once.
    bool isFastClearSupported = (subResource.arraySlice == 0) && (range.numSlices == m_createInfo.arraySize);

    // Choose which layout to use based on range aspect
    const ImageLayout layout = (subResource.aspect == ImageAspect::Depth) ? depthLayout : stencilLayout;

    // Map from layout to supported compression state
    const DepthStencilCompressionState state =
        ImageLayoutToDepthCompressionState(LayoutToDepthCompressionState(subResource), layout);

    // Layouts that do not support depth-stencil compression can not be fast cleared
    if (state != DepthStencilCompressed)
    {
        isFastClearSupported = false;
    }

    const SubResourceInfo*const pSubResInfo = m_pParent->SubresourceInfo(subResource);

    // Subresources that do not enable a fast clear method at all can not be fast cleared
    if ((pSubResInfo->clearMethod != ClearMethod::Fast) &&
        (pSubResInfo->clearMethod != ClearMethod::DepthFastGraphics))
    {
        isFastClearSupported = false;
    }

    if (pSubResInfo->flags.supportMetaDataTexFetch != 0)
    {
        if (subResource.aspect == ImageAspect::Depth)
        {
            isFastClearSupported &= IsFastClearDepthMetaFetchable(depth);
        }
        else if (subResource.aspect == ImageAspect::Stencil)
        {
            isFastClearSupported &= IsFastClearStencilMetaFetchable(stencil);
        }
    }
    else
    {
        // If we are doing a non TC compatible htile fast clear, we need to be able to execute a DB decompress
        // on any of the queue types enabled by the current layout.  This is only possible on universal queues.
        isFastClearSupported &= (layout.engines == LayoutUniversalEngine);
    }

    return isFastClearSupported;
}

// =====================================================================================================================
// Determines if this image supports being cleared or copied with format replacement.
bool Image::IsFormatReplaceable(
    const SubresId& subresId,
    ImageLayout     layout
    ) const
{
    bool  isFormatReplaceable = false;

    if (Parent()->IsDepthStencil())
    {
        const auto layoutToState = LayoutToDepthCompressionState(subresId);

        // Htile must either be disabled or we must be sure that the texture pipe doesn't need to read it.
        // Depth surfaces are either Z-16 unorm or Z-32 float; they would get replaced to x16-uint or x32-uint.
        // Z-16 unorm is actually replaceable, but Z-32 float will be converted to unorm if replaced.
        isFormatReplaceable = ((HasHtileData() == false) ||
                               (ImageLayoutToDepthCompressionState(layoutToState, layout) != DepthStencilCompressed));
    }
    else
    {
        const auto layoutToState = LayoutToColorCompressionState(subresId);

        // DCC must either be disabled or we must be sure that it is decompressed.
        isFormatReplaceable = ((HasDccData() == false) ||
                               (ImageLayoutToColorCompressionState(layoutToState, layout) == ColorDecompressed));
    }

    return isFormatReplaceable;
}

// =====================================================================================================================
// Determines the memory requirements for this query. CZ cannot immediate flip from local to non-local so we keep all
// primaries for a swap chain (same size, same device) exclusively in non-local. The workaround is described in
// detail in the DCE11_SDD_SCATTER_GATHER doc.
void Image::OverrideGpuMemHeaps(
    GpuMemoryRequirements* pMemReqs     // [in,out] returns with populated 'heap' info
    ) const
{
    if ((m_pImageInfo->internalCreateInfo.flags.primarySupportsNonLocalHeap != 0) &&
        static_cast<const Pal::Gfx6::Device*>(m_device.GetGfxDevice())->WaMiscMixedHeapFlips())
    {
        pMemReqs->heapCount = 2;
        pMemReqs->heaps[0]  = GpuHeapGartUswc;
        pMemReqs->heaps[1]  = GpuHeapGartCacheable;
    }
}

// =====================================================================================================================
// Determines if this texture-compatible color image supports fast clears regardless of the clear color.  It is the
// callers responsibility to verify that this function is not called for depth images and that it is only called for
// texture-compatible images as well.
bool Image::ColorImageSupportsAllFastClears() const
{
    const Gfx6PalSettings& settings = GetGfx6Settings(m_device);
    bool allColorClearsSupported = false;

    PAL_ASSERT(Parent()->IsDepthStencil() == false);

    if (m_createInfo.samples > 1)
    {
        allColorClearsSupported = TestAnyFlagSet(settings.gfx8FastClearAllTcCompatColorSurfs,
                                                 Gfx8FastClearAllTcCompatColorSurfsMsaa);
    }
    else
    {
        allColorClearsSupported = TestAnyFlagSet(settings.gfx8FastClearAllTcCompatColorSurfs,
                                                 Gfx8FastClearAllTcCompatColorSurfsNoAa);
    }

    return allColorClearsSupported;
}

// =====================================================================================================================
// Determines the GPU virtual address of the DCC state meta-data. Returns the GPU address of the meta-data, zero if this
// image doesn't have the DCC state meta-data.
gpusize Image::GetDccStateMetaDataAddr(
    uint32  mipLevel
    ) const
{
    PAL_ASSERT(mipLevel < m_createInfo.mipLevels);

    return (m_dccStateMetaDataOffset == 0)
        ? 0
        : Parent()->GetBoundGpuMemory().GpuVirtAddr() + m_dccStateMetaDataOffset +
          (mipLevel * sizeof(MipDccStateMetaData));
}

// =====================================================================================================================
// Determines the offset of the DCC state meta-data. Returns the offset of the meta-data, zero if this
// image doesn't have the DCC state meta-data.
gpusize Image::GetDccStateMetaDataOffset(
    uint32  mipLevel
    ) const
{
    PAL_ASSERT(mipLevel < m_createInfo.mipLevels);

    return (m_dccStateMetaDataOffset == 0)
        ? 0
        : m_dccStateMetaDataOffset + (mipLevel * sizeof(MipDccStateMetaData));
}

// =====================================================================================================================
// Determines the GPU virtual address of the fast-clear-eliminate meta-data.  This metadata is used by a
// conditional-execute packet around the fast-clear-eliminate packets. Returns the GPU address of the
// fast-clear-eliminiate packet, zero if this image does not have the FCE meta-data.
gpusize Image::GetFastClearEliminateMetaDataAddr(
    uint32  mipLevel
    ) const
{
    PAL_ASSERT(mipLevel < m_createInfo.mipLevels);

    return (m_fastClearEliminateMetaDataOffset == 0)
        ? 0
        : Parent()->GetBoundGpuMemory().GpuVirtAddr() + m_fastClearEliminateMetaDataOffset +
          (mipLevel * sizeof(MipFceStateMetaData));
}

// =====================================================================================================================
// Determines the offset of the fast-clear-eliminate meta-data.  This metadata is used by a
// conditional-execute packet around the fast-clear-eliminate packets. Returns the offset of the
// fast-clear-eliminiate packet, zero if this image does not have the FCE meta-data.
gpusize Image::GetFastClearEliminateMetaDataOffset(
    uint32  mipLevel
    ) const
{
    PAL_ASSERT(mipLevel < m_createInfo.mipLevels);

    return (m_fastClearEliminateMetaDataOffset == 0)
        ? 0
        : m_fastClearEliminateMetaDataOffset + (mipLevel * sizeof(MipFceStateMetaData));
}

// =====================================================================================================================
// Determines the GPU virtual address of the waTcCompatZRange meta-data. Returns the GPU address of the meta-data
// This function is not called if this image doesn't have waTcCompatZRange meta-data.
gpusize Image::GetWaTcCompatZRangeMetaDataAddr(
    uint32 mipLevel
    ) const
{
    return (Parent()->GetBoundGpuMemory().GpuVirtAddr() + m_waTcCompatZRangeMetaDataOffset +
              (m_waTcCompatZRangeMetaDataSizePerMip * mipLevel));
}

// =====================================================================================================================
// Determines the correct AddrLib tile mode to use for a subresource
Result Image::ComputeAddrTileMode(
    uint32        subResIdx,
    AddrTileMode* pTileMode     // [out] Computed tile mode
    ) const
{
    const AddrMgr1::TilingCaps*const pTileCaps   = AddrMgr1::GetTilingCaps(Parent(), subResIdx);
    const SubResourceInfo*const      pSubResInfo = m_pParent->SubresourceInfo(subResIdx);

    Result result = Result::Success;

    // Default to linear tiling
    *pTileMode = ADDR_TM_LINEAR_ALIGNED;

    if (m_pImageInfo->internalCreateInfo.flags.useSharedTilingOverrides)
    {
        *pTileMode = m_pImageInfo->internalCreateInfo.gfx6.sharedTileMode;
    }
    else if (m_createInfo.imageType == ImageType::Tex1d)
    {
        if (Parent()->IsDepthStencil())
        {
            // Depth/Stencil has to be tiled
            *pTileMode = ADDR_TM_1D_TILED_THIN1;
        }
        else
        {
            // Other 1D images must be linear tiled
            *pTileMode = ADDR_TM_LINEAR_ALIGNED;
        }
    }
    else if (m_createInfo.imageType == ImageType::Tex2d)
    {
        if (m_createInfo.flags.prt == 1)
        {
            // 2D, PRT Images
            if (pTileCaps->tilePrtThin1 == 1)
            {
                *pTileMode = ADDR_TM_PRT_TILED_THIN1;
            }
            else if (pTileCaps->tile2DThin1 == 1)
            {
                *pTileMode = ADDR_TM_2D_TILED_THIN1;
            }
        }
        else if (m_createInfo.samples > 1)
        {
            // non-PRT, MSAA images must be 2DThin1
            if (pTileCaps->tile2DThin1 == 1)
            {
                *pTileMode = ADDR_TM_2D_TILED_THIN1;
            }
            else
            {
                result = Result::ErrorUnknown;
            }
        }
        else
        {
            // 2D, non-PRT, non-MSAA images
            if (pTileCaps->tile2DThin1 == 1)
            {
                *pTileMode = ADDR_TM_2D_TILED_THIN1;
            }
            else if (pTileCaps->tilePrtThin1 == 1)
            {
                // This image isn't PRT but we might need to use a PRT mode if our caps can't support 2D_THIN1.
                *pTileMode = ADDR_TM_PRT_TILED_THIN1;
            }
            else if (pTileCaps->tile1DThin1 == 1)
            {
                *pTileMode = ADDR_TM_1D_TILED_THIN1;
            }
        }
    }
    else
    {
        PAL_ASSERT(m_createInfo.imageType == ImageType::Tex3d);

        // 3D Images
        if ((pSubResInfo->bitsPerTexel     <= 64)                              &&
            (m_createInfo.extent.depth     >= 8)                               &&
            (pTileCaps->tile2DXThick       == 1)                               &&
            (m_createInfo.flags.prt        == 0)                               &&
            (Parent()->IsRenderTarget()   == false)                            &&
            (Parent()->IsShaderWritable() == false)                            &&
            (Formats::IsMacroPixelPacked(pSubResInfo->format.format) == false) &&
            (ApplyXthickDccWorkaround(ADDR_TM_2D_TILED_XTHICK) == false))
        {
            // 2D-tiled-xthick can never be used with DCC surfaces if the workaround is enabled
            // Do not use 2D_TILED_XTHICK for 3D render target or UAVs. On most ASICs, XThick has only one tile type:
            // ---the thick micro tile mode, which is not efficient for 3D RT/UAV. Using 2D_TILED_THICK instead allows
            // us to choose the efficient NON_DISPLAYABLE tile type.
            *pTileMode = ADDR_TM_2D_TILED_XTHICK;
        }
        else if ((m_createInfo.extent.depth >= 4) &&
                 (pTileCaps->tile2DThick    == 1) &&
                 (Formats::IsMacroPixelPacked(pSubResInfo->format.format) == false))
        {
            // 2D-tiled-thick can be used with DCC surfaces if we force the tile-type to non-
            // displayable (done in "ComputeAddrTileType" below).
            *pTileMode = ADDR_TM_2D_TILED_THICK;
        }
        else if ((pTileCaps->tile1DThick == 1) && (Formats::IsMacroPixelPacked(pSubResInfo->format.format) == false))
        {
            *pTileMode = ADDR_TM_1D_TILED_THICK;
        }
        else if (pTileCaps->tile2DThin1 == 1)
        {
            *pTileMode = ADDR_TM_2D_TILED_THIN1;
        }
        else if (pTileCaps->tile1DThin1 == 1)
        {
            *pTileMode = ADDR_TM_1D_TILED_THIN1;
        }

        if (m_createInfo.flags.prt == 1)
        {
            // Degrade the tile to avoid a tile split on some HW (e.g. Hawaii). Additionally,
            // this lets the clients see the same tile thickness regardless of DRAM row size.
            const bool degradeThickTile = (pSubResInfo->bitsPerTexel >= 64);

            switch(*pTileMode)
            {
            case ADDR_TM_1D_TILED_THIN1:
                *pTileMode = ADDR_TM_PRT_TILED_THIN1;
                break;
            case ADDR_TM_1D_TILED_THICK:
                if (degradeThickTile == false)
                {
                    *pTileMode = ADDR_TM_PRT_TILED_THICK;
                }
                else
                {
                    *pTileMode = ADDR_TM_PRT_TILED_THIN1;
                }
                break;
            case ADDR_TM_2D_TILED_THIN1:
                *pTileMode = ADDR_TM_PRT_2D_TILED_THIN1;
                break;
            case ADDR_TM_2D_TILED_THICK:
                if (degradeThickTile == false)
                {
                    *pTileMode = ADDR_TM_PRT_2D_TILED_THICK;
                }
                else
                {
                    *pTileMode = ADDR_TM_PRT_2D_TILED_THIN1;
                }
                break;
            case ADDR_TM_3D_TILED_THIN1:
                *pTileMode = ADDR_TM_PRT_3D_TILED_THIN1;
                break;
            case ADDR_TM_3D_TILED_THICK:
                if (degradeThickTile == false)
                {
                    *pTileMode = ADDR_TM_PRT_3D_TILED_THICK;
                }
                else
                {
                    *pTileMode = ADDR_TM_PRT_3D_TILED_THIN1;
                }
                break;
            default:
                PAL_ASSERT_ALWAYS();
                break;
            }
        }
    }

    // In GFX6, 7, 8, the only tiling format UVD supports is 2DThin1.

    // Depth-stencil images must be tiled.
    PAL_ASSERT((Parent()->IsDepthStencil() == false) || (*pTileMode != ADDR_TM_LINEAR_ALIGNED));

    return result;
}

// =====================================================================================================================
// Setup bank and pipe swizzling for a subresource.
void Image::SetupBankAndPipeSwizzle(
    uint32                                  subResIdx,
    void*                                   pTileInfoList,
    const ADDR_COMPUTE_SURFACE_INFO_OUTPUT& surfInfo
    ) const
{
    const SubResourceInfo*const pSubResInfo = Parent()->SubresourceInfo(subResIdx);
    AddrMgr1::TileInfo*const    pTileInfo   = AddrMgr1::NonConstTileInfo(pTileInfoList, subResIdx);

    uint32 tileSwizzle = 0;

    // Tile swizzle is only valid for macro tiling modes. Also, it cannot be used with PRT images because the texel
    // layout within each tile must be identical.
    if (IsMacroTiled(pSubResInfo)     &&
        (m_createInfo.flags.prt == 0))
    {
        if (m_pImageInfo->internalCreateInfo.flags.useSharedTilingOverrides)
        {
            // PAL assumes shared images use the same tile swizzle for all subresources.
            tileSwizzle = m_pImageInfo->internalCreateInfo.gfx6.sharedTileSwizzle;
        }
        else if (Parent()->IsPeer())
        {
            // Peer images must have the same tile swizzle as the original image.
            tileSwizzle = AddrMgr1::GetTileInfo(Parent()->OriginalImage(), pSubResInfo->subresId)->tileSwizzle;
        }
        else if (pSubResInfo->subresId.mipLevel == 0)
        {
            // Bank and pipe swizzling is performed only for slice 0. The tile swizzle for the other slices is
            // derived from slice 0 using the AddrLib (this matches hardware behavior).
            if (pSubResInfo->subresId.arraySlice == 0)
            {
                if (m_createInfo.flags.fixedTileSwizzle != 0)
                {
                    // Our base subresource tile swizzle was specified by the client. Note that we only support
                    // this for single-sampled color images, otherwise we'd need to know the base tile swizzle
                    // of every aspect.
                    //
                    // It's possible for us to hang the HW if we use a value computed for a different aspect so we
                    // must return a safe value like the default of zero if the client breaks these rules.
                    if ((pSubResInfo->subresId.aspect == ImageAspect::Color) && (m_createInfo.fragments == 1))
                    {
                        tileSwizzle = m_createInfo.tileSwizzle;
                    }
                    else
                    {
                        PAL_ASSERT_ALWAYS();
                    }
                }
                else
                {
                    // Currently some VCE revisions don't support bank/pipe swizzle but they may still use tile mode,
                    // so we cannot give a non-zero swizzle to the base subresource. After VCE firmware have
                    // implemented the support we can then remove this exception made for YUV format.
                    if (Formats::IsYuv(m_createInfo.swizzledFormat.format) == false)
                    {
                        tileSwizzle = ComputeBaseTileSwizzle(surfInfo, *pSubResInfo);
                    }
                }
            }
            else
            {
                const SubresId                 baseSubres    = { pSubResInfo->subresId.aspect, 0, 0 };
                const AddrMgr1::TileInfo*const pBaseTileInfo = AddrMgr1::GetTileInfo(Parent(), baseSubres);

                PAL_ASSERT(surfInfo.pTileInfo != nullptr);
                ADDR_TILEINFO tileInfo = *(surfInfo.pTileInfo);

                ADDR_COMPUTE_SLICESWIZZLE_INPUT sliceSwizzleIn = { };
                sliceSwizzleIn.size           = sizeof(sliceSwizzleIn);
                sliceSwizzleIn.baseSwizzle    = pBaseTileInfo->tileSwizzle;
                sliceSwizzleIn.baseAddr       = 0;
                sliceSwizzleIn.tileIndex      = surfInfo.tileIndex;
                sliceSwizzleIn.macroModeIndex = surfInfo.macroModeIndex;
                sliceSwizzleIn.tileMode       = surfInfo.tileMode;
                sliceSwizzleIn.slice          = pSubResInfo->subresId.arraySlice;
                sliceSwizzleIn.pTileInfo      = &tileInfo;

                ADDR_COMPUTE_SLICESWIZZLE_OUTPUT sliceSwizzleOut = { };
                sliceSwizzleOut.size = sizeof(sliceSwizzleOut);

                // Call address library
                ADDR_E_RETURNCODE addrRet = AddrComputeSliceSwizzle(m_device.AddrLibHandle(),
                                                                    &sliceSwizzleIn,
                                                                    &sliceSwizzleOut);
                PAL_ASSERT(addrRet == ADDR_OK);

                tileSwizzle = sliceSwizzleOut.tileSwizzle;
            }
        }
        else // mipLevel > 0
        {
            // Bank and pipe swizzling for the lower mips is the same as the most detailed mip level.
            const SubresId baseSubres = { pSubResInfo->subresId.aspect, 0, pSubResInfo->subresId.arraySlice };

            tileSwizzle = AddrMgr1::GetTileInfo(Parent(), baseSubres)->tileSwizzle;
        }
    }

    pTileInfo->tileSwizzle = tileSwizzle;
}

// =====================================================================================================================
// Computes a tile swizzle for this image's base subresource dependent on the image's create info and base subres info.
uint32 Image::ComputeBaseTileSwizzle(
    const ADDR_COMPUTE_SURFACE_INFO_OUTPUT& surfOut,
    const SubResourceInfo&                  subResInfo
    ) const
{
    uint32 tileSwizzle = 0;

    // Presentable/flippable images cannot use tile swizzle because the display engine doesn't support it.
    if ((Parent()->IsPresentable()          == false) &&
        (Parent()->IsFlippable()            == false) &&
        (Parent()->IsPrivateScreenPresent() == false))
    {
        // Only compute a tile swizzle value if it's enabled for this kind of image in the settings.
        const uint32 enableFlags = m_device.Settings().tileSwizzleMode;
        const bool   isEnabled   = ((TestAnyFlagSet(enableFlags, TileSwizzleColor) && Parent()->IsRenderTarget()) ||
                                    (TestAnyFlagSet(enableFlags, TileSwizzleDepth) && Parent()->IsDepthStencil()) ||
                                    (TestAnyFlagSet(enableFlags, TileSwizzleShaderRes) &&
                                     (Parent()->IsShaderReadable() || Parent()->IsShaderWritable())));

        // Gfx8 HW can't use tile-swizzle on depth-stencil surfaces which get read by the texture pipe while
        // compressed. Note, this is not a bug, this is a feature.  :-)  (really!) Similarly, there is another
        // compressed read bug for mip-maps where the swizzle bits are interpreted as an offset when tile mode is
        // switched to 1D. Thus the HW won't support tile swizzle if TC compatible reads are enabled unless the image
        // is a non-depth-target with one mip level.
        const bool supportSwizzle = ((subResInfo.flags.supportMetaDataTexFetch == false) ||
                                     ((Parent()->IsDepthStencil() == false) && (m_createInfo.mipLevels == 1)));

        if (isEnabled && supportSwizzle)
        {
            // We're definitely going to use tile swizzle, but now we need to come up with a surface index for Addrlib.
            uint32 surfaceIndex = 0;

            if (Parent()->IsDepthStencil())
            {
                // The depth-stencil index is fixed to the plane index so it's safe to use it in all cases.
                surfaceIndex = Parent()->GetPlaneFromAspect(subResInfo.subresId.aspect);
            }
            else if (Parent()->IsDataInvariant() || Parent()->IsCloneable())
            {
                // Data invariant and cloneable images must generate identical swizzles given identical create info.
                // This means we can hash the public create info struct to get half-way decent swizzling.
                //
                // Note that one client is not able to guarantee that they consistently set the perSubresInit flag for
                // all images that must be identical so we need to skip over the ImageCreateFlags.
                constexpr size_t HashOffset = offsetof(ImageCreateInfo, usageFlags);
                constexpr uint64 HashSize   = sizeof(ImageCreateInfo) - HashOffset;
                const uint8*     pHashStart = reinterpret_cast<const uint8*>(&m_createInfo) + HashOffset;

                uint64 hash = 0;
                MetroHash64::Hash(
                    pHashStart,
                    HashSize,
                    reinterpret_cast<uint8* const>(&hash));

                surfaceIndex = MetroHash::Compact32(hash);
            }
            else if (Parent()->IsRenderTarget())
            {
                // Give this color target a unique index.
                surfaceIndex = s_cbSwizzleIdx++;
            }
            else
            {
                // Give this shader resource a unique index.
                surfaceIndex = s_txSwizzleIdx++;
            }

            PAL_ASSERT(surfOut.pTileInfo != nullptr);
            ADDR_TILEINFO tileInfo = *(surfOut.pTileInfo);

            ADDR_COMPUTE_BASE_SWIZZLE_INPUT baseSwizzleIn = { };
            baseSwizzleIn.size           = sizeof(baseSwizzleIn);
            baseSwizzleIn.surfIndex      = surfaceIndex;
            baseSwizzleIn.tileMode       = surfOut.tileMode;
            baseSwizzleIn.pTileInfo      = &tileInfo;
            baseSwizzleIn.tileIndex      = surfOut.tileIndex;
            baseSwizzleIn.macroModeIndex = surfOut.macroModeIndex;

            ADDR_COMPUTE_BASE_SWIZZLE_OUTPUT baseSwizzleOut = { };
            baseSwizzleOut.size = sizeof(baseSwizzleOut);

            // Call address library
            ADDR_E_RETURNCODE addrRet = AddrComputeBaseSwizzle(m_device.AddrLibHandle(),
                                                               &baseSwizzleIn,
                                                               &baseSwizzleOut);
            PAL_ASSERT(addrRet == ADDR_OK);

            tileSwizzle = baseSwizzleOut.tileSwizzle;
        }
    }

    return tileSwizzle;
}

// =====================================================================================================================
// Determines if the supplied image needs the XTHICK workaround.
bool Image::ApplyXthickDccWorkaround(
    AddrTileMode     tileMode
    ) const
{
    auto pGfx6Device = static_cast<const Pal::Gfx6::Device*>(m_device.GetGfxDevice());
    bool applyWorkaround = false;

    if ((pGfx6Device->WaEnableDccXthickUse() != false) &&
        (m_createInfo.imageType == ImageType::Tex3d) &&
        // true param assumes resource can be made TC compat since this isn't known for sure until after calling
        // addrlib.
        Gfx6Dcc::UseDccForImage(m_device, *this, tileMode, ADDR_DISPLAYABLE, true))
    {
        applyWorkaround = true;
    }

    return applyWorkaround;
}

// =====================================================================================================================
// Converts an AddrTileType enum to a h/w MICRO_TILE_MODE value
uint32 Image::HwMicroTileModeFromAddrTileType(
    AddrTileType addrType)          // AddrLib defined tile type
{
    // Note that this table is missing ADDR_SURF_THICK_MICRO_TILING__SI but it shouldn't actually be used.
    constexpr uint32 NumAddrTileType = 5;
    constexpr uint32 HwMicroTileTable[NumAddrTileType] =
    {
        ADDR_SURF_DISPLAY_MICRO_TILING,         // ADDR_DISPLAYABLE
        ADDR_SURF_THIN_MICRO_TILING,            // ADDR_NON_DISPLAYABLE
        ADDR_SURF_DEPTH_MICRO_TILING,           // ADDR_DEPTH_SAMPLE_ORDER
        ADDR_SURF_ROTATED_MICRO_TILING__CI__VI, // ADDR_ROTATED
        ADDR_SURF_THICK_MICRO_TILING__CI__VI,   // ADDR_THICK
    };

    PAL_ASSERT(addrType < NumAddrTileType);

    return HwMicroTileTable[addrType];
}

// =====================================================================================================================
// Converts an AddrTileMode enum to a h/w ARRAY_MODE value
uint32 Image::HwArrayModeFromAddrTileMode(
    AddrTileMode addrMode)          // AddrLib tile mode to convert to h/w value
{
    constexpr uint32 UnsupportedHwArrayMode = 0xFFFFFFFF;
    constexpr uint32 HwArrayModeTable[ADDR_TM_COUNT] =
    {
        ARRAY_LINEAR_GENERAL,             // ADDR_TM_LINEAR_GENERAL
        ARRAY_LINEAR_ALIGNED,             // ADDR_TM_LINEAR_ALIGNED
        ARRAY_1D_TILED_THIN1,             // ADDR_TM_1D_TILED_THIN1
        ARRAY_1D_TILED_THICK,             // ADDR_TM_1D_TILED_THICK
        ARRAY_2D_TILED_THIN1,             // ADDR_TM_2D_TILED_THIN1
        ARRAY_2D_TILED_THIN2__SI,         // ADDR_TM_2D_TILED_THIN2
        ARRAY_2D_TILED_THIN4__SI,         // ADDR_TM_2D_TILED_THIN4
        ARRAY_2D_TILED_THICK,             // ADDR_TM_2D_TILED_THICK
        UnsupportedHwArrayMode,           // ADDR_TM_2B_TILED_THIN1
        ARRAY_2B_TILED_THIN2__SI,         // ADDR_TM_2B_TILED_THIN2
        ARRAY_2B_TILED_THIN4__SI,         // ADDR_TM_2B_TILED_THIN4
        ARRAY_2B_TILED_THICK__SI,         // ADDR_TM_2B_TILED_THICK
        ARRAY_3D_TILED_THIN1,             // ADDR_TM_3D_TILED_THIN1
        ARRAY_3D_TILED_THICK,             // ADDR_TM_3D_TILED_THICK
        UnsupportedHwArrayMode,           // ADDR_TM_3B_TILED_THIN1
        UnsupportedHwArrayMode,           // ADDR_TM_3B_TILED_THICK
        ARRAY_2D_TILED_XTHICK,            // ADDR_TM_2D_TILED_XTHICK
        ARRAY_3D_TILED_XTHICK,            // ADDR_TM_3D_TILED_XTHICK
        ARRAY_POWER_SAVE__SI,             // ADDR_TM_POWER_SAVE
        ARRAY_PRT_TILED_THIN1__CI__VI,    // ADDR_TM_PRT_TILED_THIN1
        ARRAY_PRT_2D_TILED_THIN1__CI__VI, // ADDR_TM_PRT_2D_TILED_THIN1
        ARRAY_PRT_3D_TILED_THIN1__CI__VI, // ADDR_TM_PRT_3D_TILED_THIN1
        ARRAY_PRT_TILED_THICK__CI__VI,    // ADDR_TM_PRT_TILED_THICK
        ARRAY_PRT_2D_TILED_THICK__CI__VI, // ADDR_TM_PRT_2D_TILED_THICK
        ARRAY_PRT_3D_TILED_THICK__CI__VI, // ADDR_TM_PRT_3D_TILED_THICK
    };

    PAL_ASSERT(addrMode < ADDR_TM_COUNT);
    PAL_ASSERT(HwArrayModeTable[addrMode] != UnsupportedHwArrayMode);

    return HwArrayModeTable[addrMode];
}

// =====================================================================================================================
// Determines if a subresource has macro tile mode
bool Image::IsMacroTiled(
    const SubResourceInfo* pSubResInfo
    ) const
{
    return IsMacroTiled(GetSubResourceTileMode(pSubResInfo->subresId));
}

// =====================================================================================================================
// Determines if the specified tile mode is a macro tile mode
bool Image::IsMacroTiled(
    AddrTileMode tileMode)
{
    return AddrMgr1::IsMacroTiled(tileMode);
}

// =====================================================================================================================
// Returns the layout-to-state mask for a depth/stencil Image.  This should only ever be called on a depth/stencil
// Image.
const DepthStencilLayoutToState& Image::LayoutToDepthCompressionState(
    const SubresId& subresId
    ) const
{
    return m_layoutToState[subresId.mipLevel].depthStencil[GetDepthStencilStateIndex(subresId.aspect)];
}

// =====================================================================================================================
Image* GetGfx6Image(
    const IImage* pImage)
{
    return static_cast<Pal::Gfx6::Image*>(static_cast<const Pal::Image*>(pImage)->GetGfxImage());
}

// =====================================================================================================================
const Image& GetGfx6Image(
    const IImage& image)
{
    return static_cast<Pal::Gfx6::Image&>(*static_cast<const Pal::Image&>(image).GetGfxImage());
}

// =====================================================================================================================
// Ok, this image is (potentially) going to be the target of a texture fetch.  However, the texture fetch block
// only understands these four fast-clear colors:
//      1) ARGB(0, 0, 0, 0)
//      2) ARGB(1, 0, 0, 0)
//      3) ARGB(0, 1, 1, 1)
//      4) ARGB(1, 1, 1, 1)
//
// So....  If "pColor" corresponds to one of those, we're golden, otherwise, the caller needs to do slow-clears
// for everything.  This function returns whether the incoming clear value is readable.
bool Image::IsFastClearColorMetaFetchable(
    const uint32* pColor
    ) const
{
    const ChNumFormat     format        = m_createInfo.swizzledFormat.format;
    const uint32          numComponents = NumComponents(format);
    const ChannelSwizzle* pSwizzle      = &m_createInfo.swizzledFormat.swizzle.swizzle[0];

    bool   rgbSeen          = false;
    uint32 requiredRgbValue = 0; // not valid unless rgbSeen==true
    bool   isMetaFetchable  = true;

    for (uint32 cmpIdx = 0; ((cmpIdx < numComponents) && isMetaFetchable); cmpIdx++)
    {
        // Get the value of 1 in terms of this component's bit-width / numeric-type
        const uint32 one = TranslateClearCodeOneToNativeFmt(cmpIdx);

        if ((pColor[cmpIdx] != 0) && (pColor[cmpIdx] != one))
        {
            // This channel isn't zero or one, so we can't fast clear
            isMetaFetchable = false;
        }
        else
        {
            switch (pSwizzle[cmpIdx])
            {
            case ChannelSwizzle::W:
                // All we need here is a zero-or-one value, which we already verified above.
                break;

            case ChannelSwizzle::X:
            case ChannelSwizzle::Y:
            case ChannelSwizzle::Z:
                if (rgbSeen == false)
                {
                    // Don't go down this path again.
                    rgbSeen = true;

                    // This is the first r-g-b value that we've come across, and it's a known zero-or-one value.
                    // All future RGB values need to match this one, so just record this value for comparison
                    // purposes.
                    requiredRgbValue = pColor[cmpIdx];
                }
                else if (pColor[cmpIdx] != requiredRgbValue)
                {
                    // Fast clear is a no-go.
                    isMetaFetchable = false;
                }
                break;

            default:
                // We don't really care about the non-RGBA channels.  It's either going to be zero or one, which
                // suits our purposes just fine.  :-)
                break;
            } // end switch on the component select
        }
    } // end loop through all the components of this format

    return isMetaFetchable;
}

// =====================================================================================================================
// Calculates a base_256b address for a subresource with swizzle OR'ed
uint32 Image::GetSubresource256BAddrSwizzled(
    SubresId subresource
    ) const
{
    const AddrMgr1::TileInfo*const pTileInfo = AddrMgr1::GetTileInfo(Parent(), subresource);
    return Get256BAddrSwizzled(Parent()->GetSubresourceBaseAddr(subresource), pTileInfo->tileSwizzle);
}

// =====================================================================================================================
// Initializes the metadata in the given subresource range using CmdFillMemory calls.
//
// Note that pCmdBuffer may not be a GfxCmdBuffer.
void Image::InitMetadataFill(
    Pal::CmdBuffer*    pCmdBuffer,
    const SubresRange& range
    ) const
{
    const auto& boundMem    = m_pParent->GetBoundGpuMemory();
    const bool  is3dImage   = (m_createInfo.imageType == ImageType::Tex3d);
    const bool  hasMetadata = (HasHtileData() || HasCmaskData() || HasFmaskData() || HasDccData());

    if ((pCmdBuffer->GetEngineType() != EngineTypeDma) && hasMetadata)
    {
        pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    }

    if (HasHtileData())
    {
        // This function does not support separate aspect metadata initialization, as initializing either aspect
        // overwrites the entire HTile value.
        PAL_ASSERT((RequiresSeparateAspectInit() == false) ||
                   (GetHtile(range.startSubres)->GetHtileContents() != HtileContents::DepthStencil));

        const uint32 lastMip = range.startSubres.mipLevel + range.numMips - 1;
        for (uint32 mip = range.startSubres.mipLevel; mip <= lastMip; ++mip)
        {
            const auto& htile = m_pHtile[mip];

            // If this is the stencil aspect initialization pass and this hTile buffer doesn't support stencil then
            // there's nothing to do.
            if ((range.startSubres.aspect != ImageAspect::Stencil) || (htile.TileStencilDisabled() == false))
            {
                GpuMemory*  pGpuMemory = nullptr;
                gpusize     dstOffset  = 0;
                gpusize     fillSize   = 0;

                GetHtileBufferInfo(mip,
                                   range.startSubres.arraySlice,
                                   range.numSlices,
                                   HtileBufferUsage::Init,
                                   &pGpuMemory,
                                   &dstOffset,
                                   &fillSize);

                pCmdBuffer->CmdFillMemory(*boundMem.Memory(), dstOffset, fillSize, htile.GetInitialValue());
            }
        }
    }
    else
    {
        if (HasCmaskData())
        {
            const uint32 value = Gfx6Cmask::GetInitialValue(*this);

            const uint32 lastMip = range.startSubres.mipLevel + range.numMips - 1;
            for (uint32 mip = range.startSubres.mipLevel; mip <= lastMip; ++mip)
            {
                const SubresId         mipSubres  = { ImageAspect::Color, mip, 0 };
                const SubResourceInfo& subResInfo = *Parent()->SubresourceInfo(mipSubres);

                // For 3D Images, always init all depth slices of this mip level, otherwise use the range's slice info.
                const uint32 baseSlice = (is3dImage ? 0                             : range.startSubres.arraySlice);
                const uint32 numSlices = (is3dImage ? subResInfo.extentTexels.depth : range.numSlices);

                const auto&   cmask  = m_pCmask[mip];
                const gpusize offset = boundMem.Offset() + cmask.MemoryOffset() + cmask.SliceSize() * baseSlice;
                const gpusize size   = cmask.SliceSize() * numSlices;

                pCmdBuffer->CmdFillMemory(*boundMem.Memory(), offset, size, value);
            }
        }

        if (HasFmaskData())
        {
            // Note that there can only be one FMask mip level.
            const Gfx6Fmask& fmask  = m_pFmask[0];
            const gpusize    offset = boundMem.Offset() + fmask.MemoryOffset() +
                                      fmask.SliceSize() * range.startSubres.arraySlice;
            const gpusize    size   = fmask.SliceSize() * range.numSlices;

            pCmdBuffer->CmdFillMemory(*boundMem.Memory(), offset, size, Gfx6Fmask::GetPackedExpandedValue(*this));
        }

        if (HasDccData())
        {
            // For 3D Images, always init all depth slices of this mip level (as its DCC memory is not "sliced" at
            // creation time, specifying baseSlice = 0, numSlices = 1 is enough). Otherwise use the range info.
            const uint32 baseSlice = (is3dImage ? 0 : range.startSubres.arraySlice);
            const uint32 numSlices = (is3dImage ? 1 : range.numSlices);

            const uint32 lastMip = range.startSubres.mipLevel + range.numMips - 1;
            for (uint32 mip = range.startSubres.mipLevel; mip <= lastMip; ++mip)
            {
                // Assume we will initialize all slices, adjust the offset and size if we aren't.
                const auto& dcc    = m_pDcc[mip];
                gpusize     offset = boundMem.Offset() + dcc.MemoryOffset();
                gpusize     size   = dcc.TotalSize();

                if (numSlices < m_createInfo.arraySize)
                {
                    // The perSubresInit must be specified to support this case. Note that we do not have to worry about
                    // unaligned slice sizes because we will disable DCC in those cases when perSubresInit is set.
                    PAL_ASSERT(m_createInfo.flags.perSubresInit == 1);

                    size    = (dcc.SliceSize() * numSlices);
                    offset += (dcc.SliceSize() * baseSlice);
                }

                pCmdBuffer->CmdFillMemory(*boundMem.Memory(), offset, size, Gfx6Dcc::InitialValue);
            }
        }
    }

    if (HasFastClearMetaData())
    {
        // The DB Tile Summarizer requires a TC compatible clear value of stencil,
        // because TC isn't aware of DB_STENCIL_CLEAR register.
        // Please note the clear value of color or depth is also initialized together,
        // although it might be unnecessary.
        pCmdBuffer->CmdFillMemory(*boundMem.Memory(),
                                  FastClearMetaDataOffset(range.startSubres.mipLevel),
                                  FastClearMetaDataSize(range.numMips),
                                  0);
    }

    if ((pCmdBuffer->GetEngineType() != EngineTypeDma) && hasMetadata)
    {
        pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
    }
}

// =====================================================================================================================
// Returns true if the given aspect supports decompress operations on the compute queue
bool Image::SupportsComputeDecompress(
    const SubresId& subresId
    ) const
{
    const auto& layoutToState = m_layoutToState[subresId.mipLevel];
    const uint32 engines = (m_pParent->IsDepthStencil()
                           ? layoutToState.depthStencil[GetDepthStencilStateIndex(subresId.aspect)].compressed.engines
                           : layoutToState.color.compressed.engines);

    return TestAnyFlagSet(engines, LayoutComputeEngine);
}

// =====================================================================================================================
// Fillout shared metadata information.
void Image::GetSharedMetadataInfo(
    SharedMetadataInfo* pMetadataInfo
    ) const
{
    memset(pMetadataInfo, 0, sizeof(SharedMetadataInfo));

    if (m_pDcc != nullptr)
    {
        pMetadataInfo->dccOffset = m_pDcc->MemoryOffset();
    }
    if (m_pCmask != nullptr)
    {
        pMetadataInfo->cmaskOffset = m_pCmask->MemoryOffset();
    }
    if (m_pFmask != nullptr)
    {
        pMetadataInfo->fmaskOffset                = m_pFmask->MemoryOffset();
        pMetadataInfo->flags.shaderFetchableFmask = IsComprFmaskShaderReadable(m_pParent->SubresourceInfo(0));
    }
    if (m_pHtile != nullptr)
    {
        pMetadataInfo->htileOffset               = m_pHtile->MemoryOffset();
        pMetadataInfo->flags.hasWaTcCompatZRange = HasWaTcCompatZRangeMetaData();
    }
    pMetadataInfo->flags.shaderFetchable = Parent()->SubresourceInfo(0)->flags.supportMetaDataTexFetch;

    pMetadataInfo->dccStateMetaDataOffset           = m_dccStateMetaDataOffset;
    pMetadataInfo->fastClearMetaDataOffset          = m_fastClearMetaDataOffset;
    pMetadataInfo->fastClearEliminateMetaDataOffset = m_fastClearEliminateMetaDataOffset;
}

} // Gfx6
} // Pal
