/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkPs.h"
#include "core/platform.h"
#include "palPipeline.h"
#include "palPipelineAbiProcessorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
PipelineChunkPs::PipelineChunkPs(
    const Device& device)
    :
    m_device(device),
    m_pPsPerfDataInfo(nullptr)
{
    memset(&m_pm4ImageSh,        0, sizeof(m_pm4ImageSh));
    memset(&m_pm4ImageShDynamic, 0, sizeof(m_pm4ImageShDynamic));
    memset(&m_pm4ImageContext,   0, sizeof(m_pm4ImageContext));
    memset(&m_stageInfo,         0, sizeof(m_stageInfo));
    m_stageInfo.stageId = Abi::HardwareStage::Ps;
}

// =====================================================================================================================
// Initializes this pipeline chunk.
void PipelineChunkPs::Init(
    const AbiProcessor& abiProcessor,
    const PsParams&     params)
{
    const Gfx9PalSettings& settings = m_device.Settings();

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

    BuildPm4Headers(lastPsInterpolator);

    m_pm4ImageSh.spiShaderPgmRsrc1Ps.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC1_PS);
    m_pm4ImageSh.spiShaderPgmRsrc2Ps.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_PGM_RSRC2_PS);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 345
    abiProcessor.HasRegisterEntry(mmSPI_SHADER_PGM_RSRC3_PS, &m_pm4ImageShDynamic.spiShaderPgmRsrc3Ps.u32All);
