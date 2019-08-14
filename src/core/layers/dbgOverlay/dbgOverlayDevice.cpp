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

#include "core/layers/dbgOverlay/dbgOverlayCmdBuffer.h"
#include "core/layers/dbgOverlay/dbgOverlayDevice.h"
#include "core/layers/dbgOverlay/dbgOverlayImage.h"
#include "core/layers/dbgOverlay/dbgOverlayQueue.h"
#include "core/layers/dbgOverlay/dbgOverlayTextWriter.h"
#include "core/layers/dbgOverlay/dbgOverlayTimeGraph.h"
#include "palFormatInfo.h"

using namespace Util;

namespace Pal
{
namespace DbgOverlay
{

// =====================================================================================================================
Device::Device(
    PlatformDecorator* pPlatform,
    IDevice*           pNextDevice)
    :
    DeviceDecorator(pPlatform, pNextDevice),
    m_pSettings(pNextDevice->GetPublicSettings()),
    m_pCmdAllocator(nullptr),
    m_pTextWriter(nullptr),
    m_pTimeGraph(nullptr),
    m_maxSrdSize(0)
{
    memset(&m_memHeapProps[0], 0, sizeof(m_memHeapProps));

    // Note that this is just to depress buffer overrun warning/error caught by static code analysis, i.e., Buffer
    // overrun while writing to 'm_vidMemTotals[0]':  the writable size is '32' bytes, but '128' bytes might be written.
    PAL_ANALYSIS_ASSUME(sizeof(m_vidMemTotals) <= (sizeof(gpusize) * AllocTypeCount * GpuHeapCount));
    memset(const_cast<gpusize (&)[AllocTypeCount][GpuHeapCount]>(m_vidMemTotals), 0, sizeof(m_vidMemTotals));

    memset(&m_perHeapMemTotals[0], 0, sizeof(m_perHeapMemTotals));
    memset(&m_peakVidMemTotals[0], 0, sizeof(m_peakVidMemTotals));
}

// =====================================================================================================================
Result Device::Finalize(
    const DeviceFinalizeInfo& finalizeInfo)
{
    Result result = DeviceDecorator::Finalize(finalizeInfo);

    if (result == Result::Success)
    {
        result = GetGpuMemoryHeapProperties(m_memHeapProps);
    }

    if (result == Result::Success)
    {
        result = GetProperties(&m_gpuProps);

        // Determine the maximum SRD size.
        const auto& srdSizes = m_gpuProps.gfxipProperties.srdSizes;
        m_maxSrdSize         = Max(Max(srdSizes.bufferView, srdSizes.fmaskView),
                                   Max(srdSizes.imageView,  srdSizes.sampler));
    }

    if (result == Result::Success)
    {
        // We need a command allocator for the overlay's per-queue command buffers. These command buffers will be used
        // for text writer work so we can get away with small allocations and suballocations.
        //
        // We use about 4KB of embedded data for each present so 8KB suballocations are in order. If we ever switch to
        // byte-sized characters we could scale back to 4KB suballocations. 8 suballocations per allocation seems
        // reasonable; in almost all cases we wouldn't need more than one allocation for the whole overlay.
        constexpr uint32 CommandDataSuballocSize   = (1024 * 8);
        constexpr uint32 EmbeddedDataSuballocSize  = (1024 * 4);
        constexpr uint32 GpuScratchMemSuballocSize = (1024 * 4);

        CmdAllocatorCreateInfo createInfo = { };
        createInfo.flags.threadSafe      = 1;
        createInfo.flags.autoMemoryReuse = 1;
        createInfo.allocInfo[CommandDataAlloc].allocHeap      = GpuHeapGartCacheable;
        createInfo.allocInfo[CommandDataAlloc].suballocSize   = CommandDataSuballocSize;
        createInfo.allocInfo[CommandDataAlloc].allocSize      = (8 * CommandDataSuballocSize);
        createInfo.allocInfo[EmbeddedDataAlloc].allocHeap     = GpuHeapGartCacheable;
        createInfo.allocInfo[EmbeddedDataAlloc].suballocSize  = EmbeddedDataSuballocSize;
        createInfo.allocInfo[EmbeddedDataAlloc].allocSize     = (8 * EmbeddedDataSuballocSize);
        createInfo.allocInfo[GpuScratchMemAlloc].allocHeap    = GpuHeapInvisible;
        createInfo.allocInfo[GpuScratchMemAlloc].suballocSize = GpuScratchMemSuballocSize;
        createInfo.allocInfo[GpuScratchMemAlloc].allocSize    = (8 * GpuScratchMemSuballocSize);

        const size_t allocatorSize = GetCmdAllocatorSize(createInfo, &result);

        if (result == Result::Success)
        {
            void*const pAllocatorMem = PAL_MALLOC(allocatorSize, m_pPlatform, AllocInternal);
            if (pAllocatorMem != nullptr)
            {
                result = CreateCmdAllocator(createInfo,
                                            pAllocatorMem,
                                            reinterpret_cast<ICmdAllocator**>(&m_pCmdAllocator));
            }
            else
            {
                result = Result::ErrorOutOfMemory;
            }
        }
    }

    if (result == Result::Success)
    {
        m_pTextWriter = PAL_NEW(TextWriter, m_pPlatform, AllocInternal)(this);
        result        = (m_pTextWriter != nullptr) ? m_pTextWriter->Init() : Result::ErrorOutOfMemory;
    }

    if (result == Result::Success)
    {
        m_pTimeGraph = PAL_NEW(TimeGraph, m_pPlatform, AllocInternal)(this);
        result       = (m_pTimeGraph != nullptr) ? m_pTimeGraph->Init() : Result::ErrorOutOfMemory;
    }

    return result;
}

// =====================================================================================================================
Result Device::Cleanup()
{
    if (m_pTextWriter != nullptr)
    {
        PAL_SAFE_DELETE(m_pTextWriter, m_pPlatform);
    }

    if (m_pTimeGraph != nullptr)
    {
        PAL_SAFE_DELETE(m_pTimeGraph, m_pPlatform);
    }

    if (m_pCmdAllocator != nullptr)
    {
        m_pCmdAllocator->Destroy();
        PAL_SAFE_FREE(m_pCmdAllocator, m_pPlatform);
    }

    const Result result = DeviceDecorator::Cleanup();

    // If the user didn't delete everything these counts count get out of sync if this device is reused. We must do this
    // after calling the next decorator because we may attempt to free internally allocated GPU memory.
    //
    // Note that this is just to suppress buffer overrun warning/error caught by static code analysis, i.e., Buffer
    // overrun while writing to 'm_vidMemTotals[0]':  the writable size is '32' bytes, but '128' bytes might be written.
    PAL_ANALYSIS_ASSUME(sizeof(m_vidMemTotals) <= (sizeof(gpusize) * AllocTypeCount * GpuHeapCount));
    memset(const_cast<gpusize (&)[AllocTypeCount][GpuHeapCount]>(m_vidMemTotals), 0, sizeof(m_vidMemTotals));

    return result;
}

// =====================================================================================================================
size_t Device::GetQueueSize(
    const QueueCreateInfo& createInfo,
    Result*                pResult
    ) const
{
    return m_pNextLayer->GetQueueSize(createInfo, pResult) + sizeof(Queue);
}

// =====================================================================================================================
Result Device::CreateQueue(
    const QueueCreateInfo& createInfo,
    void*                  pPlacementAddr,
    IQueue**               ppQueue)
{
    IQueue* pNextQueue = nullptr;
    Queue*  pQueue     = nullptr;

    Result result = m_pNextLayer->CreateQueue(createInfo,
                                              NextObjectAddr<Queue>(pPlacementAddr),
                                              &pNextQueue);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextQueue != nullptr);
        pNextQueue->SetClientData(pPlacementAddr);

