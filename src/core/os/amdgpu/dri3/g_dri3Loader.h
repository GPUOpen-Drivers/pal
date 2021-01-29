/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
// Modify the procAnalysis.py and dri3Loader.py in the tools/generate directory OR dri3WindowSystem.proc instead
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
#include <xcb/randr.h>
extern "C"
{
    #include <X11/xshmfence.h>
}

#include "palFile.h"
#include "palLibrary.h"

#define XCB_RANDR_SUPPORTS_LEASE ((XCB_RANDR_MAJOR_VERSION > 1) || \
                                  ((XCB_RANDR_MAJOR_VERSION == 1) && (XCB_RANDR_MINOR_VERSION >= 6)))

using namespace Util;
namespace Pal
{
namespace Amdgpu
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

typedef xcb_generic_event_t* (*XcbPollForSpecialEvent)(
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

typedef void (*XcbDiscardReply)(
            xcb_connection_t*     pConnection,
            uint32                sequence);

typedef xcb_void_cookie_t (*XcbChangePropertyChecked)(
            xcb_connection_t*     pConnection,
            uint8_t               mode,
            xcb_window_t          window,
            xcb_atom_t            property,
            xcb_atom_t            type,
            uint8_t               format,
            uint32_t              data_len,
            const void *          pData);

typedef xcb_void_cookie_t (*XcbDeletePropertyChecked)(
            xcb_connection_t*     pConnection,
            xcb_window_t          window,
            xcb_atom_t            property);

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

// symbols from libxcb-randr.so.0
#if XCB_RANDR_SUPPORTS_LEASE
typedef xcb_randr_create_lease_cookie_t (*XcbRandrCreateLease)(
            xcb_connection_t*             pConnection,
            xcb_window_t                  window,
            xcb_randr_lease_t             leaseId,
            uint16_t                      numCrtcs,
            uint16_t                      numOutputs,
            const xcb_randr_crtc_t*       pCrtcs,
            const xcb_randr_output_t*     pOutputs);

typedef xcb_randr_create_lease_reply_t* (*XcbRandrCreateLeaseReply)(
            xcb_connection_t*                 pConnection,
            xcb_randr_create_lease_cookie_t   cookie,
            xcb_generic_error_t**             ppError);

typedef int* (*XcbRandrCreateLeaseReplyFds)(
            xcb_connection_t*                 pConnection,
            xcb_randr_create_lease_reply_t*   pReply);
#endif

typedef xcb_randr_get_screen_resources_cookie_t (*XcbRandrGetScreenResources)(
            xcb_connection_t*     pConnection,
            xcb_window_t          window);

typedef xcb_randr_get_screen_resources_reply_t* (*XcbRandrGetScreenResourcesReply)(
            xcb_connection_t*                         pConnection,
            xcb_randr_get_screen_resources_cookie_t   cookie,
            xcb_generic_error_t**                     ppError);

typedef xcb_randr_output_t* (*XcbRandrGetScreenResourcesOutputs)(
            const xcb_randr_get_screen_resources_reply_t*     pScrResReply);

typedef xcb_randr_crtc_t* (*XcbRandrGetScreenResourcesCrtcs)(
            const xcb_randr_get_screen_resources_reply_t*     pScrResReply);

typedef xcb_randr_get_crtc_info_cookie_t (*XcbRandrGetCrtcInfo)(
            xcb_connection_t*     pConnection,
            xcb_randr_crtc_t      output,
            xcb_timestamp_t       configTimestamp);

typedef xcb_randr_get_crtc_info_reply_t* (*XcbRandrGetCrtcInfoReply)(
            xcb_connection_t*                 pConnection,
            xcb_randr_get_crtc_info_cookie_t  cookie,
            xcb_generic_error_t**             ppError);

typedef xcb_randr_get_output_info_cookie_t (*XcbRandrGetOutputInfo)(
            xcb_connection_t*     pConnection,
            xcb_randr_output_t    output,
            xcb_timestamp_t       configTimestamp);

typedef xcb_randr_get_output_info_reply_t* (*XcbRandrGetOutputInfoReply)(
            xcb_connection_t*                     pConnection,
            xcb_randr_get_output_info_cookie_t    cookie,
            xcb_generic_error_t**                 ppError);

typedef xcb_randr_output_t* (*XcbRandrGetCrtcInfoOutputs)(
            xcb_randr_get_crtc_info_reply_t*  pCrtcInfoReply);

typedef xcb_randr_output_t* (*XcbRandrGetCrtcInfoPossible)(
            xcb_randr_get_crtc_info_reply_t*  pCrtcInfoReply);

typedef xcb_randr_get_output_property_cookie_t (*XcbRandrGetOutputProperty)(
            xcb_connection_t*     pConnection,
            xcb_randr_output_t    output,
            xcb_atom_t            property,
            xcb_atom_t            type,
            uint32_t              offset,
            uint32_t              length,
            uint8_t               _delete,
            uint8_t               pending);

typedef uint8_t* (*XcbRandrGetOutputPropertyData)(
            const xcb_randr_get_output_property_reply_t*  pReply);

typedef xcb_randr_get_output_property_reply_t* (*XcbRandrGetOutputPropertyReply)(
            xcb_connection_t*                         pConnection,
            xcb_randr_get_output_property_cookie_t    cookie,
            xcb_generic_error_t**                     ppError);

typedef xcb_randr_get_providers_cookie_t (*XcbRandrGetProviders)(
            xcb_connection_t*     c,
            xcb_window_t          window);

typedef xcb_randr_get_providers_reply_t* (*XcbRandrGetProvidersReply)(
            xcb_connection_t*                 c,
            xcb_randr_get_providers_cookie_t  cookie,
            xcb_generic_error_t**             e);

typedef xcb_randr_provider_t* (*XcbRandrGetProvidersProviders)(
            const xcb_randr_get_providers_reply_t*    R);

typedef int (*XcbRandrGetProvidersProvidersLength)(
            const xcb_randr_get_providers_reply_t*    R);

typedef xcb_randr_get_provider_info_cookie_t (*XcbRandrGetProviderInfo)(
            xcb_connection_t*     c,
            xcb_randr_provider_t  provider,
            xcb_timestamp_t       config_timestamp);

typedef xcb_randr_get_provider_info_reply_t* (*XcbRandrGetProviderInfoReply)(
            xcb_connection_t*                     c,
            xcb_randr_get_provider_info_cookie_t  cookie,
            xcb_generic_error_t**                 e);

typedef char* (*XcbRandrGetProviderInfoName)(
            const xcb_randr_get_provider_info_reply_t*    R);

typedef xcb_randr_query_version_cookie_t (*XcbRandrQueryVersion)(
            xcb_connection_t*     c,
            uint32_t              major_version,
            uint32_t              minor_version);

typedef xcb_randr_query_version_reply_t* (*XcbRandrQueryVersionReply)(
            xcb_connection_t*                 c,
            xcb_randr_query_version_cookie_t  cookie,
            xcb_generic_error_t**             e);

typedef xcb_query_tree_cookie_t (*XcbQueryTree)(
            xcb_connection_t*     c,
            xcb_window_t          window);

typedef xcb_query_tree_reply_t* (*XcbQueryTreeReply)(
            xcb_connection_t *        c,
            xcb_query_tree_cookie_t   cookie,
            xcb_generic_error_t **    e);

typedef xcb_get_window_attributes_cookie_t (*XcbGetWindowAttributes)(
            xcb_connection_t *    c,
            xcb_window_t          window);

typedef xcb_get_window_attributes_reply_t* (*XcbGetWindowAttributesReply)(
            xcb_connection_t *                    c,
            xcb_get_window_attributes_cookie_t    cookie,
            xcb_generic_error_t **                e);

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
            long          visualMask,
            XVisualInfo*  pVisualInfoList,
            int32*        count);

typedef int32 (*XFree)(
            void*     pAddress);

typedef Window (*XRootWindow)(
            Display*  pDisplay,
            int       screenNum);

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
    LibXcbRandr = 5,
    LibXcbSync = 6,
    LibX11 = 7,
    LibXcbPresent = 8,
    Dri3LoaderLibrariesCount = 9
};

