/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9AbiToPipelineRegisters.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkVsPs.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsShaderLibrary.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// Stream-out vertex stride register addresses.
constexpr uint16 VgtStrmoutVtxStrideAddr[] =
{
    Gfx10::mmVGT_STRMOUT_VTX_STRIDE_0,
    Gfx10::mmVGT_STRMOUT_VTX_STRIDE_1,
    Gfx10::mmVGT_STRMOUT_VTX_STRIDE_2,
    Gfx10::mmVGT_STRMOUT_VTX_STRIDE_3
};

// =====================================================================================================================
PipelineChunkVsPs::PipelineChunkVsPs(
    const Device&       device,
    const PerfDataInfo* pVsPerfDataInfo,
    const PerfDataInfo* pPsPerfDataInfo)
    :
    m_device(device),
    m_regs{},
    m_semanticInfo{},
    m_semanticCount(0),
    m_pVsPerfDataInfo(pVsPerfDataInfo),
    m_pPsPerfDataInfo(pPsPerfDataInfo),
    m_stageInfoVs{},
    m_stageInfoPs{},
    m_colorExportAddr{}
{
    m_paScAaConfig.u32All = 0;

    m_stageInfoVs.stageId = Abi::HardwareStage::Vs;
    m_stageInfoPs.stageId = Abi::HardwareStage::Ps;
    m_psWaveFrontSize = 32;
    m_regs.sh.userDataInternalTableVs.u32All = InvalidUserDataInternalTable;
    m_regs.sh.userDataInternalTablePs.u32All = InvalidUserDataInternalTable;
}

