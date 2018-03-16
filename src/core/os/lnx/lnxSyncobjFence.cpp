 /*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/os/lnx/lnxSyncobjFence.h"
#include "core/os/lnx/lnxQueue.h"
#include "core/os/lnx/lnxDevice.h"
#include "core/os/lnx/lnxPlatform.h"
#include "util/lnx/lnxTimeout.h"
#include "palInlineFuncs.h"
#include "palMutex.h"
#include "palAutoBuffer.h"

#include <time.h>

using namespace Util;

namespace Pal
{
namespace Linux
{

// =====================================================================================================================
SyncobjFence::SyncobjFence(
    const Device&    device)
    :
    m_fenceSyncObject(0),
    m_device(device)
{
}

// =====================================================================================================================
SyncobjFence::~SyncobjFence()
{
    Result result = Result::Success;

    result = m_device.DestroySyncObject(m_fenceSyncObject);
    PAL_ASSERT(result == Result::Success);
}

// =====================================================================================================================
Result SyncobjFence::Init(
    const FenceCreateInfo& createInfo,
    bool                   needsEvent)
{
    Result result = Result::Success;

    result = Fence::Init(createInfo, needsEvent);
    if (result == Result::Success)
    {
        result = m_device.CreateSyncObject(0, &m_fenceSyncObject);
    }

    return result;
}

// =====================================================================================================================
// Waits for one or more SyncobjFence objects to be processed by the GPU.  If waitAll is set, then this waits for all
// SyncobjFence objects to be processed.  Otherwise, this only waits for at least one SyncobjFence object
// to be processed.
//
// NOTE: On Linux, we don't have any KMD-signaled completion Event when command buffers finish, so we have no way to
// truly multiplex the set of Fences in the non-waitAll case.  This means that the best approximation we can make is
// to poll until we discover that some SyncobjFence(s) in the set have finished.
Result SyncobjFence::WaitForFences(
    const Pal::Device& device,
    uint32             fenceCount,
    const Fence*const* ppFenceList,
    bool               waitAll,
    uint64             timeout
    ) const
{
    PAL_ASSERT((fenceCount > 0) && (ppFenceList != nullptr));

    Result result = Result::ErrorOutOfMemory;

    AutoBuffer<amdgpu_syncobj_handle, 16, Pal::Platform> fenceList(fenceCount, device.GetPlatform());

    uint32 count = 0;

    if (fenceList.Capacity() >= fenceCount)
    {
        result = Result::NotReady;

        for (uint32 fence = 0; fence < fenceCount; ++fence)
        {
            if (ppFenceList[fence] == nullptr)
            {
                result = Result::ErrorInvalidPointer;
                break;
            }
            // linux heavily rely on submission to have a right fence to wait for.
            // If it is created as signaled, we'd better to skip this fence directly.
            else if (ppFenceList[fence]->InitialState())
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
            else if (ppFenceList[fence]->WasNeverSubmitted())
            {
                result = Result::ErrorFenceNeverSubmitted;
                break;
            }

            const auto*const pSyncobjFence = static_cast<const SyncobjFence*>(ppFenceList[fence]);
            const auto*const pContext = static_cast<Linux::SubmissionContext*>(pSyncobjFence->m_pContext);

            if (pContext == nullptr)
            {
                // If the fence is not associated with a submission context, return unavailable.
                result = Result::ErrorUnavailable;
                break;
            }

            // We currently have no way to wait for a batched fence on Linux. This is OK for now because Vulkan (the
            // only Linux client) doesn't permit the application to trigger queue batching. A solution must be found
            // once PAL swap chain presents have been refactored because they will trigger batching internally.
            PAL_ASSERT(ppFenceList[fence]->IsBatched() == false);

            fenceList[count] = pSyncobjFence->m_fenceSyncObject;
            count++;
        }
    }

    if (result == Result::NotReady)
    {
        struct timespec startTime = {};
        uint64 currentTimeNs = 0;
        uint64 absTimeoutNs = 0;
        uint32 firstSignaledFence = UINT32_MAX;
        uint32 flags= 0;

        ComputeTimeoutExpiration(&startTime, 0);
        currentTimeNs = startTime.tv_sec * 1000000000ull + startTime.tv_nsec;
        timeout = Util::Min(UINT64_MAX - currentTimeNs, timeout);
        absTimeoutNs = currentTimeNs + timeout;

        // definition of drm_timeout_abs_to_jiffies (int64_t timeout_nsec) require input to be int64_t,
        // so trim down the max value to be INT64_MAX, otherwise drm_timeout_abs_to_jiffies compute wrong output.
        absTimeoutNs= Util::Min(absTimeoutNs, (uint64)INT64_MAX);

        if (waitAll)
        {
            flags |= DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL;
        }

        if (count > 0)
        {
            result = m_device.WaitForSyncobjFences(&fenceList[0],
                                                   count,
                                                   absTimeoutNs,
                                                   flags,
                                                   &firstSignaledFence);
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

// =====================================================================================================================
// For SyncObj based Fence, import the external fence by dereferencing the sync file descriptor.
Result SyncobjFence::OpenHandle(
    const FenceOpenInfo& openInfo)
{
    Result result = Result::Success;

    result = m_device.SyncObjImportSyncFile(openInfo.externalFence, m_fenceSyncObject);
    return result;
}

// =====================================================================================================================
// Associate with the Queue's lastSignaledSyncObject.
Result SyncobjFence::AssociateWithLastTimestampOrSyncobj()
{
    Result result = Result::Success;
    const auto*const pLnxSubmissionContext = static_cast<Linux::SubmissionContext*>(m_pContext);

    result = Fence::AssociateWithLastTimestampOrSyncobj();
    if (result == Result::Success)
    {
        result = m_device.ConveySyncObjectState(m_fenceSyncObject,
                                                pLnxSubmissionContext->GetLastSignaledSyncObj());
    }
    return result;
}

// =====================================================================================================================
// Resets this Fence to a state where it is no longer associated with a Queue submission. GetStatus() calls on this
// Fence will fail eith 'ErrorUnavailable' until the object is associated with a new submission.
Result SyncobjFence::ResetAssociatedSubmission()
{
    Result result = Result::Success;

    result = Fence::ResetAssociatedSubmission();
    if (result == Result::Success)
    {
        result = m_device.ResetSyncObject(&m_fenceSyncObject, 1);
    }
    return result;
}

// =====================================================================================================================
// use WaitForSyncobjFences with setting timeout = 0
bool SyncobjFence::IsSyncobjSignaled(
    amdgpu_syncobj_handle    syncObj
    ) const
{
    Result result = Result::Success;
    bool ret = false;
    uint32 count = 1;
    uint64 timeout = 0;
    uint32 flags = DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL;
    uint32 firstSignaledFence = UINT32_MAX;

    result = m_device.WaitForSyncobjFences(&syncObj,
                                           count,
                                           timeout,
                                           flags,
                                           &firstSignaledFence);
    if ((result == Result::Success) && (firstSignaledFence == 0))
    {
        ret = true;
    }

    return ret;
}

// =====================================================================================================================
// Probes the status of the Queue submission which this Fence is associated with.
// NOTE: Part of the public IFence interface.
Result SyncobjFence::GetStatus() const
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
        if (IsBatched() || (IsSyncobjSignaled(m_fenceSyncObject) == false))
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

} // Linux
} // Pal
