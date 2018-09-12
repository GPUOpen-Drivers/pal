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
#include "core/os/lnx/drmLoader.h"
#include "core/os/lnx/lnxDevice.h"
#include "core/os/lnx/lnxScreen.h"
#include "palVectorImpl.h"

using namespace Util;

namespace Pal
{

namespace Linux
{
// =====================================================================================================================
Screen::Screen(
        Device*      pDevice,
        Extent2d     physicalDimension,
        Extent2d     physicalResolution,
        uint32       connectorId)
    :
    m_pDevice(pDevice),
    m_physicalDimension(physicalDimension),
    m_physicalResolution(physicalResolution)
{
    memset(&m_wsiScreenProp, 0, sizeof(m_wsiScreenProp));

    m_wsiScreenProp.connectorId = connectorId;
    m_wsiScreenProp.drmMasterFd = InvalidFd;
}

// =====================================================================================================================
void Screen::Destroy()
{
}

// =====================================================================================================================
Result Screen::Init()
{
    // The displayName should be gotten from KMS, however there is no this kind of interface so far. Hard code it.
    Util::Strncpy(m_wsiScreenProp.displayName, "monitor", sizeof(m_wsiScreenProp.displayName));

    return Result::Success;
}

// =====================================================================================================================
Result Screen::GetProperties(
    ScreenProperties* pInfo
    ) const
{
    pInfo->hDisplay = nullptr;
    pInfo->screen   = m_wsiScreenProp.connectorId;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 402
    pInfo->physicalDimension.width  = m_physicalDimension.width;
    pInfo->physicalDimension.height = m_physicalDimension.height;

    pInfo->physicalResolution.width  = m_physicalResolution.width;
    pInfo->physicalResolution.height = m_physicalResolution.height;
#endif

    pInfo->pMainDevice = m_pDevice;

    // Don't support cross display for now.
    pInfo->otherDeviceCount = 0;

    // Todo.
    pInfo->supportWindowedWaitForVerticalBlank = false;
    pInfo->supportWindowedGetScanLine          = false;

    // Linux don't have pn source id concept.
    pInfo->vidPnSourceId = 0;

    pInfo->wsiScreenProp = m_wsiScreenProp;

    Util::Strncpy(pInfo->displayName, m_wsiScreenProp.displayName, sizeof(pInfo->displayName));

    return Result::Success;
}

// =====================================================================================================================
Result Screen::GetScreenModeList(
    uint32*     pScreenModeCount,
    ScreenMode* pScreenModeList
    ) const
{
    return static_cast<Device*>(m_pDevice)->QueryScreenModesForConnector(m_wsiScreenProp.connectorId,
                                                                         pScreenModeCount,
                                                                         pScreenModeList);
}

// =====================================================================================================================
Result Screen::AcquireScreenAccess(
    OsDisplayHandle hDisplay,
    WsiPlatform     wsiPlatform)
{
    Result ret = Result::ErrorPrivateScreenUsed;

    if (m_wsiScreenProp.drmMasterFd == InvalidFd)
    {
        ret = WindowSystem::AcquireScreenAccess(m_pDevice,
                                                hDisplay,
                                                wsiPlatform,
                                                m_wsiScreenProp.connectorId,
                                                &m_wsiScreenProp.randrOutput,
                                                &m_wsiScreenProp.drmMasterFd);

        const DrmLoaderFuncs& drmLoader = m_pDevice->GetDrmLoaderFuncs();
        // Get crtc id
        uint32 crtcId = 0;
        int32 fd = m_wsiScreenProp.drmMasterFd;
        drmModeRes* pResources = drmLoader.pfnDrmModeGetResources(fd);

        drmModeConnector* pDrmModeConnector = nullptr;
        for (int i = 0; i < pResources->count_connectors; i++)
        {
            if (pResources->connectors[i] == m_wsiScreenProp.connectorId)
            {
                drmModeConnector* pConnector =
                    drmLoader.pfnDrmModeGetConnector(fd, pResources->connectors[i]);

                if ((pConnector->connection == DRM_MODE_CONNECTED) && (pConnector->count_modes > 0))
                {
                    pDrmModeConnector = pConnector;
                    break;
                }
            }
        }

        // Get the CRTC which is connecting to the pDrmModeConnector.
        if ((pDrmModeConnector != nullptr) && (pDrmModeConnector->encoder_id != 0))
        {
            drmModeEncoderPtr pEncoder = drmLoader.pfnDrmModeGetEncoder(fd, pDrmModeConnector->encoder_id);
            if (pEncoder != nullptr)
            {
                crtcId = pEncoder->crtc_id;
            }

            drmLoader.pfnDrmModeFreeEncoder(pEncoder);
            pEncoder = nullptr;
        }

        // Find an idle CRTC which is not connected to framebuffer.
        if (crtcId == 0)
        {
            for (int c = 0; c < pResources->count_crtcs; c++)
            {
                drmModeCrtcPtr pCrtc = drmLoader.pfnDrmModeGetCrtc(fd, pResources->crtcs[c]);
                if (pCrtc && (pCrtc->buffer_id == 0))
                {
                    crtcId = pCrtc->crtc_id;
                }

                drmLoader.pfnDrmModeFreeCrtc(pCrtc);
            }
        }

        drmLoader.pfnDrmModeFreeConnector(pDrmModeConnector);
        m_wsiScreenProp.crtcId = crtcId;

        drmLoader.pfnDrmModeFreeResources(pResources);
    }

    return ret;
}

// =====================================================================================================================
Result Screen::ReleaseScreenAccess()
{
    Result ret = Result::ErrorPrivateScreenNotEnabled;

    if (m_wsiScreenProp.drmMasterFd != InvalidFd)
    {
        close(m_wsiScreenProp.drmMasterFd);
        m_wsiScreenProp.drmMasterFd = InvalidFd;
        ret = Result::Success;
    }

    return ret;
}

// =====================================================================================================================
Result Screen::SetRandrOutput(
    uint32 randrOutput)
{
    m_wsiScreenProp.randrOutput = randrOutput;

    return Result::Success;
}

}
}
