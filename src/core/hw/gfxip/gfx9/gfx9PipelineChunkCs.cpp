/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/computePipeline.h"
#include "core/hw/gfxip/gfx9/gfx9ShaderLibrary.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9ComputePipeline.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkCs.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
PipelineChunkCs::PipelineChunkCs(
    const Device&    device,
    ShaderStageInfo* pStageInfo,
    PerfDataInfo*    pPerfDataInfo)
    :
    m_device(device),
    m_regs{},
    m_prefetch{},
    m_pCsPerfDataInfo(pPerfDataInfo),
    m_pStageInfo(pStageInfo)
{
    if (m_pStageInfo != nullptr)
    {
        m_pStageInfo->stageId = Abi::HardwareStage::Cs;
    }
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.
void PipelineChunkCs::LateInit(
    const AbiReader&       abiReader,
    const RegisterVector&  registers,
    uint32                 wavefrontSize,
    uint32*                pThreadsPerTgX,
    uint32*                pThreadsPerTgY,
    uint32*                pThreadsPerTgZ,
    PipelineUploader*      pUploader)
{
    const auto&              cmdUtil   = m_device.CmdUtil();
    const auto&              regInfo   = cmdUtil.GetRegInfo();
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    GpuSymbol symbol = { };
    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::CsMainEntry, &symbol) == Result::Success)
    {
        m_pStageInfo->codeLength  = static_cast<size_t>(symbol.size);
        PAL_ASSERT(IsPow2Aligned(symbol.gpuVirtAddr, 256u));

        m_regs.computePgmLo.bits.DATA = Get256BAddrLo(symbol.gpuVirtAddr);
        PAL_ASSERT(Get256BAddrHi(symbol.gpuVirtAddr) == 0);
    }

    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::CsShdrIntrlTblPtr, &symbol) == Result::Success)
    {
        m_regs.userDataInternalTable.bits.DATA = LowPart(symbol.gpuVirtAddr);
    }

    m_regs.computePgmRsrc1.u32All         = registers.At(mmCOMPUTE_PGM_RSRC1);
    m_regs.dynamic.computePgmRsrc2.u32All = registers.At(mmCOMPUTE_PGM_RSRC2);
    m_regs.computeNumThreadX.u32All       = registers.At(mmCOMPUTE_NUM_THREAD_X);
    m_regs.computeNumThreadY.u32All       = registers.At(mmCOMPUTE_NUM_THREAD_Y);
    m_regs.computeNumThreadZ.u32All       = registers.At(mmCOMPUTE_NUM_THREAD_Z);

    if (IsGfx10Plus(chipProps.gfxLevel))
    {
        m_regs.computePgmRsrc3.u32All = registers.At(Gfx10Plus::mmCOMPUTE_PGM_RSRC3);

#if PAL_ENABLE_PRINTS_ASSERTS
        m_device.AssertUserAccumRegsDisabled(registers, Gfx10Plus::mmCOMPUTE_USER_ACCUM_0);
#endif
    }

    if (chipProps.gfx9.supportSpp == 1)
    {
        PAL_ASSERT(regInfo.mmComputeShaderChksum != 0);
        registers.HasEntry(regInfo.mmComputeShaderChksum, &m_regs.computeShaderChksum.u32All);
    }

    *pThreadsPerTgX = m_regs.computeNumThreadX.bits.NUM_THREAD_FULL;
    *pThreadsPerTgY = m_regs.computeNumThreadY.bits.NUM_THREAD_FULL;
    *pThreadsPerTgZ = m_regs.computeNumThreadZ.bits.NUM_THREAD_FULL;

    registers.HasEntry(mmCOMPUTE_RESOURCE_LIMITS, &m_regs.dynamic.computeResourceLimits.u32All);

    const uint32 threadsPerGroup = (*pThreadsPerTgX) * (*pThreadsPerTgY) * (*pThreadsPerTgZ);
    const uint32 wavesPerGroup   = RoundUpQuotient(threadsPerGroup, wavefrontSize);

    // SIMD_DEST_CNTL: Controls which SIMDs thread groups get scheduled on.  If the number of
    // waves-per-TG is a multiple of 4, this should be 1, otherwise 0.
    m_regs.dynamic.computeResourceLimits.bits.SIMD_DEST_CNTL = ((wavesPerGroup % 4) == 0) ? 1 : 0;

    // Force even distribution on all SIMDs in CU for workgroup size is 64
    // This has shown some good improvements if #CU per SE not a multiple of 4
    if (((chipProps.gfx9.numShaderArrays * chipProps.gfx9.numCuPerSh) & 0x3) && (wavesPerGroup == 1))
    {
        m_regs.dynamic.computeResourceLimits.bits.FORCE_SIMD_DIST = 1;
    }

    if (m_device.Parent()->LegacyHwsTrapHandlerPresent() && (chipProps.gfxLevel == GfxIpLevel::GfxIp9))
    {

        // If the legacy HWS's trap handler is present, compute shaders must always set the TRAP_PRESENT
        // flag.

        // TODO: Handle the case where the client enabled a trap handler and the hardware scheduler's trap handler
        // is already active!
        PAL_ASSERT(m_regs.dynamic.computePgmRsrc2.bits.TRAP_PRESENT == 0);
        m_regs.dynamic.computePgmRsrc2.bits.TRAP_PRESENT = 1;
    }

    const auto& settings = m_device.Settings();

    // LOCK_THRESHOLD: Sets per-SH low threshold for locking.  Set in units of 4, 0 disables locking.
    // LOCK_THRESHOLD's maximum value: (6 bits), in units of 4, so it is max of 252.
    constexpr uint32 Gfx9MaxLockThreshold = 252;
    PAL_ASSERT(settings.csLockThreshold <= Gfx9MaxLockThreshold);
    m_regs.dynamic.computeResourceLimits.bits.LOCK_THRESHOLD = Min((settings.csLockThreshold >> 2),
                                                                   (Gfx9MaxLockThreshold >> 2));

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

    cmdUtil.BuildPipelinePrefetchPm4(*pUploader, &m_prefetch);
}

