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

#include "core/platform.h"
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6GraphicsPipeline.h"
#include "core/hw/gfxip/gfx6/gfx6PipelineChunkVsPs.h"
#include "palPipelineAbiProcessorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// Base count of SH registers which are loaded using LOAD_SH_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedShRegCount =
    1 + // mmSPI_SHADER_PGM_LO_VS
    1 + // mmSPI_SHADER_PGM_HI_VS
    1 + // mmSPI_SHADER_PGM_RSRC1_VS
    1 + // mmSPI_SHADER_PGM_RSRC2_VS
    1 + // mmSPI_SHADER_USER_DATA_VS_0 + ConstBufTblStartReg
    1 + // mmSPI_SHADER_PGM_LO_PS
    1 + // mmSPI_SHADER_PGM_HI_PS
    1 + // mmSPI_SHADER_PGM_RSRC1_PS
    1 + // mmSPI_SHADER_PGM_RSRC2_PS
    1;  // mmSPI_SHADER_USER_DATA_PS_0 + ConstBufTblStartReg

// Base count of Context registers which are loaded using LOAD_CNTX_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedCntxRegCount =
    1 + // mmSPI_SHADER_POS_FORMAT
    1 + // mmSPI_SHADER_Z_FORMAT
    1 + // mmSPI_SHADER_COL_FORMAT
    1 + // mmPA_CL_VS_OUT_CNTL
    1 + // mmVGT_PRIMITIVEID_EN
    1 + // mmSPI_BARYC_CNTL
    1 + // mmSPI_PS_INPUT_ENA
    1 + // mmSPI_PS_INPUT_ADDR
    0 + // mmSPI_PS_INPUT_CNTL_0...31 are not included because the number of interpolants depends on the pipeline
    1 + // mmVGT_STRMOUT_CONFIG
    1;  // mmVGT_STRMOUT_BUFFER_CONFIG

// Base count of Context registers which are loaded using LOAD_CNTX_REG_INDEX when binding to a command buffer when
// stream-out is enabled for this pipeline.
static constexpr uint32 BaseLoadedCntxRegCountStreamOut =
    4;  // mmVGT_STRMOUT_VTX_STRIDE_[0...3]

// Stream-out vertex stride register addresses.
constexpr uint16 VgtStrmoutVtxStrideAddr[] =
    { mmVGT_STRMOUT_VTX_STRIDE_0, mmVGT_STRMOUT_VTX_STRIDE_1, mmVGT_STRMOUT_VTX_STRIDE_2, mmVGT_STRMOUT_VTX_STRIDE_3, };

// =====================================================================================================================
PipelineChunkVsPs::PipelineChunkVsPs(
    const Device&       device,
    const PerfDataInfo* pVsPerfDataInfo,
    const PerfDataInfo* pPsPerfDataInfo)
    :
    m_device(device),
    m_pVsPerfDataInfo(pVsPerfDataInfo),
    m_pPsPerfDataInfo(pPsPerfDataInfo)
{
    m_spiPsInControl.u32All = 0;
    m_spiVsOutConfig.u32All = 0;

    memset(&m_commands,  0, sizeof(m_commands));
    memset(&m_stageInfoVs, 0, sizeof(m_stageInfoVs));
    memset(&m_stageInfoPs, 0, sizeof(m_stageInfoPs));

    m_stageInfoVs.stageId = Abi::HardwareStage::Vs;
    m_stageInfoPs.stageId = Abi::HardwareStage::Ps;
}

