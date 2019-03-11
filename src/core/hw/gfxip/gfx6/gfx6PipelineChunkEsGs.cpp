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
#include "core/hw/gfxip/gfx6/gfx6PipelineChunkEsGs.h"
#include "palPipelineAbiProcessorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// Base count of SH registers which are loaded using LOAD_SH_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedShRegCount =
    1 + // mmSPI_SHADER_PGM_LO_ES
    1 + // mmSPI_SHADER_PGM_HI_ES
    1 + // mmSPI_SHADER_PGM_RSRC1_ES
    1 + // mmSPI_SHADER_PGM_RSRC2_ES
    1 + // mmSPI_SHADER_USER_DATA_ES_0 + ConstBufTblStartReg
    1 + // mmSPI_SHADER_PGM_LO_GS
    1 + // mmSPI_SHADER_PGM_HI_GS
    1 + // mmSPI_SHADER_PGM_RSRC1_GS
    1 + // mmSPI_SHADER_PGM_RSRC2_GS
    1;  // mmSPI_SHADER_USER_DATA_GS_0 + ConstBufTblStartReg

// Base count of Context registers which are loaded using LOAD_CNTX_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedCntxRegCount =
    1 + // mmVGT_GS_MAX_VERT_OUT
    1 + // mmVGT_GS_OUT_PRIM_TYPE
    1 + // mmVGT_GS_INSTANCE_CNT
    1 + // mmVGT_GS_PER_ES
    1 + // mmVGT_ES_PER_GS
    1 + // mmVGT_GS_PER_VS
    4 + // mmVGT_GS_VERT_ITEMSIZE_*
    1 + // mmVGT_ESGS_RING_ITEMSIZE
    1 + // mmVGT_GSVS_RING_ITEMSIZE
    3 + // mmVGT_GSVS_RING_OFFSET_*
    1;  // mmVGT_GS_ONCHIP_CNTL__CI__VI

// =====================================================================================================================
PipelineChunkEsGs::PipelineChunkEsGs(
    const Device&       device,
    const PerfDataInfo* pEsPerfDataInfo,
    const PerfDataInfo* pGsPerfDataInfo)
    :
    m_device(device),
    m_pEsPerfDataInfo(pEsPerfDataInfo),
    m_pGsPerfDataInfo(pGsPerfDataInfo)
{
    memset(&m_commands, 0, sizeof(m_commands));
    memset(&m_stageInfoEs, 0, sizeof(m_stageInfoEs));
    memset(&m_stageInfoGs, 0, sizeof(m_stageInfoGs));

    m_stageInfoEs.stageId = Abi::HardwareStage::Es;
    m_stageInfoGs.stageId = Abi::HardwareStage::Gs;
}

