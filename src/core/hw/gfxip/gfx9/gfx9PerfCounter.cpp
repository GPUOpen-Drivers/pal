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

#include "core/hw/amdgpu_asic.h"

using namespace Pal::Gfx9::PerfCtrInfo;

namespace Pal
{
namespace Gfx9
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
    m_mePerfCntSrcSel(src_sel__me_copy_data__mem_mapped_register),
    m_mecPerfCntSrcSel(src_sel__mec_copy_data__mem_mapped_register)
{
    const CmdUtil&             cmdUtil    = device.CmdUtil();
    const Gfx9PerfCounterInfo& perfInfo   = m_device.Parent()->ChipProperties().gfx9.perfCounterInfo;

    // SDMA counters use 32bits per data sample. All other blocks use 64bits per sample.
    if (m_info.block == GpuBlock::Dma)
    {
        // DMA counters use 32bits per data sample.
        m_dataSize = sizeof(uint32);
    }
    else
    {
        // All other blocks use 64bits per sample.
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

        regSQ_PERFCOUNTER0_SELECT sqSelect = {};
        sqSelect.bits.PERF_SEL               = info.eventId;
        sqSelect.bits.SIMD_MASK__GFX09       = simdMask;
        sqSelect.bits.SQC_BANK_MASK          = bankMask;
        sqSelect.bits.SQC_CLIENT_MASK__GFX09 = clientMask;
        m_selectReg[0]                       = sqSelect.u32All;
    }
    else if (m_info.block == GpuBlock::Ea)
    {
        regGCEA_PERFCOUNTER0_CFG  gceaPerfCntrCfg  = {};

        gceaPerfCntrCfg.bits.PERF_SEL  = info.eventId;
        gceaPerfCntrCfg.bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;
        gceaPerfCntrCfg.bits.ENABLE    = 1;
        m_selectReg[0]                 = gceaPerfCntrCfg.u32All;

        regGCEA_PERFCOUNTER_RSLT_CNTL gceaPerfCntrResultCntl = {};
        gceaPerfCntrResultCntl.bits.PERF_COUNTER_SELECT = slot; // info.instance;
        m_rsltCntlReg = gceaPerfCntrResultCntl.u32All;
    }
    else if (m_info.block == GpuBlock::Atc)
    {
        regATC_PERFCOUNTER0_CFG  atcPerfCntrCfg  = {};

        atcPerfCntrCfg.bits.PERF_SEL  = info.eventId;
        atcPerfCntrCfg.bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;
        atcPerfCntrCfg.bits.ENABLE    = 1;
        m_selectReg[0]                = atcPerfCntrCfg.u32All;

        regATC_PERFCOUNTER_RSLT_CNTL atcPerfCntrResultCntl = {};
        atcPerfCntrResultCntl.bits.PERF_COUNTER_SELECT = slot; // info.instance;
        m_rsltCntlReg = atcPerfCntrResultCntl.u32All;
    }
    else if (m_info.block == GpuBlock::AtcL2)
    {
        regATC_L2_PERFCOUNTER0_CFG__GFX09  atcL2PerfCntrCfg  = {};

        atcL2PerfCntrCfg.bits.PERF_SEL  = info.eventId;
        atcL2PerfCntrCfg.bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;
        atcL2PerfCntrCfg.bits.ENABLE    = 1;
        m_selectReg[0]                  = atcL2PerfCntrCfg.u32All;

        regATC_L2_PERFCOUNTER_RSLT_CNTL__GFX09 atcL2PerfCntrResultCntl = {};
        atcL2PerfCntrResultCntl.bits.PERF_COUNTER_SELECT = slot; // info.instance;
        m_rsltCntlReg = atcL2PerfCntrResultCntl.u32All;
    }
    else if (m_info.block == GpuBlock::McVmL2)
    {
        regMC_VM_L2_PERFCOUNTER0_CFG__GFX09  mcVmL2PerfCntrCfg  = {};

        mcVmL2PerfCntrCfg.bits.PERF_SEL  = info.eventId;
        mcVmL2PerfCntrCfg.bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;
        mcVmL2PerfCntrCfg.bits.ENABLE    = 1;
        m_selectReg[0]                   = mcVmL2PerfCntrCfg.u32All;

        regMC_VM_L2_PERFCOUNTER_RSLT_CNTL__GFX09 mcVmL2PerfCntrResultCntl = {};
        mcVmL2PerfCntrResultCntl.bits.PERF_COUNTER_SELECT = slot; // info.instance;
        m_rsltCntlReg = mcVmL2PerfCntrResultCntl.u32All;
    }
    else if (m_info.block == GpuBlock::Rpb)
    {
        regRPB_PERFCOUNTER0_CFG  rpbPerfCntrCfg  = {};

        rpbPerfCntrCfg.bits.PERF_SEL  = info.eventId;
        rpbPerfCntrCfg.bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;
        rpbPerfCntrCfg.bits.ENABLE    = 1;
        m_selectReg[0]                 = rpbPerfCntrCfg.u32All;

        regRPB_PERFCOUNTER_RSLT_CNTL rpbPerfCntrResultCntl = {};
        rpbPerfCntrResultCntl.bits.PERF_COUNTER_SELECT = slot; // info.instance;
        m_rsltCntlReg = rpbPerfCntrResultCntl.u32All;
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
    case GpuBlock::Ea:
    case GpuBlock::Rmi:
        m_flags.isIndexed = 1;
        break;
    default:
        break;
    }

    // Setup the performance count registers to sample and the source-select for the Copy
    // Data PM4 commands issued when sampling the counter.
    const uint32 blockIdx = static_cast<uint32>(m_info.block);
    if (m_info.block == GpuBlock::Dma)
    {
        // NOTE: SDMA is a 32bit counter. The Lo and Hi register addresses represent counters 0 and 1, rather
        //       than the Lo/Hi portions of a single 64bit counter like the other blocks.
        m_perfCountLoAddr  = (m_slot == 0) ? perfInfo.block[blockIdx].regInfo[m_info.instance].perfCountLoAddr
                                           : perfInfo.block[blockIdx].regInfo[m_info.instance].perfCountHiAddr;
        m_mePerfCntSrcSel  = src_sel__me_copy_data__perfcounters;
        m_mecPerfCntSrcSel = src_sel__mec_copy_data__perfcounters;
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
                m_perfCountLoAddr = mmGRBM_SE1_PERFCOUNTER_LO;
                m_perfCountHiAddr = mmGRBM_SE1_PERFCOUNTER_HI;
                break;
            case 2:
                m_perfCountLoAddr = mmGRBM_SE2_PERFCOUNTER_LO;
                m_perfCountHiAddr = mmGRBM_SE2_PERFCOUNTER_HI;
                break;
            case 3:
                m_perfCountLoAddr = mmGRBM_SE3_PERFCOUNTER_LO;
                m_perfCountHiAddr = mmGRBM_SE3_PERFCOUNTER_HI;
                break;
            default:
                PAL_NEVER_CALLED();
                break;
            }
        }

