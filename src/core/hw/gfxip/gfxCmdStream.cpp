/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/gfxCmdStream.h"
#include "palVectorImpl.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
GfxCmdStream::GfxCmdStream(
    const GfxDevice& device,
    ICmdAllocator*   pCmdAllocator,
    EngineType       engineType,
    SubEngineType    subEngineType,
    CmdStreamUsage   cmdStreamUsage,
    uint32           chainSizeInDwords,
    uint32           minNopSizeInDwords,
    uint32           condIndirectBufferSize,
    bool             isNested)
    :
    CmdStream(device.Parent(),
              pCmdAllocator,
              engineType,
              subEngineType,
              cmdStreamUsage,
              chainSizeInDwords,
              minNopSizeInDwords,
              isNested),
    m_device(device),
    m_chainIbSpaceInDwords(chainSizeInDwords),
    m_minNopSizeInDwords(minNopSizeInDwords),
    m_condIndirectBufferSize(condIndirectBufferSize),
    m_cmdBlockOffset(0),
    m_pTailChainLocation(nullptr),
    m_numCntlFlowStatements(0),
    m_numPendingChains(0)
{
    memset(m_cntlFlowStack, 0, sizeof(m_cntlFlowStack));
    memset(m_pendingChains, 0, sizeof(m_pendingChains));
}

// =====================================================================================================================
void GfxCmdStream::Reset(
    CmdAllocator* pNewAllocator,
    bool          returnGpuMemory)
{
    // Reset all tracked state.
    m_cmdBlockOffset        = 0;
    m_numCntlFlowStatements = 0;
    m_numPendingChains      = 0;
    m_pTailChainLocation    = nullptr;

    Pal::CmdStream::Reset(pNewAllocator, returnGpuMemory);
}

// =====================================================================================================================
// Called when the command stream's final chunk is complete so that we can fill out the tail chain postamble.
void GfxCmdStream::UpdateTailChainLocation(
    uint32* pTailChain)
{
    // Fill the tail with a NOP. It may be updated at submit to point to another command stream by PatchTailChain.
    BuildNop(m_chainIbSpaceInDwords, pTailChain);

    // The tail chain address must be based on the tail chunk's mapped CPU address because it will be modified after
    // the chunk is finalized. Unfortunately pTailChain is based on the chunk's write pointer which will be different
    // than the mapped pointer if staging buffers are enabled.
    auto*const   pTailChunk = m_chunkList.Back();
    const size_t tailOffset = pTailChain - pTailChunk->GetRmwWriteAddr();

    m_pTailChainLocation = pTailChunk->GetRmwCpuAddr() + tailOffset;
}

// =====================================================================================================================
// Add a new chaining packet patch request.
void GfxCmdStream::AddChainPatch(
    ChainPatchType type,
    void*          pChainPacket)
{
    PAL_ASSERT(m_numPendingChains < MaxChainPatches);

    m_pendingChains[m_numPendingChains].type    = type;
    m_pendingChains[m_numPendingChains].pPacket = pChainPacket;
    m_numPendingChains++;
}

