/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/gpuMemory.h"
#include "core/queue.h"
#include "core/perfExperiment.h"
#include "core/hw/gfxip/borderColorPalette.h"
#include "core/hw/gfxip/computePipeline.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/pipeline.h"
#include "core/hw/gfxip/pm4CmdBuffer.h"
#include "palAutoBuffer.h"
#include "palDequeImpl.h"
#include "palHsaAbiMetadata.h"
#include "palFormatInfo.h"
#include "palHashMapImpl.h"
#include "palImage.h"
#include "palIntrusiveListImpl.h"
#include "palIterator.h"
#include "palQueryPool.h"
#include "palVectorImpl.h"

#include <limits.h>

using namespace Util;

namespace Pal
{

// =====================================================================================================================
Pm4CmdBuffer::Pm4CmdBuffer(
    const GfxDevice&           device,
    const CmdBufferCreateInfo& createInfo)
    :
    GfxCmdBuffer(device, createInfo),
    m_acqRelFenceValGpuVa(0),
    m_fceRefCountVec(device.GetPlatform()),
    m_pm4CmdBufState{},
    m_device(device)
{
    for (uint32 i = 0; i < static_cast<uint32>(QueryPoolType::Count); i++)
    {
        // Marks the specific query as "active," as in it is available to be used.
        // When we need to push state, the queries are no longer active (we deactivate them), but we want to reactivate
        // all of them after we pop state.
        m_queriesActive[i]    = true;
        m_numActiveQueries[i] = 0;
    }

    memset(m_acqRelFenceVals, 0, sizeof(m_acqRelFenceVals));
}

// =====================================================================================================================
Pm4CmdBuffer::~Pm4CmdBuffer()
{
    ResetFastClearReferenceCounts();
}

// =====================================================================================================================
// Resets the command buffer's previous contents and state, then puts it into a building state allowing new commands
// to be recorded.
// Also starts command buffer dumping, if it is enabled.
Result Pm4CmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    const Result result = CmdBuffer::Begin(info);

    if (result == Result::Success)
    {
        if (info.pStateInheritCmdBuffer != nullptr)
        {
            InheritStateFromCmdBuf(static_cast<const Pm4CmdBuffer*>(info.pStateInheritCmdBuffer));
        }

        if (info.pInheritedState != nullptr)
        {
            m_gfxCmdBufStateFlags.clientPredicate  = info.pInheritedState->stateFlags.predication;
            m_pm4CmdBufState.flags.packetPredicate = info.pInheritedState->stateFlags.predication;
        }

        // If this is a nested command buffer execution, this value should be set to 1
        // pipePoint on nested command buffer cannot be optimized using the state from primary
        if (IsNested() == true)
        {
            SetPm4CmdBufCpBltState(true);
        }
    }

    return result;
}

// =====================================================================================================================
Result Pm4CmdBuffer::Reset(
    ICmdAllocator* pCmdAllocator,
    bool           returnGpuMemory)
{
    ResetFastClearReferenceCounts();

    return GfxCmdBuffer::Reset(pCmdAllocator, returnGpuMemory);
}

// =====================================================================================================================
// Completes recording of a command buffer in the building state, making it executable.
// Also ends command buffer dumping, if it is enabled.
Result Pm4CmdBuffer::End()
{
    Result result = GfxCmdBuffer::End();

    for (uint32 i = 0; i < static_cast<uint32>(QueryPoolType::Count); i++)
    {
        PAL_ASSERT(NumActiveQueries(static_cast<QueryPoolType>(i)) == 0);
    };

    return result;
}

// =====================================================================================================================
// Disables all queries on this command buffer, stopping them and marking them as unavailable.
void Pm4CmdBuffer::DeactivateQueries()
{
    for (uint32 i = 0; i < static_cast<uint32>(QueryPoolType::Count); i++)
    {
        const QueryPoolType queryPoolType = static_cast<QueryPoolType>(i);

        if (NumActiveQueries(queryPoolType) != 0)
        {
            DeactivateQueryType(queryPoolType);
        }
    }
}

