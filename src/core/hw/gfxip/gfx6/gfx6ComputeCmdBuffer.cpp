/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx6/gfx6BorderColorPalette.h"
#include "core/hw/gfxip/gfx6/gfx6ComputeCmdBuffer.h"
#include "core/hw/gfxip/gfx6/gfx6ComputePipeline.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6IndirectCmdGenerator.h"
#include "core/hw/gfxip/queryPool.h"
#include "core/cmdAllocator.h"
#include "core/g_palPlatformSettings.h"
#include "palInlineFuncs.h"
#include "palVectorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
size_t ComputeCmdBuffer::GetSize(
    const Device& device)
{
    size_t bytes = sizeof(ComputeCmdBuffer);

    // NOTE: Because compute command buffers always use embedded data to manage the client's indirect user-data tables,
    // we need to track their contents along with the command buffer's state. Since the sizes of these tables is dynamic
    // and the client configures them at run-time, we will store them immediately following the command buffer object
    // itself in memory.
    for (uint32 tableId = 0; tableId < MaxIndirectUserDataTables; ++tableId)
    {
        bytes += (sizeof(uint32) * device.Parent()->IndirectUserDataTableSize(tableId));
    }

    return bytes;
}

// =====================================================================================================================
ComputeCmdBuffer::ComputeCmdBuffer(
    const Device&              device,
    const CmdBufferCreateInfo& createInfo)
    :
    Pal::ComputeCmdBuffer(device, createInfo, &m_prefetchMgr, &m_cmdStream),
    m_device(device),
    m_cmdUtil(device.CmdUtil()),
    m_prefetchMgr(device),
    m_cmdStream(device,
                createInfo.pCmdAllocator,
                EngineTypeCompute,
                SubEngineType::Primary,
                CmdStreamUsage::Workload,
                IsNested()),
    m_pSignatureCs(&NullCsSignature),
    m_predGpuAddr(0)
{

    // Compute command buffers suppors compute ops and CP DMA.
    m_engineSupport = CmdBufferEngineSupport::Compute | CmdBufferEngineSupport::CpDma;

    const PalPlatformSettings& settings = m_device.Parent()->GetPlatform()->PlatformSettings();
    const bool sqttEnabled = (settings.gpuProfilerMode > GpuProfilerCounterAndTimingOnly) &&
                             (TestAnyFlagSet(settings.gpuProfilerConfig.traceModeMask, GpuProfilerTraceSqtt));
    const bool issueSqttMarkerEvent = (sqttEnabled || device.GetPlatform()->IsDevDriverProfilingEnabled());

    if (issueSqttMarkerEvent)
    {
        m_funcTable.pfnCmdDispatch          = CmdDispatch<true>;
        m_funcTable.pfnCmdDispatchIndirect  = CmdDispatchIndirect<true>;
        m_funcTable.pfnCmdDispatchOffset    = CmdDispatchOffset<true>;
    }
    else
    {
        m_funcTable.pfnCmdDispatch          = CmdDispatch<false>;
        m_funcTable.pfnCmdDispatchIndirect  = CmdDispatchIndirect<false>;
        m_funcTable.pfnCmdDispatchOffset    = CmdDispatchOffset<false>;
    }
}

// =====================================================================================================================
// Initializes Gfx6-specific functionality.
Result ComputeCmdBuffer::Init(
    const CmdBufferInternalCreateInfo& internalInfo)
{
    Result result = Pal::ComputeCmdBuffer::Init(internalInfo);

    if (result == Result::Success)
    {
        result = m_cmdStream.Init();
    }

    if (result == Result::Success)
    {
        // The indirect user data tables immediately follow the command buffer object in memory. The GfxIp-specific
        // command buffer object's size must be used in order to ensure the location is correct.
        SetupIndirectUserDataTables(reinterpret_cast<uint32*>(this + 1));

    }

    return result;
}

// =====================================================================================================================
void ComputeCmdBuffer::ResetState()
{
    Pal::ComputeCmdBuffer::ResetState();

    m_pSignatureCs = &NullCsSignature;

    {
        // Non-DX12 clients and root command buffers start without a valid predicate GPU address.
        m_predGpuAddr = 0;
    }
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdBarrier(
    const BarrierInfo& barrierInfo)
{
    CmdBuffer::CmdBarrier(barrierInfo);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_gfxCmdBufState.packetPredicate;
    m_gfxCmdBufState.packetPredicate = 0;

    m_device.Barrier(this, &m_cmdStream, barrierInfo);

    m_gfxCmdBufState.packetPredicate = packetPredicate;
}

// =====================================================================================================================
// Issues a direct dispatch command. X, Y, and Z are in numbers of thread groups. We must discard the dispatch if x, y,
// or z are zero. To avoid branching, we will rely on the HW to discard the dispatch for us.
template <bool issueSqttMarkerEvent>
void PAL_STDCALL ComputeCmdBuffer::CmdDispatch(
    ICmdBuffer* pCmdBuffer,
    uint32      x,
    uint32      y,
    uint32      z)
{
    auto* pThis = static_cast<ComputeCmdBuffer*>(pCmdBuffer);

    if (issueSqttMarkerEvent)
    {
        pThis->m_device.DescribeDispatch(pThis, Developer::DrawDispatchType::CmdDispatch, 0, 0, 0, x, y, z);
    }

    uint32* pCmdSpace = pThis->m_cmdStream.ReserveCommands();

    pCmdSpace = pThis->ValidateDispatch(0uLL, x, y, z, pCmdSpace);

    const bool dimInThreads = pThis->NeedFixupMoreThan4096ThreadGroups();
    if (dimInThreads)
    {
        pThis->ConvertThreadGroupsToThreads(&x, &y, &z);
    }

    if (pThis->m_gfxCmdBufState.packetPredicate != 0)
    {
        pCmdSpace += pThis->m_cmdUtil.BuildCondExec(pThis->m_predGpuAddr, CmdUtil::GetDispatchDirectSize(), pCmdSpace);
    }

    constexpr bool ForceStartAt000 = true;
    pCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect(x, y, z, dimInThreads, ForceStartAt000, PredDisable, pCmdSpace);

    if (issueSqttMarkerEvent)
    {
        pCmdSpace += pThis->m_cmdUtil.BuildEventWrite(THREAD_TRACE_MARKER, pCmdSpace);
    }

    pThis->m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Issues an indirect dispatch command. We must discard the dispatch if x, y, or z are zero. We will rely on the HW to
// discard the dispatch for us.
template <bool issueSqttMarkerEvent>
void PAL_STDCALL ComputeCmdBuffer::CmdDispatchIndirect(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset)
{
    auto* pThis = static_cast<ComputeCmdBuffer*>(pCmdBuffer);

    if (issueSqttMarkerEvent)
    {
        pThis->m_device.DescribeDispatch(pThis, Developer::DrawDispatchType::CmdDispatchIndirect, 0, 0, 0, 0, 0, 0);
    }

    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)));
    PAL_ASSERT(offset + sizeof(DispatchIndirectArgs) <= gpuMemory.Desc().size);

    uint32* pCmdSpace = pThis->m_cmdStream.ReserveCommands();

    const gpusize gpuVirtAddr = (gpuMemory.Desc().gpuVirtAddr + offset);
    pCmdSpace = pThis->ValidateDispatch(gpuVirtAddr, 0, 0, 0, pCmdSpace);

    if (pThis->m_device.Parent()->ChipProperties().gfxLevel == GfxIpLevel::GfxIp6)
    {
        // Refer to comments added in CmdDispatch
        if (pThis->m_gfxCmdBufState.packetPredicate != 0)
        {
            pCmdSpace += pThis->m_cmdUtil.BuildCondExec(
                pThis->m_predGpuAddr,
                CmdUtil::GetSetBaseSize() + CmdUtil::GetDispatchIndirectSize(),
                pCmdSpace);
        }

        pCmdSpace += pThis->m_cmdUtil.BuildSetBase(
            ShaderCompute, BASE_INDEX_DRAW_INDIRECT, gpuMemory.Desc().gpuVirtAddr, pCmdSpace);
        pCmdSpace += pThis->m_cmdUtil.BuildDispatchIndirect(offset, PredDisable, pCmdSpace);
    }
    else
    {
        // Refer to comments added in CmdDispatch
        if (pThis->m_gfxCmdBufState.packetPredicate != 0)
        {
            pCmdSpace += pThis->m_cmdUtil.BuildCondExec(pThis->m_predGpuAddr,
                                                        CmdUtil::GetDispatchIndirectMecSize(),
                                                        pCmdSpace);
        }

        pCmdSpace += pThis->m_cmdUtil.BuildDispatchIndirectMec(gpuVirtAddr, pCmdSpace);
    }

    if (issueSqttMarkerEvent)
    {
        pCmdSpace += pThis->m_cmdUtil.BuildEventWrite(THREAD_TRACE_MARKER, pCmdSpace);
    }

    pThis->m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Issues an direct dispatch command with immediate threadgroup offsets. We must discard the dispatch if x, y, or z are
