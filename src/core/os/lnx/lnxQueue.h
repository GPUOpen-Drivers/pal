/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/os/lnx/lnxHeaders.h"
#include "palList.h"
#include "palVector.h"

// It is a temporary solution while we are waiting for open source promotion.
// The VCN IPs are going to be added in the open source header Dk/drm/amdgpu_drm.h.
#ifndef AMDGPU_HW_IP_VCN_DEC
#define AMDGPU_HW_IP_VCN_DEC      6
#endif
#ifndef AMDGPU_HW_IP_VCN_ENC
#define AMDGPU_HW_IP_VCN_ENC      7
#endif

namespace Pal
{

class CmdUploadRing;
class Image;
class GpuMemory;

namespace Linux
{

class Device;
class GpuMemory;
class SwapChain;

enum class CommandListType : uint32
{
    Context0 = 0,   // Used for the QueueContext's optional first command stream
    Context1,       // Used for the QueueContext's optional second command stream
    Ce,             // Used for the command buffers' CE command stream
    De,             // used for hte command buffers' DE command stream
    Count,
};

// =====================================================================================================================
// The Linux SubmissionContext must own an amdgpu command submission context, the last submission fence on that context,
// and a few other bits of constant state.
class SubmissionContext : public Pal::SubmissionContext
{
public:
    static Result Create(
        const Device&            device,
        EngineType               engineType,
        uint32                   engineId,
        QueuePriority            priority,
        Pal::SubmissionContext** ppContext);

    virtual bool IsTimestampRetired(uint64 timestamp) const override;

    uint32                IpType()   const { return m_ipType; }
    uint32                EngineId() const { return m_engineId; }
    amdgpu_context_handle Handle()   const { return m_hContext; }

    amdgpu_syncobj_handle GetLastSignaledSyncObj() const        { return m_lastSignaledSyncObject; }
    void SetLastSignaledSyncObj(amdgpu_syncobj_handle hSyncObj) { m_lastSignaledSyncObject = hSyncObj; }

private:
    SubmissionContext(const Device& device, EngineType engineType, uint32 engineId, Pal::QueuePriority priority);
    virtual ~SubmissionContext();

    Result Init();

    const Device&               m_device;
    const uint32                m_ipType;    // This context's HW IP type as defined by amdgpu.
    const uint32                m_engineId;
    QueuePriority               m_queuePriority;
    amdgpu_syncobj_handle       m_lastSignaledSyncObject;
    amdgpu_context_handle       m_hContext;  // Command submission context handle.

    PAL_DISALLOW_DEFAULT_CTOR(SubmissionContext);
    PAL_DISALLOW_COPY_AND_ASSIGN(SubmissionContext);
};

// =====================================================================================================================
// Linux flavor of the Queue class: manages a amdgpu resource list which gets submitted along
// with each command buffer submission.
class Queue : public Pal::Queue
{
public:
    Queue(Device* pDevice, const QueueCreateInfo& createInfo);
    virtual ~Queue();

    virtual Result Init(void* pContextPlacementAddr) override;

    // NOTE: Part of the public IQueue interface.
    Result AddGpuMemoryReferences(
        uint32              gpuMemRefCount,
        const GpuMemoryRef* pGpuMemoryRefs);

    Result RemoveGpuMemoryReferences(
        uint32            gpuMemoryCount,
        IGpuMemory*const* ppGpuMemory);

    virtual Result RemapVirtualMemoryPages(
        uint32                         rangeCount,
        const VirtualMemoryRemapRange* pRangeList,
        bool                           doNotWait,
        IFence*                        pFence) override;

    virtual Result CopyVirtualMemoryPageMappings(
        uint32                                    rangeCount,
        const VirtualMemoryCopyPageMappingsRange* pRanges,
        bool                                      doNotWait) override { return Result::ErrorUnavailable; }

    bool IsPendingWait() const { return m_pendingWait; }

    Result WaitSemaphore(
        amdgpu_semaphore_handle hSemaphore,
        uint64                  value);

    Result SignalSemaphore(
        amdgpu_semaphore_handle hSemaphore,
        uint64                  value);

    virtual Result OsSubmit(
        const SubmitInfo&         submitInfo,
        const InternalSubmitInfo& internalSubmitInfo) override;