// =====================================================================================================================
// Re-enables all previously active queries on this command buffer, starting them and marking them as available.
void Pm4CmdBuffer::ReactivateQueries()
{
    for (uint32 i = 0; i < static_cast<uint32>(QueryPoolType::Count); i++)
    {
        const QueryPoolType queryPoolType = static_cast<QueryPoolType>(i);

        if (NumActiveQueries(queryPoolType) != 0)
        {
            ActivateQueryType(queryPoolType);
        }
    }
}

// =====================================================================================================================
// Returns a new chunk by first searching the retained chunk list for a valid chunk then querying the command allocator
// if there are no retained chunks available.
CmdStreamChunk* Pm4CmdBuffer::GetNextGeneratedChunk()
{
    CmdStreamChunk* pChunk = nullptr;

    if (m_status == Result::Success)
    {
        // First search the retained chunk list
        if (m_retainedGeneratedChunkList.NumElements() > 0)
        {
            // When the chunk was retained the reference count was not modified so no need to add a reference here.
            m_retainedGeneratedChunkList.PopBack(&pChunk);
        }

        // If a retained chunk could not be found then allocate a new chunk and put it on our list. The allocator adds a
        // reference for us automatically. Embedded data chunks cannot be root chunks.
        if (pChunk == nullptr)
        {
            m_status = m_pCmdAllocator->GetNewChunk(EmbeddedDataAlloc, false, &pChunk);

            // Something bad happen and the GfxCmdBuffer will always be in error status ever after
            PAL_ALERT(m_status != Result::Success);
        }
    }

    // If we fail to get a new Chunk from GPU memory either because we ran out of GPU memory or DeviceLost, get a dummy
    // chunk to allow the program to proceed until the error is progagated back to the client.
    if (m_status != Result::Success)
    {
        pChunk = m_pCmdAllocator->GetDummyChunk();
        pChunk->Reset();

        // Make sure there is only one reference of dummy chunk at back of chunk list
        if (m_generatedChunkList.Back() == pChunk)
        {
            m_generatedChunkList.PopBack(nullptr);
        }
    }

    PAL_ASSERT(pChunk != nullptr);

    const Result result = m_generatedChunkList.PushBack(pChunk);
    PAL_ASSERT(result == Result::Success);

    // Generated chunks shouldn't be allocating their own busy trackers!
    PAL_ASSERT(pChunk->DwordsRemaining() == pChunk->SizeDwords());

    return pChunk;
}

// =====================================================================================================================
void Pm4CmdBuffer::ResetState()
{
    GfxCmdBuffer::ResetState();

    m_maxUploadFenceToken = 0;

    m_cmdBufPerfExptFlags.u32All            = 0;
    m_pm4CmdBufState.flags.u32All           = 0;
    m_pm4CmdBufState.flags.prevCmdBufActive = 1;

    // It's possible that another of our command buffers still has blts in flight, except for CP blts which must be
    // flushed in each command buffer postamble.
    m_pm4CmdBufState.flags.gfxBltActive        = IsGraphicsSupported();
    m_pm4CmdBufState.flags.gfxWriteCachesDirty = IsGraphicsSupported();
    m_pm4CmdBufState.flags.csBltActive         = IsComputeSupported();
    m_pm4CmdBufState.flags.csWriteCachesDirty  = IsComputeSupported();

    // It's possible that another of our command buffers still has rasterization kill draws in flight.
    m_pm4CmdBufState.flags.rasterKillDrawsActive = IsGraphicsSupported();

    // A previous, chained command buffer could have used a CP blt which may have accessed L2 or the memory directly.
    // By convention, our CP blts will only use L2 if the HW supports it so we only need to set one bit here.
    if (m_device.Parent()->ChipProperties().gfxLevel > GfxIpLevel::GfxIp6)
    {
        m_pm4CmdBufState.flags.cpWriteCachesDirty = IsCpDmaSupported();
    }
    else
    {
        m_pm4CmdBufState.flags.cpMemoryWriteL2CacheStale = IsCpDmaSupported();
    }

    memset(m_acqRelFenceVals, 0, sizeof(m_acqRelFenceVals));

    UpdatePm4CmdBufGfxBltExecEopFence();
    // Set a impossible waited fence until IssueReleaseSync assigns a meaningful value when sync RB cache.
    UpdatePm4CmdBufGfxBltWbEopFence(UINT32_MAX);
    UpdatePm4CmdBufCsBltExecFence();
    UpdatePm4CmdBufRasterKillDrawsExecEopFence(true);

}

