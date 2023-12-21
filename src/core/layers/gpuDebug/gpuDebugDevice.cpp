/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_DEVELOPER_BUILD

#include "core/layers/gpuDebug/gpuDebugCmdBuffer.h"
#include "core/layers/gpuDebug/gpuDebugColorBlendState.h"
#include "core/layers/gpuDebug/gpuDebugColorTargetView.h"
#include "core/layers/gpuDebug/gpuDebugDepthStencilView.h"
#include "core/layers/gpuDebug/gpuDebugDevice.h"
#include "core/layers/gpuDebug/gpuDebugImage.h"
#include "core/layers/gpuDebug/gpuDebugPipeline.h"
#include "core/layers/gpuDebug/gpuDebugPlatform.h"
#include "core/layers/gpuDebug/gpuDebugQueue.h"
#include "palSysUtil.h"

using namespace Util;

namespace Pal
{
namespace GpuDebug
{

// =====================================================================================================================
bool Device::SupportsCommentString(
    uint32                 queueCount,
    const QueueCreateInfo* pCreateInfo)
{
    bool anySupport = false;

    for (uint32 i = 0; i < queueCount; i++)
    {
        anySupport |= SupportsCommentString(pCreateInfo[i].queueType);
        break;
    }

    return anySupport;
}

// =====================================================================================================================
Device::Device(
    PlatformDecorator* pPlatform,
    IDevice*           pNextDevice)
    :
    DeviceDecorator(pPlatform, pNextDevice),
    m_colorViewSize(0),
    m_depthViewSize(0),
    m_pPublicSettings(nullptr),
    m_deviceProperties{},
    m_initialized(false)
{
}

// =====================================================================================================================
Device::~Device()
{
}

// =====================================================================================================================
Result Device::CommitSettingsAndInit()
{
    Result result = DeviceDecorator::CommitSettingsAndInit();

    if (result == Result::Success)
    {
        m_colorViewSize = GetColorTargetViewSize(&result);
    }

    if (result == Result::Success)
    {
        m_depthViewSize = GetDepthStencilViewSize(&result);
    }

    m_pPublicSettings = GetNextLayer()->GetPublicSettings();

    if (result == Result::Success)
    {
        m_colorViewSize = GetColorTargetViewSize(&result);
    }

    if (result == Result::Success)
    {
        m_depthViewSize = GetDepthStencilViewSize(&result);
    }

    return result;
}

// =====================================================================================================================
Result Device::Finalize(
    const DeviceFinalizeInfo& finalizeInfo)
{
    Result result = DeviceDecorator::Finalize(finalizeInfo);

    if (result == Result::Success)
    {
        result = GetProperties(&m_deviceProperties);
    }

    if (result == Result::Success)
    {
        m_initialized = true;
    }

    return result;
}

// =====================================================================================================================
Result Device::Cleanup()
{
    if (m_initialized)
    {
    }

    return DeviceDecorator::Cleanup();
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
    ICmdBuffer* pCmdBuffer     = nullptr;

    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    Result result = m_pNextLayer->CreateCmdBuffer(nextCreateInfo,
                                                  NextObjectAddr<CmdBuffer>(pPlacementAddr),
                                                  &pNextCmdBuffer);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextCmdBuffer != nullptr);

        pCmdBuffer = PAL_PLACEMENT_NEW(pPlacementAddr) CmdBuffer(pNextCmdBuffer, this, createInfo);
        result     = static_cast<CmdBuffer*>(pCmdBuffer)->Init();

        if (result == Result::Success)
        {
            pNextCmdBuffer->SetClientData(pPlacementAddr);
            (*ppCmdBuffer) = pCmdBuffer;
        }
        else
        {
            pCmdBuffer->Destroy();
        }
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetTargetCmdBufferSize(
    const CmdBufferCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    return m_pNextLayer->GetCmdBufferSize(nextCreateInfo, pResult) + sizeof(TargetCmdBuffer);
}

// =====================================================================================================================
Result Device::CreateTargetCmdBuffer(
    const CmdBufferCreateInfo& createInfo,
    void*                      pPlacementAddr,
    TargetCmdBuffer**          ppCmdBuffer)
{
    ICmdBuffer*      pNextCmdBuffer = nullptr;
    TargetCmdBuffer* pCmdBuffer     = nullptr;

    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    Result result = m_pNextLayer->CreateCmdBuffer(nextCreateInfo,
                                                  NextObjectAddr<TargetCmdBuffer>(pPlacementAddr),
                                                  &pNextCmdBuffer);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextCmdBuffer != nullptr);
        pNextCmdBuffer->SetClientData(pPlacementAddr);

