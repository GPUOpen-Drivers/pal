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

#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuGpuMemory.h"
#include "core/os/amdgpu/amdgpuImage.h"
#include "core/os/amdgpu/amdgpuSwapChain.h"
#include "core/os/amdgpu/amdgpuWindowSystem.h"
#include "palFormatInfo.h"
#include "palSysMemory.h"

using namespace Util;

namespace Pal
{
namespace Amdgpu
{

// =====================================================================================================================
Image::Image(
    Device*                        pDevice,
    const ImageCreateInfo&         createInfo,
    const ImageInternalCreateInfo& internalCreateInfo)
    :
    Pal::Image(pDevice,
               (this + 1),
               VoidPtrInc((this + 1), pDevice->GetGfxDevice()->GetImageSize(createInfo)),
               createInfo,
               internalCreateInfo),
    m_presentImageHandle(NullImageHandle),
    m_pWindowSystem(nullptr),
    m_framebufferId(0),
    m_idle(true)
{
}

// =====================================================================================================================
Image::~Image()
{
    if (m_presentImageHandle.pBuffer != NullImageHandle.pBuffer)
    {
        m_pWindowSystem->DestroyPresentableImage(m_presentImageHandle);
    }
}

// =====================================================================================================================
// Returns the size of a presentable image.
void Image::GetImageSizes(
    const Device&                     device,
    const PresentableImageCreateInfo& createInfo,
    size_t*                           pImageSize,
    size_t*                           pGpuMemorySize,
    Result*                           pResult)
{
    PAL_ASSERT((pImageSize != nullptr) && (pGpuMemorySize != nullptr));

    ImageCreateInfo imageInfo = { };

    imageInfo.swizzledFormat = createInfo.swizzledFormat;
    imageInfo.usageFlags     = createInfo.usage;
    imageInfo.extent.width   = createInfo.extent.width;
    imageInfo.extent.height  = createInfo.extent.height;
    imageInfo.imageType      = ImageType::Tex2d;
    imageInfo.tiling         = ImageTiling::Optimal;
    imageInfo.arraySize      = 1;
    imageInfo.mipLevels      = 1;

    if (createInfo.flags.stereo)
    {
        imageInfo.arraySize = 2;
    }

    *pImageSize = device.GetImageSize(imageInfo, pResult);

    if (*pResult == Result::Success)
    {
        GpuMemoryCreateInfo gpuMemInfo = { };
        gpuMemInfo.priority = GpuMemPriority::High;

        *pGpuMemorySize = device.GetGpuMemorySize(gpuMemInfo, nullptr);
    }
}

// =====================================================================================================================
// Converts the presentable image create info to create and initalize a concrete image object.
Result Image::CreatePresentableImage(
    Device*                           pDevice,
    const PresentableImageCreateInfo& createInfo,
    void*                             pImagePlacementAddr,
    void*                             pGpuMemoryPlacementAddr,
    IImage**                          ppImage,
    IGpuMemory**                      ppGpuMemory)
{
    PAL_ASSERT((ppImage != nullptr) && (ppGpuMemory != nullptr));

     Result result = Result::ErrorInvalidPointer;

    // Currently, all Linux presentable images require swap chains.
    // For Android, SwapChain is managed by Loader. Loader will deliver the present buffer handle to Let ICD import
    if (createInfo.pSwapChain != nullptr)
    {
        Pal::Image*     pImage        = nullptr;
        ImageCreateInfo imgCreateInfo = {};

        imgCreateInfo.imageType             = ImageType::Tex2d;
        imgCreateInfo.swizzledFormat        = createInfo.swizzledFormat;
        imgCreateInfo.tiling                = ImageTiling::Optimal;
        imgCreateInfo.usageFlags.u32All     = createInfo.usage.u32All;
        imgCreateInfo.extent.width          = createInfo.extent.width;
        imgCreateInfo.extent.height         = createInfo.extent.height;
        imgCreateInfo.extent.depth          = 1;
        imgCreateInfo.arraySize             = 1;
        imgCreateInfo.mipLevels             = 1;
        imgCreateInfo.samples               = 1;
        imgCreateInfo.fragments             = 1;
        imgCreateInfo.viewFormatCount       = createInfo.viewFormatCount;
        imgCreateInfo.pViewFormats          = createInfo.pViewFormats;
        imgCreateInfo.flags.flippable       = 1;

        // Linux doesn't support stereo images.
        PAL_ASSERT(createInfo.flags.stereo == 0);

        ImageInternalCreateInfo internalInfo = {};
        internalInfo.flags.presentable       = 1;

        result = pDevice->CreateInternalImage(imgCreateInfo, internalInfo, pImagePlacementAddr, &pImage);

        if (result == Result::Success)
        {
            Pal::GpuMemory* pGpuMemory = nullptr;
            auto*const pLnxImage = static_cast<Image*>(pImage);

            result = pDevice->CreatePresentableMemoryObject(createInfo,
                                                            pLnxImage,
                                                            pGpuMemoryPlacementAddr,
                                                            &pGpuMemory);

            if (result == Result::Success)
            {
                result = pImage->BindGpuMemory(pGpuMemory, 0);
            }

            if (result == Result::Success)
            {
                // Update the image information to external user such as Xserver.
                 result = pDevice->UpdateExternalImageInfo(createInfo, pGpuMemory, pLnxImage);
            }

            if (result == Result::Success)
            {
                *ppGpuMemory = pGpuMemory;
                *ppImage     = pImage;
            }
            else
            {
                // Destroy the image if something failed.
                pImage->Destroy();
            }
        }
    }

    return result;
}

// =====================================================================================================================
// If the memory will be exported, we update the tiling info to metadata.
// if the memory is imported from external, we update the tiling info from metadata.
void Image::UpdateMetaDataInfo(
    IGpuMemory* pGpuMemory)
{
    if (pGpuMemory != nullptr)
    {
        auto*const pAmdgpuDevice = static_cast<Amdgpu::Device*>(m_pDevice);
        auto*const pAmdgpuGpuMem = static_cast<Amdgpu::GpuMemory*>(pGpuMemory);

        if (pAmdgpuGpuMem->IsInterprocess())
        {
            pAmdgpuDevice->UpdateMetaData(pAmdgpuGpuMem->SurfaceHandle(), *this);
        }
        else if (pAmdgpuGpuMem->IsExternal())
        {
            pAmdgpuDevice->UpdateImageInfo(pAmdgpuGpuMem->SurfaceHandle(), this);
        }
    }
}

// =====================================================================================================================
// Update the memory and image info for external usage.
Result Image::UpdateExternalImageInfo(
    Device*                              pDevice,
    const PresentableImageCreateInfo&    createInfo,
    Pal::GpuMemory*                      pGpuMemory,
    Pal::Image*                          pImage)
{
    Result result = Result::Success;

    auto*const pAmdgpuImage     = static_cast<Amdgpu::Image*>(pImage);
    auto*const pAmdgpuGpuMemory = static_cast<Amdgpu::GpuMemory*>(pGpuMemory);
    auto*const pSwapChain       = static_cast<Amdgpu::SwapChain*>(createInfo.pSwapChain);
    auto*const pWindowSystem    = pSwapChain->GetWindowSystem();

    Pal::GpuMemoryExportInfo exportInfo = {};
    const int32 sharedBufferFd  = static_cast<int32>(pAmdgpuGpuMemory->ExportExternalHandle(exportInfo));

    // Update the image information to metadata.
    pDevice->UpdateMetaData(pAmdgpuGpuMemory->SurfaceHandle(), *pAmdgpuImage);

    if (sharedBufferFd >= 0)
    {
        // All presentable images must save a pointer to their swap chain's windowing system so that they
        // can destroy this image handle later on.
        pAmdgpuImage->m_pWindowSystem = pWindowSystem;

        result = pWindowSystem->CreatePresentableImage(pSwapChain, pAmdgpuImage, sharedBufferFd);
    }

    return result;
}

// =====================================================================================================================
// Creates an internal GPU memory object and binds it to the presentable Image associated with this object.
Result Image::CreatePresentableMemoryObject(
    Device*                             pDevice,
    const PresentableImageCreateInfo&   presentableImageCreateInfo,
    Image*                              pImage,        // [in] Image the memory object should be based on
    void*                               pMemObjMem,    // [in,out] Preallocated memory for the GpuMemory object
    Pal::GpuMemory**                    ppMemObjOut)   // [out] Newly created GPU memory object
{
    GpuMemoryRequirements memReqs = {};
    pImage->GetGpuMemoryRequirements(&memReqs);

    GpuMemoryCreateInfo createInfo = {};
    createInfo.flags.flippable    = pImage->IsFlippable();
    createInfo.flags.stereo       = pImage->GetInternalCreateInfo().flags.stereo;
    createInfo.flags.peerWritable = presentableImageCreateInfo.flags.peerWritable;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 516
    createInfo.size               = memReqs.size;
    createInfo.alignment          = memReqs.alignment;
#else
    const gpusize allocGranularity = pDevice->MemoryProperties().realMemAllocGranularity;
    createInfo.size               = Pow2Align(memReqs.size, allocGranularity);
    createInfo.alignment          = Pow2Align(memReqs.alignment, allocGranularity);
#endif
    createInfo.vaRange            = VaRange::Default;
    createInfo.priority           = GpuMemPriority::VeryHigh;
    createInfo.heapCount          = 0;
    createInfo.pImage             = pImage;

    for (uint32 i = 0; i < memReqs.heapCount; i++)
    {
        // Don't allocate from local visible heap since the memory won't be mapped.
        if (memReqs.heaps[i] != GpuHeapLocal)
        {
            createInfo.heaps[createInfo.heapCount] = memReqs.heaps[i];
            createInfo.heapCount++;
        }
    }

    GpuMemoryInternalCreateInfo internalInfo = {};

    Pal::GpuMemory* pGpuMemory = nullptr;
    Result result = pDevice->CreateInternalGpuMemory(createInfo, internalInfo, pMemObjMem, &pGpuMemory);

    if (result == Result::Success)
    {
        *ppMemObjOut = static_cast<GpuMemory*>(pGpuMemory);
    }
    else
    {
        // Destroy the memory if something failed.
        pGpuMemory->Destroy();
    }

    return result;
}

// =====================================================================================================================
// Fills out pCreateInfo according to the information in openInfo and sharedInfo. Assumes the contents of pCreateInfo
// are zeroed.
void Image::GetExternalSharedImageCreateInfo(
    const Device&                device,
    const ExternalImageOpenInfo& openInfo,
    const ExternalSharedInfo&    sharedInfo,
    ImageCreateInfo*             pCreateInfo)
{
    // Start with the caller's flags, we'll add some more later on.
    pCreateInfo->flags      = openInfo.flags;
    pCreateInfo->usageFlags = openInfo.usage;

    // Most information will come directly from the base subresource's surface description.
    const auto* pMetadata = reinterpret_cast<const amdgpu_bo_umd_metadata*>(
        &sharedInfo.info.metadata.umd_metadata[PRO_UMD_METADATA_OFFSET_DWORD]);

    pCreateInfo->extent.width  = pMetadata->width_in_pixels;
    pCreateInfo->extent.height = pMetadata->height;
    pCreateInfo->extent.depth  = pMetadata->depth;
    pCreateInfo->imageType     = static_cast<ImageType>(pMetadata->flags.resource_type);

    if (Formats::IsUndefined(openInfo.swizzledFormat.format))
    {
        bool changeFormat      = false;
        bool depthStencilUsage = false;
        pCreateInfo->swizzledFormat = AmdgpuFormatToPalFormat(pMetadata->format, &changeFormat, &depthStencilUsage);

        if (changeFormat)
        {
            pCreateInfo->viewFormatCount = AllCompatibleFormats;
        }
        pCreateInfo->usageFlags.depthStencil = depthStencilUsage;
    }
    else
    {
        pCreateInfo->swizzledFormat = openInfo.swizzledFormat;
    }

    bool isLinearTiled = false;
    if (device.ChipProperties().gfxLevel < GfxIpLevel::GfxIp9)
    {
        isLinearTiled = (pMetadata->tile_mode == AMDGPU_TILE_MODE__LINEAR_GENERAL) ||
                        (pMetadata->tile_mode == AMDGPU_TILE_MODE__LINEAR_ALIGNED);
    }
    else
    {
        isLinearTiled = (pMetadata->swizzleMode == AMDGPU_SWIZZLE_MODE_LINEAR) ||
                        (pMetadata->swizzleMode == AMDGPU_SWIZZLE_MODE_LINEAR_GENERAL);
    }

    if (isLinearTiled)
    {
        // Provide pitch and depth information for linear tiled images. YUV formats use linear.
        pCreateInfo->rowPitch  = pMetadata->aligned_pitch_in_bytes;
        pCreateInfo->depthPitch  = pCreateInfo->rowPitch * pMetadata->aligned_height;
    }

    pCreateInfo->tiling = isLinearTiled ? ImageTiling::Linear : ImageTiling::Optimal;

    pCreateInfo->mipLevels = pMetadata->flags.mip_levels;
    pCreateInfo->arraySize = pMetadata->array_size;
    pCreateInfo->samples   = 1;
    pCreateInfo->fragments = 1;

    pCreateInfo->flags.cubemap = (pMetadata->flags.cubemap != 0);

    // OR-in some additional usage flags.
    pCreateInfo->usageFlags.shaderRead   |= pMetadata->flags.texture;
    pCreateInfo->usageFlags.shaderWrite  |= pMetadata->flags.unodered_access;
    pCreateInfo->usageFlags.colorTarget  |= pMetadata->flags.render_target;
    pCreateInfo->usageFlags.depthStencil |= pMetadata->flags.depth_stencil;

    pCreateInfo->flags.optimalShareable = pMetadata->flags.optimal_shareable;
    // This image must be shareable (as it has already been shared); request view format change as well to be safe.
    pCreateInfo->flags.shareable       = 1;
    pCreateInfo->viewFormatCount       = AllCompatibleFormats;
    pCreateInfo->flags.flippable       = false;
}

// =====================================================================================================================
// Create an external shared image object and associated video memory object
Result Image::CreateExternalSharedImage(
    Device*                       pDevice,
    const ExternalImageOpenInfo&  openInfo,
    const ExternalSharedInfo&     sharedInfo,
    void*                         pImagePlacementAddr,
    void*                         pGpuMemoryPlacementAddr,
    GpuMemoryCreateInfo*          pMemCreateInfo,
    IImage**                      ppImage,
    IGpuMemory**                  ppGpuMemory)
{
    const GpuChipProperties& chipProps = pDevice->ChipProperties();

    auto*const  pPrivateScreen = static_cast<PrivateScreen*>(openInfo.pScreen);
    const auto* pMetadata = reinterpret_cast<const amdgpu_bo_umd_metadata*>(
        &sharedInfo.info.metadata.umd_metadata[PRO_UMD_METADATA_OFFSET_DWORD]);

    ImageCreateInfo createInfo = {};
    GetExternalSharedImageCreateInfo(*pDevice, openInfo, sharedInfo, &createInfo);

    ImageInternalCreateInfo internalCreateInfo = {};
    if (chipProps.gfxLevel < GfxIpLevel::GfxIp9)
    {
        internalCreateInfo.gfx6.sharedTileMode    = static_cast<AddrTileMode>(pMetadata->tile_mode);
        internalCreateInfo.gfx6.sharedTileType    = static_cast<AddrTileType>(pMetadata->micro_tile_mode);
        internalCreateInfo.gfx6.sharedTileSwizzle = pMetadata->pipeBankXor;
        internalCreateInfo.gfx6.sharedTileIndex   = pMetadata->tile_index;
    }
    else if (chipProps.gfxLevel >= GfxIpLevel::GfxIp9)
    {
        internalCreateInfo.gfx9.sharedPipeBankXor = pMetadata->pipeBankXor;
        internalCreateInfo.gfx9.sharedSwizzleMode = static_cast<AddrSwizzleMode>(pMetadata->swizzleMode);

        // ADDR_SW_LINEAR_GENERAL is a UBM compatible swizzle mode which treat as buffer in copy.
        // Here we try ADDR_SW_LINEAR first and fall back to typed buffer path if failure the creation as PAL::image.
        if (internalCreateInfo.gfx9.sharedSwizzleMode == ADDR_SW_LINEAR_GENERAL)
        {
            internalCreateInfo.gfx9.sharedSwizzleMode = ADDR_SW_LINEAR;
        }
    }
    else
    {
        // Which ASIC is this?
        PAL_ASSERT_ALWAYS();
    }

    internalCreateInfo.flags.privateScreenPresent     = (pPrivateScreen != nullptr);
    internalCreateInfo.flags.useSharedTilingOverrides = 1;

    if (pMetadata->flags.optimal_shareable)
    {
        auto*const pUmdSharedMetadata =
            reinterpret_cast<const amdgpu_shared_metadata_info*>
            (&pMetadata->shared_metadata_info);
        internalCreateInfo.flags.useSharedMetadata = 1;

        internalCreateInfo.sharedMetadata.dccOffset = pUmdSharedMetadata->dcc_offset;
        internalCreateInfo.sharedMetadata.cmaskOffset = pUmdSharedMetadata->cmask_offset;
        internalCreateInfo.sharedMetadata.fmaskOffset = pUmdSharedMetadata->fmask_offset;
        internalCreateInfo.sharedMetadata.htileOffset = pUmdSharedMetadata->htile_offset;

        internalCreateInfo.sharedMetadata.flags.shaderFetchable =
            pUmdSharedMetadata->flags.shader_fetchable;
        internalCreateInfo.sharedMetadata.flags.shaderFetchableFmask =
            pUmdSharedMetadata->flags.shader_fetchable_fmask;
        internalCreateInfo.sharedMetadata.flags.hasWaTcCompatZRange =
            pUmdSharedMetadata->flags.has_wa_tc_compat_z_range;
        internalCreateInfo.sharedMetadata.flags.hasEqGpuAccess =
            pUmdSharedMetadata->flags.has_eq_gpu_access;
        internalCreateInfo.sharedMetadata.flags.hasHtileLookupTable =
            pUmdSharedMetadata->flags.has_htile_lookup_table;

        internalCreateInfo.sharedMetadata.fastClearMetaDataOffset =
            pUmdSharedMetadata->fast_clear_value_offset;
        internalCreateInfo.sharedMetadata.fastClearEliminateMetaDataOffset =
            pUmdSharedMetadata->fce_state_offset;

        // The offset here will be updated once change of amdgpu_shared_metadata_info is done.
        internalCreateInfo.sharedMetadata.hisPretestMetaDataOffset = 0;

        if (pUmdSharedMetadata->dcc_offset != 0)
        {
            internalCreateInfo.sharedMetadata.dccStateMetaDataOffset =
                pUmdSharedMetadata->dcc_state_offset;
        }
        else if (pUmdSharedMetadata->flags.has_htile_lookup_table)
        {
            internalCreateInfo.sharedMetadata.htileLookupTableOffset =
                pUmdSharedMetadata->htile_lookup_table_offset;
        }

        if (pUmdSharedMetadata->flags.htile_as_fmask_xor)
        {
            PAL_ASSERT(pDevice->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp9);
            internalCreateInfo.gfx9.sharedPipeBankXorFmask =
                LowPart(internalCreateInfo.sharedMetadata.htileOffset);
            internalCreateInfo.sharedMetadata.htileOffset = 0;
        }

        internalCreateInfo.sharedMetadata.resourceId = pUmdSharedMetadata->resource_id;
        createInfo.flags.optimalShareable = 1;
    }
    else
    {
        createInfo.flags.optimalShareable = 0;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 481
        createInfo.metadataMode           = MetadataMode::Disabled;
#else
        createInfo.flags.noMetadata       = 1;
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 496
        createInfo.metadataTcCompatMode   = MetadataTcCompatMode::Disabled;
#endif
    }

    Pal::Image* pImage = nullptr;
    Result result = pDevice->CreateInternalImage(createInfo,
                                                 internalCreateInfo,
                                                 pImagePlacementAddr,
                                                 &pImage);

    uint32 imageId = 0;
    if ((result == Result::Success) && (pPrivateScreen != nullptr))
    {
        if (pPrivateScreen->FormatSupported(createInfo.swizzledFormat))
        {
            result = pPrivateScreen->ObtainImageId(&imageId);
        }
        else
        {
            result = Result::ErrorPrivateScreenInvalidFormat;
        }

        if (result == Result::Success)
        {
            pImage->SetPrivateScreen(pPrivateScreen);
            pImage->SetPrivateScreenImageId(imageId);
        }
    }

    Pal::GpuMemory*     pGpuMemory    = nullptr;
    GpuMemoryCreateInfo memCreateInfo = {};

    if (result == Result::Success)
    {
        result = pDevice->CreateGpuMemoryFromExternalShare(nullptr,
                                                           pImage,
                                                           sharedInfo,
                                                           pGpuMemoryPlacementAddr,
                                                           &memCreateInfo,
                                                           &pGpuMemory);
    }

    if (result == Result::Success)
    {
        result = pImage->BindGpuMemory(pGpuMemory, 0);
    }
    else if (pGpuMemory != nullptr)
    {
        // Something went wrong after we created the memory so we must destroy it.
        pGpuMemory->Destroy();
    }

    if ((result == Result::Success) && (pPrivateScreen != nullptr))
    {
        pPrivateScreen->SetImageSlot(imageId, pImage);
    }

    if (result == Result::Success)
    {
        // No errors occured so report back the image, memory object, and memory create info.
        (*ppImage) = pImage;

        (*ppGpuMemory) = pGpuMemory;

        if (pMemCreateInfo != nullptr)
        {
            memcpy(pMemCreateInfo, &memCreateInfo, sizeof(GpuMemoryCreateInfo));
        }
    }
    else if (pImage != nullptr)
    {
        // Something went wrong after we created the image so we must destroy it.
        pImage->Destroy();
    }

    return result;
}

} // Linux
} // Pal