// =====================================================================================================================
// Early initialization for this pipeline chunk.  Responsible for determining the number of SH and context registers to
// be loaded using LOAD_CNTX_REG_INDEX and LOAD_SH_REG_INDEX.
void PipelineChunkEsGs::EarlyInit(
    GraphicsPipelineLoadInfo* pInfo)
{
    PAL_ASSERT(pInfo != nullptr);

    const Gfx6PalSettings& settings = m_device.Settings();
    if (settings.enableLoadIndexForObjectBinds != false)
    {
        pInfo->loadedCtxRegCount += BaseLoadedCntxRegCount;
        pInfo->loadedShRegCount  += BaseLoadedShRegCount;

        if (pInfo->usesOnChipGs)
        {
            PAL_ASSERT((pInfo->esGsLdsSizeRegGs != UserDataNotMapped) &&
                       (pInfo->esGsLdsSizeRegVs != UserDataNotMapped));
            pInfo->loadedShRegCount += 2;
        }
    }
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.  Also uploads register state into GPU memory.
void PipelineChunkEsGs::LateInit(
    const AbiProcessor&             abiProcessor,
    const CodeObjectMetadata&       metadata,
    const RegisterVector&           registers,
    const GraphicsPipelineLoadInfo& loadInfo,
    GraphicsPipelineUploader*       pUploader,
    Util::MetroHash64*              pHasher)
{
    const bool useLoadIndexPath = pUploader->EnableLoadIndexPath();

    const Gfx6PalSettings&   settings  = m_device.Settings();
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    BuildPm4Headers(useLoadIndexPath, loadInfo.usesOnChipGs, loadInfo.esGsLdsSizeRegGs, loadInfo.esGsLdsSizeRegVs);

    Abi::PipelineSymbolEntry symbol = { };
    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::EsMainEntry, &symbol))
    {
        m_stageInfoEs.codeLength   = static_cast<size_t>(symbol.size);
        const gpusize programGpuVa = (pUploader->CodeGpuVirtAddr() + symbol.value);
        PAL_ASSERT(programGpuVa == Pow2Align(programGpuVa, 256));

        m_commands.sh.spiShaderPgmLoEs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_commands.sh.spiShaderPgmHiEs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::EsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (pUploader->DataGpuVirtAddr() + symbol.value);
        m_commands.sh.spiShaderUserDataLoEs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::EsDisassembly, &symbol))
    {
        m_stageInfoEs.disassemblyLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::GsMainEntry, &symbol))
    {
        m_stageInfoGs.codeLength   = static_cast<size_t>(symbol.size);
        const gpusize programGpuVa = (pUploader->CodeGpuVirtAddr() + symbol.value);
        PAL_ASSERT(programGpuVa == Pow2Align(programGpuVa, 256));

        m_commands.sh.spiShaderPgmLoGs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_commands.sh.spiShaderPgmHiGs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::GsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (pUploader->DataGpuVirtAddr() + symbol.value);
        m_commands.sh.spiShaderUserDataLoGs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::GsDisassembly, &symbol))
    {
        m_stageInfoGs.disassemblyLength = static_cast<size_t>(symbol.size);
    }

    m_commands.sh.spiShaderPgmRsrc1Es.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_ES);
    m_commands.sh.spiShaderPgmRsrc2Es.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_ES);
    registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_ES__CI__VI, &m_commands.dynamic.spiShaderPgmRsrc3Es.u32All);

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_ENABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    m_commands.sh.spiShaderPgmRsrc1Es.bits.CU_GROUP_ENABLE = (settings.esCuGroupEnabled ? 1 : 0);

    m_commands.sh.spiShaderPgmRsrc1Gs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_GS);
    m_commands.sh.spiShaderPgmRsrc2Gs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_GS);
    registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_GS__CI__VI, &m_commands.dynamic.spiShaderPgmRsrc3Gs.u32All);

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_ENABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    m_commands.sh.spiShaderPgmRsrc1Gs.bits.CU_GROUP_ENABLE = (settings.gsCuGroupEnabled ? 1 : 0);

    if (metadata.pipeline.hasEntry.esGsLdsSize != 0)
    {
        m_commands.sh.gsUserDataLdsEsGsSize.u32All = metadata.pipeline.esGsLdsSize;
        m_commands.sh.vsUserDataLdsEsGsSize.u32All = metadata.pipeline.esGsLdsSize;
    }

    m_commands.context.vgtGsMaxVertOut.u32All    = registers.At(mmVGT_GS_MAX_VERT_OUT);
    m_commands.context.vgtGsInstanceCnt.u32All   = registers.At(mmVGT_GS_INSTANCE_CNT);
    m_commands.context.vgtGsOutPrimType.u32All   = registers.At(mmVGT_GS_OUT_PRIM_TYPE);
    m_commands.context.vgtGsVertItemSize0.u32All = registers.At(mmVGT_GS_VERT_ITEMSIZE);
    m_commands.context.vgtGsVertItemSize1.u32All = registers.At(mmVGT_GS_VERT_ITEMSIZE_1);
    m_commands.context.vgtGsVertItemSize2.u32All = registers.At(mmVGT_GS_VERT_ITEMSIZE_2);
    m_commands.context.vgtGsVertItemSize3.u32All = registers.At(mmVGT_GS_VERT_ITEMSIZE_3);
    m_commands.context.ringOffset1.u32All        = registers.At(mmVGT_GSVS_RING_OFFSET_1);
    m_commands.context.ringOffset2.u32All        = registers.At(mmVGT_GSVS_RING_OFFSET_2);
    m_commands.context.ringOffset3.u32All        = registers.At(mmVGT_GSVS_RING_OFFSET_3);
    m_commands.context.gsVsRingItemsize.u32All   = registers.At(mmVGT_GSVS_RING_ITEMSIZE);
    m_commands.context.esGsRingItemsize.u32All   = registers.At(mmVGT_ESGS_RING_ITEMSIZE);
    m_commands.context.vgtGsOnchipCntl.u32All    = registers.At(mmVGT_GS_ONCHIP_CNTL__CI__VI);
    m_commands.context.vgtEsPerGs.u32All         = registers.At(mmVGT_ES_PER_GS);
    m_commands.context.vgtGsPerEs.u32All         = registers.At(mmVGT_GS_PER_ES);
    m_commands.context.vgtGsPerVs.u32All         = registers.At(mmVGT_GS_PER_VS);

    pHasher->Update(m_commands.context);

    if (chipProps.gfxLevel >= GfxIpLevel::GfxIp7)
    {
        // If we're using on-chip GS path, we need to avoid using CU1 for ES/GS waves to avoid a deadlock with the PS.
        // When on-chip GS is enabled, the HW-VS and HW-GS must run on the same CU as the HW-ES, since all communication
        // between the waves are done via LDS. This means that wherever the HW-ES launches is where the HW-VS
        // (copy shader) and HW-GS will launch.
        // This is a cause for deadlocks because when the HW-VS waves are trying to export, they are waiting for space
        // in the parameter cache, but that space is claimed by pending PS waves that can't launch on the CU due to
        // lack of space (already existing waves).
        uint16 disableCuMask = 0;
        if ((m_device.LateAllocVsLimit() > 0) && loadInfo.usesOnChipGs)
        {
            disableCuMask = 0x2;
        }

        m_commands.dynamic.spiShaderPgmRsrc3Es.bits.CU_EN =
            m_device.GetCuEnableMask(disableCuMask, settings.esCuEnLimitMask);
        m_commands.dynamic.spiShaderPgmRsrc3Gs.bits.CU_EN =
            m_device.GetCuEnableMask(disableCuMask, settings.gsCuEnLimitMask);
    }

    if (useLoadIndexPath)
    {
        pUploader->AddShReg(mmSPI_SHADER_PGM_LO_ES, m_commands.sh.spiShaderPgmLoEs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_HI_ES, m_commands.sh.spiShaderPgmHiEs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_LO_GS, m_commands.sh.spiShaderPgmLoGs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_HI_GS, m_commands.sh.spiShaderPgmHiGs);

        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC1_ES, m_commands.sh.spiShaderPgmRsrc1Es);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC2_ES, m_commands.sh.spiShaderPgmRsrc2Es);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC1_GS, m_commands.sh.spiShaderPgmRsrc1Gs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC2_GS, m_commands.sh.spiShaderPgmRsrc2Gs);

        pUploader->AddShReg(mmSPI_SHADER_USER_DATA_ES_0 + ConstBufTblStartReg, m_commands.sh.spiShaderUserDataLoEs);
        pUploader->AddShReg(mmSPI_SHADER_USER_DATA_GS_0 + ConstBufTblStartReg, m_commands.sh.spiShaderUserDataLoGs);

        if (loadInfo.usesOnChipGs)
        {
            pUploader->AddShReg(loadInfo.esGsLdsSizeRegGs, metadata.pipeline.esGsLdsSize);
            pUploader->AddShReg(loadInfo.esGsLdsSizeRegVs, metadata.pipeline.esGsLdsSize);
        }

        pUploader->AddCtxReg(mmVGT_GS_MAX_VERT_OUT,    m_commands.context.vgtGsMaxVertOut);
        pUploader->AddCtxReg(mmVGT_GS_INSTANCE_CNT,    m_commands.context.vgtGsInstanceCnt);
        pUploader->AddCtxReg(mmVGT_GS_OUT_PRIM_TYPE,   m_commands.context.vgtGsOutPrimType);
        pUploader->AddCtxReg(mmVGT_GS_VERT_ITEMSIZE,   m_commands.context.vgtGsVertItemSize0);
        pUploader->AddCtxReg(mmVGT_GS_VERT_ITEMSIZE_1, m_commands.context.vgtGsVertItemSize1);
        pUploader->AddCtxReg(mmVGT_GS_VERT_ITEMSIZE_2, m_commands.context.vgtGsVertItemSize2);
        pUploader->AddCtxReg(mmVGT_GS_VERT_ITEMSIZE_3, m_commands.context.vgtGsVertItemSize3);
        pUploader->AddCtxReg(mmVGT_GSVS_RING_OFFSET_1, m_commands.context.ringOffset1);
        pUploader->AddCtxReg(mmVGT_GSVS_RING_OFFSET_2, m_commands.context.ringOffset2);
        pUploader->AddCtxReg(mmVGT_GSVS_RING_OFFSET_3, m_commands.context.ringOffset3);
        pUploader->AddCtxReg(mmVGT_GSVS_RING_ITEMSIZE, m_commands.context.gsVsRingItemsize);
        pUploader->AddCtxReg(mmVGT_ESGS_RING_ITEMSIZE, m_commands.context.esGsRingItemsize);
        pUploader->AddCtxReg(mmVGT_ES_PER_GS,          m_commands.context.vgtEsPerGs);
        pUploader->AddCtxReg(mmVGT_GS_PER_ES,          m_commands.context.vgtGsPerEs);
        pUploader->AddCtxReg(mmVGT_GS_PER_VS,          m_commands.context.vgtGsPerVs);

        PAL_ASSERT(chipProps.gfxLevel >= GfxIpLevel::GfxIp7);
        pUploader->AddCtxReg(mmVGT_GS_ONCHIP_CNTL__CI__VI, m_commands.context.vgtGsOnchipCntl);
    }
}

