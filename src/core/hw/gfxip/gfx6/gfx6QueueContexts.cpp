/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/cmdAllocator.h"
#include "core/platform.h"
#include "core/queue.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6ComputeEngine.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6QueueContexts.h"
#include "core/hw/gfxip/gfx6/gfx6ShaderRingSet.h"
#include "core/hw/gfxip/gfx6/gfx6UniversalEngine.h"
#include "core/hw/gfxip/gfx6/g_gfx6PalSettings.h"
#include "core/hw/gfxip/gfx6/g_gfx6ShadowedRegistersInit.h"
#include "core/hw/gfxip/universalCmdBuffer.h"
#include "palAssert.h"

#include <limits.h>

using namespace Util;

namespace Pal
{
namespace Gfx6
{

static uint32* WriteTrapInstallCmds(
    const Device*     pDevice,
    CmdStream*        pCmdStream,
    PipelineBindPoint pipelineType,
    uint32*           pCmdSpace);

constexpr uint32 CmdStreamPerSubmit = 0;
constexpr uint32 CmdStreamContext   = 1;

// =====================================================================================================================
// Writes commands which are common to the preambles for Compute and Universal queues.
static uint32* WriteCommonPreamble(
    const Device& device,
    EngineType    engineType,
    CmdStream*    pCmdStream,
    uint32*       pCmdSpace)
{
    const GpuChipProperties& chipProps = device.Parent()->ChipProperties();

    // It's legal to set the CU mask to enable all CUs. The UMD does not need to know about active CUs and harvested
    // CUs at this point. Using the packet SET_SH_REG_INDEX, the UMD mask will be ANDed with the KMD mask so that UMD
    // does not use the CUs that are intended for real time compute usage.

    const uint16 cuEnableMask = device.GetCuEnableMask(0, device.Settings().csCuEnLimitMask);

    regCOMPUTE_STATIC_THREAD_MGMT_SE0 computeStaticThreadMgmtPerSe = { };
    computeStaticThreadMgmtPerSe.bits.SH0_CU_EN = cuEnableMask;
    computeStaticThreadMgmtPerSe.bits.SH1_CU_EN = cuEnableMask;

    const uint32 masksPerSe[4] =
    {
        computeStaticThreadMgmtPerSe.u32All,
        ((chipProps.gfx6.numShaderEngines >= 2) ? computeStaticThreadMgmtPerSe.u32All : 0),
        ((chipProps.gfx6.numShaderEngines >= 3) ? computeStaticThreadMgmtPerSe.u32All : 0),
        ((chipProps.gfx6.numShaderEngines >= 4) ? computeStaticThreadMgmtPerSe.u32All : 0),
    };

    pCmdSpace = pCmdStream->WriteSetSeqShRegsIndex(mmCOMPUTE_STATIC_THREAD_MGMT_SE0,
                                                   mmCOMPUTE_STATIC_THREAD_MGMT_SE1,
                                                   ShaderCompute,
                                                   &masksPerSe[0],
                                                   SET_SH_REG_INDEX_CP_MODIFY_CU_MASK,
                                                   pCmdSpace);

    if (chipProps.gfxLevel != GfxIpLevel::GfxIp6)
    {
        pCmdSpace = pCmdStream->WriteSetSeqShRegsIndex(mmCOMPUTE_STATIC_THREAD_MGMT_SE2__CI__VI,
                                                       mmCOMPUTE_STATIC_THREAD_MGMT_SE3__CI__VI,
                                                       ShaderCompute,
                                                       &masksPerSe[2],
                                                       SET_SH_REG_INDEX_CP_MODIFY_CU_MASK,
                                                       pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
ComputeQueueContext::ComputeQueueContext(
    Device* pDevice,
    Engine* pEngine,
    uint32  queueId,
    bool    isTmz)
    :
    QueueContext(pDevice->Parent()),
    m_pDevice(pDevice),
    m_pEngine(static_cast<ComputeEngine*>(pEngine)),
    m_queueId(queueId),
    m_queueUseTmzRing(isTmz),
    m_currentUpdateCounter(0),
    m_currentUpdateCounterTmz(0),
    m_cmdStream(*pDevice,
                pDevice->Parent()->InternalUntrackedCmdAllocator(),
                EngineTypeCompute,
                SubEngineType::Primary,
                CmdStreamUsage::Preamble,
                false),
    m_perSubmitCmdStream(*pDevice,
                         pDevice->Parent()->InternalUntrackedCmdAllocator(),
                         EngineTypeCompute,
                         SubEngineType::Primary,
                         CmdStreamUsage::Preamble,
                         false),
    m_postambleCmdStream(*pDevice,
                         pDevice->Parent()->InternalUntrackedCmdAllocator(),
                         EngineTypeCompute,
                         SubEngineType::Primary,
                         CmdStreamUsage::Postamble,
                         false)
{
}

// =====================================================================================================================
// Initializes this QueueContext by creating its internal command stream and rebuilding the command stream's contents.
Result ComputeQueueContext::Init()
{
    Result result = m_cmdStream.Init();

    if (result == Result::Success)
    {
        result = m_perSubmitCmdStream.Init();
    }

    if (result == Result::Success)
    {
        result = m_postambleCmdStream.Init();
    }

    if (result == Result::Success)
    {
        // If we can't use a CS_PARTIAL_FLUSH on ACE we need to allocate an extra timestamp for a full wait-for-idle.
        result = CreateTimestampMem(m_pDevice->CmdUtil().CanUseCsPartialFlush(EngineTypeCompute) == false);
    }

    if (result == Result::Success)
    {
        result = RebuildCommandStream(m_queueUseTmzRing);
    }

    return result;
}

// =====================================================================================================================
// Checks if the queue context preamble needs to be rebuilt, possibly due to the client creating new pipelines that
// require a bigger scratch ring, or due the client binding a new trap handler/buffer.  If so, the the compute shader
// rings are re-validated and our context command stream is rebuilt.
Result ComputeQueueContext::PreProcessSubmit(
    InternalSubmitInfo* pSubmitInfo,
    uint32              cmdBufferCount)
{
    bool   hasUpdated = false;
    const bool isTmz = (pSubmitInfo->flags.isTmzEnabled != 0);

    Result result     = m_pEngine->UpdateRingSet(isTmz, &m_currentUpdateCounter, &hasUpdated);

    if ((result == Result::Success) && hasUpdated)
    {
        // isTmz won't change, because TMZ submission can only be submitted to a tmz only queue.
        PAL_ASSERT(m_queueUseTmzRing == isTmz);
        result = RebuildCommandStream(isTmz);
    }
    m_queueUseTmzRing = isTmz;

    if (result == Result::Success)
    {
        pSubmitInfo->pPreambleCmdStream[0]  = &m_perSubmitCmdStream;
        pSubmitInfo->pPreambleCmdStream[1]  = &m_cmdStream;
        pSubmitInfo->pPostambleCmdStream[0] = &m_postambleCmdStream;

        pSubmitInfo->numPreambleCmdStreams  = 2;
        pSubmitInfo->numPostambleCmdStreams = 1;

        pSubmitInfo->pagingFence = m_pDevice->Parent()->InternalUntrackedCmdAllocator()->LastPagingFence();
    }

    return result;
}

// =====================================================================================================================
// Marks the context command stream as droppable, so the KMD can optimize away its execution in cases where there is no
// application context switch between back-to-back submissions.
void ComputeQueueContext::PostProcessSubmit()
{
    if (m_pDevice->CoreSettings().forcePreambleCmdStream == false)
    {
        // The next time this Queue is submitted-to, the KMD can safely skip the execution of the command stream since
        // the GPU already has received the latest updates.
        m_cmdStream.EnableDropIfSameContext(true);
    }
}

// =====================================================================================================================
// Regenerates the contents of this context's internal command stream.
Result ComputeQueueContext::RebuildCommandStream(
    bool isTmz)
{
    /*
     * There are two preambles which PAL submits with every set of command buffers: one which executes as a preamble
     * to each submission, and another which only executes when the previous submission on the GPU belonged to this
     * Queue. There is also a postamble which executes after every submission.
     *
     * The queue preamble sets up shader rings, GDS, and some global register state.
     *
     * The per-submit preamble and postamble implements a two step acquire-release on queue execution. They flush
     * and invalidate all GPU caches and prevent command buffers from different submits from overlapping. This is
     * required for some PAL clients and some PAL features.
     *
     * It is implemented using a 32-bit timestamp in local memory that is initialized to zero. The preamble waits for
     * the timestamp to be equal to zero before allowing execution to continue. It then sets the timestamp to some
     * other value (e.g., one) to indicate that the queue is busy and invalidates all read caches. The postamble issues
     * an end-of-pipe event that flushes all write caches and clears the timestamp back to zero.
     */

    const CmdUtil& cmdUtil = m_pDevice->CmdUtil();

    // The drop-if-same-context queue preamble.
    //==================================================================================================================

    m_cmdStream.Reset(nullptr, true);
    Result result = m_cmdStream.Begin({}, nullptr);

    if (result == Result::Success)
    {
        uint32* pCmdSpace = m_cmdStream.ReserveCommands();

        // Write the shader ring-set's commands before the command stream's normal preamble. If the ring sizes have
        // changed, the hardware requires a CS idle to operate properly.
        if (isTmz)
        {
            pCmdSpace = m_pEngine->TmzRingSet()->WriteCommands(&m_cmdStream, pCmdSpace);
        }
        else
        {
            pCmdSpace = m_pEngine->RingSet()->WriteCommands(&m_cmdStream, pCmdSpace);
        }

        const gpusize waitTsGpuVa = (m_waitForIdleTs.IsBound() ? m_waitForIdleTs.GpuVirtAddr() : 0);
        pCmdSpace += cmdUtil.BuildWaitCsIdle(EngineTypeCompute, waitTsGpuVa, pCmdSpace);

        pCmdSpace  = WriteCommonPreamble(*m_pDevice, EngineTypeCompute, &m_cmdStream, pCmdSpace);
        pCmdSpace  = WriteTrapInstallCmds(m_pDevice, &m_cmdStream, PipelineBindPoint::Compute, pCmdSpace);

        m_cmdStream.CommitCommands(pCmdSpace);
        result = m_cmdStream.End();
    }

    // The per-submit preamble.
    //==================================================================================================================

    if (result == Result::Success)
    {
        m_perSubmitCmdStream.Reset(nullptr, true);
        result = m_perSubmitCmdStream.Begin({}, nullptr);
    }

    if (result == Result::Success)
    {
        uint32* pCmdSpace = m_perSubmitCmdStream.ReserveCommands();

        // The following wait and surface sync must be at the beginning of the per-submit preamble.
        //
        // Wait for a prior submission on this context to be idle before executing the command buffer streams.
        // The timestamp memory is initialized to zero so the first submission on this context will not wait.
        pCmdSpace += cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_MEMORY,
                                             WAIT_REG_MEM_FUNC_EQUAL,
                                             WAIT_REG_MEM_ENGINE_PFP,
                                             m_exclusiveExecTs.GpuVirtAddr(),
                                             0,
                                             0xFFFFFFFF,
                                             false,
                                             pCmdSpace);

        // Issue a surface_sync or acquire mem packet to invalidate all L1 caches (TCP, SQ I-cache, SQ K-cache).
        //
        // Our postamble stream flushes and invalidates the L2 with an EOP event at the conclusion of each user mode
        // submission, but the L1 shader caches (SQC/TCP) are not invalidated. We waited for that event just above this
        // packet so the L2 cannot contain stale data. However, a well behaving app could read stale L1 data unless we
        // invalidate those caches here.
        regCP_COHER_CNTL invalidateL1Cache = {};
        invalidateL1Cache.bits.SH_ICACHE_ACTION_ENA = 1;
        invalidateL1Cache.bits.SH_KCACHE_ACTION_ENA = 1;
        invalidateL1Cache.bits.TCL1_ACTION_ENA      = 1;

        pCmdSpace += cmdUtil.BuildGenericSync(invalidateL1Cache,
                                              SURFACE_SYNC_ENGINE_ME,
                                              FullSyncBaseAddr,
                                              FullSyncSize,
                                              true,
                                              pCmdSpace);

        m_perSubmitCmdStream.CommitCommands(pCmdSpace);
        result = m_perSubmitCmdStream.End();
    }

    // The per-submit postamble.
    //==================================================================================================================

    if (result == Result::Success)
    {
        m_postambleCmdStream.Reset(nullptr, true);
        result = m_postambleCmdStream.Begin({}, nullptr);
    }

    if (result == Result::Success)
    {
        uint32* pCmdSpace = m_postambleCmdStream.ReserveCommands();

        // This write data and EOP event packet must be at the end of the per-submit postamble.
        //
        // Rewrite the timestamp to some other value so that the next submission will wait until this one is done.
        // Note that we must do this write in the postamble rather than the preamble. Some CP features can preempt our
        // submission frame without executing the postamble which would cause the wait in the preamble to hang if we
        // did this write in the preamble.
        WriteDataInfo writeData = {};
        writeData.dstAddr = m_exclusiveExecTs.GpuVirtAddr();
        writeData.dstSel  = WRITE_DATA_DST_SEL_MEMORY_ASYNC;

        pCmdSpace += cmdUtil.BuildWriteData(writeData, 1, pCmdSpace);

        // When the pipeline has emptied, write the timestamp back to zero so that the next submission can execute.
        // We also use this pipelined event to flush and invalidate the shader L2 cache as described above.
        pCmdSpace += cmdUtil.BuildGenericEopEvent(BOTTOM_OF_PIPE_TS,
                                                  m_exclusiveExecTs.GpuVirtAddr(),
                                                  EVENTWRITEEOP_DATA_SEL_SEND_DATA32,
                                                  0,
                                                  true,
                                                  true,
                                                  pCmdSpace);

        // The gfx7 MEC microcode assumes that all RELEASE_MEMs in indirect buffers have the same VMID.  If this
        // assumption is broken, timestamps from prior IBs will be written using the VMID of the current IB which will
        // cause a page fault.  PAL has no way to know if KMD is going to schedule work with different VMIDs on the same
        // compute ring so we must assume the CP's assumption will be broken. In that case, we must guarantee that all of
        // our timestamps are written before we end this postamble so that they use the proper VMID. We can do this by
        // simply waiting on the EOP timestamp we just issued.
        if (m_pDevice->Parent()->ChipProperties().gfxLevel == GfxIpLevel::GfxIp7)
        {
            pCmdSpace += cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_MEMORY,
                                                 WAIT_REG_MEM_FUNC_EQUAL,
                                                 WAIT_REG_MEM_ENGINE_PFP,
                                                 m_exclusiveExecTs.GpuVirtAddr(),
                                                 0,
                                                 0xFFFFFFFF,
                                                 false,
                                                 pCmdSpace);
        }

        m_postambleCmdStream.CommitCommands(pCmdSpace);
        result = m_postambleCmdStream.End();
    }

    // If this assert is hit, CmdBufInternalSuballocSize should be increased.
    PAL_ASSERT((m_cmdStream.GetNumChunks() == 1)          &&
               (m_perSubmitCmdStream.GetNumChunks() == 1) &&
               (m_postambleCmdStream.GetNumChunks() == 1));

    // Since the contents of the command stream have changed since last time, we need to force this stream to execute
    // by not allowing the KMD to optimize-away this command stream the next time around.
    m_cmdStream.EnableDropIfSameContext(false);

    // The per-submit command stream and postamble command stream must always execute. We cannot allow KMD to
    // optimize-away this command stream.
    m_perSubmitCmdStream.EnableDropIfSameContext(false);
    m_postambleCmdStream.EnableDropIfSameContext(false);

    return result;
}

// =====================================================================================================================
UniversalQueueContext::UniversalQueueContext(
    Device* pDevice,
    bool    isPreemptionSupported,
    uint32  persistentCeRamOffset,
    uint32  persistentCeRamSize,
    Engine* pEngine,
    uint32  queueId)
    :
    QueueContext(pDevice->Parent()),
    m_pDevice(pDevice),
    m_pEngine(static_cast<UniversalEngine*>(pEngine)),
    m_queueId(queueId),
    m_persistentCeRamOffset(persistentCeRamOffset),
    m_persistentCeRamSize(persistentCeRamSize),
    m_currentUpdateCounter(0),
    m_currentUpdateCounterTmz(0),
    m_cmdsUseTmzRing(false),
    m_useShadowing((Device::ForceStateShadowing &&
                    pDevice->Parent()->ChipProperties().gfx6.supportLoadRegIndexPkt) ||
                    isPreemptionSupported),
    m_shadowGpuMemSizeInBytes(0),
    m_shadowedRegCount(0),
    m_deCmdStream(*pDevice,
                  pDevice->Parent()->InternalUntrackedCmdAllocator(),
                  EngineTypeUniversal,
                  SubEngineType::Primary,
                  CmdStreamUsage::Preamble,
                  false),
    m_perSubmitCmdStream(*pDevice,
                         pDevice->Parent()->InternalUntrackedCmdAllocator(),
                         EngineTypeUniversal,
                         SubEngineType::Primary,
                         CmdStreamUsage::Preamble,
                         false),
    m_shadowInitCmdStream(*pDevice,
                          pDevice->Parent()->InternalUntrackedCmdAllocator(),
                          EngineTypeUniversal,
                          SubEngineType::Primary,
                          CmdStreamUsage::Preamble,
                          false),
    m_cePreambleCmdStream(*pDevice,
                          pDevice->Parent()->InternalUntrackedCmdAllocator(),
                          EngineTypeUniversal,
                          SubEngineType::ConstantEngine,
                          CmdStreamUsage::Preamble,
                          false),
    m_cePostambleCmdStream(*pDevice,
                           pDevice->Parent()->InternalUntrackedCmdAllocator(),
                           EngineTypeUniversal,
                           SubEngineType::ConstantEngine,
                           CmdStreamUsage::Postamble,
                           false),
    m_dePostambleCmdStream(*pDevice,
                           pDevice->Parent()->InternalUntrackedCmdAllocator(),
                           EngineTypeUniversal,
                           SubEngineType::Primary,
                           CmdStreamUsage::Postamble,
                           false)
{
}

// =====================================================================================================================
UniversalQueueContext::~UniversalQueueContext()
{
    if (m_shadowGpuMem.IsBound())
    {
        m_pDevice->Parent()->MemMgr()->FreeGpuMem(m_shadowGpuMem.Memory(), m_shadowGpuMem.Offset());
        m_shadowGpuMem.Update(nullptr, 0);
    }
}

// =====================================================================================================================
// Initializes this QueueContext by creating its internal command streams and rebuilding the command streams' contents.
Result UniversalQueueContext::Init()
{
    Result result = m_deCmdStream.Init();

    if (result == Result::Success)
    {
        result = m_perSubmitCmdStream.Init();
    }

    if ((result == Result::Success) && m_useShadowing)
    {
        result = m_shadowInitCmdStream.Init();
    }

    if (result == Result::Success)
    {
        m_cePreambleCmdStream.Init();
    }

    if (result == Result::Success)
    {
        m_cePostambleCmdStream.Init();
    }

    if (result == Result::Success)
    {
        m_dePostambleCmdStream.Init();
    }

    if (result == Result::Success)
    {
        // The universal engine can always use CS_PARTIAL_FLUSH events so we don't need the wait-for-idle TS memory.
        result = CreateTimestampMem(false);
    }

    if (result == Result::Success)
    {
        result = AllocateShadowMemory();
    }

    if (result == Result::Success)
    {
        result = BuildShadowPreamble();
    }

    if (result == Result::Success)
    {
        result = RebuildCommandStreams(m_cmdsUseTmzRing);
    }

    return result;
}

// =====================================================================================================================
// Allocates a chunk of GPU memory used for shadowing the contents of any client-requested Persistent CE RAM beetween
// submissions to this object's parent Queue.
Result UniversalQueueContext::AllocateShadowMemory()
{
    Pal::Device*const        pDevice   = m_pDevice->Parent();
    const GpuChipProperties& chipProps = pDevice->ChipProperties();

    // Shadow memory only needs to include space for the region of CE RAM which the client requested PAL makes
    // persistent between submissions.
    uint32 ceRamBytes = (m_persistentCeRamSize * sizeof(uint32));

    if (m_useShadowing)
    {
        // If mid command buffer preemption is enabled, we must also include shadow space for all of the context,
        // SH, and user-config registers. This is because the CP will restore the whole state when resuming this
        // Queue from being preempted.
        m_shadowedRegCount = (ShRegCount + CntxRegCountGfx7 + UserConfigRegCount);

        // Also, if mid command buffer preemption is enabled, we must restore all CE RAM used by the client and
        // internally by PAL. All of that data will need to be restored aftere resuming this Queue from being
        // preempted.
        ceRamBytes = static_cast<uint32>(pDevice->CeRamBytesUsed(EngineTypeUniversal));
    }

    constexpr gpusize ShadowMemoryAlignment = 256;

    GpuMemoryCreateInfo createInfo = { };
    createInfo.alignment = ShadowMemoryAlignment;
    createInfo.size      = (ceRamBytes + (sizeof(uint32) * m_shadowedRegCount));
    createInfo.priority  = GpuMemPriority::Normal;
    createInfo.vaRange   = VaRange::Default;

    m_shadowGpuMemSizeInBytes = createInfo.size;

    if (chipProps.gpuType == GpuType::Integrated)
    {
        createInfo.heapCount = 2;
        createInfo.heaps[0]  = GpuHeap::GpuHeapGartUswc;
        createInfo.heaps[1]  = GpuHeap::GpuHeapGartCacheable;
    }
    else
    {
        createInfo.heapCount = 2;
        createInfo.heaps[0]  = GpuHeap::GpuHeapInvisible;
        createInfo.heaps[1]  = GpuHeap::GpuHeapLocal;
    }

    GpuMemoryInternalCreateInfo internalInfo = { };
    internalInfo.flags.alwaysResident = 1;

    Result result = Result::Success;
    if (createInfo.size != 0)
    {
        GpuMemory* pGpuMemory = nullptr;
        gpusize    offset     = 0uLL;

        result = pDevice->MemMgr()->AllocateGpuMem(createInfo, internalInfo, false, &pGpuMemory, &offset);
        if (result == Result::Success)
        {
            m_shadowGpuMem.Update(pGpuMemory, offset);
        }
    }

    return result;
}

// =====================================================================================================================
// Constructs the shadow memory initialization preamble command stream.
Result UniversalQueueContext::BuildShadowPreamble()
{
    Result result = Result::Success;

    // This should only be called when state shadowing is being used.
    if (m_useShadowing)
    {
        m_shadowInitCmdStream.Reset(nullptr, true);
        result = m_shadowInitCmdStream.Begin({}, nullptr);

        if (result == Result::Success)
        {
            // Generate a version of the per submit preamble that initializes shadow memory.
            WritePerSubmitPreamble(&m_shadowInitCmdStream, true);

            result = m_shadowInitCmdStream.End();
        }
    }

    return result;
}

// =====================================================================================================================
// Builds a per-submit command stream for the DE. Conditionally adds shadow memory initialization commands.
void UniversalQueueContext::WritePerSubmitPreamble(
    CmdStream* pCmdStream,
    bool       initShadowMemory)
{
    // Shadow memory should only be initialized when state shadowing is being used.
    PAL_ASSERT(m_useShadowing || (initShadowMemory == false));

    const CmdUtil& cmdUtil = m_pDevice->CmdUtil();
    uint32* pCmdSpace      = pCmdStream->ReserveCommands();

    // The following wait and surface sync must be at the beginning of the per-submit DE preamble.
    //
    // Wait for a prior submission on this context to be idle before executing the command buffer streams.
    // The timestamp memory is initialized to zero so the first submission on this context will not wait.
    pCmdSpace += cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_MEMORY,
                                         WAIT_REG_MEM_FUNC_EQUAL,
                                         WAIT_REG_MEM_ENGINE_PFP,
                                         m_exclusiveExecTs.GpuVirtAddr(),
                                         0,
                                         UINT_MAX,
                                         false,
                                         pCmdSpace);

    // Issue a surface_sync or acquire mem packet to invalidate all L1 caches (TCP, SQ I-cache, SQ K-cache).
    //
    // Our postamble stream flushes and invalidates the L2 and RB caches with an EOP event at the conclusion of each
    // user mode submission, but the L1 shader caches (SQC/TCP) are not invalidated. We waited for that event just
    // above this packet so the L2 cannot contain stale data. However, a well behaving app could read stale L1 data
    // unless we invalidate those caches here.
    regCP_COHER_CNTL cpCoherCntl = {};
    cpCoherCntl.bits.SH_ICACHE_ACTION_ENA = 1;
    cpCoherCntl.bits.SH_KCACHE_ACTION_ENA = 1;
    cpCoherCntl.bits.TCL1_ACTION_ENA      = 1;

    if (m_pDevice->WaDbTcCompatFlush() != Gfx8TcCompatDbFlushWaNever)
    {
        // There is a clear-state packet in the state shadow preamble which is next in the command stream. That packet
        // writes the DB_HTILE_SURFACE register, which can trigger the "tcCompatFlush" HW bug -- i.e., if that register
        // (actually, the TC_COMPAT bit in that register) changes between draws without a flush, then very bad things
        // happen. Assume the state is changing and flush out the DB.
        cpCoherCntl.bits.DB_ACTION_ENA = 1;
    }

    pCmdSpace += cmdUtil.BuildSurfaceSync(cpCoherCntl,
                                          SURFACE_SYNC_ENGINE_ME,
                                          FullSyncBaseAddr,
                                          FullSyncSize,
                                          pCmdSpace);

    if (m_useShadowing)
    {
        // Those registers (which are used to setup UniversalRingSet) are shadowed and will be set by LOAD_*_REG.
        // We have to setup packets which issue VS_PARTIAL_FLUSH and VGT_FLUSH events before those LOAD_*_REGs
        // to make sure it is safe to write the ring config.
        pCmdSpace += cmdUtil.BuildEventWrite(VS_PARTIAL_FLUSH, pCmdSpace);
        pCmdSpace += cmdUtil.BuildEventWrite(VGT_FLUSH,        pCmdSpace);
    }

    // Write commands to issue context_control and other state-shadowing related stuff.
    pCmdSpace = WriteStateShadowingCommands(pCmdStream, pCmdSpace);

    pCmdStream->CommitCommands(pCmdSpace);

    if (initShadowMemory)
    {
        pCmdSpace = pCmdStream->ReserveCommands();

        // Use a DMA_DATA packet to initialize all shadow memory to 0s explicitely.
        DmaDataInfo dmaData  = {};
        dmaData.dstAddr      = m_shadowGpuMem.GpuVirtAddr();
        dmaData.dstAddrSpace = CPDMA_ADDR_SPACE_MEM;
        dmaData.dstSel       = CPDMA_DST_SEL_DST_ADDR;
        dmaData.srcSel       = CPDMA_SRC_SEL_DATA;
        dmaData.srcData      = 0;
        dmaData.numBytes     = static_cast<uint32>(m_shadowGpuMemSizeInBytes);
        dmaData.sync         = true;
        dmaData.usePfp       = true;
        pCmdSpace += cmdUtil.BuildDmaData(dmaData, pCmdSpace);

        // After initializing shadow memory to 0, load user config and sh register again, otherwise the registers
        // might contain invalid value. We don't need to load context register again because in the
        // InitializeContextRegisters() we will set the contexts that we can load.
        gpusize gpuVirtAddr = m_shadowGpuMem.GpuVirtAddr();

        pCmdSpace += cmdUtil.BuildLoadUserConfigRegs(gpuVirtAddr,
                                                     &UserConfigShadowRangeGfx7[0],
                                                     NumUserConfigShadowRangesGfx7,
                                                     pCmdSpace);
        gpuVirtAddr += (sizeof(uint32) * UserConfigRegCount);

        gpuVirtAddr += (sizeof(uint32) * CntxRegCountGfx7);

        pCmdSpace += cmdUtil.BuildLoadShRegs(gpuVirtAddr,
                                             &GfxShShadowRange[0],
                                             NumGfxShShadowRanges,
                                             ShaderGraphics,
                                             pCmdSpace);

        pCmdSpace += cmdUtil.BuildLoadShRegs(gpuVirtAddr,
                                             &CsShShadowRange[0],
                                             NumCsShShadowRanges,
                                             ShaderCompute,
                                             pCmdSpace);
        gpuVirtAddr += (sizeof(uint32) * ShRegCount);

        pCmdStream->CommitCommands(pCmdSpace);

        // We do this after m_stateShadowPreamble, when the LOADs are done and HW knows the shadow memory.
        // First LOADs will load garbage. InitializeContextRegisters will init the register and also the shadow Memory.
        const auto& chipProps = m_pDevice->Parent()->ChipProperties();
        if (chipProps.gfxLevel >= GfxIpLevel::GfxIp8)
        {
            InitializeContextRegistersGfx8(pCmdStream, 0, nullptr, nullptr);
        }
        else
        {
            // Only Gfx8+ supports preemption.
            PAL_NOT_IMPLEMENTED();
        }
    }

    // When shadowing is enabled, these registers don't get lost so we only need to do this when shadowing is off.
    if (m_pDevice->WaForceToWriteNonRlcRestoredRegs() && (m_useShadowing == false))
    {
        // Some hardware doesn't restore non-RLC registers following a power-management event. The workaround is to
        // restore those registers *every* submission, rather than just the ones following a ring resize event or
        // after a context switch between applications
        pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace = m_pEngine->RingSet()->WriteNonRlcRestoredRegs(pCmdStream, pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Checks if the queue context preamble needs to be rebuilt, possibly due to the client creating new pipelines that
// require bigger rings, or due the client binding a new trap handler/buffer.  If so, the the shader rings are
// re-validated and our context command stream is rebuilt.
// When MCBP is enabled, we'll force the command stream to be rebuilt when we submit the command for the first time,
// because we need to build set commands to initialize the context register and shadow memory. The sets only need to be
// done once, so we need to rebuild the command stream on the second submit.
Result UniversalQueueContext::PreProcessSubmit(
    InternalSubmitInfo* pSubmitInfo,
    uint32              cmdBufferCount)
{
    bool   hasUpdated = false;
    Result result     = Result::Success;

    // We only need to rebuild the command stream if the user submits at least one command buffer.
    if (cmdBufferCount != 0)
    {
        const bool isTmz = (pSubmitInfo->flags.isTmzEnabled != 0);
        result = m_pEngine->UpdateRingSet(isTmz, &m_currentUpdateCounter, &hasUpdated);

        if ((result == Result::Success) && (hasUpdated == false) && (m_cmdsUseTmzRing != isTmz))
        {
            result = m_pEngine->WaitIdleAllQueues();
            hasUpdated = true;
        }

        if ((result == Result::Success) && hasUpdated)
        {
            result = RebuildCommandStreams(isTmz);
        }
        m_cmdsUseTmzRing = isTmz;
    }

    if (result == Result::Success)
    {
        uint32 preambleCount  = 0;
        if (m_cePreambleCmdStream.IsEmpty() == false)
        {
            pSubmitInfo->pPreambleCmdStream[preambleCount] = &m_cePreambleCmdStream;
            ++preambleCount;
        }

        pSubmitInfo->pPreambleCmdStream[preambleCount] = &m_perSubmitCmdStream;
        ++preambleCount;

        if (m_pDevice->CoreSettings().commandBufferCombineDePreambles == false)
        {
            // Submit the per-context preamble independently.
            pSubmitInfo->pPreambleCmdStream[preambleCount] = &m_deCmdStream;
            ++preambleCount;
        }

        uint32 postambleCount = 0;
        if (m_cePostambleCmdStream.IsEmpty() == false)
        {
            pSubmitInfo->pPostambleCmdStream[postambleCount] = &m_cePostambleCmdStream;
            ++postambleCount;
        }

        pSubmitInfo->pPostambleCmdStream[postambleCount] = &m_dePostambleCmdStream;
        ++postambleCount;

        pSubmitInfo->numPreambleCmdStreams  = preambleCount;
        pSubmitInfo->numPostambleCmdStreams = postambleCount;

        pSubmitInfo->pagingFence = m_pDevice->Parent()->InternalUntrackedCmdAllocator()->LastPagingFence();
    }

    return result;
}

// =====================================================================================================================
// Marks the context command stream as droppable, so the KMD can optimize away its execution in cases where there is no
// application context switch between back-to-back submissions.
void UniversalQueueContext::PostProcessSubmit()
{
    if (m_pDevice->CoreSettings().forcePreambleCmdStream == false)
    {
        // The next time this Queue is submitted-to, the KMD can safely skip the execution of the command stream since
        // the GPU already has received the latest updates.
        m_deCmdStream.EnableDropIfSameContext(true);
        // NOTE: The per-submit command stream cannot receive this optimization because it must be executed for every
        // submit.

        // On gfx6-7, the CE preamble must be skipped if the same context runs back-to-back and the client has enabled
        // persistent CE RAM. If we don't skip the CE preamble the CE load packet will race against the DE postamble's
        // EOP cache flush possibly causing CE to load stale data back into CE RAM. If we are ever prevented from
        // using DropIfSameContext in this situation we will have to add a CE/DE counter sync to the preambles.
        //
        // On gfx8 this is just an optimization.
        m_cePreambleCmdStream.EnableDropIfSameContext(true);
    }
}

// =====================================================================================================================
// Processes the initial submit for a queue. Returns Success if the processing was required and needs to be submitted.
// Returns Unsupported otherwise.
Result UniversalQueueContext::ProcessInitialSubmit(
    InternalSubmitInfo* pSubmitInfo)
{
    Result result = Result::Unsupported;

    // We only need to perform an initial submit if we're using state shadowing.
    if (m_useShadowing)
    {
        // Submit a special version of the per submit preamble that initializes shadow memory.
        pSubmitInfo->pPreambleCmdStream[0] = &m_shadowInitCmdStream;

        // The DE postamble is always required to satisfy the acquire/release model.
        pSubmitInfo->pPostambleCmdStream[0] = &m_dePostambleCmdStream;

        pSubmitInfo->numPreambleCmdStreams  = 1;
        pSubmitInfo->numPostambleCmdStreams = 1;

        pSubmitInfo->pagingFence = m_pDevice->Parent()->InternalUntrackedCmdAllocator()->LastPagingFence();

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Regenerates the contents of this context's internal command streams.
Result UniversalQueueContext::RebuildCommandStreams(
    bool isTmz)
{
    /*
     * There are two DE preambles which PAL submits with every set of command buffers: one which executes as a preamble
     * to each submission, and another which only executes when the previous submission on the GPU belonged to this
     * Queue.
     *
     * Unless mid command buffer preemption is enabled, PAL will not enable state shadowing. This is because each PAL
     * command buffer is defined to not inherit any state from whatever command buffer(s) ran before it, which means
     * that each command buffer contains all of the render state commands it requires in order to run. (If preemption
     * is enabled, we must enable state shadowing despite the stateless nature of PAL command buffers because the GPU
     * uses state shadowing to restore GPU state after resuming a previously-preempted command buffer.)
     *
     * The preamble which executes unconditionally is executed first, and its first packet is a CONTEXT_CONTROL which
     * will either disable or enable state shadowing as described above.
     *
     * When either mid command buffer preemption is enabled, or the client has enabled the "persistent CE RAM" feature,
     * PAL also submits a CE preamble which loads CE RAM from memory, and submits a CE & DE postamble with each set of
     * command buffers. These postambles ensure that CE RAM contents are saved to memory so that they can be restored
     * when a command buffer is resumed after preemption, or restored during the next submission if the client is using
     * "persistent CE RAM".
     *
     * The per-submit preamble and postamble also implement a two step acquire-release on queue execution. They flush
     * and invalidate all GPU caches and prevent command buffers from different submits from overlapping. This is
     * required for some PAL clients and some PAL features.
     *
     * It is implemented using a 32-bit timestamp in local memory that is initialized to zero. The preamble waits for
     * the timestamp to be equal to zero before allowing execution to continue. It then sets the timestamp to some
     * other value (e.g., one) to indicate that the queue is busy and invalidates all read caches. The postamble issues
     * an end-of-pipe event that flushes all write caches and clears the timestamp back to zero.
     */

    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    const CmdUtil& cmdUtil = m_pDevice->CmdUtil();

    // The drop-if-same-context DE preamble.
    //==================================================================================================================

    m_deCmdStream.Reset(nullptr, true);
    Result result = m_deCmdStream.Begin({}, nullptr);

    if (result == Result::Success)
    {
        uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

        pCmdSpace = WriteUniversalPreamble(pCmdSpace);

        const auto* pRingSet = m_pEngine->RingSet();
        const auto* ptmzRingSet = m_pEngine->TmzRingSet();

        // Write the shader ring-set's commands after the command stream's normal preamble. If the ring sizes have changed,
        // the hardware requires a CS/VS/PS partial flush to operate properly.
        if (isTmz)
        {
            pCmdSpace = ptmzRingSet->WriteCommands(&m_deCmdStream, pCmdSpace);
        }
        else
        {
            pCmdSpace = pRingSet->WriteCommands(&m_deCmdStream, pCmdSpace);
        }
        pCmdSpace += cmdUtil.BuildEventWrite(CS_PARTIAL_FLUSH, pCmdSpace);
        pCmdSpace += cmdUtil.BuildEventWrite(VS_PARTIAL_FLUSH, pCmdSpace);
        pCmdSpace += cmdUtil.BuildEventWrite(PS_PARTIAL_FLUSH, pCmdSpace);

        // @note: This condition is temporarily commented out to fix a regression that was specific to asics which required
        //        the "waForceToWriteNonRlcRestoredRegs" workaround. Commenting out the condition causes the code to always
        //        restore the non-RLC registers after every context switch, even on asics affected by the workaround. This
        //        is necessary because of the clear state packet that happens earlier in the universal preamble. The code
        //        before the regression used to submit the universal preamble first, then the per submit preamble, but the
        //        code that caused the regression reversed the order to fix another issue. With the new setup, when we
        //        switch contexts, we can end up loading the non-rlc registers in the per submit, then executing the
        //        universal preamble which writes a clear state packet that clears some of the loaded registers. This can
        //        be fixed by unconditionally loading the registers after the clear state in the universal preamble. We
        //        still need the load in the per submit preamble for asics affected by the workaround though. If we're
        //        using the same context, the universal preamble can be dropped and only the per submit preamble will run.
        //
        //        This temporary change will be removed by a later change related to mid command buffer preemption.
        //if (m_pDevice->WaForceToWriteNonRlcRestoredRegs() == false)
        {
            // If the workaround is disabled, we only need to restore the non-RLC registers whenever the ring sizes
            // are changed or after a context switch between applications.
            pCmdSpace = pRingSet->WriteNonRlcRestoredRegs(&m_deCmdStream, pCmdSpace);
        }

        pCmdSpace = WriteTrapInstallCmds(m_pDevice, &m_deCmdStream, PipelineBindPoint::Graphics, pCmdSpace);
        pCmdSpace = WriteTrapInstallCmds(m_pDevice, &m_deCmdStream, PipelineBindPoint::Compute, pCmdSpace);

        m_deCmdStream.CommitCommands(pCmdSpace);
        result = m_deCmdStream.End();
    }

    // The per-submit DE preamble.
    //==================================================================================================================

    if (result == Result::Success)
    {
        m_perSubmitCmdStream.Reset(nullptr, true);
        result = m_perSubmitCmdStream.Begin({}, nullptr);
    }

    if (result == Result::Success)
    {
        // Generate a version of the per submit preamble that does not initialize shadow memory.
        WritePerSubmitPreamble(&m_perSubmitCmdStream, false);

        result = m_perSubmitCmdStream.End();
    }

    if (m_pDevice->CoreSettings().commandBufferCombineDePreambles)
    {
        // Combine the preambles by chaining from the per-submit preamble to the per-context preamble.
        m_perSubmitCmdStream.PatchTailChain(&m_deCmdStream);
    }

    // The per-submit CE premable, CE postamble, and DE postamble.
    //==================================================================================================================

    if (result == Result::Success)
    {
        // The DE postamble is always built. The CE preamble and postamble may not be needed.
        m_dePostambleCmdStream.Reset(nullptr, true);
        result = m_dePostambleCmdStream.Begin({}, nullptr);
    }

    bool syncCeDeCounters = false;
    // If the client has requested that this Queue maintain persistent CE RAM contents, or if the Queue supports mid
    // command buffer preemption, we need to rebuild the CE preamble, as well as the CE & DE postambles.
    if ((m_persistentCeRamSize != 0) || m_useShadowing)
    {
        PAL_ASSERT(m_shadowGpuMem.IsBound());
        const gpusize gpuVirtAddr = (m_shadowGpuMem.GpuVirtAddr() + (sizeof(uint32) * m_shadowedRegCount));
        uint32 ceRamByteOffset    = m_persistentCeRamOffset;
        uint32 ceRamDwordSize     = m_persistentCeRamSize;

        if (m_useShadowing)
        {
            // If preemption is supported, we must save & restore all CE RAM used by either PAL or the client.
            ceRamByteOffset = 0;
            ceRamDwordSize  =
                static_cast<uint32>(m_pDevice->Parent()->CeRamDwordsUsed(EngineTypeUniversal));
        }

        if (result == Result::Success)
        {
            m_cePreambleCmdStream.Reset(nullptr, true);
            result = m_cePreambleCmdStream.Begin({}, nullptr);
        }

        if (result == Result::Success)
        {
            uint32* pCmdSpace = m_cePreambleCmdStream.ReserveCommands();
            pCmdSpace += cmdUtil.BuildLoadConstRam(gpuVirtAddr, ceRamByteOffset, ceRamDwordSize, pCmdSpace);
            m_cePreambleCmdStream.CommitCommands(pCmdSpace);

            result = m_cePreambleCmdStream.End();
        }

        // The postamble command streams which dump CE RAM at the end of the submission are only necessary if (1) the
        // client requested that this Queue maintains persistent CE RAM contents, or (2) this Queue supports mid
        // command buffer preemption and the panel setting to force the dump CE RAM postamble is set.
        if ((m_persistentCeRamSize != 0) ||
            (m_pDevice->CoreSettings().commandBufferForceCeRamDumpInPostamble != false))
        {
            // On gfx6-7 we need to synchronize the CE/DE counters after the dump CE RAM because the dump writes to L2
            // and the load reads from memory. The DE postamble's EOP event will flush L2 but we still need to use the
            // CE/DE counters to stall the DE until the dump is complete.
            syncCeDeCounters = (chipProps.gfxLevel <= GfxIpLevel::GfxIp7);

            // Note that it's illegal to touch the CE/DE counters in postamble streams if MCBP is enabled. In practice
            // we don't expect these two conditions to be enabled at the same time.
            PAL_ASSERT((syncCeDeCounters == false) || (m_useShadowing == false));

            if (result == Result::Success)
            {
                m_cePostambleCmdStream.Reset(nullptr, true);
                result = m_cePostambleCmdStream.Begin({}, nullptr);
            }

            if (result == Result::Success)
            {
                uint32* pCmdSpace = m_cePostambleCmdStream.ReserveCommands();
                pCmdSpace += cmdUtil.BuildDumpConstRam(gpuVirtAddr, ceRamByteOffset, ceRamDwordSize, pCmdSpace);

                if (syncCeDeCounters)
                {
                    pCmdSpace += cmdUtil.BuildIncrementCeCounter(pCmdSpace);
                }

                m_cePostambleCmdStream.CommitCommands(pCmdSpace);
                result = m_cePostambleCmdStream.End();
            }
        }
    }

    if (result == Result::Success)
    {
        uint32* pCmdSpace = m_dePostambleCmdStream.ReserveCommands();

        if (syncCeDeCounters)
        {
            pCmdSpace += cmdUtil.BuildWaitOnCeCounter(false, pCmdSpace);
            pCmdSpace += cmdUtil.BuildIncrementDeCounter(pCmdSpace);
        }

        // This write data and EOP event packet must be at the end of the per-submit DE postamble.
        //
        // Rewrite the timestamp to some other value so that the next submission will wait until this one is done.
        // Note that we must do this write in the postamble rather than the preamble. Some CP features can preempt our
        // submission frame without executing the postamble which would cause the wait in the preamble to hang if we
        // did this write in the preamble.
        WriteDataInfo writeData = {};
        writeData.dstAddr   = m_exclusiveExecTs.GpuVirtAddr();
        writeData.engineSel = WRITE_DATA_ENGINE_PFP;
        writeData.dstSel    = WRITE_DATA_DST_SEL_MEMORY_ASYNC;

        pCmdSpace += cmdUtil.BuildWriteData(writeData, 1, pCmdSpace);

        // When the pipeline has emptied, write the timestamp back to zero so that the next submission can execute.
        // We also use this pipelined event to flush and invalidate the shader L2 cache and RB caches as described
        // above.
        pCmdSpace += cmdUtil.BuildEventWriteEop(CACHE_FLUSH_AND_INV_TS_EVENT,
                                                m_exclusiveExecTs.GpuVirtAddr(),
                                                EVENTWRITEEOP_DATA_SEL_SEND_DATA32,
                                                0,
                                                true,
                                                pCmdSpace);

        m_dePostambleCmdStream.CommitCommands(pCmdSpace);
        result = m_dePostambleCmdStream.End();
    }

    // Since the contents of these command streams have changed since last time, we need to force these streams to
    // execute by not allowing the KMD to optimize-away these command stream the next time around.
    m_deCmdStream.EnableDropIfSameContext(false);
    m_cePreambleCmdStream.EnableDropIfSameContext(false);

    // The per-submit command stream and CE/DE postambles must always execute. We cannot allow KMD to optimize-away
    // these command streams.
    m_perSubmitCmdStream.EnableDropIfSameContext(false);
    m_cePostambleCmdStream.EnableDropIfSameContext(false);
    m_dePostambleCmdStream.EnableDropIfSameContext(false);

    // If this assert is hit, CmdBufInternalSuballocSize should be increased.
    PAL_ASSERT((m_perSubmitCmdStream.GetNumChunks()   == 1) &&
               (m_deCmdStream.GetNumChunks()          == 1) &&
               (m_cePreambleCmdStream.GetNumChunks()  <= 1) &&
               (m_cePostambleCmdStream.GetNumChunks() <= 1) &&
               (m_dePostambleCmdStream.GetNumChunks() <= 1));

    return result;
}

// =====================================================================================================================
// Writes commands needed for the "Drop if same context" DE preamble.
uint32* UniversalQueueContext::WriteUniversalPreamble(
    uint32* pCmdSpace)
{
    const Pal::Device&       device    = *(m_pDevice->Parent());
    const GpuChipProperties& chipProps = device.ChipProperties();
    const Gfx6PalSettings&   settings  = m_pDevice->Settings();

    struct
    {
        regPA_SC_GENERIC_SCISSOR_TL  tl;
        regPA_SC_GENERIC_SCISSOR_BR  br;
    } paScGenericScissor = { };

    paScGenericScissor.tl.bits.WINDOW_OFFSET_DISABLE = 1;
    paScGenericScissor.br.bits.BR_X = ScissorMaxBR;
    paScGenericScissor.br.bits.BR_Y = ScissorMaxBR;

    // Several context registers are considered "sticky" by the hardware team, which means that they broadcast their
    // value to all eight render contexts. Clear state cannot reset them properly if another driver changes them,
    // because that driver's writes will have clobbered the values in our clear state reserved GPU context. We need
    // to restore default values here to be on the safe side.
    struct
    {
        regVGT_MAX_VTX_INDX  maxVtxIndx;
        regVGT_MIN_VTX_INDX  minVtxIndx;
        regVGT_INDX_OFFSET   indxOffset;
    } vgt = { };
    vgt.maxVtxIndx.bits.MAX_INDX = UINT_MAX;

    pCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmPA_SC_GENERIC_SCISSOR_TL,
                                                     mmPA_SC_GENERIC_SCISSOR_BR,
                                                     &paScGenericScissor,
                                                     pCmdSpace);
    pCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmVGT_MAX_VTX_INDX,
                                                     mmVGT_INDX_OFFSET,
                                                     &vgt,
                                                     pCmdSpace);

    if (chipProps.gfxLevel == GfxIpLevel::GfxIp6)
    {
        // On Gfx6 hardware, there is a possible deadlock scenario between the LS/HS and PS waves: Because they both
        // use LDS resources, if all CU's are backed-up running LS/HS waves then the PS can be starved from running
        // because all of LDS is used by the LS/HS waves. This causes a deadlock because PS is required to run to drain
        // the pipeline of work generated by LS/HS wavefronts. The solution to this problem is to prevent the hardware
        // from scheduling LS/HS wavefronts on one CU per shader engine and shader array.

        // Need to find a bit-mask which has the active and always-on CU masks for all shader engines and shader arrays
        // combined.

        uint32 activeCuMask   = USHRT_MAX;
        uint32 alwaysOnCuMask = USHRT_MAX;

        for (size_t se = 0; se < chipProps.gfx6.numShaderEngines; ++se)
        {
            for (size_t sh = 0; sh < chipProps.gfx6.numShaderArrays; ++sh)
            {
                activeCuMask   &= chipProps.gfx6.activeCuMaskGfx6[se][sh];
                alwaysOnCuMask &= chipProps.gfx6.alwaysOnCuMaskGfx6[se][sh];
            }
        }

        // Technically, each SE/SH on a chip could have a different mask for active CU's and/or always-on CU's. This
        // would require that our preamble have one write to the GRBM_GFX_INDEX and SPI_STATIC_THREAD_MGMT_3 registers
        // per SE/SH along with another write to GRBM_GFX_INDEX. The DXX driver does this extra work once during Device
        // init to set-up the load/shadow memory. However, DXX only shadows one copy of the SPI_STATIC_THREAD_MGMT_3
        // instead of all copies. Since DXX hasn't had any problems only restoring one copy from shadow memory, we'll
        // assume that we can simply write one copy and that there is at least one always-on CU which is common to all
        // SE/SH on every Gfx6 chip.

        // The always-on CU mask should always be a nonzero subset of the actuve CU mask.
        PAL_ASSERT((alwaysOnCuMask != 0) && ((activeCuMask & alwaysOnCuMask) == alwaysOnCuMask));

        uint32 cuIndex = 0;
        BitMaskScanForward(&cuIndex, alwaysOnCuMask);

        regSPI_STATIC_THREAD_MGMT_3__SI spiStaticThreadMgmt3 = { };
        spiStaticThreadMgmt3.bits.LSHS_CU_EN = (activeCuMask & ~(0x1 << cuIndex));

        pCmdSpace = m_deCmdStream.WriteSetOneConfigReg(mmSPI_STATIC_THREAD_MGMT_3__SI,
                                                       spiStaticThreadMgmt3.u32All,
                                                       pCmdSpace);
    }
    else if (chipProps.gfxLevel >= GfxIpLevel::GfxIp8)
    {
        // The register spec suggests these values are optimal settings for Gfx9 hardware, when VS half-pack mode is
        // disabled. If half-pack mode is active, we need to use the legacy defaults which are safer (but less optimal).
        regVGT_OUT_DEALLOC_CNTL vgtOutDeallocCntl = { };
        vgtOutDeallocCntl.bits.DEALLOC_DIST = (settings.vsHalfPackThreshold >= MaxVsExportSemantics) ? 32 : 16;

        // Set patch and donut distribution thresholds for tessellation. If we decide that this should be tunable
        // per-pipeline, we can move the registers to the Pipeline object (DXX currently uses per-Device thresholds).
        regVGT_TESS_DISTRIBUTION__VI vgtTessDistribution = { };
        vgtTessDistribution.bits.ACCUM_ISOLINE = settings.gfx8PatchDistributionFactor;
        vgtTessDistribution.bits.ACCUM_TRI     = settings.gfx8PatchDistributionFactor;
        vgtTessDistribution.bits.ACCUM_QUAD    = settings.gfx8PatchDistributionFactor;
        vgtTessDistribution.bits.DONUT_SPLIT   = settings.gfx8DonutDistributionFactor;
        vgtTessDistribution.bits.TRAP_SPLIT    = settings.gfx8TrapezoidDistributionFactor;

        // Set-and-forget DCC register.
        regCB_DCC_CONTROL__VI cbDccControl = { };
        cbDccControl.bits.OVERWRITE_COMBINER_MRT_SHARING_DISABLE = 1;
        cbDccControl.bits.OVERWRITE_COMBINER_WATERMARK = 4; // Should default to 4 according to register spec
        cbDccControl.bits.OVERWRITE_COMBINER_DISABLE   = 0; // Default enable DCC overwrite combiner

        regPA_SU_SMALL_PRIM_FILTER_CNTL__VI paSuSmallPrimFilterCntl = { };
        // Polaris10 small primitive filter control
        const uint32 smallPrimFilter = m_pDevice->GetSmallPrimFilter();
        if (smallPrimFilter != SmallPrimFilterDisable)
        {
            paSuSmallPrimFilterCntl.bits.SMALL_PRIM_FILTER_ENABLE = 1;

            paSuSmallPrimFilterCntl.bits.POINT_FILTER_DISABLE =
                ((smallPrimFilter & SmallPrimFilterEnablePoint) == 0);

            paSuSmallPrimFilterCntl.bits.LINE_FILTER_DISABLE =
                ((smallPrimFilter & SmallPrimFilterEnableLine) == 0);

            paSuSmallPrimFilterCntl.bits.TRIANGLE_FILTER_DISABLE =
                ((smallPrimFilter & SmallPrimFilterEnableTriangle) == 0);

            paSuSmallPrimFilterCntl.bits.RECTANGLE_FILTER_DISABLE =
                ((smallPrimFilter & SmallPrimFilterEnableRectangle) == 0);
        }
        else
        {
            paSuSmallPrimFilterCntl.bits.SMALL_PRIM_FILTER_ENABLE = 0;
        }

        pCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmVGT_OUT_DEALLOC_CNTL, vgtOutDeallocCntl.u32All, pCmdSpace);
        pCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmVGT_TESS_DISTRIBUTION__VI,
                                                        vgtTessDistribution.u32All,
                                                        pCmdSpace);
        pCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmCB_DCC_CONTROL__VI,
                                                        cbDccControl.u32All,
                                                        pCmdSpace);

        // Note that this register may not be present in non-Polaris10, but we choose to always write this register
        // keep things simple.  Writes to this register on non-Polaris10 are expected to be ignored by HW.
        pCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmPA_SU_SMALL_PRIM_FILTER_CNTL__VI,
                                                        paSuSmallPrimFilterCntl.u32All,
                                                        pCmdSpace);
    }

