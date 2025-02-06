/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::PrivateScreenEnable);
    const Result result = PrivateScreenDecorator::Enable(info);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

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
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::PrivateScreenDisable);
    const Result result = PrivateScreenDecorator::Disable();

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

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
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::PrivateScreenBlank);
    const Result result = PrivateScreenDecorator::Blank();

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

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
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::PrivateScreenPresent);
    const Result result = PrivateScreenDecorator::Present(presentInfo);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

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
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::PrivateScreenSetGammaRamp);
    const Result result = PrivateScreenDecorator::SetGammaRamp(pGammaRamp);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

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
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::PrivateScreenSetPowerMode);
    const Result result = PrivateScreenDecorator::SetPowerMode(powerMode);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

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
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::PrivateScreenSetDisplayMode);
    const Result result = PrivateScreenDecorator::SetDisplayMode(displayMode);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

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
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::PrivateScreenSetColorMatrix);
    const Result result = PrivateScreenDecorator::SetColorMatrix(matrix);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

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
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::PrivateScreenSetEventAfterVsync);
    const Result result = PrivateScreenDecorator::SetEventAfterVsync(hEvent, delayInUs, repeated);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

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
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::PrivateScreenEnableAudio);
    const Result result = PrivateScreenDecorator::EnableAudio(enable);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

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
