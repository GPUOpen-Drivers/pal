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
    m_connectorId(connectorId),
    m_physicalDimension(physicalDimension),
    m_physicalResolution(physicalResolution)
{
}

// =====================================================================================================================
void Screen::Destroy()
{
}

// =====================================================================================================================
Result Screen::Init()
{
    return Result::Success;
}

// =====================================================================================================================
Result Screen::GetProperties(
    ScreenProperties* pInfo
    ) const
{
    pInfo->hDisplay = nullptr;
    // hard code the screen name for now.
    const char ScreenName[] = "AmdMonitor";
    Util::Strncpy(pInfo->displayName, ScreenName, sizeof(pInfo->displayName));
    pInfo->screen = m_connectorId;
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
    pInfo->supportWindowedGetScanLine = false;
    // Linux don't have pn source id concept.
    pInfo->vidPnSourceId = 0;

    return Result::Success;
}

// =====================================================================================================================
Result Screen::GetScreenModeList(
    uint32*     pScreenModeCount,
    ScreenMode* pScreenModeList
    ) const
{
    return static_cast<Device*>(m_pDevice)->QueryScreenModesForConnector(m_connectorId, pScreenModeCount, pScreenModeList);
}

}
}
