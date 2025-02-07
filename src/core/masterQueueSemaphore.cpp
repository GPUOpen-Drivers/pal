/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
    m_waitCount(0),
    m_waitThreadEnd(false)
{
}

// =====================================================================================================================
MasterQueueSemaphore::~MasterQueueSemaphore()
{
    if (m_waitThread.IsCreated())
    {
        m_waitThreadEnd = true;
        m_threadNotify.Post();
        PAL_ASSERT(m_waitThread.IsNotCurrentThread());
        m_waitThread.Join();
    }
}

// =====================================================================================================================
// Initializes this master Queue Semaphore by initializing the base object and initializing the mutex which protects
// the list of blocked Queues.
Result MasterQueueSemaphore::Init(
    const QueueSemaphoreCreateInfo& createInfo)
{
    m_signalCount = createInfo.initialCount;

    return (m_pDevice->IsNull() ? Result::Success : OsInit(createInfo));
}

// =====================================================================================================================
// Initializes this master Queue Semaphore by initializing the base object and initializing the mutex which protects
// the list of blocked Queues.
Result MasterQueueSemaphore::InitExternal()
{
    // Initial count is unknown for external shared queue semaphore and it is applicaiton's responsibility for ensuring
    // a signal has been queued prior to wait for the semaphore to operate correctly.
    m_signalCount = 0;

    return Result::Success;
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
// Condistion for direct signal/wait
bool MasterQueueSemaphore::CanWaitBeforeSubmit() const
{
#if PAL_AMDGPU_BUILD
    // For binary semaphore on Linux, if it's external or shareable, then skip batching system.
    return (IsTimeline() == false) && (IsExternalOpened() || IsShareable());
#else
    // On Windows, if it's external or shareable or timeline, then skip batching system.
    return (IsExternalOpened() || IsShareable() || IsTimeline());
#endif
}

// =====================================================================================================================
// Condition for Thread signal
bool MasterQueueSemaphore::ExternalThreadsCanSignal() const
{
    return (IsExternalOpened() || IsShareable());
}

// =====================================================================================================================
// Releases all Queues currently blocked by this Semaphore because it was just signaled on the value.
// Return how many elements are left in m_blockedQueues, they wait for future signals.
Result MasterQueueSemaphore::TimelineReleaseBlockedQueues(
    uint64          value,
    size_t*         pNumToRelease)
{
    Result result = Result::Success;

    PAL_ASSERT(IsTimeline());

    m_queuesLock.Lock();
    size_t numToRelease = m_blockedQueues.NumElements();
    // Iterate all elments in m_blockedQueues
    while ((numToRelease > 0) && (result == Result::Success))
    {
        BlockedInfo info = { };
        result = m_blockedQueues.PopFront(&info);

        // If this Queue should be released, ask it to execute all of its batched-up commands.
        if ((result == Result::Success) && (value >= info.value))
        {
            PAL_ASSERT(info.pQueue != nullptr);
            // ...it is also safe to submit any batched-up commands to the OS.
            if (result == Result::Success)
            {
                m_queuesLock.Unlock();
                result = info.pQueue->ReleaseFromStalledState(this, info.value);
                // During unlock, there could be more waits added to m_blockedQueues.
                m_queuesLock.Lock();
                numToRelease = m_blockedQueues.NumElements();
            }
        }
        else
        {
            result = m_blockedQueues.PushBack(info);
            --numToRelease;
        }
    }
    if (pNumToRelease != nullptr)
    {
        *pNumToRelease = m_blockedQueues.NumElements();
    }
    m_queuesLock.Unlock();

    return result;
}

// =====================================================================================================================
// Signals the specified Semaphore object associated with this Semaphore from the specified Queue or Host
Result MasterQueueSemaphore::SignalHelper(
    Queue*          pQueue,
    QueueSemaphore* pSemaphore,
    uint64          value,
    bool            gpuSignal)
{
    Result result = Result::Success;

    PAL_ASSERT((IsTimeline() == false) || (value != 0));
    if (m_pDevice->IsNull() == false)
    {
        if (CanWaitBeforeSubmit())
        {
            result = gpuSignal ? OsSignal(pQueue, value) : OsSignalSemaphoreValue(value);
        }
        else if (IsTimeline())
        {
            result = gpuSignal ? OsSignal(pQueue, value) : OsSignalSemaphoreValue(value);
            //This is Linux only path
            //if timeline is internal, blockedQueue is executed by sem signal.
            //if timeline is external or shared, blockedQueue is executed by Thread.
            if ((ExternalThreadsCanSignal() == false) && (result == Result::Success))
            {
                result = TimelineReleaseBlockedQueues(value, nullptr);
            }
        }
        else if (gpuSignal)
        {
            m_queuesLock.Lock();
            result = OsSignal(pQueue, value);
            // binary, internal
            ++m_signalCount;
            if (result == Result::Success)
            {
                while ((m_blockedQueues.NumElements() > 0)  && (result == Result::Success))
                {
                    BlockedInfo info = { };

                    result = m_blockedQueues.PopFront(&info);
                    // If this Queue should be released, ask it to execute all of its batched-up commands.
                    if ((m_signalCount >= info.waitCount) && (result == Result::Success))
                    {
                        PAL_ASSERT(info.pQueue != nullptr);

                        // ...it is also safe to submit any batched-up commands to the OS.
                        if (result == Result::Success)
                        {
                            m_queuesLock.Unlock();
                            result = info.pQueue->ReleaseFromStalledState(this, info.value);
                            m_queuesLock.Lock();
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
            }
            m_queuesLock.Unlock();
        }
    }

    return result;
}

// =====================================================================================================================
// Signals the specified Semaphore object associated with this Semaphore from the specified Queue.
Result MasterQueueSemaphore::SignalInternal(
    Queue*          pQueue,
    QueueSemaphore* pSemaphore,
    uint64          value)
{
    return SignalHelper(pQueue, pSemaphore, value, true);
}

// =====================================================================================================================
// Signals the specified Semaphore object associated with this Semaphore from Host side.
Result MasterQueueSemaphore::SignalSemaphoreValueInternal(
    QueueSemaphore* pSemaphore,
    uint64          value)
{
    return SignalHelper(nullptr, pSemaphore, value, false);
}

// =====================================================================================================================
// Executes the background thread used to schedule queued jobs
void MasterQueueSemaphore::RunWaitThread()
{
    while (true)
    {
        Result result = Result::Success;

        if (result == Result::Success)
        {
            result = ThreadReleaseBlockedQueues();
            PAL_ASSERT(result == Result::Success);
        }
        if (m_waitThreadEnd)
        {
            m_waitThread.End();
        }
    }

    PAL_NEVER_CALLED(); // This area should be unreachable.
}

// =====================================================================================================================
// Callback for executing wait thread.
static void WaitThreadCallback(
    void* pParameter)   // Opaque pointer to a MasterQueueSemaphore
{
    static_cast<MasterQueueSemaphore*>(pParameter)->RunWaitThread();
}

// =====================================================================================================================
// Waits on the specified Semaphore object associated with this Semaphore from the specified Queue. Potentially, this
// could cause the Queue to become blocked if the corresponding Signal hasn't been seen yet.
Result MasterQueueSemaphore::WaitInternal(
    Queue*          pQueue,
    QueueSemaphore* pSemaphore,
    uint64          value,
    volatile bool*  pIsStalled)
{
    Result result = Result::Success;

    PAL_ASSERT((IsTimeline() == false) || (value != 0));
    if (m_pDevice->IsNull() == false)
    {
        bool blockedOnThread = false;

        MutexAuto lock(&m_queuesLock);

        (*pIsStalled) = false;

        if (CanWaitBeforeSubmit())
        {
            (*pIsStalled) = false;
        }
        else if (IsTimeline())
        {
            // This is Linux only path.

            // Let the caller know if this operation results in the Queue becoming blocked... if the corresponding
            // Signal has been issued already the Queue isn't blocked from our perspective. (Although it still may be
            // from the OS' GPU scheduler's perspective...)
            (*pIsStalled) = IsWaitBeforeSignal(value);

            if (*pIsStalled)
            {
                blockedOnThread = ExternalThreadsCanSignal();
            }
        }
        else
        {
            ++m_waitCount;

            // Let the caller know if this operation results in the Queue becoming blocked... if the corresponding
            // Signal has been issued already the Queue isn't blocked from our perspective. (Although it still may be
            // from the OS' GPU scheduler's perspective...)
            (*pIsStalled) = (m_waitCount > m_signalCount);
        }

        if (*pIsStalled)
        {
            // From our perspective, the Queue is now blocked because we haven't seen the corresponding Signal to
            // this Wait. Rather than hand the OS the Wait operation now, we'll batch this up and mark the Queue
            // as blocked.
            result = AddBlockedQueue(pQueue, pSemaphore, value);
            if (result == Result::Success)
            {
                if (blockedOnThread)
                {
                    if (m_waitThread.IsCreated() == false)
                    {
                        result = m_threadNotify.Init(Semaphore::MaximumCountLimit, 0);
                        if (result == Result::Success)
                        {
                            result = m_waitThread.Begin(&WaitThreadCallback, this);
                        }
                    }
                    if (result == Result::Success)
                    {
                        m_threadNotify.Post();
                    }
                }
            }
        }
        else
        {
            // The Queue isn't blocked from our perspective, so let the operation go down to the GPU scheduler.
            result = OsWait(pQueue, value);
        }
    }

    return result;
}

// =====================================================================================================================
// for syncobj based semaphore, the earlysignal will increase m_signalCount to let the cmd submitted to the OS
// the semaphore be appended to Queue::m_waitSemList, the wait be delayed to gpuScheduler
Result MasterQueueSemaphore::EarlySignal()
{
    Result result = Result::Success;
    MutexAuto lock(&m_queuesLock);
    ++m_signalCount;

    return result;
}

// =====================================================================================================================
// Adds a new Queue to this Semaphore's list of currently-blocked Queues. Expects the queues lock to be held by the
// caller!
Result MasterQueueSemaphore::AddBlockedQueue(
    Queue*          pQueue,
    QueueSemaphore* pSemaphore,
    uint64          value)
{
    BlockedInfo info = { };
    info.pQueue      = pQueue;
    info.pSemaphore  = pSemaphore;
    info.value       = value;
    info.waitCount   = m_waitCount;

    return m_blockedQueues.PushBack(info);
}

// =====================================================================================================================
// Releases all Queues currently blocked by this Semaphore because it was just signaled.
// This is only called by waitThread.
Result MasterQueueSemaphore::ThreadReleaseBlockedQueues()
{
    Result result = Result::Success;

    PAL_ASSERT(IsTimeline() && ExternalThreadsCanSignal());

    uint64 lastPoint    = 0;
    size_t numToRelease = 0;

    result = OsQuerySemaphoreLastValue(&lastPoint);
    if (result == Result::Success)
    {
        result = TimelineReleaseBlockedQueues(lastPoint, &numToRelease);
    }

    if (result == Result::Success)
    {
        if (numToRelease > 0)
        {
            uint64 nextLast = lastPoint + 1;
            // wait for nextLast in waitThread to catch a signal event from kernel.
            result = WaitSemaphoreValueAvailable(nextLast, std::chrono::nanoseconds::max());
        }
        else
        {
            if (m_waitThreadEnd == false)
            {
                //if m_blockedQueues is empty, we wait on Util::Semaphore wait.
                result = m_threadNotify.Wait(std::chrono::milliseconds::max());
            }
        }
    }

    return result;
}

} // Pal
