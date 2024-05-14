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

#include "palQueueTimingsTraceSource.h"
#include "palAutoBuffer.h"
#include "core/platform.h"
#include <sqtt_file_format.h>

using namespace Pal;
using namespace Util;

namespace GpuUtil
{

constexpr uint32 DefaultDeviceIndex = 0;

// =====================================================================================================================
static TraceChunk::HwEngineType ConvertSqttEngineType(
    SqttEngineType sqttEngineType)
{
    TraceChunk::HwEngineType engineType;

    switch (sqttEngineType)
    {
    case SqttEngineType::SQTT_ENGINE_TYPE_UNIVERSAL:
        engineType = TraceChunk::HwEngineType::Universal;
        break;
    case SqttEngineType::SQTT_ENGINE_TYPE_COMPUTE:
        engineType = TraceChunk::HwEngineType::Compute;
        break;
    case SqttEngineType::SQTT_ENGINE_TYPE_EXCLUSIVE_COMPUTE:
        engineType = TraceChunk::HwEngineType::ExclusiveCompute;
        break;
    case SqttEngineType::SQTT_ENGINE_TYPE_DMA:
        engineType = TraceChunk::HwEngineType::Dma;
        break;
    case SqttEngineType::SQTT_ENGINE_TYPE_HIGH_PRIORITY_UNIVERSAL:
        engineType = TraceChunk::HwEngineType::HighPriorityUniversal;
        break;
    case SqttEngineType::SQTT_ENGINE_TYPE_HIGH_PRIORITY_GRAPHICS:
        engineType = TraceChunk::HwEngineType::HighPriorityGraphics;
        break;
    case SqttEngineType::SQTT_ENGINE_TYPE_UNKNOWN:
    default:
        engineType = TraceChunk::HwEngineType::Unknown;
        break;
    }

    return engineType;
}

// =====================================================================================================================
static TraceChunk::QueueType ConvertSqttQueueType(
    SqttQueueType sqttQueueType)
{
    TraceChunk::QueueType queueType;

    switch (sqttQueueType)
    {
    case SqttQueueType::SQTT_QUEUE_TYPE_UNIVERSAL:
        queueType = TraceChunk::QueueType::Universal;
        break;
    case SqttQueueType::SQTT_QUEUE_TYPE_COMPUTE:
        queueType = TraceChunk::QueueType::Compute;
        break;
    case SqttQueueType::SQTT_QUEUE_TYPE_DMA:
        queueType = TraceChunk::QueueType::Dma;
        break;
    case SqttQueueType::SQTT_QUEUE_TYPE_UNKNOWN:
    default:
        queueType = TraceChunk::QueueType::Unknown;
        break;
    }

    return queueType;
}

// =====================================================================================================================
static TraceChunk::QueueEventType ConvertSqttQueueEventType(
    SqttQueueEventType sqttQueueEventType)
{
    TraceChunk::QueueEventType eventType;

    switch (sqttQueueEventType)
    {
    case SqttQueueEventType::SQTT_QUEUE_TIMING_EVENT_CMDBUF_SUBMIT:
        eventType = TraceChunk::QueueEventType::CmdBufSubmit;
        break;
    case SqttQueueEventType::SQTT_QUEUE_TIMING_EVENT_SIGNAL_SEMAPHORE:
        eventType = TraceChunk::QueueEventType::SignalSemaphore;
        break;
    case SqttQueueEventType::SQTT_QUEUE_TIMING_EVENT_WAIT_SEMAPHORE:
        eventType = TraceChunk::QueueEventType::WaitSemaphore;
        break;
    case SqttQueueEventType::SQTT_QUEUE_TIMING_EVENT_PRESENT:
        eventType = TraceChunk::QueueEventType::Present;
        break;
    }

    return eventType;
}

// =====================================================================================================================
QueueTimingsTraceSource::QueueTimingsTraceSource(
    IPlatform* pPlatform)
    :
    m_pPlatform(pPlatform),
    m_pGpaSession(nullptr),
    m_traceIsHealthy(false),
    m_timingInProgress(false)
{
}

// =====================================================================================================================
QueueTimingsTraceSource::~QueueTimingsTraceSource()
{
    if (m_pGpaSession != nullptr)
    {
        PAL_SAFE_FREE(m_pGpaSession, m_pPlatform);
    }

}

// =====================================================================================================================
// Returns 'true' if the driver is currently timing queue operations.
bool QueueTimingsTraceSource::IsTimingInProgress() const
{
    return m_timingInProgress;
}

// =====================================================================================================================
// Registers a queue with the gpa session that will be used in future timing operations.
Result QueueTimingsTraceSource::RegisterTimedQueue(
    IQueue* pQueue,
    uint64  queueId,
    uint64  queueContext)
{
    return (m_pGpaSession != nullptr) ? m_pGpaSession->RegisterTimedQueue(pQueue, queueId, queueContext)
                                      : Result::ErrorUnavailable;
}

// =====================================================================================================================
// Unregisters a queue prior to object destruction, and ensure that associated resources are destroyed.
Result QueueTimingsTraceSource::UnregisterTimedQueue(
    IQueue* pQueue)
{
    return (m_pGpaSession != nullptr) ? m_pGpaSession->UnregisterTimedQueue(pQueue)
                                      : Result::ErrorUnavailable;
}

// =====================================================================================================================
// Injects timing commands into a submission and submits it to pQueue.
Result QueueTimingsTraceSource::TimedSubmit(
    IQueue*                pQueue,
    const MultiSubmitInfo& submitInfo,
    const TimedSubmitInfo& timedSubmitInfo)
{
    return (m_pGpaSession != nullptr) ? m_pGpaSession->TimedSubmit(pQueue, submitInfo, timedSubmitInfo)
                                      : Result::ErrorUnavailable;
}

// =====================================================================================================================
// Injects timing commands into a queue signal operation.
Result QueueTimingsTraceSource::TimedSignalQueueSemaphore(
    IQueue*                        pQueue,
    IQueueSemaphore*               pQueueSemaphore,
    const TimedQueueSemaphoreInfo& timedSignalInfo,
    uint64                         value)
{
    return (m_pGpaSession != nullptr) ? m_pGpaSession->TimedSignalQueueSemaphore(pQueue,
                                                                                 pQueueSemaphore,
                                                                                 timedSignalInfo,
                                                                                 value)
                                      : Result::ErrorUnavailable;
}

// =====================================================================================================================
// Injects timing commands into a queue wait operation.
Result QueueTimingsTraceSource::TimedWaitQueueSemaphore(
    IQueue*                        pQueue,
    IQueueSemaphore*               pQueueSemaphore,
    const TimedQueueSemaphoreInfo& timedWaitInfo,
    uint64                         value)
{
    return (m_pGpaSession != nullptr) ? m_pGpaSession->TimedWaitQueueSemaphore(pQueue,
                                                                               pQueueSemaphore,
                                                                               timedWaitInfo,
                                                                               value)
                                      : Result::ErrorUnavailable;
}

// =====================================================================================================================
// Injects an external event for a queue wait operation.
Result QueueTimingsTraceSource::ExternalTimedWaitQueueSemaphore(
    uint64                         queueContext,
    uint64                         cpuSubmissionTimestamp,
    uint64                         cpuCompletionTimestamp,
    const TimedQueueSemaphoreInfo& timedWaitInfo)
{
    return (m_pGpaSession != nullptr) ? m_pGpaSession->ExternalTimedWaitQueueSemaphore(queueContext,
                                                                                       cpuSubmissionTimestamp,
                                                                                       cpuCompletionTimestamp,
                                                                                       timedWaitInfo)
                                      : Result::ErrorUnavailable;
}

// =====================================================================================================================
// Injects an external event for a queue signal operation.
Result QueueTimingsTraceSource::ExternalTimedSignalQueueSemaphore(
    uint64                         queueContext,
    uint64                         cpuSubmissionTimestamp,
    uint64                         cpuCompletionTimestamp,
    const TimedQueueSemaphoreInfo& timedSignalInfo)
{
    return (m_pGpaSession != nullptr) ? m_pGpaSession->ExternalTimedSignalQueueSemaphore(queueContext,
                                                                                         cpuSubmissionTimestamp,
                                                                                         cpuCompletionTimestamp,
                                                                                         timedSignalInfo)
                                      : Result::ErrorUnavailable;
}

// =====================================================================================================================
Result QueueTimingsTraceSource::TimedQueuePresent(
    IQueue*                      pQueue,
    const TimedQueuePresentInfo& timedPresentInfo)
{
    return (m_pGpaSession != nullptr) ? m_pGpaSession->TimedQueuePresent(pQueue, timedPresentInfo)
                                      : Result::ErrorUnavailable;
}

// =====================================================================================================================
void QueueTimingsTraceSource::OnConfigUpdated(
    DevDriver::StructuredValue* pJsonConfig)
{
}

// =====================================================================================================================
Result QueueTimingsTraceSource::Init(
    IDevice* pDevice)
{
    Result result = Result::Success;

    m_pGpaSession = PAL_NEW(GpaSession, m_pPlatform, SystemAllocType::AllocInternal)
                           (m_pPlatform, pDevice, 0, 0, ApiType::Generic, 0, 0);

    if (m_pGpaSession != nullptr)
    {
        result = m_pGpaSession->Init();

        if (result == Result::Success)
        {
            m_traceIsHealthy = true;
        }
        else
        {
            ReportInternalError("Error encountered when initializing the GpaSession", result);
            PAL_SAFE_FREE(m_pGpaSession, m_pPlatform);
        }
    }
    else
    {
        result = Result::ErrorOutOfMemory;
        ReportInternalError("System is out of memory: cannot allocate trace resources", result);
    }

    return result;
}

// =====================================================================================================================
void QueueTimingsTraceSource::OnTraceAccepted()
{
    if (m_traceIsHealthy)
    {
        m_timingInProgress = true;
    }
    else
    {
        // This is called each time a user starts a new trace, so log an error message if we cannot proceed
        ReportInternalError("Error starting trace", Result::ErrorUnavailable);
    }
}

// =====================================================================================================================
void QueueTimingsTraceSource::OnTraceBegin(
    uint32      gpuIndex,
    ICmdBuffer* pCmdBuf)
{
    if (m_traceIsHealthy)
    {
        GpaSessionBeginInfo beginInfo = { };
        beginInfo.flags.enableQueueTiming = 1;

        Result result = m_pGpaSession->Begin(beginInfo);

        if (result != Result::Success)
        {
            ReportInternalError("Error encountered when beginning a GpaSession", result);
        }

    }
}

// =====================================================================================================================
void QueueTimingsTraceSource::OnTraceEnd(
    uint32      gpuIndex,
    ICmdBuffer* pCmdBuf)
{
    m_timingInProgress = false;

    if (m_traceIsHealthy)
    {

        Result result = m_pGpaSession->End(pCmdBuf);

        if (result != Result::Success)
        {
            ReportInternalError("Error encountered when ending the GpaSession", result);
        }
    }
}

// =====================================================================================================================
void QueueTimingsTraceSource::OnTraceFinished()
{
    if (m_traceIsHealthy)
    {
        if (m_pGpaSession->IsReady())
        {
            QueueTimingsTraceInfo traceInfo = {};
            void*  pData    = nullptr;
            size_t dataSize = 0;

            Result result = m_pGpaSession->GetQueueTimingsData(&traceInfo, &dataSize, nullptr);

            if (result == Result::Success)
            {
                pData = PAL_MALLOC(dataSize, m_pPlatform, SystemAllocType::AllocInternalTemp);

                if (pData != nullptr)
                {
                    result = m_pGpaSession->GetQueueTimingsData(nullptr, &dataSize, pData);
                }
                else
                {
                    result = Result::ErrorOutOfMemory;
                }
            }

            if (result != Result::Success)
            {
                ReportInternalError("Error encountered when reading Queue Timings data", result);
            }

            if (result == Result::Success)
            {
                const SqttQueueInfoRecord*  pQueueInfoRecords  = static_cast<SqttQueueInfoRecord*>(pData);
                const SqttQueueEventRecord* pQueueEventRecords = static_cast<SqttQueueEventRecord*>(
                    VoidPtrInc(pData, sizeof(SqttQueueInfoRecord) * traceInfo.numQueueInfoRecords));

                WriteQueueInfoChunks(pQueueInfoRecords,
                                     traceInfo.numQueueInfoRecords);

                WriteQueueEventChunks(pQueueInfoRecords,
                                      traceInfo.numQueueInfoRecords,
                                      pQueueEventRecords,
                                      traceInfo.numQueueEventRecords);
            }

            PAL_SAFE_FREE(pData, m_pPlatform);
        }
        else
        {
            ReportInternalError("GPA Session is not ready. Could not write chunks.", Result::NotReady);
        }
    }

    // Reset the GpaSession in preparation for the next trace
    if (m_pGpaSession != nullptr)
    {
        Result result = m_pGpaSession->Reset();

        m_traceIsHealthy = (result == Result::Success);
    }
}

// =====================================================================================================================
void QueueTimingsTraceSource::WriteQueueInfoChunks(
    const SqttQueueInfoRecord* pQueueInfoRecords,
    size_t                     numQueueInfoRecords)
{
    Result result = Result::Success;

    for (int i = 0; (i < numQueueInfoRecords) && (result == Result::Success); i++)
    {
        const SqttQueueInfoRecord& record = pQueueInfoRecords[i];

        TraceChunk::QueueInfo queueInfo = {
            .pciId        = m_pPlatform->GetPciId(DefaultDeviceIndex).u32All,
            .queueId      = record.queueID,
            .queueContext = record.queueContext,
            .queueType    = ConvertSqttQueueType(record.hardwareInfo.queueType),
            .engineType   = ConvertSqttEngineType(record.hardwareInfo.engineType)
        };

        TraceChunkInfo info    = {
            .version           = TraceChunk::QueueInfoChunkVersion,
            .pHeader           = nullptr,
            .headerSize        = 0,
            .pData             = &queueInfo,
            .dataSize          = sizeof(TraceChunk::QueueInfo),
            .enableCompression = false
        };
        memcpy(info.id, TraceChunk::QueueInfoChunkId, TextIdentifierSize);

        result = m_pPlatform->GetTraceSession()->WriteDataChunk(this, info);
    }

    PAL_ASSERT(result == Result::Success);
}

// =====================================================================================================================
void QueueTimingsTraceSource::WriteQueueEventChunks(
    const SqttQueueInfoRecord*  pQueueInfoRecords,
    size_t                      numQueueInfoRecords,
    const SqttQueueEventRecord* pQueueEventRecords,
    size_t                      numQueueEventRecords)
{
    Result result = Result::Success;

    for (int i = 0; (i < numQueueEventRecords) && (result == Result::Success); i++)
    {
        const SqttQueueEventRecord& eventRecord = pQueueEventRecords[i];

        if (eventRecord.queueInfoIndex > numQueueInfoRecords)
        {
            result = Result::ErrorInvalidValue;
            ReportInternalError("Invalid value for QueueInfo index", result);
            break;
        }

        const SqttQueueInfoRecord& infoRecord = pQueueInfoRecords[eventRecord.queueInfoIndex];

        TraceChunk::QueueEvent queueEvent = {
            .pciId          = m_pPlatform->GetPciId(DefaultDeviceIndex).u32All,
            .queueId        = infoRecord.queueID,
            .eventType      = ConvertSqttQueueEventType(static_cast<SqttQueueEventType>(eventRecord.eventType)),
            .sqttCmdBufId   = eventRecord.sqttCbId,
            .frameIndex     = eventRecord.frameIndex,
            .submitSubIndex = eventRecord.submitSubIndex,
            .apiEventId     = eventRecord.apiId,
            .cpuTimestamp   = eventRecord.cpuTimestamp,
            .gpuTimestamp1  = eventRecord.gpuTimestamps[0],
            .gpuTimestamp2  = eventRecord.gpuTimestamps[1]
        };

        TraceChunkInfo info = {
            .version           = TraceChunk::QueueEventChunkVersion,
            .pHeader           = nullptr,
            .headerSize        = 0,
            .pData             = &queueEvent,
            .dataSize          = sizeof(TraceChunk::QueueEvent),
            .enableCompression = false
        };
        memcpy(info.id, TraceChunk::QueueEventChunkId, TextIdentifierSize);

        result = m_pPlatform->GetTraceSession()->WriteDataChunk(this, info);
    }

    PAL_ASSERT(result == Result::Success);
}

// =====================================================================================================================
void QueueTimingsTraceSource::ReportInternalError(
    const char* pErrorMsg,
    Result      result)
{
    // Mark that an internal error was encountered and the trace cannot proceed
    m_traceIsHealthy = false;

    // Emit the error message as an RDF chunk
    Result errResult = m_pPlatform->GetTraceSession()->ReportError(TraceChunk::QueueInfoChunkId,
                                                                   pErrorMsg,
                                                                   strlen(pErrorMsg),
                                                                   TraceErrorPayload::ErrorString,
                                                                   result);

    PAL_ASSERT(errResult == Result::Success);
}

} // namespace GpuUtil

#endif
