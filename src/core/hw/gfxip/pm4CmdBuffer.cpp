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

#include "core/cmdAllocator.h"
#include "core/gpuMemory.h"
#include "core/queue.h"
#include "core/perfExperiment.h"
#include "core/hw/gfxip/borderColorPalette.h"
#include "core/hw/gfxip/computePipeline.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/pipeline.h"
#include "core/hw/gfxip/pm4CmdBuffer.h"
#include "core/hw/gfxip/pm4Image.h"
#include "core/hw/gfxip/queryPool.h"
#include "core/hw/gfxip/rpm/rsrcProcMgr.h"
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
    const CmdBufferCreateInfo& createInfo,
    const GfxBarrierMgr*       pBarrierMgr)
    :
    GfxCmdBuffer(device, createInfo),
    m_acqRelFenceValGpuVa(0),
    m_timestampGpuVa(0),
    m_fceRefCountVec(device.GetPlatform()),
    m_pm4CmdBufState{},
    m_computeState{},
    m_computeRestoreState{},
    m_pBarrierMgr(pBarrierMgr),
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

    PAL_SAFE_FREE(m_computeState.pKernelArguments, m_device.GetPlatform());
    PAL_SAFE_FREE(m_computeRestoreState.pKernelArguments, m_device.GetPlatform());
}

