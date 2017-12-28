/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/fence.h"
#include "core/platform.h"
#include "core/queue.h"
#include "palMutex.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
Fence::Fence()
    :
    m_pContext(nullptr),
    m_timestamp(0)
{
    m_fenceState.flags = 0;
    m_fenceState.neverSubmitted = 1;
}

// =====================================================================================================================
Fence::~Fence()
{
    if (m_pContext != nullptr)
    {
        m_pContext->ReleaseReference();
        m_pContext = nullptr;
    }
}

// =====================================================================================================================
// Destroys this Fence object. Clients are responsible for freeing the system memory the object occupies.
// NOTE: Part of the public IDestroyable interface.
void Fence::Destroy()
{
    this->~Fence();
}

// =====================================================================================================================
// Destroys an internal fence object: invokes the destructor and frees the system memory block it resides in.
void Fence::DestroyInternal(
    Platform* pPlatform)
{
    Destroy();
    PAL_FREE(this, pPlatform);
}

// =====================================================================================================================
// Probes the status of the Queue submission which this Fence is associated with.
// NOTE: Part of the public IFence interface.
Result Fence::GetStatus() const
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
void Fence::AssociateWithContext(
    SubmissionContext* pContext)
{
    // Note that it's legal to associate a fence with a new context without first resetting the fence. We expect to see
    // this behavior if the client is using IQueue::AssociateFenceWithLastSubmit.
    if (m_pContext != nullptr)
    {
        m_pContext->ReleaseReference();
    }

    PAL_ASSERT(pContext != nullptr);

    m_pContext = pContext;
    m_pContext->TakeReference();

    // Note that we don't need to atomically modify m_timestamp here because this function is called during Submit()
    // and it isn't legal to poll a fence's status until after Submit() returns.
    m_timestamp = BatchedTimestamp;

    m_fenceState.neverSubmitted = 0;
}

// =====================================================================================================================
// Associate with the submission context's last timestamp.
void Fence::AssociateWithLastTimestamp()
{
    PAL_ASSERT(m_pContext != nullptr);

    // Atomically modify the timestamp because another thread could be polling GetStatus() in the background while
    // we're unrolling a batched submission or timestamp association.
    AtomicExchange64(&m_timestamp, m_pContext->LastTimestamp());
}

// =====================================================================================================================
// Resets this Fence to a state where it is no longer associated with a Queue submission. GetStatus() calls on this
// Fence will fail eith 'ErrorUnavailable' until the object is associated with a new submission.
void Fence::ResetAssociatedSubmission()
{
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

}

} // Pal