        // NOTE: Need to use a different source select for privileged registers
        if ((cmdUtil.IsPrivilegedConfigReg(m_perfCountLoAddr)) ||
            (cmdUtil.IsPrivilegedConfigReg(m_perfCountHiAddr)))
        {
            m_mePerfCntSrcSel  = src_sel__me_copy_data__perfcounters;
            m_mecPerfCntSrcSel = src_sel__mec_copy_data__perfcounters;
        }
    }
}

// =====================================================================================================================
// Compute the Shader Engine associated with this counter's global instance ID.
uint32 PerfCounter::InstanceIdToSe(
    const Gfx9PerfCounterInfo& perfInfo,
    const GpuBlock&            block,
    uint32                     instance)
{
    const uint32 blockNum = static_cast<uint32>(block);

    // SE is the truncated result of dividing our instanceId by the total instances per SE.
    const uint32 instancesPerEngine = (perfInfo.block[blockNum].numInstances *
                                       perfInfo.block[blockNum].numShaderArrays);

    return instance / instancesPerEngine;
}

// =====================================================================================================================
// Compute the Shader Array associated with this counter's global instance ID.
uint32 PerfCounter::InstanceIdToSh() const
{
    const auto& gfx9Info      = m_device.Parent()->ChipProperties().gfx9;
    const auto& perfBlockInfo = gfx9Info.perfCounterInfo.block[static_cast<uint32>(m_info.block)];

    // Compute the total shader arrays in this instanceId.
    const uint32 arraysInInstanceId = (m_info.instance / perfBlockInfo.numInstances);

    // SH is  the modulus of the total arrays in our instanceId and number of arrays per SE.
    const uint32 shIdx =  arraysInInstanceId % perfBlockInfo.numShaderArrays;

    PAL_ASSERT(shIdx < gfx9Info.numShaderArrays);

    return shIdx;
}

