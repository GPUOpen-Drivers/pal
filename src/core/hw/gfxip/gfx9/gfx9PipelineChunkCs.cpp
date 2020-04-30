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

// Base count of SH registers which are loaded using LOAD_SH_REG_INDEX when binding to a universal command buffer.
constexpr uint32 BaseLoadedShRegCount =
    1 + // mmCOMPUTE_PGM_LO
    1 + // mmCOMPUTE_PGM_HI
    1 + // mmCOMPUTE_PGM_RSRC1
    0 + // mmCOMPUTE_PGM_RSRC2 is not included because it partially depends on bind-time state
    0 + // mmCOMPUTE_PGM_RSRC3 is not included because it is not present on all HW
    0 + // mmCOMPUTE_RESOURCE_LIMITS is not included because it partially depends on bind-time state
    1 + // mmCOMPUTE_NUM_THREAD_X
    1 + // mmCOMPUTE_NUM_THREAD_Y
    1 + // mmCOMPUTE_NUM_THREAD_Z
    1 + // mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg
    0;  // mmCOMPUTE_SHADER_CHKSUM is not included because it is not present on all HW

// =====================================================================================================================
PipelineChunkCs::PipelineChunkCs(
    const Device&    device,
    ShaderStageInfo* pStageInfo,
    PerfDataInfo*    pPerfDataInfo)
    :
    m_device(device),
    m_pCsPerfDataInfo(pPerfDataInfo),
    m_pStageInfo(pStageInfo)
{
    memset(&m_regs, 0, sizeof(m_regs));
    memset(&m_loadPath, 0, sizeof(m_loadPath));
    memset(&m_prefetch, 0, sizeof(m_prefetch));
    if(m_pStageInfo != nullptr)
    {
        m_pStageInfo->stageId = Abi::HardwareStage::Cs;
    }
}

// =====================================================================================================================
// Early initialization for this pipeline chunk when used in a compute pipeline. Responsible for determining the number
// of SH registers to be loaded using LOAD_SH_REG_INDEX.
uint32 PipelineChunkCs::EarlyInit()
{
    const Gfx9PalSettings&   settings  = m_device.Settings();
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    uint32 count = 0;

    if (settings.enableLoadIndexForObjectBinds)
    {
        // Add one register if the GPU supports SPP.
        count += (BaseLoadedShRegCount + ((chipProps.gfx9.supportSpp == 1) ? 1 : 0));

        if (IsGfx10(chipProps.gfxLevel))
        {
            count += 1; //  mmCOMPUTE_PGM_RSRC3
        }
    }

    return count;
}

