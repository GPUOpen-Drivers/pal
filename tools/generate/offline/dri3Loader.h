/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017 Advanced Micro Devices, Inc.
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

typedef int32 (*XshmfenceAwait)(
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
    bool pfnXGetXCBConnectionisValid() const
    {
        return (pfnXGetXCBConnection != nullptr);
    }

    XcbGenerateId                     pfnXcbGenerateId;
    bool pfnXcbGenerateIdisValid() const
    {
        return (pfnXcbGenerateId != nullptr);
    }

    XcbRegisterForSpecialXge          pfnXcbRegisterForSpecialXge;
    bool pfnXcbRegisterForSpecialXgeisValid() const
    {
        return (pfnXcbRegisterForSpecialXge != nullptr);
    }

    XcbUnregisterForSpecialEvent      pfnXcbUnregisterForSpecialEvent;
    bool pfnXcbUnregisterForSpecialEventisValid() const
    {
        return (pfnXcbUnregisterForSpecialEvent != nullptr);
    }

    XcbWaitForSpecialEvent            pfnXcbWaitForSpecialEvent;
    bool pfnXcbWaitForSpecialEventisValid() const
    {
        return (pfnXcbWaitForSpecialEvent != nullptr);
    }

    XcbGetExtensionData               pfnXcbGetExtensionData;
    bool pfnXcbGetExtensionDataisValid() const
    {
        return (pfnXcbGetExtensionData != nullptr);
    }

    XcbPrefetchExtensionData          pfnXcbPrefetchExtensionData;
    bool pfnXcbPrefetchExtensionDataisValid() const
    {
        return (pfnXcbPrefetchExtensionData != nullptr);
    }

    XcbRequestCheck                   pfnXcbRequestCheck;
    bool pfnXcbRequestCheckisValid() const
    {
        return (pfnXcbRequestCheck != nullptr);
    }

    XcbGetGeometry                    pfnXcbGetGeometry;
    bool pfnXcbGetGeometryisValid() const
    {
        return (pfnXcbGetGeometry != nullptr);
    }

    XcbGetGeometryReply               pfnXcbGetGeometryReply;
    bool pfnXcbGetGeometryReplyisValid() const
    {
        return (pfnXcbGetGeometryReply != nullptr);
    }

    XcbFreePixmapChecked              pfnXcbFreePixmapChecked;
    bool pfnXcbFreePixmapCheckedisValid() const
    {
        return (pfnXcbFreePixmapChecked != nullptr);
    }

    XcbInternAtomReply                pfnXcbInternAtomReply;
    bool pfnXcbInternAtomReplyisValid() const
    {
        return (pfnXcbInternAtomReply != nullptr);
    }

    XcbInternAtom                     pfnXcbInternAtom;
    bool pfnXcbInternAtomisValid() const
    {
        return (pfnXcbInternAtom != nullptr);
    }

    XcbScreenAllowedDepthsIterator    pfnXcbScreenAllowedDepthsIterator;
    bool pfnXcbScreenAllowedDepthsIteratorisValid() const
    {
        return (pfnXcbScreenAllowedDepthsIterator != nullptr);
    }

    XcbDepthNext                      pfnXcbDepthNext;
    bool pfnXcbDepthNextisValid() const
    {
        return (pfnXcbDepthNext != nullptr);
    }

    XcbVisualtypeNext                 pfnXcbVisualtypeNext;
    bool pfnXcbVisualtypeNextisValid() const
    {
        return (pfnXcbVisualtypeNext != nullptr);
    }

    XcbSetupRootsIterator             pfnXcbSetupRootsIterator;
    bool pfnXcbSetupRootsIteratorisValid() const
    {
        return (pfnXcbSetupRootsIterator != nullptr);
    }

    XcbScreenNext                     pfnXcbScreenNext;
    bool pfnXcbScreenNextisValid() const
    {
        return (pfnXcbScreenNext != nullptr);
    }

    XcbDepthVisualsIterator           pfnXcbDepthVisualsIterator;
    bool pfnXcbDepthVisualsIteratorisValid() const
    {
        return (pfnXcbDepthVisualsIterator != nullptr);
    }

    XcbGetSetup                       pfnXcbGetSetup;
    bool pfnXcbGetSetupisValid() const
    {
        return (pfnXcbGetSetup != nullptr);
    }

    XcbFlush                          pfnXcbFlush;
    bool pfnXcbFlushisValid() const
    {
        return (pfnXcbFlush != nullptr);
    }

    XshmfenceUnmapShm                 pfnXshmfenceUnmapShm;
    bool pfnXshmfenceUnmapShmisValid() const
    {
        return (pfnXshmfenceUnmapShm != nullptr);
    }

    XshmfenceMapShm                   pfnXshmfenceMapShm;
    bool pfnXshmfenceMapShmisValid() const
    {
        return (pfnXshmfenceMapShm != nullptr);
    }

    XshmfenceQuery                    pfnXshmfenceQuery;
    bool pfnXshmfenceQueryisValid() const
    {
        return (pfnXshmfenceQuery != nullptr);
    }

    XshmfenceAwait                    pfnXshmfenceAwait;
    bool pfnXshmfenceAwaitisValid() const
    {
        return (pfnXshmfenceAwait != nullptr);
    }

    XshmfenceAllocShm                 pfnXshmfenceAllocShm;
    bool pfnXshmfenceAllocShmisValid() const
    {
        return (pfnXshmfenceAllocShm != nullptr);
    }

    XshmfenceTrigger                  pfnXshmfenceTrigger;
    bool pfnXshmfenceTriggerisValid() const
    {
        return (pfnXshmfenceTrigger != nullptr);
    }

    XshmfenceReset                    pfnXshmfenceReset;
    bool pfnXshmfenceResetisValid() const
    {
        return (pfnXshmfenceReset != nullptr);
    }

    XcbDri3Open                       pfnXcbDri3Open;
    bool pfnXcbDri3OpenisValid() const
    {
        return (pfnXcbDri3Open != nullptr);
    }

    XcbDri3OpenReply                  pfnXcbDri3OpenReply;
    bool pfnXcbDri3OpenReplyisValid() const
    {
        return (pfnXcbDri3OpenReply != nullptr);
    }

    XcbDri3OpenReplyFds               pfnXcbDri3OpenReplyFds;
    bool pfnXcbDri3OpenReplyFdsisValid() const
    {
        return (pfnXcbDri3OpenReplyFds != nullptr);
    }

    XcbDri3FenceFromFdChecked         pfnXcbDri3FenceFromFdChecked;
    bool pfnXcbDri3FenceFromFdCheckedisValid() const
    {
        return (pfnXcbDri3FenceFromFdChecked != nullptr);
    }

    XcbDri3PixmapFromBufferChecked    pfnXcbDri3PixmapFromBufferChecked;
    bool pfnXcbDri3PixmapFromBufferCheckedisValid() const
    {
        return (pfnXcbDri3PixmapFromBufferChecked != nullptr);
    }

    XcbDri3QueryVersion               pfnXcbDri3QueryVersion;
    bool pfnXcbDri3QueryVersionisValid() const
    {
        return (pfnXcbDri3QueryVersion != nullptr);
    }

    XcbDri3QueryVersionReply          pfnXcbDri3QueryVersionReply;
    bool pfnXcbDri3QueryVersionReplyisValid() const
    {
        return (pfnXcbDri3QueryVersionReply != nullptr);
    }

    XcbDri2Connect                    pfnXcbDri2Connect;
    bool pfnXcbDri2ConnectisValid() const
    {
        return (pfnXcbDri2Connect != nullptr);
    }

    XcbDri2ConnectDriverNameLength    pfnXcbDri2ConnectDriverNameLength;
    bool pfnXcbDri2ConnectDriverNameLengthisValid() const
    {
        return (pfnXcbDri2ConnectDriverNameLength != nullptr);
    }

    XcbDri2ConnectDriverName          pfnXcbDri2ConnectDriverName;
    bool pfnXcbDri2ConnectDriverNameisValid() const
    {
        return (pfnXcbDri2ConnectDriverName != nullptr);
    }

    XcbDri2ConnectReply               pfnXcbDri2ConnectReply;
    bool pfnXcbDri2ConnectReplyisValid() const
    {
        return (pfnXcbDri2ConnectReply != nullptr);
    }

    XcbSyncTriggerFenceChecked        pfnXcbSyncTriggerFenceChecked;
    bool pfnXcbSyncTriggerFenceCheckedisValid() const
    {
        return (pfnXcbSyncTriggerFenceChecked != nullptr);
    }

    XcbSyncDestroyFenceChecked        pfnXcbSyncDestroyFenceChecked;
    bool pfnXcbSyncDestroyFenceCheckedisValid() const
    {
        return (pfnXcbSyncDestroyFenceChecked != nullptr);
    }

    XGetVisualInfo                    pfnXGetVisualInfo;
    bool pfnXGetVisualInfoisValid() const
    {
        return (pfnXGetVisualInfo != nullptr);
    }

    XFree                             pfnXFree;
    bool pfnXFreeisValid() const
    {
        return (pfnXFree != nullptr);
    }

    XcbPresentQueryVersion            pfnXcbPresentQueryVersion;
    bool pfnXcbPresentQueryVersionisValid() const
    {
        return (pfnXcbPresentQueryVersion != nullptr);
    }

    XcbPresentQueryVersionReply       pfnXcbPresentQueryVersionReply;
    bool pfnXcbPresentQueryVersionReplyisValid() const
    {
        return (pfnXcbPresentQueryVersionReply != nullptr);
    }

    XcbPresentSelectInputChecked      pfnXcbPresentSelectInputChecked;
    bool pfnXcbPresentSelectInputCheckedisValid() const
    {
        return (pfnXcbPresentSelectInputChecked != nullptr);
    }

    XcbPresentPixmapChecked           pfnXcbPresentPixmapChecked;
    bool pfnXcbPresentPixmapCheckedisValid() const
    {
        return (pfnXcbPresentPixmapChecked != nullptr);
    }

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

    bool pfnXGetXCBConnectionisValid() const
    {
        return (m_pFuncs->pfnXGetXCBConnection != nullptr);
    }

    uint32 pfnXcbGenerateId(
            xcb_connection_t*     pConnection) const;

    bool pfnXcbGenerateIdisValid() const
    {
        return (m_pFuncs->pfnXcbGenerateId != nullptr);
    }

    xcb_special_event_t* pfnXcbRegisterForSpecialXge(
            xcb_connection_t*     pConnection,
            xcb_extension_t*      pExtensions,
            uint32                eventId,
            uint32*               pStamp) const;

    bool pfnXcbRegisterForSpecialXgeisValid() const
    {
        return (m_pFuncs->pfnXcbRegisterForSpecialXge != nullptr);
    }

    void pfnXcbUnregisterForSpecialEvent(
            xcb_connection_t*     pConnection,
            xcb_special_event_t*  pEvent) const;

    bool pfnXcbUnregisterForSpecialEventisValid() const
    {
        return (m_pFuncs->pfnXcbUnregisterForSpecialEvent != nullptr);
    }

    xcb_generic_event_t* pfnXcbWaitForSpecialEvent(
            xcb_connection_t*     pConnection,
            xcb_special_event*    pEvent) const;

    bool pfnXcbWaitForSpecialEventisValid() const
    {
        return (m_pFuncs->pfnXcbWaitForSpecialEvent != nullptr);
    }

    const xcb_query_extension_reply_t* pfnXcbGetExtensionData(
            xcb_connection_t*     pConnection,
            xcb_extension_t*      pExtension) const;

    bool pfnXcbGetExtensionDataisValid() const
    {
        return (m_pFuncs->pfnXcbGetExtensionData != nullptr);
    }

    void pfnXcbPrefetchExtensionData(
            xcb_connection_t*     pConnection,
            xcb_extension_t*      pExtension) const;

    bool pfnXcbPrefetchExtensionDataisValid() const
    {
        return (m_pFuncs->pfnXcbPrefetchExtensionData != nullptr);
    }

    xcb_generic_error_t* pfnXcbRequestCheck(
            xcb_connection_t*     pConnection,
            xcb_void_cookie_t     cookie) const;

    bool pfnXcbRequestCheckisValid() const
    {
        return (m_pFuncs->pfnXcbRequestCheck != nullptr);
    }

    xcb_get_geometry_cookie_t pfnXcbGetGeometry(
            xcb_connection_t*     pConnection,
            xcb_drawable_t        drawable) const;

    bool pfnXcbGetGeometryisValid() const
    {
        return (m_pFuncs->pfnXcbGetGeometry != nullptr);
    }

    xcb_get_geometry_reply_t* pfnXcbGetGeometryReply(
            xcb_connection_t*             pConnection,
            xcb_get_geometry_cookie_t     cookie,
            xcb_generic_error_t**         ppError) const;

    bool pfnXcbGetGeometryReplyisValid() const
    {
        return (m_pFuncs->pfnXcbGetGeometryReply != nullptr);
    }

    xcb_void_cookie_t pfnXcbFreePixmapChecked(
            xcb_connection_t*     pConnection,
            xcb_pixmap_t          pixmap) const;

    bool pfnXcbFreePixmapCheckedisValid() const
    {
        return (m_pFuncs->pfnXcbFreePixmapChecked != nullptr);
    }

    xcb_intern_atom_reply_t* pfnXcbInternAtomReply(
            xcb_connection_t*         pConnection,
            xcb_intern_atom_cookie_t  cookie,
            xcb_generic_error_t**     ppError) const;

    bool pfnXcbInternAtomReplyisValid() const
    {
        return (m_pFuncs->pfnXcbInternAtomReply != nullptr);
    }

    xcb_intern_atom_cookie_t pfnXcbInternAtom(
            xcb_connection_t*     pConnection,
            uint8                 onlyIfExists,
            uint16                nameLen,
            const char*           pName) const;

    bool pfnXcbInternAtomisValid() const
    {
        return (m_pFuncs->pfnXcbInternAtom != nullptr);
    }

    xcb_depth_iterator_t pfnXcbScreenAllowedDepthsIterator(
            const xcb_screen_t*   pScreen) const;

    bool pfnXcbScreenAllowedDepthsIteratorisValid() const
    {
        return (m_pFuncs->pfnXcbScreenAllowedDepthsIterator != nullptr);
    }

    void pfnXcbDepthNext(
            xcb_depth_iterator_t*     pDepthIter) const;

    bool pfnXcbDepthNextisValid() const
    {
        return (m_pFuncs->pfnXcbDepthNext != nullptr);
    }

    void pfnXcbVisualtypeNext(
            xcb_visualtype_iterator_t*    pVisualTypeIter) const;

    bool pfnXcbVisualtypeNextisValid() const
    {
        return (m_pFuncs->pfnXcbVisualtypeNext != nullptr);
    }

    xcb_screen_iterator_t pfnXcbSetupRootsIterator(
            const xcb_setup_t*    pSetup) const;

    bool pfnXcbSetupRootsIteratorisValid() const
    {
        return (m_pFuncs->pfnXcbSetupRootsIterator != nullptr);
    }

    void pfnXcbScreenNext(
            xcb_screen_iterator_t*    pScreenIter) const;

    bool pfnXcbScreenNextisValid() const
    {
        return (m_pFuncs->pfnXcbScreenNext != nullptr);
    }

    xcb_visualtype_iterator_t pfnXcbDepthVisualsIterator(
            const xcb_depth_t*    pDepth) const;

    bool pfnXcbDepthVisualsIteratorisValid() const
    {
        return (m_pFuncs->pfnXcbDepthVisualsIterator != nullptr);
    }

    const xcb_setup_t* pfnXcbGetSetup(
            xcb_connection_t*     pConnection) const;

    bool pfnXcbGetSetupisValid() const
    {
        return (m_pFuncs->pfnXcbGetSetup != nullptr);
    }

    const xcb_setup_t* pfnXcbFlush(
            xcb_connection_t*     pConnection) const;

    bool pfnXcbFlushisValid() const
    {
        return (m_pFuncs->pfnXcbFlush != nullptr);
    }

    int32 pfnXshmfenceUnmapShm(
            struct xshmfence*     pFence) const;

    bool pfnXshmfenceUnmapShmisValid() const
    {
        return (m_pFuncs->pfnXshmfenceUnmapShm != nullptr);
    }

    xshmfence* pfnXshmfenceMapShm(
            int32     fence) const;

    bool pfnXshmfenceMapShmisValid() const
    {
        return (m_pFuncs->pfnXshmfenceMapShm != nullptr);
    }

    int32 pfnXshmfenceQuery(
            struct xshmfence*     pFence) const;

    bool pfnXshmfenceQueryisValid() const
    {
        return (m_pFuncs->pfnXshmfenceQuery != nullptr);
    }

    int32 pfnXshmfenceAwait(
            struct xshmfence*     pFence) const;

    bool pfnXshmfenceAwaitisValid() const
    {
        return (m_pFuncs->pfnXshmfenceAwait != nullptr);
    }

    int32 pfnXshmfenceAllocShm(void) const;

    int32 pfnXshmfenceTrigger(
            struct xshmfence*     pFence) const;

    bool pfnXshmfenceTriggerisValid() const
    {
        return (m_pFuncs->pfnXshmfenceTrigger != nullptr);
    }

    void pfnXshmfenceReset(
            struct xshmfence*     pFence) const;

    bool pfnXshmfenceResetisValid() const
    {
        return (m_pFuncs->pfnXshmfenceReset != nullptr);
    }

    xcb_dri3_open_cookie_t pfnXcbDri3Open(
            xcb_connection_t*     pConnection,
            xcb_drawable_t        drawable,
            uint32                provider) const;

    bool pfnXcbDri3OpenisValid() const
    {
        return (m_pFuncs->pfnXcbDri3Open != nullptr);
    }

    xcb_dri3_open_reply_t* pfnXcbDri3OpenReply(
            xcb_connection_t*         pConnection,
            xcb_dri3_open_cookie_t    cookie,
            xcb_generic_error_t**     ppError) const;

    bool pfnXcbDri3OpenReplyisValid() const
    {
        return (m_pFuncs->pfnXcbDri3OpenReply != nullptr);
    }

    int32* pfnXcbDri3OpenReplyFds(
            xcb_connection_t*         pConnection,
            xcb_dri3_open_reply_t*    pReply) const;

    bool pfnXcbDri3OpenReplyFdsisValid() const
    {
        return (m_pFuncs->pfnXcbDri3OpenReplyFds != nullptr);
    }

    xcb_void_cookie_t pfnXcbDri3FenceFromFdChecked(
            xcb_connection_t*     pConnection,
            xcb_drawable_t        drawable,
            uint32                fence,
            uint8                 initiallyTriggered,
            int32                 fenceFd) const;

    bool pfnXcbDri3FenceFromFdCheckedisValid() const
    {
        return (m_pFuncs->pfnXcbDri3FenceFromFdChecked != nullptr);
    }

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

    bool pfnXcbDri3PixmapFromBufferCheckedisValid() const
    {
        return (m_pFuncs->pfnXcbDri3PixmapFromBufferChecked != nullptr);
    }

    xcb_dri3_query_version_cookie_t pfnXcbDri3QueryVersion(
            xcb_connection_t*     pConnection,
            uint32                majorVersion,
            uint32                minorVersion) const;

    bool pfnXcbDri3QueryVersionisValid() const
    {
        return (m_pFuncs->pfnXcbDri3QueryVersion != nullptr);
    }

    xcb_dri3_query_version_reply_t* pfnXcbDri3QueryVersionReply(
            xcb_connection_t*                 pConnection,
            xcb_dri3_query_version_cookie_t   cookie,
            xcb_generic_error_t**             ppError) const;

    bool pfnXcbDri3QueryVersionReplyisValid() const
    {
        return (m_pFuncs->pfnXcbDri3QueryVersionReply != nullptr);
    }

    xcb_dri2_connect_cookie_t pfnXcbDri2Connect(
            xcb_connection_t*     pConnection,
            xcb_window_t          window,
            uint32                driver_type) const;

    bool pfnXcbDri2ConnectisValid() const
    {
        return (m_pFuncs->pfnXcbDri2Connect != nullptr);
    }

    int pfnXcbDri2ConnectDriverNameLength(
            const xcb_dri2_connect_reply_t*   pReplay) const;

    bool pfnXcbDri2ConnectDriverNameLengthisValid() const
    {
        return (m_pFuncs->pfnXcbDri2ConnectDriverNameLength != nullptr);
    }

    char* pfnXcbDri2ConnectDriverName(
            const xcb_dri2_connect_reply_t*   pReplay) const;

    bool pfnXcbDri2ConnectDriverNameisValid() const
    {
        return (m_pFuncs->pfnXcbDri2ConnectDriverName != nullptr);
    }

    xcb_dri2_connect_reply_t* pfnXcbDri2ConnectReply(
            xcb_connection_t*             pConnection,
            xcb_dri2_connect_cookie_t     cookie,
            xcb_generic_error_t**         ppError) const;

    bool pfnXcbDri2ConnectReplyisValid() const
    {
        return (m_pFuncs->pfnXcbDri2ConnectReply != nullptr);
    }

    xcb_void_cookie_t pfnXcbSyncTriggerFenceChecked(
            xcb_connection_t*     pConnection,
            xcb_sync_fence_t      fence) const;

    bool pfnXcbSyncTriggerFenceCheckedisValid() const
    {
        return (m_pFuncs->pfnXcbSyncTriggerFenceChecked != nullptr);
    }

    xcb_void_cookie_t pfnXcbSyncDestroyFenceChecked(
            xcb_connection_t*     pConnection,
            xcb_sync_fence_t      fence) const;

    bool pfnXcbSyncDestroyFenceCheckedisValid() const
    {
        return (m_pFuncs->pfnXcbSyncDestroyFenceChecked != nullptr);
    }

    XVisualInfo* pfnXGetVisualInfo(
            Display*      pDisplay,
            uint64        visualMask,
            XVisualInfo*  pVisualInfoList,
            int32*        count) const;

    bool pfnXGetVisualInfoisValid() const
    {
        return (m_pFuncs->pfnXGetVisualInfo != nullptr);
    }

    int32 pfnXFree(
            void*     pAddress) const;

    bool pfnXFreeisValid() const
    {
        return (m_pFuncs->pfnXFree != nullptr);
    }

    xcb_present_query_version_cookie_t pfnXcbPresentQueryVersion(
            xcb_connection_t*     pConnection,
            uint32                majorVersion,
            uint32                minorVersion) const;

    bool pfnXcbPresentQueryVersionisValid() const
    {
        return (m_pFuncs->pfnXcbPresentQueryVersion != nullptr);
    }

    xcb_present_query_version_reply_t* pfnXcbPresentQueryVersionReply(
            xcb_connection_t*                     pConnection,
            xcb_present_query_version_cookie_t    cookie,
            xcb_generic_error_t**                 ppError) const;

    bool pfnXcbPresentQueryVersionReplyisValid() const
    {
        return (m_pFuncs->pfnXcbPresentQueryVersionReply != nullptr);
    }

    xcb_void_cookie_t pfnXcbPresentSelectInputChecked(
            xcb_connection_t*     pConnection,
            xcb_present_event_t   eventId,
            xcb_window_t          window,
            uint32                eventMask) const;

    bool pfnXcbPresentSelectInputCheckedisValid() const
    {
        return (m_pFuncs->pfnXcbPresentSelectInputChecked != nullptr);
    }

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

    bool pfnXcbPresentPixmapCheckedisValid() const
    {
        return (m_pFuncs->pfnXcbPresentPixmapChecked != nullptr);
    }

private:
    Util::File  m_timeLogger;
    Util::File  m_paramLogger;
    Dri3LoaderFuncs* m_pFuncs;

    PAL_DISALLOW_COPY_AND_ASSIGN(Dri3LoaderFuncsProxy);
};
#endif

class Platform;
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
    Result Init(Platform* pPlatform);

    xcb_extension_t* GetXcbDri3Id() const;

    xcb_extension_t* GetXcbPresentId() const;

    xcb_extension_t* GetXcbDri2Id() const;

private:
    xcb_extension_t* m_pXcbDri3Id;

    xcb_extension_t* m_pXcbPresentId;

    xcb_extension_t* m_pXcbDri2Id;

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