struct Dri3LoaderFuncs
{
    XGetXCBConnection                     pfnXGetXCBConnection;
    bool pfnXGetXCBConnectionisValid() const
    {
        return (pfnXGetXCBConnection != nullptr);
    }

    XcbGenerateId                         pfnXcbGenerateId;
    bool pfnXcbGenerateIdisValid() const
    {
        return (pfnXcbGenerateId != nullptr);
    }

    XcbRegisterForSpecialXge              pfnXcbRegisterForSpecialXge;
    bool pfnXcbRegisterForSpecialXgeisValid() const
    {
        return (pfnXcbRegisterForSpecialXge != nullptr);
    }

    XcbUnregisterForSpecialEvent          pfnXcbUnregisterForSpecialEvent;
    bool pfnXcbUnregisterForSpecialEventisValid() const
    {
        return (pfnXcbUnregisterForSpecialEvent != nullptr);
    }

    XcbWaitForSpecialEvent                pfnXcbWaitForSpecialEvent;
    bool pfnXcbWaitForSpecialEventisValid() const
    {
        return (pfnXcbWaitForSpecialEvent != nullptr);
    }

    XcbPollForSpecialEvent                pfnXcbPollForSpecialEvent;
    bool pfnXcbPollForSpecialEventisValid() const
    {
        return (pfnXcbPollForSpecialEvent != nullptr);
    }

