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
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkHs.h"
#include "palPipelineAbiProcessorImpl.h"

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
    memset(&m_commands, 0, sizeof(m_commands));
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
        pInfo->loadedShRegCount  += (BaseLoadedShRegCount + chipProps.gfx9.supportSpp);

    }
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.  Also uploads register state into GPU memory.
void PipelineChunkHs::LateInit(
    const AbiProcessor&       abiProcessor,
    const RegisterVector&     registers,
    GraphicsPipelineUploader* pUploader,
    Util::MetroHash64*        pHasher)
{
    const bool useLoadIndexPath = pUploader->EnableLoadIndexPath();

    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    const uint16 baseUserDataHs     = m_device.GetBaseUserDataReg(HwShaderStage::Hs);
    const uint16 mmSpiShaderPgmLoLs = m_device.CmdUtil().GetRegInfo().mmSpiShaderPgmLoLs;

    BuildPm4Headers(useLoadIndexPath);

    Abi::PipelineSymbolEntry symbol = { };
    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::HsMainEntry, &symbol))
    {
        m_stageInfo.codeLength     = static_cast<size_t>(symbol.size);
        const gpusize programGpuVa = (pUploader->CodeGpuVirtAddr() + symbol.value);
        PAL_ASSERT(IsPow2Aligned(programGpuVa, 256));

        m_commands.sh.spiShaderPgmLoLs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_commands.sh.spiShaderPgmHiLs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);
    }

    regSPI_SHADER_USER_DATA_LS_0 spiShaderUserDataLoHs = { };
    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::HsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (pUploader->DataGpuVirtAddr() + symbol.value);
        m_commands.sh.spiShaderUserDataLoHs = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::HsDisassembly, &symbol))
    {
        m_stageInfo.disassemblyLength = static_cast<size_t>(symbol.size);
    }

    m_commands.sh.spiShaderPgmRsrc1Hs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_HS);
    m_commands.sh.spiShaderPgmRsrc2Hs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_HS);
    registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_HS, &m_commands.dynamic.spiShaderPgmRsrc3Hs.u32All);

    // NOTE: The Pipeline ABI doesn't specify CU enable masks for each shader stage, so it should be safe to
    // always use the ones PAL prefers.
    m_commands.dynamic.spiShaderPgmRsrc3Hs.bits.CU_EN = m_device.GetCuEnableMask(0, UINT_MAX);

    if (chipProps.gfx9.supportSpp != 0)
    {
        registers.HasEntry(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_HS, &m_commands.sh.spiShaderPgmChksumHs.u32All);
    }

    m_commands.context.vgtHosMinTessLevel.u32All = registers.At(mmVGT_HOS_MIN_TESS_LEVEL);
    m_commands.context.vgtHosMaxTessLevel.u32All = registers.At(mmVGT_HOS_MAX_TESS_LEVEL);

    pHasher->Update(m_commands.context);

    if (useLoadIndexPath)
    {
        pUploader->AddShReg(mmSpiShaderPgmLoLs,        m_commands.sh.spiShaderPgmLoLs);
        pUploader->AddShReg(mmSpiShaderPgmLoLs + 1,    m_commands.sh.spiShaderPgmHiLs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC1_HS, m_commands.sh.spiShaderPgmRsrc1Hs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC2_HS, m_commands.sh.spiShaderPgmRsrc2Hs);

        pUploader->AddShReg(baseUserDataHs + ConstBufTblStartReg, m_commands.sh.spiShaderUserDataLoHs);

        if (chipProps.gfx9.supportSpp != 0)
        {
            pUploader->AddShReg(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_HS, m_commands.sh.spiShaderPgmChksumHs);
        }

        pUploader->AddCtxReg(mmVGT_HOS_MIN_TESS_LEVEL, m_commands.context.vgtHosMinTessLevel);
        pUploader->AddCtxReg(mmVGT_HOS_MAX_TESS_LEVEL, m_commands.context.vgtHosMaxTessLevel);
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
    if (UseLoadIndexPath == false)
    {
        pCmdSpace = pCmdStream->WritePm4Image(m_commands.sh.spaceNeeded, &m_commands.sh, pCmdSpace);
    }

    auto dynamicCmds = m_commands.dynamic;

    if (hsStageInfo.wavesPerSh > 0)
    {
        dynamicCmds.spiShaderPgmRsrc3Hs.bits.WAVE_LIMIT = hsStageInfo.wavesPerSh;
    }

    if (hsStageInfo.cuEnableMask != 0)
    {
        dynamicCmds.spiShaderPgmRsrc3Hs.bits.CU_EN &= hsStageInfo.cuEnableMask;
    }

    PAL_ASSERT(m_commands.dynamic.spaceNeeded != 0);
    pCmdSpace = pCmdStream->WritePm4Image(m_commands.dynamic.spaceNeeded, &dynamicCmds, pCmdSpace);

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

    constexpr uint32 SpaceNeeded = sizeof(m_commands.context) / sizeof(uint32);
    return pCmdStream->WritePm4Image(SpaceNeeded, &m_commands.context, pCmdSpace);
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

// =====================================================================================================================
// Assembles the PM4 headers for the commands in this Pipeline chunk.
void PipelineChunkHs::BuildPm4Headers(
    bool enableLoadIndexPath)
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();
    const CmdUtil&           cmdUtil   = m_device.CmdUtil();

    const uint16 baseUserDataHs     = m_device.GetBaseUserDataReg(HwShaderStage::Hs);
    const uint32 mmSpiShaderPgmLoLs = cmdUtil.GetRegInfo().mmSpiShaderPgmLoLs;

    m_commands.sh.spaceNeeded = cmdUtil.BuildSetSeqShRegs(mmSpiShaderPgmLoLs,
                                                          mmSpiShaderPgmLoLs + 1,
                                                          ShaderGraphics,
                                                          &m_commands.sh.hdrSpiShaderPgmHs);

    m_commands.sh.spaceNeeded += cmdUtil.BuildSetOneShReg(baseUserDataHs + ConstBufTblStartReg,
                                                          ShaderGraphics,
                                                          &m_commands.sh.hdrSpiShaderUserDataHs);

    m_commands.sh.spaceNeeded += cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_RSRC1_HS,
                                                           mmSPI_SHADER_PGM_RSRC2_HS,
                                                           ShaderGraphics,
                                                           &m_commands.sh.hdrSpiShaderPgmRsrcHs);

    if (chipProps.gfx9.supportSpp != 0)
    {
        m_commands.sh.spaceNeeded += cmdUtil.BuildSetOneShReg(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_HS,
                                                              ShaderGraphics,
                                                              &m_commands.sh.hdrSpiShaderPgmChksum);
    }
    else
    {
        m_commands.sh.spaceNeeded += cmdUtil.BuildNop(CmdUtil::ShRegSizeDwords + 1,
                                                      &m_commands.sh.hdrSpiShaderPgmChksum);
    }

    cmdUtil.BuildSetSeqContextRegs(mmVGT_HOS_MAX_TESS_LEVEL,
                                   mmVGT_HOS_MIN_TESS_LEVEL,
                                   &m_commands.context.hdrvVgtHosTessLevel);

    // NOTE: Supporting real-time compute requires use of SET_SH_REG_INDEX for this register.
    m_commands.dynamic.spaceNeeded = cmdUtil.BuildSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_HS,
                                                                   ShaderGraphics,
                                                                   index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                                   &m_commands.dynamic.hdrPgmRsrc3Hs);

}

} // Gfx9
} // Pal
