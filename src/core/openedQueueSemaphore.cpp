/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/masterQueueSemaphore.h"
#include "core/openedQueueSemaphore.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
OpenedQueueSemaphore::OpenedQueueSemaphore(
    Device* pDevice)
    :
    QueueSemaphore(pDevice),
    m_pMaster(nullptr)
{
}

// =====================================================================================================================
// Opens this shared Queue Semaphore object by saving off the original "master" Semaphore and delegating to the base
// class.
Result OpenedQueueSemaphore::Open(
    const QueueSemaphoreOpenInfo& openInfo)
{
    m_pMaster = static_cast<MasterQueueSemaphore*>(openInfo.pSharedQueueSemaphore);
    PAL_ASSERT(m_pMaster != nullptr);

    return QueueSemaphore::Open(openInfo);
}

// =====================================================================================================================
// Checks if there are outstanding signal and wait operations which haven't been processed by this Semaphore yet.
bool OpenedQueueSemaphore::HasStalledQueues()
{
    return m_pMaster->IsBlockedBySemaphore(this);
}

// =====================================================================================================================
// Signals this Semaphore object from the specified Queue.
Result OpenedQueueSemaphore::Signal(
    Queue* pQueue,
    uint64 value)
{
    return m_pMaster->SignalInternal(pQueue, this, value);
}

// =====================================================================================================================
// Waits-on this Semaphore object using the specified Queue.
Result OpenedQueueSemaphore::Wait(
    Queue*         pQueue,
    uint64         value,
    volatile bool* pIsStalled)
{
    return m_pMaster->WaitInternal(pQueue, this, value, pIsStalled);
}

// =====================================================================================================================
// Waits-on this Semaphore object using host.
Result OpenedQueueSemaphore::SignalSemaphoreValue(
    uint64  value)
{
    return m_pMaster->SignalSemaphoreValue(value);
}

} // Pal
