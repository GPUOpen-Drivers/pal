/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/os/nullDevice/ndDevice.h"
#include "core/os/nullDevice/ndQueue.h"
#include "core/os/nullDevice/ndFence.h"

using namespace Util;

namespace Pal
{
namespace NullDevice
{

// =====================================================================================================================
Result SubmissionContext::Create(
    Pal::Platform*           pPlatform,
    Pal::SubmissionContext** ppContext)
{
    Result     result  = Result::ErrorOutOfMemory;
    void*const pMemory = PAL_MALLOC(sizeof(SubmissionContext), pPlatform, AllocInternal);

    if (pMemory != nullptr)
    {
        *ppContext = PAL_PLACEMENT_NEW(pMemory) SubmissionContext(pPlatform);
    }

    return result;
}

// =====================================================================================================================
Queue::Queue(
    Device*                pDevice,
    const QueueCreateInfo& createInfo)
    :
    Pal::Queue(pDevice, createInfo)
{
}

// =====================================================================================================================
Result Queue::Init(
    void* pContextPlacementAddr)
{
    return Result::Success;
}

// =====================================================================================================================
// Copies page mappings for one or more virtual GPU memory allocations.  The NULL device doesn't have any page
// mappings to copy.
Result Queue::CopyVirtualMemoryPageMappings(
    uint32                                    rangeCount,
    const VirtualMemoryCopyPageMappingsRange* pRangeList,
    bool                                      doNotWait)
{
    return Result::Success;
}

// =====================================================================================================================
// Executes a direct present operation immediately, without any batching.
Result Queue::OsPresentDirect(
    const PresentDirectInfo& presentInfo)
{
    // We indicate via GetPresentSupport() that there is no present support, so this shouldn't get called.
    PAL_NEVER_CALLED();

    return Result::Unsupported;
}

// =====================================================================================================================
// We don't have hardware to submit to, so this is easy.  Do nothing.
Result Queue::OsSubmit(
    const SubmitInfo&         submitInfo,
    const InternalSubmitInfo& internalSubmitInfo)
{
    return Result::Success;
}

// =====================================================================================================================
// Nothing was submitted, so the null device is always idle.
Result Queue::OsWaitIdle()
{
    return Result::Success;
}

// =====================================================================================================================
// Updates page mappings for one or more virtual GPU memory allocations.  But we don't have any page tables to bother
// updating.
Result Queue::RemapVirtualMemoryPages(
    uint32                         rangeCount,
    const VirtualMemoryRemapRange* pRangeList,
    bool                           doNotWait,
    IFence*                        pFence)
{
    return Result::Success;
}

// =====================================================================================================================
Result Queue::DoAssociateFenceWithLastSubmit(
    Pal::Fence* pFence)
{
    return Result::Success;
}

} // NullDevice
} // Pal
