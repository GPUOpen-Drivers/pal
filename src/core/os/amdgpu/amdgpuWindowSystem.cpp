/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#if PAL_HAVE_DRI3_PLATFORM
#include "core/os/amdgpu/dri3/dri3WindowSystem.h"
#endif
#include "core/os/amdgpu/display/displayWindowSystem.h"
#if PAL_HAVE_WAYLAND_PLATFORM
#include "core/os/amdgpu/wayland/waylandWindowSystem.h"
#endif
#include "core/swapChain.h"

#include "palSwapChain.h"

using namespace Util;

namespace Pal
{
namespace Amdgpu
{

// More supported platforms could be added in the future.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 881
#if   PAL_HAVE_WAYLAND_PLATFORM
constexpr uint32 SupportedPlatformMask = 1 << uint32(WsiPlatform::Xcb)     |
                                         1 << uint32(WsiPlatform::Xlib)    |
                                         1 << uint32(WsiPlatform::Wayland) |
                                         1 << uint32(WsiPlatform::DirectDisplay);
#else
constexpr uint32 SupportedPlatformMask = 1 << uint32(WsiPlatform::Xcb)  |
                                         1 << uint32(WsiPlatform::Xlib) |
                                         1 << uint32(WsiPlatform::DirectDisplay);
#endif
#else
#if   PAL_HAVE_WAYLAND_PLATFORM
constexpr uint32 SupportedPlatformMask = WsiPlatform::Xcb | WsiPlatform::Xlib | WsiPlatform::Wayland |
                                         WsiPlatform::DirectDisplay;
#else
constexpr uint32 SupportedPlatformMask = WsiPlatform::Xcb | WsiPlatform::Xlib | WsiPlatform::DirectDisplay;
#endif
#endif

// =====================================================================================================================
// Initializes a single explicit sync object consisting of root DRM syncobj exported to FD and Wayland syncobj timeline.
Result WindowSystem::InitExplicitSyncObject(
    ExplicitSyncObject* pSyncObject
    ) const
{
    Result                ret           = Result::ErrorInitializationFailed;
    amdgpu_syncobj_handle syncObjHandle = 0;

    // 1. Create DRM sync object
    if (m_device.CreateSyncObject(0, &syncObjHandle) == Result::Success)       // drmSyncobjCreate() underneath
    {
        // 2. Export it to FD
        OsExternalHandle syncObjFd = m_device.ExportSyncObject(syncObjHandle); // drmSyncobjHandleToFD() underneath
        if (syncObjFd != InvalidFd)
        {
            ret                        = Result::Success;
            pSyncObject->syncObjHandle = syncObjHandle;
            pSyncObject->syncObjFd     = syncObjFd;
            pSyncObject->timeline      = 0;
        }
        else
        {
            m_device.DestroySyncObject(syncObjHandle);
        }
    }

    return ret;
}

// =====================================================================================================================
// Destroys explicit sync object resources - DRM syncobj and Wayland syncobj timeline
void WindowSystem::DestroyExplicitSyncObject(
    ExplicitSyncObject* pSyncObject
    ) const
{
    if (pSyncObject->syncObjFd != InvalidFd)
    {
        close(pSyncObject->syncObjFd);
        pSyncObject->syncObjFd = InvalidFd;
    }

    if (pSyncObject->syncObjHandle != 0)
    {
        m_device.DestroySyncObject(pSyncObject->syncObjHandle);
        pSyncObject->syncObjHandle = 0;
    }

    pSyncObject->timeline = 0;
}

// =====================================================================================================================
// Signal the acquire syncobj when the most recently submitted GPU work on the given queue is completed.
// This will inform the compositor that it can start using the image.
Result WindowSystem::SignalExplicitSyncAcquire(
    const ExplicitSyncData& imageExplicitSyncData,
    IQueue*                 pQueue
    ) const
{
    // Undereneath, it will copy the state of the syncobj that was submitted with the recent command buffer
    // to the acquireSyncObj once the command buffer is executed. This way it's not needed for acquireSyncObj
    // to be submitted directly.
    Queue* pAmdGpuQueue = static_cast<Queue*>(pQueue);
    return pAmdGpuQueue->SignalSemaphore(
        reinterpret_cast<amdgpu_semaphore_handle>(imageExplicitSyncData.acquire.syncObjHandle),
        imageExplicitSyncData.acquire.timeline);
}

// =====================================================================================================================
// Wait for the release syncobj to be signaled by the compositor
Result WindowSystem::WaitForExplicitSyncRelease(
    PresentFence* pImagePresentFence,
    bool          doWait
    ) const
{
    Result ret = Result::Unsupported;

    // For PresentFence implementations that don't support explicit sync, data will be null
    const ExplicitSyncData* pImageExplicitSyncData = pImagePresentFence->GetExplicitSyncData();
    if (pImageExplicitSyncData != nullptr)
    {
        if (pImageExplicitSyncData->release.timeline == 0)
        {
            // The timeline has never been incremented, which means the related image hasn't been used yet and it's idle.
            return Result::Success;
        }

        auto timeoutNs = doWait ? std::chrono::nanoseconds::max()
                                : std::chrono::nanoseconds(0);

        // Underneath it's drmSyncobjTimelineWait(). release.timeline is a recently sent release sync point for this image,
        // DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT waits also for an underlying fence materialization if it's still NULL.
        ret = m_device.WaitSemaphoreValue(
            reinterpret_cast<amdgpu_semaphore_handle>(pImageExplicitSyncData->release.syncObjHandle),
            pImageExplicitSyncData->release.timeline,
            DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT,
            timeoutNs);
    }

    return ret;
}

// =====================================================================================================================
// Wait for the release syncobj to be signaled by the compositor
Result WindowSystem::WaitForExplicitSyncReleaseAny(
    PresentFence** ppImagePresentFences,
    uint32         imagePresentFenceCount,
    bool           doWait,
    uint32*        pFirstSignaledIndex
    ) const
{
    PAL_ASSERT(imagePresentFenceCount <= MaxSwapChainLength);

    Result                ret                = Result::Success;
    uint32                firstSignaledIndex = -1;
    amdgpu_syncobj_handle releaseSyncObjHandles[MaxSwapChainLength] = {0};
    uint64                releaseTimelines[MaxSwapChainLength]      = {0};

    // 1. Prepare arrays of syncobj handles and timelines for all images. If any image is idle, return immediately.
    for (uint32 i = 0; i < imagePresentFenceCount; ++i)
    {
        const ExplicitSyncData* pExplicitSyncData = ppImagePresentFences[i]->GetExplicitSyncData();
        if (pExplicitSyncData == nullptr)
        {
            // Passed present fence doesn't support explicit sync, exit
            ret = Result::Unsupported;
            break;
        }

        if (pExplicitSyncData->release.timeline == 0)
        {
            // The timeline has never been incremented, which means the related image hasn't been used yet and is idle.
            firstSignaledIndex = i;
            break;
        }
        releaseSyncObjHandles[i] = pExplicitSyncData->release.syncObjHandle;
        releaseTimelines[i]      = pExplicitSyncData->release.timeline;
    }

    // 2. If none of the images are idle, wait for the first signaled release syncobj
    if ((ret == Result::Success) && (firstSignaledIndex == -1))
    {
        auto timeoutNs = doWait ? std::chrono::nanoseconds::max()
                                : std::chrono::nanoseconds(0);

        // Wait for signal of any of the release syncobjs, return the index of the first one signaled.
        // Underneath it's drmSyncobjTimelineWait(). ReleaseTimelines are recently set release sync points for the images.
        // WAIT_FOR_SUBMIT flag waits for the underlying fences materialization if they're still not submitted.
        ret = m_device.WaitSemaphoresValues(reinterpret_cast<amdgpu_semaphore_handle*>(releaseSyncObjHandles),
                                            releaseTimelines,
                                            imagePresentFenceCount,
                                            DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT,
                                            timeoutNs,
                                            &firstSignaledIndex);
    }

    if (ret == Result::Success)
    {
        *pFirstSignaledIndex = firstSignaledIndex;

        // Make the signaled fence state consistent with this wait
        ppImagePresentFences[firstSignaledIndex]->Reset();
    }

    return ret;
}

// =====================================================================================================================
size_t PresentFence::GetSize(
    WsiPlatform platform)
{
    size_t size = 0;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 881
    if (TestAnyFlagSet(SupportedPlatformMask, 1 << uint32(platform)))
#else
    if (TestAnyFlagSet(SupportedPlatformMask, platform))
#endif
    {
        switch (platform)
        {
#if PAL_HAVE_DRI3_PLATFORM
        case WsiPlatform::Xcb:
        case WsiPlatform::Xlib:
            size = Dri3PresentFence::GetSize();
            break;
#endif
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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 881
    if (TestAnyFlagSet(SupportedPlatformMask, 1 << uint32(platform)))
#else
    if (TestAnyFlagSet(SupportedPlatformMask, platform))
#endif
    {
        switch (platform)
        {
#if PAL_HAVE_DRI3_PLATFORM
        case WsiPlatform::Xcb:
        case WsiPlatform::Xlib:
            result = Dri3PresentFence::Create(static_cast<const Dri3WindowSystem&>(windowSystem),
                                              initiallySignaled,
                                              pPlacementAddr,
                                              ppPresentFence);
            break;
#endif
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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 881
    if (TestAnyFlagSet(SupportedPlatformMask, 1 << uint32(platform)))
#else
    if (TestAnyFlagSet(SupportedPlatformMask, platform))
#endif
    {
        switch (platform)
        {
#if PAL_HAVE_DRI3_PLATFORM
        case WsiPlatform::Xcb:
        case WsiPlatform::Xlib:
            size = Dri3WindowSystem::GetSize();
            break;
#endif
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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 881
    if (TestAnyFlagSet(SupportedPlatformMask, 1 << uint32(createInfo.platform)))
#else
    if (TestAnyFlagSet(SupportedPlatformMask, createInfo.platform))
#endif
    {
        switch (createInfo.platform)
        {
#if PAL_HAVE_DRI3_PLATFORM
        case WsiPlatform::Xcb:
        case WsiPlatform::Xlib:
            result = Dri3WindowSystem::Create(device, createInfo, pPlacementAddr, ppWindowSystem);
            break;
#endif
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
    const Device& device,
    WsiPlatform   platform)
    :
    m_device(device),
    m_platform(platform)
{
    m_windowSystemProperties.u64All = 0;
    m_presentOnSameGpu              = true;
}

// =====================================================================================================================
// Get the window's geometry information through platform specific implementation
Result WindowSystem::GetWindowProperties(
    Device*              pDevice,
    WsiPlatform          platform,
    OsDisplayHandle      hDisplay,
    OsWindowHandle       hWindow,
    SwapChainProperties* pWindowProperties)
{
    Result result = Result::ErrorUnavailable;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 881
    if (TestAnyFlagSet(SupportedPlatformMask, 1 << uint32(platform)))
#else
    if (TestAnyFlagSet(SupportedPlatformMask, platform))
#endif
    {
        switch (platform)
        {
#if PAL_HAVE_DRI3_PLATFORM
        case WsiPlatform::Xcb:
            result = Dri3WindowSystem::GetWindowProperties(pDevice, hDisplay, hWindow, pWindowProperties);
            break;
        case WsiPlatform::Xlib:
            result = Dri3WindowSystem::GetWindowPropertiesXlib(pDevice, hDisplay, hWindow, pWindowProperties);
            break;
#endif
#if PAL_HAVE_WAYLAND_PLATFORM
        case WsiPlatform::Wayland:
            result = WaylandWindowSystem::GetWindowProperties(pDevice, hDisplay, hWindow, pWindowProperties);
            break;
#endif
        case WsiPlatform::DirectDisplay:
            result = DisplayWindowSystem::GetWindowProperties(pDevice, hDisplay, hWindow, pWindowProperties);
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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 881
    if (TestAnyFlagSet(SupportedPlatformMask, 1 << uint32(platform)))
#else
    if (TestAnyFlagSet(SupportedPlatformMask, platform))
#endif
    {
        switch (platform)
        {
#if PAL_HAVE_DRI3_PLATFORM
        case WsiPlatform::Xcb:
            result = Dri3WindowSystem::DeterminePresentationSupported(pDevice, hDisplay, visualId);
            break;
        case WsiPlatform::Xlib:
            result = Dri3WindowSystem::DeterminePresentationSupportedXlib(pDevice, hDisplay, visualId);
            break;
#endif
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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 881
    if (TestAnyFlagSet(SupportedPlatformMask, 1 << uint32(wsiPlatform)))
#else
    if (TestAnyFlagSet(SupportedPlatformMask, wsiPlatform))
#endif
    {
        switch (wsiPlatform)
        {
#if PAL_HAVE_DRI3_PLATFORM
        case WsiPlatform::Xcb:
        case WsiPlatform::Xlib:
            result = Dri3WindowSystem::AcquireScreenAccess(hDisplay,
                                                           pDevice,
                                                           connector,
                                                           pRandrOutput,
                                                           pDrmMasterFd);
            break;
#endif
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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 881
    if (TestAnyFlagSet(SupportedPlatformMask, 1 << uint32(wsiPlatform)))
#else
    if (TestAnyFlagSet(SupportedPlatformMask, wsiPlatform))
#endif
    {
        switch (wsiPlatform)
        {
#if PAL_HAVE_DRI3_PLATFORM
        case WsiPlatform::Xcb:
        case WsiPlatform::Xlib:
            result = Dri3WindowSystem::GetOutputFromConnector(hDisplay, pDevice, connector, pOutput);
            break;
#endif
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
