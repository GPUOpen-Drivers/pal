/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_DEVELOPER_BUILD

#include "core/layers/interfaceLogger/interfaceLoggerDevice.h"
#include "core/layers/interfaceLogger/interfaceLoggerPlatform.h"
#include "core/layers/interfaceLogger/interfaceLoggerPrivateScreen.h"

namespace Pal
{
namespace InterfaceLogger
{

// =====================================================================================================================
PrivateScreen::PrivateScreen(
    IPrivateScreen*  pNextScreen,
    DeviceDecorator* pDevice,
    uint32           deviceIdx,
    uint32           objectId)
    :
    PrivateScreenDecorator(pNextScreen, pDevice, deviceIdx),
    m_pPlatform(static_cast<Platform*>(pDevice->GetPlatform())),
    m_objectId(objectId)
{
}

// =====================================================================================================================
Result PrivateScreen::Enable(
    const PrivateScreenEnableInfo& info)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::PrivateScreenEnable;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = PrivateScreenDecorator::Enable(info);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("info", info);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result PrivateScreen::Disable()
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::PrivateScreenDisable;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = PrivateScreenDecorator::Disable();
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
Result PrivateScreen::Blank()
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::PrivateScreenBlank;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = PrivateScreenDecorator::Blank();
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
Result PrivateScreen::Present(
    const PrivateScreenPresentInfo& presentInfo)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::PrivateScreenPresent;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = PrivateScreenDecorator::Present(presentInfo);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("presentInfo", presentInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result PrivateScreen::SetGammaRamp(
    const GammaRamp* pGammaRamp)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::PrivateScreenSetGammaRamp;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = PrivateScreenDecorator::SetGammaRamp(pGammaRamp);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();

        if (pGammaRamp != nullptr)
        {
            pLogContext->KeyAndStruct("gammaRamp", *pGammaRamp);
        }
        else
        {
            pLogContext->KeyAndNullValue("gammaRamp");
        }

        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result PrivateScreen::SetPowerMode(
    PrivateDisplayPowerState powerMode)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::PrivateScreenSetPowerMode;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = PrivateScreenDecorator::SetPowerMode(powerMode);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndEnum("powerMode", powerMode);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result PrivateScreen::SetDisplayMode(
    const PrivateDisplayMode& displayMode)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::PrivateScreenSetDisplayMode;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = PrivateScreenDecorator::SetDisplayMode(displayMode);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("displayMode", displayMode);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result PrivateScreen::SetColorMatrix(
    const ColorTransform& matrix)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::PrivateScreenSetColorMatrix;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = PrivateScreenDecorator::SetColorMatrix(matrix);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("matrix", matrix);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result PrivateScreen::SetEventAfterVsync(
    OsExternalHandle hEvent,
    uint32           delayInUs,
    bool             repeated)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::PrivateScreenSetEventAfterVsync;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = PrivateScreenDecorator::SetEventAfterVsync(hEvent, delayInUs, repeated);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("delayInUs", delayInUs);
        pLogContext->KeyAndValue("repeated", repeated);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result PrivateScreen::EnableAudio(
    bool enable)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::PrivateScreenEnableAudio;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = PrivateScreenDecorator::EnableAudio(enable);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("enable", enable);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

} // InterfaceLogger
} // Pal

#endif
