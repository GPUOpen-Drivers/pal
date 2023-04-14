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

#include "core/os/amdgpu/dri3/dri3WindowSystem.h"
#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuImage.h"
#include "core/os/amdgpu/amdgpuPlatform.h"
#include "util/lnx/lnxTimeout.h"
#include "palScreen.h"
#include "palSwapChain.h"

#include <algorithm>

extern "C"
{
#include <X11/xshmfence.h>
}

using namespace Util;

namespace Pal
{
namespace Amdgpu
{

constexpr uint32 InvalidPixmapId = -1;
constexpr uint8  PropSizeInBit   = 32;

// =====================================================================================================================
Result Dri3PresentFence::Create(
    const Dri3WindowSystem& windowSystem,
    bool                    initiallySignaled,
    void*                   pPlacementAddr,
    PresentFence**          ppPresentFence)
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppPresentFence != nullptr));

    auto*const pPresentFence = PAL_PLACEMENT_NEW(pPlacementAddr) Dri3PresentFence(windowSystem);
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
Dri3PresentFence::Dri3PresentFence(
    const Dri3WindowSystem& windowSystem)
    :
    m_windowSystem(windowSystem),
    m_syncFence(0),
    m_pShmFence(nullptr),
    m_presented(false),
    m_pImage(nullptr)
{
}

// =====================================================================================================================
Dri3PresentFence::~Dri3PresentFence()
{
    if (m_syncFence != 0)
    {
        const xcb_void_cookie_t cookie =
            m_windowSystem.m_dri3Procs.pfnXcbSyncDestroyFenceChecked(m_windowSystem.m_pConnection, m_syncFence);

#if PAL_ENABLE_PRINTS_ASSERTS
        xcb_generic_error_t*const pError =
            m_windowSystem.m_dri3Procs.pfnXcbRequestCheck(m_windowSystem.m_pConnection, cookie);

        if (pError != nullptr)
        {
            PAL_ASSERT_ALWAYS();
            free(pError);
        }
#endif

        m_syncFence = 0;
    }

    if (m_pShmFence != nullptr)
    {
        m_windowSystem.m_dri3Procs.pfnXshmfenceUnmapShm(m_pShmFence);
        m_pShmFence = nullptr;
    }
}

// =====================================================================================================================
// Create and initialize all of the fence's member objects. Signal the fence if it's initially signaled. We can rely on
// the destructor to free/close these objects if an error occurs during initialization.
Result Dri3PresentFence::Init(
    bool initiallySignaled)
{
    m_syncFence   = m_windowSystem.m_dri3Procs.pfnXcbGenerateId(m_windowSystem.m_pConnection);
    Result result = (m_syncFence != 0) ? Result::Success : Result::ErrorUnknown;

    if (m_windowSystem.Dri3Supported())
    {
        // Using shared memory fences is faster but requires DRI3
        // This works even if we're using software compositing for everything else.
        int32 fenceFd = InvalidFd;

        if (result == Result::Success)
        {
            fenceFd = m_windowSystem.m_dri3Procs.pfnXshmfenceAllocShm();

            if (fenceFd < 0)
            {
                result = Result::ErrorUnknown;
            }
        }

        if (result == Result::Success)
        {
            m_pShmFence = m_windowSystem.m_dri3Procs.pfnXshmfenceMapShm(fenceFd);

            if (m_pShmFence == nullptr)
            {
                result = Result::ErrorUnknown;
            }
        }

        if (result == Result::Success)
        {
            const xcb_void_cookie_t cookie =
                m_windowSystem.m_dri3Procs.pfnXcbDri3FenceFromFdChecked(m_windowSystem.m_pConnection,
                                                                        m_windowSystem.m_hWindow,
                                                                        m_syncFence,
                                                                        initiallySignaled,
                                                                        fenceFd);

            xcb_generic_error_t*const pError =
                m_windowSystem.m_dri3Procs.pfnXcbRequestCheck(m_windowSystem.m_pConnection, cookie);

            if (pError != nullptr)
            {
                free(pError);
                result = Result::ErrorUnknown;
            }
        }

        if (initiallySignaled && (result == Result::Success))
        {
            m_windowSystem.m_dri3Procs.pfnXshmfenceTrigger(m_pShmFence);
        }
    }
    else
    {
        const xcb_void_cookie_t cookie =
            m_windowSystem.m_dri3Procs.pfnXcbSyncCreateFenceChecked(m_windowSystem.m_pConnection,
                                                                    m_windowSystem.m_hWindow,
                                                                    m_syncFence,
                                                                    initiallySignaled);

        xcb_generic_error_t*const pError =
            m_windowSystem.m_dri3Procs.pfnXcbRequestCheck(m_windowSystem.m_pConnection, cookie);

        if (pError != nullptr)
        {
            free(pError);
            result = Result::ErrorUnknown;
        }
    }

    return result;
}

// =====================================================================================================================
void Dri3PresentFence::Reset()
{
    if (m_pShmFence != nullptr)
    {
        m_windowSystem.m_dri3Procs.pfnXshmfenceReset(m_pShmFence);
    }
    else
    {
        PAL_ASSERT(m_syncFence != 0);
        m_windowSystem.m_dri3Procs.pfnXcbSyncResetFence(m_windowSystem.m_pConnection, m_syncFence);
    }
    m_presented = false;
}

// =====================================================================================================================
// Trigger the SyncFence object.
Result Dri3PresentFence::Trigger()
{
    Result result = Result::Success;

    if (m_pShmFence != nullptr)
    {
        m_windowSystem.m_dri3Procs.pfnXshmfenceTrigger(m_pShmFence);
        m_presented = true;
    }
    else
    {
        PAL_ASSERT(m_syncFence != 0);
        const xcb_void_cookie_t cookie =
            m_windowSystem.m_dri3Procs.pfnXcbSyncTriggerFenceChecked(m_windowSystem.m_pConnection, m_syncFence);

        xcb_generic_error_t*const pError =
            m_windowSystem.m_dri3Procs.pfnXcbRequestCheck(m_windowSystem.m_pConnection, cookie);

        if (pError != nullptr)
        {
            free(pError);
            result = Result::ErrorUnknown;
        }
        else
        {
            m_presented = true;
        }
    }

    return result;
}

// =====================================================================================================================
// Wait for the idle fence to be signaled which indicates that the pixmap is not being used by XServer anymore.
Result Dri3PresentFence::WaitForCompletion(
    bool doWait)
{
    Result result = Result::Success;

    if (m_presented == false)
    {
        result = Result::ErrorFenceNeverSubmitted;
    }

    if (result == Result::Success)
    {
        if (doWait)
        {
            if (m_pShmFence != nullptr)
            {
                if (m_windowSystem.m_dri3Procs.pfnXshmfenceAwait(m_pShmFence) != 0)
                {
                    result = Result::ErrorUnknown;
                }
            }
            else
            {
                PAL_ASSERT(m_syncFence != 0);
                const xcb_void_cookie_t cookie =
                    m_windowSystem.m_dri3Procs.pfnXcbSyncAwaitFenceChecked(m_windowSystem.m_pConnection,
                                                                           1,
                                                                           &m_syncFence);

                xcb_generic_error_t*const pError =
                    m_windowSystem.m_dri3Procs.pfnXcbRequestCheck(m_windowSystem.m_pConnection, cookie);

                if (pError != nullptr)
                {
                    free(pError);
                    result = Result::ErrorUnknown;
                }
            }
        }
        else
        {
            result = QueryRaw();
        }
    }

    if (result == Result::Success)
    {
        m_pImage->SetIdle(true);
    }

    return result;
}

