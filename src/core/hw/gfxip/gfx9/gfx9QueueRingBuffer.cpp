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
#include "core/hw/gfxip/gfx9/gfx9QueueRingBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"

namespace Pal
{
namespace Gfx9
{
// =====================================================================================================================
Gfx9QueueRingBuffer::Gfx9QueueRingBuffer(
    GfxQueueRingBufferCreateInfo* pGfxQueueRingBufferCreateInfo)
    :
    GfxQueueRingBuffer(pGfxQueueRingBufferCreateInfo),
    m_cmdUtil(static_cast<Device*>(m_pGfxDevice)->CmdUtil())
{
}

// =====================================================================================================================
Gfx9QueueRingBuffer::~Gfx9QueueRingBuffer()
{
}

// =====================================================================================================================
Result Gfx9QueueRingBuffer::Init()
{
    Result result = Result::Success;

    return result;
}

// =====================================================================================================================
void Gfx9QueueRingBuffer::WriteIntoRBHelper(
    void* pPacket,
    uint32 packetSizeInBytes)
{
    const bool isRbWrapAround = ((m_preCommitWriteOffset + packetSizeInBytes) > m_endOffset);

    if (isRbWrapAround)
    {
        uint32 firstSegmentSize = m_endOffset - m_preCommitWriteOffset + 1;
        uint32 secondSegmentSize = packetSizeInBytes - firstSegmentSize;
        memcpy(VoidPtrInc(m_pUMSRbCpuAddr, m_preCommitWriteOffset), pPacket, firstSegmentSize);
        memcpy(VoidPtrInc(m_pUMSRbCpuAddr, m_startOffset), VoidPtrInc(pPacket, firstSegmentSize), secondSegmentSize);
        m_preCommitWriteOffset = m_startOffset + secondSegmentSize;
        PAL_ASSERT(m_preCommitWriteOffset <= m_readOffset);
    }
    else
    {
        memcpy(VoidPtrInc(m_pUMSRbCpuAddr, m_preCommitWriteOffset), pPacket, packetSizeInBytes);
        m_preCommitWriteOffset += packetSizeInBytes;
        PAL_ASSERT(m_preCommitWriteOffset <= m_endOffset);
    }
}

// =====================================================================================================================
uint32 Gfx9QueueRingBuffer::GetCmdNativeFenceRaiseInterruptSize()
{
    return PM4_MEC_RELEASE_MEM_SIZEDW__CORE * sizeof(uint32);
}

// =====================================================================================================================
uint32 Gfx9QueueRingBuffer::CmdNativeFenceRaiseInterrupt(
    gpusize monitoredValueGpuVa,
    uint64  signaledVal,
    uint32  intCtxId)
{
    constexpr uint32 packetSizeInDwords = PM4_MEC_RELEASE_MEM_SIZEDW__CORE;

    return packetSizeInDwords * sizeof(uint32);
}

// =====================================================================================================================
uint32 Gfx9QueueRingBuffer::GetCmdWriteImmediateSize(
    uint32 stageMask)
{
    uint32 packetSizeInDwords = 0;

    if (TestAnyFlagSet(stageMask, PipelineStageCs | PipelineStageBlt | PipelineStageBottomOfPipe))
    {
        packetSizeInDwords = PM4_ME_RELEASE_MEM_SIZEDW__CORE;
    }
    else
    {
        packetSizeInDwords = PM4_MEC_COPY_DATA_SIZEDW__CORE;
    }

    return packetSizeInDwords * sizeof(uint32);
}

// =====================================================================================================================
uint32 Gfx9QueueRingBuffer::CmdWriteImmediate(
    uint32             stageMask, // Bitmask of PipelineStageFlag
    uint64             data,
    ImmediateDataWidth dataSize,
    gpusize            address)
{
    const bool is32Bit = (dataSize == ImmediateDataWidth::ImmediateData32Bit);

    uint32 packetSizeInDwords = 0;

    // If multiple flags are set we must go down the path that is most conservative (writes at the latest point).
    // This is easiest to implement in this order:
    // 1. The EOP path for compute shaders.
    // 2. The CP stages can write the value directly using COPY_DATA in the MEC.
    // Note that passing in a stageMask of zero will get you an MEC write. It's not clear if that is even legal but
    // doing an MEC write is probably the least impactful thing we could do in that case.
    if (TestAnyFlagSet(stageMask, PipelineStageCs | PipelineStageBlt | PipelineStageBottomOfPipe))
    {
        packetSizeInDwords        = PM4_ME_RELEASE_MEM_SIZEDW__CORE;
        PM4_ME_RELEASE_MEM packet = {};

        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.dstAddr    = address;
        releaseInfo.data       = data;
        releaseInfo.dataSel    = is32Bit ? data_sel__mec_release_mem__send_32_bit_low
                                         : data_sel__mec_release_mem__send_64_bit_data;

        m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, &packet);

        WriteIntoRBHelper(&packet, packetSizeInDwords * sizeof(uint32));
    }
    else
    {
        packetSizeInDwords       = PM4_MEC_COPY_DATA_SIZEDW__CORE;
        PM4_MEC_COPY_DATA packet = { };

        m_cmdUtil.BuildCopyData(EngineTypeCompute,
                                0,
                                dst_sel__mec_copy_data__tc_l2_obsolete,
                                address,
                                src_sel__mec_copy_data__immediate_data,
                                data,
                                is32Bit ? count_sel__mec_copy_data__32_bits_of_data
                                        : count_sel__mec_copy_data__64_bits_of_data,
                                wr_confirm__mec_copy_data__wait_for_confirmation,
                                &packet);

        WriteIntoRBHelper(&packet, packetSizeInDwords * sizeof(uint32));
    }

