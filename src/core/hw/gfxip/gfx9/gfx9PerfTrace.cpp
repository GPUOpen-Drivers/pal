/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9PerfCounter.h"
#include "core/hw/gfxip/gfx9/gfx9PerfTrace.h"
#include "palDequeImpl.h"

#include "core/hw/amdgpu_asic.h"

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
Gfx9SpmTrace::Gfx9SpmTrace(
    const Device* pDevice)
    :
    Pal::SpmTrace(pDevice->Parent()),
    m_device(*pDevice)
{
    m_ringBaseLo.u32All     = 0;
    m_ringBaseHi.u32All     = 0;
    m_segmentSize.u32All    = 0;
}

// =====================================================================================================================
// Initializes some member variables and creates copy of SpmTraceCreateInfo.
Result Gfx9SpmTrace::Init(
    const SpmTraceCreateInfo& createInfo)
{
    Result result = Result::Success;

    m_ringSize.bits.RING_BASE_SIZE = createInfo.ringSize;

    m_spmPerfmonCntl.u32All = 0;
    m_spmPerfmonCntl.bits.PERFMON_SAMPLE_INTERVAL = static_cast<uint16>(createInfo.spmInterval);

    PAL_ASSERT(m_spmPerfmonCntl.bits.PERFMON_SAMPLE_INTERVAL == createInfo.spmInterval);
    m_numPerfCounters = createInfo.numPerfCounters;

    void* pMem = PAL_MALLOC(createInfo.numPerfCounters * sizeof(PerfCounterInfo),
                            m_device.GetPlatform(),
                            Util::SystemAllocType::AllocInternal);
    if (pMem != nullptr)
    {
        m_pPerfCounterCreateInfos = static_cast<PerfCounterInfo*>(pMem);
        memcpy(m_pPerfCounterCreateInfos,
               createInfo.pPerfCounterInfos,
               createInfo.numPerfCounters * sizeof(PerfCounterInfo));
    }
    else
    {
        result = Result::ErrorOutOfMemory;
    }

    return result;
}

// =====================================================================================================================
// Issues the PM4 commands necessary to start this thread trace. The owning Experiment object should have issued and
// idle before calling this. Returns the next unused DWORD in pCmdSpace.
uint32* Gfx9SpmTrace::WriteSetupCommands(
    gpusize         ringBaseAddr,
    Pal::CmdStream* pCmdStream,
    uint32*         pCmdSpace)
{
    CmdStream* pHwlCmdStream = static_cast<CmdStream*>(pCmdStream);

    // (1) Write setup commands for each streaming perf counter.
    StreamingPerfCounter* pStreamingCounter = nullptr;
    for (auto iter = m_spmCounters.Begin(); iter.Get(); iter.Next())
    {
        pCmdStream->CommitCommands(pCmdSpace);
        pCmdStream->ReserveCommands();

        pStreamingCounter = static_cast<StreamingPerfCounter*>(*iter.Get());

        // We might have to reset the GRBM_GFX_INDEX for programming more counters as it would've been changed for
        // programming indexed counters previously.
        if (m_flags.hasIndexedCounters)
        {
            regGRBM_GFX_INDEX__GFX09 grbmGfxIndex = {};
            grbmGfxIndex.bits.SE_BROADCAST_WRITES       = 1;
            grbmGfxIndex.bits.SH_BROADCAST_WRITES       = 1;
            grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

            pHwlCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                                grbmGfxIndex.u32All,
                                                pCmdSpace);
        }

        pCmdSpace = pStreamingCounter->WriteSetupCommands(pCmdStream, pCmdSpace);
    }

    // (2) Write muxsel ram.
    const uint32 NumShaderEngines = m_device.Parent()->ChipProperties().gfx6.numShaderEngines;

    for (uint32 seIndex = 0; seIndex < static_cast<uint32>(SpmDataSegmentType::Count); ++seIndex)
    {
        const uint32 muxselRamDwords = GetMuxselRamDwords(seIndex);

        // Write commands to write the muxsel ram data only if there is any data to write.
        if (muxselRamDwords != 0)
        {
            if (seIndex != static_cast<uint32>(SpmDataSegmentType::Global))
            {
                // Write the per-SE muxsel ram data.
                regGRBM_GFX_INDEX__GFX09 grbmGfxIndex              = {};
                grbmGfxIndex.bits.SE_INDEX                  = seIndex;
                grbmGfxIndex.bits.SH_BROADCAST_WRITES       = 1;
                grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

                pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                                                grbmGfxIndex.u32All,
                                                                pCmdSpace);

                pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_SE_MUXSEL_ADDR__GFX09,
                                                                 0,
                                                                 pCmdSpace);

                for (uint32 i = 0; i < muxselRamDwords; ++i)
                {
                    // Depending on the number of counters requested and the SE configuration a large number of
                    // write_data packets can be generated.
                    pCmdStream->CommitCommands(pCmdSpace);
                    pCmdStream->ReserveCommands();

                    pCmdSpace += m_device.CmdUtil().BuildWriteData(pCmdStream->GetEngineType(),
                                                                   mmRLC_SPM_SE_MUXSEL_DATA__GFX09,
                                                                   1,
                                                                   engine_sel__me_write_data__micro_engine,
                                                                   dst_sel__me_write_data__mem_mapped_register,
                                                                   true, // Wait for write confirmation
                                                                   (m_muxselRamData[seIndex].pMuxselRamUint32 + i),
                                                                   PredDisable,
                                                                   pCmdSpace);
                }
            }
            else
            {
                // Write the global muxsel ram data.
                regGRBM_GFX_INDEX__GFX09 grbmGfxIndex              = {};
                grbmGfxIndex.bits.SE_BROADCAST_WRITES       = 1;
                grbmGfxIndex.bits.SH_BROADCAST_WRITES       = 1;
                grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

                pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                                                grbmGfxIndex.u32All,
                                                                pCmdSpace);

                pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_GLOBAL_MUXSEL_ADDR__GFX09,
                                                                 0,
                                                                 pCmdSpace);

                for (uint32 i = 0; i < muxselRamDwords; ++i)
                {
                    pCmdStream->CommitCommands(pCmdSpace);
                    pCmdStream->ReserveCommands();

                    pCmdSpace += m_device.CmdUtil().BuildWriteData(pCmdStream->GetEngineType(),
                                                                   mmRLC_SPM_GLOBAL_MUXSEL_DATA__GFX09,
                                                                   1,
                                                                   engine_sel__me_write_data__micro_engine,
                                                                   dst_sel__me_write_data__mem_mapped_register,
                                                                   1, // Wait for write confirmation
                                                                   (m_muxselRamData[seIndex].pMuxselRamUint32 + i),
                                                                   PredDisable,
                                                                   pCmdSpace);
                }
            }
        }
    }

    // (3) Write the relevant RLC registers
    // Compute the start of the spm trace buffer location.
    const gpusize gpuVirtAddrShifted = (ringBaseAddr + m_dataOffset);

    m_spmPerfmonCntl.bits.PERFMON_RING_MODE = 0;
    m_ringBaseLo.u32All = Util::LowPart(gpuVirtAddrShifted);
    m_ringBaseHi.u32All = Util::HighPart(gpuVirtAddrShifted);

    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_CNTL,
                                                     m_spmPerfmonCntl.u32All,
                                                     pCmdSpace);

    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_SEGMENT_SIZE,
                                                     m_segmentSize.u32All,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_BASE_LO,
                                                     m_ringBaseLo.u32All,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_BASE_HI,
                                                     m_ringBaseHi.u32All,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_SIZE,
                                                     m_ringSize.u32All,
                                                     pCmdSpace);

    // We do not use the ringing functionality of the output buffers, so always write 0 as the RDPTR.
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_RING_RDPTR__GFX09, 0, pCmdSpace);

    // Finally, disable and reset all counters.
    regCP_PERFMON_CNTL cpPerfmonCntl     = { };
    cpPerfmonCntl.bits.PERFMON_STATE     = CP_PERFMON_STATE_DISABLE_AND_RESET;
    cpPerfmonCntl.bits.SPM_PERFMON_STATE = CP_PERFMON_STATE_DISABLE_AND_RESET;

    pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(mmCP_PERFMON_CNTL,
                                                    cpPerfmonCntl.u32All,
                                                    pCmdSpace);

    // NOTE: It is the caller's responsibility to reset GRBM_GFX_INDEX.

    return pCmdSpace;
}

