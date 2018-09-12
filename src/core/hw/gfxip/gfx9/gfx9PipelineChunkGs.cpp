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

#include "core/platform.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkGs.h"
#include "palPipeline.h"
#include "palPipelineAbiProcessorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
PipelineChunkGs::PipelineChunkGs(
    const Device& device)
    :
    m_device(device),
    m_pGsPerfDataInfo(nullptr),
    m_pCopyPerfDataInfo(nullptr)
{
    memset(&m_pm4ImageSh,        0, sizeof(m_pm4ImageSh));
    memset(&m_pm4ImageShDynamic, 0, sizeof(m_pm4ImageShDynamic));
    memset(&m_pm4ImageGsLds,     0, sizeof(m_pm4ImageGsLds));
    memset(&m_pm4ImageContext,   0, sizeof(m_pm4ImageContext));
    memset(&m_stageInfo,         0, sizeof(m_stageInfo));
    memset(&m_stageInfoCopy,     0, sizeof(m_stageInfoCopy));

    m_stageInfo.stageId     = Abi::HardwareStage::Gs;
    m_stageInfoCopy.stageId = Abi::HardwareStage::Vs;
}

// =====================================================================================================================
// Initializes this pipeline chunk for the scenario where the tessellation stages are inactive.
void PipelineChunkGs::Init(
    const AbiProcessor&       abiProcessor,
    const CodeObjectMetadata& metadata,
    const RegisterVector&     registers,
    const GsParams&           params)
{
    BuildPm4Headers(params.usesOnChipGs, params.isNgg, params.esGsLdsSizeRegGs, params.esGsLdsSizeRegVs);

    m_pGsPerfDataInfo   = params.pGsPerfDataInfo;
    m_pCopyPerfDataInfo = params.pCopyPerfDataInfo;

    const Gfx9PalSettings& settings = m_device.Settings();

    m_pm4ImageSh.spiShaderPgmRsrc1Gs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_GS);
    m_pm4ImageSh.spiShaderPgmRsrc2Gs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_GS);
    m_pm4ImageShDynamic.spiShaderPgmRsrc4Gs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC4_GS);
    registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_GS, &m_pm4ImageShDynamic.spiShaderPgmRsrc3Gs.u32All);

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_ENABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    m_pm4ImageSh.spiShaderPgmRsrc1Gs.bits.CU_GROUP_ENABLE = (settings.gsCuGroupEnabled ? 1 : 0);

    uint32 lateAllocWaves  = settings.lateAllocGs;
    uint16 gsCuDisableMask = 0;
    if (params.isNgg)
    {
        lateAllocWaves = settings.nggLateAllocGs;

        // It is possible, with an NGG shader, that late-alloc GS waves can deadlock the PS.  To prevent this hang
        // situation, we need to mask off one CU when NGG is enabled.

        // Disable virtualized CU #1 instead of #0 because thread traces use CU #0 by default.
        gsCuDisableMask = 0x2;
    }

    m_pm4ImageShDynamic.spiShaderPgmRsrc3Gs.bits.CU_EN =
        m_device.GetCuEnableMask(gsCuDisableMask, settings.gsCuEnLimitMask);

    switch (m_device.Parent()->ChipProperties().gfxLevel)
    {
    case GfxIpLevel::GfxIp9:
        m_pm4ImageShDynamic.spiShaderPgmRsrc4Gs.gfx09.SPI_SHADER_LATE_ALLOC_GS = lateAllocWaves;
        m_pm4ImageContext.maxPrimsPerSubgrp.u32All =
            registers.At(Gfx09::mmVGT_GS_MAX_PRIMS_PER_SUBGROUP);
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    if (params.isNgg == false)
    {
        m_pm4ImageSh.spiShaderPgmRsrc1Vs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_VS);
        m_pm4ImageSh.spiShaderPgmRsrc2Vs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_VS);
    }

    m_pm4ImageContext.vgtGsMaxVertOut.u32All    = registers.At(mmVGT_GS_MAX_VERT_OUT);

    m_pm4ImageContext.vgtGsInstanceCnt.u32All   = registers.At(mmVGT_GS_INSTANCE_CNT);
    m_pm4ImageContext.vgtGsVertItemSize0.u32All = registers.At(mmVGT_GS_VERT_ITEMSIZE);
    m_pm4ImageContext.vgtGsVertItemSize1.u32All = registers.At(mmVGT_GS_VERT_ITEMSIZE_1);
    m_pm4ImageContext.vgtGsVertItemSize2.u32All = registers.At(mmVGT_GS_VERT_ITEMSIZE_2);
    m_pm4ImageContext.vgtGsVertItemSize3.u32All = registers.At(mmVGT_GS_VERT_ITEMSIZE_3);
    m_pm4ImageContext.ringOffset1.u32All        = registers.At(mmVGT_GSVS_RING_OFFSET_1);
    m_pm4ImageContext.ringOffset2.u32All        = registers.At(mmVGT_GSVS_RING_OFFSET_2);
    m_pm4ImageContext.ringOffset3.u32All        = registers.At(mmVGT_GSVS_RING_OFFSET_3);
    m_pm4ImageContext.gsVsRingItemSize.u32All   = registers.At(mmVGT_GSVS_RING_ITEMSIZE);
    m_pm4ImageContext.esGsRingItemSize.u32All   = registers.At(mmVGT_ESGS_RING_ITEMSIZE);
    m_pm4ImageContext.vgtGsOutPrimType.u32All   = registers.At(mmVGT_GS_OUT_PRIM_TYPE);

    uint32 esGsLdsSizeBytes = 0;

    if (metadata.pipeline.hasEntry.esGsLdsSize != 0)
    {
        m_pm4ImageGsLds.gsUserDataLdsEsGsSize.u32All = metadata.pipeline.esGsLdsSize;
        m_pm4ImageSh.vsUserDataLdsEsGsSize.u32All    = metadata.pipeline.esGsLdsSize;
    }

    m_pm4ImageContext.vgtGsOnchipCntl.u32All        = registers.At(mmVGT_GS_ONCHIP_CNTL);
    m_pm4ImageContext.vgtGsPerVs.u32All             = registers.At(mmVGT_GS_PER_VS);

    m_pm4ImageContext.spiShaderPosFormat.u32All = registers.At(mmSPI_SHADER_POS_FORMAT);
    m_pm4ImageContext.paClVsOutCntl.u32All      = registers.At(mmPA_CL_VS_OUT_CNTL);
    m_pm4ImageContext.vgtPrimitiveIdEn.u32All   = registers.At(mmVGT_PRIMITIVEID_EN);

    // Compute the checksum here because we don't want it to include the GPU virtual addresses!
    params.pHasher->Update(m_pm4ImageContext);

    Abi::PipelineSymbolEntry symbol = { };
    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::GsMainEntry, &symbol))
    {
        const gpusize programGpuVa = (symbol.value + params.codeGpuVirtAddr);
        PAL_ASSERT(programGpuVa == Pow2Align(programGpuVa, 256));

        m_pm4ImageSh.spiShaderPgmLoEs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_pm4ImageSh.spiShaderPgmHiEs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);

        m_stageInfo.codeLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::VsMainEntry, &symbol))
    {
        const gpusize programGpuVa = (symbol.value + params.codeGpuVirtAddr);
        PAL_ASSERT(programGpuVa == Pow2Align(programGpuVa, 256));

        m_pm4ImageSh.spiShaderPgmLoVs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_pm4ImageSh.spiShaderPgmHiVs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);

        m_stageInfoCopy.codeLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::GsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (symbol.value + params.dataGpuVirtAddr);
        m_pm4ImageSh.spiShaderUserDataLoGs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::VsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (symbol.value + params.dataGpuVirtAddr);
        m_pm4ImageSh.spiShaderUserDataLoVs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::GsDisassembly, &symbol))
    {
        m_stageInfo.disassemblyLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::VsDisassembly, &symbol))
    {
        m_stageInfoCopy.disassemblyLength = static_cast<size_t>(symbol.size);
    }
}

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkGs::WriteShCommands(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& gsStageInfo,
    const DynamicStageInfo& vsStageInfo,
    bool                    isNgg
    ) const
{
    Pm4ImageShDynamic pm4ImageShDynamic = m_pm4ImageShDynamic;

    if (gsStageInfo.wavesPerSh > 0)
    {
        pm4ImageShDynamic.spiShaderPgmRsrc3Gs.bits.WAVE_LIMIT = gsStageInfo.wavesPerSh;
    }

    if (gsStageInfo.cuEnableMask != 0)
    {
        pm4ImageShDynamic.spiShaderPgmRsrc3Gs.bits.CU_EN &= gsStageInfo.cuEnableMask;

    }

    if (isNgg == false)
    {
        pm4ImageShDynamic.spiShaderPgmRsrc3Vs.bits.WAVE_LIMIT = vsStageInfo.wavesPerSh;

        if (vsStageInfo.cuEnableMask != 0)
        {
            pm4ImageShDynamic.spiShaderPgmRsrc3Vs.bits.CU_EN &= vsStageInfo.cuEnableMask;

        }
    }

    pCmdSpace = pCmdStream->WritePm4Image(m_pm4ImageSh.spaceNeeded, &m_pm4ImageSh, pCmdSpace);

    if (m_pm4ImageGsLds.spaceNeeded > 0)
    {
        pCmdSpace = pCmdStream->WritePm4Image(m_pm4ImageGsLds.spaceNeeded, &m_pm4ImageGsLds, pCmdSpace);
    }

    pCmdSpace = pCmdStream->WritePm4Image(pm4ImageShDynamic.spaceNeeded, &pm4ImageShDynamic, pCmdSpace);

    if (m_pGsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_pGsPerfDataInfo->regOffset,
                                                                 m_pGsPerfDataInfo->gpuVirtAddr,
                                                                 pCmdSpace);
    }

    if (m_pCopyPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_pCopyPerfDataInfo->regOffset,
                                                                 m_pCopyPerfDataInfo->gpuVirtAddr,
                                                                 pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Copies this pipeline chunk's context commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkGs::WriteContextCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    pCmdSpace = pCmdStream->WritePm4Image(m_pm4ImageContext.spaceNeeded, &m_pm4ImageContext, pCmdSpace);
    return pCmdSpace;
}

