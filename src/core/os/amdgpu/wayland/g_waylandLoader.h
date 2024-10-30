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
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!
//
// This code has been generated automatically. Do not hand-modify this code.
//
// Modify the procAnalysis.py and waylandLoader.py in the tools/generate directory OR waylandWindowSystem.proc instead
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!  WARNING!  WARNING!
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "core/os/amdgpu/wayland/protocol/wayland-dmabuf-client-protocol.h"
#include "core/os/amdgpu/wayland/protocol/wayland-drm-client-protocol.h"

#ifdef None
#undef None
#endif
#ifdef Success
#undef Success
#endif
#ifdef Always
#undef Always
#endif

#include "palFile.h"
#include "palLibrary.h"

using namespace Util;

namespace Pal
{
namespace Amdgpu
{
// symbols from libwayland-client.so.0
typedef wl_event_queue* (*WlDisplayCreateQueue)(
            struct wl_display*    display);

typedef int (*WlDisplayDispatchQueue)(
            struct wl_display*        display,
            struct wl_event_queue*    queue);

typedef int (*WlDisplayDispatchQueuePending)(
            struct wl_display*        display,
            struct wl_event_queue*    queue);

typedef int (*WlDisplayFlush)(
            struct wl_display*    display);

typedef int (*WlDisplayRoundtripQueue)(
            struct wl_display*        display,
            struct wl_event_queue*    queue);

typedef void (*WlEventQueueDestroy)(
            struct wl_event_queue*    queue);

typedef int (*WlProxyAddListener)(
            struct wl_proxy*  proxy,
            void              (**implementation)(void),
            void*             data);

typedef void* (*WlProxyCreateWrapper)(
            void*     proxy);

typedef void (*WlProxyDestroy)(
            struct wl_proxy*  proxy);

typedef uint32 (*WlProxyGetVersion)(
            struct wl_proxy*  proxy);

typedef void (*WlProxyMarshal)(
            struct wl_proxy*  p,
            uint32            opcode,
            ...);

typedef wl_proxy* (*WlProxyMarshalConstructor)(
            struct wl_proxy*              proxy,
            uint32                        opcode,
            const struct wl_interface*    interface,
            ...);

typedef wl_proxy* (*WlProxyMarshalConstructorVersioned)(
            struct wl_proxy*              proxy,
            uint32                        opcode,
            const struct wl_interface*    interface,
            uint32                        version,
            ...);

typedef void (*WlProxySetQueue)(
            struct wl_proxy*          proxy,
            struct wl_event_queue*    queue);

typedef void (*WlProxyWrapperDestroy)(
            void*     proxy_wrapper);

enum WaylandLoaderLibraries : uint32
{
    LibWaylandClient = 0,
    WaylandLoaderLibrariesCount = 1
};

struct WaylandLoaderFuncs
{
    WlDisplayCreateQueue                  pfnWlDisplayCreateQueue;
    bool pfnWlDisplayCreateQueueisValid() const
    {
        return (pfnWlDisplayCreateQueue != nullptr);
    }

    WlDisplayDispatchQueue                pfnWlDisplayDispatchQueue;
    bool pfnWlDisplayDispatchQueueisValid() const
    {
        return (pfnWlDisplayDispatchQueue != nullptr);
    }

    WlDisplayDispatchQueuePending         pfnWlDisplayDispatchQueuePending;
    bool pfnWlDisplayDispatchQueuePendingisValid() const
    {
        return (pfnWlDisplayDispatchQueuePending != nullptr);
    }

    WlDisplayFlush                        pfnWlDisplayFlush;
    bool pfnWlDisplayFlushisValid() const
    {
        return (pfnWlDisplayFlush != nullptr);
    }

    WlDisplayRoundtripQueue               pfnWlDisplayRoundtripQueue;
    bool pfnWlDisplayRoundtripQueueisValid() const
    {
        return (pfnWlDisplayRoundtripQueue != nullptr);
    }

    WlEventQueueDestroy                   pfnWlEventQueueDestroy;
    bool pfnWlEventQueueDestroyisValid() const
    {
        return (pfnWlEventQueueDestroy != nullptr);
    }

    WlProxyAddListener                    pfnWlProxyAddListener;
    bool pfnWlProxyAddListenerisValid() const
    {
        return (pfnWlProxyAddListener != nullptr);
    }

