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

#include "core/platform.h"
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6PipelineChunkEsGs.h"
#include "palPipeline.h"
#include "palPipelineAbiProcessorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
PipelineChunkEsGs::PipelineChunkEsGs(
    const Device& device)
    :
    m_device(device),
    m_pEsPerfDataInfo(nullptr),
    m_pGsPerfDataInfo(nullptr)
{
    memset(&m_pm4ImageSh,        0, sizeof(m_pm4ImageSh));
    memset(&m_pm4ImageShDynamic, 0, sizeof(m_pm4ImageShDynamic));
    memset(&m_pm4ImageContext,   0, sizeof(m_pm4ImageContext));
    memset(&m_stageInfoEs,       0, sizeof(m_stageInfoEs));
    memset(&m_stageInfoGs,       0, sizeof(m_stageInfoGs));

    m_stageInfoEs.stageId = Abi::HardwareStage::Es;
    m_stageInfoGs.stageId = Abi::HardwareStage::Gs;
}

// =====================================================================================================================
// Initializes this pipeline chunk using RelocatableShader objects representing the ES & GS hardware stages.
void PipelineChunkEsGs::Init(
    const AbiProcessor& abiProcessor,
    const EsGsParams&   params)
{
    const Gfx6PalSettings&   settings = m_device.Settings();
    const GpuChipProperties& chipInfo = m_device.Parent()->ChipProperties();

    m_pEsPerfDataInfo = params.pEsPerfDataInfo;
    m_pGsPerfDataInfo = params.pGsPerfDataInfo;

    BuildPm4Headers(params.usesOnChipGs, params.esGsLdsSizeRegGs, params.esGsLdsSizeRegVs);

    m_pm4ImageSh.spiShaderPgmRsrc1Es.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC1_ES);
    m_pm4ImageSh.spiShaderPgmRsrc2Es.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC2_ES);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 345
    abiProcessor.HasRegisterEntry(mmSPI_SHADER_PGM_RSRC3_ES__CI__VI, &m_pm4ImageShDynamic.spiShaderPgmRsrc3Es.u32All);
#endif

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_ENABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    m_pm4ImageSh.spiShaderPgmRsrc1Es.bits.CU_GROUP_ENABLE = (settings.esCuGroupEnabled ? 1 : 0);

    m_pm4ImageSh.spiShaderPgmRsrc1Gs.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC1_GS);
    m_pm4ImageSh.spiShaderPgmRsrc2Gs.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC2_GS);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 345
    abiProcessor.HasRegisterEntry(mmSPI_SHADER_PGM_RSRC3_GS__CI__VI, &m_pm4ImageShDynamic.spiShaderPgmRsrc3Gs.u32All);
