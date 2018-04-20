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

#include "core/cmdAllocator.h"
#include "core/platform.h"
#include "core/queue.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6ComputeEngine.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6Preambles.h"
#include "core/hw/gfxip/gfx6/gfx6QueueContexts.h"
#include "core/hw/gfxip/gfx6/gfx6ShaderRingSet.h"
#include "core/hw/gfxip/gfx6/gfx6UniversalEngine.h"
#include "core/hw/gfxip/gfx6/g_gfx6PalSettings.h"
#include "core/hw/gfxip/gfx6/g_gfx6ShadowedRegistersInit.h"
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
// Assembles and initializes the PM4 commands for the common preamble image.
static void SetupCommonPreamble(
    Device*               pDevice,
    CommonPreamblePm4Img* pCommonPreamble)
{
    memset(pCommonPreamble, 0, sizeof(CommonPreamblePm4Img));

    const CmdUtil&           cmdUtil   = pDevice->CmdUtil();
    const GpuChipProperties& chipProps = pDevice->Parent()->ChipProperties();

    // First build the PM4 headers.

    pCommonPreamble->spaceNeeded +=
        cmdUtil.BuildSetSeqShRegsIndex(mmCOMPUTE_STATIC_THREAD_MGMT_SE0,
                                       mmCOMPUTE_STATIC_THREAD_MGMT_SE1,
                                       ShaderCompute,
                                       SET_SH_REG_INDEX_CP_MODIFY_CU_MASK,
                                       &pCommonPreamble->hdrThreadMgmt01);

    if (chipProps.gfxLevel != GfxIpLevel::GfxIp6)
    {
        // On Gfx6, the registers in the 3rd PM4 packet are not present; no need to build it.
        pCommonPreamble->spaceNeeded +=
            cmdUtil.BuildSetSeqShRegsIndex(mmCOMPUTE_STATIC_THREAD_MGMT_SE2__CI__VI,
                                           mmCOMPUTE_STATIC_THREAD_MGMT_SE3__CI__VI,
                                           ShaderCompute,
                                           SET_SH_REG_INDEX_CP_MODIFY_CU_MASK,
                                           &pCommonPreamble->hdrThreadMgmt23);
    }

    // Now set up the values for the registers being written.

    // It's legal to set the CU mask to enable all CUs. The UMD does not need to know about active CUs and harvested CUs
    // at this point. Using the packet SET_SH_REG_INDEX, the UMD mask will be ANDed with the KMD mask so that UMD does
    // not use the CUs that are intended for real time compute usage.

    // Enable Compute workloads on all CU's of SE0/SE1.
    pCommonPreamble->computeStaticThreadMgmtSe0.bits.SH0_CU_EN = 0xFFFF;
    pCommonPreamble->computeStaticThreadMgmtSe0.bits.SH1_CU_EN = 0xFFFF;
    pCommonPreamble->computeStaticThreadMgmtSe1.bits.SH0_CU_EN = 0xFFFF;
    pCommonPreamble->computeStaticThreadMgmtSe1.bits.SH1_CU_EN = 0xFFFF;

    // On Gfx6, the registers in the 3rd PM4 packet are not present; no need to initialize them.
    if (chipProps.gfxLevel != GfxIpLevel::GfxIp6)
    {
        // Enable Compute workloads on all CU's of SE2/SE3.
        pCommonPreamble->computeStaticThreadMgmtSe2.bits.SH0_CU_EN = 0xFFFF;
        pCommonPreamble->computeStaticThreadMgmtSe2.bits.SH1_CU_EN = 0xFFFF;
        pCommonPreamble->computeStaticThreadMgmtSe3.bits.SH0_CU_EN = 0xFFFF;
        pCommonPreamble->computeStaticThreadMgmtSe3.bits.SH1_CU_EN = 0xFFFF;
    }
}

#if !PAL_COMPUTE_GDS_OPT
// =====================================================================================================================
// Returns the size, in DWORDS, of the appropriate GdsRange packet image struct.
static size_t GetGdsCounterRangeSize(
    bool isCompute)
{
    return (isCompute) ? (sizeof(GdsRangeCompute)  / sizeof(uint32)) :
                         (sizeof(GdsRangeGraphics) / sizeof(uint32));
}

// =====================================================================================================================
// Builds the GDS range PM4 packets for graphics for the given queue.
static void BuildGdsRangeGraphics(
    Device*           pDevice,
    uint32            queueIndex,
    GdsRangeGraphics* pGdsRange)
{
    // Get GDS range associated with this engine.
    GdsInfo gdsInfo = pDevice->Parent()->GdsInfo(EngineTypeUniversal, queueIndex);

    if (pDevice->Parent()->ChipProperties().gfxLevel != GfxIpLevel::GfxIp6)
    {
        // The register for SC does not work off of a zero-based address on Gfx6, instead relying on an accurate offset
        // into the overall GDS allocation. But for newer HW we simply use a zero based offset.
        gdsInfo.offset = 0;
    }

    if (pDevice->Parent()->PerPipelineBindPointGds())
    {
        // If per-pipeline bind point GDS partitions were requested then on the universal queue the GDS partition of the
        // engine is split into two so we have to adjust the size.
        // Additionally, as the graphics partition is in the second half, we also need to adjust the offset.
        gdsInfo.size /= 2;
        gdsInfo.offset += gdsInfo.size;
    }

    GdsData gdsData = {};
    gdsData.gdsOffset = gdsInfo.offset;
    gdsData.gdsSize   = gdsInfo.size;

    const CmdUtil& cmdUtil = pDevice->CmdUtil();

    // Setup the GDS data register write for the correct USER_DATA register in each stage
    cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_LS_0 + GdsRangeReg, ShaderGraphics, &pGdsRange->headerLs);
    pGdsRange->gdsDataLs = gdsData;

    cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_HS_0 + GdsRangeReg, ShaderGraphics, &pGdsRange->headerHs);
    pGdsRange->gdsDataHs = gdsData;

    cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_ES_0 + GdsRangeReg, ShaderGraphics, &pGdsRange->headerEs);
    pGdsRange->gdsDataEs = gdsData;

    cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_GS_0 + GdsRangeReg, ShaderGraphics, &pGdsRange->headerGs);
    pGdsRange->gdsDataGs = gdsData;

    cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_VS_0 + GdsRangeReg, ShaderGraphics, &pGdsRange->headerVs);
    pGdsRange->gdsDataVs = gdsData;

    cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_PS_0 + GdsRangeReg, ShaderGraphics, &pGdsRange->headerPs);
    pGdsRange->gdsDataPs = gdsData;
}
#endif

