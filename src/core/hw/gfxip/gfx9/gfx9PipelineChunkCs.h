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
struct GraphicsPipelineLoadInfo;

// =====================================================================================================================
// Represents the chunk of a pipeline object which contains all of the registers which setup the hardware CS stage.
// This is sort of a PM4 "image" of the commands which write these registers, but with some intelligence so that the
// code used to setup the commands can be reused.
class PipelineChunkCs
{
public:
    PipelineChunkCs(
        const Device&    device,
        ShaderStageInfo* pStageInfo,
        PerfDataInfo*    pPerfDataInfo);
    ~PipelineChunkCs() { }

    // Compute-only.
    uint32 EarlyInit();

    // Graphics-only.
    void EarlyInit(GraphicsPipelineLoadInfo* pInfo);

    void SetupSignatureFromElf(
        ComputeShaderSignature*   pSignature,
        const CodeObjectMetadata& metadata,
        const RegisterVector&     registers);

    template <typename CsPipelineUploader>
    void LateInit(
        const AbiProcessor&              abiProcessor,
        const RegisterVector&            registers,
        uint32                           wavefrontSize,
        ComputePipelineIndirectFuncInfo* pIndirectFuncList,
        uint32                           indirectFuncCount,
        uint32*                          pThreadsPerTgX,
        uint32*                          pThreadsPerTgY,
        uint32*                          pThreadsPerTgZ,
        CsPipelineUploader*              pUploader);

    uint32* WriteShCommands(
        CmdStream*                      pCmdStream,
        uint32*                         pCmdSpace,
        const DynamicComputeShaderInfo& csInfo,
        bool                            prefetch) const;

    gpusize CsProgramGpuVa() const
    {
        return GetOriginalAddress(m_commands.set.computePgmLo.bits.DATA,
                                  m_commands.set.computePgmHi.bits.DATA);
    }

private:
    template <typename CsPipelineUploader>
    void BuildPm4Headers(const CsPipelineUploader& uploader);

    // Pre-assembled "images" of the PM4 packets used for binding this pipeline to a command buffer.
    struct Pm4Commands
    {
        struct
        {
            PM4_ME_LOAD_SH_REG_INDEX  loadShRegIndex;
        } loadIndex; // LOAD_INDEX path, used for universal command buffers.

        struct
        {
            PM4_ME_SET_SH_REG  hdrComputePgm;
            regCOMPUTE_PGM_LO  computePgmLo;
            regCOMPUTE_PGM_HI  computePgmHi;

            PM4_ME_SET_SH_REG       hdrComputeUserData;
            regCOMPUTE_USER_DATA_0  computeUserDataLo;

            PM4_ME_SET_SH_REG     hdrComputePgmRsrc1;
            regCOMPUTE_PGM_RSRC1  computePgmRsrc1;

            PM4_ME_SET_SH_REG        hdrComputeNumThread;
            regCOMPUTE_NUM_THREAD_X  computeNumThreadX;
            regCOMPUTE_NUM_THREAD_Y  computeNumThreadY;
            regCOMPUTE_NUM_THREAD_Z  computeNumThreadZ;

            // Checksum register is optional, as not all GFX9+ hardware uses it. If we don't use it, NOP will be added.
            PM4_ME_SET_SH_REG         hdrComputeShaderChksum;
            regCOMPUTE_SHADER_CHKSUM  computeShaderChksum;

            // All GFX10 devices support the checksum register. If we don't have it, NOP will be added.
            PM4_ME_SET_SH_REG        hdrComputePgmRsrc3;
            regCOMPUTE_PGM_RSRC3     computePgmRsrc3;

            // Not all gfx10 devices support user accum registers. If we don't have it, NOP will be added.
            PM4_ME_SET_SH_REG        hdrComputeUserAccum;
            regCOMPUTE_USER_ACCUM_0  regComputeUserAccum0;
            regCOMPUTE_USER_ACCUM_1  regComputeUserAccum1;
            regCOMPUTE_USER_ACCUM_2  regComputeUserAccum2;
            regCOMPUTE_USER_ACCUM_3  regComputeUserAccum3;

            // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
            // w/ the actual commands contained above.
            size_t  spaceNeeded;
        } set; // SET path, used for compute command buffers.  The MEC doesn't support LOAD_SH_REG_INDEX.

        struct
        {
            PM4_ME_SET_SH_REG     hdrComputePgmRsrc2;
            regCOMPUTE_PGM_RSRC2  computePgmRsrc2;

            PM4_ME_SET_SH_REG           hdrComputeResourceLimits;
            regCOMPUTE_RESOURCE_LIMITS  computeResourceLimits;
        } dynamic; // Contains state which depends on bind-time parameters.

        PipelinePrefetchPm4 prefetch;
    };

    const Device&  m_device;
    Pm4Commands    m_commands;

    PerfDataInfo*const m_pCsPerfDataInfo;   // CS performance data information.

    ShaderStageInfo*  m_pStageInfo;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkCs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkCs);
};

} // Gfx9
} // Pal
