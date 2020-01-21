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
* @file  ddcKmdWinPlatform.cpp
* @brief Kernel mode Windows platform layer implementation
***********************************************************************************************************************
*/

#include <ddPlatform.h>

// /analysis has issues with some parts of Microsoft's headers
#include <codeanalysis\warnings.h>
#pragma warning(push)
#pragma warning (disable : ALL_CODE_ANALYSIS_WARNINGS)
#include <ntstrsafe.h>
#pragma warning(pop)

// Each define (in this order) sanity checks something:
//      1) We're compiling as kernel mode for real
//      2) Our Windows KM platform is setup correctly
//      3) Our generic "is km" macro is setup correctly
// This is at worst redundant, but when this is wrong it can save us half an hour of debugging the build system
#error "This file must be compiled for kernel-mode."

#include <winKernel/ddIoCtlDefines.h>

#include <stdio.h>
#include <stdlib.h>

// Dummy delete operator required to support kernel mode builds with C++14 and newer
void _cdecl operator delete(void* pMemory, size_t size)
{
    DD_ASSERT_REASON("The default new/delete operators should never be called in kernel mode code!");

    DD_UNUSED(pMemory);
    DD_UNUSED(size);
}

namespace DevDriver
{
    namespace Platform
    {
        DD_STATIC_CONST const char* kKmdModuleName = "AmdFineWine.sys";

        // Helper function to convert time in ms to the 100ns unit used by the kernel wait functions
        DD_STATIC_CONST LONGLONG MsToRelativeTimeout(uint32 millisecTimeout)
        {
            // This timeout value is expressed in
            // 100 nanosecond units...
            // Thus, it is our millisecond input times 10000.
            // we make it negative to specify relative time, not absolute
            return static_cast<LONGLONG>(millisecTimeout) * -10000;
        }

        // Wait on a KMD object
        inline Result WaitObject(HANDLE hObject, uint32 millisecTimeout)
        {
            LARGE_INTEGER *pTimeOut = nullptr;
            LARGE_INTEGER  liTimeOut;

            DD_ASSERT(hObject != kNullPtr);

            if (millisecTimeout != kLogicFailureTimeout)
            {
                liTimeOut.QuadPart = MsToRelativeTimeout(millisecTimeout);
                pTimeOut = &liTimeOut;
            }

            // Get the event handle.
            Result result = Result::Error;
            switch (KeWaitForSingleObject((PVOID)hObject, Executive, KernelMode, FALSE, pTimeOut))
            {
                case STATUS_SUCCESS:
                    result = Result::Success;
                    break;
                case STATUS_TIMEOUT:
                    result = Result::NotReady;
                    break;
            }
            DD_WARN(result != Result::Error);
            return result;
        }

        ////////////////////////
        // Open an event created in user space
        // If hObject is not nullptr, the passed in handle is opened.
        inline HANDLE OpenUserObjectHandle(HANDLE hObject)
        {
            DD_ASSERT(hObject != NULL);

            PVOID outputObject = nullptr;

            // Create the user event based on the passed in
            // handle...
            ObReferenceObjectByHandle(
                hObject,
                EVENT_ALL_ACCESS,
                nullptr,
                UserMode,
                &outputObject,
                (POBJECT_HANDLE_INFORMATION)nullptr);

            DD_WARN(outputObject != nullptr);
            return outputObject;
        }

        /////////////////////////////////////////////////////
        // Local routines.....
        //
        void DebugPrint(LogLevel lvl, const char* format, ...)
        {
            va_list args;
            va_start(args, format);
            char buffer[1024];
            Platform::Vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);

            // These filter levels are complicated.
            //      If the value is in [0, 31] (inclusive), represents the value as if it were bit shifted.
            //      Anything >= 32 represents the value as-is.
            // Serious errors should be logged with DPFLTR_ERROR_LEVEL, and larger values represent less serious messages.
            // Anything > DPFLTR_INFO_LEVEL can be used by us however we see fit.
            static constexpr uint32 kLogLevelTable[static_cast<int>(LogLevel::Count)] =
            {
                (DPFLTR_INFO_LEVEL + 1), // LogLevel::Debug
                DPFLTR_INFO_LEVEL,       // LogLevel::Verbose
                DPFLTR_TRACE_LEVEL,      // LogLevel::Info
                DPFLTR_WARNING_LEVEL,    // LogLevel::Warn
                DPFLTR_ERROR_LEVEL,      // LogLevel::Error
                DPFLTR_ERROR_LEVEL,      // LogLevel::Always
            };

            DbgPrintEx(DPFLTR_IHVDRIVER_ID, kLogLevelTable[static_cast<int>(lvl)], "%s\n", buffer);
        }

        int32 AtomicIncrement(Atomic *variable)
        {
            return static_cast<int32>(InterlockedIncrementAcquire(variable));
        }

