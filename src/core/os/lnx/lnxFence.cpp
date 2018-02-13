/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

// =====================================================================================================================
Result Fence::Init(
    const FenceCreateInfo& createInfo,
    bool                   needsEvent)
{
    // Nothing needs to be done on amdgpu.
    m_fenceState.initialSignalState = createInfo.flags.signaled ? 1 : 0;

    return Result::Success;
}

// =====================================================================================================================
// Waits for one or more Fence objects to be processed by the GPU.  If waitAll is set, then this waits for all Fence
// objects to be processed.  Otherwise, this only waits for at least one Fence object to be processed.
//
// NOTE: On Linux, we don't have any KMD-signaled completion Event when command buffers finish, so we have no way to
// truly multiplex the set of Fences in the non-waitAll case.  This means that the best approximation we can make is
// to poll until we discover that some Fence(s) in the set have finished.
Result Fence::WaitForFences(
    const Device&      device,
    uint32             fenceCount,
    const Fence*const* ppFenceList,
    bool               waitAll,
    uint64             timeout
    ) const
{
    PAL_ASSERT((fenceCount > 0) && (ppFenceList != nullptr));

    Result result = Result::ErrorOutOfMemory;

    AutoBuffer<amdgpu_cs_fence, 16, Pal::Platform> fenceList(fenceCount, device.GetPlatform());

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

            const auto*const pContext = static_cast<Linux::SubmissionContext*>(ppFenceList[fence]->m_pContext);

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

            fenceList[count].context     = pContext->Handle();
            fenceList[count].ip_type     = pContext->IpType();
            fenceList[count].ip_instance = 0;
            fenceList[count].ring        = pContext->EngineId();
            fenceList[count].fence       = ppFenceList[fence]->Timestamp();
            count++;
        }
    }

    if (result == Result::NotReady)
    {
        struct timespec startTime   = {};
        struct timespec stopTime    = {};
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

// =====================================================================================================================
Result Fence::OpenHandle(
    const FenceOpenInfo& openInfo)
{
    return Result::Unsupported;
}

} // Pal
