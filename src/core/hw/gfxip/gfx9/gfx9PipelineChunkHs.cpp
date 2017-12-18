/*
 *******************************************************************************
 *
 * Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkHs.h"
#include "core/platform.h"
#include "palPipeline.h"
#include "palPipelineAbiProcessorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
PipelineChunkHs::PipelineChunkHs(
    const Device& device)
    :
    m_device(device),
    m_pHsPerfDataInfo(nullptr)
{
    memset(&m_pm4ImageSh,        0, sizeof(m_pm4ImageSh));
    memset(&m_pm4ImageShDynamic, 0, sizeof(m_pm4ImageShDynamic));
    memset(&m_pm4ImageContext,   0, sizeof(m_pm4ImageContext));
    memset(&m_stageInfo,         0, sizeof(m_stageInfo));
    m_stageInfo.stageId = Abi::HardwareStage::Hs;
}

// =====================================================================================================================
// Initializes this pipeline chunk for the scenario where the tessellation stages are active.
void PipelineChunkHs::Init(
    const AbiProcessor& abiProcessor,
    const HsParams&     params)
{
    m_pHsPerfDataInfo = params.pHsPerfDataInfo;

    BuildPm4Headers();

    m_pm4ImageSh.spiShaderPgmRsrc1Hs.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC1_HS);
    m_pm4ImageSh.spiShaderPgmRsrc2Hs.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC2_HS);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 345
    abiProcessor.HasRegisterEntry(mmSPI_SHADER_PGM_RSRC3_HS, &m_pm4ImageShDynamic.spiShaderPgmRsrc3Hs.u32All);
#endif
    m_pm4ImageShDynamic.spiShaderPgmRsrc3Hs.gfx9.bits.CU_EN = m_device.GetCuEnableMask(0, UINT_MAX);

    m_pm4ImageContext.vgtHosMinTessLevel.u32All = abiProcessor.GetRegisterEntry(mmVGT_HOS_MIN_TESS_LEVEL);
    m_pm4ImageContext.vgtHosMaxTessLevel.u32All = abiProcessor.GetRegisterEntry(mmVGT_HOS_MAX_TESS_LEVEL);

    // Compute the checksum here because we don't want it to include the GPU virtual addresses!
    params.pHasher->Update(m_pm4ImageContext);

    Abi::PipelineSymbolEntry symbol = { };
    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::HsMainEntry, &symbol))
    {
        const gpusize programGpuVa = (symbol.value + params.codeGpuVirtAddr);
        PAL_ASSERT(programGpuVa == Pow2Align(programGpuVa, 256));

        m_pm4ImageSh.spiShaderPgmLoLs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_pm4ImageSh.spiShaderPgmHiLs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);

        m_stageInfo.codeLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::HsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (symbol.value + params.dataGpuVirtAddr);
        m_pm4ImageSh.spiShaderUserDataLoHs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::HsDisassembly, &symbol))
    {
        m_stageInfo.disassemblyLength = static_cast<size_t>(symbol.size);
    }
}

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkHs::WriteShCommands(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& hsStageInfo
    ) const
{
    Pm4ImageShDynamic pm4ImageShDynamic = m_pm4ImageShDynamic;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 345
    if (m_pm4ImageShDynamic.spiShaderPgmRsrc3Hs.gfx9.bits.WAVE_LIMIT == 0)
#endif
    {
        pm4ImageShDynamic.spiShaderPgmRsrc3Hs.gfx9.bits.WAVE_LIMIT = hsStageInfo.wavesPerSh;
    }

    if (hsStageInfo.cuEnableMask != 0)
    {
        pm4ImageShDynamic.spiShaderPgmRsrc3Hs.gfx9.bits.CU_EN &= hsStageInfo.cuEnableMask;
    }

    pCmdSpace = pCmdStream->WritePm4Image(m_pm4ImageSh.spaceNeeded, &m_pm4ImageSh, pCmdSpace);
    pCmdSpace = pCmdStream->WritePm4Image(pm4ImageShDynamic.spaceNeeded, &pm4ImageShDynamic, pCmdSpace);

    if (m_pHsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_pHsPerfDataInfo->regOffset,
                                                                 m_pHsPerfDataInfo->gpuVirtAddr,
                                                                 pCmdSpace);
    }

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
    pCmdSpace = pCmdStream->WritePm4Image(m_pm4ImageContext.spaceNeeded, &m_pm4ImageContext, pCmdSpace);
    return pCmdSpace;
}

// =====================================================================================================================
// Assembles the PM4 headers for the commands in this Pipeline chunk.
void PipelineChunkHs::BuildPm4Headers()
{
    const CmdUtil& cmdUtil            = m_device.CmdUtil();
    const uint16   baseUserDataHs     = m_device.GetBaseUserDataReg(HwShaderStage::Hs);
    const uint32   mmSpiShaderPgmLoLs = cmdUtil.GetRegInfo().mmSpiShaderPgmLoLs;

    // Sets the following SH register: SPI_SHADER_USER_DATA_LS_1.
    m_pm4ImageSh.spaceNeeded = cmdUtil.BuildSetOneShReg(baseUserDataHs + ConstBufTblStartReg,
                                                        ShaderGraphics,
                                                        &m_pm4ImageSh.hdrSpiShaderUserData);

    // Sets the following SH registers: SPI_SHADER_PGM_RSRC1_HS, SPI_SHADER_PGM_RSRC2_HS.
    m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_RSRC1_HS,
                                                          mmSPI_SHADER_PGM_RSRC2_HS,
                                                          ShaderGraphics,
                                                          &m_pm4ImageSh.hdrSpiShaderPgm);

    // Sets the following SH registers: SPI_SHADER_PGM_LO_LS, SPI_SHADER_PGM_HI_LS.
    m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetSeqShRegs(mmSpiShaderPgmLoLs,
                                                          mmSpiShaderPgmLoLs + 1,
                                                          ShaderGraphics,
                                                          &m_pm4ImageSh.hdrSpiShaderPgmLs);

    // Sets the following SH register: SPI_SHADER_PGM_RSRC3_HS.
    // We must use the SET_SH_REG_INDEX packet to support the real-time compute feature.
    m_pm4ImageShDynamic.spaceNeeded = cmdUtil.BuildSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_HS,
                                                                    ShaderGraphics,
                                                                    index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                                    &m_pm4ImageShDynamic.hdrPgmRsrc3Hs);

    // Sets the following context registers: VGT_HOS_MAX_TESS_LEVEL, VGT_HOS_MIN_TESS_LEVEL.
    m_pm4ImageContext.spaceNeeded = cmdUtil.BuildSetSeqContextRegs(mmVGT_HOS_MAX_TESS_LEVEL,
                                                                   mmVGT_HOS_MIN_TESS_LEVEL,
                                                                   &m_pm4ImageContext.hdrvVgtHosTessLevel);
}

} // Gfx9
} // Pal
