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

#include "core/platform.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9ComputePipeline.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9PrefetchMgr.h"
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
    { UserDataNotMapped, },     // User-data mapping for each shader stage
    { UserDataNotMapped, },     // Indirect user-data table mapping
    UserDataNotMapped,          // Register address for numWorkGroups
    NoUserDataSpilling,         // Spill threshold
    0,                          // User-data entry limit
};
static_assert(UserDataNotMapped == 0, "Unexpected value for indicating unmapped user-data entries!");

// =====================================================================================================================
ComputePipeline::ComputePipeline(
    Device* pDevice,
    bool    isInternal)  // True if this is a PAL-owned pipeline (i.e., an RPM pipeline).
    :
    Pal::ComputePipeline(pDevice->Parent(), isInternal),
    m_pDevice(pDevice)
{
    memset(&m_pm4Commands,        0, sizeof(m_pm4Commands));
    memset(&m_pm4CommandsDynamic, 0, sizeof(m_pm4CommandsDynamic));
    memcpy(&m_signature, &NullCsSignature, sizeof(m_signature));
}

// =====================================================================================================================
// Initializes the signature of a compute pipeline using a pipeline ELF.
static void SetupSignatureFromElf(
    const AbiProcessor&       abiProcessor,
    ComputePipelineSignature* pSignature)
{
    for (uint32 offset = mmCOMPUTE_USER_DATA_0; offset <= mmCOMPUTE_USER_DATA_15; ++offset)
    {
        uint32 value = 0;
        if (abiProcessor.HasRegisterEntry(offset, &value))
        {
            if (value < MaxUserDataEntries)
            {
                pSignature->stage.regAddr[value] = static_cast<uint16>(offset);
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
                pSignature->stage.spillTableRegAddr = static_cast<uint16>(offset);
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::Workgroup))
            {
                pSignature->numWorkGroupsRegAddr = static_cast<uint16>(offset);
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::GdsRange))
            {
                PAL_ASSERT(offset == (mmCOMPUTE_USER_DATA_0 + GdsRangeRegCompute));
            }
            else if ((value == static_cast<uint32>(Abi::UserDataMapping::BaseVertex))    ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::BaseInstance))  ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::DrawIndex))     ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::BaseIndex))     ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::Log2IndexSize)) ||
                     (value == static_cast<uint32>(Abi::UserDataMapping::EsGsLdsSize)))
            {
                PAL_ALERT_ALWAYS(); // These are for graphics pipelines only!
            }
            else
            {
                // This appears to be an illegally-specified user-data register!
                PAL_NEVER_CALLED();
            }
        } // If HasRegisterEntry()
    } // For each user-SGPR

    // Compute a hash of the regAddr array and spillTableRegAddr for the CS stage.
    constexpr uint64 HashedDataLength =
        (sizeof(pSignature->stage.regAddr) + sizeof(pSignature->stage.spillTableRegAddr));

    MetroHash64::Hash(
        reinterpret_cast<const uint8*>(pSignature->stage.regAddr),
        HashedDataLength,
        reinterpret_cast<uint8* const>(&pSignature->stage.userDataHash));

    // Indirect user-data table(s):
    uint32 value = 0;
    for (uint32 i = 0; i < MaxIndirectUserDataTables; ++i)
    {
        const auto entryType = static_cast<Abi::PipelineMetadataType>(
                static_cast<uint32>(Abi::PipelineMetadataType::IndirectTableEntryLow) + i);

        if (abiProcessor.HasPipelineMetadataEntry(entryType, &value))
        {
            pSignature->indirectTableAddr[i] = static_cast<uint16>(value);
        }
    }

    // NOTE: We skip the stream-out table address here because it is not used by compute pipelines.

    if (abiProcessor.HasPipelineMetadataEntry(Abi::PipelineMetadataType::SpillThreshold, &value))
    {
        pSignature->spillThreshold = static_cast<uint16>(value);
    }

    if (abiProcessor.HasPipelineMetadataEntry(Abi::PipelineMetadataType::UserDataLimit, &value))
    {
        pSignature->userDataLimit = static_cast<uint16>(value);
    }
}

