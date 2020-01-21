/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6PerfCtrInfo.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// These enums are defined by the SPM spec. They map block names to RLC-specific SPM block select codes.
enum Gfx7SpmGlobalBlockSelect : uint32
{
    Gfx7SpmGlobalBlockSelectCpg = 0x0,
    Gfx7SpmGlobalBlockSelectCpc = 0x1,
    Gfx7SpmGlobalBlockSelectCpf = 0x2,
    Gfx7SpmGlobalBlockSelectGds = 0x3,
    Gfx7SpmGlobalBlockSelectTcc = 0x4,
    Gfx7SpmGlobalBlockSelectTca = 0x5,
    Gfx7SpmGlobalBlockSelectIa  = 0x6,
    Gfx7SpmGlobalBlockSelectTcs = 0x7
};

enum Gfx7SpmSeBlockSelect : uint32
{
    Gfx7SpmSeBlockSelectCb  = 0x0,
    Gfx7SpmSeBlockSelectDb  = 0x1,
    Gfx7SpmSeBlockSelectPa  = 0x2,
    Gfx7SpmSeBlockSelectSx  = 0x3,
    Gfx7SpmSeBlockSelectSc  = 0x4,
    Gfx7SpmSeBlockSelectTa  = 0x5,
    Gfx7SpmSeBlockSelectTd  = 0x6,
    Gfx7SpmSeBlockSelectTcp = 0x7,
    Gfx7SpmSeBlockSelectSpi = 0x8,
    Gfx7SpmSeBlockSelectSqg = 0x9,
    Gfx7SpmSeBlockSelectVgt = 0xA
};

// =====================================================================================================================
// A helper function which fills out the mcConfig properties.
static void InitMcConfigInfo(
    const Pal::Device&   device,
    Gfx6PerfCounterInfo* pInfo)
{
    // Pitcairn has a different MC config register than other hardware.
    if (IsPitcairn(device))
    {
        pInfo->mcConfig.regAddress      = mmMC_CONFIG;
        pInfo->mcConfig.readEnableShift = MC_CONFIG__MC_RD_ENABLE__SHIFT__SI__CI;
        pInfo->mcConfig.writeEnableMask = (MC_CONFIG__MCDW_WR_ENABLE_MASK |
                                           MC_CONFIG__MCDX_WR_ENABLE_MASK |
                                           MC_CONFIG__MCDY_WR_ENABLE_MASK |
                                           MC_CONFIG__MCDZ_WR_ENABLE_MASK);
    }
    else
    {
        pInfo->mcConfig.regAddress      = mmMC_CONFIG_MCD;
        pInfo->mcConfig.readEnableShift = MC_CONFIG_MCD__MC_RD_ENABLE__SHIFT;

        // The write enable mask selects which MCDs to write to.
        // Setup the write enable mask so that we only capture from present MCDs.
        if (IsTonga(device) && (device.ChipProperties().gfx6.numMcdTiles == 4))
        {
            // The Four MCD tonga has an unusual CONFIG where it enables MCD0, 2, 3, and 5.
            pInfo->mcConfig.writeEnableMask = (MC_CONFIG_MCD__MCD0_WR_ENABLE_MASK |
                                               MC_CONFIG_MCD__MCD2_WR_ENABLE_MASK |
                                               MC_CONFIG_MCD__MCD3_WR_ENABLE_MASK |
                                               MC_CONFIG_MCD__MCD5_WR_ENABLE_MASK);
        }
        else
        {
            pInfo->mcConfig.writeEnableMask = (1 << device.ChipProperties().gfx6.numMcdTiles) - 1;

            // Confirm that the write enable bits are where we are expecting them to be for the previous calculation to
            // set the correct bits.
            static_assert(MC_CONFIG_MCD__MCD0_WR_ENABLE_MASK     == 0x1,  "Unexpected write enable bits.");
            static_assert(MC_CONFIG_MCD__MCD1_WR_ENABLE_MASK     == 0x2,  "Unexpected write enable bits.");
            static_assert(MC_CONFIG_MCD__MCD2_WR_ENABLE_MASK     == 0x4,  "Unexpected write enable bits.");
            static_assert(MC_CONFIG_MCD__MCD3_WR_ENABLE_MASK     == 0x8,  "Unexpected write enable bits.");
            static_assert(MC_CONFIG_MCD__MCD4_WR_ENABLE_MASK     == 0x10, "Unexpected write enable bits.");
            static_assert(MC_CONFIG_MCD__MCD5_WR_ENABLE_MASK     == 0x20, "Unexpected write enable bits.");
            static_assert(MC_CONFIG_MCD__MCD6_WR_ENABLE_MASK__VI == 0x40, "Unexpected write enable bits.");
            static_assert(MC_CONFIG_MCD__MCD7_WR_ENABLE_MASK__VI == 0x80, "Unexpected write enable bits.");

            // The MC_CONFIG_MCD::MCD#_RD_ENABLE bits occupy the first 8 bits of the register.
            // Assert that the generated mask is no more than 8 bits.
            PAL_ASSERT((pInfo->mcConfig.writeEnableMask & 0xFF) == pInfo->mcConfig.writeEnableMask);
        }
    }
}

