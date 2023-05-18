/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
// Copies only the valid heaps from pHeaps to pOutHeaps. The filtering happens based on if the heap exists on the
// device.
static void FilterViableHeaps(
    const GpuHeap* pHeaps,
    size_t         heapCount,
    const Device&  device,
    GpuHeap*       pOutHeaps,
    size_t*        pOutHeapCount)
{
    *pOutHeapCount = 0;
    for (uint32 heap = 0; heap < heapCount; ++heap)
    {
        if (device.HeapLogicalSize(pHeaps[heap]) > 0u)
        {
            pOutHeaps[(*pOutHeapCount)++] = pHeaps[heap];
        }
    }
}

// =====================================================================================================================
void GpuMemory::TranslateHeapInfo(
    const Device&              device,
    const GpuMemoryCreateInfo& createInfo,
    GpuHeap*                   pOutHeaps,
    size_t*                    pOutHeapCount)
{
    switch (createInfo.heapAccess)
    {
    case GpuHeapAccess::GpuHeapAccessExplicit:
        // imperative heap selection; createInfo.heaps determines the heaps
        *pOutHeapCount = createInfo.heapCount;
        for (uint32 heap = 0; heap < createInfo.heapCount; ++heap)
        {
            pOutHeaps[heap] = createInfo.heaps[heap];
        }
        break;
    case GpuHeapAccess::GpuHeapAccessCpuNoAccess:
    {
        // declarative heap selection; memory does not need to be CPU accessible
        const GpuChipProperties& chipProps = device.ChipProperties();
        switch (chipProps.gpuType)
        {
        case GpuType::Discrete:
        {
            // considering both invisible and local, in case the former is 0 (e.g., under Resizable BAR)
            constexpr GpuHeap PreferredHeaps[] = { GpuHeap::GpuHeapInvisible, GpuHeap::GpuHeapLocal };
            FilterViableHeaps(PreferredHeaps, ArrayLen32(PreferredHeaps), device, pOutHeaps, pOutHeapCount);
        }
        break;
        case GpuType::Integrated:
        {
            // integrated solutions seem to miss invisible and local
            constexpr GpuHeap PreferredHeaps[] = { GpuHeap::GpuHeapGartUswc, GpuHeap::GpuHeapGartCacheable };
            FilterViableHeaps(PreferredHeaps, ArrayLen32(PreferredHeaps), device, pOutHeaps, pOutHeapCount);
        }
        break;
        default:
            PAL_ALERT_ALWAYS_MSG("Unexpected GPU type");
            break;
        }
    }
    break;
    case GpuHeapAccess::GpuHeapAccessGpuMostly:
    {
        // declarative heap selection; optimized for GPU access
        constexpr GpuHeap PreferredHeaps[] = {
            GpuHeap::GpuHeapLocal, GpuHeap::GpuHeapGartUswc, GpuHeap::GpuHeapGartCacheable
        };
        FilterViableHeaps(PreferredHeaps, ArrayLen32(PreferredHeaps), device, pOutHeaps, pOutHeapCount);
    }
    break;
    case GpuHeapAccess::GpuHeapAccessCpuReadMostly:
    {
        // declarative heap selection; CPU will mostly do reads
        constexpr GpuHeap PreferredHeaps[] = { GpuHeap::GpuHeapGartCacheable };
        FilterViableHeaps(PreferredHeaps, ArrayLen32(PreferredHeaps), device, pOutHeaps, pOutHeapCount);
    }
    break;
    case GpuHeapAccess::GpuHeapAccessCpuWriteMostly:
    {
        // declarative heap selection; CPU will do multiple writes
        constexpr GpuHeap PreferredHeaps[] = { GpuHeap::GpuHeapGartUswc };
        FilterViableHeaps(PreferredHeaps, ArrayLen32(PreferredHeaps), device, pOutHeaps, pOutHeapCount);
    }
    break;
    case GpuHeapAccess::GpuHeapAccessCpuMostly:
    {
        // declarative heap selection; CPU will do a mix of reads and writes
        constexpr GpuHeap PreferredHeaps[] = { GpuHeap::GpuHeapGartUswc, GpuHeap::GpuHeapGartCacheable };
        FilterViableHeaps(PreferredHeaps, ArrayLen32(PreferredHeaps), device, pOutHeaps, pOutHeapCount);
    }
    break;
    default:
        PAL_ALERT_ALWAYS_MSG("Unexpected GPU heap access type");
        break;
    }
}

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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 761
    if ((createInfo.flags.startVaHintFlag == 1) && (createInfo.startVaHint != 0))
    {
        gpusize startVaHintAddr     = 0;
        gpusize endVaHintAddr       = 0;
        pDevice->VirtualAddressRange(VaPartition::Default, &startVaHintAddr, &endVaHintAddr);
        const gpusize pageSize = pDevice->MemoryProperties().virtualMemPageSize;
        const gpusize alignment = Pow2Align(createInfo.alignment, pageSize);
        const gpusize startVaHintAlign = Pow2Align(createInfo.startVaHint, alignment);

        if ((startVaHintAlign < startVaHintAddr) || ((startVaHintAlign + createInfo.size) >= endVaHintAddr))
        {
            result = Result::ErrorInvalidPointer;
        }
    }
