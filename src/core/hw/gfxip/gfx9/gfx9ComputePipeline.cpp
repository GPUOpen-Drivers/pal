/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/platform.h"
#include "core/hw/gfxip/gfx9/gfx9AbiToPipelineRegisters.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9ComputePipeline.h"
#include "core/hw/gfxip/gfx9/gfx9ComputeShaderLibrary.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/imported/hsa/AMDHSAKernelDescriptor.h"
#include "core/imported/hsa/amd_hsa_kernel_code.h"
#include "palFile.h"
#include "palHsaAbiMetadata.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
ComputePipeline::ComputePipeline(
    Device* pDevice,
    bool    isInternal)  // True if this is a PAL-owned pipeline (i.e., an RPM pipeline).
    :
    Pal::ComputePipeline(pDevice->Parent(), isInternal),
    m_pDevice(pDevice),
    m_signature{pDevice->GetNullCsSignature()},
    m_ringSizeComputeScratch{},
    m_chunkCs(*pDevice,
              &m_stageInfo,
              &m_perfDataInfo[static_cast<uint32>(Abi::HardwareStage::Cs)]),
    m_disablePartialPreempt(false),
    m_shPairsPacketSupportedCs(pDevice->Settings().gfx11EnableShRegPairOptimizationCs)
{
}