// =====================================================================================================================
uint32* Gfx9SpmTrace::WriteStartCommands(
    Pal::CmdStream* pCmdStream,
    uint32*         pCmdSpace)
{
    CmdStream* pHwlCmdStream = static_cast<CmdStream*>(pCmdStream);

    regCP_PERFMON_CNTL cpPerfmonCntl         = { };
    cpPerfmonCntl.bits.PERFMON_STATE         = CP_PERFMON_STATE_START_COUNTING;
    cpPerfmonCntl.bits.SPM_PERFMON_STATE     = CP_PERFMON_STATE_START_COUNTING;
    cpPerfmonCntl.bits.PERFMON_SAMPLE_ENABLE = 1;

    pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(mmCP_PERFMON_CNTL,
                                                    cpPerfmonCntl.u32All,
                                                    pCmdSpace);
    return pCmdSpace;
}

// =====================================================================================================================
uint32* Gfx9SpmTrace::WriteEndCommands(
    Pal::CmdStream* pCmdStream,
    uint32* pCmdSpace)
{
    CmdStream* pHwlCmdStream = static_cast<CmdStream*>(pCmdStream);

    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_CNTL,
                                                     0,
                                                     pCmdSpace);

    // Write segment size, ring buffer size, ring buffer address registers
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_SEGMENT_SIZE,
                                                     0,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_SIZE,
                                                     0,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_BASE_LO,
                                                     0,
                                                     pCmdSpace);
    pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(mmRLC_SPM_PERFMON_RING_BASE_HI,
                                                     0,
                                                     pCmdSpace);

    uint32 muxselRamDwords;
    regGRBM_GFX_INDEX__GFX09 grbmGfxIndex = { };
    grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;
    grbmGfxIndex.bits.SH_BROADCAST_WRITES       = 1;

    uint32 muxselAddrReg = mmRLC_SPM_SE_MUXSEL_ADDR__GFX09;
    const uint32 NumShaderEngines = m_device.Parent()->ChipProperties().gfx9.numShaderEngines;

    // Reset the muxsel addr register.
    for (uint32 seIndex = 0; seIndex < static_cast<uint32>(SpmDataSegmentType::Count); ++seIndex)
    {
        muxselRamDwords = GetMuxselRamDwords(seIndex);

        if (muxselRamDwords != 0)
        {
            grbmGfxIndex.bits.SE_INDEX = seIndex;

            if (seIndex == static_cast<uint32>(SpmDataSegmentType::Global))
            {
                // Global section.
                grbmGfxIndex.bits.SE_INDEX            = 0;
                grbmGfxIndex.bits.SE_BROADCAST_WRITES = 1;
                muxselAddrReg = mmRLC_SPM_GLOBAL_MUXSEL_ADDR__GFX09;
            }

            pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                                            grbmGfxIndex.u32All,
                                                            pCmdSpace);

            pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(muxselAddrReg, 0, pCmdSpace);
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Calculates number of 256-bit lines needed for the muxsel ram. The segment size also determines the layout of the
// RLC ring buffer.
void Gfx9SpmTrace::CalculateSegmentSize()
{
    Result result = Result::Success;

    // Array to track counter parity counts. Size is number of shader engines + 1 for global counters.
    ParityCount seParityCounts[static_cast<uint32>(SpmDataSegmentType::Count)];
    memset(&seParityCounts, 0, sizeof(seParityCounts));

    // Increment count in the global segment for GPU timestamp. The last element of the seParityCounts is used for
    // global counts.
    seParityCounts[static_cast<uint32>(SpmDataSegmentType::Global)].evenCount = 4;

    for (auto it = m_spmCounters.Begin(); it.Get(); it.Next())
    {
        // Check if block uses global or per-SE RLC HW.
        Pal::StreamingPerfCounter* pCounter = *it.Get();
        GpuBlock block                      = pCounter->BlockType();
        uint32  seIndex                     = 0;

        if (BlockUsesGlobalMuxsel(block))
        {
            seIndex = static_cast<uint32>(SpmDataSegmentType::Global);
            pCounter->SetSegmentIndex(SpmDataSegmentType::Global);
        }
        else
        {
            seIndex = PerfCounter::GetSeIndex(m_device.Parent()->ChipProperties().gfx9.perfCounterInfo,
                                              pCounter->BlockType(),
                                              pCounter->GetInstanceId());

            pCounter->SetSegmentIndex(static_cast<SpmDataSegmentType>(seIndex));
        }

        // Check if it is an even counter or an odd counter and increment the appropriate counts.
        for (uint32 i = 0; i < PerfCtrInfo::Gfx9StreamingCtrsPerSummaryCtr; ++i)
        {
            if(pCounter->GetEventId(i) != StreamingPerfCounter::InvalidEventId)
            {
                uint32 streamingCounterId = (block == GpuBlock::Sq) ? pCounter->GetSlot() :
                                            (pCounter->GetSlot() * PerfCtrInfo::Gfx9StreamingCtrsPerSummaryCtr + i);

                if (streamingCounterId % 2)
                {
                    seParityCounts[seIndex].oddCount++;
                }
                else
                {
                    seParityCounts[seIndex].evenCount++;
                }
            }
        }
    }

    // Pad out the even/odd counts to the width of bit lines. There can be a maximum of 16 muxsels per bit line.
    for (uint32 i = 0; i < static_cast<uint32>(SpmDataSegmentType::Count); ++i)
    {
        if ((seParityCounts[i].evenCount % MuxselEntriesPerBitline) != 0)
        {
            seParityCounts[i].evenCount += MuxselEntriesPerBitline - (seParityCounts[i].evenCount %
                                                                          MuxselEntriesPerBitline);
        }

        if ((seParityCounts[i].oddCount % MuxselEntriesPerBitline) != 0)
        {
            seParityCounts[i].oddCount += MuxselEntriesPerBitline - (seParityCounts[i].oddCount %
                                                                         MuxselEntriesPerBitline);
        }
    }

    m_segmentSize.u32All = 0;

    // Calculate number of bit lines of size 256-bits. This is used for the mux selects as well as the ring buffer.
    // Even lines hold counter0 and counter2, while odd lines hold counter1 and counter3. We need double of whichever
    // we have more of.
    // Example: If we have 32 global deltas coming from counter0 and counter2 and 16 deltas coming from counter1 and
    //          counter3, then we need four lines ( 2 * Max( 2 even, 1 odd)). Lines 0 and 2 hold the delta
    //          values coming from counter0,2 while Line 1 holds the delta values coming from counter1,3. Line 3 is
    //          empty.

    // Global counters.
    uint32 index                       = static_cast<uint32>(SpmDataSegmentType::Global);
    uint32 numEvenBitLines             = seParityCounts[index].evenCount / MuxselEntriesPerBitline;
    uint32 numOddBitLines              = seParityCounts[index].oddCount / MuxselEntriesPerBitline;
    m_segmentSize.bits.GLOBAL_NUM_LINE = 2 * Util::Max(numEvenBitLines, numOddBitLines);

    // SE0
    index                           = static_cast<uint32>(SpmDataSegmentType::Se0);
    numEvenBitLines                 = seParityCounts[index].evenCount / MuxselEntriesPerBitline;
    numOddBitLines                  = seParityCounts[index].oddCount / MuxselEntriesPerBitline;
    m_segmentSize.bits.SE0_NUM_LINE = 2 * Util::Max(numEvenBitLines, numOddBitLines);

    // SE1
    index                           = static_cast<uint32>(SpmDataSegmentType::Se1);
    numEvenBitLines                 = seParityCounts[index].evenCount / MuxselEntriesPerBitline;
    numOddBitLines                  = seParityCounts[index].oddCount / MuxselEntriesPerBitline;
    m_segmentSize.bits.SE1_NUM_LINE = 2 * Util::Max(numEvenBitLines, numOddBitLines);

    // SE2
    index                           = static_cast<uint32>(SpmDataSegmentType::Se2);
    numEvenBitLines                 = seParityCounts[index].evenCount / MuxselEntriesPerBitline;
    numOddBitLines                  = seParityCounts[index].oddCount / MuxselEntriesPerBitline;
    m_segmentSize.bits.SE2_NUM_LINE = 2 * Util::Max(numEvenBitLines, numOddBitLines);

    // SE3 does not have to be entered. It is calculated in the HW by subtracting sum of other segments from the total.
    index                       = static_cast<uint32>(SpmDataSegmentType::Se3);
    const uint32 se3SegmentSize = 2 * Util::Max(seParityCounts[index].evenCount / MuxselEntriesPerBitline,
                                                seParityCounts[index].oddCount / MuxselEntriesPerBitline);

    // Total segment size.
    m_segmentSize.bits.PERFMON_SEGMENT_SIZE = m_segmentSize.bits.GLOBAL_NUM_LINE +
                                              m_segmentSize.bits.SE0_NUM_LINE +
                                              m_segmentSize.bits.SE1_NUM_LINE +
                                              m_segmentSize.bits.SE2_NUM_LINE +
                                              se3SegmentSize;
}

// =====================================================================================================================
uint32 Gfx9SpmTrace::GetMuxselRamDwords(
    uint32 seIndex
    ) const
{
    // We will always have at least one global line for the timestamp. This value can only be zero if
    // CalculateSegmentSize has not been called.
    PAL_ASSERT(m_segmentSize.bits.GLOBAL_NUM_LINE != 0);
    uint32 muxselRamDwords = 0;

    switch (seIndex)
    {
    case 0:
        muxselRamDwords = m_segmentSize.bits.SE0_NUM_LINE * (NumBitsPerBitline / 32);
        break;
    case 1:
        muxselRamDwords = m_segmentSize.bits.SE1_NUM_LINE * (NumBitsPerBitline / 32);
        break;
    case 2:
        muxselRamDwords = m_segmentSize.bits.SE2_NUM_LINE * (NumBitsPerBitline / 32);
        break;
    case 3:
        muxselRamDwords = (m_segmentSize.bits.PERFMON_SEGMENT_SIZE - (m_segmentSize.bits.SE0_NUM_LINE +
                                                                      m_segmentSize.bits.SE1_NUM_LINE +
                                                                      m_segmentSize.bits.SE2_NUM_LINE +
                                                                      m_segmentSize.bits.GLOBAL_NUM_LINE)) *
                           (NumBitsPerBitline / 32);
        break;
    case PerfCtrInfo::MaxNumShaderEngines:
        muxselRamDwords = m_segmentSize.bits.GLOBAL_NUM_LINE * (NumBitsPerBitline / 32);
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return muxselRamDwords;
}

// =====================================================================================================================
void Gfx9SpmTrace::CalculateMuxRam()
{
    // Allocate memory for the muxsel ram data based on the segment size previously calculated.
    for (uint32 se = 0; se < static_cast<uint32>(SpmDataSegmentType::Count); se++)
    {
        uint32 muxselDwords = GetMuxselRamDwords(se);

        if (muxselDwords != 0)
        {
            // We allocate the muxsel RAM space in dwords and write the muxsel RAM in RLC with write_data packets as
            // dwords, but we calculate and write the values in system memory as uint16.
            m_muxselRamData[se].pMuxselRamUint32 = static_cast<uint32*>(
                PAL_CALLOC(sizeof(uint32) * muxselDwords,
                           m_device.GetPlatform(),
                           Util::SystemAllocType::AllocInternal));

            // Memory allocation failed.
            PAL_ASSERT(m_muxselRamData[se].pMuxselRamUint32 != nullptr);
        }
    }

    /*
    *    Example layout of the muxsel ram:
    *
    *      +---------------------+--------------------+---------------------+--
    * SE0: |       Even          |       Odd          |       Even          | ...
    *      +---------------------+--------------------+---------------------+--
    */

    struct MuxselWriteIndex
    {
        uint32 evenIndex;
        uint32 oddIndex;
    };

    // This stores the indices in the mux select ram data to which the next mux select must be written to.
    MuxselWriteIndex muxselWriteIndices[static_cast<uint32>(SpmDataSegmentType::Count)];

    // Initialize the muxsel write indices. Even indices start at 0, while odd indices start at 16.
    for (uint32 index = 0; index < static_cast<uint32>(SpmDataSegmentType::Count); index++)
    {
        muxselWriteIndices[index].evenIndex = 0;
        muxselWriteIndices[index].oddIndex  = MuxselEntriesPerBitline;
    }

    // Enter the muxsel encoding for GPU timestamp in the global section, in the even bit line.
    m_muxselRamData[static_cast<uint32>(SpmDataSegmentType::Global)].pMuxselRamUint32[0] = 0xF0F0F0F0;
    m_muxselRamData[static_cast<uint32>(SpmDataSegmentType::Global)].pMuxselRamUint32[1] = 0xF0F0F0F0;
    muxselWriteIndices[static_cast<uint32>(SpmDataSegmentType::Global)].evenIndex        = 4;

    Pal::StreamingPerfCounter* pCounter = nullptr;

    // Iterate over our deque of counters and write out the muxsel ram data.
    for (auto iter = m_spmCounters.Begin(); iter.Get(); iter.Next())
    {
        pCounter = *iter.Get();
        GpuBlock block    = pCounter->BlockType();

        for (uint32 subSlot = 0; subSlot < MaxNumStreamingCtrPerSummaryCtr; ++subSlot)
        {
            if (pCounter->GetEventId(subSlot) != StreamingPerfCounter::InvalidEventId)
            {
                uint32 seIndex            = 0;
                uint32* pWriteIndex       = nullptr;
                PerfmonSelData muxselData = {};

                if (BlockUsesGlobalMuxsel(block))
                {
                    muxselData = GetGlobalMuxselData(block, pCounter->GetInstanceId(), subSlot);
                    seIndex    = static_cast<uint32>(SpmDataSegmentType::Global);
                }
                else
                {
                    muxselData = GetPerSeMuxselData(block, pCounter->GetInstanceId(), subSlot);
                    seIndex    = PerfCounter::GetSeIndex(m_device.Parent()->ChipProperties().gfx9.perfCounterInfo,
                                                         pCounter->BlockType(),
                                                         pCounter->GetInstanceId());
                }

                // Write the mux select data in the appropriate location based on even/odd counterId (subSlot).
                if (subSlot % 2)
                {
                    pWriteIndex = &muxselWriteIndices[seIndex].oddIndex;
                }
                else
                {
                    pWriteIndex = &muxselWriteIndices[seIndex].evenIndex;
                }

                m_muxselRamData[seIndex].pMuxselRamUint16[*pWriteIndex] = muxselData.u16All;

                // Find the offset into the output buffer for this counter.
                uint32 offset = *pWriteIndex;

                // Calculate offset within the sample for this counter's data. This is where the HW will write the
                // counter value. Use the offset as-is for the global block, since it is the first segment within the
                // sample.
                if (BlockUsesGlobalMuxsel(block) == false)
                {
                    offset += m_segmentSize.bits.GLOBAL_NUM_LINE * 256 / 16;

                    // Se1
                    if (seIndex > 0)
                    {
                        offset += m_segmentSize.bits.SE0_NUM_LINE *  256 / 16;
                    }

                    if (seIndex > 1)
                    {
                        offset += m_segmentSize.bits.SE1_NUM_LINE * 256 / 16;
                    }

                    if (seIndex > 2)
                    {
                        offset += m_segmentSize.bits.SE2_NUM_LINE * 256 / 16;
                    }
                }

                // Offset 0 to 3 holds the GPU timestamp.
                PAL_ASSERT(offset > 3);
                pCounter->SetDataOffset(subSlot, offset);

                ++(*pWriteIndex);

                // Advance the write index to the next even/odd section once 16 mux selects have been written in the
                // current section.
                if ((*pWriteIndex % MuxselEntriesPerBitline) == 0)
                {
                    (*pWriteIndex) += MuxselEntriesPerBitline;
                }
            } // Valid eventID.
        } // Iterate over subSlots in the counter.
    } // Iterate over StreamingPerfCounters.
}

// =====================================================================================================================
Result Gfx9SpmTrace::GetTraceLayout(
    SpmTraceLayout* pLayout
    ) const
{
    Result result = Result::Success;

    pLayout->offset       = m_dataOffset;
    pLayout->wptrOffset   = m_dataOffset;       // The very first dword is the wptr.
    pLayout->sampleOffset = 8 * sizeof(uint32); // Data begins 8 dwords from the beginning of the buffer.

    // Fill in the segment parents.
    pLayout->sampleSizeInBytes = m_segmentSize.bits.PERFMON_SEGMENT_SIZE * (NumBitsPerBitline / 8);
    pLayout->segmentSizeInBytes[static_cast<uint32>(SpmDataSegmentType::Global)] =
        m_segmentSize.bits.GLOBAL_NUM_LINE * (NumBitsPerBitline / 8);
    pLayout->segmentSizeInBytes[static_cast<uint32>(SpmDataSegmentType::Se0)] =
        m_segmentSize.bits.SE0_NUM_LINE * (NumBitsPerBitline / 8);
    pLayout->segmentSizeInBytes[static_cast<uint32>(SpmDataSegmentType::Se1)] =
        m_segmentSize.bits.SE1_NUM_LINE * (NumBitsPerBitline / 8);
    pLayout->segmentSizeInBytes[static_cast<uint32>(SpmDataSegmentType::Se2)] =
        m_segmentSize.bits.SE2_NUM_LINE * (NumBitsPerBitline / 8);
    pLayout->segmentSizeInBytes[static_cast<uint32>(SpmDataSegmentType::Se3)] =
        (m_segmentSize.bits.PERFMON_SEGMENT_SIZE - (m_segmentSize.bits.GLOBAL_NUM_LINE +
                                                    m_segmentSize.bits.SE0_NUM_LINE +
                                                    m_segmentSize.bits.SE1_NUM_LINE +
                                                    m_segmentSize.bits.SE2_NUM_LINE )) * (NumBitsPerBitline / 8);

    // There must be enough space in the layout allocation for all the counters that were requested.
    PAL_ASSERT(pLayout->numCounters == m_numPerfCounters);

    // Fill in the SpmCounterInfo array.
    for (uint32 i = 0; i < m_numPerfCounters; ++i)
    {
        for (auto iter = m_spmCounters.Begin(); iter.Get() != nullptr; iter.Next())
        {
            Pal::StreamingPerfCounter* pHwCounter = *(iter.Get());

            if ((m_pPerfCounterCreateInfos[i].block    == pHwCounter->BlockType()) &&
                (m_pPerfCounterCreateInfos[i].instance == pHwCounter->GetInstanceId()))
            {
                for (uint32 subSlot = 0; subSlot < MaxNumStreamingCtrPerSummaryCtr; subSlot++)
                {
                    const uint32 eventId = pHwCounter->GetEventId(subSlot);

                    if (m_pPerfCounterCreateInfos[i].eventId == eventId)
                    {
                        // We have found the matching HW counter and the API counter.
                        pLayout->counterData[i].offset   = pHwCounter->GetDataOffset(subSlot);
                        pLayout->counterData[i].segment  = pHwCounter->GetSpmSegmentIndex();
                        pLayout->counterData[i].eventId  = eventId;
                        pLayout->counterData[i].gpuBlock = m_pPerfCounterCreateInfos[i].block;
                        pLayout->counterData[i].instance = m_pPerfCounterCreateInfos[i].instance;
                    }
                }
            }
        }
    }

    pLayout->wptrOffset = 0;

    return result;
}

// =====================================================================================================================
ThreadTrace::ThreadTrace(
    const Device*        pDevice,   ///< [retained] Associated Device object
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 373
    const PerfTraceInfo& info)      ///< [in] Trace creation info
#else
    const ThreadTraceInfo& info)    ///< [in] Trace creation info
#endif
    :
    Pal::ThreadTrace(pDevice->Parent(), info),
    m_device(*pDevice),
    m_info(info)
{
}

// =====================================================================================================================
// Issues the PM4 commands necessary to insert a thread trace marker. Returns the next unused DWORD in pCmdSpace.
uint32* ThreadTrace::WriteInsertMarker(
    PerfTraceMarkerType markerType, ///< Trace marker type
    uint32              data,       ///< Trace marker data payload
    CmdStream*          pCmdStream,
    uint32*             pCmdSpace
    ) const
{

    uint32 userDataRegAddr = 0;
    switch (markerType)
    {
    case PerfTraceMarkerType::A:
        userDataRegAddr = m_device.CmdUtil().GetRegInfo().mmSqThreadTraceUserData2;
        break;

    case PerfTraceMarkerType::B:
        userDataRegAddr = m_device.CmdUtil().GetRegInfo().mmSqThreadTraceUserData3;
        break;

    default:
        break;
    }

    // If this assert fires, we forgot to add a thread trace marker type to this method!
    PAL_ASSERT(userDataRegAddr != 0);

    // Writing the SQ_THREAD_TRACE_USERDATA_* register will cause the thread trace to insert
    // a user-data event with value of the register.
    return pCmdStream->WriteSetOnePerfCtrReg(userDataRegAddr, data, pCmdSpace);
}

// =====================================================================================================================
Gfx9ThreadTrace::Gfx9ThreadTrace(
    const Device*         pDevice,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 373
    const PerfTraceInfo&  info)
#else
    const ThreadTraceInfo&  info)
