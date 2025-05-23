/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
    m_flags{},
    m_regs{},
    m_prefetchAddr(0),
    m_prefetchSize(0),
    m_gfx11NumPairsRegs(0),
    m_pCsPerfDataInfo(pPerfDataInfo),
    m_pStageInfo(pStageInfo)
{
    if (m_pStageInfo != nullptr)
    {
        m_pStageInfo->stageId = Abi::HardwareStage::Cs;
    }
    m_regs.userDataInternalTable.u32All = InvalidUserDataInternalTable;

    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();
    const Gfx9PalSettings&   settings  = device.Settings();
    m_flags.supportSpp                 = chipProps.gfx9.supportSpp;
    m_flags.isGfx11                    = IsGfx11(chipProps.gfxLevel);
    m_flags.useUnpackedRegPairs        = device.IsGfx11F32UnpackedRegPairsSupported();
    m_flags.usePackedRegPairs          = device.IsGfx11PackedRegPairsSupported();
    m_flags.acePrefetchMethod          = settings.shaderPrefetchMethodAce;
    m_flags.gfxPrefetchMethod          = settings.shaderPrefetchMethodGfx;

    const uint32 maxWavesPerSe = chipProps.gfx9.numShaderArrays * Device::GetMaxWavesPerSh(chipProps, true);
    const uint32 numCuPerSe    = chipProps.gfx9.numShaderArrays * chipProps.gfx9.numCuPerSh;

    m_flags.maxWavesPerSe = maxWavesPerSe;
    m_flags.numCuPerSe    = numCuPerSe;

    // We shouldn't see a scenario where unpacked && packed are both enabled even though RS64 technically supports this.
    PAL_ASSERT((m_flags.useUnpackedRegPairs == 0) || (m_flags.useUnpackedRegPairs ^ m_flags.usePackedRegPairs));

    // Sanity check we didn't overflow any bitfields.
    PAL_ASSERT((m_flags.maxWavesPerSe == maxWavesPerSe) && (m_flags.numCuPerSe == numCuPerSe));
}

// =====================================================================================================================
// Perform LateInit after InitRegisters.
void PipelineChunkCs::DoLateInit(
    const Device&       device,
    DispatchDims*       pThreadsPerTg,
    CodeObjectUploader* pUploader)
{
    GpuSymbol symbol = { };
    if (pUploader != nullptr)
    {
        if (pUploader->GetGpuSymbol(Abi::PipelineSymbolType::CsShdrIntrlTblPtr, &symbol) == Result::Success)
        {
            m_regs.userDataInternalTable.bits.DATA = LowPart(symbol.gpuVirtAddr);
        }

        if (device.CoreSettings().pipelinePrefetchEnable)
        {
            m_prefetchAddr = pUploader->PrefetchAddr();
            m_prefetchSize = pUploader->PrefetchSize();
        }
    }

    pThreadsPerTg->x = m_regs.computeNumThreadX.bits.NUM_THREAD_FULL;
    pThreadsPerTg->y = m_regs.computeNumThreadY.bits.NUM_THREAD_FULL;
    pThreadsPerTg->z = m_regs.computeNumThreadZ.bits.NUM_THREAD_FULL;

    SetupRegPairs();
}

