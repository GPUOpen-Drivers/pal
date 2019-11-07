/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palIntrusiveList.h"
#include "palMutex.h"
#include "palQueue.h"
#include "palSemaphore.h"
#include "palThread.h"

namespace Pal
{

class Device;
class SwapChain;

// Tells the worker thread how to interpret a job.
enum class PresentJobType : uint32
{
    Terminate = 0, // The worker thread should terminate itself.
    Notify,        // The worker thread should signal a semaphore to let another thread know it has flushed prior work.
    Present,       // A present should be executed.
};

// =====================================================================================================================
// A helper class to encapsulate all objects and data needed for each asynchronous present scheduler job. This class
// uses the Create/Destroy pattern but only provides "Internal" versions because the present scheduler will never have
// the opportunity to place an instance of this class into preallocated memory.
class PresentSchedulerJob
{
    typedef Util::IntrusiveListNode<PresentSchedulerJob> Node;

public:
    static Result CreateInternal(Device* pDevice, PresentSchedulerJob** ppPresentSchedulerJob);
    void DestroyInternal(Device* pDevice);

    Node* ListNode() { return &m_node; }
#if !defined(__unix__)
    IFence* PriorWorkFence() { return m_pPriorWorkFence; }
#endif
    void SetType(PresentJobType type) { m_type = type; }
    PresentJobType GetType() const { return m_type; }

    void SetPresentInfo(const PresentSwapChainInfo& presentInfo) { m_presentInfo = presentInfo; }
    const PresentSwapChainInfo& GetPresentInfo() const { return m_presentInfo; }

    void SetQueue(IQueue* pQueue) { m_pQueue = pQueue; }
    IQueue* GetQueue() const { return m_pQueue; }

private:
    PresentSchedulerJob();
    ~PresentSchedulerJob();

    Node                 m_node;            // The present scheduler maintains intrusive lists of jobs.
#if !defined(__unix__)
    IFence*              m_pPriorWorkFence; // Signaled when the application's work prior to this present has completed.
#endif
    PresentJobType       m_type;            // How to interpret this job (e.g., execute a present).
    PresentSwapChainInfo m_presentInfo;     // All of the information for a present.
    IQueue*              m_pQueue;          // Internal queue of the same device as the original presentation queue.
};

// =====================================================================================================================
// A present scheduler consumes a stream of swap chain presentation requests, ensuring that each present is processed
// in order and executed to spec. Depending on the features and limitations of the HW and OS, the present scheduler may
// elect to execute certain presents inline on the application's queue or asynchronously on an internal queue. Many
// swap chain present modes require CPU-side synchronization so an internal thread may be used to hide the stalls.
class PresentScheduler
{
    typedef Util::IntrusiveList<PresentSchedulerJob> JobList;

public:
    // Present schedulers use the Create/Destroy pattern. The Create functions are in the OS-specific classes.
    void Destroy() { this->~PresentScheduler(); }

    // These functions correspond to ISwapChain's image acquire/release points, scheduling the necessary syncs and
    // GPU work necessary to execute a complete swap chain present cycle.
    virtual Result SignalOnAcquire(IQueueSemaphore* pWaitSemaphore, IQueueSemaphore* pSemaphore, IFence* pFence);
    Result Present(const PresentSwapChainInfo& presentInfo, IQueue* pQueue);

    // Waits for all internal present work to be idle before returning.
    Result WaitIdle();

    // Must be declared public but meant for internal use only.
    void RunWorkerThread();

protected:
    PresentScheduler(Device* pDevice);
    virtual ~PresentScheduler();

    virtual Result Init(IDevice*const pSlaveDevices[], void* pPlacementAddr);
    virtual Result PreparePresent(IQueue* pQueue, PresentSchedulerJob* pJob);

    // The OS-specific present scheduler classes must override these missing pieces of the scheduling algorithm.
    // The CanInlinePresent function should return true if its possible and desirable to immediately queue the present
    // on the given application queue. Inline presents cannot stall the calling thread.
    virtual bool CanInlinePresent(const PresentSwapChainInfo& presentInfo, const IQueue& queue) const { return false; }
    virtual Result ProcessPresent(const PresentSwapChainInfo& presentInfo, IQueue* pQueue, bool isInline) = 0;
    virtual Result FailedToQueuePresentJob(const PresentSwapChainInfo& presentInfo, IQueue* pQueue) = 0;

    Device*const m_pDevice;

    // These queues are created by the OS-specific subclasses. The present queues are not required if we can guarantee
    // that the worker thread will never be used.

    IQueue*      m_pSignalQueue;                   // Used to signal swap chain acquire semaphores and fences.
    IQueue*      m_pPresentQueues[XdmaMaxDevices]; // Used by the worker thread to execute presents asynchronously.

private:
    Result GetIdleJob(PresentSchedulerJob** ppJob);
    void EnqueueJob(PresentSchedulerJob* pJob);

    // All of this state is used to store and process asynchronous presentation requests. If all presents can be inlined
    // none of it will be used and the worker thread will never be started.

    JobList         m_idleJobList;        // Idle job objects which are waiting to be reused.
    Util::Mutex     m_idleJobMutex;       // Protects access to m_idleJobList.
    JobList         m_activeJobList;      // Passes jobs from application threads to the worker thread.
    Util::Mutex     m_activeJobMutex;     // Protects access to m_activeJobList.
    Util::Semaphore m_activeJobSemaphore; // Signaled when a job is added to m_activeJobList.
    Util::Semaphore m_workerThreadNotify; // Signaled when the worker thread completes a Notify job.
    Util::Thread    m_workerThread;       // The driver thread that executes presents later on.
    volatile bool   m_workerActive;       // If the driver thread has been created.

    PAL_DISALLOW_DEFAULT_CTOR(PresentScheduler);
    PAL_DISALLOW_COPY_AND_ASSIGN(PresentScheduler);
};

} // Pal