#endif

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
                (pObj->m_vaPartition != pDevice->ChooseVaPartition(createInfo.vaRange, createInfo.flags.virtualAlloc)))
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
    else if (createInfo.pImage != nullptr)
    {
        const Pal::Image* pImage = static_cast<const Pal::Image*>(createInfo.pImage);
        if (createInfo.flags.presentable != pImage->GetImageCreateInfo().flags.presentable)
        {
            result = Result::ErrorInvalidFlags;
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
            const GpuHeap *pHeaps = nullptr;
            size_t heapCount      = 0;

            // if necessary, translate heap access information to explicit heaps
            GpuHeap implicitHeaps[GpuHeapCount] = {};
            if (createInfo.heapAccess != GpuHeapAccess::GpuHeapAccessExplicit)
            {
                TranslateHeapInfo(*pDevice, createInfo, implicitHeaps, &heapCount);
                pHeaps = implicitHeaps;
            }
            else
            {
                pHeaps    = createInfo.heaps;
                heapCount = createInfo.heapCount;
            }

            if (heapCount == 0)
            {
                // Physical GPU memory allocations must specify at least one heap!
                result = Result::ErrorInvalidValue;
            }
            else
            {
                for (uint32 idx = 0; idx < heapCount; ++idx)
                {
                    if ((pHeaps[idx] == GpuHeapLocal) || (pHeaps[idx] == GpuHeapInvisible))
                    {
                        nonLocalOnly = false;
                        break;
                    }
                }
            }
        }
        else if ((createInfo.heapAccess != GpuHeapAccessExplicit) || (createInfo.heapCount != 0))
        {
            // Virtual GPU memory allocations cannot specify any heaps!
            result = Result::ErrorInvalidValue;
        }
    }

    const gpusize allocGranularity = createInfo.flags.virtualAlloc ? memProps.virtualMemAllocGranularity
                                                                   : memProps.realMemAllocGranularity;

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
        else if ((createInfo.descrVirtAddr != 0) &&
                 (createInfo.flags.useReservedGpuVa == false) &&
                 (createInfo.vaRange != VaRange::CaptureReplay))
        {
            // The "descrVirtAddr" field is only used for the ShadowDescriptorTable VA range.
            result = Result::ErrorInvalidValue;
        }
    }

    if ((result == Result::Success) && (createInfo.flags.mallRangeActive != 0))
    {
        // Page size for specifyng a MALL range is always in units of 4kB pages
        constexpr uint32  PageSize = 4096;

        // If the mall-range is active then the mall policy must be either "always" or "never"; the KMD
        // ensures that the memory associated with this allocation outside the specified region gets the
        // "opposite" mall policy.
        if ((createInfo.mallPolicy == GpuMemMallPolicy::Default) ||
            // Ensure that the specified range fits within the size of this allocation.
            (((createInfo.mallRange.startPage + createInfo.mallRange.numPages) * PageSize) > createInfo.size))
        {
            result = Result::ErrorInvalidValue;
        }
    }

    if ((result == Result::Success) && createInfo.flags.gl2Uncached &&
        (pDevice->ChipProperties().gfxip.supportGl2Uncached == 0))
    {
        // The gl2Uncached flag can't be set if the feature isn't supported!
        result = Result::ErrorInvalidFlags;
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
    m_vaPartition(VaPartition::Default),
    m_heapCount(0),
    m_priority(GpuMemPriority::Unused),
    m_priorityOffset(GpuMemPriorityOffset::Offset0),
    m_pImage(nullptr),
    m_mtype(MType::Default),
    m_remoteSdiSurfaceIndex(0),
    m_remoteSdiMarkerIndex(0),
    m_markerVirtualAddr(0)
    ,m_mallPolicy(GpuMemMallPolicy::Default)
{
    memset(&m_desc, 0, sizeof(m_desc));
    memset(&m_heaps[0], 0, sizeof(m_heaps));
    memset(&m_typedBufferInfo, 0, sizeof(m_typedBufferInfo));

    memset(&m_mallRange, 0, sizeof(m_mallRange));

    m_flags.u64All   = 0;
    m_pPinnedMemory  = nullptr;
    m_pOriginalMem   = nullptr;
}

// =====================================================================================================================
GpuMemory::~GpuMemory()
{
    // We need to force-remove this allocation from the device's per-heap memory totals because the client might not
    // call RemoveGpuMemoryReferences once for each time they call AddGpuMemoryReferences.
    IGpuMemory*const pGpuMemory = this;
    m_pDevice->SubtractFromReferencedMemoryTotals(1, &pGpuMemory, true);

    m_pDevice->GetPlatform()->GetGpuMemoryEventProvider()->LogDestroyGpuMemoryEvent(this);

    Developer::GpuMemoryData data = {};
    data.size                     = m_desc.size;
    data.heap                     = m_heaps[0];
    data.flags.isClient           = IsClient();
    data.flags.isFlippable        = IsFlippable();
    data.flags.isUdmaBuffer       = IsUdmaBuffer();
    data.flags.isCmdAllocator     = IsCmdAllocator();
    data.flags.isVirtual          = IsVirtual();
    data.flags.isExternal         = IsExternal();
    data.flags.buddyAllocated     = WasBuddyAllocated();
    data.pGpuMemory               = this;
    m_pDevice->DeveloperCb(Developer::CallbackType::FreeGpuMemory, &data);

}

// =====================================================================================================================
// Initializes GPU memory objects that are built from create info structs. This includes:
// - Real GPU memory allocations owned by the local process.
// - Virtual GPU memory allocations owned by the local process.
// - External, shared GPU memory objects that point to GPU memory allocations owned by an external process.
Result GpuMemory::Init(
    const GpuMemoryCreateInfo&         createInfo,
    const GpuMemoryInternalCreateInfo& internalInfo)
{
    m_pImage = static_cast<Image*>(createInfo.pImage);

    // store the requested size before any alignment
    m_desc.clientSize            = createInfo.size;

    m_desc.flags.isVirtual       = createInfo.flags.virtualAlloc || createInfo.flags.sdiExternal;
    m_desc.flags.isExternPhys    = createInfo.flags.sdiExternal;
    m_desc.flags.isExternal      = internalInfo.flags.isExternal;
    m_desc.flags.isShared        = internalInfo.flags.isExternal; // External memory is memory shared between processes.

    {
        m_flags.isPresentable    = createInfo.flags.presentable;
        m_flags.isFlippable      = createInfo.flags.flippable;
        m_flags.isShareable      = createInfo.flags.shareable;
        m_flags.interprocess     = createInfo.flags.interprocess;
        m_flags.peerWritable     = createInfo.flags.peerWritable;
        m_flags.turboSyncSurface = createInfo.flags.turboSyncSurface;
    }

    m_flags.globallyCoherent     = createInfo.flags.globallyCoherent;
    m_flags.xdmaBuffer           = createInfo.flags.xdmaBuffer || internalInfo.flags.xdmaBuffer;
    m_flags.globalGpuVa          = createInfo.flags.globalGpuVa;
    m_flags.useReservedGpuVa     = createInfo.flags.useReservedGpuVa;
    m_flags.typedBuffer          = createInfo.flags.typedBuffer;
    m_flags.busAddressable       = createInfo.flags.busAddressable;
    m_flags.isStereo             = createInfo.flags.stereo;
    m_flags.autoPriority         = createInfo.flags.autoPriority;
    m_flags.restrictedContent    = createInfo.flags.restrictedContent;
    m_flags.restrictedAccess     = createInfo.flags.restrictedAccess;
    m_flags.crossAdapter         = createInfo.flags.crossAdapter;
    m_flags.tmzProtected         = createInfo.flags.tmzProtected;
    m_flags.tmzUserQueue         = internalInfo.flags.tmzUserQueue;
    m_flags.mallRangeActive      = createInfo.flags.mallRangeActive;
    m_flags.dfSpmTraceBuffer     = internalInfo.flags.dfSpmTraceBuffer;
    m_flags.explicitSync         = createInfo.flags.explicitSync;
    m_flags.privPrimary          = createInfo.flags.privPrimary;
    m_flags.isClient             = internalInfo.flags.isClient;
    m_flags.pageDirectory        = internalInfo.flags.pageDirectory;
    m_flags.pageTableBlock       = internalInfo.flags.pageTableBlock;
    m_flags.udmaBuffer           = internalInfo.flags.udmaBuffer;
    m_flags.unmapInfoBuffer      = internalInfo.flags.unmapInfoBuffer;
    m_flags.historyBuffer        = internalInfo.flags.historyBuffer;
    m_flags.isCmdAllocator       = internalInfo.flags.isCmdAllocator;
    m_flags.buddyAllocated       = internalInfo.flags.buddyAllocated;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 723
    m_flags.privateScreen        = createInfo.flags.privateScreen;
#else
    m_flags.privateScreen        = internalInfo.flags.privateScreen;
#endif
    m_flags.isUserQueue          = internalInfo.flags.userQueue;
    m_flags.isTimestamp          = internalInfo.flags.timestamp;
    m_flags.accessedPhysically   = internalInfo.flags.accessedPhysically;
    m_flags.gpuReadOnly          = internalInfo.flags.gpuReadOnly;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 746
    m_flags.kmdShareUmdSysMem       = createInfo.flags.kmdShareUmdSysMem;
#endif

    if (IsClient() == false)
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

    const gpusize allocGranularity   = (IsVirtual() == false) ?
        m_pDevice->MemoryProperties().realMemAllocGranularity :
        m_pDevice->MemoryProperties().virtualMemAllocGranularity;

    // If this is not external SDI memory, align size and base alignment to allocGranularity. If no alignment value was
    // provided, use the allocation granularity. This enforces a general PAL assumption: GPU memory objects have page
    // aligned addresses and sizes.
    if (createInfo.flags.sdiExternal == 0)
    {
        const Pal::gpusize alignedSize = Pow2Align(createInfo.size, allocGranularity);
        if (m_desc.size < alignedSize)
        {
            m_desc.size = alignedSize;
        }

        m_desc.alignment = ((createInfo.alignment != 0) ?
                            Pow2Align(createInfo.alignment, allocGranularity) :
                            allocGranularity);

        PAL_ASSERT((createInfo.alignment == 0) || ((m_desc.alignment % createInfo.alignment) == 0));
    }
    else
    {
        if (m_desc.size < createInfo.size)
        {
            m_desc.size = createInfo.size;
        }

        m_desc.alignment = ((createInfo.alignment != 0) ? createInfo.alignment : allocGranularity);
    }

    m_vaPartition    = m_pDevice->ChooseVaPartition(createInfo.vaRange, (createInfo.flags.virtualAlloc != 0));
    m_priority       = createInfo.priority;
    m_priorityOffset = createInfo.priorityOffset;
    m_mallPolicy     = createInfo.mallPolicy;
    m_mallRange      = createInfo.mallRange;

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

    if (IsVirtual() == false)
    {
        TranslateHeapInfo(*m_pDevice, createInfo, m_heaps, &m_heapCount);

        // NOTE: Assume that the heap selection is both local-only and nonlocal-only temporarily. When we scan the
        // heap selections below, this paradoxical assumption will be corrected.
        m_flags.localOnly    = 1;
        m_flags.nonLocalOnly = 1;

        // NOTE: Any memory object not being used as a page-directory or page-table block is considered to be CPU
        // visible as long as all of its selected heaps are CPU visible.
        m_flags.cpuVisible = ((m_flags.pageDirectory  == 0) &&
                              (m_flags.pageTableBlock == 0) &&
                              (createInfo.flags.cpuInvisible == 0));

        m_desc.heapCount = static_cast<uint32>(m_heapCount);
        for (uint32 heap = 0; heap < m_heapCount; ++heap)
        {
            m_desc.heaps[heap] = m_heaps[heap];

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
                PAL_ALERT_ALWAYS_MSG("Unexpected GPU heap type");
                break;
            }
        }

        // Give OS-specific code an opportunity to examine the client-specified heaps and add an extra GART backup
        // heap for local-only allocations if needed.
        if (m_heapCount > 0)
        {
            PAL_ASSERT((m_flags.nonLocalOnly == 0) || (m_flags.localOnly == 0));
            OsFinalizeHeaps();
        }
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 746
    m_flags.deferCpuVaReservation = (m_flags.cpuVisible != false) && createInfo.flags.deferCpuVaReservation;
#endif

    m_flags.isLocalPreferred = ((m_heaps[0] == GpuHeapLocal) || (m_heaps[0] == GpuHeapInvisible));

    Result result = Result::Success;

    if (IsShared())
    {
        result = OpenSharedMemory(internalInfo.hExternalResource);

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
            PAL_ASSERT(m_vaPartition != VaPartition::ShadowDescriptorTable);

            baseVirtAddr = desc.gpuVirtAddr;
        }

        if (m_vaPartition == VaPartition::ShadowDescriptorTable)
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
            // either fragment-aligned or aligned to KMD's reported optimized large page size, big page size or for
            // specific images iterate256 page size.  This should be skipped if any of the following are true:
            // - We're not using the default VA range because non-default VA ranges have special address usage rules.
            // - We have selected a specific base VA for the allocation because it might not be 64KB aligned.
            // - The allocation prefers a non-local heap because we can only get 64KB fragments in local memory.
            // - The allocation prefers a local visible heap on ResizeBarOff case. Local visible heap size in
            //   ResizeBarOff case has small size (usually 256MB); it's easy to run out of the heap size due to various
            //   alignment padding which will cause worse performance.
            // - Type is SDI ExternalPhysical because it has no real allocation and size must be consistent with KMD.
            const GpuMemoryProperties& memoryProperties = m_pDevice->MemoryProperties();
            bool invisibleHeapIsEmpty = m_pDevice->HeapLogicalSize(GpuHeapInvisible) == 0;
            if ((baseVirtAddr == 0) &&
                ((m_heaps[0] == GpuHeapInvisible) ||
                 ((m_heaps[0] == GpuHeapLocal) && invisibleHeapIsEmpty)) &&
                (createInfo.flags.sdiExternal == 0))
            {
                gpusize idealAlignment = 0;

                if ((memoryProperties.largePageSupport.gpuVaAlignmentNeeded ||
                    memoryProperties.largePageSupport.sizeAlignmentNeeded) &&
                    m_pDevice->Settings().enableLargePagePreAlignment)
                {
                    const gpusize largePageSize = memoryProperties.largePageSupport.largePageSizeInBytes;
                    idealAlignment = Max(idealAlignment, largePageSize);
                }
                // BigPage is only supported for allocations > bigPageMinAlignment.
                // Also, if bigPageMinAlignment == 0, BigPage optimization is not supported per KMD.
                // We do either LargePage or BigPage alignment, whichever has a higher value.
                if ((memoryProperties.bigPageMinAlignment > 0) &&
                     m_pDevice->Settings().enableBigPagePreAlignment &&
                     (createInfo.size >= memoryProperties.bigPageMinAlignment))
                {
                    gpusize bigPageSize = memoryProperties.bigPageMinAlignment;
                    if ((memoryProperties.bigPageLargeAlignment > 0) &&
                        (createInfo.size >= memoryProperties.bigPageLargeAlignment))
                    {
                        bigPageSize = memoryProperties.bigPageLargeAlignment;
                    }
                    idealAlignment = Max(idealAlignment, bigPageSize);
                }

                // Finally, we try to do alignment for iterate256 hardware optimization if m_pImage is populated and
                // all required conditions for the device and image to support it are met.
                // When we do this we are actually making this Image (rather the memory block/page that contains this
                // Image) compatible to pass the conditions of Image::GetIterate256(); which in turn will actually help
                // when creating this Image's SRD or for setting the value of the iterate256 register or the related
                // DecompressOnNZPlanes register.
                if ((m_pImage != nullptr) && m_pDevice->GetGfxDevice()->SupportsIterate256())
                {
                    // If the device supports iterate256 the Image should satisy some conditions so that we can
                    // justify aligning memory to make the optimization work.
                    const SubResourceInfo* pBaseSubResInfo = m_pImage->SubresourceInfo(0);
                    if (m_pDevice->Settings().enableIterate256PreAlignment &&
                        m_pImage->GetGfxImage()->IsIterate256Meaningful(pBaseSubResInfo) &&
                        (createInfo.size >= memoryProperties.iterate256MinAlignment))
                    {
                        gpusize iterate256PageSize = memoryProperties.iterate256MinAlignment;
                        if ((memoryProperties.iterate256LargeAlignment > 0) &&
                            createInfo.size >= memoryProperties.iterate256LargeAlignment)
                        {
                            iterate256PageSize = memoryProperties.iterate256LargeAlignment;
                        }
                        idealAlignment = Max(idealAlignment, iterate256PageSize);
                    }
                }
                // The client decides whether or not we pad allocations at all and so should be the final
                // deciding factor on use of ideal alignment.
                if ((createInfo.size >= m_pDevice->GetPublicSettings()->largePageMinSizeForVaAlignmentInBytes) &&
                    (idealAlignment != 0))
                {
                    m_desc.alignment = Pow2Align(m_desc.alignment, idealAlignment);
                }

                if ((createInfo.size >= m_pDevice->GetPublicSettings()->largePageMinSizeForSizeAlignmentInBytes) &&
                    (idealAlignment != 0))
                {
                    m_desc.size = Pow2Align(m_desc.size, idealAlignment);
                }
            }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 761
            if ((createInfo.flags.startVaHintFlag == 1) && (createInfo.startVaHint != 0))
            {
                gpusize startVaHintAddr = 0;
                gpusize endVaHintAddr   = 0;
                m_pDevice->VirtualAddressRange(VaPartition::Default, &startVaHintAddr, &endVaHintAddr);
                const gpusize pageSize          = m_pDevice->MemoryProperties().virtualMemPageSize;
                const gpusize alignment         = Pow2Align(createInfo.alignment, pageSize);
                const gpusize startVaHintAlign  = Pow2Align(createInfo.startVaHint, alignment);

                if ((startVaHintAlign >= startVaHintAddr) && ((startVaHintAlign + m_desc.size) < endVaHintAddr))
                {
                    baseVirtAddr = startVaHintAlign;
                }
            }
#endif
        }
        else if (createInfo.vaRange == VaRange::CaptureReplay)
        {
            baseVirtAddr = createInfo.replayVirtAddr;
        }

        if (result == Result::Success && (IsExternPhys() == false))
        {
            result = AllocateOrPinMemory(baseVirtAddr,
                                         internalInfo.pPagingFence,
                                         createInfo.virtualAccessMode,
                                         0,
                                         nullptr,
                                         nullptr);
        }

        if (IsErrorResult(result) == false)
        {
            m_desc.uniqueId = GenerateUniqueId();

            DescribeGpuMemory(Developer::GpuMemoryAllocationMethod::Normal);
        }
    }

    // Verify that if the allocation succeeded, we got a GPU virtual address back as expected (except for
    // page directory and page table allocations and SDI External Physical Memory).
    if ((IsPageDirectory() == false) && (IsPageTableBlock() == false) && (IsExternPhys() == false))
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

    m_desc.clientSize = createInfo.size; // store the requested size before any alignment
    m_desc.size       = createInfo.size;
    m_desc.alignment  = createInfo.alignment;
    m_desc.uniqueId   = GenerateUniqueId();
    if (createInfo.flags.gl2Uncached)
    {
        m_mtype = MType::Uncached;
    }
    m_mallPolicy     = createInfo.mallPolicy;
    m_mallRange      = createInfo.mallRange;

    m_vaPartition    = VaPartition::Svm;
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
                m_heaps[m_heapCount]      = heap;
                m_desc.heaps[m_heapCount] = heap;
                m_heapCount++;
            }
        }

        m_desc.heapCount = static_cast<uint32>(m_heapCount);

        m_flags.isLocalPreferred = ((m_heaps[0] == GpuHeapLocal) || (m_heaps[0] == GpuHeapInvisible));

        result          = AllocateOrPinMemory(m_desc.gpuVirtAddr,
                                              nullptr,
                                              VirtualGpuMemAccessMode::Undefined,
                                              0,
                                              nullptr,
                                              nullptr);
        m_pPinnedMemory = reinterpret_cast<const void*>(m_desc.gpuVirtAddr);
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
    m_flags.isClient     = 1;
    m_flags.nonLocalOnly = 1; // Pinned allocations always go into a non-local heap.
    m_flags.cpuVisible   = 1; // Pinned allocations are by definition CPU visible.

    m_pPinnedMemory   = createInfo.pSysMem;
    m_mallPolicy      = createInfo.mallPolicy;
    m_mallRange       = createInfo.mallRange;
    m_desc.clientSize = createInfo.size; // store the requested size before any alignment
    m_desc.size       = createInfo.size;
    m_desc.alignment  = (createInfo.alignment != 0) ? createInfo.alignment
                                                   : m_pDevice->MemoryProperties().realMemAllocGranularity;
    m_desc.uniqueId   = GenerateUniqueId();

    m_vaPartition                    = m_pDevice->ChooseVaPartition(createInfo.vaRange, false);

    // Scan the list of available GPU heaps to determine which heap(s) this pinned allocation will end up in.
    for (uint32 idx = 0; idx < GpuHeapCount; ++idx)
    {
        const GpuHeap heap = static_cast<GpuHeap>(idx);

        if (m_pDevice->HeapProperties(heap).flags.holdsPinned != 0)
        {
            m_heaps[m_heapCount]      = heap;
            m_desc.heaps[m_heapCount] = heap;
            m_heapCount++;
        }
    }

    m_desc.heapCount = static_cast<uint32>(m_heapCount);

    m_flags.isLocalPreferred = ((m_heaps[0] == GpuHeapLocal) || (m_heaps[0] == GpuHeapInvisible));

    const Result result = AllocateOrPinMemory(0, nullptr, VirtualGpuMemAccessMode::Undefined, 0, nullptr, nullptr);

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
    m_pOriginalMem    = static_cast<GpuMemory*>(openInfo.pSharedMem);
    m_desc.clientSize = m_pOriginalMem->m_desc.clientSize;
    m_desc.size       = m_pOriginalMem->m_desc.size;
    m_desc.alignment  = m_pOriginalMem->m_desc.alignment;
    m_desc.uniqueId   = m_pOriginalMem->m_desc.uniqueId;
    m_vaPartition     = m_pOriginalMem->m_vaPartition;
    m_mtype           = m_pOriginalMem->m_mtype;
    m_heapCount       = m_pOriginalMem->m_heapCount;
    m_mallPolicy      = m_pOriginalMem->MallPolicy();
    m_mallRange       = m_pOriginalMem->MallRange();

    for (uint32 i = 0; i < m_heapCount; ++i)
    {
        m_heaps[i]      = m_pOriginalMem->m_heaps[i];
        m_desc.heaps[i] = m_pOriginalMem->m_heaps[i];
    }

    m_desc.heapCount         = static_cast<uint32>(m_heapCount);
    m_desc.flags.isShared    = 1;
    m_flags.isShareable      = m_pOriginalMem->m_flags.isShareable;
    m_flags.isFlippable      = m_pOriginalMem->m_flags.isFlippable;
    m_flags.isStereo         = m_pOriginalMem->m_flags.isStereo;
    m_flags.localOnly        = m_pOriginalMem->m_flags.localOnly;
    m_flags.nonLocalOnly     = m_pOriginalMem->m_flags.nonLocalOnly;
    m_flags.isLocalPreferred = m_pOriginalMem->m_flags.isLocalPreferred;
    m_flags.interprocess     = m_pOriginalMem->m_flags.interprocess;
    m_flags.globalGpuVa      = m_pOriginalMem->m_flags.globalGpuVa;
    m_flags.cpuVisible       = m_pOriginalMem->m_flags.cpuVisible;

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

    Pal::GpuMemoryExportInfo exportInfo = { };
    const Result result = OpenSharedMemory(
#if PAL_AMDGPU_BUILD
            m_pOriginalMem->ExportExternalHandle(exportInfo));
