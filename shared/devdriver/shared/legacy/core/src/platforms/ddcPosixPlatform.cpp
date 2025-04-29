/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "ddPlatform.h"

#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>

#include <time.h>

#include <dlfcn.h>

#if defined(DD_PLATFORM_LINUX_UM)
extern "C"
{
    #include <sys/utsname.h>
    #include <sys/sysinfo.h>
}
#elif defined(DD_PLATFORM_DARWIN_UM)
    #include <os/log.h>

    #include <sys/param.h>
    #include <sys/sysctl.h>
#endif

namespace DevDriver
{
    namespace Platform
    {
        // Constant value used to convert between seconds and nanoseconds
        DD_STATIC_CONST uint64 kNanosecsPerSec = (1000 * 1000 * 1000);

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
        void PlatformDebugPrint(LogLevel lvl, const char* pStr)
        {
#if defined(DD_PLATFORM_DARWIN_UM)
            static const os_log_t logObject = os_log_create("com.amd.devdriver", "amd devdriver");
            static constexpr os_log_type_t kLogLevelTable[static_cast<int>(LogLevel::Count)] =
            {
                OS_LOG_TYPE_DEBUG,
                OS_LOG_TYPE_INFO,
                OS_LOG_TYPE_DEFAULT,
                OS_LOG_TYPE_ERROR,
                OS_LOG_TYPE_FAULT,
                OS_LOG_TYPE_DEFAULT
            };
            os_log_with_type(logObject, kLogLevelTable[static_cast<int>(lvl)], "%s\n", pStr);
#else
            // No Linux-specific logging
            DD_UNUSED(lvl);
            DD_UNUSED(pStr);
#endif
        }

        Result GetAbsPathName(
            const char*  pPath,
            char         (&absPath)[256])
        {
            Result result = Result::InvalidParameter;

            if (pPath != nullptr)
            {
                errno = 0;
                // WA: gcc 4.8 warns if the return value of realpath isn't used
                const char* pAbsPath = realpath(pPath, absPath);
                DD_UNUSED(pAbsPath);

                if (errno != 0)
                {
                    // Details about the error are available with errno, but we can't translate this easily.
                    result = Result::FileAccessError;
                }
                else
                {
                    // Success!
                    result = Result::Success;
                }
            }

            return result;
        }

        int32 AtomicIncrement(Atomic* pVariable)
        {
            return __sync_add_and_fetch(pVariable, 1);
        }

        int32 AtomicDecrement(Atomic* pVariable)
        {
            return __sync_sub_and_fetch(pVariable, 1);
        }

        int32 AtomicAdd(Atomic* pVariable, int32 num)
        {
            return __sync_add_and_fetch(pVariable, num);
        }

        int32 AtomicSubtract(Atomic* pVariable, int32 num)
        {
            return __sync_sub_and_fetch(pVariable, num);
        }

        int64 AtomicIncrement(Atomic64* pVariable)
        {
            return __sync_add_and_fetch(pVariable, 1);
        }

        int64 AtomicDecrement(Atomic64* pVariable)
        {
            return __sync_sub_and_fetch(pVariable, 1);
        }

        int64 AtomicAdd(Atomic64* pVariable, int64 num)
        {
            return __sync_add_and_fetch(pVariable, num);
        }

        int64 AtomicSubtract(Atomic64* pVariable, int64 num)
        {
            return __sync_sub_and_fetch(pVariable, num);
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

// pthread_attr_setinheritsched is available in NDK API level >= 28. For now, only call this
// function for non-Android platform.
#if !defined (__ANDROID__)
                // POSIX thread library is not support flag SCHED_RESET_ON_FORK. If app process's realtime sched policy
                // has SCHED_RESET_ON_FORK flag(like policy SCHED_FIFO | SCHED_RESET_ON_FORK), pthread_create without attr
                // create a new thread will use __default_pthread_attr which would abandon flag SCHED_RESET_ON_FORK, then
                // new created thread may block in some case for it still in real time sched policy. One way to deal with
                // this issue is setting inheritsched attr for children thread, it can bypass the __default_pthread_attr
                // and use parent's sched policy(SCHED_FIFO | SCHED_RESET_ON_FORK), then when children thread exec, kernel
                // sched policy will be changed into SCHED_OTHER which obey linux manual.

                bool isSchedPolicyResetOnFork = (sched_getscheduler(getpid()) & SCHED_RESET_ON_FORK) ? true : false;
                if (isSchedPolicyResetOnFork)
                {
                    pthread_attr_t attr;
                    pthread_attr_init(&attr);
                    pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
                    result = (pthread_create(&hThread, &attr, &Thread::ThreadShim, this) == 0) ? Result::Success
                                                                                               : Result::Error;
                    pthread_attr_destroy(&attr);
                }
                else
#endif
                {
                    result = (pthread_create(&hThread, nullptr, &Thread::ThreadShim, this) == 0) ? Result::Success
                                                                                                 : Result::Error;
                }

                if (result != Result::Success)
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
            DD_PRINT(LogLevel::Verbose, "SetName() called, but not implemented for this platform");
#endif
            return result;
        }

