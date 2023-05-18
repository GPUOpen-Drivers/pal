/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

// UniversalQueue has 3 comamnd streams that will be reset when ringSet got resized.
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

    virtual Result PreProcessSubmit(InternalSubmitInfo* pSubmitInfo, uint32 cmdBufferCount) override;
    virtual void PostProcessSubmit() override;

private:
    Result RebuildCommandStreams(uint64 lastTimeStamp);
    void ResetCommandStream(CmdStream*                 pCmdStream,
                            ComputeQueueDeferFreeList* pList,
                            uint32*                    pIndex,
                            uint64                     lastTimeStamp);

    void ClearDeferredMemory();

    Result UpdateRingSet(bool* pHasChanged, uint32 overrideStackSize, uint64 lastTimeStamp);

    Device*const        m_pDevice;
    ComputeEngine*const m_pEngine;
    uint32              m_queueId;
    ComputeRingSet      m_ringSet;

    // Current watermark for the device-initiated context updates which have been processed by this queue context.
    uint32  m_currentUpdateCounter;

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
        uint32  persistentCeRamOffset,
        uint32  persistentCeRamSize,
        Engine* pEngine,
        uint32  queueId);
    virtual ~UniversalQueueContext();

    Result Init();

    virtual Result PreProcessSubmit(InternalSubmitInfo* pSubmitInfo, uint32 cmdBufferCount) override;
    virtual void PostProcessSubmit() override;
    virtual Result ProcessInitialSubmit(InternalSubmitInfo* pSubmitInfo) override;

    virtual gpusize ShadowMemVa() const { return  m_shadowGpuMem.GpuVirtAddr(); }

private:
    Result BuildShadowPreamble();

    void ResetCommandStream(CmdStream*                   pCmdStream,
                            UniversalQueueDeferFreeList* pList,
                            uint32*                      pIndex,
                            uint64                       lastTimeStamp);

    Result RebuildCommandStreams(bool isTmz, uint64 lastTimeStamp);

    Result AllocateShadowMemory();

    void WritePerSubmitPreamble(CmdStream* pCmdStream, bool initShadowMemory);
    uint32* WriteUniversalPreamble(CmdStream* pCmdStream, uint32* pCmdSpace);

    void ClearDeferredMemory();

    Result UpdateRingSet(bool* pHasChanged, bool isTmz, uint32 overrideStackSize, uint64 lastTimeStamp);

    Result GetAcePreambleCmdStream(CmdStream** ppAcePreambleCmdStream);

    Device*const          m_pDevice;
    const uint32          m_persistentCeRamOffset;
    const uint32          m_persistentCeRamSize;
    UniversalEngine*const m_pEngine;
    uint32                m_queueId;
    UniversalRingSet      m_ringSet;
    UniversalRingSet      m_tmzRingSet;

    // Current watermark for the device-initiated context updates which have been processed by this queue context.
    uint32  m_currentUpdateCounter;

    uint32  m_currentUpdateCounterTmz;

    uint32  m_currentStackSizeDw;

    // Indicates whether the current command streams use TMZ protected ring sets.
    bool    m_cmdsUseTmzRing;

    // GPU memory allocation used for shadowing persistent CE RAM between submissions.
    bool            m_supportMcbp;
    BoundGpuMemory  m_shadowGpuMem;
    gpusize         m_shadowGpuMemSizeInBytes;
    uint32          m_shadowedRegCount; // Number of state registers shadowed using state-shadowing.

    // Command streams which restore hardware to a known state before launching command buffers.
    CmdStream  m_deCmdStream;
    CmdStream  m_perSubmitCmdStream;
    CmdStream  m_shadowInitCmdStream;
    CmdStream  m_cePreambleCmdStream;
    CmdStream  m_cePostambleCmdStream;
    CmdStream  m_dePostambleCmdStream;

    // Late-initialized ACE command buffer stream.
    // This is used for setting up state on the ACE queue for the DispatchDraw mechanism.
    bool       m_supportsAceGang;
    CmdStream* m_pAcePreambleCmdStream;

    Util::Deque<UniversalQueueDeferFreeList, Platform> m_deferCmdStreamChunks;

    PAL_DISALLOW_DEFAULT_CTOR(UniversalQueueContext);
    PAL_DISALLOW_COPY_AND_ASSIGN(UniversalQueueContext);
};

} // Gfx9
} // Pal
