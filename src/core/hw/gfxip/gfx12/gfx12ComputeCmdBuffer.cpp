/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/gfx12/gfx12BorderColorPalette.h"
#include "core/hw/gfxip/gfx12/gfx12ComputeCmdBuffer.h"
#include "core/hw/gfxip/gfx12/gfx12ComputePipeline.h"
#include "core/hw/gfxip/gfx12/gfx12CmdUtil.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12IndirectCmdGenerator.h"
#include "core/hw/gfxip/gfx12/gfx12PerfExperiment.h"
#include "core/hw/gfxip/rpm/gfx12/gfx12RsrcProcMgr.h"
#include "core/imported/hsa/amd_hsa_kernel_code.h"
#include "core/imported/hsa/AMDHSAKernelDescriptor.h"

#include "palHsaAbiMetadata.h"
#include "palInlineFuncs.h"

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
// Placeholder function for catching illegal attempts to set graphics user-data entries on a Compute command buffer.
static void PAL_STDCALL InvalidCmdSetComputeUserData(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    PAL_ASSERT_ALWAYS();
}

// =====================================================================================================================
ComputeCmdBuffer::ComputeCmdBuffer(
    const Device&                       device,
    const CmdBufferCreateInfo&          createInfo,
    const ComputeCmdBufferDeviceConfig& deviceConfig)
    :
    Pal::ComputeCmdBuffer(device, createInfo, device.BarrierMgr(), &m_cmdStream, true),
    m_deviceConfig(deviceConfig),
    m_device(device),
    m_cmdUtil(device.CmdUtil()),
    m_rsrcProcMgr(device.RsrcProcMgr()),
    m_pPrevComputeUserDataLayoutValidatedWith(nullptr),
    m_cmdStream(device,
                createInfo.pCmdAllocator,
                EngineTypeCompute,
                SubEngineType::Primary,
                CmdStreamUsage::Workload,
                IsNested()),
    m_ringSizeComputeScratch(0)
{
    const PalPlatformSettings& platformSettings = device.Parent()->GetPlatform()->PlatformSettings();
    m_describeDispatch                          = (device.Parent()->IssueSqttMarkerEvents() ||
                                                   device.Parent()->IssueCrashAnalysisMarkerEvents() ||
                                                   platformSettings.cmdBufferLoggerConfig.embedDrawDispatchInfo);

    SwitchCmdSetUserDataFunc(PipelineBindPoint::Graphics, &InvalidCmdSetComputeUserData);

    // Assume PAL ABI compute pipelines by default.
    SetDispatchFunctions(false);
}

