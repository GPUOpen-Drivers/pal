/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/presentScheduler.h"
#include "core/queue.h"
#include "core/swapChain.h"
#include "palIntrusiveListImpl.h"
using namespace Util;

namespace Pal
{

// =====================================================================================================================
Result PresentSchedulerJob::CreateInternal(
    Device*               pDevice,
    PresentSchedulerJob** ppPresentSchedulerJob)
{
    const size_t totalSize = sizeof(PresentSchedulerJob) + pDevice->GetFenceSize(nullptr);
    void*        pMemory   = PAL_MALLOC(totalSize, pDevice->GetPlatform(), AllocInternal);
    Result       result    = Result::Success;

    if (pMemory == nullptr)
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        PresentSchedulerJob*const pJob = PAL_PLACEMENT_NEW(pMemory) PresentSchedulerJob();

        if (result == Result::Success)
        {
            *ppPresentSchedulerJob = pJob;
        }
        else
        {
            pJob->DestroyInternal(pDevice);
        }
    }

    return result;
}

// =====================================================================================================================
void PresentSchedulerJob::DestroyInternal(
    Device* pDevice)
{
    Platform*const pPlatform = pDevice->GetPlatform();
    this->~PresentSchedulerJob();
    PAL_FREE(this, pPlatform);
}

// =====================================================================================================================
PresentSchedulerJob::PresentSchedulerJob()
    :
    m_node(this),
    m_type(PresentJobType::Terminate)
{
    memset(&m_presentInfo, 0, sizeof(m_presentInfo));
}

// =====================================================================================================================
PresentSchedulerJob::~PresentSchedulerJob()
{
}