// =====================================================================================================================
void Pm4CmdBuffer::CmdDuplicateUserData(
    PipelineBindPoint source,
    PipelineBindPoint dest)
{
    PAL_ASSERT(source != PipelineBindPoint::Graphics);
    PAL_ASSERT(source != dest);

    const UserDataEntries& sourceEntries = m_computeState.csUserDataEntries;

    CmdSetUserData(dest, 0, MaxUserDataEntries, sourceEntries.entries);
}

// =====================================================================================================================
void Pm4CmdBuffer::CmdSetKernelArguments(
    uint32            firstArg,
    uint32            argCount,
    const void*const* ppValues)
{
    // It's illegal to call this function without an HSA ABI pipeline bound.
    PAL_ASSERT(m_computeState.hsaAbiMode && (m_computeState.pipelineState.pPipeline != nullptr));
    PAL_ASSERT(m_computeState.pKernelArguments != nullptr);

    const ComputePipeline& pipeline = *static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);
    const HsaAbi::CodeObjectMetadata& metadata = pipeline.HsaMetadata();

    if (firstArg + argCount > metadata.NumArguments())
    {
        // Verify that we won't go out of bounds. Legally we could demote this to an assert if we want.
        SetCmdRecordingError(Result::ErrorInvalidValue);
    }
    else
    {
        for (uint32 idx = 0; idx < argCount; ++idx)
        {
            const HsaAbi::KernelArgument& arg = metadata.Arguments()[firstArg + idx];
            memcpy(m_computeState.pKernelArguments + arg.offset, ppValues[idx], arg.size);
        }
    }
}

// =====================================================================================================================
// Puts command stream related objects into a state ready for command building.
Result Pm4CmdBuffer::BeginCommandStreams(
    CmdStreamBeginFlags cmdStreamFlags,
    bool                doReset)
{
    if (doReset)
    {
        ResetFastClearReferenceCounts();
    }

    Result result = GfxCmdBuffer::BeginCommandStreams(cmdStreamFlags, doReset);

    if (result == Result::Success)
    {
        // Allocate acquire/release synchronization fence value GPU memory from the command allocator.
        // AllocateGpuScratchMem() always returns a valid GPU address, even if we fail to obtain memory from the
        // allocator.  In that scenario, the allocator returns a dummy chunk so we can always have a valid object
        // to access, and sets m_status to a failure code.
        m_acqRelFenceValGpuVa = AllocateGpuScratchMem(static_cast<uint32>(AcqRelEventType::Count), sizeof(uint32));
        result = m_status;
    }

    if (result == Result::Success)
    {
        // Allocate GPU memory for the internal event from the command allocator.
        result = AllocateAndBindGpuMemToEvent(m_pInternalEvent);
    }

    return result;
}

// =====================================================================================================================
// Decrements the ref count of images stored in the Fast clear eliminate ref count array.
void Pm4CmdBuffer::ResetFastClearReferenceCounts()
{
    if (m_fceRefCountVec.NumElements() > 0)
    {
        uint32* pCounter = nullptr;

        while (m_fceRefCountVec.NumElements() > 0)
        {
            m_fceRefCountVec.PopBack(&pCounter);
            Util::AtomicDecrement(pCounter);
        }
    }
}

