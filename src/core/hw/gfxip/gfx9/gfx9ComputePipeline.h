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

#include "core/hw/gfxip/computePipeline.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"

namespace Pal
{

class CmdStream;
class Platform;

namespace Gfx9
{

class Device;

// Represents an "image" of the PM4 commands necessary to write a GFX9 compute pipeline to hardware.  The required
// register writes are grouped into sets based on sequential register addresses, so that we can minimize the amount of
// PM4 space needed by setting several regs in each packet.
struct ComputePipelinePm4Img
{
    PM4_ME_SET_SH_REG          hdrComputeNumThread;
    regCOMPUTE_NUM_THREAD_X    computeNumThreadX;
    regCOMPUTE_NUM_THREAD_Y    computeNumThreadY;
    regCOMPUTE_NUM_THREAD_Z    computeNumThreadZ;

    PM4_ME_SET_SH_REG          hdrComputePgm;
    regCOMPUTE_PGM_LO          computePgmLo;
    regCOMPUTE_PGM_HI          computePgmHi;

    PM4_ME_SET_SH_REG          hdrComputePgmRsrc;
    regCOMPUTE_PGM_RSRC1       computePgmRsrc1;
    regCOMPUTE_PGM_RSRC2       computePgmRsrc2;

    PM4_ME_SET_SH_REG          hdrComputeUserData;
    regCOMPUTE_USER_DATA_0     computeUserDataLo;

    // Checksum register is optional, as not all GFX9+ hardware uses it.
    PM4_ME_SET_SH_REG          hdrComputeShaderChksum;
    regCOMPUTE_SHADER_CHKSUM   computeShaderChksum;

    // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the actual
    // commands contained within.
    size_t                     spaceNeeded;
};

// Represents an "image" of the PM4 commands used to dynamically set wave and CU enable limits.
struct ComputePipelinePm4ImgDynamic
{
    PM4_ME_SET_SH_REG          hdrComputeResourceLimits;
    regCOMPUTE_RESOURCE_LIMITS computeResourceLimits;

    // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
    // actual commands contained within.
    size_t                     spaceNeeded;
};

// =====================================================================================================================
// GFX9 compute pipeline class: implements GFX9 specific functionality for the ComputePipeline class.
class ComputePipeline : public Pal::ComputePipeline
{
public:
    ComputePipeline(Device* pDevice, bool isInternal);
    virtual ~ComputePipeline() { }

    uint32* WriteCommands(
        Pal::CmdStream*                 pCmdStream,
        uint32*                         pCmdSpace,
        const DynamicComputeShaderInfo& csInfo,
        const Pal::PrefetchMgr&         prefetchMgr) const;

    virtual Result GetShaderStats(
        ShaderType   shaderType,
        ShaderStats* pShaderStats,
        bool         getDissassemblySize) const override;

    const ComputePipelineSignature& Signature() const { return m_signature; }

protected:
    virtual Result HwlInit(const AbiProcessor& abiProcessor) override;

private:
    uint32 CalcMaxWavesPerSh(uint32 maxWavesPerCu) const;

    void BuildPm4Headers();
    void UpdateRingSizes(const AbiProcessor& abiProcessor);

    void SetupSignatureFromElf(
        const AbiProcessor& abiProcessor);

    Device*const  m_pDevice;

    ComputePipelinePm4Img        m_pm4Commands;
    ComputePipelinePm4ImgDynamic m_pm4CommandsDynamic;
    ComputePipelineSignature     m_signature;

    PAL_DISALLOW_DEFAULT_CTOR(ComputePipeline);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputePipeline);
};

} // Gfx9
} // Pal
