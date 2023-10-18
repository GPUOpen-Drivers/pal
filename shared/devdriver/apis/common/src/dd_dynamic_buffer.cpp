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

#include <dd_dynamic_buffer.h>
#include <dd_assert.h>

#include <cstdlib>
#include <cstring>

namespace
{

void* StdRealloc(DDAllocatorInstance* pInstance, void* pMemory, size_t oldSize, size_t newSize)
{
    (void)pInstance;
    (void)oldSize;
    return std::realloc(pMemory, newSize);
}

void StdFree(DDAllocatorInstance* pInstance, void* pMemory, size_t size)
{
    (void)pInstance;
    (void)size;
    std::free(pMemory);
}

} // anonymous namespace

namespace DevDriver
{

DynamicBuffer::DynamicBuffer()
    : m_pBuf{nullptr}
    , m_capacity{0}
    , m_size{0}
    , m_alloc{nullptr, StdRealloc, StdFree}
    , m_error{DD_RESULT_SUCCESS}
{
}

DynamicBuffer::DynamicBuffer(DDAllocator allocator)
    : m_pBuf{nullptr}
    , m_capacity{0}
    , m_size{0}
    , m_alloc{allocator}
    , m_error{DD_RESULT_SUCCESS}
{
}

DynamicBuffer::~DynamicBuffer()
{
    if (m_pBuf != nullptr)
    {
        m_alloc.Free(m_alloc.pInstance, m_pBuf, m_capacity);
    }
}

DD_RESULT DynamicBuffer::Reserve(size_t reserveSize)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    // Reserve can only happen before any data is written to the buffer.
    if ((m_size == 0) && (reserveSize > 0))
    {
        if (reserveSize > m_capacity)
        {
            m_pBuf = m_alloc.Realloc(m_alloc.pInstance, m_pBuf, m_capacity, reserveSize);
            if (m_pBuf == nullptr)
            {
                m_error = DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
                result = m_error;
            }
            else
            {
                m_capacity = reserveSize;
            }
        }
    }
    else
    {
        result = DD_RESULT_COMMON_ALREADY_EXISTS;
        // `Copy()` can still happen after this failure, so don't set `m_error`.
    }

    return result;
}

void DynamicBuffer::Clear()
{
    m_size = 0;
}

const void* DynamicBuffer::Data() const
{
    return m_pBuf;
}

size_t DynamicBuffer::Size() const
{
    return m_size;
}

size_t DynamicBuffer::Capacity() const
{
    return m_capacity;
}

void DynamicBuffer::Copy(const void* pSrcBuf, size_t srcSize)
{
    if (m_error == DD_RESULT_SUCCESS)
    {
        if (srcSize > 0)
        {
            DD_ASSERT(m_capacity >= m_size);

            uint8_t* pDestBuf = static_cast<uint8_t*>(m_pBuf) + m_size;

            if ((m_capacity - m_size) < srcSize)
            {
                size_t newSize = (srcSize > m_capacity) ? (m_capacity + srcSize) : (m_capacity * 2);
                DD_ASSERT(newSize > m_capacity);

                m_pBuf = (uint8_t*)m_alloc.Realloc(m_alloc.pInstance, m_pBuf, m_capacity, newSize);
                if (m_pBuf == nullptr)
                {
                    m_error = DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
                }
                else
                {
                    pDestBuf   = static_cast<uint8_t*>(m_pBuf) + m_size;
                    m_capacity = newSize;
                }

            }

            if (m_error == DD_RESULT_SUCCESS)
            {
                memcpy(pDestBuf, pSrcBuf, srcSize);
                m_size += srcSize;
            }
        }
    }
}

} // namespace DevDriver
