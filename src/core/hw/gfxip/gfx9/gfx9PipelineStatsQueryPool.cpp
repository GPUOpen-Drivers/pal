/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9PipelineStatsQueryPool.h"
#include "core/hw/gfxip/pm4CmdBuffer.h"
#include "palCmdBuffer.h"
#include "palSysUtil.h"

#include <atomic>

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
    // This is a second copy of csInvocations captured by a ganged ACE queue.  When computing results, PAL must add
    // the sum to the "normal" csInvocations counter.
    uint64 csInvocationsAce;
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
    { QueryPipelineStatsMsPrimitives,  offsetof(Gfx9PipelineStatsData, msPrimitives)  / sizeof(uint64) },
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
    m_numEnabledStats(CountSetBits(m_createInfo.enabledStats & QueryPipelineStatsAll))
{
    PAL_ASSERT(m_createInfo.enabledStats != 0);
    PAL_ASSERT(m_numEnabledStats <= PipelineStatsNumSupportedCounters);
}

// =====================================================================================================================
// The SAMPLE_PIPELINESTAT event on the Compute engine only writes csInvocation, so we must write dummy zero's to other
// slots on a compute command buffer.
// This should only be called on a compute command buffer!
uint32* PipelineStatsQueryPool::FixupQueryDataOnAsyncCompute(
    gpusize gpuVirtAddr,    // Address of the whole Gfx9PipelineStatsData struct.
    uint32* pCmdSpace
    ) const
{
    constexpr uint32 DwordsBeforeCsInvocations = offsetof(Gfx9PipelineStatsData, csInvocations) / sizeof(uint32);
    constexpr uint32 Zeros[DwordsBeforeCsInvocations] { };

    WriteDataInfo writeData { };
    writeData.engineType = EngineTypeCompute;
    writeData.dstAddr    = gpuVirtAddr;
    writeData.dstSel     = dst_sel__mec_write_data__memory;

    pCmdSpace += CmdUtil::BuildWriteData(writeData, DwordsBeforeCsInvocations, Zeros, pCmdSpace);

    constexpr uint32 MsTsMask =
        (QueryPipelineStatsMsInvocations | QueryPipelineStatsMsPrimitives | QueryPipelineStatsTsInvocations);

    if (TestAnyFlagSet(m_createInfo.enabledStats, MsTsMask))
    {
        constexpr uint32 DwordsAfterCsInvocations =
            (sizeof(Gfx9PipelineStatsData) - offsetof(Gfx9PipelineStatsData, msInvocations)) / sizeof(uint32);

        writeData.dstAddr += offsetof(Gfx9PipelineStatsData, msInvocations);
        pCmdSpace += CmdUtil::BuildWriteData(writeData, DwordsAfterCsInvocations, Zeros, pCmdSpace);
    }
    else
    {
        writeData.dstAddr += offsetof(Gfx9PipelineStatsData, csInvocationsAce);
        pCmdSpace += CmdUtil::BuildWriteData(writeData, (sizeof(uint64) / sizeof(uint32)), Zeros, pCmdSpace);
    }

    return pCmdSpace;
}

#if PAL_BUILD_GFX11
// =====================================================================================================================
bool PipelineStatsQueryPool::RequiresSamplingFromGangedAce() const
{
#if   PAL_BUILD_GFX11
    // Otherwise, on GFX11 GPUs, this is only required if the query is supposed to include TsInvocations.
    return (IsGfx11(*m_device.Parent()) && TestAnyFlagSet(m_createInfo.enabledStats, QueryPipelineStatsTsInvocations));
#else
    return false;
#endif
}

// =====================================================================================================================
// Samples query data on a ganged ACE queue, as part of either a Begin() or End() operation.
uint32* PipelineStatsQueryPool::SampleQueryDataOnGangedAce(
    gpusize gpuVirtAddr,    // Address of the whole Gfx9PipelineStatsData struct.
    uint32* pAceCmdSpace
    ) const
{
    const CmdUtil& cmdUtil = m_device.CmdUtil();

#if PAL_BUILD_GFX11
    // On GFX11, Mesh Shader pipeline statistics are handled automatically when sampling the other graphics pipeline
    // statistics.  However, the Task shader is not included, because that work runs on the ganged ACE queue.
    if (IsGfx11(*(m_device.Parent())))
    {
        // Setting the countermode to samp_plst_cntr_mode__mec_event_write__new_mode__GFX11 will
        // have the CP only write the tsInvocations.
        pAceCmdSpace += cmdUtil.BuildSampleEventWrite(
            SAMPLE_PIPELINESTAT,
            event_index__me_event_write__sample_pipelinestat,
            EngineTypeCompute,
            samp_plst_cntr_mode__mec_event_write__new_mode__GFX11,
            (gpuVirtAddr + offsetof(Gfx9PipelineStatsData, tsInvocations)),
            pAceCmdSpace);
    }
#endif

    return pAceCmdSpace;
}

