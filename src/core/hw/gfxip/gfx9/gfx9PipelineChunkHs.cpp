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
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkHs.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// Base count of SH registers which are loaded using LOAD_SH_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedShRegCount =
    1 + // mmSPI_SHADER_PGM_LO_LS
    1 + // mmSPI_SHADER_PGM_HI_LS
    1 + // SPI_SHADER_PGM_RSRC1_HS
    1 + // SPI_SHADER_PGM_RSRC2_HS
    0 + // SPI_SHADER_PGM_CHKSUM_HS is not included because it is not present on all HW
    1;  // SPI_SHADER_USER_DATA_LS_0 + ConstBufTblStartReg

// Base count of Context registers which are loaded using LOAD_CNTX_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedCntxRegCount =
    1 + // VGT_HOS_MIN_TESS_LEVEL
    1;  // VGT_HOS_MAX_TESS_LEVEL

// =====================================================================================================================
PipelineChunkHs::PipelineChunkHs(
    const Device&       device,
    const PerfDataInfo* pPerfDataInfo)
    :
    m_device(device),
    m_pHsPerfDataInfo(pPerfDataInfo)
{
    memset(&m_regs, 0, sizeof(m_regs));
    memset(&m_stageInfo, 0, sizeof(m_stageInfo));
    m_stageInfo.stageId = Abi::HardwareStage::Hs;
}

