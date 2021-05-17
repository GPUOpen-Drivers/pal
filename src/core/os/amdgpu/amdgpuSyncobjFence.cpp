/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuPlatform.h"
#include "core/os/amdgpu/amdgpuQueue.h"
#include "core/os/amdgpu/amdgpuSyncobjFence.h"
#include "util/lnx/lnxTimeout.h"
#include "palInlineFuncs.h"
#include "palMutex.h"
#include "palAutoBuffer.h"

#include <time.h>

using namespace Util;

namespace Pal
{
namespace Amdgpu
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
    const FenceCreateInfo& createInfo)
{
    Result result = Result::Success;
    m_fenceState.initialSignalState = createInfo.flags.signaled ? 1 : 0;
    if (result == Result::Success)
    {
        uint32 flags = 0;
        if (createInfo.flags.signaled)
        {
            flags |= DRM_SYNCOBJ_CREATE_SIGNALED;
        }

        result = m_device.CreateSyncObject(flags, &m_fenceSyncObject);
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
    const Pal::Device&      device,
    uint32                  fenceCount,
    const Pal::Fence*const* ppFenceList,
    bool                    waitAll,
    uint64                  timeout
    ) const
{
    PAL_ASSERT((fenceCount > 0) && (ppFenceList != nullptr));

    Result result = Result::ErrorOutOfMemory;

    AutoBuffer<amdgpu_syncobj_handle, 16, Pal::Platform> fenceList(fenceCount, device.GetPlatform());

    uint32 count = 0;
    bool   isNeverSubmitted = false;

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
            else if (ppFenceList[fence]->WasNeverSubmitted())
            {
                isNeverSubmitted = true;
            }

            const auto*const pSyncobjFence = static_cast<const SyncobjFence*>(ppFenceList[fence]);

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

        //fix even if the syncobj's submit is still in m_batchedCmds.
        flags |= DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT;
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

    // For Fence never submitted, fence wait return success if it shares the payload with another signaled fence;
    // for other cases, return Timeout.
    if (isNeverSubmitted && (result != Result::Success))
    {
        result = Result::Timeout;
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

    if (openInfo.flags.isReference)
    {
        result = m_device.ImportSyncObject(openInfo.externalFence, &m_fenceSyncObject);
    }
    else
    {
        result = m_device.CreateSyncObject(0, &m_fenceSyncObject);
        if (result == Result::Success)
        {
            result = m_device.SyncObjImportSyncFile(openInfo.externalFence, m_fenceSyncObject);
        }
        if (result == Result::Success)
        {
            close(openInfo.externalFence);
        }
    }

    // For external fence, set the external opened flag.
    m_fenceState.isOpened = 1;

    return result;
}

// =====================================================================================================================
// Export the sync object handle of SyncobjFence.
OsExternalHandle SyncobjFence::ExportExternalHandle(
    const FenceExportInfo& exportInfo) const
{
    OsExternalHandle handle = InvalidFd;

    if (exportInfo.flags.isReference)
    {
        handle = m_device.ExportSyncObject(m_fenceSyncObject);
    }
    else
    {
        Result result = m_device.SyncObjExportSyncFile(m_fenceSyncObject, reinterpret_cast<int32*>(&handle));

        if ((result == Result::Success) && (exportInfo.flags.implicitReset))
        {
            m_device.ResetSyncObject(&m_fenceSyncObject, 1);
        }
    }

    return handle;
}

// =====================================================================================================================
void SyncobjFence::AssociateWithContext(
    Pal::SubmissionContext* pContext)
{
    m_fenceState.neverSubmitted = 0;
}

// =====================================================================================================================
// Resets this Fence to a state where it is no longer associated with a Queue submission. GetStatus() calls on this
// Fence will fail eith 'ErrorUnavailable' until the object is associated with a new submission.
Result SyncobjFence::Reset()
{
    Result result = Result::Success;

    // If this is called before a submission, the private screen present usage flag needs to reset as well.
    m_fenceState.privateScreenPresentUsed = 0;

    // the initial signal state should be reset to false even though it is created as signaled at the first place.
    m_fenceState.initialSignalState = 0;

    result = m_device.ResetSyncObject(&m_fenceSyncObject, 1);

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
    Result result = Result::Success;
    // In the previous version of GetStatus(), there are two scenarios we can reach ErrorFenceNeverSubmitted status.
    // One is WasNeverSubmitted() && !InitialState(). The other is !WasNeverSubmitted()&&pContext==nullptr.
    // Since we don't track submission context anymore, there's no way we can reproduce the second scenario.
    // Thus, this version of GetStatus() is not equivalent to the old one exactly.
    // ErrorFenceNeverSubmitted is not reported correctly here.
    // After we start removing ErrorFenceNeverSubmitted in another changelist, I will remove the second the if block.
    if ((IsSyncobjSignaled(m_fenceSyncObject) == false) && (WasNeverSubmitted() == false))
    {
        result = Result::NotReady;
    }
    else if ((IsSyncobjSignaled(m_fenceSyncObject) == false) && (WasNeverSubmitted() == true))
    {
        result = Result::ErrorFenceNeverSubmitted;
    }
    else
    {
        result = Result::Success;
    }

    return result;
}

} // Amdgpu
} // Pal
