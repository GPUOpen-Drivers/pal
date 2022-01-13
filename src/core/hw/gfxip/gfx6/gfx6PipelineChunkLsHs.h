/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace Pal
{

class Platform;

namespace Gfx6
{

class  CmdStream;
class  Device;
struct GraphicsPipelineLoadInfo;

// =====================================================================================================================
// Represents the chunk of a graphics pipeline object which contains all of the registers which setup the hardware LS
// and HS stages.  This is sort of a PM4 "image" of the commands which write these registers, but with some intelligence
// so that the code used to setup the commands can be reused.
//
// These register values depend on the API-VS, and the API-HS.
class PipelineChunkLsHs
{
public:
    PipelineChunkLsHs(
        const Device&       device,
        const PerfDataInfo* pLsPerfDataInfo,
        const PerfDataInfo* pHsPerfDataInfo);
    ~PipelineChunkLsHs() { }

    void LateInit(
        const AbiReader&                abiReader,
        const RegisterVector&           registers,
        PipelineUploader*               pUploader,
        const GraphicsPipelineLoadInfo& loadInfo,
        Util::MetroHash64*              pHasher);

    uint32* WriteShCommands(
        CmdStream*              pCmdStream,
        uint32*                 pCmdSpace,
        const DynamicStageInfo& lsStageInfo,
        const DynamicStageInfo& hsStageInfo) const;

    uint32* WriteContextCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    gpusize LsProgramGpuVa() const
    {
        return GetOriginalAddress(m_regs.sh.spiShaderPgmLoLs.bits.MEM_BASE,
                                  m_regs.sh.spiShaderPgmHiLs.bits.MEM_BASE);
    }

    gpusize HsProgramGpuVa() const
    {
        return GetOriginalAddress(m_regs.sh.spiShaderPgmLoHs.bits.MEM_BASE,
                                  m_regs.sh.spiShaderPgmHiHs.bits.MEM_BASE);
    }

    const ShaderStageInfo& StageInfoLs() const { return m_stageInfoLs; }
    const ShaderStageInfo& StageInfoHs() const { return m_stageInfoHs; }

private:
    const Device&  m_device;

    struct
    {
        struct
        {
            regSPI_SHADER_PGM_LO_HS     spiShaderPgmLoHs;
            regSPI_SHADER_PGM_HI_HS     spiShaderPgmHiHs;
            regSPI_SHADER_PGM_RSRC1_HS  spiShaderPgmRsrc1Hs;
            regSPI_SHADER_PGM_RSRC2_HS  spiShaderPgmRsrc2Hs;

            regSPI_SHADER_PGM_LO_LS     spiShaderPgmLoLs;
            regSPI_SHADER_PGM_HI_LS     spiShaderPgmHiLs;
            regSPI_SHADER_PGM_RSRC1_LS  spiShaderPgmRsrc1Ls;
            regSPI_SHADER_PGM_RSRC2_LS  spiShaderPgmRsrc2Ls;

            regSPI_SHADER_USER_DATA_HS_0  userDataInternalTableHs;
            regSPI_SHADER_USER_DATA_LS_0  userDataInternalTableLs;
        } sh;

        struct
        {
            regVGT_HOS_MAX_TESS_LEVEL  vgtHosMaxTessLevel;
            regVGT_HOS_MIN_TESS_LEVEL  vgtHosMinTessLevel;
        } context;

        struct
        {
            regSPI_SHADER_PGM_RSRC3_LS__CI__VI  spiShaderPgmRsrc3Ls;
            regSPI_SHADER_PGM_RSRC3_HS__CI__VI  spiShaderPgmRsrc3Hs;
        } dynamic;
    }  m_regs;

    const PerfDataInfo*const  m_pLsPerfDataInfo; // LS performance data information.
    const PerfDataInfo*const  m_pHsPerfDataInfo; // HS performance data information.

    ShaderStageInfo  m_stageInfoLs;
    ShaderStageInfo  m_stageInfoHs;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkLsHs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkLsHs);
};

} // Gfx6
} // Pal
