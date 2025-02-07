/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palRingBuffer.h
 * @brief PAL utility collection RingBuffer class declaration.
 ***********************************************************************************************************************
 */

#pragma once

#include "palSemaphore.h"

namespace Util
{

/// Function for each slot in the ring buffer.
typedef bool (PAL_STDCALL *RingBufferSlotFunc)(uint32 slotIdx, void* pData, void* pBuffer);

/**
************************************************************************************************************************
* @brief  Simple container for a ring buffer, useful for multithreaded operations.
************************************************************************************************************************
*/
template <typename Allocator>
class RingBuffer
{
public:
    /// Constructs a ring buffer object with the specified properties.
    ///
    /// @param [in] numElements Number of entries in the ring buffer.
    /// @param [in] elementSize Size, in bytes, of each entry in the ring buffer.
    /// @param [in] pAllocator  The allocator that will allocate memory if required.
    RingBuffer(uint32 numElements, size_t elementSize, Allocator*const pAllocator);
    ~RingBuffer() {};

    /// Initializes the ring buffer, allocating memory for usage.
    ///
    /// @param [in] pfnInit     Initialization function to execute on every slot in the ring buffer.
    /// @param [in] pData       User data to be passed to the initialization function.
    ///
    /// @returns @ref Success if successful, otherwise an appropriate error.
    Result Init(RingBufferSlotFunc pfnInit, void* pData);

    /// Destroys the ring buffer, undoing whatever initialization was performed in Init().
    ///
    /// @param [in] pfnDestroy  Destroy function to execute on every slot in the ring buffer.
    /// @param [in] pData       User data to be passed to the destroy function.
    ///
    /// @returns @ref Success if successful, otherwise an appropriate error.
    Result Destroy(RingBufferSlotFunc pfnDestroy, void* pData);

    /// Retrieves the next buffer to write to.
    ///
    /// @param [in]  waitTime    Number of milliseconds to wait for the next available buffer.
    /// @param [out] ppBuffer    Pointer to the next available writeable buffer.
    ///
    /// @returns @ref Success if a buffer is available within the wait time, @ref Timeout otherwise.
    Result GetBufferForWriting(std::chrono::milliseconds waitTime, void** ppBuffer);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 863
    Result GetBufferForWriting(uint32 waitTimeMs, void** ppBuffer);
#endif

    /// Releases the currently held writable buffer.
    void ReleaseWriteBuffer();

    /// Retrieves the next buffer to read from.
    ///
    /// @param [in]  waitTime       Number of milliseconds to wait for the next available buffer.
    /// @param [out] ppBuffer       Pointer to the next available readable buffer.
    ///
    /// @returns @ref Success if a buffer is available within the wait time, @ref Timeout otherwise.
    Result GetBufferForReading(std::chrono::milliseconds waitTime, const void** ppBuffer);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 863
    Result GetBufferForReading(uint32 waitTimeMs, const void** ppBuffer);
#endif

    /// Releases the currently held readable buffer.
    void ReleaseReadBuffer();

private:
    void*             m_pRingBuffer;  // Allocated ring buffer memory.
    const uint32      m_numElements;  // Number of elements in the ring buffer.
    const size_t      m_elementSize;  // Size of each element in the ring buffer.
    uint32            m_writePointer; // The write pointer of the ring buffer.
    uint32            m_readPointer;  // The read pointer of the ring buffer.
    Semaphore         m_semaWrite;    // Semaphore of the writer of the ring buffer.
    Semaphore         m_semaRead;     // Semaphore of the reader of the ring buffer.
    Allocator*const   m_pAllocator;    // Allocator for this ring buffer.

    PAL_DISALLOW_COPY_AND_ASSIGN(RingBuffer);
};

} // Util
