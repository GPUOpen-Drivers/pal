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

#pragma once

#include "core/cmdStream.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "palVector.h"

namespace Pal
{

class GfxDevice;
enum  QueueType : uint32;

// Directs a ChainPatch to a particular packet type and set of size/address fields within that packet.
enum class ChainPatchType : uint32
{
    IndirectBuffer = 0,
    CondIndirectBufferPass,
    CondIndirectBufferFail
};

// A control flow phase describes which portion of a control flow statement is represented by a control flow frame.
enum class CntlFlowPhase : uint32
{
    If = 0,
    Else,
    While
};

// A control flow frame stores the necessary data to complete if/end, if/else/end, and while/end branching logic.
// The members beginning with "while" are used by the while phase only.
struct CntlFlowFrame
{
    CntlFlowPhase  phase;
    ChainPatchType phasePatchType;    // This phase will need this type of patch added for pPhasePacket.
    void*          pPhasePacket;      // A pointer to some kind of indirect buffer packet this phase needs patched.
    gpusize        whileChainGpuAddr; // GPU virtual address where the end of the while body should jump to loop again.
    uint32         whileChainSize;    // How many command DWORDs to execute at the above address.
};

// A chain patch contains the necessary data to write/update a PM4 chaining packet once its target is complete.
struct ChainPatch
{
    ChainPatchType type;
    void*          pPacket;
};

// =====================================================================================================================
// Implements control flow and other code common to GFX-specific command stream implementations.
class GfxCmdStream : public CmdStream
{
    // A useful shorthand for a vector list of chunks.
    typedef ChunkVector<CmdStreamChunk*, 16, Platform> ChunkRefList;

public:
    virtual ~GfxCmdStream() { }

    virtual void Reset(CmdAllocator* pNewAllocator, bool returnGpuMemory) override;

    virtual void If(CompareFunc compareFunc, gpusize  compareGpuAddr, uint64  data, uint64  mask);
    virtual void Else();
    virtual void EndIf();

    virtual void While(CompareFunc compareFunc, gpusize compareGpuAddr, uint64 data, uint64 mask);
    virtual void EndWhile();

    virtual void Call(const CmdStream& targetStream, bool exclusiveSubmit, bool allowIb2Launch) override;

    void ExecuteGeneratedCommands(CmdStreamChunk** ppChunkList, uint32 numChunksExecuted, uint32 numGenChunks);

    uint32 PrepareChunkForCmdGeneration(
        CmdStreamChunk* pChunk,
        uint32          cmdBufStride,           // In dwords
        uint32          embeddedDataStride,     // In dwords
        uint32          maxCommands) const;

    virtual void PatchTailChain(const CmdStream* pTargetStream) const override;

    // This defines PAL's control flow nesting limit
    static constexpr uint32 CntlFlowNestingLimit = 8;

    virtual size_t BuildNop(uint32 numDwords, uint32* pCmdSpace) const = 0;

protected:
    GfxCmdStream(
        const GfxDevice& device,
        ICmdAllocator*   pCmdAllocator,
        EngineType       engineType,
        SubEngineType    subEngineType,
        CmdStreamUsage   cmdStreamUsage,
        uint32           chainSizeInDwords,
        uint32           minNopSizeInDwords,
        uint32           condIndirectBufferSize,
        bool             isNested);

    void UpdateTailChainLocation(uint32* pTailChain);

    void AddChainPatch(
        ChainPatchType type,
        void*          pChainPacket);

    uint32  CmdBlockOffset() const { return m_cmdBlockOffset; }

    uint32* EndCommandBlock(
        uint32    postambleDwords,
        bool      atEndOfChunk,
        gpusize*  pPostambleAddr = nullptr);

    virtual size_t BuildCondIndirectBuffer(
        CompareFunc compareFunc,
        gpusize     compareGpuAddr,
        uint64      data,
        uint64      mask,
        uint32*     pPacket) const = 0;

    virtual size_t BuildIndirectBuffer(
        gpusize  ibAddr,
        uint32   ibSize,
        bool     preemptionEnabled,
        bool     chain,
        uint32*  pPacket) const = 0;

    virtual void PatchCondIndirectBuffer(
        ChainPatch*  pPatch,
        gpusize      address,
        uint32       ibSizeDwords) const = 0;

    const GfxDevice&  m_device;
    const uint32      m_chainIbSpaceInDwords; // DWORDs needed for chaining in each chunk, 0 if unsupported

private:
    void ComputeCommandBlockSizes(
        uint32  postambleDwords,
        uint32* pPaddingDwords,
        uint32* pAllocDwords,
        uint32* pTotalDwords) const;

    const uint32   m_minNopSizeInDwords;     // The minimum NOP size in DWORDs.
    const uint32   m_condIndirectBufferSize; // Number of DWORDs needed to conditionally launch an indirect buffer
    uint32         m_cmdBlockOffset;         // The current command block began at this DW offset in the current chunk
    uint32*        m_pTailChainLocation;     // Put a chain packet here to chain this command stream to another.

    // We need a stack of control flow frames to manage nested control flow statements.
    CntlFlowFrame  m_cntlFlowStack[CntlFlowNestingLimit];
    uint32         m_numCntlFlowStatements;

    // A command block can only chain to another command block when the target block is complete as its size must be
    // known. This stack holds pointers back to chaining packets in previous command blocks that must be patched when
    // the current command block is completed. Our control flow implementation has been designed such that no more than
    // two chain patch requests will be active at any given moment.
    static constexpr uint32 MaxChainPatches = 2;
    ChainPatch     m_pendingChains[MaxChainPatches];
    uint32         m_numPendingChains;

    PAL_DISALLOW_COPY_AND_ASSIGN(GfxCmdStream);
    PAL_DISALLOW_DEFAULT_CTOR(GfxCmdStream);
};

}; // Pal
