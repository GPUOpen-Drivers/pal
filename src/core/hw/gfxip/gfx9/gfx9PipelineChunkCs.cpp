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

#include "core/platform.h"
#include "core/hw/gfxip/computePipeline.h"
#include "core/hw/gfxip/gfx9/gfx9ComputeShaderLibrary.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9ComputePipeline.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkCs.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkGs.h"
#include "core/hw/gfxip/gfx9/gfx9AbiToPipelineRegisters.h"
#include "palHsaAbiMetadata.h"

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
    m_prefetchAddr(0),
    m_prefetchSize(0),
    m_pCsPerfDataInfo(pPerfDataInfo),
    m_pStageInfo(pStageInfo)
{
    if (m_pStageInfo != nullptr)
    {
        m_pStageInfo->stageId = Abi::HardwareStage::Cs;
    }
    m_regs.userDataInternalTable.u32All = InvalidUserDataInternalTable;
}

// =====================================================================================================================
// Perform LateInit after InitRegisters.
void PipelineChunkCs::DoLateInit(
    DispatchDims*     pThreadsPerTg,
    PipelineUploader* pUploader)
{
    GpuSymbol symbol = { };
    if (pUploader != nullptr)
    {
        if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::CsMainEntry, &symbol) == Result::Success)
        {
            m_pStageInfo->codeLength = static_cast<size_t>(symbol.size);
            PAL_ASSERT(IsPow2Aligned(symbol.gpuVirtAddr, 256u));

            m_regs.computePgmLo.bits.DATA = Get256BAddrLo(symbol.gpuVirtAddr);
        }

        if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::CsShdrIntrlTblPtr, &symbol) == Result::Success)
        {
            m_regs.userDataInternalTable.bits.DATA = LowPart(symbol.gpuVirtAddr);
        }

        if (m_device.CoreSettings().pipelinePrefetchEnable)
        {
            m_prefetchAddr = pUploader->PrefetchAddr();
            m_prefetchSize = pUploader->PrefetchSize();
        }
    }

    pThreadsPerTg->x = m_regs.computeNumThreadX.bits.NUM_THREAD_FULL;
    pThreadsPerTg->y = m_regs.computeNumThreadY.bits.NUM_THREAD_FULL;
    pThreadsPerTg->z = m_regs.computeNumThreadZ.bits.NUM_THREAD_FULL;
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.
void PipelineChunkCs::LateInit(
    const Util::PalAbi::CodeObjectMetadata& metadata,
    uint32                                  wavefrontSize,
    DispatchDims*                           pThreadsPerTg,
#if PAL_BUILD_GFX11
    DispatchInterleaveSize                  interleaveSize,
#endif
    PipelineUploader*                       pUploader)
{
    InitRegisters(metadata,
#if PAL_BUILD_GFX11
                  interleaveSize,
#endif
                  wavefrontSize);

    DoLateInit(pThreadsPerTg, pUploader);
}

// =====================================================================================================================
// Late initialization for Hsa pipeline chunk.
void PipelineChunkCs::LateInit(
    const RegisterVector&   registers,
    uint32                  wavefrontSize,
    DispatchDims*           pThreadsPerTg,
#if PAL_BUILD_GFX11
    DispatchInterleaveSize  interleaveSize,
#endif
    PipelineUploader*       pUploader)
{
    InitRegisters(
        registers,
#if PAL_BUILD_GFX11
        interleaveSize,
#endif
        wavefrontSize);

    DoLateInit(pThreadsPerTg, pUploader);
}

