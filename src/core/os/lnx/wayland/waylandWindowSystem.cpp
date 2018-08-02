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
#include "core/os/lnx/wayland/waylandWindowSystem.h"
#include "core/os/lnx/lnxDevice.h"
#include "core/os/lnx/lnxImage.h"
#include "core/os/lnx/lnxPlatform.h"
#include "palSwapChain.h"
#include "util/lnx/lnxTimeout.h"
#include "dlfcn.h"

using namespace Util;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 392
namespace Pal
{
namespace Linux
{

// =====================================================================================================================
// translate ChNumFormat into WsaFormat
static WsaFormat WlDrmFormat(
    ChNumFormat  format,
    bool         alpha)
{
    WsaFormat wsaFormat = WsaFormatXRGB8888;
    switch (format)
    {
    case ChNumFormat::X8Y8Z8W8_Unorm:
    case ChNumFormat::X8Y8Z8W8_Srgb:
        wsaFormat = alpha ? WsaFormatARGB8888 : WsaFormatXRGB8888;
        break;
    default:
        PAL_ASSERT(!"Not supported format!");
        break;
    }
    return wsaFormat;
}

// =====================================================================================================================
Result WaylandPresentFence::Create(
    const WaylandWindowSystem& windowSystem,
    bool                       initiallySignaled,
    void*                      pPlacementAddr,
    PresentFence**             ppPresentFence)
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppPresentFence != nullptr));

    auto*const pPresentFence = PAL_PLACEMENT_NEW(pPlacementAddr) WaylandPresentFence(windowSystem);
    Result     result        = pPresentFence->Init(initiallySignaled);

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
WaylandPresentFence::WaylandPresentFence(
    const WaylandWindowSystem& windowSystem)
    :
    m_windowSystem(windowSystem),
    m_hImage(WaylandWindowSystem::DefaultImageHandle)
{
}

// =====================================================================================================================
WaylandPresentFence::~WaylandPresentFence()
{
}

// =====================================================================================================================
Result WaylandPresentFence::Init(
    bool initiallySignaled)
{
    return Result::Success;
}

// =====================================================================================================================
void WaylandPresentFence::Reset()
{
}

// =====================================================================================================================
Result WaylandPresentFence::Trigger()
{
    return Result::Success;
}

// =====================================================================================================================
// timeout is not needed for now.
Result WaylandPresentFence::WaitForCompletion(
    bool doWait)
{
    Result result = Result::NotReady;

    if (m_hImage == WaylandWindowSystem::DefaultImageHandle)
    {
        result = Result::Success;
    }

    if (result != Result::Success)
    {
        // quick check
        if (WaylandWindowSystem::s_pWsaInterface->pfnImageAvailable(m_windowSystem.m_hWsa, m_hImage) == Success)
        {
            result = Result::Success;
        }
    }

    if ((result != Result::Success) && doWait)
    {
        WaylandWindowSystem::s_pWsaInterface->pfnWaitForLastImagePresented(m_windowSystem.m_hWsa);
        result = Result::Success;
    }

    return result;
}

WsaInterface* WaylandWindowSystem::s_pWsaInterface = nullptr;

// =====================================================================================================================
// load wayland wsa interface
Result WaylandWindowSystem::LoadWaylandWsa()
{
    Result result = Result::Success;

    if (s_pWsaInterface == nullptr)
    {
        void* pHandle = dlopen("libamdgpu_wsa_wayland.so", RTLD_LAZY);

        if (pHandle == nullptr)
        {
            result = Result::ErrorInitializationFailed;
        }

        if (result == Result::Success)
        {
            s_pWsaInterface = (WsaInterface*)dlsym(pHandle, "WaylandWsaInterface");
        }

        if (s_pWsaInterface == nullptr)
        {
            result = Result::ErrorInitializationFailed;
        }
    }

    return result;
}

// =====================================================================================================================
Result WaylandWindowSystem::Create(
    const Device&                 device,
    const WindowSystemCreateInfo& createInfo,
    void*                         pPlacementAddr,
    WindowSystem**                ppWindowSystem)
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppWindowSystem != nullptr));

    auto*const pWindowSystem = PAL_PLACEMENT_NEW(pPlacementAddr) WaylandWindowSystem(device, createInfo);
    Result     result        = pWindowSystem->Init();

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
WaylandWindowSystem::WaylandWindowSystem(
    const Device&                 device,
    const WindowSystemCreateInfo& createInfo)
    :
    WindowSystem(createInfo.platform),
    m_device(device),
    m_pDisplay(static_cast<void*>(createInfo.hDisplay)),
    m_pSurface(createInfo.hWindow.pSurface)
{
}