// =====================================================================================================================
// Check status of the fence
Result Dri3PresentFence::QueryRaw()
{
    Result result = Result::Success;

    if (m_pShmFence != nullptr)
    {
        if (m_windowSystem.m_dri3Procs.pfnXshmfenceQuery(m_pShmFence) == 0)
        {
            result = Result::NotReady;
        }
    }
    else
    {
        PAL_ASSERT(m_syncFence != 0);
        const xcb_sync_query_fence_cookie_t cookie =
            m_windowSystem.m_dri3Procs.pfnXcbSyncQueryFence(m_windowSystem.m_pConnection, m_syncFence);

        xcb_generic_error_t* pError = nullptr;
        xcb_sync_query_fence_reply_t*const pFenceReply =
            m_windowSystem.m_dri3Procs.pfnXcbSyncQueryFenceReply(m_windowSystem.m_pConnection,
                                                                 cookie,
                                                                 &pError);

        if (pError != nullptr)
        {
            // Some error occurred
            result = Result::ErrorUnknown;
        }
        else if (pFenceReply != nullptr)
        {
            if (pFenceReply->triggered == 0)
            {
                result = Result::NotReady;
            }
        }
        else
        {
            // No error but no result?!?
            result = Result::ErrorUnknown;
        }

        free(pError);
        free(pFenceReply);
    }

    return result;
}

// =====================================================================================================================
Result Dri3WindowSystem::Create(
    const Device&                 device,
    const WindowSystemCreateInfo& createInfo,
    void*                         pPlacementAddr,
    WindowSystem**                ppWindowSystem)
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppWindowSystem != nullptr));

    auto*const pWindowSystem = PAL_PLACEMENT_NEW(pPlacementAddr) Dri3WindowSystem(device, createInfo);
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
Dri3WindowSystem::Dri3WindowSystem(
    const Device&                 device,
    const WindowSystemCreateInfo& createInfo)
    :
    WindowSystem(createInfo.platform),
    m_device(device),
    m_dri3Loader(device.GetPlatform()->GetDri3Loader()),
#if defined(PAL_DEBUG_PRINTS)
    m_dri3Procs(m_dri3Loader.GetProcsTableProxy()),
#else
    m_dri3Procs(m_dri3Loader.GetProcsTable()),
#endif
    m_format(createInfo.format),
    m_depth(0),
    m_swapChainMode(createInfo.swapChainMode),
    m_hWindow(static_cast<xcb_window_t>(createInfo.hWindow.win)),
    m_windowWidth(0),
    m_windowHeight(0),
    m_needWindowSizeChangedCheck(false),
    m_pConnection(nullptr),
    m_dri2Supported(true),
    m_dri3Supported(true),
    m_dri3MajorVersion(0),
    m_dri3MinorVersion(0),
    m_presentMajorVersion(0),
    m_presentMinorVersion(0),
    m_pPresentEvent(nullptr),
    m_localSerial(0),
    m_remoteSerial(0)
{
    PAL_ASSERT(createInfo.hDisplay != nullptr);
    PAL_ASSERT(createInfo.hWindow.win <= UINT_MAX);

    if (createInfo.platform == WsiPlatform::Xlib)
    {
        Display*const pDpy = static_cast<Display*>(createInfo.hDisplay);
        m_pConnection      = m_dri3Procs.pfnXGetXCBConnection(pDpy);
    }
    else
    {
        m_pConnection = static_cast<xcb_connection_t*>(createInfo.hDisplay);
    }
}

// =====================================================================================================================
Dri3WindowSystem::~Dri3WindowSystem()
{
    if (m_pPresentEvent != nullptr)
    {
        m_dri3Procs.pfnXcbUnregisterForSpecialEvent(m_pConnection, m_pPresentEvent);
    }
}

// =====================================================================================================================
// Initialize Dri3 and Present extension, query their versions and select the interested event here.
Result Dri3WindowSystem::Init()
{
    Result result = Result::Success;

    if (m_pConnection != nullptr)
    {
        if (IsExtensionSupported() == false)
        {
            result = Result::ErrorInitializationFailed;
        }

        if (result == Result::Success)
        {
            int32 fd = OpenDri3();
            if (m_dri3Supported)
            {
                if (fd != InvalidFd)
                {
                    result = m_device.IsSameGpu(fd, &m_presentOnSameGpu);

                    close(fd);
                }
                else
                {
                    result = Result::ErrorInitializationFailed;
                }
            }
            else
            {
                m_presentOnSameGpu = false;
            }
        }

        if (result == Result::Success)
        {
            result = QueryVersion();
        }

        if (result == Result::Success)
        {
            if (IsFormatPresentable(m_format) == false)
            {
                result = Result::ErrorInvalidFormat;
            }
            else
            {
                result = SelectEvent();
            }
        }

        if (result == Result::Success)
        {
            const auto& settings = m_device.Settings();

            if (settings.enableAdaptiveSync)
            {
                SetAdaptiveSyncProperty(true);
            }

            // Get the window size from Xorg
            const Dri3LoaderFuncs&          dri3Procs = m_device.GetPlatform()->GetDri3Loader().GetProcsTable();
            const xcb_get_geometry_cookie_t cookie    = dri3Procs.pfnXcbGetGeometry(m_pConnection, m_hWindow);

            xcb_get_geometry_reply_t*const  pReply = dri3Procs.pfnXcbGetGeometryReply(m_pConnection, cookie, nullptr);

            if (pReply != nullptr)
            {
                m_windowWidth  = pReply->width;
                m_windowHeight = pReply->height;

                free(pReply);
            }
            else
            {
                result = Result::ErrorInitializationFailed;
            }

        }
    }
    else
    {
        result = Result::ErrorInitializationFailed;
    }

    return result;
}

// =====================================================================================================================
// Check if the format is compatible with X Server.
bool Dri3WindowSystem::IsFormatPresentable(
    SwizzledFormat format)
{
    // TODO: Implement it.
    // PAL_NOT_IMPLEMENTED();
    return true;
}

// =====================================================================================================================
// Check if DRI2, DRI3 and Present extensions are supported by Xserver
bool Dri3WindowSystem::IsExtensionSupported()
{
    const xcb_query_extension_reply_t* pReply = nullptr;
    bool                               result = true;

    m_dri3Procs.pfnXcbPrefetchExtensionData(m_pConnection, m_dri3Loader.GetXcbDri2Id());
    m_dri3Procs.pfnXcbPrefetchExtensionData(m_pConnection, m_dri3Loader.GetXcbDri3Id());
    m_dri3Procs.pfnXcbPrefetchExtensionData(m_pConnection, m_dri3Loader.GetXcbPresentId());

    pReply = m_dri3Procs.pfnXcbGetExtensionData(m_pConnection, m_dri3Loader.GetXcbDri2Id());
    if ((pReply == nullptr) || (pReply->present == 0))
    {
        m_dri2Supported = false;
    }

    pReply = m_dri3Procs.pfnXcbGetExtensionData(m_pConnection, m_dri3Loader.GetXcbDri3Id());
    if ((pReply == nullptr) || (pReply->present == 0))
    {
        m_dri3Supported = false;
        if (m_device.Settings().forcePresentViaCpuBlt == false)
        {
            // If not using CPU blits, this ext is required
            result = false;
        }
    }

    if (result == true)
    {
        pReply = m_dri3Procs.pfnXcbGetExtensionData(m_pConnection, m_dri3Loader.GetXcbPresentId());
        if ((pReply == nullptr) || (pReply->present == 0))
        {
            // This ext is required
            result = false;
        }
    }

    return result;
}

