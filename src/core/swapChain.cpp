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

#include "core/device.h"
#include "core/presentScheduler.h"
#include "core/swapChain.h"
#include "palQueueSemaphore.h"
using namespace Util;

namespace Pal
{

// =====================================================================================================================
// Returns the ammount of placement memory required by this class. This shouldn't include any size needed for the
// present scheduler because the OS-specifc classes are tasked with creating that object.
size_t SwapChain::GetPlacementSize(
    const SwapChainCreateInfo& createInfo,
    const Device&              device,
    bool                       needPresentComplete)
{
    size_t totalSize = 0;

    if ((createInfo.swapChainMode != SwapChainMode::Mailbox) && (needPresentComplete == true))
    {
        // We need space for one pPresentComplete semaphore per swap chain image. The semaphores must start signaled
        // because no presents have occured yet.
        QueueSemaphoreCreateInfo semaphoreCreateInfo = {};
        semaphoreCreateInfo.maxCount     = device.MaxQueueSemaphoreCount();
        semaphoreCreateInfo.initialCount = 1;

        totalSize += createInfo.imageCount * device.GetQueueSemaphoreSize(semaphoreCreateInfo, nullptr);
    }

    return totalSize;
}

// =====================================================================================================================
SwapChain::SwapChain(
    const SwapChainCreateInfo& createInfo,
    Device*                    pDevice)
    :
    m_createInfo(createInfo),
    m_pDevice(pDevice),
    m_pScheduler(nullptr),
    m_unusedImageCount(0),
    m_mailedImageCount(0)
{
    if(pDevice->DisableSwapChainAcquireBeforeSignalingClient())
    {
        const_cast<SwapChainCreateInfo&>(m_createInfo).flags.canAcquireBeforeSignaling = 0;
    }
    else
    {
        // Keep the flag provided by the client.
    }

    memset(m_unusedImageQueue, 0, sizeof(m_unusedImageQueue));
    memset(m_mailedImageList,  0, sizeof(m_mailedImageList));
    memset(m_pPresentComplete, 0, sizeof(m_pPresentComplete));

    // All images are unused and immediately available.
    for (m_unusedImageCount = 0; m_unusedImageCount < m_createInfo.imageCount; ++m_unusedImageCount)
    {
        m_unusedImageQueue[m_unusedImageCount] = m_unusedImageCount;
    }
}

// =====================================================================================================================
SwapChain::~SwapChain()
{
    // Destroy all objects owned by this class. This excludes the presentable images.
    if (m_pScheduler != nullptr)
    {
        m_pScheduler->Destroy();
        m_pScheduler = nullptr;
    }

    for (uint32 idx = 0; idx < m_createInfo.imageCount; ++idx)
    {
        if (m_pPresentComplete[idx] != nullptr)
        {
            m_pPresentComplete[idx]->Destroy();
            m_pPresentComplete[idx] = nullptr;
        }
    }
}

// =====================================================================================================================
Result SwapChain::Init(
    void* pPlacementAddr,
    bool  needPresentComplete)
{
    Result result = Result::Success;

    if (m_createInfo.swapChainMode != SwapChainMode::Mailbox)
    {
        if (result == Result::Success)
        {
            result = m_availableImageSemaphore.Init(m_createInfo.imageCount, m_createInfo.imageCount);
        }

        if (needPresentComplete == true)
        {
            // We also need space for one pPresentComplete semaphore per swap chain image. The semaphores must start
            // signaled because no presents have occured yet.
            QueueSemaphoreCreateInfo semaphoreInfo = {};
            semaphoreInfo.maxCount     = m_pDevice->MaxQueueSemaphoreCount();
            semaphoreInfo.initialCount = 1;

            const size_t semaphoreSize = m_pDevice->GetQueueSemaphoreSize(semaphoreInfo, nullptr);

            for (uint32 idx = 0; (result == Result::Success) && (idx < m_createInfo.imageCount); ++idx)
            {
                result         = m_pDevice->CreateQueueSemaphore(semaphoreInfo, pPlacementAddr, &m_pPresentComplete[idx]);
                pPlacementAddr = VoidPtrInc(pPlacementAddr, semaphoreSize);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Gets the next available swap chain image index. If none are available it will block. If this function succeeds it
// must be followed by a call to Present to release ownership of the image.
Result SwapChain::AcquireNextImage(
    const AcquireNextImageInfo& acquireInfo,
    uint32*                     pImageIndex)
{
    PAL_ASSERT(pImageIndex != nullptr);

    Result result = Result::Success;

    // If we're in mailbox mode the OS-specific logic has to search for unused images. Otherwise we use the ring mode
    // algorithm which requires a signal on m_availableImageSemaphore each time an image can be reused.
    if (m_createInfo.swapChainMode == SwapChainMode::Mailbox)
    {
        result = ReclaimUnusedImages(acquireInfo.timeout);
    }
    else
    {
        // The given timeout must be converted from a 64-bit, nanosecond time into a 32-bit, millisecond time while:
        // 1. Mapping UINT64_MAX to UINT32_MAX, indicating an infinite wait.
        // 2. Rounding up so that a positive timeout of less than a millisecond doesn't round to zero.
        constexpr uint64 NsecPerMsec = 1000 * 1000;
        const uint32     timeoutMsec = (acquireInfo.timeout == UINT64_MAX)
                                            ? UINT32_MAX
                                            : static_cast<uint32>(RoundUpQuotient(acquireInfo.timeout, NsecPerMsec));

        result = m_availableImageSemaphore.Wait(timeoutMsec);
    }

    if (result == Result::Success)
    {
        m_unusedImageMutex.Lock();

        // It shouldn't be possible, but if this triggers we somehow have all images in use.
        PAL_ASSERT(m_unusedImageCount > 0);

        // Always select the least recently used image as the next image to acquire.
        //
        // We do this instead of simply returning the smallest unused index because some applications may use the swap
        // chain index to decide which set of game state to reuse. For example, if we were to return index zero for all
        // calls to AcquireNextImage the application may attempt to reuse the same set of state for every frame which
        // means it must wait for the previous frame to be idle before building the next. Thus, while the use of
        // m_unusedImageQueue complicates the swap chain it is more application friendly.
        const uint32 imageIndex = m_unusedImageQueue[0];
        m_unusedImageCount--;

        for (uint32 dstIdx = 0; dstIdx < m_unusedImageCount; ++dstIdx)
        {
            m_unusedImageQueue[dstIdx] = m_unusedImageQueue[dstIdx + 1];
        }

        // We must release this lock before calling SignalOnAcquire to avoid deadlocking with the queue unbatching code
        // which will call ReuseImage as it unbatches Present calls.
        m_unusedImageMutex.Unlock();

        // wait for image to be idle
        WaitForImageIdle(imageIndex);

        // Signal the caller's queue semaphore and/or fence when the selected image is done being presented. Note that
        // no wait will be queued in mailbox mode because the present complete semaphore must be null.
        result = m_pScheduler->SignalOnAcquire(m_pPresentComplete[imageIndex],
                                               acquireInfo.pSemaphore,
                                               acquireInfo.pFence);

        if (result == Result::Success)
        {
            *pImageIndex = imageIndex;
        }
        else
        {
            // Note that the present scheduler is careful to avoid putting the swap chain in an invalid state if an
            // error does occur so it's safe to immediately reuse this index if SignalOnAcquire failed.
            ReuseImage(imageIndex);
        }
    }

    return result;
}

// =====================================================================================================================
// Waits for all queued presents to drain out of this swap chain's present scheduler.
Result SwapChain::WaitIdle()
{
    return m_pScheduler->WaitIdle();
}

// =====================================================================================================================
// Issues a present for an image in this swap chain using its present scheduler.
Result SwapChain::Present(
    const PresentSwapChainInfo& presentInfo,
    IQueue*                     pQueue)
{
    if (presentInfo.pSrcImage != nullptr)
    {
        if (m_pDevice->GetGfxDevice()->UpdateSppState(*presentInfo.pSrcImage))
        {
            const uint32 pixelCount = m_pDevice->GetGfxDevice()->GetPixelCount();
            const uint32 msaaRate   = m_pDevice->GetGfxDevice()->GetMsaaRate();
            m_pDevice->SelectSppTable(pixelCount, msaaRate);
        }
    }
    Result result = m_pScheduler->Present(presentInfo, pQueue);

    if ((m_createInfo.swapChainMode != SwapChainMode::Mailbox) && (m_createInfo.flags.canAcquireBeforeSignaling == 1))
    {
        // Release this image at the end of the Present call so that the application can immediately reacquire it, even
        // if we deferred the present. This permits the application to acquire an image even if its previous present is
        // still active, improving performance in applications that acquire their image long before using it.
        //
        // Note that this optimization is not compatible with mailbox mode.
        ReuseImage(presentInfo.imageIndex);
    }

    return result;
}

// =====================================================================================================================
// Called by the present scheduler when it is done scheduling a present and all necessary synchronization. The swap
// chain can submit a fence or semaphore signal on pQueue to track present completion.
Result SwapChain::PresentComplete(
    IQueue* pQueue,
    uint32  imageIndex)
{
    Result result = Result::Success;

    if (m_createInfo.swapChainMode == SwapChainMode::Mailbox)
    {
        // Now ReclaimUnusedImages can start querying this particular mailbox image to see if it's no longer in use.
        MutexAuto lock(&m_mailedImageMutex);
        m_mailedImageList[m_mailedImageCount++] = imageIndex;
    }
    else
    {
        // The ring mode algorithm requires us to signal the present complete semaphore at this time.
        if (m_pPresentComplete[imageIndex] != nullptr)
        {
            result = pQueue->SignalQueueSemaphore(m_pPresentComplete[imageIndex]);
        }

        if (m_createInfo.flags.canAcquireBeforeSignaling == 0)
        {
            // We've finished scheduling the present; ring mode requires us to release this image if we didn't do it in
            // already in Present. This path should prevent the swap chain algorithm from triggering queue batching.
            ReuseImage(imageIndex);
        }
    }

    return result;
}

// =====================================================================================================================
// Called as soon as it's safe to release the acquisition of the given image. In ring mode this may be called before the
// scheduler submits the necessary present complete semaphore signal, triggering queue batching on the next acquire.
void SwapChain::ReuseImage(
    uint32 imageIndex)
{
    m_unusedImageMutex.Lock();
    m_unusedImageQueue[m_unusedImageCount++] = imageIndex;
    m_unusedImageMutex.Unlock();

    if (m_createInfo.swapChainMode != SwapChainMode::Mailbox)
    {
        m_availableImageSemaphore.Post();
    }
}

} // Pal
