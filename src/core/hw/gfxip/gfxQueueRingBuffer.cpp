/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfxQueueRingBuffer.h"

namespace Pal
{
// =====================================================================================================================
GfxQueueRingBuffer::GfxQueueRingBuffer(
    GfxQueueRingBufferCreateInfo* pGfxQueueRingBufferCreateInfo):
    m_pParentQueue(pGfxQueueRingBufferCreateInfo->pParentQueue),
    m_startOffset(0),
    m_endOffset(0),
    m_readOffset(0),
    m_writeOffset(0),
    m_preCommitWriteOffset(0),
    m_ringHeaderSize(0),
    m_numAvailableDwords(0),
    m_numReservedDwords(0),
    m_engineType(pGfxQueueRingBufferCreateInfo->engineType)
{
}

// =====================================================================================================================
Result GfxQueueRingBuffer::ReserveSpaceHelper(uint32 packetsSize)
{
    Result result = Result::Success;

    return result;
}

// =====================================================================================================================
Result GfxQueueRingBuffer::ReleaseSpace(uint32 spaceNeeded)
{
    Result result = Result::Success;

    return result;
}

// =====================================================================================================================
uint64 GfxQueueRingBuffer::GetWptrOffset()
{
    return m_writeOffset - m_startOffset;
}

// =====================================================================================================================
uint64 GfxQueueRingBuffer::GetPreCommitWriteOffset()
{
    return m_preCommitWriteOffset - m_startOffset;
}

// =====================================================================================================================
Result GfxQueueRingBuffer::WritePacketsCommit(uint32 packetsSize, uint64 lastTimestamp)
{
    Result result = Result::Success;

    return result;
}

}
