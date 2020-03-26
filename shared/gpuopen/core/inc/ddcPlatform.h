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
* @file  ddcPlatform.h
* @brief Developer Driver Platform Abstraction Layer Header
***********************************************************************************************************************
*/

#pragma once

#include <stdarg.h>
#include <new>

#include <ddDefs.h>
#include <ddTemplate.h>

#define DD_CACHE_LINE_BYTES 64

#define DD_MALLOC(size, alignment, allocCb) allocCb.Alloc(size, alignment, false)
#define DD_CALLOC(size, alignment, allocCb) allocCb.Alloc(size, alignment, true)
#define DD_FREE(memory, allocCb)            allocCb.Free(memory)

#define DD_NEW(className, allocCb) new(allocCb, alignof(className), true, __FILE__, __LINE__, __FUNCTION__) className
#define DD_DELETE(memory, allocCb) DevDriver::Platform::Destructor(memory); DD_FREE(memory, allocCb)

#define DD_NEW_ARRAY(className, numElements, allocCb) DevDriver::Platform::NewArray<className>(numElements, allocCb)
#define DD_DELETE_ARRAY(memory, allocCb)              DevDriver::Platform::DeleteArray(memory, allocCb)

// Always enable asserts in Debug builds
#if !defined(NDEBUG)
    #if !defined(DEVDRIVER_ASSERTS_ENABLE)
        #define DEVDRIVER_ASSERTS_ENABLE
    #endif
    #if !defined(DEVDRIVER_ASSERTS_DEBUGBREAK)
        #define DEVDRIVER_ASSERTS_DEBUGBREAK
    #endif
#endif

#define DD_PTR_TO_HANDLE(x) ((DevDriver::Handle)(uintptr_t)(x))

#define DD_SANITIZE_RESULT(x) ((x != Result::Success) ? Result::Error : x)

namespace DevDriver
{

////////////////////////////
// Common logging levels
enum struct LogLevel : uint8
{
    Debug = 0,
    Verbose,
    Info,
    Warn,
    Error,
    Always,
    Count,

    // Backwards compatibility for old alert log level
    Alert = Warn,

    Never = 0xFF
};

typedef void*(*AllocFunc)(void* pUserdata, size_t size, size_t alignment, bool zero);
typedef void(*FreeFunc)(void* pUserdata, void* pMemory);

struct AllocCb
{
    void*     pUserdata;
    AllocFunc pfnAlloc;
    FreeFunc  pfnFree;

    void* Alloc(size_t size, size_t alignment, bool zero) const;
    void* Alloc(size_t size, bool zero) const;
    void Free(void* pMemory) const;
};

namespace Platform
{

// Used by the Platform::Thread implementation.
typedef void (*ThreadFunction)(void* pThreadParameter);

} // namespace Platform

} // namespace DevDriver

#if defined(DD_PLATFORM_WINDOWS_UM)
    #include <platforms/ddWinPlatform.h>
#elif defined(DD_PLATFORM_DARWIN_UM)
    #include <platforms/ddPosixPlatform.h>
#elif defined(DD_PLATFORM_LINUX_UM)
    #include <platforms/ddPosixPlatform.h>
#else
    // Legacy system for Ati Make
    #if defined(_WIN32) && !defined(_KERNEL_MODE)
        #define DD_PLATFORM_WINDOWS_UM
        #include <platforms/ddWinPlatform.h>
    #elif defined(__linux__)
        #define DD_PLATFORM_LINUX_UM
        #include <platforms/ddPosixPlatform.h>
    #else
        #error "Unknown Platform - please configure your build system"
    #endif

    #if __x86_64__
        #define AMD_TARGET_ARCH_BITS 64
    #else
        #define AMD_TARGET_ARCH_BITS 32
    #endif
#endif

#if !defined(DD_RESTRICT)
    #error "DD_RESTRICT not defined by platform!"
#endif

