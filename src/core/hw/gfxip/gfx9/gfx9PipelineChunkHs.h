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
// Represents the chunk of a graphics pipeline object which contains all of the registers which setup the hardware LS
// and HS stages.  This is sort of a PM4 "image" of the commands which write these registers, but with some intelligence
// so that the code used to setup the commands can be reused.
//
// These register values depend on the API-VS, and the API-HS.
class PipelineChunkHs
{
public:
    PipelineChunkHs(
        const Device&       device,
        const PerfDataInfo* pPerfDataInfo);
    ~PipelineChunkHs() { }

    void EarlyInit(
        GraphicsPipelineLoadInfo* pInfo);

    void LateInit(
        const AbiReader&          abiReader,
        const RegisterVector&     registers,
        GraphicsPipelineUploader* pUploader,
        Util::MetroHash64*        pHasher);

    template <bool UseLoadIndexPath>
    uint32* WriteShCommands(
        CmdStream*              pCmdStream,
        uint32*                 pCmdSpace,
        const DynamicStageInfo& hsStageInfo) const;

    template <bool UseLoadIndexPath>
    uint32* WriteContextCommands(
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const;

    gpusize LsProgramGpuVa() const
    {
        return GetOriginalAddress(m_regs.sh.spiShaderPgmLoLs.bits.MEM_BASE,
                                  m_regs.sh.spiShaderPgmHiLs.bits.MEM_BASE);
    }

    const ShaderStageInfo& StageInfo() const { return m_stageInfo; }

private:
    const Device&  m_device;

    struct
    {
        struct
        {
            regSPI_SHADER_PGM_LO_LS     spiShaderPgmLoLs;
            regSPI_SHADER_PGM_HI_LS     spiShaderPgmHiLs;
            regSPI_SHADER_PGM_RSRC1_HS  spiShaderPgmRsrc1Hs;
            regSPI_SHADER_PGM_RSRC2_HS  spiShaderPgmRsrc2Hs;
            regSPI_SHADER_PGM_CHKSUM_HS spiShaderPgmChksumHs;
            uint32                      userDataInternalTable;
        } sh;
        struct
        {
            regVGT_HOS_MAX_TESS_LEVEL  vgtHosMaxTessLevel;
            regVGT_HOS_MIN_TESS_LEVEL  vgtHosMinTessLevel;
        } context;
        struct
        {
            regSPI_SHADER_PGM_RSRC3_HS  spiShaderPgmRsrc3Hs;
            regSPI_SHADER_PGM_RSRC4_HS  spiShaderPgmRsrc4Hs;
        } dynamic;
    }  m_regs;

    const PerfDataInfo*const  m_pHsPerfDataInfo;   // HS performance data information.
    ShaderStageInfo           m_stageInfo;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkHs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkHs);
};

} // Gfx9
} // Pal
