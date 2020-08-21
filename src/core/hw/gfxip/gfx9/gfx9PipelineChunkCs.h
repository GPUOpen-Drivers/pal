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
struct GraphicsPipelineLoadInfo;

// =====================================================================================================================
// Describe HW Reg for a Computer Pipeline
struct HwRegInfo
{
    regCOMPUTE_NUM_THREAD_X  computeNumThreadX;
    regCOMPUTE_NUM_THREAD_Y  computeNumThreadY;
    regCOMPUTE_NUM_THREAD_Z  computeNumThreadZ;
    regCOMPUTE_PGM_LO        computePgmLo;
    regCOMPUTE_PGM_HI        computePgmHi;
    regCOMPUTE_PGM_RSRC1     computePgmRsrc1;
    regCOMPUTE_PGM_RSRC3     computePgmRsrc3;
    regCOMPUTE_USER_DATA_0   userDataInternalTable;
    regCOMPUTE_SHADER_CHKSUM computeShaderChksum;

    struct
    {
        regCOMPUTE_PGM_RSRC2        computePgmRsrc2;
        regCOMPUTE_RESOURCE_LIMITS  computeResourceLimits;
    } dynamic; // Contains state which depends on bind-time parameters.
};

// =====================================================================================================================
// Describe HW Reg for a Shader Library
// Shader Library only need HW regs includes: computePgmRsrc1 / computePgmRsrc2 / computePgmRsrc3
// The PGM_LO/PGM_HI are for programming the main shader address to the HW,
// and the USER_ACCUM registers are specific to the main shader also.
struct LibHwRegInfo
{
    regCOMPUTE_PGM_RSRC1     computePgmRsrc1;
    regCOMPUTE_PGM_RSRC3     computePgmRsrc3;

    struct
    {
        regCOMPUTE_PGM_RSRC2        computePgmRsrc2;
        regCOMPUTE_RESOURCE_LIMITS  computeResourceLimits;
    } dynamic; // Contains state which depends on bind-time parameters.
};

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
        const AbiReader&                 abiReader,
        const RegisterVector&            registers,
        uint32                           wavefrontSize,
        ComputePipelineIndirectFuncInfo* pIndirectFuncList,
        uint32                           indirectFuncCount,
        uint32*                          pThreadsPerTgX,
        uint32*                          pThreadsPerTgY,
        uint32*                          pThreadsPerTgZ,
        bool                             forceDisableLoadPath,
        CsPipelineUploader*              pUploader);

    uint32* WriteShCommands(
        CmdStream*                      pCmdStream,
        uint32*                         pCmdSpace,
        const DynamicComputeShaderInfo& csInfo,
        bool                            prefetch) const;

    gpusize CsProgramGpuVa() const
        { return GetOriginalAddress(m_regs.computePgmLo.bits.DATA, m_regs.computePgmHi.bits.DATA); }

    const HwRegInfo HWInfo() const { return m_regs; }

    void UpdateComputePgmRsrsAfterLibraryLink(
        regCOMPUTE_PGM_RSRC1 Rsrc1,
        regCOMPUTE_PGM_RSRC2 Rsrc2,
        regCOMPUTE_PGM_RSRC3 Rsrc3);

private:
    uint32* WriteShCommandsSetPath(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    const Device&  m_device;

    HwRegInfo m_regs;

    struct
    {
        gpusize  gpuVirtAddr;
        uint32   count;
    }  m_loadPath;

    PipelinePrefetchPm4  m_prefetch;

    PerfDataInfo*const m_pCsPerfDataInfo;   // CS performance data information.
    ShaderStageInfo*   m_pStageInfo;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkCs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkCs);
};

// =====================================================================================================================
// Represents the chunk of a compute library object which contains all of the registers
// which setup the hardware library stage.
// This is sort of a PM4 "image" of the commands which write these registers, but with some intelligence so that the
// code used to setup the commands can be reused.
class LibraryChunkCs : PipelineChunkCs
{
public:
    explicit LibraryChunkCs(const Device& device);

    ~LibraryChunkCs() { }

    // Compute Library use only
    template <typename ShaderLibraryUploader>
    void LateInit(
        const AbiReader&                 abiReader,
        const RegisterVector&            registers,
        uint32                           wavefrontSize,
        ShaderLibraryFunctionInfo*       pFunctionList,
        uint32                           funcCount,
        ShaderLibraryUploader*           pUploader);

    const LibHwRegInfo LibHWInfo() const { return m_regs; }

private:
    const Device&  m_device;

    LibHwRegInfo m_regs;

    PAL_DISALLOW_DEFAULT_CTOR(LibraryChunkCs);
    PAL_DISALLOW_COPY_AND_ASSIGN(LibraryChunkCs);
};

} // Gfx9
} // Pal
