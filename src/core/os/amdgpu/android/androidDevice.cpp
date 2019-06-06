/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/g_palSettings.h"

#include "core/queueSemaphore.h"
#include "core/masterQueueSemaphore.h"
#include "core/os/amdgpu/android/androidDevice.h"
#include "core/os/amdgpu/android/androidQueue.h"
#include "core/os/amdgpu/amdgpuGpuMemory.h"
#include "core/os/amdgpu/amdgpuImage.h"
#include "core/os/amdgpu/amdgpuSyncobjFence.h"

#include <log/log.h>
#include <hardware/gralloc.h>
#include <system/window.h>

using namespace Util;

namespace Pal
{
namespace Amdgpu
{

// =====================================================================================================================
AndroidDevice::AndroidDevice(
    const DeviceConstructorParams& constructorParams)
    :
    Device(constructorParams)
{
    m_syncobjForExternalSignaledFence = 0;
    m_syncobjForExternalActiveFence = 0;
}

// =====================================================================================================================
// Performs potentially unsafe OS-specific late initialization steps for this Device object. Anything created or
// initialized by this function must be destroyed or deinitialized in Cleanup().
Result AndroidDevice::OsLateInit()
{
    Result result = Result::Success;

    result = Device::OsLateInit();

    PAL_ASSERT((GetFenceType() == FenceType::SyncObj) && (GetSemaphoreType() == SemaphoreType::SyncObj));

    if (result == Result::Success)
    {
        result = CreateSyncObject(DRM_SYNCOBJ_CREATE_SIGNALED, &m_syncobjForExternalSignaledFence);
        if (result == Result::Success)
        {
            result = CreateSyncObject(0, &m_syncobjForExternalActiveFence);
        }
    }

    return result;
}

// =====================================================================================================================
// This must clean up all internal GPU memory allocations and all objects created after EarlyInit and OsEarlyInit.
Result AndroidDevice::Cleanup()
{
    Result result = Device::Cleanup();

    if (m_syncobjForExternalSignaledFence > 0)
    {
        DestroySyncObject(m_syncobjForExternalSignaledFence);
    }
    if (m_syncobjForExternalActiveFence > 0)
    {
        DestroySyncObject(m_syncobjForExternalActiveFence);
    }

    return result;
}

// =====================================================================================================================
// Configure the grallocUsage to shake hand with Mesa to create vulkan presentble buffer
Result AndroidDevice::GetSwapchainGrallocUsage(
    SwizzledFormat             format,
    ImageUsageFlags            imageUsage,
    int*                       pGrallocUsage)
{
    *pGrallocUsage = GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER;
    return Result::Success;
}

// =====================================================================================================================
// Import the native fence to PalSemaphore and/or PalFence
Result AndroidDevice::AssociateNativeFence(
    int                        nativeFenceFd,
    IQueueSemaphore*           pPalSemaphore,
    IFence*                    pPalFence)
{
    Result retValue = Result::Success;

    if (nativeFenceFd > 0)
    {
        retValue = SyncObjImportSyncFile(nativeFenceFd, m_syncobjForExternalActiveFence);

        if (pPalSemaphore != nullptr)
        {
            amdgpu_semaphore_handle hSemaphore = static_cast<Pal::QueueSemaphore*>(pPalSemaphore)->GetSyncObjHandle();
            static_cast<Pal::MasterQueueSemaphore*>(pPalSemaphore)->EarlySignal();
            retValue = ConveySyncObjectState(reinterpret_cast<uintptr_t>(hSemaphore),
                                             0,
                                             m_syncobjForExternalActiveFence,
                                             0);
        }

        if (pPalFence != nullptr)
        {
            amdgpu_syncobj_handle hSyncobj = static_cast<Pal::Amdgpu::SyncobjFence*>(pPalFence)->SyncObjHandle();
            retValue = ConveySyncObjectState(hSyncobj,
                                             0,
                                             m_syncobjForExternalActiveFence,
                                             0);
        }
    }
    else
    {
        if (pPalSemaphore != nullptr)
        {
            static_cast<Pal::MasterQueueSemaphore*>(pPalSemaphore)->EarlySignal();
            amdgpu_semaphore_handle hSemaphore = static_cast<Pal::QueueSemaphore*>(pPalSemaphore)->GetSyncObjHandle();
            retValue = ConveySyncObjectState(reinterpret_cast<uintptr_t>(hSemaphore),
                                             0,
                                             m_syncobjForExternalSignaledFence,
                                             0);
        }

        if (pPalFence != nullptr)
        {
            amdgpu_syncobj_handle hSyncobj = static_cast<Pal::Amdgpu::SyncobjFence*>(pPalFence)->SyncObjHandle();
            retValue = ConveySyncObjectState(hSyncobj,
                                             0,
                                             m_syncobjForExternalSignaledFence,
                                             0);
        }
    }

    close(nativeFenceFd);

    return retValue;
}

// =====================================================================================================================
// Constructs a new Queue object in preallocated memory
Pal::Queue* AndroidDevice::ConstructQueueObject(
    const QueueCreateInfo& createInfo,
    void*                  pPlacementAddr)
{
    Pal::Queue* pQueue = nullptr;

    switch (createInfo.queueType)
    {
    case QueueTypeCompute:
    case QueueTypeUniversal:
    case QueueTypeDma:
        pQueue = PAL_PLACEMENT_NEW(pPlacementAddr) AndroidQueue(this, createInfo);
        break;
    case QueueTypeTimer:
        // Timer Queue is not supported so far.
        PAL_NOT_IMPLEMENTED();
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return pQueue;
}

// =====================================================================================================================
// For Android case: Mesa allocate and update memory/image info, vulkan query it and produce proper output,
//                   so left this function to be empty for Android.
// For Linux case: Vulkan create internal present memory and image, xserver query and consume the vulkan output,
//                   so LinuxDevice::UpdateExternalImageInfo should do proper update.
Result AndroidDevice::UpdateExternalImageInfo(
    const PresentableImageCreateInfo&    createInfo,
    Pal::GpuMemory*                      pGpuMemory,
    Pal::Image*                          pImage)
{
    return Result::Success;
}

// =====================================================================================================================
// Creates a GPU memory object with external shared handle (createInfo.hDisplay) and
// binds it to the presentable Image associated with this object.
Result AndroidDevice::CreatePresentableMemoryObject(
    const PresentableImageCreateInfo&   presentableImageCreateInfo,
    Image*                              pImage,        // [in] Image the memory object should be based on
    void*                               pMemObjMem,    // [in,out] Preallocated memory for the GpuMemory object
    Pal::GpuMemory**                    ppMemObjOut)   // [out] Newly created GPU memory object
{
    GpuMemoryRequirements memReqs = {};
    pImage->GetGpuMemoryRequirements(&memReqs);

    const gpusize allocGranularity = MemoryProperties().realMemAllocGranularity;

    GpuMemoryCreateInfo createInfo = {};
    createInfo.flags.flippable = pImage->IsFlippable();
    createInfo.flags.stereo    = pImage->GetInternalCreateInfo().flags.stereo;
    createInfo.size            = Pow2Align(memReqs.size, allocGranularity);
    createInfo.alignment       = Pow2Align(memReqs.alignment, allocGranularity);
    createInfo.vaRange         = VaRange::Default;
    createInfo.priority        = GpuMemPriority::VeryHigh;
    createInfo.heapCount       = 0;
    createInfo.pImage          = pImage;

    for (uint32 i = 0; i < memReqs.heapCount; i++)
    {
        // Don't allocate from local visible heap since the memory won't be mapped.
        if (memReqs.heaps[i] != GpuHeapLocal)
        {
            createInfo.heaps[createInfo.heapCount] = memReqs.heaps[i];
            createInfo.heapCount++;
        }
    }

    GpuMemoryInternalCreateInfo internalInfo = {};

    Pal::GpuMemory* pGpuMemory = nullptr;
    if (presentableImageCreateInfo.hDisplay != nullptr)
    {
        const buffer_handle_t  pGrallocHandle = static_cast<buffer_handle_t>(presentableImageCreateInfo.hDisplay);

        PAL_ASSERT(pGrallocHandle->numFds == 1);

        internalInfo.flags.isExternal   = 1;
        internalInfo.hExternalResource  = dup(pGrallocHandle->data[0]);
        internalInfo.externalHandleType = amdgpu_bo_handle_type_dma_buf_fd;
    }
    Result result = CreateInternalGpuMemory(createInfo, internalInfo, pMemObjMem, &pGpuMemory);

    if (result == Result::Success)
    {
        *ppMemObjOut = static_cast<GpuMemory*>(pGpuMemory);
    }
    else
    {
        // Destroy the memory if something failed.
        pGpuMemory->Destroy();
    }

    return result;
}

} // Amdgpu
} // Pal