// =====================================================================================================================
// Builds the GDS range PM4 packets for compute for the given queue.
static void BuildGdsRangeCompute(
    Device*           pDevice,
    EngineType        engineType,
    uint32            queueIndex,
    GdsRangeCompute*  pGdsRange)
{
    // Get GDS range associated with this engine.
    GdsInfo gdsInfo = pDevice->Parent()->GdsInfo(engineType, queueIndex);

    if (pDevice->Parent()->ChipProperties().gfxLevel != GfxIpLevel::GfxIp6)
    {
        // The register for SC does not work off of a zero-based address on Gfx6, instead relying on an accurate offset
        // into the overall GDS allocation. But for newer HW we simply use a zero based offset.
        gdsInfo.offset = 0;
    }

    if ((engineType == EngineTypeUniversal) && pDevice->Parent()->PerPipelineBindPointGds())
    {
        // If per-pipeline bind point GDS partitions were requested then on the universal queue the GDS partition of the
        // engine is split into two so we have to adjust the size.
        gdsInfo.size /= 2;
    }

#if !PAL_COMPUTE_GDS_OPT
    pDevice->CmdUtil().BuildSetOneShReg(mmCOMPUTE_USER_DATA_0 + GdsRangeReg, ShaderCompute, &pGdsRange->header);
#else
    pDevice->CmdUtil().BuildSetOneShReg(mmCOMPUTE_USER_DATA_0 + ComputeGdsRangeReg, ShaderCompute, &pGdsRange->header);
#endif
    pGdsRange->gdsData.gdsOffset = gdsInfo.offset;
    pGdsRange->gdsData.gdsSize   = gdsInfo.size;
}

// =====================================================================================================================
ComputeQueueContext::ComputeQueueContext(
    Device* pDevice,
    Queue*  pQueue,
    Engine* pEngine,
    uint32  queueId)
    :
    QueueContext(pDevice->Parent()),
    m_pDevice(pDevice),
    m_pQueue(pQueue),
    m_pEngine(static_cast<ComputeEngine*>(pEngine)),
    m_queueId(queueId),
    m_currentUpdateCounter(0),
    m_cmdStream(*pDevice,
                pDevice->Parent()->InternalUntrackedCmdAllocator(), EngineTypeCompute,
                SubQueueType::Primary,
                false,
                true),              // Preambles cannot be preemptible.
    m_perSubmitCmdStream(*pDevice,
                         pDevice->Parent()->InternalUntrackedCmdAllocator(), EngineTypeCompute,
                         SubQueueType::Primary,
                         false,
                         true),     // Preambles cannot be preemptible.
    m_postambleCmdStream(*pDevice,
                         pDevice->Parent()->InternalUntrackedCmdAllocator(), EngineTypeCompute,
                         SubQueueType::Primary,
                         false,
                         true)      // Postambles cannot be preemptible.
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
        result = CreateTimestampMem();
    }

    if (result == Result::Success)
    {
        SetupCommonPreamble(m_pDevice, &m_commonPreamble);
        BuildComputePreambleHeaders();
        SetupComputePreambleRegisters();
        RebuildCommandStream();
    }

    return result;
}

// =====================================================================================================================
// Checks if the queue context preamble needs to be rebuilt, possibly due to the client creating new pipelines that
// require a bigger scratch ring, or due the client binding a new trap handler/buffer.  If so, the the compute shader
// rings are re-validated and our context command stream is rebuilt.
Result ComputeQueueContext::PreProcessSubmit(
    InternalSubmitInfo* pSubmitInfo,
    const SubmitInfo&   submitInfo)
{
    bool   hasUpdated = false;
    Result result     = m_pEngine->UpdateRingSet(&m_currentUpdateCounter, &hasUpdated);

    if ((result == Result::Success) && hasUpdated)
    {
        RebuildCommandStream();
    }

    pSubmitInfo->pPreambleCmdStream[0]  = &m_perSubmitCmdStream;
    pSubmitInfo->pPreambleCmdStream[1]  = &m_cmdStream;
    pSubmitInfo->pPostambleCmdStream[0] = &m_postambleCmdStream;

    pSubmitInfo->numPreambleCmdStreams  = 2;
    pSubmitInfo->numPostambleCmdStreams = 1;

    pSubmitInfo->pagingFence = m_pDevice->Parent()->InternalUntrackedCmdAllocator()->LastPagingFence();

    return result;
}

// =====================================================================================================================
// Marks the context command stream as droppable, so the KMD can optimize away its execution in cases where there is no
// application context switch between back-to-back submissions.
void ComputeQueueContext::PostProcessSubmit()
{
    if (m_pDevice->Settings().forcePreambleCmdStream == false)
    {
        // The next time this Queue is submitted-to, the KMD can safely skip the execution of the command stream since
        // the GPU already has received the latest updates.
        m_cmdStream.EnableDropIfSameContext(true);
    }
}

