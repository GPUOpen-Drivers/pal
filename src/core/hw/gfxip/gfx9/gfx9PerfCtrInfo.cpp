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

#include "core/device.h"
#include "core/hw/gfxip/gfx9/gfx9PerfCtrInfo.h"
#include "palMath.h"
#include "palPerfExperiment.h"

#include "core/hw/amdgpu_asic.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{
namespace PerfCtrInfo
{

struct  BlockPerfCounterInfo
{
    uint32  numRegs;                             // Number of counter registers in this block instance.
    uint32  numTotalStreamingCounterRegs;        // Number of streaming counter registers in this block.
    uint32  numStreamingCounters;                // Num streaming counters this SELECT(0/1) configures.
    uint32  regOffsets[MaxCountersPerBlock]; // Address offsets for all counters in this block.
};

// Table of all the primary perf-counter select registers.  We list all the register offsets since the delta's
// between registers are not consistent.
static constexpr BlockPerfCounterInfo Gfx9PerfCountSelect0[] =
{
    { Gfx9NumCpfCounters, 1, 2,    { mmCPF_PERFCOUNTER0_SELECT,
                                     mmCPF_PERFCOUNTER1_SELECT            }, },   // cpf
    { Gfx9NumIaCounters, 1, 2,     { mmIA_PERFCOUNTER0_SELECT__GFX09,
                                     mmIA_PERFCOUNTER1_SELECT__GFX09,
                                     mmIA_PERFCOUNTER2_SELECT__GFX09,
                                     mmIA_PERFCOUNTER3_SELECT__GFX09      }, },   // ia
   // NOTE: The perfmon doc claims DB/PA/TCP/VGT each have six streaming counters, though the regspec
   //       indicates there is room for eight.
    { Gfx9NumVgtCounters, 2, 3,    { mmVGT_PERFCOUNTER0_SELECT__GFX09,
                                     mmVGT_PERFCOUNTER1_SELECT__GFX09,
                                     mmVGT_PERFCOUNTER2_SELECT__GFX09,
                                     mmVGT_PERFCOUNTER3_SELECT__GFX09     }, },  // vgt
    { Gfx9NumPaCounters, 2, 3,     { mmPA_SU_PERFCOUNTER0_SELECT,
                                     mmPA_SU_PERFCOUNTER1_SELECT,
                                     mmPA_SU_PERFCOUNTER2_SELECT,
                                     mmPA_SU_PERFCOUNTER3_SELECT__GFX09   }, },  // pa
    { Gfx9NumScCounters, 1, 2,     { mmPA_SC_PERFCOUNTER0_SELECT,
                                     mmPA_SC_PERFCOUNTER1_SELECT,
                                     mmPA_SC_PERFCOUNTER2_SELECT,
                                     mmPA_SC_PERFCOUNTER3_SELECT,
                                     mmPA_SC_PERFCOUNTER4_SELECT,
                                     mmPA_SC_PERFCOUNTER5_SELECT,
                                     mmPA_SC_PERFCOUNTER6_SELECT,
                                     mmPA_SC_PERFCOUNTER7_SELECT,         }, },  // sc
    { Gfx9NumSpiCounters, 4, 8,    { mmSPI_PERFCOUNTER0_SELECT,
                                     mmSPI_PERFCOUNTER1_SELECT,
                                     mmSPI_PERFCOUNTER2_SELECT,
                                     mmSPI_PERFCOUNTER3_SELECT,
                                     mmSPI_PERFCOUNTER4_SELECT,
                                     mmSPI_PERFCOUNTER5_SELECT,           }, },  // spi
    { Gfx9NumSqCounters, 16, 16,   { mmSQ_PERFCOUNTER0_SELECT,
                                     mmSQ_PERFCOUNTER1_SELECT,
                                     mmSQ_PERFCOUNTER2_SELECT,
                                     mmSQ_PERFCOUNTER3_SELECT,
                                     mmSQ_PERFCOUNTER4_SELECT,
                                     mmSQ_PERFCOUNTER5_SELECT,
                                     mmSQ_PERFCOUNTER6_SELECT,
                                     mmSQ_PERFCOUNTER7_SELECT,
                                     mmSQ_PERFCOUNTER8_SELECT,
                                     mmSQ_PERFCOUNTER9_SELECT,
                                     mmSQ_PERFCOUNTER10_SELECT,
                                     mmSQ_PERFCOUNTER11_SELECT,
                                     mmSQ_PERFCOUNTER12_SELECT,
                                     mmSQ_PERFCOUNTER13_SELECT,
                                     mmSQ_PERFCOUNTER14_SELECT,
                                     mmSQ_PERFCOUNTER15_SELECT,           }, }, // sq
    { Gfx9NumSxCounters, 2, 4,     { mmSX_PERFCOUNTER0_SELECT,
                                     mmSX_PERFCOUNTER1_SELECT,
                                     mmSX_PERFCOUNTER2_SELECT,
                                     mmSX_PERFCOUNTER3_SELECT             }, }, // sx
    { Gfx9NumTaCounters, 1, 2,     { mmTA_PERFCOUNTER0_SELECT,
                                     mmTA_PERFCOUNTER1_SELECT,            }, }, // ta
    { Gfx9NumTdCounters, 1, 2,     { mmTD_PERFCOUNTER0_SELECT,
                                     mmTD_PERFCOUNTER1_SELECT,            }, }, // td
    { Gfx9NumTcpCounters, 2, 3,    { mmTCP_PERFCOUNTER0_SELECT,
                                     mmTCP_PERFCOUNTER1_SELECT,
                                     mmTCP_PERFCOUNTER2_SELECT,
                                     mmTCP_PERFCOUNTER3_SELECT,           }, }, // tcp
    { Gfx9NumTccCounters, 2, 4,    { mmTCC_PERFCOUNTER0_SELECT__GFX09,
                                     mmTCC_PERFCOUNTER1_SELECT__GFX09,
                                     mmTCC_PERFCOUNTER2_SELECT__GFX09,
                                     mmTCC_PERFCOUNTER3_SELECT__GFX09,    }, }, // tcc
    { Gfx9NumTcaCounters, 2, 4,    { mmTCA_PERFCOUNTER0_SELECT__GFX09,
                                     mmTCA_PERFCOUNTER1_SELECT__GFX09,
                                     mmTCA_PERFCOUNTER2_SELECT__GFX09,
                                     mmTCA_PERFCOUNTER3_SELECT__GFX09,    }, }, // tca
    { Gfx9NumDbCounters, 2, 3,     { mmDB_PERFCOUNTER0_SELECT,
                                     mmDB_PERFCOUNTER1_SELECT,
                                     mmDB_PERFCOUNTER2_SELECT,
                                     mmDB_PERFCOUNTER3_SELECT,            }, },  // db
    { Gfx9NumCbCounters, 1, 2,     { mmCB_PERFCOUNTER0_SELECT,
                                     mmCB_PERFCOUNTER1_SELECT,
                                     mmCB_PERFCOUNTER2_SELECT,
                                     mmCB_PERFCOUNTER3_SELECT,            }, },  // cb
    { Gfx9NumGdsCounters, 1, 2,    { mmGDS_PERFCOUNTER0_SELECT,
                                     mmGDS_PERFCOUNTER1_SELECT,
                                     mmGDS_PERFCOUNTER2_SELECT,
                                     mmGDS_PERFCOUNTER3_SELECT,           }, },  // gds
    { 0, 0, 0,                     { 0,                                   }, },  // srbm, doesn't exist
    { Gfx9NumGrbmCounters, 0, 0,   { mmGRBM_PERFCOUNTER0_SELECT,
                                     mmGRBM_PERFCOUNTER1_SELECT,          }, },  // grbm
    { Gfx9NumGrbmseCounters, 0, 0, { mmGRBM_SE0_PERFCOUNTER_SELECT,
                                     mmGRBM_SE1_PERFCOUNTER_SELECT,
                                     mmGRBM_SE2_PERFCOUNTER_SELECT,
                                     mmGRBM_SE3_PERFCOUNTER_SELECT        }, },  // grbm=se
    { Gfx9NumRlcCounters, 0, 0,    { mmRLC_PERFCOUNTER0_SELECT,
                                     mmRLC_PERFCOUNTER1_SELECT,           }, },  // rlc
    { Gfx9NumSdmaCounters, 0, 0,   { mmSDMA0_PERFMON_CNTL,
                                     mmSDMA1_PERFMON_CNTL__GFX09,         }, },  // sdma
    { 0, 0, 0,                     { 0,                                   }, },  // mc
    { Gfx9NumCpgCounters, 1, 2,    { mmCPG_PERFCOUNTER0_SELECT,
                                     mmCPG_PERFCOUNTER1_SELECT,           }, },  // cpg
    { Gfx9NumCpcCounters, 1, 2,    { mmCPC_PERFCOUNTER0_SELECT,
                                     mmCPC_PERFCOUNTER1_SELECT,           }, },  // cpc
    { Gfx9NumWdCounters, 0, 0,     { mmWD_PERFCOUNTER0_SELECT__GFX09,
                                     mmWD_PERFCOUNTER1_SELECT__GFX09,
                                     mmWD_PERFCOUNTER2_SELECT__GFX09,
                                     mmWD_PERFCOUNTER3_SELECT__GFX09,     }, },  // wd
    { 0, 0, 0,                     { 0,                                   }, },  // tcs
    { Gfx9NumAtcCounters, 0, 0,    { mmATC_PERFCOUNTER0_CFG__GFX09,
                                     mmATC_PERFCOUNTER1_CFG__GFX09,
                                     mmATC_PERFCOUNTER2_CFG__GFX09,
                                     mmATC_PERFCOUNTER3_CFG__GFX09,       }, },  // atc
    { Gfx9NumAtcL2Counters, 0, 0,  { mmATC_L2_PERFCOUNTER0_CFG__GFX09,
                                     mmATC_L2_PERFCOUNTER1_CFG__GFX09,    }, },  // atc l2
    { Gfx9NumMcVmL2Counters, 0, 0, { mmMC_VM_L2_PERFCOUNTER0_CFG__GFX09,
                                     mmMC_VM_L2_PERFCOUNTER1_CFG__GFX09,
                                     mmMC_VM_L2_PERFCOUNTER2_CFG__GFX09,
                                     mmMC_VM_L2_PERFCOUNTER3_CFG__GFX09,
                                     mmMC_VM_L2_PERFCOUNTER4_CFG__GFX09,
                                     mmMC_VM_L2_PERFCOUNTER5_CFG__GFX09,
                                     mmMC_VM_L2_PERFCOUNTER6_CFG__GFX09,
                                     mmMC_VM_L2_PERFCOUNTER7_CFG__GFX09,  }, },  // mc vm l2
    { Gfx9NumEaCounters, 0, 0,     { mmGCEA_PERFCOUNTER0_CFG__GFX09,
                                     mmGCEA_PERFCOUNTER1_CFG__GFX09       }, },  // ea
    { Gfx9NumRpbCounters, 0, 0,    { mmRPB_PERFCOUNTER0_CFG__GFX09,
                                     mmRPB_PERFCOUNTER1_CFG__GFX09,
                                     mmRPB_PERFCOUNTER2_CFG__GFX09,
                                     mmRPB_PERFCOUNTER3_CFG__GFX09,       }, },  // rpb
    { Gfx9NumRmiCounters, 1, 2,    { mmRMI_PERFCOUNTER0_SELECT,
                                     mmRMI_PERFCOUNTER1_SELECT,
                                     mmRMI_PERFCOUNTER2_SELECT,
                                     mmRMI_PERFCOUNTER3_SELECT,           }, },  // rmi
};

static_assert(sizeof(Gfx9PerfCountSelect0)/sizeof(Gfx9PerfCountSelect0[0])   == static_cast<uint32>(GpuBlock::Count),
              "Gfx9PerfCountSelect0 must have one entry for each GpuBlock.");

// Table of all the secondary perf-counter select registers.  We list all the register offsets since the delta's
// between registers are not consistent.
static constexpr BlockPerfCounterInfo Gfx9PerfCountSelect1[] =
{
    { 1, 1, 2,               { mmCPF_PERFCOUNTER0_SELECT1,          }, },  // cpf
    { 1, 1, 2,               { mmIA_PERFCOUNTER0_SELECT1__GFX09,    }, },  // ia
    { 2, 1, 2,               { mmVGT_PERFCOUNTER0_SELECT1__GFX09,
                               mmVGT_PERFCOUNTER1_SELECT1__GFX09,   }, },  // vgt
    { 2, 2, 3,               { mmPA_SU_PERFCOUNTER0_SELECT1,
                               mmPA_SU_PERFCOUNTER1_SELECT1,        }, },  // pa
    { 1, 1, 2,               { mmPA_SC_PERFCOUNTER0_SELECT1,        }, },  // sc
    { 4, 4, 8,               { mmSPI_PERFCOUNTER0_SELECT1,
                               mmSPI_PERFCOUNTER1_SELECT1,
                               mmSPI_PERFCOUNTER2_SELECT1,
                               mmSPI_PERFCOUNTER3_SELECT1,          }, },  // spi
    { 0, 16, 0,              { 0,                                   }, },  // sq
    { 2, 2, 4,               { mmSX_PERFCOUNTER0_SELECT1,
                               mmSX_PERFCOUNTER1_SELECT1,           }, },  // sx
    { 1, 1, 2,               { mmTA_PERFCOUNTER0_SELECT1,           }, },  // ta
    { 1, 1, 2,               { mmTD_PERFCOUNTER0_SELECT1,           }, },  // td
    { 2, 2, 3,               { mmTCP_PERFCOUNTER0_SELECT1,
                               mmTCP_PERFCOUNTER1_SELECT1,          }, },  // tcp
    { 2, 2, 4,               { mmTCC_PERFCOUNTER0_SELECT1__GFX09,
                               mmTCC_PERFCOUNTER1_SELECT1__GFX09,   }, },  // tcc
    { 2, 2, 4,               { mmTCA_PERFCOUNTER0_SELECT1__GFX09,
                               mmTCA_PERFCOUNTER1_SELECT1__GFX09,   }, },  // tca
    { 2, 2, 3,               { mmDB_PERFCOUNTER0_SELECT1,
                               mmDB_PERFCOUNTER1_SELECT1,           }, },  // db
    { 1, 1, 2,               { mmCB_PERFCOUNTER0_SELECT1,           }, },  // cb
    { 1, 1, 2,               { mmGDS_PERFCOUNTER0_SELECT1,          }, },  // gds
    { 0, 0, 0,               { 0,                                   }, },  // srbm, doesn't exist
    { 0, 0, 0,               { 0,                                   }, },  // grbm
    { 0, 0, 0,               { 0                                    }, },  // grbm-se
    { 0, 0, 0,               { 0                                    }, },  // rlc
    { 0, 0, 0,               { 0                                    }, },  // sdma
    { 0, 0, 0,               { 0,                                   }, },  // mc,
    { 1, 1, 2,               { mmCPG_PERFCOUNTER0_SELECT1,          }, },  // cpg
    { 1, 1, 2,               { mmCPC_PERFCOUNTER0_SELECT1,          }, },  // cpc
    { 0, 0, 0,               { 0                                    }, },  // wd
    { 0, 0, 0,               { 0,                                   }, },  // tcs
    { 0, 0, 0,               { 0,                                   }, },  // atc
    { 0, 0, 0,               { 0,                                   }, },  // atcL2
    { 0, 0, 0,               { 0,                                   }, },  // mcVmL2
    { 0, 0, 0,               { 0,                                   }, },  // ea
    { 0, 0, 0,               { 0,                                   }, },  // rpb
    { 4, 1, 2,               { mmRMI_PERFCOUNTER0_SELECT1,
                               0,
                               mmRMI_PERFCOUNTER2_SELECT1,
                               0,                                   }, },  // rmi

};

static_assert(sizeof(Gfx9PerfCountSelect1)/sizeof(Gfx9PerfCountSelect1[0])   == static_cast<uint32>(GpuBlock::Count),
              "Gfx9PerfCountSelect1 must have one entry for each GpuBlock.");

// =====================================================================================================================
const BlockPerfCounterInfo* GetPrimaryBlockCounterInfo(
    const GpuChipProperties* pProps,
    GpuBlock                 block)
{
    const uint32                 blockIdx          = static_cast<uint32>(block);
    const BlockPerfCounterInfo*  pBlockCounterInfo = ((pProps->gfxLevel == GfxIpLevel::GfxIp9)
                                                      ? &Gfx9PerfCountSelect0[blockIdx]
                                                      : nullptr);

    PAL_ASSERT(pBlockCounterInfo != nullptr);

    return pBlockCounterInfo;
}

// =====================================================================================================================
const BlockPerfCounterInfo* GetSecondaryBlockCounterInfo(
    const GpuChipProperties* pProps,
    GpuBlock                 block)
{
    const uint32                 blockIdx          = static_cast<uint32>(block);
    const BlockPerfCounterInfo*  pBlockCounterInfo = ((pProps->gfxLevel == GfxIpLevel::GfxIp9)
                                                      ? &Gfx9PerfCountSelect1[blockIdx]
                                                      : nullptr);

    PAL_ASSERT(pBlockCounterInfo != nullptr);

    return pBlockCounterInfo;
}

// =====================================================================================================================
// Returns the number of performance counters supported by the specified block.
uint32 GetMaxEventId(
    GpuChipProperties* pProps,
    GpuBlock           block)
{
    const uint32  blockIdx = static_cast<uint32>(block);

    uint32  maxEventId = 0;
    if (pProps->gfxLevel == GfxIpLevel::GfxIp9)
    {
        static constexpr uint32  MaxEventId[static_cast<uint32>(GpuBlock::Count)] =
        {
            Gfx9PerfCtrCpfMaxEvent,
            Gfx9PerfCtrIaMaxEvent,
            Gfx9PerfCtrVgtMaxEvent,
            Gfx9PerfCtrPaMaxEvent,
            Gfx9PerfCtrScMaxEvent,
            Gfx9PerfCtrSpiMaxEvent,
            Gfx9PerfCtrSqMaxEvent,
            Gfx9PerfCtrSxMaxEvent,
            Gfx9PerfCtrTaMaxEvent,
            Gfx9PerfCtrTdMaxEvent,
            Gfx9PerfCtrTcpMaxEvent,
            Gfx9PerfCtrTccMaxEvent,
            Gfx9PerfCtrTcaMaxEvent,
            Gfx9PerfCtrDbMaxEvent,
            Gfx9PerfCtrCbMaxEvent,
            Gfx9PerfCtrGdsMaxEvent,
            0, // Srbm,
            Gfx9PerfCtrGrbmMaxEvent,
            Gfx9PerfCtrGrbmseMaxEvent,
            Gfx9PerfCtrRlcMaxEvent,
            Gfx9PerfCtrSdmaMaxEvent,
            Gfx9PerfCtrlEaMaxEvent,
            Gfx9PerfCtrCpgMaxEvent,
            Gfx9PerfCtrCpcMaxEvent,
            Gfx9PerfCtrWdMaxEvent,
            0, // Tcs,
            Gfx9PerfCtrlAtcMaxEvent,
            Gfx9PerfCtrlAtcL2MaxEvent,
            Gfx9PerfCtrlMcVmL2MaxEvent,
            Gfx9PerfCtrlEaMaxEvent,
            Gfx9PerfCtrlRpbMaxEvent,
            Gfx9PerfCtrRmiMaxEvent,
        };

        maxEventId = MaxEventId[blockIdx];
    }

    // Why is the caller setting up a block that doesn't have any event ID's associated with it?
    PAL_ASSERT(maxEventId != 0);

    return maxEventId;
}

// =====================================================================================================================
// Helper function to initialize the performance counter information for a specific GPU block.
void SetupBlockInfo(
    GpuChipProperties* pProps,
    GpuBlock           block,            // Block identifier
    uint32             numShaderEngines, // Number of shader engines for this block
    uint32             numShaderArrays,  // Number of shader arrays for this block
    uint32             numInstances,     // Number of instances per shader array, except for SQ block(# of SQG per SH)
    uint32             ctrLoRegAddr,     // Low counter data address register address
    uint32             ctrHiRegAddr,     // High counter data address register address
    int32              ctrRegIncr)       // Counter data register block address increment
{
    const uint32              blockIdx = static_cast<uint32>(block);
    const auto*               pSelReg0 = GetPrimaryBlockCounterInfo(pProps, block);
    const auto*               pSelReg1 = GetSecondaryBlockCounterInfo(pProps, block);
    Gfx9PerfCounterInfo*const pInfo    = &pProps->gfx9.perfCounterInfo;

    PAL_ASSERT(pSelReg0->numRegs <= MaxCountersPerBlock);

    pInfo->block[blockIdx].available               = true;
    pInfo->block[blockIdx].numShaderEngines        = numShaderEngines;
    pInfo->block[blockIdx].numShaderArrays         = numShaderArrays;
    pInfo->block[blockIdx].numInstances            = numInstances;
    pInfo->block[blockIdx].numCounters             = pSelReg0->numRegs;
    pInfo->block[blockIdx].numStreamingCounters    = pSelReg0->numStreamingCounters + pSelReg1->numStreamingCounters;
    pInfo->block[blockIdx].numStreamingCounterRegs = pSelReg0->numTotalStreamingCounterRegs;
    pInfo->block[blockIdx].maxEventId              = GetMaxEventId(pProps, block);

    // Setup the register addresses for each counter for this block.
    for (uint32  idx = 0; idx < pSelReg0->numRegs; idx++)
    {
        pInfo->block[blockIdx].regInfo[idx].perfSel0RegAddr = pSelReg0->regOffsets[idx];

        pInfo->block[blockIdx].regInfo[idx].perfCountLoAddr = ctrLoRegAddr + idx * ctrRegIncr;
        pInfo->block[blockIdx].regInfo[idx].perfCountHiAddr = ctrHiRegAddr + idx * ctrRegIncr;
    }

    for (uint32  idx = 0; idx < pSelReg1->numRegs; idx++)
    {
        pInfo->block[blockIdx].regInfo[idx].perfSel1RegAddr = pSelReg1->regOffsets[idx];
    }
}

// =====================================================================================================================
// Helper function to initialize the performance counter information for memory system GPU blocks.
void SetupMcSysBlockInfo(
    GpuChipProperties*           pProps,
    GpuBlock                     block,            // Block identifier
    uint32                       numShaderEngines, // Number of shader engines for this block
    uint32                       numShaderArrays,  // Number of shader arrays for this block
    uint32                       numInstances,     // Number of instances per shader array
    uint32                       ctrLoRegAddr,     // Low counter data address register address
    uint32                       ctrHiRegAddr,     // High counter data address register address
    int32                        ctrRegIncr,       // Counter data register block address increment
    uint32                       rsltCntlRegAddr)  // Result control register for mem block types
{
    SetupBlockInfo(pProps,
                   block,
                   numShaderEngines,
                   numShaderArrays,
                   numInstances,
                   ctrLoRegAddr,
                   ctrHiRegAddr,
                   ctrRegIncr);

    const uint32              blockIdx = static_cast<uint32>(block) - 1;
    const auto*               pSelReg0 = GetPrimaryBlockCounterInfo(pProps, block);
    Gfx9PerfCounterInfo*const pInfo    = &pProps->gfx9.perfCounterInfo;

    // Set rsltCntlRegAddr
    for (uint32  idx = 0; idx < pSelReg0->numRegs; idx++)
    {
        pInfo->block[blockIdx].regInfo[idx].perfRsltCntlRegAddr = rsltCntlRegAddr;
    }
}

// =====================================================================================================================
// Initializes the performance counter information for common hardware blocks.
static void SetupHwlCounters(
    GpuChipProperties* pProps,
    uint32             defaultNumShaderEngines, // Number of shader engines for this device
    uint32             defaultNumShaderArrays,  // Number of shader arrays for this device
    uint32             defaultNumInstances)     // Num instances per shader array, except for SQ block(# of SQG per SH)
{
    const uint32 shaderEngines    = pProps->gfx9.numShaderEngines;
    const uint32 shaderArrays     = pProps->gfx9.numShaderArrays;
    const uint32 numCuPerSh       = pProps->gfx9.numCuPerSh;
    const uint32 rbPerShaderArray = (pProps->gfx9.maxNumRbPerSe / shaderArrays);
    const uint32 RmiInstances     = 2;

    // CPF block
    SetupBlockInfo(pProps,
                   GpuBlock::Cpf,
                   defaultNumShaderEngines,
                   defaultNumShaderArrays,
                   defaultNumInstances,
                   mmCPF_PERFCOUNTER0_LO,
                   mmCPF_PERFCOUNTER0_HI,
                   (mmCPF_PERFCOUNTER1_LO - mmCPF_PERFCOUNTER0_LO));

    // CPG block
    SetupBlockInfo(pProps,
                   GpuBlock::Cpg,
                   defaultNumShaderEngines,
                   defaultNumShaderArrays,
                   defaultNumInstances,
                   mmCPG_PERFCOUNTER0_LO,
                   mmCPG_PERFCOUNTER0_HI,
                   (mmCPG_PERFCOUNTER1_LO - mmCPG_PERFCOUNTER0_LO));

    // CPC block
    SetupBlockInfo(pProps,
                   GpuBlock::Cpc,
                   defaultNumShaderEngines,
                   defaultNumShaderArrays,
                   defaultNumInstances,
                   mmCPC_PERFCOUNTER0_LO,
                   mmCPC_PERFCOUNTER0_HI,
                   (mmCPC_PERFCOUNTER1_LO - mmCPC_PERFCOUNTER0_LO));

    // CB block
    SetupBlockInfo(pProps,
                   GpuBlock::Cb,
                   shaderEngines,
                   shaderArrays,
                   rbPerShaderArray,
                   mmCB_PERFCOUNTER0_LO,
                   mmCB_PERFCOUNTER0_HI,
                   (mmCB_PERFCOUNTER1_LO - mmCB_PERFCOUNTER0_LO));

    // DB block
    SetupBlockInfo(pProps,
                   GpuBlock::Db,
                   shaderEngines,
                   shaderArrays,
                   rbPerShaderArray,
                   mmDB_PERFCOUNTER0_LO,
                   mmDB_PERFCOUNTER0_HI,
                   (mmDB_PERFCOUNTER1_LO - mmDB_PERFCOUNTER0_LO));

    // GRBM block
    SetupBlockInfo(pProps,
                   GpuBlock::Grbm,
                   defaultNumShaderEngines,
                   defaultNumShaderArrays,
                   defaultNumInstances,
                   mmGRBM_PERFCOUNTER0_LO,
                   mmGRBM_PERFCOUNTER0_HI,
                   (mmGRBM_PERFCOUNTER1_LO - mmGRBM_PERFCOUNTER0_LO));

    // GRBMSE block
    SetupBlockInfo(pProps,
                   GpuBlock::GrbmSe,
                   defaultNumShaderEngines,
                   defaultNumShaderArrays,
                   shaderEngines,
                   mmGRBM_SE0_PERFCOUNTER_LO,
                   mmGRBM_SE0_PERFCOUNTER_HI,
                   0);

    // RLC block
    SetupBlockInfo(pProps,
                   GpuBlock::Rlc,
                   defaultNumShaderEngines,
                   defaultNumShaderArrays,
                   defaultNumInstances,
                   mmRLC_PERFCOUNTER0_LO,
                   mmRLC_PERFCOUNTER0_HI,
                   (mmRLC_PERFCOUNTER1_LO - mmRLC_PERFCOUNTER0_LO));

    // PA block
    SetupBlockInfo(pProps,
                   GpuBlock::Pa,
                   shaderEngines,
                   defaultNumShaderArrays,
                   defaultNumInstances,
                   mmPA_SU_PERFCOUNTER0_LO,
                   mmPA_SU_PERFCOUNTER0_HI,
                   (mmPA_SU_PERFCOUNTER1_LO - mmPA_SU_PERFCOUNTER0_LO));

    // SC block
    SetupBlockInfo(pProps,
                   GpuBlock::Sc,
                   shaderEngines,
                   defaultNumShaderArrays,
                   defaultNumInstances,
                   mmPA_SC_PERFCOUNTER0_LO,
                   mmPA_SC_PERFCOUNTER0_HI,
                   (mmPA_SC_PERFCOUNTER1_LO - mmPA_SC_PERFCOUNTER0_LO));

    // SX block
    SetupBlockInfo(pProps,
                   GpuBlock::Sx,
                   shaderEngines,
                   shaderArrays,
                   defaultNumInstances,
                   mmSX_PERFCOUNTER0_LO,
                   mmSX_PERFCOUNTER0_HI,
                   (mmSX_PERFCOUNTER1_LO - mmSX_PERFCOUNTER0_LO));

    // SPI block
    SetupBlockInfo(pProps,
                   GpuBlock::Spi,
                   shaderEngines,
                   defaultNumShaderArrays,
                   defaultNumInstances,
                   mmSPI_PERFCOUNTER0_LO,
                   mmSPI_PERFCOUNTER0_HI,
                   (mmSPI_PERFCOUNTER1_LO - mmSPI_PERFCOUNTER0_LO));

    // TA block
    SetupBlockInfo(pProps,
                   GpuBlock::Ta,
                   shaderEngines,
                   shaderArrays,
                   numCuPerSh,
                   mmTA_PERFCOUNTER0_LO,
                   mmTA_PERFCOUNTER0_HI,
                   (mmTA_PERFCOUNTER1_LO - mmTA_PERFCOUNTER0_LO));

    // TCP block
    SetupBlockInfo(pProps,
                   GpuBlock::Tcp,
                   shaderEngines,
                   shaderArrays,
                   numCuPerSh,
                   mmTCP_PERFCOUNTER0_LO,
                   mmTCP_PERFCOUNTER0_HI,
                   (mmTCP_PERFCOUNTER1_LO - mmTCP_PERFCOUNTER0_LO));

    // TD block
    SetupBlockInfo(pProps,
                   GpuBlock::Td,
                   shaderEngines,
                   shaderArrays,
                   numCuPerSh,
                   mmTD_PERFCOUNTER0_LO,
                   mmTD_PERFCOUNTER0_HI,
                   0);

    // GDS block
    SetupBlockInfo(pProps,
                   GpuBlock::Gds,
                   defaultNumShaderEngines,
                   defaultNumShaderArrays,
                   defaultNumInstances,
                   mmGDS_PERFCOUNTER0_LO,
                   mmGDS_PERFCOUNTER0_HI,
                   (mmGDS_PERFCOUNTER1_LO - mmGDS_PERFCOUNTER0_LO));

    // RMI block
    SetupBlockInfo(pProps,
                   GpuBlock::Rmi,
                   shaderEngines,
                   shaderArrays,
                   RmiInstances,
                   mmRMI_PERFCOUNTER0_LO,
                   mmRMI_PERFCOUNTER0_HI,
                   (mmRMI_PERFCOUNTER1_LO - mmRMI_PERFCOUNTER0_LO));
}

// =====================================================================================================================
// Initializes the performance counter information for Gfx9 hardware.
void SetupGfx9Counters(
    GpuChipProperties* pProps)
{
    constexpr uint32 DefaultShaderEngines =  1;
    constexpr uint32 DefaultShaderArrays  =  1;
    constexpr uint32 DefaultInstances     =  1;
    constexpr uint32 DefaultGroups        =  1;
    constexpr uint32 SdmaInstances        =  2;
    constexpr uint32 TcaInstances         =  2;
    constexpr uint32 EaInstances          = 16;
    // Each SQ(inside a CU) counts for that CU, but you cannot see that count. There is one set of 16 master counters
    // inside SPI(really SQG) that aggregates the counts from each CU and presents 16 counters which represent all of
    // the activity on the SE.
    // SQG represents the count for the entire shader engine(SE), and it's the only one visible to the user. So both
    // numShaderArrays and numInstances must be set to 1.
    constexpr uint32 SqShaderArrays = 1;
    constexpr uint32 SqInstances    = 1;

    const uint32 shaderEngines      = pProps->gfx9.numShaderEngines;
    const uint32 shaderArrays       = pProps->gfx9.numShaderArrays;
    const uint32 numCuPerSh         = pProps->gfx9.numCuPerSh;
    const uint32 rbPerShaderArray   = (pProps->gfx9.maxNumRbPerSe / shaderArrays);

    SetupHwlCounters(pProps, DefaultShaderEngines, DefaultShaderArrays, DefaultInstances);

    // TCC block
    SetupBlockInfo(pProps,
                   GpuBlock::Tcc,
                   DefaultShaderEngines,
                   DefaultShaderArrays,
                   pProps->gfx9.numTccBlocks,
                   mmTCC_PERFCOUNTER0_LO__GFX09,
                   mmTCC_PERFCOUNTER0_HI__GFX09,
                   (mmTCC_PERFCOUNTER1_LO__GFX09 - mmTCC_PERFCOUNTER0_LO__GFX09));

    // TCA block
    SetupBlockInfo(pProps,
                   GpuBlock::Tca,
                   DefaultShaderEngines,
                   DefaultShaderArrays,
                   TcaInstances,
                   mmTCA_PERFCOUNTER0_LO__GFX09,
                   mmTCA_PERFCOUNTER0_HI__GFX09,
                   (mmTCA_PERFCOUNTER1_LO__GFX09 - mmTCA_PERFCOUNTER0_LO__GFX09));

    // SDMA block
    SetupBlockInfo(pProps,
                   GpuBlock::Dma,
                   DefaultShaderEngines,
                   DefaultShaderArrays,
                   SdmaInstances,
                   mmSDMA0_PERFCOUNTER0_RESULT,
                   mmSDMA0_PERFCOUNTER1_RESULT,
                   (mmSDMA1_PERFCOUNTER0_RESULT__GFX09 - mmSDMA0_PERFCOUNTER1_RESULT));

    // SQ block
    SetupBlockInfo(pProps,
                   GpuBlock::Sq,
                   shaderEngines,
                   SqShaderArrays,
                   SqInstances,
                   mmSQ_PERFCOUNTER0_LO,
                   mmSQ_PERFCOUNTER0_HI,
                   (mmSQ_PERFCOUNTER1_LO - mmSQ_PERFCOUNTER0_LO));

    // VGT block
    SetupBlockInfo(pProps,
                   GpuBlock::Vgt,
                   shaderEngines,
                   DefaultShaderArrays,
                   DefaultInstances,
                   mmVGT_PERFCOUNTER0_LO__GFX09,
                   mmVGT_PERFCOUNTER0_HI__GFX09,
                   (mmVGT_PERFCOUNTER1_LO__GFX09 - mmVGT_PERFCOUNTER0_LO__GFX09));

    // IA block
    SetupBlockInfo(pProps,
                   GpuBlock::Ia,
                   Max(shaderEngines / 2U, 1U),
                   DefaultShaderArrays,
                   DefaultInstances,
                   mmIA_PERFCOUNTER0_LO__GFX09,
                   mmIA_PERFCOUNTER0_HI__GFX09,
                   (mmIA_PERFCOUNTER1_LO__GFX09 - mmIA_PERFCOUNTER0_LO__GFX09));

    // WD block
    SetupBlockInfo(pProps,
                   GpuBlock::Wd,
                   DefaultShaderEngines,
                   DefaultShaderArrays,
                   DefaultInstances,
                   mmWD_PERFCOUNTER0_LO__GFX09,
                   mmWD_PERFCOUNTER0_HI__GFX09,
                   (mmWD_PERFCOUNTER1_LO__GFX09 - mmWD_PERFCOUNTER0_LO__GFX09));

    // ATC block
    SetupMcSysBlockInfo(pProps,
                        GpuBlock::Atc,
                        DefaultShaderEngines,
                        DefaultShaderArrays,
                        DefaultInstances,
                        mmATC_PERFCOUNTER_LO__GFX09,
                        mmATC_PERFCOUNTER_HI__GFX09,
                        0,
                        mmATC_PERFCOUNTER_RSLT_CNTL__GFX09);

    // ATCL2 block
    SetupMcSysBlockInfo(pProps,
                        GpuBlock::AtcL2,
                        DefaultShaderEngines,
                        DefaultShaderArrays,
                        DefaultInstances,
                        mmATC_L2_PERFCOUNTER_LO__GFX09,
                        mmATC_L2_PERFCOUNTER_HI__GFX09,
                        0,
                        mmATC_L2_PERFCOUNTER_RSLT_CNTL__GFX09);

    // MCVML2 block
    SetupMcSysBlockInfo(pProps,
                        GpuBlock::McVmL2,
                        DefaultShaderEngines,
                        DefaultShaderArrays,
                        DefaultInstances,
                        mmMC_VM_L2_PERFCOUNTER_LO__GFX09,
                        mmMC_VM_L2_PERFCOUNTER_HI__GFX09,
                        0,
                        mmMC_VM_L2_PERFCOUNTER_RSLT_CNTL__GFX09);

    // EA block
    SetupMcSysBlockInfo(pProps,
                        GpuBlock::Ea,
                        DefaultShaderEngines,
                        DefaultShaderArrays,
                        EaInstances,
                        mmGCEA_PERFCOUNTER_LO__GFX09,
                        mmGCEA_PERFCOUNTER_HI__GFX09,
                        0,
                        mmGCEA_PERFCOUNTER_RSLT_CNTL__GFX09);

    // RPB block
    SetupMcSysBlockInfo(pProps,
                        GpuBlock::Rpb,
                        DefaultShaderEngines,
                        DefaultShaderArrays,
                        DefaultInstances,
                        mmRPB_PERFCOUNTER_LO__GFX09,
                        mmRPB_PERFCOUNTER_HI__GFX09,
                        0,
                        mmRPB_PERFCOUNTER_RSLT_CNTL__GFX09);
}

// =====================================================================================================================
// Initializes the performance counter information for an adapter structure, specifically for the Gfx9 hardware layer.
void InitPerfCtrInfo(
    GpuChipProperties* pProps)
{
    Gfx9PerfCounterInfo*const pInfo = &pProps->gfx9.perfCounterInfo;

    pInfo->features.counters         = 1;
    pInfo->features.threadTrace      = 1;
    pInfo->features.spmTrace         = 1;
    pInfo->features.supportPs1Events = 1;

    if (pProps->gfxLevel == GfxIpLevel::GfxIp9)
    {
        SetupGfx9Counters(pProps);
    }
    else
    {
        // What is this?
        PAL_ASSERT_ALWAYS();
    }
}

} // PerfExperiment
} // Gfx9
} // Pal