// =====================================================================================================================
// Helper function to convert certain pipeline points to more accurate ones. This is for legacy barrier interface.
// Note: HwPipePostBlt will be converted to a more accurate stage based on the underlying implementation of
//       outstanding BLTs, but will be left as HwPipePostBlt if the internal outstanding BLTs can't be expressed as
//       a client-facing HwPipePoint (e.g., if there are CP DMA BLTs in flight).
void Pm4CmdBuffer::OptimizePipePoint(
    HwPipePoint* pPipePoint
) const
{
    if (pPipePoint != nullptr)
    {
        if (*pPipePoint == HwPipePostBlt)
        {
            // Check xxxBltActive states in order
            const Pm4CmdBufferStateFlags cmdBufStateFlags = GetPm4CmdBufState().flags;
            if (cmdBufStateFlags.gfxBltActive)
            {
                *pPipePoint = HwPipeBottom;
            }
            else if (cmdBufStateFlags.csBltActive)
            {
                *pPipePoint = HwPipePostCs;
            }
            else if (cmdBufStateFlags.cpBltActive)
            {
                // Leave it as HwPipePostBlt because CP DMA BLTs cannot be expressed as more specific HwPipePoint.
            }
            else
            {
                // If there are no BLTs in flight at this point, we will set the pipe point to HwPipeTop. This will
                // optimize any redundant stalls when called from the barrier implementation. Otherwise, this function
                // remaps the pipe point based on the gfx block that performed the BLT operation.
                *pPipePoint = HwPipeTop;
            }
        }
        else if (*pPipePoint == HwPipePreColorTarget)
        {
            // HwPipePreColorTarget is only valid as wait point. But for the sake of robustness, if it's used as pipe
            // point to wait on, it's equivalent to HwPipePostPs.
            *pPipePoint = HwPipePostPs;
        }
    }
}

// =====================================================================================================================
// Helper function to optimize cache mask by clearing unnecessary coherency flags. This is for legacy barrier interface.
void Pm4CmdBuffer::OptimizeSrcCacheMask(
    uint32* pCacheMask
) const
{
    if (pCacheMask != nullptr)
    {
        // There are various srcCache BLTs (Copy, Clear, and Resolve) which we can further optimize if we know which
        // write caches have been dirtied:
        // - If a graphics BLT occurred, alias these srcCaches to CoherColorTarget.
        // - If a compute BLT occurred, alias these srcCaches to CoherShader.
        // - If a CP L2 BLT occured, alias these srcCaches to CoherCp.
        // - If a CP direct-to-memory write occured, alias these srcCaches to CoherMemory.
        // Clear the original srcCaches from the srcCache mask for the rest of this scope.
        if (TestAnyFlagSet(*pCacheMask, CacheCoherencyBlt))
        {
            const Pm4CmdBufferStateFlags cmdBufStateFlags = GetPm4CmdBufState().flags;
            const bool                   isCopySrcOnly    = (*pCacheMask == CoherCopySrc);

            *pCacheMask &= ~CacheCoherencyBlt;

            *pCacheMask |= cmdBufStateFlags.cpWriteCachesDirty ? CoherCp : 0;
            *pCacheMask |= cmdBufStateFlags.cpMemoryWriteL2CacheStale ? CoherMemory : 0;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 740
            * pCacheMask |= cmdBufStateFlags.gfxWriteCachesDirty ? CoherColorTarget : 0;
            *pCacheMask |= cmdBufStateFlags.csWriteCachesDirty ? CoherShader : 0;
#else
            if (isCopySrcOnly)
            {
                *pCacheMask |= cmdBufStateFlags.gfxWriteCachesDirty ? CoherShaderRead : 0;
                *pCacheMask |= cmdBufStateFlags.csWriteCachesDirty ? CoherShaderRead : 0;
            }
            else
            {
                *pCacheMask |= cmdBufStateFlags.gfxWriteCachesDirty ? CoherColorTarget : 0;
                *pCacheMask |= cmdBufStateFlags.csWriteCachesDirty ? CoherShader : 0;
            }
#endif
        }
    }
}

