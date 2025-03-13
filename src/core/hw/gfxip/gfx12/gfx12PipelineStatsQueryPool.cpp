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
#include "core/hw/gfxip/queryPool.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx12/gfx12CmdUtil.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12PipelineStatsQueryPool.h"
#include "core/hw/gfxip/gfx12/gfx12UniversalCmdBuffer.h"
#include "palSysUtil.h"

#include <atomic>

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// The hardware uses 64-bit counters with this ordering internally.
struct Gfx12PipelineStatsData
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
    // We will need this when gang-submit support is added in Gfx12. This will be a second copy of csInvocations
    // captured by a ganged ACE queue. When computing results, PAL must add the sum to the "normal" csInvocations
    // counter.
    uint64 csInvocationsAce;

    // This tracks the csInvocations returned from the WGS scheduler since WGS HW doesn't support this
    uint64 csInvocationsWgs;
};

// Defines the structure of a begin / end pair of data.
struct Gfx12PipelineStatsDataPair
{
    Gfx12PipelineStatsData begin; // pipeline stats query result when "begin" was called
    Gfx12PipelineStatsData end;   // pipeline stats query result when "end" was called
};

// Data needed to assemble one entry in a pipeline stats query pool result.
struct PipelineStatsLayoutData
{
    QueryPipelineStatsFlags statFlag;      // Which stat this entry represents.
    uint32                  counterOffset; // The offset in QWORDs to this stat inside of a Gfx12PipelineStatsData.
};

constexpr PipelineStatsLayoutData PipelineStatsLayout[] =
{
    { QueryPipelineStatsIaVertices,    offsetof(Gfx12PipelineStatsData, iaVertices)    / sizeof(uint64) },
    { QueryPipelineStatsIaPrimitives,  offsetof(Gfx12PipelineStatsData, iaPrimitives)  / sizeof(uint64) },
    { QueryPipelineStatsVsInvocations, offsetof(Gfx12PipelineStatsData, vsInvocations) / sizeof(uint64) },
    { QueryPipelineStatsGsInvocations, offsetof(Gfx12PipelineStatsData, gsInvocations) / sizeof(uint64) },
    { QueryPipelineStatsGsPrimitives,  offsetof(Gfx12PipelineStatsData, gsPrimitives)  / sizeof(uint64) },
    { QueryPipelineStatsCInvocations,  offsetof(Gfx12PipelineStatsData, cInvocations)  / sizeof(uint64) },
    { QueryPipelineStatsCPrimitives,   offsetof(Gfx12PipelineStatsData, cPrimitives)   / sizeof(uint64) },
    { QueryPipelineStatsPsInvocations, offsetof(Gfx12PipelineStatsData, psInvocations) / sizeof(uint64) },
    { QueryPipelineStatsHsInvocations, offsetof(Gfx12PipelineStatsData, hsInvocations) / sizeof(uint64) },
    { QueryPipelineStatsDsInvocations, offsetof(Gfx12PipelineStatsData, dsInvocations) / sizeof(uint64) },
    { QueryPipelineStatsCsInvocations, offsetof(Gfx12PipelineStatsData, csInvocations) / sizeof(uint64) },
    { QueryPipelineStatsTsInvocations, offsetof(Gfx12PipelineStatsData, tsInvocations) / sizeof(uint64) },
    { QueryPipelineStatsMsInvocations, offsetof(Gfx12PipelineStatsData, msInvocations) / sizeof(uint64) },
    { QueryPipelineStatsMsPrimitives,  offsetof(Gfx12PipelineStatsData, msPrimitives)  / sizeof(uint64) }
};

constexpr size_t  PipelineStatsMaxNumCounters       = sizeof(Gfx12PipelineStatsData) / sizeof(uint64);
constexpr size_t  PipelineStatsNumSupportedCounters = ArrayLen(PipelineStatsLayout);
constexpr uint32  PipelineStatsResetMemValue32      = UINT32_MAX;
constexpr gpusize PipelineStatsQueryMemoryAlignment = 8;
constexpr uint32  PipelineStatsQueryTimestampEnd    = 0xABCD1234;

