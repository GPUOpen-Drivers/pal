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
#include "core/cmdStream.h"
#include "core/device.h"
#include "core/fence.h"
#include "core/g_palSettings.h"
#include "core/queue.h"
#include "palFile.h"
#include "palHashMapImpl.h"
#include "palLinearAllocator.h"
#include "palVectorImpl.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
CmdStream::CmdStream(
    Device*        pDevice,
    ICmdAllocator* pCmdAllocator,
    EngineType     engineType,
    SubEngineType  subEngineType,
    CmdStreamUsage cmdStreamUsage,
    uint32         postambleDwords,  // Each chunk must reserve at least this many DWORDs for final commands.
    uint32         minPaddingDwords, // The size of the smallest padding command this stream can write.
    bool           isNested)
    :
    m_chunkList(pDevice->GetPlatform()),
    m_retainedChunkList(pDevice->GetPlatform()),
    m_subEngineType(subEngineType),
    m_cmdStreamUsage(cmdStreamUsage),
    m_sizeAlignDwords(pDevice->EngineProperties().perEngine[engineType].sizeAlignInDwords),
    m_startAlignBytes(pDevice->EngineProperties().perEngine[engineType].startAlign),
    m_pCmdAllocator(static_cast<CmdAllocator*>(pCmdAllocator)),
    m_pMemAllocator(nullptr),
    m_pDevice(pDevice),
    m_engineType(engineType),
    m_cmdSpaceDwordPadding(0),
    m_reserveLimit(Device::CmdStreamReserveLimit),
    m_chunkDwordsAvailable(0),
    m_pReserveBuffer(nullptr),
    m_nestedChunks(32, pDevice->GetPlatform()),
    m_status(Result::Success),
    m_totalChunkDwords(0)
#if PAL_ENABLE_PRINTS_ASSERTS
    , m_streamGeneration(0)
    , m_isReserved(false)
#endif
{
    const auto& engineInfo = pDevice->EngineProperties().perEngine[engineType];

    uint32 worstCasePaddingDwords = 0;

    if (m_sizeAlignDwords > 1)
    {
        // Depending on minPaddingDwords, there are two ways to compute reservePaddingDwords:
        //   1) minPaddingDwords is one: No gaps are too small to pad, the worst case padding is m_sizeAlignDwords - 1.
        //   2) minPaddingDwords is greater than one: If the gap is minPaddingDwords - 1 we are forced to overpad to
        //      the next aligned size. The worst case padding is m_sizeAlignDwords + minPaddingDwords - 1.
        worstCasePaddingDwords = (minPaddingDwords <= 1) ? (m_sizeAlignDwords - 1)
                                                         : (m_sizeAlignDwords + minPaddingDwords - 1);
    }

    // Save enough command space for the postamble and the worst case NOP padding.
    uint32* pThisCmdSpaceDwordPadding = const_cast<uint32*>(&m_cmdSpaceDwordPadding);
    *pThisCmdSpaceDwordPadding = postambleDwords + worstCasePaddingDwords;

    // Make sure that our size needs to be aligned to something...
    PAL_ASSERT(m_sizeAlignDwords != 0);

    if (m_pCmdAllocator != nullptr)
    {
        // The reserve limit cannot be larger than the chunk size minus the padding space. Reserve limits up to 1024
        // DWORDs will always be OK; anything larger is at the mercy of the client's suballocation size.
        PAL_ASSERT(m_reserveLimit <=
            (m_pCmdAllocator->ChunkSize(CommandDataAlloc) / sizeof(uint32)) - m_cmdSpaceDwordPadding);
    }

    // Cannot init flags bitfield in the initializer list.
    m_flags.value = 0;

    if (engineInfo.flags.mustBuildCmdBuffersInSystemMem ||
        (isNested && (engineInfo.flags.indirectBufferSupport == 0)))
    {
        m_flags.buildInSysMem = 1;
    }

    // Preemption can only be enabled if:
    // - The KMD has enabled preemption support for this engine.
    // - The command stream is a workload stream.
    if ((engineInfo.flags.supportsMidCmdBufPreemption != 0) &&
        (m_cmdStreamUsage == CmdStreamUsage::Workload))
    {
        // We may want to disable preemption even if the KMD has enabled it. Because of the way the CE runs ahead of
        // the DE, we must always enable preemption for our CE workload even if we want to disable preemption or we
        // may corrupt or hang another driver's submit. Disabling preemption on our DE stream will be enough to tell
        // the KMD/CP that we want preemption disabled for our submit.
        m_flags.enablePreemption = ((m_subEngineType == SubEngineType::ConstantEngine) ||
                                    (m_pDevice->Settings().cmdBufPreemptionMode == CmdBufPreemptModeEnable));
    }
}

