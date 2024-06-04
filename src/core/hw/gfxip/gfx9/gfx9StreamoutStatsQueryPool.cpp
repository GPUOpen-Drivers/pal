/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/cmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9StreamoutStatsQueryPool.h"
#include "core/hw/gfxip/pm4CmdBuffer.h"
#include "palCmdBuffer.h"
#include "palSysUtil.h"

#include <atomic>

using namespace Util;

namespace Pal
{
namespace Gfx9
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

static constexpr uint32  StreamoutStatsResetMemValue32 = 0;
static constexpr uint64  StreamoutStatsResultValidMask = 0x8000000000000000ull;

// =====================================================================================================================
StreamoutStatsQueryPool::StreamoutStatsQueryPool(
    const Device&              device,
    const QueryPoolCreateInfo& createInfo)
    :
    QueryPool(*(device.Parent()),
              createInfo,
              StreamoutStatsQueryMemoryAlignment,
              sizeof(StreamoutStatsDataPair),
              sizeof(uint32)),
    m_device(device)
{
}

// =====================================================================================================================
// Translates between an API event type an the corresponding VGT event type
VGT_EVENT_TYPE StreamoutStatsQueryPool::XlateEventType(
    QueryType  queryType
    ) const
{
    PAL_ASSERT((queryType == QueryType::StreamoutStats)  ||
               (queryType == QueryType::StreamoutStats1) ||
               (queryType == QueryType::StreamoutStats2) ||
               (queryType == QueryType::StreamoutStats3));

    VGT_EVENT_TYPE eventType = SAMPLE_STREAMOUTSTATS;

    if (m_device.Parent()->ChipProperties().gfxip.supportsSwStrmout != 0)
    {
        // With software streamout, the hardware no longer has any concept of streamout VGT_EVENTs. As a result,
        // we need to issue a VS_PARTIAL_FLUSH - to ensure all NGG waves have completed - and then we will have to
        // tell the CP to use a specific EventIndex that indicates which buffer we wish to sample from.
        eventType = VS_PARTIAL_FLUSH;
    }
    else
    {
        eventType = (queryType == QueryType::StreamoutStats)  ? SAMPLE_STREAMOUTSTATS  :
                    (queryType == QueryType::StreamoutStats1) ? SAMPLE_STREAMOUTSTATS1 :
                    (queryType == QueryType::StreamoutStats2) ? SAMPLE_STREAMOUTSTATS2 : SAMPLE_STREAMOUTSTATS3;
    }

    return eventType;
}

// =====================================================================================================================
// Translates between an API event type an the corresponding CP event index.
ME_EVENT_WRITE_event_index_enum StreamoutStatsQueryPool::XlateEventIndex(
    QueryType  queryType
    ) const
{
    PAL_ASSERT((queryType == QueryType::StreamoutStats)  ||
               (queryType == QueryType::StreamoutStats1) ||
               (queryType == QueryType::StreamoutStats2) ||
               (queryType == QueryType::StreamoutStats3));

    ME_EVENT_WRITE_event_index_enum eventIndex = event_index__me_event_write__sample_streamoutstats__GFX10;

    constexpr auto StreamoutStats0 = static_cast<ME_EVENT_WRITE_event_index_enum>(8);
    constexpr auto StreamoutStats1 = static_cast<ME_EVENT_WRITE_event_index_enum>(9);
    constexpr auto StreamoutStats2 = static_cast<ME_EVENT_WRITE_event_index_enum>(10);
    constexpr auto StreamoutStats3 = static_cast<ME_EVENT_WRITE_event_index_enum>(11);

    static_assert(StreamoutStats0 == event_index__me_event_write__sample_streamoutstats__GFX11, "Check reg defs!");
    static_assert(StreamoutStats1 == event_index__me_event_write__sample_streamoutstats1__GFX11, "Check reg defs!");
    static_assert(StreamoutStats2 == event_index__me_event_write__sample_streamoutstats2__GFX11, "Check reg defs!");
    static_assert(StreamoutStats3 == event_index__me_event_write__sample_streamoutstats3__GFX11, "Check reg defs!");

    if (m_device.Parent()->ChipProperties().gfxip.supportsSwStrmout != 0)
    {
        eventIndex =
            (queryType == QueryType::StreamoutStats)  ? StreamoutStats0 :
            (queryType == QueryType::StreamoutStats1) ? StreamoutStats1 :
            (queryType == QueryType::StreamoutStats2) ? StreamoutStats2 :
                                                        StreamoutStats3;
    }

    return eventIndex;
}

// =====================================================================================================================
// Begins a single query
void StreamoutStatsQueryPool::Begin(
    GfxCmdBuffer*     pCmdBuffer,
    Pal::CmdStream*   pCmdStream,
    Pal::CmdStream*   pHybridCmdStream,
    QueryType         queryType,
    uint32            slot,
    QueryControlFlags flags
    ) const
{
    PAL_ASSERT(flags.u32All == 0);

    gpusize gpuAddr = 0;
    Result  result  = GetQueryGpuAddress(slot, &gpuAddr);

    if ((result == Result::Success) && pCmdBuffer->IsQueryAllowed(QueryPoolType::StreamoutStats))
    {
        const CmdUtil& cmdUtil = m_device.CmdUtil();

        pCmdBuffer->AddQuery(QueryPoolType::StreamoutStats, flags);

        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        pCmdSpace += cmdUtil.BuildSampleEventWrite(XlateEventType(queryType),
                                                   XlateEventIndex(queryType),
                                                   pCmdBuffer->GetEngineType(),
                                                   samp_plst_cntr_mode__mec_event_write__legacy_mode__GFX11,
                                                   gpuAddr,
                                                   pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Ends a single query
void StreamoutStatsQueryPool::End(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pCmdStream,
    Pal::CmdStream* pHybridCmdStream,
    QueryType       queryType,
    uint32          slot
    ) const
{
    gpusize gpuAddr       = 0;
    Result  result        = GetQueryGpuAddress(slot, &gpuAddr);
    gpusize timeStampAddr = 0;

    if (result == Result::Success)
    {
        result = GetTimestampGpuAddress(slot, &timeStampAddr);
    }

    if ((result == Result::Success) && pCmdBuffer->IsQueryAllowed(QueryPoolType::StreamoutStats))
    {
        const CmdUtil& cmdUtil = m_device.CmdUtil();

        pCmdBuffer->RemoveQuery(QueryPoolType::StreamoutStats);

        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace += cmdUtil.BuildSampleEventWrite(XlateEventType(queryType),
                                                   XlateEventIndex(queryType),
                                                   pCmdBuffer->GetEngineType(),
                                                   samp_plst_cntr_mode__mec_event_write__legacy_mode__GFX11,
                                                   gpuAddr + sizeof(StreamoutStatsData),
                                                   pCmdSpace);

        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.engineType = pCmdBuffer->GetEngineType();
        releaseInfo.dstAddr    = timeStampAddr;
        releaseInfo.dataSel    = data_sel__me_release_mem__send_32_bit_low;
        releaseInfo.data       = QueryTimestampEnd;

        pCmdSpace += cmdUtil.BuildReleaseMemGeneric(releaseInfo, pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Adds the PM4 commands needed to stall the ME until the results of the query range are in memory.
void StreamoutStatsQueryPool::WaitForSlots(
    GfxCmdBuffer*   pCmdBuffer,
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

    const uint32 waitsPerCommit = pCmdStream->ReserveLimit() / CmdUtil::WaitRegMemSizeDwords;
    uint32       remainingWaits = queryCount;

    while (remainingWaits > 0)
    {
        // Write all of the waits or as many waits as we can fit in a reserve buffer.
        const uint32 waitsToWrite = Min(remainingWaits, waitsPerCommit);
        uint32*      pCmdSpace    = pCmdStream->ReserveCommands();

        for (uint32 waitIdx = 0; waitIdx < waitsToWrite; ++waitIdx)
        {
            pCmdSpace += CmdUtil::BuildWaitRegMem(pCmdStream->GetEngineType(),
                                                  mem_space__me_wait_reg_mem__memory_space,
                                                  function__me_wait_reg_mem__equal_to_the_reference_value,
                                                  engine_sel__me_wait_reg_mem__micro_engine,
                                                  gpuAddr,
                                                  QueryTimestampEnd,
                                                  0xFFFFFFFF,
                                                  pCmdSpace);

            // Advance to the next timestamp.
            gpuAddr += m_timestampSizePerSlotInBytes;
        }

        pCmdStream->CommitCommands(pCmdSpace);
        remainingWaits -= waitsToWrite;
    }
}

// =====================================================================================================================
// Reset this query with CPU.
Result StreamoutStatsQueryPool::Reset(
    uint32  startQuery,
    uint32  queryCount,
    void*   pMappedCpuAddr)
{
    Result result = ValidateSlot(startQuery + queryCount - 1);

    if (result == Result::Success)
    {
        result = DoReset(startQuery,
                         queryCount,
                         pMappedCpuAddr,
                         4,
                         &StreamoutStatsResetMemValue32);
    }

    return result;
}

// =====================================================================================================================
// Reset query using DMA, when NormalReset() can't be used or the command buffer does not support PM4.
void StreamoutStatsQueryPool::DmaEngineReset(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pCmdStream,
    uint32          startQuery,
    uint32          queryCount
    ) const
{
    const gpusize offset   = GetQueryOffset(startQuery);
    const gpusize dataSize = GetGpuResultSizeInBytes(queryCount);

    // This function must only be called by the DMA queue. It is missing a barrier call that is necessary to issue a
    // CS_PARTIAL_FLUSH on the universal and compute queues.
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
// Reset query via PM4 commands on a PM4-supported command buffer.
// NOTE: It is safe to call this with a command buffer that does not support streamout queries.
void StreamoutStatsQueryPool::NormalReset(
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
        Pm4CmdBuffer* pPm4CmdBuf = static_cast<Pm4CmdBuffer*>(pCmdBuffer);

        // Before we initialize out the GPU's destination memory, make sure the ASIC has finished any previous reading
        // and writing of streamout stat data. Command buffers that do not support stats queries do not need to issue
        // this wait because the caller must use semaphores to make sure all queries are complete.
        pCmdSpace = pPm4CmdBuf->WriteWaitEop(HwPipePostPrefetch, false, SyncGlxNone, SyncRbNone, pCmdSpace);
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
    dmaData.dstSel       = dst_sel__pfp_dma_data__dst_addr_using_l2;
    dmaData.dstAddr      = gpuAddr;
    dmaData.dstAddrSpace = das__pfp_dma_data__memory;
    dmaData.srcSel       = src_sel__pfp_dma_data__data;
    dmaData.srcData      = StreamoutStatsResetMemValue32;
    dmaData.numBytes     = static_cast<uint32>(GetGpuResultSizeInBytes(queryCount));
    dmaData.sync         = 1;
    dmaData.usePfp       = false;

    DmaDataInfo tsDmaData  = {};
    tsDmaData.dstSel       = dst_sel__pfp_dma_data__dst_addr_using_l2;
    tsDmaData.dstAddr      = timestampGpuAddr;
    tsDmaData.dstAddrSpace = das__pfp_dma_data__memory;
    tsDmaData.srcSel       = src_sel__pfp_dma_data__data;
    tsDmaData.srcData      = 0;
    tsDmaData.numBytes     = queryCount * static_cast<uint32>(m_timestampSizePerSlotInBytes);
    tsDmaData.sync         = 1;
    tsDmaData.usePfp       = false;

    pCmdSpace += cmdUtil.BuildDmaData<false>(dmaData, pCmdSpace);
    pCmdSpace += cmdUtil.BuildDmaData<false>(tsDmaData, pCmdSpace);

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

    // We only support 64-bit results.
    PAL_ASSERT(TestAnyFlagSet(flags, QueryResult64Bit));

    return TestAnyFlagSet(flags, QueryResultOnlyPrimNeeded) ? sizeof(StreamoutStatsData::primStorageNeeded) :
                                                              sizeof(StreamoutStatsData);
}

// =====================================================================================================================
// Check if the query data is valid.
bool StreamoutStatsQueryPool::IsQueryDataValid(
    volatile const uint64* pData
    ) const
{
    bool result = false;

    volatile const uint32* pData32 = reinterpret_cast<volatile const uint32*>(pData);

    if ((pData32[0] != StreamoutStatsResetMemValue32) || (pData32[1] != StreamoutStatsResetMemValue32))
    {
        // The write from the HW isn't atomic at the host/CPU level so we can
        // end up with half the data.
        if ((pData32[0] == StreamoutStatsResetMemValue32) || (pData32[1] == StreamoutStatsResetMemValue32))
        {
            // One of the halves appears unwritten. Use memory barrier here to
            // make sure all writes to this memory from other threads/devices visible to this thread.
            std::atomic_thread_fence(std::memory_order_acq_rel);
        }
        result = true;
    }

    return result;
}

// =====================================================================================================================
// Computes 'queryCount' slots of StreamoutStats and puts the result in the memory pointed to in pData. This is required
// by DX11 API.
bool StreamoutStatsQueryPool::ComputeResults(
    QueryResultFlags flags,
    QueryType        queryType,
    uint32           queryCount,
    size_t           stride,
    const void*      pGpuData,
    void*            pData)
{
    bool queryReady = true;
    for (uint32 i = 0; i < queryCount; i++)
    {
        const StreamoutStatsDataPair* pDataPair = static_cast<const StreamoutStatsDataPair*>(pGpuData);
        StreamoutStatsData* pQueryData = static_cast<StreamoutStatsData*>(pData);

        bool countersReady = false;
        do
        {
            // Check if 64bit data is valid first,
            // then AND all 4 counters together and check whether the 63rd bit is 1 or not
            countersReady = IsQueryDataValid(&pDataPair->end.primCountWritten)    &&
                            IsQueryDataValid(&pDataPair->begin.primCountWritten)  &&
                            IsQueryDataValid(&pDataPair->end.primStorageNeeded)   &&
                            IsQueryDataValid(&pDataPair->begin.primStorageNeeded) &&
                            (((pDataPair->end.primCountWritten     &
                               pDataPair->begin.primCountWritten   &
                               pDataPair->end.primStorageNeeded    &
                               pDataPair->begin.primStorageNeeded) & StreamoutStatsResultValidMask) != 0);
        } while ((countersReady == false) && TestAnyFlagSet(flags, QueryResultWait));

        if (countersReady)
        {
            const uint64 primCountWritten  = pDataPair->end.primCountWritten - pDataPair->begin.primCountWritten;
            const uint64 primStorageNeeded = pDataPair->end.primStorageNeeded - pDataPair->begin.primStorageNeeded;

            pQueryData->primStorageNeeded = primStorageNeeded;
            if (TestAnyFlagSet(flags, QueryResultOnlyPrimNeeded) == false)
            {
                pQueryData->primCountWritten  = primCountWritten;
            }
        }

        if (TestAnyFlagSet(flags, QueryResultAvailability))
        {
            uint32 offsetDistance = sizeof(StreamoutStatsData);
            if (TestAnyFlagSet(flags, QueryResultOnlyPrimNeeded))
            {
                offsetDistance = sizeof(StreamoutStatsData::primStorageNeeded);
            }

            uint64* pAvailabilityState = static_cast<uint64*>(VoidPtrInc(pData, offsetDistance));
            *pAvailabilityState        = static_cast<uint64>(countersReady);
        }

        // The entire query will only be ready if all of its counters were ready.
        queryReady = queryReady && countersReady;

        pGpuData = VoidPtrInc(pGpuData, GetGpuResultSizeInBytes(1));
        pData    = VoidPtrInc(pData, stride);
    }

    return queryReady;
}

} // Gfx9
} // Pal
