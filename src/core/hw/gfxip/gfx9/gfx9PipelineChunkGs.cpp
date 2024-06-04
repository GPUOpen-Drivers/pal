/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9AbiToPipelineRegisters.h"
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

// =====================================================================================================================
PipelineChunkGs::PipelineChunkGs(
    const Device&       device,
    const PerfDataInfo* pPerfDataInfo)
    :
    m_device(device),
    m_regs{},
    m_pPerfDataInfo(pPerfDataInfo),
    m_stageInfo{},
    m_fastLaunchMode(GsFastLaunchMode::Disabled)
{
    m_stageInfo.stageId = Abi::HardwareStage::Gs;
    m_regs.sh.userDataInternalTable.u32All = InvalidUserDataInternalTable;
}

// =====================================================================================================================
// Early initialization for this pipeline chunk.  Responsible for determining the number of SH and context registers to
// be loaded using LOAD_CNTX_REG_INDEX and LOAD_SH_REG_INDEX.
void PipelineChunkGs::EarlyInit(
    GraphicsPipelineLoadInfo* pInfo)
{
    PAL_ASSERT(pInfo != nullptr);

    m_regs.sh.ldsEsGsSizeRegAddrGs = pInfo->esGsLdsSizeRegGs;
    m_regs.sh.ldsEsGsSizeRegAddrVs = pInfo->esGsLdsSizeRegVs;
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.
void PipelineChunkGs::LateInit(
    const AbiReader&                  abiReader,
    const PalAbi::CodeObjectMetadata& metadata,
    const GraphicsPipelineLoadInfo&   loadInfo,
    const GraphicsPipelineCreateInfo& createInfo,
    PipelineUploader*                 pUploader)
{
    const GpuChipProperties& chipProps    = m_device.Parent()->ChipProperties();

    GpuSymbol symbol = { };
    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::GsMainEntry, &symbol) == Result::Success)
    {
        m_stageInfo.codeLength     = static_cast<size_t>(symbol.size);
        PAL_ASSERT(IsPow2Aligned(symbol.gpuVirtAddr, 256));

        m_regs.sh.spiShaderPgmLoEs.bits.MEM_BASE = Get256BAddrLo(symbol.gpuVirtAddr);
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

    m_fastLaunchMode = static_cast<GsFastLaunchMode>(metadata.pipeline.graphicsRegister.vgtShaderStagesEn.gsFastLaunch);

    m_regs.sh.spiShaderPgmRsrc1Gs.u32All      = AbiRegisters::SpiShaderPgmRsrc1Gs(metadata, m_device);
    m_regs.sh.spiShaderPgmRsrc2Gs.u32All      = AbiRegisters::SpiShaderPgmRsrc2Gs(metadata);
    m_regs.dynamic.spiShaderPgmRsrc3Gs.u32All = AbiRegisters::SpiShaderPgmRsrc3Gs(metadata,
                                                                                  m_device,
                                                                                  chipProps.gfxLevel,
                                                                                  loadInfo.enableNgg,
                                                                                  loadInfo.usesOnChipGs);
    m_regs.dynamic.spiShaderPgmRsrc4Gs.u32All = AbiRegisters::SpiShaderPgmRsrc4Gs(metadata,
                                                                                  m_device,
                                                                                  chipProps.gfxLevel,
                                                                                  loadInfo.enableNgg,
                                                                                  m_stageInfo.codeLength,
                                                                                  createInfo);
    m_regs.sh.spiShaderPgmChksumGs.u32All = AbiRegisters::SpiShaderPgmChksumGs(metadata, m_device);

    m_regs.sh.spiShaderGsMeshletDim.u32All      = AbiRegisters::SpiShaderGsMeshletDim(metadata);
    m_regs.sh.spiShaderGsMeshletExpAlloc.u32All = AbiRegisters::SpiShaderGsMeshletExpAlloc(metadata);

    if (metadata.pipeline.hasEntry.esGsLdsSize != 0)
    {
        m_regs.sh.userDataLdsEsGsSize.u32All = metadata.pipeline.esGsLdsSize;
    }

    m_regs.context.vgtGsInstanceCnt.u32All    = AbiRegisters::VgtGsInstanceCnt(metadata);
    m_regs.context.vgtGsOutPrimType.u32All    = AbiRegisters::VgtGsOutPrimType(metadata, chipProps.gfxLevel);
    m_regs.context.vgtEsGsRingItemSize.u32All = AbiRegisters::VgtEsGsRingItemSize(metadata);
    m_regs.context.vgtGsMaxVertOut.u32All     = AbiRegisters::VgtGsMaxVertOut(metadata);
    m_regs.context.geNggSubgrpCntl.u32All     = AbiRegisters::GeNggSubgrpCntl(metadata);
    m_regs.context.paClNggCntl.u32All         = AbiRegisters::PaClNggCntl(createInfo, chipProps.gfxLevel);

    if (chipProps.gfxip.supportsHwVs)
    {
        bool allHere = true;
        m_regs.context.vgtGsPerVs.u32All          = AbiRegisters::VgtGsPerVs(metadata, &allHere);
        m_regs.context.vgtGsVsRingItemSize.u32All = AbiRegisters::VgtGsVsRingItemsize(metadata, &allHere);
        AbiRegisters::VgtGsVertItemsizes(metadata,
                                         &m_regs.context.vgtGsVertItemSize0,
                                         &m_regs.context.vgtGsVertItemSize1,
                                         &m_regs.context.vgtGsVertItemSize2,
                                         &m_regs.context.vgtGsVertItemSize3,
                                         &allHere);
        AbiRegisters::VgtGsVsRingOffsets(metadata,
                                         &m_regs.context.vgtGsVsRingOffset1,
                                         &m_regs.context.vgtGsVsRingOffset2,
                                         &m_regs.context.vgtGsVsRingOffset3,
                                         &allHere);

        PAL_ASSERT(loadInfo.enableNgg || allHere);
    }

    AbiRegisters::GeMaxOutputPerSubgroup(metadata, &m_regs.context.geMaxOutputPerSubgroup);
}

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkGs::WriteShCommands(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const bool              hasMeshShader
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(mmSPI_SHADER_PGM_LO_ES,
                                                             m_regs.sh.spiShaderPgmLoEs.u32All,
                                                             pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmSPI_SHADER_PGM_RSRC1_GS,
                                              mmSPI_SHADER_PGM_RSRC2_GS,
                                              ShaderGraphics,
                                              &m_regs.sh.spiShaderPgmRsrc1Gs,
                                              pCmdSpace);

    if (m_regs.sh.userDataInternalTable.u32All != InvalidUserDataInternalTable)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(
                                    mmSPI_SHADER_USER_DATA_GS_0 + ConstBufTblStartReg,
                                    m_regs.sh.userDataInternalTable.u32All,
                                    pCmdSpace);
    }

    if (chipProps.gfx9.supportSpp != 0)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(mmSPI_SHADER_PGM_CHKSUM_GS,
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

    if (hasMeshShader && (m_fastLaunchMode == GsFastLaunchMode::PrimInLane))
    {
        pCmdSpace = pCmdStream->WriteSetSeqShRegs(Gfx11::mmSPI_SHADER_GS_MESHLET_DIM,
                                                  Gfx11::mmSPI_SHADER_GS_MESHLET_EXP_ALLOC,
                                                  ShaderGraphics,
                                                  &m_regs.sh.spiShaderGsMeshletDim,
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

// =====================================================================================================================
uint32* PipelineChunkGs::WriteDynamicRegs(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& gsStageInfo
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();
    GsRegs::Dynamic          dynamic   = m_regs.dynamic;

    if (gsStageInfo.wavesPerSh > 0)
    {
        dynamic.spiShaderPgmRsrc3Gs.bits.WAVE_LIMIT = gsStageInfo.wavesPerSh;
    }

    pCmdSpace = pCmdStream->WriteSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_GS,
                                                  dynamic.spiShaderPgmRsrc3Gs.u32All,
                                                  ShaderGraphics,
                                                  index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC4_GS,
                                                  dynamic.spiShaderPgmRsrc4Gs.u32All,
                                                  ShaderGraphics,
                                                  index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                  pCmdSpace);

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
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmGE_MAX_OUTPUT_PER_SUBGROUP,
                                                  m_regs.context.geMaxOutputPerSubgroup.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmGE_NGG_SUBGRP_CNTL,
                                                  m_regs.context.geNggSubgrpCntl.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmPA_CL_NGG_CNTL,
                                                  m_regs.context.paClNggCntl.u32All,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmVGT_GS_MAX_VERT_OUT,
                                                  m_regs.context.vgtGsMaxVertOut.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmVGT_GS_INSTANCE_CNT,
                                                  m_regs.context.vgtGsInstanceCnt.u32All,
                                                  pCmdSpace);

    if (IsGfx11(*m_device.Parent()))
    {
        pCmdSpace = pCmdStream->WriteSetOneContextReg(mmVGT_ESGS_RING_ITEMSIZE,
                                                      m_regs.context.vgtEsGsRingItemSize.u32All,
                                                      pCmdSpace);
    }
    else
    {
        pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmVGT_ESGS_RING_ITEMSIZE,
                                                       Gfx10::mmVGT_GSVS_RING_ITEMSIZE,
                                                       &m_regs.context.vgtEsGsRingItemSize,
                                                       pCmdSpace);

        pCmdSpace = pCmdStream->WriteSetSeqContextRegs(Gfx10::mmVGT_GS_PER_VS,
                                                       Gfx10::mmVGT_GS_OUT_PRIM_TYPE,
                                                       &m_regs.context.vgtGsPerVs,
                                                       pCmdSpace);

        pCmdSpace = pCmdStream->WriteSetSeqContextRegs(Gfx10::mmVGT_GS_VERT_ITEMSIZE,
                                                       Gfx10::mmVGT_GS_VERT_ITEMSIZE_3,
                                                       &m_regs.context.vgtGsVertItemSize0,
                                                       pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Accumulates this pipeline chunk's SH registers into an array of packed register pairs.
void PipelineChunkGs::AccumulateShRegs(
    PackedRegisterPair* pRegPairs,
    uint32*             pNumRegs,
    const bool          hasMeshShader
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    const uint32 startingIdx = *pNumRegs;
#endif

    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    SetOneShRegValPairPacked(pRegPairs, pNumRegs, mmSPI_SHADER_PGM_LO_ES, m_regs.sh.spiShaderPgmLoEs.u32All);
    SetSeqShRegValPairPacked(pRegPairs,
                             pNumRegs,
                             mmSPI_SHADER_PGM_RSRC1_GS,
                             mmSPI_SHADER_PGM_RSRC2_GS,
                             &m_regs.sh.spiShaderPgmRsrc1Gs);

    if (m_regs.sh.userDataInternalTable.u32All != InvalidUserDataInternalTable)
    {
        SetOneShRegValPairPacked(pRegPairs,
                                 pNumRegs,
                                 mmSPI_SHADER_USER_DATA_GS_0 + ConstBufTblStartReg,
                                 m_regs.sh.userDataInternalTable.u32All);
    }

    if (chipProps.gfx9.supportSpp != 0)
    {
        SetOneShRegValPairPacked(pRegPairs,
                                 pNumRegs,
                                 mmSPI_SHADER_PGM_CHKSUM_GS,
                                 m_regs.sh.spiShaderPgmChksumGs.u32All);
    }

    if (m_regs.sh.ldsEsGsSizeRegAddrGs != UserDataNotMapped)
    {
        SetOneShRegValPairPacked(pRegPairs,
                                 pNumRegs,
                                 m_regs.sh.ldsEsGsSizeRegAddrGs,
                                 m_regs.sh.userDataLdsEsGsSize.u32All);
    }
    if (m_regs.sh.ldsEsGsSizeRegAddrVs != UserDataNotMapped)
    {
        SetOneShRegValPairPacked(pRegPairs,
                                 pNumRegs,
                                 m_regs.sh.ldsEsGsSizeRegAddrVs,
                                 m_regs.sh.userDataLdsEsGsSize.u32All);
    }

    if (hasMeshShader && (m_fastLaunchMode == GsFastLaunchMode::PrimInLane))
    {
        SetSeqShRegValPairPacked(pRegPairs,
                                 pNumRegs,
                                 Gfx11::mmSPI_SHADER_GS_MESHLET_DIM,
                                 Gfx11::mmSPI_SHADER_GS_MESHLET_EXP_ALLOC,
                                 &m_regs.sh.spiShaderGsMeshletDim);
    }

    if (m_pPerfDataInfo->regOffset != UserDataNotMapped)
    {
        SetOneShRegValPairPacked(pRegPairs, pNumRegs, m_pPerfDataInfo->regOffset, m_pPerfDataInfo->gpuVirtAddr);
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    PAL_ASSERT(InRange(*pNumRegs, startingIdx, startingIdx + GsRegs::NumShReg));
#endif
}

// =====================================================================================================================
// Accumulates this pipeline chunk's context registers into an array of packed register pairs.
void PipelineChunkGs::AccumulateContextRegs(
    PackedRegisterPair* pRegPairs,
    uint32*             pNumRegs
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    const uint32 startingIdx = *pNumRegs;
#endif

    SetOneContextRegValPairPacked(pRegPairs, pNumRegs,
                                  mmGE_MAX_OUTPUT_PER_SUBGROUP,
                                  m_regs.context.geMaxOutputPerSubgroup.u32All);
    SetOneContextRegValPairPacked(pRegPairs,
                                  pNumRegs,
                                  mmGE_NGG_SUBGRP_CNTL,
                                  m_regs.context.geNggSubgrpCntl.u32All);

    SetOneContextRegValPairPacked(pRegPairs,
                                  pNumRegs,
                                  mmPA_CL_NGG_CNTL,
                                  m_regs.context.paClNggCntl.u32All);
    SetOneContextRegValPairPacked(pRegPairs,
                                  pNumRegs,
                                  mmVGT_GS_MAX_VERT_OUT,
                                  m_regs.context.vgtGsMaxVertOut.u32All);
    SetOneContextRegValPairPacked(pRegPairs,
                                  pNumRegs,
                                  mmVGT_GS_INSTANCE_CNT,
                                  m_regs.context.vgtGsInstanceCnt.u32All);

    SetOneContextRegValPairPacked(pRegPairs,
                                  pNumRegs,
                                  mmVGT_ESGS_RING_ITEMSIZE,
                                  m_regs.context.vgtEsGsRingItemSize.u32All);

#if PAL_ENABLE_PRINTS_ASSERTS
    PAL_ASSERT(InRange(*pNumRegs, startingIdx, startingIdx + GsRegs::NumContextReg));
#endif
}

// =====================================================================================================================
void PipelineChunkGs::Clone(
    const PipelineChunkGs& chunkGs)
{
    m_regs           = chunkGs.m_regs;
    m_stageInfo      = chunkGs.m_stageInfo;
    m_fastLaunchMode = chunkGs.m_fastLaunchMode;
}
} // Gfx9
} // Pal
