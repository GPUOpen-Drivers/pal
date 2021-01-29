/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palAssert.h"
#include "util/lnx/lnxTimeout.h"

#include <cstring>
#include <time.h>

#include "palInlineFuncs.h"

namespace Util
{

constexpr int32  NanoSecPerSec = 1000 * 1000 * 1000;

// =====================================================================================================================
// On Linux, many of the thread wait functions require that a timeout for the wait be specified in terms of "absolute
// time at which the timeout should expire" rather than "how long to wait".
//
// This helper computes the absolute time at which a timeout of duration 'nanoseconds' will expire.
void ComputeTimeoutExpiration(
    timespec* pAbsTimeout,   // [out] Absolute time the timeout should expire.
    uint64    nanoseconds)   // Timeout duration in us.
{
    // Compute the number of complete seconds specified in the timeout duration.
    const uint64 wholeSeconds = nanoseconds / NanoSecPerSec;

    // Compute the number of remaining nanoseconds after accounting for the whole seconds.
    const uint64 remaniningNanoseconds  = nanoseconds % NanoSecPerSec;

    // Query the system's monotonic clock for the current time.
    if (clock_gettime(CLOCK_MONOTONIC, pAbsTimeout) == 0)
    {
        // Add the timeout duration to the current time.
        pAbsTimeout->tv_sec  += wholeSeconds;
        pAbsTimeout->tv_nsec += remaniningNanoseconds;

        // Carry a second if the summed nanoseconds count exceeds a whole second.
        if (pAbsTimeout->tv_nsec >= NanoSecPerSec)
        {
            pAbsTimeout->tv_nsec -= NanoSecPerSec;
            ++pAbsTimeout->tv_sec;
        }
    }
    else
    {
        // Something has gone seriously wrong... zero-out the timeout expiration so we won't be waiting for some
        // unknown/random duration (note that since the waits are until an absolute time, this will effectively not wait
        // at all).
        memset(pAbsTimeout, 0, sizeof(timespec));
        PAL_ASSERT_ALWAYS();
    }
}

// =====================================================================================================================
// On Linux, many of the thread wait functions require that a timeout for the wait be specified in terms of "absolute
// time at which the timeout should expire" rather than "how long to wait".
//
// This helper computes the absolute timeoutNs according timeout
int64 ComputeAbsTimeout(
    uint64 timeout)
{
    struct timespec startTime = {};

    uint64 currentTimeNs = 0;
    uint64 absTimeoutNs  = 0;

    ComputeTimeoutExpiration(&startTime, 0);
    currentTimeNs = startTime.tv_sec * 1000000000ull + startTime.tv_nsec;
    timeout       = Util::Min(UINT64_MAX - currentTimeNs, timeout);
    absTimeoutNs  = currentTimeNs + timeout;

    // definition of drm_timeout_abs_to_jiffies (int64_t timeout_nsec) require input to be int64_t,
    // so trim down the max value to be INT64_MAX, otherwise drm_timeout_abs_to_jiffies compute wrong output.
    return Util::Min(absTimeoutNs, static_cast<uint64>INT64_MAX);
}

// =====================================================================================================================
// Checks if the specified timeout has expired.  The timeout is specified in terms of "absolute time at which the
// timeout should expire", so this function simply checks what the current time is, and compares that against the
// specified timeout value.
bool IsTimeoutExpired(
    const timespec* pAbsTimeout)  // [in] Absolute time the timeout should expire.
{
    bool expired = false;

    timespec currentTime = {};
    if (clock_gettime(CLOCK_MONOTONIC, &currentTime) == 0)
    {
        if ((currentTime.tv_sec > pAbsTimeout->tv_sec) ||
            ((currentTime.tv_sec  == pAbsTimeout->tv_sec) && (currentTime.tv_nsec >= pAbsTimeout->tv_nsec)))
        {
            // Ether the seconds have passed or the seconds match and the nanoseconds have passed (or match).
            expired = true;
        }
    }
    else
    {
        // Something has gone seriously wrong... assume the timeout has expired.
        expired = true;
        PAL_ASSERT_ALWAYS();
    }

    return expired;
}

// =====================================================================================================================
// Sleep until the absolute time.
int32 SleepToAbsTime(
    const timespec* pSleepTime)
{
    return clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, pSleepTime, nullptr);
}

// =====================================================================================================================
// Computes the remaining timeout in nanosecond
void ComputeTimeoutLeft(
    const timespec* pAbsTimeout,
    uint64* pNanoSeconds)
{
    timespec currentTime = {};
    if (clock_gettime(CLOCK_MONOTONIC, &currentTime) == 0)
    {
        if ((currentTime.tv_sec > pAbsTimeout->tv_sec) ||
                ((currentTime.tv_sec  == pAbsTimeout->tv_sec) && (currentTime.tv_nsec >= pAbsTimeout->tv_nsec)))
        {
            // if the target has already passed, set the timeout to be 0.
            *pNanoSeconds = 0;
        }
        else
        {
            *pNanoSeconds = (pAbsTimeout->tv_sec - currentTime.tv_sec) * NanoSecPerSec +
                                                                         pAbsTimeout->tv_nsec - currentTime.tv_nsec;
        }
    }
    else
    {
        *pNanoSeconds = 0;
    }
}

} // Util
