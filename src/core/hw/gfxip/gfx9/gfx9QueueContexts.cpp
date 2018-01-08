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
#include "palAssert.h"
#include "core/hw/gfxip/gfx9/gfx9ComputeEngine.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9Preambles.h"
#include "core/hw/gfxip/gfx9/gfx9QueueContexts.h"
#include "core/hw/gfxip/gfx9/gfx9ShaderRingSet.h"
#include "core/hw/gfxip/gfx9/gfx9UniversalEngine.h"
#include "core/hw/gfxip/gfx9/g_gfx9ShadowedRegistersInit.h"

#include <limits.h>

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
// Assembles and initializes the PM4 commands for the common preamble image.
static void SetupCommonPreamble(
    const Device*         pDevice,
    EngineType            engineType,
    CommonPreamblePm4Img* pCommonPreamble)
{
    memset(pCommonPreamble, 0, sizeof(CommonPreamblePm4Img));

    const CmdUtil& cmdUtil  = pDevice->CmdUtil();
    const auto&    settings = pDevice->Settings();

    // First build the PM4 headers.
    if (pDevice->Parent()->EngineSupportsCompute(engineType))
    {
        pCommonPreamble->spaceNeeded +=
            cmdUtil.BuildSetSeqShRegsIndex(mmCOMPUTE_STATIC_THREAD_MGMT_SE0,
                                           mmCOMPUTE_STATIC_THREAD_MGMT_SE1,
                                           ShaderCompute,
                                           index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                           &pCommonPreamble->hdrThreadMgmt01);

        pCommonPreamble->spaceNeeded +=
            cmdUtil.BuildSetSeqShRegsIndex(mmCOMPUTE_STATIC_THREAD_MGMT_SE2,
                                           mmCOMPUTE_STATIC_THREAD_MGMT_SE3,
                                           ShaderCompute,
                                           index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                           &pCommonPreamble->hdrThreadMgmt23);

        // It's OK to set the CU mask to enable all CUs. The UMD does not need to know about active CUs and harvested
        // CUs at this point. Using the packet SET_SH_REG_INDEX, the umd mask will be ANDed with the kmd mask so that
        // UMD does not use the CUs that are intended for real time compute usage.

        // Enable Compute workloads on all CU's of SE0/SE1.
        pCommonPreamble->computeStaticThreadMgmtSe0.bits.SH0_CU_EN = 0xFFFF;
        pCommonPreamble->computeStaticThreadMgmtSe0.bits.SH1_CU_EN = 0xFFFF;
        pCommonPreamble->computeStaticThreadMgmtSe1.bits.SH0_CU_EN = 0xFFFF;
        pCommonPreamble->computeStaticThreadMgmtSe1.bits.SH1_CU_EN = 0xFFFF;

        // Enable Compute workloads on all CU's of SE2/SE3.
        pCommonPreamble->computeStaticThreadMgmtSe2.bits.SH0_CU_EN = 0xFFFF;
        pCommonPreamble->computeStaticThreadMgmtSe2.bits.SH1_CU_EN = 0xFFFF;
        pCommonPreamble->computeStaticThreadMgmtSe3.bits.SH0_CU_EN = 0xFFFF;
        pCommonPreamble->computeStaticThreadMgmtSe3.bits.SH1_CU_EN = 0xFFFF;
    }

    pCommonPreamble->spaceNeeded +=
        cmdUtil.BuildSetOneConfigReg(mmCP_COHER_START_DELAY, &pCommonPreamble->hdrCoherDelay);

    // Now set up the values for the registers being written.

    // Give the CP_COHER register (used by acquire-mem packet) a chance to think a little bit before actually
    // doing anything.
    const GfxIpLevel gfxLevel = pDevice->Parent()->ChipProperties().gfxLevel;

    if (gfxLevel == GfxIpLevel::GfxIp9)
    {
        pCommonPreamble->cpCoherStartDelay.bits.START_DELAY_COUNT = 0;
    }
}

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

    // The register for SC work off of a zero-based address on Gfx9.
    gdsInfo.offset = 0;

    if ((engineType == EngineTypeUniversal) && pDevice->Parent()->PerPipelineBindPointGds())
    {
        // If per-pipeline bind point GDS partitions were requested then on the universal queue the GDS partition of the
        // engine is split into two so we have to adjust the size.
        gdsInfo.size /= 2;
    }

    pDevice->CmdUtil().BuildSetOneShReg(mmCOMPUTE_USER_DATA_0 + GdsRangeRegCompute, ShaderCompute, &pGdsRange->header);
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
    m_pDevice(pDevice),
    m_pQueue(pQueue),
    m_pEngine(static_cast<ComputeEngine*>(pEngine)),
    m_queueId(queueId),
    m_currentUpdateCounter(0),
    m_cmdStream(*pDevice,
                pDevice->Parent()->InternalUntrackedCmdAllocator(),
                EngineTypeCompute,
                SubQueueType::Primary,
                false,
                true),              // Preambles cannot be preemptible.
    m_perSubmitCmdStream(*pDevice,
                         pDevice->Parent()->InternalUntrackedCmdAllocator(),
                         EngineTypeCompute,
                         SubQueueType::Primary,
                         false,
                         true)      // Preambles cannot be preemptible.
{
    SetupCommonPreamble(pDevice, pEngine->Type(), &m_commonPreamble);
    BuildComputePreambleHeaders();
    SetupComputePreambleRegisters();
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
        RebuildCommandStreams();
    }

    return result;
}

