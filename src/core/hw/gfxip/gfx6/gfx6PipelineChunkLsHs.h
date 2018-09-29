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

    void EarlyInit(
        GraphicsPipelineLoadInfo* pInfo);

    void LateInit(
        const AbiProcessor&       abiProcessor,
        const RegisterVector&     registers,
        GraphicsPipelineUploader* pUploader,
        Util::MetroHash64*        pHasher);

    uint32* WriteShCommands(
        CmdStream*              pCmdStream,
        uint32*                 pCmdSpace,
        const DynamicStageInfo& lsStageInfo,
        const DynamicStageInfo& hsStageInfo) const;
    uint32* WriteContextCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    gpusize LsProgramGpuVa() const
    {
        return GetOriginalAddress(m_commands.sh.spiShaderPgmLoLs.bits.MEM_BASE,
                                  m_commands.sh.spiShaderPgmHiLs.bits.MEM_BASE);
    }

    gpusize HsProgramGpuVa() const
    {
        return GetOriginalAddress(m_commands.sh.spiShaderPgmLoHs.bits.MEM_BASE,
                                  m_commands.sh.spiShaderPgmHiHs.bits.MEM_BASE);
    }

    const ShaderStageInfo& StageInfoLs() const { return m_stageInfoLs; }
    const ShaderStageInfo& StageInfoHs() const { return m_stageInfoHs; }

private:
    void BuildPm4Headers(bool enableLoadIndexPath);

    // Pre-assembled "images" of the PM4 packets used for binding this pipeline to a command buffer.
    struct Pm4Commands
    {
        struct
        {
            PM4CMDSETDATA               hdrSpiShaderPgmHs;
            regSPI_SHADER_PGM_LO_HS     spiShaderPgmLoHs;
            regSPI_SHADER_PGM_HI_HS     spiShaderPgmHiHs;
            regSPI_SHADER_PGM_RSRC1_HS  spiShaderPgmRsrc1Hs;
            regSPI_SHADER_PGM_RSRC2_HS  spiShaderPgmRsrc2Hs;

            PM4CMDSETDATA                 hdrSpiShaderUserDataHs;
            regSPI_SHADER_USER_DATA_HS_1  spiShaderUserDataLoHs;

            PM4CMDSETDATA                 hdrSpiShaderUserDataLs;
            regSPI_SHADER_USER_DATA_LS_1  spiShaderUserDataLoLs;

            // NOTE: Due to a hardware bug, we may need to issue multiple writes of the SPI_SHADER_RSRC2_LS register.  This
            // packet (which issues the normal write) must immediately precede the ones inside the 'spiBug' structure below.
            PM4CMDSETDATA               hdrSpiShaderPgmLs;
            regSPI_SHADER_PGM_LO_LS     spiShaderPgmLoLs;
            regSPI_SHADER_PGM_HI_LS     spiShaderPgmHiLs;
            regSPI_SHADER_PGM_RSRC1_LS  spiShaderPgmRsrc1Ls;
            regSPI_SHADER_PGM_RSRC2_LS  spiShaderPgmRsrc2Ls;

            // Optional PM4 packet used for an SPI hardware bug workaround.
            struct
            {
                PM4CMDSETDATA               hdrSpiShaderPgmRsrcLs;
                regSPI_SHADER_PGM_RSRC1_LS  spiShaderPgmRsrc1Ls;
                regSPI_SHADER_PGM_RSRC2_LS  spiShaderPgmRsrc2Ls;
            } spiBug;

            // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
            // w/ the actual commands contained above.
            size_t  spaceNeeded;
        } sh;

        struct
        {
            PM4CMDSETDATA              hdrVgtHosTessLevel;
            regVGT_HOS_MAX_TESS_LEVEL  vgtHosMaxTessLevel;
            regVGT_HOS_MIN_TESS_LEVEL  vgtHosMinTessLevel;
        } context;

        struct
        {
            PM4CMDSETDATA                       hdrPgmRsrc3Ls;
            regSPI_SHADER_PGM_RSRC3_LS__CI__VI  spiShaderPgmRsrc3Ls;

            PM4CMDSETDATA                       hdrPgmRsrc3Hs;
            regSPI_SHADER_PGM_RSRC3_HS__CI__VI  spiShaderPgmRsrc3Hs;
        } dynamic;
    };

    const Device&  m_device;
    Pm4Commands    m_commands;

    const PerfDataInfo*const  m_pLsPerfDataInfo; // LS performance data information.
    const PerfDataInfo*const  m_pHsPerfDataInfo; // HS performance data information.

    ShaderStageInfo  m_stageInfoLs;
    ShaderStageInfo  m_stageInfoHs;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkLsHs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkLsHs);
};

} // Gfx6
} // Pal
