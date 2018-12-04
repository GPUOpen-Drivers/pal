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
    const CmdUtil&             cmdUtil  = device.CmdUtil();
    const Gfx9PerfCounterInfo& perfInfo = m_device.Parent()->ChipProperties().gfx9.perfCounterInfo;

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

        uint32 bankMask   = (flags.sqSqcBankMask) ?
                            (values.sqSqcBankMask & DefaultSqSelectBankMask) : DefaultSqSelectBankMask;

        regSQ_PERFCOUNTER0_SELECT sqSelect = {};
        sqSelect.bits.PERF_SEL             = info.eventId;
        sqSelect.bits.SQC_BANK_MASK        = bankMask;

        if (device.Parent()->ChipProperties().gfxLevel == GfxIpLevel::GfxIp9)
        {
            const uint32 clientMask = (flags.sqSqcClientMask)
                                       ? (values.sqSqcClientMask & DefaultSqSelectClientMask)
                                       : DefaultSqSelectClientMask;

            const uint32 simdMask = (flags.sqSimdMask)
                                     ? (values.sqSimdMask & DefaultSqSelectSimdMask)
                                     : DefaultSqSelectSimdMask;

            sqSelect.gfx09.SIMD_MASK       = simdMask;
            sqSelect.gfx09.SQC_CLIENT_MASK = clientMask;
        }

        m_selectReg[0]                     = sqSelect.u32All;
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
        regATC_L2_PERFCOUNTER0_CFG  atcL2PerfCntrCfg  = {};

        atcL2PerfCntrCfg.bits.PERF_SEL  = info.eventId;
        atcL2PerfCntrCfg.bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;
        atcL2PerfCntrCfg.bits.ENABLE    = 1;
        m_selectReg[0]                  = atcL2PerfCntrCfg.u32All;

        regATC_L2_PERFCOUNTER_RSLT_CNTL atcL2PerfCntrResultCntl = {};
        atcL2PerfCntrResultCntl.bits.PERF_COUNTER_SELECT = slot; // info.instance;
        m_rsltCntlReg = atcL2PerfCntrResultCntl.u32All;
    }
    else if (m_info.block == GpuBlock::McVmL2)
    {
        regMC_VM_L2_PERFCOUNTER0_CFG  mcVmL2PerfCntrCfg  = {};

        mcVmL2PerfCntrCfg.bits.PERF_SEL  = info.eventId;
        mcVmL2PerfCntrCfg.bits.PERF_MODE = PERFMON_COUNTER_MODE_ACCUM;
        mcVmL2PerfCntrCfg.bits.ENABLE    = 1;
        m_selectReg[0]                   = mcVmL2PerfCntrCfg.u32All;

        regMC_VM_L2_PERFCOUNTER_RSLT_CNTL mcVmL2PerfCntrResultCntl = {};
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

    const uint32 blockIdx  = static_cast<uint32>(m_info.block);
    const auto&  blockInfo = perfInfo.block[blockIdx];

    // Initialize performance counter flags.
    m_flags.u32All = 0;

    switch (m_info.block)
    {
    case GpuBlock::GrbmSe:
    case GpuBlock::Dma:
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 424
    case GpuBlock::Umcch:
#endif
        m_flags.isIndexed = 0;
        break;

    default:
        m_flags.isIndexed =
            ((blockInfo.distribution != PerfCounterDistribution::Unavailable) &&
             ((blockInfo.distribution != PerfCounterDistribution::GlobalBlock) || (blockInfo.numInstances > 0)));
        break;
    }

    // Setup the performance count registers to sample and the source-select for the Copy
    // Data PM4 commands issued when sampling the counter.
    if (m_info.block == GpuBlock::Dma)
    {
        // NOTE: SDMA is a 32bit counter. The Lo and Hi register addresses represent counters 0 and 1, rather
        //       than the Lo/Hi portions of a single 64bit counter like the other blocks.
        m_perfCountLoAddr  = (m_slot == 0) ? perfInfo.block[blockIdx].regInfo[m_info.instance].perfCountLoAddr
                                           : perfInfo.block[blockIdx].regInfo[m_info.instance].perfCountHiAddr;
        m_mePerfCntSrcSel  = src_sel__me_copy_data__perfcounters;
        m_mecPerfCntSrcSel = src_sel__mec_copy_data__perfcounters;
    }
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 424
    else if (m_info.block == GpuBlock::Umcch)
    {
        // Reading Umcch performance counter registers requires two register reads. The LO register must be read first.
        // Here {instance, slot} actually means {channel num, counter ID} for Umcch block.
        m_perfCountLoAddr = perfInfo.umcChannelBlocks.regInfo[m_info.instance].counter[m_slot].resultRegLoAddr;
        m_perfCountHiAddr = perfInfo.umcChannelBlocks.regInfo[m_info.instance].counter[m_slot].resultRegHiAddr;

        m_mePerfCntSrcSel  = src_sel__me_copy_data__perfcounters;
        m_mecPerfCntSrcSel = src_sel__mec_copy_data__perfcounters;
    }
#endif
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
template <typename SdmaRegType>
void PerfCounter::SetSdmaSelectReg(
    SdmaRegType* pSdmaReg
    ) const
{
    if (m_slot == 0)
    {
        pSdmaReg->PERF_SEL0    = m_info.eventId;
        pSdmaReg->PERF_ENABLE0 = 1;
    }
    else if (m_slot == 1)
    {
        pSdmaReg->PERF_SEL1    = m_info.eventId;
        pSdmaReg->PERF_ENABLE1 = 1;
    }
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
        SetSdmaSelectReg(&pSdma0PerfmonCntl->bits);

        regValue = pSdma0PerfmonCntl->u32All;
    }
    else if (m_info.instance == 1)
    {
        {
            SetSdmaSelectReg(&pSdma1PerfmonCntl->vega);
        }

        regValue = pSdma1PerfmonCntl->u32All;
    }

    return regValue;
}

