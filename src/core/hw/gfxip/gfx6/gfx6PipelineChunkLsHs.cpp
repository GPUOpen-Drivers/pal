/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6PipelineChunkLsHs.h"
#include "core/platform.h"
#include "palPipeline.h"
#include "palPipelineAbiProcessorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
PipelineChunkLsHs::PipelineChunkLsHs(
    const Device& device)
    :
    m_device(device),
    m_pLsPerfDataInfo(nullptr),
    m_pHsPerfDataInfo(nullptr)
{
    memset(&m_pm4ImageSh,        0, sizeof(m_pm4ImageSh));
    memset(&m_pm4ImageShDynamic, 0, sizeof(m_pm4ImageShDynamic));
    memset(&m_pm4ImageContext,   0, sizeof(m_pm4ImageContext));
    memset(&m_stageInfoLs,       0, sizeof(m_stageInfoLs));
    memset(&m_stageInfoHs,       0, sizeof(m_stageInfoHs));

    m_stageInfoLs.stageId = Abi::HardwareStage::Ls;
    m_stageInfoHs.stageId = Abi::HardwareStage::Hs;
}

// =====================================================================================================================
// Initializes this pipeline chunk using RelocatableShader objects representing the LS & HS hardware stages.
void PipelineChunkLsHs::Init(
    const AbiProcessor& abiProcessor,
    const LsHsParams&   params)
{
    const Gfx6PalSettings&   settings = m_device.Settings();
    const GpuChipProperties& chipInfo = m_device.Parent()->ChipProperties();

    m_pLsPerfDataInfo = params.pLsPerfDataInfo;
    m_pHsPerfDataInfo = params.pHsPerfDataInfo;

    BuildPm4Headers();

    m_pm4ImageSh.spiShaderPgmRsrc1Ls.u32All     = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC1_LS);
    m_pm4ImageSh.spiShaderPgmRsrc2Ls.u32All     = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC2_LS);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 345
    abiProcessor.HasRegisterEntry(mmSPI_SHADER_PGM_RSRC3_LS__CI__VI, &m_pm4ImageShDynamic.spiShaderPgmRsrc3Ls.u32All);
#endif

    m_pm4ImageSh.spiShaderPgmRsrc1Hs.u32All     = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC1_HS);
    m_pm4ImageSh.spiShaderPgmRsrc2Hs.u32All     = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC2_HS);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 345
    abiProcessor.HasRegisterEntry(mmSPI_SHADER_PGM_RSRC3_HS__CI__VI, &m_pm4ImageShDynamic.spiShaderPgmRsrc3Hs.u32All);
#endif

    m_pm4ImageContext.vgtHosMinTessLevel.u32All = abiProcessor.GetRegisterEntry(mmVGT_HOS_MIN_TESS_LEVEL);
    m_pm4ImageContext.vgtHosMaxTessLevel.u32All = abiProcessor.GetRegisterEntry(mmVGT_HOS_MAX_TESS_LEVEL);

    // Set up the register values written for the WaShaderSpiWriteShaderPgmRsrc2Ls hardware bug workaround.  See
    // BuildPm4Headers() for more info.
    if (m_device.WaShaderSpiWriteShaderPgmRsrc2Ls())
    {
        m_pm4ImageSh.spiBug.spiShaderPgmRsrc1Ls.u32All = m_pm4ImageSh.spiShaderPgmRsrc1Ls.u32All;
        m_pm4ImageSh.spiBug.spiShaderPgmRsrc2Ls.u32All = m_pm4ImageSh.spiShaderPgmRsrc2Ls.u32All;
    }

    if (chipInfo.gfxLevel >= GfxIpLevel::GfxIp7)
    {
        uint16 lsCuDisableMask = 0;
        if (m_device.LateAllocVsLimit() > 0)
        {

            // Disable virtualized CU #1 instead of #0 because thread traces use CU #0 by default.
            lsCuDisableMask = 0x2;
        }

        m_pm4ImageShDynamic.spiShaderPgmRsrc3Ls.bits.CU_EN =
            m_device.GetCuEnableMask(lsCuDisableMask, settings.lsCuEnLimitMask);
        // NOTE: There is no CU enable mask for the HS stage, because the HS wavefronts are tied to the CU which
        // executes the LS wavefront(s) beforehand.
    }

    // Compute the checksum here because we don't want it to include the GPU virtual addresses!
    params.pHasher->Update(m_pm4ImageContext);

    Abi::PipelineSymbolEntry symbol = { };
    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::LsMainEntry, &symbol))
    {
        const gpusize programGpuVa = (symbol.value + params.codeGpuVirtAddr);
        PAL_ASSERT(programGpuVa == Pow2Align(programGpuVa, 256));

        m_pm4ImageSh.spiShaderPgmLoLs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_pm4ImageSh.spiShaderPgmHiLs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);

        m_stageInfoLs.codeLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::LsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (symbol.value + params.dataGpuVirtAddr);
        m_pm4ImageSh.spiShaderUserDataLoLs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::LsDisassembly, &symbol))
    {
        m_stageInfoLs.disassemblyLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::HsMainEntry, &symbol))
    {
        const gpusize programGpuVa = (symbol.value + params.codeGpuVirtAddr);
        PAL_ASSERT(programGpuVa == Pow2Align(programGpuVa, 256));

        m_pm4ImageSh.spiShaderPgmLoHs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_pm4ImageSh.spiShaderPgmHiHs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);

        m_stageInfoHs.codeLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::HsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (symbol.value + params.dataGpuVirtAddr);
        m_pm4ImageSh.spiShaderUserDataLoHs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::HsDisassembly, &symbol))
    {
        m_stageInfoHs.disassemblyLength = static_cast<size_t>(symbol.size);
    }
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
    pCmdSpace = pCmdStream->WritePm4Image(m_pm4ImageSh.spaceNeeded, &m_pm4ImageSh, pCmdSpace);

    if (m_pm4ImageShDynamic.spaceNeeded > 0)
    {
        Pm4ImageShDynamic pm4ImageShDynamic = m_pm4ImageShDynamic;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 345
        if (pm4ImageShDynamic.spiShaderPgmRsrc3Ls.bits.WAVE_LIMIT == 0)
#endif
        {
            pm4ImageShDynamic.spiShaderPgmRsrc3Ls.bits.WAVE_LIMIT = lsStageInfo.wavesPerSh;
            pm4ImageShDynamic.spiShaderPgmRsrc3Hs.bits.WAVE_LIMIT = hsStageInfo.wavesPerSh;
        }

        if (lsStageInfo.cuEnableMask != 0)
        {
            pm4ImageShDynamic.spiShaderPgmRsrc3Ls.bits.CU_EN &= lsStageInfo.cuEnableMask;
        }
        // NOTE: There is no CU enable mask for the HS stage

        pCmdSpace = pCmdStream->WritePm4Image(pm4ImageShDynamic.spaceNeeded, &pm4ImageShDynamic, pCmdSpace);
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
    pCmdSpace = pCmdStream->WritePm4Image(m_pm4ImageContext.spaceNeeded, &m_pm4ImageContext, pCmdSpace);
    return pCmdSpace;
}