// =====================================================================================================================
// Regenerates the contents of this context's internal command stream.
void ComputeQueueContext::RebuildCommandStream()
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

    constexpr CmdStreamBeginFlags beginFlags = {};
    const CmdUtil&                cmdUtil    = m_pDevice->CmdUtil();

    // The drop-if-same-context queue preamble.
    //==================================================================================================================

    m_cmdStream.Reset(nullptr, true);
    m_cmdStream.Begin(beginFlags, nullptr);

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    // Write the shader ring-set's commands before the command stream's normal preamble. If the ring sizes have changed,
    // the hardware requires a CS partial flush to operate properly.
    pCmdSpace = m_pEngine->RingSet()->WriteCommands(&m_cmdStream, pCmdSpace);

    pCmdSpace += cmdUtil.BuildEventWrite(CS_PARTIAL_FLUSH, pCmdSpace);

    // Copy the common preamble commands and compute-specific preamble commands.
    pCmdSpace = m_cmdStream.WritePm4Image(m_commonPreamble.spaceNeeded,  &m_commonPreamble,  pCmdSpace);
    pCmdSpace = m_cmdStream.WritePm4Image(m_computePreamble.spaceNeeded, &m_computePreamble, pCmdSpace);

    pCmdSpace = WriteTrapInstallCmds(m_pDevice, &m_cmdStream, PipelineBindPoint::Compute, pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
    m_cmdStream.End();

    // The per-submit preamble.
    //==================================================================================================================

    m_perSubmitCmdStream.Reset(nullptr, true);
    m_perSubmitCmdStream.Begin(beginFlags, nullptr);

    pCmdSpace = m_perSubmitCmdStream.ReserveCommands();

    // The following wait, write data, and surface sync must be at the beginning of the per-submit preamble.
    //
    // Wait for a prior submission on this context to be idle before executing the command buffer streams.
    // The timestamp memory is initialized to zero so the first submission on this context will not wait.
    pCmdSpace += cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_MEMORY,
                                         WAIT_REG_MEM_FUNC_EQUAL,
                                         WAIT_REG_MEM_ENGINE_PFP,
                                         m_timestampMem.GpuVirtAddr(),
                                         0,
                                         0xFFFFFFFF,
                                         false,
                                         pCmdSpace);

    // Then rewrite the timestamp to some other value so that the next submission will wait until this one is done.
    constexpr uint32 BusyTimestamp = 1;
    pCmdSpace += cmdUtil.BuildWriteData(m_timestampMem.GpuVirtAddr(),
                                        1,
                                        WRITE_DATA_ENGINE_PFP,
                                        WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                        true,
                                        &BusyTimestamp,
                                        PredDisable,
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
    m_perSubmitCmdStream.End();

    // The per-submit postamble.
    //==================================================================================================================

    m_postambleCmdStream.Reset(nullptr, true);
    m_postambleCmdStream.Begin(beginFlags, nullptr);

    pCmdSpace = m_postambleCmdStream.ReserveCommands();

    // This EOP event packet must be at the end of the per-submit postamble.
    //
    // When the pipeline has emptied, write the timestamp back to zero so that the next submission can execute.
    // We also use this pipelined event to flush and invalidate the shader L2 cache as described above.
    pCmdSpace += cmdUtil.BuildGenericEopEvent(BOTTOM_OF_PIPE_TS,
                                              m_timestampMem.GpuVirtAddr(),
                                              EVENTWRITEEOP_DATA_SEL_SEND_DATA32,
                                              0,
                                              true,
                                              true,
                                              pCmdSpace);

    m_postambleCmdStream.CommitCommands(pCmdSpace);
    m_postambleCmdStream.End();

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
}

// =====================================================================================================================
// Assembles the compute-only specific PM4 headers for the queue context preamble.
void ComputeQueueContext::BuildComputePreambleHeaders()
{
    memset(&m_computePreamble, 0, sizeof(m_computePreamble));

    m_computePreamble.spaceNeeded += (sizeof(GdsRangeCompute) / sizeof(uint32));
}

// =====================================================================================================================
// Sets up the compute-specific PM4 commands for the queue context preamble.
void ComputeQueueContext::SetupComputePreambleRegisters()
{
    BuildGdsRangeCompute(m_pDevice, m_pEngine->Type(), m_queueId, &m_computePreamble.gdsRange);
}

