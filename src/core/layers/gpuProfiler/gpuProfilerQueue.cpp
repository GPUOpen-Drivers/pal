/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/gpuProfiler/gpuProfilerCmdBuffer.h"
#include "core/layers/gpuProfiler/gpuProfilerPlatform.h"
#include "core/layers/gpuProfiler/gpuProfilerQueue.h"
#include "palLiterals.h"

using namespace Util;
using namespace Util::Literals;

namespace Pal
{
namespace GpuProfiler
{
// Find Queue::Queue(...), Queue::~Queue, and other state management in gpuProfilerQueueInternal.cpp
// All Queue methods in this file should take the lock and MUST NOT be called from other methods in this file.

// =====================================================================================================================
// IQueue::Submit wrapper on ProcessSubmit
Result Queue::Submit(
        const MultiSubmitInfo& submitInfo)
{
    MutexAuto lock(&m_stateMutex);

    return ProcessSubmit(submitInfo);
}

// =====================================================================================================================
// Log the WaitIdle call and pass it to the next layer.
Result Queue::WaitIdle()
{
    MutexAuto lock(&m_stateMutex);
    LogQueueCall(QueueCallId::WaitIdle);

    return QueueDecorator::WaitIdle();
}

// =====================================================================================================================
// Log the SignalQueueSemaphore call and pass it to the next layer.
Result Queue::SignalQueueSemaphore(
    IQueueSemaphore* pQueueSemaphore,
    uint64           value)
{
    MutexAuto lock(&m_stateMutex);
    LogQueueCall(QueueCallId::SignalQueueSemaphore);

    return QueueDecorator::SignalQueueSemaphore(pQueueSemaphore, value);
}

// =====================================================================================================================
// Log the WaitQueueSemaphore call and pass it to the next layer.
Result Queue::WaitQueueSemaphore(
    IQueueSemaphore* pQueueSemaphore,
    uint64           value)
{
    MutexAuto lock(&m_stateMutex);
    LogQueueCall(QueueCallId::WaitQueueSemaphore);

    return QueueDecorator::WaitQueueSemaphore(pQueueSemaphore, value);
}

// =====================================================================================================================
// Log the PresentDirect call and pass it to the next layer.
Result Queue::PresentDirect(
    const PresentDirectInfo& presentInfo)
{
    MutexAuto lock(&m_stateMutex);
    LogQueueCall(QueueCallId::PresentDirect);

    // Do the present before ending any per-frame experiments so that they will capture any present-time GPU work.
    Result result = QueueDecorator::PresentDirect(presentInfo);

    if ((result == Result::Success)                            &&
        m_pDevice->LoggingEnabled(GpuProfilerGranularityFrame) &&
        (m_perFrameLogItem.pGpaSession != nullptr))
    {
        result = SubmitFrameEndCmdBuf();
    }

    static_cast<Platform*>(m_pDevice->GetPlatform())->IncrementFrameId();

    // Begin sampling setup for next frame.
    if (result == Result::Success)
    {
        result = BeginNextFrame(m_pDevice->LoggingEnabled());
    }

    return result;
}

// =====================================================================================================================
// Log the Delay call and pass it to the next layer.
Result Queue::Delay(
    Util::fmilliseconds delay)
{
    MutexAuto lock(&m_stateMutex);
    LogQueueCall(QueueCallId::Delay);

    return QueueDecorator::Delay(delay);
}

// =====================================================================================================================
// Log the RemapVirtualMemoryPages call and pass it to the next layer.
Result Queue::RemapVirtualMemoryPages(
    uint32                         rangeCount,
    const VirtualMemoryRemapRange* pRanges,
    bool                           doNotWait,
    IFence*                        pFence)
{
    MutexAuto lock(&m_stateMutex);
    LogQueueCall(QueueCallId::RemapVirtualMemoryPages);

    return QueueDecorator::RemapVirtualMemoryPages(rangeCount, pRanges, doNotWait, pFence);
}

// =====================================================================================================================
// Log the CopyVirtualMemoryPageMappings call and pass it to the next layer.
Result Queue::CopyVirtualMemoryPageMappings(
    uint32                                    rangeCount,
    const VirtualMemoryCopyPageMappingsRange* pRanges,
    bool                                      doNotWait)
{
    MutexAuto lock(&m_stateMutex);
    LogQueueCall(QueueCallId::CopyVirtualMemoryPageMappings);

    return QueueDecorator::CopyVirtualMemoryPageMappings(rangeCount, pRanges, doNotWait);
}

// =====================================================================================================================
// Calls BeginNextFrame for clients that arent externally synchronized to this queue
Result Queue::BeginNextFrameAsync(
    bool samplingEnabled)
{
    MutexAuto lock(&m_stateMutex);
    return BeginNextFrame(samplingEnabled);
}

// =====================================================================================================================
// Calls ProcessIdleSubmits for clients that arent externally synchronized to this queue
void Queue::ProcessIdleSubmitsAsync()
{
    MutexAuto lock(&m_stateMutex);
    ProcessIdleSubmits();
}

} // GpuProfiler
} // Pal
