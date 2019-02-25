/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  ddPosixPlatform.h
* @brief POSIX Platform layer definition
***********************************************************************************************************************
*/

#pragma once

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#define DD_DARWIN
#elif defined(__linux__)
#include <semaphore.h>
#define DD_LINUX
#else
static_assert(false, "Unknown platform detected")
#endif

#define DD_RESTRICT __restrict__

#define DD_DEBUG_BREAK() raise(SIGTRAP)

// Don't use this macro. Use DD_ASSERT instead.
//
// This macro tells the compiler to take a boolean expression as absolute truth.
// Branches or operations can be deleted in optimization passes if you use this on
// something that's true *most* of the time, but not *all* of it.
//
// Observe this example: https://godbolt.org/z/3Asevh
// The programmer accidentally creates conflicting conditions in `square()`.
// As a a result, the entire square function is deleted and so are any code blocks
// that call it.
//
// Be very careful.
#if defined(__clang__)
#define DD_AXIOMATICALLY_CANNOT_HAPPEN(expr) __builtin_assume(expr)
#else
#define DD_AXIOMATICALLY_CANNOT_HAPPEN(expr) ((expr) ? DD_UNUSED(0) : __builtin_unreachable())
#endif

namespace DevDriver
{
    namespace Platform
    {
        template <typename T, typename... Args>
        int RetryTemporaryFailure(T& func, Args&&... args)
        {
            int retval;
            do
            {
                retval = func(args...);
            } while (retval == -1 && errno == EINTR);
            return retval;
        }

        typedef volatile int32 Atomic;

        struct EmptyStruct
        {
        };

        struct EventStorage
        {
            pthread_mutex_t mutex;
            pthread_cond_t condition;
            volatile bool isSet;
        };

        typedef pthread_mutex_t MutexStorage;

#if defined(DD_LINUX)
        typedef sem_t SemaphoreStorage;
        typedef drand48_data RandomStorage;
#endif

        struct ThreadStorage
        {
            ThreadFunction pFnFunction;
            void*          pParameter;
            pthread_t      hThread;

            ThreadStorage()
            {
                memset(this, 0, sizeof(*this));
            }
        };
    }
}