// =====================================================================================================================
UniversalQueueContext::UniversalQueueContext(
    Device* pDevice,
    Queue*  pQueue,
    Engine* pEngine,
    uint32  queueId)
    :
    QueueContext(pDevice->Parent()),
    m_pDevice(pDevice),
    m_pQueue(pQueue),
    m_pEngine(static_cast<UniversalEngine*>(pEngine)),
    m_queueId(queueId),
    m_currentUpdateCounter(0),
    m_useShadowing((Device::ForceStateShadowing &&
                    pDevice->Parent()->ChipProperties().gfx6.supportLoadRegIndexPkt) ||
                   m_pQueue->IsPreemptionSupported()),
    m_shadowGpuMemSizeInBytes(0),
    m_shadowedRegCount(0),
    m_submitCounter(0),
    m_deCmdStream(*pDevice,
                  pDevice->Parent()->InternalUntrackedCmdAllocator(),
                  EngineTypeUniversal,
                  SubQueueType::Primary,
                  false,
                  true),                    // Preambles cannot be preemptible.
    m_perSubmitCmdStream(*pDevice,
                         pDevice->Parent()->InternalUntrackedCmdAllocator(),
                         EngineTypeUniversal,
                         SubQueueType::Primary,
                         false,
                         true),             // Preambles cannot be preemptible.
    m_cePreambleCmdStream(*pDevice,
                          pDevice->Parent()->InternalUntrackedCmdAllocator(),
                          EngineTypeUniversal,
                          SubQueueType::ConstantEnginePreamble,
                          false,
                          true),            // Preambles cannot be preemptible.
    m_cePostambleCmdStream(*pDevice,
                           pDevice->Parent()->InternalUntrackedCmdAllocator(),
                           EngineTypeUniversal,
                           SubQueueType::ConstantEngine,
                           false,
                           true),           // Postambles cannot be preemptible.
    m_dePostambleCmdStream(*pDevice,
                           pDevice->Parent()->InternalUntrackedCmdAllocator(),
                           EngineTypeUniversal,
                           SubQueueType::Primary,
                           false,
                           true)            // Postambles cannot be preemptible.
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
        result = CreateTimestampMem();
    }

    if (result == Result::Success)
    {
        result = AllocateShadowMemory();
    }

    if (result == Result::Success)
    {
        SetupCommonPreamble(m_pDevice, &m_commonPreamble);
        BuildUniversalPreambleHeaders();
        SetupUniversalPreambleRegisters();

        RebuildCommandStreams();
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
    uint32 ceRamBytes = (m_pQueue->PersistentCeRamSize() * sizeof(uint32));

    if (m_useShadowing)
    {
        // If mid command buffer preemption is enabled, we must also include shadow space for all of the context,
        // SH, and user-config registers. This is because the CP will restore the whole state when resuming this
        // Queue from being preempted.
        m_shadowedRegCount = (ShRegCount + CntxRegCountGfx7 + UserConfigRegCount);

        // Also, if mid command buffer preemption is enabled, we must restore all CE RAM used by the client and
        // internally by PAL. All of that data will need to be restored aftere resuming this Queue from being
        // preempted.
        ceRamBytes = static_cast<uint32>(ReservedCeRamBytes + pDevice->CeRamBytesUsed(EngineTypeUniversal));
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
// Checks if the queue context preamble needs to be rebuilt, possibly due to the client creating new pipelines that
// require bigger rings, or due the client binding a new trap handler/buffer.  If so, the the shader rings are
// re-validated and our context command stream is rebuilt.
// When MCBP is enabled, we'll force the command stream to be rebuilt when we submit the command for the first time,
// because we need to build set commands to initialize the context register and shadow memory. The sets only need to be
// done once, so we need to rebuild the command stream on the second submit.
Result UniversalQueueContext::PreProcessSubmit(
    InternalSubmitInfo* pSubmitInfo,
    const SubmitInfo&   submitInfo)
{
    bool   hasUpdated = false;
    Result result     = Result::Success;

    // We only need to rebuild the command stream if the user submits at least one command buffer.
    if ((result == Result::Success) && (submitInfo.cmdBufferCount != 0))
    {
        result = m_pEngine->UpdateRingSet(&m_currentUpdateCounter, &hasUpdated);

        const bool mcbpForceUpdate = m_useShadowing && (m_submitCounter <= 1);

        // Like UpdateRingSet, we need to idle the queue before we need to RebuildCommandStreams.
        if (mcbpForceUpdate && (hasUpdated == false))
        {
            result = m_pQueue->WaitIdle();
        }

        if ((result == Result::Success) && (hasUpdated || mcbpForceUpdate))
        {
            RebuildCommandStreams();
        }
        m_submitCounter++;
    }

    uint32 preambleCount  = 0;
    if (m_cePreambleCmdStream.IsEmpty() == false)
    {
        pSubmitInfo->pPreambleCmdStream[preambleCount] = &m_cePreambleCmdStream;
        ++preambleCount;
    }

    pSubmitInfo->pPreambleCmdStream[preambleCount] = &m_perSubmitCmdStream;
    ++preambleCount;

    if (m_pDevice->Settings().commandBufferCombineDePreambles == false)
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

    return result;
}

// =====================================================================================================================
// Marks the context command stream as droppable, so the KMD can optimize away its execution in cases where there is no
// application context switch between back-to-back submissions.
void UniversalQueueContext::PostProcessSubmit()
{
    if (m_pDevice->Settings().forcePreambleCmdStream == false)
    {
        // The next time this Queue is submitted-to, the KMD can safely skip the execution of the command stream since
        // the GPU already has received the latest updates.
        m_deCmdStream.EnableDropIfSameContext(true);
        // NOTE: The per-submit command stream cannot receive this optimization because it must be executed for every
        // submit.
    }
}

// =====================================================================================================================
// Regenerates the contents of this context's internal command streams.
void UniversalQueueContext::RebuildCommandStreams()
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

    constexpr CmdStreamBeginFlags beginFlags = {};
    const     GpuChipProperties&  chipProps  = m_pDevice->Parent()->ChipProperties();
    const     CmdUtil&            cmdUtil    = m_pDevice->CmdUtil();

    // The drop-if-same-context DE preamble.
    //==================================================================================================================

    m_deCmdStream.Reset(nullptr, true);
    m_deCmdStream.Begin(beginFlags, nullptr);

    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

    // Copy the common preamble commands and the universal-specific preamble commands.
    pCmdSpace = m_deCmdStream.WritePm4Image(m_universalPreamble.spaceNeeded, &m_universalPreamble, pCmdSpace);
    pCmdSpace = m_deCmdStream.WritePm4Image(m_commonPreamble.spaceNeeded,    &m_commonPreamble,    pCmdSpace);

    if (m_gfx6UniversalPreamble.spaceNeeded > 0)
    {
        // Copy the Gfx6-specific universal preamble commands.
        pCmdSpace = m_deCmdStream.WritePm4Image(m_gfx6UniversalPreamble.spaceNeeded, &m_gfx6UniversalPreamble, pCmdSpace);
    }
    else if (m_gfx8UniversalPreamble.spaceNeeded > 0)
    {
        // Copy the Gfx8-specific universal preamble commands.
        pCmdSpace = m_deCmdStream.WritePm4Image(m_gfx8UniversalPreamble.spaceNeeded, &m_gfx8UniversalPreamble, pCmdSpace);
    }

    // Several context registers are considered "sticky" by the hardware team, which means that they broadcast their
    // value to all eight render contexts. Clear state cannot reset them properly if another driver changes them,
    // because that driver's writes will have clobbered the values in our clear state reserved GPU context. We need to
    // restore default values here to be on the safe side.
    constexpr uint32 StickyVgtRegValues[] =
    {
        UINT_MAX, // VGT_MAX_VTX_INDX
        0,        // VGT_MIN_VTX_INDX
        0,        // VGT_INDX_OFFSET
    };
    static_assert(sizeof(StickyVgtRegValues) == (sizeof(uint32) * (mmVGT_INDX_OFFSET - mmVGT_MAX_VTX_INDX + 1)),
                  "Unexpected number of sticky VGT register values!");

    pCmdSpace = m_deCmdStream.WriteSetSeqContextRegs(mmVGT_MAX_VTX_INDX,
                                                     mmVGT_INDX_OFFSET,
                                                     &StickyVgtRegValues[0],
                                                     pCmdSpace);

    const auto* pRingSet = m_pEngine->RingSet();

    // Write the shader ring-set's commands after the command stream's normal preamble. If the ring sizes have changed,
    // the hardware requires a CS/VS/PS partial flush to operate properly.
    pCmdSpace = pRingSet->WriteCommands(&m_deCmdStream, pCmdSpace);

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
    m_deCmdStream.End();

    // The per-submit DE preamble.
    //==================================================================================================================

    m_perSubmitCmdStream.Reset(nullptr, true);
    m_perSubmitCmdStream.Begin(beginFlags, nullptr);

    pCmdSpace = m_perSubmitCmdStream.ReserveCommands();

    // The following wait, write data, and surface sync must be at the beginning of the per-submit DE preamble.
    //
    // Wait for a prior submission on this context to be idle before executing the command buffer streams.
    // The timestamp memory is initialized to zero so the first submission on this context will not wait.
    pCmdSpace += cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_MEMORY,
                                         WAIT_REG_MEM_FUNC_EQUAL,
                                         WAIT_REG_MEM_ENGINE_PFP,
                                         m_timestampMem.GpuVirtAddr(),
                                         0,
                                         0xFFFFFFFF,
                                         false,
                                         pCmdSpace);

    // Then rewrite the timestamp to some other value so that the next submission will wait until this one is done.
    constexpr uint32 BusyTimestamp = 1;
    pCmdSpace += cmdUtil.BuildWriteData(m_timestampMem.GpuVirtAddr(),
                                        1,
                                        WRITE_DATA_ENGINE_PFP,
                                        WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                        true,
                                        &BusyTimestamp,
                                        PredDisable,
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

    // Set up state shadowing.
    pCmdSpace = m_perSubmitCmdStream.WritePm4Image(m_stateShadowPreamble.spaceNeeded,
                                                   &m_stateShadowPreamble,
                                                   pCmdSpace);

    // If the preemption is enabled, we need to initialize the shadow copy of this register.
    if (m_useShadowing)
    {
        // Only DX9P calls ICmdBuffer::CmdSetGlobalScissor, which writes both mmPA_SC_WINDOW_SCISSOR_TL|BR.
        // Until all other clients call this function, we'll have to initialize the register.
        regPA_SC_WINDOW_SCISSOR_BR paScWindowScissorBr = {};
        paScWindowScissorBr.bitfields.BR_X = 0x4000;
        paScWindowScissorBr.bitfields.BR_Y = 0x4000;
        pCmdSpace = m_perSubmitCmdStream.WriteSetOneContextReg(mmPA_SC_WINDOW_SCISSOR_BR,
                                                               paScWindowScissorBr.u32All,
                                                               pCmdSpace);
    }

    if (m_useShadowing && (m_submitCounter == 0))
    {
        // The following call to InitializeContextRegistersGfx8() will initialize our shadow memory for MCBP in a way
        // that matches the clear state.  The (m_submitCounter == 0) check above should ensure that these commands are
        // only inserted during the first submit on this queue.
        //
        // Unfortunately, there is a possibility that the first submit could be preempted.  In that case, the
        // initialization commands will be replayed on resume since this queue context command stream will be marked
        // as non-preemptable.  If that happens, those commands would end up overwriting the shadowed context
        // registers that will be loaded before resuming the app's command buffer.  To prevent this issue, we surround
        // the commands written by InitializeContextRegistersGfx8() with a COND_EXEC packet that can skip the
        // initialization commands once they have been executed a single time.
        //
        // We use the following packets to make sure the SETs are done once:
        //
        // 1. COND_EXEC:  Initially programmed to skip just the NOP.  The WRITE_DATA will patch this command so that if
        //                this command stream is executed again on a MCBP resume, it will skip the NOP, SETs, and
        //                WRITE_DATA.
        //
        // 2. NOP:        Just used to hide a control dword for the COND_EXEC command.  The control word will always be
        //                programmed to 0 so that the COND_EXEC always skips execution.
        //
        // 3. DMA:        Use DMA packet to initialize the shadow memory to 0. Load the user config and sh registers
        //                after this to initialize them.
        //
        // 4. SETs:       Commands written by InitializeContextRegistersGfx8(). DMA_DATA packet is used before SETs
        //                to initialize the shadow memory to 0, also needs to be done once.
        //
        // 5. WRITE_DATA: Updates the size field of the COND_EXEC to a larger value so that the COND_EXEC will now
        //                skip the NOP, SETs, and WRITE_DATA, as soon as the GPU has executed the
        //                InitializeContextRegistersGfx8() commands once.
        //
        // The COND_EXEC is technically not needed, this approach could be accomplished with just a NOP, SETs, and
        // WRITE_DATA where the WRITE_DATA updates the NOP to skip the SETs and WRITE_DATA.  However, that approach
        // would make dumping this queue context command stream useless, since all of the commands would end up as
        // the body of a NOP that would not be parsed.  The COND_EXEC approach is no slower, and will let the
        // disabled commands be parsed nicely when debugging.

        m_perSubmitCmdStream.CommitCommands(pCmdSpace);
        // Record the chunk index when we begin the commands. We expect the commands will fit in one chunk.
        const uint32  chunkIndexBegin = m_perSubmitCmdStream.GetNumChunks();

        // Record the GPU address of NOP so we can calculate how many dwords to skip for cond_exec packet.
        const gpusize nopStartGpuAddr = m_perSubmitCmdStream.GetCurrentGpuVa() +
                                        cmdUtil.GetCondExecSizeInDwords() * sizeof(uint32);
        const gpusize skipFlagGpuAddr = nopStartGpuAddr + cmdUtil.GetMinNopSizeInDwords() * sizeof(uint32);

        // We'll update the size later.
        const gpusize skipSizeGpuAddr = nopStartGpuAddr - sizeof(uint32);

        pCmdSpace = m_perSubmitCmdStream.ReserveCommands();
        // We only skip the NOP for the first time.
        pCmdSpace += cmdUtil.BuildCondExec(skipFlagGpuAddr, cmdUtil.GetMinNopSizeInDwords() + 1, pCmdSpace);
        pCmdSpace += cmdUtil.BuildNop(cmdUtil.GetMinNopSizeInDwords() + 1, pCmdSpace);
        // Cond_Exec will always skip.
        *(pCmdSpace - 1) = 0;

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

        m_perSubmitCmdStream.CommitCommands(pCmdSpace);

        // We do this after m_stateShadowPreamble, when the LOADs are done and HW knows the shadow memory.
        // First LOADs will load garbage. InitializeContextRegisters will init the register and also the shadow Memory.
        if (chipProps.gfxLevel >= GfxIpLevel::GfxIp8)
        {
            InitializeContextRegistersGfx8(&m_perSubmitCmdStream);
        }
        else
        {
            // Only Gfx8+ supports preemption.
            PAL_NOT_IMPLEMENTED();
        }

        const gpusize endOfSetsGpuAddr = m_perSubmitCmdStream.GetCurrentGpuVa();
        // Skip the NOP, DMA_DATA, all the SETs + writeData header size and 1 dword it writes.
        const uint32  condSizeDw       = static_cast<uint32>(endOfSetsGpuAddr - nopStartGpuAddr) / sizeof(uint32)
                                         + CmdUtil::GetWriteDataHeaderSize() + 1;

        pCmdSpace = m_perSubmitCmdStream.ReserveCommands();
        pCmdSpace += cmdUtil.BuildWriteData(skipSizeGpuAddr,
                                            1,
                                            WRITE_DATA_ENGINE_PFP,
                                            WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                            true,
                                            &condSizeDw,
                                            PredDisable,
                                            pCmdSpace);

        const uint32 chunkIndexEnd = m_perSubmitCmdStream.GetNumChunks();
        // We assume all the SET packets will fit in one chunk. So we only build one skip logic. If the sets are in
        // different chunks, the code is broken and we need to modify it.
        PAL_ASSERT(chunkIndexBegin == chunkIndexEnd);
    }

    if (m_pDevice->WaForceToWriteNonRlcRestoredRegs())
    {
        // Some hardware doesn't restore non-RLC registers following a power-management event. The workaround is to
        // restore those registers *every* submission, rather than just the ones following a ring resize event or
        // after a context switch between applications
        pCmdSpace = pRingSet->WriteNonRlcRestoredRegs(&m_perSubmitCmdStream, pCmdSpace);
    }

    m_perSubmitCmdStream.CommitCommands(pCmdSpace);
    m_perSubmitCmdStream.End();

    if (m_pDevice->Settings().commandBufferCombineDePreambles)
    {
        // Combine the preambles by chaining from the per-submit preamble to the per-context preamble.
        m_perSubmitCmdStream.PatchTailChain(&m_deCmdStream);
    }

    // The per-submit CE premable, CE postamble, and DE postamble.
    //==================================================================================================================

    // The DE postamble is always built. The CE preamble and postamble may not be needed.
    m_dePostambleCmdStream.Reset(nullptr, true);
    m_dePostambleCmdStream.Begin(beginFlags, nullptr);

    // If the client has requested that this Queue maintain persistent CE RAM contents, or if the Queue supports mid
    // command buffer preemption, we need to rebuild the CE preamble, as well as the CE & DE postambles.
    if ((m_pQueue->PersistentCeRamSize() != 0) || m_useShadowing)
    {
        PAL_ASSERT(m_shadowGpuMem.IsBound());
        const gpusize gpuVirtAddr = (m_shadowGpuMem.GpuVirtAddr() + (sizeof(uint32) * m_shadowedRegCount));
        uint32 ceRamByteOffset    = (m_pQueue->PersistentCeRamOffset() + ReservedCeRamBytes);
        uint32 ceRamDwordSize     =  m_pQueue->PersistentCeRamSize();

        if (m_useShadowing)
        {
            // If preemption is supported, we must save & restore all CE RAM used by either PAL or the client.
            ceRamByteOffset = 0;
            ceRamDwordSize  =
                static_cast<uint32>((ReservedCeRamDwords + m_pDevice->Parent()->CeRamDwordsUsed(EngineTypeUniversal)));
        }

        m_cePreambleCmdStream.Reset(nullptr, true);
        m_cePreambleCmdStream.Begin(beginFlags, nullptr);

        pCmdSpace  = m_cePreambleCmdStream.ReserveCommands();
        pCmdSpace += cmdUtil.BuildLoadConstRam(gpuVirtAddr, ceRamByteOffset, ceRamDwordSize, pCmdSpace);
        m_cePreambleCmdStream.CommitCommands(pCmdSpace);

        m_cePreambleCmdStream.End();

        // The postamble command streams which dump CE RAM at the end of the submission are only necessary if (1) the
        // client requested that this Queue maintains persistent CE RAM contents, or (2) this Queue supports mid
        // command buffer preemption and the panel setting to force the dump CE RAM postamble is set.
        if ((m_pQueue->PersistentCeRamSize() != 0) ||
            (m_pDevice->Settings().commandBufferForceCeRamDumpInPostamble != false))
        {
            // On gfx6-7 we need to synchronize the CE/DE counters after the dump CE RAM because the dump writes to L2
            // and the load reads from memory. The DE postamble's EOP event will flush L2 but we still need to use the
            // CE/DE counters to stall the DE until the dump is complete.
            const bool syncCounters = (chipProps.gfxLevel <= GfxIpLevel::GfxIp7);

            // Note that it's illegal to touch the CE/DE counters in postamble streams if MCBP is enabled. In practice
            // we don't expect these two conditions to be enabled at the same time.
            PAL_ASSERT((syncCounters == false) || (m_useShadowing == false));

            m_cePostambleCmdStream.Reset(nullptr, true);
            m_cePostambleCmdStream.Begin(beginFlags, nullptr);

            pCmdSpace  = m_cePostambleCmdStream.ReserveCommands();
            pCmdSpace += cmdUtil.BuildDumpConstRam(gpuVirtAddr, ceRamByteOffset, ceRamDwordSize, pCmdSpace);

            if (syncCounters)
            {
                pCmdSpace += cmdUtil.BuildIncrementCeCounter(pCmdSpace);
            }

            m_cePostambleCmdStream.CommitCommands(pCmdSpace);
            m_cePostambleCmdStream.End();

            if (syncCounters)
            {
                pCmdSpace  = m_dePostambleCmdStream.ReserveCommands();
                pCmdSpace += cmdUtil.BuildWaitOnCeCounter(false, pCmdSpace);
                pCmdSpace += cmdUtil.BuildIncrementDeCounter(pCmdSpace);
                m_dePostambleCmdStream.CommitCommands(pCmdSpace);
            }
        }
    }
    // Otherwise, we just need the CE preamble to issue a dummy LOAD_CONST_RAM packet because the KMD requires each
    // UMD to have at least one load packet for high-priority 3D Queues (HP3D) to work. The Mantle client does not
    // need this because they do not use CE RAM for anything.
    else if (m_pDevice->SupportsCePreamblePerSubmit())
    {
        m_cePreambleCmdStream.Reset(nullptr, true);
        m_cePreambleCmdStream.Begin(beginFlags, nullptr);

        pCmdSpace  = m_cePreambleCmdStream.ReserveCommands();
        pCmdSpace += cmdUtil.BuildLoadConstRam(0, 0, 0, pCmdSpace);
        m_cePreambleCmdStream.CommitCommands(pCmdSpace);

        m_cePreambleCmdStream.End();
    }

    pCmdSpace = m_dePostambleCmdStream.ReserveCommands();

    // This EOP event packet must be at the end of the per-submit DE postamble.
    //
    // When the pipeline has emptied, write the timestamp back to zero so that the next submission can execute.
    // We also use this pipelined event to flush and invalidate the shader L2 cache and RB caches as described above.
    pCmdSpace += cmdUtil.BuildEventWriteEop(CACHE_FLUSH_AND_INV_TS_EVENT,
                                            m_timestampMem.GpuVirtAddr(),
                                            EVENTWRITEEOP_DATA_SEL_SEND_DATA32,
                                            0,
                                            true,
                                            pCmdSpace);

    m_dePostambleCmdStream.CommitCommands(pCmdSpace);
    m_dePostambleCmdStream.End();

    // Since the contents of the command stream have changed since last time, we need to force this stream to execute
    // by not allowing the KMD to optimize-away this command stream the next time around.
    m_deCmdStream.EnableDropIfSameContext(false);

    // The per-submit command stream, CE preamble and CE/DE postambles must always execute. We cannot allow KMD to
    // optimize-away these command streams.
    m_perSubmitCmdStream.EnableDropIfSameContext(false);
    m_cePreambleCmdStream.EnableDropIfSameContext(false);
    m_cePostambleCmdStream.EnableDropIfSameContext(false);
    m_dePostambleCmdStream.EnableDropIfSameContext(false);

    // If this assert is hit, CmdBufInternalSuballocSize should be increased.
    PAL_ASSERT((m_perSubmitCmdStream.GetNumChunks()   == 1) &&
               (m_deCmdStream.GetNumChunks()          == 1) &&
               (m_cePreambleCmdStream.GetNumChunks()  <= 1) &&
               (m_cePostambleCmdStream.GetNumChunks() <= 1) &&
               (m_dePostambleCmdStream.GetNumChunks() <= 1));
}

// =====================================================================================================================
// Assembles the universal-only specific PM4 headers for the queue context preamble.
void UniversalQueueContext::BuildUniversalPreambleHeaders()
{
    memset(&m_universalPreamble,     0, sizeof(m_universalPreamble));
    memset(&m_gfx6UniversalPreamble, 0, sizeof(m_gfx6UniversalPreamble));
    memset(&m_gfx8UniversalPreamble, 0, sizeof(m_gfx8UniversalPreamble));
    memset(&m_stateShadowPreamble,   0, sizeof(m_stateShadowPreamble));

    const CmdUtil& cmdUtil  = m_pDevice->CmdUtil();
    const auto&    settings = m_pDevice->Settings();

    m_universalPreamble.spaceNeeded += (sizeof(GdsRangeCompute) / sizeof(uint32));
#if !PAL_COMPUTE_GDS_OPT
    m_universalPreamble.spaceNeeded += GetGdsCounterRangeSize(false);
#endif

    // Additional preamble for Universal Command Buffers (Gfx6 hardware only):
    //==================================================================================================================

    if (m_pDevice->Parent()->ChipProperties().gfxLevel == GfxIpLevel::GfxIp6)
    {
        m_gfx6UniversalPreamble.spaceNeeded =
            cmdUtil.BuildSetOneConfigReg(mmSPI_STATIC_THREAD_MGMT_3__SI,
                                         &m_gfx6UniversalPreamble.hdrSpiThreadMgmt);
    }

    // Additional preamble for Universal Command Buffers (Gfx8 hardware only):
    //==================================================================================================================

    if (m_pDevice->Parent()->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp8)
    {
        // This packet is temporary because clear state should setup these registers. Unfortunately, on Gfx8, the clear
        // state values for these are suboptimal.
        m_gfx8UniversalPreamble.spaceNeeded =
            cmdUtil.BuildSetOneContextReg(mmVGT_OUT_DEALLOC_CNTL, &m_gfx8UniversalPreamble.hdrVgtOutDeallocCntl);

        m_gfx8UniversalPreamble.spaceNeeded +=
            cmdUtil.BuildSetOneContextReg(mmVGT_TESS_DISTRIBUTION__VI, &m_gfx8UniversalPreamble.hdrDistribution);

        m_gfx8UniversalPreamble.spaceNeeded +=
            cmdUtil.BuildSetOneContextReg(mmCB_DCC_CONTROL__VI, &m_gfx8UniversalPreamble.hdrDccControl);

        // Note that this register may not be present in non-Polaris10, but we choose to always write this register to
        // be simple. If we make it optional by doing check m_pDevice->Settings().gfx8SmallPrimFilterCntl !=
        // Gfx8SmallPrimFilterDisable, which is still fine but then this register has to be the last in preamble, which
        // can't be guaranteed moving forward. The register write to non-Polaris10 is expected to be ignored by HW.
        m_gfx8UniversalPreamble.spaceNeeded +=
            cmdUtil.BuildSetOneContextReg(mmPA_SU_SMALL_PRIM_FILTER_CNTL__VI,
                                          &m_gfx8UniversalPreamble.hdrSmallPrimFilterCntl);
    }

    // Additional preamble for state shadowing:
    // =================================================================================================================

    // Since PAL doesn't preserve GPU state across command buffer boundaries, we don't need to enable state shadowing,
    // but we do need to enable loading context and SH registers.

    CONTEXT_CONTROL_ENABLE shadowBits = {};
    shadowBits.enableDw = 1;

    CONTEXT_CONTROL_ENABLE loadBits = {};
    loadBits.enableDw                 = 1;
    loadBits.enableMultiCntxRenderReg = 1;
    loadBits.enableCSSHReg            = 1;
    loadBits.enableGfxSHReg           = 1;

    if (m_useShadowing)
    {
        PAL_ASSERT(m_pDevice->Parent()->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp8);

        // If mid command buffer preemption is enabled, shadowing and loading must be enabled for all register types,
        // because the GPU state needs to be properly restored when this Queue resumes execution after being preempted.
        // (Config registers are exempted because MCBP is not supported on pre-Gfx8 hardware.
        loadBits.enableUserConfigReg__CI     = 1;
        shadowBits                           = loadBits;
        shadowBits.enableSingleCntxConfigReg = 1;

        gpusize gpuVirtAddr = m_shadowGpuMem.GpuVirtAddr();

        m_stateShadowPreamble.spaceNeeded += cmdUtil.BuildLoadUserConfigRegs(gpuVirtAddr,
                                                                             &UserConfigShadowRangeGfx7[0],
                                                                             NumUserConfigShadowRangesGfx7,
                                                                             &m_stateShadowPreamble.loadUserCfgRegs);
        gpuVirtAddr += (sizeof(uint32) * UserConfigRegCount);

        if (m_pDevice->Parent()->ChipProperties().gfx6.rbReconfigureEnabled)
        {
            m_stateShadowPreamble.spaceNeeded += cmdUtil.BuildLoadContextRegs(gpuVirtAddr,
                                                                              &ContextShadowRangeRbReconfig[0],
                                                                              NumContextShadowRangesRbReconfig,
                                                                              &m_stateShadowPreamble.loadContextRegs);
        }
        else
        {
            m_stateShadowPreamble.spaceNeeded += cmdUtil.BuildLoadContextRegs(gpuVirtAddr,
                                                                              &ContextShadowRange[0],
                                                                              NumContextShadowRanges,
                                                                              &m_stateShadowPreamble.loadContextRegs);
        }
        gpuVirtAddr += (sizeof(uint32) * CntxRegCountGfx7);

        m_stateShadowPreamble.spaceNeeded += cmdUtil.BuildLoadShRegs(gpuVirtAddr,
                                                                     &GfxShShadowRange[0],
                                                                     NumGfxShShadowRanges,
                                                                     ShaderGraphics,
                                                                     &m_stateShadowPreamble.loadShRegsGfx);

        m_stateShadowPreamble.spaceNeeded += cmdUtil.BuildLoadShRegs(gpuVirtAddr,
                                                                     &CsShShadowRange[0],
                                                                     NumCsShShadowRanges,
                                                                     ShaderCompute,
                                                                     &m_stateShadowPreamble.loadShRegsCs);
        gpuVirtAddr += (sizeof(uint32) * ShRegCount);

    }

    m_stateShadowPreamble.spaceNeeded +=
        cmdUtil.BuildContextControl(loadBits, shadowBits, &m_stateShadowPreamble.contextControl);

    m_stateShadowPreamble.spaceNeeded += cmdUtil.BuildClearState(&m_stateShadowPreamble.clearState);
}

// =====================================================================================================================
// Sets up the universal-specific PM4 commands for the queue context preamble.
void UniversalQueueContext::SetupUniversalPreambleRegisters()
{
    const Gfx6PalSettings&   settings  = m_pDevice->Settings();
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

#if !PAL_COMPUTE_GDS_OPT
    BuildGdsRangeGraphics(m_pDevice, m_queueId, &m_universalPreamble.gdsRangeGraphics);
#endif
    BuildGdsRangeCompute(m_pDevice, m_pEngine->Type(), m_queueId, &m_universalPreamble.gdsRangeCompute);

    // Additional preamble for Universal Command Buffers (Gfx6 hardware only):
    //==================================================================================================================

    if (chipProps.gfxLevel == GfxIpLevel::GfxIp6)
    {
        // On Gfx6 hardware, there is a possible deadlock scenario between the LS/HS and PS waves: Because they both
        // use LDS resources, if all CU's are backed-up running LS/HS waves then the PS can be starved from running
        // because all of LDS is used by the LS/HS waves. This causes a deadlock because PS is required to run to drain
        // the pipeline of work generated by LS/HS wavefronts. The solution to this problem is to prevent the hardware
        // from scheduling LS/HS wavefronts on one CU per shader engine and shader array.

        // Need to find a bit-mask which has the active and always-on CU masks for all shader engines and shader arrays
        // combined.

        uint32 activeCuMask   = 0xFFFF;
        uint32 alwaysOnCuMask = 0xFFFF;

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
        Util::BitMaskScanForward(&cuIndex, alwaysOnCuMask);

        const uint32 lsHsCuMask = (activeCuMask & ~(0x1 << cuIndex));

        m_gfx6UniversalPreamble.spiStaticThreadMgmt3.u32All = 0;
        m_gfx6UniversalPreamble.spiStaticThreadMgmt3.bits.LSHS_CU_EN = lsHsCuMask;
    }

    // Additional preamble for Universal Command Buffers (Gfx8 hardware only):
    //==================================================================================================================

    if (chipProps.gfxLevel >= GfxIpLevel::GfxIp8)
    {
        m_gfx8UniversalPreamble.vgtOutDeallocCntl.u32All = 0;

        // The register spec suggests these values are optimal settings for Gfx8 hardware, when VS half-pack mode is
        // disabled. If half-pack mode is active, we need to use the legacy defaults which are safer (but less optimal).
        if (settings.vsHalfPackThreshold >= MaxVsExportSemantics)
        {
            m_gfx8UniversalPreamble.vgtOutDeallocCntl.bits.DEALLOC_DIST = 32;
        }
        else
        {
            m_gfx8UniversalPreamble.vgtOutDeallocCntl.bits.DEALLOC_DIST = 16;
        }

        // Set patch and donut distribution thresholds for tessellation. If we decide that this should be tunable
        // per-pipeline, we can move the registers to the Pipeline object (DXX currently uses per-Device thresholds).

        const uint32 patchDistribution     = settings.gfx8PatchDistributionFactor;
        const uint32 donutDistribution     = settings.gfx8DonutDistributionFactor;
        const uint32 trapezoidDistribution = settings.gfx8TrapezoidDistributionFactor;

        m_gfx8UniversalPreamble.vgtTessDistribution.u32All = 0;
        m_gfx8UniversalPreamble.vgtTessDistribution.bits.ACCUM_ISOLINE = patchDistribution;
        m_gfx8UniversalPreamble.vgtTessDistribution.bits.ACCUM_TRI     = patchDistribution;
        m_gfx8UniversalPreamble.vgtTessDistribution.bits.ACCUM_QUAD    = patchDistribution;
        m_gfx8UniversalPreamble.vgtTessDistribution.bits.DONUT_SPLIT   = donutDistribution;
        m_gfx8UniversalPreamble.vgtTessDistribution.bits.TRAP_SPLIT    = trapezoidDistribution;

        // Set-and-forget DCC register.
        m_gfx8UniversalPreamble.cbDccControl.bits.OVERWRITE_COMBINER_MRT_SHARING_DISABLE = 1;

        // Should default to 4 according to register spec
        m_gfx8UniversalPreamble.cbDccControl.bits.OVERWRITE_COMBINER_WATERMARK = 4;

        // Default enable DCC overwrite combiner
        m_gfx8UniversalPreamble.cbDccControl.bits.OVERWRITE_COMBINER_DISABLE = 0;

        // Polaris10 small primitive filter control
        const uint32 smallPrimFilter = m_pDevice->GetSmallPrimFilter();
        if (smallPrimFilter != SmallPrimFilterDisable)
        {
            m_gfx8UniversalPreamble.paSuSmallPrimFilterCntl.bits.SMALL_PRIM_FILTER_ENABLE = true;

            m_gfx8UniversalPreamble.paSuSmallPrimFilterCntl.bits.POINT_FILTER_DISABLE =
                ((smallPrimFilter & SmallPrimFilterEnablePoint) == 0);

            m_gfx8UniversalPreamble.paSuSmallPrimFilterCntl.bits.LINE_FILTER_DISABLE =
                ((smallPrimFilter & SmallPrimFilterEnableLine) == 0);

            m_gfx8UniversalPreamble.paSuSmallPrimFilterCntl.bits.TRIANGLE_FILTER_DISABLE =
                ((smallPrimFilter & SmallPrimFilterEnableTriangle) == 0);

            m_gfx8UniversalPreamble.paSuSmallPrimFilterCntl.bits.RECTANGLE_FILTER_DISABLE =
                ((smallPrimFilter & SmallPrimFilterEnableRectangle) == 0);
        }
        else
        {
            m_gfx8UniversalPreamble.paSuSmallPrimFilterCntl.bits.SMALL_PRIM_FILTER_ENABLE = false;
        }
    }
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

    constexpr uint32 NumGraphicsRegAddrs = sizeof(GraphicsRegAddrs) / sizeof(GraphicsRegAddrs[0]);

    constexpr uint32 ComputeRegAddrs[] =
    {
        mmCOMPUTE_TBA_LO
    };

    constexpr uint32 NumComputeRegAddrs = sizeof(ComputeRegAddrs) / sizeof(ComputeRegAddrs[0]);

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

            pCmdSpace = pCmdStream->WriteSetSeqShRegs(pRegAddrs[i], pRegAddrs[i] + 3, shaderType, &regVals[0], pCmdSpace);
        }
    }

    return pCmdSpace;
}

} // Gfx6
} // Pal