// =====================================================================================================================
// Copies this pipeline chunk's PM4 sh commands into the specified command space. Returns the next unused
// DWORD in pCmdSpace.
template <bool UseLoadIndexPath>
uint32* PipelineChunkEsGs::WriteShCommands(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& esStageInfo,
    const DynamicStageInfo& gsStageInfo
    ) const
{
    if (UseLoadIndexPath == false)
    {
        pCmdSpace = pCmdStream->WritePm4Image(m_commands.sh.spaceNeeded, &m_commands.sh, pCmdSpace);
    }

    // NOTE: The dynamic register PM4 image headers will be zero if the GPU doesn't support these registers.
    if (m_commands.dynamic.hdrPgmRsrc3Es.header.u32All != 0)
    {
        auto dynamicCmds = m_commands.dynamic;

        if (esStageInfo.wavesPerSh > 0)
        {
            dynamicCmds.spiShaderPgmRsrc3Es.bits.WAVE_LIMIT = esStageInfo.wavesPerSh;
        }
        if (gsStageInfo.wavesPerSh > 0)
        {
            dynamicCmds.spiShaderPgmRsrc3Gs.bits.WAVE_LIMIT = gsStageInfo.wavesPerSh;
        }

        if (esStageInfo.cuEnableMask != 0)
        {
            dynamicCmds.spiShaderPgmRsrc3Es.bits.CU_EN &= esStageInfo.cuEnableMask;
        }
        if (gsStageInfo.cuEnableMask != 0)
        {
            dynamicCmds.spiShaderPgmRsrc3Gs.bits.CU_EN &= gsStageInfo.cuEnableMask;
        }

        constexpr uint32 SpaceNeededDynamic = sizeof(m_commands.dynamic) / sizeof(uint32);
        pCmdSpace = pCmdStream->WritePm4Image(SpaceNeededDynamic, &dynamicCmds, pCmdSpace);
    }

    if (m_pEsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_pEsPerfDataInfo->regOffset,
                                                                 m_pEsPerfDataInfo->gpuVirtAddr,
                                                                 pCmdSpace);
    }

    if (m_pGsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_pGsPerfDataInfo->regOffset,
                                                                 m_pGsPerfDataInfo->gpuVirtAddr,
                                                                 pCmdSpace);
    }

    return pCmdSpace;
}

