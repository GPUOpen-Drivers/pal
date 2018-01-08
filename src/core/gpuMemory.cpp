/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/gpuMemory.h"
#include "core/image.h"
#include "core/platform.h"
#include "palDeveloperHooks.h"
#include "palSysMemory.h"
#include "palFormatInfo.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
Result GpuMemory::ValidateCreateInfo(
    const Device*              pDevice,
    const GpuMemoryCreateInfo& createInfo)
{
    Result      result   = Result::Success;
    const auto& memProps = pDevice->MemoryProperties();

    if ((memProps.flags.multipleVaRangeSupport == 0) &&
        (createInfo.vaRange != VaRange::Default)     &&
        (createInfo.vaRange != VaRange::DescriptorTable)) // We map DescriptorTable to Default on low-VA space configs
    {
        result = Result::ErrorOutOfGpuMemory;
    }
    if (createInfo.flags.useReservedGpuVa)
    {
        if (createInfo.pReservedGpuVaOwner == nullptr)
        {
            result = Result::ErrorInvalidPointer;
        }
        else
        {
            const gpusize fragmentSize  = pDevice->MemoryProperties().fragmentSize;
            const gpusize alignment     = Pow2Align(createInfo.alignment, fragmentSize);
            const GpuMemory* const pObj = static_cast<const GpuMemory* const>(createInfo.pReservedGpuVaOwner);
            const GpuMemoryDesc& desc   = pObj->Desc();

            if ((desc.gpuVirtAddr != Pow2Align(desc.gpuVirtAddr, alignment)) ||
                (desc.alignment != createInfo.alignment) ||
                (desc.size < createInfo.size) ||
                (pObj->m_vaRange != createInfo.vaRange))
            {
                result = Result::ErrorInvalidValue;
            }
        }
    }

    if (createInfo.flags.typedBuffer)
    {
        if (Formats::IsUndefined(createInfo.typedBufferInfo.swizzledFormat.format))
        {
            result = Result::ErrorInvalidFormat;
        }
        else if ((createInfo.typedBufferInfo.extent.width  == 0) ||
                 (createInfo.typedBufferInfo.extent.height == 0) ||
                 (createInfo.typedBufferInfo.extent.depth  == 0) ||
                 (createInfo.typedBufferInfo.rowPitch      == 0) ||
                 (createInfo.typedBufferInfo.depthPitch    == 0))
        {
            result = Result::ErrorInvalidValue;
        }
    }

    if ((result == Result::Success) && (createInfo.size == 0))
    {
        // Cannot create an allocation of size 0!
        result = Result::ErrorInvalidMemorySize;
    }

    // If this is real GPU memory allocation, we need to know if it must reside in a non-local heap.
    bool nonLocalOnly = true;

    if (result == Result::Success)
    {
        if (createInfo.flags.virtualAlloc == false)
        {
            if (createInfo.heapCount == 0)
            {
                // Physical GPU memory allocations must specify at least one heap!
                result = Result::ErrorInvalidValue;
            }
            else
            {
                for (uint32 idx = 0; idx < createInfo.heapCount; ++idx)
                {
                    if ((createInfo.heaps[idx] == GpuHeapLocal) || (createInfo.heaps[idx] == GpuHeapInvisible))
                    {
                        nonLocalOnly = false;
                        break;
                    }
                }
            }
        }
        else if (createInfo.heapCount != 0)
        {
            // Virtual GPU memory allocations cannot specify any heaps!
            result = Result::ErrorInvalidValue;
        }
    }

    const gpusize allocGranularity = createInfo.flags.virtualAlloc ? memProps.virtualMemAllocGranularity
                                                                   : memProps.realMemAllocGranularity;

    if ((result == Result::Success) && ((createInfo.alignment % allocGranularity) != 0))
    {
        // Requested alignment must be zero or a multiple of the relevant allocation granularity!
        result = Result::ErrorInvalidAlignment;
    }

    if ((result == Result::Success) && ((createInfo.size % allocGranularity) != 0))
    {
        // The requested allocation size doesn't match the allocation granularity requirements!
        result = Result::ErrorInvalidMemorySize;
    }

    if ((result == Result::Success) && createInfo.flags.shareable && (nonLocalOnly == false))
    {
        // Shareable allocations must reside only in non-local heaps in order for multiple GPU's to access them
        // simultaneously without problems!
        result = Result::ErrorInvalidFlags;
    }

    if ((result == Result::Success) && createInfo.flags.globalGpuVa && (memProps.flags.globalGpuVaSupport == 0))
    {
        // The globalGpuVa flag can't be set if the feature isn't supported!
        result = Result::ErrorInvalidFlags;
    }

    if ((result == Result::Success) && (createInfo.vaRange == Pal::VaRange::Svm) &&
        ((memProps.flags.svmSupport == 0) || (pDevice->GetPlatform()->SvmModeEnabled() == false)))
    {
        // The SVM range can't be used if the feature isn't supported!
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success) && createInfo.flags.autoPriority && (memProps.flags.autoPrioritySupport == 0))
    {
        // The autoPriority flag can't be set if the feature isn't supported!
        result = Result::ErrorInvalidFlags;
    }

    if (result == Result::Success)
    {
        if (createInfo.vaRange == VaRange::ShadowDescriptorTable)
        {
            const gpusize alignment      = Max(createInfo.alignment, allocGranularity);
            gpusize       descrStartAddr = 0;
            gpusize       descrEndAddr   = 0;
            pDevice->VirtualAddressRange(VaPartition::DescriptorTable, &descrStartAddr,  &descrEndAddr);

            // The descriptor GPU VA must meet the address alignment and fit in the DescriptorTable range.
            if (((createInfo.descrVirtAddr % alignment) != 0) ||
                (createInfo.descrVirtAddr < descrStartAddr)   ||
                (createInfo.descrVirtAddr >= descrEndAddr))
            {
                result = Result::ErrorInvalidValue;
            }
        }
        else if ((createInfo.descrVirtAddr != 0) && (createInfo.flags.useReservedGpuVa == false))
        {
            // The "descrVirtAddr" field is only used for the ShadowDescriptorTable VA range.
            result = Result::ErrorInvalidValue;
        }
    }

    return result;
}