// =====================================================================================================================
// Initializes each block's basic hardware-defined information (distribution, numInstances, numGenericSpmModules, etc.)
static void Gfx6InitBasicBlockInfo(
    const Pal::Device& device,
    GpuChipProperties* pProps)
{
    Gfx6PerfCounterInfo*const pInfo = &pProps->gfx6.perfCounterInfo;

    // Hard-code hardware specific constants for each block. The RLC seems like it has counters on gfx6 but the old
    // code didn't implement it. We might consider exposing it in the future.
    //
    // The distribution and numInstances (per-distribution) are derived from our hardware architecture.
    // The generic module counts are determined by:
    //   1. Does the block follow the generic programming model as defined by the perf experiment code?
    //   2. If so, there's one legacy module for each SELECT (SPM is not supported on gfx6).
    // The maximum event IDs are the largest values from the hardware perf_sel enums.
    // Finally, we hard-code the PERFCOUNTER# register addresses for each module.

    // Gfx6 views the whole CP as a single block instead of splitting it into a CPF and CPG. Historically this code
    // has exposed this CP as GpuBlock::Cpf but the gfx6 event IDs match the gfx7 CPG event IDs. Maybe we should
    // use GpuBlock::Cpg instead?
    PerfCounterBlockInfo*const pCp = &pInfo->block[static_cast<uint32>(GpuBlock::Cpf)];
    pCp->distribution              = PerfCounterDistribution::GlobalBlock;
    pCp->numInstances              = 1;
    pCp->numGenericLegacyModules   = 1; // CP_PERFCOUNTER
    pCp->maxEventId                = CPG_PERF_SEL_TCIU_STALL_WAIT_ON_TAGS;

    pCp->regAddr = { 0, {
        { mmCP_PERFCOUNTER_SELECT__SI, 0, mmCP_PERFCOUNTER_LO__SI, mmCP_PERFCOUNTER_HI__SI },
    }};

    PerfCounterBlockInfo*const pIa = &pInfo->block[static_cast<uint32>(GpuBlock::Ia)];
    pIa->distribution              = PerfCounterDistribution::GlobalBlock;
    pIa->numInstances              = 1;
    pIa->numGenericLegacyModules   = 4; // IA_PERFCOUNTER0-3
    pIa->maxEventId                = ia_perf_ia_stalled__SI__VI;

    pIa->regAddr = { 0, {
        { mmIA_PERFCOUNTER0_SELECT__SI, 0, mmIA_PERFCOUNTER0_LO__SI, mmIA_PERFCOUNTER0_HI__SI },
        { mmIA_PERFCOUNTER1_SELECT__SI, 0, mmIA_PERFCOUNTER1_LO__SI, mmIA_PERFCOUNTER1_HI__SI },
        { mmIA_PERFCOUNTER2_SELECT__SI, 0, mmIA_PERFCOUNTER2_LO__SI, mmIA_PERFCOUNTER2_HI__SI },
        { mmIA_PERFCOUNTER3_SELECT__SI, 0, mmIA_PERFCOUNTER3_LO__SI, mmIA_PERFCOUNTER3_HI__SI },
    }};

    PerfCounterBlockInfo*const pVgt = &pInfo->block[static_cast<uint32>(GpuBlock::Vgt)];
    pVgt->distribution              = PerfCounterDistribution::PerShaderEngine;
    pVgt->numInstances              = 1;
    pVgt->numGenericLegacyModules   = 4; // VGT_PERFCOUNTER0-3
    pVgt->maxEventId                = vgt_perf_hs_tgs_active_high_water_mark__SI__CI;

    pVgt->regAddr = { 0, {
        { mmVGT_PERFCOUNTER0_SELECT__SI, 0, mmVGT_PERFCOUNTER0_LO__SI, mmVGT_PERFCOUNTER0_HI__SI },
        { mmVGT_PERFCOUNTER1_SELECT__SI, 0, mmVGT_PERFCOUNTER1_LO__SI, mmVGT_PERFCOUNTER1_HI__SI },
        { mmVGT_PERFCOUNTER2_SELECT__SI, 0, mmVGT_PERFCOUNTER2_LO__SI, mmVGT_PERFCOUNTER2_HI__SI },
        { mmVGT_PERFCOUNTER3_SELECT__SI, 0, mmVGT_PERFCOUNTER3_LO__SI, mmVGT_PERFCOUNTER3_HI__SI },
    }};

    // Note that the PA uses the SU select enum.
    PerfCounterBlockInfo*const pPa = &pInfo->block[static_cast<uint32>(GpuBlock::Pa)];
    pPa->distribution              = PerfCounterDistribution::PerShaderEngine;
    pPa->numInstances              = 1;
    pPa->numGenericLegacyModules   = 4; // PA_SU_PERFCOUNTER0-3
    pPa->maxEventId                = PERF_PAPC_SU_CULLED_PRIM;

    pPa->regAddr = { 0, {
        { mmPA_SU_PERFCOUNTER0_SELECT__SI, 0, mmPA_SU_PERFCOUNTER0_LO__SI, mmPA_SU_PERFCOUNTER0_HI__SI },
        { mmPA_SU_PERFCOUNTER1_SELECT__SI, 0, mmPA_SU_PERFCOUNTER1_LO__SI, mmPA_SU_PERFCOUNTER1_HI__SI },
        { mmPA_SU_PERFCOUNTER2_SELECT__SI, 0, mmPA_SU_PERFCOUNTER2_LO__SI, mmPA_SU_PERFCOUNTER2_HI__SI },
        { mmPA_SU_PERFCOUNTER3_SELECT__SI, 0, mmPA_SU_PERFCOUNTER3_LO__SI, mmPA_SU_PERFCOUNTER3_HI__SI },
    }};

    PerfCounterBlockInfo*const pSc = &pInfo->block[static_cast<uint32>(GpuBlock::Sc)];
    pSc->distribution              = PerfCounterDistribution::PerShaderEngine;
    pSc->numInstances              = 1;
    pSc->numGenericLegacyModules   = 8; // PA_SC_PERFCOUNTER0-7
    pSc->maxEventId                = SC_PS_ARB_PA_SC_BUSY;

    pSc->regAddr = { 0, {
        { mmPA_SC_PERFCOUNTER0_SELECT__SI, 0, mmPA_SC_PERFCOUNTER0_LO__SI, mmPA_SC_PERFCOUNTER0_HI__SI },
        { mmPA_SC_PERFCOUNTER1_SELECT__SI, 0, mmPA_SC_PERFCOUNTER1_LO__SI, mmPA_SC_PERFCOUNTER1_HI__SI },
        { mmPA_SC_PERFCOUNTER2_SELECT__SI, 0, mmPA_SC_PERFCOUNTER2_LO__SI, mmPA_SC_PERFCOUNTER2_HI__SI },
        { mmPA_SC_PERFCOUNTER3_SELECT__SI, 0, mmPA_SC_PERFCOUNTER3_LO__SI, mmPA_SC_PERFCOUNTER3_HI__SI },
        { mmPA_SC_PERFCOUNTER4_SELECT__SI, 0, mmPA_SC_PERFCOUNTER4_LO__SI, mmPA_SC_PERFCOUNTER4_HI__SI },
        { mmPA_SC_PERFCOUNTER5_SELECT__SI, 0, mmPA_SC_PERFCOUNTER5_LO__SI, mmPA_SC_PERFCOUNTER5_HI__SI },
        { mmPA_SC_PERFCOUNTER6_SELECT__SI, 0, mmPA_SC_PERFCOUNTER6_LO__SI, mmPA_SC_PERFCOUNTER6_HI__SI },
        { mmPA_SC_PERFCOUNTER7_SELECT__SI, 0, mmPA_SC_PERFCOUNTER7_LO__SI, mmPA_SC_PERFCOUNTER7_HI__SI },
    }};

    PerfCounterBlockInfo*const pSpi = &pInfo->block[static_cast<uint32>(GpuBlock::Spi)];
    pSpi->distribution              = PerfCounterDistribution::PerShaderEngine;
    pSpi->numInstances              = 1;
    pSpi->numGenericLegacyModules   = 4; // SPI_PERFCOUNTER0-3
    pSpi->maxEventId                = SPI_PERF_CLKGATE_CGTT_REG_ON__SI;

    pSpi->regAddr = { 0, {
        { mmSPI_PERFCOUNTER0_SELECT__SI, 0, mmSPI_PERFCOUNTER0_LO__SI, mmSPI_PERFCOUNTER0_HI__SI },
        { mmSPI_PERFCOUNTER1_SELECT__SI, 0, mmSPI_PERFCOUNTER1_LO__SI, mmSPI_PERFCOUNTER1_HI__SI },
        { mmSPI_PERFCOUNTER2_SELECT__SI, 0, mmSPI_PERFCOUNTER2_LO__SI, mmSPI_PERFCOUNTER2_HI__SI },
        { mmSPI_PERFCOUNTER3_SELECT__SI, 0, mmSPI_PERFCOUNTER3_LO__SI, mmSPI_PERFCOUNTER3_HI__SI },
    }};

    // The SQ counters are implemented by a single SQG in every shader engine. It has a unique programming model.
    // All gfx6 ASICs only contain 8 out of the possible 16 counter modules.
    PerfCounterBlockInfo*const pSq = &pInfo->block[static_cast<uint32>(GpuBlock::Sq)];
    pSq->distribution              = PerfCounterDistribution::PerShaderEngine;
    pSq->numInstances              = 1;
    pSq->numGlobalOnlyCounters     = 8;
    pSq->numGenericLegacyModules   = 0;
    pSq->maxEventId                = 398; // SQC_PERF_SEL_PT_POWER_STALL

    pSq->regAddr = { 0, {
        { mmSQ_PERFCOUNTER0_SELECT__SI, 0, mmSQ_PERFCOUNTER0_LO__SI, mmSQ_PERFCOUNTER0_HI__SI },
        { mmSQ_PERFCOUNTER1_SELECT__SI, 0, mmSQ_PERFCOUNTER1_LO__SI, mmSQ_PERFCOUNTER1_HI__SI },
        { mmSQ_PERFCOUNTER2_SELECT__SI, 0, mmSQ_PERFCOUNTER2_LO__SI, mmSQ_PERFCOUNTER2_HI__SI },
        { mmSQ_PERFCOUNTER3_SELECT__SI, 0, mmSQ_PERFCOUNTER3_LO__SI, mmSQ_PERFCOUNTER3_HI__SI },
        { mmSQ_PERFCOUNTER4_SELECT__SI, 0, mmSQ_PERFCOUNTER4_LO__SI, mmSQ_PERFCOUNTER4_HI__SI },
        { mmSQ_PERFCOUNTER5_SELECT__SI, 0, mmSQ_PERFCOUNTER5_LO__SI, mmSQ_PERFCOUNTER5_HI__SI },
        { mmSQ_PERFCOUNTER6_SELECT__SI, 0, mmSQ_PERFCOUNTER6_LO__SI, mmSQ_PERFCOUNTER6_HI__SI },
        { mmSQ_PERFCOUNTER7_SELECT__SI, 0, mmSQ_PERFCOUNTER7_LO__SI, mmSQ_PERFCOUNTER7_HI__SI },
    }};

    PerfCounterBlockInfo*const pSx = &pInfo->block[static_cast<uint32>(GpuBlock::Sx)];
    pSx->distribution              = PerfCounterDistribution::PerShaderArray;
    pSx->numInstances              = 1;
    pSx->numGenericLegacyModules   = 4;  // SX_PERFCOUNTER0-3
    pSx->maxEventId                = 31; // SX_PERF_SEL_DB3_PRED_PIXELS

    pSx->regAddr = { 0, {
        { mmSX_PERFCOUNTER0_SELECT__SI, 0, mmSX_PERFCOUNTER0_LO__SI, mmSX_PERFCOUNTER0_HI__SI },
        { mmSX_PERFCOUNTER1_SELECT__SI, 0, mmSX_PERFCOUNTER1_LO__SI, mmSX_PERFCOUNTER1_HI__SI },
        { mmSX_PERFCOUNTER2_SELECT__SI, 0, mmSX_PERFCOUNTER2_LO__SI, mmSX_PERFCOUNTER2_HI__SI },
        { mmSX_PERFCOUNTER3_SELECT__SI, 0, mmSX_PERFCOUNTER3_LO__SI, mmSX_PERFCOUNTER3_HI__SI },
    }};

    PerfCounterBlockInfo*const pTa = &pInfo->block[static_cast<uint32>(GpuBlock::Ta)];
    pTa->distribution              = PerfCounterDistribution::PerShaderArray;
    pTa->numInstances              = pProps->gfx6.numCuPerSh;
    pTa->numGenericLegacyModules   = 2; // TA_PERFCOUNTER0-1
    pTa->maxEventId                = TA_PERF_SEL_local_cg_dyn_sclk_grp5_en__SI;

    pTa->regAddr = { 0, {
        { mmTA_PERFCOUNTER0_SELECT__SI, 0, mmTA_PERFCOUNTER0_LO__SI, mmTA_PERFCOUNTER0_HI__SI },
        { mmTA_PERFCOUNTER1_SELECT__SI, 0, mmTA_PERFCOUNTER1_LO__SI, mmTA_PERFCOUNTER1_HI__SI },
    }};

    PerfCounterBlockInfo*const pTd = &pInfo->block[static_cast<uint32>(GpuBlock::Td)];
    pTd->distribution              = PerfCounterDistribution::PerShaderArray;
    pTd->numInstances              = pProps->gfx6.numCuPerSh;
    pTd->numGenericLegacyModules   = 1; // TD_PERFCOUNTER0
    pTd->maxEventId                = TD_PERF_SEL_data_poison__SI;

    pTd->regAddr = { 0, {
        { mmTD_PERFCOUNTER0_SELECT__SI, 0, mmTD_PERFCOUNTER0_LO__SI, mmTD_PERFCOUNTER0_HI__SI },
    }};

    PerfCounterBlockInfo*const pTcp = &pInfo->block[static_cast<uint32>(GpuBlock::Tcp)];
    pTcp->distribution              = PerfCounterDistribution::PerShaderArray;
    pTcp->numInstances              = pProps->gfx6.numCuPerSh;
    pTcp->numGenericLegacyModules   = 4; // TCP_PERFCOUNTER0-3
    pTcp->maxEventId                = TCP_PERF_SEL_CORE_REG_SCLK_VLD__SI;

    pTcp->regAddr = { 0, {
        { mmTCP_PERFCOUNTER0_SELECT__SI, 0, mmTCP_PERFCOUNTER0_LO__SI, mmTCP_PERFCOUNTER0_HI__SI },
        { mmTCP_PERFCOUNTER1_SELECT__SI, 0, mmTCP_PERFCOUNTER1_LO__SI, mmTCP_PERFCOUNTER1_HI__SI },
        { mmTCP_PERFCOUNTER2_SELECT__SI, 0, mmTCP_PERFCOUNTER2_LO__SI, mmTCP_PERFCOUNTER2_HI__SI },
        { mmTCP_PERFCOUNTER3_SELECT__SI, 0, mmTCP_PERFCOUNTER3_LO__SI, mmTCP_PERFCOUNTER3_HI__SI },
    }};

    PerfCounterBlockInfo*const pTcc = &pInfo->block[static_cast<uint32>(GpuBlock::Tcc)];
    pTcc->distribution              = PerfCounterDistribution::GlobalBlock,
    pTcc->numInstances              = pProps->gfx6.numTccBlocks;
    pTcc->numGenericLegacyModules   = 4; // TCC_PERFCOUNTER0-3
    pTcc->maxEventId                = TCC_PERF_SEL_CLIENT63_REQ__SI__CI;

    pTcc->regAddr = { 0, {
        { mmTCC_PERFCOUNTER0_SELECT__SI, 0, mmTCC_PERFCOUNTER0_LO__SI, mmTCC_PERFCOUNTER0_HI__SI },
        { mmTCC_PERFCOUNTER1_SELECT__SI, 0, mmTCC_PERFCOUNTER1_LO__SI, mmTCC_PERFCOUNTER1_HI__SI },
        { mmTCC_PERFCOUNTER2_SELECT__SI, 0, mmTCC_PERFCOUNTER2_LO__SI, mmTCC_PERFCOUNTER2_HI__SI },
        { mmTCC_PERFCOUNTER3_SELECT__SI, 0, mmTCC_PERFCOUNTER3_LO__SI, mmTCC_PERFCOUNTER3_HI__SI },
    }};

    PerfCounterBlockInfo*const pTca = &pInfo->block[static_cast<uint32>(GpuBlock::Tca)];
    pTca->distribution              = PerfCounterDistribution::GlobalBlock,
    pTca->numInstances              = 2;
    pTca->numGenericLegacyModules   = 4; // TCA_PERFCOUNTER0-3
    pTca->maxEventId                = TCA_PERF_SEL_CROSSBAR_STALL_TCC7;

    pTca->regAddr = { 0, {
        { mmTCA_PERFCOUNTER0_SELECT__SI, 0, mmTCA_PERFCOUNTER0_LO__SI, mmTCA_PERFCOUNTER0_HI__SI },
        { mmTCA_PERFCOUNTER1_SELECT__SI, 0, mmTCA_PERFCOUNTER1_LO__SI, mmTCA_PERFCOUNTER1_HI__SI },
        { mmTCA_PERFCOUNTER2_SELECT__SI, 0, mmTCA_PERFCOUNTER2_LO__SI, mmTCA_PERFCOUNTER2_HI__SI },
        { mmTCA_PERFCOUNTER3_SELECT__SI, 0, mmTCA_PERFCOUNTER3_LO__SI, mmTCA_PERFCOUNTER3_HI__SI },
    }};

    PerfCounterBlockInfo*const pDb = &pInfo->block[static_cast<uint32>(GpuBlock::Db)];
    pDb->distribution              = PerfCounterDistribution::PerShaderArray;
    pDb->numInstances              = pProps->gfx6.maxNumRbPerSe / pProps->gfx6.numShaderArrays;
    pDb->numGenericLegacyModules   = 4; // DB_PERFCOUNTER0-3
    pDb->maxEventId                = DB_PERF_SEL_PostZ_Samples_failing_DB__SI;

    pDb->regAddr = { 0, {
        { mmDB_PERFCOUNTER0_SELECT__SI, 0, mmDB_PERFCOUNTER0_LO__SI, mmDB_PERFCOUNTER0_HI__SI },
        { mmDB_PERFCOUNTER1_SELECT__SI, 0, mmDB_PERFCOUNTER1_LO__SI, mmDB_PERFCOUNTER1_HI__SI },
        { mmDB_PERFCOUNTER2_SELECT__SI, 0, mmDB_PERFCOUNTER2_LO__SI, mmDB_PERFCOUNTER2_HI__SI },
        { mmDB_PERFCOUNTER3_SELECT__SI, 0, mmDB_PERFCOUNTER3_LO__SI, mmDB_PERFCOUNTER3_HI__SI },
    }};

    // The CB registers do have the ability to filter based on many properties but we don't implement those filters.
    // Treating these counters as generic legacy registers will get the filters set to zero (disabled).
    PerfCounterBlockInfo*const pCb = &pInfo->block[static_cast<uint32>(GpuBlock::Cb)];
    pCb->distribution              = PerfCounterDistribution::PerShaderArray;
    pCb->numInstances              = pProps->gfx6.maxNumRbPerSe / pProps->gfx6.numShaderArrays;
    pCb->numGenericLegacyModules   = 4; // CB_PERFCOUNTER0-3
    pCb->maxEventId                = CB_PERF_SEL_QUAD_COULD_HAVE_BEEN_DISCARDED__SI__CI;

    pCb->regAddr = { 0, {
        { mmCB_PERFCOUNTER0_SELECT0__SI, 0, mmCB_PERFCOUNTER0_LO__SI, mmCB_PERFCOUNTER0_HI__SI },
        { mmCB_PERFCOUNTER1_SELECT0__SI, 0, mmCB_PERFCOUNTER1_LO__SI, mmCB_PERFCOUNTER1_HI__SI },
        { mmCB_PERFCOUNTER2_SELECT0__SI, 0, mmCB_PERFCOUNTER2_LO__SI, mmCB_PERFCOUNTER2_HI__SI },
        { mmCB_PERFCOUNTER3_SELECT0__SI, 0, mmCB_PERFCOUNTER3_LO__SI, mmCB_PERFCOUNTER3_HI__SI },
    }};

    PerfCounterBlockInfo*const pGds = &pInfo->block[static_cast<uint32>(GpuBlock::Gds)];
    pGds->distribution              = PerfCounterDistribution::GlobalBlock;
    pGds->numInstances              = 1;
    pGds->numGenericLegacyModules   = 4;  // GDS_PERFCOUNTER0-3
    pGds->maxEventId                = 64; // GDS_PERF_SEL_GWS_BYPASS

    pGds->regAddr = { 0, {
        { mmGDS_PERFCOUNTER0_SELECT__SI, 0, mmGDS_PERFCOUNTER0_LO__SI, mmGDS_PERFCOUNTER0_HI__SI },
        { mmGDS_PERFCOUNTER1_SELECT__SI, 0, mmGDS_PERFCOUNTER1_LO__SI, mmGDS_PERFCOUNTER1_HI__SI },
        { mmGDS_PERFCOUNTER2_SELECT__SI, 0, mmGDS_PERFCOUNTER2_LO__SI, mmGDS_PERFCOUNTER2_HI__SI },
        { mmGDS_PERFCOUNTER3_SELECT__SI, 0, mmGDS_PERFCOUNTER3_LO__SI, mmGDS_PERFCOUNTER3_HI__SI },
    }};

    PerfCounterBlockInfo*const pSrbm = &pInfo->block[static_cast<uint32>(GpuBlock::Srbm)];
    pSrbm->distribution              = PerfCounterDistribution::GlobalBlock;
    pSrbm->numInstances              = 1;
    pSrbm->numGenericLegacyModules   = 2; // SRBM_PERFCOUNTER0-1
    pSrbm->maxEventId                = SRBM_PERF_SEL_XDMA_BUSY;

    pSrbm->regAddr = { 0, {
        { mmSRBM_PERFCOUNTER0_SELECT__SI__CI, 0, mmSRBM_PERFCOUNTER0_LO__SI__CI, mmSRBM_PERFCOUNTER0_HI__SI__CI },
        { mmSRBM_PERFCOUNTER1_SELECT__SI__CI, 0, mmSRBM_PERFCOUNTER1_LO__SI__CI, mmSRBM_PERFCOUNTER1_HI__SI__CI },
    }};

    PerfCounterBlockInfo*const pGrbm = &pInfo->block[static_cast<uint32>(GpuBlock::Grbm)];
    pGrbm->distribution              = PerfCounterDistribution::GlobalBlock;
    pGrbm->numInstances              = 1;
    pGrbm->numGenericLegacyModules   = 2; // GRBM_PERFCOUNTER0-1
    pGrbm->maxEventId                = GRBM_PERF_SEL_TC_BUSY;

    pGrbm->regAddr = { 0, {
        { mmGRBM_PERFCOUNTER0_SELECT__SI, 0, mmGRBM_PERFCOUNTER0_LO__SI, mmGRBM_PERFCOUNTER0_HI__SI },
        { mmGRBM_PERFCOUNTER1_SELECT__SI, 0, mmGRBM_PERFCOUNTER1_LO__SI, mmGRBM_PERFCOUNTER1_HI__SI },
    }};

    // These counters are a bit special. The GRBM is a global block but it defines one special counter per SE. We
    // abstract this as a special Grbm(per)Se block which needs special handling in the perf experiment.
    PerfCounterBlockInfo*const pGrbmSe = &pInfo->block[static_cast<uint32>(GpuBlock::GrbmSe)];
    pGrbmSe->distribution              = PerfCounterDistribution::PerShaderEngine;
    pGrbmSe->numInstances              = 1;
    pGrbmSe->numGlobalOnlyCounters     = 1;
    pGrbmSe->numGenericLegacyModules   = 0;
    pGrbmSe->maxEventId                = GRBM_SE0_PERF_SEL_BCI_BUSY;

    // By convention we access the counter register address array using the SE index.
    pGrbmSe->regAddr = { 0, {
        { mmGRBM_SE0_PERFCOUNTER_SELECT__SI, 0, mmGRBM_SE0_PERFCOUNTER_LO__SI, mmGRBM_SE0_PERFCOUNTER_HI__SI },
        { mmGRBM_SE1_PERFCOUNTER_SELECT__SI, 0, mmGRBM_SE1_PERFCOUNTER_LO__SI, mmGRBM_SE1_PERFCOUNTER_HI__SI },
    }};

    // The MC uses a unique programming model; most registers are handled by the perf experiment but we must set up
    // the ASIC-specific MC_CONFIG info. Each MCD defines four counters for each of its two channels. We abstract
    // each channel as its own MC instance.
    PerfCounterBlockInfo*const pMc = &pInfo->block[static_cast<uint32>(GpuBlock::Mc)];
    pMc->distribution              = PerfCounterDistribution::GlobalBlock;
    pMc->numInstances              = NumMcChannels * pProps->gfx6.numMcdTiles; // 2 channels per MCD
    pMc->numGlobalOnlyCounters     = 4;
    pMc->numGenericLegacyModules   = 0;
    pMc->maxEventId                = 21; // Write to Read detected

    // By convention SEQ_CTL is the first select, CNTL_1 is the second select, the "Lo" registers are for channel 0,
    // and the "Hi" registers are for channel 1.
    pMc->regAddr = { 0, {
        { mmMC_SEQ_PERF_SEQ_CTL__SI__CI, mmMC_SEQ_PERF_CNTL_1__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_A_I0__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_A_I1__SI__CI },
        { mmMC_SEQ_PERF_SEQ_CTL__SI__CI, mmMC_SEQ_PERF_CNTL_1__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_B_I0__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_B_I1__SI__CI },
        { mmMC_SEQ_PERF_SEQ_CTL__SI__CI, mmMC_SEQ_PERF_CNTL_1__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_C_I0__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_C_I1__SI__CI },
        { mmMC_SEQ_PERF_SEQ_CTL__SI__CI, mmMC_SEQ_PERF_CNTL_1__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_D_I0__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_D_I1__SI__CI },
    }};
}