// =====================================================================================================================
// Ends the current command block by reserving space for the requested postamble and servicing all pending chaining
// packet patch requests. Any necessary NOP padding will be added before the postamble. Returns a pointer to the
// beginning of the postamble command space.
//
// The caller must pass true for atEndOfChunk if it is ending the current command block so that it can chain to a new
// command chunk because the padding and postamble must be allocated out of the reserved padding space managed by the
// base class. In all other cases atEndOfChunk must be false.
//
// If no commands are written between two EndCommandBlock calls the second EndCommandBlock call will guarantee that its
// command block has a non-zero size.
uint32* GfxCmdStream::EndCommandBlock(
    uint32   postambleDwords,
    bool     atEndOfChunk,
    gpusize* pPostambleAddr) // [out,optional]: The postamble's virtual address will be stored here.
{
    // Compute some size measurements that we will use later on.
    uint32 paddingDwords = 0;
    uint32 allocDwords   = 0;
    uint32 totalDwords   = 0;
    ComputeCommandBlockSizes(postambleDwords, &paddingDwords, &allocDwords, &totalDwords);

    // When we're not using the reserved space, we must force the base class to chain to a new chunk if we can't fit the
    // padding and postamble in the current chunk. Order is important here: we only want to validate the space if
    // atEndOfChunk is false. Note that we must recompute our sizes if chaining did occur.
    if ((atEndOfChunk == false) && (ValidateCommandSpace(allocDwords) == false))
    {
        ComputeCommandBlockSizes(postambleDwords, &paddingDwords, &allocDwords, &totalDwords);
    }

    uint32*       pCmdSpace = nullptr;
    auto*const    pChunk    = m_chunkList.Back();
    const gpusize blockAddr = pChunk->GpuVirtAddr() + m_cmdBlockOffset * sizeof(uint32);

    if (allocDwords > 0)
    {
        if (pPostambleAddr != nullptr)
        {
            // The caller wants to know the postamble's virtual address.
            *pPostambleAddr = blockAddr + (totalDwords - postambleDwords) * sizeof(uint32);
        }

        // Allocate enough space for the padding and the postamble. If this command block is at the end of the command
        // chunk we are allocating this space out of the reserved padding space managed by the base class so we must
        // directly allocate it from the chunk. Otherwise we're still operating within the usual allocation scheme
        // managed by the base class so we should call AllocCommandSpace.
        pCmdSpace  = atEndOfChunk ? pChunk->GetSpace(allocDwords) : AllocCommandSpace(allocDwords);
        pCmdSpace += BuildNop(paddingDwords, pCmdSpace);

        // Verify that AllocCommandSpace didn't trigger a chunk roll (ValidateCommandSpace should have done it).
        PAL_ASSERT(pChunk == m_chunkList.Back());
    }

    // Signal to the chunk that we're done allocating command space for this block.
    pChunk->EndCommandBlock(postambleDwords);

    // Now we know the total length of this command block, padding and all. Service all outstanding patch requests.
    for (uint32 idx = 0; idx < m_numPendingChains; ++idx)
    {
        ChainPatch*  pPatch = &m_pendingChains[idx];

        if (pPatch->type == ChainPatchType::IndirectBuffer)
        {
            // By convention, chaining IBs are initially filled with a NOP so we have to build the whole packet.
            BuildIndirectBuffer(blockAddr,
                                totalDwords,
                                IsPreemptionEnabled(),
                                true,
                                static_cast<uint32*>(pPatch->pPacket));
        }
        else
        {
            // We don't know the format of a conditional indirect buffer packet here (only the address and size of
            // the buffer we're conditionally executing), so ask the HW-specific implementation to fill in the packet
            // details for us.
            PatchCondIndirectBuffer(pPatch, blockAddr, totalDwords);
        }
    } // end loop through all the chains

    // Any chains within a chunk create a dependency on the GPU virtual address of the chunk and thus we must notify
    // our command stream that we're address dependent. The end-of-chunk chain doesn't apply because that chain can be
    // stripped off of the chunk using the CmdDwordsToExecuteNoPostamble size.
    if (atEndOfChunk == false)
    {
        NotifyAddressDependent();
    }

    // Initialize the command block state for the next command block.
    m_cmdBlockOffset   = atEndOfChunk ? 0 : pChunk->DwordsAllocated();
    m_numPendingChains = 0;

    // We assumed that the size alignment is at least as strict as the start alignment
    PAL_ASSERT(IsPow2Aligned(m_cmdBlockOffset * sizeof(uint32), m_startAlignBytes));

    // Return a pointer to the postamble space.
    return pCmdSpace;
}

