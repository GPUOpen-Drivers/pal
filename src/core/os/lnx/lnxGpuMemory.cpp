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

#include "core/os/lnx/lnxDevice.h"
#include "core/os/lnx/lnxPlatform.h"
#include "core/os/lnx/lnxGpuMemory.h"

using namespace Util;

namespace Pal
{
namespace Linux
{

// =====================================================================================================================
GpuMemory::GpuMemory(
    Device* pDevice)
    :
    Pal::GpuMemory(pDevice),
    m_hSurface(nullptr),
    m_hVaRange(nullptr),
    m_offset(0),
    m_isVmAlwaysValid(false),
    m_externalHandleType(amdgpu_bo_handle_type_dma_buf_fd)
{
}

// =====================================================================================================================
GpuMemory::~GpuMemory()
{
    Device* pDevice = static_cast<Device*>(m_pDevice);
    Result  result  = Result::Success;

    if (m_hExternalResource != 0)
    {
        // Driver need to close the fd if importing successfully otherwise there is a resource leak.
        close(m_hExternalResource);
        m_hExternalResource = 0;
    }

    // Unmap the buffer object and free its virtual address.
    if (m_desc.gpuVirtAddr != 0)
    {
        if (IsVirtual() == false)
        {
            // virtual allocation just reserve the va range but never map to itself.
            result = pDevice->UnmapVirtualAddress(m_hSurface, m_offset, m_desc.size, m_desc.gpuVirtAddr);
            PAL_ALERT(result != Result::Success);
        }
        else
        {
            pDevice->DiscardReservedPrtVaRange(m_desc.gpuVirtAddr, m_desc.size);
        }
        if (m_vaRange != VaRange::Svm)
        {
            pDevice->FreeVirtualAddress(this);
        }
    }

    if ((m_vaRange == VaRange::Svm) && (IsGpuVaPreReserved() == false))
    {
        Result result;
        if (IsSvmAlloc())
        {
            result = VirtualRelease(reinterpret_cast<void*>(m_desc.gpuVirtAddr), static_cast<size_t>(m_desc.size));
        }
        else
        {
            result = FreeSvmVirtualAddress();
        }
        PAL_ASSERT(result == Result::Success);
    }

    if (m_hSurface != nullptr)
    {
        result = pDevice->FreeBuffer(m_hSurface);
        PAL_ASSERT(result == Result::Success);
    }

    if (m_desc.flags.isExternPhys)
    {
        result = pDevice->FreeSdiSurface(this);
        PAL_ASSERT(result == Result::Success);
    }
}

// =====================================================================================================================
// Part of the destruction of this object requires invoking virtual functions, which is not safe to do inside of a
// destructor. So, we'll do some of the clean-up inside Destroy().
// NOTE: Part of the public IDestroyable interface.
void GpuMemory::Destroy()
{
    // The base class' Destroy method will invoke our destructor.
    Pal::GpuMemory::Destroy();
}

// =====================================================================================================================
// Performs OS-specific initialization for allocating real, pinned or virtual memory objects. Responsible for reserving
// GPU virtual address space for the allocation, and creating the allocation itself.
Result GpuMemory::AllocateOrPinMemory(
    gpusize                 baseVirtAddr,              // If non-zero, the base GPU virtual address the caller requires.
    uint64*                 pPagingFence,              // Ignored on Linux platforms.
    VirtualGpuMemAccessMode virtualAccessMode,         // Ignored on Linux platforms.
    uint32                  multiDeviceGpuMemoryCount, // Ignored on Linux platforms.
    IDevice*const*          ppDevice,                  // Ignored on Linux platforms.
    Pal::Image*const*       ppImage)                   // Ignored on Linux platforms.
{
    Device*                        pDevice      = static_cast<Device*>(m_pDevice);
    struct amdgpu_bo_alloc_request allocRequest = { };
    amdgpu_bo_handle               bufferHandle = nullptr;

    // On Linux, KMD manages our page directory and page tables, so we never expect to get an allocation request
    // for those usages.
    PAL_ASSERT((IsPageDirectory() == false) && (IsPageTableBlock() == false));

    Result result = Result::Success;

    if (IsSvmAlloc())
    {
        PAL_ASSERT(baseVirtAddr == 0);
        result = VirtualReserve(static_cast<size_t>(m_desc.size),
                                reinterpret_cast<void**>(&m_desc.gpuVirtAddr),
                                nullptr,
                                static_cast<size_t>(m_desc.alignment));
        if (result == Result::Success)
        {
            result = VirtualCommit(reinterpret_cast<void*>(m_desc.gpuVirtAddr),
                                   static_cast<size_t>(m_desc.size),
                                   IsExecutable());
        }
        if ((result == Result::Success) && IsUserQueue())
        {
            baseVirtAddr = m_desc.gpuVirtAddr;
            memset(reinterpret_cast<void*>(baseVirtAddr), 0, static_cast<size_t>(m_desc.size));
        }
    }
    else if (IsGpuVaPreReserved())
    {
        PAL_ASSERT(IsPeer() == false);
        PAL_ASSERT(baseVirtAddr != 0);
        m_desc.gpuVirtAddr = baseVirtAddr;
    }
    else if (m_vaRange != VaRange::Svm)
    {
        result = pDevice->AssignVirtualAddress(this, &baseVirtAddr);
    }

    if (result == Result::Success)
    {
        m_desc.gpuVirtAddr = baseVirtAddr;

        if (IsVirtual() == false)
        {
            if (IsPinned())
            {
                // the pinned memory has special requirement for size and base virtual address.
                // both supposed to be alinged to page boundary otherwise the pinned down operation
                // will fail.
                PAL_ASSERT(m_pPinnedMemory != nullptr);
                result = pDevice->PinMemory(m_pPinnedMemory,
                                            m_desc.size,
                                            &m_offset,
                                            &bufferHandle);
            }
            else
            {
                if (pDevice->Settings().alwaysResident)
                {
                    allocRequest.flags = AMDGPU_GEM_CREATE_NO_EVICT;
                }
                // Note: From PAL's perspective, heap[0] has its priority according to GpuMemPriority. But from amdgpu's
                // perspective, the priority is always "local invisible, local visible, remote WC, remote cacheable"
                // when multiple heaps specified. For example, if preferred_heap is GTT and VRAM, and the flags is
                // AMDGPU_GEM_CREATE_NO_CPU_ACCESS, amdgpu will try to move the buffer object to in order of
                // "invisible, visible, remote cacheable". So, in amdgpu, the priority of heap[0] is not respected.
                // Anyway, if the app set two heaps, it means the buffer could be in either of them though the
                // heap[0] might be preferred for app.

                // - If remote goes ahead of local in the heaps
                //   1: Allocate remote first.
                //   2: Allocate local if remote failed to be allocated
                // - If local goes ahead of remote in the heaps
                //   Follow current model
                if ((m_heaps[0] == GpuHeapLocal) || (m_heaps[0] == GpuHeapInvisible))
                {
                    // Linux kernel doesn't respect the priority of heaps, so:
                    // (1) Local memory: once invisible heap is selected, eliminate visible from the preferred heap.
                    // (2) Remote memory: just care about the first remote heap regardless of the second.
                    for (uint32 heap = 0; heap < m_heapCount; ++heap)
                    {
                        switch (m_heaps[heap])
                        {
                            case GpuHeapGartUswc:
                                if ((allocRequest.preferred_heap & AMDGPU_GEM_DOMAIN_GTT) == 0)
                                {
                                    allocRequest.flags |= AMDGPU_GEM_CREATE_CPU_GTT_USWC;
                                }
                                // Fall through next
                            case GpuHeapGartCacheable:
                                allocRequest.preferred_heap |= AMDGPU_GEM_DOMAIN_GTT;
                                break;
                            case GpuHeapLocal:
                                if ((allocRequest.flags & AMDGPU_GEM_CREATE_NO_CPU_ACCESS) == 0)
                                {
                                    allocRequest.flags |= AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
                                    if (IsBusAddressable())
                                    {
                                        allocRequest.preferred_heap = AMDGPU_GEM_DOMAIN_DGMA;
                                    }
                                    else
                                    {
                                        allocRequest.preferred_heap |= AMDGPU_GEM_DOMAIN_VRAM;
                                    }
                                }
                                break;
                            case GpuHeapInvisible:
                                allocRequest.flags          &= ~AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
                                allocRequest.flags          |= AMDGPU_GEM_CREATE_NO_CPU_ACCESS;
                                allocRequest.preferred_heap |= AMDGPU_GEM_DOMAIN_VRAM;
                                break;
                            default:
                                PAL_ASSERT_ALWAYS();
                                break;
                        }
                    }
                }
                else
                {
                    // we just care about the first heap if remote comes first until kernel
                    // work out a solution to respect the priority of heaps.
                    switch (m_heaps[0])
                    {
                        case GpuHeapGartUswc:
                            allocRequest.flags |= AMDGPU_GEM_CREATE_CPU_GTT_USWC;
                            // Fall through next
                        case GpuHeapGartCacheable:
                            allocRequest.preferred_heap |= AMDGPU_GEM_DOMAIN_GTT;
                            break;
                        default:
                            PAL_ASSERT_ALWAYS();
                            break;
                    }
                }

                if (pDevice->Settings().clearAllocatedLfb &&
                    (allocRequest.preferred_heap & AMDGPU_GEM_DOMAIN_VRAM))
                {
                    allocRequest.flags |= AMDGPU_GEM_CREATE_VRAM_CLEARED;
                }

                if (pDevice->IsVmAlwaysValidSupported()     &&
                    // Remove DGMA memory from optimization, since DGMA requires blocking the surface from migration.
                    // It seems KGD logic executes that operation on the usage.
                    (allocRequest.preferred_heap != AMDGPU_GEM_DOMAIN_DGMA) &&
                    (m_flags.isFlippable == 0)              && // Memory shared by multiple processes are not allowed
                    (m_flags.interprocess == 0)             && // Memory shared by multiple processes are not allowed
                    (m_desc.flags.isExternal == 0)          && // Memory shared by multiple processes are not allowed
                    (m_flags.isShareable == 0)              && // Memory shared by multiple devices are not allowed
                    (m_flags.peerWritable == 0))               // Memory can be writen by peer device are not allowed
                {
                    // VM always valid guarantees VM addresses are always valid within local VM context.
                    allocRequest.flags |= AMDGPU_GEM_CREATE_VM_ALWAYS_VALID;
                    m_isVmAlwaysValid = true;
                }

                allocRequest.alloc_size     = m_desc.size;
                allocRequest.phys_alignment = m_desc.alignment;

                result = pDevice->AllocBuffer(&allocRequest, &bufferHandle);
            }
            if (result == Result::Success)
            {
                m_hSurface = bufferHandle;
                // Mapping the virtual address to the buffer object.
                result = pDevice->MapVirtualAddress(bufferHandle, m_offset, m_desc.size, m_desc.gpuVirtAddr, m_mtype);
            }
        }
        else
        {
            // base driver requires us to reserve the PRT range ahead of time
            // they will mark the T flag as 1 and set the valid flag as 0 for the whole range.
            result = pDevice->ReservePrtVaRange(m_desc.gpuVirtAddr, m_desc.size, m_mtype);
        }
    }

    return result;
}

// =====================================================================================================================
Result GpuMemory::Init(
    const GpuMemoryCreateInfo&         createInfo,
    const GpuMemoryInternalCreateInfo& internalInfo)
{
    if (internalInfo.flags.isExternal)
    {
        m_externalHandleType = static_cast<enum amdgpu_bo_handle_type>(internalInfo.externalHandleType);
    }

    Result result = Pal::GpuMemory::Init(createInfo, internalInfo);

    if (createInfo.flags.sdiExternal)
    {
        Device* pDevice = static_cast<Device*>(m_pDevice);
        m_desc.surfaceBusAddr = createInfo.surfaceBusAddr;
        m_desc.markerBusAddr = createInfo.markerBusAddr;
        result = pDevice->SetSdiSurface(this, &(m_desc.gpuVirtAddr));
    }

    return result;
}

// =====================================================================================================================
Result GpuMemory::AllocateSvmVirtualAddress(
    gpusize baseVirtAddr,
    gpusize size,
    gpusize align,
    bool    commitCpuVa)
{
    Device* pDevice = static_cast<Device*>(m_pDevice);
    Result  result = Result::Success;

    PAL_ASSERT(m_vaRange == VaRange::Svm);

    if (baseVirtAddr == 0)
    {
        result = pDevice->GetSvmMgr()->AllocVa(size, static_cast<uint32>(align), &baseVirtAddr);
        if ((result == Result::Success) && commitCpuVa)
        {
            result = VirtualCommit(reinterpret_cast<void*>(baseVirtAddr), static_cast<size_t>(size));
        }
    }
    if (result == Result::Success)
    {
        m_desc.gpuVirtAddr = baseVirtAddr;
        m_desc.size        = size;
        m_desc.alignment   = align;
        m_pPinnedMemory    = reinterpret_cast<const void*>(m_desc.gpuVirtAddr);
    }

    return result;
}

// =====================================================================================================================
Result GpuMemory::FreeSvmVirtualAddress()
{
    Result  result = Result::Success;
    Device* pDevice = static_cast<Device*>(m_pDevice);

    PAL_ASSERT(m_vaRange == VaRange::Svm);

    if (m_pPinnedMemory != nullptr)
    {
        result = VirtualDecommit(reinterpret_cast<void*>(m_desc.gpuVirtAddr), static_cast<size_t>(m_desc.size));
        PAL_ASSERT(result == Result::Success);
    }

    if (result == Result::Success)
    {
        pDevice->GetSvmMgr()->FreeVa(m_desc.gpuVirtAddr);
    }

    return result;
}

// =====================================================================================================================
// Performs OS-specific initialization for allocating shared memory objects.
// The "shared" memory object refers to
// a:   GPU memory object residing in a non-local heap which can be accessed (shared between) two or more GPU's
//      without requiring peer memory transfers.
// b:   The memory may allocated from the same device but export/imported across driver stack or process boundary.
// c:   The memory may be allocated from peer device and need to be imported to current device.
Result GpuMemory::OpenSharedMemory()
{
    amdgpu_bo_info          bufferInfo   = {};
    Device*                 pDevice      = static_cast<Device*>(m_pDevice);
    gpusize                 baseVirtAddr = 0;

    // open the external memory with virtual address assigned
    Result result = OpenPeerMemory();

    if (result == Result::Success)
    {
        result      = pDevice->QueryBufferInfo(m_hSurface, &bufferInfo);
        m_heapCount = 1;

        if (bufferInfo.preferred_heap & AMDGPU_GEM_DOMAIN_GTT)
        {
            // Check for any unexpected flags
            PAL_ASSERT((bufferInfo.preferred_heap & ~AMDGPU_GEM_DOMAIN_GTT) == 0);

            if (bufferInfo.alloc_flags & AMDGPU_GEM_CREATE_CPU_GTT_USWC)
            {
                m_heaps[0] = GpuHeapGartUswc;
            }
            else
            {
                // Check for any unexpected flags
                PAL_ASSERT(bufferInfo.alloc_flags == 0);

                m_heaps[0] = GpuHeapGartCacheable;
            }
        }
        else if (bufferInfo.preferred_heap & AMDGPU_GEM_DOMAIN_VRAM)
        {
            // Check for any unexpected flags
            PAL_ASSERT((bufferInfo.preferred_heap & ~AMDGPU_GEM_DOMAIN_VRAM) == 0);

            if (bufferInfo.alloc_flags & AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED)
            {
                // Check for any unexpected flags
                PAL_ASSERT((bufferInfo.alloc_flags & ~AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED) == 0);
                m_heaps[0] = GpuHeapLocal;
            }
            else if (bufferInfo.alloc_flags & AMDGPU_GEM_CREATE_NO_CPU_ACCESS)
            {
                // Check for any unexpected flags
                PAL_ASSERT((bufferInfo.alloc_flags & ~AMDGPU_GEM_CREATE_NO_CPU_ACCESS) == 0);

                m_heaps[0] = GpuHeapInvisible;
            }
            else
            {
                PAL_ASSERT_ALWAYS();
            }
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }

        const GpuMemoryHeapProperties& heapProps = pDevice->HeapProperties(m_heaps[0]);

        if (heapProps.flags.cpuVisible == 0)
        {
            m_flags.cpuVisible = 0;
        }

        switch (m_heaps[0])
        {
            case GpuHeapLocal:
            case GpuHeapInvisible:
                m_flags.nonLocalOnly = 0;
                break;
            case GpuHeapGartCacheable:
            case GpuHeapGartUswc:
                m_flags.localOnly = 0;
                break;
            default:
                break;
        }

    }

    return result;
}

// =====================================================================================================================
// Performs OS-specific initialization for allocating peer memory objects.
Result GpuMemory::OpenPeerMemory()
{
    // Get the external resource handle from original memory object if it is not set before.
    if ((m_hExternalResource == 0) && (m_pOriginalMem != nullptr))
    {
        m_hExternalResource = static_cast<GpuMemory*>(m_pOriginalMem)->GetSharedExternalHandle();
    }

    amdgpu_bo_import_result importResult    = {};
    Device* pDevice                         = static_cast<Device*>(m_pDevice);
    gpusize baseVirtAddr                    = 0;

    Result result = pDevice->ImportBuffer(m_externalHandleType, m_hExternalResource, &importResult);

    if (result == Result::Success)
    {
        m_hSurface  = importResult.buf_handle;

        if (IsGpuVaPreReserved())
        {
            // It's not expected to get here. Implement later if this feature is desired for Linux.
            PAL_NOT_IMPLEMENTED();
            result = Result::Unsupported;
        }
        else
        {
            amdgpu_bo_info bufferInfo   = {};
            result = pDevice->QueryBufferInfo(m_hSurface, &bufferInfo);

            if (result == Result::Success)
            {
                m_desc.size         = bufferInfo.alloc_size;
                m_desc.alignment    = bufferInfo.phys_alignment;

                result = pDevice->AssignVirtualAddress(this, &baseVirtAddr);
            }
        }
    }

    if (result == Result::Success)
    {
        m_desc.gpuVirtAddr = baseVirtAddr;

        result = pDevice->MapVirtualAddress(m_hSurface, 0, importResult.alloc_size, m_desc.gpuVirtAddr, m_mtype);

        if (result != Result::Success)
        {
            pDevice->FreeVirtualAddress(this);
        }
    }

    return result;
}

// =====================================================================================================================
// Changes the allocation's priority. This has no meaning on Linux because the amdgpu driver doesn't support it.
Result GpuMemory::OsSetPriority(
    GpuMemPriority       priority,
    GpuMemPriorityOffset priorityOffset)
{
    return Result::Success; // Pretend everything worked!
}

// =====================================================================================================================
// Export gpu memory as fd (dma_buf_fd)
OsExternalHandle GpuMemory::GetSharedExternalHandle() const
{
    // According to Vulkan spec, the vkGetMemoryFdKHX requires a new fd for each call, and it is application's
    // responsibility to close the fd.
    // Driver do not need to maintain the share fd anymore.
    // another valid use case for this is to share image to XServer as pixmap.
    OsExternalHandle fd;
    Result result = static_cast<Device*>(m_pDevice)->ExportBuffer(m_hSurface,
                                            amdgpu_bo_handle_type_dma_buf_fd,
                                            reinterpret_cast<uint32*>(&fd));
    PAL_ASSERT(result == Result::Success);
    return fd;
}

// =====================================================================================================================
// Maps the allocation into CPU address space.
Result GpuMemory::OsMap(
    void** ppData)
{
    Device* pDevice = static_cast<Device*>(m_pDevice);

    Result result = pDevice->Map(m_hSurface, ppData);

    return result;
}

// =====================================================================================================================
// Unmaps the allocation out of CPU address space.
Result GpuMemory::OsUnmap()
{
    Device* pDevice = static_cast<Device*>(m_pDevice);

    Result result = pDevice->Unmap(m_hSurface);

    return result;
}

// =====================================================================================================================
// Get Heap information of current allocation
void GpuMemory::GetHeapsInfo(
    uint32*     pHeapCount,
    GpuHeap**   ppHeaps) const
{
    PAL_ASSERT(pHeapCount != nullptr);
    PAL_ASSERT((ppHeaps != nullptr) && (*ppHeaps != nullptr));

    *pHeapCount = m_heapCount;
    memcpy(*ppHeaps, m_heaps, m_heapCount*sizeof(GpuHeap));
}

// =====================================================================================================================
// Query bus addresses of surface and marker for BusAddressable memory
Result GpuMemory::QuerySdiBusAddress()
{
    Result result = Result::ErrorOutOfGpuMemory;
    if (IsBusAddressable())
    {
        uint64 busAddress;
        Device* pDevice = static_cast<Device*>(m_pDevice);

        result = pDevice->QuerySdiSurface(m_hSurface, &busAddress);
        if (result == Result::Success)
        {
            m_desc.surfaceBusAddr = busAddress;
            const gpusize pageSize = m_pDevice->MemoryProperties().virtualMemPageSize;
            m_desc.markerBusAddr = busAddress + m_desc.size - pageSize;

            gpusize markerVa = m_desc.gpuVirtAddr + m_desc.markerBusAddr - m_desc.surfaceBusAddr;
            SetBusAddrMarkerVa(markerVa);
        }
    }

    return result;
}
} // Linux
} // Pal