// =====================================================================================================================
// Checks if any new Pipelines the client has created require that the compute scratch ring needs to expand. If so, the
// the compute shader rings are re-validated and our context command stream is rebuilt.
Result ComputeQueueContext::PreProcessSubmit(
    InternalSubmitInfo* pSubmitInfo,
    const SubmitInfo&   submitInfo)
{
    bool   hasUpdated = false;
    Result result     = m_pEngine->UpdateRingSet(&m_currentUpdateCounter, &hasUpdated);

    if ((result == Result::Success) && hasUpdated)
    {
        RebuildCommandStreams();
    }

    pSubmitInfo->pPreambleCmdStream[0] = &m_perSubmitCmdStream;
    pSubmitInfo->pPreambleCmdStream[1] = &m_cmdStream;

    pSubmitInfo->numPreambleCmdStreams  = 2;
    pSubmitInfo->numPostambleCmdStreams = 0;

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
void ComputeQueueContext::RebuildCommandStreams()
{
    constexpr CmdStreamBeginFlags beginFlags = {};

    m_cmdStream.Reset(nullptr, true);
    m_cmdStream.Begin(beginFlags, nullptr);

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    // Write the shader ring-set's commands before the command stream's normal preamble. If the ring sizes have changed,
    // the hardware requires a CS partial flush to operate properly.
    pCmdSpace = m_pEngine->RingSet()->WriteCommands(&m_cmdStream, pCmdSpace);

    pCmdSpace += m_pDevice->CmdUtil().BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, EngineTypeCompute, pCmdSpace);

    // Copy the common preamble commands and compute-specific preamble commands.
    pCmdSpace = m_cmdStream.WritePm4Image(m_commonPreamble.spaceNeeded,  &m_commonPreamble,  pCmdSpace);
    pCmdSpace = m_cmdStream.WritePm4Image(m_computePreamble.spaceNeeded, &m_computePreamble, pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
    m_cmdStream.End();

    m_perSubmitCmdStream.Reset(nullptr, true);
    m_perSubmitCmdStream.Begin(beginFlags, nullptr);

    pCmdSpace = m_perSubmitCmdStream.ReserveCommands();
    pCmdSpace = m_perSubmitCmdStream.WritePm4Image(m_perSubmitPreamble.spaceNeeded, &m_perSubmitPreamble, pCmdSpace);

    m_perSubmitCmdStream.CommitCommands(pCmdSpace);
    m_perSubmitCmdStream.End();

    // If this assert is hit, CmdBufInternalSuballocSize should be increased.
    PAL_ASSERT((m_cmdStream.GetNumChunks() == 1) && (m_perSubmitCmdStream.GetNumChunks() == 1));

    // Since the contents of the command stream have changed since last time, we need to force this stream to execute
    // by not allowing the KMD to optimize-away this command stream the next time around.
    m_cmdStream.EnableDropIfSameContext(false);

    // The per-submit command stream must always execute. We cannot allow KMD to optimize-away this command stream.
    m_perSubmitCmdStream.EnableDropIfSameContext(false);
}

// =====================================================================================================================
// Assembles the compute-only specific PM4 headers for the queue context preamble.
void ComputeQueueContext::BuildComputePreambleHeaders()
{
    memset(&m_computePreamble, 0, sizeof(m_computePreamble));
    memset(&m_perSubmitPreamble, 0, sizeof(m_perSubmitPreamble));

    m_computePreamble.spaceNeeded += (sizeof(GdsRangeCompute) / sizeof(uint32));

    const CmdUtil& cmdUtil = m_pDevice->CmdUtil();

    // Issue an acquire mem packet to invalidate all L1 caches (TCP, SQ I-cache, SQ K-cache).  KMD automatically
    // flushes all write caches with an EOP event at the conclusion of each user mode submission, including the
    // shader L2 cache (TCC), but the L1 shader caches (SQC/TCC) are not invalidated.  An application is responsible
    // for waiting for all previous work to be complete before reusing a memory object, which thanks to KMD, ensures
    // all L2 reads/writes are flushed and invalidated.  However, a well behaving app could read stale L1 data
    // if it writes to mapped memory using the CPU unless we invalidate the L1 caches here.
    AcquireMemInfo acquireInfo = {};
    acquireInfo.flags.invSqI$ = 1;
    acquireInfo.flags.invSqK$ = 1;
    acquireInfo.tcCacheOp     = TcCacheOp::InvL1;
    acquireInfo.engineType    = EngineTypeCompute;
    acquireInfo.baseAddress   = FullSyncBaseAddr;
    acquireInfo.sizeBytes     = FullSyncSize;

    m_perSubmitPreamble.spaceNeeded += cmdUtil.BuildAcquireMem(acquireInfo, &m_perSubmitPreamble.acquireMem);
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
    m_pDevice(pDevice),
    m_pQueue(pQueue),
    m_pEngine(static_cast<UniversalEngine*>(pEngine)),
    m_queueId(queueId),
    m_currentUpdateCounter(0),
    m_useShadowing((Device::ForceStateShadowing &&
                    pDevice->Parent()->ChipProperties().gfx9.supportLoadRegIndexPkt) ||
                   m_pQueue->IsPreemptionSupported()),
    m_shadowGpuMemSizeInBytes(0),
    m_shadowedRegCount(0),
    m_submitCounter(0),
    m_deCmdStream(*pDevice,
                  pDevice->Parent()->InternalUntrackedCmdAllocator(),
                  pEngine->Type(),
                  SubQueueType::Primary,
                  false,
                  true),                // Preambles cannot be preemptible.
    m_perSubmitCmdStream(*pDevice,
                         pDevice->Parent()->InternalUntrackedCmdAllocator(),
                         pEngine->Type(),
                         SubQueueType::Primary,
                         false,
                         true),         // Preambles cannot be preemptible.
    m_cePreambleCmdStream(*pDevice,
                          pDevice->Parent()->InternalUntrackedCmdAllocator(),
                          pEngine->Type(),
                          SubQueueType::ConstantEnginePreamble,
                          false,
                          true),        // Preambles cannot be preemptible.
    m_cePostambleCmdStream(*pDevice,
                           pDevice->Parent()->InternalUntrackedCmdAllocator(),
                           pEngine->Type(),
                           SubQueueType::ConstantEngine,
                           false,
                           true),       // Postambles cannot be preemptible.
    m_dePostambleCmdStream(*pDevice,
                           pDevice->Parent()->InternalUntrackedCmdAllocator(),
                           pEngine->Type(),
                           SubQueueType::Primary,
                           false,
                           true)        // Postambles cannot be preemptible.
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
        result = AllocateShadowMemory();
    }

    if (result == Result::Success)
    {
        SetupCommonPreamble(m_pDevice, m_pEngine->Type(), &m_commonPreamble);
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
        m_shadowedRegCount = (ShRegCount + CntxRegCount + UserConfigRegCount);

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
// require a bigger scratch ring, or due the client binding a new trap handler/buffer.  If so, the the compute shader
// rings are re-validated and our context command stream is rebuilt.
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

    uint32 postambleCount = 0;
    if (m_cePostambleCmdStream.IsEmpty() == false)
    {
        pSubmitInfo->pPostambleCmdStream[postambleCount] = &m_cePostambleCmdStream;
        ++postambleCount;
    }
    if (m_dePostambleCmdStream.IsEmpty() == false)
    {
        pSubmitInfo->pPostambleCmdStream[postambleCount] = &m_dePostambleCmdStream;
        ++postambleCount;
    }

    pSubmitInfo->pPreambleCmdStream[preambleCount] = &m_perSubmitCmdStream;
    ++preambleCount;
    pSubmitInfo->pPreambleCmdStream[preambleCount] = &m_deCmdStream;
    ++preambleCount;

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
    constexpr CmdStreamBeginFlags beginFlags = {};

    m_deCmdStream.Reset(nullptr, true);
    m_deCmdStream.Begin(beginFlags, nullptr);

    const CmdUtil& cmdUtil   = m_pDevice->CmdUtil();
    uint32*        pCmdSpace = m_deCmdStream.ReserveCommands();

    // Copy the common preamble commands and the universal-specific preamble commands.
    pCmdSpace = m_deCmdStream.WritePm4Image(m_universalPreamble.spaceNeeded, &m_universalPreamble, pCmdSpace);
    pCmdSpace = m_deCmdStream.WritePm4Image(m_commonPreamble.spaceNeeded,    &m_commonPreamble,    pCmdSpace);

    // Write the shader ring-set's commands after the command stream's normal preamble. If the ring sizes have changed,
    // the hardware requires a CS/VS/PS partial flush to operate properly.
    pCmdSpace = m_pEngine->RingSet()->WriteCommands(&m_deCmdStream, pCmdSpace);
    pCmdSpace += cmdUtil.BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, EngineTypeUniversal, pCmdSpace);
    pCmdSpace += cmdUtil.BuildNonSampleEventWrite(VS_PARTIAL_FLUSH, EngineTypeUniversal, pCmdSpace);
    pCmdSpace += cmdUtil.BuildNonSampleEventWrite(PS_PARTIAL_FLUSH, EngineTypeUniversal, pCmdSpace);

    m_deCmdStream.CommitCommands(pCmdSpace);
    m_deCmdStream.End();

    // Rebuild the command stream for per-submit:

    m_perSubmitCmdStream.Reset(nullptr, true);
    m_perSubmitCmdStream.Begin(beginFlags, nullptr);

    pCmdSpace = m_perSubmitCmdStream.ReserveCommands();

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
        // the commands written by InitializeContextRegistersGfx*() with a COND_EXEC packet that can skip the
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
        // 4. SETs:       Commands written by InitializeContextRegistersGfx*(). DMA_DATA packet is used before SETs
        //                to initialize the shadow memory to 0, also needs to be done once.
        //
        // 5. WRITE_DATA: Updates the size field of the COND_EXEC to a larger value so that the COND_EXEC will now
        //                skip the NOP, SETs, and WRITE_DATA, as soon as the GPU has executed the
        //                InitializeContextRegistersGfx*() commands once.
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
                                        CmdUtil::CondExecSizeDwords * sizeof(uint32);
        const gpusize skipFlagGpuAddr = nopStartGpuAddr + CmdUtil::MinNopSizeInDwords * sizeof(uint32);

        // We'll update the size later.
        const gpusize skipSizeGpuAddr = nopStartGpuAddr - sizeof(uint32);

        pCmdSpace = m_perSubmitCmdStream.ReserveCommands();
        // We only skip the NOP for the first time.
        pCmdSpace += cmdUtil.BuildCondExec(skipFlagGpuAddr, CmdUtil::MinNopSizeInDwords + 1, pCmdSpace);
        pCmdSpace += cmdUtil.BuildNop(CmdUtil::MinNopSizeInDwords + 1, pCmdSpace);
        // Cond_Exec will always skip.
        *(pCmdSpace - 1) = 0;

        // Use a DMA_DATA packet to initialize all shadow memory to 0s explicitely.
        DmaDataInfo dmaData  = {};
        dmaData.dstSel       = dst_sel__pfp_dma_data__dst_addr_using_l2;
        dmaData.dstAddr      = m_shadowGpuMem.GpuVirtAddr();
        dmaData.dstAddrSpace = das__pfp_dma_data__memory;
        dmaData.srcSel       = src_sel__pfp_dma_data__data;
        dmaData.srcData      = 0;
        dmaData.numBytes     = static_cast<uint32>(m_shadowGpuMemSizeInBytes);
        dmaData.sync         = true;
        dmaData.usePfp       = true;
        pCmdSpace += cmdUtil.BuildDmaData(dmaData, pCmdSpace);

        // After initializing shadow memory to 0, load user config and sh register again, otherwise the registers
        // might contain invalid value. We don't need to load context register again because in the
        // InitializeContextRegisters() we will set the contexts that we can load.
        const uint32 RegRangeDwordSize = sizeof(RegisterRange) / sizeof(uint32);
        gpusize     gpuVirtAddr = m_shadowGpuMem.GpuVirtAddr();
        uint32      numEntries  = 0;
        const auto* pRegRange   = m_pDevice->GetRegisterRange(RegRangeUserConfig, &numEntries);
        pCmdSpace += cmdUtil.BuildLoadUserConfigRegs(gpuVirtAddr,
                                                     pRegRange,
                                                     numEntries,
                                                     MaxNumUserConfigRanges,
                                                     pCmdSpace);
        gpuVirtAddr += (sizeof(uint32) * UserConfigRegCount);

        gpuVirtAddr += (sizeof(uint32) * CntxRegCount);

        pRegRange = m_pDevice->GetRegisterRange(RegRangeSh, &numEntries);
        pCmdSpace += cmdUtil.BuildLoadShRegs(gpuVirtAddr,
                                             pRegRange,
                                             numEntries,
                                             MaxNumShRanges,
                                             ShaderGraphics,
                                             pCmdSpace);

        pRegRange = m_pDevice->GetRegisterRange(RegRangeCsSh, &numEntries);
        pCmdSpace += cmdUtil.BuildLoadShRegs(gpuVirtAddr,
                                             pRegRange,
                                             numEntries,
                                             MaxNumCsShRanges,
                                             ShaderCompute,
                                             pCmdSpace);
        gpuVirtAddr += (sizeof(uint32) * ShRegCount);

        m_perSubmitCmdStream.CommitCommands(pCmdSpace);
        // We do this after m_stateShadowPreamble, when the LOADs are done and HW knows the shadow memory.
        // First LOADs will load garbage. InitializeContextRegisters will init the register and also the shadow Memory.
        const auto& chipProps = m_pDevice->Parent()->ChipProperties();
        if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
        {
            InitializeContextRegistersGfx9(&m_perSubmitCmdStream);
        }
        else
        {
            PAL_NOT_IMPLEMENTED();
        }

        const gpusize endOfSetsGpuAddr = m_perSubmitCmdStream.GetCurrentGpuVa();
        // Skip the NOP, DMA_DATA, all the SETs + writeData header size and 1 dword it writes.
        const uint32  condSizeDw       = static_cast<uint32>(endOfSetsGpuAddr - nopStartGpuAddr) / sizeof(uint32)
                                         + CmdUtil::WriteDataSizeDwords + 1;

        pCmdSpace = m_perSubmitCmdStream.ReserveCommands();
        pCmdSpace += cmdUtil.BuildWriteData(EngineTypeUniversal,
                                            skipSizeGpuAddr,
                                            1,
                                            engine_sel__pfp_write_data__prefetch_parser,
                                            dst_sel__pfp_write_data__memory,
                                            wr_confirm__pfp_write_data__wait_for_write_confirmation,
                                            &condSizeDw,
                                            PredDisable,
                                            pCmdSpace);

        const uint32 chunkIndexEnd = m_perSubmitCmdStream.GetNumChunks();
        // We assume all the SET packets will fit in one chunk. So we only build one skip logic. If the sets are in
        // different chunks, the code is broken and we need to modify it.
        PAL_ASSERT(chunkIndexBegin == chunkIndexEnd);
    }

    pCmdSpace = m_perSubmitCmdStream.WritePm4Image(m_perSubmitPreamble.spaceNeeded,
                                                   &m_perSubmitPreamble,
                                                   pCmdSpace);

    m_perSubmitCmdStream.CommitCommands(pCmdSpace);
    m_perSubmitCmdStream.End();

    // If the client has requested that this Queue maintain persistent CE RAM contents, we need to rebuild the CE
    // preamble, as well as the CE & DE postambles.
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

        // The postamble command streams which dump CE RAM at the end of the submission and synchronize the CE/DE
        // counters are only necessary if (1) the client requested that this Queue maintains persistent CE RAM
        // contents, or (2) this Queue supports mid command buffer preemption and the panel setting to force the
        // dump CE RAM postamble is set.
        if ((m_pQueue->PersistentCeRamSize() != 0) ||
            (m_pDevice->Settings().commandBufferForceCeRamDumpInPostamble != false))
        {
            m_cePostambleCmdStream.Reset(nullptr, true);
            m_cePostambleCmdStream.Begin(beginFlags, nullptr);

            pCmdSpace  = m_cePostambleCmdStream.ReserveCommands();
            pCmdSpace += cmdUtil.BuildDumpConstRam(gpuVirtAddr, ceRamByteOffset, ceRamDwordSize, pCmdSpace);
            pCmdSpace += cmdUtil.BuildIncrementCeCounter(pCmdSpace);
            m_cePostambleCmdStream.CommitCommands(pCmdSpace);

            m_cePostambleCmdStream.End();

            m_dePostambleCmdStream.Reset(nullptr, true);
            m_dePostambleCmdStream.Begin(beginFlags, nullptr);

            pCmdSpace  = m_dePostambleCmdStream.ReserveCommands();
            pCmdSpace += cmdUtil.BuildWaitOnCeCounter(false, pCmdSpace);
            pCmdSpace += cmdUtil.BuildIncrementDeCounter(pCmdSpace);
            m_dePostambleCmdStream.CommitCommands(pCmdSpace);

            m_dePostambleCmdStream.End();
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
    memset(&m_universalPreamble,   0, sizeof(m_universalPreamble));
    memset(&m_perSubmitPreamble,   0, sizeof(m_perSubmitPreamble));
    memset(&m_stateShadowPreamble, 0, sizeof(m_stateShadowPreamble));

    const CmdUtil& cmdUtil  = m_pDevice->CmdUtil();
    const auto&    settings = m_pDevice->Settings();

    PM4PFP_CONTEXT_CONTROL contextControl = m_pDevice->GetContextControl();

    if (m_useShadowing)
    {
        gpusize gpuVirtAddr = m_shadowGpuMem.GpuVirtAddr();

        uint32       numEntries = 0;
        const auto*  pRegRange  = m_pDevice->GetRegisterRange(RegRangeUserConfig, &numEntries);
        m_stateShadowPreamble.spaceNeeded += cmdUtil.BuildLoadUserConfigRegs(gpuVirtAddr,
                                                                             pRegRange,
                                                                             numEntries,
                                                                             MaxNumUserConfigRanges,
                                                                             &m_stateShadowPreamble.loadUserCfgRegs);
        gpuVirtAddr += (sizeof(uint32) * UserConfigRegCount);

        pRegRange = m_pDevice->GetRegisterRange(RegRangeContext, &numEntries);
        m_stateShadowPreamble.spaceNeeded += cmdUtil.BuildLoadContextRegs(gpuVirtAddr,
                                                                          pRegRange,
                                                                          numEntries,
                                                                          &m_stateShadowPreamble.loadContextRegs);
        gpuVirtAddr += (sizeof(uint32) * CntxRegCount);

        pRegRange = m_pDevice->GetRegisterRange(RegRangeSh, &numEntries);
        m_stateShadowPreamble.spaceNeeded += cmdUtil.BuildLoadShRegs(gpuVirtAddr,
                                                                     pRegRange,
                                                                     numEntries,
                                                                     MaxNumShRanges,
                                                                     ShaderGraphics,
                                                                     &m_stateShadowPreamble.loadShRegsGfx);

        pRegRange = m_pDevice->GetRegisterRange(RegRangeCsSh, &numEntries);
        m_stateShadowPreamble.spaceNeeded += cmdUtil.BuildLoadShRegs(gpuVirtAddr,
                                                                     pRegRange,
                                                                     numEntries,
                                                                     MaxNumCsShRanges,
                                                                     ShaderCompute,
                                                                     &m_stateShadowPreamble.loadShRegsCs);
        gpuVirtAddr += (sizeof(uint32) * ShRegCount);
    }

    m_stateShadowPreamble.spaceNeeded +=
        cmdUtil.BuildContextControl(contextControl, &m_stateShadowPreamble.contextControl);

    m_stateShadowPreamble.spaceNeeded += cmdUtil.BuildClearState(cmd__pfp_clear_state__clear_state,
                                                                 &m_stateShadowPreamble.clearState);

    m_universalPreamble.spaceNeeded += (sizeof(GdsRangeCompute) / sizeof(uint32));

    // Occlusion query control event, specifies that we want one counter to dump out every 128 bits for every
    // DB that the HW supports.
    //
    // NOTE: Despite the structure definition in the HW doc, the instance_enable variable is 16 bits long, not 8.
    union PixelPipeStatControl
    {
        struct
        {
            uint32 reserved1       :  3;
            uint32 counter_id      :  6;    // Mask of which counts to dump
            uint32 stride          :  2;    // PixelPipeStride enum
                                            // (how far apart each enabled instance must dump from each other)
            uint32 instance_enable : 16;    // Mask of which of the RBs must dump the data.
            uint32 reserved2       :  5;
        } bits;

        uint32 u32All;
    };

    PixelPipeStatControl pixelPipeStatControl = {};

    // Our occlusion query data is in pairs of [begin, end], each pair being 128 bits.
    // To emulate the deprecated ZPASS_DONE, we specify COUNT_0, a stride of 128 bits, and all RBs enabled.
    pixelPipeStatControl.bits.counter_id      = PIXEL_PIPE_OCCLUSION_COUNT_0;
    pixelPipeStatControl.bits.stride          = PIXEL_PIPE_STRIDE_128_BITS;

    const auto& chipProps     = m_pDevice->Parent()->ChipProperties();
    const auto& gfx9ChipProps = chipProps.gfx9;

    pixelPipeStatControl.bits.instance_enable = ~(gfx9ChipProps.backendDisableMask) &
                                                ((1 << gfx9ChipProps.numTotalRbs) - 1);

    m_universalPreamble.spaceNeeded +=
        cmdUtil.BuildSampleEventWrite(PIXEL_PIPE_STAT_CONTROL,
                                      EngineTypeUniversal,
                                      pixelPipeStatControl.u32All,
                                      &m_universalPreamble.pixelPipeStatControl);

    if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
    {
        m_universalPreamble.spaceNeeded +=
            cmdUtil.BuildSetSeqConfigRegs(mmVGT_MAX_VTX_INDX__GFX09,
                                          mmVGT_INDX_OFFSET__GFX09,
                                          &m_universalPreamble.vgtIndexRegs.gfx9.hdrVgtIndexRegs);
    }

    // TODO: The following are set on Gfx8 because the clear state doesn't set up these registers to our liking.
    //       We might be able to remove these when the clear state for Gfx9 is finalized.
    m_universalPreamble.spaceNeeded +=
        cmdUtil.BuildSetOneContextReg(mmVGT_OUT_DEALLOC_CNTL, &m_universalPreamble.hdrVgtOutDeallocCntl);

    m_universalPreamble.spaceNeeded +=
        cmdUtil.BuildSetOneContextReg(mmVGT_TESS_DISTRIBUTION, &m_universalPreamble.hdrVgtTessDistribution);

    m_universalPreamble.spaceNeeded +=
        cmdUtil.BuildSetOneContextReg(mmCB_DCC_CONTROL, &m_universalPreamble.hdrDccControl);

    m_universalPreamble.spaceNeeded +=
        cmdUtil.BuildSetOneContextReg(mmPA_SU_SMALL_PRIM_FILTER_CNTL, &m_universalPreamble.hdrSmallPrimFilterCntl);

    // Additional preamble for Universal Queue Preambles (per-submit):
    // =================================================================================================================

    // Issue an acquire mem packet to invalidate all L1 caches (TCP, SQ I-cache, SQ K-cache). KMD automatically
    // flushes all write caches with an EOP event at the conclusion of each user mode submission, including the shader
    // L2 cache (TCC), but the L1 shader caches (SQC/TCC) are not invalidated.  An application is responsible for
    // waiting for all previous work to be complete before reusing a memory object, which thanks to KMD, ensures all L2
    // reads/writes are flushed and invalidated.  However, a well behaving app could read stale L1 data if it writes to
    // mapped memory using the CPU unless we invalidate the L1 caches here.
    AcquireMemInfo acquireInfo = {};
    acquireInfo.flags.invSqI$ = 1;
    acquireInfo.flags.invSqK$ = 1;
    acquireInfo.tcCacheOp     = TcCacheOp::InvL1;
    acquireInfo.engineType    = EngineTypeUniversal;
    acquireInfo.baseAddress   = FullSyncBaseAddr;
    acquireInfo.sizeBytes     = FullSyncSize;

    m_perSubmitPreamble.spaceNeeded += cmdUtil.BuildAcquireMem(acquireInfo, &m_perSubmitPreamble.acquireMem);
}

