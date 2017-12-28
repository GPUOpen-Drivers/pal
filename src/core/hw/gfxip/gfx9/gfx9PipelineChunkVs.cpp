/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkVs.h"
#include "core/platform.h"
#include "palPipeline.h"
#include "palPipelineAbiProcessorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
PipelineChunkVs::PipelineChunkVs(
    const Device& device)
    :
    m_device(device),
    m_pVsPerfDataInfo(nullptr)
{
    memset(&m_pm4ImageSh,        0, sizeof(m_pm4ImageSh));
    memset(&m_pm4ImageShDynamic, 0, sizeof(m_pm4ImageShDynamic));
    memset(&m_pm4ImageContext,   0, sizeof(m_pm4ImageContext));
    memset(&m_stageInfo,         0, sizeof(m_stageInfo));
    m_stageInfo.stageId = Abi::HardwareStage::Vs;
}

// =====================================================================================================================
// Initializes this pipeline chunk.
void PipelineChunkVs::Init(
    const AbiProcessor& abiProcessor,
    const VsParams&     params)
{
    const Gfx9PalSettings& settings = m_device.Settings();

    m_pVsPerfDataInfo = params.pVsPerfDataInfo;

    BuildPm4Headers();

    m_pm4ImageSh.spiShaderPgmRsrc1Vs.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC1_VS);
    m_pm4ImageSh.spiShaderPgmRsrc2Vs.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC2_VS);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 345
    abiProcessor.HasRegisterEntry(mmSPI_SHADER_PGM_RSRC3_VS, &m_pm4ImageShDynamic.spiShaderPgmRsrc3Vs.u32All);
#endif

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_ENABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    m_pm4ImageSh.spiShaderPgmRsrc1Vs.bits.CU_GROUP_ENABLE = (settings.vsCuGroupEnabled ? 1 : 0);

    uint16 vsCuDisableMask = 0;
    if (m_device.LateAllocVsLimit())
    {
        // Disable virtualized CU #1 instead of #0 because thread traces use CU #0 by default.
        vsCuDisableMask = 0x2;
    }

    // NOTE: The Pipeline ABI doesn't specify CU enable masks for each shader stage, so it should be safe to
    // always use the ones PAL prefers.
    m_pm4ImageShDynamic.spiShaderPgmRsrc3Vs.bits.CU_EN = m_device.GetCuEnableMask(vsCuDisableMask,
                                                                                  settings.vsCuEnLimitMask);

    m_pm4ImageContext.paClVsOutCntl.u32All      = abiProcessor.GetRegisterEntry(mmPA_CL_VS_OUT_CNTL);
    m_pm4ImageContext.spiShaderPosFormat.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_POS_FORMAT);
    m_pm4ImageContext.vgtPrimitiveIdEn.u32All   = abiProcessor.GetRegisterEntry(mmVGT_PRIMITIVEID_EN);

    // Compute the checksum here because we don't want it to include the GPU virtual addresses!
    params.pHasher->Update(m_pm4ImageContext);

    Abi::PipelineSymbolEntry symbol = { };
    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::VsMainEntry, &symbol))
    {
        const gpusize programGpuVa = (symbol.value + params.codeGpuVirtAddr);
        PAL_ASSERT(programGpuVa == Pow2Align(programGpuVa, 256));

        m_pm4ImageSh.spiShaderPgmLoVs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_pm4ImageSh.spiShaderPgmHiVs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);

        m_stageInfo.codeLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::VsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (symbol.value + params.dataGpuVirtAddr);
        m_pm4ImageSh.spiShaderUserDataLoVs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::VsDisassembly, &symbol))
    {
        m_stageInfo.disassemblyLength = static_cast<size_t>(symbol.size);
    }
}

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkVs::WriteShCommands(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& vsStageInfo
    ) const
{
    Pm4ImageShDynamic pm4ImageShDynamic = m_pm4ImageShDynamic;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 345
    if (m_pm4ImageShDynamic.spiShaderPgmRsrc3Vs.bits.WAVE_LIMIT == 0)
#endif
    {
        pm4ImageShDynamic.spiShaderPgmRsrc3Vs.bits.WAVE_LIMIT = vsStageInfo.wavesPerSh;
    }

    if (vsStageInfo.cuEnableMask != 0)
    {
        pm4ImageShDynamic.spiShaderPgmRsrc3Vs.bits.CU_EN &= vsStageInfo.cuEnableMask;
    }

    pCmdSpace = pCmdStream->WritePm4Image(m_pm4ImageSh.spaceNeeded, &m_pm4ImageSh, pCmdSpace);
    pCmdSpace = pCmdStream->WritePm4Image(pm4ImageShDynamic.spaceNeeded, &pm4ImageShDynamic, pCmdSpace);

    if (m_pVsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_pVsPerfDataInfo->regOffset,
                                                                 m_pVsPerfDataInfo->gpuVirtAddr,
                                                                 pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Copies this pipeline chunk's context commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkVs::WriteContextCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    pCmdSpace = pCmdStream->WritePm4Image(m_pm4ImageContext.spaceNeeded, &m_pm4ImageContext, pCmdSpace);
    return pCmdSpace;
}

// =====================================================================================================================
// Assembles the PM4 headers for the commands in this pipeline chunk.
void PipelineChunkVs::BuildPm4Headers()
{
    const CmdUtil& cmdUtil = m_device.CmdUtil();

    // Sets the following SH registers: SPI_SHADER_PGM_LO_VS, SPI_SHADER_PGM_HI_VS,
    // SPI_SHADER_PGM_RSRC1_VS, SPI_SHADER_PGM_RSRC2_VS.
    m_pm4ImageSh.spaceNeeded = cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_LO_VS,
                                                         mmSPI_SHADER_PGM_RSRC2_VS,
                                                         ShaderGraphics,
                                                         &m_pm4ImageSh.hdrSpiShaderPgmVs);

    // Sets the following SH register: SPI_SHADER_USER_DATA_VS_1.
    m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_VS_0 + ConstBufTblStartReg,
                                                         ShaderGraphics,
                                                         &m_pm4ImageSh.hdrSpiShaderUserDataVs);

    // Sets the following SH register: SPI_SHADER_PGM_RSRC3_VS.
    // We must use the SET_SH_REG_INDEX packet to support the real-time compute feature.
    m_pm4ImageShDynamic.spaceNeeded = cmdUtil.BuildSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_VS,
                                                                    ShaderGraphics,
                                                                    index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                                    &m_pm4ImageShDynamic.hdrPgmRsrc3Vs);

    // Sets the following context register: SPI_SHADER_POS_FORMAT.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmSPI_SHADER_POS_FORMAT,
                                                                   &m_pm4ImageContext.hdrSpiShaderPosFormat);

    // Sets the following context register: PA_CL_VS_OUT_CNTL.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmPA_CL_VS_OUT_CNTL,
                                                                   &m_pm4ImageContext.hdrPaClVsOutCntl);

    // Sets the following context register: VGT_PRIMITIVEID_EN.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_PRIMITIVEID_EN,
                                                                   &m_pm4ImageContext.hdrVgtPrimitiveIdEn);
}

} // Gfx9
} // Pal
