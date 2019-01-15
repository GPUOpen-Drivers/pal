/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/interfaceLogger/interfaceLoggerDevice.h"
#include "core/layers/interfaceLogger/interfaceLoggerImage.h"
#include "core/layers/interfaceLogger/interfaceLoggerPlatform.h"
#include "core/layers/interfaceLogger/interfaceLoggerScreen.h"

namespace Pal
{
namespace InterfaceLogger
{

// =====================================================================================================================
Screen::Screen(
    IScreen*          pNextScreen,
    DeviceDecorator** ppDevices,
    uint32            deviceCount,
    uint32            objectId)
    :
    ScreenDecorator(pNextScreen, ppDevices, deviceCount),
    m_pPlatform(static_cast<Platform*>(ppDevices[0]->GetPlatform())),
    m_objectId(objectId)
{
}

// =====================================================================================================================
Result Screen::IsImplicitFullscreenOwnershipSafe(
    OsDisplayHandle hDisplay,
    OsWindowHandle  hWindow,
    Extent2d        imageExtent
    ) const
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::ScreenIsImplicitFullscreenOwnershipSafe;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = ScreenDecorator::IsImplicitFullscreenOwnershipSafe(hDisplay, hWindow, imageExtent);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("hDisplay", hDisplay);
        pLogContext->KeyAndValue("hWindow", *reinterpret_cast<uint64*>(&hWindow));
        pLogContext->KeyAndStruct("imageExtent", imageExtent);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Screen::TakeFullscreenOwnership(
    const IImage& image)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::ScreenTakeFullscreenOwnership;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = ScreenDecorator::TakeFullscreenOwnership(image);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndObject("image", &image);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Screen::ReleaseFullscreenOwnership()
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::ScreenReleaseFullscreenOwnership;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = ScreenDecorator::ReleaseFullscreenOwnership();
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Screen::SetGammaRamp(
    const GammaRamp& gammaRamp)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::ScreenSetGammaRamp;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = ScreenDecorator::SetGammaRamp(gammaRamp);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("gammaRamp", gammaRamp);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result Screen::WaitForVerticalBlank() const
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::ScreenWaitForVerticalBlank;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = ScreenDecorator::WaitForVerticalBlank();
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
void Screen::Destroy()
{
    // Note that we can't time a Destroy call.
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::ScreenDestroy;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    funcInfo.postCallTime = funcInfo.preCallTime;

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        m_pPlatform->LogEndFunc(pLogContext);
    }

    ScreenDecorator::Destroy();
}

} // InterfaceLogger
} // Pal
