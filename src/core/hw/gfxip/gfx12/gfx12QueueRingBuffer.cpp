/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx12/gfx12QueueRingBuffer.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"

namespace Pal
{
namespace Gfx12
{
// =====================================================================================================================
Gfx12QueueRingBuffer::Gfx12QueueRingBuffer(
    GfxQueueRingBufferCreateInfo* pGfxQueueRingBufferCreateInfo)
    :
    GfxQueueRingBuffer(pGfxQueueRingBufferCreateInfo),
    m_cmdUtil(static_cast<Device*>(m_pGfxDevice)->CmdUtil())
{
}

// =====================================================================================================================
Gfx12QueueRingBuffer::~Gfx12QueueRingBuffer()
{
    PAL_NOT_IMPLEMENTED();
}

// =====================================================================================================================
Result Gfx12QueueRingBuffer::Init()
{
    Result result = Result::Unsupported;

    PAL_NOT_IMPLEMENTED();

    return result;
}

// =====================================================================================================================
uint32 Gfx12QueueRingBuffer::WriteIndirectBuffer(const Pal::CmdStream* pCmdStream)
{
    PAL_NOT_IMPLEMENTED();

    return 0;
}

// =====================================================================================================================
uint32 Gfx12QueueRingBuffer::EndSubmit(gpusize progressFenceAddr, uint64 nextProgressFenceValue)
{
    PAL_NOT_IMPLEMENTED();

    return 0;
}

// =====================================================================================================================
uint32 Gfx12QueueRingBuffer::MarkSubmissionEnd()
{
    PAL_NOT_IMPLEMENTED();

    return 0;
}

// =====================================================================================================================
Result Gfx12QueueRingBuffer::ReserveSpaceForWaitSemaphore(
    uint32 numDwordsLogEntry,
    uint32 numDwordsLogHeader,
    uint32* pPacketsSize)
{
    Result result = Result::Unsupported;

    PAL_NOT_IMPLEMENTED();

    return result;
}

// =====================================================================================================================
Result Gfx12QueueRingBuffer::ReserveSpaceForSignalSemaphore(
    uint32 numDwordsLogEntry,
    uint32 numDwordsLogHeader,
    uint32* pPacketsSize)
{
    Result result = Result::Unsupported;

    PAL_NOT_IMPLEMENTED();

    return result;
}

// =====================================================================================================================
Result Gfx12QueueRingBuffer::ReserveSpaceForSubmit(uint32 numCmdStreams, uint32* pPacketsSize)
{
    Result result = Result::Unsupported;

    PAL_NOT_IMPLEMENTED();

    return result;
}

// =====================================================================================================================
void Gfx12QueueRingBuffer::UpdateRingControlBuffer()
{
    PAL_NOT_IMPLEMENTED();
}

// =====================================================================================================================
void Gfx12QueueRingBuffer::UpdateRBHeader(
    uint32 logId,
    uint64 qpc,
    uint64 lastCompletedFenceId,
    uint64 lastRequestedFenceId)
{
    PAL_NOT_IMPLEMENTED();
}

// =====================================================================================================================
uint32 Gfx12QueueRingBuffer::CmdWriteImmediate(
    uint32             stageMask, // Bitmask of PipelineStageFlag
    uint64             data,
    ImmediateDataWidth dataSize,
    gpusize            address)
{
    PAL_NOT_IMPLEMENTED();

    return 0;
}

// =====================================================================================================================
uint32 Gfx12QueueRingBuffer::CmdNativeFenceRaiseInterrupt(
    gpusize monitoredValueGpuVa,
    uint64  signaledVal,
    uint32  intCtxId)
{
    PAL_NOT_IMPLEMENTED();

    return 0;
}

// =====================================================================================================================
uint32 Gfx12QueueRingBuffer::CmdWriteData(
    gpusize dstAddr,
    uint32* pData,
    uint32 numDwords)
{
    PAL_NOT_IMPLEMENTED();

    return 0;
}

// =====================================================================================================================
uint32 Gfx12QueueRingBuffer::CmdHdpFlush()
{
    PAL_NOT_IMPLEMENTED();

    return 0;
}

// =====================================================================================================================
uint32 Gfx12QueueRingBuffer::CmdWriteTimestamp(
    uint32 stageMask,
    gpusize dstGpuAddr)
{
    PAL_NOT_IMPLEMENTED();

    return 0;
}

// =====================================================================================================================
uint32 Gfx12QueueRingBuffer::CmdWaitMemoryValue(
    gpusize gpuVirtAddr,
    uint32 data,
    uint32 mask,
    CompareFunc compareFunc)
{
    PAL_NOT_IMPLEMENTED();

    return 0;
}

} // namespace Gfx12
} // namespace Pal