// =====================================================================================================================
// Initializes HW-specific state related to this compute pipeline (register values, user-data mapping, etc.) using the
// specified Pipeline ABI processor.
Result ComputePipeline::HwlInit(
    const ComputePipelineCreateInfo&  createInfo,
    const AbiReader&                  abiReader,
    const HsaAbi::CodeObjectMetadata& metadata,
    MsgPackReader*                    pMetadataReader)
{
    Result result = Result::Success;

    m_disablePartialPreempt = createInfo.disablePartialDispatchPreemption;

    // The metadata guarantees that the required size components are all zero or all non-zero.
    const bool hasRequiredSize = (metadata.RequiredWorkgroupSizeX() != 0);

    // Pick a thread group size for this pipeline. It may come from the create info or from the HSA metadata.
    Extent3d groupSize;

    // These always have to be all non-zero or all zero.
    PAL_ASSERT(((createInfo.threadsPerGroup.width  == 0) &&
                (createInfo.threadsPerGroup.height == 0) &&
                (createInfo.threadsPerGroup.depth  == 0)) ||
               ((createInfo.threadsPerGroup.width  != 0) &&
                (createInfo.threadsPerGroup.height != 0) &&
                (createInfo.threadsPerGroup.depth  != 0)));

    // The caller can pick the thread group size if the ELF wasn't compiled against a particular size.
    if (createInfo.threadsPerGroup.width != 0)
    {
        groupSize = createInfo.threadsPerGroup;

        if (hasRequiredSize &&
            ((groupSize.width  != metadata.RequiredWorkgroupSizeX()) ||
             (groupSize.height != metadata.RequiredWorkgroupSizeY()) ||
             (groupSize.depth  != metadata.RequiredWorkgroupSizeZ())))
        {
            // This ELF requires a specific thread group size which cannot be changed.
            result = Result::ErrorInvalidValue;
        }
    }
    else if (hasRequiredSize)
    {
        groupSize.width  = metadata.RequiredWorkgroupSizeX();
        groupSize.height = metadata.RequiredWorkgroupSizeY();
        groupSize.depth  = metadata.RequiredWorkgroupSizeZ();
    }
    else
    {
        // We could fail here since we don't really know what group size to use. Instead, let's assume we're
        // supposed to launch a 1D thread group of the maximum supported size. We may change this in the future.
        groupSize.width  = metadata.MaxFlatWorkgroupSize();
        groupSize.height = 1;
        groupSize.depth  = 1;
    }

    if (result == Result::Success)
    {
        // The X/Y/Z sizes must be non-zero and cover a volume no greater than the max flat group size.
        const uint32 flatSize = groupSize.width * groupSize.height * groupSize.depth;

        if ((flatSize == 0) || (flatSize > metadata.MaxFlatWorkgroupSize()))
        {
            result = Result::ErrorInvalidValue;
        }
    }

    const llvm::amdhsa::kernel_descriptor_t& desc = KernelDescriptor();

    regCOMPUTE_PGM_RSRC2 computePgmRsrc2 = {};
    computePgmRsrc2.u32All = desc.compute_pgm_rsrc2;
    computePgmRsrc2.bitfields.LDS_SIZE =
        RoundUpQuotient(NumBytesToNumDwords(desc.group_segment_fixed_size), Gfx9LdsDwGranularity);

    if (metadata.UsesDynamicStack())
    {
        PAL_ASSERT_ALWAYS_MSG("Dynamic stack is unsupported!");
        result = Result::Unsupported;
    }
    if (metadata.UniformWorkgroupSize() != 1)
    {
        PAL_ASSERT_ALWAYS_MSG("non-uniform workgroups are unsupported!");
        result = Result::Unsupported;
    }
    if (result == Result::Success)
    {
        // These features aren't yet implemented in PAL's HSA ABI path.
        // - The queue pointer, dispatch ID, flat scratch, or private segment SGPRs.
        // - Any kind of scratch memory or register spilling.
        // - Dynamic threadgroup sizes.
        // - Init or Fini kernels.
        if (TestAnyFlagSet(desc.kernel_code_properties,
                           AMD_KERNEL_CODE_PROPERTIES_ENABLE_SGPR_QUEUE_PTR         |
                           AMD_KERNEL_CODE_PROPERTIES_ENABLE_SGPR_FLAT_SCRATCH_INIT |
                           AMD_KERNEL_CODE_PROPERTIES_ENABLE_SGPR_PRIVATE_SEGMENT_SIZE) ||
            (metadata.KernelKind() != HsaAbi::Kind::Normal))
        {
            PAL_ASSERT_ALWAYS_MSG("Unsupported scratch memory usage mode");
            result = Result::Unsupported;
        }
        else
        {
            if (computePgmRsrc2.bits.SCRATCH_EN != 0)
            {
                //Navi3+ can support scratch usage due to different scrach allocation system from previous hardware
                if (IsGfx11Plus(*m_pDevice->Parent()) == false)
                {
                    PAL_ASSERT_ALWAYS_MSG("Scratch cannot be used on this device");
                    result = Result::Unsupported;
                }
            }

            // We only have partial support for the Hidden arguments. We support:
            // - HiddenNone: Which we just need to allocate and ignore.
            // - HiddenGlobalOffsetX/Y/Z: Which are the global thread starting offsets (i.e. CmdDispatchOffset).
            // We permit HiddenMultigridSyncArg but don't actually support it. Normal kernels declare it but never use
            // it. We don't have any required metadata that lets us distinguish between cases that don't use it and
            // cases that do use it. We just have to tell users to not use it or they'll get a GPU page fault. The
            // remaining hidden arguments are not supported and rejected right here.
            for (uint32 idx = 0; idx < metadata.NumArguments(); ++idx)
            {
                const HsaAbi::KernelArgument& arg = metadata.Arguments()[idx];

                if ((arg.valueKind == HsaAbi::ValueKind::HiddenPrintfBuffer)   ||
                    (arg.valueKind == HsaAbi::ValueKind::HiddenHostcallBuffer) ||
                    (arg.valueKind == HsaAbi::ValueKind::HiddenDefaultQueue)   ||
                    (arg.valueKind == HsaAbi::ValueKind::HiddenCompletionAction))
                {
                    result = Result::Unsupported;
                    break;
                }
            }
        }
    }

    PipelineUploader uploader(m_pDevice->Parent(), abiReader);
    RegisterVector   registers(m_pDevice->GetPlatform());

    if (result == Result::Success)
    {
        result = registers.Insert(mmCOMPUTE_PGM_RSRC1, desc.compute_pgm_rsrc1);
    }

    if (result == Result::Success)
    {
        result = registers.Insert(mmCOMPUTE_PGM_RSRC2, computePgmRsrc2.u32All);
    }

    if (result == Result::Success)
    {
        result = registers.Insert(mmCOMPUTE_PGM_RSRC3, desc.compute_pgm_rsrc3);
    }

    if (result == Result::Success)
    {
        regCOMPUTE_NUM_THREAD_X computeNumThreadX = {};
        computeNumThreadX.bits.NUM_THREAD_FULL = groupSize.width;

        result = registers.Insert(mmCOMPUTE_NUM_THREAD_X, computeNumThreadX.u32All);
    }

    if (result == Result::Success)
    {
        regCOMPUTE_NUM_THREAD_Y computeNumThreadY = {};
        computeNumThreadY.bits.NUM_THREAD_FULL = groupSize.height;

        result = registers.Insert(mmCOMPUTE_NUM_THREAD_Y, computeNumThreadY.u32All);
    }

    if (result == Result::Success)
    {
        regCOMPUTE_NUM_THREAD_Z computeNumThreadZ = {};
        computeNumThreadZ.bits.NUM_THREAD_FULL = groupSize.depth;

        result = registers.Insert(mmCOMPUTE_NUM_THREAD_Z, computeNumThreadZ.u32All);
    }

    if (result == Result::Success)
    {
        // Next, handle relocations and upload the pipeline code & data to GPU memory.
        const PalPublicSettings& settings = *m_pDevice->Parent()->GetPublicSettings();
        const GpuHeap            heap     = IsInternal() ? GpuHeapLocal : settings.pipelinePreferredHeap;

        result = PerformRelocationsAndUploadToGpuMemory(0u, heap, &uploader);
    }

    if (result == Result::Success)
    {
        // Update the pipeline signature with user-mapping data contained in the ELF:
        m_chunkCs.SetupSignatureFromElf(&m_signature, metadata, registers);

        const uint32 scratchMemorySize = CalcScratchMemSize(m_pDevice->Parent()->ChipProperties().gfxLevel, metadata);

        if (scratchMemorySize != 0)
        {
            UpdateRingSizeComputeScratch(scratchMemorySize);
        }

        const uint32 wavefrontSize = IsWave32() ? 32 : 64;

        m_chunkCs.LateInit(registers,
                           wavefrontSize,
                           &m_threadsPerTg,
                           createInfo.interleaveSize,
                           &uploader);
        PAL_ASSERT(m_uploadFenceToken == 0);
        result = uploader.End(&m_uploadFenceToken);
    }

    return result;
}