// =====================================================================================================================
// Send DRI3-Open request to Xserver to get the related GPU file descriptor.
int32 Dri3WindowSystem::OpenDri3()
{
    int32 fd = InvalidFd;
    int32 driverNameLength;

    if (m_dri3Supported)
    {
        const xcb_randr_provider_t   provider = 0;
        const xcb_dri3_open_cookie_t cookie   = m_dri3Procs.pfnXcbDri3Open(m_pConnection, m_hWindow, provider);
        xcb_dri3_open_reply_t*const  pReply   = m_dri3Procs.pfnXcbDri3OpenReply(m_pConnection, cookie, NULL);
        m_windowSystemProperties.supportFreeSyncExtension = 0;

        if (pReply != nullptr)
        {
            fd = (pReply->nfd == 1) ? m_dri3Procs.pfnXcbDri3OpenReplyFds(m_pConnection, pReply)[0] : InvalidFd;
            free(pReply);
        }
    }

    if (m_dri2Supported)
    {
        constexpr char ProDdxVendorString[] = "amdgpu";

        const xcb_dri2_connect_cookie_t dri2Cookie = m_dri3Procs.pfnXcbDri2Connect(m_pConnection,
                                                                                   m_hWindow,
                                                                                   DRI2DriverDRI);
        xcb_dri2_connect_reply_t*const  pDri2Reply = m_dri3Procs.pfnXcbDri2ConnectReply(m_pConnection,
                                                                                        dri2Cookie,
                                                                                        NULL);

        if ((pDri2Reply != nullptr) && (m_dri3Procs.pfnXcbDri2ConnectDriverNameLength(pDri2Reply) > 0))
        {
            const char*const pName = m_dri3Procs.pfnXcbDri2ConnectDriverName(pDri2Reply);
            if (strncmp(pName, ProDdxVendorString, strlen(ProDdxVendorString)) == 0)
            {
                m_windowSystemProperties.supportFreeSyncExtension = 1;
            }
        }

        if (pDri2Reply != nullptr)
        {
            free(pDri2Reply);
        }
    }

    if (m_device.Settings().forcePresentViaCpuBlt)
    {
        m_graphicsContext = m_dri3Procs.pfnXcbGenerateId(m_pConnection);

        const xcb_void_cookie_t gcCookie = m_dri3Procs.pfnXcbCreateGcChecked(m_pConnection,
                                                                             m_graphicsContext,
                                                                             m_hWindow,
                                                                             0,
                                                                             nullptr);

        xcb_generic_error_t*const pError = m_dri3Procs.pfnXcbRequestCheck(m_pConnection, gcCookie);
        PAL_ASSERT(pError == nullptr);
    }

    return fd;
}

// =====================================================================================================================
// Query DRI3 and Present extension versions
Result Dri3WindowSystem::QueryVersion()
{
    Result result = Result::Success;

    if (m_dri3Supported)
    {
        const xcb_dri3_query_version_cookie_t dri3Cookie =
            m_dri3Procs.pfnXcbDri3QueryVersion(m_pConnection, XCB_DRI3_MAJOR_VERSION, XCB_DRI3_MINOR_VERSION);

        xcb_dri3_query_version_reply_t*const pDri3Reply =
            m_dri3Procs.pfnXcbDri3QueryVersionReply(m_pConnection, dri3Cookie, NULL);

        if (pDri3Reply != nullptr)
        {
            m_dri3MajorVersion = pDri3Reply->major_version;
            m_dri3MinorVersion = pDri3Reply->minor_version;
            free(pDri3Reply);
        }
        else
        {
            result = Result::ErrorUnknown;
        }
    }

    if (result == Result::Success)
    {
        const xcb_present_query_version_cookie_t presentCookie =
            m_dri3Procs.pfnXcbPresentQueryVersion(m_pConnection, XCB_PRESENT_MAJOR_VERSION, XCB_PRESENT_MINOR_VERSION);

        xcb_present_query_version_reply_t*const pPresentReply =
            m_dri3Procs.pfnXcbPresentQueryVersionReply(m_pConnection, presentCookie, NULL);

        if (pPresentReply != nullptr)
        {
            m_presentMajorVersion = pPresentReply->major_version;
            m_presentMinorVersion = pPresentReply->minor_version;
            free(pPresentReply);
        }
        else
        {
            result = Result::ErrorUnknown;
        }
    }

    return result;
}

// =====================================================================================================================
// Select insterested events from Xserver. XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY is selected here which can be polled
// to get the completed present event. Complete-event means that the present action in Xserver is finished, for
// blit-present it means the presentable image is free for client to render.
Result Dri3WindowSystem::SelectEvent()
{
    Result result = Result::Success;

    // Create the event queue.
    const xcb_present_event_t eventId = m_dri3Procs.pfnXcbGenerateId(m_pConnection);
    xcb_special_event_t*const pEvent  = m_dri3Procs.pfnXcbRegisterForSpecialXge(m_pConnection,
                                                                                m_dri3Loader.GetXcbPresentId(),
                                                                                eventId,
                                                                                NULL);
    const xcb_void_cookie_t cookie =
        m_dri3Procs.pfnXcbPresentSelectInputChecked(m_pConnection,
                                                    eventId,
                                                    m_hWindow,
                                                    XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY |
                                                    XCB_PRESENT_EVENT_MASK_CONFIGURE_NOTIFY |
                                                    XCB_PRESENT_EVENT_MASK_IDLE_NOTIFY);

    xcb_generic_error_t*const pError = m_dri3Procs.pfnXcbRequestCheck(m_pConnection, cookie);

    if (pError == nullptr)
    {
        m_pPresentEvent = pEvent;
    }
    else
    {
        free(pError);
        if (pEvent != nullptr)
        {
            m_dri3Procs.pfnXcbUnregisterForSpecialEvent(m_pConnection, pEvent);
        }
        result = Result::ErrorUnknown;
    }

    return result;
}

// =====================================================================================================================
// Interface for the window system to do things related with creating presentable image. For XCB/Dri3 backend, it gets
// a pixmap packaging memory the image binds from Xserver. Then this pixmap can be presented by Xserver.
Result Dri3WindowSystem::CreatePresentableImage(
    SwapChain* pSwapChain,
    Image*     pImage,
    int32      sharedBufferFd) // Xserver can use the shared buffer (created in client side) by sharedBufferFd
{
    Result       result     = Result::Success;
    xcb_pixmap_t pixmap     = InvalidPixmapId;

    const SubResourceInfo*const pSubResInfo = pImage->SubresourceInfo(0);

    const uint32 width  = pSubResInfo->extentTexels.width;
    const uint32 height = pSubResInfo->extentTexels.height;
    const uint32 stride = pSubResInfo->rowPitch;
    const uint32 size   = pSubResInfo->size;
    const uint32 bpp    = pSubResInfo->bitsPerTexel;

    if ((width == 0) || (height == 0) || (stride == 0) || (bpp == 0) || (sharedBufferFd == InvalidFd))
    {
        result = Result::ErrorInvalidValue;
    }

    if (result == Result::Success)
    {
        const xcb_get_geometry_cookie_t cookie = m_dri3Procs.pfnXcbGetGeometry(m_pConnection, m_hWindow);
        xcb_get_geometry_reply_t*const  pReply = m_dri3Procs.pfnXcbGetGeometryReply(m_pConnection, cookie, nullptr);

        if (pReply != nullptr)
        {
            m_depth = pReply->depth;

            free(pReply);
        }
        else
        {
            result = Result::ErrorUnknown;
        }
    }

    if (result == Result::Success)
    {
        pixmap = m_dri3Procs.pfnXcbGenerateId(m_pConnection);
        if (pixmap == InvalidPixmapId)
        {
            result = Result::ErrorUnknown;
        }
    }

    if (result == Result::Success)
    {
        xcb_void_cookie_t cookie = {};
        if (m_device.Settings().forcePresentViaCpuBlt)
        {
            cookie = m_dri3Procs.pfnXcbCreatePixmapChecked(m_pConnection,
                                                           m_depth,
                                                           pixmap,
                                                           m_hWindow,
                                                           width,
                                                           height);
        }
        else
        {
            cookie = m_dri3Procs.pfnXcbDri3PixmapFromBufferChecked(m_pConnection,
                                                                   pixmap,
                                                                   m_hWindow,
                                                                   size,
                                                                   width,
                                                                   height,
                                                                   stride,
                                                                   m_depth,
                                                                   bpp,
                                                                   sharedBufferFd);
        }

        xcb_generic_error_t*const pError = m_dri3Procs.pfnXcbRequestCheck(m_pConnection, cookie);

        if (pError != nullptr)
        {
            free(pError);

            // On error, the id will be wasted because Xlib/xcb doesn't provide an interface to reclaim the id.
            result = Result::ErrorUnknown;
        }
    }
    if (result == Result::Success)
    {
        WindowSystemImageHandle imageHandle = { .hPixmap = pixmap };

        pImage->SetPresentImageHandle(imageHandle);
    }

    return result;
}