// =====================================================================================================================
// Initializes each block's basic hardware-defined information (distribution, numInstances, numGenericSpmModules, etc.)
static void Gfx7InitBasicBlockInfo(
    const Pal::Device& device,
    GpuChipProperties* pProps)
{
    Gfx6PerfCounterInfo*const pInfo = &pProps->gfx6.perfCounterInfo;

    // Hard-code hardware specific constants for each block. The ATC and VM_L2 seem like they have counters on gfx7 but
    // the old code didn't implement it. We might consider exposing it in the future.
    //
    // The distribution and numInstances (per-distribution) are derived from our hardware architecture.
    // The generic module counts are determined by:
    //   1. Does the block follow the generic programming model as defined by the perf experiment code?
    //   2. If so, there's one SPM module for each SELECT/SELECT1 pair and one legacy module for the remaining SELECTs.
    // The number of SPM wires is a hardware constant baked into each ASIC's design. So are the SPM block selects.
    // The maximum event IDs are the largest values from the hardware perf_sel enums.
    // Finally, we hard-code the PERFCOUNTER# register addresses for each module.

    PerfCounterBlockInfo*const pCpf = &pInfo->block[static_cast<uint32>(GpuBlock::Cpf)];
    pCpf->distribution              = PerfCounterDistribution::GlobalBlock;
    pCpf->numInstances              = 1;
    pCpf->numGenericSpmModules      = 1; // CPF_PERFCOUNTER0
    pCpf->numGenericLegacyModules   = 1; // CPF_PERFCOUNTER1
    pCpf->numSpmWires               = 2;
    pCpf->spmBlockSelect            = Gfx7SpmGlobalBlockSelectCpf;
    pCpf->maxEventId                = CPF_PERF_SEL_MIU_READ_REQUEST_SEND;

    pCpf->regAddr = { 0, {
        { mmCPF_PERFCOUNTER0_SELECT__CI__VI, mmCPF_PERFCOUNTER0_SELECT1__CI__VI, mmCPF_PERFCOUNTER0_LO__CI__VI, mmCPF_PERFCOUNTER0_HI__CI__VI },
        { mmCPF_PERFCOUNTER1_SELECT__CI__VI, 0,                                  mmCPF_PERFCOUNTER1_LO__CI__VI, mmCPF_PERFCOUNTER1_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pIa = &pInfo->block[static_cast<uint32>(GpuBlock::Ia)];
    pIa->distribution              = PerfCounterDistribution::GlobalBlock;
    pIa->numInstances              = Max(pProps->gfx6.numShaderEngines / 2u, 1u);
    pIa->numGenericSpmModules      = 1; // IA_PERFCOUNTER0
    pIa->numGenericLegacyModules   = 3; // IA_PERFCOUNTER1-3
    pIa->numSpmWires               = 2;
    pIa->spmBlockSelect            = Gfx7SpmGlobalBlockSelectIa;
    pIa->maxEventId                = ia_perf_ia_stalled__CI;

    pIa->regAddr = { 0, {
        { mmIA_PERFCOUNTER0_SELECT__CI__VI, mmIA_PERFCOUNTER0_SELECT1__CI__VI, mmIA_PERFCOUNTER0_LO__CI__VI, mmIA_PERFCOUNTER0_HI__CI__VI },
        { mmIA_PERFCOUNTER1_SELECT__CI__VI, 0,                                 mmIA_PERFCOUNTER1_LO__CI__VI, mmIA_PERFCOUNTER1_HI__CI__VI },
        { mmIA_PERFCOUNTER2_SELECT__CI__VI, 0,                                 mmIA_PERFCOUNTER2_LO__CI__VI, mmIA_PERFCOUNTER2_HI__CI__VI },
        { mmIA_PERFCOUNTER3_SELECT__CI__VI, 0,                                 mmIA_PERFCOUNTER3_LO__CI__VI, mmIA_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pVgt = &pInfo->block[static_cast<uint32>(GpuBlock::Vgt)];
    pVgt->distribution              = PerfCounterDistribution::PerShaderEngine;
    pVgt->numInstances              = 1;
    pVgt->numGenericSpmModules      = 2; // VGT_PERFCOUNTER0-1
    pVgt->numGenericLegacyModules   = 2; // VGT_PERFCOUNTER2-3
    pVgt->numSpmWires               = 3;
    pVgt->spmBlockSelect            = Gfx7SpmSeBlockSelectVgt;
    pVgt->maxEventId                = vgt_perf_hs_tgs_active_high_water_mark__SI__CI;

    pVgt->regAddr = { 0, {
        { mmVGT_PERFCOUNTER0_SELECT__CI__VI, mmVGT_PERFCOUNTER0_SELECT1__CI__VI, mmVGT_PERFCOUNTER0_LO__CI__VI, mmVGT_PERFCOUNTER0_HI__CI__VI },
        { mmVGT_PERFCOUNTER1_SELECT__CI__VI, mmVGT_PERFCOUNTER1_SELECT1__CI__VI, mmVGT_PERFCOUNTER1_LO__CI__VI, mmVGT_PERFCOUNTER1_HI__CI__VI },
        { mmVGT_PERFCOUNTER2_SELECT__CI__VI, 0,                                  mmVGT_PERFCOUNTER2_LO__CI__VI, mmVGT_PERFCOUNTER2_HI__CI__VI },
        { mmVGT_PERFCOUNTER3_SELECT__CI__VI, 0,                                  mmVGT_PERFCOUNTER3_LO__CI__VI, mmVGT_PERFCOUNTER3_HI__CI__VI },
    }};

    // Note that the PA uses the SU select enum.
    PerfCounterBlockInfo*const pPa = &pInfo->block[static_cast<uint32>(GpuBlock::Pa)];
    pPa->distribution              = PerfCounterDistribution::PerShaderEngine;
    pPa->numInstances              = 1;
    pPa->numGenericSpmModules      = 2; // PA_SU_PERFCOUNTER0-1
    pPa->numGenericLegacyModules   = 2; // PA_SU_PERFCOUNTER2-3
    pPa->numSpmWires               = 3;
    pPa->spmBlockSelect            = Gfx7SpmSeBlockSelectPa;
    pPa->maxEventId                = PERF_PAPC_SU_SE3_STALLED_SC__CI__VI;

    pPa->regAddr = { 0, {
        { mmPA_SU_PERFCOUNTER0_SELECT__CI__VI, mmPA_SU_PERFCOUNTER0_SELECT1__CI__VI, mmPA_SU_PERFCOUNTER0_LO__CI__VI, mmPA_SU_PERFCOUNTER0_HI__CI__VI },
        { mmPA_SU_PERFCOUNTER1_SELECT__CI__VI, mmPA_SU_PERFCOUNTER1_SELECT1__CI__VI, mmPA_SU_PERFCOUNTER1_LO__CI__VI, mmPA_SU_PERFCOUNTER1_HI__CI__VI },
        { mmPA_SU_PERFCOUNTER2_SELECT__CI__VI, 0,                                    mmPA_SU_PERFCOUNTER2_LO__CI__VI, mmPA_SU_PERFCOUNTER2_HI__CI__VI },
        { mmPA_SU_PERFCOUNTER3_SELECT__CI__VI, 0,                                    mmPA_SU_PERFCOUNTER3_LO__CI__VI, mmPA_SU_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pSc = &pInfo->block[static_cast<uint32>(GpuBlock::Sc)];
    pSc->distribution              = PerfCounterDistribution::PerShaderEngine;
    pSc->numInstances              = 1;
    pSc->numGenericSpmModules      = 1; // PA_SC_PERFCOUNTER0
    pSc->numGenericLegacyModules   = 7; // PA_SC_PERFCOUNTER1-7
    pSc->numSpmWires               = 2;
    pSc->spmBlockSelect            = Gfx7SpmSeBlockSelectSc;
    pSc->maxEventId                = SC_SCB_BUSY__CI__VI;

    pSc->regAddr = { 0, {
        { mmPA_SC_PERFCOUNTER0_SELECT__CI__VI, mmPA_SC_PERFCOUNTER0_SELECT1__CI__VI, mmPA_SC_PERFCOUNTER0_LO__CI__VI, mmPA_SC_PERFCOUNTER0_HI__CI__VI },
        { mmPA_SC_PERFCOUNTER1_SELECT__CI__VI, 0,                                    mmPA_SC_PERFCOUNTER1_LO__CI__VI, mmPA_SC_PERFCOUNTER1_HI__CI__VI },
        { mmPA_SC_PERFCOUNTER2_SELECT__CI__VI, 0,                                    mmPA_SC_PERFCOUNTER2_LO__CI__VI, mmPA_SC_PERFCOUNTER2_HI__CI__VI },
        { mmPA_SC_PERFCOUNTER3_SELECT__CI__VI, 0,                                    mmPA_SC_PERFCOUNTER3_LO__CI__VI, mmPA_SC_PERFCOUNTER3_HI__CI__VI },
        { mmPA_SC_PERFCOUNTER4_SELECT__CI__VI, 0,                                    mmPA_SC_PERFCOUNTER4_LO__CI__VI, mmPA_SC_PERFCOUNTER4_HI__CI__VI },
        { mmPA_SC_PERFCOUNTER5_SELECT__CI__VI, 0,                                    mmPA_SC_PERFCOUNTER5_LO__CI__VI, mmPA_SC_PERFCOUNTER5_HI__CI__VI },
        { mmPA_SC_PERFCOUNTER6_SELECT__CI__VI, 0,                                    mmPA_SC_PERFCOUNTER6_LO__CI__VI, mmPA_SC_PERFCOUNTER6_HI__CI__VI },
        { mmPA_SC_PERFCOUNTER7_SELECT__CI__VI, 0,                                    mmPA_SC_PERFCOUNTER7_LO__CI__VI, mmPA_SC_PERFCOUNTER7_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pSpi = &pInfo->block[static_cast<uint32>(GpuBlock::Spi)];
    pSpi->distribution              = PerfCounterDistribution::PerShaderEngine;
    pSpi->numInstances              = 1;
    pSpi->numGenericSpmModules      = 4; // SPI_PERFCOUNTER0-3
    pSpi->numGenericLegacyModules   = 2; // SPI_PERFCOUNTER4-5
    pSpi->numSpmWires               = 8;
    pSpi->spmBlockSelect            = Gfx7SpmSeBlockSelectSpi;
    pSpi->maxEventId                = SPI_PERF_CLKGATE_CGTT_REG_ON__CI__VI;

    pSpi->regAddr = { 0, {
        { mmSPI_PERFCOUNTER0_SELECT__CI__VI, mmSPI_PERFCOUNTER0_SELECT1__CI__VI, mmSPI_PERFCOUNTER0_LO__CI__VI, mmSPI_PERFCOUNTER0_HI__CI__VI },
        { mmSPI_PERFCOUNTER1_SELECT__CI__VI, mmSPI_PERFCOUNTER1_SELECT1__CI__VI, mmSPI_PERFCOUNTER1_LO__CI__VI, mmSPI_PERFCOUNTER1_HI__CI__VI },
        { mmSPI_PERFCOUNTER2_SELECT__CI__VI, mmSPI_PERFCOUNTER2_SELECT1__CI__VI, mmSPI_PERFCOUNTER2_LO__CI__VI, mmSPI_PERFCOUNTER2_HI__CI__VI },
        { mmSPI_PERFCOUNTER3_SELECT__CI__VI, mmSPI_PERFCOUNTER3_SELECT1__CI__VI, mmSPI_PERFCOUNTER3_LO__CI__VI, mmSPI_PERFCOUNTER3_HI__CI__VI },
        { mmSPI_PERFCOUNTER4_SELECT__CI__VI, 0,                                  mmSPI_PERFCOUNTER4_LO__CI__VI, mmSPI_PERFCOUNTER4_HI__CI__VI },
        { mmSPI_PERFCOUNTER5_SELECT__CI__VI, 0,                                  mmSPI_PERFCOUNTER5_LO__CI__VI, mmSPI_PERFCOUNTER5_HI__CI__VI },
    }};

    // The SQ counters are implemented by a single SQG in every shader engine. It has a unique programming model.
    // The SQ counter modules can be a global counter or one 32-bit SPM counter. 16-bit SPM is not supported but we
    // fake one 16-bit counter for now. All gfx7 ASICs only contain 8 out of the possible 16 counter modules.
    PerfCounterBlockInfo*const pSq = &pInfo->block[static_cast<uint32>(GpuBlock::Sq)];
    pSq->distribution              = PerfCounterDistribution::PerShaderEngine;
    pSq->numInstances              = 1;
    pSq->num16BitSpmCounters       = 8;
    pSq->num32BitSpmCounters       = 8;
    pSq->numGlobalSharedCounters   = 8;
    pSq->numGenericSpmModules      = 0;
    pSq->numGenericLegacyModules   = 0;
    pSq->numSpmWires               = 8;
    pSq->spmBlockSelect            = Gfx7SpmSeBlockSelectSqg;
    pSq->maxEventId                = 250; // SQC_PERF_SEL_ERR_DCACHE_REQ_16_GPR_ADDR_UNALIGNED

    pSq->regAddr = { 0, {
        { mmSQ_PERFCOUNTER0_SELECT__CI__VI, 0, mmSQ_PERFCOUNTER0_LO__CI__VI, mmSQ_PERFCOUNTER0_HI__CI__VI },
        { mmSQ_PERFCOUNTER1_SELECT__CI__VI, 0, mmSQ_PERFCOUNTER1_LO__CI__VI, mmSQ_PERFCOUNTER1_HI__CI__VI },
        { mmSQ_PERFCOUNTER2_SELECT__CI__VI, 0, mmSQ_PERFCOUNTER2_LO__CI__VI, mmSQ_PERFCOUNTER2_HI__CI__VI },
        { mmSQ_PERFCOUNTER3_SELECT__CI__VI, 0, mmSQ_PERFCOUNTER3_LO__CI__VI, mmSQ_PERFCOUNTER3_HI__CI__VI },
        { mmSQ_PERFCOUNTER4_SELECT__CI__VI, 0, mmSQ_PERFCOUNTER4_LO__CI__VI, mmSQ_PERFCOUNTER4_HI__CI__VI },
        { mmSQ_PERFCOUNTER5_SELECT__CI__VI, 0, mmSQ_PERFCOUNTER5_LO__CI__VI, mmSQ_PERFCOUNTER5_HI__CI__VI },
        { mmSQ_PERFCOUNTER6_SELECT__CI__VI, 0, mmSQ_PERFCOUNTER6_LO__CI__VI, mmSQ_PERFCOUNTER6_HI__CI__VI },
        { mmSQ_PERFCOUNTER7_SELECT__CI__VI, 0, mmSQ_PERFCOUNTER7_LO__CI__VI, mmSQ_PERFCOUNTER7_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pSx = &pInfo->block[static_cast<uint32>(GpuBlock::Sx)];
    pSx->distribution              = PerfCounterDistribution::PerShaderArray;
    pSx->numInstances              = 1;
    pSx->numGenericSpmModules      = 2; // SX_PERFCOUNTER0-1
    pSx->numGenericLegacyModules   = 2; // SX_PERFCOUNTER2-3
    pSx->numSpmWires               = 4;
    pSx->spmBlockSelect            = Gfx7SpmSeBlockSelectSx;
    pSx->maxEventId                = 33; // SX_PERF_SEL_POS_BUSY

    pSx->regAddr = { 0, {
        { mmSX_PERFCOUNTER0_SELECT__CI__VI, mmSX_PERFCOUNTER0_SELECT1__CI__VI, mmSX_PERFCOUNTER0_LO__CI__VI, mmSX_PERFCOUNTER0_HI__CI__VI },
        { mmSX_PERFCOUNTER1_SELECT__CI__VI, mmSX_PERFCOUNTER1_SELECT1__CI__VI, mmSX_PERFCOUNTER1_LO__CI__VI, mmSX_PERFCOUNTER1_HI__CI__VI },
        { mmSX_PERFCOUNTER2_SELECT__CI__VI, 0,                                 mmSX_PERFCOUNTER2_LO__CI__VI, mmSX_PERFCOUNTER2_HI__CI__VI },
        { mmSX_PERFCOUNTER3_SELECT__CI__VI, 0,                                 mmSX_PERFCOUNTER3_LO__CI__VI, mmSX_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pTa = &pInfo->block[static_cast<uint32>(GpuBlock::Ta)];
    pTa->distribution              = PerfCounterDistribution::PerShaderArray;
    pTa->numInstances              = pProps->gfx6.numCuPerSh;
    pTa->numGenericSpmModules      = 1; // TA_PERFCOUNTER0
    pTa->numGenericLegacyModules   = 1; // TA_PERFCOUNTER1
    pTa->numSpmWires               = 2;
    pTa->spmBlockSelect            = Gfx7SpmSeBlockSelectTa;
    pTa->maxEventId                = TA_PERF_SEL_local_cg_dyn_sclk_grp5_en__CI__VI;

    pTa->regAddr = { 0, {
        { mmTA_PERFCOUNTER0_SELECT__CI__VI, mmTA_PERFCOUNTER0_SELECT1__CI__VI, mmTA_PERFCOUNTER0_LO__CI__VI, mmTA_PERFCOUNTER0_HI__CI__VI },
        { mmTA_PERFCOUNTER1_SELECT__CI__VI, 0,                                 mmTA_PERFCOUNTER1_LO__CI__VI, mmTA_PERFCOUNTER1_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pTd = &pInfo->block[static_cast<uint32>(GpuBlock::Td)];
    pTd->distribution              = PerfCounterDistribution::PerShaderArray;
    pTd->numInstances              = pProps->gfx6.numCuPerSh;
    pTd->numGenericSpmModules      = 1; // TD_PERFCOUNTER0
    pTd->numGenericLegacyModules   = 1; // TD_PERFCOUNTER1
    pTd->numSpmWires               = 2;
    pTd->spmBlockSelect            = Gfx7SpmSeBlockSelectTd;
    pTd->maxEventId                = TD_PERF_SEL_null_cycle_output__CI__VI;

    pTd->regAddr = { 0, {
        { mmTD_PERFCOUNTER0_SELECT__CI__VI, mmTD_PERFCOUNTER0_SELECT1__CI__VI, mmTD_PERFCOUNTER0_LO__CI__VI, mmTD_PERFCOUNTER0_HI__CI__VI },
        { mmTD_PERFCOUNTER1_SELECT__CI__VI, 0,                                 mmTD_PERFCOUNTER1_LO__CI__VI, mmTD_PERFCOUNTER1_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pTcp = &pInfo->block[static_cast<uint32>(GpuBlock::Tcp)];
    pTcp->distribution              = PerfCounterDistribution::PerShaderArray;
    pTcp->numInstances              = pProps->gfx6.numCuPerSh;
    pTcp->numGenericSpmModules      = 2; // TCP_PERFCOUNTER0-1
    pTcp->numGenericLegacyModules   = 2; // TCP_PERFCOUNTER2-3
    pTcp->numSpmWires               = 3;
    pTcp->spmBlockSelect            = Gfx7SpmSeBlockSelectTcp;
    pTcp->maxEventId                = TCP_PERF_SEL_POWER_STALL__CI__VI;

    pTcp->regAddr = { 0, {
        { mmTCP_PERFCOUNTER0_SELECT__CI__VI, mmTCP_PERFCOUNTER0_SELECT1__CI__VI, mmTCP_PERFCOUNTER0_LO__CI__VI, mmTCP_PERFCOUNTER0_HI__CI__VI },
        { mmTCP_PERFCOUNTER1_SELECT__CI__VI, mmTCP_PERFCOUNTER1_SELECT1__CI__VI, mmTCP_PERFCOUNTER1_LO__CI__VI, mmTCP_PERFCOUNTER1_HI__CI__VI },
        { mmTCP_PERFCOUNTER2_SELECT__CI__VI, 0,                                  mmTCP_PERFCOUNTER2_LO__CI__VI, mmTCP_PERFCOUNTER2_HI__CI__VI },
        { mmTCP_PERFCOUNTER3_SELECT__CI__VI, 0,                                  mmTCP_PERFCOUNTER3_LO__CI__VI, mmTCP_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pTcc = &pInfo->block[static_cast<uint32>(GpuBlock::Tcc)];
    pTcc->distribution              = PerfCounterDistribution::GlobalBlock,
    pTcc->numInstances              = pProps->gfx6.numTccBlocks;
    pTcc->numGenericSpmModules      = 2; // TCC_PERFCOUNTER0-1
    pTcc->numGenericLegacyModules   = 2; // TCC_PERFCOUNTER2-3
    pTcc->numSpmWires               = 4;
    pTcc->spmBlockSelect            = Gfx7SpmGlobalBlockSelectTcc;
    pTcc->maxEventId                = TCC_PERF_SEL_VOL_REQ__CI;

    pTcc->regAddr = { 0, {
        { mmTCC_PERFCOUNTER0_SELECT__CI__VI, mmTCC_PERFCOUNTER0_SELECT1__CI__VI, mmTCC_PERFCOUNTER0_LO__CI__VI, mmTCC_PERFCOUNTER0_HI__CI__VI },
        { mmTCC_PERFCOUNTER1_SELECT__CI__VI, mmTCC_PERFCOUNTER1_SELECT1__CI__VI, mmTCC_PERFCOUNTER1_LO__CI__VI, mmTCC_PERFCOUNTER1_HI__CI__VI },
        { mmTCC_PERFCOUNTER2_SELECT__CI__VI, 0,                                  mmTCC_PERFCOUNTER2_LO__CI__VI, mmTCC_PERFCOUNTER2_HI__CI__VI },
        { mmTCC_PERFCOUNTER3_SELECT__CI__VI, 0,                                  mmTCC_PERFCOUNTER3_LO__CI__VI, mmTCC_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pTca = &pInfo->block[static_cast<uint32>(GpuBlock::Tca)];
    pTca->distribution              = PerfCounterDistribution::GlobalBlock,
    pTca->numInstances              = 2;
    pTca->numGenericSpmModules      = 2; // TCA_PERFCOUNTER0-1
    pTca->numGenericLegacyModules   = 2; // TCA_PERFCOUNTER2-3
    pTca->numSpmWires               = 4;
    pTca->spmBlockSelect            = Gfx7SpmGlobalBlockSelectTca;
    pTca->maxEventId                = TCA_PERF_SEL_CROSSBAR_STALL_TCS__CI;

    pTca->regAddr = { 0, {
        { mmTCA_PERFCOUNTER0_SELECT__CI__VI, mmTCA_PERFCOUNTER0_SELECT1__CI__VI, mmTCA_PERFCOUNTER0_LO__CI__VI, mmTCA_PERFCOUNTER0_HI__CI__VI },
        { mmTCA_PERFCOUNTER1_SELECT__CI__VI, mmTCA_PERFCOUNTER1_SELECT1__CI__VI, mmTCA_PERFCOUNTER1_LO__CI__VI, mmTCA_PERFCOUNTER1_HI__CI__VI },
        { mmTCA_PERFCOUNTER2_SELECT__CI__VI, 0,                                  mmTCA_PERFCOUNTER2_LO__CI__VI, mmTCA_PERFCOUNTER2_HI__CI__VI },
        { mmTCA_PERFCOUNTER3_SELECT__CI__VI, 0,                                  mmTCA_PERFCOUNTER3_LO__CI__VI, mmTCA_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pDb = &pInfo->block[static_cast<uint32>(GpuBlock::Db)];
    pDb->distribution              = PerfCounterDistribution::PerShaderArray;
    pDb->numInstances              = pProps->gfx6.maxNumRbPerSe / pProps->gfx6.numShaderArrays;
    pDb->numGenericSpmModules      = 2; // DB_PERFCOUNTER0-1
    pDb->numGenericLegacyModules   = 2; // DB_PERFCOUNTER2-3
    pDb->numSpmWires               = 3;
    pDb->spmBlockSelect            = Gfx7SpmSeBlockSelectDb;
    pDb->maxEventId                = DB_PERF_SEL_di_dt_stall__CI__VI;

    pDb->regAddr = { 0, {
        { mmDB_PERFCOUNTER0_SELECT__CI__VI, mmDB_PERFCOUNTER0_SELECT1__CI__VI, mmDB_PERFCOUNTER0_LO__CI__VI, mmDB_PERFCOUNTER0_HI__CI__VI },
        { mmDB_PERFCOUNTER1_SELECT__CI__VI, mmDB_PERFCOUNTER1_SELECT1__CI__VI, mmDB_PERFCOUNTER1_LO__CI__VI, mmDB_PERFCOUNTER1_HI__CI__VI },
        { mmDB_PERFCOUNTER2_SELECT__CI__VI, 0,                                 mmDB_PERFCOUNTER2_LO__CI__VI, mmDB_PERFCOUNTER2_HI__CI__VI },
        { mmDB_PERFCOUNTER3_SELECT__CI__VI, 0,                                 mmDB_PERFCOUNTER3_LO__CI__VI, mmDB_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pCb = &pInfo->block[static_cast<uint32>(GpuBlock::Cb)];
    pCb->distribution              = PerfCounterDistribution::PerShaderArray;
    pCb->numInstances              = pProps->gfx6.maxNumRbPerSe / pProps->gfx6.numShaderArrays;
    pCb->numGenericSpmModules      = 1; // CB_PERFCOUNTER0
    pCb->numGenericLegacyModules   = 3; // CB_PERFCOUNTER1-3
    pCb->numSpmWires               = 2;
    pCb->spmBlockSelect            = Gfx7SpmSeBlockSelectCb;
    pCb->maxEventId                = 225; // CB_PERF_SEL_FC_SEQUENCER_FMASK_COMPRESSION_DISABLE

    pCb->regAddr = { 0, {
        { mmCB_PERFCOUNTER0_SELECT__CI__VI, mmCB_PERFCOUNTER0_SELECT1__CI__VI, mmCB_PERFCOUNTER0_LO__CI__VI, mmCB_PERFCOUNTER0_HI__CI__VI },
        { mmCB_PERFCOUNTER1_SELECT__CI__VI, 0,                                 mmCB_PERFCOUNTER1_LO__CI__VI, mmCB_PERFCOUNTER1_HI__CI__VI },
        { mmCB_PERFCOUNTER2_SELECT__CI__VI, 0,                                 mmCB_PERFCOUNTER2_LO__CI__VI, mmCB_PERFCOUNTER2_HI__CI__VI },
        { mmCB_PERFCOUNTER3_SELECT__CI__VI, 0,                                 mmCB_PERFCOUNTER3_LO__CI__VI, mmCB_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pGds = &pInfo->block[static_cast<uint32>(GpuBlock::Gds)];
    pGds->distribution              = PerfCounterDistribution::GlobalBlock;
    pGds->numInstances              = 1;
    pGds->numGenericSpmModules      = 1; // GDS_PERFCOUNTER0
    pGds->numGenericLegacyModules   = 3; // GDS_PERFCOUNTER1-3
    pGds->numSpmWires               = 2;
    pGds->spmBlockSelect            = Gfx7SpmGlobalBlockSelectGds;
    pGds->maxEventId                = 120; // GDS_PERF_SEL_GWS_BYPASS

    pGds->regAddr = { 0, {
        { mmGDS_PERFCOUNTER0_SELECT__CI__VI, mmGDS_PERFCOUNTER0_SELECT1__CI__VI, mmGDS_PERFCOUNTER0_LO__CI__VI, mmGDS_PERFCOUNTER0_HI__CI__VI },
        { mmGDS_PERFCOUNTER1_SELECT__CI__VI, 0,                                  mmGDS_PERFCOUNTER1_LO__CI__VI, mmGDS_PERFCOUNTER1_HI__CI__VI },
        { mmGDS_PERFCOUNTER2_SELECT__CI__VI, 0,                                  mmGDS_PERFCOUNTER2_LO__CI__VI, mmGDS_PERFCOUNTER2_HI__CI__VI },
        { mmGDS_PERFCOUNTER3_SELECT__CI__VI, 0,                                  mmGDS_PERFCOUNTER3_LO__CI__VI, mmGDS_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pSrbm = &pInfo->block[static_cast<uint32>(GpuBlock::Srbm)];
    pSrbm->distribution              = PerfCounterDistribution::GlobalBlock;
    pSrbm->numInstances              = 1;
    pSrbm->numGenericSpmModules      = 0;
    pSrbm->numGenericLegacyModules   = 2; // SRBM_PERFCOUNTER0-1
    pSrbm->maxEventId                = SRBM_PERF_SEL_ACP_BUSY__CI__VI;

    pSrbm->regAddr = { 0, {
        { mmSRBM_PERFCOUNTER0_SELECT__SI__CI, 0, mmSRBM_PERFCOUNTER0_LO__SI__CI, mmSRBM_PERFCOUNTER0_HI__SI__CI },
        { mmSRBM_PERFCOUNTER1_SELECT__SI__CI, 0, mmSRBM_PERFCOUNTER1_LO__SI__CI, mmSRBM_PERFCOUNTER1_HI__SI__CI },
    }};

    PerfCounterBlockInfo*const pGrbm = &pInfo->block[static_cast<uint32>(GpuBlock::Grbm)];
    pGrbm->distribution              = PerfCounterDistribution::GlobalBlock;
    pGrbm->numInstances              = 1;
    pGrbm->numGenericSpmModules      = 0;
    pGrbm->numGenericLegacyModules   = 2; // GRBM_PERFCOUNTER0-1
    pGrbm->maxEventId                = GRBM_PERF_SEL_WD_NO_DMA_BUSY__CI__VI;

    pGrbm->regAddr = { 0, {
        { mmGRBM_PERFCOUNTER0_SELECT__CI__VI, 0, mmGRBM_PERFCOUNTER0_LO__CI__VI, mmGRBM_PERFCOUNTER0_HI__CI__VI },
        { mmGRBM_PERFCOUNTER1_SELECT__CI__VI, 0, mmGRBM_PERFCOUNTER1_LO__CI__VI, mmGRBM_PERFCOUNTER1_HI__CI__VI },
    }};

    // These counters are a bit special. The GRBM is a global block but it defines one special counter per SE. We
    // abstract this as a special Grbm(per)Se block which needs special handling in the perf experiment.
    PerfCounterBlockInfo*const pGrbmSe = &pInfo->block[static_cast<uint32>(GpuBlock::GrbmSe)];
    pGrbmSe->distribution              = PerfCounterDistribution::PerShaderEngine;
    pGrbmSe->numInstances              = 1;
    pGrbmSe->numGlobalOnlyCounters     = 1;
    pGrbmSe->numGenericSpmModules      = 0;
    pGrbmSe->numGenericLegacyModules   = 0;
    pGrbmSe->maxEventId                = GRBM_SE0_PERF_SEL_BCI_BUSY;

    // By convention we access the counter register address array using the SE index.
    pGrbmSe->regAddr = { 0, {
        { mmGRBM_SE0_PERFCOUNTER_SELECT__CI__VI, 0, mmGRBM_SE0_PERFCOUNTER_LO__CI__VI, mmGRBM_SE0_PERFCOUNTER_HI__CI__VI },
        { mmGRBM_SE1_PERFCOUNTER_SELECT__CI__VI, 0, mmGRBM_SE1_PERFCOUNTER_LO__CI__VI, mmGRBM_SE1_PERFCOUNTER_HI__CI__VI },
        { mmGRBM_SE2_PERFCOUNTER_SELECT__CI__VI, 0, mmGRBM_SE2_PERFCOUNTER_LO__CI__VI, mmGRBM_SE2_PERFCOUNTER_HI__CI__VI },
        { mmGRBM_SE3_PERFCOUNTER_SELECT__CI__VI, 0, mmGRBM_SE3_PERFCOUNTER_LO__CI__VI, mmGRBM_SE3_PERFCOUNTER_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pRlc = &pInfo->block[static_cast<uint32>(GpuBlock::Rlc)];
    pRlc->distribution              = PerfCounterDistribution::GlobalBlock;
    pRlc->numInstances              = 1;
    pRlc->numGenericSpmModules      = 0;
    pRlc->numGenericLegacyModules   = 2; // RLC_PERFCOUNTER0-1
    pRlc->maxEventId                = 6; // SERDES command write

    pRlc->regAddr = { 0, {
        { mmRLC_PERFCOUNTER0_SELECT__CI__VI, 0, mmRLC_PERFCOUNTER0_LO__CI__VI, mmRLC_PERFCOUNTER0_HI__CI__VI },
        { mmRLC_PERFCOUNTER1_SELECT__CI__VI, 0, mmRLC_PERFCOUNTER1_LO__CI__VI, mmRLC_PERFCOUNTER1_HI__CI__VI },
    }};

    // The SDMA block has a unique programming model with 2 32-bit counters and unique registers for each instance.
    PerfCounterBlockInfo*const pDma = &pInfo->block[static_cast<uint32>(GpuBlock::Dma)];
    pDma->distribution              = PerfCounterDistribution::GlobalBlock;
    pDma->numInstances              = 2;
    pDma->numGlobalOnlyCounters     = 2;
    pDma->numGenericSpmModules      = 0;
    pDma->numGenericLegacyModules   = 0;
    pDma->maxEventId                = SDMA_PERF_SEL_CE_WR_STALL;

    pInfo->sdmaRegAddr[0][0] = { mmSDMA0_PERFMON_CNTL__CI, 0, mmSDMA0_PERFCOUNTER0_RESULT__CI, 0 };
    pInfo->sdmaRegAddr[0][1] = { mmSDMA0_PERFMON_CNTL__CI, 0, mmSDMA0_PERFCOUNTER1_RESULT__CI, 0 };
    pInfo->sdmaRegAddr[1][0] = { mmSDMA1_PERFMON_CNTL__CI, 0, mmSDMA1_PERFCOUNTER0_RESULT__CI, 0 };
    pInfo->sdmaRegAddr[1][1] = { mmSDMA1_PERFMON_CNTL__CI, 0, mmSDMA1_PERFCOUNTER1_RESULT__CI, 0 };

    // The MC uses a unique programming model; most registers are handled by the perf experiment but we must set up
    // the ASIC-specific MC_CONFIG info. Each MCD defines four counters for each of its two channels. We abstract
    // each channel as its own MC instance.
    PerfCounterBlockInfo*const pMc = &pInfo->block[static_cast<uint32>(GpuBlock::Mc)];
    pMc->distribution              = PerfCounterDistribution::GlobalBlock;
    pMc->numInstances              = NumMcChannels * pProps->gfx6.numMcdTiles; // 2 channels per MCD
    pMc->numGlobalOnlyCounters     = 4;
    pMc->numGenericSpmModules      = 0;
    pMc->numGenericLegacyModules   = 0;
    pMc->maxEventId                = 21; // Write to Read detected

    // By convention SEQ_CTL is the first select, CNTL_1 is the second select, the "Lo" registers are for channel 0,
    // and the "Hi" registers are for channel 1.
    pMc->regAddr = { 0, {
        { mmMC_SEQ_PERF_SEQ_CTL__SI__CI, mmMC_SEQ_PERF_CNTL_1__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_A_I0__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_A_I1__SI__CI },
        { mmMC_SEQ_PERF_SEQ_CTL__SI__CI, mmMC_SEQ_PERF_CNTL_1__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_B_I0__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_B_I1__SI__CI },
        { mmMC_SEQ_PERF_SEQ_CTL__SI__CI, mmMC_SEQ_PERF_CNTL_1__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_C_I0__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_C_I1__SI__CI },
        { mmMC_SEQ_PERF_SEQ_CTL__SI__CI, mmMC_SEQ_PERF_CNTL_1__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_D_I0__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_D_I1__SI__CI },
    }};

    PerfCounterBlockInfo*const pCpg = &pInfo->block[static_cast<uint32>(GpuBlock::Cpg)];
    pCpg->distribution              = PerfCounterDistribution::GlobalBlock;
    pCpg->numInstances              = 1;
    pCpg->numGenericSpmModules      = 1; // CPG_PERFCOUNTER0
    pCpg->numGenericLegacyModules   = 1; // CPG_PERFCOUNTER1
    pCpg->numSpmWires               = 2;
    pCpg->spmBlockSelect            = Gfx7SpmGlobalBlockSelectCpg;
    pCpg->maxEventId                = CPG_PERF_SEL_TCIU_STALL_WAIT_ON_TAGS;

    pCpg->regAddr = { 0, {
        { mmCPG_PERFCOUNTER0_SELECT__CI__VI, mmCPG_PERFCOUNTER0_SELECT1__CI__VI, mmCPG_PERFCOUNTER0_LO__CI__VI, mmCPG_PERFCOUNTER0_HI__CI__VI },
        { mmCPG_PERFCOUNTER1_SELECT__CI__VI, 0,                                  mmCPG_PERFCOUNTER1_LO__CI__VI, mmCPG_PERFCOUNTER1_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pCpc = &pInfo->block[static_cast<uint32>(GpuBlock::Cpc)];
    pCpc->distribution              = PerfCounterDistribution::GlobalBlock;
    pCpc->numInstances              = 1;
    pCpc->numGenericSpmModules      = 1; // CPC_PERFCOUNTER0
    pCpc->numGenericLegacyModules   = 1; // CPC_PERFCOUNTER1
    pCpc->numSpmWires               = 2;
    pCpc->spmBlockSelect            = Gfx7SpmGlobalBlockSelectCpc;
    pCpc->maxEventId                = CPC_PERF_SEL_ME2_BUSY_FOR_PACKET_DECODE;

    pCpc->regAddr = { 0, {
        { mmCPC_PERFCOUNTER0_SELECT__CI__VI, mmCPC_PERFCOUNTER0_SELECT1__CI__VI, mmCPC_PERFCOUNTER0_LO__CI__VI, mmCPC_PERFCOUNTER0_HI__CI__VI },
        { mmCPC_PERFCOUNTER1_SELECT__CI__VI, 0,                                  mmCPC_PERFCOUNTER1_LO__CI__VI, mmCPC_PERFCOUNTER1_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pWd = &pInfo->block[static_cast<uint32>(GpuBlock::Wd)];
    pWd->distribution              = PerfCounterDistribution::GlobalBlock,
    pWd->numInstances              = 1;
    pWd->numGenericSpmModules      = 0;
    pWd->numGenericLegacyModules   = 4; // WD_PERFCOUNTER0-3
    pWd->maxEventId                = wd_perf_wd_stalled;

    pWd->regAddr = { 0, {
        { mmWD_PERFCOUNTER0_SELECT__CI__VI, 0, mmWD_PERFCOUNTER0_LO__CI__VI, mmWD_PERFCOUNTER0_HI__CI__VI },
        { mmWD_PERFCOUNTER1_SELECT__CI__VI, 0, mmWD_PERFCOUNTER1_LO__CI__VI, mmWD_PERFCOUNTER1_HI__CI__VI },
        { mmWD_PERFCOUNTER2_SELECT__CI__VI, 0, mmWD_PERFCOUNTER2_LO__CI__VI, mmWD_PERFCOUNTER2_HI__CI__VI },
        { mmWD_PERFCOUNTER3_SELECT__CI__VI, 0, mmWD_PERFCOUNTER3_LO__CI__VI, mmWD_PERFCOUNTER3_HI__CI__VI },
    }};

    // Only Kaveri (Spectre & Spooky) chips have the TCS block.
    if (IsSpectre(device) || IsSpooky(device))
    {
        PerfCounterBlockInfo*const pTcs = &pInfo->block[static_cast<uint32>(GpuBlock::Tcs)];
        pTcs->distribution              = PerfCounterDistribution::GlobalBlock;
        pTcs->numInstances              = 1;
        pTcs->numGenericSpmModules      = 1; // TCS_PERFCOUNTER0
        pTcs->numGenericLegacyModules   = 3; // TCS_PERFCOUNTER1-3
        pTcs->numSpmWires               = 2;
        pTcs->spmBlockSelect            = Gfx7SpmGlobalBlockSelectTcs;
        pTcs->maxEventId                = TCS_PERF_SEL_CLIENT63_REQ;

        pTcs->regAddr = { 0, {
            { mmTCS_PERFCOUNTER0_SELECT__CI, mmTCS_PERFCOUNTER0_SELECT1__CI, mmTCS_PERFCOUNTER0_LO__CI, mmTCS_PERFCOUNTER0_HI__CI },
            { mmTCS_PERFCOUNTER1_SELECT__CI, 0,                              mmTCS_PERFCOUNTER1_LO__CI, mmTCS_PERFCOUNTER1_HI__CI },
            { mmTCS_PERFCOUNTER2_SELECT__CI, 0,                              mmTCS_PERFCOUNTER2_LO__CI, mmTCS_PERFCOUNTER2_HI__CI },
            { mmTCS_PERFCOUNTER3_SELECT__CI, 0,                              mmTCS_PERFCOUNTER3_LO__CI, mmTCS_PERFCOUNTER3_HI__CI },
        }};
    }
}

// =====================================================================================================================
// Initializes each block's basic hardware-defined information (distribution, numInstances, numGenericSpmModules, etc.)
static void Gfx8InitBasicBlockInfo(
    const Pal::Device& device,
    GpuChipProperties* pProps)
{
    Gfx6PerfCounterInfo*const pInfo = &pProps->gfx6.perfCounterInfo;

    // Hard-code hardware specific constants for each block. The ATC and VM_L2 seem like they have counters on gfx8 but
    // the old code didn't implement it. We might consider exposing it in the future.
    //
    // The distribution and numInstances (per-distribution) are derived from our hardware architecture.
    // The generic module counts are determined by:
    //   1. Does the block follow the generic programming model as defined by the perf experiment code?
    //   2. If so, there's one SPM module for each SELECT/SELECT1 pair and one legacy module for the remaining SELECTs.
    // The number of SPM wires is a hardware constant baked into each ASIC's design. So are the SPM block selects.
    // The maximum event IDs are the largest values from the hardware perf_sel enums.
    // Finally, we hard-code the PERFCOUNTER# register addresses for each module.

    PerfCounterBlockInfo*const pCpf = &pInfo->block[static_cast<uint32>(GpuBlock::Cpf)];
    pCpf->distribution              = PerfCounterDistribution::GlobalBlock;
    pCpf->numInstances              = 1;
    pCpf->numGenericSpmModules      = 1; // CPF_PERFCOUNTER0
    pCpf->numGenericLegacyModules   = 1; // CPF_PERFCOUNTER1
    pCpf->numSpmWires               = 2;
    pCpf->spmBlockSelect            = Gfx7SpmGlobalBlockSelectCpf;
    pCpf->maxEventId                = CPF_PERF_SEL_ATCL1_STALL_ON_TRANSLATION__VI;

    pCpf->regAddr = { 0, {
        { mmCPF_PERFCOUNTER0_SELECT__CI__VI, mmCPF_PERFCOUNTER0_SELECT1__CI__VI, mmCPF_PERFCOUNTER0_LO__CI__VI, mmCPF_PERFCOUNTER0_HI__CI__VI },
        { mmCPF_PERFCOUNTER1_SELECT__CI__VI, 0,                                  mmCPF_PERFCOUNTER1_LO__CI__VI, mmCPF_PERFCOUNTER1_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pIa = &pInfo->block[static_cast<uint32>(GpuBlock::Ia)];
    pIa->distribution              = PerfCounterDistribution::GlobalBlock;
    pIa->numInstances              = Max(pProps->gfx6.numShaderEngines / 2u, 1u);
    pIa->numGenericSpmModules      = 1; // IA_PERFCOUNTER0
    pIa->numGenericLegacyModules   = 3; // IA_PERFCOUNTER1-3
    pIa->numSpmWires               = 2;
    pIa->spmBlockSelect            = Gfx7SpmGlobalBlockSelectIa;
    pIa->maxEventId                = ia_perf_shift_starved_pipe1_event__VI;

    pIa->regAddr = { 0, {
        { mmIA_PERFCOUNTER0_SELECT__CI__VI, mmIA_PERFCOUNTER0_SELECT1__CI__VI, mmIA_PERFCOUNTER0_LO__CI__VI, mmIA_PERFCOUNTER0_HI__CI__VI },
        { mmIA_PERFCOUNTER1_SELECT__CI__VI, 0,                                 mmIA_PERFCOUNTER1_LO__CI__VI, mmIA_PERFCOUNTER1_HI__CI__VI },
        { mmIA_PERFCOUNTER2_SELECT__CI__VI, 0,                                 mmIA_PERFCOUNTER2_LO__CI__VI, mmIA_PERFCOUNTER2_HI__CI__VI },
        { mmIA_PERFCOUNTER3_SELECT__CI__VI, 0,                                 mmIA_PERFCOUNTER3_LO__CI__VI, mmIA_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pVgt = &pInfo->block[static_cast<uint32>(GpuBlock::Vgt)];
    pVgt->distribution              = PerfCounterDistribution::PerShaderEngine;
    pVgt->numInstances              = 1;
    pVgt->numGenericSpmModules      = 2; // VGT_PERFCOUNTER0-1
    pVgt->numGenericLegacyModules   = 2; // VGT_PERFCOUNTER2-3
    pVgt->numSpmWires               = 3;
    pVgt->spmBlockSelect            = Gfx7SpmSeBlockSelectVgt;
    pVgt->maxEventId                = vgt_spi_vsvert_valid__VI;

    pVgt->regAddr = { 0, {
        { mmVGT_PERFCOUNTER0_SELECT__CI__VI, mmVGT_PERFCOUNTER0_SELECT1__CI__VI, mmVGT_PERFCOUNTER0_LO__CI__VI, mmVGT_PERFCOUNTER0_HI__CI__VI },
        { mmVGT_PERFCOUNTER1_SELECT__CI__VI, mmVGT_PERFCOUNTER1_SELECT1__CI__VI, mmVGT_PERFCOUNTER1_LO__CI__VI, mmVGT_PERFCOUNTER1_HI__CI__VI },
        { mmVGT_PERFCOUNTER2_SELECT__CI__VI, 0,                                  mmVGT_PERFCOUNTER2_LO__CI__VI, mmVGT_PERFCOUNTER2_HI__CI__VI },
        { mmVGT_PERFCOUNTER3_SELECT__CI__VI, 0,                                  mmVGT_PERFCOUNTER3_LO__CI__VI, mmVGT_PERFCOUNTER3_HI__CI__VI },
    }};

    // Note that the PA uses the SU select enum.
    PerfCounterBlockInfo*const pPa = &pInfo->block[static_cast<uint32>(GpuBlock::Pa)];
    pPa->distribution              = PerfCounterDistribution::PerShaderEngine;
    pPa->numInstances              = 1;
    pPa->numGenericSpmModules      = 2; // PA_SU_PERFCOUNTER0-1
    pPa->numGenericLegacyModules   = 2; // PA_SU_PERFCOUNTER2-3
    pPa->numSpmWires               = 3;
    pPa->spmBlockSelect            = Gfx7SpmSeBlockSelectPa;
    pPa->maxEventId                = PERF_PAPC_SU_SE3_STALLED_SC__CI__VI;

    pPa->regAddr = { 0, {
        { mmPA_SU_PERFCOUNTER0_SELECT__CI__VI, mmPA_SU_PERFCOUNTER0_SELECT1__CI__VI, mmPA_SU_PERFCOUNTER0_LO__CI__VI, mmPA_SU_PERFCOUNTER0_HI__CI__VI },
        { mmPA_SU_PERFCOUNTER1_SELECT__CI__VI, mmPA_SU_PERFCOUNTER1_SELECT1__CI__VI, mmPA_SU_PERFCOUNTER1_LO__CI__VI, mmPA_SU_PERFCOUNTER1_HI__CI__VI },
        { mmPA_SU_PERFCOUNTER2_SELECT__CI__VI, 0,                                    mmPA_SU_PERFCOUNTER2_LO__CI__VI, mmPA_SU_PERFCOUNTER2_HI__CI__VI },
        { mmPA_SU_PERFCOUNTER3_SELECT__CI__VI, 0,                                    mmPA_SU_PERFCOUNTER3_LO__CI__VI, mmPA_SU_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pSc = &pInfo->block[static_cast<uint32>(GpuBlock::Sc)];
    pSc->distribution              = PerfCounterDistribution::PerShaderEngine;
    pSc->numInstances              = 1;
    pSc->numGenericSpmModules      = 1; // PA_SC_PERFCOUNTER0
    pSc->numGenericLegacyModules   = 7; // PA_SC_PERFCOUNTER1-7
    pSc->numSpmWires               = 2;
    pSc->spmBlockSelect            = Gfx7SpmSeBlockSelectSc;
    pSc->maxEventId                = SC_STARVED_BY_PA_WITH_UNSELECTED_PA_FULL__VI;

    pSc->regAddr = { 0, {
        { mmPA_SC_PERFCOUNTER0_SELECT__CI__VI, mmPA_SC_PERFCOUNTER0_SELECT1__CI__VI, mmPA_SC_PERFCOUNTER0_LO__CI__VI, mmPA_SC_PERFCOUNTER0_HI__CI__VI },
        { mmPA_SC_PERFCOUNTER1_SELECT__CI__VI, 0,                                    mmPA_SC_PERFCOUNTER1_LO__CI__VI, mmPA_SC_PERFCOUNTER1_HI__CI__VI },
        { mmPA_SC_PERFCOUNTER2_SELECT__CI__VI, 0,                                    mmPA_SC_PERFCOUNTER2_LO__CI__VI, mmPA_SC_PERFCOUNTER2_HI__CI__VI },
        { mmPA_SC_PERFCOUNTER3_SELECT__CI__VI, 0,                                    mmPA_SC_PERFCOUNTER3_LO__CI__VI, mmPA_SC_PERFCOUNTER3_HI__CI__VI },
        { mmPA_SC_PERFCOUNTER4_SELECT__CI__VI, 0,                                    mmPA_SC_PERFCOUNTER4_LO__CI__VI, mmPA_SC_PERFCOUNTER4_HI__CI__VI },
        { mmPA_SC_PERFCOUNTER5_SELECT__CI__VI, 0,                                    mmPA_SC_PERFCOUNTER5_LO__CI__VI, mmPA_SC_PERFCOUNTER5_HI__CI__VI },
        { mmPA_SC_PERFCOUNTER6_SELECT__CI__VI, 0,                                    mmPA_SC_PERFCOUNTER6_LO__CI__VI, mmPA_SC_PERFCOUNTER6_HI__CI__VI },
        { mmPA_SC_PERFCOUNTER7_SELECT__CI__VI, 0,                                    mmPA_SC_PERFCOUNTER7_LO__CI__VI, mmPA_SC_PERFCOUNTER7_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pSpi = &pInfo->block[static_cast<uint32>(GpuBlock::Spi)];
    pSpi->distribution              = PerfCounterDistribution::PerShaderEngine;
    pSpi->numInstances              = 1;
    pSpi->numGenericSpmModules      = 4; // SPI_PERFCOUNTER0-3
    pSpi->numGenericLegacyModules   = 2; // SPI_PERFCOUNTER4-5
    pSpi->numSpmWires               = 8;
    pSpi->spmBlockSelect            = Gfx7SpmSeBlockSelectSpi;
    pSpi->maxEventId                = SPI_PERF_PC_ALLOC_ACCUM__VI;

    pSpi->regAddr = { 0, {
        { mmSPI_PERFCOUNTER0_SELECT__CI__VI, mmSPI_PERFCOUNTER0_SELECT1__CI__VI, mmSPI_PERFCOUNTER0_LO__CI__VI, mmSPI_PERFCOUNTER0_HI__CI__VI },
        { mmSPI_PERFCOUNTER1_SELECT__CI__VI, mmSPI_PERFCOUNTER1_SELECT1__CI__VI, mmSPI_PERFCOUNTER1_LO__CI__VI, mmSPI_PERFCOUNTER1_HI__CI__VI },
        { mmSPI_PERFCOUNTER2_SELECT__CI__VI, mmSPI_PERFCOUNTER2_SELECT1__CI__VI, mmSPI_PERFCOUNTER2_LO__CI__VI, mmSPI_PERFCOUNTER2_HI__CI__VI },
        { mmSPI_PERFCOUNTER3_SELECT__CI__VI, mmSPI_PERFCOUNTER3_SELECT1__CI__VI, mmSPI_PERFCOUNTER3_LO__CI__VI, mmSPI_PERFCOUNTER3_HI__CI__VI },
        { mmSPI_PERFCOUNTER4_SELECT__CI__VI, 0,                                  mmSPI_PERFCOUNTER4_LO__CI__VI, mmSPI_PERFCOUNTER4_HI__CI__VI },
        { mmSPI_PERFCOUNTER5_SELECT__CI__VI, 0,                                  mmSPI_PERFCOUNTER5_LO__CI__VI, mmSPI_PERFCOUNTER5_HI__CI__VI },
    }};

    // The SQ counters are implemented by a single SQG in every shader engine. It has a unique programming model.
    // The SQ counter modules can be a global counter or one 32-bit SPM counter. 16-bit SPM is not supported but we
    // fake one 16-bit counter for now. All gfx8 ASICs only contain 8 out of the possible 16 counter modules.
    PerfCounterBlockInfo*const pSq = &pInfo->block[static_cast<uint32>(GpuBlock::Sq)];
    pSq->distribution              = PerfCounterDistribution::PerShaderEngine;
    pSq->numInstances              = 1;
    pSq->num16BitSpmCounters       = 8;
    pSq->num32BitSpmCounters       = 8;
    pSq->numGlobalSharedCounters   = 8;
    pSq->numGenericSpmModules      = 0;
    pSq->numGenericLegacyModules   = 0;
    pSq->numSpmWires               = 8;
    pSq->spmBlockSelect            = Gfx7SpmSeBlockSelectSqg;
    pSq->maxEventId                = (IsIceland(device) ? SQC_PERF_SEL_DCACHE_TC_INFLIGHT_LEVEL__VI    :
                                      IsTonga(device)   ? SQC_PERF_SEL_DCACHE_GATCL1_HIT_FIFO_FULL__VI :
                                                          SQ_PERF_SEL_ATC_INSTS_SMEM_REPLAY__VI);

    pSq->regAddr = { 0, {
        { mmSQ_PERFCOUNTER0_SELECT__CI__VI, 0, mmSQ_PERFCOUNTER0_LO__CI__VI, mmSQ_PERFCOUNTER0_HI__CI__VI },
        { mmSQ_PERFCOUNTER1_SELECT__CI__VI, 0, mmSQ_PERFCOUNTER1_LO__CI__VI, mmSQ_PERFCOUNTER1_HI__CI__VI },
        { mmSQ_PERFCOUNTER2_SELECT__CI__VI, 0, mmSQ_PERFCOUNTER2_LO__CI__VI, mmSQ_PERFCOUNTER2_HI__CI__VI },
        { mmSQ_PERFCOUNTER3_SELECT__CI__VI, 0, mmSQ_PERFCOUNTER3_LO__CI__VI, mmSQ_PERFCOUNTER3_HI__CI__VI },
        { mmSQ_PERFCOUNTER4_SELECT__CI__VI, 0, mmSQ_PERFCOUNTER4_LO__CI__VI, mmSQ_PERFCOUNTER4_HI__CI__VI },
        { mmSQ_PERFCOUNTER5_SELECT__CI__VI, 0, mmSQ_PERFCOUNTER5_LO__CI__VI, mmSQ_PERFCOUNTER5_HI__CI__VI },
        { mmSQ_PERFCOUNTER6_SELECT__CI__VI, 0, mmSQ_PERFCOUNTER6_LO__CI__VI, mmSQ_PERFCOUNTER6_HI__CI__VI },
        { mmSQ_PERFCOUNTER7_SELECT__CI__VI, 0, mmSQ_PERFCOUNTER7_LO__CI__VI, mmSQ_PERFCOUNTER7_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pSx = &pInfo->block[static_cast<uint32>(GpuBlock::Sx)];
    pSx->distribution              = PerfCounterDistribution::PerShaderArray;
    pSx->numInstances              = 1;
    pSx->numGenericSpmModules      = 2; // SX_PERFCOUNTER0-1
    pSx->numGenericLegacyModules   = 2; // SX_PERFCOUNTER2-3
    pSx->numSpmWires               = 4;
    pSx->spmBlockSelect            = Gfx7SpmSeBlockSelectSx;
    pSx->maxEventId                = 33; // SX_PERF_SEL_POS_BUSY

    pSx->regAddr = { 0, {
        { mmSX_PERFCOUNTER0_SELECT__CI__VI, mmSX_PERFCOUNTER0_SELECT1__CI__VI, mmSX_PERFCOUNTER0_LO__CI__VI, mmSX_PERFCOUNTER0_HI__CI__VI },
        { mmSX_PERFCOUNTER1_SELECT__CI__VI, mmSX_PERFCOUNTER1_SELECT1__CI__VI, mmSX_PERFCOUNTER1_LO__CI__VI, mmSX_PERFCOUNTER1_HI__CI__VI },
        { mmSX_PERFCOUNTER2_SELECT__CI__VI, 0,                                 mmSX_PERFCOUNTER2_LO__CI__VI, mmSX_PERFCOUNTER2_HI__CI__VI },
        { mmSX_PERFCOUNTER3_SELECT__CI__VI, 0,                                 mmSX_PERFCOUNTER3_LO__CI__VI, mmSX_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pTa = &pInfo->block[static_cast<uint32>(GpuBlock::Ta)];
    pTa->distribution              = PerfCounterDistribution::PerShaderArray;
    pTa->numInstances              = pProps->gfx6.numCuPerSh;
    pTa->numGenericSpmModules      = 1; // TA_PERFCOUNTER0
    pTa->numGenericLegacyModules   = 1; // TA_PERFCOUNTER1
    pTa->numSpmWires               = 2;
    pTa->spmBlockSelect            = Gfx7SpmSeBlockSelectTa;
    pTa->maxEventId                = TA_PERF_SEL_first_xnack_on_phase3__VI;

    pTa->regAddr = { 0, {
        { mmTA_PERFCOUNTER0_SELECT__CI__VI, mmTA_PERFCOUNTER0_SELECT1__CI__VI, mmTA_PERFCOUNTER0_LO__CI__VI, mmTA_PERFCOUNTER0_HI__CI__VI },
        { mmTA_PERFCOUNTER1_SELECT__CI__VI, 0,                                 mmTA_PERFCOUNTER1_LO__CI__VI, mmTA_PERFCOUNTER1_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pTd = &pInfo->block[static_cast<uint32>(GpuBlock::Td)];
    pTd->distribution              = PerfCounterDistribution::PerShaderArray;
    pTd->numInstances              = pProps->gfx6.numCuPerSh;
    pTd->numGenericSpmModules      = 1; // TD_PERFCOUNTER0
    pTd->numGenericLegacyModules   = 1; // TD_PERFCOUNTER1
    pTd->numSpmWires               = 2;
    pTd->spmBlockSelect            = Gfx7SpmSeBlockSelectTd;
    pTd->maxEventId                = TD_PERF_SEL_null_cycle_output__CI__VI;

    pTd->regAddr = { 0, {
        { mmTD_PERFCOUNTER0_SELECT__CI__VI, mmTD_PERFCOUNTER0_SELECT1__CI__VI, mmTD_PERFCOUNTER0_LO__CI__VI, mmTD_PERFCOUNTER0_HI__CI__VI },
        { mmTD_PERFCOUNTER1_SELECT__CI__VI, 0,                                 mmTD_PERFCOUNTER1_LO__CI__VI, mmTD_PERFCOUNTER1_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pTcp = &pInfo->block[static_cast<uint32>(GpuBlock::Tcp)];
    pTcp->distribution              = PerfCounterDistribution::PerShaderArray;
    pTcp->numInstances              = pProps->gfx6.numCuPerSh;
    pTcp->numGenericSpmModules      = 2; // TCP_PERFCOUNTER0-1
    pTcp->numGenericLegacyModules   = 2; // TCP_PERFCOUNTER2-3
    pTcp->numSpmWires               = 3;
    pTcp->spmBlockSelect            = Gfx7SpmSeBlockSelectTcp;
    pTcp->maxEventId                = TCP_PERF_SEL_POWER_STALL__CI__VI;

    pTcp->regAddr = { 0, {
        { mmTCP_PERFCOUNTER0_SELECT__CI__VI, mmTCP_PERFCOUNTER0_SELECT1__CI__VI, mmTCP_PERFCOUNTER0_LO__CI__VI, mmTCP_PERFCOUNTER0_HI__CI__VI },
        { mmTCP_PERFCOUNTER1_SELECT__CI__VI, mmTCP_PERFCOUNTER1_SELECT1__CI__VI, mmTCP_PERFCOUNTER1_LO__CI__VI, mmTCP_PERFCOUNTER1_HI__CI__VI },
        { mmTCP_PERFCOUNTER2_SELECT__CI__VI, 0,                                  mmTCP_PERFCOUNTER2_LO__CI__VI, mmTCP_PERFCOUNTER2_HI__CI__VI },
        { mmTCP_PERFCOUNTER3_SELECT__CI__VI, 0,                                  mmTCP_PERFCOUNTER3_LO__CI__VI, mmTCP_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pTcc = &pInfo->block[static_cast<uint32>(GpuBlock::Tcc)];
    pTcc->distribution              = PerfCounterDistribution::GlobalBlock,
    pTcc->numInstances              = pProps->gfx6.numTccBlocks;
    pTcc->numGenericSpmModules      = 2; // TCC_PERFCOUNTER0-1
    pTcc->numGenericLegacyModules   = 2; // TCC_PERFCOUNTER2-3
    pTcc->numSpmWires               = 4;
    pTcc->spmBlockSelect            = Gfx7SpmGlobalBlockSelectTcc;
    pTcc->maxEventId                = TCC_PERF_SEL_CLIENT127_REQ__VI;

    pTcc->regAddr = { 0, {
        { mmTCC_PERFCOUNTER0_SELECT__CI__VI, mmTCC_PERFCOUNTER0_SELECT1__CI__VI, mmTCC_PERFCOUNTER0_LO__CI__VI, mmTCC_PERFCOUNTER0_HI__CI__VI },
        { mmTCC_PERFCOUNTER1_SELECT__CI__VI, mmTCC_PERFCOUNTER1_SELECT1__CI__VI, mmTCC_PERFCOUNTER1_LO__CI__VI, mmTCC_PERFCOUNTER1_HI__CI__VI },
        { mmTCC_PERFCOUNTER2_SELECT__CI__VI, 0,                                  mmTCC_PERFCOUNTER2_LO__CI__VI, mmTCC_PERFCOUNTER2_HI__CI__VI },
        { mmTCC_PERFCOUNTER3_SELECT__CI__VI, 0,                                  mmTCC_PERFCOUNTER3_LO__CI__VI, mmTCC_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pTca = &pInfo->block[static_cast<uint32>(GpuBlock::Tca)];
    pTca->distribution              = PerfCounterDistribution::GlobalBlock,
    pTca->numInstances              = 2;
    pTca->numGenericSpmModules      = 2; // TCA_PERFCOUNTER0-1
    pTca->numGenericLegacyModules   = 2; // TCA_PERFCOUNTER2-3
    pTca->numSpmWires               = 4;
    pTca->spmBlockSelect            = Gfx7SpmGlobalBlockSelectTca;
    pTca->maxEventId                = TCA_PERF_SEL_CROSSBAR_STALL_TCC7;

    pTca->regAddr = { 0, {
        { mmTCA_PERFCOUNTER0_SELECT__CI__VI, mmTCA_PERFCOUNTER0_SELECT1__CI__VI, mmTCA_PERFCOUNTER0_LO__CI__VI, mmTCA_PERFCOUNTER0_HI__CI__VI },
        { mmTCA_PERFCOUNTER1_SELECT__CI__VI, mmTCA_PERFCOUNTER1_SELECT1__CI__VI, mmTCA_PERFCOUNTER1_LO__CI__VI, mmTCA_PERFCOUNTER1_HI__CI__VI },
        { mmTCA_PERFCOUNTER2_SELECT__CI__VI, 0,                                  mmTCA_PERFCOUNTER2_LO__CI__VI, mmTCA_PERFCOUNTER2_HI__CI__VI },
        { mmTCA_PERFCOUNTER3_SELECT__CI__VI, 0,                                  mmTCA_PERFCOUNTER3_LO__CI__VI, mmTCA_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pDb = &pInfo->block[static_cast<uint32>(GpuBlock::Db)];
    pDb->distribution              = PerfCounterDistribution::PerShaderArray;
    pDb->numInstances              = pProps->gfx6.maxNumRbPerSe / pProps->gfx6.numShaderArrays;
    pDb->numGenericSpmModules      = 2; // DB_PERFCOUNTER0-1
    pDb->numGenericLegacyModules   = 2; // DB_PERFCOUNTER2-3
    pDb->numSpmWires               = 3;
    pDb->spmBlockSelect            = Gfx7SpmSeBlockSelectDb;
    pDb->maxEventId                = DB_PERF_SEL_di_dt_stall__CI__VI;

    pDb->regAddr = { 0, {
        { mmDB_PERFCOUNTER0_SELECT__CI__VI, mmDB_PERFCOUNTER0_SELECT1__CI__VI, mmDB_PERFCOUNTER0_LO__CI__VI, mmDB_PERFCOUNTER0_HI__CI__VI },
        { mmDB_PERFCOUNTER1_SELECT__CI__VI, mmDB_PERFCOUNTER1_SELECT1__CI__VI, mmDB_PERFCOUNTER1_LO__CI__VI, mmDB_PERFCOUNTER1_HI__CI__VI },
        { mmDB_PERFCOUNTER2_SELECT__CI__VI, 0,                                 mmDB_PERFCOUNTER2_LO__CI__VI, mmDB_PERFCOUNTER2_HI__CI__VI },
        { mmDB_PERFCOUNTER3_SELECT__CI__VI, 0,                                 mmDB_PERFCOUNTER3_LO__CI__VI, mmDB_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pCb = &pInfo->block[static_cast<uint32>(GpuBlock::Cb)];
    pCb->distribution              = PerfCounterDistribution::PerShaderArray;
    pCb->numInstances              = pProps->gfx6.maxNumRbPerSe / pProps->gfx6.numShaderArrays;
    pCb->numGenericSpmModules      = 1; // CB_PERFCOUNTER0
    pCb->numGenericLegacyModules   = 3; // CB_PERFCOUNTER1-3
    pCb->numSpmWires               = 2;
    pCb->spmBlockSelect            = Gfx7SpmSeBlockSelectCb;
    pCb->maxEventId                = CB_PERF_SEL_CC_DCC_COMPRESS_RATIO_8TO7__VI;

    pCb->regAddr = { 0, {
        { mmCB_PERFCOUNTER0_SELECT__CI__VI, mmCB_PERFCOUNTER0_SELECT1__CI__VI, mmCB_PERFCOUNTER0_LO__CI__VI, mmCB_PERFCOUNTER0_HI__CI__VI },
        { mmCB_PERFCOUNTER1_SELECT__CI__VI, 0,                                 mmCB_PERFCOUNTER1_LO__CI__VI, mmCB_PERFCOUNTER1_HI__CI__VI },
        { mmCB_PERFCOUNTER2_SELECT__CI__VI, 0,                                 mmCB_PERFCOUNTER2_LO__CI__VI, mmCB_PERFCOUNTER2_HI__CI__VI },
        { mmCB_PERFCOUNTER3_SELECT__CI__VI, 0,                                 mmCB_PERFCOUNTER3_LO__CI__VI, mmCB_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pGds = &pInfo->block[static_cast<uint32>(GpuBlock::Gds)];
    pGds->distribution              = PerfCounterDistribution::GlobalBlock;
    pGds->numInstances              = 1;
    pGds->numGenericSpmModules      = 1; // GDS_PERFCOUNTER0
    pGds->numGenericLegacyModules   = 3; // GDS_PERFCOUNTER1-3
    pGds->numSpmWires               = 2;
    pGds->spmBlockSelect            = Gfx7SpmGlobalBlockSelectGds;
    pGds->maxEventId                = 120; // GDS_PERF_SEL_GWS_BYPASS

    pGds->regAddr = { 0, {
        { mmGDS_PERFCOUNTER0_SELECT__CI__VI, mmGDS_PERFCOUNTER0_SELECT1__CI__VI, mmGDS_PERFCOUNTER0_LO__CI__VI, mmGDS_PERFCOUNTER0_HI__CI__VI },
        { mmGDS_PERFCOUNTER1_SELECT__CI__VI, 0,                                  mmGDS_PERFCOUNTER1_LO__CI__VI, mmGDS_PERFCOUNTER1_HI__CI__VI },
        { mmGDS_PERFCOUNTER2_SELECT__CI__VI, 0,                                  mmGDS_PERFCOUNTER2_LO__CI__VI, mmGDS_PERFCOUNTER2_HI__CI__VI },
        { mmGDS_PERFCOUNTER3_SELECT__CI__VI, 0,                                  mmGDS_PERFCOUNTER3_LO__CI__VI, mmGDS_PERFCOUNTER3_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pSrbm = &pInfo->block[static_cast<uint32>(GpuBlock::Srbm)];
    pSrbm->distribution              = PerfCounterDistribution::GlobalBlock;
    pSrbm->numInstances              = 1;
    pSrbm->numGenericSpmModules      = 0;
    pSrbm->numGenericLegacyModules   = 2; // SRBM_PERFCOUNTER0-1
    pSrbm->maxEventId                = SRBM_PERF_SEL_VP8_BUSY__VI;

    pSrbm->regAddr = { 0, {
        { mmSRBM_PERFCOUNTER0_SELECT__VI, 0, mmSRBM_PERFCOUNTER0_LO__VI, mmSRBM_PERFCOUNTER0_HI__VI },
        { mmSRBM_PERFCOUNTER1_SELECT__VI, 0, mmSRBM_PERFCOUNTER1_LO__VI, mmSRBM_PERFCOUNTER1_HI__VI },
    }};

    PerfCounterBlockInfo*const pGrbm = &pInfo->block[static_cast<uint32>(GpuBlock::Grbm)];
    pGrbm->distribution              = PerfCounterDistribution::GlobalBlock;
    pGrbm->numInstances              = 1;
    pGrbm->numGenericSpmModules      = 0;
    pGrbm->numGenericLegacyModules   = 2; // GRBM_PERFCOUNTER0-1
    pGrbm->maxEventId                = GRBM_PERF_SEL_WD_NO_DMA_BUSY__CI__VI;

    pGrbm->regAddr = { 0, {
        { mmGRBM_PERFCOUNTER0_SELECT__CI__VI, 0, mmGRBM_PERFCOUNTER0_LO__CI__VI, mmGRBM_PERFCOUNTER0_HI__CI__VI },
        { mmGRBM_PERFCOUNTER1_SELECT__CI__VI, 0, mmGRBM_PERFCOUNTER1_LO__CI__VI, mmGRBM_PERFCOUNTER1_HI__CI__VI },
    }};

    // These counters are a bit special. The GRBM is a global block but it defines one special counter per SE. We
    // abstract this as a special Grbm(per)Se block which needs special handling in the perf experiment.
    PerfCounterBlockInfo*const pGrbmSe = &pInfo->block[static_cast<uint32>(GpuBlock::GrbmSe)];
    pGrbmSe->distribution              = PerfCounterDistribution::PerShaderEngine;
    pGrbmSe->numInstances              = 1;
    pGrbmSe->numGlobalOnlyCounters     = 1;
    pGrbmSe->numGenericSpmModules      = 0;
    pGrbmSe->numGenericLegacyModules   = 0;
    pGrbmSe->maxEventId                = GRBM_SE0_PERF_SEL_BCI_BUSY;

    // By convention we access the counter register address array using the SE index.
    pGrbmSe->regAddr = { 0, {
        { mmGRBM_SE0_PERFCOUNTER_SELECT__CI__VI, 0, mmGRBM_SE0_PERFCOUNTER_LO__CI__VI, mmGRBM_SE0_PERFCOUNTER_HI__CI__VI },
        { mmGRBM_SE1_PERFCOUNTER_SELECT__CI__VI, 0, mmGRBM_SE1_PERFCOUNTER_LO__CI__VI, mmGRBM_SE1_PERFCOUNTER_HI__CI__VI },
        { mmGRBM_SE2_PERFCOUNTER_SELECT__CI__VI, 0, mmGRBM_SE2_PERFCOUNTER_LO__CI__VI, mmGRBM_SE2_PERFCOUNTER_HI__CI__VI },
        { mmGRBM_SE3_PERFCOUNTER_SELECT__CI__VI, 0, mmGRBM_SE3_PERFCOUNTER_LO__CI__VI, mmGRBM_SE3_PERFCOUNTER_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pRlc = &pInfo->block[static_cast<uint32>(GpuBlock::Rlc)];
    pRlc->distribution              = PerfCounterDistribution::GlobalBlock;
    pRlc->numInstances              = 1;
    pRlc->numGenericSpmModules      = 0;
    pRlc->numGenericLegacyModules   = 2; // RLC_PERFCOUNTER0-1
    pRlc->maxEventId                = 6; // SERDES command write

    pRlc->regAddr = { 0, {
        { mmRLC_PERFCOUNTER0_SELECT__CI__VI, 0, mmRLC_PERFCOUNTER0_LO__CI__VI, mmRLC_PERFCOUNTER0_HI__CI__VI },
        { mmRLC_PERFCOUNTER1_SELECT__CI__VI, 0, mmRLC_PERFCOUNTER1_LO__CI__VI, mmRLC_PERFCOUNTER1_HI__CI__VI },
    }};

    // The SDMA block has a unique programming model with 2 32-bit counters and unique registers for each instance.
    PerfCounterBlockInfo*const pDma = &pInfo->block[static_cast<uint32>(GpuBlock::Dma)];
    pDma->distribution              = PerfCounterDistribution::GlobalBlock;
    pDma->numInstances              = 2;
    pDma->numGlobalOnlyCounters     = 2;
    pDma->numGenericSpmModules      = 0;
    pDma->numGenericLegacyModules   = 0;
    pDma->maxEventId                = SDMA_PERF_SEL_WR_BA_RTR__VI;

    pInfo->sdmaRegAddr[0][0] = { mmSDMA0_PERFMON_CNTL__VI, 0, mmSDMA0_PERFCOUNTER0_RESULT__VI, 0 };
    pInfo->sdmaRegAddr[0][1] = { mmSDMA0_PERFMON_CNTL__VI, 0, mmSDMA0_PERFCOUNTER1_RESULT__VI, 0 };
    pInfo->sdmaRegAddr[1][0] = { mmSDMA1_PERFMON_CNTL__VI, 0, mmSDMA1_PERFCOUNTER0_RESULT__VI, 0 };
    pInfo->sdmaRegAddr[1][1] = { mmSDMA1_PERFMON_CNTL__VI, 0, mmSDMA1_PERFCOUNTER1_RESULT__VI, 0 };

    // The MC uses a unique programming model; most registers are handled by the perf experiment but we must set up
    // the ASIC-specific MC_CONFIG info. Each MCD defines four counters for each of its two channels. We abstract
    // each channel as its own MC instance.
    PerfCounterBlockInfo*const pMc = &pInfo->block[static_cast<uint32>(GpuBlock::Mc)];
    pMc->distribution              = PerfCounterDistribution::GlobalBlock;
    pMc->numInstances              = NumMcChannels * pProps->gfx6.numMcdTiles; // 2 channels per MCD
    pMc->numGlobalOnlyCounters     = 4;
    pMc->numGenericSpmModules      = 0;
    pMc->numGenericLegacyModules   = 0;
    pMc->maxEventId                = 21; // Write to Read detected

    // By convention SEQ_CTL is the first select, CNTL_1 is the second select, the "Lo" registers are for channel 0,
    // and the "Hi" registers are for channel 1.
    //
    // These registers do exist on *some* Gfx8 variations. The Gfx8 headers used to create the merged headers don't
    // include them though so they got the __SI__CI tag.
    pMc->regAddr = { 0, {
        { mmMC_SEQ_PERF_SEQ_CTL__SI__CI, mmMC_SEQ_PERF_CNTL_1__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_A_I0__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_A_I1__SI__CI },
        { mmMC_SEQ_PERF_SEQ_CTL__SI__CI, mmMC_SEQ_PERF_CNTL_1__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_B_I0__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_B_I1__SI__CI },
        { mmMC_SEQ_PERF_SEQ_CTL__SI__CI, mmMC_SEQ_PERF_CNTL_1__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_C_I0__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_C_I1__SI__CI },
        { mmMC_SEQ_PERF_SEQ_CTL__SI__CI, mmMC_SEQ_PERF_CNTL_1__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_D_I0__SI__CI, mmMC_SEQ_PERF_SEQ_CNT_D_I1__SI__CI },
    }};

    PerfCounterBlockInfo*const pCpg = &pInfo->block[static_cast<uint32>(GpuBlock::Cpg)];
    pCpg->distribution              = PerfCounterDistribution::GlobalBlock;
    pCpg->numInstances              = 1;
    pCpg->numGenericSpmModules      = 1; // CPG_PERFCOUNTER0
    pCpg->numGenericLegacyModules   = 1; // CPG_PERFCOUNTER1
    pCpg->numSpmWires               = 2;
    pCpg->spmBlockSelect            = Gfx7SpmGlobalBlockSelectCpg;
    pCpg->maxEventId                = CPG_PERF_SEL_ATCL1_STALL_ON_TRANSLATION__VI;

    pCpg->regAddr = { 0, {
        { mmCPG_PERFCOUNTER0_SELECT__CI__VI, mmCPG_PERFCOUNTER0_SELECT1__CI__VI, mmCPG_PERFCOUNTER0_LO__CI__VI, mmCPG_PERFCOUNTER0_HI__CI__VI },
        { mmCPG_PERFCOUNTER1_SELECT__CI__VI, 0,                                  mmCPG_PERFCOUNTER1_LO__CI__VI, mmCPG_PERFCOUNTER1_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pCpc = &pInfo->block[static_cast<uint32>(GpuBlock::Cpc)];
    pCpc->distribution              = PerfCounterDistribution::GlobalBlock;
    pCpc->numInstances              = 1;
    pCpc->numGenericSpmModules      = 1; // CPC_PERFCOUNTER0
    pCpc->numGenericLegacyModules   = 1; // CPC_PERFCOUNTER1
    pCpc->numSpmWires               = 2;
    pCpc->spmBlockSelect            = Gfx7SpmGlobalBlockSelectCpc;
    pCpc->maxEventId                = CPC_PERF_SEL_ATCL1_STALL_ON_TRANSLATION__VI;

    pCpc->regAddr = { 0, {
        { mmCPC_PERFCOUNTER0_SELECT__CI__VI, mmCPC_PERFCOUNTER0_SELECT1__CI__VI, mmCPC_PERFCOUNTER0_LO__CI__VI, mmCPC_PERFCOUNTER0_HI__CI__VI },
        { mmCPC_PERFCOUNTER1_SELECT__CI__VI, 0,                                  mmCPC_PERFCOUNTER1_LO__CI__VI, mmCPC_PERFCOUNTER1_HI__CI__VI },
    }};

    PerfCounterBlockInfo*const pWd = &pInfo->block[static_cast<uint32>(GpuBlock::Wd)];
    pWd->distribution              = PerfCounterDistribution::GlobalBlock,
    pWd->numInstances              = 1;
    pWd->numGenericSpmModules      = 0;
    pWd->numGenericLegacyModules   = 4; // WD_PERFCOUNTER0-3
    pWd->maxEventId                = wd_perf_null_patches__VI;

    pWd->regAddr = { 0, {
        { mmWD_PERFCOUNTER0_SELECT__CI__VI, 0, mmWD_PERFCOUNTER0_LO__CI__VI, mmWD_PERFCOUNTER0_HI__CI__VI },
        { mmWD_PERFCOUNTER1_SELECT__CI__VI, 0, mmWD_PERFCOUNTER1_LO__CI__VI, mmWD_PERFCOUNTER1_HI__CI__VI },
        { mmWD_PERFCOUNTER2_SELECT__CI__VI, 0, mmWD_PERFCOUNTER2_LO__CI__VI, mmWD_PERFCOUNTER2_HI__CI__VI },
        { mmWD_PERFCOUNTER3_SELECT__CI__VI, 0, mmWD_PERFCOUNTER3_LO__CI__VI, mmWD_PERFCOUNTER3_HI__CI__VI },
    }};
}

// =====================================================================================================================
// Initializes the performance counter information for an adapter structure, specifically for the Gfx6 hardware layer.
void InitPerfCtrInfo(
    const Pal::Device& device,
    GpuChipProperties* pProps)
{
    // Something pretty terrible will probably happen if this isn't true.
    PAL_ASSERT(pProps->gfx6.numShaderEngines <= Gfx6MaxShaderEngines);
    PAL_ASSERT(pProps->gfx6.numMcdTiles <= MaxMcdTiles);

    Gfx6PerfCounterInfo*const pInfo = &pProps->gfx6.perfCounterInfo;

    // The caller should already have zeroed this struct a long time ago but let's do it again just to be sure.
    // We depend very heavily on unsupported fields being zero by default.
    memset(pInfo, 0, sizeof(*pInfo));

    // The SPM block select requires a non-zero default. We use UINT32_MAX to indicate "invalid".
    for (uint32 idx = 0; idx < static_cast<uint32>(GpuBlock::Count); idx++)
    {
        pInfo->block[idx].spmBlockSelect = UINT32_MAX;
    }

    pInfo->features.counters    = 1;
    pInfo->features.threadTrace = 1;
    pInfo->features.spmTrace    = (pProps->gfxLevel >= GfxIpLevel::GfxIp7);

    // Only Fiji is known to support PS1 event tokens in thread traces.
    pInfo->features.supportPs1Events = IsFiji(device);

    // All current GFX6 hardware is affected by "SPI not differentiating pkr_id for newwave commands".
    pInfo->features.sqttBadScPackerId = 1;

    // Set the hardware specified per-block information (see the function for what exactly that means).
    // There's so much code to do this that it had to go in a helper function for each version.
    if (pProps->gfxLevel == GfxIpLevel::GfxIp6)
    {
        Gfx6InitBasicBlockInfo(device, pProps);
    }
    else if (pProps->gfxLevel == GfxIpLevel::GfxIp7)
    {
        Gfx7InitBasicBlockInfo(device, pProps);
    }
    else
    {
        Gfx8InitBasicBlockInfo(device, pProps);
    }

    // Setup the mcConfig struct.
    InitMcConfigInfo(device, pInfo);

    // Using that information, infer the remaining per-block properties.
    for (uint32 idx = 0; idx < static_cast<uint32>(GpuBlock::Count); idx++)
    {
        PerfCounterBlockInfo*const pBlock = &pInfo->block[idx];

        if (pBlock->distribution != PerfCounterDistribution::Unavailable)
        {
            // Compute the total instance count.
            if (pBlock->distribution == PerfCounterDistribution::PerShaderArray)
            {
                pBlock->numGlobalInstances =
                    pBlock->numInstances * pProps->gfx6.numShaderEngines * pProps->gfx6.numShaderArrays;
            }
            else if (pBlock->distribution == PerfCounterDistribution::PerShaderEngine)
            {
                pBlock->numGlobalInstances = pBlock->numInstances * pProps->gfx6.numShaderEngines;
            }
            else
            {
                pBlock->numGlobalInstances = pBlock->numInstances;
            }

            // If this triggers we need to increase MaxPerfModules.
            const uint32 totalGenericModules = pBlock->numGenericSpmModules + pBlock->numGenericLegacyModules;
            PAL_ASSERT(totalGenericModules <= MaxPerfModules);

            // These are a fairly simple translation for the generic blocks. The blocks that require special treatment
            // must set the generic module counts to zero and manually set their numbers of counters.
            if (totalGenericModules > 0)
            {
                PAL_ASSERT((pBlock->num16BitSpmCounters == 0) && (pBlock->num32BitSpmCounters == 0) &&
                           (pBlock->numGlobalOnlyCounters == 0) && (pBlock->numGlobalSharedCounters == 0));

                pBlock->num16BitSpmCounters     = pBlock->numGenericSpmModules * 4;
                pBlock->num32BitSpmCounters     = pBlock->numGenericSpmModules * 2;
                pBlock->numGlobalOnlyCounters   = pBlock->numGenericLegacyModules;
                pBlock->numGlobalSharedCounters = pBlock->numGenericSpmModules;
            }

            // If some block has SPM counters it must have SPM wires and a SPM block select.
            PAL_ASSERT(((pBlock->num16BitSpmCounters == 0) && (pBlock->num32BitSpmCounters == 0)) ||
                       ((pBlock->numSpmWires > 0) && (pBlock->spmBlockSelect != UINT32_MAX)));
        }
    }

    // Verify that we didn't exceed any of our hard coded per-block constants.
    PAL_ASSERT(pInfo->block[static_cast<uint32>(GpuBlock::Dma)].numGlobalInstances   <= Gfx7MaxSdmaInstances);
    PAL_ASSERT(pInfo->block[static_cast<uint32>(GpuBlock::Dma)].numGenericSpmModules <= Gfx7MaxSdmaPerfModules);
}

} // Gfx6
} // Pal