        pQueue = PAL_PLACEMENT_NEW(pPlacementAddr) Queue(pNextQueue, this, createInfo.queueType, createInfo.engineType);

        result = pQueue->Init();
    }

    if (result == Result::Success)
    {
        (*ppQueue) = pQueue;
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetCmdBufferSize(
    const CmdBufferCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    return m_pNextLayer->GetCmdBufferSize(nextCreateInfo, pResult) + sizeof(CmdBuffer);
}

// =====================================================================================================================
Result Device::CreateCmdBuffer(
    const CmdBufferCreateInfo& createInfo,
    void*                      pPlacementAddr,
    ICmdBuffer**               ppCmdBuffer)
{
    ICmdBuffer* pNextCmdBuffer = nullptr;

    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    Result result = m_pNextLayer->CreateCmdBuffer(nextCreateInfo,
                                                  NextObjectAddr<CmdBuffer>(pPlacementAddr),
                                                  &pNextCmdBuffer);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextCmdBuffer != nullptr);
        pNextCmdBuffer->SetClientData(pPlacementAddr);

        (*ppCmdBuffer) = PAL_PLACEMENT_NEW(pPlacementAddr) CmdBuffer(pNextCmdBuffer, this, createInfo.queueType);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetImageSize(
    const ImageCreateInfo& createInfo,
    Result*                pResult
    ) const
{
    return m_pNextLayer->GetImageSize(createInfo, pResult) + sizeof(Image);
}

// =====================================================================================================================
Result Device::CreateImage(
    const ImageCreateInfo& createInfo,
    void*                  pPlacementAddr,
    IImage**               ppImage)
{
    IImage* pNextImage = nullptr;

    Result result = m_pNextLayer->CreateImage(createInfo,
                                              NextObjectAddr<Image>(pPlacementAddr),
                                              &pNextImage);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextImage != nullptr);
        pNextImage->SetClientData(pPlacementAddr);

        (*ppImage) = PAL_PLACEMENT_NEW(pPlacementAddr) Image(pNextImage, this, createInfo);
    }