#if !defined(DD_DEBUG_BREAK)
    #error "DD_DEBUG_BREAK not defined by platform!"
#endif

#if !defined(DD_ASSUME)
    #error "DD_ASSUME not defined by platform!"
#endif

// This only exists for 32bit Windows to specificy callbacks as __stdcall.
#if !defined(DD_APIENTRY)
    #define DD_APIENTRY
#endif

// TODO: remove this and make kDebugLogLevel DD_STATIC_CONST when we use a version of visual studio that supports it
#ifdef DEVDRIVER_LOG_LEVEL
    #define DEVDRIVER_LOG_LEVEL_VALUE static_cast<LogLevel>(DEVDRIVER_LOG_LEVEL)
#else
    #if defined(NDEBUG)
        // In non-debug builds, default to printing asserts, Error, and Always log messages
        #define DEVDRIVER_LOG_LEVEL_VALUE LogLevel::Error
    #else
        // In debug builds, default to more messages
        #define DEVDRIVER_LOG_LEVEL_VALUE LogLevel::Verbose
    #endif
#endif

#define DD_WILL_PRINT(lvl) ((lvl >= DEVDRIVER_LOG_LEVEL_VALUE) & (lvl < DevDriver::LogLevel::Count))
#define DD_PRINT(lvl, ...) DevDriver::LogString<lvl>(__VA_ARGS__)

#if defined(DEVDRIVER_ASSERTS_DEBUGBREAK)
    #define DD_ASSERT_DEBUG_BREAK() DD_DEBUG_BREAK()
#else
    #define DD_ASSERT_DEBUG_BREAK()
#endif

// Calling `check_expr_is_bool(x)` when `x` is not exactly a bool will create a compile error.
// When it is a bool, it's a no-op.
// This allows us to enforce bool arguments to DD_ASSERT() macros
namespace DevDriver
{
    inline void check_expr_is_bool(bool) {}

    template <typename T>
    void check_expr_is_bool(const T&) = delete;
}

#if !defined(DEVDRIVER_ASSERTS_ENABLE)
    #define DD_WARN(statement)       DD_UNUSED(0)
    #define DD_WARN_REASON(reason)   DD_UNUSED(0)
    #define DD_ASSERT(statement)     DD_UNUSED(0) // WA: Do not optimize code using DD_ASSERT(), by calling DD_ASSUME().
    #define DD_ASSERT_REASON(reason) DD_UNUSED(0)
#else
    #define DD_WARN(statement) do                                                         \
    {                                                                                     \
        DevDriver::check_expr_is_bool(statement);                                         \
        if (!(statement))                                                                 \
        {                                                                                 \
            DD_PRINT(DevDriver::LogLevel::Warn, "%s (%d): Warning triggered in %s: %s",   \
                __FILE__, __LINE__, __func__, DD_STRINGIFY(statement));                   \
        }                                                                                 \
    } while (0)

    #define DD_WARN_REASON(reason) do                                                 \
    {                                                                                 \
        DD_PRINT(DevDriver::LogLevel::Warn, "%s (%d): Warning triggered in %s: %s",   \
            __FILE__, __LINE__, __func__, reason);                                    \
    } while (0)

    #define DD_ASSERT(statement) do                                                       \
    {                                                                                     \
        DevDriver::check_expr_is_bool(statement);                                         \
        if (!(statement))                                                                 \
        {                                                                                 \
            DD_PRINT(DevDriver::LogLevel::Error, "%s (%d): Assertion failed in %s: %s",   \
                __FILE__, __LINE__, __func__, DD_STRINGIFY(statement));                   \
            DD_ASSERT_DEBUG_BREAK();                                                      \
        }                                                                                 \
    } while (0)

    #define DD_ASSERT_REASON(reason) do                                               \
    {                                                                                 \
        DD_PRINT(DevDriver::LogLevel::Error, "%s (%d): Assertion failed in %s: %s",   \
            __FILE__, __LINE__, __func__, reason);                                    \
        DD_ASSERT_DEBUG_BREAK();                                                      \
    } while (0)