// =====================================================================================================================
// Computes a variety of sizes needed to end the current command block.
void GfxCmdStream::ComputeCommandBlockSizes(
    uint32  postambleDwords,
    uint32* pPaddingDwords,  // [out] NOP padding required to align the command block.
    uint32* pAllocDwords,    // [out] How much command space must be allocated for the padding and postamble.
    uint32* pTotalDwords     // [out] The total size of the command block including the padding and postamble.
    ) const
{
    // Compute the size of this command block (including the postamble) and the padding required to align it.
    const uint32 dwordsUsed    = m_chunkList.Back()->DwordsAllocated() + postambleDwords - m_cmdBlockOffset;
    uint32       paddingDwords = Util::Pow2Align(dwordsUsed, m_sizeAlignDwords) - dwordsUsed;
    uint32       totalDwords   = dwordsUsed + paddingDwords;

    // Increase the padding to the next highest alignment value if:
    // - The total block size is zero because it's illegal to chain to zero commands.
    // - We need to insert padding but the min NOP size is too big.
    if ((totalDwords == 0) || ((paddingDwords > 0) && (paddingDwords < m_minNopSizeInDwords)))
    {
        // This must be true otherwise the "totalDwords == 0" case will fail to write a valid NOP packet.
        PAL_ASSERT(m_sizeAlignDwords >= m_minNopSizeInDwords);

        paddingDwords += m_sizeAlignDwords;
        totalDwords   += m_sizeAlignDwords;
    }

    // We must allocate some command space for the padding and postamble; note that it is possible for allocDwords to be
    // zero in which case we don't need any NOP padding and the caller doesn't want a postamble.
    const uint32 allocDwords = paddingDwords + postambleDwords;

    // Save out our "return" values.
    *pPaddingDwords = paddingDwords;
    *pAllocDwords   = allocDwords;
    *pTotalDwords   = totalDwords;
}

// =====================================================================================================================
// Begins an if-statement. Subsequent commands will only be executed if the condition is true.
void GfxCmdStream::If(
    CompareFunc compareFunc,
    gpusize     compareGpuAddr,
    uint64      data,
    uint64      mask)
{
    // Terrible things will happen if the caller goes past our nesting limit.
    PAL_ASSERT(m_numCntlFlowStatements < CntlFlowNestingLimit);

    // The PM4 optimizer has no understanding of control flow. Just turn off optimization when we start using it.
    if (m_flags.optimizeCommands == 1)
    {
        PAL_ALERT_ALWAYS_MSG("PM4 Optimizer has no understanding of flow control.  Optimization is being disabled to"
                             " prevent issues.");
        m_flags.optimizeCommands = 0;
    }

    // The CP has no equivalent to CompareFunc::Never so we need to use CompareFunc::Always and swap the branches.
    ChainPatchType passPatchType = ChainPatchType::CondIndirectBufferPass;
    ChainPatchType failPatchType = ChainPatchType::CondIndirectBufferFail;
    if (compareFunc == CompareFunc::Never)
    {
        compareFunc   = CompareFunc::Always;
        passPatchType = ChainPatchType::CondIndirectBufferFail;
        failPatchType = ChainPatchType::CondIndirectBufferPass;
    }

    // Give the caller the pointer to the end of this command block so that they can insert a conditional indirect
    // buffer packet which will evaluate our comparison.
    uint32*const pCondIbPacket = EndCommandBlock(m_condIndirectBufferSize, false);
    BuildCondIndirectBuffer(compareFunc, compareGpuAddr, data, mask, pCondIbPacket);

    // If the if-check passes we want to branch to the new command block.
    AddChainPatch(passPatchType, pCondIbPacket);

    // Push some data about this if-statement onto the control flow stack; we will patch the failure path later on.
    m_cntlFlowStack[m_numCntlFlowStatements].phase          = CntlFlowPhase::If;
    m_cntlFlowStack[m_numCntlFlowStatements].phasePatchType = failPatchType;
    m_cntlFlowStack[m_numCntlFlowStatements].pPhasePacket   = pCondIbPacket;
    m_numCntlFlowStatements++;
}

