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

#include "core/platform.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkGs.h"
#include "palPipelineAbiProcessorImpl.h"

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
    1;  // mmVGT_GS_MAX_PRIMS_PER_SUBGROUP

// =====================================================================================================================
PipelineChunkGs::PipelineChunkGs(
    const Device&       device,
    const PerfDataInfo* pPerfDataInfo)
    :
    m_device(device),
    m_pPerfDataInfo(pPerfDataInfo)
{
    memset(&m_commands,  0, sizeof(m_commands));
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

    if (settings.enableLoadIndexForObjectBinds != false)
    {
        pInfo->loadedCtxRegCount += BaseLoadedCntxRegCount;
        pInfo->loadedShRegCount  += (BaseLoadedShRegCount + chipProps.gfx9.supportSpp);

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
    const AbiProcessor&             abiProcessor,
    const CodeObjectMetadata&       metadata,
    const RegisterVector&           registers,
    const GraphicsPipelineLoadInfo& loadInfo,
    GraphicsPipelineUploader*       pUploader,
    Util::MetroHash64*              pHasher)
{
    const bool useLoadIndexPath = pUploader->EnableLoadIndexPath();

    const Gfx9PalSettings&   settings  = m_device.Settings();
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    const uint16 baseUserDataGs     = m_device.GetBaseUserDataReg(HwShaderStage::Gs);
    const uint16 mmSpiShaderPgmLoEs = m_device.CmdUtil().GetRegInfo().mmSpiShaderPgmLoEs;

    BuildPm4Headers(useLoadIndexPath, loadInfo);

    Abi::PipelineSymbolEntry symbol = { };
    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::GsMainEntry, &symbol))
    {
        m_stageInfo.codeLength     = static_cast<size_t>(symbol.size);
        const gpusize programGpuVa = (pUploader->CodeGpuVirtAddr() + symbol.value);
        PAL_ASSERT(IsPow2Aligned(programGpuVa, 256));

        m_commands.sh.spiShaderPgmLoEs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_commands.sh.spiShaderPgmHiEs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::GsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (pUploader->DataGpuVirtAddr() + symbol.value);
        m_commands.sh.spiShaderUserDataLoGs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::GsDisassembly, &symbol))
    {
        m_stageInfo.disassemblyLength = static_cast<size_t>(symbol.size);
    }

    m_commands.sh.spiShaderPgmRsrc1Gs.u32All      = registers.At(mmSPI_SHADER_PGM_RSRC1_GS);
    m_commands.sh.spiShaderPgmRsrc2Gs.u32All      = registers.At(mmSPI_SHADER_PGM_RSRC2_GS);
    m_commands.dynamic.spiShaderPgmRsrc4Gs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC4_GS);
    registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_GS, &m_commands.dynamic.spiShaderPgmRsrc3Gs.u32All);

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_ENABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    m_commands.sh.spiShaderPgmRsrc1Gs.bits.CU_GROUP_ENABLE = (settings.gsCuGroupEnabled ? 1 : 0);

    uint32 lateAllocWaves  = settings.lateAllocGs;
    uint16 gsCuDisableMask = 0;
    if (loadInfo.enableNgg)
    {
        lateAllocWaves = settings.nggLateAllocGs;

        // It is possible, with an NGG shader, that late-alloc GS waves can deadlock the PS.  To prevent this hang
        // situation, we need to mask off one CU when NGG is enabled.
        {
            // Disable virtualized CU #1 instead of #0 because thread traces use CU #0 by default.
            gsCuDisableMask = 0x2;
        }
    }

    m_commands.dynamic.spiShaderPgmRsrc3Gs.bits.CU_EN = m_device.GetCuEnableMask(gsCuDisableMask,
                                                                                 settings.gsCuEnLimitMask);
    if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
    {
        m_commands.dynamic.spiShaderPgmRsrc4Gs.gfx09.SPI_SHADER_LATE_ALLOC_GS = lateAllocWaves;
    }

    if (chipProps.gfx9.supportSpp != 0)
    {
        registers.HasEntry(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_GS, &m_commands.sh.spiShaderPgmChksumGs.u32All);
    }

    if (metadata.pipeline.hasEntry.esGsLdsSize != 0)
    {
        m_commands.shLds.gsUserDataLdsEsGsSize.u32All = metadata.pipeline.esGsLdsSize;
        m_commands.shLds.vsUserDataLdsEsGsSize.u32All = metadata.pipeline.esGsLdsSize;
    }
    else
    {
        PAL_ASSERT(loadInfo.enableNgg || (loadInfo.usesOnChipGs == false));
    }

    m_commands.context.vgtGsInstanceCnt.u32All   = registers.At(mmVGT_GS_INSTANCE_CNT);
    m_commands.context.vgtGsVertItemSize0.u32All = registers.At(mmVGT_GS_VERT_ITEMSIZE);
    m_commands.context.vgtGsVertItemSize1.u32All = registers.At(mmVGT_GS_VERT_ITEMSIZE_1);
    m_commands.context.vgtGsVertItemSize2.u32All = registers.At(mmVGT_GS_VERT_ITEMSIZE_2);
    m_commands.context.vgtGsVertItemSize3.u32All = registers.At(mmVGT_GS_VERT_ITEMSIZE_3);
    m_commands.context.vgtGsVsRingOffset1.u32All = registers.At(mmVGT_GSVS_RING_OFFSET_1);
    m_commands.context.vgtGsVsRingOffset2.u32All = registers.At(mmVGT_GSVS_RING_OFFSET_2);
    m_commands.context.vgtGsVsRingOffset3.u32All = registers.At(mmVGT_GSVS_RING_OFFSET_3);
    m_commands.context.vgtGsOutPrimType.u32All   = registers.At(mmVGT_GS_OUT_PRIM_TYPE);
    m_commands.context.vgtGsPerVs.u32All         = registers.At(mmVGT_GS_PER_VS);
    m_commands.context.gsVsRingItemSize.u32All   = registers.At(mmVGT_GSVS_RING_ITEMSIZE);
    m_commands.context.esGsRingItemSize.u32All   = registers.At(mmVGT_ESGS_RING_ITEMSIZE);
    m_commands.context.vgtGsMaxVertOut.u32All    = registers.At(mmVGT_GS_MAX_VERT_OUT);

    if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
    {
        m_commands.context.maxPrimsPerSubgrp.u32All = registers.At(Gfx09::mmVGT_GS_MAX_PRIMS_PER_SUBGROUP);
    }

    pHasher->Update(m_commands.context);

    if (useLoadIndexPath)
    {
        pUploader->AddShReg(mmSpiShaderPgmLoEs,        m_commands.sh.spiShaderPgmLoEs);
        pUploader->AddShReg(mmSpiShaderPgmLoEs + 1,    m_commands.sh.spiShaderPgmHiEs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC1_GS, m_commands.sh.spiShaderPgmRsrc1Gs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC2_GS, m_commands.sh.spiShaderPgmRsrc2Gs);

        pUploader->AddShReg(baseUserDataGs + ConstBufTblStartReg, m_commands.sh.spiShaderUserDataLoGs);

        if (chipProps.gfx9.supportSpp != 0)
        {
            pUploader->AddShReg(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_GS, m_commands.sh.spiShaderPgmChksumGs);
        }

        if (loadInfo.enableNgg)
        {
            pUploader->AddShReg(loadInfo.esGsLdsSizeRegGs, m_commands.shLds.gsUserDataLdsEsGsSize);
        }
        else if (loadInfo.usesOnChipGs)
        {
            pUploader->AddShReg(loadInfo.esGsLdsSizeRegGs, m_commands.shLds.gsUserDataLdsEsGsSize);
            pUploader->AddShReg(loadInfo.esGsLdsSizeRegVs, m_commands.shLds.vsUserDataLdsEsGsSize);
        }

        pUploader->AddCtxReg(mmVGT_GS_INSTANCE_CNT,    m_commands.context.vgtGsInstanceCnt);
        pUploader->AddCtxReg(mmVGT_GS_VERT_ITEMSIZE,   m_commands.context.vgtGsVertItemSize0);
        pUploader->AddCtxReg(mmVGT_GS_VERT_ITEMSIZE_1, m_commands.context.vgtGsVertItemSize1);
        pUploader->AddCtxReg(mmVGT_GS_VERT_ITEMSIZE_2, m_commands.context.vgtGsVertItemSize2);
        pUploader->AddCtxReg(mmVGT_GS_VERT_ITEMSIZE_3, m_commands.context.vgtGsVertItemSize3);
        pUploader->AddCtxReg(mmVGT_GSVS_RING_OFFSET_1, m_commands.context.vgtGsVsRingOffset1);
        pUploader->AddCtxReg(mmVGT_GSVS_RING_OFFSET_2, m_commands.context.vgtGsVsRingOffset2);
        pUploader->AddCtxReg(mmVGT_GSVS_RING_OFFSET_3, m_commands.context.vgtGsVsRingOffset3);
        pUploader->AddCtxReg(mmVGT_GS_OUT_PRIM_TYPE,   m_commands.context.vgtGsOutPrimType);
        pUploader->AddCtxReg(mmVGT_GS_PER_VS,          m_commands.context.vgtGsPerVs);
        pUploader->AddCtxReg(mmVGT_GSVS_RING_ITEMSIZE, m_commands.context.gsVsRingItemSize);
        pUploader->AddCtxReg(mmVGT_ESGS_RING_ITEMSIZE, m_commands.context.esGsRingItemSize);
        pUploader->AddCtxReg(mmVGT_GS_MAX_VERT_OUT,    m_commands.context.vgtGsMaxVertOut);

        if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
        {
            pUploader->AddCtxReg(Gfx09::mmVGT_GS_MAX_PRIMS_PER_SUBGROUP, m_commands.context.maxPrimsPerSubgrp);
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
    if (UseLoadIndexPath == false)
    {
        pCmdSpace = pCmdStream->WritePm4Image(m_commands.sh.spaceNeeded, &m_commands.sh, pCmdSpace);

        // NOTE: The SH-LDS register PM4 size will be zero if both NGG and on-chip GS are disabled.
        if (m_commands.shLds.spaceNeeded != 0)
        {
            pCmdSpace = pCmdStream->WritePm4Image(m_commands.shLds.spaceNeeded, &m_commands.shLds, pCmdSpace);
        }
    }

    auto dynamicCmds = m_commands.dynamic;

    if (gsStageInfo.wavesPerSh > 0)
    {
        dynamicCmds.spiShaderPgmRsrc3Gs.bits.WAVE_LIMIT = gsStageInfo.wavesPerSh;
    }

    if (gsStageInfo.cuEnableMask != 0)
    {
        dynamicCmds.spiShaderPgmRsrc3Gs.bits.CU_EN &= gsStageInfo.cuEnableMask;
    }

    constexpr uint32 SpaceNeededDynamic = sizeof(m_commands.dynamic) / sizeof(uint32);
    pCmdSpace = pCmdStream->WritePm4Image(SpaceNeededDynamic, &dynamicCmds, pCmdSpace);

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

    return pCmdStream->WritePm4Image(m_commands.context.spaceNeeded, &m_commands.context, pCmdSpace);
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

// =====================================================================================================================
// Assembles the PM4 headers for the commands in this pipeline chunk.
void PipelineChunkGs::BuildPm4Headers(
    bool                            enableLoadIndexPath,
    const GraphicsPipelineLoadInfo& loadInfo)
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();
    const CmdUtil&           cmdUtil   = m_device.CmdUtil();
    const auto&              regInfo   = cmdUtil.GetRegInfo();

    const uint32 mmSpiShaderPgmLoEs           = regInfo.mmSpiShaderPgmLoEs;
    const uint32 mmUserDataStartGsShaderStage = regInfo.mmUserDataStartGsShaderStage;
    const uint32 mmVgtGsMaxPrimsPerSubGroup   = regInfo.mmVgtGsMaxPrimsPerSubGroup;

    m_commands.sh.spaceNeeded = cmdUtil.BuildSetSeqShRegs(mmSpiShaderPgmLoEs,
                                                          mmSpiShaderPgmLoEs + 1,
                                                          ShaderGraphics,
                                                          &m_commands.sh.hdrSpiShaderPgmGs);

    m_commands.sh.spaceNeeded += cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_RSRC1_GS,
                                                           mmSPI_SHADER_PGM_RSRC2_GS,
                                                           ShaderGraphics,
                                                           &m_commands.sh.hdrSpiShaderPgmRsrcGs);

    m_commands.sh.spaceNeeded += cmdUtil.BuildSetOneShReg(mmUserDataStartGsShaderStage + ConstBufTblStartReg,
                                                          ShaderGraphics,
                                                          &m_commands.sh.hdrSpiShaderUserDataGs);

    if (chipProps.gfx9.supportSpp != 0)
    {
        m_commands.sh.spaceNeeded += cmdUtil.BuildSetOneShReg(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_GS,
                                                              ShaderGraphics,
                                                              &m_commands.sh.hdrSpiShaderPgmChksum);
    }
    else
    {
        m_commands.sh.spaceNeeded += cmdUtil.BuildNop(CmdUtil::ShRegSizeDwords + 1,
                                                      &m_commands.sh.hdrSpiShaderPgmChksum);
    }

    if (loadInfo.enableNgg || loadInfo.usesOnChipGs)
    {
        PAL_ASSERT(loadInfo.esGsLdsSizeRegGs != UserDataNotMapped);
        m_commands.shLds.spaceNeeded = cmdUtil.BuildSetOneShReg(loadInfo.esGsLdsSizeRegGs,
                                                                ShaderGraphics,
                                                                &m_commands.shLds.hdrEsGsSizeForGs);
    }
    if ((loadInfo.enableNgg == false) && loadInfo.usesOnChipGs)
    {
        PAL_ASSERT(loadInfo.esGsLdsSizeRegVs != UserDataNotMapped);
        m_commands.shLds.spaceNeeded += cmdUtil.BuildSetOneShReg(loadInfo.esGsLdsSizeRegVs,
                                                                 ShaderGraphics,
                                                                 &m_commands.shLds.hdrEsGsSizeForVs);
    }

    m_commands.context.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_GS_MAX_VERT_OUT,
                                                                    &m_commands.context.hdrVgtGsMaxVertOut);

    m_commands.context.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_GS_OUT_PRIM_TYPE,
                                                                    &m_commands.context.hdrVgtGsOutPrimType);

    m_commands.context.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_GS_INSTANCE_CNT,
                                                                    &m_commands.context.hdrVgtGsInstanceCnt);

    m_commands.context.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmVGT_ESGS_RING_ITEMSIZE,
                                                                     mmVGT_GSVS_RING_ITEMSIZE,
                                                                     &m_commands.context.hdrEsGsVsRingItemSize);

    m_commands.context.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmVGT_GSVS_RING_OFFSET_1,
                                                                     mmVGT_GSVS_RING_OFFSET_3,
                                                                     &m_commands.context.hdrVgtGsVsRingOffset);

    m_commands.context.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_GS_PER_VS,
                                                                    &m_commands.context.hdrVgtGsPerVs);

    m_commands.context.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmVGT_GS_VERT_ITEMSIZE,
                                                                     mmVGT_GS_VERT_ITEMSIZE_3,
                                                                     &m_commands.context.hdrVgtGsVertItemSize);

    m_commands.context.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVgtGsMaxPrimsPerSubGroup,
                                                                    &m_commands.context.hdrVgtMaxPrimsPerSubgrp);

    // NOTE: Supporting real-time compute requires use of SET_SH_REG_INDEX for this register.
    cmdUtil.BuildSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_GS,
                                  ShaderGraphics,
                                  index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                  &m_commands.dynamic.hdrPgmRsrc3Gs);

    if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
    {
        cmdUtil.BuildSetOneShReg(mmSPI_SHADER_PGM_RSRC4_GS,
                                 ShaderGraphics,
                                 &m_commands.dynamic.hdrPgmRsrc4Gs);
    }
}

} // Gfx9
} // Pal
