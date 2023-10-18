/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include "core/hw/gfxip/pipeline.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"

namespace Pal
{

class Platform;

namespace Gfx9
{

class  CmdStream;
class  Device;
struct GraphicsPipelineLoadInfo;

struct VsPsRegs
{
    struct Sh
    {
        regSPI_SHADER_PGM_LO_VS     spiShaderPgmLoVs;
        regSPI_SHADER_PGM_HI_VS     spiShaderPgmHiVs;
        regSPI_SHADER_PGM_RSRC1_VS  spiShaderPgmRsrc1Vs;
        regSPI_SHADER_PGM_RSRC2_VS  spiShaderPgmRsrc2Vs;
        regSPI_SHADER_PGM_CHKSUM_VS spiShaderPgmChksumVs;

        regSPI_SHADER_PGM_LO_PS     spiShaderPgmLoPs;
        regSPI_SHADER_PGM_HI_PS     spiShaderPgmHiPs;
        regSPI_SHADER_PGM_RSRC1_PS  spiShaderPgmRsrc1Ps;
        regSPI_SHADER_PGM_RSRC2_PS  spiShaderPgmRsrc2Ps;
        regSPI_SHADER_PGM_CHKSUM_PS spiShaderPgmChksumPs;

        regSPI_SHADER_USER_DATA_VS_0 userDataInternalTableVs;
        regSPI_SHADER_USER_DATA_PS_0 userDataInternalTablePs;
    } sh;

    struct Context
    {
        regSPI_BARYC_CNTL       spiBarycCntl;
        regSPI_PS_INPUT_ENA     spiPsInputEna;
        regSPI_PS_INPUT_ADDR    spiPsInputAddr;
        regDB_SHADER_CONTROL    dbShaderControl;
        regPA_SC_SHADER_CONTROL paScShaderControl;
        regPA_CL_VS_OUT_CNTL    paClVsOutCntl;
        regVGT_PRIMITIVEID_EN   vgtPrimitiveIdEn;

        regVGT_STRMOUT_CONFIG        vgtStrmoutConfig;
        regVGT_STRMOUT_BUFFER_CONFIG vgtStrmoutBufferConfig;
        regVGT_STRMOUT_VTX_STRIDE_0  vgtStrmoutVtxStride[MaxStreamOutTargets];

        uint32  interpolatorCount;
        regSPI_PS_INPUT_CNTL_0   spiPsInputCntl[MaxPsInputSemantics];
    } context;

    struct Dynamic
    {
        regSPI_SHADER_PGM_RSRC3_PS spiShaderPgmRsrc3Ps;
        regSPI_SHADER_PGM_RSRC4_PS spiShaderPgmRsrc4Ps;
        regSPI_SHADER_PGM_RSRC3_VS spiShaderPgmRsrc3Vs;
        regSPI_SHADER_PGM_RSRC4_VS spiShaderPgmRsrc4Vs;
    } dynamic;

    static constexpr uint32 NumContextReg = sizeof(Context) / sizeof(uint32_t);
    static constexpr uint32 NumShReg      = sizeof(Sh)      / sizeof(uint32_t);
};

// Contains the semantic info for interface match
struct SemanticInfo
{
    uint16 semantic;
    uint16 index;
};

// =====================================================================================================================
// Manages the chunk of a graphics pipeline which contains the registers associated with the hardware VS and PS stages.
// Many of the registers for the hardware VS stage are not required when running in NGG mode.
class PipelineChunkVsPs
{
public:
    PipelineChunkVsPs(
        const Device&       device,
        const PerfDataInfo* pVsPerfDataInfo,
        const PerfDataInfo* pPsPerfDataInfo);
    ~PipelineChunkVsPs() { }

    void EarlyInit(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        GraphicsPipelineLoadInfo*               pInfo);

    void LateInit(
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        const GraphicsPipelineLoadInfo&         loadInfo,
        const GraphicsPipelineCreateInfo&       createInfo,
        PipelineUploader*                       pUploader);