// Instantiate template versions for the linker.
template
uint32* PipelineChunkEsGs::WriteShCommands<false>(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& esStageInfo,
    const DynamicStageInfo& gsStageInfo
    ) const;
template
uint32* PipelineChunkEsGs::WriteShCommands<true>(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& esStageInfo,
    const DynamicStageInfo& gsStageInfo
    ) const;

// =====================================================================================================================
// Copies this pipeline chunk's context commands into the specified command space. Returns the next unused
// DWORD in pCmdSpace.
template <bool UseLoadIndexPath>
uint32* PipelineChunkEsGs::WriteContextCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // NOTE: It is expected that this function will only ever be called when the set path is in use.
    PAL_ASSERT(UseLoadIndexPath == false);

    return pCmdStream->WritePm4Image(m_commands.context.spaceNeeded, &m_commands.context, pCmdSpace);
}

// Instantiate template versions for the linker.
template
uint32* PipelineChunkEsGs::WriteContextCommands<false>(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const;
template
uint32* PipelineChunkEsGs::WriteContextCommands<true>(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const;

// =====================================================================================================================
// Assembles the PM4 headers for the commands in this pipeline chunk.
void PipelineChunkEsGs::BuildPm4Headers(
    bool   enableLoadIndexPath,
    bool   useOnchipGs,
    uint16 esGsLdsSizeRegGs,
    uint16 esGsLdsSizeRegVs)
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();
    const CmdUtil&           cmdUtil   = m_device.CmdUtil();

    m_commands.sh.spaceNeeded = cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_LO_ES,
                                                          mmSPI_SHADER_PGM_RSRC2_ES,
                                                          ShaderGraphics,
                                                          &m_commands.sh.hdrSpiShaderPgmEs);

    m_commands.sh.spaceNeeded += cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_ES_0 + ConstBufTblStartReg,
                                                          ShaderGraphics,
                                                          &m_commands.sh.hdrSpiShaderUserDataEs);

    m_commands.sh.spaceNeeded += cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_LO_GS,
                                                           mmSPI_SHADER_PGM_RSRC2_GS,
                                                           ShaderGraphics,
                                                           &m_commands.sh.hdrSpiShaderPgmGs);

    m_commands.sh.spaceNeeded += cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_GS_0 + ConstBufTblStartReg,
                                                          ShaderGraphics,
                                                          &m_commands.sh.hdrSpiShaderUserDataGs);

    m_commands.context.spaceNeeded = cmdUtil.BuildSetOneContextReg(mmVGT_GS_MAX_VERT_OUT,
                                                                   &m_commands.context.hdrVgtGsMaxVertOut);

    m_commands.context.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_GS_OUT_PRIM_TYPE,
                                                                    &m_commands.context.hdrVgtGsOutPrimType);

    m_commands.context.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_GS_INSTANCE_CNT,
                                                                    &m_commands.context.hdrVgtGsInstanceCnt);

    m_commands.context.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmVGT_GS_PER_ES, mmVGT_GS_PER_VS,
                                                                     &m_commands.context.hdrVgtGsPerEs);

    m_commands.context.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmVGT_GS_VERT_ITEMSIZE,
                                                                     mmVGT_GS_VERT_ITEMSIZE_3,
                                                                     &m_commands.context.hdrVgtGsVertItemSize);

    m_commands.context.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmVGT_ESGS_RING_ITEMSIZE,
                                                                     mmVGT_GSVS_RING_ITEMSIZE,
                                                                     &m_commands.context.hdrRingItemsize);

    m_commands.context.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmVGT_GSVS_RING_OFFSET_1,
                                                                     mmVGT_GSVS_RING_OFFSET_3,
                                                                     &m_commands.context.hdrRingOffset);

    if (chipProps.gfxLevel >= GfxIpLevel::GfxIp7)
    {
        // We must use the SET_SH_REG_INDEX packet to support the real-time compute feature.
        cmdUtil.BuildSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_ES__CI__VI,
                                      ShaderGraphics,
                                      SET_SH_REG_INDEX_CP_MODIFY_CU_MASK,
                                      &m_commands.dynamic.hdrPgmRsrc3Es);
        cmdUtil.BuildSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_GS__CI__VI,
                                      ShaderGraphics,
                                      SET_SH_REG_INDEX_CP_MODIFY_CU_MASK,
                                      &m_commands.dynamic.hdrPgmRsrc3Gs);

        // NOTE: It is unclear whether we need to write this register if a pipeline uses offchip GS mode.
        m_commands.context.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_GS_ONCHIP_CNTL__CI__VI,
                                                                        &m_commands.context.hdrGsOnchipCnt);

        if (useOnchipGs)
        {
            m_commands.sh.spaceNeeded +=
                cmdUtil.BuildSetOneShReg(esGsLdsSizeRegGs, ShaderGraphics, &m_commands.sh.hdrGsUserData);
            m_commands.sh.spaceNeeded +=
                cmdUtil.BuildSetOneShReg(esGsLdsSizeRegVs, ShaderGraphics, &m_commands.sh.hdrVsUserData);
        }
    } // if gfxIpLevel >= GfxIp7
}

} // Gfx6
} // Pal
