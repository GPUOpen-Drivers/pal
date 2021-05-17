/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/masterQueueSemaphore.h"
#include "core/cmdBuffer.h"
#include "core/queue.h"
#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuImage.h"
#include "core/os/amdgpu/amdgpuPresentScheduler.h"
#include "core/os/amdgpu/amdgpuQueue.h"
#include "core/os/amdgpu/amdgpuSyncobjFence.h"
#include "core/os/amdgpu/amdgpuSwapChain.h"
#include "core/os/amdgpu/amdgpuWindowSystem.h"

using namespace Util;

namespace Pal
{
namespace Amdgpu
{

// =====================================================================================================================
// This function assumes that pCreateInfo has been initialized to zero.
static void GetInternalQueueInfo(
    const Pal::Device& device,
    QueueCreateInfo*   pCreateInfo)
{
    const auto& engineProps = device.EngineProperties();

    // No need to optimize something just for semaphores and fences.
    pCreateInfo->submitOptMode = SubmitOptMode::Disabled;

    // The Linux present scheduler's internal signal and present queues both only need to support fences and semaphores.
    // Select the most light-weight queue that can meet those requirements.
    if (engineProps.perEngine[EngineTypeDma].numAvailable > 0)
    {
        pCreateInfo->queueType  = QueueTypeDma;
        pCreateInfo->engineType = EngineTypeDma;
    }
    else if (engineProps.perEngine[EngineTypeCompute].numAvailable > 0)
    {
        pCreateInfo->queueType  = QueueTypeCompute;
        pCreateInfo->engineType = EngineTypeCompute;
    }
    else if (engineProps.perEngine[EngineTypeUniversal].numAvailable > 0)
    {
        pCreateInfo->queueType  = QueueTypeUniversal;
        pCreateInfo->engineType = EngineTypeUniversal;
    }
    else
    {
        // We assume we can always find at least one queue to use.
        PAL_ASSERT_ALWAYS();
    }
}

// =====================================================================================================================
size_t PresentScheduler::GetSize(
    const Device& device,
    IDevice*const pSlaveDevices[],
    WsiPlatform   wsiPlatform)
{
    QueueCreateInfo queueInfo = {};
    GetInternalQueueInfo(device, &queueInfo);

    // We need space for the object, m_pSignalQueue, and m_pPresentQueues.
    size_t objectSize = (sizeof(PresentScheduler) + (2 * device.GetQueueSize(queueInfo, nullptr)));

    // Additional present queues for slave devices may have different create info/sizes.
    for (uint32 i = 0; i < (XdmaMaxDevices - 1); i++)
    {
        Pal::Device* pDevice = static_cast<Pal::Device*>(pSlaveDevices[i]);

        if (pDevice != nullptr)
        {
            GetInternalQueueInfo(*pDevice, &queueInfo);

            objectSize += pDevice->GetQueueSize(queueInfo, nullptr);
        }
    }

    return objectSize;
}

// =====================================================================================================================
Result PresentScheduler::Create(
    Device*                 pDevice,
    IDevice*const           pSlaveDevices[],
    WindowSystem*           pWindowSystem,
    void*                   pPlacementAddr,
    Pal::PresentScheduler** ppPresentScheduler)
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppPresentScheduler != nullptr));

    auto*const pScheduler = PAL_PLACEMENT_NEW(pPlacementAddr) PresentScheduler(pDevice, pWindowSystem);
    Result     result     = pScheduler->Init(pSlaveDevices, pScheduler + 1);

    if (result == Result::Success)
    {
        *ppPresentScheduler = pScheduler;
    }
    else
    {
        pScheduler->Destroy();
    }

    return result;
}

// =====================================================================================================================
PresentScheduler::PresentScheduler(
    Device*       pDevice,
    WindowSystem* pWindowSystem)
    :
    Pal::PresentScheduler(pDevice),
    m_pCpuBltCmdBuffer(),
    m_pWindowSystem(pWindowSystem)
{
}

// =====================================================================================================================
PresentScheduler::~PresentScheduler()
{
    if (m_pCpuBltCmdBuffer != nullptr)
    {
        m_pCpuBltCmdBuffer->Destroy();
        PAL_SAFE_FREE(m_pCpuBltCmdBuffer, m_pDevice->GetPlatform());
    }
}