    XcbGetExtensionData                   pfnXcbGetExtensionData;
    bool pfnXcbGetExtensionDataisValid() const
    {
        return (pfnXcbGetExtensionData != nullptr);
    }

    XcbPrefetchExtensionData              pfnXcbPrefetchExtensionData;
    bool pfnXcbPrefetchExtensionDataisValid() const
    {
        return (pfnXcbPrefetchExtensionData != nullptr);
    }

    XcbRequestCheck                       pfnXcbRequestCheck;
    bool pfnXcbRequestCheckisValid() const
    {
        return (pfnXcbRequestCheck != nullptr);
    }

    XcbGetGeometry                        pfnXcbGetGeometry;
    bool pfnXcbGetGeometryisValid() const
    {
        return (pfnXcbGetGeometry != nullptr);
    }

    XcbGetGeometryReply                   pfnXcbGetGeometryReply;
    bool pfnXcbGetGeometryReplyisValid() const
    {
        return (pfnXcbGetGeometryReply != nullptr);
    }

    XcbFreePixmapChecked                  pfnXcbFreePixmapChecked;
    bool pfnXcbFreePixmapCheckedisValid() const
    {
        return (pfnXcbFreePixmapChecked != nullptr);
    }

    XcbInternAtomReply                    pfnXcbInternAtomReply;
    bool pfnXcbInternAtomReplyisValid() const
    {
        return (pfnXcbInternAtomReply != nullptr);
    }

    XcbInternAtom                         pfnXcbInternAtom;
    bool pfnXcbInternAtomisValid() const
    {
        return (pfnXcbInternAtom != nullptr);
    }

    XcbScreenAllowedDepthsIterator        pfnXcbScreenAllowedDepthsIterator;
    bool pfnXcbScreenAllowedDepthsIteratorisValid() const
    {
        return (pfnXcbScreenAllowedDepthsIterator != nullptr);
    }

    XcbDepthNext                          pfnXcbDepthNext;
    bool pfnXcbDepthNextisValid() const
    {
        return (pfnXcbDepthNext != nullptr);
    }

    XcbVisualtypeNext                     pfnXcbVisualtypeNext;
    bool pfnXcbVisualtypeNextisValid() const
    {
        return (pfnXcbVisualtypeNext != nullptr);
    }

    XcbSetupRootsIterator                 pfnXcbSetupRootsIterator;
    bool pfnXcbSetupRootsIteratorisValid() const
    {
        return (pfnXcbSetupRootsIterator != nullptr);
    }

    XcbScreenNext                         pfnXcbScreenNext;
    bool pfnXcbScreenNextisValid() const
    {
        return (pfnXcbScreenNext != nullptr);
    }

    XcbDepthVisualsIterator               pfnXcbDepthVisualsIterator;
    bool pfnXcbDepthVisualsIteratorisValid() const
    {
        return (pfnXcbDepthVisualsIterator != nullptr);
    }

    XcbGetSetup                           pfnXcbGetSetup;
    bool pfnXcbGetSetupisValid() const
    {
        return (pfnXcbGetSetup != nullptr);
    }

    XcbFlush                              pfnXcbFlush;
    bool pfnXcbFlushisValid() const
    {
        return (pfnXcbFlush != nullptr);
    }

    XcbDiscardReply                       pfnXcbDiscardReply;
    bool pfnXcbDiscardReplyisValid() const
    {
        return (pfnXcbDiscardReply != nullptr);
    }

    XcbChangePropertyChecked              pfnXcbChangePropertyChecked;
    bool pfnXcbChangePropertyCheckedisValid() const
    {
        return (pfnXcbChangePropertyChecked != nullptr);
    }

    XcbDeletePropertyChecked              pfnXcbDeletePropertyChecked;
    bool pfnXcbDeletePropertyCheckedisValid() const
    {
        return (pfnXcbDeletePropertyChecked != nullptr);
    }

    XshmfenceUnmapShm                     pfnXshmfenceUnmapShm;
    bool pfnXshmfenceUnmapShmisValid() const
    {
        return (pfnXshmfenceUnmapShm != nullptr);
    }

    XshmfenceMapShm                       pfnXshmfenceMapShm;
    bool pfnXshmfenceMapShmisValid() const
    {
        return (pfnXshmfenceMapShm != nullptr);
    }

    XshmfenceQuery                        pfnXshmfenceQuery;
    bool pfnXshmfenceQueryisValid() const
    {
        return (pfnXshmfenceQuery != nullptr);
    }

    XshmfenceAwait                        pfnXshmfenceAwait;
    bool pfnXshmfenceAwaitisValid() const
    {
        return (pfnXshmfenceAwait != nullptr);
    }

    XshmfenceAllocShm                     pfnXshmfenceAllocShm;
    bool pfnXshmfenceAllocShmisValid() const
    {
        return (pfnXshmfenceAllocShm != nullptr);
    }

