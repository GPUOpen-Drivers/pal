/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6ComputePipeline.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "palFile.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// User-data signature for an unbound compute pipeline.
const ComputePipelineSignature NullCsSignature =
{
    { 0, },                     // User-data mapping for each shader stage
    UserDataNotMapped,          // Register address for numWorkGroups
    NoUserDataSpilling,         // Spill threshold
    0,                          // User-data entry limit
    0,                          // User-data hash
};
static_assert(UserDataNotMapped == 0, "Unexpected value for indicating unmapped user-data entries!");

// Base count of SH registers which are loaded using LOAD_SH_REG_INDEX when binding to a universal command buffer.
constexpr uint32 BaseLoadedShRegCount =
    1 + // mmCOMPUTE_PGM_LO
    1 + // mmCOMPUTE_PGM_HI
    1 + // mmCOMPUTE_PGM_RSRC1
    0 + // mmCOMPUTE_PGM_RSRC2 is not included because it partially depends on bind-time state
    0 + // mmCOMPUTE_RESOURCE_LIMITS is not included because it partially depends on bind-time state
    1 + // mmCOMPUTE_NUM_THREAD_X
    1 + // mmCOMPUTE_NUM_THREAD_Y
    1 + // mmCOMPUTE_NUM_THREAD_Z
    1;  // mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg

// =====================================================================================================================
ComputePipeline::ComputePipeline(
    Device* pDevice,
    bool    isInternal)  // True if this is a PAL-owned pipeline (i.e., an RPM pipeline).
    :
    Pal::ComputePipeline(pDevice->Parent(), isInternal),
    m_pDevice(pDevice)
{
    memset(&m_regs, 0, sizeof(m_regs));
    memset(&m_loadPath, 0, sizeof(m_loadPath));
    memset(&m_prefetch, 0, sizeof(m_prefetch));
    memcpy(&m_signature, &NullCsSignature, sizeof(m_signature));
}

