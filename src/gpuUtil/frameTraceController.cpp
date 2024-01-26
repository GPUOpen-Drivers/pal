/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 844
constexpr Pal::uint64 FrameTraceControllerFenceTimeoutNs = 5000000000;
#endif

namespace GpuUtil
{

// =====================================================================================================================
FrameTraceController::FrameTraceController(
    Platform* pPlatform)
    :
    m_pPlatform(pPlatform),
    m_supportedGpuMask(1),
    m_frameCount(0),
    m_numPrepFrames(0),
    m_captureStartIndex(0),
    m_currentTraceStartIndex(0),
    m_captureFrameCount(1),
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 844
    m_pCmdBufTraceBegin(nullptr),
    m_pCmdBufTraceEnd(nullptr),
    m_pTraceEndFence(nullptr),
#else
    m_pCurrentCmdBuffer(nullptr),
#endif
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 844
                Result result = SubmitBeginTraceGpuWork();
                PAL_ASSERT(result == Result::Success);
                if (result == Result::Success)
#endif
                {
                    m_pTraceSession->SetTraceSessionState(TraceSessionState::Running);
                }
            }
        }
    }
    else if (traceSessionState == TraceSessionState::Running)
    {
        if (m_frameCount == m_currentTraceStartIndex + m_captureFrameCount + m_numPrepFrames)
        {
            if (m_pTraceSession->EndTrace() == Result::Success)
            {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 844
                // update the Trace Session State before submitting the GPU work
                // because the PAL submission code itself will call back into
                // FrameTraceController::FinishTrace() and set the Trace Session State to Completed.
                // The expected flow is therefore that we set the state to Waiting now, then
                // the PAL submission code path will call into FrameTraceController::FinishTrace()
                // and the state will be set to Completed
#endif
                m_pTraceSession->SetTraceSessionState(TraceSessionState::Waiting);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 844
                Result result = SubmitEndTraceGpuWork();
                PAL_ASSERT(result == Result::Success);
#endif
            }
        }
    }
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 844
// =====================================================================================================================
// Submit the GPU command buffer to begin a trace
Result FrameTraceController::SubmitBeginTraceGpuWork() const
{
    return SubmitGpuWork(m_pCmdBufTraceBegin, nullptr);
}

// =====================================================================================================================
// Submit the GPU command buffer to end a trace
Result FrameTraceController::SubmitEndTraceGpuWork() const
{
    return SubmitGpuWork(m_pCmdBufTraceEnd, m_pTraceEndFence);
}