    return WriteCommonPreamble(*m_pDevice, EngineTypeUniversal, &m_deCmdStream, pCmdSpace);
}

// =====================================================================================================================
// Writes commands issuing a context_control as well as other state-shadowing related things.
uint32* UniversalQueueContext::WriteStateShadowingCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace)
{
    const CmdUtil& cmdUtil = m_pDevice->CmdUtil();

    // By default, PAL doesn't preserve GPU state across command buffer boundaries, thus we don't need to enable state
    // shadowing.  However, we do need to enable loading context registers to support loading fast-clear colors/values.

    CONTEXT_CONTROL_ENABLE shadowBits = {};
    shadowBits.enableDw = 1;

    CONTEXT_CONTROL_ENABLE loadBits = {};
    loadBits.enableDw                 = 1;
    loadBits.enableMultiCntxRenderReg = 1;

    if (m_useShadowing)
    {
        PAL_ASSERT(m_pDevice->Parent()->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp8);

        // If mid command buffer preemption is enabled, shadowing and loading must be enabled for all register types
        // because the GPU state needs to be properly restored when this Queue resumes execution after being preempted.
        // (Config registers are excluded because MCBP is not supported on pre-Gfx8 hardware.
        loadBits.enableUserConfigReg__CI = 1;
        loadBits.enableCSSHReg           = 1;
        loadBits.enableGfxSHReg          = 1;

        shadowBits = loadBits;
        shadowBits.enableSingleCntxConfigReg = 1;
    }

    pCmdSpace += cmdUtil.BuildContextControl(loadBits, shadowBits, pCmdSpace);
    pCmdSpace += cmdUtil.BuildClearState(pCmdSpace);

    if (m_useShadowing)
    {
        gpusize gpuVirtAddr = m_shadowGpuMem.GpuVirtAddr();

        pCmdSpace += cmdUtil.BuildLoadUserConfigRegs(gpuVirtAddr,
                                                     &UserConfigShadowRangeGfx7[0],
                                                     NumUserConfigShadowRangesGfx7,
                                                     pCmdSpace);
        gpuVirtAddr += (sizeof(uint32) * UserConfigRegCount);

        if (m_pDevice->Parent()->ChipProperties().gfx6.rbReconfigureEnabled)
        {
            pCmdSpace += cmdUtil.BuildLoadContextRegs(gpuVirtAddr,
                                                      &ContextShadowRangeRbReconfig[0],
                                                      NumContextShadowRangesRbReconfig,
                                                      pCmdSpace);
        }
        else
        {
            pCmdSpace += cmdUtil.BuildLoadContextRegs(gpuVirtAddr,
                                                      &ContextShadowRange[0],
                                                      NumContextShadowRanges,
                                                      pCmdSpace);
        }
        gpuVirtAddr += (sizeof(uint32) * CntxRegCountGfx7);

        pCmdSpace += cmdUtil.BuildLoadShRegs(gpuVirtAddr,
                                             &GfxShShadowRange[0],
                                             NumGfxShShadowRanges,
                                             ShaderGraphics,
                                             pCmdSpace);
        pCmdSpace += cmdUtil.BuildLoadShRegs(gpuVirtAddr,
                                             &CsShShadowRange[0],
                                             NumCsShShadowRanges,
                                             ShaderCompute,
                                             pCmdSpace);
        gpuVirtAddr += (sizeof(uint32) * ShRegCount);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Writes commands to install the device's currently bound trap handler and trap buffer for the specified pipeline type.
static uint32* WriteTrapInstallCmds(
    const Device*     pDevice,
    CmdStream*        pCmdStream,
    PipelineBindPoint pipelineType,
    uint32*           pCmdSpace)
{
    constexpr uint32 GraphicsRegAddrs[] =
    {
        mmSPI_SHADER_TBA_LO_LS,
        mmSPI_SHADER_TBA_LO_HS,
        mmSPI_SHADER_TBA_LO_ES,
        mmSPI_SHADER_TBA_LO_GS,
        mmSPI_SHADER_TBA_LO_VS,
        mmSPI_SHADER_TBA_LO_PS
    };

    constexpr uint32 NumGraphicsRegAddrs = static_cast<uint32>(ArrayLen(GraphicsRegAddrs));

    constexpr uint32 ComputeRegAddrs[] =
    {
        mmCOMPUTE_TBA_LO
    };

    constexpr uint32 NumComputeRegAddrs = static_cast<uint32>(ArrayLen(ComputeRegAddrs));

    const PM4ShaderType shaderType = (pipelineType == PipelineBindPoint::Graphics) ? ShaderGraphics : ShaderCompute;
    const uint32* const pRegAddrs  = (pipelineType == PipelineBindPoint::Graphics) ? GraphicsRegAddrs : ComputeRegAddrs;
    const uint32        numAddrs   = (pipelineType == PipelineBindPoint::Graphics) ? NumGraphicsRegAddrs :
                                                                                     NumComputeRegAddrs;

    const BoundGpuMemory& trapHandler = pDevice->TrapHandler(pipelineType);
    const gpusize tbaGpuVirtAddr      = (trapHandler.IsBound()) ? trapHandler.GpuVirtAddr() : 0;

    const BoundGpuMemory& trapBuffer  = pDevice->TrapBuffer(pipelineType);
    const gpusize tmaGpuVirtAddr      = (trapBuffer.IsBound()) ? trapBuffer.GpuVirtAddr() : 0;

    // Program these registers only if trap handler/buffer are bound.
    if ((tbaGpuVirtAddr != 0) && (tmaGpuVirtAddr != 0))
    {
        for (uint32 i = 0; i < numAddrs; i++)
        {
            const uint32 regVals[] =
            {
                Get256BAddrLo(tbaGpuVirtAddr),
                Get256BAddrHi(tbaGpuVirtAddr),
                Get256BAddrLo(tmaGpuVirtAddr),
                Get256BAddrHi(tmaGpuVirtAddr)
            };

            pCmdSpace = pCmdStream->WriteSetSeqShRegs(pRegAddrs[i],
                                                      pRegAddrs[i] + 3,
                                                      shaderType,
                                                      &regVals[0],
                                                      pCmdSpace);
        }
    }

    return pCmdSpace;
}

} // Gfx6
} // Pal
