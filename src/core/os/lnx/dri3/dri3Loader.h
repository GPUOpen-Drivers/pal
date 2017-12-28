/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
// Modify the procsAnalysis.py and dri3Loader.py in the tools/generate directory OR dri3WindowSystem.proc instead
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!  WARNING!  WARNING!
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/dri2tokens.h>
#ifdef None
#undef None
#endif
#ifdef Success
#undef Success
#endif
#ifdef Always
#undef Always
#endif
#include <xcb/dri3.h>
#include <xcb/dri2.h>
#include <xcb/xcb.h>
#include <xcb/present.h>
extern "C"
{
    #include <X11/xshmfence.h>
}

#include "pal.h"
#include "palFile.h"
using namespace Util;
namespace Pal
{
namespace Linux
{
// symbols from libX11-xcb.so.1
typedef xcb_connection_t* (*XGetXCBConnection)(
            Display*  pDisplay);

// symbols from libxcb.so.1
typedef uint32 (*XcbGenerateId)(
            xcb_connection_t*     pConnection);

typedef xcb_special_event_t* (*XcbRegisterForSpecialXge)(
            xcb_connection_t*     pConnection,
            xcb_extension_t*      pExtensions,
            uint32                eventId,
            uint32*               pStamp);

typedef void (*XcbUnregisterForSpecialEvent)(
            xcb_connection_t*     pConnection,
            xcb_special_event_t*  pEvent);

typedef xcb_generic_event_t* (*XcbWaitForSpecialEvent)(
            xcb_connection_t*     pConnection,
            xcb_special_event*    pEvent);

typedef const xcb_query_extension_reply_t* (*XcbGetExtensionData)(
            xcb_connection_t*     pConnection,
            xcb_extension_t*      pExtension);

typedef void (*XcbPrefetchExtensionData)(
            xcb_connection_t*     pConnection,
            xcb_extension_t*      pExtension);

typedef xcb_generic_error_t* (*XcbRequestCheck)(
            xcb_connection_t*     pConnection,
            xcb_void_cookie_t     cookie);

typedef xcb_get_geometry_cookie_t (*XcbGetGeometry)(
            xcb_connection_t*     pConnection,
            xcb_drawable_t        drawable);

typedef xcb_get_geometry_reply_t* (*XcbGetGeometryReply)(
            xcb_connection_t*             pConnection,
            xcb_get_geometry_cookie_t     cookie,
            xcb_generic_error_t**         ppError);

typedef xcb_void_cookie_t (*XcbFreePixmapChecked)(
            xcb_connection_t*     pConnection,
            xcb_pixmap_t          pixmap);

typedef xcb_intern_atom_reply_t* (*XcbInternAtomReply)(
            xcb_connection_t*         pConnection,
            xcb_intern_atom_cookie_t  cookie,
            xcb_generic_error_t**     ppError);

typedef xcb_intern_atom_cookie_t (*XcbInternAtom)(
            xcb_connection_t*     pConnection,
            uint8                 onlyIfExists,
            uint16                nameLen,
            const char*           pName);

typedef xcb_depth_iterator_t (*XcbScreenAllowedDepthsIterator)(
            const xcb_screen_t*   pScreen);

typedef void (*XcbDepthNext)(
            xcb_depth_iterator_t*     pDepthIter);

typedef void (*XcbVisualtypeNext)(
            xcb_visualtype_iterator_t*    pVisualTypeIter);

typedef xcb_screen_iterator_t (*XcbSetupRootsIterator)(
            const xcb_setup_t*    pSetup);

typedef void (*XcbScreenNext)(
            xcb_screen_iterator_t*    pScreenIter);

typedef xcb_visualtype_iterator_t (*XcbDepthVisualsIterator)(
            const xcb_depth_t*    pDepth);

typedef const xcb_setup_t* (*XcbGetSetup)(
            xcb_connection_t*     pConnection);

typedef const xcb_setup_t* (*XcbFlush)(
            xcb_connection_t*     pConnection);

// symbols from libxshmfence.so.1
typedef int32 (*XshmfenceUnmapShm)(
            struct xshmfence*     pFence);

typedef xshmfence* (*XshmfenceMapShm)(
            int32     fence);

typedef int32 (*XshmfenceQuery)(
            struct xshmfence*     pFence);

typedef int32 (*XshmfenceAllocShm)(void);

typedef int32 (*XshmfenceTrigger)(
            struct xshmfence*     pFence);

typedef void (*XshmfenceReset)(
            struct xshmfence*     pFence);

// symbols from libxcb-dri3.so.0
typedef xcb_dri3_open_cookie_t (*XcbDri3Open)(
            xcb_connection_t*     pConnection,
            xcb_drawable_t        drawable,
            uint32                provider);

typedef xcb_dri3_open_reply_t* (*XcbDri3OpenReply)(
            xcb_connection_t*         pConnection,
            xcb_dri3_open_cookie_t    cookie,
            xcb_generic_error_t**     ppError);

typedef int32* (*XcbDri3OpenReplyFds)(
            xcb_connection_t*         pConnection,
            xcb_dri3_open_reply_t*    pReply);

typedef xcb_void_cookie_t (*XcbDri3FenceFromFdChecked)(
            xcb_connection_t*     pConnection,
            xcb_drawable_t        drawable,
            uint32                fence,
            uint8                 initiallyTriggered,
            int32                 fenceFd);

typedef xcb_void_cookie_t (*XcbDri3PixmapFromBufferChecked)(
            xcb_connection_t*     pConnection,
            xcb_pixmap_t          pixmap,
            xcb_drawable_t        drawable,
            uint32                size,
            uint16                width,
            uint16                height,
            uint16                stride,
            uint8                 depth,
            uint8                 bpp,
            int32                 pixmapFd);

typedef xcb_dri3_query_version_cookie_t (*XcbDri3QueryVersion)(
            xcb_connection_t*     pConnection,
            uint32                majorVersion,
            uint32                minorVersion);

typedef xcb_dri3_query_version_reply_t* (*XcbDri3QueryVersionReply)(
            xcb_connection_t*                 pConnection,
            xcb_dri3_query_version_cookie_t   cookie,
            xcb_generic_error_t**             ppError);

// symbols from libxcb-dri2.so.0
typedef xcb_dri2_connect_cookie_t (*XcbDri2Connect)(
            xcb_connection_t*     pConnection,
            xcb_window_t          window,
            uint32                driver_type);

typedef int (*XcbDri2ConnectDriverNameLength)(
            const xcb_dri2_connect_reply_t*   pReplay);

typedef char* (*XcbDri2ConnectDriverName)(
            const xcb_dri2_connect_reply_t*   pReplay);

typedef xcb_dri2_connect_reply_t* (*XcbDri2ConnectReply)(
            xcb_connection_t*             pConnection,
            xcb_dri2_connect_cookie_t     cookie,
            xcb_generic_error_t**         ppError);

// symbols from libxcb-sync.so.1
typedef xcb_void_cookie_t (*XcbSyncTriggerFenceChecked)(
            xcb_connection_t*     pConnection,
            xcb_sync_fence_t      fence);

typedef xcb_void_cookie_t (*XcbSyncDestroyFenceChecked)(
            xcb_connection_t*     pConnection,
            xcb_sync_fence_t      fence);

// symbols from libX11.so.6
typedef XVisualInfo* (*XGetVisualInfo)(
            Display*      pDisplay,
            uint64        visualMask,
            XVisualInfo*  pVisualInfoList,
            int32*        count);

typedef int32 (*XFree)(
            void*     pAddress);

// symbols from libxcb-present.so.0
typedef xcb_present_query_version_cookie_t (*XcbPresentQueryVersion)(
            xcb_connection_t*     pConnection,
            uint32                majorVersion,
            uint32                minorVersion);

typedef xcb_present_query_version_reply_t* (*XcbPresentQueryVersionReply)(
            xcb_connection_t*                     pConnection,
            xcb_present_query_version_cookie_t    cookie,
            xcb_generic_error_t**                 ppError);

typedef xcb_void_cookie_t (*XcbPresentSelectInputChecked)(
            xcb_connection_t*     pConnection,
            xcb_present_event_t   eventId,
            xcb_window_t          window,
            uint32                eventMask);

typedef xcb_void_cookie_t (*XcbPresentPixmapChecked)(
            xcb_connection_t*             pConnection,
            xcb_window_t                  window,
            xcb_pixmap_t                  pixmap,
            uint32                        serial,
            xcb_xfixes_region_t           valid,
            xcb_xfixes_region_t           update,
            int16                         xOff,
            int16                         yO_off,
            xcb_randr_crtc_t              targetCrtc,
            xcb_sync_fence_t              waitFence,
            xcb_sync_fence_t              idleFence,
            uint32                        options,
            uint64                        targetMsc,
            uint64                        divisor,
            uint64                        remainder,
            uint32                        notifiesLen,
            const xcb_present_notify_t*   pNotifies);

enum Dri3LoaderLibraries : uint32
{
    LibX11Xcb = 0,
    LibXcb = 1,
    LibXshmFence = 2,
    LibXcbDri3 = 3,
    LibXcbDri2 = 4,
    LibXcbSync = 5,
    LibX11 = 6,
    LibXcbPresent = 7,
    Dri3LoaderLibrariesCount = 8
};

struct Dri3LoaderFuncs
{
    XGetXCBConnection                 pfnXGetXCBConnection;
    XcbGenerateId                     pfnXcbGenerateId;
    XcbRegisterForSpecialXge          pfnXcbRegisterForSpecialXge;
    XcbUnregisterForSpecialEvent      pfnXcbUnregisterForSpecialEvent;
    XcbWaitForSpecialEvent            pfnXcbWaitForSpecialEvent;
    XcbGetExtensionData               pfnXcbGetExtensionData;
    XcbPrefetchExtensionData          pfnXcbPrefetchExtensionData;
    XcbRequestCheck                   pfnXcbRequestCheck;
    XcbGetGeometry                    pfnXcbGetGeometry;
    XcbGetGeometryReply               pfnXcbGetGeometryReply;
    XcbFreePixmapChecked              pfnXcbFreePixmapChecked;
    XcbInternAtomReply                pfnXcbInternAtomReply;
    XcbInternAtom                     pfnXcbInternAtom;
    XcbScreenAllowedDepthsIterator    pfnXcbScreenAllowedDepthsIterator;
    XcbDepthNext                      pfnXcbDepthNext;
    XcbVisualtypeNext                 pfnXcbVisualtypeNext;
    XcbSetupRootsIterator             pfnXcbSetupRootsIterator;
    XcbScreenNext                     pfnXcbScreenNext;
    XcbDepthVisualsIterator           pfnXcbDepthVisualsIterator;
    XcbGetSetup                       pfnXcbGetSetup;
    XcbFlush                          pfnXcbFlush;
    XshmfenceUnmapShm                 pfnXshmfenceUnmapShm;
    XshmfenceMapShm                   pfnXshmfenceMapShm;
    XshmfenceQuery                    pfnXshmfenceQuery;
    XshmfenceAllocShm                 pfnXshmfenceAllocShm;
    XshmfenceTrigger                  pfnXshmfenceTrigger;
    XshmfenceReset                    pfnXshmfenceReset;
    XcbDri3Open                       pfnXcbDri3Open;
    XcbDri3OpenReply                  pfnXcbDri3OpenReply;
    XcbDri3OpenReplyFds               pfnXcbDri3OpenReplyFds;
    XcbDri3FenceFromFdChecked         pfnXcbDri3FenceFromFdChecked;
    XcbDri3PixmapFromBufferChecked    pfnXcbDri3PixmapFromBufferChecked;
    XcbDri3QueryVersion               pfnXcbDri3QueryVersion;
    XcbDri3QueryVersionReply          pfnXcbDri3QueryVersionReply;
    XcbDri2Connect                    pfnXcbDri2Connect;
    XcbDri2ConnectDriverNameLength    pfnXcbDri2ConnectDriverNameLength;
    XcbDri2ConnectDriverName          pfnXcbDri2ConnectDriverName;
    XcbDri2ConnectReply               pfnXcbDri2ConnectReply;
    XcbSyncTriggerFenceChecked        pfnXcbSyncTriggerFenceChecked;
    XcbSyncDestroyFenceChecked        pfnXcbSyncDestroyFenceChecked;
    XGetVisualInfo                    pfnXGetVisualInfo;
    XFree                             pfnXFree;
    XcbPresentQueryVersion            pfnXcbPresentQueryVersion;
    XcbPresentQueryVersionReply       pfnXcbPresentQueryVersionReply;
    XcbPresentSelectInputChecked      pfnXcbPresentSelectInputChecked;
    XcbPresentPixmapChecked           pfnXcbPresentPixmapChecked;
};

// =====================================================================================================================
// the class serves as a proxy layer to add more functionality to wrapped callbacks.
#if defined(PAL_DEBUG_PRINTS)
class Dri3LoaderFuncsProxy
{
public:
    Dri3LoaderFuncsProxy() { }
    ~Dri3LoaderFuncsProxy() { }

