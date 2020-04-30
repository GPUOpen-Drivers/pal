/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
    memset(&m_regs, 0, sizeof(m_regs));
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
    registers.HasEntry(mmVGT_STRMOUT_CONFIG, &m_regs.context.vgtStrmoutConfig.u32All);

    // Determine the number of PS interpolators and save them for LateInit to consume.
    m_regs.context.interpolatorCount = 0;
    for (uint16 i = 0; i < MaxPsInputSemantics; ++i)
    {
        const uint16 offset = (mmSPI_PS_INPUT_CNTL_0 + i);
        if (registers.HasEntry(offset, &m_regs.context.spiPsInputCntl[i].u32All) == false)
        {
            break;
        }

        ++m_regs.context.interpolatorCount;
    }

    const Gfx6PalSettings& settings = m_device.Settings();
    if (settings.enableLoadIndexForObjectBinds != false)
    {
        pInfo->loadedCtxRegCount += (BaseLoadedCntxRegCount + m_regs.context.interpolatorCount);
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
    const AbiReader&                    abiReader,
    const RegisterVector&               registers,
    const GraphicsPipelineLoadInfo&     loadInfo,
    const GraphicsPipelineCreateInfo&   createInfo,
    GraphicsPipelineUploader*           pUploader,
    MetroHash64*                        pHasher)
{
    const Gfx6PalSettings&   settings  = m_device.Settings();
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    GpuSymbol symbol = { };
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

    const Elf::SymbolTableEntry* pElfSymbol = abiReader.GetPipelineSymbol(Abi::PipelineSymbolType::VsDisassembly);
    if (pElfSymbol != nullptr)
    {
        m_stageInfoVs.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
    }

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

    pElfSymbol = abiReader.GetPipelineSymbol(Abi::PipelineSymbolType::PsDisassembly);
    if (pElfSymbol != nullptr)
    {
        m_stageInfoPs.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
    }

    m_regs.sh.spiShaderPgmRsrc1Vs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_VS);
    m_regs.sh.spiShaderPgmRsrc2Vs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_VS);
    registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_VS__CI__VI, &m_regs.dynamic.spiShaderPgmRsrc3Vs.u32All);

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_ENABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    m_regs.sh.spiShaderPgmRsrc1Vs.bits.CU_GROUP_ENABLE = (settings.vsCuGroupEnabled ? 1 : 0);

    m_regs.sh.spiShaderPgmRsrc1Ps.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_PS);
    m_regs.sh.spiShaderPgmRsrc2Ps.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_PS);
    registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_PS__CI__VI, &m_regs.dynamic.spiShaderPgmRsrc3Ps.u32All);

    // NOTE: The Pipeline ABI doesn't specify CU_GROUP_DISABLE for various shader stages, so it should be safe to
    // always use the setting PAL prefers.
    m_regs.sh.spiShaderPgmRsrc1Ps.bits.CU_GROUP_DISABLE = (settings.psCuGroupEnabled ? 0 : 1);

    m_regs.context.paClVsOutCntl.u32All = registers.At(mmPA_CL_VS_OUT_CNTL);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 524
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
#endif

    m_regs.context.spiShaderPosFormat.u32All = registers.At(mmSPI_SHADER_POS_FORMAT);
    m_regs.context.vgtPrimitiveIdEn.u32All   = registers.At(mmVGT_PRIMITIVEID_EN);

    // If the number of VS output semantics exceeds the half-pack threshold, then enable VS half-pack mode.  Keep in
    // mind that the number of VS exports are represented by a -1 field in the HW register!
    m_regs.context.spiVsOutConfig.u32All = registers.At(mmSPI_VS_OUT_CONFIG);
    if ((m_regs.context.spiVsOutConfig.bits.VS_EXPORT_COUNT + 1u) > settings.vsHalfPackThreshold)
    {
        m_regs.context.spiVsOutConfig.bits.VS_HALF_PACK = 1;
    }

    m_regs.context.spiPsInControl.u32All     = registers.At(mmSPI_PS_IN_CONTROL);
    m_regs.context.spiBarycCntl.u32All       = registers.At(mmSPI_BARYC_CNTL);
    m_regs.context.spiPsInputAddr.u32All     = registers.At(mmSPI_PS_INPUT_ADDR);
    m_regs.context.spiPsInputEna.u32All      = registers.At(mmSPI_PS_INPUT_ENA);
    m_regs.context.spiShaderColFormat.u32All = registers.At(mmSPI_SHADER_COL_FORMAT);
    m_regs.context.spiShaderZFormat.u32All   = registers.At(mmSPI_SHADER_Z_FORMAT);

    if (UsesStreamOut())
    {
        for (uint32 i = 0; i < MaxStreamOutTargets; ++i)
        {
            m_regs.context.vgtStrmoutVtxStride[i].u32All = registers.At(VgtStrmoutVtxStrideAddr[i]);
        }

        m_regs.context.vgtStrmoutBufferConfig.u32All = registers.At(mmVGT_STRMOUT_BUFFER_CONFIG);
    }

    pHasher->Update(m_regs.context);

    if (chipProps.gfxLevel >= GfxIpLevel::GfxIp7)
    {
        uint16 vsCuDisableMask = 0;
        if (m_device.LateAllocVsLimit())
        {
            // Disable virtualized CU #1 instead of #0 because thread traces use CU #0 by default.
            vsCuDisableMask = 0x2;
        }

        m_regs.dynamic.spiShaderPgmRsrc3Vs.bits.CU_EN = m_device.GetCuEnableMask(vsCuDisableMask,
                                                                                 settings.vsCuEnLimitMask);
        m_regs.dynamic.spiShaderPgmRsrc3Ps.bits.CU_EN = m_device.GetCuEnableMask(0, settings.psCuEnLimitMask);
    }

    if (pUploader->EnableLoadIndexPath())
    {
        pUploader->AddShReg(mmSPI_SHADER_PGM_LO_VS, m_regs.sh.spiShaderPgmLoVs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_HI_VS, m_regs.sh.spiShaderPgmHiVs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_LO_PS, m_regs.sh.spiShaderPgmLoPs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_HI_PS, m_regs.sh.spiShaderPgmHiPs);

        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC1_VS, m_regs.sh.spiShaderPgmRsrc1Vs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC2_VS, m_regs.sh.spiShaderPgmRsrc2Vs);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC1_PS, m_regs.sh.spiShaderPgmRsrc1Ps);
        pUploader->AddShReg(mmSPI_SHADER_PGM_RSRC2_PS, m_regs.sh.spiShaderPgmRsrc2Ps);

        pUploader->AddShReg(mmSPI_SHADER_USER_DATA_VS_0 + ConstBufTblStartReg, m_regs.sh.userDataInternalTableVs);
        pUploader->AddShReg(mmSPI_SHADER_USER_DATA_PS_0 + ConstBufTblStartReg, m_regs.sh.userDataInternalTablePs);

        pUploader->AddCtxReg(mmSPI_SHADER_POS_FORMAT,     m_regs.context.spiShaderPosFormat);
        pUploader->AddCtxReg(mmSPI_SHADER_Z_FORMAT,       m_regs.context.spiShaderZFormat);
        pUploader->AddCtxReg(mmSPI_SHADER_COL_FORMAT,     m_regs.context.spiShaderColFormat);
        pUploader->AddCtxReg(mmPA_CL_VS_OUT_CNTL,         m_regs.context.paClVsOutCntl);
        pUploader->AddCtxReg(mmVGT_PRIMITIVEID_EN,        m_regs.context.vgtPrimitiveIdEn);
        pUploader->AddCtxReg(mmSPI_BARYC_CNTL,            m_regs.context.spiBarycCntl);
        pUploader->AddCtxReg(mmSPI_PS_INPUT_ENA,          m_regs.context.spiPsInputEna);
        pUploader->AddCtxReg(mmSPI_PS_INPUT_ADDR,         m_regs.context.spiPsInputAddr);
        pUploader->AddCtxReg(mmVGT_STRMOUT_CONFIG,        m_regs.context.vgtStrmoutConfig);
        pUploader->AddCtxReg(mmVGT_STRMOUT_BUFFER_CONFIG, m_regs.context.vgtStrmoutBufferConfig);

        for (uint16 i = 0; i < m_regs.context.interpolatorCount; ++i)
        {
            pUploader->AddCtxReg(mmSPI_PS_INPUT_CNTL_0 + i, m_regs.context.spiPsInputCntl[i]);
        }

        if (UsesStreamOut())
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
    const DynamicStageInfo& vsStageInfo,
    const DynamicStageInfo& psStageInfo
    ) const
{
    if (UseLoadIndexPath == false)
    {
        pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmSPI_SHADER_PGM_LO_VS,
                                                  mmSPI_SHADER_PGM_RSRC2_VS,
                                                  ShaderGraphics,
                                                  &m_regs.sh.spiShaderPgmLoVs,
                                                  pCmdSpace);
        pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmSPI_SHADER_PGM_LO_PS,
                                                  mmSPI_SHADER_PGM_RSRC2_PS,
                                                  ShaderGraphics,
                                                  &m_regs.sh.spiShaderPgmLoPs,
                                                  pCmdSpace);

        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(mmSPI_SHADER_USER_DATA_VS_0 + ConstBufTblStartReg,
                                                                 m_regs.sh.userDataInternalTableVs.u32All,
                                                                 pCmdSpace);
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(mmSPI_SHADER_USER_DATA_PS_0 + ConstBufTblStartReg,
                                                                 m_regs.sh.userDataInternalTablePs.u32All,
                                                                 pCmdSpace);
    }

    // The "dynamic" registers don't exist on Gfx6.
    if (m_device.CmdUtil().IpLevel() >= GfxIpLevel::GfxIp7)
    {
        auto dynamic = m_regs.dynamic;

        if (vsStageInfo.wavesPerSh > 0)
        {
            dynamic.spiShaderPgmRsrc3Vs.bits.WAVE_LIMIT = vsStageInfo.wavesPerSh;
        }

        if (psStageInfo.wavesPerSh > 0)
        {
            dynamic.spiShaderPgmRsrc3Ps.bits.WAVE_LIMIT = psStageInfo.wavesPerSh;
        }

        if (vsStageInfo.cuEnableMask != 0)
        {
            dynamic.spiShaderPgmRsrc3Vs.bits.CU_EN &= vsStageInfo.cuEnableMask;
        }

        if (psStageInfo.cuEnableMask != 0)
        {
            dynamic.spiShaderPgmRsrc3Ps.bits.CU_EN &= psStageInfo.cuEnableMask;
        }

        pCmdSpace = pCmdStream->WriteSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_VS__CI__VI,
                                                      dynamic.spiShaderPgmRsrc3Vs.u32All,
                                                      ShaderGraphics,
                                                      SET_SH_REG_INDEX_CP_MODIFY_CU_MASK,
                                                      pCmdSpace);

        pCmdSpace = pCmdStream->WriteSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_PS__CI__VI,
                                                      dynamic.spiShaderPgmRsrc3Ps.u32All,
                                                      ShaderGraphics,
                                                      SET_SH_REG_INDEX_CP_MODIFY_CU_MASK,
                                                      pCmdSpace);
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