// =====================================================================================================================
// Early initialization for this pipeline chunk when used in a graphics pipeline. Responsible for determining the number
// of SH registers to be loaded using LOAD_SH_REG_INDEX.
void PipelineChunkCs::EarlyInit(
    GraphicsPipelineLoadInfo* pInfo)
{
    PAL_ASSERT(pInfo != nullptr);
    pInfo->loadedShRegCount += EarlyInit();
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.  Also uploads register state into GPU memory.
template <typename CsPipelineUploader>
void PipelineChunkCs::LateInit(
    const AbiReader&                 abiReader,
    const RegisterVector&            registers,
    uint32                           wavefrontSize,
    ComputePipelineIndirectFuncInfo* pIndirectFuncList,
    uint32                           indirectFuncCount,
    uint32*                          pThreadsPerTgX,
    uint32*                          pThreadsPerTgY,
    uint32*                          pThreadsPerTgZ,
    CsPipelineUploader*              pUploader)
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
        m_regs.computePgmHi.bits.DATA = Get256BAddrHi(symbol.gpuVirtAddr);
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

    if (IsGfx10(chipProps.gfxLevel))
    {
        m_regs.computePgmRsrc3.u32All = registers.At(Gfx10::mmCOMPUTE_PGM_RSRC3);

#if PAL_ENABLE_PRINTS_ASSERTS
        m_device.AssertUserAccumRegsDisabled(registers, Gfx10::mmCOMPUTE_USER_ACCUM_0);
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

    if (pUploader->EnableLoadIndexPath())
    {
        m_loadPath.gpuVirtAddr = pUploader->ShRegGpuVirtAddr();
        m_loadPath.count       = pUploader->ShRegisterCount();

        pUploader->AddShReg(mmCOMPUTE_PGM_LO, m_regs.computePgmLo);
        pUploader->AddShReg(mmCOMPUTE_PGM_HI, m_regs.computePgmHi);

        pUploader->AddShReg((mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg), m_regs.userDataInternalTable);

        pUploader->AddShReg(mmCOMPUTE_PGM_RSRC1,    m_regs.computePgmRsrc1);
        pUploader->AddShReg(mmCOMPUTE_NUM_THREAD_X, m_regs.computeNumThreadX);
        pUploader->AddShReg(mmCOMPUTE_NUM_THREAD_Y, m_regs.computeNumThreadY);
        pUploader->AddShReg(mmCOMPUTE_NUM_THREAD_Z, m_regs.computeNumThreadZ);

        if (IsGfx10(chipProps.gfxLevel))
        {
            pUploader->AddShReg(Gfx10::mmCOMPUTE_PGM_RSRC3, m_regs.computePgmRsrc3);
        }

        if (chipProps.gfx9.supportSpp != 0)
        {
            pUploader->AddShReg(regInfo.mmComputeShaderChksum, m_regs.computeShaderChksum);
        }
    }

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

    ComputePipeline::GetFunctionGpuVirtAddrs(*pUploader, pIndirectFuncList, indirectFuncCount);
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

#if PAL_ENABLE_PRINTS_ASSERTS
    // Indirect user-data table(s) are not supported on compute pipelines, so just assert that the table addresses
    // are unmapped.
    if (metadata.pipeline.hasEntry.indirectUserDataTableAddresses != 0)
    {
        constexpr uint32 MetadataIndirectTableAddressCount =
            (sizeof(metadata.pipeline.indirectUserDataTableAddresses) /
             sizeof(metadata.pipeline.indirectUserDataTableAddresses[0]));
        constexpr uint32 DummyAddresses[MetadataIndirectTableAddressCount] = { 0 };

        PAL_ASSERT_MSG(0 == memcmp(&metadata.pipeline.indirectUserDataTableAddresses[0],
                                   &DummyAddresses[0], sizeof(DummyAddresses)),
                       "Indirect user-data tables are not supported for Compute Pipelines!");
    }
#endif

    // NOTE: We skip the stream-out table address here because it is not used by compute pipelines.

    if (metadata.pipeline.hasEntry.spillThreshold != 0)
    {
        pSignature->spillThreshold = static_cast<uint16>(metadata.pipeline.spillThreshold);
    }

    if (metadata.pipeline.hasEntry.userDataLimit != 0)
    {
        pSignature->userDataLimit = static_cast<uint16>(metadata.pipeline.userDataLimit);
    }

    // We don't bother checking the wavefront size for pre-Gfx10 GPU's since it is implicitly 64 before Gfx10. Any ELF
    // which doesn't specify a wavefront size is assumed to use 64, even on Gfx10 and newer.
    if (IsGfx10(chipProps.gfxLevel))
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 495
        // Older ABI versions encoded wave32 vs. wave64 using the CS_W32_EN field of COMPUTE_DISPATCH_INITIATOR. Fall
        // back to that encoding if the CS metadata does not specify a wavefront size.
        regCOMPUTE_DISPATCH_INITIATOR dispatchInitiator = { };
#endif

        const auto& csMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Cs)];
        if (csMetadata.hasEntry.wavefrontSize != 0)
        {
            PAL_ASSERT((csMetadata.wavefrontSize == 64) || (csMetadata.wavefrontSize == 32));
            pSignature->flags.isWave32 = (csMetadata.wavefrontSize == 32);
        }
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 495
        else if (registers.HasEntry(mmCOMPUTE_DISPATCH_INITIATOR, &dispatchInitiator.u32All))
        {
            pSignature->flags.isWave32 = dispatchInitiator.gfx10.CS_W32_EN;
        }
#endif
    }
}

