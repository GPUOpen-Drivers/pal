/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/queueSemaphore.h"
#include "core/os/lnx/lnxDevice.h"
#include "core/os/lnx/lnxQueue.h"

namespace Pal
{

// =====================================================================================================================
QueueSemaphore::~QueueSemaphore()
{
    if (m_hSemaphore != 0)
    {
        Result result = static_cast<Linux::Device*>(m_pDevice)->DestroySemaphore(m_hSemaphore);
        PAL_ASSERT(result == Result::Success);
    }
}

// =====================================================================================================================
// Finishes initializing a QueueSemaphore object.
Result QueueSemaphore::OsInit(
    const QueueSemaphoreCreateInfo& createInfo)
{
    bool isCreateSignaledSemaphore = false;
    Linux::Device* pLnxDevice = static_cast<Linux::Device*>(m_pDevice);

    if ((pLnxDevice->GetSemaphoreType() == Linux::SemaphoreType::SyncObj) &&
        (pLnxDevice->IsInitialSignaledSyncobjSemaphoreSupported()))
    {
        m_skipNextWait = false;
        isCreateSignaledSemaphore = (createInfo.initialCount != 0);
    }
    else
    {
        m_skipNextWait = (createInfo.initialCount != 0);
        isCreateSignaledSemaphore = false;
    }

    return pLnxDevice->CreateSemaphore(isCreateSignaledSemaphore, &m_hSemaphore);
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
    const QueueSemaphoreExportInfo& exportInfo) const
{
    return static_cast<Linux::Device*>(m_pDevice)->ExportSemaphore(m_hSemaphore, exportInfo.flags.isReference);
}

// =====================================================================================================================
Result QueueSemaphore::OpenExternal(
    const ExternalQueueSemaphoreOpenInfo& openInfo)
{
    PAL_ASSERT(static_cast<int32>(openInfo.externalSemaphore) != -1);

    m_flags.shared         = 1;
    m_flags.externalOpened = 1;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 398
    Result result = static_cast<Linux::Device*>(m_pDevice)->ImportSemaphore(openInfo.externalSemaphore,
                                                                            &m_hSemaphore,
                                                                            openInfo.flags.isReference);
#else
    Result result = static_cast<Linux::Device*>(m_pDevice)->ImportSemaphore(openInfo.externalSemaphore,
                                                                            &m_hSemaphore,
                                                                            true);
#endif

    return result;
}

// =====================================================================================================================
// Enqueues a command on the specified Queue to signal this Semaphore when all outstanding command buffers have
// completed.
Result QueueSemaphore::OsSignal(
    Queue* pQueue)
{
    return static_cast<Linux::Queue*>(pQueue)->SignalSemaphore(m_hSemaphore);
}

// =====================================================================================================================
// Enqueues a command on the specified Queue to stall that Queue until the Semaphore is signalled by another Queue.
Result QueueSemaphore::OsWait(
    Queue* pQueue)
{
    Result result = Result::Success;

    // Currently amdgpu lacks a way to signal a semaphore at creation. As a workaround, we skip the wait if this is set.
    if (m_skipNextWait)
    {
        m_skipNextWait = false;
    }
    else
    {
        result = static_cast<Linux::Queue*>(pQueue)->WaitSemaphore(m_hSemaphore);
    }

    return result;
}

} // Pal