#endif
    :
    ThreadTrace(pDevice, info)
{
    m_sqThreadTraceSize.u32All = 0;

    m_sqThreadTraceMode.u32All = 0;
    m_sqThreadTraceMode.bits.MASK_PS      = 1;
    m_sqThreadTraceMode.bits.MASK_VS      = 1;
    m_sqThreadTraceMode.bits.MASK_GS      = 1;
    m_sqThreadTraceMode.bits.MASK_ES      = 1;
    m_sqThreadTraceMode.bits.MASK_HS      = 1;
    m_sqThreadTraceMode.bits.MASK_LS      = 1;
    m_sqThreadTraceMode.bits.MASK_CS      = 1;
    m_sqThreadTraceMode.bits.AUTOFLUSH_EN = 1;

    m_sqThreadTraceMask.u32All = 0;
    m_sqThreadTraceMask.bits.SIMD_EN     = SimdMaskAll;
    m_sqThreadTraceMask.bits.VM_ID_MASK  = SQ_THREAD_TRACE_VM_ID_MASK_SINGLE;

    const GpuChipProperties& chipProps = pDevice->Parent()->ChipProperties();

    // We need to pull some register fields for SQ_THREAD_TRACE_MASK from the Adapter.
    regSQ_THREAD_TRACE_MASK__GFX09 sqThreadTraceMask = {};

    m_sqThreadTraceMask.bits.REG_STALL_EN = sqThreadTraceMask.bits.REG_STALL_EN;
    m_sqThreadTraceMask.bits.SQ_STALL_EN  = sqThreadTraceMask.bits.SQ_STALL_EN;
    m_sqThreadTraceMask.bits.SPI_STALL_EN = sqThreadTraceMask.bits.SPI_STALL_EN;

    // NOTE: DXX mentions in a comment that for Oland, the driver may need to force
    //       SPI_STALL_EN to zero to avoid doubly creating some wavefronts, avoiding a
    //       possible hang situation.
    // TODO: Need to follow-up on this, the DXX comments for it are unclear.

    m_sqThreadTraceTokenMask.u32All = 0;
    m_sqThreadTraceTokenMask.bits.TOKEN_MASK = TokenMaskAll;
    m_sqThreadTraceTokenMask.bits.REG_MASK   = RegMaskAll;

    m_sqThreadTracePerfMask.u32All = 0;
    m_sqThreadTracePerfMask.bits.SH0_MASK = ShCuMaskAll;
    m_sqThreadTracePerfMask.bits.SH1_MASK = ShCuMaskAll;

    // Default to only selecting CUs that are active and not reserved for realtime use.  GFX9 only has one
    // shader array.
    PAL_ASSERT(chipProps.gfx9.numShaderArrays == 1);
    const uint32 cuTraceableCuMask = chipProps.gfx9.activeCuMask[0][m_shaderEngine] & ~chipProps.gfxip.realTimeCuMask;

    // If it exists, select the first available CU from the mask
    uint32 firstActiveCu = 0;
    if (Util::BitMaskScanForward(&firstActiveCu, cuTraceableCuMask))
    {
        m_sqThreadTraceMask.bits.CU_SEL = firstActiveCu;
    }

    SetOptions();
}

