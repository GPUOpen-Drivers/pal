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
#include "core/hw/gfxip/gfx12/gfx12StreamoutStatsQueryPool.h"
#include "core/hw/gfxip/gfx12/gfx12UniversalCmdBuffer.h"

#include <atomic>

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// The streamIndex used to call BuildStreamoutStatsQuery() won't work properly if this assert fails.
static_assert(Util::CheckSequential({
    static_cast<uint32>(QueryType::StreamoutStats),
    static_cast<uint32>(QueryType::StreamoutStats1),
    static_cast<uint32>(QueryType::StreamoutStats2),
    static_cast<uint32>(QueryType::StreamoutStats3),
}), "Query types are not sequential as expected!");

struct Gfx12StreamoutStatsData
{
    uint64 primStorageNeeded; // Number of primitives that would have been written to the SO resource
    uint64 primCountWritten;  // Number of primitives written to the SO resource
};

struct Gfx12StreamoutStatsDataPair
{
    Gfx12StreamoutStatsData begin; // streamout stats query result when "begin" was called
    Gfx12StreamoutStatsData end;   // streamout stats query result when "end" was called
};

constexpr gpusize StreamoutStatsQueryMemoryAlignment = 32;
constexpr uint32  StreamoutStatsResetMemValue32      = 0;
constexpr uint64  StreamoutStatsResultValidMask      = 0x8000000000000000ull;
constexpr uint32  QueryTimestampEnd                  = 0xABCD1234;

// =====================================================================================================================
StreamoutStatsQueryPool::StreamoutStatsQueryPool(
    const Device&              device,
    const QueryPoolCreateInfo& createInfo)
    :
    QueryPool(*(device.Parent()),
              createInfo,
              StreamoutStatsQueryMemoryAlignment,
              sizeof(Gfx12StreamoutStatsDataPair),
              sizeof(uint32)),
    m_device(device)
{
}

