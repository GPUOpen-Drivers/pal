/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

    #include <cerrno>
    #include <pthread.h>

#include "dd_assert.h"

namespace DevDriver
{

class Mutex
{
private:
    using MutexData = pthread_mutex_t;

    // Opaque structure to the OS-specific mutex data.
    MutexData m_osMutex;

public:
    Mutex() noexcept : m_osMutex {} { pthread_mutex_init(&m_osMutex, nullptr); }
    ~Mutex() { pthread_mutex_destroy(&m_osMutex); };

    /// Enters the critical section if it is not contended.  If it is contended, wait for the critical section to become
    /// available, then enter it.
    void Lock()
    {
        const int ret = pthread_mutex_lock(&m_osMutex);
        DD_ASSERT(ret == 0);
    }

    /// Enters the critical section if it is not contended.  Does not wait for the critical section to become available
    /// if it is contended.
    ///
    /// @returns True if the critical section was entered, false otherwise.
    bool TryLock()
    {
        const int ret = pthread_mutex_trylock(&m_osMutex);
        DD_ASSERT((ret == 0) || (ret == EBUSY));
        return (ret == 0);
    }

    /// Leaves the critical section.
    void Unlock()
    {
        const int ret = pthread_mutex_unlock(&m_osMutex);
        DD_ASSERT(ret == 0);
    }

private:
    Mutex(Mutex&&) = delete;
    Mutex(const Mutex&) = delete;
    Mutex& operator=(Mutex&&) = delete;
    Mutex& operator=(const Mutex&) = delete;
};

/// The class LockGuard is a mutex wrapper that provides a convenient RAII-style mechanism for owning a
/// \ref Mutex for the duration of a scoped block.
class LockGuard
{
private:
    Mutex& m_mutex;

public:
    /// Lock the given mutex.
    explicit LockGuard(Mutex& mutex)
        : m_mutex(mutex)
    {
        m_mutex.Lock();
    }

    /// Release the mutex.
    ~LockGuard()
    {
        m_mutex.Unlock();
    }

private:
    LockGuard(LockGuard&&) = delete;
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(LockGuard&&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
};

class RWLock
{
private:
    using RWLockData = pthread_rwlock_t;

    RWLockData m_osRWLock;

public:
    enum LockType
    {
        Read = 0, // Shared for multiple read accesses.
        Write     // Exclusive for one read/write access.
    };

    RWLock() noexcept : m_osRWLock {} { pthread_rwlock_init(&m_osRWLock, nullptr); }
    ~RWLock() noexcept { pthread_rwlock_destroy(&m_osRWLock); };

    void AcquireReadLock()
    {
        const int err = pthread_rwlock_rdlock(&m_osRWLock);
        DD_ASSERT(err == 0);
    }

    void ReleaseReadLock()
    {
        const int err = pthread_rwlock_unlock(&m_osRWLock);
        DD_ASSERT(err == 0);
    }

    void AcquireWriteLock()
    {
        const int err = pthread_rwlock_wrlock(&m_osRWLock);
        DD_ASSERT(err == 0);
    }

    void ReleaseWriteLock()
    {
        const int err = pthread_rwlock_unlock(&m_osRWLock);
        DD_ASSERT(err == 0);
    }

private:
    RWLock(RWLock&&) = delete;
    RWLock(const RWLock&) = delete;
    RWLock& operator=(RWLock&&) = delete;
    RWLock& operator=(const RWLock&) = delete;
};

template<RWLock::LockType L>
class RWLockGuard
{
private:
    RWLock& m_rwLock;

public:
    explicit RWLockGuard(RWLock& lock)
        : m_rwLock(lock)
    {
        if (L == RWLock::LockType::Read)
        {
            m_rwLock.AcquireReadLock();
        }
        else
        {
            m_rwLock.AcquireWriteLock();
        }
    }

    ~RWLockGuard()
    {
        if (L == RWLock::LockType::Read)
        {
            m_rwLock.ReleaseReadLock();
        }
        else
        {
            m_rwLock.ReleaseWriteLock();
        }
    }

private:
    RWLockGuard(RWLockGuard&&) = delete;
    RWLockGuard(const RWLockGuard&) = delete;
    RWLockGuard& operator=(RWLockGuard&&) = delete;
    RWLockGuard& operator=(const RWLockGuard&) = delete;
};

} // namespace DevDriver
