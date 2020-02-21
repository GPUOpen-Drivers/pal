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
* @file  ddcWinKernelPlatform.h
* @brief Windows Kernel Platform Layer Header
***********************************************************************************************************************
*/
#pragma once

// /analysis has issues with some parts of Microsoft's headers
#include <codeanalysis\warnings.h>
#pragma warning(push)
#pragma warning (disable : ALL_CODE_ANALYSIS_WARNINGS)
#include <ntddk.h>
#pragma warning(pop)

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
// [MSVC] __assume()
//
// [C++] Contracts: expects, ensures, assert, axiom
#define DD_ASSUME(expression) __assume((expression))

// Dummy delete operator required to support kernel mode builds with C++14 and newer
// As far as I know, this is required because we use placement new in the kernel code which causes the compiler to
// look for the global new and delete operators even if they aren't used. Since we're compiling with /kernel, these
// operators are defined by the runtime so we define them manually here to fix the issue even though they should
// never be called. Also, The _cdecl part is actually required, defining this function without it will result
// in linker errors.
void _cdecl operator delete(void* pMemory, size_t size);

namespace DevDriver
{
    namespace Platform
    {
        /* platform functions for performing atomic operations */
        typedef volatile long Atomic;
        DD_CHECK_SIZE(Atomic, sizeof(int32));

        struct MutexStorage
        {
            FAST_MUTEX mutex;
#if !defined(NDEBUG)
            Atomic     lockCount;
#endif
        };
        typedef KSEMAPHORE SemaphoreStorage;
        typedef KEVENT EventStorage;
        typedef HANDLE ThreadHandle;
        typedef void   ThreadReturnType;

        // Libraries should never be used in the kernel but we need to define the handle type so we don't get compile
        // errors from the platform headers. The library implementation should remain undefined so we'll get linker
        // errors if someone attempts to use it.
        typedef void* LibraryHandle;

        constexpr ThreadHandle kInvalidThreadHandle = NULL;

        // Maximum supported size for thread names, including NULL byte
        // This exists because some platforms have hard limits on thread name size.
        // Thread naming isn't currently supported in the Windows Kernel platform so we just use
        // the regular max size defined by the Windows Usermode platform.
        static constexpr size_t kThreadNameMaxLength = 64;

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
        }
    }
}
