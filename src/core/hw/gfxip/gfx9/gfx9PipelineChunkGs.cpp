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

#include "core/platform.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkGs.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// Base count of SH registers which are loaded using LOAD_SH_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedShRegCount =
    1 + // mmSPI_SHADER_PGM_LO_ES
    1 + // mmSPI_SHADER_PGM_HI_ES
    1 + // SPI_SHADER_PGM_RSRC1_GS
    1 + // SPI_SHADER_PGM_RSRC2_GS
    0 + // SPI_SHADER_PGM_CHKSUM_GS is not included because it is not present on all HW
    1;  // SPI_SHADER_USER_DATA_ES_0 + ConstBufTblStartReg

// Base count of Context registers which are loaded using LOAD_CNTX_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedCntxRegCount =
    1 + // mmVGT_GS_MAX_VERT_OUT
    1 + // mmVGT_GS_INSTANCE_CNT
    4 + // mmVGT_GS_VERT_ITEMSIZE_*
    3 + // mmVGT_GSVS_RING_OFFSET_*
    1 + // mmVGT_GS_OUT_PRIM_TYPE
    1 + // mmVGT_GS_PER_VS
    1 + // mmVGT_GSVS_RING_ITEMSIZE
    1 + // mmVGT_ESGS_RING_ITEMSIZE
    1 + // mmVGT_GS_MAX_PRIMS_PER_SUBGROUP or mmGE_MAX_OUTPUT_PER_SUBGROUP, depending on GfxIp version
    0 + // mmSPI_SHADER_IDX_FORMAT is not included because it is not present on all HW
    0;  // mmGE_NGG_SUBGRP_CNTL is not included because it is not present on all HW

// =====================================================================================================================
PipelineChunkGs::PipelineChunkGs(
    const Device&       device,
    const PerfDataInfo* pPerfDataInfo)
    :
    m_device(device),
    m_pPerfDataInfo(pPerfDataInfo)
{
    memset(&m_regs,  0, sizeof(m_regs));
    memset(&m_stageInfo, 0, sizeof(m_stageInfo));
    m_stageInfo.stageId = Abi::HardwareStage::Gs;
}