// =====================================================================================================================
// Initializes HW-specific state related to this compute pipeline (register values, user-data mapping, etc.) using the
// specified Pipeline ABI processor.
Result ComputePipeline::HwlInit(
    const AbiProcessor& abiProcessor)
{
    const Gfx9PalSettings& settings  = m_pDevice->Settings();
    const auto&            chipProps = m_pDevice->Parent()->ChipProperties();

    // First, handle relocations and upload the pipeline code & data to GPU memory.
    gpusize codeGpuVirtAddr = 0;
    gpusize dataGpuVirtAddr = 0;
    Result result = PerformRelocationsAndUploadToGpuMemory(abiProcessor, &codeGpuVirtAddr, &dataGpuVirtAddr);
    if (result ==  Result::Success)
    {
        BuildPm4Headers();
        UpdateRingSizes(abiProcessor);

        // Next, update our PM4 image with the now-known GPU virtual addresses for the shader entrypoints and
        // internal SRD table addresses:
        Abi::PipelineSymbolEntry csProgram  = { };
        if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::CsMainEntry, &csProgram))
        {
            const gpusize csProgramVa = (csProgram.value + codeGpuVirtAddr);
            PAL_ASSERT(csProgramVa == Pow2Align(csProgramVa, 256));
            PAL_ASSERT(Get256BAddrHi(csProgramVa) == 0);

            m_pm4Commands.computePgmLo.bits.DATA = Get256BAddrLo(csProgramVa);
            m_pm4Commands.computePgmHi.bits.DATA = 0;

            m_stageInfo.codeLength = static_cast<size_t>(csProgram.size);
        }

        Abi::PipelineSymbolEntry csSrdTable = { };
        if (abiProcessor.HasPipelineSymbolEntry(Abi::PipelineSymbolType::CsShdrIntrlTblPtr, &csSrdTable))
        {
            const gpusize csSrdTableVa = (csSrdTable.value + dataGpuVirtAddr);
            m_pm4Commands.computeUserDataLo.bits.DATA = LowPart(csSrdTableVa);
        }

        // Initialize the rest of the PM4 image initialization with register data contained in the ELF:

        m_pm4Commands.computePgmRsrc1.u32All   = abiProcessor.GetRegisterEntry(mmCOMPUTE_PGM_RSRC1);
        m_pm4Commands.computePgmRsrc2.u32All   = abiProcessor.GetRegisterEntry(mmCOMPUTE_PGM_RSRC2);
        m_pm4Commands.computeNumThreadX.u32All = abiProcessor.GetRegisterEntry(mmCOMPUTE_NUM_THREAD_X);
        m_pm4Commands.computeNumThreadY.u32All = abiProcessor.GetRegisterEntry(mmCOMPUTE_NUM_THREAD_Y);
        m_pm4Commands.computeNumThreadZ.u32All = abiProcessor.GetRegisterEntry(mmCOMPUTE_NUM_THREAD_Z);

        m_threadsPerTgX = m_pm4Commands.computeNumThreadX.bits.NUM_THREAD_FULL;
        m_threadsPerTgY = m_pm4Commands.computeNumThreadY.bits.NUM_THREAD_FULL;
        m_threadsPerTgZ = m_pm4Commands.computeNumThreadZ.bits.NUM_THREAD_FULL;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 345
        if (false == abiProcessor.HasRegisterEntry(mmCOMPUTE_RESOURCE_LIMITS,
                                                   &m_pm4CommandsDynamic.computeResourceLimits.u32All))
