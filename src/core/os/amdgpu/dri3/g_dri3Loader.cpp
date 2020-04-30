/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/os/amdgpu/dri3/g_dri3Loader.h"
#include "palAssert.h"
#include "palSysUtil.h"

#include <string.h>
#include <xcb/xcb.h>

using namespace Util;

namespace Pal
{
namespace Amdgpu
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
void Dri3LoaderFuncsProxy::pfnXcbDiscardReply(
    xcb_connection_t*  pConnection,
    uint32             sequence
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnXcbDiscardReply(pConnection,
                                 sequence);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbDiscardReply,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbDiscardReply(%p, %x)\n",
        pConnection,
        sequence);
    m_paramLogger.Flush();
}

// =====================================================================================================================
xcb_void_cookie_t Dri3LoaderFuncsProxy::pfnXcbChangePropertyChecked(
    xcb_connection_t*  pConnection,
    uint8_t            mode,
    xcb_window_t       window,
    xcb_atom_t         property,
    xcb_atom_t         type,
    uint8_t            format,
    uint32_t           data_len,
    const void         *pData
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_void_cookie_t ret = m_pFuncs->pfnXcbChangePropertyChecked(pConnection,
                                                                  mode,
                                                                  window,
                                                                  property,
                                                                  type,
                                                                  format,
                                                                  data_len,
                                                                  *pData);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbChangePropertyChecked,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbChangePropertyChecked(%p, %x, %x, %x, %x, %x, %x, %x)\n",
        pConnection,
        mode,
        window,
        property,
        type,
        format,
        data_len,
        *pData);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_void_cookie_t Dri3LoaderFuncsProxy::pfnXcbDeletePropertyChecked(
    xcb_connection_t*  pConnection,
    xcb_window_t       window,
    xcb_atom_t         property
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_void_cookie_t ret = m_pFuncs->pfnXcbDeletePropertyChecked(pConnection,
                                                                  window,
                                                                  property);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbDeletePropertyChecked,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbDeletePropertyChecked(%p, %x, %x)\n",
        pConnection,
        window,
        property);
    m_paramLogger.Flush();

    return ret;
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

#if XCB_RANDR_SUPPORTS_LEASE
// =====================================================================================================================
xcb_randr_create_lease_cookie_t Dri3LoaderFuncsProxy::pfnXcbRandrCreateLease(
    xcb_connection_t*          pConnection,
    xcb_window_t               window,
    xcb_randr_lease_t          leaseId,
    uint16_t                   numCrtcs,
    uint16_t                   numOutputs,
    const xcb_randr_crtc_t*    pCrtcs,
    const xcb_randr_output_t*  pOutputs
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_create_lease_cookie_t ret = m_pFuncs->pfnXcbRandrCreateLease(pConnection,
                                                                           window,
                                                                           leaseId,
                                                                           numCrtcs,
                                                                           numOutputs,
                                                                           pCrtcs,
                                                                           pOutputs);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrCreateLease,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrCreateLease(%p, %x, %x, %x, %x, %p, %p)\n",
        pConnection,
        window,
        leaseId,
        numCrtcs,
        numOutputs,
        pCrtcs,
        pOutputs);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_randr_create_lease_reply_t* Dri3LoaderFuncsProxy::pfnXcbRandrCreateLeaseReply(
    xcb_connection_t*                pConnection,
    xcb_randr_create_lease_cookie_t  cookie,
    xcb_generic_error_t**            ppError
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_create_lease_reply_t* pRet = m_pFuncs->pfnXcbRandrCreateLeaseReply(pConnection,
                                                                                 cookie,
                                                                                 ppError);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrCreateLeaseReply,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrCreateLeaseReply(%p, %p, %p)\n",
        pConnection,
        &cookie,
        ppError);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
int* Dri3LoaderFuncsProxy::pfnXcbRandrCreateLeaseReplyFds(
    xcb_connection_t*                pConnection,
    xcb_randr_create_lease_reply_t*  pReply
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int* pRet = m_pFuncs->pfnXcbRandrCreateLeaseReplyFds(pConnection,
                                                         pReply);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrCreateLeaseReplyFds,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrCreateLeaseReplyFds(%p, %p)\n",
        pConnection,
        pReply);
    m_paramLogger.Flush();

    return pRet;
}
#endif