    XshmfenceTrigger                      pfnXshmfenceTrigger;
    bool pfnXshmfenceTriggerisValid() const
    {
        return (pfnXshmfenceTrigger != nullptr);
    }

    XshmfenceReset                        pfnXshmfenceReset;
    bool pfnXshmfenceResetisValid() const
    {
        return (pfnXshmfenceReset != nullptr);
    }

    XcbDri3Open                           pfnXcbDri3Open;
    bool pfnXcbDri3OpenisValid() const
    {
        return (pfnXcbDri3Open != nullptr);
    }

    XcbDri3OpenReply                      pfnXcbDri3OpenReply;
    bool pfnXcbDri3OpenReplyisValid() const
    {
        return (pfnXcbDri3OpenReply != nullptr);
    }

    XcbDri3OpenReplyFds                   pfnXcbDri3OpenReplyFds;
    bool pfnXcbDri3OpenReplyFdsisValid() const
    {
        return (pfnXcbDri3OpenReplyFds != nullptr);
    }

    XcbDri3FenceFromFdChecked             pfnXcbDri3FenceFromFdChecked;
    bool pfnXcbDri3FenceFromFdCheckedisValid() const
    {
        return (pfnXcbDri3FenceFromFdChecked != nullptr);
    }

    XcbDri3PixmapFromBufferChecked        pfnXcbDri3PixmapFromBufferChecked;
    bool pfnXcbDri3PixmapFromBufferCheckedisValid() const
    {
        return (pfnXcbDri3PixmapFromBufferChecked != nullptr);
    }

    XcbDri3QueryVersion                   pfnXcbDri3QueryVersion;
    bool pfnXcbDri3QueryVersionisValid() const
    {
        return (pfnXcbDri3QueryVersion != nullptr);
    }

    XcbDri3QueryVersionReply              pfnXcbDri3QueryVersionReply;
    bool pfnXcbDri3QueryVersionReplyisValid() const
    {
        return (pfnXcbDri3QueryVersionReply != nullptr);
    }

    XcbDri2Connect                        pfnXcbDri2Connect;
    bool pfnXcbDri2ConnectisValid() const
    {
        return (pfnXcbDri2Connect != nullptr);
    }

    XcbDri2ConnectDriverNameLength        pfnXcbDri2ConnectDriverNameLength;
    bool pfnXcbDri2ConnectDriverNameLengthisValid() const
    {
        return (pfnXcbDri2ConnectDriverNameLength != nullptr);
    }

    XcbDri2ConnectDriverName              pfnXcbDri2ConnectDriverName;
    bool pfnXcbDri2ConnectDriverNameisValid() const
    {
        return (pfnXcbDri2ConnectDriverName != nullptr);
    }

    XcbDri2ConnectReply                   pfnXcbDri2ConnectReply;
    bool pfnXcbDri2ConnectReplyisValid() const
    {
        return (pfnXcbDri2ConnectReply != nullptr);
    }

#if XCB_RANDR_SUPPORTS_LEASE
    XcbRandrCreateLease                   pfnXcbRandrCreateLease;
    bool pfnXcbRandrCreateLeaseisValid() const
    {
        return (pfnXcbRandrCreateLease != nullptr);
    }

    XcbRandrCreateLeaseReply              pfnXcbRandrCreateLeaseReply;
    bool pfnXcbRandrCreateLeaseReplyisValid() const
    {
        return (pfnXcbRandrCreateLeaseReply != nullptr);
    }

    XcbRandrCreateLeaseReplyFds           pfnXcbRandrCreateLeaseReplyFds;
    bool pfnXcbRandrCreateLeaseReplyFdsisValid() const
    {
        return (pfnXcbRandrCreateLeaseReplyFds != nullptr);
    }
#endif

    XcbRandrGetScreenResources            pfnXcbRandrGetScreenResources;
    bool pfnXcbRandrGetScreenResourcesisValid() const
    {
        return (pfnXcbRandrGetScreenResources != nullptr);
    }

    XcbRandrGetScreenResourcesReply       pfnXcbRandrGetScreenResourcesReply;
    bool pfnXcbRandrGetScreenResourcesReplyisValid() const
    {
        return (pfnXcbRandrGetScreenResourcesReply != nullptr);
    }

    XcbRandrGetScreenResourcesOutputs     pfnXcbRandrGetScreenResourcesOutputs;
    bool pfnXcbRandrGetScreenResourcesOutputsisValid() const
    {
        return (pfnXcbRandrGetScreenResourcesOutputs != nullptr);
    }

    XcbRandrGetScreenResourcesCrtcs       pfnXcbRandrGetScreenResourcesCrtcs;
    bool pfnXcbRandrGetScreenResourcesCrtcsisValid() const
    {
        return (pfnXcbRandrGetScreenResourcesCrtcs != nullptr);
    }

