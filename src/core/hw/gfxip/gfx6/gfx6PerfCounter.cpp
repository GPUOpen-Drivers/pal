/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6PerfCounter.h"

#include "core/hw/amdgpu_asic.h"

using namespace Pal::Gfx6::PerfCtrInfo;

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
PerfCounter::PerfCounter(
    const Device&          device,
    const PerfCounterInfo& info,
    uint32                 slot)    // Counter slot to occupy
    :
    Pal::PerfCounter(device.Parent(), info, slot),
    m_device(device),
    m_numActiveRegs(1),
    m_perfCountLoAddr(0),
    m_perfCountHiAddr(0),
    m_perfCountSrcSel(COPY_DATA_SEL_REG)
{
    const CmdUtil&             cmdUtil    = device.CmdUtil();
    const GfxIpLevel           gfxIpLevel = m_device.Parent()->ChipProperties().gfxLevel;
    const Gfx6PerfCounterInfo& perfInfo   = m_device.Parent()->ChipProperties().gfx6.perfCounterInfo;

    // MC and DMA counters use 32bits per data sample. All other blocks use 64bits per sample.
    if ((m_info.block == GpuBlock::Mc) || (m_info.block == GpuBlock::Dma))
    {
        m_dataSize = sizeof(uint32);
    }
    else
    {
        m_dataSize = sizeof(uint64);
    }

    // Currently all performance counter options only apply to SQ counters.
    if (m_info.block == GpuBlock::Sq)
    {
        const auto& flags  = info.optionFlags;
        const auto& values = info.optionValues;

        uint32 simdMask   = (flags.sqSimdMask) ?
                            (values.sqSimdMask & DefaultSqSelectSimdMask) : DefaultSqSelectSimdMask;
        uint32 bankMask   = (flags.sqSqcBankMask) ?
                            (values.sqSqcBankMask & DefaultSqSelectBankMask) : DefaultSqSelectBankMask;
        uint32 clientMask = (flags.sqSqcClientMask) ?
                            (values.sqSqcClientMask & DefaultSqSelectClientMask) : DefaultSqSelectClientMask;

        // The SQ counter select register layout differs between Gfx6/Gfx7-8 families.
        if (gfxIpLevel == GfxIpLevel::GfxIp6)
        {
            regSQ_PERFCOUNTER0_SELECT__SI sqSelect = {};
            sqSelect.bits.PERF_SEL  = info.eventId;
            sqSelect.bits.SIMD_MASK = simdMask;
            m_selectReg[0]          = sqSelect.u32All;
        }
        else
        {
            regSQ_PERFCOUNTER0_SELECT__CI__VI sqSelect = {};
            sqSelect.bits.PERF_SEL        = info.eventId;
            sqSelect.bits.SIMD_MASK       = simdMask;
            sqSelect.bits.SQC_BANK_MASK   = bankMask;
            sqSelect.bits.SQC_CLIENT_MASK = clientMask;
            m_selectReg[0]                = sqSelect.u32All;
        }
    }
    else
    {
        // For all other blocks, the eventId is the value of the select register.
        m_selectReg[0] = info.eventId;
    }

    // Currently, select register #1 is unused.
    m_selectReg[1] = 0;

    // Initialize performance counter flags.
    m_flags.u32All = 0;
    switch (m_info.block)
    {
    // Each of the following blocks are indexed for reads and writes. Fall-throughs
    // are all intentional.
    case GpuBlock::Cb:
    case GpuBlock::Db:
    case GpuBlock::Pa:
    case GpuBlock::Sc:
    case GpuBlock::Sx:
    case GpuBlock::Spi:
    case GpuBlock::Sq:
    case GpuBlock::Ta:
    case GpuBlock::Td:
    case GpuBlock::Tcp:
    case GpuBlock::Tcc:
    case GpuBlock::Tca:
    case GpuBlock::Gds:
    case GpuBlock::Vgt:
        m_flags.isIndexed = 1;
        break;
    default:
        break;
    }

    // Setup the performance count registers to sample and the source-select for the Copy
    // Data PM4 commands issued when sampling the counter.
    const uint32 blockIdx = static_cast<uint32>(m_info.block);
    if (m_info.block == GpuBlock::Mc)
    {
        // NOTE: The MC doesn't follow the normal pattern for counters. The SE index is the MC channel. Also, the
        // perfCountLoAddr and perfCountHiAddr represent the 32bit values for the first and second channels
        // respectively, and not the low and high halves of a 64bit value as with the other blocks' perf counters.
        const uint32 seIndex = PerfCounter::InstanceIdToSe(
                                   perfInfo.block[static_cast<uint32>(m_info.block)].numInstances,
                                   m_device.Parent()->ChipProperties().gfx6.numShaderArrays,
                                   m_info.instance);

        m_perfCountLoAddr =  (seIndex == 0) ? perfInfo.block[blockIdx].regInfo[m_slot].perfCountLoAddr
                                              : perfInfo.block[blockIdx].regInfo[m_slot].perfCountHiAddr;
        m_perfCountSrcSel = COPY_DATA_SEL_SRC_SYS_PERF_COUNTER;
    }
    else if ((m_info.block == GpuBlock::Dma) && (gfxIpLevel != GfxIpLevel::GfxIp6))
    {
        // NOTE: DMA on Gfx7+ is the SDMA block (on Gfx6 it is DRMDMA). SDMA is also a 32bit counter. The Lo and Hi
        // register addresses represent counters 0 and 1, rather than the Lo/Hi portions of a single 64bit counter
        // like the other blocks.
        m_perfCountLoAddr = (m_slot == 0) ? perfInfo.block[blockIdx].regInfo[m_info.instance].perfCountLoAddr
                                          : perfInfo.block[blockIdx].regInfo[m_info.instance].perfCountHiAddr;
        m_perfCountSrcSel = COPY_DATA_SEL_SRC_SYS_PERF_COUNTER;
    }
    else
    {
        m_perfCountLoAddr = perfInfo.block[blockIdx].regInfo[m_slot].perfCountLoAddr;
        m_perfCountHiAddr = perfInfo.block[blockIdx].regInfo[m_slot].perfCountHiAddr;

        // NOTE: The GRBMSE block requires special handling: its counters are not indexed based on SE/SH/instance,
        // but actually occupy different physical registers.
        if ((m_info.block == GpuBlock::GrbmSe) && (m_info.instance != 0))
        {
            switch (m_info.instance)
            {
            case 1:
                if (gfxIpLevel == GfxIpLevel::GfxIp6)
                {
                    m_perfCountLoAddr = mmGRBM_SE1_PERFCOUNTER_LO__SI;
                    m_perfCountHiAddr = mmGRBM_SE1_PERFCOUNTER_HI__SI;
                }
                else
                {
                    m_perfCountLoAddr = mmGRBM_SE1_PERFCOUNTER_LO__CI__VI;
                    m_perfCountHiAddr = mmGRBM_SE1_PERFCOUNTER_HI__CI__VI;
                }
                break;
            case 2:
                m_perfCountLoAddr = mmGRBM_SE2_PERFCOUNTER_LO__CI__VI;
                m_perfCountHiAddr = mmGRBM_SE2_PERFCOUNTER_HI__CI__VI;
                break;
            case 3:
                m_perfCountLoAddr = mmGRBM_SE3_PERFCOUNTER_LO__CI__VI;
                m_perfCountHiAddr = mmGRBM_SE3_PERFCOUNTER_HI__CI__VI;
                break;
            default:
                PAL_NEVER_CALLED();
                break;
            }
        }

        // NOTE: Need to use a different source select for privileged registers on Gfx7.
        //       (Need to check if this is also true for Gfx8.)
        if ((gfxIpLevel != GfxIpLevel::GfxIp6) &&
            ((cmdUtil.IsPrivilegedConfigReg(m_perfCountLoAddr)) ||
             (cmdUtil.IsPrivilegedConfigReg(m_perfCountHiAddr))))
        {
            m_perfCountSrcSel = COPY_DATA_SEL_SRC_SYS_PERF_COUNTER;
        }
    }
}

