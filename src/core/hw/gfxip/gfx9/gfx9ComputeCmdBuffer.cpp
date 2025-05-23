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

#include "core/hw/gfxip/gfx9/gfx9BorderColorPalette.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9ComputeCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9ComputePipeline.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9IndirectCmdGenerator.h"
#include "core/hw/gfxip/gfx9/gfx9PerfExperiment.h"
#include "core/hw/gfxip/queryPool.h"
#include "core/imported/hsa/AMDHSAKernelDescriptor.h"
#include "core/imported/hsa/amd_hsa_kernel_code.h"
#include "core/cmdAllocator.h"
#include "g_platformSettings.h"
#include "core/settingsLoader.h"
#include "palHsaAbiMetadata.h"
#include "palInlineFuncs.h"
#include "palVectorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
ComputeCmdBuffer::ComputeCmdBuffer(
    const Device&              device,
    const CmdBufferCreateInfo& createInfo)
    :
    Pal::ComputeCmdBuffer(device, createInfo, device.BarrierMgr(), &m_cmdStream, false),
    m_device(device),
    m_cmdUtil(device.CmdUtil()),
    m_issueSqttMarkerEvent(device.Parent()->IssueSqttMarkerEvents()),
    m_cmdStream(device,
                createInfo.pCmdAllocator,
                EngineTypeCompute,
                SubEngineType::Primary,
                CmdStreamUsage::Workload,
                IsNested()),
    m_pSignatureCs(&device.GetNullCsSignature()),
    m_ringSizeComputeScratch(0)
{
    const PalPlatformSettings& platformSettings = device.Parent()->GetPlatform()->PlatformSettings();
    m_describeDispatch                          = (device.Parent()->IssueSqttMarkerEvents() ||
                                                   device.Parent()->IssueCrashAnalysisMarkerEvents() ||
                                                   platformSettings.cmdBufferLoggerConfig.embedDrawDispatchInfo);

    // Assume PAL ABI compute pipelines by default.
    SetDispatchFunctions(false);
}

// =====================================================================================================================
// Initializes Gfx9-specific functionality.
Result ComputeCmdBuffer::Init(
    const CmdBufferInternalCreateInfo& internalInfo)
{
    Result result = Pal::ComputeCmdBuffer::Init(internalInfo);

    if (result == Result::Success)
    {
        result = m_cmdStream.Init();
    }

    return result;
}

// =====================================================================================================================
void ComputeCmdBuffer::ResetState()
{
    Pal::ComputeCmdBuffer::ResetState();

    // Assume PAL ABI compute pipelines by default.
    SetDispatchFunctions(false);

    m_pSignatureCs = &m_device.GetNullCsSignature();

    m_ringSizeComputeScratch  = 0;
}

// =====================================================================================================================
// Binds a graphics or compute pipeline to this command buffer.
void ComputeCmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    auto* const pNewPipeline = static_cast<const ComputePipeline*>(params.pPipeline);
    auto* const pOldPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);

    const bool newUsesHsaAbi = (pNewPipeline != nullptr) && (pNewPipeline->GetInfo().flags.hsaAbi == 1u);
    const bool oldUsesHsaAbi = (pOldPipeline != nullptr) && (pOldPipeline->GetInfo().flags.hsaAbi == 1u);

    if (oldUsesHsaAbi != newUsesHsaAbi)
    {
        // The HSA abi can clobber USER_DATA_0, which holds the global internal table address for PAL ABI, so we must
        // save the address to memory before switching to an HSA ABI or restore it when switching back to PAL ABI.
        if (newUsesHsaAbi && (m_globalInternalTableAddr == 0))
        {
            m_globalInternalTableAddr = AllocateGpuScratchMem(1, 1);
            m_device.RsrcProcMgr().EchoGlobalInternalTableAddr(this, m_globalInternalTableAddr);
        }
        else if (newUsesHsaAbi == false)
        {
            uint32* pCmdSpace = m_cmdStream.ReserveCommands();
            pCmdSpace += m_cmdUtil.BuildLoadShRegsIndex(index__pfp_load_sh_reg_index__direct_addr,
                                                        data_format__pfp_load_sh_reg_index__offset_and_size,
                                                        m_globalInternalTableAddr,
                                                        mmCOMPUTE_USER_DATA_0,
                                                        1,
                                                        Pm4ShaderType::ShaderCompute,
                                                        pCmdSpace);
            m_cmdStream.CommitCommands(pCmdSpace);
        }

        SetDispatchFunctions(newUsesHsaAbi);
    }

    if (pNewPipeline != nullptr)
    {
        m_ringSizeComputeScratch = Max(pNewPipeline->GetRingSizeComputeScratch(), m_ringSizeComputeScratch);
    }

    GfxCmdBuffer::CmdBindPipeline(params);
}

// =====================================================================================================================
// Sets-up function pointers for the Dispatch entrypoint and all variants using template parameters.
template <bool HsaAbi, bool IssueSqttMarkerEvent, bool DescribeCallback>
void ComputeCmdBuffer::SetDispatchFunctions()
{
    static_assert(DescribeCallback || (IssueSqttMarkerEvent == false),
                  "DescribeCallback must be true if IssueSqttMarkerEvent is true!");

    m_funcTable.pfnCmdDispatch       = CmdDispatch<HsaAbi, IssueSqttMarkerEvent, DescribeCallback>;
    m_funcTable.pfnCmdDispatchOffset = CmdDispatchOffset<HsaAbi, IssueSqttMarkerEvent, DescribeCallback>;

    if (HsaAbi)
    {
        // Note that CmdDispatchIndirect does not support the HSA ABI.
        m_funcTable.pfnCmdDispatchIndirect = nullptr;
    }
    else
    {
        m_funcTable.pfnCmdDispatchIndirect = CmdDispatchIndirect<IssueSqttMarkerEvent, DescribeCallback>;
    }

}

// =====================================================================================================================
// Sets-up function pointers for the Dispatch entrypoint and all variants.
void ComputeCmdBuffer::SetDispatchFunctions(
    bool hsaAbi)
{
    if (hsaAbi)
    {
        if (m_issueSqttMarkerEvent)
        {
            SetDispatchFunctions<true, true, true>();
        }
        else if (m_describeDispatch)
        {
            SetDispatchFunctions<true, false, true>();
        }
        else
        {
            SetDispatchFunctions<true, false, false>();
        }
    }
    else
    {
        if (m_issueSqttMarkerEvent)
        {
            SetDispatchFunctions<false, true, true>();
        }
        else if (m_describeDispatch)
        {
            SetDispatchFunctions<false, false, true>();
        }
        else
        {
            SetDispatchFunctions<false, false, false>();
        }
    }
}

// =====================================================================================================================
// Issues a direct dispatch command. X, Y, and Z are in numbers of thread groups. We must discard the dispatch if x, y,
// or z are zero. To avoid branching, we will rely on the HW to discard the dispatch for us.
template <bool HsaAbi, bool IssueSqttMarkerEvent, bool DescribeCallback>
void PAL_STDCALL ComputeCmdBuffer::CmdDispatch(
    ICmdBuffer*       pCmdBuffer,
    DispatchDims      size,
    DispatchInfoFlags infoFlags)
{
    auto* pThis = static_cast<ComputeCmdBuffer*>(pCmdBuffer);

    if (DescribeCallback)
    {
        pThis->DescribeDispatch(Developer::DrawDispatchType::CmdDispatch, size, infoFlags);
    }

    uint32* pCmdSpace = pThis->m_cmdStream.ReserveCommands();

    if (HsaAbi)
    {
        pCmdSpace = pThis->ValidateDispatchHsaAbi({}, size, pCmdSpace);
    }
    else
    {
        pCmdSpace = pThis->ValidateDispatchPalAbi(0uLL, size, pCmdSpace);
    }

    if (pThis->m_cmdBufState.flags.packetPredicate != 0)
    {
        uint32 predSize = CmdUtil::DispatchDirectSize;
        if (IssueSqttMarkerEvent)
        {
            predSize += CmdUtil::WriteNonSampleEventDwords;
        }
        pCmdSpace += pThis->m_cmdUtil.BuildCondExec(pThis->m_predGpuAddr, predSize, pCmdSpace);
    }

    pCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect<false, true>(size,
                                                                   PredDisable,
                                                                   pThis->m_pSignatureCs->flags.isWave32,
                                                                   pThis->UsesDispatchTunneling(),
                                                                   pThis->DisablePartialPreempt(),
                                                                   pCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeCompute, pCmdSpace);
    }

    pThis->m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Issues an indirect dispatch command. We must discard the dispatch if x, y, or z are zero. We will rely on the HW to