    XcbRandrGetCrtcInfo                   pfnXcbRandrGetCrtcInfo;
    bool pfnXcbRandrGetCrtcInfoisValid() const
    {
        return (pfnXcbRandrGetCrtcInfo != nullptr);
    }

    XcbRandrGetCrtcInfoReply              pfnXcbRandrGetCrtcInfoReply;
    bool pfnXcbRandrGetCrtcInfoReplyisValid() const
    {
        return (pfnXcbRandrGetCrtcInfoReply != nullptr);
    }

    XcbRandrGetOutputInfo                 pfnXcbRandrGetOutputInfo;
    bool pfnXcbRandrGetOutputInfoisValid() const
    {
        return (pfnXcbRandrGetOutputInfo != nullptr);
    }

    XcbRandrGetOutputInfoReply            pfnXcbRandrGetOutputInfoReply;
    bool pfnXcbRandrGetOutputInfoReplyisValid() const
    {
        return (pfnXcbRandrGetOutputInfoReply != nullptr);
    }

    XcbRandrGetCrtcInfoOutputs            pfnXcbRandrGetCrtcInfoOutputs;
    bool pfnXcbRandrGetCrtcInfoOutputsisValid() const
    {
        return (pfnXcbRandrGetCrtcInfoOutputs != nullptr);
    }

    XcbRandrGetCrtcInfoPossible           pfnXcbRandrGetCrtcInfoPossible;
    bool pfnXcbRandrGetCrtcInfoPossibleisValid() const
    {
        return (pfnXcbRandrGetCrtcInfoPossible != nullptr);
    }

    XcbRandrGetOutputProperty             pfnXcbRandrGetOutputProperty;
    bool pfnXcbRandrGetOutputPropertyisValid() const
    {
        return (pfnXcbRandrGetOutputProperty != nullptr);
    }

    XcbRandrGetOutputPropertyData         pfnXcbRandrGetOutputPropertyData;
    bool pfnXcbRandrGetOutputPropertyDataisValid() const
    {
        return (pfnXcbRandrGetOutputPropertyData != nullptr);
    }

    XcbRandrGetOutputPropertyReply        pfnXcbRandrGetOutputPropertyReply;
    bool pfnXcbRandrGetOutputPropertyReplyisValid() const
    {
        return (pfnXcbRandrGetOutputPropertyReply != nullptr);
    }

    XcbRandrGetProviders                  pfnXcbRandrGetProviders;
    bool pfnXcbRandrGetProvidersisValid() const
    {
        return (pfnXcbRandrGetProviders != nullptr);
    }

    XcbRandrGetProvidersReply             pfnXcbRandrGetProvidersReply;
    bool pfnXcbRandrGetProvidersReplyisValid() const
    {
        return (pfnXcbRandrGetProvidersReply != nullptr);
    }

    XcbRandrGetProvidersProviders         pfnXcbRandrGetProvidersProviders;
    bool pfnXcbRandrGetProvidersProvidersisValid() const
    {
        return (pfnXcbRandrGetProvidersProviders != nullptr);
    }

    XcbRandrGetProvidersProvidersLength   pfnXcbRandrGetProvidersProvidersLength;
    bool pfnXcbRandrGetProvidersProvidersLengthisValid() const
    {
        return (pfnXcbRandrGetProvidersProvidersLength != nullptr);
    }

    XcbRandrGetProviderInfo               pfnXcbRandrGetProviderInfo;
    bool pfnXcbRandrGetProviderInfoisValid() const
    {
        return (pfnXcbRandrGetProviderInfo != nullptr);
    }

    XcbRandrGetProviderInfoReply          pfnXcbRandrGetProviderInfoReply;
    bool pfnXcbRandrGetProviderInfoReplyisValid() const
    {
        return (pfnXcbRandrGetProviderInfoReply != nullptr);
    }

    XcbRandrGetProviderInfoName           pfnXcbRandrGetProviderInfoName;
    bool pfnXcbRandrGetProviderInfoNameisValid() const
    {
        return (pfnXcbRandrGetProviderInfoName != nullptr);
    }

    XcbRandrQueryVersion                  pfnXcbRandrQueryVersion;
    bool pfnXcbRandrQueryVersionisValid() const
    {
        return (pfnXcbRandrQueryVersion != nullptr);
    }

    XcbRandrQueryVersionReply             pfnXcbRandrQueryVersionReply;
    bool pfnXcbRandrQueryVersionReplyisValid() const
    {
        return (pfnXcbRandrQueryVersionReply != nullptr);
    }

    XcbQueryTree                          pfnXcbQueryTree;
    bool pfnXcbQueryTreeisValid() const
    {
        return (pfnXcbQueryTree != nullptr);
    }

    XcbQueryTreeReply                     pfnXcbQueryTreeReply;
    bool pfnXcbQueryTreeReplyisValid() const
    {
        return (pfnXcbQueryTreeReply != nullptr);
    }