    void  AssociateFenceWithContext(
        IFence* pFence);

protected:
    virtual Result OsDelay(float delay, const IPrivateScreen* pScreen) override;

    virtual Result OsWaitIdle() override;
    virtual Result OsPresentDirect(const PresentDirectInfo& presentInfo) override;

    Result DoAssociateFenceWithLastSubmit(Pal::Fence* pFence) override;

    const Device&          m_device;
    amdgpu_bo_handle*const m_pResourceList;
    const size_t           m_resourceListSize;
    size_t                 m_numResourcesInList;
    size_t                 m_memListResourcesInList; // The number of resources added from global memory list
    size_t                 m_memMgrResourcesInList;  // The number of resources added from internal memory manager

private:
    Result UpdateResourceList(
        const GpuMemoryRef*    pMemRefList,
        size_t                 memRefCount);

    Result AppendResourceToList(
        const GpuMemory* pGpuMemory);

    Result AddCmdStream(
        const CmdStream& cmdStream);

    Result AddIb(
        gpusize gpuVirtAddr,
        uint32  sizeInDwords,
        bool    isConstantEngine,
        bool    isPreemptionEnabled,
        bool    dropIfSameContext);

    Result SubmitIbs(
        const InternalSubmitInfo& internalSubmitInfo,
        bool                      isDummySubmission);

    Result SubmitIbsRaw(
        const InternalSubmitInfo& internalSubmitInfo,
        bool                      isDummySubmission);

    Result SubmitPm4(
        const SubmitInfo&         submitInfo,
        const InternalSubmitInfo& internalSubmitInfo,
        bool                      isDummySubmission);

    Result PrepareChainedCommandBuffers(
        const InternalSubmitInfo& internalSubmitInfo,
        uint32                    cmdBufferCount,
        ICmdBuffer*const*         ppCmdBuffers,
        uint32*                   pAppendedCmdBuffers);

    Result PrepareUploadedCommandBuffers(
        const InternalSubmitInfo& internalSubmitInfo,
        uint32                    cmdBufferCount,
        ICmdBuffer*const*         ppCmdBuffers,
        uint32*                   pAppendedCmdBuffers,
        IQueueSemaphore**         ppWaitBeforeLaunch,
        IQueueSemaphore**         ppSignalAfterLaunch);

    Result SubmitNonGfxIp(
        const SubmitInfo&         submitInfo,
        const InternalSubmitInfo& internalSubmitInfo,
        bool                      isDummySubmission);

    // Kernel object representing a list of GPU memory allocations referenced by a submit.
    // Stored as a member variable to prevent re-creating the kernel object on every submit
    // in the common case where the set of resident allocations doesn't change.
    amdgpu_bo_list_handle m_hResourceList;
    amdgpu_bo_list_handle m_hDummyResourceList;   // The dummy resource list used by dummy submission.
    Pal::GpuMemory*       m_pDummyGpuMemory;      // The dummy gpu memory used by dummy resource list.
    bool                  m_memListDirty;         // Indicates m_memList has changed since the last submit.
    Util::RWLock          m_memListLock;          // Protect m_memListLock from muli-thread access.
    uint32                m_internalMgrTimestamp; // Store timestamp of internal memory mgr.
    uint32                m_appMemRefCount;       // Store count of application's submission memory references.
    bool                  m_pendingWait;          // Queue needs a dummy submission between wait and signal.
    CmdUploadRing*        m_pCmdUploadRing;       // Uploads gfxip command streams to a large local memory buffer.

    Util::List<IGpuMemory*, Platform> m_memList;  // List of memory which is referenced by Queue.

    // These IBs will be sent to the kernel when SubmitIbs is called.
    uint32                m_numIbs;
    amdgpu_cs_ib_info     m_ibs[MaxIbsPerSubmit];

    // The sync object refers to the fence of last submission.
    amdgpu_syncobj_handle m_lastSignaledSyncObject;

    struct SemaphoreInfo
    {
        amdgpu_semaphore_handle hSemaphore;
        uint64                  value;
    };
    // The vector to store the pending wait semaphore when sync object is in using.
    Util::Vector<SemaphoreInfo, 16, Platform> m_waitSemList;

    PAL_DISALLOW_DEFAULT_CTOR(Queue);
    PAL_DISALLOW_COPY_AND_ASSIGN(Queue);
};

} // Linux
} // Pal
