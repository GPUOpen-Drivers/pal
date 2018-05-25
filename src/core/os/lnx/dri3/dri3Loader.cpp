/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <dlfcn.h>
#include <string.h>
#include <xcb/xcb.h>
#include "core/os/lnx/dri3/dri3Loader.h"
#include "palAssert.h"

#include "palSysUtil.h"

using namespace Util;
namespace Pal
{
namespace Linux
{

// =====================================================================================================================
#if defined(PAL_DEBUG_PRINTS)
void Dri3LoaderFuncsProxy::Init(const char* pLogPath)
{
    char file[128] = {0};
    Util::Snprintf(file, sizeof(file), "%s/Dri3LoaderTimeLogger.csv", pLogPath);
    m_timeLogger.Open(file, FileAccessMode::FileAccessWrite);
    Util::Snprintf(file, sizeof(file), "%s/Dri3LoaderParamLogger.trace", pLogPath);
    m_paramLogger.Open(file, FileAccessMode::FileAccessWrite);
}

// =====================================================================================================================

// =====================================================================================================================
xcb_connection_t* Dri3LoaderFuncsProxy::pfnXGetXCBConnection(
    Display*  pDisplay
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_connection_t* pRet = m_pFuncs->pfnXGetXCBConnection(pDisplay);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XGetXCBConnection,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XGetXCBConnection(%p)\n",
        pDisplay);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
uint32 Dri3LoaderFuncsProxy::pfnXcbGenerateId(
    xcb_connection_t*  pConnection
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    uint32 ret = m_pFuncs->pfnXcbGenerateId(pConnection);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbGenerateId,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbGenerateId(%p)\n",
        pConnection);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_special_event_t* Dri3LoaderFuncsProxy::pfnXcbRegisterForSpecialXge(
    xcb_connection_t*  pConnection,
    xcb_extension_t*   pExtensions,
    uint32             eventId,
    uint32*            pStamp
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_special_event_t* pRet = m_pFuncs->pfnXcbRegisterForSpecialXge(pConnection,
                                                                      pExtensions,
                                                                      eventId,
                                                                      pStamp);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRegisterForSpecialXge,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRegisterForSpecialXge(%p, %p, %x, %p)\n",
        pConnection,
        pExtensions,
        eventId,
        pStamp);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
void Dri3LoaderFuncsProxy::pfnXcbUnregisterForSpecialEvent(
    xcb_connection_t*     pConnection,
    xcb_special_event_t*  pEvent
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnXcbUnregisterForSpecialEvent(pConnection,
                                              pEvent);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbUnregisterForSpecialEvent,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbUnregisterForSpecialEvent(%p, %p)\n",
        pConnection,
        pEvent);
    m_paramLogger.Flush();
}

// =====================================================================================================================
xcb_generic_event_t* Dri3LoaderFuncsProxy::pfnXcbWaitForSpecialEvent(
    xcb_connection_t*   pConnection,
    xcb_special_event*  pEvent
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_generic_event_t* pRet = m_pFuncs->pfnXcbWaitForSpecialEvent(pConnection,
                                                                    pEvent);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbWaitForSpecialEvent,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbWaitForSpecialEvent(%p, %p)\n",
        pConnection,
        pEvent);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
const xcb_query_extension_reply_t* Dri3LoaderFuncsProxy::pfnXcbGetExtensionData(
    xcb_connection_t*  pConnection,
    xcb_extension_t*   pExtension
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    const xcb_query_extension_reply_t* pRet = m_pFuncs->pfnXcbGetExtensionData(pConnection,
                                                                               pExtension);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbGetExtensionData,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbGetExtensionData(%p, %p)\n",
        pConnection,
        pExtension);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
void Dri3LoaderFuncsProxy::pfnXcbPrefetchExtensionData(
    xcb_connection_t*  pConnection,
    xcb_extension_t*   pExtension
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnXcbPrefetchExtensionData(pConnection,
                                          pExtension);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbPrefetchExtensionData,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbPrefetchExtensionData(%p, %p)\n",
        pConnection,
        pExtension);
    m_paramLogger.Flush();
}

