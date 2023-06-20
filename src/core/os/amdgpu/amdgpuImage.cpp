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

#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuGpuMemory.h"
#include "core/os/amdgpu/amdgpuImage.h"
#include "core/os/amdgpu/amdgpuSwapChain.h"
#include "core/os/amdgpu/amdgpuWindowSystem.h"
#include "core/addrMgr/addrMgr1/addrMgr1.h"
#include "core/hw/gfxip/gfx9/gfx9MaskRam.h"
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
    m_pPresentableBuffer(),
    m_framebufferId(0),
    m_idle(true),
    m_pSwapChain(nullptr),
    m_imageIndex(InvalidImageIndex),
    m_drmModeIsSet(false)
{
    // Pip swap-chain is only supported on Windows platforms.
    PAL_ASSERT(createInfo.flags.pipSwapChain == 0);
}

// =====================================================================================================================
Image::~Image()
{
    if (m_presentImageHandle.pBuffer != NullImageHandle.pBuffer)
    {
        if (m_pWindowSystem != nullptr)
        {
            m_pWindowSystem->DestroyPresentableImage(m_presentImageHandle);
        }
    }
    if (m_pPresentableBuffer != nullptr)
    {
        m_pPresentableBuffer->Destroy();
        PAL_SAFE_FREE(m_pPresentableBuffer, m_pDevice->GetPlatform());
    }
}