// =====================================================================================================================
// Early initialization for this pipeline chunk.  Responsible for determining the number of SH and context registers to
// be loaded using LOAD_CNTX_REG_INDEX and LOAD_SH_REG_INDEX.
void PipelineChunkVsPs::EarlyInit(
    const PalAbi::CodeObjectMetadata& metadata,
    GraphicsPipelineLoadInfo*         pInfo)
{
    PAL_ASSERT(pInfo != nullptr);

    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    if (chipProps.gfxip.supportsHwVs)
    {
        m_regs.context.vgtStrmoutConfig.u32All = AbiRegisters::VgtStrmoutConfig(metadata);
    }

    // Determine the number of PS interpolators and save them for LateInit to consume.
    AbiRegisters::SpiPsInputCntl(metadata,
                                 chipProps.gfxLevel,
                                 &m_regs.context.spiPsInputCntl[0],
                                 &m_regs.context.interpolatorCount);
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.
void PipelineChunkVsPs::LateInit(
    const AbiReader&                    abiReader,
    const PalAbi::CodeObjectMetadata&   metadata,
    const GraphicsPipelineLoadInfo&     loadInfo,
    const GraphicsPipelineCreateInfo&   createInfo,
    CodeObjectUploader*                 pUploader)
{
    const Gfx9PalSettings&   settings  = m_device.Settings();
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    GpuSymbol symbol = { };
    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::PsMainEntry, &symbol) == Result::Success)
    {
        m_stageInfoPs.codeLength   = static_cast<size_t>(symbol.size);
        PAL_ASSERT(symbol.gpuVirtAddr == Pow2Align(symbol.gpuVirtAddr, 256));

        m_regs.sh.spiShaderPgmLoPs.bits.MEM_BASE = Get256BAddrLo(symbol.gpuVirtAddr);
        m_regs.sh.spiShaderPgmHiPs.bits.MEM_BASE = Get256BAddrHi(symbol.gpuVirtAddr);
        PAL_ASSERT(m_regs.sh.spiShaderPgmHiPs.u32All == 0);
    }

    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::PsShdrIntrlTblPtr, &symbol) == Result::Success)
    {
        m_regs.sh.userDataInternalTablePs.bits.DATA = LowPart(symbol.gpuVirtAddr);
    }

    // PsColorExportEntry will always exist, while PsColorExportDualSourceEntry is always created.
    // So it needs to initialize the m_colorExportAddr[Default] and m_colorExportAddr[DualSourceBlendEnable]
    // with the same default value, then update m_colorExportAddr[DualSourceBlendEnable] if
    // PsColorExportDualSourceEntry created.
    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::PsColorExportEntry, &symbol) == Result::Success)
    {
        m_colorExportAddr[static_cast<uint32>(ColorExportShaderType::Default)] = LowPart(symbol.gpuVirtAddr);
        m_colorExportAddr[static_cast<uint32>(ColorExportShaderType::DualSourceBlendEnable)] = LowPart(symbol.gpuVirtAddr);
    }

    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::PsColorExportDualSourceEntry, &symbol) ==
        Result::Success)
    {
        m_colorExportAddr[static_cast<uint32>(ColorExportShaderType::DualSourceBlendEnable)] =
            LowPart(symbol.gpuVirtAddr);
    }

    const Elf::SymbolTableEntry* pElfSymbol = abiReader.GetPipelineSymbol(Abi::PipelineSymbolType::PsDisassembly);
    if (pElfSymbol != nullptr)
    {
        m_stageInfoPs.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
    }

    m_psWaveFrontSize = metadata.pipeline.hardwareStage[uint32(Util::Abi::HardwareStage::Ps)].wavefrontSize;
    m_regs.sh.spiShaderPgmRsrc1Ps.u32All = AbiRegisters::SpiShaderPgmRsrc1Ps(metadata, m_device, chipProps.gfxLevel);
    m_regs.sh.spiShaderPgmRsrc2Ps.u32All = AbiRegisters::SpiShaderPgmRsrc2Ps(metadata, chipProps.gfxLevel);
    m_regs.dynamic.spiShaderPgmRsrc3Ps.u32All =
        AbiRegisters::SpiShaderPgmRsrc3Ps(metadata, createInfo, m_device, chipProps.gfxLevel);
    m_regs.dynamic.spiShaderPgmRsrc4Ps.u32All =
        AbiRegisters::SpiShaderPgmRsrc4Ps(metadata, m_device, chipProps.gfxLevel, m_stageInfoPs.codeLength);
    m_regs.sh.spiShaderPgmChksumPs.u32All = AbiRegisters::SpiShaderPgmChksumPs(metadata, m_device);

    if (loadInfo.enableNgg == false)
    {
        if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::VsMainEntry, &symbol) == Result::Success)
        {
            m_stageInfoVs.codeLength   = static_cast<size_t>(symbol.size);
            PAL_ASSERT(symbol.gpuVirtAddr == Pow2Align(symbol.gpuVirtAddr, 256));

            m_regs.sh.spiShaderPgmLoVs.bits.MEM_BASE = Get256BAddrLo(symbol.gpuVirtAddr);
            m_regs.sh.spiShaderPgmHiVs.bits.MEM_BASE = Get256BAddrHi(symbol.gpuVirtAddr);
            PAL_ASSERT(m_regs.sh.spiShaderPgmHiVs.u32All == 0);
        }

        if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::VsShdrIntrlTblPtr, &symbol) == Result::Success)
        {
            m_regs.sh.userDataInternalTableVs.bits.DATA = LowPart(symbol.gpuVirtAddr);
        }

        pElfSymbol = abiReader.GetPipelineSymbol(Abi::PipelineSymbolType::VsDisassembly);
        if (pElfSymbol != nullptr)
        {
            m_stageInfoVs.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
        }

        m_regs.sh.spiShaderPgmRsrc1Vs.u32All      =
            AbiRegisters::SpiShaderPgmRsrc1Vs(metadata, m_device, chipProps.gfxLevel);
        m_regs.sh.spiShaderPgmRsrc2Vs.u32All      = AbiRegisters::SpiShaderPgmRsrc2Vs(metadata, chipProps.gfxLevel);
        m_regs.dynamic.spiShaderPgmRsrc3Vs.u32All =
            AbiRegisters::SpiShaderPgmRsrc3Vs(metadata, m_device, chipProps.gfxLevel);
        m_regs.dynamic.spiShaderPgmRsrc4Vs.u32All = AbiRegisters::SpiShaderPgmRsrc4Vs(metadata, m_device);
        m_regs.sh.spiShaderPgmChksumVs.u32All     = AbiRegisters::SpiShaderPgmChksumVs(metadata, m_device);
    } // if enableNgg == false

    if (UsesHwStreamout())
    {
        m_regs.context.vgtStrmoutBufferConfig.u32All = AbiRegisters::VgtStrmoutBufferConfig(metadata);
        AbiRegisters::VgtStrmoutVtxStrides(metadata, &m_regs.context.vgtStrmoutVtxStride[0]);
    }

    m_regs.context.dbShaderControl.u32All   = AbiRegisters::DbShaderControl(metadata, m_device, chipProps.gfxLevel);
    m_regs.context.spiBarycCntl.u32All      = AbiRegisters::SpiBarycCntl(metadata);
    m_regs.context.spiPsInputAddr.u32All    = AbiRegisters::SpiPsInputAddr(metadata);
    m_regs.context.spiPsInputEna.u32All     = AbiRegisters::SpiPsInputEna(metadata);
    m_regs.context.paClVsOutCntl.u32All     = AbiRegisters::PaClVsOutCntl(metadata, createInfo, chipProps.gfxLevel);
    m_regs.context.vgtPrimitiveIdEn.u32All  = AbiRegisters::VgtPrimitiveIdEn(metadata);
    m_regs.context.paScShaderControl.u32All = AbiRegisters::PaScShaderControl(metadata, m_device);
    m_paScAaConfig.u32All                   = AbiRegisters::PaScAaConfig(metadata);

     m_semanticCount = 0;
    if (metadata.pipeline.prerasterOutputSemantic[0].hasEntry.semantic)
    {
        for (uint32 i = 0; i < Util::ArrayLen32(metadata.pipeline.prerasterOutputSemantic); i++)
        {
            if (metadata.pipeline.prerasterOutputSemantic[i].hasEntry.semantic)
            {
                m_semanticCount++;
                m_semanticInfo[i].semantic = metadata.pipeline.prerasterOutputSemantic[i].semantic;
                m_semanticInfo[i].index = metadata.pipeline.prerasterOutputSemantic[i].index;
            }
            else
            {
                break;
            }
        }
    }
    else if (metadata.pipeline.psInputSemantic[0].hasEntry.semantic)
    {
        m_semanticCount = m_regs.context.interpolatorCount;
        for (uint32 i = 0; i < m_regs.context.interpolatorCount; i++)
        {
            m_semanticInfo[i].semantic = metadata.pipeline.psInputSemantic[i].semantic;
        }
    }
}