// =====================================================================================================================
xcb_generic_error_t* Dri3LoaderFuncsProxy::pfnXcbRequestCheck(
    xcb_connection_t*  pConnection,
    xcb_void_cookie_t  cookie
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_generic_error_t* pRet = m_pFuncs->pfnXcbRequestCheck(pConnection,
                                                             cookie);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRequestCheck,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRequestCheck(%p, %p)\n",
        pConnection,
        &cookie);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_get_geometry_cookie_t Dri3LoaderFuncsProxy::pfnXcbGetGeometry(
    xcb_connection_t*  pConnection,
    xcb_drawable_t     drawable
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_get_geometry_cookie_t ret = m_pFuncs->pfnXcbGetGeometry(pConnection,
                                                                drawable);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbGetGeometry,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbGetGeometry(%p, %x)\n",
        pConnection,
        drawable);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_get_geometry_reply_t* Dri3LoaderFuncsProxy::pfnXcbGetGeometryReply(
    xcb_connection_t*          pConnection,
    xcb_get_geometry_cookie_t  cookie,
    xcb_generic_error_t**      ppError
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_get_geometry_reply_t* pRet = m_pFuncs->pfnXcbGetGeometryReply(pConnection,
                                                                      cookie,
                                                                      ppError);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbGetGeometryReply,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbGetGeometryReply(%p, %p, %p)\n",
        pConnection,
        &cookie,
        ppError);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_void_cookie_t Dri3LoaderFuncsProxy::pfnXcbFreePixmapChecked(
    xcb_connection_t*  pConnection,
    xcb_pixmap_t       pixmap
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_void_cookie_t ret = m_pFuncs->pfnXcbFreePixmapChecked(pConnection,
                                                              pixmap);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbFreePixmapChecked,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbFreePixmapChecked(%p, %x)\n",
        pConnection,
        pixmap);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_intern_atom_reply_t* Dri3LoaderFuncsProxy::pfnXcbInternAtomReply(
    xcb_connection_t*         pConnection,
    xcb_intern_atom_cookie_t  cookie,
    xcb_generic_error_t**     ppError
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_intern_atom_reply_t* pRet = m_pFuncs->pfnXcbInternAtomReply(pConnection,
                                                                    cookie,
                                                                    ppError);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbInternAtomReply,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbInternAtomReply(%p, %p, %p)\n",
        pConnection,
        &cookie,
        ppError);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_intern_atom_cookie_t Dri3LoaderFuncsProxy::pfnXcbInternAtom(
    xcb_connection_t*  pConnection,
    uint8              onlyIfExists,
    uint16             nameLen,
    const char*        pName
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_intern_atom_cookie_t ret = m_pFuncs->pfnXcbInternAtom(pConnection,
                                                              onlyIfExists,
                                                              nameLen,
                                                              pName);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbInternAtom,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbInternAtom(%p, %x, %x, %p)\n",
        pConnection,
        onlyIfExists,
        nameLen,
        pName);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_depth_iterator_t Dri3LoaderFuncsProxy::pfnXcbScreenAllowedDepthsIterator(
    const xcb_screen_t*  pScreen
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_depth_iterator_t ret = m_pFuncs->pfnXcbScreenAllowedDepthsIterator(pScreen);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbScreenAllowedDepthsIterator,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbScreenAllowedDepthsIterator(%p)\n",
        pScreen);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
void Dri3LoaderFuncsProxy::pfnXcbDepthNext(
    xcb_depth_iterator_t*  pDepthIter
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnXcbDepthNext(pDepthIter);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbDepthNext,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbDepthNext(%p)\n",
        pDepthIter);
    m_paramLogger.Flush();
}

// =====================================================================================================================
void Dri3LoaderFuncsProxy::pfnXcbVisualtypeNext(
    xcb_visualtype_iterator_t*  pVisualTypeIter
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnXcbVisualtypeNext(pVisualTypeIter);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbVisualtypeNext,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbVisualtypeNext(%p)\n",
        pVisualTypeIter);
    m_paramLogger.Flush();
}

// =====================================================================================================================
xcb_screen_iterator_t Dri3LoaderFuncsProxy::pfnXcbSetupRootsIterator(
    const xcb_setup_t*  pSetup
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_screen_iterator_t ret = m_pFuncs->pfnXcbSetupRootsIterator(pSetup);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbSetupRootsIterator,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbSetupRootsIterator(%p)\n",
        pSetup);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
void Dri3LoaderFuncsProxy::pfnXcbScreenNext(
    xcb_screen_iterator_t*  pScreenIter
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnXcbScreenNext(pScreenIter);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbScreenNext,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbScreenNext(%p)\n",
        pScreenIter);
    m_paramLogger.Flush();
}

// =====================================================================================================================
xcb_visualtype_iterator_t Dri3LoaderFuncsProxy::pfnXcbDepthVisualsIterator(
    const xcb_depth_t*  pDepth
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_visualtype_iterator_t ret = m_pFuncs->pfnXcbDepthVisualsIterator(pDepth);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbDepthVisualsIterator,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbDepthVisualsIterator(%p)\n",
        pDepth);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
const xcb_setup_t* Dri3LoaderFuncsProxy::pfnXcbGetSetup(
    xcb_connection_t*  pConnection
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    const xcb_setup_t* pRet = m_pFuncs->pfnXcbGetSetup(pConnection);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbGetSetup,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbGetSetup(%p)\n",
        pConnection);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