// =====================================================================================================================
// Resets the command buffer's previous contents and state, then puts it into a building state allowing new commands
// to be recorded.
// Also starts command buffer dumping, if it is enabled.
Result Pm4CmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    const Result result = GfxCmdBuffer::Begin(info);

    if (result == Result::Success)
    {
        if (info.pStateInheritCmdBuffer != nullptr)
        {
            InheritStateFromCmdBuf(static_cast<const Pm4CmdBuffer*>(info.pStateInheritCmdBuffer));
        }

        if (info.pInheritedState != nullptr)
        {
            m_pm4CmdBufState.flags.packetPredicate = info.pInheritedState->stateFlags.predication;
        }

        // If this is a nested command buffer execution, this value should be set to 1
        // pipePoint on nested command buffer cannot be optimized using the state from primary
        if (IsNested() == true)
        {
            SetCpBltState(true);
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
// Helper function for updating user data entries and tracking flags common to different pipeline types. Specializes
// updating a single user data entry as well as WideBitfieldSetBit* functions to set two UserDataFlags bitmasks.
void Pm4CmdBuffer::SetUserData(
    uint32           firstEntry,
    uint32           entryCount,
    UserDataEntries* pEntries,
    const uint32*    pEntryValues)
{
    uint32 index       = (firstEntry / UserDataEntriesPerMask);
    uint32 startingBit = firstEntry & (UserDataEntriesPerMask - 1);

    if (entryCount == 1)
    {
        // Equivalent to WideBitfieldSetBit for both touched and dirty bitmasks
        const size_t mask  = (static_cast<size_t>(1) << startingBit);

        pEntries->touched[index] |= mask;
        pEntries->dirty[index]   |= mask;

        pEntries->entries[firstEntry] = pEntryValues[0];
    }
    else
    {
        // Equivalent to WideBitfieldSetRange for both touched and dirty bitmasks
        uint32 numBits = entryCount;

        while (numBits > 0)
        {
            const uint32 maxNumBits = UserDataEntriesPerMask - startingBit;
            const uint32 curNumBits = (maxNumBits < numBits) ? maxNumBits : numBits;
            const size_t bitMask    = (curNumBits == UserDataEntriesPerMask) ? -1 : ((static_cast<size_t>(1) << curNumBits) - 1);

            pEntries->touched[index] |= (bitMask << startingBit);
            pEntries->dirty[index]   |= (bitMask << startingBit);

            index++;
            startingBit  = 0;
            numBits     -= curNumBits;
        }

        memcpy(&pEntries->entries[firstEntry], pEntryValues, entryCount * sizeof(uint32));
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

    m_pm4CmdBufState.flags.u32All           = 0;
    m_pm4CmdBufState.flags.prevCmdBufActive = 1;

    // It's possible that another of our command buffers still has blts in flight, except for CP blts which must be
    // flushed in each command buffer postamble.
    if (IsGraphicsSupported())
    {
        m_pm4CmdBufState.flags.gfxBltActive        = 1;
        m_pm4CmdBufState.flags.gfxWriteCachesDirty = 1;
    }

    if (IsComputeSupported())
    {
        m_pm4CmdBufState.flags.csBltActive        = 1;
        m_pm4CmdBufState.flags.csWriteCachesDirty = 1;
    }

    if (IsCpDmaSupported())
    {
        // A previous, chained command buffer could have used a CP blt which may have accessed L2 or memory directly.
        // By convention, our CP blts will only use L2 if the HW supports it so we only need to set one bit here.
        {
            m_pm4CmdBufState.flags.cpWriteCachesDirty = 1;
        }
    }

    memset(m_acqRelFenceVals, 0, sizeof(m_acqRelFenceVals));

    UpdateGfxBltExecEopFence();
    // Set a impossible waited fence until IssueReleaseSync assigns a meaningful value when sync RB cache.
    UpdateGfxBltWbEopFence(UINT32_MAX);
    UpdateCsBltExecFence();

    PAL_SAFE_FREE(m_computeState.pKernelArguments, m_device.GetPlatform());
    memset(&m_computeState, 0, sizeof(m_computeState));

    PAL_SAFE_FREE(m_computeRestoreState.pKernelArguments, m_device.GetPlatform());
    memset(&m_computeRestoreState, 0, sizeof(m_computeRestoreState));
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
        PAL_ASSERT_ALWAYS_MSG("Kernel argument count is off! More arguments than expected");
        // Verify that we won't go out of bounds.
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
        // Allocate timestamp GPU memory from the command allocator.
        // AllocateGpuScratchMem() always returns a valid GPU address, even if we fail to obtain memory from the
        // allocator.  In that scenario, the allocator returns a dummy chunk so we can always have a valid object
        // to access, and sets m_status to a failure code.
        m_timestampGpuVa = AllocateGpuScratchMem(sizeof(uint32), sizeof(uint32));
        result = m_status;
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
        }
    }
}

// =====================================================================================================================
// Helper function to optimize pipeline stages and cache access masks for BLTs. This is for acquire/release interface.
// Note: PipelineStageBlt will be converted to a more accurate stage based on the underlying implementation of
//       outstanding BLTs, but will be left as PipelineStageBlt if the internal outstanding BLTs can't be expressed as
//       a client-facing PipelineStage (e.g., if there are CP DMA BLTs in flight).
void Pm4CmdBuffer::OptimizePipeStageAndCacheMask(
    uint32* pSrcStageMask,
    uint32* pSrcAccessMask,
    uint32* pDstStageMask,
    uint32* pDstAccessMask
    ) const
{
    const Pm4CmdBufferStateFlags cmdBufStateFlags = GetPm4CmdBufState().flags;

    // Update pipeline stages if valid input stage mask is provided.
    if (pSrcStageMask != nullptr)
    {
        uint32 localStageMask = *pSrcStageMask;

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

        *pSrcStageMask = localStageMask;
    }

    // Update cache access masks if valid input access mask is provided.
    if (pSrcAccessMask != nullptr)
    {
        uint32 localAccessMask = *pSrcAccessMask;

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
        }

        *pSrcAccessMask = localAccessMask;
    }
}

