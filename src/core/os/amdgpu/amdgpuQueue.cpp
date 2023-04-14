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

#include "core/cmdAllocator.h"
#include "core/engine.h"
#include "core/hw/gfxip/cmdUploadRing.h"
#include "core/hw/gfxip/pm4UniversalCmdBuffer.h"
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

#if PAL_BUILD_RDF
#include "gpuUtil/frameTraceController.h"
#endif

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
    Device*                  pDevice,
    EngineType               engineType,
    uint32                   engineId,
    Pal::QueuePriority       priority,
    bool                     isTmzOnly,
    Pal::SubmissionContext** ppContext)
{
    Result     result   = Result::ErrorOutOfMemory;
    auto*const pContext = PAL_NEW(SubmissionContext, pDevice->GetPlatform(), AllocInternal)(*pDevice,
                                                                                            engineType,
                                                                                            engineId,
                                                                                            priority,
                                                                                            isTmzOnly);

    if (pContext != nullptr)
    {
        result = pContext->Init(pDevice);

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
    Pal::QueuePriority  priority,
    bool                isTmzOnly)
    :
    Pal::SubmissionContext(device.GetPlatform()),
    m_device(device),
    m_ipType(GetIpType(engineType)),
    m_engineId(engineId),
    m_queuePriority(priority),
    m_isTmzOnly(isTmzOnly),
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
Result SubmissionContext::Init(
    Device* pDevice)
{
    return pDevice->CreateCommandSubmissionContext(&m_hContext, m_queuePriority, m_isTmzOnly);
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
    m_resourceListSize(Pal::Device::CmdBufMemReferenceLimit),
    m_numResourcesInList(0),
    m_numDummyResourcesInList(0),
    m_memListResourcesInList(0),
    m_memMgrResourcesInList(0),
    m_hResourceList(nullptr),
    m_hDummyResourceList(nullptr),
    m_dummyResourceList(0),
    m_dummyResourceEntryList(pDevice->GetPlatform()),
    m_pDummyCmdStream(nullptr),
    m_globalRefMap(static_cast<Device*>(m_pDevice)->IsVmAlwaysValidSupported() ? MemoryRefMapElementsPerVmBo :
                   MemoryRefMapElements, m_pDevice->GetPlatform()),
    m_globalRefDirty(true),
    m_appMemRefCount(0),
    m_pendingWait(false),
    m_pCmdUploadRing(nullptr),
    m_sqttWaRequired(false),
    m_perfCtrWaRequired(false),
    m_numIbs(0),
    m_lastSignaledSyncObject(0),
    m_waitSemList(pDevice->GetPlatform()),
    m_requiresGangedInterface(false)
{
    // The space allocated will be used to save either the handle of each command or the pointer of the command
    // itself. When raw2 submit is supported, we save the pointer.
    if (pDevice->IsRaw2SubmitSupported())
    {
        m_pResourceList = nullptr;
        m_pResourceObjectList = reinterpret_cast<GpuMemory**>(this + 1);
        m_pResourcePriorityList = pCreateInfo[0].enableGpuMemoryPriorities ?
            reinterpret_cast<uint8*>(m_pResourceObjectList + Pal::Device::CmdBufMemReferenceLimit) : nullptr;
    }
    else
    {
        m_pResourceList = reinterpret_cast<amdgpu_bo_handle*>(this + 1);
        m_pResourceObjectList = nullptr;
        m_pResourcePriorityList = pCreateInfo[0].enableGpuMemoryPriorities ?
            reinterpret_cast<uint8*>(m_pResourceList + Pal::Device::CmdBufMemReferenceLimit) : nullptr;
    }
    memset(m_ibs, 0, sizeof(m_ibs));
}

// =====================================================================================================================
Queue::~Queue()
{
    if (m_pCmdUploadRing != nullptr)
    {
        m_pCmdUploadRing->DestroyInternal();
    }

    Device* pDevice = static_cast<Device*>(m_pDevice);

    if (m_hResourceList != nullptr)
    {
        pDevice->DestroyResourceList(m_hResourceList);
    }

    if (m_hDummyResourceList != nullptr)
    {
        pDevice->DestroyResourceList(m_hDummyResourceList);
    }

    if (m_dummyResourceList != 0)
    {
        pDevice->DestroyResourceListRaw(m_dummyResourceList);
    }

    if (m_pDummyCmdStream != nullptr)
    {
        m_pDummyCmdStream = nullptr;
    }

    if (m_lastSignaledSyncObject > 0)
    {
        pDevice->DestroySyncObject(m_lastSignaledSyncObject);
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
        result = SubmissionContext::Create(static_cast<Device*>(m_pDevice),
                                           GetEngineType(),
                                           EngineId(),
                                           Priority(),
                                           pCreateInfo->tmzOnly,
                                           &m_pSubmissionContext);
    }

    if (result == Result::Success)
    {
        result = m_globalRefMap.Init();
    }

    // Note that the presence of the command upload ring will be used later to determine if these conditions are true.
    if ((result == Result::Success)                                              &&
        (m_device.EngineProperties().perEngine[EngineTypeDma].numAvailable != 0) &&
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
            createInfo.numCmdStreams = supportsGraphics ? Pm4::UniversalCmdBuffer::NumCmdStreamsVal : 1;

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
            for (auto iter = m_pDummyCmdStream->GetFwdIterator();
                    (iter.IsValid()) && (result == Result::Success); iter.Next())
            {
                m_numDummyResourcesInList++;
                GpuMemory* pGpuMemory = static_cast<GpuMemory*>(iter.Get()->GpuMemory());
                dummyResourceList.PushBack(pGpuMemory->SurfaceHandle());

                // For GpuMemory to be submitted in list, export and save its KMS handle.
                uint32 kmsHandle = pGpuMemory->SurfaceKmsHandle();
                if (kmsHandle == 0)
                {
                    result = pDevice->ExportBuffer(pGpuMemory->SurfaceHandle(),
                                                   amdgpu_bo_handle_type_kms,
                                                   &kmsHandle);
                    if (result == Result::Success)
                    {
                        pGpuMemory->SetSurfaceKmsHandle(kmsHandle);
                    }
                }
                result = m_dummyResourceEntryList.PushBack({kmsHandle, 0});
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
        if (result == Result::Success && pDevice->UseBoListCreate())
        {
            result = pDevice->CreateResourceListRaw(dummyResourceList.NumElements(),
                                                    m_dummyResourceEntryList.Data(),
                                                    &m_dummyResourceList);
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
Result Queue::OsRemapVirtualMemoryPages(
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

    Result result = Result::Success;

    bool sqttActive    = m_sqttWaRequired;
    bool sqttClosed    = (m_sqttWaRequired == false);
    bool perfCtrActive = m_perfCtrWaRequired;
    bool perfCtrClosed = (m_perfCtrWaRequired == false);
    for (uint32 queueCountIdx = 0; queueCountIdx < submitInfo.perSubQueueInfoCount; queueCountIdx++)
    {
        for (uint32 cmdBufferIdx = 0; (cmdBufferIdx < submitInfo.pPerSubQueueInfo[queueCountIdx].cmdBufferCount);
            ++cmdBufferIdx)
        {
            if ((Type() == QueueType::QueueTypeUniversal) || (Type() == QueueType::QueueTypeCompute))
            {
                const GfxCmdBuffer*const pGfxCmdBuffer =
                    static_cast<GfxCmdBuffer*>(submitInfo.pPerSubQueueInfo[queueCountIdx].ppCmdBuffers[cmdBufferIdx]);

                if (pGfxCmdBuffer->SqttStarted() || pGfxCmdBuffer->SqttClosed())
                {
                    sqttActive = true;
                    sqttClosed = pGfxCmdBuffer->SqttClosed();
                }
                if (pGfxCmdBuffer->PerfCounterStarted() || pGfxCmdBuffer->PerfCounterClosed())
                {
                    perfCtrActive = true;
                    perfCtrClosed = pGfxCmdBuffer->PerfCounterClosed();
                }
            }
        }
    }

    m_sqttWaRequired    = sqttActive;
    m_perfCtrWaRequired = perfCtrActive;

    {
        // Serialize access to m_globalRefMap in-case a call to GpuMemory::Destroy() happens between
        // UpdateResourceList() and SubmitNonGfxIp()
        RWLockAuto<RWLock::ReadOnly> lock(&m_globalRefLock);

        // if the allocation is always resident, Pal doesn't need to build up the allocation list.
        if ((m_pDevice->Settings().alwaysResident == false) && (pInternalSubmitInfos->flags.isDummySubmission == false))
        {
            result = UpdateResourceList(submitInfo.pGpuMemoryRefs, submitInfo.gpuMemRefCount);
        }

        if (result == Result::Success)
        {
            // localSubmitInfo is used for SubmitPm4() and SubmitNonGfxIp() calls
            MultiSubmitInfo       localSubmitInfo       = submitInfo;
            PerSubQueueSubmitInfo perSubQueueSubmitInfo = {};
            if (pInternalSubmitInfos->flags.isDummySubmission == false)
            {
                perSubQueueSubmitInfo            = submitInfo.pPerSubQueueInfo[0];
            }
            localSubmitInfo.pPerSubQueueInfo     = &perSubQueueSubmitInfo;
            localSubmitInfo.perSubQueueInfoCount = 1;

            // Clear pending wait flag.
            m_pendingWait = false;

            if ((Type() == QueueTypeUniversal) || (Type() == QueueTypeCompute))
            {
                if (m_queueCount > 1)
                {
                    result = SubmitMultiQueuePm4(submitInfo, pInternalSubmitInfos);
                }
                else if (pInternalSubmitInfos->implicitGangedSubQueues > 0)
                {
                    // We only support Gfx+ImplicitAce submissions as a single queue on the Universal Engine.
                    PAL_ASSERT((m_queueCount == 1) &&
                               (m_pQueueInfos[0].createInfo.engineType == EngineType::EngineTypeUniversal));
                    // There's a race condition with a Queue using both normal and gang submission.
                    // Because normal submissions use GPU to write to fence memory, and gang submissions uses the CPU
                    // to write to fence memory, there is a chance that while we are trying to write a gang submit
                    // fence, the GPU will write a fence for normal submission.
                    // Solution:
                    // If m_queuecount == 1, have PAL track if it has ever seen a submit which uses
                    // ImplicitAceCmdStream. Until it encounters one, it can use the normal path.The first time it
                    // encounters a usesImplicitAceCmdStream submit, it needs to idle the queue then use the gang
                    // submission interface from that point forward.
                    if (m_requiresGangedInterface == false)
                    {
                        result = WaitIdle();
                        PAL_ALERT(result == Result::Success);
                        m_requiresGangedInterface = true;
                    }
                    if (result == Result::Success)
                    {
                        PAL_NOT_IMPLEMENTED_MSG("Implicit Gang Submission not yet implemented!");
                    }
                }
                else
                {
                    if (m_requiresGangedInterface == false)
                    {
                        IncrementDummySubmitCount(pInternalSubmitInfos, perSubQueueSubmitInfo.ppCmdBuffers,
                                                  perSubQueueSubmitInfo.cmdBufferCount);

                        result = SubmitPm4(localSubmitInfo, pInternalSubmitInfos[0]);
                    }
                    else
                    {
                        // If we reach this branch, it indicates that pInternalSubmitInfos->implicitGangedSubQueues is 0
                        // while m_requiresGangedInterface is true. Specifically, it indicates that this submission does
                        // not use the ImplicitAce while there was an ImplicitAce + Gfx submission on this queue before.
                        // Based on the solution mentioned above, we need to use gang submit interface for this submission.
                        result = SubmitMultiQueuePm4(submitInfo, pInternalSubmitInfos);
                    }
                }
            }
            else if ((Type() == QueueTypeDma)
                    )
            {
                // amdgpu won't give us a new fence value unless the submission has at least one command buffer.
                IncrementDummySubmitCount(pInternalSubmitInfos, perSubQueueSubmitInfo.ppCmdBuffers,
                                          perSubQueueSubmitInfo.cmdBufferCount);

                result = SubmitNonGfxIp(localSubmitInfo, pInternalSubmitInfos[0]);
            }
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
    if ((result == Result::Success) && (submitInfo.fenceCount > 0))
    {
        for (uint32 i = 0; i < submitInfo.fenceCount; i++)
        {
            DoAssociateFenceWithLastSubmit(static_cast<Pal::Fence*>(submitInfo.ppFences[i]));
        }
    }

    m_sqttWaRequired     = (sqttClosed == false);
    m_perfCtrWaRequired  = (perfCtrClosed == false);

#if PAL_BUILD_RDF
    // In order to avoid RRA sync issues, we need to idle the queue when the end-trace command buffer is submitted,
    // and then finish the trace.
    if (result == Result::Success)
    {
        for (uint32 idxQueue = 0; idxQueue < submitInfo.perSubQueueInfoCount; ++idxQueue)
        {
            for (uint32 idxCmdBuf = 0; idxCmdBuf < submitInfo.pPerSubQueueInfo[idxQueue].cmdBufferCount; ++idxCmdBuf)
            {
                CmdBuffer* const pCmdBuffer =
                    static_cast<CmdBuffer*>(submitInfo.pPerSubQueueInfo[idxQueue].ppCmdBuffers[idxCmdBuf]);
                if (pCmdBuffer->IsUsedInEndTrace())
                {
                    this->WaitIdle();
                    GpuUtil::FrameTraceController* frameController = m_device.GetPlatform()->GetFrameTraceController();
                    frameController->FinishTrace();
                    pCmdBuffer->SetEndTraceFlag(0);
                }
            }
        }
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

    // SubmitPm4 function should not handle more than 1 subqueue
    PAL_ASSERT(submitInfo.perSubQueueInfoCount == 1);

    // For linux platforms, there will exist at most 3 preamble + 2 postamble:
    // Preamble  CE IB (always)
    // Preamble  DE IB (always)
    // Preamble  DE IB (if context switch)
    // Postamble CE IB
    // Postamble DE IB
    constexpr uint32 MaxPreamblePostambleCmdStreams = 5;
    PAL_ASSERT((internalSubmitInfo.numPreambleCmdStreams + internalSubmitInfo.numPostambleCmdStreams) <=
                MaxPreamblePostambleCmdStreams);

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
            result = static_cast<Device*>(m_pDevice)->SetStaticVmidMode(true);
            break;
        }
    }

    if (result == Result::Success)
    {
        result = GfxIpWaitPipelineUploading(submitInfo);
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
                                                      &batchSize,
                                                      EngineId());
            }
        }
        else
        {
            result = PrepareChainedCommandBuffers(internalSubmitInfo,
                                                  numNextCmdBuffers,
                                                  ppNextCmdBuffers,
                                                  &batchSize,
                                                  EngineId());
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
// Submits one or more PM4 command buffers.
Result Queue::SubmitMultiQueuePm4(
    const MultiSubmitInfo&    submitInfo,
    const InternalSubmitInfo* internalSubmitInfo)
{
    Result result = Result::Success;

    // The OsSubmit function should guarantee that we have at least one universal or compute command buffer.
    PAL_ASSERT(submitInfo.pPerSubQueueInfo[0].cmdBufferCount > 0);
    PAL_ASSERT((Type() == QueueTypeUniversal) || (Type() == QueueTypeCompute));

    // For linux platforms, there will exist at most 4 preamble + 3 postamble:
    // Preamble  (gang submit)
    // Preamble  CE IB (optional)
    // Preamble  DE IB (always)
    // Preamble  DE IB (if context switch)
    // Postamble CE IB
    // Postamble DE IB
    // Postamble  (gang submit)
    constexpr uint32 MaxPreamblePostambleCmdStreams = 7;
    PAL_ASSERT((internalSubmitInfo[0].numPreambleCmdStreams + internalSubmitInfo[0].numPostambleCmdStreams) <=
                MaxPreamblePostambleCmdStreams);

    auto*const pDevice                  = static_cast<Device*>(m_pDevice);
    uint32 numOfNonEmptyPerSubQueueInfo = 0;

    for (uint32 qIndex = 0; qIndex < submitInfo.perSubQueueInfoCount; qIndex++)
    {
        // Iteratively build batches of command buffers and launch their command streams.
        uint32            numNextCmdBuffers = submitInfo.pPerSubQueueInfo[qIndex].cmdBufferCount;
        ICmdBuffer*const* ppNextCmdBuffers  = submitInfo.pPerSubQueueInfo[qIndex].ppCmdBuffers;

        IncrementDummySubmitCount(internalSubmitInfo, ppNextCmdBuffers, numNextCmdBuffers);

        // If there are no provided cmdbuffers provided by the client, we don't attach gang submit headers for this
        // sub queue.
        if (numNextCmdBuffers == 0)
        {
            continue;
        }

        numOfNonEmptyPerSubQueueInfo++;

        // If spm enabled commands included, reserve a vmid so that the SPM_VMID could be updated
        // by KMD.
        for (uint32 idx = 0; idx < numNextCmdBuffers; ++idx)
        {
            auto pCmdBuf = static_cast<GfxCmdBuffer*>(ppNextCmdBuffers[idx]);
            if (pCmdBuf->PerfTracesEnabled().spmTraceEnabled)
            {
                result = static_cast<Device*>(m_pDevice)->SetStaticVmidMode(true);
                break;
            }
        }

        while ((result == Result::Success) && (numNextCmdBuffers > 0))
        {
            uint32 batchSize = 0;

            result = PrepareChainedCommandBuffers(internalSubmitInfo[qIndex],
                                                  numNextCmdBuffers,
                                                  ppNextCmdBuffers,
                                                  &batchSize,
                                                  m_pQueueInfos[qIndex].createInfo.engineIndex,
                                                  true);
            if (result == Result::Success)
            {
                // The batch is fully prepared, advance our tracking variables and launch the command streams.
                PAL_ASSERT(numNextCmdBuffers >= batchSize);

                numNextCmdBuffers -= batchSize;
                ppNextCmdBuffers  += batchSize;
            }
        }
    }

    if (result == Result::Success)
    {
        result = GfxIpWaitPipelineUploading(submitInfo);
    }

    if (result == Result::Success)
    {
        if (pDevice->IsRaw2SubmitSupported())
        {
            result = SubmitIbsRaw(internalSubmitInfo[0]);
        }
        else
        {
            PAL_ASSERT_ALWAYS_MSG("Attempted to perform a Multi-Submit on a device which does not support Raw2Submit");
            result = Result::Unsupported;
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
    uint32*                   pAppendedCmdBuffers,
    uint32                    engineId,
    const bool                isMultiQueue)
{
    Result result = Result::Success;

    const uint32 maxBatchSize = Min(cmdBufferCount, m_device.GetPublicSettings()->cmdBufBatchedSubmitChainLimit);

    // Determine the number of command buffers we can chain together into a single set of command streams. We can only
    // do this if exclusive submit is set. This way, we don't need to worry about the GPU reading this command buffer
    // while we patch it using the CPU.
    uint32 batchSize = 1;
    while ((batchSize < maxBatchSize) && static_cast<CmdBuffer*>(ppCmdBuffers[batchSize - 1])->IsExclusiveSubmit())
    {
        if (static_cast<CmdBuffer*>(ppCmdBuffers[0])->IsTmzEnabled() !=
            static_cast<CmdBuffer*>(ppCmdBuffers[batchSize])->IsTmzEnabled())
        {
            // All chained IBs must have the same TMZ mode since this can only be set on a per submission basis.
            break;
        }
        batchSize++;
    }

    // In MultiQueue, all command streams of a subQueue should be able to chained together.
    if (isMultiQueue && (batchSize < cmdBufferCount))
    {
        result = Result::ErrorUnavailable;
    }

    // The preamble command streams must be added to beginning of each kernel submission and cannot be chained because
    // they are shared by all submissions on this queue context. They must also separate streams because when MCBP is
    // enabled the preamble streams need to be marked as non-preemptible whereas the workload streams would be marked
    // as preemptible.
    for (uint32 idx = 0; (result == Result::Success) && (idx < internalSubmitInfo.numPreambleCmdStreams); ++idx)
    {
        PAL_ASSERT(internalSubmitInfo.pPreambleCmdStream[idx] != nullptr);
        result = AddCmdStream(*internalSubmitInfo.pPreambleCmdStream[idx],
                              engineId,
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

            if ((pCurCmdStream != nullptr) && (pCurCmdStream->IsEmpty() == false))
            {
                if (pPrevCmdStream == nullptr)
                {
                    // The first command buffer's command streams are what the kernel will launch.
                    result = AddCmdStream(*pCurCmdStream,
                                          engineId,
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
                              engineId,
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
                              EngineId(),
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
                           streamInfo.engineType,
                           streamInfo.subEngineType,
                           EngineId(),
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
                              EngineId(),
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

        for (auto iter = pCmdStream->GetFwdIterator();
            (pCmdStream != nullptr) && iter.IsValid() && (result == Result::Success);
            iter.Next())
        {
            const CmdStreamChunk*const pChunk = iter.Get();

            result = AddIb(pChunk->GpuVirtAddr(),
                           pChunk->CmdDwordsToExecute(),
                           pCmdStream->GetEngineType(),
                           pCmdStream->GetSubEngineType(),
                           EngineId(),
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
// Assigns ppCmdBuffers to a dummy command buffer and sets the command buffer count to 1 if operating in IfhModePal or
// if this is a dummy submission. Will increment submit count if m_ifhMode isn't IfhModePal and not a dummy
// subbmission.
void Queue::IncrementDummySubmitCount(
    const InternalSubmitInfo* internalSubmitInfo,
    ICmdBuffer*const*        &ppCmdBuffers,
    uint32                   &cmdBufferCount)
{
    // Use m_pDummyCmdBuffer for dummy submissions
    if ((internalSubmitInfo->flags.isDummySubmission) || (m_ifhMode == IfhModePal))
    {
        ppCmdBuffers    = reinterpret_cast<Pal::ICmdBuffer* const*>(&m_pDummyCmdBuffer);
        cmdBufferCount  = 1;
        if ((m_ifhMode != IfhModePal) && (internalSubmitInfo->flags.isDummySubmission == false))
        {
            m_pDummyCmdBuffer->IncrementSubmitCount();
        }
    }
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
// Note: user MUST lock the mutex m_globalRefLock before calling this function and ensure the lock remains until all
// functions have finished accessing m_globalRefMap and the memory pointed to by that map. This includes Submit*()
Result Queue::UpdateResourceList(
    const GpuMemoryRef* pMemRefList,
    size_t              memRefCount)
{
    InternalMemMgr*const pMemMgr = m_pDevice->MemMgr();

    Result result = Result::Success;

    // Serialize access to internalMgr and queue memory list
    RWLockAuto<RWLock::ReadOnly> lockMgr(pMemMgr->GetRefListLock());

    const bool reuseResourceList = (m_globalRefDirty == false)                               &&
                                   (memRefCount == 0)                                        &&
                                   (m_appMemRefCount == 0)                                   &&
                                   (m_hResourceList != nullptr)                              &&
                                   (m_pDevice->Settings().allocationListReusable);

    if (reuseResourceList == false)
    {
        // Ensure the caller has locked the m_globalRefLock mutex before reading m_globalRefMap
        PAL_ASSERT(m_globalRefLock.TryLockForWrite() == false);

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
                    auto*const pGpuMemory = static_cast<GpuMemory*>(iter.Get()->key);

                    result = AppendGlobalResourceToList(pGpuMemory);

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

        auto*const pDevice  = static_cast<Device*>(m_pDevice);
        // raw2 submit not supported.
        if (!(pDevice->IsRaw2SubmitSupported()))
        {
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
// Appends a global resident bo to the list of buffer objects which get submitted with a set of command buffers.
Result Queue::AppendGlobalResourceToList(
    GpuMemory* pGpuMemory)
{
    PAL_ASSERT(pGpuMemory != nullptr);

    Result result = Result::Success;

    Image* pImage = static_cast<Image*>(pGpuMemory->GetImage());
    // Skip presentable image which is already owned by Window System from globalRefBOs.
    // Designed for Vulkan, because Vulkan cannot figure out per-submission BO residency.
    if ((pGpuMemory->IsVmAlwaysValid() == false) &&
        ((pImage == nullptr) || (pImage->IsPresentable() == false) || pImage->GetIdle()))
    {
        result = AppendResourceToList(pGpuMemory);
    }

    return result;
}

// =====================================================================================================================
// Appends a bo to the list of buffer objects which get submitted with a set of command buffers.
Result Queue::AppendResourceToList(
    GpuMemory* pGpuMemory)
{
    PAL_ASSERT(pGpuMemory != nullptr);

    Result result = Result::ErrorTooManyMemoryReferences;

    if ((m_numResourcesInList + 1) <= m_resourceListSize)
    {
        // If VM is always valid, not necessary to add into the resource list.
        if (pGpuMemory->IsVmAlwaysValid() == false)
        {
            Image* pImage = static_cast<Image*>(pGpuMemory->GetImage());
            PAL_ALERT_MSG((pImage != nullptr) && (pImage->IsPresentable() == true) && (pImage->GetIdle() == false),
                "BO of presentable image which is currently owned by Window System is referenced. "
                "VA %llx, explicitSync %d. If not explicitSynced, it may trigger kernel implicit sync.",
                pGpuMemory->Desc().gpuVirtAddr, pGpuMemory->IsExplicitSync());

            auto*const pDevice  = static_cast<Device*>(m_pDevice);
            // Use raw2 submit.
            if (pDevice->IsRaw2SubmitSupported())
            {
                // For GpuMemory to be submitted in list, export and save its KMS handle.
                uint32 kmsHandle = pGpuMemory->SurfaceKmsHandle();
                if (kmsHandle == 0)
                {
                    result = pDevice->ExportBuffer(pGpuMemory->SurfaceHandle(),
                                                   amdgpu_bo_handle_type_kms,
                                                   &kmsHandle);
                    if (result == Result::Success)
                    {
                        pGpuMemory->SetSurfaceKmsHandle(kmsHandle);
                    }
                }
                m_pResourceObjectList[m_numResourcesInList] = pGpuMemory;
            }
            else
            {
                m_pResourceList[m_numResourcesInList] = pGpuMemory->SurfaceHandle();
            }

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
    uint32           engineId,
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
                        cmdStream.GetEngineType(),
                        cmdStream.GetSubEngineType(),
                        engineId,
                        cmdStream.IsPreemptionEnabled(),
                        cmdStream.DropIfSameContext(),
                        isTmzEnabled);
    }

    return result;
}

// =====================================================================================================================
// Adds an IB to the internal list. It will be submitted to the GPU during the next call to SubmitIbs.
Result Queue::AddIb(
    gpusize       gpuVirtAddr,
    uint32        sizeInDwords,
    EngineType    engineType,
    SubEngineType subEngineType,
    uint32        engineId,
    bool          isPreemptionEnabled,
    bool          dropIfSameContext,
    bool          isTmzEnabled)
{
    Result result = Result::ErrorUnknown;

    bool isConstantEngine = (subEngineType == SubEngineType::ConstantEngine);

    if (m_numIbs < MaxIbsPerSubmit)
    {
        result = Result::Success;

        bool isConstantEngine = (subEngineType == SubEngineType::ConstantEngine);

        m_ibs[m_numIbs]._pad = 0;

        // In Linux KMD, AMDGPU_IB_FLAG_PREAMBLE simply behaves just like flag "dropIfSameCtx" in windows.
        // But we are forbidden to change the flag name because the interface was already upstreamed to
        // open source libDRM, so we have to still use it for backward compatibility.
        // So far the flag is always 0 for drm_amdgpu_cs_chunk_ib chunks.
        m_ibs[m_numIbs].flags         = ((isConstantEngine       ? AMDGPU_IB_FLAG_CE              : 0) |
                                         (isPreemptionEnabled    ? AMDGPU_IB_FLAG_PREEMPT         : 0) |
                                         (dropIfSameContext      ? AMDGPU_IB_FLAG_PREAMBLE        : 0) |
                                         (m_numIbs == 0          ? AMDGPU_IB_FLAG_EMIT_MEM_SYNC   : 0) |
                                         (isTmzEnabled           ? AMDGPU_IB_FLAGS_SECURE         : 0) |
                                         (m_perfCtrWaRequired    ? AMDGPU_IB_FLAG_PERF_COUNTER    : 0) |
                                         (m_sqttWaRequired       ? AMDGPU_IB_FLAG_SQ_THREAD_TRACE : 0));

        m_ibs[m_numIbs].va_start = gpuVirtAddr;
        m_ibs[m_numIbs].ib_bytes = sizeInDwords * 4;
        m_ibs[m_numIbs].ip_type  = GetIpType(static_cast<EngineType>(engineType));
        // Quote From Kernel : Right now all IPs have only one instance - multiple rings.
        // The ip_instance should always stay at 0 for now.
        m_ibs[m_numIbs].ip_instance = 0;
        m_ibs[m_numIbs].ring        = engineId;

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

    // to use raw2 submit with DRM >= 3.27, amdgpu_bo_handles will be submitted with an extra chunk
    totalChunk += pDevice->UseBoListCreate() ? 0 : 1;

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
            chunkArray[i].chunk_id              = AMDGPU_CHUNK_ID_IB;
            chunkArray[i].length_dw             = sizeof(struct drm_amdgpu_cs_chunk_ib) / 4;
            chunkArray[i].chunk_data            = static_cast<uint64>(reinterpret_cast<uintptr_t>(&chunkDataArray[i]));
            chunkDataArray[i].ib_data._pad        = m_ibs[i]._pad;
            chunkDataArray[i].ib_data.va_start    = m_ibs[i].va_start;
            chunkDataArray[i].ib_data.ib_bytes    = m_ibs[i].ib_bytes;
            chunkDataArray[i].ib_data.ip_type     = m_ibs[i].ip_type;
            chunkDataArray[i].ib_data.ip_instance = m_ibs[i].ip_instance;
            chunkDataArray[i].ib_data.ring        = m_ibs[i].ring;
            chunkDataArray[i].ib_data.flags       = m_ibs[i].flags;
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

	// Serialize access to internalMgr and queue memory list
	RWLockAuto<RWLock::ReadWrite> lockMgr(m_pDevice->MemMgr()->GetRefListLock());

        // Prepare the resourceListEntry for non-dummy submission.
        Vector<drm_amdgpu_bo_list_entry, 1, Platform> resourceEntryList(pDevice->GetPlatform());
        if (!internalSubmitInfo.flags.isDummySubmission)
        {
            result = resourceEntryList.Reserve(m_numResourcesInList);
            if (result == Result::Success)
            {
                for (uint32 index = 0; index < m_numResourcesInList; ++index)
                {
                    if (m_pResourcePriorityList != nullptr)
                    {
                        resourceEntryList.PushBack({(*(m_pResourceObjectList + index))->SurfaceKmsHandle(),
                                                    *(m_pResourcePriorityList + index)});
                    }
                    else
                    {
                        resourceEntryList.PushBack({(*(m_pResourceObjectList + index))->SurfaceKmsHandle(), 0});
                    }
                }
            }
        }

        uint32 boList = 0;
        drm_amdgpu_bo_list_in boListIn = {};
        if (pDevice->UseBoListCreate())
        {
            // Legacy path, using the buffer list handle (uint) and passing it to the CS ioctl.
            if (internalSubmitInfo.flags.isDummySubmission)
            {
                boList = m_dummyResourceList;
            }
            else
            {
                result = pDevice->CreateResourceListRaw(m_numResourcesInList,
                                                        resourceEntryList.Data(),
                                                        &boList);
            }
        }
        else
        {
            // Standard path, passing the buffer list via the CS ioctl.
            boListIn.operation = UINT_MAX;
            boListIn.list_handle = UINT_MAX;
            boListIn.bo_number = internalSubmitInfo.flags.isDummySubmission ?
                                 m_numDummyResourcesInList :
                                 m_numResourcesInList;
            boListIn.bo_info_size = sizeof(struct drm_amdgpu_bo_list_entry);
            // The pointer needs to be reinterpreted as unsigned 64-bit value in DRM.
            boListIn.bo_info_ptr = static_cast<uint64>(reinterpret_cast<uintptr_t>(
                                            internalSubmitInfo.flags.isDummySubmission ?
                                            m_dummyResourceEntryList.Data() :
                                            resourceEntryList.Data()));

            currentChunk ++;
            chunkArray[currentChunk].chunk_id = AMDGPU_CHUNK_ID_BO_HANDLES;
            chunkArray[currentChunk].length_dw = sizeof(struct drm_amdgpu_bo_list_in) / 4;
            // The pointer needs to be reinterpreted as unsigned 64-bit value in DRM.
            chunkArray[currentChunk].chunk_data = static_cast<uint64>(reinterpret_cast<uintptr_t>(&boListIn));
        }

        result = pDevice->SubmitRaw2(pContext->Handle(),
                boList,
                totalChunk,
                &chunkArray[0],
                pContext->LastTimestampPtr());

        pContext->SetLastSignaledSyncObj(m_lastSignaledSyncObject);

        if (boList != 0 && boList != m_dummyResourceList)
        {
            // m_dummyResourceList will be destroyed in destructor.
            pDevice->DestroyResourceListRaw(boList);
        }

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

    // we should only use new submit routine when sync object is supported in the kenrel as well as raw2 submit.
    if (pDevice->IsRaw2SubmitSupported())
    {
        result = SubmitIbsRaw(internalSubmitInfo);
    }
    else
    {
        // We are using the newer drm_amdgpu_cs_chunk_ib to store data. Switch back to using the older
        // amdgpu_cs_request struct for legacy submit.
        amdgpu_cs_ib_info legacy_ibs[MaxIbsPerSubmit];
        memset(legacy_ibs, 0, sizeof(legacy_ibs));

        for (uint32 i = 0; i < m_numIbs; i++)
        {
            legacy_ibs[i].ib_mc_address = m_ibs[i].va_start;
            legacy_ibs[i].size          = (m_ibs[i].ib_bytes / 4);
            legacy_ibs[i].flags         = m_ibs[i].flags;
        }

        struct amdgpu_cs_request ibsRequest = {};
        ibsRequest.flags         = internalSubmitInfo.flags.isTmzEnabled;
        ibsRequest.ip_type       = pContext->IpType();
        ibsRequest.ring          = pContext->EngineId();
        ibsRequest.resources     = internalSubmitInfo.flags.isDummySubmission ? m_hDummyResourceList : m_hResourceList;
        ibsRequest.number_of_ibs = m_numIbs;
        ibsRequest.ibs           = legacy_ibs;

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
