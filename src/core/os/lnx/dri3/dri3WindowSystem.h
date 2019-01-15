/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/os/lnx/dri3/dri3Loader.h"
#include "core/os/lnx/lnxWindowSystem.h"
#include <xcb/xcb.h>
#include <xcb/present.h>

namespace Pal
{
namespace Linux
{

class Device;
class Dri3WindowSystem;

// =====================================================================================================================
// The definition of Present fence for Dri3 platform. The syncFence/pShmFence members refer to the idle-fence, which
// will be signaled by XServer when present is done or discarded.
class Dri3PresentFence : public PresentFence
{
public:
    static size_t GetSize() { return sizeof(Dri3PresentFence); }

    static Result Create(
        const Dri3WindowSystem& windowSystem,
        bool                    initiallySignaled,
        void*                   pPlacementAddr,
        PresentFence**          ppPresentFence);

    virtual void Reset() override;

    virtual Result Trigger() override;

    virtual Result WaitForCompletion(bool doWait) override;

    void SetPresented(bool set) { m_presented = set; }

    xcb_sync_fence_t  SyncFence() const { return m_syncFence; }
    struct xshmfence* ShmFence()  const { return m_pShmFence; }

private:
    Dri3PresentFence(const Dri3WindowSystem& windowSystem);
    virtual ~Dri3PresentFence();

    Result Init(bool initiallySignaled);

    const Dri3WindowSystem& m_windowSystem;
    xcb_sync_fence_t        m_syncFence;
    struct xshmfence*       m_pShmFence;
    bool                    m_presented;

    PAL_DISALLOW_DEFAULT_CTOR(Dri3PresentFence);
    PAL_DISALLOW_COPY_AND_ASSIGN(Dri3PresentFence);
};

// =====================================================================================================================
// Represent a window system with DRI3 extension. Responsibilities include setting up the DRI3 connection with X server,
// creating presentable pixmaps, asking X Server to present a pixmap with DRI3 extension, and waiting for X server to
// complete presents.
class Dri3WindowSystem : public WindowSystem
{
public:
    // The WindowSystem class is designed to be placed into other PAL objects which requires the Create/Destroy pattern.
    static size_t GetSize() { return sizeof(Dri3WindowSystem); }

    static Result Create(
        const Device&                 device,
        const WindowSystemCreateInfo& createInfo,
        void*                         pPlacementAddr,
        WindowSystem**                ppWindowSystem);

    // Helper functions to describe the properties of a window system we will create in the future.
    static Result GetWindowGeometry(
        Device*             pDevice,
        OsDisplayHandle     hDisplay,
        OsWindowHandle      hWindow,
        Extent2d*           pExtents);

    static Result GetWindowGeometryXlib(
        Device*             pDevice,
        OsDisplayHandle     hDisplay,
        OsWindowHandle      hWindow,
        Extent2d*           pExtents);

    static Result DeterminePresentationSupported(
        Device*             pDevice,
        OsDisplayHandle     hDisplay,
        int64               visualId);

    static Result DeterminePresentationSupportedXlib(
        Device*             pDevice,
        OsDisplayHandle     hDisplay,
        int64               visualId);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 447
    static Result GetConnectorIdFromOutput(
        Device*         pDevice,
        OsDisplayHandle hDisplay,
        uint32          randrOutput,
        uint32*         pConnectorId);
#endif

    static Result AcquireScreenAccess(
        OsDisplayHandle hDisplay,
        Device*         pDevice,
        uint32          connector,
        uint32*         pRandrOutput,
        int32*          pDrmMasterFd);

    static Result GetOutputFromConnector(
        OsDisplayHandle hDisplay,
        Device*         pDevice,
        uint32          connector,
        uint32*         pOutput);

    virtual Result CreatePresentableImage(
        SwapChain* pSwapChain,
        Image*     pImage,
        int32      sharedBufferFd) override;

    virtual void DestroyPresentableImage(
        WindowSystemImageHandle hImage) override;

    virtual Result Present(
        const PresentSwapChainInfo& presentInfo,
        PresentFence*               pRenderFence,
        PresentFence*               pIdleFence) override;

    virtual Result WaitForLastImagePresented() override;

private:
    Dri3WindowSystem(const Device& device, const WindowSystemCreateInfo& createInfo);
    virtual ~Dri3WindowSystem();

    Result Init();

    bool IsFormatPresentable(SwizzledFormat format);
    bool IsExtensionSupported();

    int32 OpenDri3();
    Result QueryVersion();
    Result SelectEvent();

    Result HandlePresentEvent(xcb_present_generic_event_t* pPresentEvent);

    static Result GetRootWindowFromOutput(
        OsDisplayHandle hDisplay,
        Device*         pDevice,
        uint32          randrOutput,
        uint32*         pRootWindow);

    const Device&          m_device;
    const Dri3Loader&      m_dri3Loader;
#if defined(PAL_DEBUG_PRINTS)
    const Dri3LoaderFuncsProxy& m_dri3Procs;
#else
    const Dri3LoaderFuncs&      m_dri3Procs;
#endif
    const SwizzledFormat   m_format;              // format for presentable image
    const SwapChainMode    m_swapChainMode;       // swapchain mode
    const xcb_window_t     m_hWindow;             // xcb window created by App
    xcb_connection_t*      m_pConnection;         // xcb connection created by App
    bool                   m_dri2Supported;
    int32                  m_dri3MajorVersion;
    int32                  m_dri3MinorVersion;
    int32                  m_presentMajorVersion;
    int32                  m_presentMinorVersion;
    xcb_special_event_t*   m_pPresentEvent;       // An event used to poll special present events from Xserver,
                                                  // e.g. the "XCB_PRESENT_COMPLETE_NOTIFY" event.
    uint32                 m_localSerial;         // Latest local present serial number that was sent to Xserver.
    uint32                 m_remoteSerial;        // The serial number of the latest present completed by Xserver.

    // The DRI3 present fence is tightly coupled to its windowing system. We declare it as a friend to make it easy to
    // call DRI3 functions within the fence.
    friend Dri3PresentFence;

    PAL_DISALLOW_DEFAULT_CTOR(Dri3WindowSystem);
    PAL_DISALLOW_COPY_AND_ASSIGN(Dri3WindowSystem);
};

} // Linux
} // Pal
