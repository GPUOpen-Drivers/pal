/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palRingBuffer.h"
#include "palSysMemory.h"

namespace Util
{

// =====================================================================================================================
template <typename Allocator>
RingBuffer<Allocator>::RingBuffer(
    uint32          numElements,
    size_t          elementSize,
    Allocator*const pAllocator)
    :
    m_pRingBuffer(nullptr),
    m_numElements(numElements),
    m_elementSize(elementSize),
    m_writePointer(0),
    m_readPointer(0),
    m_semaWrite(),
    m_semaRead(),
    m_pAllocator(pAllocator)
{
    PAL_ASSERT(numElements > 0);
    PAL_ASSERT(elementSize > 0);
}

// =====================================================================================================================
// Initializes the contents of the ring buffer.
template <typename Allocator>
Result RingBuffer<Allocator>::Init(
    RingBufferSlotFunc pfnInit,
    void*              pData)
{
    Result result = Result::ErrorOutOfMemory;
    m_pRingBuffer = PAL_MALLOC(m_numElements * m_elementSize, m_pAllocator, SystemAllocType::AllocInternal);

    if (m_pRingBuffer != nullptr)
    {
        result = m_semaWrite.Init(m_numElements, m_numElements);
    }

    if (result == Result::Success)
    {
        result = m_semaRead.Init(m_numElements, 0);
    }

    if ((result == Result::Success) && (pfnInit != nullptr))
    {
        bool initSuccessful = true;

        for (uint32 i = 0; ((initSuccessful == true) && (i < m_numElements)); i++)
        {
            initSuccessful = pfnInit(i, pData, VoidPtrInc(m_pRingBuffer, i * m_elementSize));
        }

        if (initSuccessful == false)
        {
            result = Result::ErrorInitializationFailed;
        }
    }

    return result;
}

// =====================================================================================================================
// Destroys the contents of the ring buffer.
template <typename Allocator>
Result RingBuffer<Allocator>::Destroy(
    RingBufferSlotFunc pfnDestroy,
    void*              pData)
{
    Result result = Result::Success;

    if (pfnDestroy != nullptr)
    {
        bool initSuccessful = true;

        for (uint32 i = 0; ((initSuccessful == true) && (i < m_numElements)); i++)
        {
            initSuccessful = pfnDestroy(i, pData, VoidPtrInc(m_pRingBuffer, i * m_elementSize));
        }

        if (initSuccessful == false)
        {
            result = Result::ErrorUnavailable;
        }
    }

    PAL_SAFE_FREE(m_pRingBuffer, m_pAllocator);

    return result;
}

// =====================================================================================================================
// Retrieve next writeable buffer in the ring.
template <typename Allocator>
Result RingBuffer<Allocator>::GetBufferForWriting(
    uint32 waitTimeMs, // Wait time in milliseconds.
    void** ppBuffer)
{
    Result result = Result::Timeout;

    if (m_semaWrite.Wait(waitTimeMs) == Result::Success)
    {
        (*ppBuffer) = VoidPtrInc(m_pRingBuffer, m_writePointer * m_elementSize);
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Releases the held writeable buffer, making it as written and that the ring is ready to fill the next slot.
template <typename Allocator>
void RingBuffer<Allocator>::ReleaseWriteBuffer()
{
    m_writePointer = (m_writePointer + 1) % m_numElements;

    m_semaRead.Post();
}

// =====================================================================================================================
// Retrieve next readable buffer in the ring.
template <typename Allocator>
Result RingBuffer<Allocator>::GetBufferForReading(
    uint32       waitTimeMs, // Wait time in milliseconds.
    const void** ppBuffer)
{
    Result result = Result::Timeout;

    if (m_semaRead.Wait(waitTimeMs) == Result::Success)
    {
        (*ppBuffer) = VoidPtrInc(m_pRingBuffer, m_readPointer * m_elementSize);
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Releases the held readable buffer, making it as read and that the ring is ready to read the next slot.
template <typename Allocator>
void RingBuffer<Allocator>::ReleaseReadBuffer()
{
    m_readPointer = (m_readPointer + 1) % m_numElements;

    m_semaWrite.Post();
}

} // Util
