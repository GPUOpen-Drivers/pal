/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

class CmdStream;
class Device;
class PrefetchMgr;

// Initialization parameters.
struct EsGsParams
{
    gpusize             codeGpuVirtAddr;
    gpusize             dataGpuVirtAddr;
    bool                usesOnChipGs;
    uint16              esGsLdsSizeRegGs;
    uint16              esGsLdsSizeRegVs;
    const PerfDataInfo* pEsPerfDataInfo;
    const PerfDataInfo* pGsPerfDataInfo;
    Util::MetroHash64*  pHasher;
};

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
    explicit PipelineChunkEsGs(const Device& device);
    ~PipelineChunkEsGs() { }

    void Init(
        const AbiProcessor& abiProcessor,
        const EsGsParams&   params);

    uint32* WriteShCommands(
        CmdStream*              pCmdStream,
        uint32*                 pCmdSpace,
        const DynamicStageInfo& esStageInfo,
        const DynamicStageInfo& gsStageInfo) const;
    uint32* WriteContextCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    gpusize GsProgramGpuVa() const
    {
        return GetOriginalAddress(m_pm4ImageSh.spiShaderPgmLoGs.bits.MEM_BASE,
                                  m_pm4ImageSh.spiShaderPgmHiGs.bits.MEM_BASE);
    }

    gpusize EsProgramGpuVa() const
    {
        return GetOriginalAddress(m_pm4ImageSh.spiShaderPgmLoEs.bits.MEM_BASE,
                                  m_pm4ImageSh.spiShaderPgmHiEs.bits.MEM_BASE);
    }

    const ShaderStageInfo& StageInfoEs() const { return m_stageInfoEs; }
    const ShaderStageInfo& StageInfoGs() const { return m_stageInfoGs; }

    uint32 EsGsRingItemSize() const { return m_pm4ImageContext.esGsRingItemsize.bits.ITEMSIZE; }
    uint32 GsVsRingItemSize() const { return m_pm4ImageContext.gsVsRingItemsize.bits.ITEMSIZE; }

    // Returns the register addresses for the user-data registers where the ES/GS ring buffer LDS size is passed to the
    // pipeline. (Only valid for pipelines which use on-chip GS).
    uint16 EsGsLdsSizeRegAddrGs() const
        { return static_cast<uint16>(m_pm4ImageSh.hdrGsUserData.regOffset + PERSISTENT_SPACE_START); }
    uint16 EsGsLdsSizeRegAddrVs() const
        { return static_cast<uint16>(m_pm4ImageSh.hdrVsUserData.regOffset + PERSISTENT_SPACE_START); }