        Result Thread::Join(uint32 timeoutInMs)
        {
            Result result = Result::Error;

            if (IsJoinable())
            {
                // TODO: Improve robustness in cases of external thread termination

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

            Result result = Result::Success;
            if (m_hLib == nullptr)
            {
                result = Result::FileNotFound;
                DD_PRINT(LogLevel::Warn, "Failed to load library \"%s\". Reason: %s", pLibraryName, dlerror());
            }

            return result;
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

        bool AtomicLock::TryLock()
        {
            return (__sync_val_compare_and_swap(&m_lock, 0, 1) == 0);
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
#elif defined(DD_PLATFORM_DARWIN_UM)
        Semaphore::Semaphore(uint32 initialCount, uint32 maxCount)
        {
            // macOS doesn't enforce a max. Beware.
            DD_UNUSED(maxCount);
            m_semaphore = dispatch_semaphore_create(initialCount);
            DD_ASSERT(m_semaphore != NULL);
        }

        Semaphore::~Semaphore()
        {
        }

        Result Semaphore::Signal()
        {
            dispatch_semaphore_signal(m_semaphore);
            return Result::Success;
        }

        Result Semaphore::Wait(uint32 timeoutInMs)
        {
            const int64_t timeoutInNs = timeoutInMs * 1000000;
            const dispatch_time_t waitTime = dispatch_time(DISPATCH_TIME_NOW, timeoutInNs);
            const int retVal = dispatch_semaphore_wait(m_semaphore, waitTime);
            return (retVal == 0) ? Result::Success : Result::NotReady;
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

        Result Mkdir(const char* pDir, MkdirStatus* pStatus)
        {
            Result result = Result::InvalidParameter;

            if (pDir != nullptr)
            {
                errno = 0;

                const int ret = mkdir(pDir, 0777);
                if (ret == 0)
                {
                    // The directory did not exist, and was created successfully
                    result = Result::Success;

                    if (pStatus != nullptr)
                    {
                        *pStatus = MkdirStatus::Created;
                    }
                }
                else
                {
                    if (errno == EEXIST)
                    {
                        // The directory did exist, which is fine.
                        result = Result::Success;

                        if (pStatus != nullptr)
                        {
                            *pStatus = MkdirStatus::Existed;
                        }
                    }
                    else
                    {
                        result = Result::FileIoError;
                    }
                }
            }

            return result;
        }

#if defined(DD_PLATFORM_DARWIN_UM)
        // On Mac the DevDriver is an XPC service, which can create multiple connections concurrently.
        // Therefore to override the process id used to create connections we need a thread-local variable.
        static thread_local ProcessId GOverrideProcessId = 0;

        void OverrideProcessId(ProcessId id)
        {
            GOverrideProcessId = id;
        }

        ProcessId GetProcessId()
        {
            if (GOverrideProcessId)
            {
                return GOverrideProcessId;
            }
            else
            {
                return static_cast<ProcessId>(getpid());
            }
        }
#else
        ProcessId GetProcessId()
        {
            return static_cast<ProcessId>(getpid());
        }
#endif

        uint64 GetCurrentTimeInMs()
        {
            timespec timeValue = {};

            const int result = clock_gettime(CLOCK_MONOTONIC, &timeValue);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);

            return static_cast<uint64>(timeValue.tv_sec * 1000 + timeValue.tv_nsec / 1000000);
        }

        uint64 QueryTimestampFrequency()
        {
            // Posix platforms always have a 1 nanosecond timestamp frequency
            return kNanosecsPerSec;
        }

        uint64 QueryTimestamp()
        {
            uint64 timestamp = 0;

            timespec timeSpec = {};
            if (clock_gettime(CLOCK_MONOTONIC, &timeSpec) == 0)
            {
                timestamp = ((timeSpec.tv_sec * kNanosecsPerSec) + timeSpec.tv_nsec);
            }
            else
            {
                DD_ASSERT_REASON("Failed to query monotonic clock for timestamp!");
            }

            return timestamp;
        }

        void Sleep(uint32 millisecTimeout)
        {
            timespec relativeTime;
            const time_t sec = static_cast<time_t>(millisecTimeout / 1000);
            relativeTime.tv_sec = sec;
            relativeTime.tv_nsec = (millisecTimeout - (sec * 1000)) * 1000000;
            RetryTemporaryFailure(nanosleep, &relativeTime, &relativeTime);
        }

#if defined(DD_PLATFORM_DARWIN_UM)
        // On Mac the DevDriver is an XPC service, which can create multiple connections concurrently.
        // Therefore to override the process name used to create connections we need a thread-local variable.
        static constexpr size_t kMaxStringLength = 128;
        static thread_local char GOverrideProcessName[kMaxStringLength] = "";

        void OverrideProcessName(char const* name)
        {
            Strncpy(GOverrideProcessName, name, kMaxStringLength);
        }

        void GetProcessName(char* buffer, size_t bufferSize)
        {
            DD_ASSERT(buffer != nullptr);
            const char* pProcessName = (strlen(GOverrideProcessName) > 0) ? GOverrideProcessName : getprogname();
            Strncpy(buffer, (pProcessName != nullptr) ? pProcessName : "Unknown", bufferSize);
        }
#else
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
#endif

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

        void Strncat(char* pDst, const char* pSrc, size_t dstSize)
        {
            DD_ASSERT(pDst != nullptr);
            DD_ASSERT(pSrc != nullptr);

            // Compute the length of the destination string to prevent buffer overruns.
            const size_t dstLength = strlen(pDst);
            strncat(pDst, pSrc, (dstSize - dstLength - 1));
        }

        int32 Strcmpi(const char* pSrc1, const char* pSrc2)
        {
            DD_ASSERT(pSrc1 != nullptr);
            DD_ASSERT(pSrc2 != nullptr);

            return strcasecmp(pSrc1, pSrc2);
        }

#if defined(DD_PLATFORM_DARWIN_UM)
        template <size_t BufferSize>
        Result DarwinSysCtlString(int key0, int key1, char (&buffer)[BufferSize])
        {
            Result result = Result::InvalidParameter;
            size_t length = 0;
            int    ret    = 0;

            int keys[2] = {
                key0,
                key1,
            };

            ret = sysctl(keys, ArraySize(keys), nullptr, &length, nullptr, 0);
            result = (ret < 0) ? Result::Error : Result::Success;
            if (result == Result::Success)
            {
               if (length >= BufferSize)
               {
                   result = Result::InsufficientMemory;
               }
            }

            if (result == Result::Success)
            {
                ret = sysctl(keys, ArraySize(keys), buffer, &length, nullptr, 0);
                result = (ret < 0) ? Result::Error : Result::Success;
            }

            return result;
        }
#endif

        Result QueryEtwInfo(EtwSupportInfo* pInfo)
        {
            *pInfo             = {};
            return Result::Unavailable;
        }

        Result QueryOsInfo(OsInfo* pInfo)
        {
            Result result = Result::Success;

            DD_ASSERT(pInfo != nullptr);
            memset(pInfo, 0, sizeof(*pInfo));

#if defined(DD_PLATFORM_LINUX_UM)
            Strncpy(pInfo->type, OsInfo::kOsTypeLinux);

            // Query OS name
            {
                // Reference: https://man7.org/linux/man-pages/man5/os-release.5.html

                FILE* pOsReleaseFile = nullptr;
                pOsReleaseFile = fopen("/etc/os-release", "r");
                if (pOsReleaseFile == nullptr)
                {
                    pOsReleaseFile = fopen("/usr/lib/os-release", "r");
                }

                const char* const OsNameLinePrefix = "NAME=";
                char* pNameLine = nullptr;

                const size_t LineSize = 64;
                char line[LineSize] = {};

                if (pOsReleaseFile != nullptr)
                {
                    // Find the line that contains the OS name.
                    while (fgets(line, LineSize, pOsReleaseFile))
                    {
                        pNameLine = strstr(line, OsNameLinePrefix);
                        if (pNameLine)
                        {
                            break;
                        }
                    }
                    fclose(pOsReleaseFile);
                }

                if (pNameLine != nullptr)
                {
                    size_t nameLen = 0;

                    // trim surrounding quotes
                    char* pName = pNameLine + strlen(OsNameLinePrefix);
                    char* pFirstQuote = strstr(pName, "\"");
                    if (pFirstQuote)
                    {
                        pName += 1;
                        char* pSecondQuote = strstr(pName, "\"");
                        if (pSecondQuote)
                        {
                            nameLen = (size_t)(pSecondQuote - pName);
                        }
                    }

                    // if no quotes, trim the ending newline character
                    if (nameLen == 0)
                    {
                        char* pNewline = strstr(pName, "\n");
                        if (pNewline)
                        {
                            nameLen = (size_t)(pNewline - pName);
                        }
                    }

                    // otherwise just get the entire length
                    if (nameLen == 0)
                    {
                        nameLen = strlen(pName);
                    }

                    size_t destNameSize = sizeof(pInfo->name) - 1;
                    size_t copySize = Platform::Min(destNameSize, nameLen);

                    strncpy(pInfo->name, pName, copySize);
                    pInfo->name[copySize] = '\0';
                }
            }

            // Query description
            {
                utsname info = {};
                uname(&info);

                // Show this info in any order. We just need to see it.
                // This produces output like this:
                //      Linux 4.9.184-linuxkit x86_64     #1 SMP Tue Jul 2 22:58:16 UTC 2019
                Snprintf(pInfo->description, ArraySize(pInfo->description),
                         "%s %s %s     %s",
                         info.sysname, info.release, info.machine, info.version);
            }

            // Query available memory
            {
                // reference: https://man7.org/linux/man-pages/man2/sysinfo.2.html

                struct sysinfo info = {};
                int err = sysinfo(&info);
                if (err == 0)
                {
                    pInfo->physMemory = info.totalram;
                    pInfo->swapMemory = info.totalswap;
                }
                else
                {
                    DD_PRINT(LogLevel::Warn, "[Platform::QueryOsInfo] sysinfo failed with errno: %d", errno);
                }
            }

#elif defined(DD_PLATFORM_DARWIN_UM)
            Strncpy(pInfo->type, OsInfo::kOsTypeDarwin);

            // Query OS name
            {
                // TODO: Query macOS revision name, e.g. "Mojave" or "Catalina"
            }

            // Query description
            {
                char model[128]   = {};
                char version[128] = {};

                if (result == Result::Success)
                {
                    // e.g. "MacPro4,1" or "iPhone8,1"
                    result = DarwinSysCtlString(CTL_HW, HW_MODEL, model);
                }

                if (result == Result::Success)
                {
                    // e.g. "Darwin Kernel Version 18.7.0: Tue Aug 20 16:57:14 PDT 2019; root:xnu-4903.271.2~2/RELEASE_X86_64"
                    result = DarwinSysCtlString(CTL_KERN, KERN_VERSION, version);
                }

                Snprintf(pInfo->description, ArraySize(pInfo->description),
                         "%s - %s",
                         model,
                         version);
            }
#else
            static_assert(false, "Building on an unknown platform: " DD_PLATFORM_STRING ". Add an implementation to QueryOsInfo().");
#endif
            /// Query information about the current user
            {
                const char* pUser = getenv("USER");
                DD_WARN(pUser != nullptr);
                if (pUser != nullptr)
                {
                    Platform::Strncpy(pInfo->user.name, pUser);
                }

                const char* pHomeDir = getenv("HOME");
                DD_WARN(pHomeDir != nullptr);
                if (pHomeDir != nullptr)
                {
                    Platform::Strncpy(pInfo->user.homeDir, pHomeDir);
                }
            }

            // Query memory
            {
                // TODO: Query available physical memory and swap space
            }

            if (result == Result::Success)
            {
                result = (gethostname(pInfo->hostname, sizeof(pInfo->hostname)) == 0)
                    ? Result::Success
                    : Result::Error;
            }

            return result;
        }
    }
}