// =====================================================================================================================
CmdStream::~CmdStream()
{
    // Call reset to drop all chunk references.
    Reset(nullptr, true);
}

// =====================================================================================================================
Result CmdStream::Init()
{
    return m_nestedChunks.Init();
}

// =====================================================================================================================
// Subclasses should override this function and modify requestOptimization if they wish to control when optimizations
// are enabled or disabled.
Result CmdStream::Begin(
    CmdStreamBeginFlags     flags,
    VirtualLinearAllocator* pMemAllocator)
{
    m_flags.prefetchCommands = flags.prefetchCommands;
    m_flags.optimizeCommands = flags.optimizeCommands;

    // Save the caller's memory allocator for later use.
    m_pMemAllocator = pMemAllocator;

    return Result::Success;
}

// =====================================================================================================================
// Returns a pointer to enough memory to store m_reserveLimit DWORDs of commands.
uint32* CmdStream::ReserveCommands()
{
    // Why are we reserving constant engine space when we don't have a constant engine?
    PAL_ASSERT((m_subEngineType != SubEngineType::ConstantEngine) ||
               (m_pDevice->EngineProperties().perEngine[m_engineType].flags.constantEngineSupport != 0));

#if PAL_ENABLE_PRINTS_ASSERTS
    // It's not legal to call ReserveCommands twice in a row.
    PAL_ASSERT(m_isReserved == false);
    m_isReserved = true;
#endif

    // Preemptively allocate enough space to store all commands the caller could write.
    m_pReserveBuffer = AllocCommandSpace(m_reserveLimit);

#if PAL_ENABLE_PRINTS_ASSERTS
    // Debug builds can memset all command space before the caller has a chance to write packets to help expose holes
    // in our packet building logic.
    if (m_pDevice->Settings().cmdStreamEnableMemsetOnReserve)
    {
        memset(m_pReserveBuffer,
               m_pDevice->Settings().cmdStreamMemsetValue,
               m_reserveLimit * sizeof(uint32));
    }
#endif

    PAL_ASSERT(m_pReserveBuffer != nullptr);
    return m_pReserveBuffer;
}

// =====================================================================================================================
// Returns a pointer to enough memory to store m_reserveLimit DWORDs of commands.
uint32* CmdStream::ReserveCommandsInNewChunk()
{
#if PAL_ENABLE_PRINTS_ASSERTS
    // It's not legal to call ReserveCommands twice in a row.
    PAL_ASSERT(m_isReserved == false);
    m_isReserved = true;
#endif

    CmdStreamChunk* pChunk = GetNextChunk(m_reserveLimit);

    // Record that the tail object in our chunk list has less space available than it did before.
    m_chunkDwordsAvailable -= m_reserveLimit;

    // Preemptively allocate enough space from a new chunk to store all commands the caller could write.
    m_pReserveBuffer = pChunk->GetSpace(m_reserveLimit);

#if PAL_ENABLE_PRINTS_ASSERTS
    // Debug builds can memset all command space before the caller has a chance to write packets to help expose holes
    // in our packet building logic.
    if (m_pDevice->Settings().cmdStreamEnableMemsetOnReserve)
    {
        memset(m_pReserveBuffer,
               m_pDevice->Settings().cmdStreamMemsetValue,
               m_reserveLimit * sizeof(uint32));
    }
#endif

    PAL_ASSERT(m_pReserveBuffer != nullptr);
    return m_pReserveBuffer;
}

