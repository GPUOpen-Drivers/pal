/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/addrMgr/addrMgr.h"
#include "core/device.h"
#include "core/platform.h"
#include "palFormatInfo.h"
#include "palSysMemory.h"

using namespace Util;

namespace Pal
{

// Forward declaration of AddrLib callbacks
static void* ADDR_API AllocSysMemCb(const ADDR_ALLOCSYSMEM_INPUT* pInput);
static ADDR_E_RETURNCODE ADDR_API FreeSysMemCb(const ADDR_FREESYSMEM_INPUT* pInput);

// We are going to make some assumptions about AddrLib's swizzle equations.
static_assert(SwizzleEquationMaxBits == ADDR_MAX_EQUATION_BIT, "AddrLib equations are too long or too short!");
static_assert(sizeof(SwizzleEquationBit) == sizeof(ADDR_CHANNEL_SETTING), "AddrLib equation bits are the wrong size!");

// =====================================================================================================================
AddrMgr::AddrMgr(
    const Device* pDevice,
    size_t        tileInfoBytes)
    :
    m_pDevice(pDevice),
    m_gfxLevel(pDevice->ChipProperties().gfxLevel),
    m_hAddrLib(nullptr),
    m_pSwizzleEquations(nullptr),
    m_numSwizzleEquations(0),
    m_tileInfoBytes(tileInfoBytes)
{
}

// =====================================================================================================================
AddrMgr::~AddrMgr()
{
    if (m_hAddrLib != nullptr)
    {
        ADDR_E_RETURNCODE result = AddrDestroy(m_hAddrLib);
        PAL_ASSERT(result == ADDR_OK);
    }

    PAL_SAFE_DELETE_ARRAY(m_pSwizzleEquations, m_pDevice->GetPlatform());
}

// =====================================================================================================================
// Initializes the GPU address library.
Result AddrMgr::Init()
{
    ADDR_CREATE_INPUT  createInput  = {};
    ADDR_CREATE_OUTPUT createOutput = {};

    const GfxDevice*const pGfxDevice = m_pDevice->GetGfxDevice();

    // Setup chip info
    const GpuChipProperties& chipProps = m_pDevice->ChipProperties();
    pGfxDevice->InitAddrLibChipId(&createInput);

    createInput.minPitchAlignPixels = chipProps.imageProperties.minPitchAlignPixel;

    // Setup callbacks
    createInput.hClient               = this;
    createInput.callbacks.allocSysMem = AllocSysMemCb;
    createInput.callbacks.freeSysMem  = FreeSysMemCb;

    // Call the HWL to determine HW-specific register values.
    Result result = pGfxDevice->InitAddrLibCreateInput(&createInput.createFlags, &createInput.regValue);

    if (result == Result::Success)
    {
        ADDR_E_RETURNCODE addrRet = AddrCreate(&createInput, &createOutput);

        if (addrRet == ADDR_OK)
        {
            m_hAddrLib = createOutput.hLib;
        }
        else if (addrRet == ADDR_OUTOFMEMORY)
        {
            result = Result::ErrorOutOfMemory;
            PAL_ALERT_ALWAYS();
        }
        else
        {
            result = Result::ErrorUnknown;
            PAL_ALERT_ALWAYS();
        }
    }

    // Create a local copy of the swizzle equations/
    if (result == Result::Success)
    {
        m_numSwizzleEquations = createOutput.numEquations;
        m_pSwizzleEquations   = nullptr;

        if (m_numSwizzleEquations > 0)
        {
            m_pSwizzleEquations = PAL_NEW_ARRAY(SwizzleEquation,
                                                m_numSwizzleEquations,
                                                m_pDevice->GetPlatform(),
                                                SystemAllocType::AllocInternal);

            if (m_pSwizzleEquations != nullptr)
            {
                // If we have more than InvalidSwizzleEqIndex equations then it's no longer an invalid index.
                PAL_ASSERT(m_numSwizzleEquations <= InvalidSwizzleEqIndex);

                for (uint32 idx = 0; idx < m_numSwizzleEquations; ++idx)
                {
                    const auto& addrEq = createOutput.pEquationTable[idx];

                    // The static asserts at the top of this file make these copies legal.
                    memcpy(m_pSwizzleEquations[idx].addr, addrEq.addr, sizeof(addrEq.addr));
                    memcpy(m_pSwizzleEquations[idx].xor1, addrEq.xor1, sizeof(addrEq.xor1));
                    memcpy(m_pSwizzleEquations[idx].xor2, addrEq.xor2, sizeof(addrEq.xor2));

                    m_pSwizzleEquations[idx].numBits = addrEq.numBits;
                    m_pSwizzleEquations[idx].stackedDepthSlices = (addrEq.stackedDepthSlices != 0);
                }
            }
            else
            {
                result = Result::ErrorOutOfMemory;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Computes the information for the PRT packed mip tail belonging to an Image.
void AddrMgr::ComputePackedMipInfo(
    const Image&       image,
    ImageMemoryLayout* pGpuMemLayout
    ) const
{
    const ImageCreateInfo& createInfo = image.GetImageCreateInfo();

    // This function is supposed to be called for PRT images only
    PAL_ASSERT(createInfo.flags.prt != 0);

    pGpuMemLayout->prtMinPackedLod     = 0;
    pGpuMemLayout->prtMipTailTileCount = 0;

    // First determine the first mip level that will be part of the mip tail.
    for (pGpuMemLayout->prtMinPackedLod = 0;
         pGpuMemLayout->prtMinPackedLod < createInfo.mipLevels;
         pGpuMemLayout->prtMinPackedLod++)
    {
        const SubresId subresId = Subres(0, pGpuMemLayout->prtMinPackedLod, 0);
        const SubResourceInfo*const pSubResInfo = image.SubresourceInfo(subresId);

        if (m_pDevice->ChipProperties().imageProperties.prtFeatures & PrtFeatureUnalignedMipSize)
        {
            // If the HW supports unaligned mip sizes then stop at the first mip level that is
            // smaller than a single tile.
            if ((pSubResInfo->actualExtentElements.width  < pGpuMemLayout->prtTileWidth) ||
                (pSubResInfo->actualExtentElements.height < pGpuMemLayout->prtTileHeight))
            {
                break;
            }
        }
        else
        {
            // Otherwise stop at the first mip level that is not tile aligned.
            if (((pSubResInfo->actualExtentElements.width  % pGpuMemLayout->prtTileWidth)  != 0) ||
                ((pSubResInfo->actualExtentElements.height % pGpuMemLayout->prtTileHeight) != 0))
            {
                break;
            }
        }
    }

    // The mip tail will contain all of the mip levels that are smaller than a single tile. Not all images will have
    // a mip tail, because some image may only have mip levels which are larger than a single tile.
    if (pGpuMemLayout->prtMinPackedLod < createInfo.mipLevels)
    {
        ComputeTilesInMipTail(image, 0, pGpuMemLayout);
    }
}

// =====================================================================================================================
uint32 AddrMgr::CalcBytesPerElement(
    const SubResourceInfo* pSubResInfo
    ) const
{
    // The 96-bit formats which have three 32-bit element per texel.
    const uint32 bytesPerElement = (pSubResInfo->bitsPerTexel == 96) ? 4 :
        ElemSize(AddrLibHandle(), Image::GetAddrFormat(pSubResInfo->format.format)) >> 3;

    PAL_ASSERT(bytesPerElement > 0);

    return bytesPerElement;
}

// =====================================================================================================================
// Allocates memory for the AddrLib to use. Returns a pointer to allocated memory, or nullptr if failed.
void* ADDR_API AllocSysMemCb(
    const ADDR_ALLOCSYSMEM_INPUT* pInput)
{
    PAL_ASSERT(pInput != nullptr);
    AddrMgr*   pAddrMgr  = static_cast<AddrMgr*>(pInput->hClient);
    Platform*  pPlatform = pAddrMgr->GetDevice()->GetPlatform();

    return PAL_MALLOC(pInput->sizeInBytes, pPlatform, SystemAllocType::AllocInternal);
}

// =====================================================================================================================
// Frees memory allocated by AllocSysMemCb.
ADDR_E_RETURNCODE ADDR_API FreeSysMemCb(
    const ADDR_FREESYSMEM_INPUT* pInput)
{
    if (pInput != nullptr)
    {
        PAL_ASSERT(pInput->pVirtAddr != nullptr);

        AddrMgr*   pAddrMgr  = static_cast<AddrMgr*>(pInput->hClient);
        Platform*  pPlatform = pAddrMgr->GetDevice()->GetPlatform();

        PAL_FREE(pInput->pVirtAddr, pPlatform);
    }

    return ADDR_OK;
}

// =====================================================================================================================
SubResIterator::SubResIterator(
    const Image& image)
    :
    m_image(image),
    m_plane(0),
    m_mipLevel(0),
    m_arraySlice(0),
    m_subResIndex(0),
    m_baseSubResIndex(0)
{
}

// =====================================================================================================================
// Advances this iterator to the next subresource in GPU memory.
// Returns: true if there are still subresources remaining to be walked-over; false otherwise.
bool SubResIterator::Next()
{
    const ImageCreateInfo& createInfo = m_image.GetImageCreateInfo();
    const ImageInfo&       imageInfo  = m_image.GetImageInfo();

    if (Formats::IsYuvPlanar(createInfo.swizzledFormat.format))
    {
        // Images with YUV formats are stored in plane-major order where all planes of an array slice preceed
        // the all planes of the next array slice.
        PAL_ASSERT(createInfo.mipLevels == 1);

        ++m_plane;
        if (m_plane >= imageInfo.numPlanes)
        {
            m_plane = 0;
            ++m_arraySlice;
        }
    }
    else
    {
        // Images with color or depth/stencil formats are stored in subresource-major order where all mips and
        // slices of depth preceed all mips and slices of stencil.

        ++m_arraySlice;
        if (m_arraySlice >= createInfo.arraySize)
        {
            m_arraySlice = 0;
            ++m_mipLevel;
            if (m_mipLevel >= createInfo.mipLevels)
            {
                m_mipLevel = 0;
                ++m_plane;
            }
        }
    }

    // Compute the current sub-resource index.
    const uint32 subResourcesPerPlane = (createInfo.arraySize * createInfo.mipLevels);
    const uint32 subResourceInPlane   = (m_mipLevel * createInfo.arraySize) + m_arraySlice;

    m_subResIndex     = (m_plane * subResourcesPerPlane) + subResourceInPlane;
    m_baseSubResIndex = (m_plane * subResourcesPerPlane) + m_arraySlice;

    return ((m_plane      < imageInfo.numPlanes)  &&
            (m_arraySlice < createInfo.arraySize) &&
            (m_mipLevel   < createInfo.mipLevels));
}

} // Pal