    WlProxyCreateWrapper                  pfnWlProxyCreateWrapper;
    bool pfnWlProxyCreateWrapperisValid() const
    {
        return (pfnWlProxyCreateWrapper != nullptr);
    }

    WlProxyDestroy                        pfnWlProxyDestroy;
    bool pfnWlProxyDestroyisValid() const
    {
        return (pfnWlProxyDestroy != nullptr);
    }

    WlProxyGetVersion                     pfnWlProxyGetVersion;
    bool pfnWlProxyGetVersionisValid() const
    {
        return (pfnWlProxyGetVersion != nullptr);
    }

    WlProxyMarshal                        pfnWlProxyMarshal;
    bool pfnWlProxyMarshalisValid() const
    {
        return (pfnWlProxyMarshal != nullptr);
    }

    WlProxyMarshalConstructor             pfnWlProxyMarshalConstructor;
    bool pfnWlProxyMarshalConstructorisValid() const
    {
        return (pfnWlProxyMarshalConstructor != nullptr);
    }

    WlProxyMarshalConstructorVersioned    pfnWlProxyMarshalConstructorVersioned;
    bool pfnWlProxyMarshalConstructorVersionedisValid() const
    {
        return (pfnWlProxyMarshalConstructorVersioned != nullptr);
    }

    WlProxySetQueue                       pfnWlProxySetQueue;
    bool pfnWlProxySetQueueisValid() const
    {
        return (pfnWlProxySetQueue != nullptr);
    }

    WlProxyWrapperDestroy                 pfnWlProxyWrapperDestroy;
    bool pfnWlProxyWrapperDestroyisValid() const
    {
        return (pfnWlProxyWrapperDestroy != nullptr);
    }

};

// =====================================================================================================================
// the class serves as a proxy layer to add more functionality to wrapped callbacks.
#if defined(PAL_DEBUG_PRINTS)
class WaylandLoaderFuncsProxy
{
public:
    WaylandLoaderFuncsProxy() { }
    ~WaylandLoaderFuncsProxy() { }

    void SetFuncCalls(WaylandLoaderFuncs* pFuncs) { m_pFuncs = pFuncs; }

    void Init(const char* pPath);

    wl_event_queue* pfnWlDisplayCreateQueue(
            struct wl_display*    display) const;

    bool pfnWlDisplayCreateQueueisValid() const
    {
        return (m_pFuncs->pfnWlDisplayCreateQueue != nullptr);
    }

    int pfnWlDisplayDispatchQueue(
            struct wl_display*        display,
            struct wl_event_queue*    queue) const;

    bool pfnWlDisplayDispatchQueueisValid() const
    {
        return (m_pFuncs->pfnWlDisplayDispatchQueue != nullptr);
    }

    int pfnWlDisplayDispatchQueuePending(
            struct wl_display*        display,
            struct wl_event_queue*    queue) const;

    bool pfnWlDisplayDispatchQueuePendingisValid() const
    {
        return (m_pFuncs->pfnWlDisplayDispatchQueuePending != nullptr);
    }

    int pfnWlDisplayFlush(
            struct wl_display*    display) const;

    bool pfnWlDisplayFlushisValid() const
    {
        return (m_pFuncs->pfnWlDisplayFlush != nullptr);
    }

    int pfnWlDisplayRoundtripQueue(
            struct wl_display*        display,
            struct wl_event_queue*    queue) const;

    bool pfnWlDisplayRoundtripQueueisValid() const
    {
        return (m_pFuncs->pfnWlDisplayRoundtripQueue != nullptr);
    }

    void pfnWlEventQueueDestroy(
            struct wl_event_queue*    queue) const;

    bool pfnWlEventQueueDestroyisValid() const
    {
        return (m_pFuncs->pfnWlEventQueueDestroy != nullptr);
    }

    int pfnWlProxyAddListener(
            struct wl_proxy*  proxy,
            void              (**implementation)(void),
            void*             data) const;

    bool pfnWlProxyAddListenerisValid() const
    {
        return (m_pFuncs->pfnWlProxyAddListener != nullptr);
    }

    void* pfnWlProxyCreateWrapper(
            void*     proxy) const;

    bool pfnWlProxyCreateWrapperisValid() const
    {
        return (m_pFuncs->pfnWlProxyCreateWrapper != nullptr);
    }