// =====================================================================================================================
xcb_randr_get_screen_resources_cookie_t Dri3LoaderFuncsProxy::pfnXcbRandrGetScreenResources(
    xcb_connection_t*  pConnection,
    xcb_window_t       window
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_get_screen_resources_cookie_t ret = m_pFuncs->pfnXcbRandrGetScreenResources(pConnection,
                                                                                          window);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetScreenResources,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetScreenResources(%p, %x)\n",
        pConnection,
        window);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_randr_get_screen_resources_reply_t* Dri3LoaderFuncsProxy::pfnXcbRandrGetScreenResourcesReply(
    xcb_connection_t*                        pConnection,
    xcb_randr_get_screen_resources_cookie_t  cookie,
    xcb_generic_error_t**                    ppError
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_get_screen_resources_reply_t* pRet = m_pFuncs->pfnXcbRandrGetScreenResourcesReply(pConnection,
                                                                                                cookie,
                                                                                                ppError);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetScreenResourcesReply,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetScreenResourcesReply(%p, %p, %p)\n",
        pConnection,
        &cookie,
        ppError);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_randr_output_t* Dri3LoaderFuncsProxy::pfnXcbRandrGetScreenResourcesOutputs(
    const xcb_randr_get_screen_resources_reply_t*  pScrResReply
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_output_t* pRet = m_pFuncs->pfnXcbRandrGetScreenResourcesOutputs(pScrResReply);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetScreenResourcesOutputs,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetScreenResourcesOutputs(%p)\n",
        pScrResReply);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_randr_crtc_t* Dri3LoaderFuncsProxy::pfnXcbRandrGetScreenResourcesCrtcs(
    const xcb_randr_get_screen_resources_reply_t*  pScrResReply
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_crtc_t* pRet = m_pFuncs->pfnXcbRandrGetScreenResourcesCrtcs(pScrResReply);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetScreenResourcesCrtcs,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetScreenResourcesCrtcs(%p)\n",
        pScrResReply);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_randr_get_crtc_info_cookie_t Dri3LoaderFuncsProxy::pfnXcbRandrGetCrtcInfo(
    xcb_connection_t*  pConnection,
    xcb_randr_crtc_t   output,
    xcb_timestamp_t    configTimestamp
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_get_crtc_info_cookie_t ret = m_pFuncs->pfnXcbRandrGetCrtcInfo(pConnection,
                                                                            output,
                                                                            configTimestamp);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetCrtcInfo,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetCrtcInfo(%p, %x, %x)\n",
        pConnection,
        output,
        configTimestamp);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_randr_get_crtc_info_reply_t* Dri3LoaderFuncsProxy::pfnXcbRandrGetCrtcInfoReply(
    xcb_connection_t*                 pConnection,
    xcb_randr_get_crtc_info_cookie_t  cookie,
    xcb_generic_error_t**             ppError
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_get_crtc_info_reply_t* pRet = m_pFuncs->pfnXcbRandrGetCrtcInfoReply(pConnection,
                                                                                  cookie,
                                                                                  ppError);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetCrtcInfoReply,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetCrtcInfoReply(%p, %p, %p)\n",
        pConnection,
        &cookie,
        ppError);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_randr_get_output_info_cookie_t Dri3LoaderFuncsProxy::pfnXcbRandrGetOutputInfo(
    xcb_connection_t*   pConnection,
    xcb_randr_output_t  output,
    xcb_timestamp_t     configTimestamp
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_get_output_info_cookie_t ret = m_pFuncs->pfnXcbRandrGetOutputInfo(pConnection,
                                                                                output,
                                                                                configTimestamp);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetOutputInfo,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetOutputInfo(%p, %x, %x)\n",
        pConnection,
        output,
        configTimestamp);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_randr_get_output_info_reply_t* Dri3LoaderFuncsProxy::pfnXcbRandrGetOutputInfoReply(
    xcb_connection_t*                   pConnection,
    xcb_randr_get_output_info_cookie_t  cookie,
    xcb_generic_error_t**               ppError
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_get_output_info_reply_t* pRet = m_pFuncs->pfnXcbRandrGetOutputInfoReply(pConnection,
                                                                                      cookie,
                                                                                      ppError);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetOutputInfoReply,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetOutputInfoReply(%p, %p, %p)\n",
        pConnection,
        &cookie,
        ppError);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_randr_output_t* Dri3LoaderFuncsProxy::pfnXcbRandrGetCrtcInfoOutputs(
    xcb_randr_get_crtc_info_reply_t*  pCrtcInfoReply
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_output_t* pRet = m_pFuncs->pfnXcbRandrGetCrtcInfoOutputs(pCrtcInfoReply);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetCrtcInfoOutputs,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetCrtcInfoOutputs(%p)\n",
        pCrtcInfoReply);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_randr_output_t* Dri3LoaderFuncsProxy::pfnXcbRandrGetCrtcInfoPossible(
    xcb_randr_get_crtc_info_reply_t*  pCrtcInfoReply
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_output_t* pRet = m_pFuncs->pfnXcbRandrGetCrtcInfoPossible(pCrtcInfoReply);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetCrtcInfoPossible,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetCrtcInfoPossible(%p)\n",
        pCrtcInfoReply);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_randr_get_output_property_cookie_t Dri3LoaderFuncsProxy::pfnXcbRandrGetOutputProperty(
    xcb_connection_t*   pConnection,
    xcb_randr_output_t  output,
    xcb_atom_t          property,
    xcb_atom_t          type,
    uint32_t            offset,
    uint32_t            length,
    uint8_t             _delete,
    uint8_t             pending
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_get_output_property_cookie_t ret = m_pFuncs->pfnXcbRandrGetOutputProperty(pConnection,
                                                                                        output,
                                                                                        property,
                                                                                        type,
                                                                                        offset,
                                                                                        length,
                                                                                        _delete,
                                                                                        pending);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetOutputProperty,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetOutputProperty(%p, %x, %x, %x, %x, %x, %x, %x)\n",
        pConnection,
        output,
        property,
        type,
        offset,
        length,
        _delete,
        pending);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
