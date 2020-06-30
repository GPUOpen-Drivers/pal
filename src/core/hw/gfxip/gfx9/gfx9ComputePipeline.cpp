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

#include "core/platform.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9ComputePipeline.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9ShaderLibrary.h"
#include "palFile.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// User-data signature for an unbound compute pipeline.
const ComputePipelineSignature NullCsSignature =
{
    { 0, },                     // User-data mapping for each shader stage
    UserDataNotMapped,          // Register address for numWorkGroups
    NoUserDataSpilling,         // Spill threshold
    0,                          // User-data entry limit
    UserDataNotMapped,          // Register address for performance data buffer
    0,                          // Flags
};
static_assert(UserDataNotMapped == 0, "Unexpected value for indicating unmapped user-data entries!");

// =====================================================================================================================
ComputePipeline::ComputePipeline(
    Device* pDevice,
    bool    isInternal)  // True if this is a PAL-owned pipeline (i.e., an RPM pipeline).
    :
    Pal::ComputePipeline(pDevice->Parent(), isInternal),
    m_pDevice(pDevice),
    m_chunkCs(*pDevice,
              &m_stageInfo,
              &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Cs)])
{
    memcpy(&m_signature, &NullCsSignature, sizeof(m_signature));
}

// =====================================================================================================================
// Initializes HW-specific state related to this compute pipeline (register values, user-data mapping, etc.) using the
// specified Pipeline ABI processor.
Result ComputePipeline::HwlInit(
    const ComputePipelineCreateInfo& createInfo,
    const AbiReader&                 abiReader,
    const CodeObjectMetadata&        metadata,
    MsgPackReader*                   pMetadataReader)
{
    const Gfx9PalSettings&   settings  = m_pDevice->Settings();
    const CmdUtil&           cmdUtil   = m_pDevice->CmdUtil();
    const auto&              regInfo   = cmdUtil.GetRegInfo();
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    RegisterVector registers(m_pDevice->GetPlatform());
    Result result = pMetadataReader->Seek(metadata.pipeline.registers);

    if (result == Result::Success)
    {
        result = pMetadataReader->Unpack(&registers);
    }

    const uint32 loadedShRegCount = m_chunkCs.EarlyInit();
    ComputePipelineUploader uploader(m_pDevice, abiReader, loadedShRegCount);

    if (result == Result::Success)
    {
        // Next, handle relocations and upload the pipeline code & data to GPU memory.
        result = PerformRelocationsAndUploadToGpuMemory(
            metadata,
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION < 502)
            (createInfo.flags.preferNonLocalHeap == 1) ? GpuHeapGartUswc : GpuHeapInvisible,
#else
            (createInfo.flags.overrideGpuHeap == 1) ? createInfo.preferredHeapType : GpuHeapInvisible,
#endif
            &uploader);
    }

    if (result ==  Result::Success)
    {
        // Update the pipeline signature with user-mapping data contained in the ELF:
        m_chunkCs.SetupSignatureFromElf(&m_signature, metadata, registers);

        const auto& csStageMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Cs)];
        if (csStageMetadata.hasEntry.scratchMemorySize != 0)
        {
            UpdateRingSizes(csStageMetadata.scratchMemorySize);
        }

        const uint32 wavefrontSize = IsWave32() ? 32 : 64;

        m_chunkCs.LateInit<ComputePipelineUploader>(abiReader,
                                                    registers,
                                                    wavefrontSize,
 #if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 556
                                                    createInfo.pIndirectFuncList,
                                                    createInfo.indirectFuncCount,
#else
                                                    nullptr,
                                                    0,
#endif
                                                    &m_threadsPerTgX,
                                                    &m_threadsPerTgY,
                                                    &m_threadsPerTgZ,
                                                    &uploader);
        PAL_ASSERT(m_uploadFenceToken == 0);
        result = uploader.End(&m_uploadFenceToken);
    }

    if (result == Result::Success)
    {
        ResourceDescriptionPipeline desc = {};
        desc.pPipelineInfo = &GetInfo();
        desc.pCreateFlags = &createInfo.flags;
        ResourceCreateEventData data = {};
        data.type = ResourceType::Pipeline;
        data.pResourceDescData = &desc;
        data.resourceDescSize = sizeof(ResourceDescriptionPipeline);
        data.pObj = this;
        m_pDevice->GetPlatform()->GetEventProvider()->LogGpuMemoryResourceCreateEvent(data);

        GpuMemoryResourceBindEventData bindData = {};
        bindData.pObj = this;
        bindData.pGpuMemory = m_gpuMem.Memory();
        bindData.requiredGpuMemSize = m_gpuMemSize;
        bindData.offset = m_gpuMem.Offset();
        m_pDevice->GetPlatform()->GetEventProvider()->LogGpuMemoryResourceBindEvent(bindData);
    }

    return result;
}