const xcb_setup_t* Dri3LoaderFuncsProxy::pfnXcbFlush(
    xcb_connection_t*  pConnection
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    const xcb_setup_t* pRet = m_pFuncs->pfnXcbFlush(pConnection);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbFlush,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbFlush(%p)\n",
        pConnection);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
int32 Dri3LoaderFuncsProxy::pfnXshmfenceUnmapShm(
    struct xshmfence*  pFence
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnXshmfenceUnmapShm(pFence);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XshmfenceUnmapShm,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XshmfenceUnmapShm(%p)\n",
        pFence);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xshmfence* Dri3LoaderFuncsProxy::pfnXshmfenceMapShm(
    int32  fence
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xshmfence* pRet = m_pFuncs->pfnXshmfenceMapShm(fence);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XshmfenceMapShm,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XshmfenceMapShm(%x)\n",
        fence);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
int32 Dri3LoaderFuncsProxy::pfnXshmfenceQuery(
    struct xshmfence*  pFence
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnXshmfenceQuery(pFence);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XshmfenceQuery,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XshmfenceQuery(%p)\n",
        pFence);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 Dri3LoaderFuncsProxy::pfnXshmfenceAwait(
    struct xshmfence*  pFence
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnXshmfenceAwait(pFence);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XshmfenceAwait,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XshmfenceAwait(%p)\n",
        pFence);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 Dri3LoaderFuncsProxy::pfnXshmfenceAllocShm(    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnXshmfenceAllocShm();
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XshmfenceAllocShm,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf("XshmfenceAllocShm()\n");
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 Dri3LoaderFuncsProxy::pfnXshmfenceTrigger(
    struct xshmfence*  pFence
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnXshmfenceTrigger(pFence);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XshmfenceTrigger,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XshmfenceTrigger(%p)\n",
        pFence);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
void Dri3LoaderFuncsProxy::pfnXshmfenceReset(
    struct xshmfence*  pFence
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnXshmfenceReset(pFence);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XshmfenceReset,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XshmfenceReset(%p)\n",
        pFence);
    m_paramLogger.Flush();
}

// =====================================================================================================================
xcb_dri3_open_cookie_t Dri3LoaderFuncsProxy::pfnXcbDri3Open(
    xcb_connection_t*  pConnection,
    xcb_drawable_t     drawable,
    uint32             provider
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_dri3_open_cookie_t ret = m_pFuncs->pfnXcbDri3Open(pConnection,
                                                          drawable,
                                                          provider);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbDri3Open,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbDri3Open(%p, %x, %x)\n",
        pConnection,
        drawable,
        provider);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_dri3_open_reply_t* Dri3LoaderFuncsProxy::pfnXcbDri3OpenReply(
    xcb_connection_t*       pConnection,
    xcb_dri3_open_cookie_t  cookie,
    xcb_generic_error_t**   ppError
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_dri3_open_reply_t* pRet = m_pFuncs->pfnXcbDri3OpenReply(pConnection,
                                                                cookie,
                                                                ppError);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbDri3OpenReply,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbDri3OpenReply(%p, %p, %p)\n",
        pConnection,
        &cookie,
        ppError);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