// =====================================================================================================================
// Initializes HW-specific state related to this compute pipeline (register values, user-data mapping, etc.) using the
// specified Pipeline ABI processor.
Result ComputePipeline::HwlInit(
    const ComputePipelineCreateInfo&  createInfo,
    const AbiReader&                  abiReader,
    const PalAbi::CodeObjectMetadata& metadata,
    MsgPackReader*                    pMetadataReader)
{
    m_disablePartialPreempt = createInfo.disablePartialDispatchPreemption;

    PipelineUploader uploader(m_pDevice->Parent(), abiReader);

    // Next, handle relocations and upload the pipeline code & data to GPU memory.
    const PalPublicSettings& settings = *m_pDevice->Parent()->GetPublicSettings();
    const GpuHeap            heap     = IsInternal() ? GpuHeapLocal : settings.pipelinePreferredHeap;

    Result result = PerformRelocationsAndUploadToGpuMemory(0u, heap, &uploader);

    if (result ==  Result::Success)
    {
        // Update the pipeline signature with user-mapping data contained in the ELF:
        m_chunkCs.SetupSignatureFromElf(&m_signature, metadata);

        const uint32 scratchMemorySize = CalcScratchMemSize(m_pDevice->Parent()->ChipProperties().gfxLevel, metadata);

        if (scratchMemorySize != 0)
        {
            UpdateRingSizeComputeScratch(scratchMemorySize);
        }

        const uint32 wavefrontSize = IsWave32() ? 32 : 64;

        m_chunkCs.LateInit(metadata,
                           wavefrontSize,
                           &m_threadsPerTg,
                           createInfo.interleaveSize,
                           &uploader);
        PAL_ASSERT(m_uploadFenceToken == 0);
        result = uploader.End(&m_uploadFenceToken);
    }

    return result;
}

