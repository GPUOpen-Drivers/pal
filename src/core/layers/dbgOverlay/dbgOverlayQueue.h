/*
 *******************************************************************************
 *
 * Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

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

// =====================================================================================================================
class Queue : public QueueDecorator
{
public:
    Queue(IQueue* pNextQueue, Device* pDevice, QueueType queueType, EngineType engineType);
    virtual ~Queue();

    Result Init();

    // Part of the IQueue public interface.
    virtual Result PresentDirect(const PresentDirectInfo& presentInfo) override;
    virtual Result PresentSwapChain(const PresentSwapChainInfo& presentInfo) override;

    virtual Result Submit(const SubmitInfo& submitInfo) override;

private:
    Result CreateCmdBuffer(const CmdBufferCreateInfo& createInfo, ICmdBuffer** ppCmdBuffer);
    Result CreateFence(
        const FenceCreateInfo& createInfo,
        IFence**               ppFence);

    Result CreateGpuTimestampPairMemory();

    Result SubmitOverlayCmdBuffer(const Image& image, PresentMode presentMode);
    Result SubmitWithGpuTimestampPair(const SubmitInfo& submitInfo, GpuTimestampPair* pTimestamp);

    Result CreateGpuTimestampPair(GpuTimestampPair** ppTimestamp);
    void DestroyGpuTimestampPair(GpuTimestampPair* pTimestamp);

    Result CreateTrackedCmdBuffer(TrackedCmdBuffer** ppTrackedCmdBuffer);
    void DestroyTrackedCmdBuffer(TrackedCmdBuffer* pTrackedCmdBuffer);

    static constexpr uint32 MaxGpuTimestampPairCount = 256;

    Device*const     m_pDevice;
    const QueueType  m_queueType;
    const EngineType m_engineType;
    const bool       m_overlaySupported;
    const bool       m_supportTimestamps;    // Queue (based on engine type) supports timestamps.
    const uint32     m_timestampAlignment;   // Aligns the Timestamps in Memory
    const size_t     m_timestampMemorySize;  // Size of the GpuTimestamp buffer (mapped CPU memory and GPU memory).
    size_t           m_nextTimestampOffset;  // Offset for the next GpuTimestamp
    void*            m_pMappedTimestampData; // Mapped memory pointing to the initial GpuTimestamp
    IGpuMemory*      m_pTimestampMemory;

    // Contains a Deque of GpuTimestampPair being used to record Timestamps where the least recently used
    // GpuTimestampPair is always at the front
    Util::Deque<GpuTimestampPair*, PlatformDecorator> m_gpuTimestampPairDeque;

    // All command buffers used by this queue to render the debug overlay. The least recently used item is at the front.
    Util::Deque<TrackedCmdBuffer*, PlatformDecorator> m_overlayCmdBufferDeque;
};

} // DbgOverlay
} // Pal