        int32 AtomicAdd(Atomic *variable, int32 num)
        {
            return static_cast<int32>(InterlockedAddAcquire(variable, static_cast<long>(num)));
        }

        int32 AtomicDecrement(Atomic *variable)
        {
            return static_cast<int32>(InterlockedDecrementAcquire(variable));
        }

        int32 AtomicSubtract(Atomic *variable, int32 num)
        {
            return static_cast<int32>(InterlockedAddAcquire(variable, -static_cast<long>(num)));
        }

        /////////////////////////////////////////////////////
        // Thread routines.....
        //

        Result Thread::Start(ThreadFunction pFnThreadFunc, void* pThreadParameter)
        {
            Result result = Result::Error;

            if ((hThread == NULL) && (pFnThreadFunc != nullptr))
            {
                OBJECT_ATTRIBUTES ObjectAttributes;
                pParameter  = pThreadParameter;
                pFnFunction = pFnThreadFunc;
                hThread     = 0;

                InitializeObjectAttributes(&ObjectAttributes, nullptr, OBJ_KERNEL_HANDLE, nullptr, nullptr);

                // Shim the shim because we need to terminate the system thread after the user callback returns.
                PKSTART_ROUTINE pfnShim;
                pfnShim = [](void* pShimParam) {
                    Thread::ThreadShim(pShimParam);

                    // This correctly shuts down a driver-spawned system thread.
                    //
                    // This thread will continue running until system shutdown or it calls this:
                    PsTerminateSystemThread(0);
                };

                HANDLE sysHandle;
                NTSTATUS createResult = PsCreateSystemThread(
                    &sysHandle,        // ThreadHandle - receives the new thread handle
                    GENERIC_ALL,       // DesiredAccess
                    &ObjectAttributes, // ObjectAttributes
                    NULL,              // ProcessHandle - NULL for driver-created threads.
                    NULL,              // ClientId      - NULL for driver-created threads.
                    pfnShim,           // StartRoutine  - Entry point for new thread
                    this               // This pointer is passed to the new thread entry
                );

                if (createResult == STATUS_SUCCESS)
                {
                    ObReferenceObjectByHandle(sysHandle,
                                              THREAD_ALL_ACCESS,
                                              nullptr,
                                              KernelMode,
                                              (PVOID*)&hThread,
                                              nullptr);
                    ZwClose(sysHandle);

                    result = Result::Success;
                }
            }

            return result;
        };

        Result Thread::SetNameRaw(const char* pThreadName)
        {
            // TODO: Investigate how to do this in system threads
            DD_PRINT(LogLevel::Warn, "SetName() called, but not implemented for system threads");
            DD_UNUSED(pThreadName);
            return Result::Error;
        }

        Result Thread::Join(uint32 timeoutInMs)
        {
            Result result = IsJoinable() ? Result::Success : Result::Error;

            if (result == Result::Success)
            {
                result = onExit.Wait(timeoutInMs);
            }

            if (result == Result::Success)
            {
                ObDereferenceObject((PVOID)hThread);
                Reset();
            }

            return result;
        };

        bool Thread::IsJoinable() const
        {
            return (hThread != NULL);
        };

        /////////////////////////////////////////////////////
        // Memory Management
        /////////////////////////////////////////////////////

        void* AllocateMemory(size_t size, size_t alignment, bool zero)
        {
            DD_UNUSED(alignment);
            DD_ASSERT(size > 0);
            void* pvKernelAddr =
                ExAllocatePoolWithTag(
                    PagedPool,
                    size,
                    kPoolTag);

            // When I looked at this I couldn't find any way to get zeroed pages out of the KMD. It's probably worth
            // revisiting this at sometime, though, since the OS does have a dedicated allocation pool for zeroed
            // pages in user mode.
            if ((pvKernelAddr != nullptr) && zero)
            {
                memset(pvKernelAddr, 0, size);
            }

            return pvKernelAddr;
        }

        void FreeMemory(void* pMemory)
        {
            DD_ASSERT(pMemory != nullptr);
            ExFreePool(pMemory);
        }

        /////////////////////////////////////////////////////
        // Synchronization primatives...
        //

        void AtomicLock::Lock()
        {
            // TODO - implement timeout
            while (InterlockedCompareExchangeAcquire(&m_lock, 1, 0) == 1)
            {
                while (m_lock != 0)
                {
                    // Spin until the mutex is unlocked again
                }
            }
        }

        void AtomicLock::Unlock()
        {
            if (InterlockedCompareExchangeRelease(&m_lock, 0, 1) == 0)
            {
                DD_WARN_REASON("Tried to unlock an already unlocked AtomicLock");
            }
        }

