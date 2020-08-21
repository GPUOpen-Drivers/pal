/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/os/amdgpu/display/displayWindowSystem.h"
#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuImage.h"
#include "core/os/amdgpu/amdgpuPlatform.h"
#include "core/os/amdgpu/amdgpuScreen.h"
#include "core/os/amdgpu/amdgpuSwapChain.h"
#include "palScreen.h"
#include "palSwapChain.h"
#include "palSysUtil.h"

#include <poll.h>

using namespace Util;
namespace Pal
{
namespace Amdgpu
{

// ====================================================================================================================a
// Convert PAL format to Wayland Drm format
static uint32 PalToDrmFormat(
    SwizzledFormat format)
{
    uint32 drmFormat = DRM_FORMAT_XRGB8888;

    switch (format.format)
    {
        case ChNumFormat::X8Y8Z8W8_Unorm:
        case ChNumFormat::X8Y8Z8W8_Srgb:
            drmFormat = DRM_FORMAT_XRGB8888;
            break;

        case ChNumFormat::X10Y10Z10W2_Unorm:
            if ((format.swizzle.r == ChannelSwizzle::Z) &&
                (format.swizzle.g == ChannelSwizzle::Y) &&
                (format.swizzle.b == ChannelSwizzle::X) &&
                (format.swizzle.a == ChannelSwizzle::W))
            {
                drmFormat = DRM_FORMAT_XRGB2101010;
            }
            else
            {
                drmFormat = DRM_FORMAT_XBGR2101010;
            }

            break;

        default:
            PAL_ASSERT(!"Not supported format!");
            break;
    }

    return drmFormat;
}
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
    return m_imageIdle.Init(1, initiallySignaled ? 1 : 0);
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
    uint32 timeoutMsec = doWait ? -1 : 0;

