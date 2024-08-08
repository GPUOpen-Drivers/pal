/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <ddPlatform.h>

#if DD_PLATFORM_IS_UM
    #include <cstddef>
    #include <stdio.h>
#endif

#include <stb_sprintf.h>

#include <util/vector.h>

namespace DevDriver
{
    namespace Platform
    {
        void* GenericAlloc(void* pUserdata, size_t size, size_t alignment, bool zero)
        {
            DD_UNUSED(pUserdata);
            return AllocateMemory(size, alignment, zero);
        }

        void GenericFree(void* pUserdata, void* pMemory)
        {
            DD_UNUSED(pUserdata);
            FreeMemory(pMemory);
        }

        AllocCb GenericAllocCb =
        {
            nullptr,
            &GenericAlloc,
            &GenericFree
        };

        // Write not more than dataSize characters into pDst, including the NULL terminator.
        // Returns the number of characters that would have been written if the buffer is large enough, including the NULL terminator.
        int32 Snprintf(char* pDst, size_t dstSize, const char* pFmt, ...)
        {
            va_list args;
            va_start(args, pFmt);

            const int32 ret = Vsnprintf(pDst, dstSize, pFmt, args);

            va_end(args);

            if (ret >= 0)
            {
                // ret is the minimum size of the buffer required to hold this formatted string - including a NULL terminator
                if (static_cast<size_t>(ret) > dstSize)
                {
                    // It's common practice to call this function with an empty buffer to query the size.
                    // This warning is just to help track down bugs, so silence it when the buffer in question is empty.
                    if (dstSize != 0)
                    {
                        DD_PRINT(LogLevel::Warn,
                            "Snprintf truncating output from %zu to %zu",
                            ret,
                            dstSize
                        );
                    }
                }
            }
            else
            {
                // A negative value means that some error occurred
                DD_PRINT(LogLevel::Warn,
                    "An unknown io error occured in Vsnprintf: %d (0x%x)",
                    ret,
                    ret);
            }

            return ret;
        }

        int32 Vsnprintf(char* pDst, size_t dstSize, const char* format, va_list args)
        {
            DD_ASSERT(dstSize < INT32_MAX);
            int32 ret = stbsp_vsnprintf(pDst, int(dstSize), format, args);

            // If the return value looks like a valid length, add one to account for a NULL byte.
            if (ret >= 0)
            {
                ret += 1;
            }
            else
            {
                // A negative value means that some error occurred
                // We don't print anything here because our logging requires Vsnprintf
            }

            return ret;
        }

        /////////////////////////////////////////////////////
        // Print to consoles and debuggers
        void DebugPrint(LogLevel lvl, const char* pFormat, ...)
        {
            // Use the typical pattern of snprintf-style functions:
            //      1. Call a first time to get the required buffer size
            //      2. Call a second time to do the actual formatting
            // We'll need two va_lists for this, since each execution of vsnprintf consumes its va_list

            // This buffer has a fixed amount stack-allocated for the common case
            // It's pretty rare that we need to spill onto the heap.
            Vector<char, 128> buffer(GenericAllocCb);
            va_list args;
            {
                // 1. Dry run format to get the required buffer size
                va_list dryRunArgs;
                va_start(dryRunArgs, pFormat);
                va_copy(args, dryRunArgs);

                // This is the buffer requirement including the NULL terminator.
                const int requiredSize = Platform::Vsnprintf(nullptr, 0, pFormat, dryRunArgs);
                va_end(dryRunArgs);

                // Reserve enough space for the fully formatted output and a trailing newline.
                buffer.ResizeAndZero(requiredSize + 1);
            }

            // 2. Do the actual formatting
            Platform::Vsnprintf(buffer.Data(), buffer.Size(), pFormat, args);
            va_end(args);

            // Append a newline - This keeps consecutive messages clearly delimited.
            Platform::Strncat(buffer.Data(), "\n", buffer.Size());

#if DD_PLATFORM_IS_UM
            printf("[DevDriver] %s", buffer.Data());
#else
            // On Kernel mode platforms, printf() isn't available, so we skip it and let PlatformDebugPrint handle output
#endif

            // Platforms may have additional logging to do - e.g. system logging frameworks like OutputDebugStringA().
            PlatformDebugPrint(lvl, buffer.Data());
        }

