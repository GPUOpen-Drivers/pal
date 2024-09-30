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

#include <dd_assert.h>
#include <dd_integer.h>
#include <dd_ring_buffer.h>

namespace DevDriver
{

void RingBuffer::SetBuffer(const MirroredBuffer* pBuffer)
{
    m_pBuf = reinterpret_cast<uint8_t*>(pBuffer->pBuffer);
    m_bufSize = pBuffer->bufferSize;

    DD_ASSERT((m_bufSize & (m_bufSize - 1)) == 0);
    m_bufSizeMask = m_bufSize - 1;
}

RingBuffer::Work RingBuffer::AcquireForWrite(uint32_t size)
{
    Work work {};

    m_wholeBufferMutex.Lock();

    if (size > 0)
    {
        DD_ASSERT(m_write >= m_read);

        uint64_t occupiedBufSize = m_write - m_read;
        DD_ASSERT(occupiedBufSize <= m_bufSize);

        uint64_t availableBufSize = m_bufSize - occupiedBufSize;
        if (availableBufSize >= size)
        {
            work.offset = static_cast<uint32_t>(m_write & m_bufSizeMask);
            work.size = size;
            m_write += size;
        }
    }

    return work;
}

RingBuffer::Work RingBuffer::AcquireForRead(uint32_t maxSize)
{
    Work work {};

    m_wholeBufferMutex.Lock();

    DD_ASSERT(m_write >= m_read);
    uint64_t writtenDataSize = m_write - m_read;
    DD_ASSERT(writtenDataSize <= m_bufSize);
    work.offset = static_cast<uint32_t>(m_read & m_bufSizeMask);
    uint64_t acquiredSize = (writtenDataSize > maxSize ? maxSize : writtenDataSize);
    work.size = static_cast<uint32_t>(acquiredSize);
    m_read += work.size;

    return work;
}

RingBuffer::Work RingBuffer::AcquireForReadAll()
{
    Work work {};

    m_wholeBufferMutex.Lock();

    DD_ASSERT(m_write >= m_read);
    uint64_t writtenDataSize = m_write - m_read;
    DD_ASSERT(writtenDataSize <= m_bufSize);
    work.offset = static_cast<uint32_t>(m_read & m_bufSizeMask);
    work.size = static_cast<uint32_t>(writtenDataSize);
    m_read += work.size;

    return work;
}

void RingBuffer::Release()
{
    m_wholeBufferMutex.Unlock();
}

} // namespace DevDriver