    XcbGetWindowAttributes                pfnXcbGetWindowAttributes;
    bool pfnXcbGetWindowAttributesisValid() const
    {
        return (pfnXcbGetWindowAttributes != nullptr);
    }

    XcbGetWindowAttributesReply           pfnXcbGetWindowAttributesReply;
    bool pfnXcbGetWindowAttributesReplyisValid() const
    {
        return (pfnXcbGetWindowAttributesReply != nullptr);
    }

    XcbSyncTriggerFenceChecked            pfnXcbSyncTriggerFenceChecked;
    bool pfnXcbSyncTriggerFenceCheckedisValid() const
    {
        return (pfnXcbSyncTriggerFenceChecked != nullptr);
    }

    XcbSyncDestroyFenceChecked            pfnXcbSyncDestroyFenceChecked;
    bool pfnXcbSyncDestroyFenceCheckedisValid() const
    {
        return (pfnXcbSyncDestroyFenceChecked != nullptr);
    }

    XGetVisualInfo                        pfnXGetVisualInfo;
    bool pfnXGetVisualInfoisValid() const
    {
        return (pfnXGetVisualInfo != nullptr);
    }

    XFree                                 pfnXFree;
    bool pfnXFreeisValid() const
    {
        return (pfnXFree != nullptr);
    }

    XRootWindow                           pfnXRootWindow;
    bool pfnXRootWindowisValid() const
    {
        return (pfnXRootWindow != nullptr);
    }

    XcbPresentQueryVersion                pfnXcbPresentQueryVersion;
    bool pfnXcbPresentQueryVersionisValid() const
    {
        return (pfnXcbPresentQueryVersion != nullptr);
    }

    XcbPresentQueryVersionReply           pfnXcbPresentQueryVersionReply;
    bool pfnXcbPresentQueryVersionReplyisValid() const
    {
        return (pfnXcbPresentQueryVersionReply != nullptr);
    }

    XcbPresentSelectInputChecked          pfnXcbPresentSelectInputChecked;
    bool pfnXcbPresentSelectInputCheckedisValid() const
    {
        return (pfnXcbPresentSelectInputChecked != nullptr);
    }

    XcbPresentPixmapChecked               pfnXcbPresentPixmapChecked;
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

    xcb_generic_event_t* pfnXcbPollForSpecialEvent(
            xcb_connection_t*     pConnection,
            xcb_special_event*    pEvent) const;

    bool pfnXcbPollForSpecialEventisValid() const
    {
        return (m_pFuncs->pfnXcbPollForSpecialEvent != nullptr);
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

    void pfnXcbDiscardReply(
            xcb_connection_t*     pConnection,
            uint32                sequence) const;

    bool pfnXcbDiscardReplyisValid() const
    {
        return (m_pFuncs->pfnXcbDiscardReply != nullptr);
    }

    xcb_void_cookie_t pfnXcbChangePropertyChecked(
            xcb_connection_t*     pConnection,
            uint8_t               mode,
            xcb_window_t          window,
            xcb_atom_t            property,
            xcb_atom_t            type,
            uint8_t               format,
            uint32_t              data_len,
            const void *          pData) const;

    bool pfnXcbChangePropertyCheckedisValid() const
    {
        return (m_pFuncs->pfnXcbChangePropertyChecked != nullptr);
    }

    xcb_void_cookie_t pfnXcbDeletePropertyChecked(
            xcb_connection_t*     pConnection,
            xcb_window_t          window,
            xcb_atom_t            property) const;

    bool pfnXcbDeletePropertyCheckedisValid() const
    {
        return (m_pFuncs->pfnXcbDeletePropertyChecked != nullptr);
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

#if XCB_RANDR_SUPPORTS_LEASE
    xcb_randr_create_lease_cookie_t pfnXcbRandrCreateLease(
            xcb_connection_t*             pConnection,
            xcb_window_t                  window,
            xcb_randr_lease_t             leaseId,
            uint16_t                      numCrtcs,
            uint16_t                      numOutputs,
            const xcb_randr_crtc_t*       pCrtcs,
            const xcb_randr_output_t*     pOutputs) const;

    bool pfnXcbRandrCreateLeaseisValid() const
    {
        return (m_pFuncs->pfnXcbRandrCreateLease != nullptr);
    }

    xcb_randr_create_lease_reply_t* pfnXcbRandrCreateLeaseReply(
            xcb_connection_t*                 pConnection,
            xcb_randr_create_lease_cookie_t   cookie,
            xcb_generic_error_t**             ppError) const;

    bool pfnXcbRandrCreateLeaseReplyisValid() const
    {
        return (m_pFuncs->pfnXcbRandrCreateLeaseReply != nullptr);
    }

    int* pfnXcbRandrCreateLeaseReplyFds(
            xcb_connection_t*                 pConnection,
            xcb_randr_create_lease_reply_t*   pReply) const;

    bool pfnXcbRandrCreateLeaseReplyFdsisValid() const
    {
        return (m_pFuncs->pfnXcbRandrCreateLeaseReplyFds != nullptr);
    }
#endif

    xcb_randr_get_screen_resources_cookie_t pfnXcbRandrGetScreenResources(
            xcb_connection_t*     pConnection,
            xcb_window_t          window) const;

    bool pfnXcbRandrGetScreenResourcesisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetScreenResources != nullptr);
    }