// =====================================================================================================================
// Concludes the previous ReserveCommands call by making sure the commands made it to chunk memory and resetting state.
void CmdStream::CommitCommands(
    const uint32* pEndOfBuffer)
{
#if PAL_ENABLE_PRINTS_ASSERTS
    // It's not legal to call CommitCommands before ReserveCommands.
    PAL_ASSERT(m_isReserved);
    m_isReserved = false;
#endif

    const uint32 dwordsUsed = static_cast<uint32>(pEndOfBuffer - m_pReserveBuffer);
    PAL_ASSERT(dwordsUsed <= m_reserveLimit);

#if PAL_ENABLE_PRINTS_ASSERTS
    // If commit size logging is enabled, make the appropriate call to the allocator to update its histogram.
    if (m_pDevice->Settings().logCmdBufCommitSizes)
    {
        m_pCmdAllocator->LogCommit(m_engineType, (m_subEngineType == SubEngineType::ConstantEngine), dwordsUsed);
    }
#endif

    // We must have already done an AllocCommandSpace call so we just need to reclaim any unused space.
    ReclaimCommandSpace(m_reserveLimit - dwordsUsed);

    // Technically this pointer is invalid now.
    m_pReserveBuffer = nullptr;
}

// =====================================================================================================================
// Returns a pointer to chunk command space that can hold commands of the given size. This may cause the command stream
// to switch to a new chunk if the current chunk does not have enough free space.
uint32* CmdStream::AllocCommandSpace(
    uint32 sizeInDwords)
{
    CmdStreamChunk*const pChunk = GetChunk(sizeInDwords);

    // Record that the tail object in our chunk list has less space available than it did before.
    m_chunkDwordsAvailable -= sizeInDwords;

    return pChunk->GetSpace(sizeInDwords);
}

// =====================================================================================================================
// Signals that the caller asked for more chunk command space than necessary, up to sizeInDwords at the end of the
// previous allocation can be reclaimed. If the allocate call caused us to roll to a new chunk any unused space at the
// end of the previous chunk cannot be reclaimed.
void CmdStream::ReclaimCommandSpace(
    uint32 sizeInDwords)
{
    // Because AllocCommandSpace guarantees that the caller gets a block of memory the size it asked for, we can just
    // add our argument to m_chunkDwordsAvaliable and no matter what we will have reclaimed the extra space.
    m_chunkDwordsAvailable += sizeInDwords;

    // We need to do the same with the active chunk or we will end up with gaps in our command stream.
    m_chunkList.Back()->ReclaimSpace(sizeInDwords);
}

// =====================================================================================================================
// Verifies that the current chunk has enough free space for an allocation of the given size. It will obtain a new chunk
// if more space is needed. Returns true if it didn't get a new chunk.
bool CmdStream::ValidateCommandSpace(
    uint32 sizeInDwords)
{
    auto*const pCurChunk = m_chunkList.IsEmpty() ? nullptr : m_chunkList.Back();
    auto*const pGetChunk = GetChunk(sizeInDwords);

    return pCurChunk == pGetChunk;
}

