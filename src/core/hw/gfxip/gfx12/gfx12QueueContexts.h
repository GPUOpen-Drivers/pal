/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/queueContext.h"
#include "core/hw/gfxip/gfx12/gfx12CmdStream.h"
#include "core/hw/gfxip/gfx12/gfx12ShaderRingSet.h"

#include "palDeque.h"

namespace Pal
{
namespace Gfx12
{

class Device;

// Structure to pair command stream with the corresponding LastSubmissionTimeStamp from submissionContext
template <size_t NumStreams>
struct DeferFreeListItem
{
    CmdStreamChunk* pChunk[NumStreams];
    uint64          timestamp;
};

// Queues have at most 6 command streams that will be reset when their Ring Set gets resized.
constexpr uint32 QueueCmdStreamNum = 6;

using QueueDeferFreeList = DeferFreeListItem<QueueCmdStreamNum>;

// =====================================================================================================================
class QueueContext : public Pal::QueueContext
{
public:
    QueueContext(Device* pDevice, EngineType engineType);
    virtual ~QueueContext();

protected:
    Result Init();

    virtual Result LateInit() override;

    virtual void PostProcessSubmit() override;
    Result AllocateExecuteIndirectBuffer(BoundGpuMemory* pExecuteIndirectMem);

    virtual Result ProcessInitialSubmit(InternalSubmitInfo* pSumbitInfo) override;

    virtual Result ProcessFinalSubmit(InternalSubmitInfo* pSumbitInfo) override;

    void ResetCommandStream(
        CmdStream*          pCmdStream,
        QueueDeferFreeList* pList,
        uint32*             pIndex,
        uint64              lastTimeStamp);

    void ClearDeferredMemory();
    void ReleaseCmdStreamMemory(CmdStream* pCmdStream);