// =====================================================================================================================
// Generates the GRBM_GFX_INDEX for an instance that exists per shader array, per shader engine.
static const uint32 GrbmGfxIndexPerSa(
    const Device&            device,
    const GpuChipProperties& chipProps,
    GpuBlock                 block,
    uint32                   numInstances,
    uint32                   instance)
{
    regGRBM_GFX_INDEX grbmGfxIndex    = {};
    const uint32      numShaderArrays = chipProps.gfx9.numShaderArrays;

    const uint32 seIndex = PerfCounter::InstanceIdToSe(numInstances, numShaderArrays, instance);
    const uint32 shIndex = PerfCounter::InstanceIdToSh(numInstances, numShaderArrays, instance);

    PAL_ASSERT(seIndex < chipProps.gfx9.numShaderEngines);
    PAL_ASSERT(shIndex < chipProps.gfx9.numShaderArrays);

    grbmGfxIndex.bits.SE_INDEX  = seIndex;
    grbmGfxIndex.gfx09.SH_INDEX = shIndex;
    if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
    {
        grbmGfxIndex.bits.INSTANCE_INDEX = PerfCounter::InstanceIdToInstance(numInstances, instance);
    }

    return grbmGfxIndex.u32All;
}

// =====================================================================================================================
// Generates the GRBM_GFX_INDEX for an instance that exists per shader engine.
static const uint32 GrbmGfxIndexPerSe(
    const Device&            device,
    const GpuChipProperties& chipProps,
    GpuBlock                 block,
    uint32                   numInstances,
    uint32                   instance)
{
    const uint32 numShaderEngines = chipProps.gfx9.numShaderEngines;

    // For PerShaderEngine, numInstances is in number of instances per SE. A quick divide gives us our seIndex.
    const uint32 seIndex = instance / numInstances;
    PAL_ASSERT(seIndex < chipProps.gfx9.numShaderEngines);

    regGRBM_GFX_INDEX grbmGfxIndex         = {};
    grbmGfxIndex.bits.SE_INDEX             = seIndex;
    grbmGfxIndex.gfx09.SH_BROADCAST_WRITES = 1;
    if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
    {
        grbmGfxIndex.bits.INSTANCE_INDEX = PerfCounter::InstanceIdToInstance(numInstances, instance);
    }

    return grbmGfxIndex.u32All;
}

// =====================================================================================================================
// Generates the GRBM_GFX_INDEX with special handling for the SQ setup which broadcasts the setup to all SQ instances
// within a shader engine.
static const uint32 GrbmGfxIndexSq(
    const GpuChipProperties& chipProps,
    uint32                   numInstances,
    uint32                   instance)
{
    const uint32 numShaderArrays = chipProps.gfx9.numShaderArrays;
    const uint32 seIndex         = instance / numInstances;

    regGRBM_GFX_INDEX grbmGfxIndex              = {};
    grbmGfxIndex.bits.SE_INDEX                  = seIndex;
    grbmGfxIndex.gfx09.SH_BROADCAST_WRITES      = 1;
    grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

    return grbmGfxIndex.u32All;
}