// Instantiate template versions for the linker.
template
void PipelineChunkCs::LateInit<ComputePipelineUploader>(
    const AbiReader&                 abiReader,
    const RegisterVector&            registers,
    uint32                           wavefrontSize,
    ComputePipelineIndirectFuncInfo* pIndirectFuncList,
    uint32                           indirectFuncCount,
    uint32*                          pThreadsPerTgX,
    uint32*                          pThreadsPerTgY,
    uint32*                          pThreadsPerTgZ,
    ComputePipelineUploader*         pUploader);

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkCs::WriteShCommands(
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
        const CmdUtil& cmdUtil = m_device.CmdUtil();
        pCmdSpace += cmdUtil.BuildLoadShRegsIndex(m_loadPath.gpuVirtAddr, m_loadPath.count, ShaderCompute, pCmdSpace);
    }

    auto dynamic = m_regs.dynamic; // "Dynamic" bind-time register state

    // TG_PER_CU: Sets the CS threadgroup limit per CU. Range is 1 to 15, 0 disables the limit.
    constexpr uint32 Gfx9MaxTgPerCu = 15;
    dynamic.computeResourceLimits.bits.TG_PER_CU = Min(csInfo.maxThreadGroupsPerCu, Gfx9MaxTgPerCu);
    if (csInfo.maxWavesPerCu > 0)
    {
        dynamic.computeResourceLimits.bits.WAVES_PER_SH =
            Gfx9::ComputePipeline::CalcMaxWavesPerSh(m_device.Parent()->ChipProperties(), csInfo.maxWavesPerCu);
    }

    if (csInfo.ldsBytesPerTg > 0)
    {
        // Round to nearest multiple of the LDS granularity, then convert to the register value.
        // NOTE: Granularity for the LDS_SIZE field is 128, range is 0->128 which allocates 0 to 16K DWORDs.
        dynamic.computePgmRsrc2.bits.LDS_SIZE =
            Pow2Align((csInfo.ldsBytesPerTg / sizeof(uint32)), Gfx9LdsDwGranularity) >> Gfx9LdsDwGranularityShift;
    }

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PGM_RSRC2,
                                                            dynamic.computePgmRsrc2.u32All,
                                                            pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_RESOURCE_LIMITS,
                                                            dynamic.computeResourceLimits.u32All,
                                                            pCmdSpace);

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
// Writes PM4 SET commands to the specified command stream.  This is only expected to be called when the LOAD path is
// not in use and we need to use the SET path fallback.
uint32* PipelineChunkCs::WriteShCommandsSetPath(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();
    const RegisterInfo&      regInfo   = m_device.CmdUtil().GetRegInfo();

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

    if (chipProps.gfx9.supportSpp != 0)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(regInfo.mmComputeShaderChksum,
                                                                m_regs.computeShaderChksum.u32All,
                                                                pCmdSpace);
    }

    if (IsGfx10(chipProps.gfxLevel))
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(Gfx10::mmCOMPUTE_PGM_RSRC3,
                                                                m_regs.computePgmRsrc3.u32All,
                                                                pCmdSpace);
    }

    return pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg,
                                                       m_regs.userDataInternalTable.u32All,
                                                       pCmdSpace);
}

// =====================================================================================================================
void PipelineChunkCs::UpdateComputePgmRsrsAfterLibraryLink(
    regCOMPUTE_PGM_RSRC1 rsrc1,
    regCOMPUTE_PGM_RSRC2 rsrc2,
    regCOMPUTE_PGM_RSRC3 rsrc3)
{
    // If this pipeline will be linked to ShaderLibrary,
    // we need to delay the CP load packet path till the linking is done
    //
    // At this point, there is no longer valid to go down the buildloadshregsindex path in "BuildLoadShRegsIndex"
    // This can be achieved by setting the m_loadPath.count to 0.
    m_loadPath.count = 0;

    m_regs.computePgmRsrc1         = rsrc1;
    m_regs.dynamic.computePgmRsrc2 = rsrc2;
    m_regs.computePgmRsrc3         = rsrc3;
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
// Also uploads register state into GPU memory.
template <typename ShaderLibraryUploader>
void LibraryChunkCs::LateInit(
    const AbiReader&                 abiReader,
    const RegisterVector&            registers,
    uint32                           wavefrontSize,
    ShaderLibraryFunctionInfo*       pFunctionList,
    uint32                           funcCount,
    ShaderLibraryUploader*           pUploader)
{
    const auto&              cmdUtil   = m_device.CmdUtil();
    const auto&              regInfo   = cmdUtil.GetRegInfo();
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    m_regs.computePgmRsrc1.u32All         = registers.At(mmCOMPUTE_PGM_RSRC1);
    m_regs.dynamic.computePgmRsrc2.u32All = registers.At(mmCOMPUTE_PGM_RSRC2);

    if (IsGfx10(chipProps.gfxLevel))
    {
        m_regs.computePgmRsrc3.u32All = registers.At(Gfx10::mmCOMPUTE_PGM_RSRC3);
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

template
void LibraryChunkCs::LateInit<ShaderLibraryUploader>(
    const AbiReader&                 abiReader,
    const RegisterVector&            registers,
    uint32                           wavefrontSize,
    ShaderLibraryFunctionInfo*       pFunctionList,
    uint32                           funcCount,
    ShaderLibraryUploader*           pUploader);

} // Gfx9
} // Pal
