/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include "core/queueSemaphore.h"
#include "palDeque.h"
#include "palMutex.h"
#include "palSemaphore.h"
#include "palThread.h"

namespace Pal
{

// Forward decl's
class Platform;

// =====================================================================================================================
// Specialization of QueueSemaphore to handle Semaphores for single-GPU scenarios or the "master" Semaphore for multi-
// GPU shared Semaphore objects.
class MasterQueueSemaphore : public QueueSemaphore
{
public:
    explicit MasterQueueSemaphore(Device* pDevice);
    virtual ~MasterQueueSemaphore();

    virtual Result Init(const QueueSemaphoreCreateInfo& createInfo);

    // Master Queue Semaphores don't suppor the 'Open' operation!
    virtual Result Open(const QueueSemaphoreOpenInfo& openInfo) override { return Result::ErrorUnavailable; }

    // Instructs a Queue to signal this Semaphore.
    virtual Result Signal(
        Queue* pQueue,
        uint64 value) override { return SignalInternal(pQueue, this, value); }

    // Instructs host to signal this semaphore.
    virtual Result SignalSemaphoreValue(uint64  value) override { return SignalSemaphoreValueInternal(this, value); };

    // Instructs a Queue to wait on this Semaphore.
    virtual Result Wait(
        Queue*         pQueue,
        uint64         value,
        volatile bool* pIsStalled) override { return WaitInternal(pQueue, this, value, pIsStalled); }

    // NOTE: Part of the public IQueueSemaphore interface.
    virtual bool HasStalledQueues() override;

    bool IsBlockedBySemaphore(const QueueSemaphore* pSemaphore);
    Result SignalInternal(
        Queue*          pQueue,
        QueueSemaphore* pSemaphore,
        uint64          value);

    Result SignalSemaphoreValueInternal(
        QueueSemaphore* pSemaphore,
        uint64          value);

    Result WaitInternal(
        Queue*          pQueue,
        QueueSemaphore* pSemaphore,
        uint64          value,
        volatile bool*  pIsStalled);

    Result InitExternal();

    Result EarlySignal();

    void RunWaitThread();

private:
    Result AddBlockedQueue(
        Queue*          pQueue,
        QueueSemaphore* pSemaphore,
        uint64          value);
    Result ThreadReleaseBlockedQueues();

    Result SignalHelper(
        Queue*          pQueue,
        QueueSemaphore* pSemaphore,
        uint64          value,
        bool            gpuSignal);

    Result TimelineReleaseBlockedQueues(
        uint64          value,
        size_t*         pNumToRelease);

    bool CanWaitBeforeSubmit() const;
    bool ExternalThreadsCanSignal() const;

    Util::Mutex  m_queuesLock;

    // Information associating a wait-count with the appropriate blocking Queue.
    struct BlockedInfo
    {
        Queue*           pQueue;        // The blocked Queue
        QueueSemaphore*  pSemaphore;    // The blocking Semaphore
        uint64           value;         // timeline semaphore point value
        uint64           waitCount;     // The wait-count before the Queue becomes unblocked
    };

    // Tracks the set of Queues blocked by this Semaphore, and their associated wait-counts.
    Util::Deque<BlockedInfo, Platform>  m_blockedQueues;

    // Tracks the total number of times this Semaphore has been waited-on and signaled. Includes the initial
    // count specified at creation-time.
    uint64  m_signalCount;
    uint64  m_waitCount;

    Util::Semaphore m_threadNotify;         // Notify wait thread to consume semaphore wait.
    Util::Thread    m_waitThread;           // The wait thread that executes semaphore wait.
    volatile bool   m_waitThreadEnd;        // Send signal to thread to end thread

    PAL_DISALLOW_DEFAULT_CTOR(MasterQueueSemaphore);
    PAL_DISALLOW_COPY_AND_ASSIGN(MasterQueueSemaphore);
};

} // Pal
