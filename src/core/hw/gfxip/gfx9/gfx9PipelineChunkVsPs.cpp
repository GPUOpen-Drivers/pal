/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
    { mmVGT_STRMOUT_VTX_STRIDE_0, mmVGT_STRMOUT_VTX_STRIDE_1, mmVGT_STRMOUT_VTX_STRIDE_2, mmVGT_STRMOUT_VTX_STRIDE_3, };

// Base count of PS SH registers which are loaded using LOAD_SH_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedShRegCountPs =
    1 + // mmSPI_SHADER_PGM_LO_PS
    1 + // mmSPI_SHADER_PGM_HI_PS
    1 + // mmSPI_SHADER_PGM_RSRC1_PS
    1 + // mmSPI_SHADER_PGM_RSRC2_PS
    0 + // SPI_SHADER_PGM_CHKSUM_PS is not included because it is not present on all HW
    1;  // mmSPI_SHADER_USER_DATA_PS_0 + ConstBufTblStartReg

// Base count of VS SH registers which are loaded using LOAD_SH_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedShRegCountVs =
    1 + // Gfx09_10::mmSPI_SHADER_PGM_LO_VS
    1 + // Gfx09_10::mmSPI_SHADER_PGM_HI_VS
    1 + // Gfx09_10::mmSPI_SHADER_PGM_RSRC1_VS
    1 + // Gfx09_10::mmSPI_SHADER_PGM_RSRC2_VS
    0 + // SPI_SHADER_PGM_CHKSUM_VS is not included because it is not present on all HW
    1;  // Gfx09_10::mmSPI_SHADER_USER_DATA_VS_0 + ConstBufTblStartReg

// Base count of Context registers which are loaded using LOAD_CNTX_REG_INDEX when binding to a command buffer.
static constexpr uint32 BaseLoadedCntxRegCount =
    1 + // mmSPI_SHADER_Z_FORMAT
    1 + // mmSPI_SHADER_COL_FORMAT
    1 + // mmSPI_BARYC_CNTL
    1 + // mmSPI_PS_INPUT_ENA
    1 + // mmSPI_PS_INPUT_ADDR
    1 + // mmDB_SHADER_CONTROL
    1 + // mmPA_SC_SHADER_CONTROL
    1 + // mmPA_SC_BINNER_CNTL_1
    1 + // mmSPI_SHADER_POS_FORMAT
    1 + // mmPA_CL_VS_OUT_CNTL
    1 + // mmVGT_PRIMITIVEID_EN
    0 + // mmSPI_PS_INPUT_CNTL_0...31 are not included because the number of interpolants depends on the pipeline
    1 + // mmVGT_STRMOUT_CONFIG
    1;  // mmVGT_STRMOUT_BUFFER_CONFIG