// =====================================================================================================================
// Initializes the signature of a compute pipeline using a pipeline ELF.
void ComputePipeline::SetupSignatureFromElf(
    const CodeObjectMetadata& metadata,
    const RegisterVector&     registers)
{
    uint16  entryToRegAddr[MaxUserDataEntries] = { };

    m_signature.stage.firstUserSgprRegAddr = (mmCOMPUTE_USER_DATA_0 + FastUserDataStartReg);
    for (uint16 offset = mmCOMPUTE_USER_DATA_0; offset <= mmCOMPUTE_USER_DATA_15; ++offset)
    {
        uint32 value = 0;
        if (registers.HasEntry(offset, &value))
        {
            if (value < MaxUserDataEntries)
            {
                PAL_ASSERT(offset >= m_signature.stage.firstUserSgprRegAddr);
                const uint8 userSgprId = static_cast<uint8>(offset - m_signature.stage.firstUserSgprRegAddr);
                entryToRegAddr[value]  = offset;

                m_signature.stage.mappedEntry[userSgprId] = static_cast<uint8>(value);
                m_signature.stage.userSgprCount = Max<uint8>(userSgprId + 1, m_signature.stage.userSgprCount);
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::GlobalTable))
            {
                PAL_ASSERT(offset == (mmCOMPUTE_USER_DATA_0 + InternalTblStartReg));
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::PerShaderTable))
            {
                PAL_ASSERT(offset == (mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg));
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::SpillTable))
            {
                m_signature.stage.spillTableRegAddr = static_cast<uint16>(offset);
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::Workgroup))
            {
                m_signature.numWorkGroupsRegAddr = static_cast<uint16>(offset);
            }
            else if ((value == static_cast<uint32>(Abi::UserDataMapping::VertexBufferTable)) ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::StreamOutTable))    ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::BaseVertex))        ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::BaseInstance))      ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::DrawIndex))         ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::BaseIndex))         ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::Log2IndexSize))     ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::EsGsLdsSize)))
            {
                PAL_ALERT_ALWAYS(); // These are for graphics pipelines only!
            }
            else
            {
                // This appears to be an illegally-specified user-data register!
                PAL_NEVER_CALLED();
            }
        } // If HasEntry()
    } // For each user-SGPR

    if (metadata.pipeline.hasEntry.spillThreshold != 0)
    {
        m_signature.spillThreshold = static_cast<uint16>(metadata.pipeline.spillThreshold);
    }

    if (metadata.pipeline.hasEntry.userDataLimit != 0)
    {
        m_signature.userDataLimit = static_cast<uint16>(metadata.pipeline.userDataLimit);
    }
    // Compute a hash of the regAddr array and spillTableRegAddr for the CS stage.
    MetroHash64::Hash(
        reinterpret_cast<const uint8*>(&m_signature.stage),
        sizeof(UserDataEntryMap),
        reinterpret_cast<uint8* const>(&m_signature.userDataHash));
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
    const Gfx6PalSettings&   settings  = m_pDevice->Settings();
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    RegisterVector registers(m_pDevice->GetPlatform());

    Result result = pMetadataReader->Seek(metadata.pipeline.registers);

    if (result == Result::Success)
    {
        result = pMetadataReader->Unpack(&registers);
    }

    ComputePipelineUploader uploader(m_pDevice,
                                     abiReader,
                                     settings.enableLoadIndexForObjectBinds ? BaseLoadedShRegCount : 0);
    if (result == Result::Success)
    {
        // Next, handle relocations and upload the pipeline code & data to GPU memory.
        result = PerformRelocationsAndUploadToGpuMemory(
            metadata,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 631
            (createInfo.flags.overrideGpuHeap == 1) ? createInfo.preferredHeapType : GpuHeapInvisible,
#else
            IsInternal() ? GpuHeapLocal : m_pDevice->Parent()->GetPublicSettings()->pipelinePreferredHeap,
#endif
            &uploader);
    }

    if (result == Result::Success)
    {
        const auto& csStageMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Cs)];
        if (csStageMetadata.hasEntry.scratchMemorySize != 0)
        {
            UpdateRingSizes(csStageMetadata.scratchMemorySize);
        }

        // Next, update our PM4 image with the now-known GPU virtual addresses for the shader entrypoints and
        // internal SRD table addresses:

        GpuSymbol symbol = { };
        if (uploader.GetPipelineGpuSymbol(Abi::PipelineSymbolType::CsMainEntry, &symbol) == Result::Success)
        {
            m_stageInfo.codeLength    = static_cast<size_t>(symbol.size);
            PAL_ASSERT(symbol.gpuVirtAddr == Pow2Align(symbol.gpuVirtAddr, 256));
            PAL_ASSERT(Get256BAddrHi(symbol.gpuVirtAddr) == 0);

            m_regs.computePgmLo.bits.DATA = Get256BAddrLo(symbol.gpuVirtAddr);
            m_regs.computePgmHi.bits.DATA = 0;
        }

        if (uploader.GetPipelineGpuSymbol(Abi::PipelineSymbolType::CsShdrIntrlTblPtr, &symbol) == Result::Success)
        {
            m_regs.computeUserDataLo.bits.DATA = LowPart(symbol.gpuVirtAddr);
        }

        // Initialize the rest of the PM4 image initialization with register data contained in the ELF:

        m_regs.computePgmRsrc1.u32All         = registers.At(mmCOMPUTE_PGM_RSRC1);
        m_regs.dynamic.computePgmRsrc2.u32All = registers.At(mmCOMPUTE_PGM_RSRC2);
        m_regs.computeNumThreadX.u32All       = registers.At(mmCOMPUTE_NUM_THREAD_X);
        m_regs.computeNumThreadY.u32All       = registers.At(mmCOMPUTE_NUM_THREAD_Y);
        m_regs.computeNumThreadZ.u32All       = registers.At(mmCOMPUTE_NUM_THREAD_Z);

        m_threadsPerTgX = m_regs.computeNumThreadX.bits.NUM_THREAD_FULL;
        m_threadsPerTgY = m_regs.computeNumThreadY.bits.NUM_THREAD_FULL;
        m_threadsPerTgZ = m_regs.computeNumThreadZ.bits.NUM_THREAD_FULL;

        if (uploader.EnableLoadIndexPath())
        {
            m_loadPath.gpuVirtAddr = uploader.ShRegGpuVirtAddr();
            m_loadPath.count       = uploader.ShRegisterCount();

            uploader.AddShReg(mmCOMPUTE_PGM_LO, m_regs.computePgmLo);
            uploader.AddShReg(mmCOMPUTE_PGM_HI, m_regs.computePgmHi);

            uploader.AddShReg((mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg), m_regs.computeUserDataLo);

            uploader.AddShReg(mmCOMPUTE_PGM_RSRC1,    m_regs.computePgmRsrc1);
            uploader.AddShReg(mmCOMPUTE_NUM_THREAD_X, m_regs.computeNumThreadX);
            uploader.AddShReg(mmCOMPUTE_NUM_THREAD_Y, m_regs.computeNumThreadY);
            uploader.AddShReg(mmCOMPUTE_NUM_THREAD_Z, m_regs.computeNumThreadZ);
        }
        PAL_ASSERT(m_uploadFenceToken == 0);
        result = uploader.End(&m_uploadFenceToken);

        if (result == Result::Success)
        {
            registers.HasEntry(mmCOMPUTE_RESOURCE_LIMITS, &m_regs.dynamic.computeResourceLimits.u32All);
            const uint32 threadsPerGroup = (m_threadsPerTgX * m_threadsPerTgY * m_threadsPerTgZ);
            const uint32 wavesPerGroup   = RoundUpQuotient(threadsPerGroup, chipProps.gfx6.nativeWavefrontSize);

            // SIMD_DEST_CNTL: Controls whichs SIMDs thread groups get scheduled on.  If the number of
            // waves-per-TG is a multiple of 4, this should be 1, otherwise 0.
            m_regs.dynamic.computeResourceLimits.bits.SIMD_DEST_CNTL = ((wavesPerGroup % 4) == 0) ? 1 : 0;

            // Force even distribution on all SIMDs in CU for workgroup size is 64
            // This has shown some good improvements if #CU per SE not a multiple of 4
            if (((chipProps.gfx6.numShaderArrays * chipProps.gfx6.numCuPerSh) & 0x3) && (wavesPerGroup == 1))
            {
                m_regs.dynamic.computeResourceLimits.bits.FORCE_SIMD_DIST__CI__VI = 1;
            }

            if (m_pDevice->Parent()->LegacyHwsTrapHandlerPresent())
            {
                // If the legacy HWS's trap handler is present, compute shaders must always set the TRAP_PRESENT
                // flag.

                // TODO: Handle the case where the client enabled a trap handler and the hardware scheduler's trap handler
                // is already active!
                PAL_ASSERT(m_regs.dynamic.computePgmRsrc2.bits.TRAP_PRESENT == 0);
                m_regs.dynamic.computePgmRsrc2.bits.TRAP_PRESENT = 1;
            }

            // LOCK_THRESHOLD: Sets per-SH low threshold for locking.  Set in units of 4, 0 disables locking.
            // LOCK_THRESHOLD's maximum value: (6 bits), in units of 4, so it is max of 252.
            constexpr uint32 Gfx6MaxLockThreshold = 252;
            PAL_ASSERT(settings.csLockThreshold <= Gfx6MaxLockThreshold);
            m_regs.dynamic.computeResourceLimits.bits.LOCK_THRESHOLD =
                Min((settings.csLockThreshold >> 2), Gfx6MaxLockThreshold >> 2);

            // SIMD_DEST_CNTL: Controls whichs SIMDs thread groups get scheduled on.  If no override is set, just keep
            // the existing value in COMPUTE_RESOURCE_LIMITS.
            switch (settings.csSimdDestCntl)
            {
            case CsSimdDestCntlForce1:
                m_regs.dynamic.computeResourceLimits.bits.SIMD_DEST_CNTL = 1;
                break;
            case CsSimdDestCntlForce0:
                m_regs.dynamic.computeResourceLimits.bits.SIMD_DEST_CNTL = 0;
                break;
            default:
                PAL_ASSERT(settings.csSimdDestCntl == CsSimdDestCntlDefault);
                break;
            }

            m_pDevice->CmdUtil().BuildPipelinePrefetchPm4(uploader, &m_prefetch);

            // Finally, update the pipeline signature with user-mapping data contained in the ELF:
            SetupSignatureFromElf(metadata, registers);
        }
    }

    return result;
}

