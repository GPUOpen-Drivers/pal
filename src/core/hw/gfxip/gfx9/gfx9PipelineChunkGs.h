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
struct GsParams
{
    gpusize             codeGpuVirtAddr;
    gpusize             dataGpuVirtAddr;
    bool                usesOnChipGs;
    bool                isNgg;
    uint16              esGsLdsSizeRegGs;
    uint16              esGsLdsSizeRegVs;
    const PerfDataInfo* pGsPerfDataInfo;
    const PerfDataInfo* pCopyPerfDataInfo;
    Util::MetroHash64*  pHasher;
};

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
    explicit PipelineChunkGs(const Device& device);
    ~PipelineChunkGs() { }

    void Init(
        const AbiProcessor& abiProcessor,
        const GsParams&     params);

    uint32* WriteShCommands(
        CmdStream*              pCmdStream,
        uint32*                 pCmdSpace,
        const DynamicStageInfo& gsStageInfo,
        const DynamicStageInfo& vsStageInfo,
        bool                    isNgg) const;
    uint32* WriteContextCommands(
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const;

    uint32 EsGsRingItemSize() const { return m_pm4ImageContext.esGsRingItemSize.bits.ITEMSIZE; }
    uint32 GsVsRingItemSize() const { return m_pm4ImageContext.gsVsRingItemSize.bits.ITEMSIZE; }
    const regVGT_GS_ONCHIP_CNTL VgtGsOnchipCntl() const { return m_pm4ImageContext.vgtGsOnchipCntl; }

    gpusize EsProgramGpuVa() const
    {
        return GetOriginalAddress(m_pm4ImageSh.spiShaderPgmLoEs.bits.MEM_BASE,
                                  m_pm4ImageSh.spiShaderPgmHiEs.bits.MEM_BASE);
    }

    gpusize VsProgramGpuVa() const
    {
        return GetOriginalAddress(m_pm4ImageSh.spiShaderPgmLoVs.bits.MEM_BASE,
                                  m_pm4ImageSh.spiShaderPgmHiVs.bits.MEM_BASE);
    }

    const ShaderStageInfo& StageInfo() const { return m_stageInfo; }
    const ShaderStageInfo& StageInfoCopy() const { return m_stageInfoCopy; }

private:
    void BuildPm4Headers(
        bool   useOnchipGs,
        bool   isNgg,
        uint16 esGsLdsSizeRegAddrGs,
        uint16 esGsLdsSizeRegAddrVs);

    struct Pm4ImageSh
    {
        PM4_ME_SET_SH_REG                 hdrSpiShaderPgmEs;
        regSPI_SHADER_PGM_LO_ES           spiShaderPgmLoEs;
        regSPI_SHADER_PGM_HI_ES           spiShaderPgmHiEs;

        PM4_ME_SET_SH_REG                 hdrSpiShaderUserDataEs;
        regSPI_SHADER_USER_DATA_ES_1      spiShaderUserDataLoGs;

        PM4_ME_SET_SH_REG                 hdrSpiShaderPgmGs;
        regSPI_SHADER_PGM_RSRC1_GS        spiShaderPgmRsrc1Gs;
        SpiShaderPgmRsrc2Gs               spiShaderPgmRsrc2Gs;

        // Everything past this point is only necessary for legacy pipelines (non-NGG pipelines).
        PM4_ME_SET_SH_REG                 hdrSpiShaderPgmVs;
        regSPI_SHADER_PGM_LO_VS           spiShaderPgmLoVs;
        regSPI_SHADER_PGM_HI_VS           spiShaderPgmHiVs;
        regSPI_SHADER_PGM_RSRC1_VS        spiShaderPgmRsrc1Vs;   // copy-shader internal table address
        SpiShaderPgmRsrc2Vs               spiShaderPgmRsrc2Vs;   // copy-shader internal table address

        PM4_ME_SET_SH_REG                 hdrSpiShaderUserDataVs;
        regSPI_SHADER_USER_DATA_VS_1      spiShaderUserDataLoVs; // copy-shader internal constant buffer table address

        // The following are only necessary if the GsVs is onchip.
        PM4_ME_SET_SH_REG                 hdrEsGsSizeForVs;
        regSPI_SHADER_USER_DATA_VS_0      vsUserDataLdsEsGsSize;

        // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
        // actual commands contained within.
        size_t                            spaceNeeded;
    };

    // This is only for register writes determined during Pipeline Bind.
    struct Pm4ImageShDynamic
    {
        PM4_ME_SET_SH_REG_INDEX           hdrPgmRsrc3Gs;
        SpiShaderPgmRsrc3Gs               spiShaderPgmRsrc3Gs;

        PM4_ME_SET_SH_REG                 hdrPgmRsrc4Gs;
        SpiShaderPgmRsrc4Gs               spiShaderPgmRsrc4Gs;

        // Everything past this point is only necessary for legacy pipelines (non-NGG pipelines).
        PM4_ME_SET_SH_REG_INDEX           hdrPgmRsrc3Vs;
        regSPI_SHADER_PGM_RSRC3_VS        spiShaderPgmRsrc3Vs;

        // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
        // actual commands contained within.
        size_t spaceNeeded;
    };

    struct Pm4ImageGsLds
    {
        PM4_ME_SET_SH_REG                hdrEsGsSizeForGs;
        regSPI_SHADER_USER_DATA_ES_0     gsUserDataLdsEsGsSize;
        // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
        // actual commands contained within.
        size_t                           spaceNeeded;
    };

    struct Pm4ImageContext
    {
        PM4_PFP_SET_CONTEXT_REG          hdrVgtGsMaxVertOut;
        regVGT_GS_MAX_VERT_OUT           vgtGsMaxVertOut;

        PM4_PFP_SET_CONTEXT_REG          hdrVgtGsOutPrimType;
        regVGT_GS_OUT_PRIM_TYPE          vgtGsOutPrimType;

        PM4_PFP_SET_CONTEXT_REG          hdrVgtGsInstanceCnt;
        regVGT_GS_INSTANCE_CNT           vgtGsInstanceCnt;

        PM4_PFP_SET_CONTEXT_REG          hdrEsGsVsRingItemSize;
        regVGT_ESGS_RING_ITEMSIZE        esGsRingItemSize;
        regVGT_GSVS_RING_ITEMSIZE        gsVsRingItemSize;

        PM4_PFP_SET_CONTEXT_REG          hdrVgtGsVsRingOffset;
        regVGT_GSVS_RING_OFFSET_1        ringOffset1;
        regVGT_GSVS_RING_OFFSET_2        ringOffset2;
        regVGT_GSVS_RING_OFFSET_3        ringOffset3;

        PM4_PFP_SET_CONTEXT_REG          hdrVgtGsPerVs;
        regVGT_GS_PER_VS                 vgtGsPerVs;

        PM4_PFP_SET_CONTEXT_REG          hdrVgtGsVertItemSize;
        regVGT_GS_VERT_ITEMSIZE          vgtGsVertItemSize0;
        regVGT_GS_VERT_ITEMSIZE_1        vgtGsVertItemSize1;
        regVGT_GS_VERT_ITEMSIZE_2        vgtGsVertItemSize2;
        regVGT_GS_VERT_ITEMSIZE_3        vgtGsVertItemSize3;

        PM4_ME_SET_CONTEXT_REG           hdrVgtMaxPrimsPerSubgrp;
        union
        {
            uint32                                   u32All;
            regVGT_GS_MAX_PRIMS_PER_SUBGROUP__GFX09  gfx9;
        } maxPrimsPerSubgrp;

        PM4_PFP_SET_CONTEXT_REG          hdrVgtGsOnchipCntl;
        regVGT_GS_ONCHIP_CNTL            vgtGsOnchipCntl;

        PM4_PFP_SET_CONTEXT_REG          hdrSpiShaderPosFormat;
        regSPI_SHADER_POS_FORMAT         spiShaderPosFormat;

        PM4_PFP_SET_CONTEXT_REG          hdrPaClVsOutCntl;
        PaClVsOutCntl                    paClVsOutCntl;

        PM4_PFP_SET_CONTEXT_REG          hdrVgtPrimitiveIdEn;
        regVGT_PRIMITIVEID_EN            vgtPrimitiveIdEn;

        // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
        // actual commands contained within.
        size_t                           spaceNeeded;
    };

    const Device&  m_device;

    Pm4ImageSh        m_pm4ImageSh;        // GS sh commands to be written when the associated pipeline is bound.
    Pm4ImageShDynamic m_pm4ImageShDynamic; // GS sh commands to be calculated and written when the associated pipeline
                                           // is bound.
    Pm4ImageGsLds     m_pm4ImageGsLds;     // Commands related to the configuration of the ES/GS LDS space
    Pm4ImageContext   m_pm4ImageContext;   // ES/GS PM4 commands to be written when the associated pipeline is bound.

    const PerfDataInfo* m_pGsPerfDataInfo;   // GS performance data information.
    const PerfDataInfo* m_pCopyPerfDataInfo; // Copy shader performance data information.

    ShaderStageInfo   m_stageInfo;
    ShaderStageInfo   m_stageInfoCopy;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkGs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkGs);
};

} // Gfx9
} // Pal
