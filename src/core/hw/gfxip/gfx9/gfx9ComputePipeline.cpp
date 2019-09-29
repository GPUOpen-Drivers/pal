/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palPipelineAbiProcessorImpl.h"
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
    m_chunkCs(*pDevice, &m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Cs)])
{
    memcpy(&m_signature, &NullCsSignature, sizeof(m_signature));
}

// =====================================================================================================================
// Initializes HW-specific state related to this compute pipeline (register values, user-data mapping, etc.) using the
// specified Pipeline ABI processor.
Result ComputePipeline::HwlInit(
    const ComputePipelineCreateInfo& createInfo,
    const AbiProcessor&              abiProcessor,
    const CodeObjectMetadata&        metadata,
    MsgPackReader*                   pMetadataReader)
{
    const Gfx9PalSettings&   settings  = m_pDevice->Settings();
    const CmdUtil&           cmdUtil   = m_pDevice->CmdUtil();
    const auto&              regInfo   = cmdUtil.GetRegInfo();
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    RegisterVector registers(m_pDevice->GetPlatform());
    Result result = pMetadataReader->Unpack(&registers);

    const uint32 loadedShRegCount = m_chunkCs.EarlyInit();
    ComputePipelineUploader uploader(m_pDevice, loadedShRegCount);

    if (result == Result::Success)
    {
        // Next, handle relocations and upload the pipeline code & data to GPU memory.
        result = PerformRelocationsAndUploadToGpuMemory(
            abiProcessor,
            metadata,
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 488)
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 488) && (PAL_CLIENT_INTERFACE_MAJOR_VERSION < 502)
            (createInfo.flags.preferNonLocalHeap == 1) ? GpuHeapGartUswc : GpuHeapInvisible,
#else
            (createInfo.flags.overrideGpuHeap == 1) ? createInfo.preferredHeapType : GpuHeapInvisible,
#endif
#else
            GpuHeapInvisible,
#endif
            &uploader);
    }

    if (result ==  Result::Success)
    {
        UpdateRingSizes(metadata);

        // Update the pipeline signature with user-mapping data contained in the ELF:
        m_chunkCs.SetupSignatureFromElf(&m_signature, metadata, registers);

        const uint32 wavefrontSize = IsWave32() ? 32 : 64;

        m_chunkCs.LateInit<ComputePipelineUploader>(abiProcessor,
                                                    registers,
                                                    wavefrontSize,
                                                    createInfo.pIndirectFuncList,
                                                    createInfo.indirectFuncCount,
                                                    &m_threadsPerTgX,
                                                    &m_threadsPerTgY,
                                                    &m_threadsPerTgZ,
                                                    &uploader);
        result = uploader.End();
    }

    return result;
}

// =====================================================================================================================
// Helper function to compute the WAVES_PER_SH field of the COMPUTE_RESOURCE_LIMITS register.
uint32 ComputePipeline::CalcMaxWavesPerSh(
    const GpuChipProperties& chipProps,
    uint32                   maxWavesPerCu)
{
    // The maximum number of waves per SH in "register units".
    // By default set the WAVE_LIMIT field to be unlimited.
    // Limits given by the ELF will only apply if the caller doesn't set their own limit.
    uint32 wavesPerSh = 0;

    if (maxWavesPerCu > 0)
    {
        const auto&  gfx9ChipProps        = chipProps.gfx9;
        const uint32 numWavefrontsPerCu   = (gfx9ChipProps.numSimdPerCu * gfx9ChipProps.numWavesPerSimd);
        const uint32 maxWavesPerShCompute = numWavefrontsPerCu * gfx9ChipProps.numCuPerSh;

        // We assume no one is trying to use more than 100% of all waves.
        PAL_ASSERT(maxWavesPerCu <= numWavefrontsPerCu);

        const uint32 maxWavesPerSh = (maxWavesPerCu * gfx9ChipProps.numCuPerSh);

        // For compute shaders, it is in units of 1 wave and must not exceed the max.
        wavesPerSh = Min(maxWavesPerShCompute, maxWavesPerSh);
    }

    return wavesPerSh;
}

// =====================================================================================================================
// Writes the PM4 commands required to bind this pipeline. Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* ComputePipeline::WriteCommands(
    Pal::CmdStream*                 pCmdStream,
    uint32*                         pCmdSpace,
    const DynamicComputeShaderInfo& csInfo,
    bool                            prefetch
    ) const
{
    auto* pGfx9CmdStream = static_cast<CmdStream*>(pCmdStream);
    pCmdSpace = m_chunkCs.WriteShCommands(pGfx9CmdStream, pCmdSpace, csInfo, prefetch);
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
        }
    }

    return result;
}

// =====================================================================================================================
// Update the device that this compute pipeline has some new ring-size requirements.
void ComputePipeline::UpdateRingSizes(
    const CodeObjectMetadata& metadata)
{
    ShaderRingItemSizes ringSizes = { };

    const auto& csStageMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Cs)];
    if (csStageMetadata.hasEntry.scratchMemorySize != 0)
    {
        ringSizes.itemSize[static_cast<size_t>(ShaderRingType::ComputeScratch)] =
            (csStageMetadata.scratchMemorySize / sizeof(uint32));
    }

    // Inform the device that this pipeline has some new ring-size requirements.
    m_pDevice->UpdateLargestRingSizes(&ringSizes);
}

} // Gfx9
} // Pal
