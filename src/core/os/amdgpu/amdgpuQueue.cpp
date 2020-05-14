/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/cmdAllocator.h"
#include "core/engine.h"
#include "core/hw/gfxip/cmdUploadRing.h"
#include "core/hw/gfxip/universalCmdBuffer.h"
#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuGpuMemory.h"
#include "core/os/amdgpu/amdgpuImage.h"
#include "core/os/amdgpu/amdgpuPlatform.h"
#include "core/os/amdgpu/amdgpuQueue.h"
#include "core/os/amdgpu/amdgpuSyncobjFence.h"
#include "core/os/amdgpu/amdgpuTimestampFence.h"
#include "core/queueSemaphore.h"

#include "palAutoBuffer.h"
#include "palDequeImpl.h"
#include "palListImpl.h"
#include "palHashMapImpl.h"
#include "palVectorImpl.h"

#include <climits>

using namespace Util;

namespace Pal
{
namespace Amdgpu
{

// Lookup table for converting GpuMemPriority enums to resource priority value.
static constexpr uint8 LnxResourcePriorityTable[] =
{
    0,  // Unused
    1,  // VeryLow
    2,  // Low
    3,  // Normal
    4,  // High
    5,  // VeryHigh
};

// =====================================================================================================================
// Helper function to get the IP type from engine type
static uint32 GetIpType(
    EngineType engineType)
{
    uint32 ipType = 0;

    switch (engineType)
    {
    case EngineTypeUniversal:
        ipType = AMDGPU_HW_IP_GFX;
        break;
    case EngineTypeCompute:
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 530
    case EngineTypeExclusiveCompute:
#endif
        ipType = AMDGPU_HW_IP_COMPUTE;
        break;
    case EngineTypeDma:
        ipType = AMDGPU_HW_IP_DMA;
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return ipType;
}

// =====================================================================================================================
Result SubmissionContext::Create(
    const Device&            device,
    EngineType               engineType,
    uint32                   engineId,
    Pal::QueuePriority       priority,
    Pal::SubmissionContext** ppContext)
{
    Result     result   = Result::ErrorOutOfMemory;
    auto*const pContext =
        PAL_NEW(SubmissionContext, device.GetPlatform(), AllocInternal)(device, engineType, engineId, priority);

    if (pContext != nullptr)
    {
        result = pContext->Init();

        if (result == Result::Success)
        {
            *ppContext = pContext;
        }
        else
        {
            // Note that we take a reference on construction so we must destroy our incomplete object this way.
            pContext->ReleaseReference();
        }
    }

    return result;
}

// =====================================================================================================================
SubmissionContext::SubmissionContext(
    const Device&       device,
    EngineType          engineType,
    uint32              engineId,
    Pal::QueuePriority  priority)
    :
    Pal::SubmissionContext(device.GetPlatform()),
    m_device(device),
    m_ipType(GetIpType(engineType)),
    m_engineId(engineId),
    m_queuePriority(priority),
    m_lastSignaledSyncObject(0),
    m_hContext(nullptr)
{
}

// =====================================================================================================================
SubmissionContext::~SubmissionContext()
{
    if (m_hContext != nullptr)
    {
        const Result result = m_device.DestroyCommandSubmissionContext(m_hContext);
        PAL_ASSERT(result == Result::Success);

        m_hContext = nullptr;
    }
}

// =====================================================================================================================
Result SubmissionContext::Init()
{
    return m_device.CreateCommandSubmissionContext(&m_hContext, m_queuePriority);
}

// =====================================================================================================================
// Queries if a particular fence timestamp has been retired by the GPU.
bool SubmissionContext::IsTimestampRetired(
    uint64 timestamp
    ) const
{
    struct amdgpu_cs_fence queryFence = {};

    queryFence.context     = m_hContext;
    queryFence.fence       = timestamp;
    queryFence.ring        = m_engineId;
    queryFence.ip_instance = 0;
    queryFence.ip_type     = m_ipType;

    return (m_device.QueryFenceStatus(&queryFence, 0) == Result::Success);
}

// =====================================================================================================================
Queue::Queue(
    uint32                 qCount,
    Device*                pDevice,
    const QueueCreateInfo* pCreateInfo)
    :
    Pal::Queue(qCount, pDevice, pCreateInfo),
    m_device(*pDevice),
    m_pResourceList(reinterpret_cast<amdgpu_bo_handle*>(this + 1)),
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 479)
    m_pResourcePriorityList(pCreateInfo[0].enableGpuMemoryPriorities ?
        reinterpret_cast<uint8*>(m_pResourceList + Pal::Device::CmdBufMemReferenceLimit) : nullptr),
#else
    m_pResourcePriorityList(nullptr),
#endif
    m_resourceListSize(Pal::Device::CmdBufMemReferenceLimit),
    m_numResourcesInList(0),
    m_memListResourcesInList(0),
    m_memMgrResourcesInList(0),
    m_hResourceList(nullptr),
    m_hDummyResourceList(nullptr),
    m_pDummyCmdStream(nullptr),
    m_globalRefMap(static_cast<Device*>(m_pDevice)->IsVmAlwaysValidSupported() ? MemoryRefMapElementsPerVmBo :
                   MemoryRefMapElements, m_pDevice->GetPlatform()),
    m_globalRefDirty(true),
    m_appMemRefCount(0),
    m_pendingWait(false),
    m_pCmdUploadRing(nullptr),
    m_numIbs(0),
    m_lastSignaledSyncObject(0),
    m_waitSemList(pDevice->GetPlatform())
{
    memset(m_ibs, 0, sizeof(m_ibs));
}

// =====================================================================================================================
Queue::~Queue()
{
    if (m_pCmdUploadRing != nullptr)
    {
        m_pCmdUploadRing->DestroyInternal();
    }

    if (m_hResourceList != nullptr)
    {
        static_cast<Device*>(m_pDevice)->DestroyResourceList(m_hResourceList);
    }

    if (m_hDummyResourceList != nullptr)
    {
        static_cast<Device*>(m_pDevice)->DestroyResourceList(m_hDummyResourceList);
    }

    if (m_pDummyCmdStream != nullptr)
    {
        m_pDummyCmdStream = nullptr;
    }

    if (m_lastSignaledSyncObject > 0)
    {
        static_cast<Device*>(m_pDevice)->DestroySyncObject(m_lastSignaledSyncObject);
    }
}

// =====================================================================================================================
// Initializes this Queue object.
Result Queue::Init(
    const QueueCreateInfo* pCreateInfo,
    void*                  pContextPlacementAddr)
{
    Result result = Pal::Queue::Init(pCreateInfo, pContextPlacementAddr);

    if (result == Result::Success)
    {
        result = SubmissionContext::Create(static_cast<Device&>(*m_pDevice),
                                           GetEngineType(),
                                           EngineId(),
                                           Priority(),
                                           &m_pSubmissionContext);
    }

    if (result == Result::Success)
    {
        result = m_globalRefMap.Init();
    }

    if (result == Result::Success)
    {
        result = m_globalRefLock.Init();
    }

    // Note that the presence of the command upload ring will be used later to determine if these conditions are true.
    if ((result == Result::Success)                               &&
        (m_device.Settings().disableSdmaEngine == false)          &&
        (m_device.ChipProperties().ossLevel != OssIpLevel::_None) &&
        (m_pQueueInfos[0].createInfo.submitOptMode != SubmitOptMode::Disabled))
    {
        const bool supportsGraphics = Pal::Device::EngineSupportsGraphics(GetEngineType());
        const bool supportsCompute  = Pal::Device::EngineSupportsCompute(GetEngineType());

        // By default we only enable the command upload ring for graphics queues but we can also support compute queues
        // if the client asks for it.
        if (supportsGraphics ||
            (supportsCompute && (m_pQueueInfos[0].createInfo.submitOptMode != SubmitOptMode::Default)))
        {
            CmdUploadRingCreateInfo createInfo = {};
            createInfo.engineType    = GetEngineType();
            createInfo.numCmdStreams = supportsGraphics ? UniversalCmdBuffer::NumCmdStreamsVal : 1;

            result = m_pDevice->GetGfxDevice()->CreateCmdUploadRingInternal(createInfo, &m_pCmdUploadRing);
        }
    }

    if (result == Result::Success)
    {
        Device* pDevice = static_cast<Device*>(m_pDevice);

        Vector<amdgpu_bo_handle, 1, Platform> dummyResourceList(pDevice->GetPlatform());

        m_pDummyCmdStream = m_pDevice->GetDummyCommandStream(GetEngineType());

        if (m_pDummyCmdStream != nullptr)
        {
            for (auto iter = m_pDummyCmdStream->GetFwdIterator(); iter.IsValid(); iter.Next())
            {
                const CmdStreamChunk*const pChunk = iter.Get();
                dummyResourceList.PushBack(static_cast<GpuMemory*>(pChunk->GpuMemory())->SurfaceHandle());
            }
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }

        if (result == Result::Success)
        {
            result = pDevice->CreateResourceList(dummyResourceList.NumElements(),
                                                 &(dummyResourceList.Front()),
                                                 nullptr,
                                                 &m_hDummyResourceList);
        }
    }

    // create sync object to track submission state if it is supported.
    if ((result == Result::Success) &&
        (static_cast<Device*>(m_pDevice)->GetSemaphoreType() == SemaphoreType::SyncObj))
    {
        result =  static_cast<Device*>(m_pDevice)->CreateSyncObject(0, &m_lastSignaledSyncObject);
    }

    return result;
}

// =====================================================================================================================
// Adds GPU memory references to the per-queue global list which gets added to the patch/alloc list at submit time.
Result Queue::AddGpuMemoryReferences(
    uint32              gpuMemRefCount,
    const GpuMemoryRef* pGpuMemoryRefs)
{
    Result result = Result::Success;

    RWLockAuto<RWLock::ReadWrite> lock(&m_globalRefLock);

    for (uint32 idx = 0; (idx < gpuMemRefCount) && (result == Result::Success); ++idx)
    {
        uint32*    pRefCount     = nullptr;
        bool       alreadyExists = false;
        GpuMemory* pGpuMemory    = reinterpret_cast<GpuMemory*>(pGpuMemoryRefs[idx].pGpuMemory);

        if (pGpuMemory->IsVmAlwaysValid())
        {
            continue;
        }

        result = m_globalRefMap.FindAllocate(pGpuMemory, &alreadyExists, &pRefCount);

        if (result == Result::Success)
        {
            if (alreadyExists)
            {
                // The reference is already in the map, increment the ref count.
                (*pRefCount)++;
            }
            else
            {
                // Initialize the new value with one reference.
                *pRefCount       = 1;
                m_globalRefDirty = true;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Decrements the GPU memory reference count and if necessary removes it from the per-queue global list.
void Queue::RemoveGpuMemoryReferences(
    uint32            gpuMemoryCount,
    IGpuMemory*const* ppGpuMemory,
    bool              forceRemove)
{
    RWLockAuto<RWLock::ReadWrite> lock(&m_globalRefLock);

    for (uint32 idx = 0; idx < gpuMemoryCount; ++idx)
    {
        uint32* pRefCount = m_globalRefMap.FindKey(ppGpuMemory[idx]);

        if (pRefCount != nullptr)
        {
            PAL_ASSERT(*pRefCount > 0);
            (*pRefCount)--;

            if ((*pRefCount == 0) || forceRemove)
            {
                m_globalRefMap.Erase(ppGpuMemory[idx]);
                m_globalRefDirty = true;
            }
        }
    }
}

// =====================================================================================================================
// Remapping the physical memory with new virtual address.
Result Queue::RemapVirtualMemoryPages(
    uint32                         rangeCount,
    const VirtualMemoryRemapRange* pRangeList,
    bool                           doNotWait,   // Ignored on Linux platforms.
    IFence*                        pFence)
{
    Result     result  = Result::Success;
    auto*const pDevice = static_cast<Amdgpu::Device*>(m_pDevice);

    if (rangeCount == 0)
    {
        result = Result::ErrorInvalidValue;
    }
    else if (pRangeList == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }

    for (uint32 idx = 0; ((idx < rangeCount) && (result == Result::Success)); ++idx)
    {
        GpuMemory*const       pVirtGpuMem = static_cast<GpuMemory*>(pRangeList[idx].pVirtualGpuMem);
        const GpuMemoryDesc   gpuMemDesc  = pVirtGpuMem->Desc();
        const GpuMemory*const pRealGpuMem = static_cast<const GpuMemory*>(pRangeList[idx].pRealGpuMem);

        const gpusize pageSize = m_device.MemoryProperties().virtualMemPageSize;

        if ((pRangeList[idx].size == 0) || ((pRangeList[idx].size % pageSize) != 0))
        {
            result = Result::ErrorInvalidValue;
        }
        else if (pVirtGpuMem == nullptr)
        {
            result = Result::ErrorInvalidPointer;
        }
        else if (pVirtGpuMem->IsVirtual() == false)
        {
            result = Result::ErrorInvalidObjectType;
        }
        else if (((pRangeList[idx].virtualStartOffset % pageSize) != 0) ||
                 (pVirtGpuMem->IsByteRangeValid(pRangeList[idx].virtualStartOffset,
                                                pRangeList[idx].size) == false))
        {
            result = Result::ErrorInvalidValue;
        }
        else if (pRealGpuMem == nullptr)
        {
            result = pDevice->ReplacePrtVirtualAddress(nullptr,
                         0,
                         pRangeList[idx].size,
                         gpuMemDesc.gpuVirtAddr + pRangeList[idx].virtualStartOffset,
                         pVirtGpuMem->Mtype());
        }
        else
        {
            if (pRealGpuMem->IsVirtual())
            {
                result = Result::ErrorInvalidObjectType;
            }
            else if (((pRangeList[idx].realStartOffset % pageSize) != 0) ||
                     (pRealGpuMem->IsByteRangeValid(pRangeList[idx].realStartOffset,
                                                    pRangeList[idx].size) == false))
            {
                result = Result::ErrorInvalidValue;
            }
            else
            {
                gpusize offset = 0;

                if (result == Result::Success)
                {
                    const gpusize pageSize  = pDevice->MemoryProperties().virtualMemPageSize;

                    gpusize  virtualAddress = gpuMemDesc.gpuVirtAddr + pRangeList[idx].virtualStartOffset;
                    gpusize  size           = pRangeList[idx].size;
                    offset                  = pRangeList[idx].realStartOffset;

                    result = pDevice->ReplacePrtVirtualAddress(pRealGpuMem->SurfaceHandle(),
                                                               offset,
                                                               size,
                                                               virtualAddress,
                                                               pVirtGpuMem->Mtype());
                }
            }
        }
    }

    if ((pFence != nullptr) && (result == Result::Success))
    {
        result = Queue::SubmitFence(pFence);
    }

    return result;
}

// =====================================================================================================================
Result Queue::WaitSemaphore(
    amdgpu_semaphore_handle hSemaphore,
    uint64                  value)
{
    Result result = Result::Success;
    const auto& device  = static_cast<Device&>(*m_pDevice);
    const auto& context = static_cast<SubmissionContext&>(*m_pSubmissionContext);
    if (device.GetSemaphoreType() == SemaphoreType::SyncObj)
    {
        struct SemaphoreInfo semaphoreInfo = { };

        semaphoreInfo.hSemaphore    = hSemaphore;
        semaphoreInfo.value         = value;
        result = m_waitSemList.PushBack(semaphoreInfo);
    }
    else
    {
        result = device.WaitSemaphore(context.Handle(), context.IpType(), 0, context.EngineId(), hSemaphore);
    }

    // For the legacy semaphore interfaces,
    // the wait semaphore operation do not take effect without a dummy submission.
    if ((device.SemWaitRequiresSubmission()) && (result == Result::Success))
    {
        m_pendingWait = true;
    }

    return result;
}

// =====================================================================================================================
Result Queue::SignalSemaphore(
    amdgpu_semaphore_handle hSemaphore,
    uint64                  value)
{
    Result result       = Result::Success;
    const auto& device  = static_cast<Device&>(*m_pDevice);
    const auto& context = static_cast<SubmissionContext&>(*m_pSubmissionContext);

    if ((m_pendingWait == true) || (context.LastTimestamp() == 0))
    {
        result = DummySubmit(true);
    }

    if (result == Result::Success)
    {
        if (device.GetSemaphoreType() == SemaphoreType::SyncObj)
        {
            result = device.ConveySyncObjectState(
                         reinterpret_cast<uintptr_t>(hSemaphore),
                         value,
                         m_lastSignaledSyncObject,
                         0);
        }
        else
        {
            result = device.SignalSemaphore(context.Handle(), context.IpType(), 0, context.EngineId(), hSemaphore);
        }
    }
    return result;
}

// =====================================================================================================================
// Perform low-level Delay behavior for a Queue. NOTE: Linux doesn't yet support Timer Queues.
Result Queue::OsDelay(
    float                 delay,
    const IPrivateScreen* pScreen)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
// Submits one or more command buffers to the hardware using command submission context.
Result Queue::OsSubmit(
    const MultiSubmitInfo&    submitInfo,
    const InternalSubmitInfo* pInternalSubmitInfos)
{
    // If this triggers we forgot to flush one or more IBs to the GPU during the previous submit.
    PAL_ASSERT(m_numIbs == 0);
    PAL_ASSERT(submitInfo.perSubQueueInfoCount <= 1);

    Result result = Result::Success;

    if (pInternalSubmitInfos->flags.isDummySubmission == false)
    {
        result = UpdateResourceList(submitInfo.pGpuMemoryRefs, submitInfo.gpuMemRefCount);
    }

    if (result == Result::Success)
    {
        MultiSubmitInfo       localSubmitInfo       = submitInfo;
        PerSubQueueSubmitInfo perSubQueueSubmitInfo = {};
        if (pInternalSubmitInfos->flags.isDummySubmission == false)
        {
            perSubQueueSubmitInfo            = submitInfo.pPerSubQueueInfo[0];
        }
        localSubmitInfo.pPerSubQueueInfo     = &perSubQueueSubmitInfo;
        localSubmitInfo.perSubQueueInfoCount = 1;

        // amdgpu won't give us a new fence value unless the submission has at least one command buffer.
        if ((pInternalSubmitInfos->flags.isDummySubmission) || (m_ifhMode == IfhModePal))
        {
            perSubQueueSubmitInfo.ppCmdBuffers =
                reinterpret_cast<Pal::ICmdBuffer* const*>(&m_pDummyCmdBuffer);
            perSubQueueSubmitInfo.cmdBufferCount  = 1;
            if ((m_ifhMode != IfhModePal) && (pInternalSubmitInfos->flags.isDummySubmission == false))
            {
                m_pDummyCmdBuffer->IncrementSubmitCount();
            }
        }

        // Clear pending wait flag.
        m_pendingWait = false;

        if ((Type() == QueueTypeUniversal) || (Type() == QueueTypeCompute))
        {
            result = SubmitPm4(localSubmitInfo, pInternalSubmitInfos[0]);
        }
        else if ((Type() == QueueTypeDma)
                )
        {
            result = SubmitNonGfxIp(localSubmitInfo, pInternalSubmitInfos[0]);
        }
    }

    // By default, we don't destroy the allocation list object and attempt to reuse it for the next submit.
    // This can cause issues, though, if an app doesn't regularly submit on every queue, since the existence
    // of this list will prevent the kernel from freeing memory immediately when requested by an application.
    // Setting allocationListReusable to false will prevent this particular problem,
    // and cause us to recreate m_hResourceList on every submit.
    if ((result == Result::Success)  &&
        (m_hResourceList != nullptr) &&
        (m_pDevice->Settings().allocationListReusable == false))
    {
        result = static_cast<Device*>(m_pDevice)->DestroyResourceList(m_hResourceList);
        m_hResourceList = nullptr;
    }

    // Update the fence
    if ((result == Result::Success) &&
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 568
        (submitInfo.fenceCount > 0))
    {
        for (uint32 i = 0; i < submitInfo.fenceCount; i++)
        {
            DoAssociateFenceWithLastSubmit(static_cast<Pal::Fence*>(submitInfo.ppFences[i]));
        }
    }
#else
        (submitInfo.pFence != nullptr))
    {
        DoAssociateFenceWithLastSubmit(static_cast<Pal::Fence*>(submitInfo.pFence));
    }
#endif

    return result;
}

// =====================================================================================================================
// Executes a direct present without any batching. NOTE: Linux doesn't support direct presents.
Result Queue::OsPresentDirect(
    const PresentDirectInfo& presentInfo)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
// Submits one or more PM4 command buffers.
Result Queue::SubmitPm4(
    const MultiSubmitInfo&    submitInfo,
    const InternalSubmitInfo& internalSubmitInfo)
{
    Result result = Result::Success;

    // The OsSubmit function should guarantee that we have at least one universal or compute command buffer.
    PAL_ASSERT(submitInfo.pPerSubQueueInfo[0].cmdBufferCount > 0);
    PAL_ASSERT((Type() == QueueTypeUniversal) || (Type() == QueueTypeCompute));

    // For linux platforms, there will exist at most 3 preamble + 2 postamble:
    // Preamble  CE IB (always)
    // Preamble  DE IB (always)
    // Preamble  DE IB (if context switch)
    // Postamble CE IB
    // Postamble DE IB
    PAL_ASSERT((internalSubmitInfo.numPreambleCmdStreams + internalSubmitInfo.numPostambleCmdStreams) <= 5);

    // Determine which optimization modes should be enabled for this submit.
    const bool minGpuCmdOverhead     = (m_pQueueInfos[0].createInfo.submitOptMode == SubmitOptMode::MinGpuCmdOverhead);
    bool       tryToUploadCmdBuffers = false;

    if (m_pCmdUploadRing != nullptr)
    {
        if (minGpuCmdOverhead)
        {
            // We should upload all command buffers because the command ring is in the local heap.
            tryToUploadCmdBuffers = true;
        }
        else if (submitInfo.pPerSubQueueInfo[0].cmdBufferCount > 1)
        {
            // Otherwise we're doing the MinKernelSubmits or Default paths which only want to upload command buffers
            // if it will save us kernel submits. This means we shouldn't upload if we only have one command buffer
            // or if all of the command buffers can be chained together.
            for (uint32 idx = 0; idx < submitInfo.pPerSubQueueInfo[0].cmdBufferCount - 1; ++idx)
            {
                if (static_cast<CmdBuffer*>(
                    submitInfo.pPerSubQueueInfo[0].ppCmdBuffers[idx])->IsExclusiveSubmit() == false)
                {

                    tryToUploadCmdBuffers = true;
                    break;
                }
            }
        }
    }

    // Iteratively build batches of command buffers and launch their command streams.
    uint32            numNextCmdBuffers = submitInfo.pPerSubQueueInfo[0].cmdBufferCount;
    ICmdBuffer*const* ppNextCmdBuffers  = submitInfo.pPerSubQueueInfo[0].ppCmdBuffers;

    // If spm enabled commands included, reserve a vmid so that the SPM_VMID could be updated
    // by KMD.
    for (uint32 idx = 0; idx < numNextCmdBuffers; ++idx)
    {
        auto pCmdBuf = static_cast<GfxCmdBuffer*>(ppNextCmdBuffers[idx]);
        if (pCmdBuf->PerfTracesEnabled().spmTraceEnabled)
        {
            result = static_cast<Device*>(m_pDevice)->ReserveVmid();
            break;
        }
    }

    while ((result == Result::Success) && (numNextCmdBuffers > 0))
    {
        uint32           batchSize          = 0;
        IQueueSemaphore* pWaitBeforeLaunch  = nullptr;
        IQueueSemaphore* pSignalAfterLaunch = nullptr;

        if (tryToUploadCmdBuffers)
        {
            // Predict how many command buffers we can upload in the next batch, falling back to chaining if:
            // - We can't upload any command buffers.
            // - We're not in the MinGpuCmdOverhead mode and the batch will only hold one command buffer.
            const uint32 predictedUploadBatchSize =
                m_pCmdUploadRing->PredictBatchSize(numNextCmdBuffers, ppNextCmdBuffers);

            if ((predictedUploadBatchSize > 0) && (minGpuCmdOverhead || (predictedUploadBatchSize > 1)))
            {
                result = PrepareUploadedCommandBuffers(internalSubmitInfo,
                                                       numNextCmdBuffers,
                                                       ppNextCmdBuffers,
                                                       &batchSize,
                                                       &pWaitBeforeLaunch,
                                                       &pSignalAfterLaunch);
            }
            else
            {
                result = PrepareChainedCommandBuffers(internalSubmitInfo,
                                                      numNextCmdBuffers,
                                                      ppNextCmdBuffers,
                                                      &batchSize);
            }
        }
        else
        {
            result = PrepareChainedCommandBuffers(internalSubmitInfo,
                                                  numNextCmdBuffers,
                                                  ppNextCmdBuffers,
                                                  &batchSize);
        }

        if (result == Result::Success)
        {
            // The batch is fully prepared, advance our tracking variables and launch the command streams.
            PAL_ASSERT(numNextCmdBuffers >= batchSize);

            numNextCmdBuffers -= batchSize;
            ppNextCmdBuffers  += batchSize;

            // Note that we must bypass our batching logic when using these semaphores because we're already in the
            // post-batching code. The command uploader provides these semaphores and must guarantee this is safe.
            if (pWaitBeforeLaunch != nullptr)
            {
                result = WaitQueueSemaphoreInternal(pWaitBeforeLaunch, 0, true);
            }

            result = SubmitIbs(internalSubmitInfo);

            if ((pSignalAfterLaunch != nullptr) && (result == Result::Success))
            {
                result = SignalQueueSemaphoreInternal(pSignalAfterLaunch, 0, true);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// The GFX IP engines all support IB chaining, so we can submit multiple command buffers together as one. This function
// will add command streams for the preambles, chained command streams, and the postambles.
Result Queue::PrepareChainedCommandBuffers(
    const InternalSubmitInfo& internalSubmitInfo,
    uint32                    cmdBufferCount,
    ICmdBuffer*const*         ppCmdBuffers,
    uint32*                   pAppendedCmdBuffers)
{
    Result result = Result::Success;

    const uint32 maxBatchSize = Min(cmdBufferCount, m_device.GetPublicSettings()->cmdBufBatchedSubmitChainLimit);

    // Determine the number of command buffers we can chain together into a single set of command streams. We can only
    // do this if exclusive submit is set. This way, we don't need to worry about the GPU reading this command buffer
    // while we patch it using the CPU.
    uint32 batchSize = 1;
    while ((batchSize < maxBatchSize) && static_cast<CmdBuffer*>(ppCmdBuffers[batchSize - 1])->IsExclusiveSubmit())
    {
        batchSize++;
    }

    // The preamble command streams must be added to beginning of each kernel submission and cannot be chained because
    // they are shared by all submissions on this queue context. They must also separate streams because when MCBP is
    // enabled the preamble streams need to be marked as non-preemptible whereas the workload streams would be marked
    // as preemptible.
    for (uint32 idx = 0; (result == Result::Success) && (idx < internalSubmitInfo.numPreambleCmdStreams); ++idx)
    {
        PAL_ASSERT(internalSubmitInfo.pPreambleCmdStream[idx] != nullptr);
        result = AddCmdStream(*internalSubmitInfo.pPreambleCmdStream[idx],
                              internalSubmitInfo.flags.isDummySubmission,
                              internalSubmitInfo.flags.isTmzEnabled);
    }

    // The command buffer streams are grouped by stream index.
    const uint32 numStreams = static_cast<CmdBuffer*>(ppCmdBuffers[0])->NumCmdStreams();

    for (uint32 streamIdx = 0; (result == Result::Success) && (streamIdx < numStreams); ++streamIdx)
    {
        const CmdBuffer* pPrevCmdBuf    = nullptr;
        const CmdStream* pPrevCmdStream = nullptr;

        for (uint32 cmdBufIdx = 0; (result == Result::Success) && (cmdBufIdx < batchSize); ++cmdBufIdx)
        {
            const CmdBuffer*const pCurCmdBuf = static_cast<CmdBuffer*>(ppCmdBuffers[cmdBufIdx]);
            PAL_ASSERT(pCurCmdBuf != nullptr);

            // We assume that all command buffers for this queue type have the same number of streams.
            PAL_ASSERT(numStreams == pCurCmdBuf->NumCmdStreams());

            const CmdStream*const pCurCmdStream = pCurCmdBuf->GetCmdStream(streamIdx);
            PAL_ASSERT(pCurCmdStream != nullptr);

            if (pCurCmdStream->IsEmpty() == false)
            {
                if (pPrevCmdStream == nullptr)
                {
                    // The first command buffer's command streams are what the kernel will launch.
                    result = AddCmdStream(*pCurCmdStream,
                                          internalSubmitInfo.flags.isDummySubmission,
                                          internalSubmitInfo.flags.isTmzEnabled);
                }
                else
                {
                    // Chain the tail of the previous command buffer to the first chunk of this command buffer.
                    // We selected batchSize such that this will always be legal.
                    PAL_ASSERT(pPrevCmdBuf->IsExclusiveSubmit());
                    pPrevCmdStream->PatchTailChain(pCurCmdStream);
                }

                pPrevCmdBuf    = pCurCmdBuf;
                pPrevCmdStream = pCurCmdStream;
            }
        }

        // Clobber any previous tail chaining commands from the end of the final command stream in this batch to
        // overwrite anything which might be there from the last time this command buffer was submitted. This must
        // only be done if the command buffer has exclusive submit enabled.
        if ((pPrevCmdBuf != nullptr)           &&
            (pPrevCmdBuf->IsExclusiveSubmit()) &&
            (pPrevCmdStream != nullptr)        &&
            (pPrevCmdStream->IsEmpty() == false))
        {
            // Add a null tail-chain (which equates to a no-op).
            pPrevCmdStream->PatchTailChain(nullptr);
        }
    }

    // The postamble command streams must be added to the end of each kernel submission and are not chained.
    // In some situations it may be technically possible to chain the last command buffer stream to a postamble but
    // detecting those cases and properly managing the chaining logic is difficult. MCBP further complicates things
    // because chained postamble streams would not be executed at the end of a preempted frame but non-chained
    // postambles will always be executed.
    for (uint32 idx = 0; (result == Result::Success) && (idx < internalSubmitInfo.numPostambleCmdStreams); ++idx)
    {
        PAL_ASSERT(internalSubmitInfo.pPostambleCmdStream[idx] != nullptr);
        result = AddCmdStream(*internalSubmitInfo.pPostambleCmdStream[idx],
                              internalSubmitInfo.flags.isDummySubmission,
                              internalSubmitInfo.flags.isTmzEnabled);
    }

    if (result == Result::Success)
    {
        *pAppendedCmdBuffers = batchSize;
    }

    return result;
}

// =====================================================================================================================
// The GFX IP engines all support IB chaining, so we can submit multiple command buffers together as one. This function
// will add command streams for the preambles, chained command streams, and the postambles.
Result Queue::PrepareUploadedCommandBuffers(
    const InternalSubmitInfo& internalSubmitInfo,
    uint32                    cmdBufferCount,
    ICmdBuffer*const*         ppCmdBuffers,
    uint32*                   pAppendedCmdBuffers,
    IQueueSemaphore**         ppWaitBeforeLaunch,
    IQueueSemaphore**         ppSignalAfterLaunch)
{
    UploadedCmdBufferInfo uploadInfo = {};
    Result result = m_pCmdUploadRing->UploadCmdBuffers(cmdBufferCount, ppCmdBuffers, &uploadInfo);

    // The preamble command streams must be added to beginning of each kernel submission and cannot be uploaded because
    // they must not be preempted.
    for (uint32 idx = 0; (result == Result::Success) && (idx < internalSubmitInfo.numPreambleCmdStreams); ++idx)
    {
        PAL_ASSERT(internalSubmitInfo.pPreambleCmdStream[idx] != nullptr);
        result = AddCmdStream(*internalSubmitInfo.pPreambleCmdStream[idx],
                              internalSubmitInfo.flags.isDummySubmission,
                              internalSubmitInfo.flags.isTmzEnabled);
    }

    // Append all non-empty uploaded command streams.
    for (uint32 idx = 0; (result == Result::Success) && (idx < uploadInfo.uploadedCmdStreams); ++idx)
    {
        const UploadedStreamInfo& streamInfo = uploadInfo.streamInfo[idx];

        if (streamInfo.pGpuMemory != nullptr)
        {
            PAL_ASSERT(HighPart(streamInfo.launchSize / sizeof(uint32)) == 0);

            result = AddIb(streamInfo.pGpuMemory->Desc().gpuVirtAddr,
                           static_cast<uint32>(streamInfo.launchSize / sizeof(uint32)),
                           (streamInfo.subEngineType == SubEngineType::ConstantEngine),
                           streamInfo.flags.isPreemptionEnabled,
                           streamInfo.flags.dropIfSameContext,
                           internalSubmitInfo.flags.isTmzEnabled);
        }
    }

    // The postamble command streams must be added to the end of each kernel submission and are not chained.
    // In some situations it may be technically possible to chain the last command buffer stream to a postamble but
    // detecting those cases and properly managing the chaining logic is difficult. MCBP further complicates things
    // because chained postamble streams would not be executed at the end of a preempted frame but non-chained
    // postambles will always be executed.
    for (uint32 idx = 0; (result == Result::Success) && (idx < internalSubmitInfo.numPostambleCmdStreams); ++idx)
    {
        PAL_ASSERT(internalSubmitInfo.pPostambleCmdStream[idx] != nullptr);
        result = AddCmdStream(*internalSubmitInfo.pPostambleCmdStream[idx],
                              internalSubmitInfo.flags.isDummySubmission,
                              internalSubmitInfo.flags.isTmzEnabled);
    }

    if (result == Result::Success)
    {
        *pAppendedCmdBuffers = uploadInfo.uploadedCmdBuffers;
        *ppWaitBeforeLaunch  = uploadInfo.pUploadComplete;
        *ppSignalAfterLaunch = uploadInfo.pExecutionComplete;
    }

    return result;
}

// =====================================================================================================================
// Submits one or more Non GFX IP command buffers. Non GFX IP engines don't support chaining, so each chunk of every
// command buffer is submitted as a separate command buffer. It is not expected for the context command streams to be
// present for Non GFX IP Queues.
Result Queue::SubmitNonGfxIp(
    const MultiSubmitInfo&    submitInfo,
    const InternalSubmitInfo& internalSubmitInfo)
{
    PAL_ASSERT((internalSubmitInfo.numPreambleCmdStreams == 0) && (internalSubmitInfo.numPostambleCmdStreams == 0));

    // The OsSubmit function should guarantee that we have at least one DMA, VCE, or UVD command buffer.
    PAL_ASSERT(submitInfo.pPerSubQueueInfo[0].cmdBufferCount > 0);

    uint32 maxChunkCount = 0;
    switch(Type())
    {
    case QueueTypeDma:
        maxChunkCount = MaxIbsPerSubmit;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    Result result = Result::Success;

    for (uint32 idx = 0; (idx < submitInfo.pPerSubQueueInfo[0].cmdBufferCount) && (result == Result::Success); ++idx)
    {
        const CmdBuffer*const pCmdBuffer = static_cast<CmdBuffer*>(submitInfo.pPerSubQueueInfo[0].ppCmdBuffers[idx]);

        // Non GFX IP command buffers are expected to only have a single command stream.
        PAL_ASSERT(pCmdBuffer->NumCmdStreams() == 1);

        const CmdStream*const pCmdStream = internalSubmitInfo.flags.isDummySubmission ?
                                           m_pDummyCmdStream : pCmdBuffer->GetCmdStream(0);
        uint32                chunkCount = 0; // Keep track of how many chunks will be submitted next.

        for (auto iter = pCmdStream->GetFwdIterator(); iter.IsValid() && (result == Result::Success); iter.Next())
        {
            const CmdStreamChunk*const pChunk = iter.Get();

            result = AddIb(pChunk->GpuVirtAddr(),
                           pChunk->CmdDwordsToExecute(),
                           (pCmdStream->GetSubEngineType() == SubEngineType::ConstantEngine),
                           pCmdStream->IsPreemptionEnabled(),
                           pCmdStream->DropIfSameContext(),
                           internalSubmitInfo.flags.isTmzEnabled);
            // There is a limitation on amdgpu that the ib counts can't exceed MaxIbsPerSubmit.
            // Need to submit several times when there are more than MaxIbsPerSubmit chunks in a
            // command stream.
            if ((++chunkCount == maxChunkCount) && (result == Result::Success))
            {
                // Submit the command buffer and reset the chunk count.
                result     = SubmitIbs(internalSubmitInfo);
                chunkCount = 0;
            }
        }

        // Submit the rest of the chunks.
        if ((chunkCount > 0) && (result == Result::Success))
        {
            result = SubmitIbs(internalSubmitInfo);
        }
    }

    return result;
}

// =====================================================================================================================
// Wait all the commands was submit by this queue to be finished.
Result Queue::OsWaitIdle()
{
    Result      result  = Result::Success;
    const auto& context = static_cast<SubmissionContext&>(*m_pSubmissionContext);

    // Make sure something has been submitted before attempting to wait for idle!
    if ((m_pSubmissionContext != nullptr) && (context.LastTimestamp() > 0))
    {
        struct amdgpu_cs_fence queryFence = {};

        queryFence.context     = context.Handle();
        queryFence.fence       = context.LastTimestamp();
        queryFence.ring        = context.EngineId();
        queryFence.ip_instance = 0;
        queryFence.ip_type     = context.IpType();

        result = static_cast<Device*>(m_pDevice)->QueryFenceStatus(&queryFence, AMDGPU_TIMEOUT_INFINITE);
    }

    return result;
}

// =====================================================================================================================
Result Queue::DoAssociateFenceWithLastSubmit(
    Pal::Fence* pFence)
{
    Result result = Result::Success;
    if (m_device.GetFenceType() == FenceType::SyncObj)
    {
        result = m_device.ConveySyncObjectState(
            static_cast<SyncobjFence*>(pFence)->SyncObjHandle(),
            0,
            static_cast<SubmissionContext*>(m_pSubmissionContext)->GetLastSignaledSyncObj(),
            0);
    }
    else
    {
        PAL_ASSERT(m_device.GetFenceType() == FenceType::Legacy);
        result = static_cast<Amdgpu::TimestampFence*>(pFence)->AssociateWithLastTimestamp();
    }
    return result;
}

// =====================================================================================================================
// Updates the resource list with all GPU memory allocations which will participate in a submission to amdgpu.
Result Queue::UpdateResourceList(
    const GpuMemoryRef* pMemRefList,
    size_t              memRefCount)
{
    InternalMemMgr*const pMemMgr = m_pDevice->MemMgr();

    Result result = Result::Success;

    // if the allocation is always resident, Pal doesn't need to build up the allocation list.
    if (m_pDevice->Settings().alwaysResident == false)
    {
        // Serialize access to internalMgr and queue memory list
        RWLockAuto<RWLock::ReadOnly> lockMgr(pMemMgr->GetRefListLock());
        RWLockAuto<RWLock::ReadOnly> lock(&m_globalRefLock);

        const bool reuseResourceList = (m_globalRefDirty == false)                               &&
                                       (memRefCount == 0)                                        &&
                                       (m_appMemRefCount == 0)                                   &&
                                       (m_hResourceList != nullptr)                              &&
                                       (m_pDevice->Settings().allocationListReusable);

        if (reuseResourceList == false)
        {
            // Reset the list
            m_numResourcesInList = 0;
            if (m_hResourceList != nullptr)
            {
                result = static_cast<Device*>(m_pDevice)->DestroyResourceList(m_hResourceList);
                m_hResourceList = nullptr;
            }

            // First add all of the global memory references.
            if (result == Result::Success)
            {
                // If the global memory references haven't been modified since the last submit,
                // the resources in our UMD-side list (m_pResourceList) should be up to date.
                // So, there is no need to re-walk through m_memList.
                if (m_globalRefDirty == false)
                {
                    m_numResourcesInList += m_memListResourcesInList;
                }
                else
                {
                    m_globalRefDirty = false;

                    for (auto iter = m_globalRefMap.Begin(); iter.Get() != nullptr; iter.Next())
                    {
                        const auto*const pGpuMemory = static_cast<const GpuMemory*>(iter.Get()->key);

                        result = AppendResourceToList(pGpuMemory);

                        if (result != Result::_Success)
                        {
                            // We didn't rebuild the whole list so keep it marked as dirty.
                            m_globalRefDirty = true;
                            break;
                        }
                    }

                    m_memListResourcesInList = m_numResourcesInList;
                }
            }

            // Finally, add all of the application's submission memory references.
            if (result == Result::Success)
            {
                m_appMemRefCount = memRefCount;
                for (size_t idx = 0; ((idx < memRefCount) && (result == Result::_Success)); ++idx)
                {
                    result = AppendResourceToList(static_cast<GpuMemory*>(pMemRefList[idx].pGpuMemory));
                }
            }

            if ((result == Result::Success) && (m_numResourcesInList > 0))
            {
                result = static_cast<Device*>(m_pDevice)->CreateResourceList(m_numResourcesInList,
                                                                             m_pResourceList,
                                                                             m_pResourcePriorityList,
                                                                             &m_hResourceList);
            }
        }
    }
    return result;
}

// =====================================================================================================================
// Appends a bo to the list of buffer objects which get submitted with a set of command buffers.
Result Queue::AppendResourceToList(
    const GpuMemory* pGpuMemory)
{
    PAL_ASSERT(pGpuMemory != nullptr);

    Result result = Result::ErrorTooManyMemoryReferences;

    if ((m_numResourcesInList + 1) <= m_resourceListSize)
    {
        Image* pImage = static_cast<Image*>(pGpuMemory->GetImage());

        // If VM is always valid, not necessary to add into the resource list.
        // Presentable image which is already owned by Window System can't be added into reference list.
        if ((pGpuMemory->IsVmAlwaysValid() == false) &&
            ((pImage == nullptr) || (pImage->IsPresentable() == false) || pImage->GetIdle()))
        {
            m_pResourceList[m_numResourcesInList] = pGpuMemory->SurfaceHandle();

            if (m_pResourcePriorityList != nullptr)
            {
                // Max priority that Os accepts is 32, see AMDGPU_BO_LIST_MAX_PRIORITY.
                // We reserve 3 bits for priority while 2 bits for offset
                const uint8 offsetBits = static_cast<uint8>(pGpuMemory->PriorityOffset()) / 2;

                static_assert(
                    (static_cast<uint32>(Pal::GpuMemPriority::Count) == 6) &&
                     static_cast<uint32>(Pal::GpuMemPriorityOffset::Count) == 8,
                    "Pal GpuMemPriority or GpuMemPriorityOffset values changed. Consider to update strategy to convert"
                    "Pal GpuMemPriority and GpuMemPriorityOffset to lnx resource priority");
                m_pResourcePriorityList[m_numResourcesInList] =
                    (LnxResourcePriorityTable[static_cast<size_t>(pGpuMemory->Priority())] << 2) | offsetBits;
            }

            ++m_numResourcesInList;
        }

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Calls AddIb on the first chunk from the given command stream.
Result Queue::AddCmdStream(
    const CmdStream& cmdStream,
    bool             isDummySubmission,
    bool             isTmzEnabled)
{
    Result result = Result::Success;

    if ((isDummySubmission == false) || (cmdStream.GetCmdStreamUsage() == CmdStreamUsage::Workload))
    {
        const CmdStreamChunk*const pChunk = isDummySubmission ?
                                            m_pDummyCmdStream->GetFirstChunk() :
                                            cmdStream.GetFirstChunk();

        result =  AddIb(pChunk->GpuVirtAddr(),
                        pChunk->CmdDwordsToExecute(),
                        (cmdStream.GetSubEngineType() == SubEngineType::ConstantEngine),
                        cmdStream.IsPreemptionEnabled(),
                        cmdStream.DropIfSameContext(),
                        isTmzEnabled);
    }

    return result;
}

// =====================================================================================================================
// Adds an IB to the internal list. It will be submitted to the GPU during the next call to SubmitIbs.
Result Queue::AddIb(
    gpusize gpuVirtAddr,
    uint32  sizeInDwords,
    bool    isConstantEngine,
    bool    isPreemptionEnabled,
    bool    dropIfSameContext,
    bool    isTmzEnabled)
{
    Result result = Result::ErrorUnknown;

    if (m_numIbs < MaxIbsPerSubmit)
    {
        result = Result::Success;

        m_ibs[m_numIbs].ib_mc_address = gpuVirtAddr;
        m_ibs[m_numIbs].size          = sizeInDwords;

        // In Linux KMD, AMDGPU_IB_FLAG_PREAMBLE simply behaves just like flag "dropIfSameCtx" in windows.
        // But we are forbidden to change the flag name because the interface was already upstreamed to
        // open source libDRM, so we have to still use it for backward compatibility.
        m_ibs[m_numIbs].flags         = ((isConstantEngine    ? AMDGPU_IB_FLAG_CE            : 0) |
                                         (isPreemptionEnabled ? AMDGPU_IB_FLAG_PREEMPT       : 0) |
                                         (dropIfSameContext   ? AMDGPU_IB_FLAG_PREAMBLE      : 0) |
                                         (m_numIbs == 0       ? AMDGPU_IB_FLAG_EMIT_MEM_SYNC : 0) |
                                         (isTmzEnabled        ? AMDGPU_IB_FLAGS_SECURE       : 0));

        m_numIbs++;
    }

    return result;
}

// =====================================================================================================================
// Submits the accumulated list of IBs to the GPU. Resets the IB list to begin building the next submission.
Result Queue::SubmitIbsRaw(
    const InternalSubmitInfo& internalSubmitInfo)
{
    auto*const pDevice  = static_cast<Device*>(m_pDevice);
    auto*const pContext = static_cast<SubmissionContext*>(m_pSubmissionContext);
    Result result = Result::Success;

    uint32  totalChunk = m_numIbs;
    uint32  currentChunk = 0;
    uint32  waitCount = m_waitSemList.NumElements() + internalSubmitInfo.waitSemaphoreCount;
    // Each queue manages one sync object which refers to the fence of last submission.
    uint32  signalCount = internalSubmitInfo.signalSemaphoreCount + 1;
    // all semaphores supposed to be waited before submission need one chunk
    totalChunk += waitCount > 0 ? 1 : 0;
    // all semaphores supposed to be signaled after submission need one chunk
    totalChunk += 1;

    AutoBuffer<struct drm_amdgpu_cs_chunk, 8, Pal::Platform>
                        chunkArray(totalChunk, m_pDevice->GetPlatform());
    AutoBuffer<struct drm_amdgpu_cs_chunk_data, 8, Pal::Platform>
                        chunkDataArray(m_numIbs, m_pDevice->GetPlatform());

    const size_t syncobjChunkSize = (signalCount + waitCount) * Max(sizeof(drm_amdgpu_cs_chunk_sem),
            sizeof(drm_amdgpu_cs_chunk_syncobj));
    void*const pMemory = PAL_MALLOC(syncobjChunkSize, m_pDevice->GetPlatform(), AllocInternal);

    // default size is the minumum capacity of AutoBuffer.
    if ((chunkArray.Capacity()       <  totalChunk)  ||
        (chunkDataArray.Capacity()   <  m_numIbs)    ||
        (pMemory                     == nullptr))
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        memset(pMemory, 0, syncobjChunkSize);
        // kernel requires IB chunk goes ahead of others.
        for (uint32 i = 0; i < m_numIbs; i++)
        {
            chunkArray[i].chunk_id = AMDGPU_CHUNK_ID_IB;
            chunkArray[i].length_dw = sizeof(struct drm_amdgpu_cs_chunk_ib) / 4;
            chunkArray[i].chunk_data  = static_cast<uint64>(reinterpret_cast<uintptr_t>(&chunkDataArray[i]));
            chunkDataArray[i].ib_data._pad = 0;
            chunkDataArray[i].ib_data.va_start = m_ibs[i].ib_mc_address;
            chunkDataArray[i].ib_data.ib_bytes = m_ibs[i].size * 4;
            chunkDataArray[i].ib_data.ip_type = pContext->IpType();
            // Quote From Kernel : Right now all IPs have only one instance - multiple rings.
            // The ip_instance should always stay at 0 for now.
            chunkDataArray[i].ib_data.ip_instance = 0;
            chunkDataArray[i].ib_data.ring = pContext->EngineId();
            // so far the flag is always 0
            chunkDataArray[i].ib_data.flags = m_ibs[i].flags;
            currentChunk ++;
        }

        if (pDevice->IsTimelineSyncobjSemaphoreSupported())
        {
            struct drm_amdgpu_cs_chunk_syncobj* pWaitChunkArray     =
                static_cast<struct drm_amdgpu_cs_chunk_syncobj*>(pMemory);
            struct drm_amdgpu_cs_chunk_syncobj* pSignalChunkArray   =
                static_cast<struct drm_amdgpu_cs_chunk_syncobj*>(
                    VoidPtrInc(pMemory, sizeof(drm_amdgpu_cs_chunk_syncobj) * waitCount));

            // add the semaphore supposed to be waited before the submission.
            if (waitCount > 0)
            {
                chunkArray[currentChunk].chunk_id   = AMDGPU_CHUNK_ID_SYNCOBJ_TIMELINE_WAIT;
                chunkArray[currentChunk].length_dw  = waitCount * sizeof(struct drm_amdgpu_cs_chunk_syncobj) / 4;
                chunkArray[currentChunk].chunk_data =
                    static_cast<uint64>(reinterpret_cast<uintptr_t>(&pWaitChunkArray[0]));

                uint32 waitListSize = m_waitSemList.NumElements();
                uint32 index        = 0;
                for (uint32 i = 0; i < waitListSize; i++, index++)
                {
                    struct SemaphoreInfo semaInfo  = { };
                    m_waitSemList.PopBack(&semaInfo);
                    pWaitChunkArray[index].handle    = reinterpret_cast<uintptr_t>(semaInfo.hSemaphore);
                    pWaitChunkArray[index].flags     = DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT;
                    pWaitChunkArray[index].point     = semaInfo.value;
                }
                for (uint32 i = 0; i < internalSubmitInfo.waitSemaphoreCount; i++, index++)
                {
                    amdgpu_semaphore_handle handle  =
                        static_cast<QueueSemaphore*>(internalSubmitInfo.ppWaitSemaphores[i])->GetSyncObjHandle();

                    const bool timeline =
                        static_cast<QueueSemaphore*>(internalSubmitInfo.ppWaitSemaphores[i])->IsTimeline();

                    PAL_ASSERT((timeline == false) || (internalSubmitInfo.pWaitPoints[i] != 0));

                    pWaitChunkArray[index].handle    = reinterpret_cast<uintptr_t>(handle);
                    pWaitChunkArray[index].flags     = DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT;
                    pWaitChunkArray[index].point     = timeline ? internalSubmitInfo.pWaitPoints[i] : 0;
                }
                currentChunk ++;
            }

            // add the semaphore supposed to be signaled after the submission.
            if (signalCount > 0)
            {
                chunkArray[currentChunk].chunk_id   = AMDGPU_CHUNK_ID_SYNCOBJ_TIMELINE_SIGNAL;
                chunkArray[currentChunk].length_dw  = signalCount * sizeof(struct drm_amdgpu_cs_chunk_syncobj) / 4;
                chunkArray[currentChunk].chunk_data =
                    static_cast<uint64>(reinterpret_cast<uintptr_t>(&pSignalChunkArray[0]));

                for (uint32 i = 0; i < internalSubmitInfo.signalSemaphoreCount; i++)
                {
                    amdgpu_semaphore_handle handle   =
                        static_cast<QueueSemaphore*>(internalSubmitInfo.ppSignalSemaphores[i])->GetSyncObjHandle();

                    const bool timeline =
                        static_cast<QueueSemaphore*>(internalSubmitInfo.ppSignalSemaphores[i])->IsTimeline();

                    PAL_ASSERT((timeline == false) || (internalSubmitInfo.pSignalPoints[i] != 0));

                    pSignalChunkArray[i].handle      = reinterpret_cast<uintptr_t>(handle);
                    pSignalChunkArray[i].point       = timeline ? internalSubmitInfo.pSignalPoints[i] : 0;
                }
                pSignalChunkArray[internalSubmitInfo.signalSemaphoreCount].handle = m_lastSignaledSyncObject;
                pSignalChunkArray[internalSubmitInfo.signalSemaphoreCount].point = 0;
            }
        }
        else
        {
            struct drm_amdgpu_cs_chunk_sem* pWaitChunkArray     =
                static_cast<struct drm_amdgpu_cs_chunk_sem*>(pMemory);
            struct drm_amdgpu_cs_chunk_sem* pSignalChunkArray   =
                static_cast<struct drm_amdgpu_cs_chunk_sem*>(
                    VoidPtrInc(pMemory, sizeof(drm_amdgpu_cs_chunk_sem) * waitCount));

            // add the semaphore supposed to be waited before the submission.
            if (waitCount > 0)
            {
                chunkArray[currentChunk].chunk_id   = AMDGPU_CHUNK_ID_SYNCOBJ_IN;
                chunkArray[currentChunk].length_dw  = waitCount * sizeof(struct drm_amdgpu_cs_chunk_sem) / 4;
                chunkArray[currentChunk].chunk_data =
                    static_cast<uint64>(reinterpret_cast<uintptr_t>(&pWaitChunkArray[0]));

                uint32 waitListSize = m_waitSemList.NumElements();
                uint32 index        = 0;

                for (uint32 i = 0; i < waitListSize; i++, index++)
                {
                    struct SemaphoreInfo semaInfo   = { };
                    m_waitSemList.PopBack(&semaInfo);
                    pWaitChunkArray[index].handle   = reinterpret_cast<uintptr_t>(semaInfo.hSemaphore);
                }
                for (uint32 i = 0; i < internalSubmitInfo.waitSemaphoreCount; i++, index++)
                {
                    amdgpu_semaphore_handle handle  =
                        static_cast<QueueSemaphore*>(internalSubmitInfo.ppWaitSemaphores[i])->GetSyncObjHandle();
                    pWaitChunkArray[index].handle   = reinterpret_cast<uintptr_t>(handle);
                }
                currentChunk ++;
            }

            // add the semaphore supposed to be signaled after the submission.
            if (signalCount > 0)
            {
                chunkArray[currentChunk].chunk_id   = AMDGPU_CHUNK_ID_SYNCOBJ_OUT;
                chunkArray[currentChunk].length_dw  = signalCount * sizeof(struct drm_amdgpu_cs_chunk_sem) / 4;
                chunkArray[currentChunk].chunk_data =
                    static_cast<uint64>(reinterpret_cast<uintptr_t>(&pSignalChunkArray[0]));

                for (uint32 i = 0; i < internalSubmitInfo.signalSemaphoreCount; i++)
                {
                    amdgpu_semaphore_handle handle  =
                        static_cast<QueueSemaphore*>(internalSubmitInfo.ppSignalSemaphores[i])->GetSyncObjHandle();
                    pSignalChunkArray[i].handle     = reinterpret_cast<uintptr_t>(handle);
                }
                pSignalChunkArray[internalSubmitInfo.signalSemaphoreCount].handle = m_lastSignaledSyncObject;
            }
        }
        result = pDevice->SubmitRaw(pContext->Handle(),
                internalSubmitInfo.flags.isDummySubmission ? m_hDummyResourceList : m_hResourceList,
                totalChunk,
                &chunkArray[0],
                pContext->LastTimestampPtr());

        pContext->SetLastSignaledSyncObj(m_lastSignaledSyncObject);

        PAL_FREE(pMemory, m_pDevice->GetPlatform());
        // all pending waited semaphore has been poped already.
        PAL_ASSERT(m_waitSemList.IsEmpty());
    }

    return result;
}

// =====================================================================================================================
// Submits the accumulated list of IBs to the GPU. Resets the IB list to begin building the next submission.
Result Queue::SubmitIbs(
    const InternalSubmitInfo& internalSubmitInfo)
{
    auto*const pDevice  = static_cast<Device*>(m_pDevice);
    auto*const pContext = static_cast<SubmissionContext*>(m_pSubmissionContext);
    Result result = Result::Success;

    // we should only use new submit routine when sync object is supported in the kenrel as well as u/k interfaces.
    if (pDevice->GetSemaphoreType() == SemaphoreType::SyncObj)
    {
        result = SubmitIbsRaw(internalSubmitInfo);
    }
    else
    {
        struct amdgpu_cs_request ibsRequest = {};
        ibsRequest.flags         = internalSubmitInfo.flags.isTmzEnabled;
        ibsRequest.ip_type       = pContext->IpType();
        ibsRequest.ring          = pContext->EngineId();
        ibsRequest.resources     = internalSubmitInfo.flags.isDummySubmission ? m_hDummyResourceList : m_hResourceList;
        ibsRequest.number_of_ibs = m_numIbs;
        ibsRequest.ibs           = m_ibs;

        result = pDevice->Submit(pContext->Handle(), 0, &ibsRequest, 1, pContext->LastTimestampPtr());
    }

    m_numIbs = 0;
    memset(m_ibs, 0, sizeof(m_ibs));

    return result;
}

// =====================================================================================================================
void Queue::AssociateFenceWithContext(
    IFence*  pFence)
{
    PAL_ASSERT(pFence != nullptr);
    static_cast<Fence*>(pFence)->AssociateWithContext(m_pSubmissionContext);
}

// =====================================================================================================================
// Set globalRefDirty true so that the resource list of the queue could be rebuilt.
void Queue::DirtyGlobalReferences()
{
    RWLockAuto<RWLock::ReadWrite> lock(&m_globalRefLock);

    m_globalRefDirty = true;
}

} // Amdgpu
} // Pal
