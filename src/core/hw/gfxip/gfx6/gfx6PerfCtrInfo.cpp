/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/hw/gfxip/gfx6/gfx6PerfCtrInfo.h"
#include "palMath.h"
#include "palPerfExperiment.h"

#include "core/hw/amdgpu_asic.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{
namespace PerfCtrInfo
{

// =====================================================================================================================
// Helper function to initialize the performance counter information for a specific GPU block.
void SetupBlockInfo(
    GpuChipProperties* pProps,
    GpuBlock           block,            // Block identifier
    uint32             numShaderEngines, // Number of shader engines for this block
    uint32             numShaderArrays,  // Number of shader arrays for this block
    uint32             numInstances,     // Number of instances per shader array, except for SQ block(# of SQG per SH)
    uint32             numCounters,      // Number of counters per instance
    uint32             maxEventId,       // Maximum event ID for this block
    uint32             selReg0Addr,      // Register address for primary select register
    int32              selReg0Incr,      // Primary select register increment
    uint32             selReg1Addr,      // Register address for secondary select register
    int32              selReg1Incr,      // Secondary select register increment
    uint32             ctrLoRegAddr,     // Low counter data address register address
    uint32             ctrHiRegAddr,     // High counter data address register address
    int32              ctrRegIncr)       // Counter data register block address increment
{
    const GfxIpLevel          gfxIpLevel = pProps->gfxLevel;
    Gfx6PerfCounterInfo*const pInfo      = &pProps->gfx6.perfCounterInfo;

    const uint32 blockIdx = static_cast<uint32>(block);
    PAL_ASSERT(numCounters <= Gfx6MaxCountersPerBlock);

    pInfo->block[blockIdx].available        = true;
    pInfo->block[blockIdx].numShaderEngines = numShaderEngines;
    pInfo->block[blockIdx].numShaderArrays  = numShaderArrays;
    pInfo->block[blockIdx].numInstances     = numInstances;
    pInfo->block[blockIdx].numCounters      = numCounters;
    pInfo->block[blockIdx].maxEventId       = maxEventId;

    // Setup the register addresses for each counter for this block.
    uint32 perfSel0RegAddr = selReg0Addr;
    uint32 perfSel1RegAddr = selReg1Addr;
    uint32 perfCountLoAddr = ctrLoRegAddr;
    uint32 perfCountHiAddr = ctrHiRegAddr;
    for (size_t idx = 0; idx < numCounters; ++idx)
    {
        pInfo->block[blockIdx].regInfo[idx].perfSel0RegAddr = perfSel0RegAddr;
        pInfo->block[blockIdx].regInfo[idx].perfSel1RegAddr = perfSel1RegAddr;
        pInfo->block[blockIdx].regInfo[idx].perfCountLoAddr = perfCountLoAddr;
        pInfo->block[blockIdx].regInfo[idx].perfCountHiAddr = perfCountHiAddr;

        // On Gfx7 and Gfx8, many SELECT0 registers don't increase uniformly.
        if (gfxIpLevel >= GfxIpLevel::GfxIp7)
        {
            if (idx == 1)
            {
                switch (block)
                {
                case GpuBlock::Cb:
                case GpuBlock::Sc:
                case GpuBlock::Tcs:
                    selReg0Incr = 1;
                    break;
                default:
                    break;
                }
            }
            else if (idx == 2)
            {
                switch (block)
                {
                case GpuBlock::Pa:
                case GpuBlock::Tca:
                case GpuBlock::Tcc:
                case GpuBlock::Tcp:
                    selReg0Incr = 1;
                    break;
                default:
                    break;
                }
            }
            else if ((idx == 3) && (block == GpuBlock::Spi))
            {
                selReg0Incr = 5;
            }
            else if ((idx == 4) && (block == GpuBlock::Spi))
            {
                selReg0Incr = 1;
            }
        }

        perfSel0RegAddr += selReg0Incr;
        perfSel1RegAddr += selReg1Incr;
        perfCountLoAddr += ctrRegIncr;
        perfCountHiAddr += ctrRegIncr;
    }

    // Setup the number of streaming perf counters available to each block instance.
    if (gfxIpLevel == GfxIpLevel::GfxIp6)
    {
        // SI does not support streaming counters.
        pInfo->block[blockIdx].numStreamingCounters    = 0;
        pInfo->block[blockIdx].numStreamingCounterRegs = 0;
    }
    else if (gfxIpLevel >= GfxIpLevel::GfxIp7)
    {
        // For Gfx7,8 the number of streaming counters depends on which block we're looking at.
        switch (block)
        {
        case GpuBlock::Cb:
        case GpuBlock::Cpc:
        case GpuBlock::Cpf:
        case GpuBlock::Cpg:
        case GpuBlock::Gds:
        case GpuBlock::Ia:
        case GpuBlock::Sc:
        case GpuBlock::Ta:
        case GpuBlock::Tcs:
        case GpuBlock::Td:
            pInfo->block[blockIdx].numStreamingCounters    = 4;
            pInfo->block[blockIdx].numStreamingCounterRegs = 1;
            break;
        case GpuBlock::Db:
        case GpuBlock::Pa:
        case GpuBlock::Tcp:
        case GpuBlock::Vgt:
            // NOTE: The perfmon doc claims DB/PA/TCP/VGT each have six streaming counters, though the regspec
            //       indicates there is room for eight.
            pInfo->block[blockIdx].numStreamingCounters    = 6;
            pInfo->block[blockIdx].numStreamingCounterRegs = 2;
            break;
        case GpuBlock::Sx:
        case GpuBlock::Tca:
        case GpuBlock::Tcc:
            pInfo->block[blockIdx].numStreamingCounters    = 8;
            pInfo->block[blockIdx].numStreamingCounterRegs = 2;
            break;
        case GpuBlock::Spi:
            pInfo->block[blockIdx].numStreamingCounters    = 16;
            pInfo->block[blockIdx].numStreamingCounterRegs = 4;
            break;
        case GpuBlock::Sq:
            // NOTE: SQ streaming counters are not packed.
            pInfo->block[blockIdx].numStreamingCounters    = 16;
            pInfo->block[blockIdx].numStreamingCounterRegs = 16;
            break;
        default:
            pInfo->block[blockIdx].numStreamingCounters    = 0;
            pInfo->block[blockIdx].numStreamingCounterRegs = 0;
            break;
        }
    }
}

// =====================================================================================================================
// Helper function to initialize the performance counter information for the MC block.
void SetupMcBlockAndRegInfo(
    GpuChipProperties* pProps)
{
    const GfxIpLevel gfxIpLevel = pProps->gfxLevel;
    constexpr size_t BlockIdx   = static_cast<size_t>(GpuBlock::Mc);

    Gfx6PerfCounterInfo*const pInfo = &pProps->gfx6.perfCounterInfo;

    pInfo->block[BlockIdx].available               = true;
    pInfo->block[BlockIdx].numShaderEngines        = NumMcChannels;
    pInfo->block[BlockIdx].numShaderArrays         = 1;
    pInfo->block[BlockIdx].numInstances            = pProps->gfx6.numMcdTiles;
    pInfo->block[BlockIdx].numStreamingCounters    = 0;
    pInfo->block[BlockIdx].numStreamingCounterRegs = 0;

    if (gfxIpLevel == GfxIpLevel::GfxIp6)
    {
        pInfo->block[BlockIdx].numCounters = Gfx6NumMcCounters;
        pInfo->block[BlockIdx].maxEventId  = Gfx6PerfCtrMcSeqMaxEvent;
    }
    else if (gfxIpLevel == GfxIpLevel::GfxIp7)
    {
        pInfo->block[BlockIdx].numCounters = Gfx7NumMcCounters;
        pInfo->block[BlockIdx].maxEventId = Gfx7PerfCtrMcSeqMaxEvent;
    }
    else if (gfxIpLevel >= GfxIpLevel::GfxIp8)
    {
        pInfo->block[BlockIdx].numCounters = Gfx8NumMcCounters;
        pInfo->block[BlockIdx].maxEventId = Gfx8PerfCtrMcSeqMaxEvent;
    }

    constexpr uint32 RegStride = (mmMC_SEQ_PERF_SEQ_CNT_B_I0 - mmMC_SEQ_PERF_SEQ_CNT_A_I0);

    uint32 perfCountAddrChannel0 = mmMC_SEQ_PERF_SEQ_CNT_A_I0;
    uint32 perfCountAddrChannel1 = mmMC_SEQ_PERF_SEQ_CNT_A_I1;
    for (uint32 idx = 0; idx < pInfo->block[BlockIdx].numCounters; ++idx)
    {
        pInfo->block[BlockIdx].regInfo[idx].perfSel0RegAddr = mmMC_SEQ_PERF_SEQ_CTL;
        pInfo->block[BlockIdx].regInfo[idx].perfSel0RegAddr = mmMC_SEQ_PERF_CNTL_1;
        pInfo->block[BlockIdx].regInfo[idx].perfCountLoAddr = perfCountAddrChannel0;
        pInfo->block[BlockIdx].regInfo[idx].perfCountHiAddr = perfCountAddrChannel1;

        if (idx == 1)
        {
            // NOTE: There is a non-uniform stride between the register data counter B and C.
            //       Reset the address after setting the address for counter B.
            perfCountAddrChannel0 = mmMC_SEQ_PERF_SEQ_CNT_C_I0;
            perfCountAddrChannel1 = mmMC_SEQ_PERF_SEQ_CNT_C_I1;
        }
        else
        {
            perfCountAddrChannel0 += RegStride;
            perfCountAddrChannel1 += RegStride;
        }
    }

    // Pitcairn has a different MC config register than other hardware.
    if (AMDGPU_IS_PITCAIRN(pProps->familyId, pProps->eRevId))
    {
        pInfo->mcConfigRegAddress = mmMC_CONFIG;
        pInfo->mcWriteEnableMask  = (MC_CONFIG__MCDW_WR_ENABLE_MASK |
                                     MC_CONFIG__MCDX_WR_ENABLE_MASK |
                                     MC_CONFIG__MCDY_WR_ENABLE_MASK |
                                     MC_CONFIG__MCDZ_WR_ENABLE_MASK);
        pInfo->mcReadEnableShift  = MC_CONFIG__MC_RD_ENABLE__SHIFT__SI__CI;
    }
    else
    {
        pInfo->mcConfigRegAddress = mmMC_CONFIG_MCD;

        // The write enable mask selects which MCDs to write to.
        // Setup the write enable mask so that we only capture from present MCDs

        if (AMDGPU_IS_TONGA(pProps->familyId, pProps->eRevId) &&
            (pProps->gfx6.numMcdTiles == 4))
        {
            // The Four MCD tonga has an unusual CONFIG where it enables MCD0, 2, 3, and 5
            pInfo->mcWriteEnableMask = MC_CONFIG_MCD__MCD0_WR_ENABLE_MASK |
                                       MC_CONFIG_MCD__MCD2_WR_ENABLE_MASK |
                                       MC_CONFIG_MCD__MCD3_WR_ENABLE_MASK |
                                       MC_CONFIG_MCD__MCD5_WR_ENABLE_MASK;
        }
        else
        {
            pInfo->mcWriteEnableMask = (1 << pProps->gfx6.numMcdTiles) - 1;

            // Confirm that the write enable bits are where we are expecting the to be
            // for the previous calculation to set the correct bits.
            static_assert(MC_CONFIG_MCD__MCD0_WR_ENABLE_MASK == 0x1,
                "Write enable bits are not what we expect them to be.");
            static_assert(MC_CONFIG_MCD__MCD1_WR_ENABLE_MASK == 0x2,
                "Write enable bits are not what we expect them to be.");
            static_assert(MC_CONFIG_MCD__MCD2_WR_ENABLE_MASK == 0x4,
                "Write enable bits are not what we expect them to be.");
            static_assert(MC_CONFIG_MCD__MCD3_WR_ENABLE_MASK == 0x8,
                "Write enable bits are not what we expect them to be.");
            static_assert(MC_CONFIG_MCD__MCD4_WR_ENABLE_MASK == 0x10,
                "Write enable bits are not what we expect them to be.");
            static_assert(MC_CONFIG_MCD__MCD5_WR_ENABLE_MASK == 0x20,
                "Write enable bits are not what we expect them to be.");
            static_assert(MC_CONFIG_MCD__MCD6_WR_ENABLE_MASK__VI == 0x40,
                "Write enable bits are not what we expect them to be.");
            static_assert(MC_CONFIG_MCD__MCD7_WR_ENABLE_MASK__VI == 0x80,
                "Write enable bits are not what we expect them to be.");

            // The MC_CONFIG_MCD::MCD#_RD_ENABLE bits occupy the first 8 bits of the register.
            // Assert that the generated mask is no more than 8 bits.
            PAL_ASSERT((pInfo->mcWriteEnableMask & 0xFF) == pInfo->mcWriteEnableMask);
        }

        pInfo->mcReadEnableShift  = MC_CONFIG_MCD__MC_RD_ENABLE__SHIFT;
    }
}

// =====================================================================================================================
// Initializes the performance counter information for SI hardware.
void SetupGfx6Counters(
    GpuChipProperties* pProps)
{
    PAL_ASSERT(pProps->gfxLevel == GfxIpLevel::GfxIp6);

    constexpr uint32 DefaultShaderEngines = 1;
    constexpr uint32 DefaultShaderArrays  = 1;
    constexpr uint32 DefaultInstances     = 1;
    constexpr uint32 DefaultGroups        = 1;
    constexpr uint32 TcaInstances         = 2;
    // Each SQ(inside a CU) counts for that CU, but you cannot see that count. There is one set of 16 master counters
    // inside SPI(really SQG) that aggregates the counts from each CU and presents 16 counters which represent all of
    // the activity on the SE.
    // SQG represents the count for the entire shader engine(SE), and it's the only one visible to the user. So both
    // numShaderArrays and numInstances must be set to 1.
    constexpr uint32 SqShaderArrays = 1;
    constexpr uint32 SqInstances    = 1;

    const uint32 shaderEngines      = pProps->gfx6.numShaderEngines;
    const uint32 shaderArrays       = pProps->gfx6.numShaderArrays;
    const uint32 numCuPerSh         = pProps->gfx6.maxNumCuPerSh;
    const uint32 rbPerShaderArray   = (pProps->gfx6.maxNumRbPerSe / shaderArrays);

    // SRBM block
    SetupBlockInfo(pProps, GpuBlock::Srbm,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx6NumSrbmCounters, Gfx6PerfCtrSrbmMaxEvent,
                   mmSRBM_PERFCOUNTER0_SELECT__SI__CI,
                   (mmSRBM_PERFCOUNTER1_SELECT__SI__CI - mmSRBM_PERFCOUNTER0_SELECT__SI__CI),
                   0, 0,
                   mmSRBM_PERFCOUNTER0_LO__SI__CI, mmSRBM_PERFCOUNTER0_HI__SI__CI,
                   (mmSRBM_PERFCOUNTER1_LO__SI__CI - mmSRBM_PERFCOUNTER0_LO__SI__CI));

    // CP block
    SetupBlockInfo(pProps, GpuBlock::Cpf,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx6NumCpCounters, Gfx6PerfCtrCpMaxEvent,
                   mmCP_PERFCOUNTER_SELECT__SI, 0,
                   0, 0,
                   mmCP_PERFCOUNTER_LO__SI, mmCP_PERFCOUNTER_HI__SI, 0);

    // CB block
    SetupBlockInfo(pProps, GpuBlock::Cb,
                   shaderEngines, shaderArrays, rbPerShaderArray,
                   Gfx6NumCbCounters, Gfx6PerfCtrCbMaxEvent,
                   mmCB_PERFCOUNTER0_SELECT0__SI,
                   (mmCB_PERFCOUNTER1_SELECT0__SI - mmCB_PERFCOUNTER0_SELECT0__SI),
                   mmCB_PERFCOUNTER0_SELECT1__SI,
                   (mmCB_PERFCOUNTER1_SELECT1__SI - mmCB_PERFCOUNTER0_SELECT1__SI),
                   mmCB_PERFCOUNTER0_LO__SI, mmCB_PERFCOUNTER0_HI__SI,
                   (mmCB_PERFCOUNTER1_LO__SI - mmCB_PERFCOUNTER0_LO__SI));

    // DB block
    SetupBlockInfo(pProps, GpuBlock::Db,
                   shaderEngines, shaderArrays, rbPerShaderArray,
                   Gfx6NumDbCounters, Gfx6PerfCtrDbMaxEvent,
                   mmDB_PERFCOUNTER0_SELECT__SI,
                   (mmDB_PERFCOUNTER1_SELECT__SI - mmDB_PERFCOUNTER0_SELECT__SI),
                   0, 0,
                   mmDB_PERFCOUNTER0_LO__SI, mmDB_PERFCOUNTER0_HI__SI,
                   (mmDB_PERFCOUNTER1_LO__SI - mmDB_PERFCOUNTER0_LO__SI));

    // GRBM block
    SetupBlockInfo(pProps, GpuBlock::Grbm,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx6NumGrbmCounters, Gfx6PerfCtrGrbmMaxEvent,
                   mmGRBM_PERFCOUNTER0_SELECT__SI,
                   (mmGRBM_PERFCOUNTER1_SELECT__SI - mmGRBM_PERFCOUNTER0_SELECT__SI),
                   0, 0,
                   mmGRBM_PERFCOUNTER0_LO__SI, mmGRBM_PERFCOUNTER0_HI__SI,
                   (mmGRBM_PERFCOUNTER1_LO__SI - mmGRBM_PERFCOUNTER0_LO__SI));

    // GRBMSE block
    SetupBlockInfo(pProps, GpuBlock::GrbmSe,
                   DefaultShaderEngines, DefaultShaderArrays, shaderEngines,
                   Gfx6NumGrbmseCounters, Gfx6PerfCtrGrbmseMaxEvent,
                   mmGRBM_SE0_PERFCOUNTER_SELECT__SI, 0,
                   mmGRBM_SE1_PERFCOUNTER_SELECT__SI, 0,
                   mmGRBM_SE0_PERFCOUNTER_LO__SI, mmGRBM_SE0_PERFCOUNTER_HI__SI, 0);

    // PA block
    SetupBlockInfo(pProps, GpuBlock::Pa,
                   shaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx6NumPaCounters, Gfx6PerfCtrPaMaxEvent,
                   mmPA_SU_PERFCOUNTER0_SELECT__SI,
                   (mmPA_SU_PERFCOUNTER1_SELECT__SI - mmPA_SU_PERFCOUNTER0_SELECT__SI),
                   0, 0,
                   mmPA_SU_PERFCOUNTER0_LO__SI, mmPA_SU_PERFCOUNTER0_HI__SI,
                   (mmPA_SU_PERFCOUNTER1_LO__SI - mmPA_SU_PERFCOUNTER0_LO__SI));

    // SC block
    SetupBlockInfo(pProps, GpuBlock::Sc,
                   shaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx6NumScCounters, Gfx6PerfCtrScMaxEvent,
                   mmPA_SC_PERFCOUNTER0_SELECT__SI,
                   (mmPA_SC_PERFCOUNTER1_SELECT__SI - mmPA_SC_PERFCOUNTER0_SELECT__SI),
                   0, 0,
                   mmPA_SC_PERFCOUNTER0_LO__SI, mmPA_SC_PERFCOUNTER0_HI__SI,
                   (mmPA_SC_PERFCOUNTER1_LO__SI - mmPA_SC_PERFCOUNTER0_LO__SI));

    // SX block
    SetupBlockInfo(pProps, GpuBlock::Sx,
                   shaderEngines, shaderArrays, DefaultInstances,
                   Gfx6NumSxCounters, Gfx6PerfCtrSxMaxEvent,
                   mmSX_PERFCOUNTER0_SELECT__SI,
                   (mmSX_PERFCOUNTER1_SELECT__SI - mmSX_PERFCOUNTER0_SELECT__SI),
                   0, 0,
                   mmSX_PERFCOUNTER0_LO__SI, mmSX_PERFCOUNTER0_HI__SI,
                   (mmSX_PERFCOUNTER1_LO__SI - mmSX_PERFCOUNTER0_LO__SI));

    // SPI block
    SetupBlockInfo(pProps, GpuBlock::Spi,
                   shaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx6NumSpiCounters, Gfx6PerfCtrSpiMaxEvent,
                   mmSPI_PERFCOUNTER0_SELECT__SI,
                   (mmSPI_PERFCOUNTER1_SELECT__SI - mmSPI_PERFCOUNTER0_SELECT__SI),
                   0, 0,
                   mmSPI_PERFCOUNTER0_LO__SI, mmSPI_PERFCOUNTER0_HI__SI,
                   (mmSPI_PERFCOUNTER1_LO__SI - mmSPI_PERFCOUNTER0_LO__SI));

    // SQ block
    SetupBlockInfo(pProps, GpuBlock::Sq,
                   shaderEngines, SqShaderArrays, SqInstances,
                   Gfx6NumSqCounters, Gfx6PerfCtrSqMaxEvent,
                   mmSQ_PERFCOUNTER0_SELECT__SI,
                   (mmSQ_PERFCOUNTER1_SELECT__SI - mmSQ_PERFCOUNTER0_SELECT__SI),
                   0, 0,
                   mmSQ_PERFCOUNTER0_LO__SI, mmSQ_PERFCOUNTER0_HI__SI,
                   (mmSQ_PERFCOUNTER1_LO__SI - mmSQ_PERFCOUNTER0_LO__SI));

    // TA block
    SetupBlockInfo(pProps, GpuBlock::Ta,
                   shaderEngines, shaderArrays, numCuPerSh,
                   Gfx6NumTaCounters, Gfx6PerfCtrTaMaxEvent,
                   mmTA_PERFCOUNTER0_SELECT__SI,
                   (mmTA_PERFCOUNTER1_SELECT__SI - mmTA_PERFCOUNTER0_SELECT__SI),
                   0, 0,
                   mmTA_PERFCOUNTER0_LO__SI, mmTA_PERFCOUNTER0_HI__SI,
                   (mmTA_PERFCOUNTER1_LO__SI - mmTA_PERFCOUNTER0_LO__SI));

    // TD block
    SetupBlockInfo(pProps, GpuBlock::Td,
                   shaderEngines, shaderArrays, numCuPerSh,
                   Gfx6NumTdCounters, Gfx6PerfCtrTdMaxEvent,
                   mmTD_PERFCOUNTER0_SELECT__SI, 0,
                   0, 0,
                   mmTD_PERFCOUNTER0_LO__SI, mmTD_PERFCOUNTER0_HI__SI, 0);

    // TCP block
    SetupBlockInfo(pProps, GpuBlock::Tcp,
                   shaderEngines, shaderArrays, numCuPerSh,
                   Gfx6NumTcpCounters, Gfx6PerfCtrTcpMaxEvent,
                   mmTCP_PERFCOUNTER0_SELECT__SI,
                   (mmTCP_PERFCOUNTER1_SELECT__SI - mmTCP_PERFCOUNTER0_SELECT__SI),
                   0, 0,
                   mmTCP_PERFCOUNTER0_LO__SI, mmTCP_PERFCOUNTER0_HI__SI,
                   (mmTCP_PERFCOUNTER1_LO__SI - mmTCP_PERFCOUNTER0_LO__SI));

    // TCC block
    SetupBlockInfo(pProps, GpuBlock::Tcc,
                   DefaultShaderEngines, DefaultShaderArrays, pProps->gfx6.numTccBlocks,
                   Gfx6NumTccCounters, Gfx6PerfCtrTccMaxEvent,
                   mmTCC_PERFCOUNTER0_SELECT__SI,
                   (mmTCC_PERFCOUNTER1_SELECT__SI - mmTCC_PERFCOUNTER0_SELECT__SI),
                   0, 0,
                   mmTCC_PERFCOUNTER0_LO__SI, mmTCC_PERFCOUNTER0_HI__SI,
                   (mmTCC_PERFCOUNTER1_LO__SI - mmTCC_PERFCOUNTER0_LO__SI));

    // TCA block
    SetupBlockInfo(pProps, GpuBlock::Tca,
                   DefaultShaderEngines, DefaultShaderArrays, TcaInstances,
                   Gfx6NumTcaCounters, Gfx6PerfCtrTcaMaxEvent,
                   mmTCA_PERFCOUNTER0_SELECT__SI,
                   (mmTCA_PERFCOUNTER1_SELECT__SI - mmTCA_PERFCOUNTER0_SELECT__SI),
                   0, 0,
                   mmTCA_PERFCOUNTER0_LO__SI, mmTCA_PERFCOUNTER0_HI__SI,
                   (mmTCA_PERFCOUNTER1_LO__SI - mmTCA_PERFCOUNTER0_LO__SI));

    // GDS block
    SetupBlockInfo(pProps, GpuBlock::Gds,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx6NumGdsCounters, Gfx6PerfCtrGdsMaxEvent,
                   mmGDS_PERFCOUNTER0_SELECT__SI,
                   (mmGDS_PERFCOUNTER1_SELECT__SI - mmGDS_PERFCOUNTER0_SELECT__SI),
                   0, 0,
                   mmGDS_PERFCOUNTER0_LO__SI, mmGDS_PERFCOUNTER0_HI__SI,
                   (mmGDS_PERFCOUNTER1_LO__SI - mmGDS_PERFCOUNTER0_LO__SI));

    // VGT block
    SetupBlockInfo(pProps, GpuBlock::Vgt,
                   shaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx6NumVgtCounters, Gfx6PerfCtrVgtMaxEvent,
                   mmVGT_PERFCOUNTER0_SELECT__SI,
                   (mmVGT_PERFCOUNTER1_SELECT__SI - mmVGT_PERFCOUNTER0_SELECT__SI),
                   0, 0,
                   mmVGT_PERFCOUNTER0_LO__SI, mmVGT_PERFCOUNTER0_HI__SI,
                   (mmVGT_PERFCOUNTER1_LO__SI - mmVGT_PERFCOUNTER0_LO__SI));

    // IA block
    SetupBlockInfo(pProps, GpuBlock::Ia,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx6NumIaCounters, Gfx6PerfCtrIaMaxEvent,
                   mmIA_PERFCOUNTER0_SELECT__SI,
                   (mmIA_PERFCOUNTER1_SELECT__SI - mmIA_PERFCOUNTER0_SELECT__SI),
                   0, 0,
                   mmIA_PERFCOUNTER0_LO__SI, mmIA_PERFCOUNTER0_HI__SI,
                   (mmIA_PERFCOUNTER1_LO__SI - mmIA_PERFCOUNTER0_LO__SI));

    // MC block
    SetupMcBlockAndRegInfo(pProps);
}

// =====================================================================================================================
// Initializes the performance counter information for Gfx7 hardware.
void SetupGfx7Counters(
    GpuChipProperties* pProps)
{
    PAL_ASSERT(pProps->gfxLevel == GfxIpLevel::GfxIp7);

    constexpr uint32 DefaultShaderEngines = 1;
    constexpr uint32 DefaultShaderArrays  = 1;
    constexpr uint32 DefaultInstances     = 1;
    constexpr uint32 DefaultGroups        = 1;
    constexpr uint32 SdmaInstances        = 2;
    constexpr uint32 TcaInstances         = 2;
    // Each SQ(inside a CU) counts for that CU, but you cannot see that count. There is one set of 16 master counters
    // inside SPI(really SQG) that aggregates the counts from each CU and presents 16 counters which represent all of
    // the activity on the SE.
    // SQG represents the count for the entire shader engine(SE), and it's the only one visible to the user. So both
    // numShaderArrays and numInstances must be set to 1.
    constexpr uint32 SqShaderArrays = 1;
    constexpr uint32 SqInstances    = 1;

    const uint32 shaderEngines      = pProps->gfx6.numShaderEngines;
    const uint32 shaderArrays       = pProps->gfx6.numShaderArrays;
    const uint32 numCuPerSh         = pProps->gfx6.maxNumCuPerSh;
    const uint32 rbPerShaderArray   = (pProps->gfx6.maxNumRbPerSe / shaderArrays);

    // SRBM block
    SetupBlockInfo(pProps, GpuBlock::Srbm,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx7NumSrbmCounters, Gfx7PerfCtrSrbmMaxEvent,
                   mmSRBM_PERFCOUNTER0_SELECT__SI__CI,
                   (mmSRBM_PERFCOUNTER1_SELECT__SI__CI - mmSRBM_PERFCOUNTER0_SELECT__SI__CI),
                   0, 0,
                   mmSRBM_PERFCOUNTER0_LO__SI__CI, mmSRBM_PERFCOUNTER0_HI__SI__CI,
                   (mmSRBM_PERFCOUNTER1_LO__SI__CI - mmSRBM_PERFCOUNTER0_LO__SI__CI));

    // CPF block
    SetupBlockInfo(pProps, GpuBlock::Cpf,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx7NumCpfCounters, Gfx7PerfCtrCpfMaxEvent,
                   mmCPF_PERFCOUNTER0_SELECT__CI__VI,
                   (mmCPF_PERFCOUNTER1_SELECT__CI__VI - mmCPF_PERFCOUNTER0_SELECT__CI__VI),
                   mmCPF_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmCPF_PERFCOUNTER0_LO__CI__VI, mmCPF_PERFCOUNTER0_HI__CI__VI,
                   (mmCPF_PERFCOUNTER1_LO__CI__VI - mmCPF_PERFCOUNTER0_LO__CI__VI));

    // CPG block
    SetupBlockInfo(pProps, GpuBlock::Cpg,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx7NumCpgCounters, Gfx7PerfCtrCpgMaxEvent,
                   mmCPG_PERFCOUNTER0_SELECT__CI__VI,
                   (mmCPG_PERFCOUNTER1_SELECT__CI__VI - mmCPG_PERFCOUNTER0_SELECT__CI__VI),
                   mmCPG_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmCPG_PERFCOUNTER0_LO__CI__VI, mmCPG_PERFCOUNTER0_HI__CI__VI,
                   (mmCPG_PERFCOUNTER1_LO__CI__VI - mmCPG_PERFCOUNTER0_LO__CI__VI));

    // CPC block
    SetupBlockInfo(pProps, GpuBlock::Cpc,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx7NumCpcCounters, Gfx7PerfCtrCpcMaxEvent,
                   mmCPC_PERFCOUNTER0_SELECT__CI__VI,
                   (mmCPC_PERFCOUNTER1_SELECT__CI__VI - mmCPC_PERFCOUNTER0_SELECT__CI__VI),
                   mmCPC_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmCPC_PERFCOUNTER0_LO__CI__VI, mmCPC_PERFCOUNTER0_HI__CI__VI,
                   (mmCPC_PERFCOUNTER1_LO__CI__VI - mmCPC_PERFCOUNTER0_LO__CI__VI));

    // CB block
    SetupBlockInfo(pProps, GpuBlock::Cb,
                   shaderEngines, shaderArrays, rbPerShaderArray,
                   Gfx7NumCbCounters, Gfx7PerfCtrCbMaxEvent,
                   mmCB_PERFCOUNTER0_SELECT__CI__VI,
                   (mmCB_PERFCOUNTER1_SELECT__CI__VI - mmCB_PERFCOUNTER0_SELECT__CI__VI),
                   mmCB_PERFCOUNTER0_SELECT1__CI__VI,
                   0,
                   mmCB_PERFCOUNTER0_LO__CI__VI, mmCB_PERFCOUNTER0_HI__CI__VI,
                   (mmCB_PERFCOUNTER1_LO__CI__VI - mmCB_PERFCOUNTER0_LO__CI__VI));

    // DB block
    SetupBlockInfo(pProps, GpuBlock::Db,
                   shaderEngines, shaderArrays, rbPerShaderArray,
                   Gfx7NumDbCounters, Gfx7PerfCtrDbMaxEvent,
                   mmDB_PERFCOUNTER0_SELECT__CI__VI,
                   (mmDB_PERFCOUNTER1_SELECT__CI__VI - mmDB_PERFCOUNTER0_SELECT__CI__VI),
                   0, 0,
                   mmDB_PERFCOUNTER0_LO__CI__VI, mmDB_PERFCOUNTER0_HI__CI__VI,
                   (mmDB_PERFCOUNTER1_LO__CI__VI - mmDB_PERFCOUNTER0_LO__CI__VI));

    // GRBM block
    SetupBlockInfo(pProps, GpuBlock::Grbm,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx7NumGrbmCounters, Gfx7PerfCtrGrbmMaxEvent,
                   mmGRBM_PERFCOUNTER0_SELECT__CI__VI,
                   (mmGRBM_PERFCOUNTER1_SELECT__CI__VI - mmGRBM_PERFCOUNTER0_SELECT__CI__VI),
                   0, 0,
                   mmGRBM_PERFCOUNTER0_LO__CI__VI, mmGRBM_PERFCOUNTER0_HI__CI__VI,
                   (mmGRBM_PERFCOUNTER1_LO__CI__VI - mmGRBM_PERFCOUNTER0_LO__CI__VI));

    // GRBMSE block
    SetupBlockInfo(pProps, GpuBlock::GrbmSe,
                   DefaultShaderEngines, DefaultShaderArrays, shaderEngines,
                   Gfx7NumGrbmseCounters, Gfx7PerfCtrGrbmseMaxEvent,
                   mmGRBM_SE0_PERFCOUNTER_SELECT__CI__VI, 0,
                   mmGRBM_SE1_PERFCOUNTER_SELECT__CI__VI, 0,
                   mmGRBM_SE0_PERFCOUNTER_LO__CI__VI, mmGRBM_SE0_PERFCOUNTER_HI__CI__VI, 0);

    // RLC block
    SetupBlockInfo(pProps, GpuBlock::Rlc,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx7NumRlcCounters, Gfx7PerfCtrRlcMaxEvent,
                   mmRLC_PERFCOUNTER0_SELECT__CI__VI,
                   (mmRLC_PERFCOUNTER1_SELECT__CI__VI - mmRLC_PERFCOUNTER0_SELECT__CI__VI),
                   0, 0,
                   mmRLC_PERFCOUNTER0_LO__CI__VI, mmRLC_PERFCOUNTER0_HI__CI__VI,
                   (mmRLC_PERFCOUNTER1_LO__CI__VI - mmRLC_PERFCOUNTER0_LO__CI__VI));

    // PA block
    SetupBlockInfo(pProps, GpuBlock::Pa,
                   shaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx7NumPaCounters, Gfx7PerfCtrPaMaxEvent,
                   mmPA_SU_PERFCOUNTER0_SELECT__CI__VI,
                   (mmPA_SU_PERFCOUNTER1_SELECT__CI__VI - mmPA_SU_PERFCOUNTER0_SELECT__CI__VI),
                   mmPA_SU_PERFCOUNTER0_SELECT1__CI__VI,
                   (mmPA_SU_PERFCOUNTER1_SELECT1__CI__VI - mmPA_SU_PERFCOUNTER0_SELECT1__CI__VI),
                   mmPA_SU_PERFCOUNTER0_LO__CI__VI, mmPA_SU_PERFCOUNTER0_HI__CI__VI,
                   (mmPA_SU_PERFCOUNTER1_LO__CI__VI - mmPA_SU_PERFCOUNTER0_LO__CI__VI));

    // SC block
    SetupBlockInfo(pProps, GpuBlock::Sc,
                   shaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx7NumScCounters, Gfx7PerfCtrScMaxEvent,
                   mmPA_SC_PERFCOUNTER0_SELECT__CI__VI,
                   (mmPA_SC_PERFCOUNTER1_SELECT__CI__VI - mmPA_SC_PERFCOUNTER0_SELECT__CI__VI),
                   mmPA_SC_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmPA_SC_PERFCOUNTER0_LO__CI__VI, mmPA_SC_PERFCOUNTER0_HI__CI__VI,
                   (mmPA_SC_PERFCOUNTER1_LO__CI__VI - mmPA_SC_PERFCOUNTER0_LO__CI__VI));

    // SX block
    SetupBlockInfo(pProps, GpuBlock::Sx,
                   shaderEngines, shaderArrays, DefaultInstances,
                   Gfx7NumSxCounters, Gfx7PerfCtrSxMaxEvent,
                   mmSX_PERFCOUNTER0_SELECT__CI__VI,
                   (mmSX_PERFCOUNTER1_SELECT__CI__VI - mmSX_PERFCOUNTER0_SELECT__CI__VI),
                   mmSX_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmSX_PERFCOUNTER0_LO__CI__VI, mmSX_PERFCOUNTER0_HI__CI__VI,
                   (mmSX_PERFCOUNTER1_LO__CI__VI - mmSX_PERFCOUNTER0_LO__CI__VI));

    // SPI block
    SetupBlockInfo(pProps, GpuBlock::Spi,
                   shaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx7NumSpiCounters, Gfx7PerfCtrSpiMaxEvent,
                   mmSPI_PERFCOUNTER0_SELECT__CI__VI,
                   (mmSPI_PERFCOUNTER1_SELECT__CI__VI - mmSPI_PERFCOUNTER0_SELECT__CI__VI),
                   mmSPI_PERFCOUNTER0_SELECT1__CI__VI,
                   (mmSPI_PERFCOUNTER1_SELECT1__CI__VI - mmSPI_PERFCOUNTER0_SELECT1__CI__VI),
                   mmSPI_PERFCOUNTER0_LO__CI__VI, mmSPI_PERFCOUNTER0_HI__CI__VI,
                   (mmSPI_PERFCOUNTER1_LO__CI__VI - mmSPI_PERFCOUNTER0_LO__CI__VI));

    // SQ block
    SetupBlockInfo(pProps, GpuBlock::Sq,
                   shaderEngines, SqShaderArrays, SqInstances,
                   Gfx7NumSqCounters, Gfx7PerfCtrSqMaxEvent,
                   mmSQ_PERFCOUNTER0_SELECT__CI__VI,
                   (mmSQ_PERFCOUNTER1_SELECT__CI__VI - mmSQ_PERFCOUNTER0_SELECT__CI__VI),
                   0, 0,
                   mmSQ_PERFCOUNTER0_LO__CI__VI, mmSQ_PERFCOUNTER0_HI__CI__VI,
                   (mmSQ_PERFCOUNTER1_LO__CI__VI - mmSQ_PERFCOUNTER0_LO__CI__VI));

    // TA block
    SetupBlockInfo(pProps, GpuBlock::Ta,
                   shaderEngines, shaderArrays, numCuPerSh,
                   Gfx7NumTaCounters, Gfx7PerfCtrTaMaxEvent,
                   mmTA_PERFCOUNTER0_SELECT__CI__VI,
                   (mmTA_PERFCOUNTER1_SELECT__CI__VI - mmTA_PERFCOUNTER0_SELECT__CI__VI),
                   mmTA_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmTA_PERFCOUNTER0_LO__CI__VI, mmTA_PERFCOUNTER0_HI__CI__VI,
                   (mmTA_PERFCOUNTER1_LO__CI__VI - mmTA_PERFCOUNTER0_LO__CI__VI));

    // TD block
    SetupBlockInfo(pProps, GpuBlock::Td,
                   shaderEngines, shaderArrays, numCuPerSh,
                   Gfx7NumTdCounters, Gfx7PerfCtrTdMaxEvent,
                   mmTD_PERFCOUNTER0_SELECT__CI__VI,
                   (mmTD_PERFCOUNTER1_SELECT__CI__VI - mmTD_PERFCOUNTER0_SELECT__CI__VI),
                   mmTD_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmTD_PERFCOUNTER0_LO__CI__VI, mmTD_PERFCOUNTER0_HI__CI__VI, 0);

    // TCP block
    SetupBlockInfo(pProps, GpuBlock::Tcp,
                   shaderEngines, shaderArrays, numCuPerSh,
                   Gfx7NumTcpCounters, Gfx7PerfCtrTcpMaxEvent,
                   mmTCP_PERFCOUNTER0_SELECT__CI__VI,
                   (mmTCP_PERFCOUNTER1_SELECT__CI__VI - mmTCP_PERFCOUNTER0_SELECT__CI__VI),
                   mmTCP_PERFCOUNTER0_SELECT1__CI__VI,
                   (mmTCP_PERFCOUNTER1_SELECT1__CI__VI - mmTCP_PERFCOUNTER0_SELECT1__CI__VI),
                   mmTCP_PERFCOUNTER0_LO__CI__VI, mmTCP_PERFCOUNTER0_HI__CI__VI,
                   (mmTCP_PERFCOUNTER1_LO__CI__VI - mmTCP_PERFCOUNTER0_LO__CI__VI));

    // TCC block
    SetupBlockInfo(pProps, GpuBlock::Tcc,
                   DefaultShaderEngines, DefaultShaderArrays, pProps->gfx6.numTccBlocks,
                   Gfx7NumTccCounters, Gfx7PerfCtrTccMaxEvent,
                   mmTCC_PERFCOUNTER0_SELECT__CI__VI,
                   (mmTCC_PERFCOUNTER1_SELECT__CI__VI - mmTCC_PERFCOUNTER0_SELECT__CI__VI),
                   mmTCC_PERFCOUNTER0_SELECT1__CI__VI,
                   (mmTCC_PERFCOUNTER1_SELECT1__CI__VI - mmTCC_PERFCOUNTER0_SELECT1__CI__VI),
                   mmTCC_PERFCOUNTER0_LO__CI__VI, mmTCC_PERFCOUNTER0_HI__CI__VI,
                   (mmTCC_PERFCOUNTER1_LO__CI__VI - mmTCC_PERFCOUNTER0_LO__CI__VI));

    // TCA block
    SetupBlockInfo(pProps, GpuBlock::Tca,
                   DefaultShaderEngines, DefaultShaderArrays, TcaInstances,
                   Gfx7NumTcaCounters, Gfx7PerfCtrTcaMaxEvent,
                   mmTCA_PERFCOUNTER0_SELECT__CI__VI,
                   (mmTCA_PERFCOUNTER1_SELECT__CI__VI - mmTCA_PERFCOUNTER0_SELECT__CI__VI),
                   mmTCA_PERFCOUNTER0_SELECT1__CI__VI,
                   (mmTCA_PERFCOUNTER1_SELECT1__CI__VI - mmTCA_PERFCOUNTER0_SELECT1__CI__VI),
                   mmTCA_PERFCOUNTER0_LO__CI__VI, mmTCA_PERFCOUNTER0_HI__CI__VI,
                   (mmTCA_PERFCOUNTER1_LO__CI__VI - mmTCA_PERFCOUNTER0_LO__CI__VI));

    // GDS block
    SetupBlockInfo(pProps, GpuBlock::Gds,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx7NumGdsCounters, Gfx7PerfCtrGdsMaxEvent,
                   mmGDS_PERFCOUNTER0_SELECT__CI__VI,
                   (mmGDS_PERFCOUNTER1_SELECT__CI__VI - mmGDS_PERFCOUNTER0_SELECT__CI__VI),
                   mmGDS_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmGDS_PERFCOUNTER0_LO__CI__VI, mmGDS_PERFCOUNTER0_HI__CI__VI,
                   (mmGDS_PERFCOUNTER1_LO__CI__VI - mmGDS_PERFCOUNTER0_LO__CI__VI));

    // VGT block
    SetupBlockInfo(pProps, GpuBlock::Vgt,
                   shaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx7NumVgtCounters, Gfx7PerfCtrVgtMaxEvent,
                   mmVGT_PERFCOUNTER0_SELECT__CI__VI,
                   (mmVGT_PERFCOUNTER1_SELECT__CI__VI - mmVGT_PERFCOUNTER0_SELECT__CI__VI),
                   mmVGT_PERFCOUNTER0_SELECT1__CI__VI,
                   (mmVGT_PERFCOUNTER1_SELECT1__CI__VI - mmVGT_PERFCOUNTER0_SELECT1__CI__VI),
                   mmVGT_PERFCOUNTER0_LO__CI__VI, mmVGT_PERFCOUNTER0_HI__CI__VI,
                   (mmVGT_PERFCOUNTER1_LO__CI__VI - mmVGT_PERFCOUNTER0_LO__CI__VI));

    // IA block
    SetupBlockInfo(pProps, GpuBlock::Ia,
                   Max(shaderEngines / 2U, 1U), DefaultShaderArrays, DefaultInstances,
                   Gfx7NumIaCounters, Gfx7PerfCtrIaMaxEvent,
                   mmIA_PERFCOUNTER0_SELECT__CI__VI,
                   (mmIA_PERFCOUNTER1_SELECT__CI__VI - mmIA_PERFCOUNTER0_SELECT__CI__VI),
                   mmIA_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmIA_PERFCOUNTER0_LO__CI__VI, mmIA_PERFCOUNTER0_HI__CI__VI,
                   (mmIA_PERFCOUNTER1_LO__CI__VI - mmIA_PERFCOUNTER0_LO__CI__VI));

    // WD block
    SetupBlockInfo(pProps, GpuBlock::Wd,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx7NumWdCounters, Gfx7PerfCtrWdMaxEvent,
                   mmWD_PERFCOUNTER0_SELECT__CI__VI,
                   (mmWD_PERFCOUNTER1_SELECT__CI__VI - mmWD_PERFCOUNTER0_SELECT__CI__VI),
                   0, 0,
                   mmWD_PERFCOUNTER0_LO__CI__VI, mmWD_PERFCOUNTER0_HI__CI__VI,
                   (mmWD_PERFCOUNTER1_LO__CI__VI - mmWD_PERFCOUNTER0_LO__CI__VI));

    // MC block
    SetupMcBlockAndRegInfo(pProps);

    // SDMA block
    SetupBlockInfo(pProps, GpuBlock::Dma,
                   DefaultShaderEngines, DefaultShaderArrays, SdmaInstances,
                   Gfx7NumSdmaCounters, Gfx7PerfCtrSdmaMaxEvent,
                   mmSDMA0_PERFMON_CNTL__CI,
                   (mmSDMA1_PERFMON_CNTL__CI - mmSDMA0_PERFMON_CNTL__CI),
                   0, 0,
                   mmSDMA0_PERFCOUNTER0_RESULT__CI,
                   mmSDMA0_PERFCOUNTER1_RESULT__CI,
                   (mmSDMA1_PERFCOUNTER0_RESULT__CI - mmSDMA0_PERFCOUNTER1_RESULT__CI));

    // Only Kaveri (Spectre & Spooky) chips have the TCS block.
    if (AMDGPU_IS_SPECTRE(pProps->familyId, pProps->eRevId) ||
        AMDGPU_IS_SPOOKY(pProps->familyId, pProps->eRevId))
    {
        SetupBlockInfo(pProps, GpuBlock::Tcs,
                       DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                       Gfx7NumTcsCounters, Gfx7PerfCtrTcsMaxEvent,
                       mmTCS_PERFCOUNTER0_SELECT__CI,
                       (mmTCS_PERFCOUNTER1_SELECT__CI - mmTCS_PERFCOUNTER0_SELECT__CI),
                       mmTCS_PERFCOUNTER0_SELECT1__CI, 0,
                       mmTCS_PERFCOUNTER0_LO__CI,
                       mmTCS_PERFCOUNTER0_HI__CI,
                       (mmTCS_PERFCOUNTER1_LO__CI - mmTCS_PERFCOUNTER0_LO__CI));
    }
}

// =====================================================================================================================
// Initializes the performance counter information for Gfx8 hardware.
void SetupGfx8Counters(
    GpuChipProperties* pProps)
{
    PAL_ASSERT(pProps->gfxLevel >= GfxIpLevel::GfxIp8);

    constexpr uint32 DefaultShaderEngines = 1;
    constexpr uint32 DefaultShaderArrays  = 1;
    constexpr uint32 DefaultInstances     = 1;
    constexpr uint32 DefaultGroups        = 1;
    constexpr uint32 SdmaInstances        = 2;
    constexpr uint32 TcaInstances         = 2;
    // Each SQ(inside a CU) counts for that CU, but you cannot see that count. There is one set of 16 master counters
    // inside SPI(really SQG) that aggregates the counts from each CU and presents 16 counters which represent all of
    // the activity on the SE.
    // SQG represents the count for the entire shader engine(SE), and it's the only one visible to the user. So both
    // numShaderArrays and numInstances must be set to 1.
    constexpr uint32 SqShaderArrays = 1;
    constexpr uint32 SqInstances    = 1;

    const uint32 shaderEngines      = pProps->gfx6.numShaderEngines;
    const uint32 shaderArrays       = pProps->gfx6.numShaderArrays;
    const uint32 numCuPerSh         = pProps->gfx6.maxNumCuPerSh;
    const uint32 rbPerShaderArray   = (pProps->gfx6.maxNumRbPerSe / shaderArrays);

    // SRBM block
    SetupBlockInfo(pProps, GpuBlock::Srbm,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx8NumSrbmCounters, Gfx8PerfCtrSrbmMaxEvent,
                   mmSRBM_PERFCOUNTER0_SELECT__VI,
                   (mmSRBM_PERFCOUNTER1_SELECT__VI - mmSRBM_PERFCOUNTER0_SELECT__VI),
                   0, 0,
                   mmSRBM_PERFCOUNTER0_LO__VI, mmSRBM_PERFCOUNTER0_HI__VI,
                   (mmSRBM_PERFCOUNTER1_LO__VI - mmSRBM_PERFCOUNTER0_LO__VI));

    // CPF block
    SetupBlockInfo(pProps, GpuBlock::Cpf,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx8NumCpfCounters, Gfx8PerfCtrCpfMaxEvent,
                   mmCPF_PERFCOUNTER0_SELECT__CI__VI,
                   (mmCPF_PERFCOUNTER1_SELECT__CI__VI - mmCPF_PERFCOUNTER0_SELECT__CI__VI),
                   mmCPF_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmCPF_PERFCOUNTER0_LO__CI__VI, mmCPF_PERFCOUNTER0_HI__CI__VI,
                   (mmCPF_PERFCOUNTER1_LO__CI__VI - mmCPF_PERFCOUNTER0_LO__CI__VI));

    // CPG block
    SetupBlockInfo(pProps, GpuBlock::Cpg,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx8NumCpgCounters, Gfx8PerfCtrCpgMaxEvent,
                   mmCPG_PERFCOUNTER0_SELECT__CI__VI,
                   (mmCPG_PERFCOUNTER1_SELECT__CI__VI - mmCPG_PERFCOUNTER0_SELECT__CI__VI),
                   mmCPG_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmCPG_PERFCOUNTER0_LO__CI__VI, mmCPG_PERFCOUNTER0_HI__CI__VI,
                   (mmCPG_PERFCOUNTER1_LO__CI__VI - mmCPG_PERFCOUNTER0_LO__CI__VI));

    // CPC block
    SetupBlockInfo(pProps, GpuBlock::Cpc,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx8NumCpcCounters, Gfx8PerfCtrCpcMaxEvent,
                   mmCPC_PERFCOUNTER0_SELECT__CI__VI,
                   (mmCPC_PERFCOUNTER1_SELECT__CI__VI - mmCPC_PERFCOUNTER0_SELECT__CI__VI),
                   mmCPC_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmCPC_PERFCOUNTER0_LO__CI__VI, mmCPC_PERFCOUNTER0_HI__CI__VI,
                   (mmCPC_PERFCOUNTER1_LO__CI__VI - mmCPC_PERFCOUNTER0_LO__CI__VI));

    // CB block
    SetupBlockInfo(pProps, GpuBlock::Cb,
                   shaderEngines, shaderArrays, rbPerShaderArray,
                   Gfx8NumCbCounters, Gfx8PerfCtrCbMaxEvent,
                   mmCB_PERFCOUNTER0_SELECT__CI__VI,
                   (mmCB_PERFCOUNTER1_SELECT__CI__VI - mmCB_PERFCOUNTER0_SELECT__CI__VI),
                   mmCB_PERFCOUNTER0_SELECT1__CI__VI,
                   0,
                   mmCB_PERFCOUNTER0_LO__CI__VI, mmCB_PERFCOUNTER0_HI__CI__VI,
                   (mmCB_PERFCOUNTER1_LO__CI__VI - mmCB_PERFCOUNTER0_LO__CI__VI));

    // DB block
    SetupBlockInfo(pProps, GpuBlock::Db,
                   shaderEngines, shaderArrays, rbPerShaderArray,
                   Gfx8NumDbCounters, Gfx8PerfCtrDbMaxEvent,
                   mmDB_PERFCOUNTER0_SELECT__CI__VI,
                   (mmDB_PERFCOUNTER1_SELECT__CI__VI - mmDB_PERFCOUNTER0_SELECT__CI__VI),
                   0, 0,
                   mmDB_PERFCOUNTER0_LO__CI__VI, mmDB_PERFCOUNTER0_HI__CI__VI,
                   (mmDB_PERFCOUNTER1_LO__CI__VI - mmDB_PERFCOUNTER0_LO__CI__VI));

    // GRBM block
    SetupBlockInfo(pProps, GpuBlock::Grbm,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx8NumGrbmCounters, Gfx8PerfCtrGrbmMaxEvent,
                   mmGRBM_PERFCOUNTER0_SELECT__CI__VI,
                   (mmGRBM_PERFCOUNTER1_SELECT__CI__VI - mmGRBM_PERFCOUNTER0_SELECT__CI__VI),
                   0, 0,
                   mmGRBM_PERFCOUNTER0_LO__CI__VI, mmGRBM_PERFCOUNTER0_HI__CI__VI,
                   (mmGRBM_PERFCOUNTER1_LO__CI__VI - mmGRBM_PERFCOUNTER0_LO__CI__VI));

    // GRBMSE block
    SetupBlockInfo(pProps, GpuBlock::GrbmSe,
                   DefaultShaderEngines, DefaultShaderArrays, shaderEngines,
                   Gfx8NumGrbmseCounters, Gfx8PerfCtrGrbmseMaxEvent,
                   mmGRBM_SE0_PERFCOUNTER_SELECT__CI__VI, 0,
                   mmGRBM_SE1_PERFCOUNTER_SELECT__CI__VI, 0,
                   mmGRBM_SE0_PERFCOUNTER_LO__CI__VI, mmGRBM_SE0_PERFCOUNTER_HI__CI__VI, 0);

    // RLC block
    SetupBlockInfo(pProps, GpuBlock::Rlc,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx8NumRlcCounters, Gfx8PerfCtrRlcMaxEvent,
                   mmRLC_PERFCOUNTER0_SELECT__CI__VI,
                   (mmRLC_PERFCOUNTER1_SELECT__CI__VI - mmRLC_PERFCOUNTER0_SELECT__CI__VI),
                   0, 0,
                   mmRLC_PERFCOUNTER0_LO__CI__VI, mmRLC_PERFCOUNTER0_HI__CI__VI,
                   (mmRLC_PERFCOUNTER1_LO__CI__VI - mmRLC_PERFCOUNTER0_LO__CI__VI));

    // PA block
    SetupBlockInfo(pProps, GpuBlock::Pa,
                   shaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx8NumPaCounters, Gfx8PerfCtrPaMaxEvent,
                   mmPA_SU_PERFCOUNTER0_SELECT__CI__VI,
                   (mmPA_SU_PERFCOUNTER1_SELECT__CI__VI - mmPA_SU_PERFCOUNTER0_SELECT__CI__VI),
                   mmPA_SU_PERFCOUNTER0_SELECT1__CI__VI,
                   (mmPA_SU_PERFCOUNTER1_SELECT1__CI__VI - mmPA_SU_PERFCOUNTER0_SELECT1__CI__VI),
                   mmPA_SU_PERFCOUNTER0_LO__CI__VI, mmPA_SU_PERFCOUNTER0_HI__CI__VI,
                   (mmPA_SU_PERFCOUNTER1_LO__CI__VI - mmPA_SU_PERFCOUNTER0_LO__CI__VI));

    // SC block
    SetupBlockInfo(pProps, GpuBlock::Sc,
                   shaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx8NumScCounters, Gfx8PerfCtrScMaxEvent,
                   mmPA_SC_PERFCOUNTER0_SELECT__CI__VI,
                   (mmPA_SC_PERFCOUNTER1_SELECT__CI__VI - mmPA_SC_PERFCOUNTER0_SELECT__CI__VI),
                   mmPA_SC_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmPA_SC_PERFCOUNTER0_LO__CI__VI, mmPA_SC_PERFCOUNTER0_HI__CI__VI,
                   (mmPA_SC_PERFCOUNTER1_LO__CI__VI - mmPA_SC_PERFCOUNTER0_LO__CI__VI));

    // SX block
    SetupBlockInfo(pProps, GpuBlock::Sx,
                   shaderEngines, shaderArrays, DefaultInstances,
                   Gfx8NumSxCounters, Gfx8PerfCtrSxMaxEvent,
                   mmSX_PERFCOUNTER0_SELECT__CI__VI,
                   (mmSX_PERFCOUNTER1_SELECT__CI__VI - mmSX_PERFCOUNTER0_SELECT__CI__VI),
                   mmSX_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmSX_PERFCOUNTER0_LO__CI__VI, mmSX_PERFCOUNTER0_HI__CI__VI,
                   (mmSX_PERFCOUNTER1_LO__CI__VI - mmSX_PERFCOUNTER0_LO__CI__VI));

    // SPI block
    SetupBlockInfo(pProps, GpuBlock::Spi,
                   shaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx8NumSpiCounters, Gfx8PerfCtrSpiMaxEvent,
                   mmSPI_PERFCOUNTER0_SELECT__CI__VI,
                   (mmSPI_PERFCOUNTER1_SELECT__CI__VI - mmSPI_PERFCOUNTER0_SELECT__CI__VI),
                   mmSPI_PERFCOUNTER0_SELECT1__CI__VI,
                   (mmSPI_PERFCOUNTER1_SELECT1__CI__VI - mmSPI_PERFCOUNTER0_SELECT1__CI__VI),
                   mmSPI_PERFCOUNTER0_LO__CI__VI, mmSPI_PERFCOUNTER0_HI__CI__VI,
                   (mmSPI_PERFCOUNTER1_LO__CI__VI - mmSPI_PERFCOUNTER0_LO__CI__VI));

    // SQ block
    SetupBlockInfo(pProps, GpuBlock::Sq,
                   shaderEngines, SqShaderArrays, SqInstances,
                   Gfx8NumSqCounters,
                   AMDGPU_IS_FIJI(pProps->familyId, pProps->eRevId) ? Gfx8PerfCtrSqMaxEventFiji : Gfx8PerfCtrSqMaxEvent,
                   mmSQ_PERFCOUNTER0_SELECT__CI__VI,
                   (mmSQ_PERFCOUNTER1_SELECT__CI__VI - mmSQ_PERFCOUNTER0_SELECT__CI__VI),
                   0, 0,
                   mmSQ_PERFCOUNTER0_LO__CI__VI, mmSQ_PERFCOUNTER0_HI__CI__VI,
                   (mmSQ_PERFCOUNTER1_LO__CI__VI - mmSQ_PERFCOUNTER0_LO__CI__VI));

    // TA block
    SetupBlockInfo(pProps, GpuBlock::Ta,
                   shaderEngines, shaderArrays, numCuPerSh,
                   Gfx8NumTaCounters, Gfx8PerfCtrTaMaxEvent,
                   mmTA_PERFCOUNTER0_SELECT__CI__VI,
                   (mmTA_PERFCOUNTER1_SELECT__CI__VI - mmTA_PERFCOUNTER0_SELECT__CI__VI),
                   mmTA_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmTA_PERFCOUNTER0_LO__CI__VI, mmTA_PERFCOUNTER0_HI__CI__VI,
                   (mmTA_PERFCOUNTER1_LO__CI__VI - mmTA_PERFCOUNTER0_LO__CI__VI));

    // TD block
    SetupBlockInfo(pProps, GpuBlock::Td,
                   shaderEngines, shaderArrays, numCuPerSh,
                   Gfx8NumTdCounters, Gfx8PerfCtrTdMaxEvent,
                   mmTD_PERFCOUNTER0_SELECT__CI__VI,
                   (mmTD_PERFCOUNTER1_SELECT__CI__VI - mmTD_PERFCOUNTER0_SELECT__CI__VI),
                   mmTD_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmTD_PERFCOUNTER0_LO__CI__VI, mmTD_PERFCOUNTER0_HI__CI__VI, 0);

    // TCP block
    SetupBlockInfo(pProps, GpuBlock::Tcp,
                   shaderEngines, shaderArrays, numCuPerSh,
                   Gfx8NumTcpCounters, Gfx8PerfCtrTcpMaxEvent,
                   mmTCP_PERFCOUNTER0_SELECT__CI__VI,
                   (mmTCP_PERFCOUNTER1_SELECT__CI__VI - mmTCP_PERFCOUNTER0_SELECT__CI__VI),
                   mmTCP_PERFCOUNTER0_SELECT1__CI__VI,
                   (mmTCP_PERFCOUNTER1_SELECT1__CI__VI - mmTCP_PERFCOUNTER0_SELECT1__CI__VI),
                   mmTCP_PERFCOUNTER0_LO__CI__VI, mmTCP_PERFCOUNTER0_HI__CI__VI,
                   (mmTCP_PERFCOUNTER1_LO__CI__VI - mmTCP_PERFCOUNTER0_LO__CI__VI));

    // TCC block
    SetupBlockInfo(pProps, GpuBlock::Tcc,
                   DefaultShaderEngines, DefaultShaderArrays, pProps->gfx6.numTccBlocks,
                   Gfx8NumTccCounters, Gfx8PerfCtrTccMaxEvent,
                   mmTCC_PERFCOUNTER0_SELECT__CI__VI,
                   (mmTCC_PERFCOUNTER1_SELECT__CI__VI - mmTCC_PERFCOUNTER0_SELECT__CI__VI),
                   mmTCC_PERFCOUNTER0_SELECT1__CI__VI,
                   (mmTCC_PERFCOUNTER1_SELECT1__CI__VI - mmTCC_PERFCOUNTER0_SELECT1__CI__VI),
                   mmTCC_PERFCOUNTER0_LO__CI__VI, mmTCC_PERFCOUNTER0_HI__CI__VI,
                   (mmTCC_PERFCOUNTER1_LO__CI__VI - mmTCC_PERFCOUNTER0_LO__CI__VI));

    // TCA block
    SetupBlockInfo(pProps, GpuBlock::Tca,
                   DefaultShaderEngines, DefaultShaderArrays, TcaInstances,
                   Gfx8NumTcaCounters, Gfx8PerfCtrTcaMaxEvent,
                   mmTCA_PERFCOUNTER0_SELECT__CI__VI,
                   (mmTCA_PERFCOUNTER1_SELECT__CI__VI - mmTCA_PERFCOUNTER0_SELECT__CI__VI),
                   mmTCA_PERFCOUNTER0_SELECT1__CI__VI,
                   (mmTCA_PERFCOUNTER1_SELECT1__CI__VI - mmTCA_PERFCOUNTER0_SELECT1__CI__VI),
                   mmTCA_PERFCOUNTER0_LO__CI__VI, mmTCA_PERFCOUNTER0_HI__CI__VI,
                   (mmTCA_PERFCOUNTER1_LO__CI__VI - mmTCA_PERFCOUNTER0_LO__CI__VI));

    // GDS block
    SetupBlockInfo(pProps, GpuBlock::Gds,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx8NumGdsCounters, Gfx8PerfCtrGdsMaxEvent,
                   mmGDS_PERFCOUNTER0_SELECT__CI__VI,
                   (mmGDS_PERFCOUNTER1_SELECT__CI__VI - mmGDS_PERFCOUNTER0_SELECT__CI__VI),
                   mmGDS_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmGDS_PERFCOUNTER0_LO__CI__VI, mmGDS_PERFCOUNTER0_HI__CI__VI,
                   (mmGDS_PERFCOUNTER1_LO__CI__VI - mmGDS_PERFCOUNTER0_LO__CI__VI));

    // VGT block
    SetupBlockInfo(pProps, GpuBlock::Vgt,
                   shaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx8NumVgtCounters, Gfx8PerfCtrVgtMaxEvent,
                   mmVGT_PERFCOUNTER0_SELECT__CI__VI,
                   (mmVGT_PERFCOUNTER1_SELECT__CI__VI - mmVGT_PERFCOUNTER0_SELECT__CI__VI),
                   mmVGT_PERFCOUNTER0_SELECT1__CI__VI,
                   (mmVGT_PERFCOUNTER1_SELECT1__CI__VI - mmVGT_PERFCOUNTER0_SELECT1__CI__VI),
                   mmVGT_PERFCOUNTER0_LO__CI__VI, mmVGT_PERFCOUNTER0_HI__CI__VI,
                   (mmVGT_PERFCOUNTER1_LO__CI__VI - mmVGT_PERFCOUNTER0_LO__CI__VI));

    // IA block
    SetupBlockInfo(pProps, GpuBlock::Ia,
                   Max(shaderEngines / 2U, 1U), DefaultShaderArrays, DefaultInstances,
                   Gfx8NumIaCounters, Gfx8PerfCtrIaMaxEvent,
                   mmIA_PERFCOUNTER0_SELECT__CI__VI,
                   (mmIA_PERFCOUNTER1_SELECT__CI__VI - mmIA_PERFCOUNTER0_SELECT__CI__VI),
                   mmIA_PERFCOUNTER0_SELECT1__CI__VI, 0,
                   mmIA_PERFCOUNTER0_LO__CI__VI, mmIA_PERFCOUNTER0_HI__CI__VI,
                   (mmIA_PERFCOUNTER1_LO__CI__VI - mmIA_PERFCOUNTER0_LO__CI__VI));

    // WD block
    SetupBlockInfo(pProps, GpuBlock::Wd,
                   DefaultShaderEngines, DefaultShaderArrays, DefaultInstances,
                   Gfx8NumWdCounters, Gfx8PerfCtrWdMaxEvent,
                   mmWD_PERFCOUNTER0_SELECT__CI__VI,
                   (mmWD_PERFCOUNTER1_SELECT__CI__VI - mmWD_PERFCOUNTER0_SELECT__CI__VI),
                   0, 0,
                   mmWD_PERFCOUNTER0_LO__CI__VI, mmWD_PERFCOUNTER0_HI__CI__VI,
                   (mmWD_PERFCOUNTER1_LO__CI__VI - mmWD_PERFCOUNTER0_LO__CI__VI));

    // MC block
    SetupMcBlockAndRegInfo(pProps);

    // SDMA block
    SetupBlockInfo(pProps, GpuBlock::Dma,
                   DefaultShaderEngines, DefaultShaderArrays, SdmaInstances,
                   Gfx8NumSdmaCounters, Gfx8PerfCtrSdmaMaxEvent,
                   mmSDMA0_PERFMON_CNTL__VI,
                   (mmSDMA1_PERFMON_CNTL__VI - mmSDMA0_PERFMON_CNTL__VI),
                   0, 0,
                   mmSDMA0_PERFCOUNTER0_RESULT__VI,
                   mmSDMA0_PERFCOUNTER1_RESULT__VI,
                   (mmSDMA1_PERFCOUNTER0_RESULT__VI - mmSDMA0_PERFCOUNTER1_RESULT__VI));
}

// =====================================================================================================================
// Initializes the performance counter information for an adapter structure, specifically for the Gfx6-Gfx8 hardware
// layer.
void InitPerfCtrInfo(
    GpuChipProperties* pProps)
{
    Gfx6PerfCounterInfo*const pInfo = &pProps->gfx6.perfCounterInfo;

    // All current GFX6 hardware is affected by "SPI not differentiating pkr_id for newwave commands".
    pInfo->features.sqttBadScPackerId = 1;

    switch (pProps->gfxLevel)
    {
    case GfxIpLevel::GfxIp6:
        pInfo->features.counters    = 1;
        pInfo->features.threadTrace = 1;
        SetupGfx6Counters(pProps);
        break;

    case GfxIpLevel::GfxIp7:
        pInfo->features.counters    = 1;
        pInfo->features.threadTrace = 1;
        pInfo->features.spmTrace    = 1;
        SetupGfx7Counters(pProps);
        break;

    case GfxIpLevel::GfxIp8:
    case GfxIpLevel::GfxIp8_1:
        pInfo->features.counters    = 1;
        pInfo->features.threadTrace = 1;
        pInfo->features.spmTrace    = 1;

        // Only Fiji is known to support PS1 event tokens in thread traces.
        pInfo->features.supportPs1Events = AMDGPU_IS_FIJI(pProps->familyId, pProps->eRevId);

        SetupGfx8Counters(pProps);
        break;

    default:
        PAL_NEVER_CALLED();
        break;
    }
}

// =====================================================================================================================
// Validates the value of a thread-trace creation option.
Result ValidateThreadTraceOptions(
    const Device&        device,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 373
    const PerfTraceInfo& info)
#else
    const ThreadTraceInfo& info)
