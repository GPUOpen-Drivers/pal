/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/gpuMemory.h"
#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuQueue.h"
#include "core/queueSemaphore.h"

namespace Pal
{

// =====================================================================================================================
QueueSemaphore::~QueueSemaphore()
{
    if (m_hSemaphore != nullptr)
    {
        Result result = static_cast<Amdgpu::Device*>(m_pDevice)->DestroySemaphore(m_hSemaphore);
        PAL_ASSERT(result == Result::Success);
    }
}

// =====================================================================================================================
// Finishes initializing a QueueSemaphore object.
Result QueueSemaphore::OsInit(
    const QueueSemaphoreCreateInfo& createInfo)
{
    m_flags.shareable      = createInfo.flags.shareable;
    m_flags.externalOpened = createInfo.flags.externalOpened;
    m_flags.timeline       = createInfo.flags.timeline;
    m_maxWaitsPerSignal    = createInfo.maxCount;

    auto*const pAmdgpuDevice = static_cast<Amdgpu::Device*>(m_pDevice);
    bool       isCreateSignaledSemaphore = false;

    if ((pAmdgpuDevice->GetSemaphoreType() == Amdgpu::SemaphoreType::SyncObj) &&
        (pAmdgpuDevice->IsInitialSignaledSyncobjSemaphoreSupported()))
    {
        m_skipNextWait = false;
        isCreateSignaledSemaphore = (createInfo.initialCount != 0);
    }
    else
    {
        m_skipNextWait = (createInfo.initialCount != 0);
        isCreateSignaledSemaphore = false;
    }

    return pAmdgpuDevice->CreateSemaphore(isCreateSignaledSemaphore,
                                          m_flags.timeline,
                                          createInfo.initialCount,
                                          &m_hSemaphore);
}

// =====================================================================================================================
// Finishes opening a shared QueueSemaphore which was created from another GPU in this GPU's linked-adapter chain.
Result QueueSemaphore::Open(
    const QueueSemaphoreOpenInfo& openInfo)
{
    Result result = Result::Success;

    // Not supported yet.
    PAL_NOT_IMPLEMENTED();

    return result;
}

// =====================================================================================================================
OsExternalHandle QueueSemaphore::ExportExternalHandle(
    const QueueSemaphoreExportInfo& exportInfo
    ) const
{
    return static_cast<Amdgpu::Device*>(m_pDevice)->ExportSemaphore(m_hSemaphore, exportInfo.flags.isReference);
}

// =====================================================================================================================
Result QueueSemaphore::OpenExternal(
    const ExternalQueueSemaphoreOpenInfo& openInfo)
{
    PAL_ASSERT(static_cast<int32>(openInfo.externalSemaphore) != -1);

    m_flags.shared         = 1;
    m_flags.externalOpened = 1;
    m_flags.timeline       = openInfo.flags.timeline;

    return static_cast<Amdgpu::Device*>(m_pDevice)->ImportSemaphore(openInfo.externalSemaphore,
                                                                    &m_hSemaphore,
                                                                    openInfo.flags.isReference);
}

// =====================================================================================================================
// Enqueues a command on the specified Queue to signal this Semaphore when all outstanding command buffers have
// completed.
Result QueueSemaphore::OsSignal(
    Queue* pQueue,
    uint64 value)
{
    return static_cast<Amdgpu::Queue*>(pQueue)->SignalSemaphore(m_hSemaphore, value);
}

// =====================================================================================================================
// Enqueues a command on the specified Queue to stall that Queue until the Semaphore is signalled by another Queue.
Result QueueSemaphore::OsWait(
    Queue* pQueue,
    uint64 value)
{
    Result result = Result::Success;

    // Currently amdgpu lacks a way to signal a semaphore at creation. As a workaround, we skip the wait if this is set.
    if (m_skipNextWait)
    {
        m_skipNextWait = false;
    }
    else
    {
        result = static_cast<Amdgpu::Queue*>(pQueue)->WaitSemaphore(m_hSemaphore, value);
    }

    return result;
}

// =====================================================================================================================
// Query timeline semaphore payload.
Result QueueSemaphore::QuerySemaphoreValue(
    uint64*                  pValue)
{
    // Only timeline semaphore supports this method.
    return m_flags.timeline ?
             static_cast<Amdgpu::Device*>(m_pDevice)->QuerySemaphoreValue(m_hSemaphore, pValue, 0) :
             Result::ErrorInvalidObjectType;
}

// =====================================================================================================================
// Wait on timeline specific point with timeoutNs
Result QueueSemaphore::WaitSemaphoreValue(
    uint64                   value,
    uint64                   timeoutNs)
{
    uint32 flags = DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL |
        DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT;

    // Only timeline semaphore supports this method.
    return m_flags.timeline ?
             static_cast<Amdgpu::Device*>(m_pDevice)->WaitSemaphoreValue(m_hSemaphore, value, flags, timeoutNs) :
             Result::ErrorInvalidObjectType;
}

// =====================================================================================================================
// Query last available point of timelien semaphore
Result QueueSemaphore::OsQuerySemaphoreLastValue(uint64*  pValue)
{

    return static_cast<Amdgpu::Device*>(m_pDevice)->QuerySemaphoreValue(
            m_hSemaphore, pValue, DRM_SYNCOBJ_QUERY_FLAGS_LAST_SUBMITTED);
}

// =====================================================================================================================
// Wait available on timeline specific point with timeoutNs
Result QueueSemaphore::WaitSemaphoreValueAvailable(
    uint64                   value,
    uint64                   timeoutNs)
{
    uint32 flags = DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE |
        DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT;

    return static_cast<Amdgpu::Device*>(m_pDevice)->WaitSemaphoreValue(m_hSemaphore, value, flags, timeoutNs);

}

// =====================================================================================================================
// Query if WaitBeforeSignal happens on timeline specific point.
bool QueueSemaphore::IsWaitBeforeSignal(
    uint64                   value)
{
    // Only timeline semaphore supports this method.
    return m_flags.timeline ?
             static_cast<Amdgpu::Device*>(m_pDevice)->IsWaitBeforeSignal(m_hSemaphore, value) :
             false;
}

// =====================================================================================================================
// Signal on timeline specific point
Result QueueSemaphore::OsSignalSemaphoreValue(
    uint64                   value)
{
    // Only timeline semaphore supports this method.
    return m_flags.timeline ?
        static_cast<Amdgpu::Device*>(m_pDevice)->SignalSemaphoreValue(m_hSemaphore, value) :
        Result::ErrorInvalidObjectType;
}
} // Pal
