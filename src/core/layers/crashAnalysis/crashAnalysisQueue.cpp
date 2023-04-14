/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/crashAnalysis/crashAnalysis.h"
#include "core/layers/crashAnalysis/crashAnalysisCmdBuffer.h"
#include "core/layers/crashAnalysis/crashAnalysisDevice.h"
#include "core/layers/crashAnalysis/crashAnalysisPlatform.h"
#include "core/layers/crashAnalysis/crashAnalysisQueue.h"
#include "core/layers/crashAnalysis/crashAnalysisEventProvider.h"

#include "palDequeImpl.h"
#include "palVectorImpl.h"
#include "palIntrusiveListImpl.h"

using namespace Util;

namespace Pal
{
namespace CrashAnalysis
{

// =====================================================================================================================
Queue::Queue(
    IQueue* pNextQueue,
    Device* pDevice,
    uint32  queueCount)
    :
    QueueDecorator(pNextQueue, pDevice),
    m_pDevice(pDevice),
    m_queueCount(queueCount),
    m_availableFences(static_cast<Platform*>(pDevice->GetPlatform())),
    m_pendingSubmits(static_cast<Platform*>(pDevice->GetPlatform())),
    m_node(this)
{
}

// =====================================================================================================================
Result Queue::Init(
    const QueueCreateInfo* pCreateInfo)
{
    Result result = Result::Success;

    if (DeviceMembershipNode()->InList() == false)
    {
        m_pDevice->TrackQueue(this);
    }

    return result;
}

// =====================================================================================================================
// Acquires a queue-owned fence.
IFence* Queue::AcquireFence()
{
    IFence* pFence = nullptr;

    if (m_availableFences.NumElements() > 0)
    {
        // Use an idle fence from the pool if available.
        m_availableFences.PopFront(&pFence);
    }
    else
    {
        // No fences are currently idle (or possibly none exist at all) - allocate a new fence.
        void* pMemory = PAL_MALLOC(m_pDevice->GetFenceSize(nullptr), m_pDevice->GetPlatform(), AllocInternal);

        if (pMemory != nullptr)
        {
            Pal::FenceCreateInfo createInfo = {};
            Result result = m_pDevice->CreateFence(createInfo, pMemory, &pFence);
            if (result != Result::Success)
            {
                PAL_SAFE_FREE(pMemory, m_pDevice->GetPlatform());
            }
        }
    }

    if (pFence != nullptr)
    {
        m_pDevice->ResetFences(1, &pFence);
    }

    PAL_ALERT(pFence == nullptr);

    return pFence;
}

// =====================================================================================================================
void Queue::Destroy()
{
    ProcessIdleSubmits();
    PAL_ASSERT(m_pendingSubmits.NumElements() == 0);

    if (DeviceMembershipNode()->InList())
    {
        m_pDevice->UntrackQueue(this);
    }

    while (m_availableFences.NumElements() > 0)
    {
        IFence* pFence = nullptr;
        Result  result = m_availableFences.PopFront(&pFence);

        if (result == Result::Success)
        {
            pFence->Destroy();
            PAL_SAFE_FREE(pFence, m_pDevice->GetPlatform());
        }
    }

    QueueDecorator::Destroy();
}

// =====================================================================================================================
Result Queue::Submit(
    const MultiSubmitInfo& submitInfo)
{
    // Clear out any previous (successful) submissions before proceeding
    ProcessIdleSubmits();

    PendingSubmitInfo pendingInfo = { };
    pendingInfo.pFence            = AcquireFence();
    pendingInfo.pStateList        = PAL_NEW(MarkerStateList, m_pDevice->GetPlatform(), AllocInternal)
                                           (static_cast<Platform*>(m_pDevice->GetPlatform()));
    pendingInfo.pEventList        = PAL_NEW(EventCacheList, m_pDevice->GetPlatform(), AllocInternal)
                                           (static_cast<Platform*>(m_pDevice->GetPlatform()));

    Result result = ((pendingInfo.pFence     != nullptr) &&
                     (pendingInfo.pStateList != nullptr) &&
                     (pendingInfo.pEventList != nullptr))
                  ? Result::Success
                  : Result::ErrorOutOfMemory;

    if (result != Result::Success)
    {
        if (pendingInfo.pFence != nullptr)
        {
            m_availableFences.PushBack(pendingInfo.pFence);
        }

        if (pendingInfo.pStateList != nullptr)
        {
            PAL_SAFE_FREE(pendingInfo.pStateList, m_pDevice->GetPlatform());
        }

        if (pendingInfo.pEventList != nullptr)
        {
            PAL_SAFE_FREE(pendingInfo.pEventList, m_pDevice->GetPlatform());
        }
    }

    // Grab the memory chunk info from all CmdBuffers queued for submission
    if (result == Result::Success)
    {
        for (uint32 i = 0; i < submitInfo.perSubQueueInfoCount; i++)
        {
            const auto& subQueueInfo = submitInfo.pPerSubQueueInfo[i];

            for (uint32 j = 0; j < subQueueInfo.cmdBufferCount; j++)
            {
                CmdBuffer*const pCmdBuffer = static_cast<CmdBuffer*>(subQueueInfo.ppCmdBuffers[j]);

                if (pCmdBuffer != nullptr)
                {
                    result = pendingInfo.pStateList->PushBack(pCmdBuffer->GetMemoryChunk());

                    if (result == Result::Success)
                    {
                        result = pendingInfo.pEventList->PushBack(pCmdBuffer->GetEventCache());
                    }

                    if (result != Result::Success)
                    {
                        break;
                    }
                }
            }
        }
    }

    result = QueueDecorator::Submit(submitInfo);

    if (result == Result::Success)
    {
        result = m_pendingSubmits.PushBack(pendingInfo);
    }

    if (result == Result::Success)
    {
        result = AssociateFenceWithLastSubmit(pendingInfo.pFence);
    }

    return result;
}

// =====================================================================================================================
void Queue::ProcessIdleSubmits()
{
    while ((m_pendingSubmits.NumElements() > 0) && (m_pendingSubmits.Front().pFence->GetStatus() == Result::Success))
    {
        PendingSubmitInfo submitInfo;
        m_pendingSubmits.PopFront(&submitInfo);

        PAL_ASSERT(submitInfo.pFence     != nullptr);
        PAL_ASSERT(submitInfo.pStateList != nullptr);
        PAL_ASSERT(submitInfo.pEventList != nullptr);

        // Release all held memory chunks: they are no longer needed by
        // either the CmdBuffer or Crash Analysis
        while (submitInfo.pStateList->NumElements() > 0)
        {
            MemoryChunk* pChunk;
            submitInfo.pStateList->PopBack(&pChunk);
            pChunk->ReleaseReference();
        }
        PAL_SAFE_DELETE(submitInfo.pStateList, m_pDevice->GetPlatform());

        // Release all held events: since we have not crashed by now, they are no longer
        // needed.
        while (submitInfo.pEventList->NumElements() > 0)
        {
            EventCache* pCache;
            submitInfo.pEventList->PopBack(&pCache);
            pCache->ReleaseReference();
        }
        PAL_SAFE_DELETE(submitInfo.pEventList, m_pDevice->GetPlatform());

        m_availableFences.PushBack(submitInfo.pFence);
    }
}

// =====================================================================================================================
void Queue::LogCrashAnalysisMarkerData() const
{
    CrashAnalysisEventProvider* pProvider =
        static_cast<Platform*>(m_pDevice->GetPlatform())->GetCrashAnalysisEventProvider();

    PAL_ASSERT_MSG((pProvider != nullptr) && (pProvider->IsProviderRegistered()) && (pProvider->IsSessionAcquired()),
                   "CrashAnalysisEventProvider not available: cannot send crash dump data");

    if (pProvider != nullptr)
    {
        // Iterate through all pending submissions...
        for (auto pendingSubmitIter = m_pendingSubmits.Begin();
             pendingSubmitIter.IsValid();
             pendingSubmitIter.Next())
        {
            const PendingSubmitInfo* pSubmission = pendingSubmitIter.Get();

            if ((pSubmission->pEventList == nullptr) || (pSubmission->pStateList == nullptr))
            {
                // These items should not be invalidated yet - if they are, it means
                // GPU execution has continued past the crash point, and possibly while we're in this callback.
                PAL_ASSERT_ALWAYS();
                continue;
            }

            // Sanity check: the number of event cache items should match the number of
            // memory chunks (one per CmdBuffer)
            PAL_ASSERT(pSubmission->pEventList->NumElements() == pSubmission->pStateList->NumElements());

            // Iterate through all CmdBuffers in this submission...
            for (uint32 j = 0; j < pSubmission->pStateList->NumElements(); j++)
            {
                const MemoryChunk* pChunk = pSubmission->pStateList->At(j);
                PAL_ASSERT((pChunk != nullptr) && (pChunk->pCpuAddr != nullptr));

                pProvider->LogCrashDebugMarkerData(pChunk->pCpuAddr);
                pProvider->ReplayEventCache(pSubmission->pEventList->At(j));
            }
        }
    }
}

} // namespace CrashAnalysis
} // namespace Pal