// =====================================================================================================================
Result PresentScheduler::Init(
    IDevice*const pSlaveDevices[],
    void*         pPlacementAddr)
{
    Result result = Result::Success;

    // Create the internal presentation queue as well as any additional internal queues for slave fullscreen presents
    QueueCreateInfo presentQueueInfo = {};
    Pal::Device*    pDevice          = m_pDevice;
    uint32          queueIndex       = 0;

    do
    {
        if (result == Result::Success)
        {
            GetInternalQueueInfo(*pDevice, &presentQueueInfo);

            if (pDevice->GetEngine(presentQueueInfo.engineType, presentQueueInfo.engineIndex) == nullptr)
            {
                // If the client didn't request this engine when they finalized the device, we need to create it.
                result = pDevice->CreateEngine(presentQueueInfo.engineType, presentQueueInfo.engineIndex);
            }
        }

        if (result == Result::Success)
        {
            result         = pDevice->CreateQueue(presentQueueInfo, pPlacementAddr, &m_pPresentQueues[queueIndex]);
            pPlacementAddr = VoidPtrInc(pPlacementAddr, pDevice->GetQueueSize(presentQueueInfo, nullptr));
        }

        pDevice = static_cast<Pal::Device*>(pSlaveDevices[queueIndex]);
        queueIndex++;
    }
    while ((pDevice != nullptr) && (queueIndex < XdmaMaxDevices));

    if (result == Result::Success)
    {
        QueueCreateInfo signalQueueInfo = {};

        GetInternalQueueInfo(*m_pDevice, &signalQueueInfo);

        PAL_ASSERT(m_pDevice->GetEngine(signalQueueInfo.engineType, signalQueueInfo.engineIndex) != nullptr);

        result         = m_pDevice->CreateQueue(signalQueueInfo, pPlacementAddr, &m_pSignalQueue);
        pPlacementAddr = VoidPtrInc(pPlacementAddr, m_pDevice->GetQueueSize(signalQueueInfo, nullptr));
    }

    if (result == Result::Success)
    {
        result = Pal::PresentScheduler::Init(pSlaveDevices, pPlacementAddr);
    }

    return result;
}

// =====================================================================================================================
// Queues a present followed by any necessary signals or waits on the given queue to reuse swap chain images.
// It will block the current thread if required to meet the requirements of the present (e.g., guarantee that the given
// image is displayed for at least one vblank).
//
// This function must do its best to continue to make progress even if an error occurs to keep the swap chain valid.
Result PresentScheduler::ProcessPresent(
    const PresentSwapChainInfo& presentInfo,
    IQueue*                     pQueue,
    bool                        isInline)
{
    SwapChain*const     pSwapChain    = static_cast<SwapChain*>(presentInfo.pSwapChain);
    const SwapChainMode swapChainMode = pSwapChain->CreateInfo().swapChainMode;

    // The Linux present scheduler doesn't support inline presents because it doesn't use queues to execute presents.
    // Unless swapChainMode is Immediate.
    PAL_ASSERT((swapChainMode == SwapChainMode::Immediate) || (isInline == false));

    // We only support these modes on Linux.
    PAL_ASSERT((swapChainMode == SwapChainMode::Immediate) ||
               (swapChainMode == SwapChainMode::Mailbox)   ||
               (swapChainMode == SwapChainMode::Fifo));

    // Ask the windowing system to present our image with the swap chain's idle fence. We don't need it to wait for
    // prior rendering because that was already done by our caller.
    PresentFence*const pIdleFence = pSwapChain->PresentIdleFence(presentInfo.imageIndex);
    pIdleFence->AssociatePriorRenderFence(pQueue);
    Result             result     = m_pWindowSystem->Present(presentInfo,
                                                             nullptr,
                                                             pIdleFence);

    if (swapChainMode == SwapChainMode::Mailbox)
    {
        // The image has been submitted to the mailbox so we consider the present complete.
        const Result completedResult = pSwapChain->PresentComplete(pQueue, presentInfo.imageIndex);
        result = CollapseResults(result, completedResult);
    }
    else
    {
        if (swapChainMode == SwapChainMode::Fifo)
        {
            // Present returns as soon as the windowing system has queued our request. To meet FIFO's requirements we
            // must wait until that request has been submitted to hardware.
            const Result waitResult = m_pWindowSystem->WaitForLastImagePresented();
            result = CollapseResults(result, waitResult);
        }

        // Otherwise we must be doing a blit present and would rather wait for it to complete now so that the
        // application can reacquire the image as quickly as possible.
        const Result completedResult = pSwapChain->PresentComplete(pQueue, presentInfo.imageIndex);
        result = CollapseResults(result, completedResult);
    }

    return result;
}

