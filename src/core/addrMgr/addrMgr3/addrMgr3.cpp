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

#include "palFormatInfo.h"
#include "palLiterals.h"
#include "core/addrMgr/addrMgr3/addrMgr3.h"
#include "core/device.h"
#include "core/hw/gfxip/gfx12/gfx12Chip.h"
#include "core/image.h"
#include "core/settingsLoader.h"

using namespace Util;
using namespace Literals;

namespace Pal
{
namespace AddrMgr3
{

// =====================================================================================================================
AddrMgr3::AddrMgr3(
    const Device* pDevice)
    :
    // Note: Each subresource for AddrMgr3 hardware needs the following tiling information: the actual tiling
    // information for itself as computed by the AddrLib.
    AddrMgr(pDevice, sizeof(TileInfo))
{
}

// =====================================================================================================================
Result Create(
    const Device*  pDevice,
    void*          pPlacementAddr,
    AddrMgr**      ppAddrMgr)
{
    PAL_ASSERT((pDevice != nullptr) && (pPlacementAddr != nullptr) && (ppAddrMgr != nullptr));

    AddrMgr* pAddrMgr = PAL_PLACEMENT_NEW(pPlacementAddr) AddrMgr3(pDevice);

    Result result = pAddrMgr->Init();

    if (result == Result::Success)
    {
        *ppAddrMgr = pAddrMgr;
    }
    else
    {
        pAddrMgr->Destroy();
    }

    return result;
}

// =====================================================================================================================
// Returns the size, in bytes, required to support an AddrMgr3 object
size_t GetSize()
{
    return sizeof(AddrMgr3);
}

// =====================================================================================================================
// Assembles the tile token for the given subresource. The tile token is a generated key which can determine if two
// optimally tiled images are compatible for copying when the supportsMismatchedTileTokenCopy capability flag is false.
void AddrMgr3::BuildTileToken(
    SubResourceInfo*  pSubResInfo,
    Addr3SwizzleMode  swizzleMode)
{
    TileToken token = { };

    token.bits.elementSize = Log2(pSubResInfo->bitsPerTexel >> 3);

    pSubResInfo->tileToken = token.u32All;
}

// =====================================================================================================================
// Return if need compute minimum padded surface size. Also return required ratioLow/Hi when selecting swizzle mode.
static bool GetSwizzleModeSelectionParams(
    const ImageCreateInfo& createInfo,
    bool                   isImageSpecial,
    uint32*                pRatioLow,
    uint32*                pRatioHi)
{
    const bool isBudgetPreferred = (createInfo.imageMemoryBudget >= 1.0);
    const bool computeMinSize    = isBudgetPreferred ||
                                   (isImageSpecial && (createInfo.tilingOptMode == TilingOptMode::OptForSpace));

    // Set ratioLow and ratioHi to get optimal swizzle mode among all valid modes based on calculated
    // surface size.
    // The logic is as belows:
    //      1. Traverse all valid swizzle modes, and assume mode i and j, with corresponding S_i and
    //         S_j as surface size of each other calculated from Addr3ComputeSurfaceInfo(xx)
    //              if S_j / S_i <= ratioLow / ratioHi
    //                    minSizeSwizzle = j
    //                    minSize        = S_j
    //      2. (Only for memoryBudget >= 1.0) Traverse all valid swizzle modes,
    //              if S_j / minSize > memoryBudget
    //                    disable swizzle mode j
    // In this way, the final minSizeSwizzle will be the optimal swizzle mode!

    *pRatioLow = 2;
    *pRatioHi  = 1;

    if (computeMinSize)
    {
        *pRatioLow = 1;
    }
    else if (isImageSpecial && (createInfo.tilingOptMode == TilingOptMode::Balanced))
    {
        *pRatioLow = 3;
        *pRatioHi  = 2;
    }

    return computeMinSize;
}

// =====================================================================================================================
Addr3SwizzleMode  AddrMgr3::SelectFinalSwizzleMode(
    const Image*                                   pImage,
    const SubResourceInfo*                         pBaseSubRes,
    const ADDR3_GET_POSSIBLE_SWIZZLE_MODE_INPUT*   pIn,
    ADDR3_GET_POSSIBLE_SWIZZLE_MODE_OUTPUT*        pPossibleSwizzles
    ) const
{
    const ImageCreateInfo&         createInfo              = pImage->GetImageCreateInfo();
    const ImageInternalCreateInfo& imageInternalCreateInfo = pImage->GetImageInfo().internalCreateInfo;

    Addr3SwizzleMode mode = ADDR3_LINEAR;

    const bool isNv12OrP010 = (pImage->GetGfxImage()->IsNv12OrP010FormatSurface());
    const bool isYuv        = (Formats::IsYuv(createInfo.swizzledFormat.format));

    // For the opening external shared images with flag 'useSharedTilingOverrides' set, we should use sharedSwizzleMode
    // directly.
    if (imageInternalCreateInfo.flags.useSharedTilingOverrides &&
        (imageInternalCreateInfo.gfx12.sharedSwizzleMode != ADDR3_MAX_TYPE))
    {
        mode = imageInternalCreateInfo.gfx12.sharedSwizzleMode;
    }
#if PAL_CLIENT_EXAMPLE
    else if (createInfo.flags.useFixedSwizzleMode)
    {
        mode = GetValidSwizzleMode(pPossibleSwizzles->validModes.value,
                                   GetAddrSwizzleMode(createInfo.fixedSwizzleMode));
    }
#endif
    else if ((createInfo.tiling == ImageTiling::Linear) || // Client requests linear.
        // Note: Most YUV-packed formats can be interpreted in a shader as having a different effective bits-per-pixel
        // than the YUV format actually has. This requires that we use linear tiling because the tile swizzle pattern
        // depends highly on the bits-per-pixel of the tiled Image. The exception is NV12/P010 format. This needs to
        // support tiling because NV12/P010 Images can be presentable for some API's, and the display hardware
        // requires tiling.
        // I suppost there's no harm to port GFX11 logic to GFX12 unless clarified further.
        // That's to say, YUV-packet format except NV12/P010 will be forced to be linear.
        (isYuv && (isNv12OrP010 == false))         ||
        // The following is from GFX10+ and I assume it applies to GFX12 despite the mentioned doc is gone.
        Formats::IsMacroPixelPackedRgbOnly(createInfo.swizzledFormat.format))
    {
        mode = GetValidSwizzleMode(pPossibleSwizzles->validModes.value, ADDR3_LINEAR);
    }
    else if (pImage->IsStencilPlane(pBaseSubRes->subresId.plane) && pImage->HasDepthPlane())
    {
        // Due to alignment reasons, the stencil plane and the depth plane have to share the same
        // swizzle mode.  This stencil plane has a depth plane, so just ensure that the previously
        // selected depth mode is valid for the stencil aspect as well and continue on.
        const SubresId  depthSubResId    = { 0, pBaseSubRes->subresId.mipLevel, pBaseSubRes->subresId.arraySlice };
        const auto*     pDepthSubResInfo = pImage->SubresourceInfo(depthSubResId);
        const uint32    depthSwizzle     = pImage->GetGfxImage()->GetSwTileMode(pDepthSubResInfo);

        mode = GetValidSwizzleMode(pPossibleSwizzles->validModes.value, static_cast<Addr3SwizzleMode>(depthSwizzle));
    }
    else
    {
        // If the client does not want to allow 256KB swizzle modes, then disable them here.
        // If the client sets this flag, then it's the address library's responsibility to set these bits
        // in the first place -- i.e., there's nothing to do in the "else" case.
        if (createInfo.flags.enable256KBSwizzleModes == 0)
        {
            pPossibleSwizzles->validModes.sw2d256kB = 0;
            pPossibleSwizzles->validModes.sw3d256kB = 0;
        }

        // Tiled resource must use 64KB block size for GFX12 PRTs.
        if (createInfo.flags.prt)
        {
            // 3D PRT should use 64KB_3D swizzle mode, even though it's BCn format, e.g.,
            // dEQP-VK.memory.requirements.extended.image.sparse_residency_aliased_tiling_optimal
            //
            // The addrlib input and the createInfo flags both have a "view3dAs2d" flag; they are not
            // the same.  If addrlib sees view3dAs2d==1, then it will not report any 3D swizzle modes.
            // We need to use the addrlib input here when chosing between 3D and 2D modes.
            if ((pIn->flags.view3dAs2dArray == 0) && (pIn->resourceType == AddrResourceType::ADDR_RSRC_TEX_3D)
#if ADDRLIB_VERSION < ADDRLIB_MAKE_VERSION(10, 1)
                // Addrlib 10.1 adds SW_64KB_3D to PRT BCn.
                // This BCn check is needed for back-compability. It guarantees that when building w/ addrlib < 10.1,
                // the 3D PRT BCn images will be choosing 64KB_2D swizzle mode.
                // W/o this back-compa, PAL w/ addrlib < 9.12 opts to select 64KB_3D while addrlib reports no support
                // for 64KB_3D, which results in crash.
                && (Formats::IsBlockCompressed(createInfo.swizzledFormat.format) == false)
#endif
                )
            {
                mode = GetValidSwizzleMode(pPossibleSwizzles->validModes.value, ADDR3_64KB_3D);
            }
            // All other PRTs should use ADDR3_64KB_2D
            else
            {
                mode = GetValidSwizzleMode(pPossibleSwizzles->validModes.value, ADDR3_64KB_2D);
            }

            // Depth/stencil PRTs should not be supported. We may need to re-evaluate the swizzle mode determination
            // in the event that this changes.
            PAL_ASSERT(TestAnyFlagSet(Gfx12::PrtFeatures, PrtFeatureImageDepthStencil) == false);
        }
        else if (isNv12OrP010 && (createInfo.tiling == ImageTiling::Optimal))
        {
            // It's hard to move this check to addrlib as it's inside the imageTiling optimal...
            if (createInfo.flags.videoReferenceOnly)
            {
                mode = GetValidSwizzleMode(pPossibleSwizzles->validModes.value, ADDR3_256B_2D);
            }
            else
            {
                // GFX11 is ADDR_SW_64KB_D and it's ADDR3_64KB_2D for GFX12 and GFX12 nv12/p010 don't support 3D
                // swizzle mode so we restrict it as below.
                mode = GetValidSwizzleMode(pPossibleSwizzles->validModes.value, ADDR3_64KB_2D);
            }
        }
        else
        {
            // We'll have a loop over all valid swizzle modes to find the optimal one as output...
            ADDR_E_RETURNCODE addrRet = ADDR_OK;

            // We've two or more valid swizzle modes, so need to determine which is preferred.
            if (IsPowerOfTwo(pPossibleSwizzles->validModes.value) == false)
            {
                uint32 ratioLow;
                uint32 ratioHi;

                // Copied from GFX11 and assume it applies to GFX12.
                // For two cases we need to change the ratio values
                //     1. No shared surfaces otherwise the tiling mode is already defined.
                //     2. Not NV12 or PO10, since they only support 2D THIN1 or linear tile mode and setting
                //        the ratio for those surfaces could change the tile mode to 1D THIN1.
                const bool isImageSpecial = ((pImage->IsShared() == false) &&
                                             (pIn->flags.nv12 == 0)        &&
                                             (pIn->flags.p010 == 0));
                const bool computeMinSize =
                    GetSwizzleModeSelectionParams(createInfo, isImageSpecial, &ratioLow, &ratioHi);

                if ((pIn->height > 1) && (computeMinSize == false))
                {
                    // Always ignore linear swizzle mode if:
                    // 1. This is a (2D/3D) resource with height > 1
                    // 2. Client doesn't require computing minimize size
                    pPossibleSwizzles->validModes.swLinear = 0;
                }

                // Determine swizzle mode if there are 2 or more swizzle mode candidates
                if (IsPowerOfTwo(pPossibleSwizzles->validModes.value) == false)
                {
                    ADDR3_COMPUTE_SURFACE_INFO_INPUT localIn = {};

                    localIn.flags        = pIn->flags;
                    localIn.resourceType = pIn->resourceType;
                    localIn.format       = Image::GetAddrFormat(pBaseSubRes->format.format);
                    localIn.width        = pIn->width;
                    localIn.height       = pIn->height;
                    localIn.bpp          = pIn->bpp;
                    localIn.numSlices    = Max(pIn->numSlices, 1u);
                    localIn.numMipLevels = Max(pIn->numMipLevels, 1u);
                    localIn.numSamples   = Max(pIn->numSamples, 1u);

                    uint64 padSize[ADDR3_MAX_TYPE] = {};

                    uint32 minSizeSwizzle = static_cast<uint32>(ADDR3_LINEAR);
                    uint32 minSize        = 0;

                    ADDR3_COMPUTE_SURFACE_INFO_OUTPUT localOut = {};

                    for (uint32 i = ADDR3_LINEAR; i < ADDR3_MAX_TYPE; i++)
                    {
                        if (TestAnyFlagSet(pPossibleSwizzles->validModes.value, (1u << i)))
                        {
                            localIn.swizzleMode = static_cast<Addr3SwizzleMode>(i);

                            addrRet = Addr3ComputeSurfaceInfo(AddrLibHandle(), &localIn, &localOut);

                            if (addrRet == ADDR_OK)
                            {
                                padSize[i] = localOut.surfSize;

                                if ((minSize == 0) ||
                                    SwizzleTypeWithinMemoryBudget(minSize, padSize[i], ratioLow, ratioHi))
                                {
                                    minSize        = padSize[i];
                                    minSizeSwizzle = i;
                                }
                            }
                            else
                            {
                                PAL_ASSERT_ALWAYS();
                                break;
                            }
                        }
                    }

                    if (createInfo.imageMemoryBudget > 1.0)
                    {
                        for (uint32 i = ADDR3_256B_2D; i < ADDR3_MAX_TYPE; i++)
                        {
                            if ((i != minSizeSwizzle) &&
                                TestAnyFlagSet(pPossibleSwizzles->validModes.value, (1u << i)))
                            {
                                if (SwizzleTypeWithinMemoryBudget(minSize, padSize[i], 0, 0,
                                                                  createInfo.imageMemoryBudget) == false)
                                {
                                    // Clear the swizzle type if the memory waste is unacceptable
                                    pPossibleSwizzles->validModes.value &= ~(1u << i);
                                }
                            }
                        }

                        // Remove linear swizzle type if 2 or more swizzle types are allowed
                        if (IsPowerOfTwo(pPossibleSwizzles->validModes.value) == false)
                        {
                            pPossibleSwizzles->validModes.swLinear = 0;
                        }

                        // Select the biggest allowed swizzle mode
                        minSizeSwizzle = Log2(pPossibleSwizzles->validModes.value);
                    }

                    pPossibleSwizzles->validModes.value &= (1u << minSizeSwizzle);
                }
            }

            // Determine swizzle mode now. Always select the "largest" swizzle mode.
            mode = static_cast<Addr3SwizzleMode>(Log2(pPossibleSwizzles->validModes.value));
        }
    }

    PAL_ASSERT(uint32(mode) < ADDR3_MAX_TYPE);

    return mode;
}

// =====================================================================================================================
// Computes the size (in PRT tiles) of the mip tail for a particular Image plane.
void AddrMgr3::ComputeTilesInMipTail(
    const Image&       image,
    uint32             plane,
    ImageMemoryLayout* pGpuMemLayout) const
{
    const ImageCreateInfo& createInfo = image.GetImageCreateInfo();
    // This function is only supposed to be called for PRT Images which have a mip tail.
    PAL_ASSERT((createInfo.flags.prt != 0) && (pGpuMemLayout->prtMinPackedLod < createInfo.mipLevels));

    // AddrMgr3 only supports GPU's whose tiling has a single mip tail per array slice.
    const auto& imageProperties = GetDevice()->ChipProperties().imageProperties;
    PAL_ASSERT((imageProperties.prtFeatures & PrtFeaturePerSliceMipTail) != 0);

    // 3D image may need one more tiles for mip tail considering depth.
    if (createInfo.imageType == ImageType::Tex3d)
    {
        const SubresId subresId = Subres(plane, pGpuMemLayout->prtMinPackedLod, 0);
        const SubResourceInfo*const pSubResInfo = image.SubresourceInfo(subresId);

        pGpuMemLayout->prtMipTailTileCount =
            uint32(RoundUpQuotient(pSubResInfo->extentElements.depth, pGpuMemLayout->prtTileDepth));
    }
    else
    {
        // The GPU addressing document states that if a mip tail is present, it is always exactly one tile block per
        // array slice.
        pGpuMemLayout->prtMipTailTileCount = 1;
    }
}

// =====================================================================================================================
// Computes the swizzling mode for all subresources for the plane associated with the specified subresource.
Result AddrMgr3::ComputePlaneSwizzleMode(
    const Image*           pImage,
    const SubResourceInfo* pBaseSubRes,
    bool                   forFmask,
    Addr3SwizzleMode*      pFinalMode
    ) const
{
    Result result = Result::ErrorUnknown;

    const ImageCreateInfo& createInfo = pImage->GetImageCreateInfo();

    // Standard swizzle modes should not be supported by AddrMgr3
    PAL_ASSERT(createInfo.tiling != ImageTiling::Standard64Kb);

    ADDR3_GET_POSSIBLE_SWIZZLE_MODE_INPUT input = { };
    input.size          = sizeof(input);
    input.flags         = DetermineSurfaceFlags(*pImage, pBaseSubRes->subresId.plane);
    input.resourceType  = GetAddrResourceType(createInfo.imageType);
    input.bpp           = Formats::BitsPerPixel(pBaseSubRes->format.format);
    input.width         = createInfo.extent.width;
    input.height        = createInfo.extent.height;
    input.numSlices     = ((createInfo.imageType != ImageType::Tex3d) ? createInfo.arraySize : createInfo.extent.depth);
    input.numMipLevels  = createInfo.mipLevels;
    input.numSamples    = createInfo.samples;
    input.maxAlign      = (createInfo.maxBaseAlign > 0) ? createInfo.maxBaseAlign : UINT32_MAX;

    ADDR3_GET_POSSIBLE_SWIZZLE_MODE_OUTPUT validSwizzles = { .size = sizeof(ADDR3_GET_POSSIBLE_SWIZZLE_MODE_OUTPUT) };

    ADDR_E_RETURNCODE addrRet = Addr3GetPossibleSwizzleModes(AddrLibHandle(), &input, &validSwizzles);

    if (addrRet == ADDR_OK)
    {
        const uint32 validSwizzleMask = validSwizzles.validModes.value;
        const uint32 userSwizzleMask  = m_pDevice->Settings().addr3SelectSwizzleModes;
        if (TestAnyFlagSet(validSwizzleMask, userSwizzleMask))
        {
            validSwizzles.validModes.value &= userSwizzleMask;
        }
        else
        {
            PAL_ALERT_MSG(
                true,
                "User-specified swizzle mask (0x%X) is incompatible with valid swizzle modes (0x%X) for this surface!",
                userSwizzleMask,
                validSwizzleMask);
        }

        *pFinalMode = SelectFinalSwizzleMode(pImage, pBaseSubRes, &input, &validSwizzles);

        if (*pFinalMode != ADDR3_MAX_TYPE)
        {
            result = Result::Success;
        }
    }

    return result;
}

// =====================================================================================================================
// Computes the swizzling mode for HiZ/HiS associated with the specified image.
Result AddrMgr3::ComputeHiSZSwizzleMode(
    const Image&      image,
    const Extent3d&   hiSZExtent,
    ChNumFormat       hiSZFormat,
    bool              isHiZ,       // If compute swizzle mode for HiZ or HiS.
    Addr3SwizzleMode* pFinalMode
    ) const
{
    Result result = Result::ErrorUnknown;

#if PAL_BUILD_GFX12
    // The following hiZ/hiS are valid only for gfx12.
    PAL_ASSERT(IsGfx12(*m_pDevice));
    const ImageInternalCreateInfo& internalCrInfo = image.GetImageInfo().internalCreateInfo;

    if (internalCrInfo.flags.useSharedMetadata)
    {
        *pFinalMode = isHiZ ? internalCrInfo.sharedMetadata.hiZSwizzleMode
                            : internalCrInfo.sharedMetadata.hiSSwizzleMode;
        result = Result::Success;
    }
    else
    {
        const ImageCreateInfo& createInfo = image.GetImageCreateInfo();

        ADDR3_GET_POSSIBLE_SWIZZLE_MODE_INPUT  input  = {};
        ADDR3_GET_POSSIBLE_SWIZZLE_MODE_OUTPUT output = {};

        input.size         = sizeof(input);
        input.bpp          = Formats::BitsPerPixel(hiSZFormat);
        input.width        = hiSZExtent.width;
        input.height       = hiSZExtent.height;
        input.numSlices    = createInfo.arraySize;
        input.numMipLevels = createInfo.mipLevels;
        input.numSamples   = createInfo.fragments;
        input.resourceType = GetAddrResourceType(createInfo.imageType);
        input.flags.hiZHiS = 1;
        input.maxAlign     = (createInfo.maxBaseAlign > 0) ? createInfo.maxBaseAlign : UINT32_MAX;

        ADDR_E_RETURNCODE addrRet = Addr3GetPossibleSwizzleModes(AddrLibHandle(), &input, &output);

        // Below swizzle mode selection logic is referenced from SelectFinalSwizzleMode().
        if (addrRet == ADDR_OK)
        {
            // HiZ/HiS only allows swizzle modes: 256B_2D, 4KB_2D, 64KB_2D and 256KB_2D.
            constexpr uint32 HiSZValidSwizzleModeMin = uint32(ADDR3_256B_2D);
            constexpr uint32 HiSZValidSwizzleModeMax = uint32(ADDR3_256KB_2D);

            // We've two or more valid swizzle modes, so need to determine which is preferred.
            if (IsPowerOfTwo(output.validModes.value) == false)
            {
                uint32 ratioLow;
                uint32 ratioHi;
                GetSwizzleModeSelectionParams(createInfo, true, &ratioLow, &ratioHi);

                uint64 padSize[ADDR3_MAX_TYPE] = {};
                uint32 minSizeSwizzle          = HiSZValidSwizzleModeMin;
                uint32 minSize                 = 0;

                ADDR3_COMPUTE_SURFACE_INFO_OUTPUT localOut = {};

                for (uint32 i = HiSZValidSwizzleModeMin; i <= HiSZValidSwizzleModeMax; i++)
                {
                    if (TestAnyFlagSet(output.validModes.value, (1u << i)))
                    {
                        result = ComputeHiSZInfo(image, hiSZExtent, hiSZFormat, Addr3SwizzleMode(i), &localOut);

                        if (result == Result::Success)
                        {
                            padSize[i] = localOut.surfSize;

                            if ((minSize == 0) || SwizzleTypeWithinMemoryBudget(minSize, padSize[i], ratioLow, ratioHi))
                            {
                                minSize        = padSize[i];
                                minSizeSwizzle = i;
                            }
                        }
                        else
                        {
                            PAL_ASSERT_ALWAYS();
                            break;
                        }
                    }
                }

                if (createInfo.imageMemoryBudget > 1.0)
                {
                    for (uint32 i = HiSZValidSwizzleModeMin; i <= HiSZValidSwizzleModeMax; i++)
                    {
                        if ((i != minSizeSwizzle) && TestAnyFlagSet(output.validModes.value, (1u << i)))
                        {
                            if (SwizzleTypeWithinMemoryBudget(minSize, padSize[i], 0, 0,
                                                              createInfo.imageMemoryBudget) == false)
                            {
                                // Clear the swizzle type if the memory waste is unacceptable
                                output.validModes.value &= ~(1u << i);
                            }
                        }
                    }

                    // Select the biggest allowed swizzle mode
                    minSizeSwizzle = Log2(output.validModes.value);
                }

                *pFinalMode = Addr3SwizzleMode(minSizeSwizzle);
            }
            else
            {
                result      = Result::Success;
                *pFinalMode = Addr3SwizzleMode(Log2(output.validModes.value));
            }

            PAL_ASSERT((*pFinalMode >= HiSZValidSwizzleModeMin) && (*pFinalMode <= HiSZValidSwizzleModeMax));
        }
    }
#endif

    return result;
}

// =====================================================================================================================
Result AddrMgr3::ComputeHiSZInfo(
    const Image&                       image,
    const Extent3d&                    hiSZExtent,
    ChNumFormat                        hiSZFormat,
    Addr3SwizzleMode                   hiSZSwizzleMode,
    ADDR3_COMPUTE_SURFACE_INFO_OUTPUT* pOut
    ) const
{

    Result result = Result::ErrorUnknown;

    const ImageCreateInfo& createInfo = image.GetImageCreateInfo();

    ADDR3_COMPUTE_SURFACE_INFO_INPUT surfInfoIn = { };
    surfInfoIn.size          = sizeof(surfInfoIn);
    surfInfoIn.width         = hiSZExtent.width;
    surfInfoIn.height        = hiSZExtent.height;
    surfInfoIn.resourceType  = GetAddrResourceType(createInfo.imageType);
    surfInfoIn.bpp           = Formats::BitsPerPixel(hiSZFormat);
    surfInfoIn.numSlices     = createInfo.arraySize;
    surfInfoIn.numMipLevels  = createInfo.mipLevels;
    surfInfoIn.numSamples    = createInfo.fragments;
    surfInfoIn.swizzleMode   = hiSZSwizzleMode;

    ADDR_E_RETURNCODE addrRet = Addr3ComputeSurfaceInfo(AddrLibHandle(), &surfInfoIn, pOut);

    if (addrRet == ADDR_OK)
    {
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
AddrResourceType AddrMgr3::GetAddrResourceType(
    ImageType imageType)
{
    static_assert(
        (EnumSameVal(AddrResourceType::ADDR_RSRC_TEX_1D, ImageType::Tex1d) &&
         EnumSameVal(AddrResourceType::ADDR_RSRC_TEX_2D, ImageType::Tex2d) &&
         EnumSameVal(AddrResourceType::ADDR_RSRC_TEX_3D, ImageType::Tex3d)),
        "Resource type enumerations don't match between PAL and addrlib!");

    return static_cast<AddrResourceType>(imageType);
}

// =====================================================================================================================
Addr3SwizzleMode AddrMgr3::GetAddrSwizzleMode(
    SwizzleMode swMode)
{
    // Lookup table for converting between SwizzleMode enums and Addr3SwizzleMode enums.
    constexpr Addr3SwizzleMode AddrSwizzles[] =
    {
        ADDR3_LINEAR,   // SwizzleModeLinear
        ADDR3_MAX_TYPE, // SwizzleMode256BS
        ADDR3_MAX_TYPE, // SwizzleMode256BD
        ADDR3_MAX_TYPE, // SwizzleMode256BR
        ADDR3_MAX_TYPE, // SwizzleMode4KbZ
        ADDR3_MAX_TYPE, // SwizzleMode4KbS
        ADDR3_MAX_TYPE, // SwizzleMode4KbD
        ADDR3_MAX_TYPE, // SwizzleMode4KbR
        ADDR3_MAX_TYPE, // SwizzleMode64KbZ
        ADDR3_MAX_TYPE, // SwizzleMode64KbS
        ADDR3_MAX_TYPE, // SwizzleMode64KbD
        ADDR3_MAX_TYPE, // SwizzleMode64KbR
        ADDR3_MAX_TYPE, // SwizzleMode64KbZT
        ADDR3_MAX_TYPE, // SwizzleMode64KbST
        ADDR3_MAX_TYPE, // SwizzleMode64KbDT
        ADDR3_MAX_TYPE, // SwizzleMode64KbRT
        ADDR3_MAX_TYPE, // SwizzleMode4KbZX
        ADDR3_MAX_TYPE, // SwizzleMode4KbSX
        ADDR3_MAX_TYPE, // SwizzleMode4KbDX
        ADDR3_MAX_TYPE, // SwizzleMode4KbRX
        ADDR3_MAX_TYPE, // SwizzleMode64KbZX
        ADDR3_MAX_TYPE, // SwizzleMode64KbSX
        ADDR3_MAX_TYPE, // SwizzleMode64KbDX
        ADDR3_MAX_TYPE, // SwizzleMode64KbRX
        ADDR3_MAX_TYPE, // SwizzleMode256KbVarZX
        ADDR3_MAX_TYPE, // SwizzleMode256KbVarSX
        ADDR3_MAX_TYPE, // SwizzleMode256KbVarDX
        ADDR3_MAX_TYPE, // SwizzleMode256KbVarRX
        ADDR3_256B_2D,  // SwizzleMode256B2D
        ADDR3_4KB_2D,   // SwizzleMode4Kb2D
        ADDR3_4KB_3D,   // SwizzleMode4Kb3D
        ADDR3_64KB_2D,  // SwizzleMode64Kb2D
        ADDR3_64KB_3D,  // SwizzleMode64Kb3D
        ADDR3_256KB_2D, // SwizzleMode256Kb2D
        ADDR3_256KB_3D, // SwizzleMode256Kb3D
        ADDR3_MAX_TYPE, // SwizzleMode64Kb2Dz
        ADDR3_MAX_TYPE, // SwizzleMode256Kb2Dz
    };

    static_assert(ArrayLen(AddrSwizzles) == SwizzleModeCount);
    PAL_ASSERT(swMode < SwizzleModeCount);

    return AddrSwizzles[uint32(swMode)];
}

// =====================================================================================================================
// Initializes all subresources for an Image object.
Result AddrMgr3::InitSubresourcesForImage(
    Image*             pImage,
    gpusize*           pGpuMemSize,
    gpusize*           pGpuMemAlignment,
    ImageMemoryLayout* pGpuMemLayout,
    SubResourceInfo*   pSubResInfoList,
    void*              pSubResTileInfoList,
    bool*              pDccUnsupported
    ) const
{
    // For AddrMgr3 style addressing, there's no chance of a single subresource being incapable of supporting DCC.
    *pDccUnsupported = false;

    Result result = Result::Success;

    const ImageCreateInfo& createInfo = pImage->GetImageCreateInfo();
    const ImageInfo&       imageInfo  = pImage->GetImageInfo();

    const uint32 subResourcesPerPlane = (createInfo.mipLevels * createInfo.arraySize);
    for (uint32 plane = 0; plane < imageInfo.numPlanes; ++plane)
    {
        // Base subresource for the current plane:
        SubResourceInfo* const pBaseSubRes    = (pSubResInfoList + (plane * subResourcesPerPlane));
        TileInfo* const        pBaseTileInfo  = NonConstTileInfo(pSubResTileInfoList, (plane * subResourcesPerPlane));
        ADDR3_COMPUTE_SURFACE_INFO_OUTPUT surfInfoOut                = {};
        ADDR3_MIP_INFO                    mipInfo[MaxImageMipLevels] = {};
        Addr3SwizzleMode                  finalSwizzle               = ADDR3_LINEAR;
        ADDR_QBSTEREOINFO                 addrStereoInfo             = { };

        surfInfoOut.size     = sizeof(ADDR3_COMPUTE_SURFACE_INFO_OUTPUT);
        surfInfoOut.pMipInfo = &mipInfo[0];
        result = ComputePlaneSwizzleMode(pImage, pBaseSubRes, false, &finalSwizzle);
        if (result == Result::Success)
        {
            surfInfoOut.pStereoInfo = &addrStereoInfo;

            // Use AddrLib to compute the padded and aligned dimensions of the entire mip-chain.
            result = ComputeAlignedPlaneDimensions(pImage,
                                                   pBaseSubRes,
                                                   pBaseTileInfo,
                                                   finalSwizzle,
                                                   &surfInfoOut);
        }

        if (createInfo.flags.stereo == 1)
        {
            const uint32 tileSwizzleRight = surfInfoOut.pStereoInfo->rightSwizzle << 8;

            pGpuMemLayout->stereoLineOffset       = surfInfoOut.pStereoInfo->eyeHeight;
            pSubResInfoList->extentTexels.height += pGpuMemLayout->stereoLineOffset;
            pSubResInfoList->stereoLineOffset     = pGpuMemLayout->stereoLineOffset;
            pSubResInfoList->stereoOffset         = (surfInfoOut.pStereoInfo->rightOffset | tileSwizzleRight);
        }

        if (result == Result::Success)
        {
            if (plane == 0)
            {
                pGpuMemLayout->prtTileWidth  = surfInfoOut.blockExtent.width;
                pGpuMemLayout->prtTileHeight = surfInfoOut.blockExtent.height;
                pGpuMemLayout->prtTileDepth  = surfInfoOut.blockExtent.depth;
            }

            pBaseTileInfo->mip0InMipTail = (surfInfoOut.mipChainInTail != 0);
            pBaseTileInfo->mipTailMask   = ((surfInfoOut.bpp / 8) * surfInfoOut.blockExtent.width *
                                            surfInfoOut.blockExtent.height * surfInfoOut.blockExtent.depth) - 1;

            result = pImage->GetGfxImage()->Addr3FinalizePlane(pBaseSubRes,
                                                               pBaseTileInfo,
                                                               finalSwizzle,
                                                               surfInfoOut);
        }

        if (result == Result::Success)
        {
            SubresId subRes = pBaseSubRes->subresId;
            for (subRes.mipLevel = 0; subRes.mipLevel < createInfo.mipLevels; ++subRes.mipLevel)
            {
                for (subRes.arraySlice = 0; subRes.arraySlice < createInfo.arraySize; ++subRes.arraySlice)
                {
                    const uint32          subResIdx = pImage->CalcSubresourceId(subRes);
                    SubResourceInfo*const pSubRes   = (pSubResInfoList + subResIdx);
                    TileInfo*const        pTileInfo = NonConstTileInfo(pSubResTileInfoList, subResIdx);

                    // Each subresource in the plane uses the same tiling info as the base subresource.
                    *pTileInfo = *pBaseTileInfo;

                    pTileInfo->swizzleMode = finalSwizzle;

                    result = InitSubresourceInfo(pImage, pSubRes, pTileInfo, pTileInfo->swizzleMode, surfInfoOut);
                    if (result != Result::Success)
                    {
                        PAL_ALERT_ALWAYS();
                        break;
                    }
                } // End loop over slices

                // Stop initialize next mipLevel since error occurs
                if (result != Result::Success)
                {
                    break;
                }

                // Update the memory layout's swizzle equation information. These propagate down from index 0 to index
                // 1 so this check should skip this logic once we're found both swizzle equations.
                subRes.arraySlice = 0;
                const SubResourceInfo*const pSubResSlice0 = pImage->SubresourceInfo(subRes);
                // Use eqIdx already set by InitSubresourceInfo().
                const uint8 eqIdx = pSubResSlice0->swizzleEqIndex;
                if ((pGpuMemLayout->swizzleEqIndices[1] != eqIdx) &&
                    // Don't give the caller the swizzle equations unless they've actually been requested.  Giving
                    // DX unrequested swizzle equations causes them to believe that they did request swizzle eqs,
                    // which causes all kinds of bizarre side effects, including requesting tile-swizzles for surfaces
                    // that don't support them.
                    (createInfo.flags.preferSwizzleEqs || createInfo.flags.needSwizzleEqs))
                {
                    if (pGpuMemLayout->swizzleEqIndices[0] == InvalidSwizzleEqIndex)
                    {
                        // We set both indices because they must both be valid even if the image only uses one.
                        pGpuMemLayout->swizzleEqIndices[0] = eqIdx;
                        pGpuMemLayout->swizzleEqIndices[1] = eqIdx;
                    }
                    else if (pGpuMemLayout->swizzleEqIndices[0] == pGpuMemLayout->swizzleEqIndices[1])
                    {
                        // We've just transitioned to the second swizzle index.
                        pGpuMemLayout->swizzleEqIndices[1]    = eqIdx;

                        // The transition cound happen either between two mip levels, or between two planes.
                        if ((pImage->GetImageInfo().numPlanes > 1) && (subRes.plane != 0))
                        {
                            pGpuMemLayout->swizzleEqTransitionPlane = static_cast<uint8>(subRes.plane);
                        }
                        else
                        {
                            pGpuMemLayout->swizzleEqTransitionMip = static_cast<uint8>(subRes.mipLevel);
                        }
                    }
                    else
                    {
                        // We found an unexpected third swizzle index.
                        PAL_ASSERT(pGpuMemLayout->swizzleEqIndices[0] == eqIdx);
                    }
                }
            } // End loop over mip levels

            // Update the Image's alignment. We will update the GPU memory size in the loop at the end of this method.
            *pGpuMemAlignment = Max<gpusize>(*pGpuMemAlignment, surfInfoOut.baseAlign);

            // The loop below will work through each sub-resource to calculate its offset and maintain a running total
            // of the image size that is ultimately reported back to the caller.  Adress library considers one slice
            // to be:
            //      a) A single slice of a 2D array.  This is good as it matches the PAL definition of a slice.
            //      b) A single slice of a 3D volume.  This is bad as PAL considers one slice of a volume to be all
            //         the slices.
            //
            // Calculate the number of slices that the address-library "sees" so that the slice size (should) match the
            // reported surface size.
            const uint32 numSlices = ((createInfo.imageType == ImageType::Tex3d)
                                      ? GetNumAddrLib3dSlices(pImage, finalSwizzle, surfInfoOut)
                                      : createInfo.arraySize);

            PAL_ASSERT(surfInfoOut.surfSize == (surfInfoOut.sliceSize * numSlices));
        }

        // Stop initialize next plane since error occurs
        if (result != Result::Success)
        {
            break;
        }

    } // End loop over planes

    // Depth/stencil and YUV images have different orderings of subresources and planes. To handle this, we'll loop
    // through again to compute the final offsets for each subresource.
    if (result == Result::Success)
    {
        // Depth/stencil and YUV images have different orderings of subresources and planes. To handle this, we'll
        // loop through again to compute the final offsets for each subresource.
        //
        // This loops through all the slices of a mip level first before incrementing the mip-level part of
        // the subresId.
        SubResIterator subResIt(*pImage);
        do
        {
            pImage->GetGfxImage()->Addr3InitSubResInfo(subResIt,
                                                       pSubResInfoList,
                                                       pSubResTileInfoList,
                                                       pGpuMemSize);
            SubResourceInfo* const pSubResInfo = (pSubResInfoList + subResIt.Index());
            const auto* pGfxImage              = pImage->GetGfxImage();
            const auto  swizzleMode            = static_cast<Addr3SwizzleMode>(pGfxImage->GetSwTileMode(pSubResInfo));

            // For linear modes or with on-mipmap or non 2d and non arrayed textures, the swizzleOffset is the same as
            // mem offset.
            if (IsLinearSwizzleMode(swizzleMode) ||
                (createInfo.mipLevels == 1)      ||
                ((createInfo.imageType != Pal::ImageType::Tex2d) && (createInfo.arraySize == 1)))
            {
                pSubResInfo->swizzleOffset = pSubResInfo->offset;
            }
        } while (subResIt.Next());
    }

    return result;
}

// =====================================================================================================================
// Computes the padded dimensions for all subresources for the plane associated with the specified subresource.
Result AddrMgr3::ComputeAlignedPlaneDimensions(
    Image*                             pImage,
    SubResourceInfo*                   pBaseSubRes,     // Base subresource for the plane
    TileInfo*                          pBaseTileInfo,   // Base subresource tiling info for the plane
    Addr3SwizzleMode                   swizzleMode,
    ADDR3_COMPUTE_SURFACE_INFO_OUTPUT* pOut
    ) const
{
    PAL_ASSERT((pBaseSubRes->subresId.mipLevel == 0) && (pBaseSubRes->subresId.arraySlice == 0));

    Result result = Result::ErrorUnknown;

    const ImageCreateInfo& createInfo = pImage->GetImageCreateInfo();
    const ImageInfo&       imageInfo  = pImage->GetImageInfo();

    ADDR3_COMPUTE_SURFACE_INFO_INPUT surfInfoIn = { };
    surfInfoIn.size         = sizeof(surfInfoIn);
    surfInfoIn.flags        = DetermineSurfaceFlags(*pImage, pBaseSubRes->subresId.plane);
    surfInfoIn.swizzleMode  = swizzleMode;
    surfInfoIn.resourceType = GetAddrResourceType(createInfo.imageType);
    surfInfoIn.format       = Image::GetAddrFormat(pBaseSubRes->format.format);
    surfInfoIn.bpp          = Formats::BitsPerPixel(pBaseSubRes->format.format);
    surfInfoIn.width        = pBaseSubRes->extentTexels.width;
    surfInfoIn.height       = pBaseSubRes->extentTexels.height;
    surfInfoIn.numSlices    = ((createInfo.imageType != ImageType::Tex3d) ? createInfo.arraySize
                                                                          : createInfo.extent.depth);
    surfInfoIn.numMipLevels = createInfo.mipLevels;
    surfInfoIn.numSamples   = createInfo.samples;

    if ((createInfo.rowPitch > 0) && (createInfo.depthPitch > 0))
    {
        // We must convert our byte pitches into units of elements. For most formats (including BC formats) the subresource
        // bitsPerTexel is already the size of an element. The exception is 96-bit formats which have three 32-bit element
        // per texel.
        const uint32 bytesPerElement = CalcBytesPerElement(pBaseSubRes);

        PAL_ASSERT((createInfo.rowPitch % bytesPerElement) == 0);

        surfInfoIn.pitchInElement = createInfo.rowPitch / bytesPerElement;
        surfInfoIn.sliceAlign     = createInfo.depthPitch;
    }

    ADDR_E_RETURNCODE addrRet = Addr3ComputeSurfaceInfo(AddrLibHandle(), &surfInfoIn, pOut);
    if (addrRet == ADDR_OK)
    {
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Initialize the information for a single subresource given the properties of its plane (as computed by
// AddrLib).
Result AddrMgr3::InitSubresourceInfo(
    Image*                                    pImage,
    SubResourceInfo*                          pSubResInfo,
    TileInfo*                                 pTileInfo,
    Addr3SwizzleMode                          swizzleMode,
    const ADDR3_COMPUTE_SURFACE_INFO_OUTPUT&  surfaceInfo
    ) const
{
    Result  result = Result::Success;

    const ImageCreateInfo& createInfo = pImage->GetImageCreateInfo();
    const ADDR3_MIP_INFO&  mipInfo    = surfaceInfo.pMipInfo[pSubResInfo->subresId.mipLevel];

    // The actual element extents come directly from AddrLib.
    pSubResInfo->actualExtentElements.width  = mipInfo.pitch;
    pSubResInfo->actualExtentElements.height = mipInfo.height;
    pSubResInfo->actualExtentElements.depth  = mipInfo.depth;
    pSubResInfo->mipTailCoord.x              = mipInfo.mipTailCoordX;
    pSubResInfo->mipTailCoord.y              = mipInfo.mipTailCoordY;
    pSubResInfo->mipTailCoord.z              = mipInfo.mipTailCoordZ;

    // AddrLib doesn't tell us the values for extentElements or actualExtentTexels so we must compute them ourselves.
    // It also doesn't tell us the exact ratios between texels and elements but we can compute them from the pitch
    // and height data which is returned in both texels and elements.
    if (surfaceInfo.pixelPitch >= surfaceInfo.pitch)
    {
        const uint32 texelsPerElem = (surfaceInfo.pixelPitch / surfaceInfo.pitch);

        // We must round to the nearest element because the caller is not required to pad the texel extent.
        pSubResInfo->extentElements.width     = RoundUpQuotient(pSubResInfo->extentTexels.width, texelsPerElem);
        pSubResInfo->actualExtentTexels.width = (pSubResInfo->actualExtentElements.width * texelsPerElem);
    }
    else
    {
        const uint32 elemsPerTexel = (surfaceInfo.pitch / surfaceInfo.pixelPitch);

        pSubResInfo->extentElements.width     = (pSubResInfo->extentTexels.width         * elemsPerTexel);
        pSubResInfo->actualExtentTexels.width = (pSubResInfo->actualExtentElements.width / elemsPerTexel);
    }

    if (surfaceInfo.pixelHeight >= surfaceInfo.height)
    {
        const uint32 texelsPerElem = (surfaceInfo.pixelHeight / surfaceInfo.height);

        // We must round to the nearest element because the caller is not required to pad the texel extent.
        pSubResInfo->extentElements.height     = RoundUpQuotient(pSubResInfo->extentTexels.height, texelsPerElem);
        pSubResInfo->actualExtentTexels.height = (pSubResInfo->actualExtentElements.height * texelsPerElem);
    }
    else
    {
        const uint32 elemsPerTexel = (surfaceInfo.height / surfaceInfo.pixelHeight);

        pSubResInfo->extentElements.height     = (pSubResInfo->extentTexels.height         * elemsPerTexel);
        pSubResInfo->actualExtentTexels.height = (pSubResInfo->actualExtentElements.height / elemsPerTexel);
    }

    // The depth values are always equal.
    pSubResInfo->extentElements.depth     = pSubResInfo->extentTexels.depth;
    pSubResInfo->actualExtentTexels.depth = pSubResInfo->actualExtentElements.depth;
    pSubResInfo->actualArraySize          = createInfo.arraySize;

    // Finish with the subresource's memory layout data.
    pSubResInfo->baseAlign = surfaceInfo.baseAlign;

    // Note that because the mipmap levels in an array slice are tightly packed, the size of a single subresource
    // is a somewhat meaningless quantity for AddrMgr3. Just use the whole array slice's size for each subresource,
    // even though this isn't accurate.
    //
    // From the address library's perspective, one "slice" is either one slice of a 2D array or one slice of a volume
    // texture.  From PAL's perspective, one sub-resource of a 2D array is one slice...  However, we consider one-sub
    // resource of a volume texture to be the entire thing.  Further complicating things is that, due to padding
    // requirements, the number of slices in a 3D image can be far larger than the number requested.
    pSubResInfo->size = surfaceInfo.sliceSize *
                        ((createInfo.imageType == ImageType::Tex3d)
                            ? GetNumAddrLib3dSlices(pImage, swizzleMode, surfaceInfo)
                            : 1);

    if (pImage->GetImageCreateInfo().flags.stereo == 1)
    {
        pSubResInfo->size = surfaceInfo.surfSize;
    }

    // Compute the exact row pitch in bytes. This math must be done in terms of elements instead of texels
    // because some formats (e.g., R32G32B32) have pitches that are not multiples of their texel size.
    // GFX10+ devices and linear images do not have tightly packed mipmap levels, so the rowPitch
    // of a subresource is the size in bytes of one row of that subresource.
    pSubResInfo->rowPitch = (pSubResInfo->actualExtentElements.width * (surfaceInfo.bpp >> 3));

    // The depth pitch is a constant for each plane.  This is the number of bytes it takes to get to the next
    // slice of any given mip-level (i.e., each slice has the exact same layout).
    pSubResInfo->depthPitch = surfaceInfo.sliceSize;

    // Note: The full offset to this subresource will be computed later. For now, just set it to the offset of
    // the mipmap level within the current array-slice.
    if (IsLinearSwizzleMode(swizzleMode))
    {
        // For linear Images, the mip offset computed by AddrLib is correct.
        pSubResInfo->offset = mipInfo.offset;

        // Linear resource must have block sizes of zero. This is assumed by DdiResource::CheckSubresourceInfo().
        pSubResInfo->blockSize.width  = 0;
        pSubResInfo->blockSize.height = 0;
        pSubResInfo->blockSize.depth  = 0;
    }
    else
    {
        // On GFX12, mips are stored in reverse order (i.e., the largest mip is farthest away from the start)
        pSubResInfo->offset = mipInfo.macroBlockOffset + mipInfo.mipTailOffset;

        pSubResInfo->blockSize.width  = surfaceInfo.blockExtent.width;
        pSubResInfo->blockSize.height = surfaceInfo.blockExtent.height;
        pSubResInfo->blockSize.depth  = surfaceInfo.blockExtent.depth;

        // Initialize the pipe-bank xor of right eye surface for DXGI stereo.
        if ((pImage->GetImageCreateInfo().flags.dxgiStereo == 1) && (pSubResInfo->subresId.arraySlice == 1))
        {
            const uint32 basePipeBankXor = GetTileSwizzle(pImage, BaseSubres(0));

            result = GetStereoRightEyePipeBankXor(*pImage,
                                                  pSubResInfo,
                                                  swizzleMode,
                                                  basePipeBankXor,
                                                  &pTileInfo->pipeBankXor);
        }

        if ((result == Result::Success) &&
            ((createInfo.mipLevels > 1) &&
             ((createInfo.arraySize > 1) || (createInfo.imageType == Pal::ImageType::Tex2d))))
        {
            ADDR3_COMPUTE_SUBRESOURCE_OFFSET_FORSWIZZLEPATTERN_INPUT  addr3Input = {};
            ADDR3_COMPUTE_SUBRESOURCE_OFFSET_FORSWIZZLEPATTERN_OUTPUT addr3Output = {};

            addr3Input.size             = sizeof(ADDR3_COMPUTE_SUBRESOURCE_OFFSET_FORSWIZZLEPATTERN_INPUT);
            addr3Input.swizzleMode      = swizzleMode;
            addr3Input.resourceType     = GetAddrResourceType(createInfo.imageType);
            addr3Input.pipeBankXor      = pTileInfo->pipeBankXor;
            addr3Input.slice            = pSubResInfo->subresId.arraySlice;
            addr3Input.sliceSize        = surfaceInfo.sliceSize;
            addr3Input.macroBlockOffset = mipInfo.macroBlockOffset;
            addr3Input.mipTailOffset    = mipInfo.mipTailOffset;

            addr3Input.size = sizeof(ADDR3_COMPUTE_SUBRESOURCE_OFFSET_FORSWIZZLEPATTERN_OUTPUT);

            ADDR_E_RETURNCODE addrResult = Addr3ComputeSubResourceOffsetForSwizzlePattern(AddrLibHandle(),
                                                                                          &addr3Input,
                                                                                          &addr3Output);

            if (addrResult != ADDR_OK)
            {
                result = Result::ErrorUnknown;
            }

            pSubResInfo->swizzleOffset = addr3Output.offset;
        }
    }

    // KMD maintains a backing store copy in nonlocal memory for some Images. This backing store is always
    // linear-tiled, so the offset to each mipmap level is different than for the original Image. Track the linear
    // offset to each mip level as though the Image were linear tiled* so we can report this offset to the KMD.
    // Fortunately, AddrLib will provide this offset to us in the ADDR3_MIP_INFO structure.
    pTileInfo->backingStoreOffset = mipInfo.offset;

    // Give the GfxIp HWL a chance to finalize or override any subresource properties.
    pImage->GetGfxImage()->Addr3FinalizeSubresource(pSubResInfo, swizzleMode);

    BuildTileToken(pSubResInfo, swizzleMode);

    // Convert the address library's swizzle equation index into Pal's representation. Note that linear swizzle
    // modes will result in an invalid equation index. To give our clients a way to handle linear modes we set the
    // index to LinearSwizzleEqIndex.
    const uint32 eqIdx = mipInfo.equationIndex;
    if (swizzleMode == ADDR3_LINEAR)
    {
        pSubResInfo->swizzleEqIndex = LinearSwizzleEqIndex;
    }
    else if (eqIdx == ADDR_INVALID_EQUATION_INDEX)
    {
        pSubResInfo->swizzleEqIndex = InvalidSwizzleEqIndex;
    }
    else
    {
        pSubResInfo->swizzleEqIndex = static_cast<uint8>(eqIdx);
    }

    if ((result == Result::Success) && (pSubResInfo->subresId.mipLevel == 0))
    {
        // Fail if we didn't satisfy the client's requested row and depth pitches.
        if ((createInfo.rowPitch != 0) && (pSubResInfo->rowPitch != createInfo.rowPitch))
        {
            result = Result::ErrorMismatchedImageRowPitch;
        }
        else if ((createInfo.depthPitch != 0) && (pSubResInfo->depthPitch != createInfo.depthPitch))
        {
            result = Result::ErrorMismatchedImageDepthPitch;
        }
    }

    return result;
}

// =====================================================================================================================
// Helper function for determining the ADDR3 surface flags for a specific plane of an Image.
ADDR3_SURFACE_FLAGS AddrMgr3::DetermineSurfaceFlags(
    const Image& image,
    uint32       plane
    ) const
{
    ADDR3_SURFACE_FLAGS flags = { };

    const ImageCreateInfo& createInfo = image.GetImageCreateInfo();

    if (image.IsDepthStencilTarget())
    {
        flags.depth   = image.IsDepthPlane(plane);
        flags.stencil = image.IsStencilPlane(plane);
    }

    // Note: We should always set the texture flag since even Color or Depth/Stencil resources could be bound as a
    // shader resource for RPM blts.
    if (Formats::IsBlockCompressed(createInfo.swizzledFormat.format) && (createInfo.tiling == ImageTiling::Linear))
    {
        // A linear block compressed image can only be used as staging resource, so leave texture flag to 0 to let
        // AddrLib correctly choose preferred linear mode (otherwise AddrLib returns ADDR_INVALIDPARAMS).
        PAL_ASSERT((createInfo.usageFlags.shaderRead == 0) && (createInfo.usageFlags.shaderWrite == 0));
    }

    flags.blockCompressed = Formats::IsBlockCompressed(createInfo.swizzledFormat.format);
    flags.nv12            = (createInfo.swizzledFormat.format == ChNumFormat::NV12);
    flags.p010            = (createInfo.swizzledFormat.format == ChNumFormat::P010);

    //  GFX11 uses createInfo.tilingPreference to select valid swizzle modes:
    //  permittedSwSet.sw_Z = (createInfo.tilingPreference == ImageTilingPattern::Interleaved);
    //  permittedSwSet.sw_S = (createInfo.tilingPreference == ImageTilingPattern::Standard);
    //  permittedSwSet.sw_D = (createInfo.tilingPreference == ImageTilingPattern::XMajor);
    //  permittedSwSet.sw_R = (createInfo.tilingPreference == ImageTilingPattern::YMajor);
    //
    //  However, Addr::V3 swizzle modes don't have such Z S D R variations.
    //  When clients request YMajor they prefer the depth data to be separated out in slice order, like the 2D array
    //  arrangement.
    //  So we can turn on view3dAs2dArray bit based on tilingPreference == ImageTilingPattern::YMajor.
    //  GFX12 SW_XXX_2D is equivalent to GFX11 SW_XXX_D, and GFX11 sw_D is enabled for XMajor, similarly we can also
    //  toggle view3dAs2dArray bit for XMjaor.
    flags.view3dAs2dArray = createInfo.flags.view3dAs2dArray                            ||
                            (createInfo.tilingPreference == ImageTilingPattern::XMajor) ||
                            (createInfo.tilingPreference == ImageTilingPattern::YMajor);

    flags.isVrsImage      = createInfo.usageFlags.vrsRateImage;

    // We're not sure of the constraints DX requires, so do the conservative calculation.
    // For common YUV formats, we never hit the 'inexact' case anyways due to even height being required.
    flags.denseSliceExact = Formats::IsYuvPlanar(createInfo.swizzledFormat.format);
    flags.qbStereo        = createInfo.flags.stereo;

    // From addrMgr2.cpp:
    flags.display         = createInfo.flags.flippable     |
                            image.IsPrivateScreenPresent() |
                            image.IsTurboSyncSurface()     |
                            createInfo.flags.pipSwapChain;

#if ADDRLIB_VERSION >= ADDRLIB_MAKE_VERSION(10, 1)
    // Pass prt flag to addrlib to relax swizzle mode restrictions on PRT images especially.
    flags.standardPrt = createInfo.flags.prt;
#endif

    return flags;
}

// =====================================================================================================================
// Returns the number of slices an 3D image was *created* by the *address library* with.
uint32 AddrMgr3::GetNumAddrLib3dSlices(
    const Pal::Image*                         pImage,
    const Addr3SwizzleMode                    swizzleMode,
    const ADDR3_COMPUTE_SURFACE_INFO_OUTPUT&  surfInfoOut
    ) const
{
    const auto&  createInfo = pImage->GetImageCreateInfo();

    // It's the callers responsibility to verify that the image type is 3D
    PAL_ASSERT(createInfo.imageType == ImageType::Tex3d);

    // The number of slices used by the addrlib is what you'd expect for linear images and for tiled
    // images is based on the "numSlices" field
    const uint32  numSlices = (IsLinearSwizzleMode(swizzleMode) ? createInfo.extent.depth : surfInfoOut.numSlices);

    return numSlices;
}

// =====================================================================================================================
// Returns the HW enumeration swizzle mode that corresponds to the supplied swizzle mode.
uint32 AddrMgr3::GetHwSwizzleMode(
    Addr3SwizzleMode swizzleMode
    ) const
{
    uint32 retSwizzle = 0;

    {
        retSwizzle = static_cast<uint32>(swizzleMode);
    }

    return retSwizzle;
}

// =====================================================================================================================
// Determine whether a new swizzle type is acceptable based on memory waste ratio. Will favor larger swizzle types.
bool AddrMgr3::SwizzleTypeWithinMemoryBudget(
    uint32                      minSize,
    uint64                      newSwizzleTypeSize,
    uint32                      ratioLow,
    uint32                      ratioHi,
    float                       memoryBudget,
    bool                        newSwizzleTypeBigger
    ) const
{
    bool accept = false;

    if (memoryBudget >= 1.0)
    {
        if (newSwizzleTypeBigger)
        {
            if ((static_cast<float>(newSwizzleTypeSize) / minSize) <= memoryBudget)
            {
                accept = true;
            }
        }
        else
        {
            if ((static_cast<float>(minSize) / newSwizzleTypeSize) > memoryBudget)
            {
                accept = true;
            }
        }
    }
    else
    {
        if (newSwizzleTypeBigger)
        {
            // second surface/first surface <= ratioLow/RatioHi, select the second surface.
            if ((newSwizzleTypeSize * ratioHi) <= (minSize * ratioLow))
            {
                accept = true;
            }
        }
        else
        {
            if ((newSwizzleTypeSize * ratioLow) < (minSize * ratioHi))
            {
                accept = true;
            }
        }
    }

    return accept;
}

// =====================================================================================================================
// Check if a swizzle mode is valid in possible swizzle mode set or not. If not valid, it errors out.
Addr3SwizzleMode AddrMgr3::GetValidSwizzleMode(
    uint32                  possibleSwSet,
    Addr3SwizzleMode        outputSw
    ) const
{
    return TestAnyFlagSet(possibleSwSet, 1u << outputSw) ? outputSw : ADDR3_MAX_TYPE;
}

// =====================================================================================================================
// Computes the swizzling mode for an fmask surface
Result AddrMgr3::ComputeFmaskSwizzleMode(
    const Image*      pImage,
    Addr3SwizzleMode* pFinalMode
    ) const
{
    return ComputePlaneSwizzleMode(pImage, pImage->SubresourceInfo(0), true, pFinalMode);
}

// Compute the pipe-bank xor of right eye surface for DXGI stereo
Result AddrMgr3::GetStereoRightEyePipeBankXor(
    const Image&           image,
    const SubResourceInfo* pSubResInfo,
    Addr3SwizzleMode       swizzleMode,
    uint32                 basePipeBankXor,
    uint32*                pPipeBankXor
    ) const
{
    ADDR3_COMPUTE_SLICE_PIPEBANKXOR_INPUT  inSliceXor      = { 0 };
    ADDR3_COMPUTE_SLICE_PIPEBANKXOR_OUTPUT outSliceXor     = { 0 };
    const ImageCreateInfo&                 imageCreateInfo = image.GetImageCreateInfo();
    Pal::Device*                           pDevice         = image.GetDevice();

    inSliceXor.size            = sizeof(ADDR3_COMPUTE_SLICE_PIPEBANKXOR_INPUT);
    inSliceXor.swizzleMode     = swizzleMode;
    inSliceXor.resourceType    = GetAddrResourceType(imageCreateInfo.imageType);
    inSliceXor.bpe             = ElemSize(AddrLibHandle(), Image::GetAddrFormat(pSubResInfo->format.format));
    // We always have DXGI stereo primary's base PipeBankXor as zero for GFX12+
    PAL_ASSERT(basePipeBankXor == 0);
    inSliceXor.basePipeBankXor = basePipeBankXor;
    inSliceXor.slice           = 1;
    inSliceXor.numSamples      = imageCreateInfo.samples;

    ADDR_E_RETURNCODE addrRetCode = Addr3ComputeSlicePipeBankXor(pDevice->AddrLibHandle(),
                                                                 &inSliceXor,
                                                                 &outSliceXor);

    *pPipeBankXor = outSliceXor.pipeBankXor;

    return ((addrRetCode == ADDR_OK) ? Result::Success : Result::ErrorUnknown);
}

} // AddrMgr3
} // Pal