// =====================================================================================================================
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
            m_rsrcProcMgr.EchoGlobalInternalTableAddr(this, m_globalInternalTableAddr);
        }
        else if (newUsesHsaAbi == false)
        {
            CmdUtil::BuildLoadShRegsIndex(index__pfp_load_sh_reg_index__direct_addr,
                                          data_format__pfp_load_sh_reg_index__offset_and_size,
                                          m_globalInternalTableAddr,
                                          mmCOMPUTE_USER_DATA_0,
                                          1,
                                          Pm4ShaderType::ShaderCompute,
                                          m_cmdStream.AllocateCommands(CmdUtil::LoadShRegsIndexSizeDwords));
        }

        SetDispatchFunctions(newUsesHsaAbi);
    }

    if (pNewPipeline != nullptr)
    {
        uint32* pCmdSpace = m_cmdStream.ReserveCommands();

#if PAL_DEVELOPER_BUILD
        const uint32* pStartingCmdSpace = pCmdSpace;
#endif

        pCmdSpace = pNewPipeline->WriteCommands(
            pOldPipeline, params.cs, m_buildFlags.prefetchShaders, pCmdSpace, &m_cmdStream);

#if PAL_DEVELOPER_BUILD
        if (m_deviceConfig.enablePm4Instrumentation)
        {
            const uint32 pipelineCmdLen = (uint32(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
            m_device.DescribeBindPipelineValidation(this, pipelineCmdLen);
        }
#endif

        m_cmdStream.CommitCommands(pCmdSpace);

        m_ringSizeComputeScratch =
            Max(pNewPipeline->GetRingSizeComputeScratch() + pNewPipeline->GetDvgprExtraAceScratch(),
                m_ringSizeComputeScratch);
    }

    GfxCmdBuffer::CmdBindPipeline(params);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdUpdateBusAddressableMemoryMarker(
    const IGpuMemory& dstGpuMemory,
    gpusize           offset,
    uint32            value)
{
    const GpuMemory*    pGpuMemory = static_cast<const GpuMemory*>(&dstGpuMemory);
    const WriteDataInfo writeData  =
    {
        .engineType = GetEngineType(),
        .dstAddr    = pGpuMemory->GetBusAddrMarkerVa() + offset,
        .dstSel     = dst_sel__mec_write_data__memory
    };
    CmdUtil::BuildWriteData(writeData, value, m_cmdStream.AllocateCommands(CmdUtil::WriteDataSizeDwords(1)));
}

// =====================================================================================================================
// Use the GPU's command processor to execute an atomic memory operation
void ComputeCmdBuffer::CmdMemoryAtomic(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    uint64            srcData,
    AtomicOp          atomicOp)
{
    const gpusize address = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;

    CmdUtil::BuildAtomicMem(atomicOp, address, srcData, m_cmdStream.AllocateCommands(CmdUtil::AtomicMemSizeDwords));
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdWriteTimestamp(
    uint32            stageMask,    // Bitmask of PipelineStageFlag
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    const gpusize address   = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;
    uint32*       pCmdSpace = m_cmdStream.ReserveCommands();

    // If multiple flags are set we must go down the path that is most conservative (writes at the latest point).
    // This is easiest to implement in this order:
    // 1. The EOP path for compute shaders.
    // 2. The CP stages can write the value directly using COPY_DATA in the MEC.
    // Note that passing in a stageMask of zero will get you an MEC write. It's not clear if that is even legal but
    // doing an MEC write is probably the least impactful thing we could do in that case.
    if (TestAnyFlagSet(stageMask, PipelineStageCs | PipelineStageBlt | PipelineStageBottomOfPipe))
    {
        ReleaseMemGeneric info = {};
        info.dstAddr     = address;
        info.dataSel     = data_sel__mec_release_mem__send_gpu_clock_counter;
        info.vgtEvent    = BOTTOM_OF_PIPE_TS;
        info.noConfirmWr = true;

        pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(info, pCmdSpace);
    }
    else
    {
        CopyDataInfo info = {};
        info.engineType = EngineTypeCompute;
        info.dstSel     = dst_sel__mec_copy_data__tc_l2;
        info.dstAddr    = address;
        info.srcSel     = src_sel__mec_copy_data__gpu_clock_count;
        info.countSel   = count_sel__mec_copy_data__64_bits_of_data;
        info.wrConfirm  = wr_confirm__mec_copy_data__do_not_wait_for_confirmation;

        pCmdSpace += CmdUtil::BuildCopyData(info, pCmdSpace);
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
    const bool is32Bit = (dataSize == ImmediateDataWidth::ImmediateData32Bit);

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    // If multiple flags are set we must go down the path that is most conservative (writes at the latest point).
    // This is easiest to implement in this order:
    // 1. The EOP path for compute shaders.
    // 2. The CP stages can write the value directly using COPY_DATA in the MEC.
    // Note that passing in a stageMask of zero will get you an MEC write. It's not clear if that is even legal but
    // doing an MEC write is probably the least impactful thing we could do in that case.
    if (TestAnyFlagSet(stageMask, PipelineStageCs | PipelineStageBlt | PipelineStageBottomOfPipe))
    {
        ReleaseMemGeneric releaseInfo{};
        releaseInfo.dstAddr  = address;
        releaseInfo.data     = data;
        releaseInfo.dataSel  = is32Bit ? data_sel__mec_release_mem__send_32_bit_low
                                       : data_sel__mec_release_mem__send_64_bit_data;
        releaseInfo.vgtEvent = BOTTOM_OF_PIPE_TS;

        pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, pCmdSpace);
    }
    else
    {
        CopyDataInfo info{};
        info.engineType = EngineTypeCompute;
        info.dstSel     = dst_sel__mec_copy_data__tc_l2;
        info.dstAddr    = address;
        info.srcSel     = src_sel__mec_copy_data__immediate_data;
        info.srcAddr    = data;
        info.countSel   = is32Bit ? count_sel__mec_copy_data__32_bits_of_data
                                  : count_sel__mec_copy_data__64_bits_of_data;
        info.wrConfirm = wr_confirm__mec_copy_data__wait_for_confirmation;

        pCmdSpace += CmdUtil::BuildCopyData(info, pCmdSpace);
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
    if (m_deviceConfig.disableBorderColorPaletteBinds == false)
    {
        const auto*const pNewPalette = static_cast<const BorderColorPalette*>(pPalette);

        {
            PAL_ASSERT(pipelineBindPoint == PipelineBindPoint::Compute);
            if (pNewPalette != nullptr)
            {
                uint32* pCmdSpace = m_cmdStream.ReserveCommands();
                pCmdSpace = pNewPalette->WriteCommands(pipelineBindPoint,
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
        if (m_deviceConfig.enablePreamblePipelineStats == 0)
        {
            ActivateQueryType(queryPoolType);
        }
    }
}

// =====================================================================================================================
// Adds PM4 commands needed to write any registers associated with ending the last active query in this command buffer.
void ComputeCmdBuffer::RemoveQuery(
    QueryPoolType queryPoolType)
{
    // Compute command buffers only support pipeline stat queries.
    PAL_ASSERT(queryPoolType == QueryPoolType::PipelineStats);

    // We're not bothering with PIPELINE_STOP events, as leaving these counters running doesn't hurt anything
    if (IsLastActiveQuery(queryPoolType))
    {
        // This will remove the active query as required.
    }
}

// =====================================================================================================================
// Enables the specified query type.
void ComputeCmdBuffer::ActivateQueryType(
    QueryPoolType queryPoolType)
{
    // Compute command buffers only support pipeline stat queries.
    PAL_ASSERT(queryPoolType == QueryPoolType::PipelineStats);

    GfxCmdBuffer::ActivateQueryType(queryPoolType);

    CmdUtil::BuildNonSampleEventWrite(PIPELINESTAT_START, EngineTypeCompute,
                                      m_cmdStream.AllocateCommands(CmdUtil::NonSampleEventWriteSizeDwords));
}

// =====================================================================================================================
// Disables the specified query type.
void ComputeCmdBuffer::DeactivateQueryType(
    QueryPoolType queryPoolType)
{
    // Compute command buffers only support pipeline stat queries.
    PAL_ASSERT(queryPoolType == QueryPoolType::PipelineStats);

    GfxCmdBuffer::DeactivateQueryType(queryPoolType);

    CmdUtil::BuildNonSampleEventWrite(PIPELINESTAT_STOP, EngineTypeCompute,
                                      m_cmdStream.AllocateCommands(CmdUtil::NonSampleEventWriteSizeDwords));
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
// Enables or disables a flexible predication check which the CP uses to determine if a draw or dispatch can be skipped
// based on the results of prior GPU work.
// SEE: CmdUtil::BuildSetPredication(...) for more details on the meaning of this method's parameters.
// Note that this function is currently only implemented for memory-based/DX12 predication
void ComputeCmdBuffer::CmdSetPredication(
    IQueryPool*       pQueryPool,
    uint32            slot,
    const IGpuMemory* pGpuMemory,
    gpusize           offset,
    PredicateType     predType,
    bool              predPolarity,
    bool              waitResults,
    bool              accumulateData)
{
    // This emulation doesn't work for QueryPool based predication, fortunately DX12 just has Boolean type
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
        writeData.engineType    = EngineTypeCompute;
        writeData.dstAddr       = m_predGpuAddr;
        writeData.dstSel        = dst_sel__mec_write_data__memory;

        pCmdSpace += CmdUtil::BuildCondExec(gpuVirtAddr, PM4_MEC_WRITE_DATA_SIZEDW__CORE + 1, pCmdSpace);
        pCmdSpace += CmdUtil::BuildWriteData(writeData, predCopyData, pCmdSpace);

        if (predType == PredicateType::Boolean64)
        {
            pCmdSpace += CmdUtil::BuildCondExec(gpuVirtAddr + 4, PM4_MEC_WRITE_DATA_SIZEDW__CORE + 1, pCmdSpace);
            pCmdSpace += CmdUtil::BuildWriteData(writeData, predCopyData, pCmdSpace);
        }

        m_cmdStream.CommitCommands(pCmdSpace);
    }
    else
    {
        m_predGpuAddr = 0;
    }
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdNop(
    const void* pPayload,
    uint32      payloadSize)
{
    // Write a 1-DW NOP header followed by the caller's payload.
    CmdUtil::BuildNopPayload(pPayload, payloadSize,
                             m_cmdStream.AllocateCommands(CmdUtil::NopPayloadSizeDwords(payloadSize)));
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
    const DmaDataInfo dmaData =
    {
        .dstSel       = dst_sel__pfp_dma_data__dst_addr_using_das,
        .dstAddr      = dstGpuMemory.Desc().gpuVirtAddr + dstOffset,
        .dstAddrSpace = das__pfp_dma_data__memory,
        .srcSel       = src_sel__pfp_dma_data__src_addr_using_sas,
        .srcAddr      = srcRegisterOffset,
        .srcAddrSpace = sas__pfp_dma_data__register,
        .usePfp       = false,
        .sync         = true
    };
    CmdUtil::BuildDmaData<false>(dmaData, m_cmdStream.AllocateCommands(CmdUtil::DmaDataSizeDwords));
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdWaitMemoryValue(
    gpusize     gpuVirtAddr,
    uint32      data,
    uint32      mask,
    CompareFunc compareFunc)
{
    CmdUtil::BuildWaitRegMem(EngineTypeCompute,
                             mem_space__mec_wait_reg_mem__memory_space,
                             CmdUtil::WaitRegMemFunc(compareFunc),
                             engine_sel__me_wait_reg_mem__micro_engine,
                             gpuVirtAddr,
                             data,
                             mask,
                             m_cmdStream.AllocateCommands(CmdUtil::WaitRegMemSizeDwords));
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdWaitBusAddressableMemoryMarker(
    const IGpuMemory& gpuMemory,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    const GpuMemory* pGpuMemory = static_cast<const GpuMemory*>(&gpuMemory);

    CmdUtil::BuildWaitRegMem(EngineTypeCompute,
                             mem_space__mec_wait_reg_mem__memory_space,
                             CmdUtil::WaitRegMemFunc(compareFunc),
                             engine_sel__me_wait_reg_mem__micro_engine,
                             pGpuMemory->GetBusAddrMarkerVa(),
                             data,
                             mask,
                             m_cmdStream.AllocateCommands(CmdUtil::WaitRegMemSizeDwords));
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32             cmdBufferCount,
    ICmdBuffer* const* ppCmdBuffers)
{
    for (uint32 cmdIdx = 0; cmdIdx < cmdBufferCount; cmdIdx++)
    {
        auto* const pNestedCmdBuffer = static_cast<Gfx12::ComputeCmdBuffer*>(ppCmdBuffers[cmdIdx]);
        PAL_ASSERT(pNestedCmdBuffer != nullptr);

        if ((pNestedCmdBuffer->m_inheritedPredGpuAddr != 0) && (m_predGpuAddr != 0))
        {
            const CopyDataInfo copyInfo = {
                .engineType = EngineTypeCompute,
                .dstSel     = dst_sel__mec_copy_data__tc_l2,
                .dstAddr    = pNestedCmdBuffer->m_inheritedPredGpuAddr,
                .srcSel     = src_sel__mec_copy_data__tc_l2,
                .srcAddr    = m_predGpuAddr,
                .countSel   = count_sel__mec_copy_data__32_bits_of_data,
                .wrConfirm  = wr_confirm__mec_copy_data__wait_for_confirmation
            };
            CmdUtil::BuildCopyData(copyInfo, m_cmdStream.AllocateCommands(CmdUtil::CopyDataSizeDwords));
        }

        // Track the most recent OS paging fence value across all nested command buffers called from this one.
        m_lastPagingFence = Max(m_lastPagingFence, pNestedCmdBuffer->LastPagingFence());

        // Track the lastest fence token across all nested command buffers called from this one.
        m_maxUploadFenceToken = Max(m_maxUploadFenceToken, pNestedCmdBuffer->GetMaxUploadFenceToken());

        // All user-data entries have been uploaded into the GPU memory the callee expects to receive them in, so we
        // can safely "call" the nested command buffer's command stream.
        m_cmdStream.TrackNestedEmbeddedData(pNestedCmdBuffer->m_embeddedData.chunkList);
        m_cmdStream.TrackNestedEmbeddedData(pNestedCmdBuffer->m_gpuScratchMem.chunkList);
        m_cmdStream.TrackNestedCommands(pNestedCmdBuffer->m_cmdStream);
        m_cmdStream.Call(pNestedCmdBuffer->m_cmdStream, pNestedCmdBuffer->IsExclusiveSubmit(), false);

        // Callee command buffers are also able to leak any changes they made to bound user-data entries and any other
        // state back to the caller.
        LeakNestedCmdBufferState(*pNestedCmdBuffer);
    }
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdCommentString(
    const char* pComment)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace += CmdUtil::BuildCommentString(pComment, ShaderCompute, pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Helper function for updating a command buffer's tracking of which user-data entries have known values after running
// an indirect-command generator and executing the generated commands.
static void CommandGeneratorTouchedUserData(
    const IndirectCmdGenerator& generator,
    size_t*                     pMask)
{
    // Mark any user-data entries which the command generator touched as "untouched" so that redundant user-data
    // filtering won't incorrectly reject subsequent user-data updates.
    for (uint32 idx = 0; idx < NumUserDataFlagsParts; ++idx)
    {
        pMask[idx] &= ~(generator.TouchedUserDataEntries()[idx]);
    }
}

// =====================================================================================================================
// Validation of the ExecuteIndirectOperation.
void ComputeCmdBuffer::ValidateExecuteIndirect(
    const IndirectCmdGenerator& gfx12Generator)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    // Just validate with arbitrary dispatch dims here as the real dims are in GPU memory which we don't
    // know at this point.
    constexpr DispatchDims LogicalSize = { .x = 1, .y = 1, .z = 1 };

    // This is Execute Indirect call here so indirect argument buffer shouldn't be passed for numWorkGroupReg.
    pCmdSpace = ValidateDispatchPalAbi(0, // indirectGpuVirtAddr
                                       LogicalSize,
                                       pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);

    CommandGeneratorTouchedUserData(gfx12Generator, &m_computeState.csUserDataEntries.touched[0]);
}

// =====================================================================================================================
// Construct some portions of the ExecuteIndirect operation and fill the corresponding execute indirect packet info.
void ComputeCmdBuffer::PreprocessExecuteIndirect(
    const IndirectCmdGenerator& generator,
    const ComputePipeline*      pCsPipeline,
    ExecuteIndirectPacketInfo*  pPacketInfo,
    ExecuteIndirectMeta*        pMeta,
    const EiDispatchOptions&    options)
{
    const GeneratorProperties& properties      = generator.Properties();
    const UserDataLayout*      pUserDataLayout = pUserDataLayout = pCsPipeline->UserDataLayout();
    ExecuteIndirectMetaData*   pMetaData       = pMeta->GetMetaData();

    const uint32 spillDwords = (pUserDataLayout->GetSpillThreshold() <= properties.userDataWatermark) ?
        properties.maxUserDataEntries : 0;

    uint32  spillTableStrideBytes = spillDwords * sizeof(uint32);
    gpusize spillTableAddress     = 0;

    // UserData that spills over the assigned SGPRs.
    if (spillTableStrideBytes > 0)
    {
        // Allocate and populate SpillTable Buffer with UserData. Each instance of the SpillTable needs to be
        // initialized with UserDataEntries of current context.
        uint32* pUserDataSpace = CmdAllocateEmbeddedData(spillDwords,
                                                         EiSpillTblStrideAlignmentDwords,
                                                         &spillTableAddress);

        PAL_ASSERT(pUserDataSpace != nullptr);
        memcpy(pUserDataSpace, m_computeState.csUserDataEntries.entries, (sizeof(uint32) * spillDwords));
    }

    const EiUserDataRegs regs = {};

    generator.PopulateExecuteIndirectParams(pCsPipeline,
                                            false,       // isGfx is false for ComputeCmdBuffer.
                                            true,        // ComputeCmdBuffer submission on ACE queue.
                                            pPacketInfo,
                                            pMeta,
                                            0,           // vertexBufTableDwords is 0 on ACE queue.
                                            options,
                                            regs);

    pMetaData->threadTraceEnable |= m_deviceConfig.issueSqttMarkerEvent;

    // Fill in execute indirect packet info.
    pPacketInfo->spillTableAddr        = spillTableAddress;
    pPacketInfo->spillTableStrideBytes = Pow2Align(spillTableStrideBytes, EiSpillTblStrideAlignmentBytes);
    pPacketInfo->pUserDataLayout       = pUserDataLayout;
}

// =====================================================================================================================
// This method helps create a CP packet to perform the ExecuteIndirect operation. We do this in 3 steps (1) Validate,
// (2) Pre-process and (3) Build PM4(s).
void ComputeCmdBuffer::ExecuteIndirectPacket(
    const IIndirectCmdGenerator& generator,
    const gpusize                gpuVirtAddr,
    const uint32                 maximumCount,
    const gpusize                countGpuAddr)
{
    const auto&                gfx12Generator = static_cast<const IndirectCmdGenerator&>(generator);
    const GeneratorProperties& properties     = gfx12Generator.Properties();

    // The generation of indirect commands is determined by the currently-bound pipeline.
    const auto* pCsPipeline  = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);

    // Step 1:-> Validate Dispatch Op/s.
    ValidateExecuteIndirect(gfx12Generator);

    // Step 2:-> Pre-process ExecuteIndirect.
    ExecuteIndirectPacketInfo packetInfo = {};
    packetInfo.argumentBufferAddr        = gpuVirtAddr;
    packetInfo.countBufferAddr           = countGpuAddr;
    packetInfo.argumentBufferStrideBytes = properties.argBufStride;
    packetInfo.maxCount                  = maximumCount;

    ExecuteIndirectOp packetOp {};
    ExecuteIndirectMeta meta {};

    const EiDispatchOptions options =
    {
        .enable2dInterleave    = false,
        .pingPongEnable        = false,
        .usesDispatchTunneling = UsesDispatchTunneling(),
        .isLinearDispatch      = false,
        .isWave32              = pCsPipeline->IsWave32()
    };

    // The rest of the packet info needs to be filled based on the input param buffer.
    PreprocessExecuteIndirect(gfx12Generator,
                              pCsPipeline,
                              &packetInfo,
                              &meta,
                              options);

    // Step3:-> Setup and Build PM4(s).

    // The GlobalSpillTable for EI V2 is only used when there will be updateMemCopy Ops (UserData SpillTable changes
    // between consecutive Draw/Dispatch Ops) or there is a buildSrd Op (VBTable). The FW expects the full allocation
    // for a HW workaround. So we allocate it every time.
    constexpr bool HasTask = false;
    SetExecuteIndirectV2GlobalSpill(HasTask);

    uint32* pCmdSpace         = m_cmdStream.ReserveCommands();
    uint32* pCondExecCmdSpace = nullptr;

    if (m_cmdBufState.flags.packetPredicate != 0)
    {
        // Reserve cmd space for Cond Exec
        pCondExecCmdSpace = pCmdSpace;
        pCmdSpace += CmdUtil::CondExecMecSize;
    }

    const size_t pktSize = CmdUtil::BuildExecuteIndirectV2Ace(PredDisable,
                                                              packetInfo,
                                                              &meta,
                                                              pCmdSpace);
    pCmdSpace += pktSize;

    if (m_cmdBufState.flags.packetPredicate != 0)
    {
        // Fill in Cond Exec as we know the exact packet size to be predicated now
        CmdUtil::BuildCondExec(m_predGpuAddr, uint32(pktSize), pCondExecCmdSpace);
    }

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdExecuteIndirectCmds(
    const IIndirectCmdGenerator& generator,
    gpusize                      gpuVirtAddr,
    uint32                       maximumCount,
    gpusize                      countGpuAddr)
{
    // We handle this cmd call by building an ExecuteIndirectV2 PM4.
    ExecuteIndirectPacket(generator, gpuVirtAddr, maximumCount, countGpuAddr);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdPrimeGpuCaches(
    uint32                    rangeCount,
    const PrimeGpuCacheRange* pRanges)
{
    PAL_ASSERT((rangeCount == 0) || (pRanges != nullptr));

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    for (uint32 i = 0; i < rangeCount; ++i)
    {
        pCmdSpace += CmdUtil::BuildPrimeGpuCaches(pRanges[i],
                                                  m_deviceConfig.prefetchClampSize,
                                                  EngineTypeCompute,
                                                  pCmdSpace);

    }

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Dumps this command buffer's command streams to the given file with an appropriate header.
void ComputeCmdBuffer::DumpCmdStreamsToFile(
    File*            pFile,
    CmdBufDumpFormat mode
    ) const
{
    m_cmdStream.DumpCommands(pFile, "# Compute Queue - Command length = ", mode);
}

// =====================================================================================================================
// Add any commands to restore state, etc. that are required at the beginning of every command buffer.
void ComputeCmdBuffer::AddPreamble()
{
    Result result = ComputeCmdBuffer::WritePreambleCommands(m_deviceConfig, &m_cmdStream);

}

// =====================================================================================================================
void ComputeCmdBuffer::AddPostamble()
{
    if ((m_globalInternalTableAddr != 0) &&
        (m_computeState.pipelineState.pPipeline != nullptr) &&
        (static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline)->GetInfo().flags.hsaAbi != 0u))
    {
        // If we're ending this cmdbuf with an HSA pipeline bound, the global table may currently
        // be invalid and we need to restore it for any subsequent chained cmdbufs.
        // Note 'nullptr' is considered PAL ABI and the restore must have already happened if needed.
        CmdUtil::BuildLoadShRegsIndex(index__pfp_load_sh_reg_index__direct_addr,
                                      data_format__pfp_load_sh_reg_index__offset_and_size,
                                      m_globalInternalTableAddr,
                                      mmCOMPUTE_USER_DATA_0,
                                      1,
                                      Pm4ShaderType::ShaderCompute,
                                      m_cmdStream.AllocateCommands(CmdUtil::LoadShRegsIndexSizeDwords));
    }

    ComputeCmdBuffer::WritePostambleCommands(this, &m_cmdStream);

}

// =====================================================================================================================
// Adds a preamble to the start of a new command buffer.
Result ComputeCmdBuffer::WritePreambleCommands(
    const ComputeCmdBufferDeviceConfig& deviceConfig,
    CmdStream*                          pCmdStream)
{
    // If this trips, it means that this isn't really the preamble -- i.e., somebody has inserted something into the
    // command stream before the preamble.  :-(
    PAL_ASSERT(pCmdStream->IsEmpty());

    if (deviceConfig.enablePreamblePipelineStats == 1)
    {
        CmdUtil::BuildNonSampleEventWrite(PIPELINESTAT_START, EngineTypeCompute,
                                          pCmdStream->AllocateCommands(CmdUtil::NonSampleEventWriteSizeDwords));
    }

    return Result::Success;
}

// =====================================================================================================================
// Adds a postamble to the end of a new command buffer.
void ComputeCmdBuffer::WritePostambleCommands(
    GfxCmdBuffer*const pCmdBuffer,
    CmdStream*         pCmdStream)
{
    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    if (pCmdBuffer->GetCmdBufState().flags.cpBltActive)
    {
        // Stalls the CP MEC until the CP's DMA engine has finished all previous "CP blts" (DMA_DATA commands
        // without the sync bit set). The ring won't wait for CP DMAs to finish so we need to do this manually.
        pCmdSpace += CmdUtil::BuildWaitDmaData(pCmdSpace);
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
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, EngineTypeCompute, pCmdSpace);
        pCmdSpace += CmdUtil::BuildAtomicMem(AtomicOp::AddInt32,
                                             pCmdStream->GetFirstChunk()->BusyTrackerGpuAddr(),
                                             1,
                                             pCmdSpace);
    }

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Adds commands necessary to write "data" to the specified memory
void ComputeCmdBuffer::WriteEventCmd(
    const BoundGpuMemory& boundMemObj,
    uint32                stageMask,   // Bitmask of PipelineStageFlag
    uint32                data)
{
    uint32* pCmdSpace           = m_cmdStream.ReserveCommands();
    bool    releaseMemWaitCpDma = false;

    if (GfxBarrierMgr::NeedWaitCpDma(this, stageMask))
    {
        // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
        // the CmdSetEvent and CmdResetEvent functions expect that the prior blts have completed by the time the event
        // is written to memory. Given that our CP DMA blts are asynchronous to the pipeline stages the only way to
        // satisfy this requirement is to force the MEC to stall until the CP DMAs are completed.
        if (m_device.EnableReleaseMemWaitCpDma())
        {
            releaseMemWaitCpDma = true;
        }
        else
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
    if (TestAnyFlagSet(stageMask, PipelineStageCs | PipelineStageBlt | PipelineStageBottomOfPipe))
    {
        // Implement set/reset with an EOP event written when all prior GPU work completes. Note that waiting on an
        // EOS timestamp and waiting on an EOP timestamp are exactly equivalent on compute queues. There's no reason
        // to implement a CS_DONE path for HwPipePostCs.
        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.dstAddr   = boundMemObj.GpuVirtAddr();
        releaseInfo.dataSel   = data_sel__mec_release_mem__send_32_bit_low;
        releaseInfo.data      = data;
        releaseInfo.vgtEvent  = BOTTOM_OF_PIPE_TS;
        releaseInfo.waitCpDma = releaseMemWaitCpDma;

        pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, pCmdSpace);
    }
    else
    {
        // Implement set/reset event with a WRITE_DATA command using the CP.
        WriteDataInfo writeData = {};
        writeData.engineType = EngineTypeCompute;
        writeData.dstAddr    = boundMemObj.GpuVirtAddr();
        writeData.dstSel     = dst_sel__mec_write_data__memory;

        pCmdSpace += CmdUtil::BuildWriteData(writeData, data, pCmdSpace);
    }

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
template <bool HasPipelineChanged>
uint32* ComputeCmdBuffer::ValidateComputeUserData(
    UserDataEntries*             pUserData,
    UserDataTableState*          pSpillTable,
    const ComputeUserDataLayout* pCurrentComputeUserDataLayout,
    const ComputeUserDataLayout* pPrevComputeUserDataLayout,
    gpusize                      indirectGpuVirtAddr,
    DispatchDims                 logicalSize,
    uint32*                      pCmdSpace)
{
    PAL_ASSERT(pCurrentComputeUserDataLayout != nullptr);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Insert a single packet for all persistent state registers
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Save off a location for a single SET_PAIRS header for all SH regs written for this bind
    uint32* const pSetPairsHeader = pCmdSpace;
    pCmdSpace += 1;

    const bool anyUserDataDirty = IsAnyUserDataDirty(pUserData);

    if (HasPipelineChanged || anyUserDataDirty)
    {
        pCmdSpace = pCurrentComputeUserDataLayout->CopyUserDataPairsToCmdSpace<HasPipelineChanged>(
            pPrevComputeUserDataLayout,
            pUserData->dirty,
            pUserData->entries,
            pCmdSpace);

        const UserDataReg spillTableUserDataReg = pCurrentComputeUserDataLayout->GetSpillTable();

        if ((spillTableUserDataReg.u32All != UserDataNotMapped) &&
            (pCurrentComputeUserDataLayout->GetSpillThreshold() != NoUserDataSpilling))
        {
            bool         reUpload       = false;
            uint16 spillThreshold = pCurrentComputeUserDataLayout->GetSpillThreshold();
            const uint32 userDataLimit  = pCurrentComputeUserDataLayout->GetUserDataLimit();

            pSpillTable->sizeInDwords = userDataLimit;

            PAL_ASSERT(userDataLimit > 0);
            const uint16 lastUserData = (userDataLimit - 1);

            PAL_ASSERT(pSpillTable->dirty == 0); // Not ever setting this today.

            if (HasPipelineChanged &&
                ((pPrevComputeUserDataLayout == nullptr)                             ||
                 (spillThreshold != pPrevComputeUserDataLayout->GetSpillThreshold()) ||
                 (userDataLimit > pPrevComputeUserDataLayout->GetUserDataLimit())))
            {
                // If the pipeline is changing and the spilled region is changing, reupload.
                reUpload = true;
            }
            else if (anyUserDataDirty)
            {
                const uint32 firstMaskId = (spillThreshold / UserDataEntriesPerMask);
                const uint32 lastMaskId  = (lastUserData   / UserDataEntriesPerMask);
                for (uint32 maskId = firstMaskId; maskId <= lastMaskId; ++maskId)
                {
                    size_t dirtyMask = pUserData->dirty[maskId];
                    if (maskId == firstMaskId)
                    {
                        // Ignore the dirty bits for any entries below the spill threshold.
                        const uint32 firstEntryInMask = (spillThreshold & (UserDataEntriesPerMask - 1));
                        dirtyMask &= ~BitfieldGenMask(static_cast<size_t>(firstEntryInMask));
                    }
                    if (maskId == lastMaskId)
                    {
                        // Ignore the dirty bits for any entries beyond the user-data limit.
                        const uint32 lastEntryInMask = (lastUserData & (UserDataEntriesPerMask - 1));
                        dirtyMask &= BitfieldGenMask(static_cast<size_t>(lastEntryInMask + 1));
                    }

                    if (dirtyMask != 0)
                    {
                        reUpload = true;
                        break; // We only care if *any* spill table contents change!
                    }
                }
            }

            if (reUpload)
            {
                UpdateUserDataTableCpu(pSpillTable,
                                       (userDataLimit - spillThreshold),
                                       spillThreshold,
                                       &pUserData->entries[0]);
            }

            if (HasPipelineChanged || reUpload)
            {
                const uint32 gpuVirtAddrLo = LowPart(pSpillTable->gpuVirtAddr);
                PAL_ASSERT(spillTableUserDataReg.regOffset != 0);

                pCmdSpace[0] = spillTableUserDataReg.regOffset;
                pCmdSpace[1] = gpuVirtAddrLo;
                pCmdSpace += 2;
            }
        }

        // Clear dirty bits
        size_t* pDirtyMask = &pUserData->dirty[0];
        for (uint32 i = 0; i < NumUserDataFlagsParts; i++)
        {
            pDirtyMask[i] = 0;
        }
    }

    const UserDataReg workGroupsRegAddr = pCurrentComputeUserDataLayout->GetWorkgroup();

    if (workGroupsRegAddr.regOffset != UserDataNotMapped)
    {
        // Indirect Dispatches by definition have the number of thread-groups to launch stored in GPU memory at the
        // specified address.  However, for direct Dispatches, we must allocate some embedded memory to store this
        // information.
        if (indirectGpuVirtAddr == 0uLL) // This is a direct Dispatch.
        {
            *reinterpret_cast<DispatchDims*>(CmdAllocateEmbeddedData(sizeof(DispatchDims),
                                                                     sizeof(uint32),
                                                                     &indirectGpuVirtAddr)) = logicalSize;
        }

        pCmdSpace[0] = workGroupsRegAddr.regOffset;
        pCmdSpace[1] = LowPart(indirectGpuVirtAddr);
        pCmdSpace[2] = workGroupsRegAddr.regOffset + 1;
        pCmdSpace[3] = HighPart(indirectGpuVirtAddr);
        pCmdSpace += 4;
    }

    // (pSetPairsHeader + 1) not needed
    const uint32 numRegPairs = uint32(VoidPtrDiff(pCmdSpace, pSetPairsHeader) / sizeof(RegisterValuePair));
    if (numRegPairs > 0)
    {
        void* pThrowAway;
        const size_t pktSize = CmdUtil::BuildSetShPairsHeader<ShaderCompute>(numRegPairs,
                                                                             &pThrowAway,
                                                                             pSetPairsHeader);
        PAL_ASSERT(pktSize == size_t(pCmdSpace - pSetPairsHeader));
    }
    else
    {
        // Remove reserved space for header!
        pCmdSpace -= 1;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // End of SET_SH_REG_PAIRS pkt
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    return pCmdSpace;
}

// =====================================================================================================================
uint32* ComputeCmdBuffer::ValidateDispatchPalAbi(
    gpusize      indirectGpuVirtAddr,
    DispatchDims logicalSize,
    uint32*      pCmdSpace)
{
    auto* const pPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);
    PAL_ASSERT(pPipeline != nullptr);

#if PAL_DEVELOPER_BUILD
    const uint32* pStartingCmdSpace = pCmdSpace;
#endif

    if (m_computeState.pipelineState.dirtyFlags.pipeline)
    {
        pCmdSpace = ValidateComputeUserData<true>(&m_computeState.csUserDataEntries,
                                                  &m_spillTable.stateCs,
                                                  pPipeline->UserDataLayout(),
                                                  m_pPrevComputeUserDataLayoutValidatedWith,
                                                  indirectGpuVirtAddr,
                                                  logicalSize,
                                                  pCmdSpace);
        m_pPrevComputeUserDataLayoutValidatedWith = pPipeline->UserDataLayout();
    }
    else
    {
        pCmdSpace = ValidateComputeUserData<false>(&m_computeState.csUserDataEntries,
                                                   &m_spillTable.stateCs,
                                                   pPipeline->UserDataLayout(),
                                                   m_pPrevComputeUserDataLayoutValidatedWith,
                                                   indirectGpuVirtAddr,
                                                   logicalSize,
                                                   pCmdSpace);
        PAL_ASSERT(m_pPrevComputeUserDataLayoutValidatedWith == pPipeline->UserDataLayout());
    }

#if PAL_DEVELOPER_BUILD
    if (m_deviceConfig.enablePm4Instrumentation)
    {
        const uint32 userDataCmdLen = (static_cast<uint32>(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
        // No misc. commands written during dispatch validation.
        m_device.DescribeDrawDispatchValidation(this, userDataCmdLen, 0);
    }
#endif

    // Clear the dirty flags
    m_computeState.pipelineState.dirtyFlags.u32All = 0;

    return pCmdSpace;
}

// =====================================================================================================================
uint32* ComputeCmdBuffer::ValidateDispatchHsaAbi(
    DispatchDims        offset,
    const DispatchDims& logicalSize,
    uint32*             pCmdSpace)
{
    auto* const pPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);
    PAL_ASSERT(pPipeline != nullptr);

#if PAL_DEVELOPER_BUILD
    const uint32* pStartingCmdSpace = pCmdSpace;
#endif

    // PAL thinks in terms of threadgroups but the HSA ABI thinks in terms of global threads, we need to convert.
    const DispatchDims threads = pPipeline->ThreadsPerGroupXyz();

    offset *= threads;

    // Now we write the required SGPRs. These depend on per-dispatch state so we don't have dirty bit tracking.
    const HsaAbi::CodeObjectMetadata& metadata = pPipeline->HsaMetadata();
    const llvm::amdhsa::kernel_descriptor_t& desc = pPipeline->KernelDescriptor();

    gpusize kernargsGpuVa = 0;
    uint32 ldsSize = metadata.GroupSegmentFixedSize();
    if (TestAnyFlagSet(desc.kernel_code_properties, AMD_KERNEL_CODE_PROPERTIES_ENABLE_SGPR_KERNARG_SEGMENT_PTR))
    {
        GfxCmdBuffer::CopyHsaKernelArgsToMem(offset, threads, logicalSize, &kernargsGpuVa, &ldsSize, metadata);
    }

    // If ldsBytesPerTg was specified then that's what LDS_SIZE was programmed to, otherwise we used the fixed size.
    const uint32 boundLdsSize =
        (m_computeState.dynamicCsInfo.ldsBytesPerTg > 0) ? m_computeState.dynamicCsInfo.ldsBytesPerTg
                                                         : metadata.GroupSegmentFixedSize();

    // If our computed total LDS size is larger than the previously bound size we must rewrite it.
    if (boundLdsSize < ldsSize)
    {
        pCmdSpace = pPipeline->WriteUpdatedLdsSize(pCmdSpace, ldsSize);

        // We've effectively rebound this state. Update its value so that we don't needlessly rewrite it on the
        // next dispatch call.
        m_computeState.dynamicCsInfo.ldsBytesPerTg = ldsSize;
    }

    uint32 startReg = mmCOMPUTE_USER_DATA_0;

    m_pPrevComputeUserDataLayoutValidatedWith = nullptr;

    // Many HSA ELFs request private segment buffer registers, but never actually use them. Space is reserved to
    // adhere to initialization order but will be unset as we do not support scratch space in this execution path.
    if (TestAnyFlagSet(desc.kernel_code_properties, AMD_KERNEL_CODE_PROPERTIES_ENABLE_SGPR_PRIVATE_SEGMENT_BUFFER))
    {
        startReg += 4;
    }
    if (TestAnyFlagSet(desc.kernel_code_properties, AMD_KERNEL_CODE_PROPERTIES_ENABLE_SGPR_DISPATCH_PTR))
    {
        const DispatchDims logicalSizeInWorkItems = logicalSize * threads;

        // Fake an AQL dispatch packet for the shader to read.
        gpusize aqlPacketGpu = 0;
        auto* const pAqlPacket = reinterpret_cast<hsa_kernel_dispatch_packet_t*>(
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

        pCmdSpace = CmdStream::WriteSetSeqShRegs<ShaderCompute>(startReg,
                                                               (startReg + 1),
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
    if (TestAnyFlagSet(desc.kernel_code_properties, AMD_KERNEL_CODE_PROPERTIES_ENABLE_SGPR_QUEUE_PTR))
    {
        startReg += 2;
    }

    if (TestAnyFlagSet(desc.kernel_code_properties, AMD_KERNEL_CODE_PROPERTIES_ENABLE_SGPR_KERNARG_SEGMENT_PTR))
    {
        pCmdSpace = CmdStream::WriteSetSeqShRegs<ShaderCompute>(startReg, (startReg + 1), &kernargsGpuVa, pCmdSpace);
        startReg += 2;
    }

    if (TestAnyFlagSet(desc.kernel_code_properties, AMD_KERNEL_CODE_PROPERTIES_ENABLE_SGPR_DISPATCH_ID))
    {
        // This feature may be enabled as a side effect of indirect calls.
        // However, the compiler team confirmed that the dispatch id itself is not used,
        // so safe to send 0 for each dispatch.
        constexpr uint32 DispatchId[2] = {};
        pCmdSpace = CmdStream::WriteSetSeqShRegs<ShaderCompute>(startReg, (startReg + 1), &DispatchId, pCmdSpace);
        startReg += 2;
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    regCOMPUTE_PGM_RSRC2 computePgmRsrc2 = {};
    computePgmRsrc2.u32All = desc.compute_pgm_rsrc2;

    PAL_ASSERT((startReg - mmCOMPUTE_USER_DATA_0) <= computePgmRsrc2.bitfields.USER_SGPR);
#endif

#if PAL_DEVELOPER_BUILD
    if (m_deviceConfig.enablePm4Instrumentation)
    {
        const uint32 userDataCmdLen = (static_cast<uint32>(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
        // No misc. commands written during dispatch validation.
        m_device.DescribeDrawDispatchValidation(this, userDataCmdLen, 0);
    }
#endif

    // Clear the dirty flags
    m_computeState.pipelineState.dirtyFlags.u32All = 0;

    return pCmdSpace;
}

// =====================================================================================================================
// Sets-up function pointers for the Dispatch entrypoint and all variants using template parameters.
template<bool HsaAbi, bool IssueSqtt, bool DescribeCallback>
void ComputeCmdBuffer::SetDispatchFunctions()
{
    static_assert(DescribeCallback || (IssueSqtt == false),
        "DescribeCallback must be true if IssueSqtt is true!");

    m_funcTable.pfnCmdDispatch             = CmdDispatch<HsaAbi, IssueSqtt, DescribeCallback>;
    m_funcTable.pfnCmdDispatchOffset       = CmdDispatchOffset<HsaAbi, IssueSqtt, DescribeCallback>;

    if (HsaAbi)
    {
        // Note that CmdDispatchIndirect does not support the HSA ABI.
        m_funcTable.pfnCmdDispatchIndirect = nullptr;
    }
    else
    {
        m_funcTable.pfnCmdDispatchIndirect = CmdDispatchIndirect<IssueSqtt, DescribeCallback>;
    }
}

// =====================================================================================================================
// Sets-up function pointers for the Dispatch entrypoint and all variants.
void ComputeCmdBuffer::SetDispatchFunctions(
    bool hsaAbi)
{
    if (hsaAbi)
    {
        if (m_deviceConfig.issueSqttMarkerEvent)
        {
            SetDispatchFunctions<true, true, true>();
        }
        else
        {
            SetDispatchFunctions<true, false, false>();
        }
    }
    else
    {
        if (m_deviceConfig.issueSqttMarkerEvent)
        {
            SetDispatchFunctions<false, true, true>();
        }
        else
        {
            SetDispatchFunctions<false, false, false>();
        }
    }
}

// =====================================================================================================================
template<bool HsaAbi, bool IssueSqtt, bool DescribeCallback>
void PAL_STDCALL ComputeCmdBuffer::CmdDispatch(
    ICmdBuffer*       pCmdBuffer,
    DispatchDims      size,
    DispatchInfoFlags infoFlags)
{
    auto*                  pThis     = static_cast<ComputeCmdBuffer*>(pCmdBuffer);
    const ComputePipeline* pPipeline =
        static_cast<const ComputePipeline*>(pThis->m_computeState.pipelineState.pPipeline);

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
        pCmdSpace = pThis->ValidateDispatchPalAbi(0ull, size, pCmdSpace);
    }

    if (pThis->m_cmdBufState.flags.packetPredicate != 0)
    {
        uint32 predSize = CmdUtil::DispatchDirectSize;
        if (IssueSqtt)
        {
            predSize += CmdUtil::NonSampleEventWriteSizeDwords;
        }
        pCmdSpace += CmdUtil::BuildCondExec(pThis->m_predGpuAddr, predSize, pCmdSpace);
    }

    pCmdSpace += CmdUtil::BuildDispatchDirect<false, true>(size,
                                                           PredDisable,
                                                           pPipeline->IsWave32(),
                                                           pThis->UsesDispatchTunneling(),
                                                           pPipeline->DisablePartialPreempt(),
                                                           false, // PING_PONG not compatible with ACE!
                                                           false, // 2D interleave not compatible with ACE!
                                                           pCmdSpace);

    if (IssueSqtt)
    {
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeCompute, pCmdSpace);
    }

    pThis->m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
template<bool IssueSqtt, bool DescribeCallback>
void PAL_STDCALL ComputeCmdBuffer::CmdDispatchIndirect(
    ICmdBuffer* pCmdBuffer,
    gpusize     gpuVirtAddr)
{
    auto*                  pThis     = static_cast<ComputeCmdBuffer*>(pCmdBuffer);
    const ComputePipeline* pPipeline =
        static_cast<const ComputePipeline*>(pThis->m_computeState.pipelineState.pPipeline);

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
        if (IssueSqtt)
        {
            size += CmdUtil::NonSampleEventWriteSizeDwords;
        }
        pCmdSpace += CmdUtil::BuildCondExec(pThis->m_predGpuAddr, size, pCmdSpace);
    }

    pCmdSpace += CmdUtil::BuildDispatchIndirectMec(gpuVirtAddr,
                                                   pPipeline->IsWave32(),
                                                   pThis->UsesDispatchTunneling(),
                                                   pPipeline->DisablePartialPreempt(),
                                                   pCmdSpace);

    if (IssueSqtt)
    {
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeCompute, pCmdSpace);
    }

    pThis->m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
template<bool HsaAbi, bool IssueSqtt, bool DescribeCallback>
void PAL_STDCALL ComputeCmdBuffer::CmdDispatchOffset(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims offset,
    DispatchDims launchSize,
    DispatchDims logicalSize)
{
    auto*                  pThis     = static_cast<ComputeCmdBuffer*>(pCmdBuffer);
    const ComputePipeline* pPipeline =
        static_cast<const ComputePipeline*>(pThis->m_computeState.pipelineState.pPipeline);

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

    pCmdSpace = CmdStream::WriteSetSeqShRegs<ShaderCompute>(mmCOMPUTE_START_X,
                                                            mmCOMPUTE_START_Z,
                                                            &offset,
                                                            pCmdSpace);

    if (pThis->m_cmdBufState.flags.packetPredicate != 0)
    {
        uint32 size = CmdUtil::DispatchDirectSize;
        if (IssueSqtt)
        {
            size += CmdUtil::NonSampleEventWriteSizeDwords;
        }
        pCmdSpace += CmdUtil::BuildCondExec(pThis->m_predGpuAddr, size, pCmdSpace);
    }

    // The DIM_X/Y/Z in DISPATCH_DIRECT packet are used to program COMPUTE_DIM_X/Y/Z registers, which are actually the
    // end block positions instead of execution block dimensions. So we need to use the dimensions plus offsets.
    pCmdSpace += CmdUtil::BuildDispatchDirect<false, false>(offset + launchSize,
                                                            PredDisable,
                                                            pPipeline->IsWave32(),
                                                            pThis->UsesDispatchTunneling(),
                                                            pPipeline->DisablePartialPreempt(),
                                                            false, // PING_PONG not compatible with ACE!
                                                            false, // 2D interleave not compatible with ACE!
                                                            pCmdSpace);

    if (IssueSqtt)
    {
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeCompute, pCmdSpace);
    }

    pThis->m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::ResetState()
{
    Pal::ComputeCmdBuffer::ResetState();

    // Assume PAL ABI compute pipelines by default.
    SetDispatchFunctions(false);

    m_pPrevComputeUserDataLayoutValidatedWith = nullptr;
    m_ringSizeComputeScratch                  = 0;
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
            pCmdSpace += CmdUtil::BuildCondExec(m_predGpuAddr, PM4_MEC_DMA_DATA_SIZEDW__CORE, pCmdSpace);
        }
        pCmdSpace += CmdUtil::BuildDmaData<false>(dmaDataInfo, pCmdSpace);
        m_cmdStream.CommitCommands(pCmdSpace);

        dmaDataInfo.dstAddr += dmaDataInfo.numBytes;
        dmaDataInfo.srcAddr += dmaDataInfo.numBytes;
        numBytes            -= dmaDataInfo.numBytes;
    }

    SetCpBltState(true);
    SetCpMemoryWriteL2CacheStaleState(true);

#if PAL_DEVELOPER_BUILD
    Developer::RpmBltData cbData = { .pCmdBuffer = this, .bltType = Developer::RpmBltType::CpDmaCopy };
    m_device.Parent()->DeveloperCb(Developer::CallbackType::RpmBlt, &cbData);
#endif
}

// =====================================================================================================================
// Updates the SQTT token mask for all SEs outside of a specific PerfExperiment. Used by GPA Session when targeting
// a single event for instruction level trace during command buffer building.
void ComputeCmdBuffer::CmdUpdateSqttTokenMask(
    const ThreadTraceTokenConfig& sqttTokenConfig)
{
    PerfExperiment::UpdateSqttTokenMaskStatic(&m_cmdStream, sqttTokenConfig);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdInsertTraceMarker(
    PerfTraceMarkerType markerType,
    uint32              markerData)
{
    constexpr uint32 MarkerRegisters[] = { mmSQ_THREAD_TRACE_USERDATA_2,
                                           mmSQ_THREAD_TRACE_USERDATA_3,
                                           mmRLC_SPM_GLOBAL_USER_DATA_0,
                                           mmRLC_SPM_GLOBAL_USER_DATA_1,
                                           mmRLC_SPM_GLOBAL_USER_DATA_2,
                                           mmRLC_SPM_GLOBAL_USER_DATA_3 };
    static_assert(ArrayLen(MarkerRegisters) == uint32(PerfTraceMarkerType::Count),
                   "Array does not match expected length!");

    m_cmdStream.AllocateAndBuildSetOneUConfigReg(MarkerRegisters[uint32(markerType)], markerData);
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

        // Allocate command space inside this loop. Some of the RGP packets are unbounded, like adding a
        // comment string, so it's not safe to assume the whole packet will fit in one command chunk.
        m_cmdStream.AllocateAndBuildSetSeqUConfigRegs(mmSQ_THREAD_TRACE_USERDATA_2,
                                                      mmSQ_THREAD_TRACE_USERDATA_2 + dwordsToWrite - 1,
                                                      pDwordData);

        pDwordData += dwordsToWrite;
        numDwords -= dwordsToWrite;
    }
}

// =====================================================================================================================
// Helper method for handling the state "leakage" from a nested command buffer back to its caller. Since the callee has
// tracked its own state during the building phase, we can access the final state of the command buffer since its stored
// in the UniversalCmdBuffer object itself.
void ComputeCmdBuffer::LeakNestedCmdBufferState(
    const ComputeCmdBuffer& cmdBuffer)
{
    Pal::ComputeCmdBuffer::LeakNestedCmdBufferState(cmdBuffer);

    if (cmdBuffer.m_computeState.pipelineState.pPipeline != nullptr)
    {
        m_pPrevComputeUserDataLayoutValidatedWith = cmdBuffer.m_pPrevComputeUserDataLayoutValidatedWith;
    }

    m_ringSizeComputeScratch = Max(cmdBuffer.m_ringSizeComputeScratch, m_ringSizeComputeScratch);
}

// =====================================================================================================================
uint32* ComputeCmdBuffer::WriteWaitEop(
    WriteWaitEopInfo info,
    uint32*          pCmdSpace)
{
    SyncGlxFlags       glxSync   = SyncGlxFlags(info.hwGlxSync);
    const AcquirePoint acqPoint  = AcquirePoint(info.hwAcqPoint);
    const bool         waitCpDma = info.waitCpDma;

    PAL_ASSERT(info.hwRbSync == SyncRbNone);

    // Issue explicit waitCpDma packet if ReleaseMem doesn't support it.
    bool releaseMemWaitCpDma = waitCpDma;
    if (waitCpDma && (m_deviceConfig.enableReleaseMemWaitCpDma == false))
    {
        pCmdSpace += CmdUtil::BuildWaitDmaData(pCmdSpace);
        releaseMemWaitCpDma = false;
    }

    // We define an "EOP" wait to mean a release without a WaitRegMem.
    // If glxSync still has some flags left over we still need a WaitRegMem to issue the GCR.
    const bool    needWaitRegMem = (acqPoint != AcquirePointEop) || (glxSync != SyncGlxNone);
    const gpusize timestampAddr  = TimestampGpuVirtAddr();

    if (needWaitRegMem)
    {
        // Write a known value to the timestamp.
        WriteDataInfo writeData = {};
        writeData.engineType = EngineTypeUniversal;
        writeData.dstAddr    = timestampAddr;
        writeData.engineSel  = engine_sel__me_write_data__micro_engine;
        writeData.dstSel     = dst_sel__me_write_data__tc_l2;

        pCmdSpace += CmdUtil::BuildWriteData(writeData, ClearedTimestamp, pCmdSpace);
    }

    // We prefer to do our GCR in the release_mem if we can. This function always does an EOP wait so we don't have
    // to worry about release_mem not supporting GCRs with EOS events. Any remaining sync flags must be handled in a
    // trailing acquire_mem packet.
    ReleaseMemGeneric releaseInfo = {};
    releaseInfo.vgtEvent  = BOTTOM_OF_PIPE_TS;
    releaseInfo.cacheSync = CmdUtil::SelectReleaseMemCaches(&glxSync);
    releaseInfo.dataSel   = needWaitRegMem ? data_sel__me_release_mem__send_32_bit_low
                                           : data_sel__me_release_mem__none;
    releaseInfo.dstAddr   = timestampAddr;
    releaseInfo.data      = CompletedTimestamp;
    releaseInfo.waitCpDma = releaseMemWaitCpDma;

    pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, pCmdSpace);

    if (needWaitRegMem)
    {
        pCmdSpace += CmdUtil::BuildWaitRegMem(EngineTypeCompute,
                                              mem_space__me_wait_reg_mem__memory_space,
                                              function__me_wait_reg_mem__equal_to_the_reference_value,
                                              engine_sel__me_wait_reg_mem__micro_engine,
                                              timestampAddr,
                                              releaseInfo.data,
                                              UINT32_MAX,
                                              pCmdSpace);

        // If we still have some caches to sync we require a final acquire_mem. It doesn't do any waiting, it just
        // immediately does some full-range cache flush and invalidates. The previous WRM packet is the real wait.
        if (glxSync != SyncGlxNone)
        {
            AcquireMemGeneric acquireInfo = {};
            acquireInfo.engineType = EngineTypeCompute;
            acquireInfo.cacheSync  = glxSync;

            pCmdSpace += CmdUtil::BuildAcquireMemGeneric(acquireInfo, pCmdSpace);
        }

        SetCsBltState(false);
        SetPrevCmdBufInactive();

        UpdateRetiredAcqRelFenceVal(ReleaseTokenEop, GetCurAcqRelFenceVal(ReleaseTokenEop));
        UpdateRetiredAcqRelFenceVal(ReleaseTokenCsDone, GetCurAcqRelFenceVal(ReleaseTokenCsDone));
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
    pCmdSpace += CmdUtil::BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, EngineTypeCompute, pCmdSpace);

    SetCsBltState(false);

    UpdateRetiredAcqRelFenceVal(ReleaseTokenCsDone, GetCurAcqRelFenceVal(ReleaseTokenCsDone));

    return pCmdSpace;
}

} // Gfx12
} // Pal
