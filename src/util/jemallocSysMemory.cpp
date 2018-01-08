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
#include "jemalloc.h"

namespace Util
{
// =====================================================================================================================
// Jemalloc pfnAlloc implementation used if the client doesn't specify allocation callbacks.  Returns nullptr if the
// allocation fails.
static void* PAL_STDCALL JemallocAllocCb(
    void*                 pClientData, // Ignored in jemalloc implementation.
    size_t                size,        // Size of the requested allocation in bytes.
    size_t                alignment,   // Required alignment of the requested allocation in bytes.
    Util::SystemAllocType allocType)   // Ignored in jemalloc implementation.
{
    void* pMem = nullptr;
    je_posix_memalign(&pMem, Pow2Align(alignment, sizeof(void*)), size);
    return pMem;
}

// =====================================================================================================================
// Jemalloc pfnFree implementation used if the client doesn't specify allocation callbacks.
static void PAL_STDCALL JemallocFreeCb(
    void* pClientData,  // Ignored in jemalloc implementation.
    void* pMem)         // System memory to be freed.
{
    je_free(pMem);
}

#if PAL_JEMALLOC_DEBUG
// =====================================================================================================================
// Callback for when jemalloc outputs diagnostic messages.  Messages are prefixed by "<jemalloc>: ".
// Doing anything which tries to allocate memory in this function is likely to result in a crash or deadlock.
static void JemallocMessageCb(
    void*       pClientData,
    const char* pMessage)
{
    PAL_DPINFO("%s", pMessage);
}
#endif

#if PAL_JEMALLOC_STATS
// =====================================================================================================================
// Callback to print out the memory stats for when je_malloc_stats_print is called.
static void JemallocStatsCb(
    void*       pClientData,
    const char* pStats)
{
    PAL_DPINFO("%s", pStats);
}

// =====================================================================================================================
// Print out the jemalloc stats and omit general information that does not change with "g" option.
// Additional Options:
// m: omit merged arena statistics
// a: omit per arena statistics
// b, l, h: omit per size class statistics for bins, large objects, and huge objects, respectively
void JemallocStatsPrint()
{
    je_malloc_stats_print(JemallocStatsCb, nullptr, "g");
}
#endif

// =====================================================================================================================
// Initializes the specified allocation callback structure with the jemalloc allocation callbacks.
void InitJemallocAllocCallbacks(
    AllocCallbacks* pAllocCb)
{
    // Jemalloc callbacks shouldn't be set if there are already callbacks installed.
    PAL_ASSERT(pAllocCb->pfnAlloc == nullptr);
    PAL_ASSERT(pAllocCb->pfnFree == nullptr);

    // Example: "opt.junk:true" initialize allocated memory to 0xa5 and initialize deallocated memory to 0x5a
    // See jemalloc documentation for more possible options and information
    // je_malloc_conf = "";

#if PAL_JEMALLOC_DEBUG
    je_malloc_message = JemallocMessageCb;
#endif

#if PAL_JEMALLOC_STATS
    je_malloc_stats_print(JemallocStatsCb, nullptr, nullptr);
#endif

    // Override the null callbacks with the jemalloc callbacks.
    pAllocCb->pfnAlloc = JemallocAllocCb;
    pAllocCb->pfnFree = JemallocFreeCb;
}

// =====================================================================================================================
// No clean up required- but can print stats one last time if enabled.
void DestroyJemallocAllocCallbacks()
{
#if PAL_JEMALLOC_STATS
    JemallocStatsPrint();
#endif
}

} // Util
