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
    uint32  numRegs;                         // Number of counter registers in this block instance.
    uint32  numTotalStreamingCounterRegs;    // Number of streaming counter registers in this block.
    uint32  numStreamingCounters;            // Num streaming counters this SELECT(0/1) configures.
    uint32  regOffsets[MaxCountersPerBlock]; // Address offsets for all counters in this block.
};

// Table of all the primary perf-counter select registers.  We list all the register offsets since the delta's
// between registers are not consistent.
static constexpr BlockPerfCounterInfo Gfx9PerfCountSelect0[] =
{
    { Gfx9NumCpfCounters, 1, 2,    { mmCPF_PERFCOUNTER0_SELECT,
                                     mmCPF_PERFCOUNTER1_SELECT            }, },   // cpf
    { Gfx9NumIaCounters, 1, 2,     { Gfx09::mmIA_PERFCOUNTER0_SELECT,
                                     Gfx09::mmIA_PERFCOUNTER1_SELECT,
                                     Gfx09::mmIA_PERFCOUNTER2_SELECT,
                                     Gfx09::mmIA_PERFCOUNTER3_SELECT      }, },   // ia
   // NOTE: The perfmon doc claims DB/PA/TCP/VGT each have six streaming counters, though the regspec
   //       indicates there is room for eight.
    { Gfx9NumVgtCounters, 2, 3,    { Gfx09::mmVGT_PERFCOUNTER0_SELECT,
                                     Gfx09::mmVGT_PERFCOUNTER1_SELECT,
                                     Gfx09::mmVGT_PERFCOUNTER2_SELECT,
                                     Gfx09::mmVGT_PERFCOUNTER3_SELECT     }, },  // vgt
    { Gfx9NumPaCounters, 2, 3,     { mmPA_SU_PERFCOUNTER0_SELECT,
                                     mmPA_SU_PERFCOUNTER1_SELECT,
                                     mmPA_SU_PERFCOUNTER2_SELECT,
                                     Gfx09::mmPA_SU_PERFCOUNTER3_SELECT   }, },  // pa
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
    { Gfx9NumTccCounters, 2, 4,    { Gfx09::mmTCC_PERFCOUNTER0_SELECT,
                                     Gfx09::mmTCC_PERFCOUNTER1_SELECT,
                                     Gfx09::mmTCC_PERFCOUNTER2_SELECT,
                                     Gfx09::mmTCC_PERFCOUNTER3_SELECT,    }, }, // tcc
    { Gfx9NumTcaCounters, 2, 4,    { Gfx09::mmTCA_PERFCOUNTER0_SELECT,
                                     Gfx09::mmTCA_PERFCOUNTER1_SELECT,
                                     Gfx09::mmTCA_PERFCOUNTER2_SELECT,
                                     Gfx09::mmTCA_PERFCOUNTER3_SELECT,    }, }, // tca
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
    { Gfx9NumSdmaCounters, 0, 0,   { mmSDMA0_PERFMON_CNTL,                       // sdma, fixed for Raven
                                     Vega::mmSDMA1_PERFMON_CNTL,          }, },  //   in GetPrimaryBlockCounterInfo
    { 0, 0, 0,                     { 0,                                   }, },  // mc
    { Gfx9NumCpgCounters, 1, 2,    { mmCPG_PERFCOUNTER0_SELECT,
                                     mmCPG_PERFCOUNTER1_SELECT,           }, },  // cpg
    { Gfx9NumCpcCounters, 1, 2,    { mmCPC_PERFCOUNTER0_SELECT,
                                     mmCPC_PERFCOUNTER1_SELECT,           }, },  // cpc
    { Gfx9NumWdCounters, 0, 0,     { Gfx09::mmWD_PERFCOUNTER0_SELECT,
                                     Gfx09::mmWD_PERFCOUNTER1_SELECT,
                                     Gfx09::mmWD_PERFCOUNTER2_SELECT,
                                     Gfx09::mmWD_PERFCOUNTER3_SELECT,     }, },  // wd
    { 0, 0, 0,                     { 0,                                   }, },  // tcs
    { Gfx9NumAtcCounters, 0, 0,    { Gfx09::mmATC_PERFCOUNTER0_CFG,
                                     Gfx09::mmATC_PERFCOUNTER1_CFG,
                                     Gfx09::mmATC_PERFCOUNTER2_CFG,
                                     Gfx09::mmATC_PERFCOUNTER3_CFG,       }, },  // atc
    { Gfx9NumAtcL2Counters, 0, 0,  { Gfx09::mmATC_L2_PERFCOUNTER0_CFG,
                                     Gfx09::mmATC_L2_PERFCOUNTER1_CFG,    }, },  // atc l2
    { Gfx9NumMcVmL2Counters, 0, 0, { Gfx09::mmMC_VM_L2_PERFCOUNTER0_CFG,
                                     Gfx09::mmMC_VM_L2_PERFCOUNTER1_CFG,
                                     Gfx09::mmMC_VM_L2_PERFCOUNTER2_CFG,
                                     Gfx09::mmMC_VM_L2_PERFCOUNTER3_CFG,
                                     Gfx09::mmMC_VM_L2_PERFCOUNTER4_CFG,
                                     Gfx09::mmMC_VM_L2_PERFCOUNTER5_CFG,
                                     Gfx09::mmMC_VM_L2_PERFCOUNTER6_CFG,
                                     Gfx09::mmMC_VM_L2_PERFCOUNTER7_CFG,  }, },  // mc vm l2
    { Gfx9NumEaCounters, 0, 0,     { 0                                    }, },  // ea
    { Gfx9NumRpbCounters, 0, 0,    { Gfx09::mmRPB_PERFCOUNTER0_CFG,
                                     Gfx09::mmRPB_PERFCOUNTER1_CFG,
                                     Gfx09::mmRPB_PERFCOUNTER2_CFG,
                                     Gfx09::mmRPB_PERFCOUNTER3_CFG,       }, },  // rpb
    { Gfx9NumRmiCounters, 1, 2,    { mmRMI_PERFCOUNTER0_SELECT,
                                     mmRMI_PERFCOUNTER1_SELECT,
                                     mmRMI_PERFCOUNTER2_SELECT,
                                     mmRMI_PERFCOUNTER3_SELECT,           }, },  // rmi
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 424
    { Gfx9NumUmcchCounters, 0, 0,  { 0, 0, 0, 0, 0,                       }, },  // Umcch
#endif
};