// =====================================================================================================================
PresentScheduler::PresentScheduler(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_pSignalQueue(nullptr),
    m_pPresentQueue(nullptr),
    m_workerActive(false)
{
}

// =====================================================================================================================
PresentScheduler::~PresentScheduler()
{
    // Closing down the scheduling thread must be the first thing we do, to prevent data races.
    if (m_workerActive)
    {
        PAL_ASSERT(m_workerThread.IsNotCurrentThread());

        PresentSchedulerJob* pJob = nullptr;
        if (GetIdleJob(&pJob) == Result::Success)
        {
            pJob->SetType(PresentJobType::Terminate);

            EnqueueJob(pJob);
            m_workerThread.Join();
        }
        else
        {
            // We failed to queue a Terminate job so the worker thread isn't going to terminate.
            PAL_ASSERT_ALWAYS();
        }
    }

    if (m_pSignalQueue != nullptr)
    {
        m_pSignalQueue->Destroy();
        m_pSignalQueue = nullptr;
    }

    if (m_pPresentQueue != nullptr)
    {
        m_pPresentQueue->Destroy();
        m_pPresentQueue = nullptr;
    }

    for (auto iter = m_idleJobList.Begin(); iter.IsValid(); )
    {
        PresentSchedulerJob*const pJob = iter.Get();
        m_idleJobList.Erase(&iter);
        pJob->DestroyInternal(m_pDevice);
    }

    for (auto iter = m_activeJobList.Begin(); iter.IsValid(); )
    {
        PresentSchedulerJob*const pJob = iter.Get();
        m_activeJobList.Erase(&iter);
        pJob->DestroyInternal(m_pDevice);
    }
}

// =====================================================================================================================
Result PresentScheduler::Init(
    void* pPlacementAddr)
{
    Result result = m_idleJobMutex.Init();

    if (result == Result::Success)
    {
        result = m_activeJobMutex.Init();
    }

    if (result == Result::Success)
    {
        result = m_activeJobSemaphore.Init(Semaphore::MaximumCountLimit, 0);
    }

    if (result == Result::Success)
    {
        result = m_workerThreadNotify.Init(Semaphore::MaximumCountLimit, 0);
    }

    return result;
}

// =====================================================================================================================
// Asks the scheduler to signal the given semaphore and/or fence on its internal signal queue, indicating that it's safe
// for the application to begin using some swap chain image. If pPresentComplete is non-null the queue will wait on it
// first before signaling the other objects. Otherwise the caller must guarantee that it is safe to immediately begin
// using the image.
Result PresentScheduler::SignalOnAcquire(
    IQueueSemaphore* pPresentComplete,
    IQueueSemaphore* pSemaphore,
    IFence*          pFence)
{
    // We expect that at least one of these is valid or it's not possible for the app to acquire the image.
    PAL_ASSERT((pSemaphore != nullptr) || (pFence != nullptr));

    // Note that we don't need to take a mutex to protect the signal queue because this function is only ever called
    // by the swap chain that owns this present scheduler. The caller must already be protecting access to that swap
    // chain.
    const Result result = (pPresentComplete != nullptr) ? m_pSignalQueue->WaitQueueSemaphore(pPresentComplete)
                                                        : Result::Success;

    if (result == Result::Success)
    {
        // If we queued the present complete wait but failed to queue one of these signals we still want to return
        // Success to allow the application to acquire the given swap chain image because we need a follow-up signal
        // on the present complete semaphore to guarantee that we don't deadlock the swap chain.
        if (pSemaphore != nullptr)
        {
            const Result semaphoreResult = m_pSignalQueue->SignalQueueSemaphore(pSemaphore);
            PAL_ASSERT(semaphoreResult == Result::Success);
        }

        if (pFence != nullptr)
        {
            SubmitInfo submitInfo = {};
            submitInfo.pFence     = pFence;

            const Result fenceResult = m_pSignalQueue->Submit(submitInfo);
            PAL_ASSERT(fenceResult == Result::Success);
        }
    }

    return result;
}

// =====================================================================================================================
// Callback for executing the present scheduler's worker thread.
static void WorkerThreadCallback(
    void* pParameter)   // Opaque pointer to a PresentScheduler object
{
    static_cast<PresentScheduler*>(pParameter)->RunWorkerThread();
}

// =====================================================================================================================
Result PresentScheduler::PreparePresent(
    IQueue*              pQueue,
    PresentSchedulerJob* pJob)
{
    return Result::Success;
}

// =====================================================================================================================
// Asks the scheduler to queue a present. The present operation may be queued immediately on the given queue or
// scheduled for presentation at a later time using the internal scheduling thread and queue.
Result PresentScheduler::Present(
    const PresentSwapChainInfo& presentInfo,
    IQueue*                     pQueue)
{
    Result result = Result::Success;

    // Check if we can immediately process a present on the current thread and queue.
    if (CanInlinePresent(presentInfo, *pQueue))
    {
        result = ProcessPresent(presentInfo, pQueue, true);
    }
    else
    {
        // Otherwise, we must queue this present for delayed execution on our scheduling thread.
        if (m_workerThread.IsCreated() == false)
        {
            result = m_workerThread.Begin(&WorkerThreadCallback, this);

            // Now that we've launched the scheduling thread it must be terminated in our destructor.
            m_workerActive = m_workerThread.IsCreated();
        }

        PresentSchedulerJob* pJob = nullptr;

        if (result == Result::Success)
        {
            result = GetIdleJob(&pJob);
        }

        if (result == Result::Success)
        {
            result = PreparePresent(pQueue, pJob);
        }

        if (result == Result::Success)
        {
            pJob->SetType(PresentJobType::Present);
            pJob->SetPresentInfo(presentInfo);

            EnqueueJob(pJob);
        }
        else
        {
            // If we failed to queue the job we must clean up some state to prevent the swap chain from deadlocking.
            const Result cleanupResult = FailedToQueuePresentJob(presentInfo, pQueue);
            result = CollapseResults(result, cleanupResult);
        }
    }

    return result;
}

// =====================================================================================================================
Result PresentScheduler::WaitIdle()
{
    Result result = Result::Success;

    // If the worker thread is in use, wait for it to notify us that it's flushed all of its prior work.
    if (m_workerActive)
    {
        PresentSchedulerJob* pJob = nullptr;
        result = GetIdleJob(&pJob);

        if (result == Result::Success)
        {
            pJob->SetType(PresentJobType::Notify);
            EnqueueJob(pJob);

            result = m_workerThreadNotify.Wait(UINT32_MAX);
        }
    }

    // Then wait for the present queue and signal queue in that order to flush any remaining queue operations.
    if (result == Result::Success)
    {
        result = m_pPresentQueue->WaitIdle();
    }

    if (result == Result::Success)
    {
        result = m_pSignalQueue->WaitIdle();
    }

    return result;
}

// =====================================================================================================================
// A thread-safe helper function to reuse an idle PresentSchedulerJob or create a new one.
Result PresentScheduler::GetIdleJob(
    PresentSchedulerJob** ppJob)
{
    Result    result = Result::Success;
    MutexAuto lock(&m_idleJobMutex);

    if (m_idleJobList.IsEmpty())
    {
        result = PresentSchedulerJob::CreateInternal(m_pDevice, ppJob);
    }
    else
    {
        *ppJob = m_idleJobList.Front();
        m_idleJobList.Erase((*ppJob)->ListNode());
    }

    return result;
}

// =====================================================================================================================
// A thread-safe helper function to add the given job to the job queue and signal the job semaphore.
void PresentScheduler::EnqueueJob(
    PresentSchedulerJob* pJob)
{
    m_activeJobMutex.Lock();
    m_activeJobList.PushBack(pJob->ListNode());
    m_activeJobMutex.Unlock();

    // Post after we unlock the mutex to prevent the worker thread from blocking if it wakes up too quickly.
    m_activeJobSemaphore.Post();
}

// =====================================================================================================================
// Executes the background thread used to schedule presents at the appropriate times.
void PresentScheduler::RunWorkerThread()
{
    while (true)
    {
        // Sleep until we have a job to process.
        const Result result = m_activeJobSemaphore.Wait(UINT32_MAX);
        PAL_ASSERT(IsErrorResult(result) == false);

        if (result == Result::Success)
        {
            m_activeJobMutex.Lock();
            PresentSchedulerJob*const pJob = m_activeJobList.Front();
            m_activeJobList.Erase(pJob->ListNode());

            m_activeJobMutex.Unlock();

            switch (pJob->GetType())
            {
            case PresentJobType::Terminate:
                m_idleJobMutex.Lock();
                m_idleJobList.PushBack(pJob->ListNode());
                m_idleJobMutex.Unlock();

                // We've been asked to kill this thread.
                m_workerActive = false;
                m_workerThread.End();
                break;

            case PresentJobType::Notify:
                m_idleJobMutex.Lock();
                m_idleJobList.PushBack(pJob->ListNode());
                m_idleJobMutex.Unlock();

                m_workerThreadNotify.Post();
                break;

            case PresentJobType::Present:
                {
                    const Result presentResult = ProcessPresent(pJob->GetPresentInfo(), m_pPresentQueue, false);
                    PAL_ALERT(IsErrorResult(presentResult));
                }

                m_idleJobMutex.Lock();
                m_idleJobList.PushBack(pJob->ListNode());
                m_idleJobMutex.Unlock();
                break;

            default:
                PAL_ASSERT_ALWAYS();
                break;
            }
        }
    }

    PAL_NEVER_CALLED(); // This area should be unreachable.
}

} // Pal
