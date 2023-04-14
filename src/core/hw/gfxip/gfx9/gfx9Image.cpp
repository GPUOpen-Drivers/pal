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

#include "core/platform.h"
#include "core/image.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9FormatInfo.h"
#include "core/hw/gfxip/gfx9/gfx9MaskRam.h"
#include "g_gfx9Settings.h"
#include "core/hw/gfxip/gfx9/gfx9DepthStencilState.h"
#include "core/addrMgr/addrMgr2/addrMgr2.h"
#include "palHasher.h"
#include "palMath.h"
#include "palLiterals.h"
#include "palGpuMemory.h"

#include <atomic>
#include <limits.h>
#include <type_traits>

using namespace Pal::AddrMgr2;
using namespace Util;
using namespace Util::Literals;
using namespace Pal::Formats;
using namespace Pal::Formats::Gfx9;

namespace Pal
{
namespace Gfx9
{

// These variables ensure that we are assigning a rotating set of swizzle indices for each new image.
static std::atomic<uint32> cbSwizzleIdx{};
static std::atomic<uint32> txSwizzleIdx{};
static std::atomic<uint32> fMaskSwizzleIdx{};

// =====================================================================================================================
// Helper function for allocating memory for a Gfx9MaskRam object, and instantiating the derived class.
template <typename MaskRamType, typename ...Args>
Result CreateGfx9MaskRam(
    const Pal::Device& device,
    const Image&       image,
    MaskRamType**      ppMaskRam,
    Args...            args)
{
    static_assert(std::is_base_of<Gfx9MaskRam, MaskRamType>::value, "MaskRamType must be derived from Gfx9MaskRam");

    PAL_ASSERT(*ppMaskRam == nullptr);

    Result result = Result::ErrorOutOfMemory;

    size_t allocSize   = sizeof(MaskRamType) + sizeof(Gfx9MetaEqGenerator);
    void*  pMaskRamMem = PAL_MALLOC(allocSize, device.GetPlatform(), SystemAllocType::AllocObject);

    if (pMaskRamMem != nullptr)
    {
        void* pEqGeneratorMem = VoidPtrInc(pMaskRamMem, sizeof(MaskRamType));
        *ppMaskRam = PAL_PLACEMENT_NEW(pMaskRamMem) MaskRamType(image, pEqGeneratorMem, args...);

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
Image::Image(
    Pal::Image*        pParentImage,
    ImageInfo*         pImageInfo,
    const Pal::Device& device)
    :
    Pm4Image(pParentImage, pImageInfo, device),
    m_totalPlaneSize(0),
    m_gfxDevice(static_cast<const Device&>(*device.GetGfxDevice())),
    m_pHtile(nullptr),
    m_numDccPlanes(0),
    m_pCmask(nullptr),
    m_pFmask(nullptr),
    m_waTcCompatZRangeMetaDataOffset(0),
    m_waTcCompatZRangeMetaDataSizePerMip(0),
    m_dccLookupTableOffset(0),
    m_dccLookupTableSize(0),
    m_mallCursorCacheSize(0),
    m_mallCursorCacheOffset(0),
    m_useCompToSingleForFastClears(false)
{
    memset(&m_layoutToState,      0, sizeof(m_layoutToState));
    memset(&m_defaultGfxLayout,   0, sizeof(m_defaultGfxLayout));
    memset(m_addrSurfOutput,      0, sizeof(m_addrSurfOutput));
    memset(m_addrMipOutput,       0, sizeof(m_addrMipOutput));
    memset(m_addrSurfSetting,     0, sizeof(m_addrSurfSetting));
    memset(&m_metaDataClearConst, 0, sizeof(m_metaDataClearConst));
    memset(m_metaDataLookupTableOffsets, 0, sizeof(m_metaDataLookupTableOffsets));
    memset(m_metaDataLookupTableSizes,   0, sizeof(m_metaDataLookupTableSizes));
    memset(m_planeOffset,                0, sizeof(m_planeOffset));
    memset(m_pDcc,                       0, sizeof(m_pDcc));
    memset(m_pDispDcc,                   0, sizeof(m_pDispDcc));
    memset(m_dccStateMetaDataOffset,     0, sizeof(m_dccStateMetaDataOffset));
    memset(m_dccStateMetaDataSize,       0, sizeof(m_dccStateMetaDataSize));
    memset(m_fastClearEliminateMetaDataOffset, 0, sizeof(m_fastClearEliminateMetaDataOffset));
    memset(m_fastClearEliminateMetaDataSize,   0, sizeof(m_fastClearEliminateMetaDataSize));

    for (uint32  planeIdx = 0; planeIdx < MaxNumPlanes; planeIdx++)
    {
        m_addrSurfOutput[planeIdx].size  = sizeof(ADDR2_COMPUTE_SURFACE_INFO_OUTPUT);
        m_addrSurfSetting[planeIdx].size = sizeof(ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT);

        m_addrSurfOutput[planeIdx].pMipInfo = &m_addrMipOutput[planeIdx][0];

        m_firstMipMetadataPipeMisaligned[planeIdx] = UINT_MAX;
    }
}

// =====================================================================================================================
Image::~Image()
{
    Pal::GfxImage::Destroy();

    PAL_SAFE_FREE(m_pHtile, m_device.GetPlatform());
    PAL_SAFE_DELETE(m_pFmask, m_device.GetPlatform());
    PAL_SAFE_FREE(m_pCmask, m_device.GetPlatform());

    for (uint32  idx = 0; idx < MaxNumPlanes; idx++)
    {
        PAL_SAFE_FREE(m_pDcc[idx], m_device.GetPlatform());
        PAL_SAFE_FREE(m_pDispDcc[idx], m_device.GetPlatform());
    }
}

// =====================================================================================================================
// Saves state from the AddrMgr about a particular plane for this Image and computes the bank/pipe XOR value for
// the plane.
Result Image::Addr2FinalizePlane(
    SubResourceInfo*                               pBaseSubRes,
    void*                                          pBaseTileInfo,
    const ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT& surfaceSetting,
    const ADDR2_COMPUTE_SURFACE_INFO_OUTPUT&       surfaceInfo)
{
    const uint32 plane = pBaseSubRes->subresId.plane;

    memcpy(&m_addrSurfSetting[plane], &surfaceSetting, sizeof(m_addrSurfSetting[0]));
    memcpy(&m_addrSurfOutput[plane],  &surfaceInfo,    sizeof(m_addrSurfOutput[0]));
    m_addrSurfOutput[plane].pMipInfo = &m_addrMipOutput[plane][0];

    for (uint32 mip = 0; mip < m_createInfo.mipLevels; ++mip)
    {
        memcpy(&m_addrMipOutput[plane][mip], (surfaceInfo.pMipInfo + mip), sizeof(m_addrMipOutput[0][0]));
    }

    auto*const pTileInfo = static_cast<AddrMgr2::TileInfo*>(pBaseTileInfo);

    // Compute the pipe/bank XOR value for the subresource.
    return ComputePipeBankXor(plane, false, &surfaceSetting, &pTileInfo->pipeBankXor);
}

// =====================================================================================================================
// Finalizes the subresource info info for a single subresource, based on the results reported by AddrLib.
void Image::Addr2FinalizeSubresource(
    SubResourceInfo*                               pSubResInfo,
    const ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT& surfaceSetting
    ) const
{
    // All we need to do is evaluate whether or not this subresource can support TC compatibility.
    pSubResInfo->flags.supportMetaDataTexFetch = SupportsMetaDataTextureFetch(surfaceSetting.swizzleMode,
                                                                              pSubResInfo->format.format,
                                                                              pSubResInfo->subresId);
}

// =====================================================================================================================
// Returns constants which needs to be passed for meta data optimized clear to work
void Image::GetMetaEquationConstParam(
    MetaDataClearConst*  pParam,
    const uint32         metaBlkFastClearSize,
    bool                 cMaskMetaData
    ) const
{
    const Gfx9PalSettings& settings    = GetGfx9Settings(m_device);
    const bool optimizedFastClearDepth = ((Parent()->IsDepthStencilTarget()) &&
                                          TestAnyFlagSet(settings.optimizedFastClear,
                                                         Gfx9OptimizedFastClearDepth));
    const bool optimizedFastClearDcc   = ((Parent()->IsRenderTarget()) &&
                                          TestAnyFlagSet(settings.optimizedFastClear,
                                                         Gfx9OptimizedFastClearColorDcc));
    const bool optimizedFastClearCmask = ((Parent()->IsRenderTarget()) &&
                                          TestAnyFlagSet(settings.optimizedFastClear,
                                                         Gfx9OptimizedFastClearColorCmask));

    MetaEquationParam clearPara;

    // check if optimized fast clear is on.
    if ((optimizedFastClearDepth || optimizedFastClearDcc || optimizedFastClearCmask) &&
        // Also, optimized fast clears don't work with YUV images
        (Formats::IsYuv(Parent()->GetImageCreateInfo().swizzledFormat.format) == false))
    {
        if (m_createInfo.usageFlags.colorTarget == 1)
        {
            if (cMaskMetaData)
            {
                // Must be an MSAA color target
                PAL_ASSERT(m_createInfo.samples > 1);

                const Gfx9Cmask*const pCmask = GetCmask();
                // we must have a valid MaskRam surface
                PAL_ASSERT((pCmask != nullptr) && pCmask->HasMetaEqGenerator());
                clearPara = pCmask->GetMetaEqGenerator()->GetMetaEquationParam();
            }
            else
            {
                const Gfx9Dcc*const pDcc = GetDcc(0);
                // we must have a valid MaskRam surface
                PAL_ASSERT((pDcc != nullptr) && pDcc->HasMetaEqGenerator());
                clearPara = pDcc->GetMetaEqGenerator()->GetMetaEquationParam();
            }
        }
        else
        {
            PAL_ASSERT(m_createInfo.usageFlags.depthStencil == 1);

            const Gfx9Htile*const pHtile = GetHtile();
            // we must have a valid MaskRam surface
            PAL_ASSERT((pHtile != nullptr) && pHtile->HasMetaEqGenerator());
            clearPara = pHtile->GetMetaEqGenerator()->GetMetaEquationParam();
        }

        // metaBlocks are generally interleaved in memory except one case as defined below.
        pParam->metaInterleaved = true;

        const bool sampleHiCloseToMetaHi = ((clearPara.sampleHiBitsOffset + clearPara.sampleHiBitsLength) ==
                                             clearPara.metablkIdxHiBitsOffset);

        if ((clearPara.metablkIdxLoBitsLength == 0) && (clearPara.sampleHiBitsLength == 0))
        {
            // Metablock[all], CombinedOffset[all]
            PAL_ASSERT(clearPara.metablkIdxLoBitsOffset == 0);
            PAL_ASSERT(clearPara.sampleHiBitsOffset == 0);
            PAL_ASSERT(clearPara.metablkIdxHiBitsOffset == clearPara.metaBlkSizeLog2);

            // Number of Metablock offset low bits + sample low bits
            pParam->combinedOffsetLowBits      = 0;
            // Shift of Combined offset MSBs
            pParam->combinedOffsetHighBitShift = 0;

            // Since all metablocks are above combinedoffset bits they are not interleaved.
            pParam->metaInterleaved = false;
        }
        else if (clearPara.metablkIdxLoBitsLength == 0)
        {
            if (sampleHiCloseToMetaHi)
            {
                // Metablock[all], Sample[Hi], CombinedOffset[all]
                PAL_ASSERT(clearPara.sampleHiBitsOffset == clearPara.metaBlkSizeLog2);

                // Number of Metablock offset low bits + sample low bits
                pParam->combinedOffsetLowBits      = 0;
                // Shift of Combined offset MSBs
                pParam->combinedOffsetHighBitShift = 0;
            }
            else
            {
                // Metablock[all], CombinedOffset[Hi], Sample[Hi], CombinedOffset[Lo]
                //
                // Metablock index bits are above combined offset bits and sample hi bits
                // Sample high bits split combined offset into 2 parts
                //
                PAL_ASSERT((clearPara.metaBlkSizeLog2 + clearPara.sampleHiBitsLength) ==
                            clearPara.metablkIdxHiBitsOffset);
                PAL_ASSERT(clearPara.metablkIdxHiBitsOffset > clearPara.sampleHiBitsOffset);

                // Number of Metablock offset low bits + sample low bits
                pParam->combinedOffsetLowBits      = clearPara.sampleHiBitsOffset;
                // Shift of Combined offset MSBs
                pParam->combinedOffsetHighBitShift = clearPara.sampleHiBitsOffset + clearPara.sampleHiBitsLength;
            }
        }
        else if (clearPara.sampleHiBitsLength == 0)
        {
            // Metablock[Hi], CombinedOffset[Hi], Metablock[Lo], CombinedOffset[Lo]
            PAL_ASSERT((clearPara.metaBlkSizeLog2 + clearPara.metablkIdxLoBitsLength) ==
                        clearPara.metablkIdxHiBitsOffset);
            PAL_ASSERT(clearPara.metablkIdxHiBitsOffset > clearPara.metablkIdxLoBitsOffset);

            // Number of Metablock offset low bits + sample low bits
            pParam->combinedOffsetLowBits      = clearPara.metablkIdxLoBitsOffset;
            // Shift of Combined offset MSBs
            pParam->combinedOffsetHighBitShift = clearPara.metablkIdxLoBitsOffset + clearPara.metablkIdxLoBitsLength;
        }
        else
        {
            // Metablock[Hi], Sample[Hi], CombinedOffset[Hi], Metablock[Lo], CombinedOffset[Lo]
            PAL_ASSERT(sampleHiCloseToMetaHi);
            PAL_ASSERT((clearPara.metaBlkSizeLog2 + clearPara.metablkIdxLoBitsLength) == clearPara.sampleHiBitsOffset);

            // Number of Metablock offset low bits + sample low bits
            pParam->combinedOffsetLowBits      = clearPara.metablkIdxLoBitsOffset;
            // Shift of Combined offset MSBs
            pParam->combinedOffsetHighBitShift = clearPara.metablkIdxLoBitsOffset + clearPara.metablkIdxLoBitsLength;
        }

        // Number of Metablock offset bits and sample low bits (Combined offset bits)
        pParam->metablockSizeLog2     = clearPara.metaBlkSizeLog2;
        // Number of Metablock index bits which under metablock offset MSBs
        pParam->metaBlockLsb          = clearPara.metablkIdxLoBitsLength;
        // Shift of Metablock index MSBs
        pParam->metaBlockHighBitShift = clearPara.metablkIdxHiBitsOffset;

        // Number of Metablock offset bits and sample low bits (Combined offset bits)
        pParam->metablockSizeLog2BitMask  = (1 << pParam->metablockSizeLog2) - 1;
        // Combined offset LSBs' mask
        pParam->combinedOffsetLowBitsMask = (1 << pParam->combinedOffsetLowBits) - 1;
        // Metablock index LSBs' mask
        pParam->metaBlockLsbBitMask       = (1 << pParam->metaBlockLsb) - 1;

        PAL_ASSERT(metaBlkFastClearSize == (1u << (clearPara.metaBlkSizeLog2 + 4)));
    }
}

// =====================================================================================================================
// Calculates the byte offset from the start of bound image memory as to where each plane physically begins.
void Image::SetupPlaneOffsets()
{
    const Pal::Image*  pParent      = Parent();
    const auto&        createFormat = pParent->GetImageCreateInfo().swizzledFormat;
    const bool         isYuvPlanar  = Formats::IsYuvPlanar(createFormat.format);
    const auto&        imageInfo    = pParent->GetImageInfo();
    gpusize            planeOffset = 0;

    // Loop through all the planes associated with this surface
    for (uint32  planeIdx = 0; planeIdx < imageInfo.numPlanes; planeIdx++)
    {
        // Record where this plane starts
        m_planeOffset[planeIdx] = planeOffset;

        // Don't check the YUV status based on the return value of "planeFormat" as that will indicated the X8/X16
        // format reflective of how the data is accessed (i.e., the HW doesn't natively understand YUV operations).
        SwizzledFormat planeFormat = createFormat; // this is a don't care for this function
        pParent->DetermineFormatForPlane(&planeFormat, planeIdx);

        // Address library output is on a per-plane basis, so the mip / slice info in the sub-res is a don't care.
        const SubresId  baseSubResId    = { planeIdx, 0, 0 };
        const auto*     pBaseSubResInfo = pParent->SubresourceInfo(baseSubResId);
        const auto*     pAddrOutput     = GetAddrOutput(pBaseSubResInfo);

        // For depth/stencil surfaces, the HW assumes that each plane is stored contiguously, so store the
        // plane-offset to correspond to the size of the entire plane.
        planeOffset += (isYuvPlanar ? pAddrOutput->sliceSize : pAddrOutput->surfSize);
    } // end loop through every possible plane

    // Record the adderss where m_planeOffset starts repeating.
    m_totalPlaneSize = planeOffset;
}

// =====================================================================================================================
// Determines whether we need to set first pixel of each block that corresponds to 1 byte in DCC memory.  i.e.,
// should fast-clears be done via the CompToSingle mode
// Sets the "m_needToSetFirstPixel" variable.
void Image::CheckCompToSingle()
{
    const auto&  createInfo = Parent()->GetImageCreateInfo();
    const auto   imageType  = GetOverrideImageType();

    const bool   fragsCompressable = ((IsGfx10(m_device) == false) ||
                                      (m_gfxDevice.GetMaxFragsLog2() >= Log2(createInfo.fragments)));

    // Comp-to-single is only available on GFX10 and newer products
    if (IsGfx10Plus(m_device)           &&
        // Disable comp-to-single for 1D images.  The tiling pattern for 1D images is essentially the same pattern as
        // used by a 2D image;  i.e., for 16bpp images, it's either 16x8 or 8x16 pixels, with everything outside the
        // first scanline being wasted.  As such, the clear color needs to be written either every 8 or 16 pixels, and
        // knowing which isn't trivial.  At that rate, we're essentially doing a slow clear again.  So don't bother.
        (imageType != ImageType::Tex1d) &&
        // If the HW is compressing fewer samples then this image has, then we can't do comp-to-single as any attempt
        // by a shader to read pixels that don't fall on the DCC block boundaries will get the wrong color.  i.e., if
        // comp-to-single was done, then an attempt to read pixel (2,2) at a compressed sample would read the DCC
        // memory (since that sample was compressed) and find the real clear color at coordinates (0,0) and proceed
        // correctly. However, reading pixel (2,2) at an uncompressed sample will simply read the image memory
        // associated with that pixel/sample without looking at DCC memory and the texture pipe will have no way of
        // understanding the clear color.  We should still be able to do comp-to-reg with a fast-clear-eliminate, but
        // comp-to-single is out.
        fragsCompressable)
    {
        const Gfx9PalSettings& settings             = GetGfx9Settings(m_device);
        const SubResourceInfo*const pBaseSubResInfo = Parent()->SubresourceInfo(0);
        if (pBaseSubResInfo->bitsPerTexel <= 16)
        {
            // comp-to-single expects the clear color to be stored every 256 bytes, but we cheat and store the clear
            // color on pixel boundaries.  i.e., for an 8bpp surface, every 256 bytes should line up on a 16x16 pixel
            // grid.  16 * 16 * 1byte = 256 bytes.  However, due to the intricacies of GFX10 addressing, it doesn't
            // necessarily work out that way (i.e., with the addressing bits XOR'd together, etc).
            // And the driver can't store the clear color every 256 bytes because we need to be able to individually
            // clear subresources, so we'd need to know where each subresource begins in DCC memory.
            const  uint32 neededFlag  = (pBaseSubResInfo->bitsPerTexel == 8) ?
                                        Gfx10UseCompToSingle8bpp : Gfx10UseCompToSingle16bpp;
            m_useCompToSingleForFastClears = TestAllFlagsSet(settings.useCompToSingle, neededFlag);
        }
        else if (imageType == ImageType::Tex3d)
        {
            // 3d images are easy as they don't have arrays or MSAA.
            m_useCompToSingleForFastClears = TestAnyFlagSet(settings.useCompToSingle, Gfx10UseCompToSingle3D);
        }
        else if (TestAnyFlagSet(settings.useCompToSingle, Gfx10UseCompToSingle2d))
        {
            // Ok, if it's not a 3D image and we have DCC memory, then this is a 2D image (1D images can't have
            // DCC memory).  Make sure that any 2D images are allowed to use this feature.
            const  uint32 neededFlags  =
                (((createInfo.arraySize > 1) ? Gfx10UseCompToSingle2dArray : 0) |
                 ((createInfo.samples > 1)   ? Gfx10UseCompToSingleMsaa    : 0));

            m_useCompToSingleForFastClears = TestAllFlagsSet(settings.useCompToSingle, neededFlags);
        }
    }
}

// =====================================================================================================================
// "Finalizes" this Image object: this includes determining what metadata surfaces need to be used for this Image, and
// initializing the data structures for them.
Result Image::Finalize(
    bool               dccUnsupported,
    SubResourceInfo*   pSubResInfoList,
    void*              pTileInfoList2, // Not used in Gfx6 version
    ImageMemoryLayout* pGpuMemLayout,
    gpusize*           pGpuMemSize,
    gpusize*           pGpuMemAlignment)
{
    // For AddrMgr2 style addressing, there's no chance of a single subresource being incapable of supporting DCC.
    PAL_ASSERT(dccUnsupported == false);

    const PalSettings&          coreSettings      = m_device.Settings();
    const Gfx9PalSettings&      settings          = GetGfx9Settings(m_device);
    const auto*                 pPublicSettings   = m_device.GetPublicSettings();
    const SubResourceInfo*const pBaseSubResInfo   = pSubResInfoList;
    const SharedMetadataInfo&   sharedMetadata    = m_pImageInfo->internalCreateInfo.sharedMetadata;
    const bool                  useSharedMetadata = m_pImageInfo->internalCreateInfo.flags.useSharedMetadata;

    bool            useDcc     = false;
    HtileUsageFlags htileUsage = {};
    bool            useCmask   = false;

    Result result = Result::Success;

#if PAL_BUILD_GFX11
    if (IsGfx11(m_device) && (Parent()->IsEqaa() == false))
    {
        m_pImageInfo->resolveMethod.shaderPs = 1;
    }
#endif

    if (useSharedMetadata)
    {
        PAL_ASSERT(sharedMetadata.numPlanes == 1);

        useDcc                = (sharedMetadata.dccOffset[0] != 0);
        htileUsage.dsMetadata = (sharedMetadata.flags.htileHasDsMetadata != 0);
        useCmask              = (sharedMetadata.cmaskOffset  != 0) && (sharedMetadata.fmaskOffset != 0);

        // Fast-clear metadata is a must for shared DCC and HTILE. Sharing is disabled if it is not provided.
        // however, when dcc is shared from mesa, fastClearMetaDataOffset doesn't exist for mesa, so add the filter.
        if (useDcc &&
            (sharedMetadata.fastClearMetaDataOffset[0] == 0) && (m_createInfo.flags.sharedWithMesa != 1)
#if PAL_BUILD_GFX11
            // Fast-clear metadata do not exist on GFX11, so we should not check it.
            && (IsGfx11(m_device) == false)
#endif
            )
        {
            useDcc = false;
            result = Result::ErrorNotShareable;
        }

        if ((htileUsage.dsMetadata != 0) && (sharedMetadata.fastClearMetaDataOffset[0] == 0))
        {
            htileUsage.dsMetadata = 0;
            result                = Result::ErrorNotShareable;
        }
    }
    else
    {
        htileUsage = Gfx9Htile::UseHtileForImage(m_device, *this);
        useDcc     = Gfx9Dcc::UseDccForImage(*this, (pBaseSubResInfo->flags.supportMetaDataTexFetch != 0));
        useCmask   = Gfx9Cmask::UseCmaskForImage(m_device, *this);
    }

    // The only metadata that makes sense together is F/Cmask + DCC.
    PAL_ASSERT((htileUsage.value == 0) || ((useDcc == false) && (useCmask == false)));

    // Also determine if we need any metadata for these mask RAM objects.
    bool needsFastColorClearMetaData   = false;
    bool needsFastDepthClearMetaData   = false;
    bool needsDccStateMetaData         = false;
    bool needsHtileLookupTable         = false;
    bool needsWaTcCompatZRangeMetaData = false;
    bool needsHiSPretestsMetaData      = false;
    bool needsDccLookupTable           = false;

    // Initialize Htile:
    if (htileUsage.value != 0)
    {
        result = CreateGfx9MaskRam(m_device, *this, &m_pHtile, htileUsage);
        if (result == Result::Success)
        {
            if (useSharedMetadata)
            {
                gpusize forcedOffset = sharedMetadata.htileOffset;
                result = m_pHtile->Init(&forcedOffset, sharedMetadata.flags.hasEqGpuAccess);
                *pGpuMemSize = Max(forcedOffset, *pGpuMemSize);
            }
            else
            {
                result = m_pHtile->Init(pGpuMemSize, true);
            }

            if (result == Result::Success)
            {
                needsWaTcCompatZRangeMetaData = (m_device.GetGfxDevice()->WaTcCompatZRange() &&
                                                (m_pHtile->TileStencilDisabled() == false)   &&
                                                (pBaseSubResInfo->flags.supportMetaDataTexFetch != 0));

                if (useSharedMetadata &&
                    needsWaTcCompatZRangeMetaData &&
                    (sharedMetadata.flags.hasWaTcCompatZRange == false))
                {
                    result = Result::ErrorNotShareable;
                }
            }

            // If this hTile surface doesn't support compression, then we can't do fast clears.
            if ((result == Result::Success) && (htileUsage.dsMetadata == 1))
            {
                // Depth subresources with hTile memory must be fast-cleared either through the compute or graphics
                // engine.  Slow clears won't work as the hTile memory wouldn't get updated.
                ClearMethod fastClearMethod;
                switch (pPublicSettings->fastDepthStencilClearMode)
                {
                case FastDepthStencilClearMode::Graphics:
                    fastClearMethod = ClearMethod::DepthFastGraphics;
                    break;

                case FastDepthStencilClearMode::Compute:
                    fastClearMethod = ClearMethod::Fast;
                    break;

                case FastDepthStencilClearMode::Default:
                default:
                    fastClearMethod = (useSharedMetadata && (sharedMetadata.flags.hasEqGpuAccess == false))
                                      ? ClearMethod::DepthFastGraphics
                                      : ClearMethod::Fast;
                    break;
                }

                const bool supportsDepth   =
                    m_device.SupportsDepth(m_createInfo.swizzledFormat.format, m_createInfo.tiling);
                const bool supportsStencil =
                    m_device.SupportsStencil(m_createInfo.swizzledFormat.format, m_createInfo.tiling);

                for (uint32 mip = 0; ((mip < m_createInfo.mipLevels) && (result == Result::Success)); ++mip)
                {
                    if (CanMipSupportMetaData(mip))
                    {
                        if (supportsDepth)
                        {
                            UpdateClearMethod(pSubResInfoList, 0, mip, fastClearMethod);
                        }

                        if (supportsStencil)
                        {
                            UpdateClearMethod(pSubResInfoList,
                                              GetStencilPlane(),
                                              mip,
                                              fastClearMethod);
                        }
                    }
                }

                needsFastDepthClearMetaData = true;

                needsHiSPretestsMetaData = useSharedMetadata ? (sharedMetadata.hisPretestMetaDataOffset != 0)
                                           : ((htileUsage.dsMetadata != 0) && supportsStencil);

                // It's possible for the metadata allocation to require more alignment than the base allocation. Bump
                // up the required alignment of the app-provided allocation if necessary.
                *pGpuMemAlignment = Max(*pGpuMemAlignment, m_pHtile->Alignment());

                UpdateMetaDataLayout(pGpuMemLayout, m_pHtile->MemoryOffset(), m_pHtile->Alignment());

                const auto&  hTileAddrOutput      = m_pHtile->GetAddrOutput();
                const uint32 metaBlkFastClearSize = hTileAddrOutput.sliceSize / hTileAddrOutput.metaBlkNumPerSlice;

                // Get the constant data for clears based on Htile meta equation
                GetMetaEquationConstParam(&m_metaDataClearConst[MetaDataHtile], metaBlkFastClearSize);

                if (useSharedMetadata == false)
                {
                    if (Parent()->IsResolveSrc() || Parent()->IsResolveDst())
                    {
                        needsHtileLookupTable = true;
                    }
                    if (IsGfx10Plus(m_device))
                    {
                        needsHtileLookupTable = false;
                    }
                }
                else
                {
                    needsHtileLookupTable = sharedMetadata.flags.hasHtileLookupTable;
                }
            }
        }
    } // End check for needing hTile data

    // Initialize DCC:
    if (useDcc && (result == Result::Success))
    {
        // If we have DCC but don't suport shader write(e.g. gfx9), we'll use graphic copy by default for scaled copy.
        if (IsGfx9(m_device))
        {
            m_pParent->SetPreferGraphicsScaledCopy(true);
        }

        result = CreateDccObject(pSubResInfoList, pGpuMemLayout, pGpuMemSize, pGpuMemAlignment);

        if (result == Result::Success)
        {
            // Set up the size & GPU offset for the fast-clear metadata. Only need to do this once for all mip
            // levels. The HW will only use this data if fast-clears have been used, but the fast-clear meta data
            // is used by the driver if DCC memory is present for any reason, so we always need to do this.
            // SEE: Gfx9ColorTargetView::WriteCommands for details.
            needsFastColorClearMetaData = true;

            if (useSharedMetadata)
            {
                needsDccStateMetaData = (sharedMetadata.dccStateMetaDataOffset[0] != 0);
            }
            else
            {
                // We also need the DCC state metadata when DCC is enabled.
                needsDccStateMetaData = true;
            }

            // As CP doesn't support to LOAD_***REG/SET_PREDICATION/WRITE_DATA on TMZ images,
            // Fast clears on TMZ images are only supported if they are TC-compatible and the
            // clear value is TC-compatible, so there is no need to load the clear colors or FCE.
            // Also disable DCC state metadata to avoid using predication on TMZ images.
            if (m_pParent->IsTmz())
            {
                needsFastColorClearMetaData = false;
                needsDccStateMetaData = false;
            }
#if PAL_BUILD_GFX11
            else if (IsGfx11(m_device))
            {
                // The fast-color-clear meta-data is used for storing values associated with the
                // CB_COLORx_CLEAR_WORDx register(s); these registers do not exist on GFX11 and
                // thus we don't need this memory.
                //
                // The "needsDccStateMetaData" allocation is different; that is used to prevent
                // unnecessary decompresses and is still useful for GFX11.
                needsFastColorClearMetaData = false;
            }
#endif
        }
    } // End check for (useDcc != false)

    // Initialize Cmask:
    if (useCmask && (result == Result::Success))
    {
        // Cmask setup depends on Fmask swizzle mode, so setup Fmask first.
        m_pFmask = PAL_NEW(Gfx9Fmask, m_device.GetPlatform(), SystemAllocType::AllocObject)();
        if (m_pFmask != nullptr)
        {
            if (useSharedMetadata)
            {
                gpusize forcedOffset = sharedMetadata.fmaskOffset;
                result = m_pFmask->Init(*this, &forcedOffset);
                *pGpuMemSize = Max(forcedOffset, *pGpuMemSize);
            }
            else
            {
                result = m_pFmask->Init(*this, pGpuMemSize);
            }

            if ((m_createInfo.flags.repetitiveResolve != 0) || (coreSettings.forceFixedFuncColorResolve != 0))
            {
                // According to the CB Micro-Architecture Specification, it is illegal to resolve a 1 fragment eqaa
                // surface.
                if ((Parent()->IsEqaa() == false) || (m_createInfo.fragments > 1))
                {
                    m_pImageInfo->resolveMethod.fixedFunc = 1;
                }
            }

            // It's possible for the metadata allocation to require more alignment than the base allocation. Bump
            // up the required alignment of the app-provided allocation if necessary.
            *pGpuMemAlignment = Max(*pGpuMemAlignment, m_pFmask->Alignment());

            // Update the layout information against the fMask offset and alignment requirements.
            UpdateMetaDataLayout(pGpuMemLayout, m_pFmask->MemoryOffset(), m_pFmask->Alignment());

            // NOTE: If FMask is present, use the FMask-accelerated resolve path.
            m_pImageInfo->resolveMethod.shaderCsFmask = 1;
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }

        // On GFX9, Cmask and fmask go together. There's no point to having just one of them.
        if (result == Result::Success)
        {
            result = CreateGfx9MaskRam(m_device, *this, &m_pCmask);
            if (result == Result::Success)
            {
                if (useSharedMetadata)
                {
                    gpusize forcedOffset = sharedMetadata.cmaskOffset;
                    result = m_pCmask->Init(&forcedOffset, sharedMetadata.flags.hasCmaskEqGpuAccess);
                    *pGpuMemSize = Max(forcedOffset, *pGpuMemSize);
                }
                else
                {
                    result = m_pCmask->Init(pGpuMemSize, true);
                }

                // It's possible for the metadata allocation to require more alignment than the base allocation. Bump
                // up the required alignment of the app-provided allocation if necessary.
                *pGpuMemAlignment = Max(*pGpuMemAlignment, m_pCmask->Alignment());

                // Update the layout information against the cMask offset and alignment requirements.
                UpdateMetaDataLayout(pGpuMemLayout, m_pCmask->MemoryOffset(), m_pCmask->Alignment());

                const auto&  addrOutput           = m_pCmask->GetAddrOutput();
                const uint32 metaBlkFastClearSize = addrOutput.sliceSize / addrOutput.metaBlkNumPerSlice;

                // Get the constant data for clears based on cmask meta equation
                GetMetaEquationConstParam(&m_metaDataClearConst[MetaDataCmask], metaBlkFastClearSize, true);
            }
        }
    } // End check for (useCmask != false)

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
                for (uint32  plane = 0; plane < sharedMetadata.numPlanes; plane++)
                {
                    gpusize forcedOffset = sharedMetadata.fastClearMetaDataOffset[plane];

                    InitFastClearMetaData(pGpuMemLayout,
                                          &forcedOffset,
                                          sizeof(Gfx9FastColorClearMetaData),
                                          sizeof(uint32),
                                          plane);
                    *pGpuMemSize = Max(forcedOffset, *pGpuMemSize);
                }
            }
            else
            {
                for (uint32  dccPlane = 0; dccPlane < m_numDccPlanes; dccPlane++)
                {
                    InitFastClearMetaData(pGpuMemLayout,
                                          pGpuMemSize,
                                          sizeof(Gfx9FastColorClearMetaData),
                                          sizeof(uint32),
                                          dccPlane);
                }
            }
        }
        else if (needsFastDepthClearMetaData)
        {
            if (useSharedMetadata)
            {
                // Depth / stencil surfaces share the same hTile data, so there will only be one meta-surface
                // that contains fast-clear data.
                PAL_ASSERT(sharedMetadata.numPlanes == 1);

                gpusize forcedOffset = sharedMetadata.fastClearMetaDataOffset[0];
                InitFastClearMetaData(pGpuMemLayout, &forcedOffset, sizeof(Gfx9FastDepthClearMetaData), sizeof(uint32));
                *pGpuMemSize = Max(forcedOffset, *pGpuMemSize);
            }
            else
            {
                InitFastClearMetaData(pGpuMemLayout, pGpuMemSize, sizeof(Gfx9FastDepthClearMetaData), sizeof(uint32));
            }
        }

        // Set up the GPU offset for the HiSPretests metadata
        if (needsHiSPretestsMetaData && GetGfx9Settings(m_device).hiStencilEnable)
        {
            if (useSharedMetadata)
            {
                gpusize forcedOffset = sharedMetadata.hisPretestMetaDataOffset;
                InitHiSPretestsMetaData(pGpuMemLayout, &forcedOffset, sizeof(Gfx9HiSPretestsMetaData), sizeof(uint32));
                *pGpuMemSize = Max(forcedOffset, *pGpuMemSize);
            }
            else
            {
                InitHiSPretestsMetaData(pGpuMemLayout, pGpuMemSize, sizeof(Gfx9HiSPretestsMetaData), sizeof(uint32));
            }
        }

        // For shared metadata, the Z Range workaround metadata offset is not listed but following the
        // FastClearMetaData.  Set up the GPU offset for the waTcCompatZRange metadata
        if (needsWaTcCompatZRangeMetaData)
        {
            InitWaTcCompatZRangeMetaData(pGpuMemLayout, pGpuMemSize);
        }

        // Set up the GPU offset for the DCC state metadata.
        if (needsDccStateMetaData)
        {
            if (useSharedMetadata)
            {
                for (uint32 plane = 0; plane < sharedMetadata.numPlanes; plane++)
                {
                    gpusize forcedOffset = sharedMetadata.dccStateMetaDataOffset[plane];
                    InitDccStateMetaData(plane, pGpuMemLayout, &forcedOffset);
                    *pGpuMemSize = Max(forcedOffset, *pGpuMemSize);
                }
            }
            else
            {
                for (uint32  dccPlaneIdx = 0; dccPlaneIdx < m_numDccPlanes; dccPlaneIdx++)
                {
                    InitDccStateMetaData(dccPlaneIdx, pGpuMemLayout, pGpuMemSize);
                }
            }
        }

        // Texture-compatible color images on can only be fast-cleared to certain colors; otherwise the TC won't
        // understand the color data.  For non-supported fast-clear colors, we can either
        //    a) do a slow-clear of the image
        //    b) fast-clear the image anyway and do a fast-clear-eliminate pass when the image is bound as a texture.
        //
        // So, if all these conditions are true:
        //    a) This image supports fast-clears in the first place
        //    b) This is a color image
        //    c) We always fast-clear regardless of the clear-color (meaning a fast-clear eliminate will be required)
        //    d) This image is going to be used as a texture
        //    e) Image not use comp-to-single or use comp-to-single but not disable comp-to-reg
        //
        // Then setup memory to be used to conditionally-execute the fast-clear-eliminate pass based on the clear-color.
        if (needsFastColorClearMetaData                           &&
            ColorImageSupportsAllFastClears()                     &&
            (pBaseSubResInfo->flags.supportMetaDataTexFetch != 0) &&
            ((Gfx10UseCompToSingleFastClears() == false) ||
             (TestAnyFlagSet(settings.useCompToSingle, Gfx10DisableCompToReg) == false)))
        {
            if (useSharedMetadata)
            {
                for (uint32  dccIdx = 0; dccIdx < sharedMetadata.numPlanes; dccIdx++)
                {
                    gpusize forcedOffset = sharedMetadata.fastClearEliminateMetaDataOffset[dccIdx];
                    if (forcedOffset != 0)
                    {
                        InitFastClearEliminateMetaData(dccIdx, pGpuMemLayout, &forcedOffset);
                        *pGpuMemSize = Max(forcedOffset, *pGpuMemSize);
                    }
                }
            }
            else
            {
                for (uint32  dccIdx = 0; dccIdx < m_numDccPlanes; dccIdx++)
                {
                    InitFastClearEliminateMetaData(dccIdx, pGpuMemLayout, pGpuMemSize);
                }
            }
        }

        if ((settings.enableMallCursorCache == true) && (m_mallCursorCacheSize > 0))
        {
            // Mall cursor cache has a requirement from DAL that the cursor cache starts on a 2KB aligned
            // address. Adjust the original allocation size accordingly before saving off our offset.
            *pGpuMemSize = Pow2Align(*pGpuMemSize, 2048);
            // Add the size of the mall cursor cache to the allocation size and save the original value off
            // in order to inform the KMD of the offset to the cursor cache.
            m_mallCursorCacheOffset = *pGpuMemSize;
            *pGpuMemSize += m_mallCursorCacheSize;
        }

        // NOTE: We're done adding bits of GPU memory to our image; its GPU memory size is now final.

        // If we have a valid metadata header offset we also need a metadata header size.
        if (pGpuMemLayout->metadataHeaderOffset != 0)
        {
            pGpuMemLayout->metadataHeaderSize = (*pGpuMemSize - pGpuMemLayout->metadataHeaderOffset);
        }

        if (needsHtileLookupTable)
        {
            if (useSharedMetadata)
            {
                gpusize forcedOffset = sharedMetadata.htileLookupTableOffset;
                InitHtileLookupTable(pGpuMemLayout, &forcedOffset, pGpuMemAlignment);
                *pGpuMemSize = Max(forcedOffset, *pGpuMemSize);
            }
            else
            {
                InitHtileLookupTable(pGpuMemLayout, pGpuMemSize, pGpuMemAlignment);
            }
        }

        if (needsDccLookupTable)
        {
            if (useSharedMetadata == false)
            {
                // Currently, shared dcc lookup table is not available.
                InitDccLookupTable(pGpuMemLayout, pGpuMemSize, pGpuMemAlignment);
            }
        }

        m_gpuMemSyncSize = *pGpuMemSize;

        // Force its size 16 bytes aligned so it's able to go through the fastest CopyBufferDqword in
        // CopyMemoryCs (e.g. called by CmdCopyMemory or CmdCloneImageData).
        *pGpuMemSize = Pow2Align(*pGpuMemSize, 16);

        if (useCmask && settings.waCmaskImageSyncs)
        {
            // Keep the size to sync the same, and pad the required allocation size up to the next fragment multiple.
            *pGpuMemSize = Pow2Align(*pGpuMemSize, m_device.MemoryProperties().fragmentSize);
        }

        InitLayoutStateMasks();
        InitPipeMisalignedMetadataFirstMip();

        if (m_createInfo.flags.prt != 0)
        {
            m_device.GetAddrMgr()->ComputePackedMipInfo(*Parent(), pGpuMemLayout);
        }

#if PAL_DEVELOPER_BUILD
        const ImageInternalCreateInfo* pImageInternalCreateInfo = &(m_pImageInfo->internalCreateInfo);
        if (m_pParent != nullptr)
        {
            const SubResourceInfo* pSubResInfo = m_pParent->SubresourceInfo(0);
            if (pSubResInfo != nullptr)
            {
                Developer::ImageDataAddrMgrSurfInfo data = {};

                data.tiling.gfx9.swizzle = m_addrSurfSetting[0].swizzleMode;

                data.flags.properties.color = m_createInfo.usageFlags.colorTarget;
                data.flags.properties.depth = m_createInfo.usageFlags.depthStencil;
                data.flags.properties.stencil = (m_createInfo.usageFlags.noStencilShaderRead == 0);
                data.flags.properties.texture = m_createInfo.usageFlags.shaderRead;
                data.flags.properties.volume = (m_createInfo.imageType == ImageType::Tex3d);
                data.flags.properties.cube = m_createInfo.flags.cubemap;
                data.flags.properties.fmask = HasFmaskData();
                data.flags.properties.display = m_createInfo.flags.flippable;
                data.flags.properties.prt = m_createInfo.flags.prt;
                data.flags.properties.tcCompatible = pSubResInfo->flags.supportMetaDataTexFetch;
                data.flags.properties.dccCompatible = HasDccData();

                data.size = m_pParent->GetGpuMemSize();
                data.bpp = pSubResInfo->bitsPerTexel;
                data.width = m_createInfo.extent.width;
                data.height = m_createInfo.extent.height;
                data.depth = m_createInfo.extent.depth;

                m_device.DeveloperCb(Developer::CallbackType::CreateImage, &data);
            }
        }
#endif
    }

