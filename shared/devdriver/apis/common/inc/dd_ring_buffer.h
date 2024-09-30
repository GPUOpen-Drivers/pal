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

#pragma once

#include <dd_common_api.h>
#include <dd_memory.h>
#include <dd_mutex.h>
#include <cstdint>

namespace DevDriver
{

/// RingBuffer is a simple circular buffer data structure. It uses a single mutex to synchronize both writing and
/// reading from multiple producer and consumer threads.
///
/// RingBuffer doesn't own the underlying backing memory. Users of this class are responsible for allocating and
/// freeing the buffer memory.
class RingBuffer
{
private:
    uint8_t* m_pBuf;
    uint32_t m_bufSize;
    uint32_t m_bufSizeMask;

    // We assume 64-bit is large enough that m_write and m_read will never wrap around.
    uint64_t m_write; // tracking where the next write starts
    uint64_t m_read;  // tracking where the next read starts

    Mutex m_wholeBufferMutex; // Lock the entire buffer.

public:
    struct Work
    {
        uint32_t offset;
        uint32_t size;
    };

    RingBuffer()
        : m_pBuf {nullptr}
        , m_bufSize {0},
          m_bufSizeMask {0}
        , m_write {0}
        , m_read {0}
        , m_wholeBufferMutex {}
    {}

    /// Set the backing buffer memory.
    void SetBuffer(const MirroredBuffer* pBuffer);

    /// Acquire a range of empty memory block to write data to.
    ///
    /// NB. This call MUST be paired with a call to \ref RingBuffer::Release, no matter the return value.
    ///
    /// @param[in] size How much memory to acquire from the ring buffer.
    /// @return An object of \ref Work, representing the starting offset and the size of the acquired memory block.
    /// Note, the return value \ref Work::size is 0 when 1) \param size is 0, 2) not enough empty space in the ring
    /// buffer.
    Work AcquireForWrite(uint32_t size);

    /// Acquire a range of memory to read data from. The range is at maximum \param maxSize . The range can be
    /// smaller than \param maxSize if there is not enough written data.
    ///
    /// NB. This call MUST be paired with a call to \ref RingBuffer::Release, no matter the return value.
    ///
    /// @param[in] the maximum size of memory of written data to acquire.
    /// @return An object of \ref Work, representing the starting offset and the size of the acquired memory block.
    Work AcquireForRead(uint32_t maxSize);

    /// Acquire all memory of write data to read from.
    ///
    /// NB. This call MUST be paired with a call to \ref RingBuffer::Release, no matter the return value.
    ///
    /// @return An object of \ref Work, representing the starting offset and the size of the acquired memory block.
    /// Note, the return value \ref Work::size is 0 if the ring buffer doesn't contain any written data.
    Work AcquireForReadAll();

    /// Release the lock on the ring buffer.
    void Release();
};

} // namespace DevDriver
