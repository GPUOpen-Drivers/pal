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
        const AbiProcessor&       abiProcessor,
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
        return GetOriginalAddress(m_commands.sh.spiShaderPgmLoLs.bits.MEM_BASE,
                                  m_commands.sh.spiShaderPgmHiLs.bits.MEM_BASE);
    }

    const ShaderStageInfo& StageInfo() const { return m_stageInfo; }

private:
    void BuildPm4Headers(bool enableLoadIndexPath);

    // Pre-assembled "images" of the PM4 packets used for binding this pipeline to a command buffer.
    struct Pm4Commands
    {
        struct
        {
            PM4_ME_SET_SH_REG        hdrSpiShaderPgmHs;
            regSPI_SHADER_PGM_LO_LS  spiShaderPgmLoLs;
            regSPI_SHADER_PGM_HI_LS  spiShaderPgmHiLs;

            PM4_ME_SET_SH_REG           hdrSpiShaderPgmRsrcHs;
            regSPI_SHADER_PGM_RSRC1_HS  spiShaderPgmRsrc1Hs;
            regSPI_SHADER_PGM_RSRC2_HS  spiShaderPgmRsrc2Hs;

            PM4_ME_SET_SH_REG             hdrSpiShaderUserDataHs;
            regSPI_SHADER_USER_DATA_LS_1  spiShaderUserDataLoHs;

            // Checksum register is optional, as not all GFX9+ hardware uses it. If we don't have it, NOP will be added.
            PM4_ME_SET_SH_REG            hdrSpiShaderPgmChksum;
            regSPI_SHADER_PGM_CHKSUM_HS  spiShaderPgmChksumHs;
            // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
            // w/ the actual commands contained above.
            size_t  spaceNeeded;
        } sh; // Writes SH registers when using the SET path.

        struct
        {
            PM4_PFP_SET_CONTEXT_REG    hdrvVgtHosTessLevel;
            regVGT_HOS_MAX_TESS_LEVEL  vgtHosMaxTessLevel;
            regVGT_HOS_MIN_TESS_LEVEL  vgtHosMinTessLevel;
        } context; // Writes context registers when using the SET path.

        struct
        {
            PM4_ME_SET_SH_REG_INDEX     hdrPgmRsrc3Hs;
            regSPI_SHADER_PGM_RSRC3_HS  spiShaderPgmRsrc3Hs;

            // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
            // w/ the actual commands contained above.
            size_t  spaceNeeded;
        } dynamic; // Contains state which depends on bind-time parameters.
    };

    const Device&  m_device;
    Pm4Commands    m_commands;

    const PerfDataInfo*const  m_pHsPerfDataInfo;   // HS performance data information.

    ShaderStageInfo  m_stageInfo;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkHs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkHs);
};

} // Gfx9
} // Pal
