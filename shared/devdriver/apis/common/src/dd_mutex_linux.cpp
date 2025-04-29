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

#include <dd_mutex.h>
#include <dd_assert.h>
#include <errno.h>
#include <pthread.h>

namespace DevDriver
{

Mutex::Mutex() noexcept
    : m_osMutexData {}
{
    static_assert(sizeof(Mutex::m_osMutexData) >= sizeof(pthread_mutex_t));

    auto pMutex = reinterpret_cast<pthread_mutex_t*>(m_osMutexData);
    int err = pthread_mutex_init(pMutex, nullptr);
    DD_ASSERT(err == 0);
}

Mutex::~Mutex() noexcept
{
    auto pMutex = reinterpret_cast<pthread_mutex_t*>(m_osMutexData);
    int err = pthread_mutex_destroy(pMutex);
    DD_ASSERT(err == 0);
}

void Mutex::Lock()
{
    auto pMutex = reinterpret_cast<pthread_mutex_t*>(m_osMutexData);
    const int ret = pthread_mutex_lock(pMutex);
    DD_ASSERT(ret == 0);
}

bool Mutex::TryLock()
{
    auto pMutex = reinterpret_cast<pthread_mutex_t*>(m_osMutexData);
    const int ret = pthread_mutex_trylock(pMutex);
    DD_ASSERT((ret == 0) || (ret == EBUSY));
    return (ret == 0);
}

void Mutex::Unlock()
{
    auto pMutex = reinterpret_cast<pthread_mutex_t*>(m_osMutexData);
    const int ret = pthread_mutex_unlock(pMutex);
    DD_ASSERT(ret == 0);
}

RWLock::RWLock() noexcept
    : m_osLockData {}
{
    static_assert(sizeof(RWLock::m_osLockData) >= sizeof(pthread_rwlock_t));

    auto pLock = reinterpret_cast<pthread_rwlock_t*>(m_osLockData);
    int err = pthread_rwlock_init(pLock, nullptr);
    DD_ASSERT(err == 0);
}

RWLock::~RWLock() noexcept
{
    auto pLock = reinterpret_cast<pthread_rwlock_t*>(m_osLockData);
    int err = pthread_rwlock_destroy(pLock);
    DD_ASSERT(err == 0);
}

void RWLock::AcquireReadLock()
{
    auto pLock = reinterpret_cast<pthread_rwlock_t*>(m_osLockData);
    const int err = pthread_rwlock_rdlock(pLock);
    DD_ASSERT(err == 0);
}

void RWLock::ReleaseReadLock()
{
    auto pLock = reinterpret_cast<pthread_rwlock_t*>(m_osLockData);
    const int err = pthread_rwlock_unlock(pLock);
    DD_ASSERT(err == 0);
}

void RWLock::AcquireWriteLock()
{
    auto pLock = reinterpret_cast<pthread_rwlock_t*>(m_osLockData);
    const int err = pthread_rwlock_wrlock(pLock);
    DD_ASSERT(err == 0);
}

void RWLock::ReleaseWriteLock()
{
    auto pLock = reinterpret_cast<pthread_rwlock_t*>(m_osLockData);
    const int err = pthread_rwlock_unlock(pLock);
    DD_ASSERT(err == 0);
}

} // namespace DevDriver