// Instantiate template versions for the linker.
template
uint32* PipelineChunkVsPs::WriteShCommands<false>(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& vsStageInfo,
    const DynamicStageInfo& psStageInfo
    ) const;
template
uint32* PipelineChunkVsPs::WriteShCommands<true>(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& vsStageInfo,
    const DynamicStageInfo& psStageInfo
    ) const;

// =====================================================================================================================
// Copies this pipeline chunk's context commands into the specified command space. Returns the next unused
// DWORD in pCmdSpace.
template <bool UseLoadIndexPath>
uint32* PipelineChunkVsPs::WriteContextCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // NOTE: It is expected that this function will only ever be called when the set path is in use.
    PAL_ASSERT(UseLoadIndexPath == false);

    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmSPI_SHADER_POS_FORMAT,
                                                   mmSPI_SHADER_COL_FORMAT,
                                                   &m_regs.context.spiShaderPosFormat,
                                                   pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmPA_CL_VS_OUT_CNTL,
                                                  m_regs.context.paClVsOutCntl.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmVGT_PRIMITIVEID_EN,
                                                  m_regs.context.vgtPrimitiveIdEn.u32All,
                                                  pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmSPI_BARYC_CNTL, m_regs.context.spiBarycCntl.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmSPI_PS_INPUT_ENA,
                                                   mmSPI_PS_INPUT_ADDR,
                                                   &m_regs.context.spiPsInputEna,
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

    if (UsesStreamOut())
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

} // Gfx6
} // Pal