// =====================================================================================================================
PipelineStatsQueryPool::PipelineStatsQueryPool(
    const Device&              device,
    const QueryPoolCreateInfo& createInfo)
    :
    QueryPool(*(device.Parent()),
              createInfo,
              PipelineStatsQueryMemoryAlignment,
              sizeof(Gfx12PipelineStatsDataPair),
              sizeof(uint32)),
    m_device(device),
    m_numEnabledStats(0)
{
    PAL_ASSERT(m_createInfo.enabledStats != 0);

    // Compute the number of pipeline stats that are enabled by counting enable bits.
    constexpr uint32 EnabledStatsMask = (1 << PipelineStatsMaxNumCounters) - 1;
    m_numEnabledStats = CountSetBits(m_createInfo.enabledStats & EnabledStatsMask);
}
// =====================================================================================================================
// Helper function to write 0 to csInvocationsWgs.
static uint32* WriteZeroCsInvocationsWgs(
    gpusize gpuVirtAddr,
    uint32* pCmdSpace)
{
    constexpr uint32 DwordsCsInvocationsWgs = sizeof(uint64) / sizeof(uint32);
    constexpr uint32 Zeros[DwordsCsInvocationsWgs]{ }; //for uint64 csInvocationsWgs

    WriteDataInfo writeData{ };
    writeData.engineType = EngineTypeCompute;
    writeData.dstAddr = gpuVirtAddr + offsetof(Gfx12PipelineStatsData, csInvocationsWgs);
    writeData.dstSel = dst_sel__mec_write_data__memory;

    pCmdSpace += CmdUtil::BuildWriteData(writeData, DwordsCsInvocationsWgs, Zeros, pCmdSpace);
    return pCmdSpace;
}

