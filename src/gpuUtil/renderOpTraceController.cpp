/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palRenderOpTraceController.h"
#include "util/ddStructuredReader.h"
#include "core/platform.h"
#include "core/device.h"
#include "core/cmdBuffer.h"
#include "core/engine.h"

using namespace Pal;
using namespace std::chrono_literals;
using namespace Util::Literals;
using DevDriver::StructuredValue;

namespace GpuUtil
{

// =====================================================================================================================
RenderOpTraceController::RenderOpTraceController(
    IPlatform* pPlatform,
    IDevice*   pDevice)
    :
    m_pPlatform(pPlatform),
    m_pDevice(pDevice),
    m_pTraceSession(pPlatform->GetTraceSession()),
    m_supportedGpuMask(1),
    m_renderOpMask(0),
    m_renderOpCount(0),
    m_numPrepRenderOps(0),
    m_captureRenderOpCount(1),
    m_renderOpTraceAccepted(0),
    m_renderOpLock(),
    m_pQueue(nullptr),
    m_pCmdAllocator(nullptr),
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 908
    m_pCmdBufTracePrepare(nullptr),
#endif
    m_pCmdBufTraceBegin(nullptr),
    m_pCmdBufTraceEnd(nullptr),
    m_pFenceTraceEnd(nullptr)
{
}

// =====================================================================================================================
RenderOpTraceController::~RenderOpTraceController()
{
    FreeResources();
}

// =====================================================================================================================
void RenderOpTraceController::OnConfigUpdated(
    StructuredValue* pJsonConfig)
{
    StructuredValue value;

    // Configure the render op mask
    if (pJsonConfig->GetValueByKey("renderOpMode", &value))
    {
        char buffer[32] = {'\0'};

        if (value.GetStringCopy(buffer))
        {
            if (strcmp(buffer, "draw") == 0)
            {
                m_renderOpMask = RenderOpDraw;
            }
            else if (strcmp(buffer, "dispatch") == 0)
            {
                m_renderOpMask = RenderOpDispatch;
            }
            else if (strcmp(buffer, "all") == 0)
            {
                m_renderOpMask = RenderOpDraw | RenderOpDispatch;
            }
        }
    }

    // Sets the capture mode as 'relative' or 'absolute'
    if (pJsonConfig->GetValueByKey("captureMode", &value))
    {
        char buffer[32] = {'\0'};

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

    // RenderOp number indicating when the trace should begin
    // This is relative to when the trace request was receieved if captureMode is 'relative',
    // or the absolute render op number if captureMode is 'absolute'
    if (pJsonConfig->GetValueByKey("preparationStartRenderOp", &value))
    {
        m_prepStartRenderOp = value.GetUint32Or(0);
    }

    // Configure the number of preparation operations (i.e. how many renderops)
    if (pJsonConfig->GetValueByKey("numPrepRenderOps", &value))
    {
        m_numPrepRenderOps = value.GetUint32Or(0);
    }

    // Configure the duration of the trace, as measured by render ops
    if (pJsonConfig->GetValueByKey("captureRenderOpCount", &value))
    {
        m_captureRenderOpCount = value.GetUint32Or(1);

        // We can't capture 0 render ops
        if (m_captureRenderOpCount < 1)
        {
            m_captureRenderOpCount = 1;
        }
    }
}

// =====================================================================================================================
Result RenderOpTraceController::OnTraceRequested()
{
    Result result = Result::Success;

    if ((m_captureMode == CaptureMode::Absolute) && (m_renderOpCount >= m_prepStartRenderOp))
    {
        result = Result::ErrorInitializationFailed;
    }

    return result;
}

// =====================================================================================================================
void RenderOpTraceController::OnRenderOpUpdated(
    uint64 countRecorded)
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
                if (m_prepStartRenderOp > 0)
                {
                    // Saturated subtraction to avoid underflow
                    m_prepStartRenderOp = (m_prepStartRenderOp > countRecorded) ? 0 : m_prepStartRenderOp - countRecorded;
                }
                else
                {
                    Result result = AcceptTrace();
                    if (result != Result::Success)
                    {
                        AbortTrace();
                    }
                }
                break;
            case CaptureMode::Absolute:
                if (m_renderOpCount >= m_prepStartRenderOp)
                {
                    Result result = AcceptTrace();

                    if (result != Result::Success)
                    {
                        AbortTrace();
                    }
                }
                break;
            }
        }
        else
        {
            // If trace is canceled, finish it as fast as possible ie. move requested->preparing immediately.
            if (m_pTraceSession->AcceptTrace(this, m_supportedGpuMask) == Result::Success)
            {
                m_renderOpTraceAccepted = m_renderOpCount + 1; // Begin the next frame
                m_pTraceSession->SetTraceSessionState(TraceSessionState::Preparing);
            }
        }
        break;
    }
    case TraceSessionState::Preparing:
    {
        // Move from Preparing -> Running if the number of prep render ops has elapsed
        if ((m_renderOpCount >= (m_renderOpTraceAccepted + m_numPrepRenderOps)) || m_pTraceSession->IsCancelingTrace())
        {
            Result result = BeginTrace();
            PAL_ASSERT(result == Result::Success);

            if (result != Result::Success)
            {
                AbortTrace();
            }
        }
        break;
    }
    case TraceSessionState::Running:
    {
        // Move from Running -> Waiting once the requested # of render ops has been processed
        if ((m_renderOpCount >= (m_renderOpTraceAccepted + m_captureRenderOpCount + m_numPrepRenderOps)) ||
            m_pTraceSession->IsCancelingTrace())
        {
            if (m_pTraceSession->EndTrace() == Result::Success)
            {
                m_pTraceSession->SetTraceSessionState(TraceSessionState::Waiting);

                Result result = SubmitEndTraceGpuWork();
                PAL_ASSERT(result == Result::Success);

                if (result == Result::Success)
                {
                    FinishTrace();
                }
                else
                {
                    AbortTrace();
                }
            }
            else
            {
                AbortTrace();
            }
        }
        break;
    }
    default:
        break;
    }
}