int32* Dri3LoaderFuncsProxy::pfnXcbDri3OpenReplyFds(
    xcb_connection_t*       pConnection,
    xcb_dri3_open_reply_t*  pReply
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32* pRet = m_pFuncs->pfnXcbDri3OpenReplyFds(pConnection,
                                                   pReply);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbDri3OpenReplyFds,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbDri3OpenReplyFds(%p, %p)\n",
        pConnection,
        pReply);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_void_cookie_t Dri3LoaderFuncsProxy::pfnXcbDri3FenceFromFdChecked(
    xcb_connection_t*  pConnection,
    xcb_drawable_t     drawable,
    uint32             fence,
    uint8              initiallyTriggered,
    int32              fenceFd
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_void_cookie_t ret = m_pFuncs->pfnXcbDri3FenceFromFdChecked(pConnection,
                                                                   drawable,
                                                                   fence,
                                                                   initiallyTriggered,
                                                                   fenceFd);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbDri3FenceFromFdChecked,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbDri3FenceFromFdChecked(%p, %x, %x, %x, %x)\n",
        pConnection,
        drawable,
        fence,
        initiallyTriggered,
        fenceFd);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_void_cookie_t Dri3LoaderFuncsProxy::pfnXcbDri3PixmapFromBufferChecked(
    xcb_connection_t*  pConnection,
    xcb_pixmap_t       pixmap,
    xcb_drawable_t     drawable,
    uint32             size,
    uint16             width,
    uint16             height,
    uint16             stride,
    uint8              depth,
    uint8              bpp,
    int32              pixmapFd
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_void_cookie_t ret = m_pFuncs->pfnXcbDri3PixmapFromBufferChecked(pConnection,
                                                                        pixmap,
                                                                        drawable,
                                                                        size,
                                                                        width,
                                                                        height,
                                                                        stride,
                                                                        depth,
                                                                        bpp,
                                                                        pixmapFd);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbDri3PixmapFromBufferChecked,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbDri3PixmapFromBufferChecked(%p, %x, %x, %x, %x, %x, %x, %x, %x, %x)\n",
        pConnection,
        pixmap,
        drawable,
        size,
        width,
        height,
        stride,
        depth,
        bpp,
        pixmapFd);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_dri3_query_version_cookie_t Dri3LoaderFuncsProxy::pfnXcbDri3QueryVersion(
    xcb_connection_t*  pConnection,
    uint32             majorVersion,
    uint32             minorVersion
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_dri3_query_version_cookie_t ret = m_pFuncs->pfnXcbDri3QueryVersion(pConnection,
                                                                           majorVersion,
                                                                           minorVersion);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbDri3QueryVersion,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbDri3QueryVersion(%p, %x, %x)\n",
        pConnection,
        majorVersion,
        minorVersion);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_dri3_query_version_reply_t* Dri3LoaderFuncsProxy::pfnXcbDri3QueryVersionReply(
    xcb_connection_t*                pConnection,
    xcb_dri3_query_version_cookie_t  cookie,
    xcb_generic_error_t**            ppError
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_dri3_query_version_reply_t* pRet = m_pFuncs->pfnXcbDri3QueryVersionReply(pConnection,
                                                                                 cookie,
                                                                                 ppError);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbDri3QueryVersionReply,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbDri3QueryVersionReply(%p, %p, %p)\n",
        pConnection,
        &cookie,
        ppError);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_dri2_connect_cookie_t Dri3LoaderFuncsProxy::pfnXcbDri2Connect(
    xcb_connection_t*  pConnection,
    xcb_window_t       window,
    uint32             driver_type
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_dri2_connect_cookie_t ret = m_pFuncs->pfnXcbDri2Connect(pConnection,
                                                                window,
                                                                driver_type);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbDri2Connect,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbDri2Connect(%p, %x, %x)\n",
        pConnection,
        window,
        driver_type);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int Dri3LoaderFuncsProxy::pfnXcbDri2ConnectDriverNameLength(
    const xcb_dri2_connect_reply_t*  pReplay
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnXcbDri2ConnectDriverNameLength(pReplay);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbDri2ConnectDriverNameLength,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbDri2ConnectDriverNameLength(%p)\n",
        pReplay);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
char* Dri3LoaderFuncsProxy::pfnXcbDri2ConnectDriverName(
    const xcb_dri2_connect_reply_t*  pReplay
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    char* pRet = m_pFuncs->pfnXcbDri2ConnectDriverName(pReplay);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbDri2ConnectDriverName,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbDri2ConnectDriverName(%p)\n",
        pReplay);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_dri2_connect_reply_t* Dri3LoaderFuncsProxy::pfnXcbDri2ConnectReply(
    xcb_connection_t*          pConnection,
    xcb_dri2_connect_cookie_t  cookie,
    xcb_generic_error_t**      ppError
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_dri2_connect_reply_t* pRet = m_pFuncs->pfnXcbDri2ConnectReply(pConnection,
                                                                      cookie,
                                                                      ppError);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbDri2ConnectReply,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbDri2ConnectReply(%p, %p, %p)\n",
        pConnection,
        &cookie,
        ppError);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_void_cookie_t Dri3LoaderFuncsProxy::pfnXcbSyncTriggerFenceChecked(
    xcb_connection_t*  pConnection,
    xcb_sync_fence_t   fence
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_void_cookie_t ret = m_pFuncs->pfnXcbSyncTriggerFenceChecked(pConnection,
                                                                    fence);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbSyncTriggerFenceChecked,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbSyncTriggerFenceChecked(%p, %x)\n",
        pConnection,
        fence);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_void_cookie_t Dri3LoaderFuncsProxy::pfnXcbSyncDestroyFenceChecked(
    xcb_connection_t*  pConnection,
    xcb_sync_fence_t   fence
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_void_cookie_t ret = m_pFuncs->pfnXcbSyncDestroyFenceChecked(pConnection,
                                                                    fence);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbSyncDestroyFenceChecked,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbSyncDestroyFenceChecked(%p, %x)\n",
        pConnection,
        fence);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