// =====================================================================================================================
void Pm4CmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    // Try to catch users who try to bind graphics pipelines to compute command buffers.
    PAL_DEBUG_BUILD_ONLY_ASSERT((params.pipelineBindPoint == PipelineBindPoint::Compute) || IsGraphicsSupported());

    const auto* const pPipeline = static_cast<const Pipeline*>(params.pPipeline);

    if (params.pipelineBindPoint == PipelineBindPoint::Compute)
    {
        m_computeState.pipelineState.pPipeline  = pPipeline;
        m_computeState.pipelineState.apiPsoHash = params.apiPsoHash;
        m_computeState.pipelineState.dirtyFlags.pipeline = 1;

        m_computeState.dynamicCsInfo = params.cs;
        m_computeState.hsaAbiMode    = (pPipeline != nullptr) && (pPipeline->GetInfo().flags.hsaAbi == 1);

        // It's simplest to always free the kernel args buffer and allocate a new one with the proper size if needed.
        PAL_SAFE_FREE(m_computeState.pKernelArguments, m_device.GetPlatform());

        if (m_computeState.hsaAbiMode)
        {
            // HSA mode overwrites the user-data SGPRs. The easiest way to force user-data validation when we return
            // to PAL mode is to mark all user-data values that have ever been set as dirty.
            memcpy(m_computeState.csUserDataEntries.dirty,
                   m_computeState.csUserDataEntries.touched,
                   sizeof(m_computeState.csUserDataEntries.dirty));

            const ComputePipeline& pipeline            = *static_cast<const ComputePipeline*>(pPipeline);
            const HsaAbi::CodeObjectMetadata& metadata = pipeline.HsaMetadata();

            // We're callocing here on purpose because some HSA ABI arguments need to use zero by default.
            m_computeState.pKernelArguments = static_cast<uint8*>(PAL_CALLOC(metadata.KernargSegmentSize(),
                                                                             m_device.GetPlatform(),
                                                                             AllocInternal));

            if (m_computeState.pKernelArguments == nullptr)
            {
                // Allocation failure, mark buffer as faulty.
                NotifyAllocFailure();
            }
        }
    }

    m_device.DescribeBindPipeline(this, pPipeline, params.apiPsoHash, params.pipelineBindPoint);

    if (pPipeline != nullptr)
    {
        m_maxUploadFenceToken = Max(m_maxUploadFenceToken, pPipeline->GetUploadFenceToken());
        m_lastPagingFence     = Max(m_lastPagingFence, pPipeline->GetPagingFenceVal());
    }
}

// =====================================================================================================================
// Set all specified state on this command buffer.
void Pm4CmdBuffer::SetComputeState(
    const ComputeState& newComputeState,
    uint32              stateFlags)
{
    if (TestAnyFlagSet(stateFlags, ComputeStatePipelineAndUserData))
    {
        if (newComputeState.pipelineState.pPipeline != m_computeState.pipelineState.pPipeline)
        {
            PipelineBindParams bindParams = {};
            bindParams.pipelineBindPoint  = PipelineBindPoint::Compute;
            bindParams.pPipeline          = newComputeState.pipelineState.pPipeline;
            bindParams.cs                 = newComputeState.dynamicCsInfo;
            bindParams.apiPsoHash         = newComputeState.pipelineState.apiPsoHash;

            CmdBindPipeline(bindParams);
        }

        // We're only supposed to save/restore kernel args in HSA mode and user-data in PAL mode.
        if (m_computeState.hsaAbiMode)
        {
            // It's impossible to be in HSA mode without a pipeline.
            PAL_ASSERT(m_computeState.pipelineState.pPipeline != nullptr);

            // By now the current pKernelArguments must have the same size as the saved original. We must memcpy it
            // because this function is used in places where we can't just assume ownership of the saved copy's buffer.
            const auto& pipeline = *static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);
            const uint32 size    = pipeline.HsaMetadata().KernargSegmentSize();

            memcpy(m_computeState.pKernelArguments, newComputeState.pKernelArguments, size);
        }
        else
        {
            CmdSetUserData(PipelineBindPoint::Compute,
                           0,
                           m_device.Parent()->ChipProperties().gfxip.maxUserDataEntries,
                           &newComputeState.csUserDataEntries.entries[0]);
        }
    }

    if (TestAnyFlagSet(stateFlags, ComputeStateBorderColorPalette) &&
        (newComputeState.pipelineState.pBorderColorPalette != m_computeState.pipelineState.pBorderColorPalette))
    {
        CmdBindBorderColorPalette(PipelineBindPoint::Compute, newComputeState.pipelineState.pBorderColorPalette);
    }
}

// =====================================================================================================================
// CmdSetUserData callback which updates the tracked user-data entries for the compute state.
void PAL_STDCALL Pm4CmdBuffer::CmdSetUserDataCs(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    PAL_ASSERT((pCmdBuffer != nullptr) && (entryCount != 0) && (pEntryValues != nullptr));

    auto* const pThis    = static_cast<Pm4CmdBuffer*>(pCmdBuffer);
    auto* const pEntries = &pThis->m_computeState.csUserDataEntries;

    // It's illegal to bind user-data when in HSA ABI mode.
    PAL_ASSERT(pThis->m_computeState.hsaAbiMode == false);

    // NOTE: Compute operations are expected to be far rarer than graphics ones, so at the moment it is not expected
    // that filtering-out redundant compute user-data updates is worthwhile.
    SetUserData(firstEntry, entryCount, pEntries, pEntryValues);
}

