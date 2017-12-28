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
/**
 ***********************************************************************************************************************
 * @file  palSemaphore.h
 * @brief PAL utility collection Semaphore class declaration.
 ***********************************************************************************************************************
 */

#pragma once

#include "palUtil.h"

#include <limits.h>

#include <semaphore.h>

// Define the POSIX SEM_VALUE_MAX constant to the required minimum value if the compiler didn't do it for us.
#ifndef SEM_VALUE_MAX
#define SEM_VALUE_MAX 32767
#endif

namespace Util
{

/**
************************************************************************************************************************
* @brief Platform-agnostic semaphore primitive.
************************************************************************************************************************
*/
class Semaphore
{
public:
    Semaphore() : m_osSemaphore() { }
    ~Semaphore();

    /// Initializes the semaphore object.
    ///
    /// Creates and initializes the appropriate OS-specific semaphore object.  It is invalid to use a semaphore that
    /// hasn't been initialized via this method.
    ///
    /// @param [in] maximumCount Maximum count for this semaphore.  Post() calls that would push the count higher
    ///                          will be ignored.  Cannot be larger than MaximumCountLimit.
    /// @param [in] initialCount Initial count value.  Cannot be larger than maximumCount.
    ///
    /// @returns Success if the semaphore was initialized successfully.  Otherwise one of these errors may be returned:
    ///          + ErrorInvalidValue will be returned if maximumCount or eventCount are too large.
    ///          + ErrorInitializationFailed will be returned if an OS-specific error occured.
    Result Init(uint32 maximumCount, uint32 initialCount);

    /// Stalls the current thread until the semaphore is in the signaled state.
    ///
    /// Decrements the semaphore count if the wait succeeds.
    ///
    /// @param [in] milliseconds Time in milliseconds before the call will timeout and return control to the caller.
    ///                          Can be set to 0xFFFFFFFF to never timeout.
    ///
    /// @returns @ref Success if the wait completed successfully, or @ref Timeout if the wait timed out.
    Result Wait(uint32 milliseconds);

    /// Increments the semaphore count value.
    ///
    /// @param [in] postCount Amount to increment the semaphore count.  Default value of 1.
    void Post(uint32 postCount = 1);

    /// Specifies the largest value supported by Init's maximumCount parameter.
    static constexpr uint32 MaximumCountLimit = SEM_VALUE_MAX;

private:
    sem_t   m_osSemaphore;  // Linux-specific semaphore handle.

    PAL_DISALLOW_COPY_AND_ASSIGN(Semaphore);
};

} // Util
