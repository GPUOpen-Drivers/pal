/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6PipelineStatsQueryPool.h"
#include "palCmdBuffer.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

static constexpr uint32 QueryTimestampEnd = 0xABCD1234;

// The hardware uses 64-bit counters with this ordering internally.
struct Gfx6PipelineStatsData
{
    uint64 psInvocations;
    uint64 cPrimitives;
    uint64 cInvocations;
    uint64 vsInvocations;
    uint64 gsInvocations;
    uint64 gsPrimitives;
    uint64 iaPrimitives;
    uint64 iaVertices;
    uint64 hsInvocations;
    uint64 dsInvocations;
    uint64 csInvocations;
    uint64 unused[3];     // 3 QWORDs-placeholder as fixed-size structure padding for easier shader access.
};

// Defines the structure of a begin / end pair of data.
struct Gfx6PipelineStatsDataPair
{
    Gfx6PipelineStatsData begin; // pipeline stats query result when "begin" was called
    Gfx6PipelineStatsData end;   // pipeline stats query result when "end" was called
};

// Data needed to assemble one entry in a pipeline stats query pool result.
struct PipelineStatsLayoutData
{
    QueryPipelineStatsFlags statFlag;      // Which stat this entry represents.
    uint32                  counterOffset; // The offset in QWORDs to this stat inside of a Gfx6PipelineStatsData.
};

static constexpr uint32  PipelineStatsMaxNumCounters       = sizeof(Gfx6PipelineStatsData) / sizeof(uint64);
static constexpr uint32  PipelineStatsResetMemValue32      = 0xFFFFFFFF;
static constexpr uint64  PipelineStatsResetMemValue64      = 0xFFFFFFFFFFFFFFFF;
static constexpr gpusize PipelineStatsQueryMemoryAlignment = 8;

// All other clients use this layout.
static constexpr PipelineStatsLayoutData PipelineStatsLayout[PipelineStatsMaxNumCounters] =
{
    { QueryPipelineStatsIaVertices,    offsetof(Gfx6PipelineStatsData, iaVertices)    / sizeof(uint64) },
    { QueryPipelineStatsIaPrimitives,  offsetof(Gfx6PipelineStatsData, iaPrimitives)  / sizeof(uint64) },
    { QueryPipelineStatsVsInvocations, offsetof(Gfx6PipelineStatsData, vsInvocations) / sizeof(uint64) },
    { QueryPipelineStatsGsInvocations, offsetof(Gfx6PipelineStatsData, gsInvocations) / sizeof(uint64) },
    { QueryPipelineStatsGsPrimitives,  offsetof(Gfx6PipelineStatsData, gsPrimitives)  / sizeof(uint64) },
    { QueryPipelineStatsCInvocations,  offsetof(Gfx6PipelineStatsData, cInvocations)  / sizeof(uint64) },
    { QueryPipelineStatsCPrimitives,   offsetof(Gfx6PipelineStatsData, cPrimitives)   / sizeof(uint64) },
    { QueryPipelineStatsPsInvocations, offsetof(Gfx6PipelineStatsData, psInvocations) / sizeof(uint64) },
    { QueryPipelineStatsHsInvocations, offsetof(Gfx6PipelineStatsData, hsInvocations) / sizeof(uint64) },
    { QueryPipelineStatsDsInvocations, offsetof(Gfx6PipelineStatsData, dsInvocations) / sizeof(uint64) },
    { QueryPipelineStatsCsInvocations, offsetof(Gfx6PipelineStatsData, csInvocations) / sizeof(uint64) }
};

// =====================================================================================================================
PipelineStatsQueryPool::PipelineStatsQueryPool(
    const Device&              device,
    const QueryPoolCreateInfo& createInfo)
    :
    QueryPool(*(device.Parent()),
              createInfo,
              Device::CpDmaCompatAlignment(device, PipelineStatsQueryMemoryAlignment),
              sizeof(Gfx6PipelineStatsDataPair),
              sizeof(uint32)),
    m_device(device),
    m_numEnabledStats(0)
{
    PAL_ASSERT(m_createInfo.enabledStats != 0);

    // Compute the number of pipeline stats that are enabled by counting enable bits.
    constexpr uint32 LastMask = 1 << (PipelineStatsMaxNumCounters - 1);
    for (uint32 enableMask = 1; enableMask <= LastMask; enableMask <<= 1)
    {
        if (TestAnyFlagSet(m_createInfo.enabledStats, enableMask))
        {
            m_numEnabledStats++;
        }
    }
}