#else
            0);
#endif
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
    m_pOriginalMem    = static_cast<GpuMemory*>(peerInfo.pOriginalMem);
    m_desc.clientSize = m_pOriginalMem->m_desc.clientSize;
    m_desc.size       = m_pOriginalMem->m_desc.size;
    m_desc.alignment  = m_pOriginalMem->m_desc.alignment;
    m_desc.uniqueId   = m_pOriginalMem->m_desc.uniqueId;
    m_vaPartition     = m_pOriginalMem->m_vaPartition;
    m_mtype           = m_pOriginalMem->m_mtype;
    m_heapCount       = m_pOriginalMem->m_heapCount;
    m_mallPolicy      = m_pOriginalMem->MallPolicy();
    m_mallRange       = m_pOriginalMem->MallRange();

    for (uint32 i = 0; i < m_heapCount; ++i)
    {
        m_heaps[i]      = m_pOriginalMem->m_heaps[i];
        m_desc.heaps[i] = m_pOriginalMem->m_heaps[i];
    }

    m_desc.heapCount         = static_cast<uint32>(m_heapCount);
    m_desc.flags.isPeer      = 1;
    m_flags.isShareable      = m_pOriginalMem->m_flags.isShareable;
    m_flags.isFlippable      = m_pOriginalMem->m_flags.isFlippable;
    m_flags.isStereo         = m_pOriginalMem->m_flags.isStereo;
    m_flags.localOnly        = m_pOriginalMem->m_flags.localOnly;
    m_flags.nonLocalOnly     = m_pOriginalMem->m_flags.nonLocalOnly;
    m_flags.isLocalPreferred = m_pOriginalMem->m_flags.isLocalPreferred;
    m_flags.interprocess     = m_pOriginalMem->m_flags.interprocess;
    m_flags.globalGpuVa      = m_pOriginalMem->m_flags.globalGpuVa;
    m_flags.useReservedGpuVa = (m_vaPartition == VaPartition::Svm);
    m_flags.cpuVisible       = m_pOriginalMem->m_flags.cpuVisible;
    m_flags.peerWritable     = m_pOriginalMem->m_flags.peerWritable;

    // Set the gpuVirtAddr if the GPU VA is visible to all devices
    if (IsGlobalGpuVa() || IsGpuVaPreReserved())
    {
        m_desc.gpuVirtAddr = m_pOriginalMem->m_desc.gpuVirtAddr;
    }

    // NOTE: The following flags are not expected to be set for peer memory objects!
    PAL_ASSERT((m_pOriginalMem->m_desc.flags.isVirtual == 0) &&
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
    PAL_ASSERT(pMapDestPeerMem->IsPeer());
    PAL_ASSERT((m_pMapDestPeerMem == nullptr) || (m_pMapDestPeerMem == pMapDestPeerMem));
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

        if (result == Result::Success)
        {
            m_pDevice->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryCpuMapEvent(this);
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

    if (result == Result::Success)
    {
        m_pDevice->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryCpuUnmapEvent(this);
    }

    return result;
}

// =====================================================================================================================
// Returns the preferred heap.
GpuHeap GpuMemory::PreferredHeap() const
{
    // Virtual memory is not backed by a heap, so there can be no preferred heap.
    PAL_ASSERT_MSG((IsVirtual() == false), "Getting preferred heap of virtual memory is invalid");
    PAL_ASSERT(m_heapCount > 0);
    return m_heaps[0];
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
    data.flags.isExternal         = IsExternal();
    data.flags.buddyAllocated     = WasBuddyAllocated();
    data.allocMethod              = allocMethod;
    data.pGpuMemory               = this;
    m_pDevice->DeveloperCb(Developer::CallbackType::AllocGpuMemory, &data);
}

// =====================================================================================================================
bool GpuMemory::IsCpuVisible() const
{
    bool  isCpuVisible = (m_flags.cpuVisible != 0);

    return isCpuVisible;
}

// =====================================================================================================================
// Returns an acceptable physical memory address base alignment for the given gpu memory object. To avoid fragmentation
// this should be a small value unless there are hardware or OS reasons to increase it.
gpusize GpuMemory::GetPhysicalAddressAlignment() const
{
    // By default copy the virtual address alignment. This is the safest thing we can do and will meet all HW
    // requirements, assuming the caller gave us a properly aligned alignment as required by the PAL interface.
    gpusize alignment = m_desc.alignment;

    // Now if this GPU memory object doesn't place special requirements on the physical address alignment we want to
    // pick a much smaller alignment to avoid heap fragmentation. Clearly this means we can't change the alignment if
    // we're going to use physical engines like some of the video engines or the display controller. Conceptually any
    // hardware that uses virtual addresses will never care about the physical address so we can make its alignment as
    // low as we want. Note that the PhysicalEnginesAvailable check is total overkill and effectively forces large
    // alignments for all allocations if someone creates a physical queue, however we have no other choice because
    // we don't know if this allocation will be used on a physical engine until we see the patch list at submit time.
    //
    // However when non-PAL code opens a shared resource it may use the physical alignment as the virtual alignment
    // which means that we need to tie the two alignments together to avoid corruption. In theory we can fix this issue
    // by modifying the KMD and UMDs but that's a big can of worms so let's just keep the larger alignment for now.
    if ((IsSvmAlloc() == false)         &&
        (IsShareable() == false)        &&
        (IsFlippable() == false)        &&
        (IsXdmaBuffer() == false)       &&
        (IsInterprocess() == false)     &&
        (IsBusAddressable() == false)   &&
        (IsTurboSyncSurface() == false) &&
        (IsPrivPrimary() == false)      &&
        (m_pDevice->PhysicalEnginesAvailable() == false))
    {
        const GpuMemoryProperties& memProps = m_pDevice->MemoryProperties();

        // Runtime will keep the same alignment between physical alignment and virtual alignment by default.
        // If this function returns a smaller alignment, we have to use ReserveGpuVirtualAddress to reserve
        // the VA which aligns to customer required alignment.
        if (memProps.flags.virtualRemappingSupport == 1)
        {
            // Default to clamping the physical address to system page alignment.
            gpusize clamp = memProps.realMemAllocGranularity;

            if (IsNonLocalOnly() == false)
            {
                // If the allocation supports local heaps and is suitably large, increase the clamp to the large
                // page size or big page size (typically 256KiB or 2MiB) or fragment size (typically 64KiB) as
                // appropriate to allow hardware-specific big page features when the allocation resides in local.
                // If the allocation is small, stick with the system page alignment to avoid fragmentation.
                const gpusize fragmentSize = memProps.fragmentSize;

                // If client allows it we can try to do alignments for LargePage, BigPage or Iterate256 optimization.
                if (m_desc.size >= m_pDevice->GetPublicSettings()->largePageMinSizeForSizeAlignmentInBytes)
                {
                    // LargePage alignment.
                    if (memProps.largePageSupport.sizeAlignmentNeeded)
                    {
                        clamp = Max(clamp, memProps.largePageSupport.largePageSizeInBytes);
                    }
                    // BigPage alignment.
                    if ((memProps.bigPageMinAlignment > 0) &&
                        (m_desc.size >= memProps.bigPageMinAlignment))
                    {
                        clamp = Max(clamp, memProps.bigPageMinAlignment);
                        if ((memProps.bigPageLargeAlignment > 0) && (m_desc.size >= memProps.bigPageLargeAlignment))
                        {
                            clamp = Max(clamp, memProps.bigPageLargeAlignment);
                        }
                    }
                    // Iterate256 alignment.
                    if ((m_pImage != nullptr) &&
                        m_pDevice->GetGfxDevice()->SupportsIterate256() &&
                        (m_pImage->GetGfxImage()->IsIterate256Meaningful(
                            (m_pImage->SubresourceInfo(0)))) &&
                        (m_desc.size >= memProps.iterate256MinAlignment))
                    {
                        clamp = Max(clamp, memProps.iterate256MinAlignment);
                        if ((memProps.iterate256LargeAlignment > 0) &&
                            (m_desc.size >= memProps.iterate256LargeAlignment))
                        {
                            clamp = Max(clamp, memProps.iterate256LargeAlignment);
                        }
                    }
                }
                if (m_desc.size >= fragmentSize)
                {
                    clamp = Max(clamp, fragmentSize);
                }
            }

            alignment = Min(alignment, clamp);
        }
    }

    return alignment;
}

// =====================================================================================================================
/// Generate a 64-bit unique ID for this GPU memory.
uint64 GpuMemory::GenerateUniqueId(void) const
{
    return reinterpret_cast<uint64>(this) ^ Uint64CombineParts(0, GetIdOfCurrentProcess());
}

} // Pal