    return packetSizeInDwords * sizeof(uint32);
}

// =====================================================================================================================
uint32 Gfx9QueueRingBuffer::GetCmdWriteDataSize(
    uint32 numDwords)
{
    uint32 packetSizeInDwords = PM4_ME_WRITE_DATA_SIZEDW__CORE + numDwords;

    return packetSizeInDwords * sizeof(uint32);
}

// =====================================================================================================================
uint32 Gfx9QueueRingBuffer::CmdWriteData(gpusize dstAddr, uint32* pData, uint32 numDwords)
{
    uint32 packetSizeInBytes = PM4_ME_WRITE_DATA_SIZEDW__CORE * sizeof(uint32);

    // We build the packet with the ME definition, but the MEC definition is identical, so it should work...
    PM4_ME_WRITE_DATA packet = { };

    WriteDataInfo writeDataInfo = { };
    writeDataInfo.engineType    = m_engineType;
    writeDataInfo.dstSel        = dst_sel__mec_write_data__memory;
    writeDataInfo.dstAddr       = dstAddr;

    const uint32 packetSizeWithWrittenDwords =
        static_cast<uint32>(CmdUtil::BuildWriteDataInternal(writeDataInfo, numDwords, &packet));

    WriteIntoRBHelper(&packet, packetSizeInBytes);

    WriteIntoRBHelper(pData, numDwords*sizeof(uint32));

    return packetSizeWithWrittenDwords;
}

// =====================================================================================================================
uint32 Gfx9QueueRingBuffer::GetCmdWaitMemoryValueSize()
{
    return PM4_MEC_WAIT_REG_MEM_SIZEDW__CORE * sizeof(uint32);
}

// =====================================================================================================================
uint32 Gfx9QueueRingBuffer::CmdWaitMemoryValue(gpusize gpuVirtAddr, uint32 data, uint32 mask, CompareFunc compareFunc)
{
    uint32 packetSizeInDwords = 0;
    uint32 packetSizeInBytes  = 0;

    PM4_MEC_WAIT_REG_MEM packet = { };
    packetSizeInDwords = static_cast<uint32>(m_cmdUtil.BuildWaitRegMem(EngineTypeCompute,
                                                    mem_space__me_wait_reg_mem__memory_space,
                                                    CmdUtil::WaitRegMemFunc(compareFunc),
                                                    engine_sel__me_wait_reg_mem__micro_engine,
                                                    gpuVirtAddr,
                                                    data,
                                                    mask,
                                                    &packet));

    packetSizeInBytes = packetSizeInDwords * sizeof(uint32);

    WriteIntoRBHelper(&packet, packetSizeInBytes);

    return packetSizeInBytes;
}

// =====================================================================================================================
uint32 Gfx9QueueRingBuffer::GetCmdHdpFlushSize()
{
    return PM4_MEC_HDP_FLUSH_SIZEDW__CORE * sizeof(uint32);
}

// =====================================================================================================================
uint32 Gfx9QueueRingBuffer::CmdHdpFlush()
{
    PM4_MEC_HDP_FLUSH packet = { };

    uint32 packetSizeInDwords = static_cast<uint32>(m_cmdUtil.BuildHdpFlush(&packet));

    uint32 packetSizeInBytes = packetSizeInDwords * sizeof(uint32);

    WriteIntoRBHelper(&packet, packetSizeInBytes);

    return packetSizeInBytes;
}

// =====================================================================================================================
uint32 Gfx9QueueRingBuffer::GetCmdWriteTimestampSize(uint32 stageMask)
{
    uint32 packetSizeInDwords = 0;

    if (TestAnyFlagSet(stageMask, PipelineStageCs | PipelineStageBlt | PipelineStageBottomOfPipe))
    {
        packetSizeInDwords = PM4_ME_RELEASE_MEM_SIZEDW__CORE;
    }
    else
    {
        packetSizeInDwords = PM4_ME_COPY_DATA_SIZEDW__CORE;
    }

    return packetSizeInDwords * sizeof(uint32);
}

// =====================================================================================================================
uint32 Gfx9QueueRingBuffer::CmdWriteTimestamp(uint32 stageMask, gpusize dstGpuAddr)
{
    uint32 packetSizeInDwords = 0;
    uint32 packetSizeInBytes  = 0;

    // If multiple flags are set we must go down the path that is most conservative (writes at the latest point).
    // This is easiest to implement in this order:
    // 1. The EOP path for compute shaders.
    // 2. The CP stages can write the value directly using COPY_DATA in the MEC.
    // Note that passing in a stageMask of zero will get you an MEC write. It's not clear if that is even legal but
    // doing an MEC write is probably the least impactful thing we could do in that case.
    if (TestAnyFlagSet(stageMask, PipelineStageCs | PipelineStageBlt | PipelineStageBottomOfPipe))
    {
        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.dstAddr           = dstGpuAddr;
        releaseInfo.dataSel           = data_sel__mec_release_mem__send_gpu_clock_counter;

        PM4_ME_RELEASE_MEM packet = {};

        packetSizeInDwords = static_cast<uint32>(m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, &packet));

        packetSizeInBytes = packetSizeInDwords * sizeof(uint32);

        WriteIntoRBHelper(&packet, packetSizeInBytes);
    }
    else
    {
        PM4_MEC_COPY_DATA packet = { };

        packetSizeInDwords = static_cast<uint32>(m_cmdUtil.BuildCopyData(
                                                     EngineTypeCompute,
                                                     0,
                                                     dst_sel__mec_copy_data__tc_l2_obsolete,
                                                     dstGpuAddr,
                                                     src_sel__mec_copy_data__gpu_clock_count,
                                                     0,
                                                     count_sel__mec_copy_data__64_bits_of_data,
                                                     wr_confirm__mec_copy_data__wait_for_confirmation,
                                                     &packet));

        packetSizeInBytes = packetSizeInDwords * sizeof(uint32);

        WriteIntoRBHelper(&packet, packetSizeInBytes);
    }

