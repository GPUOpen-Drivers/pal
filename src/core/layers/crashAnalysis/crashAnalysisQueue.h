/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "crashAnalysis.h"
#include "core/layers/decorators.h"
#include "palVector.h"
#include "palDeque.h"
#include "palIntrusiveList.h"

namespace Pal
{
namespace CrashAnalysis
{

class Device; // Forward-declare

typedef Util::Vector<MemoryChunk*, 16, IPlatform> MarkerStateList;
typedef Util::Vector<EventCache*,  16, IPlatform> EventCacheList;

// =====================================================================================================================
class Queue final : public QueueDecorator
{
public:
    Queue(IQueue* pNextQueue,
          Device* pDevice,
          uint32  queueCount);

    Result Init(const QueueCreateInfo* pCreateInfo);

    void LogCrashAnalysisMarkerData() const;

    Util::IntrusiveListNode<Queue>* DeviceMembershipNode() { return &m_node; }

    // Public IQueue interface methods:
    virtual Result Submit(
        const MultiSubmitInfo& submitInfo) override;
    virtual void Destroy() override;

private:
    virtual ~Queue() { }

    IFence* AcquireFence();
    void    ProcessIdleSubmits();

    Device*const m_pDevice;
    const uint32 m_queueCount;

    // Each queue must register itself with its device and engine so that they can manage their internal lists.
    Util::IntrusiveListNode<Queue> m_node;

    // Tracks a list of fence objects owned by this queue that are ready for reuse.
    Util::Deque<IFence*, IPlatform> m_availableFences;

    struct PendingSubmitInfo
    {
        IFence*          pFence;
        MarkerStateList* pStateList;
        EventCacheList*  pEventList;
    };
    Util::Deque<PendingSubmitInfo, IPlatform> m_pendingSubmits;

    PAL_DISALLOW_DEFAULT_CTOR(Queue);
    PAL_DISALLOW_COPY_AND_ASSIGN(Queue);
};

} // namespace CrashAnalysis
} // namespace Pal