#endif

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_ENABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    m_pm4ImageSh.spiShaderPgmRsrc1Gs.bits.CU_GROUP_ENABLE = (settings.gsCuGroupEnabled ? 1 : 0);

    uint32 esGsLdsSizeBytes = 0;
    if (abiProcessor.HasPipelineMetadataEntry(Abi::PipelineMetadataType::EsGsLdsByteSize, &esGsLdsSizeBytes))
    {
        m_pm4ImageSh.gsUserDataLdsEsGsSize.u32All = esGsLdsSizeBytes;
        m_pm4ImageSh.vsUserDataLdsEsGsSize.u32All = esGsLdsSizeBytes;
    }

    m_pm4ImageContext.vgtGsMaxVertOut.u32All    = abiProcessor.GetRegisterEntry(mmVGT_GS_MAX_VERT_OUT);
    m_pm4ImageContext.vgtGsInstanceCnt.u32All   = abiProcessor.GetRegisterEntry(mmVGT_GS_INSTANCE_CNT);
    m_pm4ImageContext.vgtGsOutPrimType.u32All   = abiProcessor.GetRegisterEntry(mmVGT_GS_OUT_PRIM_TYPE);
    m_pm4ImageContext.vgtGsVertItemSize0.u32All = abiProcessor.GetRegisterEntry(mmVGT_GS_VERT_ITEMSIZE);
    m_pm4ImageContext.vgtGsVertItemSize1.u32All = abiProcessor.GetRegisterEntry(mmVGT_GS_VERT_ITEMSIZE_1);
    m_pm4ImageContext.vgtGsVertItemSize2.u32All = abiProcessor.GetRegisterEntry(mmVGT_GS_VERT_ITEMSIZE_2);
    m_pm4ImageContext.vgtGsVertItemSize3.u32All = abiProcessor.GetRegisterEntry(mmVGT_GS_VERT_ITEMSIZE_3);
    m_pm4ImageContext.ringOffset1.u32All        = abiProcessor.GetRegisterEntry(mmVGT_GSVS_RING_OFFSET_1);
    m_pm4ImageContext.ringOffset2.u32All        = abiProcessor.GetRegisterEntry(mmVGT_GSVS_RING_OFFSET_2);
    m_pm4ImageContext.ringOffset3.u32All        = abiProcessor.GetRegisterEntry(mmVGT_GSVS_RING_OFFSET_3);
    m_pm4ImageContext.gsVsRingItemsize.u32All   = abiProcessor.GetRegisterEntry(mmVGT_GSVS_RING_ITEMSIZE);
    m_pm4ImageContext.esGsRingItemsize.u32All   = abiProcessor.GetRegisterEntry(mmVGT_ESGS_RING_ITEMSIZE);
    m_pm4ImageContext.vgtGsOnchipCntl.u32All    = abiProcessor.GetRegisterEntry(mmVGT_GS_ONCHIP_CNTL__CI__VI);
    m_pm4ImageContext.vgtEsPerGs.u32All         = abiProcessor.GetRegisterEntry(mmVGT_ES_PER_GS);
    m_pm4ImageContext.vgtGsPerEs.u32All         = abiProcessor.GetRegisterEntry(mmVGT_GS_PER_ES);
    m_pm4ImageContext.vgtGsPerVs.u32All         = abiProcessor.GetRegisterEntry(mmVGT_GS_PER_VS);

    if (chipInfo.gfxLevel >= GfxIpLevel::GfxIp7)
    {
        m_pm4ImageShDynamic.spiShaderPgmRsrc3Es.bits.CU_EN = m_device.GetCuEnableMask(0, settings.esCuEnLimitMask);
        m_pm4ImageShDynamic.spiShaderPgmRsrc3Gs.bits.CU_EN = m_device.GetCuEnableMask(0, settings.gsCuEnLimitMask);
    }

    // Compute the checksum here because we don't want it to include the GPU virtual addresses!
    params.pHasher->Update(m_pm4ImageContext);

    Abi::PipelineSymbolEntry symbol = { };
    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::EsMainEntry, &symbol))
    {
        const gpusize programGpuVa = (symbol.value + params.codeGpuVirtAddr);
        PAL_ASSERT(programGpuVa == Pow2Align(programGpuVa, 256));

        m_pm4ImageSh.spiShaderPgmLoEs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_pm4ImageSh.spiShaderPgmHiEs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);

        m_stageInfoEs.codeLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::EsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (symbol.value + params.dataGpuVirtAddr);
        m_pm4ImageSh.spiShaderUserDataLoEs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::EsDisassembly, &symbol))
    {
        m_stageInfoEs.disassemblyLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::GsMainEntry, &symbol))
    {
        const gpusize programGpuVa = (symbol.value + params.codeGpuVirtAddr);
        PAL_ASSERT(programGpuVa == Pow2Align(programGpuVa, 256));

        m_pm4ImageSh.spiShaderPgmLoGs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_pm4ImageSh.spiShaderPgmHiGs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);

        m_stageInfoGs.codeLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::GsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (symbol.value + params.dataGpuVirtAddr);
        m_pm4ImageSh.spiShaderUserDataLoGs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::GsDisassembly, &symbol))
    {
        m_stageInfoGs.disassemblyLength = static_cast<size_t>(symbol.size);
    }
}