// =====================================================================================================================
// Helper function to optimize pipeline stages and cache access masks for BLTs. This is for acquire/release interface.
// Note: PipelineStageBlt will be converted to a more accurate stage based on the underlying implementation of
//       outstanding BLTs, but will be left as PipelineStageBlt if the internal outstanding BLTs can't be expressed as
//       a client-facing PipelineStage (e.g., if there are CP DMA BLTs in flight).
void Pm4CmdBuffer::OptimizePipeAndCacheMaskForRelease(
    uint32* pStageMask, // [in/out] A representation of PipelineStageFlag
    uint32* pAccessMask // [in/out] A representation of CacheCoherencyUsageFlags
) const
{
    const Pm4CmdBufferStateFlags cmdBufStateFlags = GetPm4CmdBufState().flags;

    // Update pipeline stages if valid input stage mask is provided.
    if (pStageMask != nullptr)
    {
        uint32 localStageMask = *pStageMask;

        if (TestAnyFlagSet(localStageMask, PipelineStageBlt))
        {
            localStageMask &= ~PipelineStageBlt;

            // Check xxxBltActive states in order.
            if (cmdBufStateFlags.gfxBltActive)
            {
                localStageMask |= PipelineStageEarlyDsTarget | PipelineStageLateDsTarget | PipelineStageColorTarget;
            }
            if (cmdBufStateFlags.csBltActive)
            {
                localStageMask |= PipelineStageCs;
            }
            if (cmdBufStateFlags.cpBltActive)
            {
                // Add back PipelineStageBlt because we cannot express it with a more accurate stage.
                localStageMask |= PipelineStageBlt;
            }
        }

        *pStageMask = localStageMask;
    }

    // Update cache access masks if valid input access mask is provided.
    if (pAccessMask != nullptr)
    {
        uint32 localAccessMask = *pAccessMask;

        if (TestAnyFlagSet(localAccessMask, CacheCoherencyBlt))
        {
            const bool isCopySrcOnly = (localAccessMask == CoherCopySrc);

            // There are various srcCache BLTs (Copy, Clear, and Resolve) which we can further optimize if we know
            // which write caches have been dirtied:
            // - If a graphics BLT occurred, alias these srcCaches to CoherColorTarget.
            // - If a compute BLT occurred, alias these srcCaches to CoherShader.
            // - If a CP L2 BLT occured, alias these srcCaches to CoherCp.
            // - If a CP direct-to-memory write occured, alias these srcCaches to CoherMemory.
            // Clear the original srcCaches from the srcCache mask for the rest of this scope.
            localAccessMask &= ~CacheCoherencyBlt;

            localAccessMask |= cmdBufStateFlags.cpWriteCachesDirty ? CoherCp : 0;
            localAccessMask |= cmdBufStateFlags.cpMemoryWriteL2CacheStale ? CoherMemory : 0;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 740
            localAccessMask |= cmdBufStateFlags.gfxWriteCachesDirty ? CoherColorTarget : 0;
            localAccessMask |= cmdBufStateFlags.csWriteCachesDirty ? CoherShader : 0;
#else
            if (isCopySrcOnly)
            {
                localAccessMask |= cmdBufStateFlags.gfxWriteCachesDirty ? CoherShaderRead : 0;
                localAccessMask |= cmdBufStateFlags.csWriteCachesDirty ? CoherShaderRead : 0;
            }
            else
            {
                localAccessMask |= cmdBufStateFlags.gfxWriteCachesDirty ? CoherColorTarget : 0;
                localAccessMask |= cmdBufStateFlags.csWriteCachesDirty ? CoherShader : 0;
            }
#endif
        }

        *pAccessMask = localAccessMask;
    }
}

// =====================================================================================================================
void Pm4CmdBuffer::CmdSaveComputeState(uint32 stateFlags)
{
    GfxCmdBuffer::CmdSaveComputeState(stateFlags);

}

// =====================================================================================================================
void Pm4CmdBuffer::CmdRestoreComputeState(uint32 stateFlags)
{
    GfxCmdBuffer::CmdRestoreComputeState(stateFlags);

    // The caller has just executed one or more CS blts.
    SetPm4CmdBufCsBltState(true);
    SetPm4CmdBufCsBltWriteCacheState(true);

    UpdatePm4CmdBufCsBltExecFence();

}