// =====================================================================================================================
// Ends the current if-case and starts an else-case. Subsequent commands will only be executed if the if-statement's
// condition is false.
void GfxCmdStream::Else()
{
    const uint32 stackIdx = m_numCntlFlowStatements - 1;

    // Terrible things will happen if the caller hasn't previously put us in CntlFlowPhase::If.
    PAL_ASSERT((m_numCntlFlowStatements > 0) && (m_cntlFlowStack[stackIdx].phase == CntlFlowPhase::If));

    // End the current command block with a chaining packet so we can jump out of the if-case.
    uint32*const pChainPacket = EndCommandBlock(m_chainIbSpaceInDwords, false);
    BuildNop(m_chainIbSpaceInDwords, pChainPacket);

    // Patch up the packet we stored earlier; in most cases this makes a failed if-check jump to the new command block.
    AddChainPatch(m_cntlFlowStack[stackIdx].phasePatchType, m_cntlFlowStack[stackIdx].pPhasePacket);

    // Transition to the else phase: store a pointer to the above chaining packet so we can patch it later on.
    m_cntlFlowStack[stackIdx].phase          = CntlFlowPhase::Else;
    m_cntlFlowStack[stackIdx].phasePatchType = ChainPatchType::IndirectBuffer;
    m_cntlFlowStack[stackIdx].pPhasePacket   = pChainPacket;
}

// =====================================================================================================================
// Terminates an if-statement. Subsequent commands will be unconditionally executed (unless this is a nested control
// flow statement).
void GfxCmdStream::EndIf()
{
    const uint32 stackIdx = m_numCntlFlowStatements - 1;

    // Terrible things will happen if the caller hasn't previously put us in CntlFlowPhase::If or CntlFlowPhase::Else.
    PAL_ASSERT((m_numCntlFlowStatements > 0) &&
               ((m_cntlFlowStack[stackIdx].phase == CntlFlowPhase::If) ||
                (m_cntlFlowStack[stackIdx].phase == CntlFlowPhase::Else)));

    /// End the current command block with a chaining packet so we can jump out of this control flow block.
    uint32*const pChainPacket = EndCommandBlock(m_chainIbSpaceInDwords, false);
    BuildNop(m_chainIbSpaceInDwords, pChainPacket);

    AddChainPatch(ChainPatchType::IndirectBuffer, pChainPacket);

    // Patch up the packet we stored earlier. If we've built a "one-armed" if-statement this will patch the fail branch
    // to the new command block; otherwise it patches the end of the if-case to the new command block.
    AddChainPatch(m_cntlFlowStack[stackIdx].phasePatchType, m_cntlFlowStack[stackIdx].pPhasePacket);

    // We're done with this control flow statement so pop it off the stack.
    m_numCntlFlowStatements--;
}