#endif

/// Convenience macro that always warns.
#define DD_WARN_ALWAYS() DD_WARN_REASON("Unconditional Warning")

/// Convenience macro that always asserts.
#define DD_ASSERT_ALWAYS() DD_ASSERT_REASON("Unconditional Assertion")

/// Convenience macro that asserts if something has not been implemented.
#define DD_NOT_IMPLEMENTED() DD_ASSERT_REASON("Code not implemented!")

/// Convenience macro that asserts if an area of code that shouldn't be executed is reached.
#define DD_UNREACHABLE() DD_ASSERT_REASON("Unreachable code has been reached!")

// Backwards compatibility for old alert macro
#define DD_ALERT(statement)      DD_WARN(statement)
#define DD_ALERT_REASON(reason)  DD_WARN_REASON(reason)
#define DD_ALERT_ALWAYS()        DD_WARN_ALWAYS()

// Allocates memory using an AllocCb.
// This overload is declared noexcept, and will correctly handle AllocCb::pfnAlloc() returning NULL.
void* operator new(
    size_t                    size,
    const DevDriver::AllocCb& allocCb,
    size_t                    align,
    bool                      zero,
    const char*               pFilename,
    int                       lineNumber,
    const char*               pFunction
) noexcept;

// Overload of operator delete that matches the previously declared operator new.
// The compiler can call this version automatically in the case of exceptions thrown in the Constructor
// ... even though we turn them off?
// Compilers are fussy.
void operator delete(
    void*                     pObject,
    const DevDriver::AllocCb& allocCb,
    size_t                    align,
    bool                      zero,
    const char*               pFilename,
    int                       lineNumber,
    const char*               pFunction
) noexcept;

namespace DevDriver
{

namespace Platform
{

template<typename T>
inline void static Destructor(T* p)
{
    if (p != nullptr)
    {
        p->~T();
    }
}

template<typename T>
static T* NewArray(size_t numElements, const AllocCb& allocCb)
{
    size_t allocSize = (sizeof(T) * numElements) + DD_CACHE_LINE_BYTES;
    size_t allocAlign = DD_CACHE_LINE_BYTES;

    T* pMem = reinterpret_cast<T*>(DD_MALLOC(allocSize, allocAlign, allocCb));
    if (pMem != nullptr)
    {
        pMem = reinterpret_cast<T*>(reinterpret_cast<char*>(pMem) + DD_CACHE_LINE_BYTES);
        size_t* pNumElements = reinterpret_cast<size_t*>(reinterpret_cast<char*>(pMem) - sizeof(size_t));
        *pNumElements = numElements;
        T* pCurrentElement = pMem;
        for (size_t elementIndex = 0; elementIndex < numElements; ++elementIndex)
        {
            new(pCurrentElement) T;
            ++pCurrentElement;
        }
    }

    return pMem;
}

template<typename T>
static void DeleteArray(T* pElements, const AllocCb& allocCb)
{
    if (pElements != nullptr)
    {
        size_t numElements = *reinterpret_cast<size_t*>(reinterpret_cast<char*>(pElements) - sizeof(size_t));
        T* pCurrentElement = pElements;
        for (size_t elementIndex = 0; elementIndex < numElements; ++elementIndex)
        {
            pCurrentElement->~T();
            ++pCurrentElement;
        }

        pElements = reinterpret_cast<T*>(reinterpret_cast<char*>(pElements) - DD_CACHE_LINE_BYTES);
    }

    DD_FREE(pElements, allocCb);
}

// Get the number of elements in a statically sized array
// Usage:
//      char buffer[1024];
//      size_t size = ArraySize(buffer); // size == 1024
//
//  With a cast:
//      char buffer[1024];
//      uint32 size = ArraySize<uint32>(buffer);
//
template <
    typename SizeT = size_t,    // Type to return
    typename T,                 // Inferred type of array elements - you should not need to supply this argument
    size_t   Size               // Inferred length of array (in elements) - you should not need to supply this argument
>
constexpr SizeT ArraySize(const T(&)[Size])
{
    return static_cast<SizeT>(Size);
}

void DebugPrint(LogLevel lvl, const char* format, ...);

/* platform functions for performing atomic operations */

int32 AtomicIncrement(Atomic *variable);
int32 AtomicDecrement(Atomic *variable);
int32 AtomicAdd(Atomic *variable, int32 num);
int32 AtomicSubtract(Atomic *variable, int32 num);

// A generic AllocCb that defers allocation to Platform::AllocateMemory()
// Suitable for memory allocation if you don't care about it.
extern AllocCb GenericAllocCb;

void* AllocateMemory(size_t size, size_t alignment, bool zero);
void FreeMemory(void* pMemory);

/* fast locks */
class AtomicLock
{
public:
    AtomicLock() : m_lock(0) {};
    ~AtomicLock() {};
    void Lock();
    void Unlock();
    bool IsLocked() { return (m_lock != 0); };
private:
    Atomic m_lock;
};

class Mutex
{
public:
    Mutex();
    ~Mutex();
    void Lock();
    void Unlock();
private:
    MutexStorage m_mutex;
};

class Semaphore
{
public:
    explicit Semaphore(uint32 initialCount, uint32 maxCount);
    ~Semaphore();
    Result Signal();
    Result Wait(uint32 millisecTimeout);
private:
    SemaphoreStorage m_semaphore;
};

class Event
{
public:
    explicit Event(bool signaled);
    ~Event();
    void Clear();
    void Signal();
    Result Wait(uint32 timeoutInMs);
private:
    EventStorage m_event;
};

class Thread
{
public:
    Thread() = default;