    return result;
}

// =====================================================================================================================
void Device::GetPresentableImageSizes(
    const PresentableImageCreateInfo& createInfo,
    size_t*                           pImageSize,
    size_t*                           pGpuMemorySize,
    Result*                           pResult
    ) const
{
    PresentableImageCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pScreen    = NextScreen(createInfo.pScreen);
    nextCreateInfo.pSwapChain = NextSwapChain(createInfo.pSwapChain);

    m_pNextLayer->GetPresentableImageSizes(nextCreateInfo, pImageSize, pGpuMemorySize, pResult);
    (*pImageSize)     += sizeof(Image);
    (*pGpuMemorySize) += sizeof(GpuMemoryDecorator);
}

// =====================================================================================================================
// Converts PresentableImageCreateInfo to ImageCreateInfo
static void ConvertPresentableImageCreateInfo(
    const PresentableImageCreateInfo& in,
    ImageCreateInfo*                  pOut)
{
    pOut->swizzledFormat        = in.swizzledFormat;
    pOut->extent.width          = in.extent.width;
    pOut->extent.height         = in.extent.height;
    pOut->extent.depth          = 1;
    pOut->arraySize             = 1;
    pOut->mipLevels             = 1;
    pOut->samples               = 1;
    pOut->imageType             = ImageType::Tex2d;
    pOut->tiling                = ImageTiling::Optimal;
    pOut->usageFlags            = in.usage;
    pOut->viewFormatCount       = in.viewFormatCount;
    pOut->pViewFormats          = in.pViewFormats;
}

// =====================================================================================================================
Result Device::CreatePresentableImage(
    const PresentableImageCreateInfo& createInfo,
    void*                             pImagePlacementAddr,
    void*                             pGpuMemoryPlacementAddr,
    IImage**                          ppImage,
    IGpuMemory**                      ppGpuMemory)
{
    IImage*     pNextImage     = nullptr;
    IGpuMemory* pNextGpuMemory = nullptr;

    PresentableImageCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pScreen                    = NextScreen(createInfo.pScreen);
    nextCreateInfo.pSwapChain                 = NextSwapChain(createInfo.pSwapChain);

    Result result =
        m_pNextLayer->CreatePresentableImage(nextCreateInfo,
                                             NextObjectAddr<Image>(pImagePlacementAddr),
                                             NextObjectAddr<GpuMemoryDecorator>(pGpuMemoryPlacementAddr),
                                             &pNextImage,
                                             &pNextGpuMemory);

    if ((result == Result::Success) || (result == Result::TooManyFlippableAllocations))
    {
        pNextImage->SetClientData(pImagePlacementAddr);
        pNextGpuMemory->SetClientData(pGpuMemoryPlacementAddr);

        ImageCreateInfo imageInfo = {};
        ConvertPresentableImageCreateInfo(nextCreateInfo, &imageInfo);

        Image* pImage  = PAL_PLACEMENT_NEW(pImagePlacementAddr) Image(pNextImage, this, imageInfo);
        (*ppImage)     = pImage;
        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pGpuMemoryPlacementAddr) GpuMemoryDecorator(pNextGpuMemory, this);

        pImage->SetBoundGpuMemory(*ppGpuMemory, 0);
    }

    return result;
}

