/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6GraphicsPipeline.h"
#include "core/hw/gfxip/gfx6/gfx6PipelineChunkLsHs.h"
#include "palPipelineAbiProcessorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// Base count of SH registers which are loaded using LOAD_SH_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedShRegCount =
    1 + // mmSPI_SHADER_PGM_LO_LS
    1 + // mmSPI_SHADER_PGM_HI_LS
    1 + // mmSPI_SHADER_PGM_RSRC1_LS
    1 + // mmSPI_SHADER_PGM_RSRC2_LS
    1 + // mmSPI_SHADER_USER_DATA_LS_0 + ConstBufTblStartReg
    1 + // mmSPI_SHADER_PGM_LO_HS
    1 + // mmSPI_SHADER_PGM_HI_HS
    1 + // mmSPI_SHADER_PGM_RSRC1_HS
    1 + // mmSPI_SHADER_PGM_RSRC2_HS
    1;  // mmSPI_SHADER_USER_DATA_HS_0 + ConstBufTblStartReg

// Base count of Context registers which are loaded using LOAD_CNTX_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedCntxRegCount =
    1 + // mmVGT_HOS_MAX_TESS_LEVEL
    1;  // mmVGT_HOS_MIN_TESS_LEVEL

// =====================================================================================================================
PipelineChunkLsHs::PipelineChunkLsHs(
    const Device&       device,
    const PerfDataInfo* pLsPerfDataInfo,
    const PerfDataInfo* pHsPerfDataInfo)
    :
    m_device(device),
    m_pLsPerfDataInfo(pLsPerfDataInfo),
    m_pHsPerfDataInfo(pHsPerfDataInfo)
{
    memset(&m_commands, 0, sizeof(m_commands));
    memset(&m_stageInfoLs, 0, sizeof(m_stageInfoLs));
    memset(&m_stageInfoHs, 0, sizeof(m_stageInfoHs));

    m_stageInfoLs.stageId = Abi::HardwareStage::Ls;
    m_stageInfoHs.stageId = Abi::HardwareStage::Hs;
}