// =====================================================================================================================
// Helper function which handles "leaking" a nested command buffer's per-pipeline state after being executed by a root
// command buffer.
void Pm4CmdBuffer::LeakPerPipelineStateChanges(
    const Pal::PipelineState& leakedPipelineState,
    const UserDataEntries&    leakedUserDataEntries,
    Pal::PipelineState*       pDestPipelineState,
    UserDataEntries*          pDestUserDataEntries)
{
    if (leakedPipelineState.pBorderColorPalette != nullptr)
    {
        pDestPipelineState->pBorderColorPalette = leakedPipelineState.pBorderColorPalette;
        pDestPipelineState->dirtyFlags.borderColorPaletteDirty = 1;
    }

    if (leakedPipelineState.pPipeline != nullptr)
    {
        pDestPipelineState->pPipeline = leakedPipelineState.pPipeline;
        pDestPipelineState->dirtyFlags.pipelineDirty = 1;
    }

    for (uint32 index = 0; index < NumUserDataFlagsParts; ++index)
    {
        pDestUserDataEntries->dirty[index]   |= leakedUserDataEntries.dirty[index];
        pDestUserDataEntries->touched[index] |= leakedUserDataEntries.touched[index];

        auto mask = leakedUserDataEntries.touched[index];
        for (uint32 bit : BitIterSizeT(mask))
        {
            const uint32 entry = (bit + (UserDataEntriesPerMask * index));
            pDestUserDataEntries->entries[entry] = leakedUserDataEntries.entries[entry];
        }
    }
}

// =====================================================================================================================
// Begins recording performance data using the specified Experiment object.
void Pm4CmdBuffer::CmdBeginPerfExperiment(
    IPerfExperiment* pPerfExperiment)
{
    const PerfExperiment*const pExperiment = static_cast<PerfExperiment*>(pPerfExperiment);
    PAL_ASSERT(pExperiment != nullptr);
    CmdStream* pCmdStream = GetCmdStreamByEngine(GetPerfExperimentEngine());

    // Indicates that this command buffer is used for enabling a perf experiment. This is used to write any VCOPs that
    // may be needed during submit time.
    const PerfExperimentFlags tracesEnabled = pExperiment->TracesEnabled();
    m_cmdBufPerfExptFlags.u32All |= tracesEnabled.u32All;

    pExperiment->IssueBegin(this, pCmdStream);
    if (tracesEnabled.perfCtrsEnabled || tracesEnabled.spmTraceEnabled)
    {
        m_pm4CmdBufState.flags.perfCounterStarted = 1;
        m_pm4CmdBufState.flags.perfCounterStopped = 0;
    }
    if (tracesEnabled.sqtTraceEnabled)
    {
        m_pm4CmdBufState.flags.sqttStarted = 1;
        m_pm4CmdBufState.flags.sqttStopped = 0;
    }
    if (tracesEnabled.dfSpmTraceEnabled)
    {
        // Cache a pointer to the DF SPM Perfmon Info so we can access it at submit time
        const DfSpmPerfmonInfo* pDfSpmPerfmonInfo = pExperiment->GetDfSpmPerfmonInfo();
        // We only support 1 DF perf experiment per command buffer.
        PAL_ASSERT((m_pDfSpmPerfmonInfo == nullptr) || (pDfSpmPerfmonInfo == m_pDfSpmPerfmonInfo));
        if (m_pDfSpmPerfmonInfo == nullptr)
        {
            m_pDfSpmPerfmonInfo = pDfSpmPerfmonInfo;
        }
    }

    m_pCurrentExperiment = pExperiment;
}