static_assert(ArrayLen(Gfx9PerfCountSelect0) == static_cast<uint32>(GpuBlock::Count),
              "Gfx9PerfCountSelect0 must have one entry for each GpuBlock.");

// Table of all the secondary perf-counter select registers.  We list all the register offsets since the delta's
// between registers are not consistent.
static constexpr BlockPerfCounterInfo Gfx9PerfCountSelect1[] =
{
    { 1, 1, 2,               { mmCPF_PERFCOUNTER0_SELECT1,          }, },  // cpf
    { 1, 1, 2,               { Gfx09::mmIA_PERFCOUNTER0_SELECT1,    }, },  // ia
    { 2, 1, 2,               { Gfx09::mmVGT_PERFCOUNTER0_SELECT1,
                               Gfx09::mmVGT_PERFCOUNTER1_SELECT1,   }, },  // vgt
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
    { 2, 2, 4,               { Gfx09::mmTCC_PERFCOUNTER0_SELECT1,
                               Gfx09::mmTCC_PERFCOUNTER1_SELECT1,   }, },  // tcc
    { 2, 2, 4,               { Gfx09::mmTCA_PERFCOUNTER0_SELECT1,
                               Gfx09::mmTCA_PERFCOUNTER1_SELECT1,   }, },  // tca
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 424
    { 0, 0, 0,               { 0, 0, 0, 0, 0,                       }, },  // Umcch
#endif
};

static_assert(ArrayLen(Gfx9PerfCountSelect1) == static_cast<uint32>(GpuBlock::Count),
              "Gfx9PerfCountSelect1 must have one entry for each GpuBlock.");