// =====================================================================================================================
// Initializes one of the thread-trace creation options.
void Gfx9ThreadTrace::SetOptions()
{
    const auto& flags  = m_info.optionFlags;
    const auto& values = m_info.optionValues;

    const size_t bufferSize = (flags.bufferSize) ? values.bufferSize : PerfCtrInfo::DefaultBufferSize;

    m_sqThreadTraceSize.bits.SIZE = (bufferSize >> PerfCtrInfo::BufferAlignShift);

    // Need to update our buffer-size parameter.
    m_dataSize = bufferSize;

    if (flags.threadTraceTokenMask)
    {
        m_sqThreadTraceTokenMask.bits.TOKEN_MASK = values.threadTraceTokenMask;
    }

    if (flags.threadTraceRegMask)
    {
        m_sqThreadTraceTokenMask.bits.REG_MASK = values.threadTraceRegMask;
    }

    if (flags.threadTraceTargetSh)
    {
        m_sqThreadTraceMask.bits.SH_SEL = values.threadTraceTargetSh;
    }

    if (flags.threadTraceTargetCu)
    {
        m_sqThreadTraceMask.bits.CU_SEL = values.threadTraceTargetCu;
    }

    if (flags.threadTraceSh0CounterMask)
    {
        m_sqThreadTracePerfMask.bits.SH0_MASK = values.threadTraceSh0CounterMask;
    }

    if (flags.threadTraceSh1CounterMask)
    {
        m_sqThreadTracePerfMask.bits.SH1_MASK = values.threadTraceSh1CounterMask;
    }

    if (flags.threadTraceSimdMask)
    {
        m_sqThreadTraceMask.bits.SIMD_EN = values.threadTraceSimdMask;
    }

    if (flags.threadTraceVmIdMask)
    {
        m_sqThreadTraceMask.bits.VM_ID_MASK = values.threadTraceVmIdMask;
    }

    if (flags.threadTraceShaderTypeMask)
    {
        m_sqThreadTraceMode.bits.MASK_PS = (values.threadTraceShaderTypeMask & PerfShaderMaskPs) ? 1 : 0;
        m_sqThreadTraceMode.bits.MASK_VS = (values.threadTraceShaderTypeMask & PerfShaderMaskVs) ? 1 : 0;
        m_sqThreadTraceMode.bits.MASK_GS = (values.threadTraceShaderTypeMask & PerfShaderMaskGs) ? 1 : 0;
        m_sqThreadTraceMode.bits.MASK_ES = (values.threadTraceShaderTypeMask & PerfShaderMaskEs) ? 1 : 0;
        m_sqThreadTraceMode.bits.MASK_HS = (values.threadTraceShaderTypeMask & PerfShaderMaskHs) ? 1 : 0;
        m_sqThreadTraceMode.bits.MASK_LS = (values.threadTraceShaderTypeMask & PerfShaderMaskLs) ? 1 : 0;
        m_sqThreadTraceMode.bits.MASK_CS = (values.threadTraceShaderTypeMask & PerfShaderMaskCs) ? 1 : 0;
    }

    if (flags.threadTraceIssueMask)
    {
        m_sqThreadTraceMode.bits.ISSUE_MASK = values.threadTraceIssueMask;
    }

    if (flags.threadTraceWrapBuffer)
    {
        m_sqThreadTraceMode.bits.WRAP = values.threadTraceWrapBuffer;
    }
}

