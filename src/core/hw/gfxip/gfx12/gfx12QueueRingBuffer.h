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
#include "core/hw/gfxip/gfx12/gfx12CmdUtil.h"

namespace Pal
{
namespace Gfx12
{

class Gfx12QueueRingBuffer final : public GfxQueueRingBuffer
{
public:
    Gfx12QueueRingBuffer(GfxQueueRingBufferCreateInfo* pGfxQueueRingBufferCreateInfo);

    ~Gfx12QueueRingBuffer();

    Result Init();

    uint32 WriteIndirectBuffer(const Pal::CmdStream * pCmdStream);

    uint32 EndSubmit(gpusize progressFenceAddr, uint64 nextProgressFenceValue);

    uint32 MarkSubmissionEnd();

    Result ReserveSpaceForWaitSemaphore(
                    uint32  numDwordsLogEntry,
                    uint32  numDwordsLogHeader,
                    uint32* pPacketsSize);

    Result ReserveSpaceForSignalSemaphore(
                    uint32  numDwordsLogEntry,
                    uint32  numDwordsLogHeader,
                    uint32* pPacketsSize);

    Result ReserveSpaceForSubmit(uint32 numCmdStreams, uint32* pPacketsSize);

    void UpdateRingControlBuffer();

    void UpdateRBHeader(uint32 logId, uint64 qpc, uint64 lastCompletedFenceId, uint64 lastRequestedFenceId);

    uint32 CmdWriteImmediate(
        uint32             stageMask, // Bitmask of PipelineStageFlag
        uint64             data,
        ImmediateDataWidth dataSize,
        gpusize            address);

    uint32 CmdNativeFenceRaiseInterrupt(
                gpusize monitoredValueGpuVa,
                uint64  signaledVal,
                uint32  intCtxId);

    uint32 CmdWriteData(gpusize dstAddr, uint32* pData, uint32 numDwords);

    uint32 CmdHdpFlush();

    uint32 CmdWriteTimestamp(uint32 stageMask, gpusize dstGpuAddr);

    uint32 CmdWaitMemoryValue(gpusize gpuVirtAddr, uint32 data, uint32 mask, CompareFunc compareFunc);

private:
    const CmdUtil& m_cmdUtil;
    PAL_DISALLOW_DEFAULT_CTOR(Gfx12QueueRingBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx12QueueRingBuffer);
};

}
}