// =====================================================================================================================
// Copies an image into a linear buffer so that a present can be performed without hardware acceleration.
// This is only used for CPU presents, where it's needed because the images aren't backed by real GPU memory.
Result PresentScheduler::DoCpuPresentBlit(
    Queue*               pQueue,
    Image*               pImage)
{
    Pal::Device* pDevice = pQueue->GetDevice();
    Result  result  = Result::Success;
    if ((m_pCpuBltCmdBuffer != nullptr) &&
        (static_cast<CmdBuffer*>(m_pCpuBltCmdBuffer)->GetEngineType() != pQueue->GetEngineType()))
    {
        // We're using a different type of queue, so we need to recreate our CmdBuffer, not just reset it.
        m_pCpuBltCmdBuffer->Destroy();
        PAL_SAFE_FREE(m_pCpuBltCmdBuffer, pDevice->GetPlatform());
    }

    // Create a cmd buffer if we don't already have one (or on a different kind of queue)
    if (m_pCpuBltCmdBuffer == nullptr)
    {
        CmdBufferCreateInfo createInfo = { };
        createInfo.pCmdAllocator = pDevice->InternalCmdAllocator(pQueue->GetEngineType());
        createInfo.queueType     = pQueue->Type();
        createInfo.engineType    = pQueue->GetEngineType();

        CmdBufferInternalCreateInfo internalInfo = { };
        internalInfo.flags.isInternal = 1;

        result = pDevice->CreateInternalCmdBuffer(createInfo,
                              internalInfo,
                              reinterpret_cast<CmdBuffer**>(&m_pCpuBltCmdBuffer));
    }

    // Lazily create (linear) memory to copy the presented image into
    if ((result == Result::Success) && (pImage->GetPresentableBuffer() == nullptr))
    {
        result = pImage->CreatePresentableBuffer();
    }

    if (result == Result::Success)
    {
        PAL_ASSERT(m_pCpuBltCmdBuffer != nullptr);
        result = m_pCpuBltCmdBuffer->Reset(nullptr, true);
    }

    if (result == Result::Success)
    {
        CmdBufferBuildInfo info = {};
        info.flags.optimizeOneTimeSubmit = 1;

        result = m_pCpuBltCmdBuffer->Begin(info);
    }

    // Actually build the copy operation
    if (result == Result::Success)
    {
        ImageLayout layout = { LayoutPresentWindowed | LayoutCopySrc, LayoutAllEngines};
        switch (pQueue->GetEngineType())
        {
        case EngineTypeUniversal:
            layout.engines = LayoutUniversalEngine;
            break;
        case EngineTypeCompute:
            layout.engines = LayoutComputeEngine;
            break;
        case EngineTypeDma:
            layout.engines = LayoutDmaEngine;
            break;
        default:
            PAL_ASSERT_ALWAYS(); // Engine not supported for presents
        }

        const ChNumFormat imgFormat = pImage->GetImageCreateInfo().swizzledFormat.format;

        MemoryImageCopyRegion copyRegion = {};
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        copyRegion.imageSubres = {ImageAspect::Color, 0, 0};
#endif
        copyRegion.imageExtent = pImage->GetImageCreateInfo().extent;

        copyRegion.numSlices = 1;
        copyRegion.gpuMemoryRowPitch = copyRegion.imageExtent.width * Formats::BytesPerPixel(imgFormat);
        copyRegion.gpuMemoryDepthPitch = copyRegion.imageExtent.height * copyRegion.gpuMemoryRowPitch;

        // Copy the image data to linear memory
        m_pCpuBltCmdBuffer->CmdCopyImageToMemory(*pImage,
                layout,
                *pImage->GetPresentableBuffer(),
                1,
                &copyRegion);

        // Ensure it's CPU-visible
        BarrierInfo barrier   = {};
        HwPipePoint pipePoint = HwPipePoint::HwPipePostBlt;

        barrier.waitPoint          = HwPipePoint::HwPipePostIndexFetch;
        barrier.pipePointWaitCount = 1;
        barrier.pPipePoints        = &pipePoint;
        barrier.globalSrcCacheMask = CoherCopy;
        barrier.globalDstCacheMask = CoherCpu;

        m_pCpuBltCmdBuffer->CmdBarrier(barrier);

        result = m_pCpuBltCmdBuffer->End();
    }

    // Finally, execute the GPU work
    if (result == Result::Success)
    {
        PerSubQueueSubmitInfo perSubQueueInfo = { 1, &m_pCpuBltCmdBuffer };
        MultiSubmitInfo submitInfo            = {};
        submitInfo.perSubQueueInfoCount       = 1;
        submitInfo.pPerSubQueueInfo           = &perSubQueueInfo;

        result = pQueue->Submit(submitInfo);
    }

    return result;
}