// =====================================================================================================================
// Issues commands to set-up the GRBM_GFX_INDEX register to write to only the Shader Engine and Shader Array that this
// trace is associated with. Returns the next unused DWORD in pCmdSpace.
uint32* Gfx9ThreadTrace::WriteGrbmGfxIndex(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    regGRBM_GFX_INDEX__GFX09 grbmGfxIndex = {};
    grbmGfxIndex.bits.SE_INDEX                  = m_shaderEngine;
    grbmGfxIndex.bits.SH_INDEX                  = m_sqThreadTraceMask.bits.SH_SEL;
    grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

    return pCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                            grbmGfxIndex.u32All,
                                            pCmdSpace);
}

// =====================================================================================================================
// Issues the PM4 commands necessary to setup this thread trace. Returns the next unused DWORD in pCmdSpace.
uint32* Gfx9ThreadTrace::WriteSetupCommands(
    gpusize    baseGpuVirtAddr, ///< Base GPU virtual address of the owning Experiment
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // Set GRBM_GFX_INDEX to isolate the SE/SH this trace is associated with.
    pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);

    // Compute the base address of the thread trace data, including the shift amount the
    // register expects.
    const gpusize gpuVirtAddrShifted = (baseGpuVirtAddr + m_dataOffset) >> PerfCtrInfo::BufferAlignShift;

    // Write the base address of the thread trace buffer.
    regSQ_THREAD_TRACE_BASE2__GFX09 sqThreadTraceBase2 = {};
    sqThreadTraceBase2.bits.ADDR_HI = Util::HighPart(gpuVirtAddrShifted);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_BASE2__GFX09,
                                                  sqThreadTraceBase2.u32All,
                                                  pCmdSpace);

    regSQ_THREAD_TRACE_BASE__GFX09 sqThreadTraceBase = {};
    sqThreadTraceBase.bits.ADDR = Util::LowPart(gpuVirtAddrShifted);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_BASE__GFX09,
                                                  sqThreadTraceBase.u32All,
                                                  pCmdSpace);

    // Write the perf counter registers which control the thread trace properties.
    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_SIZE__GFX09,
                                                  m_sqThreadTraceSize.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_MASK__GFX09,
                                                  m_sqThreadTraceMask.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_TOKEN_MASK__GFX09,
                                                  m_sqThreadTraceTokenMask.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_PERF_MASK__GFX09,
                                                  m_sqThreadTracePerfMask.u32All,
                                                  pCmdSpace);

    // NOTE: It is the caller's responsibility to reset GRBM_GFX_INDEX.

    return pCmdSpace;
}