    Thread(Thread&& other) noexcept
    {
        other.pFnFunction = this->pFnFunction;
        other.pParameter  = this->pParameter;
        other.hThread     = this->hThread;
        other.onExit      = this->onExit;
        Reset();
    }
    Thread& operator= (Thread&& other) noexcept = default;

    // Copying a thread doesn't make sense
    Thread(const Thread&) = delete;
    Thread& operator= (const Thread& other) = delete;

    ~Thread();

    Result Start(ThreadFunction pFnThreadFunc, void* pThreadParameter);

    // Set the user-visible name for the thread using printf-style formatters
    // This should only be called on valid thread objects. (Threads that have been started)
    // This function will return Result::Error if it's called on an invalid thread.
    // Note: This change is global to the thread and can be changed by other means
    //       Treat this as an aid for people
    Result SetName(const char* pFmt, ...);

    Result Join(uint32 timeoutInMs);

    bool IsJoinable() const;

private:
    static ThreadReturnType DD_APIENTRY ThreadShim(void* pShimParam);

    // Reset our object to a default state
    void Reset()
    {
        pFnFunction = nullptr;
        pParameter  = nullptr;
        hThread     = kInvalidThreadHandle;

        onExit.Clear();
    }

    // Set the thread name to a hard-coded string.
    // The thread name passed to this function must be no larger than kThreadNameMaxLength including the NULL byte.
    // If a larger string is passed, errors may occur on some platforms.
    Result SetNameRaw(const char* pThreadName);

    ThreadFunction pFnFunction = nullptr;
    void*          pParameter  = nullptr;
    ThreadHandle   hThread     = kInvalidThreadHandle;
    Event          onExit      = Event(false); // Start unsignaled
};

class Random
{
public:
    // Algorithm Constants
    static constexpr uint64 kModulus    = (uint64(1) << 48);
    static constexpr uint64 kMultiplier = 0X5DEECE66Dull;
    static constexpr uint16 kIncrement  = 0xB;

    Random();
    Random(uint64 seed)
    {
        Reseed(seed);
    }
    ~Random() {}

