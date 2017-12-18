/*
 *******************************************************************************
 *
 * Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "core/cmdStream.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6StreamoutStatsQueryPool.h"
#include "palCmdBuffer.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

static constexpr uint32  QueryTimestampEnd = 0xABCD1234;

struct StreamoutStatsData
{
    uint64 primStorageNeeded; // Number of primitives that would have been written to the SO resource
    uint64 primCountWritten;  // Number of primitives written to the SO resource
};

struct StreamoutStatsDataPair
{
    StreamoutStatsData begin; // streamout stats query result when "begin" was called
    StreamoutStatsData end;   // streamout stats query result when "end" was called
};

static constexpr gpusize StreamoutStatsQueryMemoryAlignment = 32;

static constexpr uint64  StreamoutStatsResetMemValue32 = 0xFFFFFFFF;
static constexpr uint64  StreamoutStatsResetMemValue64 = 0xFFFFFFFFFFFFFFFF;

// =====================================================================================================================
StreamoutStatsQueryPool::StreamoutStatsQueryPool(
    const Device&              device,
    const QueryPoolCreateInfo& createInfo)
    :
    QueryPool(*(device.Parent()),
              createInfo,
              Device::CpDmaCompatAlignment(device, StreamoutStatsQueryMemoryAlignment),
              sizeof(StreamoutStatsDataPair),
              sizeof(uint32)),
    m_device(device)
{
}

// =====================================================================================================================
// Adds the PM4 commands needed to begin this query to the supplied stream.
void StreamoutStatsQueryPool::Begin(
    GfxCmdBuffer*     pCmdBuffer,
    Pal::CmdStream*   pCmdStream,
    QueryType         queryType,
    uint32            slot,
    QueryControlFlags flags
    ) const
{
    PAL_ASSERT((queryType == QueryType::StreamoutStats)  ||
               (queryType == QueryType::StreamoutStats1) ||
               (queryType == QueryType::StreamoutStats2) ||
               (queryType == QueryType::StreamoutStats3));

    PAL_ASSERT(flags.u32All == 0);

    gpusize gpuAddr = 0;
    Result  result  = GetQueryGpuAddress(slot, &gpuAddr);

    if ((result == Result::Success) && pCmdBuffer->IsQueryAllowed(QueryPoolType::StreamoutStats))
    {
        pCmdBuffer->AddQuery(QueryPoolType::StreamoutStats, flags);

        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        VGT_EVENT_TYPE eventType =
            (queryType == QueryType::StreamoutStats)  ? SAMPLE_STREAMOUTSTATS  :
            (queryType == QueryType::StreamoutStats1) ? SAMPLE_STREAMOUTSTATS1 :
            (queryType == QueryType::StreamoutStats2) ? SAMPLE_STREAMOUTSTATS2 : SAMPLE_STREAMOUTSTATS3;

        pCmdSpace += m_device.CmdUtil().BuildEventWriteQuery(eventType, gpuAddr, pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Adds the PM4 commands needed to end this query to the supplied stream.
void StreamoutStatsQueryPool::End(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pCmdStream,
    QueryType       queryType,
    uint32          slot
    ) const
{
    PAL_ASSERT((queryType == QueryType::StreamoutStats) ||
               (queryType == QueryType::StreamoutStats1) ||
               (queryType == QueryType::StreamoutStats2) ||
               (queryType == QueryType::StreamoutStats3));

    gpusize gpuAddr = 0;
    Result  result  = GetQueryGpuAddress(slot, &gpuAddr);

    gpusize timeStampAddr = 0;
    if (result == Result::Success)
    {
        result = GetTimestampGpuAddress(slot, &timeStampAddr);
    }

    if ((result == Result::Success) && pCmdBuffer->IsQueryAllowed(QueryPoolType::StreamoutStats))
    {
        pCmdBuffer->RemoveQuery(QueryPoolType::StreamoutStats);

        VGT_EVENT_TYPE eventType =
            (queryType == QueryType::StreamoutStats) ? SAMPLE_STREAMOUTSTATS :
            (queryType == QueryType::StreamoutStats1) ? SAMPLE_STREAMOUTSTATS1 :
            (queryType == QueryType::StreamoutStats2) ? SAMPLE_STREAMOUTSTATS2 : SAMPLE_STREAMOUTSTATS3;

        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace += m_device.CmdUtil().BuildEventWriteQuery(eventType,
                                                             gpuAddr + sizeof(StreamoutStatsData),
                                                             pCmdSpace);

        pCmdSpace += m_device.CmdUtil().BuildEventWriteEop(BOTTOM_OF_PIPE_TS,
                                                           timeStampAddr,
                                                           EVENTWRITEEOP_DATA_SEL_SEND_DATA32,
                                                           QueryTimestampEnd,
                                                           false,
                                                           pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Adds the PM4 commands needed to stall the ME until the results of the query range are in memory.
void StreamoutStatsQueryPool::WaitForSlots(
    Pal::CmdStream* pCmdStream,
    uint32          startQuery,
    uint32          queryCount
    ) const
{
    // The query slot will be ready when the QueryTimestampEnd is written to the timestamp GPU address. Thus, we
    // must issue one WAIT_REG_MEM for each slot. If the caller specified a large queryCount we may need multiple
    // reserve/commit calls.
    gpusize gpuAddr = 0;
    Result  result  = GetTimestampGpuAddress(startQuery, &gpuAddr);
    PAL_ASSERT(result == Result::Success);

    const auto&  cmdUtil        = m_device.CmdUtil();
    const uint32 waitsPerCommit = pCmdStream->ReserveLimit() / CmdUtil::GetWaitRegMemSize();
    uint32       remainingWaits = queryCount;

    while (remainingWaits > 0)
    {
        // Write all of the waits or as many waits as we can fit in a reserve buffer.
        const uint32 waitsToWrite = Min(remainingWaits, waitsPerCommit);
        uint32*      pCmdSpace    = pCmdStream->ReserveCommands();

        for (uint32 waitIdx = 0; waitIdx < waitsToWrite; ++waitIdx)
        {
            pCmdSpace += cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_MEMORY,
                                                 WAIT_REG_MEM_FUNC_EQUAL,
                                                 WAIT_REG_MEM_ENGINE_ME,
                                                 gpuAddr,
                                                 QueryTimestampEnd,
                                                 0xFFFFFFFF,
                                                 false,
                                                 pCmdSpace);

            // Advance to the next timestamp.
            gpuAddr += m_timestampSizePerSlotInBytes;
        }

        pCmdStream->CommitCommands(pCmdSpace);
        remainingWaits -= waitsToWrite;
    }
}

// =====================================================================================================================
// Adds commands needed to reset this query to the supplied stream on a command buffer that does not support
// PM4 commands, or when an optimized path is unavailable.
// Note that for DX12, except for timestamp all queries occurs on universal queue / direct command list only,
// so CmdResetQuery called in DX12 client driver is  expected to be on universal queue as wellby default, but
// DMA queue still could be selected to do CmdResetQuery on.
void StreamoutStatsQueryPool::NormalReset(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pCmdStream,
    uint32          startQuery,
    uint32          queryCount
    ) const
{
    const gpusize offset   = GetQueryOffset(startQuery);
    const gpusize dataSize = GetGpuResultSizeInBytes(queryCount);

    // This function must only be called by the DMA queue. It is missing a barrier call that is necessary to issue a
    // CS_PARTIAL_FLUSH and L2 cache flush on the universal and compute queues.
    PAL_ASSERT(pCmdBuffer->GetEngineType() == EngineTypeDma);
    PAL_ASSERT(m_gpuMemory.IsBound());

    pCmdBuffer->CmdFillMemory(*m_gpuMemory.Memory(), offset, dataSize, StreamoutStatsResetMemValue32);

    // Reset the memory for querypool timestamps.
    pCmdBuffer->CmdFillMemory(*m_gpuMemory.Memory(),
                              GetTimestampOffset(startQuery),
                              m_timestampSizePerSlotInBytes * queryCount,
                              0);
}

// =====================================================================================================================
// Adds the PM4 commands needed to reset this query to the supplied stream on a command buffer built for PM4 commands.
// NOTE: It is safe to call this with a command buffer that does not support occlusion queries.
void StreamoutStatsQueryPool::OptimizedReset(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pCmdStream,
    uint32          startQuery,
    uint32          queryCount
    ) const
{
    const auto& cmdUtil = m_device.CmdUtil();
    uint32*     pCmdSpace = pCmdStream->ReserveCommands();

    if (pCmdBuffer->IsQueryAllowed(QueryPoolType::StreamoutStats))
    {
        // Before we initialize out the GPU's destination memory, make sure the ASIC has finished any previous reading
        // and writing of streamout stat data.

        // Command buffers that do not support stats queries do not need to issue this wait because the caller must use
        // semaphores to make sure all queries are complete.
        pCmdSpace += cmdUtil.BuildEventWrite(CS_PARTIAL_FLUSH, pCmdSpace);

        // And make sure the graphics pipeline is idled here.
        pCmdSpace += cmdUtil.BuildWaitOnGenericEopEvent(BOTTOM_OF_PIPE_TS,
                                                        pCmdBuffer->TimestampGpuVirtAddr(),
                                                        pCmdBuffer->GetEngineType() == EngineTypeCompute,
                                                        pCmdSpace);
    }

    gpusize gpuAddr          = 0;
    gpusize timestampGpuAddr = 0;
    Result  result           = GetQueryGpuAddress(startQuery, &gpuAddr);

    if (result == Result::Success)
    {
        result = GetTimestampGpuAddress(startQuery, &timestampGpuAddr);
    }
    PAL_ALERT(result != Result::Success);

    // Issue a CPDMA packet to zero out the memory associated with all the slots we're going to reset.
    DmaDataInfo dmaData  = {};
    dmaData.dstSel       = CPDMA_DST_SEL_DST_ADDR;
    dmaData.dstAddr      = gpuAddr;
    dmaData.dstAddrSpace = CPDMA_ADDR_SPACE_MEM;
    dmaData.srcSel       = CPDMA_SRC_SEL_DATA;
    dmaData.srcData      = StreamoutStatsResetMemValue32;
    dmaData.numBytes     = static_cast<uint32>(GetGpuResultSizeInBytes(queryCount));
    dmaData.sync         = 1;
    dmaData.usePfp       = false;

    DmaDataInfo tsDmaData  = {};
    tsDmaData.dstSel       = CPDMA_DST_SEL_DST_ADDR;
    tsDmaData.dstAddr      = timestampGpuAddr;
    tsDmaData.dstAddrSpace = CPDMA_ADDR_SPACE_MEM;
    tsDmaData.srcSel       = CPDMA_SRC_SEL_DATA;
    tsDmaData.srcData      = 0;
    tsDmaData.numBytes     = queryCount * static_cast<uint32>(m_timestampSizePerSlotInBytes);
    tsDmaData.sync         = 1;
    tsDmaData.usePfp       = false;

    pCmdSpace += cmdUtil.BuildDmaData(dmaData, pCmdSpace);
    pCmdSpace += cmdUtil.BuildDmaData(tsDmaData, pCmdSpace);

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Computes the size each CPU result needs for one slot.
size_t StreamoutStatsQueryPool::GetResultSizeForOneSlot(
    QueryResultFlags flags
    ) const
{
    // Currently this function seems to be just referenced in QueryPool::GetResults so it doesn't
    // even need to be implemented at this point but just put two lines of code as simple enough

    PAL_ASSERT(flags == (QueryResult64Bit | QueryResultWait));

    // primStorageNeeded and primCountWritten
    return sizeof(StreamoutStatsData);
}

// =====================================================================================================================
// Dummy function at this point as streamout query is currently DX12 specific where the query has no GetQueryData API
bool StreamoutStatsQueryPool::ComputeResults(
    QueryResultFlags flags,
    QueryType        queryType,
    uint32           queryCount,
    size_t           stride,
    const void*      pGpuData,
    void*            pData)
{
    PAL_NOT_IMPLEMENTED();

    return true;
}

} // Gfx6
} // Pal
