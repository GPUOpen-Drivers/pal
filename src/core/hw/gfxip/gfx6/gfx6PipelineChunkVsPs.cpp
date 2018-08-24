/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6PipelineChunkVsPs.h"
#include "core/platform.h"
#include "palPipeline.h"
#include "palPipelineAbiProcessorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
PipelineChunkVsPs::PipelineChunkVsPs(
    const Device& device)
    :
    m_device(device),
    m_pVsPerfDataInfo(nullptr),
    m_pPsPerfDataInfo(nullptr)
{
    memset(&m_pm4ImageSh,       0, sizeof(m_pm4ImageSh));
    memset(&m_pm4ImageShDynamic, 0, sizeof(m_pm4ImageShDynamic));
    memset(&m_pm4ImageContext,  0, sizeof(m_pm4ImageContext));
    memset(&m_pm4ImageStrmout,  0, sizeof(m_pm4ImageStrmout));
    memset(&m_stageInfoVs,      0, sizeof(m_stageInfoVs));
    memset(&m_stageInfoPs,      0, sizeof(m_stageInfoPs));

    m_stageInfoVs.stageId = Abi::HardwareStage::Vs;
    m_stageInfoPs.stageId = Abi::HardwareStage::Ps;
}

// =====================================================================================================================
// Initializes this pipeline chunk using RelocatableShader objects representing the VS & PS hardware stages.
void PipelineChunkVsPs::Init(
    const AbiProcessor& abiProcessor,
    const VsPsParams&   params)
{
    const Gfx6PalSettings&   settings = m_device.Settings();
    const GpuChipProperties& chipInfo = m_device.Parent()->ChipProperties();

    m_pVsPerfDataInfo = params.pVsPerfDataInfo;
    m_pPsPerfDataInfo = params.pPsPerfDataInfo;

    uint16 lastPsInterpolator = mmSPI_PS_INPUT_CNTL_0;
    for (uint32 i = 0; i < MaxPsInputSemantics; ++i)
    {
        const uint16 offset = static_cast<uint16>(mmSPI_PS_INPUT_CNTL_0 + i);
        if (abiProcessor.HasRegisterEntry(offset, &m_pm4ImageContext.spiPsInputCntl[i].u32All))
        {
            lastPsInterpolator = offset;
        }
        else
        {
            break;
        }
    }

    if (abiProcessor.HasRegisterEntry(mmVGT_STRMOUT_CONFIG, &m_pm4ImageStrmout.vgtStrmoutConfig.u32All) &&
        (m_pm4ImageStrmout.vgtStrmoutConfig.u32All != 0))
    {
        for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
        {
            m_pm4ImageStrmout.stride[i].vgtStrmoutVtxStride.u32All =
                abiProcessor.GetRegisterEntry(mmVGT_STRMOUT_VTX_STRIDE_0 + i);
        }

        m_pm4ImageStrmout.vgtStrmoutBufferConfig.u32All = abiProcessor.GetRegisterEntry(mmVGT_STRMOUT_BUFFER_CONFIG);
    }

    BuildPm4Headers(lastPsInterpolator, (m_pm4ImageStrmout.vgtStrmoutConfig.u32All != 0));

    m_pm4ImageSh.spiShaderPgmRsrc1Vs.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC1_VS);
    m_pm4ImageSh.spiShaderPgmRsrc2Vs.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC2_VS);
    abiProcessor.HasRegisterEntry(mmSPI_SHADER_PGM_RSRC3_VS__CI__VI, &m_pm4ImageShDynamic.spiShaderPgmRsrc3Vs.u32All);

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_ENABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    m_pm4ImageSh.spiShaderPgmRsrc1Vs.bits.CU_GROUP_ENABLE = (settings.vsCuGroupEnabled ? 1 : 0);

    m_pm4ImageContext.paClVsOutCntl.u32All      = abiProcessor.GetRegisterEntry(mmPA_CL_VS_OUT_CNTL);
    m_pm4ImageContext.spiShaderPosFormat.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_POS_FORMAT);
    m_pm4ImageContext.vgtPrimitiveIdEn.u32All   = abiProcessor.GetRegisterEntry(mmVGT_PRIMITIVEID_EN);
    m_spiVsOutConfig.u32All                     = abiProcessor.GetRegisterEntry(mmSPI_VS_OUT_CONFIG);

    // If the number of VS output semantics exceeds the half-pack threshold, then enable VS half-pack mode.  Keep in
    // mind that the number of VS exports are represented by a -1 field in the HW register!
    if ((m_spiVsOutConfig.bits.VS_EXPORT_COUNT + 1u) > settings.vsHalfPackThreshold)
    {
        m_spiVsOutConfig.bits.VS_HALF_PACK = 1;
    }

    m_pm4ImageSh.spiShaderPgmRsrc1Ps.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC1_PS);
    m_pm4ImageSh.spiShaderPgmRsrc2Ps.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC2_PS);
    abiProcessor.HasRegisterEntry(mmSPI_SHADER_PGM_RSRC3_PS__CI__VI, &m_pm4ImageShDynamic.spiShaderPgmRsrc3Ps.u32All);

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_DISABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    m_pm4ImageSh.spiShaderPgmRsrc1Ps.bits.CU_GROUP_DISABLE = (settings.psCuGroupEnabled ? 0 : 1);

    m_spiPsInControl.u32All                     = abiProcessor.GetRegisterEntry(mmSPI_PS_IN_CONTROL);
    m_pm4ImageContext.spiBarycCntl.u32All       = abiProcessor.GetRegisterEntry(mmSPI_BARYC_CNTL);
    m_pm4ImageContext.spiPsInputAddr.u32All     = abiProcessor.GetRegisterEntry(mmSPI_PS_INPUT_ADDR);
    m_pm4ImageContext.spiPsInputEna.u32All      = abiProcessor.GetRegisterEntry(mmSPI_PS_INPUT_ENA);
    m_pm4ImageContext.spiShaderColFormat.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_COL_FORMAT);
    m_pm4ImageContext.spiShaderZFormat.u32All   = abiProcessor.GetRegisterEntry(mmSPI_SHADER_Z_FORMAT);

    if (chipInfo.gfxLevel >= GfxIpLevel::GfxIp7)
    {
        uint16 vsCuDisableMask = 0;
        if (m_device.LateAllocVsLimit())
        {
            // Disable virtualized CU #1 instead of #0 because thread traces use CU #0 by default.
            vsCuDisableMask = 0x2;
        }

        m_pm4ImageShDynamic.spiShaderPgmRsrc3Vs.bits.CU_EN =
            m_device.GetCuEnableMask(vsCuDisableMask, settings.vsCuEnLimitMask);
        m_pm4ImageShDynamic.spiShaderPgmRsrc3Ps.bits.CU_EN = m_device.GetCuEnableMask(0, settings.psCuEnLimitMask);
    }

    // Compute the checksum here because we don't want it to include the GPU virtual addresses!
    params.pHasher->Update(m_pm4ImageContext);
    params.pHasher->Update(m_pm4ImageStrmout);

    Abi::PipelineSymbolEntry symbol = { };
    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::VsMainEntry, &symbol))
    {
        const gpusize programGpuVa = (symbol.value + params.codeGpuVirtAddr);
        PAL_ASSERT(programGpuVa == Pow2Align(programGpuVa, 256));

        m_pm4ImageSh.spiShaderPgmLoVs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_pm4ImageSh.spiShaderPgmHiVs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);

        m_stageInfoVs.codeLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::VsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (symbol.value + params.dataGpuVirtAddr);
        m_pm4ImageSh.spiShaderUserDataLoVs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::VsDisassembly, &symbol))
    {
        m_stageInfoVs.disassemblyLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::PsMainEntry, &symbol))
    {
        const gpusize programGpuVa = (symbol.value + params.codeGpuVirtAddr);
        PAL_ASSERT(programGpuVa == Pow2Align(programGpuVa, 256));

        m_pm4ImageSh.spiShaderPgmLoPs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_pm4ImageSh.spiShaderPgmHiPs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);

        m_stageInfoPs.codeLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::PsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (symbol.value + params.dataGpuVirtAddr);
        m_pm4ImageSh.spiShaderUserDataLoPs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::PsDisassembly, &symbol))
    {
        m_stageInfoPs.disassemblyLength = static_cast<size_t>(symbol.size);
    }
}

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkVsPs::WriteShCommands(

    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& vsStageInfo,
    const DynamicStageInfo& psStageInfo
    ) const
{
    pCmdSpace = pCmdStream->WritePm4Image(m_pm4ImageSh.spaceNeeded, &m_pm4ImageSh, pCmdSpace);

    if (m_pm4ImageShDynamic.spaceNeeded > 0)
    {
        Pm4ImageShDynamic pm4ImageShDynamic = m_pm4ImageShDynamic;

        if (vsStageInfo.wavesPerSh > 0)
        {
            pm4ImageShDynamic.spiShaderPgmRsrc3Vs.bits.WAVE_LIMIT = vsStageInfo.wavesPerSh;
        }

        if (psStageInfo.wavesPerSh > 0)
        {
            pm4ImageShDynamic.spiShaderPgmRsrc3Ps.bits.WAVE_LIMIT = psStageInfo.wavesPerSh;
        }

        if (vsStageInfo.cuEnableMask != 0)
        {
            pm4ImageShDynamic.spiShaderPgmRsrc3Vs.bits.CU_EN &= vsStageInfo.cuEnableMask;
        }

        if (psStageInfo.cuEnableMask != 0)
        {
            pm4ImageShDynamic.spiShaderPgmRsrc3Ps.bits.CU_EN &= psStageInfo.cuEnableMask;
        }

        pCmdSpace = pCmdStream->WritePm4Image(pm4ImageShDynamic.spaceNeeded, &pm4ImageShDynamic, pCmdSpace);
    }

    if (m_pVsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_pVsPerfDataInfo->regOffset,
                                                                 m_pVsPerfDataInfo->gpuVirtAddr,
                                                                 pCmdSpace);
    }

    if (m_pPsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_pPsPerfDataInfo->regOffset,
                                                                 m_pPsPerfDataInfo->gpuVirtAddr,
                                                                 pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Copies this pipeline chunk's context commands into the specified command space. Returns the next unused
// DWORD in pCmdSpace.
uint32* PipelineChunkVsPs::WriteContextCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    pCmdSpace = pCmdStream->WritePm4Image(m_pm4ImageContext.spaceNeeded, &m_pm4ImageContext, pCmdSpace);
    pCmdSpace = pCmdStream->WritePm4Image(m_pm4ImageStrmout.spaceNeeded, &m_pm4ImageStrmout, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Assembles the PM4 headers for the commands in this pipeline chunk.
void PipelineChunkVsPs::BuildPm4Headers(
    uint32 lastPsInterpolator,
    bool   useStreamOutput)
{
    const CmdUtil& cmdUtil = m_device.CmdUtil();

    // Sets the following SH registers: SPI_SHADER_PGM_LO_VS, SPI_SHADER_PGM_HI_VS,
    // SPI_SHADER_PGM_RSRC1_VS, SPI_SHADER_PGM_RSRC2_VS.
    m_pm4ImageSh.spaceNeeded = cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_LO_VS,
                                                         mmSPI_SHADER_PGM_RSRC2_VS,
                                                         ShaderGraphics,
                                                         &m_pm4ImageSh.hdrPgmVs);

    // Sets the following SH register: SPI_SHADER_USER_DATA_VS_1.
    m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_VS_0 + ConstBufTblStartReg,
                                                         ShaderGraphics,
                                                         &m_pm4ImageSh.hdrUserDataVs);

    // Sets the following SH registers: SPI_SHADER_PGM_LO_PS, SPI_SHADER_PGM_HI_PS,
    // SPI_SHADER_PGM_RSRC1_PS, SPI_SHADER_PGM_RSRC2_PS.
    m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_LO_PS,
                                                          mmSPI_SHADER_PGM_RSRC2_PS,
                                                          ShaderGraphics,
                                                          &m_pm4ImageSh.hdrPgmPs);

    // Sets the following SH register: SPI_SHADER_USER_DATA_PS_1.
    m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_PS_0 + ConstBufTblStartReg,
                                                         ShaderGraphics,
                                                         &m_pm4ImageSh.hdrUserDataPs);

    if (m_device.Parent()->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp7)
    {
        // Sets the following SH register: SPI_SHADER_PGM_RSRC3_VS.
        // We must use the SET_SH_REG_INDEX packet to support the real-time compute feature.
        m_pm4ImageShDynamic.spaceNeeded = cmdUtil.BuildSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_VS__CI__VI,
                                                                        ShaderGraphics,
                                                                        SET_SH_REG_INDEX_CP_MODIFY_CU_MASK,
                                                                        &m_pm4ImageShDynamic.hdrPgmRsrc3Vs);

        // Sets the following SH register: SPI_SHADER_PGM_RSRC3_PS.
        // We must use the SET_SH_REG_INDEX packet to support the real-time compute feature.
        m_pm4ImageShDynamic.spaceNeeded += cmdUtil.BuildSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_PS__CI__VI,
                                                                         ShaderGraphics,
                                                                         SET_SH_REG_INDEX_CP_MODIFY_CU_MASK,
                                                                         &m_pm4ImageShDynamic.hdrPgmRsrc3Ps);
    }

    // Sets the following context registers:
    // SPI_SHADER_POS_FORMAT, SPI_SHADER_Z_FORMAT, SPI_SHADER_COL_FORMAT.
    m_pm4ImageContext.spaceNeeded = cmdUtil.BuildSetSeqContextRegs(mmSPI_SHADER_POS_FORMAT,
                                                                   mmSPI_SHADER_COL_FORMAT,
                                                                   &m_pm4ImageContext.hdrOutFormat);

    // Sets the following context register: PA_CL_VS_OUT_CNTL.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmPA_CL_VS_OUT_CNTL,
                                                                   &m_pm4ImageContext.hdrVsOutCntl);

    // Sets the following context register: VGT_PRIMITIVEID_EN.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_PRIMITIVEID_EN,
                                                                   &m_pm4ImageContext.hdrPrimId);

    // Sets the following context register: SPI_BARYC_CNTL.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmSPI_BARYC_CNTL, &m_pm4ImageContext.hdrBarycCntl);

    // Sets the following context registers: SPI_PS_INPUT_ENA, SPI_PS_INPUT_ADDR.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmSPI_PS_INPUT_ENA,
                                                                    mmSPI_PS_INPUT_ADDR,
                                                                    &m_pm4ImageContext.hdrPsIn);

    // Sets the following context registers: SPI_PS_INPUT_CNTL_0 - SPI_PS_INPUT_CNTL_X.  The number of registers
    // written by this packet depends on the pixel shader & hardware VS.
    PAL_ASSERT((lastPsInterpolator >= mmSPI_PS_INPUT_CNTL_0) && (lastPsInterpolator <= mmSPI_PS_INPUT_CNTL_31));
    m_pm4ImageContext.spaceNeeded += m_device.CmdUtil().BuildSetSeqContextRegs(mmSPI_PS_INPUT_CNTL_0,
                                                                               lastPsInterpolator,
                                                                               &m_pm4ImageContext.hdrPsInputs);

    // Sets the following context registers: VGT_STRMOUT_CONFIG and VGT_STRMOUT_BUFFER_CONFIG.
    m_pm4ImageStrmout.spaceNeeded = cmdUtil.BuildSetSeqContextRegs(mmVGT_STRMOUT_CONFIG,
                                                                   mmVGT_STRMOUT_BUFFER_CONFIG,
                                                                   &m_pm4ImageStrmout.hdrStrmoutCfg);
    if (useStreamOutput)
    {
        // Sets the following context registers: VGT_STRMOUT_VTX_STRIDE_*
        // NOTE: These register writes are unnecessary if stream-out is not active.
        constexpr uint16 VgtStrmoutVtxStride[] = { mmVGT_STRMOUT_VTX_STRIDE_0, mmVGT_STRMOUT_VTX_STRIDE_1,
                                                   mmVGT_STRMOUT_VTX_STRIDE_2, mmVGT_STRMOUT_VTX_STRIDE_3, };

        for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
        {
            m_pm4ImageStrmout.spaceNeeded +=
                cmdUtil.BuildSetOneContextReg(VgtStrmoutVtxStride[i],
                                              &m_pm4ImageStrmout.stride[i].hdrVgtStrmoutVtxStride);
        }
    }
}

} // Gfx6
} // Pal