    uint32 Generate();
    void Reseed(uint64 seed);
private:
    uint64 m_prevState = 0;

    // Sanity checks.
    static_assert(0 < kModulus,           "Invalid modulus");
    static_assert(0 < kMultiplier,        "Invalid multiplier");
    static_assert(kMultiplier < kModulus, "Invalid multiplier");
    static_assert(kIncrement < kModulus,  "Invalid increment");
};

class Library
{
public:
    Library() : m_hLib(nullptr) { }
    ~Library() { Close(); }

    Result Load(const char* pLibraryName);

    void Close();

    bool IsLoaded() const { return (m_hLib != nullptr); }

    void Swap(Library* pLibrary)
    {
        m_hLib = pLibrary->m_hLib;
        pLibrary->m_hLib = nullptr;
    }

    // Retrieve a function address from the dynamic library object. Returns true if successful, false otherwise.
    template <typename Func_t>
    bool GetFunction(const char* pName, Func_t* ppfnFunc) const
    {
        (*ppfnFunc) = reinterpret_cast<Func_t>(GetFunctionHelper(pName));
        return ((*ppfnFunc) != nullptr);
    }

private:
    void* GetFunctionHelper(const char* pName) const;

    LibraryHandle m_hLib;

    DD_DISALLOW_COPY_AND_ASSIGN(Library);
};

ProcessId GetProcessId();

uint64 GetCurrentTimeInMs();

uint64 QueryTimestampFrequency();
uint64 QueryTimestamp();

// Todo: Remove Sleep() entirely from our platform API. It cannot be used in the KMD and should not be used
// anywhere else either.
void Sleep(uint32 millisecTimeout);

void GetProcessName(char* buffer, size_t bufferSize);

void Strncpy(char* pDst, const char* pSrc, size_t dstSize);

template <size_t DstSize>
void Strncpy(char(&dst)[DstSize], const char* pSrc)
{
    Strncpy(dst, pSrc, DstSize);
}

char* Strtok(char* pDst, const char* pDelimiter, char** ppContext);

void Strcat(char* pDst, const char* pSrc, size_t dstSize);

int32 Strcmpi(const char* pSrc1, const char* pSrc2);

int32 Snprintf(char* pDst, size_t dstSize, const char* pFormat, ...);
int32 Vsnprintf(char* pDst, size_t dstSize, const char* pFormat, va_list args);

template <size_t DstSize, typename... Args>
int32 Snprintf(char(&dst)[DstSize], const char* pFormat, Args&&... args)
{
    return Snprintf(dst, DstSize, pFormat, args...);
}

struct OsInfo
{
    char name[32];         /// A human-readable string to identify the version of the OS running
    char description[256]; /// A human-readable string to identify the detailed version of the OS running
    char hostname[128];    /// The hostname for the machine

