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
#include "core/cmdStream.h"
#include "core/device.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/pm4CmdStream.h"
#include "palVectorImpl.h"

using namespace Util;

namespace Pal
{
namespace Pm4
{

// =====================================================================================================================
CmdStream::CmdStream(
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
    Pal::GfxCmdStream(device,
                      pCmdAllocator,
                      engineType,
                      subEngineType,
                      cmdStreamUsage,
                      chainSizeInDwords,
                      minNopSizeInDwords,
                      condIndirectBufferSize,
                      isNested)
{
}

// =====================================================================================================================
// Specialized implementation of "Call" for GFXIP command streams.  This will attempt to use either an IB2 packet or
// take advantage of command buffer chaining instead of just copying the callee's command stream contents into this
// stream.
void CmdStream::Call(
    const Pal::CmdStream & targetStream,
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
void CmdStream::ExecuteGeneratedCommands(
    CmdStreamChunk** ppChunkList,   // Points to a list of command chunks containing GPU-generated commands
    uint32           numChunksExecuted,
    uint32           numGenChunks)
{
    PAL_ASSERT(numGenChunks != numChunksExecuted); // It is illegal to execute zero generated command chunks!

    // This operation is only intended for use on command streams which support command-chunk chaining.
    PAL_ASSERT(m_chainIbSpaceInDwords != 0);
    const uint32 postambleDwords = m_chainIbSpaceInDwords;

    // End our current command block, using the jump to the first executed chunk as our block postamble. The
    // chain location will be filled with a chain packet to that chunk in the loop below.
    uint32* pChainPacket = EndCommandBlock(postambleDwords, false);

    for (uint32 i = numChunksExecuted; i < numGenChunks; i++)
    {
        // Fill the chain packet location with a jump to the next command chunk which was generated by the GPU.
        BuildIndirectBuffer(ppChunkList[i]->GpuVirtAddr(),
                            ppChunkList[i]->CmdDwordsToExecute(),
                            IsPreemptionEnabled(),
                            true,
                            pChainPacket);

        // NOTE: The call to PrepareChunkForCmdGeneration() reserves enough space at the end of the chunk for a
        // chain packet by writing an equally-sized NOP before the GPU generated the actual meat of this command
        // chunk. We just have to update our chain packet location for the next run of the loop. The chain packet
        // should be the very last item in the command buffer, following any padding for size-alignment.
        pChainPacket = (ppChunkList[i]->GetRmwWriteAddr() + ppChunkList[i]->CmdDwordsToExecute() - postambleDwords);
    }

    // NOTE: As mentioned above, the last chunk being executed already has a reserved location for a chain packet
    // which is needed to jump back to the main command stream.
    AddChainPatch(ChainPatchType::IndirectBuffer, pChainPacket);
}

// =====================================================================================================================
// Prepares a blank command-stream chunk for use as the target for GPU-generated commands, including adding the correct
// amount of padding "after" the generated commands. Returns the number of GPU-generated commands which will safely fit
// in the chunk.
uint32 CmdStream::PrepareChunkForCmdGeneration(
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

} // Pm4
} // Pal