XVisualInfo* Dri3LoaderFuncsProxy::pfnXGetVisualInfo(
    Display*      pDisplay,
    uint64        visualMask,
    XVisualInfo*  pVisualInfoList,
    int32*        count
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    XVisualInfo* pRet = m_pFuncs->pfnXGetVisualInfo(pDisplay,
                                                    visualMask,
                                                    pVisualInfoList,
                                                    count);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XGetVisualInfo,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XGetVisualInfo(%p, %lx, %p, %p)\n",
        pDisplay,
        visualMask,
        pVisualInfoList,
        count);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
int32 Dri3LoaderFuncsProxy::pfnXFree(
    void*  pAddress
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnXFree(pAddress);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XFree,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XFree(%p)\n",
        pAddress);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_present_query_version_cookie_t Dri3LoaderFuncsProxy::pfnXcbPresentQueryVersion(
    xcb_connection_t*  pConnection,
    uint32             majorVersion,
    uint32             minorVersion
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_present_query_version_cookie_t ret = m_pFuncs->pfnXcbPresentQueryVersion(pConnection,
                                                                                 majorVersion,
                                                                                 minorVersion);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbPresentQueryVersion,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbPresentQueryVersion(%p, %x, %x)\n",
        pConnection,
        majorVersion,
        minorVersion);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_present_query_version_reply_t* Dri3LoaderFuncsProxy::pfnXcbPresentQueryVersionReply(
    xcb_connection_t*                   pConnection,
    xcb_present_query_version_cookie_t  cookie,
    xcb_generic_error_t**               ppError
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_present_query_version_reply_t* pRet = m_pFuncs->pfnXcbPresentQueryVersionReply(pConnection,
                                                                                       cookie,
                                                                                       ppError);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbPresentQueryVersionReply,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbPresentQueryVersionReply(%p, %p, %p)\n",
        pConnection,
        &cookie,
        ppError);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_void_cookie_t Dri3LoaderFuncsProxy::pfnXcbPresentSelectInputChecked(
    xcb_connection_t*    pConnection,
    xcb_present_event_t  eventId,
    xcb_window_t         window,
    uint32               eventMask
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_void_cookie_t ret = m_pFuncs->pfnXcbPresentSelectInputChecked(pConnection,
                                                                      eventId,
                                                                      window,
                                                                      eventMask);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbPresentSelectInputChecked,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbPresentSelectInputChecked(%p, %x, %x, %x)\n",
        pConnection,
        eventId,
        window,
        eventMask);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_void_cookie_t Dri3LoaderFuncsProxy::pfnXcbPresentPixmapChecked(
    xcb_connection_t*            pConnection,
    xcb_window_t                 window,
    xcb_pixmap_t                 pixmap,
    uint32                       serial,
    xcb_xfixes_region_t          valid,
    xcb_xfixes_region_t          update,
    int16                        xOff,
    int16                        yO_off,
    xcb_randr_crtc_t             targetCrtc,
    xcb_sync_fence_t             waitFence,
    xcb_sync_fence_t             idleFence,
    uint32                       options,
    uint64                       targetMsc,
    uint64                       divisor,
    uint64                       remainder,
    uint32                       notifiesLen,
    const xcb_present_notify_t*  pNotifies
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_void_cookie_t ret = m_pFuncs->pfnXcbPresentPixmapChecked(pConnection,
                                                                 window,
                                                                 pixmap,
                                                                 serial,
                                                                 valid,
                                                                 update,
                                                                 xOff,
                                                                 yO_off,
                                                                 targetCrtc,
                                                                 waitFence,
                                                                 idleFence,
                                                                 options,
                                                                 targetMsc,
                                                                 divisor,
                                                                 remainder,
                                                                 notifiesLen,
                                                                 pNotifies);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbPresentPixmapChecked,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbPresentPixmapChecked(%p, %x, %x, %x, %x, %x, %x, %x, %x, %x, %x, %x, %lx, %lx, %lx, %x, %p)\n",
        pConnection,
        window,
        pixmap,
        serial,
        valid,
        update,
        xOff,
        yO_off,
        targetCrtc,
        waitFence,
        idleFence,
        options,
        targetMsc,
        divisor,
        remainder,
        notifiesLen,
        pNotifies);
    m_paramLogger.Flush();

    return ret;
}

#endif

// =====================================================================================================================
Dri3Loader::Dri3Loader()
    :
    m_pXcbDri3Id(nullptr),
    m_pXcbPresentId(nullptr),
    m_pXcbDri2Id(nullptr),
    m_initialized(false)
{
    memset(m_libraryHandles, 0, sizeof(m_libraryHandles));
    memset(&m_funcs, 0, sizeof(m_funcs));
}