// =====================================================================================================================
// Setups up reg pairs from m_regs.
void PipelineChunkCs::SetupRegPairs()
{
    m_gfx11NumPairsRegs = 0;

    if (m_flags.usePackedRegPairs)
    {
        AccumulateShCommandsSetPath(m_gfx11PackedRegPairs, &m_gfx11NumPairsRegs);

        // Ensure we pad out to an even number of regs. It is important to reuse the very first
        // value in this case which is always safe!
        if ((m_gfx11NumPairsRegs % 2) != 0)
        {
            m_gfx11PackedRegPairs[m_gfx11NumPairsRegs / 2].offset1 = m_gfx11PackedRegPairs[0].offset0;
            m_gfx11PackedRegPairs[m_gfx11NumPairsRegs / 2].value1  = m_gfx11PackedRegPairs[0].value0;
            m_gfx11NumPairsRegs++;
        }

        PAL_ASSERT(m_gfx11NumPairsRegs <= (Gfx11MaxNumRs64PackedPairs * 2));
    }
    else if (m_flags.useUnpackedRegPairs)
    {
        AccumulateShCommandsSetPath(m_gfx11UnpackedRegPairs, &m_gfx11NumPairsRegs);

        PAL_ASSERT(m_gfx11NumPairsRegs <= Gfx11MaxNumShRegPairRegs);
    }
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.
void PipelineChunkCs::LateInit(
    const Device&                           device,
    const Util::PalAbi::CodeObjectMetadata& metadata,
    uint32                                  wavefrontSize,
    DispatchDims*                           pThreadsPerTg,
    DispatchInterleaveSize                  interleaveSize,
    CodeObjectUploader*                     pUploader)
{
    InitRegisters(device, metadata, interleaveSize, wavefrontSize);

    GpuSymbol symbol = { };
    if ((pUploader != nullptr) &&
        (pUploader->GetEntryPointGpuSymbol(Abi::HardwareStage::Cs, metadata, &symbol) == Result::Success))
    {
        m_pStageInfo->codeLength = static_cast<size_t>(symbol.size);
        PAL_ASSERT(IsPow2Aligned(symbol.gpuVirtAddr, 256u));

        m_regs.computePgmLo.bits.DATA = Get256BAddrLo(symbol.gpuVirtAddr);
    }

    DoLateInit(device, pThreadsPerTg, pUploader);
}

// =====================================================================================================================
// Late initialization for Hsa pipeline chunk.
void PipelineChunkCs::LateInit(
    const Device&           device,
    const RegisterVector&   registers,
    uint32                  wavefrontSize,
    DispatchDims*           pThreadsPerTg,
    DispatchInterleaveSize  interleaveSize,
    CodeObjectUploader*     pUploader)
{
    InitRegisters(device, registers, interleaveSize, wavefrontSize);

    GpuSymbol symbol = { };
    if ((pUploader != nullptr) &&
        (pUploader->GetGpuSymbol(Abi::PipelineSymbolType::CsMainEntry, &symbol) == Result::Success))
    {
        m_pStageInfo->codeLength = static_cast<size_t>(symbol.size);
        PAL_ASSERT(IsPow2Aligned(symbol.gpuVirtAddr, 256u));

        m_regs.computePgmLo.bits.DATA = Get256BAddrLo(symbol.gpuVirtAddr);
    }

    DoLateInit(device, pThreadsPerTg, pUploader);
}

// =====================================================================================================================
void PipelineChunkCs::InitGpuAddrFromMesh(
    const AbiReader&       abiReader,
    const PipelineChunkGs& chunkGs)
{
    const Elf::SymbolTableEntry* pCsMainEntry = abiReader.GetSymbolHeader(Abi::PipelineSymbolType::CsMainEntry);
    const Elf::SymbolTableEntry* pGsMainEntry = abiReader.GetSymbolHeader(Abi::PipelineSymbolType::GsMainEntry);
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
        abiReader.GetSymbolHeader(Abi::PipelineSymbolType::CsShdrIntrlTblPtr);
    const Elf::SymbolTableEntry* pGsInternalTable =
        abiReader.GetSymbolHeader(Abi::PipelineSymbolType::GsShdrIntrlTblPtr);
    if ((pCsInternalTable != nullptr) && (pGsInternalTable != nullptr))
    {
        uint32 gsTableLoVa = chunkGs.UserDataInternalTableLoVa();
        uint32 csTableLoVa = gsTableLoVa + pCsInternalTable->st_value - pGsInternalTable->st_value;
        m_regs.userDataInternalTable.bits.DATA = csTableLoVa;
    }

    SetupRegPairs();
}

// =====================================================================================================================
// Helper method which initializes registers from the metadata extraced from an ELF metadata blob.
void PipelineChunkCs::InitRegisters(
    const Device&                     device,
    const PalAbi::CodeObjectMetadata& metadata,
    DispatchInterleaveSize            interleaveSize,
    uint32                            wavefrontSize)
{
    const Gfx9PalSettings& settings = device.Settings();

    m_regs.computePgmRsrc1.u32All = AbiRegisters::ComputePgmRsrc1(metadata);
    if (settings.waCwsrThreadgroupTrap != 0)
    {
        m_regs.computePgmRsrc1.bits.PRIV = 1;
    }

    m_regs.dynamic.computePgmRsrc2.u32All = AbiRegisters::ComputePgmRsrc2(metadata);

    // These are optional for shader libraries.
    m_regs.computeNumThreadX.u32All = AbiRegisters::ComputeNumThreadX(metadata);
    m_regs.computeNumThreadY.u32All = AbiRegisters::ComputeNumThreadY(metadata);
    m_regs.computeNumThreadZ.u32All = AbiRegisters::ComputeNumThreadZ(metadata);

    m_regs.computePgmRsrc3.u32All = AbiRegisters::ComputePgmRsrc3(metadata, device, m_pStageInfo->codeLength);
    m_regs.computeShaderChksum.u32All = AbiRegisters::ComputeShaderChkSum(metadata, device);
    m_regs.dynamic.computeResourceLimits.u32All =
        AbiRegisters::ComputeResourceLimits(metadata, device, wavefrontSize);

    m_regs.computeDispatchInterleave = AbiRegisters::ComputeDispatchInterleave(device, interleaveSize);
}

// =====================================================================================================================
// Helper method which initializes registers from the register vector extraced from an ELF metadata blob.
void PipelineChunkCs::InitRegisters(
    const Device&          device,
    const RegisterVector&  registers,
    DispatchInterleaveSize interleaveSize,
    uint32                 wavefrontSize)
{
    const Gfx9PalSettings&   settings  = device.Settings();
    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();

    m_regs.computePgmRsrc1.u32All = registers.At(mmCOMPUTE_PGM_RSRC1);
    if (settings.waCwsrThreadgroupTrap != 0)
    {
        m_regs.computePgmRsrc1.bits.PRIV = 1;
    }

    m_regs.dynamic.computePgmRsrc2.u32All = registers.At(mmCOMPUTE_PGM_RSRC2);

    // These are optional for shader libraries.
    registers.HasEntry(mmCOMPUTE_NUM_THREAD_X, &m_regs.computeNumThreadX.u32All);
    registers.HasEntry(mmCOMPUTE_NUM_THREAD_Y, &m_regs.computeNumThreadY.u32All);
    registers.HasEntry(mmCOMPUTE_NUM_THREAD_Z, &m_regs.computeNumThreadZ.u32All);

    m_regs.computePgmRsrc3.u32All = registers.At(mmCOMPUTE_PGM_RSRC3);

    if (m_flags.isGfx11)
    {
        m_regs.computePgmRsrc3.gfx11.INST_PREF_SIZE = device.GetShaderPrefetchSize(m_pStageInfo->codeLength);

        // PWS+ only support pre-shader waits if the IMAGE_OP bit is set. Theoretically we only set it for shaders that
        // do an image operation. However that would mean that our use of the pre-shader PWS+ wait is dependent on us
        // only waiting on image resources, which we don't know in our interface. For now always set the IMAGE_OP bit
        // for corresponding shaders, making the pre-shader waits global.
        m_regs.computePgmRsrc3.gfx11.IMAGE_OP = 1;
    }

    if (m_flags.supportSpp == 1)
    {
        registers.HasEntry(mmCOMPUTE_SHADER_CHKSUM, &m_regs.computeShaderChksum.u32All);
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

    // LOCK_THRESHOLD: Sets per-SH low threshold for locking.  Set in units of 4, 0 disables locking.
    // LOCK_THRESHOLD's maximum value: (6 bits), in units of 4, so it is max of 252.
    constexpr uint32 Gfx9MaxLockThreshold = 252;
    PAL_ASSERT(settings.csLockThreshold <= Gfx9MaxLockThreshold);

    if (settings.waForceLockThresholdZero)
    {
        m_regs.dynamic.computeResourceLimits.bits.LOCK_THRESHOLD = 0;
    }
    else
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

    if (m_flags.isGfx11)
    {
        m_regs.computeDispatchInterleave = AbiRegisters::ComputeDispatchInterleave(device, interleaveSize);
    }
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
    pSignature->flags.isWave32 = (metadata.WavefrontSize() == 32);
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

    const auto& csMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Cs)];
    if (csMetadata.hasEntry.wavefrontSize != 0)
    {
        PAL_ASSERT((csMetadata.wavefrontSize == 64) || (csMetadata.wavefrontSize == 32));
        pSignature->flags.isWave32 = (csMetadata.wavefrontSize == 32);
    }
}

