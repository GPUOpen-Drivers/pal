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

#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuGpuMemory.h"
#include "core/os/amdgpu/amdgpuPlatform.h"

using namespace Util;

namespace Pal
{
namespace Amdgpu
{

// =====================================================================================================================
GpuMemory::GpuMemory(
    Device* pDevice)
    :
    Pal::GpuMemory(pDevice),
    m_hSurface(nullptr),
    m_hVaRange(nullptr),
    m_hSurfaceKms(0),
    m_offset(0),
    m_amdgpuFlags{},
    m_externalHandleType(amdgpu_bo_handle_type_dma_buf_fd)
{
}

// =====================================================================================================================
GpuMemory::~GpuMemory()
{
    Amdgpu::Device*const pDevice = static_cast<Amdgpu::Device*>(m_pDevice);

    Result  result  = Result::Success;

    // The amdgpu device tracks per-allocation residency information which we must force it to remove because the
    // client might not call RemoveGpuMemoryReferences once for each time they call AddGpuMemoryReferences.
    IGpuMemory* pGpuMemory = this;
    pDevice->RemoveGlobalReferences(1, &pGpuMemory, true);

    if (IsExternPhys() && (m_desc.gpuVirtAddr != 0))
    {
        result = pDevice->FreeSdiSurface(this);
        PAL_ASSERT(result == Result::Success);
    }

    // Unmap the buffer object and free its virtual address.
    if (m_desc.gpuVirtAddr != 0)
    {
        const bool freeVirtAddr = m_amdgpuFlags.isShared ? pDevice->RemoveFromSharedBoMap(m_hSurface) : true;

        if (IsVirtual() == false)
        {
            if (freeVirtAddr)
            {
                // virtual allocation just reserve the va range but never map to itself.
                result = pDevice->UnmapVirtualAddress(m_hSurface, m_offset, m_desc.size, m_desc.gpuVirtAddr);
                PAL_ALERT(result != Result::Success);
            }
        }
        else
        {
            pDevice->DiscardReservedPrtVaRange(m_desc.gpuVirtAddr, m_desc.size);
        }
        if (m_vaPartition != VaPartition::Svm)
        {
            if (freeVirtAddr)
            {
                pDevice->FreeVirtualAddress(this);
            }
        }
    }

    if ((m_vaPartition == VaPartition::Svm) && (IsGpuVaPreReserved() == false))
    {
        result = IsSvmAlloc()
                    ? VirtualRelease(reinterpret_cast<void*>(m_desc.gpuVirtAddr), static_cast<size_t>(m_desc.size))
                    : FreeSvmVirtualAddress();
        PAL_ASSERT(result == Result::Success);
    }

    if (m_hSurface != nullptr)
    {
        result = pDevice->FreeBuffer(m_hSurface);
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
    Amdgpu::Device*const pDevice = static_cast<Amdgpu::Device*>(m_pDevice);

    struct amdgpu_bo_alloc_request allocRequest = { };
    amdgpu_bo_handle               bufferHandle = nullptr;

    // On Linux, KMD manages our page directory and page tables, so we never expect to get an allocation request
    // for those usages.
    PAL_ASSERT((IsPageDirectory() == false) && (IsPageTableBlock() == false));

    Result result = Result::Success;
    const GpuChipProperties& chipProps = pDevice->ChipProperties();

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
    else if (m_vaPartition != VaPartition::Svm)
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
                    bool  validHeapFound = false; // be pessimistic

                    // Linux kernel doesn't respect the priority of heaps, so:
                    // (1) Local memory: once invisible heap is selected, eliminate visible from the preferred heap.
                    // (2) Remote memory: just care about the first remote heap regardless of the second.
                    for (uint32 heap = 0; heap < m_heapCount; ++heap)
                    {
                        const GpuHeap gpuHeap  = m_heaps[heap];
                        const gpusize heapSize = m_pDevice->HeapLogicalSize(gpuHeap);

                        // Make sure the requested heap exists
                        if (heapSize != 0)
                        {
                            validHeapFound = true;

                            switch (gpuHeap)
                            {
                                case GpuHeapGartUswc:
                                    if ((allocRequest.preferred_heap & AMDGPU_GEM_DOMAIN_GTT) == 0)
                                    {
                                        allocRequest.flags |= AMDGPU_GEM_CREATE_CPU_GTT_USWC;
                                    }
                                    // Kernel request: Protected memory should be allocated with flag AMDGPU_GEM_CREATE_ENCRYPTED.
                                    if (m_flags.tmzProtected)
                                    {
                                        allocRequest.flags |=  AMDGPU_GEM_CREATE_ENCRYPTED;
                                    }
                                    // Fall through next
                                case GpuHeapGartCacheable:
                                    allocRequest.preferred_heap |= AMDGPU_GEM_DOMAIN_GTT;
                                    // Kernel request: Protected memory should be allocated with flag AMDGPU_GEM_CREATE_ENCRYPTED.
                                    if (m_flags.tmzProtected)
                                    {
                                        allocRequest.flags |=  AMDGPU_GEM_CREATE_ENCRYPTED;
                                    }
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
                                    // Kernel request: Protected memory should be allocated with flag AMDGPU_GEM_CREATE_ENCRYPTED.
                                    if (m_flags.tmzProtected)
                                    {
                                        allocRequest.flags |=  AMDGPU_GEM_CREATE_ENCRYPTED;
                                    }
                                    break;
                                case GpuHeapInvisible:
                                    allocRequest.flags          &= ~AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
                                    allocRequest.flags          |= AMDGPU_GEM_CREATE_NO_CPU_ACCESS;
                                    allocRequest.preferred_heap |= AMDGPU_GEM_DOMAIN_VRAM;

                                    // Kernel request: Protected memory should be allocated with flag AMDGPU_GEM_CREATE_ENCRYPTED.
                                    if (m_flags.tmzProtected)
                                    {
                                        allocRequest.flags |=  AMDGPU_GEM_CREATE_ENCRYPTED;
                                    }
                                    break;
                                default:
                                    PAL_ASSERT_ALWAYS();
                                    break;
                            }
                        }
                    }

                    if (validHeapFound == false)
                    {
                        // Provide some info that we're getting into this path
                        PAL_ALERT_ALWAYS();

                        // Duplication of Windows path
                        PAL_NOT_TESTED();

                        // None of the heaps the client requested exist; provide a fallback to the GART heap here.
                        allocRequest.flags          |= AMDGPU_GEM_CREATE_CPU_GTT_USWC;
                        allocRequest.preferred_heap |= AMDGPU_GEM_DOMAIN_GTT;
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

                            // Kernel request: Protected memory should be allocated with flag AMDGPU_GEM_CREATE_ENCRYPTED.
                            if (m_flags.tmzProtected)
                            {
                                allocRequest.flags |=  AMDGPU_GEM_CREATE_ENCRYPTED;
                            }
                            // Fall through next
                        case GpuHeapGartCacheable:
                            allocRequest.preferred_heap |= AMDGPU_GEM_DOMAIN_GTT;

                            // Kernel request: Protected memory should be allocated with flag AMDGPU_GEM_CREATE_ENCRYPTED.
                            if (m_flags.tmzProtected)
                            {
                                allocRequest.flags |=  AMDGPU_GEM_CREATE_ENCRYPTED;
                            }
                            break;
                        default:
                            PAL_ASSERT_ALWAYS();
                            break;
                    }
                }

                if ((pDevice->Settings().isLocalHeapPreferred || (m_priority >= GpuMemPriority::VeryHigh)) &&
                    (allocRequest.preferred_heap & AMDGPU_GEM_DOMAIN_VRAM) && (chipProps.gpuType != GpuType::Integrated))
                {
                    allocRequest.flags          &= ~AMDGPU_GEM_CREATE_CPU_GTT_USWC;
                    allocRequest.preferred_heap &= ~AMDGPU_GEM_DOMAIN_GTT;

                }

                if (pDevice->Settings().enableNullCpuAccessFlag &&
                    (allocRequest.preferred_heap & AMDGPU_GEM_DOMAIN_VRAM))
                {
                    allocRequest.flags &= ~AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
                    allocRequest.flags &= ~AMDGPU_GEM_CREATE_NO_CPU_ACCESS;
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
                    m_amdgpuFlags.isVmAlwaysValid = true;
                }

                // Use explicit sync for multi process memory and assume external synchronization
                if (IsExplicitSync() &&
                    ((m_flags.interprocess == 1)    ||
                     (m_desc.flags.isExternal == 1) ||
                     (m_flags.isShareable == 1)))
                {
                    allocRequest.flags |= AMDGPU_GEM_CREATE_EXPLICIT_SYNC;
                }

                allocRequest.alloc_size     = m_desc.size;
                allocRequest.phys_alignment = GetPhysicalAddressAlignment();

                result = pDevice->AllocBuffer(&allocRequest, &bufferHandle);
            }
            if (result == Result::Success)
            {
                m_hSurface = bufferHandle;
                // Mapping the virtual address to the buffer object.
                result = pDevice->MapVirtualAddress(bufferHandle, m_offset, m_desc.size, m_desc.gpuVirtAddr, m_mtype);
            }