// =====================================================================================================================
Result RenderOpTraceController::AcceptTrace()
{
    Result result = m_pTraceSession->AcceptTrace(this, m_supportedGpuMask);

    if (result == Result::Success)
    {
        m_pTraceSession->SetTraceSessionState(TraceSessionState::Preparing);
        m_renderOpTraceAccepted = m_renderOpCount;

        if (m_numPrepRenderOps == 0)
        {
            result = BeginTrace();
        }
    }

    return result;
}

// =====================================================================================================================
Result RenderOpTraceController::BeginTrace()
{
    Result result = m_pTraceSession->BeginTrace();

    if (result == Result::Success)
    {
        result = SubmitBeginTraceGpuWork();

        if (result == Result::Success)
        {
            m_pTraceSession->SetTraceSessionState(TraceSessionState::Running);
        }
    }

    return result;
}

// =====================================================================================================================
Result RenderOpTraceController::SubmitBeginTraceGpuWork() const
{
    PAL_ASSERT(m_pCmdBufTraceBegin != nullptr);
    Result result = m_pCmdBufTraceBegin->End();

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 908
    PAL_ASSERT(m_pCmdBufTracePrepare != nullptr);

    if (result == Result::Success)
    {
        result = m_pCmdBufTraceBegin->End();
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
Result RenderOpTraceController::SubmitEndTraceGpuWork()
{
    PAL_ASSERT((m_pCmdBufTraceEnd != nullptr) && (m_pFenceTraceEnd != nullptr));

    Result result = m_pCmdBufTraceEnd->End();

    if (result == Result::Success)
    {
        PerSubQueueSubmitInfo perSubQueueInfo = {};
        perSubQueueInfo.cmdBufferCount        = 1;
        perSubQueueInfo.ppCmdBuffers          = &m_pCmdBufTraceEnd;

        MultiSubmitInfo submitInfo      = {};
        submitInfo.perSubQueueInfoCount = 1;
        submitInfo.pPerSubQueueInfo     = &perSubQueueInfo;
        submitInfo.ppFences             = &m_pFenceTraceEnd;
        submitInfo.fenceCount           = 1;

        result = m_pQueue->Submit(submitInfo);
    }

    return result;
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 908
// =====================================================================================================================
// Called during 'TraceSession::TraceAccepted'. Creates the command buffer that the preparation 'SubmitGpuWork'
// call will use.
Result RenderOpTraceController::OnPreparationGpuWork(
    uint32       gpuIndex,
    ICmdBuffer** ppCmdBuf)
{
    // Requiring the gpuIndex be zero -- interface changes are needed to the
    // Trace Controller state flow to ensure that a Device is managing the trace.
    // Currently, the Trace Session hardcodes the gpuIndex to 0, so this is safe.
    PAL_ASSERT(gpuIndex == 0);

    Result result = CreateCmdAllocator();

    if (result == Result::Success)
    {
        result = CreateCommandBuffer(false, &m_pCmdBufTracePrepare);
    }

    if (result == Result::Success)
    {
        (*ppCmdBuf) = m_pCmdBufTracePrepare;
    }

    return result;
}
#endif

// =====================================================================================================================
// Called during 'TraceSession::BeginTrace'. Creates the command buffer that the begin 'SubmitGpuWork' call will use.
Result RenderOpTraceController::OnBeginGpuWork(
    uint32       gpuIndex,
    ICmdBuffer** ppCmdBuf)
{
    // Requiring the gpuIndex be zero -- interface changes are needed to the
    // Trace Controller state flow to ensure that a Device is managing the trace.
    // Currently, the Trace Session hardcodes the gpuIndex to 0, so this is safe.
    PAL_ASSERT(gpuIndex == 0);

    Result result = Result::Success;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 908
    result = CreateCmdAllocator();
#endif

    if (result == Result::Success)
    {
        result = CreateCommandBuffer(false, &m_pCmdBufTraceBegin);
    }

    if (result == Result::Success)
    {
        (*ppCmdBuf) = m_pCmdBufTraceBegin;
    }

    return result;
}

// =====================================================================================================================
// Called during 'TraceSession::EndTrace'. Creates the command buffer and fence that the ending
// 'SubmitGpuWork' call will use.
Result RenderOpTraceController::OnEndGpuWork(
    uint32       gpuIndex,
    ICmdBuffer** ppCmdBuf)
{
    PAL_ASSERT(gpuIndex == 0);

    Result result = CreateCommandBuffer(true, &m_pCmdBufTraceEnd);

    if (result == Result::Success)
    {
        result = CreateFence(&m_pFenceTraceEnd);
    }

    if (result == Result::Success)
    {
        (*ppCmdBuf) = m_pCmdBufTraceEnd;
    }

    return result;
}

// =====================================================================================================================
// Create a fence for RenderOpTraceController use.
Result RenderOpTraceController::CreateFence(
    IFence** ppFence
    ) const
{
    Result result    = Result::Success;
    size_t fenceSize = m_pDevice->GetFenceSize(&result);

    void* pFenceMemory = PAL_MALLOC(fenceSize, m_pPlatform, Util::AllocInternal);

    if (pFenceMemory != nullptr)
    {
        FenceCreateInfo createInfo = {};
        result = m_pDevice->CreateFence(createInfo, pFenceMemory, ppFence);

        if (result != Result::Success)
        {
            PAL_SAFE_FREE(pFenceMemory, m_pPlatform);
            (*ppFence) = nullptr;
        }
    }
    else
    {
        result = Result::ErrorOutOfMemory;
    }

    return result;
}

// =====================================================================================================================
Result RenderOpTraceController::CreateCmdAllocator()
{
    Result result = Result::Success;

    if (m_pCmdAllocator == nullptr)
    {
        // Create internal cmd allocator for this gpaSession object
        CmdAllocatorCreateInfo createInfo = { };
        createInfo.flags.threadSafe       = 1;

        // Reasonable constants for allocation and suballocation sizes
        constexpr size_t CmdAllocSize    = 2_MiB;
        constexpr size_t CmdSubAllocSize = 64_KiB;

        createInfo.allocInfo[CommandDataAlloc].allocHeap          = GpuHeapGartUswc;
        createInfo.allocInfo[CommandDataAlloc].allocSize          = CmdAllocSize;
        createInfo.allocInfo[CommandDataAlloc].suballocSize       = CmdSubAllocSize;
        createInfo.allocInfo[EmbeddedDataAlloc].allocHeap         = GpuHeapGartUswc;
        createInfo.allocInfo[EmbeddedDataAlloc].allocSize         = CmdAllocSize;
        createInfo.allocInfo[EmbeddedDataAlloc].suballocSize      = CmdSubAllocSize;
        createInfo.allocInfo[LargeEmbeddedDataAlloc].allocHeap    = GpuHeapGartUswc;
        createInfo.allocInfo[LargeEmbeddedDataAlloc].allocSize    = CmdAllocSize;
        createInfo.allocInfo[LargeEmbeddedDataAlloc].suballocSize = CmdSubAllocSize;
        createInfo.allocInfo[GpuScratchMemAlloc].allocHeap        = GpuHeapInvisible;
        createInfo.allocInfo[GpuScratchMemAlloc].allocSize        = CmdAllocSize;
        createInfo.allocInfo[GpuScratchMemAlloc].suballocSize     = CmdSubAllocSize;

        const size_t cmdAllocatorSize = m_pDevice->GetCmdAllocatorSize(createInfo, &result);
        if (result == Result::Success)
        {
            void* pMemory = PAL_MALLOC(cmdAllocatorSize, m_pPlatform, Util::AllocObject);
            if (pMemory == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                result = m_pDevice->CreateCmdAllocator(createInfo, pMemory, &m_pCmdAllocator);

                if (result != Result::Success)
                {
                    PAL_SAFE_FREE(pMemory, m_pPlatform);
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Allocate a command buffer and prepare the command buffer for use
Result RenderOpTraceController::CreateCommandBuffer(
    bool         traceEnd,
    ICmdBuffer** ppCmdBuf
    ) const
{
    PAL_ASSERT(m_pCmdAllocator != nullptr);

    Result result = Result::Success;

    CmdBufferCreateInfo createInfo = {};
    createInfo.queueType           = m_pQueue->Type();
    createInfo.engineType          = m_pQueue->GetEngineType();
    createInfo.pCmdAllocator       = m_pCmdAllocator;

    // Calculate size required and allocate memory for command buffer
    const size_t cmdBufferSize = m_pDevice->GetCmdBufferSize(createInfo, &result);

    if (result == Pal::Result::Success)
    {
        void* pMemory = PAL_MALLOC(cmdBufferSize, m_pPlatform, Util::SystemAllocType::AllocInternal);

        if (pMemory != nullptr)
        {
            result = m_pDevice->CreateCmdBuffer(createInfo, pMemory, ppCmdBuf);

            if (result != Result::Success)
            {
                PAL_SAFE_FREE(pMemory, m_pPlatform);
                (*ppCmdBuf) = nullptr;
            }
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    if (result == Result::Success)
    {
        CmdBufferBuildInfo buildInfo          = { };
        buildInfo.flags.optimizeOneTimeSubmit = true;

        result = (*ppCmdBuf)->Begin(buildInfo);
    }

    return result;
}

#if PAL_CLIENT_MAJOR_INTERFACE_VERSION < 896
// =====================================================================================================================
void RenderOpTraceController::RecordRenderOp(
    Pal::IQueue* pQueue,
    RenderOp     renderOp)
{
    RenderOpCounts opCounts =
    {
        .drawCount     = (renderOp == RenderOpDraw)     ? 1u : 0u,
        .dispatchCount = (renderOp == RenderOpDispatch) ? 1u : 0u,
    };
    RecordRenderOps(pQueue, opCounts);
}
#endif

// =====================================================================================================================
void RenderOpTraceController::RecordRenderOps(
    Pal::IQueue*          pQueue,
    const RenderOpCounts& renderOpCounts)
{
    Util::MutexAuto lock(&m_renderOpLock);

    const uint64 previousCount = m_renderOpCount;

    if (RenderOpDraw & m_renderOpMask)
    {
        m_renderOpCount += renderOpCounts.drawCount;
    }

    if (RenderOpDispatch & m_renderOpMask)
    {
        m_renderOpCount += renderOpCounts.dispatchCount;
    }

    m_pQueue = pQueue;
    OnRenderOpUpdated(m_renderOpCount - previousCount);
    m_pQueue = nullptr;
}

// =====================================================================================================================
void RenderOpTraceController::FinishTrace()
{
    Result result = WaitForTraceEndGpuWorkCompletion();
    PAL_ASSERT(result == Result::Success);

    if (result == Result::Success)
    {
        m_pTraceSession->FinishTrace();
        m_pTraceSession->SetTraceSessionState(TraceSessionState::Completed);

        FreeResources();
    }
}

// =====================================================================================================================
// Wait for the fence associated with the GPU command which ends the trace
Result RenderOpTraceController::WaitForTraceEndGpuWorkCompletion() const
{
    Result result = Result::ErrorInvalidPointer;

    if ((m_pQueue != nullptr) && (m_pFenceTraceEnd != nullptr))
    {
        result = m_pDevice->WaitForFences(1, &m_pFenceTraceEnd, true, 5s);
        PAL_ASSERT(m_pFenceTraceEnd->GetStatus() == Result::Success);

        if (result == Result::Success)
        {
            result = m_pDevice->ResetFences(1, &m_pFenceTraceEnd);
            PAL_ASSERT(result == Result::Success);
        }
    }

    return result;
}

// =====================================================================================================================
void RenderOpTraceController::FreeResources()
{
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

    if (m_pFenceTraceEnd != nullptr)
    {
        m_pFenceTraceEnd->Destroy();
        PAL_SAFE_FREE(m_pFenceTraceEnd, m_pPlatform);
    }

    if (m_pCmdAllocator != nullptr)
    {
        m_pCmdAllocator->Destroy();
        PAL_SAFE_FREE(m_pCmdAllocator, m_pPlatform);
    }
}

// =====================================================================================================================
Result RenderOpTraceController::OnTraceCanceled()
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
void RenderOpTraceController::AbortTrace()
{
    m_pTraceSession->FinishTrace();
    m_pTraceSession->SetTraceSessionState(TraceSessionState::Completed);

    FreeResources();
}

} // namespace GpuUtil

#endif
