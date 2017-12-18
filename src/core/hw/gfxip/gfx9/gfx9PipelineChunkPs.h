/*
 *******************************************************************************
 *
 * Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include "core/hw/gfxip/pipeline.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "palPipelineAbiProcessor.h"

namespace Pal
{

class Platform;
class PrefetchMgr;

namespace Gfx9
{

class CmdStream;
class Device;

// Initialization parameters.
struct PsParams
{
    gpusize             codeGpuVirtAddr;
    gpusize             dataGpuVirtAddr;
    bool                isNgg;
    const PerfDataInfo* pPsPerfDataInfo;
    Util::MetroHash64*  pHasher;
};

// =====================================================================================================================
// Represents the chunk of a graphics pipeline object which contains all of the registers which setup the hardware PS
// stage.  This is sort of a PM4 "image" of the commands which write these registers, but with some intelligence so
// that the code used to setup the commands can be reused.
//
// These register values depend on the API-PS.
class PipelineChunkPs
{
public:
    explicit PipelineChunkPs(const Device& device);
    ~PipelineChunkPs() { }

    void Init(
        const AbiProcessor& abiProcessor,
        const PsParams&     params);

    uint32* WriteShCommands(
        CmdStream*              pCmdStream,
        uint32*                 pCmdSpace,
        const DynamicStageInfo& psStageInfo) const;
    uint32* WriteContextCommands(
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const;

    regSPI_SHADER_Z_FORMAT SpiShaderZFormat() const { return m_pm4ImageContext.spiShaderZFormat; }
    regDB_SHADER_CONTROL DbShaderControl() const { return m_pm4ImageContext.dbShaderControl; }
    regPA_SC_AA_CONFIG PaScAaConfig() const
        { return *reinterpret_cast<const regPA_SC_AA_CONFIG*>(&m_pm4ImageContext.paScAaConfig.reg_data); }

    // Shortcut for checking if the shader has enabled INNER_COVERAGE mode.
    bool UsesInnerCoverage() const
        { return (PaScAaConfig().bits.COVERAGE_TO_SHADER_SELECT == INPUT_INNER_COVERAGE); }

    gpusize PsProgramGpuVa() const
    {
        return GetOriginalAddress(m_pm4ImageSh.spiShaderPgmLoPs.bits.MEM_BASE,
                                  m_pm4ImageSh.spiShaderPgmHiPs.bits.MEM_BASE);
    }

    const ShaderStageInfo& StageInfo() const { return m_stageInfo; }

private:
    void BuildPm4Headers(uint32 lastPsInterpolator);

    struct Pm4ImageSh
    {
        PM4_ME_SET_SH_REG                 hdrSpiShaderPgm;
        regSPI_SHADER_PGM_LO_PS           spiShaderPgmLoPs;
        regSPI_SHADER_PGM_HI_PS           spiShaderPgmHiPs;
        regSPI_SHADER_PGM_RSRC1_PS        spiShaderPgmRsrc1Ps;   // per-shader internal table address
        SpiShaderPgmRsrc2Ps               spiShaderPgmRsrc2Ps;   // per-shader internal table address

        PM4_ME_SET_SH_REG                 hdrSpiShaderUserData;
        regSPI_SHADER_USER_DATA_PS_1      spiShaderUserDataLoPs; // per-shader internal constant buffer table address

        // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
        // actual commands contained within.
        size_t                            spaceNeeded;
    };

    // This is only for register writes determined during Pipeline Bind.
    struct Pm4ImageShDynamic
    {
        PM4_ME_SET_SH_REG_INDEX           hdrPgmRsrc3Ps;
        regSPI_SHADER_PGM_RSRC3_PS        spiShaderPgmRsrc3Ps;

        // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
        // actual commands contained within.
        size_t spaceNeeded;
    };

    struct Pm4ImageContext
    {
        PM4_PFP_SET_CONTEXT_REG      hdrSpiShaderFormat;
        regSPI_SHADER_Z_FORMAT       spiShaderZFormat;
        regSPI_SHADER_COL_FORMAT     spiShaderColFormat;

        PM4_PFP_SET_CONTEXT_REG      hdrSpiBarycCntl;
        regSPI_BARYC_CNTL            spiBarycCntl;

        PM4_PFP_SET_CONTEXT_REG      hdrSpiPsInput;
        regSPI_PS_INPUT_ENA          spiPsInputEna;
        regSPI_PS_INPUT_ADDR         spiPsInputAddr;

        PM4_PFP_SET_CONTEXT_REG      hdrDbShaderControl;
        regDB_SHADER_CONTROL         dbShaderControl;

        PM4_PFP_SET_CONTEXT_REG      hdrPaScShaderControl;
        regPA_SC_SHADER_CONTROL      paScShaderControl;

        PM4_PFP_SET_CONTEXT_REG      hdrPaScBinnerCntl1;
        regPA_SC_BINNER_CNTL_1       paScBinnerCntl1;

        PM4ME_CONTEXT_REG_RMW        paScAaConfig;
        PM4ME_CONTEXT_REG_RMW        paScConservativeRastCntl;

        // SPI PS input control registers: between 0 and 32 of these will actually be written.  Note: Should always be
        // the last bunch of registers in the PM4 image because the amount of regs which will actually be written varies
        // between pipelines (based on SC output from compiling the shader.
        PM4_PFP_SET_CONTEXT_REG      hdrSpiPsInputCntl;
        regSPI_PS_INPUT_CNTL_0       spiPsInputCntl[MaxPsInputSemantics];

        // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
        // actual commands contained within.
        size_t                       spaceNeeded;
    };

    const Device&  m_device;

    Pm4ImageSh        m_pm4ImageSh;        // PS sh commands to be written when the associated pipeline is bound.
    Pm4ImageShDynamic m_pm4ImageShDynamic; // PS sh commands to be calculated and written when the associated pipeline
                                           // is bound.
    Pm4ImageContext   m_pm4ImageContext;   // PS context commands to be written when the associated pipeline is bound.

    const PerfDataInfo* m_pPsPerfDataInfo;   // PS performance data information.

    ShaderStageInfo   m_stageInfo;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkPs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkPs);
};

} // Gfx9
} // Pal