// =====================================================================================================================
// Get the image size, memory size and the create info of image from an external shared image
Result Device::GetExternalSharedImageSizes(
    const ExternalImageOpenInfo& openInfo,
    size_t*                      pImageSize,
    size_t*                      pGpuMemorySize,
    ImageCreateInfo*             pImgCreateInfo
    ) const
{
    Result result = m_pNextLayer->GetExternalSharedImageSizes(openInfo, pImageSize, pGpuMemorySize, pImgCreateInfo);

    (*pImageSize)     += sizeof(Image);
    (*pGpuMemorySize) += sizeof(GpuMemoryDecorator);

    return result;
}

// =====================================================================================================================
// Opens shared image from anyone except another PAL device in the same LDA chain.
Result Device::OpenExternalSharedImage(
    const ExternalImageOpenInfo& openInfo,
    void*                        pImagePlacementAddr,
    void*                        pGpuMemoryPlacementAddr,
    GpuMemoryCreateInfo*         pMemCreateInfo,
    IImage**                     ppImage,
    IGpuMemory**                 ppGpuMemory)
{
    IImage*     pNextImage = nullptr;
    IGpuMemory* pNextGpuMemory = nullptr;

    ExternalImageOpenInfo nextOpenInfo = openInfo;
    nextOpenInfo.pScreen = NextPrivateScreen(openInfo.pScreen);
    Result result = m_pNextLayer->OpenExternalSharedImage(nextOpenInfo,
                                                          NextObjectAddr<Image>(pImagePlacementAddr),
                                                          NextObjectAddr<GpuMemoryDecorator>(pGpuMemoryPlacementAddr),
                                                          pMemCreateInfo,
                                                          &pNextImage,
                                                          &pNextGpuMemory);

    if (result == Result::Success)
    {
        pNextImage->SetClientData(pImagePlacementAddr);
        pNextGpuMemory->SetClientData(pGpuMemoryPlacementAddr);

        const ImageCreateInfo& imageCreateInfo = pNextImage->GetImageCreateInfo();
        Image* pImage  = PAL_PLACEMENT_NEW(pImagePlacementAddr) Image(pNextImage, this, imageCreateInfo);
        (*ppImage)     = pImage;
        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pGpuMemoryPlacementAddr) GpuMemoryDecorator(pNextGpuMemory, this);

        pImage->SetBoundGpuMemory(*ppGpuMemory, 0);
    }

    return result;
}

// =====================================================================================================================
void Device::GetPrivateScreenImageSizes(
    const PrivateScreenImageCreateInfo& createInfo,
    size_t*                             pImageSize,
    size_t*                             pGpuMemorySize,
    Result*                             pResult
    ) const
{
    PrivateScreenImageCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pScreen = NextPrivateScreen(createInfo.pScreen);

    m_pNextLayer->GetPrivateScreenImageSizes(nextCreateInfo, pImageSize, pGpuMemorySize, pResult);
    (*pImageSize)     += sizeof(Image);
    (*pGpuMemorySize) += sizeof(GpuMemoryDecorator);
}

// =====================================================================================================================
Result Device::CreatePrivateScreenImage(
    const PrivateScreenImageCreateInfo& createInfo,
    void*                               pImagePlacementAddr,
    void*                               pGpuMemoryPlacementAddr,
    IImage**                            ppImage,
    IGpuMemory**                        ppGpuMemory)
{
    IImage*     pNextImage     = nullptr;
    IGpuMemory* pNextGpuMemory = nullptr;

    PrivateScreenImageCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pScreen = NextPrivateScreen(createInfo.pScreen);

    Result result =
        m_pNextLayer->CreatePrivateScreenImage(nextCreateInfo,
        NextObjectAddr<Image>(pImagePlacementAddr),
        NextObjectAddr<GpuMemoryDecorator>(pGpuMemoryPlacementAddr),
        &pNextImage,
        &pNextGpuMemory);

    ImageCreateInfo imgCreateInfo = { 0 };
    imgCreateInfo.extent.depth   = 1;
    imgCreateInfo.extent.width   = createInfo.extent.width;
    imgCreateInfo.extent.height  = createInfo.extent.height;
    imgCreateInfo.arraySize      = 1;
    imgCreateInfo.swizzledFormat = createInfo.swizzledFormat;

    if (result == Result::Success)
    {
        pNextImage->SetClientData(pImagePlacementAddr);
        pNextGpuMemory->SetClientData(pGpuMemoryPlacementAddr);
        (*ppImage) = PAL_PLACEMENT_NEW(pImagePlacementAddr) Image(pNextImage, this, imgCreateInfo);
        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pGpuMemoryPlacementAddr) GpuMemoryDecorator(pNextGpuMemory, this);
    }

    return result;
}

} // DbgOverlay
} // Pal