// =====================================================================================================================
void PipelineChunkCs::InitGpuAddrFromMesh(
    const AbiReader&       abiReader,
    const PipelineChunkGs& chunkGs)
{
    const Elf::SymbolTableEntry* pCsMainEntry = abiReader.GetPipelineSymbol(Abi::PipelineSymbolType::CsMainEntry);
    const Elf::SymbolTableEntry* pGsMainEntry = abiReader.GetPipelineSymbol(Abi::PipelineSymbolType::GsMainEntry);
    if ((pCsMainEntry != nullptr) && (pGsMainEntry != nullptr))
    {
        m_pStageInfo->codeLength = static_cast<size_t>(pCsMainEntry->st_size);
        gpusize gsGpuVa = chunkGs.EsProgramGpuVa();
        gpusize csGpuVa = gsGpuVa + pCsMainEntry->st_value - pGsMainEntry->st_value;
        PAL_ASSERT(IsPow2Aligned(gsGpuVa, 256u));
        PAL_ASSERT(IsPow2Aligned(csGpuVa, 256u));
        m_regs.computePgmLo.bits.DATA = Get256BAddrLo(csGpuVa);
    }

    const Elf::SymbolTableEntry* pCsInternalTable =
        abiReader.GetPipelineSymbol(Abi::PipelineSymbolType::CsShdrIntrlTblPtr);
    const Elf::SymbolTableEntry* pGsInternalTable =
        abiReader.GetPipelineSymbol(Abi::PipelineSymbolType::GsShdrIntrlTblPtr);
    if ((pCsInternalTable != nullptr) && (pGsInternalTable != nullptr))
    {
        uint32 gsTableLoVa = chunkGs.UserDataInternalTableLoVa();
        uint32 csTableLoVa = gsTableLoVa + pCsInternalTable->st_value - pGsInternalTable->st_value;
        m_regs.userDataInternalTable.bits.DATA = csTableLoVa;
    }
}

// =====================================================================================================================
// Helper method which initializes registers from the metadata extraced from an ELF metadata blob.
void PipelineChunkCs::InitRegisters(
    const PalAbi::CodeObjectMetadata& metadata,
#if PAL_BUILD_GFX11
    DispatchInterleaveSize            interleaveSize,
#endif
    uint32                            wavefrontSize)
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();
    const GfxIpLevel         gfxLevel  = chipProps.gfxLevel;

    m_regs.computePgmRsrc1.u32All         = AbiRegisters::ComputePgmRsrc1(metadata, gfxLevel);
    m_regs.dynamic.computePgmRsrc2.u32All = AbiRegisters::ComputePgmRsrc2(metadata, m_device);

    // These are optional for shader libraries.
    {
        m_regs.computeNumThreadX.u32All = AbiRegisters::ComputeNumThreadX(metadata);
        m_regs.computeNumThreadY.u32All = AbiRegisters::ComputeNumThreadY(metadata);
        m_regs.computeNumThreadZ.u32All = AbiRegisters::ComputeNumThreadZ(metadata);
    }

    m_regs.computePgmRsrc3.u32All = AbiRegisters::ComputePgmRsrc3(metadata, m_device, m_pStageInfo->codeLength);
    m_regs.computeShaderChksum.u32All = AbiRegisters::ComputeShaderChkSum(metadata, m_device);
    m_regs.dynamic.computeResourceLimits.u32All =
        AbiRegisters::ComputeResourceLimits(metadata, m_device, wavefrontSize);

#if PAL_BUILD_GFX11
    m_regs.computeDispatchInterleave = AbiRegisters::ComputeDispatchInterleave(m_device, interleaveSize);
#endif

}

