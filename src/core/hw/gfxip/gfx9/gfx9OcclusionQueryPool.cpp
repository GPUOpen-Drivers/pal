/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9OcclusionQueryPool.h"
#include "core/hw/gfxip/gfx9/gfx9UniversalCmdBuffer.h"
#include "palCmdBuffer.h"
#include "palIntervalTreeImpl.h"
#include "palSysUtil.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
OcclusionQueryPool::OcclusionQueryPool(
    const Device&              device,
    const QueryPoolCreateInfo& createInfo)
    :
    QueryPool(*(device.Parent()),
              createInfo,
              OcclusionQueryMemoryAlignment,
              device.Parent()->ChipProperties().gfx9.numTotalRbs * sizeof(OcclusionQueryResultPair),
              0),
    m_device(device),
    m_canUseDmaFill(device.Parent()->ChipProperties().gfx9.numActiveRbs ==
                    device.Parent()->ChipProperties().gfx9.numTotalRbs)
{

}

// =====================================================================================================================
// Adds the PM4 commands needed to begin this query to the supplied stream.
void OcclusionQueryPool::Begin(
    GfxCmdBuffer*     pCmdBuffer,
    Pal::CmdStream*   pCmdStream,
    Pal::CmdStream*   pHybridCmdStream,
    QueryType         queryType,
    uint32            slot,
    QueryControlFlags flags
    ) const
{
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported() &&
               ((queryType == QueryType::Occlusion) || (queryType == QueryType::BinaryOcclusion)));

    gpusize gpuAddr = 0;
    Result  result  = GetQueryGpuAddress(slot, &gpuAddr);

    if ((result == Result::Success) && pCmdBuffer->IsQueryAllowed(QueryPoolType::Occlusion))
    {
        const CmdUtil& cmdUtil = m_device.CmdUtil();

        pCmdBuffer->AddQuery(QueryPoolType::Occlusion, flags);

        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace +=
            cmdUtil.BuildSampleEventWrite(PIXEL_PIPE_STAT_DUMP,
                                          event_index__me_event_write__pixel_pipe_stat_control_or_dump,
                                          pCmdBuffer->GetEngineType(),
                                          gpuAddr + offsetof(OcclusionQueryResultPair, begin),
                                          pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Adds the PM4 commands needed to end this query to the supplied stream.
void OcclusionQueryPool::End(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pCmdStream,
    Pal::CmdStream* pHybridCmdStream,
    QueryType       queryType,
    uint32          slot
    ) const
{
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported() &&
               ((queryType == QueryType::Occlusion) || (queryType == QueryType::BinaryOcclusion)));

    gpusize gpuAddr = 0;
    Result  result  = GetQueryGpuAddress(slot, &gpuAddr);

    if ((result == Result::Success) && pCmdBuffer->IsQueryAllowed(QueryPoolType::Occlusion))
    {
        const CmdUtil&    cmdUtil    = m_device.CmdUtil();
        const EngineType  engineType = pCmdBuffer->GetEngineType();

        pCmdBuffer->RemoveQuery(QueryPoolType::Occlusion);

        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace +=
            cmdUtil.BuildSampleEventWrite(PIXEL_PIPE_STAT_DUMP,
                                          event_index__me_event_write__pixel_pipe_stat_control_or_dump,
                                          engineType,
                                          gpuAddr + offsetof(OcclusionQueryResultPair, end),
                                          pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);

        // Now that the occlusion query has ended, track the relevant memory range so that we can wait for all writes to
        // complete before reseting this range in OptimizedReset().
        auto* pActiveRanges = static_cast<UniversalCmdBuffer*>(pCmdBuffer)->ActiveOcclusionQueryWriteRanges();

        const Interval<gpusize, bool> interval = { gpuAddr, gpuAddr + GetGpuResultSizeInBytes(1) - 1 };

        PAL_ASSERT(pActiveRanges->Overlap(&interval) == false);
        pActiveRanges->Insert(&interval);
    }
}

// =====================================================================================================================
// Adds the PM4 commands needed to stall the ME until the results of the query range are in memory.
void OcclusionQueryPool::WaitForSlots(
    Pal::CmdStream* pCmdStream,
    uint32          startQuery,
    uint32          queryCount
    ) const
{
    // This function should never be called for GFX9 occlusion queries, as waiting is implemented in the shader.
    PAL_NEVER_CALLED();
}

// =====================================================================================================================
// Adds the PM4 commands needed to reset this query to the supplied stream on a command buffer that does not support
// PM4 commands, or when an optimized path is unavailable.
void OcclusionQueryPool::NormalReset(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pCmdStream,
    uint32          startQuery,
    uint32          queryCount
    ) const
{
    gpusize     offset       = GetQueryOffset(startQuery);

    // This function must only be called by the DMA queue. It is missing a barrier call that is necessary to issue a
    // CS_PARTIAL_FLUSH on the universal and compute queues.
    PAL_ASSERT(pCmdBuffer->GetEngineType() == EngineTypeDma);
    PAL_ASSERT(m_gpuMemory.IsBound());

    if (m_canUseDmaFill)
    {
        // Some quick testing shows that this is just as fast as a DMA copy on Hawaii. Until a client actually uses this
        // path and gives us a reason to go and do a detailed performance run we will just assume this is the best path
        // in general.
        pCmdBuffer->CmdFillMemory(*reinterpret_cast<const IGpuMemory*>(m_gpuMemory.Memory()),
                                  offset,
                                  GetGpuResultSizeInBytes(queryCount),
                                  0);
    }
    else
    {
        const BoundGpuMemory& srcBuffer = m_device.OcclusionResetMem();
        const IGpuMemory&     srcMem    = static_cast<const IGpuMemory&>(*srcBuffer.Memory());

        MemoryCopyRegion region = {};
        region.srcOffset  = srcBuffer.Offset();
        region.dstOffset  = offset;

        // Issue a series of DMAs until we run out of query slots to reset. Note that numToReset will be updated before
        // the loop subtracts it.
        uint32 numToReset = 0;
        for (uint32 remaining = queryCount; remaining > 0; remaining -= numToReset)
        {
            numToReset      = Min(remaining, Pal::Device::OcclusionQueryDmaBufferSlots);
            region.copySize = GetGpuResultSizeInBytes(numToReset);

            pCmdBuffer->CmdCopyMemory(srcMem, *static_cast<const IGpuMemory*>(m_gpuMemory.Memory()), 1, &region);

            region.dstOffset += region.copySize;
        }
    }
}

// =====================================================================================================================
// Reset this query with CPU.
Result OcclusionQueryPool::Reset(
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
                         m_gpuResultSizePerSlotInBytes,
                         m_device.OcclusionSlotResetValue());
    }

    return result;
}

