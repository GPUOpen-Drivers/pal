/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/os/amdgpu/amdgpuHeaders.h"
#include "palHashMap.h"
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

namespace Amdgpu
{

class Device;
class GpuMemory;
class SwapChain;
class Platform;

enum class CommandListType : uint32
{
    Context0 = 0,   // Used for the QueueContext's optional first command stream
    Context1,       // Used for the QueueContext's optional second command stream
    Ce,             // Used for the command buffers' CE command stream
    De,             // used for hte command buffers' DE command stream
    Count,
};

// Maximum number of IB's we will specify in a single submission to the GPU.
constexpr uint32 MaxIbsPerSubmit = 16;

// Initial size of m_globalRefMap, the size of hashmap affects the performance of traversing the hashmap badly. When
// perVmBo enabled, there is usually less than 3 presentable image in the m_globalRefMap. So set it 16 is enough for
// most of the games when perVmBo enabled. When perVmBo disabled, set it 1024.
constexpr uint32 MemoryRefMapElementsPerVmBo = 16;
constexpr uint32 MemoryRefMapElements        = 1024;

// =====================================================================================================================
// The Linux SubmissionContext must own an amdgpu command submission context, the last submission fence on that context,
// and a few other bits of constant state.
class SubmissionContext : public Pal::SubmissionContext
{
public:
    static Result Create(
        Device*                  pDevice,
        EngineType               engineType,
        uint32                   engineId,
        QueuePriority            priority,
        bool                     isTmzOnly,
        Pal::SubmissionContext** ppContext);

    virtual bool IsTimestampRetired(uint64 timestamp) const override;

    uint32                IpType()   const { return m_ipType; }
    uint32                EngineId() const { return m_engineId; }
    amdgpu_context_handle Handle()   const { return m_hContext; }

    amdgpu_syncobj_handle GetLastSignaledSyncObj() const        { return m_lastSignaledSyncObject; }
    void SetLastSignaledSyncObj(amdgpu_syncobj_handle hSyncObj) { m_lastSignaledSyncObject = hSyncObj; }

private:
    SubmissionContext(const Device&      device,
                      EngineType         engineType,
                      uint32             engineId,
                      Pal::QueuePriority priority,
                      bool               isTmzOnly);
    virtual ~SubmissionContext();

    Result Init(Device* pDevice);

    const Device&               m_device;
    const uint32                m_ipType;    // This context's HW IP type as defined by amdgpu.
    const uint32                m_engineId;
    QueuePriority               m_queuePriority;
    bool                        m_isTmzOnly;
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
    Queue(uint32 qCount, Device* pDevice, const QueueCreateInfo* pCreateInfo);
    virtual ~Queue();

    virtual Result Init(const QueueCreateInfo* pCreateInfo, void* pContextPlacementAddr) override;

    bool IsPendingWait() const { return m_pendingWait; }

    Result WaitSemaphore(
        amdgpu_semaphore_handle hSemaphore,
        uint64                  value);

    Result SignalSemaphore(
        amdgpu_semaphore_handle hSemaphore,
        uint64                  value);

    virtual Result OsSubmit(
        const MultiSubmitInfo&    submitInfo,
        const InternalSubmitInfo* pInternalSubmitInfos) override;

    void  AssociateFenceWithContext(
        IFence* pFence);

    void DirtyGlobalReferences();

    Result AddGpuMemoryReferences(
        uint32              gpuMemRefCount,
        const GpuMemoryRef* pGpuMemoryRefs);

    void RemoveGpuMemoryReferences(
        uint32            gpuMemoryCount,
        IGpuMemory*const* ppGpuMemory,
        bool              forceRemove);

    Result DoAssociateFenceWithLastSubmit(Pal::Fence* pFence) override;

protected:
    virtual Result OsDelay(float delay, const IPrivateScreen* pScreen) override;

    virtual Result OsWaitIdle() override;
    virtual Result OsPresentDirect(const PresentDirectInfo& presentInfo) override;
    virtual Result OsRemapVirtualMemoryPages(
        uint32                         rangeCount,
        const VirtualMemoryRemapRange* pRanges,
        bool                           doNotWait,
        IFence*                        pFence) override;

    virtual Result OsCopyVirtualMemoryPageMappings(
        uint32                                    rangeCount,
        const VirtualMemoryCopyPageMappingsRange* pRanges,
        bool                                      doNotWait) override { return Result::ErrorUnavailable; }

    const Device&          m_device;
    amdgpu_bo_handle*      m_pResourceList;
    GpuMemory**            m_pResourceObjectList;
    uint8*                 m_pResourcePriorityList;
    const size_t           m_resourceListSize;
    size_t                 m_numResourcesInList;
    size_t                 m_numDummyResourcesInList;
    size_t                 m_memListResourcesInList; // The number of resources added from global memory list
    size_t                 m_memMgrResourcesInList;  // The number of resources added from internal memory manager

private:
    // Note: user MUST lock the mutex m_globalRefLock before calling this function and ensure the lock remains until all
    // functions have finished accessing m_globalRefMap and the memory pointed to by that map. This includes Submit*()
    Result UpdateResourceList(
        const GpuMemoryRef*    pMemRefList,
        size_t                 memRefCount);

