/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/os/lnx/lnxPresentScheduler.h"
#include "core/os/lnx/lnxSwapChain.h"
#include "core/os/lnx/lnxWindowSystem.h"
#include "util/lnx/lnxTimeout.h"

using namespace Util;

namespace Pal
{
namespace Linux
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
    return (sizeof(SwapChain)                                                       +
            WindowSystem::GetSize(createInfo.wsiPlatform)                           +
            (createInfo.imageCount * PresentFence::GetSize(createInfo.wsiPlatform)) +
            PresentScheduler::GetSize(device, createInfo.wsiPlatform)               +
            Pal::SwapChain::GetPlacementSize(createInfo, device));
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
        result                = pSwapChain->Init(pSwapChain + 1);

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
    void* pPlacementAddr)
{
    Device*const pLnxDevice = static_cast<Device*>(m_pDevice);

    WindowSystemCreateInfo windowSystemInfo = {};
    windowSystemInfo.platform = m_createInfo.wsiPlatform;
    windowSystemInfo.hDisplay = m_createInfo.hDisplay;
    windowSystemInfo.hWindow  = m_createInfo.hWindow;
    windowSystemInfo.format   = m_createInfo.imageSwizzledFormat;

    windowSystemInfo.swapChainMode = m_createInfo.swapChainMode;

    Result result  = WindowSystem::Create(*pLnxDevice, windowSystemInfo, pPlacementAddr, &m_pWindowSystem);

    pPlacementAddr = VoidPtrInc(pPlacementAddr, WindowSystem::GetSize(m_createInfo.wsiPlatform));

    if (result == Result::Success)
    {
        result         = PresentScheduler::Create(pLnxDevice, m_pWindowSystem, pPlacementAddr, &m_pScheduler);
        pPlacementAddr = VoidPtrInc(pPlacementAddr, PresentScheduler::GetSize(*pLnxDevice, m_createInfo.wsiPlatform));
    }

    const size_t fenceSize = PresentFence::GetSize(m_createInfo.wsiPlatform);

    for (uint32 idx = 0; (result == Result::Success) && (idx < m_createInfo.imageCount); ++idx)
    {
        result         = PresentFence::Create(*m_pWindowSystem, false, pPlacementAddr, &m_pPresentIdle[idx]);
        pPlacementAddr = VoidPtrInc(pPlacementAddr, fenceSize);
    }

    if (result == Result::Success)
    {
        result = Pal::SwapChain::Init(pPlacementAddr);
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

    if (m_createInfo.swapChainMode != SwapChainMode::Mailbox)
    {
        // Linux presents aren't queue operations so we must manually wait for the present to complete by waiting on
        // its idle fence before we let the base class do its work. Note that we shouldn't wait in mailbox mode because
        // it has no semaphore to signal and waiting now could deadlock the algorithm.
        //
        // Wait for a maximum of 2 seconds.
        constexpr uint64 Timeout = 2000000000ull;
        result = m_pPresentIdle[imageIndex]->WaitForCompletion(Timeout);

        m_pPresentIdle[imageIndex]->Reset();
    }

    const Result baseResult = Pal::SwapChain::PresentComplete(pQueue, imageIndex);
    return CollapseResults(result, baseResult);
}

// =====================================================================================================================
// In our Linux mailbox mode implementation, this function is a busy-wait loop that polls the present idle fence of
// each image in the mailbox list until it finds at least one unused image. It would be more efficient if we could block
// the thread until any one of the idle fences became signaled but we can only wait for one fence at a time.
Result SwapChain::ReclaimUnusedImages(
    uint64 timeout)
{
    Result   result   = Result::Success;
    timespec stopTime = {};

    if (timeout > 0)
    {
        ComputeTimeoutExpiration(&stopTime, timeout);
    }

    // Note that we don't need to take the unused image lock because this is the only thread that should be looking at
    // the unused image state in mailbox mode.
    while (m_unusedImageCount == 0)
    {
        m_mailedImageMutex.Lock();

        for (uint32 idx = 0; idx < m_mailedImageCount; )
        {
            PresentFence*const pFence = m_pPresentIdle[m_mailedImageList[idx]];
            const Result       status = pFence->WaitForCompletion(0);

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
            }
        }

        m_mailedImageMutex.Unlock();

        // If none of the mailbox images were ready we should sleep for a bit and try again.
        if (m_unusedImageCount == 0)
        {
            if ((timeout == 0) || IsTimeoutExpired(&stopTime))
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

    return result;
}

} // Linux
} // Pal
