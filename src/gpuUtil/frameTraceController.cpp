/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
using namespace std::chrono_literals;

namespace GpuUtil
{

// =====================================================================================================================
FrameTraceController::FrameTraceController(
    Platform* pPlatform)
    :
    m_pPlatform(pPlatform),
    m_supportedGpuMask(1),
    m_captureMode(CaptureMode::Relative),
    m_frameCount(0),
    m_prepStartIndex(0),
    m_frameTraceAccepted(0),
    m_numPrepFrames(0),
    m_captureFrameCount(1),
    m_framePresentLock(),
    m_pTraceSession(pPlatform->GetTraceSession()),
    m_pQueue(nullptr),
    m_pCmdBufTraceBegin(nullptr),
    m_pCmdBufTraceEnd(nullptr),
    m_pTraceEndFence(nullptr)
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 908
    ,
    m_pCmdBufTracePrepare(nullptr)
#endif
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

    // Configures whether the capture mode is 'relative' or 'absolute'
    if (pJsonConfig->GetValueByKey("captureMode", &value))
    {
        char buffer[64] = {'\0'};

        if (value.GetStringCopy(buffer))
        {
            if (strncmp(buffer, "relative", strlen(buffer)) == 0)
            {
                m_captureMode = CaptureMode::Relative;
            }
            else if (strncmp(buffer, "absolute", strlen(buffer)) == 0)
            {
                m_captureMode = CaptureMode::Absolute;
            }
        }
    }

    // Configures the start index of the trace
    // This is relative to when the trace request is received if captureMode is 'relative',
    // or an absolute index if captureMode is 'absolute'
    if (pJsonConfig->GetValueByKey("preparationStartIndex", &value))
    {
        m_prepStartIndex = value.GetUint32Or(0);
    }

    // Number of frames run in "preparing" state before the trace transitions to running
    if (pJsonConfig->GetValueByKey("numPrepFrames", &value))
    {
        m_numPrepFrames = value.GetUint32Or(0);
    }

    // Duration of the trace
    if (pJsonConfig->GetValueByKey("captureFrameCount", &value))
    {
        m_captureFrameCount = value.GetUint32Or(1);

        // We can't capture 0 frames
        if (m_captureFrameCount < 1)
        {
            m_captureFrameCount = 1;
        }
    }
}

// =====================================================================================================================
Result FrameTraceController::OnTraceRequested()
{
    Result result = Result::Success;

    if ((m_captureMode == CaptureMode::Absolute) && (m_frameCount >= m_prepStartIndex))
    {
        result = Result::ErrorInitializationFailed;
    }

    return result;
}