// =====================================================================================================================
// Writes the commands required to update the sqtt token mask.
uint32* Gfx9ThreadTrace::WriteUpdateSqttTokenMaskCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace,
    uint32     sqttTokenMask
) const
{
    const auto& regInfo = m_device.CmdUtil().GetRegInfo();

    // Set GRBM_GFX_INDEX to isolate the SE/SH this trace is associated with.
    pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);

    // Update the token mask register
    regSQ_THREAD_TRACE_TOKEN_MASK__GFX09 tokenMaskReg = m_sqThreadTraceTokenMask;
    tokenMaskReg.bits.TOKEN_MASK = sqttTokenMask;
    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_TOKEN_MASK__GFX09,
                                                  tokenMaskReg.u32All,
                                                  pCmdSpace);

    // NOTE: It is the caller's responsibility to reset GRBM_GFX_INDEX.

    return pCmdSpace;
}

// =====================================================================================================================
// Issues the PM4 commands necessary to start this thread trace. The owning Experiment object should have issued and
// idle before calling this. Returns the next unused DWORD in pCmdSpace.
uint32* Gfx9ThreadTrace::WriteStartCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // Set GRBM_GFX_INDEX to isolate the SE/SH this trace is associated with.
    pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);

    // Write SQ_THREAD_TRACE_CTRL with the reset_buffer flag set to instruct the hardware to
    // reset the trace buffer.
    regSQ_THREAD_TRACE_CTRL__GFX09 sqThreadTraceCtrl = {};
    sqThreadTraceCtrl.bits.RESET_BUFFER = 1;

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_CTRL__GFX09,
                                                  sqThreadTraceCtrl.u32All,
                                                  pCmdSpace);

    // Write SQ_THREAD_TRACE_MODE with the mode field set to "on" to enable the trace.
    regSQ_THREAD_TRACE_MODE__GFX09 sqThreadTraceMode;
    sqThreadTraceMode.u32All    = m_sqThreadTraceMode.u32All;
    sqThreadTraceMode.bits.MODE = SQ_THREAD_TRACE_MODE_ON;

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_MODE__GFX09,
                                                  sqThreadTraceMode.u32All,
                                                  pCmdSpace);

    // NOTE: It is the caller's responsibility to reset GRBM_GFX_INDEX.

    return pCmdSpace;
}

