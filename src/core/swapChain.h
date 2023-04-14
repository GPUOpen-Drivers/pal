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

#pragma once

#include "palMutex.h"
#include "palSemaphore.h"
#include "palSwapChain.h"

namespace Pal
{

// Forward declarations.
class Device;
class PresentScheduler;

// Maximum number of presentable images supported by swap chain.
constexpr uint32 MaxSwapChainLength = 16;

// =====================================================================================================================
// This class implements all necessary resource management and synchronization logic for the ISwapChain interface.
// Each SwapChain uses a PresentScheduler to schedule and execute its presents and signal the application's fence
// and/or semaphore when AcquireNextImage is called.
class SwapChain : public ISwapChain
{
public:
    // Part of the public ISwapChain interface.
    virtual Result AcquireNextImage(
        const AcquireNextImageInfo& acquireInfo,
        uint32*                     pImageIndex) override;

    virtual Result WaitIdle() override;

    // Part of the public IDestroyable interface.
    virtual void Destroy() override { this->~SwapChain(); }

    virtual bool NeedWindowSizeChangedCheck() const override { return true; }

    virtual Result SetHdrMetaData(const ScreenColorConfig& colorConfig) override { return Result::Unsupported; }

    // These begin and end a swap chain present. The present scheduler must call PresentComplete once it has scheduled
    // the present and all necessary synchronization.
    // Note that the DXGI swapchain is an exception to the above rule and all necessary functionality is self
    // contained in it's own class implementation.
    virtual Result Present(const PresentSwapChainInfo& presentInfo, IQueue* pQueue);

    Result PresentComplete(IQueue* pQueue, uint32 imageIndex);

    // the function to wait for image idle in the acquire time.
    virtual void WaitForImageIdle(uint32 imageIndex) { }

    const SwapChainCreateInfo& CreateInfo() const { return m_createInfo; }

protected:
    static size_t GetPlacementSize(const SwapChainCreateInfo& createInfo,
                                   const Device& device,
                                   bool needPresentComplete);

    SwapChain(const SwapChainCreateInfo& createInfo, Device* pDevice);
    virtual ~SwapChain();

    virtual Result Init(void* pPlacementAddr, bool needPresentComplete);

    // Abstracts OS-specific logic necessary to find unused images in mailbox mode.
    virtual Result ReclaimUnusedImages(uint64 timeout) { return Result::ErrorUnavailable; }

    // Called when it's safe to allow the application to reacquire the given image.
    void ReuseImage(uint32 imageIndex);

    const SwapChainCreateInfo m_createInfo;
    Device*const              m_pDevice;
    PresentScheduler*         m_pScheduler; // Created by the OS-specific subclasses.

    uint32           m_unusedImageQueue[MaxSwapChainLength]; // Indices of unused images from least to most recent.
    uint32           m_unusedImageCount;                     // The number of unused images in the queue above.
    Util::Mutex      m_unusedImageMutex;                     // Protects access to the unused image state.

    // This state is only needed by swap chains using the mailbox ordering mode.
    uint32           m_mailedImageList[MaxSwapChainLength];  // Indices of images sent to the presentation mailbox.
    uint32           m_mailedImageCount;                     // The number of images in the list above.
    Util::Mutex      m_mailedImageMutex;                     // Protects access to the above image state.

    // This state is only needed by swap chains using the ring ordering mode.
    IQueueSemaphore* m_pPresentComplete[MaxSwapChainLength]; // Signaled when each image is done being presented.
    Util::Semaphore  m_availableImageSemaphore;              // Signaled when an image is ready to be acquired.

private:

    PAL_DISALLOW_DEFAULT_CTOR(SwapChain)
    PAL_DISALLOW_COPY_AND_ASSIGN(SwapChain);
};

} // Pal

