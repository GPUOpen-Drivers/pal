/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/queueSemaphore.h"
#include "palAssert.h"

namespace Pal
{

// =====================================================================================================================
QueueSemaphore::QueueSemaphore(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_maxWaitsPerSignal(pDevice->MaxQueueSemaphoreCount())
#if PAL_AMDGPU_BUILD
    ,m_hSemaphore(nullptr),
    m_skipNextWait(false)
#endif
{
    m_flags.u32All = 0;
}

// =====================================================================================================================
// Performs validation on the semaphore's creation info.  Child classes should call this method during their own
// initialization.
Result QueueSemaphore::ValidateInit(
    const Device*                   pDevice,
    const QueueSemaphoreCreateInfo& createInfo)
{
    Result result = Result::ErrorInvalidValue;

    // maxCount does not apply to timeline semaphores.
    if (createInfo.flags.timeline ||
        ((createInfo.maxCount > 0)                                  &&
         (createInfo.maxCount <= pDevice->MaxQueueSemaphoreCount()) &&
         (createInfo.initialCount <= createInfo.maxCount)))
    {
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Performs validation on the semaphore's open info.  Child classes should call this method during their own
// initialization.
Result QueueSemaphore::ValidateOpen(
    const Device*                 pDevice,
    const QueueSemaphoreOpenInfo& openInfo)
{
    const QueueSemaphore*const pSharedSema = static_cast<const QueueSemaphore*>(openInfo.pSharedQueueSemaphore);

    Result result = Result::ErrorInvalidPointer;

    if (pSharedSema != nullptr)
    {
        const Device* pOtherDevice = pSharedSema->m_pDevice;

        // NOTE: It is only legal to share a Semaphore if the pair of GPU's it will be shared between supports shared
        // synchronization primitives.
        GpuCompatibilityInfo compatInfo = { };
        result = pDevice->GetMultiGpuCompatibility(*pOtherDevice, &compatInfo);
        PAL_ALERT(result != Result::Success);

        if (compatInfo.flags.sharedSync == 0)
        {
            result = Result::ErrorIncompatibleDevice;
        }
    }

    return result;
}

// =====================================================================================================================
// Destroys this Queue Semaphore object. Clients are responsible for freeing the system memory the object occupies.
// NOTE: Part of the public IDestroyable interface.
void QueueSemaphore::Destroy()
{
    PAL_ASSERT(HasStalledQueues() == false);
    this->~QueueSemaphore();
}

} // Pal
