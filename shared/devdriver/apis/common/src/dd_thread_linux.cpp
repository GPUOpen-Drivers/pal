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
#include <dd_result.h>
#include <dd_thread.h>
#include <cstdlib>
#include <cstring>
#include <pthread.h>

namespace DevDriver
{

struct ThreadIdentifier
{
    pthread_t id;
};

Thread::Thread()
    : m_pThreadId {nullptr}
    , m_pThreadFn {nullptr}
    , m_pUserdata {nullptr}
{
}

Thread::Thread(Thread&& other) noexcept
    : m_pThreadId {other.m_pThreadId}
    , m_pThreadFn {other.m_pThreadFn}
    , m_pUserdata {other.m_pUserdata}
{
    other.m_pThreadId = 0;
    other.m_pThreadFn = nullptr;
    other.m_pUserdata = nullptr;
}

void Thread::operator=(Thread&& other) noexcept
{
    m_pThreadId = other.m_pThreadId;
    m_pThreadFn = other.m_pThreadFn;
    m_pUserdata = other.m_pUserdata;

    other.m_pThreadId = 0;
    other.m_pThreadFn = nullptr;
    other.m_pUserdata = nullptr;
}

Thread::~Thread()
{
    // Thread should be joined before being destroyed.
    DD_ASSERT(m_pThreadId == nullptr);
    if (m_pThreadId != nullptr)
    {
        free(m_pThreadId);
    }
}

DD_RESULT Thread::Start(ThreadFunction pThreadFn, void* pUserdata)
{
    if (pThreadFn == nullptr)
    {
        return DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    m_pThreadId = (ThreadIdentifier*)std::malloc(sizeof(*m_pThreadId));
    if (m_pThreadId == nullptr)
    {
        return DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
    }

    DD_RESULT result = DD_RESULT_SUCCESS;

    m_pThreadFn = pThreadFn;
    m_pUserdata = pUserdata;

    int err = pthread_create(&m_pThreadId->id, nullptr, ThreadFnShim, this);
    result = ResultFromErrno(err);

    return result;
}

DD_RESULT Thread::Join()
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    if (m_pThreadId != nullptr)
    {
        int err = pthread_join(m_pThreadId->id, nullptr);
        result = ResultFromErrno(err);

        m_pThreadFn = nullptr;
        m_pUserdata = nullptr;

        std::free(m_pThreadId);
        m_pThreadId = nullptr;
    }

    return result;
}

DD_RESULT Thread::SetDebugName(const char* pName)
{
    if ((m_pThreadId == nullptr) || (pName == nullptr))
    {
        return DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    const size_t NameBufferSize = 16;
    char nameBuf[NameBufferSize] {};
    std::strncpy(nameBuf, pName, NameBufferSize - 1);
    int err = pthread_setname_np(m_pThreadId->id, nameBuf);
    return ResultFromErrno(err);
}

void* Thread::ThreadFnShim(void* pThread)
{
    DevDriver::Thread* pThisThread = static_cast<DevDriver::Thread*>(pThread);
    pThisThread->m_pThreadFn(pThisThread->m_pUserdata);
    return nullptr;
}

} // namespace DevDriver