// =====================================================================================================================
// Updates the sqtt token mask on the specified Experiment object.
void Pm4CmdBuffer::CmdUpdatePerfExperimentSqttTokenMask(
    IPerfExperiment*              pPerfExperiment,
    const ThreadTraceTokenConfig& sqttTokenConfig)
{
    const PerfExperiment*const pExperiment = static_cast<PerfExperiment*>(pPerfExperiment);
    PAL_ASSERT(pExperiment != nullptr);
    CmdStream* pCmdStream = GetCmdStreamByEngine(GetPerfExperimentEngine());
    pExperiment->UpdateSqttTokenMask(pCmdStream, sqttTokenConfig);
}

// =====================================================================================================================
void Pm4CmdBuffer::CmdEndPerfExperiment(
    IPerfExperiment* pPerfExperiment)
{
    const PerfExperiment*const pExperiment = static_cast<PerfExperiment*>(pPerfExperiment);
    PAL_ASSERT(pPerfExperiment != nullptr);
    CmdStream* pCmdStream = GetCmdStreamByEngine(GetPerfExperimentEngine());
    // Normally, we should only be ending the currently bound perf experiment opened in this command buffer.  However,
    // when gathering full-frame SQ thread traces, an experiment could be opened in one command buffer and ended in
    // another.
    PAL_ASSERT((pPerfExperiment == m_pCurrentExperiment) || (m_pCurrentExperiment == nullptr));

    pExperiment->IssueEnd(this, pCmdStream);

    const PerfExperimentFlags tracesEnabled = pExperiment->TracesEnabled();
    if (tracesEnabled.perfCtrsEnabled || tracesEnabled.spmTraceEnabled)
    {
        m_pm4CmdBufState.flags.perfCounterStopped = 1;
    }
    if (tracesEnabled.sqtTraceEnabled)
    {
        m_pm4CmdBufState.flags.sqttStopped = 1;
    }

    m_pCurrentExperiment = nullptr;
}

// =====================================================================================================================
void Pm4CmdBuffer::OptimizeBarrierReleaseInfo(
    uint32             pipePointCount,
    HwPipePoint*       pPipePoints,
    uint32*            pCacheMask
    ) const
{
    for (uint32 i = 0; i < pipePointCount; i++)
    {
        OptimizePipePoint(&pPipePoints[i]);
    }

    if (pCacheMask != nullptr)
    {
        OptimizeSrcCacheMask(pCacheMask);
    }
}

// =====================================================================================================================
void Pm4CmdBuffer::OptimizeAcqRelReleaseInfo(
    uint32*                   pStageMask,
    uint32*                   pAccessMasks
    ) const
{
    OptimizePipeAndCacheMaskForRelease(pStageMask, pAccessMasks);
}