    void SetFuncCalls(Dri3LoaderFuncs* pFuncs) { m_pFuncs = pFuncs; }

    void Init(const char* pPath);

    xcb_connection_t* pfnXGetXCBConnection(
            Display*  pDisplay) const;

    uint32 pfnXcbGenerateId(
            xcb_connection_t*     pConnection) const;

    xcb_special_event_t* pfnXcbRegisterForSpecialXge(
            xcb_connection_t*     pConnection,
            xcb_extension_t*      pExtensions,
            uint32                eventId,
            uint32*               pStamp) const;

    void pfnXcbUnregisterForSpecialEvent(
            xcb_connection_t*     pConnection,
            xcb_special_event_t*  pEvent) const;

    xcb_generic_event_t* pfnXcbWaitForSpecialEvent(
            xcb_connection_t*     pConnection,
            xcb_special_event*    pEvent) const;

    const xcb_query_extension_reply_t* pfnXcbGetExtensionData(
            xcb_connection_t*     pConnection,
            xcb_extension_t*      pExtension) const;

    void pfnXcbPrefetchExtensionData(
            xcb_connection_t*     pConnection,
            xcb_extension_t*      pExtension) const;

    xcb_generic_error_t* pfnXcbRequestCheck(
            xcb_connection_t*     pConnection,
            xcb_void_cookie_t     cookie) const;