        pCmdBuffer = PAL_PLACEMENT_NEW(pPlacementAddr) TargetCmdBuffer(createInfo, pNextCmdBuffer, this);
        result = pCmdBuffer->Init();

        if (result == Result::Success)
        {
            (*ppCmdBuffer) = pCmdBuffer;
        }
        else
        {
            pCmdBuffer->Destroy();
        }
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetGraphicsPipelineSize(
    const GraphicsPipelineCreateInfo& createInfo,
    Result*                           pResult
    ) const
{
    return m_pNextLayer->GetGraphicsPipelineSize(createInfo, pResult) + sizeof(Pipeline);
}

// =====================================================================================================================
Result Device::CreateGraphicsPipeline(
    const GraphicsPipelineCreateInfo& createInfo,
    void*                             pPlacementAddr,
    IPipeline**                       ppPipeline)
{
    IPipeline* pNextPipeline = nullptr;
    Pipeline* pPipeline = nullptr;
    Result result = CallNextCreateGraphicsPipeline(createInfo,
                                                   NextObjectAddr<Pipeline>(pPlacementAddr),
                                                   &pNextPipeline);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextPipeline != nullptr);
        pNextPipeline->SetClientData(pPlacementAddr);

        pPipeline = PAL_PLACEMENT_NEW(pPlacementAddr) Pipeline(pNextPipeline, createInfo, this);
        (*ppPipeline) = pPipeline;
    }

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
    IQueue* pQueue = nullptr;

    Result result = m_pNextLayer->CreateQueue(createInfo,
                                              VoidPtrInc(pPlacementAddr, sizeof(Queue)),
                                              &pNextQueue);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextQueue != nullptr);

        pQueue = PAL_PLACEMENT_NEW(pPlacementAddr) Queue(pNextQueue, this, 1);
        result = static_cast<Queue*>(pQueue)->Init(&createInfo);

        if (result == Result::Success)
        {
            pNextQueue->SetClientData(pPlacementAddr);
            (*ppQueue) = pQueue;
        }
        else
        {
            pQueue->Destroy();
        }
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetMultiQueueSize(
    uint32                 queueCount,
    const QueueCreateInfo* pCreateInfo,
    Result*                pResult
    ) const
{
    return m_pNextLayer->GetMultiQueueSize(queueCount, pCreateInfo, pResult) + sizeof(Queue);
}

