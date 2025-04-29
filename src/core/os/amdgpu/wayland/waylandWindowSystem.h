/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuImage.h"
#include "core/os/amdgpu/amdgpuWindowSystem.h"
#include "core/os/amdgpu/wayland/g_waylandLoader.h"
#include "palFile.h"
#include "palHashSet.h"

namespace Pal
{
namespace Amdgpu
{

class Device;
class WaylandWindowSystem;

// =====================================================================================================================
// The definition of present fence for Wayland platform.
// There is no present fence on Wayland platform yet, a listener function registered will be called once the present
// buffer is released by Wayland Server. Then the m_idle will be set true to indicate that the present fence
// of the image is signaled.

// Defined by the zwp_linux_dmabuf_feedback_v1.format_table, the format table provided by the Compositor will be a list
// of these structures
struct ZwpDmaBufFormat
{
    uint32 format;
    uint32 padding;
    uint64 modifier;
};
// Sanity check for compatibility between wl_drm and drm_fourcc format codes
static_assert((WL_DRM_FORMAT_ARGB8888    == DRM_FORMAT_ARGB8888)      &&
              (WL_DRM_FORMAT_XRGB8888    == DRM_FORMAT_XRGB8888)      &&
              (WL_DRM_FORMAT_ABGR8888    == DRM_FORMAT_ABGR8888)      &&
              (WL_DRM_FORMAT_XBGR8888    == DRM_FORMAT_XBGR8888)      &&
              (WL_DRM_FORMAT_ARGB2101010 == DRM_FORMAT_ARGB2101010)   &&
              (WL_DRM_FORMAT_XRGB2101010 == DRM_FORMAT_XRGB2101010)   &&
              (WL_DRM_FORMAT_ABGR2101010 == DRM_FORMAT_ABGR2101010)   &&
              (WL_DRM_FORMAT_XBGR2101010 == DRM_FORMAT_XBGR2101010)   &&
              (WL_DRM_FORMAT_RGB565      == DRM_FORMAT_RGB565)        &&
              (WL_DRM_FORMAT_BGR565      == DRM_FORMAT_BGR565)        &&
              (WL_DRM_FORMAT_ABGR16F     == DRM_FORMAT_ABGR16161616F) &&
              (WL_DRM_FORMAT_XBGR16F     == DRM_FORMAT_XBGR16161616F),
              "Mismatch between Linux DRM format codes and Wayland DRM format codes!");

struct WlFormatTable
{
    uint32 size;
    ZwpDmaBufFormat* pData;
};

class WaylandPresentFence final : public PresentFence
{
public:
    static size_t GetSize() { return sizeof(WaylandPresentFence); }

    static Result Create(
        const WaylandWindowSystem& windowSystem,
        bool                       initiallySignaled,
        void*                      pPlacementAddr,
        PresentFence**             ppPresentFence);

    virtual void   Reset()                        override;
    virtual Result Trigger()                      override;
    virtual Result WaitForCompletion(bool doWait) override;
    virtual Result AssociatePriorRenderFence(IQueue* pQueue) override { return Result::Success; }
    virtual ExplicitSyncData* GetExplicitSyncData() override { return &m_explicitSyncData; }
    void    AssociateImage(Image* pImage) { m_pImage = pImage; }
    virtual bool   IsIdle()                       override;

private:
    explicit WaylandPresentFence(const WaylandWindowSystem& windowSystem);
    virtual ~WaylandPresentFence();

    Result Init();

    Result InitExplicitSyncData();
    Result WaitForCompletionImplicitSync(bool doWait);
    Result WaitForCompletionExplicitSync(bool doWait);

    const WaylandWindowSystem& m_windowSystem;
    Image*                     m_pImage;

    ExplicitSyncData           m_explicitSyncData; // Acquire and release sync objects
                                                   // for presentable images

    PAL_DISALLOW_DEFAULT_CTOR(WaylandPresentFence);
    PAL_DISALLOW_COPY_AND_ASSIGN(WaylandPresentFence);
};

// =====================================================================================================================
// WaylandWindowSystem is responsible for creating Wayland presentable image, presenting image to Wayland server,
// waiting for Wayland server present done, etc.
class WaylandWindowSystem final : public WindowSystem
{
public:
    static size_t GetSize() { return sizeof(WaylandWindowSystem); }

    static Result Create(
        const Device&                 device,
        const WindowSystemCreateInfo& createInfo,
        void*                         pPlacementAddr,
        WindowSystem**                ppWindowSystem);

    static Result GetWindowProperties(
        Device*              pDevice,
        OsDisplayHandle      hDisplay,
        OsWindowHandle       hWindow,
        SwapChainProperties* pSwapChainProperties);

    static Result DeterminePresentationSupported(
        Device*             pDevice,
        OsDisplayHandle     hDisplay,
        int64               visualId);

    virtual Result CreatePresentableImage(
        SwapChain* pSwapChain,
        Image*     pImage,
        int32      sharedBufferFd) override;

    virtual void DestroyPresentableImage(
        WindowSystemImageHandle hImage) override;

    virtual Result Present(
        const PresentSwapChainInfo& presentInfo,
        PresentFence*               pRenderFence,
        PresentFence*               pIdleFence,
        IQueue*                     pQueue) override;

    virtual Result WaitForLastImagePresented() override;

    virtual bool NeedWindowSizeChangedCheck() const override { return false; }

    virtual bool SupportWaitingOnCompletion() const override { return !m_windowSystemProperties.useExplicitSync; }

    virtual void WaitOnCompletion() override;