// =====================================================================================================================
xcb_extension_t* Dri3Loader::GetXcbDri3Id() const
{
    return m_pXcbDri3Id;
}

// =====================================================================================================================
xcb_extension_t* Dri3Loader::GetXcbPresentId() const
{
    return m_pXcbPresentId;
}

// =====================================================================================================================
xcb_extension_t* Dri3Loader::GetXcbDri2Id() const
{
    return m_pXcbDri2Id;
}

// =====================================================================================================================
Dri3Loader::~Dri3Loader()
{
    if (m_libraryHandles[LibX11Xcb] != nullptr)
    {
        dlclose(m_libraryHandles[LibX11Xcb]);
    }
    if (m_libraryHandles[LibXcb] != nullptr)
    {
        dlclose(m_libraryHandles[LibXcb]);
    }
    if (m_libraryHandles[LibXshmFence] != nullptr)
    {
        dlclose(m_libraryHandles[LibXshmFence]);
    }
    if (m_libraryHandles[LibXcbDri3] != nullptr)
    {
        dlclose(m_libraryHandles[LibXcbDri3]);
    }
    if (m_libraryHandles[LibXcbDri2] != nullptr)
    {
        dlclose(m_libraryHandles[LibXcbDri2]);
    }
    if (m_libraryHandles[LibXcbSync] != nullptr)
    {
        dlclose(m_libraryHandles[LibXcbSync]);
    }
    if (m_libraryHandles[LibX11] != nullptr)
    {
        dlclose(m_libraryHandles[LibX11]);
    }
    if (m_libraryHandles[LibXcbPresent] != nullptr)
    {
        dlclose(m_libraryHandles[LibXcbPresent]);
    }
}