// =====================================================================================================================
// Copies this pipeline chunk's SH commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkVsPs::WriteShCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace,
    bool       isNgg
    ) const
{
    pCmdSpace = WriteShCommandsSetPathPs(pCmdStream, pCmdSpace);

    if (m_pPsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_pPsPerfDataInfo->regOffset,
                                                                 m_pPsPerfDataInfo->gpuVirtAddr,
                                                                 pCmdSpace);
    }

    if (isNgg == false)
    {
        pCmdSpace = WriteShCommandsSetPathVs(pCmdStream, pCmdSpace);

        if (m_pVsPerfDataInfo->regOffset != UserDataNotMapped)
        {
            pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_pVsPerfDataInfo->regOffset,
                                                                     m_pVsPerfDataInfo->gpuVirtAddr,
                                                                     pCmdSpace);
        }
    } // if isNgg == false

    return pCmdSpace;
}

// =====================================================================================================================
uint32* PipelineChunkVsPs::WriteDynamicRegs(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    bool                    isNgg,
    const DynamicStageInfo& vsStageInfo,
    const DynamicStageInfo& psStageInfo
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();
    VsPsRegs::Dynamic        dynamic   = m_regs.dynamic;

    if (psStageInfo.wavesPerSh > 0)
    {
        dynamic.spiShaderPgmRsrc3Ps.bits.WAVE_LIMIT = psStageInfo.wavesPerSh;
    }

    if (isNgg == false)
    {
        if (vsStageInfo.wavesPerSh != 0)
        {
            dynamic.spiShaderPgmRsrc3Vs.bits.WAVE_LIMIT = vsStageInfo.wavesPerSh;
        }
    }

    pCmdSpace = pCmdStream->WriteSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_PS,
                                                  dynamic.spiShaderPgmRsrc3Ps.u32All,
                                                  ShaderGraphics,
                                                  index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                  pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC4_PS,
                                                  dynamic.spiShaderPgmRsrc4Ps.u32All,
                                                  ShaderGraphics,
                                                  index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                  pCmdSpace);

    if (isNgg == false)
    {
        pCmdSpace = pCmdStream->WriteSetOneShRegIndex(Gfx10::mmSPI_SHADER_PGM_RSRC3_VS,
                                                      dynamic.spiShaderPgmRsrc3Vs.u32All,
                                                      ShaderGraphics,
                                                      index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                      pCmdSpace);

        pCmdSpace = pCmdStream->WriteSetOneShRegIndex(Gfx10::mmSPI_SHADER_PGM_RSRC4_VS,
                                                      dynamic.spiShaderPgmRsrc4Vs.u32All,
                                                      ShaderGraphics,
                                                      index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                      pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Copies this pipeline chunk's context commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkVsPs::WriteContextCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmSPI_BARYC_CNTL, m_regs.context.spiBarycCntl.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmSPI_PS_INPUT_ENA,
                                                   mmSPI_PS_INPUT_ADDR,
                                                   &m_regs.context.spiPsInputEna.u32All,
                                                   pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmPA_SC_SHADER_CONTROL,
                                                  m_regs.context.paScShaderControl.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmPA_CL_VS_OUT_CNTL,
                                                  m_regs.context.paClVsOutCntl.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmVGT_PRIMITIVEID_EN,
                                                  m_regs.context.vgtPrimitiveIdEn.u32All,
                                                  pCmdSpace);

    if (m_regs.context.interpolatorCount > 0)
    {
        const uint32 endRegisterAddr = (mmSPI_PS_INPUT_CNTL_0 + m_regs.context.interpolatorCount - 1);
        PAL_ASSERT(endRegisterAddr <= mmSPI_PS_INPUT_CNTL_31);

        pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmSPI_PS_INPUT_CNTL_0,
                                                       endRegisterAddr,
                                                       &m_regs.context.spiPsInputCntl[0],
                                                       pCmdSpace);
    }

    const auto& palDevice = *(m_device.Parent());
    const bool hasVgtStreamOut = m_device.Parent()->ChipProperties().gfxip.supportsHwVs;
    if (hasVgtStreamOut)
    {
        pCmdSpace = pCmdStream->WriteSetSeqContextRegs(Gfx10::mmVGT_STRMOUT_CONFIG,
                                                       Gfx10::mmVGT_STRMOUT_BUFFER_CONFIG,
                                                       &m_regs.context.vgtStrmoutConfig,
                                                       pCmdSpace);
    }

    if (UsesHwStreamout())
    {
        for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
        {
            pCmdSpace = pCmdStream->WriteSetOneContextReg(VgtStrmoutVtxStrideAddr[i],
                                                          m_regs.context.vgtStrmoutVtxStride[i].u32All,
                                                          pCmdSpace);
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Accumulates this pipeline chunk's SH registers into an array of packed register pairs.
void PipelineChunkVsPs::AccumulateShRegs(
    PackedRegisterPair* pRegPairs,
    uint32*             pNumRegs
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    const uint32 startingIdx = *pNumRegs;
#endif

    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    AccumulateShRegsPs(pRegPairs, pNumRegs);

    if (m_pPsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        SetOneShRegValPairPacked(pRegPairs, pNumRegs, m_pPsPerfDataInfo->regOffset, m_pPsPerfDataInfo->gpuVirtAddr);
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    PAL_ASSERT(InRange(*pNumRegs, startingIdx, startingIdx + VsPsRegs::NumShReg));
#endif
}

// =====================================================================================================================
// Accumulates this pipeline's context registers into an array of packed register pairs.
void PipelineChunkVsPs::AccumulateContextRegs(
    PackedRegisterPair* pRegPairs,
    uint32*             pNumRegs
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    const uint32 startingIdx = *pNumRegs;
#endif

    SetOneContextRegValPairPacked(pRegPairs,
                                  pNumRegs,
                                  mmSPI_BARYC_CNTL,
                                  m_regs.context.spiBarycCntl.u32All);
    SetSeqContextRegValPairPacked(pRegPairs,
                                  pNumRegs,
                                  mmSPI_PS_INPUT_ENA,
                                  mmSPI_PS_INPUT_ADDR,
                                  &m_regs.context.spiPsInputEna.u32All);
    SetOneContextRegValPairPacked(pRegPairs,
                                  pNumRegs,
                                  mmPA_SC_SHADER_CONTROL,
                                  m_regs.context.paScShaderControl.u32All);
    SetOneContextRegValPairPacked(pRegPairs,
                                  pNumRegs,
                                  mmPA_CL_VS_OUT_CNTL,
                                  m_regs.context.paClVsOutCntl.u32All);
    SetOneContextRegValPairPacked(pRegPairs,
                               pNumRegs,
                               mmVGT_PRIMITIVEID_EN,
                               m_regs.context.vgtPrimitiveIdEn.u32All);

    if (m_regs.context.interpolatorCount > 0)
    {
        const uint32 endRegisterAddr = (mmSPI_PS_INPUT_CNTL_0 + m_regs.context.interpolatorCount - 1);
        PAL_ASSERT(endRegisterAddr <= mmSPI_PS_INPUT_CNTL_31);

        SetSeqContextRegValPairPacked(pRegPairs,
                                      pNumRegs,
                                      mmSPI_PS_INPUT_CNTL_0,
                                      endRegisterAddr,
                                      &m_regs.context.spiPsInputCntl[0]);
    }

    const auto& palDevice = *(m_device.Parent());
    const bool hasVgtStreamOut = m_device.Parent()->ChipProperties().gfxip.supportsHwVs;
    if (hasVgtStreamOut)
    {
        SetSeqContextRegValPairPacked(pRegPairs,
                                      pNumRegs,
                                      Gfx10::mmVGT_STRMOUT_CONFIG,
                                      Gfx10::mmVGT_STRMOUT_BUFFER_CONFIG,
                                      &m_regs.context.vgtStrmoutConfig);
    }

    if (UsesHwStreamout())
    {
        for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
        {
            SetOneContextRegValPairPacked(pRegPairs,
                                          pNumRegs,
                                          VgtStrmoutVtxStrideAddr[i],
                                          m_regs.context.vgtStrmoutVtxStride[i].u32All);
        }
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    PAL_ASSERT(InRange(*pNumRegs, startingIdx, startingIdx + VsPsRegs::NumContextReg));
#endif
}

// =====================================================================================================================
// Writes PM4 commands to program the SH registers for the VS. Returns the next unused DWORD in pCmdSpace.
uint32* PipelineChunkVsPs::WriteShCommandsSetPathVs(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();
    PAL_ASSERT(chipProps.gfxip.supportsHwVs);

    pCmdSpace = pCmdStream->WriteSetSeqShRegs(Gfx10::mmSPI_SHADER_PGM_LO_VS,
                                              Gfx10::mmSPI_SHADER_PGM_RSRC2_VS,
                                              ShaderGraphics,
                                              &m_regs.sh.spiShaderPgmLoVs,
                                              pCmdSpace);
    if (m_regs.sh.userDataInternalTableVs.u32All != InvalidUserDataInternalTable)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(
            Gfx10::mmSPI_SHADER_USER_DATA_VS_0 + ConstBufTblStartReg,
            m_regs.sh.userDataInternalTableVs.u32All,
            pCmdSpace);
    }

    if (chipProps.gfx9.supportSpp != 0)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(Gfx10::mmSPI_SHADER_PGM_CHKSUM_VS,
                                                                 m_regs.sh.spiShaderPgmChksumVs.u32All,
                                                                 pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Writes PM4 commands to program the SH registers for the PS. Returns the next unused DWORD in pCmdSpace.
uint32* PipelineChunkVsPs::WriteShCommandsSetPathPs(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmSPI_SHADER_PGM_LO_PS,
                                              mmSPI_SHADER_PGM_RSRC2_PS,
                                              ShaderGraphics,
                                              &m_regs.sh.spiShaderPgmLoPs,
                                              pCmdSpace);

    if (m_regs.sh.userDataInternalTablePs.u32All != InvalidUserDataInternalTable)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(mmSPI_SHADER_USER_DATA_PS_0 + ConstBufTblStartReg,
                                                                 m_regs.sh.userDataInternalTablePs.u32All,
                                                                 pCmdSpace);
    }

    if (chipProps.gfx9.supportSpp != 0)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(mmSPI_SHADER_PGM_CHKSUM_PS,
                                                                 m_regs.sh.spiShaderPgmChksumPs.u32All,
                                                                 pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Accumulates registers into an array of packed register pairs to program the SH registers for the PS
void PipelineChunkVsPs::AccumulateShRegsPs(
    PackedRegisterPair* pRegPairs,
    uint32*             pNumRegs
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    SetSeqShRegValPairPacked(pRegPairs,
                             pNumRegs,
                             mmSPI_SHADER_PGM_LO_PS,
                             mmSPI_SHADER_PGM_RSRC2_PS,
                             &m_regs.sh.spiShaderPgmLoPs);

    if (m_regs.sh.userDataInternalTablePs.u32All != InvalidUserDataInternalTable)
    {
        SetOneShRegValPairPacked(pRegPairs,
                                 pNumRegs,
                                 mmSPI_SHADER_USER_DATA_PS_0 + ConstBufTblStartReg,
                                 m_regs.sh.userDataInternalTablePs.u32All);
    }

    if (chipProps.gfx9.supportSpp != 0)
    {
        SetOneShRegValPairPacked(pRegPairs,
                                 pNumRegs,
                                 mmSPI_SHADER_PGM_CHKSUM_PS,
                                 m_regs.sh.spiShaderPgmChksumPs.u32All);
    }
}

// =====================================================================================================================
// Merge Vs register chunk and Ps register chunk into single chunk.
void PipelineChunkVsPs::Clone(
    const PipelineChunkVsPs& chunkVs,
    const PipelineChunkVsPs& chunkPs,
    const GraphicsShaderLibrary* pExpLibrary)
{
    const GraphicsPipeline* pExpLib = static_cast<const GraphicsPipeline*>(pExpLibrary->GetPartialPipeline());
    const PipelineChunkVsPs& chunkExp = pExpLib->GetChunkVsPs();
    // Stage info
    m_stageInfoPs  = chunkPs.m_stageInfoPs;
    m_stageInfoVs  = chunkVs.m_stageInfoVs;

    // Vs registers
    m_regs.sh.spiShaderPgmLoVs            = chunkVs.m_regs.sh.spiShaderPgmLoVs;
    m_regs.sh.spiShaderPgmHiVs            = chunkVs.m_regs.sh.spiShaderPgmHiVs;
    m_regs.sh.spiShaderPgmRsrc1Vs         = chunkVs.m_regs.sh.spiShaderPgmRsrc1Vs;
    m_regs.sh.spiShaderPgmRsrc2Vs         = chunkVs.m_regs.sh.spiShaderPgmRsrc2Vs;
    m_regs.sh.spiShaderPgmChksumVs        = chunkVs.m_regs.sh.spiShaderPgmChksumVs;
    m_regs.sh.userDataInternalTableVs     = chunkVs.m_regs.sh.userDataInternalTableVs;
    m_regs.context.paClVsOutCntl          = chunkVs.m_regs.context.paClVsOutCntl;
    m_regs.context.vgtPrimitiveIdEn       = chunkVs.m_regs.context.vgtPrimitiveIdEn;
    m_regs.context.vgtStrmoutConfig       = chunkVs.m_regs.context.vgtStrmoutConfig;
    m_regs.context.vgtStrmoutBufferConfig = chunkVs.m_regs.context.vgtStrmoutBufferConfig;

    memcpy(m_regs.context.vgtStrmoutVtxStride,
           chunkVs.m_regs.context.vgtStrmoutVtxStride,
           sizeof(m_regs.context.vgtStrmoutVtxStride));
    m_regs.dynamic.spiShaderPgmRsrc3Vs = chunkVs.m_regs.dynamic.spiShaderPgmRsrc3Vs;
    m_regs.dynamic.spiShaderPgmRsrc4Vs = chunkVs.m_regs.dynamic.spiShaderPgmRsrc4Vs;

    // Ps registers
    m_regs.sh.spiShaderPgmLoPs        = chunkPs.m_regs.sh.spiShaderPgmLoPs;
    m_regs.sh.spiShaderPgmHiPs        = chunkPs.m_regs.sh.spiShaderPgmHiPs;
    m_regs.sh.spiShaderPgmRsrc1Ps     = chunkPs.m_regs.sh.spiShaderPgmRsrc1Ps;
    m_regs.sh.spiShaderPgmRsrc2Ps     = chunkPs.m_regs.sh.spiShaderPgmRsrc2Ps;
    m_regs.sh.spiShaderPgmChksumPs    = chunkPs.m_regs.sh.spiShaderPgmChksumPs;
    m_regs.sh.userDataInternalTablePs = chunkPs.m_regs.sh.userDataInternalTablePs;
    m_regs.context.spiBarycCntl       = chunkPs.m_regs.context.spiBarycCntl;
    m_regs.context.spiPsInputEna      = chunkPs.m_regs.context.spiPsInputEna;
    m_regs.context.spiPsInputAddr     = chunkPs.m_regs.context.spiPsInputAddr;
    m_regs.context.dbShaderControl    = chunkPs.m_regs.context.dbShaderControl;
    m_regs.context.paScShaderControl  = chunkPs.m_regs.context.paScShaderControl;
    m_regs.context.interpolatorCount  = chunkPs.m_regs.context.interpolatorCount;
    memcpy(m_regs.context.spiPsInputCntl,
           chunkPs.m_regs.context.spiPsInputCntl,
           m_regs.context.interpolatorCount * sizeof(regSPI_PS_INPUT_CNTL_0));

    m_regs.dynamic.spiShaderPgmRsrc3Ps = chunkPs.m_regs.dynamic.spiShaderPgmRsrc3Ps;
    m_regs.dynamic.spiShaderPgmRsrc4Ps = chunkPs.m_regs.dynamic.spiShaderPgmRsrc4Ps;

    m_paScAaConfig = chunkPs.m_paScAaConfig;

    // ColorExport registers
    // Need to override ALPHA_TO_MASK_DISABLE with respect to the color export library
    m_regs.context.dbShaderControl.bits.ALPHA_TO_MASK_DISABLE &=
        chunkExp.m_regs.context.dbShaderControl.bits.ALPHA_TO_MASK_DISABLE;
    memcpy(m_colorExportAddr, chunkExp.m_colorExportAddr, sizeof(m_colorExportAddr));

    if (m_colorExportAddr != 0)
    {
        PAL_ASSERT(pExpLibrary->IsColorExportShader());
        ColorExportProperty colorExportProperty = {};
        pExpLibrary->GetColorExportProperty(&colorExportProperty);
        uint32 expVgprNum =
            AbiRegisters::CalcNumVgprs(colorExportProperty.vgprCount, chunkPs.m_psWaveFrontSize == 32);
        uint32 expSgprNum =
            AbiRegisters::CalcNumSgprs(colorExportProperty.sgprCount);
        m_regs.sh.spiShaderPgmRsrc1Ps.bits.VGPRS = Max(expVgprNum, m_regs.sh.spiShaderPgmRsrc1Ps.bits.VGPRS);
        m_regs.sh.spiShaderPgmRsrc1Ps.bits.SGPRS = Max(expSgprNum, m_regs.sh.spiShaderPgmRsrc1Ps.bits.SGPRS);
    }

    if ((chunkPs.m_semanticCount > 0) && (chunkVs.m_semanticCount > 0))
    {
        constexpr uint32 DefaultValOffset = (1 << 5);
        constexpr uint32 ValOffsetMask    = ((1 << 5) - 1);
        for (uint32 i = 0; i < chunkPs.m_semanticCount; i++)
        {
            uint32 index = DefaultValOffset;
            for (uint32 j = 0; j < chunkVs.m_semanticCount; j++)
            {
                if (chunkVs.m_semanticInfo[j].semantic == chunkPs.m_semanticInfo[i].semantic)
                {
                    index = chunkVs.m_semanticInfo[j].index;
                }
            }
            m_regs.context.spiPsInputCntl[i].bits.OFFSET &= ~ValOffsetMask;
            m_regs.context.spiPsInputCntl[i].bits.OFFSET |= index;
        }
    }
}
} // Gfx9
} // Pal
