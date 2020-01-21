/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

    template <bool UseLoadIndexPath>
    uint32* WriteShCommands(
        CmdStream*              pCmdStream,
        uint32*                 pCmdSpace,
        const DynamicStageInfo& esStageInfo,
        const DynamicStageInfo& gsStageInfo) const;

    template <bool UseLoadIndexPath>
    uint32* WriteContextCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    gpusize GsProgramGpuVa() const
    {
        return GetOriginalAddress(m_regs.sh.spiShaderPgmLoGs.bits.MEM_BASE,
                                  m_regs.sh.spiShaderPgmHiGs.bits.MEM_BASE);
    }

    gpusize EsProgramGpuVa() const
    {
        return GetOriginalAddress(m_regs.sh.spiShaderPgmLoEs.bits.MEM_BASE,
                                  m_regs.sh.spiShaderPgmHiEs.bits.MEM_BASE);
    }

    const ShaderStageInfo& StageInfoEs() const { return m_stageInfoEs; }
    const ShaderStageInfo& StageInfoGs() const { return m_stageInfoGs; }

    uint32 EsGsRingItemSize() const { return m_regs.context.esGsRingItemsize.bits.ITEMSIZE; }
    uint32 GsVsRingItemSize() const { return m_regs.context.gsVsRingItemsize.bits.ITEMSIZE; }

private:
    const Device&  m_device;

    struct
    {
        struct
        {
            regSPI_SHADER_PGM_LO_ES     spiShaderPgmLoEs;
            regSPI_SHADER_PGM_HI_ES     spiShaderPgmHiEs;
            regSPI_SHADER_PGM_RSRC1_ES  spiShaderPgmRsrc1Es;
            regSPI_SHADER_PGM_RSRC2_ES  spiShaderPgmRsrc2Es;

            regSPI_SHADER_PGM_LO_GS     spiShaderPgmLoGs;
            regSPI_SHADER_PGM_HI_GS     spiShaderPgmHiGs;
            regSPI_SHADER_PGM_RSRC1_GS  spiShaderPgmRsrc1Gs;
            regSPI_SHADER_PGM_RSRC2_GS  spiShaderPgmRsrc2Gs;

            regSPI_SHADER_USER_DATA_ES_0  userDataInternalTableEs;
            regSPI_SHADER_USER_DATA_GS_0  userDataInternalTableGs;
            regSPI_SHADER_USER_DATA_GS_0  userDataLdsEsGsSize;

            uint16  ldsEsGsSizeRegAddrGs;
            uint16  ldsEsGsSizeRegAddrVs;
        } sh;

        struct
        {
            regVGT_GS_MAX_VERT_OUT         vgtGsMaxVertOut;
            regVGT_GS_OUT_PRIM_TYPE        vgtGsOutPrimType;
            regVGT_GS_INSTANCE_CNT         vgtGsInstanceCnt;
            regVGT_GS_PER_ES               vgtGsPerEs;
            regVGT_ES_PER_GS               vgtEsPerGs;
            regVGT_GS_PER_VS               vgtGsPerVs;
            regVGT_GS_VERT_ITEMSIZE        vgtGsVertItemSize0;
            regVGT_GS_VERT_ITEMSIZE_1      vgtGsVertItemSize1;
            regVGT_GS_VERT_ITEMSIZE_2      vgtGsVertItemSize2;
            regVGT_GS_VERT_ITEMSIZE_3      vgtGsVertItemSize3;
            regVGT_ESGS_RING_ITEMSIZE      esGsRingItemsize;
            regVGT_GSVS_RING_ITEMSIZE      gsVsRingItemsize;
            regVGT_GSVS_RING_OFFSET_1      ringOffset1;
            regVGT_GSVS_RING_OFFSET_2      ringOffset2;
            regVGT_GSVS_RING_OFFSET_3      ringOffset3;
            regVGT_GS_ONCHIP_CNTL__CI__VI  vgtGsOnchipCntl;
        } context;

        struct
        {
            regSPI_SHADER_PGM_RSRC3_ES__CI__VI  spiShaderPgmRsrc3Es;
            regSPI_SHADER_PGM_RSRC3_GS__CI__VI  spiShaderPgmRsrc3Gs;
        } dynamic;
    }  m_regs;

    const PerfDataInfo*const  m_pEsPerfDataInfo; // ES performance data information.
    const PerfDataInfo*const  m_pGsPerfDataInfo; // GS performance data information.

    ShaderStageInfo  m_stageInfoEs;
    ShaderStageInfo  m_stageInfoGs;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkEsGs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkEsGs);
};

} // Gfx6
} // Pal