// =====================================================================================================================
// Accumulates the values of the MC counter setup registers across multiple counters.
void PerfCounter::SetupMcSeqRegisters(
    regMC_SEQ_PERF_SEQ_CTL* pMcSeqPerfCtl,  // MC_SEQ_PERF_SEQ_CTL register value
    regMC_SEQ_PERF_CNTL_1*  pMcSeqPerfCtl1  // MC_SEQ_PERF_CNTL_1 register value
    ) const
{
    PAL_ASSERT(m_info.block == GpuBlock::Mc);

    // For the MC block, the "SE" corresponds to the MC channel.
    const Gfx6PerfCounterInfo& perfInfo = m_device.Parent()->ChipProperties().gfx6.perfCounterInfo;
    const uint32 numShaderArrays        = m_device.Parent()->ChipProperties().gfx6.numShaderArrays;
    const uint32 numInstances           = perfInfo.block[static_cast<uint32>(m_info.block)].numInstances;

    const uint32 channelId = PerfCounter::InstanceIdToSe(numInstances, numShaderArrays, m_info.instance);

    PAL_ASSERT(channelId < NumMcChannels);

    // NOTE: The event select fields of the MC registers in MC_SEQ_PERF_SEQ_CTL only have four bits per event. However,
    //       the eventId for the MC block uses up to five bits, so the MSB's for each event are stored in the
    //       MC_SEQ_PERF_CNTL_1 register.
    constexpr uint32 McEventMask = 0xF;

    const uint32 eventSel    = (m_info.eventId & McEventMask);
    const uint32 eventSelMsb = (m_info.eventId > McEventMask) ? 1 : 0;

    switch (m_slot)
    {
    case 0:
        if (channelId == 0)
        {
            pMcSeqPerfCtl->bits.SEL_A      = eventSel;
            pMcSeqPerfCtl1->bits.SEL_A_MSB = eventSelMsb;
        }
        else if (channelId == 1)
        {
            pMcSeqPerfCtl->bits.SEL_CH1_A      = eventSel;
            pMcSeqPerfCtl1->bits.SEL_CH1_A_MSB = eventSelMsb;
        }
        break;
    case 1:
        if (channelId == 0)
        {
            pMcSeqPerfCtl->bits.SEL_B      = eventSel;
            pMcSeqPerfCtl1->bits.SEL_B_MSB = eventSelMsb;
        }
        else if (channelId == 1)
        {
            pMcSeqPerfCtl->bits.SEL_CH1_B      = eventSel;
            pMcSeqPerfCtl1->bits.SEL_CH1_B_MSB = eventSelMsb;
        }
        break;
    case 2:
        if (channelId == 0)
        {
            pMcSeqPerfCtl->bits.SEL_CH0_C      = eventSel;
            pMcSeqPerfCtl1->bits.SEL_CH0_C_MSB = eventSelMsb;
        }
        else if (channelId == 1)
        {
            pMcSeqPerfCtl->bits.SEL_CH1_C      = eventSel;
            pMcSeqPerfCtl1->bits.SEL_CH1_C_MSB = eventSelMsb;
        }
        break;
    case 3:
        if (channelId == 0)
        {
            pMcSeqPerfCtl->bits.SEL_CH0_D      = eventSel;
            pMcSeqPerfCtl1->bits.SEL_CH0_D_MSB = eventSelMsb;
        }
        else if (channelId == 1)
        {
            pMcSeqPerfCtl->bits.SEL_CH1_D      = eventSel;
            pMcSeqPerfCtl1->bits.SEL_CH1_D_MSB = eventSelMsb;
        }
        break;
    default:
        PAL_NEVER_CALLED();
        break;
    }
}

