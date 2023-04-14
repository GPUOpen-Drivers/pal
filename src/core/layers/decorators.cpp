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

#include "core/layers/decorators.h"
#include "palAutoBuffer.h"
#include "palHashMapImpl.h"
#include "palSysUtil.h"
#include "palVectorImpl.h"

#include <ctime>

using namespace Util;

namespace Pal
{

// =====================================================================================================================
IBorderColorPalette* NextBorderColorPalette(
    const IBorderColorPalette* pBorderColorPalette)
{
    return (pBorderColorPalette != nullptr) ?
            static_cast<const BorderColorPaletteDecorator*>(pBorderColorPalette)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
ICmdAllocator* NextCmdAllocator(
    const ICmdAllocator* pCmdAllocator)
{
    return (pCmdAllocator != nullptr) ?
            static_cast<const CmdAllocatorDecorator*>(pCmdAllocator)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
ICmdBuffer* NextCmdBuffer(
    const ICmdBuffer* pCmdBuffer)
{
    return (pCmdBuffer != nullptr) ?
            static_cast<const CmdBufferDecorator*>(pCmdBuffer)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
CmdBufferBuildInfo NextCmdBufferBuildInfo(
    const CmdBufferBuildInfo& buildInfo)
{
    CmdBufferBuildInfo nextBuildInfo = buildInfo;
    nextBuildInfo.pStateInheritCmdBuffer = NextCmdBuffer(buildInfo.pStateInheritCmdBuffer);

    return nextBuildInfo;
}

// =====================================================================================================================
IColorBlendState* NextColorBlendState(
    const IColorBlendState* pColorBlendState)
{
    return (pColorBlendState != nullptr) ?
            static_cast<const ColorBlendStateDecorator*>(pColorBlendState)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
const IColorTargetView* NextColorTargetView(
    const IColorTargetView* pView)
{
    return (pView != nullptr) ?
            static_cast<const ColorTargetViewDecorator*>(pView)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
IDepthStencilState* NextDepthStencilState(
    const IDepthStencilState* pDepthStencilState)
{
    return (pDepthStencilState != nullptr) ?
            static_cast<const DepthStencilStateDecorator*>(pDepthStencilState)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
const IDepthStencilView* NextDepthStencilView(
    const IDepthStencilView* pDepthStencilView)
{
    return (pDepthStencilView != nullptr) ?
            static_cast<const DepthStencilViewDecorator*>(pDepthStencilView)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
IDevice* NextDevice(
    const IDevice* pDevice)
{
    return (pDevice != nullptr) ? static_cast<const DeviceDecorator*>(pDevice)->GetNextLayer() : nullptr;
}

// =====================================================================================================================
IFence* NextFence(
    const IFence* pFence)
{
    return (pFence != nullptr) ?
            static_cast<const FenceDecorator*>(pFence)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
IGpuEvent* NextGpuEvent(
    const IGpuEvent* pGpuEvent)
{
    return (pGpuEvent != nullptr) ?
            static_cast<const GpuEventDecorator*>(pGpuEvent)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
IGpuMemory* NextGpuMemory(
    const IGpuMemory* pGpuMemory)
{
    return (pGpuMemory != nullptr) ?
            static_cast<const GpuMemoryDecorator*>(pGpuMemory)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
IImage* NextImage(
    const IImage* pImage)
{
    return (pImage != nullptr) ?
            static_cast<const ImageDecorator*>(pImage)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
IIndirectCmdGenerator* NextIndirectCmdGenerator(
    const IIndirectCmdGenerator* pGenerator)
{
    return (pGenerator != nullptr) ?
            static_cast<const IndirectCmdGeneratorDecorator*>(pGenerator)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
IMsaaState* NextMsaaState(
    const IMsaaState* pMsaaState)
{
    return (pMsaaState != nullptr) ?
            static_cast<const MsaaStateDecorator*>(pMsaaState)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
IPerfExperiment* NextPerfExperiment(
    const IPerfExperiment* pPerfExperiment)
{
    return (pPerfExperiment != nullptr) ?
            static_cast<const PerfExperimentDecorator*>(pPerfExperiment)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
IPipeline* NextPipeline(
    const IPipeline* pPipeline)
{
    return (pPipeline != nullptr) ?
            static_cast<const PipelineDecorator*>(pPipeline)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
PipelineBindParams NextPipelineBindParams(
    const PipelineBindParams& params)
{
    PipelineBindParams nextParams = params;
    nextParams.pPipeline = NextPipeline(nextParams.pPipeline);

    return nextParams;
}

// =====================================================================================================================
IShaderLibrary* NextShaderLibrary(
    const IShaderLibrary* pShaderLibrary)
{
    return (pShaderLibrary != nullptr) ?
            static_cast<const ShaderLibraryDecorator*>(pShaderLibrary)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
IPlatform* NextPlatform(
    const IPlatform* pPlatform)
{
    return (pPlatform != nullptr) ?
            static_cast<const PlatformDecorator*>(pPlatform)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
IPrivateScreen* NextPrivateScreen(
    const IPrivateScreen* pScreen)
{
    return (pScreen != nullptr) ?
           static_cast<const PrivateScreenDecorator*>(pScreen)->GetNextLayer() :
           nullptr;
}

// =====================================================================================================================
IQueryPool* NextQueryPool(
    const IQueryPool* pQueryPool)
{
    return (pQueryPool != nullptr) ?
            static_cast<const QueryPoolDecorator*>(pQueryPool)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
IQueue* NextQueue(
    const IQueue* pQueue)
{
    return (pQueue != nullptr) ?
            static_cast<const QueueDecorator*>(pQueue)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
IQueueSemaphore* NextQueueSemaphore(
    const IQueueSemaphore* pQueueSemaphore)
{
    return (pQueueSemaphore != nullptr) ?
            static_cast<const QueueSemaphoreDecorator*>(pQueueSemaphore)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
IScreen* NextScreen(
    const IScreen* pScreen)
{
    return (pScreen != nullptr) ?
           static_cast<const ScreenDecorator*>(pScreen)->GetNextLayer() :
           nullptr;
}

// =====================================================================================================================
ISwapChain* NextSwapChain(
    const ISwapChain* pSwapChain)
{
    return (pSwapChain != nullptr) ?
            static_cast<const SwapChainDecorator*>(pSwapChain)->GetNextLayer() :
            nullptr;
}

// =====================================================================================================================
DeviceDecorator::DeviceDecorator(
    PlatformDecorator*      pPlatform,
    IDevice*                pNextDevice)
    :
    m_pNextLayer(pNextDevice),
    m_pPlatform(pPlatform)
{
    memset(&m_finalizeInfo, 0, sizeof(m_finalizeInfo));
    memset(&m_pPrivateScreens[0], 0, sizeof(m_pPrivateScreens));

    m_pfnTable.pfnCreateTypedBufViewSrds   = &DeviceDecorator::DecoratorCreateTypedBufViewSrds;
    m_pfnTable.pfnCreateUntypedBufViewSrds = &DeviceDecorator::DecoratorCreateUntypedBufViewSrds;
    m_pfnTable.pfnCreateImageViewSrds      = &DeviceDecorator::DecoratorCreateImageViewSrds;
    m_pfnTable.pfnCreateFmaskViewSrds      = &DeviceDecorator::DecoratorCreateFmaskViewSrds;
    m_pfnTable.pfnCreateSamplerSrds        = &DeviceDecorator::DecoratorCreateSamplerSrds;
    m_pfnTable.pfnCreateBvhSrds            = &DeviceDecorator::DecoratorCreateBvhSrds;
}

// =====================================================================================================================
Result DeviceDecorator::Finalize(
    const DeviceFinalizeInfo& finalizeInfo)
{
    // Save the caller's finalize info for later use (e.g., calling their private screen destroy callback).
    m_finalizeInfo = finalizeInfo;

    // Replace the caller's private screen destroy callback with one of our own.
    DeviceFinalizeInfo nextFinalizeInfo = finalizeInfo;
    nextFinalizeInfo.privateScreenNotifyInfo.pfnOnDestroy = &DeviceDecorator::DestroyPrivateScreen;

    return m_pNextLayer->Finalize(nextFinalizeInfo);
}

// =====================================================================================================================
Result DeviceDecorator::AddGpuMemoryReferences(
    uint32              gpuMemRefCount,
    const GpuMemoryRef* pGpuMemoryRefs,
    IQueue*             pQueue,
    uint32              flags
    )
{
    AutoBuffer<GpuMemoryRef, 128, PlatformDecorator> nextGpuMemRef(gpuMemRefCount, m_pPlatform);

    Result result = Result::Success;

    if (nextGpuMemRef.Capacity() < gpuMemRefCount)
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        for (uint32 i = 0; i < gpuMemRefCount; i++)
        {
            nextGpuMemRef[i].flags      = pGpuMemoryRefs[i].flags;
            nextGpuMemRef[i].pGpuMemory = NextGpuMemory(pGpuMemoryRefs[i].pGpuMemory);
        }

        result = m_pNextLayer->AddGpuMemoryReferences(gpuMemRefCount, &nextGpuMemRef[0], NextQueue(pQueue), flags);
    }

    return result;
}

// =====================================================================================================================
Result DeviceDecorator::RemoveGpuMemoryReferences(
    uint32            gpuMemoryCount,
    IGpuMemory*const* ppGpuMemory,
    IQueue*           pQueue
    )
{
    AutoBuffer<IGpuMemory*, 128, PlatformDecorator> nextGpuMemory(gpuMemoryCount, m_pPlatform);

    Result result = Result::Success;

    if (nextGpuMemory.Capacity() < gpuMemoryCount)
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        for (uint32 i = 0; i < gpuMemoryCount; i++)
        {
            nextGpuMemory[i] = NextGpuMemory(ppGpuMemory[i]);
        }

        result = m_pNextLayer->RemoveGpuMemoryReferences(gpuMemoryCount, &nextGpuMemory[0], NextQueue(pQueue));
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetQueueSize(
    const QueueCreateInfo& createInfo,
    Result*                pResult
    ) const
{
    return m_pNextLayer->GetQueueSize(createInfo, pResult) + sizeof(QueueDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateQueue(
    const QueueCreateInfo& createInfo,
    void*                  pPlacementAddr,
    IQueue**               ppQueue)
{
    IQueue* pNextQueue = nullptr;

    Result result = m_pNextLayer->CreateQueue(createInfo,
                                              NextObjectAddr<QueueDecorator>(pPlacementAddr),
                                              &pNextQueue);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextQueue != nullptr);
        pNextQueue->SetClientData(pPlacementAddr);

        (*ppQueue) = PAL_PLACEMENT_NEW(pPlacementAddr) QueueDecorator(pNextQueue, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetMultiQueueSize(
    uint32                 queueCount,
    const QueueCreateInfo* pCreateInfo,
    Result*                pResult
    ) const
{
    return m_pNextLayer->GetMultiQueueSize(queueCount, pCreateInfo, pResult) + sizeof(QueueDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateMultiQueue(
    uint32                 queueCount,
    const QueueCreateInfo* pCreateInfo,
    void*                  pPlacementAddr,
    IQueue**               ppQueue)
{
    IQueue*          pNextQueue = nullptr;
    QueueDecorator*  pQueue     = nullptr;

    Result result = m_pNextLayer->CreateMultiQueue(queueCount,
                                                   pCreateInfo,
                                                   NextObjectAddr<QueueDecorator>(pPlacementAddr),
                                                   &pNextQueue);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextQueue != nullptr);
        pNextQueue->SetClientData(pPlacementAddr);

        (*ppQueue) = PAL_PLACEMENT_NEW(pPlacementAddr) QueueDecorator(pNextQueue, this);
    }

    return result;
}

// =====================================================================================================================
Result DeviceDecorator::ResetFences(
    uint32              fenceCount,
    IFence*const*       ppFences
    ) const
{
    AutoBuffer<IFence*, 16, PlatformDecorator> fences(fenceCount, GetPlatform());

    Result result = Result::Success;

    if (fences.Capacity() < fenceCount)
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        for (uint32 i = 0; i < fenceCount; i++)
        {
            fences[i] = NextFence(ppFences[i]);
        }

        result = m_pNextLayer->ResetFences(fenceCount, &fences[0]);
    }

    return result;
}

// =====================================================================================================================
Result DeviceDecorator::WaitForFences(
    uint32              fenceCount,
    const IFence*const* ppFences,
    bool                waitAll,
    uint64              timeout
    ) const
{
    AutoBuffer<const IFence*, 16, PlatformDecorator> fences(fenceCount, GetPlatform());

    Result result = Result::Success;

    if (fences.Capacity() < fenceCount)
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        for (uint32 i = 0; i < fenceCount; i++)
        {
            fences[i] = NextFence(ppFences[i]);
        }

        result = m_pNextLayer->WaitForFences(fenceCount, &fences[0], waitAll, timeout);
    }

    return result;
}

// =====================================================================================================================
Result DeviceDecorator::WaitForSemaphores(
        uint32                       semaphoreCount,
        const IQueueSemaphore*const* ppSemaphores,
        const uint64*                pValues,
        uint32                       flags,
        uint64                       timeout) const
{
    AutoBuffer<const IQueueSemaphore*, 16, PlatformDecorator> semaphores(semaphoreCount, GetPlatform());

    Result result = Result::Success;

    if (semaphores.Capacity() < semaphoreCount)
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        for (uint32 i = 0; i < semaphoreCount; i++)
        {
            semaphores[i] = NextQueueSemaphore(ppSemaphores[i]);
        }

        result = m_pNextLayer->WaitForSemaphores(semaphoreCount, &semaphores[0], pValues, flags, timeout);
    }

    return result;

}

// =====================================================================================================================
Result DeviceDecorator::GetSwapChainInfo(
    OsDisplayHandle      hDisplay,
    OsWindowHandle       hWindow,
    WsiPlatform          wsiPlatform,
    SwapChainProperties* pSwapChainProperties)
{
    return m_pNextLayer->GetSwapChainInfo(hDisplay,
                                          hWindow,
                                          wsiPlatform,
                                          pSwapChainProperties);
}

// =====================================================================================================================
size_t DeviceDecorator::GetGpuMemorySize(
    const GpuMemoryCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    GpuMemoryCreateInfo nextCreateInfo = createInfo;

    if (nextCreateInfo.pImage != nullptr)
    {
        nextCreateInfo.pImage = NextImage(nextCreateInfo.pImage);
    }

    return m_pNextLayer->GetGpuMemorySize(nextCreateInfo, pResult) + sizeof(GpuMemoryDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateGpuMemory(
    const GpuMemoryCreateInfo& createInfo,
    void*                      pPlacementAddr,
    IGpuMemory**               ppGpuMemory)
{
    IGpuMemory*         pNextMemObj    = nullptr;
    GpuMemoryCreateInfo nextCreateInfo = createInfo;

    if (nextCreateInfo.pImage != nullptr)
    {
        nextCreateInfo.pImage = NextImage(nextCreateInfo.pImage);
    }

    Result result = m_pNextLayer->CreateGpuMemory(nextCreateInfo,
                                                  NextObjectAddr<GpuMemoryDecorator>(pPlacementAddr),
                                                  &pNextMemObj);

    if ((result == Result::Success) || (result == Result::TooManyFlippableAllocations))
    {
        PAL_ASSERT(pNextMemObj != nullptr);
        pNextMemObj->SetClientData(pPlacementAddr);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemoryDecorator(pNextMemObj, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetPinnedGpuMemorySize(
    const PinnedGpuMemoryCreateInfo& createInfo,
    Result*                          pResult
    ) const
{
    return m_pNextLayer->GetPinnedGpuMemorySize(createInfo, pResult) + sizeof(GpuMemoryDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreatePinnedGpuMemory(
    const PinnedGpuMemoryCreateInfo& createInfo,
    void*                            pPlacementAddr,
    IGpuMemory**                     ppGpuMemory)
{
    IGpuMemory* pNextMemObj = nullptr;

    Result result = m_pNextLayer->CreatePinnedGpuMemory(createInfo,
                                                        NextObjectAddr<GpuMemoryDecorator>(pPlacementAddr),
                                                        &pNextMemObj);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextMemObj != nullptr);
        pNextMemObj->SetClientData(pPlacementAddr);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemoryDecorator(pNextMemObj, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetSvmGpuMemorySize(
    const SvmGpuMemoryCreateInfo& createInfo,
    Result*                       pResult
    ) const
{
    SvmGpuMemoryCreateInfo nextCreateInfo = createInfo;

    if (nextCreateInfo.pReservedGpuVaOwner != nullptr)
    {
        nextCreateInfo.pReservedGpuVaOwner = NextGpuMemory(nextCreateInfo.pReservedGpuVaOwner);
    }

    return m_pNextLayer->GetSvmGpuMemorySize(nextCreateInfo, pResult) + sizeof(GpuMemoryDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateSvmGpuMemory(
    const SvmGpuMemoryCreateInfo& createInfo,
    void*                         pPlacementAddr,
    IGpuMemory**                  ppGpuMemory)
{
    IGpuMemory*            pNextMemObj    = nullptr;
    SvmGpuMemoryCreateInfo nextCreateInfo = createInfo;

    if (nextCreateInfo.pReservedGpuVaOwner != nullptr)
    {
        nextCreateInfo.pReservedGpuVaOwner = NextGpuMemory(nextCreateInfo.pReservedGpuVaOwner);
    }

    Result result = m_pNextLayer->CreateSvmGpuMemory(nextCreateInfo,
                                                     NextObjectAddr<GpuMemoryDecorator>(pPlacementAddr),
                                                     &pNextMemObj);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextMemObj != nullptr);
        pNextMemObj->SetClientData(pPlacementAddr);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemoryDecorator(pNextMemObj, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetSharedGpuMemorySize(
    const GpuMemoryOpenInfo& openInfo,
    Result*                  pResult
    ) const
{
    GpuMemoryOpenInfo nextOpenInfo = openInfo;
    nextOpenInfo.pSharedMem = NextGpuMemory(openInfo.pSharedMem);

    return m_pNextLayer->GetSharedGpuMemorySize(nextOpenInfo, pResult) + sizeof(GpuMemoryDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::OpenSharedGpuMemory(
    const GpuMemoryOpenInfo& openInfo,
    void*                    pPlacementAddr,
    IGpuMemory**             ppGpuMemory)
{
    IGpuMemory* pNextMemObj = nullptr;

    GpuMemoryOpenInfo nextOpenInfo = openInfo;
    nextOpenInfo.pSharedMem = NextGpuMemory(openInfo.pSharedMem);

    Result result = m_pNextLayer->OpenSharedGpuMemory(nextOpenInfo,
                                                      NextObjectAddr<GpuMemoryDecorator>(pPlacementAddr),
                                                      &pNextMemObj);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextMemObj != nullptr);
        pNextMemObj->SetClientData(pPlacementAddr);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemoryDecorator(pNextMemObj, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetExternalSharedGpuMemorySize(
    Result* pResult
    ) const
{
    return m_pNextLayer->GetExternalSharedGpuMemorySize(pResult) + sizeof(GpuMemoryDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::OpenExternalSharedGpuMemory(
    const ExternalGpuMemoryOpenInfo& openInfo,
    void*                            pPlacementAddr,
    GpuMemoryCreateInfo*             pMemCreateInfo,
    IGpuMemory**                     ppGpuMemory)
{
    IGpuMemory*  pNextMemObj = nullptr;
    const Result result      =
        m_pNextLayer->OpenExternalSharedGpuMemory(openInfo,
                                                  NextObjectAddr<GpuMemoryDecorator>(pPlacementAddr),
                                                  pMemCreateInfo,
                                                  &pNextMemObj);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextMemObj != nullptr);
        pNextMemObj->SetClientData(pPlacementAddr);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemoryDecorator(pNextMemObj, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetPeerGpuMemorySize(
    const PeerGpuMemoryOpenInfo& openInfo,
    Result*                      pResult
    ) const
{
    PeerGpuMemoryOpenInfo nextOpenInfo = openInfo;
    nextOpenInfo.pOriginalMem = NextGpuMemory(openInfo.pOriginalMem);
    return m_pNextLayer->GetPeerGpuMemorySize(nextOpenInfo, pResult) + sizeof(GpuMemoryDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::OpenPeerGpuMemory(
    const PeerGpuMemoryOpenInfo& openInfo,
    void*                        pPlacementAddr,
    IGpuMemory**                 ppGpuMemory)
{
    IGpuMemory* pNextMemObj = nullptr;

    PeerGpuMemoryOpenInfo nextOpenInfo = openInfo;
    nextOpenInfo.pOriginalMem = NextGpuMemory(openInfo.pOriginalMem);

    Result result = m_pNextLayer->OpenPeerGpuMemory(nextOpenInfo,
                                                    NextObjectAddr<GpuMemoryDecorator>(pPlacementAddr),
                                                    &pNextMemObj);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextMemObj != nullptr);
        pNextMemObj->SetClientData(pPlacementAddr);

        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuMemoryDecorator(pNextMemObj, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetImageSize(
    const ImageCreateInfo& createInfo,
    Result*                pResult
    ) const
{
    return m_pNextLayer->GetImageSize(createInfo, pResult) + sizeof(ImageDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateImage(
    const ImageCreateInfo& createInfo,
    void*                  pPlacementAddr,
    IImage**               ppImage)
{
    IImage* pNextImage = nullptr;

    Result result = m_pNextLayer->CreateImage(createInfo,
                                              NextObjectAddr<ImageDecorator>(pPlacementAddr),
                                              &pNextImage);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextImage != nullptr);
        pNextImage->SetClientData(pPlacementAddr);

        (*ppImage) = PAL_PLACEMENT_NEW(pPlacementAddr) ImageDecorator(pNextImage, this);
    }

    return result;
}

// =====================================================================================================================
void DeviceDecorator::GetPresentableImageSizes(
    const PresentableImageCreateInfo& createInfo,
    size_t*                           pImageSize,
    size_t*                           pGpuMemorySize,
    Result*                           pResult
    ) const
{
    PresentableImageCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pScreen                    = NextScreen(createInfo.pScreen);
    nextCreateInfo.pSwapChain                 = NextSwapChain(createInfo.pSwapChain);

    m_pNextLayer->GetPresentableImageSizes(nextCreateInfo, pImageSize, pGpuMemorySize, pResult);
    (*pImageSize)     += sizeof(ImageDecorator);
    (*pGpuMemorySize) += sizeof(GpuMemoryDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreatePresentableImage(
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
                                             NextObjectAddr<ImageDecorator>(pImagePlacementAddr),
                                             NextObjectAddr<GpuMemoryDecorator>(pGpuMemoryPlacementAddr),
                                             &pNextImage,
                                             &pNextGpuMemory);

    if ((result == Result::Success) || (result == Result::TooManyFlippableAllocations))
    {
        pNextImage->SetClientData(pImagePlacementAddr);
        pNextGpuMemory->SetClientData(pGpuMemoryPlacementAddr);
        (*ppImage)     = PAL_PLACEMENT_NEW(pImagePlacementAddr) ImageDecorator(pNextImage, this);
        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pGpuMemoryPlacementAddr) GpuMemoryDecorator(pNextGpuMemory, this);
    }

    return result;
}

// =====================================================================================================================
void DeviceDecorator::GetPeerImageSizes(
    const PeerImageOpenInfo& openInfo,
    size_t*                  pPeerImageSize,
    size_t*                  pPeerGpuMemorySize,
    Result*                  pResult
    ) const
{
    PeerImageOpenInfo nextOpenInfo = openInfo;
    nextOpenInfo.pOriginalImage = NextImage(openInfo.pOriginalImage);
    m_pNextLayer->GetPeerImageSizes(nextOpenInfo, pPeerImageSize, pPeerGpuMemorySize, pResult);
    (*pPeerImageSize)     += sizeof(ImageDecorator);
    (*pPeerGpuMemorySize) += sizeof(GpuMemoryDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::OpenPeerImage(
    const PeerImageOpenInfo& openInfo,
    void*                    pImagePlacementAddr,
    void*                    pGpuMemoryPlacementAddr,
    IImage**                 ppImage,
    IGpuMemory**             ppGpuMemory)
{
    IImage*     pNextImage                  = nullptr;
    IGpuMemory* pNextGpuMemory              = nullptr;
    void*       pNextGpuMemoryPlacementAddr = nullptr;

    const bool peerMemoryPreallocated = (pGpuMemoryPlacementAddr == nullptr);

    if (peerMemoryPreallocated)
    {
        pNextGpuMemory = reinterpret_cast<IGpuMemory*>(NextObjectAddr<GpuMemoryDecorator>(*ppGpuMemory));
    }
    else
    {
        pNextGpuMemoryPlacementAddr = NextObjectAddr<GpuMemoryDecorator>(pGpuMemoryPlacementAddr);
    }

    PeerImageOpenInfo nextOpenInfo = openInfo;
    nextOpenInfo.pOriginalImage = NextImage(openInfo.pOriginalImage);

    Result result = m_pNextLayer->OpenPeerImage(nextOpenInfo,
                                                NextObjectAddr<ImageDecorator>(pImagePlacementAddr),
                                                pNextGpuMemoryPlacementAddr,
                                                &pNextImage,
                                                &pNextGpuMemory);

    if (result == Result::Success)
    {
        pNextImage->SetClientData(pImagePlacementAddr);
        (*ppImage) = PAL_PLACEMENT_NEW(pImagePlacementAddr) ImageDecorator(pNextImage, this);

        if (peerMemoryPreallocated == false)
        {
            pNextGpuMemory->SetClientData(pGpuMemoryPlacementAddr);
            (*ppGpuMemory) = PAL_PLACEMENT_NEW(pGpuMemoryPlacementAddr) GpuMemoryDecorator(pNextGpuMemory, this);
        }
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetColorTargetViewSize(
    Result* pResult
    ) const
{
    return m_pNextLayer->GetColorTargetViewSize(pResult) + sizeof(ColorTargetViewDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateColorTargetView(
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
                                                        NextObjectAddr<ColorTargetViewDecorator>(pPlacementAddr),
                                                        &pNextView);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextView != nullptr);
        pNextView->SetClientData(pPlacementAddr);

        (*ppColorTargetView) = PAL_PLACEMENT_NEW(pPlacementAddr) ColorTargetViewDecorator(pNextView, createInfo, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetDepthStencilViewSize(
    Result* pResult
    ) const
{
    return m_pNextLayer->GetDepthStencilViewSize(pResult) + sizeof(DepthStencilViewDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateDepthStencilView(
    const DepthStencilViewCreateInfo& createInfo,
    void*                             pPlacementAddr,
    IDepthStencilView**               ppDepthStencilView
    ) const
{
    IDepthStencilView* pNextView = nullptr;

    DepthStencilViewCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pImage                     = NextImage(createInfo.pImage);

    Result result = m_pNextLayer->CreateDepthStencilView(nextCreateInfo,
                                                         NextObjectAddr<DepthStencilViewDecorator>(pPlacementAddr),
                                                         &pNextView);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextView != nullptr);
        pNextView->SetClientData(pPlacementAddr);

        (*ppDepthStencilView) = PAL_PLACEMENT_NEW(pPlacementAddr) DepthStencilViewDecorator(pNextView, this);
    }

    return result;
}

// =====================================================================================================================
void PAL_STDCALL DeviceDecorator::DecoratorCreateTypedBufViewSrds(
    const IDevice*        pDevice,
    uint32                count,
    const BufferViewInfo* pBufferViewInfo,
    void*                 pOut)
{
    const DeviceDecorator* pDeviceDecorator = static_cast<const DeviceDecorator*>(pDevice);
    const IDevice*         pNextDevice      = pDeviceDecorator->GetNextLayer();

    pNextDevice->CreateTypedBufferViewSrds(count, pBufferViewInfo, pOut);
}

// =====================================================================================================================
void PAL_STDCALL DeviceDecorator::DecoratorCreateUntypedBufViewSrds(
    const IDevice*        pDevice,
    uint32                count,
    const BufferViewInfo* pBufferViewInfo,
    void*                 pOut)
{
    const DeviceDecorator* pDeviceDecorator = static_cast<const DeviceDecorator*>(pDevice);
    const IDevice*         pNextDevice      = pDeviceDecorator->GetNextLayer();

    pNextDevice->CreateUntypedBufferViewSrds(count, pBufferViewInfo, pOut);
}

// =====================================================================================================================
Result DeviceDecorator::ValidateSamplerInfo(
    const SamplerInfo& info
    ) const
{
    return m_pNextLayer->ValidateSamplerInfo(info);
}

// =====================================================================================================================
void PAL_STDCALL DeviceDecorator::DecoratorCreateSamplerSrds(
    const IDevice*     pDevice,
    uint32             count,
    const SamplerInfo* pSamplerInfo,
    void*              pOut)
{
    const DeviceDecorator* pDeviceDecorator = static_cast<const DeviceDecorator*>(pDevice);
    const IDevice*         pNextDevice      = pDeviceDecorator->GetNextLayer();

    pNextDevice->CreateSamplerSrds(count, pSamplerInfo, pOut);
}

// =====================================================================================================================
void PAL_STDCALL DeviceDecorator::DecoratorCreateBvhSrds(
    const IDevice*  pDevice,
    uint32          count,
    const BvhInfo*  pBvhInfo,
    void*           pOut)
{
    const DeviceDecorator* pDeviceDecorator = static_cast<const DeviceDecorator*>(pDevice);
    const IDevice*         pNextDevice      = pDeviceDecorator->GetNextLayer();

    AutoBuffer<BvhInfo, 16, PlatformDecorator> bvhInfo(count, pDeviceDecorator->GetPlatform());

    if (bvhInfo.Capacity() < count) [[unlikely]]
    {
        // No way to report this error...
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        for (uint32 i = 0; i < count; i++)
        {
            bvhInfo[i]         = pBvhInfo[i];
            bvhInfo[i].pMemory = NextGpuMemory(pBvhInfo[i].pMemory);
        }

        pNextDevice->CreateBvhSrds(count, pBvhInfo, pOut);
    }
}

// =====================================================================================================================
Result DeviceDecorator::ValidateImageViewInfo(
    const ImageViewInfo& viewInfo
    ) const
{
    ImageViewInfo imageViewInfo = viewInfo;
    imageViewInfo.pImage = NextImage(viewInfo.pImage);

    return m_pNextLayer->ValidateImageViewInfo(imageViewInfo);
}

// =====================================================================================================================
void DeviceDecorator::DecoratorCreateImageViewSrds(
    const IDevice*       pDevice,
    uint32               count,
    const ImageViewInfo* pImgViewInfo,
    void*                pOut)
{
    const DeviceDecorator* pDeviceDecorator = static_cast<const DeviceDecorator*>(pDevice);
    const IDevice*         pNextDevice      = pDeviceDecorator->GetNextLayer();

    AutoBuffer<ImageViewInfo, 16, PlatformDecorator> imageViewInfo(count, pDeviceDecorator->GetPlatform());

    if (imageViewInfo.Capacity() < count)
    {
        // No way to report this error...
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        for (uint32 i = 0; i < count; i++)
        {
            imageViewInfo[i]               = pImgViewInfo[i];
            imageViewInfo[i].pImage        = NextImage(pImgViewInfo[i].pImage);

            imageViewInfo[i].pPrtParentImg = NextImage(pImgViewInfo[i].pPrtParentImg);
        }

        pNextDevice->CreateImageViewSrds(count, &imageViewInfo[0], pOut);
    }
}

// =====================================================================================================================
Result DeviceDecorator::ValidateFmaskViewInfo(
    const FmaskViewInfo& viewInfo
    ) const
{
    FmaskViewInfo fmaskViewInfo = viewInfo;
    fmaskViewInfo.pImage = NextImage(viewInfo.pImage);

    return m_pNextLayer->ValidateFmaskViewInfo(fmaskViewInfo);
}

// =====================================================================================================================
void PAL_STDCALL DeviceDecorator::DecoratorCreateFmaskViewSrds(
    const IDevice*       pDevice,
    uint32               count,
    const FmaskViewInfo* pFmaskViewInfo,
    void*                pOut)
{
    const DeviceDecorator* pDeviceDecorator = static_cast<const DeviceDecorator*>(pDevice);
    const IDevice*         pNextDevice      = pDeviceDecorator->GetNextLayer();

    AutoBuffer<FmaskViewInfo, 16, PlatformDecorator> fmaskViewInfo(count, pDeviceDecorator->GetPlatform());

    if (fmaskViewInfo.Capacity() < count)
    {
        // No way to report this error...
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        for (uint32 i = 0; i < count; i++)
        {
            fmaskViewInfo[i]        = pFmaskViewInfo[i];
            fmaskViewInfo[i].pImage = NextImage(pFmaskViewInfo[i].pImage);
        }

        pNextDevice->CreateFmaskViewSrds(count, &fmaskViewInfo[0], pOut);
    }
}

// =====================================================================================================================
size_t DeviceDecorator::GetBorderColorPaletteSize(
    const BorderColorPaletteCreateInfo& createInfo,
    Result*                             pResult
    ) const
{
    return m_pNextLayer->GetBorderColorPaletteSize(createInfo, pResult) +
           sizeof(BorderColorPaletteDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateBorderColorPalette(
    const BorderColorPaletteCreateInfo& createInfo,
    void*                               pPlacementAddr,
    IBorderColorPalette**               ppPalette
    ) const
{
    IBorderColorPalette* pNextPalette = nullptr;

    Result result =
        m_pNextLayer->CreateBorderColorPalette(createInfo,
                                               NextObjectAddr<BorderColorPaletteDecorator>(pPlacementAddr),
                                               &pNextPalette);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextPalette != nullptr);
        pNextPalette->SetClientData(pPlacementAddr);

        (*ppPalette) = PAL_PLACEMENT_NEW(pPlacementAddr) BorderColorPaletteDecorator(pNextPalette, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetComputePipelineSize(
    const ComputePipelineCreateInfo& createInfo,
    Result*                          pResult
    ) const
{
    return m_pNextLayer->GetComputePipelineSize(createInfo, pResult) + sizeof(PipelineDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateComputePipeline(
    const ComputePipelineCreateInfo& createInfo,
    void*                            pPlacementAddr,
    IPipeline**                      ppPipeline)
{
    IPipeline* pPipeline = nullptr;

    Result result = m_pNextLayer->CreateComputePipeline(createInfo,
                                                        NextObjectAddr<PipelineDecorator>(pPlacementAddr),
                                                        &pPipeline);

    if (result == Result::Success)
    {
        PAL_ASSERT(pPipeline != nullptr);
        pPipeline->SetClientData(pPlacementAddr);

        (*ppPipeline) = PAL_PLACEMENT_NEW(pPlacementAddr) PipelineDecorator(pPipeline, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetShaderLibrarySize(
    const ShaderLibraryCreateInfo& createInfo,
    Result*                        pResult) const
{
    return m_pNextLayer->GetShaderLibrarySize(createInfo, pResult) + sizeof(ShaderLibraryDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateShaderLibrary(
    const ShaderLibraryCreateInfo& createInfo,
    void*                          pPlacementAddr,
    IShaderLibrary**               ppLibrary)
{
    IShaderLibrary* pLib = nullptr;

    Result result = m_pNextLayer->CreateShaderLibrary(createInfo,
                                                      NextObjectAddr<ShaderLibraryDecorator>(pPlacementAddr),
                                                      &pLib);

    if (result == Result::Success)
    {
        PAL_ASSERT(pLib != nullptr);
        pLib->SetClientData(pPlacementAddr);

        (*ppLibrary) = PAL_PLACEMENT_NEW(pPlacementAddr) ShaderLibraryDecorator(pLib, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetGraphicsPipelineSize(
    const GraphicsPipelineCreateInfo& createInfo,
    Result*                           pResult
    ) const
{
    return m_pNextLayer->GetGraphicsPipelineSize(createInfo, pResult) + sizeof(PipelineDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateGraphicsPipeline(
    const GraphicsPipelineCreateInfo& createInfo,
    void*                             pPlacementAddr,
    IPipeline**                       ppPipeline)
{
    IPipeline* pPipeline = nullptr;

    Result result = m_pNextLayer->CreateGraphicsPipeline(createInfo,
                                                         NextObjectAddr<PipelineDecorator>(pPlacementAddr),
                                                         &pPipeline);

    if (result == Result::Success)
    {
        PAL_ASSERT(pPipeline != nullptr);
        pPipeline->SetClientData(pPlacementAddr);

        (*ppPipeline) = PAL_PLACEMENT_NEW(pPlacementAddr) PipelineDecorator(pPipeline, this);
        result = static_cast<PipelineDecorator*>(*ppPipeline)->Init();
        if (result != Result::Success)
        {
            (*ppPipeline)->Destroy();
            *ppPipeline = nullptr;
        }
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetMsaaStateSize(
    const MsaaStateCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    return m_pNextLayer->GetMsaaStateSize(createInfo, pResult) + sizeof(MsaaStateDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateMsaaState(
    const MsaaStateCreateInfo& createInfo,
    void*                      pPlacementAddr,
    IMsaaState**               ppMsaaState
    ) const
{
    IMsaaState* pState = nullptr;

    Result result = m_pNextLayer->CreateMsaaState(createInfo,
                                                  NextObjectAddr<MsaaStateDecorator>(pPlacementAddr),
                                                  &pState);

    if (result == Result::Success)
    {
        PAL_ASSERT(pState != nullptr);
        pState->SetClientData(pPlacementAddr);

        (*ppMsaaState) = PAL_PLACEMENT_NEW(pPlacementAddr) MsaaStateDecorator(pState, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetColorBlendStateSize(
    const ColorBlendStateCreateInfo& createInfo,
    Result*                          pResult
    ) const
{
    return m_pNextLayer->GetColorBlendStateSize(createInfo, pResult) + sizeof(ColorBlendStateDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateColorBlendState(
    const ColorBlendStateCreateInfo& createInfo,
    void*                            pPlacementAddr,
    IColorBlendState**               ppColorBlendState
    ) const
{
    IColorBlendState* pState = nullptr;

    Result result = m_pNextLayer->CreateColorBlendState(createInfo,
                                                        NextObjectAddr<ColorBlendStateDecorator>(pPlacementAddr),
                                                        &pState);

    if (result == Result::Success)
    {
        PAL_ASSERT(pState != nullptr);
        pState->SetClientData(pPlacementAddr);

        (*ppColorBlendState) = PAL_PLACEMENT_NEW(pPlacementAddr) ColorBlendStateDecorator(pState, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetDepthStencilStateSize(
    const DepthStencilStateCreateInfo& createInfo,
    Result*                            pResult
    ) const
{
    return m_pNextLayer->GetDepthStencilStateSize(createInfo, pResult) + sizeof(DepthStencilStateDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateDepthStencilState(
    const DepthStencilStateCreateInfo& createInfo,
    void*                              pPlacementAddr,
    IDepthStencilState**               ppDepthStencilState
    ) const
{
    IDepthStencilState* pState = nullptr;

    Result result =
        m_pNextLayer->CreateDepthStencilState(createInfo,
                                              NextObjectAddr<DepthStencilStateDecorator>(pPlacementAddr),
                                              &pState);

    if (result == Result::Success)
    {
        PAL_ASSERT(pState != nullptr);
        pState->SetClientData(pPlacementAddr);

        (*ppDepthStencilState) = PAL_PLACEMENT_NEW(pPlacementAddr) DepthStencilStateDecorator(pState, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetQueueSemaphoreSize(
    const QueueSemaphoreCreateInfo& createInfo,
    Result*                         pResult
    ) const
{
    return m_pNextLayer->GetQueueSemaphoreSize(createInfo, pResult) + sizeof(QueueSemaphoreDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateQueueSemaphore(
    const QueueSemaphoreCreateInfo& createInfo,
    void*                           pPlacementAddr,
    IQueueSemaphore**               ppQueueSemaphore)
{
    IQueueSemaphore* pSemaphore = nullptr;

    Result result = m_pNextLayer->CreateQueueSemaphore(createInfo,
                                                       NextObjectAddr<QueueSemaphoreDecorator>(pPlacementAddr),
                                                       &pSemaphore);

    if (result == Result::Success)
    {
        PAL_ASSERT(pSemaphore != nullptr);
        pSemaphore->SetClientData(pPlacementAddr);

        (*ppQueueSemaphore) = PAL_PLACEMENT_NEW(pPlacementAddr) QueueSemaphoreDecorator(pSemaphore, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetSharedQueueSemaphoreSize(
    const QueueSemaphoreOpenInfo& openInfo,
    Result*                       pResult
    ) const
{
    QueueSemaphoreOpenInfo nextOpenInfo = openInfo;
    nextOpenInfo.pSharedQueueSemaphore  = NextQueueSemaphore(openInfo.pSharedQueueSemaphore);

    return m_pNextLayer->GetSharedQueueSemaphoreSize(nextOpenInfo, pResult) + sizeof(QueueSemaphoreDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::OpenSharedQueueSemaphore(
    const QueueSemaphoreOpenInfo& openInfo,
    void*                         pPlacementAddr,
    IQueueSemaphore**             ppQueueSemaphore)
{
    IQueueSemaphore* pSemaphore = nullptr;

    QueueSemaphoreOpenInfo nextOpenInfo = openInfo;
    nextOpenInfo.pSharedQueueSemaphore  = NextQueueSemaphore(openInfo.pSharedQueueSemaphore);

    Result result = m_pNextLayer->OpenSharedQueueSemaphore(nextOpenInfo,
                                                           NextObjectAddr<QueueSemaphoreDecorator>(pPlacementAddr),
                                                           &pSemaphore);

    if (result == Result::Success)
    {
        PAL_ASSERT(pSemaphore != nullptr);
        pSemaphore->SetClientData(pPlacementAddr);

        (*ppQueueSemaphore) = PAL_PLACEMENT_NEW(pPlacementAddr) QueueSemaphoreDecorator(pSemaphore, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetExternalSharedQueueSemaphoreSize(
    const ExternalQueueSemaphoreOpenInfo& openInfo,
    Result*                               pResult
    ) const
{
    return m_pNextLayer->GetExternalSharedQueueSemaphoreSize(openInfo, pResult) + sizeof(QueueSemaphoreDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::OpenExternalSharedQueueSemaphore(
    const ExternalQueueSemaphoreOpenInfo& openInfo,
    void*                                 pPlacementAddr,
    IQueueSemaphore**                     ppQueueSemaphore)
{
    IQueueSemaphore* pSemaphore = nullptr;

    Result result = m_pNextLayer->OpenExternalSharedQueueSemaphore(
                                        openInfo,
                                        NextObjectAddr<QueueSemaphoreDecorator>(pPlacementAddr),
                                        &pSemaphore);

    if (result == Result::Success)
    {
        PAL_ASSERT(pSemaphore != nullptr);
        pSemaphore->SetClientData(pPlacementAddr);

        (*ppQueueSemaphore) = PAL_PLACEMENT_NEW(pPlacementAddr) QueueSemaphoreDecorator(pSemaphore, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetFenceSize(
    Result* pResult
    ) const
{
    return m_pNextLayer->GetFenceSize(pResult) + sizeof(FenceDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateFence(
    const FenceCreateInfo& createInfo,
    void*                  pPlacementAddr,
    IFence**               ppFence
    ) const
{
    IFence* pFence = nullptr;
    Result result = m_pNextLayer->CreateFence(createInfo,
                                              NextObjectAddr<FenceDecorator>(pPlacementAddr),
                                              &pFence);

    if (result == Result::Success)
    {
        PAL_ASSERT(pFence != nullptr);
        pFence->SetClientData(pPlacementAddr);

        (*ppFence) = PAL_PLACEMENT_NEW(pPlacementAddr) FenceDecorator(pFence, this);
    }

    return result;
}

// =====================================================================================================================
Result DeviceDecorator::OpenFence(
    const FenceOpenInfo& openInfo,
    void*                pPlacementAddr,
    IFence**             ppFence
    ) const
{
    IFence* pFence = nullptr;
    Result result = m_pNextLayer->OpenFence(openInfo,
                                            NextObjectAddr<FenceDecorator>(pPlacementAddr),
                                            &pFence);

    if (result == Result::Success)
    {
        PAL_ASSERT(pFence != nullptr);
        pFence->SetClientData(pPlacementAddr);

        (*ppFence) = PAL_PLACEMENT_NEW(pPlacementAddr) FenceDecorator(pFence, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetGpuEventSize(
    const GpuEventCreateInfo& createInfo,
    Result*                   pResult
    ) const
{
    return m_pNextLayer->GetGpuEventSize(createInfo, pResult) + sizeof(GpuEventDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateGpuEvent(
    const GpuEventCreateInfo& createInfo,
    void*                     pPlacementAddr,
    IGpuEvent**               ppGpuEvent)
{
    IGpuEvent* pGpuEvent = nullptr;

    Result result = m_pNextLayer->CreateGpuEvent(createInfo,
                                                 NextObjectAddr<GpuEventDecorator>(pPlacementAddr),
                                                 &pGpuEvent);

    if (result == Result::Success)
    {
        PAL_ASSERT(pGpuEvent != nullptr);
        pGpuEvent->SetClientData(pPlacementAddr);

        (*ppGpuEvent) = PAL_PLACEMENT_NEW(pPlacementAddr) GpuEventDecorator(pGpuEvent, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetQueryPoolSize(
    const QueryPoolCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    return m_pNextLayer->GetQueryPoolSize(createInfo, pResult) + sizeof(QueryPoolDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateQueryPool(
    const QueryPoolCreateInfo& createInfo,
    void*                      pPlacementAddr,
    IQueryPool**               ppQueryPool
    ) const
{
    IQueryPool* pQueryPool = nullptr;

    Result result = m_pNextLayer->CreateQueryPool(createInfo,
                                                  NextObjectAddr<QueryPoolDecorator>(pPlacementAddr),
                                                  &pQueryPool);

    if (result == Result::Success)
    {
        PAL_ASSERT(pQueryPool != nullptr);
        pQueryPool->SetClientData(pPlacementAddr);

        (*ppQueryPool) = PAL_PLACEMENT_NEW(pPlacementAddr) QueryPoolDecorator(pQueryPool, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetCmdAllocatorSize(
    const CmdAllocatorCreateInfo& createInfo,
    Result*                       pResult
    ) const
{
    return m_pNextLayer->GetCmdAllocatorSize(createInfo, pResult) + sizeof(CmdAllocatorDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateCmdAllocator(
    const CmdAllocatorCreateInfo& createInfo,
    void*                         pPlacementAddr,
    ICmdAllocator**               ppCmdAllocator)
{
    ICmdAllocator* pNextCmdAllocator = nullptr;

    Result result = m_pNextLayer->CreateCmdAllocator(createInfo,
                                                     NextObjectAddr<CmdAllocatorDecorator>(pPlacementAddr),
                                                     &pNextCmdAllocator);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextCmdAllocator != nullptr);
        pNextCmdAllocator->SetClientData(pPlacementAddr);

        CmdAllocatorDecorator*const pCmdAllocator =
            PAL_PLACEMENT_NEW(pPlacementAddr) CmdAllocatorDecorator(pNextCmdAllocator);

        (*ppCmdAllocator) = pCmdAllocator;
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetCmdBufferSize(
    const CmdBufferCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    return m_pNextLayer->GetCmdBufferSize(nextCreateInfo, pResult) + sizeof(CmdBufferFwdDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateCmdBuffer(
    const CmdBufferCreateInfo& createInfo,
    void*                      pPlacementAddr,
    ICmdBuffer**               ppCmdBuffer)
{
    ICmdBuffer* pNextCmdBuffer = nullptr;

    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    Result result = m_pNextLayer->CreateCmdBuffer(nextCreateInfo,
                                                  NextObjectAddr<CmdBufferFwdDecorator>(pPlacementAddr),
                                                  &pNextCmdBuffer);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextCmdBuffer != nullptr);
        pNextCmdBuffer->SetClientData(pPlacementAddr);

        (*ppCmdBuffer) = PAL_PLACEMENT_NEW(pPlacementAddr) CmdBufferFwdDecorator(pNextCmdBuffer, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetIndirectCmdGeneratorSize(
    const IndirectCmdGeneratorCreateInfo& createInfo,
    Result*                               pResult) const
{
    return m_pNextLayer->GetIndirectCmdGeneratorSize(createInfo, pResult) +
           sizeof(IndirectCmdGeneratorDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateIndirectCmdGenerator(
    const IndirectCmdGeneratorCreateInfo& createInfo,
    void*                                 pPlacementAddr,
    IIndirectCmdGenerator**               ppGenerator) const
{
    IIndirectCmdGenerator* pNextGenerator = nullptr;

    Result result = m_pNextLayer->CreateIndirectCmdGenerator(
        createInfo,
        NextObjectAddr<IndirectCmdGeneratorDecorator>(pPlacementAddr),
        &pNextGenerator);
    if (result == Result::Success)
    {
        pNextGenerator->SetClientData(pPlacementAddr);
        (*ppGenerator) = PAL_PLACEMENT_NEW(pPlacementAddr) IndirectCmdGeneratorDecorator(pNextGenerator, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetPerfExperimentSize(
    const PerfExperimentCreateInfo& createInfo,
    Result*                         pResult
    ) const
{
    return m_pNextLayer->GetPerfExperimentSize(createInfo, pResult) + sizeof(PerfExperimentDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreatePerfExperiment(
    const PerfExperimentCreateInfo& createInfo,
    void*                           pPlacementAddr,
    IPerfExperiment**               ppPerfExperiment
    ) const
{
    IPerfExperiment* pPerfExperiment = nullptr;

    Result result = m_pNextLayer->CreatePerfExperiment(createInfo,
                                                       NextObjectAddr<PerfExperimentDecorator>(pPlacementAddr),
                                                       &pPerfExperiment);

    if (result == Result::Success)
    {
        PAL_ASSERT(pPerfExperiment != nullptr);
        pPerfExperiment->SetClientData(pPlacementAddr);

        (*ppPerfExperiment) = PAL_PLACEMENT_NEW(pPlacementAddr) PerfExperimentDecorator(pPerfExperiment, this);
    }

    return result;
}

// =====================================================================================================================
Result DeviceDecorator::GetPrivateScreens(
    uint32*          pNumScreens,
    IPrivateScreen** ppScreens)
{
    uint32           numScreens = 0;
    IPrivateScreen*  pNextScreens[MaxPrivateScreens] = {};
    IPrivateScreen** ppNextScreens = (ppScreens == nullptr) ? nullptr : pNextScreens;

    Result result = m_pNextLayer->GetPrivateScreens(&numScreens, ppNextScreens);

    if (result == Result::Success)
    {
        PAL_ASSERT(numScreens <= MaxPrivateScreens);
        *pNumScreens = numScreens;

        if (ppScreens != nullptr)
        {
            for (uint32 nextIdx = 0; (nextIdx < MaxPrivateScreens) && (result == Result::Success); ++nextIdx)
            {
                ppScreens[nextIdx] = nullptr;

                if (pNextScreens[nextIdx] != nullptr)
                {
                    // Search our array of screen decorators to see if this screen was previously decorated.
                    // Real screens will always be at the same indices between calls to GetPrivateScreens but it's
                    // possible for emulated screens to move around. Note that we make an effort to replicate the next
                    // screen ordering in ppScreens.
                    for (uint32 idx = 0; idx < MaxPrivateScreens; ++idx)
                    {
                        if ((m_pPrivateScreens[idx] != nullptr) &&
                            (m_pPrivateScreens[idx]->GetNextLayer() == pNextScreens[nextIdx]))
                        {
                            ppScreens[nextIdx] = m_pPrivateScreens[idx];
                            break;
                        }
                    }

                    // We haven't decorated this screen on a previous call, create a new decorator.
                    if (ppScreens[nextIdx] == nullptr)
                    {
                        // Search for an empty slot in our device's array.
                        uint32 newIdx = 0;
                        while (m_pPrivateScreens[newIdx] != nullptr)
                        {
                            ++newIdx;
                        }
                        PAL_ASSERT(newIdx < MaxPrivateScreens);

                        m_pPrivateScreens[newIdx] = NewPrivateScreenDecorator(pNextScreens[nextIdx], newIdx);

                        if (m_pPrivateScreens[newIdx] == nullptr)
                        {
                            result = Result::ErrorOutOfMemory;
                        }
                        else
                        {
                            // Tell the next layer that the new decorator owns the next screen. This is how we manage
                            // the lifetime of the decorator.
                            pNextScreens[nextIdx]->BindOwner(m_pPrivateScreens[newIdx]);

                            ppScreens[nextIdx] = m_pPrivateScreens[newIdx];
                        }
                    }
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
void DeviceDecorator::GetPrivateScreenImageSizes(
    const PrivateScreenImageCreateInfo& createInfo,
    size_t*                             pImageSize,
    size_t*                             pGpuMemorySize,
    Result*                             pResult
    ) const
{
    m_pNextLayer->GetPrivateScreenImageSizes(createInfo, pImageSize, pGpuMemorySize, pResult);
    (*pImageSize)     += sizeof(ImageDecorator);
    (*pGpuMemorySize) += sizeof(GpuMemoryDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreatePrivateScreenImage(
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
                                               NextObjectAddr<ImageDecorator>(pImagePlacementAddr),
                                               NextObjectAddr<GpuMemoryDecorator>(pGpuMemoryPlacementAddr),
                                               &pNextImage,
                                               &pNextGpuMemory);

    if (result == Result::Success)
    {
        pNextImage->SetClientData(pImagePlacementAddr);
        pNextGpuMemory->SetClientData(pGpuMemoryPlacementAddr);
        (*ppImage)     = PAL_PLACEMENT_NEW(pImagePlacementAddr) ImageDecorator(pNextImage, this);
        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pGpuMemoryPlacementAddr) GpuMemoryDecorator(pNextGpuMemory, this);
    }

    return result;
}

// =====================================================================================================================
size_t DeviceDecorator::GetSwapChainSize(
    const SwapChainCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    SwapChainCreateInfo nextCreateInfo = createInfo;

    for (uint32 i = 0; i < (XdmaMaxDevices - 1); i++)
    {
        nextCreateInfo.pSlaveDevices[i] = NextDevice(createInfo.pSlaveDevices[i]);
    }

    return m_pNextLayer->GetSwapChainSize(nextCreateInfo, pResult) + sizeof(SwapChainDecorator);
}

// =====================================================================================================================
Result DeviceDecorator::CreateSwapChain(
    const SwapChainCreateInfo& createInfo,
    void*                      pPlacementAddr,
    ISwapChain**               ppSwapChain)
{
    ISwapChain*         pSwapChain     = nullptr;
    SwapChainCreateInfo nextCreateInfo = createInfo;

    for (uint32 i = 0; i < (XdmaMaxDevices - 1); i++)
    {
        nextCreateInfo.pSlaveDevices[i] = NextDevice(createInfo.pSlaveDevices[i]);
    }

    const Result result = m_pNextLayer->CreateSwapChain(nextCreateInfo,
                                                        NextObjectAddr<SwapChainDecorator>(pPlacementAddr),
                                                        &pSwapChain);

    if (result == Result::Success)
    {
        PAL_ASSERT(pSwapChain != nullptr);
        pSwapChain->SetClientData(pPlacementAddr);
        (*ppSwapChain) = PAL_PLACEMENT_NEW(pPlacementAddr) SwapChainDecorator(pSwapChain, this);
    }

    return result;
}

// =====================================================================================================================
Result DeviceDecorator::GetExternalSharedImageSizes(
    const ExternalImageOpenInfo& openInfo,
    size_t*                      pImageSize,
    size_t*                      pGpuMemorySize,
    ImageCreateInfo*             pImgCreateInfo) const
{
    Result result = m_pNextLayer->GetExternalSharedImageSizes(openInfo, pImageSize, pGpuMemorySize, pImgCreateInfo);

    (*pImageSize)     += sizeof(ImageDecorator);
    (*pGpuMemorySize) += sizeof(GpuMemoryDecorator);

    return result;
}

// =====================================================================================================================
Result DeviceDecorator::OpenExternalSharedImage(
    const ExternalImageOpenInfo& openInfo,
    void*                        pImagePlacementAddr,
    void*                        pGpuMemoryPlacementAddr,
    GpuMemoryCreateInfo*         pMemCreateInfo,
    IImage**                     ppImage,
    IGpuMemory**                 ppGpuMemory)
{
    IImage*      pNextImage     = nullptr;
    IGpuMemory*  pNextGpuMemory = nullptr;
    const Result result         =
        m_pNextLayer->OpenExternalSharedImage(openInfo,
                                              NextObjectAddr<ImageDecorator>(pImagePlacementAddr),
                                              NextObjectAddr<GpuMemoryDecorator>(pGpuMemoryPlacementAddr),
                                              pMemCreateInfo,
                                              &pNextImage,
                                              &pNextGpuMemory);

    if (result == Result::Success)
    {
        pNextImage->SetClientData(pImagePlacementAddr);
        pNextGpuMemory->SetClientData(pGpuMemoryPlacementAddr);
        (*ppImage)     = PAL_PLACEMENT_NEW(pImagePlacementAddr) ImageDecorator(pNextImage, this);
        (*ppGpuMemory) = PAL_PLACEMENT_NEW(pGpuMemoryPlacementAddr) GpuMemoryDecorator(pNextGpuMemory, this);
    }

    return result;
}

// =====================================================================================================================
Result DeviceDecorator::SetPowerProfile(
    PowerProfile        profile,
    CustomPowerProfile* pInfo)
{
    CustomPowerProfile  nextInfo  = {};
    CustomPowerProfile* pNextInfo = nullptr;

    if (pInfo != nullptr)
    {
        nextInfo = *pInfo;
        nextInfo.pScreen = NextPrivateScreen(pInfo->pScreen);

        pNextInfo = &nextInfo;
    }
    return m_pNextLayer->SetPowerProfile(profile, pNextInfo);
}

// =====================================================================================================================
Result DeviceDecorator::InitBusAddressableGpuMemory(
    IQueue*           pQueue,
    uint32            gpuMemCount,
    IGpuMemory*const* ppGpuMemList)
{
    AutoBuffer<IGpuMemory*, 128, PlatformDecorator> nextGpuMemory(gpuMemCount, m_pPlatform);

    Result result = Result::Success;

    if (nextGpuMemory.Capacity() < gpuMemCount)
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        for (uint32 i = 0; i < gpuMemCount; i++)
        {
            nextGpuMemory[i] = NextGpuMemory(ppGpuMemList[i]);
        }

        result = m_pNextLayer->InitBusAddressableGpuMemory(NextQueue(pQueue),
                                                           gpuMemCount,
                                                           &nextGpuMemory[0]);

        // Accessing IGpuMemory::m_desc is intented to be a non-virtual and inlined call to avoid extra CPU cost for
        // PAL clients, because IGpuMemory::m_desc rarely update after creation of IGpuMemory.
        // Writing BusAddressable is one of the exceptions that IGpuMemory::m_desc is changed, so updates need to be
        // populated to GpuMemoryDecorator in each layer.
        for (uint32 i = 0; i < gpuMemCount; i++)
        {
            static_cast<GpuMemoryDecorator*>(ppGpuMemList[i])->PopulateNextLayerDesc();
        }
    }

    return result;
}

// =====================================================================================================================
const CmdPostProcessFrameInfo* CmdBufferDecorator::NextCmdPostProcessFrameInfo(
    const CmdPostProcessFrameInfo& postProcessInfo,
    CmdPostProcessFrameInfo*       pNextPostProcessInfo)
{
    PAL_ASSERT(pNextPostProcessInfo != nullptr);

    pNextPostProcessInfo->flags.u32All = postProcessInfo.flags.u32All;

    if (postProcessInfo.flags.srcIsTypedBuffer != 0)
    {
        pNextPostProcessInfo->pSrcTypedBuffer = NextGpuMemory(postProcessInfo.pSrcTypedBuffer);
    }
    else
    {
        pNextPostProcessInfo->pSrcImage = NextImage(postProcessInfo.pSrcImage);
    }

    pNextPostProcessInfo->fullScreenFrameMetadataControlFlags.u32All =
        postProcessInfo.fullScreenFrameMetadataControlFlags.u32All;

    return pNextPostProcessInfo;
}

// =====================================================================================================================
// Abstracts private screen decorator creation so that subclasses of DeviceDecorator can use their own private screen
// decorators without the need to reimplement all of the GetPrivateScreens logic.
PrivateScreenDecorator* DeviceDecorator::NewPrivateScreenDecorator(
    IPrivateScreen* pNextScreen,
    uint32          deviceIdx)
{
    constexpr size_t Size = sizeof(PrivateScreenDecorator);

    PrivateScreenDecorator* pDecorator     = nullptr;
    void*                   pPlacementAddr = PAL_MALLOC(Size, m_pPlatform, AllocInternal);

    if (pPlacementAddr != nullptr)
    {
        pNextScreen->SetClientData(pPlacementAddr);
        pDecorator = PAL_PLACEMENT_NEW(pPlacementAddr) PrivateScreenDecorator(pNextScreen, this, deviceIdx);
    }

    return pDecorator;
}

// =====================================================================================================================
// Called by the next layer when a private screen is destroyed.
void PAL_STDCALL DeviceDecorator::DestroyPrivateScreen(
    void* pOwner) // This layer sets the owner of the next layer's private screen to this layer's decorator.
{
    PrivateScreenDecorator*const pScreen      = static_cast<PrivateScreenDecorator*>(pOwner);
    DeviceDecorator*const        pDevice      = pScreen->GetDevice();
    DestroyNotificationFunc      pfnOnDestroy = pDevice->GetFinalizeInfo().privateScreenNotifyInfo.pfnOnDestroy;

    // Call the destroy callback of the layer above us with the owner they provided to us.
    if (pfnOnDestroy != nullptr)
    {
        pfnOnDestroy(pScreen->GetOwner());
    }

    // Destroy this layer's decorator and null-out its pointer in the device's screen array. It's important that we
    // null-out the device's pointer so that we can reuse it.
    PAL_ASSERT(pDevice->m_pPrivateScreens[pScreen->GetIndex()] == pScreen);
    pDevice->m_pPrivateScreens[pScreen->GetIndex()] = nullptr;

    pScreen->~PrivateScreenDecorator();
    PAL_FREE(pScreen, pDevice->GetPlatform());
}

// =====================================================================================================================
void PAL_STDCALL CmdBufferFwdDecorator::CmdSetUserDataDecoratorCs(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    auto*const pNextLayer = static_cast<CmdBufferFwdDecorator*>(pCmdBuffer)->GetNextLayer();
    pNextLayer->CmdSetUserData(PipelineBindPoint::Compute, firstEntry, entryCount, pEntryValues);
}

// =====================================================================================================================
void PAL_STDCALL CmdBufferFwdDecorator::CmdSetUserDataDecoratorGfx(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    auto*const pNextLayer = static_cast<CmdBufferFwdDecorator*>(pCmdBuffer)->GetNextLayer();
    pNextLayer->CmdSetUserData(PipelineBindPoint::Graphics, firstEntry, entryCount, pEntryValues);
}

// =====================================================================================================================
void CmdBufferFwdDecorator::CmdBindTargets(
    const BindTargetParams& params)
{
    BindTargetParams nextParams = params;

    for (uint32 i = 0; i < params.colorTargetCount; i++)
    {
        nextParams.colorTargets[i].pColorTargetView = NextColorTargetView(params.colorTargets[i].pColorTargetView);
    }

    nextParams.depthTarget.pDepthStencilView = NextDepthStencilView(params.depthTarget.pDepthStencilView);

    m_pNextLayer->CmdBindTargets(nextParams);
}

// =====================================================================================================================
void CmdBufferFwdDecorator::CmdBarrier(
    const BarrierInfo& barrierInfo)
{
    PlatformDecorator*const pPlatform = m_pDevice->GetPlatform();
    AutoBuffer<const IGpuEvent*,  16, PlatformDecorator> gpuEvents(barrierInfo.gpuEventWaitCount, pPlatform);
    AutoBuffer<const IImage*,     16, PlatformDecorator> targets(barrierInfo.rangeCheckedTargetWaitCount, pPlatform);
    AutoBuffer<BarrierTransition, 32, PlatformDecorator> transitions(barrierInfo.transitionCount, pPlatform);

    if ((gpuEvents.Capacity()   < barrierInfo.gpuEventWaitCount)           ||
        (targets.Capacity()     < barrierInfo.rangeCheckedTargetWaitCount) ||
        (transitions.Capacity() < barrierInfo.transitionCount))
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        BarrierInfo nextBarrierInfo = barrierInfo;

        for (uint32 i = 0; i < barrierInfo.gpuEventWaitCount; i++)
        {
            gpuEvents[i] = NextGpuEvent(barrierInfo.ppGpuEvents[i]);
        }
        nextBarrierInfo.ppGpuEvents = &gpuEvents[0];

        for (uint32 i = 0; i < barrierInfo.rangeCheckedTargetWaitCount; i++)
        {
            targets[i] = NextImage(barrierInfo.ppTargets[i]);
        }
        nextBarrierInfo.ppTargets = &targets[0];

        for (uint32 i = 0; i < barrierInfo.transitionCount; i++)
        {
            transitions[i]                  = barrierInfo.pTransitions[i];
            transitions[i].imageInfo.pImage = NextImage(barrierInfo.pTransitions[i].imageInfo.pImage);
        }
        nextBarrierInfo.pTransitions = &transitions[0];

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 751
        nextBarrierInfo.pSplitBarrierGpuEvent = NextGpuEvent(barrierInfo.pSplitBarrierGpuEvent);
#endif

        m_pNextLayer->CmdBarrier(nextBarrierInfo);
    }
}

// =====================================================================================================================
uint32 CmdBufferFwdDecorator::CmdRelease(
    const AcquireReleaseInfo& releaseInfo)
{
    PlatformDecorator*const pPlatform = m_pDevice->GetPlatform();
    AutoBuffer<ImgBarrier, 32, PlatformDecorator> imageBarriers(releaseInfo.imageBarrierCount, pPlatform);

    uint32 syncToken = 0;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 731
    AutoBuffer<MemBarrier, 32, PlatformDecorator> memoryBarriers(releaseInfo.memoryBarrierCount, pPlatform);
    if (memoryBarriers.Capacity() < releaseInfo.memoryBarrierCount)
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
#endif
    if (imageBarriers.Capacity() < releaseInfo.imageBarrierCount)
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        AcquireReleaseInfo nextReleaseInfo = releaseInfo;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 731
        for (uint32 i = 0; i < releaseInfo.memoryBarrierCount; i++)
        {
            memoryBarriers[i]                   = releaseInfo.pMemoryBarriers[i];
            memoryBarriers[i].memory.pGpuMemory = NextGpuMemory(releaseInfo.pMemoryBarriers[i].memory.pGpuMemory);
        }
        nextReleaseInfo.pMemoryBarriers = &memoryBarriers[0];
#endif

        for (uint32 i = 0; i < releaseInfo.imageBarrierCount; i++)
        {
            imageBarriers[i]        = releaseInfo.pImageBarriers[i];
            imageBarriers[i].pImage = NextImage(releaseInfo.pImageBarriers[i].pImage);
        }
        nextReleaseInfo.pImageBarriers = &imageBarriers[0];

        syncToken = m_pNextLayer->CmdRelease(nextReleaseInfo);
    }

    return syncToken;
}

// =====================================================================================================================
void CmdBufferFwdDecorator::CmdAcquire(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    syncTokenCount,
    const uint32*             pSyncTokens)
{
    PlatformDecorator*const pPlatform = m_pDevice->GetPlatform();
    AutoBuffer<ImgBarrier, 32, PlatformDecorator> imageBarriers(acquireInfo.imageBarrierCount, pPlatform);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 731
    AutoBuffer<MemBarrier, 32, PlatformDecorator> memoryBarriers(acquireInfo.memoryBarrierCount, pPlatform);
    if (memoryBarriers.Capacity() < acquireInfo.memoryBarrierCount)
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
#endif
    if (imageBarriers.Capacity() < acquireInfo.imageBarrierCount)
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        AcquireReleaseInfo nextAcquireInfo = acquireInfo;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 731
        for (uint32 i = 0; i < acquireInfo.memoryBarrierCount; i++)
        {
            memoryBarriers[i]                   = acquireInfo.pMemoryBarriers[i];
            memoryBarriers[i].memory.pGpuMemory = NextGpuMemory(acquireInfo.pMemoryBarriers[i].memory.pGpuMemory);
        }
        nextAcquireInfo.pMemoryBarriers = &memoryBarriers[0];
#endif

        for (uint32 i = 0; i < acquireInfo.imageBarrierCount; i++)
        {
            imageBarriers[i]        = acquireInfo.pImageBarriers[i];
            imageBarriers[i].pImage = NextImage(acquireInfo.pImageBarriers[i].pImage);
        }
        nextAcquireInfo.pImageBarriers = &imageBarriers[0];

        m_pNextLayer->CmdAcquire(nextAcquireInfo, syncTokenCount, pSyncTokens);
    }
}

// =====================================================================================================================
void CmdBufferFwdDecorator::CmdReleaseEvent(
    const AcquireReleaseInfo& releaseInfo,
    const IGpuEvent*          pGpuEvent)
{
    PlatformDecorator* const pPlatform = m_pDevice->GetPlatform();
    AutoBuffer<ImgBarrier, 32, PlatformDecorator> imageBarriers(releaseInfo.imageBarrierCount, pPlatform);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 731
    AutoBuffer<MemBarrier, 32, PlatformDecorator> memoryBarriers(releaseInfo.memoryBarrierCount, pPlatform);
    if (memoryBarriers.Capacity() < releaseInfo.memoryBarrierCount)
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
#endif
    if (imageBarriers.Capacity() < releaseInfo.imageBarrierCount)
    {
    }
    else
    {
        AcquireReleaseInfo nextReleaseInfo = releaseInfo;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 731
        for (uint32 i = 0; i < releaseInfo.memoryBarrierCount; i++)
        {
            memoryBarriers[i]                   = releaseInfo.pMemoryBarriers[i];
            memoryBarriers[i].memory.pGpuMemory = NextGpuMemory(releaseInfo.pMemoryBarriers[i].memory.pGpuMemory);
        }
        nextReleaseInfo.pMemoryBarriers = &memoryBarriers[0];
#endif

        for (uint32 i = 0; i < releaseInfo.imageBarrierCount; i++)
        {
            imageBarriers[i]        = releaseInfo.pImageBarriers[i];
            imageBarriers[i].pImage = NextImage(releaseInfo.pImageBarriers[i].pImage);
        }
        nextReleaseInfo.pImageBarriers = &imageBarriers[0];

        m_pNextLayer->CmdReleaseEvent(nextReleaseInfo, NextGpuEvent(pGpuEvent));
    }
}

// =====================================================================================================================
void CmdBufferFwdDecorator::CmdAcquireEvent(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    gpuEventCount,
    const IGpuEvent* const*   ppGpuEvents)
{
    PlatformDecorator* const pPlatform = m_pDevice->GetPlatform();
    AutoBuffer<ImgBarrier, 32, PlatformDecorator> imageBarriers(acquireInfo.imageBarrierCount, pPlatform);

    AutoBuffer<const IGpuEvent*, 16, PlatformDecorator> nextGpuEvents(gpuEventCount, pPlatform);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 731
    AutoBuffer<MemBarrier, 32, PlatformDecorator> memoryBarriers(acquireInfo.memoryBarrierCount, pPlatform);
    if (memoryBarriers.Capacity() < acquireInfo.memoryBarrierCount)
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
#endif
    if ((imageBarriers.Capacity() < acquireInfo.imageBarrierCount)   ||
        (nextGpuEvents.Capacity() < gpuEventCount))
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        AcquireReleaseInfo nextAcquireInfo = acquireInfo;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 731
        for (uint32 i = 0; i < acquireInfo.memoryBarrierCount; i++)
        {
            memoryBarriers[i]                   = acquireInfo.pMemoryBarriers[i];
            memoryBarriers[i].memory.pGpuMemory = NextGpuMemory(acquireInfo.pMemoryBarriers[i].memory.pGpuMemory);
        }
        nextAcquireInfo.pMemoryBarriers = &memoryBarriers[0];
#endif

        for (uint32 i = 0; i < acquireInfo.imageBarrierCount; i++)
        {
            imageBarriers[i]        = acquireInfo.pImageBarriers[i];
            imageBarriers[i].pImage = NextImage(acquireInfo.pImageBarriers[i].pImage);
        }
        nextAcquireInfo.pImageBarriers = &imageBarriers[0];

        for (uint32 i = 0; i < gpuEventCount; i++)
        {
            nextGpuEvents[i] = NextGpuEvent(ppGpuEvents[i]);
        }

        m_pNextLayer->CmdAcquireEvent(nextAcquireInfo, gpuEventCount, &nextGpuEvents[0]);
    }
}

// =====================================================================================================================
void CmdBufferFwdDecorator::CmdReleaseThenAcquire(
    const AcquireReleaseInfo& barrierInfo)
{
    PlatformDecorator*const pPlatform = m_pDevice->GetPlatform();
    AutoBuffer<ImgBarrier, 32, PlatformDecorator> imageBarriers(barrierInfo.imageBarrierCount, pPlatform);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 731
    AutoBuffer<MemBarrier, 32, PlatformDecorator> memoryBarriers(barrierInfo.memoryBarrierCount, pPlatform);
    if (memoryBarriers.Capacity() < barrierInfo.memoryBarrierCount)
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
#endif
    if (imageBarriers.Capacity() < barrierInfo.imageBarrierCount)
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        AcquireReleaseInfo nextBarrierInfo = barrierInfo;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 731
        for (uint32 i = 0; i < barrierInfo.memoryBarrierCount; i++)
        {
            memoryBarriers[i]                   = barrierInfo.pMemoryBarriers[i];
            memoryBarriers[i].memory.pGpuMemory = NextGpuMemory(barrierInfo.pMemoryBarriers[i].memory.pGpuMemory);
        }
        nextBarrierInfo.pMemoryBarriers = &memoryBarriers[0];
#endif

        for (uint32 i = 0; i < barrierInfo.imageBarrierCount; i++)
        {
            imageBarriers[i]        = barrierInfo.pImageBarriers[i];
            imageBarriers[i].pImage = NextImage(barrierInfo.pImageBarriers[i].pImage);
        }
        nextBarrierInfo.pImageBarriers = &imageBarriers[0];

        m_pNextLayer->CmdReleaseThenAcquire(nextBarrierInfo);
    }
}

// =====================================================================================================================
void CmdBufferFwdDecorator::CmdExecuteNestedCmdBuffers(
    uint32            cmdBufferCount,
    ICmdBuffer*const* ppCmdBuffers)
{
    AutoBuffer<ICmdBuffer*, 16, PlatformDecorator> nextCmdBuffers(cmdBufferCount, m_pDevice->GetPlatform());

    if (nextCmdBuffers.Capacity() < cmdBufferCount)
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        for (uint32 i = 0; i < cmdBufferCount; ++i)
        {
            nextCmdBuffers[i] = NextCmdBuffer(ppCmdBuffers[i]);
        }

        m_pNextLayer->CmdExecuteNestedCmdBuffers(cmdBufferCount, &nextCmdBuffers[0]);
    }
}

// =====================================================================================================================
void CmdBufferFwdDecorator::CmdScaledCopyImage(
    const ScaledCopyInfo& copyInfo)
{
    ScaledCopyInfo nextCopyInfo = copyInfo;
    nextCopyInfo.pSrcImage      = NextImage(copyInfo.pSrcImage);
    nextCopyInfo.pDstImage      = NextImage(copyInfo.pDstImage);

    m_pNextLayer->CmdScaledCopyImage(nextCopyInfo);
}

// =====================================================================================================================
void CmdBufferFwdDecorator::CmdGenerateMipmaps(
    const GenMipmapsInfo& genInfo)
{
    GenMipmapsInfo nextGenInfo = genInfo;
    nextGenInfo.pImage         = NextImage(genInfo.pImage);

    m_pNextLayer->CmdGenerateMipmaps(nextGenInfo);
}

// =====================================================================================================================
// This constructor must be called after pNextGpuMem has been fully initialized.
GpuMemoryDecorator::GpuMemoryDecorator(
    IGpuMemory*            pNextGpuMem,
    const DeviceDecorator* pNextDevice)
    :
    m_pNextLayer(pNextGpuMem),
    m_pDevice(pNextDevice)
{
    // We must duplicate the next layer's GpuMemoryDesc or the client will get the wrong data when it calls our Desc().
    PopulateNextLayerDesc();
}

// =====================================================================================================================
PlatformDecorator::PlatformDecorator(
    const PlatformCreateInfo&    createInfo,
    const AllocCallbacks&        allocCb,
    Developer::Callback          pfnDeveloperCb,
    bool                         installDeveloperCb,
    bool                         isLayerEnabled,
    IPlatform*                   pNextPlatform)
    :
    Pal::IPlatform(allocCb),
    m_pNextLayer(pNextPlatform),
    m_deviceCount(0),
    m_pfnDeveloperCb(nullptr),
    m_pClientPrivateData(nullptr),
    m_installDeveloperCb(installDeveloperCb),
    m_layerEnabled(isLayerEnabled),
#if  (PAL_CLIENT_INTERFACE_MAJOR_VERSION>= 734)
    m_clientApiId(createInfo.clientApiId),
#else
    m_clientApiId(ClientApi::Vulkan),
#endif
    m_logDirCreated(false)
{
    memset(&m_pDevices[0], 0, sizeof(m_pDevices));
    memset(m_logDirPath, 0, sizeof(m_logDirPath));

    if (installDeveloperCb)
    {
        IPlatform::InstallDeveloperCb(pNextPlatform, pfnDeveloperCb, this);
    }
}

// =====================================================================================================================
PlatformDecorator::~PlatformDecorator()
{
    TearDownGpus();
}

// =====================================================================================================================
Result PlatformDecorator::Init()
{
    return IPlatform::Init();
}

// =====================================================================================================================
void PlatformDecorator::TearDownGpus()
{
    for (uint32 gpu = 0; gpu < m_deviceCount; ++gpu)
    {
        if (m_pDevices[gpu] != nullptr)
        {
            const Result result = m_pDevices[gpu]->Cleanup();
            PAL_ASSERT(result == Result::Success);

            PAL_SAFE_DELETE(m_pDevices[gpu], this);
        }
    }

    memset(&m_pDevices[0], 0, sizeof(m_pDevices));
    m_deviceCount = 0;
}

// =====================================================================================================================
Result PlatformDecorator::EnumerateDevices(
    uint32*    pDeviceCount,
    IDevice*   pDevices[MaxDevices])
{
    // We must tear down our GPUs before calling EnumerateDevices() because TearDownGpus() will call Cleanup() which
    // will destroy any state set by the lower layers in EnumerateDevices().
    TearDownGpus();

    Result result = m_pNextLayer->EnumerateDevices(pDeviceCount, pDevices);

    if (result == Result::Success)
    {
        m_deviceCount = (*pDeviceCount);
        for (uint32 gpu = 0; gpu < m_deviceCount; gpu++)
        {
            m_pDevices[gpu] = PAL_NEW(DeviceDecorator, this, SystemAllocType::AllocObject)(this, pDevices[gpu]);
            pDevices[gpu]->SetClientData(m_pDevices[gpu]);
            pDevices[gpu]   = m_pDevices[gpu];

            if (m_pDevices[gpu] == nullptr)
            {
                result = Result::ErrorOutOfMemory;
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
size_t PlatformDecorator::GetScreenObjectSize() const
{
    return m_pNextLayer->GetScreenObjectSize() + sizeof(ScreenDecorator);
}

// =====================================================================================================================
Result PlatformDecorator::GetScreens(
    uint32*  pScreenCount,
    void*    pStorage[MaxScreens],
    IScreen* pScreens[MaxScreens])
{
    PAL_ASSERT(pScreenCount != nullptr);
    PAL_ASSERT(pStorage != nullptr);
    PAL_ASSERT(pScreens != nullptr);

    IScreen* pNextScreens[MaxScreens] = {};
    void* pNextStorage[MaxScreens] = {};

    for (uint32 i = 0; i < MaxScreens; i++)
    {
        PAL_ASSERT(pStorage[i] != nullptr);

        pNextStorage[i] = NextObjectAddr<ScreenDecorator>(pStorage[i]);
    }

    Result result = m_pNextLayer->GetScreens(pScreenCount, pNextStorage, pNextScreens);

    if (result == Result::Success)
    {
        const uint32 outScreenCount = *pScreenCount;
        for (uint32 screen = 0; screen < outScreenCount; screen++)
        {
            PAL_ASSERT(pNextScreens[screen] != nullptr);
            pNextScreens[screen]->SetClientData(pStorage[screen]);

            pScreens[screen] = PAL_PLACEMENT_NEW(pStorage[screen]) ScreenDecorator(pNextScreens[screen],
                                                                                   &m_pDevices[0],
                                                                                   m_deviceCount);
        }
    }

    return result;
}

// =====================================================================================================================
void PlatformDecorator::InstallDeveloperCb(
    Developer::Callback pfnDeveloperCb,
    void*               pPrivateData)
{
    if (m_installDeveloperCb)
    {
        m_pfnDeveloperCb     = pfnDeveloperCb;
        m_pClientPrivateData = pPrivateData;
    }
    else
    {
        IPlatform::InstallDeveloperCb(m_pNextLayer, pfnDeveloperCb, pPrivateData);
    }
}

// =====================================================================================================================
// Default layer event callback. For layers that do not need a callback, this will perform and necessary unraveling
// of the layered objects upward.
void PAL_STDCALL PlatformDecorator::DefaultDeveloperCb(
    void*                   pPrivateData,
    const uint32            deviceIndex,
    Developer::CallbackType type,
    void*                   pCbData)
{
    PAL_ASSERT(pPrivateData != nullptr);
    PlatformDecorator* pPlatform = static_cast<PlatformDecorator*>(pPrivateData);

    switch (type)
    {
    case Developer::CallbackType::ImageBarrier:
        PAL_ASSERT(pCbData != nullptr);
        TranslateBarrierEventData(pCbData);
        break;
    case Developer::CallbackType::DrawDispatch:
        PAL_ASSERT(pCbData != nullptr);
        TranslateDrawDispatchData(pCbData);
        break;
    case Developer::CallbackType::BindPipeline:
        PAL_ASSERT(pCbData != nullptr);
        TranslateBindPipelineData(pCbData);
        break;
    case Developer::CallbackType::AllocGpuMemory: // fallthrough intentional
    case Developer::CallbackType::FreeGpuMemory:
    case Developer::CallbackType::SubAllocGpuMemory:
    case Developer::CallbackType::SubFreeGpuMemory:
        PAL_ASSERT(pCbData != nullptr);
        TranslateGpuMemoryData(pCbData);
        break;
    case Developer::CallbackType::PresentConcluded:
    case Developer::CallbackType::CreateImage:
    case Developer::CallbackType::BarrierBegin:
    case Developer::CallbackType::BarrierEnd:
        break;
    case Developer::CallbackType::BindGpuMemory:
        PAL_ASSERT(pCbData != nullptr);
        TranslateBindGpuMemoryData(pCbData);
        break;
    default:
        // If we are here, there is a callback we haven't implemented above!
        PAL_ASSERT_ALWAYS();
        break;
    }

    pPlatform->DeveloperCb(deviceIndex, type, pCbData);
}

// =====================================================================================================================
Result PlatformDecorator::TurboSyncControl(
    const TurboSyncControlInput& turboSyncControlInput)
{
    Result result = Result::Success;

    if (m_layerEnabled)
    {
        TurboSyncControlInput nextTurboSyncControlInput = turboSyncControlInput;

        // When layer is enabled, unwrap GpuMemory pointers in the input struct
        for (uint32 iGpu = 0; iGpu < Pal::MaxDevices; iGpu++)
        {
            for (uint32 iSurf = 0; iSurf < Pal::TurboSyncMaxSurfaces; iSurf++)
            {
                nextTurboSyncControlInput.pPrimaryMemoryArray[iGpu][iSurf] =
                    NextGpuMemory(turboSyncControlInput.pPrimaryMemoryArray[iGpu][iSurf]);
            }
        }

        result = m_pNextLayer->TurboSyncControl(nextTurboSyncControlInput);
    }
    else
    {
        result = m_pNextLayer->TurboSyncControl(turboSyncControlInput);
    }

    return result;
}

// =====================================================================================================================
// The first device to call this function gets to create the shared log directory.
Result PlatformDecorator::CreateLogDir(
    const char* pBaseDir) // The shared directory will be created within this directory.
{
    // Prevent multiple threads from racing to create the shared directory. For example, each device could try to call
    // this function on a separate thread.
    MutexAuto lock(&m_logDirMutex);

    Result result = Result::Success;

    if (m_logDirCreated == false)
    {
        // Try to create the root log directory first, which may already exist.
        const Result tmpResult = MkDir(pBaseDir);
        result = (tmpResult == Result::AlreadyExists) ? Result::Success : tmpResult;

        if (result == Result::Success)
        {
            // Even if the dir already exists we try to set permissions in case it was device user who ceated the dir but
            // with not enough permisions.
            result = SetRwxFilePermissions(pBaseDir);
            PAL_ASSERT_MSG(result == Result::Success, "Failed to set main logs directory permissions to RWX for all");
        }

        // Create a directory name that will hold any dumped logs this session.  The name will be composed of the
        // executable name and current date/time, looking something like this: app.exe_2015-08-26_07.49.20.
        // Note that we will append a suffix if some other platform already made this directory in this same second.
        // (Yes, this can actually happen in reality.)
        char  executableNameBuffer[256] = {};
        char* pExecutableName = nullptr;

        if (result == Result::Success)
        {
            result = GetExecutableName(executableNameBuffer, &pExecutableName, sizeof(executableNameBuffer));
        }

        if (result == Result::Success)
        {
            const time_t   rawTime   = time(nullptr);
            const tm*const pTimeInfo = localtime(&rawTime);

            char  dateTimeBuffer[64] = {};
            strftime(dateTimeBuffer, sizeof(dateTimeBuffer), "%Y-%m-%d_%H.%M.%S", pTimeInfo);

            Snprintf(m_logDirPath, sizeof(m_logDirPath), "%s/%s_%s", pBaseDir, pExecutableName, dateTimeBuffer);

            // Try to create the directory. If it already exists, keep incrementing the suffix until it works.
            const size_t suffixOffset = strlen(m_logDirPath);
            uint32       suffix       = 0;

            do
            {
                Snprintf(m_logDirPath + suffixOffset, sizeof(m_logDirPath) - suffixOffset, "_%02d", suffix++);
                result = MkDir(m_logDirPath);
            }
            while (result == Result::AlreadyExists);
        }

        m_logDirCreated = (result == Result::Success);
    }

    return result;
}

// =====================================================================================================================
const char* PlatformDecorator::GetClientApiStr() const
{
    const char* pStr = "Unknown";

    switch(m_clientApiId)
    {
    case ClientApi::Pal:
        pStr = "PAL";
        break;
    case ClientApi::Dx9:
        pStr = "DirectX9";
        break;
    case ClientApi::Dx12:
        pStr = "DirectX12";
        break;
    case ClientApi::Vulkan:
        pStr = "Vulkan";
        break;
    case ClientApi::Mantle:
        pStr = "Mantle";
        break;
    case ClientApi::OpenCl:
        pStr = "OpenCL";
        break;
    case ClientApi::Hip:
        pStr = "HIP";
        break;
    }

    return pStr;
}

// =====================================================================================================================
Result QueueDecorator::Submit(
    const MultiSubmitInfo& submitInfo)
{
    Result     result    = Result::Success;
    auto*const pPlatform = m_pDevice->GetPlatform();
    AutoBuffer<PerSubQueueSubmitInfo, 64, PlatformDecorator> nextPerSubQueueInfo(
        submitInfo.perSubQueueInfoCount, pPlatform);

    uint32 cmdBufferCount = 0;
    for (uint32 i = 0; i < submitInfo.perSubQueueInfoCount; i++)
    {
        cmdBufferCount += submitInfo.pPerSubQueueInfo[i].cmdBufferCount;
    }

    AutoBuffer<ICmdBuffer*, 64, PlatformDecorator> nextCmdBuffers(Max(cmdBufferCount, 1u), pPlatform);
    AutoBuffer<CmdBufInfo, 64, PlatformDecorator>  nextCmdBufInfoList(Max(cmdBufferCount, 1u), pPlatform);
    AutoBuffer<GpuMemoryRef, 64, PlatformDecorator> nextGpuMemoryRefs(submitInfo.gpuMemRefCount, pPlatform);
    AutoBuffer<DoppRef,      64, PlatformDecorator> nextDoppRefs(submitInfo.doppRefCount, pPlatform);
    AutoBuffer<IFence*, 64, PlatformDecorator> nextFences(submitInfo.fenceCount, pPlatform);

    if ((nextPerSubQueueInfo.Capacity() < submitInfo.perSubQueueInfoCount)  ||
        (nextCmdBuffers.Capacity() < cmdBufferCount)                        ||
        (nextCmdBufInfoList.Capacity() < cmdBufferCount)                    ||
        (nextDoppRefs.Capacity() < submitInfo.doppRefCount)                 ||
        (nextGpuMemoryRefs.Capacity() < submitInfo.gpuMemRefCount)          ||
        (nextFences.Capacity() < submitInfo.fenceCount))
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        MultiSubmitInfo nextSubmitInfo    = {};
        uint32          currCmdBufIdx     = 0;
        uint32          currCmdBufInfoIdx = 0;

        memset(nextPerSubQueueInfo.Data(), 0, sizeof(PerSubQueueSubmitInfo) * submitInfo.perSubQueueInfoCount);

        for (uint32 subQueueIdx = 0; subQueueIdx < submitInfo.perSubQueueInfoCount; subQueueIdx++)
        {
            const PerSubQueueSubmitInfo& origSubQueueInfo  = submitInfo.pPerSubQueueInfo[subQueueIdx];
            PerSubQueueSubmitInfo*       pNextSubQueueInfo = &nextPerSubQueueInfo[subQueueIdx];
            pNextSubQueueInfo->cmdBufferCount = origSubQueueInfo.cmdBufferCount;

            if (origSubQueueInfo.cmdBufferCount > 0)
            {
                pNextSubQueueInfo->ppCmdBuffers = &nextCmdBuffers[currCmdBufIdx];
                for (uint32 cmdBufIdx = 0; cmdBufIdx < origSubQueueInfo.cmdBufferCount; cmdBufIdx++)
                {
                    nextCmdBuffers[currCmdBufIdx + cmdBufIdx] = NextCmdBuffer(origSubQueueInfo.ppCmdBuffers[cmdBufIdx]);
                }

                currCmdBufIdx += origSubQueueInfo.cmdBufferCount;
            }

            if (origSubQueueInfo.pCmdBufInfoList != nullptr)
            {
                PAL_ASSERT(origSubQueueInfo.cmdBufferCount > 0);
                pNextSubQueueInfo->pCmdBufInfoList = &nextCmdBufInfoList[currCmdBufInfoIdx];

                for (uint32 cmdBufIdx = 0; cmdBufIdx < origSubQueueInfo.cmdBufferCount; cmdBufIdx++)
                {
                    CmdBufInfo* pNextCmdBufInfoList = &nextCmdBufInfoList[currCmdBufInfoIdx + cmdBufIdx];
                    pNextCmdBufInfoList->u32All = origSubQueueInfo.pCmdBufInfoList[cmdBufIdx].u32All;

                    if (pNextCmdBufInfoList->isValid)
                    {
                        pNextCmdBufInfoList->pPrimaryMemory =
                            NextGpuMemory(origSubQueueInfo.pCmdBufInfoList[cmdBufIdx].pPrimaryMemory);

                        if ((pNextCmdBufInfoList->captureBegin) || (pNextCmdBufInfoList->captureEnd))
                        {
                            pNextCmdBufInfoList->pDirectCapMemory =
                                NextGpuMemory(origSubQueueInfo.pCmdBufInfoList[cmdBufIdx].pDirectCapMemory);

                            if (pNextCmdBufInfoList->privateFlip)
                            {
                                pNextCmdBufInfoList->pPrivFlipMemory =
                                    NextGpuMemory(origSubQueueInfo.pCmdBufInfoList[cmdBufIdx].pPrivFlipMemory);
                            }

                            pNextCmdBufInfoList->frameIndex = origSubQueueInfo.pCmdBufInfoList[cmdBufIdx].frameIndex;
                        }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 779
                        pNextCmdBufInfoList->pEarlyPresentEvent =
                            origSubQueueInfo.pCmdBufInfoList[cmdBufIdx].pEarlyPresentEvent;
#endif
                    }
                }

                currCmdBufInfoIdx += origSubQueueInfo.cmdBufferCount;
            }
        }

        nextSubmitInfo.pPerSubQueueInfo     = nextPerSubQueueInfo.Data();
        nextSubmitInfo.perSubQueueInfoCount = submitInfo.perSubQueueInfoCount;
        nextSubmitInfo.gpuMemRefCount       = submitInfo.gpuMemRefCount;
        nextSubmitInfo.pGpuMemoryRefs       = &nextGpuMemoryRefs[0];
        nextSubmitInfo.doppRefCount         = submitInfo.doppRefCount;
        nextSubmitInfo.pDoppRefs            = &nextDoppRefs[0];

        const IGpuMemory* pNextBlockIfFlipping[MaxBlockIfFlippingCount] = {};
        PAL_ASSERT(submitInfo.blockIfFlippingCount <= MaxBlockIfFlippingCount);
        nextSubmitInfo.blockIfFlippingCount = submitInfo.blockIfFlippingCount;
        nextSubmitInfo.ppBlockIfFlipping    = &pNextBlockIfFlipping[0];
        nextSubmitInfo.fenceCount           = submitInfo.fenceCount;
        nextSubmitInfo.ppFences             = &nextFences[0];
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 764
        nextSubmitInfo.pFreeMuxMemory       = NextGpuMemory(submitInfo.pFreeMuxMemory);
#endif

        for (uint32 i = 0; i < submitInfo.gpuMemRefCount; i++)
        {
            nextGpuMemoryRefs[i].pGpuMemory   = NextGpuMemory(submitInfo.pGpuMemoryRefs[i].pGpuMemory);
            nextGpuMemoryRefs[i].flags.u32All = submitInfo.pGpuMemoryRefs[i].flags.u32All;
        }

        for (uint32 i = 0; i < submitInfo.doppRefCount; i++)
        {
            nextDoppRefs[i].pGpuMemory   = NextGpuMemory(submitInfo.pDoppRefs[i].pGpuMemory);
            nextDoppRefs[i].flags.u32All = submitInfo.pDoppRefs[i].flags.u32All;
        }

        for (uint32 i = 0; i < submitInfo.blockIfFlippingCount; i++)
        {
            pNextBlockIfFlipping[i] = NextGpuMemory(submitInfo.ppBlockIfFlipping[i]);
        }

        for (uint32 i = 0; i < submitInfo.fenceCount; i++)
        {
            nextFences[i] = NextFence(submitInfo.ppFences[i]);
        }

        result = m_pNextLayer->Submit(nextSubmitInfo);
    }

    return result;
}

// =====================================================================================================================
Result QueueDecorator::PresentDirect(
    const PresentDirectInfo& presentInfo)
{
    PresentDirectInfo nextPresentInfo = presentInfo;

    if (presentInfo.flags.srcIsTypedBuffer)
    {
        nextPresentInfo.pSrcTypedBuffer = NextGpuMemory(presentInfo.pSrcTypedBuffer);
    }
    else
    {
        nextPresentInfo.pSrcImage = NextImage(presentInfo.pSrcImage);
    }
    if (presentInfo.flags.dstIsTypedBuffer)
    {
        nextPresentInfo.pDstTypedBuffer = NextGpuMemory(presentInfo.pDstTypedBuffer);
    }
    else
    {
        nextPresentInfo.pDstImage = NextImage(presentInfo.pDstImage);
    }

    return m_pNextLayer->PresentDirect(nextPresentInfo);
}

// =====================================================================================================================
Result QueueDecorator::PresentSwapChain(
    const PresentSwapChainInfo& presentInfo)
{
    PresentSwapChainInfo nextPresentInfo = presentInfo;
    nextPresentInfo.pSrcImage  = NextImage(presentInfo.pSrcImage);
    nextPresentInfo.pSwapChain = NextSwapChain(presentInfo.pSwapChain);

    return m_pNextLayer->PresentSwapChain(nextPresentInfo);
}

// =====================================================================================================================
Result QueueDecorator::RemapVirtualMemoryPages(
    uint32                         rangeCount,
    const VirtualMemoryRemapRange* pRanges,
    bool                           doNotWait,
    IFence*                        pFence)
{
    AutoBuffer<VirtualMemoryRemapRange, 64, PlatformDecorator> ranges(rangeCount, m_pDevice->GetPlatform());

    Result result = Result::Success;

    if (ranges.Capacity() < rangeCount)
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        for (uint32 i = 0; i < rangeCount; i++)
        {
            ranges[i]                = pRanges[i];
            ranges[i].pRealGpuMem    = NextGpuMemory(pRanges[i].pRealGpuMem);
            ranges[i].pVirtualGpuMem = NextGpuMemory(pRanges[i].pVirtualGpuMem);
        }

        result = m_pNextLayer->RemapVirtualMemoryPages(rangeCount, &ranges[0], doNotWait, NextFence(pFence));
    }

    return result;
}

// =====================================================================================================================
Result QueueDecorator::CopyVirtualMemoryPageMappings(
    uint32                                    rangeCount,
    const VirtualMemoryCopyPageMappingsRange* pRanges,
    bool                                      doNotWait)
{
    AutoBuffer<VirtualMemoryCopyPageMappingsRange, 64, PlatformDecorator> ranges(rangeCount, m_pDevice->GetPlatform());

    Result result = Result::Success;

    if (ranges.Capacity() < rangeCount)
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        for (uint32 i = 0; i < rangeCount; i++)
        {
            ranges[i]            = pRanges[i];
            ranges[i].pSrcGpuMem = NextGpuMemory(pRanges[i].pSrcGpuMem);
            ranges[i].pDstGpuMem = NextGpuMemory(pRanges[i].pDstGpuMem);
        }

        result = m_pNextLayer->CopyVirtualMemoryPageMappings(rangeCount, &ranges[0], doNotWait);
    }

    return result;
}

// =====================================================================================================================
Result ScreenDecorator::GetProperties(
    ScreenProperties* pInfo
    ) const
{
    Result result = m_pNextLayer->GetProperties(pInfo);

    if (result == Result::Success)
    {
        pInfo->pMainDevice = GetDeviceFromNextLayer(pInfo->pMainDevice);

        for (uint32 i = 0; i < pInfo->otherDeviceCount; i++)
        {
            pInfo->pOtherDevice[i] = GetDeviceFromNextLayer(pInfo->pOtherDevice[i]);
        }
    }

    return result;
}

// =====================================================================================================================
const IDevice* ScreenDecorator::GetDeviceFromNextLayer(
    const IDevice* pDevice
    ) const
{
    IDevice* pDecoratedDevice = nullptr;

    for (uint32 i = 0; i < m_deviceCount; i++)
    {
        if (pDevice == m_ppDevices[i]->GetNextLayer())
        {
            pDecoratedDevice = m_ppDevices[i];
        }
    }

    return pDecoratedDevice;
}

// =====================================================================================================================
Result SwapChainDecorator::AcquireNextImage(
    const AcquireNextImageInfo& acquireInfo,
    uint32*                     pImageIndex)
{
    AcquireNextImageInfo nextAcquireInfo = acquireInfo;
    nextAcquireInfo.pSemaphore = NextQueueSemaphore(acquireInfo.pSemaphore);
    nextAcquireInfo.pFence     = NextFence(acquireInfo.pFence);

    return m_pNextLayer->AcquireNextImage(nextAcquireInfo, pImageIndex);
}

// =====================================================================================================================
// Initialize the PipelineDecorator. Populates the m_pipelines vector.
Result PipelineDecorator::Init()
{
    Result result = Result::Success;
    for (const IPipeline* pPipeline : GetNextLayer()->GetPipelines())
    {
        result = m_pipelines.PushBack(PreviousObject(pPipeline));
        if (result != Result::Success)
        {
            break;
        }
    }
    return result;
}

// =====================================================================================================================
Result PipelineDecorator::LinkWithLibraries(
    const IShaderLibrary*const* ppLibraryList,
    uint32                      libraryCount)
{
    AutoBuffer<IShaderLibrary*, 16, PlatformDecorator> nextLibraryList(libraryCount, m_pDevice->GetPlatform());

    Result result = Result::Success;

    if (nextLibraryList.Capacity() < libraryCount)
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        for (uint32 i = 0; i < libraryCount; i++)
        {
            nextLibraryList[i] = NextShaderLibrary(ppLibraryList[i]);
        }

        result = m_pNextLayer->LinkWithLibraries(&nextLibraryList[0], libraryCount);
    }

    return result;
}

} // Pal
