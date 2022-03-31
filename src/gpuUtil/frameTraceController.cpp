/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "frameTraceController.h"
#include "palHashMapImpl.h"

using namespace Pal;

namespace GpuUtil
{

// =====================================================================================================================
FrameTraceController::FrameTraceController(
    IPlatform* pPlatform)
    :
    m_pPlatform(pPlatform),
    m_captureStartIndex(0),
    m_currentTraceStartIndex(0),
    m_captureFrameCount(1),
    m_supportedGpuMask(1),
    m_frameCount(0),
    m_currentCmdBuffer(nullptr)
{
    m_pTraceSession = m_pPlatform->GetTraceSession();
}

// =====================================================================================================================
FrameTraceController::~FrameTraceController()
{
}

// =====================================================================================================================
void FrameTraceController::OnFrameUpdated()
{
    TraceSessionState traceSessionState = m_pTraceSession->GetTraceSessionState();

    switch (traceSessionState)
    {
        case TraceSessionState::Requested:
        {
            if ((m_captureStartIndex == 0 || m_frameCount == m_captureStartIndex) &&
                (m_pTraceSession->AcceptTrace(this, m_supportedGpuMask) == Result::Success))
            {
                m_currentTraceStartIndex = m_frameCount;

                if (m_pTraceSession->BeginTrace() == Result::Success)
                {
                    m_pTraceSession->SetTraceSessionState(TraceSessionState::Running);
                }
            }
            break;
        }

        case TraceSessionState::Running:
        {
            if (m_frameCount == m_currentTraceStartIndex + m_captureFrameCount)
            {
                if (m_pTraceSession->EndTrace() == Result::Success)
                {
                    m_pTraceSession->SetTraceSessionState(TraceSessionState::Waiting);
                }
            }
            break;
        }

        case TraceSessionState::Waiting:
        {
            m_pTraceSession->FinishTrace();
            m_pTraceSession->SetTraceSessionState(TraceSessionState::Completed);
            break;
        }
    }
}

// =====================================================================================================================
Result FrameTraceController::OnBeginGpuWork(
    uint32       gpuIndex,
    ICmdBuffer** ppCmdBuf)
{
    Result result = Result::Success;

    if (m_currentCmdBuffer != nullptr)
    {
        *ppCmdBuf = m_currentCmdBuffer;
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
    Result result = Result::Success;

    if (m_currentCmdBuffer != nullptr)
    {
        *ppCmdBuf = m_currentCmdBuffer;
    }
    else
    {
        result = Result::ErrorUnknown;
    }

    return result;
}

// =====================================================================================================================
void FrameTraceController::UpdateFrame(
    Pal::ICmdBuffer* pCmdBuffer)
{
    Util::MutexAuto lock(&m_framePresentLock);

    m_currentCmdBuffer = pCmdBuffer;
    Util::AtomicIncrement(&m_frameCount);
    OnFrameUpdated();

    m_currentCmdBuffer = nullptr;
}
}
#endif