// =====================================================================================================================
// Generates the GRBM_GFX_INDEX for an instance that exists outside of the shader engines.
static const uint32 GrbmGfxIndexGlobal(
    const Device&            device,
    const GpuChipProperties& chipProps,
    GpuBlock                 block,
    uint32                   numInstances,
    uint32                   instance)
{
    regGRBM_GFX_INDEX grbmGfxIndex         = {};
    grbmGfxIndex.bits.SE_BROADCAST_WRITES  = 1;
    grbmGfxIndex.gfx09.SH_BROADCAST_WRITES = 1;
    if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
    {
        grbmGfxIndex.bits.INSTANCE_INDEX = PerfCounter::InstanceIdToInstance(numInstances, instance);
    }

    return grbmGfxIndex.u32All;
}

// =====================================================================================================================
// Counters associated with indexed GPU blocks need to write GRBM_GFX_INDEX to mask-off the SE/SH/Instance the counter
// is sampling from. This issues the PM4 command which sets up GRBM_GFX_INDEX appropriately. Returns the next unused
// DWORD in pCmdSpace.
static uint32* WriteGrbmGfxIndex(
    const Device& device,
    GpuBlock      block,
    uint32        instance,
    bool          sqSpecialHandling, // SQ needs special handling when setting up the initial register state, as
                                     // setting up the select registers need to be broadcast to all instances.
    CmdStream*    pCmdStream,
    uint32*       pCmdSpace)
{
    const auto&  chipProps    = device.Parent()->ChipProperties();
    const auto&  perfInfo     = chipProps.gfx9.perfCounterInfo;
    const auto&  blockInfo    = perfInfo.block[static_cast<uint32>(block)];
    const uint32 numInstances = blockInfo.numInstances;

    uint32 grbmGfxIndex = 0;

    if ((block == GpuBlock::Sq) && sqSpecialHandling)
    {
        // SQ needs special handling, as the SQG is per shader engine and we need to broadcast to all SH/Interface.
        // We only need to do this when setting up the selects. When sampling, we want to target a particular SQ.
        grbmGfxIndex = GrbmGfxIndexSq(chipProps, numInstances, instance);
    }
    else if (blockInfo.distribution == PerfCounterDistribution::PerShaderArray)
    {
        grbmGfxIndex = GrbmGfxIndexPerSa(device, chipProps, block, numInstances, instance);
    }
    else if (blockInfo.distribution == PerfCounterDistribution::PerShaderEngine)
    {
        grbmGfxIndex = GrbmGfxIndexPerSe(device, chipProps, block, numInstances, instance);
    }
    else if (blockInfo.distribution == PerfCounterDistribution::GlobalBlock)
    {
        grbmGfxIndex = GrbmGfxIndexGlobal(device, chipProps, block, numInstances, instance);
    }
    else
    {
        // What is this?
        PAL_ASSERT_ALWAYS();
    }

    pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                                  grbmGfxIndex,
                                                  pCmdSpace);

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

    const auto&  chipProps = m_device.Parent()->ChipProperties();
    const auto&  perfInfo  = chipProps.gfx9.perfCounterInfo;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 424
    if (m_info.block == GpuBlock::Umcch)
    {
        // The umc block is outside the GPU core. No need to write the GRBM_GFX_INDEX. There is also only one primary
        // select register that needs to be programmed for Umcch.
        const uint32 cntlRegAddr = perfInfo.umcChannelBlocks.regInfo[m_info.instance].counter[m_slot].ctrControlRegAddr;
        const auto engineType    = pCmdStream->GetEngineType();
        const auto& cmdUtil      = m_device.CmdUtil();

        regUMCCH0_PerfMonCtl1 ctlRegVal = { };
        ctlRegVal.bits.Enable           = 1;
        ctlRegVal.bits.EventSelect      = m_info.eventId;

        // The dst_reg_offset of the copy_data packet has a bit width of 18 in ordinal5. Hence for UMC channels
        // 3+, which have reg offsets >18 bits wide, we cannot use this packet and must resort to other methods to
        // program those. Currently we skip these channels.
        if (IsDstRegCopyDataPossible(m_perfCountLoAddr) == true)
        {
            // Write the select/cntl register with the event ID we wish to track.
            if (engineType == EngineTypeCompute)
            {
                pCmdSpace += cmdUtil.BuildCopyDataCompute(dst_sel__mec_copy_data__perfcounters,
                                                          cntlRegAddr,
                                                          src_sel__mec_copy_data__immediate_data,
                                                          ctlRegVal.u32All,
                                                          count_sel__mec_copy_data__32_bits_of_data,
                                                          wr_confirm__mec_copy_data__do_not_wait_for_confirmation,
                                                          pCmdSpace);
            }
            else
            {
                pCmdSpace += cmdUtil.BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
                                                           dst_sel__me_copy_data__perfcounters,
                                                           cntlRegAddr,
                                                           src_sel__me_copy_data__immediate_data,
                                                           ctlRegVal.u32All,
                                                           count_sel__me_copy_data__32_bits_of_data,
                                                           wr_confirm__me_copy_data__do_not_wait_for_confirmation,
                                                           pCmdSpace);
            }
        }

    }
    else