    uint32* WriteShCommands(
        CmdStream* pCmdStream,
        uint32*    pCmdSpace,
        bool       isNgg) const;
    uint32* WriteDynamicRegs(
        CmdStream*              pCmdStream,
        uint32*                 pCmdSpace,
        bool                    isNgg,
        const DynamicStageInfo& vsStageInfo,
        const DynamicStageInfo& psStageInfo) const;

    uint32* WriteContextCommands(
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const;

#if PAL_BUILD_GFX11
    void AccumulateShRegs(PackedRegisterPair* pRegPairs, uint32* pNumRegs) const;
    void AccumulateContextRegs(PackedRegisterPair* pRegPairs, uint32* pNumRegs) const;
#endif

    bool UsesHwStreamout() const { return (m_regs.context.vgtStrmoutConfig.u32All != 0);  }
    regVGT_STRMOUT_VTX_STRIDE_0 VgtStrmoutVtxStride(uint32 idx) const
        { return m_regs.context.vgtStrmoutVtxStride[idx]; }

    regDB_SHADER_CONTROL DbShaderControl() const { return m_regs.context.dbShaderControl; }
    regPA_CL_VS_OUT_CNTL PaClVsOutCntl() const { return m_regs.context.paClVsOutCntl; }
    regPA_SC_AA_CONFIG PaScAaConfig() const { return m_paScAaConfig; }
    regSPI_PS_INPUT_ENA SpiPsInputEna() const { return m_regs.context.spiPsInputEna; }
    regSPI_BARYC_CNTL SpiBarycCntl() const { return m_regs.context.spiBarycCntl; }

    // Shortcut for checking if the shader has enabled INNER_COVERAGE mode.
    bool UsesInnerCoverage() const
        { return (PaScAaConfig().bits.COVERAGE_TO_SHADER_SELECT == INPUT_INNER_COVERAGE); }

    gpusize VsProgramGpuVa() const
    {
        return GetOriginalAddress(m_regs.sh.spiShaderPgmLoVs.bits.MEM_BASE,
                                  m_regs.sh.spiShaderPgmHiVs.bits.MEM_BASE);
    }

    gpusize PsProgramGpuVa() const
    {
        return GetOriginalAddress(m_regs.sh.spiShaderPgmLoPs.bits.MEM_BASE,
                                  m_regs.sh.spiShaderPgmHiPs.bits.MEM_BASE);
    }

    gpusize ColorExportGpuVa() const
    {
        return m_colorExportAddr;
    }
    const ShaderStageInfo& StageInfoVs() const { return m_stageInfoVs; }
    const ShaderStageInfo& StageInfoPs() const { return m_stageInfoPs; }

    void Clone(const PipelineChunkVsPs& chunkVs,
               const PipelineChunkVsPs& chunkPs,
               const PipelineChunkVsPs& chunkExp);

    void AccumulateRegistersHash(Util::MetroHash64& hasher)  const { hasher.Update(m_regs.context); }
private:
    uint32* WriteShCommandsSetPathVs(CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteShCommandsSetPathPs(CmdStream* pCmdStream, uint32* pCmdSpace) const;
#if PAL_BUILD_GFX11
    void AccumulateShRegsPs(PackedRegisterPair* pRegPairs, uint32* pNumRegs) const;
#endif

    const Device&  m_device;

    VsPsRegs m_regs;

    SemanticInfo m_semanticInfo[MaxPsInputSemantics];
    uint32       m_semanticCount;

    const PerfDataInfo*const  m_pVsPerfDataInfo;   // VS performance data information.
    const PerfDataInfo*const  m_pPsPerfDataInfo;   // PS performance data information.

    ShaderStageInfo    m_stageInfoVs;
    ShaderStageInfo    m_stageInfoPs;
    regPA_SC_AA_CONFIG m_paScAaConfig; // This register is only written in the draw-time validation code.

    gpusize            m_colorExportAddr;
    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkVsPs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkVsPs);
};

} // Gfx9
} // Pal