        Mutex::Mutex()
            : m_mutex()
        {
            ExInitializeFastMutex(&m_mutex.mutex);
        }

        Mutex::~Mutex()
        {
#if !defined(NDEBUG)
            // This mutex was destroyed while locked.
            // Potentially hazardous due to possibility of there being a pending wait.
            DD_ASSERT(m_mutex.lockCount == 0);
#endif
        }

        void Mutex::Lock()
        {
            ExAcquireFastMutex(&m_mutex.mutex);
#if !defined(NDEBUG)
            const int32 count = AtomicIncrement(&m_mutex.lockCount);
            // This lock was successfully locked twice.
            // This indicates recursive lock usage, which is non supported on all platforms
            DD_ASSERT(count == 1);
            DD_UNUSED(count);
#endif
        }

        void Mutex::Unlock()
        {
#if !defined(NDEBUG)
            AtomicDecrement(&m_mutex.lockCount);
#endif
            ExReleaseFastMutex(&m_mutex.mutex);
        }

        Semaphore::Semaphore(uint32 initialCount, uint32 maxCount)
        {
            KeInitializeSemaphore(&m_semaphore, initialCount, maxCount);
        }

        Semaphore::~Semaphore()
        {
        }

        Result Semaphore::Signal()
        {
            KeReleaseSemaphore(&m_semaphore, 0, 1, FALSE);
            return Result::Success;
        }

        Result Semaphore::Wait(uint32 millisecTimeout)
        {
            return WaitObject(&m_semaphore, millisecTimeout);
        }

        Event::Event(bool signaled)
        {
            KeInitializeEvent(&m_event, NotificationEvent, signaled);
        }

        Event::~Event()
        {
        }

        void Event::Clear()
        {
            KeClearEvent(&m_event);
        }

        void Event::Signal()
        {
            KeSetEvent(&m_event, 0, FALSE);
        }

        Result Event::Wait(uint32 timeoutInMs)
        {
            return WaitObject(&m_event, timeoutInMs);
        }

        Random::Random()
        {
            LARGE_INTEGER seed;
            KeQueryPerformanceCounter(&seed);
            m_prevState = (seed.LowPart ^ seed.HighPart);
        }

        // Determining the process ID inside the kernel is a bit complicated as kernel threads don't have a process
        // ID of their own, whereas code executed during an escape call is executing inside the address space of the
        // invoking process. As a result we default to assuming all code in the KMD has a process ID of 0, and
        // propagate the invoking process ID as part of escape call dispatching in case we need it.
        ProcessId GetProcessId()
        {
            return 0;
        }

        uint64 GetCurrentTimeInMs()
        {
            LARGE_INTEGER time = {};
            // Note: We cannot use KeQuerySystemTimePrecise here because it is not supported on Windows 7, and using it
            // will cause the driver to BSOD on load.
            KeQuerySystemTime(&time);
            return (time.QuadPart / 10000);
        }

        uint64 QueryTimestampFrequency()
        {
            LARGE_INTEGER perfFrequency = {};
            KeQueryPerformanceCounter(&perfFrequency);

            const uint64 frequency = perfFrequency.QuadPart;

            return frequency;
        }

        uint64 QueryTimestamp()
        {
            const LARGE_INTEGER perfTimestamp = KeQueryPerformanceCounter(nullptr);

            const uint64 timestamp = perfTimestamp.QuadPart;

            return timestamp;
        }

        void Sleep(uint32 millisecTimeout)
        {
            LARGE_INTEGER time = {};
            time.QuadPart = MsToRelativeTimeout(millisecTimeout);
            NTSTATUS result = KeDelayExecutionThread(KernelMode, FALSE, &time);
            DD_ASSERT(result == STATUS_SUCCESS);
            DD_UNUSED(result);
        }

        void GetProcessName(char* buffer, size_t bufferSize)
        {
            Strncpy(buffer, kKmdModuleName, bufferSize);
        }

        void Strncpy(char* pDst, const char* pSrc, size_t dstSize)
        {
            DD_ASSERT(pDst != nullptr);
            DD_ASSERT(pSrc != nullptr);

            // Clamp the copy to the size of the dst buffer (1 char reserved for the null terminator).
            RtlStringCbCopyA(pDst, dstSize, pSrc);
        }

        int32 Vsnprintf(char* pDst, size_t dstSize, const char* pFormat, va_list args)
        {
            char* pDstEnd = pDst;
            RtlStringCbVPrintfExA(
                pDst,
                dstSize,
                &pDstEnd,
                nullptr,
                STRSAFE_FILL_BEHIND_NULL, // Zero the entire buffer past the final NULL byte
                pFormat,
                args
            );
            return static_cast<int32>(pDstEnd - pDst);
        }

        namespace Windows
        {
            /////////////////////////////////////////////////////
            // Local routines.....
            //

