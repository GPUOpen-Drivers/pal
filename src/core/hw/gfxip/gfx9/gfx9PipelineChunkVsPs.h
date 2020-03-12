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

#pragma once

#include "core/hw/gfxip/pipeline.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "palPipelineAbiProcessor.h"

namespace Pal
{

class Platform;

namespace Gfx9
{

class  CmdStream;
class  Device;
class  GraphicsPipelineUploader;
struct GraphicsPipelineLoadInfo;

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
        const RegisterVector&     registers,
        GraphicsPipelineLoadInfo* pInfo);

    void LateInit(
        const AbiProcessor&                 abiProcessor,
        const CodeObjectMetadata&           metadata,
        const RegisterVector&               registers,
        const GraphicsPipelineLoadInfo&     loadInfo,
        const GraphicsPipelineCreateInfo&   createInfo,
        GraphicsPipelineUploader*           pUploader,
        Util::MetroHash64*                  pHasher);

    template <bool UseLoadIndexPath>
    uint32* WriteShCommands(
        CmdStream*              pCmdStream,
        uint32*                 pCmdSpace,
        bool                    isNgg,
        const DynamicStageInfo& vsStageInfo,
        const DynamicStageInfo& psStageInfo) const;

    template <bool UseLoadIndexPath>
    uint32* WriteContextCommands(
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const;

    regVGT_STRMOUT_CONFIG VgtStrmoutConfig() const { return m_regs.context.vgtStrmoutConfig; }
    regVGT_STRMOUT_BUFFER_CONFIG VgtStrmoutBufferConfig() const { return m_regs.context.vgtStrmoutBufferConfig; }
    regVGT_STRMOUT_VTX_STRIDE_0 VgtStrmoutVtxStride(uint32 idx) const
        { return m_regs.context.vgtStrmoutVtxStride[idx]; }

    regSPI_SHADER_Z_FORMAT SpiShaderZFormat() const { return m_regs.context.spiShaderZFormat; }
    regDB_SHADER_CONTROL DbShaderControl() const { return m_regs.context.dbShaderControl; }
    regPA_CL_VS_OUT_CNTL PaClVsOutCntl() const { return m_regs.context.paClVsOutCntl; }
    regPA_SC_AA_CONFIG PaScAaConfig() const { return m_regs.context.paScAaConfig; }

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

    const ShaderStageInfo& StageInfoVs() const { return m_stageInfoVs; }
    const ShaderStageInfo& StageInfoPs() const { return m_stageInfoPs; }

private:
    uint32* WriteShCommandsSetPathVs(CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteShCommandsSetPathPs(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    const Device&  m_device;

    struct
    {
        struct
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

            regSPI_SHADER_USER_DATA_VS_0  userDataInternalTableVs;
            regSPI_SHADER_USER_DATA_PS_0  userDataInternalTablePs;
        } sh;

        struct
        {
            regSPI_SHADER_POS_FORMAT  spiShaderPosFormat;
            regSPI_SHADER_Z_FORMAT    spiShaderZFormat;
            regSPI_SHADER_COL_FORMAT  spiShaderColFormat;
            regSPI_BARYC_CNTL         spiBarycCntl;
            regSPI_PS_INPUT_ENA       spiPsInputEna;
            regSPI_PS_INPUT_ADDR      spiPsInputAddr;
            regDB_SHADER_CONTROL      dbShaderControl;
            regPA_SC_AA_CONFIG        paScAaConfig;
            regPA_SC_SHADER_CONTROL   paScShaderControl;
            regPA_SC_BINNER_CNTL_1    paScBinnerCntl1;
            regPA_CL_VS_OUT_CNTL      paClVsOutCntl;
            regVGT_PRIMITIVEID_EN     vgtPrimitiveIdEn;

            regVGT_STRMOUT_CONFIG         vgtStrmoutConfig;
            regVGT_STRMOUT_BUFFER_CONFIG  vgtStrmoutBufferConfig;
            regVGT_STRMOUT_VTX_STRIDE_0   vgtStrmoutVtxStride[MaxStreamOutTargets];

            uint32  interpolatorCount;
            regSPI_PS_INPUT_CNTL_0   spiPsInputCntl[MaxPsInputSemantics];
        } context;

        struct
        {
            regSPI_SHADER_PGM_RSRC3_PS  spiShaderPgmRsrc3Ps;
            regSPI_SHADER_PGM_RSRC4_PS  spiShaderPgmRsrc4Ps;
            regSPI_SHADER_PGM_RSRC3_VS  spiShaderPgmRsrc3Vs;
            regSPI_SHADER_PGM_RSRC4_VS  spiShaderPgmRsrc4Vs;
        } dynamic;
    }  m_regs;

    const PerfDataInfo*const  m_pVsPerfDataInfo;   // VS performance data information.
    const PerfDataInfo*const  m_pPsPerfDataInfo;   // PS performance data information.

    ShaderStageInfo  m_stageInfoVs;
    ShaderStageInfo  m_stageInfoPs;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkVsPs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkVsPs);
};

} // Gfx9
} // Pal
