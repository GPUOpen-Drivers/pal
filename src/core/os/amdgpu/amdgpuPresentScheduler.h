/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/presentScheduler.h"

namespace Pal
{

class IDevice;
class IQueue;
enum WsiPlatform : uint32;

namespace Amdgpu
{

class Device;
class PresentFence;
class Queue;
class WindowSystem;

// =====================================================================================================================
// On Linux, the present scheduler's core logic implements all presents using the native WindowSystem.
class PresentScheduler final : public Pal::PresentScheduler
{
public:
    // The present scheduler is designed to be placed into other PAL objects which requires the Create/Destroy pattern.
    static size_t GetSize(const Device& device, IDevice*const pSlaveDevices[], WsiPlatform wsiPlatform);

    static Result Create(
        Device*                 pDevice,
        IDevice*const           pSlaveDevices[],
        WindowSystem*           pWindowSystem,
        void*                   pPlacementAddr,
        Pal::PresentScheduler** ppPresentScheduler);

    virtual Result SignalOnAcquire(IQueueSemaphore* pWaitSemaphore,
                                   IQueueSemaphore* pSemaphore,
                                   IFence* pFence) override;

private:
    PresentScheduler(Device* pDevice, WindowSystem* pWindowSystem);
    virtual ~PresentScheduler();

    virtual Result Init(IDevice*const pSlaveDevices[], void* pPlacementAddr) override;
    virtual Result PreparePresent(IQueue* pQueue, PresentSchedulerJob* pJob) override;

    virtual Result ProcessPresent(const PresentSwapChainInfo& presentInfo, IQueue* pQueue, bool isInline) override;
    virtual Result FailedToQueuePresentJob(const PresentSwapChainInfo& presentInfo, IQueue* pQueue) override;

    virtual bool CanInlinePresent(const PresentSwapChainInfo& presentInfo, const IQueue& queue) const override;

    Result DoCpuPresentBlit(Queue* pQueue, Image* pImage);
    ICmdBuffer* m_pCpuBltCmdBuffer;

    WindowSystem*const m_pWindowSystem; // A cached pointer to our parent swap chain's WindowSystem.

    PAL_DISALLOW_DEFAULT_CTOR(PresentScheduler);
    PAL_DISALLOW_COPY_AND_ASSIGN(PresentScheduler);
};

} // Amdgpu
} // Pal