// =====================================================================================================================
// Early initialization for this pipeline chunk.  Responsible for determining the number of SH and context registers to
// be loaded using LOAD_CNTX_REG_INDEX and LOAD_SH_REG_INDEX, as well as determining the number of PS interpolators and
// saving that information for LateInit to use.
void PipelineChunkVsPs::EarlyInit(
    const RegisterVector&     registers,
    GraphicsPipelineLoadInfo* pInfo)
{
    PAL_ASSERT(pInfo != nullptr);

    // Determine if stream-out is enabled for this pipeline.
    registers.HasEntry(mmVGT_STRMOUT_CONFIG, &m_commands.streamOut.vgtStrmoutConfig.u32All);

    // Determine the number of PS interpolators and save them for LateInit to consume.
    pInfo->interpolatorCount = 0;
    for (uint16 i = 0; i < MaxPsInputSemantics; ++i)
    {
        const uint16 offset = (mmSPI_PS_INPUT_CNTL_0 + i);
        if (registers.HasEntry(offset, &m_commands.context.spiPsInputCntl[i].u32All) == false)
        {
            break;
        }

        ++(pInfo->interpolatorCount);
    }

    PAL_ASSERT(pInfo->interpolatorCount >= 1);

    const Gfx6PalSettings& settings = m_device.Settings();
    if (settings.enableLoadIndexForObjectBinds != false)
    {
        pInfo->loadedCtxRegCount += (BaseLoadedCntxRegCount + pInfo->interpolatorCount);
        pInfo->loadedShRegCount  +=  BaseLoadedShRegCount;

        if (UsesStreamOut())
        {
            pInfo->loadedCtxRegCount += BaseLoadedCntxRegCountStreamOut;
        }
    }
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.  Also uploads register state into GPU memory.
void PipelineChunkVsPs::LateInit(
    const AbiProcessor&             abiProcessor,
    const RegisterVector&           registers,
    const GraphicsPipelineLoadInfo& loadInfo,
    GraphicsPipelineUploader*       pUploader,
    Util::MetroHash64*              pHasher)
{
    const bool useLoadIndexPath = pUploader->EnableLoadIndexPath();

    const Gfx6PalSettings&   settings  = m_device.Settings();
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    BuildPm4Headers(useLoadIndexPath, loadInfo.interpolatorCount);

    Abi::PipelineSymbolEntry symbol = {};
    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::VsMainEntry, &symbol))
    {
        m_stageInfoVs.codeLength   = static_cast<size_t>(symbol.size);
        const gpusize programGpuVa = (pUploader->CodeGpuVirtAddr() + symbol.value);
        PAL_ASSERT(programGpuVa == Pow2Align(programGpuVa, 256));

        m_commands.sh.spiShaderPgmLoVs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_commands.sh.spiShaderPgmHiVs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::VsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (pUploader->DataGpuVirtAddr() + symbol.value);
        m_commands.sh.spiShaderUserDataLoVs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::VsDisassembly, &symbol))
    {
        m_stageInfoVs.disassemblyLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::PsMainEntry, &symbol))
    {
        m_stageInfoPs.codeLength   = static_cast<size_t>(symbol.size);
        const gpusize programGpuVa = (pUploader->CodeGpuVirtAddr() + symbol.value);
        PAL_ASSERT(programGpuVa == Pow2Align(programGpuVa, 256));

        m_commands.sh.spiShaderPgmLoPs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_commands.sh.spiShaderPgmHiPs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::PsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (pUploader->DataGpuVirtAddr() + symbol.value);
        m_commands.sh.spiShaderUserDataLoPs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::PsDisassembly, &symbol))
    {
        m_stageInfoPs.disassemblyLength = static_cast<size_t>(symbol.size);
    }

    m_commands.sh.spiShaderPgmRsrc1Vs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_VS);
    m_commands.sh.spiShaderPgmRsrc2Vs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_VS);
    registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_VS__CI__VI, &m_commands.dynamic.spiShaderPgmRsrc3Vs.u32All);

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_ENABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    m_commands.sh.spiShaderPgmRsrc1Vs.bits.CU_GROUP_ENABLE = (settings.vsCuGroupEnabled ? 1 : 0);

    m_commands.sh.spiShaderPgmRsrc1Ps.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_PS);
    m_commands.sh.spiShaderPgmRsrc2Ps.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_PS);
    registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_PS__CI__VI, &m_commands.dynamic.spiShaderPgmRsrc3Ps.u32All);

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_DISABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    m_commands.sh.spiShaderPgmRsrc1Ps.bits.CU_GROUP_DISABLE = (settings.psCuGroupEnabled ? 0 : 1);

    m_commands.context.paClVsOutCntl.u32All      = registers.At(mmPA_CL_VS_OUT_CNTL);
    m_commands.context.spiShaderPosFormat.u32All = registers.At(mmSPI_SHADER_POS_FORMAT);
    m_commands.context.vgtPrimitiveIdEn.u32All   = registers.At(mmVGT_PRIMITIVEID_EN);

    // If the number of VS output semantics exceeds the half-pack threshold, then enable VS half-pack mode.  Keep in
    // mind that the number of VS exports are represented by a -1 field in the HW register!
    m_spiVsOutConfig.u32All = registers.At(mmSPI_VS_OUT_CONFIG);
    if ((m_spiVsOutConfig.bits.VS_EXPORT_COUNT + 1u) > settings.vsHalfPackThreshold)
    {
        m_spiVsOutConfig.bits.VS_HALF_PACK = 1;
    }

    m_spiPsInControl.u32All                      = registers.At(mmSPI_PS_IN_CONTROL);
    m_commands.context.spiBarycCntl.u32All       = registers.At(mmSPI_BARYC_CNTL);
    m_commands.context.spiPsInputAddr.u32All     = registers.At(mmSPI_PS_INPUT_ADDR);
    m_commands.context.spiPsInputEna.u32All      = registers.At(mmSPI_PS_INPUT_ENA);
    m_commands.context.spiShaderColFormat.u32All = registers.At(mmSPI_SHADER_COL_FORMAT);
    m_commands.context.spiShaderZFormat.u32All   = registers.At(mmSPI_SHADER_Z_FORMAT);

    if (UsesStreamOut())
    {
        for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
        {
            m_commands.streamOut.stride[i].vgtStrmoutVtxStride.u32All = registers.At(VgtStrmoutVtxStrideAddr[i]);
        }

        m_commands.streamOut.vgtStrmoutBufferConfig.u32All = registers.At(mmVGT_STRMOUT_BUFFER_CONFIG);
    }

    pHasher->Update(m_commands.context);
    pHasher->Update(m_commands.streamOut);

    if (chipProps.gfxLevel >= GfxIpLevel::GfxIp7)
    {
        uint16 vsCuDisableMask = 0;
        if (m_device.LateAllocVsLimit())
        {
            // Disable virtualized CU #1 instead of #0 because thread traces use CU #0 by default.
            vsCuDisableMask = 0x2;
        }

        m_commands.dynamic.spiShaderPgmRsrc3Vs.bits.CU_EN = m_device.GetCuEnableMask(vsCuDisableMask,
                                                                                     settings.vsCuEnLimitMask);
        m_commands.dynamic.spiShaderPgmRsrc3Ps.bits.CU_EN = m_device.GetCuEnableMask(0, settings.psCuEnLimitMask);
    }

    if (useLoadIndexPath)
    {
        pUploader->AddShReg(mmSPI_SHADER_PGM_LO_VS, m_commands.sh.spiShaderPgmLoVs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_HI_VS, m_commands.sh.spiShaderPgmHiVs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_LO_PS, m_commands.sh.spiShaderPgmLoPs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_HI_PS, m_commands.sh.spiShaderPgmHiPs);

        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC1_VS, m_commands.sh.spiShaderPgmRsrc1Vs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC2_VS, m_commands.sh.spiShaderPgmRsrc2Vs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC1_PS, m_commands.sh.spiShaderPgmRsrc1Ps);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC2_PS, m_commands.sh.spiShaderPgmRsrc2Ps);

        pUploader->AddShReg(mmSPI_SHADER_USER_DATA_VS_0 + ConstBufTblStartReg, m_commands.sh.spiShaderUserDataLoVs);
        pUploader->AddShReg(mmSPI_SHADER_USER_DATA_PS_0 + ConstBufTblStartReg, m_commands.sh.spiShaderUserDataLoPs);

        pUploader->AddCtxReg(mmSPI_SHADER_POS_FORMAT,     m_commands.context.spiShaderPosFormat);
        pUploader->AddCtxReg(mmSPI_SHADER_Z_FORMAT,       m_commands.context.spiShaderZFormat);
        pUploader->AddCtxReg(mmSPI_SHADER_COL_FORMAT,     m_commands.context.spiShaderColFormat);
        pUploader->AddCtxReg(mmPA_CL_VS_OUT_CNTL,         m_commands.context.paClVsOutCntl);
        pUploader->AddCtxReg(mmVGT_PRIMITIVEID_EN,        m_commands.context.vgtPrimitiveIdEn);
        pUploader->AddCtxReg(mmSPI_BARYC_CNTL,            m_commands.context.spiBarycCntl);
        pUploader->AddCtxReg(mmSPI_PS_INPUT_ENA,          m_commands.context.spiPsInputEna);
        pUploader->AddCtxReg(mmSPI_PS_INPUT_ADDR,         m_commands.context.spiPsInputAddr);
        pUploader->AddCtxReg(mmVGT_STRMOUT_CONFIG,        m_commands.streamOut.vgtStrmoutConfig);
        pUploader->AddCtxReg(mmVGT_STRMOUT_BUFFER_CONFIG, m_commands.streamOut.vgtStrmoutBufferConfig);

        for (uint16 i = 0; i < loadInfo.interpolatorCount; ++i)
        {
            pUploader->AddCtxReg(mmSPI_PS_INPUT_CNTL_0 + i, m_commands.context.spiPsInputCntl[i]);
        }

        if (UsesStreamOut())
        {
            for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
            {
                pUploader->AddCtxReg(VgtStrmoutVtxStrideAddr[i], m_commands.streamOut.stride[i].vgtStrmoutVtxStride);
            }
        }
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
    // NOTE: The SH register PM4 image headers will be zero if the GPU doesn't support these registers.
    if (m_commands.sh.hdrSpiShaderPgmVs.header.u32All != 0)
    {
        constexpr uint32 SpaceNeededSh = sizeof(m_commands.sh) / sizeof(uint32);
        pCmdSpace = pCmdStream->WritePm4Image(SpaceNeededSh, &m_commands.sh, pCmdSpace);
    }

    // NOTE: The dynamic register PM4 image headers will be zero if the GPU doesn't support these registers.
    if (m_commands.dynamic.hdrPgmRsrc3Vs.header.u32All != 0)
    {
        auto dynamicCmds = m_commands.dynamic;

        if (vsStageInfo.wavesPerSh > 0)
        {
            dynamicCmds.spiShaderPgmRsrc3Vs.bits.WAVE_LIMIT = vsStageInfo.wavesPerSh;
        }

        if (psStageInfo.wavesPerSh > 0)
        {
            dynamicCmds.spiShaderPgmRsrc3Ps.bits.WAVE_LIMIT = psStageInfo.wavesPerSh;
        }

        if (vsStageInfo.cuEnableMask != 0)
        {
            dynamicCmds.spiShaderPgmRsrc3Vs.bits.CU_EN &= vsStageInfo.cuEnableMask;
        }

        if (psStageInfo.cuEnableMask != 0)
        {
            dynamicCmds.spiShaderPgmRsrc3Ps.bits.CU_EN &= psStageInfo.cuEnableMask;
        }

        constexpr uint32 SpaceNeededDynamic = sizeof(m_commands.dynamic) / sizeof(uint32);
        pCmdSpace = pCmdStream->WritePm4Image(SpaceNeededDynamic, &dynamicCmds, pCmdSpace);
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
    // NOTE: The context and stream-out PM4 image sizes will be zero if the LOAD_INDEX path is enabled.  It is only
    // expected that this will be called if the pipeline is using the SET path.
    PAL_ASSERT(m_commands.context.spaceNeeded > 0);
    PAL_ASSERT(m_commands.streamOut.spaceNeeded > 0);

    pCmdSpace = pCmdStream->WritePm4Image(m_commands.context.spaceNeeded, &m_commands.context, pCmdSpace);
    return pCmdStream->WritePm4Image(m_commands.streamOut.spaceNeeded, &m_commands.streamOut, pCmdSpace);
}

