/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  ddcPosixPlatform.cpp
* @brief POSIX platform layer implementation
***********************************************************************************************************************
*/

#include "ddPlatform.h"

#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <time.h>
#include <dlfcn.h>

namespace DevDriver
{
    namespace Platform
    {
        static Result GetAbsTime(uint32 offsetInMs, timespec *pOutput)
        {
            Result result = Result::Error;
            timespec timeValue = {};
            if (clock_gettime(CLOCK_REALTIME, &timeValue) == 0)
            {
                const uint64 timeInMs = (static_cast<uint64>(timeValue.tv_sec) * 1000) +
                                        (static_cast<uint64>(timeValue.tv_nsec) / 1000000) + offsetInMs;
                const uint64 sec = timeInMs / 1000;
                const uint64 nsec = (timeInMs - (sec * 1000)) * 1000000;
                pOutput->tv_sec = static_cast<time_t>(sec);
                pOutput->tv_nsec = static_cast<long>(nsec);
                result = Result::Success;
            }
            return result;
        }

        /////////////////////////////////////////////////////
        // Local routines.....
        //
        void DebugPrint(LogLevel lvl, const char* pFormat, ...)
        {
            DD_UNUSED(lvl);

            va_list args;
            va_start(args, pFormat);
            char buffer[1024];
            Platform::Vsnprintf(buffer, sizeof(buffer), pFormat, args);
            va_end(args);

            printf("%s\n", buffer);

        }

        int32 AtomicIncrement(Atomic *variable)
        {
            return __sync_add_and_fetch(variable, 1);
        }

        int32 AtomicDecrement(Atomic *variable)
        {
            return __sync_sub_and_fetch(variable, 1);
        }

        int32 AtomicAdd(Atomic *variable, int32 num)
        {
            return __sync_add_and_fetch(variable, num);
        }

        int32 AtomicSubtract(Atomic *variable, int32 num)
        {
            return __sync_sub_and_fetch(variable, num);
        }

        /////////////////////////////////////////////////////
        // Thread routines.....
        //

        Result Thread::Start(ThreadFunction pFnThreadFunc, void* pThreadParameter)
        {
            Result result = Result::Error;

            // Check if this thread handle has already been initialized.
            // pthread_t types act as opaque, and do not work portably when compared directly.
            // To get around this, we use the threadFunc pointer instead, since it is never allowed to be NULL.
            if ((pFnFunction == nullptr) && (pFnThreadFunc != nullptr))
            {
                pParameter  = pThreadParameter;
                pFnFunction = pFnThreadFunc;

                if (pthread_create(&hThread, nullptr, &Thread::ThreadShim, this) == 0)
                {
                    result = Result::Success;
                }
                else
                {
                    Reset();
                }

                DD_WARN(result != Result::Error);
            }
            return result;
        }

        Result Thread::SetNameRaw(const char* pThreadName)
        {
            Result result = Result::Error;
#if DD_PLATFORM_LINUX_UM && DD_PLATFORM_IS_GNU
            const int ret = pthread_setname_np(hThread, pThreadName);
            if (ret != 0)
            {
                DD_PRINT(LogLevel::Error,
                         "pthread_setname_np() failed with: %d (0x%x)",
                         ret,
                         ret);
            }
            else
            {
                result = Result::Success;
            }
#else
            DD_UNUSED(pThreadName);
            DD_PRINT(LogLevel::Warn, "SetName() called, but not implemented for this platform");
#endif
            return result;
        }

        Result Thread::Join(uint32 timeoutInMs)
        {
            Result result = Result::Error;

            if (IsJoinable())
            {
                // Wait for the thread to signal that it has exited.
                result = onExit.Wait(timeoutInMs);
            }
            else
            {
                DD_WARN_REASON("Join()ing a thread that's not joinable");
            }

            if (result == Result::Success)
            {
                // The thread exited normally, so we can join here and not worry about timing out.
                const int ret = pthread_join(hThread, nullptr);
                if (ret == 0)
                {
                    Reset();
                    result = Result::Success;
                }
                else
                {
                    // See:
                    //      man 3 pthread_join
                    // Expected errors you might see here if something went wrong:
                    //      EDEADLK
                    //            A deadlock was detected (e.g., two threads tried to join with
                    //            each other); or thread specifies the calling thread.
                    //      EINVAL thread is not a joinable thread.
                    //      EINVAL Another thread is already waiting to join with this thread.
                    //      ESRCH  No thread with the ID thread could be found.
                    DD_PRINT(LogLevel::Debug, "pthread_join() failed with 0x%x", ret);
                    result = Result::Error;
                }
            }

            DD_WARN(result != Result::Error);
            return result;
        }