// =====================================================================================================================
void Dri3WindowSystem::WaitOnIdleEvent(
    WindowSystemImageHandle* pImage)
{
    WindowSystemImageHandle image = NullImageHandle;

    while (image.hPixmap == 0)
    {
        xcb_present_generic_event_t*const pPresentEvent = reinterpret_cast<xcb_present_generic_event_t*>(
            m_dri3Procs.pfnXcbWaitForSpecialEvent(m_pConnection, m_pPresentEvent));

        if (pPresentEvent == nullptr)
        {
            break;
        }

        HandlePresentEvent(pPresentEvent, &image);
    }

    pImage->hPixmap = image.hPixmap;
}

// =====================================================================================================================
// Destroy the present image.
// the present image is pixmap in dri3 platform.
void Dri3WindowSystem::DestroyPresentableImage(
    WindowSystemImageHandle hImage)
{
    const xcb_void_cookie_t   cookie = m_dri3Procs.pfnXcbFreePixmapChecked(m_pConnection, hImage.hPixmap);
#if PAL_ENABLE_PRINTS_ASSERTS
    xcb_generic_error_t*const pError = m_dri3Procs.pfnXcbRequestCheck(m_pConnection, cookie);

    if (pError != nullptr)
    {
        PAL_ASSERT_ALWAYS();
        free(pError);
    }
#endif
}

// =====================================================================================================================
// Present a pixmap by DRI3/Present extension; generate the serial number which is used by function
// WaitForPresent() to wait for the present is completed. When IdleNotify event return this serial number, it means
// that usage of this image is completed by Xserver.
Result Dri3WindowSystem::Present(
    const PresentSwapChainInfo& presentInfo,
    PresentFence*               pRenderFence,
    PresentFence*               pIdleFence)
{
    Result                 result         = Result::Success;
    Dri3PresentFence*const pDri3WaitFence = static_cast<Dri3PresentFence*>(pRenderFence);
    Dri3PresentFence*const pDri3IdleFence = static_cast<Dri3PresentFence*>(pIdleFence);
    const xcb_sync_fence_t waitSyncFence  = (pDri3WaitFence != nullptr) ? pDri3WaitFence->SyncFence() : 0;
    const xcb_sync_fence_t idleSyncFence  = (pDri3IdleFence != nullptr) ? pDri3IdleFence->SyncFence() : 0;
    Image*                 pSrcImage      = static_cast<Image*>(presentInfo.pSrcImage);
    uint32                 pixmap         = pSrcImage->GetPresentImageHandle().hPixmap;
    PresentMode            presentMode    = presentInfo.presentMode;
    PAL_ASSERT((pDri3IdleFence == nullptr) || (pDri3IdleFence->QueryRaw() == Result::NotReady));

    if (m_device.Settings().forcePresentViaCpuBlt)
    {
        const SubResourceInfo* pSubresInfo = pSrcImage->SubresourceInfo(0);

        // The gpu memory size may be padded out; get the size without padding.
        PAL_ASSERT((pSubresInfo->bitsPerTexel % 8) == 0); // Does this even happen?
        const size_t bufferSize = pSubresInfo->extentTexels.width *
                                  pSubresInfo->extentTexels.height *
                                  pSubresInfo->bitsPerTexel / 8;

        // X11 only allows software presents from a linear image, which should have previously been
        // copied into this 'presentable buffer'.
        void* pPresentBuf = nullptr;

        if (result == Result::Success)
        {
            PAL_ASSERT(pSrcImage->GetPresentableBuffer() != nullptr);
            PAL_ASSERT(pSrcImage->GetPresentableBuffer()->Desc().size >= bufferSize);
            result = pSrcImage->GetPresentableBuffer()->Map(&pPresentBuf);
        }

        if (result == Result::Success)
        {
            // If soft present is enabled, the pixmap isn't really GPU memory and doesn't already have the image data

            // This essentially means we have three allocations:
            //   - The image's original memory in its swizzled form
            //   - The linear GPU memory that we previously converted to (updated in DoCpuPresentBlit)
            //   - The X11-managed CPU memory backing the pixmap (updated here)
            const xcb_void_cookie_t cpuBltCookie = m_dri3Procs.pfnXcbPutImageChecked(m_pConnection,
                    XCB_IMAGE_FORMAT_Z_PIXMAP, // Format is in, eg., RGBRGBRGB vs RRRGGGBBB
                    pixmap,
                    m_graphicsContext,
                    pSubresInfo->extentTexels.width,
                    pSubresInfo->extentTexels.height,
                    0, // dstX
                    0, // dstY
                    0, // leftPad
                    m_depth,
                    bufferSize,
                    static_cast<const uint8*>(pPresentBuf));

            xcb_generic_error_t*const pCpuBltError = m_dri3Procs.pfnXcbRequestCheck(m_pConnection, cpuBltCookie);
            if (pCpuBltError != nullptr)
            {
                PAL_ASSERT_ALWAYS();
                free(pCpuBltError);
                result = Result::ErrorUnknown;
            }
        }

        if (pPresentBuf != nullptr)
        {
            Result tmpResult = pSrcImage->GetPresentableBuffer()->Unmap();
            // If it fails to unmap, still succeed the whole present call
            PAL_ASSERT(tmpResult == Result::Success);
        }
    }

    if (result == Result::Success)
    {
        // The setting below means if XCB_PRESENT_OPTION_ASYNC is set, display the image immediately, otherwise display
        // the image on next vblank.
        uint64 targetMsc            = presentInfo.mscInfo.targetMsc;
        uint64 remainder            = presentInfo.mscInfo.remainder;
        uint64 divisor              = presentInfo.mscInfo.divisor;
        uint32 options = XCB_PRESENT_OPTION_NONE;

        if (presentMode == PresentMode::Windowed)
        {
            options |= XCB_PRESENT_OPTION_COPY;
        }
        // PresentOptionAsync: the present will be performed as soon as possible, not necessarily waiting for
        // next vertical blank interval
        if ((m_swapChainMode == SwapChainMode::Immediate) &&
            (targetMsc == 0))
        {
            options |= XCB_PRESENT_OPTION_ASYNC;
        }

        uint32 serial = m_localSerial + 1;
        xcb_void_cookie_t cookie = m_dri3Procs.pfnXcbPresentPixmapChecked(m_pConnection,
                                          m_hWindow,
                                          pixmap,
                                          serial,
                                          0,                              // valid-area
                                          0,                              // update-area
                                          0,                              // x-off
                                          0,                              // y-off
                                          0,                              // crtc
                                          waitSyncFence,                  // wait-fence
                                          idleSyncFence,                  // idle-fence
                                          options,
                                          targetMsc,
                                          divisor,
                                          remainder,
                                          0,                              // notifies_len
                                          nullptr);                       // notifies

        m_dri3Procs.pfnXcbDiscardReply(m_pConnection, cookie.sequence);

        m_localSerial = serial;

        if (pDri3IdleFence != nullptr)
        {
            pDri3IdleFence->SetPresented(true);
            pDri3IdleFence->AttachImage(pSrcImage);
        }

        pSrcImage->SetIdle(false); // From now on, the image/buffer is owned by Xorg

        m_dri3Procs.pfnXcbFlush(m_pConnection);

        if (m_swapChainMode != SwapChainMode::Immediate)
        {
            // For other modes like FIFO, handle events in the present thread only.
            GoThroughEvent();
        }
    }

    return result;
}