// =====================================================================================================================
// Adds the PM4 commands needed to begin this query to the supplied stream.
void PipelineStatsQueryPool::Begin(
    GfxCmdBuffer*     pCmdBuffer,
    Pal::CmdStream*   pCmdStream,
    QueryType         queryType,
    uint32            slot,
    QueryControlFlags flags
    ) const
{
    PAL_ASSERT(queryType == QueryType::PipelineStats);

    gpusize gpuAddr = 0;
    Result  result  = GetQueryGpuAddress(slot, &gpuAddr);

    if ((result == Result::Success) && pCmdBuffer->IsQueryAllowed(QueryPoolType::PipelineStats))
    {
        pCmdBuffer->AddQuery(QueryPoolType::PipelineStats, flags);

        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        gpuAddr += offsetof(Gfx6PipelineStatsDataPair, begin);

        if (pCmdBuffer->GetEngineType() == EngineTypeCompute)
        {
            // Query event for compute engine only writes csInvocation, must write dummy zero's to other slots.
            constexpr uint32 DwordsToWrite = offsetof(Gfx6PipelineStatsData, csInvocations) / sizeof(uint32);
            const uint32 pData[DwordsToWrite] = {};

            WriteDataInfo writeData = {};
            writeData.dstAddr = gpuAddr;
            writeData.dstSel  = WRITE_DATA_DST_SEL_MEMORY_ASYNC;

            pCmdSpace += m_device.CmdUtil().BuildWriteData(writeData, DwordsToWrite, pData, pCmdSpace);

            gpuAddr += offsetof(Gfx6PipelineStatsData, csInvocations);
        }

        // There are other events that "should/could" be used for Gfx7 and Gfx8 ASICs, but since we are supporting Gfx6
        // as well we'll use the old-reliable-standby.
        pCmdSpace += m_device.CmdUtil().BuildEventWriteQuery(SAMPLE_PIPELINESTAT,
                                                             gpuAddr,
                                                             pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Adds the PM4 commands needed to end this query to the supplied stream.
void PipelineStatsQueryPool::End(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pCmdStream,
    QueryType       queryType,
    uint32          slot
    ) const
{
    PAL_ASSERT(queryType == QueryType::PipelineStats);

    gpusize gpuAddr = 0;
    Result  result  = GetQueryGpuAddress(slot, &gpuAddr);

    gpusize timeStampAddr = 0;
    if (result == Result::Success)
    {
        result = GetTimestampGpuAddress(slot, &timeStampAddr);
    }

    if ((result == Result::Success) && pCmdBuffer->IsQueryAllowed(QueryPoolType::PipelineStats))
    {
        pCmdBuffer->RemoveQuery(QueryPoolType::PipelineStats);

        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        gpuAddr += offsetof(Gfx6PipelineStatsDataPair, end);

        const bool isComputeEngine = (pCmdBuffer->GetEngineType() == EngineTypeCompute);

        if (isComputeEngine)
        {
            // Query event for compute engine only writes csInvocation, must write dummy zero's to other slots.
            constexpr uint32 DwordsToWrite = offsetof(Gfx6PipelineStatsData, csInvocations) / sizeof(uint32);
            const uint32 pData[DwordsToWrite] = {};

            WriteDataInfo writeData = {};
            writeData.dstAddr = gpuAddr;
            writeData.dstSel  = WRITE_DATA_DST_SEL_MEMORY_ASYNC;

            pCmdSpace += m_device.CmdUtil().BuildWriteData(writeData, DwordsToWrite, pData, pCmdSpace);

            gpuAddr += offsetof(Gfx6PipelineStatsData, csInvocations);
        }

        // There are other events that "should/could" be used for Gfx7 and Gfx8 ASICs, but since we are supporting Gfx6
        // as well we'll use the old-reliable-standby.
        pCmdSpace += m_device.CmdUtil().BuildEventWriteQuery(SAMPLE_PIPELINESTAT,
                                                             gpuAddr,
                                                             pCmdSpace);

        // CmdUtil will properly route to EventWriteEop/ReleaseMem as appropriate.
        pCmdSpace += m_device.CmdUtil().BuildGenericEopEvent(BOTTOM_OF_PIPE_TS,
                                                             timeStampAddr,
                                                             EVENTWRITEEOP_DATA_SEL_SEND_DATA32,
                                                             QueryTimestampEnd,
                                                             isComputeEngine,
                                                             false,
                                                             pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Reset this query with CPU.
Result PipelineStatsQueryPool::Reset(
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
                         &PipelineStatsResetMemValue32);
    }

    return result;
}

// =====================================================================================================================
// Adds the PM4 commands needed to stall the ME until the results of the query range are in memory.
void PipelineStatsQueryPool::WaitForSlots(
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
// Adds the PM4 commands needed to reset this query to the supplied stream on a command buffer that does not support
// PM4 commands, or when an optimized path is unavailable.
void PipelineStatsQueryPool::NormalReset(
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

    pCmdBuffer->CmdFillMemory(*m_gpuMemory.Memory(), offset, dataSize, PipelineStatsResetMemValue32);

    // Reset the memory for querypool timestamps.
    pCmdBuffer->CmdFillMemory(*m_gpuMemory.Memory(),
                              GetTimestampOffset(startQuery),
                              m_timestampSizePerSlotInBytes * queryCount,
                              0);
}

// =====================================================================================================================
// Adds the PM4 commands needed to reset this query to the supplied stream on a command buffer built for PM4 commands.
// NOTE: It is safe to call this with a command buffer that does not support pipeline stats.
void PipelineStatsQueryPool::OptimizedReset(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pCmdStream,
    uint32          startQuery,
    uint32          queryCount
    ) const
{
    const auto& cmdUtil   = m_device.CmdUtil();
    uint32*     pCmdSpace = pCmdStream->ReserveCommands();

    if (pCmdBuffer->IsQueryAllowed(QueryPoolType::PipelineStats))
    {
        // Before we initialize out the GPU's destination memory, make sure the ASIC has finished any previous writing
        // of pipeline stat data. Command buffers that do not support stats queries do not need to issue this wait
        // because the caller must use semaphores to make sure all queries are complete.
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

    // Issue a CPDMA packet to zero out the memory associated with all the slots
    // we're going to reset.
    DmaDataInfo dmaData = {};
    dmaData.dstSel       = CPDMA_DST_SEL_DST_ADDR;
    dmaData.dstAddr      = gpuAddr;
    dmaData.dstAddrSpace = CPDMA_ADDR_SPACE_MEM;
    dmaData.srcSel       = CPDMA_SRC_SEL_DATA;
    dmaData.srcData      = PipelineStatsResetMemValue32;
    dmaData.numBytes     = static_cast<uint32>(GetGpuResultSizeInBytes(queryCount));
    dmaData.sync         = 1;
    dmaData.usePfp       = false;

    DmaDataInfo tsDmaData = {};
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
// Computes the size each result needs for one slot.
size_t PipelineStatsQueryPool::GetResultSizeForOneSlot(
    QueryResultFlags flags
    ) const
{
    const size_t resultIntegerSize = TestAnyFlagSet(flags, QueryResult64Bit) ? sizeof(uint64) : sizeof(uint32);
    const size_t numResultIntegers = TestAnyFlagSet(flags, QueryResultAvailability) + m_numEnabledStats;

    return numResultIntegers * resultIntegerSize;
}

// =====================================================================================================================
// Helper function for ComputeResults. It computes the result data according to the given flags, storing all data in
// integers of type ResultUint. Returns true if all counters were ready. Note that the counter pointers are volatile
// because the GPU could write them at any time (and if QueryResultWait is set we expect it to do so).
template <typename ResultUint>
static bool ComputeResultsForOneSlot(
    QueryResultFlags       resultFlags,
    uint32                 enableStatsFlags,
    volatile const uint64* pBeginCounters,
    volatile const uint64* pEndCounters,
    ResultUint*            pOutputBuffer)
{
    // Unless QueryResultPartial is set, we can't touch the destination buffer if some results aren't ready. We will
    // store our results in here until we know whether or not it's safe to write to the output buffer.
    ResultUint results[PipelineStatsMaxNumCounters] = {};
    uint32     numStatsEnabled = 0;
    bool       queryReady      = true;

    for (uint32 layoutIdx = 0; layoutIdx < PipelineStatsMaxNumCounters; ++layoutIdx)
    {
        // Filter out stats that are not enabled for this pool.
        if (TestAnyFlagSet(enableStatsFlags, PipelineStatsLayout[layoutIdx].statFlag))
        {
            const uint32 counterOffset = PipelineStatsLayout[layoutIdx].counterOffset;
            bool         countersReady = false;

            do
            {
                // If the initial value is still in one of the counters it implies that the query hasn't finished yet.
                // We will loop here for as long as necessary if the caller has requested it.
                countersReady = ((pBeginCounters[counterOffset] != PipelineStatsResetMemValue64) &&
                                 (pEndCounters[counterOffset]   != PipelineStatsResetMemValue64));
            }
            while ((countersReady == false) && TestAnyFlagSet(resultFlags, QueryResultWait));

            if (countersReady)
            {
                results[numStatsEnabled] = static_cast<ResultUint>(pEndCounters[counterOffset] -
                                                                   pBeginCounters[counterOffset]);
            }

            // The entire query will only be ready if all of its counters were ready.
            queryReady = queryReady && countersReady;

            numStatsEnabled++;
        }
    }

    // Store the results in the output buffer if it's legal for us to do so.
    if (queryReady || TestAnyFlagSet(resultFlags, QueryResultPartial))
    {
        // Accumulate the present data.
        if (TestAnyFlagSet(resultFlags, QueryResultAccumulate))
        {
            for (uint32 idx = 0; idx < numStatsEnabled; ++idx)
            {
                results[idx] += pOutputBuffer[idx];
            }
        }

        memcpy(pOutputBuffer, results, numStatsEnabled * sizeof(ResultUint));
    }

    // The caller also wants us to output whether or not the final query results were available. If we're
    // accumulating data we must AND our data the present data so the caller knows if all queries were available.
    if (TestAnyFlagSet(resultFlags, QueryResultAvailability))
    {
        if (TestAnyFlagSet(resultFlags, QueryResultAccumulate))
        {
            queryReady = queryReady && (pOutputBuffer[numStatsEnabled] != 0);
        }

        pOutputBuffer[numStatsEnabled] = queryReady;
    }

    return queryReady;
}

// =====================================================================================================================
// Gets the pipeline statistics data pointed to by pGpuData. This function wraps a template function to reduce code
// duplication due to selecting between 32-bit and 64-bit results. Returns true if all counters were ready.
bool PipelineStatsQueryPool::ComputeResults(
    QueryResultFlags flags,
    QueryType        queryType,
    uint32           queryCount,
    size_t           stride,
    const void*      pGpuData,
    void*            pData)
{
    PAL_ASSERT(queryType == QueryType::PipelineStats);

    bool allQueriesReady = true;
    for (uint32 queryIdx = 0; queryIdx < queryCount; ++queryIdx)
    {
        const auto*const pGpuPair = reinterpret_cast<const Gfx6PipelineStatsDataPair*>(pGpuData);
        const uint64*    pBegin   = reinterpret_cast<const uint64*>(&pGpuPair->begin);
        const uint64*    pEnd     = reinterpret_cast<const uint64*>(&pGpuPair->end);

        const bool queryReady = ((TestAnyFlagSet(flags, QueryResult64Bit))
            ? ComputeResultsForOneSlot(flags, m_createInfo.enabledStats, pBegin, pEnd, static_cast<uint64*>(pData))
            : ComputeResultsForOneSlot(flags, m_createInfo.enabledStats, pBegin, pEnd, static_cast<uint32*>(pData)));

        allQueriesReady = allQueriesReady && queryReady;
        pGpuData        = VoidPtrInc(pGpuData, GetGpuResultSizeInBytes(1));
        pData           = VoidPtrInc(pData,    stride);
    }

    return allQueriesReady;
}

} // Gfx6
} // Pal