// =====================================================================================================================
// Begins a while loop. Subsequent commands will be executed in a loop until the condition is false.
void GfxCmdStream::While(
    CompareFunc compareFunc,
    gpusize     compareGpuAddr,
    uint64      data,
    uint64      mask)
{
    // Terrible things will happen if the caller goes past our nesting limit.
    PAL_ASSERT(m_numCntlFlowStatements < CntlFlowNestingLimit);

    // The PM4 optimizer has no understanding of control flow. Just turn off optimization when we start using it.
    if (m_flags.optimizeCommands == 1)
    {
        PAL_ALERT_ALWAYS_MSG("PM4 Optimizer has no understanding of flow control.  Optimization is being disabled to"
                             " prevent issues.");
        m_flags.optimizeCommands = 0;
    }

    // The CP has no equivalent to CompareFunc::Never so we need to use CompareFunc::Always and swap the branches.
    ChainPatchType passPatchType = ChainPatchType::CondIndirectBufferPass;
    ChainPatchType failPatchType = ChainPatchType::CondIndirectBufferFail;
    if (compareFunc == CompareFunc::Never)
    {
        compareFunc   = CompareFunc::Always;
        passPatchType = ChainPatchType::CondIndirectBufferFail;
        failPatchType = ChainPatchType::CondIndirectBufferPass;
    }

    // We need to jump back to the while comparison at the end of the while loop. If we ask for a postamble that is
    // size-aligned we will be able to jump to it without worrying about alignment issues.
    uint32  paddingDwords = Util::Pow2Align(m_condIndirectBufferSize, m_sizeAlignDwords) - m_condIndirectBufferSize;

    if ((paddingDwords > 0) && (paddingDwords < m_minNopSizeInDwords))
    {
        // We need to insert padding but the min NOP size is too big.
        paddingDwords += m_sizeAlignDwords;
    }

    gpusize      postambleAddr   = 0;
    const uint32 postambleDwords = paddingDwords + m_condIndirectBufferSize;
    uint32*      pCmdSpace       = EndCommandBlock(postambleDwords, false, &postambleAddr);

    pCmdSpace += BuildNop(paddingDwords, pCmdSpace);
    BuildCondIndirectBuffer(compareFunc, compareGpuAddr, data, mask, pCmdSpace);

    // If the loop condition passes we want to branch to the new command block.
    AddChainPatch(passPatchType, pCmdSpace);

    // Push some data about this while-statement onto the control flow stack; we will patch the failure path later on.
    m_cntlFlowStack[m_numCntlFlowStatements].phase             = CntlFlowPhase::While;
    m_cntlFlowStack[m_numCntlFlowStatements].phasePatchType    = failPatchType;
    m_cntlFlowStack[m_numCntlFlowStatements].pPhasePacket      = pCmdSpace;
    m_cntlFlowStack[m_numCntlFlowStatements].whileChainGpuAddr = postambleAddr;
    m_cntlFlowStack[m_numCntlFlowStatements].whileChainSize    = postambleDwords;
    m_numCntlFlowStatements++;
}

// =====================================================================================================================
// Terminates a while loop. Subsequent commands will be unconditionally executed (unless this is a nested control
// flow statement).
void GfxCmdStream::EndWhile()
{
    const uint32          stackIdx = m_numCntlFlowStatements - 1;
    const CntlFlowFrame*  pFrame   = &m_cntlFlowStack[stackIdx];

    // Terrible things will happen if the caller hasn't previously put us in CntlFlowPhase::While.
    PAL_ASSERT((m_numCntlFlowStatements > 0) && (pFrame->phase == CntlFlowPhase::While));

    // End the current command block with a chaining packet so we can jump back to the beginning of the while loop.
    uint32*const pChainPacket = EndCommandBlock(m_chainIbSpaceInDwords, false);

    // We already know everything about our chain destination so just build the chaining packet directly.
    BuildIndirectBuffer(pFrame->whileChainGpuAddr,
                        pFrame->whileChainSize,
                        IsPreemptionEnabled(),
                        true,
                        pChainPacket);

    // Patch up the packet we stored earlier; in most cases this makes the while jump to the new command block.
    AddChainPatch(pFrame->phasePatchType, pFrame->pPhasePacket);

    // We're done with this control flow statement so pop it off the stack.
    m_numCntlFlowStatements--;
}