// =====================================================================================================================
// Handle the present event received from Xserver, so far just registered the present-complete event.
Result Dri3WindowSystem::HandlePresentEvent(
    xcb_present_generic_event_t* pPresentEvent,
    WindowSystemImageHandle*     pImage)
{
    Result result = Result::Success;

    switch (pPresentEvent->evtype)
    {
    case XCB_PRESENT_COMPLETE_NOTIFY:
    {
        m_remoteSerial = reinterpret_cast<xcb_present_complete_notify_event_t*>(pPresentEvent)->serial;

        Developer::PresentationModeData data = {};
        auto mode = (reinterpret_cast<xcb_present_complete_notify_event_t*>(pPresentEvent))->mode;

        if (mode == XCB_PRESENT_COMPLETE_MODE_FLIP)
        {
            data.presentationMode = Developer::PresentModeType::Flip;
        }
        else
        {
            data.presentationMode = Developer::PresentModeType::Composite;
        }

        m_device.DeveloperCb(Developer::CallbackType::PresentConcluded, &data);
        break;
    }

    case XCB_PRESENT_CONFIGURE_NOTIFY:
    {
        xcb_present_configure_notify_event_t *pConfig =
            reinterpret_cast<xcb_present_configure_notify_event_t*>(pPresentEvent);

        if (m_windowWidth != pConfig->width || m_windowHeight != pConfig->height)
        {
            m_needWindowSizeChangedCheck = true;

            m_windowWidth  = pConfig->width;
            m_windowHeight = pConfig->height;
        }

        break;
    }
    case  XCB_PRESENT_EVENT_IDLE_NOTIFY:
    {
        xcb_present_idle_notify_event_t *ie =
            reinterpret_cast<xcb_present_idle_notify_event_t*>(pPresentEvent);

        if (pImage)
        {
            pImage->hPixmap = ie->pixmap;
        }

        break;
    }
    default:
        result = Result::ErrorUnknown;
        break;
    }

    free(pPresentEvent);

    return result;
}

// =====================================================================================================================
// Wait for XServer present the last pixmap sent by Dri3WindowSystem::Present. Wait for the XCB_PRESENT_COMPLETE_NOTIFY
// event and compare the serial number to tell if the the pixmap is already presented by XServer.
Result Dri3WindowSystem::WaitForLastImagePresented()
{
    Result result     = Result::Success;
    uint32 lastSerial = m_localSerial;

    PAL_ASSERT(m_swapChainMode == SwapChainMode::Fifo);

    while ((lastSerial > m_remoteSerial) && (result == Result::Success))
    {
        m_dri3Procs.pfnXcbFlush(m_pConnection);

        xcb_present_generic_event_t*const pPresentEvent = reinterpret_cast<xcb_present_generic_event_t*>(
                m_dri3Procs.pfnXcbWaitForSpecialEvent(m_pConnection, m_pPresentEvent));

        if (pPresentEvent == nullptr)
        {
            result = Result::ErrorUnknown;
            break;
        }
        else
        {
            result = HandlePresentEvent(pPresentEvent, nullptr);
        }
    }

    return result;
}

// =====================================================================================================================
// Get the current width and height of the window from Xserver.
Result Dri3WindowSystem::GetWindowPropertiesXlib(
    Device*              pDevice,
    OsDisplayHandle      hDisplay,
    OsWindowHandle       hWindow,
    SwapChainProperties* pSwapChainProperties)
{
    xcb_connection_t*const pConnection =
        pDevice->GetPlatform()->GetDri3Loader().GetProcsTable().pfnXGetXCBConnection(static_cast<Display*>(hDisplay));

    return GetWindowProperties(pDevice, pConnection, hWindow, pSwapChainProperties);
}

// =====================================================================================================================
// Get the current width and height of the window from Xserver.
Result Dri3WindowSystem::GetWindowProperties(
    Device*              pDevice,
    OsDisplayHandle      hDisplay,
    OsWindowHandle       hWindow,
    SwapChainProperties* pSwapChainProperties)
{
    PAL_ASSERT(hWindow.win <= UINT_MAX);

    Result                          result      = Result::ErrorUnknown;

    const xcb_window_t              hXcbWindow  = static_cast<xcb_window_t>(hWindow.win);
    xcb_connection_t*const          pConnection = static_cast<xcb_connection_t*>(hDisplay);
    const Dri3LoaderFuncs&          dri3Procs   = pDevice->GetPlatform()->GetDri3Loader().GetProcsTable();
    const xcb_get_geometry_cookie_t cookie      = dri3Procs.pfnXcbGetGeometry(pConnection, hXcbWindow);
    xcb_get_geometry_reply_t*const  pReply      = dri3Procs.pfnXcbGetGeometryReply(pConnection, cookie, nullptr);

    // Set the alpha composite mode. Tell if opaque supported by alpha channel of X visual.
    if (IsAlphaSupported(pDevice, hDisplay, hWindow))
    {
        pSwapChainProperties->compositeAlphaMode = static_cast<uint32>(CompositeAlphaMode::Inherit) |
                                                   static_cast<uint32>(CompositeAlphaMode::PostMultiplied);
    }
    else
    {
        pSwapChainProperties->compositeAlphaMode = static_cast<uint32>(CompositeAlphaMode::Inherit) |
                                                   static_cast<uint32>(CompositeAlphaMode::Opaque);
    }

    pSwapChainProperties->minImageCount = 2;

    // XWayland is a transition from Xorg to Wayland, which has poor performance in fullscreen present
    // mode, so windowed mode is preferred on XWayland.
    pSwapChainProperties->preferredPresentModes =
        IsXWayland(hDisplay, pDevice) ?
        static_cast<uint32>(PreferredPresentModeFlags::PreferWindowedPresentMode) :
        static_cast<uint32>(PreferredPresentModeFlags::NoPreference);

    if (pReply != nullptr)
    {
        pSwapChainProperties->currentExtent.width  = pReply->width;
        pSwapChainProperties->currentExtent.height = pReply->height;
        result                                     = Result::Success;

        free(pReply);
    }

    return result;
}

