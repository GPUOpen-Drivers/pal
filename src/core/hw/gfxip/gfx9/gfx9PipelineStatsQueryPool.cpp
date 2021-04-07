/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9PipelineStatsQueryPool.h"
#include "palCmdBuffer.h"
#include "palSysUtil.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

constexpr uint32  QueryTimestampEnd = 0xABCD1234;

// The hardware uses 64-bit counters with this ordering internally.
struct Gfx9PipelineStatsData
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
    uint64 msInvocations;
    uint64 msPrimitives;
    uint64 tsInvocations;
};

// Defines the structure of a begin / end pair of data.
struct Gfx9PipelineStatsDataPair
{
    Gfx9PipelineStatsData begin; // pipeline stats query result when "begin" was called
    Gfx9PipelineStatsData end;   // pipeline stats query result when "end" was called
};

// Data needed to assemble one entry in a pipeline stats query pool result.
struct PipelineStatsLayoutData
{
    QueryPipelineStatsFlags statFlag;      // Which stat this entry represents.
    uint32                  counterOffset; // The offset in QWORDs to this stat inside of a Gfx9ipelineStatsData.
};

// All other clients use this layout.
constexpr PipelineStatsLayoutData PipelineStatsLayout[] =
{
    { QueryPipelineStatsIaVertices,    offsetof(Gfx9PipelineStatsData, iaVertices)    / sizeof(uint64) },
    { QueryPipelineStatsIaPrimitives,  offsetof(Gfx9PipelineStatsData, iaPrimitives)  / sizeof(uint64) },
    { QueryPipelineStatsVsInvocations, offsetof(Gfx9PipelineStatsData, vsInvocations) / sizeof(uint64) },
    { QueryPipelineStatsGsInvocations, offsetof(Gfx9PipelineStatsData, gsInvocations) / sizeof(uint64) },
    { QueryPipelineStatsGsPrimitives,  offsetof(Gfx9PipelineStatsData, gsPrimitives)  / sizeof(uint64) },
    { QueryPipelineStatsCInvocations,  offsetof(Gfx9PipelineStatsData, cInvocations)  / sizeof(uint64) },
    { QueryPipelineStatsCPrimitives,   offsetof(Gfx9PipelineStatsData, cPrimitives)   / sizeof(uint64) },
    { QueryPipelineStatsPsInvocations, offsetof(Gfx9PipelineStatsData, psInvocations) / sizeof(uint64) },
    { QueryPipelineStatsHsInvocations, offsetof(Gfx9PipelineStatsData, hsInvocations) / sizeof(uint64) },
    { QueryPipelineStatsDsInvocations, offsetof(Gfx9PipelineStatsData, dsInvocations) / sizeof(uint64) },
    { QueryPipelineStatsCsInvocations, offsetof(Gfx9PipelineStatsData, csInvocations) / sizeof(uint64) },
    { QueryPipelineStatsTsInvocations, offsetof(Gfx9PipelineStatsData, tsInvocations) / sizeof(uint64) },
    { QueryPipelineStatsMsInvocations, offsetof(Gfx9PipelineStatsData, msInvocations) / sizeof(uint64) },
    { QueryPipelineStatsMsPrimitives,  offsetof(Gfx9PipelineStatsData, msPrimitives)  / sizeof(uint64) }
};

constexpr size_t  PipelineStatsMaxNumCounters       = sizeof(Gfx9PipelineStatsData) / sizeof(uint64);
constexpr size_t  PipelineStatsNumSupportedCounters = ArrayLen(PipelineStatsLayout);
constexpr uint32  PipelineStatsResetMemValue32      = 0xFFFFFFFF;
constexpr gpusize PipelineStatsQueryMemoryAlignment = 8;