// discard the dispatch for us.
template <bool IssueSqttMarkerEvent, bool DescribeCallback>
void PAL_STDCALL ComputeCmdBuffer::CmdDispatchIndirect(
    ICmdBuffer* pCmdBuffer,
    gpusize     gpuVirtAddr)
{
    auto* pThis = static_cast<ComputeCmdBuffer*>(pCmdBuffer);

    if (DescribeCallback)
    {
        pThis->DescribeDispatchIndirect();
    }

    PAL_ASSERT(IsPow2Aligned(gpuVirtAddr, sizeof(uint32)));

    uint32* pCmdSpace = pThis->m_cmdStream.ReserveCommands();

    pCmdSpace = pThis->ValidateDispatchPalAbi(gpuVirtAddr, {}, pCmdSpace);

    if (pThis->m_cmdBufState.flags.packetPredicate != 0)
    {
        uint32 size = CmdUtil::DispatchIndirectMecSize;
        if (IssueSqttMarkerEvent)
        {
            size += CmdUtil::WriteNonSampleEventDwords;
        }
        pCmdSpace += pThis->m_cmdUtil.BuildCondExec(pThis->m_predGpuAddr, size, pCmdSpace);
    }

    pCmdSpace += pThis->m_cmdUtil.BuildDispatchIndirectMec(gpuVirtAddr,
                                                           pThis->m_pSignatureCs->flags.isWave32,
                                                           pThis->UsesDispatchTunneling(),
                                                           pThis->DisablePartialPreempt(),
                                                           pCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeCompute, pCmdSpace);
    }

    pThis->m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Issues an direct dispatch command with immediate threadgroup offsets. We must discard the dispatch if x, y, or z are
// zero. To avoid branching, we will rely on the HW to discard the dispatch for us.
template <bool HsaAbi, bool IssueSqttMarkerEvent, bool DescribeCallback>
void PAL_STDCALL ComputeCmdBuffer::CmdDispatchOffset(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims offset,
    DispatchDims launchSize,
    DispatchDims logicalSize)
{
    auto* pThis = static_cast<ComputeCmdBuffer*>(pCmdBuffer);

    if (DescribeCallback)
    {
        pThis->DescribeDispatchOffset(offset, launchSize, logicalSize);
    }

    uint32* pCmdSpace = pThis->m_cmdStream.ReserveCommands();

    if (HsaAbi)
    {
        pCmdSpace = pThis->ValidateDispatchHsaAbi(offset, logicalSize, pCmdSpace);
    }
    else
    {
        pCmdSpace = pThis->ValidateDispatchPalAbi(0uLL, logicalSize, pCmdSpace);
    }

    pCmdSpace = pThis->m_cmdStream.WriteSetSeqShRegs(mmCOMPUTE_START_X,
                                                     mmCOMPUTE_START_Z,
                                                     ShaderCompute,
                                                     &offset,
                                                     pCmdSpace);

    if (pThis->m_cmdBufState.flags.packetPredicate != 0)
    {
        uint32 size = CmdUtil::DispatchDirectSize;
        if (IssueSqttMarkerEvent)
        {
            size += CmdUtil::WriteNonSampleEventDwords;
        }
        pCmdSpace += pThis->m_cmdUtil.BuildCondExec(pThis->m_predGpuAddr, size, pCmdSpace);
    }

    // The DIM_X/Y/Z in DISPATCH_DIRECT packet are used to program COMPUTE_DIM_X/Y/Z registers, which are actually the
    // end block positions instead of execution block dimensions. So we need to use the dimensions plus offsets.
    pCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect<false, false>(offset + launchSize,
                                                                    PredDisable,
                                                                    pThis->m_pSignatureCs->flags.isWave32,
                                                                    pThis->UsesDispatchTunneling(),
                                                                    pThis->DisablePartialPreempt(),
                                                                    pCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeCompute, pCmdSpace);
    }

    pThis->m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdUpdateBusAddressableMemoryMarker(
    const IGpuMemory& dstGpuMemory,
    gpusize           offset,
    uint32            value)
{
    const GpuMemory*     pGpuMemory = static_cast<const GpuMemory*>(&dstGpuMemory);
    const WriteDataInfo  writeData  =
    {
        .engineType = GetEngineType(),
        .dstAddr    = pGpuMemory->GetBusAddrMarkerVa() + offset,
        .dstSel     = dst_sel__mec_write_data__memory,
    };
    CmdUtil::BuildWriteData(writeData, value, m_cmdStream.AllocateCommands(CmdUtil::WriteDataSizeDwords + 1));
}

// =====================================================================================================================
// Use the GPU's command processor to execute an atomic memory operation
void ComputeCmdBuffer::CmdMemoryAtomic(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    uint64            srcData,
    AtomicOp          atomicOp)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    pCmdSpace += m_cmdUtil.BuildAtomicMem(atomicOp, dstGpuMemory.Desc().gpuVirtAddr + dstOffset, srcData, pCmdSpace);
    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdWriteTimestamp(
    uint32            stageMask,    // Bitmask of PipelineStageFlag
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    // This will replace PipelineStageBlt with a more specific set of flags if we haven't done any CP DMAs.
    m_barrierMgr.OptimizeStageMask(this, BarrierType::Global, &stageMask, nullptr);

    uint32*       pCmdSpace           = m_cmdStream.ReserveCommands();
    const gpusize address             = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;
    const bool    issueReleaseMem     = TestAnyFlagSet(stageMask, PipelineStageCs | PipelineStageBottomOfPipe);
    bool          releaseMemWaitCpDma = false;

    // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
    // this function expect that the prior blts have completed by the time the event is written to memory.
    // Given that our CP DMA blts are asynchronous to the pipeline stages the only way to satisfy this requirement
    // is to force the ME to stall until the CP DMAs are completed.
    if (GfxBarrierMgr::NeedWaitCpDma(this, stageMask))
    {
        releaseMemWaitCpDma = issueReleaseMem && m_device.EnableReleaseMemWaitCpDma();
        if (releaseMemWaitCpDma == false)
        {
            pCmdSpace += CmdUtil::BuildWaitDmaData(pCmdSpace);
        }
        SetCpBltState(false);
    }

    // If multiple flags are set we must go down the path that is most conservative (writes at the latest point).
    // This is easiest to implement in this order:
    // 1. The EOP path for compute shaders.
    // 2. The CP stages can write the value directly using COPY_DATA in the MEC.
    // Note that passing in a stageMask of zero will get you an MEC write. It's not clear if that is even legal but
    // doing an MEC write is probably the least impactful thing we could do in that case.
    if (issueReleaseMem)
    {
        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.dstAddr        = address;
        releaseInfo.dataSel        = data_sel__mec_release_mem__send_gpu_clock_counter;
        releaseInfo.gfx11WaitCpDma = releaseMemWaitCpDma;

        pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, pCmdSpace);
    }
    else
    {
        pCmdSpace += m_cmdUtil.BuildCopyData(EngineTypeCompute,
                                             0,
                                             dst_sel__mec_copy_data__tc_l2_obsolete,
                                             address,
                                             src_sel__mec_copy_data__gpu_clock_count,
                                             0,
                                             count_sel__mec_copy_data__64_bits_of_data,
                                             wr_confirm__mec_copy_data__wait_for_confirmation,
                                             pCmdSpace);
    }

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdWriteImmediate(
    uint32             stageMask, // Bitmask of PipelineStageFlag
    uint64             data,
    ImmediateDataWidth dataSize,
    gpusize            address)
{
    // This will replace PipelineStageBlt with a more specific set of flags if we haven't done any CP DMAs.
    m_barrierMgr.OptimizeStageMask(this, BarrierType::Global, &stageMask, nullptr);

    uint32*    pCmdSpace           = m_cmdStream.ReserveCommands();
    const bool is32Bit             = (dataSize == ImmediateDataWidth::ImmediateData32Bit);
    const bool issueReleaseMem     = TestAnyFlagSet(stageMask, PipelineStageCs | PipelineStageBottomOfPipe);
    bool       releaseMemWaitCpDma = false;

    // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
    // this function expect that the prior blts have completed by the time the event is written to memory.
    // Given that our CP DMA blts are asynchronous to the pipeline stages the only way to satisfy this requirement
    // is to force the ME to stall until the CP DMAs are completed.
    if (GfxBarrierMgr::NeedWaitCpDma(this, stageMask))
    {
        releaseMemWaitCpDma = issueReleaseMem && m_device.EnableReleaseMemWaitCpDma();
        if (releaseMemWaitCpDma == false)
        {
            pCmdSpace += CmdUtil::BuildWaitDmaData(pCmdSpace);
        }
        SetCpBltState(false);
    }

    // If multiple flags are set we must go down the path that is most conservative (writes at the latest point).
    // This is easiest to implement in this order:
    // 1. The EOP path for compute shaders.
    // 2. The CP stages can write the value directly using COPY_DATA in the MEC.
    // Note that passing in a stageMask of zero will get you an MEC write. It's not clear if that is even legal but
    // doing an MEC write is probably the least impactful thing we could do in that case.
    if (issueReleaseMem)
    {
        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.dstAddr        = address;
        releaseInfo.data           = data;
        releaseInfo.dataSel        = is32Bit ? data_sel__mec_release_mem__send_32_bit_low
                                             : data_sel__mec_release_mem__send_64_bit_data;
        releaseInfo.gfx11WaitCpDma = releaseMemWaitCpDma;

        pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, pCmdSpace);
    }
    else
    {
        pCmdSpace += m_cmdUtil.BuildCopyData(EngineTypeCompute,
                                             0,
                                             dst_sel__mec_copy_data__tc_l2_obsolete,
                                             address,
                                             src_sel__mec_copy_data__immediate_data,
                                             data,
                                             is32Bit ? count_sel__mec_copy_data__32_bits_of_data
                                                     : count_sel__mec_copy_data__64_bits_of_data,
                                             wr_confirm__mec_copy_data__wait_for_confirmation,
                                             pCmdSpace);
    }

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdBindBorderColorPalette(
    PipelineBindPoint          pipelineBindPoint,
    const IBorderColorPalette* pPalette)
{
    // NOTE: The hardware fundamentally does not support multiple border color palettes for compute as the register
    //       which controls the address of the palette is a config register. We need to support this for our clients,
    //       but it should not be considered a correct implementation. As a result we may see arbitrary hangs that
    //       do not reproduce easily. This setting (disableBorderColorPaletteBinds) should be set to TRUE in the event
    //       that one of these hangs is suspected. At that point we will need to come up with a more robust solution
    //       which may involve getting KMD support.
    if (m_device.Settings().disableBorderColorPaletteBinds == false)
    {
        const auto*const pNewPalette = static_cast<const BorderColorPalette*>(pPalette);

        {
            PAL_ASSERT(pipelineBindPoint == PipelineBindPoint::Compute);
            if (pNewPalette != nullptr)
            {
                uint32* pCmdSpace = m_cmdStream.ReserveCommands();
                const gpusize tsGpuVa = GetWaitIdleTsGpuVa(&pCmdSpace);
                pCmdSpace = pNewPalette->WriteCommands(pipelineBindPoint,
                                                       tsGpuVa,
                                                       WaitIdleTsValue(),
                                                       &m_cmdStream,
                                                       pCmdSpace);
                m_cmdStream.CommitCommands(pCmdSpace);
            }

            m_computeState.pipelineState.pBorderColorPalette = pNewPalette;
            m_computeState.pipelineState.dirtyFlags.borderColorPalette = 1;
        }
    }
}