// =====================================================================================================================
// Adds the PM4 commands needed to reset this query to the supplied stream on a command buffer built for PM4 commands.
// NOTE: It is safe to call this with a command buffer that does not support occlusion queries.
void OcclusionQueryPool::OptimizedReset(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pCmdStream,
    uint32          startQuery,
    uint32          queryCount
    ) const
{
    const auto& cmdUtil      = m_device.CmdUtil();

    // We'll need a pointer to the beginning of the reserve buffer later on.
    uint32*const pCmdSpaceBase = pCmdStream->ReserveCommands();
    uint32*      pCmdSpace     = pCmdSpaceBase;

    gpusize gpuAddr          = 0;
    Result  result           = GetQueryGpuAddress(startQuery, &gpuAddr);

    PAL_ASSERT(result == Result::Success);

    if (pCmdBuffer->IsQueryAllowed(QueryPoolType::Occlusion))
    {
        // Before we zero out the GPU's destination memory, make sure the DB has finished any ZPASS events that might
        // update this memory. Otherwise, we could zero it out and then the DB would write the z-pass data into it.

        // Command buffers that do not support occlusion queries do not need to issue this wait because the caller must
        // use semaphores to make sure all queries are complete.

        // By calling WriteWaitOnReleaseMemEvent we assume this command buffer must support graphics operations.
        PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());

        // Insert the wait only if 1) we know all work done in previous command buffers has completed (because we
        // have no idea if any relevant Ends() occurred there, and 2) there are outstanding End() calls in this
        // command buffer that we know will affect the range that is being reset.
        auto* pActiveRanges = static_cast<UniversalCmdBuffer*>(pCmdBuffer)->ActiveOcclusionQueryWriteRanges();

        const Interval<gpusize, bool> interval = { gpuAddr, gpuAddr + GetGpuResultSizeInBytes(queryCount) - 1 };

        if (pCmdBuffer->GetGfxCmdBufState().flags.prevCmdBufActive || pActiveRanges->Overlap(&interval))
        {
            pCmdSpace += cmdUtil.BuildWaitOnReleaseMemEventTs(pCmdBuffer->GetEngineType(),
                                                              BOTTOM_OF_PIPE_TS,
                                                              TcCacheOp::Nop,
                                                              pCmdBuffer->TimestampGpuVirtAddr(),
                                                              pCmdSpace);

            // The previous EOP event and wait mean that anything prior to this point, including previous command
            // buffers on this queue, have completed.
            pCmdBuffer->SetPrevCmdBufInactive();

            // The global wait guaranteed all work has completed, including any outstanding End() calls.
            pActiveRanges->Clear();
        }
    }

    if (Pal::Device::OcclusionQueryDmaLowerBound <= GetGpuResultSizeInBytes(queryCount))
    {
        // Execute the reset using the DMA copy optimization. Set everything except DMA size.
        DmaDataInfo dmaData = {};
        dmaData.dstSel       = dst_sel__pfp_dma_data__dst_addr_using_l2;
        dmaData.dstAddr      = gpuAddr;
        dmaData.dstAddrSpace = das__pfp_dma_data__memory;
        dmaData.srcSel       = src_sel__pfp_dma_data__src_addr_using_l2;
        dmaData.srcAddr      = m_device.OcclusionResetMem().GpuVirtAddr();
        dmaData.srcAddrSpace = sas__pfp_dma_data__memory;
        dmaData.sync         = 1;
        dmaData.usePfp       = false;

        // We need to know exactly how much space we have left.
        uint32 remainingDwords = pCmdStream->ReserveLimit() - static_cast<uint32>(pCmdSpace - pCmdSpaceBase);

        // Unlike most other command building loops, in this case we don't know exactly how many DWORDs each call to
        // BuilDmaData will write. We need to write the packets one-by-one until we can't fit the worst case call.
        const uint32 maxPacketSize = CmdUtil::DmaDataSizeDwords;

        while (queryCount > 0)
        {
            while ((queryCount > 0) && (remainingDwords >= maxPacketSize))
            {
                const uint32 slotCount = Min(queryCount, Pal::Device::OcclusionQueryDmaBufferSlots);

                // Only now do we know how many bytes we need to DMA.
                dmaData.numBytes = static_cast<uint32>(GetGpuResultSizeInBytes(slotCount));

                const size_t numDwords = cmdUtil.BuildDmaData(dmaData, pCmdSpace);

                PAL_ASSERT(numDwords <= maxPacketSize);

                remainingDwords -= static_cast<uint32>(numDwords);
                pCmdSpace       += numDwords;

                // Update the destination address and query count for the next iteration.
                dmaData.dstAddr += dmaData.numBytes;
                queryCount      -= slotCount;
            }

            // Get a fresh reserve buffer if we're going to loop again.
            if (queryCount > 0)
            {
                pCmdStream->CommitCommands(pCmdSpace);

                pCmdSpace       = pCmdStream->ReserveCommands();
                remainingDwords = pCmdStream->ReserveLimit();
            }
        }
    }
    else
    {
        // Use WRITE_DATA to do the reset if any of these conditions are met:
        // 1. We've been forced to use it because we can't do a DMA fill.
        // 2. We're going to be executing the reset on an APU (DMA fills are slow on APUs).
        // 3. The destination is in system memory (DMA fills are slow to system).
        if ((m_canUseDmaFill == false)                                           ||
            (m_device.Parent()->ChipProperties().gpuType == GpuType::Integrated) ||
            (m_gpuMemory.Memory()->PreferredHeap() == GpuHeapGartCacheable)      ||
            (m_gpuMemory.Memory()->PreferredHeap() == GpuHeapGartUswc))
        {
            // We need to know exactly how much space we have left.
            uint32 remainingDwords = pCmdStream->ReserveLimit() - static_cast<uint32>(pCmdSpace - pCmdSpaceBase);

            // Get some information about the source data.
            const uint32* pSrcData     = m_device.OcclusionSlotResetValue();
            const uint32  periodBytes  = static_cast<uint32>(GetGpuResultSizeInBytes(1));
            const uint32  periodDwords = periodBytes / sizeof(uint32);

            WriteDataInfo writeData = {};
            writeData.engineType = pCmdBuffer->GetEngineType();
            writeData.dstAddr    = gpuAddr;
            writeData.engineSel  = engine_sel__me_write_data__micro_engine;
            writeData.dstSel     = dst_sel__me_write_data__memory;

            while (queryCount > 0)
            {
                // We'll need to know how many DWORDs we can write without exceeding the size of the reserve buffer.
                // If we're writing more DWORDs than will fit, we will adjust dstAddr and queryCount and loop again.
                const uint32 maxSlots  = (remainingDwords - CmdUtil::WriteDataSizeDwords * 2) / periodDwords;
                const uint32 slotCount = Min(queryCount, maxSlots);

                pCmdSpace += cmdUtil.BuildWriteDataPeriodic(writeData, periodDwords, slotCount, pSrcData, pCmdSpace);

                writeData.dstAddr += slotCount * periodBytes;
                queryCount        -= slotCount;

                // Get a fresh reserve buffer if we're going to loop again.
                if (queryCount > 0)
                {
                    pCmdStream->CommitCommands(pCmdSpace);

                    pCmdSpace       = pCmdStream->ReserveCommands();
                    remainingDwords = pCmdStream->ReserveLimit();
                }
            }
        }
        else
        {
            // DMA fill: issue a CPDMA packet to zero out the entire slot range.
            DmaDataInfo dmaData = {};
            dmaData.dstSel       = dst_sel__pfp_dma_data__dst_addr_using_l2;
            dmaData.dstAddr      = gpuAddr;
            dmaData.dstAddrSpace = das__pfp_dma_data__memory;
            dmaData.srcSel       = src_sel__pfp_dma_data__data;
            dmaData.srcData      = 0;
            dmaData.numBytes     = static_cast<uint32>(GetGpuResultSizeInBytes(queryCount));
            dmaData.sync         = 1;
            dmaData.usePfp       = false;

            pCmdSpace += cmdUtil.BuildDmaData(dmaData, pCmdSpace);
        }
    }

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Computes the size each result needs for one slot.
size_t OcclusionQueryPool::GetResultSizeForOneSlot(
    QueryResultFlags flags
    ) const
{
    const size_t resultIntegerSize = TestAnyFlagSet(flags, QueryResult64Bit) ? sizeof(uint64) : sizeof(uint32);
    const size_t numResultIntegers = TestAnyFlagSet(flags, QueryResultAvailability) + 1;

    return numResultIntegers * resultIntegerSize;
}