    return packetSizeInBytes;
}

// =====================================================================================================================
uint32 Gfx9QueueRingBuffer::WriteIndirectBuffer(const Pal::CmdStream* pCmdStream)
{
    const CmdStreamChunk* const pChunk = pCmdStream->GetFirstChunk();

    PM4_PFP_INDIRECT_BUFFER packet = {};

    uint32 packetSizeInDwords = static_cast<uint32>(m_cmdUtil.BuildIndirectBuffer(
                                                    m_engineType,
                                                    pChunk->GpuVirtAddr(),
                                                    pChunk->CmdDwordsToExecute(),
                                                    false,
                                                    (pCmdStream->GetSubEngineType() == SubEngineType::ConstantEngine),
                                                    pCmdStream->IsPreemptionEnabled(),
                                                    &packet));

    uint32 packetSizeInBytes = packetSizeInDwords * sizeof(uint32);

    WriteIntoRBHelper(&packet, packetSizeInBytes);

    return packetSizeInBytes;
}

// =====================================================================================================================
uint32 Gfx9QueueRingBuffer::GetEndSubmitSize()
{
    return PM4_ME_RELEASE_MEM_SIZEDW__CORE * sizeof(uint32);
}

// =====================================================================================================================
uint32 Gfx9QueueRingBuffer::EndSubmit(gpusize progressFenceAddr, uint64 nextProgressFenceValue)
{
    return 0;
}

// =====================================================================================================================
Result Gfx9QueueRingBuffer::ReserveSpaceForWaitSemaphore(
                            uint32 numDwordsLogEntry,
                            uint32 numDwordsLogHeader,
                            uint32* pPacketsSize)
{
    Result result = Result::Success;

    return result;
}

// =====================================================================================================================
Result Gfx9QueueRingBuffer::ReserveSpaceForSignalSemaphore(
                         uint32  numDwordsLogEntry,
                         uint32  numDwordsLogHeader,
                         uint32* pPacketsSize)
{
    Result result = Result::Success;

    return result;
}

// =====================================================================================================================
Result Gfx9QueueRingBuffer::ReserveSpaceForSubmit(uint32 numCmdStreams, uint32* pPacketsSize)
{
    Result result = Result::Success;

    return result;
}

// =====================================================================================================================
void Gfx9QueueRingBuffer::UpdateRingControlBuffer()
{
}

// =====================================================================================================================
void Gfx9QueueRingBuffer::UpdateRBHeader(
    uint32 logId,
    uint64 qpc,
    uint64 lastCompletedFenceId,
    uint64 lastRequestedFenceId)
{
}

} // Gfx9
} // Pal