// =====================================================================================================================
// Early initialization for this pipeline chunk.  Responsible for determining the number of SH and context registers to
// be loaded using LOAD_CNTX_REG_INDEX and LOAD_SH_REG_INDEX.
void PipelineChunkLsHs::EarlyInit(
    GraphicsPipelineLoadInfo* pInfo)
{
    PAL_ASSERT(pInfo != nullptr);

    const Gfx6PalSettings& settings = m_device.Settings();
    if (settings.enableLoadIndexForObjectBinds != false)
    {
        pInfo->loadedCtxRegCount += BaseLoadedCntxRegCount;
        pInfo->loadedShRegCount  += BaseLoadedShRegCount;
    }
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.  Also uploads register state into GPU memory.
void PipelineChunkLsHs::LateInit(
    const AbiProcessor&             abiProcessor,
    const RegisterVector&           registers,
    GraphicsPipelineUploader*       pUploader,
    const GraphicsPipelineLoadInfo& loadInfo,
    MetroHash64*                    pHasher)
{
    const bool useLoadIndexPath = pUploader->EnableLoadIndexPath();

    const Gfx6PalSettings&   settings  = m_device.Settings();
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    BuildPm4Headers(useLoadIndexPath);

    Abi::PipelineSymbolEntry symbol = {};
    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::LsMainEntry, &symbol))
    {
        m_stageInfoLs.codeLength   = static_cast<size_t>(symbol.size);
        const gpusize programGpuVa = (pUploader->CodeGpuVirtAddr() + symbol.value);
        PAL_ASSERT(programGpuVa == Pow2Align(programGpuVa, 256));

        m_commands.sh.spiShaderPgmLoLs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_commands.sh.spiShaderPgmHiLs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::LsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (pUploader->DataGpuVirtAddr() + symbol.value);
        m_commands.sh.spiShaderUserDataLoLs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::LsDisassembly, &symbol))
    {
        m_stageInfoLs.disassemblyLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::HsMainEntry, &symbol))
    {
        m_stageInfoHs.codeLength   = static_cast<size_t>(symbol.size);
        const gpusize programGpuVa = (pUploader->CodeGpuVirtAddr() + symbol.value);
        PAL_ASSERT(programGpuVa == Pow2Align(programGpuVa, 256));

        m_commands.sh.spiShaderPgmLoHs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_commands.sh.spiShaderPgmHiHs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::HsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (pUploader->DataGpuVirtAddr() + symbol.value);
        m_commands.sh.spiShaderUserDataLoHs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::HsDisassembly, &symbol))
    {
        m_stageInfoHs.disassemblyLength = static_cast<size_t>(symbol.size);
    }

    m_commands.sh.spiShaderPgmRsrc1Ls.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_LS);
    m_commands.sh.spiShaderPgmRsrc2Ls.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_LS);
    registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_LS__CI__VI, &m_commands.dynamic.spiShaderPgmRsrc3Ls.u32All);

    m_commands.sh.spiShaderPgmRsrc1Hs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_HS);
    m_commands.sh.spiShaderPgmRsrc2Hs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_HS);
    registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_HS__CI__VI, &m_commands.dynamic.spiShaderPgmRsrc3Hs.u32All);

    m_commands.context.vgtHosMinTessLevel.u32All = registers.At(mmVGT_HOS_MIN_TESS_LEVEL);
    m_commands.context.vgtHosMaxTessLevel.u32All = registers.At(mmVGT_HOS_MAX_TESS_LEVEL);

    if (chipProps.gfxLevel >= GfxIpLevel::GfxIp7)
    {
        uint16 lsCuDisableMask = 0;
        if (loadInfo.usesOnchipTess                               &&
            ((loadInfo.usesGs == false) || loadInfo.usesOnChipGs) &&
            (m_device.LateAllocVsLimit() > 0))
        {
            // If we're using on-chip tessellation, we need to avoid using CU1 for LS/HS waves to avoid a deadlock with
            // the PS. When on-chip tessellation is enabled, all of the tessellation stages (LS, HS, VS) are run on the
            // same CU because communication between the stages are done via LDS.
            // This is a cause for deadlocks because when the HW-VS waves are trying to export, they are waiting for space
            // in the parameter cache, but that space is claimed by pending PS waves that can't launch on the CU due to
            // lack of space (already existing waves).

            // Disable virtualized CU #1 instead of #0 because thread traces use CU #0 by default.
            lsCuDisableMask = 0x2;
        }

        m_commands.dynamic.spiShaderPgmRsrc3Ls.bits.CU_EN =
            m_device.GetCuEnableMask(lsCuDisableMask, settings.lsCuEnLimitMask);
        // NOTE: There is no CU enable mask for the HS stage, because the HS wavefronts are tied to the CU which
        // executes the LS wavefront(s) beforehand.
    }

    pHasher->Update(m_commands.context);

    if (m_device.WaShaderSpiWriteShaderPgmRsrc2Ls())
    {
        // See BuildPm4Headers for more information about WaShaderSpiWriteShaderPgmRsrc2Ls.  This workaround is
        // incompatible with the LOAD_INDEX path.
        PAL_ASSERT(useLoadIndexPath == false);

        m_commands.sh.spiBug.spiShaderPgmRsrc1Ls = m_commands.sh.spiShaderPgmRsrc1Ls;
        m_commands.sh.spiBug.spiShaderPgmRsrc2Ls = m_commands.sh.spiShaderPgmRsrc2Ls;
    }
    else if (useLoadIndexPath)
    {
        pUploader->AddShReg(mmSPI_SHADER_PGM_LO_LS, m_commands.sh.spiShaderPgmLoLs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_HI_LS, m_commands.sh.spiShaderPgmHiLs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_LO_HS, m_commands.sh.spiShaderPgmLoHs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_HI_HS, m_commands.sh.spiShaderPgmHiHs);

        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC1_LS, m_commands.sh.spiShaderPgmRsrc1Ls);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC2_LS, m_commands.sh.spiShaderPgmRsrc2Ls);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC1_HS, m_commands.sh.spiShaderPgmRsrc1Hs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC2_HS, m_commands.sh.spiShaderPgmRsrc2Hs);

        pUploader->AddShReg(mmSPI_SHADER_USER_DATA_LS_0 + ConstBufTblStartReg, m_commands.sh.spiShaderUserDataLoLs);
        pUploader->AddShReg(mmSPI_SHADER_USER_DATA_HS_0 + ConstBufTblStartReg, m_commands.sh.spiShaderUserDataLoHs);

        pUploader->AddCtxReg(mmVGT_HOS_MIN_TESS_LEVEL, m_commands.context.vgtHosMinTessLevel);
        pUploader->AddCtxReg(mmVGT_HOS_MAX_TESS_LEVEL, m_commands.context.vgtHosMaxTessLevel);
    }
}

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
template <bool UseLoadIndexPath>
uint32* PipelineChunkLsHs::WriteShCommands(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& lsStageInfo,
    const DynamicStageInfo& hsStageInfo
    ) const
{
    if (UseLoadIndexPath == false)
    {
        pCmdSpace = pCmdStream->WritePm4Image(m_commands.sh.spaceNeeded, &m_commands.sh, pCmdSpace);
    }

    // NOTE: The dynamic register PM4 image headers will be zero if the GPU doesn't support these registers.
    if (m_commands.dynamic.hdrPgmRsrc3Ls.header.u32All != 0)
    {
        auto dynamicCmds = m_commands.dynamic;

        if (lsStageInfo.wavesPerSh > 0)
        {
            dynamicCmds.spiShaderPgmRsrc3Ls.bits.WAVE_LIMIT = lsStageInfo.wavesPerSh;
        }
        if (hsStageInfo.wavesPerSh > 0)
        {
            dynamicCmds.spiShaderPgmRsrc3Hs.bits.WAVE_LIMIT = hsStageInfo.wavesPerSh;
        }

        if (lsStageInfo.cuEnableMask != 0)
        {
            dynamicCmds.spiShaderPgmRsrc3Ls.bits.CU_EN &= lsStageInfo.cuEnableMask;
        }
        // NOTE: There is no CU enable mask for the HS stage!

        constexpr uint32 SpaceNeededDynamic = sizeof(m_commands.dynamic) / sizeof(uint32);
        pCmdSpace = pCmdStream->WritePm4Image(SpaceNeededDynamic, &dynamicCmds, pCmdSpace);
    }

    if (m_pLsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_pLsPerfDataInfo->regOffset,
                                                                 m_pLsPerfDataInfo->gpuVirtAddr,
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
uint32* PipelineChunkLsHs::WriteShCommands<false>(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& lsStageInfo,
    const DynamicStageInfo& hsStageInfo
    ) const;
template
uint32* PipelineChunkLsHs::WriteShCommands<true>(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& lsStageInfo,
    const DynamicStageInfo& hsStageInfo
    ) const;

// =====================================================================================================================
// Copies this pipeline chunk's context commands into the specified command space. Returns the next unused
// DWORD in pCmdSpace.
template <bool UseLoadIndexPath>
uint32* PipelineChunkLsHs::WriteContextCommands(
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
uint32* PipelineChunkLsHs::WriteContextCommands<false>(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const;
template
uint32* PipelineChunkLsHs::WriteContextCommands<true>(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const;

// =====================================================================================================================
// Assembles the PM4 headers for the commands in this Pipeline chunk.
void PipelineChunkLsHs::BuildPm4Headers(
    bool enableLoadIndexPath)
{
    const CmdUtil& cmdUtil = m_device.CmdUtil();

    m_commands.sh.spaceNeeded = cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_LO_LS,
                                                          mmSPI_SHADER_PGM_RSRC2_LS,
                                                          ShaderGraphics,
                                                          &m_commands.sh.hdrSpiShaderPgmLs);

    m_commands.sh.spaceNeeded += cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_LS_0 + ConstBufTblStartReg,
                                                          ShaderGraphics,
                                                          &m_commands.sh.hdrSpiShaderUserDataLs);

    m_commands.sh.spaceNeeded += cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_LO_HS,
                                                           mmSPI_SHADER_PGM_RSRC2_HS,
                                                           ShaderGraphics,
                                                           &m_commands.sh.hdrSpiShaderPgmHs);

    m_commands.sh.spaceNeeded += cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_HS_0 + ConstBufTblStartReg,
                                                          ShaderGraphics,
                                                          &m_commands.sh.hdrSpiShaderUserDataHs);

    cmdUtil.BuildSetSeqContextRegs(mmVGT_HOS_MAX_TESS_LEVEL,
                                   mmVGT_HOS_MIN_TESS_LEVEL,
                                   &m_commands.context.hdrVgtHosTessLevel);

    // Build the PM4 image used in the workaround for the WaShaderSpiWriteShaderPgmRsrc2Ls hardware bug:
    //
    // Some GFX7 hardware has a bug where writes to the SPI_SHADER_PGM_RSRC2_LS register can be dropped if the LS
    // stage's SP persistent state FIFO is full.  This allows incorrect values of the LDS_SIZE and/or USER_SGPR fields
    // to be read when launching LS waves, which can cause geometry corruption when tessellation is active.
    //
    // The workaround proposed by the HW team and implemented is to write this register twice, with a dummy write
    // to another register in-between the duplicate writes.  This dummy write can be to any SH register in the range
    // between SPI_SHADER_TBA_LO_LS and SPI_SHADER_USER_DATA_LS_15.  The workaround works because the SPI will see the
    // write to the other register and correctly stall when the LS persistent-state FIFO is full.  The 2nd write to
    // SPI_SHADER_PGM_RSRC2_LS will then be correctly handled by the SPI.
    //
    // The dummy write we are choosing to do is to the SPI_SHADER_PGM_RSRC1_LS register.
    if (m_device.WaShaderSpiWriteShaderPgmRsrc2Ls())
    {
        // This workaround is incompatible with the LOAD_INDEX path!
        PAL_ASSERT(enableLoadIndexPath == false);

        m_commands.sh.spaceNeeded += cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_RSRC1_LS,
                                                               mmSPI_SHADER_PGM_RSRC2_LS,
                                                               ShaderGraphics,
                                                               &m_commands.sh.spiBug.hdrSpiShaderPgmRsrcLs);
    }

    if (m_device.Parent()->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp7)
    {
        cmdUtil.BuildSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_LS__CI__VI,
                                      ShaderGraphics,
                                      SET_SH_REG_INDEX_CP_MODIFY_CU_MASK,
                                      &m_commands.dynamic.hdrPgmRsrc3Ls);

        cmdUtil.BuildSetOneShReg(mmSPI_SHADER_PGM_RSRC3_HS__CI__VI,
                                 ShaderGraphics,
                                 &m_commands.dynamic.hdrPgmRsrc3Hs);
    }
}

} // Gfx6
} // Pal
