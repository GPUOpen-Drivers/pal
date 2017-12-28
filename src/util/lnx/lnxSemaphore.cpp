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

#include "palSemaphore.h"
#include "palSysMemory.h"
#include "util/lnx/lnxTimeout.h"
#include <errno.h>

namespace Util
{

// =====================================================================================================================
Semaphore::~Semaphore()
{
    sem_destroy(&m_osSemaphore);
}

// =====================================================================================================================
// Initializes the POSIX semaphore this object encapsulates.
Result Semaphore::Init(
    uint32 maximumCount,
    uint32 initialCount)
{
    Result result = Result::ErrorInvalidValue;

    if ((initialCount <= maximumCount) && (maximumCount <= MaximumCountLimit))
    {
        const int ret = sem_init(&m_osSemaphore, 0, initialCount);
        result        = (ret == 0) ? Result::Success : Result::ErrorInitializationFailed;
    }

    return result;
}

// =====================================================================================================================
// Waits until the semaphore's count is nonzero, then decrease the count by one.
Result Semaphore::Wait(
    uint32 milliseconds)  // Milliseconds to sleep before timing-out.
{
    constexpr uint32 Infinite = 0xFFFFFFFF;

    int result = 0;
    switch (milliseconds)
    {
    case Infinite:
        // Wait on the semaphore indefinitely.
        result = sem_wait(&m_osSemaphore);
        break;

    case 0:
        // Decrement the semaphore if it can be done so immediately, but don't wait.
        result = sem_trywait(&m_osSemaphore);
        break;

    default:
        {
            timespec timeout = { };
            ComputeTimeoutExpiration(&timeout, milliseconds * 1000 * 1000);

            // Wait on the semaphore until a timeout occurs.
            result = sem_timedwait(&m_osSemaphore, &timeout);
        }
        break;
    }

    if (result == -1)
    {
        result = errno;
    }

    // EAGAIN and ETIMEDOUT are nonfatal if we're not waiting indefinitely.
    PAL_ASSERT((result == 0) || (result == EAGAIN) || (result == ETIMEDOUT));
    return ((result == 0) ? Result::Success : Result::Timeout);
}

// =====================================================================================================================
// Increases the semaphore's count by the given amount.  There is no facility to increment a POSIX semaphore's count by
// more than one atomically: the only way is to call sem_post(..) in a loop.
void Semaphore::Post(
    uint32 postCount)
{
    for (uint32 n = 0; n < postCount; ++n)
    {
        const int32 ret = sem_post(&m_osSemaphore);
        PAL_ASSERT(ret == 0);
    }
}

} // Util