// =====================================================================================================================
// Early initialization for this pipeline chunk.  Responsible for determining the number of SH and context registers to
// be loaded using LOAD_CNTX_REG_INDEX and LOAD_SH_REG_INDEX.
void PipelineChunkHs::EarlyInit(
    GraphicsPipelineLoadInfo* pInfo)
{
    PAL_ASSERT(pInfo != nullptr);

    const Gfx9PalSettings&   settings  = m_device.Settings();
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    if (settings.enableLoadIndexForObjectBinds != false)
    {
        pInfo->loadedCtxRegCount += BaseLoadedCntxRegCount;
        pInfo->loadedShRegCount  += (BaseLoadedShRegCount + ((chipProps.gfx9.supportSpp == 1) ? 1 : 0));
    }
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.  Also uploads register state into GPU memory.
void PipelineChunkHs::LateInit(
    const AbiReader&          abiReader,
    const RegisterVector&     registers,
    GraphicsPipelineUploader* pUploader,
    MetroHash64*              pHasher)
{
    const GpuChipProperties& chipProps    = m_device.Parent()->ChipProperties();
    const RegisterInfo&      registerInfo = m_device.CmdUtil().GetRegInfo();

    const uint16 mmSpiShaderUserDataHs0 = registerInfo.mmUserDataStartHsShaderStage;
    const uint16 mmSpiShaderPgmLoLs     = registerInfo.mmSpiShaderPgmLoLs;

    GpuSymbol symbol = { };
    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::HsMainEntry, &symbol) == Result::Success)
    {
        m_stageInfo.codeLength     = static_cast<size_t>(symbol.size);
        PAL_ASSERT(IsPow2Aligned(symbol.gpuVirtAddr, 256));

        m_regs.sh.spiShaderPgmLoLs.bits.MEM_BASE = Get256BAddrLo(symbol.gpuVirtAddr);
        m_regs.sh.spiShaderPgmHiLs.bits.MEM_BASE = Get256BAddrHi(symbol.gpuVirtAddr);
    }

    regSPI_SHADER_USER_DATA_LS_0 spiShaderUserDataLoHs = { };
    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::HsShdrIntrlTblPtr, &symbol) == Result::Success)
    {
        m_regs.sh.userDataInternalTable = LowPart(symbol.gpuVirtAddr);
    }

    const Elf::SymbolTableEntry* pElfSymbol = abiReader.GetPipelineSymbol(Abi::PipelineSymbolType::HsDisassembly);
    if (pElfSymbol != nullptr)
    {
        m_stageInfo.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
    }

    m_regs.sh.spiShaderPgmRsrc1Hs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_HS);
    m_regs.sh.spiShaderPgmRsrc2Hs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_HS);
    registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_HS, &m_regs.dynamic.spiShaderPgmRsrc3Hs.u32All);

    // NOTE: The Pipeline ABI doesn't specify CU enable masks for each shader stage, so it should be safe to
    // always use the ones PAL prefers.
    m_regs.dynamic.spiShaderPgmRsrc3Hs.bits.CU_EN = m_device.GetCuEnableMask(0, UINT_MAX);

    if (IsGfx10Plus(chipProps.gfxLevel))
    {
        m_regs.dynamic.spiShaderPgmRsrc4Hs.gfx10Plus.CU_EN = m_device.GetCuEnableMaskHi(0, UINT_MAX);

#if PAL_ENABLE_PRINTS_ASSERTS
        m_device.AssertUserAccumRegsDisabled(registers, Gfx10Plus::mmSPI_SHADER_USER_ACCUM_LSHS_0);
#endif
    }

    if (chipProps.gfx9.supportSpp != 0)
    {
        registers.HasEntry(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_HS, &m_regs.sh.spiShaderPgmChksumHs.u32All);
    }

    m_regs.context.vgtHosMinTessLevel.u32All = registers.At(mmVGT_HOS_MIN_TESS_LEVEL);
    m_regs.context.vgtHosMaxTessLevel.u32All = registers.At(mmVGT_HOS_MAX_TESS_LEVEL);

    pHasher->Update(m_regs.context);

    if (pUploader->EnableLoadIndexPath())
    {
        pUploader->AddShReg(mmSpiShaderPgmLoLs,        m_regs.sh.spiShaderPgmLoLs);
        pUploader->AddShReg(mmSpiShaderPgmLoLs + 1,    m_regs.sh.spiShaderPgmHiLs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC1_HS, m_regs.sh.spiShaderPgmRsrc1Hs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC2_HS, m_regs.sh.spiShaderPgmRsrc2Hs);

        pUploader->AddShReg(mmSpiShaderUserDataHs0 + ConstBufTblStartReg, m_regs.sh.userDataInternalTable);

        if (chipProps.gfx9.supportSpp != 0)
        {
            pUploader->AddShReg(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_HS, m_regs.sh.spiShaderPgmChksumHs);
        }

        pUploader->AddCtxReg(mmVGT_HOS_MIN_TESS_LEVEL, m_regs.context.vgtHosMinTessLevel);
        pUploader->AddCtxReg(mmVGT_HOS_MAX_TESS_LEVEL, m_regs.context.vgtHosMaxTessLevel);
    }
}

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
template <bool UseLoadIndexPath>
uint32* PipelineChunkHs::WriteShCommands(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& hsStageInfo
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    if (UseLoadIndexPath == false)
    {
        const RegisterInfo& registerInfo = m_device.CmdUtil().GetRegInfo();

        const uint16 mmSpiShaderUserDataHs0 = registerInfo.mmUserDataStartHsShaderStage;
        const uint16 mmSpiShaderPgmLoLs     = registerInfo.mmSpiShaderPgmLoLs;

        pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmSpiShaderPgmLoLs,
                                                  mmSpiShaderPgmLoLs + 1,
                                                  ShaderGraphics,
                                                  &m_regs.sh.spiShaderPgmLoLs,
                                                  pCmdSpace);
       pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmSPI_SHADER_PGM_RSRC1_HS,
                                                 mmSPI_SHADER_PGM_RSRC2_HS,
                                                 ShaderGraphics,
                                                 &m_regs.sh.spiShaderPgmRsrc1Hs,
                                                 pCmdSpace);

        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(mmSpiShaderUserDataHs0 + ConstBufTblStartReg,
                                                                 m_regs.sh.userDataInternalTable,
                                                                 pCmdSpace);

        if (chipProps.gfx9.supportSpp != 0)
        {
            pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_HS,
                                                                     m_regs.sh.spiShaderPgmChksumHs.u32All,
                                                                     pCmdSpace);
        }
    }

    auto dynamic = m_regs.dynamic;

    if (hsStageInfo.wavesPerSh > 0)
    {
        dynamic.spiShaderPgmRsrc3Hs.bits.WAVE_LIMIT = hsStageInfo.wavesPerSh;
    }

    if (hsStageInfo.cuEnableMask != 0)
    {
        dynamic.spiShaderPgmRsrc3Hs.bits.CU_EN &= hsStageInfo.cuEnableMask;
        dynamic.spiShaderPgmRsrc4Hs.gfx10Plus.CU_EN =
            Device::AdjustCuEnHi(dynamic.spiShaderPgmRsrc4Hs.gfx10Plus.CU_EN, hsStageInfo.cuEnableMask);
    }

    pCmdSpace = pCmdStream->WriteSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_HS,
                                                  dynamic.spiShaderPgmRsrc3Hs.u32All,
                                                  ShaderGraphics,
                                                  index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                  pCmdSpace);

    if (IsGfx10Plus(chipProps.gfxLevel))
    {
        pCmdSpace = pCmdStream->WriteSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC4_HS,
                                                      dynamic.spiShaderPgmRsrc4Hs.u32All,
                                                      ShaderGraphics,
                                                      index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
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

// Instantiate template versions for the linker.
template
uint32* PipelineChunkHs::WriteShCommands<false>(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& hsStageInfo
    ) const;
template
uint32* PipelineChunkHs::WriteShCommands<true>(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& hsStageInfo
    ) const;

// =====================================================================================================================
// Copies this pipeline chunk's context commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
template <bool UseLoadIndexPath>
uint32* PipelineChunkHs::WriteContextCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // NOTE: It is expected that this function will only ever be called when the set path is in use.
    PAL_ASSERT(UseLoadIndexPath == false);

    return pCmdStream->WriteSetSeqContextRegs(mmVGT_HOS_MAX_TESS_LEVEL,
                                              mmVGT_HOS_MIN_TESS_LEVEL,
                                              &m_regs.context.vgtHosMaxTessLevel,
                                              pCmdSpace);
}

// Instantiate template versions for the linker.
template
uint32* PipelineChunkHs::WriteContextCommands<false>(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const;
template
uint32* PipelineChunkHs::WriteContextCommands<true>(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const;

} // Gfx9
} // Pal
