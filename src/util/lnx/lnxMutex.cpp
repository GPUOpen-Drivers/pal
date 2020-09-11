/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palMutex.h"
#include "palSysMemory.h"
#include <errno.h>
#include <sched.h>

namespace Util
{

// =====================================================================================================================
// Frees the pthreads mutex this object encapsulates.
Mutex::~Mutex()
{
    if (m_initialized)
    {
        const int ret = pthread_mutex_destroy(&m_osMutex);
        PAL_ASSERT(ret == 0);
    }
}

// =====================================================================================================================
// Initializes the pthreads mutex this object encapsulates.
Result Mutex::Init()
{
    m_initialized = m_initialized || (pthread_mutex_init(&m_osMutex, nullptr) == 0);

    return m_initialized ? Result::Success : Result::ErrorUnknown;
}

// =====================================================================================================================
// Acquires the mutex if it is not contended.  If it is contended, waits for the mutex to become available, then
// acquires it.
void Mutex::Lock()
{
    const int ret = pthread_mutex_lock(&m_osMutex);
    PAL_ASSERT(ret == 0);
}

// =====================================================================================================================
// Acquires the mutex if it is not contended.  Does not wait for the mutex to become available if it is contended.
// Returns true if the mutex was successfully acquired.
bool Mutex::TryLock()
{
    const int ret = pthread_mutex_trylock(&m_osMutex);
    PAL_ASSERT((ret == 0) || (ret == EBUSY));

    return (ret == 0);
}

// =====================================================================================================================
// Releases the mutex.
void Mutex::Unlock()
{
    const int ret = pthread_mutex_unlock(&m_osMutex);
    PAL_ASSERT(ret == 0);
}

// =====================================================================================================================
// Initializes pthread rwlock.
Result RWLock::Init()
{
    m_initialized = m_initialized || (pthread_rwlock_init(&m_osRWLock, nullptr) == 0);

    return m_initialized ? Result::Success : Result::ErrorUnknown;
}

// =====================================================================================================================
// Destroys pthread rwlock.
RWLock::~RWLock()
{
    if (m_initialized)
    {
        const int ret = pthread_rwlock_destroy(&m_osRWLock);
        PAL_ASSERT(ret == 0);
    }
}

// =====================================================================================================================
// Acquires a rw lock in readonly mode if it is not contended in readwrite mode.
// If it is contended, wait for rw lock to become available, then enter it.
void RWLock::LockForRead()
{
    const int ret = pthread_rwlock_rdlock(&m_osRWLock);
    PAL_ASSERT(ret == 0);
}

// =====================================================================================================================
// Acquires a rw lock in readwrite mode if it is not contended.
// If it is contended, wait for rw lock to become available, then enter it.
void RWLock::LockForWrite()
{
    const int ret = pthread_rwlock_wrlock(&m_osRWLock);
    PAL_ASSERT(ret == 0);
}

// =====================================================================================================================
// Tries to acquire a rw lock in readonly mode if it is not contended in readwrite mode.
// Does not wait for the rw lock to become available.
bool RWLock::TryLockForRead()
{
    const int ret = pthread_rwlock_tryrdlock(&m_osRWLock);
    PAL_ASSERT((ret == 0) || (ret == EBUSY));

    return (ret == 0);
}

// =====================================================================================================================
// Tries to acquire a rw lock in readonly mode if it is not contended.
// Does not wait for the rw lock to become available.
bool RWLock::TryLockForWrite()
{
    const int ret = pthread_rwlock_trywrlock(&m_osRWLock);
    PAL_ASSERT((ret == 0) || (ret == EBUSY));

    return (ret == 0);
}

// =====================================================================================================================
// Release the rw lock which is previously contended.
void RWLock::UnlockForRead()
{
    const int ret = pthread_rwlock_unlock(&m_osRWLock);
    PAL_ASSERT(ret == 0);
}

// =====================================================================================================================
// Release the rw lock which is previously contended.
void RWLock::UnlockForWrite()
{
    const int ret = pthread_rwlock_unlock(&m_osRWLock);
    PAL_ASSERT(ret == 0);
}

// =====================================================================================================================
// Yields the current thread to another thread in the ready state (if available).
void YieldThread()
{
    sched_yield();
}

// =====================================================================================================================
// Thread-safe method to write a 64-bit value, using relaxed memory ordering.
void AtomicWriteRelaxed64(
    volatile uint64* pTarget,
    uint64 newValue)
{
    // The variable pointed to by the Addend parameter must be aligned on a 64 - bit boundary;
    // otherwise, this function will behave unpredictably on multiprocessor x86 systems and any non - x86 systems.
    PAL_ASSERT(IsPow2Aligned(reinterpret_cast<size_t>(pTarget), sizeof(uint64)));

#ifdef __x86_64__
    // This is atomic on x64 CPUs.
    *pTarget = newValue;
#else
    __atomic_store_n(pTarget, newValue, __ATOMIC_RELAXED);
#endif
}

// =====================================================================================================================
// Thread-safe method to read a 64-bit value, using relaxed memory ordering.
uint64 AtomicReadRelaxed64(
    const volatile uint64* pTarget)
{
    // The variable pointed to by the Addend parameter must be aligned on a 64 - bit boundary;
    // otherwise, this function will behave unpredictably on multiprocessor x86 systems and any non - x86 systems.
    PAL_ASSERT(IsPow2Aligned(reinterpret_cast<size_t>(pTarget), sizeof(uint64)));

#ifdef __x86_64__
    // This is atomic on x64 CPUs.
    return *pTarget;
#else
    return __atomic_load_n(pTarget, __ATOMIC_RELAXED);
#endif
}

// =====================================================================================================================
// Atomically increments a 32-bit unsigned integer, returning the new value.
uint32 AtomicIncrement(
    volatile uint32* pValue)
{
    // The variable pointed to by the pValue parameter must be aligned on a 32-bit boundary; otherwise, this function
    // will behave unpredictably on multiprocessor x86 systems and any non-x86 systems.
    PAL_ASSERT(IsPow2Aligned(reinterpret_cast<size_t>(pValue), sizeof(uint32)));

    return __sync_add_and_fetch(pValue, 1U);
}

// =====================================================================================================================
// Atomically increments a 64-bit unsigned integer, returning the new value.
uint64 AtomicIncrement64(
    volatile uint64* pValue)
{
    // The variable pointed to by the pValue parameter must be aligned on a 64-bit boundary; otherwise, this function
    // will behave unpredictably on multiprocessor x86 systems and any non-x86 systems.
    PAL_ASSERT(IsPow2Aligned(reinterpret_cast<size_t>(pValue), sizeof(uint64)));

    return __sync_add_and_fetch(pValue, 1ULL);
}

// =====================================================================================================================
// Atomically decrements a 32-bit unsigned integer, returning the new value.
uint32 AtomicDecrement(
    volatile uint32* pValue)
{
    // The variable pointed to by the pValue parameter must be aligned on a 32-bit boundary; otherwise, this function
    // will behave unpredictably on multiprocessor x86 systems and any non-x86 systems
    PAL_ASSERT(IsPow2Aligned(reinterpret_cast<size_t>(pValue), sizeof(uint32)));

    return __sync_sub_and_fetch(pValue, 1U);
}

// =====================================================================================================================
// Thread-safe method to compare and swap two 32-bit values.
// Returns the value at (*pTarget) before this method was called.
uint32 AtomicCompareAndSwap(
    volatile uint32* pTarget,
    uint32           oldValue,
    uint32           newValue)
{
    PAL_ASSERT(IsPow2Aligned(reinterpret_cast<size_t>(pTarget), sizeof(uint32)));

    return __sync_val_compare_and_swap(pTarget, oldValue, newValue);
}

// =====================================================================================================================
// Thread-safe method to exchange a 32-bit integer.  Returns the value at (*pTarget) before this method was called.
uint32 AtomicExchange(
    volatile uint32* pTarget,
    uint32           value)
{
    return __sync_lock_test_and_set(pTarget, value);
}

// =====================================================================================================================
// Thread-safe method to exchange a 64-bit integer.  Returns the value at (*pTarget) before this method was called.
uint64 AtomicExchange64(
    volatile uint64* pTarget,
    uint64           value)
{
    return __sync_lock_test_and_set(pTarget, value);
}

// =====================================================================================================================
// Thread-safe method to exchange a pointer value.  Returns the value at (*ppTarget) before this method was called.
void* AtomicExchangePointer(
    void*volatile* ppTarget,
    void*          pValue)
{
    PAL_ASSERT(IsPow2Aligned(reinterpret_cast<size_t>(pValue), sizeof(void*)));

    return __sync_lock_test_and_set(ppTarget, pValue);
}

// =====================================================================================================================
// Atomically add two 32-bit integers, returning the result of the addition.
uint32 AtomicAdd(
    volatile uint32* pAddend,
    uint32           value)
{
    return __sync_add_and_fetch(pAddend, value);
}

// =====================================================================================================================
// Atomically add two 64-bit integers, returning the result of the addition.
uint64 AtomicAdd64(
    volatile uint64* pAddend,
    uint64           value)
{
    return __sync_add_and_fetch(pAddend, value);
}

// =====================================================================================================================
// Atomically OR two 32-bit integers, returning the original value.
uint32 AtomicOr(
    volatile uint32* pTarget,
    uint32           value)
{
    return __sync_fetch_and_or(pTarget, value);
}

// =====================================================================================================================
// Atomically OR two 64-bit integers, returning the original value.
uint64 AtomicOr64(
    volatile uint64* pTarget,
    uint64           value)
{
    return __sync_fetch_and_or(pTarget, value);
}

// =====================================================================================================================
// Atomically AND two 32-bit integers, returning the original value.
uint32 AtomicAnd(
    volatile uint32* pTarget,
    uint32           value)
{
    return __sync_fetch_and_and(pTarget, value);
}

// =====================================================================================================================
// Atomically OR two 64-bit integers, returning the original value.
uint64 AtomicAnd64(
    volatile uint64* pTarget,
    uint64           value)
{
    return __sync_fetch_and_and(pTarget, value);
}

} // Util
