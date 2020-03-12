/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/queue.h"

namespace Pal
{
namespace NullDevice
{

class Device;

// =====================================================================================================================
// The Null SubmissionContext should always say submissions are idle.
class SubmissionContext : public Pal::SubmissionContext
{
public:
    static Result Create(Pal::Platform* pPlatform, Pal::SubmissionContext** ppContext);

    virtual bool IsTimestampRetired(uint64 timestamp) const override { return true; }

private:
    SubmissionContext(Pal::Platform* pPlatform) : Pal::SubmissionContext(pPlatform) {}
    virtual ~SubmissionContext() {}

    PAL_DISALLOW_DEFAULT_CTOR(SubmissionContext);
    PAL_DISALLOW_COPY_AND_ASSIGN(SubmissionContext);
};

// =====================================================================================================================
// Null device flavor of the Queue class.
class Queue : public Pal::Queue
{
public:
    Queue(uint32 qCount, Device* pDevice, const QueueCreateInfo* pCreateInfo);
    virtual ~Queue() {}

    virtual Result Init(
        const QueueCreateInfo* pCreateInfo,
        void*                  pContextPlacementAddr) override;

    virtual QueueType Type() const override { return QueueType::QueueTypeCount; }

    virtual EngineType GetEngineType() const override { return EngineType::EngineTypeCount; }

    virtual void Destroy() {}

    virtual Result LateInit() { return Result::Success; }

    virtual uint32 EngineId() const { return 0; }
    virtual QueuePriority Priority() const { return QueuePriority::Realtime; }

    virtual bool UsesDispatchTunneling() const { return false; }
    virtual bool IsWindowedPriorBlit()   const { return false; }

    virtual uint32 PersistentCeRamOffset() const { return 0; }
    virtual uint32 PersistentCeRamSize()   const { return 0; }

    virtual bool IsPresentModeSupported(PresentMode presentMode) const { return false; };

    virtual Result OsSubmit(
        const MultiSubmitInfo&    submitInfo,
        const InternalSubmitInfo* pInternalSubmitInfos) override;

    virtual Result CopyVirtualMemoryPageMappings(
        uint32                                    rangeCount,
        const VirtualMemoryCopyPageMappingsRange* pRanges,
        bool                                      doNotWait) override;

    virtual Result RemapVirtualMemoryPages(
        uint32                         rangeCount,
        const VirtualMemoryRemapRange* pRangeList,
        bool                           doNotWait,
        IFence*                        pFence) override;

protected:
    // Can't delay a queue that doesn't exist in HW.
    virtual Result OsDelay(float delay, const IPrivateScreen* pScreen) override { return Result::ErrorUnavailable; }

    virtual Result OsPresentDirect(const PresentDirectInfo& presentInfo) override;

    virtual Result OsWaitIdle() override;

    Result DoAssociateFenceWithLastSubmit(Pal::Fence* pFence) override;
private:
    PAL_DISALLOW_DEFAULT_CTOR(Queue);
    PAL_DISALLOW_COPY_AND_ASSIGN(Queue);
};

} // NullDevice
} // Pal
