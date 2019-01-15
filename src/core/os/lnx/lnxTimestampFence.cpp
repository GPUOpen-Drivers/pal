/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "lnxTimestampFence.h"

using namespace Util;

namespace Pal
{
namespace Linux
{

// =====================================================================================================================
TimestampFence::TimestampFence()
    :
    m_pContext(nullptr),
    m_timestamp(0)
{
}

// =====================================================================================================================
Result TimestampFence::Init(
    const FenceCreateInfo& createInfo)
{
    // Nothing needs to be done on amdgpu.
    m_fenceState.initialSignalState = createInfo.flags.signaled ? 1 : 0;

    return Result::Success;
}

// =====================================================================================================================
TimestampFence::~TimestampFence()
{
    if (m_pContext != nullptr)
    {
        m_pContext->ReleaseReference();
        m_pContext = nullptr;
    }
}

// =====================================================================================================================
// Probes the status of the Queue submission which this Fence is associated with.
// NOTE: Part of the public IFence interface.
Result TimestampFence::GetStatus() const
{
    Result result = Result::ErrorFenceNeverSubmitted;

    // We should only check the InitialState when the fence has never been submitted by the client
    if (WasNeverSubmitted() && InitialState())
    {
        result = Result::Success;
    }
    // If a Fence is not associated with a submission context, the Fence status is considered unavailable (which
    // implies neither retired nor busy).
    else if (m_pContext != nullptr)
    {
        // We must report NotReady if this fence's submission has been batched or is not retired.
        if (IsBatched() || (m_pContext->IsTimestampRetired(m_timestamp) == false))
        {
            result = Result::NotReady;
        }
        else
        {
            result = Result::Success;
        }
    }

    return result;
}

// =====================================================================================================================
// Associates this Fence with a submission context. When a Queue submission is being prepared (or batched-up) this is
// done to tie the Fence with the appropriate context.
void TimestampFence::AssociateWithContext(
    Pal::SubmissionContext* pContext)
{
    // Note that it's legal to associate a fence with a new context without first resetting the fence. We expect to see
    // this behavior if the client is using IQueue::AssociateFenceWithLastSubmit.
    if (m_pContext != nullptr)
    {
        m_pContext->ReleaseReference();
    }

    PAL_ASSERT(pContext != nullptr);

    m_pContext = static_cast<Pal::Linux::SubmissionContext*>(pContext);
    m_pContext->TakeReference();

    // Note that we don't need to atomically modify m_timestamp here because this function is called during Submit()
    // and it isn't legal to poll a fence's status until after Submit() returns.
    m_timestamp = BatchedTimestamp;

    m_fenceState.neverSubmitted = 0;
}

// =====================================================================================================================
// Associate with the submission context's last timestamp.
Result TimestampFence::AssociateWithLastTimestamp()
{
    PAL_ASSERT(m_pContext != nullptr);

    // Atomically modify the timestamp because another thread could be polling GetStatus() in the background while
    // we're unrolling a batched submission or timestamp association.
    AtomicExchange64(&m_timestamp, m_pContext->LastTimestamp());

    return Result::Success;
}

// =====================================================================================================================
// Resets this Fence to a state where it is no longer associated with a Queue submission. GetStatus() calls on this
// Fence will fail eith 'ErrorUnavailable' until the object is associated with a new submission.
Result TimestampFence::Reset()
{
    Result result = Result::Success;

    if (m_pContext != nullptr)
    {
        m_pContext->ReleaseReference();
        m_pContext = nullptr;
    }

    // The fence is no longer associated with any submissions.
    m_timestamp = 0;

    // If this is called before a submission, the private screen present usage flag needs to reset as well.
    m_fenceState.privateScreenPresentUsed = 0;

    // the initial signal state should be reset to false even though it is created as signaled at the first place.
    m_fenceState.initialSignalState = 0;

    return result;
}

// =====================================================================================================================
// Waits for one or more Fence objects to be processed by the GPU.  If waitAll is set, then this waits for all Fence
// objects to be processed.  Otherwise, this only waits for at least one Fence object to be processed.
//
// NOTE: On Legacy Linux, we don't have any KMD-signaled completion Event when command buffers finish, so we have no
// way to truly multiplex the set of Fences in the non-waitAll case.  This means that the best approximation we can make
// is to poll until we discover that some Fence(s) in the set have finished.
Result TimestampFence::WaitForFences(
    const Pal::Device&      device,
    uint32                  fenceCount,
    const Pal::Fence*const* ppFenceList,
    bool                    waitAll,
    uint64                  timeout) const
{
    PAL_ASSERT((fenceCount > 0) && (ppFenceList != nullptr));

    Result result = Result::ErrorOutOfMemory;

    const TimestampFence*const* ppLnxFenceList = reinterpret_cast<const TimestampFence*const*>(ppFenceList);
    const Device& lnxDevice                    = reinterpret_cast<const Device&>(device);

    AutoBuffer<amdgpu_cs_fence, 16, Platform> fenceList(fenceCount, lnxDevice.GetPlatform());

    uint32 count = 0;

    if (fenceList.Capacity() >= fenceCount)
    {
        result = Result::NotReady;

        for (uint32 fence = 0; fence < fenceCount; ++fence)
        {
            if (ppLnxFenceList[fence] == nullptr)
            {
                result = Result::ErrorInvalidPointer;
                break;
            }
            // linux heavily rely on submission to have a right fence to wait for.
            // If it is created as signaled, we'd better to skip this fence directly.
            else if (ppLnxFenceList[fence]->InitialState())
            {
                if (waitAll == true)
                {
                    continue;
                }
                else
                {
                    result = Result::Success;
                    break;
                }
            }
            else if (ppLnxFenceList[fence]->WasNeverSubmitted())
            {
                result = Result::ErrorFenceNeverSubmitted;
                break;
            }

            const auto*const pContext = ppLnxFenceList[fence]->m_pContext;

            if (pContext == nullptr)
            {
                // If the fence is not associated with a submission context, return unavailable.
                result = Result::ErrorUnavailable;
                break;
            }

            // We currently have no way to wait for a batched fence on Linux. This is OK for now because Vulkan (the
            // only Linux client) doesn't permit the application to trigger queue batching. A solution must be found
            // once PAL swap chain presents have been refactored because they will trigger batching internally.
            PAL_ASSERT(ppLnxFenceList[fence]->IsBatched() == false);

            fenceList[count].context = pContext->Handle();
            fenceList[count].ip_type = pContext->IpType();
            fenceList[count].ip_instance = 0;
            fenceList[count].ring = pContext->EngineId();
            fenceList[count].fence = ppLnxFenceList[fence]->Timestamp();
            count++;
        }
    }

    if (result == Result::NotReady)
    {
        struct timespec startTime = {};
        struct timespec stopTime = {};
        uint64 timeoutLeft = 0;

        ComputeTimeoutExpiration(&startTime, 0);
        ComputeTimeoutExpiration(&stopTime, timeout);

        if (count > 0)
        {
            result = static_cast<const Linux::Device*>(&device)->WaitForFences(&fenceList[0],
                count,
                waitAll,
                timeout);
        }
        else
        {
            result = Result::Success;
        }
    }

    // return Timeout in failed scenario no matter whether timeout is 0.
    if (result == Result::NotReady)
    {
        result = Result::Timeout;
    }

    return result;
}

} // Linux
} // Pal
