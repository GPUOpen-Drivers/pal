/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/platform.h"
#include "core/queue.h"
#include "palDequeImpl.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
MasterQueueSemaphore::MasterQueueSemaphore(
    Device* pDevice)
    :
    QueueSemaphore(pDevice),
    m_blockedQueues(pDevice->GetPlatform()),
    m_signalCount(0),
    m_waitCount(0)
{
}

// =====================================================================================================================
MasterQueueSemaphore::~MasterQueueSemaphore()
{
}

// =====================================================================================================================
// Initializes this master Queue Semaphore by initializing the base object and initializing the mutex which protects
// the list of blocked Queues.
Result MasterQueueSemaphore::Init(
    const QueueSemaphoreCreateInfo& createInfo)
{
    m_signalCount = createInfo.initialCount;

    Result result = (m_pDevice->IsNull() ? Result::Success : OsInit(createInfo));
    if (result == Result::Success)
    {
        result = m_queuesLock.Init();
    }

    return result;
}

// =====================================================================================================================
// Initializes this master Queue Semaphore by initializing the base object and initializing the mutex which protects
// the list of blocked Queues.
Result MasterQueueSemaphore::InitExternal()
{
    // Initial count is unknown for external shared queue semaphore and it is applicaiton's responsibility for ensuring
    // a signal has been queued prior to wait for the semaphore to operate correctly.
    m_signalCount = 0;

    // This is not really needed as the signal may be called by other APIs and we are unable to check that so the lock
    // should never be used.
    Result result = m_queuesLock.Init();

    return result;
}

// =====================================================================================================================
// Checks if there are outstanding signal and wait operations which haven't been processed by this Semaphore yet.
bool MasterQueueSemaphore::HasStalledQueues()
{
    bool hasStalledQueues = false;
    if (IsExternalOpened() == false)
    {
        MutexAuto lock(&m_queuesLock);
        hasStalledQueues = (m_blockedQueues.NumElements() != 0);
    }
    return hasStalledQueues;
}

// =====================================================================================================================
// Checks if there are any outstanting signal and wait operations on the specified Semaphore which haven't been
// processed by this Semaphore yet.
bool MasterQueueSemaphore::IsBlockedBySemaphore(
    const QueueSemaphore* pSemaphore)
{
    PAL_ASSERT((pSemaphore != nullptr) && (pSemaphore->IsShareable() == false));

    bool blocked = false;
    if (IsExternalOpened() == false)
    {
        MutexAuto lock(&m_queuesLock);
        for (auto iter = m_blockedQueues.Begin(); iter.Get() != nullptr; iter.Next())
        {
            if (pSemaphore == iter.Get()->pSemaphore)
            {
                blocked = true;
                break;
            }
        }
    }

    return blocked;
}

// =====================================================================================================================
// Signals the specified Semaphore object associated with this Semaphore from the specified Queue.
Result MasterQueueSemaphore::SignalInternal(
    Queue*          pQueue,
    QueueSemaphore* pSemaphore)
{
    Result result = Result::Success;

    if (m_pDevice->IsNull() == false)
    {
        if (IsExternalOpened() || IsShareable())
        {
            result = OsSignal(pQueue);
        }
        else
        {
            MutexAuto lock(&m_queuesLock);

            ++m_signalCount;

            result = OsSignal(pQueue);
            if (result == Result::Success)
            {
                result = ReleaseBlockedQueues();
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Waits on the specified Semaphore object associated with this Semaphore from the specified Queue. Potentially, this
// could cause the Queue to become blocked if the corresponding Signal hasn't been seen yet.
Result MasterQueueSemaphore::WaitInternal(
    Queue*          pQueue,
    QueueSemaphore* pSemaphore,
    volatile bool*  pIsStalled)
{
    Result result = Result::Success;

    if (m_pDevice->IsNull() == false)
    {
        if (IsExternalOpened() || IsShareable())
        {
            result = OsWait(pQueue);
        }
        else
        {
            MutexAuto lock(&m_queuesLock);

            ++m_waitCount;

            // Let the caller know if this operation results in the Queue becoming blocked... if the corresponding
            // Signal has been issued already the Queue isn't blocked from our perspective. (Although it still may be
            // from the OS' GPU scheduler's perspective...)
            (*pIsStalled) = (m_waitCount > m_signalCount);
            if (*pIsStalled)
            {
                // From our perspective, the Queue is now blocked because we haven't seen the corresponding Signal to
                // this Wait. Rather than hand the OS the Wait operation now, we'll batch this up and mark the Queue
                // as blocked.
                result = AddBlockedQueue(pQueue, pSemaphore);
                if (result == Result::Success)
                {
                    // NOTE: This assertion could trip if the application or client waited on the same Queue with two
                    // separate Semaphores from multiple threads simultaneously.
                    PAL_ASSERT(pQueue->WaitingSemaphore() == nullptr);
                    pQueue->SetWaitingSemaphore(this);
                }
            }
            else
            {
                // The Queue isn't blocked from our perspective, so let the operation go down to the GPU scheduler.
                result = OsWait(pQueue);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Adds a new Queue to this Semaphore's list of currently-blocked Queues. Expects the queues lock to be held by the
// caller!
Result MasterQueueSemaphore::AddBlockedQueue(
    Queue*          pQueue,
    QueueSemaphore* pSemaphore)
{
    BlockedInfo info = { };
    info.pQueue      = pQueue;
    info.pSemaphore  = pSemaphore;
    info.waitCount   = m_waitCount;

    return m_blockedQueues.PushBack(info);
}

// =====================================================================================================================
// Releases all Queues currently blocked by this Semaphore because it was just signaled. Expects the queues lock to be
// held by the caller!
Result MasterQueueSemaphore::ReleaseBlockedQueues()
{
    Result result = Result::Success;

    while ((m_blockedQueues.NumElements() > 0) && (result == Result::Success))
    {
        BlockedInfo info = { };
        result = m_blockedQueues.PopFront(&info);

        // If this Queue should be released, ask it to execute all of its batched-up commands.
        if (m_signalCount >= info.waitCount)
        {
            // The Semaphore has been signaled already by some other Queue, so it is safe to submit the Wait request
            // from the OS' perspective.
            PAL_ASSERT(info.pQueue != nullptr);
            result = OsWait(info.pQueue);

            // ...it is also safe to submit any batched-up commands to the OS.
            if (result == Result::Success)
            {
                PAL_ASSERT(info.pQueue->WaitingSemaphore() == this);
                info.pQueue->SetWaitingSemaphore(nullptr);
                result = info.pQueue->ReleaseFromStalledState();
            }
        }
        else
        {
            // If the Queue cannot actually be relased, place it back on the list of stalled Queues and abort. No
            // other Queues after this one in the list will be release-able.
            result = m_blockedQueues.PushFront(info);
            break;
        }

    }

    return result;
}

} // Pal