// zero. To avoid branching, we will rely on the HW to discard the dispatch for us.
template <bool issueSqttMarkerEvent>
void PAL_STDCALL ComputeCmdBuffer::CmdDispatchOffset(
    ICmdBuffer* pCmdBuffer,
    uint32      xOffset,
    uint32      yOffset,
    uint32      zOffset,
    uint32      xDim,
    uint32      yDim,
    uint32      zDim)
{
    auto* pThis = static_cast<ComputeCmdBuffer*>(pCmdBuffer);

    if (issueSqttMarkerEvent)
    {
        pThis->m_device.DescribeDispatch(pThis, Developer::DrawDispatchType::CmdDispatchOffset,
            xOffset, yOffset, zOffset, xDim, yDim, zDim);
    }

    uint32* pCmdSpace = pThis->m_cmdStream.ReserveCommands();

    pCmdSpace = pThis->ValidateDispatch(0uLL, xDim, yDim, zDim, pCmdSpace);

    const uint32 starts[3] = { xOffset, yOffset, zOffset };
    pCmdSpace  = pThis->m_cmdStream.WriteSetSeqShRegs(mmCOMPUTE_START_X,
                                                      mmCOMPUTE_START_Z,
                                                      ShaderCompute,
                                                      starts,
                                                      pCmdSpace);

    xDim += xOffset;
    yDim += yOffset;
    zDim += zOffset;

    const bool dimInThreads = pThis->NeedFixupMoreThan4096ThreadGroups();
    if (dimInThreads)
    {
        pThis->ConvertThreadGroupsToThreads(&xDim, &yDim, &zDim);
    }

    if (pThis->m_gfxCmdBufState.packetPredicate != 0)
    {
        pCmdSpace += pThis->m_cmdUtil.BuildCondExec(pThis->m_predGpuAddr, CmdUtil::GetDispatchDirectSize(), pCmdSpace);
    }

    // The DIM_X/Y/Z in DISPATCH_DIRECT packet are used to program COMPUTE_DIM_X/Y/Z registers, which are actually the
    // end block positions instead of execution block dimensions. So we need to use the dimensions plus offsets.
    constexpr bool ForceStartAt000 = false;
    pCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect(xDim,
                                                      yDim,
                                                      zDim,
                                                      dimInThreads,
                                                      ForceStartAt000,
                                                      PredDisable,
                                                      pCmdSpace);

    if (issueSqttMarkerEvent)
    {
        pCmdSpace += pThis->m_cmdUtil.BuildEventWrite(THREAD_TRACE_MARKER, pCmdSpace);
    }

    pThis->m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdCopyMemory(
    const IGpuMemory&       srcGpuMemory,
    const IGpuMemory&       dstGpuMemory,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions)
{
    m_device.RsrcProcMgr().CmdCopyMemory(this,
                                         static_cast<const GpuMemory&>(srcGpuMemory),
                                         static_cast<const GpuMemory&>(dstGpuMemory),
                                         regionCount,
                                         pRegions);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdUpdateMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           dataSize,
    const uint32*     pData)
{
    PAL_ASSERT(pData != nullptr);
    m_device.RsrcProcMgr().CmdUpdateMemory(this,
                                           static_cast<const GpuMemory&>(dstGpuMemory),
                                           dstOffset,
                                           dataSize,
                                           pData);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdUpdateBusAddressableMemoryMarker(
    const IGpuMemory& dstGpuMemory,
    gpusize           offset,
    uint32            value)
{
    const GpuMemory* pGpuMemory = static_cast<const GpuMemory*>(&dstGpuMemory);

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    pCmdSpace += m_cmdUtil.BuildWriteData(pGpuMemory->GetBusAddrMarkerVa() + offset,
                                          1,
                                          WRITE_DATA_ENGINE_ME,
                                          WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                          true,
                                          &value,
                                          PredDisable,
                                          pCmdSpace);
    m_cmdStream.CommitCommands(pCmdSpace);
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
// Issues either an end-of-pipe timestamp or a start of pipe timestamp event.  Writes the results to the pGpuMemory +
// destOffset.
void ComputeCmdBuffer::CmdWriteTimestamp(
    HwPipePoint       pipePoint,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    const gpusize address   = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;
    uint32*       pCmdSpace = m_cmdStream.ReserveCommands();

    if (pipePoint == HwPipeTop)
    {
        pCmdSpace += m_cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_ASYNC_MEMORY,
                                             address,
                                             COPY_DATA_SEL_SRC_GPU_CLOCK_COUNT,
                                             0,
                                             COPY_DATA_SEL_COUNT_2DW,
                                             COPY_DATA_ENGINE_ME,
                                             COPY_DATA_WR_CONFIRM_WAIT,
                                             pCmdSpace);
    }
    else
    {
        PAL_ASSERT(pipePoint == HwPipeBottom);

        // CmdUtil will properly route to EventWriteEop/ReleaseMem as appropriate.
        pCmdSpace += m_cmdUtil.BuildGenericEopEvent(BOTTOM_OF_PIPE_TS,
                                                    address,
                                                    EVENTWRITEEOP_DATA_SEL_SEND_GPU_CLOCK,
                                                    0,
                                                    true,
                                                    false,
                                                    pCmdSpace);
    }

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Writes an immediate value either during top-of-pipe or bottom-of-pipe event.
void ComputeCmdBuffer::CmdWriteImmediate(
    HwPipePoint        pipePoint,
    uint64             data,
    ImmediateDataWidth dataSize,
    gpusize            address)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    if (pipePoint == HwPipeTop)
    {
        pCmdSpace += m_cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_ASYNC_MEMORY,
                                             address,
                                             COPY_DATA_SEL_SRC_IMME_DATA,
                                             data,
                                             ((dataSize == ImmediateDataWidth::ImmediateData32Bit) ?
                                                 COPY_DATA_SEL_COUNT_1DW :
                                                 COPY_DATA_SEL_COUNT_2DW),
                                             COPY_DATA_ENGINE_ME,
                                             COPY_DATA_WR_CONFIRM_WAIT,
                                             pCmdSpace);
    }
    else
    {
        PAL_ASSERT(pipePoint == HwPipeBottom);

        // CmdUtil will properly route to EventWriteEop/ReleaseMem as appropriate.
        pCmdSpace += m_cmdUtil.BuildGenericEopEvent(BOTTOM_OF_PIPE_TS,
                                                    address,
                                                    ((dataSize == ImmediateDataWidth::ImmediateData32Bit) ?
                                                        EVENTWRITEEOP_DATA_SEL_SEND_DATA32 :
                                                        EVENTWRITEEOP_DATA_SEL_SEND_DATA64),
                                                    data,
                                                    true,
                                                    false,
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
        auto*const       pPipelineState = PipelineState(pipelineBindPoint);
        const auto*const pNewPalette    = static_cast<const BorderColorPalette*>(pPalette);
        const auto*const pOldPalette    = static_cast<const BorderColorPalette*>(pPipelineState->pBorderColorPalette);

        if (pNewPalette != nullptr)
        {
            uint32* pCmdSpace = m_cmdStream.ReserveCommands();
            pCmdSpace = pNewPalette->WriteCommands(pipelineBindPoint, &m_cmdStream, pCmdSpace);
            m_cmdStream.CommitCommands(pCmdSpace);
        }

        // Update the border-color palette state.
        pPipelineState->pBorderColorPalette                = pNewPalette;
        pPipelineState->dirtyFlags.borderColorPaletteDirty = 1;
    }
}

// =====================================================================================================================
// Helper function which is responsible for making sure all user-data entries are written to either the spill table or
// to user-SGPR's, as well as making sure that all indirect user-data tables are up-to-date in GPU memory.  Part of
// Dispatch-time validation.
template <bool HasPipelineChanged>
uint32* ComputeCmdBuffer::ValidateUserData(
    const ComputePipelineSignature* pPrevSignature, // In: Signature of pipeline bound for previous Dispatch. Will be
                                                    // nullptr if the pipeline is not changing.
    uint32*                         pCmdSpace)
{
    PAL_ASSERT((HasPipelineChanged  && (pPrevSignature != nullptr)) ||
               (!HasPipelineChanged && (pPrevSignature == nullptr)));

    // Step #1:
    // If any indirect user-data tables were updated since the previous Dispatch, and are referenced by the current
    // pipeline, they must be relocated to a new location in GPU memory, and re-uploaded.  This will result in the
    // user-data entry associated with those tables also being updated with the new address.
    for (uint16 tableId = 0; tableId < MaxIndirectUserDataTables; ++tableId)
    {
        const uint16 entryPlusOne = m_pSignatureCs->indirectTableAddr[tableId];
        if ((entryPlusOne != UserDataNotMapped) && (m_indirectUserDataInfo[tableId].watermark > 0))
        {
            bool relocated = false;
            if (m_indirectUserDataInfo[tableId].state.dirty)
            {
                UpdateUserDataTable(&m_indirectUserDataInfo[tableId].state,
                                    m_indirectUserDataInfo[tableId].watermark,
                                    0,
                                    m_indirectUserDataInfo[tableId].pData);
                relocated = true;
            }
            // The GPU virtual address for the indirect table needs to be updated if either the table was relocated,
            // or if the pipeline has changed and the previous pipeline's mapping for this table doesn't match the
            // new mapping.
            if ((HasPipelineChanged && (pPrevSignature->indirectTableAddr[tableId] != entryPlusOne)) || relocated)
            {
                const uint32 gpuVirtAddrLo = LowPart(m_indirectUserDataInfo[tableId].state.gpuVirtAddr);
                const uint16 entry         = (entryPlusOne - 1);

                WideBitfieldSetBit(m_computeState.csUserDataEntries.touched, entry);
                WideBitfieldSetBit(m_computeState.csUserDataEntries.dirty,   entry);
                m_computeState.csUserDataEntries.entries[entry] = gpuVirtAddrLo;
            }
        }
    } // for each indirect user-data table

    // Step #2:
    // All indirect user-data tables are now up-to-date in GPU memory, and their GPU virtual addresses are now stored
    // in the associated user-data entries.  It is now safe to write all dirty user-data entries to their mapped user
    // SGPR's (including any which were dirtied in step #1), and to check if the spill table needs updating.
    pCmdSpace = WriteDirtyUserDataEntries(pCmdSpace);

    if (m_pSignatureCs->spillThreshold != NoUserDataSpilling)
    {
        PAL_ASSERT(m_pSignatureCs->userDataLimit > 0);
        bool relocated = false;

        // Step #3:
        // The spill table will be marked dirty if the checks during step #2 above found that any dirty user-data falls
        // within the spilled region for the active pipeline.  Also, if the pipeline is changing, it is possible that
        // the region of the spill table which was relevant to that pipeline doesn't match the important region for the
        // new pipeline.  In that case, the spill table contents must also be updated.
        if ((HasPipelineChanged && ((m_pSignatureCs->spillThreshold < pPrevSignature->spillThreshold) ||
                                    (m_pSignatureCs->userDataLimit  > pPrevSignature->userDataLimit)))
            || m_spillTableCs.dirty)
        {
            const uint32 sizeInDwords = (m_pSignatureCs->userDataLimit - m_pSignatureCs->spillThreshold);

            UpdateUserDataTable(&m_spillTableCs,
                                sizeInDwords,
                                m_pSignatureCs->spillThreshold,
                                &m_computeState.csUserDataEntries.entries[0]);
            relocated = true;

        }

        // Step #4:
        // If the spill table was relocated during step #3, or if the pipeline is changing and the previous pipeline
        // did not spill any user-data to memory, we need to re-write the spill table GPU address to its user-SGPR.
        if ((HasPipelineChanged && (pPrevSignature->spillThreshold == NoUserDataSpilling)) || relocated)
        {
            pCmdSpace = m_cmdStream.WriteSetOneShReg<ShaderCompute>(m_pSignatureCs->stage.spillTableRegAddr,
                                                                    LowPart(m_spillTableCs.gpuVirtAddr),
                                                                    pCmdSpace);
        }
    } // if current pipeline uses the spill table

    return pCmdSpace;
}

// =====================================================================================================================
// Performs Dispatch-time validation of pipeline state and user-data entries.
uint32* ComputeCmdBuffer::ValidateDispatch(
    gpusize indirectGpuVirtAddr,
    uint32  xDim,
    uint32  yDim,
    uint32  zDim,
    uint32* pCmdSpace)
{
    if (m_computeState.pipelineState.dirtyFlags.pipelineDirty)
    {
        const auto*const pNewPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);

        pCmdSpace = pNewPipeline->WriteCommands(&m_cmdStream,
                                                pCmdSpace,
                                                m_computeState.dynamicCsInfo,
                                                *m_pPrefetchMgr);

        const auto*const pPrevSignature = m_pSignatureCs;
        m_pSignatureCs                  = &pNewPipeline->Signature();

        pCmdSpace = ValidateUserData<true>(pPrevSignature, pCmdSpace);
    }
    else
    {
        pCmdSpace = ValidateUserData<false>(nullptr, pCmdSpace);
    }

    m_computeState.pipelineState.dirtyFlags.u32All = 0;

    if (m_pSignatureCs->numWorkGroupsRegAddr != UserDataNotMapped)
    {
        // Indirect Dispatches by definition have the number of thread-groups to launch stored in GPU memory at the
        // specified address.  However, for direct Dispatches, we must allocate some embedded memory to store this
        // information.
        if (indirectGpuVirtAddr == 0uLL) // This is a direct Dispatch.
        {
            uint32*const pData = CmdAllocateEmbeddedData(3, 4, &indirectGpuVirtAddr);
            pData[0] = xDim;
            pData[1] = yDim;
            pData[2] = zDim;
        }

        pCmdSpace = m_cmdStream.WriteSetSeqShRegs(m_pSignatureCs->numWorkGroupsRegAddr,
                                                  (m_pSignatureCs->numWorkGroupsRegAddr + 1),
                                                  ShaderCompute,
                                                  &indirectGpuVirtAddr,
                                                  pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Helper function responsible for writing all dirty user-data entries to their respective user-SGPR's.  Also checks if
// any dirty user-data entries fall into the spill-table region and marks the spill table dirty accordingly.
uint32* ComputeCmdBuffer::WriteDirtyUserDataEntries(
    uint32* pCmdSpace)
{
    // Compute pipelines all use a fixed user-data mapping of entries to user-SGPR's, because compute command buffers
    // are not able to use LOAD_SH_REG packets, which are used for inheriting user-data entries in a nested command
    // buffer.  The only way to correctly handle user-data inheritance is by using a fixed mapping.  This has the side
    // effect of allowing us to know that only the first few entries ever need to be written to user-SGPR's, which lets
    // us get away with only checking the first sub-mask of the user-data entries' wide-bitfield of dirty flags.
    static_assert(MaxFastUserDataEntriesCs <= UserDataEntriesPerMask,
                  "The CS user-data entries mapped to user-SGPR's spans multiple wide-bitfield elements!");
    constexpr uint32 AllFastUserDataEntriesMask = ((1 << MaxFastUserDataEntriesCs) - 1);
    const uint16 userSgprDirtyMask = (m_computeState.csUserDataEntries.dirty[0] & AllFastUserDataEntriesMask);

    // Additionally, dirty compute user-data is always written to user-SGPR's if it could be mapped by a pipeline,
    // which lets us avoid any complex logic when switching pipelines.
    constexpr uint16 BaseUserSgpr = FirstUserDataRegAddr[static_cast<uint32>(HwShaderStage::Cs)];

    uint16 lastEntry = 0;
    uint16 count     = 0;
    for (uint16 e = 0; e < MaxFastUserDataEntriesCs; ++e)
    {
        while ((e < MaxFastUserDataEntriesCs) && ((userSgprDirtyMask & (1 << e)) != 0))
        {
            PAL_ASSERT((lastEntry == 0) || (lastEntry == (e - 1)));
            lastEntry = e;
            ++count;
            ++e;
        }

        if (count > 0)
        {
            const uint16 firstEntry = (lastEntry - count + 1);
            pCmdSpace = m_cmdStream.WriteSetSeqShRegs((BaseUserSgpr + firstEntry),
                                                      (BaseUserSgpr + lastEntry),
                                                      ShaderCompute,
                                                      &m_computeState.csUserDataEntries.entries[firstEntry],
                                                      pCmdSpace);

            // Reset accumulators for the next packet.
            lastEntry = 0;
            count     = 0;
        }
    }

    // If the currently active pipeline spills any entries to GPU memory, we need to check if any of the dirty
    // user-data entries fall within the spilled region for the current pipeline.
    if (m_pSignatureCs->spillThreshold != NoUserDataSpilling)
    {
        PAL_ASSERT(m_pSignatureCs->userDataLimit != 0);

        // Since the spill table is managed by the CPU in embedded memory, it needs to be fully "re-uploaded" for
        // each Dispatch whenever any contents change.  Therefore, the following loop just needs to check the
        // relevant dirty flags and mark the spill table dirty if any were set.
        const uint32 firstMaskId = (m_pSignatureCs->spillThreshold / UserDataEntriesPerMask);
        const uint32 lastMaskId  = ((m_pSignatureCs->userDataLimit - 1) / UserDataEntriesPerMask);
        for (uint32 maskId = firstMaskId; maskId <= lastMaskId; ++maskId)
        {
            uint16 dirtyMask = m_computeState.csUserDataEntries.dirty[maskId];
            if (maskId == firstMaskId)
            {
                // Ignore the dirty bits for any entries below the spill threshold.
                const uint16 firstEntryInMask = (m_pSignatureCs->spillThreshold & (UserDataEntriesPerMask - 1));
                dirtyMask &= ~((1 << firstEntryInMask) - 1);
            }
            if (maskId == lastMaskId)
            {
                // Ignore the dirty bits for any entries beyond the user-data limit.
                const uint16 lastEntryInMask = ((m_pSignatureCs->userDataLimit - 1) & (UserDataEntriesPerMask - 1));
                dirtyMask &= ((1 << (lastEntryInMask + 1)) - 1);
            }

            if (dirtyMask != 0)
            {
                m_spillTableCs.dirty = 1;
                m_computeState.csUserDataEntries.dirty[maskId] &= ~dirtyMask;
            }
        } // for each wide-bitfield sub-mask
    } // if current pipeline spills user-data

    // Clear all dirty bits for user-data entries which were written to user-SGPR's.  These are cleared last because
    // some entries may be simultaneously spilled to GPU memory and mapped to a user-SGPR.
    m_computeState.csUserDataEntries.dirty[0] &= ~AllFastUserDataEntriesMask;

    return pCmdSpace;
}

// =====================================================================================================================
// Adds PM4 commands needed to write any registers associated with starting a query.
void ComputeCmdBuffer::AddQuery(
    QueryPoolType     queryPoolType,
    QueryControlFlags flags)
{
    // PIPELINE_START event was issued in the preamble, so no need to do anything here.
}

// =====================================================================================================================
// Adds PM4 commands needed to write any registers associated with ending the last active query in this command buffer.
void ComputeCmdBuffer::RemoveQuery(
    QueryPoolType queryPoolType)
{
    // We're not bothering with PIPELINE_STOP events, as leaving these counters running doesn't hurt anything.
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdLoadGds(
    HwPipePoint       pipePoint,
    uint32            dstGdsOffset,
    const IGpuMemory& srcGpuMemory,
    gpusize           srcMemOffset,
    uint32            size)
{
    BuildLoadGds(&m_cmdStream,
                 &m_cmdUtil,
                 pipePoint,
                 dstGdsOffset,
                 srcGpuMemory,
                 srcMemOffset,
                 size);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdStoreGds(
    HwPipePoint       pipePoint,
    uint32            srcGdsOffset,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstMemOffset,
    uint32            size,
    bool              waitForWC)
{
    BuildStoreGds(&m_cmdStream,
                  &m_cmdUtil,
                  pipePoint,
                  srcGdsOffset,
                  dstGpuMemory,
                  dstMemOffset,
                  size,
                  waitForWC,
                  true,
                  TimestampGpuVirtAddr());
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdUpdateGds(
    HwPipePoint       pipePoint,
    uint32            gdsOffset,
    uint32            dataSize,
    const uint32*     pData)
{
    BuildUpdateGds(&m_cmdStream,
                   &m_cmdUtil,
                   pipePoint,
                   gdsOffset,
                   dataSize,
                   pData);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdFillGds(
    HwPipePoint       pipePoint,
    uint32            gdsOffset,
    uint32            fillSize,
    uint32            data)
{
    BuildFillGds(&m_cmdStream,
                 &m_cmdUtil,
                 pipePoint,
                 gdsOffset,
                 fillSize,
                 data);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdBeginQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot,
    QueryControlFlags flags)
{
    static_cast<const QueryPool&>(queryPool).Begin(this, &m_cmdStream, queryType, slot, flags);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdEndQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot)
{
    static_cast<const QueryPool&>(queryPool).End(this, &m_cmdStream, queryType, slot);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdResetQueryPool(
    const IQueryPool& queryPool,
    uint32            startQuery,
    uint32            queryCount)
{
    static_cast<const QueryPool&>(queryPool).Reset(this, &m_cmdStream, startQuery, queryCount);
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
    dmaData.dstSel       = CPDMA_DST_SEL_DST_ADDR;
    dmaData.dstAddr      = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;
    dmaData.dstAddrSpace = CPDMA_ADDR_SPACE_MEM;
    dmaData.srcSel       = CPDMA_SRC_SEL_SRC_ADDR;
    dmaData.srcAddr      = srcRegisterOffset;
    dmaData.srcAddrSpace = CPDMA_ADDR_SPACE_REG;
    dmaData.sync         = true;
    dmaData.usePfp       = false;
    pCmdSpace += m_cmdUtil.BuildDmaData(dmaData, pCmdSpace);

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

    pCmdSpace += m_cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_REGISTER,
                                           CmdUtil::WaitRegMemFuncFromCompareType(compareFunc),
                                           WAIT_REG_MEM_ENGINE_ME,
                                           registerOffset,
                                           data,
                                           mask,
                                           false,
                                           pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdWaitMemoryValue(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    const GpuMemory* pGpuMemory = static_cast<const GpuMemory*>(&gpuMemory);

    pCmdSpace += m_cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_MEMORY,
                                           CmdUtil::WaitRegMemFuncFromCompareType(compareFunc),
                                           WAIT_REG_MEM_ENGINE_ME,
                                           gpuMemory.Desc().gpuVirtAddr + offset,
                                           data,
                                           mask,
                                           pGpuMemory->IsBusAddressable(),
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
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    const GpuMemory* pGpuMemory = static_cast<const GpuMemory*>(&gpuMemory);

    pCmdSpace += m_cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_MEMORY,
                                           CmdUtil::WaitRegMemFuncFromCompareType(compareFunc),
                                           WAIT_REG_MEM_ENGINE_ME,
                                           pGpuMemory->GetBusAddrMarkerVa(),
                                           data,
                                           mask,
                                           pGpuMemory->IsBusAddressable(),
                                           pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32            cmdBufferCount,
    ICmdBuffer*const* ppCmdBuffers)
{
    for (uint32 buf = 0; buf < cmdBufferCount; ++buf)
    {
        auto*const pCallee = static_cast<Gfx6::ComputeCmdBuffer*>(ppCmdBuffers[buf]);
        PAL_ASSERT(pCallee != nullptr);

        // Track the most recent OS paging fence value across all nested command buffers called from this one.
        m_lastPagingFence = Max(m_lastPagingFence, pCallee->LastPagingFence());

        // All user-data entries have been uploaded into the GPU memory the callee expects to receive them in, so we
        // can safely "call" the nested command buffer's command stream.
        m_cmdStream.TrackNestedEmbeddedData(pCallee->m_embeddedData.chunkList);
        m_cmdStream.TrackNestedCommands(pCallee->m_cmdStream);
        m_cmdStream.Call(pCallee->m_cmdStream, pCallee->IsExclusiveSubmit(), false);

        // Callee command buffers are also able to leak any changes they made to bound user-data entries and any other
        // state back to the caller.
        LeakNestedCmdBufferState(*pCallee);
    }
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdCommentString(
    const char* pComment)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace += m_cmdUtil.BuildCommentString(pComment, pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdExecuteIndirectCmds(
    const IIndirectCmdGenerator& generator,
    const IGpuMemory&            gpuMemory,
    gpusize                      offset,
    uint32                       maximumCount,
    gpusize                      countGpuAddr)
{
    // It is only safe to generate indirect commands on a one-time-submit or exclusive-submit command buffer because
    // there is a potential race condition on the memory used to receive the generated commands.
    PAL_ASSERT(IsOneTimeSubmit() || IsExclusiveSubmit());

    const auto& gfx6Generator = static_cast<const IndirectCmdGenerator&>(generator);

    if (countGpuAddr == 0uLL)
    {
        // If the count GPU address is zero, then we are expected to use the maximumCount value as the actual number
        // of indirect commands to generate and execute.
        uint32* pMemory = CmdAllocateEmbeddedData(1, 1, &countGpuAddr);
        *pMemory = maximumCount;
    }

    // NOTE: Save an iterator to the current end of the generated-chunk list. Each command buffer chunk generated by
    // the call to RPM below will be added to the end of the list, so we can iterate over the new chunks starting
    // from the first item in the list following this iterator.
    auto chunkIter = m_generatedChunkList.End();

    // Generate the indirect command buffer chunk(s) using RPM. Since we're wrapping the command generation and
    // execution inside a CmdIf, we want to disable normal predication for this blit.
    const uint32 packetPredicate = m_gfxCmdBufState.packetPredicate;
    m_gfxCmdBufState.packetPredicate = 0;

    constexpr uint32 DummyIndexBufSize = 0; // Compute doesn't care about the index buffer size.
    m_device.RsrcProcMgr().CmdGenerateIndirectCmds(this,
                                                   m_computeState.pipelineState.pPipeline,
                                                   gfx6Generator,
                                                   (gpuMemory.Desc().gpuVirtAddr + offset),
                                                   countGpuAddr,
                                                   DummyIndexBufSize,
                                                   maximumCount);

    m_gfxCmdBufState.packetPredicate = packetPredicate;

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    // Insert a CS_PARTIAL_FLUSH and invalidate/flush the texture caches to make sure that the generated commands
    // are written out to memory before we attempt to execute them.
    regCP_COHER_CNTL cpCoherCntl = { };
    cpCoherCntl.u32All = CpCoherCntlTexCacheMask;

    pCmdSpace += m_cmdUtil.BuildEventWrite(CS_PARTIAL_FLUSH, pCmdSpace);
    pCmdSpace += m_cmdUtil.BuildGenericSync(cpCoherCntl,
                                            SURFACE_SYNC_ENGINE_ME,
                                            FullSyncBaseAddr,
                                            FullSyncSize,
                                            true,
                                            pCmdSpace);
    if (m_cmdUtil.IpLevel() == GfxIpLevel::GfxIp6)
    {
        // On GFXIP 6, we need to issue a PFP_SYNC_ME packet to prevent the PFP from prefetching the generated
        // command chunk(s) before the generation shader has finished.
        pCmdSpace += m_cmdUtil.BuildPfpSyncMe(pCmdSpace);
    }
    else
    {
        // On GFXIP 7+, PFP_SYNC_ME cannot be used on an async compute engine, so we need to use REWIND packet
        // isntead.
        pCmdSpace += m_cmdUtil.BuildRewind(false, true, pCmdSpace);
    }

    // Just like a normal direct/indirect dispatch, we need to perform state validation before executing the
    // generated command chunks.
    pCmdSpace = ValidateDispatch(0uLL, 0, 0, 0, pCmdSpace);
    m_cmdStream.CommitCommands(pCmdSpace);

    CommandGeneratorTouchedUserData(m_computeState.csUserDataEntries.touched, gfx6Generator, *m_pSignatureCs);

    // NOTE: The command stream expects an iterator to the first chunk to execute, but this iterator points to the
    // place in the list before the first generated chunk (see comments above).
    chunkIter.Next();
    m_cmdStream.ExecuteGeneratedCommands(chunkIter);

}

// =====================================================================================================================
CmdStreamChunk* ComputeCmdBuffer::GetChunkForCmdGeneration(
    const Pal::IndirectCmdGenerator& generator,
    const Pal::Pipeline&             pipeline,
    uint32                           maxCommands,
    uint32*                          pCommandsInChunk, // Out: How many commands can safely fit into the command chunk
    gpusize*                         pEmbeddedDataAddr,
    uint32*                          pEmbeddedDataSize)
{
    const GeneratorProperties&      properties = generator.Properties();
    const ComputePipelineSignature& signature  = static_cast<const ComputePipeline&>(pipeline).Signature();

    PAL_ASSERT(m_pCmdAllocator != nullptr);

    CmdStreamChunk*const pChunk = Pal::GfxCmdBuffer::GetNextGeneratedChunk();

    // NOTE: RPM uses a compute shader to generate indirect commands, so we need to use the saved user-data state
    // because RPM will have pushed its own state before calling this method.
    const uint32* pUserDataEntries = &m_computeRestoreState.csUserDataEntries.entries[0];

    // Total amount of embedded data space needed for each generated command, including indirect user-data tables and
    // user-data spilling.
    uint32 embeddedDwords = 0;
    // Amount of embedded data space needed for each generated command, per indirect user-data table:
    uint32 indirectTableDwords[MaxIndirectUserDataTables] = { };
    // User-data high watermark for this command Generator. It depends on the command Generator itself, as well as the
    // pipeline signature for the active pipeline. This is due to the fact that if the command Generator modifies the
    // contents of an indirect user-data table, the command Generator must also fix-up the user-data entry used for the
    // table's GPU virtual address.
    uint32 userDataWatermark = properties.userDataWatermark;

    for (uint32 id = 0; id < MaxIndirectUserDataTables; ++id)
    {
        if ((signature.indirectTableAddr[id] != 0) &&
            (properties.indirectUserDataThreshold[id] < m_device.Parent()->IndirectUserDataTableSize(id)))
        {
            userDataWatermark       = Max<uint32>(userDataWatermark, (signature.indirectTableAddr[id] - 1));
            indirectTableDwords[id] = static_cast<uint32>(m_device.Parent()->IndirectUserDataTableSize(id));
            embeddedDwords         += indirectTableDwords[id];
        }
    }

    const uint32 commandDwords = (generator.Properties().cmdBufStride / sizeof(uint32));
    // There are three possibilities when determining how much spill-table space a generated command will need:
    //  (1) The active pipeline doesn't spill at all. This requires no spill-table space.
    //  (2) The active pipeline spills, but the generator doesn't update the any user-data entries beyond the
    //      spill threshold. This requires no spill-table space.
    //  (3) The active pipeline spills, and the generator updates user-data entries which are beyond the spill
    //      threshold. This means each generated command needs to relocate the spill table in addition to the other
    ///     stuff it would normally do.
    const uint32 spillDwords =
        (signature.spillThreshold < properties.userDataWatermark) ? properties.maxUserDataEntries : 0;
    embeddedDwords += spillDwords;

    // Ask the DE command stream to make sure the command chunk is ready to receive GPU-generated commands (this
    // includes setting up padding for size alignment, allocating command space, etc.
    (*pCommandsInChunk)  = m_cmdStream.PrepareChunkForCmdGeneration(pChunk, commandDwords, embeddedDwords, maxCommands);
    (*pEmbeddedDataSize) = ((*pCommandsInChunk) * embeddedDwords);

    if (spillDwords > 0)
    {
        // If each generated command requires some amount of spill-table space, then we need to allocate embeded data
        // space for all of the generated commands which will go into this chunk. PrepareChunkForCmdGeneration() should
        // have determined a value for commandsInChunk which allows us to allocate the appropriate amount of embeded
        // data space.
        uint32* pDataSpace = pChunk->ValidateCmdGenerationDataSpace((*pEmbeddedDataSize), pEmbeddedDataAddr);

        // We also need to seed the embedded data for each generated command with the current indirect user-data table
        // and spill-table contents, because the generator will only update the table entries which get modified.
        for (uint32 cmd = 0; cmd < (*pCommandsInChunk); ++cmd)
        {
            for (uint32 id = 0; id < MaxIndirectUserDataTables; ++id)
            {
                memcpy(pDataSpace,
                       m_indirectUserDataInfo[id].pData,
                       (sizeof(uint32) * m_indirectUserDataInfo[id].watermark));
                pDataSpace += indirectTableDwords[id];
            }
            memcpy(pDataSpace, pUserDataEntries, (sizeof(uint32) * spillDwords));
            pDataSpace += spillDwords;
        }
    }

    return pChunk;
}

// =====================================================================================================================
// Helper method for handling the state "leakage" from a nested command buffer back to its caller. Since the callee has
// tracked its own state during the building phase, we can access the final state of the command buffer since its stored
// in the UniversalCmdBuffer object itself.
void ComputeCmdBuffer::LeakNestedCmdBufferState(
    const ComputeCmdBuffer& cmdBuffer)
{
    Pal::ComputeCmdBuffer::LeakNestedCmdBufferState(cmdBuffer);

    // Invalidate PM4 optimizer state on post-execute since the current command buffer state does not reflect
    // state changes from the nested command buffer. We will need to resolve the nested PM4 state onto the
    // current command buffer for this to work correctly.
    m_cmdStream.NotifyNestedCmdBufferExecute();
}

// =====================================================================================================================
// Checks if the workaround for more than 4096 thread groups needs to be applied. Returns true indicates the dimensions
// need to be converted in unit of threads.
bool ComputeCmdBuffer::NeedFixupMoreThan4096ThreadGroups() const
{
    // CP has a bug for async compute dispatch when thread groups > 4096, which may cause hang. The workaround is to
    // change the "threadgroup" dimension mode to "thread" dimension mode. Note that if there are multiple dispatches on
    // the same "queue" (should be "queue" of multi-queue compute pipe) with the total sum being greater than 4096, the
    // asic might hang as well. As we don't know the exact number of thread groups currently being launched, we always
    // use thread dimension mode for async compute dispatches when the workaround bit is set.
    return m_device.WaAsyncComputeMoreThan4096ThreadGroups();
}

// =====================================================================================================================
// Converting dimensions from numbers of thread groups to numbers of threads.
void ComputeCmdBuffer::ConvertThreadGroupsToThreads(
    uint32* pX,
    uint32* pY,
    uint32* pZ
    ) const
{
    PAL_ASSERT((pX != nullptr) && (pY != nullptr) && (pZ != nullptr));

    const auto*const pPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);

    uint32 threadsPerGroup[3] = {};
    pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

    (*pX) *= threadsPerGroup[0];
    (*pY) *= threadsPerGroup[1];
    (*pZ) *= threadsPerGroup[2];
}

// =====================================================================================================================
// Adds a preamble to the start of a new command buffer.
// SEE: ComputePreamblePm4Img and CommonPreamblePm4Img structures in gfx6Preambles.h for what is written in the preamble
Result ComputeCmdBuffer::AddPreamble()
{
    // If this trips, it means that this isn't really the preamble -- i.e., somebody has inserted something into the
    // command stream before the preamble.  :-(
    PAL_ASSERT(m_cmdStream.IsEmpty());

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    pCmdSpace += m_cmdUtil.BuildEventWrite(PIPELINESTAT_START, pCmdSpace);
    m_cmdStream.CommitCommands(pCmdSpace);

    return Result::Success;
}

// =====================================================================================================================
// Adds a postamble to the end of a new command buffer.
Result ComputeCmdBuffer::AddPostamble()
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    if (m_gfxCmdBufState.cpBltActive)
    {
        // Stalls the CP MEC until the CP's DMA engine has finished all previous "CP blts" (CP_DMA/DMA_DATA commands
        // without the sync bit set). The ring won't wait for CP DMAs to finish so we need to do this manually.
        pCmdSpace += m_cmdUtil.BuildWaitDmaData(pCmdSpace);
        SetGfxCmdBufCpBltState(false);
    }

    // The following ATOMIC_MEM packet increments the done-count for the command stream, so that we can probe when the
    // command buffer has completed execution on the GPU.
    // NOTE: Normally, we would need to flush the L2 cache to guarantee that this memory operation makes it out to
    // memory. However, since we're at the end of the command buffer, we can rely on the fact that the KMD inserts
    // an EOP event which flushes and invalidates the caches in between command buffers.
    if (m_cmdStream.GetFirstChunk()->BusyTrackerGpuAddr() != 0)
    {
        pCmdSpace += m_cmdUtil.BuildAtomicMem(AtomicOp::AddInt32,
                                              m_cmdStream.GetFirstChunk()->BusyTrackerGpuAddr(),
                                              1,
                                              pCmdSpace);
    }

    m_cmdStream.CommitCommands(pCmdSpace);

    return Result::Success;
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
    pCmdSpace += m_cmdUtil.BuildEventWrite(PIPELINESTAT_START, pCmdSpace);
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
    pCmdSpace += m_cmdUtil.BuildEventWrite(PIPELINESTAT_STOP, pCmdSpace);
    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Adds commands necessary to write "data" to the specified event's memory.
void ComputeCmdBuffer::WriteEventCmd(
    const BoundGpuMemory& boundMemObj,
    HwPipePoint           pipePoint,
    uint32                data)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    if ((pipePoint >= HwPipePostBlt) && (m_gfxCmdBufState.cpBltActive))
    {
        // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
        // the CmdSetEvent and CmdResetEvent functions expect that the prior blts have reached the post-blt stage by
        // the time the event is written to memory. Given that our CP DMA blts are asynchronous to the pipeline stages
        // the only way to satisfy this requirement is to force the MEC to stall until the CP DMAs are completed.
        pCmdSpace += m_cmdUtil.BuildWaitDmaData(pCmdSpace);
        SetGfxCmdBufCpBltState(false);
    }

    if ((pipePoint == HwPipeTop) || (pipePoint == HwPipePreCs))
    {
        // Implement set/reset event with a WRITE_DATA command using the CP.
        pCmdSpace += m_cmdUtil.BuildWriteData(boundMemObj.GpuVirtAddr(),
                                              1,
                                              0, // ignored for compute
                                              WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                              true,
                                              &data,
                                              PredDisable,
                                              pCmdSpace);
    }
    else if (pipePoint == HwPipePostCs)
    {
        // Implement set/reset with an EOS event waiting for CS waves to complete.
        pCmdSpace += m_cmdUtil.BuildGenericEosEvent(CS_DONE,
                                                    boundMemObj.GpuVirtAddr(),
                                                    EVENT_WRITE_EOS_CMD_STORE_32BIT_DATA_TO_MEMORY,
                                                    data,
                                                    0,
                                                    0,
                                                    true,
                                                    pCmdSpace);
    }
    else
    {
        // Don't expect to see HwPipePreRasterization or HwPipePostPs on the compute queue...
        PAL_ASSERT(pipePoint == HwPipeBottom);

        // Implement set/reset with an EOP event written when all prior GPU work completes.  HwPipeBottom shouldn't be
        // much different than HwPipePostCs on a compute queue, but this command will ensure proper ordering if any
        // other EOP events were used (e.g., CmdWriteTimestamp).
        pCmdSpace += m_cmdUtil.BuildGenericEopEvent(BOTTOM_OF_PIPE_TS,
                                                    boundMemObj.GpuVirtAddr(),
                                                    EVENTWRITEEOP_DATA_SEL_SEND_DATA32,
                                                    data,
                                                    true,
                                                    false,
                                                    pCmdSpace);
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
    // This emulation doesn't work for QueryPool based predication, fortunately DX12 just has Boolean type
    // predication.
    PAL_ASSERT((predType == PredicateType::Boolean) && (pQueryPool == nullptr));

    // When gpuVirtAddr is 0, it means client is disabling/resetting predication
    m_gfxCmdBufState.clientPredicate = (pGpuMemory != nullptr);
    m_gfxCmdBufState.packetPredicate = m_gfxCmdBufState.clientPredicate;

    if (pGpuMemory != nullptr)
    {
        gpusize gpuVirtAddr  = pGpuMemory->Desc().gpuVirtAddr + offset;
        uint32 *pPredCpuAddr = CmdAllocateEmbeddedData(1, 1, &m_predGpuAddr);

        uint32 *pCmdSpace    = m_cmdStream.ReserveCommands();

        // Execute if 64-bit value in memory are all 0 when predPolarity is false,
        // or Execute if one or more bits of 64-bit value in memory are not 0 when predPolarity is true.
        uint32 predCopyData  = (predPolarity == true);
        *pPredCpuAddr        = (predPolarity == false);

        pCmdSpace += m_cmdUtil.BuildCondExec(gpuVirtAddr, CmdUtil::GetWriteDataHeaderSize() + 1, pCmdSpace);
        pCmdSpace += m_cmdUtil.BuildWriteData(m_predGpuAddr,
                                              1,
                                              WRITE_DATA_ENGINE_PFP,
                                              WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                              true,
                                              &predCopyData,
                                              PredDisable,
                                              pCmdSpace);

        pCmdSpace += m_cmdUtil.BuildCondExec(gpuVirtAddr + 4, CmdUtil::GetWriteDataHeaderSize() + 1, pCmdSpace);
        pCmdSpace += m_cmdUtil.BuildWriteData(m_predGpuAddr,
                                              1,
                                              WRITE_DATA_ENGINE_PFP,
                                              WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                              true,
                                              &predCopyData,
                                              PredDisable,
                                              pCmdSpace);

        m_cmdStream.CommitCommands(pCmdSpace);
    }
    else
    {
        m_predGpuAddr = 0;
    }
}

// =====================================================================================================================
void ComputeCmdBuffer::AddPerPresentCommands(
    gpusize frameCountGpuAddr,
    uint32  frameCntReg)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace += m_cmdUtil.BuildAtomicMem(AtomicOp::IncUint32,
                                          frameCountGpuAddr,
                                          UINT32_MAX,
                                          pCmdSpace);

    pCmdSpace += m_cmdUtil.BuildCopyData(COPY_DATA_SEL_DST_SYS_PERF_COUNTER,
                                         frameCntReg,
                                         COPY_DATA_SEL_SRC_TC_L2,
                                         frameCountGpuAddr,
                                         COPY_DATA_SEL_COUNT_1DW,
                                         COPY_DATA_ENGINE_ME,
                                         COPY_DATA_WR_CONFIRM_NO_WAIT,
                                         pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdInsertRgpTraceMarker(
    uint32      numDwords,
    const void* pData)
{
    // The first dword of every RGP trace marker packet is written to SQ_THREAD_TRACE_USERDATA_2.  The second dword
    // is written to SQ_THREAD_TRACE_USERDATA_3.  For packets longer than 64-bits, continue alternating between
    // user data 2 and 3.

    const uint32 userDataAddr = m_device.CmdUtil().GetRegInfo().mmSqThreadTraceUserData2;
    PAL_ASSERT(m_device.CmdUtil().IsPrivilegedConfigReg(userDataAddr) == false);
    PAL_ASSERT(m_device.CmdUtil().GetRegInfo().mmSqThreadTraceUserData3 == (userDataAddr + 1));

    const uint32* pDwordData = static_cast<const uint32*>(pData);
    while (numDwords > 0)
    {
        const uint32 dwordsToWrite = Min(numDwords, 2u);

        // Reserve and commit command space inside this loop.  Some of the RGP packets are unbounded, like adding a
        // comment string, so it's not safe to assume the whole packet will fit under our reserve limit.
        uint32* pCmdSpace = m_cmdStream.ReserveCommands();

        pCmdSpace = m_cmdStream.WriteSetSeqConfigRegs(userDataAddr,
                                                      userDataAddr + dwordsToWrite - 1,
                                                      pDwordData,
                                                      pCmdSpace);
        pDwordData += dwordsToWrite;
        numDwords -= dwordsToWrite;

        m_cmdStream.CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Bind the last state set on the specified command buffer
void ComputeCmdBuffer::InheritStateFromCmdBuf(
    const GfxCmdBuffer* pCmdBuffer)
{
    const ComputeCmdBuffer* pComputeCmdBuffer = static_cast<const ComputeCmdBuffer*>(pCmdBuffer);
    SetComputeState(pCmdBuffer->GetComputeState(), ComputeStateAll);

    for (uint16 i = 0; i < MaxIndirectUserDataTables; i++)
    {
        const uint32  numEntries = pComputeCmdBuffer->m_indirectUserDataInfo[i].watermark;
        const uint32* pData      = pComputeCmdBuffer->m_indirectUserDataInfo[i].pData;
        if (numEntries > 0)
        {
            CmdSetIndirectUserData(i, 0, numEntries, pData);
        }
    }
}

// =====================================================================================================================
// Copy memory using the CP's DMA engine
void ComputeCmdBuffer::CpCopyMemory(
    gpusize dstAddr,
    gpusize srcAddr,
    gpusize numBytes)
{
    // We want to read and write through L2 because it's faster and expected by CoherCopy but if it isn't supported
    // we need to fall back to a memory-to-memory copy.
    const bool supportsL2 = (m_device.Parent()->ChipProperties().gfxLevel > GfxIpLevel::GfxIp6);

    PAL_ASSERT(numBytes < (1ull << 32));

    DmaDataInfo dmaDataInfo = {};
    dmaDataInfo.dstSel      = supportsL2 ? CPDMA_DST_SEL_DST_ADDR_USING_L2 : CPDMA_DST_SEL_DST_ADDR;
    dmaDataInfo.srcSel      = supportsL2 ? CPDMA_SRC_SEL_SRC_ADDR_USING_L2 : CPDMA_SRC_SEL_SRC_ADDR;
    dmaDataInfo.sync        = false;
    dmaDataInfo.usePfp      = false;
    dmaDataInfo.dstAddr     = dstAddr;
    dmaDataInfo.srcAddr     = srcAddr;
    dmaDataInfo.numBytes    = static_cast<uint32>(numBytes);

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    if (m_gfxCmdBufState.packetPredicate != 0)
    {
        pCmdSpace += m_cmdUtil.BuildCondExec(m_predGpuAddr, m_cmdUtil.GetDmaDataSizeInDwords(dmaDataInfo), pCmdSpace);
    }
    pCmdSpace += m_cmdUtil.BuildDmaData(dmaDataInfo, pCmdSpace);
    m_cmdStream.CommitCommands(pCmdSpace);

    SetGfxCmdBufCpBltState(true);

    if (supportsL2)
    {
        SetGfxCmdBufCpBltWriteCacheState(true);
    }
    else
    {
        SetGfxCmdBufCpMemoryWriteL2CacheStaleState(true);
    }
}

} // Gfx6
} // Pal
