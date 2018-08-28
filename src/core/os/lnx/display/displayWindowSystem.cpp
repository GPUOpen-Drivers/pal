/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include <poll.h>
#include "core/platform.h"
#include "core/os/lnx/display/displayWindowSystem.h"
#include "core/os/lnx/lnxDevice.h"
#include "core/os/lnx/lnxImage.h"
#include "core/os/lnx/lnxPlatform.h"
#include "core/os/lnx/lnxScreen.h"
#include "core/os/lnx/lnxSwapChain.h"
#include "palScreen.h"
#include "palSwapChain.h"
#include "palSysUtil.h"

using namespace Util;

namespace Pal
{
namespace Linux
{
// =====================================================================================================================
Result DisplayPresentFence::Create(
    const DisplayWindowSystem& windowSystem,
    bool                       initiallySignaled,
    void*                      pPlacementAddr,
    PresentFence**             ppPresentFence)
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppPresentFence != nullptr));

    auto*const pPresentFence = PAL_PLACEMENT_NEW(pPlacementAddr) DisplayPresentFence(windowSystem);
    Result     result = pPresentFence->Init(initiallySignaled);

    if (result == Result::Success)
    {
        *ppPresentFence = pPresentFence;
    }
    else
    {
        pPresentFence->Destroy();
    }

    return result;
}

// =====================================================================================================================
Result DisplayPresentFence::Init(
    bool initiallySignaled)
{
    return m_imageIdle.Init(1, 1);
}

// =====================================================================================================================
void DisplayPresentFence::Reset()
{

}

// =====================================================================================================================
Result DisplayPresentFence::Trigger()
{
    m_imageIdle.Post();
    return Result::Success;
}

// =====================================================================================================================
Result DisplayPresentFence::WaitForCompletion(
    bool doWait)
{
    Result result = Result::Success;
    if (doWait)
    {
        uint32 timeoutMsec = -1;
        result = m_imageIdle.Wait(timeoutMsec);
    }
    return result;
}

// =====================================================================================================================
DisplayPresentFence::DisplayPresentFence(
    const DisplayWindowSystem& windowSystem)
    :
    m_windowSystem(windowSystem),
    m_presentState(PresentState::Idle)
{

}

// =====================================================================================================================
DisplayPresentFence::~DisplayPresentFence()
{

}

// =====================================================================================================================
Result DisplayWindowSystem::Create(
    const Device&                 device,
    const WindowSystemCreateInfo& createInfo,
    void*                         pPlacementAddr,
    WindowSystem**                ppWindowSystem)
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppWindowSystem != nullptr));

    auto*const pWindowSystem = PAL_PLACEMENT_NEW(pPlacementAddr) DisplayWindowSystem(device, createInfo);
    Result     result = pWindowSystem->Init();

    if (result == Result::Success)
    {
        *ppWindowSystem = pWindowSystem;
    }
    else
    {
        pWindowSystem->Destroy();
    }

    return result;
}

// =====================================================================================================================
DisplayWindowSystem::DisplayWindowSystem(
    const Device&                 device,
    const WindowSystemCreateInfo& createInfo)
    :
    WindowSystem(createInfo.platform),
    m_device(device),
    m_drmLoader(device.GetPlatform()->GetDrmLoader()),
    m_drmProcs(m_drmLoader.GetProcsTable()),
    m_crtcId(createInfo.crtcId),
    m_drmMasterFd(createInfo.drmMasterFd),
    m_connectorId(createInfo.connectorId),
    m_waitMutex()
{

}

// =====================================================================================================================
Result DisplayWindowSystem::Init()
{
    Result result = m_waitEventThread.Begin(&EventPolling, this);
    if (result == Result::Success)
    {
        result = m_waitMutex.Init();
    }
    if (result == Result::Success)
    {
        result = m_flipSemaphore.Init(1, 0);
    }
    if (result == Result::Success)
    {
        EventCreateFlags flags  = {};
        flags.manualReset       = true;
        flags.semaphore         = true;

        result = m_exitThreadEvent.Init(flags);
    }

    return result;
}

// =====================================================================================================================
// Helper functions to describe the properties of a window system we will create in the future.
Result DisplayWindowSystem::DeterminePresentationSupported(
    Device*             pDevice,
    OsDisplayHandle     hDisplay,
    int64               visualId)
{
    return Result::Success;
}

