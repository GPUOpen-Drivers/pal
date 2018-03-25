/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/os/lnx/dri3/dri3WindowSystem.h"
#include "core/os/lnx/lnxDevice.h"
#include "core/os/lnx/lnxImage.h"
#include "core/os/lnx/lnxPlatform.h"
#include "palSwapChain.h"
#include "util/lnx/lnxTimeout.h"
extern "C"
{
#include <X11/xshmfence.h>
}

using namespace Util;

namespace Pal
{
namespace Linux
{

constexpr uint32 InvalidPixmapId = -1;

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
    m_presented(false)
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

    return result;
}

// =====================================================================================================================
void Dri3PresentFence::Reset()
{
    PAL_ASSERT(m_pShmFence != nullptr);

    m_windowSystem.m_dri3Procs.pfnXshmfenceReset(m_pShmFence);
    m_presented = false;
}

// =====================================================================================================================
// Trigger the SyncFence object.
Result Dri3PresentFence::Trigger()
{
    PAL_ASSERT(m_syncFence != 0);

    const xcb_void_cookie_t cookie =
        m_windowSystem.m_dri3Procs.pfnXcbSyncTriggerFenceChecked(m_windowSystem.m_pConnection, m_syncFence);

    xcb_generic_error_t*const pError =
        m_windowSystem.m_dri3Procs.pfnXcbRequestCheck(m_windowSystem.m_pConnection, cookie);

    Result result = Result::Success;

    if (pError != nullptr)
    {
        free(pError);
        result = Result::ErrorUnknown;
    }
    else
    {
        m_presented = true;
    }

    return result;
}

// =====================================================================================================================
// Wait for the idle fence to be signaled which indicates that the pixmap is not being used by XServer anymore.
Result Dri3PresentFence::WaitForCompletion(
    bool doWait)
{
    Result          result   = Result::Success;

    if (m_presented == false)
    {
        result = Result::ErrorFenceNeverSubmitted;
    }

    if (result == Result::Success)
    {
        if (doWait)
        {
            if (m_windowSystem.m_dri3Procs.pfnXshmfenceAwait(m_pShmFence) != 0)
            {
                result = Result::ErrorUnknown;
            }
        }
        else
        {
            if (m_windowSystem.m_dri3Procs.pfnXshmfenceQuery(m_pShmFence) == 0)
            {
                result = Result::NotReady;
            }
        }
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
    m_swapChainMode(createInfo.swapChainMode),
    m_hWindow(static_cast<xcb_window_t>(createInfo.hWindow)),
    m_pConnection(nullptr),
    m_dri2Supported(true),
    m_dri3MajorVersion(0),
    m_dri3MinorVersion(0),
    m_presentMajorVersion(0),
    m_presentMinorVersion(0),
    m_pPresentEvent(nullptr),
    m_localSerial(1),
    m_remoteSerial(0)
{
    PAL_ASSERT(createInfo.hDisplay != nullptr);

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
    int32  fd     = InvalidFd;

    if (m_pConnection != nullptr)
    {
        if (IsExtensionSupported() == false)
        {
            result = Result::ErrorInitializationFailed;
        }

        if (result == Result::Success)
        {
            fd = OpenDri3();
            if (fd == InvalidFd)
            {
                result = Result::ErrorInitializationFailed;
            }
        }

        if (result == Result::Success)
        {
            bool isSameGpu = false;
            result = m_device.IsSameGpu(fd, &isSameGpu);

            if (result == Result::Success)
            {
                if (isSameGpu == false)
                {
                    result = Result::ErrorInitializationFailed;
                }
            }

            // The X Server's file descriptor is closed here. For KMD interface access,
            // the fd stored in device, which is only for rendering, will be used.
            close(fd);
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
            // for non-fifo mode, we rely on the idle fence and should not wait for the complete event.
            if ((m_swapChainMode == SwapChainMode::Fifo) &&
                (result == Result::Success))
            {
                result = SelectEvent();
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
        result = false;
    }

    if (result == true)
    {
        pReply = m_dri3Procs.pfnXcbGetExtensionData(m_pConnection, m_dri3Loader.GetXcbPresentId());
        if ((pReply == nullptr) || (pReply->present == 0))
        {
            result = false;
        }
    }

    return result;
}

// =====================================================================================================================
// Send DRI3-Open request to Xserver to get the related GPU file descriptor.
int32 Dri3WindowSystem::OpenDri3()
{
    int32                        fd       = InvalidFd;
    const xcb_randr_provider_t   provider = 0;
    const xcb_dri3_open_cookie_t cookie   = m_dri3Procs.pfnXcbDri3Open(m_pConnection, m_hWindow, provider);
    xcb_dri3_open_reply_t*const  pReply   = m_dri3Procs.pfnXcbDri3OpenReply(m_pConnection, cookie, NULL);
    int32                        driverNameLength;

    m_windowSystemProperties.supportFreeSyncExtension = 0;

    if (pReply != nullptr)
    {
        fd = (pReply->nfd == 1) ? m_dri3Procs.pfnXcbDri3OpenReplyFds(m_pConnection, pReply)[0] : InvalidFd;
        free(pReply);
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

    return fd;
}

// =====================================================================================================================
// Query DRI3 and Present extension versions
Result Dri3WindowSystem::QueryVersion()
{
    Result result = Result::Success;

    const xcb_dri3_query_version_cookie_t dri3Cookie =
        m_dri3Procs.pfnXcbDri3QueryVersion(m_pConnection, XCB_DRI3_MAJOR_VERSION, XCB_DRI3_MINOR_VERSION);

    const xcb_present_query_version_cookie_t presentCookie =
        m_dri3Procs.pfnXcbPresentQueryVersion(m_pConnection, XCB_PRESENT_MAJOR_VERSION, XCB_PRESENT_MINOR_VERSION);

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

    if (result == Result::Success)
    {
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
                                                    XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY);

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
            m_dri3Procs.pfnXcbUnregisterForSpecialEvent(m_pConnection, m_pPresentEvent);
        }
        result = Result::ErrorUnknown;
    }

    return result;
}

// =====================================================================================================================
// Interface for the window system to do things related with creating presentable image. For XCB/Dri3 backend, it gets
// a pixmap packaging memory the image binds from Xserver. Then this pixmap can be presented by Xserver.
Result Dri3WindowSystem::CreatePresentableImage(
    const Image&   image,
    int32          sharedBufferFd, // Xserver can use the shared buffer (created in client side) by sharedBufferFd
    uint32*        pPresentImage)
{
    Result       result     = Result::Success;
    xcb_pixmap_t pixmap     = InvalidPixmapId;
    uint32       depth      = 0;

    const SubresId              subres      = { ImageAspect::Color, 0, 0 };
    const SubResourceInfo*const pSubResInfo = image.SubresourceInfo(subres);

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
            depth = pReply->depth;

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
        const xcb_void_cookie_t cookie = m_dri3Procs.pfnXcbDri3PixmapFromBufferChecked(m_pConnection,
                                                                                       pixmap,
                                                                                       m_hWindow,
                                                                                       size,
                                                                                       width,
                                                                                       height,
                                                                                       stride,
                                                                                       depth,
                                                                                       bpp,
                                                                                       sharedBufferFd);

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
        *pPresentImage = pixmap;
    }

    return result;
}

// =====================================================================================================================
// Destroy the present image.
// the present image is pixmap in dri3 platform.
void Dri3WindowSystem::DestroyPresentableImage(
    uint32 image)
{
    const xcb_void_cookie_t   cookie = m_dri3Procs.pfnXcbFreePixmapChecked(m_pConnection, image);
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
    uint32             pixmap,
    PresentMode        presentMode,
    PresentFence*      pRenderFence,
    PresentFence*      pIdleFence)
{
    Result                 result         = Result::Success;
    Dri3PresentFence*const pDri3WaitFence = static_cast<Dri3PresentFence*>(pRenderFence);
    Dri3PresentFence*const pDri3IdleFence = static_cast<Dri3PresentFence*>(pIdleFence);
    const xcb_sync_fence_t waitSyncFence  = (pDri3WaitFence != nullptr) ? pDri3WaitFence->SyncFence() : 0;
    const xcb_sync_fence_t idleSyncFence  = (pDri3IdleFence != nullptr) ? pDri3IdleFence->SyncFence() : 0;

    PAL_ASSERT((pDri3IdleFence == nullptr) || (m_dri3Procs.pfnXshmfenceQuery(pDri3IdleFence->ShmFence()) == 0));

    // The setting below means if XCB_PRESENT_OPTION_ASYNC is set, display the image immediately, otherwise display
    // the image on next vblank.
    constexpr uint32 TargetMsc  = 0;
    constexpr uint32 Remainder  = 0;
    constexpr uint32 Divisor    = 1;

    uint32 options = XCB_PRESENT_OPTION_NONE;

    if (presentMode == PresentMode::Windowed)
    {
        options |= XCB_PRESENT_OPTION_COPY;
    }
    // PresentOptionAsync: the present will be performed as soon as possible, not necessarily waiting for
    // next vertical blank interval
    if (m_swapChainMode == SwapChainMode::Immediate)
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
                                      TargetMsc,
                                      Divisor,
                                      Remainder,
                                      0,                              // notifies_len
                                      nullptr);                       // notifies

    xcb_generic_error_t*const pError = m_dri3Procs.pfnXcbRequestCheck(m_pConnection, cookie);

    if (pError != nullptr)
    {
        free(pError);
        result = Result::ErrorUnknown;
    }
    else
    {
        m_localSerial = serial;

        if (pDri3IdleFence != nullptr)
        {
            pDri3IdleFence->SetPresented(true);
        }
    }

    m_device.DeveloperCb(Developer::CallbackType::PresentConcluded, nullptr);

    return result;
}

// =====================================================================================================================
// Handle the present event received from Xserver, so far just registered the present-complete event.
Result Dri3WindowSystem::HandlePresentEvent(
    xcb_present_generic_event_t* pPresentEvent)
{
    Result result = Result::Success;

    switch (pPresentEvent->evtype)
    {
    case XCB_PRESENT_COMPLETE_NOTIFY:
        m_remoteSerial = reinterpret_cast<xcb_present_complete_notify_event_t*>(pPresentEvent)->serial;
        break;
    default:
        result = Result::ErrorUnknown;
        break;
    }

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
            result = HandlePresentEvent(pPresentEvent);
            free(pPresentEvent);
        }
    }

