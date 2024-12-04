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

#include <dd_mutex.h>
#include <Windows.h>

namespace DevDriver
{

Mutex::Mutex() noexcept
    : m_osMutexData {}
{
    // We use SRWLock for our mutex implementation based on the following considerations:
    // - According to Microsoft, SRWLock is must faster than CRITICL_SECTION regardless of high or low contention.
    // - SRWLock (8 bytes) takes up significantly lower memory compared to CRITICAL_SECTION (40 bytes).
    // - SRWLock is non-recurisve, whereas CRITICAL_SECTION can be locked recursively.
    static_assert(sizeof(Mutex::m_osMutexData) >= sizeof(SRWLOCK));

    auto pWinLock = reinterpret_cast<SRWLOCK*>(m_osMutexData);
    InitializeSRWLock(pWinLock);
}

Mutex::~Mutex() noexcept
{
    // No destroy function for SRWLock.
}

void Mutex::Lock()
{
    auto pWinLock = reinterpret_cast<SRWLOCK*>(m_osMutexData);
    AcquireSRWLockExclusive(pWinLock);
}

bool Mutex::TryLock()
{
    auto pWinMutex = reinterpret_cast<SRWLOCK*>(m_osMutexData);
    return TryAcquireSRWLockExclusive(pWinMutex);
}

void Mutex::Unlock()
{
    auto pWinMutex = reinterpret_cast<SRWLOCK*>(m_osMutexData);
    ReleaseSRWLockExclusive(pWinMutex);
}

RWLock::RWLock() noexcept
    : m_osLockData {}
{
    static_assert(sizeof(RWLock::m_osLockData) >= sizeof(SRWLOCK));

    auto pWinRWLock = reinterpret_cast<SRWLOCK*>(m_osLockData);
    InitializeSRWLock(pWinRWLock);
}

RWLock::~RWLock() noexcept
{
    // no destroy function for Win32 SRWLock.
}

void RWLock::AcquireReadLock()
{
    auto pWinRWLock = reinterpret_cast<SRWLOCK*>(m_osLockData);
    AcquireSRWLockShared(pWinRWLock);
}

void RWLock::ReleaseReadLock()
{
    auto pWinRWLock = reinterpret_cast<SRWLOCK*>(m_osLockData);
    ReleaseSRWLockShared(pWinRWLock);
}

void RWLock::AcquireWriteLock()
{
    auto pWinRWLock = reinterpret_cast<SRWLOCK*>(m_osLockData);
    AcquireSRWLockExclusive(pWinRWLock);
}

void RWLock::ReleaseWriteLock()
{
    auto pWinRWLock = reinterpret_cast<SRWLOCK*>(m_osLockData);
    ReleaseSRWLockExclusive(pWinRWLock);
}

} // namespace DevDriver