// =====================================================================================================================
// Helper function to compute the WAVES_PER_SH field of the COMPUTE_RESOURCE_LIMITS register.
uint32 ComputePipeline::CalcMaxWavesPerSe(
    const GpuChipProperties& chipProps,
    float                    maxWavesPerCu)
{
    // The maximum number of waves per SH in "register units".
    // By default set the WAVE_LIMIT field to be unlimited.
    // Limits given by the ELF will only apply if the caller doesn't set their own limit.
    uint32 wavesPerSe = 0;

    if (maxWavesPerCu > 0)
    {
        wavesPerSe = CalcMaxWavesPerSh(chipProps, maxWavesPerCu) * chipProps.gfx9.numShaderArrays;
    }

    return wavesPerSe;
}

// =====================================================================================================================
uint32 ComputePipeline::CalcMaxWavesPerSh(
    const GpuChipProperties& chipProps,
    float                    maxWavesPerCu)
{
    // The maximum number of waves per SH in "register units".
    // By default set the WAVE_LIMIT field to be unlimited.
    // Limits given by the ELF will only apply if the caller doesn't set their own limit.
    uint32 wavesPerSh = 0;

    if (maxWavesPerCu > 0)
    {
        const uint32 maxWavesPerShCompute = Device::GetMaxWavesPerSh(chipProps, true);

        // We assume no one is trying to use more than 100% of all waves.
        PAL_ASSERT(maxWavesPerCu <= (maxWavesPerShCompute / chipProps.gfx9.maxNumCuPerSh));

        const uint32 maxWavesPerSh = static_cast<uint32>(round(maxWavesPerCu * chipProps.gfx9.numCuPerSh));

        // For compute shaders, it is in units of 1 wave and must not exceed the max.
        wavesPerSh = Min(maxWavesPerShCompute, maxWavesPerSh);
    }

    return wavesPerSh;
}