uint8_t* Dri3LoaderFuncsProxy::pfnXcbRandrGetOutputPropertyData(
    const xcb_randr_get_output_property_reply_t*  pReply
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    uint8_t* pRet = m_pFuncs->pfnXcbRandrGetOutputPropertyData(pReply);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetOutputPropertyData,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetOutputPropertyData(%p)\n",
        pReply);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_randr_get_output_property_reply_t* Dri3LoaderFuncsProxy::pfnXcbRandrGetOutputPropertyReply(
    xcb_connection_t*                       pConnection,
    xcb_randr_get_output_property_cookie_t  cookie,
    xcb_generic_error_t**                   ppError
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_get_output_property_reply_t* pRet = m_pFuncs->pfnXcbRandrGetOutputPropertyReply(pConnection,
                                                                                              cookie,
                                                                                              ppError);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetOutputPropertyReply,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetOutputPropertyReply(%p, %p, %p)\n",
        pConnection,
        &cookie,
        ppError);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_randr_get_providers_cookie_t Dri3LoaderFuncsProxy::pfnXcbRandrGetProviders(
    xcb_connection_t*  c,
    xcb_window_t       window
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_get_providers_cookie_t ret = m_pFuncs->pfnXcbRandrGetProviders(c,
                                                                             window);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetProviders,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetProviders(%p, %x)\n",
        c,
        window);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_randr_get_providers_reply_t* Dri3LoaderFuncsProxy::pfnXcbRandrGetProvidersReply(
    xcb_connection_t*                 c,
    xcb_randr_get_providers_cookie_t  cookie,
    xcb_generic_error_t**             e
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_get_providers_reply_t* pRet = m_pFuncs->pfnXcbRandrGetProvidersReply(c,
                                                                                   cookie,
                                                                                   e);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetProvidersReply,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetProvidersReply(%p, %p, %p)\n",
        c,
        &cookie,
        e);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_randr_provider_t* Dri3LoaderFuncsProxy::pfnXcbRandrGetProvidersProviders(
    const xcb_randr_get_providers_reply_t*  R
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_provider_t* pRet = m_pFuncs->pfnXcbRandrGetProvidersProviders(R);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetProvidersProviders,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetProvidersProviders(%p)\n",
        R);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
