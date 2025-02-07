/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/decorators.h"
#include "palDeque.h"

namespace Pal
{

namespace DbgOverlay
{

// Forward decl's
class Device;
class Image;

// A GpuTimestampPair times the execution of a set of command buffers from a single submission.
// It uses two internal command buffers to timestamp when the submission begins and ends.
struct GpuTimestampPair
{
    const IQueue* pOwner;
    ICmdBuffer*   pBeginCmdBuffer;
    ICmdBuffer*   pEndCmdBuffer;
    IFence*       pFence;               // Fence corresponding to the last submit that accessed this range.
    uint64        frameNumber;          // Frame Number to associate the GpuTimestampPair with the frame
    uint64        timestampFrequency;   // Provides the number of timestamp clock ticks per second

    volatile uint64* pBeginTimestamp;
    volatile uint64* pEndTimestamp;
    volatile uint32  numActiveSubmissions;  // Keeps track of GpuTimestampPair currently in use
};

// A command buffer and a fence to track its submission state wrapped into one object.
struct TrackedCmdBuffer
{
    ICmdBuffer* pCmdBuffer;
    IFence*     pFence;
};

typedef Util::Deque<GpuTimestampPair*, PlatformDecorator> GpuTimestampDeque;

// This struct tracks per subQueue info when we do gang submission.
struct SubQueueInfo
{
    QueueType   queueType;
    EngineType  engineType;
    uint32      engineIndex;
    bool        supportTimestamps;    // Queue (based on engine type) supports timestamps.
    uint32      timestampAlignment;   // Aligns the Timestamps in Memory
    size_t      timestampMemorySize;  // Size of the GpuTimestamp buffer (mapped CPU memory and GPU memory).
    size_t      nextTimestampOffset;  // Offset for the next GpuTimestamp
    void*       pMappedTimestampData; // Mapped memory pointing to the initial GpuTimestamp
    IGpuMemory* pTimestampMemory;

    GpuTimestampDeque* pGpuTimestamps;
};

// =====================================================================================================================
class Queue final : public QueueDecorator
{
public:
    Queue(IQueue* pNextQueue, Device* pDevice, uint32 queueCount);
    virtual ~Queue();

    Result Init(const QueueCreateInfo* pCreateInfo);

    virtual Result Submit(const MultiSubmitInfo& submitInfo) override;

private:
    Result CreateCmdBuffer(const CmdBufferCreateInfo& createInfo, ICmdBuffer** ppCmdBuffer);
    Result CreateFence(
        const FenceCreateInfo& createInfo,
        IFence**               ppFence);

    Result CreateGpuTimestampPairMemory(SubQueueInfo* pSubQueueInfo);

    Result SubmitWithGpuTimestampPair(const MultiSubmitInfo& submitInfo, GpuTimestampPair** ppTimestamp);

    Result CreateGpuTimestampPair(SubQueueInfo* pSubQueueInfo, GpuTimestampPair** ppTimestamp);
    void DestroyGpuTimestampPair(GpuTimestampPair* pTimestamp);

    static constexpr uint32 MaxGpuTimestampPairCount = 256;

    Device*const     m_pDevice;
    const uint32     m_queueCount;
    SubQueueInfo*    m_pSubQueueInfos;
    bool             m_supportAnyTimestamp;
};

} // DbgOverlay
} // Pal