    xcb_get_geometry_cookie_t pfnXcbGetGeometry(
            xcb_connection_t*     pConnection,
            xcb_drawable_t        drawable) const;

    xcb_get_geometry_reply_t* pfnXcbGetGeometryReply(
            xcb_connection_t*             pConnection,
            xcb_get_geometry_cookie_t     cookie,
            xcb_generic_error_t**         ppError) const;

    xcb_void_cookie_t pfnXcbFreePixmapChecked(
            xcb_connection_t*     pConnection,
            xcb_pixmap_t          pixmap) const;

    xcb_intern_atom_reply_t* pfnXcbInternAtomReply(
            xcb_connection_t*         pConnection,
            xcb_intern_atom_cookie_t  cookie,
            xcb_generic_error_t**     ppError) const;

    xcb_intern_atom_cookie_t pfnXcbInternAtom(
            xcb_connection_t*     pConnection,
            uint8                 onlyIfExists,
            uint16                nameLen,
            const char*           pName) const;

    xcb_depth_iterator_t pfnXcbScreenAllowedDepthsIterator(
            const xcb_screen_t*   pScreen) const;

    void pfnXcbDepthNext(
            xcb_depth_iterator_t*     pDepthIter) const;

    void pfnXcbVisualtypeNext(
            xcb_visualtype_iterator_t*    pVisualTypeIter) const;