#endif
{
    const GpuChipProperties& chipProps = device.ChipProperties();
    const GfxIpLevel gfxIpLevel        = chipProps.gfxLevel;

    Result result = Result::Success;

    const auto& flags  = info.optionFlags;
    const auto& values = info.optionValues;

    if ((flags.bufferSize) &&
        ((values.bufferSize > MaximumBufferSize) ||
         (Pow2Align(values.bufferSize, BufferAlignment) != values.bufferSize)))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success) &&
        (flags.threadTraceTokenMask) &&
        ((values.threadTraceTokenMask & TokenMaskAll) != values.threadTraceTokenMask))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success) &&
        (flags.threadTraceRegMask) &&
        ((values.threadTraceRegMask & RegMaskAll) != values.threadTraceRegMask))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success) &&
        (flags.threadTraceTargetSh) &&
        (values.threadTraceTargetSh >= chipProps.gfx6.numShaderArrays))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success) &&
        (flags.threadTraceTargetCu) &&
        (values.threadTraceTargetCu >= chipProps.gfx6.numCuPerSh))
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

    if ((result == Result::Success)   &&
        (flags.threadTraceVmIdMask)   &&
        (values.threadTraceVmIdMask > SQ_THREAD_TRACE_VM_ID_MASK_SINGLE_DETAIL))
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success)   &&
        (flags.threadTraceRandomSeed) &&
        (values.threadTraceRandomSeed > MaximumRandomSeed))
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
        (((gfxIpLevel != GfxIpLevel::GfxIp6) &&
          (values.threadTraceIssueMask > SQ_THREAD_TRACE_ISSUE_MASK_IMMED__CI__VI)) ||
         ((gfxIpLevel == GfxIpLevel::GfxIp6) &&
          (values.threadTraceIssueMask > SQ_THREAD_TRACE_ISSUE_MASK_STALLED_AND_IMMED))))
    {
        result = Result::ErrorInvalidValue;
    }

    return result;
}

// =====================================================================================================================
// Validates the spm trace configuration.
Result ValidateSpmTraceOptions(
    const Device&             device,
    const SpmTraceCreateInfo& info)
{
    Result result = Result::ErrorInvalidValue;

    auto pChipProps = &device.ChipProperties();
    auto pPerfCounterInfo  = &pChipProps->gfx6.perfCounterInfo;

    for (uint32 i = 0; i < info.numPerfCounters; i++)
    {
        auto blockIdx = static_cast<uint32>(info.pPerfCounterInfos[i].block);

        // Check if block, eventid and instance number are within bounds.
        if ((info.pPerfCounterInfos[i].block < GpuBlock::Count) &&
            (info.pPerfCounterInfos[i].eventId < pPerfCounterInfo->block[blockIdx].maxEventId) &&
            (info.pPerfCounterInfos[i].instance < (pPerfCounterInfo->block[blockIdx].numInstances *
             pPerfCounterInfo->block[blockIdx].numShaderEngines)))
        {
            result = Result::Success;
        }
        else
        {
            break;
        }
    }

    PAL_ALERT(result == Result::Success);

    return result;
}

} // PerfExperiment
} // Gfx6
} // Pal
