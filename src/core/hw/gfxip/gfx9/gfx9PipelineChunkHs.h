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

namespace Gfx9
{

class CmdStream;
class Device;
class PrefetchMgr;

// Initialization parameters.
struct HsParams
{
    gpusize             codeGpuVirtAddr;
    gpusize             dataGpuVirtAddr;
    const PerfDataInfo* pHsPerfDataInfo;
    Util::MetroHash64*  pHasher;
};

// =====================================================================================================================
// Represents the chunk of a graphics pipeline object which contains all of the registers which setup the hardware LS
// and HS stages.  This is sort of a PM4 "image" of the commands which write these registers, but with some intelligence
// so that the code used to setup the commands can be reused.
//
// These register values depend on the API-VS, and the API-HS.
class PipelineChunkHs
{
public:
    explicit PipelineChunkHs(const Device& device);
    ~PipelineChunkHs() { }

    void Init(
        const AbiProcessor& abiProcessor,
        const HsParams&     params);

    uint32* WriteShCommands(
        CmdStream*              pCmdStream,
        uint32*                 pCmdSpace,
        const DynamicStageInfo& hsStageInfo) const;
    uint32* WriteContextCommands(
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const;

    gpusize LsProgramGpuVa() const
    {
        return GetOriginalAddress(m_pm4ImageSh.spiShaderPgmLoLs.bits.MEM_BASE,
                                  m_pm4ImageSh.spiShaderPgmHiLs.bits.MEM_BASE);
    }

    const ShaderStageInfo& StageInfo() const { return m_stageInfo; }

private:
    void BuildPm4Headers();

    struct Pm4ImageSh
    {
        PM4_ME_SET_SH_REG             hdrSpiShaderUserData;
        regSPI_SHADER_USER_DATA_LS_1  spiShaderUserDataLoHs;

        PM4_ME_SET_SH_REG             hdrSpiShaderPgm;
        regSPI_SHADER_PGM_RSRC1_HS    spiShaderPgmRsrc1Hs;
        SpiShaderPgmRsrc2Hs           spiShaderPgmRsrc2Hs;

        PM4_ME_SET_SH_REG             hdrSpiShaderPgmLs;
        regSPI_SHADER_PGM_LO_LS       spiShaderPgmLoLs;
        regSPI_SHADER_PGM_HI_LS       spiShaderPgmHiLs;

        // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
        // actual commands contained within.
        size_t                        spaceNeeded;
    };

    // This is only for register writes determined during Pipeline Bind.
    struct Pm4ImageShDynamic
    {
        PM4_ME_SET_SH_REG_INDEX            hdrPgmRsrc3Hs;
        SpiShaderPgmRsrc3Hs                spiShaderPgmRsrc3Hs;

        // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
        // actual commands contained within.
        size_t spaceNeeded;
    };

    struct Pm4ImageContext
    {
        PM4_PFP_SET_CONTEXT_REG      hdrvVgtHosTessLevel;
        regVGT_HOS_MAX_TESS_LEVEL    vgtHosMaxTessLevel;
        regVGT_HOS_MIN_TESS_LEVEL    vgtHosMinTessLevel;

        // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
        // actual commands contained within.
        size_t spaceNeeded;
    };

    const Device&  m_device;

    Pm4ImageSh        m_pm4ImageSh;        // HS sh commands to be written when the associated pipeline is bound.
    Pm4ImageShDynamic m_pm4ImageShDynamic; // HS sh commands to be calculated and written when the associated pipeline
                                           // is bound.
    Pm4ImageContext   m_pm4ImageContext;   // HS context commands to be written when the associated pipeline is bound.

    const PerfDataInfo* m_pHsPerfDataInfo;   // HS performance data information.

    ShaderStageInfo  m_stageInfo;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkHs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkHs);
};

} // Gfx9
} // Pal