// =====================================================================================================================
void PipelineChunkCs::SetupSignatureFromMetadata(
    ComputeShaderSignature*           pSignature,
    const PalAbi::CodeObjectMetadata& metadata)
{
    const Util::PalAbi::HardwareStageMetadata& hwCs = metadata.pipeline.hardwareStage[uint32(Abi::HardwareStage::Cs)];
    PAL_ASSERT(hwCs.userSgprs <= 16);

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
    ComputeShaderSignature* pSignature,
    const RegisterVector&   registers)
{
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
void PipelineChunkCs::UpdateDynamicRegInfo(
    HwRegInfo::Dynamic*             pDynamicRegs,
    const DynamicComputeShaderInfo& csInfo
    ) const
{
    // TG_PER_CU: Sets the CS threadgroup limit per CU. Range is 1 to 15, 0 disables the limit.
    constexpr uint32 Gfx9MaxTgPerCu = 15;
    pDynamicRegs->computeResourceLimits.bits.TG_PER_CU = Min(csInfo.maxThreadGroupsPerCu, Gfx9MaxTgPerCu);
    if (csInfo.maxWavesPerCu > 0)
    {
        const uint32 wavesPerSe = Util::Min(m_flags.maxWavesPerSe,
                                            static_cast<uint32>(csInfo.maxWavesPerCu * m_flags.numCuPerSe));
        PAL_ASSERT(wavesPerSe <= (COMPUTE_RESOURCE_LIMITS__WAVES_PER_SH_MASK >>
                                  COMPUTE_RESOURCE_LIMITS__WAVES_PER_SH__SHIFT));

        // Yes, this is actually WAVES_PER_SE.
        pDynamicRegs->computeResourceLimits.bits.WAVES_PER_SH = wavesPerSe;
    }

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
}

// =====================================================================================================================
// Accumulates a set of registers into an array of packed register pairs, analagous to WriteShCommandsDynamic().
template<typename T>
void PipelineChunkCs::AccumulateShCommandsSetPath(
    T*      pRegPairs,
    uint32* pNumRegs
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    const uint32 startingIdx = *pNumRegs;
#endif

    SetSeqShRegValPair(pRegPairs,
                       pNumRegs,
                       mmCOMPUTE_NUM_THREAD_X,
                       mmCOMPUTE_NUM_THREAD_Z,
                       &m_regs.computeNumThreadX);

    SetOneShRegValPair(pRegPairs,
                       pNumRegs,
                       mmCOMPUTE_PGM_LO,
                       m_regs.computePgmLo.u32All);

    SetOneShRegValPair(pRegPairs,
                       pNumRegs,
                       mmCOMPUTE_PGM_RSRC1,
                       m_regs.computePgmRsrc1.u32All);

    SetOneShRegValPair(pRegPairs,
                       pNumRegs,
                       mmCOMPUTE_PGM_RSRC3,
                       m_regs.computePgmRsrc3.u32All);

    if (m_regs.userDataInternalTable.u32All != InvalidUserDataInternalTable)
    {
        SetOneShRegValPair(pRegPairs,
                           pNumRegs,
                           mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg,
                           m_regs.userDataInternalTable.u32All);
    }

    SetOneShRegValPair(pRegPairs,
                       pNumRegs,
                       Gfx11::mmCOMPUTE_DISPATCH_INTERLEAVE,
                       m_regs.computeDispatchInterleave.u32All);

    if (m_flags.supportSpp != 0)
    {
        SetOneShRegValPair(pRegPairs,
                           pNumRegs,
                           mmCOMPUTE_SHADER_CHKSUM,
                           m_regs.computeShaderChksum.u32All);
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    PAL_ASSERT(InRange(*pNumRegs, startingIdx, startingIdx + NumShRegs));
#endif
}

template
void PipelineChunkCs::AccumulateShCommandsSetPath(
    PackedRegisterPair* pRegPairs,
    uint32*             pNumRegs) const;
template
void PipelineChunkCs::AccumulateShCommandsSetPath(
    RegisterValuePair* pRegPairs,
    uint32*            pNumRegs) const;

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
template <bool IsAce>
uint32* PipelineChunkCs::WriteShCommands(
    CmdStream*                      pCmdStream,
    uint32*                         pCmdSpace,
    const DynamicComputeShaderInfo& csInfo,
    bool                            prefetch
    ) const
{
    HwRegInfo::Dynamic dynamicRegs = m_regs.dynamic; // "Dynamic" bind-time register state.

    UpdateDynamicRegInfo(&dynamicRegs, csInfo);

    if ((m_flags.usePackedRegPairs) && (IsAce == false))
    {
        if (pCmdStream->Pm4OptimizerEnabled() == false)
        {
            pCmdSpace = pCmdStream->WriteSetConstShRegPairs<ShaderCompute, false>(m_gfx11PackedRegPairs,
                                                                                  m_gfx11NumPairsRegs,
                                                                                  pCmdSpace);
        }
        else
        {
            pCmdSpace = pCmdStream->WriteSetConstShRegPairs<ShaderCompute, true>(m_gfx11PackedRegPairs,
                                                                                 m_gfx11NumPairsRegs,
                                                                                 pCmdSpace);
        }
    }
    else if (m_flags.useUnpackedRegPairs && (IsAce == false)) //< PFP only supported.
    {
        pCmdSpace = pCmdStream->WriteSetShRegPairs<ShaderCompute>(m_gfx11UnpackedRegPairs,
                                                                  m_gfx11NumPairsRegs,
                                                                  pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteShCommandsSetPath(pCmdStream, pCmdSpace);
    }

    pCmdSpace = WriteShCommandsDynamic(pCmdStream, pCmdSpace, dynamicRegs);

    if (m_pCsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(m_pCsPerfDataInfo->regOffset,
                                                                m_pCsPerfDataInfo->gpuVirtAddr,
                                                                pCmdSpace);
    }

    if (prefetch && (m_prefetchAddr != 0))
    {
        const EngineType     engine = pCmdStream->GetEngineType();
        const PrefetchMethod method = (engine == EngineTypeCompute) ? m_flags.acePrefetchMethod
                                                                    : m_flags.gfxPrefetchMethod;

        if (method != PrefetchDisabled)
        {
            PrimeGpuCacheRange cacheInfo;
            cacheInfo.gpuVirtAddr         = m_prefetchAddr;
            cacheInfo.size                = m_prefetchSize;
            cacheInfo.usageMask           = CoherShaderRead;
            cacheInfo.addrTranslationOnly = (method == PrefetchPrimeUtcL2);

            pCmdSpace = pCmdStream->WritePrimeGpuCaches(cacheInfo, engine, pCmdSpace);
        }
    }

    return pCmdSpace;
}

template
uint32* PipelineChunkCs::WriteShCommands<true>(
    CmdStream*                      pCmdStream,
    uint32*                         pCmdSpace,
    const DynamicComputeShaderInfo& csInfo,
    bool                            prefetch
    ) const;
template
uint32* PipelineChunkCs::WriteShCommands<false>(
    CmdStream*                      pCmdStream,
    uint32*                         pCmdSpace,
    const DynamicComputeShaderInfo& csInfo,
    bool                            prefetch
    ) const;

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

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PGM_RSRC3,
                                                            m_regs.computePgmRsrc3.u32All,
                                                            pCmdSpace);

    if (m_regs.userDataInternalTable.u32All != InvalidUserDataInternalTable)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_USER_DATA_0 + ConstBufTblStartReg,
                                                                m_regs.userDataInternalTable.u32All,
                                                                pCmdSpace);
    }

    if (m_flags.isGfx11)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(Gfx11::mmCOMPUTE_DISPATCH_INTERLEAVE,
                                                                m_regs.computeDispatchInterleave.u32All,
                                                                pCmdSpace);
    }

    if (m_flags.supportSpp != 0)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_SHADER_CHKSUM,
                                                                m_regs.computeShaderChksum.u32All,
                                                                pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