// =====================================================================================================================
// Issues the PM4 commands necessary to stop this thread trace, and populate the parent experiment's GPU memory with the
// appropriate ThreadTraceInfoData contents. Returns the next unused DWORD in pCmdSpace.
uint32* Gfx9ThreadTrace::WriteStopCommands(
    gpusize    baseGpuVirtAddr, ///< Base GPU virtual address of the owning Experiment
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const auto& cmdUtil = m_device.CmdUtil();

    // Set GRBM_GFX_INDEX to isolate the SE/SH this trace is associated with.
    pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);

    // Write SQ_THREAD_TRACE_MODE with the mode field set to "off" to disable the trace.
    regSQ_THREAD_TRACE_MODE__GFX09 sqThreadTraceMode;
    sqThreadTraceMode.u32All    = m_sqThreadTraceMode.u32All;
    sqThreadTraceMode.bits.MODE = SQ_THREAD_TRACE_MODE_OFF;

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_MODE__GFX09,
                                                  sqThreadTraceMode.u32All,
                                                  pCmdSpace);

    // Flush the thread trace buffer to memory.
    pCmdSpace += cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_FLUSH__GFX09, pCmdStream->GetEngineType(), pCmdSpace);

    // Poll the status register's busy bit to ensure that no events are being logged and written to memory.
    pCmdSpace += cmdUtil.BuildWaitRegMem(mem_space__me_wait_reg_mem__register_space,
                                         function__me_wait_reg_mem__not_equal_reference_value,
                                         engine_sel__me_wait_reg_mem__micro_engine,
                                         mmSQ_THREAD_TRACE_STATUS__GFX09,
                                         0x1,
                                         SQ_THREAD_TRACE_STATUS__BUSY_MASK__GFX09,
                                         pCmdSpace);

    // The following code which issues the COPY_DATA commands assumes that the layout of the ThreadTraceInfoData
    // structure is ordered a particular way. Compile-time asserts help us help us guarantee the assumption.
    static_assert(offsetof(ThreadTraceInfoData, curOffset)    == 0, "");
    static_assert(offsetof(ThreadTraceInfoData, traceStatus)  == sizeof(uint32), "");
    static_assert(offsetof(ThreadTraceInfoData, writeCounter) == (sizeof(uint32) * 2), "");

    // Compute the base address of the thread trace info segment.
    const gpusize gpuVirtAddr = (baseGpuVirtAddr + m_infoOffset);

    // Issue a trio of COPY_DATA commands to populate the ThreadTraceInfoData for this
    // thread trace:

    const EngineType engineType = pCmdStream->GetEngineType();
    const uint32     data[]     = { mmSQ_THREAD_TRACE_WPTR__GFX09,
                                    mmSQ_THREAD_TRACE_STATUS__GFX09,
                                    mmSQ_THREAD_TRACE_CNTR__GFX09 };

    for (uint32 i = 0; i < (sizeof(data) / sizeof(data[0])); i++)
    {
        if (engineType == EngineTypeCompute)
        {
            pCmdSpace += cmdUtil.BuildCopyDataCompute(dst_sel__mec_copy_data__memory__GFX09,
                                                      gpuVirtAddr + i * sizeof(uint32),
                                                      src_sel__mec_copy_data__perfcounters,
                                                      data[i],
                                                      count_sel__mec_copy_data__32_bits_of_data,
                                                      wr_confirm__mec_copy_data__wait_for_confirmation,
                                                      pCmdSpace);
        }
        else
        {
            pCmdSpace += cmdUtil.BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
                                                       dst_sel__me_copy_data__memory__GFX09,
                                                       gpuVirtAddr + i * sizeof(uint32),
                                                       src_sel__me_copy_data__perfcounters,
                                                       data[i],
                                                       count_sel__me_copy_data__32_bits_of_data,
                                                       wr_confirm__me_copy_data__wait_for_confirmation,
                                                       pCmdSpace);
        }
    }
    // NOTE: It is the caller's responsibility to reset GRBM_GFX_INDEX.

    return pCmdSpace;
}