// =====================================================================================================================
// Handles properly beginning the query on a ganged ACE command stream when the query was begun before the ganged ACE
// stream was initialized.
void PipelineStatsQueryPool::DeferredBeginOnGangedAce(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pAceCmdStream,
    uint32          slot
    ) const
{
    PAL_ASSERT((pCmdBuffer != nullptr) && (pCmdBuffer->GetEngineType() == EngineTypeUniversal));
    PAL_ASSERT(pAceCmdStream != nullptr);

    gpusize gpuAddr = 0;
    Result  result  = GetQueryGpuAddress(slot, &gpuAddr);
    gpuAddr        += offsetof(Gfx9PipelineStatsDataPair, begin);

    if ((result == Result::Success) && pCmdBuffer->IsQueryAllowed(QueryPoolType::PipelineStats))
    {
        // Note: There is no need to register the query with the command buffer here; it was done already in Begin().

        uint32* pAceCmdSpace = pAceCmdStream->ReserveCommands();
        pAceCmdSpace = SampleQueryDataOnGangedAce(gpuAddr, pAceCmdSpace);
        pAceCmdStream->CommitCommands(pAceCmdSpace);
    }
}

// =====================================================================================================================
// If the ganged ACE was not initialized by the time the query ends, then no work using it must have ocurred within
// the query's duration.  Therefore, we need to zero out the TsInvocations counters and the ACE instance of the
// CsInvocations counters for both the begin and end sample of this query slot so that when we compute the results or
// resolve the query, the ACE counter correctly contrubites zero to the final CsInvocations count.
uint32* PipelineStatsQueryPool::FixupQueryForNoGangedAce(
    gpusize gpuVirtAddr,    // Address of the whole Gfx9PipelineStatsData struct for the end sample.
    uint32* pCmdSpace
    ) const
{
    constexpr uint32 Zeros[4] { }; // Need 4 uint32's to fill 2 uint64s.
    constexpr uint32 DwordCount = (sizeof(Zeros) / sizeof(uint32));

    static_assert(CheckSequential({
        offsetof(Gfx9PipelineStatsData, tsInvocations),
        offsetof(Gfx9PipelineStatsData, csInvocationsAce)},
        sizeof(uint64)), "TsInvocations and CsInvocationsAce counters are not adjacent in memory!");

    WriteDataInfo writeData { };
    writeData.engineType = EngineTypeUniversal;
    writeData.dstAddr    = (gpuVirtAddr + offsetof(Gfx9PipelineStatsData, tsInvocations));
    writeData.dstSel     = dst_sel__me_write_data__memory;
    // The whole query slot memory was previously reset by CPDMA packet performed on ME, this write needs to be
    // performed on ME too to avoid issuing a PfpSyncMe.
    writeData.engineSel  = engine_sel__me_write_data__micro_engine;

    // Zero out the end counters.
    pCmdSpace += CmdUtil::BuildWriteData(writeData, DwordCount, Zeros, pCmdSpace);

    static_assert(CheckSequential({
        offsetof(Gfx9PipelineStatsDataPair, begin),
        offsetof(Gfx9PipelineStatsDataPair, end)},
        sizeof(Gfx9PipelineStatsData)), "Begin and end samples are not adjacent in memory!");

    writeData.dstAddr -= sizeof(Gfx9PipelineStatsData);

    // Zero out the begin counters.
    pCmdSpace += CmdUtil::BuildWriteData(writeData, DwordCount, Zeros, pCmdSpace);

    return pCmdSpace;
}
#endif