// =====================================================================================================================
// Returns a new chunk by first searching the retained chunk list for a valid chunk then querying the command allocator
// if there are no retained chunks available.
CmdStreamChunk* CmdStream::GetNextChunk(
    uint32 numDwords)
{
    CmdStreamChunk* pChunk = nullptr;

    if (m_status == Result::Success)
    {
        // First search the retained chunk list
        if (m_retainedChunkList.IsEmpty() == false)
        {
            // The command allocator always allocates uniformly-sized chunks, so any retained chunk should be big enough
            m_retainedChunkList.PopBack(&pChunk);
        }

        // If a retained chunk could not be found then allocate a new one from the command allocator
        if (pChunk == nullptr)
        {
            // It's either the first time we're requesting space for this stream, or the "most recent" chunk for this
            // stream doesn't have enough space to accomodate this request.  Either way, we need to obtain a new chunk.
            // The allocator adds a reference for us automatically. If the chunk list is empty, then the new chunk will
            // be the root.
            m_status = m_pCmdAllocator->GetNewChunk(CommandDataAlloc, (m_flags.buildInSysMem != 0), &pChunk);

            if (m_status != Result::Success)
            {
                // Something bad happen and the stream will always be in error status ever after
                PAL_ALERT_ALWAYS();
            }
            else
            {
                // Make sure that the start address of this chunk work with the requirements of this command stream if
                // the stream isn't being assembled in system memory.
                PAL_ASSERT(pChunk->UsesSystemMemory() || IsPow2Aligned(pChunk->GpuVirtAddr(), m_startAlignBytes));
            }
        }
    }

    PAL_ASSERT((pChunk != nullptr) || (m_status != Result::Success));

    if (m_chunkList.IsEmpty() == false)
    {
        // If we have a valid current chunk we must end it to do things like fill out the postamble.
        EndCurrentChunk(false);

        // Add in the total DWORDs allocated to compute an upper-bound on total command size.
        m_totalChunkDwords += m_chunkList.Back()->DwordsAllocated();
    }
    else if ((m_status == Result::Success) && m_pCmdAllocator->TrackBusyChunks())
    {
        // This is the first chunk in the list so we have to initialize the busy tracker
        const Result result = pChunk->InitRootBusyTracker(m_pCmdAllocator);

        if (result != Result::Success)
        {
            // Something bad happen and the stream will always be in error status ever after
            PAL_ALERT_ALWAYS();

            m_status = result;
        }
    }

    if (m_status != Result::Success)
    {
        // Always pop up and use dummy chunk in error status.
        pChunk = m_pCmdAllocator->GetDummyChunk();

        // Make sure there is only one reference of dummy chunk at back of chunk list
        if (m_chunkList.Back() == pChunk)
        {
            m_chunkList.PopBack(nullptr);
        }

        pChunk->Reset(true);
    }

    // And just add this chunk to the end of our list.
    const Result result = m_chunkList.PushBack(pChunk);
    PAL_ASSERT(result == Result::Success);

    // And remember how much of this chunk is available, accounting for any potential padding and/or postamble.
    m_chunkDwordsAvailable = pChunk->DwordsRemaining() - m_cmdSpaceDwordPadding;

    // The chunk and command stream are now ready to allocate space so we can safely call BeginCurrentChunk to
    // possibly allocate a chunk preamble.
    BeginCurrentChunk();

    // It's possible for a client to request more command buffer space then what fits in a single chunk.
    // This is unsupported.
    PAL_ASSERT (numDwords <= m_chunkDwordsAvailable);

    return pChunk;
}

// =====================================================================================================================
// Returns a chunk that can accomodate the specified number of dwords.  A new chunk will be allocated if necessary.
CmdStreamChunk* CmdStream::GetChunk(
    uint32 numDwords)
{
    CmdStreamChunk* pChunk = nullptr;

    if (numDwords > m_chunkDwordsAvailable)
    {
        pChunk = GetNextChunk(numDwords);
    }
    else
    {
        // Ok, the chunk at the end of our chunk list has room to support this request, so just use that.
        pChunk = m_chunkList.Back();
    }

    return pChunk;
}

// =====================================================================================================================
// Resets a command stream to its default state.
void CmdStream::Reset(
    CmdAllocator* pNewAllocator,
    bool          returnGpuMemory)
{
    if (pNewAllocator == nullptr)
    {
        // We must have a command allocator or the command stream can't function. This function gets called at
        // CmdBuffer::Init and at CmdBuffer::Reset. The retained chunk list must be empty at Init while we better
        // have a valid allocator by now if we recorded commands in this stream.
        PAL_ASSERT(m_retainedChunkList.IsEmpty() || (m_pCmdAllocator != nullptr));
    }

    ResetNestedChunks();

    if (returnGpuMemory)
    {
        // The client requested that we return all chunks, add any remaining retained chunks to the chunk list so they
        // can be returned to the allocator with the rest.
        while (m_retainedChunkList.IsEmpty() == false)
        {
            CmdStreamChunk* pChunk = nullptr;
            m_retainedChunkList.PopBack(&pChunk);
            m_chunkList.PushBack(pChunk);
        }

        // Return all remaining chunks to the command allocator.
        if (m_chunkList.IsEmpty() == false)
        {
            for (auto iter = m_chunkList.Begin(); iter.IsValid(); iter.Next())
            {
                iter.Get()->RemoveCommandStreamReference();
            }
            // if any error occurs, the memory allocated before might be useless and need to be free.
            if (m_status == Result::Success)
            {
                m_pCmdAllocator->ReuseChunks(CommandDataAlloc, (m_flags.buildInSysMem != 0), m_chunkList.Begin());
            }
        }
    }
    else
    {
        // Reset the chunks to be retained and add them to the retained list.
        for (auto iter = m_chunkList.Begin(); iter.IsValid(); iter.Next())
        {
            iter.Get()->Reset(false);
            m_retainedChunkList.PushBack(iter.Get());
        }
    }

    // We own zero chunks and have zero DWORDs available.
    m_chunkList.Clear();
    m_chunkDwordsAvailable   = 0;
    m_totalChunkDwords       = 0;
    m_flags.addressDependent = 0;

    if ((pNewAllocator != nullptr) && (pNewAllocator != m_pCmdAllocator))
    {
        // It is illegal to switch the allocator while retaining chunks.
        PAL_ASSERT(returnGpuMemory == true);

        // Switch to the new command allocator.
        m_pCmdAllocator = pNewAllocator;
    }

    if (m_pCmdAllocator != nullptr)
    {
        // The reserve limit cannot be larger than the chunk size minus the padding space. Reserve limits up to
        // 1024 DWORDs will always be OK; anything larger is at the mercy of the client's suballocation size.
        PAL_ASSERT(m_reserveLimit <=
            (m_pCmdAllocator->ChunkSize(CommandDataAlloc) / sizeof(uint32)) - m_cmdSpaceDwordPadding);
    }

    // It's not legal to use this allocator now that command building is over. We make no attempt to rewind the
    // allocator because that must be managed by our parent command buffer.
    m_pMemAllocator = nullptr;
}