// =====================================================================================================================
// Early initialization for this pipeline chunk.  Responsible for determining the number of SH and context registers to
// be loaded using LOAD_CNTX_REG_INDEX and LOAD_SH_REG_INDEX.
void PipelineChunkGs::EarlyInit(
    GraphicsPipelineLoadInfo* pInfo)
{
    PAL_ASSERT(pInfo != nullptr);

    const Gfx9PalSettings&   settings  = m_device.Settings();
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    m_regs.sh.ldsEsGsSizeRegAddrGs = pInfo->esGsLdsSizeRegGs;
    m_regs.sh.ldsEsGsSizeRegAddrVs = pInfo->esGsLdsSizeRegVs;

    if (settings.enableLoadIndexForObjectBinds != false)
    {
        pInfo->loadedCtxRegCount += BaseLoadedCntxRegCount;
        pInfo->loadedShRegCount  += (BaseLoadedShRegCount + ((chipProps.gfx9.supportSpp == 1) ? 1 : 0));

        // Handle GFX10 specific context registers
        if (IsGfx10(chipProps.gfxLevel))
        {
            // mmSPI_SHADER_IDX_FORMAT
            // mmGE_NGG_SUBGRP_CNTL
            pInfo->loadedCtxRegCount += 2;
        }

        // Up to two additional SH registers will be loaded for on-chip GS or NGG pipelines for the ES/GS LDS sizes.
        if (pInfo->enableNgg)
        {
            PAL_ASSERT(pInfo->esGsLdsSizeRegGs != UserDataNotMapped);
            pInfo->loadedShRegCount += 1;
        }
        else if (pInfo->usesOnChipGs)
        {
            PAL_ASSERT((pInfo->esGsLdsSizeRegGs != UserDataNotMapped) &&
                       (pInfo->esGsLdsSizeRegVs != UserDataNotMapped));
            pInfo->loadedShRegCount += 2;
        }
    }
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.  Also uploads register state into GPU memory.
void PipelineChunkGs::LateInit(
    const AbiReader&                abiReader,
    const CodeObjectMetadata&       metadata,
    const RegisterVector&           registers,
    const GraphicsPipelineLoadInfo& loadInfo,
    GraphicsPipelineUploader*       pUploader,
    MetroHash64*                    pHasher)
{
    const Gfx9PalSettings&   settings     = m_device.Settings();
    const GpuChipProperties& chipProps    = m_device.Parent()->ChipProperties();
    const RegisterInfo&      registerInfo = m_device.CmdUtil().GetRegInfo();

    const uint16 mmSpiShaderUserDataGs0 = registerInfo.mmUserDataStartGsShaderStage;
    const uint16 mmSpiShaderPgmLoEs     = registerInfo.mmSpiShaderPgmLoEs;

    GpuSymbol symbol = { };
    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::GsMainEntry, &symbol) == Result::Success)
    {
        m_stageInfo.codeLength     = static_cast<size_t>(symbol.size);
        PAL_ASSERT(IsPow2Aligned(symbol.gpuVirtAddr, 256));

        m_regs.sh.spiShaderPgmLoEs.bits.MEM_BASE = Get256BAddrLo(symbol.gpuVirtAddr);
        m_regs.sh.spiShaderPgmHiEs.bits.MEM_BASE = Get256BAddrHi(symbol.gpuVirtAddr);
    }

    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::GsShdrIntrlTblPtr, &symbol) == Result::Success)
    {
        m_regs.sh.userDataInternalTable.u32All = LowPart(symbol.gpuVirtAddr);
    }

    const Elf::SymbolTableEntry* pElfSymbol = abiReader.GetPipelineSymbol(Abi::PipelineSymbolType::GsDisassembly);
    if (pElfSymbol != nullptr)
    {
        m_stageInfo.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
    }

    m_regs.sh.spiShaderPgmRsrc1Gs.u32All      = registers.At(mmSPI_SHADER_PGM_RSRC1_GS);
    m_regs.sh.spiShaderPgmRsrc2Gs.u32All      = registers.At(mmSPI_SHADER_PGM_RSRC2_GS);
    m_regs.dynamic.spiShaderPgmRsrc4Gs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC4_GS);

    registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_GS, &m_regs.dynamic.spiShaderPgmRsrc3Gs.u32All);

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_ENABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    m_regs.sh.spiShaderPgmRsrc1Gs.bits.CU_GROUP_ENABLE = (settings.gsCuGroupEnabled ? 1 : 0);

#if PAL_ENABLE_PRINTS_ASSERTS
    m_device.AssertUserAccumRegsDisabled(registers, Gfx10Plus::mmSPI_SHADER_USER_ACCUM_ESGS_0);
