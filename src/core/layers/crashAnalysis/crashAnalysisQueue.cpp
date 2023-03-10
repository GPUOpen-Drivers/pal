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

    Result result = (pendingInfo.pFence != nullptr)
                    ? Result::Success
                    : Result::ErrorOutOfMemory;

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

        // Release all held memory chunks: they are no longer needed by
        // either the CmdBuffer or Crash Analysis
        while (submitInfo.pStateList->NumElements() > 0)
        {
            MemoryChunk chunk;
            submitInfo.pStateList->PopBack(&chunk);
            m_pDevice->ReleaseMemoryChunk(&chunk);
        }

        PAL_SAFE_DELETE(submitInfo.pStateList, m_pDevice->GetPlatform());

        m_availableFences.PushBack(submitInfo.pFence);
    }
}

// =====================================================================================================================
void Queue::LogCrashAnalysisMarkerData() const
{
    CrashAnalysisEventProvider* pProvider =
        static_cast<Platform*>(m_pDevice->GetPlatform())->GetCrashAnalysisEventProvider();

    PAL_ASSERT_MSG(pProvider != nullptr,
                   "CrashAnalysisEventProvider not available: cannot send crash dump data");

    if (pProvider != nullptr)
    {
        for (uint32 i = 0; i < m_pendingSubmits.NumElements(); i++)
        {
            auto it = m_pendingSubmits[i].pStateList->Begin();
            while (it.IsValid())
            {
                pProvider->LogCrashDebugMarkerData(it.Get().pCpuAddr);
                it.Next();
            }
        }
    }
}

} // namespace CrashAnalysis
} // namespace Pal