        bool Thread::IsJoinable() const
        {
            // pthread_t types act as opaque, and do not work portably when compared directly.
            // To get around this, we use the threadFunc pointer instead, since it is never allowed to be NULL.
            return (pFnFunction != nullptr);
        }

        /////////////////////////////////////////////////////
        // Library
        /////////////////////////////////////////////////////

        // Loads a Shared Object with the specified name into this process.
        Result Library::Load(
            const char* pLibraryName)
        {
            constexpr uint32 Flags = RTLD_LAZY;
            m_hLib = dlopen(pLibraryName, Flags);

            return (m_hLib == nullptr) ? Result::FileNotFound : Result::Success;
        }

        // Unloads this Shared Object if it was loaded previously.  Called automatically during the object destructor.
        void Library::Close()
        {
            if (m_hLib != nullptr)
            {
                dlclose(m_hLib);
                m_hLib = nullptr;
            }
        }

        void* Library::GetFunctionHelper(
            const char* pName
        ) const
        {
            DD_ASSERT(m_hLib != nullptr);
            return reinterpret_cast<void*>(dlsym(m_hLib, pName));
        }

        /////////////////////////////////////////////////////
        // Memory Management
        /////////////////////////////////////////////////////

        void* AllocateMemory(size_t size, size_t alignment, bool zero)
        {
            void* pMemory = nullptr;
            const int retVal = posix_memalign(&pMemory, alignment, size);
            if ((retVal == 0) && (pMemory != nullptr) && zero)
            {
                memset(pMemory, 0, size);
            }

            return pMemory;
        }

        void FreeMemory(void* pMemory)
        {
            free(pMemory);
        }

        /////////////////////////////////////////////////////
        // Synchronization primatives
        //

        void AtomicLock::Lock()
        {
            // TODO - implement timeout
            while (__sync_val_compare_and_swap(&m_lock, 0, 1) == 1)
            {
                // spin until the mutex is unlocked again
                while (m_lock != 0)
                {
                }
            }
        }

        void AtomicLock::Unlock()
        {
            const long result = __sync_val_compare_and_swap(&m_lock, 1, 0);
            DD_UNUSED(result);
            DD_ASSERT(result != 0);
        }

        Mutex::Mutex()
        {
            const int result = pthread_mutex_init(&m_mutex, nullptr);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);
        }

        Mutex::~Mutex()
        {
            const int result = pthread_mutex_destroy(&m_mutex);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);
        }

        void Mutex::Lock()
        {
            const int result = pthread_mutex_lock(&m_mutex);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);
        }

        void Mutex::Unlock()
        {
            const int result = pthread_mutex_unlock(&m_mutex);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);
        }

#if defined(DD_PLATFORM_LINUX_UM)
        Semaphore::Semaphore(uint32 initialCount, uint32 maxCount)
        {
            // linux doesn't enforce a max. Beware.
            DD_UNUSED(maxCount);

            int result = sem_init(&m_semaphore, 0, initialCount);
            DD_ASSERT(result == 0);
            DD_UNUSED(result);
        }

        Semaphore::~Semaphore()
        {
            int result = sem_destroy(&m_semaphore);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);
        }

        Result Semaphore::Signal()
        {
            int result = sem_post(&m_semaphore);
            DD_ASSERT(result == 0);
            return (result == 0) ? Result::Success : Result::Error;
        }

        Result Semaphore::Wait(uint32 timeoutInMs)
        {
            Result result = Result::Error;

            timespec timeout = {};

            if (GetAbsTime(timeoutInMs, &timeout) == Result::Success)
            {
                int retVal = RetryTemporaryFailure(sem_timedwait,&m_semaphore, &timeout);
                if (retVal != -1)
                {
                    result = Result::Success;
                } else if (errno == ETIMEDOUT)
                {
                    result = Result::NotReady;
                }
            }
            return result;
        }