// =====================================================================================================================
// Submit the GPU command buffer to begin a trace
Result FrameTraceController::SubmitGpuWork(
    ICmdBuffer* pCmdBuf,
    IFence*     pFence
    ) const
{
    Result result = Result::Success;

    // The command buffer must always be valid
    PAL_ASSERT(pCmdBuf != nullptr);

    PerSubQueueSubmitInfo perSubQueueInfo = {};
    perSubQueueInfo.cmdBufferCount        = 1;
    perSubQueueInfo.ppCmdBuffers          = &pCmdBuf;
    MultiSubmitInfo submitInfo            = {};
    submitInfo.perSubQueueInfoCount       = 1;
    submitInfo.pPerSubQueueInfo           = &perSubQueueInfo;

    if (pFence != nullptr)
    {
        submitInfo.ppFences   = &pFence;
        submitInfo.fenceCount = 1;
    }

    result = pCmdBuf->End();

    if (result == Result::Success)
    {
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
    Result result      = Result::Success;
    size_t fenceSize   = pDevice->GetFenceSize(&result);

    void* pFenceMemory = PAL_MALLOC(fenceSize,
                                    m_pPlatform,
                                    Util::AllocInternal);

    if (pFenceMemory != nullptr)
    {
        FenceCreateInfo createInfo = {};

        result = pDevice->CreateFence(createInfo,
                                      pFenceMemory,
                                      ppFence);
        if (result != Result::Success)
        {
            PAL_SAFE_FREE(pFenceMemory, m_pPlatform);
            *ppFence = nullptr;
        }
    }
    else
    {
        result = Result::ErrorOutOfMemory;
    }

    return result;
}
#endif

// =====================================================================================================================
Result FrameTraceController::OnBeginGpuWork(
    uint32       gpuIndex,
    ICmdBuffer** ppCmdBuf)
{
    Result result             = Result::Success;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 844
    CmdBuffer* pCommandBuffer = nullptr;
    Device*    pDevice        = m_pPlatform->GetDevice(gpuIndex);

    result = CreateCommandBuffer(pDevice,
                                 false,
                                 &m_pCmdBufTraceBegin);

    if (result == Result::Success)
    {
        *ppCmdBuf = m_pCmdBufTraceBegin;
    }
#else
    if (m_pCurrentCmdBuffer != nullptr)
    {
        *ppCmdBuf = m_pCurrentCmdBuffer;
    }
    else
    {
        result = Result::ErrorUnknown;
    }
#endif
    return result;
}

// =====================================================================================================================
Result FrameTraceController::OnEndGpuWork(
    uint32       gpuIndex,
    ICmdBuffer** ppCmdBuf)
{
    Result     result         = Result::Success;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 844
    CmdBuffer* pCommandBuffer = nullptr;
    Device*    pDevice        = m_pPlatform->GetDevice(gpuIndex);

    result = CreateCommandBuffer(pDevice,
                                 true,
                                 &m_pCmdBufTraceEnd);

    if (result == Result::Success)
    {
        result = CreateFence(pDevice, &m_pTraceEndFence);
    }

    if (result == Result::Success)
    {
        *ppCmdBuf = m_pCmdBufTraceEnd;
    }
#else
    if (m_pCurrentCmdBuffer != nullptr)
    {
        m_pCurrentCmdBuffer->SetEndTraceFlag(1);
        *ppCmdBuf = m_pCurrentCmdBuffer;
    }
    else
    {
        result = Result::ErrorUnknown;
    }
#endif

    return result;
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 844
// =====================================================================================================================
// Allocate a command buffer and prepare the command buffer for use
Result FrameTraceController::CreateCommandBuffer(
    Device*      pDevice,
    bool         traceEnd,
    ICmdBuffer** ppCmdBuf
    ) const
{
    Result result = Result::Success;

    CmdBufferCreateInfo createInfo = {};
    createInfo.queueType           = m_pQueue->Type();
    createInfo.engineType          = m_pQueue->GetEngineType();
    createInfo.pCmdAllocator       = pDevice->InternalCmdAllocator(createInfo.engineType);

    // Calculate size required and allocate memory for command buffer
    const size_t cmdBufferSize = pDevice->GetCmdBufferSize(createInfo, nullptr);
    void*        pMemory       = PAL_MALLOC(cmdBufferSize,
                                            m_pPlatform,
                                            Util::SystemAllocType::AllocInternal);
    if (pMemory != nullptr)
    {
        result = pDevice->CreateCmdBuffer(createInfo,
                                          pMemory,
                                          ppCmdBuf);

        if (result != Result::Success)
        {
            PAL_SAFE_FREE(pMemory, m_pPlatform);
            *ppCmdBuf = nullptr;
        }
    }
    else
    {
        result = Result::ErrorOutOfMemory;
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
#endif

// =====================================================================================================================
void FrameTraceController::UpdateFrame(
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 844
    IQueue* pQueue)
#else
    CmdBuffer* pCmdBuffer)
#endif
{
    Util::MutexAuto lock(&m_framePresentLock);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 844
    m_pQueue = pQueue;
#else
    m_pCurrentCmdBuffer = pCmdBuffer;
#endif
    Util::AtomicIncrement(&m_frameCount);
    OnFrameUpdated();

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 844
    m_pQueue = nullptr;
#else
    m_pCurrentCmdBuffer = nullptr;
#endif
}

// =====================================================================================================================
void FrameTraceController::FinishTrace()
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 844
    Result result = Result::Success;

    result = WaitForTraceEndGpuWorkCompletion();
    PAL_ASSERT(result == Result::Success);

    if (result == Result::Success)
#endif
    {
        m_pTraceSession->FinishTrace();
        m_pTraceSession->SetTraceSessionState(TraceSessionState::Completed);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 844
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
#endif
    }
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 844
// =====================================================================================================================
// Wait for the fence associated with the GPU command which ends the trace
Result FrameTraceController::WaitForTraceEndGpuWorkCompletion() const
{
    Result result = Result::ErrorInvalidPointer;

    if ((m_pQueue != nullptr) && (m_pTraceEndFence != nullptr))
    {
        Device* pDevice = (static_cast<Queue*>(m_pQueue))->GetDevice();

        result = pDevice->WaitForFences(1,
                                        &m_pTraceEndFence,
                                        true,
                                        FrameTraceControllerFenceTimeoutNs);

        if (result == Result::Success)
        {
            result = pDevice->ResetFences(1, &m_pTraceEndFence);
        }
    }

    return result;
}
#endif
}

#endif
