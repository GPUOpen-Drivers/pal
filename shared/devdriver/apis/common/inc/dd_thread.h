/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <dd_common_api.h>
#include <dd_assert.h>
#include <cstdint>

namespace DevDriver
{

typedef void (*ThreadFunction)(void* pUserdata);

struct ThreadIdentifier;

class Thread
{
private:
    ThreadIdentifier* m_pThreadId;

    ThreadFunction m_pThreadFn; // user passed function to be executed
    void*          m_pUserdata; // passed to `m_pThreadFn`

public:
    Thread()
        : m_pThreadId {nullptr}
        , m_pThreadFn {nullptr}
        , m_pUserdata {nullptr}
    {}

    Thread(Thread&& other) noexcept
        : m_pThreadId {other.m_pThreadId}
        , m_pThreadFn {other.m_pThreadFn}
        , m_pUserdata {other.m_pUserdata}
    {
        other.m_pThreadId = nullptr;
        other.m_pThreadFn = nullptr;
        other.m_pUserdata = nullptr;
    }

    void operator=(Thread&& other) noexcept
    {
        m_pThreadId = other.m_pThreadId;
        m_pThreadFn = other.m_pThreadFn;
        m_pUserdata = other.m_pUserdata;

        other.m_pThreadId = nullptr;
        other.m_pThreadFn = nullptr;
        other.m_pUserdata = nullptr;
    }

    ~Thread();

    /// Start executing a thread.
    ///
    /// @param[in] pThreadFn Function to start the thread with.
    /// @param[in] pUserdata Argument passed to \param pThreadFn when invoked.
    /// @return DD_RESULT_COMMON_INVALID_PARAMETER if \param pThreadFn is nullptr.
    /// @return other errors.
    DD_RESULT Start(ThreadFunction pThreadFn, void* pUserdata);

    /// Blocks indefinitely until the thread terminates.
    ///
    /// @return DD_RESULT_SUCCESS the thread terminated successfully. Note, this return value does NOT necessarily mean
    /// the thread function ran successfully.
    /// @return other errors.
    DD_RESULT Join();

    /// Set debug name of the thread.
    ///
    /// @param[in] pName Pointer to a null-terminated string. On Linux \param pName is truncated to be maximumly 16
    /// bytes including null-terminator.
    DD_RESULT SetDebugName(const char* pName);

private:
    // ThreadFnShim is an extra layer of indirection to converge different thread function signatures on various
    // platforms.
#if defined(DD_TARGET_PLATFORM_WINDOWS)
    static uint32_t __stdcall ThreadFnShim(void* pThread);
#elif defined(DD_TARGET_PLATFORM_LINUX)
    static void* ThreadFnShim(void* pThread);
#else
#error Unsupported platform
#endif

    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;
};

} // namespace DevDriver