// =====================================================================================================================
// Accumulates the values of the SDMA counter setup registers across multiple counters.
uint32 PerfCounter::SetupSdmaSelectReg(
    regSDMA0_PERFMON_CNTL__CI__VI* pSdma0PerfmonCntl, // SDMA0_PERFMON_CNTL reg value
    regSDMA1_PERFMON_CNTL__CI__VI* pSdma1PerfmonCntl  // SDMA1_PERFMON_CNTL reg value
    ) const
{
    PAL_ASSERT((m_info.block == GpuBlock::Dma) &&
               (m_device.Parent()->ChipProperties().gfxLevel != GfxIpLevel::GfxIp6));

    uint32 regValue = 0;

    if (m_info.instance == 0)
    {
        if (m_slot == 0)
        {
            pSdma0PerfmonCntl->bits.PERF_SEL0    = m_info.eventId;
            pSdma0PerfmonCntl->bits.PERF_ENABLE0 = 1;
        }
        else if (m_slot == 1)
        {
            pSdma0PerfmonCntl->bits.PERF_SEL1    = m_info.eventId;
            pSdma0PerfmonCntl->bits.PERF_ENABLE1 = 1;
        }

        regValue = pSdma0PerfmonCntl->u32All;
    }
    else if (m_info.instance == 1)
    {
        if (m_slot == 0)
        {
            pSdma1PerfmonCntl->bits.PERF_SEL0    = m_info.eventId;
            pSdma1PerfmonCntl->bits.PERF_ENABLE0 = 1;
        }
        else if (m_slot == 1)
        {
            pSdma1PerfmonCntl->bits.PERF_SEL1    = m_info.eventId;
            pSdma1PerfmonCntl->bits.PERF_ENABLE1 = 1;
        }

        regValue = pSdma1PerfmonCntl->u32All;
    }

    return regValue;
}