// =====================================================================================================================
void GetPrimaryBlockCounterInfo(
    const GpuChipProperties* pProps,
    GpuBlock                 block,
    BlockPerfCounterInfo*    pBlockCounterInfo)
{
    const uint32 blockIdx  = static_cast<uint32>(block);

    if (pProps->gfxLevel == GfxIpLevel::GfxIp9)
    {
        memcpy(pBlockCounterInfo, &Gfx9PerfCountSelect0[blockIdx], sizeof(BlockPerfCounterInfo));

        // The base table contains the Vega10 information; fix up any differences with the variations here
        if ((pProps->familyId == FAMILY_RV) && (block == GpuBlock::Dma))
        {
            // The *only* difference between the Raven and Vega family is that Raven has one of these
            // and Vega has two.
            constexpr BlockPerfCounterInfo  SdmaPerfCounterInfo = { 1, 0, 0, { mmSDMA0_PERFMON_CNTL, }, };

            memcpy(pBlockCounterInfo, &SdmaPerfCounterInfo, sizeof(BlockPerfCounterInfo));
        }
        else if (block == GpuBlock::Ea)
        {
            if (AMDGPU_IS_VEGA10(pProps->familyId, pProps->eRevId) ||
                AMDGPU_IS_RAVEN(pProps->familyId, pProps->eRevId))
            {
                constexpr BlockPerfCounterInfo  EaPerfCounterInfo =
                    { Gfx9NumEaCounters, 0, 0,{ Gfx09_0::mmGCEA_PERFCOUNTER0_CFG,
                                                Gfx09_0::mmGCEA_PERFCOUNTER1_CFG } };

                memcpy(pBlockCounterInfo, &EaPerfCounterInfo, sizeof(BlockPerfCounterInfo));
            }
            else
            {
                constexpr BlockPerfCounterInfo  EaPerfCounterInfo =
                    { Gfx9NumEaCounters, 0, 0,{ Gfx09_1x::mmGCEA_PERFCOUNTER0_CFG,
                                                Gfx09_1x::mmGCEA_PERFCOUNTER1_CFG } };

                memcpy(pBlockCounterInfo, &EaPerfCounterInfo, sizeof(BlockPerfCounterInfo));
            }
        }
    }
}

// =====================================================================================================================
void GetSecondaryBlockCounterInfo(
    const GpuChipProperties* pProps,
    GpuBlock                 block,
    BlockPerfCounterInfo*    pBlockCounterInfo)
{
    const uint32  blockIdx = static_cast<uint32>(block);

    if (pProps->gfxLevel == GfxIpLevel::GfxIp9)
    {
        memcpy(pBlockCounterInfo, &Gfx9PerfCountSelect1[blockIdx], sizeof(BlockPerfCounterInfo));
    }
}