// =====================================================================================================================
// Helper method which initializes registers from the register vector extraced from an ELF metadata blob.
void PipelineChunkCs::InitRegisters(
    const RegisterVector&  registers,
#if PAL_BUILD_GFX11
    DispatchInterleaveSize interleaveSize,
#endif
    uint32                 wavefrontSize)
{
    const RegisterInfo&      regInfo   = m_device.CmdUtil().GetRegInfo();
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    m_regs.computePgmRsrc1.u32All         = registers.At(mmCOMPUTE_PGM_RSRC1);
    m_regs.dynamic.computePgmRsrc2.u32All = registers.At(mmCOMPUTE_PGM_RSRC2);

    // These are optional for shader libraries.
    registers.HasEntry(mmCOMPUTE_NUM_THREAD_X, &m_regs.computeNumThreadX.u32All);
    registers.HasEntry(mmCOMPUTE_NUM_THREAD_Y, &m_regs.computeNumThreadY.u32All);
    registers.HasEntry(mmCOMPUTE_NUM_THREAD_Z, &m_regs.computeNumThreadZ.u32All);

    if (IsGfx10Plus(chipProps.gfxLevel))
    {
        m_regs.computePgmRsrc3.u32All = registers.At(Gfx10Plus::mmCOMPUTE_PGM_RSRC3);

#if  PAL_BUILD_GFX11
        if (IsGfx104Plus(chipProps.gfxLevel))
        {
            m_regs.computePgmRsrc3.gfx104Plus.INST_PREF_SIZE =
                m_device.GetShaderPrefetchSize(m_pStageInfo->codeLength);
        }
#endif

#if PAL_BUILD_GFX11
        // PWS+ only support pre-shader waits if the IMAGE_OP bit is set. Theoretically we only set it for shaders that
        // do an image operation. However that would mean that our use of the pre-shader PWS+ wait is dependent on us
        // only waiting on image resources, which we don't know in our interface. For now always set the IMAGE_OP bit
        // for corresponding shaders, making the pre-shader waits global.
        if (IsGfx11(chipProps.gfxLevel))
        {
            m_regs.computePgmRsrc3.gfx11.IMAGE_OP = 1;
        }
#endif
    }

    if (chipProps.gfx9.supportSpp == 1)
    {
        PAL_ASSERT(regInfo.mmComputeShaderChksum != 0);
        registers.HasEntry(regInfo.mmComputeShaderChksum, &m_regs.computeShaderChksum.u32All);
    }

    registers.HasEntry(mmCOMPUTE_RESOURCE_LIMITS, &m_regs.dynamic.computeResourceLimits.u32All);

    const uint32 threadsPerGroup = (m_regs.computeNumThreadX.bits.NUM_THREAD_FULL *
                                    m_regs.computeNumThreadY.bits.NUM_THREAD_FULL *
                                    m_regs.computeNumThreadZ.bits.NUM_THREAD_FULL);
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

#if PAL_BUILD_GFX11
    if (settings.waForceLockThresholdZero)
    {
        m_regs.dynamic.computeResourceLimits.bits.LOCK_THRESHOLD = 0;
    }
    else
#endif
    {
        m_regs.dynamic.computeResourceLimits.bits.LOCK_THRESHOLD = Min((settings.csLockThreshold >> 2),
                                                                       (Gfx9MaxLockThreshold >> 2));
    }

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

#if PAL_BUILD_GFX11
    if (IsGfx11(chipProps.gfxLevel))
    {
        m_regs.computeDispatchInterleave = AbiRegisters::ComputeDispatchInterleave(m_device, interleaveSize);
    }
#endif
}

// =====================================================================================================================
// Initializes the signature of a compute shader using a pipeline ELF.
// NOTE: Must be called before LateInit!
void PipelineChunkCs::SetupSignatureFromElf(
    ComputeShaderSignature*           pSignature,
    const HsaAbi::CodeObjectMetadata& metadata,
    const RegisterVector&             registers)
{
    SetupSignatureFromRegisters(pSignature, registers);

    // The HSA ABI doesn't use PAL's user-data system at all.
    pSignature->spillThreshold = 0xFFFF;
    pSignature->userDataLimit  = 0;

    // Compute a hash of the user data mapping
    pSignature->userDataHash = ComputeUserDataHash(&pSignature->stage);

    // Only gfx10+ can run in wave32 mode.
    pSignature->flags.isWave32 = IsGfx10Plus(*m_device.Parent()) && (metadata.WavefrontSize() == 32);
}