    const WaylandLoader&      GetWaylandLoader() const                            { return m_waylandLoader; }
#if defined(PAL_DEBUG_PRINTS)
    const WaylandLoaderFuncsProxy& GetWaylandProcs()  const                       { return m_waylandProcs; }
#else
    const WaylandLoaderFuncs& GetWaylandProcs()  const                            { return m_waylandProcs; }
#endif
    wl_display*               GetDisplay() const                                  { return m_pDisplay; }
    wl_event_queue*           GetEventQueue() const                               { return m_pEventQueue; }
    wl_surface*               GetSurfaceWrapper() const                           { return m_pSurfaceWrapper; }
    WlFormatTable&            GetGlobalFormatTable()                              { return m_globalFormatTable; }
    zwp_linux_dmabuf_feedback_v1* GetDefaultFeedback()                            { return m_pDefaultDmaBuffFeedback; }
    bool                      UseZwpDmaBufProtocol() const                        { return m_useZwpDmaBufProtocol; }

    bool                      IsSupportedFormat(uint32 fmt);

    bool                      GetFrameCompleted() const                           { return m_frameCompleted; }

    void                      SetWaylandDrm(wl_drm* pWaylandDrm)                  { m_pWaylandDrm = pWaylandDrm; }
    void                      SetDmaBuffer(zwp_linux_dmabuf_v1* pDmaBuf)          { m_pDmaBuffer   = pDmaBuf; }

    void                      SetCapabilities(uint32 capibilities)                { m_capabilities = capibilities; }
    void                      SetFrameCompleted()                                 { m_frameCompleted = true; }
    void                      SetFrameCallback(wl_callback* pFrameCallback)       { m_pFrameCallback = pFrameCallback; }
    void                      SetZwpDmaBufProtocolUsage(bool inUse)               { m_useZwpDmaBufProtocol = inUse; }
    void                      SetDmaDevice(const wl_array* device) { memcpy(&m_DmaDevice, device->data, device->size); }
    void                      SetDeviceName(const char* pName)   { Util::Strncpy(m_deviceName, pName, MaxNodeNameLen); }
    void                      ConfigPresentOnSameGpu();

    Result                    FinishInit();
    Result                    FinishWlDrmInit();
    Result                    FinishZwpDmaBufInit();
    void                      CleanupExcessInit();

    Result                    CreateWlBuffer(uint32      width,
                                             uint32      height,
                                             uint32      stride,
                                             uint32      format,
                                             uint32      flags,
                                             int32       sharedBufferFd,
                                             wl_buffer** ppWlBuf);

    Result                    AddFormat(ZwpDmaBufFormat dmaFmt);
    Result                    AddFormat(uint32 wlDrmFormat);

    bool                      IsExplicitSyncEnabled() const;
    void                      SetSyncObjManager(wp_linux_drm_syncobj_manager_v1* pSyncObjManager);
    Result                    InitExplicitSyncObject(ExplicitSyncObject* pSyncObject) const override;
    void                      DestroyExplicitSyncObject(ExplicitSyncObject* pSyncObject) const override;

private:
    WaylandWindowSystem(const Device& device, const WindowSystemCreateInfo& createInfo);
    virtual ~WaylandWindowSystem();

    Result Init();

    wl_drm*                           m_pWaylandDrm;
    wl_display*                       m_pDisplay;
    wl_surface*                       m_pSurface;
    dev_t                             m_DmaDevice;
    zwp_linux_dmabuf_v1*              m_pDmaBuffer;
    zwp_linux_dmabuf_feedback_v1*     m_pDefaultDmaBuffFeedback;
    WlFormatTable                     m_globalFormatTable;

    // Explicit sync related variables
    wp_linux_drm_syncobj_manager_v1*  m_pSyncObjManager;    // Root explicit sync object
    wp_linux_drm_syncobj_surface_v1*  m_pSyncObjSurface;    // Wayland surface explicit sync extension

    const WaylandLoader&              m_waylandLoader;
#if defined(PAL_DEBUG_PRINTS)
    const WaylandLoaderFuncsProxy&    m_waylandProcs;
#else
    const WaylandLoaderFuncs&         m_waylandProcs;
#endif

    // This hashmap contains the advertized supported formats. The compositor also advertises modifiers for to
    // facilitate displaying compressed or tiled surfaces. For now, the supported dsiplay modes are linear formats, so
    // we can ignore them
    HashSet<uint32, Platform> m_validFormats;

    // PAL present buffers with multi-threads (two threads, one is main thread and another is present thread), in order
    // to avoid dead lock with shared queue, two queues are introduced for buffer idle and frame complete respectively.
    // m_pSurfaceEventQueue is for frame complete only, and other events, including buffer idle, will be dispatched to
    // m_pEventQueue.
    wl_event_queue*                   m_pEventQueue;
    wl_event_queue*                   m_pSurfaceEventQueue;

    wl_drm*                           m_pWaylandDrmWrapper;
    wl_display*                       m_pDisplayWrapper;
    wl_surface*                       m_pSurfaceWrapper;
    wl_callback*                      m_pFrameCallback;
    bool                              m_frameCompleted;
    uint32                            m_capabilities;
    uint32                            m_surfaceVersion;
    bool                              m_useZwpDmaBufProtocol;

    char                              m_deviceName[MaxNodeNameLen];

    PAL_DISALLOW_DEFAULT_CTOR(WaylandWindowSystem);
    PAL_DISALLOW_COPY_AND_ASSIGN(WaylandWindowSystem);
};

} // Amdgpu
} // Pal
