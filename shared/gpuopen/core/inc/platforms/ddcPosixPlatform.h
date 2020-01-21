/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  ddcPosixPlatform.h
* @brief POSIX Platform Layer Header
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

#if defined(DD_PLATFORM_LINUX_UM)
    #include <semaphore.h>
#else
    static_assert(false, "Unknown platform detected")
#endif

#define DD_RESTRICT __restrict__

#define DD_DEBUG_BREAK() ::raise(SIGTRAP)

// The DD_ASSUME() macro passes to a C++ compiler a boolean expression,
// which is assumed to be an *absolute truth*, without any checking.
//
// Note that, C++ optimizer will use input expression to generate faster code,
// because calling DD_ASSUME() on expressions which are not *always* true
// is undefined behavior.
//
// This macro can be used to inform the compiler about preconditions
// that your code assumes, but cannot validate.
//
// ## Example Use Case - Generators
//
// A generator represents a potentially stateless object,
// which computes a series of values lazily (on demand).
// Dereferencing an iterator of a generator triggers a computation,
// which will produce the next value - that means after reading it once, it's gone.
// Because generator can be stateless,
// its iterator has to model an input iterator (it cannot model a forward iterator).
//
// This means precondition of get_val() cannot be checked,
// otherwise generated value will be lost!
//
//     float get_val (generator<float>::iterator it)
//     {
//         DD_ASSUME(valid(it)); // Do not call DD_ASSERT(valid(it)) here!
//         return *it;
//     }
//
// In this case calling DD_ASSUME() is correct and desirable.
// The code is written in such a way, that if the precondition is not met,
// we have a crash, so it makes sense to generate code assuming callers
// are not violating get_val()'s contract.
//
// For scenarios where one *can* validate this assumption,
// it is recommended to use DD_ASSERT(), because DD_ASSERT() will
// do that validation in a Debug build and behave like DD_ASSUME() in Release build.
//
// [GCC] __builtin_unreachable()
//  ~ https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
//
// [C++] Contracts: expects, ensures, assert, axiom
//  ~ https://en.cppreference.com/w/cpp/language/attributes/contract
#define DD_ASSUME(expression) do \
{                                \
    if (!(expression))           \
    {                            \
        __builtin_unreachable(); \
    }                            \
} while (0)

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

#if defined(DD_PLATFORM_LINUX_UM)
        typedef sem_t SemaphoreStorage;
#endif

        typedef pthread_t ThreadHandle;
        typedef void*     ThreadReturnType;
        typedef void*     LibraryHandle;

        constexpr ThreadHandle kInvalidThreadHandle = 0;

        // Maximum supported size for thread names, including NULL byte
        // This exists because some platforms have hard limits on thread name size.
        // The Linux Kernel has a hard limit of 16 bytes for the thread name size including NULL.
        static constexpr size_t kThreadNameMaxLength = 16;
    }
}