// =====================================================================================================================
// Check if this is XWayland
bool Dri3WindowSystem::IsXWayland(
    OsDisplayHandle hDisplay,
    Device*         pDevice)
{
    bool result = false;
    const Dri3LoaderFuncs& dri3Procs   = pDevice->GetPlatform()->GetDri3Loader().GetProcsTable();
    xcb_connection_t*const pConnection = static_cast<xcb_connection_t*>(hDisplay);
    xcb_randr_query_version_cookie_t  versionCookie = dri3Procs.pfnXcbRandrQueryVersion(pConnection, 1, 3);
    xcb_randr_query_version_reply_t*  pVersionReply =
        dri3Procs.pfnXcbRandrQueryVersionReply(pConnection, versionCookie, nullptr);

    if ((pVersionReply == nullptr) ||
        ((pVersionReply->major_version == 1) && (pVersionReply->minor_version < 3)))
    {
        if (pVersionReply != nullptr)
        {
            free(pVersionReply);
        }
        result = false;
    }

    const xcb_setup_t*    pSetup = dri3Procs.pfnXcbGetSetup(pConnection);
    xcb_screen_iterator_t iter = dri3Procs.pfnXcbSetupRootsIterator(pSetup);
    xcb_randr_get_screen_resources_cookie_t scrResCookie =
        dri3Procs.pfnXcbRandrGetScreenResourcesCurrent(pConnection, iter.data->root);
    xcb_randr_get_screen_resources_reply_t* pScrResReply =
        dri3Procs.pfnXcbRandrGetScreenResourcesReply(pConnection, scrResCookie, nullptr);

    if ((pScrResReply != nullptr) && (pScrResReply->num_outputs > 0))
    {
        xcb_randr_output_t* pRandrOutput = dri3Procs.pfnXcbRandrGetScreenResourcesOutputs(pScrResReply);

        for (int32 i = 0; i < pScrResReply->num_outputs; i++)
        {
            xcb_randr_get_output_info_cookie_t outCookie =
                dri3Procs.pfnXcbRandrGetOutputInfo(pConnection, pRandrOutput[i], pScrResReply->config_timestamp);
            xcb_randr_get_output_info_reply_t* pOutReply =
                dri3Procs.pfnXcbRandrGetOutputInfoReply(pConnection, outCookie, nullptr);

            if (pOutReply != nullptr)
            {
                const char*const pName =
                    reinterpret_cast<const char*>(dri3Procs.pfnXcbRandrGetOutputInfoName(pOutReply));
                const int nameLength = dri3Procs.pfnXcbRandrGetOutputInfoNameLength(pOutReply);

                if ((pName != nullptr) && (strncmp(pName, "XWAYLAND", std::min(nameLength, 8)) == 0))
                {
                    result = true;
                }
                free(pOutReply);
            }
        }
        free(pScrResReply);

    }

    return result;
}

// =====================================================================================================================
bool Dri3WindowSystem::IsAlphaSupported(
    Device*             pDevice,
    OsDisplayHandle     hDisplay,
    OsWindowHandle      hWindow)
{
    PAL_ASSERT(hWindow.win <= UINT_MAX);

    const xcb_window_t      hXcbWindow  = static_cast<xcb_window_t>(hWindow.win);
    const Dri3LoaderFuncs&  dri3Procs   = pDevice->GetPlatform()->GetDri3Loader().GetProcsTable();
    xcb_connection_t*const  pConnection = static_cast<xcb_connection_t*>(hDisplay);
    xcb_screen_iterator_t   iter        = dri3Procs.pfnXcbSetupRootsIterator(dri3Procs.pfnXcbGetSetup(pConnection));
    xcb_query_tree_cookie_t treeCookie  = {};
    xcb_query_tree_reply_t* pTree       = nullptr;
    xcb_visualtype_t*       pVisualType = nullptr;
    uint32                  depth       = 0;
    bool                    hasAlpha    = false;

    xcb_get_window_attributes_cookie_t attribCookie = {};
    xcb_get_window_attributes_reply_t* pAttrib      = nullptr;

    bool queryTreeSupported = dri3Procs.pfnXcbQueryTreeisValid() &&
                              dri3Procs.pfnXcbQueryTreeReplyisValid() &&
                              dri3Procs.pfnXcbGetWindowAttributesisValid() &&
                              dri3Procs.pfnXcbGetWindowAttributesReplyisValid();

    if (queryTreeSupported)
    {
        treeCookie   = dri3Procs.pfnXcbQueryTree(pConnection, hXcbWindow);
        pTree        = dri3Procs.pfnXcbQueryTreeReply(pConnection, treeCookie, nullptr);
        attribCookie = dri3Procs.pfnXcbGetWindowAttributes(pConnection, hXcbWindow);
        pAttrib      = dri3Procs.pfnXcbGetWindowAttributesReply(pConnection, attribCookie, nullptr);
    }

    if ((pTree != nullptr) && (pAttrib != nullptr))
    {
        xcb_window_t   root     = pTree->root;
        xcb_visualid_t visualId = pAttrib->visual;

        while (iter.rem)
        {
            if (iter.data->root != root)
            {
                continue;
            }

            xcb_depth_iterator_t depthIter = dri3Procs.pfnXcbScreenAllowedDepthsIterator(iter.data);

            for (; depthIter.rem; dri3Procs.pfnXcbDepthNext(&depthIter))
            {
                xcb_visualtype_iterator_t visualIter = dri3Procs.pfnXcbDepthVisualsIterator(depthIter.data);

                for (; visualIter.rem; dri3Procs.pfnXcbVisualtypeNext(&visualIter))
                {
                    if (visualId == visualIter.data->visual_id)
                    {
                        pVisualType = visualIter.data;
                        depth       = depthIter.data->depth;
                        break;
                    }
                }

                if (pVisualType != nullptr)
                {
                    break;
                }
            }

            // Tell if the visualType contains alpha channel
            if (pVisualType != nullptr)
            {
                const uint32 rgbMask   = pVisualType->red_mask |  pVisualType->green_mask | pVisualType->blue_mask;
                const uint32 colorMask = 0xffffffff >> (32 - depth);

                hasAlpha = (colorMask & ~rgbMask) != 0;

                break;
            }

            dri3Procs.pfnXcbScreenNext(&iter);
        }
    }

    if (pTree != nullptr)
    {
        free(pTree);
    }

    if (pAttrib != nullptr)
    {
        free(pAttrib);
    }

    return hasAlpha;
}

// =====================================================================================================================
Result Dri3WindowSystem::DeterminePresentationSupportedXlib(
    Device*             pDevice,
    OsDisplayHandle     hDisplay,
    int64               visualId)
{
    const Dri3LoaderFuncs& dri3Procs = pDevice->GetPlatform()->GetDri3Loader().GetProcsTable();
    Display*const          pDisplay  = static_cast<Display*>(hDisplay);

    XVisualInfo visualInfo = {};
    visualInfo.visualid    = visualId;

    Result result = Result::Unsupported;
    int32  count  = 0;

    XVisualInfo*const pVisualList = dri3Procs.pfnXGetVisualInfo(pDisplay, VisualIDMask, &visualInfo, &count);

    // find the visual which means the visual is supported by current connection.
    if (count >= 1)
    {
        PAL_ASSERT((pVisualList[0].red_mask   == 0xff0000) &&
                   (pVisualList[0].green_mask == 0x00ff00) &&
                   (pVisualList[0].blue_mask  == 0x0000ff));

        result = Result::Success;

        dri3Procs.pfnXFree(pVisualList);
    }

    return result;
}
// =====================================================================================================================
Result Dri3WindowSystem::DeterminePresentationSupported(
    Device*             pDevice,
    OsDisplayHandle     hDisplay,
    int64               visualId)
{
    const Dri3LoaderFuncs& dri3Procs   = pDevice->GetPlatform()->GetDri3Loader().GetProcsTable();
    xcb_connection_t*const pConnection = static_cast<xcb_connection_t*>(hDisplay);
    xcb_visualtype_t*      pVisualType = nullptr;
    xcb_screen_iterator_t  iter        = dri3Procs.pfnXcbSetupRootsIterator(dri3Procs.pfnXcbGetSetup(pConnection));
    Result                 result      = Result::Unsupported;

    // Iterate over the screens of the connection to see whether we can find the required visual_id.
    while (iter.rem)
    {
        xcb_depth_iterator_t depthIter = dri3Procs.pfnXcbScreenAllowedDepthsIterator(iter.data);

        for (; depthIter.rem; dri3Procs.pfnXcbDepthNext(&depthIter))
        {
            xcb_visualtype_iterator_t visualIter = dri3Procs.pfnXcbDepthVisualsIterator(depthIter.data);

            for (; visualIter.rem; dri3Procs.pfnXcbVisualtypeNext(&visualIter))
            {
                if (visualId == visualIter.data->visual_id)
                {
                    pVisualType = visualIter.data;
                    break;
                }
            }

            if (pVisualType != nullptr)
            {
                break;
            }
        }

        if (pVisualType != nullptr)
        {
            // From the xcb's source code: the bits_per_rgb_value is per color channel but not per pixel.
            if (pVisualType->bits_per_rgb_value == 8)
            {
                PAL_ASSERT((pVisualType->red_mask   == 0xff0000) &&
                           (pVisualType->green_mask == 0x00ff00) &&
                           (pVisualType->blue_mask  == 0x0000ff));

                result = Result::Success;
                break;
            }
            else
            {
                PAL_NEVER_CALLED();
            }
        }

        dri3Procs.pfnXcbScreenNext(&iter);
    }

    return result;
}

