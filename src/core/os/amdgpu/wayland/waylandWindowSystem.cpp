/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palHashSetImpl.h"

#include <sys/mman.h>
#include <sys/sysmacros.h>

using namespace Util;

extern "C" {
// The buffer sharing depends on wl_drm interface, which relies on wl_buffer_interface.
// However, wl_buffer_interface can't be located because libwayland-client.so is not linked directly. To solve this,
// wl_buffer_interface is replaced by a different variable here, which is set to a valid value in Init().

#define WL_BUFFER_INTERFACE
#define wl_buffer_interface wlBufferInterface
static struct wl_interface wlBufferInterface = {};

#define WL_SURFACE_INTERFACE
#define wl_surface_interface wlSurfaceInterface
static struct wl_interface wlSurfaceInterface = {};

// Also remove any symbol exports
#undef WL_EXPORT
#define WL_EXPORT

#include "core/os/amdgpu/wayland/mesa/wayland-dmabuf-protocol.inc"
#include "core/os/amdgpu/wayland/mesa/wayland-drm-protocol.inc"
}

namespace Pal
{
namespace Amdgpu
{

// Define a function type for listeners to be added to proxy
typedef void (*Listener)(void);

// Mapping table between wayland, Linux DRM and PAL formats.
// There can be duplicates for unorm vs sgrb, so this table always assumes unorm.
struct FormatMapping
{
    wl_drm_format  wlDrmFormat; // Defined by wl_drm protocol in wayland-drm-client-protocl.h
    uint32         drmFormat;   // Native DRM format defined in <drm_fourcc.h>
    SwizzledFormat palFormat;
};

// Mapping table between wayland and PAL formats.
// There can be duplicates for unorm vs sgrb, so this table always assumes unorm.
constexpr FormatMapping FormatMappings[] = {
    { WL_DRM_FORMAT_ARGB8888, DRM_FORMAT_ARGB8888,
      {
         ChNumFormat::X8Y8Z8W8_Unorm,
         {{{ ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W }}}
      } },
    { WL_DRM_FORMAT_XRGB8888, DRM_FORMAT_XRGB8888,
      {
         ChNumFormat::X8Y8Z8W8_Unorm,
         {{{ ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::One }}}
      } },
    { WL_DRM_FORMAT_ABGR8888, DRM_FORMAT_ABGR8888,
      {
         ChNumFormat::X8Y8Z8W8_Unorm,
         {{{ ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W }}}
      } },
    { WL_DRM_FORMAT_XBGR8888, DRM_FORMAT_XBGR8888,
      {
         ChNumFormat::X8Y8Z8W8_Unorm,
         {{{ ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::One }}}
      } },
    { WL_DRM_FORMAT_ARGB2101010, DRM_FORMAT_ARGB2101010,
      {
         ChNumFormat::X10Y10Z10W2_Unorm,
         {{{ ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W }}}
      } },
    { WL_DRM_FORMAT_XRGB2101010, DRM_FORMAT_XRGB2101010,
      {
         ChNumFormat::X10Y10Z10W2_Unorm,
         {{{ ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::One }}}
      } },
    { WL_DRM_FORMAT_ABGR2101010, DRM_FORMAT_ABGR2101010,
      {
         ChNumFormat::X10Y10Z10W2_Unorm,
         {{{ ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W }}}
      } },
    { WL_DRM_FORMAT_XBGR2101010, DRM_FORMAT_XBGR2101010,
      {
         ChNumFormat::X10Y10Z10W2_Unorm,
         {{{ ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::One }}}
      } },
    { WL_DRM_FORMAT_RGB565, DRM_FORMAT_RGB565,
      {
         ChNumFormat::X5Y6Z5_Unorm,
         {{{ ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::One }}}
      } },
    { WL_DRM_FORMAT_BGR565, DRM_FORMAT_BGR565,
      {
         ChNumFormat::X5Y6Z5_Unorm,
         {{{ ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::One }}}
      } },
    { WL_DRM_FORMAT_ABGR16F, DRM_FORMAT_ABGR16161616,
      {
         ChNumFormat::X16Y16Z16W16_Float,
         {{{ ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W }}}
      } },
    { WL_DRM_FORMAT_XBGR16F, DRM_FORMAT_XRGB16161616,
      {
         ChNumFormat::X16Y16Z16W16_Float,
         {{{ ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::One }}}
      } },
};

// ====================================================================================================================a
// Convert Linux Drm format to PAL format
static SwizzledFormat DrmToPalFormat(
    uint32 format)
{
    SwizzledFormat outFormat = UndefinedSwizzledFormat;

    for (const auto& fmtPair : FormatMappings)
    {
        if (fmtPair.drmFormat == format)
        {
            outFormat = fmtPair.palFormat;
            break;
        }
    }

    return outFormat;
}

// ====================================================================================================================a
// Convert PAL format to Linux Drm format
static uint32 PalToDrmFormat(
    SwizzledFormat format,
    bool           alpha)
{
    uint32 drmFormat = DRM_FORMAT_XRGB8888;

    if (alpha == false)
    {
        format.swizzle.a = ChannelSwizzle::One;
    }

    if (Formats::IsSrgb(format.format))
    {
        // Wayland has no difference between srgb and unorm; our mapping table uses unorm
        format.format = Formats::ConvertToUnorm(format.format);
    }

    bool found = false;
    for (const auto& fmtPair : FormatMappings)
    {
        if (Formats::IsSameFormat(fmtPair.palFormat, format))
        {
            drmFormat = fmtPair.drmFormat;
            found = true;
            break;
        }
    }

    if (found == false)
    {
        PAL_ASSERT_ALWAYS_MSG("No native format mapping for PAL format %u with swizzle 0x%08x!",
                              format.format, format.swizzle);
    }

    return drmFormat;
}
// ====================================================================================================================a
// Convert Wayland Drm format to Linux Drm format
static uint32 WlDrmToDrmFormat(
    wl_drm_format format
)
{
    uint32 outFormat = DRM_FORMAT_XRGB8888;

    for (const auto& fmtPair : FormatMappings)
    {
        if (fmtPair.wlDrmFormat == format)
        {
            outFormat = fmtPair.drmFormat;
            break;
        }
    }

    return outFormat;
}
// =====================================================================================================================
// Get the notification of the path of the drm device which is used by the server. For multi-GPU, Pal should use this
// device for creating local buffers.
static void DrmHandleDevice(
    void*       pData,
    wl_drm*     pDrm,
    const char* pName)
{
    WaylandWindowSystem* pWaylandWindowSystem = static_cast<WaylandWindowSystem*>(pData);

    pWaylandWindowSystem->SetDeviceName(pName);
}

// =====================================================================================================================
// Get the formats that Wayland compositor supports if using wl_drm
static void DrmHandleFormat(
    void*   pData,
    wl_drm* pDrm,
    uint32  wl_format)
{
    WaylandWindowSystem* pWaylandWindowSystem = static_cast<WaylandWindowSystem*>(pData);

    pWaylandWindowSystem->AddFormat(static_cast<wl_drm_format>(wl_format));
}

// =====================================================================================================================
// Receive if the magic is autenticated by Wayland server, meaningful for EGL and useless for PAL
static void DrmHandleAuthenticated(
    void*   pData,
    wl_drm* pDrm)
{}

// =====================================================================================================================
// Bitmask of capabilities the wl_drm supports, WL_DRM_CAPABILITY_PRIME is a must, otherwise can't create prime buffer.
static void DrmHandleCapabilities(
    void*   pData,
    wl_drm* pDrm,
    uint32  capabilities)
{
    WaylandWindowSystem* pWaylandWindowSystem = static_cast<WaylandWindowSystem*>(pData);

    pWaylandWindowSystem->SetCapabilities(capabilities);
}

// The listener for wl_drm to get the drm device, buffer format and capabilities.
static constexpr wl_drm_listener WaylandDrmListener =
{
    DrmHandleDevice,
    DrmHandleFormat,
    DrmHandleAuthenticated,
    DrmHandleCapabilities,
};

// =====================================================================================================================
// Get the formats that Wayland compositor supports
// The formats are also advertized through the zwp_linux_dmabuf_v1.handle_modfier event
// so we don't need to handle it here
static void DmaHandleFormat(
    void*                pData,
    zwp_linux_dmabuf_v1* pDmaBuf,
    uint32               format)
{}

// =====================================================================================================================
// Get the formats the Wayland compositor supports along with any modifiers it supports with that format
// formats and modifiers are defined in <drm_fourcc.h>
static void DmaHandleModifier(
    void*                pData,
    zwp_linux_dmabuf_v1* pDmaBuf,
    uint32               format,
    uint32               modifierHi,
    uint32               modifierLo)
{
    WaylandWindowSystem* pWaylandWindowSystem = static_cast<WaylandWindowSystem*>(pData);
    // The compositor already advertised formats via the default feedback
    if (pWaylandWindowSystem->GetDefaultFeedback())
    {
        return;
    }

    const ZwpDmaBufFormat fmt = {
        .format = format,
        .padding = 0,
        .modifier = Util::Uint64CombineParts(modifierLo, modifierHi)
    };
    pWaylandWindowSystem->AddFormat(fmt);
}

// The listener to handle recieving format and modifier info via the dmabuf interface directly
// In newer versions of the protocol this is handled via the zwp_dmabuf_default_feedback_v1 interface
static constexpr zwp_linux_dmabuf_v1_listener DmaBufListener =
{
    DmaHandleFormat,
    DmaHandleModifier,
};

// =====================================================================================================================
// Get the buffer created from calling zwp_linux_buffer_params_v1_create
static void DmaCreateBuffer(
    void *pData,
    zwp_linux_buffer_params_v1 *pBufferParams,
    wl_buffer *buffer)
{}

// =====================================================================================================================
// Failure callback if zwp_linux_buffer_params_v1_create failed to create a buffer
static void DmaCreateBufferFailed(
    void *pData,
    zwp_linux_buffer_params_v1 *pBufferParams)
{}
// =====================================================================================================================
// The listener to handle wl_buffer creation and wl_buffer creation failure
static constexpr zwp_linux_buffer_params_v1_listener DmaBufParamsListener =
{
    DmaCreateBuffer,
    DmaCreateBufferFailed
};
// =====================================================================================================================
// Event indicates that all feedback from the compositor has been sent
static void DmaDone(
    void *pData,
    struct zwp_linux_dmabuf_feedback_v1 *pDmaBufFeedback)
{}

// =====================================================================================================================
// Get the formats the Wayland compositor supports along with any modifiers it supports with that format via a
// memory-mappable fd, each entry in the table is a 32-bit format followed by 16 bits of unused padding and a 64-bit
// modifier

// The client must map the fd in read-only private mode
static void DmaFormatTable(
    void *pData,
    struct zwp_linux_dmabuf_feedback_v1 *pDmaBufFeedback,
    int32_t fd,
    uint32_t size)
{
    WaylandWindowSystem* pWaylandWindowSystem = static_cast<WaylandWindowSystem*>(pData);
    WlFormatTable& globalFormatTable = pWaylandWindowSystem->GetGlobalFormatTable();

    globalFormatTable.size = size;
    globalFormatTable.pData = reinterpret_cast<ZwpDmaBufFormat*>(mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0));
}

// =====================================================================================================================
// Get the preferred device by the server when direct scan-out to the target device isn't available
static void DmaMainDevice(
    void *pData,
    struct zwp_linux_dmabuf_feedback_v1 *pDmaBufFeedback,
    struct wl_array *device)
{
    WaylandWindowSystem* pWaylandWindowSystem = static_cast<WaylandWindowSystem*>(pData);
    PAL_ASSERT(device->size == sizeof(dev_t));
    pWaylandWindowSystem->SetDmaDevice(device);
}

// =====================================================================================================================
// Get the indices within the mapped format table of supported formats. The indices are 16-but unsigned integers
static void DmaTrancheFormats(
    void *pData,
    struct zwp_linux_dmabuf_feedback_v1 *pDmaBufFeedback,
    struct wl_array *pIndices)
{
    WaylandWindowSystem* pWaylandWindowSystem = static_cast<WaylandWindowSystem*>(pData);
    WlFormatTable& globalFormatTable = pWaylandWindowSystem->GetGlobalFormatTable();