// =====================================================================================================================
// Initializes the signature of a compute shader using a pipeline ELF.
// NOTE: Must be called before LateInit!
void PipelineChunkCs::SetupSignatureFromElf(
    ComputeShaderSignature*           pSignature,
    const PalAbi::CodeObjectMetadata& metadata)
{
    SetupSignatureFromMetadata(pSignature, metadata);

    if (metadata.pipeline.hasEntry.spillThreshold != 0)
    {
        pSignature->spillThreshold = static_cast<uint16>(metadata.pipeline.spillThreshold);
    }

    if (metadata.pipeline.hasEntry.userDataLimit != 0)
    {
        pSignature->userDataLimit = static_cast<uint16>(metadata.pipeline.userDataLimit);
    }

    // Compute a hash of the user data mapping
    pSignature->userDataHash = ComputeUserDataHash(&pSignature->stage);

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
void PipelineChunkCs::SetupSignatureFromMetadata(
    ComputeShaderSignature*           pSignature,
    const PalAbi::CodeObjectMetadata& metadata)
{
    const Util::PalAbi::HardwareStageMetadata& hwCs = metadata.pipeline.hardwareStage[uint32(Abi::HardwareStage::Cs)];
    PAL_ASSERT(hwCs.userSgprs <= 16);

    const auto& chipProps = m_device.Parent()->ChipProperties();

    for (uint16 offset = 0; offset < 16; ++offset)
    {
        uint32 value = 0;
        if (hwCs.hasEntry.userDataRegMap)
        {
            value = hwCs.userDataRegMap[offset];

            // value is not mapped, move on to the next entry
            if (value == uint32(Abi::UserDataMapping::NotMapped))
            {
                continue;
            }

            if (value < MaxUserDataEntries)
            {
                if (pSignature->stage.firstUserSgprRegAddr == UserDataNotMapped)
                {
                    pSignature->stage.firstUserSgprRegAddr = offset + mmCOMPUTE_USER_DATA_0;
                }

                const uint8 userSgprId = static_cast<uint8>(
                    offset + mmCOMPUTE_USER_DATA_0 - pSignature->stage.firstUserSgprRegAddr);

                pSignature->stage.mappedEntry[userSgprId] = static_cast<uint8>(value);
                pSignature->stage.userSgprCount = Max<uint8>(userSgprId + 1, pSignature->stage.userSgprCount);
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::GlobalTable))
            {
                PAL_ASSERT(offset == (InternalTblStartReg));
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::PerShaderTable))
            {
                PAL_ASSERT(offset == (ConstBufTblStartReg));
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::SpillTable))
            {
                pSignature->stage.spillTableRegAddr = static_cast<uint16>(offset + mmCOMPUTE_USER_DATA_0);
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::Workgroup))
            {
                pSignature->numWorkGroupsRegAddr = static_cast<uint16>(offset + mmCOMPUTE_USER_DATA_0);
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::MeshTaskDispatchDims))
            {
                pSignature->taskDispatchDimsAddr = static_cast<uint16_t>(offset + mmCOMPUTE_USER_DATA_0);
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::MeshTaskRingIndex))
            {
                pSignature->taskRingIndexAddr = static_cast<uint16>(offset + mmCOMPUTE_USER_DATA_0);
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::TaskDispatchIndex))
            {
                pSignature->dispatchIndexRegAddr = static_cast<uint16>(offset + mmCOMPUTE_USER_DATA_0);
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::MeshPipeStatsBuf))
            {
                pSignature->taskPipeStatsBufRegAddr = offset + mmCOMPUTE_USER_DATA_0;
            }
            else if (value == static_cast<uint32>(Abi::UserDataMapping::PerShaderPerfData))
            {
                m_pCsPerfDataInfo->regOffset = offset + mmCOMPUTE_USER_DATA_0;
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
}