    uint64 physMemory; /// Total amount of memory available on host in bytes
    uint64 swapMemory; /// Total amount of swap memory available on host in bytes
};

Result QueryOsInfo(OsInfo* pInfo);

} // Platform

#ifndef DD_PRINT_FUNC
#define DD_PRINT_FUNC Platform::DebugPrint
#else
void DD_PRINT_FUNC(LogLevel logLevel, const char* format, ...);
#endif

template <LogLevel logLevel = LogLevel::Info, class ...Ts>
inline void LogString(const char *format, Ts&&... args)
{
    if (DD_WILL_PRINT(logLevel))
    {
        DD_PRINT_FUNC(logLevel, format, Platform::Forward<Ts>(args)...);
    }
}

// Increments a const pointer by numBytes by first casting it to a const uint8*.
DD_NODISCARD
constexpr const void* VoidPtrInc(
    const void* pPtr,
    size_t      numBytes)
{
    return (static_cast<const uint8*>(pPtr) + numBytes);
}

// Increments a pointer by numBytes by first casting it to a uint8*.
DD_NODISCARD
constexpr void* VoidPtrInc(
    void*  pPtr,
    size_t numBytes)
{
    return (static_cast<uint8*>(pPtr) + numBytes);
}

// Decrements a const pointer by numBytes by first casting it to a const uint8*.
DD_NODISCARD
constexpr const void* VoidPtrDec(
    const void* pPtr,
    size_t      numBytes)
{
    return (static_cast<const uint8*>(pPtr) - numBytes);
}

// Decrements a pointer by numBytes by first casting it to a uint8*.
DD_NODISCARD
constexpr void* VoidPtrDec(
    void*  pPtr,
    size_t numBytes)
{
    return (static_cast<uint8*>(pPtr) - numBytes);
}

//---------------------------------------------------------------------
// CRC32
//
// Calculate a 32bit crc using a the Sarwate look up table method. The original algorithm was created by
// Dilip V. Sarwate, and is based off of Stephan Brumme's implementation. See also:
// https://dl.acm.org/citation.cfm?doid=63030.63037
// http://create.stephan-brumme.com/crc32/#sarwate
//
//// Copyright (c) 2011-2016 Stephan Brumme. All rights reserved.
//*****************************************************************************************************************
// * This software is provided 'as-is', without any express or implied warranty. In no event will the author be held
// * liable for any damages arising from the use of this software. Permission is granted to anyone to use this
// * software for any purpose, including commercial applications, and to alter it and redistribute it freely,
// * subject to the following restrictions:
// *    1. The origin of this software must not be misrepresented; you must not claim that you wrote the original
// *         software
// *    2. If you use this software in a product, an acknowledgment in the product documentation would be
// *         appreciated but is not required.
// *    3. Altered source versions must be plainly marked as such, and must not be misrepresented as being the
// *         original software.
// *****************************************************************************************************************
//
// ... and the following slicing-by-8 algorithm (from Intel):
// http://www.intel.com/technology/comms/perfnet/download/CRC_generators.pdf
// http://sourceforge.net/projects/slicing-by-8/
//
// Copyright (c) 2004-2006 Intel Corporation - All Rights Reserved
//
// This software program is licensed subject to the BSD License,
// available at http://www.opensource.org/licenses/bsd-license.html.
//
//
// Tables for software CRC generation
//
static inline uint32 CRC32(const void *pData, size_t length, uint32 lastCRC = 0)
{
    DD_STATIC_CONST uint32_t lookupTable[256] =
    {
        0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
        0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
        0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
        0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
        0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
        0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
        0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
        0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
        0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
        0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
        0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
        0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
        0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
        0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
        0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
        0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
        0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
        0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
        0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
        0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
        0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
        0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
        0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
        0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
        0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
        0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
        0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
        0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
        0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
        0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
        0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,
        0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D,
    };

    uint32 crc = ~lastCRC; // same as lastCRC ^ 0xFFFFFFFF
    const unsigned char* DD_RESTRICT pCurrent = (const unsigned char*)pData;
    while (length--)
        crc = (crc >> 8) ^ lookupTable[(crc & 0xFF) ^ *pCurrent++];
    return ~crc;
}

/// Convert a `DevDriver::Result` into a human recognizable string.
static inline const char* ResultToString(Result result)
{
    switch (result)
    {
        //// Generic Result Code  ////
        case Result::Success:            return "Success";
        case Result::Error:              return "Error";
        case Result::NotReady:           return "NotReady";
        case Result::VersionMismatch:    return "VersionMismatch";
        case Result::Unavailable:        return "Unavailable";
        case Result::Rejected:           return "Rejected";
        case Result::EndOfStream:        return "EndOfStream";
        case Result::Aborted:            return "Aborted";
        case Result::InsufficientMemory: return "InsufficientMemory";
        case Result::InvalidParameter:   return "InvalidParameter";
        case Result::InvalidClientId:    return "InvalidClientId";
        case Result::ConnectionExists:   return "ConnectionExists";
        case Result::FileNotFound:       return "FileNotFound";
        case Result::FunctionNotFound:   return "FunctionNotFound";
        case Result::InterfaceNotFound:  return "InterfaceNotFound";
        case Result::EntryExists:        return "EntryExists";
        case Result::FileAccessError:    return "FileAccessError";
        case Result::FileIoError:        return "FileIoError";
        case Result::LimitReached:       return "LimitReached";

        //// URI PROTOCOL  ////
        case Result::UriServiceRegistrationError:  return "UriServiceRegistrationError";
        case Result::UriStringParseError:          return "UriStringParseError";
        case Result::UriInvalidParameters:         return "UriInvalidParameters";
        case Result::UriInvalidPostDataBlock:      return "UriInvalidPostDataBlock";
        case Result::UriInvalidPostDataSize:       return "UriInvalidPostDataSize";
        case Result::UriFailedToAcquirePostBlock:  return "UriFailedToAcquirePostBlock";
        case Result::UriFailedToOpenResponseBlock: return "UriFailedToOpenResponseBlock";
        case Result::UriRequestFailed:             return "UriRequestFailed";
        case Result::UriPendingRequestError:       return "UriPendingRequestError";
        case Result::UriInvalidChar:               return "UriInvalidChar";
        case Result::UriInvalidJson:               return "UriInvalidJson";

        //// Settings URI Service  ////
        case Result::SettingsUriInvalidComponent:        return "SettingsUriInvalidComponent";
        case Result::SettingsUriInvalidSettingName:      return "SettingsUriInvalidSettingName";
        case Result::SettingsUriInvalidSettingValue:     return "SettingsUriInvalidSettingValue";
        case Result::SettingsUriInvalidSettingValueSize: return "SettingsUriInvalidSettingValueSize";

        //// Info URI Service ////
        case Result::InfoUriSourceNameInvalid:       return "InfoUriSourceNameInvalid";
        case Result::InfoUriSourceCallbackInvalid:   return "InfoUriSourceCallbackInvalid";
        case Result::InfoUriSourceAlreadyRegistered: return "InfoUriSourceAlreadyRegistered";
        case Result::InfoUriSourceWriteFailed:       return "InfoUriSourceWriteFailed";
    }

    DD_PRINT(LogLevel::Warn, "Result code %u is not handled", static_cast<uint32>(result));
    return "Unrecognized DevDriver::Result";
}

// Helper function for converting bool values into Result enums
// Useful for cases where Results and bools are interleaved in logic
static inline Result BoolToResult(bool value)
{
    return (value ? Result::Success : Result::Error);
}

// Use this macro to mark Result values that have not been or cannot be handled correctly.
#define DD_UNHANDLED_RESULT(x) DevDriver::MarkUnhandledResultImpl((x), DD_STRINGIFY(x), __FILE__, __LINE__, __func__)

// Implementation for DD_UNHANDLED_RESULT.
// This is a specialized assert that should be used through the macro, and not called directly.
// This is implemented in ddPlatform.h, so that it has access to DD_ASSERT.
static inline void MarkUnhandledResultImpl(
    Result      result,
    const char* pExpr,
    const char* pFile,
    int         lineNumber,
    const char* pFunc)
{
#if defined(DEVDRIVER_ASSERTS_ENABLE)
    if (result != Result::Success)
    {
        DD_PRINT(DevDriver::LogLevel::Error,
            "%s (%d): Unchecked Result in %s: \"%s\" == \"%s\" (0x%X)\n",
            pFile,
            lineNumber,
            pFunc,
            pExpr,
            ResultToString(result),
            result);
        DD_ASSERT_DEBUG_BREAK();
    }
#else
    DD_UNUSED(result);
    DD_UNUSED(pExpr);
    DD_UNUSED(pFile);
    DD_UNUSED(lineNumber);
    DD_UNUSED(pFunc);
#endif
}

} // DevDriver