// =====================================================================================================================
// Helper function to compute the WAVES_PER_SH field of the COMPUTE_RESOURCE_LIMITS register.
uint32 ComputePipeline::CalcMaxWavesPerSh(
    float maxWavesPerCu
    ) const
{
    // The maximum number of waves per SH in "register units".
    // By default leave the WAVES_PER_SH field unchanged (either 0 or populated from ELF).
    uint32 wavesPerSh = m_regs.dynamic.computeResourceLimits.bits.WAVES_PER_SH;

    if (maxWavesPerCu > 0)
    {
        const auto&  chipProps            = m_pDevice->Parent()->ChipProperties();
        const auto&  gfx6ChipProps        = chipProps.gfx6;
        const uint32 numWavefrontsPerCu   = gfx6ChipProps.numSimdPerCu * gfx6ChipProps.numWavesPerSimd;
        const uint32 maxWavesPerShCompute = gfx6ChipProps.maxNumCuPerSh * numWavefrontsPerCu;

        // We assume no one is trying to use more than 100% of all waves.
        PAL_ASSERT(maxWavesPerCu <= numWavefrontsPerCu);

        const uint32 maxWavesPerSh = static_cast<uint32>(round(maxWavesPerCu * gfx6ChipProps.numCuPerSh));

        if (chipProps.gfxLevel == GfxIpLevel::GfxIp6)
        {
            constexpr uint32 MaxWavesPerShGfx6ComputeUnitSize = 16u;
            const uint32     maxWavesPerShGfx6Compute         = maxWavesPerShCompute / MaxWavesPerShGfx6ComputeUnitSize;

            // For Gfx6 compute shaders, the WAVES_PER_SH field is in units of 16 waves and must not exceed 63.
            // We must also clamp to one if maxWavesPerSh rounded down to zero to prevent the limit from being removed.
            wavesPerSh = Min(maxWavesPerShGfx6Compute, Max(1u, maxWavesPerSh / MaxWavesPerShGfx6ComputeUnitSize));
        }
        else
        {
            // For gfx7+ compute shaders, it is in units of 1 wave and must not exceed 1023.
            wavesPerSh = Min(maxWavesPerShCompute, maxWavesPerSh);
        }
    }

    return wavesPerSh;
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
    // Disable the LOAD_INDEX path if the PM4 optimizer is enabled or for compute command buffers.  The optimizer cannot
    // optimize these load packets because the register values are in GPU memory.  Additionally, any client requesting
    // PM4 optimization is trading CPU cycles for GPU performance, so the savings of using LOAD_INDEX is not important.
    // This gets disabled for compute command buffers because the MEC does not support any LOAD packets.
    if ((m_loadPath.count == 0)           ||
        pCmdStream->Pm4OptimizerEnabled() ||
        (pCmdStream->GetEngineType() == EngineType::EngineTypeCompute))
    {
        pCmdSpace = WriteShCommandsSetPath(pCmdStream, pCmdSpace);
    }
    else
    {
        const CmdUtil& cmdUtil = m_pDevice->CmdUtil();
        pCmdSpace += cmdUtil.BuildLoadShRegsIndex(m_loadPath.gpuVirtAddr, m_loadPath.count, ShaderCompute, pCmdSpace);
    }

    auto dynamic = m_regs.dynamic; // "Dynamic" bind-time register state

    // TG_PER_CU: Sets the CS threadgroup limit per CU. Range is 1 to 15, 0 disables the limit.
    constexpr uint32 Gfx6MaxTgPerCu = 15;
    dynamic.computeResourceLimits.bits.TG_PER_CU = Min(csInfo.maxThreadGroupsPerCu, Gfx6MaxTgPerCu);
    if (csInfo.maxWavesPerCu > 0)
    {
        dynamic.computeResourceLimits.bits.WAVES_PER_SH = CalcMaxWavesPerSh(csInfo.maxWavesPerCu);
    }

    const auto& chipProperties = m_pDevice->Parent()->ChipProperties();

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 628
    if (chipProperties.gfxLevel != GfxIpLevel::GfxIp6)
    {
        // CU_GROUP_COUNT: Sets the number of CS threadgroups to attempt to send to a single CU before moving to
        // the next CU. Range is 1 to 8, 0 disables the limit.
        constexpr uint32 Gfx7PlusMaxCuGroupCount = 8;
        if (csInfo.tgScheduleCountPerCu > 0)
        {
            dynamic.computeResourceLimits.bits.CU_GROUP_COUNT__CI__VI =
                Min(csInfo.tgScheduleCountPerCu, Gfx7PlusMaxCuGroupCount) - 1;
        }
    }
#endif

    if (csInfo.ldsBytesPerTg > 0)
    {
        const uint32 ldsSizeDwords = csInfo.ldsBytesPerTg / sizeof(uint32);

        // Round to nearest multiple of the LDS granularity, then convert to the register value.
        if (chipProperties.gfxLevel == GfxIpLevel::GfxIp6)
        {
            // NOTE: Gfx6: Granularity for the LDS_SIZE field is 64, range is 0->128 which allocates 0 to 8K DWORDs.
            dynamic.computePgmRsrc2.bits.LDS_SIZE =
                Pow2Align(ldsSizeDwords, Gfx6LdsDwGranularity) >> Gfx6LdsDwGranularityShift;
        }
        else
        {
            // NOTE: Gfx7+: Granularity for the LDS_SIZE field is 128, range is 0->128 which allocates 0 to 16K DWORDs.
            dynamic.computePgmRsrc2.bits.LDS_SIZE =
                Pow2Align(ldsSizeDwords, Gfx7LdsDwGranularity) >> Gfx7LdsDwGranularityShift;
        }
    }

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PGM_RSRC2,
                                                            dynamic.computePgmRsrc2.u32All,
                                                            pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_RESOURCE_LIMITS,
                                                            dynamic.computeResourceLimits.u32All,
                                                            pCmdSpace);

    const auto& perfData = m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Cs)];
    if (perfData.regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(perfData.regOffset, perfData.gpuVirtAddr, pCmdSpace);
    }

    if (prefetch)
    {
        memcpy(pCmdSpace, &m_prefetch, m_prefetch.spaceNeeded * sizeof(uint32));
        pCmdSpace += m_prefetch.spaceNeeded;
    }

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
            pShaderStats->shaderStageMask        = ApiShaderStageCompute;
            pShaderStats->palShaderHash          = m_info.shader[static_cast<uint32>(shaderType)].hash;
            pShaderStats->cs.numThreadsPerGroupX = m_threadsPerTgX;
            pShaderStats->cs.numThreadsPerGroupY = m_threadsPerTgY;
            pShaderStats->cs.numThreadsPerGroupZ = m_threadsPerTgZ;
            pShaderStats->common.gpuVirtAddress  = GetOriginalAddress(m_regs.computePgmLo.bits.DATA,
                                                                      m_regs.computePgmHi.bits.DATA);

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

