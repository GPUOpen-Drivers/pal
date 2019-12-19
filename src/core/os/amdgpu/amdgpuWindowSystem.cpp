/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/os/amdgpu/amdgpuWindowSystem.h"
#include "core/os/amdgpu/dri3/dri3WindowSystem.h"
#include "core/os/amdgpu/display/displayWindowSystem.h"
#if PAL_HAVE_WAYLAND_PLATFORM
#include "core/os/amdgpu/wayland/waylandWindowSystem.h"
#endif

#include "palSwapChain.h"

namespace Pal
{
namespace Amdgpu
{

// More supported platforms could be added in the future.
#if   PAL_HAVE_WAYLAND_PLATFORM
constexpr uint32 SupportedPlatformMask =
    WsiPlatform::Xcb | WsiPlatform::Xlib | WsiPlatform::Wayland | WsiPlatform::DirectDisplay;
#else
constexpr uint32 SupportedPlatformMask = WsiPlatform::Xcb | WsiPlatform::Xlib | WsiPlatform::DirectDisplay;
#endif

// =====================================================================================================================
size_t PresentFence::GetSize(
    WsiPlatform platform)
{
    size_t size = 0;

    if (SupportedPlatformMask & platform)
    {
        switch (platform)
        {
        case WsiPlatform::Xcb:
        case WsiPlatform::Xlib:
            size = Dri3PresentFence::GetSize();
            break;
#if PAL_HAVE_WAYLAND_PLATFORM
        case WsiPlatform::Wayland:
            size = WaylandPresentFence::GetSize();
            break;
#endif
        case WsiPlatform::DirectDisplay:
            size = DisplayPresentFence::GetSize();
            break;
        default:
            PAL_NOT_IMPLEMENTED();
            break;
        }
    }
    else
    {
        PAL_NOT_IMPLEMENTED();
    }

    return size;
}

// =====================================================================================================================
Result PresentFence::Create(
    const WindowSystem& windowSystem,
    bool                initiallySignaled,
    void*               pPlacementAddr,
    PresentFence**      ppPresentFence)
{
    const WsiPlatform platform = windowSystem.PlatformType();
    Result            result   = Result::ErrorUnavailable;

    if (SupportedPlatformMask & platform)
    {
        switch (platform)
        {
        case WsiPlatform::Xcb:
        case WsiPlatform::Xlib:
            result = Dri3PresentFence::Create(static_cast<const Dri3WindowSystem&>(windowSystem),
                                              initiallySignaled,
                                              pPlacementAddr,
                                              ppPresentFence);
            break;
#if PAL_HAVE_WAYLAND_PLATFORM
        case WsiPlatform::Wayland:
            result = WaylandPresentFence::Create(static_cast<const WaylandWindowSystem&>(windowSystem),
                                                 initiallySignaled,
                                                 pPlacementAddr,
                                                 ppPresentFence);
            break;
#endif
        case WsiPlatform::DirectDisplay:
            result = DisplayPresentFence::Create(static_cast<const DisplayWindowSystem&>(windowSystem),
                                                 initiallySignaled,
                                                 pPlacementAddr,
                                                 ppPresentFence);
            break;
        default:
            PAL_NOT_IMPLEMENTED();
            break;
        }
    }
    else
    {
        PAL_NOT_IMPLEMENTED();
    }

    return result;
}

// =====================================================================================================================
size_t WindowSystem::GetSize(
    WsiPlatform platform)
{
    size_t size = 0;

    if (SupportedPlatformMask & platform)
    {
        switch (platform)
        {
        case WsiPlatform::Xcb:
        case WsiPlatform::Xlib:
            size = Dri3WindowSystem::GetSize();
            break;
#if PAL_HAVE_WAYLAND_PLATFORM
        case WsiPlatform::Wayland:
            size = WaylandWindowSystem::GetSize();
            break;
#endif
        case WsiPlatform::DirectDisplay:
            size = DisplayWindowSystem::GetSize();
            break;
        default:
            PAL_NOT_IMPLEMENTED();
            break;
        }
    }
    else
    {
        PAL_NOT_IMPLEMENTED();
    }

    return size;
}

// =====================================================================================================================
Result WindowSystem::Create(
    const Device&                 device,
    const WindowSystemCreateInfo& createInfo,
    void*                         pPlacementAddr,
    WindowSystem**                ppWindowSystem)
{
    Result result = Result::ErrorUnavailable;

    if (SupportedPlatformMask & createInfo.platform)
    {
        switch (createInfo.platform)
        {
        case WsiPlatform::Xcb:
        case WsiPlatform::Xlib:
            result = Dri3WindowSystem::Create(device, createInfo, pPlacementAddr, ppWindowSystem);
            break;
#if PAL_HAVE_WAYLAND_PLATFORM
        case WsiPlatform::Wayland:
            result = WaylandWindowSystem::Create(device, createInfo, pPlacementAddr, ppWindowSystem);
            break;
#endif
        case WsiPlatform::DirectDisplay:
            result = DisplayWindowSystem::Create(device, createInfo, pPlacementAddr, ppWindowSystem);
            break;
        default:
            PAL_NOT_IMPLEMENTED();
            break;
        }
    }
    else
    {
        PAL_NOT_IMPLEMENTED();
    }

    return result;
}

// =====================================================================================================================
WindowSystem::WindowSystem(
    WsiPlatform platform)
    :
    m_platform(platform)
{
    m_windowSystemProperties.u64All = 0;
    m_presentOnSameGpu              = true;
}

// =====================================================================================================================
// Get the window's geometry information through platform specific implementation
Result WindowSystem::GetWindowGeometry(
    Device*         pDevice,
    WsiPlatform     platform,
    OsDisplayHandle hDisplay,
    OsWindowHandle  hWindow,
    Extent2d*       pExtents)
{
    Result result = Result::ErrorUnavailable;

    if (SupportedPlatformMask & platform)
    {
        switch (platform)
        {
        case WsiPlatform::Xcb:
            result = Dri3WindowSystem::GetWindowGeometry(pDevice, hDisplay, hWindow, pExtents);
            break;
        case WsiPlatform::Xlib:
            result = Dri3WindowSystem::GetWindowGeometryXlib(pDevice, hDisplay, hWindow, pExtents);
            break;
#if PAL_HAVE_WAYLAND_PLATFORM
        case WsiPlatform::Wayland:
            result = WaylandWindowSystem::GetWindowGeometry(pDevice, hDisplay, hWindow, pExtents);
            break;
#endif
        case WsiPlatform::DirectDisplay:
            result = Result::Success;
            break;
        default:
            PAL_NEVER_CALLED();
            break;
        }
    }
    else
    {
        PAL_NEVER_CALLED();
    }

    return result;
}
// =====================================================================================================================
// Determine whether the presentation is supported in platform with certain visual id.
Result WindowSystem::DeterminePresentationSupported(
    Device*         pDevice,
    OsDisplayHandle hDisplay,
    WsiPlatform     platform,
    int64           visualId)
{
    Result result = Result::ErrorUnavailable;

    if (SupportedPlatformMask & platform)
    {
        switch (platform)
        {
        case WsiPlatform::Xcb:
            result = Dri3WindowSystem::DeterminePresentationSupported(pDevice, hDisplay, visualId);
            break;
        case WsiPlatform::Xlib:
            result = Dri3WindowSystem::DeterminePresentationSupportedXlib(pDevice, hDisplay, visualId);
            break;
#if PAL_HAVE_WAYLAND_PLATFORM
        case WsiPlatform::Wayland:
            result = WaylandWindowSystem::DeterminePresentationSupported(pDevice, hDisplay, visualId);
            break;
#endif
        case WsiPlatform::DirectDisplay:
            result = DisplayWindowSystem::DeterminePresentationSupported(pDevice, hDisplay, visualId);
            break;
        default:
            PAL_NEVER_CALLED();
            break;
        }
    }
    else
    {
        PAL_NEVER_CALLED();
    }

    return result;
}

// =====================================================================================================================
Result WindowSystem::AcquireScreenAccess(
    Device*         pDevice,
    OsDisplayHandle hDisplay,
    WsiPlatform     wsiPlatform,
    uint32          connector,
    uint32*         pRandrOutput,
    int32*          pDrmMasterFd)
{
    Result result = Result::ErrorUnavailable;
    if (SupportedPlatformMask & wsiPlatform)
    {
        switch (wsiPlatform)
        {
        case WsiPlatform::Xcb:
        case WsiPlatform::Xlib:
            result = Dri3WindowSystem::AcquireScreenAccess(hDisplay,
                                                           pDevice,
                                                           connector,
                                                           pRandrOutput,
                                                           pDrmMasterFd);
            break;
        default:
            PAL_NOT_IMPLEMENTED();
            break;
        }
    }
    else
    {
        PAL_NOT_IMPLEMENTED();
    }

    return result;
}

// =====================================================================================================================
Result WindowSystem::GetOutputFromConnector(
    OsDisplayHandle hDisplay,
    Device*         pDevice,
    WsiPlatform     wsiPlatform,
    uint32          connector,
    uint32*         pOutput)
{
    Result result = Result::ErrorUnavailable;

    if (SupportedPlatformMask & wsiPlatform)
    {
        switch (wsiPlatform)
        {
        case WsiPlatform::Xcb:
        case WsiPlatform::Xlib:
            result = Dri3WindowSystem::GetOutputFromConnector(hDisplay, pDevice, connector, pOutput);
            break;
        default:
            PAL_NOT_IMPLEMENTED();
            break;
        }
    }
    else
    {
        PAL_NOT_IMPLEMENTED();
    }

    return result;
}

} // Amdgpu
} // Pal
