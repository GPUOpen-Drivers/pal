/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx12/gfx12CmdStream.h"
#include "core/hw/gfxip/gfx12/gfx12CmdUtil.h"
#include "core/hw/gfxip/gfx12/gfx12ComputePipeline.h"
#include "core/hw/gfxip/gfx12/gfx12ComputeShaderLibrary.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12UserDataLayout.h"
#include "core/imported/hsa/AMDHSAKernelDescriptor.h"
#include "core/imported/hsa/amd_hsa_kernel_code.h"
#include "palHsaAbiMetadata.h"

using namespace Util;
using namespace Pal::Gfx12::Chip;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
ComputePipeline::ComputePipeline(
    Device* pDevice,
    bool    isInternal)
    :
    Pal::ComputePipeline(pDevice->Parent(), isInternal),
    m_chunkCs(pDevice),
    m_flags{},
    m_ringSizeComputeScratch(0),
    m_dvgprExtraAceScratch(0)
{
}

// =====================================================================================================================
Result ComputePipeline::HwlInit(
    const ComputePipelineCreateInfo&  createInfo,
    const AbiReader&                  abiReader,
    const PalAbi::CodeObjectMetadata& metadata,
    Util::MsgPackReader*              pMetadataReader)
{
    m_flags.pingPongEn = createInfo.flags.reverseWorkgroupOrder;

    if (createInfo.groupLaunchGuarantee != TriState::Disable)
    {
        m_flags.enableGroupLaunchGuarantee = true;
    }

    Device* pDevice = static_cast<Device*>(m_pDevice->GetGfxDevice());

    CodeObjectUploader uploader(m_pDevice, abiReader);

    const GpuHeap heap = IsInternal() ? GpuHeapLocal : m_pDevice->GetPublicSettings()->pipelinePreferredHeap;
    Result result = PerformRelocationsAndUploadToGpuMemory(metadata, heap, &uploader);

    if (result == Result::Success)
    {
        m_chunkCs.HwlInit(uploader, metadata, createInfo.interleaveSize, m_flags.enableGroupLaunchGuarantee);

        m_flags.isWave32                    = m_chunkCs.IsWave32();
        m_flags.is2dDispatchInterleave      = m_chunkCs.Is2dDispatchInterleave();
        m_flags.isDefaultDispatchInterleave = m_chunkCs.IsDefaultDispatchInterleave();

        m_threadsPerTg.x = m_chunkCs.GetHwReg<mmCOMPUTE_NUM_THREAD_X, COMPUTE_NUM_THREAD_X>().bits.NUM_THREAD_FULL;
        m_threadsPerTg.y = m_chunkCs.GetHwReg<mmCOMPUTE_NUM_THREAD_Y, COMPUTE_NUM_THREAD_Y>().bits.NUM_THREAD_FULL;
        m_threadsPerTg.z = m_chunkCs.GetHwReg<mmCOMPUTE_NUM_THREAD_Z, COMPUTE_NUM_THREAD_Z>().bits.NUM_THREAD_FULL;

        m_dvgprExtraAceScratch = m_chunkCs.GetDvgprExtraAceScratch();
    }

    if (result == Result::Success)
    {
        result = uploader.End(&m_uploadFenceToken);
    }

    if (result == Result::Success)
    {
        UpdateRingSizeComputeScratch(CalcScratchMemSize(metadata));
    }

    return result;
}