// =====================================================================================================================
// Sets up the universal-specific PM4 commands for the queue context preamble.
void UniversalQueueContext::SetupUniversalPreambleRegisters()
{
    const Gfx9PalSettings& settings = m_pDevice->Settings();
    const GfxIpLevel       gfxLevel = m_pDevice->Parent()->ChipProperties().gfxLevel;

    BuildGdsRangeCompute(m_pDevice, EngineTypeUniversal, m_queueId, &m_universalPreamble.gdsRangeCompute);

    // TODO: Add support for Late Alloc VS Limit

    m_universalPreamble.vgtOutDeallocCntl.u32All = 0;

    // The register spec suggests these values are optimal settings for Gfx9 hardware, when VS half-pack mode is
    // disabled. If half-pack mode is active, we need to use the legacy defaults which are safer (but less optimal).
    if (settings.vsHalfPackThreshold >= MaxVsExportSemantics)
    {
        m_universalPreamble.vgtOutDeallocCntl.bits.DEALLOC_DIST = 32;
    }
    else
    {
        m_universalPreamble.vgtOutDeallocCntl.bits.DEALLOC_DIST = 16;
    }

    // Set patch and donut distribution thresholds for tessellation. If we decide that this should be tunable
    // per-pipeline, we can move the registers to the Pipeline object (DXX currently uses per-Device thresholds).

    const uint32 isolineDistribution   = settings.isolineDistributionFactor;
    const uint32 triDistribution       = settings.triDistributionFactor;
    const uint32 quadDistribution      = settings.quadDistributionFactor;
    const uint32 donutDistribution     = settings.donutDistributionFactor;
    const uint32 trapezoidDistribution = settings.trapezoidDistributionFactor;

    m_universalPreamble.vgtTessDistribution.u32All             = 0;
    m_universalPreamble.vgtTessDistribution.bits.ACCUM_ISOLINE = isolineDistribution;
    m_universalPreamble.vgtTessDistribution.bits.ACCUM_TRI     = triDistribution;
    m_universalPreamble.vgtTessDistribution.bits.ACCUM_QUAD    = quadDistribution;
    m_universalPreamble.vgtTessDistribution.bits.DONUT_SPLIT   = donutDistribution;
    m_universalPreamble.vgtTessDistribution.bits.TRAP_SPLIT    = trapezoidDistribution;

    if (gfxLevel == GfxIpLevel::GfxIp9)
    {
        m_universalPreamble.vgtIndexRegs.gfx9.vgtMaxVtxIndx.bits.MAX_INDX    = 0xFFFFFFFF;
        m_universalPreamble.vgtIndexRegs.gfx9.vgtMinVtxIndx.bits.MIN_INDX    = 0;
        m_universalPreamble.vgtIndexRegs.gfx9.vgtIndxOffset.bits.INDX_OFFSET = 0;
    }

    // Set-and-forget DCC register:
    m_universalPreamble.cbDccControl.bits.OVERWRITE_COMBINER_MRT_SHARING_DISABLE__GFX09 = 1;

    //     Should default to 4 according to register spec
    m_universalPreamble.cbDccControl.bits.OVERWRITE_COMBINER_WATERMARK = 4;

    //     Default enable DCC overwrite combiner
    m_universalPreamble.cbDccControl.bits.OVERWRITE_COMBINER_DISABLE = 0;

    // Small primitive filter control
    const uint32 smallPrimFilter = m_pDevice->GetSmallPrimFilter();
    if (smallPrimFilter != SmallPrimFilterDisable)
    {
        m_universalPreamble.paSuSmallPrimFilterCntl.bits.SMALL_PRIM_FILTER_ENABLE = 1;

        m_universalPreamble.paSuSmallPrimFilterCntl.bits.POINT_FILTER_DISABLE =
            ((smallPrimFilter & SmallPrimFilterEnablePoint) == 0);

        m_universalPreamble.paSuSmallPrimFilterCntl.bits.LINE_FILTER_DISABLE =
            ((smallPrimFilter & SmallPrimFilterEnableLine) == 0);

        m_universalPreamble.paSuSmallPrimFilterCntl.bits.TRIANGLE_FILTER_DISABLE =
            ((smallPrimFilter & SmallPrimFilterEnableTriangle) == 0);

        m_universalPreamble.paSuSmallPrimFilterCntl.bits.RECTANGLE_FILTER_DISABLE =
            ((smallPrimFilter & SmallPrimFilterEnableRectangle) == 0);
    }
    else
    {
        m_universalPreamble.paSuSmallPrimFilterCntl.bits.SMALL_PRIM_FILTER_ENABLE = 0;
    }
}

} // Gfx9
} // Pal