// Base count of Context registers which are loaded using LOAD_CNTX_REG_INDEX when binding to a command buffer when
// stream-out is enabled for this pipeline.
static constexpr uint32 BaseLoadedCntxRegCountStreamOut =
    4;  // mmVGT_STRMOUT_VTX_STRIDE_[0...3]

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
    memset(&m_regs, 0, sizeof(m_regs));
    memset(&m_stageInfoVs, 0, sizeof(m_stageInfoVs));
    memset(&m_stageInfoPs, 0, sizeof(m_stageInfoPs));
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

    const Gfx9PalSettings&   settings  = m_device.Settings();
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    // Determine if stream-out is enabled for this pipeline.
    registers.HasEntry(mmVGT_STRMOUT_CONFIG, &m_regs.context.vgtStrmoutConfig.u32All);

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

    if (settings.enableLoadIndexForObjectBinds != false)
    {
        pInfo->loadedCtxRegCount += (BaseLoadedCntxRegCount + m_regs.context.interpolatorCount);
        pInfo->loadedShRegCount  += (BaseLoadedShRegCountPs + ((chipProps.gfx9.supportSpp == 1) ? 1 : 0));

        if (pInfo->enableNgg == false)
        {
            pInfo->loadedShRegCount += (BaseLoadedShRegCountVs + ((chipProps.gfx9.supportSpp == 1) ? 1 : 0));
        }

        if (VgtStrmoutConfig().u32All != 0)
        {
            pInfo->loadedCtxRegCount += BaseLoadedCntxRegCountStreamOut;
        }

    }
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.  Also uploads register state into GPU memory.
void PipelineChunkVsPs::LateInit(
    const AbiReader&                    abiReader,
    const CodeObjectMetadata&           metadata,
    const RegisterVector&               registers,
    const GraphicsPipelineLoadInfo&     loadInfo,
    const GraphicsPipelineCreateInfo&   createInfo,
    GraphicsPipelineUploader*           pUploader,
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

        m_regs.sh.spiShaderPgmRsrc1Vs.u32All = registers.At(Gfx09_10::mmSPI_SHADER_PGM_RSRC1_VS);
        m_regs.sh.spiShaderPgmRsrc2Vs.u32All = registers.At(Gfx09_10::mmSPI_SHADER_PGM_RSRC2_VS);
        registers.HasEntry(Gfx09_10::mmSPI_SHADER_PGM_RSRC3_VS, &m_regs.dynamic.spiShaderPgmRsrc3Vs.u32All);

        // NOTE: The Pipeline ABI doesn't specify CU_GROUP_ENABLE for various shader stages, so it should be safe to
        // always use the setting PAL prefers.
        m_regs.sh.spiShaderPgmRsrc1Vs.bits.CU_GROUP_ENABLE = (settings.numVsWavesSoftGroupedPerCu > 0 ? 1 : 0);

        if (chipProps.gfx9.supportSpp != 0)
        {
            static_assert((Gfx10::mmSPI_SHADER_PGM_CHKSUM_VS == Rn::mmSPI_SHADER_PGM_CHKSUM_VS),
                          "CHKSUM_VS register has moved between GFX10 and  Renoir!");
            static_assert((Gfx10::mmSPI_SHADER_PGM_CHKSUM_VS == Rv2x::mmSPI_SHADER_PGM_CHKSUM_VS),
                          "CHKSUM_VS register has moved between GFX10 and  Raven2!");

            registers.HasEntry(Gfx10::mmSPI_SHADER_PGM_CHKSUM_VS, &m_regs.sh.spiShaderPgmChksumVs.u32All);
        }

        uint16 vsCuDisableMask = 0;
        if (IsGfx10Plus(chipProps.gfxLevel))
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

    if (VgtStrmoutConfig().u32All != 0)
    {
        m_regs.context.vgtStrmoutBufferConfig.u32All = registers.At(mmVGT_STRMOUT_BUFFER_CONFIG);

        for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
        {
            m_regs.context.vgtStrmoutVtxStride[i].u32All = registers.At(VgtStrmoutVtxStrideAddr[i]);
        }
    }

    m_regs.context.dbShaderControl.u32All    = registers.At(mmDB_SHADER_CONTROL);
    m_regs.context.spiBarycCntl.u32All       = registers.At(mmSPI_BARYC_CNTL);
    m_regs.context.spiPsInputAddr.u32All     = registers.At(mmSPI_PS_INPUT_ADDR);
    m_regs.context.spiPsInputEna.u32All      = registers.At(mmSPI_PS_INPUT_ENA);
    m_regs.context.spiShaderColFormat.u32All = registers.At(mmSPI_SHADER_COL_FORMAT);
    m_regs.context.spiShaderZFormat.u32All   = registers.At(mmSPI_SHADER_Z_FORMAT);
    m_regs.context.paClVsOutCntl.u32All      = registers.At(mmPA_CL_VS_OUT_CNTL);

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

    m_regs.context.spiShaderPosFormat.u32All = registers.At(mmSPI_SHADER_POS_FORMAT);
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

    // Binner_cntl1:
    // 16 bits: Maximum amount of parameter storage allowed per batch.
    // - Legacy: param cache lines/2 (groups of 16 vert-attributes) (0 means 1 encoding)
    // - NGG: number of vert-attributes (0 means 1 encoding)
    // - NGG + PC: param cache lines/2 (groups of 16 vert-attributes) (0 means 1 encoding)
    // 16 bits: Max number of primitives in batch
    m_regs.context.paScBinnerCntl1.u32All = 0;
    m_regs.context.paScBinnerCntl1.bits.MAX_PRIM_PER_BATCH = settings.binningMaxPrimPerBatch - 1;

    if (loadInfo.enableNgg)
    {
        m_regs.context.paScBinnerCntl1.bits.MAX_ALLOC_COUNT = settings.binningMaxAllocCountNggOnChip - 1;
    }
    else
    {
        m_regs.context.paScBinnerCntl1.bits.MAX_ALLOC_COUNT = settings.binningMaxAllocCountLegacy - 1;
    }

    pHasher->Update(m_regs.context);

    if (pUploader->EnableLoadIndexPath())
    {
        pUploader->AddShReg(mmSPI_SHADER_PGM_LO_PS,    m_regs.sh.spiShaderPgmLoPs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_HI_PS,    m_regs.sh.spiShaderPgmHiPs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC1_PS, m_regs.sh.spiShaderPgmRsrc1Ps);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC2_PS, m_regs.sh.spiShaderPgmRsrc2Ps);

        pUploader->AddShReg(mmSPI_SHADER_USER_DATA_PS_0 + ConstBufTblStartReg, m_regs.sh.userDataInternalTablePs);

        if (chipProps.gfx9.supportSpp != 0)
        {
            pUploader->AddShReg(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_PS, m_regs.sh.spiShaderPgmChksumPs);
        }

        if (loadInfo.enableNgg == false)
        {
            pUploader->AddShReg(Gfx09_10::mmSPI_SHADER_PGM_LO_VS,    m_regs.sh.spiShaderPgmLoVs);
            pUploader->AddShReg(Gfx09_10::mmSPI_SHADER_PGM_HI_VS,    m_regs.sh.spiShaderPgmHiVs);
            pUploader->AddShReg(Gfx09_10::mmSPI_SHADER_PGM_RSRC1_VS, m_regs.sh.spiShaderPgmRsrc1Vs);
            pUploader->AddShReg(Gfx09_10::mmSPI_SHADER_PGM_RSRC2_VS, m_regs.sh.spiShaderPgmRsrc2Vs);
            pUploader->AddShReg(Gfx09_10::mmSPI_SHADER_USER_DATA_VS_0 + ConstBufTblStartReg,
                                m_regs.sh.userDataInternalTableVs);

            if (chipProps.gfx9.supportSpp != 0)
            {
                pUploader->AddShReg(Gfx10::mmSPI_SHADER_PGM_CHKSUM_VS, m_regs.sh.spiShaderPgmChksumVs);
            }
        } // if enableNgg == false

        pUploader->AddCtxReg(mmDB_SHADER_CONTROL,         m_regs.context.dbShaderControl);
        pUploader->AddCtxReg(mmSPI_BARYC_CNTL,            m_regs.context.spiBarycCntl);
        pUploader->AddCtxReg(mmSPI_PS_INPUT_ADDR,         m_regs.context.spiPsInputAddr);
        pUploader->AddCtxReg(mmSPI_PS_INPUT_ENA,          m_regs.context.spiPsInputEna);
        pUploader->AddCtxReg(mmSPI_SHADER_COL_FORMAT,     m_regs.context.spiShaderColFormat);
        pUploader->AddCtxReg(mmSPI_SHADER_Z_FORMAT,       m_regs.context.spiShaderZFormat);;
        pUploader->AddCtxReg(mmSPI_SHADER_POS_FORMAT,     m_regs.context.spiShaderPosFormat);
        pUploader->AddCtxReg(mmPA_CL_VS_OUT_CNTL,         m_regs.context.paClVsOutCntl);
        pUploader->AddCtxReg(mmVGT_PRIMITIVEID_EN,        m_regs.context.vgtPrimitiveIdEn);
        pUploader->AddCtxReg(mmPA_SC_SHADER_CONTROL,      m_regs.context.paScShaderControl);
        pUploader->AddCtxReg(mmPA_SC_BINNER_CNTL_1,       m_regs.context.paScBinnerCntl1);
        pUploader->AddCtxReg(mmVGT_STRMOUT_CONFIG,        m_regs.context.vgtStrmoutConfig);
        pUploader->AddCtxReg(mmVGT_STRMOUT_BUFFER_CONFIG, m_regs.context.vgtStrmoutBufferConfig);

        for (uint16 i = 0; i < m_regs.context.interpolatorCount; ++i)
        {
            pUploader->AddCtxReg(mmSPI_PS_INPUT_CNTL_0 + i, m_regs.context.spiPsInputCntl[i]);
        }

        if (VgtStrmoutConfig().u32All != 0)
        {
            for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
            {
                pUploader->AddCtxReg(VgtStrmoutVtxStrideAddr[i], m_regs.context.vgtStrmoutVtxStride[i]);
            }
        }
    }
}

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
template <bool UseLoadIndexPath>
uint32* PipelineChunkVsPs::WriteShCommands(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    bool                    isNgg,
    const DynamicStageInfo& vsStageInfo,
    const DynamicStageInfo& psStageInfo
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    if (UseLoadIndexPath == false)
    {
        pCmdSpace = WriteShCommandsSetPathPs(pCmdStream, pCmdSpace);
    }

    auto dynamic = m_regs.dynamic;

    if (psStageInfo.wavesPerSh > 0)
    {
        dynamic.spiShaderPgmRsrc3Ps.bits.WAVE_LIMIT = psStageInfo.wavesPerSh;
    }

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
        if (UseLoadIndexPath == false)
        {
            pCmdSpace = WriteShCommandsSetPathVs(pCmdStream, pCmdSpace);
        }

        if (vsStageInfo.wavesPerSh != 0)
        {
            dynamic.spiShaderPgmRsrc3Vs.bits.WAVE_LIMIT = vsStageInfo.wavesPerSh;
        }

        if (vsStageInfo.cuEnableMask != 0)
        {
            dynamic.spiShaderPgmRsrc3Vs.bits.CU_EN &= vsStageInfo.cuEnableMask;
            dynamic.spiShaderPgmRsrc4Vs.bits.CU_EN  =
                Device::AdjustCuEnHi(dynamic.spiShaderPgmRsrc4Vs.bits.CU_EN, vsStageInfo.cuEnableMask);
        }

        pCmdSpace = pCmdStream->WriteSetOneShRegIndex(Gfx09_10::mmSPI_SHADER_PGM_RSRC3_VS,
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

// Instantiate template versions for the linker.
template
uint32* PipelineChunkVsPs::WriteShCommands<false>(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    bool                    isNgg,
    const DynamicStageInfo& vsStageInfo,
    const DynamicStageInfo& psStageInfo
    ) const;
template
uint32* PipelineChunkVsPs::WriteShCommands<true>(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    bool                    isNgg,
    const DynamicStageInfo& vsStageInfo,
    const DynamicStageInfo& psStageInfo
    ) const;

// =====================================================================================================================
// Copies this pipeline chunk's context commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
template <bool UseLoadIndexPath>
uint32* PipelineChunkVsPs::WriteContextCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    if (UseLoadIndexPath == false)
    {
        pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmSPI_SHADER_POS_FORMAT,
                                                       mmSPI_SHADER_COL_FORMAT,
                                                       &m_regs.context.spiShaderPosFormat,
                                                       pCmdSpace);
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
        pCmdSpace = pCmdStream->WriteSetOneContextReg(mmPA_SC_BINNER_CNTL_1,
                                                      m_regs.context.paScBinnerCntl1.u32All,
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

        pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmVGT_STRMOUT_CONFIG,
                                                       mmVGT_STRMOUT_BUFFER_CONFIG,
                                                       &m_regs.context.vgtStrmoutConfig,
                                                       pCmdSpace);

        if (VgtStrmoutConfig().u32All != 0)
        {
            for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
            {
                pCmdSpace = pCmdStream->WriteSetOneContextReg(VgtStrmoutVtxStrideAddr[i],
                                                              m_regs.context.vgtStrmoutVtxStride[i].u32All,
                                                              pCmdSpace);
            }
        }
    }

    return pCmdSpace;
}

// Instantiate template versions for the linker.
template
uint32* PipelineChunkVsPs::WriteContextCommands<false>(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const;
template
uint32* PipelineChunkVsPs::WriteContextCommands<true>(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const;

// =====================================================================================================================
// Writes PM4 commands to program the SH registers for the VS.
uint32* PipelineChunkVsPs::WriteShCommandsSetPathVs(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    pCmdSpace = pCmdStream->WriteSetSeqShRegs(Gfx09_10::mmSPI_SHADER_PGM_LO_VS,
                                              Gfx09_10::mmSPI_SHADER_PGM_RSRC2_VS,
                                              ShaderGraphics,
                                              &m_regs.sh.spiShaderPgmLoVs,
                                              pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(Gfx09_10::mmSPI_SHADER_USER_DATA_VS_0 + ConstBufTblStartReg,
                                                             m_regs.sh.userDataInternalTableVs.u32All,
                                                             pCmdSpace);

    if (chipProps.gfx9.supportSpp != 0)
    {
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