// =====================================================================================================================
// This is only used for the CPU present path, where it's needed because the images aren't backed by real GPU memory.
// So first, we need a linear image that is kept in this 'presentable buffer'.
// This is dynamically allocated at the first present with a given image.
Result Image::CreatePresentableBuffer()
{
    Result result = Result::Success;
    PAL_ASSERT(m_pPresentableBuffer == nullptr);

    IGpuMemory* pGpuMemoryOut = nullptr;
    ImageCreateInfo imgInfo = GetImageCreateInfo();

    GpuMemoryCreateInfo createInfo = {};
    createInfo.size = imgInfo.extent.width *
                      imgInfo.extent.height *
                      Formats::BytesPerPixel(imgInfo.swizzledFormat.format);

    createInfo.priority  = GpuMemPriority::Normal;

    createInfo.heapCount = 2;
    createInfo.heaps[0]  = GpuHeapLocal;
    createInfo.heaps[1]  = GpuHeapGartCacheable;

    const size_t objectSize = m_pDevice->GetGpuMemorySize(createInfo, &result);
    if (result == Result::Success)
    {
        void* pMemory = PAL_MALLOC(objectSize, m_pDevice->GetPlatform(), AllocObject);
        if (pMemory != nullptr)
        {
            result = m_pDevice->CreateGpuMemory(createInfo, pMemory, &pGpuMemoryOut);
            if (result != Pal::Result::Success)
            {
                PAL_SAFE_FREE(pMemory, m_pDevice->GetPlatform());
            }
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    if (result == Result::Success)
    {
        m_pPresentableBuffer = static_cast<GpuMemory*>(pGpuMemoryOut);
    }
    return result;
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
    if (createInfo.pSwapChain != nullptr)
    {
        auto*const      pSwapChain    = static_cast<Amdgpu::SwapChain*>(createInfo.pSwapChain);
        auto*const      pWindowSystem = pSwapChain->GetWindowSystem();
        Pal::Image*     pImage        = nullptr;
        ImageCreateInfo imgCreateInfo = {};

        // When it's multiGpu, the metadata of BO on other gpu can't be shared across GPUs since it's possible that
        // the metadata is meaningless for other GPUs. So, the GBM (amdgpu backend) set linear meta when the BO is
        // from other AMD GPUs. Enable Linear mode only when presenting on different GPU.
        imgCreateInfo.tiling = pWindowSystem->PresentOnSameGpu() ? ImageTiling::Optimal : ImageTiling::Linear;

        imgCreateInfo.imageType             = ImageType::Tex2d;
        imgCreateInfo.swizzledFormat        = createInfo.swizzledFormat;
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
        imgCreateInfo.flags.presentable     = 1;

        // Linux doesn't support stereo images.
        PAL_ASSERT(createInfo.flags.stereo == 0);

        ImageInternalCreateInfo internalInfo = {};
#if PAL_DISPLAY_DCC
            if ((imgCreateInfo.usageFlags.disableOptimizedDisplay == 0) &&
                (pDevice->SupportDisplayDcc() == true) &&
                // VCAM_SURFACE_DESC does not support YUV presentable yet
                (Formats::IsYuv(imgCreateInfo.swizzledFormat.format) == false))
            {
                DisplayDccCaps displayDcc = { };

                pDevice->GetDisplayDccInfo(displayDcc);
                PAL_ASSERT(displayDcc.dcc_256_128_128 ||
                           displayDcc.dcc_128_128_unconstrained ||
                           displayDcc.dcc_256_64_64);
                if (displayDcc.pipeAligned == 0)
                {
                    internalInfo.displayDcc.value   = displayDcc.value;
                    internalInfo.displayDcc.enabled = 1;
                    imgCreateInfo.flags.optimalShareable     = 1;
                }
            }
#endif

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
            pAmdgpuDevice->UpdateMetaData(pAmdgpuGpuMem->SurfaceHandle(), *this, pAmdgpuGpuMem);
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
    pDevice->UpdateMetaData(pAmdgpuGpuMemory->SurfaceHandle(), *pAmdgpuImage, pAmdgpuGpuMemory);

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
    createInfo.flags.presentable  = 1;
    createInfo.flags.flippable    = pImage->IsFlippable();
    createInfo.flags.stereo       = pImage->GetInternalCreateInfo().flags.stereo;
    createInfo.flags.peerWritable = presentableImageCreateInfo.flags.peerWritable;

    // If client creates presentable image without swapchain, TMZ state should be determined by PresentableImageCreateInfo.
    const bool tmzEnable = (presentableImageCreateInfo.pSwapChain == nullptr)
        ? presentableImageCreateInfo.flags.tmzProtected
        : static_cast<SwapChain*>(presentableImageCreateInfo.pSwapChain)->CreateInfo().flags.tmzProtected;

    createInfo.size               = memReqs.size;
    createInfo.alignment          = memReqs.alignment;
    createInfo.vaRange            = VaRange::Default;
    createInfo.priority           = GpuMemPriority::VeryHigh;
    createInfo.heapCount          = 0;
    createInfo.pImage             = pImage;

    for (uint32 i = 0; i < memReqs.heapCount; i++)
    {
        // Don't allocate from local visible heap since the memory won't be mapped.
        if ((memReqs.heaps[i] != GpuHeapLocal) || (pDevice->HeapLogicalSize(GpuHeapInvisible) == 0))
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
Result Image::GetExternalSharedImageCreateInfo(
    const Device&                device,
    const ExternalImageOpenInfo& openInfo,
    const ExternalSharedInfo&    sharedInfo,
    ImageCreateInfo*             pCreateInfo)
{
    Result result = Result::Success;

    // Start with the caller's flags, we'll add some more later on.
    pCreateInfo->flags      = openInfo.flags;
    pCreateInfo->usageFlags = openInfo.usage;

    const bool hasMetadata = sharedInfo.info.metadata.size_metadata > 0;

    // Most information will come directly from the base subresource's surface description.
    const auto* pMetadata = reinterpret_cast<const amdgpu_bo_umd_metadata*>(
        &sharedInfo.info.metadata.umd_metadata[PRO_UMD_METADATA_OFFSET_DWORD]);

    bool changeFormat = false;
    SwizzledFormat formatInMetadata = UndefinedSwizzledFormat;
    if (hasMetadata)
    {
        bool depthStencilUsage = false;
        formatInMetadata = AmdgpuFormatToPalFormat(pMetadata->format, &changeFormat, &depthStencilUsage);
        pCreateInfo->usageFlags.depthStencil = depthStencilUsage;
    }
    if (Formats::IsUndefined(openInfo.swizzledFormat.format))
    {
        if (hasMetadata)
        {
            pCreateInfo->swizzledFormat = formatInMetadata;
        }
        else
        {
            result = Result::ErrorFormatIncompatibleWithImageFormat;
        }
    }
    else
    {
        pCreateInfo->swizzledFormat = openInfo.swizzledFormat;
        if (hasMetadata && (formatInMetadata.format != openInfo.swizzledFormat.format))
        {
            changeFormat = true;
        }
    }

    if ((result == Result::Success) && changeFormat)
    {
        pCreateInfo->viewFormatCount = AllCompatibleFormats;
    }

    if ((result == Result::Success) && IsMesaMetadata(sharedInfo.info.metadata))
    {
        pCreateInfo->flags.sharedWithMesa = 1;
    }
    // If the width and height passed by pMetadata is not the same as expected, the buffer may still be valid:
    // E.g. Planar YUV images are allocated as a single block of memory and passed in by one handle. We can not
    //      figure out the width and height settled in Metadata points to which plane or maybe it just means the
    //      whole image size. A more robost method is to use the dedicated image's extent from client side as
    //      createinfo to initialize the subresources for each plane.
    if ((result == Result::Success) &&
        (openInfo.extent.width != 0) && (openInfo.extent.height != 0) && (openInfo.extent.depth != 0) &&
        ((openInfo.extent.width != pMetadata->width_in_pixels) || (openInfo.extent.height != pMetadata->height)))
    {
        if (Formats::IsYuv(pCreateInfo->swizzledFormat.format) ||
            // In VDPAU case(metadata comes from mesa), we need to update width/height/depth accordingly,
            // which were acquired from vdpau handle and passed by openInfo.extent
            pCreateInfo->flags.sharedWithMesa                  ||
            // If the bo has different importing format than the format in Metadata, width/height are taken from
            // openInfo to match the new format view.
            changeFormat                                       ||
            // If the bo is shared from another device, it would not have metadata. Width/height/depth are passed
            // from application.
            (hasMetadata == false))
        {
            pCreateInfo->extent.width = openInfo.extent.width;
            pCreateInfo->extent.height = openInfo.extent.height;
            pCreateInfo->extent.depth  = openInfo.extent.depth;
        }
        else
        {
            // The dimensions of imported image is smaller than the internal one.
            // Reject this import as it may leads to unexpected results.
            result = Result::ErrorInvalidExternalHandle;
        }
    }
    else
    {
        pCreateInfo->extent.width  = pMetadata->width_in_pixels;
        pCreateInfo->extent.height = pMetadata->height;
        pCreateInfo->extent.depth  = pMetadata->depth;
    }

    if (result == Result::Success)
    {
        if (hasMetadata && (pCreateInfo->flags.sharedWithMesa == 0))
        {
            pCreateInfo->imageType = static_cast<ImageType>(pMetadata->flags.resource_type);
        }
        else
        {
            pCreateInfo->imageType = ImageType::Tex2d;
        }

        if (hasMetadata)
        {
            bool isLinearTiled = false;
            if (device.ChipProperties().gfxLevel >= GfxIpLevel::GfxIp9)
            {
                const AMDGPU_SWIZZLE_MODE swizzleMode =
                    (pCreateInfo->flags.sharedWithMesa
                        ? static_cast<AMDGPU_SWIZZLE_MODE>(
                          AMDGPU_TILING_GET(sharedInfo.info.metadata.swizzle_info, SWIZZLE_MODE))
                        : pMetadata->swizzleMode);

                isLinearTiled = (swizzleMode == AMDGPU_SWIZZLE_MODE_LINEAR) ||
                                (swizzleMode == AMDGPU_SWIZZLE_MODE_LINEAR_GENERAL);
            }
            else
            {
                const uint32 tileMode =
                    (pCreateInfo->flags.sharedWithMesa
                        ? static_cast<uint32>(AddrMgr1::AddrTileModeFromHwArrayMode(
                          AMDGPU_TILING_GET(sharedInfo.info.metadata.tiling_info, ARRAY_MODE)))
                        : pMetadata->tile_mode);

                isLinearTiled = (tileMode == AMDGPU_TILE_MODE__LINEAR_GENERAL) ||
                                (tileMode == AMDGPU_TILE_MODE__LINEAR_ALIGNED);
            }
            pCreateInfo->tiling = isLinearTiled ? ImageTiling::Linear : ImageTiling::Optimal;
        }
        else
        {
            pCreateInfo->tiling = ImageTiling::Linear;
        }

        if (hasMetadata)
        {
            // For the bo created by other driver(display), the miplevels and
            // arraySize might not be initialized as 1, which will cause seqfault,
            // set the default value as 1 here to provide the robustness when mipLevels and
            // arraySize are zero
            pCreateInfo->mipLevels = Util::Max(1u, static_cast<uint32>(pMetadata->flags.mip_levels));
            pCreateInfo->arraySize = Util::Max(1u, static_cast<uint32>(pMetadata->array_size));

            pCreateInfo->samples   = Util::Max(1u, static_cast<uint32>(pMetadata->flags.samples));
            pCreateInfo->flags.cubemap = (pMetadata->flags.cubemap != 0);

            // OR-in some additional usage flags.
            pCreateInfo->usageFlags.shaderRead   |= pMetadata->flags.texture;
            pCreateInfo->usageFlags.shaderWrite  |= pMetadata->flags.unodered_access;
            pCreateInfo->usageFlags.colorTarget  |= pMetadata->flags.render_target;
            pCreateInfo->usageFlags.depthStencil |= pMetadata->flags.depth_stencil;

            pCreateInfo->flags.optimalShareable = pMetadata->flags.optimal_shareable;
            pCreateInfo->flags.presentable      = ((device.ChipProperties().gfxLevel >= GfxIpLevel::GfxIp9) &&
                                                   (pCreateInfo->flags.hasModifier == 0))
                                                    ? AMDGPU_TILING_GET(sharedInfo.info.metadata.tiling_info, SCANOUT)
                                                    : false;
            pCreateInfo->flags.flippable        = pCreateInfo->flags.presentable;
        }
        else
        {
            pCreateInfo->mipLevels  = 1u;
            pCreateInfo->arraySize  = 1u;
            pCreateInfo->samples    = 1u;
        }

        pCreateInfo->fragments = pCreateInfo->samples;
        // This image must be shareable (as it has already been shared); request view format change as well to be safe.
        pCreateInfo->flags.shareable = 1;
        pCreateInfo->viewFormatCount = AllCompatibleFormats;
    }

    return result;
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
    const bool  hasMetadata = sharedInfo.info.metadata.size_metadata > 0;
    const auto* pMetadata = reinterpret_cast<const amdgpu_bo_umd_metadata*>(
        &sharedInfo.info.metadata.umd_metadata[PRO_UMD_METADATA_OFFSET_DWORD]);

    ImageInternalCreateInfo internalCreateInfo = {};
    if (chipProps.gfxLevel >= GfxIpLevel::GfxIp9)
    {
        if (hasMetadata == false)
        {
            internalCreateInfo.gfx9.sharedSwizzleMode = ADDR_SW_LINEAR;
        }
        else
        {
            const uint64 tilingInfo = sharedInfo.info.metadata.tiling_info;

            if (IsMesaMetadata(sharedInfo.info.metadata))
            {
                internalCreateInfo.gfx9.sharedSwizzleMode = static_cast<AddrSwizzleMode>
                    (AMDGPU_TILING_GET(sharedInfo.info.metadata.swizzle_info, SWIZZLE_MODE));
            }
            else
            {
                internalCreateInfo.gfx9.sharedPipeBankXor[0] = pMetadata->pipeBankXor;
                for (uint32 plane = 1; plane < MaxNumPlanes; plane++)
                {
                    internalCreateInfo.gfx9.sharedPipeBankXor[plane] = pMetadata->additionalPipeBankXor[plane - 1];
                }

                internalCreateInfo.gfx9.sharedSwizzleMode = static_cast<AddrSwizzleMode>(pMetadata->swizzleMode);
                PAL_ASSERT(AMDGPU_TILING_GET(tilingInfo, SWIZZLE_MODE) == pMetadata->swizzleMode);
            }

            // ADDR_SW_LINEAR_GENERAL is a UBM compatible swizzle mode which treat as buffer in copy.
            // Here we try ADDR_SW_LINEAR first and fall back to typed buffer path if failure the
            // creation as PAL::image.
            if (internalCreateInfo.gfx9.sharedSwizzleMode == ADDR_SW_LINEAR_GENERAL)
            {
                internalCreateInfo.gfx9.sharedSwizzleMode = ADDR_SW_LINEAR;
            }

            internalCreateInfo.flags.useSharedDccState = 1;

            DccState*    pDccState  = &internalCreateInfo.gfx9.sharedDccState;

            pDccState->maxCompressedBlockSize   = AMDGPU_TILING_GET(tilingInfo, DCC_MAX_COMPRESSED_BLOCK_SIZE);
            pDccState->maxUncompressedBlockSize = AMDGPU_TILING_GET(tilingInfo, DCC_MAX_UNCOMPRESSED_BLOCK_SIZE);
            pDccState->independentBlk64B        = AMDGPU_TILING_GET(tilingInfo, DCC_INDEPENDENT_64B);
            pDccState->independentBlk128B       = AMDGPU_TILING_GET(tilingInfo, DCC_INDEPENDENT_128B);
        }
    }
    else
    {
        if (hasMetadata == false)
        {
            internalCreateInfo.gfx6.sharedTileMode  = ADDR_TM_LINEAR_GENERAL;
            internalCreateInfo.gfx6.sharedTileType  = ADDR_DISPLAYABLE;
            internalCreateInfo.gfx6.sharedTileIndex = TILEINDEX_LINEAR_ALIGNED;
        }
        else if (IsMesaMetadata(sharedInfo.info.metadata))
        {
            auto*const pBoMetaData  = reinterpret_cast<const amdgpu_bo_umd_metadata*>
                                                        (&sharedInfo.info.metadata.umd_metadata);
            auto*const pRawMetaData = reinterpret_cast<const uint32*>(pBoMetaData);

            internalCreateInfo.gfx6.sharedTileIndex = (pRawMetaData[5] >> 20) & 0x1F;
            internalCreateInfo.gfx6.sharedTileMode  = AddrMgr1::AddrTileModeFromHwArrayMode(
                                                   AMDGPU_TILING_GET(sharedInfo.info.metadata.tiling_info, ARRAY_MODE));
            internalCreateInfo.gfx6.sharedTileType  = static_cast<AddrTileType>
                                             (AMDGPU_TILING_GET(sharedInfo.info.metadata.tiling_info, MICRO_TILE_MODE));
        }
        else
        {
            internalCreateInfo.gfx6.sharedTileMode       = static_cast<AddrTileMode>
                                                           (AmdGpuToAddrTileModeConversion(pMetadata->tile_mode));
            internalCreateInfo.gfx6.sharedTileType       = static_cast<AddrTileType>(pMetadata->micro_tile_mode);
            internalCreateInfo.gfx6.sharedTileSwizzle[0] = pMetadata->pipeBankXor;

            for (uint32 plane = 1; plane < MaxNumPlanes; plane++)
            {
                internalCreateInfo.gfx6.sharedTileSwizzle[plane] = pMetadata->additionalPipeBankXor[plane - 1];
            }

            internalCreateInfo.gfx6.sharedTileIndex = pMetadata->tile_index;
        }
    }

    internalCreateInfo.flags.privateScreenPresent     = (pPrivateScreen != nullptr);
    internalCreateInfo.flags.useSharedTilingOverrides = 1;

    ImageCreateInfo createInfo = {};
    Result result = GetExternalSharedImageCreateInfo(*pDevice, openInfo, sharedInfo, &createInfo);

    if (result == Result::Success)
    {

        if (hasMetadata && pMetadata->flags.optimal_shareable)
        {
            auto*const pUmdSharedMetadata =
                reinterpret_cast<const amdgpu_shared_metadata_info*>
                (&pMetadata->shared_metadata_info);
            internalCreateInfo.flags.useSharedMetadata = 1;

            internalCreateInfo.sharedMetadata.numPlanes = 1;

            internalCreateInfo.sharedMetadata.dccOffset[0] = pUmdSharedMetadata->dcc_offset;
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
            internalCreateInfo.sharedMetadata.flags.hasCmaskEqGpuAccess =
                pUmdSharedMetadata->flags.has_cmask_eq_gpu_access;
            internalCreateInfo.sharedMetadata.flags.hasHtileLookupTable =
                pUmdSharedMetadata->flags.has_htile_lookup_table;
            internalCreateInfo.sharedMetadata.flags.htileHasDsMetadata =
                pUmdSharedMetadata->flags.htile_has_ds_metadata;

            internalCreateInfo.sharedMetadata.fastClearMetaDataOffset[0] =
                pUmdSharedMetadata->fast_clear_value_offset;
            internalCreateInfo.sharedMetadata.fastClearEliminateMetaDataOffset[0] =
                pUmdSharedMetadata->fce_state_offset;

            // The offset here will be updated once change of amdgpu_shared_metadata_info is done.
            internalCreateInfo.sharedMetadata.hisPretestMetaDataOffset = 0;

            if (pUmdSharedMetadata->dcc_offset != 0)
            {
                internalCreateInfo.sharedMetadata.dccStateMetaDataOffset[0] =
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

            internalCreateInfo.sharedMetadata.resourceId       = Uint64CombineParts(pUmdSharedMetadata->resource_id,
                pUmdSharedMetadata->resource_id_high32);
            internalCreateInfo.sharedMetadata.fmaskSwizzleMode = static_cast<AddrSwizzleMode>
                (pUmdSharedMetadata->fmaskSwizzleMode);

            createInfo.flags.optimalShareable = 1;
        }
        else
        {
            createInfo.flags.optimalShareable = 0;
            createInfo.metadataMode           = MetadataMode::Disabled;
            createInfo.metadataTcCompatMode   = MetadataTcCompatMode::Disabled;
        }
    }
    if (IsMesaMetadata(sharedInfo.info.metadata))
    {
        const auto*const pMesaUmdMetaData = reinterpret_cast<const MesaUmdMetaData*>(&sharedInfo.info.metadata.umd_metadata[0]);
        // from mesa function si_set_mutable_tex_desc_fields, only when dcc enable or hile enable, compressionEnable bit
        // will be set as 1
        if (pMesaUmdMetaData->imageSrd.gfx10.compressionEnable == 1)
        {
            // According to Mesa3D metadata encoding function si_set_tex_bo_metadata,
            // Mesa3D share standard dcc metadata though first 10 dwords of umd_metadata of struct amdgpu_bo_metadata.
            internalCreateInfo.sharedMetadata.dccOffset[0] = (pMesaUmdMetaData->imageSrd.gfx10.metaDataOffset) << 8;
            internalCreateInfo.flags.useSharedMetadata = 1;
            createInfo.flags.optimalShareable = 1;
            internalCreateInfo.sharedMetadata.numPlanes = 1;
            internalCreateInfo.sharedMetadata.flags.shaderFetchable = 1;
            internalCreateInfo.sharedMetadata.pipeAligned[0] = 1;
            createInfo.metadataMode           = MetadataMode::Default;
            createInfo.metadataTcCompatMode   = MetadataTcCompatMode::Default;
        }
    }

    if (openInfo.flags.hasModifier != 0)
    {
        pDevice->GetModifierInfo(openInfo.modifier, &createInfo, &internalCreateInfo);

        internalCreateInfo.sharedMetadata.dccOffset[0]        = openInfo.dccOffset;
        internalCreateInfo.sharedMetadata.displayDccOffset[0] = openInfo.displayDccOffset;
    }

    Pal::Image* pImage = nullptr;
    if (result == Result::Success)
    {
        result = pDevice->CreateInternalImage(createInfo,
                                             internalCreateInfo,
                                             pImagePlacementAddr,
                                             &pImage);
    }

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
                                                           openInfo,
                                                           sharedInfo,
                                                           pGpuMemoryPlacementAddr,
                                                           &memCreateInfo,
                                                           &pGpuMemory);
    }

    if (result == Result::Success)
    {
        result = pImage->BindGpuMemory(pGpuMemory,
                                       openInfo.gpuMemOffset
                                      );
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

// =====================================================================================================================
// Set the idle status of the image and dirty global references.
void Image::SetIdle(
    bool idle)
{
    if (m_idle != idle)
    {
        m_idle = idle;

        auto*const pAmdgpuDevice = static_cast<Amdgpu::Device*>(m_pDevice);

        pAmdgpuDevice->DirtyGlobalReferences();
    }
}

// =====================================================================================================================
// Fills in the SubresLayout struct with info for the image with drm format modifier.
Result Image::GetModifierSubresourceLayout(
    uint32        memoryPlane,
    SubresLayout* pLayout
    ) const
{
    Result ret = Result::ErrorInvalidValue;

    DccState dccState = {};

    if (pLayout != nullptr)
    {
        switch (memoryPlane)
        {
        // Order of memory plane subresource layout is defined by drm_fourcc.h.
        case 0:
            // The main surface layout is already obtained from GetSubresourceLayout().
            PAL_ASSERT_ALWAYS();
            break;
        case 1:
            if (GetGfxImage()->HasDisplayDccData())
            {
                GetGfxImage()->GetDisplayDccState(&dccState);
            }
            else
            {
                GetGfxImage()->GetDccState(&dccState);
            }
            pLayout->offset     = dccState.primaryOffset;
            pLayout->size       = dccState.size;
            pLayout->rowPitch   = dccState.pitch;
            break;
        case 2:
            GetGfxImage()->GetDccState(&dccState);
            pLayout->offset     = dccState.primaryOffset;
            pLayout->size       = dccState.size;
            pLayout->rowPitch   = dccState.pitch;
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }

        if (pLayout->size != 0)
        {
            ret = Result::Success;
        }
    }

    return ret;
}

} // Linux
} // Pal