// =====================================================================================================================
// Helper function to check if the query data is valid. For disabled RBs, the check should always pass but just with a
// memory barrier inserted.
static bool IsQueryDataValid(
    volatile const uint64* pData)
{
    bool result = false;

    volatile const uint32* pData32 = reinterpret_cast<volatile const uint32*>(pData);

    if ((pData32[0] != 0) || (pData32[1] != 0))
    {
        // The write from the HW isn't atomic at the host/CPU level so we can
        // end up with half the data.
        if ((pData32[0] == 0) || (pData32[1] == 0))
        {
            // One of the halves appears unwritten. Use memory barrier here to
            // make sure all writes to this memory from other threads/devices visible to this thread.
            Util::MemoryBarrier();
        }
        result = true;
    }

    return result;
}

// =====================================================================================================================
// Helper function for ComputeResults. It computes the result data according to the given flags, storing all data in
// integers of type ResultUint. Returns true if all counters were ready. Note that the counters pointer is volatile
// because the GPU could write them at any time (and if QueryResultWait is set we expect it to do so).
template <typename ResultUint>
static bool ComputeResultsForOneSlot(
    QueryResultFlags                         flags,
    uint32                                   numTotalRbs,
    bool                                     isBinary,
    volatile const OcclusionQueryResultPair* pRbCounters,
    ResultUint*                              pOutputBuffer)
{
    ResultUint result     = 0;
    bool       queryReady = true;

    // Loop through all the RBs associated with this ASIC.
    for (uint32 idx = 0; idx < numTotalRbs; idx++)
    {
        bool countersReady = false;

        do
        {
            // The RBs will set the valid bits when they have written their data. We do not need to skip disabled RBs
            // because they are initialized to valid with zPassData equal to zero. We will loop here for as long as
            // necessary if the caller has requested it.
            countersReady = IsQueryDataValid(&pRbCounters[idx].begin.data) &&
                            IsQueryDataValid(&pRbCounters[idx].end.data)   &&
                            ((pRbCounters[idx].begin.bits.valid == 1) && (pRbCounters[idx].end.bits.valid == 1));
        }
        while ((countersReady == false) && TestAnyFlagSet(flags, QueryResultWait));

        if (countersReady)
        {
            result += static_cast<ResultUint>(pRbCounters[idx].end.bits.zPassData -
                                              pRbCounters[idx].begin.bits.zPassData);
        }

        // The entire query will only be ready if all of its counters were ready.
        queryReady = queryReady && countersReady;
    }

    // Store the result in the output buffer if it's legal for us to do so.
    if (queryReady || TestAnyFlagSet(flags, QueryResultPartial))
    {
        if (TestAnyFlagSet(flags, QueryResultAccumulate))
        {
            // Accumulate the present data; we do this first so that the if isBinary is set we still get a 0 or 1.
            result += pOutputBuffer[0];
        }

        pOutputBuffer[0] = isBinary ? (result != 0) : result;
    }

    // The caller also wants us to output whether or not the final query results were available. If we're
    // accumulating data we must AND our data the present data so the caller knows if all queries were available.
    if (TestAnyFlagSet(flags, QueryResultAvailability))
    {
        if (TestAnyFlagSet(flags, QueryResultAccumulate))
        {
            queryReady = queryReady && (pOutputBuffer[1] != 0);
        }

        pOutputBuffer[1] = queryReady;
    }

    return queryReady;
}