// =====================================================================================================================
void FrameTraceController::OnFrameUpdated()
{
    const TraceSessionState sessionState = m_pTraceSession->GetTraceSessionState();

    switch (sessionState)
    {
    case TraceSessionState::Requested:
    {
        if (m_pTraceSession->IsCancelingTrace() == false)
        {
            switch (m_captureMode)
            {
            case CaptureMode::Relative:
                // Once 'prepStartIndex' hits 0, move to accepting the trace.
                // Otherwise, decrement counter and wait for next frame.
                if (m_prepStartIndex > 0)
                {
                    m_prepStartIndex--;
                }
                else
                {
                    Result result = AcceptTrace();
                    PAL_ASSERT(result == Result::Success);
                }
                break;
            case CaptureMode::Absolute:
                PAL_ALERT(m_frameCount > m_prepStartIndex);

                // In the absolute case, prepStartIndex references a specific
                // frame index. So we do not decrement, like above, but instead
                // wait for this index to be reached.
                if (m_frameCount == m_prepStartIndex)
                {
                    Result result = AcceptTrace();
                    PAL_ASSERT(result == Result::Success);
                }
                break;
            }
        }
        else
        {
            // If trace is canceled, finish it as fast as possible ie. move requested->preparing immediately.
            if (m_pTraceSession->AcceptTrace(this, m_supportedGpuMask) == Result::Success)
            {
                m_frameTraceAccepted = m_frameCount + 1; // Begin the next frame
                m_pTraceSession->SetTraceSessionState(TraceSessionState::Preparing);
            }
        }
        break;
    }
    case TraceSessionState::Preparing:
    {
        // Move from Preparing -> Running if the number of prep frames has elapsed
        if ((m_frameCount == (m_frameTraceAccepted + m_numPrepFrames)) || (m_pTraceSession->IsCancelingTrace()))
        {
            Result result = BeginTrace();
            PAL_ASSERT(result == Result::Success);
        }
        break;
    }
    case TraceSessionState::Running:
    {
        // Move from Running -> Waiting once the requested # of frames has been processed
        if ((m_frameCount == (m_frameTraceAccepted + m_numPrepFrames + m_captureFrameCount)) ||
            (m_pTraceSession->IsCancelingTrace()))
        {
            Result result = m_pTraceSession->EndTrace();

            if (result == Result::Success)
            {
                // Update the Trace Session State before submitting the GPU work
                // because the PAL submission code itself will call back into
                // FrameTraceController::FinishTrace() and set the Trace Session State to Completed.
                // The expected flow is therefore that we set the state to Waiting now, then
                // the PAL submission code path will call into FrameTraceController::FinishTrace()
                // and the state will be set to Completed
                m_pTraceSession->SetTraceSessionState(TraceSessionState::Waiting);

                result = SubmitEndTraceGpuWork();
            }

            PAL_ASSERT(result == Result::Success);
        }
        break;
    }
    case TraceSessionState::Waiting:
    {
        Result result = m_pTraceEndFence->GetStatus();

        // The submission associated with the fence should be done by now.
        // If it isn't, something went wrong. Try waiting before ending the trace.
        if (result != Pal::Result::Success)
        {
            PAL_ALERT_ALWAYS_MSG("FrameTraceController end trace fence is not ready");

            Device* pDevice = static_cast<Queue*>(m_pQueue)->GetDevice();
            result = pDevice->WaitForFences(1, &m_pTraceEndFence, true, 2s);
            PAL_ASSERT(result == Result::Success);
        }

        FinishTrace();

        break;
    }
    default:
        break;
    }
}

// =====================================================================================================================
Result FrameTraceController::AcceptTrace()
{
    Result result = m_pTraceSession->AcceptTrace(this, m_supportedGpuMask);

    if (result == Result::Success)
    {
        m_pTraceSession->SetTraceSessionState(TraceSessionState::Preparing);
        m_frameTraceAccepted = m_frameCount;

        // If we don't have any preparation frames, start the trace immediately
        if (m_numPrepFrames == 0)
        {
            result = BeginTrace();
        }
    }

    return result;
}

// =====================================================================================================================
Result FrameTraceController::BeginTrace()
{
    Result result = m_pTraceSession->BeginTrace();

    if (result == Result::Success)
    {
        result = SubmitBeginTraceGpuWork();
        m_pTraceSession->SetTraceSessionState(TraceSessionState::Running);
    }

    return result;
}

// =====================================================================================================================
// Submit the GPU command buffer to begin a trace
Result FrameTraceController::SubmitBeginTraceGpuWork() const
{
    PAL_ASSERT(m_pCmdBufTraceBegin != nullptr);
    Result result = m_pCmdBufTraceBegin->End();

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 908
    PAL_ASSERT(m_pCmdBufTracePrepare != nullptr);

    if (result == Result::Success)
    {
        result = m_pCmdBufTracePrepare->End();
    }
#endif

    if (result == Result::Success)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 908
        ICmdBuffer* cmdBuffers[2] = { m_pCmdBufTracePrepare, m_pCmdBufTraceBegin };

        PerSubQueueSubmitInfo perSubQueueInfo = {};
        perSubQueueInfo.cmdBufferCount        = 2;
        perSubQueueInfo.ppCmdBuffers          = cmdBuffers;
#else
        PerSubQueueSubmitInfo perSubQueueInfo = {};
        perSubQueueInfo.cmdBufferCount        = 1;
        perSubQueueInfo.ppCmdBuffers          = &m_pCmdBufTraceBegin;
#endif

        MultiSubmitInfo submitInfo      = {};
        submitInfo.perSubQueueInfoCount = 1;
        submitInfo.pPerSubQueueInfo     = &perSubQueueInfo;

        result = m_pQueue->Submit(submitInfo);
    }

    return result;
}

