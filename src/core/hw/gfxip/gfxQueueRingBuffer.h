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
#pragma once

#include "palDequeImpl.h"
#include "pal.h"
#include "palCmdBuffer.h"

namespace Pal
{
class  GfxDevice;
class  Queue;
class  GpuMemory;
class  CmdStream;
class  Platform;

struct GfxRbTsSize
{
    uint64 timestamp;
    uint32 submittedWorkSize;
};

struct GfxQueueRingBufferCreateInfo
{
    GfxDevice* pGfxDevice;
    Queue*     pParentQueue;
    GpuMemory* pUMSRingBuffer;
    gpusize    umsRBSize;
    GpuMemory* pUMSRingControlBuffer;
    gpusize    umsRCBSize;
    EngineType engineType;
};

class GfxQueueRingBuffer
{
public:
    GfxQueueRingBuffer(GfxQueueRingBufferCreateInfo* pGfxQueueRingBufferCreateInfo);
    virtual ~GfxQueueRingBuffer() { };

    virtual uint32 WriteIndirectBuffer(const CmdStream* pCmdStream) = 0;

    virtual uint32 EndSubmit(gpusize progressFenceAddr, uint64 nextProgressFenceValue) = 0;

    // @logId the index of Log entry where the data below is recorded.
    // @DoorbellRingTimeQpc  QPC timestamp of the doorbell ring for this submission
    // @LastCompletedFenceId Value of the progress fence at the time of submission
    // @LastRequestedFenceId Value of the progress fence for this submission
    virtual void UpdateRBHeader(
        uint32 logId,
        uint64 qpc,
        uint64 lastCompletedFenceId,
        uint64 lastRequestedFenceId) = 0;

    virtual Result ReserveSpaceForSubmit(uint32 numCmdStreams, uint32* pPacketsSize) = 0;

    virtual Result ReserveSpaceForWaitSemaphore(
                    uint32 numDwordsLogEntry,
                    uint32 numDwordsLogHeader,
                    uint32* pPacketsSize) = 0;

    virtual Result ReserveSpaceForSignalSemaphore(
                    uint32 numDwordsLogEntry,
                    uint32 numDwordsLogHeader,
                    uint32* pPacketsSize) = 0;

    Result ReleaseSpace(uint32 spaceNeeded);

    virtual Result Init() = 0;

    uint64 GetWptrOffset();

    uint64 GetPreCommitWriteOffset();

    virtual void UpdateRingControlBuffer() = 0;

    Result WritePacketsCommit(uint32 packetsSize, uint64 lastTimestamp);

    virtual uint32 CmdWriteImmediate(
        uint32             stageMask, // Bitmask of PipelineStageFlag
        uint64             data,
        ImmediateDataWidth dataSize,
        gpusize            address) = 0;

    virtual uint32 CmdNativeFenceRaiseInterrupt(
                    gpusize monitoredValueGpuVa,
                    uint64  signaledVal,
                    uint32  intCtxId) = 0;

    virtual uint32 CmdWriteData(gpusize dstAddr, uint32* pData, uint32 numDwords) = 0;

    virtual uint32 CmdHdpFlush() = 0;

    virtual uint32 CmdWriteTimestamp(uint32 stageMask, gpusize dstGpuAddr) = 0;

    virtual uint32 CmdWaitMemoryValue(gpusize gpuVirtAddr, uint32 data, uint32 mask, CompareFunc compareFunc) = 0;

    static constexpr uint32 NumUMSRBLogEntries = 32;

protected:
    Result ReserveSpaceHelper(uint32 packetsSize);

    GfxDevice* m_pGfxDevice;
    Queue* m_pParentQueue;
    GpuMemory* m_pUMSRingBuffer;
    void* m_pUMSRbCpuAddr;
    gpusize m_umsRbSize;
    GpuMemory* m_pUMSRingControlBuffer;
    void* m_pUMSRcbCpuAddr;
    gpusize m_umsRcbSize;
    uint32 m_startOffset;
    uint32 m_endOffset;
    uint32 m_readOffset;
    uint32 m_writeOffset;
    uint32 m_preCommitWriteOffset;
    uint32 m_ringHeaderSize;
    uint32 m_numAvailableDwords;
    uint32 m_numReservedDwords;

    EngineType m_engineType;

    PAL_DISALLOW_DEFAULT_CTOR(GfxQueueRingBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(GfxQueueRingBuffer);
};

}
