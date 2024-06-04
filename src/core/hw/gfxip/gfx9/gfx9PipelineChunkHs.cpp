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
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkHs.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
PipelineChunkHs::PipelineChunkHs(
    const Device&       device,
    const PerfDataInfo* pPerfDataInfo)
    :
    m_device(device),
    m_regs{},
    m_pHsPerfDataInfo(pPerfDataInfo),
    m_stageInfo{}
{
    m_stageInfo.stageId = Abi::HardwareStage::Hs;
    m_regs.sh.userDataInternalTable = InvalidUserDataInternalTable;
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.
void PipelineChunkHs::LateInit(
    const AbiReader&                  abiReader,
    const PalAbi::CodeObjectMetadata& metadata,
    PipelineUploader*                 pUploader)
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    const auto& hwHs = metadata.pipeline.hardwareStage[uint32(Abi::HardwareStage::Hs)];

    GpuSymbol symbol = { };
    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::HsMainEntry, &symbol) == Result::Success)
    {
        m_stageInfo.codeLength     = static_cast<size_t>(symbol.size);
        PAL_ASSERT(IsPow2Aligned(symbol.gpuVirtAddr, 256));

        m_regs.sh.spiShaderPgmLoLs.bits.MEM_BASE = Get256BAddrLo(symbol.gpuVirtAddr);
    }

    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::HsShdrIntrlTblPtr, &symbol) == Result::Success)
    {
        m_regs.sh.userDataInternalTable = LowPart(symbol.gpuVirtAddr);
    }

    const Elf::SymbolTableEntry* pElfSymbol = abiReader.GetPipelineSymbol(Abi::PipelineSymbolType::HsDisassembly);
    if (pElfSymbol != nullptr)
    {
        m_stageInfo.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
    }

    m_regs.sh.spiShaderPgmRsrc1Hs.u32All      = AbiRegisters::SpiShaderPgmRsrc1Hs(metadata);
    m_regs.sh.spiShaderPgmRsrc2Hs.u32All      = AbiRegisters::SpiShaderPgmRsrc2Hs(metadata);
    m_regs.dynamic.spiShaderPgmRsrc3Hs.u32All = AbiRegisters::SpiShaderPgmRsrc3Hs(metadata, m_device);
    m_regs.dynamic.spiShaderPgmRsrc4Hs.u32All =
        AbiRegisters::SpiShaderPgmRsrc4Hs(metadata, m_device, chipProps.gfxLevel, m_stageInfo.codeLength);
    m_regs.sh.spiShaderPgmChksumHs.u32All     = AbiRegisters::SpiShaderPgmChksumHs(metadata, m_device);
    m_regs.context.vgtHosMinTessLevel.u32All  = AbiRegisters::VgtHosMinTessLevel(metadata);
    m_regs.context.vgtHosMaxTessLevel.u32All  = AbiRegisters::VgtHosMaxTessLevel(metadata);
}

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkHs::WriteShCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(mmSPI_SHADER_PGM_LO_LS,
                                                             m_regs.sh.spiShaderPgmLoLs.u32All,
                                                             pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmSPI_SHADER_PGM_RSRC1_HS,
                                              mmSPI_SHADER_PGM_RSRC2_HS,
                                              ShaderGraphics,
                                              &m_regs.sh.spiShaderPgmRsrc1Hs,
                                              pCmdSpace);
    if (m_regs.sh.userDataInternalTable != InvalidUserDataInternalTable)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(
                                    mmSPI_SHADER_USER_DATA_HS_0 + ConstBufTblStartReg,
                                    m_regs.sh.userDataInternalTable,
                                    pCmdSpace);
    }

    if (chipProps.gfx9.supportSpp != 0)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(mmSPI_SHADER_PGM_CHKSUM_HS,
                                                                 m_regs.sh.spiShaderPgmChksumHs.u32All,
                                                                 pCmdSpace);
    }

    if (m_pHsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_pHsPerfDataInfo->regOffset,
                                                                 m_pHsPerfDataInfo->gpuVirtAddr,
                                                                 pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
uint32* PipelineChunkHs::WriteDynamicRegs(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& hsStageInfo
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();
    HsRegs::Dynamic          dynamic   = m_regs.dynamic;

    if (hsStageInfo.wavesPerSh > 0)
    {
        dynamic.spiShaderPgmRsrc3Hs.bits.WAVE_LIMIT = hsStageInfo.wavesPerSh;
    }

    pCmdSpace = pCmdStream->WriteSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_HS,
                                                  dynamic.spiShaderPgmRsrc3Hs.u32All,
                                                  ShaderGraphics,
                                                  index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC4_HS,
                                                  dynamic.spiShaderPgmRsrc4Hs.u32All,
                                                  ShaderGraphics,
                                                  index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                  pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Copies this pipeline chunk's context commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkHs::WriteContextCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    return pCmdStream->WriteSetSeqContextRegs(mmVGT_HOS_MAX_TESS_LEVEL,
                                              mmVGT_HOS_MIN_TESS_LEVEL,
                                              &m_regs.context.vgtHosMaxTessLevel,
                                              pCmdSpace);
}

