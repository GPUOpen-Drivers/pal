/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  platform.h
* @brief GPUOpen Platform Abstraction Layer
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"

#if !defined(NDEBUG)
#if !defined(DEVDRIVER_FORCE_ASSERT)
#define DEVDRIVER_FORCE_ASSERT
#endif
#if !defined(DEVDRIVER_HARD_ASSERT)
#define DEVDRIVER_HARD_ASSERT
#endif
#endif

#if   defined(__APPLE__) || defined(__linux__)
#include "posix/ddPosixPlatform.h"
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define DD_ARCH_STRING "x86-64"
#elif defined(__i386__) || defined(_M_IX86)
#define DD_ARCH_STRING "x86"
#else
#define DD_ARCH_STRING "Unk"
#endif

#if   defined(DD_LINUX)
#define DD_OS_STRING "Linux"
#else
#define DD_OS_STRING "Unknown"
#endif

#include "util/template.h"
#include "util/memory.h"
#include <cstdarg>

// TODO: remove this and make kDebugLogLevel DD_STATIC_CONST when we use a version of visual studio that supports it
#ifdef DEVDRIVER_LOG_LEVEL
    #define DEVDRIVER_LOG_LEVEL_VALUE static_cast<LogLevel>(DEVDRIVER_LOG_LEVEL)
#else
    #if defined(NDEBUG)
        #define DEVDRIVER_LOG_LEVEL_VALUE LogLevel::Always
    #else
        #define DEVDRIVER_LOG_LEVEL_VALUE LogLevel::Verbose
    #endif
#endif

#define DD_WILL_PRINT(lvl) ((lvl >= DEVDRIVER_LOG_LEVEL_VALUE) & (lvl < DevDriver::LogLevel::Count))
#define DD_PRINT(lvl, ...) DevDriver::LogString<lvl>(__VA_ARGS__)

#if !defined(DEVDRIVER_FORCE_ASSERT)

#define DD_ALERT(statement)  ((void)0)
#define DD_ASSERT(statement) ((void)0)
#define DD_ASSERT_REASON(reason) ((void)0)
#define DD_ALERT_REASON(reason) ((void)0)

#else

#define DD_STRINGIFY(str) #str
#define DD_STRINGIFY_(x) DD_STRINGIFY(x)
#define DD_ALERT(statement)                                                                     \
{                                                                                               \
    if (!(statement))                                                                           \
    {                                                                                           \
        DD_PRINT(DevDriver::LogLevel::Alert, "%s (%d): Alert triggered in %s: %s\n",            \
            __FILE__, __LINE__, __func__, DD_STRINGIFY(statement));                             \
    }                                                                                           \
}

#define DD_ASSERT(statement)                                                                    \
{                                                                                               \
    if (!(statement))                                                                           \
    {                                                                                           \
        DD_PRINT(DevDriver::LogLevel::Error, "%s (%d): Assertion failed in %s: %s\n",           \
            __FILE__, __LINE__, __func__, DD_STRINGIFY(statement));                             \
        DevDriver::Platform::DebugBreak(__FILE__, __LINE__, __func__, DD_STRINGIFY(statement)); \
    }                                                                                           \
}

#define DD_ALERT_REASON(reason)                                                     \
{                                                                                   \
    DD_PRINT(DevDriver::LogLevel::Alert, "%s (%d): Alert triggered in %s: %s\n",    \
        __FILE__, __LINE__, __func__, reason);                                      \
}

#define DD_ASSERT_REASON(reason)                                                    \
{                                                                                   \
    DD_PRINT(DevDriver::LogLevel::Error, "%s (%d): Assertion failed in %s: %s\n",   \
        __FILE__, __LINE__, __func__, reason);                                      \
    DevDriver::Platform::DebugBreak(__FILE__, __LINE__, __func__, reason);          \
}

#endif

#if !defined(DD_RESTRICT)
#define DD_RESTRICT
#endif

/// Convenience macro that always asserts.
#define DD_ASSERT_ALWAYS() DD_ASSERT_REASON("Unconditional Assert")

/// Convenience macro that always alerts.
#define DD_ALERT_ALWAYS() DD_ALERT_REASON("Unconditional Alert")

/// Convenience macro that asserts if something has not been implemented.
#define DD_NOT_IMPLEMENTED() DD_ASSERT_REASON("Code not implemented!")

/// Convenience macro that asserts if an area of code that shouldn't be executed is reached.
#define DD_UNREACHABLE() DD_ASSERT_REASON("Unreachable code has been reached!")

namespace DevDriver
{
    namespace Platform
    {
        void DebugPrint(LogLevel lvl, const char* format, ...);

        /* platform functions for performing atomic operations */

        int32 AtomicIncrement(Atomic *variable);
        int32 AtomicDecrement(Atomic *variable);
        int32 AtomicAdd(Atomic *variable, int32 num);
        int32 AtomicSubtract(Atomic *variable, int32 num);

        class Thread
        {
        public:
            Thread();
            ~Thread();
            Result Start(void(*threadCallback)(void *), void *threadParameter);
            Result Join();
            bool IsJoinable() const;

        private:
            ThreadStorage m_thread;
        };

        void* AllocateMemory(size_t size, size_t alignment, bool zero);
        void FreeMemory(void* pMemory);

        /* fast locks */
        class AtomicLock
        {
        public:
            AtomicLock() : m_lock(0) {};
            ~AtomicLock() {};
            void Lock();
            void Unlock();
            bool IsLocked() { return m_lock != 0; };
        private:
            Atomic m_lock;
        };

        class Mutex
        {
        public:
            Mutex();
            ~Mutex();
            void Lock();
            void Unlock();
        private:
            MutexStorage m_mutex;
        };

        class Semaphore
        {
        public:
            explicit Semaphore(uint32 initialCount, uint32 maxCount);
            ~Semaphore();
            Result Signal();
            Result Wait(uint32 millisecTimeout);
        private:
            SemaphoreStorage m_semaphore;
        };

        class Event
        {
        public:
            explicit Event(bool signaled);
            ~Event();
            void Clear();
            void Signal();
            Result Wait(uint32 timeoutInMs);
        private:
            EventStorage m_event;
        };

        class Random
        {
        public:
            Random();
            ~Random();
            uint32 Generate();
            static uint32 Max();

        private:
            RandomStorage m_randState;
        };

        ProcessId GetProcessId();

        uint64 GetCurrentTimeInMs();

        // Todo: Remove Sleep() entirely from our platform API. It cannot be used in the KMD and should not be used
        // anywhere else either.
        void Sleep(uint32 millisecTimeout);

        void GetProcessName(char* buffer, size_t bufferSize);

        void Strncpy(char* pDst, const char* pSrc, size_t dstSize);

        void Snprintf(char* pDst, size_t dstSize, const char* format, ...);
        void Vsnprintf(char* pDst, size_t dstSize, const char* format, va_list args);
    }

#ifndef DD_PRINT_FUNC
#define DD_PRINT_FUNC Platform::DebugPrint
#else
    void DD_PRINT_FUNC(LogLevel logLevel, const char* format, ...);
#endif

    template <LogLevel logLevel = LogLevel::Info, class ...Ts>
    inline void LogString(const char *format, Ts&&... args)
    {
        if (DD_WILL_PRINT(logLevel))
        {
            DD_PRINT_FUNC(logLevel, format, Platform::Forward<Ts>(args)...);
        }
    }
}