// =====================================================================================================================
// If pipeline may make indirect function calls, perform any late linking steps required to valid execution
// of the possible function calls.
// (this could include adjusting hardware resources such as GPRs or LDS space for the pipeline).
// This function should be called by clients prior to CmdDispatch.
Result ComputePipeline::LinkWithLibraries(
    const IShaderLibrary*const* ppLibraryList,
    uint32                      libraryCount)
{
    Result result = Result::Success;
    const auto&  gpuInfo       = m_pDevice->Parent()->ChipProperties();

    // When linking this pipeline with any shader function library,
    // the compute resource registers we write into the ELF binary must
    // account for the worst-case of any hardware resource used by either the main shader,
    // or any of the function library.
    const HwRegInfo& mainCsRegInfo = m_chunkCs.HwInfo();

    const bool isWave32 = IsWave32();

    regCOMPUTE_PGM_RSRC1 computePgmRsrc1 = mainCsRegInfo.computePgmRsrc1;
    regCOMPUTE_PGM_RSRC2 computePgmRsrc2 = mainCsRegInfo.dynamic.computePgmRsrc2;
    regCOMPUTE_PGM_RSRC3 computePgmRsrc3 = mainCsRegInfo.computePgmRsrc3;

    for (uint32 idx = 0; idx < libraryCount; idx++)
    {
        const auto*const pLibObj = static_cast<const Pal::Gfx9::ComputeShaderLibrary*const>(ppLibraryList[idx]);
        // In case this shaderLibrary did not use internal dma queue to upload ELF, the UploadFenceToken
        // of the shaderLibrary is 0.
        m_uploadFenceToken = Max(m_uploadFenceToken, pLibObj->GetUploadFenceToken());
        m_pagingFenceVal   = Max(m_pagingFenceVal,   pLibObj->GetPagingFenceVal());

        if (pLibObj->GetShaderLibFunctionInfos().NumElements() == 0)
        {
            // Skip library with no functions for propagation of register usage etc.
            continue;
        }

        const LibraryHwInfo& libObjRegInfo = pLibObj->HwInfo();

        PAL_ASSERT(pLibObj->IsWave32() == isWave32);
        if (pLibObj->IsWave32() != isWave32)
        {
            // If the main pipeline and the shader library has a different wavefront size,
            // LinkWithLibraries should fail.
            result = Result::ErrorIncompatibleLibrary;
            break;
        }

        ShaderLibStats libStats = {};
        result = pLibObj->GetAggregateFunctionStats(&libStats);
        if (result != Result::Success)
        {
            break;
        }

        const uint32 libLdsWords  = static_cast<uint32>(libStats.common.ldsUsageSizeInBytes >> 2);
        const uint32 libLds       = RoundUpQuotient(libLdsWords, Gfx9LdsDwGranularity);
        const uint32 libSgprs     = AbiRegisters::CalcNumSgprs(libStats.common.numUsedSgprs);
        const uint32 libVgprs     = AbiRegisters::CalcNumVgprs(libStats.common.numUsedVgprs, IsWave32());

        computePgmRsrc1.bits.SGPRS =
            Max(computePgmRsrc1.bits.SGPRS, libSgprs, libObjRegInfo.libRegs.computePgmRsrc1.bits.SGPRS);
        computePgmRsrc1.bits.VGPRS =
            Max(computePgmRsrc1.bits.VGPRS, libVgprs, libObjRegInfo.libRegs.computePgmRsrc1.bits.VGPRS);

        computePgmRsrc2.bits.USER_SGPR =
            Max(computePgmRsrc2.bits.USER_SGPR, libObjRegInfo.libRegs.computePgmRsrc2.bits.USER_SGPR);
        computePgmRsrc2.bits.LDS_SIZE =
            Max(computePgmRsrc2.bits.LDS_SIZE, libLds, libObjRegInfo.libRegs.computePgmRsrc2.bits.LDS_SIZE);
        computePgmRsrc2.bits.TIDIG_COMP_CNT =
            Max(computePgmRsrc2.bits.TIDIG_COMP_CNT, libObjRegInfo.libRegs.computePgmRsrc2.bits.TIDIG_COMP_CNT);
        computePgmRsrc2.bits.SCRATCH_EN |= libObjRegInfo.libRegs.computePgmRsrc2.bits.SCRATCH_EN;
        computePgmRsrc2.bits.TGID_X_EN  |= libObjRegInfo.libRegs.computePgmRsrc2.bits.TGID_X_EN;
        computePgmRsrc2.bits.TGID_Y_EN  |= libObjRegInfo.libRegs.computePgmRsrc2.bits.TGID_Y_EN;
        computePgmRsrc2.bits.TGID_Z_EN  |= libObjRegInfo.libRegs.computePgmRsrc2.bits.TGID_Z_EN;
        computePgmRsrc2.bits.TG_SIZE_EN |= libObjRegInfo.libRegs.computePgmRsrc2.bits.TG_SIZE_EN;

        // FWD_PROGRESS and WGP_MODE should match across all the shader functions and the main shader.
        PAL_ALERT_MSG((computePgmRsrc1.bits.FWD_PROGRESS != libObjRegInfo.libRegs.computePgmRsrc1.bits.FWD_PROGRESS),
                      "Running non-FWD_PROGRESS work in FWD_PROGRESS pipeline is supported but suboptimal");
        PAL_ALERT_MSG((computePgmRsrc1.bits.WGP_MODE != libObjRegInfo.libRegs.computePgmRsrc1.bits.WGP_MODE),
                      "Running non-WGP_MODE work in WGP_MODE pipeline is supported but suboptimal");

        computePgmRsrc1.bits.MEM_ORDERED  |= libObjRegInfo.libRegs.computePgmRsrc1.bits.MEM_ORDERED;
        computePgmRsrc1.bits.FWD_PROGRESS |= libObjRegInfo.libRegs.computePgmRsrc1.bits.FWD_PROGRESS;
        computePgmRsrc1.bits.WGP_MODE     |= libObjRegInfo.libRegs.computePgmRsrc1.bits.WGP_MODE;

        computePgmRsrc3.bits.SHARED_VGPR_CNT =
            Max(computePgmRsrc3.bits.SHARED_VGPR_CNT, libObjRegInfo.libRegs.computePgmRsrc3.bits.SHARED_VGPR_CNT);

        const uint32 stackSizeNeededInBytes = pLibObj->GetMaxStackSizeInBytes() * m_maxFunctionCallDepth;

        if (stackSizeNeededInBytes > m_stackSizeInBytes)
        {
            m_stackSizeInBytes = stackSizeNeededInBytes;

            UpdateRingSizeComputeScratch(m_stackSizeInBytes / sizeof(uint32));
        }
    }

    // Update m_chunCs with updated register values
    m_chunkCs.UpdateComputePgmRsrsAfterLibraryLink(computePgmRsrc1, computePgmRsrc2, computePgmRsrc3);

    return result;
}

