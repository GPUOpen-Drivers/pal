/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/os/lnx/lnxDevice.h"
#include "core/os/lnx/lnxImage.h"
#include "core/os/lnx/lnxPresentScheduler.h"
#include "core/os/lnx/lnxQueue.h"
#include "core/os/lnx/lnxSwapChain.h"
#include "core/os/lnx/lnxWindowSystem.h"

using namespace Util;

namespace Pal
{
namespace Linux
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
    WsiPlatform   wsiPlatform)
{
    QueueCreateInfo queueInfo = {};
    GetInternalQueueInfo(device, &queueInfo);

    // We need space for the object, m_pSignalQueue, and m_pPresentQueue.
    return (sizeof(PresentScheduler) + (2 * device.GetQueueSize(queueInfo, nullptr)));
}

// =====================================================================================================================
Result PresentScheduler::Create(
    Device*                 pDevice,
    WindowSystem*           pWindowSystem,
    void*                   pPlacementAddr,
    Pal::PresentScheduler** ppPresentScheduler)
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppPresentScheduler != nullptr));

    auto*const pScheduler = PAL_PLACEMENT_NEW(pPlacementAddr) PresentScheduler(pDevice, pWindowSystem);
    Result     result     = pScheduler->Init(pScheduler + 1);

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
    m_pWindowSystem(pWindowSystem),
    m_pPrevSwapChain(nullptr),
    m_prevImageIndex(0)
{
}

// =====================================================================================================================
Result PresentScheduler::Init(
    void* pPlacementAddr)
{
    Result result = Result::Success;

    QueueCreateInfo queueInfo = {};
    GetInternalQueueInfo(*m_pDevice, &queueInfo);

    if (m_pDevice->GetEngine(queueInfo.engineType, queueInfo.engineIndex) == nullptr)
    {
        // If the client didn't request this engine when they finalized the device, we need to create it.
        result = m_pDevice->CreateEngine(queueInfo.engineType, queueInfo.engineIndex);
    }

    const size_t queueSize = m_pDevice->GetQueueSize(queueInfo, nullptr);

    if (result == Result::Success)
    {
        result         = m_pDevice->CreateQueue(queueInfo, pPlacementAddr, &m_pSignalQueue);
        pPlacementAddr = VoidPtrInc(pPlacementAddr, queueSize);
    }

    if (result == Result::Success)
    {
        result         = m_pDevice->CreateQueue(queueInfo, pPlacementAddr, &m_pPresentQueue);
        pPlacementAddr = VoidPtrInc(pPlacementAddr, queueSize);
    }

    if (result == Result::Success)
    {
        result = Pal::PresentScheduler::Init(pPlacementAddr);
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
    // The Linux present scheduler doesn't support inline presents because it doesn't use queues to execute presents.
    PAL_ASSERT(isInline == false);

    SwapChain*const     pSwapChain    = static_cast<SwapChain*>(presentInfo.pSwapChain);
    const SwapChainMode swapChainMode = pSwapChain->CreateInfo().swapChainMode;

    // We only support these modes on Linux.
    PAL_ASSERT((swapChainMode == SwapChainMode::Immediate) ||
               (swapChainMode == SwapChainMode::Mailbox)   ||
               (swapChainMode == SwapChainMode::Fifo));

    // Ask the windowing system to present our image with the swap chain's idle fence. We don't need it to wait for
    // prior rendering because that was already done by our caller.
    const Image&       srcImage   = static_cast<Image&>(*presentInfo.pSrcImage);
    PresentFence*const pIdleFence = pSwapChain->PresentIdleFence(presentInfo.imageIndex);
    Result             result     = m_pWindowSystem->Present(srcImage.GetPresentPixmapHandle(),
                                                             presentInfo.presentMode,
                                                             nullptr,
                                                             pIdleFence);

    // If the previous present was a ring-mode present it's possible we needed to delay its queue semaphore signal until
    // we did this present. If so, wait for the previous present to be idle and signal its semaphore.
    if (m_pPrevSwapChain != nullptr)
    {
        const Result completedResult = m_pPrevSwapChain->PresentComplete(pQueue, m_prevImageIndex);
        result = CollapseResults(result, completedResult);

        m_pPrevSwapChain = nullptr;
        m_prevImageIndex = 0;
    }

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

        if (presentInfo.presentMode == PresentMode::Fullscreen)
        {
            // The client requested a fullscreen present which may or may not actually result in a flip. To be safe, we
            // must assume that a flip was queued which means it won't become idle until after the next present.
            m_pPrevSwapChain = pSwapChain;
            m_prevImageIndex = presentInfo.imageIndex;
        }
        else
        {
            // Otherwise we must be doing a blit present and would rather wait for it to complete now so that the
            // application can reacquire the image as quickly as possible.
            const Result completedResult = pSwapChain->PresentComplete(pQueue, presentInfo.imageIndex);
            result = CollapseResults(result, completedResult);
        }
    }

    return result;
}

// =====================================================================================================================
Result PresentScheduler::PreparePresent(
    IQueue*              pQueue,
    PresentSchedulerJob* pJob)
{
    Result result = Result::Success;

    if (static_cast<Queue*>(pQueue)->IsPendingWait())
    {
        SubmitInfo submitInfo = {};
        result = pQueue->Submit(submitInfo);
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

} // Linux
} // Pal