            Handle CopySemaphoreFromProcess(ProcessId processId, Handle hObject)
            {
                DD_UNUSED(processId);
                return DD_PTR_TO_HANDLE(OpenUserObjectHandle(reinterpret_cast<HANDLE>(hObject)));
            }

            Handle CreateSharedSemaphore(uint32 initialCount, uint32 maxCount)
            {
                DD_UNUSED(initialCount);
                DD_UNUSED(maxCount);
                // Not implemented in kernel mode
                DD_NOT_IMPLEMENTED();
                return kNullPtr;
            }

            Result SignalSharedSemaphore(Handle pSemaphore)
            {
                KeReleaseSemaphore(reinterpret_cast<PKSEMAPHORE>(pSemaphore), 0, 1, FALSE);
                return Result::Success;
            }

            Result WaitSharedSemaphore(Handle pSemaphore, uint32 millisecTimeout)
            {
                return WaitObject(reinterpret_cast<PKSEMAPHORE>(pSemaphore), millisecTimeout);
            }

            void CloseSharedSemaphore(Handle pSemaphore)
            {
                ObDereferenceObject(reinterpret_cast<PKSEMAPHORE>(pSemaphore));
            }

            Handle CreateSharedBuffer(Size bufferSizeInBytes)
            {
                PHYSICAL_ADDRESS lowAddress;
                PHYSICAL_ADDRESS highAddress;
                SIZE_T totalBytes;

                //
                // Initialize the Physical addresses need for MmAllocatePagesForMdl
                //
                lowAddress.QuadPart = 0;
                highAddress.QuadPart = 0xFFFFFFFFFFFFFFFF;
                totalBytes = bufferSizeInBytes;

                //
                // Allocate a 4K buffer to share with the application
                //
                PMDL hSharedBuffer = MmAllocatePagesForMdl(lowAddress, highAddress, lowAddress, totalBytes);

                DD_WARN(hSharedBuffer != nullptr);

                return DD_PTR_TO_HANDLE(hSharedBuffer);
            }

            Handle MapSystemBufferView(Handle hBuffer, Size bufferSizeInBytes)
            {
                DD_UNUSED(bufferSizeInBytes);
                DD_ASSERT(hBuffer != kNullPtr);
                PVOID pSharedBufferView =
                    MmGetSystemAddressForMdlSafe(reinterpret_cast<PMDL>(hBuffer), NormalPagePriority);
                DD_ASSERT(pSharedBufferView != nullptr);
                DD_ASSUME(pSharedBufferView != nullptr);
                DD_PRINT(LogLevel::Verbose, "Generated system VA = %#p", (void *)pSharedBufferView);
                return DD_PTR_TO_HANDLE(pSharedBufferView);
            }

            void UnmapBufferView(Handle hSharedBuffer, Handle hSharedBufferView)
            {
                DD_ASSERT(hSharedBufferView != kNullPtr);
                DD_PRINT(LogLevel::Verbose, "Unmapping VA = %#p", (void *)hSharedBufferView);
                MmUnmapLockedPages(reinterpret_cast<PVOID>(hSharedBufferView),
                                   reinterpret_cast<PMDL>(hSharedBuffer));
            }

            void CloseSharedBuffer(Handle hSharedBuffer)
            {
                if (hSharedBuffer != kNullPtr)
                {
                    PMDL pMDL = reinterpret_cast<PMDL>(hSharedBuffer);
                    MmFreePagesFromMdl(pMDL);
                    IoFreeMdl(pMDL);
                }
            }

#pragma warning(push)
//      warning C28125: The function 'MmMapLockedPagesSpecifyCache' must be called from within a try/except block:  The requirement might be conditional.
// This IS conditional! There is no try/catch in kernel mode. I don't know why it fires here.
#pragma warning (disable : 28125)
//      warning C28193: 'sharedBuffer' holds a value that must be examined.
// We DO examine it - we convert it and return it. What more does it want?
#pragma warning (disable : 28193)
            Handle MapProcessBufferView(Handle hBuffer, ProcessId processId)
            {
                DD_UNUSED(processId);
                PVOID sharedBuffer = MmMapLockedPagesSpecifyCache(
                    reinterpret_cast<PMDL>(hBuffer),          // MDL
                    UserMode,     // Mode
                    MmCached,     // Caching
                    nullptr,         // Address
                    FALSE,        // Bugcheck?
                    NormalPagePriority); // Priority
                DD_ASSERT(sharedBuffer != nullptr);
                DD_ASSUME(sharedBuffer != nullptr);

                DD_PRINT(LogLevel::Verbose, "Generated user VA = %#p", (void *)sharedBuffer);
                return DD_PTR_TO_HANDLE(sharedBuffer);
            }
#pragma warning(pop)
        }
    }
}