    xcb_screen_iterator_t pfnXcbSetupRootsIterator(
            const xcb_setup_t*    pSetup) const;

    void pfnXcbScreenNext(
            xcb_screen_iterator_t*    pScreenIter) const;

    xcb_visualtype_iterator_t pfnXcbDepthVisualsIterator(
            const xcb_depth_t*    pDepth) const;

    const xcb_setup_t* pfnXcbGetSetup(
            xcb_connection_t*     pConnection) const;

    const xcb_setup_t* pfnXcbFlush(
            xcb_connection_t*     pConnection) const;

    int32 pfnXshmfenceUnmapShm(
            struct xshmfence*     pFence) const;

    xshmfence* pfnXshmfenceMapShm(
            int32     fence) const;

    int32 pfnXshmfenceQuery(
            struct xshmfence*     pFence) const;

    int32 pfnXshmfenceAllocShm(void) const;

    int32 pfnXshmfenceTrigger(
            struct xshmfence*     pFence) const;

    void pfnXshmfenceReset(
            struct xshmfence*     pFence) const;

    xcb_dri3_open_cookie_t pfnXcbDri3Open(
            xcb_connection_t*     pConnection,
            xcb_drawable_t        drawable,
            uint32                provider) const;

    xcb_dri3_open_reply_t* pfnXcbDri3OpenReply(
            xcb_connection_t*         pConnection,
            xcb_dri3_open_cookie_t    cookie,
            xcb_generic_error_t**     ppError) const;

    int32* pfnXcbDri3OpenReplyFds(
            xcb_connection_t*         pConnection,
            xcb_dri3_open_reply_t*    pReply) const;

    xcb_void_cookie_t pfnXcbDri3FenceFromFdChecked(
            xcb_connection_t*     pConnection,
            xcb_drawable_t        drawable,
            uint32                fence,
            uint8                 initiallyTriggered,
            int32                 fenceFd) const;

    xcb_void_cookie_t pfnXcbDri3PixmapFromBufferChecked(
            xcb_connection_t*     pConnection,
            xcb_pixmap_t          pixmap,
            xcb_drawable_t        drawable,
            uint32                size,
            uint16                width,
            uint16                height,
            uint16                stride,
            uint8                 depth,
            uint8                 bpp,
            int32                 pixmapFd) const;