// =====================================================================================================================
PipelineStatsQueryPool::PipelineStatsQueryPool(
    const Device&              device,
    const QueryPoolCreateInfo& createInfo)
    :
    QueryPool(*(device.Parent()),
              createInfo,
              PipelineStatsQueryMemoryAlignment,
              sizeof(Gfx9PipelineStatsDataPair),
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
    gpuAddr += offsetof(Gfx9PipelineStatsDataPair, begin);

    if ((result == Result::Success) && pCmdBuffer->IsQueryAllowed(QueryPoolType::PipelineStats))
    {
        pCmdBuffer->AddQuery(QueryPoolType::PipelineStats, flags);

        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        const CmdUtil&   cmdUtil        = m_device.CmdUtil();
        const EngineType engineType     = pCmdBuffer->GetEngineType();
        const gpusize    beginQueryAddr = gpuAddr;

        if (engineType == EngineTypeCompute)
        {
            // Query event for compute engine only writes csInvocation, must write dummy zero's to other slots.
            constexpr uint32 DwordsToWrite = offsetof(Gfx9PipelineStatsData, csInvocations) / sizeof(uint32);
            const uint32 pData[DwordsToWrite] = {};

            gpuAddr = beginQueryAddr;

            WriteDataInfo writeData = {};
            writeData.engineType = engineType;
            writeData.dstAddr    = gpuAddr;
            writeData.dstSel     = dst_sel__mec_write_data__memory;

            pCmdSpace += CmdUtil::BuildWriteData(writeData, DwordsToWrite, pData, pCmdSpace);

            gpuAddr += offsetof(Gfx9PipelineStatsData, csInvocations);
        }

        pCmdSpace += cmdUtil.BuildSampleEventWrite(SAMPLE_PIPELINESTAT,
                                                   event_index__me_event_write__sample_pipelinestat,
                                                   engineType,
                                                   gpuAddr,
                                                   pCmdSpace);

        gpuAddr   = beginQueryAddr + offsetof(Gfx9PipelineStatsData, msInvocations);
        pCmdSpace = CopyMeshPipeStatsToQuerySlots(pCmdBuffer, gpuAddr, pCmdSpace);

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

    gpusize gpuAddr       = 0;
    Result  result        = GetQueryGpuAddress(slot, &gpuAddr);
    gpuAddr              += offsetof(Gfx9PipelineStatsDataPair, end);
    gpusize timeStampAddr = 0;

    if (result == Result::Success)
    {
        result = GetTimestampGpuAddress(slot, &timeStampAddr);
    }

    if ((result == Result::Success) && pCmdBuffer->IsQueryAllowed(QueryPoolType::PipelineStats))
    {
        pCmdBuffer->RemoveQuery(QueryPoolType::PipelineStats);

        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        const CmdUtil&   cmdUtil      = m_device.CmdUtil();
        const EngineType engineType   = pCmdBuffer->GetEngineType();
        const gpusize    endQueryAddr = gpuAddr;

        if (engineType == EngineTypeCompute)
        {
            // Query event for compute engine only writes csInvocation, must write dummy zero's to other slots.
            constexpr uint32 DwordsToWrite = offsetof(Gfx9PipelineStatsData, csInvocations) / sizeof(uint32);
            const uint32 pData[DwordsToWrite] = {};

            gpuAddr = endQueryAddr;

            WriteDataInfo writeData = {};
            writeData.engineType = engineType;
            writeData.dstAddr    = gpuAddr;
            writeData.dstSel     = dst_sel__mec_write_data__memory;

            pCmdSpace += CmdUtil::BuildWriteData(writeData, DwordsToWrite, pData, pCmdSpace);

            gpuAddr += offsetof(Gfx9PipelineStatsData, csInvocations);
        }

        pCmdSpace += cmdUtil.BuildSampleEventWrite(SAMPLE_PIPELINESTAT,
                                                   event_index__me_event_write__sample_pipelinestat,
                                                   engineType,
                                                   gpuAddr,
                                                   pCmdSpace);

        gpuAddr   = endQueryAddr + offsetof(Gfx9PipelineStatsData, msInvocations);
        pCmdSpace = CopyMeshPipeStatsToQuerySlots(pCmdBuffer, gpuAddr, pCmdSpace);

        ReleaseMemInfo releaseInfo = {};
        releaseInfo.engineType     = engineType;
        releaseInfo.vgtEvent       = BOTTOM_OF_PIPE_TS;
        releaseInfo.tcCacheOp      = TcCacheOp::Nop;
        releaseInfo.dstAddr        = timeStampAddr;
        releaseInfo.dataSel        = data_sel__me_release_mem__send_32_bit_low;
        releaseInfo.data           = QueryTimestampEnd;

        pCmdSpace += m_device.CmdUtil().BuildReleaseMem(releaseInfo, pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }
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
    // CS_PARTIAL_FLUSH on the universal and compute queues.
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
        //
        // TODO: Investigate if we can optimize this, we might just need a VS/PS/CS_PARTIAL_FLUSH on universal queue.
        pCmdSpace += cmdUtil.BuildWaitOnReleaseMemEventTs(pCmdBuffer->GetEngineType(),
                                                          BOTTOM_OF_PIPE_TS,
                                                          TcCacheOp::Nop,
                                                          pCmdBuffer->TimestampGpuVirtAddr(),
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
    DmaDataInfo dmaData   = {};
    dmaData.dstSel        = dst_sel__pfp_dma_data__dst_addr_using_l2;
    dmaData.dstAddr       = gpuAddr;
    dmaData.srcSel        = src_sel__pfp_dma_data__data;
    dmaData.srcData       = PipelineStatsResetMemValue32;
    dmaData.numBytes      = static_cast<uint32>(GetGpuResultSizeInBytes(queryCount));
    dmaData.sync          = 1;
    dmaData.usePfp        = false;

    DmaDataInfo tsDmaData = {};
    tsDmaData.dstSel      = dst_sel__pfp_dma_data__dst_addr_using_l2;
    tsDmaData.dstAddr     = timestampGpuAddr;
    tsDmaData.srcSel      = src_sel__pfp_dma_data__data;
    tsDmaData.srcData     = 0;
    tsDmaData.numBytes    = queryCount * static_cast<uint32>(m_timestampSizePerSlotInBytes);
    tsDmaData.sync        = 1;
    tsDmaData.usePfp      = false;

    pCmdSpace += CmdUtil::BuildDmaData(dmaData, pCmdSpace);
    pCmdSpace += CmdUtil::BuildDmaData(tsDmaData, pCmdSpace);

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
// Helper function to check if the query data is valid.
static bool IsQueryDataValid(
    volatile const uint64* pData)
{
    bool result = false;

    volatile const uint32* pData32 = reinterpret_cast<volatile const uint32*>(pData);

    if ((pData32[0] != PipelineStatsResetMemValue32) || (pData32[1] != PipelineStatsResetMemValue32))
    {
        // The write from the HW isn't atomic at the host/CPU level so we can
        // end up with half the data.
        if ((pData32[0] == PipelineStatsResetMemValue32) || (pData32[1] == PipelineStatsResetMemValue32))
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
    ResultUint results[PipelineStatsNumSupportedCounters] = {};
    uint32     numStatsEnabled = 0;
    bool       queryReady      = true;

    for (uint32 layoutIdx = 0; layoutIdx < PipelineStatsNumSupportedCounters; ++layoutIdx)
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
                countersReady = IsQueryDataValid(&pBeginCounters[counterOffset]) &&
                                IsQueryDataValid(&pEndCounters[counterOffset])   &&
                                ((pBeginCounters[counterOffset] != PipelineStatsResetMemValue64) &&
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
        const auto*const pGpuPair = reinterpret_cast<const Gfx9PipelineStatsDataPair*>(pGpuData);
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

// =====================================================================================================================
// Helper function that copies over shader-emulated mesh pipeline stats query to query slots. Both Begin() and End()
// should copy the scratch buffer data to pipeline stats query slots, regardless of whether the pipeline is Ms/Ts.
// Non-Ms/Ts pipeline should have same begin and end value for Ms/Ts-related query.
uint32* PipelineStatsQueryPool::CopyMeshPipeStatsToQuerySlots(
    GfxCmdBuffer* pCmdBuffer,
    gpusize       gpuAddr,    // gpuAddr is assumed to be pointing at msInvocations
    uint32*       pCmdSpace
    ) const
{
    const EngineType engineType = pCmdBuffer->GetEngineType();

    constexpr uint32 SizeOfMeshTaskQuerySlots = sizeof(PipelineStatsResetMemValue64) * PipelineStatsNumMeshCounters;
    constexpr uint32 SizeInDwords             = SizeOfMeshTaskQuerySlots / sizeof(uint32);

    if ((engineType == EngineTypeUniversal) && (pCmdBuffer->GetMeshPipeStatsGpuAddr() != 0))
    {
        const auto& cmdUtil = m_device.CmdUtil();

        // Waits in the CP ME for all previously issued VS waves to complete. The atomics are coming out of the shaders,
        // so we have to ensure all of the shaders have finished by the time we attempt to sample from this buffer.
        pCmdSpace += cmdUtil.BuildNonSampleEventWrite(VS_PARTIAL_FLUSH, engineType, pCmdSpace);

        const gpusize meshPipeStatsStartAddr = pCmdBuffer->GetMeshPipeStatsGpuAddr();

        static_assert((offsetof(Gfx9PipelineStatsData, msInvocations) + sizeof(uint64)) ==
                      offsetof(Gfx9PipelineStatsData, msPrimitives),
                      "Make sure the three QWORDs are next to each other so it's safe to do 3-QWORD copy.");
        static_assert((offsetof(Gfx9PipelineStatsData, msPrimitives) + sizeof(uint64)) ==
                      offsetof(Gfx9PipelineStatsData, tsInvocations),
                      "Make sure the three QWORDs are next to each other so it's safe to do 3-QWORD copy.");

        // Issue a DmaData packet to zero out the memory associated with all the mesh/task-slots we're going to reset.
        // Both the source (scratch buffer that SC writes to) and destination (internal query slots) follow CP-defined
        // ordering for the three mesh/task counters. For performance just do a 3-QWORD copy.
        DmaDataInfo copyInfo = {};
        copyInfo.srcAddr      = meshPipeStatsStartAddr;
        copyInfo.srcAddrSpace = sas__pfp_dma_data__memory;
        copyInfo.srcSel       = src_sel__pfp_dma_data__src_addr_using_l2;
        copyInfo.dstAddr      = gpuAddr;
        copyInfo.dstAddrSpace = das__pfp_dma_data__memory;
        copyInfo.dstSel       = dst_sel__pfp_dma_data__dst_addr_using_l2;
        copyInfo.numBytes     = SizeOfMeshTaskQuerySlots;
        copyInfo.usePfp       = false;
        copyInfo.sync         = true;

        pCmdSpace += CmdUtil::BuildDmaData(copyInfo, pCmdSpace);
    }
    else
    {
        // Must write dummy zero's to mesh/task slots if either,
        // 1. Query event for compute engine.
        // 2. When the first PipelineStatsQueryPool::Begin() is called, it's possible no valid Mesh-shader pipeline
        //    is bound yet, in this case scratch buffer is not allocated, so just set the Ms/Ts-related PipeStats
        //    query slots to zero.
        // 3. When no mesh/task enabled pipeline bound, zero out those slots for begin and end query.
        WriteDataInfo writeData = {};
        writeData.engineType    = engineType;
        writeData.dstAddr       = gpuAddr;
        // The whole query slot memory was previously reset by CPDMA packet performed on ME, this write needs to be
        // performed on ME too to avoid issuing a PfpSyncMe.
        writeData.engineSel     = engine_sel__me_write_data__micro_engine;
        writeData.dstSel        = dst_sel__me_write_data__memory;

        const uint32 pResetData[SizeInDwords] = {};

        pCmdSpace += CmdUtil::BuildWriteData(writeData, SizeInDwords, pResetData, pCmdSpace);
    }

    return pCmdSpace;
}

} // Gfx9
} // Pal