// =====================================================================================================================
// Returns the number of performance counters supported by the specified block.
uint32 GetMaxEventId(
    GpuChipProperties* pProps,
    GpuBlock           block)
{
    const uint32  blockIdx = static_cast<uint32>(block);

    PAL_ASSERT(blockIdx < static_cast<uint32>(GpuBlock::Count));

    uint32  maxEventId = 0;
    if (pProps->gfxLevel == GfxIpLevel::GfxIp9)
    {
        constexpr uint32 Gfx9PerfCtrRlcMaxEvent = 7; //< RLC, doesn't have enumerations, look in reg spec

        // Define the generic max event IDs.  Most of these are the same between the GFX9 variations
        static constexpr uint32  MaxEventId[static_cast<uint32>(GpuBlock::Count)] =
        {
            MaxCpfPerfcountSelGfx09,
            0, // Ia, see below
            MaxVgtPerfcountSelect,
            0, // PA, see below
            0, // SC, see below
            MaxSpiPerfcntSelGfx09,
            MaxSqPerfSelGfx09,
            MaxSxPerfcounterValsGfx09,
            MaxTaPerfcountSelGfx09,
            MaxTdPerfcountSelGfx09,
            MaxTcpPerfcountSelectGfx09,
            0, // Tcc, see below,
            MaxTcaPerfSel,
            MaxPerfcounterValsGfx09,
            MaxCBPerfSelGfx09,
            MaxGdsPerfcountSelectGfx09,
            0, // Srbm,
            MaxGrbmPerfSelGfx09,
            MaxGrbmPerfSelGfx09,
            Gfx9PerfCtrRlcMaxEvent,
            MaxSdmaPerfSelGfx09,
            Gfx9PerfCtrlEaMaxEvent,
            0, // Cpg, see below
            MaxCpcPerfcountSelGfx09,
            MaxWdPerfcountSelect,
            0, // Tcs,
            Gfx9PerfCtrlAtcMaxEvent,
            Gfx9PerfCtrlAtcL2MaxEvent,
            Gfx9PerfCtrlMcVmL2MaxEvent,
            Gfx9PerfCtrlEaMaxEvent,
            Gfx9PerfCtrlRpbMaxEvent,
            Gfx9PerfCtrRmiMaxEvent,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 424
            Gfx9PerfCtrUmcMaxEvent,
#endif
        };

        maxEventId = MaxEventId[blockIdx];

        if (maxEventId == 0)
        {
            if ((block == GpuBlock::Cpg) || (block == GpuBlock::Ia))
            {
                if ((AMDGPU_IS_VEGA10(pProps->familyId, pProps->eRevId)) ||
                    (AMDGPU_IS_RAVEN(pProps->familyId, pProps->eRevId)))
                {
                    maxEventId = MaxIaPerfcountSelectGfx09_0;
                }
                else
                {
                    maxEventId = MaxIaPerfcountSelectGfx09_1x;
                }
            }
            else if (block == GpuBlock::Pa)
            {
                if ((AMDGPU_IS_VEGA10(pProps->familyId, pProps->eRevId)) ||
                    (AMDGPU_IS_RAVEN(pProps->familyId, pProps->eRevId)))
                {
                    maxEventId = MaxSuPerfcntSelGfx09_0;
                }
                else
                {
                    maxEventId = MaxSuPerfcntSelGfx09_1x;
                }
            }
            else if (block == GpuBlock::Sc)
            {
                maxEventId = MaxScPerfcntSelGfx09_0;
                if (AMDGPU_IS_VEGA12(pProps->familyId, pProps->eRevId))
                {
                    maxEventId = MaxScPerfcntSelVg12;
                }
            }
            else if (block == GpuBlock::Tcc)
            {
                maxEventId = MaxTccPerfSelVg10_Vg12_Rv1x;

            }
        } // end check for an invalid block ID
    }

    // Why is the caller setting up a block that doesn't have any event ID's associated with it?
    PAL_ASSERT(maxEventId != 0);

    return maxEventId + 1;
}

