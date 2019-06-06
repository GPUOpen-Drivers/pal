/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/g_palSettings.h"
#include "core/os/amdgpu/android/androidQueue.h"
#include "core/os/amdgpu/android/androidDevice.h"
#include "core/queueSemaphore.h"

using namespace Util;

namespace Pal
{
namespace Amdgpu
{

// =====================================================================================================================
AndroidQueue::AndroidQueue(
    AndroidDevice*                pDevice,
    const QueueCreateInfo&        createInfo)
    :
    Queue(pDevice, createInfo)
{
}

// =====================================================================================================================
// Export the PalWaitSemaphores to native fence for external wait.
Result AndroidQueue::SignalNativeFence(
    uint32                     waitSemaphoreCount,
    IQueueSemaphore**          pPalWaitSemaphores,
    int*                       pNativeFenceFd)
{
    Result result = Result::Success;
    // For SemaphoreType::SyncObj semaphore, amdgpu_semaphore_handle is actually a syncobj handle.
    amdgpu_semaphore_handle hSyncObj = nullptr;

    if (waitSemaphoreCount > 0)
    {
        // append pPalWaitSemaphores to Queue's m_waitSemList
        for (uint32 i = 0; i < waitSemaphoreCount; i++)
        {
            hSyncObj = static_cast<Pal::QueueSemaphore*>(pPalWaitSemaphores[i])->GetSyncObjHandle();
            result = WaitSemaphore(hSyncObj, 0);
        }

        //constuct dummy command and convert Queue's m_lastSignaledSyncObj status to syncObj
        //SignalSemaphore will issue dummy command in case there is pending wait
        if (result == Result::Success)
        {
            result = SignalSemaphore(hSyncObj, 0);
        }

        //export the syncObj as file descriptor
        if (result == Result::Success)
        {
            result = static_cast<Device*>(m_pDevice)->SyncObjExportSyncFile(
                                              reinterpret_cast<uintptr_t>(hSyncObj),
                                              pNativeFenceFd);
        }
    }
    else
    {
        *pNativeFenceFd = InvalidFd;
    }

    return result;
}

} // Amdgpu
} // Pal
