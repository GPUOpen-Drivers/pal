/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "palPipelineAbiProcessor.h"

namespace Pal
{

class Platform;

namespace Gfx6
{

class  CmdStream;
class  Device;
class  GraphicsPipelineUploader;
struct GraphicsPipelineLoadInfo;

// =====================================================================================================================
// Represents the chunk of a graphics pipeline object which contains all of the registers which setup the hardware VS
// and PS stages.  This is sort of a PM4 "image" of the commands which write these registers, but with some intelligence
// so that the code used to setup the commands can be reused.
//
// These register values depend on the API-PS, and either the API-VS, API-GS or API-DS, depending on which shader stages
// are active for the owning pipeline.
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
        const RegisterVector&           registers,
        const GraphicsPipelineLoadInfo& loadInfo,
        GraphicsPipelineUploader*       pUploader,
        Util::MetroHash64*              pHasher);

    uint32* WriteShCommands(
        CmdStream*              pCmdStream,
        uint32*                 pCmdSpace,
        const DynamicStageInfo& vsStageInfo,
        const DynamicStageInfo& psStageInfo) const;
    uint32* WriteContextCommands(
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const;

    regVGT_STRMOUT_CONFIG VgtStrmoutConfig() const { return m_commands.streamOut.vgtStrmoutConfig; }
    regVGT_STRMOUT_BUFFER_CONFIG VgtStrmoutBufferConfig() const { return m_commands.streamOut.vgtStrmoutBufferConfig; }
    regVGT_STRMOUT_VTX_STRIDE_0 VgtStrmoutVtxStride(uint32 idx) const
        { return m_commands.streamOut.stride[idx].vgtStrmoutVtxStride; }

    regSPI_SHADER_Z_FORMAT SpiShaderZFormat() const { return m_commands.context.spiShaderZFormat; }
    regPA_CL_VS_OUT_CNTL PaClVsOutCntl() const { return m_commands.context.paClVsOutCntl; }
    regSPI_VS_OUT_CONFIG SpiVsOutConfig() const { return m_spiVsOutConfig; }
    regSPI_PS_IN_CONTROL SpiPsInControl() const { return m_spiPsInControl; }

    gpusize PsProgramGpuVa() const
    {
        return GetOriginalAddress(m_commands.sh.spiShaderPgmLoPs.bits.MEM_BASE,
                                  m_commands.sh.spiShaderPgmHiPs.bits.MEM_BASE);
    }

    gpusize VsProgramGpuVa() const
    {
        return GetOriginalAddress(m_commands.sh.spiShaderPgmLoVs.bits.MEM_BASE,
                                  m_commands.sh.spiShaderPgmHiVs.bits.MEM_BASE);
    }

    const ShaderStageInfo& StageInfoVs() const { return m_stageInfoVs; }
    const ShaderStageInfo& StageInfoPs() const { return m_stageInfoPs; }

    bool UsesStreamOut() const { return (VgtStrmoutConfig().u32All != 0); }

private:
    void BuildPm4Headers(bool enableLoadIndexPath, uint32 interpolatorCount);

    // Pre-assembled "images" of the PM4 packets used for binding this pipeline to a command buffer.
    struct Pm4Commands
    {
        struct
        {
            PM4CMDSETDATA               hdrSpiShaderPgmVs;
            regSPI_SHADER_PGM_LO_VS     spiShaderPgmLoVs;
            regSPI_SHADER_PGM_HI_VS     spiShaderPgmHiVs;
            regSPI_SHADER_PGM_RSRC1_VS  spiShaderPgmRsrc1Vs;
            regSPI_SHADER_PGM_RSRC2_VS  spiShaderPgmRsrc2Vs;

            PM4CMDSETDATA                 hdrSpiShaderUserDataVs;
            regSPI_SHADER_USER_DATA_VS_1  spiShaderUserDataLoVs;

            PM4CMDSETDATA               hdrSpiSHaderPgmPs;
            regSPI_SHADER_PGM_LO_PS     spiShaderPgmLoPs;
            regSPI_SHADER_PGM_HI_PS     spiShaderPgmHiPs;
            regSPI_SHADER_PGM_RSRC1_PS  spiShaderPgmRsrc1Ps;
            regSPI_SHADER_PGM_RSRC2_PS  spiShaderPgmRsrc2Ps;

            PM4CMDSETDATA                 hdrSpiShaderUserDataPs;
            regSPI_SHADER_USER_DATA_PS_1  spiShaderUserDataLoPs;
        } sh;

        struct
        {
            PM4CMDSETDATA                hdrOutFormat;
            regSPI_SHADER_POS_FORMAT     spiShaderPosFormat;
            regSPI_SHADER_Z_FORMAT       spiShaderZFormat;
            regSPI_SHADER_COL_FORMAT     spiShaderColFormat;

            PM4CMDSETDATA                hdrVsOutCntl;
            regPA_CL_VS_OUT_CNTL         paClVsOutCntl;

            PM4CMDSETDATA                hdrPrimId;
            regVGT_PRIMITIVEID_EN        vgtPrimitiveIdEn;

            PM4CMDSETDATA                hdrBarycCntl;
            regSPI_BARYC_CNTL            spiBarycCntl;

            PM4CMDSETDATA                hdrPsIn;
            regSPI_PS_INPUT_ENA          spiPsInputEna;
            regSPI_PS_INPUT_ADDR         spiPsInputAddr;

            // SPI PS input control registers: between 0 and 32 of these will actually be written.
            // NOTE: Should always be the last bunch of registers in the PM4 image because the amount of regs which
            // will actually be written varies between pipelines (based on SC output from compiling the shader.
            PM4CMDSETDATA                hdrPsInputs;
            regSPI_PS_INPUT_CNTL_0       spiPsInputCntl[MaxPsInputSemantics];

            // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
            // w/ the actual commands contained above.
            size_t  spaceNeeded;
        } context;

        struct
        {
            PM4CMDSETDATA                 hdrStrmoutCfg;
            regVGT_STRMOUT_CONFIG         vgtStrmoutConfig;
            regVGT_STRMOUT_BUFFER_CONFIG  vgtStrmoutBufferConfig;

            struct
            {
                PM4CMDSETDATA                hdrVgtStrmoutVtxStride;
                regVGT_STRMOUT_VTX_STRIDE_0  vgtStrmoutVtxStride;
            } stride[MaxStreamOutTargets];

            // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
            // w/ the actual commands contained above.
            size_t  spaceNeeded;
        } streamOut;

        struct
        {
            PM4CMDSETDATA                       hdrPgmRsrc3Vs;
            regSPI_SHADER_PGM_RSRC3_VS__CI__VI  spiShaderPgmRsrc3Vs;

            PM4CMDSETDATA                       hdrPgmRsrc3Ps;
            regSPI_SHADER_PGM_RSRC3_PS__CI__VI  spiShaderPgmRsrc3Ps;
        } dynamic;
    };

    const Device&  m_device;
    Pm4Commands    m_commands;

    // As an optimization to avoid context rolls by sacrificing parameter cache, these registers are values are forced
    // to current maximum and written at SwitchGraphicsPipeline.
    regSPI_VS_OUT_CONFIG  m_spiVsOutConfig;
    regSPI_PS_IN_CONTROL  m_spiPsInControl;

    const PerfDataInfo*const  m_pVsPerfDataInfo; // VS performance data information.
    const PerfDataInfo*const  m_pPsPerfDataInfo; // PS performance data information.

    ShaderStageInfo  m_stageInfoVs;
    ShaderStageInfo  m_stageInfoPs;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkVsPs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkVsPs);
};

} // Gfx6
} // Pal