#endif

    uint32 lateAllocWaves  = (loadInfo.enableNgg) ? settings.nggLateAllocGs : settings.lateAllocGs;
    uint32 lateAllocLimit  = 127;
    uint16 gsCuDisableMask = 0;

    if (loadInfo.enableNgg == false)
    {
        const auto&  pgmRsrc1Gs = m_regs.sh.spiShaderPgmRsrc1Gs.bits;
        const auto&  pgmRsrc2Gs = m_regs.sh.spiShaderPgmRsrc2Gs.bits;
        lateAllocLimit          = GraphicsPipeline::CalcMaxLateAllocLimit(m_device,
                                                                          registers,
                                                                          pgmRsrc1Gs.VGPRS,
                                                                          pgmRsrc1Gs.SGPRS,
                                                                          pgmRsrc2Gs.SCRATCH_EN,
                                                                          lateAllocWaves);
    }
    else if (IsGfx10(chipProps.gfxLevel))
    {
        VGT_SHADER_STAGES_EN vgtShaderStagesEn = {};
        vgtShaderStagesEn.u32All = registers.At(mmVGT_SHADER_STAGES_EN);

        if (settings.waLimitLateAllocGsNggFifo)
        {
            lateAllocLimit = 64;
        }
    }

    lateAllocWaves = Min(lateAllocWaves, lateAllocLimit);

    // If late-alloc for NGG is enabled, or if we're using on-chip legacy GS path, we need to avoid using CU1
    // for GS waves to avoid a deadlock with the PS. It is impossible to fully disable LateAlloc on Gfx9+, even
    // with LateAlloc = 0.
    // There are two issues:
    //    1. NGG:
    //       The HW-GS can perform exports which require parameter cache space. There are pending PS waves who have
    //       claims on parameter cache space (before the interpolants are moved to LDS). This can cause a deadlock
    //       where the HW-GS waves are waiting for space in the cache, but that space is claimed by pending PS waves
    //       that can't launch on the CU due to lack of space (already existing waves).
    //    2. On-chip legacy GS:
    //       When on-chip is enabled, the HW-VS must run on the same CU as the HW-GS, since all communication between
    //       the waves are done via LDS. This means that wherever the HW-GS launches is where the HW-VS (copy shader)
    //       will launch. Due to the same issues as above (HW-VS waiting for parameter cache space, pending PS waves),
    //       this could also cause a deadlock.
    if (loadInfo.enableNgg || loadInfo.usesOnChipGs)
    {
        // It is possible, with an NGG shader, that late-alloc GS waves can deadlock the PS.  To prevent this hang
        // situation, we need to mask off one CU when NGG is enabled.
        if (IsGfx10(chipProps.gfxLevel))
        {
            // Both CU's of a WGP need to be disabled for better performance.
            gsCuDisableMask = 0xC;
        }
        else
        {
            // Disable virtualized CU #1 instead of #0 because thread traces use CU #0 by default.
            gsCuDisableMask = 0x2;
        }

        if ((loadInfo.enableNgg) && (settings.allowNggOnAllCusWgps))
        {
            gsCuDisableMask = 0x0;
        }
    }

    m_regs.dynamic.spiShaderPgmRsrc3Gs.bits.CU_EN = m_device.GetCuEnableMask(gsCuDisableMask,
                                                                             settings.gsCuEnLimitMask);
    if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
    {
        m_regs.dynamic.spiShaderPgmRsrc4Gs.gfx09.SPI_SHADER_LATE_ALLOC_GS = lateAllocWaves;
    }
    else // Gfx10+
    {
        // Note that SPI_SHADER_PGM_RSRC4_GS has a totally different layout on Gfx10+ vs. Gfx9!
        m_regs.dynamic.spiShaderPgmRsrc4Gs.gfx10Plus.SPI_SHADER_LATE_ALLOC_GS = lateAllocWaves;

        constexpr uint16 GsCuDisableMaskHi = 0;
        m_regs.dynamic.spiShaderPgmRsrc4Gs.gfx10Plus.CU_EN = m_device.GetCuEnableMaskHi(GsCuDisableMaskHi,
                                                                                    settings.gsCuEnLimitMask);
    }

    if (chipProps.gfx9.supportSpp != 0)
    {
        registers.HasEntry(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_GS, &m_regs.sh.spiShaderPgmChksumGs.u32All);
    }

    if (metadata.pipeline.hasEntry.esGsLdsSize != 0)
    {
        m_regs.sh.userDataLdsEsGsSize.u32All = metadata.pipeline.esGsLdsSize;
    }
    else
    {
        PAL_ASSERT(loadInfo.enableNgg || (loadInfo.usesOnChipGs == false));
    }

    m_regs.context.vgtGsInstanceCnt.u32All    = registers.At(mmVGT_GS_INSTANCE_CNT);
    m_regs.context.vgtGsVertItemSize0.u32All  = registers.At(mmVGT_GS_VERT_ITEMSIZE);
    m_regs.context.vgtGsVertItemSize1.u32All  = registers.At(mmVGT_GS_VERT_ITEMSIZE_1);
    m_regs.context.vgtGsVertItemSize2.u32All  = registers.At(mmVGT_GS_VERT_ITEMSIZE_2);
    m_regs.context.vgtGsVertItemSize3.u32All  = registers.At(mmVGT_GS_VERT_ITEMSIZE_3);
    m_regs.context.vgtGsVsRingOffset1.u32All  = registers.At(mmVGT_GSVS_RING_OFFSET_1);
    m_regs.context.vgtGsVsRingOffset2.u32All  = registers.At(mmVGT_GSVS_RING_OFFSET_2);
    m_regs.context.vgtGsVsRingOffset3.u32All  = registers.At(mmVGT_GSVS_RING_OFFSET_3);
    m_regs.context.vgtGsOutPrimType.u32All    = registers.At(mmVGT_GS_OUT_PRIM_TYPE);
    m_regs.context.vgtGsPerVs.u32All          = registers.At(mmVGT_GS_PER_VS);
    m_regs.context.vgtGsVsRingItemSize.u32All = registers.At(mmVGT_GSVS_RING_ITEMSIZE);
    m_regs.context.vgtEsGsRingItemSize.u32All = registers.At(mmVGT_ESGS_RING_ITEMSIZE);
    m_regs.context.vgtGsMaxVertOut.u32All     = registers.At(mmVGT_GS_MAX_VERT_OUT);

    if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
    {
        m_regs.context.vgtGsMaxPrimsPerSubgroup.u32All = registers.At(Gfx09::mmVGT_GS_MAX_PRIMS_PER_SUBGROUP);
    }
    else
    {
        m_regs.context.geMaxOutputPerSubgroup.u32All = registers.At(Gfx10Plus::mmGE_MAX_OUTPUT_PER_SUBGROUP);
        m_regs.context.spiShaderIdxFormat.u32All     = registers.At(Gfx10Plus::mmSPI_SHADER_IDX_FORMAT);
        m_regs.context.geNggSubgrpCntl.u32All        = registers.At(Gfx10Plus::mmGE_NGG_SUBGRP_CNTL);
    }

    pHasher->Update(m_regs.context);

    if (pUploader->EnableLoadIndexPath())
    {
        pUploader->AddShReg(mmSpiShaderPgmLoEs,        m_regs.sh.spiShaderPgmLoEs);
        pUploader->AddShReg(mmSpiShaderPgmLoEs + 1,    m_regs.sh.spiShaderPgmHiEs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC1_GS, m_regs.sh.spiShaderPgmRsrc1Gs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC2_GS, m_regs.sh.spiShaderPgmRsrc2Gs);

        pUploader->AddShReg(mmSpiShaderUserDataGs0 + ConstBufTblStartReg, m_regs.sh.userDataInternalTable);

        if (chipProps.gfx9.supportSpp != 0)
        {
            pUploader->AddShReg(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_GS, m_regs.sh.spiShaderPgmChksumGs);
        }

        if (loadInfo.enableNgg)
        {
            PAL_ASSERT(m_regs.sh.ldsEsGsSizeRegAddrGs != UserDataNotMapped);
            pUploader->AddShReg(m_regs.sh.ldsEsGsSizeRegAddrGs, m_regs.sh.userDataLdsEsGsSize);
        }
        else if (loadInfo.usesOnChipGs)
        {
            PAL_ASSERT((m_regs.sh.ldsEsGsSizeRegAddrGs != UserDataNotMapped) &&
                       (m_regs.sh.ldsEsGsSizeRegAddrVs != UserDataNotMapped));
            pUploader->AddShReg(m_regs.sh.ldsEsGsSizeRegAddrGs, m_regs.sh.userDataLdsEsGsSize);
            pUploader->AddShReg(m_regs.sh.ldsEsGsSizeRegAddrVs, m_regs.sh.userDataLdsEsGsSize);
        }

        pUploader->AddCtxReg(mmVGT_GS_INSTANCE_CNT,    m_regs.context.vgtGsInstanceCnt);
        pUploader->AddCtxReg(mmVGT_GS_VERT_ITEMSIZE,   m_regs.context.vgtGsVertItemSize0);
        pUploader->AddCtxReg(mmVGT_GS_VERT_ITEMSIZE_1, m_regs.context.vgtGsVertItemSize1);
        pUploader->AddCtxReg(mmVGT_GS_VERT_ITEMSIZE_2, m_regs.context.vgtGsVertItemSize2);
        pUploader->AddCtxReg(mmVGT_GS_VERT_ITEMSIZE_3, m_regs.context.vgtGsVertItemSize3);
        pUploader->AddCtxReg(mmVGT_GSVS_RING_OFFSET_1, m_regs.context.vgtGsVsRingOffset1);
        pUploader->AddCtxReg(mmVGT_GSVS_RING_OFFSET_2, m_regs.context.vgtGsVsRingOffset2);
        pUploader->AddCtxReg(mmVGT_GSVS_RING_OFFSET_3, m_regs.context.vgtGsVsRingOffset3);
        pUploader->AddCtxReg(mmVGT_GS_OUT_PRIM_TYPE,   m_regs.context.vgtGsOutPrimType);
        pUploader->AddCtxReg(mmVGT_GS_PER_VS,          m_regs.context.vgtGsPerVs);
        pUploader->AddCtxReg(mmVGT_GSVS_RING_ITEMSIZE, m_regs.context.vgtGsVsRingItemSize);
        pUploader->AddCtxReg(mmVGT_ESGS_RING_ITEMSIZE, m_regs.context.vgtEsGsRingItemSize);
        pUploader->AddCtxReg(mmVGT_GS_MAX_VERT_OUT,    m_regs.context.vgtGsMaxVertOut);

        if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
        {
            pUploader->AddCtxReg(Gfx09::mmVGT_GS_MAX_PRIMS_PER_SUBGROUP, m_regs.context.vgtGsMaxPrimsPerSubgroup);
        }
        else // Gfx10+
        {
            pUploader->AddCtxReg(Gfx10Plus::mmGE_MAX_OUTPUT_PER_SUBGROUP, m_regs.context.geMaxOutputPerSubgroup);
            pUploader->AddCtxReg(Gfx10Plus::mmSPI_SHADER_IDX_FORMAT,      m_regs.context.spiShaderIdxFormat);
            pUploader->AddCtxReg(Gfx10Plus::mmGE_NGG_SUBGRP_CNTL,         m_regs.context.geNggSubgrpCntl);
        }
    }
}

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
template <bool UseLoadIndexPath>
uint32* PipelineChunkGs::WriteShCommands(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& gsStageInfo
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    if (UseLoadIndexPath == false)
    {
        const RegisterInfo& registerInfo = m_device.CmdUtil().GetRegInfo();

        const uint16 mmSpiShaderUserDataGs0 = registerInfo.mmUserDataStartGsShaderStage;
        const uint16 mmSpiShaderPgmLoEs     = registerInfo.mmSpiShaderPgmLoEs;

        pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmSpiShaderPgmLoEs,
                                                  mmSpiShaderPgmLoEs + 1,
                                                  ShaderGraphics,
                                                  &m_regs.sh.spiShaderPgmLoEs,
                                                  pCmdSpace);

        pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmSPI_SHADER_PGM_RSRC1_GS,
                                                  mmSPI_SHADER_PGM_RSRC2_GS,
                                                  ShaderGraphics,
                                                  &m_regs.sh.spiShaderPgmRsrc1Gs,
                                                  pCmdSpace);

        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(mmSpiShaderUserDataGs0 + ConstBufTblStartReg,
                                                                 m_regs.sh.userDataInternalTable.u32All,
                                                                 pCmdSpace);

        if (chipProps.gfx9.supportSpp != 0)
        {
            pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_GS,
                                                                     m_regs.sh.spiShaderPgmChksumGs.u32All,
                                                                     pCmdSpace);
        }

        if (m_regs.sh.ldsEsGsSizeRegAddrGs != UserDataNotMapped)
        {
            pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_regs.sh.ldsEsGsSizeRegAddrGs,
                                                                     m_regs.sh.userDataLdsEsGsSize.u32All,
                                                                     pCmdSpace);
        }
        if (m_regs.sh.ldsEsGsSizeRegAddrVs != UserDataNotMapped)
        {
            pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_regs.sh.ldsEsGsSizeRegAddrVs,
                                                                     m_regs.sh.userDataLdsEsGsSize.u32All,
                                                                     pCmdSpace);
        }
    }

    auto dynamic = m_regs.dynamic;

    if (gsStageInfo.wavesPerSh > 0)
    {
        dynamic.spiShaderPgmRsrc3Gs.bits.WAVE_LIMIT = gsStageInfo.wavesPerSh;
    }

    if (gsStageInfo.cuEnableMask != 0)
    {
        dynamic.spiShaderPgmRsrc3Gs.bits.CU_EN &= gsStageInfo.cuEnableMask;
        dynamic.spiShaderPgmRsrc4Gs.gfx10Plus.CU_EN =
            Device::AdjustCuEnHi(dynamic.spiShaderPgmRsrc4Gs.gfx10Plus.CU_EN, gsStageInfo.cuEnableMask);
    }

    pCmdSpace = pCmdStream->WriteSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_GS,
                                                  dynamic.spiShaderPgmRsrc3Gs.u32All,
                                                  ShaderGraphics,
                                                  index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                  pCmdSpace);

    if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(mmSPI_SHADER_PGM_RSRC4_GS,
                                                                 dynamic.spiShaderPgmRsrc4Gs.u32All,
                                                                 pCmdSpace);
    }
    else
    {
        pCmdSpace = pCmdStream->WriteSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC4_GS,
                                                      dynamic.spiShaderPgmRsrc4Gs.u32All,
                                                      ShaderGraphics,
                                                      index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                      pCmdSpace);
    }

    if (m_pPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_pPerfDataInfo->regOffset,
                                                                 m_pPerfDataInfo->gpuVirtAddr,
                                                                 pCmdSpace);
    }

    return pCmdSpace;
}

