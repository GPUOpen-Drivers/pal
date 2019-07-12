/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
        const AbiProcessor&             abiProcessor,
        const CodeObjectMetadata&       metadata,
        const RegisterVector&           registers,
        const GraphicsPipelineLoadInfo& loadInfo,
        GraphicsPipelineUploader*       pUploader,
        Util::MetroHash64*              pHasher);

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

    regVGT_STRMOUT_CONFIG VgtStrmoutConfig() const { return m_commands.streamOut.vgtStrmoutConfig; }
    regVGT_STRMOUT_BUFFER_CONFIG VgtStrmoutBufferConfig() const { return m_commands.streamOut.vgtStrmoutBufferConfig; }
    regVGT_STRMOUT_VTX_STRIDE_0 VgtStrmoutVtxStride(uint32 idx) const
        { return m_commands.streamOut.stride[idx].vgtStrmoutVtxStride; }

    regSPI_SHADER_Z_FORMAT SpiShaderZFormat() const { return m_commands.context.spiShaderZFormat; }
    regDB_SHADER_CONTROL DbShaderControl() const { return m_commands.context.dbShaderControl; }
    regPA_CL_VS_OUT_CNTL PaClVsOutCntl() const { return m_commands.context.paClVsOutCntl; }

    regPA_SC_AA_CONFIG PaScAaConfig() const
        { return *reinterpret_cast<const regPA_SC_AA_CONFIG*>(&m_commands.common.paScAaConfig.reg_data); }

    regPA_SC_SHADER_CONTROL PaScShaderControl(uint32  numIndices) const;

    // Shortcut for checking if the shader has enabled INNER_COVERAGE mode.
    bool UsesInnerCoverage() const
        { return (PaScAaConfig().bits.COVERAGE_TO_SHADER_SELECT == INPUT_INNER_COVERAGE); }

    gpusize VsProgramGpuVa() const
    {
        return GetOriginalAddress(m_commands.sh.vs.spiShaderPgmLoVs.bits.MEM_BASE,
                                  m_commands.sh.vs.spiShaderPgmHiVs.bits.MEM_BASE);
    }

    gpusize PsProgramGpuVa() const
    {
        return GetOriginalAddress(m_commands.sh.ps.spiShaderPgmLoPs.bits.MEM_BASE,
                                  m_commands.sh.ps.spiShaderPgmHiPs.bits.MEM_BASE);
    }

    const ShaderStageInfo& StageInfoVs() const { return m_stageInfoVs; }
    const ShaderStageInfo& StageInfoPs() const { return m_stageInfoPs; }