#endif

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_DISABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    m_pm4ImageSh.spiShaderPgmRsrc1Ps.bits.CU_GROUP_DISABLE = (settings.psCuGroupEnabled ? 0 : 1);

    if (m_device.Parent()->ChipProperties().gfx9.supportSpp != 0)
    {
        abiProcessor.HasRegisterEntry(mmSPI_SHADER_PGM_CHKSUM_PS,
                                      &m_pm4ImageSh.spiShaderPgmChksumPs.u32All);
    }

    m_pm4ImageShDynamic.spiShaderPgmRsrc3Ps.bits.CU_EN      = m_device.GetCuEnableMask(0, settings.psCuEnLimitMask);

    m_pm4ImageContext.dbShaderControl.u32All    = abiProcessor.GetRegisterEntry(mmDB_SHADER_CONTROL);
    m_pm4ImageContext.paScAaConfig.reg_data     = abiProcessor.GetRegisterEntry(mmPA_SC_AA_CONFIG);
    m_pm4ImageContext.paScShaderControl.u32All  = abiProcessor.GetRegisterEntry(mmPA_SC_SHADER_CONTROL);
    m_pm4ImageContext.spiBarycCntl.u32All       = abiProcessor.GetRegisterEntry(mmSPI_BARYC_CNTL);
    m_pm4ImageContext.spiPsInputAddr.u32All     = abiProcessor.GetRegisterEntry(mmSPI_PS_INPUT_ADDR);
    m_pm4ImageContext.spiPsInputEna.u32All      = abiProcessor.GetRegisterEntry(mmSPI_PS_INPUT_ENA);
    m_pm4ImageContext.spiShaderColFormat.u32All = abiProcessor.GetRegisterEntry(mmSPI_SHADER_COL_FORMAT);
    m_pm4ImageContext.spiShaderZFormat.u32All   = abiProcessor.GetRegisterEntry(mmSPI_SHADER_Z_FORMAT);
    m_pm4ImageContext.paScConservativeRastCntl.reg_data =
            abiProcessor.GetRegisterEntry(mmPA_SC_CONSERVATIVE_RASTERIZATION_CNTL);

    // Override the Pipeline ABI's reported COVERAGE_AA_MASK_ENABLE bit if the settings request it.
    if (settings.disableCoverageAaMask)
    {
        m_pm4ImageContext.paScConservativeRastCntl.reg_data &=
            ~PA_SC_CONSERVATIVE_RASTERIZATION_CNTL__COVERAGE_AA_MASK_ENABLE_MASK;
    }

    // Binner_cntl1:
    // 16 bits: Maximum amount of parameter storage allowed per batch.
    // - Legacy: param cache lines/2 (groups of 16 vert-attributes) (0 means 1 encoding)
    // - NGG: number of vert-attributes (0 means 1 encoding)
    // - NGG + PC: param cache lines/2 (groups of 16 vert-attributes) (0 means 1 encoding)
    // 16 bits: Max number of primitives in batch
    m_pm4ImageContext.paScBinnerCntl1.u32All = 0;
    m_pm4ImageContext.paScBinnerCntl1.bits.MAX_PRIM_PER_BATCH = settings.binningMaxPrimPerBatch - 1;

    if (params.isNgg)
    {
        // If we add support for off-chip parameter cache this code will need to be updated as well.
        PAL_ALERT(m_device.Parent()->ChipProperties().gfx9.primShaderInfo.parameterCacheSize != 0);

        m_pm4ImageContext.paScBinnerCntl1.bits.MAX_ALLOC_COUNT = settings.binningMaxAllocCountNggOnChip - 1;
    }
    else
    {
        m_pm4ImageContext.paScBinnerCntl1.bits.MAX_ALLOC_COUNT = settings.binningMaxAllocCountLegacy - 1;
    }

    // Compute the checksum here because we don't want it to include the GPU virtual addresses!
    params.pHasher->Update(m_pm4ImageContext);

    Abi::PipelineSymbolEntry symbol = { };
    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::PsMainEntry, &symbol))
    {
        const gpusize programGpuVa = (symbol.value + params.codeGpuVirtAddr);
        PAL_ASSERT(programGpuVa == Pow2Align(programGpuVa, 256));

        m_pm4ImageSh.spiShaderPgmLoPs.bits.MEM_BASE = Get256BAddrLo(programGpuVa);
        m_pm4ImageSh.spiShaderPgmHiPs.bits.MEM_BASE = Get256BAddrHi(programGpuVa);

        m_stageInfo.codeLength = static_cast<size_t>(symbol.size);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::PsShdrIntrlTblPtr, &symbol))
    {
        const gpusize srdTableGpuVa = (symbol.value + params.dataGpuVirtAddr);
        m_pm4ImageSh.spiShaderUserDataLoPs.bits.DATA = LowPart(srdTableGpuVa);
    }

    if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::PsDisassembly, &symbol))
    {
        m_stageInfo.disassemblyLength = static_cast<size_t>(symbol.size);
    }
}

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkPs::WriteShCommands(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& vsStageInfo
    ) const
{
    Pm4ImageShDynamic pm4ImageShDynamic = m_pm4ImageShDynamic;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 345
    if (m_pm4ImageShDynamic.spiShaderPgmRsrc3Ps.bits.WAVE_LIMIT == 0)
#endif
    {
        pm4ImageShDynamic.spiShaderPgmRsrc3Ps.bits.WAVE_LIMIT = vsStageInfo.wavesPerSh;
    }

    if (vsStageInfo.cuEnableMask != 0)
    {
        pm4ImageShDynamic.spiShaderPgmRsrc3Ps.bits.CU_EN &= vsStageInfo.cuEnableMask;
    }

    pCmdSpace = pCmdStream->WritePm4Image(m_pm4ImageSh.spaceNeeded, &m_pm4ImageSh, pCmdSpace);
    pCmdSpace = pCmdStream->WritePm4Image(pm4ImageShDynamic.spaceNeeded, &pm4ImageShDynamic, pCmdSpace);

    if (m_pPsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_pPsPerfDataInfo->regOffset,
                                                                 m_pPsPerfDataInfo->gpuVirtAddr,
                                                                 pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Copies this pipeline chunk's context commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkPs::WriteContextCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    pCmdSpace = pCmdStream->WritePm4Image(m_pm4ImageContext.spaceNeeded, &m_pm4ImageContext, pCmdSpace);
    return pCmdSpace;
}

// =====================================================================================================================
// Assembles the PM4 headers for the commands in this pipeline chunk.
void PipelineChunkPs::BuildPm4Headers(
    uint32 lastPsInterpolator)
{
    const CmdUtil& cmdUtil = m_device.CmdUtil();

    // Sets the following SH registers: SPI_SHADER_PGM_LO_PS, SPI_SHADER_PGM_HI_PS,
    // SPI_SHADER_PGM_RSRC1_PS, SPI_SHADER_PGM_RSRC2_PS.
    m_pm4ImageSh.spaceNeeded = cmdUtil.BuildSetSeqShRegs(mmSPI_SHADER_PGM_LO_PS,
                                                         mmSPI_SHADER_PGM_RSRC2_PS,
                                                         ShaderGraphics,
                                                         &m_pm4ImageSh.hdrSpiShaderPgm);

    // Sets the following SH register: SPI_SHADER_USER_DATA_PS_1.
    m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_PS_0 + ConstBufTblStartReg,
                                                         ShaderGraphics,
                                                         &m_pm4ImageSh.hdrSpiShaderUserData);

    // Sets the following SH register: SPI_SHADER_PGM_RSRC3_PS.
    // We must use the SET_SH_REG_INDEX packet to support the real-time compute feature.
    m_pm4ImageShDynamic.spaceNeeded = cmdUtil.BuildSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_PS,
                                                                    ShaderGraphics,
                                                                    index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                                    &m_pm4ImageShDynamic.hdrPgmRsrc3Ps);

    // Sets the following context register:
    // SPI_SHADER_Z_FORMAT, SPI_SHADER_COL_FORMAT.
    m_pm4ImageContext.spaceNeeded = cmdUtil.BuildSetSeqContextRegs(mmSPI_SHADER_Z_FORMAT,
                                                                   mmSPI_SHADER_COL_FORMAT,
                                                                   &m_pm4ImageContext.hdrSpiShaderFormat);

    // Sets the following context register: SPI_BARYC_CNTL.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmSPI_BARYC_CNTL,
                                                                   &m_pm4ImageContext.hdrSpiBarycCntl);

    // Sets the following context registers: SPI_PS_INPUT_ENA, SPI_PS_INPUT_ADDR.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetSeqContextRegs(mmSPI_PS_INPUT_ENA,
                                                                    mmSPI_PS_INPUT_ADDR,
                                                                    &m_pm4ImageContext.hdrSpiPsInput);

    // Sets the following context register: DB_SHADER_CONTROL.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmDB_SHADER_CONTROL,
                                                                   &m_pm4ImageContext.hdrDbShaderControl);

    // Sets the following context register: PA_SC_SHADER_CONTROL.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmPA_SC_SHADER_CONTROL,
                                                                   &m_pm4ImageContext.hdrPaScShaderControl);

    // Sets the following context register: PA_SC_BINNER_CNTL_1.
    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildSetOneContextReg(mmPA_SC_BINNER_CNTL_1,
                                                                   &m_pm4ImageContext.hdrPaScBinnerCntl1);

    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildContextRegRmw(
            mmPA_SC_AA_CONFIG,
            static_cast<uint32>(PA_SC_AA_CONFIG__COVERAGE_TO_SHADER_SELECT_MASK),
            0,
            &m_pm4ImageContext.paScAaConfig);

    m_pm4ImageContext.spaceNeeded += cmdUtil.BuildContextRegRmw(
            mmPA_SC_CONSERVATIVE_RASTERIZATION_CNTL,
            static_cast<uint32>(PA_SC_CONSERVATIVE_RASTERIZATION_CNTL__COVERAGE_AA_MASK_ENABLE_MASK |
                                PA_SC_CONSERVATIVE_RASTERIZATION_CNTL__UNDER_RAST_ENABLE_MASK),
            0, // filled in by the "Init" function
            &m_pm4ImageContext.paScConservativeRastCntl);

    // Sets the following context registers: SPI_PS_INPUT_CNTL_0 - SPI_PS_INPUT_CNTL_X.
    m_pm4ImageContext.spaceNeeded += m_device.CmdUtil().BuildSetSeqContextRegs(mmSPI_PS_INPUT_CNTL_0,
                                                                               lastPsInterpolator,
                                                                               &m_pm4ImageContext.hdrSpiPsInputCntl);

    // Sets the following SH register: SPI_SHADER_PGM_CHKSUM_PS.
    if (m_device.Parent()->ChipProperties().gfx9.supportSpp != 0)
    {
        m_pm4ImageSh.spaceNeeded += cmdUtil.BuildSetOneShReg(mmSPI_SHADER_PGM_CHKSUM_PS,
                                                             ShaderGraphics,
                                                             &m_pm4ImageSh.hdrSpiShaderPgmChksum);
    }
}

} // Gfx9
} // Pal
