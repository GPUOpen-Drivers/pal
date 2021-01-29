/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6ShadowedRegisters.h"

namespace Pal
{
namespace Gfx6
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
// GFX6+ hardware requires an internal scratch ring of memory to be used for register spilling if a shader uses too many
// temp registers. This scratch ring can be dynamically resized based on the highest scratch memory needs of any Compute
// Pipeline which has been created thus far. Thus, a small command stream needs to be submitted along with any client
// submission which follows a resize event or context-switch between applications. This class is responsible for
// guaranteeing that the scratch ring is in a valid state before launching GPU work.
//
// SEE: Gfx6::ShaderRingMgr, ShaderRingSet and ShaderRing.
class ComputeQueueContext final : public QueueContext
{
public:
    ComputeQueueContext(Device* pDevice, Engine* pEngine, uint32 queueId, bool isTmz);
    virtual ~ComputeQueueContext() {}

    Result Init();

    Result UpdateRingSet(bool isTmz, bool* pHasChanged, uint64  lastTimeStamp);
    virtual Result PreProcessSubmit(InternalSubmitInfo* pSubmitInfo, uint32 cmdBufferCount) override;
    virtual void PostProcessSubmit() override;

private:
    Result RebuildCommandStreams(bool isTmz, uint64 lastTimeStamp);

    void ResetCommandStream(
        CmdStream*                 pCmdStream,
        ComputeQueueDeferFreeList* pList,
        uint32*                    pIndex,
        uint64                     lastTimeStamp);

    void ClearDeferredMemory();

    Device*const        m_pDevice;
    ComputeEngine*const m_pEngine;
    uint32              m_queueId;
    bool                m_queueUseTmzRing;

    ComputeRingSet      m_ringSet;
    ComputeRingSet      m_tmzRingSet;

    // Current watermark for the device-initiated context updates which have been processed by this queue context.
    uint32  m_currentUpdateCounter;
    uint32  m_currentUpdateCounterTmz;

    CmdStream  m_cmdStream;
    CmdStream  m_perSubmitCmdStream;
    CmdStream  m_postambleCmdStream;

    Util::Deque<ComputeQueueDeferFreeList, Platform> m_deferCmdStreamChunks;

    PAL_DISALLOW_DEFAULT_CTOR(ComputeQueueContext);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputeQueueContext);
};

// =====================================================================================================================
// In addition to the internal scratch ring requirement mentioned above, GFX6+ hardware also requires several internal
// memory rings for various other needs (such as Geometry Shaders or Tessellation). Like with the scratch rings, these
// others can also be dynamically resized based on the highest ring memory needs of any Compute or Graphics Pipeline
// which has been created thus far. Furthermore, some hardware has a bug which doesn't restore the state of certain
// registers after a power-managment event. Thus, a pair of small command streams may need to be submitted along with
// any client submission which follows a resize event, power-managment event, or context-switch between applications.
// This class is responsible for guaranteeing that the state of the internal memory rings and non-restored registers
// are valid before launching GPU work.
//
// SEE: Gfx6::ShaderRingMgr, ShaderRingSet and ShaderRing.
class UniversalQueueContext : public QueueContext
{
public:
    UniversalQueueContext(
        Device* pDevice,
        bool    isPreemptionSupported,
        uint32  persistentCeRamOffset,
        uint32  persistentCeRamSize,
        Engine* pEngine,
        uint32  queueId);
    virtual ~UniversalQueueContext();

    Result Init();

    virtual Result PreProcessSubmit(InternalSubmitInfo* pSubmitInfo, uint32 cmdBufferCount) override;
    virtual void PostProcessSubmit() override;
    virtual Result ProcessInitialSubmit(InternalSubmitInfo* pSubmitInfo) override;

    Result UpdateRingSet(bool isTmz, bool* pHasChanged, uint64 lastTimeStamp);

private:
    Result BuildShadowPreamble();
    void ResetCommandStream(CmdStream*                   pCmdStream,
                            UniversalQueueDeferFreeList* pList,
                            uint32*                      pIndex,
                            uint64                       lastTimeStamp);

    Result RebuildCommandStreams(bool isTmz, uint64 lastTimeStamp);

    void ClearDeferredMemory();

    Result AllocateShadowMemory();

    void WritePerSubmitPreamble(CmdStream* pCmdStream, bool initShadowMemory);
    uint32* WriteStateShadowingCommands(CmdStream* pCmdStream, uint32* pCmdSpace);
    uint32* WriteUniversalPreamble(uint32* pCmdSpace);

    Device*const          m_pDevice;
    UniversalEngine*const m_pEngine;
    uint32                m_queueId;
    const uint32          m_persistentCeRamOffset;
    const uint32          m_persistentCeRamSize;

    UniversalRingSet m_ringSet;
    UniversalRingSet m_tmzRingSet;

    // Current watermark for the device-initiated context updates which have been processed by this queue context.
    uint32  m_currentUpdateCounter;
    uint32  m_currentUpdateCounterTmz;
    bool    m_cmdsUseTmzRing;

    // GPU memory allocation used for shadowing persistent CE RAM between submissions.
    bool            m_useShadowing;
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

    Util::Deque<UniversalQueueDeferFreeList, Platform> m_deferCmdStreamChunks;

    PAL_DISALLOW_DEFAULT_CTOR(UniversalQueueContext);
    PAL_DISALLOW_COPY_AND_ASSIGN(UniversalQueueContext);
};

} // Gfx6
} // Pal