// =====================================================================================================================
// Specialized implementation of "Call" for GFXIP command streams.  This will attempt to use either an IB2 packet or
// take advantage of command buffer chaining instead of just copying the callee's command stream contents into this
// stream.
void GfxCmdStream::Call(
    const CmdStream& targetStream,
    bool             exclusiveSubmit,      // If the target stream belongs to a cmd buffer with this option enabled!
    bool             allowIb2Launch)
{
    const auto& gfxStream = static_cast<const GfxCmdStream&>(targetStream);

    if (targetStream.IsEmpty() == false)
    {
        // The following are some sanity checks to make sure that the caller and callee are compatible.
        PAL_ASSERT((gfxStream.m_chainIbSpaceInDwords == m_chainIbSpaceInDwords) ||
                   (gfxStream.m_chainIbSpaceInDwords == 0));
        PAL_ASSERT(m_pCmdAllocator->ChunkSize(CommandDataAlloc) >= targetStream.GetFirstChunk()->Size());

        // If this command stream is preemptible, PAL assumes that the target command stream to also be preemptible.
        PAL_ASSERT(IsPreemptionEnabled() == targetStream.IsPreemptionEnabled());

        if (allowIb2Launch)
        {
            PAL_ASSERT(GetEngineType() != EngineTypeCompute);

            // The simplest way of "calling" a nested command stream is to use an IB2 packet, which tells the CP to
            // go execute the indirect buffer and automatically return to the call site. However, compute queues do
            // not support IB2 packets.
            if (gfxStream.m_chainIbSpaceInDwords == 0)
            {
                for (auto chunkIter = targetStream.GetFwdIterator(); chunkIter.IsValid(); chunkIter.Next())
                {
                    // Note: For nested command buffer which don't support chaining, we need to issue a separate IB2
                    // packet for each chunk.
                    const auto*const pChunk     = chunkIter.Get();
                    uint32*const     pIb2Packet = AllocCommandSpace(m_chainIbSpaceInDwords);
                    BuildIndirectBuffer(pChunk->GpuVirtAddr(),
                                        pChunk->CmdDwordsToExecute(),
                                        targetStream.IsPreemptionEnabled(),
                                        false,
                                        pIb2Packet);
                }
            }
            else
            {
                const auto*const pJumpChunk = targetStream.GetFirstChunk();
                uint32*const     pIb2Packet = AllocCommandSpace(m_chainIbSpaceInDwords);
                BuildIndirectBuffer(pJumpChunk->GpuVirtAddr(),
                                    pJumpChunk->CmdDwordsToExecute(),
                                    targetStream.IsPreemptionEnabled(),
                                    false,
                                    pIb2Packet);
            }
        }
        else if (exclusiveSubmit && (m_chainIbSpaceInDwords != 0) && (gfxStream.m_chainIbSpaceInDwords != 0))
        {
            // NOTE: To call a command stream which supports chaining and has the exclusive submit optmization enabled,
            // we only need to jump to the callee's first chunk, and then jump back here when the callee finishes.

            if (IsEmpty())
            {
                // The call to EndCommandBlock() below will not succeed if this command stream is currently empty. Add
                // the smallest-possible NOP packet to prevent the stream from being empty.
                uint32*const pNopPacket = AllocCommandSpace(m_minNopSizeInDwords);
                BuildNop(m_minNopSizeInDwords, pNopPacket);
            }

            // End our current command block, using the jump to the callee's first chunk as our block postamble.
            const auto*const pJumpChunk   = targetStream.GetFirstChunk();
            uint32*const     pChainPacket = EndCommandBlock(m_chainIbSpaceInDwords, false);
            BuildIndirectBuffer(pJumpChunk->GpuVirtAddr(),
                                pJumpChunk->CmdDwordsToExecute(),
                                targetStream.IsPreemptionEnabled(),
                                true,
                                pChainPacket);

            // Returning to the call site requires patching the callee's tail-chain with a packet which brings us
            // back here. However, we need to know the size of the current command block in order to fully construct
            // a chaining packet. So, the solution is to add a chain patch at the callee's tail-chain location which
            // will correspond to the current block.

            // NOTE: The callee's End() method was called after it was done being recorded. That call already built
            // us a dummy NOP packet at the tail-chain location, so we don't need to build a new one at this time!
            AddChainPatch(ChainPatchType::IndirectBuffer, gfxStream.m_pTailChainLocation);
        }
        else
        {
            // NOTE: The target command stream either doesn't have the exclusive submit optimization turned on, or
            // does not support chaining. In either case, we just simply walk over the target's command chunks, and
            // copy their contents into this stream (effectively making this an "inline" call).
            for (auto chunkIter = targetStream.GetFwdIterator(); chunkIter.IsValid(); chunkIter.Next())
            {
                const auto*const pChunk = chunkIter.Get();
                const uint32 sizeInDwords = (pChunk->CmdDwordsToExecute() - gfxStream.m_chainIbSpaceInDwords);

                uint32*const pCmdSpace = AllocCommandSpace(sizeInDwords);
                memcpy(pCmdSpace, pChunk->CpuAddr(), (sizeof(uint32) * sizeInDwords));
            }
        }
    }
}