int Dri3LoaderFuncsProxy::pfnXcbRandrGetProvidersProvidersLength(
    const xcb_randr_get_providers_reply_t*  R
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnXcbRandrGetProvidersProvidersLength(R);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetProvidersProvidersLength,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetProvidersProvidersLength(%p)\n",
        R);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_randr_get_provider_info_cookie_t Dri3LoaderFuncsProxy::pfnXcbRandrGetProviderInfo(
    xcb_connection_t*     c,
    xcb_randr_provider_t  provider,
    xcb_timestamp_t       config_timestamp
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_get_provider_info_cookie_t ret = m_pFuncs->pfnXcbRandrGetProviderInfo(c,
                                                                                    provider,
                                                                                    config_timestamp);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetProviderInfo,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetProviderInfo(%p, %x, %x)\n",
        c,
        provider,
        config_timestamp);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_randr_get_provider_info_reply_t* Dri3LoaderFuncsProxy::pfnXcbRandrGetProviderInfoReply(
    xcb_connection_t*                     c,
    xcb_randr_get_provider_info_cookie_t  cookie,
    xcb_generic_error_t**                 e
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_get_provider_info_reply_t* pRet = m_pFuncs->pfnXcbRandrGetProviderInfoReply(c,
                                                                                          cookie,
                                                                                          e);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetProviderInfoReply,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetProviderInfoReply(%p, %p, %p)\n",
        c,
        &cookie,
        e);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
char* Dri3LoaderFuncsProxy::pfnXcbRandrGetProviderInfoName(
    const xcb_randr_get_provider_info_reply_t*  R
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    char* pRet = m_pFuncs->pfnXcbRandrGetProviderInfoName(R);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrGetProviderInfoName,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrGetProviderInfoName(%p)\n",
        R);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
xcb_randr_query_version_cookie_t Dri3LoaderFuncsProxy::pfnXcbRandrQueryVersion(
    xcb_connection_t*  c,
    uint32_t           major_version,
    uint32_t           minor_version
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_query_version_cookie_t ret = m_pFuncs->pfnXcbRandrQueryVersion(c,
                                                                             major_version,
                                                                             minor_version);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrQueryVersion,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrQueryVersion(%p, %x, %x)\n",
        c,
        major_version,
        minor_version);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
xcb_randr_query_version_reply_t* Dri3LoaderFuncsProxy::pfnXcbRandrQueryVersionReply(
    xcb_connection_t*                 c,
    xcb_randr_query_version_cookie_t  cookie,
    xcb_generic_error_t**             e
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    xcb_randr_query_version_reply_t* pRet = m_pFuncs->pfnXcbRandrQueryVersionReply(c,
                                                                                   cookie,
                                                                                   e);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XcbRandrQueryVersionReply,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XcbRandrQueryVersionReply(%p, %p, %p)\n",
        c,
        &cookie,
        e);
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
Window Dri3LoaderFuncsProxy::pfnXRootWindow(
    Display*  pDisplay,
    int       screenNum
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    Window ret = m_pFuncs->pfnXRootWindow(pDisplay,
                                          screenNum);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("XRootWindow,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "XRootWindow(%p, %x)\n",
        pDisplay,
        screenNum);
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
}

