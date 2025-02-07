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
#pragma once

#include "core/hw/gfxip/gfxQueueRingBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"

namespace Pal
{
namespace Gfx9
{

// Size of NOP Payload to mark End of this UMS.
constexpr uint32 NopPayloadSizeInDwords = 2;

class Gfx9QueueRingBuffer final : public GfxQueueRingBuffer
{
public:
    Gfx9QueueRingBuffer(GfxQueueRingBufferCreateInfo* pGfxQueueRingBufferCreateInfo);

    ~Gfx9QueueRingBuffer();

    Result Init();

    uint32 WriteIndirectBuffer(const Pal::CmdStream * pCmdStream);

    uint32 GetCmdHdpFlushSize();

    uint32 CmdHdpFlush();

    uint32 GetCmdWriteTimestampSize(uint32 stageMask);

    uint32 CmdWriteTimestamp(uint32 stageMask, gpusize dstGpuAddr);

    uint32 GetCmdWaitMemoryValueSize();

    uint32 CmdWaitMemoryValue(gpusize gpuVirtAddr, uint32 data, uint32 mask, CompareFunc compareFunc);

    uint32 GetCmdWriteDataSize(uint32 numDwords);

    uint32 CmdWriteData(gpusize dstAddr, uint32* pData, uint32 numDwords);

    uint32 GetCmdWriteImmediateSize(uint32 stageMask);

    uint32 CmdWriteImmediate(
        uint32             stageMask, // Bitmask of PipelineStageFlag
        uint64             data,
        ImmediateDataWidth dataSize,
        gpusize            address);

    uint32 GetCmdNativeFenceRaiseInterruptSize();

    uint32 CmdNativeFenceRaiseInterrupt(gpusize monitoredValueGpuVa, uint64  signaledVal, uint32  intCtxId);

    uint32 GetEndSubmitSize();

    uint32 EndSubmit(gpusize progressFenceAddr, uint64 nextProgressFenceValue);

    uint32 MarkSubmissionEnd();

    Result ReserveSpaceForWaitSemaphore(
        uint32 numDwordsLogEntry,
        uint32 numDwordsLogHeader,
        uint32* pPacketsSize);

    Result ReserveSpaceForSignalSemaphore(
        uint32 numDwordsLogEntry,
        uint32 numDwordsLogHeader,
        uint32* pPacketsSize);

    Result ReserveSpaceForSubmit(uint32 numCmdStreams, uint32* pPacketsSize);

    void UpdateRingControlBuffer();

    void UpdateRBHeader(uint32 logId, uint64 qpc, uint64 lastCompletedFenceId, uint64 lastRequestedFenceId);

private:
    const CmdUtil& m_cmdUtil;
    void WriteIntoRBHelper(void* pPacket, uint32 packetSize);
    PAL_DISALLOW_DEFAULT_CTOR(Gfx9QueueRingBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9QueueRingBuffer);
};

} // Gfx9
} // Pal