private:
    void BuildPm4Headers(
        bool                            enableLoadIndexPath,
        const GraphicsPipelineLoadInfo& loadInfo);

    // Pre-assembled "images" of the PM4 packets used for binding this pipeline to a command buffer.
    struct Pm4Commands
    {
        struct
        {
            struct
            {
                PM4_ME_SET_SH_REG           hdrSpiShaderPgm;
                regSPI_SHADER_PGM_LO_PS     spiShaderPgmLoPs;
                regSPI_SHADER_PGM_HI_PS     spiShaderPgmHiPs;
                regSPI_SHADER_PGM_RSRC1_PS  spiShaderPgmRsrc1Ps;
                regSPI_SHADER_PGM_RSRC2_PS  spiShaderPgmRsrc2Ps;

                PM4_ME_SET_SH_REG             hdrSpiShaderUserData;
                regSPI_SHADER_USER_DATA_PS_1  spiShaderUserDataLoPs;

                // Not all gfx10 devices support user accum registers. If we don't have it, NOP will be used.
                PM4_ME_SET_SH_REG             hdrSpishaderUserAccumPs;
                regSPI_SHADER_USER_ACCUM_PS_0 shaderUserAccumPs0;
                regSPI_SHADER_USER_ACCUM_PS_1 shaderUserAccumPs1;
                regSPI_SHADER_USER_ACCUM_PS_2 shaderUserAccumPs2;
                regSPI_SHADER_USER_ACCUM_PS_3 shaderUserAccumPs3;

                PM4_ME_SET_SH_REG             hdrShaderReqCtrlPs;
                regSPI_SHADER_REQ_CTRL_PS     shaderReqCtrlPs;

                // Checksum register is optional, as not all GFX9+ hardware uses it.
                // If we don't have it, NOP will be used.
                PM4_ME_SET_SH_REG            hdrSpiShaderPgmChksum;
                regSPI_SHADER_PGM_CHKSUM_PS  spiShaderPgmChksumPs;
                // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
                // w/ the actual commands contained above.
                size_t  spaceNeeded;
            } ps;

            struct
            {
                // If we don't set hdrSpiShaderPgm/ hdrSpiShaderUserData when NGG is disabled, we'll add NOP.
                PM4_ME_SET_SH_REG           hdrSpiShaderPgm;
                regSPI_SHADER_PGM_LO_VS     spiShaderPgmLoVs;
                regSPI_SHADER_PGM_HI_VS     spiShaderPgmHiVs;
                regSPI_SHADER_PGM_RSRC1_VS  spiShaderPgmRsrc1Vs;
                regSPI_SHADER_PGM_RSRC2_VS  spiShaderPgmRsrc2Vs;

                PM4_ME_SET_SH_REG             hdrSpiShaderUserData;
                regSPI_SHADER_USER_DATA_VS_1  spiShaderUserDataLoVs;

                // Not all gfx10 devices support user accum registers. If we don't have it, NOP will be added.
                PM4_ME_SET_SH_REG             hdrSpishaderUserAccumVs;
                regSPI_SHADER_USER_ACCUM_VS_0 shaderUserAccumVs0;
                regSPI_SHADER_USER_ACCUM_VS_1 shaderUserAccumVs1;
                regSPI_SHADER_USER_ACCUM_VS_2 shaderUserAccumVs2;
                regSPI_SHADER_USER_ACCUM_VS_3 shaderUserAccumVs3;

                PM4_ME_SET_SH_REG             hdrShaderReqCtrlVs;
                regSPI_SHADER_REQ_CTRL_VS     shaderReqCtrlVs;

                // Checksum register is optional, as not all GFX9+ hardware uses it.
                // If we don't have it, NOP will be added.
                PM4_ME_SET_SH_REG            hdrSpiShaderPgmChksum;
                regSPI_SHADER_PGM_CHKSUM_VS  spiShaderPgmChksumVs;
                // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
                // w/ the actual commands contained above.
                size_t  spaceNeeded;
            } vs;
        } sh; // Writes SH registers when using the SET path.

        struct
        {
            struct
            {
                PM4_ME_SET_SH_REG_INDEX     hdrPgmRsrc3Ps;
                regSPI_SHADER_PGM_RSRC3_PS  spiShaderPgmRsrc3Ps;

                PM4_ME_SET_SH_REG_INDEX     hdrPgmRsrc4Ps;
                regSPI_SHADER_PGM_RSRC4_PS  spiShaderPgmRsrc4Ps;

                // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
                // w/ the actual commands contained above.
                size_t  spaceNeeded;
            } ps;

            struct
            {
                PM4_ME_SET_SH_REG_INDEX     hdrPgmRsrc3Vs;
                regSPI_SHADER_PGM_RSRC3_VS  spiShaderPgmRsrc3Vs;

                PM4_ME_SET_SH_REG_INDEX     hdrPgmRsrc4Vs;
                regSPI_SHADER_PGM_RSRC4_VS  spiShaderPgmRsrc4Vs;
                // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
                // w/ the actual commands contained above.
                size_t  spaceNeeded;
            } vs;
        } dynamic; // Contains state which depends on bind-time parameters.

        struct
        {
            PM4ME_CONTEXT_REG_RMW  paScAaConfig;
        } common; // Packets which are common to both the SET and LOAD_INDEX paths (such as read-modify-writes).

        struct
        {
            PM4_PFP_SET_CONTEXT_REG   hdrSpiShaderFormat;
            regSPI_SHADER_Z_FORMAT    spiShaderZFormat;
            regSPI_SHADER_COL_FORMAT  spiShaderColFormat;

            PM4_PFP_SET_CONTEXT_REG  hdrSpiBarycCntl;
            regSPI_BARYC_CNTL        spiBarycCntl;

            PM4_PFP_SET_CONTEXT_REG  hdrSpiPsInput;
            regSPI_PS_INPUT_ENA      spiPsInputEna;
            regSPI_PS_INPUT_ADDR     spiPsInputAddr;

            PM4_PFP_SET_CONTEXT_REG  hdrDbShaderControl;
            regDB_SHADER_CONTROL     dbShaderControl;

            PM4_PFP_SET_CONTEXT_REG  hdrPaScBinnerCntl1;
            regPA_SC_BINNER_CNTL_1   paScBinnerCntl1;

            PM4_PFP_SET_CONTEXT_REG   hdrSpiShaderPosFormat;
            regSPI_SHADER_POS_FORMAT  spiShaderPosFormat;

            PM4_PFP_SET_CONTEXT_REG  hdrPaClVsOutCntl;
            regPA_CL_VS_OUT_CNTL     paClVsOutCntl;

            PM4_PFP_SET_CONTEXT_REG  hdrVgtPrimitiveIdEn;
            regVGT_PRIMITIVEID_EN    vgtPrimitiveIdEn;

            // SPI PS input control registers: between 0 and 32 of these will actually be written.  Note: Should always
            // be the last bunch of registers in the PM4 image because the amount of regs which will actually be written
            // varies between pipelines (based on SC output from compiling the shader.
            PM4_PFP_SET_CONTEXT_REG  hdrSpiPsInputCntl;
            regSPI_PS_INPUT_CNTL_0   spiPsInputCntl[MaxPsInputSemantics];

            // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
            // w/ the actual commands contained above.
            size_t  spaceNeeded;
        } context; // Writes context registers when using the SET path.

        struct
        {
            PM4_PFP_SET_CONTEXT_REG       headerStrmoutCfg;
            regVGT_STRMOUT_CONFIG         vgtStrmoutConfig;
            regVGT_STRMOUT_BUFFER_CONFIG  vgtStrmoutBufferConfig;

            struct
            {
                PM4_PFP_SET_CONTEXT_REG      header;
                regVGT_STRMOUT_VTX_STRIDE_0  vgtStrmoutVtxStride;
            } stride[MaxStreamOutTargets];

            // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
            // w/ the actual commands contained above.
            size_t  spaceNeeded;
        } streamOut; //  Writes stream-out context registers when using the SET path.
    };

    const Device&  m_device;
    Pm4Commands    m_commands;

    regPA_SC_SHADER_CONTROL  m_paScShaderControl;
    bool                     m_calcWaveBreakAtDrawTime;

    const PerfDataInfo*const  m_pVsPerfDataInfo;   // VS performance data information.
    const PerfDataInfo*const  m_pPsPerfDataInfo;   // PS performance data information.

    ShaderStageInfo  m_stageInfoVs;
    ShaderStageInfo  m_stageInfoPs;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkVsPs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkVsPs);
};

} // Gfx9
} // Pal