// =====================================================================================================================
// Assembles the PM4 headers for the commands in this Pipeline chunk.
void PipelineChunkLsHs::BuildPm4Headers()
{
    const CmdUtil& cmdUtil = m_device.CmdUtil();

    // Sets the following SH register: SPI_SHADER_USER_DATA_LS_1.
    m_pm4ImageSh.spaceNeeded = cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_LS_0 + ConstBufTblStartReg,
                                                        ShaderGraphics,
                                                        &m_pm4ImageSh.hdrSpiShaderUserDataLs);

    // Sets the following SH registers: SPI_SHADER_PGM_LO_HS, SPI_SHADER_PGM_HI_HS,
    // SPI_SHADER_PGM_RSRC1_HS, SPI_SHADER_PGM_RSRC2_HS.
    m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_LO_HS,
                                                          mmSPI_SHADER_PGM_RSRC2_HS,
                                                          ShaderGraphics,
                                                          &m_pm4ImageSh.hdrSpiShaderPgmHs);

    // Sets the following SH register: SPI_SHADER_USER_DATA_HS_1.
    m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_HS_0 + ConstBufTblStartReg,
                                                         ShaderGraphics,
                                                         &m_pm4ImageSh.hdrSpiShaderUserDataHs);

    // Sets the following SH registers: SPI_SHADER_PGM_LO_LS, SPI_SHADER_PGM_HI_LS,
    // SPI_SHADER_PGM_RSRC1_LS, SPI_SHADER_PGM_RSRC2_LS.
    m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_LO_LS,
                                                          mmSPI_SHADER_PGM_RSRC2_LS,
                                                          ShaderGraphics,
                                                          &m_pm4ImageSh.hdrSpiShaderPgmLs);

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
        m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_RSRC1_LS,
                                                              mmSPI_SHADER_PGM_RSRC2_LS,
                                                              ShaderGraphics,
                                                              &m_pm4ImageSh.spiBug.hdrSpiShaderPgmRsrcLs);
    }

    if (m_device.Parent()->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp7)
    {
        // Sets the following SH register: SPI_SHADER_PGM_RSRC3_LS.
        // We must use the SET_SH_REG_INDEX packet to support the real-time compute feature.
        m_pm4ImageShDynamic.spaceNeeded = cmdUtil.BuildSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_LS__CI__VI,
                                                                        ShaderGraphics,
                                                                        SET_SH_REG_INDEX_CP_MODIFY_CU_MASK,
                                                                        &m_pm4ImageShDynamic.hdrPgmRsrc3Ls);

        // Sets the following SH register: SPI_SHADER_PGM_RSRC3_HS.
        // It does not have a CU_EN field. This register can be set using the plain SET_SH_REG packet.
        m_pm4ImageShDynamic.spaceNeeded += cmdUtil.BuildSetOneShReg(mmSPI_SHADER_PGM_RSRC3_HS__CI__VI,
                                                                    ShaderGraphics,
                                                                    &m_pm4ImageShDynamic.hdrPgmRsrc3Hs);
    }

    // Sets the following context registers: VGT_HOS_MAX_TESS_LEVEL, VGT_HOS_MIN_TESS_LEVEL.
    m_pm4ImageContext.spaceNeeded = cmdUtil.BuildSetSeqContextRegs(mmVGT_HOS_MAX_TESS_LEVEL,
                                                                   mmVGT_HOS_MIN_TESS_LEVEL,
                                                                   &m_pm4ImageContext.hdrVgtHosTessLevel);
}

} // Gfx6
} // Pal