// =====================================================================================================================
// Updates a user-data table managed by embedded data & CPU updates.
void Pm4CmdBuffer::UpdateUserDataTableCpu(
    UserDataTableState* pTable,
    uint32              dwordsNeeded,
    uint32              offsetInDwords,
    const uint32* pSrcData,       // In: Data representing the *full* contents of the table, not just the part
                                        // between offsetInDwords and dwordsNeeded.
    uint32              alignmentInDwords)
{
    // The dwordsNeeded and offsetInDwords parameters together specify a "window" of the table which is relevant to
    // the active pipeline.  To save memory as well as cycles spent copying data, this will only allocate and populate
    // the portion of the user-data table inside that window.
    PAL_ASSERT((dwordsNeeded + offsetInDwords) <= pTable->sizeInDwords);

    // User-data can contain inline constant buffers which, for historical reasons, are defined in 4x32-bit chunks in
    // HLSL but are only DWORD size-aligned in the user-data layout. This means the following can occur:
    // 1. The app compiles a shader with 2 DWORDs in a constant buffer. The HLSL compiler implicitly pads the size
    //    of the constant buffer out to 4 DWORDs to meet the constant buffer size alignment rule. It also emits DXIL
    //    instructions which load a vector of 4 DWORDs from the constant buffer even though it will only use 2 values.
    // 2. The app defines a root signature which only contains 2 constants. The app is not required to add padding to
    //    the root signature. Accessing past the end of the root constants is defined to be undefined behavior.
    // Given that the input DXIL code instructs us to load 4 DWORDs, our compiled shader will do just that if the root
    // constants are spilled to memory. The values of those extra 2 DWORDs will be ignored but they are still read.
    // This can cause a GPU page fault if we get unlucky and the constant buffer padding falls in unmapped GPU memory.
    //
    // Page faulting is legal in this case but it's not at all user-friendly. We can avoid the page fault if we align
    // our table's base address to a multiple of 4 DWORDs. If each 4x32-bit load occurs on a 4x32-bit aligned address
    // it's impossible for part of that load to address unmapped memory.
    //
    // Aligning all tables to 4 DWORDs isn't expected to waste much memory so for simplictly we do it for all clients.
    // It should only matter if we interleave 1-3 DWORD embedded data allocations with table allocations many times,
    // such that this command buffer must allocate an additional embedded data chunk.
    const uint32 cbAlignment = Max(alignmentInDwords, 4u);

    gpusize gpuVirtAddr  = 0uLL;
    pTable->pCpuVirtAddr = (CmdAllocateEmbeddedData(dwordsNeeded, cbAlignment, &gpuVirtAddr) - offsetInDwords);
    pTable->gpuVirtAddr  = (gpuVirtAddr - (sizeof(uint32) * offsetInDwords));

    // There's technically a bug in the above table address calculation. We only write the low 32-bits of the table
    // address to user-data and assume the high bits are always the same. This is usually the case because we allocate
    // embedded data from a single 4GB virtual address range, but because we subtract the table offset from the real
    // vitual address we could underflow out of our fixed 4GB address range. This wouldn't be a problem if we sent the
    // full address to the GPU, but because the shader code infers the top 32 bits we can accidentally round up by 4GB.
    // This assert exists to detect this case at runtime.
    //
    // It's not that easy to fix this issue, we have two routes and neither seem attractive:
    // 1. Stop computing invalid pointers. This is probably the most correct solution but it's also the most difficult
    //    because we have an implicit contract with multiple compilers that the table pointer starts at offset zero.
    // 2. Define a maximum offset value and reserve enough VA space at the beginning of the VA range to ensure that we
    //    can never allocate embeded data in the range that can underflow. This will waste VA space and seems hacky.
    PAL_ASSERT(HighPart(gpuVirtAddr) == HighPart(pTable->gpuVirtAddr));

    uint32* pDstData = (pTable->pCpuVirtAddr + offsetInDwords);
    pSrcData += offsetInDwords;
    for (uint32 i = 0; i < dwordsNeeded; ++i)
    {
        *pDstData = *pSrcData;
        ++pDstData;
        ++pSrcData;
    }

    // Mark that the latest contents of the user-data table have been uploaded to the current embedded data chunk.
    pTable->dirty = 0;
}

// =====================================================================================================================
// Adds the gfxImage for which a fast clear eliminate was skipped to this command buffers list for tracking and
// increments the ref counter associated with the image.
// Note: The fast clear eliminate optimization aims to remove the unnecessary CPU work that is done for fast clear
//       eliminates for certain barrier transistions (compressed old state to compressed new state). If the clear color
//       was TC-compatible, the corresponding fast clear eliminate operation need not be done as it is predicated by the
//       GPU anyway. We accomplish this by allowing the fast clear eliminate, for this specific transition, only
//       when the image had been cleared with a non-TC-compatible clear color in the past, else we update a counter
//       and skip the fast clear eliminate. During command buffer reset, this counter is decremented for each command
//       buffer and for each time the fast clear eliminate was skipped. This cost of looping through the list is
//       outweighed by all the work that was skipped for setting up the FCE.
void Pm4CmdBuffer::AddFceSkippedImageCounter(
    GfxImage* pGfxImage)
{
    PAL_ASSERT(pGfxImage != nullptr);
    PAL_ASSERT(pGfxImage->IsFceOptimizationEnabled());

    const Result result = m_fceRefCountVec.PushBack(pGfxImage->GetFceRefCounter());
    if (result != Result::Success)
    {
        SetCmdRecordingError(result);
    }

    pGfxImage->IncrementFceRefCount();
}
} // Pal
