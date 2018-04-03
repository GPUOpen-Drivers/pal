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
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6ComputePipeline.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6PrefetchMgr.h"
#include "palPipelineAbiProcessorImpl.h"
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
void ComputePipeline::SetupSignatureFromElf(
    const AbiProcessor& abiProcessor)
{
    uint16  entryToRegAddr[MaxUserDataEntries] = { };

    m_signature.stage.firstUserSgprRegAddr = (mmCOMPUTE_USER_DATA_0 + FastUserDataStartReg);
    for (uint16 offset = mmCOMPUTE_USER_DATA_0; offset <= mmCOMPUTE_USER_DATA_15; ++offset)
    {
        uint32 value = 0;
        if (abiProcessor.HasRegisterEntry(offset, &value))
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
            else if (value == static_cast<uint32>(Abi::UserDataMapping::GdsRange))
            {
#if PAL_COMPUTE_GDS_OPT
                PAL_ASSERT(offset == (mmCOMPUTE_USER_DATA_0 + ComputeGdsRangeReg));
#else
                PAL_ASSERT(offset == (mmCOMPUTE_USER_DATA_0 + GdsRangeReg));
#endif
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

    // Indirect user-data table(s):
    uint32 value = 0;
    for (uint32 i = 0; i < MaxIndirectUserDataTables; ++i)
    {
        const auto entryType = static_cast<Abi::PipelineMetadataType>(
                static_cast<uint32>(Abi::PipelineMetadataType::IndirectTableEntryLow) + i);

        if (abiProcessor.HasPipelineMetadataEntry(entryType, &value) && (value != UserDataNotMapped))
        {
            m_signature.indirectTableAddr[i]          = static_cast<uint16>(value);
            m_signature.stage.indirectTableRegAddr[i] = entryToRegAddr[value - 1];
        }
    }

    // NOTE: We skip the stream-out table address here because it is not used by compute pipelines.

    if (abiProcessor.HasPipelineMetadataEntry(Abi::PipelineMetadataType::SpillThreshold, &value))
    {
        m_signature.spillThreshold = static_cast<uint16>(value);
    }

    if (abiProcessor.HasPipelineMetadataEntry(Abi::PipelineMetadataType::UserDataLimit, &value))
    {
        m_signature.userDataLimit = static_cast<uint16>(value);
    }
}

// =====================================================================================================================
// Initializes HW-specific state related to this compute pipeline (register values, user-data mapping, etc.) using the
// specified Pipeline ABI processor.
Result ComputePipeline::HwlInit(
    const AbiProcessor& abiProcessor)
{
    const Gfx6PalSettings&   settings  = m_pDevice->Settings();
    const auto&              chipProps = m_pDevice->Parent()->ChipProperties();

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
            const gpusize csProgramVa  = (csProgram.value + codeGpuVirtAddr);
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

        m_pm4Commands.computePgmRsrc1.u32All       = abiProcessor.GetRegisterEntry(mmCOMPUTE_PGM_RSRC1);
        m_pm4Commands.computePgmRsrc2.u32All       = abiProcessor.GetRegisterEntry(mmCOMPUTE_PGM_RSRC2);
        m_pm4Commands.computeNumThreadX.u32All     = abiProcessor.GetRegisterEntry(mmCOMPUTE_NUM_THREAD_X);
        m_pm4Commands.computeNumThreadY.u32All     = abiProcessor.GetRegisterEntry(mmCOMPUTE_NUM_THREAD_Y);
        m_pm4Commands.computeNumThreadZ.u32All     = abiProcessor.GetRegisterEntry(mmCOMPUTE_NUM_THREAD_Z);

        m_threadsPerTgX = m_pm4Commands.computeNumThreadX.bits.NUM_THREAD_FULL;
        m_threadsPerTgY = m_pm4Commands.computeNumThreadY.bits.NUM_THREAD_FULL;
        m_threadsPerTgZ = m_pm4Commands.computeNumThreadZ.bits.NUM_THREAD_FULL;

        const uint32 threadsPerGroup = (m_threadsPerTgX * m_threadsPerTgY * m_threadsPerTgZ);
        const uint32 wavesPerGroup   = RoundUpQuotient(threadsPerGroup, chipProps.gfx6.wavefrontSize);

        // SIMD_DEST_CNTL: Controls whichs SIMDs thread groups get scheduled on.  If the number of
        // waves-per-TG is a multiple of 4, this should be 1, otherwise 0.
        m_pm4CommandsDynamic.computeResourceLimits.bits.SIMD_DEST_CNTL = ((wavesPerGroup % 4) == 0) ? 1 : 0;

        // Force even distribution on all SIMDs in CU for workgroup size is 64
        // This has shown some good improvements if #CU per SE not a multiple of 4
        if (((chipProps.gfx6.numShaderArrays * chipProps.gfx6.numCuPerSh) & 0x3) && (wavesPerGroup == 1))
        {
            m_pm4CommandsDynamic.computeResourceLimits.bits.FORCE_SIMD_DIST__CI__VI = 1;
        }

        if (m_pDevice->Parent()->HwsTrapHandlerPresent())
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
        m_pm4CommandsDynamic.computeResourceLimits.bits.LOCK_THRESHOLD =
            Min((settings.csLockThreshold >> 2), Gfx6MaxLockThreshold >> 2);

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
        SetupSignatureFromElf(abiProcessor);
    }

    return result;
}

// =====================================================================================================================
// Helper function to compute the WAVES_PER_SH field of the COMPUTE_RESOURCE_LIMITS register.
uint32 ComputePipeline::CalcMaxWavesPerSh(
    uint32 maxWavesPerCu
    ) const
{
    constexpr uint32 MaxWavesPerShCompute             = 1023u;
    constexpr uint32 MaxWavesPerShGfx6Compute         = 63u;
    constexpr uint32 MaxWavesPerShGfx6ComputeUnitSize = 16u;

    const auto& chipProps  = m_pDevice->Parent()->ChipProperties();

    // The maximum number of waves per SH in "register units".
    // By default set the WAVES_PER_SH field to the maximum possible value.
    uint32 wavesPerSh = (chipProps.gfxLevel == GfxIpLevel::GfxIp6) ? MaxWavesPerShGfx6Compute : MaxWavesPerShCompute;

    if (maxWavesPerCu > 0)
    {
        // We assume no one is trying to use more than 100% of all waves.
        const uint32 numWavefrontsPerCu = (NumSimdPerCu * chipProps.gfx6.numWavesPerSimd);
        PAL_ASSERT(maxWavesPerCu <= numWavefrontsPerCu);

        const uint32 maxWavesPerSh = (maxWavesPerCu * chipProps.gfx6.numCuPerSh);

        if (chipProps.gfxLevel == GfxIpLevel::GfxIp6)
        {
            // For Gfx6 compute shaders, the WAVES_PER_SH field is in units of 16 waves and must not exceed 63.
            // We must also clamp to one if maxWavesPerSh rounded down to zero to prevent the limit from being removed.
            wavesPerSh = Min(MaxWavesPerShGfx6Compute, Max(1u, maxWavesPerSh / MaxWavesPerShGfx6ComputeUnitSize));
        }
        else
        {
            // For gfx7+ compute shaders, it is in units of 1 wave and must not exceed 1023.
            wavesPerSh = Min(MaxWavesPerShCompute, maxWavesPerSh);
        }
    }

    return wavesPerSh;
}

// =====================================================================================================================
// Writes the PM4 commands required to bind this pipeline. Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* ComputePipeline::WriteCommands(
    Pal::CmdStream*                 pCmdStream,
    uint32*                         pCmdSpace,
    const DynamicComputeShaderInfo& csInfo,
    const Pal::PrefetchMgr&         prefetchMgr
    ) const
{
    auto*const pGfx6CmdStream = static_cast<CmdStream*>(pCmdStream);

    pCmdSpace = pGfx6CmdStream->WritePm4Image(m_pm4Commands.spaceNeeded, &m_pm4Commands, pCmdSpace);

    ComputePipelinePm4ImgDynamic pm4CommandsDynamic = m_pm4CommandsDynamic;

    pm4CommandsDynamic.computeResourceLimits.bits.WAVES_PER_SH = CalcMaxWavesPerSh(csInfo.maxWavesPerCu);

    // TG_PER_CU: Sets the CS threadgroup limit per CU. Range is 1 to 15, 0 disables the limit.
    constexpr uint32 Gfx6MaxTgPerCu = 15;
    pm4CommandsDynamic.computeResourceLimits.bits.TG_PER_CU = Min(csInfo.maxThreadGroupsPerCu, Gfx6MaxTgPerCu);

    pCmdSpace = pGfx6CmdStream->WritePm4Image(pm4CommandsDynamic.spaceNeeded, &pm4CommandsDynamic, pCmdSpace);

    const PerfDataInfo& perfData = m_perfDataInfo[static_cast<uint32>(Util::Abi::HardwareStage::Cs)];
    if (perfData.regOffset != UserDataNotMapped)
    {
        pCmdSpace =
            pGfx6CmdStream->WriteSetOneShReg<ShaderCompute>(perfData.regOffset, perfData.gpuVirtAddr, pCmdSpace);
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 384
    if (csInfo.ldsBytesPerTg > 0)
    {
        const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
        const uint32 ldsSizeDwords = csInfo.ldsBytesPerTg / sizeof(uint32);
        uint32 ldsSpace = 0;

        // Round to nearest multiple of the LDS granularity, then convert to the register value.
        if (chipProps.gfxLevel == GfxIpLevel::GfxIp6)
        {
            // NOTE: On GFX6, granularity for the LDS_SIZE field is 64, range is 0->128 which allocates 0 to 8K DWORDs.
            ldsSpace = Pow2Align(ldsSizeDwords, Gfx6LdsDwGranularity) >> Gfx6LdsDwGranularityShift;
        }
        else
        {
            // NOTE: On GFX7+, granularity for the LDS_SIZE field is 128, range is 0->128 which allocates 0 to 16K DWORDs.
            ldsSpace = Pow2Align(ldsSizeDwords, Gfx7LdsDwGranularity) >> Gfx7LdsDwGranularityShift;
        }
        regCOMPUTE_PGM_RSRC2 computePgmRsrc2 = m_pm4Commands.computePgmRsrc2;
        computePgmRsrc2.bits.LDS_SIZE = ldsSpace;
        pCmdSpace =
            pGfx6CmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PGM_RSRC2, computePgmRsrc2.u32All, pCmdSpace);
    }
#endif

    const auto& gfx6PrefetchMgr = static_cast<const PrefetchMgr&>(prefetchMgr);

    pCmdSpace = gfx6PrefetchMgr.RequestPrefetch(PrefetchCs,
                                                GetOriginalAddress(m_pm4Commands.computePgmLo.bits.DATA,
                                                                   m_pm4Commands.computePgmHi.bits.DATA),
                                                m_stageInfo.codeLength,
                                                pCmdSpace);

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

} // Gfx6
} // Pal