// =====================================================================================================================
// A basic implementation of End. If any IP-specific subclasses need more functionality (e.g., chaining) they should
// override EndCurrentChunk.
Result CmdStream::End()
{
    if (IsEmpty() == false)
    {
        // only if no error, the command stream is valid.
        if (m_status == Result::Success)
        {
            // End the last chunk in the command stream.
            EndCurrentChunk(true);

            // Add in the total DWORDs allocated to compute an upper-bound on total command size.
            m_totalChunkDwords += m_chunkList.Back()->DwordsAllocated();
        }

        CmdStreamChunk* pRootChunk = m_chunkList.Front();
#if PAL_ENABLE_PRINTS_ASSERTS
        // Save the root chunk's generation for checking if command allocator was reset before submit.
        m_streamGeneration = pRootChunk->GetGeneration();
#endif

        // Walk through our chunk list and finalize all chunks.
        for (auto iter = m_chunkList.Begin(); iter.IsValid(); iter.Next())
        {
            auto*const pChunk = iter.Get();

            // This implementation doesn't do any padding so this better be true.
            PAL_ASSERT(Pow2Align(pChunk->DwordsAllocated(), m_sizeAlignDwords) == pChunk->DwordsAllocated());
            // Update the root info for each chunk of the command stream.
            pChunk->UpdateRootInfo(pRootChunk);

            // The chunk is complete and ready for submission.
            pChunk->FinalizeCommands();
        }
    }

    // Destroy anything allocated using m_pMemAllocator.
    CleanupTempObjects();

    // It's not legal to use this allocator now that command building is over. We make no attempt to rewind the
    // allocator because that must be managed by our parent command buffer.
    m_pMemAllocator = nullptr;
    return m_status;
}

// =====================================================================================================================
void CmdStream::Call(
    const CmdStream& targetStream,
    bool             exclusiveSubmit,      // If the target stream belongs to a cmd buffer with this option enabled!
    bool             allowIb2Launch)       // Ignored by the base implementation, because most engines don't support
                                           // command chaining or IB2's.
{
    if (targetStream.IsEmpty() == false)
    {
        PAL_ASSERT(m_pCmdAllocator->ChunkSize(CommandDataAlloc) >= targetStream.GetFirstChunk()->Size());

        for (auto chunkIter = targetStream.GetFwdIterator(); chunkIter.IsValid(); chunkIter.Next())
        {
            const auto*const pChunk = chunkIter.Get();
            const uint32 sizeInDwords = pChunk->CmdDwordsToExecute();

            uint32*const pCmdSpace = AllocCommandSpace(sizeInDwords);
            memcpy(pCmdSpace, pChunk->CpuAddr(), (sizeof(uint32) * sizeInDwords));
        }
    }
}