// =====================================================================================================================
// Assembles the PM4 headers for the commands in this pipeline chunk.
void PipelineChunkVsPs::BuildPm4Headers(
    bool   enableLoadIndexPath,
    uint32 interpolatorCount)
{
    const CmdUtil& cmdUtil = m_device.CmdUtil();

    // NOTE: The context, stream-out and SH PM4 images should have zero size when the LOAD_INDEX path is enabled.  There
    // is no need to build the PM4 headers when running in that mode.
    if (!enableLoadIndexPath)
    {
        cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_LO_VS,
                                  mmSPI_SHADER_PGM_RSRC2_VS,
                                  ShaderGraphics,
                                  &m_commands.sh.hdrSpiShaderPgmVs);

        cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_VS_0 + ConstBufTblStartReg,
                                 ShaderGraphics,
                                 &m_commands.sh.hdrSpiShaderUserDataVs);

        cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_LO_PS,
                                  mmSPI_SHADER_PGM_RSRC2_PS,
                                  ShaderGraphics,
                                  &m_commands.sh.hdrSpiSHaderPgmPs);

        cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_PS_0 + ConstBufTblStartReg,
                                 ShaderGraphics,
                                 &m_commands.sh.hdrSpiShaderUserDataPs);

        m_commands.context.spaceNeeded =  cmdUtil.BuildSetSeqContextRegs(mmSPI_SHADER_POS_FORMAT,
                                                                         mmSPI_SHADER_COL_FORMAT,
                                                                         &m_commands.context.hdrOutFormat);

        m_commands.context.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmPA_CL_VS_OUT_CNTL,
                                                                        &m_commands.context.hdrVsOutCntl);
        m_commands.context.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_PRIMITIVEID_EN,
                                                                        &m_commands.context.hdrPrimId);

        m_commands.context.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmSPI_BARYC_CNTL,
                                                                        &m_commands.context.hdrBarycCntl);

        m_commands.context.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmSPI_PS_INPUT_ENA,
                                                                         mmSPI_PS_INPUT_ADDR,
                                                                         &m_commands.context.hdrPsIn);

        PAL_ASSERT(interpolatorCount <= MaxPsInputSemantics);
        m_commands.context.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmSPI_PS_INPUT_CNTL_0,
                                                                        (mmSPI_PS_INPUT_CNTL_0 + interpolatorCount - 1),
                                                                        &m_commands.context.hdrPsInputs);

        m_commands.streamOut.spaceNeeded = cmdUtil.BuildSetSeqContextRegs(mmVGT_STRMOUT_CONFIG,
                                                                          mmVGT_STRMOUT_BUFFER_CONFIG,
                                                                          &m_commands.streamOut.hdrStrmoutCfg);
        if (UsesStreamOut())
        {
            for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
            {
                m_commands.streamOut.spaceNeeded += cmdUtil.BuildSetOneContextReg(
                    VgtStrmoutVtxStrideAddr[i],
                    &m_commands.streamOut.stride[i].hdrVgtStrmoutVtxStride);
            }
        }
    }

    if (m_device.Parent()->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp7)
    {
        cmdUtil.BuildSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_VS__CI__VI,
                                      ShaderGraphics,
                                      SET_SH_REG_INDEX_CP_MODIFY_CU_MASK,
                                      &m_commands.dynamic.hdrPgmRsrc3Vs);

        cmdUtil.BuildSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_PS__CI__VI,
                                      ShaderGraphics,
                                      SET_SH_REG_INDEX_CP_MODIFY_CU_MASK,
                                      &m_commands.dynamic.hdrPgmRsrc3Ps);
    }
}

} // Gfx6
} // Pal
