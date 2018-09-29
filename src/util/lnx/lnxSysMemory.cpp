/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palSysMemory.h"
#include <cstdlib>
#include <unistd.h>
#include <sys/mman.h>

namespace Util
{

// =====================================================================================================================
// Default pfnAlloc implementation used if the client doesn't specify allocation callbacks.  Memory will be allocated
// from the standard C runtime.  Returns nullptr if the allocation fails.
// Due to Android libc do not support aligned_alloc, use posix_memalign instead
static void* PAL_STDCALL DefaultAllocCb(
    void*           pClientData,  // Ignored in default implementation.
    size_t          size,         // Size of the requested allocation in bytes.
    size_t          alignment,    // Required alignment of the requested allocation in bytes.
    SystemAllocType allocType)    // Ignored in default implementation.
{
    PAL_ASSERT(IsPowerOfTwo(alignment));

    alignment = Pow2Align(alignment, sizeof(void*));
    void* pMem = nullptr;
    pMem = aligned_alloc(alignment, Pow2Align(size, alignment));
    return pMem;
}

// =====================================================================================================================
// Default pfnFree implementation used if the client doesn't specify allocation callbacks.
static void PAL_STDCALL DefaultFreeCb(
    void* pClientData,  // Ignored in default implementation.
    void* pMem)         // System memory to be freed.
{
    free(pMem);
}

// =====================================================================================================================
// Initializes the specified allocation callback structure with the default Linux allocation callbacks.
Result OsInitDefaultAllocCallbacks(
    AllocCallbacks* pAllocCb)
{
    // The OS-independent layer shouldn't call us if there are already callbacks installed.
    PAL_ASSERT(pAllocCb->pfnAlloc == nullptr);
    PAL_ASSERT(pAllocCb->pfnFree == nullptr);

    pAllocCb->pfnAlloc = DefaultAllocCb;
    pAllocCb->pfnFree  = DefaultFreeCb;

    return Result::Success;
}

// =====================================================================================================================
// Returns the Windows-specific page size.
size_t VirtualPageSize()
{
    return sysconf(_SC_PAGESIZE);
}

// =====================================================================================================================
// Reserves the specified amount of virtual address space.
Result VirtualReserve(
    size_t sizeInBytes,
    void** ppOut,
    void*  pMem,
    size_t alignment
    )
{
    Result result = Result::Success;

    if (sizeInBytes == 0)
    {
        result = Result::ErrorInvalidValue;
    }
    else if (ppOut == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }

    if (result == Result::Success)
    {
        void* pMemory = mmap(pMem, sizeInBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if ((pMemory != nullptr) && (pMemory != MAP_FAILED))
        {
            PAL_ASSERT(ppOut != nullptr);
            (*ppOut) = pMemory;
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
// Commits the specified amount of virtual address space, requesting backing memory from the OS.
Result VirtualCommit(
    void*  pMem,
    size_t sizeInBytes,
    bool isExecutable
    )
{
    Result result = Result::Success;

    if (sizeInBytes == 0)
    {
        result = Result::ErrorInvalidValue;
    }
    else if (pMem == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }

    if (result == Result::Success)
    {
        int32 protFlags = PROT_READ | PROT_WRITE;
        protFlags = isExecutable ? (protFlags | PROT_EXEC) : protFlags;

        void* pMemory = mmap(pMem, sizeInBytes, protFlags, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if ((pMemory != pMem) || (pMemory == MAP_FAILED))
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
// Decommits the specified amount of virtual address space, freeing the backing memory back to the OS.
Result VirtualDecommit(
    void*  pMem,
    size_t sizeInBytes)
{
    Result result = Result::Success;

    if (sizeInBytes == 0)
    {
        result = Result::ErrorInvalidValue;
    }
    else if (pMem == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }

    if (result == Result::Success)
    {
        void* pMemory = mmap(pMem, sizeInBytes, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if ((pMemory != pMem) || (pMemory == MAP_FAILED))
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
// Releases the specified amount of virtual address space, both freeing the backing memory and virtual address space
// back to the OS.
Result VirtualRelease(
    void*  pMem,
    size_t sizeInBytes)
{
    Result result = Result::Success;

    if (sizeInBytes == 0)
    {
        result = Result::ErrorInvalidValue;
    }
    else if (pMem == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }

    if (result == Result::Success)
    {
        result = Result::ErrorOutOfMemory;

        int releaseResult = munmap(pMem, sizeInBytes);

        if (releaseResult == 0)
        {
            result = Result::Success;
        }
    }

    return result;
}

// =====================================================================================================================
void* GenericAllocator::Alloc(
    const AllocInfo& allocInfo)
{
    PAL_ASSERT(IsPowerOfTwo(allocInfo.alignment));

    const size_t alignment = Pow2Align(allocInfo.alignment, sizeof(void*));
    const size_t size      = Pow2Align(allocInfo.bytes, alignment);

    void* pMem = aligned_alloc(alignment, size);

    if ((pMem != nullptr) && allocInfo.zeroMem)
    {
        memset(pMem, 0, size);
    }

    return pMem;
}

// =====================================================================================================================
void GenericAllocator::Free(
    const FreeInfo& freeInfo)
{
    free(freeInfo.pClientMem);
}

} // Util