// =====================================================================================================================
// Adds up all the results from each RB (stored in pGpuData) and puts the accumulated result in the memory pointed to in
// pData. This function wraps a template function to reduce code duplication due to selecting between 32-bit and 64-bit
// results. Returns true if all counters were ready.
bool OcclusionQueryPool::ComputeResults(
    QueryResultFlags flags,
    QueryType        queryType,
    uint32           queryCount,
    size_t           stride,
    const void*      pGpuData,
    void*            pData)
{
    PAL_ASSERT((queryType == QueryType::Occlusion) || (queryType == QueryType::BinaryOcclusion));

    const uint32 numTotalRbs = m_device.Parent()->ChipProperties().gfx9.numTotalRbs;
    const bool   isBinary    = (queryType == QueryType::BinaryOcclusion);

    bool allQueriesReady = true;
    for (uint32 queryIdx = 0; queryIdx < queryCount; ++queryIdx)
    {
        const auto* pRbCounters = static_cast<const OcclusionQueryResultPair*>(pGpuData);

        const bool queryReady = ((TestAnyFlagSet(flags, QueryResult64Bit))
            ? ComputeResultsForOneSlot(flags, numTotalRbs, isBinary, pRbCounters, static_cast<uint64*>(pData))
            : ComputeResultsForOneSlot(flags, numTotalRbs, isBinary, pRbCounters, static_cast<uint32*>(pData)));

        allQueriesReady = allQueriesReady && queryReady;
        pGpuData        = VoidPtrInc(pGpuData, GetGpuResultSizeInBytes(1));
        pData           = VoidPtrInc(pData,    stride);
    }

    return allQueriesReady;
}

} // Gfx9
} // Pal