// =====================================================================================================================
// Begins a single query
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

    gpusize gpuAddr = 0;
    Result  result  = GetQueryGpuAddress(slot, &gpuAddr);
    gpuAddr        += offsetof(Gfx9PipelineStatsDataPair, begin);

    if ((result == Result::Success) && pCmdBuffer->IsQueryAllowed(QueryPoolType::PipelineStats))
    {
        pCmdBuffer->AddQuery(QueryPoolType::PipelineStats, flags);

        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        const CmdUtil&   cmdUtil        = m_device.CmdUtil();
        const EngineType engineType     = pCmdBuffer->GetEngineType();
        const gpusize    beginQueryAddr = gpuAddr;

        if (engineType == EngineTypeCompute)
        {
            pCmdSpace = FixupQueryDataOnAsyncCompute(beginQueryAddr, pCmdSpace);
            gpuAddr += offsetof(Gfx9PipelineStatsData, csInvocations);
        }

        pCmdSpace += cmdUtil.BuildSampleEventWrite(SAMPLE_PIPELINESTAT,
                                                   event_index__me_event_write__sample_pipelinestat,
                                                   engineType,
#if PAL_BUILD_GFX11
                                                   samp_plst_cntr_mode__mec_event_write__legacy_mode__GFX11,
#endif
                                                   gpuAddr,
                                                   pCmdSpace);

        if (engineType == EngineTypeUniversal)
        {
            if (IsGfx10(*m_device.Parent()))
            {
                // GFX10 requires software emulation for Mesh and Task Shader pipeline stats.
                pCmdSpace = CopyMeshPipeStatsToQuerySlots(pCmdBuffer, gpuAddr, pCmdSpace);
            }

#if PAL_BUILD_GFX11
            if (pHybridCmdStream != nullptr)
            {
                uint32* pAceCmdSpace = pHybridCmdStream->ReserveCommands();
                pAceCmdSpace = SampleQueryDataOnGangedAce(gpuAddr, pAceCmdSpace);
                pHybridCmdStream->CommitCommands(pAceCmdSpace);
            }
#endif
        }

        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Ends a single query
void PipelineStatsQueryPool::End(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pCmdStream,
    Pal::CmdStream* pHybridCmdStream,
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
            pCmdSpace = FixupQueryDataOnAsyncCompute(endQueryAddr, pCmdSpace);
            gpuAddr += offsetof(Gfx9PipelineStatsData, csInvocations);
        }

        pCmdSpace += cmdUtil.BuildSampleEventWrite(SAMPLE_PIPELINESTAT,
                                                   event_index__me_event_write__sample_pipelinestat,
                                                   engineType,
#if PAL_BUILD_GFX11
                                                   samp_plst_cntr_mode__mec_event_write__legacy_mode__GFX11,
#endif
                                                   gpuAddr,
                                                   pCmdSpace);

        if (engineType == EngineTypeUniversal)
        {
            if (IsGfx10(*m_device.Parent()))
            {
                // GFX10 requires software emulation for Mesh and Task Shader pipeline stats.
                pCmdSpace = CopyMeshPipeStatsToQuerySlots(pCmdBuffer, gpuAddr, pCmdSpace);
            }

#if PAL_BUILD_GFX11
            if (pHybridCmdStream != nullptr)
            {
                uint32* pAceCmdSpace = pHybridCmdStream->ReserveCommands();
                pAceCmdSpace = SampleQueryDataOnGangedAce(gpuAddr, pAceCmdSpace);
                pHybridCmdStream->CommitCommands(pAceCmdSpace);
            }
            else
            {
                pCmdSpace = FixupQueryForNoGangedAce(gpuAddr, pCmdSpace);
            }
#endif
        }

        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.engineType = engineType;
        releaseInfo.dstAddr    = timeStampAddr;
        releaseInfo.dataSel    = data_sel__me_release_mem__send_32_bit_low;
        releaseInfo.data       = QueryTimestampEnd;

        pCmdSpace += cmdUtil.BuildReleaseMemGeneric(releaseInfo, pCmdSpace);
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
// Reset query using DMA, when NormalReset() can't be used or the command buffer does not support PM4.
void PipelineStatsQueryPool::DmaEngineReset(
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
// Reset query via PM4 commands on a PM4-supported command buffer.
// NOTE: It is safe to call this with a command buffer that does not support pipeline stats.
void PipelineStatsQueryPool::NormalReset(
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
        Pm4CmdBuffer* pPm4CmdBuf = static_cast<Pm4CmdBuffer*>(pCmdBuffer);

        // Before we initialize out the GPU's destination memory, make sure the ASIC has finished any previous writing
        // of pipeline stat data. Command buffers that do not support stats queries do not need to issue this wait
        // because the caller must use semaphores to make sure all queries are complete.
        //
        // TODO: Investigate if we can optimize this, we might just need a VS/PS/CS_PARTIAL_FLUSH on universal queue.
        pCmdSpace = pPm4CmdBuf->WriteWaitEop(HwPipePostPrefetch, SyncGlxNone, SyncRbNone, pCmdSpace);
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

    pCmdSpace += CmdUtil::BuildDmaData<false>(dmaData, pCmdSpace);
    pCmdSpace += CmdUtil::BuildDmaData<false>(tsDmaData, pCmdSpace);

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
                        IsQueryDataValid(&pEndCounters[counterIndex])   &&
                        ((pBeginCounters[counterIndex] != PipelineStatsResetMemValue64) &&
                         (pEndCounters[counterIndex]   != PipelineStatsResetMemValue64));
    }
    while ((countersReady == false) && TestAnyFlagSet(resultFlags, QueryResultWait));

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
            bool counterReady = AccumulateResultForOneCounter(
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
                counterReady &= AccumulateResultForOneCounter(
                    resultFlags,
                    (offsetof(Gfx9PipelineStatsData, csInvocationsAce) / sizeof(uint64)),
                    pBeginCounters,
                    pEndCounters,
                    &results[numStatsEnabled]);
            }

            // The entire query will only be ready if all of its counters were ready.
            queryReady &= counterReady;

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
    gpusize       gpuVirtAddr,  // Address of the whole Gfx9PipelineStatsData struct.
    uint32*       pCmdSpace
    ) const
{
    const EngineType engineType = pCmdBuffer->GetEngineType();

    constexpr uint32 SizeOfMeshTaskQuerySlots = sizeof(PipelineStatsResetMemValue64) * PipelineStatsNumMeshCounters;
    constexpr uint32 SizeInDwords             = SizeOfMeshTaskQuerySlots / sizeof(uint32);

    const gpusize msInvocGpuAddr = (gpuVirtAddr + offsetof(Gfx9PipelineStatsData, msInvocations));

    if ((engineType == EngineTypeUniversal) && (pCmdBuffer->GetMeshPipeStatsGpuAddr() != 0))
    {
        const auto& cmdUtil = m_device.CmdUtil();

        // Waits in the CP ME for all previously issued VS waves to complete. The atomics are coming out of the shaders,
        // so we have to ensure all of the shaders have finished by the time we attempt to sample from this buffer.
        pCmdSpace += cmdUtil.BuildNonSampleEventWrite(VS_PARTIAL_FLUSH, engineType, pCmdSpace);

        const gpusize meshPipeStatsStartAddr = pCmdBuffer->GetMeshPipeStatsGpuAddr();

        static_assert(CheckSequential({
            offsetof(Gfx9PipelineStatsData, msInvocations),
            offsetof(Gfx9PipelineStatsData, msPrimitives),
            offsetof(Gfx9PipelineStatsData, tsInvocations),
        }, sizeof(uint64)), "Make sure the three QWORDs are next to each other so it's safe to do 3-QWORD copy.");

        // Issue a DmaData packet to zero out the memory associated with all the mesh/task-slots we're going to reset.
        // Both the source (scratch buffer that SC writes to) and destination (internal query slots) follow CP-defined
        // ordering for the three mesh/task counters. For performance just do a 3-QWORD copy.
        DmaDataInfo copyInfo = {};
        copyInfo.srcAddr      = meshPipeStatsStartAddr;
        copyInfo.srcAddrSpace = sas__pfp_dma_data__memory;
        copyInfo.srcSel       = src_sel__pfp_dma_data__src_addr_using_l2;
        copyInfo.dstAddr      = msInvocGpuAddr;
        copyInfo.dstAddrSpace = das__pfp_dma_data__memory;
        copyInfo.dstSel       = dst_sel__pfp_dma_data__dst_addr_using_l2;
        copyInfo.numBytes     = SizeOfMeshTaskQuerySlots;
        copyInfo.usePfp       = false;
        copyInfo.sync         = true;

        pCmdSpace += CmdUtil::BuildDmaData<false>(copyInfo, pCmdSpace);
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
        writeData.dstAddr       = msInvocGpuAddr;
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
