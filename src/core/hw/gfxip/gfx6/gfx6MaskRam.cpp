/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx6/g_gfx6PalSettings.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6FormatInfo.h"
#include "core/hw/gfxip/gfx6/gfx6Image.h"
#include "core/hw/gfxip/gfx6/gfx6MaskRam.h"
#include "core/addrMgr/addrMgr1/addrMgr1.h"
#include "palMath.h"

#include <limits.h>

using namespace Util;
using namespace Pal::Formats;
using namespace Pal::Formats::Gfx6;
using namespace Pal::AddrMgr1;

namespace Pal
{
namespace Gfx6
{

// Packed version of fully expanded FMASK value. This should be used by ClearFmask.
constexpr uint64 PackedFmaskExpandedValues[MaxLog2AaFragments + 1][MaxLog2AaSamples + 1] =
{
    // Fragment counts down the right, sample counts along the top. Note: 1 fragment/1 sample is invalid.
    // 1   2                   4                   8                   16
    { 0x0, 0x0202020202020202, 0x0E0E0E0E0E0E0E0E, 0xFEFEFEFEFEFEFEFE, 0xFFFEFFFEFFFEFFFE }, // 1
    { 0x0, 0x0202020202020202, 0xA4A4A4A4A4A4A4A4, 0xAAA4AAA4AAA4AAA4, 0xAAAAAAA4AAAAAAA4 }, // 2
    { 0x0, 0x0               , 0xE4E4E4E4E4E4E4E4, 0x4444321044443210, 0x4444444444443210 }, // 4
    { 0x0, 0x0               , 0x0,                0x7654321076543210, 0x8888888876543210 }  // 8
};

//=============== Implementation for Gfx6Htile: ========================================================================

// =====================================================================================================================
// Determimes if the given Image object should use HTILE metadata.
bool Gfx6Htile::UseHtileForImage(
    const Pal::Device& device,
    const Image&       image,
    bool               metaDataTexFetchSupported)
{
    const Gfx6PalSettings& settings        = GetGfx6Settings(device);
    const Pal::Image*const pParent         = image.Parent();

    bool useHtile = false;

    // Htile will only ever be used for depth stencil images.
    if (pParent->IsDepthStencilTarget())
    {
        if (pParent->GetInternalCreateInfo().flags.useSharedMetadata)
        {
            const auto& metadata = pParent->GetInternalCreateInfo().sharedMetadata;
            useHtile = (metadata.htileOffset != 0) && (metadata.fastClearMetaDataOffset[0] != 0);
        }
        else
        {
            const ImageCreateInfo  createInfo      = pParent->GetImageCreateInfo();
            const bool             supportsStencil = device.SupportsStencil(createInfo.swizzledFormat.format,
                                                                            createInfo.tiling);

            // The waTcCompatZRange workaround requires tileStencilDisable = 0 for TC-compatible images. However, images
            // with both depth and stencil, per-subresource initialization, and separate plane initialization require
            // tileStencilDisable = 1 if the metadata planes cannot be initialized separately. If all of these things are
            // true, we must report that we cannot use Htile, which will result in TC-compatibility being disabled for the
            // image but will still allow us to use Htile.
            const bool waDisableHtile = (device.GetGfxDevice()->WaTcCompatZRange()                          &&
                                         metaDataTexFetchSupported                                          &&
                                         (ExpectedHtileContents(device, image) == HtileContents::DepthOnly) &&
                                         supportsStencil);

            // Disabling Htile for this type of image could potentially cause performance issues for the apps using them.
            PAL_ALERT(waDisableHtile);

            useHtile = ((pParent->IsShared()                   == false) &&
                        (pParent->IsMetadataDisabledByClient() == false) &&
                        (pParent->IsTmz()                      == false) &&
                        (settings.htileEnable                  == true)  &&
                        (waDisableHtile                        == false));
        }
    }

    return useHtile;
}

// =====================================================================================================================
Gfx6Htile::Gfx6Htile()
    :
    MaskRam()
{
    m_flags.value             = 0;
    m_dbHtileSurface.u32All   = 0;
    m_dbPreloadControl.u32All = 0;
    m_htileContents           = HtileContents::DepthStencil;
}

// =====================================================================================================================
// Initializes this HTile object for the given Image and mipmap level.
Result Gfx6Htile::Init(
    const Pal::Device& device,
    const Image&       image,
    uint32             mipLevel,
    gpusize*           pGpuOffset)    // [in,out] Current GPU memory offset & size
{
    const Gfx6PalSettings& settings = GetGfx6Settings(device);

    // Determine the subResource ID of the base slice for this mipmap level:
    Pal::SubresId subresource = { 0, mipLevel, 0 };

    m_htileContents = ExpectedHtileContents(device, image);

    m_flags.compressZ = settings.depthCompressEnable;
    m_flags.compressS = settings.stencilCompressEnable;

    Pal::SubresId baseSubresource = subresource;
    baseSubresource.mipLevel = 0;

    const SubResourceInfo*const pSubResInfo     = image.Parent()->SubresourceInfo(subresource);
    const SubResourceInfo*const pBaseSubResInfo = image.Parent()->SubresourceInfo(baseSubresource);

    // Are any of the miplevels in this chain going to be texture-fetched?
    if (pBaseSubResInfo->flags.supportMetaDataTexFetch != 0)
    {
        // Yes, is this specific sub-resource going to be texture-fetched?  This is conceivably going to be different
        // from the base-sub-resource result due to the two sub-resources having differing tile modes.
        if (pSubResInfo->flags.supportMetaDataTexFetch != 0)
        {
            // Yes, so allow the texture block to read from it
            m_dbHtileSurface.bits.TC_COMPATIBLE__VI = 1;
        }
        else
        {
            // This sub-resource's hTile memory is only texture-compatible if we disable all compression.
            m_flags.compressZ = false;
            m_flags.compressS = false;
        }
    }

    // NOTE: Default ZRANGE_PRECISION to 1, since this is typically the optimal value for DX applications, since they
    // usually clear Z to 1.0f and use a < depth comparison for their depth testing. We want more precision on the far
    // Z plane. (We assume the same holds for PAL apps).
    m_flags.zrangePrecision = 1;

    if (device.GetGfxDevice()->WaTcCompatZRange() && (pBaseSubResInfo->flags.supportMetaDataTexFetch != 0))
    {
        // We must set DB_STENCIL_INFO.TILE_STENCIL_DISABLE to 0 for the waTcCompatZRange workaround, even if the image
        // does not contain stencil data.
        m_flags.tileStencilDisable = 0;
    }
    else if (m_htileContents == HtileContents::DepthOnly)
    {
        // If this Image's format does not contain stencil data, allow the HW to use the extra HTile bits for improved
        // HiZ Z-range precision.
        m_flags.tileStencilDisable = 1;
    }

    const uint32 activeRbCount     = device.ChipProperties().gfx6.numActiveRbs;
    const uint32 imageSizeInPixels = (pSubResInfo->actualExtentTexels.width * pSubResInfo->actualExtentTexels.height);
    const uint32 pixelsPerRb       = (imageSizeInPixels / activeRbCount);

    // NOTE: These values come from the SI DB programming guide.
    if (pixelsPerRb <= (256 * 1024)) // <= 256K pixels
    {
        m_dbHtileSurface.bits.FULL_CACHE = 0;
        m_dbHtileSurface.bits.LINEAR     = settings.linearHtileEnable;
    }
    else if (pixelsPerRb <= (512 * 1024)) // <= 512K pixels
    {
        m_dbHtileSurface.bits.FULL_CACHE = 1;
        m_dbHtileSurface.bits.LINEAR     = settings.linearHtileEnable;
    }
    else // > 512K pixels
    {
        m_dbHtileSurface.bits.FULL_CACHE = 1;
        m_dbHtileSurface.bits.LINEAR     = 0;
    }

    // NOTE: Linear layout HTILE of 1D tiling depth image does not aligned enough thus the address of tile bits may lie
    // across the slice size boundaries. So per subresource fast clear with CS, which clears HTILE directly, may not
    // work as expected. Using gfx fast depth clear may relieve this restriction.
    if (image.IsMacroTiled(pSubResInfo) == false)
    {
        m_dbHtileSurface.bits.LINEAR = 0;
    }

    m_dbHtileSurface.bits.PREFETCH_WIDTH          = 0;
    m_dbHtileSurface.bits.PREFETCH_HEIGHT         = 0;
    m_dbHtileSurface.bits.DST_OUTSIDE_ZERO_TO_ONE = 0;

    auto pGfx6Device = static_cast<const Pal::Gfx6::Device*>(device.GetGfxDevice());
    if (settings.dbPreloadEnable &&
        // If this device doesn't require the workaround, then preload can be used
        ((pGfx6Device->WaDbTcCompatFlush() == Gfx8TcCompatDbFlushWaNever) ||
         // Devices that require the workaround can't use preload on depth images that support
         // texture fetches of compressed data.
         (pSubResInfo->flags.supportMetaDataTexFetch == false)))
    {
        m_dbHtileSurface.bits.HTILE_USES_PRELOAD_WIN = settings.dbPreloadWinEnable;
        m_dbHtileSurface.bits.PRELOAD                = 1;

        // NOTE: For preloading to be enabled efficiently, the DB_PRELOAD_CONTROL register needs to be set-up. The ideal
        // setting is the largest rectangle of the Image's aspect ratio which can completely fit within the DB cache
        // (centered in the Image). The preload rectangle doesn't need to be exact.

        const uint32 cacheSizeInPixels = (DbHtileCacheSizeInPixels * activeRbCount);
        const uint32 width             = pSubResInfo->extentTexels.width;
        const uint32 height            = pSubResInfo->extentTexels.height;

        // DB Preload window is in 64 pixel increments both horizontally & vertically.
        constexpr uint32 BlockWidth  = 64;
        constexpr uint32 BlockHeight = 64;

        if (imageSizeInPixels <= cacheSizeInPixels)
        {
            // The entire Image fits into the DB cache!
            m_dbPreloadControl.bits.START_X = 0;
            m_dbPreloadControl.bits.START_Y = 0;
            m_dbPreloadControl.bits.MAX_X   = ((width  - 1) / BlockWidth);
            m_dbPreloadControl.bits.MAX_Y   = ((height - 1) / BlockHeight);
        }
        else
        {
            // Image doesn't fit into the DB cache; compute the largest centered rectangle, while preserving the
            // Image's aspect ratio.
            //
            // From DXX:
            //      w*h = cacheSize, where w = aspectRatio*h
            // Thus,
            //      aspectRatio*(h^2) = cacheSize
            // so,
            //      h = sqrt(cacheSize/aspectRatio)
            const float ratio = static_cast<float>(width) / static_cast<float>(height);

            // Compute the height in blocks first; assume there will be more width than height, giving the width
            // decision a lower granularity, and by doing it second typically more cache will be utilized.
            const uint32 preloadWinHeight = static_cast<uint32>(Math::Sqrt(cacheSizeInPixels / ratio));
            // Round up, but not beyond the window size.
            const uint32 preloadWinHeightInBlocks = Min((preloadWinHeight + BlockHeight - 1) / BlockHeight,
                                                         height / BlockHeight);

            // Accurate width can now be derived from the height.
            const uint32 preloadWinWidth = Min(cacheSizeInPixels / (preloadWinHeightInBlocks * BlockHeight), width);
            // Round down, to ensure that the size is smaller than the DB cache.
            const uint32 preloadWinWidthInBlocks = (preloadWinWidth / BlockWidth);

            PAL_ASSERT(cacheSizeInPixels >=
                (preloadWinWidthInBlocks * BlockWidth * preloadWinHeightInBlocks * BlockHeight));

            // Program the preload window, offsetting the preloaded area towards the middle of the Image. Round down
            // to ensure the area is positioned partially outside the Image. (Rounding to nearest would position the
            // rectangle more evenly, but would not guarantee the whole rectangle is inside the Image.)
            m_dbPreloadControl.bits.START_X = ((width - preloadWinWidthInBlocks * BlockWidth) / 2) / BlockWidth;
            m_dbPreloadControl.bits.START_Y = ((height - preloadWinHeightInBlocks * BlockHeight) / 2) / BlockHeight;
            m_dbPreloadControl.bits.MAX_X   = (m_dbPreloadControl.bits.START_X + preloadWinWidthInBlocks);
            m_dbPreloadControl.bits.MAX_Y   = (m_dbPreloadControl.bits.START_Y + preloadWinHeightInBlocks);
        }
    }

    // Call the address library to compute the HTile properties.
    HtileInfo htileInfo = {};
    Result result = ComputeHtileInfo(device,
                                     image,
                                     (*pSubResInfo),
                                     m_dbHtileSurface.bits.LINEAR,
                                     false,
                                     &htileInfo);
    if (result == Result::Success)
    {
        m_totalSize               = htileInfo.maskSize;
        m_sliceSize               = htileInfo.sliceSize;
        m_alignment               = htileInfo.baseAlign;
        m_flags.slicesInterleaved = htileInfo.slicesInterleaved;

        const uint32 lastMipLevel = (image.Parent()->GetImageCreateInfo().mipLevels - 1);
        if ((settings.gfx8IgnoreMipInterleave == false)    &&
            (m_dbHtileSurface.bits.TC_COMPATIBLE__VI == 1) &&
            (htileInfo.nextMipLevelCompressible == false)  &&
            (mipLevel != lastMipLevel))
        {
            // Once mipInterleave detected, we will pad htile size of all child mips into htile of first
            // affected mip, since tc reading following child mip might reference interleaved htile in the
            // first affected mip.
            // Be careful that it's possible that m_totalSize != m_sliceSize * numSlize.
            // The addtional padded htile is only required to be set to expanded state at init time.
            // More details please see gfx6::Image::GetHtileBufferInfo.
            for (uint32 childMip = mipLevel + 1; childMip <= lastMipLevel; ++childMip)
            {
                HtileInfo childHtileInfo = {};
                Pal::SubresId childSubRes = subresource;
                childSubRes.mipLevel = childMip;

                const SubResourceInfo*const pChildSubResIno = image.Parent()->SubresourceInfo(childSubRes);

                // Tc-compatible flag shall be set to 1 for interleaved child mips.
                result = ComputeHtileInfo(device,
                                          image,
                                          (*pChildSubResIno),
                                          m_dbHtileSurface.bits.LINEAR,
                                          true,
                                          &childHtileInfo);

                m_totalSize += childHtileInfo.maskSize;
            }

            // After padding, total htile size shall be aligned with respect the alignment of current mip, since
            // addrLib might not perform the alignment for child mips and tc-compatible htile accessing by texture
            // engine requires the alignment.
            m_totalSize = Pow2Align(m_totalSize, m_alignment);

            m_flags.firstInterleavedMip = 1;
        }

        // Compute our aligned GPU memory offset and update the caller-provided running total.
        UpdateGpuMemOffset(pGpuOffset);
    }

    return result;
}

// =====================================================================================================================
// Computes a value for updating the HTile buffer for a fast depth clear.
uint32 Gfx6Htile::GetClearValue(
    float depthValue
    ) const
{
    // Maximum 14-bit UINT value.
    constexpr uint32 MaxZVal = 0x3FFF;

    // For clears, Zmask and Smem will always be set to zero.
    constexpr uint32 ZMask = 0;
    constexpr uint32 SMem  = 0;

    // Convert depthValue to 14-bit zmin/zmax uint values:
    const uint32 zMin = static_cast<uint32>((depthValue * MaxZVal) + 0.5f);
    const uint32 zMax = zMin;

    uint32 htileValue;

    if (TileStencilDisabled() == false)
    {
        // If stencil is present, each HTILE is laid out as-follows, according to the DB spec:
        // |31       12|11 10|9    8|7   6|5   4|3     0|
        // +-----------+-----+------+-----+-----+-------+
        // |  Z Range  |     | SMem | SR1 | SR0 | ZMask |

        // The base value for zRange is either zMax or zMin, depending on ZRANGE_PRECISION. For a fast clear,
        // zMin == zMax == clearValue. This means that the base will always be the clear value (converted to 14-bit
        // UINT).
        //
        // When abs(zMax-zMin) < 16, the delta is equal to the difference. In the case of fast clears, where
        // zMax == zMin, the delta is always zero.
        constexpr uint32 Delta = 0;
        const uint32 zRange    = ((zMax << 6) | Delta);

        // SResults 0 & 1 are set based on the stencil compare state.
        // For fast-clear, the default value of sr0 and sr1 are both 0x3.
        constexpr uint32 SResults = 0xf;

        htileValue = ( ((zRange   & 0xFFFFF) << 12) |
                       ((SMem     &     0x3) <<  8) |
                       ((SResults &     0xF) <<  4) |
                       ((ZMask    &     0xF) <<  0) );
    }
    else
    {
        // If stencil is absent, each HTILE is laid out as follows, according to the DB spec:
        // |31     18|17      4|3     0|
        // +---------+---------+-------+
        // |  Max Z  |  Min Z  | ZMask |

        htileValue = ( ((zMax  & 0x3FFF) << 18) |
                       ((zMin  & 0x3FFF) <<  4) |
                       ((ZMask &    0xF) <<  0) );
    }

    return htileValue;
}

// =====================================================================================================================
// Computes a mask for updating the specified planes of the HTile buffer
uint32 Gfx6Htile::GetPlaneMask(
    uint32 planeFlags
    ) const
{
    uint32 htileMask;

    if (TileStencilDisabled() == false)
    {
        const bool updateDepth   = TestAnyFlagSet(planeFlags, HtilePlaneDepth);
        const bool updateStencil = TestAnyFlagSet(planeFlags, HtilePlaneStencil);

        if ((updateDepth && updateStencil) || (m_htileContents == HtileContents::DepthOnly))
        {
            htileMask = UINT_MAX;
        }
        else if(updateDepth)
        {
            // Only update the HTile bits used to encode depth compression.
            htileMask = Gfx6HtileDepthMask;
        }
        else
        {
            htileMask = Gfx6HtileStencilMask;
        }
    }
    else
    {
        // Always update the entire HTile for depth-only Images.
        htileMask = UINT_MAX;
    }

    return htileMask;
}

// =====================================================================================================================
// A helper function for when the caller just wants the plane mask for a single image plane.
uint32 Gfx6Htile::GetPlaneMask(
    const Image&       image,
    const SubresRange& range
    ) const
{
    PAL_ASSERT(range.numPlanes == 1);
    PAL_ASSERT(image.Parent()->IsDepthStencilTarget());

    uint32 htileMask = image.Parent()->IsDepthPlane(range.startSubres.plane) ? HtilePlaneDepth : HtilePlaneStencil;

    return GetPlaneMask(htileMask);
}

// =====================================================================================================================
// Calls into AddrLib to compute HTILE info for a subresource
Result Gfx6Htile::ComputeHtileInfo(
    const Pal::Device&     device,
    const Image&           image,
    const SubResourceInfo& subResInfo,
    bool                   isLinear,
    bool                   mipInterleavedChildMip,
    HtileInfo*             pHtileInfo    // [out] HTILE info struct
    ) const
{
    const AddrMgr1::TileInfo*const pTileInfo = AddrMgr1::GetTileInfo(image.Parent(), subResInfo.subresId);

    Result result = Result::ErrorInitializationFailed;

    ADDR_COMPUTE_HTILE_INFO_INPUT addrHtileIn = {};
    addrHtileIn.size               = sizeof(addrHtileIn);
    addrHtileIn.tileIndex          = pTileInfo->tileIndex;
    addrHtileIn.macroModeIndex     = pTileInfo->macroModeIndex;
    addrHtileIn.pitch              = subResInfo.actualExtentTexels.width;
    addrHtileIn.height             = subResInfo.actualExtentTexels.height;
    addrHtileIn.numSlices          = MaskRamSlices((*image.Parent()), subResInfo);
    addrHtileIn.isLinear           = isLinear;

    // mipInterleavedChildMip = 1 denotes that htile calculation is for htile of child mips padded in htile of the
    // first mip-interleaved mip. So we ought to compute tcCompatible htile size in this case since the first
    // mip-interleaved mip will always be tc-compatible. Moreover, AddrLib will align htileSize to 256xBankxPipe
    // by default. Such padding is unecessary here since mip-interleaved child mips will be layouted and padded
    // together(padding will be performed after the last mip). We could set skipTcCompatSizeAlign to 1 to skip
    // unecessary padding.
    addrHtileIn.flags.tcCompatible = mipInterleavedChildMip ? 1 : subResInfo.flags.supportMetaDataTexFetch;
    addrHtileIn.flags.skipTcCompatSizeAlign = mipInterleavedChildMip;

    // HTILE block size is always 8x8.
    addrHtileIn.blockWidth     = ADDR_HTILE_BLOCKSIZE_8;
    addrHtileIn.blockHeight    = ADDR_HTILE_BLOCKSIZE_8;

    ADDR_COMPUTE_HTILE_INFO_OUTPUT addrHtileOut = {};
    addrHtileOut.size = sizeof(addrHtileOut);

    ADDR_E_RETURNCODE addrRet = AddrComputeHtileInfo(device.AddrLibHandle(), &addrHtileIn, &addrHtileOut);
    PAL_ASSERT(addrRet == ADDR_OK);

    if (addrRet == ADDR_OK)
    {
        pHtileInfo->sliceSize                = addrHtileOut.sliceSize;
        pHtileInfo->maskSize                 = addrHtileOut.htileBytes;
        pHtileInfo->baseAlign                = addrHtileOut.baseAlign;
        pHtileInfo->blockSize                = 0; // Not needed for HTILE.
        pHtileInfo->slicesInterleaved        = (addrHtileOut.sliceInterleaved == 1);
        pHtileInfo->nextMipLevelCompressible = (addrHtileOut.nextMipLevelCompressible == 1);

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Computes the initial value of the htile which depends on whether or not tile stencil is disabled. We want this
// initial value to disable all HTile-based optimizations so that the image is in a trivially valid state. This should
// work well for inits and also for "fast" resummarize blits where we just want the HW to see the base data values.
uint32 Gfx6Htile::GetInitialValue() const
{
    constexpr uint32 Uint14Max = 0x3FFF; // Maximum value of a 14bit integer.

    // Convert the trivial z bounds to 14-bit zmin/zmax uint values. These values will give us HiZ bounds that cover
    // all Z values, effectively disabling HiZ.
    constexpr uint32 ZMin  = 0;
    constexpr uint32 ZMax  = Uint14Max;
    constexpr uint32 ZMask = 0xf;        // No Z compression.

    uint32 initialValue;

    if (TileStencilDisabled())
    {
        // Z only (no stencil):
        //      |31     18|17      4|3     0|
        //      +---------+---------+-------+
        //      |  Max Z  |  Min Z  | ZMask |

        initialValue = (((ZMax  & Uint14Max) << 18) |
                        ((ZMin  & Uint14Max) <<  4) |
                        ((ZMask &       0xF) <<  0));
    }
    else
    {
        // Z and stencil:
        //      |31       12|11 10|9    8|7   6|5   4|3     0|
        //      +-----------+-----+------+-----+-----+-------+
        //      |  Z Range  |     | SMem | SR1 | SR0 | ZMask |

        // The base value for zRange is either zMax or zMin, depending on ZRANGE_PRECISION. Currently, PAL programs
        // ZRANGE_PRECISION to 1 (zMax is the base) by default. Sometimes we switch to 0 if we detect a fast-clear to
        // Z = 0 but that will rewrite HTile so we can ignore that case when we compute our initial value.
        //
        // zRange is encoded as follows: the high 14 bits are the base z value (zMax in our case). The low 6 bits
        // are a code represending the abs(zBase - zOther). In our case, we need to select a delta code representing
        // abs(zMax - zMin), which is always 0x3FFF (maximum 14 bit uint value). The delta code in our case would be
        // 0x3F (all 6 bits set).
        constexpr uint32 Delta  = 0x3F;
        constexpr uint32 ZRange = ((ZMax << 6) | Delta);
        constexpr uint32 SMem   = 0x3; // No stencil compression.
        constexpr uint32 SR1    = 0x3; // Unknown stencil test result.
        constexpr uint32 SR0    = 0x3; // Unknown stencil test result.

        initialValue = (((ZRange & 0xFFFFF) << 12) |
                        ((SMem   &     0x3) <<  8) |
                        ((SR1    &     0x3) <<  6) |
                        ((SR0    &     0x3) <<  4) |
                        ((ZMask  &     0xF) <<  0));
    }

    return initialValue;
}

// =====================================================================================================================
// Determines which planes of Htile are meaningful
HtileContents Gfx6Htile::ExpectedHtileContents(
    const Pal::Device& device,
    const Image&       image)
{
    const Gfx6PalSettings& settings   = GetGfx6Settings(device);
    const ImageCreateInfo& createInfo = image.Parent()->GetImageCreateInfo();
    const bool supportsDepth          = device.SupportsDepth(createInfo.swizzledFormat.format, createInfo.tiling);
    const bool supportsStencil        = device.SupportsStencil(createInfo.swizzledFormat.format, createInfo.tiling);

    HtileContents contents;

    // In the GFX6-8 HW architecture, depth and stencil data share hTile data. Therefore, if separate plane metadata
    // initialization is not enabled, initializing one plane will blow away whatever data is already present in the
    // other plane. Therefore, if the image has
    //    1) Depth data -and-
    //    2) Stencil data -and-
    //    3) Per-subresource initialization -and-
    //    4) Separate init passes for the depth and stencil planes,
    // we must either support separate plane metadata intialization or disable the stencil portion of hTile.
    if (supportsDepth   &&
        supportsStencil &&
        (settings.enableSeparatePlaneMetadataInit ||
         (image.RequiresSeparateDepthPlaneInit() == false)))
    {
        contents = HtileContents::DepthStencil;
    }
    else if (supportsDepth)
    {
        contents = HtileContents::DepthOnly;
    }
    else
    {
        PAL_ASSERT(supportsStencil);

        contents = HtileContents::StencilOnly;
    }

    return contents;
}

//=============== Implementation for Gfx6Cmask: ====================================================

// =====================================================================================================================
// Determines if the given Image object should use CMask metadata.
bool Gfx6Cmask::UseCmaskForImage(
    const Pal::Device& device,
    const Image&       image,
    bool               useDcc)     // true if this image will have a DCC surface
{
    const Pal::Image*const pParent = image.Parent();

    bool useCmask = false;

    if (pParent->GetInternalCreateInfo().flags.useSharedMetadata)
    {
        useCmask = (pParent->GetInternalCreateInfo().sharedMetadata.cmaskOffset != 0);
    }
    else if (pParent->IsRenderTarget()      &&
             (pParent->IsShared() == false) &&
             (pParent->IsMetadataDisabledByClient() == false))
    {
        if (pParent->GetImageCreateInfo().samples > 1)
        {
            // Multisampled Images require CMask.
            useCmask = true;
        }
        else
        {
            const auto             pPalSettings = device.GetPublicSettings();
            const ImageCreateInfo& createInfo = pParent->GetImageCreateInfo();

            // We just care about the tile mode of the base subresource
            const Pal::SubresId subResource = {};
            const AddrTileMode tileMode     = image.GetSubResourceTileMode(subResource);
            const AddrTileType tileType     = image.GetSubResourceTileType(subResource);

            // Avoid using CMasks for small surfaces, where the CMask would be too large relative
            // to the plain resource.
            const bool skipSmallSurface = ((createInfo.extent.width * createInfo.extent.height) <=
                                          (pPalSettings->hintDisableSmallSurfColorCompressionSize *
                                           pPalSettings->hintDisableSmallSurfColorCompressionSize));

            // Single-sampled Images require CMask if fast color clears are enabled and no
            // DCC surface is present.
            useCmask = (useDcc == false)           &&
                       (skipSmallSurface == false) &&
                       SupportFastColorClear(device, image, tileMode, tileType);
        }
    }

    return useCmask;
}

// =====================================================================================================================
// Determines if the given Image object should use fast color clears for CMask.
bool Gfx6Cmask::SupportFastColorClear(
    const Pal::Device& device,
    const Image&       image,
    AddrTileMode       tileMode,
    AddrTileType       tileType)
{
    const Gfx6PalSettings& settings   = GetGfx6Settings(device);
    const ImageCreateInfo& createInfo = image.Parent()->GetImageCreateInfo();

    // Choose which fast-clear setting to examine based on the type of Image we have.
    const bool fastColorClearEnable = (createInfo.imageType == ImageType::Tex2d) ?
                                      settings.fastColorClearEnable : settings.fastColorClearOn3dEnable;

    // Only enable CMask fast color clear iff:
    // - The Image's format supports it.
    // - The Image is a Color Target - (ensured by caller)
    // - The Image is not usable for Shader Write Access
    // - The Image is not linear tiled.
    // - The Image is not thick micro-tiled.
    PAL_ASSERT(image.Parent()->IsRenderTarget());

    return fastColorClearEnable                          &&
           (settings.gfx8RbPlusEnable == false)          &&
           (tileType != ADDR_THICK)                      &&
           (image.Parent()->IsShaderWritable() == false) &&
           (AddrMgr1::IsLinearTiled(tileMode)  == false) &&
           (SupportsFastColorClear(createInfo.swizzledFormat.format));
}

// =====================================================================================================================
Gfx6Cmask::Gfx6Cmask()
    :
    MaskRam()
{
    m_flags.value              = 0;
    m_cbColorCmaskSlice.u32All = 0;
}

// =====================================================================================================================
// Initializes this CMask object for the given Image and mipmap level.
Result Gfx6Cmask::Init(
    const Pal::Device& device,
    const Image&       image,
    uint32             mipLevel,
    gpusize*           pGpuOffset)    // [in,out] Current GPU memory offset & size
{
    const Pal::SubresId         subresource = { 0, mipLevel, 0 };
    const SubResourceInfo*const pSubResInfo = image.Parent()->SubresourceInfo(subresource);
    const AddrTileMode          tileMode    = image.GetSubResourceTileMode(subresource);
    const AddrTileType          tileType    = image.GetSubResourceTileType(subresource);

    m_flags.linear    = 0;
    m_flags.fastClear = SupportFastColorClear(device, image, tileMode, tileType);

    // Call the address library to compute the CMask properties.
    MaskRamInfo cmaskInfo = {};
    Result result = ComputeCmaskInfo(device, image, (*pSubResInfo), &cmaskInfo);

    if (result == Result::Success)
    {
        m_totalSize                       = cmaskInfo.maskSize;
        m_sliceSize                       = cmaskInfo.sliceSize;
        m_alignment                       = cmaskInfo.baseAlign;
        m_cbColorCmaskSlice.bits.TILE_MAX = cmaskInfo.blockSize;

        // Compute our aligned GPU memory offset and update the caller-provided running total.
        UpdateGpuMemOffset(pGpuOffset);
    }

    return result;
}

// =====================================================================================================================
// Here we want to give a value to correctly indicate that CMask is in expanded state, According to cb.doc, the Cmask
// Encoding for AA without fast clear is bits 3:2(2'b11) and bits 1:0(compression mode).
uint32 Gfx6Cmask::GetInitialValue(
    const Image& image)
{
    const auto& imgCreateInfo = image.Parent()->GetImageCreateInfo();
    // We need enough bits to fit all fragments, plus an extra bit for EQAA support.
    const bool   isEqaa       = (imgCreateInfo.fragments != imgCreateInfo.samples);
    const uint32 numBits      = Log2(imgCreateInfo.fragments) + isEqaa;
    uint32       cmaskValue   = Gfx6Cmask::FullyExpanded;

    switch (numBits)
    {
    case 0:
        PAL_ASSERT(image.HasFmaskData() == false);
        // For single-sampled image, cmask value is represented as fast-cleared state if not has DCC surface
        cmaskValue = Gfx6Cmask::FullyExpanded;
        break;
    case 1:
        cmaskValue = 0xDDDDDDDD;  // bits 3:2(2'b11)   bits 1:0(2'b01)
        break;
    case 2:
        cmaskValue = 0xEEEEEEEE;  // bits 3:2(2'b11)   bits 1:0(2'b10)
        break;
    case 3:
    case 4:                       // 8f16s EQAA also has a 0xFF clear value
        cmaskValue = 0xFFFFFFFF;  // bits 3:2(2'b11)   bits 1:0(2'b11)
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    };

    return cmaskValue;
}

// =====================================================================================================================
// Determines the fast-clear code for the cmask memory associated with the provided image
uint32 Gfx6Cmask::GetFastClearCode(
    const Image& image)
{
    // Assume that there's no DCC memory and that this will be easy.
    uint32  fastClearCode = Gfx6Cmask::FastClearValue;

    // The fast-clear code for images that have both cmask and dcc data is different from images that have just
    // cMask data.
    if (image.HasDccData())
    {
        // Only need the info from the base sub-resource
        const Pal::SubresId  subResource = {};
        const Gfx6Dcc*       pDcc        = image.GetDcc(subResource);

        if (pDcc->GetFastClearSize() != 0)
        {
            // Do not set CMask to be fast-cleared when used with DCC compression.
            fastClearCode = (image.Parent()->GetImageCreateInfo().samples > 1) ? Gfx6Cmask::FastClearValueDcc :
                                                                                 Gfx6Cmask::FullyExpanded;
        }
    }

    return fastClearCode;
}

// =====================================================================================================================
// Calls into AddrLib to compute CMASK info for a subresource
Result Gfx6Cmask::ComputeCmaskInfo(
    const Pal::Device&     device,
    const Image&           image,
    const SubResourceInfo& subResInfo,
    MaskRamInfo*           pCmaskInfo   // [out] CMASK info struct
    ) const
{
    const AddrMgr1::TileInfo*const pTileInfo = AddrMgr1::GetTileInfo(image.Parent(), subResInfo.subresId);

    Result result = Result::ErrorInitializationFailed;

    ADDR_COMPUTE_CMASK_INFO_INPUT addrCmaskIn = {};
    addrCmaskIn.size               = sizeof(addrCmaskIn);
    addrCmaskIn.tileIndex          = pTileInfo->tileIndex;
    addrCmaskIn.macroModeIndex     = pTileInfo->macroModeIndex;
    addrCmaskIn.pitch              = subResInfo.actualExtentTexels.width;
    addrCmaskIn.height             = subResInfo.actualExtentTexels.height;
    addrCmaskIn.numSlices          = MaskRamSlices((*image.Parent()), subResInfo);
    addrCmaskIn.isLinear           = false;
    addrCmaskIn.flags.tcCompatible = image.IsComprFmaskShaderReadable(&subResInfo);

    ADDR_COMPUTE_CMASK_INFO_OUTPUT addrCmaskOut = {};
    addrCmaskOut.size = sizeof(addrCmaskOut);

    ADDR_E_RETURNCODE addrRet = AddrComputeCmaskInfo(device.AddrLibHandle(), &addrCmaskIn, &addrCmaskOut);
    PAL_ASSERT(addrRet == ADDR_OK);

    if (addrRet == ADDR_OK)
    {
        pCmaskInfo->sliceSize = addrCmaskOut.sliceSize;
        pCmaskInfo->maskSize  = addrCmaskOut.cmaskBytes;
        pCmaskInfo->baseAlign = addrCmaskOut.baseAlign;
        pCmaskInfo->blockSize = addrCmaskOut.blockMax;

        result = Result::Success;
    }

    return result;
}

//=============== Implementation for Gfx6Fmask: ====================================================

// =====================================================================================================================
// Determines if the given Image object should use FMask metadata.
bool Gfx6Fmask::UseFmaskForImage(
    const Pal::Device& device,
    const Image&       image)
{
    const Pal::Image*const pParent = image.Parent();

    // Multisampled Images require FMask.
    return (pParent->IsEqaa() ||
            ((pParent->IsRenderTarget()             == true)  &&
             (pParent->IsShared()                   == false) &&
             (pParent->IsMetadataDisabledByClient() == false) &&
             (pParent->GetImageCreateInfo().samples > 1)));
}

// =====================================================================================================================
// Determines the Image Data Format used by SRD's which access an Image's FMask allocation. Returns The appropriate
// IMG_DATA_FORMAT enum value.
IMG_DATA_FORMAT Gfx6Fmask::FmaskFormat(
    uint32 samples,
    uint32 fragments,
    bool   isUav         // Is the fmask being setup as a UAV
    ) const
{
    IMG_DATA_FORMAT ret = IMG_DATA_FORMAT_8;

    if (isUav)
    {
        switch (m_bitsPerPixel)
        {
        case 8:
            ret = IMG_DATA_FORMAT_8;
            break;
        case 16:
            ret = IMG_DATA_FORMAT_16;
            break;
        case 32:
            ret = IMG_DATA_FORMAT_32;
            break;
        case 64:
            ret = IMG_DATA_FORMAT_32_32;
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }
    }
    else
    {
        // Lookup table of FMask Image Data Formats:
        // The table is indexed by: [log_2(samples) - 1][log_2(fragments)].
        constexpr IMG_DATA_FORMAT FMaskFormatTbl[4][4] =
        {
            // Two-sample formats
            { IMG_DATA_FORMAT_FMASK8_S2_F1,         // One fragment
              IMG_DATA_FORMAT_FMASK8_S2_F2, },      // Two fragments

            // Four-sample formats
            { IMG_DATA_FORMAT_FMASK8_S4_F1,         // One fragment
              IMG_DATA_FORMAT_FMASK8_S4_F2,         // Two fragments
              IMG_DATA_FORMAT_FMASK8_S4_F4, },      // Four fragments

            // Eight-sample formats
            { IMG_DATA_FORMAT_FMASK8_S8_F1,         // One fragment
              IMG_DATA_FORMAT_FMASK16_S8_F2,        // Two fragments
              IMG_DATA_FORMAT_FMASK32_S8_F4,        // Four fragments
              IMG_DATA_FORMAT_FMASK32_S8_F8, },     // Eight fragments

            // Sixteen-sample formats
            { IMG_DATA_FORMAT_FMASK16_S16_F1,       // One fragment
              IMG_DATA_FORMAT_FMASK32_S16_F2,       // Two fragments
              IMG_DATA_FORMAT_FMASK64_S16_F4,       // Four fragments
              IMG_DATA_FORMAT_FMASK64_S16_F8, },    // Eight fragments
        };

        const uint32 log2Samples   = Log2(samples);
        const uint32 log2Fragments = Log2(fragments);

        PAL_ASSERT((log2Samples   >= 1) && (log2Samples   <= 4));
        PAL_ASSERT(log2Fragments <= 3);

        ret = FMaskFormatTbl[log2Samples - 1][log2Fragments];
    }

    return ret;
}

// =====================================================================================================================
Gfx6Fmask::Gfx6Fmask()
    :
    MaskRam(),
    m_tileIndex(TileIndexUnused),
    m_bankHeight(0),
    m_pitch(0),
    m_bitsPerPixel(0)
{
    m_flags.value              = 0;
    m_cbColorFmaskSlice.u32All = 0;
}

// =====================================================================================================================
// Initializes this FMask object for the given Image and mipmap level.
Result Gfx6Fmask::Init(
    const Pal::Device& device,
    const Image&       image,
    uint32             mipLevel,
    gpusize*           pGpuOffset)    // [in,out] Current GPU memory offset & size
{
    PAL_ASSERT(mipLevel == 0); // MSAA Images only support a single mipmap level.

    Pal::SubresId               subresource = { 0, mipLevel, 0 };
    const SubResourceInfo*const pSubResInfo = image.Parent()->SubresourceInfo(subresource);

    const uint32 numSamples   = image.Parent()->GetImageCreateInfo().samples;
    const uint32 numFragments = image.Parent()->GetImageCreateInfo().fragments;

    // Setup the compression flag according to the settings.
    m_flags.compression = GetGfx6Settings(device).fmaskCompressEnable;

    // Call the address library to compute the FMask properties.
    FmaskInfo fmaskInfo;
    memset(&fmaskInfo, 0, sizeof(fmaskInfo));
    Result result = ComputeFmaskInfo(device, image, (*pSubResInfo), numSamples, numFragments, &fmaskInfo);

    if (result == Result::Success)
    {
        m_totalSize                       = fmaskInfo.maskSize;
        m_sliceSize                       = fmaskInfo.sliceSize;
        m_alignment                       = fmaskInfo.baseAlign;
        m_tileIndex                       = fmaskInfo.tileIndex;
        m_bankHeight                      = fmaskInfo.bankHeight;
        m_pitch                           = fmaskInfo.pitch;
        m_bitsPerPixel                    = fmaskInfo.bpp;
        m_cbColorFmaskSlice.bits.TILE_MAX = fmaskInfo.blockSize;

        // Compute our aligned GPU memory offset and update the caller-provided running total.
        UpdateGpuMemOffset(pGpuOffset);
    }

    return result;
}

// =====================================================================================================================
// Determines the 64-bit value that the fmask memory associated with the provided image should be initialized to
uint32 Gfx6Fmask::GetPackedExpandedValue(
    const Image& image)
{
    const uint32 log2Fragments = Log2(image.Parent()->GetImageCreateInfo().fragments);
    const uint32 log2Samples   = Log2(image.Parent()->GetImageCreateInfo().samples);

    // 4/8 fragments + 16 samples has double DWORD memory pattern and can't represent by a single uint32.
    PAL_ASSERT((log2Samples < 4) || (log2Fragments < 2));

    return LowPart(PackedFmaskExpandedValues[log2Fragments][log2Samples]);
}

// =====================================================================================================================
// Calls into AddrLib to compute FMASK info for a subresource
Result Gfx6Fmask::ComputeFmaskInfo(
    const Pal::Device&     device,
    const Image&           image,
    const SubResourceInfo& subResInfo,
    uint32                 numSamples,
    uint32                 numFragments,
    FmaskInfo*             pFmaskInfo     // [out] FMASK info struct
    ) const
{
    Result result = Result::ErrorInitializationFailed;

    ADDR_COMPUTE_FMASK_INFO_INPUT addrFmaskIn = {};
    addrFmaskIn.size       = sizeof(addrFmaskIn);
    addrFmaskIn.tileIndex  = TileIndexUnused;
    addrFmaskIn.tileMode   = image.GetSubResourceTileMode(subResInfo.subresId);
    // NOTE: On SI+, the hardware looks at the pitch of the color surface and the FMASK block
    // size to calculate FMASK height assuming the pitch to be the same. Passing in the actual
    // surface width to the address library will ensure the FMASK pitch is consistent with the
    // surface. Since height alignments vary according to bpp this causes the block size to be
    // calculated incorrectly. So we need to pass in the height of the resource for the Address
    // Library to get the correct height alignment.
    addrFmaskIn.pitch      = subResInfo.actualExtentTexels.width;
    addrFmaskIn.height     = subResInfo.extentTexels.height;
    addrFmaskIn.numSlices  = MaskRamSlices((*image.Parent()), subResInfo);
    addrFmaskIn.numSamples = numSamples;
    addrFmaskIn.numFrags   = numFragments;

    ADDR_COMPUTE_FMASK_INFO_OUTPUT addrFmaskOut    = {};
    ADDR_TILEINFO                  addrTileInfoOut = {};
    addrFmaskOut.size      = sizeof(addrFmaskOut);
    addrFmaskOut.pTileInfo = &addrTileInfoOut;

    ADDR_E_RETURNCODE addrRet = AddrComputeFmaskInfo(device.AddrLibHandle(), &addrFmaskIn, &addrFmaskOut);
    PAL_ASSERT(addrRet == ADDR_OK);

    if (addrRet == ADDR_OK)
    {
        constexpr uint32 NumPixelsPerTile = 64; // Pixels per 8x8 tile.

        // Bits per pixel is expressed as the number of bitplanes, so to get bits per pixel we need
        // to multiply bit planes times number of samples. It is rounded up to the nearest pow2
        // with a minimum of 8 bits
        constexpr uint32 MinBitsPerPixelFmask = 8;
        pFmaskInfo->bpp        = Pow2Pad(addrFmaskOut.bpp * numSamples);
        pFmaskInfo->bpp        = Max(pFmaskInfo->bpp, MinBitsPerPixelFmask);
        pFmaskInfo->pitch      = addrFmaskOut.pitch;
        pFmaskInfo->height     = addrFmaskOut.height;
        pFmaskInfo->sliceSize  = addrFmaskOut.sliceSize;
        pFmaskInfo->maskSize   = addrFmaskOut.fmaskBytes;
        pFmaskInfo->baseAlign  = addrFmaskOut.baseAlign;
        pFmaskInfo->blockSize  = (pFmaskInfo->pitch * pFmaskInfo->height / NumPixelsPerTile) - 1;
        pFmaskInfo->tileIndex  = addrFmaskOut.tileIndex;
        pFmaskInfo->bankHeight = Log2(addrFmaskOut.pTileInfo->bankHeight);

        result = Result::Success;
    }

    return result;
}

//=============== Implementation for Gfx6Dcc: ==================================================

static_assert((sizeof(MipFceStateMetaData) % PredicationAlign) == 0,
              "Size of MipFceStateMetaData must be a multiple of the SET_PREDICATION alignment!");

static_assert((sizeof(MipDccStateMetaData) % PredicationAlign) == 0,
              "Size of MipDccStateMetaData must be a multiple of the SET_PREDICATION alignment!");

// =====================================================================================================================
Gfx6Dcc::Gfx6Dcc()
    :
    MaskRam(),
    m_flags(),
    m_fastClearSize(0),
    m_dccControl(),
    m_clearKind(DccInitialClearKind::Uncompressed)
{
}

// =====================================================================================================================
// Determines if the given Image object should use DCC (delta color compression) metadata.
bool Gfx6Dcc::UseDccForImage(
    const Pal::Device& device,
    const Image&       image,
    AddrTileMode       tileMode,                  // Tile mode associated with the parent image
    AddrTileType       tileType,                  // Tile type associated with the parent image
    bool               metaDataTexFetchSupported) // If meta data tex fetch is suppported
{
    const Pal::Image*const pParent      = image.Parent();
    const auto&            createInfo   = pParent->GetImageCreateInfo();
    const auto&            settings     = device.Settings();
    const auto             pPalSettings = device.GetPublicSettings();

    // Assume that DCC is available; check for conditions where it won't work.
    bool useDcc         = true;
    bool mustDisableDcc = false;

    if (pParent->GetInternalCreateInfo().flags.useSharedMetadata)
    {
        const auto& metadata = image.Parent()->GetInternalCreateInfo().sharedMetadata;
        useDcc = (metadata.dccOffset[0] != 0) && (metadata.fastClearMetaDataOffset[0] != 0);
        if (useDcc == false)
        {
            mustDisableDcc = true;
        }
    }
    else
    {
        bool allMipsShaderWritable = pParent->IsShaderWritable();

        allMipsShaderWritable = (allMipsShaderWritable && (pParent->FirstShaderWritableMip() == 0));

        // DCC is never available on Gfx6 or Gfx7 ASICs
        if ((device.ChipProperties().gfxLevel == GfxIpLevel::GfxIp6) ||
            (device.ChipProperties().gfxLevel == GfxIpLevel::GfxIp7))
        {
            useDcc = false;
            mustDisableDcc = true;
        }
        else if (pParent->IsMetadataDisabledByClient())
        {
            // Don't use DCC if the caller asked that we allocate no metadata.
            useDcc = false;
            mustDisableDcc = true;
        }
        else if ((createInfo.metadataMode == MetadataMode::FmaskOnly) &&
                 (createInfo.samples > 1) &&
                 (pParent->IsRenderTarget() == true))
        {
            // Don't use DCC if the caller asked that we allocate color msaa image with Fmask metadata only.
            useDcc = false;
            mustDisableDcc = true;
        }
        else if (pParent->GetDccFormatEncoding() == DccFormatEncoding::Incompatible)
        {
            // Don't use DCC if the caller can switch between color target formats.
            // Or if caller can switch between shader formats
            useDcc = false;
            mustDisableDcc = true;
        }
        else if (tileType == ADDR_THICK)
        {
            // THICK micro-tiling does not support DCC. The reason for this is that the CB does not support doing a DCC
            // decompress operation on THICK micro-tiled Images.
            useDcc = false;
            mustDisableDcc = true;
        }
        else if (image.IsMacroTiled(tileMode) == false)
        {
            // If the tile-mode is 1D or linear, then this surface has no chance of using DCC memory.  2D tiled surfaces
            // get much more complicated...  allow DCC for whatever levels of the surface can support it.
            useDcc = false;
            mustDisableDcc = true;
        }
        else if (pParent->IsDepthStencilTarget() || (pParent->IsRenderTarget() == false))
        {
            // DCC only makes sense for renderable color buffers
            useDcc = false;
            mustDisableDcc = true;
        }
        else if (pParent->IsShared() || pParent->IsPresentable() || pParent->IsFlippable())
        {
            // DCC is never available for shared, presentable, or flippable images.
            useDcc = false;
            mustDisableDcc = true;
        }
        else if (IsYuv(createInfo.swizzledFormat.format))
        {
            // DCC isn't useful for YUV formats, since those are usually accessed heavily by the multimedia engines.
            useDcc = false;
            mustDisableDcc = true;
        }
        else if ((Pal::Gfx6::Device::WaEnableDcc8bppWithMsaa == false) &
                 (createInfo.samples > 1) &
                 (BitsPerPixel(createInfo.swizzledFormat.format) == 8))
        {
            // There is known issue that CB can only partially decompress DCC KEY for 4x+ 8bpp MSAA resource
            // (even with sample_split = 4).
            useDcc = false;
            mustDisableDcc = true;
        }
        else if (allMipsShaderWritable)
        {
            // DCC does not make sense for UAVs or RT+UAVs (all mips are shader writeable).
            useDcc = false;
            // Give a chance for clients to force enabling DCC for RT+UAVs. i.e. App flags the resource as both render
            // target and unordered access but never uses it as UAV.
            mustDisableDcc = pParent->IsRenderTarget() ? false : true;
        }
        // Msaa image with resolveSrc usage flag will go through shader based resolve if fixed function resolve is not
        // preferred, the image will be readable by a shader.
        else if ((pParent->IsShaderReadable() ||
                  (pParent->IsResolveSrc() && (pParent->PreferCbResolve() == false))) &&
                 (metaDataTexFetchSupported == false) &&
                 (TestAnyFlagSet(settings.useDcc, UseDccNonTcCompatShaderRead) == false))
        {
            // Disable DCC for shader read resource that cannot be made TC compat, this avoids DCC decompress
            // for RT->SR barrier.
            useDcc = false;
        }
        else if ((createInfo.extent.width * createInfo.extent.height) <=
                (pPalSettings->hintDisableSmallSurfColorCompressionSize *
                 pPalSettings->hintDisableSmallSurfColorCompressionSize))
        {
            // DCC should be disabled if the client has indicated that they want to disable color compression on small
            // surfaces and this surface qualifies.
            useDcc = false;
        }
        else if (pPalSettings->dccBitsPerPixelThreshold > BitsPerPixel(createInfo.swizzledFormat.format))
        {
            //Disable DCC if the threshold is greater than the BPP of the image.
            useDcc = false;
        }
        else
        {
            const ChNumFormat format = createInfo.swizzledFormat.format;

            // Make sure the settings allow use of DCC surfaces for sRGB Images.
            if (IsSrgb(format) && (TestAnyFlagSet(settings.useDcc, UseDccSrgb) == false))
            {
                useDcc = false;
            }
            else if ((createInfo.flags.prt == 1) && (TestAnyFlagSet(settings.useDcc, UseDccPrt) == false))
            {
                // Disable DCC for PRT if the settings don't allow it.
                useDcc = false;
            }
            else if (createInfo.samples > 1)
            {
                // Make sure the settings allow use of DCC surfaces for MSAA.
                if (createInfo.samples == 2)
                {
                    useDcc = useDcc && TestAnyFlagSet(settings.useDcc, UseDccMultiSample2x);
                }
                else if (createInfo.samples == 4)
                {
                    useDcc = useDcc && TestAnyFlagSet(settings.useDcc, UseDccMultiSample4x);
                }
                else if (createInfo.samples == 8)
                {
                    useDcc = useDcc && TestAnyFlagSet(settings.useDcc, UseDccMultiSample8x);
                }

                if (createInfo.samples != createInfo.fragments)
                {
                    useDcc = useDcc && TestAnyFlagSet(settings.useDcc, UseDccEqaa);
                }
            }
            else
            {
                // Make sure the settings allow use of DCC surfaces for single-sampled surfaces
                useDcc = useDcc && TestAnyFlagSet(settings.useDcc, UseDccSingleSample);
            }

            // According to DXX engineers, using DCC for mipmapped arrays has worse performance, so just disable it.
            if (useDcc && (createInfo.arraySize > 1) && (createInfo.mipLevels > 1))
            {
                useDcc = false;
            }
        }
    }

    if ((mustDisableDcc == false) && (createInfo.metadataMode == MetadataMode::ForceEnabled))
    {
        useDcc = true;
    }

    return useDcc;
}

// =====================================================================================================================
// Determines if the given Image object should use DCC (delta color compression) metadata.
Result Gfx6Dcc::Init(
    const Pal::Device& device,
    const Image&       image,
    uint32             mipLevel,
    gpusize*           pSizeAvail,    // [in, out] Size of pre-calculated Dcc size available
    gpusize*           pGpuOffset,    // [in,out] Current GPU memory offset & size
    bool*              pCanUseDcc)    // [in] true if this mip level can actually use DCC
                                      // [out] true if the *next* mip level can use DCC
{
    const Gfx6PalSettings& settings        = GetGfx6Settings(device);
    const ImageCreateInfo& imageCreateInfo = image.Parent()->GetImageCreateInfo();

    const Pal::SubresId         subresource = { 0, mipLevel, 0 /* slice */ };
    const SubResourceInfo*const pSubResInfo = image.Parent()->SubresourceInfo(subresource);

    // Record the usefullness of this DCC memory
    m_flags.enableCompression = *pCanUseDcc;

    // Assume that we can enable DCC fast clear iff:
    // - Settings are configured to allow fast-clear
    // - The Image's format supports fast-clear
    // - This mipmap level is actually able to use DCC
    // NOTE: This may be overridden below after AddrLib computes the DCC information below!

    const bool fastColorClearEnable = (imageCreateInfo.imageType == ImageType::Tex2d)
                                        ? settings.fastColorClearEnable : settings.fastColorClearOn3dEnable;

    m_flags.enableFastClear = (fastColorClearEnable                                          &&
                               SupportsFastColorClear(imageCreateInfo.swizzledFormat.format) &&
                               IsCompressionEnabled());

    // Assume by default the memory is contiguous.
    m_flags.contiguousSubresMem = 1;

    // We disable DCC memory for mipmapped arrays due to bad performance, see UseDccForImage().
    PAL_ASSERT ((imageCreateInfo.arraySize == 1) || (mipLevel == 0));

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 706
    // Save away the initial clear behavior
    m_clearKind = static_cast<DccInitialClearKind>(device.GetPublicSettings()->dccInitialClearKind);
#else
    m_clearKind = DccInitialClearKind::Uncompressed;
#endif

    // First pass is to calculate DCC memory size of all array slices. This should be *actual* arraySize of this mip,
    // But we disable DCC memory for mipmaped arrays, which might cause *extra* slice padding. For tex3d, the arraySize
    // is always 1 and its depth slices are counted into pSubResInfo->size. So we don't need to care the *extra* padding
    // array slices now.
    const gpusize mipLevelSize = pSubResInfo->size * imageCreateInfo.arraySize;
    DccInfo  dccInfo;
    Result result = ComputeDccInfo(device, image, subresource, mipLevelSize, &dccInfo, pCanUseDcc);

    if (result == Result::Success)
    {
        m_fastClearSize = dccInfo.fastClearSize;
        m_totalSize     = dccInfo.maskSize;
        m_sliceSize     = dccInfo.sliceSize;

        // The address library does not have mip level as input, so it returns the base address alignment for every
        // call. But we only need to take care of base address alignment for mip level 0, as the base address of sub-
        // levels are just padded to maskSize of prior level (not the base alignment!).
        if (mipLevel == 0)
        {
            m_alignment = dccInfo.baseAlign;
        }
        else
        {
            m_alignment = 1;
        }

        // For arraySize > 1, we need to call ComputeDccInfo() with one slice size to get a correct fast-clear size.
        if (imageCreateInfo.arraySize > 1)
        {
            bool dummyCanUseDcc = IsCompressionEnabled();
            result = ComputeDccInfo(device, image, subresource, pSubResInfo->size, &dccInfo, &dummyCanUseDcc);
            PAL_ASSERT(result == Result::Success);
            // Fast-clear size is per-slice based.
            m_fastClearSize = dccInfo.fastClearSize;

            if (dccInfo.sizeAligned == false)
            {
                PAL_ASSERT(dccInfo.maskSize != m_sliceSize);
                // If the DCC slice size is not aligned, the data are interleaved across the slices.
                m_flags.contiguousSubresMem = false;
            }
            // NOTE: For array slices other than 0, it can be compressed or fast-cleared only if we always program the
            // slice 0's base address in both rendering and fetching! Currently RPM uses the exact slice's base address
            // when copying a single subresource of a block-compressed image but we don't use DCC for block-compressed
            // image and are not likely to do so in the future.
            PAL_ASSERT(Formats::IsBlockCompressed(imageCreateInfo.swizzledFormat.format) == false);
        }
        // If the DCC memory size is not aligned properly, the memory will not be aligned unless we are at the last mip
        // level as there are no overlapping bits.
        else if (UseFastClear() && (mipLevel != (imageCreateInfo.mipLevels - 1)))
        {
            m_flags.contiguousSubresMem = dccInfo.sizeAligned;
        }

        // if the memory is not contiguous, we cannot do a fast clear.
        if (m_flags.contiguousSubresMem == 0)
        {
            m_flags.enableFastClear = 0;
        }

        // If this level's DCC memory size itself is not aligned, assign all remaining size to it. All levels below do
        // not really own DCC memory at all.
        if (dccInfo.sizeAligned == false)
        {
            m_totalSize = *pSizeAvail;
        }

        *pSizeAvail -= m_totalSize;

        // Compute our aligned GPU memory offset and update the caller-provided running total.
        UpdateGpuMemOffset(pGpuOffset);

        SetControlReg(image, (*pSubResInfo));
    }

    return result;
}

// =====================================================================================================================
// Determines the total DCC memory size and alignment
Result Gfx6Dcc::InitTotal(
    const Pal::Device& device,
    const Image&       image,
    gpusize            totalMipSize,  // Mipmap total size for this image
    gpusize*           pGpuOffset,    // [in,out] Current GPU memory offset & size
    gpusize*           pTotalSize)    // [out] Total GPU memory size
{
    const SubresId subresource = { 0, 0, 0 /* slice */ };

    // We disable DCC memory for mipmapped arrays due to bad performance, see UseDccForImage().
    PAL_ASSERT((image.Parent()->GetImageCreateInfo().arraySize == 1) ||
               (image.Parent()->GetImageCreateInfo().mipLevels == 1));

    DccInfo dccInfo;
    bool    dummyNextLevelUseDcc = false;
    Result result = ComputeDccInfo(device,
                                   image,
                                   subresource,
                                   totalMipSize,
                                   &dccInfo,
                                   &dummyNextLevelUseDcc);

    const gpusize offset = Pow2Align((*pGpuOffset), dccInfo.baseAlign);

    // Compute our aligned GPU memory offset and update the caller-provided running total.
    (*pGpuOffset) = offset + dccInfo.maskSize;
    (*pTotalSize) = dccInfo.maskSize;

    return result;
}

// =====================================================================================================================
// Returns the optimal value of DCC_CONTROL.MIN_COMPERSSED_BLOCK_SIZE
uint32 Gfx6Dcc::GetMinCompressedBlockSize(
    const Image&  image)
{
    const auto&  chipProp = image.Parent()->GetDevice()->ChipProperties();

    //    [min-compressed-block-size] should be set to 32 for dGPU and 64 for APU because all of our APUs to date
    //    use DIMMs which have a request granularity size of 64B while all other chips have a 32B request size
    return static_cast<uint32>((chipProp.gpuType == GpuType::Integrated) ? Gfx8DccMinBlockSize::BlockSize64B
                                                                         : Gfx8DccMinBlockSize::BlockSize32B);
}

// =====================================================================================================================
// Determines if the given Image object should use DCC (delta color compression) metadata.
void Gfx6Dcc::SetControlReg(
    const Image&           image,
    const SubResourceInfo& subResInfo)
{
    // Setup DCC control registers with suggested value from spec
    m_dccControl.bits.KEY_CLEAR_ENABLE = 0; // not supported on VI

    // MAX_UNCOMPRESSED_BLOCK_SIZE 3:2 none Sets the maximum amount of data that may be compressed into one block. Some
    // other clients may not be able to handle larger sizes. CB_RESOLVEs cannot have this setting larger than the size
    // of one sample's data.
    // 64B (Set for 8bpp 2+ fragment surfaces needing HW resolves)
    // 128B (Set for 16bpp 2+ fragment surfaces needing HW resolves)
    // 256B (default)
    m_dccControl.bits.MAX_UNCOMPRESSED_BLOCK_SIZE = static_cast<unsigned int>(Gfx8DccMaxBlockSize::BlockSize256B);

    const ImageCreateInfo&  createInfo = image.Parent()->GetImageCreateInfo();
    if (createInfo.samples >= 2)
    {
        const uint32 bitsPerPixel = BitsPerPixel(createInfo.swizzledFormat.format);

        if (bitsPerPixel == 8)
        {
            m_dccControl.bits.MAX_UNCOMPRESSED_BLOCK_SIZE =
                static_cast<unsigned int>(Gfx8DccMaxBlockSize::BlockSize64B);
        }
        else if (bitsPerPixel == 16)
        {
            m_dccControl.bits.MAX_UNCOMPRESSED_BLOCK_SIZE =
                static_cast<unsigned int>(Gfx8DccMaxBlockSize::BlockSize128B);
        }
    }

    m_dccControl.bits.MIN_COMPRESSED_BLOCK_SIZE = GetMinCompressedBlockSize(image);
    m_dccControl.bits.COLOR_TRANSFORM           = DCC_CT_AUTO;
    m_dccControl.bits.LOSSY_RGB_PRECISION       = 0;
    m_dccControl.bits.LOSSY_ALPHA_PRECISION     = 0;

    // If this DCC surface is potentially going to be used in texture fetches though, we need some special settings.
    if (subResInfo.flags.supportMetaDataTexFetch)
    {
        m_dccControl.bits.INDEPENDENT_64B_BLOCKS    = true;
        m_dccControl.bits.MAX_COMPRESSED_BLOCK_SIZE = static_cast<unsigned int>(Gfx8DccMaxBlockSize::BlockSize64B);
    }
    else
    {
        m_dccControl.bits.INDEPENDENT_64B_BLOCKS    = false;

        // Note that MAX_UNCOMPRESSED_BLOCK_SIZE must >= MAX_COMPRESSED_BLOCK_SIZE
        // Set MAX_COMPRESSED_BLOCK_SIZE as big as possible for better compression ratio
        m_dccControl.bits.MAX_COMPRESSED_BLOCK_SIZE = m_dccControl.bits.MAX_UNCOMPRESSED_BLOCK_SIZE;
    }
}

// =====================================================================================================================
// Calculates the 32-bit value which represents the value the DCC surface should be cleared to.
// NOTE:
//    Surfaces that will not be texture-fetched can be fast-cleared to any color.  These will always return a clear
//    code that corresponds to "Gfx8DccClearColor::Reg".  Surfaces that will potentially be texture-fetched though can
//    only be fast-cleared to one of four HW-defined colors.
uint32 Gfx6Dcc::GetFastClearCode(
    const Image&            image,
    const Pal::SubresRange& clearRange,
    const uint32*           pConvertedColor,
    bool*                   pNeedFastClearElim) // [out] true if this surface will require a fast-clear-eliminate pass
                                                //       before it can be used as a texture
{
    PAL_ASSERT(clearRange.numPlanes == 1);

    // Fast-clear code that is valid for images that won't be texture fetched.
    Gfx8DccClearColor clearCode = Gfx8DccClearColor::ClearColorReg;

    const Pal::SubresId baseSubResource = { clearRange.startSubres.plane,
                                            clearRange.startSubres.mipLevel,
                                            clearRange.startSubres.arraySlice };
    const SubResourceInfo*const pSubResInfo = image.Parent()->SubresourceInfo(baseSubResource);

    if (pSubResInfo->flags.supportMetaDataTexFetch != 0)
    {
        // Surfaces that are fast cleared to one of the following colors may be texture fetched:
        //      1) ARGB(0, 0, 0, 0)
        //      2) ARGB(1, 0, 0, 0)
        //      3) ARGB(0, 1, 1, 1)
        //      4) ARGB(1, 1, 1, 1)
        //
        // If the clear-color is *not* one of those colors, then this routine will produce the "default"
        // clear-code.  The default clear-code is not understood by the TC and a fast-clear-eliminate pass must be
        // issued prior to using this surface as a texture.
        const ImageCreateInfo& createInfo    = image.Parent()->GetImageCreateInfo();
        const uint32           numComponents = NumComponents(createInfo.swizzledFormat.format);
        const SurfaceSwap      surfSwap      = Formats::Gfx6::ColorCompSwap(createInfo.swizzledFormat);
        const ChannelSwizzle*  pSwizzle      = &createInfo.swizzledFormat.swizzle.swizzle[0];

        uint32                 color[4] = {};
        uint32                 ones[4]  = {};
        uint32                 cmpIdx   = 0;
        uint32                 rgbaIdx  = 0;

        switch(numComponents)
        {
        case 1:
            while ((pSwizzle[rgbaIdx] != ChannelSwizzle::X) && (rgbaIdx < 4))
            {
                rgbaIdx++;
            }

            PAL_ASSERT(pSwizzle[rgbaIdx] == ChannelSwizzle::X);

            color[0] =
            color[1] =
            color[2] =
            color[3] = pConvertedColor[rgbaIdx];

            ones[0] =
            ones[1] =
            ones[2] =
            ones[3] = image.TranslateClearCodeOneToNativeFmt(0);
            break;

        // Formats with two channels are special. Value from X channel represents color in clear code,
        // and value from Y channel represents alpha in clear code.
        case 2:
            color[0] =
            color[1] =
            color[2] = pConvertedColor[0];

            PAL_ASSERT(pSwizzle[0] >= ChannelSwizzle::X);

            cmpIdx  = static_cast<uint32>(pSwizzle[0]) - static_cast<uint32>(ChannelSwizzle::X);
            ones[0] =
            ones[1] =
            ones[2] = image.TranslateClearCodeOneToNativeFmt(cmpIdx);

            // In SWAP_STD case, clear color (in RGBA) has swizzle format of XY--. Clear code is RRRG.
            // In SWAP_STD_REV case, clear color (in RGBA) has swizzle format of YX--. Clear code is GGGR.
            if ((surfSwap == SWAP_STD) || (surfSwap == SWAP_STD_REV))
            {
                color[3] = pConvertedColor[1];

                PAL_ASSERT(pSwizzle[1] >= ChannelSwizzle::X);

                cmpIdx   = static_cast<uint32>(pSwizzle[1]) - static_cast<uint32>(ChannelSwizzle::X);
                ones[3]  = image.TranslateClearCodeOneToNativeFmt(cmpIdx);
            }
            // In SWAP_ALT case, clear color (in RGBA) has swizzle format of X--Y. Clear code is RRRA.
            // In SWAP_ALT_REV case, clear color (in RGBA) has swizzle format of Y--X. Clear code is AAAR.
            else if ((surfSwap == SWAP_ALT) || (surfSwap == SWAP_ALT_REV))
            {
                color[3] = pConvertedColor[3];

                PAL_ASSERT(pSwizzle[3] >= ChannelSwizzle::X);

                cmpIdx   = static_cast<uint32>(pSwizzle[3]) - static_cast<uint32>(ChannelSwizzle::X);
                ones[3]  = image.TranslateClearCodeOneToNativeFmt(cmpIdx);
            }
            break;
        case 3:
            for (rgbaIdx = 0; rgbaIdx < 3; rgbaIdx++)
            {
                color[rgbaIdx] = pConvertedColor[rgbaIdx];

                PAL_ASSERT(pSwizzle[rgbaIdx] >= ChannelSwizzle::X);

                cmpIdx = static_cast<uint32>(pSwizzle[rgbaIdx]) - static_cast<uint32>(ChannelSwizzle::X);
                ones[rgbaIdx] = image.TranslateClearCodeOneToNativeFmt(cmpIdx);
            }
            color[3] = 0;
            ones[3]  = 0;
            break;
        case 4:
            for (rgbaIdx = 0; rgbaIdx < 4; rgbaIdx++)
            {
                color[rgbaIdx] = pConvertedColor[rgbaIdx];

                if (pSwizzle[rgbaIdx] == ChannelSwizzle::One)
                {
                    // Only for swizzle format XYZ1 / ZYX1
                    PAL_ASSERT(rgbaIdx == 3);

                    color[rgbaIdx] = color[2];
                    ones[rgbaIdx]  = ones[2];
                }
                else
                {
                    PAL_ASSERT(pSwizzle[rgbaIdx] != ChannelSwizzle::Zero);

                    cmpIdx = static_cast<uint32>(pSwizzle[rgbaIdx]) - static_cast<uint32>(ChannelSwizzle::X);
                    ones[rgbaIdx] = image.TranslateClearCodeOneToNativeFmt(cmpIdx);
                }
            }
            break;
        default:
            break;
        }

        *pNeedFastClearElim = false;

        if ((color[0] == 0) &&
            (color[1] == 0) &&
            (color[2] == 0) &&
            (color[3] == 0))
        {
            clearCode = Gfx8DccClearColor::ClearColor0000;
        }
        else if (image.Parent()->GetDccFormatEncoding() == DccFormatEncoding::SignIndependent)
        {
            // cant allow special clear color code because the formats do not support DCC Constant
            // encoding. This happens when we mix signed and unsigned formats. There is no problem with
            // clearcolor0000.The issue is only seen when there is a 1 in any of the channels
            *pNeedFastClearElim = true;
        }
        else if ((color[0] == 0) &&
                 (color[1] == 0) &&
                 (color[2] == 0) &&
                 (color[3] == ones[3]))
        {
            clearCode = Gfx8DccClearColor::ClearColor0001;
        }
        else if ((color[0] == ones[0]) &&
                 (color[1] == ones[1]) &&
                 (color[2] == ones[2]) &&
                 (color[3] == 0))
        {
            clearCode = Gfx8DccClearColor::ClearColor1110;
        }
        else if ((color[0] == ones[0]) &&
                 (color[1] == ones[1]) &&
                 (color[2] == ones[2]) &&
                 (color[3] == ones[3]))
        {
            clearCode = Gfx8DccClearColor::ClearColor1111;
        }
        else
        {
            *pNeedFastClearElim = true;
        }
    }
    else
    {
        // Even though it won't be texture feched, it is still safer to unconditionally do FCE to guarantee the base
        // data is coherent with prior clears
        *pNeedFastClearElim = true;
    }

    // DCC memory is organized in bytes from the HW perspective; however, the caller expects the clear code to be a
    // DWORD value, so replicate the clear code byte value across all four positions.
    uint8 clearCodeVal = static_cast<uint8>(clearCode);
    return ReplicateByteAcrossDword(clearCodeVal);
}

// =====================================================================================================================
uint8 Gfx6Dcc::GetInitialValue(
    const Image& image,
    SubresId     subRes,
    ImageLayout  layout
    ) const
{
    // If nothing else applies, initialize to "uncompressed"
    uint8 initialValue = Gfx6Dcc::DecompressedValue;
    bool isForceEnabled = TestAnyFlagSet(static_cast<uint32>(m_clearKind),
                                         static_cast<uint32>(DccInitialClearKind::ForceBit));

    if ((m_clearKind != DccInitialClearKind::Uncompressed) &&
        ((ImageLayoutToColorCompressionState(image.LayoutToColorCompressionState(subRes), layout)
           != ColorDecompressed) ||
         isForceEnabled))
    {
        switch (m_clearKind)
        {
            case DccInitialClearKind::ForceOpaqueBlack:
            case DccInitialClearKind::OpaqueBlack:
                initialValue = static_cast<uint8>(Gfx8DccClearColor::ClearColor0001);
                break;
            case DccInitialClearKind::ForceOpaqueWhite:
            case DccInitialClearKind::OpaqueWhite:
                initialValue = static_cast<uint8>(Gfx8DccClearColor::ClearColor1111);
                break;
            default:
                PAL_ASSERT_ALWAYS();
        }
    }

    return initialValue;
}

// =====================================================================================================================
// Calls into AddrLib to compute DCC info for a subresource
Result Gfx6Dcc::ComputeDccInfo(
    const Pal::Device& device,
    const Image&       image,
    const SubresId&    subResource,
    gpusize            colorSurfSize,     // size, in bytes, of all the slices
    DccInfo*           pDccInfo,          // [out] information on the DCC surface
    bool*              pNextMipCanUseDcc) // [out] true if the *next* mip level can use DCC
{
    Result result = Result::ErrorInitializationFailed;

    const SubResourceInfo*const    pSubResInfo     = image.Parent()->SubresourceInfo(subResource);
    const ImageCreateInfo&         imageCreateInfo = image.Parent()->GetImageCreateInfo();
    const AddrMgr1::TileInfo*const pTileInfo       = AddrMgr1::GetTileInfo(image.Parent(), subResource);

    ADDR_COMPUTE_DCCINFO_INPUT   dccInfoIn  = {};

    dccInfoIn.size                      = sizeof(dccInfoIn);
    dccInfoIn.bpp                       = pSubResInfo->bitsPerTexel;
    dccInfoIn.numSamples                = imageCreateInfo.fragments;
    dccInfoIn.colorSurfSize             = colorSurfSize;
    dccInfoIn.tileMode                  = AddrMgr1::AddrTileModeFromHwArrayMode(pTileInfo->tileMode);
    dccInfoIn.tileInfo.banks            = pTileInfo->banks;
    dccInfoIn.tileInfo.bankWidth        = pTileInfo->bankWidth;
    dccInfoIn.tileInfo.bankHeight       = pTileInfo->bankHeight;
    dccInfoIn.tileInfo.macroAspectRatio = pTileInfo->macroAspectRatio;
    dccInfoIn.tileInfo.tileSplitBytes   = pTileInfo->tileSplitBytes;
    // Address library pipe configuration enumerations are one more than the HW enumerations.
    dccInfoIn.tileInfo.pipeConfig       = static_cast<AddrPipeCfg>(pTileInfo->pipeConfig + 1);
    dccInfoIn.tileSwizzle               = pTileInfo->tileSwizzle;
    dccInfoIn.tileIndex                 = pTileInfo->tileIndex;
    dccInfoIn.macroModeIndex            = pTileInfo->macroModeIndex;

    // DCC is only supported for 2D/3D tiled resources.  For DCC resources in a mip chain the 1D
    // tiled levels cannot have DCC.  However, a DCC key is needed for the 1D tiled levels to
    // support texture reads.  The 1D tiled DCC keys are like padded areas set to no compression.
    if ((dccInfoIn.tileMode == ADDR_TM_1D_TILED_THIN1) || (dccInfoIn.tileMode == ADDR_TM_1D_TILED_THICK))
    {
        SubresId subResLevel0 = subResource;
        subResLevel0.mipLevel = 0;

        const auto*const pLevel0TileInfo = AddrMgr1::GetTileInfo(image.Parent(), subResLevel0);

        // Use 2D/3D tile mode from mip level 0 for Addrlib to calculate a DCC key size.
        // Addrlib will fail with 1D tile mode.
        dccInfoIn.tileMode       = AddrMgr1::AddrTileModeFromHwArrayMode(pLevel0TileInfo->tileMode);
        dccInfoIn.tileSwizzle    = pLevel0TileInfo->tileSwizzle;
        dccInfoIn.tileIndex      = pLevel0TileInfo->tileIndex;
        dccInfoIn.macroModeIndex = pLevel0TileInfo->macroModeIndex;
    }

    ADDR_COMPUTE_DCCINFO_OUTPUT  dccInfoOut = {};
    dccInfoOut.size = sizeof(dccInfoOut);

    const ADDR_E_RETURNCODE  addrRet = AddrComputeDccInfo(device.AddrLibHandle(), &dccInfoIn, &dccInfoOut);
    PAL_ASSERT(addrRet == ADDR_OK);

    if (addrRet == ADDR_OK)
    {
        pDccInfo->maskSize  = dccInfoOut.dccRamSize;
        pDccInfo->baseAlign = dccInfoOut.dccRamBaseAlign;
        pDccInfo->blockSize = 0; // not relevant for DCC

        // The address library does not provide any sort of "per slice" information for
        // DCC memory.  However, DCC memory is linear; i.e., each slice is the same size,
        // etc. so the size of one slice is simple math.  This works only if there is no
        // padding, but on VI there isn't.
        pDccInfo->sliceSize = pDccInfo->maskSize / imageCreateInfo.arraySize;

        // Record the amount of DCC memory that needs to be fast-cleared.  usually this is
        // the same as "dccRamSize", but not always.
        pDccInfo->fastClearSize = dccInfoOut.dccFastClearSize;

        // If the DCC memory size is properly aligned, it is fast-clearable
        pDccInfo->sizeAligned = (dccInfoOut.dccRamSizeAligned != 0);

        // The address library tells us if the *next* mip-level's DCC key meets all the
        // necessary alignment constraints, etc. to be actually useable by the HW.
        // Record the actual state of this level's useability with this DCC info so that
        // we're not always backing up one level when we go to look at this info.
        *pNextMipCanUseDcc = dccInfoOut.subLvlCompressible != 0;

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
void Gfx6Dcc::SetEnableCompression(uint32 val)
{
    m_flags.enableCompression = val;
}

} // Gfx6
} // Pal
