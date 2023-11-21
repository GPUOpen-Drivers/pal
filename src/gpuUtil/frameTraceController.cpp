/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_BUILD_RDF

#include "palHashMapImpl.h"
#include "core/cmdBuffer.h"
#include "core/engine.h"
#include "util/ddStructuredReader.h"

#include "frameTraceController.h"

using namespace Pal;
using DevDriver::StructuredValue;

namespace GpuUtil
{

// =====================================================================================================================
FrameTraceController::FrameTraceController(
    Platform* pPlatform)
    :
    m_pPlatform(pPlatform),
    m_pCurrentDevice(nullptr),
    m_supportedGpuMask(1),
    m_frameCount(0),
    m_numPrepFrames(0),
    m_captureStartIndex(0),
    m_currentTraceStartIndex(0),
    m_captureFrameCount(1),
    m_pCurrentCmdBuffer(nullptr),
    m_pTraceSession(pPlatform->GetTraceSession())
{
}

// =====================================================================================================================
FrameTraceController::~FrameTraceController()
{
}

// =====================================================================================================================
void FrameTraceController::OnConfigUpdated(
    StructuredValue* pJsonConfig)
{
    StructuredValue value;

    if (pJsonConfig->GetValueByKey("numPrepFrames", &value))
    {
        m_numPrepFrames = value.GetUint32Or(0);
    }

    if (pJsonConfig->GetValueByKey("captureStartFrame", &value))
    {
        m_captureStartIndex = value.GetUint32Or(0);
    }

    if (pJsonConfig->GetValueByKey("captureFrameCount", &value))
    {
        m_captureFrameCount = value.GetUint32Or(1);
    }
}

// =====================================================================================================================
void FrameTraceController::OnFrameUpdated()
{
    TraceSessionState traceSessionState = m_pTraceSession->GetTraceSessionState();

    if (traceSessionState == TraceSessionState::Requested)
    {
        if (((m_captureStartIndex + m_numPrepFrames) == 0 || m_frameCount == (m_captureStartIndex + m_numPrepFrames)) &&
             (m_pTraceSession->AcceptTrace(this, m_supportedGpuMask) == Result::Success))
        {
            m_currentTraceStartIndex = m_frameCount;

            if (m_pTraceSession->BeginTrace() == Result::Success)
            {
                m_pTraceSession->SetTraceSessionState(TraceSessionState::Running);
            }
        }
    }
    else if (traceSessionState == TraceSessionState::Running)
    {
        if (m_frameCount == m_currentTraceStartIndex + m_captureFrameCount + m_numPrepFrames)
        {
            if (m_pTraceSession->EndTrace() == Result::Success)
            {
                m_pTraceSession->SetTraceSessionState(TraceSessionState::Waiting);
            }
        }
    }
}

// =====================================================================================================================
Result FrameTraceController::OnBeginGpuWork(
    uint32       gpuIndex,
    ICmdBuffer** ppCmdBuf)
{
    m_pCurrentDevice = m_pPlatform->GetDevice(gpuIndex);
    Result result = Result::Success;

    if (m_pCurrentCmdBuffer != nullptr)
    {
        *ppCmdBuf = m_pCurrentCmdBuffer;
    }
    else
    {
        result = Result::ErrorUnknown;
    }

    return result;
}

// =====================================================================================================================
Result FrameTraceController::OnEndGpuWork(
    uint32       gpuIndex,
    ICmdBuffer** ppCmdBuf)
{
    m_pCurrentDevice = m_pPlatform->GetDevice(gpuIndex);
    Result result = Result::Success;

    if (m_pCurrentCmdBuffer != nullptr)
    {
        m_pCurrentCmdBuffer->SetEndTraceFlag(1);
        *ppCmdBuf = m_pCurrentCmdBuffer;
    }
    else
    {
        result = Result::ErrorUnknown;
    }

    return result;
}

// =====================================================================================================================
void FrameTraceController::UpdateFrame(
    CmdBuffer* pCmdBuffer)
{
    Util::MutexAuto lock(&m_framePresentLock);

    m_pCurrentCmdBuffer = pCmdBuffer;
    Util::AtomicIncrement(&m_frameCount);
    OnFrameUpdated();

    m_pCurrentCmdBuffer = nullptr;
}

// =====================================================================================================================
void FrameTraceController::FinishTrace()
{
    m_pTraceSession->FinishTrace();
    m_pTraceSession->SetTraceSessionState(TraceSessionState::Completed);
}
}
#endif