// =====================================================================================================================
// Uses command buffer chaining to "execute" a series of GPU-generated command chunks. All chunks starting at the given
// iterator until the end of whichever list it belongs to are chained together. Additionally, the final chunk chains
// back to the normal command stream so that future commands can be recorded as though nothing was chained.
void GfxCmdStream::ExecuteGeneratedCommands(
    ChunkRefList::Iter chunkIter)   // Points to the first command chunk containing GPU-generated commands
{
    PAL_ASSERT(chunkIter.IsValid()); // It is illegal to execute zero generated command chunks!

    // This operation is only intended for use on command streams which support command-chunk chaining.
    PAL_ASSERT(m_chainIbSpaceInDwords != 0);
    const uint32 postambleDwords = m_chainIbSpaceInDwords;

    // End our current command block, using the jump to the first executed chunk as our block postamble. The
    // chain location will be filled with a chain packet to that chunk in the loop below.
    uint32* pChainPacket = EndCommandBlock(postambleDwords, false);

    do
    {
        // Fill the chain packet location with a jump to the next command chunk which was generated by the GPU.
        BuildIndirectBuffer(chunkIter.Get()->GpuVirtAddr(),
                            chunkIter.Get()->CmdDwordsToExecute(),
                            IsPreemptionEnabled(),
                            true,
                            pChainPacket);

        // NOTE: The call to PrepareChunkForCmdGeneration() reserves enough space at the end of the chunk for a
        // chain packet by writing an equally-sized NOP before the GPU generated the actual meat of this command
        // chunk. We just have to update our chain packet location for the next run of the loop. The chain packet
        // should be the very last item in the command buffer, following any padding for size-alignment.
        pChainPacket = (chunkIter.Get()->GetRmwWriteAddr() + chunkIter.Get()->CmdDwordsToExecute() - postambleDwords);

        chunkIter.Next();
    }
    while (chunkIter.IsValid());

    // NOTE: As mentioned above, the last chunk being executed already has a reserved location for a chain packet
    // which is needed to jump back to the main command stream.
    AddChainPatch(ChainPatchType::IndirectBuffer, pChainPacket);
}