    xcb_dri3_query_version_cookie_t pfnXcbDri3QueryVersion(
            xcb_connection_t*     pConnection,
            uint32                majorVersion,
            uint32                minorVersion) const;

    xcb_dri3_query_version_reply_t* pfnXcbDri3QueryVersionReply(
            xcb_connection_t*                 pConnection,
            xcb_dri3_query_version_cookie_t   cookie,
            xcb_generic_error_t**             ppError) const;

    xcb_dri2_connect_cookie_t pfnXcbDri2Connect(
            xcb_connection_t*     pConnection,
            xcb_window_t          window,
            uint32                driver_type) const;

    int pfnXcbDri2ConnectDriverNameLength(
            const xcb_dri2_connect_reply_t*   pReplay) const;

    char* pfnXcbDri2ConnectDriverName(
            const xcb_dri2_connect_reply_t*   pReplay) const;

    xcb_dri2_connect_reply_t* pfnXcbDri2ConnectReply(
            xcb_connection_t*             pConnection,
            xcb_dri2_connect_cookie_t     cookie,
            xcb_generic_error_t**         ppError) const;

    xcb_void_cookie_t pfnXcbSyncTriggerFenceChecked(
            xcb_connection_t*     pConnection,
            xcb_sync_fence_t      fence) const;

    xcb_void_cookie_t pfnXcbSyncDestroyFenceChecked(
            xcb_connection_t*     pConnection,
            xcb_sync_fence_t      fence) const;

    XVisualInfo* pfnXGetVisualInfo(
            Display*      pDisplay,
            uint64        visualMask,
            XVisualInfo*  pVisualInfoList,
            int32*        count) const;

    int32 pfnXFree(
            void*     pAddress) const;

    xcb_present_query_version_cookie_t pfnXcbPresentQueryVersion(
            xcb_connection_t*     pConnection,
            uint32                majorVersion,
            uint32                minorVersion) const;

    xcb_present_query_version_reply_t* pfnXcbPresentQueryVersionReply(
            xcb_connection_t*                     pConnection,
            xcb_present_query_version_cookie_t    cookie,
            xcb_generic_error_t**                 ppError) const;

    xcb_void_cookie_t pfnXcbPresentSelectInputChecked(
            xcb_connection_t*     pConnection,
            xcb_present_event_t   eventId,
            xcb_window_t          window,
            uint32                eventMask) const;

    xcb_void_cookie_t pfnXcbPresentPixmapChecked(
            xcb_connection_t*             pConnection,
            xcb_window_t                  window,
            xcb_pixmap_t                  pixmap,
            uint32                        serial,
            xcb_xfixes_region_t           valid,
            xcb_xfixes_region_t           update,
            int16                         xOff,
            int16                         yO_off,
            xcb_randr_crtc_t              targetCrtc,
            xcb_sync_fence_t              waitFence,
            xcb_sync_fence_t              idleFence,
            uint32                        options,
            uint64                        targetMsc,
            uint64                        divisor,
            uint64                        remainder,
            uint32                        notifiesLen,
            const xcb_present_notify_t*   pNotifies) const;

private:
    Util::File  m_timeLogger;
    Util::File  m_paramLogger;
    Dri3LoaderFuncs* m_pFuncs;

    PAL_DISALLOW_COPY_AND_ASSIGN(Dri3LoaderFuncsProxy);
};
#endif

// =====================================================================================================================
// the class is responsible to resolve all external symbols that required by the Dri3WindowSystem.
class Dri3Loader
{
public:
    Dri3Loader();
    ~Dri3Loader();
    bool   Initialized() { return m_initialized; }
    const Dri3LoaderFuncs& GetProcsTable()const { return m_funcs; }
#if defined(PAL_DEBUG_PRINTS)
    const Dri3LoaderFuncsProxy& GetProcsTableProxy()const { return m_proxy; }
    void SetLogPath(const char* pPath) { m_proxy.Init(pPath); }
#endif
    Result Init();

    xcb_extension_t* GetXcbDri3Id() const;

    xcb_extension_t* GetXcbPresentId() const;

private:
    xcb_extension_t* m_pXcbDri3Id;

    xcb_extension_t* m_pXcbPresentId;

    void* m_libraryHandles[Dri3LoaderLibrariesCount];
    bool  m_initialized;
    Dri3LoaderFuncs m_funcs;
#if defined(PAL_DEBUG_PRINTS)
    Dri3LoaderFuncsProxy m_proxy;
#endif

    PAL_DISALLOW_COPY_AND_ASSIGN(Dri3Loader);
};

} //namespace Linux
} //namespace Pal
