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

#if defined(_KERNEL_MODE)
static_assert(false, "This header is for user mode windows, and it does not work in kernel mode.");
#endif

// Our code expects these defined before including Windows.h.
// However, we need to guard against clients defining them too.
#ifndef _CRT_RAND_S
    #define _CRT_RAND_S
#endif

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
    #define NOMINMAX
#endif

// WIN32_NO_STATUS makes Windows.h not include macro definitions from winnt.h
// which collide with those from ntstatus.h. This avoids compilation errors
// when other files that include ntstatus.h also include this file.
#define WIN32_NO_STATUS
#include <Windows.h>
#undef WIN32_NO_STATUS

#include <intrin.h>

#define DD_RESTRICT __restrict

#define DD_DEBUG_BREAK() __debugbreak()

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
// [MSVC] __assume()
//  ~ https://github.com/MicrosoftDocs/cpp-docs/blob/master/docs/intrinsics/assume.md
#define DD_ASSUME(expression) __assume((expression))

namespace DevDriver
{
    namespace Platform
    {
        /* platform functions for performing atomic operations */
        typedef volatile LONG Atomic;
        DD_CHECK_SIZE(Atomic, sizeof(int32));

        typedef volatile LONG64 Atomic64;
        DD_CHECK_SIZE(Atomic64, sizeof(int64));

        struct EmptyStruct {};

        struct MutexStorage
        {
            CRITICAL_SECTION criticalSection;
#if !defined(NDEBUG)
            Atomic           lockCount;
#endif
        };
        typedef Handle SemaphoreStorage;
        typedef HANDLE EventStorage;
        typedef HANDLE ThreadHandle;
        typedef DWORD  ThreadReturnType;
        typedef HMODULE LibraryHandle;

        constexpr ThreadHandle kInvalidThreadHandle = NULL;

        // Maximum supported size for thread names, including NULL byte
        // This exists because some platforms have hard limits on thread name size.
        // Windows doesn't seem to have a thread name size limit, but we use this variable to control
        // a formatting buffer as well and we want to keep it reasonably small since it's stack allocated.
        static constexpr size_t kThreadNameMaxLength = 64;

        #define DD_APIENTRY APIENTRY

        namespace Windows
        {
            // Windows specific functions required for in-memory communication
            Handle CreateSharedSemaphore(uint32 initialCount, uint32 maxCount);
            Handle CopySemaphoreFromProcess(ProcessId processId, Handle hObject);
            Result SignalSharedSemaphore(Handle pSemaphore);
            Result WaitSharedSemaphore(Handle pSemaphore, uint32 millisecTimeout);
            void CloseSharedSemaphore(Handle pSemaphore);

            Handle CreateSharedBuffer(Size bufferSizeInBytes);
            void CloseSharedBuffer(Handle hSharedBuffer);

            Handle MapSystemBufferView(Handle hBuffer, Size bufferSizeInBytes);
            Handle MapProcessBufferView(Handle hBuffer, ProcessId processId);
            void UnmapBufferView(Handle hSharedBuffer, Handle hSharedBufferView);

            // Whether or not the user has enabled Windows Developer Mode on their system
            // See: https://github.com/MicrosoftDocs/windows-uwp/blob/docs/hub/apps/get-started/enable-your-device-for-development.md
            bool IsWin10DeveloperModeEnabled();
        }
    }
}