    xcb_randr_get_screen_resources_reply_t* pfnXcbRandrGetScreenResourcesReply(
            xcb_connection_t*                         pConnection,
            xcb_randr_get_screen_resources_cookie_t   cookie,
            xcb_generic_error_t**                     ppError) const;

    bool pfnXcbRandrGetScreenResourcesReplyisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetScreenResourcesReply != nullptr);
    }

    xcb_randr_output_t* pfnXcbRandrGetScreenResourcesOutputs(
            const xcb_randr_get_screen_resources_reply_t*     pScrResReply) const;

    bool pfnXcbRandrGetScreenResourcesOutputsisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetScreenResourcesOutputs != nullptr);
    }

    xcb_randr_crtc_t* pfnXcbRandrGetScreenResourcesCrtcs(
            const xcb_randr_get_screen_resources_reply_t*     pScrResReply) const;

    bool pfnXcbRandrGetScreenResourcesCrtcsisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetScreenResourcesCrtcs != nullptr);
    }

    xcb_randr_get_crtc_info_cookie_t pfnXcbRandrGetCrtcInfo(
            xcb_connection_t*     pConnection,
            xcb_randr_crtc_t      output,
            xcb_timestamp_t       configTimestamp) const;

    bool pfnXcbRandrGetCrtcInfoisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetCrtcInfo != nullptr);
    }

    xcb_randr_get_crtc_info_reply_t* pfnXcbRandrGetCrtcInfoReply(
            xcb_connection_t*                 pConnection,
            xcb_randr_get_crtc_info_cookie_t  cookie,
            xcb_generic_error_t**             ppError) const;

    bool pfnXcbRandrGetCrtcInfoReplyisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetCrtcInfoReply != nullptr);
    }

    xcb_randr_get_output_info_cookie_t pfnXcbRandrGetOutputInfo(
            xcb_connection_t*     pConnection,
            xcb_randr_output_t    output,
            xcb_timestamp_t       configTimestamp) const;

    bool pfnXcbRandrGetOutputInfoisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetOutputInfo != nullptr);
    }

    xcb_randr_get_output_info_reply_t* pfnXcbRandrGetOutputInfoReply(
            xcb_connection_t*                     pConnection,
            xcb_randr_get_output_info_cookie_t    cookie,
            xcb_generic_error_t**                 ppError) const;

    bool pfnXcbRandrGetOutputInfoReplyisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetOutputInfoReply != nullptr);
    }

    xcb_randr_output_t* pfnXcbRandrGetCrtcInfoOutputs(
            xcb_randr_get_crtc_info_reply_t*  pCrtcInfoReply) const;

    bool pfnXcbRandrGetCrtcInfoOutputsisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetCrtcInfoOutputs != nullptr);
    }

    xcb_randr_output_t* pfnXcbRandrGetCrtcInfoPossible(
            xcb_randr_get_crtc_info_reply_t*  pCrtcInfoReply) const;

    bool pfnXcbRandrGetCrtcInfoPossibleisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetCrtcInfoPossible != nullptr);
    }

    xcb_randr_get_output_property_cookie_t pfnXcbRandrGetOutputProperty(
            xcb_connection_t*     pConnection,
            xcb_randr_output_t    output,
            xcb_atom_t            property,
            xcb_atom_t            type,
            uint32_t              offset,
            uint32_t              length,
            uint8_t               _delete,
            uint8_t               pending) const;

    bool pfnXcbRandrGetOutputPropertyisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetOutputProperty != nullptr);
    }

    uint8_t* pfnXcbRandrGetOutputPropertyData(
            const xcb_randr_get_output_property_reply_t*  pReply) const;

    bool pfnXcbRandrGetOutputPropertyDataisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetOutputPropertyData != nullptr);
    }

    xcb_randr_get_output_property_reply_t* pfnXcbRandrGetOutputPropertyReply(
            xcb_connection_t*                         pConnection,
            xcb_randr_get_output_property_cookie_t    cookie,
            xcb_generic_error_t**                     ppError) const;

    bool pfnXcbRandrGetOutputPropertyReplyisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetOutputPropertyReply != nullptr);
    }

    xcb_randr_get_providers_cookie_t pfnXcbRandrGetProviders(
            xcb_connection_t*     c,
            xcb_window_t          window) const;

    bool pfnXcbRandrGetProvidersisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetProviders != nullptr);
    }

    xcb_randr_get_providers_reply_t* pfnXcbRandrGetProvidersReply(
            xcb_connection_t*                 c,
            xcb_randr_get_providers_cookie_t  cookie,
            xcb_generic_error_t**             e) const;

    bool pfnXcbRandrGetProvidersReplyisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetProvidersReply != nullptr);
    }

    xcb_randr_provider_t* pfnXcbRandrGetProvidersProviders(
            const xcb_randr_get_providers_reply_t*    R) const;

    bool pfnXcbRandrGetProvidersProvidersisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetProvidersProviders != nullptr);
    }

    int pfnXcbRandrGetProvidersProvidersLength(
            const xcb_randr_get_providers_reply_t*    R) const;

    bool pfnXcbRandrGetProvidersProvidersLengthisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetProvidersProvidersLength != nullptr);
    }

    xcb_randr_get_provider_info_cookie_t pfnXcbRandrGetProviderInfo(
            xcb_connection_t*     c,
            xcb_randr_provider_t  provider,
            xcb_timestamp_t       config_timestamp) const;

    bool pfnXcbRandrGetProviderInfoisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetProviderInfo != nullptr);
    }

    xcb_randr_get_provider_info_reply_t* pfnXcbRandrGetProviderInfoReply(
            xcb_connection_t*                     c,
            xcb_randr_get_provider_info_cookie_t  cookie,
            xcb_generic_error_t**                 e) const;

    bool pfnXcbRandrGetProviderInfoReplyisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetProviderInfoReply != nullptr);
    }

    char* pfnXcbRandrGetProviderInfoName(
            const xcb_randr_get_provider_info_reply_t*    R) const;

    bool pfnXcbRandrGetProviderInfoNameisValid() const
    {
        return (m_pFuncs->pfnXcbRandrGetProviderInfoName != nullptr);
    }

    xcb_randr_query_version_cookie_t pfnXcbRandrQueryVersion(
            xcb_connection_t*     c,
            uint32_t              major_version,
            uint32_t              minor_version) const;

    bool pfnXcbRandrQueryVersionisValid() const
    {
        return (m_pFuncs->pfnXcbRandrQueryVersion != nullptr);
    }

    xcb_randr_query_version_reply_t* pfnXcbRandrQueryVersionReply(
            xcb_connection_t*                 c,
            xcb_randr_query_version_cookie_t  cookie,
            xcb_generic_error_t**             e) const;

    bool pfnXcbRandrQueryVersionReplyisValid() const
    {
        return (m_pFuncs->pfnXcbRandrQueryVersionReply != nullptr);
    }

    xcb_query_tree_cookie_t pfnXcbQueryTree(
            xcb_connection_t*     c,
            xcb_window_t          window) const;

    bool pfnXcbQueryTreeisValid() const
    {
        return (m_pFuncs->pfnXcbQueryTree != nullptr);
    }

    xcb_query_tree_reply_t* pfnXcbQueryTreeReply(
            xcb_connection_t *        c,
            xcb_query_tree_cookie_t   cookie,
            xcb_generic_error_t **    e) const;

    bool pfnXcbQueryTreeReplyisValid() const
    {
        return (m_pFuncs->pfnXcbQueryTreeReply != nullptr);
    }

    xcb_get_window_attributes_cookie_t pfnXcbGetWindowAttributes(
            xcb_connection_t *    c,
            xcb_window_t          window) const;

    bool pfnXcbGetWindowAttributesisValid() const
    {
        return (m_pFuncs->pfnXcbGetWindowAttributes != nullptr);
    }

    xcb_get_window_attributes_reply_t* pfnXcbGetWindowAttributesReply(
            xcb_connection_t *                    c,
            xcb_get_window_attributes_cookie_t    cookie,
            xcb_generic_error_t **                e) const;

    bool pfnXcbGetWindowAttributesReplyisValid() const
    {
        return (m_pFuncs->pfnXcbGetWindowAttributesReply != nullptr);
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
            long          visualMask,
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

    Window pfnXRootWindow(
            Display*  pDisplay,
            int       screenNum) const;

    bool pfnXRootWindowisValid() const
    {
        return (m_pFuncs->pfnXRootWindow != nullptr);
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
// the class is responsible for resolving all external symbols that required by the Dri3WindowSystem.
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

    Util::Library m_library[Dri3LoaderLibrariesCount];
    bool          m_initialized;

    Dri3LoaderFuncs      m_funcs;
#if defined(PAL_DEBUG_PRINTS)
    Dri3LoaderFuncsProxy m_proxy;
#endif

    PAL_DISALLOW_COPY_AND_ASSIGN(Dri3Loader);
};

} // Amdgpu
} // Pal