// =====================================================================================================================
// Private help function to get the root window from output.
Result Dri3WindowSystem::GetRootWindowFromOutput(
    OsDisplayHandle hDisplay,
    Device*         pDevice,
    uint32          randrOutput,
    uint32*         pRootWindow)
{
    Result                 result      = Result::Success;
    const Dri3LoaderFuncs& dri3Procs   = pDevice->GetPlatform()->GetDri3Loader().GetProcsTable();
    xcb_connection_t*const pConnection = dri3Procs.pfnXGetXCBConnection(static_cast<Display*>(hDisplay));
    const xcb_setup_t*     pSetup      = dri3Procs.pfnXcbGetSetup(pConnection);

    *pRootWindow = 0;

    for (auto iter = dri3Procs.pfnXcbSetupRootsIterator(pSetup);
         (iter.rem > 0) && (result == Result::Success) && (*pRootWindow == 0);
         dri3Procs.pfnXcbScreenNext(&iter))
    {
        xcb_randr_get_screen_resources_cookie_t scrResCookie =
            dri3Procs.pfnXcbRandrGetScreenResources(pConnection, iter.data->root);

        xcb_randr_get_screen_resources_reply_t* pScrResReply =
            dri3Procs.pfnXcbRandrGetScreenResourcesReply(pConnection, scrResCookie, NULL);

        if (pScrResReply != nullptr)
        {
            xcb_randr_output_t* pRandrOutput = dri3Procs.pfnXcbRandrGetScreenResourcesOutputs(pScrResReply);

            for (int i = 0; i < pScrResReply->num_outputs; i++)
            {
                if (randrOutput == pRandrOutput[i])
                {
                    *pRootWindow = iter.data->root;

                    break;
                }
            }
            free(pScrResReply);
        }
        else
        {
            result = Result::ErrorInitializationFailed;
        }
    }

    return result;
}

// =====================================================================================================================
// Private help function to get the output from connector.
Result Dri3WindowSystem::GetOutputFromConnector(
    OsDisplayHandle hDisplay,
    Device*         pDevice,
    uint32          connector,
    uint32*         pOutput)
{
    Result                 result        = Result::Success;
    const Dri3LoaderFuncs& dri3Procs     = pDevice->GetPlatform()->GetDri3Loader().GetProcsTable();
    xcb_connection_t*const pConnection   = dri3Procs.pfnXGetXCBConnection(static_cast<Display*>(hDisplay));
    uint32                 randrOutput   = 0;
    const xcb_setup_t*     pSetup        = dri3Procs.pfnXcbGetSetup(pConnection);
    xcb_atom_t             connectorAtom = 0;

    xcb_intern_atom_cookie_t atomCookie  = dri3Procs.pfnXcbInternAtom(pConnection,
                                                                      true,
                                                                      12,
                                                                      "CONNECTOR_ID");
    xcb_intern_atom_reply_t *pAtomReply  = dri3Procs.pfnXcbInternAtomReply(pConnection,
                                                                           atomCookie,
                                                                           NULL);

    if (pAtomReply)
    {
        connectorAtom = pAtomReply->atom;
        free(pAtomReply);
    }
    else
    {
        result = Result::ErrorInitializationFailed;
    }

    for (auto iter = dri3Procs.pfnXcbSetupRootsIterator(pSetup);
         (iter.rem > 0) && (result == Result::Success) && (randrOutput == 0);
         dri3Procs.pfnXcbScreenNext(&iter))
    {
        uint32 connectorId = 0;

        xcb_randr_get_screen_resources_cookie_t scrResCookie =
            dri3Procs.pfnXcbRandrGetScreenResources(pConnection, iter.data->root);

        xcb_randr_get_screen_resources_reply_t* pScrResReply =
            dri3Procs.pfnXcbRandrGetScreenResourcesReply(pConnection, scrResCookie, NULL);

        if (pScrResReply != nullptr)
        {
            xcb_randr_output_t* pRandrOutput = dri3Procs.pfnXcbRandrGetScreenResourcesOutputs(pScrResReply);

            for (int i = 0; (i < pScrResReply->num_outputs) && (randrOutput == 0); i++)
            {
                xcb_randr_get_output_property_cookie_t outputPropertyCookie =
                    dri3Procs.pfnXcbRandrGetOutputProperty(pConnection,
                                                           pRandrOutput[i],
                                                           connectorAtom,
                                                           0,
                                                           0,
                                                           0xffffffffUL,
                                                           0,
                                                           0);
                xcb_randr_get_output_property_reply_t* pOutputPropertyReply =
                    dri3Procs.pfnXcbRandrGetOutputPropertyReply(pConnection, outputPropertyCookie, NULL);

                if (pOutputPropertyReply)
                {
                    if ((pOutputPropertyReply->num_items == 1) && (pOutputPropertyReply->format == PropSizeInBit))
                    {
                        memcpy(&connectorId, dri3Procs.pfnXcbRandrGetOutputPropertyData(pOutputPropertyReply), 4);
                        if (connectorId == connector)
                        {
                            randrOutput  = pRandrOutput[i];
                        }
                    }
                    free(pOutputPropertyReply);
                }
                else
                {
                    result = Result::ErrorInitializationFailed;
                }
            }
            free(pScrResReply);
        }
        else
        {
            result = Result::ErrorInitializationFailed;
        }
    }

    *pOutput = randrOutput;

    return result;
}

// =====================================================================================================================
// Find an usable crtc for a given output. If the output has an active crtc, we use that. Otherwise we pick one
// whose possible output list contains the given output.
Result Dri3WindowSystem::FindCrtcForOutput(
    OsDisplayHandle hDisplay,
    Device*         pDevice,
    uint32          randrOutput,
    uint32          rootWindow,
    uint32*         pRandrCrtc)
{
    Result                 result      = Result::Success;
    const Dri3LoaderFuncs& dri3Procs   = pDevice->GetPlatform()->GetDri3Loader().GetProcsTable();
    xcb_connection_t*const pConnection = dri3Procs.pfnXGetXCBConnection(static_cast<Display*>(hDisplay));

    *pRandrCrtc = 0;

    xcb_randr_get_screen_resources_cookie_t scrResCookie =
        dri3Procs.pfnXcbRandrGetScreenResources(pConnection, rootWindow);

    xcb_randr_get_screen_resources_reply_t* pScrResReply =
        dri3Procs.pfnXcbRandrGetScreenResourcesReply(pConnection, scrResCookie, NULL);

    if (pScrResReply == nullptr)
    {
        result = Result::ErrorInitializationFailed;
    }

    if (result == Result::Success)
    {
        xcb_randr_crtc_t* pCrtc      = dri3Procs.pfnXcbRandrGetScreenResourcesCrtcs(pScrResReply);
        uint32            activeCrtc = 0;
        uint32            freeCrtc   = 0;
        for (int i = 0; i < pScrResReply->num_crtcs; i++)
        {
            xcb_randr_get_crtc_info_cookie_t crtcInfoCookie =
                dri3Procs.pfnXcbRandrGetCrtcInfo(pConnection, pCrtc[i], pScrResReply->timestamp);
            xcb_randr_get_crtc_info_reply_t* pCrtcInfoReply =
                dri3Procs.pfnXcbRandrGetCrtcInfoReply(pConnection, crtcInfoCookie, NULL);
            if (pCrtcInfoReply == nullptr)
            {
                continue;
            }

            xcb_randr_output_t* pOutput =
                dri3Procs.pfnXcbRandrGetCrtcInfoOutputs(pCrtcInfoReply);
            if (pCrtcInfoReply->mode && (pCrtcInfoReply->num_outputs == 1) && (pOutput[0] == randrOutput))
            {
                // this crtc is currently in use by randrOutput
                activeCrtc = pCrtc[i];
                free(pCrtcInfoReply);
                break;
            }

            if (!pCrtcInfoReply->mode)
            {
                // this crtc is free, check if it can output to randrOutput.
                // even if this crtc is usable, we don't break the outer loop immediately, since there
                // might still be an active crtc we haven't seen.
                xcb_randr_output_t* pPossibleOutput =
                    dri3Procs.pfnXcbRandrGetCrtcInfoPossible(pCrtcInfoReply);
                for (int j = 0; j < pCrtcInfoReply->num_possible_outputs; j++)
                {
                    if (pPossibleOutput[j] == randrOutput)
                    {
                        freeCrtc = pCrtc[i];
                        break;
                    }
                }
            }
            free(pCrtcInfoReply);
        }

        free(pScrResReply);

        if (activeCrtc != 0)
        {
            *pRandrCrtc = activeCrtc;
        }
        else
        {
            if (freeCrtc != 0)
            {
                *pRandrCrtc = freeCrtc;
            }
            else
            {
                result = Result::ErrorInitializationFailed;
            }
        }
    }
    return result;
}