// =====================================================================================================================
// Increments the submission count of the first command chunk contained in this stream along with the submit counts for
// any nested chunks referenced by this command stream.
void CmdStream::IncrementSubmitCount()
{
    if (IsEmpty() == false)
    {
#if PAL_ENABLE_PRINTS_ASSERTS
        // Verfiy that the root chunk's generation hasn't changed.
        PAL_ASSERT(m_streamGeneration == m_chunkList.Front()->GetGeneration());
#endif

        m_chunkList.Front()->IncrementSubmitCount();
    }

    // Increment the submit counts for every nested commmand buffer chunk by the number of times it was executed as
    // part of this command stream.
    for (auto iter = m_nestedChunks.Begin(); iter.Get() != nullptr; iter.Next())
    {
#if PAL_ENABLE_PRINTS_ASSERTS
        // Compare each chunk's submit time generation with its call time generation. See TrackNestedChunks.
        PAL_ASSERT(iter.Get()->key->GetGeneration() == iter.Get()->value.recordedGeneration);
#endif

        iter.Get()->key->IncrementSubmitCount(iter.Get()->value.executeCount);
    }
}

// =====================================================================================================================
// Helper method which "tracks" a nested command buffer's command or data chunks by adding them to a hash-table. The
// table maps chunk objects to the number of times in this command stream that chunk was executed in this command
// stream. It is expected that this will only be called for non-empty nested command streams.
void CmdStream::TrackNestedChunks(
    const ChunkRefList& chunkList)
{
    PAL_ASSERT(chunkList.IsEmpty() == false);

    auto chunkIter = chunkList.Begin();

    // Perform a hash lookup on the first chunk in the target list to determine whether or not this is the first time
    // that the target stream is being "called" from this stream.
    bool    existed             = false;
    NestedChunkData* pChunkData = nullptr;
    const Result result = m_nestedChunks.FindAllocate(chunkIter.Get(), &existed, &pChunkData);
    PAL_ASSERT(result == Result::Success);

    if (existed == false)
    {
        // The target command stream has not been "called" from this command stream before, so initialize its
        // executed-count to one. FindAllocate() will have already created space for this chunk in the table.
        chunkIter.Get()->AddNestedCommandStreamReference();
        pChunkData->executeCount = 1;

#if PAL_ENABLE_PRINTS_ASSERTS
        pChunkData->recordedGeneration = chunkIter.Get()->GetGeneration();
#endif

        // Furthermore, we also need to add the target stream's other chunks into our table. They each receive an
        // execute-count of zero to indicate that they aren't the first chunk in any command stream.
        NestedChunkData nonFirstChunkData = {};
        CmdStreamChunk* pNestedChunk      = nullptr;
        for (chunkIter.Next(); chunkIter.IsValid(); chunkIter.Next())
        {
            pNestedChunk = chunkIter.Get();
#if PAL_ENABLE_PRINTS_ASSERTS
            nonFirstChunkData.recordedGeneration = pNestedChunk->GetGeneration();
#endif

            pNestedChunk->AddNestedCommandStreamReference();
            m_nestedChunks.Insert(pNestedChunk, nonFirstChunkData);
        }
    }
    else
    {
        // The target command stream has indeed been "called" before from this command stream, so increment its
        // execute-count to reflect the total number of calls. There is no need to update the counters for the
        // other chunks, since those non-first chunks need to keep a count of zero.
        ++pChunkData->executeCount;
    }
}

// =====================================================================================================================
// Helper method which tracks a nested command buffer's command chunks by adding them to the hash table of chunks.
void CmdStream::TrackNestedCommands(
    const CmdStream& targetStream)
{
    if (targetStream.m_chunkList.IsEmpty() == false)
    {
        TrackNestedChunks(targetStream.m_chunkList);
    }
}

// =====================================================================================================================
// Helper method which tracks a nested command buffer's embedded-data chunks by adding them to the hash table of chunks
// used by the command stream's command chunks. This must be called on precisely ONE of a command buffer's command
// streams!
void CmdStream::TrackNestedEmbeddedData(
    const ChunkRefList& dataChunkList)
{
    if (dataChunkList.IsEmpty() == false)
    {
        TrackNestedChunks(dataChunkList);
    }
}

// =====================================================================================================================
void CmdStream::ResetNestedChunks()
{
    if (m_nestedChunks.GetNumEntries() != 0)
    {
        // NOTE: On DX12 builds it is unsafe for us to release our references on nested command-stream chunks, so we
        // have to just drop the contents of the nested-chunk table on the floor. See CmdStreamChunk::ResetBusyTracker()
        // for more details.
        for (auto iter = m_nestedChunks.Begin(); iter.Get() != nullptr; iter.Next())
        {
            CmdStreamChunk*const pChunk = iter.Get()->key;
            PAL_ASSERT(pChunk != nullptr);

            pChunk->RemoveCommandStreamReference();
        }

        m_nestedChunks.Reset();
    }
}

