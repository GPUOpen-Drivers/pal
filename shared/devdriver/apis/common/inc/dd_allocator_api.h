/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef DD_ALLOCATOR_H
#define DD_ALLOCATOR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DDAllocatorInstance DDAllocatorInstance;

/// Basic memory allocation interface for DevDriver.
typedef struct DDAllocator
{
    /// A opaque pointer to the internal memory allocation implementation.
    DDAllocatorInstance* pInstance;

    /// This function acts similarly to `std::realloc()`.
    ///
    /// @param pInstance Must be \ref DDModulesApi.pInstance.
    /// @param pMemory A pointer to a block of memory returned by a previous call to `Realloc()`. This
    /// parameter can be NULL.
    /// @param oldSize The size of the memory pointed to by \param pMemory. Unlike `std::realloc()` Callers
    /// of this function must keep track of memory size themselves. Passing 0 will result in failure and
    /// NULL pointer returned, except when \param pMemory is NULL.
    /// @param newSize The new size of memory to allocate.
    /// @return A pointer to a block of memory the size of \param newSize.
    void* (*Realloc)(DDAllocatorInstance* pInstance, void* pMemory, size_t oldSize, size_t newSize);

    /// Deallocates a block of memory previously allocated by \ref DDAllocator.Realloc.
    ///
    /// @param pInstance Must be \ref DDModulesApi.pInstance.
    /// @param pMem A pointer to a block of memory. This pointer must be obtained by an earlier call
    /// to \ref DDAllocator.Realloc.
    /// @param size The size of the memory to be deallocated.
    void (*Free)(DDAllocatorInstance* pInstance, void* pMem, size_t size);
} DDAllocator;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