// =====================================================================================================================
// Helper function which is responsible for making sure all user-data entries are written to either the spill table or
// to user-SGPR's, as well as making sure that all indirect user-data tables are up-to-date in GPU memory. Part of
// Dispatch-time validation.
template <bool HasPipelineChanged>
uint32* ComputeCmdBuffer::ValidateUserData(
    const ComputePipelineSignature* pPrevSignature, // In: Signature of pipeline bound for previous Dispatch. Will be
                                                    // nullptr if the pipeline is not changing.
    UserDataEntries*                pUserData,
    UserDataTableState*             pSpillTable,
    uint32*                         pCmdSpace)
{
    PAL_ASSERT((HasPipelineChanged  && (pPrevSignature != nullptr)) ||
               (!HasPipelineChanged && (pPrevSignature == nullptr)));

    // Step #1:
    // Write all dirty user-data entries to their mapped user SGPR's and check if the spill table needs updating.
    // If the pipeline has changed we must also fixup the dirty bits because the prior compute pipeline could use
    // fewer fast sgprs than the current pipeline.

    bool alreadyWritten = false;
    if (HasPipelineChanged)
    {
        alreadyWritten = FixupUserSgprsOnPipelineSwitch(*pUserData, pPrevSignature, &pCmdSpace);
    }

    if (alreadyWritten == false)
    {
        pCmdSpace = m_cmdStream.WriteUserDataEntriesToSgprs<false, ShaderCompute>(m_pSignatureCs->stage,
                                                                                  *pUserData,
                                                                                  pCmdSpace);
    }

    const uint16 spillThreshold = m_pSignatureCs->spillThreshold;
    if (spillThreshold != NoUserDataSpilling)
    {
        const uint16 userDataLimit = m_pSignatureCs->userDataLimit;
        PAL_ASSERT(userDataLimit != 0);
        const uint16 lastUserData = (userDataLimit - 1);

        // Step #2:
        // Because the spill table is managed using CPU writes to embedded data, it must be fully re-uploaded for any
        // Dispatch whenever *any* contents have changed.
        bool reUpload = (pSpillTable->dirty != 0);
        if (HasPipelineChanged &&
            ((spillThreshold < pPrevSignature->spillThreshold) || (userDataLimit > pPrevSignature->userDataLimit)))
        {
            // If the pipeline is changing and the spilled region is expanding, we need to re-upload the table because
            // we normally only update the portions useable by the bound pipeline to minimize memory usage.
            reUpload = true;
        }
        else
        {
            // Otherwise, use the following loop to check if any of the spilled user-data entries are dirty.
            const uint32 firstMaskId = (spillThreshold / UserDataEntriesPerMask);
            const uint32 lastMaskId = (lastUserData / UserDataEntriesPerMask);
            for (uint32 maskId = firstMaskId; maskId <= lastMaskId; ++maskId)
            {
                size_t dirtyMask = pUserData->dirty[maskId];
                if (maskId == firstMaskId)
                {
                    // Ignore the dirty bits for any entries below the spill threshold.
                    const uint32 firstEntryInMask = (spillThreshold & (UserDataEntriesPerMask - 1));
                    dirtyMask &= ~BitfieldGenMask(size_t(firstEntryInMask));
                }
                if (maskId == lastMaskId)
                {
                    // Ignore the dirty bits for any entries beyond the user-data limit.
                    const uint32 lastEntryInMask = (lastUserData & (UserDataEntriesPerMask - 1));
                    dirtyMask &= BitfieldGenMask(size_t(lastEntryInMask + 1));
                }

                if (dirtyMask != 0)
                {
                    reUpload = true;
                    break; // We only care if *any* spill table contents change!
                }
            } // for each wide-bitfield sub-mask
        }

        // Step #3:
        // Re-upload spill table contents if necessary, and write the new GPU virtual address to the user-SGPR(s).
        if (reUpload)
        {
            UpdateUserDataTableCpu(pSpillTable,
                                   (userDataLimit - spillThreshold),
                                   spillThreshold,
                                   &pUserData->entries[0]);
        }

        if (reUpload || HasPipelineChanged)
        {
            if (m_pSignatureCs->stage.spillTableRegAddr != UserDataNotMapped)
            {
                pCmdSpace = m_cmdStream.WriteSetOneShReg<ShaderCompute>(m_pSignatureCs->stage.spillTableRegAddr,
                                                                        LowPart(pSpillTable->gpuVirtAddr),
                                                                        pCmdSpace);
            }
        }
    } // if current pipeline spills user-data

    // All dirtied user-data entries have been written to user-SGPR's or to the spill table somewhere in this method,
    // so it is safe to clear these bits.
    memset(&pUserData->dirty[0], 0, sizeof(UserDataEntries::dirty));

    return pCmdSpace;
}