    return result;
}

// =====================================================================================================================
// "Finalizes" this Image object: this includes determining what metadata surfaces need to be used for this Image, and
// initializing the data structures for them.
Result Image::CreateDccObject(
    SubResourceInfo*   pSubResInfoList,
    ImageMemoryLayout* pGpuMemLayout,
    gpusize*           pGpuMemSize,
    gpusize*           pGpuMemAlignment)
{
    const bool                  useSharedMetadata = m_pImageInfo->internalCreateInfo.flags.useSharedMetadata;
    const SharedMetadataInfo&   sharedMetadata    = m_pImageInfo->internalCreateInfo.sharedMetadata;

    Result  result = Result::Success;

    for (uint32  planeIdx = 0; ((result == Result::Success) && (planeIdx < m_pImageInfo->numPlanes)); planeIdx++)
    {
        const SubresId          planeBaseSubResId    = { planeIdx, 0, 0 };
        const SubResourceInfo*  pPlaneBaseSubResInfo = Parent()->SubresourceInfo(planeBaseSubResId);

        // There is nothing mip-level specific about DCC on Gfx9+, so we just have one DCC objct that represents the
        // entire DCC allocation.
        result = CreateGfx9MaskRam(m_device, *this, &m_pDcc[planeIdx], false);
        if (result == Result::Success)
        {
            // Determine whether or not we need to set the first pixel of each block that corresponds to each
            // byte of DCC memory.  This result is used during DCC initialization.
            CheckCompToSingle();

            Gfx9Dcc*  pDcc = m_pDcc[planeIdx];

            if (useSharedMetadata)
            {
                PAL_ASSERT(planeIdx < sharedMetadata.numPlanes);

                gpusize forcedOffset = sharedMetadata.dccOffset[planeIdx];
                result = pDcc->Init(planeBaseSubResId, &forcedOffset, sharedMetadata.flags.hasEqGpuAccess);
                *pGpuMemSize = Max(forcedOffset, *pGpuMemSize);
            }
            else
            {
                result = pDcc->Init(planeBaseSubResId, pGpuMemSize, true);
            }

            if (result == Result::Success)
            {
                if ((useSharedMetadata == false) || (sharedMetadata.flags.hasEqGpuAccess == true))
                {
                    const auto& surfSettings = GetAddrSettings(pPlaneBaseSubResInfo);

                    if (Gfx9Dcc::SupportFastColorClear(m_device, *this, surfSettings.swizzleMode))
                    {
                        for (uint32 mip = 0; mip < m_createInfo.mipLevels; ++mip)
                        {
                            // Enable fast Clear support for RTV/SRV or if we have a mip chain in which some mips aren't
                            // going to be used as UAV but some can be then we enable dcc fast clear on those who aren't
                            // going to be used as UAV and disable dcc fast clear on other mips.
                            // On Gfx10+, shader writable surface also can have a DCC memory so we can still fast clear.
                            if ((m_createInfo.usageFlags.shaderWrite == 0) ||
                                IsGfx10Plus(m_device)                      ||
                                (mip < m_createInfo.usageFlags.firstShaderWritableMip))
                            {
                                if (CanMipSupportMetaData(mip))
                                {
                                    UpdateClearMethod(pSubResInfoList, planeIdx, mip, ClearMethod::Fast);
                                }
                            }
                        } // end loop through all the mip levels
                    } // end check for this image supporting fast clears at all
                }

                // It's possible for the metadata allocation to require more alignment than the base allocation. Bump
                // up the required alignment of the app-provided allocation if necessary.
                *pGpuMemAlignment = Max(*pGpuMemAlignment, pDcc->Alignment());

                // Update the layout information against mip 0's DCC offset and alignment requirements.
                UpdateMetaDataLayout(pGpuMemLayout, pDcc->MemoryOffset(), pDcc->Alignment());

                const auto&  addrOutput           = pDcc->GetAddrOutput();
                const uint32 metaBlkFastClearSize = addrOutput.fastClearSizePerSlice / addrOutput.metaBlkNumPerSlice;

                if (m_pImageInfo->internalCreateInfo.displayDcc.enabled != 0)
                {
                    result = CreateGfx9MaskRam(m_device, *this, &m_pDispDcc[planeIdx], true);
                    if (result == Result::Success)
                    {
                        m_pDispDcc[planeIdx]->Init(planeBaseSubResId, pGpuMemSize, true);
                        PAL_ASSERT(pDcc->GetControlReg().u32All == m_pDispDcc[planeIdx]->GetControlReg().u32All);
                    }
                }
                m_numDccPlanes++;

                // Get the constant data for clears based on DCC meta equation
                GetMetaEquationConstParam(&m_metaDataClearConst[MetaDataDcc], metaBlkFastClearSize);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// The CopyImageToMemory functions use the same format for the source and destination (i.e., image and buffer).
// Not all image formats are supported as buffer formats.  If the format doesn't work for both, then we need
// to force decompressions which will force image-replacement in the copy code.
bool Image::DoesImageSupportCopyCompression() const
{
    const Platform&   platform            = *m_device.GetPlatform();
    const GfxIpLevel  gfxLevel            = m_device.ChipProperties().gfxLevel;
    ChNumFormat       copyFormat          = m_createInfo.swizzledFormat.format;
    bool              supportsCompression = true;

    // CmdCopyImageToMemory always forces sRGB to UNORM.
    copyFormat = IsSrgb(copyFormat) ? Formats::ConvertToUnorm(copyFormat) : copyFormat;

    if (gfxLevel == GfxIpLevel::GfxIp9)
    {
        const auto*const      pFmtInfo        = MergedChannelFmtInfoTbl(gfxLevel, &platform.PlatformSettings());
        const BUF_DATA_FORMAT hwBufferDataFmt = HwBufDataFmt(pFmtInfo, copyFormat);

        supportsCompression = (hwBufferDataFmt != BUF_DATA_FORMAT_INVALID);
    }
    else
    {
        const auto*const pFmtInfo       = MergedChannelFlatFmtInfoTbl(gfxLevel, &platform.PlatformSettings());
        const BUF_FMT    hwBufferFormat = HwBufFmt(pFmtInfo, copyFormat);

        supportsCompression = (hwBufferFormat != BUF_FMT_INVALID);
    }

    return supportsCompression;
}

// =====================================================================================================================
// Initializes the layout-to-state masks which are used by Device::Barrier() to determine which operations are needed
// when transitioning between different Image layouts. DefaultGfxLayouts are used by Device::Acquire() and
// Device::Release().
void Image::InitLayoutStateMasks()
{
    const SubResourceInfo*const pBaseSubResInfo = Parent()->SubresourceInfo(0);
    const bool isComprFmaskShaderReadable       = IsComprFmaskShaderReadable({});
    const bool                  isMsaa          = (m_createInfo.samples > 1);

    if (HasColorMetaData())
    {
        PAL_ASSERT(Parent()->IsDepthStencilTarget() == false);

        // Always allow compression for layouts that only support the color target usage.
        ImageLayout compressedWriteLayout = {};
        compressedWriteLayout.usages  = LayoutColorTarget;
        compressedWriteLayout.engines = LayoutUniversalEngine;

        ImageLayout compressedLayout        = {};
        ImageLayout fmaskDecompressedLayout = {};

        // Additional usages may be allowed for an image in the compressed state.
        if (pBaseSubResInfo->flags.supportMetaDataTexFetch != 0)
        {
            const Gfx9PalSettings& settings = GetGfx9Settings(m_device);
            // In Gfx10, UAV surface can have a DCC memory. So we allow compression for:
            //   - ShaderWrite (because the app can write compressed to the surface)
            //   - CopyDst     (because PAL copies can write compressed to the surface)
            // We don't add ResolveDst here because the CB requires that the destination of a FixedFunction resolve must
            // be decompressed. Additionally, after a FixedFunction resolve the CB does not compress the destination.
            // Because of this, we can't let ResolveDst be considered a "compressed" state, nor can we allow our compute
            // resolve to compress the destination data. However, if we are not doing a fixed func resolve, the shader
            // will compress the resolve dst, so we can add ResolveDst to the compressedWriteLayout group.
            if (IsGfx10Plus(m_device))
            {
                compressedWriteLayout.usages |= LayoutShaderWrite;

                // Ensure that images with replacable formats do not have the LayoutCopyDst compression usage. This
                // should prevent compression corruption during memory to image copies.
                if (DoesImageSupportCopyCompression())
                {
                    compressedWriteLayout.usages |= LayoutCopyDst;
                }

                // We don't have to check for MSAA here because nobody should transition an MSAA image to ResolveDst
                if (m_pImageInfo->resolveMethod.fixedFunc == 0)
                {
                    compressedWriteLayout.usages |= LayoutResolveDst;
                }
            }
            if (TestAnyFlagSet(UseComputeExpand, UseComputeExpandDcc) &&
                ((isMsaa == false)         ||
                 (HasFmaskData() == false) ||
                 TestAnyFlagSet(UseComputeExpand, UseComputeExpandDccWithFmask)))
            {
                compressedWriteLayout.engines |= LayoutComputeEngine;
            }

            if (isMsaa)
            {
                // Both CmdCopyImage and CmdCopyImageToMemory support Fmask based non-eqaa msaa access to CopySrc. So
                // can keep colorCompressed for non-eqaa color msaa image if it supports supportMetaDataTexFetch.
                if (DoesImageSupportCopyCompression() && (m_createInfo.samples == m_createInfo.fragments))
                {
                    compressedLayout.usages |= LayoutCopySrc;
                }

                // As stated above we only land up here if dcc is allocated and we are tc-compatible and also in
                // this case on gfxip8 we will have fmask surface tc-compatible, which means we can keep colorcompressed
                // for fmaskbasedmsaaread
                compressedLayout.usages |= LayoutShaderFmaskBasedRead;
            }
            else
            {
                if (DoesImageSupportCopyCompression())
                {
                    // Our copy path has been designed to allow compressed copy sources.
                    compressedLayout.usages |= LayoutCopySrc;
                }

                // We can keep this layout compressed if all view formats are DCC compatible.
                if (Parent()->GetDccFormatEncoding() != DccFormatEncoding::Incompatible)
                {
                    // LayoutSampleRate just means "read from this in RPM's VRS copy shader".
                    compressedLayout.usages |= LayoutShaderRead | LayoutSampleRate;
                }
            }

#if PAL_BUILD_GFX11
            // Gfx11 can always support compressed reads if the image is tc-compatible
            if (IsGfx11(m_device))
            {
                compressedLayout.usages |= LayoutShaderRead;
            }
#endif
            const GfxIpLevel  gfxLevel = m_device.ChipProperties().gfxLevel;

            if (HasDccData() && (gfxLevel == GfxIpLevel::GfxIp10_1))
            {
                // Verify that transitions to presentable state will invoke a DCC decompress on GFX10 for texture-
                // fetchable images.
                PAL_ASSERT(((pBaseSubResInfo->bitsPerTexel / 8) < 4) ||
                           TestAnyFlagSet(compressedLayout.usages, LayoutPresentFullscreen) == false);
            }

            if (HasDisplayDccData())
            {
                compressedLayout.usages |= LayoutPresentWindowed | LayoutPresentFullscreen;
            }
        } // if supportMetaDataTexFetch
        else if (isMsaa && isComprFmaskShaderReadable)
        {
            // We can't be tc-compatible here
            PAL_ASSERT(pBaseSubResInfo->flags.supportMetaDataTexFetch == 0);

            // Also since we can't be tc-compatible we must not have dcc data
            PAL_ASSERT(HasDccData() == false);

#if PAL_BUILD_GFX11
            PAL_ASSERT(IsGfx11(m_device) == false);
#endif

            // The only case it won't work if DCC is allocated and yet this surface is not tc-compatible, if dcc
            // was never allocated then we can keep entire image color compressed (isComprFmaskShaderReadable takes
            // care of it).
            compressedLayout.usages |= LayoutShaderFmaskBasedRead;
        }

        // The Fmask-decompressed state is only valid for MSAA images.  This state implies that the base color data
        // is still compressed, but fmask is expanded so that it is readable by the texture unit even if metadata
        // texture fetches are not supported.
        if (isMsaa)
        {
            // If copy to the image is always fully copy, fix up metadata to uncompressed state instead of heavy expand
            // at barrier to "LayoutCopyDst" if this isn't a compressed copy.
            if (m_createInfo.flags.fullCopyDstOnly != 0)
            {
                compressedLayout.usages |= LayoutCopyDst;
            }

            if (HasFmaskData())
            {
                // Our copy path has been designed to allow color compressed MSAA copy sources.
                fmaskDecompressedLayout.usages = LayoutColorTarget | LayoutCopySrc;

                // Resolve can take 3 different paths inside pal:-
                // a. FixedFuncHWResolve :- in this case since CB does all the work we can keep everything compressed.
                // b. ShaderBasedResolve (when format match/native resolve):- We can keep entire color compressed and
                // hence also in fmaskdecompressed state. If we have a DCC surface but no tc-compatibility even that
                // case is not a problem since at barrier time we will issue a dccdecompress
                // c. ShaderBasedResolve (when format don't match) :- we won't have dcc surface in this case and hence
                //  it is completely fine to keep color into fmaskdecompressed state.
                fmaskDecompressedLayout.usages |= LayoutResolveSrc;

                // We can keep this resource into Fmaskcompressed state since barrier will handle any corresponding
                // decompress for cases when dcc is present and we are not tc-compatible.
                fmaskDecompressedLayout.usages |= LayoutShaderFmaskBasedRead;

                fmaskDecompressedLayout.engines = LayoutUniversalEngine | LayoutComputeEngine;
            }
        }

        // ResolveDst for an MSAA image is impossible and ResolveSrc is impossible for non-MSAA images,
        // but we are including it here for easier client implementation. If the implementation is impossible
        // then we don't need to decompress because of it since it can't happen. This could come up of a client
        // has a simple implementation where e.g. GENERAL layout includes every usage without checking image caps.

#if PAL_BUILD_GFX11
        // GFX11 doesn't support fixed function resolves, so LayoutResolveSrc can only remain compressed
        // if the image is TC-compatible.
        if ((IsGfx11(m_device) == false) || pBaseSubResInfo->flags.supportMetaDataTexFetch)
#endif
        {
            compressedLayout.usages |= LayoutResolveSrc;
        }

        // If the image is always be fully overwritten when being resolved:
        // a. Fix-function/Compute Shader resolve :- There is no need to issue DccExpand at barrier time.
        // We can do dcc fixup after the resolve.
        // b. Pixel shader resolve :- There is no need to issue DccExpand at barrier time.
        if (m_createInfo.flags.fullResolveDstOnly == 1)
        {
            compressedLayout.usages |= LayoutResolveDst;
        }

        // The compressWriteLayout set must always be a subset of the compressedLayout set.
        compressedLayout.usages  |= compressedWriteLayout.usages;
        compressedLayout.engines |= compressedWriteLayout.engines;

        m_layoutToState.color.compressedWrite   = compressedWriteLayout;
        m_layoutToState.color.compressed        = compressedLayout;
        m_layoutToState.color.fmaskDecompressed = fmaskDecompressedLayout;

        m_defaultGfxLayout.color = compressedLayout;

        // If this trips, the SDMA engine is in danger of seeing compressed images, which it can't understand.
        PAL_ASSERT((GetGfx9Settings(m_device).waSdmaPreventCompressedSurfUse == false) ||
                   (TestAnyFlagSet(m_layoutToState.color.compressed.engines, LayoutDmaEngine) == false));
    }
    else if (HasDsMetadata())
    {
        PAL_ASSERT(Parent()->IsDepthStencilTarget());

        // Identify usages supporting DB rendering
        constexpr uint32 DbUsages = LayoutDepthStencilTarget;

        // NOTE: we also have DB-based resolve and copy paths, but we choose compute-based paths for those for
        // depth-stencil.  The path also does not yet check the layout at all.  That is why here we do not
        // report them as being DB-compatible layouts.

        // Identify the supported shader readable usages.
        // LayoutSampleRate just means "read from this in RPM's VRS copy shader".
        constexpr uint32 ShaderReadUsages = LayoutCopySrc | LayoutResolveSrc | LayoutShaderRead | LayoutSampleRate;

        // Layouts that are decompressed (with hiz enabled) support both depth rendering and shader reads (though
        // not shader writes) in the universal queue and compute queue.
        // For resolve dst, HiZ is always valid whatever pixel shader resolve or depth-stencil copy resolve performed:
        // 1. Htile is valid during pixel shader resolve.
        // 2. Htile copy-and-fix-up will be performed after depth-stencil copy resolve to ensure HiZ to be valid.
        ImageLayout decomprWithHiZLayout;

        decomprWithHiZLayout.usages  = DbUsages | ShaderReadUsages | LayoutResolveDst;
        decomprWithHiZLayout.engines = LayoutUniversalEngine | LayoutComputeEngine;

        // If the client has given us a hint that this Image never does anything to this Image which would cause
        // the Image data and Hi-Z to become out-of-sync, we can include all layouts in the decomprWithHiZ state
        // because this Image will never need to do a resummarization blit.
        if (m_createInfo.usageFlags.hiZNeverInvalid != 0)
        {
            decomprWithHiZLayout.usages  = AllDepthImageLayoutFlags;
            decomprWithHiZLayout.engines = LayoutUniversalEngine | LayoutComputeEngine | LayoutDmaEngine;
        }

        // Layouts that are compressed support all DB compatible usages in the universal queue
        ImageLayout compressedLayouts;

        compressedLayouts.usages  = DbUsages;
        compressedLayouts.engines = LayoutUniversalEngine;

        // Setting ResolveSrc for non-MSAA images and ResolveDst for MSAA images is incorrect because it
        // is an impossible combination, but it makes it easier on the client and doesn't hurt. If the implementation
        // is impossible then we don't need to decompress because of it since it can't happen. This could come up of
        // a client has a simple implementation where e.g. GENERAL layout includes every usage without checking
        // image caps.
        if (Formats::BitsPerPixel(m_createInfo.swizzledFormat.format) == 8)
        {
            // Decompress stencil only format image does not need sample location information
            compressedLayouts.usages |= LayoutResolveSrc;
        }
        else
        {
            bool sampleLocsAlwaysKnown = m_createInfo.flags.sampleLocsAlwaysKnown;

            // In Gfx10, sample location will always be stored in MSAA depth buffer
            if (IsGfx10Plus(m_device))
            {
                sampleLocsAlwaysKnown = true;
            }

            // Postpone decompresses for HTILE from Barrier-time to Resolve-time if sample location is always known.
            if (sampleLocsAlwaysKnown)
            {
                compressedLayouts.usages |= LayoutResolveSrc;
            }
        }

        // If the depth-stencil image is always be fully overwritten when being resolved:
        // a. Fix-function/Compute Shader resolve :- Instead of expanding HTILE, we can fixup HTILE after resolve.
        // b. Pixel shader resolve :- There is no need to expand HTILE.
        if (m_createInfo.flags.fullResolveDstOnly != 0)
        {
            compressedLayouts.usages |= LayoutResolveDst;
        }

        // Copy to depth can always support compression. GFX BLTs always support compression and CS BLTs support
        // compression on GFX10+. For CS BLTs on earlier hardware, RPM will automatically fixup the metadata after
        // the copy.
        compressedLayouts.usages |= LayoutCopyDst;

        // With a TC-compatible htile, even the compressed layout is shader-readable
        if (pBaseSubResInfo->flags.supportMetaDataTexFetch != 0)
        {
            compressedLayouts.usages |= ShaderReadUsages;

            const bool supportsDepth = m_device.SupportsDepth(m_createInfo.swizzledFormat.format,
                m_createInfo.tiling);
            const bool supportsStencil = m_device.SupportsStencil(m_createInfo.swizzledFormat.format,
                m_createInfo.tiling);

            // On Gfx9 our compute-based hTile expand option can only operate on one plane (depth or stencil) at a
            // time, but it will overwrite hTile data for both planes once it's done.  :-(  So we can only
            // use the compute path for images with a single plane.
            // On Gfx10+ hTile data should stay in sync the whole time so we can allow depth+stencil exapnds.
            if (((IsGfx9(m_device) && (supportsDepth ^ supportsStencil)) || IsGfx10Plus(m_device)) &&
                TestAnyFlagSet(UseComputeExpand, (isMsaa ? UseComputeExpandMsaaDepth : UseComputeExpandDepth)))
            {
                compressedLayouts.engines |= LayoutComputeEngine;
            }

            if (IsGfx10Plus(m_device) && (supportsDepth ^ supportsStencil) && (isMsaa == false))
            {
                // GFX10+ supports compressed writes to HTILE, so it should be safe to add ShaderWrite to the
                // compressed usages (LayoutCopyDst was already added as above).
                compressedLayouts.usages |= LayoutShaderWrite;
            }
        }

        // If this trips, the SDMA engine is in danger of seeing compressed image data, which it can't understand.
        PAL_ASSERT((GetGfx9Settings(m_device).waSdmaPreventCompressedSurfUse   == false) ||
                   (TestAnyFlagSet(compressedLayouts.engines, LayoutDmaEngine) == false));

        // Supported depth layouts per compression state
        const uint32 depth   = 0;
        const uint32 stencil = GetStencilPlane();

        if (m_pHtile->DepthCompressed())
        {
            m_layoutToState.depthStencil[depth].compressed  = compressedLayouts;
            m_defaultGfxLayout.depthStencil[depth]          = compressedLayouts;
        }
        else
        {
            m_layoutToState.depthStencil[depth].compressed.usages   = 0;
            m_layoutToState.depthStencil[depth].compressed.engines  = 0;
            m_defaultGfxLayout.depthStencil[depth].usages           = LayoutAllUsages & (~LayoutUninitializedTarget);
            m_defaultGfxLayout.depthStencil[depth].engines          = LayoutAllEngines;
        }

        m_layoutToState.depthStencil[depth].decomprWithHiZ = decomprWithHiZLayout;

        if (depth != stencil)
        {
            // Supported stencil layouts per compression state
            if (m_pHtile->TileStencilDisabled() == false)
            {
                m_layoutToState.depthStencil[stencil].decomprWithHiZ = decomprWithHiZLayout;
            }
            else
            {
                m_layoutToState.depthStencil[stencil].decomprWithHiZ.usages  = 0;
                m_layoutToState.depthStencil[stencil].decomprWithHiZ.engines = 0;
            }

            if ((m_pHtile->TileStencilDisabled() == false) &&
                m_pHtile->StencilCompressed())
            {
                m_layoutToState.depthStencil[stencil].compressed = compressedLayouts;
                m_defaultGfxLayout.depthStencil[stencil]         = compressedLayouts;
            }
            else
            {
                m_layoutToState.depthStencil[stencil].compressed.usages  = 0;
                m_layoutToState.depthStencil[stencil].compressed.engines = 0;
                m_defaultGfxLayout.depthStencil[stencil].usages  = LayoutAllUsages & (~LayoutUninitializedTarget);
                m_defaultGfxLayout.depthStencil[stencil].engines = LayoutAllEngines;
            }
        }
    }
    else
    {
        // If compression is not supported, there is only one layout.
        m_defaultGfxLayout.color.usages  = LayoutAllUsages;
        m_defaultGfxLayout.color.engines = LayoutAllEngines;

        const uint32 depth   = 0;
        const uint32 stencil = GetStencilPlane();
        m_defaultGfxLayout.depthStencil[depth].usages    = LayoutAllUsages & (~LayoutUninitializedTarget);
        m_defaultGfxLayout.depthStencil[depth].engines   = LayoutAllEngines;
        m_defaultGfxLayout.depthStencil[stencil].usages  = LayoutAllUsages & (~LayoutUninitializedTarget);
        m_defaultGfxLayout.depthStencil[stencil].engines = LayoutAllEngines;
    }
}

// =====================================================================================================================
// Gets the offset for the specified mask-ram, relative to the image's bound GPU memory base address
gpusize Image::GetMaskRamBaseOffset(
    const MaskRam* pMaskRam,
    uint32         arraySlice
    ) const
{
    gpusize maskRamMemOffset = pMaskRam->MemoryOffset();

    if (arraySlice != 0)
    {
        const auto* pGfx9MaskRam = static_cast<const Gfx9MaskRam*>(pMaskRam);

        maskRamMemOffset += pGfx9MaskRam->SliceOffset(arraySlice);
    }

    // Verify that the mask ram isn't thought to be in the same place as the image itself.  That would be "bad".
    // Exception:  if the image is being created to be "mask ram only", then the offset could very well
    //             be zero as there is no image data.
    if (Parent()->GetInternalCreateInfo().flags.vrsOnlyDepth == 0)
    {
        PAL_ASSERT(maskRamMemOffset != 0);
    }

    return maskRamMemOffset + m_pParent->GetBoundGpuMemory().Offset();
}

// =====================================================================================================================
// Gets the raw base address for the specified mask-ram.
gpusize Image::GetMaskRamBaseAddr(
    const MaskRam* pMaskRam,
    uint32         arraySlice
    ) const
{
    const gpusize memAddr  = m_pParent->GetBoundGpuMemory().Memory()->Desc().gpuVirtAddr;
    const gpusize baseAddr = memAddr + GetMaskRamBaseOffset(pMaskRam, arraySlice);

    return baseAddr;
}

// =====================================================================================================================
// Calculate the tile swizzle (pipe/bank XOR value).
Result Image::ComputePipeBankXor(
    uint32                                          plane,
    bool                                            forFmask,
    const ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT*  pSurfSetting,
    uint32*                                         pPipeBankXor
    ) const
{
    Result  result = Result::Success;

    const PalSettings& coreSettings = m_device.Settings();
    const bool isDepthStencil       = Parent()->IsDepthStencilTarget();
    const bool isColorPlane         = Parent()->IsColorPlane(plane);

    PAL_ASSERT((forFmask && (plane == 0)) || (forFmask == false));
    PAL_ASSERT((isDepthStencil && (plane < 2)) || (isDepthStencil == false));

    // Also need to make sure that mip0 is not in miptail. In this case, tile swizzle cannot be supported. With current
    // design, when mip0 is in the miptail, swizzleOffset would be negative. This is a problem because the offset in MS
    // interface is a UINT.
    //
    // However, fMask is an independent surface from the parent image; it has its own swizzle mode and everything.
    // fMask only applies to MSAA surfaces and MSAA surfaces don't support mip levels.
    const BOOL_32  mipChainInTail = (forFmask ? FALSE : m_addrSurfOutput[plane].mipChainInTail);

    // A pipe/bank xor setting of zero is always valid.
    *pPipeBankXor = 0;

    // Tile swizzle only works with some of the tiling modes...  make sure the tile mode is compatible.  Note that
    // while the pSurfSetting structure has a "canXor" output, that simply means that the returned swizzle mode
    // has an "_X" equivalent, not that the supplied swizzle mode is an "_X" mode.  We need to check that ourselves.
    if (AddrMgr2::IsXorSwizzle(pSurfSetting->swizzleMode) && (mipChainInTail == FALSE))
    {
        if (m_pImageInfo->internalCreateInfo.flags.useSharedTilingOverrides)
        {
            if (forFmask)
            {
                // If this is a shared image, then the pipe/bank xor value has been given to us. Just take that.
                *pPipeBankXor = m_pImageInfo->internalCreateInfo.gfx9.sharedPipeBankXorFmask;
            }
            else if (isColorPlane || isDepthStencil)
            {
                // If this is a shared image, then the pipe/bank xor value has been given to us. Just take that.
                *pPipeBankXor = m_pImageInfo->internalCreateInfo.gfx9.sharedPipeBankXor[plane];
            }
            else if (Formats::IsYuv(m_createInfo.swizzledFormat.format))
            {
                // If this is a shared Yuv image, then the pipe/bank xor value has been given to us. Just take that.
                *pPipeBankXor = m_pImageInfo->internalCreateInfo.gfx9.sharedPipeBankXor[plane];
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
            const SubresId  subResId = { plane, 0, 0 };

            *pPipeBankXor = AddrMgr2::GetTileInfo(Parent()->OriginalImage(), subResId)->pipeBankXor;
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
            }
            else
            {
                // Otherwise for other cases, tileSwizzle specified by clients can only be 0.
                PAL_ASSERT(m_createInfo.tileSwizzle == 0);
            }
        }
        else
        {
            const Gfx9PalSettings& settings = GetGfx9Settings(m_device);

            // Presentable/flippable images cannot use tile swizzle because the display engine doesn't support it.
            const bool supportSwizzle = ((Parent()->IsPresentable()          == false) &&
                                         (Parent()->IsFlippable()            == false) &&
                                         (Parent()->IsPrivateScreenPresent() == false));

            // Ok, this surface can conceivably use swizzling...  make sure the settings allow swizzling for
            // this surface type as well.
            if (supportSwizzle &&
                // Check to see if non-zero fMask pipe-bank-xor values are allowed.
                ((forFmask == false) || settings.fmaskAllowPipeBankXor) &&
                ((TestAnyFlagSet(coreSettings.tileSwizzleMode, TileSwizzleColor) && Parent()->IsRenderTarget())       ||
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
                    uint64 hash = 0;
                    Hasher64 hasher;
                    hasher.Hash(m_createInfo);
                    hasher.Finalize(&hash);

                    surfaceIndex = MetroHash::Compact32(hash);
                }
                else if (forFmask)
                {
                    // Fmask check has to be first because everything else is checking the properties of the image
                    // which owns the Fmask buffer...  those properties will still be true.
                    surfaceIndex = fMaskSwizzleIdx.fetch_add(1, std::memory_order_relaxed);
                }
                else if (Parent()->IsRenderTarget())
                {
                    surfaceIndex = cbSwizzleIdx.fetch_add(1, std::memory_order_relaxed);
                }
                else
                {
                    surfaceIndex = txSwizzleIdx.fetch_add(1, std::memory_order_relaxed);
                }

                const auto*      pBaseSubResInfo = Parent()->SubresourceInfo(0);
                const auto*const pAddrMgr        = static_cast<const AddrMgr2::AddrMgr2*>(m_device.GetAddrMgr());

                ADDR2_COMPUTE_PIPEBANKXOR_INPUT pipeBankXorInput = { };
                pipeBankXorInput.size         = sizeof(pipeBankXorInput);
                pipeBankXorInput.surfIndex    = surfaceIndex;
                pipeBankXorInput.flags        = pAddrMgr->DetermineSurfaceFlags(*Parent(), plane, forFmask);
                pipeBankXorInput.swizzleMode  = pSurfSetting->swizzleMode;
                pipeBankXorInput.resourceType = pSurfSetting->resourceType;
                pipeBankXorInput.format       = Pal::Image::GetAddrFormat(pBaseSubResInfo->format.format);
                pipeBankXorInput.numSamples   = m_createInfo.samples;
                pipeBankXorInput.numFrags     = m_createInfo.fragments;

                ADDR2_COMPUTE_PIPEBANKXOR_OUTPUT pipeBankXorOutput = { };
                pipeBankXorOutput.size = sizeof(pipeBankXorOutput);

                ADDR_E_RETURNCODE addrRetCode = Addr2ComputePipeBankXor(m_device.AddrLibHandle(),
                                                                        &pipeBankXorInput,
                                                                        &pipeBankXorOutput);
                if (addrRetCode == ADDR_OK)
                {
                    *pPipeBankXor = pipeBankXorOutput.pipeBankXor;

#if PAL_BUILD_GFX11
                    if (IsGfx11(m_device))
                    {
                        // GFX9 and GFX10 have the pipe-bank XOR value included in the address starting at bit 8;
                        // GFX11 changes this so it starts at bit 10.  To avoid updating the address calculation in
                        // a variety of places, simply shift the XOR value by two here so that it's in the right place
                        // to be shifted eight more times.
                        *pPipeBankXor <<= 2;
                    }
#endif
                }
                else
                {
                    result = Result::ErrorUnknown;
                }
            } // End check for a suitable surface
        } // End check for a suitable swizzle mode
    }

    return result;
}

// =====================================================================================================================
// Returns the layout-to-state mask for a depth/stencil Image.  This should only ever be called on a depth/stencil
// Image.
const DepthStencilLayoutToState& Image::LayoutToDepthCompressionState(
    const SubresId& subresId
    ) const
{
    return m_layoutToState.depthStencil[subresId.plane];
}

// =====================================================================================================================
Image* GetGfx9Image(
    const IImage* pImage)
{
    return static_cast<Pal::Gfx9::Image*>(static_cast<const Pal::Image*>(pImage)->GetGfxImage());
}

// =====================================================================================================================
const Image& GetGfx9Image(
    const IImage& image)
{
    return static_cast<Pal::Gfx9::Image&>(*static_cast<const Pal::Image&>(image).GetGfxImage());
}

// =====================================================================================================================
// Helper function to determine if the layout supports comp-to-reg
bool Image::SupportsCompToReg(
    ImageLayout     layout,
    const SubresId& subResId
    ) const
{
    // Comp-to-reg fast clears are only ever supported in the compressed layout.
    ImageLayout compToRegLayout = m_layoutToState.color.compressed;

    // Fast clear eliminates are only supported on the universal engine, so that is the only engine that
    // can support comp-to-reg.
    compToRegLayout.engines = LayoutUniversalEngine;

    // The texture block does not understand comp-to-reg.
    // LayoutResolveSrc is treated as supporting compToReg under the assumption we will perform a fixed-function
    // resolve. If we end up executing a shader resolve, RPM will transition to one of the ShaderRead states explicitly.
    compToRegLayout.usages &= ~(LayoutShaderRead           |
                                LayoutShaderFmaskBasedRead |
                                LayoutShaderWrite          |
                                LayoutSampleRate           |
                                LayoutCopySrc              |
                                LayoutCopyDst);

    // Comp-to-reg is only supported if the image setup the FCE metadata and the specified
    // layout can support compToReg.
    return (GetFastClearEliminateMetaDataAddr(subResId) != 0) &&
           (TestAnyFlagSet(layout.engines, ~compToRegLayout.engines) == false) &&
           (TestAnyFlagSet(layout.usages, ~compToRegLayout.usages) == false);
}

// =====================================================================================================================
bool Image::IsFastColorClearSupported(
    GfxCmdBuffer*      pCmdBuffer,
    ImageLayout        colorLayout,
    const uint32*      pColor,
    const SubresRange& range)
{
    PAL_ASSERT(range.numPlanes == 1);

    const SubresId&  subResource = range.startSubres;

    const bool        allSlices = (subResource.arraySlice == 0) && (range.numSlices == m_createInfo.arraySize);
    const GfxIpLevel  gfxLevel  = m_device.ChipProperties().gfxLevel;

    bool isFastClearSupported =
        (ImageLayoutToColorCompressionState(m_layoutToState.color, colorLayout) == ColorCompressed) &&
        // Allow fast clears when all array slices are contained in the clear range or the part is GFX10+
        // Fast clears that do not contain all slices are not supported on GFX9 because the CB will always get the
        // fast clear color value from the fast clear reg instead of the DCC key.
        (allSlices || IsGfx10Plus(gfxLevel));

    // GFX9 only supports fast color clears using DCC memory; having cMask does nothing for fast-clears.
    if (HasDccData() && isFastClearSupported)
    {
        const auto&  settings = GetGfx9Settings(m_device);

        // Fast clears with DCC really implies using a compute shader to write a special code into DCC memory.
        //
        // Allow fast clears if we are:
        //    1) Using the compute engine to overwrite DCC memory.
        //    2) Using the graphics engine and the settings are requesting compute-based clears.
        // Compute-based clears should be faster than graphics-based "semi fast clears"
        isFastClearSupported = ((pCmdBuffer->GetEngineType() == EngineTypeCompute) ||
                                TestAnyFlagSet(settings.dccOnComputeEnable, Gfx9DccOnComputeFastClear));

        if (isFastClearSupported)
        {
            // A count of 1 indicates that no command buffer has skipped a fast clear eliminate and hence holds a
            // reference to this image's ref counter. 0 indicates that the optimzation is not enabled for this image.
            const bool noSkippedFastClearElim   = (GetFceRefCount() <= 1);
            const bool isClearColorTcCompatible = IsFastClearColorMetaFetchable(pColor);

            SetNonTcCompatClearFlag(isClearColorTcCompatible == false);

            // Figure out if we can do a a non-TC compatible DCC fast clear.  This kind of fast clear works on any
            // clear color, but requires a fast clear eliminate blt.
            const bool nonTcCompatibleFastClearPossible =
                // If the layout supports comp-to-reg then we can do a non-TC compatible DCC fast clear.
                SupportsCompToReg(colorLayout, subResource) &&
                // Allow non-TC compatible clears only if there are no skipped fast clear eliminates.
                noSkippedFastClearElim &&
                // We can only non-TC compat fast clear all arrays at once.
                // The reason is that there is only a single fast clear color value register per render target.
                // In order to support render target array index, the slices cannot have different fast clear values.
                allSlices;

            // Figure out if we can do a TC-compatible DCC fast clear (one that requires no fast clear eliminate blt)
            const bool tcCompatibleFastClearPossible =
                // Short-circuit the rest of the checks: if we can already agree to do a full fast clear, we don't need
                // to care about evaluating a TC-compatible fast clear
                (nonTcCompatibleFastClearPossible == false)                                  &&
                // The image must support TC-compatible reads from DCC-compressed surfaces
                (Parent()->SubresourceInfo(subResource)->flags.supportMetaDataTexFetch != 0) &&
                // The clear value must be TC-compatible
                isClearColorTcCompatible;

            // For non-tmz image, allow fast clear if either is possible.
            // As CP doesn't support to LOAD_***REG/SET_PREDICATION/WRITE_DATA on TMZ images,
            // Only support fast clears on TMZ images if they are TC-compatible and the clear value is TC-compatible.
            isFastClearSupported = m_pParent->IsTmz()
                                    ? tcCompatibleFastClearPossible
                                    : (nonTcCompatibleFastClearPossible || tcCompatibleFastClearPossible);
        }
    }

    return isFastClearSupported;
}

// =====================================================================================================================
// Returns true if the pColor[cmpIdx] is equivalent to 0.0f or 1.0f
bool Image::IsColorDataZeroOrOne(
    const uint32*  pColor,
    uint32         cmpIdx
    ) const
{
    bool  isZeroOrOne = false;

    // Determining if the X9Y9Z9E5 format is one of the special colors involves looking at both the exponent
    // and the fractional portion of the clear color, as both zero and one are represented by a fractional portion
    // of zero.
    if (Parent()->GetImageCreateInfo().swizzledFormat.format != ChNumFormat::X9Y9Z9E5_Float)
    {
        const uint32 one = TranslateClearCodeOneToNativeFmt(cmpIdx);

        isZeroOrOne = ((pColor[cmpIdx] == 0) || (pColor[cmpIdx] == one));
    }

    return isZeroOrOne;
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
    bool isMetaFetchable = true;

    // If we're doing comp-to-single fast clears on this image, then there's no need to check for the clear color
    // being one of the four magic codes.  If it is, great, if not, the image will still be meta-fetchable without
    // requiring a fast-clear-eliminate step.
    if (m_useCompToSingleForFastClears == false)
    {
        // Ok, not using comp-to-single, so we need to check for one of the four magic clear colors here.
        const ChNumFormat     format        = m_createInfo.swizzledFormat.format;
        const ChannelSwizzle* pSwizzle      = &m_createInfo.swizzledFormat.swizzle.swizzle[0];
        const auto&           settings      = GetGfx9Settings(m_device);

        bool   rgbSeen          = false;
        uint32 requiredRgbValue = 0; // not valid unless rgbSeen==true

        for (uint32 cmpIdx = 0; ((cmpIdx < 4) && isMetaFetchable); cmpIdx++)
        {
            //  If forceRegularClearCode is set then we are not using one of the four "magic"
            //  fast-clear colors so the fast-clear can't be meta-fetchable.
            if ((IsColorDataZeroOrOne(pColor, cmpIdx) == false) || settings.forceRegularClearCode)
            {
                // This channel isn't zero or one, so the fast-clear can't be meta-fetchable.
                isMetaFetchable = false;
            }
            else if ((pColor[cmpIdx] != 0) &&
                     (m_pParent->GetDccFormatEncoding() == DccFormatEncoding::SignIndependent))
            {
                // cant allow special clear color code because the formats do not support DCC Constant
                // encoding. This happens when we mix signed and unsigned formats. There is no problem with
                // clearcolor0000.The issue is only seen when there is a 1 in any of the channels
                isMetaFetchable = false;
            }
            else
            {
                uint8 tgtCmpIdx = static_cast<uint8>(pSwizzle[cmpIdx]) - static_cast<uint8>(ChannelSwizzle::X);

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
                        requiredRgbValue = pColor[tgtCmpIdx];
                    }
                    else if (pColor[tgtCmpIdx] != requiredRgbValue)
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
    }

    return isMetaFetchable;
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
bool Image::IsFastDepthStencilClearSupported(
    ImageLayout        depthLayout,
    ImageLayout        stencilLayout,
    float              depth,
    uint8              stencil,
    uint8              stencilWriteMask,
    const SubresRange& range
    ) const
{
    PAL_ASSERT(range.numPlanes == 1);

    const SubresId& subResource = range.startSubres;
    const bool      isDepthPlane   = Parent()->IsDepthPlane(subResource.plane);
    const bool      isStencilPlane = Parent()->IsStencilPlane(subResource.plane);

    // We can only fast clear all arrays at once.
    bool isFastClearSupported = (subResource.arraySlice == 0) && (range.numSlices == m_createInfo.arraySize);

    // We cannot fast clear if it's doing masked stencil clear.
    if (isStencilPlane && (stencilWriteMask != 0xFF))
    {
        isFastClearSupported = false;
    }

    if (isFastClearSupported)
    {
        const SubResourceInfo*const pSubResInfo = m_pParent->SubresourceInfo(subResource);

        // Subresources that do not enable a fast clear method at all can not be fast cleared
        const ClearMethod clearMethod = pSubResInfo->clearMethod;

        // Choose which layout to use based on the range plane
        const ImageLayout layout = isDepthPlane ? depthLayout : stencilLayout;

        // Check if we're even allowing fast (compute) or depth-fast-graphics (gfx) based fast clears on this surface.
        // If not, there's nothing to do.
        if ((clearMethod != ClearMethod::Fast) && (clearMethod != ClearMethod::DepthFastGraphics))
        {
            isFastClearSupported = false;
        }
        else
        {
            // Map from layout to supported compression state
            const DepthStencilCompressionState state =
                ImageLayoutToDepthCompressionState(LayoutToDepthCompressionState(subResource), layout);

            // Layouts that do not support depth-stencil compression can not be fast cleared
            if (state != DepthStencilCompressed)
            {
                isFastClearSupported = false;
            }
        }

        if (pSubResInfo->flags.supportMetaDataTexFetch != 0)
        {
            if (isDepthPlane)
            {
                isFastClearSupported &= IsFastClearDepthMetaFetchable(depth);
            }
            else if (isStencilPlane)
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

        // The client is clearing stencil plane while the htile is of depth only format.
        // In this case, we should not do fast clear.
        if (isStencilPlane && HasHtileData() && GetHtile()->TileStencilDisabled())
        {
            isFastClearSupported = false;
        }
        // Only depth in the [0.0, 1.0] range can be compressed.
        else if (isDepthPlane && ((depth < 0.0f) || (depth > 1.0f)))
        {
            isFastClearSupported = false;
        }
    }

    return isFastClearSupported;
}

// =====================================================================================================================
// Determines if this image supports being cleared or copied with format replacement.
bool Image::IsFormatReplaceable(
    const SubresId& subresId,
    ImageLayout     layout,
    bool            isDst,
    uint8           disabledChannelMask
    ) const
{
    bool  isFormatReplaceable = false;

    // The image can only be cleared or copied with format replacement
    // when all channels of the color are being written.
    if (disabledChannelMask == 0)
    {
        if (Parent()->IsDepthStencilTarget())
        {
            const auto layoutToState = m_layoutToState.depthStencil[subresId.plane];

            // Htile must either be disabled or we must be sure that the texture pipe doesn't need to read it.
            // Depth surfaces are either Z-16 unorm or Z-32 float; they would get replaced to x16-uint or x32-uint.
            // Z-16 unorm is actually replaceable, but Z-32 float will be converted to unorm if replaced.
            isFormatReplaceable =
                ((HasDsMetadata() == false) ||
                 (ImageLayoutToDepthCompressionState(layoutToState, layout) != DepthStencilCompressed));
        }
        else
        {
            const bool isMmFormat = Formats::IsMmFormat(m_createInfo.swizzledFormat.format);

            // DCC must either be disabled or we must be sure that it is decompressed.
            // MM formats are also not replaceable
            // because they have different black and white values and HW will convert which changes the data.
            isFormatReplaceable =
                (((HasDccData() == false)                                                               ||
#if PAL_BUILD_GFX11
                  // GFX11 can always allow format replacement since the original format is encoded
                  // into the DCC key; gate by the panel setting.
                  (IsGfx11(m_device) && GetGfx9Settings(m_device).gfx11AlwaysAllowDccFormatReplacement) ||
#endif
                  (ImageLayoutToColorCompressionState(m_layoutToState.color, layout) == ColorDecompressed)) &&
                 (isMmFormat == false));
        }
    }

    return isFormatReplaceable;
}

// =====================================================================================================================
// Answers the question: "If I do shader writes in this layout, will it break my metadata?". For example, this
// would return true if we promised that CopyDst would be compressed but tried to use a compute copy path.
bool Image::ShaderWriteIncompatibleWithLayout(
    const SubresId& subresId,
    ImageLayout     layout
    ) const
{
    bool writeIncompatible = false;

    const ImageLayout writeLayout = {LayoutShaderWrite, layout.engines};

    if (HasDsMetadata())
    {
        const DepthStencilLayoutToState& layoutToState = LayoutToDepthCompressionState(subresId);

        // If the given layout demands a higher compression state than shader writes can produce it's incompatible.
        writeIncompatible = (ImageLayoutToDepthCompressionState(layoutToState, layout) >
                             ImageLayoutToDepthCompressionState(layoutToState, writeLayout));
    }
    else if (HasColorMetaData())
    {
        // If the given layout demands a higher compression state than shader writes can produce it's incompatible.
        writeIncompatible = (ImageLayoutToColorCompressionState(m_layoutToState.color, layout) >
                             ImageLayoutToColorCompressionState(m_layoutToState.color, writeLayout));
    }

    return writeIncompatible;
}

// =====================================================================================================================
bool Image::IsSubResourceLinear(
    const SubresId& subresource
    ) const
{
    const AddrSwizzleMode swizzleMode = m_addrSurfSetting[subresource.plane].swizzleMode;

    return (swizzleMode == ADDR_SW_LINEAR);
}

// =====================================================================================================================
bool Image::IsIterate256Meaningful(
    const SubResourceInfo* subResInfo
    ) const
{
    const auto& imageCreateInfo = Parent()->GetImageCreateInfo();
    const Pal::Device& device   = *Parent()->GetDevice();

    //  Note that this recommendation really only applies to MSAA depth or stencil surfaces that are
    //  tc-compatible.  The iterate_256 state has no effect on 1xaa surfaces.
    return ((imageCreateInfo.samples > 1)             &&
            imageCreateInfo.usageFlags.depthStencil   &&
            subResInfo->flags.supportMetaDataTexFetch &&
            ((IsNavi2x(device) && (imageCreateInfo.samples >= 4)) == false) &&
            (subResInfo->subresId.plane < m_pImageInfo->numPlanes));
}

// =====================================================================================================================
HtileUsageFlags Image::GetHtileUsage() const
{
    HtileUsageFlags  hTileUsage = {};

    if (m_pHtile != nullptr)
    {
        hTileUsage = m_pHtile->GetHtileUsage();
    }

    return hTileUsage;
}

// =====================================================================================================================
// Returns a pointer to all of the address library's surface-output calculations that pertain to the specified
// subresource.
const ADDR2_COMPUTE_SURFACE_INFO_OUTPUT* Image::GetAddrOutput(
    const SubResourceInfo*  pSubResInfo
    ) const
{
    return &m_addrSurfOutput[pSubResInfo->subresId.plane];
}

// =====================================================================================================================
uint32 Image::GetTileSwizzle(
    const SubresId& subresId
    ) const
{
    return AddrMgr2::GetTileInfo(m_pParent, subresId)->pipeBankXor;
}

// =====================================================================================================================
gpusize Image::GetSubresourceAddr(
    SubresId  subResId
    ) const
{
    return GetPlaneBaseAddr(subResId.plane);
}

// =====================================================================================================================
bool Image::HasDccStateMetaData(
    const SubresRange& range
    ) const
{
    bool result = false;
    for (uint32 plane = range.startSubres.plane; (plane < (range.startSubres.plane + range.numPlanes)); plane++)
    {
        result |= HasDccStateMetaData(plane);
    }
    return result;
}

// =====================================================================================================================
bool Image::HasFastClearEliminateMetaData(
    const SubresRange& range
    ) const
{
    bool result = false;
    for (uint32 plane = range.startSubres.plane; (plane < (range.startSubres.plane + range.numPlanes)); plane++)
    {
        result |= HasFastClearEliminateMetaData(plane);
    }
    return result;
}

// =====================================================================================================================
// Determines the GPU virtual address of the DCC state meta-data. Returns the GPU address of the meta-data, zero if this
// image doesn't have the DCC state meta-data.
gpusize Image::GetDccStateMetaDataAddr(
    const SubresId&  subResId
    ) const
{
    PAL_ASSERT(subResId.mipLevel < m_createInfo.mipLevels);

    // All the metadata for slices of a single mipmap level are contiguous region in memory.
    // So we can use one WRITE_DATA packet to update multiple array slices' metadata.
    const uint32   metaDataIndex  = m_createInfo.arraySize * subResId.mipLevel + subResId.arraySlice;
    const gpusize  metaDataOffset = m_dccStateMetaDataOffset[subResId.plane];

    return (metaDataOffset == 0)
            ? 0
            : m_pParent->GetBoundGpuMemory().GpuVirtAddr() + metaDataOffset +
              (metaDataIndex * sizeof(MipDccStateMetaData));
}

// =====================================================================================================================
// Determines the offset of the DCC state meta-data. Returns the offset of the meta-data, zero if this
// image doesn't have the DCC state meta-data.
gpusize Image::GetDccStateMetaDataOffset(
    const SubresId&  subResId
    ) const
{
    PAL_ASSERT(subResId.mipLevel < m_createInfo.mipLevels);

    // All the metadata for slices of a single mipmap level are contiguous region in memory.
    // So we can use one WRITE_DATA packet to update multiple array slices' metadata.
    const uint32   metaDataIndex  = m_createInfo.arraySize * subResId.mipLevel + subResId.arraySlice;
    const gpusize  metaDataOffset = m_dccStateMetaDataOffset[subResId.plane];

    return (metaDataOffset == 0)
            ? 0
            : metaDataOffset + (metaDataIndex * sizeof(MipDccStateMetaData));
}

// =====================================================================================================================
// Returns the GPU memory size of the dcc state metadata for the specified num mips.
gpusize Image::GetDccStateMetaDataSize(
    const SubresId&  subResId,
    uint32           numMips
    ) const
{
    PAL_ASSERT(HasDccStateMetaData(subResId.plane));

    return (sizeof(MipDccStateMetaData) * numMips);
}

// =====================================================================================================================
// Initializes the GPU offset for this Image's DCC state metadata. It must include an array of Gfx9DccMipMetaData with
// one item for each mip level.
void Image::InitDccStateMetaData(
    uint32             planeIdx,
    ImageMemoryLayout* pGpuMemLayout,
    gpusize*           pGpuMemSize)
{
    m_dccStateMetaDataOffset[planeIdx] = Pow2Align(*pGpuMemSize, PredicationAlign);
    m_dccStateMetaDataSize[planeIdx]   = m_createInfo.mipLevels * m_createInfo.arraySize * sizeof(MipDccStateMetaData);
    *pGpuMemSize                       = (m_dccStateMetaDataOffset[planeIdx] + m_dccStateMetaDataSize[planeIdx]);

    // Update the layout information against the DCC state metadata.
    UpdateMetaDataHeaderLayout(pGpuMemLayout, m_dccStateMetaDataOffset[planeIdx], PredicationAlign);
}

// =====================================================================================================================
// Initializes the GPU offset for this Image's waTcCompatZRange metadata.
void Image::InitWaTcCompatZRangeMetaData(
    ImageMemoryLayout* pGpuMemLayout,
    gpusize*           pGpuMemSize)
{
    m_waTcCompatZRangeMetaDataOffset       = Pow2Align(*pGpuMemSize, sizeof(uint32));
    m_waTcCompatZRangeMetaDataSizePerMip   = sizeof(uint32);
    *pGpuMemSize                           = (m_waTcCompatZRangeMetaDataOffset +
                                              (m_waTcCompatZRangeMetaDataSizePerMip * m_createInfo.mipLevels));

    // Update the layout information against the waTcCompatZRange metadata.
    UpdateMetaDataHeaderLayout(pGpuMemLayout, m_waTcCompatZRangeMetaDataOffset, sizeof(uint32));
}

// =====================================================================================================================
// Initializes the GPU offset for this Image's fast-clear-eliminate metadata. FCE metadata is one DWORD for each mip
// level of the image; if the corresponding DWORD for a miplevel is zero, then a fast-clear-eliminate operation will not
// be required.
void Image::InitFastClearEliminateMetaData(
    uint32             plane,
    ImageMemoryLayout* pGpuMemLayout,
    gpusize*           pGpuMemSize)
{
    m_fastClearEliminateMetaDataOffset[plane] = Pow2Align(*pGpuMemSize, PredicationAlign);
    m_fastClearEliminateMetaDataSize[plane]   = (m_createInfo.mipLevels * sizeof(MipFceStateMetaData));
    *pGpuMemSize                                 = m_fastClearEliminateMetaDataOffset[plane] +
                                                   m_fastClearEliminateMetaDataSize[plane];

    // Update the layout information against the fast-clear eliminate metadata.
    UpdateMetaDataHeaderLayout(pGpuMemLayout, m_fastClearEliminateMetaDataOffset[plane], PredicationAlign);

    // Initialize data structure for fast clear eliminate optimization. The GPU predicates fast clear eliminates
    // when the clear color is TC compatible. So here, we try to not perform fast clear eliminate and save the
    // CPU cycles required to set up the fast clear eliminate.
    m_pNumSkippedFceCounter =  m_device.GetGfxDevice()->AllocateFceRefCount();
}

// =====================================================================================================================
// Initializes the GPU offset of lookup table for Image's htile metadata. The htile lookup table is 4-byte-aligned,
// in which htile meta offset is stored for each pixel(coordinate/mip/arraySlice). All mip levels included in the table.
void Image::InitHtileLookupTable(
    ImageMemoryLayout* pGpuMemLayout,
    gpusize*           pGpuOffset,
    gpusize*           pGpuMemAlignment)
{
    // Metadata offset will be used as uint in shader
    static constexpr gpusize HtileLookupTableAlignment = 4u;

    *pGpuMemAlignment = Max(*pGpuMemAlignment, HtileLookupTableAlignment);

    gpusize mipLevelOffset = Util::Pow2Align((*pGpuOffset), HtileLookupTableAlignment);

    uint32 mipLevels      = m_createInfo.mipLevels;

    SubresId subresId = {};

    while (mipLevels > 0)
    {
        const uint32 curMipLevel = m_createInfo.mipLevels - mipLevels;

        subresId.mipLevel = curMipLevel;
        uint32 mipLevelWidth = Parent()->SubresourceInfo(subresId)->extentTexels.width;
        uint32 mipLevelHeight = Parent()->SubresourceInfo(subresId)->extentTexels.height;

        const uint32 hTileWidth  = Util::Pow2Align(mipLevelWidth, 8u) / 8u;
        const uint32 hTileHeight = Util::Pow2Align(mipLevelHeight, 8u) / 8u;

        m_metaDataLookupTableOffsets[curMipLevel] = mipLevelOffset;
        m_metaDataLookupTableSizes[curMipLevel] = (hTileWidth * hTileHeight) * m_createInfo.arraySize * 4u;

        mipLevelOffset += m_metaDataLookupTableSizes[curMipLevel];

        --mipLevels;
    }

    *pGpuOffset = mipLevelOffset;
}

// =====================================================================================================================
void Image::InitDccLookupTable(
    ImageMemoryLayout* pGpuMemLayout,
    gpusize*           pGpuOffset,
    gpusize*           pGpuMemAlignment)
{
    const auto*  pBaseDcc = GetDcc(0);

    if (pBaseDcc->GetNumEffectiveSamples(DccClearPurpose::FastClear) <= 2)
    {
        // The current implementation of Dcc lookup table works for hardwares that compress maximum two samples.
        // Element of dcc lookup table is uint32.
        static constexpr uint32  ElementSize             = sizeof(uint32);
        static constexpr gpusize DccLookupTableAlignment = ElementSize;

        *pGpuMemAlignment = Max(*pGpuMemAlignment, DccLookupTableAlignment);

        const auto&  dccAddrOutput     = pBaseDcc->GetAddrOutput();
        const uint32 lookupTableWidth  = dccAddrOutput.metaBlkWidth  / dccAddrOutput.compressBlkWidth;
        const uint32 lookupTableHeight = dccAddrOutput.metaBlkHeight / dccAddrOutput.compressBlkHeight;

        m_dccLookupTableOffset = Util::Pow2Align((*pGpuOffset), DccLookupTableAlignment);
        m_dccLookupTableSize   = lookupTableWidth * lookupTableHeight * ElementSize * m_createInfo.arraySize;

        UpdateMetaDataHeaderLayout(pGpuMemLayout, m_dccLookupTableOffset, DccLookupTableAlignment);

        *pGpuOffset += m_dccLookupTableSize;
    }
}

// =====================================================================================================================
void Image::BuildDccLookupTableBufferView(
    BufferViewInfo* pViewInfo) const
{
    pViewInfo->gpuAddr        = Parent()->GetGpuVirtualAddr() + m_dccLookupTableOffset;
    pViewInfo->range          = m_dccLookupTableSize;
    pViewInfo->stride         = 1;
    pViewInfo->swizzledFormat = UndefinedSwizzledFormat;

    const auto* pPublicSettings = m_device.GetPublicSettings();

    pViewInfo->flags.bypassMallRead =
        TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnRead);
    pViewInfo->flags.bypassMallWrite =
        TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnWrite);
}

// =====================================================================================================================
// Builds PM4 commands into the command buffer which will update this image's fast-clear metadata to reflect the most
// recent clear color. Returns the next unused DWORD in pCmdSpace.
uint32* Image::UpdateColorClearMetaData(
    const SubresRange&  clearRange,
    const uint32        packedColor[4],
    Pm4Predicate        predicate,
    uint32*             pCmdSpace
    ) const
{
    // Verify that we have DCC data that's required for handling fast-clears on gfx9
    PAL_ASSERT(HasDccData());

    // Number of DWORD registers which represent the fast-clear color for a bound color target:
    constexpr size_t MetaDataDwords = sizeof(Gfx9FastColorClearMetaData) / sizeof(uint32);

    // Issue a WRITE_DATA command to update the fast-clear metadata.
    WriteDataInfo writeData = {};
    writeData.engineType = EngineTypeUniversal;
    writeData.engineSel  = engine_sel__pfp_write_data__prefetch_parser;
    writeData.dstSel     = dst_sel__pfp_write_data__memory;
    writeData.predicate  = predicate;

    uint32* pSpace = pCmdSpace;
    for (SubresId subresId = clearRange.startSubres;
         subresId.plane < (clearRange.startSubres.plane + clearRange.numPlanes);
         subresId.plane++)
    {
        writeData.dstAddr = FastClearMetaDataAddr(subresId);

        if (writeData.dstAddr != 0)
        {
            pSpace += CmdUtil::BuildWriteDataPeriodic(writeData,
                                                      MetaDataDwords,
                                                      clearRange.numMips,
                                                      packedColor,
                                                      pSpace);
        }
    }

    return pSpace;
}

// =====================================================================================================================
// Builds PM4 commands into the command buffer which will update this Image's DCC state metadata over the given mip
// range to reflect the given compression state. Returns the next unused DWORD in pCmdSpace.
void Image::UpdateDccStateMetaData(
    Pal::CmdStream*      pCmdStream,
    const SubresRange&   range,
    bool                 isCompressed,
    EngineType           engineType,
    Pm4Predicate         predicate
    ) const
{
    if (HasDccStateMetaData(range))
    {
        PAL_ASSERT(HasDccData());

        MipDccStateMetaData metaData = { };
        metaData.isCompressed = (isCompressed ? 1 : 0);

        constexpr uint32 DwordsPerSlice = sizeof(metaData) / sizeof(uint32);

        // We need to limit the length for the commands generated by BuildWriteDataPeriodic to fit the reserved
        // limitation.
        const uint32 maxSlicesPerPacket = (pCmdStream->ReserveLimit() - CmdUtil::WriteDataSizeDwords) / DwordsPerSlice;

        const uint32 mipBegin   = range.startSubres.mipLevel;
        const uint32 mipEnd     = range.startSubres.mipLevel + range.numMips;
        const uint32 sliceBegin = range.startSubres.arraySlice;
        const uint32 sliceEnd   = range.startSubres.arraySlice + range.numSlices;

        WriteDataInfo writeData = {};
        writeData.engineType = engineType;
        writeData.engineSel  = engine_sel__pfp_write_data__prefetch_parser;
        writeData.dstSel     = dst_sel__pfp_write_data__memory;
        writeData.predicate  = predicate;

        SubresId subresId = {};
        for (uint32 planeIdx = range.startSubres.plane;
            planeIdx < (range.startSubres.plane + range.numPlanes);
            planeIdx++)
        {
            subresId.plane = planeIdx;

            for (uint32 mipLevelIdx = mipBegin; mipLevelIdx < mipEnd; mipLevelIdx++)
            {
                subresId.mipLevel = mipLevelIdx;

                for (uint32 sliceIdx = sliceBegin; sliceIdx < sliceEnd; sliceIdx += maxSlicesPerPacket)
                {
                    subresId.arraySlice = sliceIdx;

                    uint32 periodsToWrite = (sliceIdx + maxSlicesPerPacket <= sliceEnd) ? maxSlicesPerPacket :
                                                                                          sliceEnd - sliceIdx;

                    writeData.dstAddr = GetDccStateMetaDataAddr(subresId);
                    PAL_ASSERT(writeData.dstAddr != 0);

                    uint32* pCmdSpace = pCmdStream->ReserveCommands();
                    pCmdSpace += CmdUtil::BuildWriteDataPeriodic(writeData,
                                                                 DwordsPerSlice,
                                                                 periodsToWrite,
                                                                 reinterpret_cast<uint32*>(&metaData),
                                                                 pCmdSpace);
                    pCmdStream->CommitCommands(pCmdSpace);
                }
            }
        }
    }
}

// =====================================================================================================================
// Builds PM4 commands into the command buffer which will update this Image's fast-clear-eliminate metadata over the
// given mip range to reflect the given value. Returns the next unused DWORD in pCmdSpace.
uint32* Image::UpdateFastClearEliminateMetaData(
    const GfxCmdBuffer*  pCmdBuffer,
    const SubresRange&   range,
    uint32               value,
    Pm4Predicate         predicate,
    uint32*              pCmdSpace
    ) const
{
    PAL_ASSERT(range.numPlanes == 1);
    // We need to write one DWORD per mip in the range. We can do this most efficiently with a single WRITE_DATA.
    PAL_ASSERT(range.numMips <= MaxImageMipLevels);

    MipFceStateMetaData metaData = { };
    metaData.fceRequired = value;

    WriteDataInfo writeData = {};
    writeData.engineType = pCmdBuffer->GetEngineType();
    writeData.dstAddr    = GetFastClearEliminateMetaDataAddr(range.startSubres);
    writeData.engineSel  = engine_sel__pfp_write_data__prefetch_parser;
    writeData.dstSel     = dst_sel__pfp_write_data__memory;
    writeData.predicate  = predicate;

    PAL_ASSERT(writeData.dstAddr != 0);

    pCmdSpace += CmdUtil::BuildWriteDataPeriodic(writeData,
                                                 (sizeof(metaData) / sizeof(uint32)),
                                                 range.numMips,
                                                 reinterpret_cast<uint32*>(&metaData),
                                                 pCmdSpace);
    return pCmdSpace;
}

// =====================================================================================================================
// Builds PM4 commands into the command buffer which will update this Image's waTcCompatZRange metadata to reflect the
// most recent depth fast clear value. Returns the next unused DWORD in pCmdSpace.
uint32* Image::UpdateWaTcCompatZRangeMetaData(
    const SubresRange& range,
    float              depthValue,
    Pm4Predicate       predicate,
    uint32*            pCmdSpace
    ) const
{
    // If the last fast clear value was 0.0f, the DB_Z_INFO.ZRANGE_PRECISION register field should be written to 0
    // when a depth target is bound. The metadata is used as a COND_EXEC condition, so it needs to be set to true
    // when the clear value is 0.0f, and false otherwise.
    const uint32 metaData = (depthValue == 0.0f) ? UINT_MAX : 0;

    // Write a single DWORD per mip starting at the GPU address of waTcCompatZRange metadata.
    constexpr size_t dwordsToCopy = 1;

    WriteDataInfo writeData = {};
    writeData.engineType = EngineTypeUniversal;
    writeData.dstAddr    = GetWaTcCompatZRangeMetaDataAddr(range.startSubres.mipLevel);
    writeData.engineSel  = engine_sel__pfp_write_data__prefetch_parser;
    writeData.dstSel     = dst_sel__pfp_write_data__memory;
    writeData.predicate  = predicate;

    return pCmdSpace + CmdUtil::BuildWriteDataPeriodic(writeData, dwordsToCopy, range.numMips, &metaData, pCmdSpace);
}

// =====================================================================================================================
// Determines the GPU virtual address of the fast-clear-eliminate meta-data.  This metadata is used by a
// conditional-execute packet around the fast-clear-eliminate packets. Returns the GPU address of the
// fast-clear-eliminiate packet, zero if this image does not have the FCE meta-data.
gpusize Image::GetFastClearEliminateMetaDataAddr(
    const SubresId&  subResId
    ) const
{
    const uint32  planeIdx = subResId.plane;

    PAL_ASSERT(subResId.mipLevel < m_createInfo.mipLevels);

    return (m_fastClearEliminateMetaDataOffset[planeIdx] == 0)
            ? 0
            : m_pParent->GetBoundGpuMemory().GpuVirtAddr() + m_fastClearEliminateMetaDataOffset[planeIdx] +
              (subResId.mipLevel * sizeof(MipFceStateMetaData));
}

// =====================================================================================================================
// Determines the offset of the fast-clear-eliminate meta-data.  This metadata is used by a
// conditional-execute packet around the fast-clear-eliminate packets. Returns the offset of the
// fast-clear-eliminiate packet, zero if this image does not have the FCE meta-data.
gpusize Image::GetFastClearEliminateMetaDataOffset(
    const SubresId&  subResId
    ) const
{
    const uint32  planeIdx = subResId.plane;

    PAL_ASSERT(subResId.mipLevel < m_createInfo.mipLevels);

    return (m_fastClearEliminateMetaDataOffset[planeIdx] == 0)
            ? 0
            : m_fastClearEliminateMetaDataOffset[planeIdx] + (subResId.mipLevel * sizeof(MipFceStateMetaData));
}

// =====================================================================================================================
// Returns the GPU memory size of the fast-clear-eliminate metadata for the specified num mips.
gpusize Image::GetFastClearEliminateMetaDataSize(
    uint32 numMips
    ) const
{
    PAL_ASSERT(numMips <= MaxImageMipLevels);
    return (sizeof(MipFceStateMetaData) * numMips);
}

//=====================================================================================================================
// Returns the GPU address of the meta-data. This function is not called if this image doesn't have waTcCompatZRange
// meta-data.
gpusize Image::GetWaTcCompatZRangeMetaDataAddr(
    uint32 mipLevel
    ) const
{
    return (Parent()->GetBoundGpuMemory().GpuVirtAddr() + m_waTcCompatZRangeMetaDataOffset +
            (m_waTcCompatZRangeMetaDataSizePerMip * mipLevel));
}

// =====================================================================================================================
// Returns the offset relative to the bound GPU memory of the waTcCompatZRange metadata for the specified mip level.
gpusize Image::WaTcCompatZRangeMetaDataOffset(
    uint32 mipLevel
    ) const
{
    PAL_ASSERT(HasWaTcCompatZRangeMetaData());

    return Parent()->GetBoundGpuMemory().Offset() + m_waTcCompatZRangeMetaDataOffset +
           (m_waTcCompatZRangeMetaDataSizePerMip * mipLevel);
}

// =====================================================================================================================
// Returns the GPU memory size of the waTcCompatZRange metadata for the specified num mips.
gpusize Image::WaTcCompatZRangeMetaDataSize(
    uint32 numMips
    ) const
{
    PAL_ASSERT(HasWaTcCompatZRangeMetaData());

    return (m_waTcCompatZRangeMetaDataSizePerMip * numMips);
}

// =====================================================================================================================
// Builds PM4 commands into the command buffer which will update this Image's meta-data to reflect the updated
// HiSPretests values. Returns the next unused DWORD in pCmdSpace.
uint32* Image::UpdateHiSPretestsMetaData(
    const SubresRange& range,
    const HiSPretests& pretests,
    Pm4Predicate       predicate,
    uint32*            pCmdSpace
    ) const
{
    PAL_ASSERT(HasHiSPretestsMetaData());

    PAL_ASSERT((range.startSubres.arraySlice == 0) && (range.numSlices == m_createInfo.arraySize));

    Gfx9HiSPretestsMetaData pretestsMetaData = {};

    pretestsMetaData.dbSResultCompare0.bitfields.COMPAREFUNC0  =
                                                 DepthStencilState::HwStencilCompare(pretests.test[0].func);
    pretestsMetaData.dbSResultCompare0.bitfields.COMPAREMASK0  = pretests.test[0].mask;
    pretestsMetaData.dbSResultCompare0.bitfields.COMPAREVALUE0 = pretests.test[0].value;
    pretestsMetaData.dbSResultCompare0.bitfields.ENABLE0       = pretests.test[0].isValid;

    pretestsMetaData.dbSResultCompare1.bitfields.COMPAREFUNC1  =
                                                 DepthStencilState::HwStencilCompare(pretests.test[1].func);
    pretestsMetaData.dbSResultCompare1.bitfields.COMPAREMASK1  = pretests.test[1].mask;
    pretestsMetaData.dbSResultCompare1.bitfields.COMPAREVALUE1 = pretests.test[1].value;
    pretestsMetaData.dbSResultCompare1.bitfields.ENABLE1       = pretests.test[1].isValid;

    // Base GPU virtual address of the Image's HiSPretests metadata.
    const gpusize gpuVirtAddr     = HiSPretestsMetaDataAddr(range.startSubres.mipLevel);
    const uint32* pSrcData        = reinterpret_cast<uint32*>(&pretestsMetaData.dbSResultCompare0);
    constexpr size_t DwordsToCopy = sizeof(pretestsMetaData) / sizeof(uint32);

    PAL_ASSERT(gpuVirtAddr != 0);

    const CmdUtil& cmdUtil = static_cast<const Device*>(m_device.GetGfxDevice())->CmdUtil();

    WriteDataInfo writeData = {};
    writeData.engineType    = EngineTypeUniversal;
    writeData.dstAddr       = gpuVirtAddr;
    writeData.engineSel     = engine_sel__pfp_write_data__prefetch_parser;
    writeData.dstSel        = dst_sel__pfp_write_data__memory;
    writeData.predicate     = predicate;

    return pCmdSpace + cmdUtil.BuildWriteDataPeriodic(writeData,
                                                      DwordsToCopy,
                                                      range.numMips,
                                                      pSrcData,
                                                      pCmdSpace);

}

// =====================================================================================================================
// Builds PM4 commands into the command buffer which will update this Image's meta-data to reflect the updated fast
// clear values. Returns the next unused DWORD in pCmdSpace.
uint32* Image::UpdateDepthClearMetaData(
    const SubresRange& range,
    uint32             writeMask,
    float              depthValue,
    uint8              stencilValue,
    Pm4Predicate       predicate,
    uint32*            pCmdSpace
    ) const
{
    PAL_ASSERT(HasDsMetadata());

    PAL_ASSERT((range.startSubres.arraySlice == 0) && (range.numSlices == m_createInfo.arraySize));

    Gfx9FastDepthClearMetaData clearData;
    clearData.dbStencilClear.u32All     = 0;
    clearData.dbStencilClear.bits.CLEAR = stencilValue;
    clearData.dbDepthClear.f32All       = depthValue;

    // Base GPU virtual address of the Image's fast-clear metadata.
    gpusize       gpuVirtAddr  = FastClearMetaDataAddr(range.startSubres);
    const uint32* pSrcData     = nullptr;
    size_t        dwordsToCopy = 0;

    const bool writeDepth   = TestAnyFlagSet(writeMask, HtilePlaneDepth);
    const bool writeStencil = TestAnyFlagSet(writeMask, HtilePlaneStencil);

    if (writeStencil)
    {
        // Stencil-only or depth/stencil clear: start at the GPU address of the DB_STENCIL_CLEAR register value. Copy
        // one DWORD for stencil-only and two DWORDs for depth/stencil.
        gpuVirtAddr += offsetof(Gfx9FastDepthClearMetaData, dbStencilClear);
        pSrcData     = reinterpret_cast<uint32*>(&clearData.dbStencilClear);
        dwordsToCopy = (writeDepth ? 2 : 1);
    }
    else if (writeDepth)
    {
        // Depth-only clear: write a single DWORD starting at the GPU address of the DB_DEPTH_CLEAR register value.
        gpuVirtAddr += offsetof(Gfx9FastDepthClearMetaData, dbDepthClear);
        pSrcData     = reinterpret_cast<uint32*>(&clearData.dbDepthClear);
        dwordsToCopy = 1;
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    PAL_ASSERT(gpuVirtAddr != 0);

    // depth stencil meta data storage as the pair, n levels layout is following,
    //
    // S-stencil, D-depth.
    //  ___________________________________________
    // | mipmap0 | mipmap1 | mipmap2  | ... | mipmapn |
    // |________ |_________|_________|___|_________|
    // |  S   |  D  |  S   |  D   |  S   |  D   | ... |  S  |  D   |
    // |__________________________________________ |
    // depth-only write or stencil-only wirte should respective skip S/D offset.
    if (writeDepth && writeStencil)
    {
        // update depth-stencil meta data
        PAL_ASSERT(dwordsToCopy == 2);

        WriteDataInfo writeData = {};
        writeData.engineType = EngineTypeUniversal;
        writeData.dstAddr    = gpuVirtAddr;
        writeData.engineSel  = engine_sel__pfp_write_data__prefetch_parser;
        writeData.dstSel     = dst_sel__pfp_write_data__memory;
        writeData.predicate  = predicate;

        pCmdSpace += CmdUtil::BuildWriteDataPeriodic(writeData, dwordsToCopy, range.numMips, pSrcData, pCmdSpace);
    }
    else
    {
        // update depth-only or stencil-only meta data
        PAL_ASSERT(dwordsToCopy == 1);
        const size_t strideWriteData = sizeof(Gfx9FastDepthClearMetaData);

        WriteDataInfo writeData = {};
        writeData.engineType = EngineTypeUniversal;
        writeData.dstAddr    = gpuVirtAddr;
        writeData.engineSel  = engine_sel__pfp_write_data__prefetch_parser;
        writeData.dstSel     = dst_sel__pfp_write_data__memory;
        writeData.predicate  = predicate;

        for (size_t levelOffset = 0; levelOffset < range.numMips; levelOffset++)
        {
            pCmdSpace         += CmdUtil::BuildWriteData(writeData, dwordsToCopy, pSrcData, pCmdSpace);
            writeData.dstAddr += strideWriteData;
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Determines if this texture-compatible color image supports fast clears regardless of the clear color.  It is the
// callers responsibility to verify that this function is not called for depth images and that it is only called for
// texture-compatible images as well.
bool Image::ColorImageSupportsAllFastClears() const
{
    bool allColorClearsSupported = false;

    PAL_ASSERT (m_pParent->IsDepthStencilTarget() == false);

    if (m_createInfo.samples > 1)
    {
        allColorClearsSupported = TestAnyFlagSet(FastClearAllTcCompatColorSurfs,
                                                 FastClearAllTcCompatColorSurfsMsaa);
    }
    else
    {
        allColorClearsSupported = TestAnyFlagSet(FastClearAllTcCompatColorSurfs,
                                                 FastClearAllTcCompatColorSurfsNoAa);
    }

    return allColorClearsSupported;
}

// =====================================================================================================================
bool Image::HasFmaskData() const
{
    // If this trips, that means that we only have either cMask or fMask which is invalid for GFX9.
    PAL_ASSERT (((m_pCmask == nullptr) ^ (m_pFmask == nullptr)) == false);

    return (m_pFmask != nullptr);
}

// =====================================================================================================================
// Determines if a resource's fmask is TC compatible/shader readable, allowing read access without an fmask expand.
bool Image::IsComprFmaskShaderReadable(
    const SubresId& subresource
    ) const
{
    const auto* pSettings                   = m_device.GetPublicSettings();
    const SubResourceInfo*const pSubResInfo = Parent()->SubresourceInfo(subresource);
    bool  isComprFmaskShaderReadable        = false;

    if (m_pImageInfo->internalCreateInfo.flags.useSharedMetadata)
    {
        isComprFmaskShaderReadable = m_pImageInfo->internalCreateInfo.sharedMetadata.flags.shaderFetchableFmask;
    }
    // If this device doesn't allow any tex fetches of fmask meta data, then don't bother continuing
    else if ((TestAnyFlagSet(pSettings->tcCompatibleMetaData, Pal::TexFetchMetaDataCapsFmask)) &&
             // MSAA surfaces on GFX9 must have fMask and must have cMask data as well.
             (m_createInfo.samples > 1))
    {
        // Either image is tc-compatible or if not it has no dcc and hence we can keep famsk surface
        // in tccompatible state
        const bool  supportsMetaFetches =
               ((pSubResInfo->flags.supportMetaDataTexFetch == 1) ||
                ((pSubResInfo->flags.supportMetaDataTexFetch == 0) && (HasDccData() == false)));

        // If this image isn't readable by a shader then no shader is going to be performing texture fetches from
        // it... Msaa image with resolveSrc usage flag will go through shader based resolve if fixed function
        // resolve is not preferred, the image will be readable by a shader.
        const bool  isShaderReadable    =
                (m_pParent->IsShaderReadable() ||
                 (m_pParent->IsResolveSrc() && (m_pParent->PreferCbResolve() == false)));

        isComprFmaskShaderReadable = supportsMetaFetches &&
                                     isShaderReadable    &&
                                     // The GFX10 fMask SRD will never set the "write" bit but we don't want
                                     // to make a shader-writeable image disallow reading of fMask data either.
                                     (IsGfx10(m_device) || (m_pParent->IsShaderWritable() == false));
    }

    return isComprFmaskShaderReadable;
}

// =====================================================================================================================
// Determines if this swizzle supports direct texture fetches of its meta data or not
bool Image::SupportsMetaDataTextureFetch(
    AddrSwizzleMode  swizzleMode,
    ChNumFormat      format,
    const SubresId&  subResource
    ) const
{
    bool texFetchSupported = false;

    if (m_pImageInfo->internalCreateInfo.flags.useSharedMetadata)
    {
        texFetchSupported = m_pImageInfo->internalCreateInfo.sharedMetadata.flags.shaderFetchable;
    }
    else
    {
        // If this device doesn't allow any tex fetches of meta data, then don't bother continuing
        if ((m_device.GetPublicSettings()->tcCompatibleMetaData != 0) &&
            // If the image requested TC compat off
            (m_pParent->GetImageCreateInfo().metadataTcCompatMode != MetadataTcCompatMode::Disabled) &&
            // If this image isn't readable by a shader then no shader is going to be performing texture fetches from
            // it... Msaa image with resolveSrc usage flag will go through shader based resolve if fixed function
            // resolve is not preferred, the image will be readable by a shader.
            (m_pParent->IsShaderReadable() ||
             // For GFX10+, copy (UAV) dst layout is a compressed state. Barrier to this layout won't trigger metadata
             // decompression. If supportMetaDataTexFetch is not set properly, then RPM copy operation will not enable
             // metadata access (read/write) but write to image directly -- data and meta can be incoherent afterwards.
             (IsGfx10Plus(m_device) && m_pParent->IsShaderWritable()) ||
             (m_pParent->IsResolveSrc() && (m_pParent->PreferCbResolve() == false))) &&
            // Meta-data isn't fetchable if the meta-data itself isn't addressable
            CanMipSupportMetaData(subResource.mipLevel) &&
            // Linear swizzle modes don't have meta-data to be fetched
            (AddrMgr2::IsLinearSwizzleMode(swizzleMode) == false))
        {
            if (m_pParent->IsDepthStencilTarget())
            {
                // Check if DB resource can use shader compatible compression
                texFetchSupported = DepthImageSupportsMetaDataTextureFetch(format, subResource);
            }
            else
            {
                // Check if this color resource can use shader compatible compression
                texFetchSupported = ColorImageSupportsMetaDataTextureFetch();
            }
        }
    }

    return texFetchSupported;
}

// =====================================================================================================================
// Determines if this color surface supports direct texture fetches of its cmask/fmask/dcc data or not. Note that this
// function is more a heurestic then actual fact, so it should be used with care.
bool Image::ColorImageSupportsMetaDataTextureFetch() const
{
    bool         texFetchAllowed = false;  // Assume texture fetches won't be allowed

    // Does this image have DCC memory?  Note that we have yet to allocate DCC memory
    // true param assumes resource can be made TC compat since this isn't known for sure at this time.
    if (Gfx9Dcc::UseDccForImage((*this), true))
    {
        if ((m_createInfo.samples > 1) &&
            // MSAA meta-data surfaces are only texture fetchable if allowed in the caps.
            TestAnyFlagSet(m_device.GetPublicSettings()->tcCompatibleMetaData, TexFetchMetaDataCapsMsaaColor))
        {
            texFetchAllowed = true;
        }
        else if ((m_createInfo.samples == 1) &&
                 TestAnyFlagSet(m_device.GetPublicSettings()->tcCompatibleMetaData, TexFetchMetaDataCapsNoAaColor))
        {
            texFetchAllowed = true;
        }
    }

    return texFetchAllowed;
}

// =====================================================================================================================
// Returns true if the format surface's hTile data can be directly fetched by the texture block. The z-specific plane
// of the surface must be z-32.
bool Image::DepthMetaDataTexFetchIsZValid(
    ChNumFormat  format
    ) const
{
    const ZFormat zHwFmt   = m_gfxDevice.GetHwZFmt(format);

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
// Determines if this depth surface supports direct texture fetches of its htile data
bool Image::DepthImageSupportsMetaDataTextureFetch(
    ChNumFormat     format,
    const SubresId& subResource
    ) const
{
    const bool   isStencilPlane = m_pParent->IsStencilPlane(subResource.plane);
    bool         isFmtLegal     = true;

    if (m_pParent->HasStencilPlane() &&
        (TestAnyFlagSet(m_device.GetPublicSettings()->tcCompatibleMetaData, TexFetchMetaDataCapsAllowStencil) == false))
    {
        // The settings disallows tex fetches of any compressed depth image that contains stencil
        isFmtLegal = false;
    }

    if (isFmtLegal)
    {
        if (m_pParent->IsDepthPlane(subResource.plane))
        {
            isFmtLegal = DepthMetaDataTexFetchIsZValid(format);
        }
        else if (isStencilPlane)
        {
            if (m_pParent->HasDepthPlane())
            {
                // Verify that the z-plane of this image is compatible with the texture pipe and compression.
                const SubresId zSubres = { 0, subResource.mipLevel, subResource.arraySlice };

                isFmtLegal = DepthMetaDataTexFetchIsZValid(Parent()->SubresourceInfo(zSubres)->format.format);
            }
        }
    }

    // Assume that texture fetches won't work.
    bool  texFetchAllowed = false;

    // Image must have hTile data for a meta-data texture fetch to make sense.  This function is called before any
    // hTile memory has been allocated, so we can't look to see if hTile memory actually exists, because it won't.
    const HtileUsageFlags  hTileUsage = Gfx9Htile::UseHtileForImage(m_device, (*this));
    if (hTileUsage.dsMetadata != 0)
    {
        if (isFmtLegal)
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

        if (isStencilPlane && IsHtileDepthOnly())
        {
            // If this is the stencil plane and the hTile data will only contain Z data, then we can't meta-fetch
            // this data.
            texFetchAllowed = false;
        }
    }

    return texFetchAllowed;
}

// =====================================================================================================================
// Initializes the metadata in the given subresource range using CmdFillMemory calls.
void Image::InitMetadataFill(
    Pal::CmdBuffer*    pCmdBuffer,
    const SubresRange& range,
    ImageLayout        layout
    ) const
{
    PAL_ASSERT(Parent()->IsRangeFullPlane(range));

    const auto& gpuMemObj = *Parent()->GetBoundGpuMemory().Memory();
    const auto  boundGpuMemOffset = Parent()->GetBoundGpuMemory().Offset();

    if (HasHtileData())
    {
        const uint32 initValue = m_pHtile->GetInitialValue();

        // This will initialize both the depth and stencil planes simultaneously.  They share hTile data,
        // so it isn't practical to init them separately anyway
        pCmdBuffer->CmdFillMemory(gpuMemObj,
                                  m_pHtile->MemoryOffset() + boundGpuMemOffset,
                                  m_pHtile->TotalSize(),
                                  initValue);

        PAL_ASSERT(m_pHtile->HasMetaEqGenerator());
        m_pHtile->GetMetaEqGenerator()->UploadEq(pCmdBuffer);
    }
    else
    {
        if (HasDccData())
        {
            for (SubresId subresId = range.startSubres;
                 subresId.plane < (range.startSubres.plane + range.numPlanes);
                 subresId.plane++)
            {
                const Gfx9Dcc* pDcc = GetDcc(subresId.plane);
                const uint32 dccInitValue = ReplicateByteAcrossDword(pDcc->GetInitialValue(layout));

                pCmdBuffer->CmdFillMemory(gpuMemObj,
                                          pDcc->MemoryOffset() + boundGpuMemOffset,
                                          pDcc->TotalSize(),
                                          dccInitValue);

                PAL_ASSERT(pDcc->HasMetaEqGenerator());
                pDcc->GetMetaEqGenerator()->UploadEq(pCmdBuffer);

                if (HasDisplayDccData())
                {
                    const Gfx9Dcc* pDispDcc = GetDisplayDcc(subresId.plane);

                    PAL_ASSERT(pDispDcc->HasMetaEqGenerator());
                    pDispDcc->GetMetaEqGenerator()->UploadEq(pCmdBuffer);
                }
            }
        }

        // If we have fMask then we also have cMask.
        if (HasFmaskData())
        {
            const uint8  cmaskInitValue    = m_pCmask->GetInitialValue();
            const uint32 expandedInitValue = ReplicateByteAcrossDword(cmaskInitValue);

            pCmdBuffer->CmdFillMemory(gpuMemObj,
                                      m_pCmask->MemoryOffset() + boundGpuMemOffset,
                                      m_pCmask->TotalSize(),
                                      expandedInitValue);

            PAL_ASSERT(m_pCmask->HasMetaEqGenerator());
            m_pCmask->GetMetaEqGenerator()->UploadEq(pCmdBuffer);

            pCmdBuffer->CmdFillMemory(gpuMemObj,
                                      m_pFmask->MemoryOffset(),
                                      m_pFmask->TotalSize(),
                                      Gfx9Fmask::GetPackedExpandedValue(*this));
        }
    }

    if (HasFastClearMetaData(range))
    {
        // The DB Tile Summarizer requires a TC compatible clear value of stencil,
        // because TC isn't aware of DB_STENCIL_CLEAR register.
        // Please note the clear value of color or depth is also initialized together,
        // although it might be unnecessary.
        for (SubresId subresId = range.startSubres;
             subresId.plane < (range.startSubres.plane + range.numPlanes);
             subresId.plane++)
        {
            pCmdBuffer->CmdFillMemory(gpuMemObj,
                                      FastClearMetaDataOffset(subresId),
                                      FastClearMetaDataSize(subresId.plane, range.numMips),
                                      0);
        }
    }

    if (HasHiSPretestsMetaData() && Parent()->HasStencilPlane(range))
    {
        pCmdBuffer->CmdFillMemory(gpuMemObj,
                                  HiSPretestsMetaDataOffset(range.startSubres.mipLevel),
                                  HiSPretestsMetaDataSize(range.numMips),
                                  0);
    }

    if (HasWaTcCompatZRangeMetaData() && Parent()->HasDepthPlane(range))
    {
        pCmdBuffer->CmdFillMemory(gpuMemObj,
                                  WaTcCompatZRangeMetaDataOffset(range.startSubres.mipLevel),
                                  WaTcCompatZRangeMetaDataSize(range.numMips),
                                  0);
    }

    if (HasDccStateMetaData(range))
    {
        // We need to initialize the Image's DCC state metadata to indicate that the Image can become DCC compressed
        // (or not) in upcoming operations.
        const bool canCompress = ImageLayoutCanCompressColorData(LayoutToColorCompressionState(), layout);
        const uint32 dccStateInitValue = canCompress ? 1 : 0;

        for (SubresId subresId = range.startSubres;
             subresId.plane < (range.startSubres.plane + range.numPlanes);
             subresId.plane++)
        {
            pCmdBuffer->CmdFillMemory(gpuMemObj,
                                      GetDccStateMetaDataOffset(subresId),
                                      GetDccStateMetaDataSize(subresId, range.numMips),
                                      dccStateInitValue);
        }
    }

    if (HasFastClearEliminateMetaData(range))
    {
        pCmdBuffer->CmdFillMemory(gpuMemObj,
                                  GetFastClearEliminateMetaDataOffset(range.startSubres),
                                  GetFastClearEliminateMetaDataSize(range.numMips),
                                  0);
    }
}

// =====================================================================================================================
ImageType Image::GetOverrideImageType() const
{
    const auto*  pParent    = Parent();
    const auto&  createInfo = pParent->GetImageCreateInfo();
    const auto&  settings   = GetGfx9Settings(m_device);
    ImageType    imageType  = createInfo.imageType;

    // You would think this would be nice and simple, but it's not.  :-(  The Vulkan and DX12 APIs require that
    // 1D depth images work.  GFX9 imposes these requirements that make that difficult:
    //    1) 1D images must be linear
    //    2) Depth images must be swizzled with one of the _Z modes (i.e., not linear).
    //
    // We're going to work around this by forcing 1D depth image requests to be 2D images.  This requires SC help
    // to adjust the coordinates.  Since SC doesn't understand the difference between color and depth images, all
    // 1D image requests need to be overriden to 2D.
    if (settings.treat1dAs2d && (imageType == ImageType::Tex1d))
    {
        imageType = ImageType::Tex2d;
    }

    return imageType;
}

// =====================================================================================================================
// Returns true if the given plane supports decompress operations on the compute queue
bool Image::SupportsComputeDecompress(
    const SubresRange& range
    ) const
{
    const auto& layoutToState = m_layoutToState;

    bool supportsCompression = true;

    for (uint32 plane = range.startSubres.plane; plane < (range.startSubres.plane + range.numPlanes); plane++)
    {
        const uint32 engines = (m_pParent->IsDepthStencilTarget()
                                ? layoutToState.depthStencil[plane].compressed.engines
                                : layoutToState.color.compressed.engines);

        supportsCompression &= TestAnyFlagSet(engines, LayoutComputeEngine);
    }

    return supportsCompression;
}

// =====================================================================================================================
// Returns the virtual address used for HW programming of the given mip.  Returned value includes any pipe-bank-xor
// value associated with this plane and does not include the mip tail offset.
gpusize Image::GetPlaneBaseAddr(
    uint32 plane,
    uint32 arraySlice
    ) const
{
    // On GFX9, the registers are programmed to select the proper mip level and slice
    const SubresId subresId = { plane, 0, arraySlice };
    return GetMipAddr(subresId);
}

// =====================================================================================================================
// Returns the virtual address used for HW programming of the given mip.  Returned value includes any pipe-bank-xor
// value associated with this subresource id.
gpusize Image::GetMipAddr(
    SubresId  subresId
    ) const
{
    const Pal::Image* pParent          = Parent();
    const auto*       pBaseSubResInfo  = pParent->SubresourceInfo(subresId);
    const auto*       pAddrOutput      = GetAddrOutput(pBaseSubResInfo);
    const auto&       mipInfo          = pAddrOutput->pMipInfo[subresId.mipLevel];
    const GfxIpLevel  gfxLevel         = pParent->GetDevice()->ChipProperties().gfxLevel;
    gpusize           imageBaseAddr    = 0;
    const bool        isYuvPlanarArray = pParent->IsYuvPlanarArray();

    if (gfxLevel == GfxIpLevel::GfxIp9)
    {
        // Making this complicated (of course, it's what we do), if mip 0 / slice 0 is part of the mip-tail, then it
        // won't reside at the start of the allocation!  Subtract off the mip-tail-offset to get back to where the
        // plane starts.
        imageBaseAddr = pParent->GetSubresourceBaseAddr(subresId) - mipInfo.mipTailOffset;
    }
    else if (IsGfx10Plus(gfxLevel))
    {
        // On GFX10, programming is based on the logical starting address of the plane.  Mips are stored in
        // reverse order (i.e., mip 0 is *last* and the last mip level isn't necessarily at offset zero either), so
        // we need to figure out where this plane begins.  Fun!
        const gpusize planeOffset = isYuvPlanarArray ? pBaseSubResInfo->offset : m_planeOffset[subresId.plane];

        imageBaseAddr = pParent->GetBoundGpuMemory().GpuVirtAddr() + planeOffset;
    }
    else
    {
        // What is this?
        PAL_ASSERT_ALWAYS();
    }

    const auto*   pTileInfo   = AddrMgr2::GetTileInfo(pParent, subresId);
    const gpusize pipeBankXor = pTileInfo->pipeBankXor;
    const gpusize addrWithXor = imageBaseAddr | (pipeBankXor << 8);

    return addrWithXor;
}

// =====================================================================================================================
// Returns the buffer view of metadata lookup table for specified mip level
void Image::BuildMetadataLookupTableBufferView(
    BufferViewInfo* pViewInfo,
    uint32 mipLevel
    ) const
{
    const auto* pPublicSettings = m_device.GetPublicSettings();

    pViewInfo->gpuAddr        = Parent()->GetGpuVirtualAddr() + m_metaDataLookupTableOffsets[mipLevel];
    pViewInfo->range          = m_metaDataLookupTableSizes[mipLevel];
    pViewInfo->stride         = 1;
    pViewInfo->swizzledFormat = UndefinedSwizzledFormat;
    pViewInfo->flags.bypassMallRead =
        TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnRead);
    pViewInfo->flags.bypassMallWrite =
        TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnWrite);
}

// =====================================================================================================================
// Returns true if specified mip level is in the MetaData tail region.
bool Image::IsInMetadataMipTail(
    const SubresId&  subResId
    ) const
{
    bool inMipTail = false;
    if (m_createInfo.mipLevels > 1)
    {
        const Gfx9Dcc*  pDcc = GetDcc(subResId.plane);
        if (pDcc != nullptr)
        {
            inMipTail = (pDcc->GetAddrMipInfo(subResId.mipLevel).inMiptail != 0);
        }
        else if (m_pHtile != nullptr)
        {
            inMipTail = (m_pHtile->GetAddrMipInfo(subResId.mipLevel).inMiptail != 0);
        }
    }
    return inMipTail;
}

// =====================================================================================================================
// Returns true if the HW supports meta-data on the specified mip level
bool Image::CanMipSupportMetaData(
    uint32 mip
    ) const
{
    const bool supportsMetaData = ((IsGfx10Plus(*(m_gfxDevice.Parent())) == false) ||
                                   (mip <= m_addrSurfOutput[0].firstMipIdInTail));
    return supportsMetaData;
}

// =====================================================================================================================
// Function for updating the subResInfo offset to reflect each sub-resources position in the final image.  On input,
// the subres offset reflects the offset of that subresource within a generic slice, but not that slice's position
// in the overall image.
void Image::Addr2InitSubResInfo(
    const SubResIterator&  subResIt,
    SubResourceInfo*       pSubResInfoList,
    void*                  pSubResTileInfoList,
    gpusize*               pGpuMemSize)
{
    const GfxIpLevel  gfxLevel = m_device.ChipProperties().gfxLevel;

    SetupPlaneOffsets();

    if (gfxLevel == GfxIpLevel::GfxIp9)
    {
        Addr2InitSubResInfoGfx9(subResIt, pSubResInfoList, pSubResTileInfoList, pGpuMemSize);
    }
    else if (IsGfx10Plus(gfxLevel))
    {
        Addr2InitSubResInfoGfx10(subResIt, pSubResInfoList, pSubResTileInfoList, pGpuMemSize);
    }
}

// =====================================================================================================================
// GFX9 specific version of the Addr2InitSubResInfo function
void Image::Addr2InitSubResInfoGfx9(
    const SubResIterator&  subResIt,
    SubResourceInfo*       pSubResInfoList,
    void*                  pSubResTileInfoList,
    gpusize*               pGpuMemSize)
{
    SubResourceInfo*const pSubRes       = (pSubResInfoList + subResIt.Index());
    SubResourceInfo*const pBaseSubRes   = (pSubResInfoList + subResIt.BaseIndex());
    TileInfo*const        pTileInfo     = NonConstTileInfo(pSubResTileInfoList, subResIt.Index());
    TileInfo*const        pBaseTileInfo = NonConstTileInfo(pSubResTileInfoList, subResIt.BaseIndex());

    // Each subresource's offset is currently relative to the base mip level within its plane & array slice. The
    // overall offset for each subresource must be computed.
    if (pSubRes->subresId.mipLevel == 0)
    {
        // For the base mip level, the offset and backing-store offset need to be updated to include the total
        // offset of all array slices and planes seen so far.
        pSubRes->offset               += *pGpuMemSize;
        pTileInfo->backingStoreOffset += *pGpuMemSize;
        // In AddrMgr2, each subresource's size represents the size of the full mip-chain it belongs to. By
        // adding the size of mip-level zero to the running GPU memory size, we can keep a running total of
        // the entire Image's size.
        *pGpuMemSize += pSubRes->size;
    }
    else
    {
        // For other mip levels, the offset and backing store offset need to include the offset from the Image's
        // base to the base mip level of the current array slice & plane.
        // Also, need to be careful if mip 0 is in the mip tail. In this case, mipN's offset is less than mip0's.
        if (pBaseTileInfo->mip0InMipTail == true)
        {
            const gpusize baseOffsetNoMipTail = pBaseSubRes->offset & (~pBaseTileInfo->mipTailMask);
            pSubRes->offset += baseOffsetNoMipTail;
        }
        else
        {
            pSubRes->offset += pBaseSubRes->offset;
        }
        pTileInfo->backingStoreOffset += pBaseTileInfo->backingStoreOffset;
    }
}

// =====================================================================================================================
// Returns TRUE if this image should set ITERATE_256 for the specified subresource
//
// The driver may only set ITERATE_256 to 0, if every 64KB of the surface is guaranteed to be contiguous in
// physical memory, for example, if the VM system uses 64 KB pages.  It is recommended that the driver make
// every effort to keep ITERATE_256 to be 0, since this gives optimal performance.  So if a surface has a
// potential to spill out to system memory, which typically uses 4 KB pages, it can still set ITERATE_256 = 0,
// so long as the driver can guarantee that the hardware is not allowed to access this surface while it is in
// system memory.  Note that this recommendation really only applies to MSAA depth or stencil surfaces that are
// tc compatible.  The iterate_256 state has no effect on 1xaa surfaces.
//
// For APUs, which typically use 4 KB pages, this optimization isn't an option, so the driver can force
// ITERATE_256 to be enabled, by setting DB_DEBUG2.FORCE_ITERATE_256 = FORCE_ENABLE
uint32 Image::GetIterate256(
    const SubResourceInfo*  pSubResInfo
    ) const
{
    const auto& settings         = GetGfx9Settings(m_device);
    const auto& chipProperties   = m_device.ChipProperties();
    const auto& memoryProperties = m_device.MemoryProperties();

    // The iterate-256 bit doesn't exist except on GFX10+ products...  It's not "bad" to call this function on
    // other GPUs, but the answer isn't meaningful
    PAL_ASSERT(IsGfx10Plus(chipProperties.gfxLevel));

    // Leaving iterate256=1 is the safe case.
    uint32  iterate256 = 1;

    const auto* pParent = Parent();
    // Look if the device supports setting iterate256 = 0.
    if (m_gfxDevice.SupportsIterate256() &&
        // Check for image-specific constraints here.
        pParent->GetGfxImage()->IsIterate256Meaningful(pSubResInfo) &&
        // If the panel is forcing setting the iterate256 bit to disable the optimization, then ignore everything else.
        (settings.disableIterate256Opt == false))
    {
        // And finally check the memory alignment requirements.  If there's no bound memory we don't know
        // what the page-size is. Also, shared images are always in system memory where we have no idea
        // what the page-size is.
        const bool  isShared  = pParent->IsShared();
        const auto& boundMem  = pParent->GetBoundGpuMemory();

        if ((isShared == false) && boundMem.IsBound() && boundMem.Memory()->IsLocalOnly())
        {
            const gpusize allocSize     = boundMem.Memory()->Desc().size;
            const gpusize alignment     = boundMem.Memory()->Desc().alignment;
            const gpusize physAlignment = boundMem.Memory()->GetPhysicalAddressAlignment();

            // Iterate256 is not supported for allocations < iterate256MinAlignment.
            if ((allocSize >= memoryProperties.iterate256LargeAlignment) &&
                (memoryProperties.iterate256LargeAlignment > 0) &&
                IsPow2Aligned(allocSize, memoryProperties.iterate256LargeAlignment) &&
                IsPow2Aligned(alignment, memoryProperties.iterate256LargeAlignment) &&
                IsPow2Aligned(physAlignment, memoryProperties.iterate256LargeAlignment))
            {
                iterate256 = 0;
            }
            else if ((allocSize >= memoryProperties.iterate256MinAlignment) &&
                     (memoryProperties.iterate256MinAlignment > 0) &&
                     IsPow2Aligned(allocSize, memoryProperties.iterate256MinAlignment) &&
                     IsPow2Aligned(alignment, memoryProperties.iterate256MinAlignment) &&
                     IsPow2Aligned(physAlignment, memoryProperties.iterate256MinAlignment))
            {
                iterate256 = 0;
            }
        }
    }
    return iterate256;
}

// =====================================================================================================================
// GFX10 specific version of the Addr2InitSubResInfo function
void Image::Addr2InitSubResInfoGfx10(
    const SubResIterator&  subResIt,
    SubResourceInfo*       pSubResInfoList,
    void*                  pSubResTileInfoList,
    gpusize*               pGpuMemSize)
{
    const Pal::Image*     pParent     = Parent();
    const auto&           createInfo  = pParent->GetImageCreateInfo();
    const bool            isYuvPlanar = Formats::IsYuvPlanar(createInfo.swizzledFormat.format);
    SubResourceInfo*const pSubRes     = (pSubResInfoList + subResIt.Index());
    const auto*           pAddrOutput = GetAddrOutput(pSubRes);
    const uint32          plane       = pSubRes->subresId.plane;
    TileInfo*const        pTileInfo   = NonConstTileInfo(pSubResTileInfoList, subResIt.Index());

    if (isYuvPlanar == false)
    {
        // For non-YUV planar surfaces, each plane is stored contiguously.  i.e., all of plane 0 data is stored
        // prior to plane 1 data starting.
        pSubRes->offset = pSubRes->offset      + // existing offset to this miplevel within the slice
                          m_planeOffset[plane] + // offset of any previous planes
                          pSubRes->subresId.arraySlice * pAddrOutput->sliceSize; // offset of any previous slices
    }
    else
    {
        // YUV planar surfaces are stored in [y] [uv] order for each slice.  i.e., the Y data across various
        // slices is non-contiguous.  YUV surfaces can't have multiple mip levels.
        pSubRes->offset = m_planeOffset[plane] +                           // offset within this slice
                          pSubRes->subresId.arraySlice * m_totalPlaneSize; // all previous slices
    }

    if (pSubRes->subresId.mipLevel == 0)
    {
        // In AddrMgr2, each subresource's size represents the size of the full mip-chain it belongs to. By
        // adding the size of mip-level zero to the running GPU memory size, we can keep a running total of
        // the entire Image's size.

        // If this image is being created for the purpose of VRS storage, then don't bother allocating
        // memory for the image portion...  we only care about the hTile memory that will get ultimately
        // be associated with this image.
        if (pParent->GetImageInfo().internalCreateInfo.flags.vrsOnlyDepth == 0)
        {
            *pGpuMemSize += pSubRes->size;
        }

        pTileInfo->backingStoreOffset += *pGpuMemSize;
    }
    else
    {
        const TileInfo*const pBaseTileInfo = NonConstTileInfo(pSubResTileInfoList, subResIt.BaseIndex());

        pTileInfo->backingStoreOffset += pBaseTileInfo->backingStoreOffset;
    }
}

// =====================================================================================================================
// Fillout shared metadata information.
void Image::GetSharedMetadataInfo(
    SharedMetadataInfo* pMetadataInfo
    ) const
{
    memset(pMetadataInfo, 0, sizeof(SharedMetadataInfo));

    const SubresId baseSubResId = {};

    pMetadataInfo->numPlanes = 0;
    for (uint32  dccPlane = 0; dccPlane < m_numDccPlanes; dccPlane++)
    {
        pMetadataInfo->dccOffset[dccPlane]                        = m_pDcc[dccPlane]->MemoryOffset();
        pMetadataInfo->pipeAligned[dccPlane]                      = m_pDcc[dccPlane]->PipeAligned();
        pMetadataInfo->dccStateMetaDataOffset[dccPlane]           = m_dccStateMetaDataOffset[dccPlane];
        pMetadataInfo->fastClearEliminateMetaDataOffset[dccPlane] = m_fastClearEliminateMetaDataOffset[dccPlane];
        pMetadataInfo->fastClearMetaDataOffset[dccPlane]          = m_fastClearMetaDataOffset[dccPlane];

        PAL_ASSERT(m_pDcc[dccPlane]->HasMetaEqGenerator());
        pMetadataInfo->flags.hasEqGpuAccess = m_pDcc[dccPlane]->GetMetaEqGenerator()->HasEqGpuAccess();

        pMetadataInfo->numPlanes = m_numDccPlanes;
    }
    if (m_pCmask != nullptr)
    {
        pMetadataInfo->cmaskOffset               = m_pCmask->MemoryOffset();

        PAL_ASSERT(m_pCmask->HasMetaEqGenerator());
        pMetadataInfo->flags.hasCmaskEqGpuAccess = m_pCmask->GetMetaEqGenerator()->HasEqGpuAccess();
    }
    if (m_pFmask != nullptr)
    {
        pMetadataInfo->fmaskOffset                = m_pFmask->MemoryOffset();
        pMetadataInfo->flags.shaderFetchableFmask = IsComprFmaskShaderReadable(baseSubResId);
        pMetadataInfo->fmaskXor                   = m_pFmask->GetPipeBankXor();
        pMetadataInfo->fmaskSwizzleMode           = m_pFmask->GetSwizzleMode();
    }
    if (m_pHtile != nullptr)
    {
        pMetadataInfo->htileOffset                = m_pHtile->MemoryOffset();
        pMetadataInfo->flags.hasWaTcCompatZRange  = HasWaTcCompatZRangeMetaData();
        pMetadataInfo->flags.hasHtileLookupTable  = HasHtileLookupTable();
        pMetadataInfo->flags.htileHasDsMetadata   = HasDsMetadata();

        PAL_ASSERT(m_pHtile->HasMetaEqGenerator());
        pMetadataInfo->flags.hasEqGpuAccess       = m_pHtile->GetMetaEqGenerator()->HasEqGpuAccess();

        pMetadataInfo->fastClearMetaDataOffset[0] = m_fastClearMetaDataOffset[0];
        pMetadataInfo->numPlanes                  = 1;
    }

    pMetadataInfo->flags.shaderFetchable            =
        Parent()->SubresourceInfo(baseSubResId)->flags.supportMetaDataTexFetch;

    pMetadataInfo->hisPretestMetaDataOffset         = m_hiSPretestsMetaDataOffset;
    pMetadataInfo->htileLookupTableOffset           = m_metaDataLookupTableOffsets[0];
}

// =====================================================================================================================
void Image::GetDisplayDccState(
    DccState* pState
    ) const
{
    if (m_pDispDcc[0] != nullptr)
    {
        PAL_ASSERT(m_numDccPlanes == 1); // VCAM_SURFACE_DESCdoes not support YUV presentable yet
        m_pDispDcc[0]->GetState(pState);
    }
}

// =====================================================================================================================
void Image::GetDccState(
    DccState* pState
    ) const
{
    if (m_pDcc[0] != nullptr)
    {
        PAL_ASSERT(m_numDccPlanes == 1);
        m_pDcc[0]->GetState(pState);
    }
}

// =====================================================================================================================
// Get the default layout which is the optimally compressed layout for the subresource.
Result Image::GetDefaultGfxLayout(
    SubresId     subresId,
    ImageLayout* pLayout
    ) const
{
    Result result = Result::ErrorInvalidPointer;

    if (pLayout != nullptr)
    {
        if (Parent()->IsDepthStencilTarget())
        {
            *pLayout = m_defaultGfxLayout.depthStencil[subresId.plane];
        }
        else
        {
            *pLayout = m_defaultGfxLayout.color;
        }

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Returns true if this image's hTile data will not contain stencil data.  Used before creating the hTile object.
bool Image::IsHtileDepthOnly() const
{
    const Pal::Device*  pDevice    = m_gfxDevice.Parent();
    const Pal::Image*   pParent    = Parent();
    const auto&         createInfo = pParent->GetImageCreateInfo();
    const auto&         settings   = GetGfx9Settings(*pDevice);

    bool  depthOnly = false;

    PAL_ASSERT(pParent->IsDepthStencilTarget());

    // Use Z-only hTile if this image's format doesn't have a stencil plane
    if ((pDevice->SupportsStencil(createInfo.swizzledFormat.format, createInfo.tiling) == false) ||
        (settings.waForceZonlyHtileForMipmaps && (createInfo.mipLevels > 1))
       )
    {
        PAL_ASSERT(pDevice->SupportsDepth(createInfo.swizzledFormat.format, createInfo.tiling));

        // If this Image's format does not contain stencil data, allow the HW to use the extra HTile bits for improved
        // HiZ Z-range precision.
        depthOnly = true;
    }

    return depthOnly;
}

// =====================================================================================================================
bool Image::NeedFlushForMetadataPipeMisalignment(
    const SubresRange& range
    ) const
{
    bool result = false;

    for (uint32 plane = range.startSubres.plane;
         plane < (range.startSubres.plane + range.numPlanes);
         plane++)
    {
        result |= ((range.startSubres.mipLevel + range.numMips - 1) >= m_firstMipMetadataPipeMisaligned[plane]);
    }

    return result;
}

// =====================================================================================================================
// We need reset the base level when the block szie is larger than the mip chain, e.g:
// Uncompressed pixels          Compressed block sizes (astc8x8)
//  mip0:       604 x 604                 80 x 80
//  mip1:       302 x 302                 40 x 40
//  mip2:       151 x 151                 20 x 20
//  mip3:        75 x  75                 10 x 10
//  mip4:        37 x  37                  5 x 5
//  mip5:        18 x  18                  2 x 2
// For mip5, if we don't compute the non-BC view, HW will get 2 according to the mip chain.
// To fix it, we need call Addr2ComputeNonBlockCompressedView for it.
gpusize Image::ComputeNonBlockCompressedView(
    const SubResourceInfo* pBaseSubResInfo,
    const SubResourceInfo* pMipSubResInfo,
    uint32*                pMipLevels,  // Out: Number of mips in the view
    uint32*                pMipId,      // Out: First mip in the view
    Extent3d*              pExtent      // Out: width/height of the first mip in the view
    )const
{
    ADDR2_COMPUTE_SLICE_PIPEBANKXOR_INPUT  inSliceXor      = { 0 };
    ADDR2_COMPUTE_SLICE_PIPEBANKXOR_OUTPUT outSliceXor     = { 0 };
    const auto*const                       pParent         = Parent();
    const ImageCreateInfo&                 imageCreateInfo = pParent->GetImageCreateInfo();
    Pal::Device*                           pDevice         = pParent->GetDevice();
    const auto*const                       pAddrOutput     = GetAddrOutput(pBaseSubResInfo);
    const auto&                            surfSetting     = GetAddrSettings(pBaseSubResInfo);
    const auto*                            pTileInfo       = Pal::AddrMgr2::GetTileInfo(pParent, pBaseSubResInfo->subresId);

    // It should be non BC view compatible
    PAL_ASSERT(AddrMgr2::IsNonBcViewCompatible(surfSetting.swizzleMode, imageCreateInfo.imageType));

    ADDR2_COMPUTE_NONBLOCKCOMPRESSEDVIEW_INPUT nbcIn = {};
    nbcIn.size         = sizeof(nbcIn);

    nbcIn.swizzleMode  = surfSetting.swizzleMode;
    nbcIn.resourceType = surfSetting.resourceType;
    nbcIn.format       = Pal::Image::GetAddrFormat(imageCreateInfo.swizzledFormat.format);
    nbcIn.width        = pBaseSubResInfo->extentTexels.width;
    nbcIn.height       = pBaseSubResInfo->extentTexels.height;
    nbcIn.numSlices    = imageCreateInfo.arraySize;
    nbcIn.numMipLevels = imageCreateInfo.mipLevels;
    nbcIn.slice        = pMipSubResInfo->subresId.arraySlice;
    nbcIn.mipId        = pMipSubResInfo->subresId.mipLevel;
    nbcIn.pipeBankXor  = pTileInfo->pipeBankXor;

    ADDR2_COMPUTE_NONBLOCKCOMPRESSEDVIEW_OUTPUT nbcOut = {};
    nbcOut.size = sizeof(nbcOut);

    ADDR_E_RETURNCODE addrResult = Addr2ComputeNonBlockCompressedView(pDevice->AddrLibHandle(), &nbcIn, &nbcOut);
    PAL_ASSERT(addrResult == ADDR_OK);

    pExtent->width = nbcOut.unalignedWidth;
    pExtent->height = nbcOut.unalignedHeight;
    *pMipLevels  = nbcOut.numMipLevels;
    *pMipId = nbcOut.mipId;

    const gpusize gpuVirtAddress = pParent->GetGpuVirtualAddr() + nbcOut.offset;
    const gpusize pipeBankXor    = nbcOut.pipeBankXor;
    const gpusize addrWithXor    = gpuVirtAddress | (pipeBankXor<< 8);

    return (addrWithXor >> 8);
}

// =====================================================================================================================
// The driver will need to Flush & Invalidate cachelines in L2 which access metadata surfaces when switching between
// CB/DB accesses and TC accesses of an Image.  This is because the driver assumes that all metadata surfaces are pipe
// aligned, but there are cases where the data is not *actually* pipe-aligned because the CB/DB use a slightly different
// addressing scheme than the TC does.
void Image::InitPipeMisalignedMetadataFirstMip()
{
    const ImageCreateInfo& createInfo = m_pParent->GetImageCreateInfo();
    const ImageInfo&       imageInfo  = m_pParent->GetImageInfo();

    const uint32 subresourcesPerPlane = (createInfo.arraySize * createInfo.mipLevels);
    for (uint32 planeId = 0; planeId < imageInfo.numPlanes; ++planeId)
    {
        const SubResourceInfo& subRes = *(m_pParent->SubresourceInfo(0) + (planeId * subresourcesPerPlane));
        m_firstMipMetadataPipeMisaligned[planeId] = GetPipeMisalignedMetadataFirstMip(createInfo, subRes);
    }
}

// =====================================================================================================================
// Determines the first mipmap level for a single plane which suffers the pipe-misaligned metadata issue. A value of
// UINT_MAX indicates no mipmaps for this plane are vulnerable, and zero indicates all mips are.
uint32 Image::GetPipeMisalignedMetadataFirstMip(
    const ImageCreateInfo& createInfo,
    const SubResourceInfo& baseSubRes   // Base subresource for the current plane
    ) const
{
    uint32 firstMip = UINT_MAX;

    // Different GFX IP levels have different flavors of the pipe misalignment issue, and the workaround needs to be
    // applied in different circumstances for each.

    const GpuChipProperties& chipProps = m_gfxDevice.Parent()->ChipProperties();
    const bool               isDepth   = m_pParent->IsDepthStencilTarget();

    if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
    {
        // The pipe misalignment issue occurs on MSAA Z, MSAA color, mips in the metadata mip-tail, or any stencil.

        uint32 firstMipInMetadataMipTail = UINT_MAX;
        for (uint32 mip = 0; mip < createInfo.mipLevels; ++mip)
        {
            const SubresId  mipBaseSubResId = { baseSubRes.subresId.plane, mip, 0 };

            if (IsInMetadataMipTail(mipBaseSubResId))
            {
                firstMipInMetadataMipTail = mip;
                break;
            }
        }

        if (isDepth)
        {
            if (HasHtileData() && (baseSubRes.flags.supportMetaDataTexFetch != 0))
            {
                const bool stencil = m_pParent->IsStencilPlane(baseSubRes.subresId.plane);
                firstMip = ((stencil || (createInfo.samples > 1)) ? 0 : firstMipInMetadataMipTail);
            }
        }
        else
        {
            if (HasFmaskData() && (HasDccData() == false))
            {
                firstMip = 0;
            }
            else if (HasDccData() && (baseSubRes.flags.supportMetaDataTexFetch != 0))
            {
                firstMip = ((createInfo.samples > 1) ? 0 : firstMipInMetadataMipTail);
            }
        }
    }
    else
    {
        // The pipe misalignment issue occurs on Images which satisfy the following test:
        //
        // mod_log2_bpp        = (depth && num_slices >= 8) ? 2 : log2_bpp
        // clamped_bpe_samples = MIN(mod_log2_bpp + log2_samples, 6)
        //
        // overlap             = MAX(clamped_bpe_samples + log2_pipes - 8, 0)
        // samples_overlap     = MIN(log2_samples, overlap)
        //
        // Navi 1x/2x:
        // if (non-pow2-memory)
        //      do_flush = true
        // else if (depth)
        //      do_flush = (overlap > 0)
        // else if (color)
        //      do_flush = (samples_overlap > MAX(log2_samples - log2_max_compressed_frags, 0))
        //

        regGB_ADDR_CONFIG gbAddrConfig;
        gbAddrConfig.u32All = chipProps.gfx9.gbAddrConfig;

        const int32 log2Samples = Log2(createInfo.samples);
        const int32 log2Bpp     = Log2(baseSubRes.bitsPerTexel >> 3);

        int32 log2BppAndSamplesClamped = 0;
        if (IsGfx103Plus(chipProps.gfxLevel))
        {
            log2BppAndSamplesClamped = (log2Bpp + log2Samples);
        }
        else
        {
            const int32 modifiedLog2Bpp = ((isDepth && (createInfo.arraySize >= 8)) ? 2 : log2Bpp);
            log2BppAndSamplesClamped    = Min(6, (modifiedLog2Bpp + log2Samples));
        }

        const int32 overlap        = Max<int32>(0, (log2BppAndSamplesClamped + gbAddrConfig.bits.NUM_PIPES - 8));
        const int32 samplesOverlap = Min(log2Samples, overlap);
        const bool  isNonPow2Vram  = (IsPowerOfTwo(m_gfxDevice.Parent()->MemoryProperties().vramBusBitWidth) == false);

#if PAL_BUILD_GFX11
        // Add case of mips in the metadata mip-tail for GFX11
        if (IsGfx11(chipProps.gfxLevel))
        {
            uint32 firstMipInMetadataMipTail = UINT_MAX;
            for (uint32 mip = 0; mip < createInfo.mipLevels; ++mip)
            {
                const SubresId  mipBaseSubResId = { baseSubRes.subresId.plane, mip, 0 };

                if (IsInMetadataMipTail(mipBaseSubResId))
                {
                    firstMip = mip;
                    break;
                }
            }
        }
#endif

        if (isDepth)
        {
            if (HasHtileData() && (baseSubRes.flags.supportMetaDataTexFetch != 0) &&
                (isNonPow2Vram || (overlap > 0)))
            {
               firstMip = 0;
            }
        }
#if PAL_BUILD_GFX11
        else if (IsGfx11(chipProps.gfxLevel))
        {
            if (HasDccData() && (baseSubRes.flags.supportMetaDataTexFetch != 0) && (isNonPow2Vram || (overlap > 0)))
            {
                firstMip = 0;
            }
        }
#endif
        else
        {
            const int32 log2SamplesFragsDiff = Max<int32>(0, (log2Samples - gbAddrConfig.bits.MAX_COMPRESSED_FRAGS));
            if (isNonPow2Vram || (samplesOverlap > log2SamplesFragsDiff))
            {
                if ( (HasDccData() && (baseSubRes.flags.supportMetaDataTexFetch != 0)) ||
                     (HasFmaskData() && (HasDccData() == false) && IsComprFmaskShaderReadable(baseSubRes.subresId)) )
                {
                    firstMip = 0;
                }
            }
        }
    }

    return firstMip;
}

} // Gfx9
} // Pal
