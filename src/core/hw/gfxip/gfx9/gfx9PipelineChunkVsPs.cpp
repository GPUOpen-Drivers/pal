/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkVsPs.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// Stream-out vertex stride register addresses.
constexpr uint16 VgtStrmoutVtxStrideAddr[] =
{
    HasHwVs::mmVGT_STRMOUT_VTX_STRIDE_0,
    HasHwVs::mmVGT_STRMOUT_VTX_STRIDE_1,
    HasHwVs::mmVGT_STRMOUT_VTX_STRIDE_2,
    HasHwVs::mmVGT_STRMOUT_VTX_STRIDE_3
};

// =====================================================================================================================
PipelineChunkVsPs::PipelineChunkVsPs(
    const Device&       device,
    const PerfDataInfo* pVsPerfDataInfo,
    const PerfDataInfo* pPsPerfDataInfo)
    :
    m_device(device),
    m_regs{},
    m_pVsPerfDataInfo(pVsPerfDataInfo),
    m_pPsPerfDataInfo(pPsPerfDataInfo),
    m_stageInfoVs{},
    m_stageInfoPs{}
{
    m_paScAaConfig.u32All = 0;

    m_stageInfoVs.stageId = Abi::HardwareStage::Vs;
    m_stageInfoPs.stageId = Abi::HardwareStage::Ps;
}