// =====================================================================================================================
uint32 GetSpmBlockSelect(
    GpuChipProperties* pProps,
    GpuBlock           block)
{

    const uint32  blockIdx = static_cast<uint32>(block);
    const uint32 DefaultBlockSelect = 0xFFFF;

    uint32  blockSelectCode = 0;
    if (pProps->gfxLevel == GfxIpLevel::GfxIp9)
    {
        static constexpr uint32  BlockSelectCodes[static_cast<uint32>(GpuBlock::Count)] =
        {
            Gfx9SpmGlobalBlockSelect::Cpf,
            Gfx9SpmGlobalBlockSelect::Ia,
            Gfx9SpmSeBlockSelect::Vgt,
            Gfx9SpmSeBlockSelect::Pa,
            Gfx9SpmSeBlockSelect::Sc,
            Gfx9SpmSeBlockSelect::Spi,
            Gfx9SpmSeBlockSelect::Sqg,
            Gfx9SpmSeBlockSelect::Sx,
            Gfx9SpmSeBlockSelect::Ta,
            Gfx9SpmSeBlockSelect::Td,
            Gfx9SpmSeBlockSelect::Tcp,
            Gfx9SpmGlobalBlockSelect::Tcc,
            Gfx9SpmGlobalBlockSelect::Tca,
            Gfx9SpmSeBlockSelect::Db,
            Gfx9SpmSeBlockSelect::Cb,
            Gfx9SpmGlobalBlockSelect::Gds,
            DefaultBlockSelect, // Srbm,
            DefaultBlockSelect,
            DefaultBlockSelect,
            DefaultBlockSelect,
            DefaultBlockSelect,
            DefaultBlockSelect,
            Gfx9SpmGlobalBlockSelect::Cpg,
            Gfx9SpmGlobalBlockSelect::Cpc,
            DefaultBlockSelect,
            DefaultBlockSelect,
            DefaultBlockSelect,
            DefaultBlockSelect,
            DefaultBlockSelect,
            DefaultBlockSelect,
            DefaultBlockSelect,
            Gfx9SpmSeBlockSelect::Rmi,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 424
            DefaultBlockSelect, // UMCCH
#endif
        };

        blockSelectCode = BlockSelectCodes[blockIdx];
    }

    return blockSelectCode;
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
    BlockPerfCounterInfo  selReg0 = {};
    BlockPerfCounterInfo  selReg1 = {};

    const uint32              blockIdx = static_cast<uint32>(block);
    Gfx9PerfCounterInfo*const pInfo    = &pProps->gfx9.perfCounterInfo;

    GetPrimaryBlockCounterInfo(pProps,   block, &selReg0);
    GetSecondaryBlockCounterInfo(pProps, block, &selReg1);

    PAL_ASSERT(selReg0.numRegs <= MaxCountersPerBlock);

    pInfo->block[blockIdx].available               = true;
    pInfo->block[blockIdx].numShaderEngines        = numShaderEngines;
    pInfo->block[blockIdx].numShaderArrays         = numShaderArrays;
    pInfo->block[blockIdx].numInstances            = numInstances;
    pInfo->block[blockIdx].numCounters             = selReg0.numRegs;
    pInfo->block[blockIdx].numStreamingCounters    = selReg0.numStreamingCounters + selReg1.numStreamingCounters;
    pInfo->block[blockIdx].numStreamingCounterRegs = selReg0.numTotalStreamingCounterRegs;
    pInfo->block[blockIdx].maxEventId              = GetMaxEventId(pProps, block);
    pInfo->block[blockIdx].spmBlockSelectCode      = GetSpmBlockSelect(pProps, block);

    // Setup the register addresses for each counter for this block.
    for (uint32  idx = 0; idx < selReg0.numRegs; idx++)
    {
        pInfo->block[blockIdx].regInfo[idx].perfSel0RegAddr = selReg0.regOffsets[idx];

        pInfo->block[blockIdx].regInfo[idx].perfCountLoAddr = ctrLoRegAddr + idx * ctrRegIncr;
        pInfo->block[blockIdx].regInfo[idx].perfCountHiAddr = ctrHiRegAddr + idx * ctrRegIncr;
    }

    for (uint32  idx = 0; idx < selReg1.numRegs; idx++)
    {
        pInfo->block[blockIdx].regInfo[idx].perfSel1RegAddr = selReg1.regOffsets[idx];
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
    Gfx9PerfCounterInfo*const pInfo    = &pProps->gfx9.perfCounterInfo;

    // Set rsltCntlRegAddr
    BlockPerfCounterInfo  selReg0 = {};
    GetPrimaryBlockCounterInfo(pProps, block, &selReg0);
    for (uint32  idx = 0; idx < selReg0.numRegs; idx++)
    {
        pInfo->block[blockIdx].regInfo[idx].perfRsltCntlRegAddr = rsltCntlRegAddr;
    }
}

// =====================================================================================================================
// Populates the PerfCounterInfo with the perf counter configuration and addresses for the Umcch block.
void SetupUmcchBlockInfo(
    GpuChipProperties* pProps)
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 424
    constexpr uint32 DefaultShaderEngines =  1;
    constexpr uint32 DefaultShaderArrays  =  1;

    Gfx9PerfCounterInfo*const pInfo = &pProps->gfx9.perfCounterInfo;
    auto& blockInfo    = pInfo->umcChannelBlocks;
    auto& perfCtrInfo  = pInfo->block[static_cast<uint32>(GpuBlock::Umcch)];

    perfCtrInfo.available        = true;
    perfCtrInfo.numInstances     = pProps->gfx9.numSdpInterfaces;  // The number of UMC channels is equal to the number
                                                                   // of EA blocks or the number of SDP interface ports.
    perfCtrInfo.numCounters      = Gfx9NumUmcchCounters;
    perfCtrInfo.maxEventId       = Gfx9PerfCtrUmcMaxEvent;
    perfCtrInfo.numShaderArrays  = DefaultShaderArrays;
    perfCtrInfo.numShaderEngines = DefaultShaderEngines;

    const UmcchPerfCounterAddr* pPerfCtrAddr = nullptr;

    if (ASICREV_IS_VEGA10_P(pProps->eRevId))
    {
        pPerfCtrAddr = &Gfx9UmcchPerfCounterInfo_vg10[0];
    }
    else if (ASICREV_IS_VEGA12_P(pProps->eRevId))
    {
        pPerfCtrAddr = &Gfx9UmcchPerfCounterInfo_vg12[0];
    }
    else if (ASICREV_IS_RAVEN(pProps->eRevId)
    )
    {
        // Both Ravens.
        pPerfCtrAddr = &Gfx9UmcchPerfCounterInfo_Raven[0];
    }
#if PAL_BUILD_GXF10
    else if (AMDGPU_IS_NAVI(pProps->familyId, pProps->eRevId)
    {
        pPerfCtrAddr = &Gfx10UmcchPerfCounterInfo_Navi[0];
    }
#endif
    else
    {
        // This ASIC is not supported.
        PAL_ASSERT_ALWAYS();
    }

    for (uint32 chIdx = 0; chIdx < perfCtrInfo.numInstances; ++chIdx)
    {
        blockInfo.regInfo[chIdx].ctlClkRegAddr = pPerfCtrAddr[chIdx].perfMonCtlClk;
        const uint32 ctr1ControlReg            = pPerfCtrAddr[chIdx].perfMonCtl1;
        const uint32 ctr1ResultLoReg           = pPerfCtrAddr[chIdx].perfMonCtr1Lo;

        for (uint32 ctrIdx = 0; ctrIdx < Gfx9NumUmcchCounters; ++ctrIdx)
        {
            blockInfo.regInfo[chIdx].counter[ctrIdx].ctrControlRegAddr = ctr1ControlReg + ctrIdx;

            const uint32 resultRegLoAddr =
                ctr1ResultLoReg + (ctrIdx * (mmUMCCH0_PerfMonCtr2_Lo - mmUMCCH0_PerfMonCtr1_Lo));

            blockInfo.regInfo[chIdx].counter[ctrIdx].resultRegLoAddr = resultRegLoAddr;
            blockInfo.regInfo[chIdx].counter[ctrIdx].resultRegHiAddr = resultRegLoAddr + 1;
        }
    }
#endif
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

    // Umcch block
    SetupUmcchBlockInfo(pProps);
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
    constexpr uint32 TcaInstances         =  2;
    constexpr uint32 EaInstances          = 16;

    // Vega (AI) vs. Raven have different number of SDMA instances
    const     uint32 SdmaInstances = ((pProps->familyId == FAMILY_AI) ? 2 : 1);

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
                   Gfx09::mmTCC_PERFCOUNTER0_LO,
                   Gfx09::mmTCC_PERFCOUNTER0_HI,
                   (Gfx09::mmTCC_PERFCOUNTER1_LO - Gfx09::mmTCC_PERFCOUNTER0_LO));

    // TCA block
    SetupBlockInfo(pProps,
                   GpuBlock::Tca,
                   DefaultShaderEngines,
                   DefaultShaderArrays,
                   TcaInstances,
                   Gfx09::mmTCA_PERFCOUNTER0_LO,
                   Gfx09::mmTCA_PERFCOUNTER0_HI,
                   (Gfx09::mmTCA_PERFCOUNTER1_LO - Gfx09::mmTCA_PERFCOUNTER0_LO));

    // SDMA block
    SetupBlockInfo(pProps,
                   GpuBlock::Dma,
                   DefaultShaderEngines,
                   DefaultShaderArrays,
                   SdmaInstances,
                   mmSDMA0_PERFCOUNTER0_RESULT,
                   mmSDMA0_PERFCOUNTER1_RESULT,
                   ((pProps->familyId == FAMILY_AI)
                    ? (Vega::mmSDMA1_PERFCOUNTER0_RESULT - mmSDMA0_PERFCOUNTER1_RESULT)
                    : 0));

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
                   Gfx09::mmVGT_PERFCOUNTER0_LO,
                   Gfx09::mmVGT_PERFCOUNTER0_HI,
                   (Gfx09::mmVGT_PERFCOUNTER1_LO - Gfx09::mmVGT_PERFCOUNTER0_LO));

    // IA block
    SetupBlockInfo(pProps,
                   GpuBlock::Ia,
                   Max(shaderEngines / 2U, 1U),
                   DefaultShaderArrays,
                   DefaultInstances,
                   Gfx09::mmIA_PERFCOUNTER0_LO,
                   Gfx09::mmIA_PERFCOUNTER0_HI,
                   (Gfx09::mmIA_PERFCOUNTER1_LO - Gfx09::mmIA_PERFCOUNTER0_LO));

    // WD block
    SetupBlockInfo(pProps,
                   GpuBlock::Wd,
                   DefaultShaderEngines,
                   DefaultShaderArrays,
                   DefaultInstances,
                   Gfx09::mmWD_PERFCOUNTER0_LO,
                   Gfx09::mmWD_PERFCOUNTER0_HI,
                   (Gfx09::mmWD_PERFCOUNTER1_LO - Gfx09::mmWD_PERFCOUNTER0_LO));

    // ATC block
    SetupMcSysBlockInfo(pProps,
                        GpuBlock::Atc,
                        DefaultShaderEngines,
                        DefaultShaderArrays,
                        DefaultInstances,
                        Gfx09::mmATC_PERFCOUNTER_LO,
                        Gfx09::mmATC_PERFCOUNTER_HI,
                        0,
                        Gfx09::mmATC_PERFCOUNTER_RSLT_CNTL);

    // ATCL2 block
    SetupMcSysBlockInfo(pProps,
                        GpuBlock::AtcL2,
                        DefaultShaderEngines,
                        DefaultShaderArrays,
                        DefaultInstances,
                        Gfx09::mmATC_L2_PERFCOUNTER_LO,
                        Gfx09::mmATC_L2_PERFCOUNTER_HI,
                        0,
                        Gfx09::mmATC_L2_PERFCOUNTER_RSLT_CNTL);

    // MCVML2 block
    SetupMcSysBlockInfo(pProps,
                        GpuBlock::McVmL2,
                        DefaultShaderEngines,
                        DefaultShaderArrays,
                        DefaultInstances,
                        Gfx09::mmMC_VM_L2_PERFCOUNTER_LO,
                        Gfx09::mmMC_VM_L2_PERFCOUNTER_HI,
                        0,
                        Gfx09::mmMC_VM_L2_PERFCOUNTER_RSLT_CNTL);

    // EA block
    if (AMDGPU_IS_VEGA10(pProps->familyId, pProps->eRevId) ||
        AMDGPU_IS_RAVEN(pProps->familyId, pProps->eRevId))
    {
        SetupMcSysBlockInfo(pProps,
                            GpuBlock::Ea,
                            DefaultShaderEngines,
                            DefaultShaderArrays,
                            EaInstances,
                            Gfx09_0::mmGCEA_PERFCOUNTER_LO,
                            Gfx09_0::mmGCEA_PERFCOUNTER_HI,
                            0,
                            Gfx09_0::mmGCEA_PERFCOUNTER_RSLT_CNTL);
    }
    else
    {
        SetupMcSysBlockInfo(pProps,
                            GpuBlock::Ea,
                            DefaultShaderEngines,
                            DefaultShaderArrays,
                            EaInstances,
                            Gfx09_1x::mmGCEA_PERFCOUNTER_LO,
                            Gfx09_1x::mmGCEA_PERFCOUNTER_HI,
                            0,
                            Gfx09_1x::mmGCEA_PERFCOUNTER_RSLT_CNTL);
    }

    // RPB block
    SetupMcSysBlockInfo(pProps,
                        GpuBlock::Rpb,
                        DefaultShaderEngines,
                        DefaultShaderArrays,
                        DefaultInstances,
                        Gfx09::mmRPB_PERFCOUNTER_LO,
                        Gfx09::mmRPB_PERFCOUNTER_HI,
                        0,
                        Gfx09::mmRPB_PERFCOUNTER_RSLT_CNTL);
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