    void pfnWlProxyDestroy(
            struct wl_proxy*  proxy) const;

    bool pfnWlProxyDestroyisValid() const
    {
        return (m_pFuncs->pfnWlProxyDestroy != nullptr);
    }

    uint32 pfnWlProxyGetVersion(
            struct wl_proxy*  proxy) const;

    bool pfnWlProxyGetVersionisValid() const
    {
        return (m_pFuncs->pfnWlProxyGetVersion != nullptr);
    }

    void pfnWlProxyMarshal(
            struct wl_proxy*  p,
            uint32            opcode,
            ...) const;

    bool pfnWlProxyMarshalisValid() const
    {
        return (m_pFuncs->pfnWlProxyMarshal != nullptr);
    }

    wl_proxy* pfnWlProxyMarshalConstructor(
            struct wl_proxy*              proxy,
            uint32                        opcode,
            const struct wl_interface*    interface,
            ...) const;

    bool pfnWlProxyMarshalConstructorisValid() const
    {
        return (m_pFuncs->pfnWlProxyMarshalConstructor != nullptr);
    }

    wl_proxy* pfnWlProxyMarshalConstructorVersioned(
            struct wl_proxy*              proxy,
            uint32                        opcode,
            const struct wl_interface*    interface,
            uint32                        version,
            ...) const;

    bool pfnWlProxyMarshalConstructorVersionedisValid() const
    {
        return (m_pFuncs->pfnWlProxyMarshalConstructorVersioned != nullptr);
    }

    void pfnWlProxySetQueue(
            struct wl_proxy*          proxy,
            struct wl_event_queue*    queue) const;

    bool pfnWlProxySetQueueisValid() const
    {
        return (m_pFuncs->pfnWlProxySetQueue != nullptr);
    }

    void pfnWlProxyWrapperDestroy(
            void*     proxy_wrapper) const;

    bool pfnWlProxyWrapperDestroyisValid() const
    {
        return (m_pFuncs->pfnWlProxyWrapperDestroy != nullptr);
    }

private:
    Util::File  m_timeLogger;
    Util::File  m_paramLogger;
    WaylandLoaderFuncs* m_pFuncs;

    PAL_DISALLOW_COPY_AND_ASSIGN(WaylandLoaderFuncsProxy);
};
#endif

class Platform;

// =====================================================================================================================
// the class is responsible for resolving all external symbols that required by the Dri3WindowSystem.
class WaylandLoader
{
public:
    WaylandLoader();
    ~WaylandLoader();

    bool   Initialized() { return m_initialized; }

    const WaylandLoaderFuncs& GetProcsTable()const { return m_funcs; }
#if defined(PAL_DEBUG_PRINTS)
    const WaylandLoaderFuncsProxy& GetProcsTableProxy()const { return m_proxy; }

    void SetLogPath(const char* pPath) { m_proxy.Init(pPath); }
#endif

    Result Init(Platform* pPlatform);

    wl_interface* GetWlRegistryInterface() const;
    wl_interface* GetWlBufferInterface() const;
    wl_interface* GetWlCallbackInterface() const;
    wl_interface* GetWlSurfaceInterface() const;
    wl_interface* GetZwpLinuxDmabufV1Interface() const;
    wl_interface* GetZwpLinuxBufferParamsV1Interface() const;
    wl_interface* GetZwpLinuxDmabufFeedbackV1Interface() const;

private:
    wl_interface* m_pWlRegistryInterface;
    wl_interface* m_pWlBufferInterface;
    wl_interface* m_pWlCallbackInterface;
    wl_interface* m_pWlSurfaceInterface;
    wl_interface* m_pZwpLinuxDmabufV1Interface;
    wl_interface* m_pZwpLinuxBufferParamsV1Interface;
    wl_interface* m_pZwpLinuxDmabufFeedbackV1Interface;

    Util::Library m_library[WaylandLoaderLibrariesCount];
    bool          m_initialized;

    WaylandLoaderFuncs      m_funcs;
#if defined(PAL_DEBUG_PRINTS)
    WaylandLoaderFuncsProxy m_proxy;
#endif

    PAL_DISALLOW_COPY_AND_ASSIGN(WaylandLoader);
};

} // Amdgpu
} // Pal