    Result result = m_imageIdle.Wait(timeoutMsec);

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
    m_crtcId(0),
    m_drmMasterFd(createInfo.drmMasterFd),
    m_connectorId(createInfo.connectorId)
{

}

// =====================================================================================================================
Result DisplayWindowSystem::Init()
{
    EventCreateFlags flags  = {};

    flags.manualReset = true;
    flags.semaphore   = true;

    // m_exitThreadEvent must be inited before m_waitEventThread because the exitThread fd is added in EventPolling
    Result result = m_exitThreadEvent.Init(flags);

    if (result == Result::Success)
    {
        result = m_flipSemaphore.Init(1, 0);
    }

    if ((result == Result::Success) && (m_drmMasterFd == InvalidFd))
    {
        m_drmMasterFd = m_device.GetPrimaryFileDescriptor();
    }

    return result;
}

// =====================================================================================================================
// Get the window properties.
Result DisplayWindowSystem::GetWindowProperties(
    Device*              pDevice,
    OsDisplayHandle      hDisplay,
    OsWindowHandle       hWindow,
    SwapChainProperties* pSwapChainProperties)
{

    // DirectDisplay can support one presentable image for rendering on front buffer.
    pSwapChainProperties->minImageCount = 1;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 610
    pSwapChainProperties->compositeAlphaMode = static_cast<uint32>(CompositeAlphaMode::Opaque);
#endif

    return Result::Success;
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
    SwapChain* pSwapChain,
    Image*     pImage,
    int32      sharedBufferFd)
{
    uint32 bufferHandle[4];

    int32 ret = m_drmProcs.pfnDrmPrimeFDToHandle(m_drmMasterFd, sharedBufferFd, &bufferHandle[0]);
    if (ret == 0)
    {
        const SubresId subres = { ImageAspect::Color, 0, 0 };

        const SubResourceInfo*const pSubResInfo = pImage->SubresourceInfo(subres);

        uint32 pitches[4]   = {};
        uint32 offset[4]    = {};
        uint32 fbId         = 0;
        uint32 drmFormat    = PalToDrmFormat(pSubResInfo->format);
        const uint32 width  = pSubResInfo->extentTexels.width;
        const uint32 height = pSubResInfo->extentTexels.height;

        pitches[0]          = pSubResInfo->rowPitch;
        offset[0]           = pSubResInfo->offset;

        ret = m_drmProcs.pfnDrmModeAddFB2(m_drmMasterFd,
                                          width,
                                          height,
                                          drmFormat,
                                          bufferHandle,
                                          pitches,
                                          offset,
                                          &fbId, 0);

        close(sharedBufferFd);

        if (ret == 0)
        {
            WindowSystemImageHandle imageHandle = { .hBuffer = bufferHandle[0] };

            pImage->SetFramebufferId(fbId);
            pImage->SetPresentImageHandle(imageHandle);

            FindCrtc();
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
    WindowSystemImageHandle hImage)
{
    drm_gem_close dreq = {};

    dreq.handle = hImage.hBuffer;

    m_drmProcs.pfnDrmIoctl(m_drmMasterFd, DRM_IOCTL_GEM_CLOSE, &dreq);
}

// =====================================================================================================================
Result DisplayWindowSystem::Present(
    const PresentSwapChainInfo& presentInfo,
    PresentFence*               pRenderFence,
    PresentFence*               pIdleFence)
{
    Image* pImage = static_cast<Image*>(presentInfo.pSrcImage);

    DisplayPresentFence* pFence        = static_cast<DisplayPresentFence*>(pIdleFence);
    SwapChain*const      pSwapChain    = static_cast<SwapChain*>(presentInfo.pSwapChain);
    const SwapChainMode  swapChainMode = pSwapChain->CreateInfo().swapChainMode;
    uint32               flipFlag      = (swapChainMode == SwapChainMode::Immediate) ? DRM_MODE_PAGE_FLIP_ASYNC : 0;

    Result result = Result::ErrorUnknown;

    // For display window system, two or more swapchain is not supported because the DRM event will be consumed by
    // other swapchains. But there are some applications will create more than one swapchain although only one of
    // them is used to present. In order to handle this case, create the waitEventThread here instead of
    // DisplayWindowSystem::Init().
    if (m_waitEventThread.IsCreated() == false)
    {
        m_waitEventThread.Begin(&EventPolling, this);
    }

    if (pImage->GetImageIndex() == InvalidImageIndex)
    {
        pImage->SetImageIndex(presentInfo.imageIndex);
    }
    PAL_ASSERT(pImage->GetImageIndex() == presentInfo.imageIndex);

    if (pImage->GetSwapChain() == nullptr)
    {
        pImage->SetSwapChain(pSwapChain);
    }
    PAL_ASSERT(pImage->GetSwapChain() == pSwapChain);

    while (result != Result::Success)
    {
        int32 ret = m_drmProcs.pfnDrmModePageFlip(m_drmMasterFd,
                                                  m_crtcId,
                                                  pImage->GetFrameBufferId(),
                                                  flipFlag | DRM_MODE_PAGE_FLIP_EVENT,
                                                  pImage);

        if (ret == 0)
        {
            result = Result::Success;
            break;
        }
        else if (ret == -EINVAL)
        {
            // If DrmModePageFlip is called, the flip is not executed until VSync happened. And the DrmModePageFlip will
            // fail if the mode deson't match between FrameBuffer and CRTC, so the mode will be reset and image will be
            // flipped without waitting for VSync by called DrmModeSetCrtc. DrmModeSetCrtc does not generate a Flip
            // event into FD, so we need to post the semaphore here in case deadlock.
            // This exception probably cause tearing, but it will not break present or the other normal work of
            // application.
            ModeSet(pImage);
            m_flipSemaphore.Post();

            result = Result::ErrorIncompatibleDisplayMode;
            break;
        }
        else if (ret == -EBUSY)
        {
            // Discard this frame if it's mailbox mode
            if (swapChainMode == SwapChainMode::Mailbox)
            {
                pFence->SetPresentState(PresentState::Idle);
                pFence->Trigger();
                break;
            }
            // For aync mode, it's possible that the old pageflip request is not handled by the KMD yet. It gets EBUSY
            // for this case. sleep for a while and try again.
            usleep(1);
        }
        else
        {
            break;
        }
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
    Image*               pImage     = static_cast<Image*>(pUserData);
    SwapChain*           pSwapChain = pImage->GetSwapChain();
    const uint32         imageIndex = pImage->GetImageIndex();
    DisplayPresentFence* pFence     = static_cast<DisplayPresentFence*>(pSwapChain->PresentIdleFence(imageIndex));

    // Now the image is being scanned out.
    pFence->SetPresentState(PresentState::Flip);

    // Idle the previous flipped image
    for (uint32 i = 0; i < pSwapChain->CreateInfo().imageCount; i++)
    {
        if (i != imageIndex)
        {
            pFence = static_cast<DisplayPresentFence*>(pSwapChain->PresentIdleFence(i));

            if ((pFence != nullptr) &&
                (pFence->GetPresentState() == PresentState::Flip))
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
                pWindowSystem->m_drmProcs.pfnDrmHandleEvent(pWindowSystem->GetMasterFd(), &eventContext);
                pWindowSystem->m_flipSemaphore.Post();
            }
            if ((pfd[1].revents & POLLIN) != 0)
            {
                break;
            }
        }
    }
}

// =====================================================================================================================
// Help function to find an idle Crtc to driver the display
Result DisplayWindowSystem::FindCrtc()
{
    Result              result         = Result::Success;
    drmModeRes*         pModeResources = nullptr;
    drmModeConnectorPtr pConnector     = nullptr;

    if (m_crtcId == 0)
    {
        pModeResources = m_drmProcs.pfnDrmModeGetResources(m_drmMasterFd);

        if (pModeResources == nullptr)
        {
            result = Result::ErrorUnknown;
        }
    }

    if ((m_crtcId == 0) && (result == Result::Success))
    {
        pConnector = m_drmProcs.pfnDrmModeGetConnectorCurrent(m_drmMasterFd, m_connectorId);

        if (pConnector == nullptr)
        {
            result = Result::ErrorUnknown;
        }
    }

    if ((m_crtcId == 0) && (result == Result::Success))
    {
        // Prefer the current Crtc which is driving the connector and not shared by others.
        if (pConnector->encoder_id)
        {
            drmModeEncoderPtr pEncoder = m_drmProcs.pfnDrmModeGetEncoder(m_drmMasterFd, pConnector->encoder_id);

            if (pEncoder)
            {
                uint32_t crtcId = pEncoder->crtc_id;

                m_drmProcs.pfnDrmModeFreeEncoder(pEncoder);

                if (crtcId)
                {
                    bool encoderIsShared = false;
                    bool crtcIsShared    = false;

                    // Check if the encoder is shared by other connectors
                    for (int i = 0; i < pModeResources->count_connectors; i++)
                    {
                        uint32 connectorId = pModeResources->connectors[i];

                        if (connectorId == m_connectorId)
                        {
                            continue;
                        }

                        drmModeConnectorPtr pModeConnector = m_drmProcs.pfnDrmModeGetConnector(m_drmMasterFd,
                                                                                               connectorId);

                        if (pModeConnector)
                        {
                            encoderIsShared = (pModeConnector->encoder_id == pConnector->encoder_id);
                            m_drmProcs.pfnDrmModeFreeConnector(pModeConnector);

                            if (encoderIsShared)
                            {
                                break;
                            }
                        }
                    }

                    // Check if the crtc is driving other connectors
                    if (!encoderIsShared)
                    {
                        for (int i = 0; i < pModeResources->count_encoders; i++)
                        {
                            uint32 encoderId = pModeResources->encoders[i];

                            if (encoderId == pConnector->encoder_id)
                            {
                                continue;
                            }

                            drmModeEncoderPtr pModeEncoder = m_drmProcs.pfnDrmModeGetEncoder(m_drmMasterFd, encoderId);

                            if (pModeEncoder)
                            {
                                crtcIsShared = (pModeEncoder->crtc_id == crtcId);

                                m_drmProcs.pfnDrmModeFreeEncoder(pModeEncoder);

                                if (crtcIsShared)
                                {
                                    break;
                                }
                            }
                        }
                    }

                    if (!crtcIsShared && !encoderIsShared)
                    {
                        m_crtcId = crtcId;
                    }
                }
            }
        }
    }

    if ((m_crtcId == 0) && (result == Result::Success))
    {
        for (int i = 0; (m_crtcId == 0) && (i < pModeResources->count_crtcs); i++)
        {
            drmModeCrtcPtr pModeCrtc = m_drmProcs.pfnDrmModeGetCrtc(m_drmMasterFd, pModeResources->crtcs[i]);

            if (pModeCrtc)
            {
                if (pModeCrtc->buffer_id == 0)
                {
                    m_crtcId = pModeCrtc->crtc_id;
                }
                m_drmProcs.pfnDrmModeFreeCrtc(pModeCrtc);
            }
        }
    }

    if (pModeResources != nullptr)
    {
        m_drmProcs.pfnDrmModeFreeResources(pModeResources);
    }

    if (pConnector != nullptr)
    {
        m_drmProcs.pfnDrmModeFreeConnector(pConnector);
    }

    if (m_crtcId == 0)
    {
        result = Result::ErrorUnknown;
    }

    return result;
}

}
}