// =====================================================================================================================
WaylandWindowSystem::~WaylandWindowSystem()
{
}

// =====================================================================================================================
Result WaylandWindowSystem::Init()
{
    Result result = Result::Success;

    if (s_pWsaInterface == nullptr)
    {
        result = LoadWaylandWsa();
    }

    if (result == Result::Success)
    {
        uint32 wsaVersion = s_pWsaInterface->pfnQueryVersion();
        if (wsaVersion < WSA_INTERFACE_VER)
        {
            result = Result::ErrorInitializationFailed;
        }
    }

    if (result == Result::Success)
    {
        WsaError ret = s_pWsaInterface->pfnCreateWsa(&m_hWsa);

        if (ret != Success)
        {
            result = Result::ErrorInitializationFailed;
        }
    }

    if (result == Result::Success)
    {
        WsaError ret = s_pWsaInterface->pfnInitialize(m_hWsa, m_pDisplay, m_pSurface);

        if (ret != Success)
        {
            result = Result::ErrorInitializationFailed;
        }
    }

    return result;
}

// =====================================================================================================================
// Interface for the window system to do things related with creating presentable image.
Result WaylandWindowSystem::CreatePresentableImage(
    Image*         pImage,
    int32          sharedBufferFd)
{
    PAL_ASSERT(s_pWsaInterface != nullptr);

    Result result = Result::Success;
    const SubresId              subres      = { ImageAspect::Color, 0, 0 };
    const SubResourceInfo*const pSubResInfo = pImage->SubresourceInfo(subres);

    const uint32 width  = pSubResInfo->extentTexels.width;
    const uint32 height = pSubResInfo->extentTexels.height;
    const uint32 stride = pSubResInfo->rowPitch;
    const uint32 bpp    = pSubResInfo->bitsPerTexel;
    uint32 presentImage = 0;

    //we should get alpha from create info, now just hard code it temporarily.
    WsaFormat format = WlDrmFormat(pSubResInfo->format.format, false);

    if ((width == 0) || (height == 0) || (stride == 0) || (bpp == 0) || (sharedBufferFd == InvalidFd))
    {
        result = Result::ErrorInvalidValue;
    }

    if (result == Result::Success)
    {
        WsaError ret = s_pWsaInterface->pfnCreateImage(
                               m_hWsa, sharedBufferFd, width, height, format,
                               stride, reinterpret_cast<int32*>(&presentImage));
        pImage->SetPresentImageHandle(presentImage);

        if (ret != Success)
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
// Destroy the present image.
void WaylandWindowSystem::DestroyPresentableImage(
    uint32 image)
{
    PAL_ASSERT(s_pWsaInterface != nullptr);

    s_pWsaInterface->pfnDestroyImage(image);
}

// =====================================================================================================================
// Present a pixmap on wayland.
Result WaylandWindowSystem::Present(
    const PresentSwapChainInfo& presentInfo,
    PresentFence*               pRenderFence,
    PresentFence*               pIdleFence)
{
    PAL_ASSERT(s_pWsaInterface != nullptr);
    const Image&       srcImage = static_cast<Image&>(*presentInfo.pSrcImage);
    uint32             pixmap   = srcImage.GetPresentImageHandle();

    WaylandPresentFence*const pWaylandIdleFence = static_cast<WaylandPresentFence*>(pIdleFence);

    pWaylandIdleFence->SetImage(pixmap);
    s_pWsaInterface->pfnPresent(m_hWsa, pixmap, nullptr);

    return Result::Success;
}

// =====================================================================================================================
// Wait until the image is available.
Result WaylandWindowSystem::WaitForLastImagePresented()
{
    PAL_ASSERT(s_pWsaInterface != nullptr);

    Result result = Result::Success;
    s_pWsaInterface->pfnWaitForLastImagePresented(m_hWsa);

    return result;
}

// =====================================================================================================================
// Get window information.
Result WaylandWindowSystem::GetWindowGeometry(
    Device*             pDevice,
    OsDisplayHandle     hDisplay,
    OsWindowHandle      hWindow,
    Extent2d*           pExtents)
{
    Result result = Result::Success;

    if (s_pWsaInterface == nullptr)
    {
        result = LoadWaylandWsa();
    }

    if (result == Result::Success)
    {
        s_pWsaInterface->pfnGetWindowGeometry(hDisplay, hWindow.pSurface, &pExtents->width, &pExtents->height);
    }

    return result;
}

// =====================================================================================================================
// Check whether it's presentable.
Result WaylandWindowSystem::DeterminePresentationSupported(
    Device*             pDevice,
    OsDisplayHandle     hDisplay,
    int64               visualId)
{
    return Result::Success;
}

} // Linux
} // Pal
#endif