// =====================================================================================================================
// Validates the value of a thread-trace creation option.
Result Gfx9ThreadTrace::Init()
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();
    const auto&              flags     = m_info.optionFlags;
    const auto&              values    = m_info.optionValues;
    Result                   result    = ThreadTrace::Init();

    if ((result == Result::Success)  &&
        (flags.bufferSize)           &&
        ((values.bufferSize > PerfCtrInfo::MaximumBufferSize) ||
         (Util::Pow2Align(values.bufferSize, PerfCtrInfo::BufferAlignment) != values.bufferSize)))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success)  &&
        (flags.threadTraceTokenMask) &&
        ((values.threadTraceTokenMask & TokenMaskAll) != values.threadTraceTokenMask))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success) &&
        (flags.threadTraceRegMask)  &&
        ((values.threadTraceRegMask & RegMaskAll) != values.threadTraceRegMask))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success) &&
        (flags.threadTraceTargetSh) &&
        (values.threadTraceTargetSh >= chipProps.gfx9.numShaderArrays))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success) &&
        (flags.threadTraceTargetCu) &&
        (values.threadTraceTargetCu >= chipProps.gfx9.numCuPerSh))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success)         &&
        (flags.threadTraceSh0CounterMask)   &&
        ((values.threadTraceSh0CounterMask & ShCuMaskAll) != values.threadTraceSh0CounterMask))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success)         &&
        (flags.threadTraceSh1CounterMask)   &&
        ((values.threadTraceSh1CounterMask & ShCuMaskAll) != values.threadTraceSh1CounterMask))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success)   &&
        (flags.threadTraceSimdMask)   &&
        ((values.threadTraceSimdMask & SimdMaskAll) != values.threadTraceSimdMask))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success)   &&
        (flags.threadTraceVmIdMask)   &&
        (values.threadTraceVmIdMask > SQ_THREAD_TRACE_VM_ID_MASK_SINGLE_DETAIL))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success)         &&
        (flags.threadTraceShaderTypeMask)   &&
        ((values.threadTraceShaderTypeMask & PerfShaderMaskAll) != values.threadTraceShaderTypeMask))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success)   &&
        (flags.threadTraceIssueMask)  &&
        (values.threadTraceIssueMask > SQ_THREAD_TRACE_ISSUE_MASK_IMMED))
    {
        result = Result::ErrorInvalidValue;
    }

    return result;
}

} // Gfx9
} // Pal