// =====================================================================================================================
// Initializes HW-specific state related to this compute pipeline (register values, user-data mapping, etc.) using the
// specified Pipeline ABI processor.
Result ComputePipeline::HwlInit(
    const ComputePipelineCreateInfo&  createInfo,
    const AbiReader&                  abiReader,
    const HsaAbi::CodeObjectMetadata& metadata,
    MsgPackReader*                    pMetadataReader,
    const Extent3d&                   groupSize)
{
    Result result = Result::Success;

    m_flags.pingPongEn = createInfo.flags.reverseWorkgroupOrder;

    if (createInfo.groupLaunchGuarantee != TriState::Disable)
    {
        m_flags.enableGroupLaunchGuarantee = true;
    }

    const GpuHeap heap = IsInternal() ? GpuHeapLocal : m_pDevice->GetPublicSettings()->pipelinePreferredHeap;

    CodeObjectUploader uploader(m_pDevice, abiReader);
    const llvm::amdhsa::kernel_descriptor_t& desc = KernelDescriptor();

    result = PerformRelocationsAndUploadToGpuMemory(0u, heap, &uploader);

    if (result == Result::Success)
    {
        const uint32 hash = MetroHash::Compact32(m_info.internalPipelineHash.stable);
        m_chunkCs.HwlInit(uploader,
                          metadata,
                          desc,
                          hash,
                          groupSize,
                          createInfo.interleaveSize,
                          m_flags.enableGroupLaunchGuarantee);

        m_flags.isWave32 = m_chunkCs.IsWave32();
        m_flags.is2dDispatchInterleave = m_chunkCs.Is2dDispatchInterleave();
        m_flags.isDefaultDispatchInterleave = m_chunkCs.IsDefaultDispatchInterleave();

        m_threadsPerTg.x = m_chunkCs.GetHwReg<mmCOMPUTE_NUM_THREAD_X, COMPUTE_NUM_THREAD_X>().bits.NUM_THREAD_FULL;
        m_threadsPerTg.y = m_chunkCs.GetHwReg<mmCOMPUTE_NUM_THREAD_Y, COMPUTE_NUM_THREAD_Y>().bits.NUM_THREAD_FULL;
        m_threadsPerTg.z = m_chunkCs.GetHwReg<mmCOMPUTE_NUM_THREAD_Z, COMPUTE_NUM_THREAD_Z>().bits.NUM_THREAD_FULL;
    }

    if (result == Result::Success)
    {
        result = uploader.End(&m_uploadFenceToken);
    }

    if (result == Result::Success)
    {
        UpdateRingSizeComputeScratch(CalcScratchMemSize(metadata));
    }
    return result;
}

// =====================================================================================================================
void ComputePipeline::UpdateRingSizeComputeScratch(
    uint32 scratchMemorySizeInDword)
{
    m_ringSizeComputeScratch = scratchMemorySizeInDword;
}

// =====================================================================================================================
uint32 ComputePipeline::CalcScratchMemSize(
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
    const Util::HsaAbi::CodeObjectMetadata& metadata)
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
Result ComputePipeline::GetShaderStats(
    ShaderType   shaderType,
    ShaderStats* pShaderStats,
    bool         getDisassemblySize
    ) const
{
    PAL_ASSERT(pShaderStats != nullptr);

    Result result = Result::ErrorUnavailable;

    if (shaderType == ShaderType::Compute)
    {
        result = GetShaderStatsForStage(shaderType, m_stageInfo, nullptr, pShaderStats);

        if (result == Result::Success)
        {
            pShaderStats->shaderStageMask       = ApiShaderStageCompute;
            pShaderStats->palShaderHash         = m_info.shader[static_cast<uint32>(shaderType)].hash;
            pShaderStats->cs.numThreadsPerGroup = m_threadsPerTg;
            pShaderStats->common.gpuVirtAddress =
                GetOriginalAddress(m_chunkCs.GetHwReg<mmCOMPUTE_PGM_LO, COMPUTE_PGM_LO>().bits.DATA, 0);

            pShaderStats->common.ldsSizePerThreadGroup =
                m_chunkCs.GetHwReg<mmCOMPUTE_PGM_RSRC2, COMPUTE_PGM_RSRC2>().bits.LDS_SIZE <<
                (LdsDwGranularityShift + 2);
        }
    }

    return result;
}