    virtual uint32* WritePerSubmitPreambleCmds(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    uint32* WritePerSubmitPostambleCmds(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    uint32* WriteInitialSubmitPreambleCmds(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    uint32* WriteFinalSubmitPostambleCmds(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    Result RecordPrePostAmbleCmdStreams();

    const Device&  m_device;

    // Current watermark for the sample-pos palette updates which have been processed by this queue context.
    uint32         m_queueContextUpdateCounter;
    uint32         m_queueContextUpdateCounterTmz;

    uint32         m_currentStackSizeDw;

    CmdStream      m_perSubmitPreambleCmdStream;    // Static commands to preceded every client submission
    CmdStream      m_perSubmitPostambleCmdStream;   // Static commands to follow every client submission
    CmdStream      m_sharedInternalCmdStream;       // CmdStream that is build, used, and then not needed

    static constexpr uint32 TotalNumCommonCmdStreams = 4; // m_perSubmitPreambleCmdStream,
                                                          // m_perSubmitPostambleCmdStream,
                                                          // m_sharedInternalCmdStream

    static_assert(TotalNumCommonCmdStreams <= QueueCmdStreamNum,
                  "QueueDeferFreeList must be large enough to handle all CmdStreams");
    EngineType     m_engineType;

    // Bound GpuMemory object for the per-queue buffer allocation required for Spill+VBTable data as a memory
    // optimization.
    BoundGpuMemory m_executeIndirectMemAce;
    BoundGpuMemory m_executeIndirectMemGfx;

    // Store the command stream chunks should not be freed immediately.
    Util::Deque<QueueDeferFreeList, Platform> m_deferCmdStreamChunks;

    PAL_DISALLOW_DEFAULT_CTOR(QueueContext);
    PAL_DISALLOW_COPY_AND_ASSIGN(QueueContext);
};

// =====================================================================================================================
// A QueueContext is responsible for managing any Device or hardware-layer state which needs to potentially be updated
// or re-validated prior to any of the operations which the IQueue interface exposes.  Most notably, this includes
// managing various per-queue GPU memory allocations needed for things like shader scratch memory.
class UniversalQueueContext : public QueueContext
{
public:
    UniversalQueueContext(Device* pDevice);
    virtual ~UniversalQueueContext();

    Result Init();

protected:
    virtual Result PreProcessSubmit(
        InternalSubmitInfo*      pSubmitInfo,
        uint32                   cmdBufferCount,
        const ICmdBuffer* const* ppCmdBuffers) override;
    virtual void PostProcessSubmit() override;

    virtual uint32* WritePerSubmitPreambleCmds(CmdStream* pCmdStream, uint32* pCmdSpace) const override;

private:
    void ClearDeferredMemory();

    Result UpdatePerContextDependencies(
        bool*                    pHasChanged,
        bool                     isTmz,
        uint32                   overrideStackSize,
        uint64                   lastTimeStamp,
        uint32                   cmdBufferCount,
        const ICmdBuffer* const* ppCmdBuffers,
        bool                     hasAce,
        bool                     hasInitAce);
    Result RebuildPerSubmitPreambleCmdStream(bool isTmz, bool hasAce);

    Result InitAcePreambleCmdStream();
    Result InitAcePostambleCmdStream();

    bool           m_firstSubmit;

    UniversalRingSet m_ringSet;    // Set of shader-accessible rings (scratch, ATM, etc.)
    UniversalRingSet m_tmzRingSet; // Set of shader-accessible rings with tmz enabled.

    bool m_cmdsUseTmzRing; // Indicates whether the current commands streams use TMZ protected ring sets.

    // Late-initialized ACE command buffer stream.
    // This is used for setting up state on the ACE queue for the DispatchDraw mechanism.
    const bool m_supportsAceGang;
    CmdStream* m_pAcePreambleCmdStream;
    CmdStream* m_pAcePostambleCmdStream;

    static constexpr uint32 TotalNumUniversalCmdStreams = 2; // m_pAcePreambleCmdStream,
                                                             // m_pAcePostambleCmdStream

    static_assert((TotalNumCommonCmdStreams + TotalNumUniversalCmdStreams) <= QueueCmdStreamNum,
                  "QueueDeferFreeList must be large enough to handle all CmdStreams");

    PAL_DISALLOW_DEFAULT_CTOR(UniversalQueueContext);
    PAL_DISALLOW_COPY_AND_ASSIGN(UniversalQueueContext);
};

// =====================================================================================================================
// A QueueContext is responsible for managing any Device or hardware-layer state which needs to potentially be updated
// or re-validated prior to any of the operations which the IQueue interface exposes.  Most notably, this includes
// managing various per-queue GPU memory allocations needed for things like shader scratch memory.
class ComputeQueueContext : public QueueContext
{
public:
    ComputeQueueContext(Device* pDevice, bool isTmz);
    virtual ~ComputeQueueContext();

    Result Init();

protected:
    virtual Result PreProcessSubmit(
        InternalSubmitInfo*      pSubmitInfo,
        uint32                   cmdBufferCount,
        const ICmdBuffer* const* ppCmdBuffers) override;
    virtual void PostProcessSubmit() override;

    virtual uint32* WritePerSubmitPreambleCmds(CmdStream* pCmdStream, uint32* pCmdSpace) const override;

    uint32* WritePerSubmitPreambleCmds(const ComputeRingSet& ringSet, CmdStream* pCmdStream, uint32* pCmdSpace) const;

private:
    void ClearDeferredMemory();

    Result UpdatePerContextDependencies(
        bool*                    pHasChanged,
        uint32                   overrideStackSize,
        uint64                   lastTimeStamp,
        uint32                   cmdBufferCount,
        const ICmdBuffer* const* ppCmdBuffers);
    Result RebuildPerSubmitPreambleCmdStream();

    ComputeRingSet m_ringSet; // Compute shader-accessible ring

    PAL_DISALLOW_DEFAULT_CTOR(ComputeQueueContext);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputeQueueContext);
};

} // namespace Gfx12
} // namespace Pal