    return result;
}

// =====================================================================================================================
// Get the current width and height of the window from Xserver.
Result Dri3WindowSystem::GetWindowGeometryXlib(
    Device*             pDevice,
    OsDisplayHandle     hDisplay,
    OsWindowHandle      hWindow,
    Extent2d*           pExtents)
{
    xcb_connection_t*const pConnection =
        pDevice->GetPlatform()->GetDri3Loader().GetProcsTable().pfnXGetXCBConnection(static_cast<Display*>(hDisplay));

    return GetWindowGeometry(pDevice, pConnection, hWindow, pExtents);
}

// =====================================================================================================================
// Get the current width and height of the window from Xserver.
Result Dri3WindowSystem::GetWindowGeometry(
    Device*             pDevice,
    OsDisplayHandle     hDisplay,
    OsWindowHandle      hWindow,
    Extent2d*           pExtents)
{
    Result                          result      = Result::ErrorUnknown;
    const xcb_window_t              hXcbWindow  = static_cast<xcb_window_t>(hWindow);
    xcb_connection_t*const          pConnection = static_cast<xcb_connection_t*>(hDisplay);
    const Dri3LoaderFuncs&          dri3Procs   = pDevice->GetPlatform()->GetDri3Loader().GetProcsTable();
    const xcb_get_geometry_cookie_t cookie      = dri3Procs.pfnXcbGetGeometry(pConnection, hXcbWindow);
    xcb_get_geometry_reply_t*const  pReply      = dri3Procs.pfnXcbGetGeometryReply(pConnection, cookie, nullptr);

    if (pReply != nullptr)
    {
        pExtents->width  = pReply->width;
        pExtents->height = pReply->height;
        result           = Result::Success;

        free(pReply);
    }

    return result;
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

        dri3Procs.pfnXFree(pVisualList);
        result = Result::Success;
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

    // Record the screen index where we find the matching visual_id.
    uint32_t screenIndex = 0;

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
            break;
        }
        else
        {
            dri3Procs.pfnXcbScreenNext(&iter);
            screenIndex++;
        }
    }

    Result result = Result::Unsupported;

    if (pVisualType != nullptr)
    {
        // From the xcb's source code: the bits_per_rgb_value is per color channel but not per pixel.
        if (pVisualType->bits_per_rgb_value == 8)
        {
            PAL_ASSERT((pVisualType->red_mask   == 0xff0000) &&
                       (pVisualType->green_mask == 0x00ff00) &&
                       (pVisualType->blue_mask  == 0x0000ff));
            result = Result::Success;
        }
        else
        {
            PAL_NEVER_CALLED();
        }
    }

    return result;
}

} // Linux
} // Pal
