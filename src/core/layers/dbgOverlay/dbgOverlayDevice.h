/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/layers/dbgOverlay/dbgOverlayPlatform.h"
#include "palMutex.h"

namespace Pal
{

struct DebugOverlaySettings;

namespace DbgOverlay
{

class TextWriter;
class TimeGraph;

// =====================================================================================================================
// DbgOverlay device decorator implementation.
class Device final : public Pal::DeviceDecorator
{
public:
    Device(PlatformDecorator* pPlatform, IDevice* pNextDevice);
    virtual ~Device() {}

    virtual Result Finalize(const DeviceFinalizeInfo& finalizeInfo) override;

    virtual Result Cleanup() override;

    virtual size_t GetQueueSize(
        const QueueCreateInfo& createInfo,
        Result*                pResult) const override;

    virtual Result CreateQueue(
        const QueueCreateInfo& createInfo,
        void*                  pPlacementAddr,
        IQueue**               ppQueue) override;

    virtual size_t GetMultiQueueSize(
        uint32                 queueCount,
        const QueueCreateInfo* pCreateInfo,
        Result*                pResult) const override;

    virtual Result CreateMultiQueue(
        uint32                 queueCount,
        const QueueCreateInfo* pCreateInfo,
        void*                  pPlacementAddr,
        IQueue**               ppQueue) override;

    virtual size_t GetCmdBufferSize(
        const CmdBufferCreateInfo& createInfo,
        Result*                    pResult) const override;

    virtual Result CreateCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        void*                      pPlacementAddr,
        ICmdBuffer**               ppCmdBuffer) override;

    virtual size_t GetImageSize(
        const ImageCreateInfo& createInfo,
        Result*                pResult) const override;

    virtual Result CreateImage(
        const ImageCreateInfo& createInfo,
        void*                  pPlacementAddr,
        IImage**               ppImage) override;

    virtual void GetPresentableImageSizes(
        const PresentableImageCreateInfo& createInfo,
        size_t*                           pImageSize,
        size_t*                           pGpuMemorySize,
        Result*                           pResult) const override;

    virtual Result CreatePresentableImage(
        const PresentableImageCreateInfo& createInfo,
        void*                             pImagePlacementAddr,
        void*                             pGpuMemoryPlacementAddr,
        IImage**                          ppImage,
        IGpuMemory**                      ppGpuMemory) override;

    virtual Result GetExternalSharedImageSizes(
        const ExternalImageOpenInfo& openInfo,
        size_t*                      pImageSize,
        size_t*                      pGpuMemorySize,
        ImageCreateInfo*             pImgCreateInfo) const override;

    virtual Result OpenExternalSharedImage(
        const ExternalImageOpenInfo& openInfo,
        void*                        pImagePlacementAddr,
        void*                        pGpuMemoryPlacementAddr,
        GpuMemoryCreateInfo*         pMemCreateInfo,
        IImage**                     ppImage,
        IGpuMemory**                 ppGpuMemory) override;

    virtual void GetPrivateScreenImageSizes(
        const PrivateScreenImageCreateInfo& createInfo,
        size_t*                             pImageSize,
        size_t*                             pGpuMemorySize,
        Result*                             pResult) const override;

    virtual Result CreatePrivateScreenImage(
        const PrivateScreenImageCreateInfo& createInfo,
        void*                               pImagePlacementAddr,
        void*                               pGpuMemoryPlacementAddr,
        IImage**                            ppImage,
        IGpuMemory**                        ppGpuMemory) override;

    // Sum and cache the total bytes of video memory allocated in each heap for all allocation types.
    void SumVidMemAllocations()
    {
        for (uint32 heapIdx = 0; heapIdx < GpuHeapCount; heapIdx++)
        {
            gpusize vidMemSum = 0;
            GpuHeap heapType  = static_cast<GpuHeap>(heapIdx);

            for (uint32 i = 0; i < AllocTypeCount; i++)
            {
                vidMemSum += GetVidMemTotal(static_cast<AllocType>(i), heapType);
            }

            m_perHeapMemTotals[heapIdx] = vidMemSum;
        }
    }

    // Gets the sum of the total bytes of video memory allocated for specified heap of all allocation types.
    gpusize GetVidMemTotalSum(Pal::GpuHeap gpuHeap) const { return m_perHeapMemTotals[gpuHeap]; }

    gpusize GetPeakMemTotal(Pal::GpuHeap gpuHeap)
    {
        m_peakVidMemTotals[gpuHeap] = Util::Max(m_peakVidMemTotals[gpuHeap], m_perHeapMemTotals[gpuHeap]);
        return m_peakVidMemTotals[gpuHeap];
    }

    // Gets the total bytes of video memory currently allocated preferring the specified heap.
    gpusize GetVidMemTotal(AllocType allocType, Pal::GpuHeap heap) const
        { return m_vidMemTotals[allocType][heap]; }

    // Adds to the total of video memory currently allocated preferring the specified heap.
    void AddAllocatedVidMem(AllocType allocType, Pal::GpuHeap heap, gpusize sizeInBytes)
        { Util::AtomicAdd64(&m_vidMemTotals[allocType][heap], sizeInBytes); }

    // Subtracts from the total video memory currently allocated preferring the specified heap.
    void SubFreedVidMem(AllocType allocType, Pal::GpuHeap heap, gpusize sizeInBytes)
        { Util::AtomicAdd64(&m_vidMemTotals[allocType][heap], (-1 * sizeInBytes)); }

    // Returns the memory heap properties of a particular heap
    const Pal::GpuMemoryHeapProperties& GetMemHeapProps(Pal::GpuHeap heap) const
        { return m_memHeapProps[static_cast<size_t>(heap)]; }

    const PalPublicSettings* GetSettings() const { return m_pSettings; }
    const DeviceProperties& GpuProps() const { return m_gpuProps; }
    const TextWriter& GetTextWriter() const { return *m_pTextWriter; }
    const TimeGraph& GetTimeGraph() const { return *m_pTimeGraph; }

    CmdAllocatorDecorator* InternalCmdAllocator() const { return m_pCmdAllocator; }
    uint32 MaxSrdSize() const { return m_maxSrdSize; }

    static bool DetermineDbgOverlaySupport(QueueType queueType)
        { return (queueType == QueueTypeUniversal) || (queueType == QueueTypeCompute); }

private:

    const PalPublicSettings*  m_pSettings;
    CmdAllocatorDecorator*    m_pCmdAllocator;
    TextWriter*               m_pTextWriter;
    TimeGraph*                m_pTimeGraph;
    DeviceProperties          m_gpuProps;
    uint32                    m_maxSrdSize;
    GpuMemoryHeapProperties   m_memHeapProps[GpuHeapCount];

    // Tracks the total bytes of video memory currently allocated via the external client.
    PAL_ALIGN_CACHE_LINE volatile gpusize m_vidMemTotals[AllocTypeCount][GpuHeapCount];

    gpusize m_perHeapMemTotals[GpuHeapCount]; // Represents total sum of allocations of all AllocTypes in each heap.
    gpusize m_peakVidMemTotals[GpuHeapCount]; // Represents peak sum of allocations of all AllocTypes in each heap.

    PAL_DISALLOW_DEFAULT_CTOR(Device);
    PAL_DISALLOW_COPY_AND_ASSIGN(Device);
};

} // DbgOverlay
} // Pal
