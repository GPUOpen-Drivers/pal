/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palAssert.h"
#include <errno.h>
#include <pthread.h>
#include "palThread.h"
#include "palInlineFuncs.h"

namespace Util
{

// =====================================================================================================================
// Static method to get current thread ID, only useful to give to ThreadIdEqual().
ThreadId Thread::GetCurrentThreadId()
{
    return pthread_self();
}

// =====================================================================================================================
Thread::Thread()
    :
    m_pfnStartFunction(nullptr),
    m_pStartParameter(nullptr),
    m_threadStatus(Result::ErrorUnknown),
    m_threadId(0)
{
}

// =====================================================================================================================
Thread::~Thread()
{
    if (m_threadStatus == Result::Success)
    {
        // If the thread is still active, or no one called Join() on it, we need to release the thread's system
        // resources.  The call below will release the resources of a thread which has already terminated, or it will
        // cause the thread to release its own resources upon termination.
        const int32 result = pthread_detach(m_threadId);
        PAL_ASSERT(result == 0);

        m_threadStatus = Result::ErrorUnknown; // Mark the thread as no longer valid.
    }
}

// =====================================================================================================================
// Starts a new thread which runs the specified function. The pointer pParameter will be passed to the spawned thread
// as a parameter into pfnFunction. When pfnFunction returns, the thread terminates.
Result Thread::Begin(
    StartFunction pfnFunction,
    void*         pParameter,
    uint32        priority)
{
    Result result = Result::ErrorUnavailable;

    if (m_threadStatus == Result::ErrorUnknown)
    {
        // First, create a thread attributes object so we can specify the thread priority.
        pthread_attr_t attr;
        if (pthread_attr_init(&attr) == 0)
        {
            // Query the default scheduling parameters, because we only want to override the priority.
            sched_param schedParam = { };
            int32 ret = pthread_attr_getschedparam(&attr, &schedParam);
            PAL_ASSERT(ret == 0);

            schedParam.sched_priority = priority;
            if (pthread_attr_setschedparam(&attr, &schedParam) == 0)
            {
                // Finally, we can attempt to spawn our thread.
                m_pfnStartFunction = pfnFunction;
                m_pStartParameter  = pParameter;

                if (pthread_create(&m_threadId, &attr, &StartThread, this) == 0)
                {
                    result = Result::Success;
                }
            }

            ret = pthread_attr_destroy(&attr);
            PAL_ASSERT(ret == 0);
        }

        m_threadStatus = result;
    }

    return result;
}

// =====================================================================================================================
/// Returns true if the thread was created successfully
bool Thread::IsCreated() const
{
    return m_threadStatus == Result::Success;
}

// =====================================================================================================================
/// Assigns a name to a thread
Result Thread::SetThreadName(
    const char* pName
    ) const
{
    // pthread_setname_np restricts to 16 char, including the terminating null byte
    char tmp[16] = { };
    Util::Strncpy(tmp, pName, 15);
    int err = pthread_setname_np(m_threadId, tmp);

    return (err == 0) ? Result::Success : Result::ErrorUnknown;
}

// =====================================================================================================================
// Makes this Thread object represent the calling thread.
Result Thread::SetToSelf()
{
    Result result = Result::ErrorUnavailable;

    if (m_threadStatus == Result::ErrorUnknown)
    {
        // Note: It is important to not set the status to Success here, since we do not want to call a pthread_detach
        // or pthread_join on a thread which we do not own.
        m_threadStatus = Result::Unsupported;
        m_threadId     = pthread_self();
        result         = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Waits for the thread to finish executing. Must not be called on this thread which this object encapsulates.
void Thread::Join()
{
    PAL_ASSERT(IsNotCurrentThread());

    if (m_threadStatus == Result::Success)
    {
        const int32 result = pthread_join(m_threadId, nullptr);
        PAL_ASSERT(result == 0);

        m_threadStatus = Result::ErrorUnknown; // Mark the thread as no longer valid.
    }
}

// =====================================================================================================================
// Called when the thread wants to exit.  It must be called on the thread which this object encapsulates.
void Thread::End()
{
    PAL_ASSERT(IsCurrentThread());

    pthread_exit(nullptr);

    // We should never get here...
    PAL_NEVER_CALLED();
}

// =====================================================================================================================
// Determines whether or not the calling thread is the thread encapsulated by this object.
bool Thread::IsCurrentThread() const
{
    return (pthread_equal(pthread_self(), m_threadId) != 0);
}

// =====================================================================================================================
// Bootstraps the given thread object by launching the client's start function.
void* Thread::StartThread(
    void* pThreadObject)
{
    auto*const pThis = static_cast<Thread*>(pThreadObject);

    pThis->m_pfnStartFunction(pThis->m_pStartParameter);

    return nullptr;
}

// =====================================================================================================================
// Creates a new key for this process to store and retrieve thread-local data.
Result CreateThreadLocalKey(
    ThreadLocalKey*       pKey,
    ThreadLocalDestructor pDestructor)
{
    Result result = Result::Success;

    if (pKey == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (pthread_key_create(pKey, pDestructor) != 0)
    {
        result = Result::ErrorUnavailable;
    }

    return result;
}

// =====================================================================================================================
// Deletes a key that was previously created by CreateThreadLocalKey.
Result DeleteThreadLocalKey(
    ThreadLocalKey key)
{
    return (pthread_key_delete(key) == 0) ? Result::Success : Result::ErrorUnknown;
}

// =====================================================================================================================
// Gets the value that the current thread has associated with the given key or null if no value has been set.
void* GetThreadLocalValue(
    ThreadLocalKey key)
{
    return pthread_getspecific(key);
}

// =====================================================================================================================
// Sets the value that the current thread has associated with the given key.
Result SetThreadLocalValue(
    ThreadLocalKey key,
    void*          pValue)
{
    return (pthread_setspecific(key, pValue) == 0) ? Result::Success : Result::ErrorUnknown;
}

} // Util
