/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palThread.h
 * @brief PAL utility collection Thread class declaration.
 ***********************************************************************************************************************
 */

#pragma once

#include <pthread.h>

#include "palUtil.h"

namespace Util
{

// Define portable thread ID
typedef pthread_t ThreadId;

// Function to check if two ThreadIds are equal.
inline bool ThreadIdEqual(ThreadId id1, ThreadId id2) { return pthread_equal(id1, id2); }

/**
 ***********************************************************************************************************************
 * @brief Platform-agnostic thread primitive.
 ***********************************************************************************************************************
 */
class Thread
{
public:
    Thread();
    ~Thread();

    /// Entrypoint into the thread.  When this function returns, the thread implicitly terminates.
    typedef void (*StartFunction)(void*);

    /// Starts a new thread which starts by running the specified function.
    ///
    /// When pfnFunction returns, the thread terminates.
    ///
    /// @param [in] pfnFunction Function to be run when the thread launches.
    /// @param [in] pParameter  Argument to be passed to pfnFunction.
    /// @param [in] priority    Priority adjustment for this thread.  This is an OS-specific value and should generally
    ///                         be left at its default value of 0.
    ///
    /// @returns @ref Success if the thread was successfully launched, @ref ErrorUnavailable if this object is currently
    ///          active due to a previous Begin() or SetToSelf() call, or @ref ErrorUnknown if an internal error occurs.
    Result Begin(StartFunction pfnFunction, void* pParameter = nullptr, uint32 priority = 0);

    /// Makes this Thread object represent the current thread of execution.
    ///
    /// @returns @ref Success if this Thread object was successfully initialized to correspond to the current thread,
    ///          or @ref ErrorUnavailable if this object is currently active due to a previous Begin() or SetToSelf()
    ///          call.
    Result SetToSelf();

    /// Waits for the this object's thread to finish executing.
    ///
    /// @warning Must not be called from this object's thread.
    void Join();

    /// Called to end this object's thread.
    ///
    /// @warning Must be called from this object's thread.
    [[noreturn]] void End();

    /// Returns true if the calling thread is this Thread object's thread.
    bool IsCurrentThread() const;

    /// Returns true if the calling thread is not this Thread object's thread.
    bool IsNotCurrentThread() const { return (IsCurrentThread() == false); }

    /// Returns true if the thread was created successfully
    bool IsCreated() const;

    /// Static method to get current thread ID, only useful to give to ThreadIdEqual().
    static ThreadId GetCurrentThreadId();

private:
    // Our platforms' internal start functions all return different types so we can't directly launch our client's
    // StartFunction. We must bootstrap each thread using an internal function which then calls the client's function.
    StartFunction m_pfnStartFunction;
    void*         m_pStartParameter;

#if   defined(__unix__)
    static void* StartThread(void* pThreadObject);

    // Linux/pthreads has no portable way of representing an 'invalid' thread ID, so we will simply store the result of
    // the pthread_create call used to spawn the thread.  This can then be used to determine if the thread is valid.
    Result        m_threadStatus;
    pthread_t     m_threadId;     // Pthreads thread identifier.
#endif

    PAL_DISALLOW_COPY_AND_ASSIGN(Thread);
};

#if   defined(__unix__)
/// Defines an opaque key, visible to all threads, that is used to store and retrieve data local to the current thread.
typedef pthread_key_t ThreadLocalKey;
#endif

/// Defines the destructor called when the thread exits.
typedef void (*ThreadLocalDestructor)(void*);

/// Creates a new key for this process to store and retrieve thread-local data.  It is a good idea to use a small
/// number of keys because some platforms may place low limits on the number of keys per process.
///
/// @param [in,out] pKey Pointer to the key being created.
/// @param [in] pDestructor Pointer to the destructor function.
///
/// @returns Success if the key was successfully created.  Otherwise, one of the following error codes may be returned.
///          + ErrorInvalidPointer if pKey is null.
///          + ErrorUnavailable if no more keys can be created.
extern Result CreateThreadLocalKey(ThreadLocalKey* pKey, ThreadLocalDestructor pDestructor = nullptr);

/// Deletes a key that was previously created by @ref CreateThreadLocalKey.  It is the caller's responsibility to free
/// any thread-local dynamic allocations stored at this key.  The key is considered invalid after the call returns.
///
/// @param [in] key The key to delete.
///
/// @returns Success if the key was successfully deleted.
extern Result DeleteThreadLocalKey(ThreadLocalKey key);

/// Gets the value that the current thread has associated with the given key or null if no value has been set.
///
/// @param [in] key Look up data for this key.
///
/// @warning Calling this function with an invalid key results in undefined behavior.
///
/// @returns the value associated with the given key or null if no value has been set.
extern void* GetThreadLocalValue(ThreadLocalKey key);

/// Sets the value that the current thread has associated with the given key.
///
/// @param [in] key    The key that will have its value set for the current thread.
/// @param [in] pValue The value to associate with the key.
///
/// @warning Calling this function with an invalid key results in undefined behavior.
///
/// @returns Success if the value was successfully associated with the key.
extern Result SetThreadLocalValue(ThreadLocalKey key, void* pValue);

} // Util