// =====================================================================================================================
// Writes the PM4 commands required to bind this pipeline. Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* ComputePipeline::WriteCommands(
    CmdStream*                      pCmdStream,
    uint32*                         pCmdSpace,
    const DynamicComputeShaderInfo& csInfo,
    bool                            prefetch
    ) const
{
    pCmdSpace =  m_chunkCs.WriteShCommands(pCmdStream,
                                           pCmdSpace,
                                           m_shPairsPacketSupportedCs,
                                           csInfo,
                                           prefetch);

    return pCmdSpace;
}

// =====================================================================================================================
// Obtains shader compilation stats.
Result ComputePipeline::GetShaderStats(
    ShaderType   shaderType,
    ShaderStats* pShaderStats,
    bool         getDisassemblySize
    ) const
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    PAL_ASSERT(pShaderStats != nullptr);
    Result result = Result::ErrorUnavailable;

    if (shaderType == ShaderType::Compute)
    {
        result = GetShaderStatsForStage(shaderType, m_stageInfo, nullptr, pShaderStats);
        if (result == Result::Success)
        {
            pShaderStats->shaderStageMask              = ApiShaderStageCompute;
            pShaderStats->palShaderHash                = m_info.shader[static_cast<uint32>(shaderType)].hash;
            pShaderStats->cs.numThreadsPerGroup        = m_threadsPerTg;
            pShaderStats->common.gpuVirtAddress        = m_chunkCs.CsProgramGpuVa();
            pShaderStats->common.ldsSizePerThreadGroup = chipProps.gfxip.ldsSizePerThreadGroup;
        }
    }

    return result;
}

// =====================================================================================================================
// Set the stack frame size for indirect shaders in the pipeline
void ComputePipeline::SetStackSizeInBytes(
    uint32 stackSizeInBytes)
{
    m_stackSizeInBytes = stackSizeInBytes;
    UpdateRingSizeComputeScratch(stackSizeInBytes / sizeof(uint32));
}

// =====================================================================================================================
uint32 ComputePipeline::CalcScratchMemSize(
    GfxIpLevel                        gfxIpLevel,
    const PalAbi::CodeObjectMetadata& metadata)
{
    uint32 scratchMemorySize = 0;

    const auto& csStageMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Cs)];
    if (csStageMetadata.hasEntry.scratchMemorySize != 0)
    {
        scratchMemorySize = csStageMetadata.scratchMemorySize;
    }

    // If there is no metadata entry for wavefront size, we assume it is Wave64.
    if ((csStageMetadata.hasEntry.wavefrontSize == 0) || (csStageMetadata.wavefrontSize == 64))
    {
        // We allocate scratch memory based on the minimum wave size for the chip, which for Gfx10+ ASICs will
        // be Wave32. In order to appropriately size the scratch memory (reported in the ELF as per-thread) for
        // a Wave64, we need to multiply by 2.
        scratchMemorySize *= 2;
    }

    return scratchMemorySize / sizeof(uint32);
}
// =====================================================================================================================
uint32 ComputePipeline::CalcScratchMemSize(
    GfxIpLevel                        gfxIpLevel,
    const HsaAbi::CodeObjectMetadata& metadata)
{
    uint32 scratchMemorySize = metadata.PrivateSegmentFixedSize();

    if (metadata.WavefrontSize() == 64)
    {
        // We allocate scratch memory based on the minimum wave size for the chip, which for Gfx10+ ASICs will
        // be Wave32. In order to appropriately size the scratch memory (reported in the ELF as per-thread) for
        // a Wave64, we need to multiply by 2.
        scratchMemorySize *= 2;
    }

    return Pow2Align<uint32>((scratchMemorySize / sizeof(uint32)), sizeof(uint32));
}

// =====================================================================================================================
// Update the device that this compute pipeline has some new ring-size requirements.
void ComputePipeline::UpdateRingSizeComputeScratch(
    uint32 scratchMemorySize)
{
    PAL_ASSERT(scratchMemorySize != 0);

    m_ringSizeComputeScratch = scratchMemorySize;
}

} // Gfx9
} // Pal
