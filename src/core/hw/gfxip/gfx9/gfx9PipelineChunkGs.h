/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
// Represents the chunk of a graphics pipeline object which contains all of the registers which setup the hardware GS
// stage.  This is sort of a PM4 "image" of the commands which write these registers, but with some intelligence so
// that the code used to setup the commands can be reused.
//
// These register values depend on the API-GS, and either the API-VS or API-DS, depending on which shader stages are
// active for the owning pipeline.
class PipelineChunkGs
{
public:
    PipelineChunkGs(
        const Device&       device,
        const PerfDataInfo* pPerfDataInfo);
    ~PipelineChunkGs() { }

    void EarlyInit(
        GraphicsPipelineLoadInfo* pInfo);

    void LateInit(
        const AbiProcessor&             abiProcessor,
        const CodeObjectMetadata&       metadata,
        const RegisterVector&           registers,
        const GraphicsPipelineLoadInfo& loadInfo,
        GraphicsPipelineUploader*       pUploader,
        Util::MetroHash64*              pHasher);

    uint32* WriteShCommands(
        CmdStream*              pCmdStream,
        uint32*                 pCmdSpace,
        const DynamicStageInfo& gsStageInfo) const;
    uint32* WriteContextCommands(
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const;

    uint32 GsVsRingItemSize() const { return m_commands.context.gsVsRingItemSize.bits.ITEMSIZE; }
    const regVGT_GS_ONCHIP_CNTL VgtGsOnchipCntl() const { return m_commands.context.vgtGsOnchipCntl; }

    gpusize EsProgramGpuVa() const
    {
        return GetOriginalAddress(m_commands.sh.spiShaderPgmLoEs.bits.MEM_BASE,
                                  m_commands.sh.spiShaderPgmHiEs.bits.MEM_BASE);
    }

    const ShaderStageInfo& StageInfo() const { return m_stageInfo; }

private:
    void BuildPm4Headers(
        bool                            enableLoadIndexPath,
        const GraphicsPipelineLoadInfo& loadInfo);

    // Pre-assembled "images" of the PM4 packets used for binding this pipeline to a command buffer.
    struct Pm4Commands
    {
        struct
        {
            PM4_ME_SET_SH_REG        hdrSpiShaderPgmGs;
            regSPI_SHADER_PGM_LO_ES  spiShaderPgmLoEs;
            regSPI_SHADER_PGM_HI_ES  spiShaderPgmHiEs;

            PM4_ME_SET_SH_REG           hdrSpiShaderPgmRsrcGs;
            regSPI_SHADER_PGM_RSRC1_GS  spiShaderPgmRsrc1Gs;
            regSPI_SHADER_PGM_RSRC2_GS  spiShaderPgmRsrc2Gs;

            PM4_ME_SET_SH_REG             hdrSpiShaderUserDataGs;
            regSPI_SHADER_USER_DATA_ES_1  spiShaderUserDataLoGs;

            // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
            // w/ the actual commands contained above.
            size_t  spaceNeeded;
        } sh; // Writes SH registers when using the SET path.

        struct
        {
            PM4_ME_SET_SH_REG             hdrEsGsSizeForGs;
            regSPI_SHADER_USER_DATA_ES_0  gsUserDataLdsEsGsSize;

            PM4_ME_SET_SH_REG             hdrEsGsSizeForVs;
            regSPI_SHADER_USER_DATA_VS_0  vsUserDataLdsEsGsSize;

            // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
            // w/ the actual commands contained above.
            size_t  spaceNeeded;
        } shLds; // Writes SH registers when using the SET path for ES/GS LDS bytes.

        struct
        {
            PM4_PFP_SET_CONTEXT_REG  hdrVgtGsMaxVertOut;
            regVGT_GS_MAX_VERT_OUT   vgtGsMaxVertOut;

            PM4_PFP_SET_CONTEXT_REG  hdrVgtGsOutPrimType;
            regVGT_GS_OUT_PRIM_TYPE  vgtGsOutPrimType;

            PM4_PFP_SET_CONTEXT_REG  hdrVgtGsInstanceCnt;
            regVGT_GS_INSTANCE_CNT   vgtGsInstanceCnt;

            PM4_PFP_SET_CONTEXT_REG    hdrEsGsVsRingItemSize;
            regVGT_ESGS_RING_ITEMSIZE  esGsRingItemSize;
            regVGT_GSVS_RING_ITEMSIZE  gsVsRingItemSize;

            PM4_PFP_SET_CONTEXT_REG    hdrVgtGsVsRingOffset;
            regVGT_GSVS_RING_OFFSET_1  vgtGsVsRingOffset1;
            regVGT_GSVS_RING_OFFSET_2  vgtGsVsRingOffset2;
            regVGT_GSVS_RING_OFFSET_3  vgtGsVsRingOffset3;

            PM4_PFP_SET_CONTEXT_REG  hdrVgtGsPerVs;
            regVGT_GS_PER_VS         vgtGsPerVs;

            PM4_PFP_SET_CONTEXT_REG    hdrVgtGsVertItemSize;
            regVGT_GS_VERT_ITEMSIZE    vgtGsVertItemSize0;
            regVGT_GS_VERT_ITEMSIZE_1  vgtGsVertItemSize1;
            regVGT_GS_VERT_ITEMSIZE_2  vgtGsVertItemSize2;
            regVGT_GS_VERT_ITEMSIZE_3  vgtGsVertItemSize3;

            PM4_ME_SET_CONTEXT_REG  hdrVgtMaxPrimsPerSubgrp;
            union
            {
                uint32                            u32All;
                regVGT_GS_MAX_PRIMS_PER_SUBGROUP  gfx9;
            } maxPrimsPerSubgrp;

            PM4_PFP_SET_CONTEXT_REG  hdrVgtGsOnchipCntl;
            regVGT_GS_ONCHIP_CNTL    vgtGsOnchipCntl;

            // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
            // w/ the actual commands contained above.
            size_t  spaceNeeded;
        } context; // Writes context registers when using the SET path.

        struct
        {
            PM4_ME_SET_SH_REG_INDEX     hdrPgmRsrc3Gs;
            regSPI_SHADER_PGM_RSRC3_GS  spiShaderPgmRsrc3Gs;

            PM4_ME_SET_SH_REG           hdrPgmRsrc4Gs;
            regSPI_SHADER_PGM_RSRC4_GS  spiShaderPgmRsrc4Gs;
        } dynamic; // Contains state which depends on bind-time parameters.
    };

    const Device&  m_device;
    Pm4Commands    m_commands;

    const PerfDataInfo*const  m_pPerfDataInfo;   // GS performance data information.

    ShaderStageInfo  m_stageInfo;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkGs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkGs);
};

} // Gfx9
} // Pal