// =====================================================================================================================
Result ComputePipeline::LinkWithLibraries(
    const IShaderLibrary*const* ppLibraryList,
    uint32                      libraryCount)
{
    Result result = Result::Success;

    const uint32 prevStackSizeInBytes = m_stackSizeInBytes;

    COMPUTE_PGM_RSRC1 computePgmRsrc1 = m_chunkCs.GetHwReg<mmCOMPUTE_PGM_RSRC1, COMPUTE_PGM_RSRC1>();
    COMPUTE_PGM_RSRC2 computePgmRsrc2 = m_chunkCs.GetHwReg<mmCOMPUTE_PGM_RSRC2, COMPUTE_PGM_RSRC2>();
    COMPUTE_PGM_RSRC3 computePgmRsrc3 = m_chunkCs.GetHwReg<mmCOMPUTE_PGM_RSRC3, COMPUTE_PGM_RSRC3>();

    for (uint32 idx = 0; (idx < libraryCount) && (result == Result::Success); idx++)
    {
        const auto*const pLibObj = static_cast<const Gfx12::ComputeShaderLibrary*const>(ppLibraryList[idx]);
        // In case this shaderLibrary did not use internal dma queue to upload ELF, the UploadFenceToken
        // of the shaderLibrary is 0.
        m_uploadFenceToken = Max(m_uploadFenceToken, pLibObj->GetUploadFenceToken());
        m_pagingFenceVal   = Max(m_pagingFenceVal,   pLibObj->GetPagingFenceVal());

        result = m_chunkCs.MergeUserDataLayout(pLibObj->UserDataLayout());
        if (result != Result::Success)
        {
            break;
        }

        if (pLibObj->GetShaderLibFunctionInfos().NumElements() == 0)
        {
            // Skip library with no functions for propagation of register usage etc.
            continue;
        }

        const LibraryHwInfo& libObjRegInfo = pLibObj->HwInfo();
        if (pLibObj->IsWave32() != bool(m_flags.isWave32))
        {
            PAL_ASSERT(0);
            // If the main pipeline and the shader library has a different wavefront size,
            // LinkWithLibraries should fail.
            result = Result::ErrorIncompatibleLibrary;
            break;
        }

        PAL_ASSERT((computePgmRsrc1.bits.FWD_PROGRESS == libObjRegInfo.libRegs.computePgmRsrc1.bits.FWD_PROGRESS) &&
                   (computePgmRsrc1.bits.WGP_MODE     == libObjRegInfo.libRegs.computePgmRsrc1.bits.WGP_MODE));

        computePgmRsrc1.bits.SGPRS = Max(computePgmRsrc1.bits.SGPRS, libObjRegInfo.libRegs.computePgmRsrc1.bits.SGPRS);
        computePgmRsrc1.bits.VGPRS = Max(computePgmRsrc1.bits.VGPRS, libObjRegInfo.libRegs.computePgmRsrc1.bits.VGPRS);
        computePgmRsrc1.bits.MEM_ORDERED  |= libObjRegInfo.libRegs.computePgmRsrc1.bits.MEM_ORDERED;
        computePgmRsrc1.bits.FWD_PROGRESS |= libObjRegInfo.libRegs.computePgmRsrc1.bits.FWD_PROGRESS;
        computePgmRsrc1.bits.WGP_MODE     |= libObjRegInfo.libRegs.computePgmRsrc1.bits.WGP_MODE;

        computePgmRsrc2.bits.USER_SGPR =
            Max(computePgmRsrc2.bits.USER_SGPR, libObjRegInfo.libRegs.computePgmRsrc2.bits.USER_SGPR);
        computePgmRsrc2.bits.LDS_SIZE =
            Max(computePgmRsrc2.bits.LDS_SIZE, libObjRegInfo.libRegs.computePgmRsrc2.bits.LDS_SIZE);
        computePgmRsrc2.bits.TIDIG_COMP_CNT =
            Max(computePgmRsrc2.bits.TIDIG_COMP_CNT, libObjRegInfo.libRegs.computePgmRsrc2.bits.TIDIG_COMP_CNT);
        computePgmRsrc2.bits.SCRATCH_EN |= libObjRegInfo.libRegs.computePgmRsrc2.bits.SCRATCH_EN;
        computePgmRsrc2.bits.TGID_X_EN  |= libObjRegInfo.libRegs.computePgmRsrc2.bits.TGID_X_EN;
        computePgmRsrc2.bits.TGID_Y_EN  |= libObjRegInfo.libRegs.computePgmRsrc2.bits.TGID_Y_EN;
        computePgmRsrc2.bits.TGID_Z_EN  |= libObjRegInfo.libRegs.computePgmRsrc2.bits.TGID_Z_EN;
        computePgmRsrc2.bits.TG_SIZE_EN |= libObjRegInfo.libRegs.computePgmRsrc2.bits.TG_SIZE_EN;

        computePgmRsrc3.bits.SHARED_VGPR_CNT =
            Max(computePgmRsrc3.bits.SHARED_VGPR_CNT, libObjRegInfo.libRegs.computePgmRsrc3.bits.SHARED_VGPR_CNT);

        m_stackSizeInBytes = Max(m_stackSizeInBytes,
                                 pLibObj->GetMaxStackSizeInBytes() * m_maxFunctionCallDepth);
    }

    if (result == Result::Success)
    {
        LibraryHwInfo updatedLibHwInfo;
        updatedLibHwInfo.libRegs.computePgmRsrc1 = computePgmRsrc1;
        updatedLibHwInfo.libRegs.computePgmRsrc2 = computePgmRsrc2;
        updatedLibHwInfo.libRegs.computePgmRsrc3 = computePgmRsrc3;
        m_chunkCs.UpdateAfterLibraryLink(updatedLibHwInfo);

        const uint32 currStackSizeInDword = RoundUpQuotient(m_stackSizeInBytes, uint32(sizeof(uint32)));
        if (currStackSizeInDword > RoundUpQuotient(prevStackSizeInBytes, uint32(sizeof(uint32))))
        {
            UpdateRingSizeComputeScratch(currStackSizeInDword);
        }
    }

    return result;
}

} // Gfx12
} // Pal
