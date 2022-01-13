/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
        PAL_ASSERT(pJob != nullptr);
#if !defined(__unix__)
        // The only unusual part of this process is here, where we place our job's fence immediately after it.
        // Create the fence if it is required to do explicit synchronization between present and prior work
        // Linux kernel would guarantee the in-order execution of render and present by respecting the
        // order of submissions that refers to the same buffer object.
        // In this case, the prior work fence is not needed since it is already serialized implicitly in the kernel.
        {
            pMemory = VoidPtrInc(pMemory, sizeof(PresentSchedulerJob));
            Pal::FenceCreateInfo createInfo = {};
            result  = pDevice->CreateFence(createInfo, pMemory, &pJob->m_pPriorWorkFence);
        }
#endif

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
#if !defined(__unix__)
    m_pPriorWorkFence(nullptr),
#endif
    m_type(PresentJobType::Terminate)
{
    memset(&m_presentInfo, 0, sizeof(m_presentInfo));
}

// =====================================================================================================================
PresentSchedulerJob::~PresentSchedulerJob()
{
#if !defined(__unix__)
    if (m_pPriorWorkFence != nullptr)
    {
        m_pPriorWorkFence->Destroy();
        m_pPriorWorkFence = nullptr;
    }
#endif
}

// =====================================================================================================================
PresentScheduler::PresentScheduler(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_pSignalQueue(nullptr),
    m_workerActive(false),
    m_previousPresentResult(Result::Success)
{
    for (uint32 deviceIndex = 0; deviceIndex < XdmaMaxDevices; deviceIndex++)
    {
        m_pPresentQueues[deviceIndex] = nullptr;
    }
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

    for (uint32 deviceIndex = 0; deviceIndex < XdmaMaxDevices; deviceIndex++)
    {
        if (m_pPresentQueues[deviceIndex] != nullptr)
        {
            m_pPresentQueues[deviceIndex]->Destroy();
            m_pPresentQueues[deviceIndex] = nullptr;
        }
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
    IDevice*const pSlaveDevices[],
    void*         pPlacementAddr)
{
    Result result = Result::Success;

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
            constexpr PerSubQueueSubmitInfo PerSubQueueInfo = {};
            MultiSubmitInfo submitInfo      = {};
            submitInfo.perSubQueueInfoCount = 1;
            submitInfo.pPerSubQueueInfo     = &PerSubQueueInfo;
            submitInfo.ppFences             = &pFence;
            submitInfo.fenceCount           = 1;
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
#if !defined(__unix__)
    // Use an empty submit to get the job's fence signaled once the app's prior rendering is completed.
    // The scheduling thread will use this fence to know when the image is ready to be presented.
    IFence* pFence = pJob->PriorWorkFence();
    Result result = m_pDevice->ResetFences(1, &pFence);

    if (result == Result::Success)
    {
        constexpr PerSubQueueSubmitInfo PerSubQueueInfo = {};
        MultiSubmitInfo submitInfo      = {};
        submitInfo.perSubQueueInfoCount = 1;
        submitInfo.pPerSubQueueInfo     = &PerSubQueueInfo;
        submitInfo.ppFences             = &pFence;
        submitInfo.fenceCount           = 1;

        result = pQueue->Submit(submitInfo);
    }
    return result;
#else
    return Result::Success;
#endif
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
            pJob->SetType(PresentJobType::Present);
            pJob->SetPresentInfo(presentInfo);
        }

        if (result == Result::Success)
        {
            result = PreparePresent(pQueue, pJob);
        }

        if (result == Result::Success)
        {
            // Choose the internal presentation queue of the same device as the provided presentation queue.
            Queue*  pClientQueue   = static_cast<Queue*>(pQueue);
            IQueue* pInternalQueue = nullptr;

            for (uint32 deviceIndex = 0; deviceIndex < XdmaMaxDevices; deviceIndex++)
            {
                if ((m_pPresentQueues[deviceIndex] != nullptr) &&
                    (static_cast<Queue*>(m_pPresentQueues[deviceIndex])->GetDevice()) == pClientQueue->GetDevice())
                {
                    pInternalQueue = m_pPresentQueues[deviceIndex];
                    break;
                }
            }

            if ((pInternalQueue != nullptr) &&
                ((presentInfo.presentMode != PresentMode::Windowed) || (m_pDevice == pClientQueue->GetDevice())))
            {
                pJob->SetQueue(pInternalQueue);

                EnqueueJob(pJob);
            }
            else
            {
                // A valid present queue was not found either because:
                // 1. We didn't find a matching queue in the m_pPresentQueues array.
                // 2. This is a windowed present and the pClientQueue's parent device is not the swap chain's parent
                //    device.
                result = Result::ErrorIncompatibleQueue;
            }
        }

        if (result != Result::Success)
        {
            // If we failed to queue the job we must clean up some state to prevent the swap chain from deadlocking.
            const Result cleanupResult = FailedToQueuePresentJob(presentInfo, pQueue);
            result = CollapseResults(result, cleanupResult);
        }
        else
        {
            // If we succesfully queued the job, report the result of previous job to the client so they can
            // handle any presentation errors.
            result = m_previousPresentResult;
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

    // Then wait for the present queues and signal queue in that order to flush any remaining queue operations.
    for (uint32 deviceIndex = 0; deviceIndex < XdmaMaxDevices; deviceIndex++)
    {
        if ((result == Result::Success) && (m_pPresentQueues[deviceIndex] != nullptr))
        {
            result = m_pPresentQueues[deviceIndex]->WaitIdle();
        }
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
#if !defined(__unix__)
                    // Block the thread until the current job's image is ready to be presented. Directly waiting on
                    // the fence is preferable to submitting a queue semaphore wait because some OS-specific
                    // presentation logic that requires the CPU to know that we can begin executing a present before
                    // preceeding.
                    constexpr uint64 Timeout    = 2000000000;
                    IFence*const     pFence     = pJob->PriorWorkFence();
                    const Result     waitResult = m_pDevice->WaitForFences(1, &pFence, true, Timeout);
                    PAL_ALERT(IsErrorResult(waitResult) || (waitResult == Result::Timeout));
#endif
                    const Result presentResult = ProcessPresent(pJob->GetPresentInfo(), pJob->GetQueue(), false);
                    m_previousPresentResult    = presentResult;
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
