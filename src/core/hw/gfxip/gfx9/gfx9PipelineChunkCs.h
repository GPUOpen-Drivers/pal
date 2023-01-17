/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
// Describe HW Reg for a Computer Pipeline or Library.
struct HwRegInfo
{
    regCOMPUTE_NUM_THREAD_X        computeNumThreadX;
    regCOMPUTE_NUM_THREAD_Y        computeNumThreadY;
    regCOMPUTE_NUM_THREAD_Z        computeNumThreadZ;
    regCOMPUTE_PGM_LO              computePgmLo;
    regCOMPUTE_PGM_RSRC1           computePgmRsrc1;
    regCOMPUTE_PGM_RSRC3           computePgmRsrc3;
    regCOMPUTE_USER_DATA_0         userDataInternalTable;
    regCOMPUTE_SHADER_CHKSUM       computeShaderChksum;
#if PAL_BUILD_GFX11
    regCOMPUTE_DISPATCH_INTERLEAVE computeDispatchInterleave;
#endif

    struct Dynamic
    {
        regCOMPUTE_PGM_RSRC2        computePgmRsrc2;
        regCOMPUTE_RESOURCE_LIMITS  computeResourceLimits;
    } dynamic; // Contains state which depends on bind-time parameters.
};

constexpr uint32 NumHwRegInfoRegs = sizeof(HwRegInfo) / sizeof(uint32);
constexpr uint32 NumDynamicRegs   = sizeof(HwRegInfo::Dynamic) / sizeof(uint32);
constexpr uint32 NumShRegs        = NumHwRegInfoRegs - NumDynamicRegs;

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

    void SetupSignatureFromElf(
        ComputeShaderSignature*                 pSignature,
        const Util::PalAbi::CodeObjectMetadata& metadata);

    void SetupSignatureFromElf(
        ComputeShaderSignature*                 pSignature,
        const Util::HsaAbi::CodeObjectMetadata& metadata,
        const RegisterVector&                   registers);

    void LateInit(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        uint32                                  wavefrontSize,
        DispatchDims*                           pThreadsPerTg,
#if PAL_BUILD_GFX11
        DispatchInterleaveSize                  interleaveSize,
#endif
        PipelineUploader*                       pUploader);

    void LateInit(
        const RegisterVector&                   registers,
        uint32                                  wavefrontSize,
        DispatchDims*                           pThreadsPerTg,
#if PAL_BUILD_GFX11
        DispatchInterleaveSize                  interleaveSize,
#endif
        PipelineUploader*                       pUploader);

    uint32* UpdateDynamicRegInfo(
        CmdStream*                      pCmdStream,
        uint32*                         pCmdSpace,
        HwRegInfo::Dynamic*             pDynamicRegs,
        const DynamicComputeShaderInfo& csInfo,
        gpusize                         launchDescGpuVa) const;

#if PAL_BUILD_GFX11
    void AccumulateShCommandsDynamic(
        PackedRegisterPair* pRegPairs,
        uint32*             pNumRegs,
        HwRegInfo::Dynamic  dynamicRegs,
        gpusize             launchDescGpuVa) const;
    void AccumulateShCommandsSetPath(
        PackedRegisterPair* pRegPairs,
        uint32*             pNumRegs,
        bool                usingLaunchDesc) const;
#endif

    uint32* WriteShCommands(
        CmdStream*                      pCmdStream,
        uint32*                         pCmdSpace,
#if PAL_BUILD_GFX11
        bool                            regPairsSupported,
#endif
        const DynamicComputeShaderInfo& csInfo,
        gpusize                         launchDescGpuVa,
        bool                            prefetch) const;

    uint32* WriteShCommandsDynamic(
        CmdStream*         pCmdStream,
        uint32*            pCmdSpace,
        HwRegInfo::Dynamic dynamicRegs,
        gpusize            launchDescGpuVa) const;

    uint32* WriteShCommandsSetPath(
        CmdStream* pCmdStream,
        uint32*    pCmdSpace,
        bool       usingLauncDesc) const;

    gpusize CsProgramGpuVa() const
        { return GetOriginalAddress(m_regs.computePgmLo.bits.DATA, 0); }

    const HwRegInfo& HwInfo() const { return m_regs; }

    void UpdateComputePgmRsrsAfterLibraryLink(
        regCOMPUTE_PGM_RSRC1 Rsrc1,
        regCOMPUTE_PGM_RSRC2 Rsrc2,
        regCOMPUTE_PGM_RSRC3 Rsrc3);

    Result CreateLaunchDescriptor(void* pOut, bool resolve);

private:
    void InitRegisters(
        const Util::PalAbi::CodeObjectMetadata& metadata,
#if PAL_BUILD_GFX11
        DispatchInterleaveSize                  interleaveSize,
#endif
        uint32                                  wavefrontSize);

    void InitRegisters(
        const RegisterVector&                   registers,
#if PAL_BUILD_GFX11
        DispatchInterleaveSize                  interleaveSize,
#endif
        uint32                                  wavefrontSize);

    void DoLateInit(DispatchDims* pThreadsPerTg, PipelineUploader* pUploader);

    void SetupSignatureFromMetadata(
        ComputeShaderSignature*                 pSignature,
        const Util::PalAbi::CodeObjectMetadata& metadata);

    void SetupSignatureFromRegisters(
        ComputeShaderSignature* pSignature,
        const RegisterVector&   registers);

    const Device& m_device;

    HwRegInfo m_regs;
    gpusize   m_prefetchAddr;
    gpusize   m_prefetchSize;

    PerfDataInfo*const m_pCsPerfDataInfo; // CS performance data information.
    ShaderStageInfo*   m_pStageInfo;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkCs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkCs);
};

} // Gfx9
} // Pal
