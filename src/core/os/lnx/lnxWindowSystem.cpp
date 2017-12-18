/*
 *******************************************************************************
 *
 * Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "core/os/lnx/dri3/dri3WindowSystem.h"
#include "core/os/lnx/lnxDevice.h"
#include "core/os/lnx/lnxWindowSystem.h"
#include "palSwapChain.h"

namespace Pal
{
namespace Linux
{

// More supported platforms could be added in the future.
constexpr uint32 SupportedPlatformMask = WsiPlatform::Xcb | WsiPlatform::Xlib;

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

}
}