    Result AppendResourceToList(
        GpuMemory* pGpuMemory);

    Result AppendGlobalResourceToList(
        GpuMemory* pGpuMemory);

    Result AddCmdStream(
        const CmdStream& cmdStream,
        uint32           engineId,
        bool             isDummySubmission,
        bool             isTmzEnabled);

    Result AddIb(
        gpusize       gpuVirtAddr,
        uint32        sizeInDwords,
        EngineType    engineType,
        SubEngineType subEngineType,
        uint32        engineId,
        bool          isPreemptionEnabled,
        bool          dropIfSameContext,
        bool          isTmzEnabled);

    Result SubmitIbs(
        const InternalSubmitInfo& internalSubmitInfo);

    Result SubmitIbsRaw(
        const InternalSubmitInfo& internalSubmitInfo);

    Result SubmitPm4(
        const MultiSubmitInfo&    submitInfo,
        const InternalSubmitInfo& internalSubmitInfo);

    Result SubmitMultiQueuePm4(
        const MultiSubmitInfo&    submitInfo,
        const InternalSubmitInfo* internalSubmitInfo);

    Result PrepareChainedCommandBuffers(
        const InternalSubmitInfo& internalSubmitInfo,
        uint32                    cmdBufferCount,
        ICmdBuffer*const*         ppCmdBuffers,
        uint32*                   pAppendedCmdBuffers,
        uint32                    engineId,
        const bool                isMultiQueue = false);

    Result PrepareUploadedCommandBuffers(
        const InternalSubmitInfo& internalSubmitInfo,
        uint32                    cmdBufferCount,
        ICmdBuffer*const*         ppCmdBuffers,
        uint32*                   pAppendedCmdBuffers,
        IQueueSemaphore**         ppWaitBeforeLaunch,
        IQueueSemaphore**         ppSignalAfterLaunch);

    Result SubmitNonGfxIp(
        const MultiSubmitInfo&    submitInfo,
        const InternalSubmitInfo& internalSubmitInfo);

    void IncrementDummySubmitCount(
        const InternalSubmitInfo* internalSubmitInfo,
        ICmdBuffer*const*        &ppCmdBuffers,
        uint32                   &cmdBufferCount);

    // Tracks global memory references for this queue. Each key is a GPU memory object and each value is a refcount.
    typedef Util::HashMap<IGpuMemory*, uint32, Pal::Platform> MemoryRefMap;

    // Kernel object representing a list of GPU memory allocations referenced by a submit.
    // Stored as a member variable to prevent re-creating the kernel object on every submit
    // in the common case where the set of resident allocations doesn't change.
    amdgpu_bo_list_handle m_hResourceList;
    amdgpu_bo_list_handle m_hDummyResourceList;    // The dummy resource list used by dummy submission.
    uint32                m_dummyResourceList;     // The dummy resource list handle used by raw2 submission.
    Util::Vector<drm_amdgpu_bo_list_entry, 1, Platform> m_dummyResourceEntryList;
                                                   // Used by amdgpu_cs_submit_raw2, saves kms_handle and priority
    Pal::CmdStream*    m_pDummyCmdStream;          // The dummy command stream used by dummy submission.
    MemoryRefMap       m_globalRefMap;             // A hashmap acting as a refcounted list of memory references.
    bool               m_globalRefDirty;           // Indicates m_globalRefMap has changed since the last submit.
    Util::RWLock       m_globalRefLock;            // Protect m_globalRefMap from muli-thread access.
    uint32             m_appMemRefCount;           // Store count of application's submission memory references.
    bool               m_pendingWait;              // Queue needs a dummy submission between wait and signal.
    CmdUploadRing*     m_pCmdUploadRing;           // Uploads gfxip command streams to a large local memory buffer.
    bool               m_sqttWaRequired;           // if a perfCounter in any cmdBuffer is active we need to tell KMD
    bool               m_perfCtrWaRequired;        // if SQ Thread Trace in any cmdBuffer is active we need to tell KMD

    // These IBs will be sent to the kernel when SubmitIbs is called.
    uint32                 m_numIbs;
    drm_amdgpu_cs_chunk_ib m_ibs[MaxIbsPerSubmit];

    // The sync object refers to the fence of last submission.
    amdgpu_syncobj_handle m_lastSignaledSyncObject;

    struct SemaphoreInfo
    {
        amdgpu_semaphore_handle hSemaphore;
        uint64                  value;
    };
    // The vector to store the pending wait semaphore when sync object is in using.
    Util::Vector<SemaphoreInfo, 16, Platform> m_waitSemList;

    // If we are using the ImplicitAce along with Gfx.
    bool m_requiresGangedInterface;

    PAL_DISALLOW_DEFAULT_CTOR(Queue);
    PAL_DISALLOW_COPY_AND_ASSIGN(Queue);
};

} // Amdgpu
} // Pal