#endif
        {
            const uint32 threadsPerGroup = (m_threadsPerTgX * m_threadsPerTgY * m_threadsPerTgZ);
            const uint32 wavesPerGroup   = RoundUpQuotient(threadsPerGroup, chipProps.gfx9.wavefrontSize);

            // SIMD_DEST_CNTL: Controls which SIMDs thread groups get scheduled on.  If the number of
            // waves-per-TG is a multiple of 4, this should be 1, otherwise 0.
            m_pm4CommandsDynamic.computeResourceLimits.bits.SIMD_DEST_CNTL = ((wavesPerGroup % 4) == 0) ? 1 : 0;

            // Force even distribution on all SIMDs in CU for workgroup size is 64
            // This has shown some good improvements if #CU per SE not a multiple of 4
            if (((chipProps.gfx9.numShaderArrays * chipProps.gfx9.numCuPerSh) & 0x3) && (wavesPerGroup == 1))
            {
                m_pm4CommandsDynamic.computeResourceLimits.bits.FORCE_SIMD_DIST = 1;
            }
        }

        if (m_pDevice->Parent()->HwsTrapHandlerPresent() &&
            (chipProps.gfxLevel == GfxIpLevel::GfxIp9))
        {

            // If the hardware scheduler's trap handler is present, compute shaders must always set the TRAP_PRESENT
            // flag.

            // TODO: Handle the case where the client enabled a trap handler and the hardware scheduler's trap handler
            // is already active!
            PAL_ASSERT(m_pm4Commands.computePgmRsrc2.bits.TRAP_PRESENT == 0);
            m_pm4Commands.computePgmRsrc2.bits.TRAP_PRESENT = 1;
        }

        // LOCK_THRESHOLD: Sets per-SH low threshold for locking.  Set in units of 4, 0 disables locking.
        // LOCK_THRESHOLD's maximum value: (6 bits), in units of 4, so it is max of 252.
        constexpr uint32 Gfx6MaxLockThreshold = 252;
        PAL_ASSERT(settings.csLockThreshold <= Gfx6MaxLockThreshold);
        m_pm4CommandsDynamic.computeResourceLimits.bits.LOCK_THRESHOLD = Min((settings.csLockThreshold >> 2),
                                                                             Gfx6MaxLockThreshold >> 2);

        // SIMD_DEST_CNTL: Controls whichs SIMDs thread groups get scheduled on.  If no override is set, just keep
        // the existing value in COMPUTE_RESOURCE_LIMITS.
        switch (settings.csSimdDestCntl)
        {
        case CsSimdDestCntlForce1:
            m_pm4CommandsDynamic.computeResourceLimits.bits.SIMD_DEST_CNTL = 1;
            break;
        case CsSimdDestCntlForce0:
            m_pm4CommandsDynamic.computeResourceLimits.bits.SIMD_DEST_CNTL = 0;
            break;
        default:
            PAL_ASSERT(settings.csSimdDestCntl == CsSimdDestCntlDefault);
            break;
        }

        // Finally, update the pipeline signature with user-mapping data contained in the ELF:
        SetupSignatureFromElf(abiProcessor, &m_signature);
    }

    return result;
}

// =====================================================================================================================
// Helper function to compute the WAVES_PER_SH field of the COMPUTE_RESOURCE_LIMITS register.
uint32 ComputePipeline::CalcMaxWavesPerSh(
    uint32 maxWavesPerCu
    ) const
{
    constexpr uint32 MaxWavesPerShCompute = (COMPUTE_RESOURCE_LIMITS__WAVES_PER_SH_MASK >>
                                             COMPUTE_RESOURCE_LIMITS__WAVES_PER_SH__SHIFT);

    const auto& gfx9ChipProps = m_pDevice->Parent()->ChipProperties().gfx9;

    // The maximum number of waves per SH in "register units".
    // By default set the WAVES_PER_SH field to the maximum possible value.
    uint32 wavesPerSh = MaxWavesPerShCompute;

    if (maxWavesPerCu > 0)
    {
        // We assume no one is trying to use more than 100% of all waves.
        const uint32 numWavefrontsPerCu = (NumSimdPerCu * gfx9ChipProps.numWavesPerSimd);
        PAL_ASSERT(maxWavesPerCu <= numWavefrontsPerCu);

        const uint32 maxWavesPerSh = (maxWavesPerCu * gfx9ChipProps.numCuPerSh);

        // For compute shaders, it is in units of 1 wave and must not exceed the max.
        wavesPerSh = Min(MaxWavesPerShCompute, maxWavesPerSh);
    }

    return wavesPerSh;
}