// =====================================================================================================================
// Helper function to compute the WAVES_PER_SH field of the COMPUTE_RESOURCE_LIMITS register.
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
        const auto&  gfx9ChipProps        = chipProps.gfx9;
        const uint32 numWavefrontsPerCu   = (gfx9ChipProps.numSimdPerCu * gfx9ChipProps.numWavesPerSimd);
        const uint32 maxWavesPerShCompute = numWavefrontsPerCu * gfx9ChipProps.maxNumCuPerSh;

        // We assume no one is trying to use more than 100% of all waves.
        PAL_ASSERT(maxWavesPerCu <= numWavefrontsPerCu);

        const uint32 maxWavesPerSh = static_cast<uint32>(round(maxWavesPerCu * gfx9ChipProps.numCuPerSh));

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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 556
    Result result = Result::Success;
    const auto&  gpuInfo       = m_pDevice->Parent()->ChipProperties();

    // When linking this pipeline with any shader function library,
    // the compute resource registers we write into the ELF binary must
    // account for the worst-case of any hardware resource used by either the main shader,
    // or any of the function library.
    const HwRegInfo& mainCsRegInfo = m_chunkCs.HWInfo();

    const bool isWave32 = IsWave32();

    regCOMPUTE_PGM_RSRC1 computePgmRsrc1 = mainCsRegInfo.computePgmRsrc1;
    regCOMPUTE_PGM_RSRC2 computePgmRsrc2 = mainCsRegInfo.dynamic.computePgmRsrc2;
    regCOMPUTE_PGM_RSRC3 computePgmRsrc3 = mainCsRegInfo.computePgmRsrc3;

    for (uint32 idx = 0; idx < libraryCount; idx++)
    {
        const auto*const pLibObj = static_cast<const Pal::Gfx9::ShaderLibrary*const>(ppLibraryList[idx]);
        // In case this shaderLibrary did not use internal dma queue to upload ELF, the UploadFenceToken
        // of the shaderLibrary is 0.
        m_uploadFenceToken = Max(m_uploadFenceToken, pLibObj->GetUploadFenceToken());

        const LibraryHwInfo& libObjRegInfo = pLibObj->HwInfo();

        PAL_ASSERT(pLibObj->IsWave32() == isWave32);
        if (pLibObj->IsWave32() != isWave32)
        {
            // If the main pipeline and the shader library has a different wavefront size,
            // LinkWithLibraries should fail.
            result = Result::ErrorIncompatibleLibrary;
            break;
        }

        computePgmRsrc1.bits.SGPRS = Max(computePgmRsrc1.bits.SGPRS, libObjRegInfo.libRegs.computePgmRsrc1.bits.SGPRS);
        computePgmRsrc1.bits.VGPRS = Max(computePgmRsrc1.bits.VGPRS, libObjRegInfo.libRegs.computePgmRsrc1.bits.VGPRS);

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

        if (IsGfx10Plus(gpuInfo.gfxLevel))
        {
            // FWD_PROGRESS and WGP_MODE should match across all the shader functions and the main shader.
            //
            /// @note Currently we do not support null main shader, but OR the FWD_PROGRESS and WGP_MODE registers from
            ///       the shader functions anyway to make it work with null main shader in the future.
            PAL_ASSERT((computePgmRsrc1.gfx10Plus.FWD_PROGRESS ==
                            libObjRegInfo.libRegs.computePgmRsrc1.gfx10Plus.FWD_PROGRESS) &&
                       (computePgmRsrc1.gfx10Plus.WGP_MODE     ==
                            libObjRegInfo.libRegs.computePgmRsrc1.gfx10Plus.WGP_MODE));

            computePgmRsrc1.gfx10Plus.MEM_ORDERED  |= libObjRegInfo.libRegs.computePgmRsrc1.gfx10Plus.MEM_ORDERED;
            computePgmRsrc1.gfx10Plus.FWD_PROGRESS |= libObjRegInfo.libRegs.computePgmRsrc1.gfx10Plus.FWD_PROGRESS;
            computePgmRsrc1.gfx10Plus.WGP_MODE     |= libObjRegInfo.libRegs.computePgmRsrc1.gfx10Plus.WGP_MODE;

            computePgmRsrc3.bits.SHARED_VGPR_CNT =
                Max(computePgmRsrc3.bits.SHARED_VGPR_CNT, libObjRegInfo.libRegs.computePgmRsrc3.bits.SHARED_VGPR_CNT);
        }

        const uint32 stackSizeNeededInBytes = pLibObj->GetMaxStackSizeInBytes() * m_maxFunctionCallDepth;
        if (stackSizeNeededInBytes > m_stackSizeInBytes)
        {
            m_stackSizeInBytes = stackSizeNeededInBytes;
            UpdateRingSizes(stackSizeNeededInBytes);
        }
    }

    // Update m_chunCs with updated register values
    m_chunkCs.UpdateComputePgmRsrsAfterLibraryLink(computePgmRsrc1, computePgmRsrc2, computePgmRsrc3);

    return result;