// =====================================================================================================================
// Accumulates this pipeline chunk's SH registers into an array of packed register pairs.
void PipelineChunkHs::AccumulateShRegs(
    PackedRegisterPair* pRegPairs,
    uint32*             pNumRegs
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    const uint32 startingIdx = *pNumRegs;
#endif

    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    SetOneShRegValPairPacked(pRegPairs,
                             pNumRegs,
                             mmSPI_SHADER_PGM_LO_LS,
                             m_regs.sh.spiShaderPgmLoLs.u32All);

    if (m_regs.sh.userDataInternalTable != InvalidUserDataInternalTable)
    {
        SetOneShRegValPairPacked(pRegPairs,
                                 pNumRegs,
                                 mmSPI_SHADER_USER_DATA_HS_0 + ConstBufTblStartReg,
                                 m_regs.sh.userDataInternalTable);
    }

    SetSeqShRegValPairPacked(pRegPairs,
                             pNumRegs,
                             mmSPI_SHADER_PGM_RSRC1_HS,
                             mmSPI_SHADER_PGM_RSRC2_HS,
                             &m_regs.sh.spiShaderPgmRsrc1Hs);

    if (chipProps.gfx9.supportSpp != 0)
    {
        SetOneShRegValPairPacked(pRegPairs,
                                 pNumRegs,
                                 mmSPI_SHADER_PGM_CHKSUM_HS,
                                 m_regs.sh.spiShaderPgmChksumHs.u32All);
    }

    if (m_pHsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        SetOneShRegValPairPacked(pRegPairs,
                                 pNumRegs,
                                 m_pHsPerfDataInfo->regOffset,
                                 m_pHsPerfDataInfo->gpuVirtAddr);
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    PAL_ASSERT(InRange(*pNumRegs, startingIdx, startingIdx + HsRegs::NumShReg));
#endif
}

// =====================================================================================================================
// Accumulates this pipeline chunk's context registers into an array of packed register pairs.
void PipelineChunkHs::AccumulateContextRegs(
    PackedRegisterPair* pRegPairs,
    uint32*             pNumRegs
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    const uint32 startingIdx = *pNumRegs;
#endif

    SetSeqContextRegValPairPacked(pRegPairs,
                                  pNumRegs,
                                  mmVGT_HOS_MAX_TESS_LEVEL,
                                  mmVGT_HOS_MIN_TESS_LEVEL,
                                  &m_regs.context.vgtHosMaxTessLevel);

#if PAL_ENABLE_PRINTS_ASSERTS
    PAL_ASSERT(InRange(*pNumRegs, startingIdx, startingIdx + HsRegs::NumContextReg));
#endif
}

// =====================================================================================================================
void PipelineChunkHs::Clone(
    const PipelineChunkHs& chunkHs)
{
    m_regs      = chunkHs.m_regs;
    m_stageInfo = chunkHs.m_stageInfo;
}
} // Gfx9
} // Pal
