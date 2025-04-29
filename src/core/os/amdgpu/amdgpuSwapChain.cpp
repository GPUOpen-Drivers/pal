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

#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuPresentScheduler.h"
#include "core/os/amdgpu/amdgpuScreen.h"
#include "core/os/amdgpu/amdgpuSwapChain.h"
#include "core/os/amdgpu/amdgpuWindowSystem.h"
#include "util/lnx/lnxTimeout.h"

#include <time.h>

using namespace Util;
using namespace std::chrono_literals;

namespace Pal
{
namespace Amdgpu
{

// =====================================================================================================================
size_t SwapChain::GetSize(
    const SwapChainCreateInfo& createInfo,
    const Device&              device)
{
    // In addition to this object, the Linux swap chain has to reserve space for:
    // - A window system for the current platform.
    // - One PresentFence for each swap chain image.
    // - A Linux present scheduler for the parent class.
    // - Enough space for all of the OS-independent objects in the parent class.
    return (sizeof(SwapChain)                                                                   +
            WindowSystem::GetSize(createInfo.wsiPlatform)                                       +
            (createInfo.imageCount * PresentFence::GetSize(createInfo.wsiPlatform))             +
            PresentScheduler::GetSize(device, createInfo.pSlaveDevices, createInfo.wsiPlatform) +
            // No need to create present complete semaphore.
            Pal::SwapChain::GetPlacementSize(createInfo, device, false));
}

// =====================================================================================================================
Result SwapChain::Create(
    const SwapChainCreateInfo& createInfo,
    Device*                    pDevice,
    void*                      pPlacementAddr,
    ISwapChain**               ppSwapChain)
{
    Result result = Result::ErrorInvalidPointer;

    if ((pPlacementAddr != nullptr) && (ppSwapChain != nullptr))
    {
        auto*const pSwapChain = PAL_PLACEMENT_NEW(pPlacementAddr) SwapChain(createInfo, pDevice);
        result                = pSwapChain->Init(pSwapChain + 1, false);

        if (result == Result::Success)
        {
            *ppSwapChain = pSwapChain;
        }
        else
        {
            pSwapChain->Destroy();
        }
    }

    return result;
}

// =====================================================================================================================
SwapChain::SwapChain(
    const SwapChainCreateInfo& createInfo,
    Device*                    pDevice)
    :
    Pal::SwapChain(createInfo, pDevice),
    m_pWindowSystem(nullptr)
{
    memset(m_pPresentIdle, 0, sizeof(m_pPresentIdle));
}

// =====================================================================================================================
SwapChain::~SwapChain()
{
    for (uint32 idx = 0; idx < m_createInfo.imageCount; ++idx)
    {
        if (m_pPresentIdle[idx] != nullptr)
        {
            m_pPresentIdle[idx]->Destroy();
            m_pPresentIdle[idx] = nullptr;
        }
    }

    if (m_pWindowSystem != nullptr)
    {
        m_pWindowSystem->Destroy();
        m_pWindowSystem = nullptr;
    }
}

// =====================================================================================================================
// Creates our Linux objects then gives our parent class a chance to create its objects.
Result SwapChain::Init(
    void* pPlacementAddr,
    bool  needPresentComplete)
{
    Device*const pLnxDevice = static_cast<Device*>(m_pDevice);

    WindowSystemCreateInfo windowSystemInfo = {};
    windowSystemInfo.platform       = m_createInfo.wsiPlatform;
    windowSystemInfo.swapChainMode  = m_createInfo.swapChainMode;

    if (m_createInfo.wsiPlatform == WsiPlatform::DirectDisplay)
    {
        PAL_ASSERT(m_createInfo.pScreen != nullptr);

        const Screen* pScreen = static_cast<Screen*>(m_createInfo.pScreen);
        windowSystemInfo.drmMasterFd = pScreen->GetDrmMasterFd();
        windowSystemInfo.connectorId = pScreen->GetConnectorId();
    }
    else
    {
        windowSystemInfo.hDisplay = m_createInfo.hDisplay;
        windowSystemInfo.hWindow  = m_createInfo.hWindow;
        windowSystemInfo.format   = m_createInfo.imageSwizzledFormat;
    }

    Result result  = WindowSystem::Create(*pLnxDevice, windowSystemInfo, pPlacementAddr, &m_pWindowSystem);

    pPlacementAddr = VoidPtrInc(pPlacementAddr, WindowSystem::GetSize(m_createInfo.wsiPlatform));

    if (result == Result::Success)
    {
        result         = PresentScheduler::Create(pLnxDevice,
                                                  m_createInfo.pSlaveDevices,
                                                  m_pWindowSystem,
                                                  pPlacementAddr,
                                                  &m_pScheduler);
        pPlacementAddr = VoidPtrInc(pPlacementAddr, PresentScheduler::GetSize(*pLnxDevice,
                                                                              m_createInfo.pSlaveDevices,
                                                                              m_createInfo.wsiPlatform));
    }

    const size_t fenceSize = PresentFence::GetSize(m_createInfo.wsiPlatform);

    bool initiallySignaled = false;

    if ((m_createInfo.wsiPlatform == WsiPlatform::DirectDisplay) &&
       ((m_createInfo.swapChainMode == SwapChainMode::Immediate) ||
        (m_createInfo.swapChainMode == SwapChainMode::Fifo)))
    {
        initiallySignaled = true;
    }

    for (uint32 idx = 0; (result == Result::Success) && (idx < m_createInfo.imageCount); ++idx)
    {
        result         = PresentFence::Create(*m_pWindowSystem, initiallySignaled, pPlacementAddr, &m_pPresentIdle[idx]);
        pPlacementAddr = VoidPtrInc(pPlacementAddr, fenceSize);
    }

    if (result == Result::Success)
    {
        result = Pal::SwapChain::Init(pPlacementAddr, needPresentComplete);
    }

    return result;
}

// =====================================================================================================================
bool SwapChain::NeedWindowSizeChangedCheck(
    ) const
{
    return m_pWindowSystem->NeedWindowSizeChangedCheck();
}

// =====================================================================================================================
void SwapChain::WaitForImageIdle(
    uint32 imageIndex)
{
    if (m_createInfo.swapChainMode != SwapChainMode::Mailbox)
    {
        // Linux presents aren't queue operations so we must manually wait for the present to complete by waiting on
        // its idle fence before we let the base class do its work. Note that we shouldn't wait in mailbox mode because
        // it has no semaphore to signal and waiting now could deadlock the algorithm.
        //
        // In DirectDisplay, the presentable image need to be signaled by the next VSync, however, if there is only one
        // presentable image, it has no chance to be signaled.
        bool doWait =
            ((m_createInfo.wsiPlatform == WsiPlatform::DirectDisplay) && (m_createInfo.imageCount == 1)) ? false : true;
        Result result = m_pPresentIdle[imageIndex]->WaitForCompletion(doWait);
        if (result == Result::Success)
        {
            m_pPresentIdle[imageIndex]->Reset();
        }
        else
        {
            // in case the present fence has not been associated with a present
            PAL_ASSERT(result == Result::ErrorFenceNeverSubmitted);
        }
    }
}

// =====================================================================================================================
// In our Linux mailbox mode implementation, this function is a busy-wait loop that polls the present idle fence of
// each image in the mailbox list until it finds at least one unused image. It would be more efficient if we could block
// the thread until any one of the idle fences became signaled but we can only wait for one fence at a time.
Result SwapChain::ReclaimUnusedImages(
    std::chrono::nanoseconds timeout)
{
    Result   result   = Result::Success;
    timespec stopTime = {};

    if (timeout > 0ns)
    {
        ComputeTimeoutExpiration(&stopTime, timeout.count());
    }

    const bool blockMode = m_pWindowSystem->SupportWaitingOnCompletion() &&
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 863
                           (timeout == std::chrono::nanoseconds::max());
#else
                           (timeout == UINT64_MAX);
#endif

    // Note that we don't need to take the unused image lock because this is the only thread that should be looking at
    // the unused image state in mailbox mode.
    while (m_unusedImageCount == 0)
    {
        m_mailedImageMutex.Lock();

        for (uint32 idx = 0; idx < m_mailedImageCount; )
        {
            PresentFence*const pFence = m_pPresentIdle[m_mailedImageList[idx]];
            Result status = Result::Success;

            if (blockMode == false)
            {
                status = pFence->WaitForCompletion(false);
            }
            else
            {
                status = pFence->IsIdle() ? Result::Success : Result::NotReady;
            }

            if (status == Result::NotReady)
            {
                idx++;
            }
            else
            {
                if (IsErrorResult(status))
                {
                    // Something went wrong but still reuse the image to prevent an application deadlock.
                    result = CollapseResults(result, status);
                }

                // Reset the fence to its initial state.
                pFence->Reset();

                // Transfer the image index from the mailbox list to the unused image queue.
                ReuseImage(m_mailedImageList[idx]);
                m_mailedImageCount--;

                for (uint32 dstIdx = idx; dstIdx < m_mailedImageCount; ++dstIdx)
                {
                    m_mailedImageList[dstIdx] = m_mailedImageList[dstIdx + 1];
                }
                break;
            }
        }

        m_mailedImageMutex.Unlock();

        // If none of the mailbox images were ready we should sleep for a bit and try again.
        if (m_unusedImageCount == 0)
        {
            if (blockMode)
            {
                m_pWindowSystem->WaitOnCompletion();
            }
            else
            {
                if ((timeout == 0ns) || IsTimeoutExpired(&stopTime))
                {
                    result = CollapseResults(result, Result::Timeout);
                    break;
                }
                else
                {
                    YieldThread();
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
bool SwapChain::OptimizedHandlingForNativeWindowSystem(
    uint32* pImageIndex)
{
    // Optimize for immediate mode only
    if (m_createInfo.swapChainMode != SwapChainMode::Immediate)
    {
        return false;
    }

    Result ret = Result::ErrorUnknown;

    if (m_pWindowSystem->SupportIdleEvent())
    {
        // Events are read here [app thread] only for immediate mode with idle event enabled. Other cases read them
        // in Present [present thread].
        m_pWindowSystem->GoThroughEvent();

        bool found = false;

        // Go through all images first
        {
            MutexAuto lock(&m_unusedImageMutex);
            for(uint32 i = 0; i < m_unusedImageCount; i++)
            {
                Result status = m_pPresentIdle[m_unusedImageQueue[i]]->WaitForCompletion(false);
                if ((status == Result::Success) ||
                    (status == Result::ErrorFenceNeverSubmitted))
                {
                    *pImageIndex = m_unusedImageQueue[i];
                    found        = true;
                    ret          = Result::Success;
                    break;
                }
            }
        }

        // Wait on idle event to find an available image,
        // The thread will be blocked in WaitOnIdleEvent()
        if (found == false)
        {
            WindowSystemImageHandle idleImage = NullImageHandle;
            // AcquireNextImage and Present might be called in different threads,
            // make sure they will not be blocked together.
            m_pWindowSystem->WaitOnIdleEvent(&idleImage);

            MutexAuto lock(&m_unusedImageMutex);

            for (uint32 i = 0; i < m_unusedImageCount; i++)
            {
                if (m_pWindowSystem->CheckIdleImage(
                    &idleImage, m_pPresentIdle[m_unusedImageQueue[i]]) == true)
                {
                    *pImageIndex = m_unusedImageQueue[i];
                    found        = true;
                    ret          = Result::Success;
                    break;
                }
            }
        }

        if (found)
        {
            m_pPresentIdle[*pImageIndex]->Reset();

            MutexAuto lock(&m_unusedImageMutex);

            for (uint32 i = 0; i < m_unusedImageCount; i++)
            {
                if (m_unusedImageQueue[i] == *pImageIndex)
                {
                    m_unusedImageCount--;
                    for (uint32 j = i; j < m_unusedImageCount; j ++)
                    {
                        m_unusedImageQueue[j] =  m_unusedImageQueue[j+1];
                    }
                    break;
                }
            }
        }
    }
    else if (m_pWindowSystem->GetWindowSystemProperties().useExplicitSync)
    {
        // 1. Prepare an array of PresentFences for all unused images
        PresentFence* unusedImagePresentFences[MaxSwapChainLength] = {nullptr};
        uint32        unusedImageCount = 0;
        {
            MutexAuto lock(&m_unusedImageMutex);
            for(uint32 i = 0; i < m_unusedImageCount; i++)
            {
                unusedImagePresentFences[i] = m_pPresentIdle[m_unusedImageQueue[i]];
            }
            unusedImageCount = m_unusedImageCount;
        }

        // 2. Wait for any image to be released. Thread may be blocked in WaitForExplicitSyncReleaseAny()
        uint32 firstSignaledIndex = -1;
        ret = m_pWindowSystem->WaitForExplicitSyncReleaseAny(unusedImagePresentFences,
                                                             unusedImageCount,
                                                             true,
                                                             &firstSignaledIndex);
        if (ret == Result::Success)
        {
            // Image has been released
            MutexAuto lock(&m_unusedImageMutex);

            uint32        releasedImageIndex = -1;
            PresentFence* pReleasedFence     = unusedImagePresentFences[firstSignaledIndex];

            // 3. Match the returned, signaled image index with the current unused image queue to return correct index
            for (uint32 i = 0; i < m_unusedImageCount; i++)
            {
                if (m_pPresentIdle[m_unusedImageQueue[i]] == pReleasedFence)
                {
                    // Released image current index
                    releasedImageIndex = m_unusedImageQueue[i];

                    // Adjust unused image queue
                    m_unusedImageCount--;
                    for (uint32 j = i; j < m_unusedImageCount; j++)
                    {
                        m_unusedImageQueue[j] = m_unusedImageQueue[j+1];
                    }
                    break;
                }
            }

            if (releasedImageIndex != -1)
            {
                // 4. Return released image index
                *pImageIndex = releasedImageIndex;
                ret          = Result::Success;
            }
        }
    }
    else
    {
        // For example, for the case where forcePresentViaCpuBlt is enabled
        ret = Result::ErrorUnavailable;
    }

    return (ret == Result::Success);
}

// =====================================================================================================================
Result SwapChain::AcquireNextImage(
    const AcquireNextImageInfo& acquireInfo,
    uint32*                     pImageIndex)
{
    Result ret = Result::Success;

    if (OptimizedHandlingForNativeWindowSystem(pImageIndex) == false)
    {
        ret = Pal::SwapChain::AcquireNextImage(acquireInfo, pImageIndex);
    }
    else if (m_createInfo.swapChainMode != SwapChainMode::Mailbox)
    {
        ret = m_pScheduler->SignalOnAcquire(m_pPresentComplete[*pImageIndex],
                                            acquireInfo.pSemaphore,
                                            acquireInfo.pFence);
    }

    return ret;
}

} // Amdgpu
} // Pal