// =====================================================================================================================
// Counters associated with indexed GPU blocks need to write GRBM_GFX_INDEX to mask-off the SE/SH/Instance the counter
// is sampling from. This issues the PM4 command which sets up GRBM_GFX_INDEX appropriately. Returns the next unused
// DWORD in pCmdSpace.
uint32* PerfCounter::WriteGrbmGfxIndex(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    if (IsIndexed())
    {
        const Gfx6PerfCounterInfo& perfInfo = m_device.Parent()->ChipProperties().gfx6.perfCounterInfo;
        const uint32 numShaderArrays        = m_device.Parent()->ChipProperties().gfx6.numShaderArrays;
        const uint32 numInstances           = perfInfo.block[static_cast<uint32>(m_info.block)].numInstances;

        regGRBM_GFX_INDEX grbmGfxIndex   = {};
        grbmGfxIndex.bits.SE_INDEX       = PerfCounter::InstanceIdToSe(numInstances, numShaderArrays, m_info.instance);
        grbmGfxIndex.bits.SH_INDEX       = PerfCounter::InstanceIdToSh(numInstances, numShaderArrays, m_info.instance);
        grbmGfxIndex.bits.INSTANCE_INDEX = PerfCounter::InstanceIdToInstance(numInstances, m_info.instance);

        pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                                     grbmGfxIndex.u32All,
                                                     pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Broadcast sampling info to all Instance/SH inside a SE.
// This issues the PM4 command which sets up GRBM_GFX_INDEX appropriately. Returns the next unused DWORD in pCmdSpace.
uint32* PerfCounter::WriteGrbmGfxBroadcastSe(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    if (IsIndexed())
    {
        const Gfx6PerfCounterInfo& perfInfo = m_device.Parent()->ChipProperties().gfx6.perfCounterInfo;
        const uint32 numShaderArrays        = m_device.Parent()->ChipProperties().gfx6.numShaderArrays;
        const uint32 numInstances           = perfInfo.block[static_cast<uint32>(m_info.block)].numInstances;

        regGRBM_GFX_INDEX grbmGfxIndex = {};
        grbmGfxIndex.bits.SE_INDEX     = PerfCounter::InstanceIdToSe(numInstances, numShaderArrays, m_info.instance);

        grbmGfxIndex.bits.SH_BROADCAST_WRITES       = 1;
        grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

        pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                                     grbmGfxIndex.u32All,
                                                     pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Issues the PM4 commands necessary to setup this counter. Returns the next unused DWORD in pCmdSpace.
uint32* PerfCounter::WriteSetupCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const auto&  chipProps    = m_device.Parent()->ChipProperties();
    const auto&  perfInfo     = chipProps.gfx6.perfCounterInfo;

    const uint32 blockIdx     = static_cast<uint32>(m_info.block);
    const uint32 primaryReg   = perfInfo.block[blockIdx].regInfo[m_slot].perfSel0RegAddr;
    const uint32 secondaryReg = perfInfo.block[blockIdx].regInfo[m_slot].perfSel1RegAddr;

    if ((m_info.block == GpuBlock::Mc) ||
        ((m_info.block == GpuBlock::Dma) && (chipProps.gfxLevel == GfxIpLevel::GfxIp7)))
    {
        // NOTE: MC and SDMA blocks are handled outside of this function because multiple counters' state are all
        //       packed into the same registers.
        PAL_NEVER_CALLED();
    }
    else if ((m_info.block == GpuBlock::Srbm) && (chipProps.gfxLevel == GfxIpLevel::GfxIp6))
    {
        // SRBM performance counters need to use a COPY_DATA command.
        pCmdSpace += m_device.CmdUtil().BuildCopyData(COPY_DATA_SEL_REG,
                                                      primaryReg,
                                                      COPY_DATA_SEL_SRC_IMME_DATA,
                                                      m_selectReg[0],
                                                      COPY_DATA_SEL_COUNT_1DW,
                                                      COPY_DATA_ENGINE_ME,
                                                      COPY_DATA_WR_CONFIRM_NO_WAIT,
                                                      pCmdSpace);
    }
    else if (m_info.block == GpuBlock::GrbmSe)
    {
        // NOTE: Special handling needed for GRBMSE: the select register addresses for the second instance is stored
        //       in perfSel1RegAddr.
        const uint32 regAddress = (InstanceIdToInstance(perfInfo.block[blockIdx].numInstances,m_info.instance) == 0) ?
                                   primaryReg : secondaryReg;

        pCmdSpace = pCmdStream->WriteSetOneConfigReg(regAddress, m_selectReg[0], pCmdSpace);
    }
    else if (m_info.block == GpuBlock::Sq)
    {
        pCmdSpace = WriteGrbmGfxBroadcastSe(pCmdStream, pCmdSpace);

        // Always write primary select register.
        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(primaryReg, m_selectReg[0], pCmdSpace);

        // Only write the secondary select register if necessary.
        if (m_numActiveRegs > 1)
        {
            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(secondaryReg, m_selectReg[1], pCmdSpace);
        }
    }
    else
    {
        pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);

        // Always write primary select register.
        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(primaryReg, m_selectReg[0], pCmdSpace);

        // Only write the secondary select register if necessary.
        if (m_numActiveRegs > 1)
        {
            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(secondaryReg, m_selectReg[1], pCmdSpace);
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Issues the PM4 commands necessary to sample the value of this counter. Returns the next unused DWORD in pCmdSpace.
uint32* PerfCounter::WriteSampleCommands(
    gpusize    baseGpuVirtAddr, // GPU virtual addr where performance counter samples begin
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const auto& chipProps = m_device.Parent()->ChipProperties();
    const auto& perfInfo  = chipProps.gfx6.perfCounterInfo;
    const auto& cmdUtil   = m_device.CmdUtil();

    // NOTE: SQ reads can time out and fail if the performance result read occurs too shortly after the sample. The
    //       workaround for this is to set the read timeout duration to its maximum value while we sample the counter.
    //       This is acceptable since we know the data is going to be written and we're willing to wait for it.
    //       Unfortunately, we cannot write GRBM_CNTL on Gfx7, but the problem still exists on those chips. DXX doesn't
    //       have a solution for this, either.
    if ((m_info.block == GpuBlock::Sq) && (chipProps.gfxLevel == GfxIpLevel::GfxIp6))
    {
        regGRBM_CNTL grbmCntlWait = {};
        grbmCntlWait.bits.READ_TIMEOUT = 0xFF;

        pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmGRBM_CNTL, grbmCntlWait.u32All, pCmdSpace);
    }

    if (m_info.block == GpuBlock::Mc)
    {
        uint32 mcd = InstanceIdToInstance(perfInfo.block[static_cast<uint32>(m_info.block)].numInstances,
                                          m_info.instance);

        if (IsTonga(*m_device.Parent()) &&
            (chipProps.gfx6.numMcdTiles == 4))
        {
            // The four MCD Tonga uses MCDs 0, 2, 3, and 5.
            // So we must map the instance in the [0123] range to MCD [0235].
            constexpr uint32 InstanceToMcdMap[] = {0, 2, 3, 5};
            mcd = InstanceToMcdMap[mcd];
        }

        // MC counters need an extra register write when sampling the counters.
        const uint32 mcRegValue = ((mcd << perfInfo.mcReadEnableShift) | perfInfo.mcWriteEnableMask);

        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(perfInfo.mcConfigRegAddress, mcRegValue, pCmdSpace);
    }

    pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);

    const gpusize gpuVirtAddr = (baseGpuVirtAddr + GetDataOffset());

    // Write low 32bit portion of performance counter sample to the GPU virtual address.
    pCmdSpace += cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_ASYNC_MEMORY,
                                       gpuVirtAddr,
                                       m_perfCountSrcSel,
                                       m_perfCountLoAddr,
                                       COPY_DATA_SEL_COUNT_1DW,
                                       COPY_DATA_ENGINE_ME,
                                       COPY_DATA_WR_CONFIRM_WAIT,
                                       pCmdSpace);

    // Write high 32bit portion of performance counter sample to the GPU virtual address, if the
    // block uses 64bit counters.
    if (GetSampleSize() == sizeof(uint64))
    {
        pCmdSpace += cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_ASYNC_MEMORY,
                                           (gpuVirtAddr + sizeof(uint32)),
                                           m_perfCountSrcSel,
                                           m_perfCountHiAddr,
                                           COPY_DATA_SEL_COUNT_1DW,
                                           COPY_DATA_ENGINE_ME,
                                           COPY_DATA_WR_CONFIRM_WAIT,
                                           pCmdSpace);
    }

    // Restore the default value of GRBM_CNTL if we changed it earlier.
    if ((m_info.block == GpuBlock::Sq) && (chipProps.gfxLevel == GfxIpLevel::GfxIp6))
    {
        regGRBM_CNTL grbmCntlDefault = {};
        grbmCntlDefault.bits.READ_TIMEOUT = 0x18;

        pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmGRBM_CNTL, grbmCntlDefault.u32All, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
StreamingPerfCounter::StreamingPerfCounter(
    const Device& device,
    GpuBlock      block,
    uint32        instance,
    uint32        slot)
    :
    Pal::StreamingPerfCounter(device.Parent(), block, instance, slot),
    m_device(device)
{
    // Initialize parent's performance counter flags.
    m_flags.u16All = 0;
    switch (block)
    {
    // Each of the following blocks are indexed for reads and writes. Fall-throughs
    // are all intentional.
    case GpuBlock::Cb:
    case GpuBlock::Db:
    case GpuBlock::Pa:
    case GpuBlock::Sc:
    case GpuBlock::Sx:
    case GpuBlock::Spi:
    case GpuBlock::Sq:
    case GpuBlock::Ta:
    case GpuBlock::Td:
    case GpuBlock::Tcp:
    case GpuBlock::Tcc:
    case GpuBlock::Tca:
    case GpuBlock::Gds:
    case GpuBlock::Vgt:
        m_flags.isIndexed = 1;
        break;
    default:
        break;
    }

    switch (block)
    {
    case GpuBlock::Cpg:
    case GpuBlock::Cpc:
    case GpuBlock::Cpf:
    case GpuBlock::Gds:
    case GpuBlock::Tcc:
    case GpuBlock::Tca:
    case GpuBlock::Ia:
    case GpuBlock::Tcs:
    case GpuBlock::Ea:
        m_flags.isGlobalBlock = 1;
        break;
    default:
        break;
    }

    if (m_flags.isGlobalBlock)
    {
        m_segmentType = SpmDataSegmentType::Global;
    }
    else
    {
        auto const& gfx6ChipProps = m_device.Parent()->ChipProperties().gfx6;
        const uint32 numInstances = gfx6ChipProps.perfCounterInfo.block[static_cast<uint32>(m_block)].numInstances;

        m_segmentType = static_cast<SpmDataSegmentType>(PerfCounter::InstanceIdToSe(numInstances,
                                                                                    gfx6ChipProps.numShaderArrays,
                                                                                    m_instance));
    }

    PAL_ASSERT(m_segmentType < SpmDataSegmentType::Count);
}

// =====================================================================================================================
// Adds an event to this StreamingPerfCounter. One StreamingPerfCounter can support upto 4x16-bit streaming counters.
// Fails if the number of sub-slots is maxed out in this HW counter.
Result StreamingPerfCounter::AddEvent(
    const GpuBlock& block,
    uint32          eventId)
{
    Result result = Result::ErrorOutOfGpuMemory; // Assume that all sub-slots are used.

    if (block == GpuBlock::Sq)
    {
        // For SQ, each of the 16 64-bit summary counters can support only one 16-bit streaming counter.
        if (m_eventId[0] == InvalidEventId)
        {
            m_eventId[0] = eventId;
            result       = Result::Success;
        }
    }
    else
    {
        for (uint32 i = 0; i < MaxNumStreamingCtrPerSummaryCtr; ++i)
        {
            // Check for a free streaming counter slot.
            if (m_eventId[i] == InvalidEventId)
            {
                m_eventId[i] = eventId;
                result       = Result::Success;
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Returns true if any of the events governing perfcounter_select0 register is valid.
bool StreamingPerfCounter::IsSelect0RegisterValid() const
{
    // SQ counters have only one event id per StreamingPerfCounter.
    return ((m_eventId[0] != StreamingPerfCounter::InvalidEventId) ||
            ((m_eventId[1] != StreamingPerfCounter::InvalidEventId) && (m_block != GpuBlock::Sq)));
}

// =====================================================================================================================
// Returns true if any of the events governing perfcounter_select1 field is valid.
bool StreamingPerfCounter::IsSelect1RegisterValid() const
{
    // SQ counters don't have a select1 register.
    PAL_ASSERT(m_block != GpuBlock::Sq);

    return ((m_eventId[2] != StreamingPerfCounter::InvalidEventId) ||
            (m_eventId[3] != StreamingPerfCounter::InvalidEventId));
}

// =====================================================================================================================
// Writes commands necessary to enable this perf counter. This is specific to the gfx6 HW layer.
uint32* StreamingPerfCounter::WriteSetupCommands(
    Pal::CmdStream* pCmdStream,
    uint32*         pCmdSpace)
{
    CmdStream* pHwlCmdStream = static_cast<CmdStream*>(pCmdStream);

    const auto& perfInfo      = m_device.Parent()->ChipProperties().gfx6.perfCounterInfo;
    const uint32 blockIdx     = static_cast<uint32>(m_block);
    const uint32 primaryReg   = perfInfo.block[blockIdx].regInfo[m_slot].perfSel0RegAddr;
    const uint32 secondaryReg = perfInfo.block[blockIdx].regInfo[m_slot].perfSel1RegAddr;

    const uint32 numInstances    = perfInfo.block[blockIdx].numInstances;
    const uint32 numShaderArrays = m_device.Parent()->ChipProperties().gfx6.numShaderArrays;

    // If this is an indexed counter, we need to modify the GRBM_GFX_INDEX.
    if (m_flags.isIndexed)
    {
        regGRBM_GFX_INDEX grbmGfxIndex   = {};
        grbmGfxIndex.bits.SE_INDEX       = PerfCounter::InstanceIdToSe(numInstances, numShaderArrays, m_instance);
        grbmGfxIndex.bits.SH_INDEX       = PerfCounter::InstanceIdToSh(numInstances, numShaderArrays, m_instance);
        grbmGfxIndex.bits.INSTANCE_INDEX = PerfCounter::InstanceIdToInstance(numInstances, m_instance);

        pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                                        grbmGfxIndex.u32All,
                                                        pCmdSpace);
    }

    // Write the PERFCOUNTERx_SELECT register corresponding to valid eventIds.
    if (IsSelect0RegisterValid())
    {
        pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(primaryReg, GetSelect0RegisterData(), pCmdSpace);
    }

    // Gfx6 SQ blocks have only one SELECT register.
    if (m_block != GpuBlock::Sq)
    {
        if (IsSelect1RegisterValid())
        {
            pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(secondaryReg, GetSelect1RegisterData(), pCmdSpace);
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Returns gfx6 hw specific muxselect encoding
uint16 StreamingPerfCounter::GetMuxselEncoding(
    uint32 subSlot) const
{
    MuxselEncoding muxselEncoding;
    muxselEncoding.u16All = 0;

    auto const& gfx6ChipProps = m_device.Parent()->ChipProperties().gfx6;
    const uint32 numInstances = gfx6ChipProps.perfCounterInfo.block[static_cast<uint32>(m_block)].numInstances;

    muxselEncoding.counter  = subSlot;
    muxselEncoding.instance = PerfCounter::InstanceIdToInstance(numInstances, m_instance);
    muxselEncoding.block    = gfx6ChipProps.perfCounterInfo.block[static_cast<uint32>(m_block)].spmBlockSelectCode;

    return muxselEncoding.u16All;
}

// =====================================================================================================================
uint32 StreamingPerfCounter::GetSelect0RegisterData() const
{
    // All blocks with streaming support except SQ are of the following format
    // PERF_SEL0 - 9:0
    // PERF_SEL1 - 19:10
    // CNTR_MODE - 23:20

    uint32 selectReg = 0;

    // PERF_SEL field of perfcounterx_select register.
    if (m_eventId[0] != InvalidEventId)
    {
        selectReg |= (m_eventId[0] << Gfx6PerfCounterPerfSel0Shift);
    }

    // PERF_SEL1 field for perfcounterx_select register. SQ perfcounterx_select registers don't have a PERF_SEL1 field.
    if ((m_eventId[1] != InvalidEventId) && (m_block != GpuBlock::Sq))
    {
        selectReg |= (m_eventId[1] << Gfx6PerfCounterPerfSel1Shift);
    }

    // The CNTR_MODE is set to clamp for now.
    selectReg |= (1 << Gfx6PerfCounterCntrModeShift);

    return selectReg;
}

// =====================================================================================================================
uint32 StreamingPerfCounter::GetSelect1RegisterData() const
{
    uint32 select1Reg = 0;

    // All blocks with streaming support except SQ are of the following format
    // PERF_SEL0 - 9:0
    // PERF_SEL1 - 19:10

    // Some blocks have more options in the higher bits. Support for these may be added later.

    // SQ counters don't have a select1 reg.
    PAL_ASSERT(m_block != GpuBlock::Sq);

    // PERF_SEL0 field
    if (m_eventId[2] != InvalidEventId)
    {
        select1Reg |= (m_eventId[2] << Gfx6PerfCounterPerfSel0Shift);
    }

    // PERF_SEL1 field
    if (m_eventId[3] != InvalidEventId)
    {
        select1Reg |= (m_eventId[3] << Gfx6PerfCounterPerfSel1Shift);
    }

    return select1Reg;
}

} // Gfx6
} // Pal