// =====================================================================================================================
// Early initialization for this pipeline chunk.  Responsible for determining the number of SH and context registers to
// be loaded using LOAD_CNTX_REG_INDEX and LOAD_SH_REG_INDEX.
void PipelineChunkVsPs::EarlyInit(
    const RegisterVector&     registers,
    GraphicsPipelineLoadInfo* pInfo)
{
    PAL_ASSERT(pInfo != nullptr);

    const bool hasVgtStreamOut = m_device.Parent()->ChipProperties().gfxip.supportsHwVs;

    // Determine if stream-out is enabled for this pipeline.
    if (hasVgtStreamOut)
    {
        registers.HasEntry(HasHwVs::mmVGT_STRMOUT_CONFIG, &m_regs.context.vgtStrmoutConfig.u32All);
    }

    // Determine the number of PS interpolators and save them for LateInit to consume.
    m_regs.context.interpolatorCount = 0;
    for (uint32 i = 0; i < MaxPsInputSemantics; ++i)
    {
        const uint16 offset = static_cast<uint16>(mmSPI_PS_INPUT_CNTL_0 + i);
        if (registers.HasEntry(offset, &m_regs.context.spiPsInputCntl[i].u32All) == false)
        {
            break;
        }

        ++m_regs.context.interpolatorCount;
    }
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.
void PipelineChunkVsPs::LateInit(
    const AbiReader&                    abiReader,
    const PalAbi::CodeObjectMetadata&   metadata,
    const RegisterVector&               registers,
    const GraphicsPipelineLoadInfo&     loadInfo,
    const GraphicsPipelineCreateInfo&   createInfo,
    PipelineUploader*                   pUploader,
    MetroHash64*                        pHasher)
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

    const Elf::SymbolTableEntry* pElfSymbol = abiReader.GetPipelineSymbol(Abi::PipelineSymbolType::PsDisassembly);
    if (pElfSymbol != nullptr)
    {
        m_stageInfoPs.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
    }

    m_regs.sh.spiShaderPgmRsrc1Ps.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_PS);
    m_regs.sh.spiShaderPgmRsrc2Ps.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_PS);
    registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_PS, &m_regs.dynamic.spiShaderPgmRsrc3Ps.u32All);

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_DISABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    m_regs.sh.spiShaderPgmRsrc1Ps.bits.CU_GROUP_DISABLE = (settings.numPsWavesSoftGroupedPerCu > 0 ? 0 : 1);

    if (chipProps.gfx9.supportSpp != 0)
    {
        registers.HasEntry(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_PS, &m_regs.sh.spiShaderPgmChksumPs.u32All);
    }

    m_regs.dynamic.spiShaderPgmRsrc3Ps.bits.CU_EN = m_device.GetCuEnableMask(0, settings.psCuEnLimitMask);

    if (IsGfx10Plus(chipProps.gfxLevel))
    {
        m_regs.dynamic.spiShaderPgmRsrc4Ps.bits.CU_EN = m_device.GetCuEnableMaskHi(0, settings.psCuEnLimitMask);

#if PAL_ENABLE_PRINTS_ASSERTS
        m_device.AssertUserAccumRegsDisabled(registers, Gfx10Plus::mmSPI_SHADER_USER_ACCUM_PS_0);
        if (loadInfo.enableNgg == false)
        {
            m_device.AssertUserAccumRegsDisabled(registers, Gfx10::mmSPI_SHADER_USER_ACCUM_VS_0);
        }
#endif
    }

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

        m_regs.sh.spiShaderPgmRsrc1Vs.u32All = registers.At(HasHwVs::mmSPI_SHADER_PGM_RSRC1_VS);
        m_regs.sh.spiShaderPgmRsrc2Vs.u32All = registers.At(HasHwVs::mmSPI_SHADER_PGM_RSRC2_VS);
        registers.HasEntry(HasHwVs::mmSPI_SHADER_PGM_RSRC3_VS, &m_regs.dynamic.spiShaderPgmRsrc3Vs.u32All);

        // NOTE: The Pipeline ABI doesn't specify CU_GROUP_ENABLE for various shader stages, so it should be safe to
        // always use the setting PAL prefers.
        m_regs.sh.spiShaderPgmRsrc1Vs.bits.CU_GROUP_ENABLE = (settings.numVsWavesSoftGroupedPerCu > 0 ? 1 : 0);

        if (chipProps.gfx9.supportSpp != 0)
        {
            static_assert((Gfx10::mmSPI_SHADER_PGM_CHKSUM_VS == Rv2x_Rn::mmSPI_SHADER_PGM_CHKSUM_VS),
                          "CHKSUM_VS register has moved between GFX10 and Raven2/Renoir!");

            registers.HasEntry(Gfx10::mmSPI_SHADER_PGM_CHKSUM_VS, &m_regs.sh.spiShaderPgmChksumVs.u32All);
        }

        uint16 vsCuDisableMask = 0;
        if (IsGfx101(chipProps.gfxLevel))
        {
            // Both CU's of a WGP need to be disabled for better performance.
            vsCuDisableMask = 0xC;
        }
        else
        {
            // Disable virtualized CU #1 instead of #0 because thread traces use CU #0 by default.
            vsCuDisableMask = 0x2;
        }

        // NOTE: The Pipeline ABI doesn't specify CU enable masks for each shader stage, so it should be safe to
        // always use the ones PAL prefers.
        m_regs.dynamic.spiShaderPgmRsrc3Vs.bits.CU_EN =
                    m_device.GetCuEnableMask(vsCuDisableMask, settings.vsCuEnLimitMask);
        if (IsGfx10Plus(chipProps.gfxLevel))
        {
            const uint16 vsCuDisableMaskHi = 0;
            m_regs.dynamic.spiShaderPgmRsrc4Vs.bits.CU_EN =
                    m_device.GetCuEnableMaskHi(vsCuDisableMaskHi, settings.vsCuEnLimitMask);

        }
    } // if enableNgg == false

    if (UsesHwStreamout())
    {
        m_regs.context.vgtStrmoutBufferConfig.u32All = registers.At(HasHwVs::mmVGT_STRMOUT_BUFFER_CONFIG);

        for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
        {
            m_regs.context.vgtStrmoutVtxStride[i].u32All = registers.At(VgtStrmoutVtxStrideAddr[i]);
        }
    }

    m_regs.context.dbShaderControl.u32All = registers.At(mmDB_SHADER_CONTROL);
    m_regs.context.spiBarycCntl.u32All    = registers.At(mmSPI_BARYC_CNTL);
    m_regs.context.spiPsInputAddr.u32All  = registers.At(mmSPI_PS_INPUT_ADDR);
    m_regs.context.spiPsInputEna.u32All   = registers.At(mmSPI_PS_INPUT_ENA);
    m_regs.context.paClVsOutCntl.u32All   = registers.At(mmPA_CL_VS_OUT_CNTL);

    if (createInfo.rsState.clipDistMask != 0)
    {
        m_regs.context.paClVsOutCntl.bitfields.CLIP_DIST_ENA_0 &= (createInfo.rsState.clipDistMask & 0x1) != 0;
        m_regs.context.paClVsOutCntl.bitfields.CLIP_DIST_ENA_1 &= (createInfo.rsState.clipDistMask & 0x2) != 0;
        m_regs.context.paClVsOutCntl.bitfields.CLIP_DIST_ENA_2 &= (createInfo.rsState.clipDistMask & 0x4) != 0;
        m_regs.context.paClVsOutCntl.bitfields.CLIP_DIST_ENA_3 &= (createInfo.rsState.clipDistMask & 0x8) != 0;
        m_regs.context.paClVsOutCntl.bitfields.CLIP_DIST_ENA_4 &= (createInfo.rsState.clipDistMask & 0x10) != 0;
        m_regs.context.paClVsOutCntl.bitfields.CLIP_DIST_ENA_5 &= (createInfo.rsState.clipDistMask & 0x20) != 0;
        m_regs.context.paClVsOutCntl.bitfields.CLIP_DIST_ENA_6 &= (createInfo.rsState.clipDistMask & 0x40) != 0;
        m_regs.context.paClVsOutCntl.bitfields.CLIP_DIST_ENA_7 &= (createInfo.rsState.clipDistMask & 0x80) != 0;
    }

    // Unlike our hardware, DX12 and Vulkan do not have separate vertex and primitive combiners.
    // A mesh shader is the only shader that can export a primitive rate so if there is
    // no mesh shader then we can bypass the prim rate combiner. Vulkan does not use mesh shaders
    // so BYPASS_PRIM_RATE_COMBINER should always be 1 there.
    if (IsGfx103Plus(*m_device.Parent()))
    {
        if (metadata.pipeline.shader[static_cast<uint32>(Abi::ApiShaderType::Mesh)].hasEntry.uAll != 0)
        {
            m_regs.context.paClVsOutCntl.gfx103Plus.BYPASS_VTX_RATE_COMBINER = 1;
        }
        else
        {
            m_regs.context.paClVsOutCntl.gfx103Plus.BYPASS_PRIM_RATE_COMBINER = 1;
        }
    }

    m_regs.context.vgtPrimitiveIdEn.u32All   = registers.At(mmVGT_PRIMITIVEID_EN);
    m_regs.context.paScShaderControl.u32All  = registers.At(mmPA_SC_SHADER_CONTROL);
    m_paScAaConfig.u32All                    = registers.At(mmPA_SC_AA_CONFIG);

    if (chipProps.gfx9.supportCustomWaveBreakSize && (settings.forceWaveBreakSize != Gfx10ForceWaveBreakSizeClient))
    {
        // Override whatever wave-break size was specified by the pipeline binary if the panel is forcing a
        // value for the preferred wave-break size.
        m_regs.context.paScShaderControl.gfx10Plus.WAVE_BREAK_REGION_SIZE =
            static_cast<uint32>(settings.forceWaveBreakSize);
    }

    pHasher->Update(m_regs.context);
}

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkVsPs::WriteShCommands(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    bool                    isNgg,
    const DynamicStageInfo& vsStageInfo,
    const DynamicStageInfo& psStageInfo
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    pCmdSpace = WriteShCommandsSetPathPs(pCmdStream, pCmdSpace);

    auto dynamic = m_regs.dynamic;

    if (psStageInfo.wavesPerSh > 0)
    {
        dynamic.spiShaderPgmRsrc3Ps.bits.WAVE_LIMIT = psStageInfo.wavesPerSh;
    }
#if PAL_AMDGPU_BUILD
    else if (IsGfx9(chipProps.gfxLevel) && (dynamic.spiShaderPgmRsrc3Ps.bits.WAVE_LIMIT == 0))
    {
        // GFX9 GPUs have a HW bug where a wave limit size of 0 does not correctly map to "no limit",
        // potentially breaking high-priority compute.
        dynamic.spiShaderPgmRsrc3Ps.bits.WAVE_LIMIT = m_device.GetMaxWavesPerSh(chipProps, false);
    }
#endif

    if (psStageInfo.cuEnableMask != 0)
    {
        dynamic.spiShaderPgmRsrc3Ps.bits.CU_EN &= psStageInfo.cuEnableMask;
        dynamic.spiShaderPgmRsrc4Ps.bits.CU_EN  =
            Device::AdjustCuEnHi(dynamic.spiShaderPgmRsrc4Ps.bits.CU_EN, psStageInfo.cuEnableMask);
    }

    pCmdSpace = pCmdStream->WriteSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_PS,
                                                  dynamic.spiShaderPgmRsrc3Ps.u32All,
                                                  ShaderGraphics,
                                                  index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                  pCmdSpace);

    if (IsGfx10Plus(chipProps.gfxLevel))
    {
        pCmdSpace = pCmdStream->WriteSetOneShRegIndex(Gfx10Plus::mmSPI_SHADER_PGM_RSRC4_PS,
                                                      dynamic.spiShaderPgmRsrc4Ps.u32All,
                                                      ShaderGraphics,
                                                      index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                      pCmdSpace);
    }

    if (m_pPsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_pPsPerfDataInfo->regOffset,
                                                                 m_pPsPerfDataInfo->gpuVirtAddr,
                                                                 pCmdSpace);
    }

    if (isNgg == false)
    {
        pCmdSpace = WriteShCommandsSetPathVs(pCmdStream, pCmdSpace);

        if (vsStageInfo.wavesPerSh != 0)
        {
            dynamic.spiShaderPgmRsrc3Vs.bits.WAVE_LIMIT = vsStageInfo.wavesPerSh;
        }
#if PAL_AMDGPU_BUILD
        else if (IsGfx9(chipProps.gfxLevel) && (dynamic.spiShaderPgmRsrc3Vs.bits.WAVE_LIMIT == 0))
        {
            // GFX9 GPUs have a HW bug where a wave limit size of 0 does not correctly map to "no limit",
            // potentially breaking high-priority compute.
            dynamic.spiShaderPgmRsrc3Vs.bits.WAVE_LIMIT = m_device.GetMaxWavesPerSh(chipProps, false);
        }
#endif

        if (vsStageInfo.cuEnableMask != 0)
        {
            dynamic.spiShaderPgmRsrc3Vs.bits.CU_EN &= vsStageInfo.cuEnableMask;
            dynamic.spiShaderPgmRsrc4Vs.bits.CU_EN  =
                Device::AdjustCuEnHi(dynamic.spiShaderPgmRsrc4Vs.bits.CU_EN, vsStageInfo.cuEnableMask);
        }

        pCmdSpace = pCmdStream->WriteSetOneShRegIndex(HasHwVs::mmSPI_SHADER_PGM_RSRC3_VS,
                                                      dynamic.spiShaderPgmRsrc3Vs.u32All,
                                                      ShaderGraphics,
                                                      index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                      pCmdSpace);

        if (IsGfx10Plus(chipProps.gfxLevel))
        {
            pCmdSpace = pCmdStream->WriteSetOneShRegIndex(Gfx10::mmSPI_SHADER_PGM_RSRC4_VS,
                                                          dynamic.spiShaderPgmRsrc4Vs.u32All,
                                                          ShaderGraphics,
                                                          index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                          pCmdSpace);
        }

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
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_SHADER_CONTROL,
                                                  m_regs.context.dbShaderControl.u32All,
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
        pCmdSpace = pCmdStream->WriteSetSeqContextRegs(HasHwVs::mmVGT_STRMOUT_CONFIG,
                                                       HasHwVs::mmVGT_STRMOUT_BUFFER_CONFIG,
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
// Writes PM4 commands to program the SH registers for the VS.
uint32* PipelineChunkVsPs::WriteShCommandsSetPathVs(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();
    PAL_ASSERT(chipProps.gfxip.supportsHwVs);

    pCmdSpace = pCmdStream->WriteSetSeqShRegs(HasHwVs::mmSPI_SHADER_PGM_LO_VS,
                                              HasHwVs::mmSPI_SHADER_PGM_RSRC2_VS,
                                              ShaderGraphics,
                                              &m_regs.sh.spiShaderPgmLoVs,
                                              pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(HasHwVs::mmSPI_SHADER_USER_DATA_VS_0 + ConstBufTblStartReg,
                                                             m_regs.sh.userDataInternalTableVs.u32All,
                                                             pCmdSpace);

    if (chipProps.gfx9.supportSpp != 0)
    {
        static_assert(Gfx10::mmSPI_SHADER_PGM_CHKSUM_VS == Rv2x_Rn::mmSPI_SHADER_PGM_CHKSUM_VS,
                      "Registers have changed");

        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(Gfx10::mmSPI_SHADER_PGM_CHKSUM_VS,
                                                                 m_regs.sh.spiShaderPgmChksumVs.u32All,
                                                                 pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Writes PM4 commands to program the SH registers for the PS.
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

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(mmSPI_SHADER_USER_DATA_PS_0 + ConstBufTblStartReg,
                                                             m_regs.sh.userDataInternalTablePs.u32All,
                                                             pCmdSpace);

    if (chipProps.gfx9.supportSpp != 0)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_PS,
                                                                 m_regs.sh.spiShaderPgmChksumPs.u32All,
                                                                 pCmdSpace);
    }

    return pCmdSpace;
}

} // Gfx9
} // Pal