// =====================================================================================================================
// Copies the requested portion of the currently bound compute state to m_computeRestoreState. All active queries will
// be disabled.
void Pm4CmdBuffer::CmdSaveComputeState(uint32 stateFlags)
{
    GfxCmdBuffer::CmdSaveComputeState(stateFlags);

    if (TestAnyFlagSet(stateFlags, ComputeStatePipelineAndUserData))
    {
        // It should be impossible to already have this allocated because we null it out on restore.
        PAL_ASSERT(m_computeRestoreState.pKernelArguments == nullptr);

        // Copy over the bound pipeline and all non-indirect user-data state.
        m_computeRestoreState = m_computeState;

        // In HSA mode we must also duplicate the dynamically allocated current kernel argument buffer.
        if (m_computeState.hsaAbiMode)
        {
            // It's impossible to be in HSA mode without a pipeline.
            PAL_ASSERT(m_computeState.pipelineState.pPipeline != nullptr);

            const auto& pipeline = *static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);
            const uint32 size = pipeline.HsaMetadata().KernargSegmentSize();

            m_computeRestoreState.pKernelArguments =
                static_cast<uint8*>(PAL_MALLOC(size, m_device.GetPlatform(), AllocInternal));

            if (m_computeRestoreState.pKernelArguments == nullptr)
            {
                // Allocation failure, mark buffer as faulty.
                NotifyAllocFailure();
            }
            else
            {
                memcpy(m_computeRestoreState.pKernelArguments, m_computeState.pKernelArguments, size);
            }
        }
    }

    if (TestAnyFlagSet(stateFlags, ComputeStateBorderColorPalette))
    {
        // Copy over the bound border color palette.
        m_computeRestoreState.pipelineState.pBorderColorPalette = m_computeState.pipelineState.pBorderColorPalette;
    }

    // Disable all active queries so that we don't sample internal operations in the app's query pool slots.
    //
    // NOTE: We expect Vulkan won't set this flag because Vulkan allows blits to occur inside nested command buffers.
    // In a nested command buffer, we don't know what value of DB_COUNT_CONTROL to restore because the query state may
    // have been inherited from the calling command buffer. Luckily, Vulkan also states that whether blit or barrier
    // operations affect the results of queries is implementation-defined. So, for symmetry, they should not disable
    // active queries for blits.
    if (m_buildFlags.disableQueryInternalOps)
    {
        DeactivateQueries();
    }
}

// =====================================================================================================================
// Restores the requested portion of the last saved compute state in m_computeRestoreState, rebinding all objects as
// necessary. All previously disabled queries will be reactivated.
void Pm4CmdBuffer::CmdRestoreComputeStateInternal(
    uint32 stateFlags,
    bool   trackBltActiveFlags)
{
    // Vulkan does allow blits in nested command buffers, but they do not support inheriting user-data values from
    // the caller. Therefore, simply "setting" the restored-state's user-data is sufficient, just like it is in a
    // root command buffer. (If Vulkan decides to support user-data inheritance in a later API version, we'll need
    // to revisit this!)

    SetComputeState(m_computeRestoreState, stateFlags);

    // We may have allocated this if we saved while in HSA mode. It makes things simpler if we just free it now.
    PAL_SAFE_FREE(m_computeRestoreState.pKernelArguments, m_device.GetPlatform());

    GfxCmdBuffer::CmdRestoreComputeStateInternal(stateFlags, trackBltActiveFlags);

    // Reactivate all queries that we stopped in CmdSaveComputeState.
    if (m_buildFlags.disableQueryInternalOps)
    {
        ReactivateQueries();
    }

    // No need track blt active flags (expect trackBltActiveFlags == false) for below cases:
    //  1. CmdRestoreComputeState() call from PAL clients.
    //  2. CmdRestoreComputeState() call from auto sync clear case.
    if (trackBltActiveFlags)
    {
        // The caller has just executed one or more CS blts.
        SetCsBltState(true);
        SetCsBltWriteCacheState(true);

        UpdateCsBltExecFence();
    }
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
        pDestPipelineState->dirtyFlags.borderColorPalette = 1;
    }

    if (leakedPipelineState.pPipeline != nullptr)
    {
        pDestPipelineState->pPipeline = leakedPipelineState.pPipeline;
        pDestPipelineState->dirtyFlags.pipeline = 1;
        pDestPipelineState->dirtyFlags.dynamicState = 1;
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

    // Preemption needs to be disabled during any perf experiment for accuracy.
    pCmdStream->DisablePreemption();

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
    // Preemption needs to be disabled during any perf experiment for accuracy.
    pCmdStream->DisablePreemption();
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

    // Preemption needs to be disabled during any perf experiment for accuracy.
    pCmdStream->DisablePreemption();

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
    if (m_pBarrierMgr != nullptr)
    {
        for (uint32 i = 0; i < pipePointCount; i++)
        {
            m_pBarrierMgr->OptimizePipePoint(this, &pPipePoints[i]);
        }

        if (pCacheMask != nullptr)
        {
            m_pBarrierMgr->OptimizeSrcCacheMask(this, pCacheMask);
        }
    }
    else
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
}

