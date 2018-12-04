/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include "palUtil.h"

struct timespec;

namespace Util
{

// Computes the absolute time at which a timeout duration should expire.
extern void ComputeTimeoutExpiration(timespec* pAbsTimeout, uint64 nanoseconds);

// Computes the absolute time according timeout.
extern int64 ComputeAbsTimeout(uint64 timeout);

// Determines whether or not an absolute timeout duration has passed.
extern bool IsTimeoutExpired(const timespec* pAbsTimeout);

// Suspends the execution of the calling thread until the time specified by pSleepTime has elapsed.
extern int32 SleepToAbsTime(const timespec* pSleepTime);

// Computes the remaining timeout
extern void ComputeTimeoutLeft(const timespec* pAbsTimeout, uint64* pNanoSeconds);

} // Util
