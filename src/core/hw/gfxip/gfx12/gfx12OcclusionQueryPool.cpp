/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx12/gfx12CmdUtil.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12OcclusionQueryPool.h"
#include "core/hw/gfxip/gfx12/gfx12UniversalCmdBuffer.h"
#include "palSysUtil.h"
#include "palIntervalTreeImpl.h"

#include <atomic>
#include <stddef.h>

using namespace Util;

namespace Pal
{
namespace Gfx12
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
    m_device(device)
{
}

// =====================================================================================================================
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
        pCmdBuffer->AddQuery(QueryPoolType::Occlusion, flags);

        static_cast<UniversalCmdBuffer*>(pCmdBuffer)->WriteBeginEndOcclusionQueryCmds(
            gpuAddr + offsetof(OcclusionQueryResultPair, begin));
    }
}

// =====================================================================================================================
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
        pCmdBuffer->RemoveQuery(QueryPoolType::Occlusion);

        static_cast<UniversalCmdBuffer*>(pCmdBuffer)->WriteBeginEndOcclusionQueryCmds(
            gpuAddr + offsetof(OcclusionQueryResultPair, end));

        // Now that the occlusion query has ended, track the relevant memory range so that we can wait for all writes to
        // complete before reseting this range in OptimizedReset().
        auto* pActiveRanges = static_cast<UniversalCmdBuffer*>(pCmdBuffer)->ActiveOcclusionQueryWriteRanges();

        const Interval<gpusize, bool> interval = { gpuAddr, gpuAddr + GetGpuResultSizeInBytes(1) - 1 };

        PAL_ASSERT(pActiveRanges->Overlap(&interval) == false);
        pActiveRanges->InsertOrExtend(&interval);
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
        result = CpuReset(startQuery,
                          queryCount,
                          pMappedCpuAddr,
                          m_gpuResultSizePerSlotInBytes,
                          m_device.OcclusionSlotResetValue());
    }

    return result;
}

// =====================================================================================================================
void OcclusionQueryPool::GpuReset(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pCmdStream,
    uint32          startQuery,
    uint32          queryCount
    ) const
{
    gpusize gpuAddr      = 0;
    Result  result       = GetQueryGpuAddress(startQuery, &gpuAddr);
    gpusize resetSrcAddr = m_device.OcclusionResetMem().GpuVirtAddr();

    PAL_ASSERT(result == Result::Success);

    uint32* pCmdSpaceBase = pCmdStream->ReserveCommands();
    uint32* pCmdSpace     = pCmdSpaceBase;

    if (pCmdBuffer->IsQueryAllowed(QueryPoolType::Occlusion))
    {
        // Before we zero out the GPU's destination memory, make sure the DB has finished any ZPASS events that might
        // update this memory. Otherwise, we could zero it out and then the DB would write the z-pass data into it.
        //
        // Command buffers that do not support occlusion queries do not need to issue this wait because the caller must
        // use semaphores to make sure all queries are complete.
        //
        // Insert the wait only if:
        // 1) We know all work done in previous command buffers has not completed.
        // 2) There are outstanding End() calls in this command buffer that we know will affect the range that is being
        //    reset.
        auto* pActiveRanges = static_cast<UniversalCmdBuffer*>(pCmdBuffer)->ActiveOcclusionQueryWriteRanges();

        const Interval<gpusize, bool> interval = { gpuAddr, gpuAddr + GetGpuResultSizeInBytes(queryCount) - 1 };

        if ( pCmdBuffer->GetCmdBufState().flags.prevCmdBufActive || pActiveRanges->Overlap(&interval))
        {
            constexpr WriteWaitEopInfo WaitEopInfo = { .hwAcqPoint = AcquirePointMe };

            pCmdSpace = pCmdBuffer->WriteWaitEop(WaitEopInfo, pCmdSpace);

            // The global wait guaranteed all work has completed, including any outstanding End() calls.
            pActiveRanges->Clear();
        }
    }

    // Each RB has a begin-end pair of 64-bit z-pass data.
    const uint32 qwordsPerQuery = 2 * m_device.Parent()->ChipProperties().gfx9.numTotalRbs;
    const uint32 bytesPerQuery  = qwordsPerQuery * sizeof(uint64);

    DmaDataInfo dmaDataInfo = {};
    dmaDataInfo.dstSel      = dst_sel__pfp_dma_data__dst_addr_using_l2;
    dmaDataInfo.srcSel      = src_sel__pfp_dma_data__src_addr_using_l2;
    dmaDataInfo.dstAddr     = gpuAddr;
    dmaDataInfo.srcAddr     = resetSrcAddr;
    dmaDataInfo.sync        = true;
    dmaDataInfo.usePfp      = false;
    dmaDataInfo.predicate   = Pm4Predicate(pCmdBuffer->GetPacketPredicate());

    // We need to know exactly how much space we have left.
    uint32 remainingDwords = pCmdStream->ReserveLimit() - static_cast<uint32>(pCmdSpace - pCmdSpaceBase);

    // Valid bits of reset data have been set for inactive RB's, so that HW doesn't hang.
    // Do the reset only for the number of slots worth of source data, to avoid segmentation faults.
    while (queryCount > 0)
    {
        if (remainingDwords < CmdUtil::DmaDataSizeDwords)
        {
            pCmdStream->CommitCommands(pCmdSpace);

            // Get a fresh reserve buffer for the remaining query results.
            pCmdSpace       = pCmdStream->ReserveCommands();
            remainingDwords = pCmdStream->ReserveLimit();
        }

        const uint32 slotCount = Min(queryCount, Pal::Device::OcclusionQueryDmaBufferSlots);

        // Only now do we know how many bytes we need to DMA.
        dmaDataInfo.numBytes = static_cast<uint32>(GetGpuResultSizeInBytes(slotCount));

        const size_t numDwords = CmdUtil::BuildDmaData<false>(dmaDataInfo, pCmdSpace);
        PAL_ASSERT(numDwords == CmdUtil::DmaDataSizeDwords);

        remainingDwords -= CmdUtil::DmaDataSizeDwords;
        pCmdSpace       += CmdUtil::DmaDataSizeDwords;

        // Update the destination address and query count for the next iteration.
        dmaDataInfo.dstAddr += dmaDataInfo.numBytes;
        queryCount          -= slotCount;
    }

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
size_t OcclusionQueryPool::GetResultSizeForOneSlot(
    Pal::QueryResultFlags flags
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
            std::atomic_thread_fence(std::memory_order_acq_rel);
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
    Pal::QueryResultFlags                    flags,
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
    Pal::QueryResultFlags flags,
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

} // Gfx12
} // Pal