// =====================================================================================================================
// Writes the PM4 commands required to bind this pipeline. Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* ComputePipeline::WriteCommands(
    Pal::CmdStream*                 pCmdStream,
    uint32*                         pCmdSpace,
    const DynamicComputeShaderInfo& csInfo
    ) const
{
    auto*const pGfx9CmdStream = static_cast<CmdStream*>(pCmdStream);

    pCmdSpace = pGfx9CmdStream->WritePm4Image(m_pm4Commands.spaceNeeded, &m_pm4Commands, pCmdSpace);

    ComputePipelinePm4ImgDynamic pm4CommandsDynamic = m_pm4CommandsDynamic;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 345
    if (pm4CommandsDynamic.computeResourceLimits.bits.WAVES_PER_SH == 0)
#endif
    {
        pm4CommandsDynamic.computeResourceLimits.bits.WAVES_PER_SH = CalcMaxWavesPerSh(csInfo.maxWavesPerCu);
    }

    // TG_PER_CU: Sets the CS threadgroup limit per CU. Range is 1 to 15, 0 disables the limit.
    constexpr uint32 Gfx9MaxTgPerCu = 15;
    pm4CommandsDynamic.computeResourceLimits.bits.TG_PER_CU = Min(csInfo.maxThreadGroupsPerCu, Gfx9MaxTgPerCu);

    pCmdSpace = pGfx9CmdStream->WritePm4Image(pm4CommandsDynamic.spaceNeeded, &pm4CommandsDynamic, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Requests that this pipeline indicates what it would like to prefetch.
uint32* ComputePipeline::RequestPrefetch(
    const Pal::PrefetchMgr& prefetchMgr,
    uint32*                 pCmdSpace
    ) const
{
    const auto& gfx9PrefetchMgr = static_cast<const PrefetchMgr&>(prefetchMgr);

    return gfx9PrefetchMgr.RequestPrefetch(PrefetchCs,
                                           GetOriginalAddress(m_pm4Commands.computePgmLo.bits.DATA,
                                                              m_pm4Commands.computePgmHi.bits.DATA),
                                           m_stageInfo.codeLength,
                                           pCmdSpace);
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
            pShaderStats->common.gpuVirtAddress  = GetOriginalAddress(m_pm4Commands.computePgmLo.bits.DATA,
                                                                      m_pm4Commands.computePgmHi.bits.DATA);

            pShaderStats->common.ldsSizePerThreadGroup = chipProps.gfxip.ldsSizePerThreadGroup;
        }
    }

    return result;
}

// =====================================================================================================================
// Builds the packet headers for the various PM4 images associated with this pipeline.  Register values and packet
// payloads are computed elsewhere.
void ComputePipeline::BuildPm4Headers()
{
    const CmdUtil& cmdUtil = m_pDevice->CmdUtil();

    // 1st PM4 packet: sets the following compute registers: COMPUTE_NUM_THREAD_X, COMPUTE_NUM_THREAD_Y,
    // COMPUTE_NUM_THREAD_Z.
    m_pm4Commands.spaceNeeded = cmdUtil.BuildSetSeqShRegs(mmCOMPUTE_NUM_THREAD_X,
                                                          mmCOMPUTE_NUM_THREAD_Z,
                                                          ShaderCompute,
                                                          &m_pm4Commands.hdrComputeNumThread);

    // 2nd PM4 packet: sets the following compute registers: COMPUTE_PGM_LO, COMPUTE_PGM_HI.
    m_pm4Commands.spaceNeeded += cmdUtil.BuildSetSeqShRegs(mmCOMPUTE_PGM_LO,
                                                           mmCOMPUTE_PGM_HI,
                                                           ShaderCompute,
                                                           &m_pm4Commands.hdrComputePgm);

    // 3rd PM4 packet: sets the following compute registers: COMPUTE_PGM_RSRC1, COMPUTE_PGM_RSRC2.
    m_pm4Commands.spaceNeeded += cmdUtil.BuildSetSeqShRegs(mmCOMPUTE_PGM_RSRC1,
                                                           mmCOMPUTE_PGM_RSRC2,
                                                           ShaderCompute,
                                                           &m_pm4Commands.hdrComputePgmRsrc);

    // 4th PM4 packet: sets the following compute register: COMPUTE_USER_DATA_1.
    m_pm4Commands.spaceNeeded += cmdUtil.BuildSetOneShReg(mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg,
                                                          ShaderCompute,
                                                          &m_pm4Commands.hdrComputeUserData);

    // 5th PM4 packet: sets the following compute register: COMPUTE_RESOURCE_LIMITS.
    m_pm4CommandsDynamic.spaceNeeded = cmdUtil.BuildSetOneShReg(mmCOMPUTE_RESOURCE_LIMITS,
                                                                ShaderCompute,
                                                                &m_pm4CommandsDynamic.hdrComputeResourceLimits);
}

// =====================================================================================================================
// Update the device that this compute pipeline has some new ring-size requirements.
void ComputePipeline::UpdateRingSizes(
    const AbiProcessor& abiProcessor)
{
    ShaderRingItemSizes ringSizes = { };

    uint32 scratchUsageBytes = 0;
    if (abiProcessor.HasPipelineMetadataEntry(Abi::PipelineMetadataType::CsScratchByteSize, &scratchUsageBytes))
    {
        ringSizes.itemSize[static_cast<size_t>(ShaderRingType::ComputeScratch)] = (scratchUsageBytes / sizeof(uint32));
    }

    // Inform the device that this pipeline has some new ring-size requirements.
    m_pDevice->UpdateLargestRingSizes(&ringSizes);
}

} // Gfx9
} // Pal