#endif
    {
        const uint32 blockIdx     = static_cast<uint32>(m_info.block);
        const uint32 secondaryReg = perfInfo.block[blockIdx].regInfo[m_slot].perfSel1RegAddr;
        const uint32 primaryReg   = perfInfo.block[blockIdx].regInfo[m_slot].perfSel0RegAddr;

        if (m_flags.isIndexed)
        {
            pCmdSpace = WriteGrbmGfxIndex(m_device, m_info.block, m_info.instance, true, pCmdStream, pCmdSpace);
        }

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
    const auto& cmdUtil   = m_device.CmdUtil();
    const auto& chipProps = m_device.Parent()->ChipProperties();
    const auto& perfInfo  = chipProps.gfx9.perfCounterInfo;

    pCmdSpace = WriteGrbmGfxIndex(m_device, m_info.block, m_info.instance, false, pCmdStream, pCmdSpace);

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

    // The dst_reg_offset of the copy_data packet has a bit width of 18 in ordinal5. Hence for UMC channels
    // 3+, which have reg offsets >18 bits wide, we cannot use this packet and must resort to other methods to
    // program those. Currently we skip these channels.
    if (IsDstRegCopyDataPossible(m_perfCountLoAddr) == true)
    {
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
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Constructor for gfx9 variant of streaming perf counter object.
Gfx9StreamingPerfCounter::Gfx9StreamingPerfCounter(
    const Device& device,
    GpuBlock      block,
    uint32        instance,
    uint32        slot
    )
    :
    Pal::StreamingPerfCounter(device.Parent(), block, instance, slot),
    m_device(device)
{
    const auto& gfx9ChipProps = m_device.Parent()->ChipProperties().gfx9;
    const auto& blockInfo     = gfx9ChipProps.perfCounterInfo.block[static_cast<uint32>(m_block)];

    // Initialize parent's performance counter flags.
    m_flags.u16All = 0;

    m_flags.isIndexed =
        ((blockInfo.distribution != PerfCounterDistribution::Unavailable) &&
        ((blockInfo.distribution != PerfCounterDistribution::GlobalBlock) || (blockInfo.numInstances > 0)));

    m_flags.isGlobalBlock = (blockInfo.distribution == PerfCounterDistribution::GlobalBlock);

    if (m_flags.isGlobalBlock)
    {
        m_segmentType = SpmDataSegmentType::Global;
    }
    else
    {
        const uint32 numInstances = blockInfo.numInstances;

        if (blockInfo.distribution == PerfCounterDistribution::PerShaderEngine)
        {
            m_segmentType = static_cast<SpmDataSegmentType>(m_instance / numInstances);
        }
        else if (blockInfo.distribution == PerfCounterDistribution::PerShaderArray)
        {
            m_segmentType = static_cast<SpmDataSegmentType>(PerfCounter::InstanceIdToSe(numInstances,
                                                                                        gfx9ChipProps.numShaderArrays,
                                                                                        m_instance));
        }
    }

    PAL_ASSERT(m_segmentType < SpmDataSegmentType::Count);

}
// =====================================================================================================================
// Returns true if any of the events governing perfcounter_select0 register is valid.
bool Gfx9StreamingPerfCounter::IsSelect0RegisterValid() const
{
    // SQ counters have only one event id per StreamingPerfCounter.
    return ((m_eventId[0] != StreamingPerfCounter::InvalidEventId) ||
            ((m_eventId[1] != StreamingPerfCounter::InvalidEventId) && (m_block != GpuBlock::Sq)));
}

// =====================================================================================================================
// Returns true if any of the events governing perfcounter_select1 field is valid.
bool Gfx9StreamingPerfCounter::IsSelect1RegisterValid() const
{
    // SQ counters don't have a select1 register.
    PAL_ASSERT(m_block != GpuBlock::Sq);

    return ((m_eventId[2] != StreamingPerfCounter::InvalidEventId) ||
            (m_eventId[3] != StreamingPerfCounter::InvalidEventId));
}

// =====================================================================================================================
Result Gfx9StreamingPerfCounter::AddEvent(
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
// Writes commands necessary to enable this perf counter. This is specific to the gfx9 HW layer.
uint32* Gfx9StreamingPerfCounter::WriteSetupCommands(
    Pal::CmdStream* pCmdStream,
    uint32*         pCmdSpace)
{
    CmdStream* pHwlCmdStream = static_cast<CmdStream*>(pCmdStream);

    const auto&  chipProps    = m_device.Parent()->ChipProperties();
    const auto&  perfInfo     = chipProps.gfx9.perfCounterInfo;
    const auto&  blockInfo    = perfInfo.block[static_cast<uint32>(m_block)];
    const uint32 primaryReg   = blockInfo.regInfo[m_slot].perfSel0RegAddr;
    const uint32 secondaryReg = blockInfo.regInfo[m_slot].perfSel1RegAddr;

    // If this is an indexed counter, we need to modify the GRBM_GFX_INDEX.
    if (m_flags.isIndexed)
    {
        pCmdSpace = WriteGrbmGfxIndex(m_device, m_block, m_instance, false, pHwlCmdStream, pCmdSpace);
    }

    // Write the PERFCOUNTERx_SELECT register corresponding to valid eventIds.
    if (IsSelect0RegisterValid())
    {
        pCmdSpace = pHwlCmdStream->WriteSetOnePerfCtrReg(primaryReg, GetSelect0RegisterData(), pCmdSpace);
    }

    // SQ blocks have only one SELECT register.
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
// Returns gfx9 hw specific muxselect encoding
uint16 Gfx9StreamingPerfCounter::GetMuxselEncoding(
    uint32 subSlot) const
{
    MuxselEncoding muxselEncoding;
    muxselEncoding.u16All = 0;

    auto const& gfx9ChipProps = m_device.Parent()->ChipProperties().gfx9;
    const uint32 numInstances = gfx9ChipProps.perfCounterInfo.block[static_cast<uint32>(m_block)].numInstances;

    muxselEncoding.counter  = subSlot;
    muxselEncoding.instance = PerfCounter::InstanceIdToInstance(numInstances, m_instance);
    muxselEncoding.block    = gfx9ChipProps.perfCounterInfo.block[static_cast<uint32>(m_block)].spmBlockSelectCode;

    return muxselEncoding.u16All;
}

// =====================================================================================================================
uint32 Gfx9StreamingPerfCounter::GetSelect0RegisterData() const
{
    // All blocks with streaming support except SQ are of the following format
    // PERF_SEL0 - 9:0
    // PERF_SEL1 - 19:10
    // CNTR_MODE - 23:20

    uint32 selectReg = 0;

    // PERF_SEL field of perfcounterx_select register.
    if (m_eventId[0] != InvalidEventId)
    {
        selectReg |= (m_eventId[0] << Gfx9PerfCounterPerfSel0Shift);
    }

    // PERF_SEL1 field for perfcounterx_select register. SQ perfcounterx_select registers don't have a PERF_SEL1 field.
    if ((m_eventId[1] != InvalidEventId) && (m_block != GpuBlock::Sq))
    {
        selectReg |= (m_eventId[1] << Gfx9PerfCounterPerfSel1Shift);
    }

    // The CNTR_MODE is set to clamp for now.
    selectReg |= (PERFMON_SPM_MODE_16BIT_CLAMP << Gfx9PerfCounterCntrModeShift);

    return selectReg;
}

// =====================================================================================================================
uint32 Gfx9StreamingPerfCounter::GetSelect1RegisterData() const
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
        select1Reg |= (m_eventId[2] << Gfx9PerfCounterPerfSel0Shift);
    }

    // PERF_SEL1 field
    if (m_eventId[3] != InvalidEventId)
    {
        select1Reg |= (m_eventId[3] << Gfx9PerfCounterPerfSel1Shift);
    }

    return select1Reg;
}

} // Gfx9
} // Pal