// =====================================================================================================================
// Compute the Instance Index associated with this counter's global instance ID.
uint32 PerfCounter::InstanceIdToInstance() const
{
    const auto& gfx9Info      = m_device.Parent()->ChipProperties().gfx9;
    const auto& perfBlockInfo = gfx9Info.perfCounterInfo.block[static_cast<uint32>(m_info.block)];

    // 'Local' instance index is the modulus of the global instance index and the number of instances per shader array.
    return (m_info.instance % perfBlockInfo.numInstances);
}

// =====================================================================================================================
// Accumulates the values of the SDMA counter setup registers across multiple counters.
uint32 PerfCounter::SetupSdmaSelectReg(
    regSDMA0_PERFMON_CNTL* pSdma0PerfmonCntl, // SDMA0_PERFMON_CNTL reg value
    regSDMA1_PERFMON_CNTL* pSdma1PerfmonCntl  // SDMA1_PERFMON_CNTL reg value
    ) const
{
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

        const uint32 seIndex = PerfCounter::InstanceIdToSe(
                                               m_device.Parent()->ChipProperties().gfx9.perfCounterInfo,
                                               m_info.block,
                                               m_info.instance);
        PAL_ASSERT(seIndex < m_device.Parent()->ChipProperties().gfx9.numShaderEngines);

        regGRBM_GFX_INDEX__GFX09 grbmGfxIndex = {};
        grbmGfxIndex.bits.SE_INDEX       = seIndex;
        grbmGfxIndex.bits.SH_INDEX       = InstanceIdToSh();
        grbmGfxIndex.bits.INSTANCE_INDEX = InstanceIdToInstance();

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

        const uint32 seIndex = PerfCounter::InstanceIdToSe(m_device.Parent()->ChipProperties().gfx9.perfCounterInfo,
                                                           m_info.block,
                                                           m_info.instance);
        PAL_ASSERT(seIndex < m_device.Parent()->ChipProperties().gfx9.numShaderEngines);

        regGRBM_GFX_INDEX__GFX09 grbmGfxIndex = {};
        grbmGfxIndex.bits.SE_INDEX                  = seIndex;
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
    // NOTE: SDMA block is handled outside of this function because multiple counters' state are all
    //       packed into the same registers.
    PAL_ASSERT(m_info.block != GpuBlock::Dma);

    const auto&  chipProps    = m_device.Parent()->ChipProperties();
    const auto&  perfInfo     = chipProps.gfx9.perfCounterInfo;

    const uint32 blockIdx     = static_cast<uint32>(m_info.block);
    const uint32 primaryReg   = perfInfo.block[blockIdx].regInfo[m_slot].perfSel0RegAddr;
    const uint32 secondaryReg = perfInfo.block[blockIdx].regInfo[m_slot].perfSel1RegAddr;

    if (m_info.block == GpuBlock::Sq)
    {
        pCmdSpace = WriteGrbmGfxBroadcastSe(pCmdStream, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);
    }

    // Always write primary select register.
    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(primaryReg, m_selectReg[0], pCmdSpace);

    // Only write the secondary select register if necessary.
    if (m_numActiveRegs > 1)
    {
        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(secondaryReg, m_selectReg[1], pCmdSpace);
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
    const auto& cmdUtil   = m_device.CmdUtil();
    const auto& chipProps = m_device.Parent()->ChipProperties();
    const auto& perfInfo  = chipProps.gfx9.perfCounterInfo;

    pCmdSpace = WriteGrbmGfxIndex(pCmdStream, pCmdSpace);

    if ((m_info.block == GpuBlock::Atc)    ||
        (m_info.block == GpuBlock::AtcL2)  ||
        (m_info.block == GpuBlock::McVmL2) ||
        (m_info.block == GpuBlock::Ea)     ||
        (m_info.block == GpuBlock::Rpb))
    {
        // There is only one set (low and high) of readback registers for this group of block perf-counters.
        // Before reading the perf counter, we must first say which perf counter to make available on the lo / hi
        // readback registers.
        const uint32 blockIdx        = static_cast<uint32>(m_info.block) - 1;
        const uint32 rsltCntlRegAddr = perfInfo.block[blockIdx].regInfo[m_slot].perfRsltCntlRegAddr;

        pCmdSpace = pCmdStream->WriteSetOnePrivilegedConfigReg(rsltCntlRegAddr,
                                                               m_rsltCntlReg,
                                                               pCmdSpace);
    }

    const gpusize gpuVirtAddr = (baseGpuVirtAddr + GetDataOffset());

    const EngineType engineType = pCmdStream->GetEngineType();

    // Write low 32bit portion of performance counter sample to the GPU virtual address.
    if (engineType == EngineTypeCompute)
    {
        pCmdSpace += cmdUtil.BuildCopyDataCompute(dst_sel__mec_copy_data__memory__GFX09,
                                                  gpuVirtAddr,
                                                  m_mecPerfCntSrcSel,
                                                  m_perfCountLoAddr,
                                                  count_sel__mec_copy_data__32_bits_of_data,
                                                  wr_confirm__mec_copy_data__wait_for_confirmation,
                                                  pCmdSpace);
    }
    else
    {
        pCmdSpace += cmdUtil.BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
                                                   dst_sel__me_copy_data__memory__GFX09,
                                                   gpuVirtAddr,
                                                   m_mePerfCntSrcSel,
                                                   m_perfCountLoAddr,
                                                   count_sel__me_copy_data__32_bits_of_data,
                                                   wr_confirm__me_copy_data__wait_for_confirmation,
                                                   pCmdSpace);
    }
    // Write high 32bit portion of performance counter sample to the GPU virtual address, if the
    // block uses 64bit counters.
    if (GetSampleSize() == sizeof(uint64))
    {
        if (engineType == EngineTypeCompute)
        {
            pCmdSpace += cmdUtil.BuildCopyDataCompute(dst_sel__mec_copy_data__memory__GFX09,
                                                      (gpuVirtAddr + sizeof(uint32)),
                                                      m_mecPerfCntSrcSel,
                                                      m_perfCountHiAddr,
                                                      count_sel__mec_copy_data__32_bits_of_data,
                                                      wr_confirm__mec_copy_data__wait_for_confirmation,
                                                      pCmdSpace);
        }
        else
        {
            pCmdSpace += cmdUtil.BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
                                                       dst_sel__me_copy_data__memory__GFX09,
                                                       (gpuVirtAddr + sizeof(uint32)),
                                                       m_mePerfCntSrcSel,
                                                       m_perfCountHiAddr,
                                                       count_sel__me_copy_data__32_bits_of_data,
                                                       wr_confirm__me_copy_data__wait_for_confirmation,
                                                       pCmdSpace);
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
StreamingPerfCounter::StreamingPerfCounter(
    const Device& device,
    GpuBlock      block,
    uint32        instance,
    uint32        slot
    )
    :
    Pal::StreamingPerfCounter(device.Parent(), block, instance, slot),
    m_device(device)
{

}

// =====================================================================================================================
Result StreamingPerfCounter::AddEvent(
    const GpuBlock& block,
    uint32          eventId)
{
    Result result = Result::Success;
    PAL_NOT_IMPLEMENTED();

    return result;
}

// =====================================================================================================================
// Writes commands necessary to enable this perf counter. This is specific to the gfx9 HW layer.
uint32* StreamingPerfCounter::WriteSetupCommands(
    Pal::CmdStream* pCmdStream,
    uint32*         pCmdSpace)
{
    return pCmdSpace;
}

} // Gfx9
} // Pal
