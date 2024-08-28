/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#if defined(DD_PLATFORM_DARWIN_UM)
    #include <dispatch/dispatch.h>
#elif defined(DD_PLATFORM_LINUX_UM)
    #include <semaphore.h>
#else
    static_assert(false, "Unknown platform detected");
#endif

#include <sys/stat.h>

#define DD_RESTRICT __restrict__

#define DD_DEBUG_BREAK() ::raise(SIGTRAP)

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

        /* platform functions for performing atomic operations */
        typedef volatile int32 Atomic;
        typedef volatile int64 Atomic64;

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
#elif defined(DD_PLATFORM_DARWIN_UM)
        typedef dispatch_semaphore_t SemaphoreStorage;

        // Functions to override the process name & ID returned by GetProcessId/GetProcessName.
        // On Mac the DevDriver is compiled as a separate XPC service executable.
        // Each XPC connection therefore has to provide the actual program name & PID,
        // otherwise all the connections will report as coming from the XPC service!

        // Overrides the ProcessId with one provided, should only be used by RadeonDeveloperServiceXPC
        void OverrideProcessId(ProcessId id);

        // Overrides the Process name with one provided, should only be used by RadeonDeveloperServiceXPC
        void OverrideProcessName(char const* name);
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