// =====================================================================================================================
Result PresentScheduler::PreparePresent(
    IQueue*              pQueue,
    PresentSchedulerJob* pJob)
{
    Result result = Result::Success;

    Queue* pAmdGpuQueue = static_cast<Queue*>(pQueue);
    if (pAmdGpuQueue->GetDevice()->Settings().forcePresentViaCpuBlt)
    {
        result = DoCpuPresentBlit(pAmdGpuQueue, static_cast<Image*>(pJob->GetPresentInfo().pSrcImage));
    }

    return result;
}

// =====================================================================================================================
// Must clean up any dangling synchronization state in the event that we fail to queue a present job.
Result PresentScheduler::FailedToQueuePresentJob(
    const PresentSwapChainInfo& presentInfo,
    IQueue*                     pQueue)
{
    // We must signal the image's idle fence because we're about to wait on it.
    SwapChain*const pSwapChain = static_cast<SwapChain*>(presentInfo.pSwapChain);
    Result          result     = pSwapChain->PresentIdleFence(presentInfo.imageIndex)->Trigger();

    // Now call PresentComplete to fix the swap chain.
    const Result completedResult = pSwapChain->PresentComplete(pQueue, presentInfo.imageIndex);
    return CollapseResults(result, completedResult);
}

// =====================================================================================================================
Result PresentScheduler::SignalOnAcquire(
    IQueueSemaphore* pPresentComplete,
    IQueueSemaphore* pSemaphore,
    IFence* pFence)
{
    Result result = Result::Success;

    if (static_cast<Device*>(m_pDevice)->GetSemaphoreType() == SemaphoreType::SyncObj)
    {
        if ((result == Result::Success) && (pPresentComplete != nullptr))
        {
            result = m_pSignalQueue->WaitQueueSemaphore(pPresentComplete);
        }

        if (result == Result::Success)
        {
            amdgpu_syncobj_handle syncObjects[2] = { 0, 0 };
            uint32 numSyncObj = 0;

            if (pSemaphore != nullptr)
            {
                static_cast<MasterQueueSemaphore*>(pSemaphore)->EarlySignal();
                amdgpu_semaphore_handle hSemaphore = static_cast<const QueueSemaphore*>(pSemaphore)->GetSyncObjHandle();
                syncObjects[numSyncObj] = reinterpret_cast<uintptr_t>(hSemaphore);
                numSyncObj++;
            }

            if (pFence != nullptr)
            {
                static_cast<Queue*>(m_pSignalQueue)->AssociateFenceWithContext(pFence);
                syncObjects[numSyncObj] = static_cast<const SyncobjFence*>(pFence)->SyncObjHandle();
                numSyncObj++;
            }

            if (numSyncObj > 0)
            {
                result = static_cast<Device*>(m_pDevice)->SignalSyncObject(syncObjects, numSyncObj);
            }

            PAL_ASSERT(result == Result::Success);
        }
    }
    else
    {
        result = Pal::PresentScheduler::SignalOnAcquire(pPresentComplete, pSemaphore, pFence);
    }

    return result;
}

// =====================================================================================================================
// The CanInlinePresent function should return true if its possible and desirable to immediately queue the present
// on the given application queue. Inline presents cannot stall the calling thread.
bool PresentScheduler::CanInlinePresent(
    const PresentSwapChainInfo& presentInfo,
    const IQueue&               queue
    ) const
{
    SwapChain*const     pSwapChain    = static_cast<SwapChain*>(presentInfo.pSwapChain);
    const SwapChainMode swapChainMode = pSwapChain->CreateInfo().swapChainMode;

    bool canInline = (swapChainMode == SwapChainMode::Immediate);

    const Queue* pAmdGpuQueue = static_cast<const Queue*>(&queue);
    if (pAmdGpuQueue->GetDevice()->Settings().forcePresentViaCpuBlt)
    {
        canInline = false;
    }

    return canInline;
}

} // Amdgpu
} // Pal