// =====================================================================================================================
// Returns the current GPU VA of this stream.
gpusize CmdStream::GetCurrentGpuVa()
{
    gpusize gpuVa = {};

    // It's illegal to call this funciton if the command stream is empty.
    PAL_ASSERT(IsEmpty() == false);

    CmdStreamChunk*const pChunk = GetChunk(0);

    pChunk->GetSpace(0, &gpuVa);

    return gpuVa;
}

#if PAL_ENABLE_PRINTS_ASSERTS
// =====================================================================================================================
// Saves all the command data associated with this stream to the file pointed to by pFile.
//
// It is the callers responsibility to verify that pFile is pointing to an open file. "pHeader" should point to a
// string of the format "text = ". It will be appended with the number of DWORDs associated with this stream.
void CmdStream::DumpCommands(
    File*            pFile,          // [in] pointer to an opened file object
    const char*      pHeader,        // [in] pointer to a string that contains header info
    CmdBufDumpFormat mode            // [in] true if it's binary dump requested
    ) const
{
    Result result = Result::Success;

    if (mode == CmdBufDumpFormat::CmdBufDumpFormatText)
    {
        // Compute the size of all data associated with this stream.
        uint64 streamSizeInDwords = 0;
        for (auto iter = m_chunkList.Begin(); iter.IsValid(); iter.Next())
        {
            streamSizeInDwords += iter.Get()->DwordsAllocated();
        }

        // First, output the header information.
        constexpr size_t MaxLineSize = 128;
        char line[MaxLineSize];

        Snprintf(line, MaxLineSize, "%s%llu\n", pHeader, streamSizeInDwords);
        result = pFile->Write(line, strlen(line));
    }

    uint32 subEngineId = 0; // DE subengine ID

    if (m_subEngineType == SubEngineType::ConstantEngine)
    {
        if (m_cmdStreamUsage == CmdStreamUsage::Preamble)
        {
            subEngineId = 2; // CE preamble subengine ID
        }
        else
        {
            subEngineId = 1; // CE subengine ID
        }
    }
    else if (GetEngineType() == EngineType::EngineTypeDma)
    {
        subEngineId = 4; // SDMA engine ID
    }

    // Next, walk through all the chunks that make up this command stream and write their command to the file.
    for (auto iter = m_chunkList.Begin(); iter.IsValid() && (result == Result::Success); iter.Next())
    {
        result = iter.Get()->WriteCommandsToFile(pFile, subEngineId, mode);
    }

    // Don't bother returning an error if the command stream wasn't dumped correctly as we don't want this to affect
    // operation of the "important" stuff...  but still make it apparent that the dump file isn't accurate.
    PAL_ALERT(result != Result::Success);
}
#endif

// =====================================================================================================================
uint32 CmdStream::GetUsedCmdMemorySize() const
{
    gpusize runningTotalDw = TotalChunkDwords();
    if ((m_pMemAllocator != nullptr) && (m_chunkList.IsEmpty() == false))
    {
        // If the linear memory allocator is non-null, then this stream is still recording and we need to add the
        // current number of DWORDs in the (current) final chunk of the stream.
        runningTotalDw += m_chunkList.Back()->DwordsAllocated();
    }

    PAL_ASSERT(HighPart(sizeof(uint32) * runningTotalDw) == 0);
    return LowPart(sizeof(uint32) * runningTotalDw);
}

// =====================================================================================================================
Result CmdStream::TransferRetainedChunks(
    ChunkRefList* pDest)
{
    Result result = Result::Success;
    while ((m_retainedChunkList.IsEmpty() == false) && (result == Result::Success))
    {
        CmdStreamChunk* pChunk = nullptr;
        m_retainedChunkList.PopBack(&pChunk);
        pChunk->RemoveCommandStreamReference();
        result = pDest->PushBack(pChunk);

        // PushBack can fail if there's not enough space,
        // but since the DefaultCapacity of the Vector used in ChunkRefList is 16 entries,
        // this case we should never fail the call.
        PAL_ASSERT(result == Result::Success);
    }

    return result;
}

} // Pal
