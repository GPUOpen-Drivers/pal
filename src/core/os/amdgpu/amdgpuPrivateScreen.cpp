/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/privateScreen.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
Result PrivateScreen::OsInitProperties()
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
Result PrivateScreen::OsGetPresentStats(
    PrivateScreenPresentStats* pStats)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
Result PrivateScreen::OsEnable(
    const PrivateScreenEnableInfo& enableInfo)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
Result PrivateScreen::OsDisable()
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
Result PrivateScreen::OsBlank()
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
Result PrivateScreen::OsPresent(
    const PrivateScreenPresentInfo& presentInfo)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
Result PrivateScreen::OsGetScanLine(
    int32* pScanLine)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
Result PrivateScreen::OsGetConnectorProperties(
    uint32*                           pDataSize,
    PrivateScreenConnectorProperties* pConnectorProperties)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
Result PrivateScreen::OsSetGammaRamp(
    const GammaRamp* pGammaRamp)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
Result PrivateScreen::OsSetPowerMode(
    PrivateDisplayPowerState powerMode)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
Result PrivateScreen::OsSetDisplayMode(
    const PrivateDisplayMode& displayMode)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
Result PrivateScreen::OsSetColorMatrix(
    const ColorTransform& matrix)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
Result PrivateScreen::OsSetEventAfterVsync(
    OsExternalHandle hEvent,
    uint32           delayInUs,
    bool             repeated)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
Result PrivateScreen::OsGetHdcpStatus(
    HdcpStatus* pStatus)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
Result PrivateScreen::OsEnableAudio(
    bool enable)
{
    return Result::ErrorUnavailable;
}

} // Pal