    if ((globalFormatTable.pData != MAP_FAILED) && (globalFormatTable.pData != nullptr))
    {
        // The c++ compiler does not allow the macro wl_array_for_each to force void* into uint16*, so
        // modify the macro definition here.
        const Span<uint16> tableIndices { static_cast<uint16*>(pIndices->data), (pIndices->size / sizeof(uint16)) };
        for (uint16 index : tableIndices)
        {
            pWaylandWindowSystem->AddFormat(globalFormatTable.pData[index]);
        }

        munmap(globalFormatTable.pData, globalFormatTable.size);
    }
}

// =====================================================================================================================
// Event inidcates that a preference tranche has been sent
static void DmaTrancheDone(
    void *pData,
    struct zwp_linux_dmabuf_feedback_v1 *pDmaBufFeedback)
{}

// =====================================================================================================================
// Get the target device for buffer creation for a given tranche. This is a preferred device, but the buffer must be
// accessible to the main device
static void DmaTrancheTargetDevice(
    void *pData,
    struct zwp_linux_dmabuf_feedback_v1 *pDmaBufFeedback,
    struct wl_array *device)
{}

// =====================================================================================================================
// Get any flags associated with a given tranche
static void DmaTrancheFlags(
    void *pData,
    struct zwp_linux_dmabuf_feedback_v1 *pDmaBufFeedback,
    uint32_t flags)
{}

// Listener for receiving supported formats, modifiers, the main device
static constexpr zwp_linux_dmabuf_feedback_v1_listener FeedbackListener =
{
    DmaDone,
    DmaFormatTable,
    DmaMainDevice,
    DmaTrancheDone,
    DmaTrancheTargetDevice,
    DmaTrancheFormats,
    DmaTrancheFlags,
};

// =====================================================================================================================
// This function is called while there is any global interface is registered to server. Check if
// zwp_linux_dmabuf_v1 is registered if setting indicated to use it, otherwise we check for wl_drm and override
// the cached setting
static void RegistryHandleGlobal(
    void*        pData,
    wl_registry* pRegistry,
    uint32       name,
    const char*  pInterface,
    uint32       version)
{
    WaylandWindowSystem* pWaylandWindowSystem = static_cast<WaylandWindowSystem*>(pData);
    if (pWaylandWindowSystem->UseZwpDmaBufProtocol() == true)
    {
        if (strcmp(pInterface, "zwp_linux_dmabuf_v1") == 0)
        {
            PAL_ASSERT(version >= ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION);

            zwp_linux_dmabuf_v1* pDmaBuffer = reinterpret_cast<struct zwp_linux_dmabuf_v1*>(
                pWaylandWindowSystem->GetWaylandProcs().pfnWlProxyMarshalConstructorVersioned(
                    reinterpret_cast<wl_proxy*>(pRegistry),
                    WL_REGISTRY_BIND,
                    &zwp_linux_dmabuf_v1_interface,
                    version,
                    name,
                    zwp_linux_dmabuf_v1_interface.name,
                    version,
                    nullptr));

            if (pDmaBuffer != nullptr)
            {
                pWaylandWindowSystem->GetWaylandProcs().pfnWlProxyAddListener(
                    reinterpret_cast<wl_proxy*>(pDmaBuffer),
                    reinterpret_cast<Listener*>(const_cast<zwp_linux_dmabuf_v1_listener*>
                        (&DmaBufListener)),
                    pWaylandWindowSystem);

                pWaylandWindowSystem->SetDmaBuffer(pDmaBuffer);
                return;
            }
        }
    }
    // If the zwp_linux_dmabuf_v1 protocol isn't supported by the compositor, we fallback to wl_drm
    if (strcmp(pInterface, "wl_drm") == 0)
    {
        PAL_ASSERT(version >= 2);

        wl_drm* pWaylandDrm = reinterpret_cast<struct wl_drm*>(
            pWaylandWindowSystem->GetWaylandProcs().pfnWlProxyMarshalConstructorVersioned(
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
            pWaylandWindowSystem->GetWaylandProcs().pfnWlProxyAddListener(
                reinterpret_cast<wl_proxy*>(pWaylandDrm),
                reinterpret_cast<Listener*>(const_cast<wl_drm_listener*>
                    (&WaylandDrmListener)),
                pWaylandWindowSystem);

            pWaylandWindowSystem->SetWaylandDrm(pWaylandDrm);
            pWaylandWindowSystem->SetZwpDmaBufProtocolUsage(false);
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

// A listener to handle the interfaces registered to server
static constexpr wl_registry_listener RegistryListener =
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
    Image* pImage = static_cast<Image*>(pData);

    pImage->SetIdle(true);
}

// A listener for event that buffer is released from Wayland server.
static constexpr wl_buffer_listener BufferListener =
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
    WaylandWindowSystem* pWaylandWindowSystem = static_cast<WaylandWindowSystem*>(pData);

    pWaylandWindowSystem->SetFrameCallback(nullptr);
    pWaylandWindowSystem->SetFrameCompleted();

    pWaylandWindowSystem->GetWaylandProcs().pfnWlProxyDestroy(reinterpret_cast<wl_proxy*>(pCallback));
}

// Handle the notification when it is a good time to start drawing a new frame
static constexpr wl_callback_listener FrameListener =
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
            // Dispatch the event in pending status so that quick check if the present fence is signaled.
            m_windowSystem.GetWaylandProcs().pfnWlDisplayDispatchQueuePending(m_windowSystem.GetDisplay(),
                                                                              m_windowSystem.GetEventQueue());

            signaled = m_pImage->GetIdle();

            if (!signaled)
            {
                // Block until all of the requests are processed by the server.
                m_windowSystem.GetWaylandProcs().pfnWlDisplayRoundtripQueue(m_windowSystem.GetDisplay(),
                                                                            m_windowSystem.GetEventQueue());
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
    m_waylandLoader(device.GetPlatform()->GetWaylandLoader()),
#if defined(PAL_DEBUG_PRINTS)
    m_waylandProcs(m_waylandLoader.GetProcsTableProxy()),
#else
    m_waylandProcs(m_waylandLoader.GetProcsTable()),
#endif
    m_validFormats(8, device.GetPlatform()),
    m_pEventQueue(nullptr),
    m_pSurfaceEventQueue(nullptr),
    m_pDisplayWrapper(nullptr),
    m_pSurfaceWrapper(nullptr),
    m_pDmaBuffer(nullptr),
    m_pWaylandDrm(nullptr),
    m_pWaylandDrmWrapper(nullptr),
    m_pDefaultDmaBuffFeedback(nullptr),
    m_DmaDevice(0), // This is a valid dev_t, must change
    m_globalFormatTable({}),
    m_pFrameCallback(nullptr),
    m_frameCompleted(false),
    m_capabilities(0),
    m_surfaceVersion(0),
    m_useZwpDmaBufProtocol(false)
{}

// =====================================================================================================================
WaylandWindowSystem::~WaylandWindowSystem()
{
    // The wrapper object must be destroyed before the object it was created from.
    if (m_pDmaBuffer != nullptr)
    {
        m_waylandProcs.pfnWlProxyMarshal(reinterpret_cast<wl_proxy*>(m_pDmaBuffer), ZWP_LINUX_DMABUF_V1_DESTROY);
        m_waylandProcs.pfnWlProxyDestroy(reinterpret_cast<wl_proxy*>(m_pDmaBuffer));
    }
    if (m_pDefaultDmaBuffFeedback != nullptr)
    {
        m_waylandProcs.pfnWlProxyMarshal(reinterpret_cast<wl_proxy*>(m_pDefaultDmaBuffFeedback),
                                         ZWP_LINUX_DMABUF_FEEDBACK_V1_DESTROY);
        m_waylandProcs.pfnWlProxyDestroy(reinterpret_cast<wl_proxy*>(m_pDefaultDmaBuffFeedback));
    }
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

    // wlBufferInterface must be set before calling any wl_drm interfaces.
    // wlBufferInterface and wlSurfaceInterface must be set before calling any zwp_linux_dmabuf_v1 interfaces.
    wlBufferInterface       = *m_waylandLoader.GetWlBufferInterface();
    wlSurfaceInterface      = *m_waylandLoader.GetWlSurfaceInterface();

    m_pEventQueue           = m_waylandProcs.pfnWlDisplayCreateQueue(m_pDisplay);
    m_useZwpDmaBufProtocol  = m_device.Settings().useZwpDmaBufProtocol;

    if (m_pEventQueue == nullptr)
    {
        result = Result::ErrorInitializationFailed;
    }

    if (result == Result::Success)
    {
        result = m_validFormats.Init();
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

        //At this point, round-trip to build the global instance
        m_waylandProcs.pfnWlDisplayRoundtripQueue(m_pDisplay, m_pEventQueue);

        result = FinishInit();
    }

    if (result == Result::Success)
    {

        m_pSurfaceWrapper = reinterpret_cast<wl_surface*>(m_waylandProcs.pfnWlProxyCreateWrapper(m_pSurface));

        if (m_pSurfaceWrapper == nullptr)
        {
            result = Result::ErrorInitializationFailed;
        }
        else
        {
            m_surfaceVersion = m_waylandProcs.pfnWlProxyGetVersion(reinterpret_cast<wl_proxy*>(m_pSurfaceWrapper));
        }
    }

    if (result == Result::Success)
    {
        m_waylandProcs.pfnWlProxySetQueue(reinterpret_cast<wl_proxy*>(m_pSurfaceWrapper), m_pSurfaceEventQueue);
        m_waylandProcs.pfnWlProxyDestroy(reinterpret_cast<wl_proxy*>(pRegistry));
    }

    if (result == Result::Success)
    {
        ConfigPresentOnSameGpu();
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
    Result                      result        = Result::Success;
    const SubResourceInfo*const pSubResInfo   = pImage->SubresourceInfo(0);
    wl_buffer*                  pBuffer       = nullptr;

    const uint32 width  = pSubResInfo->extentTexels.width;
    const uint32 height = pSubResInfo->extentTexels.height;
    const uint32 stride = pSubResInfo->rowPitch;
    const uint32 bpp    = pSubResInfo->bitsPerTexel;
    const bool   alpha  = pSwapChain->CreateInfo().compositeAlpha == CompositeAlphaMode::PostMultiplied;

    uint32 format = PalToDrmFormat(pSubResInfo->format, alpha);
    PAL_ASSERT(IsSupportedFormat(format) == true);

    if ((width == 0) || (height == 0) || (stride == 0) || (bpp == 0) || (sharedBufferFd == InvalidFd))
    {
        result = Result::ErrorInvalidValue;
    }

    if (result == Result::Success)
    {
        result = CreateWlBuffer(width, height, stride, format, 0, sharedBufferFd, &pBuffer);

        if (result == Result::Success)
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

    if ((m_surfaceVersion >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) &&
        (presentInfo.rectangleCount > 0) &&
        (presentInfo.pRectangles != nullptr))
    {
        for (uint32 r = 0; r < presentInfo.rectangleCount; ++r)
        {
            const Rect& damageRect = presentInfo.pRectangles[r];
            m_waylandProcs.pfnWlProxyMarshal(reinterpret_cast<wl_proxy*>(m_pSurfaceWrapper),
                                             WL_SURFACE_DAMAGE_BUFFER,
                                             damageRect.offset.x,
                                             damageRect.offset.y,
                                             damageRect.extent.width,
                                             damageRect.extent.height);
        }
    }
    else
    {
        m_waylandProcs.pfnWlProxyMarshal(reinterpret_cast<wl_proxy*>(m_pSurfaceWrapper),
                                         WL_SURFACE_DAMAGE,
                                         0,
                                         0,
                                         srcImageCreateInfo.extent.width,
                                         srcImageCreateInfo.extent.height);
    }

    m_waylandProcs.pfnWlProxyMarshal(reinterpret_cast<wl_proxy*>(m_pSurfaceWrapper), WL_SURFACE_COMMIT);
    m_waylandProcs.pfnWlDisplayFlush(m_pDisplay);

    Developer::PresentationModeData data = {};
    m_device.DeveloperCb(Developer::CallbackType::PresentConcluded, &data);

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
Result WaylandWindowSystem::GetWindowProperties(
    Device*              pDevice,
    OsDisplayHandle      hDisplay,
    OsWindowHandle       hWindow,
    SwapChainProperties* pSwapChainProperties)
{
    Result result = Result::Success;

    pSwapChainProperties->currentExtent = { UINT32_MAX, UINT32_MAX };
    pSwapChainProperties->compositeAlphaMode = static_cast<uint32>(CompositeAlphaMode::PostMultiplied) |
                                               static_cast<uint32>(CompositeAlphaMode::Opaque);

    WindowSystemCreateInfo createInfo = {};
    createInfo.platform = WsiPlatform::Wayland;
    createInfo.hDisplay = hDisplay;
    createInfo.hWindow  = hWindow;
    createInfo.format   = UndefinedSwizzledFormat; // Meaningless on Wayland.
    // Other fields don't matter if we are only querying info.

    WaylandWindowSystem wsi(*pDevice, createInfo);
    result = wsi.Init();
    // Wayland will happily tell us all sorts of things... but we need to make an event loop and init first.
    // After init, all properties should be available.

    if ((result == Result::Success) && (wsi.m_validFormats.GetNumEntries() != 0))
    {
        pSwapChainProperties->imageFormatCount = 0;
        for (auto fmtIter = wsi.m_validFormats.Begin(); fmtIter.Get() != nullptr; fmtIter.Next())
        {
            if (pSwapChainProperties->imageFormatCount >= MaxPresentableImageFormat)
            {
                PAL_ALERT_ALWAYS_MSG("Could not fit all presentable formats in window properties");
                break;
            }

            SwizzledFormat palFormat = DrmToPalFormat(fmtIter.Get()->key);
            if (palFormat.format != UndefinedSwizzledFormat.format)
            {
                pSwapChainProperties->imageFormat[pSwapChainProperties->imageFormatCount] = palFormat;
                pSwapChainProperties->imageFormatCount++;

                // Wayland treats SRGB vs unorm identically, so if we support one, we support both
                // Our mapping tables use unorm, so convert.
                palFormat.format = Formats::ConvertToSrgb(palFormat.format);
                if ((palFormat.format != UndefinedSwizzledFormat.format) &&
                    (pSwapChainProperties->imageFormatCount < MaxPresentableImageFormat))
                {
                    pSwapChainProperties->imageFormat[pSwapChainProperties->imageFormatCount] = palFormat;
                    pSwapChainProperties->imageFormatCount++;
                }
            }
        }
    }

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

// =====================================================================================================================
// Check whether the advertised present target is our current device
void WaylandWindowSystem::ConfigPresentOnSameGpu()
{
   if (m_useZwpDmaBufProtocol == true)
   {
        DrmNodeProperties drmProps;
        Pal::Result result = m_device.GetDrmNodeProperties(&drmProps);

        PAL_ASSERT(result == Result::Success);

        int64 drmNodeMajor, drmNodeMinor;
        if (drmProps.flags.hasRenderDrmNode)
        {
            drmNodeMajor = drmProps.renderDrmNodeMajor;
            drmNodeMinor = drmProps.renderDrmNodeMinor;
        }
        if (drmProps.flags.hasPrimaryDrmNode)
        {
            drmNodeMajor = drmProps.primaryDrmNodeMajor;
            drmNodeMinor = drmProps.primaryDrmNodeMinor;

        }
        if ((major(m_DmaDevice) == drmNodeMajor) &&
            (minor(m_DmaDevice) == drmNodeMinor))
        {
            m_presentOnSameGpu = true;
        }
    }
    else
    {
        PAL_ASSERT(strlen(m_deviceName) > 0);
        m_device.IsSameGpu(m_deviceName, &m_presentOnSameGpu);
    }
}

// Finalize initialization specific to the wl_drm interface
Result WaylandWindowSystem::FinishWlDrmInit()
{
    Result result = Result::Success;

    if (m_pWaylandDrm == nullptr)
    {
        result = Result::ErrorInitializationFailed;
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
    return result;
}

// Finalize initialization specific to the zwp_linux_dmabuf_v1 interface
Result WaylandWindowSystem::FinishZwpDmaBufInit()
{
    Result result = Result::Success;

    if (m_pDmaBuffer == nullptr)
    {
        result = Result::ErrorInitializationFailed;
    }

    if (result == Result::Success)
    {
        m_waylandProcs.pfnWlDisplayRoundtripQueue(m_pDisplay, m_pEventQueue);

        m_pDefaultDmaBuffFeedback = reinterpret_cast<zwp_linux_dmabuf_feedback_v1*>(
                                                    m_waylandProcs.pfnWlProxyMarshalConstructor(
                                                        reinterpret_cast<wl_proxy*>(m_pDmaBuffer),
                                                        ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK,
                                                        m_waylandLoader.GetZwpLinuxDmabufFeedbackV1Interface(),
                                                        nullptr));

        if (m_pDefaultDmaBuffFeedback == nullptr)
        {
            m_waylandProcs.pfnWlProxyAddListener(
                reinterpret_cast<wl_proxy*>(m_pDefaultDmaBuffFeedback),
                reinterpret_cast<Listener*>(
                const_cast<zwp_linux_dmabuf_feedback_v1_listener*>(&FeedbackListener)),
                this);

            m_waylandProcs.pfnWlDisplayRoundtripQueue(m_pDisplay, m_pEventQueue);
        }
        else
        {
            result = Result::ErrorInitializationFailed;
        }

    }

    return result;
}

// Finalize configuration-specific initialization
Result WaylandWindowSystem::FinishInit()
{
    Result result = Result::Success;

    if (m_useZwpDmaBufProtocol == true)
    {
        result = FinishZwpDmaBufInit();
    }
    else
    {
        // We default back to the wayland drm extension
        result = FinishWlDrmInit();
    }

    return result;
}

// Create a wl_buffer from a given image
// Buffer creation differs on whether we use the wl_drm or zwp_linux_dmabuf_v1 interfaces
Result WaylandWindowSystem::CreateWlBuffer(
    uint32 width,
    uint32 height,
    uint32 stride,
    uint32 format,
    uint32 flags,
    int32 sharedBufferFd,
    wl_buffer** ppWlBuf)
{
    wl_buffer*                  pBuffer       = nullptr;
    zwp_linux_buffer_params_v1* pBufferParams = nullptr;
    Result result                             = Result::Success;

    if (m_useZwpDmaBufProtocol == true)
    {
        pBufferParams = reinterpret_cast<zwp_linux_buffer_params_v1*>(m_waylandProcs.pfnWlProxyMarshalConstructor(
            reinterpret_cast<wl_proxy*>(m_pDmaBuffer),
            ZWP_LINUX_DMABUF_V1_CREATE_PARAMS,
            m_waylandLoader.GetZwpLinuxBufferParamsV1Interface(),
            nullptr
        ));

        if (pBufferParams == nullptr)
        {
            result = Result::ErrorUnknown;
        }
        else
        {
            // All of our supported formats are linear
            m_waylandProcs.pfnWlProxyMarshal(reinterpret_cast<wl_proxy*>(pBufferParams),
                                            ZWP_LINUX_BUFFER_PARAMS_V1_ADD,
                                            sharedBufferFd,
                                            0,
                                            0,
                                            stride,
                                            Util::HighPart(DRM_FORMAT_MOD_LINEAR),
                                            Util::LowPart(DRM_FORMAT_MOD_LINEAR));

            pBuffer = reinterpret_cast<wl_buffer*>(m_waylandProcs.pfnWlProxyMarshalConstructor(
                                                    reinterpret_cast<wl_proxy*>(pBufferParams),
                                                    ZWP_LINUX_BUFFER_PARAMS_V1_CREATE_IMMED,
                                                    m_waylandLoader.GetWlBufferInterface(),
                                                    nullptr,
                                                    width,
                                                    height,
                                                    format,
                                                    0));
        }
    }
    else
    {
        pBuffer = reinterpret_cast<wl_buffer*>(m_waylandProcs.pfnWlProxyMarshalConstructor(
                                                   reinterpret_cast<wl_proxy*>(m_pWaylandDrmWrapper),
                                                   WL_DRM_CREATE_PRIME_BUFFER,
                                                   m_waylandLoader.GetWlBufferInterface(),
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
    }

    if (pBuffer == nullptr)
    {
        result = Result::ErrorUnknown;
    }
    else if (ppWlBuf != nullptr)
    {
        *ppWlBuf = pBuffer;
    }

    close(sharedBufferFd);

    //Destroy params
    if (pBufferParams != nullptr)
    {
        m_waylandProcs.pfnWlProxyMarshal(reinterpret_cast<wl_proxy*>(pBufferParams), ZWP_LINUX_BUFFER_PARAMS_V1_DESTROY);
        m_waylandProcs.pfnWlProxyDestroy(reinterpret_cast<wl_proxy*>(pBufferParams));
    }

    return result;
}

// Add a format advertised via the zwp_linux_dmabuf_v1 or zwp_linux_dmabuf_default_feedback_v1 interfaces
Result WaylandWindowSystem::AddFormat(ZwpDmaBufFormat fmt)
{
    return m_validFormats.Insert(fmt.format);
}

// Add a format advertised via the wl_drm interface
Result WaylandWindowSystem::AddFormat(wl_drm_format fmt)
{
    uint32 drmFormat = WlDrmToDrmFormat(fmt);
    return m_validFormats.Insert(drmFormat);
}

// For now the modifiers won't impact the rendered image so we check against a HashSet,
// but in the future supporting modifiers would require more complex logic
bool WaylandWindowSystem::IsSupportedFormat(uint32 fmt)
{
    return (m_validFormats.Contains(fmt) != false);
}

} // Amdgpu
} // Pal