// =====================================================================================================================
void Pm4CmdBuffer::OptimizeAcqRelReleaseInfo(
    uint32*                   pStageMask,
    uint32*                   pAccessMasks
    ) const
{
    if (m_pBarrierMgr != nullptr)
    {
        m_pBarrierMgr->OptimizePipeStageAndCacheMask(this, pStageMask, pAccessMasks, nullptr, nullptr);
    }
    else
    {
        OptimizePipeStageAndCacheMask(pStageMask, pAccessMasks, nullptr, nullptr);
    }
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
    PAL_DEBUG_BUILD_ONLY_ASSERT((dwordsNeeded + offsetInDwords) <= pTable->sizeInDwords);

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
    PAL_DEBUG_BUILD_ONLY_ASSERT(HighPart(gpuVirtAddr) == HighPart(pTable->gpuVirtAddr));

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
    Pm4Image* pPm4Image)
{
    PAL_ASSERT(pPm4Image != nullptr);
    PAL_ASSERT(pPm4Image->IsFceOptimizationEnabled());

    const Result result = m_fceRefCountVec.PushBack(pPm4Image->GetFceRefCounter());
    if (result != Result::Success)
    {
        SetCmdRecordingError(result);
    }

    pPm4Image->IncrementFceRefCount();
}

// =====================================================================================================================
void Pm4CmdBuffer::CmdBarrier(
    const BarrierInfo& barrierInfo)
{
    PAL_ASSERT(m_pBarrierMgr != nullptr);

    CmdBuffer::CmdBarrier(barrierInfo);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_pm4CmdBufState.flags.packetPredicate;
    m_pm4CmdBufState.flags.packetPredicate = 0;

    // Mark these as traditional barriers in RGP
    m_pBarrierMgr->DescribeBarrierStart(this, barrierInfo.reason, Developer::BarrierType::Full);

    bool splitMemAllocated;
    BarrierInfo splitBarrierInfo = barrierInfo;
    Result result = GfxBarrierMgr::SplitBarrierTransitions(m_device.GetPlatform(),
                                                           &splitBarrierInfo,
                                                           &splitMemAllocated);

    Developer::BarrierOperations barrierOps = {};

    if (result == Result::Success)
    {
        m_pBarrierMgr->Barrier(this, splitBarrierInfo, &barrierOps);
    }
    else if (result == Result::ErrorOutOfMemory)
    {
        NotifyAllocFailure();
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    // Delete memory allocated for splitting the BarrierTransitions if necessary.
    if (splitMemAllocated)
    {
        PAL_SAFE_DELETE_ARRAY(splitBarrierInfo.pTransitions, m_device.GetPlatform());
    }

    m_pBarrierMgr->DescribeBarrierEnd(this, &barrierOps);

    m_pm4CmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
uint32 Pm4CmdBuffer::CmdRelease(
    const AcquireReleaseInfo& releaseInfo)
{
    PAL_ASSERT(m_pBarrierMgr != nullptr);

    CmdBuffer::CmdRelease(releaseInfo);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_pm4CmdBufState.flags.packetPredicate;
    m_pm4CmdBufState.flags.packetPredicate = 0;

    // Mark these as traditional barriers in RGP
    m_pBarrierMgr->DescribeBarrierStart(this, releaseInfo.reason, Developer::BarrierType::Release);

    bool splitMemAllocated;
    AcquireReleaseInfo splitReleaseInfo = releaseInfo;
    Result result = GfxBarrierMgr::SplitImgBarriers(m_device.GetPlatform(), &splitReleaseInfo, &splitMemAllocated);

    Developer::BarrierOperations barrierOps = {};
    uint32 syncToken = 0;

    if (result == Result::Success)
    {
        syncToken = m_pBarrierMgr->Release(this, splitReleaseInfo, &barrierOps);
    }
    else if (result == Result::ErrorOutOfMemory)
    {
        NotifyAllocFailure();
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    // Delete memory allocated for splitting ImgBarriers if necessary.
    if (splitMemAllocated)
    {
        PAL_SAFE_DELETE_ARRAY(splitReleaseInfo.pImageBarriers, m_device.GetPlatform());
    }

    m_pBarrierMgr->DescribeBarrierEnd(this, &barrierOps);

    m_pm4CmdBufState.flags.packetPredicate = packetPredicate;

    return syncToken;
}

// =====================================================================================================================
void Pm4CmdBuffer::CmdAcquire(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    syncTokenCount,
    const uint32*             pSyncTokens)
{
    PAL_ASSERT(m_pBarrierMgr != nullptr);

    CmdBuffer::CmdAcquire(acquireInfo, syncTokenCount, pSyncTokens);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_pm4CmdBufState.flags.packetPredicate;
    m_pm4CmdBufState.flags.packetPredicate = 0;

    // Mark these as traditional barriers in RGP
    m_pBarrierMgr->DescribeBarrierStart(this, acquireInfo.reason, Developer::BarrierType::Acquire);

    bool splitMemAllocated;
    AcquireReleaseInfo splitAcquireInfo = acquireInfo;
    Result result = GfxBarrierMgr::SplitImgBarriers(m_device.GetPlatform(), &splitAcquireInfo, &splitMemAllocated);

    Developer::BarrierOperations barrierOps = {};

    if (result == Result::Success)
    {
        m_pBarrierMgr->Acquire(this, splitAcquireInfo, syncTokenCount, pSyncTokens, &barrierOps);
    }
    else if (result == Result::ErrorOutOfMemory)
    {
        NotifyAllocFailure();
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    // Delete memory allocated for splitting ImgBarriers if necessary.
    if (splitMemAllocated)
    {
        PAL_SAFE_DELETE_ARRAY(splitAcquireInfo.pImageBarriers, m_device.GetPlatform());
    }

    m_pBarrierMgr->DescribeBarrierEnd(this, &barrierOps);

    m_pm4CmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void Pm4CmdBuffer::CmdReleaseEvent(
    const AcquireReleaseInfo& releaseInfo,
    const IGpuEvent*          pGpuEvent)
{
    PAL_ASSERT(m_pBarrierMgr != nullptr);

    CmdBuffer::CmdReleaseEvent(releaseInfo, pGpuEvent);

    // Barriers do not honor predication.
    const uint32 packetPredicate           = m_pm4CmdBufState.flags.packetPredicate;
    m_pm4CmdBufState.flags.packetPredicate = 0;

    // Mark these as traditional barriers in RGP
    m_pBarrierMgr->DescribeBarrierStart(this, releaseInfo.reason, Developer::BarrierType::Release);

    bool splitMemAllocated;
    AcquireReleaseInfo splitReleaseInfo = releaseInfo;
    Result result = GfxBarrierMgr::SplitImgBarriers(m_device.GetPlatform(), &splitReleaseInfo, &splitMemAllocated);

    Developer::BarrierOperations barrierOps = {};

    if (result == Result::Success)
    {
        m_pBarrierMgr->ReleaseEvent(this, splitReleaseInfo, pGpuEvent, &barrierOps);
    }
    else if (result == Result::ErrorOutOfMemory)
    {
        NotifyAllocFailure();
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    // Delete memory allocated for splitting ImgBarriers if necessary.
    if (splitMemAllocated)
    {
        PAL_SAFE_DELETE_ARRAY(splitReleaseInfo.pImageBarriers, m_device.GetPlatform());
    }

    m_pBarrierMgr->DescribeBarrierEnd(this, &barrierOps);

    m_pm4CmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void Pm4CmdBuffer::CmdAcquireEvent(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    gpuEventCount,
    const IGpuEvent* const*   ppGpuEvents)
{
    PAL_ASSERT(m_pBarrierMgr != nullptr);

    CmdBuffer::CmdAcquireEvent(acquireInfo, gpuEventCount, ppGpuEvents);

    // Barriers do not honor predication.
    const uint32 packetPredicate           = m_pm4CmdBufState.flags.packetPredicate;
    m_pm4CmdBufState.flags.packetPredicate = 0;

    // Mark these as traditional barriers in RGP
    m_pBarrierMgr->DescribeBarrierStart(this, acquireInfo.reason, Developer::BarrierType::Acquire);

    bool splitMemAllocated;
    AcquireReleaseInfo splitAcquireInfo = acquireInfo;
    Result result = GfxBarrierMgr::SplitImgBarriers(m_device.GetPlatform(), &splitAcquireInfo, &splitMemAllocated);

    Developer::BarrierOperations barrierOps = {};

    if (result == Result::Success)
    {
        m_pBarrierMgr->AcquireEvent(this, splitAcquireInfo, gpuEventCount, ppGpuEvents, &barrierOps);
    }
    else if (result == Result::ErrorOutOfMemory)
    {
        NotifyAllocFailure();
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    // Delete memory allocated for splitting ImgBarriers if necessary.
    if (splitMemAllocated)
    {
        PAL_SAFE_DELETE_ARRAY(splitAcquireInfo.pImageBarriers, m_device.GetPlatform());
    }

    m_pBarrierMgr->DescribeBarrierEnd(this, &barrierOps);

    m_pm4CmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void Pm4CmdBuffer::CmdReleaseThenAcquire(
    const AcquireReleaseInfo& barrierInfo)
{
    PAL_ASSERT(m_pBarrierMgr != nullptr);

    CmdBuffer::CmdReleaseThenAcquire(barrierInfo);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_pm4CmdBufState.flags.packetPredicate;
    m_pm4CmdBufState.flags.packetPredicate = 0;

    // Mark these as traditional barriers in RGP
    m_pBarrierMgr->DescribeBarrierStart(this, barrierInfo.reason, Developer::BarrierType::Full);

    bool splitMemAllocated;
    AcquireReleaseInfo splitBarrierInfo = barrierInfo;
    Result result = GfxBarrierMgr::SplitImgBarriers(m_device.GetPlatform(), &splitBarrierInfo, &splitMemAllocated);

    Developer::BarrierOperations barrierOps = {};

    if (result == Result::Success)
    {
        m_pBarrierMgr->ReleaseThenAcquire(this, splitBarrierInfo, &barrierOps);
    }
    else if (result == Result::ErrorOutOfMemory)
    {
        NotifyAllocFailure();
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    // Delete memory allocated for splitting ImgBarriers if necessary.
    if (splitMemAllocated)
    {
        PAL_SAFE_DELETE_ARRAY(splitBarrierInfo.pImageBarriers, m_device.GetPlatform());
    }

    m_pBarrierMgr->DescribeBarrierEnd(this, &barrierOps);

    m_pm4CmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void Pm4CmdBuffer::CmdResolveQuery(
    const IQueryPool& queryPool,
    QueryResultFlags  flags,
    QueryType         queryType,
    uint32            startQuery,
    uint32            queryCount,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           dstStride)
{
    // Resolving a query is not supposed to honor predication.
    const uint32 packetPredicate = m_pm4CmdBufState.flags.packetPredicate;
    m_pm4CmdBufState.flags.packetPredicate = 0;
    m_device.RsrcProcMgr().CmdResolveQuery(this,
                                           static_cast<const QueryPool&>(queryPool),
                                           flags,
                                           queryType,
                                           startQuery,
                                           queryCount,
                                           static_cast<const GpuMemory&>(dstGpuMemory),
                                           dstOffset,
                                           dstStride);

    m_pm4CmdBufState.flags.packetPredicate = packetPredicate;
}

} // Pal