private:
    void BuildPm4Headers(bool useOnchipGs, uint16 esGsLdsSizeRegGs, uint16 esGsLdsSizeRegVs);

    // Only non-context register writes go in here.
    struct Pm4ImageSh
    {
        PM4CMDSETDATA                      hdrSpiShaderPgmEs;
        regSPI_SHADER_PGM_LO_ES            spiShaderPgmLoEs;
        regSPI_SHADER_PGM_HI_ES            spiShaderPgmHiEs;
        regSPI_SHADER_PGM_RSRC1_ES         spiShaderPgmRsrc1Es;
        regSPI_SHADER_PGM_RSRC2_ES         spiShaderPgmRsrc2Es;

        PM4CMDSETDATA                      hdrSpiShaderUserDataEs;
        regSPI_SHADER_USER_DATA_ES_1       spiShaderUserDataLoEs;

        PM4CMDSETDATA                      hdrSpiShaderPgmGs;
        regSPI_SHADER_PGM_LO_GS            spiShaderPgmLoGs;
        regSPI_SHADER_PGM_HI_GS            spiShaderPgmHiGs;
        regSPI_SHADER_PGM_RSRC1_GS         spiShaderPgmRsrc1Gs;
        regSPI_SHADER_PGM_RSRC2_GS         spiShaderPgmRsrc2Gs;

        PM4CMDSETDATA                      hdrSpiShaderUserDataGs;
        regSPI_SHADER_USER_DATA_GS_1       spiShaderUserDataLoGs;

        // These two packets need to go last because they are only needed if on-chip GS is enabled.
        PM4CMDSETDATA                      hdrGsUserData;
        regSPI_SHADER_USER_DATA_GS_12      gsUserDataLdsEsGsSize;

        PM4CMDSETDATA                      hdrVsUserData;
        regSPI_SHADER_USER_DATA_VS_12      vsUserDataLdsEsGsSize;

        // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
        // actual commands contained within.
        size_t                             spaceNeeded;
    };

    // This is only for register writes determined during Pipeline Bind.
    struct Pm4ImageShDynamic
    {
        // Note: The following PM4 packets are only needed on GFX7 and newer hardware.
        PM4CMDSETDATA                      hdrPgmRsrc3Es;
        regSPI_SHADER_PGM_RSRC3_ES__CI__VI spiShaderPgmRsrc3Es;

        PM4CMDSETDATA                      hdrPgmRsrc3Gs;
        regSPI_SHADER_PGM_RSRC3_GS__CI__VI spiShaderPgmRsrc3Gs;

        // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
        // actual commands contained within.
        size_t spaceNeeded;
    };

    // This is only for context register writes.
    struct Pm4ImageContext
    {
        PM4CMDSETDATA                hdrVgtGsMaxVertOut;
        regVGT_GS_MAX_VERT_OUT       vgtGsMaxVertOut;

        PM4CMDSETDATA                hdrVgtGsOutPrimType;
        regVGT_GS_OUT_PRIM_TYPE      vgtGsOutPrimType;

        PM4CMDSETDATA                hdrVgtGsInstanceCnt;
        regVGT_GS_INSTANCE_CNT       vgtGsInstanceCnt;

        PM4CMDSETDATA                hdrVgtGsPerEs;
        regVGT_GS_PER_ES             vgtGsPerEs;
        regVGT_ES_PER_GS             vgtEsPerGs;
        regVGT_GS_PER_VS             vgtGsPerVs;

        PM4CMDSETDATA                hdrVgtGsVertItemSize;
        regVGT_GS_VERT_ITEMSIZE      vgtGsVertItemSize0;
        regVGT_GS_VERT_ITEMSIZE_1    vgtGsVertItemSize1;
        regVGT_GS_VERT_ITEMSIZE_2    vgtGsVertItemSize2;
        regVGT_GS_VERT_ITEMSIZE_3    vgtGsVertItemSize3;

        PM4CMDSETDATA                hdrRingItemsize;
        regVGT_ESGS_RING_ITEMSIZE    esGsRingItemsize;
        regVGT_GSVS_RING_ITEMSIZE    gsVsRingItemsize;

        PM4CMDSETDATA                hdrRingOffset;
        regVGT_GSVS_RING_OFFSET_1    ringOffset1;
        regVGT_GSVS_RING_OFFSET_2    ringOffset2;
        regVGT_GSVS_RING_OFFSET_3    ringOffset3;

        // Note: The following PM4 packets are only needed on GFX7 and newer hardware.  They should be the last register
        // values in the structure.
        PM4CMDSETDATA                 hdrGsOnchipCnt;
        regVGT_GS_ONCHIP_CNTL__CI__VI vgtGsOnchipCntl;

        // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
        // actual commands contained within.
        size_t                        spaceNeeded;
    };

    const Device&     m_device;
    Pm4ImageSh        m_pm4ImageSh;        // ES/GS sh commands to be written when the associated pipeline is bound.
    Pm4ImageShDynamic m_pm4ImageShDynamic; // ES/GS sh commands to be calculated and written when the associated pipeline
                                           // is bound.
    Pm4ImageContext   m_pm4ImageContext;   // ES/GS context commands to be written when the associated pipeline is bound.

    const PerfDataInfo* m_pEsPerfDataInfo; // ES performance data information.
    const PerfDataInfo* m_pGsPerfDataInfo; // GS performance data information.

    ShaderStageInfo   m_stageInfoEs;
    ShaderStageInfo   m_stageInfoGs;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkEsGs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkEsGs);
};

} // Gfx6
} // Pal