// =====================================================================================================================
// Submit the GPU command buffer to end a trace
Result FrameTraceController::SubmitEndTraceGpuWork()
{
    PAL_ASSERT((m_pCmdBufTraceEnd != nullptr) && (m_pTraceEndFence != nullptr));

    Result result = m_pCmdBufTraceEnd->End();

    if (result == Result::Success)
    {
        Device* pDevice = static_cast<Queue*>(m_pQueue)->GetDevice();
        result = pDevice->ResetFences(1, &m_pTraceEndFence);
    }

    if (result == Result::Success)
    {
        PerSubQueueSubmitInfo perSubQueueInfo = {};
        perSubQueueInfo.cmdBufferCount        = 1;
        perSubQueueInfo.ppCmdBuffers          = &m_pCmdBufTraceEnd;

        MultiSubmitInfo submitInfo            = {};
        submitInfo.perSubQueueInfoCount       = 1;
        submitInfo.pPerSubQueueInfo           = &perSubQueueInfo;
        submitInfo.fenceCount                 = 1;
        submitInfo.ppFences                   = &m_pTraceEndFence;

        result = m_pQueue->Submit(submitInfo);
    }

    return result;
}

// =====================================================================================================================
// Create a fence for FrameTraceController use.
Result FrameTraceController::CreateFence(
    Device*  pDevice,
    IFence** ppFence
    ) const
{
    Result result    = Result::ErrorOutOfMemory;
    size_t fenceSize = pDevice->GetFenceSize(&result);

    void* pFenceMemory = PAL_MALLOC(fenceSize, m_pPlatform, Util::AllocInternal);

    if (pFenceMemory != nullptr)
    {
        FenceCreateInfo createInfo = {};

        result = pDevice->CreateFence(createInfo, pFenceMemory, ppFence);

        if (result != Result::Success)
        {
            PAL_SAFE_FREE(pFenceMemory, m_pPlatform);
            *ppFence = nullptr;
        }
    }

    return result;
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 908
// =====================================================================================================================
Result FrameTraceController::OnPreparationGpuWork(
    uint32       gpuIndex,
    ICmdBuffer** ppCmdBuf)
{
    CmdBuffer* pCommandBuffer = nullptr;
    Device*    pDevice        = m_pPlatform->GetDevice(gpuIndex);

    Result result = CreateCommandBuffer(pDevice, false, &m_pCmdBufTracePrepare);

    if (result == Result::Success)
    {
        *ppCmdBuf = m_pCmdBufTracePrepare;
    }

    return result;
}
#endif

// =====================================================================================================================
Result FrameTraceController::OnTraceCanceled()
{
    Result result = Result::Success;

    if (m_pTraceSession->GetTraceSessionState() < TraceSessionState::Completed)
    {
        result = Result::NotReady;
    }
    else
    {
        result = m_pTraceSession->CleanupChunkStream();
        m_pTraceSession->SetTraceSessionState(TraceSessionState::Ready);
    }

    return result;
}

// =====================================================================================================================
Result FrameTraceController::OnBeginGpuWork(
    uint32       gpuIndex,
    ICmdBuffer** ppCmdBuf)
{
    Result result             = Result::Success;
    CmdBuffer* pCommandBuffer = nullptr;
    Device*    pDevice        = m_pPlatform->GetDevice(gpuIndex);

    result = CreateCommandBuffer(pDevice, false, &m_pCmdBufTraceBegin);

    if (result == Result::Success)
    {
        *ppCmdBuf = m_pCmdBufTraceBegin;
    }

    return result;
}

// =====================================================================================================================
Result FrameTraceController::OnEndGpuWork(
    uint32       gpuIndex,
    ICmdBuffer** ppCmdBuf)
{
    Result     result         = Result::Success;
    CmdBuffer* pCommandBuffer = nullptr;
    Device*    pDevice        = m_pPlatform->GetDevice(gpuIndex);

    result = CreateCommandBuffer(pDevice, true, &m_pCmdBufTraceEnd);

    if (result == Result::Success)
    {
        result = CreateFence(pDevice, &m_pTraceEndFence);
    }

    if (result == Result::Success)
    {
        *ppCmdBuf = m_pCmdBufTraceEnd;
    }

    return result;
}

// =====================================================================================================================
// Allocate a command buffer and prepare the command buffer for use
Result FrameTraceController::CreateCommandBuffer(
    Device*      pDevice,
    bool         traceEnd,
    ICmdBuffer** ppCmdBuf
    ) const
{
    Result result = Result::ErrorOutOfMemory;

    CmdBufferCreateInfo createInfo = {};
    createInfo.queueType           = m_pQueue->Type();
    createInfo.engineType          = m_pQueue->GetEngineType();
    createInfo.pCmdAllocator       = pDevice->InternalCmdAllocator(createInfo.engineType);

    // Calculate size required and allocate memory for command buffer
    const size_t cmdBufferSize = pDevice->GetCmdBufferSize(createInfo, nullptr);
    void*        pMemory       = PAL_MALLOC(cmdBufferSize, m_pPlatform, Util::AllocInternal);

    if (pMemory != nullptr)
    {
        result = pDevice->CreateCmdBuffer(createInfo, pMemory, ppCmdBuf);

        if (result != Result::Success)
        {
            PAL_SAFE_FREE(pMemory, m_pPlatform);
            *ppCmdBuf = nullptr;
        }
    }

    if (result == Result::Success)
    {
        CmdBufferBuildInfo buildInfo           = { };
        buildInfo.flags.optimizeOneTimeSubmit  = true;

        result = (*ppCmdBuf)->Begin(buildInfo);
    }

    if ((result == Result::Success) && (traceEnd == true))
    {
        (static_cast<CmdBuffer*>(*ppCmdBuf))->SetEndTraceFlag(1);
    }

    return result;
}

// =====================================================================================================================
void FrameTraceController::UpdateFrame(
    IQueue* pQueue)
{
    Util::MutexAuto lock(&m_framePresentLock);

    m_pQueue = pQueue;

    Util::AtomicIncrement64(&m_frameCount);
    OnFrameUpdated();

    m_pQueue = nullptr;
}

// =====================================================================================================================
void FrameTraceController::FinishTrace()
{
    m_pTraceSession->FinishTrace();
    m_pTraceSession->SetTraceSessionState(TraceSessionState::Completed);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 908
    if (m_pCmdBufTracePrepare != nullptr)
    {
        m_pCmdBufTracePrepare->Destroy();
        PAL_SAFE_FREE(m_pCmdBufTracePrepare, m_pPlatform);
    }
#endif

    if (m_pCmdBufTraceBegin != nullptr)
    {
        m_pCmdBufTraceBegin->Destroy();
        PAL_SAFE_FREE(m_pCmdBufTraceBegin, m_pPlatform);
    }

    if (m_pCmdBufTraceEnd != nullptr)
    {
        m_pCmdBufTraceEnd->Destroy();
        PAL_SAFE_FREE(m_pCmdBufTraceEnd, m_pPlatform);
    }

    if (m_pTraceEndFence != nullptr)
    {
        m_pTraceEndFence->Destroy();
        PAL_SAFE_FREE(m_pTraceEndFence, m_pPlatform);
    }
}

} // namespace GpuUtil

#endif