// =====================================================================================================================
Result Dri3Loader::Init(
    Platform* pPlatform)
{
    Result result                   = Result::Success;
    constexpr uint32_t LibNameSize  = 64;
    char LibNames[Dri3LoaderLibrariesCount][LibNameSize] = {
        "libX11-xcb.so.1",
        "libxcb.so.1",
        "libxshmfence.so.1",
        "libxcb-dri3.so.0",
        "libxcb-dri2.so.0",
        "libxcb-sync.so.1",
        "libX11.so.6",
        "libxcb-present.so.0",
    };

    if (m_initialized == false)
    {
        // resolve symbols from libX11-xcb.so.1
        m_libraryHandles[LibX11Xcb] = dlopen(LibNames[LibX11Xcb], RTLD_LAZY);
        if (m_libraryHandles[LibX11Xcb] == nullptr)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            m_funcs.pfnXGetXCBConnection = reinterpret_cast<XGetXCBConnection>(dlsym(
                        m_libraryHandles[LibX11Xcb],
                        "XGetXCBConnection"));
        }

        // resolve symbols from libxcb.so.1
        m_libraryHandles[LibXcb] = dlopen(LibNames[LibXcb], RTLD_LAZY);
        if (m_libraryHandles[LibXcb] == nullptr)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            m_funcs.pfnXcbGenerateId = reinterpret_cast<XcbGenerateId>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_generate_id"));
            m_funcs.pfnXcbRegisterForSpecialXge = reinterpret_cast<XcbRegisterForSpecialXge>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_register_for_special_xge"));
            m_funcs.pfnXcbUnregisterForSpecialEvent = reinterpret_cast<XcbUnregisterForSpecialEvent>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_unregister_for_special_event"));
            m_funcs.pfnXcbWaitForSpecialEvent = reinterpret_cast<XcbWaitForSpecialEvent>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_wait_for_special_event"));
            m_funcs.pfnXcbGetExtensionData = reinterpret_cast<XcbGetExtensionData>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_get_extension_data"));
            m_funcs.pfnXcbPrefetchExtensionData = reinterpret_cast<XcbPrefetchExtensionData>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_prefetch_extension_data"));
            m_funcs.pfnXcbRequestCheck = reinterpret_cast<XcbRequestCheck>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_request_check"));
            m_funcs.pfnXcbGetGeometry = reinterpret_cast<XcbGetGeometry>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_get_geometry"));
            m_funcs.pfnXcbGetGeometryReply = reinterpret_cast<XcbGetGeometryReply>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_get_geometry_reply"));
            m_funcs.pfnXcbFreePixmapChecked = reinterpret_cast<XcbFreePixmapChecked>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_free_pixmap_checked"));
            m_funcs.pfnXcbInternAtomReply = reinterpret_cast<XcbInternAtomReply>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_intern_atom_reply"));
            m_funcs.pfnXcbInternAtom = reinterpret_cast<XcbInternAtom>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_intern_atom"));
            m_funcs.pfnXcbScreenAllowedDepthsIterator = reinterpret_cast<XcbScreenAllowedDepthsIterator>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_screen_allowed_depths_iterator"));
            m_funcs.pfnXcbDepthNext = reinterpret_cast<XcbDepthNext>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_depth_next"));
            m_funcs.pfnXcbVisualtypeNext = reinterpret_cast<XcbVisualtypeNext>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_visualtype_next"));
            m_funcs.pfnXcbSetupRootsIterator = reinterpret_cast<XcbSetupRootsIterator>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_setup_roots_iterator"));
            m_funcs.pfnXcbScreenNext = reinterpret_cast<XcbScreenNext>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_screen_next"));
            m_funcs.pfnXcbDepthVisualsIterator = reinterpret_cast<XcbDepthVisualsIterator>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_depth_visuals_iterator"));
            m_funcs.pfnXcbGetSetup = reinterpret_cast<XcbGetSetup>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_get_setup"));
            m_funcs.pfnXcbFlush = reinterpret_cast<XcbFlush>(dlsym(
                        m_libraryHandles[LibXcb],
                        "xcb_flush"));
        }

        // resolve symbols from libxshmfence.so.1
        m_libraryHandles[LibXshmFence] = dlopen(LibNames[LibXshmFence], RTLD_LAZY);
        if (m_libraryHandles[LibXshmFence] == nullptr)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            m_funcs.pfnXshmfenceUnmapShm = reinterpret_cast<XshmfenceUnmapShm>(dlsym(
                        m_libraryHandles[LibXshmFence],
                        "xshmfence_unmap_shm"));
            m_funcs.pfnXshmfenceMapShm = reinterpret_cast<XshmfenceMapShm>(dlsym(
                        m_libraryHandles[LibXshmFence],
                        "xshmfence_map_shm"));
            m_funcs.pfnXshmfenceQuery = reinterpret_cast<XshmfenceQuery>(dlsym(
                        m_libraryHandles[LibXshmFence],
                        "xshmfence_query"));
            m_funcs.pfnXshmfenceAwait = reinterpret_cast<XshmfenceAwait>(dlsym(
                        m_libraryHandles[LibXshmFence],
                        "xshmfence_await"));
            m_funcs.pfnXshmfenceAllocShm = reinterpret_cast<XshmfenceAllocShm>(dlsym(
                        m_libraryHandles[LibXshmFence],
                        "xshmfence_alloc_shm"));
            m_funcs.pfnXshmfenceTrigger = reinterpret_cast<XshmfenceTrigger>(dlsym(
                        m_libraryHandles[LibXshmFence],
                        "xshmfence_trigger"));
            m_funcs.pfnXshmfenceReset = reinterpret_cast<XshmfenceReset>(dlsym(
                        m_libraryHandles[LibXshmFence],
                        "xshmfence_reset"));
        }

        // resolve symbols from libxcb-dri3.so.0
        m_libraryHandles[LibXcbDri3] = dlopen(LibNames[LibXcbDri3], RTLD_LAZY);
        if (m_libraryHandles[LibXcbDri3] == nullptr)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            m_funcs.pfnXcbDri3Open = reinterpret_cast<XcbDri3Open>(dlsym(
                        m_libraryHandles[LibXcbDri3],
                        "xcb_dri3_open"));
            m_funcs.pfnXcbDri3OpenReply = reinterpret_cast<XcbDri3OpenReply>(dlsym(
                        m_libraryHandles[LibXcbDri3],
                        "xcb_dri3_open_reply"));
            m_funcs.pfnXcbDri3OpenReplyFds = reinterpret_cast<XcbDri3OpenReplyFds>(dlsym(
                        m_libraryHandles[LibXcbDri3],
                        "xcb_dri3_open_reply_fds"));
            m_funcs.pfnXcbDri3FenceFromFdChecked = reinterpret_cast<XcbDri3FenceFromFdChecked>(dlsym(
                        m_libraryHandles[LibXcbDri3],
                        "xcb_dri3_fence_from_fd_checked"));
            m_funcs.pfnXcbDri3PixmapFromBufferChecked = reinterpret_cast<XcbDri3PixmapFromBufferChecked>(dlsym(
                        m_libraryHandles[LibXcbDri3],
                        "xcb_dri3_pixmap_from_buffer_checked"));
            m_funcs.pfnXcbDri3QueryVersion = reinterpret_cast<XcbDri3QueryVersion>(dlsym(
                        m_libraryHandles[LibXcbDri3],
                        "xcb_dri3_query_version"));
            m_funcs.pfnXcbDri3QueryVersionReply = reinterpret_cast<XcbDri3QueryVersionReply>(dlsym(
                        m_libraryHandles[LibXcbDri3],
                        "xcb_dri3_query_version_reply"));
        }

        // resolve symbols from libxcb-dri2.so.0
        m_libraryHandles[LibXcbDri2] = dlopen(LibNames[LibXcbDri2], RTLD_LAZY);
        if (m_libraryHandles[LibXcbDri2] == nullptr)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            m_funcs.pfnXcbDri2Connect = reinterpret_cast<XcbDri2Connect>(dlsym(
                        m_libraryHandles[LibXcbDri2],
                        "xcb_dri2_connect"));
            m_funcs.pfnXcbDri2ConnectDriverNameLength = reinterpret_cast<XcbDri2ConnectDriverNameLength>(dlsym(
                        m_libraryHandles[LibXcbDri2],
                        "xcb_dri2_connect_driver_name_length"));
            m_funcs.pfnXcbDri2ConnectDriverName = reinterpret_cast<XcbDri2ConnectDriverName>(dlsym(
                        m_libraryHandles[LibXcbDri2],
                        "xcb_dri2_connect_driver_name"));
            m_funcs.pfnXcbDri2ConnectReply = reinterpret_cast<XcbDri2ConnectReply>(dlsym(
                        m_libraryHandles[LibXcbDri2],
                        "xcb_dri2_connect_reply"));
        }

        // resolve symbols from libxcb-sync.so.1
        m_libraryHandles[LibXcbSync] = dlopen(LibNames[LibXcbSync], RTLD_LAZY);
        if (m_libraryHandles[LibXcbSync] == nullptr)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            m_funcs.pfnXcbSyncTriggerFenceChecked = reinterpret_cast<XcbSyncTriggerFenceChecked>(dlsym(
                        m_libraryHandles[LibXcbSync],
                        "xcb_sync_trigger_fence_checked"));
            m_funcs.pfnXcbSyncDestroyFenceChecked = reinterpret_cast<XcbSyncDestroyFenceChecked>(dlsym(
                        m_libraryHandles[LibXcbSync],
                        "xcb_sync_destroy_fence_checked"));
        }

        // resolve symbols from libX11.so.6
        m_libraryHandles[LibX11] = dlopen(LibNames[LibX11], RTLD_LAZY);
        if (m_libraryHandles[LibX11] == nullptr)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            m_funcs.pfnXGetVisualInfo = reinterpret_cast<XGetVisualInfo>(dlsym(
                        m_libraryHandles[LibX11],
                        "XGetVisualInfo"));
            m_funcs.pfnXFree = reinterpret_cast<XFree>(dlsym(
                        m_libraryHandles[LibX11],
                        "XFree"));
        }

        // resolve symbols from libxcb-present.so.0
        m_libraryHandles[LibXcbPresent] = dlopen(LibNames[LibXcbPresent], RTLD_LAZY);
        if (m_libraryHandles[LibXcbPresent] == nullptr)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            m_funcs.pfnXcbPresentQueryVersion = reinterpret_cast<XcbPresentQueryVersion>(dlsym(
                        m_libraryHandles[LibXcbPresent],
                        "xcb_present_query_version"));
            m_funcs.pfnXcbPresentQueryVersionReply = reinterpret_cast<XcbPresentQueryVersionReply>(dlsym(
                        m_libraryHandles[LibXcbPresent],
                        "xcb_present_query_version_reply"));
            m_funcs.pfnXcbPresentSelectInputChecked = reinterpret_cast<XcbPresentSelectInputChecked>(dlsym(
                        m_libraryHandles[LibXcbPresent],
                        "xcb_present_select_input_checked"));
            m_funcs.pfnXcbPresentPixmapChecked = reinterpret_cast<XcbPresentPixmapChecked>(dlsym(
                        m_libraryHandles[LibXcbPresent],
                        "xcb_present_pixmap_checked"));
        }

        if (m_libraryHandles[LibXcbDri3] == nullptr)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            m_pXcbDri3Id = reinterpret_cast<xcb_extension_t*>(dlsym(m_libraryHandles[LibXcbDri3], "xcb_dri3_id"));
        }
        if (m_libraryHandles[LibXcbPresent] == nullptr)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            m_pXcbPresentId = reinterpret_cast<xcb_extension_t*>(dlsym(m_libraryHandles[LibXcbPresent], "xcb_present_id"));
        }
        if (m_libraryHandles[LibXcbDri2] == nullptr)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            m_pXcbDri2Id = reinterpret_cast<xcb_extension_t*>(dlsym(m_libraryHandles[LibXcbDri2], "xcb_dri2_id"));
        }
        if (result == Result::Success)
        {
            m_initialized = true;
#if defined(PAL_DEBUG_PRINTS)
            m_proxy.SetFuncCalls(&m_funcs);
#endif
        }
    }
    return result;
}

} //namespace Linux
} //namespace Pal
