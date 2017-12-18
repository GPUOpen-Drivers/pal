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
struct VsParams
{
    gpusize             codeGpuVirtAddr;
    gpusize             dataGpuVirtAddr;
    const PerfDataInfo* pVsPerfDataInfo;
    Util::MetroHash64*  pHasher;
};

// =====================================================================================================================
// Represents the chunk of a graphics pipeline object which contains all of the registers which setup the hardware VS
// stage.  This is sort of a PM4 "image" of the commands which write these registers, but with some intelligence so
// that the code used to setup the commands can be reused.
//
// These register values depend on the API-VS.
class PipelineChunkVs
{
public:
    explicit PipelineChunkVs(const Device& device);
    ~PipelineChunkVs() { }

    void Init(
        const AbiProcessor& abiProcessor,
        const VsParams&     params);

    uint32* WriteShCommands(
        CmdStream*              pCmdStream,
        uint32*                 pCmdSpace,
        const DynamicStageInfo& vsStageInfo) const;
    uint32* WriteContextCommands(
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const;

    gpusize VsProgramGpuVa() const
    {
        return GetOriginalAddress(m_pm4ImageSh.spiShaderPgmLoVs.bits.MEM_BASE,
                                  m_pm4ImageSh.spiShaderPgmHiVs.bits.MEM_BASE);
    }

    const ShaderStageInfo& StageInfo() const { return m_stageInfo; }

private:
    void BuildPm4Headers();

    struct Pm4ImageSh
    {
        PM4_ME_SET_SH_REG                 hdrSpiShaderPgmVs;
        regSPI_SHADER_PGM_LO_VS           spiShaderPgmLoVs;
        regSPI_SHADER_PGM_HI_VS           spiShaderPgmHiVs;
        regSPI_SHADER_PGM_RSRC1_VS        spiShaderPgmRsrc1Vs;   // per-shader internal table address
        SpiShaderPgmRsrc2Vs               spiShaderPgmRsrc2Vs;   // per-shader internal table address

        PM4_ME_SET_SH_REG                 hdrSpiShaderUserDataVs;
        regSPI_SHADER_USER_DATA_VS_1      spiShaderUserDataLoVs; // per-shader internal constant buffer table address

        // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
        // actual commands contained within.
        size_t                            spaceNeeded;
    };

    // This is only for register writes determined during Pipeline Bind.
    struct Pm4ImageShDynamic
    {
        PM4_ME_SET_SH_REG_INDEX           hdrPgmRsrc3Vs;
        regSPI_SHADER_PGM_RSRC3_VS        spiShaderPgmRsrc3Vs;

        // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
        // actual commands contained within.
        size_t spaceNeeded;
    };

    struct Pm4ImageContext
    {
        PM4_PFP_SET_CONTEXT_REG      hdrSpiShaderPosFormat;
        regSPI_SHADER_POS_FORMAT     spiShaderPosFormat;

        PM4_PFP_SET_CONTEXT_REG      hdrPaClVsOutCntl;
        PaClVsOutCntl                paClVsOutCntl;

        PM4_PFP_SET_CONTEXT_REG      hdrVgtPrimitiveIdEn;
        regVGT_PRIMITIVEID_EN        vgtPrimitiveIdEn;

        // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
        // actual commands contained within.
        size_t                       spaceNeeded;
    };

    const Device&  m_device;

    Pm4ImageSh        m_pm4ImageSh;        // VS sh commands to be written when the associated pipeline is bound
    Pm4ImageShDynamic m_pm4ImageShDynamic; // VS sh commands to be calculated and written when the associated pipeline
                                           // is bound.
    Pm4ImageContext   m_pm4ImageContext;   // VS context commands to be written when the associated pipeline is bound

    const PerfDataInfo* m_pVsPerfDataInfo; // VS performance data information.

    ShaderStageInfo   m_stageInfo;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkVs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkVs);
};

} // Gfx9
} // Pal