#else
    return Result::Unsupported;
#endif
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
    return m_chunkCs.WriteShCommands(pCmdStream, pCmdSpace, csInfo, prefetch);
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
        result = GetShaderStatsForStage(m_stageInfo, nullptr, pShaderStats);
        if (result == Result::Success)
        {
            pShaderStats->shaderStageMask              = ApiShaderStageCompute;
            pShaderStats->palShaderHash                = m_info.shader[static_cast<uint32>(shaderType)].hash;
            pShaderStats->cs.numThreadsPerGroupX       = m_threadsPerTgX;
            pShaderStats->cs.numThreadsPerGroupY       = m_threadsPerTgY;
            pShaderStats->cs.numThreadsPerGroupZ       = m_threadsPerTgZ;
            pShaderStats->common.gpuVirtAddress        = m_chunkCs.CsProgramGpuVa();
            pShaderStats->common.ldsSizePerThreadGroup = chipProps.gfxip.ldsSizePerThreadGroup;

            AbiReader abiReader(m_pDevice->GetPlatform(), m_pPipelineBinary);
            result = abiReader.Init();

            MsgPackReader      metadataReader;
            CodeObjectMetadata metadata;

            if (result == Result::Success)
            {
                result = abiReader.GetMetadata(&metadataReader, &metadata);
            }
        }
    }

    return result;
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 580
// =====================================================================================================================
// Sets the total stack frame size for indirect shaders in the pipeline
void ComputePipeline::SetStackSizeInBytes(
    uint32 stackSizeInBytes)
{
    m_stackSizeInBytes = stackSizeInBytes;
    UpdateRingSizes(stackSizeInBytes);
}
#endif

// =====================================================================================================================
// Update the device that this compute pipeline has some new ring-size requirements.
void ComputePipeline::UpdateRingSizes(
    uint32 scratchMemorySize)
{
    ShaderRingItemSizes ringSizes = { };

    if (scratchMemorySize != 0)
    {
        if (IsGfx10Plus(m_pDevice->Parent()->ChipProperties().gfxLevel) && (IsWave32() == false))
        {
            // We allocate scratch memory based on the minimum wave size for the chip, which for Gfx10+ ASICs will
            // be Wave32. In order to appropriately size the scratch memory (reported in the ELF as per-thread) for
            // a Wave64, we need to multiply by 2.
            scratchMemorySize *= 2;
        }

        ringSizes.itemSize[static_cast<size_t>(ShaderRingType::ComputeScratch)] =
            (scratchMemorySize / sizeof(uint32));
    }

    // Inform the device that this pipeline has some new ring-size requirements.
    m_pDevice->UpdateLargestRingSizes(&ringSizes);
}

} // Gfx9
} // Pal