// =====================================================================================================================
Result Device::CreateMultiQueue(
    uint32                 queueCount,
    const QueueCreateInfo* pCreateInfo,
    void*                  pPlacementAddr,
    IQueue**               ppQueue)
{
    IQueue* pNextQueue = nullptr;
    IQueue* pQueue = nullptr;

    Result result = m_pNextLayer->CreateMultiQueue(queueCount,
                                                   pCreateInfo,
                                                   VoidPtrInc(pPlacementAddr, sizeof(Queue)),
                                                   &pNextQueue);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextQueue != nullptr);

        pQueue = PAL_PLACEMENT_NEW(pPlacementAddr) Queue(pNextQueue, this, queueCount);
        result = static_cast<Queue*>(pQueue)->Init(pCreateInfo);

        if (result == Result::Success)
        {
            pNextQueue->SetClientData(pPlacementAddr);
            (*ppQueue) = pQueue;
        }
        else
        {
            pQueue->Destroy();
        }
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

        (*ppImage) = PAL_PLACEMENT_NEW(pPlacementAddr) Image(pNextImage, createInfo.swizzledFormat, this);
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
Result Device::CreatePresentableImage(
    const PresentableImageCreateInfo& createInfo,
    void*                             pImagePlacementAddr,
    void*                             pGpuMemoryPlacementAddr,
    IImage**                          ppImage,
    IGpuMemory**                      ppGpuMemory)
{
    IImage*     pNextImage     = nullptr;
    IGpuMemory* pNextGpuMemory = nullptr;

    // In order to be able to overlay debug information on presentable images, the images must also be ShaderWrite.
    PresentableImageCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pScreen                    = NextScreen(createInfo.pScreen);
    nextCreateInfo.pSwapChain                 = NextSwapChain(createInfo.pSwapChain);

    nextCreateInfo.usage.shaderWrite = 1;

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

        Image* pImage  = PAL_PLACEMENT_NEW(pImagePlacementAddr) Image(pNextImage, createInfo.swizzledFormat, this);
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

        Image* pImage  = PAL_PLACEMENT_NEW(pImagePlacementAddr) Image(pNextImage, openInfo.swizzledFormat, this);
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

    Result result = m_pNextLayer->CreatePrivateScreenImage(nextCreateInfo,
                                                           NextObjectAddr<Image>(pImagePlacementAddr),
                                                           NextObjectAddr<GpuMemoryDecorator>(pGpuMemoryPlacementAddr),
                                                           &pNextImage,
                                                           &pNextGpuMemory);
    if (result == Result::Success)
    {
        pNextImage->SetClientData(pImagePlacementAddr);
        pNextGpuMemory->SetClientData(pGpuMemoryPlacementAddr);
        Image* pImage  = PAL_PLACEMENT_NEW(pImagePlacementAddr) Image(pNextImage, createInfo.swizzledFormat, this);
        (*ppImage)     = pImage;
        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pGpuMemoryPlacementAddr) GpuMemoryDecorator(pNextGpuMemory, this);

        pImage->SetBoundGpuMemory(*ppGpuMemory, 0);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetColorTargetViewSize(
    Result* pResult
    ) const
{
    return m_pNextLayer->GetColorTargetViewSize(pResult) + sizeof(ColorTargetView);
}

// =====================================================================================================================
Result Device::CreateColorTargetView(
    const ColorTargetViewCreateInfo& createInfo,
    void*                            pPlacementAddr,
    IColorTargetView**               ppColorTargetView
    ) const
{
    IColorTargetView* pNextView = nullptr;

    ColorTargetViewCreateInfo nextCreateInfo = createInfo;

    if (createInfo.flags.isBufferView)
    {
        nextCreateInfo.bufferInfo.pGpuMemory = NextGpuMemory(createInfo.bufferInfo.pGpuMemory);
    }
    else
    {
        nextCreateInfo.imageInfo.pImage      = NextImage(createInfo.imageInfo.pImage);
    }

    Result result = m_pNextLayer->CreateColorTargetView(nextCreateInfo,
                                                        NextObjectAddr<ColorTargetView>(pPlacementAddr),
                                                        &pNextView);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextView != nullptr);
        pNextView->SetClientData(pPlacementAddr);

        (*ppColorTargetView) = PAL_PLACEMENT_NEW(pPlacementAddr) ColorTargetView(pNextView, createInfo, this);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetDepthStencilViewSize(
    Result* pResult
    ) const
{
    return m_pNextLayer->GetDepthStencilViewSize(pResult) + sizeof(DepthStencilView);
}

// =====================================================================================================================
Result Device::CreateDepthStencilView(
    const DepthStencilViewCreateInfo& createInfo,
    void*                             pPlacementAddr,
    IDepthStencilView**               ppDepthStencilView
    ) const
{
    IDepthStencilView* pNextView = nullptr;

    DepthStencilViewCreateInfo nextCreateInfo = createInfo;

    nextCreateInfo.pImage = NextImage(createInfo.pImage);

    Result result = m_pNextLayer->CreateDepthStencilView(nextCreateInfo,
                                                         NextObjectAddr<DepthStencilView>(pPlacementAddr),
                                                         &pNextView);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextView != nullptr);
        pNextView->SetClientData(pPlacementAddr);

        (*ppDepthStencilView) = PAL_PLACEMENT_NEW(pPlacementAddr) DepthStencilView(pNextView, createInfo, this);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetColorBlendStateSize(
    const ColorBlendStateCreateInfo& createInfo,
    Result*                          pResult
    ) const
{
    return m_pNextLayer->GetColorBlendStateSize(createInfo, pResult) + sizeof(ColorBlendState);
}

// =====================================================================================================================
Result Device::CreateColorBlendState(
    const ColorBlendStateCreateInfo& createInfo,
    void*                            pPlacementAddr,
    IColorBlendState**               ppColorBlendState
    ) const
{
    IColorBlendState* pState = nullptr;

    Result result = m_pNextLayer->CreateColorBlendState(createInfo,
                                                        NextObjectAddr<ColorBlendState>(pPlacementAddr),
                                                        &pState);

    if (result == Result::Success)
    {
        PAL_ASSERT(pState != nullptr);
        pState->SetClientData(pPlacementAddr);

        (*ppColorBlendState) = PAL_PLACEMENT_NEW(pPlacementAddr) ColorBlendState(pState, createInfo, this);
    }

    return result;
}

} // GpuDebug
} // Pal

#endif
