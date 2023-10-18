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

#pragma once

#include <dd_common_api.h>
#include <dd_allocator_api.h>

namespace DevDriver
{

class DynamicBuffer
{
private:
    void*       m_pBuf;
    size_t      m_capacity;
    size_t      m_size;
    DDAllocator m_alloc;
    DD_RESULT   m_error;

public:
    /// Construct a dynamic buffer using system memory allocator.
    DynamicBuffer();

    /// Construct a dynamic buffer with a custom allocator.
    /// @param allocator Custom user-defined allocator.
    DynamicBuffer(DDAllocator allocator);

    /// Destruct the dynamic buffer and free the underlying memory.
    ~DynamicBuffer();

    /// Return the error happened during any operation.
    DD_RESULT Error() const { return m_error; }

    /// Reserve `size` bytes of memory for future copy. Repeatedly calling this function will
    /// reserve the largest \param size passed. It's illegal to call this function after
    /// \ref Copy().
    ///
    /// @param size Number of bytes to reserve.
    /// @return DD_RESULT_SUCCESS Reservation succeeded.
    /// @return DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY Out of memory failure.
    /// @return DD_RESULT_COMMON_ALREADY_EXISTS Failed to reserve because there is already data
    /// written to the buffer.
    DD_RESULT Reserve(size_t size);

    /// Move the write-pointer to the beginning of the buffer, so that the next \ref Copy() writes
    /// data from the beginning.
    void Clear();

    /// Return the pointer to the underlying buffer.
    const void* Data() const;

    /// The size of written data in bytes.
    size_t Size() const;

    /// The size of underlying buffer in bytes.
    size_t Capacity() const;

    /// Copy data into the dynamic buffer. Internally, this function only proceeds if no error had
    /// occurred. This allows a user to call this function consecutively and check error only once
    /// at the end, as opposed to checking error for every call. This function allocates new memory
    /// if the existing capacity isn't big enough.
    ///
    /// @param pSrcBuf Source buffer to copy data from.
    /// @param srcSize The size of the source buffer.
    void Copy(const void* pSrcBuf, size_t srcSize);
};

} // namespace DevDriver
