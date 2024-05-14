/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
 ***********************************************************************************************************************
 * @file  palConditionVariable.h
 * @brief PAL utility collection ConditionVariable class declaration.
 ***********************************************************************************************************************
 */

#pragma once

#include "palUtil.h"
#include "palMutex.h"

#if   defined(__unix__)
#include <pthread.h>
#endif
#include <chrono>

namespace Util
{

/**
 ***********************************************************************************************************************
 * @brief Encapsulates details of condition variable management for various platforms.
 ***********************************************************************************************************************
 */
class ConditionVariable
{
public:

#if   defined(__unix__)
    /// Defines ConditionVariableData as a unix pthread_cond_t
    typedef pthread_cond_t ConditionVariableData;
    /// @note pthread_cond_init will not fail as called
    ConditionVariable() noexcept : m_osCondVariable {} { pthread_cond_init(&m_osCondVariable, nullptr); }
    ~ConditionVariable() noexcept { pthread_cond_destroy(&m_osCondVariable); };
#endif

    /// Atomically releases the given mutex lock and initiates a sleep waiting for WakeOne() or WakeAll() to be called
    /// on this condition variable from a different thread.
    ///
    /// @param [in] pMutex   Mutex object to be released when the sleep is begun and reacquired before returning
    ///                      control to the caller.
    /// @param [in] timeout  Number of milliseconds to sleep before timing out the operation and returning.  The
    ///                      mutex will be re-acquired before returning even if a timeout occurs.
    ///
    /// @returns True if the call succeeded and the thread was successfully awoken, false if the sleep timed out.
    bool Wait(Mutex* pMutex, std::chrono::milliseconds timeout);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 863
    bool Wait(Mutex* pMutex, uint32 timeoutMs);
#endif

    /// Wakes up one thread that is waiting on this condition variable.
    void WakeOne();

    /// Wakes up all threads that are waiting on this condition variable.
    void WakeAll();

private:
    ConditionVariableData m_osCondVariable; // os-specific ConditionVariable structure.

    PAL_DISALLOW_COPY_AND_ASSIGN(ConditionVariable);
};

} // Util