// =====================================================================================================================
Result DisplayWindowSystem::CreatePresentableImage(
    Image*              pImage,
    int32               sharedBufferFd)
{
    uint32 bufferHandle[4];

    int32 ret = m_drmProcs.pfnDrmPrimeFDToHandle(m_drmMasterFd, sharedBufferFd, &bufferHandle[0]);
    if (ret == 0)
    {
        const SubresId subres = { ImageAspect::Color, 0, 0 };
        const SubResourceInfo*const pSubResInfo = pImage->SubresourceInfo(subres);
        uint32 pitches[4]   = {};
        uint32 offset[4]    = {};
        const uint32 width  = pSubResInfo->extentTexels.width;
        const uint32 height = pSubResInfo->extentTexels.height;
        pitches[0]          = pSubResInfo->rowPitch;
        offset[0]           = pSubResInfo->offset;
        uint32 fbId         = 0;
        ret = m_drmProcs.pfnDrmModeAddFB2(m_drmMasterFd,
                                          width,
                                          height,
                                          DRM_FORMAT_XRGB8888,
                                          bufferHandle,
                                          pitches,
                                          offset,
                                          &fbId, 0);

        close(sharedBufferFd);

        if (ret == 0)
        {
            pImage->SetFramebufferId(fbId);
            pImage->SetPresentImageHandle(bufferHandle[0]);

            ModeSet(pImage);
        }
    }

    return (ret == 0) ? Result::Success : Result::ErrorInvalidValue;
}

// =====================================================================================================================
Result DisplayWindowSystem::ModeSet(
    Image* pImage)
{
    const SubresId subres = { ImageAspect::Color, 0, 0 };
    const SubResourceInfo*const pSubResInfo = pImage->SubresourceInfo(subres);
    const uint32 width  = pSubResInfo->extentTexels.width;
    const uint32 height = pSubResInfo->extentTexels.height;

    drmModeConnectorPtr drmConnector = m_drmProcs.pfnDrmModeGetConnectorCurrent(m_drmMasterFd, m_connectorId);
    drmModeModeInfoPtr  drmMode;
    drmMode = NULL;
    for (int m = 0; m < drmConnector->count_modes; m++)
    {
        drmMode = &drmConnector->modes[m];
        if ((drmMode->vdisplay == height) && (drmMode->hdisplay == width)) break;
        drmMode = NULL;
    }
    uint32 cid = (uint32)(m_connectorId);
    int ret = m_drmProcs.pfnDrmModeSetCrtc(m_drmMasterFd, m_crtcId, pImage->GetFrameBufferId(), 0, 0,
                                           &cid, 1, drmMode);

    return (ret == 0) ? Result::Success : Result::ErrorUnknown;
}