#endif

        Event::Event(bool signaled)
        {
            int result = pthread_mutex_init(&m_event.mutex, nullptr);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);

            result = pthread_cond_init(&m_event.condition, nullptr);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);

            m_event.isSet = signaled;
        }

        Event::~Event()
        {
            int result = pthread_cond_destroy(&m_event.condition);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);

            result = pthread_mutex_destroy(&m_event.mutex);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);
        }

        void Event::Clear()
        {
            int result = pthread_mutex_lock(&m_event.mutex);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);

            m_event.isSet = false;

            result = pthread_mutex_unlock(&m_event.mutex);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);
        }

        void Event::Signal()
        {
            int result = pthread_mutex_lock(&m_event.mutex);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);

            m_event.isSet = true;

            result = pthread_cond_signal(&m_event.condition);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);

            result = pthread_mutex_unlock(&m_event.mutex);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);
        }

        Result Event::Wait(uint32 timeoutInMs)
        {
            Result result = Result::Error;

            timespec timeout = {};

            if (GetAbsTime(timeoutInMs, &timeout) == Result::Success)
            {
                int retVal = pthread_mutex_lock(&m_event.mutex);
                DD_UNUSED(retVal);
                DD_ASSERT(retVal == 0);

                int waitResult = 0;
                while (!m_event.isSet && waitResult == 0)
                {
                    waitResult = pthread_cond_timedwait(&m_event.condition, &m_event.mutex, &timeout);
                }

                if (waitResult == 0)
                {
                    result = Result::Success;
                } else if (waitResult == ETIMEDOUT)
                {
                    result = Result::NotReady;
                }

                retVal = pthread_mutex_unlock(&m_event.mutex);
                DD_UNUSED(retVal);
                DD_ASSERT(retVal == 0);
            }
            return result;
        }

#if defined(DD_PLATFORM_LINUX_UM)
        Random::Random()
        {
            timespec timeValue = {};
            int result = clock_gettime(CLOCK_MONOTONIC, &timeValue);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);

            // Use the current time to generate a random seed. Has to be 64 bit because seed48_r expects to get
            // unsigned short seed[3] as a parameter.
            m_prevState = static_cast<uint64>(timeValue.tv_sec * 1000000000 + timeValue.tv_nsec);
        }
#endif

        ProcessId GetProcessId()
        {
            return static_cast<ProcessId>(getpid());
        }

        uint64 GetCurrentTimeInMs()
        {
            timespec timeValue = {};

            const int result = clock_gettime(CLOCK_MONOTONIC, &timeValue);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);

            return static_cast<uint64>(timeValue.tv_sec * 1000 + timeValue.tv_nsec / 1000000);
        }

        void Sleep(uint32 millisecTimeout)
        {
            timespec relativeTime;
            const time_t sec = static_cast<time_t>(millisecTimeout / 1000);
            relativeTime.tv_sec = sec;
            relativeTime.tv_nsec = (millisecTimeout - (sec * 1000)) * 1000000;
            RetryTemporaryFailure(nanosleep, &relativeTime, &relativeTime);
        }

        void GetProcessName(char* buffer, size_t bufferSize)
        {
            DD_ASSERT(buffer != nullptr);
#if DD_PLATFORM_IS_GNU
            const char* pProcessName = program_invocation_short_name;
#else
            const char* pProcessName = getprogname();
#endif
            Strncpy(buffer, (pProcessName != nullptr) ? pProcessName : "Unknown", bufferSize);

        }

        void Strncpy(char* pDst, const char* pSrc, size_t dstSize)
        {
            DD_ASSERT(pDst != nullptr);
            DD_ASSERT(pSrc != nullptr);
            DD_WARN(strlen(pSrc) < dstSize);

            strncpy(pDst, pSrc, (dstSize - 1));
            pDst[dstSize - 1] = '\0';
        }

        char* Strtok(char* pDst, const char* pDelimiter, char** ppContext)
        {
            DD_ASSERT(pDelimiter != nullptr);
            return strtok_r(pDst, pDelimiter, ppContext);
        }

        void Strcat(char* pDst, const char* pSrc, size_t dstSize)
        {
            DD_ASSERT(pDst != nullptr);
            DD_ASSERT(pSrc != nullptr);
            DD_UNUSED(dstSize);

            strcat(pDst, pSrc);
        }

        int32 Vsnprintf(char* pDst, size_t dstSize, const char* format, va_list args)
        {
            int32 ret = vsnprintf(pDst, dstSize, format, args);

            // If the return value looks like a valid length, add one to account for a NULL byte.
            if (ret >= 0)
            {
                ret += 1;
            }
            else
            {
                // A negative value means that some error occurred
                // We don't print anything here because our logging requires Vsnprintf
            }

            return ret;
        }

    }
}
