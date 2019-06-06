/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/os/amdgpu/amdgpuImage.h"
#include "core/os/amdgpu/amdgpuPlatform.h"
#include "core/os/amdgpu/amdgpuSwapChain.h"
#include "core/os/amdgpu/wayland/waylandWindowSystem.h"
#include "palFormatInfo.h"

using namespace Util;

// Because the buffer sharing depends on wl_drm interface, which relies on wl_buffer_interface. However,
// wl_buffer_interface can't be located because libwayland-client.so is not linked directly. To solve this issue,
// wl_buffer_interface is replaced by pWlBufferInterface using a macro, and pWlBufferInterface is set to the dlsym-ed
// wl_buffer_interface symbol in WaylandWindowSystem::Init().
wl_interface* pWlBufferInterface = nullptr;

namespace Pal
{
namespace Amdgpu
{

// Define a function type for listeners to be added to proxy
typedef void (*Listener)(void);

// ====================================================================================================================a
// Convert PAL format to Wayland Drm format
static wl_drm_format PalToWlDrmFormat(
    ChNumFormat format,
    bool        alpha)
{
    wl_drm_format waylandDrmFormat = WL_DRM_FORMAT_XRGB8888;

    switch (format)
    {
        case ChNumFormat::X8Y8Z8W8_Unorm:
        case ChNumFormat::X8Y8Z8W8_Srgb:
            waylandDrmFormat = alpha ? WL_DRM_FORMAT_ARGB8888 : WL_DRM_FORMAT_XRGB8888;
            break;

        default:
            PAL_ASSERT(!"Not supported format!");
            break;
    }

    return waylandDrmFormat;
}

// =====================================================================================================================
// Get the notification of the path of the drm device which is used by the server. For multi-GPU, Pal should use this
// device for creating local buffers.
static void DrmHandleDevice(
    void*       pData,
    wl_drm*     pDrm,
    const char* pName)
{

}

// =====================================================================================================================
// Get the formats that Wayland compositor supports
static void DrmHandleFormat(
    void*   pData,
    wl_drm* pDrm,
    uint32  wl_format)
{

}

// =====================================================================================================================
// Receive if the magic is autenticated by Wayland server, meaningful for EGL and useless for PAL
static void DrmHandleAuthenticated(
    void*   pData,
    wl_drm* pDrm)
{

}

// =====================================================================================================================
// Bitmask of capabilities the wl_drm supports, WL_DRM_CAPABILITY_PRIME is a must, otherwise can't create prime buffer.
static void DrmHandleCapabilities(
    void*   pData,
    wl_drm* pDrm,
    uint32  capabilities)
{
    WaylandWindowSystem* pWaylandWindowSystem = reinterpret_cast<WaylandWindowSystem*>(pData);

    pWaylandWindowSystem->SetCapabilities(capabilities);
}

// The listener for wl_drm to get the drm device, buffer format and capabilities.
static const wl_drm_listener WaylandDrmListener =
{
    DrmHandleDevice,
    DrmHandleFormat,
    DrmHandleAuthenticated,
    DrmHandleCapabilities,
};

// =====================================================================================================================
// This function is called while there is any global interface is registered to server. Check if wl_drm is registered.
static void RegistryHandleGlobal(
    void*        pData,
    wl_registry* pRegistry,
    uint32       name,
    const char*  pInterface,
    uint32       version)
{
    WaylandWindowSystem*       pWaylandWindowSystem = reinterpret_cast<WaylandWindowSystem*>(pData);
    const WaylandLoaderFuncs&  waylandProcs         = pWaylandWindowSystem->GetWaylandProcs();

    if (strcmp(pInterface, "wl_drm") == 0)
    {
        PAL_ASSERT(version >= 2);

        wl_drm* pWaylandDrm = reinterpret_cast<struct wl_drm*>(waylandProcs.pfnWlProxyMarshalConstructorVersioned(
                                                               reinterpret_cast<wl_proxy*>(pRegistry),
                                                               WL_REGISTRY_BIND,
                                                               &wl_drm_interface,
                                                               version,
                                                               name,
                                                               wl_drm_interface.name,
                                                               version,
                                                               nullptr));

        if (pWaylandDrm != nullptr)
        {
            waylandProcs.pfnWlProxyAddListener(reinterpret_cast<wl_proxy*>(pWaylandDrm),
                                               reinterpret_cast<Listener*>(const_cast<wl_drm_listener*>
                                                   (&WaylandDrmListener)),
                                               pWaylandWindowSystem);

            pWaylandWindowSystem->SetWaylandDrm(pWaylandDrm);
        }
    }
}

// =====================================================================================================================
// This function is called while there is any global interface is unregistered from server.
static void RegistryHandleGlobalRemove(
    void*        pData,
    wl_registry* pRegistry,
    uint32       name)
{

}

// A listener to handle global interfaces registered to server
static const wl_registry_listener RegistryListener =
{
    RegistryHandleGlobal,
    RegistryHandleGlobalRemove,
};

// =====================================================================================================================
// This function is triggred once the event which indicates buffer is released from Wayland server is dispatched
static void BufferHandleRelease(
    void*      pData,
    wl_buffer* pBuffer)
{
    Image* pImage = reinterpret_cast<Image*>(pData);

    pImage->SetIdle(true);
}

// A listener for event that buffer is released from Wayland server.
static const wl_buffer_listener BufferListener =
{
    BufferHandleRelease,
};

// =====================================================================================================================
// It indicates that the previous frame is already shown on screen and it's good time to draw next frame
static void FrameHandleDone(
    void*        pData,
    wl_callback* pCallback,
    uint32       callbackData)
{
    WaylandWindowSystem*       pWaylandWindowSystem = reinterpret_cast<WaylandWindowSystem*>(pData);
    const WaylandLoaderFuncs&  waylandProcs         = pWaylandWindowSystem->GetWaylandProcs();

    pWaylandWindowSystem->SetFrameCallback(nullptr);
    pWaylandWindowSystem->SetFrameCompleted();

    waylandProcs.pfnWlProxyDestroy(reinterpret_cast<wl_proxy*>(pCallback));
}

// Handle the notification when it is a good time to start drawing a new frame
static const wl_callback_listener FrameListener =
{
    FrameHandleDone,
};

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
    m_pImage(nullptr)
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
// Wait for the image released by Wayland server while acquiring next image from main thread.
Result WaylandPresentFence::WaitForCompletion(
    bool doWait)
{
    Result result   = Result::NotReady;
    bool   signaled = (m_pImage != nullptr) ? m_pImage->GetIdle() : false;

    if ((m_pImage != nullptr) && !signaled)
    {
        // The idle flag will not be set without pfnWlDisplayDispatchQueue, using "do {} while" here so that give a
        // chance to set the idle flag for doWait=false case.
        do
        {
            const WaylandLoaderFuncs& waylandProcs  = m_windowSystem.GetWaylandProcs();

            // Dispatch the event in pending status so that quick check if the present fence is signaled.
            waylandProcs.pfnWlDisplayDispatchQueuePending(m_windowSystem.GetDisplay(), m_windowSystem.GetEventQueue());

            signaled = m_pImage->GetIdle();

            if (!signaled)
            {
                // Block until all of the requests are processed by the server.
                waylandProcs.pfnWlDisplayRoundtripQueue(m_windowSystem.GetDisplay(), m_windowSystem.GetEventQueue());
                signaled = m_pImage->GetIdle();
            }

        } while (doWait && !signaled);
    }

    if (m_pImage == nullptr)
    {
        result = Result::ErrorFenceNeverSubmitted;
    }
    else if (signaled)
    {
        result = Result::Success;
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
    m_pDisplay(static_cast<wl_display*>(createInfo.hDisplay)),
    m_pSurface(static_cast<wl_surface*>(createInfo.hWindow.pSurface)),
    m_pWaylandDrm(nullptr),
    m_waylandLoader(device.GetPlatform()->GetWaylandLoader()),
#if defined(PAL_DEBUG_PRINTS)
    m_waylandProcs(m_waylandLoader.GetProcsTableProxy())
#else
    m_waylandProcs(m_waylandLoader.GetProcsTable()),
#endif
    m_pEventQueue(nullptr),
    m_pSurfaceEventQueue(nullptr),
    m_pDisplayWrapper(nullptr),
    m_pSurfaceWrapper(nullptr),
    m_pWaylandDrmWrapper(nullptr),
    m_pFrameCallback(nullptr),
    m_frameCompleted(false),
    m_capabilities(0)
{

}

// =====================================================================================================================
WaylandWindowSystem::~WaylandWindowSystem()
{
    // The wrapper object must be destroyed before the object it was created from.
    if (m_pWaylandDrmWrapper != nullptr)
    {
        m_waylandProcs.pfnWlProxyWrapperDestroy(reinterpret_cast<void*>(m_pWaylandDrmWrapper));
    }

    if (m_pWaylandDrm != nullptr)
    {
        m_waylandProcs.pfnWlProxyDestroy(reinterpret_cast<wl_proxy*>(m_pWaylandDrm));
    }

    if (m_pFrameCallback != nullptr)
    {
        m_waylandProcs.pfnWlProxyDestroy(reinterpret_cast<wl_proxy*>(m_pFrameCallback));
    }

    if (m_pSurfaceWrapper != nullptr)
    {
        m_waylandProcs.pfnWlProxyWrapperDestroy(reinterpret_cast<void*>(m_pSurfaceWrapper));
    }

    if (m_pDisplayWrapper != nullptr)
    {
        m_waylandProcs.pfnWlProxyWrapperDestroy(reinterpret_cast<void*>(m_pDisplayWrapper));
    }

    if (m_pSurfaceEventQueue != nullptr)
    {
        m_waylandProcs.pfnWlEventQueueDestroy(m_pSurfaceEventQueue);
    }

    if (m_pEventQueue != nullptr)
    {
        m_waylandProcs.pfnWlEventQueueDestroy(m_pEventQueue);
    }
}

// =====================================================================================================================
Result WaylandWindowSystem::Init()
{
    Result       result    = Result::Success;
    wl_registry* pRegistry = nullptr;

    // pWlBufferInterface must be set before calling any wl_drm interfaces.
    pWlBufferInterface = m_waylandLoader.GetWlBufferInterface();

    m_pEventQueue = m_waylandProcs.pfnWlDisplayCreateQueue(m_pDisplay);

    if (m_pEventQueue == nullptr)
    {
        result = Result::ErrorInitializationFailed;
    }

    if (result == Result::Success)
    {
        m_pSurfaceEventQueue = m_waylandProcs.pfnWlDisplayCreateQueue(m_pDisplay);

        if (m_pSurfaceEventQueue == nullptr)
        {
            result = Result::ErrorInitializationFailed;
        }
    }

    if (result == Result::Success)
    {
        m_pDisplayWrapper = reinterpret_cast<wl_display*>(m_waylandProcs.pfnWlProxyCreateWrapper(m_pDisplay));

        if (m_pDisplayWrapper == nullptr)
        {
            result = Result::ErrorInitializationFailed;
        }
    }

    if (result == Result::Success)
    {
        m_waylandProcs.pfnWlProxySetQueue(reinterpret_cast<wl_proxy*>(m_pDisplayWrapper), m_pEventQueue);

        pRegistry = reinterpret_cast<wl_registry*>(m_waylandProcs.pfnWlProxyMarshalConstructor(
                                                       reinterpret_cast<wl_proxy*>(m_pDisplayWrapper),
                                                       WL_DISPLAY_GET_REGISTRY,
                                                       m_waylandLoader.GetWlRegistryInterface(),
                                                       NULL));
        if (pRegistry == nullptr)
        {
            result = Result::ErrorUnavailable;
        }
    }

    if (result == Result::Success)
    {
        m_waylandProcs.pfnWlProxyAddListener(reinterpret_cast<wl_proxy*>(pRegistry),
                                             reinterpret_cast<Listener*>(const_cast<wl_registry_listener*>
                                                 (&RegistryListener)),
                                             this);

        m_waylandProcs.pfnWlDisplayRoundtripQueue(m_pDisplay, m_pEventQueue);

        if (m_pWaylandDrm == nullptr)
        {
            result = Result::ErrorUnavailable;
        }
    }

    if (result == Result::Success)
    {
        m_waylandProcs.pfnWlDisplayRoundtripQueue(m_pDisplay, m_pEventQueue);

        if (!(m_capabilities & WL_DRM_CAPABILITY_PRIME))
        {
            result = Result::ErrorUnavailable;
        }
    }

    if (result == Result::Success)
    {
        m_pWaylandDrmWrapper = reinterpret_cast<wl_drm*>(m_waylandProcs.pfnWlProxyCreateWrapper(m_pWaylandDrm));

        if (m_pWaylandDrmWrapper == nullptr)
        {
            result = Result::ErrorInitializationFailed;
        }
    }

    if (result == Result::Success)
    {
        m_pSurfaceWrapper = reinterpret_cast<wl_surface*>(m_waylandProcs.pfnWlProxyCreateWrapper(m_pSurface));

        if (m_pSurfaceWrapper == nullptr)
        {
            result = Result::ErrorInitializationFailed;
        }
    }

    if (result == Result::Success)
    {
        m_waylandProcs.pfnWlProxySetQueue(reinterpret_cast<wl_proxy*>(m_pSurfaceWrapper), m_pSurfaceEventQueue);
        m_waylandProcs.pfnWlProxyDestroy(reinterpret_cast<wl_proxy*>(pRegistry));
    }

    return result;
}

// =====================================================================================================================
// Create an image could be presented to wayland.
Result WaylandWindowSystem::CreatePresentableImage(
    SwapChain* pSwapChain,
    Image*     pImage,
    int32      sharedBufferFd)
{
    Result                      result      = Result::Success;
    const SubresId              subres      = { ImageAspect::Color, 0, 0 };
    const SubResourceInfo*const pSubResInfo = pImage->SubresourceInfo(subres);
    wl_buffer*                  pBuffer     = nullptr;

    const uint32 width  = pSubResInfo->extentTexels.width;
    const uint32 height = pSubResInfo->extentTexels.height;
    const uint32 stride = pSubResInfo->rowPitch;
    const uint32 bpp    = pSubResInfo->bitsPerTexel;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 445
    const bool   alpha  = pSwapChain->CreateInfo().compositeAlpha == CompositeAlphaMode::PostMultiplied;
#else
    const bool   alpha  = false;
#endif

    wl_drm_format format = PalToWlDrmFormat(pSubResInfo->format.format, alpha);

    if ((width == 0) || (height == 0) || (stride == 0) || (bpp == 0) || (sharedBufferFd == InvalidFd))
    {
        result = Result::ErrorInvalidValue;
    }

    if (result == Result::Success)
    {
        wl_interface* pBufferInterface = m_waylandLoader.GetWlBufferInterface();

        pBuffer = reinterpret_cast<wl_buffer*>(m_waylandProcs.pfnWlProxyMarshalConstructor(
                                                   reinterpret_cast<wl_proxy*>(m_pWaylandDrmWrapper),
                                                   WL_DRM_CREATE_PRIME_BUFFER,
                                                   pBufferInterface,
                                                   nullptr,
                                                   sharedBufferFd,
                                                   width,
                                                   height,
                                                   format,
                                                   0,
                                                   stride,
                                                   0,
                                                   0,
                                                   0,
                                                   0));

        close(sharedBufferFd);

        if (pBuffer == nullptr)
        {
            result = Result::ErrorUnknown;
        }
        else
        {
            m_waylandProcs.pfnWlProxySetQueue(reinterpret_cast<wl_proxy*>(pBuffer), m_pEventQueue);
        }
    }

    if (result == Result::Success)
    {
        WindowSystemImageHandle imageHandle = { .pBuffer = pBuffer };

        m_waylandProcs.pfnWlProxyAddListener(reinterpret_cast<wl_proxy*>(pBuffer),
                                             reinterpret_cast<Listener*>(const_cast<wl_buffer_listener*>
                                                 (&BufferListener)),
                                             reinterpret_cast<void*>(pImage));

        pImage->SetPresentImageHandle(imageHandle);
    }

    return result;
}

// =====================================================================================================================
// Destroy the present image.
void WaylandWindowSystem::DestroyPresentableImage(
    WindowSystemImageHandle hImage)
{
    wl_proxy* pBuffer = reinterpret_cast<wl_proxy*>(hImage.pBuffer);

    m_waylandProcs.pfnWlProxyMarshal(pBuffer, WL_BUFFER_DESTROY);
    m_waylandProcs.pfnWlProxyDestroy(pBuffer);
}

// =====================================================================================================================
// Ask Wayland server to present a buffer.
Result WaylandWindowSystem::Present(
    const PresentSwapChainInfo& presentInfo,
    PresentFence*               pRenderFence,
    PresentFence*               pIdleFence)
{
    Image&                 srcImage           = static_cast<Image&>(*presentInfo.pSrcImage);
    const ImageCreateInfo& srcImageCreateInfo = srcImage.GetImageCreateInfo();
    void*                  pBuffer            = srcImage.GetPresentImageHandle().pBuffer;
    SwapChain*             pSwapChain         = static_cast<SwapChain*>(presentInfo.pSwapChain);
    const SwapChainMode    swapChainMode      = pSwapChain->CreateInfo().swapChainMode;

    WaylandPresentFence*const pWaylandIdleFence = static_cast<WaylandPresentFence*>(pIdleFence);

    srcImage.SetIdle(false); // From now on, the image/buffer is owned by Wayland.

    m_frameCompleted = false;

    pWaylandIdleFence->AssociateImage(&srcImage);

    m_waylandProcs.pfnWlProxyMarshal(reinterpret_cast<wl_proxy*>(m_pSurfaceWrapper),
                                     WL_SURFACE_ATTACH,
                                     reinterpret_cast<wl_buffer*>(pBuffer),
                                     0,
                                     0);

    m_waylandProcs.pfnWlProxyMarshal(reinterpret_cast<wl_proxy*>(m_pSurfaceWrapper),
                                     WL_SURFACE_DAMAGE,
                                     0,
                                     0,
                                     srcImageCreateInfo.extent.width,
                                     srcImageCreateInfo.extent.height);

    m_waylandProcs.pfnWlProxyMarshal(reinterpret_cast<wl_proxy*>(m_pSurfaceWrapper), WL_SURFACE_COMMIT);
    m_waylandProcs.pfnWlDisplayFlush(m_pDisplay);

    return Result::Success;
}

// =====================================================================================================================
// Wait until the frame is presented. It's called from present thread.
Result WaylandWindowSystem::WaitForLastImagePresented()
{
    Result result = Result::Success;

    wl_interface* pCallbackInterface = m_waylandLoader.GetWlCallbackInterface();

    m_pFrameCallback = reinterpret_cast<wl_callback*>(m_waylandProcs.pfnWlProxyMarshalConstructor(
                                                          reinterpret_cast<wl_proxy*>(m_pSurfaceWrapper),
                                                          WL_SURFACE_FRAME,
                                                          pCallbackInterface,
                                                          nullptr));

    m_waylandProcs.pfnWlProxyAddListener(reinterpret_cast<wl_proxy*>(m_pFrameCallback),
                                         reinterpret_cast<Listener*>(const_cast<wl_callback_listener*>
                                             (&FrameListener)),
                                         this);

    m_waylandProcs.pfnWlProxyMarshal(reinterpret_cast<wl_proxy*>(m_pSurfaceWrapper), WL_SURFACE_COMMIT);
    m_waylandProcs.pfnWlDisplayFlush(m_pDisplay);

    while ((!m_frameCompleted) && (result == Result::Success))
    {
        if (m_waylandProcs.pfnWlDisplayRoundtripQueue(m_pDisplay, m_pSurfaceEventQueue) < 0)
        {
            result = Result::ErrorUnknown;
        }
    }

    return result;
}

// =====================================================================================================================
// Get window width and height.
Result WaylandWindowSystem::GetWindowGeometry(
    Device*             pDevice,
    OsDisplayHandle     hDisplay,
    OsWindowHandle      hWindow,
    Extent2d*           pExtents)
{
    Result result = Result::Success;

    // UINT32_MAX indicates that the surface size will be determined by the extent of a swapchain
    // targeting the surface.
    pExtents->width  = UINT32_MAX;
    pExtents->height = UINT32_MAX;

    return result;
}

// =====================================================================================================================
// Check whether it's presentable or not.
Result WaylandWindowSystem::DeterminePresentationSupported(
    Device*             pDevice,
    OsDisplayHandle     hDisplay,
    int64               visualId)
{
    return Result::Success;
}

} // Amdgpu
} // Pal