// =====================================================================================================================
// Prepares a blank command-stream chunk for use as the target for GPU-generated commands, including adding the correct
// amount of padding "after" the generated commands. Returns the number of GPU-generated commands which will safely fit
// in the chunk.
uint32 GfxCmdStream::PrepareChunkForCmdGeneration(
    CmdStreamChunk* pChunk,
    uint32          cmdBufStride,       // In dwords
    uint32          embeddedDataStride, // In dwords
    uint32          maxCommands
    ) const
{
    // This operation is only intended for use on command streams which support command-chunk chaining.
    PAL_ASSERT(m_chainIbSpaceInDwords != 0);
    const uint32 postambleDwords = m_chainIbSpaceInDwords;

    // Compute the total number of command-chunk dwords each generated command will need. This is simply the sum of
    // the embedded data and command buffer requirements because we assume a one-dword alignment for embedded data.
    const uint32 dwordsPerCommand = (cmdBufStride + embeddedDataStride);

    // Determine the maximum number of commands we can fit into this chunk, assuming no padding is necessary.
    uint32 commandCount  = Min(maxCommands, ((pChunk->SizeDwords() - postambleDwords) / dwordsPerCommand));
    uint32 dwordsInChunk = (commandCount * dwordsPerCommand);

    // Compute the padding requirements. If the padding is below the minimum NOP size, we need to bump the padding up
    // to the next full size alignment.
    uint32 paddingDwords = (Pow2Align(dwordsInChunk + postambleDwords, m_sizeAlignDwords) -
                                      (dwordsInChunk + postambleDwords));
    if ((paddingDwords > 0) && (paddingDwords < m_minNopSizeInDwords))
    {
        paddingDwords += m_sizeAlignDwords;
    }

    // However, if the padding was increased because of the minimum NOP size, its possible for us to have run over
    // the chunk's capacity.
    if ((dwordsInChunk + postambleDwords + paddingDwords) > pChunk->SizeDwords())
    {
        // If this happens, we'll need to execute one fewer command so that the padding can fit.
        --commandCount;
        dwordsInChunk -= dwordsPerCommand;

        // Recompute the padding requirements since the dwords-per-command might not be aligned to the chunks size
        // alignment requirements.
        paddingDwords = (Pow2Align(dwordsInChunk + postambleDwords, m_sizeAlignDwords) -
                                   (dwordsInChunk + postambleDwords));
        if ((paddingDwords > 0) && (paddingDwords < m_minNopSizeInDwords))
        {
            paddingDwords += m_sizeAlignDwords;
            PAL_ASSERT((dwordsInChunk + paddingDwords) <= pChunk->SizeDwords());
        }
    }

    // The caller will allocate the entire chunk of embedded data space the chunk will need for all of the generated
    // commands. We assume a one dword alignment for this data, which makes computations simpler. If this assumption
    // ever changes, the arithmetic above to compute the number of commands which will fit would also need to change.
    const uint32 embeddedDataDwords = (embeddedDataStride * commandCount);
    PAL_ASSERT(pChunk->ComputeSpaceSize(embeddedDataDwords, 1u) == embeddedDataDwords);

    // Finally, allocate enough command space for the generated commands and any required padding and postamble, and
    // fill out the NOP packet for the padding and postamble (if present). We use a NOP packet for the postamble so
    // that if the postamble is not actually needed at command-generation time, the command space allocated for it
    // is initialized to something the CP can understand.
    const uint32 commandDwords = (cmdBufStride * commandCount);
    uint32*      pCmdSpace     = pChunk->GetSpace(commandDwords + postambleDwords + paddingDwords);

    pCmdSpace += commandDwords;
    pCmdSpace += BuildNop(paddingDwords, pCmdSpace);

    if (postambleDwords > 0)
    {
        BuildNop(postambleDwords, pCmdSpace);
    }

    pChunk->EndCommandBlock(postambleDwords);

    return commandCount;
}

// =====================================================================================================================
// Updates the last chunk in this command stream so that it chains to the beginning of the first chunk of the given
// target command stream. If a null pointer is provided, a NOP is written to clear out any previous chaining commands.
//
// This is used at submit time to chain together multiple command buffers that were submitted in a single batch. This
// will avoid KMD overhead of a submit and GPU overhead of flushing cached between submits, etc. It must be called after
// End but before Reset/Begin so that m_pTailChainLocation is valid.
void GfxCmdStream::PatchTailChain(
    const CmdStream* pTargetStream
    ) const
{
    // Tail Chaining is disabled in some situations, so skip when m_pTailChainLocation is NULL
    if (m_pTailChainLocation != nullptr)
    {
        // The caller must be sure that chaining is supported.
        PAL_ASSERT(m_chainIbSpaceInDwords > 0);

        if (pTargetStream != nullptr)
        {
            // Non-preemptible command streams don't expect to chain to a preemptible command stream!
            PAL_ASSERT(IsPreemptionEnabled() || (pTargetStream->IsPreemptionEnabled() == false));

            const auto*const pFirstChunk = pTargetStream->GetFirstChunk();

            BuildIndirectBuffer(pFirstChunk->GpuVirtAddr(),
                                pFirstChunk->CmdDwordsToExecute(),
                                pTargetStream->IsPreemptionEnabled(),
                                true,
                                m_pTailChainLocation);
        }
        else
        {
            BuildNop(m_chainIbSpaceInDwords, m_pTailChainLocation);
        }
    }
}

} // Pal