// =====================================================================================================================
// Writes PM4 SET commands to the specified command stream.  This is only expected to be called when the LOAD path is
// not in use and we need to use the SET path fallback.
uint32* ComputePipeline::WriteShCommandsSetPath(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmCOMPUTE_NUM_THREAD_X,
                                              mmCOMPUTE_NUM_THREAD_Z,
                                              ShaderCompute,
                                              &m_regs.computeNumThreadX,
                                              pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmCOMPUTE_PGM_LO,
                                              mmCOMPUTE_PGM_HI,
                                              ShaderCompute,
                                              &m_regs.computePgmLo,
                                              pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PGM_RSRC1,
                                                            m_regs.computePgmRsrc1.u32All,
                                                            pCmdSpace);

    return pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg,
                                                       m_regs.computeUserDataLo.u32All,
                                                       pCmdSpace);
}

// =====================================================================================================================
// Sets the total stack frame size for indirect shaders in the pipeline
void ComputePipeline::SetStackSizeInBytes(
    uint32 stackSizeInBytes)
{
    m_stackSizeInBytes = stackSizeInBytes;
    UpdateRingSizes(stackSizeInBytes);
}

// =====================================================================================================================
// Update the device that this compute pipeline has some new ring-size requirements.
void ComputePipeline::UpdateRingSizes(
    uint32 scratchMemorySize)
{
    ShaderRingItemSizes ringSizes = { };

    if (scratchMemorySize != 0)
    {
        ringSizes.itemSize[static_cast<uint32>(ShaderRingType::ComputeScratch)] = (scratchMemorySize / sizeof(uint32));
    }

    // Inform the device that this pipeline has some new ring-size requirements.
    m_pDevice->UpdateLargestRingSizes(&ringSizes);
}

} // Gfx6
} // Pal