// =====================================================================================================================
// Acquires exclusive access to the display.
Result Dri3WindowSystem::AcquireScreenAccess(
    OsDisplayHandle hDisplay,
    Device*         pDevice,
    uint32          connector,
    uint32*         pRandrOutput,
    int32*          pDrmMasterFd)
{
    Result                 result      = Result::ErrorInitializationFailed;
    const Dri3LoaderFuncs& dri3Procs   = pDevice->GetPlatform()->GetDri3Loader().GetProcsTable();
    xcb_connection_t*const pConnection = dri3Procs.pfnXGetXCBConnection(static_cast<Display*>(hDisplay));
    uint32                 randrOutput = *pRandrOutput;
    uint32                 randrCrtc   = 0;
    uint32                 rootWindow  = 0;

#if XCB_RANDR_SUPPORTS_LEASE
    if (dri3Procs.pfnXcbRandrCreateLeaseisValid()      &&
        dri3Procs.pfnXcbRandrCreateLeaseReplyisValid() &&
        dri3Procs.pfnXcbRandrCreateLeaseReplyFdsisValid())
    {
        result = Result::Success;
    }

    // Check the version of randr, randr version >= 1.6 is required for Lease feature.
    if (result == Result::Success)
    {
        xcb_randr_query_version_cookie_t  versionCookie = dri3Procs.pfnXcbRandrQueryVersion(pConnection, 1, 6);
        xcb_randr_query_version_reply_t*  pVersionReply = dri3Procs.pfnXcbRandrQueryVersionReply(pConnection,
                                                                                                 versionCookie,
                                                                                                 NULL);
        if (pVersionReply == nullptr)
        {
            result = Result::ErrorInitializationFailed;
        }
        else
        {
            if (((pVersionReply->major_version == 1) && (pVersionReply->minor_version < 6)) ||
                (pVersionReply->major_version < 1))
            {
                result = Result::ErrorInitializationFailed;
            }
            free(pVersionReply);
        }
    }

    if (result == Result::Success)
    {
        if (randrOutput == 0)
        {
            result = GetOutputFromConnector(hDisplay, pDevice, connector, &randrOutput);
        }
    }

    if (result == Result::Success)
    {
        result = GetRootWindowFromOutput(hDisplay, pDevice, randrOutput, &rootWindow);
    }

    if (result == Result::Success)
    {
        result = FindCrtcForOutput(hDisplay, pDevice, randrOutput, rootWindow, &randrCrtc);
    }

    if (result == Result::Success)
    {
        xcb_randr_lease_t               lease       = dri3Procs.pfnXcbGenerateId(pConnection);
        xcb_randr_create_lease_cookie_t leaseCookie = dri3Procs.pfnXcbRandrCreateLease(pConnection,
                                                                                       rootWindow,
                                                                                       lease,
                                                                                       1,
                                                                                       1,
                                                                                       &randrCrtc,
                                                                                       &randrOutput);

        xcb_randr_create_lease_reply_t* pLeaseReply = dri3Procs.pfnXcbRandrCreateLeaseReply(pConnection,
                                                                                            leaseCookie,
                                                                                            NULL);

        if (pLeaseReply && (pLeaseReply->nfd > 0))
        {
            int* pLeaseReplyFds = dri3Procs.pfnXcbRandrCreateLeaseReplyFds(pConnection, pLeaseReply);

            *pDrmMasterFd = pLeaseReplyFds[0];

            free(pLeaseReply);
        }
        else
        {
            result = Result::ErrorInitializationFailed;
        }

    }

    if (result == Result::Success)
    {
        *pRandrOutput = randrOutput;
    }

#endif

    return result;
}

// Enable adaptive sync of the X window
void Dri3WindowSystem::SetAdaptiveSyncProperty(
    bool enable)
{
    const Dri3LoaderFuncs&         dri3Procs      = m_device.GetPlatform()->GetDri3Loader().GetProcsTable();
    constexpr const char           propertyName[] = "_VARIABLE_REFRESH";
    const xcb_intern_atom_cookie_t cookie         = dri3Procs.pfnXcbInternAtom(m_pConnection,
                                                                               0,
                                                                               strlen(propertyName),
                                                                               propertyName);
    xcb_intern_atom_reply_t*const  pReply         = dri3Procs.pfnXcbInternAtomReply(m_pConnection,
                                                                                    cookie,
                                                                                    nullptr);

    if (pReply != nullptr)
    {
        xcb_void_cookie_t check = {};

        if (enable)
        {
            uint32 state = 1;
            check = dri3Procs.pfnXcbChangePropertyChecked(m_pConnection,
                                                          XCB_PROP_MODE_REPLACE,
                                                          m_hWindow,
                                                          pReply->atom,
                                                          XCB_ATOM_CARDINAL,
                                                          32,
                                                          1,
                                                          &state);
        }
        else
        {
            check = dri3Procs.pfnXcbDeletePropertyChecked(m_pConnection,
                                                          m_hWindow,
                                                          pReply->atom);
        }

        dri3Procs.pfnXcbDiscardReply(m_pConnection, check.sequence);

        free(pReply);
    }

}

// =====================================================================================================================
// Go through all existing events
void Dri3WindowSystem::GoThroughEvent()
{
    xcb_generic_event_t* pEvent = nullptr;

    while ((pEvent = m_dri3Procs.pfnXcbPollForSpecialEvent(m_pConnection, m_pPresentEvent)) != nullptr)
    {
        HandlePresentEvent(reinterpret_cast<xcb_present_generic_event_t*>(pEvent), nullptr);
    }
}

// =====================================================================================================================
// Check whether the idle image is the one attched to the fence
bool Dri3WindowSystem::CheckIdleImage(
    WindowSystemImageHandle* pIdleImage,
    PresentFence*            pFence)
{
    Dri3PresentFence* pDri3Fence = reinterpret_cast<Dri3PresentFence*>(pFence);
    bool ret = false;

    if (pIdleImage->hPixmap == pDri3Fence->GetImage()->GetPresentImageHandle().hPixmap)
    {
        pDri3Fence->GetImage()->SetIdle(true);
        ret = true;
    }

    return ret;
}

} // Amdgpu
} // Pal