        ThreadReturnType Thread::ThreadShim(void* pShimParam)
        {
            DD_ASSERT(pShimParam != nullptr);

            Thread* pThread = reinterpret_cast<Thread*>(pShimParam);
            DD_ASSERT(pThread->pFnFunction != nullptr);
            DD_ASSERT(pThread->hThread     != kInvalidThreadHandle);

            // Execute the caller's thread function
            pThread->pFnFunction(pThread->pParameter);

            // Posix platforms do not have a simple way to timeout a thread join.
            // To get around this, we wrap user-supplied callbacks and explicitly signal when the
            // user callback returns.
            // Thread::Join() can then wait on this event to know if the thread exited normally.
            // If it returns without timing out, we can call the posix join without having to
            // worry about blocking indefinitely.
            // This behavior is toggle-able across all platforms until we have a more native solution.
            pThread->onExit.Signal();

            return ThreadReturnType(0);
        }

        Result Thread::SetName(const char* pFmt, ...)
        {
            Result result = Result::Error;

            DD_WARN(hThread != kInvalidThreadHandle);
            if (hThread != kInvalidThreadHandle)
            {
                // Limit the size of the thread name to the platform defined maximum.
                char threadNameBuffer[kThreadNameMaxLength];
                memset(threadNameBuffer, 0, sizeof(threadNameBuffer));

                va_list args;
                va_start(args, pFmt);
                const int32 ret = Vsnprintf(threadNameBuffer, ArraySize(threadNameBuffer), pFmt, args);
                va_end(args);

                if (ret < 0)
                {
                    result = Result::Error;
                }
                else
                {
                    result = SetNameRaw(threadNameBuffer);
                }
            }

            return result;
        }

        Thread::~Thread()
        {
            if (IsJoinable())
            {
                DD_ASSERT_REASON("A Thread object left scope without calling Join()");
            }
        }

        // Random::Random() is implemented per platform, and seeded with the
        // time.

        // Standard Linear Congruential Generator.
        // It's basically rand() but consistent across platforms.
        uint32 Random::Generate()
        {
            // Keep the naming consistent with math notation.
            constexpr auto m = kModulus;
            constexpr auto a = kMultiplier;
            constexpr auto c = kIncrement;

            m_prevState = (m_prevState * a + c) % m;

            // Return a subset of the bits
            uint32 parts[3] = {};
            parts[0] = (m_prevState >>  0) & 0xffff;
            parts[1] = (m_prevState >> 16) & 0xffff;
            parts[2] = (m_prevState >> 32) & 0xffff;
            return  (parts[2] << 15) | (parts[1] >> 1);
        }

        void Random::Reseed(uint64 seed)
        {
            // Seeds must be smaller than the modulus.
            // If we silently do the wrapping, a seed of 1 and (kModulus + 1) will generate the same sequence.
            // This is bad but not the end of the world.
            DD_WARN(seed < kModulus);
            m_prevState = seed % kModulus;
        }

        void AtomicLock::Lock()
        {
            // TODO - implement timeout
            while (TryLock() == false)
            {
                while (m_lock != 0)
                {
                    // Spin until the mutex is unlocked again
                }
            }
        }
    }

    // The minimum alignment that system allocators are expected to adhere to.
#if !DD_PLATFORM_IS_KM
    constexpr size_t kMinSystemAlignment = alignof(max_align_t);
#else
    // In the kernel, we have to hardcode this to 16 bytes because of header issues...
    constexpr size_t kMinSystemAlignment = 16;
#endif

    void* AllocCb::Alloc(size_t size, size_t alignment, bool zero) const
    {
        // Allocators are not expected to ever align smaller than the system minimum.
        // (This is usually sizeof(void*), but always check against this constant)
        if (alignment < kMinSystemAlignment)
        {
            alignment = kMinSystemAlignment;
        }

        return pfnAlloc(pUserdata, size, alignment, zero);
    }

    void* AllocCb::Alloc(size_t size, bool zero) const
    {
        return Alloc(size, kMinSystemAlignment, zero);
    }

    void AllocCb::Free(void* pMemory) const
    {
        pfnFree(pUserdata, pMemory);
    }
}