// =====================================================================================================================
// Assembles the PM4 headers for the commands in this pipeline chunk.
void PipelineChunkGs::BuildPm4Headers(
    bool   useOnchipGs,
    bool   isNgg,
    uint16 esGsLdsSizeRegAddrGs,
    uint16 esGsLdsSizeRegAddrVs)
{
    const CmdUtil& cmdUtil                      = m_device.CmdUtil();
    const auto&    regInfo                      = cmdUtil.GetRegInfo();
    const uint32   mmSpiShaderPgmLoEs           = regInfo.mmSpiShaderPgmLoEs;
    const uint32   mmUserDataStartGsShaderStage = regInfo.mmUserDataStartGsShaderStage;
    const uint32   mmVgtGsMaxPrimsPerSubGroup   = regInfo.mmVgtGsMaxPrimsPerSubGroup;

    // Sets the following SH registers: SPI_SHADER_PGM_LO_ES, SPI_SHADER_PGM_HI_ES
    m_pm4ImageSh.spaceNeeded = cmdUtil.BuildSetSeqShRegs(mmSpiShaderPgmLoEs,
                                                         mmSpiShaderPgmLoEs + 1,
                                                         ShaderGraphics,
                                                         &m_pm4ImageSh.hdrSpiShaderPgmEs);

    // Sets the following SH register: SPI_SHADER_USER_DATA_ES_1.
    m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetOneShReg(mmUserDataStartGsShaderStage + ConstBufTblStartReg,
                                                         ShaderGraphics,
                                                         &m_pm4ImageSh.hdrSpiShaderUserDataEs);

    // Sets the following SH registers: SPI_SHADER_PGM_RSRC1_GS, SPI_SHADER_PGM_RSRC2_GS.
    m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_RSRC1_GS,
                                                          mmSPI_SHADER_PGM_RSRC2_GS,
                                                          ShaderGraphics,
                                                          &m_pm4ImageSh.hdrSpiShaderPgmGs);

    // Sets the following SH register: SPI_SHADER_PGM_RSRC3_GS.
    // We must use the SET_SH_REG_INDEX packet to support the real-time compute feature.
    m_pm4ImageShDynamic.spaceNeeded = cmdUtil.BuildSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_GS,
                                                                    ShaderGraphics,
                                                                    index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                                    &m_pm4ImageShDynamic.hdrPgmRsrc3Gs);

    // Sets the following SH register: SPI_SHADER_PGM_RSRC4_GS.
    if (m_device.Parent()->ChipProperties().gfxLevel == GfxIpLevel::GfxIp9)
    {
        m_pm4ImageShDynamic.spaceNeeded += cmdUtil.BuildSetOneShReg(mmSPI_SHADER_PGM_RSRC4_GS,
                                                                    ShaderGraphics,
                                                                    &m_pm4ImageShDynamic.hdrPgmRsrc4Gs);
    }

    // Sets the following context register: VGT_GS_MAX_VERT_OUT.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_GS_MAX_VERT_OUT,
                                                                   &m_pm4ImageContext.hdrVgtGsMaxVertOut);

    // Sets the following context register: VGT_GS_OUT_PRIM_TYPE.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_GS_OUT_PRIM_TYPE,
                                                                   &m_pm4ImageContext.hdrVgtGsOutPrimType);

    // Sets the following context register: VGT_GS_INSTANCE_CNT.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_GS_INSTANCE_CNT,
                                                                   &m_pm4ImageContext.hdrVgtGsInstanceCnt);

    // Sets the following context registers: VGT_ESGS_RING_ITEMSIZE and VGT_GSVS_RING_ITEMSIZE.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmVGT_ESGS_RING_ITEMSIZE,
                                                                    mmVGT_GSVS_RING_ITEMSIZE,
                                                                    &m_pm4ImageContext.hdrEsGsVsRingItemSize);

    // Sets the following context registers: VGT_GSVS_RING_OFFSET_1, VGT_GSVS_RING_OFFSET_2, and VGT_GSVS_RING_OFFSET_3.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmVGT_GSVS_RING_OFFSET_1,
                                                                    mmVGT_GSVS_RING_OFFSET_3,
                                                                    &m_pm4ImageContext.hdrVgtGsVsRingOffset);

    // Sets the following context register: VGT_GS_PER_VS.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_GS_PER_VS, &m_pm4ImageContext.hdrVgtGsPerVs);

    // Sets the following context registers: VGT_GS_VERT_ITEMSIZE, VGT_GS_VERT_ITEMSIZE_1,
    // VGT_GS_VERT_ITEMSIZE_2, VGT_GS_VERT_ITEMSIZE_3.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmVGT_GS_VERT_ITEMSIZE, mmVGT_GS_VERT_ITEMSIZE_3,
                                                                    &m_pm4ImageContext.hdrVgtGsVertItemSize);

    // Sets the following context register: VGT_GS_MAX_PRIMS_PER_SUBGROUP.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVgtGsMaxPrimsPerSubGroup,
                                                                   &m_pm4ImageContext.hdrVgtMaxPrimsPerSubgrp);

    // NOTE: It is unclear whether we need to write this register if a pipeline uses offchip GS mode.  DXX seems to
    // always write the register for Sea Islands and newer hardware.
    // Sets the following context register: VGT_GS_ONCHIP_CNTL
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_GS_ONCHIP_CNTL,
                                                                   &m_pm4ImageContext.hdrVgtGsOnchipCntl);

    // Sets the following context register: SPI_SHADER_POS_FORMAT.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmSPI_SHADER_POS_FORMAT,
                                                                   &m_pm4ImageContext.hdrSpiShaderPosFormat);

    // Sets the following context register: PA_CL_VS_OUT_CNTL.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmPA_CL_VS_OUT_CNTL,
                                                                   &m_pm4ImageContext.hdrPaClVsOutCntl);

    // Sets the following context register: VGT_PRIMITIVEID_EN.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_PRIMITIVEID_EN,
                                                                   &m_pm4ImageContext.hdrVgtPrimitiveIdEn);

    if (isNgg == false)
    {
        // Sets the following SH registers: SPI_SHADER_PGM_LO_VS, SPI_SHADER_PGM_HI_VS,
        // SPI_SHADER_PGM_RSRC1_VS, SPI_SHADER_PGM_RSRC2_VS.
        m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_LO_VS,
                                                              mmSPI_SHADER_PGM_RSRC2_VS,
                                                              ShaderGraphics,
                                                              &m_pm4ImageSh.hdrSpiShaderPgmVs);

        // Sets the following SH registers: SPI_SHADER_USER_DATA_VS_1.
        m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_VS_0 + ConstBufTblStartReg,
                                                             ShaderGraphics,
                                                             &m_pm4ImageSh.hdrSpiShaderUserDataVs);

        // Sets the following SH register: SPI_SHADER_PGM_RSRC3_VS.
        // We must use the SET_SH_REG_INDEX packet to support the real-time compute feature.
        m_pm4ImageShDynamic.spaceNeeded += cmdUtil.BuildSetOneShRegIndex(
                                                mmSPI_SHADER_PGM_RSRC3_VS,
                                                ShaderGraphics,
                                                index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                &m_pm4ImageShDynamic.hdrPgmRsrc3Vs);

        if (useOnchipGs)
        {
            PAL_ASSERT(esGsLdsSizeRegAddrGs != UserDataNotMapped);
            m_pm4ImageGsLds.spaceNeeded += cmdUtil.BuildSetOneShReg(esGsLdsSizeRegAddrGs,
                                                                    ShaderGraphics,
                                                                    &m_pm4ImageGsLds.hdrEsGsSizeForGs);
            PAL_ASSERT(esGsLdsSizeRegAddrVs != UserDataNotMapped);
            m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetOneShReg(esGsLdsSizeRegAddrVs,
                                                                 ShaderGraphics,
                                                                 &m_pm4ImageSh.hdrEsGsSizeForVs);
        }
    }
    else
    {
        PAL_ASSERT(esGsLdsSizeRegAddrGs != UserDataNotMapped);
        m_pm4ImageGsLds.spaceNeeded += cmdUtil.BuildSetOneShReg(esGsLdsSizeRegAddrGs,
                                                                ShaderGraphics,
                                                                &m_pm4ImageGsLds.hdrEsGsSizeForGs);
    }

}

} // Gfx9
} // Pal
