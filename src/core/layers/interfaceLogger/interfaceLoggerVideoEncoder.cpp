/*
 *******************************************************************************
 *
 * Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "core/layers/interfaceLogger/interfaceLoggerDevice.h"
#include "core/layers/interfaceLogger/interfaceLoggerGpuMemory.h"
#include "core/layers/interfaceLogger/interfaceLoggerVideoEncoder.h"
#include "core/layers/interfaceLogger/interfaceLoggerPlatform.h"

namespace Pal
{
namespace InterfaceLogger
{

// =====================================================================================================================
VideoEncoder::VideoEncoder(
    IVideoEncoder*  pNextEncoder,
    const Device*   pDevice,
    uint32          objectId)
    :
    VideoEncoderDecorator(pNextEncoder, pDevice),
    m_pPlatform(static_cast<Platform*>(pDevice->GetPlatform())),
    m_objectId(objectId)
{
}

// =====================================================================================================================
Result VideoEncoder::BindGpuMemory(
    IGpuMemory* pGpuMemory,
    gpusize     offset)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::VideoEncoderBindGpuMemory;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = VideoEncoderDecorator::BindGpuMemory(pGpuMemory, offset);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndObject("gpuMemory", pGpuMemory);
        pLogContext->KeyAndValue("offset", offset);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
void VideoEncoder::Destroy()
{
    // Note that we can't time a Destroy call.
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::VideoEncoderDestroy;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    funcInfo.postCallTime = funcInfo.preCallTime;

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        m_pPlatform->LogEndFunc(pLogContext);
    }

    VideoEncoderDecorator::Destroy();
}

// =====================================================================================================================
uint32 VideoEncoder::GetFeedbackCount()
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::VideoEncoderGetFeedbackCount;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const uint32 result   = VideoEncoderDecorator::GetFeedbackCount();
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginOutput();
        pLogContext->KeyAndValue("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result VideoEncoder::GetFeedback(
    VideoEncodeFeedback*     pFeedback,
    VideoEncodeFeedbackFlags flags)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::VideoEncoderGetFeedback;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = VideoEncoderDecorator::GetFeedback(pFeedback, flags);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndVideoEncodeFeedbackFlags("flags", flags);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        if (pFeedback != nullptr)
        {
            pLogContext->KeyAndStruct("feedback", *pFeedback);
        }
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

} // InterfaceLogger
} // Pal
