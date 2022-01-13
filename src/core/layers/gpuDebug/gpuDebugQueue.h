/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_DEVELOPER_BUILD

#include "core/layers/decorators.h"
#include "palDeque.h"
#include "palLinearAllocator.h"

namespace Pal
{
namespace GpuDebug
{

class CmdBuffer;
class Device;

typedef Util::Deque<TargetCmdBuffer*, Platform> CmdBufDeque;

// This struct tracks per subQueue info when we do gang submission.
struct SubQueueInfo
{
    QueueType  queueType;
    EngineType engineType;
    uint32     engineIndex;
    bool       commentsSupported;
    // For each subQueue, track 3 lists of various objects:
    //   1. CmdBuffers that are available (not-busy) for use in the next submit.
    //   2. CmdBuffers that are tracked as part of the next submit. These CmdBuffers are moved to the busy list once
    //      the submit is done.
    //   3. CmdBuffers that may be executing on the GPU.
    CmdBufDeque* pAvailableCmdBufs;
    CmdBufDeque* pNextSubmitCmdBufs;
    CmdBufDeque* pBusyCmdBufs;

    CmdBufDeque* pAvailableNestedCmdBufs;
    CmdBufDeque* pNextSubmitNestedCmdBufs;
    CmdBufDeque* pBusyNestedCmdBufs;
};

// =====================================================================================================================
class Queue final : public QueueDecorator
{
public:
    Queue(IQueue*    pNextQueue,
          Device*    pDevice,
          uint32     queueCount);

    Result Init(const QueueCreateInfo* pCreateInfo);

    // Acquire methods return corresponding objects for use by a command buffer being replayed from reusable pools
    // managed by the Queue.
    TargetCmdBuffer* AcquireCmdBuf(const CmdBufInfo* pCmdBufInfo, uint32 subQueueIdx, bool nested);

    // Public IQueue interface methods:
    virtual Result Submit(
        const MultiSubmitInfo& submitInfo) override;
    virtual void Destroy() override;

    Util::VirtualLinearAllocator* ReplayAllocator() { return &m_replayAllocator; }

private:
    virtual ~Queue() {}

    Result ProcessRemaps(
        const MultiSubmitInfo&   submitInfo,
        size_t                   totalCmdBufferCount);
    Result WaitForFence(const IFence* pFence) const;

    Result SubmitAll(
        const MultiSubmitInfo& submitInfo,
        PerSubQueueSubmitInfo* pPerSubQueueInfos,
        ICmdBuffer**           ppCmdBuffers,
        CmdBufInfo*            pCmdBufInfos,
        size_t                 totalCmdBufferCount);

    Result SubmitSplit(
        const MultiSubmitInfo& submitInfo,
        ICmdBuffer**           ppCmdBuffers,
        CmdBufInfo*            pCmdBufInfos,
        size_t                 totalCmdBufferCount);

    Result InitCmdAllocator();
    Result InitNestedCmdAllocator();
    Result InitCmdBuffers(
        const QueueCreateInfo* pCreateInfo);
    void AddRemapRange(
        uint32                   queueId,
        VirtualMemoryRemapRange* pRange,
        CmdBuffer*               pCmdBuffer) const;

    IFence* AcquireFence();
    void ProcessIdleSubmits();

    Device*const   m_pDevice;
    const uint32   m_queueCount;
    SubQueueInfo*  m_pQueueInfos;

    bool           m_timestampingActive;
    ICmdAllocator* m_pCmdAllocator;
    ICmdAllocator* m_pNestedCmdAllocator;
    CmdBuffer**    m_ppCmdBuffer;
    IGpuMemory**   m_ppTimestamp;

    // Tracks a list of pending (not retired yet) submits on this queue. When the corresponding pFence object is
    // signaled, we know we can:
    //     - Reclaim the first cmdBufCount/etc. entries in each of the "m_busyFoo" deques.
    //     - Reclaim that fence as well.
    struct PendingSubmitInfo
    {
        IFence* pFence;
        uint32* pCmdBufCount;
        uint32* pNestedCmdBufCount;
    };
    Util::Deque<PendingSubmitInfo, Platform> m_pendingSubmits;

    // Tracks a list of fence objects owned by this queue that are ready for reuse.
    Util::Deque<IFence*, Platform> m_availableFences;
    Util::VirtualLinearAllocator   m_replayAllocator; // Used to allocate temporary memory during command buffer replay.

    uint32 m_submitOnActionCount;
    uint32 m_waitIdleSleepMs;

    PAL_DISALLOW_DEFAULT_CTOR(Queue);
    PAL_DISALLOW_COPY_AND_ASSIGN(Queue);
};

} // GpuDebug
} // Pal

#endif