// =====================================================================================================================
Result Dri3Loader::Init(
    Platform* pPlatform)
{
    Result           result      = Result::Success;
    constexpr uint32 LibNameSize = 64;
    char LibNames[Dri3LoaderLibrariesCount][LibNameSize] = {
        "libX11-xcb.so.1",
        "libxcb.so.1",
        "libxshmfence.so.1",
        "libxcb-dri3.so.0",
        "libxcb-dri2.so.0",
        "libxcb-randr.so.0",
        "libxcb-sync.so.1",
        "libX11.so.6",
        "libxcb-present.so.0",
    };

    if (m_initialized == false)
    {
        // resolve symbols from libX11-xcb.so.1
        result = m_library[LibX11Xcb].Load(LibNames[LibX11Xcb]);
        if (result == Result::Success)
        {
            m_library[LibX11Xcb].GetFunction("XGetXCBConnection", &m_funcs.pfnXGetXCBConnection);
        }

        // resolve symbols from libxcb.so.1
        result = m_library[LibXcb].Load(LibNames[LibXcb]);
        if (result == Result::Success)
        {
            m_library[LibXcb].GetFunction("xcb_generate_id", &m_funcs.pfnXcbGenerateId);
            m_library[LibXcb].GetFunction("xcb_register_for_special_xge", &m_funcs.pfnXcbRegisterForSpecialXge);
            m_library[LibXcb].GetFunction("xcb_unregister_for_special_event", &m_funcs.pfnXcbUnregisterForSpecialEvent);
            m_library[LibXcb].GetFunction("xcb_wait_for_special_event", &m_funcs.pfnXcbWaitForSpecialEvent);
            m_library[LibXcb].GetFunction("xcb_get_extension_data", &m_funcs.pfnXcbGetExtensionData);
            m_library[LibXcb].GetFunction("xcb_prefetch_extension_data", &m_funcs.pfnXcbPrefetchExtensionData);
            m_library[LibXcb].GetFunction("xcb_request_check", &m_funcs.pfnXcbRequestCheck);
            m_library[LibXcb].GetFunction("xcb_get_geometry", &m_funcs.pfnXcbGetGeometry);
            m_library[LibXcb].GetFunction("xcb_get_geometry_reply", &m_funcs.pfnXcbGetGeometryReply);
            m_library[LibXcb].GetFunction("xcb_free_pixmap_checked", &m_funcs.pfnXcbFreePixmapChecked);
            m_library[LibXcb].GetFunction("xcb_intern_atom_reply", &m_funcs.pfnXcbInternAtomReply);
            m_library[LibXcb].GetFunction("xcb_intern_atom", &m_funcs.pfnXcbInternAtom);
            m_library[LibXcb].GetFunction("xcb_screen_allowed_depths_iterator", &m_funcs.pfnXcbScreenAllowedDepthsIterator);
            m_library[LibXcb].GetFunction("xcb_depth_next", &m_funcs.pfnXcbDepthNext);
            m_library[LibXcb].GetFunction("xcb_visualtype_next", &m_funcs.pfnXcbVisualtypeNext);
            m_library[LibXcb].GetFunction("xcb_setup_roots_iterator", &m_funcs.pfnXcbSetupRootsIterator);
            m_library[LibXcb].GetFunction("xcb_screen_next", &m_funcs.pfnXcbScreenNext);
            m_library[LibXcb].GetFunction("xcb_depth_visuals_iterator", &m_funcs.pfnXcbDepthVisualsIterator);
            m_library[LibXcb].GetFunction("xcb_get_setup", &m_funcs.pfnXcbGetSetup);
            m_library[LibXcb].GetFunction("xcb_flush", &m_funcs.pfnXcbFlush);
            m_library[LibXcb].GetFunction("xcb_discard_reply", &m_funcs.pfnXcbDiscardReply);
            m_library[LibXcb].GetFunction("xcb_change_property_checked", &m_funcs.pfnXcbChangePropertyChecked);
            m_library[LibXcb].GetFunction("xcb_delete_property_checked", &m_funcs.pfnXcbDeletePropertyChecked);
        }

        // resolve symbols from libxshmfence.so.1
        result = m_library[LibXshmFence].Load(LibNames[LibXshmFence]);
        if (result == Result::Success)
        {
            m_library[LibXshmFence].GetFunction("xshmfence_unmap_shm", &m_funcs.pfnXshmfenceUnmapShm);
            m_library[LibXshmFence].GetFunction("xshmfence_map_shm", &m_funcs.pfnXshmfenceMapShm);
            m_library[LibXshmFence].GetFunction("xshmfence_query", &m_funcs.pfnXshmfenceQuery);
            m_library[LibXshmFence].GetFunction("xshmfence_await", &m_funcs.pfnXshmfenceAwait);
            m_library[LibXshmFence].GetFunction("xshmfence_alloc_shm", &m_funcs.pfnXshmfenceAllocShm);
            m_library[LibXshmFence].GetFunction("xshmfence_trigger", &m_funcs.pfnXshmfenceTrigger);
            m_library[LibXshmFence].GetFunction("xshmfence_reset", &m_funcs.pfnXshmfenceReset);
        }

        // resolve symbols from libxcb-dri3.so.0
        result = m_library[LibXcbDri3].Load(LibNames[LibXcbDri3]);
        if (result == Result::Success)
        {
            m_library[LibXcbDri3].GetFunction("xcb_dri3_open", &m_funcs.pfnXcbDri3Open);
            m_library[LibXcbDri3].GetFunction("xcb_dri3_open_reply", &m_funcs.pfnXcbDri3OpenReply);
            m_library[LibXcbDri3].GetFunction("xcb_dri3_open_reply_fds", &m_funcs.pfnXcbDri3OpenReplyFds);
            m_library[LibXcbDri3].GetFunction("xcb_dri3_fence_from_fd_checked", &m_funcs.pfnXcbDri3FenceFromFdChecked);
            m_library[LibXcbDri3].GetFunction("xcb_dri3_pixmap_from_buffer_checked", &m_funcs.pfnXcbDri3PixmapFromBufferChecked);
            m_library[LibXcbDri3].GetFunction("xcb_dri3_query_version", &m_funcs.pfnXcbDri3QueryVersion);
            m_library[LibXcbDri3].GetFunction("xcb_dri3_query_version_reply", &m_funcs.pfnXcbDri3QueryVersionReply);
        }

        // resolve symbols from libxcb-dri2.so.0
        result = m_library[LibXcbDri2].Load(LibNames[LibXcbDri2]);
        if (result == Result::Success)
        {
            m_library[LibXcbDri2].GetFunction("xcb_dri2_connect", &m_funcs.pfnXcbDri2Connect);
            m_library[LibXcbDri2].GetFunction("xcb_dri2_connect_driver_name_length", &m_funcs.pfnXcbDri2ConnectDriverNameLength);
            m_library[LibXcbDri2].GetFunction("xcb_dri2_connect_driver_name", &m_funcs.pfnXcbDri2ConnectDriverName);
            m_library[LibXcbDri2].GetFunction("xcb_dri2_connect_reply", &m_funcs.pfnXcbDri2ConnectReply);
        }

        // resolve symbols from libxcb-randr.so.0
        result = m_library[LibXcbRandr].Load(LibNames[LibXcbRandr]);
        if (result == Result::Success)
        {
#if XCB_RANDR_SUPPORTS_LEASE
            m_library[LibXcbRandr].GetFunction("xcb_randr_create_lease", &m_funcs.pfnXcbRandrCreateLease);
            m_library[LibXcbRandr].GetFunction("xcb_randr_create_lease_reply", &m_funcs.pfnXcbRandrCreateLeaseReply);
            m_library[LibXcbRandr].GetFunction("xcb_randr_create_lease_reply_fds", &m_funcs.pfnXcbRandrCreateLeaseReplyFds);
#endif
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_screen_resources", &m_funcs.pfnXcbRandrGetScreenResources);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_screen_resources_reply", &m_funcs.pfnXcbRandrGetScreenResourcesReply);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_screen_resources_outputs", &m_funcs.pfnXcbRandrGetScreenResourcesOutputs);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_screen_resources_crtcs", &m_funcs.pfnXcbRandrGetScreenResourcesCrtcs);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_crtc_info", &m_funcs.pfnXcbRandrGetCrtcInfo);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_crtc_info_reply", &m_funcs.pfnXcbRandrGetCrtcInfoReply);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_output_info", &m_funcs.pfnXcbRandrGetOutputInfo);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_output_info_reply", &m_funcs.pfnXcbRandrGetOutputInfoReply);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_crtc_info_outputs", &m_funcs.pfnXcbRandrGetCrtcInfoOutputs);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_crtc_info_possible", &m_funcs.pfnXcbRandrGetCrtcInfoPossible);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_output_property", &m_funcs.pfnXcbRandrGetOutputProperty);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_output_property_data", &m_funcs.pfnXcbRandrGetOutputPropertyData);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_output_property_reply", &m_funcs.pfnXcbRandrGetOutputPropertyReply);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_providers", &m_funcs.pfnXcbRandrGetProviders);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_providers_reply", &m_funcs.pfnXcbRandrGetProvidersReply);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_providers_providers", &m_funcs.pfnXcbRandrGetProvidersProviders);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_providers_providers_length", &m_funcs.pfnXcbRandrGetProvidersProvidersLength);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_provider_info", &m_funcs.pfnXcbRandrGetProviderInfo);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_provider_info_reply", &m_funcs.pfnXcbRandrGetProviderInfoReply);
            m_library[LibXcbRandr].GetFunction("xcb_randr_get_provider_info_name", &m_funcs.pfnXcbRandrGetProviderInfoName);
            m_library[LibXcbRandr].GetFunction("xcb_randr_query_version", &m_funcs.pfnXcbRandrQueryVersion);
            m_library[LibXcbRandr].GetFunction("xcb_randr_query_version_reply", &m_funcs.pfnXcbRandrQueryVersionReply);
        }

        // resolve symbols from libxcb-sync.so.1
        result = m_library[LibXcbSync].Load(LibNames[LibXcbSync]);
        if (result == Result::Success)
        {
            m_library[LibXcbSync].GetFunction("xcb_sync_trigger_fence_checked", &m_funcs.pfnXcbSyncTriggerFenceChecked);
            m_library[LibXcbSync].GetFunction("xcb_sync_destroy_fence_checked", &m_funcs.pfnXcbSyncDestroyFenceChecked);
        }

        // resolve symbols from libX11.so.6
        result = m_library[LibX11].Load(LibNames[LibX11]);
        if (result == Result::Success)
        {
            m_library[LibX11].GetFunction("XGetVisualInfo", &m_funcs.pfnXGetVisualInfo);
            m_library[LibX11].GetFunction("XFree", &m_funcs.pfnXFree);
            m_library[LibX11].GetFunction("XRootWindow", &m_funcs.pfnXRootWindow);
        }

        // resolve symbols from libxcb-present.so.0
        result = m_library[LibXcbPresent].Load(LibNames[LibXcbPresent]);
        if (result == Result::Success)
        {
            m_library[LibXcbPresent].GetFunction("xcb_present_query_version", &m_funcs.pfnXcbPresentQueryVersion);
            m_library[LibXcbPresent].GetFunction("xcb_present_query_version_reply", &m_funcs.pfnXcbPresentQueryVersionReply);
            m_library[LibXcbPresent].GetFunction("xcb_present_select_input_checked", &m_funcs.pfnXcbPresentSelectInputChecked);
            m_library[LibXcbPresent].GetFunction("xcb_present_pixmap_checked", &m_funcs.pfnXcbPresentPixmapChecked);
        }

        if (m_library[LibXcbDri3].IsLoaded() == false)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            m_library[LibXcbDri3].GetFunction("xcb_dri3_id", &m_pXcbDri3Id);
        }
        if (m_library[LibXcbPresent].IsLoaded() == false)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            m_library[LibXcbPresent].GetFunction("xcb_present_id", &m_pXcbPresentId);
        }
        if (m_library[LibXcbDri2].IsLoaded() == false)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            m_library[LibXcbDri2].GetFunction("xcb_dri2_id", &m_pXcbDri2Id);
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

} // Amdgpu
} // Pal