uint32* PipelineChunkCs::WriteShCommandsLdsSize(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace,
    uint32     ldsBytesPerTg
    ) const
{
    // If ldsBytesPerTg is zero, which means there is no dynamic LDS, keep LDS_SIZE register as static LDS size.
    if (ldsBytesPerTg > 0)
    {
        regCOMPUTE_PGM_RSRC2 computePgmRsrc2 = m_regs.dynamic.computePgmRsrc2;

        // Round to nearest multiple of the LDS granularity, then convert to the register value.
        // NOTE: Granularity for the LDS_SIZE field is 128, range is 0->128 which allocates 0 to 16K DWORDs.
        computePgmRsrc2.bits.LDS_SIZE =
            Pow2Align((ldsBytesPerTg / sizeof(uint32)), Gfx9LdsDwGranularity) >> Gfx9LdsDwGranularityShift;

        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PGM_RSRC2,
                                                                computePgmRsrc2.u32All,
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

    SetupRegPairs();
}

// =====================================================================================================================
void PipelineChunkCs::Clone(
    const PipelineChunkCs& chunkCs)
{
    m_regs         = chunkCs.m_regs;
    m_prefetchAddr = chunkCs.m_prefetchAddr;
    m_prefetchSize = chunkCs.m_prefetchSize;
    m_flags        = chunkCs.m_flags;

    if (m_flags.usePackedRegPairs)
    {
        memcpy(m_gfx11PackedRegPairs, chunkCs.m_gfx11PackedRegPairs, sizeof(m_gfx11PackedRegPairs));
    }
    else if (m_flags.useUnpackedRegPairs)
    {
        memcpy(m_gfx11UnpackedRegPairs, chunkCs.m_gfx11UnpackedRegPairs, sizeof(m_gfx11UnpackedRegPairs));
    }
    m_gfx11NumPairsRegs = chunkCs.m_gfx11NumPairsRegs;
}

} // Gfx9
} // Pal