// Instantiate template versions for the linker.
template
uint32* PipelineChunkGs::WriteShCommands<false>(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& gsStageInfo
    ) const;
template
uint32* PipelineChunkGs::WriteShCommands<true>(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& gsStageInfo
    ) const;

// =====================================================================================================================
// Copies this pipeline chunk's context commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
template <bool UseLoadIndexPath>
uint32* PipelineChunkGs::WriteContextCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // NOTE: It is expected that this function will only ever be called when the set path is in use.
    PAL_ASSERT(UseLoadIndexPath == false);

    if (m_device.Parent()->ChipProperties().gfxLevel == GfxIpLevel::GfxIp9)
    {
        pCmdSpace = pCmdStream->WriteSetOneContextReg(Gfx09::mmVGT_GS_MAX_PRIMS_PER_SUBGROUP,
                                                      m_regs.context.vgtGsMaxPrimsPerSubgroup.u32All,
                                                      pCmdSpace);
    }
    else
    {
        pCmdSpace = pCmdStream->WriteSetOneContextReg(Gfx10Plus::mmGE_MAX_OUTPUT_PER_SUBGROUP,
                                                      m_regs.context.geMaxOutputPerSubgroup.u32All,
                                                      pCmdSpace);
        pCmdSpace = pCmdStream->WriteSetOneContextReg(Gfx10Plus::mmSPI_SHADER_IDX_FORMAT,
                                                      m_regs.context.spiShaderIdxFormat.u32All,
                                                      pCmdSpace);
        pCmdSpace = pCmdStream->WriteSetOneContextReg(Gfx10Plus::mmGE_NGG_SUBGRP_CNTL,
                                                      m_regs.context.geNggSubgrpCntl.u32All,
                                                      pCmdSpace);
    }

    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmVGT_GS_MAX_VERT_OUT,
                                                  m_regs.context.vgtGsMaxVertOut.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmVGT_GS_INSTANCE_CNT,
                                                  m_regs.context.vgtGsInstanceCnt.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmVGT_ESGS_RING_ITEMSIZE,
                                                   mmVGT_GSVS_RING_ITEMSIZE,
                                                   &m_regs.context.vgtEsGsRingItemSize,
                                                   pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmVGT_GS_PER_VS,
                                                   mmVGT_GS_OUT_PRIM_TYPE,
                                                   &m_regs.context.vgtGsPerVs,
                                                   pCmdSpace);
    return pCmdStream->WriteSetSeqContextRegs(mmVGT_GS_VERT_ITEMSIZE,
                                              mmVGT_GS_VERT_ITEMSIZE_3,
                                              &m_regs.context.vgtGsVertItemSize0,
                                              pCmdSpace);
}

// Instantiate template versions for the linker.
template
uint32* PipelineChunkGs::WriteContextCommands<false>(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const;
template
uint32* PipelineChunkGs::WriteContextCommands<true>(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const;

} // Gfx9
} // Pal