// =====================================================================================================================
void StreamoutStatsQueryPool::Begin(
    GfxCmdBuffer*     pCmdBuffer,
    Pal::CmdStream*   pCmdStream,
    Pal::CmdStream*   pHybridCmdStream,
    QueryType         queryType,
    uint32            slot,
    QueryControlFlags flags
    ) const
{
    PAL_ASSERT((queryType == QueryType::StreamoutStats)  ||
               (queryType == QueryType::StreamoutStats1) ||
               (queryType == QueryType::StreamoutStats2) ||
               (queryType == QueryType::StreamoutStats3));

    static_assert((static_cast<uint32>(QueryType::StreamoutStats1) - static_cast<uint32>(QueryType::StreamoutStats)) == 1);
    static_assert((static_cast<uint32>(QueryType::StreamoutStats2) - static_cast<uint32>(QueryType::StreamoutStats)) == 2);
    static_assert((static_cast<uint32>(QueryType::StreamoutStats3) - static_cast<uint32>(QueryType::StreamoutStats)) == 3);

    gpusize gpuAddr = 0;
    Result result   = GetQueryGpuAddress(slot, &gpuAddr);

    if ((result == Result::Success) && pCmdBuffer->IsQueryAllowed(QueryPoolType::StreamoutStats))
    {
        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        pCmdSpace = reinterpret_cast<UniversalCmdBuffer*>(pCmdBuffer)->VerifyStreamoutCtrlBuf(pCmdSpace);
        gpusize streamoutCtrlBuf = reinterpret_cast<UniversalCmdBuffer*>(pCmdBuffer)->GetStreamoutCtrlBufAddr();

        pCmdSpace += CmdUtil::BuildStreamoutStatsQuery(streamoutCtrlBuf,
                                                       static_cast<uint32>(queryType) -
                                                       static_cast<uint32>(QueryType::StreamoutStats),
                                                       gpuAddr + offsetof(Gfx12StreamoutStatsDataPair, begin),
                                                       pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
void StreamoutStatsQueryPool::End(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pCmdStream,
    Pal::CmdStream* pHybridCmdStream,
    QueryType       queryType,
    uint32          slot
    ) const
{
    PAL_ASSERT((queryType == QueryType::StreamoutStats)  ||
               (queryType == QueryType::StreamoutStats1) ||
               (queryType == QueryType::StreamoutStats2) ||
               (queryType == QueryType::StreamoutStats3));

    gpusize gpuAddr       = 0;
    Result result         = GetQueryGpuAddress(slot, &gpuAddr);
    gpusize timeStampAddr = 0;

    if (result == Result::Success)
    {
        result = GetTimestampGpuAddress(slot, &timeStampAddr);
    }

    if ((result == Result::Success) && pCmdBuffer->IsQueryAllowed(QueryPoolType::StreamoutStats))
    {
        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        pCmdSpace = reinterpret_cast<UniversalCmdBuffer*>(pCmdBuffer)->VerifyStreamoutCtrlBuf(pCmdSpace);
        gpusize streamoutCtrlBuf = reinterpret_cast<UniversalCmdBuffer*>(pCmdBuffer)->GetStreamoutCtrlBufAddr();

        pCmdSpace += CmdUtil::BuildStreamoutStatsQuery(streamoutCtrlBuf,
                                                       static_cast<uint32>(queryType) -
                                                       static_cast<uint32>(QueryType::StreamoutStats),
                                                       gpuAddr + offsetof(Gfx12StreamoutStatsDataPair, end),
                                                       pCmdSpace);

        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.dstAddr           = timeStampAddr;
        releaseInfo.dataSel           = data_sel__me_release_mem__send_32_bit_low;
        releaseInfo.data              = QueryTimestampEnd;
        releaseInfo.vgtEvent          = VGT_EVENT_TYPE::BOTTOM_OF_PIPE_TS;

        pCmdSpace += m_device.CmdUtil().BuildReleaseMemGeneric(releaseInfo, pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
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
Result StreamoutStatsQueryPool::Reset(
    uint32 startQuery,
    uint32 queryCount,
    void*  pMappedCpuAddr)
{
    Result result = ValidateSlot(startQuery + queryCount - 1);

    if (result == Result::Success)
    {
        result = CpuReset(startQuery, queryCount, pMappedCpuAddr, 4, &StreamoutStatsResetMemValue32);
    }

    return result;
}

// =====================================================================================================================
void StreamoutStatsQueryPool::GpuReset(
    GfxCmdBuffer*   pCmdBuffer,
    Pal::CmdStream* pCmdStream,
    uint32          startQuery,
    uint32          queryCount
    ) const
{
    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    if (pCmdBuffer->IsQueryAllowed(QueryPoolType::StreamoutStats))
    {
        constexpr WriteWaitEopInfo WaitEopInfo = { .hwAcqPoint = AcquirePointMe };

        pCmdSpace = pCmdBuffer->WriteWaitEop(WaitEopInfo, pCmdSpace);
    }

    gpusize gpuAddr          = 0;
    gpusize timestampGpuAddr = 0;
    Result result            = GetQueryGpuAddress(startQuery, &gpuAddr);

    if (result == Result::Success)
    {
        result = GetTimestampGpuAddress(startQuery, &timestampGpuAddr);
    }

    if (result == Result::Success)
    {
        // Reset the query pool slots
        DmaDataInfo dmaData = {};
        dmaData.dstSel      = dst_sel__pfp_dma_data__dst_addr_using_l2;
        dmaData.dstAddr     = gpuAddr;
        dmaData.srcSel      = src_sel__pfp_dma_data__data;
        dmaData.srcData     = StreamoutStatsResetMemValue32;
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
}

// =====================================================================================================================
size_t StreamoutStatsQueryPool::GetResultSizeForOneSlot(
    Pal::QueryResultFlags flags
    ) const
{
    // Currently this function seems to be just referenced in QueryPool::GetResults so it doesn't
    // even need to be implemented at this point but just put two lines of code as simple enough

    // We only support 64-bit results.
    PAL_ASSERT(TestAnyFlagSet(flags, QueryResult64Bit));

    return TestAnyFlagSet(flags, QueryResultOnlyPrimNeeded) ? sizeof(Gfx12StreamoutStatsData::primStorageNeeded) :
                                                              sizeof(Gfx12StreamoutStatsData);
}

// =====================================================================================================================
bool StreamoutStatsQueryPool::ComputeResults(
    Pal::QueryResultFlags flags,
    QueryType             queryType,
    uint32                queryCount,
    size_t                stride,
    const void*           pGpuData,
    void*                 pData)
{
    bool queryReady = true;

    for (uint32 queryIdx = 0; queryIdx < queryCount; queryIdx++)
    {
        const Gfx12StreamoutStatsDataPair* pDataPair = static_cast<const Gfx12StreamoutStatsDataPair*>(pGpuData);
        Gfx12StreamoutStatsData* pQueryData          = static_cast<Gfx12StreamoutStatsData*>(pData);

        bool countersReady = false;
        do
        {
            countersReady = IsQueryDataValid(&pDataPair->end.primCountWritten)    &&
                            IsQueryDataValid(&pDataPair->begin.primCountWritten)  &&
                            IsQueryDataValid(&pDataPair->end.primStorageNeeded)   &&
                            IsQueryDataValid(&pDataPair->begin.primStorageNeeded) &&
                            (((pDataPair->end.primCountWritten   &
                               pDataPair->begin.primCountWritten &
                               pDataPair->end.primStorageNeeded  &
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
            uint32 offsetDistance = sizeof(Gfx12StreamoutStatsData);
            if (TestAnyFlagSet(flags, QueryResultOnlyPrimNeeded))
            {
                offsetDistance = sizeof(Gfx12StreamoutStatsData::primStorageNeeded);
            }

            uint64* pAvailabilityState = static_cast<uint64*>(VoidPtrInc(pData, offsetDistance));
            *pAvailabilityState = static_cast<uint64>(countersReady);
        }

        queryReady = queryReady && countersReady;

        pGpuData = VoidPtrInc(pGpuData, GetGpuResultSizeInBytes(1));
        pData    = VoidPtrInc(pData, stride);
    }

    return queryReady;
}

// =====================================================================================================================
bool StreamoutStatsQueryPool::IsQueryDataValid(
    volatile const uint64* pData
    ) const
{
    bool result = false;

    volatile const uint32* pData32 = reinterpret_cast<volatile const uint32*>(pData);

    if ((pData32[0] != StreamoutStatsResetMemValue32) || (pData32[1] != StreamoutStatsResetMemValue32))
    {
        if ((pData32[0] == StreamoutStatsResetMemValue32) || (pData32[1] == StreamoutStatsResetMemValue32))
        {
            std::atomic_thread_fence(std::memory_order_acq_rel);
        }
        result = true;
    }

    return result;
}

} // Gfx12
} // Pal