// =====================================================================================================================
// Helper function to handle CS Invocations counter.
uint32* PipelineStatsQueryPool::SampleWgsCsInvocationsCounter(
    gpusize queryAddr,
    uint32* pCmdSpace
    ) const
{
    {
        pCmdSpace = WriteZeroCsInvocationsWgs(queryAddr, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// The SAMPLE_PIPELINESTAT event on the Compute engine only writes csInvocation, so we must write dummy zero's to other
// slots on a compute command buffer.
// This should only be called on a compute command buffer!
uint32* PipelineStatsQueryPool::FixupQueryDataOnAsyncCompute(
    gpusize gpuVirtAddr,
    uint32* pCmdSpace
    ) const
{
    constexpr uint32 DwordsBeforeCsInvocations = offsetof(Gfx12PipelineStatsData, csInvocations) / sizeof(uint32);
    constexpr uint32 Zeros[DwordsBeforeCsInvocations] = {};

    WriteDataInfo writeData = {};
    writeData.engineType    = EngineTypeCompute;
    writeData.dstAddr       = gpuVirtAddr;
    writeData.dstSel        = dst_sel__mec_write_data__memory;

    pCmdSpace += CmdUtil::BuildWriteData(writeData, DwordsBeforeCsInvocations, Zeros, pCmdSpace);

    constexpr uint32 MsTsMask =
        (QueryPipelineStatsMsInvocations | QueryPipelineStatsMsPrimitives | QueryPipelineStatsTsInvocations);

    if (TestAnyFlagSet(m_createInfo.enabledStats, MsTsMask))
    {
        constexpr uint32 DwordsAfterCsInvocations =
            (sizeof(Gfx12PipelineStatsData) - offsetof(Gfx12PipelineStatsData, msInvocations)) / sizeof(uint32);

        writeData.dstAddr += offsetof(Gfx12PipelineStatsData, msInvocations);
        pCmdSpace += CmdUtil::BuildWriteData(writeData, DwordsAfterCsInvocations, Zeros, pCmdSpace);
    }
    else
    {
        writeData.dstAddr += offsetof(Gfx12PipelineStatsData, csInvocationsAce);
        pCmdSpace += CmdUtil::BuildWriteData(writeData, (2 * sizeof(uint64) / sizeof(uint32)), Zeros, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// If the ganged ACE was not initialized by the time the query ends, then no work using it must have ocurred within
// the query's duration.  Therefore, we need to zero out the TsInvocations counters and the ACE instance of the
// CsInvocations counters for both the begin and end sample of this query slot so that when we compute the results or
// resolve the query, the ACE counter correctly contrubites zero to the final CsInvocations count.
uint32* PipelineStatsQueryPool::FixupQueryForNoGangedAce(
    gpusize gpuVirtAddr,    // Address of the whole Gfx12PipelineStatsData struct for the end sample.
    uint32* pCmdSpace
    ) const
{
    constexpr uint32 Zeros[6] { }; // Need 6 uint32's to fill 3 uint64s.
    constexpr uint32 DwordCount = (sizeof(Zeros) / sizeof(uint32));

    static_assert(CheckSequential({
        offsetof(Gfx12PipelineStatsData, tsInvocations),
        offsetof(Gfx12PipelineStatsData, csInvocationsAce),
        offsetof(Gfx12PipelineStatsData, csInvocationsWgs)},
        sizeof(uint64)), "TsInvocations, CsInvocationsAce, and csInvocationsWgs counters are not adjacent in memory!");

    WriteDataInfo writeData { };
    writeData.engineType = EngineTypeUniversal;
    writeData.dstAddr    = (gpuVirtAddr + offsetof(Gfx12PipelineStatsData, tsInvocations));
    writeData.dstSel     = dst_sel__me_write_data__memory;
    // The whole query slot memory was previously reset by CPDMA packet performed on ME, this write needs to be
    // performed on ME too to avoid issuing a PfpSyncMe.
    writeData.engineSel  = engine_sel__me_write_data__micro_engine;

    // Zero out the end counters.
    pCmdSpace += CmdUtil::BuildWriteData(writeData, DwordCount, Zeros, pCmdSpace);

    static_assert(CheckSequential({
        offsetof(Gfx12PipelineStatsDataPair, begin),
        offsetof(Gfx12PipelineStatsDataPair, end)},
        sizeof(Gfx12PipelineStatsData)), "Begin and end samples are not adjacent in memory!");

    writeData.dstAddr -= sizeof(Gfx12PipelineStatsData);

    // Zero out the begin counters.
    pCmdSpace += CmdUtil::BuildWriteData(writeData, DwordCount, Zeros, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
void PipelineStatsQueryPool::Begin(
    GfxCmdBuffer*     pCmdBuffer,
    Pal::CmdStream*   pCmdStream,
    Pal::CmdStream*   pHybridCmdStream,
    QueryType         queryType,
    uint32            slot,
    QueryControlFlags flags
    ) const
{
    PAL_ASSERT(queryType == QueryType::PipelineStats);

    gpusize slotGpuAddr = 0;
    Result  result      = GetQueryGpuAddress(slot, &slotGpuAddr);

    if ((result == Result::Success) && pCmdBuffer->IsQueryAllowed(QueryPoolType::PipelineStats))
    {
        pCmdBuffer->AddQuery(QueryPoolType::PipelineStats, flags);

        const EngineType engineType     = pCmdBuffer->GetEngineType();
        const gpusize    beginQueryAddr = slotGpuAddr + offsetof(Gfx12PipelineStatsDataPair, begin);
        gpusize          gpuAddr        = beginQueryAddr;

        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        if (engineType == EngineTypeCompute)
        {
            pCmdSpace = FixupQueryDataOnAsyncCompute(beginQueryAddr, pCmdSpace);
            gpuAddr  += offsetof(Gfx12PipelineStatsData, csInvocations);
        }

        pCmdSpace += CmdUtil::BuildSampleEventWrite(SAMPLE_PIPELINESTAT,
                                                    event_index__me_event_write__sample_pipelinestat,
                                                    engineType,
                                                    samp_plst_cntr_mode__mec_event_write__legacy_mode,
                                                    gpuAddr,
                                                    pCmdSpace);

        if (pHybridCmdStream != nullptr)
        {
            uint32* pAceCmdSpace = pHybridCmdStream->ReserveCommands();
            pAceCmdSpace         = SampleQueryDataOnGangedAce(gpuAddr, pAceCmdSpace);

            pHybridCmdStream->CommitCommands(pAceCmdSpace);
        }
        else
        {
            if (engineType == EngineTypeCompute)
            {
                // We could be in a deferred query begin state so no valid ace command stream at the moment.
                // This block is for the compute command stream only.
                // Special handling for CS invocation counter
                pCmdSpace = SampleWgsCsInvocationsCounter(beginQueryAddr, pCmdSpace);
            }
        }

        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
void PipelineStatsQueryPool::End(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pCmdStream,
    Pal::CmdStream* pHybridCmdStream,
    QueryType       queryType,
    uint32          slot
    ) const
{
    PAL_ASSERT(queryType == QueryType::PipelineStats);

    gpusize slotGpuAddr   = 0;
    Result  result        = GetQueryGpuAddress(slot, &slotGpuAddr);
    gpusize timeStampAddr = 0;

    if (result == Result::Success)
    {
        result = GetTimestampGpuAddress(slot, &timeStampAddr);
    }

    if ((result == Result::Success) && pCmdBuffer->IsQueryAllowed(QueryPoolType::PipelineStats))
    {
        pCmdBuffer->RemoveQuery(QueryPoolType::PipelineStats);

        const EngineType engineType   = pCmdBuffer->GetEngineType();
        const gpusize    endQueryAddr = slotGpuAddr + offsetof(Gfx12PipelineStatsDataPair, end);
        gpusize          gpuAddr      = endQueryAddr;

        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        if (engineType == EngineTypeCompute)
        {
            pCmdSpace = FixupQueryDataOnAsyncCompute(endQueryAddr, pCmdSpace);
            gpuAddr  += offsetof(Gfx12PipelineStatsData, csInvocations);
        }

        pCmdSpace += CmdUtil::BuildSampleEventWrite(SAMPLE_PIPELINESTAT,
                                                    event_index__me_event_write__sample_pipelinestat,
                                                    engineType,
                                                    samp_plst_cntr_mode__mec_event_write__legacy_mode,
                                                    gpuAddr,
                                                    pCmdSpace);

        if (pHybridCmdStream != nullptr)
        {
            uint32* pAceCmdSpace = pHybridCmdStream->ReserveCommands();
            pAceCmdSpace         = SampleQueryDataOnGangedAce(gpuAddr, pAceCmdSpace);

            pHybridCmdStream->CommitCommands(pAceCmdSpace);
        }
        else
        {
            pCmdSpace = FixupQueryForNoGangedAce(gpuAddr, pCmdSpace);

            if (engineType == EngineTypeCompute)
            {
                // Special handling for CS invocation counter
                pCmdSpace = SampleWgsCsInvocationsCounter(endQueryAddr, pCmdSpace);
            }
        }

        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.dstAddr  = timeStampAddr;
        releaseInfo.dataSel  = data_sel__me_release_mem__send_32_bit_low;
        releaseInfo.data     = PipelineStatsQueryTimestampEnd;
        releaseInfo.vgtEvent = BOTTOM_OF_PIPE_TS;

        pCmdSpace += m_device.CmdUtil().BuildReleaseMemGeneric(releaseInfo, pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Samples query data on a ganged ACE queue, as part of either a Begin() or End() operation.
uint32* PipelineStatsQueryPool::SampleQueryDataOnGangedAce(
    gpusize gpuVirtAddr,
    uint32* pAceCmdSpace
    ) const
{
    // Setting the countermode to samp_plst_cntr_mode__mec_event_write__new_mode will
    // have the CP only write the tsInvocations.
    pAceCmdSpace += CmdUtil::BuildSampleEventWrite(SAMPLE_PIPELINESTAT,
                                                   event_index__me_event_write__sample_pipelinestat,
                                                   EngineTypeCompute,
                                                   samp_plst_cntr_mode__mec_event_write__new_mode,
                                                   (gpuVirtAddr + offsetof(Gfx12PipelineStatsData, tsInvocations)),
                                                   pAceCmdSpace);

    //Special handling for CS invocation counter
    pAceCmdSpace = SampleWgsCsInvocationsCounter(gpuVirtAddr, pAceCmdSpace);

    return pAceCmdSpace;
}

// =====================================================================================================================
// Handles properly beginning the query on a ganged ACE command stream when the query was begun before the ganged ACE
// stream was initialized.
uint32* PipelineStatsQueryPool::DeferredBeginOnGangedAce(
    GfxCmdBuffer*   pCmdBuffer,
    uint32*         pCmdSpace,
    uint32          slot
    ) const
{
    PAL_ASSERT((pCmdBuffer != nullptr) && (pCmdBuffer->GetEngineType() == EngineTypeUniversal));
    PAL_ASSERT(pCmdSpace != nullptr);

    gpusize gpuAddr = 0;
    Result  result  = GetQueryGpuAddress(slot, &gpuAddr);
    gpuAddr        += offsetof(Gfx12PipelineStatsDataPair, begin);

    if ((result == Result::Success) && pCmdBuffer->IsQueryAllowed(QueryPoolType::PipelineStats))
    {
        // Note: There is no need to register the query with the command buffer here; it was done already in Begin().

        pCmdSpace = SampleQueryDataOnGangedAce(gpuAddr, pCmdSpace);
    }
    return pCmdSpace;
}

// =====================================================================================================================
// Adds the PM4 commands needed to stall the ME until the results of the query range are in memory.
void PipelineStatsQueryPool::WaitForSlots(
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

    const uint32 waitsPerCommit = pCmdStream->ReserveLimit() / PM4_ME_WAIT_REG_MEM_SIZEDW__CORE;
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
                                                  PipelineStatsQueryTimestampEnd,
                                                  UINT32_MAX,
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
        result = CpuReset(startQuery, queryCount, pMappedCpuAddr, 4, &PipelineStatsResetMemValue32);
    }

    return result;
}

// =====================================================================================================================
bool PipelineStatsQueryPool::RequiresSamplingFromGangedAce() const
{
    return TestAnyFlagSet(m_createInfo.enabledStats, QueryPipelineStatsTsInvocations);
}

// =====================================================================================================================
void PipelineStatsQueryPool::GpuReset(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pCmdStream,
    uint32          startQuery,
    uint32          queryCount
    ) const
{
    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    if (pCmdBuffer->IsQueryAllowed(QueryPoolType::PipelineStats))
    {
        // Before we initialize out the GPU's destination memory, make sure the ASIC has finished any previous writing
        // of pipeline stat data. Command buffers that do not support stats queries do not need to issue this wait
        // because the caller must use semaphores to make sure all queries are complete.
        constexpr WriteWaitEopInfo WaitEopInfo = { .hwAcqPoint = AcquirePointMe };

        pCmdSpace = pCmdBuffer->WriteWaitEop(WaitEopInfo, pCmdSpace);
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
    DmaDataInfo dmaData = {};
    dmaData.dstSel      = dst_sel__pfp_dma_data__dst_addr_using_l2;
    dmaData.dstAddr     = gpuAddr;
    dmaData.srcSel      = src_sel__pfp_dma_data__data;
    dmaData.srcData     = PipelineStatsResetMemValue32;
    dmaData.numBytes    = static_cast<uint32>(GetGpuResultSizeInBytes(queryCount));
    dmaData.sync        = 1;
    dmaData.usePfp      = false;

    pCmdSpace += CmdUtil::BuildDmaData<false>(dmaData, pCmdSpace);

    DmaDataInfo tsDmaData = {};
    tsDmaData.dstSel      = dst_sel__pfp_dma_data__dst_addr_using_l2;
    tsDmaData.dstAddr     = timestampGpuAddr;
    tsDmaData.srcSel      = src_sel__pfp_dma_data__data;
    tsDmaData.srcData     = 0;
    tsDmaData.numBytes    = queryCount * static_cast<uint32>(m_timestampSizePerSlotInBytes);
    tsDmaData.sync        = 1;
    tsDmaData.usePfp      = false;

    pCmdSpace += CmdUtil::BuildDmaData<false>(tsDmaData, pCmdSpace);

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Computes the size each result needs for one slot.
size_t PipelineStatsQueryPool::GetResultSizeForOneSlot(
    Pal::QueryResultFlags flags
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
            std::atomic_thread_fence(std::memory_order_acq_rel);
        }
        result = true;
    }

    return result;
}

// =====================================================================================================================
// Helper function for ComputeResultsForOneSlot. It computes one counter value according to the given flags, storing the
// value into an integer of type ResultUint. Returns true if all counters were ready. Note that the counter pointers are
// volatile because the GPU could write them at any time (and if QueryResultWait is set we expect it to do so).
template <typename ResultUint>
static bool AccumulateResultForOneCounter(
    QueryResultFlags       resultFlags,
    uint32                 counterIndex,
    volatile const uint64* pBeginCounters,
    volatile const uint64* pEndCounters,
    ResultUint*            pAccumulatedValue) // Output counter to accumulate. Not modified if counter data is not ready.
{
    bool countersReady = false;

    do
    {
        // If the initial value is still in one of the counters it implies that the query hasn't finished yet.
        // We will loop here for as long as necessary if the caller has requested it.
        countersReady = IsQueryDataValid(&pBeginCounters[counterIndex]) &&
            IsQueryDataValid(&pEndCounters[counterIndex]) &&
            (pBeginCounters[counterIndex] != PipelineStatsResetMemValue64) &&
            (pEndCounters[counterIndex] != PipelineStatsResetMemValue64);
    } while ((countersReady == false) && TestAnyFlagSet(resultFlags, QueryResultWait));

    if (countersReady)
    {
        *pAccumulatedValue += static_cast<ResultUint>(pEndCounters[counterIndex] - pBeginCounters[counterIndex]);
    }

    return countersReady;
}

// =====================================================================================================================
// Helper function for ComputeResults. It computes the result data according to the given flags, storing all data in
// integers of type ResultUint. Returns true if all counters were ready. Note that the counter pointers are volatile
// because the GPU could write them at any time (and if QueryResultWait is set we expect it to do so).
template <typename ResultUint>
static bool ComputeResultsForOneSlot(
    Pal::QueryResultFlags  resultFlags,
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
            bool countersReady = AccumulateResultForOneCounter(
                resultFlags,
                PipelineStatsLayout[layoutIdx].counterOffset,
                pBeginCounters,
                pEndCounters,
                &results[numStatsEnabled]);

            if (PipelineStatsLayout[layoutIdx].statFlag == QueryPipelineStatsCsInvocations)
            {
                // Special handling for CsInvocations:
                // In cases where gang-submission of GFX+ACE is used, the counter is stored in a separate location on
                // the ganged ACE queue so that it doesn't cause a data race with the GFX queue's copy.  We need to sum
                // both counters together when computing the actual value.
                countersReady &= AccumulateResultForOneCounter(
                    resultFlags,
                    (offsetof(Gfx12PipelineStatsData, csInvocationsAce) / sizeof(uint64)),
                    pBeginCounters,
                    pEndCounters,
                    &results[numStatsEnabled]);
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
    Pal::QueryResultFlags flags,
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
        const auto*const pGpuPair = reinterpret_cast<const Gfx12PipelineStatsDataPair*>(pGpuData);
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

} // Gfx12
} // Pal