// =====================================================================================================================
// Copies this pipeline chunk's PM4 sh commands into the specified command space. Returns the next unused
// DWORD in pCmdSpace.
uint32* PipelineChunkEsGs::WriteShCommands(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& esStageInfo,
    const DynamicStageInfo& gsStageInfo
    ) const
{
    pCmdSpace = pCmdStream->WritePm4Image(m_pm4ImageSh.spaceNeeded, &m_pm4ImageSh, pCmdSpace);

    if (m_pm4ImageShDynamic.spaceNeeded > 0)
    {
        Pm4ImageShDynamic pm4ImageShDynamic = m_pm4ImageShDynamic;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 345
        if (pm4ImageShDynamic.spiShaderPgmRsrc3Es.bits.WAVE_LIMIT == 0)
#endif
        {
            pm4ImageShDynamic.spiShaderPgmRsrc3Es.bits.WAVE_LIMIT = esStageInfo.wavesPerSh;
            pm4ImageShDynamic.spiShaderPgmRsrc3Gs.bits.WAVE_LIMIT = gsStageInfo.wavesPerSh;
        }

        if (esStageInfo.cuEnableMask != 0)
        {
            pm4ImageShDynamic.spiShaderPgmRsrc3Es.bits.CU_EN &= esStageInfo.cuEnableMask;
        }
        if (gsStageInfo.cuEnableMask != 0)
        {
            pm4ImageShDynamic.spiShaderPgmRsrc3Gs.bits.CU_EN &= gsStageInfo.cuEnableMask;
        }

        pCmdSpace = pCmdStream->WritePm4Image(pm4ImageShDynamic.spaceNeeded, &pm4ImageShDynamic, pCmdSpace);
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

// =====================================================================================================================
// Copies this pipeline chunk's context commands into the specified command space. Returns the next unused
// DWORD in pCmdSpace.
uint32* PipelineChunkEsGs::WriteContextCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    pCmdSpace = pCmdStream->WritePm4Image(m_pm4ImageContext.spaceNeeded, &m_pm4ImageContext, pCmdSpace);
    return pCmdSpace;
}

// =====================================================================================================================
// Assembles the PM4 headers for the commands in this pipeline chunk.
void PipelineChunkEsGs::BuildPm4Headers(
    bool   useOnchipGs,
    uint16 esGsLdsSizeRegGs,
    uint16 esGsLdsSizeRegVs)
{
    const CmdUtil& cmdUtil = m_device.CmdUtil();

    // Sets the following SH registers: SPI_SHADER_PGM_LO_ES, SPI_SHADER_PGM_HI_ES,
    // SPI_SHADER_PGM_RSRC1_ES, SPI_SHADER_PGM_RSRC2_ES.
    m_pm4ImageSh.spaceNeeded = cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_LO_ES,
                                                         mmSPI_SHADER_PGM_RSRC2_ES,
                                                         ShaderGraphics,
                                                         &m_pm4ImageSh.hdrSpiShaderPgmEs);

    // Sets the following SH register: SPI_SHADER_USER_DATA_ES_1.
    m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_ES_0 + ConstBufTblStartReg,
                                                         ShaderGraphics,
                                                         &m_pm4ImageSh.hdrSpiShaderUserDataEs);

    // Sets the following SH registers: SPI_SHADER_PGM_LO_GS, SPI_SHADER_PGM_HI_GS,
    // SPI_SHADER_PGM_RSRC1_GS, SPI_SHADER_PGM_RSRC2_GS.
    m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_LO_GS,
                                                          mmSPI_SHADER_PGM_RSRC2_GS,
                                                          ShaderGraphics,
                                                          &m_pm4ImageSh.hdrSpiShaderPgmGs);

    // Sets the following SH register: SPI_SHADER_USER_DATA_GS_1.
    m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_GS_0 + ConstBufTblStartReg,
                                                         ShaderGraphics,
                                                         &m_pm4ImageSh.hdrSpiShaderUserDataGs);

    // Sets the following context register: VGT_GS_MAX_VERT_OUT.
    m_pm4ImageContext.spaceNeeded = cmdUtil.BuildSetOneContextReg(mmVGT_GS_MAX_VERT_OUT,
                                                                  &m_pm4ImageContext.hdrVgtGsMaxVertOut);

    // Sets the following context register: VGT_GS_OUT_PRIM_TYPE.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_GS_OUT_PRIM_TYPE,
                                                                   &m_pm4ImageContext.hdrVgtGsOutPrimType);

    // Sets the following context register: VGT_GS_INSTANCE_CNT.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_GS_INSTANCE_CNT,
                                                                   &m_pm4ImageContext.hdrVgtGsInstanceCnt);

    // Sets the following context registers: VGT_GS_PER_ES, VGT_ES_PER_GS, and VGT_GS_PER_VS.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmVGT_GS_PER_ES, mmVGT_GS_PER_VS,
                                                                    &m_pm4ImageContext.hdrVgtGsPerEs);

    // Sets the following context registers: VGT_GS_VERT_ITEMSIZE, VGT_GS_VERT_ITEMSIZE_1,
    // VGT_GS_VERT_ITEMSIZE_2, VGT_GS_VERT_ITEMSIZE_3.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmVGT_GS_VERT_ITEMSIZE, mmVGT_GS_VERT_ITEMSIZE_3,
                                                                    &m_pm4ImageContext.hdrVgtGsVertItemSize);

    // Sets the context registers: VGT_ESGS_RING_ITEMSIZE and VGT_GSVS_RING_ITEMSIZE.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmVGT_ESGS_RING_ITEMSIZE,
                                                                    mmVGT_GSVS_RING_ITEMSIZE,
                                                                    &m_pm4ImageContext.hdrRingItemsize);

    // Sets the context registers VGT_GSVS_RING_OFFSET_1, VGT_GSVS_RING_OFFSET_2, and VGT_GSVS_RING_OFFSET_3;
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmVGT_GSVS_RING_OFFSET_1,
                                                                    mmVGT_GSVS_RING_OFFSET_3,
                                                                    &m_pm4ImageContext.hdrRingOffset);

    if (m_device.Parent()->ChipProperties().gfxLevel > GfxIpLevel::GfxIp6)
    {
        // Sets the following SH register: SPI_SHADER_PGM_RSRC3_ES.
        // We must use the SET_SH_REG_INDEX packet to support the real-time compute feature.
        m_pm4ImageShDynamic.spaceNeeded = cmdUtil.BuildSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_ES__CI__VI,
                                                                        ShaderGraphics,
                                                                        SET_SH_REG_INDEX_CP_MODIFY_CU_MASK,
                                                                        &m_pm4ImageShDynamic.hdrPgmRsrc3Es);

        // Sets the following SH register: SPI_SHADER_PGM_RSRC3_GS.
        // We must use the SET_SH_REG_INDEX packet to support the real-time compute feature.
        m_pm4ImageShDynamic.spaceNeeded += cmdUtil.BuildSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_GS__CI__VI,
                                                                         ShaderGraphics,
                                                                         SET_SH_REG_INDEX_CP_MODIFY_CU_MASK,
                                                                         &m_pm4ImageShDynamic.hdrPgmRsrc3Gs);

        // Note: It is unclear whether we need to write this register if a pipeline uses offchip GS mode.  DXX seems to
        // always write the register for Sea Islands and newer hardware.

        // Sets the following context register: VGT_GS_ONCHIP_CNTL
        m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmVGT_GS_ONCHIP_CNTL__CI__VI,
                                                                       &m_pm4ImageContext.hdrGsOnchipCnt);

        if (useOnchipGs)
        {
            // Sets the following SH registers: SPI_SHADER_USER_DATA_GS_N
            m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetOneShReg(esGsLdsSizeRegGs,
                                                                 ShaderGraphics,
                                                                 &m_pm4ImageSh.hdrGsUserData);

            // Sets the following SH registers: SPI_SHADER_USER_DATA_VS_N.
            m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetOneShReg(esGsLdsSizeRegVs,
                                                                 ShaderGraphics,
                                                                 &m_pm4ImageSh.hdrVsUserData);
        }
    }
}

} // Gfx6
} // Pal