// =====================================================================================================================
void PipelineChunkCs::SetupSignatureFromRegisters(
    ComputeShaderSignature*   pSignature,
    const RegisterVector&     registers)
{
    const auto& chipProps = m_device.Parent()->ChipProperties();

    for (uint16 offset = mmCOMPUTE_USER_DATA_0; offset <= mmCOMPUTE_USER_DATA_15; ++offset)
    {
        uint32 value = 0;
        if (registers.HasEntry(offset, &value))
        {
            if (value < MaxUserDataEntries)
            {
                if (pSignature->stage.firstUserSgprRegAddr == UserDataNotMapped)
                {
                    pSignature->stage.firstUserSgprRegAddr = offset;
                }
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
}

// =====================================================================================================================
// Helper to write DynamicLaunchDesc registers and update dynamic register state. This function must be called
// immediately before any dynamic register writes. Returns the next unused DWORD in pCmdSpace.
uint32* PipelineChunkCs::UpdateDynamicRegInfo(
    CmdStream*                      pCmdStream,
    uint32*                         pCmdSpace,
    HwRegInfo::Dynamic*             pDynamicRegs,
    const DynamicComputeShaderInfo& csInfo
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    // TG_PER_CU: Sets the CS threadgroup limit per CU. Range is 1 to 15, 0 disables the limit.
    constexpr uint32 Gfx9MaxTgPerCu = 15;
    pDynamicRegs->computeResourceLimits.bits.TG_PER_CU = Min(csInfo.maxThreadGroupsPerCu, Gfx9MaxTgPerCu);
    if (csInfo.maxWavesPerCu > 0)
    {
        pDynamicRegs->computeResourceLimits.bits.WAVES_PER_SH = IsGfx10Plus(chipProps.gfxLevel) ?
                            ComputePipeline::CalcMaxWavesPerSe(chipProps, csInfo.maxWavesPerCu) :
                            ComputePipeline::CalcMaxWavesPerSh(chipProps, csInfo.maxWavesPerCu);
    }
#if PAL_AMDGPU_BUILD
    else if (IsGfx9(chipProps.gfxLevel) && (pDynamicRegs->computeResourceLimits.bits.WAVES_PER_SH == 0))
    {
        // GFX9 GPUs have a HW bug where a wave limit size of 0 does not correctly map to "no limit",
        // potentially breaking high-priority compute.
        pDynamicRegs->computeResourceLimits.bits.WAVES_PER_SH = m_device.GetMaxWavesPerSh(chipProps, true);
    }
#endif

    // CU_GROUP_COUNT: Sets the number of CS threadgroups to attempt to send to a single CU before moving to the next CU.
    // Range is 1 to 8, 0 disables the limit.
    constexpr uint32 Gfx9MaxCuGroupCount = 8;
    if (csInfo.tgScheduleCountPerCu > 0)
    {
        pDynamicRegs->computeResourceLimits.bits.CU_GROUP_COUNT =
            Min(csInfo.tgScheduleCountPerCu, Gfx9MaxCuGroupCount) - 1;
    }

    if (csInfo.ldsBytesPerTg > 0)
    {
        // Round to nearest multiple of the LDS granularity, then convert to the register value.
        // NOTE: Granularity for the LDS_SIZE field is 128, range is 0->128 which allocates 0 to 16K DWORDs.
        pDynamicRegs->computePgmRsrc2.bits.LDS_SIZE =
            Pow2Align((csInfo.ldsBytesPerTg / sizeof(uint32)), Gfx9LdsDwGranularity) >> Gfx9LdsDwGranularityShift;
    }

    return pCmdSpace;
}

#if PAL_BUILD_GFX11
// =====================================================================================================================
// Accumulates a set of registers into an array of packed register pairs, analagous to WriteShCommandsSetPath().
void PipelineChunkCs::AccumulateShCommandsDynamic(
    PackedRegisterPair* pRegPairs,
    uint32*             pNumRegs,
    HwRegInfo::Dynamic  dynamicRegs
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    const uint32 startingIdx = *pNumRegs;
#endif

    SetOneShRegValPairPacked(pRegPairs, pNumRegs, mmCOMPUTE_PGM_RSRC2, dynamicRegs.computePgmRsrc2.u32All);

    SetOneShRegValPairPacked(pRegPairs,
                             pNumRegs,
                             mmCOMPUTE_RESOURCE_LIMITS,
                             dynamicRegs.computeResourceLimits.u32All);

#if PAL_ENABLE_PRINTS_ASSERTS
    PAL_ASSERT(InRange(*pNumRegs, startingIdx, startingIdx + NumDynamicRegs));
#endif
}

// =====================================================================================================================
// Accumulates a set of registers into an array of packed register pairs, analagous to WriteShCommandsDynamic().
void PipelineChunkCs::AccumulateShCommandsSetPath(
    PackedRegisterPair* pRegPairs,
    uint32*             pNumRegs
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    const uint32 startingIdx = *pNumRegs;
#endif

    SetSeqShRegValPairPacked(pRegPairs,
                             pNumRegs,
                             mmCOMPUTE_NUM_THREAD_X,
                             mmCOMPUTE_NUM_THREAD_Z,
                             &m_regs.computeNumThreadX);

    SetOneShRegValPairPacked(pRegPairs,
                             pNumRegs,
                             mmCOMPUTE_PGM_LO,
                             m_regs.computePgmLo.u32All);

    SetOneShRegValPairPacked(pRegPairs,
                             pNumRegs,
                             mmCOMPUTE_PGM_RSRC1,
                             m_regs.computePgmRsrc1.u32All);

    SetOneShRegValPairPacked(pRegPairs,
                             pNumRegs,
                             Gfx10Plus::mmCOMPUTE_PGM_RSRC3,
                             m_regs.computePgmRsrc3.u32All);

    if (m_regs.userDataInternalTable.u32All != InvalidUserDataInternalTable)
    {
        SetOneShRegValPairPacked(pRegPairs,
                                 pNumRegs,
                                 mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg,
                                 m_regs.userDataInternalTable.u32All);
    }

    SetOneShRegValPairPacked(pRegPairs,
                             pNumRegs,
                             Gfx11::mmCOMPUTE_DISPATCH_INTERLEAVE,
                             m_regs.computeDispatchInterleave.u32All);

    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();
    if (chipProps.gfx9.supportSpp != 0)
    {
        const RegisterInfo& regInfo = m_device.CmdUtil().GetRegInfo();

        SetOneShRegValPairPacked(pRegPairs,
                                 pNumRegs,
                                 regInfo.mmComputeShaderChksum,
                                 m_regs.computeShaderChksum.u32All);
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    PAL_ASSERT(InRange(*pNumRegs, startingIdx, startingIdx + NumShRegs));
#endif
}
#endif

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkCs::WriteShCommands(
    CmdStream*                      pCmdStream,
    uint32*                         pCmdSpace,
#if PAL_BUILD_GFX11
    bool                            regPairsSupported,
#endif
    const DynamicComputeShaderInfo& csInfo,
    bool                            prefetch
    ) const
{
    HwRegInfo::Dynamic dynamicRegs = m_regs.dynamic; // "Dynamic" bind-time register state.

    pCmdSpace = UpdateDynamicRegInfo(pCmdStream, pCmdSpace, &dynamicRegs, csInfo);

#if PAL_BUILD_GFX11
    if (regPairsSupported)
    {
        PackedRegisterPair regPairs[NumHwRegInfoRegs] = {};
        uint32             numRegs = 0;

        static_assert(NumHwRegInfoRegs <= Gfx11RegPairMaxRegCount, "Requesting too many registers!");

        AccumulateShCommandsSetPath(regPairs, &numRegs);
        AccumulateShCommandsDynamic(regPairs, &numRegs, dynamicRegs);

        if (m_pCsPerfDataInfo->regOffset != UserDataNotMapped)
        {
            SetOneShRegValPairPacked(regPairs, &numRegs, m_pCsPerfDataInfo->regOffset, m_pCsPerfDataInfo->gpuVirtAddr);
        }

        PAL_ASSERT(numRegs <= NumHwRegInfoRegs);

        pCmdSpace = pCmdStream->WriteSetShRegPairs<ShaderCompute>(regPairs, numRegs, pCmdSpace);
    }
    else
#endif
    {
        pCmdSpace = WriteShCommandsSetPath(pCmdStream, pCmdSpace);
        pCmdSpace = WriteShCommandsDynamic(pCmdStream, pCmdSpace, dynamicRegs);

        if (m_pCsPerfDataInfo->regOffset != UserDataNotMapped)
        {
            pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(m_pCsPerfDataInfo->regOffset,
                                                                    m_pCsPerfDataInfo->gpuVirtAddr,
                                                                    pCmdSpace);
        }
    }

    if (prefetch && (m_prefetchAddr != 0))
    {
        const EngineType       engine   = pCmdStream->GetEngineType();
        const Gfx9PalSettings& settings = m_device.Settings();
        const PrefetchMethod   method   = (engine == EngineTypeCompute) ? settings.shaderPrefetchMethodAce
                                                                        : settings.shaderPrefetchMethodGfx;

        if (method != PrefetchDisabled)
        {
            PrimeGpuCacheRange cacheInfo;
            cacheInfo.gpuVirtAddr         = m_prefetchAddr;
            cacheInfo.size                = m_prefetchSize;
            cacheInfo.usageMask           = CoherShaderRead;
            cacheInfo.addrTranslationOnly = (method == PrefetchPrimeUtcL2);

            pCmdSpace += m_device.CmdUtil().BuildPrimeGpuCaches(cacheInfo, engine, pCmdSpace);
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Writes PM4 set commands to the specified command stream.  This is used for writing pipeline state registers whose
// values are not known until pipeline bind time. Returns the next unused DWORD in pCmdSpace.
uint32* PipelineChunkCs::WriteShCommandsDynamic(
    CmdStream*         pCmdStream,
    uint32*            pCmdSpace,
    HwRegInfo::Dynamic dynamicRegs
    ) const
{
    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PGM_RSRC2,
                                                            dynamicRegs.computePgmRsrc2.u32All,
                                                            pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_RESOURCE_LIMITS,
                                                            dynamicRegs.computeResourceLimits.u32All,
                                                            pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Writes PM4 SET commands to the specified command stream.  This is only expected to be called when the LOAD path is
// not in use and we need to use the SET path fallback. Returns the next unused DWORD in pCmdSpace.
uint32* PipelineChunkCs::WriteShCommandsSetPath(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmCOMPUTE_NUM_THREAD_X,
                                              mmCOMPUTE_NUM_THREAD_Z,
                                              ShaderCompute,
                                              &m_regs.computeNumThreadX,
                                              pCmdSpace);

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

    if (m_regs.userDataInternalTable.u32All != InvalidUserDataInternalTable)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg,
                                                                m_regs.userDataInternalTable.u32All,
                                                                pCmdSpace);
    }

#if PAL_BUILD_GFX11
    if (IsGfx11(chipProps.gfxLevel))
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(Gfx11::mmCOMPUTE_DISPATCH_INTERLEAVE,
                                                                m_regs.computeDispatchInterleave.u32All,
                                                                pCmdSpace);
    }
#endif

    if (chipProps.gfx9.supportSpp != 0)
    {
        const RegisterInfo& regInfo = m_device.CmdUtil().GetRegInfo();
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
void PipelineChunkCs::Clone(
    const PipelineChunkCs& chunkCs)
{
    m_regs = chunkCs.m_regs;
    m_prefetchAddr = chunkCs.m_prefetchAddr;
    m_prefetchSize = chunkCs.m_prefetchSize;
}

} // Gfx9
} // Pal
