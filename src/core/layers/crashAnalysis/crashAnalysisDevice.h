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
#include "palBuddyAllocator.h"
#include "palIntrusiveList.h"
#include "palMutex.h"

namespace Pal
{
namespace CrashAnalysis
{

// Forward declarations
class Queue;

// =====================================================================================================================
class Device final : public DeviceDecorator
{
public:
    Device(PlatformDecorator* pPlatform, IDevice* pNextDevice);

    void TrackQueue(
        Queue* pQueue);

    void UntrackQueue(
        Queue* pQueue);

    Result GetMemoryChunk(
        MemoryChunk** ppMemChunk);

    void LogCrashAnalysisMarkerData();

    void FreeMemoryChunkAllocation(
        uint32  raftIndex,
        gpusize gpuVirtAddr);

    // Public IDevice interface methods
    virtual size_t GetCmdBufferSize(
        const CmdBufferCreateInfo& createInfo,
        Result*                    pResult) const override;

    virtual Result CreateCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        void*                      pPlacementAddr,
        ICmdBuffer**               ppCmdBuffer) override;

    virtual size_t GetQueueSize(
        const QueueCreateInfo&     createInfo,
        Result*                    pResult) const override;

    virtual Result CreateQueue(
        const QueueCreateInfo&     createInfo,
        void*                      pPlacementAddr,
        IQueue**                   ppQueue) override;

    virtual size_t GetMultiQueueSize(
        uint32                     queueCount,
        const QueueCreateInfo*     pCreateInfo,
        Result*                    pResult) const override;

    virtual Result CreateMultiQueue(
        uint32                     queueCount,
        const QueueCreateInfo*     pCreateInfo,
        void*                      pPlacementAddr,
        IQueue**                   ppQueue) override;

    virtual Result CommitSettingsAndInit() override;

    virtual Result Finalize(
        const DeviceFinalizeInfo&  finalizeInfo) override;

    virtual Result Cleanup() override;

private:
    virtual ~Device();

    Result CreateMemoryRaft();
    void   FreeMemoryRafts();

    const PalPublicSettings*                  m_pPublicSettings;
    DeviceProperties                          m_deviceProperties;

    Util::IntrusiveList<Queue>                m_queues;
    Util::Mutex                               m_queueLock;
    Util::Mutex                               m_memoryLock;
    bool                                      m_initialized;

    struct RaftAllocator
    {
        Util::BuddyAllocator<IPlatform>*      pBuddyAllocator;
        IGpuMemory*                           pGpuMemory;
        void*                                 pSystemMemory;
    };
    Util::Vector<RaftAllocator, 1, IPlatform> m_memoryRafts;

    PAL_DISALLOW_DEFAULT_CTOR(Device);
    PAL_DISALLOW_COPY_AND_ASSIGN(Device);
};

} // namespace CrashAnalysis
} // namespace Pal