// =====================================================================================================================
Result GpuMemory::ValidatePinInfo(
    const Device*                    pDevice,
    const PinnedGpuMemoryCreateInfo& createInfo)
{
    Result        result    = Result::ErrorInvalidPointer;
    const gpusize alignment = pDevice->MemoryProperties().realMemAllocGranularity;

    if (IsPow2Aligned(reinterpret_cast<gpusize>(createInfo.pSysMem), alignment))
    {
        result = IsPow2Aligned(createInfo.size, alignment) ? Result::Success : Result::ErrorInvalidMemorySize;
    }

    return result;
}

// =====================================================================================================================
Result GpuMemory::ValidateSvmInfo(
    const Device*                 pDevice,
    const SvmGpuMemoryCreateInfo& createInfo)
{
    Result        result    = Result::ErrorInvalidPointer;
    const gpusize alignment = pDevice->MemoryProperties().realMemAllocGranularity;

    if (IsPow2Aligned(createInfo.alignment, alignment))
    {
        result = IsPow2Aligned(createInfo.size, alignment) ? Result::Success : Result::ErrorInvalidAlignment;
    }

    return result;
}

// =====================================================================================================================
Result GpuMemory::ValidateOpenInfo(
    const Device*            pDevice,
    const GpuMemoryOpenInfo& openInfo)
{
    Result           result       = Result::Success;
    const GpuMemory* pOriginalMem = static_cast<GpuMemory*>(openInfo.pSharedMem);

    if (openInfo.pSharedMem == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (pOriginalMem->IsShareable() == false)
    {
        result = Result::ErrorNotShareable;
    }

    return result;
}

// =====================================================================================================================
Result GpuMemory::ValidatePeerOpenInfo(
    const Device*                pDevice,
    const PeerGpuMemoryOpenInfo& peerInfo)
{
    Result result = Result::Success;

    if (peerInfo.pOriginalMem == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }

    return result;
}

// =====================================================================================================================
GpuMemory::GpuMemory(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_vaRange(VaRange::Default),
    m_heapCount(0),
    m_priority(GpuMemPriority::Unused),
    m_priorityOffset(GpuMemPriorityOffset::Offset0),
    m_pImage(nullptr),
    m_hExternalResource(0),
    m_mtype(MType::Default),
    m_remoteSdiSurfaceIndex(0),
    m_remoteSdiMarkerIndex(0)
{
    memset(&m_desc, 0, sizeof(m_desc));
    memset(&m_heaps[0], 0, sizeof(m_heaps));
    memset(&m_typedBufferInfo, 0, sizeof(m_typedBufferInfo));

    m_flags.u32All   = 0;
    m_pPinnedMemory  = nullptr;
    m_pOriginalMem   = nullptr;
}

// =====================================================================================================================
GpuMemory::~GpuMemory()
{
    Developer::GpuMemoryData data = {};
    data.size                     = m_desc.size;
    data.heap                     = m_heaps[0];
    data.flags.isClient           = IsClient();
    data.flags.isFlippable        = IsFlippable();
    data.flags.isUdmaBuffer       = IsUdmaBuffer();
    data.flags.isCmdAllocator     = IsCmdAllocator();
    data.flags.isVirtual          = IsVirtual();
    m_pDevice->DeveloperCb(Developer::CallbackType::FreeGpuMemory, &data);
}

// =====================================================================================================================
// Initializes GPU memory objects that are build from create info structs. This includes:
// - Real GPU memory allocations owned by the local process.
// - Virtual GPU memory allocations owned by the local process.
// - External, shared GPU memory objects that point to GPU memory allocations owned by an external process.
Result GpuMemory::Init(
    const GpuMemoryCreateInfo&         createInfo,
    const GpuMemoryInternalCreateInfo& internalInfo)
{
    m_desc.flags.isVirtual     = createInfo.flags.virtualAlloc || createInfo.flags.sdiExternal;
    m_desc.flags.isExternPhys  = createInfo.flags.sdiExternal;
    m_desc.flags.isExternal    = internalInfo.flags.isExternal;
    m_desc.flags.isShared      = internalInfo.flags.isExternal; // External memory is memory shared between processes.

    m_flags.isShareable        = createInfo.flags.shareable;
    m_flags.isFlippable        = createInfo.flags.flippable;
    m_flags.interprocess       = createInfo.flags.interprocess;
    m_flags.globallyCoherent   = createInfo.flags.globallyCoherent;
    m_flags.xdmaBuffer         = createInfo.flags.xdmaBuffer || internalInfo.flags.xdmaBuffer;
    m_flags.globalGpuVa        = createInfo.flags.globalGpuVa;
    m_flags.useReservedGpuVa   = createInfo.flags.useReservedGpuVa;
    m_flags.typedBuffer        = createInfo.flags.typedBuffer;
    m_flags.turboSyncSurface   = createInfo.flags.turboSyncSurface;
    m_flags.busAddressable     = createInfo.flags.busAddressable;
    m_flags.isStereo           = createInfo.flags.stereo;
    m_flags.autoPriority       = createInfo.flags.autoPriority;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 312
    m_flags.peerWritable       = createInfo.flags.peerWritable;
#endif
    m_flags.isClient           = internalInfo.flags.isClient;
    m_flags.pageDirectory      = internalInfo.flags.pageDirectory;
    m_flags.pageTableBlock     = internalInfo.flags.pageTableBlock;
    m_flags.udmaBuffer         = internalInfo.flags.udmaBuffer;
    m_flags.unmapInfoBuffer    = internalInfo.flags.unmapInfoBuffer;
    m_flags.historyBuffer      = internalInfo.flags.historyBuffer;
    m_flags.isCmdAllocator     = internalInfo.flags.isCmdAllocator;
    m_flags.buddyAllocated     = internalInfo.flags.buddyAllocated;
    m_flags.privateScreen      = internalInfo.flags.privateScreen;
    m_flags.isUserQueue        = internalInfo.flags.userQueue;
    m_flags.isTimestamp        = internalInfo.flags.timestamp;
    m_flags.accessedPhysically = internalInfo.flags.accessedPhysically;

    if (!IsClient())
    {
        m_flags.autoPriority = m_pDevice->IsUsingAutoPriorityForInternalAllocations();
    }

    if (IsTypedBuffer())
    {
        memcpy(&m_typedBufferInfo, &(createInfo.typedBufferInfo), sizeof(TypedBufferCreateInfo));
    }

    // In general, private driver resources are expected to be always resident. The app and/or client is expected to
    // manage residency for anything that doesn't set this flag, including:
    // - Resources allocated using CreateGpuMemory().
    // - Presentable images.
    // - Private screens.
    // - Peer memory and images.
    // - Shared memory and images.
    // - External, shared memory and images.
    // - setting has enabled always resident by default
    m_flags.alwaysResident = m_pDevice->Settings().alwaysResident || internalInfo.flags.alwaysResident;

    // Asking for the paging fence value returned by the OS is pointless if the allocation is not marked as
    // always resident.
    PAL_ALERT((IsAlwaysResident() == false) && (internalInfo.pPagingFence != nullptr));

    m_desc.size      = createInfo.size;
    m_desc.alignment = createInfo.alignment;
    m_vaRange        = createInfo.vaRange;
    m_priority       = createInfo.priority;
    m_priorityOffset = createInfo.priorityOffset;
    m_heapCount      = createInfo.heapCount;
    m_pImage         = static_cast<Image*>(createInfo.pImage);
    m_schedulerId    = internalInfo.schedulerId;
    m_mtype          = internalInfo.mtype;

    // The number of reserved compute units for a real-time queue
    m_numReservedCu = internalInfo.numReservedCu;

    if (IsBusAddressable())
    {
        // one extra page for marker
        const gpusize pageSize = m_pDevice->MemoryProperties().virtualMemPageSize;
        m_desc.size = Pow2Align(m_desc.size, pageSize) + pageSize;
    }

    // the handle is used for importing resource
    m_hExternalResource = internalInfo.hExternalResource;

    gpusize allocGranularity = 0;

    if (IsVirtual())
    {
        allocGranularity = m_pDevice->MemoryProperties().virtualMemAllocGranularity;
    }
    else
    {
        allocGranularity = m_pDevice->MemoryProperties().realMemAllocGranularity;

        // NOTE: Assume that the heap selection is both local-only and nonlocal-only temporarily. When we scan the
        // heap selections below, this paradoxical assumption will be corrected.
        m_flags.localOnly    = 1;
        m_flags.nonLocalOnly = 1;
        // NOTE: Any memory object not being used as a page-directory or page-table block is considered to be CPU
        // visible as long as all of its selected heaps are CPU visible.
        m_flags.cpuVisible   = ((m_flags.pageDirectory == 0) && (m_flags.pageTableBlock == 0));

        for (uint32 heap = 0; heap < m_heapCount; ++heap)
        {
            m_heaps[heap] = createInfo.heaps[heap];
            const GpuMemoryHeapProperties& heapProps = m_pDevice->HeapProperties(m_heaps[heap]);

            if (heapProps.flags.cpuVisible == 0)
            {
                m_flags.cpuVisible = 0;
            }

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
        }
        // we cannot fire the assert if m_heapCount is 0
        PAL_ASSERT(((m_flags.nonLocalOnly == 0) || (m_flags.localOnly == 0)) || (m_heapCount == 0));
    }

    m_desc.preferredHeap = m_heaps[0];

    // Requested alignment must be a multiple of the relevant allocation granularity! If no alignment value was
    // provided, use the allocation granularity.
    if (m_desc.alignment == 0)
    {
        m_desc.alignment = allocGranularity;
    }
    else
    {
        // The caller provided their own alignment value, make sure it's a multiple of the allocation granularity.
        PAL_ASSERT(IsPowerOfTwo(allocGranularity));
        PAL_ASSERT(IsPow2Aligned(m_desc.alignment, allocGranularity));
    }

    Result result = Result::Success;

    if (IsShared())
    {
        result = OpenSharedMemory();

        if (IsErrorResult(result) == false)
        {
            DescribeGpuMemory(Developer::GpuMemoryAllocationMethod::Opened);
        }
    }
    else
    {
        gpusize baseVirtAddr = internalInfo.baseVirtAddr;

        if (createInfo.flags.useReservedGpuVa && (createInfo.pReservedGpuVaOwner != nullptr))
        {
            const GpuMemoryDesc& desc = createInfo.pReservedGpuVaOwner->Desc();

            // It's illegal for the internal path to specify non-zero base VA when client already does.
            PAL_ASSERT(internalInfo.baseVirtAddr == 0);
            // Do not expect client set "useReservedGpuVa" for ShadowDescriptorTable case
            PAL_ASSERT(m_vaRange != VaRange::ShadowDescriptorTable);

            baseVirtAddr = desc.gpuVirtAddr;
        }

        if (m_vaRange == VaRange::ShadowDescriptorTable)
        {
            // It's illegal for the internal path to use this VA range.
            PAL_ASSERT(IsClient());

            gpusize descrStartAddr  = 0;
            gpusize descrEndAddr    = 0;
            gpusize shadowStartAddr = 0;
            gpusize shadowEndAddr   = 0;
            m_pDevice->VirtualAddressRange(VaPartition::DescriptorTable,       &descrStartAddr,  &descrEndAddr);
            m_pDevice->VirtualAddressRange(VaPartition::ShadowDescriptorTable, &shadowStartAddr, &shadowEndAddr);

            // The descriptor GPU VA must meet the address alignment and fit in the DescriptorTable range.
            PAL_ASSERT(((createInfo.descrVirtAddr % m_desc.alignment) == 0) &&
                       (createInfo.descrVirtAddr >= descrStartAddr)         &&
                       (createInfo.descrVirtAddr < descrEndAddr));

            baseVirtAddr = shadowStartAddr + (createInfo.descrVirtAddr - descrStartAddr);
        }
        else if ((createInfo.vaRange == VaRange::Svm) && (m_pDevice->MemoryProperties().flags.iommuv2Support == 0))
        {
            result = AllocateSvmVirtualAddress(baseVirtAddr, createInfo.size, createInfo.alignment, false);
            baseVirtAddr = m_desc.gpuVirtAddr;
        }
        else if (createInfo.vaRange == VaRange::Default)
        {
            // For performance reasons we may wish to force our GPU memory allocations' addresses and sizes to be
            // either fragment-aligned or aligned to KMD's reported optimized large page size.  This should be
            // skipped if any of the following are true:
            // - We're not using the default VA range because non-default VA ranges have special address usage rules.
            // - We have selected a specific base VA for the allocation because it might not be 64KB aligned.
            // - The allocation prefers a non-local heap because we can only get 64KB fragments in local memory.
            // - Type is SDI ExternalPhysical because it has no real allocation and size must be consistent with KMD.
            if ((baseVirtAddr == 0)                                                                    &&
                ((m_desc.preferredHeap == GpuHeapLocal) || (m_desc.preferredHeap == GpuHeapInvisible)) &&
                (createInfo.flags.sdiExternal == 0))
            {
#if !defined(PAL_BUILD_BRANCH) || (PAL_BUILD_BRANCH >= 1740)
                if (m_desc.size >= m_pDevice->MemoryProperties().largePageSupport.minSurfaceSizeForAlignmentInBytes)
                {
                    const gpusize largePageSize = m_pDevice->MemoryProperties().largePageSupport.largePageSizeInBytes;

                    if (m_pDevice->MemoryProperties().largePageSupport.sizeAlignmentNeeded)
                    {
                        m_desc.size = Pow2Align(m_desc.size, largePageSize);
                    }

                    if (m_pDevice->MemoryProperties().largePageSupport.gpuVaAlignmentNeeded)
                    {
                        m_desc.alignment = Pow2Align(m_desc.alignment, largePageSize);
                    }
                }
#endif
            }
        }

        if (result == Result::Success && (Desc().flags.isExternPhys == false))
        {
            result = AllocateOrPinMemory(baseVirtAddr, internalInfo.pPagingFence, createInfo.virtualAccessMode);
        }

        if (IsErrorResult(result) == false)
        {
            DescribeGpuMemory(Developer::GpuMemoryAllocationMethod::Normal);
        }
    }

    // Verify that if the allocation succeeded, we got a GPU virtual address back as expected (except for
    // page directory and page table allocations and SDI External Physical Memory).
    if ((IsPageDirectory() == false) && (IsPageTableBlock() == false) && (Desc().flags.isExternPhys == false))
    {
        PAL_ASSERT((result != Result::Success) || (m_desc.gpuVirtAddr != 0));
    }

    return result;
}

// =====================================================================================================================
// Initializes this GPU memory object as a SVM memory allocation.
Result GpuMemory::Init(
    const SvmGpuMemoryCreateInfo& createInfo)
{
    Result result = Result::Success;

    m_flags.isPinned     = 1;
    m_flags.nonLocalOnly = 1; // Pinned allocations always go into a non-local heap.
    m_flags.cpuVisible   = 1; // Pinned allocations are by definition CPU visible.

    m_flags.useReservedGpuVa = createInfo.flags.useReservedGpuVa;

    m_desc.size      = createInfo.size;
    m_desc.alignment = createInfo.alignment;

    m_vaRange        = Pal::VaRange::Svm;
    gpusize baseVirtAddr = 0;

    if (IsGpuVaPreReserved())
    {
        baseVirtAddr = createInfo.pReservedGpuVaOwner->Desc().gpuVirtAddr;
    }

    if (m_pDevice->MemoryProperties().flags.iommuv2Support)
    {
        m_desc.flags.isSvmAlloc = 1;
        if (createInfo.isUsedForKernel)
        {
            m_desc.flags.isExecutable = 1;
        }
    }
    else
    {
        result = AllocateSvmVirtualAddress(baseVirtAddr, createInfo.size, createInfo.alignment, true);
    }
    if (result == Result::Success)
    {
        // Scan the list of available GPU heaps to determine which heap(s) this pinned allocation will end up in.
        for (uint32 idx = 0; idx < GpuHeapCount; ++idx)
        {
            const GpuHeap heap = static_cast<GpuHeap>(idx);

            if (m_pDevice->HeapProperties(heap).flags.holdsPinned != 0)
            {
                m_heaps[m_heapCount++] = heap;
            }
        }

        m_desc.preferredHeap = m_heaps[0];
        result               = AllocateOrPinMemory(m_desc.gpuVirtAddr, nullptr, VirtualGpuMemAccessMode::Undefined);
        m_pPinnedMemory      = reinterpret_cast<const void*>(m_desc.gpuVirtAddr);
    }

    PAL_ASSERT((result != Result::Success) || (m_desc.gpuVirtAddr != 0));

    if (IsErrorResult(result) == false)
    {
        DescribeGpuMemory(Developer::GpuMemoryAllocationMethod::Svm);
    }

    return result;
}

// =====================================================================================================================
// Initializes this GPU memory object as a pinned (GPU-accessible) system-memory allocation.
Result GpuMemory::Init(
    const PinnedGpuMemoryCreateInfo& createInfo)
{
    m_flags.isPinned     = 1;
    m_flags.nonLocalOnly = 1; // Pinned allocations always go into a non-local heap.
    m_flags.cpuVisible   = 1; // Pinned allocations are by definition CPU visible.

    m_pPinnedMemory  = createInfo.pSysMem;
    m_desc.size      = createInfo.size;
    m_desc.alignment = m_pDevice->MemoryProperties().realMemAllocGranularity;
    m_vaRange        = createInfo.vaRange;

    // Scan the list of available GPU heaps to determine which heap(s) this pinned allocation will end up in.
    for (uint32 idx = 0; idx < GpuHeapCount; ++idx)
    {
        const GpuHeap heap = static_cast<GpuHeap>(idx);

        if (m_pDevice->HeapProperties(heap).flags.holdsPinned != 0)
        {
            m_heaps[m_heapCount++] = heap;
        }
    }

    m_desc.preferredHeap = m_heaps[0];

    const Result result = AllocateOrPinMemory(0, nullptr, VirtualGpuMemAccessMode::Undefined);

    // Verify that if the pinning succeeded, we got a GPU virtual address back as expected.
    PAL_ASSERT((result != Result::Success) || (m_desc.gpuVirtAddr != 0));

    if (IsErrorResult(result) == false)
    {
        DescribeGpuMemory(Developer::GpuMemoryAllocationMethod::Pinned);
    }

    return result;
}

// =====================================================================================================================
// Initializes this GPU memory object as a share of the other memory object specified in openInfo.pSharedMem.
// The shared memory must be owned by the local process, external shared memory uses a different init path.
Result GpuMemory::Init(
    const GpuMemoryOpenInfo& openInfo)
{
    m_pOriginalMem   = static_cast<GpuMemory*>(openInfo.pSharedMem);
    m_desc.size      = m_pOriginalMem->m_desc.size;
    m_desc.alignment = m_pOriginalMem->m_desc.alignment;
    m_vaRange        = m_pOriginalMem->m_vaRange;
    m_mtype          = m_pOriginalMem->m_mtype;
    m_heapCount      = m_pOriginalMem->m_heapCount;

    for (uint32 i = 0; i < m_heapCount; ++i)
    {
        m_heaps[i] = m_pOriginalMem->m_heaps[i];
    }

    m_desc.preferredHeap  = m_heaps[0];
    m_desc.flags.isShared = 1;
    m_flags.isShareable   = m_pOriginalMem->m_flags.isShareable;
    m_flags.isFlippable   = m_pOriginalMem->m_flags.isFlippable;
    m_flags.isStereo      = m_pOriginalMem->m_flags.isStereo;
    m_flags.localOnly     = m_pOriginalMem->m_flags.localOnly;
    m_flags.nonLocalOnly  = m_pOriginalMem->m_flags.nonLocalOnly;
    m_flags.interprocess  = m_pOriginalMem->m_flags.interprocess;
    m_flags.globalGpuVa   = m_pOriginalMem->m_flags.globalGpuVa;

    // Set the gpuVirtAddr if the GPU VA is visible to all devices
    if (IsGlobalGpuVa())
    {
        m_desc.gpuVirtAddr = m_pOriginalMem->m_desc.gpuVirtAddr;
    }

    // NOTE: The following flags are not expected to be set for shared memory objects!
    PAL_ASSERT((m_pOriginalMem->m_desc.flags.isVirtual == 0) &&
               (m_pOriginalMem->m_desc.flags.isPeer    == 0) &&
               (m_pOriginalMem->m_flags.isPinned       == 0) &&
               (m_pOriginalMem->m_flags.pageDirectory  == 0) &&
               (m_pOriginalMem->m_flags.pageTableBlock == 0) &&
               (m_pOriginalMem->m_flags.isCmdAllocator == 0) &&
               (m_pOriginalMem->m_flags.udmaBuffer     == 0) &&
               (m_pOriginalMem->m_flags.historyBuffer  == 0) &&
               (m_pOriginalMem->m_flags.xdmaBuffer     == 0) &&
               (m_pOriginalMem->m_flags.buddyAllocated == 0) &&
               (m_pOriginalMem->m_flags.alwaysResident == 0));

    const Result result = OpenSharedMemory();

    if (IsErrorResult(result) == false)
    {
        DescribeGpuMemory(Developer::GpuMemoryAllocationMethod::Opened);
    }

    // Verify that if opening the peer memory connection succeeded, we got a GPU virtual address back as expected.
    PAL_ASSERT((result != Result::Success) || (m_desc.gpuVirtAddr != 0));

    return result;
}

// =====================================================================================================================
// Initializes this GPU memory object as a peer of the other memory object specified in peerInfo.pOriginalMem.
Result GpuMemory::Init(
    const PeerGpuMemoryOpenInfo& peerInfo)
{
    m_pOriginalMem   = static_cast<GpuMemory*>(peerInfo.pOriginalMem);
    m_desc.size      = m_pOriginalMem->m_desc.size;
    m_desc.alignment = m_pOriginalMem->m_desc.alignment;
    m_vaRange        = m_pOriginalMem->m_vaRange;
    m_mtype          = m_pOriginalMem->m_mtype;
    m_heapCount      = m_pOriginalMem->m_heapCount;

    for (uint32 i = 0; i < m_heapCount; ++i)
    {
        m_heaps[i] = m_pOriginalMem->m_heaps[i];
    }

    m_desc.preferredHeap  = m_heaps[0];
    m_desc.flags.isPeer   = 1;
    m_flags.isShareable   = m_pOriginalMem->m_flags.isShareable;
    m_flags.isFlippable   = m_pOriginalMem->m_flags.isFlippable;
    m_flags.isStereo      = m_pOriginalMem->m_flags.isStereo;
    m_flags.localOnly     = m_pOriginalMem->m_flags.localOnly;
    m_flags.nonLocalOnly  = m_pOriginalMem->m_flags.nonLocalOnly;
    m_flags.interprocess  = m_pOriginalMem->m_flags.interprocess;
    m_flags.globalGpuVa   = m_pOriginalMem->m_flags.globalGpuVa;
    m_flags.cpuVisible    = m_pOriginalMem->m_flags.cpuVisible;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 312
    m_flags.peerWritable  = m_pOriginalMem->m_flags.peerWritable;
    PAL_ASSERT(m_flags.peerWritable == 1);
#endif

    // Set the gpuVirtAddr if the GPU VA is visible to all devices
    if (IsGlobalGpuVa())
    {
        m_desc.gpuVirtAddr = m_pOriginalMem->m_desc.gpuVirtAddr;
    }

    // NOTE: The following flags are not expected to be set for peer memory objects!
    PAL_ASSERT((m_pOriginalMem->m_desc.flags.isVirtual == 0) &&
               (m_pOriginalMem->m_desc.flags.isShared  == 0) &&
               (m_pOriginalMem->m_flags.isPinned       == 0) &&
               (m_pOriginalMem->m_flags.pageDirectory  == 0) &&
               (m_pOriginalMem->m_flags.pageTableBlock == 0) &&
               (m_pOriginalMem->m_flags.isCmdAllocator == 0) &&
               (m_pOriginalMem->m_flags.udmaBuffer     == 0) &&
               (m_pOriginalMem->m_flags.historyBuffer  == 0) &&
               (m_pOriginalMem->m_flags.xdmaBuffer     == 0) &&
               (m_pOriginalMem->m_flags.buddyAllocated == 0));

    const Result result = OpenPeerMemory();

    if (IsErrorResult(result) == false)
    {
        DescribeGpuMemory(Developer::GpuMemoryAllocationMethod::Peer);
    }

    // Verify that if opening the peer memory connection succeeded, we got a GPU virtual address back as expected.
    PAL_ASSERT((result != Result::Success) || (m_desc.gpuVirtAddr != 0));

    return result;
}

// =====================================================================================================================
// Destroys an internal GPU memory object: invokes the destructor and frees the system memory block it resides in.
void GpuMemory::DestroyInternal()
{
    Platform* pPlatform = m_pDevice->GetPlatform();
    Destroy();
    PAL_FREE(this, pPlatform);
}

// =====================================================================================================================
// Set mapppedToPeerMemory flag for virtual GPU memory when mapped to peer real memory.
void GpuMemory::SetMapDestPeerMem(GpuMemory* pMapDestPeerMem)
{
    // The p2p workaround only supports one mapping per virtual GPU memory object.
    PAL_ASSERT((pMapDestPeerMem->IsPeer() && (m_pMapDestPeerMem == nullptr)));
    m_pMapDestPeerMem = pMapDestPeerMem;
    m_flags.mapppedToPeerMemory = 1;
}

// =====================================================================================================================
// Changes the allocation's priority. This is only supported for "real" allocations.
Result GpuMemory::SetPriority(
    GpuMemPriority       priority,
    GpuMemPriorityOffset priorityOffset)
{
    Result result = Result::ErrorUnavailable;

    if ((IsPinned() == false)       &&
        (IsVirtual() == false)      &&
        (IsPeer() == false)         &&
        (IsAutoPriority() == false) &&
        (IsShared() == false))
    {
        // Save off the new priority information.
        m_priority       = priority;
        m_priorityOffset = priorityOffset;

        // Call the OS to change the priority of the GpuMemory allocation.
        result = OsSetPriority(priority, priorityOffset);
    }

    return result;
}

// =====================================================================================================================
// Maps the GPU memory allocation into CPU address space.
Result GpuMemory::Map(
    void** ppData)
{
    Result result = Result::ErrorInvalidPointer;

    if (ppData != nullptr)
    {
        if (IsPinned())
        {
            PAL_ASSERT(m_pPinnedMemory != nullptr);
            (*ppData) = const_cast<void*>(m_pPinnedMemory);

            result = Result::Success;
        }
        else if (IsVirtual())
        {
            (*ppData) = nullptr;
            result = Result::ErrorUnavailable;
        }
        else if (IsCpuVisible())
        {
            if (IsSvmAlloc())
            {
                (*ppData) = reinterpret_cast<void*>(m_desc.gpuVirtAddr);
                result = Result::Success;
            }
            else
            {
                result = OsMap(ppData);
            }
        }
        else
        {
            (*ppData) = nullptr;
            result = Result::ErrorNotMappable;
        }
    }

    return result;
}

// =====================================================================================================================
// Unmaps the GPU memory allocation out of CPU address space.
Result GpuMemory::Unmap()
{
    Result result = Result::ErrorNotMappable;

    if (IsPinned())
    {
        result = Result::Success;
    }
    else if (IsCpuVisible())
    {
        if (IsSvmAlloc())
        {
            result = Result::Success;
        }
        else
        {
            result = OsUnmap();
        }
    }
    else if (IsVirtual())
    {
        result = Result::ErrorUnavailable;
    }

    return result;
}

// =====================================================================================================================
// Describes the GPU memory allocation to the above layers
void GpuMemory::DescribeGpuMemory(
    Developer::GpuMemoryAllocationMethod allocMethod
    ) const
{
    Developer::GpuMemoryData data = {};
    data.size                     = m_desc.size;
    data.heap                     = m_heaps[0];
    data.flags.isClient           = IsClient();
    data.flags.isFlippable        = IsFlippable();
    data.flags.isCmdAllocator     = IsCmdAllocator();
    data.flags.isUdmaBuffer       = IsUdmaBuffer();
    data.flags.isVirtual          = IsVirtual();
    data.allocMethod              = allocMethod;
    m_pDevice->DeveloperCb(Developer::CallbackType::AllocGpuMemory, &data);
}

// =====================================================================================================================
bool GpuMemory::IsCpuVisible() const
{
    bool  isCpuVisible = (m_flags.cpuVisible != 0);

    return isCpuVisible;
}

} // Pal
