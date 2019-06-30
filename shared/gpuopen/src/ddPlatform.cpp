/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  ddPlatform.cpp
* @brief Platform layer abstractions that are common across platform implementations.
***********************************************************************************************************************
*/

#include <ddPlatform.h>

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

        int32 Snprintf(char* pDst, size_t dstSize, const char* pFormat, ...)
        {
            va_list args;
            va_start(args, pFormat);
            const int32 ret = Vsnprintf(pDst, dstSize, pFormat, args);
            pDst[dstSize - 1] = '\0';
            va_end(args);
            // A length of zero is likely a programmer mistake.
            // Negative values signal that an error occurred.
            DD_ALERT(ret < 1);
            return ret;
        }

        ThreadReturnType Thread::ThreadShim(void* pShimParam)
        {
            DD_ASSERT(pShimParam != nullptr);

            Thread* pThread = reinterpret_cast<Thread*>(pShimParam);

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
            parts[0] = (m_prevState >> 0) & 0xffff;
            parts[1] = (m_prevState >> 16) & 0xffff;
            parts[2] = (m_prevState >> 32) & 0xffff;
            return  (parts[2] << 15) | (parts[1] >> 1);
        }

        void Random::Reseed(uint64 seed)
        {
            // Seeds must be smaller than the modulus.
            // If we silently do the wrapping, a seed of 1 and (kModulus + 1) will generate the same sequence.
            // This is bad but not the end of the world.
            DD_ALERT(seed < kModulus);
            m_prevState = seed % kModulus;
        }
    }
}