            // Add internal memory to the global list, all of the internal memory are alwaysResident memory.
            // When alwaysResident enabled by settings, resource list is not necessary.
            if ((result == Result::Success) && (m_amdgpuFlags.isVmAlwaysValid == false) && IsAlwaysResident() &&
                (pDevice->Settings().alwaysResident == false))
            {
                GpuMemoryRef memRef = {};

                memRef.pGpuMemory = this;

                result = pDevice->AddGpuMemoryReferences(1, &memRef, nullptr, 0);
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

    if (createInfo.flags.sdiExternal &&
        ((createInfo.surfaceBusAddr != 0) || (createInfo.markerBusAddr != 0)))
    {
        Device* pDevice = static_cast<Device*>(m_pDevice);
        SetSurfaceBusAddr(createInfo.surfaceBusAddr);
        SetMarkerBusAddr(createInfo.markerBusAddr);
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

    PAL_ASSERT(m_vaPartition == VaPartition::Svm);

    if (baseVirtAddr == 0)
    {
        result = pDevice->GetSvmMgr()->AllocVa(size, static_cast<uint32>(align), &baseVirtAddr);

        if (result == Result::Success)
        {
            m_desc.gpuVirtAddr = baseVirtAddr;
            m_desc.size        = size;
            m_desc.alignment   = align;

            if (commitCpuVa)
            {
                result = VirtualCommit(reinterpret_cast<void*>(baseVirtAddr), static_cast<size_t>(size));

                if (result == Result::Success)
                {
                    m_pPinnedMemory = reinterpret_cast<const void*>(m_desc.gpuVirtAddr);
                }
            }
        }
    }
    else
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

    PAL_ASSERT(m_vaPartition == VaPartition::Svm);

    if (m_pPinnedMemory != nullptr)
    {
        result = VirtualDecommit(const_cast<void*>(m_pPinnedMemory), static_cast<size_t>(m_desc.size));
        PAL_ASSERT(result == Result::Success);
    }

    if ((result == Result::Success) && (m_desc.gpuVirtAddr != 0))
    {
        pDevice->GetSvmMgr()->FreeVa(m_desc.gpuVirtAddr);
    }

    return result;
}

// =====================================================================================================================
Result GpuMemory::ImportMemory(
    amdgpu_bo_handle_type handleType,
    OsExternalHandle      handle)
{
    amdgpu_bo_import_result importResult = {};
    Device* const           pDevice      = static_cast<Device*>(m_pDevice);

    Result result = pDevice->ImportBuffer(handleType, handle, &importResult);

    if (result == Result::Success)
    {
        m_hSurface  = importResult.buf_handle;

        if (IsGpuVaPreReserved() == false)
        {
            // if we allocate from external memory handle, the size/alignment of original memory is unknown.
            // we have to query the kernel to get those information and fill the desc struct.
            // Otherwise, we should not override the size/alignment.
            if (m_desc.size == 0)
            {
                amdgpu_bo_info bufferInfo   = {};
                result = pDevice->QueryBufferInfo(m_hSurface, &bufferInfo);
                if (result == Result::Success)
                {
                    m_desc.size         = bufferInfo.alloc_size;
                    m_desc.alignment    = bufferInfo.phys_alignment;
                }
            }

            if (result == Result::Success)
            {
                m_hVaRange = pDevice->SearchSharedBoMap(m_hSurface, &m_desc.gpuVirtAddr);

                if (m_hVaRange != nullptr)
                {
                    m_amdgpuFlags.isShared = true;
                }
                else
                {
                    result = pDevice->AssignVirtualAddress(this, &m_desc.gpuVirtAddr);
                }
            }
        }
    }

    if ((result == Result::Success) && (m_amdgpuFlags.isShared == 0))
    {
        result = pDevice->MapVirtualAddress(m_hSurface, 0, m_desc.size, m_desc.gpuVirtAddr, m_mtype);

        if (result == Result::Success)
        {
            if (IsGpuVaPreReserved() == false)
            {
                m_amdgpuFlags.isShared = pDevice->AddToSharedBoMap(m_hSurface, m_hVaRange, m_desc.gpuVirtAddr);
            }
        }
        else
        {
            pDevice->FreeVirtualAddress(this);
        }
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
Result GpuMemory::OpenSharedMemory(
    OsExternalHandle handle)
{
    Amdgpu::Device*const pDevice = static_cast<Amdgpu::Device*>(m_pDevice);

    amdgpu_bo_info bufferInfo   = {};
    gpusize        baseVirtAddr = 0;

    // open the external memory with virtual address assigned
    Result result = ImportMemory(m_externalHandleType, handle);

    if (result == Result::Success)
    {
        PAL_ASSERT(m_hSurface != nullptr);

        result      = pDevice->QueryBufferInfo(m_hSurface, &bufferInfo);
        m_heapCount = 0;

        if ((result == Result::Success) && IsExternal())
        {
            auto*const pUmdMetaData = reinterpret_cast<amdgpu_bo_umd_metadata*>
                                (&bufferInfo.metadata.umd_metadata[PRO_UMD_METADATA_OFFSET_DWORD]);

            auto*const pUmdSharedMetadata = reinterpret_cast<amdgpu_shared_metadata_info*>
                                (&pUmdMetaData->shared_metadata_info);

            m_desc.uniqueId = Uint64CombineParts(pUmdSharedMetadata->resource_id,
                pUmdSharedMetadata->resource_id_high32);
        }

        if (bufferInfo.preferred_heap & AMDGPU_GEM_DOMAIN_VRAM)
        {
            if (bufferInfo.alloc_flags & AMDGPU_GEM_CREATE_NO_CPU_ACCESS)
            {
                m_heaps[m_heapCount++] = GpuHeapInvisible;
            }
            else
            {
                m_heaps[m_heapCount++] = GpuHeapLocal;
            }
        }

        if (bufferInfo.preferred_heap & AMDGPU_GEM_DOMAIN_GTT)
        {
            if (bufferInfo.alloc_flags & AMDGPU_GEM_CREATE_CPU_GTT_USWC)
            {
                m_heaps[m_heapCount++] = GpuHeapGartUswc;
            }
            else
            {
                m_heaps[m_heapCount++] = GpuHeapGartCacheable;
            }
        }

        if (m_heapCount == 0)
        {
            PAL_ASSERT_ALWAYS();
        }

        m_flags.cpuVisible = 1;
        m_desc.heapCount   = m_heapCount;

        for (uint32 heap = 0; heap < m_heapCount; ++heap)
        {
            const GpuMemoryHeapProperties& heapProps = pDevice->HeapProperties(m_heaps[heap]);

            m_flags.cpuVisible &= heapProps.flags.cpuVisible;

            switch (m_heaps[heap])
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

            m_desc.heaps[heap] = m_heaps[heap];
        }
        if (bufferInfo.alloc_flags & AMDGPU_GEM_CREATE_EXPLICIT_SYNC)
        {
            m_flags.explicitSync = 1;
        }
    }

    // On native Linux, handle should be closed here if it's a DMA buf fd. Otherwise, the memory would never be freed
    // since it takes one extra refcount.
    if (m_externalHandleType == amdgpu_bo_handle_type_dma_buf_fd)
    {
        close(handle);
    }
    return result;
}

// =====================================================================================================================
// Performs OS-specific initialization for allocating peer memory objects.
// For peer memory, the external handle and type are getting from the m_pOriginalMem.
Result GpuMemory::OpenPeerMemory()
{
    // Get the external resource handle from original memory object if it is not set before.
    amdgpu_bo_handle_type handleType = static_cast<GpuMemory*>(m_pOriginalMem)->GetSharedExternalHandleType();

    Pal::GpuMemoryExportInfo handleInfo = {};
    OsExternalHandle handle = static_cast<GpuMemory*>(m_pOriginalMem)->ExportExternalHandle(handleInfo);

    PAL_ASSERT(handle);

    Result result = ImportMemory(handleType, handle);

    // handle should be closed here otherwise, the memory would never be freed since it takes one extra refcount.
    close(handle);

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
OsExternalHandle GpuMemory::ExportExternalHandle(
    const GpuMemoryExportInfo& exportInfo
    ) const
{
    // According to Vulkan spec, the vkGetMemoryFdKHX requires a new fd for each call, and it is application's
    // responsibility to close the fd.
    // Driver does not need to maintain the share fd anymore.
    // another valid use case for this is to share image to XServer as pixmap.
    OsExternalHandle fd;
    {
        amdgpu_bo_handle_type type = m_externalHandleType;
        switch (exportInfo.exportType)
        {
            case ExportHandleType::FileDescriptor:
                type = amdgpu_bo_handle_type_dma_buf_fd;
                break;
            case ExportHandleType::Kms:
                type = amdgpu_bo_handle_type_kms;
                break;
            default:
                type = m_externalHandleType;
                break;
        }

        Device* const pDevice = static_cast<Device*>(m_pDevice);

        Result result = pDevice->ExportBuffer(m_hSurface,
                                              type,
                                              reinterpret_cast<uint32*>(&fd));

        if ((result == Result::Success) && (m_amdgpuFlags.isShared == false))
        {
            pDevice->UpdateMetaDataUniqueId(this);

            m_amdgpuFlags.isShared = pDevice->AddToSharedBoMap(m_hSurface, m_hVaRange, m_desc.gpuVirtAddr);
        }
    }

    return fd;
}

// =====================================================================================================================
// Maps the allocation into CPU address space.
Result GpuMemory::OsMap(
    void** ppData)
{
    Amdgpu::Device*  pDevice = nullptr;
    amdgpu_bo_handle hSurface = nullptr;

    if (m_pOriginalMem == nullptr)
    {
        pDevice = static_cast<Amdgpu::Device*>(m_pDevice);
        hSurface = m_hSurface;
    }
    else
    {
        // It is the case that memory shared cross devices but in one same process,
        // On linux, each struct page has a mapping field pointing to the struct
        // address_space it originates from for reverse mapping purposes.
        // We use one address_space structure for each device we manage and this
        // address_space in turn is referenced by the file descriptor you use for
        // the mmap() call.
        // Now when those two doesn't match the kernel can't figure out where a
        // page is mapped when the reverse mapping is needed. E.g. just a cat
        // /proc/self/mem could crash really badly.
        // The problem is that a file descriptor can only point to one address
        // space. In other words the file descriptor which has imported an DMA-buf
        // handle obviously points to the importer and not the exporter.
        // There is no way around that as far as I know. We either need to use the
        // file descriptor of the exporting device or the DMA-buf file descriptor
        // for the mmap().

        pDevice  = static_cast<Amdgpu::Device*>(m_pOriginalMem->GetDevice());
        hSurface = static_cast<Amdgpu::GpuMemory*>(m_pOriginalMem)->m_hSurface;
    }

    return pDevice->Map(hSurface, ppData);
}

// =====================================================================================================================
// Unmaps the allocation out of CPU address space.
Result GpuMemory::OsUnmap()
{
    Amdgpu::Device*  pDevice = nullptr;
    amdgpu_bo_handle hSurface = nullptr;

    if (m_pOriginalMem == nullptr)
    {
        pDevice  = static_cast<Device*>(m_pDevice);
        hSurface = m_hSurface;
    }
    else
    {
        // same as OsMap
        pDevice  = static_cast<Amdgpu::Device*>(m_pOriginalMem->GetDevice());
        hSurface = static_cast<Amdgpu::GpuMemory*>(m_pOriginalMem)->m_hSurface;
    }

    return pDevice->Unmap(hSurface);
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
        Amdgpu::Device*const pDevice = static_cast<Amdgpu::Device*>(m_pDevice);

        uint64 busAddress = 0;
        result = pDevice->QuerySdiSurface(m_hSurface, &busAddress);
        if (result == Result::Success)
        {
            m_desc.surfaceBusAddr = busAddress;
            const gpusize pageSize = m_pDevice->MemoryProperties().virtualMemPageSize;
            m_desc.markerBusAddr = busAddress + m_desc.size - pageSize;

            const gpusize markerVa = m_desc.gpuVirtAddr + m_desc.markerBusAddr - m_desc.surfaceBusAddr;
            SetBusAddrMarkerVa(markerVa);
        }
    }

    return result;
}

// =====================================================================================================================
Result GpuMemory::SetSdiRemoteBusAddress(
    gpusize surfaceBusAddr,
    gpusize markerBusAddr)
{
    Result result = Result::Success;

    if (IsExternPhys() && (m_desc.gpuVirtAddr == 0))
    {
        if ((surfaceBusAddr != 0) || (markerBusAddr != 0))
        {
            Device* pDevice = static_cast<Device*>(m_pDevice);
            SetSurfaceBusAddr(surfaceBusAddr);
            SetMarkerBusAddr(markerBusAddr);
            result = pDevice->SetSdiSurface(this, &(m_desc.gpuVirtAddr));
        }
        else
        {
            result = Result::ErrorInvalidValue;
        }
    }
    else
    {
        result = Result::ErrorUnavailable;
    }

    return result;
}

} // Amdgpu
} // Pal