// =====================================================================================================================
void DisplayWindowSystem::DestroyPresentableImage(
    uint32              image)
{
    drm_mode_destroy_dumb dreq = {};
    dreq.handle = image;
    m_drmProcs.pfnDrmIoctl(m_drmMasterFd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
}

// =====================================================================================================================
Result DisplayWindowSystem::Present(
    const PresentSwapChainInfo& presentInfo,
    PresentFence*               pRenderFence,
    PresentFence*               pIdleFence)
{
    Image* pImage = static_cast<Image*>(presentInfo.pSrcImage);

    DisplayPresentFence* pFence = static_cast<DisplayPresentFence*>(pIdleFence);
    SwapChain* pSwapChain = static_cast<SwapChain*>(presentInfo.pSwapChain);

    m_waitMutex.Lock();
    pSwapChain->SetFlipImageIdx(presentInfo.imageIndex);
    m_waitMutex.Unlock();

    int32 ret = m_drmProcs.pfnDrmModePageFlip(m_drmMasterFd, m_crtcId, pImage->GetFrameBufferId(),
                                              DRM_MODE_PAGE_FLIP_EVENT, pSwapChain);

    Result result = Result::ErrorUnknown;
    if (ret == 0)
    {
        result = Result::Success;
    }
    else if (ret == -EINVAL)
    {
        // If DrmModePageFlip is called, the flip is not executed until VSync happened. And the DrmModePageFlip will
        // fail if the mode deson't match between FrameBuffer and CRTC, so the mode will be reset and image will be
        // flipped without waitting for VSync by called DrmModeSetCrtc. DrmModeSetCrtc does not generate a Flip event
        // into FD, so we need to post the semaphore here in case deadlock.
        // This exception probably cause tearing, but it will not break present or the other normal work of application.
        ModeSet(pImage);
        m_flipSemaphore.Post();
        result = Result::ErrorIncompatibleDisplayMode;
    }

    return result;
}

// =====================================================================================================================
Result DisplayWindowSystem::WaitForLastImagePresented()
{
    // Waiting for flipping. When the semaphore is signaled, it means the current presentable image is being scan-out
    // and the previous presentable images are idle.
    uint32 timeoutMsec = -1;
    m_flipSemaphore.Wait(timeoutMsec);
    return Result::Success;
}

// =====================================================================================================================
DisplayWindowSystem::~DisplayWindowSystem()
{
    if (m_waitEventThread.IsCreated())
    {
        m_exitThreadEvent.Set();
        m_waitEventThread.Join();
    }
}

// =====================================================================================================================
void DisplayWindowSystem::DisplayVblankCb(
    int32  fd,
    uint32 frame,
    uint32 sec,
    uint32 usec,
    void*  pUserData)
{
    /// When VSync is on, the Vblank and Flipping callback are happened at the same time.
}

// =====================================================================================================================
void DisplayWindowSystem::DisplayPageFlipCb(
    int32  fd,
    uint32 frame,
    uint32 sec,
    uint32 usec,
    void*  pUserData)
{
    DisplayPageFlip2Cb(fd, frame, sec, usec, 0, pUserData);
}

// =====================================================================================================================
void DisplayWindowSystem::DisplayPageFlip2Cb(
    int32  fd,
    uint32 frame,
    uint32 sec,
    uint32 usec,
    uint32 crtcId,
    void*  pUserData)
{
    SwapChain* pSwapChain = static_cast<SwapChain*>(pUserData);
    const uint32 curIdx = pSwapChain->GetFlipImageIdx();
    DisplayPresentFence* pFence = static_cast<DisplayPresentFence*> (pSwapChain->PresentIdleFence(curIdx));
    pFence->SetPresentState(PresentState::Flip);
    for (uint32 i = 0; i < pSwapChain->CreateInfo().imageCount; i++)
    {
        if (i != pSwapChain->GetFlipImageIdx())
        {
            pFence = static_cast<DisplayPresentFence*> (pSwapChain->PresentIdleFence(i));
            if ((pFence != nullptr) && (pFence->GetPresentState() == PresentState::Flip))
            {
                pFence->SetPresentState(PresentState::Idle);
                pFence->Trigger();
            }
        }
    }
}

// =====================================================================================================================
void DisplayWindowSystem::EventPolling(
    void* pData)
{
    DisplayWindowSystem* pWindowSystem = static_cast<DisplayWindowSystem*>(pData);
    const uint32 pollFdCount = 2;
    pollfd pfd[pollFdCount] = {};
    pfd[0].fd     = pWindowSystem->GetMasterFd();
    pfd[0].events = POLLIN;
    pfd[1].fd     = pWindowSystem->GetExitThreadEventFd();
    pfd[1].events = POLLIN;

    drmEventContext eventContext    = {};
    eventContext.version            = DRM_EVENT_CONTEXT_VERSION;
    eventContext.page_flip_handler  = DisplayPageFlipCb;
    eventContext.page_flip_handler2 = DisplayPageFlip2Cb;
    eventContext.vblank_handler     = DisplayVblankCb;

    while (true)
    {
        const int timeout = -1;
        if (poll(pfd, pollFdCount, timeout) > 0)
        {
            if ((pfd[0].revents & POLLIN) != 0)
            {
                pWindowSystem->m_waitMutex.Lock();
                pWindowSystem->m_drmProcs.pfnDrmHandleEvent(pWindowSystem->GetMasterFd(), &eventContext);
                pWindowSystem->m_waitMutex.Unlock();
                pWindowSystem->m_flipSemaphore.Post();
            }
            if ((pfd[1].revents & POLLIN) != 0)
            {
                break;
            }
        }
    }
}

}
}
