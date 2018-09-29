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
// Represents the chunk of a graphics pipeline object which contains all of the registers which setup the hardware ES
// and GS stages.  This is sort of a PM4 "image" of the commands which write these registers, but with some intelligence
// so that the code used to setup the commands can be reused.
//
// These register values depend on the API-GS, and either the API-VS or API-DS, depending on which shader stages are
// active for the owning pipeline.
class PipelineChunkEsGs
{
public:
    PipelineChunkEsGs(
        const Device&       device,
        const PerfDataInfo* pEsPerfDataInfo,
        const PerfDataInfo* pGsPerfDataInfo);
    ~PipelineChunkEsGs() { }

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
        const DynamicStageInfo& esStageInfo,
        const DynamicStageInfo& gsStageInfo) const;
    uint32* WriteContextCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    gpusize GsProgramGpuVa() const
    {
        return GetOriginalAddress(m_commands.sh.spiShaderPgmLoGs.bits.MEM_BASE,
                                  m_commands.sh.spiShaderPgmHiGs.bits.MEM_BASE);
    }

    gpusize EsProgramGpuVa() const
    {
        return GetOriginalAddress(m_commands.sh.spiShaderPgmLoEs.bits.MEM_BASE,
                                  m_commands.sh.spiShaderPgmHiEs.bits.MEM_BASE);
    }

    const ShaderStageInfo& StageInfoEs() const { return m_stageInfoEs; }
    const ShaderStageInfo& StageInfoGs() const { return m_stageInfoGs; }

    uint32 EsGsRingItemSize() const { return m_commands.context.esGsRingItemsize.bits.ITEMSIZE; }
    uint32 GsVsRingItemSize() const { return m_commands.context.gsVsRingItemsize.bits.ITEMSIZE; }

private:
    void BuildPm4Headers(
        bool   enableLoadIndexPath,
        bool   useOnchipGs,
        uint16 esGsLdsSizeRegGs,
        uint16 esGsLdsSizeRegVs);

    // Pre-assembled "images" of the PM4 packets used for binding this pipeline to a command buffer.
    struct Pm4Commands
    {
        struct
        {
            PM4CMDSETDATA               hdrSpiShaderPgmEs;
            regSPI_SHADER_PGM_LO_ES     spiShaderPgmLoEs;
            regSPI_SHADER_PGM_HI_ES     spiShaderPgmHiEs;
            regSPI_SHADER_PGM_RSRC1_ES  spiShaderPgmRsrc1Es;
            regSPI_SHADER_PGM_RSRC2_ES  spiShaderPgmRsrc2Es;

            PM4CMDSETDATA                 hdrSpiShaderUserDataEs;
            regSPI_SHADER_USER_DATA_ES_1  spiShaderUserDataLoEs;

            PM4CMDSETDATA               hdrSpiShaderPgmGs;
            regSPI_SHADER_PGM_LO_GS     spiShaderPgmLoGs;
            regSPI_SHADER_PGM_HI_GS     spiShaderPgmHiGs;
            regSPI_SHADER_PGM_RSRC1_GS  spiShaderPgmRsrc1Gs;
            regSPI_SHADER_PGM_RSRC2_GS  spiShaderPgmRsrc2Gs;

            PM4CMDSETDATA                 hdrSpiShaderUserDataGs;
            regSPI_SHADER_USER_DATA_GS_1  spiShaderUserDataLoGs;

            // These two packets need to go last because they are only needed if on-chip GS is enabled.
            PM4CMDSETDATA                  hdrGsUserData;
            regSPI_SHADER_USER_DATA_GS_12  gsUserDataLdsEsGsSize;

            PM4CMDSETDATA                  hdrVsUserData;
            regSPI_SHADER_USER_DATA_VS_12  vsUserDataLdsEsGsSize;

            // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
            // w/ the actual commands contained above.
            size_t  spaceNeeded;
        } sh;

        struct
        {
            PM4CMDSETDATA           hdrVgtGsMaxVertOut;
            regVGT_GS_MAX_VERT_OUT  vgtGsMaxVertOut;

            PM4CMDSETDATA            hdrVgtGsOutPrimType;
            regVGT_GS_OUT_PRIM_TYPE  vgtGsOutPrimType;

            PM4CMDSETDATA           hdrVgtGsInstanceCnt;
            regVGT_GS_INSTANCE_CNT  vgtGsInstanceCnt;

            PM4CMDSETDATA     hdrVgtGsPerEs;
            regVGT_GS_PER_ES  vgtGsPerEs;
            regVGT_ES_PER_GS  vgtEsPerGs;
            regVGT_GS_PER_VS  vgtGsPerVs;

            PM4CMDSETDATA              hdrVgtGsVertItemSize;
            regVGT_GS_VERT_ITEMSIZE    vgtGsVertItemSize0;
            regVGT_GS_VERT_ITEMSIZE_1  vgtGsVertItemSize1;
            regVGT_GS_VERT_ITEMSIZE_2  vgtGsVertItemSize2;
            regVGT_GS_VERT_ITEMSIZE_3  vgtGsVertItemSize3;

            PM4CMDSETDATA              hdrRingItemsize;
            regVGT_ESGS_RING_ITEMSIZE  esGsRingItemsize;
            regVGT_GSVS_RING_ITEMSIZE  gsVsRingItemsize;

            PM4CMDSETDATA              hdrRingOffset;
            regVGT_GSVS_RING_OFFSET_1  ringOffset1;
            regVGT_GSVS_RING_OFFSET_2  ringOffset2;
            regVGT_GSVS_RING_OFFSET_3  ringOffset3;

            // NOTE: The following PM4 packets are only needed on GFX7 and newer hardware.  They should be the last
            // register values in the structure.
            PM4CMDSETDATA                  hdrGsOnchipCnt;
            regVGT_GS_ONCHIP_CNTL__CI__VI  vgtGsOnchipCntl;

            // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
            // w/ the actual commands contained above.
            size_t  spaceNeeded;
        } context;

        struct
        {
            PM4CMDSETDATA                       hdrPgmRsrc3Es;
            regSPI_SHADER_PGM_RSRC3_ES__CI__VI  spiShaderPgmRsrc3Es;

            PM4CMDSETDATA                       hdrPgmRsrc3Gs;
            regSPI_SHADER_PGM_RSRC3_GS__CI__VI  spiShaderPgmRsrc3Gs;
        } dynamic;
    };

    const Device&  m_device;
    Pm4Commands    m_commands;

    const PerfDataInfo*const  m_pEsPerfDataInfo; // ES performance data information.
    const PerfDataInfo*const  m_pGsPerfDataInfo; // GS performance data information.

    ShaderStageInfo  m_stageInfoEs;
    ShaderStageInfo  m_stageInfoGs;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkEsGs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkEsGs);
};

} // Gfx6
} // Pal
