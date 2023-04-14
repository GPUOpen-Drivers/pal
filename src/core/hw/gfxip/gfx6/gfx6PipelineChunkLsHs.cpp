/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
PipelineChunkLsHs::PipelineChunkLsHs(
    const Device&       device,
    const PerfDataInfo* pLsPerfDataInfo,
    const PerfDataInfo* pHsPerfDataInfo)
    :
    m_device(device),
    m_regs{},
    m_pLsPerfDataInfo(pLsPerfDataInfo),
    m_pHsPerfDataInfo(pHsPerfDataInfo),
    m_stageInfoLs{},
    m_stageInfoHs{}
{
    m_stageInfoLs.stageId = Abi::HardwareStage::Ls;
    m_stageInfoHs.stageId = Abi::HardwareStage::Hs;
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.  Also uploads register state into GPU memory.
void PipelineChunkLsHs::LateInit(
    const AbiReader&                abiReader,
    const RegisterVector&           registers,
    PipelineUploader*               pUploader,
    const GraphicsPipelineLoadInfo& loadInfo,
    MetroHash64*                    pHasher)
{
    const Gfx6PalSettings&   settings  = m_device.Settings();
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    GpuSymbol symbol = {};
    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::LsMainEntry, &symbol) == Result::Success)
    {
        m_stageInfoLs.codeLength   = static_cast<size_t>(symbol.size);
        PAL_ASSERT(symbol.gpuVirtAddr == Pow2Align(symbol.gpuVirtAddr, 256));

        m_regs.sh.spiShaderPgmLoLs.bits.MEM_BASE = Get256BAddrLo(symbol.gpuVirtAddr);
        m_regs.sh.spiShaderPgmHiLs.bits.MEM_BASE = Get256BAddrHi(symbol.gpuVirtAddr);
    }

    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::LsShdrIntrlTblPtr, &symbol) == Result::Success)
    {
        m_regs.sh.userDataInternalTableLs.bits.DATA = LowPart(symbol.gpuVirtAddr);
    }

    const Elf::SymbolTableEntry* pElfSymbol = abiReader.GetPipelineSymbol(Abi::PipelineSymbolType::LsDisassembly);
    if (pElfSymbol != nullptr)
    {
        m_stageInfoLs.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
    }

    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::HsMainEntry, &symbol) == Result::Success)
    {
        m_stageInfoHs.codeLength   = static_cast<size_t>(symbol.size);
        PAL_ASSERT(symbol.gpuVirtAddr == Pow2Align(symbol.gpuVirtAddr, 256));

        m_regs.sh.spiShaderPgmLoHs.bits.MEM_BASE = Get256BAddrLo(symbol.gpuVirtAddr);
        m_regs.sh.spiShaderPgmHiHs.bits.MEM_BASE = Get256BAddrHi(symbol.gpuVirtAddr);
    }

    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::HsShdrIntrlTblPtr, &symbol) == Result::Success)
    {
        m_regs.sh.userDataInternalTableHs.bits.DATA = LowPart(symbol.gpuVirtAddr);
    }

    pElfSymbol = abiReader.GetPipelineSymbol(Abi::PipelineSymbolType::HsDisassembly);
    if (pElfSymbol != nullptr)
    {
        m_stageInfoHs.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
    }

    m_regs.sh.spiShaderPgmRsrc1Ls.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_LS);
    m_regs.sh.spiShaderPgmRsrc2Ls.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_LS);
    registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_LS__CI__VI, &m_regs.dynamic.spiShaderPgmRsrc3Ls.u32All);

    m_regs.sh.spiShaderPgmRsrc1Hs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_HS);
    m_regs.sh.spiShaderPgmRsrc2Hs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_HS);
    registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_HS__CI__VI, &m_regs.dynamic.spiShaderPgmRsrc3Hs.u32All);

    m_regs.context.vgtHosMinTessLevel.u32All = registers.At(mmVGT_HOS_MIN_TESS_LEVEL);
    m_regs.context.vgtHosMaxTessLevel.u32All = registers.At(mmVGT_HOS_MAX_TESS_LEVEL);

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

        m_regs.dynamic.spiShaderPgmRsrc3Ls.bits.CU_EN =
            m_device.GetCuEnableMask(lsCuDisableMask, settings.lsCuEnLimitMask);
        // NOTE: There is no CU enable mask for the HS stage, because the HS wavefronts are tied to the CU which
        // executes the LS wavefront(s) beforehand.
    }

    pHasher->Update(m_regs.context);
}

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkLsHs::WriteShCommands(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& lsStageInfo,
    const DynamicStageInfo& hsStageInfo
    ) const
{
    pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmSPI_SHADER_PGM_LO_LS,
                                                mmSPI_SHADER_PGM_RSRC2_LS,
                                                ShaderGraphics,
                                                &m_regs.sh.spiShaderPgmLoLs,
                                                pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmSPI_SHADER_PGM_LO_HS,
                                                mmSPI_SHADER_PGM_RSRC2_HS,
                                                ShaderGraphics,
                                                &m_regs.sh.spiShaderPgmLoHs,
                                                pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(mmSPI_SHADER_USER_DATA_LS_0 + ConstBufTblStartReg,
                                                                m_regs.sh.userDataInternalTableLs.u32All,
                                                                pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(mmSPI_SHADER_USER_DATA_HS_0 + ConstBufTblStartReg,
                                                                m_regs.sh.userDataInternalTableHs.u32All,
                                                                pCmdSpace);

    // Some GFX7 hardware has a bug where writes to the SPI_SHADER_PGM_RSRC2_LS register can be dropped if the
    // LS stage's SP persistent state FIFO is full.  This allows incorrect values of the LDS_SIZE and/or USER_SGPR
    // fields to be read when launching LS waves, which can cause geometry corruption when tessellation is active.
    //
    // The workaround proposed by the HW team and implemented is to write this register twice, with a dummy write
    // to another register in-between the duplicate writes.  This dummy write can be to any SH register in the
    // range between SPI_SHADER_TBA_LO_LS and SPI_SHADER_USER_DATA_LS_15.  The workaround works because the SPI
    // will see the write to the other register and correctly stall when the LS persistent-state FIFO is full.
    // The 2nd write to SPI_SHADER_PGM_RSRC2_LS will then be correctly handled by the SPI.
    //
    // The dummy write we are choosing to do is to the SPI_SHADER_PGM_RSRC1_LS register.
    if (m_device.WaShaderSpiWriteShaderPgmRsrc2Ls())
    {
        pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmSPI_SHADER_PGM_RSRC1_LS,
                                                    mmSPI_SHADER_PGM_RSRC2_LS,
                                                    ShaderGraphics,
                                                    &m_regs.sh.spiShaderPgmRsrc1Ls,
                                                    pCmdSpace);
    }

    // The "dynamic" registers don't exist on Gfx6.
    if (m_device.CmdUtil().IpLevel() >= GfxIpLevel::GfxIp7)
    {
        auto dynamic = m_regs.dynamic;

        if (lsStageInfo.wavesPerSh > 0)
        {
            dynamic.spiShaderPgmRsrc3Ls.bits.WAVE_LIMIT = lsStageInfo.wavesPerSh;
        }
        if (hsStageInfo.wavesPerSh > 0)
        {
            dynamic.spiShaderPgmRsrc3Hs.bits.WAVE_LIMIT = hsStageInfo.wavesPerSh;
        }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 789
        if (lsStageInfo.cuEnableMask != 0)
        {
            dynamic.spiShaderPgmRsrc3Ls.bits.CU_EN &= lsStageInfo.cuEnableMask;
        }
        // NOTE: There is no CU enable mask for the HS stage!
#endif

        pCmdSpace = pCmdStream->WriteSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_LS__CI__VI,
                                                      dynamic.spiShaderPgmRsrc3Ls.u32All,
                                                      ShaderGraphics,
                                                      SET_SH_REG_INDEX_CP_MODIFY_CU_MASK,
                                                      pCmdSpace);
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(mmSPI_SHADER_PGM_RSRC3_HS__CI__VI,
                                                                 dynamic.spiShaderPgmRsrc3Hs.u32All,
                                                                 pCmdSpace);
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

// =====================================================================================================================
// Copies this pipeline chunk's context commands into the specified command space. Returns the next unused
// DWORD in pCmdSpace.
uint32* PipelineChunkLsHs::WriteContextCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    return pCmdStream->WriteSetSeqContextRegs(mmVGT_HOS_MAX_TESS_LEVEL,
                                              mmVGT_HOS_MIN_TESS_LEVEL,
                                              &m_regs.context.vgtHosMaxTessLevel,
                                              pCmdSpace);
}

} // Gfx6
} // Pal
