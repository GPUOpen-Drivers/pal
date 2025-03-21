/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9ShaderRingSet.h"
#include "core/hw/gfxip/gfx9/gfx9ShadowedRegisters.h"

namespace Pal
{
namespace Gfx9
{

class ComputeEngine;
class Device;
class UniversalEngine;

// Structure to pair command stream with the corresponding LastSubmissionTimeStamp from submissionContext
template <size_t NumStreams>
struct DeferFreeListItem
{
   CmdStreamChunk* pChunk[NumStreams];
   uint64          timestamp;
};

// ComputeQueue has 3 comamnd streams that will be reset when ringSet got resized.
static const uint32 ComputeQueueCmdStreamNum = 3;
using ComputeQueueDeferFreeList = DeferFreeListItem<ComputeQueueCmdStreamNum>;

// UniversalQueue has 5 comamnd streams that will be reset when ringSet got resized.
static const uint32 UniversalQueueCmdStreamNum = 5;
using UniversalQueueDeferFreeList = DeferFreeListItem<UniversalQueueCmdStreamNum>;

// =====================================================================================================================
class ComputeQueueContext final : public QueueContext
{
public:
    ComputeQueueContext(Device* pDevice, Engine* pEngine, uint32 queueId, bool isTmz);
    virtual ~ComputeQueueContext() { }

    Result Init();

    virtual Result PreProcessSubmit(
        InternalSubmitInfo*      pSubmitInfo,
        uint32                   cmdBufferCount,
        const ICmdBuffer* const* ppCmdBuffers) override;
    virtual void PostProcessSubmit() override;

private:
    Result RebuildCommandStreams(uint64 lastTimeStamp);
    void ResetCommandStream(CmdStream*                 pCmdStream,
                            ComputeQueueDeferFreeList* pList,
                            uint32*                    pIndex,
                            uint64                     lastTimeStamp);

    void ClearDeferredMemory();

    Result UpdateRingSet(
        bool*                    pHasChanged,
        uint32                   overrideStackSize,
        uint64                   lastTimeStamp,
        uint32                   cmdBufferCount,
        const ICmdBuffer* const* ppCmdBuffers);

    Device*const        m_pDevice;
    ComputeEngine*const m_pEngine;
    uint32              m_queueId;
    ComputeRingSet      m_ringSet;

    // Current watermark for the sample-pos palette updates which have been processed by this queue context.
    uint32  m_queueContextUpdateCounter;

    uint32  m_currentStackSizeDw;

    CmdStream  m_cmdStream;
    CmdStream  m_perSubmitCmdStream;
    CmdStream  m_postambleCmdStream;

    Util::Deque<ComputeQueueDeferFreeList, Platform> m_deferCmdStreamChunks;

    PAL_DISALLOW_DEFAULT_CTOR(ComputeQueueContext);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputeQueueContext);
};

// =====================================================================================================================
class UniversalQueueContext final : public QueueContext
{
public:
    UniversalQueueContext(
        Device* pDevice,
        bool    supportMcbp,
        Engine* pEngine,
        uint32  queueId);
    virtual ~UniversalQueueContext();

    Result Init();

    virtual Result PreProcessSubmit(
        InternalSubmitInfo*      pSubmitInfo,
        uint32                   cmdBufferCount,
        const ICmdBuffer* const* ppCmdBuffers) override;
    virtual void PostProcessSubmit() override;
    virtual Result ProcessInitialSubmit(InternalSubmitInfo* pSubmitInfo) override;

    virtual gpusize ShadowMemVa() const override { return  m_shadowGpuMem.GpuVirtAddr(); }

    Result AllocateExecuteIndirectBufferGfx();

    const BoundGpuMemory& GetExecuteIndirectGfxBuffer() const { return m_executeIndirectMemGfx; }

private:
    Result BuildShadowPreamble();

    void ResetCommandStream(CmdStream*                   pCmdStream,
                            UniversalQueueDeferFreeList* pList,
                            uint32*                      pIndex,
                            uint64                       lastTimeStamp);

    Result RebuildCommandStreams(bool isTmz, uint64 lastTimeStamp, bool hasAce);

    Result AllocateShadowMemory();

    void WritePerSubmitPreamble(CmdStream* pCmdStream, bool initShadowMemory);
    uint32* WriteUniversalPreamble(CmdStream* pCmdStream, uint32* pCmdSpace);

    void ClearDeferredMemory();

    Result UpdateRingSet(
        bool*                    pHasChanged,
        bool                     isTmz,
        bool                     hasAce,
        bool                     hasInitAce,
        uint32                   overrideStackSize,
        uint64                   lastTimeStamp,
        uint32                   cmdBufferCount,
        const ICmdBuffer* const* ppCmdBuffers);

    Result InitAcePreambleCmdStream();
    Result GetAcePreambleCmdStream(CmdStream** ppAcePreambleCmdStream);

    Device*const          m_pDevice;
    UniversalEngine*const m_pEngine;
    uint32                m_queueId;
    UniversalRingSet      m_ringSet;
    UniversalRingSet      m_tmzRingSet;

    // Current watermark for the sample-pos palette updates which have been processed by this queue context.
    uint32  m_queueContextUpdateCounter;
    uint32  m_queueContextUpdateCounterTmz;

    uint32 m_currentStackSizeDw;
    bool   m_cmdsUseTmzRing; // Indicates whether the current command streams use TMZ protected ring sets.

    // GPU memory allocation used for shadowing contents between submissions.
    bool            m_supportMcbp;
    BoundGpuMemory  m_shadowGpuMem;
    gpusize         m_shadowGpuMemSizeInBytes;
    uint32          m_shadowedRegCount; // Number of state registers shadowed using state-shadowing.

    // Command streams which restore hardware to a known state before launching command buffers.
    CmdStream  m_deCmdStream;
    CmdStream  m_perSubmitCmdStream;
    CmdStream  m_shadowInitCmdStream;
    CmdStream  m_cePreambleCmdStream;
    CmdStream  m_dePostambleCmdStream;

    // Late-initialized ACE command buffer stream.
    // This is used for setting up state on the ACE queue for the DispatchDraw mechanism.
    bool       m_supportsAceGang;
    CmdStream* m_pAcePreambleCmdStream;

    // Bound GpuMemory object for the per-queue buffer allocation required for Spill+VBTable data as a memory
    // optimization.
    BoundGpuMemory m_executeIndirectMemGfx;

    Util::Deque<UniversalQueueDeferFreeList, Platform> m_deferCmdStreamChunks;

    PAL_DISALLOW_DEFAULT_CTOR(UniversalQueueContext);
    PAL_DISALLOW_COPY_AND_ASSIGN(UniversalQueueContext);
};

} // Gfx9
} // Pal
