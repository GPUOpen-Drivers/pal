/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/layers/interfaceLogger/interfaceLoggerGpuEvent.h"
#include "core/layers/interfaceLogger/interfaceLoggerGpuMemory.h"
#include "core/layers/interfaceLogger/interfaceLoggerPlatform.h"

namespace Pal
{
namespace InterfaceLogger
{

// =====================================================================================================================
GpuEvent::GpuEvent(
    IGpuEvent*    pNextGpuEvent,
    const Device* pDevice,
    uint32        objectId)
    :
    GpuEventDecorator(pNextGpuEvent, pDevice),
    m_pPlatform(static_cast<Platform*>(pDevice->GetPlatform())),
    m_objectId(objectId)
{
}

// =====================================================================================================================
Result GpuEvent::Set()
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::GpuEventSet;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = GpuEventDecorator::Set();
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
Result GpuEvent::Reset()
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::GpuEventReset;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = GpuEventDecorator::Reset();
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
void GpuEvent::Destroy()
{
    // Note that we can't time a Destroy call.
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::GpuEventDestroy;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    funcInfo.postCallTime = funcInfo.preCallTime;

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        m_pPlatform->LogEndFunc(pLogContext);
    }

    GpuEventDecorator::Destroy();
}

} // InterfaceLogger
} // Pal