// =====================================================================================================================
// Initializes the signature of a compute shader using a pipeline ELF.
// NOTE: Must be called before LateInit!
void PipelineChunkCs::SetupSignatureFromElf(
    ComputeShaderSignature*   pSignature,
    const CodeObjectMetadata& metadata,
    const RegisterVector&     registers)
{
    const auto& chipProps = m_device.Parent()->ChipProperties();

    pSignature->stage.firstUserSgprRegAddr = (mmCOMPUTE_USER_DATA_0 + FastUserDataStartReg);
    for (uint16 offset = mmCOMPUTE_USER_DATA_0; offset <= mmCOMPUTE_USER_DATA_15; ++offset)
    {
        uint32 value = 0;
        if (registers.HasEntry(offset, &value))
        {
            if (value < MaxUserDataEntries)
            {
                PAL_ASSERT(offset >= pSignature->stage.firstUserSgprRegAddr);
                const uint8 userSgprId = static_cast<uint8>(offset - pSignature->stage.firstUserSgprRegAddr);

                pSignature->stage.mappedEntry[userSgprId] = static_cast<uint8>(value);
                pSignature->stage.userSgprCount = Max<uint8>(userSgprId + 1, pSignature->stage.userSgprCount);
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
            else if (value == static_cast<uint32>(Abi::UserDataMapping::MeshTaskDispatchDims))
            {
                pSignature->taskDispatchDimsAddr = static_cast<uint16_t>(offset);
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::MeshTaskRingIndex))
            {
                pSignature->taskRingIndexAddr = static_cast<uint16>(offset);
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::TaskDispatchIndex))
            {
                pSignature->dispatchIndexRegAddr = static_cast<uint16>(offset);
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::MeshPipeStatsBuf))
            {
                pSignature->taskPipeStatsBufRegAddr = offset;
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::PerShaderPerfData))
            {
                m_pCsPerfDataInfo->regOffset = offset;
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
        pSignature->spillThreshold = static_cast<uint16>(metadata.pipeline.spillThreshold);
    }

    if (metadata.pipeline.hasEntry.userDataLimit != 0)
    {
        pSignature->userDataLimit = static_cast<uint16>(metadata.pipeline.userDataLimit);
    }

    // Compute a hash of the regAddr array and spillTableRegAddr for the CS stage.
     MetroHash64::Hash(
        reinterpret_cast<const uint8*>(&pSignature->stage),
        sizeof(UserDataEntryMap),
        reinterpret_cast<uint8* const>(&pSignature->userDataHash));

    // We don't bother checking the wavefront size for pre-Gfx10 GPU's since it is implicitly 64 before Gfx10. Any ELF
    // which doesn't specify a wavefront size is assumed to use 64, even on Gfx10 and newer.
    if (IsGfx9(*(m_device.Parent())) == false)
    {
        const auto& csMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Cs)];
        if (csMetadata.hasEntry.wavefrontSize != 0)
        {
            PAL_ASSERT((csMetadata.wavefrontSize == 64) || (csMetadata.wavefrontSize == 32));
            pSignature->flags.isWave32 = (csMetadata.wavefrontSize == 32);
        }
    }
}

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkCs::WriteShCommands(
    CmdStream*                      pCmdStream,
    uint32*                         pCmdSpace,
    const DynamicComputeShaderInfo& csInfo,
    gpusize                         launchDescGpuVa,
    bool                            prefetch
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    pCmdSpace = WriteShCommandsSetPath(pCmdStream, pCmdSpace, (launchDescGpuVa != 0uLL));

    pCmdSpace = WriteShCommandsDynamic(pCmdStream, pCmdSpace, csInfo, launchDescGpuVa);

    if (m_pCsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(m_pCsPerfDataInfo->regOffset,
                                                                m_pCsPerfDataInfo->gpuVirtAddr,
                                                                pCmdSpace);
    }

    if (prefetch)
    {
        memcpy(pCmdSpace, &m_prefetch, m_prefetch.spaceNeeded * sizeof(uint32));
        pCmdSpace += m_prefetch.spaceNeeded;
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Writes PM4 set commands to the specified command stream.  This is used for writing pipeline state registers whose
// values are not known until pipeline bind time.
uint32* PipelineChunkCs::WriteShCommandsDynamic(
    CmdStream*                      pCmdStream,
    uint32*                         pCmdSpace,
    const DynamicComputeShaderInfo& csInfo,
    gpusize                         launchDescGpuVa
    ) const
{
    if (launchDescGpuVa != 0uLL)
    {
        pCmdSpace = pCmdStream->WriteDynamicLaunchDesc(launchDescGpuVa, pCmdSpace);
    }

    auto dynamic = m_regs.dynamic; // "Dynamic" bind-time register state

    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    // TG_PER_CU: Sets the CS threadgroup limit per CU. Range is 1 to 15, 0 disables the limit.
    constexpr uint32 Gfx9MaxTgPerCu = 15;
    dynamic.computeResourceLimits.bits.TG_PER_CU = Min(csInfo.maxThreadGroupsPerCu, Gfx9MaxTgPerCu);
    if (csInfo.maxWavesPerCu > 0)
    {
        dynamic.computeResourceLimits.bits.WAVES_PER_SH = IsGfx10Plus(chipProps.gfxLevel) ?
            ComputePipeline::CalcMaxWavesPerSe(chipProps, csInfo.maxWavesPerCu) :
            ComputePipeline::CalcMaxWavesPerSh(chipProps, csInfo.maxWavesPerCu);
    }
#if PAL_AMDGPU_BUILD
    else if (IsGfx9(chipProps.gfxLevel) && (dynamic.computeResourceLimits.bits.WAVES_PER_SH == 0))
    {
        // GFX9 GPUs have a HW bug where a wave limit size of 0 does not correctly map to "no limit",
        // potentially breaking high-priority compute.
        dynamic.computeResourceLimits.bits.WAVES_PER_SH = m_device.GetMaxWavesPerSh(chipProps, true);
    }
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 628
    // CU_GROUP_COUNT: Sets the number of CS threadgroups to attempt to send to a single CU before moving to the next CU.
    // Range is 1 to 8, 0 disables the limit.
    constexpr uint32 Gfx9MaxCuGroupCount = 8;
    if (csInfo.tgScheduleCountPerCu > 0)
    {
        dynamic.computeResourceLimits.bits.CU_GROUP_COUNT = Min(csInfo.tgScheduleCountPerCu, Gfx9MaxCuGroupCount) - 1;
    }
#endif

    if (csInfo.ldsBytesPerTg > 0)
    {
        // Round to nearest multiple of the LDS granularity, then convert to the register value.
        // NOTE: Granularity for the LDS_SIZE field is 128, range is 0->128 which allocates 0 to 16K DWORDs.
        dynamic.computePgmRsrc2.bits.LDS_SIZE =
            Pow2Align((csInfo.ldsBytesPerTg / sizeof(uint32)), Gfx9LdsDwGranularity) >> Gfx9LdsDwGranularityShift;
    }

    if (launchDescGpuVa == 0uLL)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PGM_RSRC2,
                                                                dynamic.computePgmRsrc2.u32All,
                                                                pCmdSpace);
    }

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_RESOURCE_LIMITS,
                                                            dynamic.computeResourceLimits.u32All,
                                                            pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Writes PM4 SET commands to the specified command stream.  This is only expected to be called when the LOAD path is
// not in use and we need to use the SET path fallback.
uint32* PipelineChunkCs::WriteShCommandsSetPath(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace,
    bool       usingLaunchDesc
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();
    const RegisterInfo&      regInfo   = m_device.CmdUtil().GetRegInfo();

    pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmCOMPUTE_NUM_THREAD_X,
                                              mmCOMPUTE_NUM_THREAD_Z,
                                              ShaderCompute,
                                              &m_regs.computeNumThreadX,
                                              pCmdSpace);

    if (usingLaunchDesc == false)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PGM_LO,
                                                                m_regs.computePgmLo.u32All,
                                                                pCmdSpace);

        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PGM_RSRC1,
                                                                m_regs.computePgmRsrc1.u32All,
                                                                pCmdSpace);

        if (IsGfx10Plus(chipProps.gfxLevel))
        {
            pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(Gfx10Plus::mmCOMPUTE_PGM_RSRC3,
                                                                    m_regs.computePgmRsrc3.u32All,
                                                                    pCmdSpace);
        }

        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg,
                                                                m_regs.userDataInternalTable.u32All,
                                                                pCmdSpace);
    }

    if (chipProps.gfx9.supportSpp != 0)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(regInfo.mmComputeShaderChksum,
                                                                m_regs.computeShaderChksum.u32All,
                                                                pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
void PipelineChunkCs::UpdateComputePgmRsrsAfterLibraryLink(
    regCOMPUTE_PGM_RSRC1 rsrc1,
    regCOMPUTE_PGM_RSRC2 rsrc2,
    regCOMPUTE_PGM_RSRC3 rsrc3)
{
    m_regs.computePgmRsrc1         = rsrc1;
    m_regs.dynamic.computePgmRsrc2 = rsrc2;
    m_regs.computePgmRsrc3         = rsrc3;
}

// =====================================================================================================================
Result PipelineChunkCs::CreateLaunchDescriptor(
    void* pOut,
    bool  resolve)
{
    PAL_ASSERT(IsGfx10Plus(*m_device.Parent()) == true);

    DynamicCsLaunchDescLayout layout = { };
    layout.mmComputePgmLo = (mmCOMPUTE_PGM_LO - PERSISTENT_SPACE_START);
    layout.computePgmLo   = m_regs.computePgmLo;

    layout.mmComputePgmRsrc1 = (mmCOMPUTE_PGM_RSRC1 - PERSISTENT_SPACE_START);
    layout.computePgmRsrc1   = m_regs.computePgmRsrc1;
    layout.mmComputePgmRsrc2 = (mmCOMPUTE_PGM_RSRC2 - PERSISTENT_SPACE_START);
    layout.computePgmRsrc2   = m_regs.dynamic.computePgmRsrc2;

    layout.mmComputeUserData0 = (mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg - PERSISTENT_SPACE_START);
    layout.userDataInternalTable = m_regs.userDataInternalTable;

    layout.mmComputePgmRsrc3 = (Gfx10Plus::mmCOMPUTE_PGM_RSRC3 - PERSISTENT_SPACE_START);
    layout.computePgmRsrc3 = m_regs.computePgmRsrc3;

    // Resolve operation for cases where the launch descriptor is shared between pipelines
    if (resolve)
    {
        DynamicCsLaunchDescLayout* pIn = static_cast<DynamicCsLaunchDescLayout*>(pOut);

        layout.computePgmRsrc1.bits.VGPRS = Max(layout.computePgmRsrc1.bits.VGPRS, pIn->computePgmRsrc1.bits.VGPRS);
        layout.computePgmRsrc1.bits.SGPRS = Max(layout.computePgmRsrc1.bits.SGPRS, pIn->computePgmRsrc1.bits.SGPRS);

        layout.computePgmRsrc2.bits.LDS_SIZE =
            Max(layout.computePgmRsrc2.bits.LDS_SIZE, pIn->computePgmRsrc2.bits.LDS_SIZE);

        // All remaining bits in COMPUTE_PGM_RSRC2 register must be identical
        constexpr uint32 ComputePgmRsrc2BitMask =
            COMPUTE_PGM_RSRC2__EXCP_EN_MASK        |
            COMPUTE_PGM_RSRC2__EXCP_EN_MSB_MASK    |
            COMPUTE_PGM_RSRC2__SCRATCH_EN_MASK     |
            COMPUTE_PGM_RSRC2__TGID_X_EN_MASK      |
            COMPUTE_PGM_RSRC2__TGID_Y_EN_MASK      |
            COMPUTE_PGM_RSRC2__TGID_Z_EN_MASK      |
            COMPUTE_PGM_RSRC2__TG_SIZE_EN_MASK     |
            COMPUTE_PGM_RSRC2__TIDIG_COMP_CNT_MASK |
            COMPUTE_PGM_RSRC2__TRAP_PRESENT_MASK   |
            COMPUTE_PGM_RSRC2__USER_SGPR_MASK;

        const uint32 computePgmRsrc2ValidBitsIn = (pIn->computePgmRsrc2.u32All & ComputePgmRsrc2BitMask);
        const uint32 computePgmRsrc2ValidBitsOut = (layout.computePgmRsrc2.u32All & ComputePgmRsrc2BitMask);
        PAL_ASSERT(computePgmRsrc2ValidBitsIn == computePgmRsrc2ValidBitsOut);

        layout.computePgmRsrc3.bits.SHARED_VGPR_CNT =
            Max(layout.computePgmRsrc3.bits.SHARED_VGPR_CNT, pIn->computePgmRsrc3.bits.SHARED_VGPR_CNT);
    }

    memcpy(pOut, &layout, sizeof(layout));

    return Result::Success;
}

// =====================================================================================================================
LibraryChunkCs::LibraryChunkCs(
    const Device&    device)
    :
    PipelineChunkCs(device, nullptr, nullptr),
    m_device(device)
{
    memset(&m_regs, 0, sizeof(m_regs));
}

// =====================================================================================================================
// Late initialization for this Compute Library chunk.
// Responsible for fetching register values from the library binary and
// determining the values of other registers.
void LibraryChunkCs::LateInit(
    const AbiReader&            abiReader,
    const RegisterVector&       registers,
    uint32                      wavefrontSize,
    ShaderLibraryFunctionInfo*  pFunctionList,
    uint32                      funcCount,
    PipelineUploader*           pUploader)
{
    const auto&              cmdUtil   = m_device.CmdUtil();
    const auto&              regInfo   = cmdUtil.GetRegInfo();
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    m_regs.computePgmRsrc1.u32All         = registers.At(mmCOMPUTE_PGM_RSRC1);
    m_regs.dynamic.computePgmRsrc2.u32All = registers.At(mmCOMPUTE_PGM_RSRC2);

    if (IsGfx10Plus(chipProps.gfxLevel))
    {
        m_regs.computePgmRsrc3.u32All = registers.At(Gfx10Plus::mmCOMPUTE_PGM_RSRC3);
    }

    // Double check with Rob - Is this the correct way to calculate wavesPerGroup?
    const uint32 threadsPerGroup = 0;
    const uint32 wavesPerGroup   = RoundUpQuotient(threadsPerGroup, wavefrontSize);

    // SIMD_DEST_CNTL: Controls which SIMDs thread groups get scheduled on.  If the number of
    // waves-per-TG is a multiple of 4, this should be 1, otherwise 0.
    m_regs.dynamic.computeResourceLimits.bits.SIMD_DEST_CNTL = ((wavesPerGroup % 4) == 0) ? 1 : 0;

    // Force even distribution on all SIMDs in CU for workgroup size is 64
    // This has shown some good improvements if #CU per SE not a multiple of 4
    if (((chipProps.gfx9.numShaderArrays * chipProps.gfx9.numCuPerSh) & 0x3) && (wavesPerGroup == 1))
    {
        m_regs.dynamic.computeResourceLimits.bits.FORCE_SIMD_DIST = 1;
    }

    if (m_device.Parent()->LegacyHwsTrapHandlerPresent() && (chipProps.gfxLevel == GfxIpLevel::GfxIp9))
    {

        // If the legacy HWS's trap handler is present, compute shaders must always set the TRAP_PRESENT
        // flag.

        // TODO: Handle the case where the client enabled a trap handler and the hardware scheduler's trap handler
        // is already active!
        PAL_ASSERT(m_regs.dynamic.computePgmRsrc2.bits.TRAP_PRESENT == 0);
        m_regs.dynamic.computePgmRsrc2.bits.TRAP_PRESENT = 1;
    }

    const auto& settings = m_device.Settings();

    // LOCK_THRESHOLD: Sets per-SH low threshold for locking.  Set in units of 4, 0 disables locking.
    // LOCK_THRESHOLD's maximum value: (6 bits), in units of 4, so it is max of 252.
    constexpr uint32 Gfx9MaxLockThreshold = 252;
    PAL_ASSERT(settings.csLockThreshold <= Gfx9MaxLockThreshold);
    m_regs.dynamic.computeResourceLimits.bits.LOCK_THRESHOLD = Min((settings.csLockThreshold >> 2),
                                                                   (Gfx9MaxLockThreshold >> 2));

    // libraries probably don't need any prefetching,
    // i.e., no need for " cmdUtil.BuildPipelinePrefetchPm4(*pUploader, &m_prefetch)" here.
    //
    // Might be nice to do so, but it adds up complexity, in that we would have to maintain a list
    // of every linked library with each pipeline object and pretech the main pipeline with each library...

    ShaderLibrary::GetFunctionGpuVirtAddrs(*pUploader, pFunctionList, funcCount);
}

} // Gfx9
} // Pal
