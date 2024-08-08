/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <dd_assert.h>
#include <dd_thread.h>
#include <dd_result.h>
#include <cerrno>
#include <cstdlib>
#include <process.h>
#include <Windows.h>

namespace DevDriver
{

struct ThreadIdentifier
{
    HANDLE id;
};

Thread::~Thread()
{
    // Thread should be joined before being destroyed.
    DD_ASSERT(m_pThreadId == nullptr);
    if (m_pThreadId != nullptr)
    {
        std::free(m_pThreadId);
    }
}

DD_RESULT Thread::Start(ThreadFunction pThreadFn, void* pUserdata)
{
    if (pThreadFn == nullptr)
    {
        return DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    if (m_pThreadId != nullptr)
    {
        // Previously started thread is still running.
        return DD_RESULT_COMMON_ALREADY_EXISTS;
    }

    m_pThreadId = (ThreadIdentifier*)std::malloc(sizeof(*m_pThreadId));
    if (m_pThreadId == nullptr)
    {
        return DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
    }

    DD_RESULT result = DD_RESULT_SUCCESS;

    m_pThreadFn = pThreadFn;
    m_pUserdata = pUserdata;

    uintptr_t threadHandle = _beginthreadex(
        nullptr,      // security attributes
        0,            // stack size, use default size
        ThreadFnShim, // thread starting function
        this,         // user data passed to ThreadFnShim
        0,            // thread runs immediately
        nullptr);

    if ((threadHandle == 0) || (threadHandle == (uintptr_t)-1L))
    {
        result = ResultFromErrno(errno);
    }
    else
    {
        m_pThreadId->id = reinterpret_cast<HANDLE>(threadHandle);
    }

    if (result != DD_RESULT_SUCCESS)
    {
        std::free(m_pThreadId);
        m_pThreadId = nullptr;
    }

    return result;
}

DD_RESULT Thread::Join()
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    if (m_pThreadId != nullptr)
    {
        DWORD waitResult = WaitForSingleObject(m_pThreadId->id, INFINITE);
        result = (waitResult == WAIT_OBJECT_0) ? DD_RESULT_SUCCESS : DD_RESULT_COMMON_UNKNOWN;

        CloseHandle(m_pThreadId->id);

        m_pThreadFn = nullptr;
        m_pUserdata = nullptr;

        std::free(m_pThreadId);
        m_pThreadId = nullptr;
    }

    return result;
}

DD_RESULT Thread::SetDebugName(const char* pName)
{
    if (pName == nullptr)
    {
        return DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    if (m_pThreadId == nullptr)
    {
        return DD_RESULT_COMMON_DOES_NOT_EXIST;
    }

    DD_RESULT result = DD_RESULT_SUCCESS;

    const uint32_t UTF16_BUFFER_SIZE = 128;
    wchar_t utf16Buffer[UTF16_BUFFER_SIZE] {};

    size_t convertedNum = 0;
    errno_t err = mbstowcs_s(&convertedNum, utf16Buffer, UTF16_BUFFER_SIZE, pName, UTF16_BUFFER_SIZE - 1);
    result = ResultFromErrno(err);

    if (result == DD_RESULT_SUCCESS)
    {
        HRESULT hr = SetThreadDescription(m_pThreadId->id, utf16Buffer);
        if (FAILED(hr))
        {
            result = DD_RESULT_COMMON_UNKNOWN;
        }
    }

    return result;
}

uint32_t __stdcall Thread::ThreadFnShim(void* pThread)
{
    DevDriver::Thread* pThisThread = static_cast<DevDriver::Thread*>(pThread);
    pThisThread->m_pThreadFn(pThisThread->m_pUserdata);
    return 0;
}

} // namespace DevDriver