// =====================================================================================================================
// Performs PAL ABI dispatch-time validation.
uint32* ComputeCmdBuffer::ValidateDispatchPalAbi(
    gpusize      indirectGpuVirtAddr,
    DispatchDims logicalSize,
    uint32*      pCmdSpace)
{
#if PAL_DEVELOPER_BUILD
    const bool enablePm4Instrumentation = m_device.GetPlatform()->PlatformSettings().pm4InstrumentorEnabled;

    uint32* pStartingCmdSpace = pCmdSpace;
    uint32  userDataCmdLen    = 0;
#endif

    if (m_computeState.pipelineState.dirtyFlags.pipeline)
    {
        const auto*const pNewPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);

        pCmdSpace = pNewPipeline->WriteCommands<true>(&m_cmdStream,
                                                      pCmdSpace,
                                                      m_computeState.dynamicCsInfo,
                                                      m_buildFlags.prefetchShaders);

#if PAL_DEVELOPER_BUILD
        if (enablePm4Instrumentation)
        {
            const uint32 pipelineCmdLen = (static_cast<uint32>(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
            m_device.DescribeBindPipelineValidation(this, pipelineCmdLen);
            pStartingCmdSpace = pCmdSpace;
        }
#endif

        const auto*const pPrevSignature = m_pSignatureCs;
        m_pSignatureCs                  = &pNewPipeline->Signature();

        pCmdSpace = ValidateUserData<true>(pPrevSignature,
                                           &m_computeState.csUserDataEntries,
                                           &m_spillTable.stateCs,
                                           pCmdSpace);
    }
    else
    {
        pCmdSpace = ValidateUserData<false>(nullptr,
                                            &m_computeState.csUserDataEntries,
                                            &m_spillTable.stateCs,
                                            pCmdSpace);
    }

#if PAL_DEVELOPER_BUILD
    if (enablePm4Instrumentation)
    {
        userDataCmdLen    = (static_cast<uint32>(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
        pStartingCmdSpace = pCmdSpace;
    }
#endif

    m_computeState.pipelineState.dirtyFlags.u32All = 0;

    if (m_pSignatureCs->numWorkGroupsRegAddr != UserDataNotMapped)
    {
        // Indirect Dispatches by definition have the number of thread-groups to launch stored in GPU memory at the
        // specified address.  However, for direct Dispatches, we must allocate some embedded memory to store this
        // information.
        if (indirectGpuVirtAddr == 0uLL) // This is a direct Dispatch.
        {
            *reinterpret_cast<DispatchDims*>(CmdAllocateEmbeddedData(3, 4, &indirectGpuVirtAddr)) = logicalSize;
        }

        pCmdSpace = m_cmdStream.WriteSetSeqShRegs(m_pSignatureCs->numWorkGroupsRegAddr,
                                                  (m_pSignatureCs->numWorkGroupsRegAddr + 1),
                                                  ShaderCompute,
                                                  &indirectGpuVirtAddr,
                                                  pCmdSpace);
    }

#if PAL_DEVELOPER_BUILD
    if (enablePm4Instrumentation)
    {
        const uint32 miscCmdLen = (static_cast<uint32>(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
        m_device.DescribeDrawDispatchValidation(this, userDataCmdLen, miscCmdLen);
    }
#endif

    return pCmdSpace;
}

// =====================================================================================================================
// Performs PAL ABI dispatch-time validation.
uint32* ComputeCmdBuffer::ValidateDispatchHsaAbi(
    DispatchDims        offset,
    const DispatchDims& logicalSize,
    uint32*             pCmdSpace)
{
#if PAL_DEVELOPER_BUILD
    const bool enablePm4Instrumentation = m_device.GetPlatform()->PlatformSettings().pm4InstrumentorEnabled;

    uint32* pStartingCmdSpace = pCmdSpace;
    uint32  userDataCmdLen    = 0;
#endif

    const auto*const pPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);

    // PAL thinks in terms of threadgroups but the HSA ABI thinks in terms of global threads, we need to convert.
    const DispatchDims threads = pPipeline->ThreadsPerGroupXyz();

    offset *= threads;

    // Now we write the required SGPRs. These depend on per-dispatch state so we don't have dirty bit tracking.
    const HsaAbi::CodeObjectMetadata& metadata = pPipeline->HsaMetadata();
    const llvm::amdhsa::kernel_descriptor_t& desc = pPipeline->KernelDescriptor();

    gpusize kernargsGpuVa = 0;
    uint32 ldsSize = metadata.GroupSegmentFixedSize();
    if (TestAnyFlagSet(desc.kernel_code_properties, llvm::amdhsa::KERNEL_CODE_PROPERTY_ENABLE_SGPR_KERNARG_SEGMENT_PTR))
    {
        GfxCmdBuffer::CopyHsaKernelArgsToMem(offset, threads, logicalSize, &kernargsGpuVa, &ldsSize, metadata);
    }

    // If ldsBytesPerTg was specified then that's what LDS_SIZE was programmed to, otherwise we used the fixed size.
    const uint32 boundLdsSize =
        (m_computeState.dynamicCsInfo.ldsBytesPerTg > 0) ? m_computeState.dynamicCsInfo.ldsBytesPerTg
                                                         : metadata.GroupSegmentFixedSize();

    // If our computed total LDS size is larger than the previously bound size we must rewrite it.
    bool mustUpdateLdsSize = false;
    if (boundLdsSize < ldsSize)
    {
        mustUpdateLdsSize = true;

        // We rebound this state. Update its value so that we don't needlessly rewrite it on the
        // next dispatch call.
        m_computeState.dynamicCsInfo.ldsBytesPerTg = ldsSize;
    }

    if (m_computeState.pipelineState.dirtyFlags.pipeline)
    {
        pCmdSpace = pPipeline->WriteCommands<true>(&m_cmdStream,
                                                   pCmdSpace,
                                                   m_computeState.dynamicCsInfo,
                                                   m_buildFlags.prefetchShaders);

        m_pSignatureCs = &pPipeline->Signature();
    }
    else if (mustUpdateLdsSize)
    {
        // If we skipped pPipeline->WriteCommands we still need to rewrite the LDS_SIZE.
        pCmdSpace = pPipeline->WriteUpdatedLdsSize(&m_cmdStream, pCmdSpace, ldsSize);
    }

#if PAL_DEVELOPER_BUILD
    if (enablePm4Instrumentation)
    {
        const uint32 pipelineCmdLen = (static_cast<uint32>(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
        m_device.DescribeBindPipelineValidation(this, pipelineCmdLen);
        pStartingCmdSpace = pCmdSpace;
    }
#endif

    uint32 startReg = mmCOMPUTE_USER_DATA_0;

    // Many HSA ELFs request private segment buffer registers, but never actually use them. Space is reserved to
    // adhere to initialization order but will be unset as we do not support scratch space in this execution path.
    if (TestAnyFlagSet(desc.kernel_code_properties,
                       llvm::amdhsa::KERNEL_CODE_PROPERTY_ENABLE_SGPR_PRIVATE_SEGMENT_BUFFER))
    {
        startReg += 4;
    }

    if (TestAnyFlagSet(desc.kernel_code_properties, llvm::amdhsa::KERNEL_CODE_PROPERTY_ENABLE_SGPR_DISPATCH_PTR))
    {
        const DispatchDims logicalSizeInWorkItems = logicalSize * threads;

        // Fake an AQL dispatch packet for the shader to read.
        gpusize aqlPacketGpu  = 0;
        auto*const pAqlPacket = reinterpret_cast<hsa_kernel_dispatch_packet_t*>(
                                CmdAllocateEmbeddedData(sizeof(hsa_kernel_dispatch_packet_t) / sizeof(uint32),
                                                        1,
                                                        &aqlPacketGpu));

        // Zero everything out then fill in certain fields the shader is likely to read.
        memset(pAqlPacket, 0, sizeof(hsa_kernel_dispatch_packet_t));

        pAqlPacket->workgroup_size_x     = static_cast<uint16>(threads.x);
        pAqlPacket->workgroup_size_y     = static_cast<uint16>(threads.y);
        pAqlPacket->workgroup_size_z     = static_cast<uint16>(threads.z);
        pAqlPacket->grid_size_x          = logicalSizeInWorkItems.x;
        pAqlPacket->grid_size_y          = logicalSizeInWorkItems.y;
        pAqlPacket->grid_size_z          = logicalSizeInWorkItems.z;
        pAqlPacket->private_segment_size = metadata.PrivateSegmentFixedSize();
        pAqlPacket->group_segment_size   = ldsSize;

        pCmdSpace = m_cmdStream.WriteSetSeqShRegs(startReg,
                                                  (startReg + 1),
                                                  ShaderCompute,
                                                  &aqlPacketGpu,
                                                  pCmdSpace);
        startReg += 2;
    }

    // When kernels request queue ptr, for COV4 (Code Object Version 4) and earlier, ENABLE_SGPR_QUEUE_PTR is set,
    // which means that the queue ptr is passed in two SGPRs, for COV5 and later, ENABLE_SGPR_QUEUE_PTR is deprecated
    // and HiddenQueuePtr is set, which means that the queue ptr is passed in hidden kernel arguments.
    // When there are indirect function call, such as virtual functions, HSA ABI compiler makes the optimization pass
    // unable to infer if queue ptr will be used or not. As a result, the pass has to assume the queue ptr
    // might be used, so HSA ELFs request queue ptrs but never actually use them. SGPR Space is reserved to adhere to
    // initialization order for COV4 when ENABLE_SGPR_QUEUE_PTR is set, but is unset as we can't support queue ptr.
    if (TestAnyFlagSet(desc.kernel_code_properties, llvm::amdhsa::KERNEL_CODE_PROPERTY_ENABLE_SGPR_QUEUE_PTR))
    {
        startReg += 2;
    }

    if (TestAnyFlagSet(desc.kernel_code_properties, llvm::amdhsa::KERNEL_CODE_PROPERTY_ENABLE_SGPR_KERNARG_SEGMENT_PTR))
    {
        pCmdSpace = m_cmdStream.WriteSetSeqShRegs(startReg,
                                                  (startReg + 1),
                                                  ShaderCompute,
                                                  &kernargsGpuVa,
                                                  pCmdSpace);
        startReg += 2;
    }

    if (TestAnyFlagSet(desc.kernel_code_properties, llvm::amdhsa::KERNEL_CODE_PROPERTY_ENABLE_SGPR_DISPATCH_ID))
    {
        // This feature may be enabled as a side effect of indirect calls.
        // However, the compiler team confirmed that the dispatch id itself is not used,
        // so safe to send 0 for each dispatch.
        constexpr uint32 DispatchId[2] = {};
        pCmdSpace = m_cmdStream.WriteSetSeqShRegs(startReg,
                                                  (startReg + 1),
                                                  ShaderCompute,
                                                  &DispatchId,
                                                  pCmdSpace);
        startReg += 2;
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    regCOMPUTE_PGM_RSRC2 computePgmRsrc2 = {};
    computePgmRsrc2.u32All = desc.compute_pgm_rsrc2;

    PAL_ASSERT((startReg - mmCOMPUTE_USER_DATA_0) <= computePgmRsrc2.bitfields.USER_SGPR);
#endif

#if PAL_DEVELOPER_BUILD
    if (enablePm4Instrumentation)
    {
        userDataCmdLen    = (static_cast<uint32>(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
        pStartingCmdSpace = pCmdSpace;
    }
#endif

    m_computeState.pipelineState.dirtyFlags.u32All = 0;

    // We don't expect HSA ABI pipelines to use these.
    PAL_ASSERT(m_pSignatureCs->numWorkGroupsRegAddr == UserDataNotMapped);

#if PAL_DEVELOPER_BUILD
    if (enablePm4Instrumentation)
    {
        const uint32 miscCmdLen = (static_cast<uint32>(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
        m_device.DescribeDrawDispatchValidation(this, userDataCmdLen, miscCmdLen);
    }
#endif

    return pCmdSpace;
}

// =====================================================================================================================
// Helper function responsible for handling user-SGPR updates during Dispatch-time validation when the active pipeline
// has changed since the previous Dispatch operation. It is expected that this will be called only when the pipeline
// is changing and immediately before a call to WriteUserDataEntriesToSgprs<false, ..>().
bool ComputeCmdBuffer::FixupUserSgprsOnPipelineSwitch(
    const UserDataEntries&          userData,
    const ComputePipelineSignature* pPrevSignature,
    uint32**                        ppCmdSpace)
{
    PAL_ASSERT(pPrevSignature != nullptr);

    // The WriteUserDataEntriesToSgprs() method only writes entries which are mapped to user-SGPR's and have
    // been marked dirty.  When the active pipeline is changing, the set of entries mapped to user-SGPR's can change
    // and which entries are mapped to which registers can also change. The simplest way to handle this is to write
    // all mapped user-SGPR's whose mappings are changing.  If mappings are not changing it will be handled through
    // the normal "pipeline not changing" path.

    bool written = false;
    uint32* pCmdSpace = (*ppCmdSpace);

    if (m_pSignatureCs->userDataHash != pPrevSignature->userDataHash)
    {
        pCmdSpace = m_cmdStream.WriteUserDataEntriesToSgprs<true, ShaderCompute>(m_pSignatureCs->stage,
                                                                                 userData,
                                                                                 pCmdSpace);

        written = true;
        (*ppCmdSpace) = pCmdSpace;
    }

    return written;
}

// =====================================================================================================================
// Adds PM4 commands needed to write any registers associated with starting a query.
void ComputeCmdBuffer::AddQuery(
    QueryPoolType     queryPoolType,
    QueryControlFlags flags)
{
    // Compute command buffers only support pipeline stat queries.
    PAL_ASSERT(queryPoolType == QueryPoolType::PipelineStats);

    // PIPELINE_START event may not have been issued in the preamble, so do this for safety.
    if (IsFirstQuery(queryPoolType))
    {
        // This adds the required active query.
    }
}

// =====================================================================================================================
// Adds PM4 commands needed to write any registers associated with ending the last active query in this command buffer.
void ComputeCmdBuffer::RemoveQuery(
    QueryPoolType queryPoolType)
{
    // Compute command buffers only support pipeline stat queries.
    PAL_ASSERT(queryPoolType == QueryPoolType::PipelineStats);

    // Acknowledge this PIPELINE_STOP event.
    if (IsLastActiveQuery(queryPoolType))
    {
        // This will remove the active query as required.
    }
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdBeginQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot,
    QueryControlFlags flags)
{
    static_cast<const QueryPool&>(queryPool).Begin(this, &m_cmdStream, nullptr, queryType, slot, flags);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdEndQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot)
{
    static_cast<const QueryPool&>(queryPool).End(this, &m_cmdStream, nullptr, queryType, slot);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdResetQueryPool(
    const IQueryPool& queryPool,
    uint32            startQuery,
    uint32            queryCount)
{
    static_cast<const QueryPool&>(queryPool).DoGpuReset(this, &m_cmdStream, startQuery, queryCount);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdIf(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    // Nested command buffers don't support control flow yet.
    PAL_ASSERT(IsNested() == false);

    m_cmdStream.If(compareFunc, gpuMemory.Desc().gpuVirtAddr + offset, data, mask);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdElse()
{
    // Nested command buffers don't support control flow yet.
    PAL_ASSERT(IsNested() == false);

    m_cmdStream.Else();
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdEndIf()
{
    // Nested command buffers don't support control flow yet.
    PAL_ASSERT(IsNested() == false);

    m_cmdStream.EndIf();
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdWhile(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    // Nested command buffers don't support control flow yet.
    PAL_ASSERT(IsNested() == false);

    m_cmdStream.While(compareFunc, gpuMemory.Desc().gpuVirtAddr + offset, data, mask);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdEndWhile()
{
    // Nested command buffers don't support control flow yet.
    PAL_ASSERT(IsNested() == false);

    m_cmdStream.EndWhile();
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdCopyRegisterToMemory(
    uint32            srcRegisterOffset,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    DmaDataInfo dmaData = {};
    dmaData.dstSel       = dst_sel__pfp_dma_data__dst_addr_using_das;
    dmaData.dstAddr      = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;
    dmaData.dstAddrSpace = das__pfp_dma_data__memory;
    dmaData.srcSel       = src_sel__pfp_dma_data__src_addr_using_sas;
    dmaData.srcAddr      = srcRegisterOffset;
    dmaData.srcAddrSpace = sas__pfp_dma_data__register;
    dmaData.sync         = true;
    dmaData.usePfp       = false;
    pCmdSpace += m_cmdUtil.BuildDmaData<false, false>(dmaData, pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdWaitRegisterValue(
    uint32      registerOffset,
    uint32      data,
    uint32      mask,
    CompareFunc compareFunc)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace += m_cmdUtil.BuildWaitRegMem(EngineTypeCompute,
                                           mem_space__me_wait_reg_mem__register_space,
                                           CmdUtil::WaitRegMemFunc(compareFunc),
                                           engine_sel__me_wait_reg_mem__micro_engine,
                                           registerOffset,
                                           data,
                                           mask,
                                           pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdWaitMemoryValue(
    gpusize     gpuVirtAddr,
    uint32      data,
    uint32      mask,
    CompareFunc compareFunc)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace += m_cmdUtil.BuildWaitRegMem(EngineTypeCompute,
                                           mem_space__me_wait_reg_mem__memory_space,
                                           CmdUtil::WaitRegMemFunc(compareFunc),
                                           engine_sel__me_wait_reg_mem__micro_engine,
                                           gpuVirtAddr,
                                           data,
                                           mask,
                                           pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdWaitBusAddressableMemoryMarker(
    const IGpuMemory& gpuMemory,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    const GpuMemory* pGpuMemory = static_cast<const GpuMemory*>(&gpuMemory);
    CmdWaitMemoryValue(pGpuMemory->GetBusAddrMarkerVa(), data, mask, compareFunc);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdCommentString(
    const char* pComment)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace += m_cmdUtil.BuildCommentString(pComment, ShaderCompute, pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
size_t ComputeCmdBuffer::BuildWriteToZero(
    gpusize       dstAddr,
    uint32        numDwords,
    const uint32* pZeros,
    uint32*       pCmdSpace
    ) const
{
    WriteDataInfo info = {};
    info.engineType = EngineTypeCompute;
    info.dstAddr    = dstAddr;
    info.dstSel     = dst_sel__mec_write_data__memory;

    return CmdUtil::BuildWriteData(info, numDwords, pZeros, pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdNop(
    const void* pPayload,
    uint32      payloadSize)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    pCmdSpace += m_cmdUtil.BuildNopPayload(pPayload, payloadSize, pCmdSpace);
    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Helper method for handling the state "leakage" from a nested command buffer back to its caller. Since the callee has
// tracked its own state during the building phase, we can access the final state of the command buffer since its stored
// in the UniversalCmdBuffer object itself.
void ComputeCmdBuffer::LeakNestedCmdBufferState(
    const ComputeCmdBuffer& callee)
{
    Pal::ComputeCmdBuffer::LeakNestedCmdBufferState(callee);

    m_ringSizeComputeScratch = Max(callee.m_ringSizeComputeScratch, m_ringSizeComputeScratch);

    // Invalidate PM4 optimizer state on post-execute since the current command buffer state does not reflect
    // state changes from the nested command buffer. We will need to resolve the nested PM4 state onto the
    // current command buffer for this to work correctly.
    m_cmdStream.NotifyNestedCmdBufferExecute();
}

// =====================================================================================================================
// Adds a preamble to the start of a new command buffer.
// SEE: ComputePreamblePm4Img and CommonPreamblePm4Img structures in gfx9Preambles.h for what is written in the preamble
void ComputeCmdBuffer::WritePreambleCommands(
    const CmdUtil& cmdUtil,
    CmdStream*     pCmdStream)
{
    // If this trips, it means that this isn't really the preamble -- i.e., somebody has inserted something into the
    // command stream before the preamble.  :-(
    PAL_ASSERT(pCmdStream->IsEmpty());

    uint32* pCmdSpace = pCmdStream->ReserveCommands();
    pCmdSpace += cmdUtil.BuildNonSampleEventWrite(PIPELINESTAT_START, EngineTypeCompute, pCmdSpace);
    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Adds a preamble to the start of a new command buffer.
void ComputeCmdBuffer::AddPreamble()
{
    ComputeCmdBuffer::WritePreambleCommands(m_cmdUtil, &m_cmdStream);

}

// =====================================================================================================================
// Adds a postamble to the end of a new command buffer.
void ComputeCmdBuffer::WritePostambleCommands(
    const CmdUtil&     cmdUtil,
    GfxCmdBuffer*const pCmdBuffer,
    CmdStream*         pCmdStream)
{
    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    if (pCmdBuffer->GetCmdBufState().flags.cpBltActive)
    {
        // Stalls the CP MEC until the CP's DMA engine has finished all previous "CP blts" (DMA_DATA commands
        // without the sync bit set). The ring won't wait for CP DMAs to finish so we need to do this manually.
        pCmdSpace += cmdUtil.BuildWaitDmaData(pCmdSpace);
        pCmdBuffer->SetCpBltState(false);
    }

    // The following ATOMIC_MEM packet increments the done-count for the command stream, so that we can probe when the
    // command buffer has completed execution on the GPU.
    // NOTE: Normally, we would need to flush the L2 cache to guarantee that this memory operation makes it out to
    // memory. However, since we're at the end of the command buffer, we can rely on the fact that the KMD inserts
    // an EOP event which flushes and invalidates the caches in between command buffers.
    if (pCmdStream->GetFirstChunk()->BusyTrackerGpuAddr() != 0)
    {
        // We also need a wait-for-idle before the atomic increment because command memory might be read or written
        // by dispatches. If we don't wait for idle then the driver might reset and write over that memory before the
        // shaders are done executing.
        const gpusize tsGpuVa = pCmdBuffer->GetWaitIdleTsGpuVa(&pCmdSpace);
        pCmdSpace += cmdUtil.BuildWaitCsIdle(EngineTypeCompute,
                                             tsGpuVa,
                                             pCmdBuffer->WaitIdleTsValue(),
                                             true,
                                             pCmdSpace);
        pCmdSpace += cmdUtil.BuildAtomicMem(AtomicOp::AddInt32,
                                            pCmdStream->GetFirstChunk()->BusyTrackerGpuAddr(),
                                            1,
                                            pCmdSpace);
    }

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Adds a postamble to the end of a new command buffer.
void ComputeCmdBuffer::AddPostamble()
{
    if ((m_globalInternalTableAddr != 0) &&
        (m_computeState.pipelineState.pPipeline != nullptr) &&
        (static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline)->GetInfo().flags.hsaAbi != 0u))
    {
        // If we're ending this cmdbuf with an HSA pipeline bound, the global table may currently
        // be invalid and we need to restore it for any subsequent chained cmdbufs.
        // Note 'nullptr' is considered PAL ABI and the restore must have already happened if needed.
        uint32* pCmdSpace = m_cmdStream.ReserveCommands();
        pCmdSpace += m_cmdUtil.BuildLoadShRegsIndex(index__pfp_load_sh_reg_index__direct_addr,
                                                    data_format__pfp_load_sh_reg_index__offset_and_size,
                                                    m_globalInternalTableAddr,
                                                    mmCOMPUTE_USER_DATA_0,
                                                    1,
                                                    Pm4ShaderType::ShaderCompute,
                                                    pCmdSpace);
        m_cmdStream.CommitCommands(pCmdSpace);
    }

    ComputeCmdBuffer::WritePostambleCommands(m_cmdUtil, this, &m_cmdStream);

}

// =====================================================================================================================
// Enables the specified query type.
void ComputeCmdBuffer::ActivateQueryType(
    QueryPoolType queryPoolType)
{
    // Compute command buffers only support pipeline stat queries.
    PAL_ASSERT(queryPoolType == QueryPoolType::PipelineStats);

    GfxCmdBuffer::ActivateQueryType(queryPoolType);

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PIPELINESTAT_START, EngineTypeCompute, pCmdSpace);
    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Disables the specified query type.
void ComputeCmdBuffer::DeactivateQueryType(
    QueryPoolType queryPoolType)
{
    // Compute command buffers only support pipeline stat queries.
    PAL_ASSERT(queryPoolType == QueryPoolType::PipelineStats);

    GfxCmdBuffer::DeactivateQueryType(queryPoolType);

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PIPELINESTAT_STOP, EngineTypeCompute, pCmdSpace);
    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Adds commands necessary to write "data" to the specified event's memory.
void ComputeCmdBuffer::WriteEventCmd(
    const BoundGpuMemory& boundMemObj,
    uint32                stageMask,   // Bitmask of PipelineStageFlag
    uint32                data)
{
    // This will replace PipelineStageBlt with a more specific set of flags if we haven't done any CP DMAs.
    m_barrierMgr.OptimizeStageMask(this, BarrierType::Global, &stageMask, nullptr);

    uint32*    pCmdSpace           = m_cmdStream.ReserveCommands();
    const bool issueReleaseMem     = TestAnyFlagSet(stageMask, PipelineStageCs | PipelineStageBottomOfPipe);
    bool       releaseMemWaitCpDma = false;

    // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
    // this function expect that the prior blts have completed by the time the event is written to memory.
    // Given that our CP DMA blts are asynchronous to the pipeline stages the only way to satisfy this requirement
    // is to force the ME to stall until the CP DMAs are completed.
    if (GfxBarrierMgr::NeedWaitCpDma(this, stageMask))
    {
        releaseMemWaitCpDma = issueReleaseMem && m_device.EnableReleaseMemWaitCpDma();
        if (releaseMemWaitCpDma == false)
        {
            pCmdSpace += CmdUtil::BuildWaitDmaData(pCmdSpace);
        }
        SetCpBltState(false);
    }

    // Now pick the packet that actually writes to the event. If multiple flags are set we must go down the path that
    // is most conservative (sets the event at the latest point). This is easiest to implement in this order:
    // 1. The EOP/EOS path for compute shaders.
    // 2. Any other stages must be implemented by the MEC so just do a direct write.
    // Note that passing in a stageMask of zero will get you an MEC write. It's not clear if that is even legal but
    // doing an MEC write is probably the least impactful thing we could do in that case.
    if (issueReleaseMem)
    {
        // Implement set/reset with an EOP event written when all prior GPU work completes. Note that waiting on an
        // EOS timestamp and waiting on an EOP timestamp are exactly equivalent on compute queues. There's no reason
        // to implement a CS_DONE path for HwPipePostCs.
        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.gfx11WaitCpDma = releaseMemWaitCpDma;
        releaseInfo.dstAddr        = boundMemObj.GpuVirtAddr();
        releaseInfo.dataSel        = data_sel__mec_release_mem__send_32_bit_low;
        releaseInfo.data           = data;

        pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, pCmdSpace);
    }
    else
    {
        // Implement set/reset event with a WRITE_DATA command using the CP.
        WriteDataInfo writeData = {};
        writeData.engineType = EngineTypeCompute;
        writeData.dstAddr    = boundMemObj.GpuVirtAddr();
        writeData.dstSel     = dst_sel__mec_write_data__memory;

        pCmdSpace += m_cmdUtil.BuildWriteData(writeData, data, pCmdSpace);
    }

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Enables or disables a flexible predication check which the CP uses to determine if a draw or dispatch can be skipped
// based on the results of prior GPU work.
// SEE: CmdUtil::BuildSetPredication(...) for more details on the meaning of this method's parameters.
// Note that this function is currently only implemented for memory-based/DX12 predication
void ComputeCmdBuffer::CmdSetPredication(
    IQueryPool*         pQueryPool,
    uint32              slot,
    const IGpuMemory*   pGpuMemory,
    gpusize             offset,
    PredicateType       predType,
    bool                predPolarity,
    bool                waitResults,
    bool                accumulateData)
{
    // This emulation doesn't work for QueryPool based predication, fortuanately DX12 just has Boolean type
    // predication. TODO: emulation for Zpass and Streamout predication if they are really used on compute.
    PAL_ASSERT(pQueryPool == nullptr);
    PAL_ASSERT((predType == PredicateType::Boolean64) || (predType == PredicateType::Boolean32));

    // When gpuVirtAddr is 0, it means client is disabling/resetting predication
    m_cmdBufState.flags.clientPredicate = (pGpuMemory != nullptr);
    m_cmdBufState.flags.packetPredicate = m_cmdBufState.flags.clientPredicate;

    if (pGpuMemory != nullptr)
    {
        gpusize gpuVirtAddr  = pGpuMemory->Desc().gpuVirtAddr + offset;
        uint32* pPredCpuAddr = CmdAllocateEmbeddedData(1, 1, &m_predGpuAddr);
        uint32* pCmdSpace    = m_cmdStream.ReserveCommands();

        // Execute if 64-bit value in memory are all 0 when predPolarity is false,
        // or Execute if one or more bits of 64-bit value in memory are not 0 when predPolarity is true.
        uint32 predCopyData  = (predPolarity == true);
        *pPredCpuAddr        = (predPolarity == false);

        WriteDataInfo writeData = {};
        writeData.engineType = EngineTypeCompute;
        writeData.dstAddr    = m_predGpuAddr;
        writeData.dstSel     = dst_sel__mec_write_data__memory;

        pCmdSpace += m_cmdUtil.BuildCondExec(gpuVirtAddr, CmdUtil::WriteDataSizeDwords + 1, pCmdSpace);
        pCmdSpace += m_cmdUtil.BuildWriteData(writeData, predCopyData, pCmdSpace);

        if (predType == PredicateType::Boolean64)
        {
            pCmdSpace += m_cmdUtil.BuildCondExec(gpuVirtAddr + 4, CmdUtil::WriteDataSizeDwords + 1, pCmdSpace);
            pCmdSpace += m_cmdUtil.BuildWriteData(writeData, predCopyData, pCmdSpace);
        }

        m_cmdStream.CommitCommands(pCmdSpace);
    }
    else
    {
        m_predGpuAddr = 0;
    }
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdExecuteIndirectCmds(
    const IIndirectCmdGenerator& generator,
    gpusize                      gpuVirtAddr,
    uint32                       maximumCount,
    gpusize                      countGpuAddr)
{
    // It is only safe to generate indirect commands on a one-time-submit or exclusive-submit command buffer because
    // there is a potential race condition on the memory used to receive the generated commands.
    PAL_ASSERT(IsOneTimeSubmit() || IsExclusiveSubmit());

    if (m_predGpuAddr != 0)
    {
        m_cmdStream.If(CompareFunc::Equal, m_predGpuAddr, 1, UINT_MAX);
    }

    const auto& gfx9Generator = static_cast<const IndirectCmdGenerator&>(generator);

    if (m_describeDispatch)
    {
        m_device.DescribeDispatch(this,
                                  {},
                                  Developer::DrawDispatchType::CmdGenExecuteIndirectDispatch,
                                  {},
                                  {},
                                  {},
                                  {});
    }

    if (countGpuAddr == 0uLL)
    {
        // If the count GPU address is zero, then we are expected to use the maximumCount value as the actual number
        // of indirect commands to generate and execute.
        uint32* pMemory = CmdAllocateEmbeddedData(1, 1, &countGpuAddr);
        *pMemory = maximumCount;
    }

    AutoBuffer<CmdStreamChunk*, 16, Platform> deChunks(maximumCount, m_device.GetPlatform());

    if (deChunks.Capacity() < maximumCount)
    {
        NotifyAllocFailure();
    }
    else
    {
        CmdStreamChunk** ppChunkList[] =
        {
            deChunks.Data(),
        };
        uint32 numGenChunks = 0;

        // Generate the indirect command buffer chunk(s) using RPM. Since we're wrapping the command generation and
        // execution inside a CmdIf, we want to disable normal predication for this blit.
        const uint32 packetPredicate = m_cmdBufState.flags.packetPredicate;
        m_cmdBufState.flags.packetPredicate = 0;

        constexpr uint32 DummyIndexBufSize = 0; // Compute doesn't care about the index buffer size.
        const IndirectCmdGenerateInfo genInfo =
        {
            this,
            m_computeState.pipelineState.pPipeline,
            gfx9Generator,
            DummyIndexBufSize,
            maximumCount,
            gpuVirtAddr,
            countGpuAddr
        };

        m_device.RsrcProcMgr().CmdGenerateIndirectCmds(genInfo, &ppChunkList[0], &numGenChunks);

        m_cmdBufState.flags.packetPredicate = packetPredicate;

        uint32* pCmdSpace = m_cmdStream.ReserveCommands();

        // Insert a wait-for-idle to make sure that the generated commands are written out to L2 before we attempt to
        // execute them.
        AcquireMemGeneric acquireInfo = {};
        acquireInfo.engineType = EngineTypeCompute;
        acquireInfo.cacheSync  = SyncGlkInv;

        pCmdSpace  = WriteWaitCsIdle(pCmdSpace);
        pCmdSpace += m_cmdUtil.BuildAcquireMemGeneric(acquireInfo, pCmdSpace);

        // PFP_SYNC_ME cannot be used on an async compute engine so we need to use REWIND packet instead.
        pCmdSpace += m_cmdUtil.BuildRewind(false, true, pCmdSpace);

        // Just like a normal direct/indirect dispatch, we need to perform state validation before executing the
        // generated command chunks.
        pCmdSpace = ValidateDispatchPalAbi(0uLL, {}, pCmdSpace);

        m_cmdStream.CommitCommands(pCmdSpace);

        CommandGeneratorTouchedUserData(m_computeState.csUserDataEntries.touched, gfx9Generator, *m_pSignatureCs);

        // NOTE: The command stream expects an iterator to the first chunk to execute, but this iterator points to the
        // place in the list before the first generated chunk (see comments above).
        m_cmdStream.ExecuteGeneratedCommands(ppChunkList[0], 0, numGenChunks);
    }

    if (m_predGpuAddr != 0)
    {
        m_cmdStream.EndIf();
    }
}

// =====================================================================================================================
void ComputeCmdBuffer::GetChunkForCmdGeneration(
    const Pal::IndirectCmdGenerator& generator,
    const Pal::Pipeline&             pipeline,
    uint32                           maxCommands,
    uint32                           numChunkOutputs,
    ChunkOutput*                     pChunkOutputs)
{
    const GeneratorProperties&      properties = generator.Properties();
    const ComputePipelineSignature& signature  = static_cast<const ComputePipeline&>(pipeline).Signature();

    PAL_ASSERT(m_pCmdAllocator != nullptr);
    PAL_ASSERT(numChunkOutputs == 1);

    CmdStreamChunk*const pChunk = GfxCmdBuffer::GetNextGeneratedChunk();
    pChunkOutputs->pChunk = pChunk;

    // NOTE: RPM uses a compute shader to generate indirect commands, so we need to use the saved user-data state
    // because RPM will have pushed its own state before calling this method.
    const uint32* pUserDataEntries = &m_computeRestoreState.csUserDataEntries.entries[0];

    // User-data high watermark for this command Generator. It depends on the command Generator itself, as well as the
    // pipeline signature for the active pipeline. This is due to the fact that if the command Generator modifies the
    // contents of an indirect user-data table, the command Generator must also fix-up the user-data entry used for the
    // table's GPU virtual address.
    uint32 userDataWatermark = properties.userDataWatermark;

    const uint32 commandDwords = (generator.Properties().cmdBufStride / sizeof(uint32));
    // There are three possibilities when determining how much spill-table space a generated command will need:
    //  (1) The active pipeline doesn't spill at all. This requires no spill-table space.
    //  (2) The active pipeline spills, but the generator doesn't update the any user-data entries beyond the
    //      spill threshold. This requires no spill-table space.
    //  (3) The active pipeline spills, and the generator updates user-data entries which are beyond the spill
    //      threshold. This means each generated command needs to relocate the spill table in addition to the other
    //      stuff it would normally do.
    const uint32 spillDwords =
        (signature.spillThreshold < properties.userDataWatermark) ? properties.maxUserDataEntries : 0;
    // Total amount of embedded data space needed for each generated command, including indirect user-data tables and
    // user-data spilling.
    uint32 embeddedDwords = spillDwords;

    // Ask the DE command stream to make sure the command chunk is ready to receive GPU-generated commands (this
    // includes setting up padding for size alignment, allocating command space, etc.
    (pChunkOutputs->commandsInChunk)  = m_cmdStream.PrepareChunkForCmdGeneration(pChunk,
                                                                                 commandDwords,
                                                                                 embeddedDwords,
                                                                                 maxCommands);
    (pChunkOutputs->embeddedDataSize) = ((pChunkOutputs->commandsInChunk) * embeddedDwords);

    // Populate command buffer chain size required later for an indirect command generation optimization.
    (pChunkOutputs->chainSizeInDwords) = CmdUtil::ChainSizeInDwords(EngineTypeCompute);

    if (spillDwords > 0)
    {
        // If each generated command requires some amount of spill-table space, then we need to allocate embeded data
        // space for all of the generated commands which will go into this chunk. PrepareChunkForCmdGeneration() should
        // have determined a value for commandsInChunk which allows us to allocate the appropriate amount of embeded
        // data space.
        uint32* pDataSpace = pChunk->ValidateCmdGenerationDataSpace(pChunkOutputs->embeddedDataSize,
                                                                    &(pChunkOutputs->embeddedDataAddr));

        // We also need to seed the embedded data for each generated command with the current indirect user-data table
        // and spill-table contents, because the generator will only update the table entries which get modified.
        for (uint32 cmd = 0; cmd < (pChunkOutputs->commandsInChunk); ++cmd)
        {
            memcpy(pDataSpace, pUserDataEntries, (sizeof(uint32) * spillDwords));
            pDataSpace += spillDwords;
        }
    }
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdPrimeGpuCaches(
    uint32                    rangeCount,
    const PrimeGpuCacheRange* pRanges)
{
    PAL_ASSERT((rangeCount == 0) || (pRanges != nullptr));

    for (uint32 i = 0; i < rangeCount; ++i)
    {
        uint32* pCmdSpace = m_cmdStream.ReserveCommands();

        pCmdSpace += m_cmdUtil.BuildPrimeGpuCaches(pRanges[i], EngineTypeCompute, pCmdSpace);

        m_cmdStream.CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32            cmdBufferCount,
    ICmdBuffer*const* ppCmdBuffers)
{
    for (uint32 buf = 0; buf < cmdBufferCount; ++buf)
    {
        auto*const pCallee = static_cast<Gfx9::ComputeCmdBuffer*>(ppCmdBuffers[buf]);
        PAL_ASSERT(pCallee != nullptr);

        if ((pCallee->m_inheritedPredGpuAddr != 0uLL) && (m_predGpuAddr != 0uLL))
        {
            uint32* pCmdSpace = m_cmdStream.ReserveCommands();

            pCmdSpace += m_cmdUtil.BuildCopyData(EngineTypeCompute,
                                                 0,
                                                 dst_sel__mec_copy_data__tc_l2,
                                                 pCallee->m_inheritedPredGpuAddr,
                                                 src_sel__mec_copy_data__tc_l2,
                                                 m_predGpuAddr,
                                                 count_sel__mec_copy_data__32_bits_of_data,
                                                 wr_confirm__mec_copy_data__wait_for_confirmation,
                                                 pCmdSpace);

            m_cmdStream.CommitCommands(pCmdSpace);
        }

        // Track the most recent OS paging fence value across all nested command buffers called from this one.
        m_lastPagingFence = Max(m_lastPagingFence, pCallee->LastPagingFence());

        // Track the lastest fence token across all nested command buffers called from this one.
        m_maxUploadFenceToken = Max(m_maxUploadFenceToken, pCallee->GetMaxUploadFenceToken());

        // All user-data entries have been uploaded into the GPU memory the callee expects to receive them in, so we
        // can safely "call" the nested command buffer's command stream.
        m_cmdStream.TrackNestedEmbeddedData(pCallee->m_embeddedData.chunkList);
        m_cmdStream.TrackNestedEmbeddedData(pCallee->m_gpuScratchMem.chunkList);
        m_cmdStream.TrackNestedCommands(pCallee->m_cmdStream);
        m_cmdStream.Call(pCallee->m_cmdStream, pCallee->IsExclusiveSubmit(), false);

        // Callee command buffers are also able to leak any changes they made to bound user-data entries and any other
        // state back to the caller.
        LeakNestedCmdBufferState(*pCallee);
    }
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdInsertTraceMarker(
    PerfTraceMarkerType markerType,
    uint32              markerData)
{
    const uint32 userDataAddr =
        (markerType == PerfTraceMarkerType::SqttA) ? mmSQ_THREAD_TRACE_USERDATA_2 : mmSQ_THREAD_TRACE_USERDATA_3;

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    pCmdSpace = m_cmdStream.WriteSetOneConfigReg(userDataAddr, markerData, pCmdSpace);
    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdInsertRgpTraceMarker(
    RgpMarkerSubQueueFlags subQueueFlags,
    uint32                 numDwords,
    const void*            pData)
{
    PAL_ASSERT((subQueueFlags.includeMainSubQueue == 1) && (subQueueFlags.includeGangedSubQueues == 0));

    // The first dword of every RGP trace marker packet is written to SQ_THREAD_TRACE_USERDATA_2.  The second dword
    // is written to SQ_THREAD_TRACE_USERDATA_3.  For packets longer than 64-bits, continue alternating between
    // user data 2 and 3.
    static_assert(mmSQ_THREAD_TRACE_USERDATA_3 == mmSQ_THREAD_TRACE_USERDATA_2 + 1, "Registers not sequential!");

    const uint32* pDwordData = static_cast<const uint32*>(pData);
    while (numDwords > 0)
    {
        const uint32 dwordsToWrite = Min(numDwords, 2u);

        // Reserve and commit command space inside this loop.  Some of the RGP packets are unbounded, like adding a
        // comment string, so it's not safe to assume the whole packet will fit under our reserve limit.
        uint32* pCmdSpace = m_cmdStream.ReserveCommands();

        pCmdSpace = m_cmdStream.WriteSetSeqConfigRegs(mmSQ_THREAD_TRACE_USERDATA_2,
                                                      mmSQ_THREAD_TRACE_USERDATA_2 + dwordsToWrite - 1,
                                                      pDwordData,
                                                      pCmdSpace);
        pDwordData += dwordsToWrite;
        numDwords -= dwordsToWrite;

        m_cmdStream.CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Updates the SQTT token mask for all SEs outside of a specific PerfExperiment.  Used by GPA Session when targeting
// a single event for instruction level trace during command buffer building.
void ComputeCmdBuffer::CmdUpdateSqttTokenMask(
    const ThreadTraceTokenConfig& sqttTokenConfig)
{
    PerfExperiment::UpdateSqttTokenMaskStatic(&m_cmdStream, sqttTokenConfig, m_device);
}

// =====================================================================================================================
// Copy memory using the CP's DMA engine
void ComputeCmdBuffer::CopyMemoryCp(
    gpusize dstAddr,
    gpusize srcAddr,
    gpusize numBytes)
{
    DmaDataInfo dmaDataInfo = {};
    dmaDataInfo.dstSel      = dst_sel__pfp_dma_data__dst_addr_using_l2;
    dmaDataInfo.srcSel      = src_sel__pfp_dma_data__src_addr_using_l2;
    dmaDataInfo.sync        = false;
    dmaDataInfo.dstAddr     = dstAddr;
    dmaDataInfo.srcAddr     = srcAddr;

    while (numBytes > 0)
    {
        // The numBytes arg is a gpusize so we must upcast, clamp against MaxDmaDataByteCount, then safely downcast.
        dmaDataInfo.numBytes = uint32(Min(numBytes, gpusize(CmdUtil::MaxDmaDataByteCount)));

        uint32* pCmdSpace = m_cmdStream.ReserveCommands();
        if (m_cmdBufState.flags.packetPredicate != 0)
        {
            pCmdSpace += m_cmdUtil.BuildCondExec(m_predGpuAddr, CmdUtil::DmaDataSizeDwords, pCmdSpace);
        }
        pCmdSpace += m_cmdUtil.BuildDmaData<false, false>(dmaDataInfo, pCmdSpace);
        m_cmdStream.CommitCommands(pCmdSpace);

        dmaDataInfo.dstAddr += dmaDataInfo.numBytes;
        dmaDataInfo.srcAddr += dmaDataInfo.numBytes;
        numBytes            -= dmaDataInfo.numBytes;
    }

    SetCpBltState(true);
    SetCpBltWriteCacheState(true);

#if PAL_DEVELOPER_BUILD
    Developer::RpmBltData cbData = { .pCmdBuffer = this, .bltType = Developer::RpmBltType::CpDmaCopy };
    m_device.Parent()->DeveloperCb(Developer::CallbackType::RpmBlt, &cbData);
#endif
}

// =====================================================================================================================
uint32* ComputeCmdBuffer::WriteWaitEop(
    WriteWaitEopInfo info,
    uint32*          pCmdSpace)
{
    SyncGlxFlags       glxSync   = SyncGlxFlags(info.hwGlxSync);
    const AcquirePoint acqPoint  = AcquirePoint(info.hwAcqPoint);
    const bool         waitCpDma = info.waitCpDma;

    // Can optimize acquire at Eop case if hit it.
    PAL_ASSERT(acqPoint != AcquirePointEop);
    PAL_ASSERT(info.hwRbSync == SyncRbNone);

    // Issue explicit waitCpDma packet if ReleaseMem doesn't support it.
    bool releaseMemWaitCpDma = waitCpDma;
    if (waitCpDma && (m_device.Settings().gfx11EnableReleaseMemWaitCpDma == false))
    {
        pCmdSpace += m_cmdUtil.BuildWaitDmaData(pCmdSpace);
        releaseMemWaitCpDma = false;
    }

    // We prefer to do our GCR in the release_mem if we can. This function always does an EOP wait so we don't have
    // to worry about release_mem not supporting GCRs with EOS events. Any remaining sync flags must be handled in a
    // trailing acquire_mem packet.
    ReleaseMemGeneric releaseInfo = {};
    releaseInfo.cacheSync      = m_cmdUtil.SelectReleaseMemCaches(&glxSync);
    releaseInfo.dataSel        = data_sel__me_release_mem__send_32_bit_low;
    releaseInfo.dstAddr        = GetWaitIdleTsGpuVa(&pCmdSpace);
    releaseInfo.data           = WaitIdleTsValue();
    releaseInfo.gfx11WaitCpDma = releaseMemWaitCpDma;

    pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, pCmdSpace);

    pCmdSpace += m_cmdUtil.BuildWaitRegMem(EngineTypeCompute,
                                           mem_space__me_wait_reg_mem__memory_space,
                                           function__me_wait_reg_mem__equal_to_the_reference_value,
                                           engine_sel__me_wait_reg_mem__micro_engine,
                                           releaseInfo.dstAddr,
                                           uint32(releaseInfo.data),
                                           UINT32_MAX,
                                           pCmdSpace);

    // If we still have some caches to sync we require a final acquire_mem. It doesn't do any waiting, it just
    // immediately does some full-range cache flush and invalidates. The previous WRM packet is the real wait.
    if (glxSync != SyncGlxNone)
    {
        AcquireMemGeneric acquireInfo = {};
        acquireInfo.engineType = EngineTypeCompute;
        acquireInfo.cacheSync  = glxSync;

        pCmdSpace += m_cmdUtil.BuildAcquireMemGeneric(acquireInfo, pCmdSpace);
    }

    SetCsBltState(false);
    SetPrevCmdBufInactive();

    UpdateRetiredAcqRelFenceVal(ReleaseTokenEop, GetCurAcqRelFenceVal(ReleaseTokenEop));
    UpdateRetiredAcqRelFenceVal(ReleaseTokenCsDone, GetCurAcqRelFenceVal(ReleaseTokenCsDone));

    if (TestAllFlagsSet(glxSync, SyncGl2WbInv))
    {
        ClearBltWriteMisalignMdState();
    }

    if (waitCpDma)
    {
        SetCpBltState(false);
    }

    return pCmdSpace;
}

// =====================================================================================================================
uint32* ComputeCmdBuffer::WriteWaitCsIdle(
    uint32* pCmdSpace)
{
    const gpusize tsGpuVa = GetWaitIdleTsGpuVa(&pCmdSpace);

    pCmdSpace += m_cmdUtil.BuildWaitCsIdle(GetEngineType(), tsGpuVa, WaitIdleTsValue(), true, pCmdSpace);

    SetCsBltState(false);

    UpdateRetiredAcqRelFenceVal(ReleaseTokenCsDone, GetCurAcqRelFenceVal(ReleaseTokenCsDone));

    return pCmdSpace;
}

} // Gfx9
} // Pal
