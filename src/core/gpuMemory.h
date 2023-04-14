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

#pragma once

#include "palGpuMemory.h"
#include "palInlineFuncs.h"
#include "palDeveloperHooks.h"

namespace Pal
{

class  Device;
class  Image;
class  Queue;
struct VirtualMemoryRemapRange;
struct VirtualMemoryCopyPageMappingsRange;
enum class VaPartition : uint32;

// A somewhat abstracted version of the gfxip cache MTYPE. Which caches respect the this property is hardware specific.
enum class MType : uint32
{
    Default = 0,       // The kernel should use its default MTYPE.
    CachedNoncoherent, // Cache reads and writes without worrying about CPU coherency.
    CachedCoherent,    // Cache reads and writes while maintaining CPU coherency.
    Uncached,          // Cache reads and writes punch through the cache.
    Count
};

// Defines additional information describing GPU memory objects beyond what PAL clients are able to specify.
struct GpuMemoryInternalCreateInfo
{
    union
    {
        struct
        {
            uint32 isExternal         :  1; // GPU memory is owned by an external process.
            uint32 isClient           :  1; // GPU memory is requested by the client.
            uint32 pageDirectory      :  1; // GPU memory will be used for a Page Directory.
            uint32 pageTableBlock     :  1; // GPU memory will be used for a Page Table Block.
            uint32 isCmdAllocator     :  1; // GPU memory is owned by an ICmdAllocator.
            uint32 udmaBuffer         :  1; // GPU memory will be used for a UDMA buffer containing GPU commands which
                                            // are contained within a command buffer.
            uint32 unmapInfoBuffer    :  1; // Gpu memory will be used for GPU DMA writeback of unmapped VA info.
            uint32 historyBuffer      :  1; // GPU memory will be used for a history buffer containing GPU timestamps.
            uint32 xdmaBuffer         :  1; // GPU memory will be used for an XDMA cache buffer for transferring data
                                            // between GPU's in a multi-GPU configuration.
            uint32 alwaysResident     :  1; // Indicates the GPU memory allocation must be always resident.
            uint32 buddyAllocated     :  1; // GPU memory was allocated by a buddy allocator to be used for
                                            // suballocating smaller memory blocks.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 723
            uint32 placeHolder723     :  1;
#else
            uint32 privateScreen      :  1; // GPU memory will be used for a private screen image.
#endif
            uint32 userQueue          :  1; // GPU memory will be used for an user queue.
            uint32 tmzUserQueue       :  1; // GPU memory will be used for a TMZ enabled user queue.
            uint32 timestamp          :  1; // GPU memory will be used for KMD timestamp writeback.
            uint32 accessedPhysically :  1; // GPU memory will be accessed physically (physical engine like MM video).
            uint32 pageFaultDebugSrd  :  1; // GPU memory will be used for PageFaultDebugSrd feature.
            uint32 gpuReadOnly        :  1; // Indicates the memory is read-only on the GPU
            uint32 dfSpmTraceBuffer   :  1; // GPU memory will be used for a DF SPM trace buffer.
            uint32 reserved           : 13;
        };
        uint32 u32All;
    } flags;

    // Base virtual address to use for page-table block allocations.
    gpusize baseVirtAddr;

    // The UMDKMDIF_SCHEDULERIDENTIFIER will be recorded if the GpuMemory is for user queue.
    uint32 schedulerId;

    // Number of CUs to reserve. Used by KMD for a real-time user queue in the legacy HWS model.
    uint32 numReservedCu;

    // Pointer to where the OS-returned paging fence value should be stored. This is ignored if flags.alwaysResident is
    // not set. If alwaysResident is set and this is nullptr, then the paging fence returned by the OS will be waited
    // on before returning from the call to allocate GPU memory.
    uint64* pPagingFence;

    // the resource handle to use for importing the buffer.
    // it is only valid when flags.isExternal is set.
    OsExternalHandle hExternalResource;

    // The type of hExternalResource(Linux) such as: GEM global name or dma-buf file descriptor.
    uint32 externalHandleType;

    // Used to override the KMD's default MTYPE selection. Default to zero if it doesn't matter.
    MType mtype;
};

// All of the flags which describe the traits of a GPU memory allocation.
union GpuMemoryFlags
{
    struct
    {
        uint32 isPinned                 :  1; // GPU memory was pinned for GPU access from CPU memory.
        uint32 isPresentable            :  1; // GPU memory can be used by presents.
        uint32 isFlippable              :  1; // GPU memory can be used by flip presents.
        uint32 isStereo                 :  1; // GPU memory will be used for a stereoscopic surface.
        uint32 isClient                 :  1; // GPU memory is requested by the client.
        uint32 isShareable              :  1; // GPU memory can be shared with other Device's without needing peer
                                              // transfers.
        uint32 interprocess             :  1; // GPU memory is visible to other processes (they may choose to open it).
        uint32 pageDirectory            :  1; // GPU memory will be used for a Page Directory.
        uint32 pageTableBlock           :  1; // GPU memory will be used for a Page Table Block.
        uint32 isCmdAllocator           :  1; // GPU memory is owned by an ICmdAllocator.
        uint32 udmaBuffer               :  1; // GPU memory will be used for a UDMA buffer containing GPU commands which
                                              // are contained within a command buffer.
        uint32 unmapInfoBuffer          :  1; // GPU memory will be used for GPU DMA writeback of unmapped VA ranges.
        uint32 historyBuffer            :  1; // GPU memory will be used for a history buffer containing GPU timestamps.
        uint32 xdmaBuffer               :  1; // GPU memory will be used for an XDMA cache buffer for transferring data
                                              // between GPU's in a multi-GPU configuration.
        uint32 turboSyncSurface         :  1; // GPU memory is private swapchain primary allocation for TurboSync.
        uint32 alwaysResident           :  1; // GPU memory needs to always be resident for proper operation. Not all
                                              // platforms care about this flag.
        uint32 buddyAllocated           :  1; // GPU memory was allocated by a buddy allocator to be used for
                                              // suballocating smaller memory blocks.
        uint32 localOnly                :  1; // GPU memory doesn't prefer nonlocal heaps.
        uint32 nonLocalOnly             :  1; // GPU memory doesn't prefer local heaps.
        uint32 isLocalPreferred         :  1; // If GPU memory prefers local heaps as the first heap choice.
        uint32 cpuVisible               :  1; // GPU memory is CPU accessible via Map().
        uint32 privateScreen            :  1; // GPU memory is bound to a private screen image.
        uint32 isUserQueue              :  1; // GPU memory is used for an user queue.
        uint32 globallyCoherent         :  1; // GPU memory is globally coherent.
        uint32 isTimestamp              :  1; // GPU memory will be used for KMD timestamp writeback.
        uint32 globalGpuVa              :  1; // GPU memory virtual address must be visible to all devices.
        uint32 useReservedGpuVa         :  1; // GPU memory virtual address is provided by client and was previously
                                              // reserved by other object. Current object should not free this VA.
        uint32 typedBuffer              :  1; // GPU memory is bound to a typed buffer.
        uint32 accessedPhysically       :  1; // GPU memory can be accessed physically (physical engine like MM video).
        uint32 busAddressable           :  1; // GPU memory is Bus Addressable memory
        uint32 autoPriority             :  1; // GPU memory priority is to be managed automatically
        uint32 peerWritable             :  1; // GPU memory can be open as peer memory and be writable
        uint32 mapppedToPeerMemory      :  1; // GPU memory is remapped to at least one peer physical memory.
        uint32 tmzProtected             :  1; // GPU memory is TMZ protected.
        uint32 tmzUserQueue             :  1; // GPU memory is a user queue for TMZ submission.
        uint32 placeholder0             :  1; // Placeholder.
        uint32 restrictedContent        :  1; // GPU memory is protected content
        uint32 restrictedAccess         :  1; // GPU memory is restricted shared access resource
        uint32 crossAdapter             :  1; // GPU memory is shared cross-adapter resource
        uint32 gpuReadOnly              :  1; // GPU memory is read only.
        uint32 mallRangeActive          :  1;
        uint32 dfSpmTraceBuffer         :  1; // GPU memory will be used by KMD for DF SPM trace.
        uint32 explicitSync             :  1;
        uint32 privPrimary              :  1; // GPU memory is a private primary
        uint32 kmdShareUmdSysMem        :  1; // GPU memory is shared with KMD
        uint32 deferCpuVaReservation    :  1; // GPU memory can be locked for read on CPU, but will not reserve CPU VA.
        uint32 placeholder1             : 1; // Placeholder.
        uint32 reserved                 : 17;
    };
    uint64  u64All;
};

// =====================================================================================================================
// Represents a single allocation of GPU-accessible memory. Depending on creation parameters, this could correspond to:
//
// * A real memory object, corresponding directly to a physical allocation made on this device (whether it resides in
//   a local or non-local heap).
// * A virtual memory object, only consisting of virtual address space that can be mapped on a page basis to pages in
//   real memory objects via IDevice::RemapVirtualMemoryPages.
// * Pinned memory, a real memory object created by pinning down client system memory.
// * Peer memory, a real memory object corresponding to GPU memory that is likely local to another GPU.  Only copy
//   operations (peer-to-peer transfers) are allowed with this memory.
// * Opened/shared memory, a real memory object that is fully shared between multiple GPUs, residing in a non-local
//   heap.
class GpuMemory : public IGpuMemory
{
public:
    static void TranslateHeapInfo(
        const Device&              device,
        const GpuMemoryCreateInfo& createInfo,
        GpuHeap*                   pOutHeaps,
        size_t*                    pOutHeapCount);
    static Result ValidateCreateInfo(const Device* pDevice, const GpuMemoryCreateInfo& createInfo);
    static Result ValidatePinInfo(const Device* pDevice, const PinnedGpuMemoryCreateInfo& createInfo);
    static Result ValidateSvmInfo(const Device* pDevice, const SvmGpuMemoryCreateInfo& createInfo);
    static Result ValidateOpenInfo(const Device* pDevice, const GpuMemoryOpenInfo& openInfo);
    static Result ValidatePeerOpenInfo(const Device* pDevice, const PeerGpuMemoryOpenInfo& peerInfo);

    virtual Result Init(const GpuMemoryCreateInfo& createInfo, const GpuMemoryInternalCreateInfo& internalInfo);
    virtual Result Init(const PinnedGpuMemoryCreateInfo& createInfo);
    virtual Result Init(const SvmGpuMemoryCreateInfo& createInfo);
    Result Init(const GpuMemoryOpenInfo& openInfo);
    Result Init(const PeerGpuMemoryOpenInfo& peerInfo);

    // NOTE: Part of the public IDestroyable interface. Since clients own the memory allocation this object resides
    // in, this only invokes the object's destructor.
    virtual void Destroy() override { this->~GpuMemory(); }
    void DestroyInternal();

    // NOTE: Part of the public IGpuMemory interface.
    virtual Result SetPriority(
        GpuMemPriority       priority,
        GpuMemPriorityOffset priorityOffset) override;

    GpuMemPriority Priority() const { return m_priority; }

    GpuMemPriorityOffset PriorityOffset() const { return m_priorityOffset; }

    GpuMemMallPolicy MallPolicy() const { return m_mallPolicy; }

    const GpuMemMallRange& MallRange() const { return m_mallRange; }

    // NOTE: Part of the public IGpuMemory interface.
    virtual Result Map(
        void** ppData) override;

    // NOTE: Part of the public IGpuMemory interface.
    virtual Result Unmap() override;

    VaPartition VirtAddrPartition() const { return m_vaPartition; }
    MType Mtype() const { return m_mtype; }

    GpuHeap PreferredHeap() const;

    // NOTE: Part of the public IGpuMemory interface. Set SDI remote surface bus address and marker bus address.
    virtual Result SetSdiRemoteBusAddress(gpusize surfaceBusAddr, gpusize markerBusAddr) override
        { return Result::ErrorUnavailable; }

    bool IsVirtual()             const { return (m_desc.flags.isVirtual           != 0); }
    bool IsPeer()                const { return (m_desc.flags.isPeer              != 0); }
    bool IsShared()              const { return (m_desc.flags.isShared            != 0); }
    bool IsExternal()            const { return (m_desc.flags.isExternal          != 0); }
    bool IsExternPhys()          const { return (m_desc.flags.isExternPhys        != 0); }
    bool IsPinned()              const { return (m_flags.isPinned                 != 0); }
    bool IsShareable()           const { return (m_flags.isShareable              != 0); }
    bool IsPresentable()         const { return (m_flags.isPresentable            != 0); }
    bool IsFlippable()           const { return (m_flags.isFlippable              != 0); }
    bool IsStereo()              const { return (m_flags.isStereo                 != 0); }
    bool IsClient()              const { return (m_flags.isClient                 != 0); }
    bool IsPageDirectory()       const { return (m_flags.pageDirectory            != 0); }
    bool IsPageTableBlock()      const { return (m_flags.pageTableBlock           != 0); }
    bool IsCmdAllocator()        const { return (m_flags.isCmdAllocator           != 0); }
    bool IsUdmaBuffer()          const { return (m_flags.udmaBuffer               != 0); }
    bool IsUnmapInfoBuffer()     const { return (m_flags.unmapInfoBuffer          != 0); }
    bool IsHistoryBuffer()       const { return (m_flags.historyBuffer            != 0); }
    bool IsXdmaBuffer()          const { return (m_flags.xdmaBuffer               != 0); }
    bool IsAlwaysResident()      const { return (m_flags.alwaysResident           != 0); }
    bool WasBuddyAllocated()     const { return (m_flags.buddyAllocated           != 0); }
    bool IsLocalOnly()           const { return (m_flags.localOnly                != 0); }
    bool IsNonLocalOnly()        const { return (m_flags.nonLocalOnly             != 0); }
    bool IsLocalPreferred()      const { return (m_flags.isLocalPreferred         != 0); }
    bool IsCpuVisible()          const;
    bool IsPrivateScreen()       const { return (m_flags.privateScreen            != 0); }
    bool IsInterprocess()        const { return (m_flags.interprocess             != 0); }
    bool IsUserQueue()           const { return (m_flags.isUserQueue              != 0); }
    bool IsGloballyCoherent()    const { return (m_flags.globallyCoherent         != 0); }
    bool IsTimestamp()           const { return (m_flags.isTimestamp              != 0); }
    bool IsGlobalGpuVa()         const { return (m_flags.globalGpuVa              != 0); }
    bool IsGpuVaPreReserved()    const { return (m_flags.useReservedGpuVa         != 0); }
    bool IsTypedBuffer()         const { return (m_flags.typedBuffer              != 0); }
    bool IsAutoPriority()        const { return (m_flags.autoPriority             != 0); }
    bool IsBusAddressable()      const { return (m_flags.busAddressable           != 0); }
    bool IsTurboSyncSurface()    const { return (m_flags.turboSyncSurface         != 0); }
    bool IsPeerWritable()        const { return (m_flags.peerWritable             != 0); }
    bool IsRestrictedContent()   const { return (m_flags.restrictedContent        != 0); }
    bool IsRestrictedAccess()    const { return (m_flags.restrictedAccess         != 0); }
    bool IsCrossAdapter()        const { return (m_flags.crossAdapter             != 0); }
    bool IsTmzProtected()        const { return (m_flags.tmzProtected             != 0); }
    bool IsTmzUserQueue()        const { return (m_flags.tmzUserQueue             != 0); }
    bool IsMapppedToPeerMemory() const { return (m_flags.mapppedToPeerMemory      != 0); }
    bool IsSvmAlloc()            const { return (m_desc.flags.isSvmAlloc          != 0); }
    bool IsExecutable()          const { return (m_desc.flags.isExecutable        != 0); }
    bool IsReadOnlyOnGpu()       const { return (m_flags.gpuReadOnly              != 0); }
    bool IsAccessedPhysically()  const { return (m_flags.accessedPhysically       != 0); }
    bool IsMallRangeActive()     const { return (m_flags.mallRangeActive          != 0); }
    bool IsDfSpmTraceBuffer()    const { return (m_flags.dfSpmTraceBuffer         != 0); }
    bool IsExplicitSync()        const { return (m_flags.explicitSync             != 0); }
    bool IsPrivPrimary()         const { return (m_flags.privPrimary              != 0); }
    bool IsKmdShareUmdSysMem()   const { return (m_flags.kmdShareUmdSysMem        != 0); }
    bool IsLockableOnDemand()    const { return (m_flags.deferCpuVaReservation    != 0); }
    void SetAccessedPhysically() { m_flags.accessedPhysically = 1; }
    void SetSurfaceBusAddr(gpusize surfaceBusAddr) { m_desc.surfaceBusAddr = surfaceBusAddr; }
    void SetMarkerBusAddr(gpusize markerBusAddr)   { m_desc.markerBusAddr  = markerBusAddr;  }
    gpusize GetRemoteSdiSurfaceIndex() const { return m_remoteSdiSurfaceIndex; }
    gpusize GetRemoteSdiMarkerIndex()  const { return m_remoteSdiMarkerIndex;  }
    gpusize GetBusAddrMarkerVa()       const { return m_markerVirtualAddr;     }
    void SetRemoteSdiSurfaceIndex(gpusize index) { m_remoteSdiSurfaceIndex = index;    }
    void SetRemoteSdiMarkerIndex(gpusize index)  { m_remoteSdiMarkerIndex  = index;    }
    void SetBusAddrMarkerVa(gpusize markerVa)    { m_markerVirtualAddr     = markerVa; }

    bool IsByteRangeValid(gpusize startOffset, gpusize size) const { return ((startOffset + size) <= m_desc.size); }

    Image* GetImage() const { return m_pImage; }

    Device* GetDevice() const { return m_pDevice; }

    gpusize GetPhysicalAddressAlignment() const;

    GpuMemory* OriginalGpuMem() const { return (IsPeer() ? m_pOriginalMem : nullptr); }

    void SetMapDestPeerMem(GpuMemory* pMapDestPeerMem);
    GpuMemory* MapDestPeerGpuMem() const { return (IsMapppedToPeerMemory() ? m_pMapDestPeerMem : nullptr); }

    bool AccessesPeerMemory() const { return (IsPeer() || IsMapppedToPeerMemory()); }

protected:
    explicit GpuMemory(Device* pDevice);
    virtual ~GpuMemory();

    // Performs OS-specific operation for allocating SVM virtual memory objects.
    virtual Result AllocateSvmVirtualAddress(
        gpusize baseVirtAddr,
        gpusize size,
        gpusize align,
        bool    commitCpuVa) = 0;

    // Performs OS-specific operation for destroying SVM virtual memory objects.
    virtual Result FreeSvmVirtualAddress() = 0;

    // Performs OS-specific initialization for allocating real, pinned or virtual memory objects.
    virtual Result AllocateOrPinMemory(
        gpusize                 baseVirtAddr,
        uint64*                 pPagingFence,
        VirtualGpuMemAccessMode virtualAccessMode,
        uint32                  multiDeviceGpuMemoryCount,
        IDevice*const*          ppDevice,
        Image*const*            ppImage) = 0;
    // Performs OS-specific initialization for opening a shared, non-peer memory object.
    virtual Result OpenSharedMemory(OsExternalHandle handle) = 0;

    // Performs OS-specific initialization for opening a connection to a peer memory object.
    virtual Result OpenPeerMemory() = 0;

    // Performs an OS-specific SetPriority operation after all PAL validation has been done.
    virtual Result OsSetPriority(GpuMemPriority priority, GpuMemPriorityOffset priorityOffset) = 0;

    // Performs an OS-specific Map operation after all PAL validation has been done.
    virtual Result OsMap(void** ppData) = 0;

    // Performs an OS-specific Unmap operation after all PAL validation has been done.
    virtual Result OsUnmap() = 0;

    virtual void DescribeGpuMemory(Developer::GpuMemoryAllocationMethod allocMethod) const;

    /// Generate a 64-bit unique ID for this GPU memory.
    uint64 GenerateUniqueId(void) const;

    Device*const   m_pDevice;
    VaPartition    m_vaPartition;
    size_t         m_heapCount;
    GpuHeap        m_heaps[GpuHeapCount];

    // Priority of the memory allocation: serves as a hint to the operating system of how important it is to keep
    // this memory object in its preferred heap(s).
    GpuMemPriority m_priority;

    GpuMemPriorityOffset m_priorityOffset;

    // NOTE: The following members are unioned because no single memory object requires more than one of them.
    union
    {
        // Pinned CPU memory address: the base address of the CPU memory allocation being pinned for GPU access. Only
        // needed by "pinned" memory objects.
        const void*  m_pPinnedMemory;
        // Pointer to this allocation's peer memory object, or to the memory object which this was opened from. Only
        // needed by peer and opened ("shared") memory objects from other devices within this process.
        GpuMemory*   m_pOriginalMem;
        // Pointer to the peer physical memory object to which this allocation maps. Only used by virtual memory
        // objects. One virtual memory object could legally map to multiple peer physical memory objects.  Currently
        // only support one peer physical memory object.
        GpuMemory*   m_pMapDestPeerMem;
        // System memory shared between KMD and UMD.
        void* m_pBackingStoreMemory;
    };

    // The pointer to an Image object the memory object is bound to. It is only necessary in special cases where an
    // internal memory object is permanently linked to an Image such as presentable images or shared resources on
    // Windows.
    Image*  m_pImage;

    // The UMDKMDIF_SCHEDULERIDENTIFIER will be recorded if the GpuMemory is for user queue. And it will be passed
    // to KMD via command buffer.
    uint32 m_schedulerId;

    // The reserved CUs value, used in KMD for a real-time user queue.
    uint32 m_numReservedCu;

    // If the typedBuffer flag is set, this GPU memory will be permanently considered a typed buffer.
    TypedBufferCreateInfo   m_typedBufferInfo;

    GpuMemoryFlags          m_flags;

    MType                   m_mtype;

    // SDI External Physical Memory PTE index for surface and marker
    gpusize m_remoteSdiSurfaceIndex;
    gpusize m_remoteSdiMarkerIndex;

private:
    // Some OSes have special rules about heap preferences.  This method should be overriden by such OSes to examine
    // the memory object and update/finalize the heap preferences as required.  One example is adding a backup GART
    // heap for client-requested local-only allocations on some OSes.
    virtual void OsFinalizeHeaps() { }

    // Marker virtual address as returned by KMD
    gpusize m_markerVirtualAddr;

    GpuMemMallPolicy  m_mallPolicy;
    GpuMemMallRange   m_mallRange;

    PAL_DISALLOW_DEFAULT_CTOR(GpuMemory);
    PAL_DISALLOW_COPY_AND_ASSIGN(GpuMemory);
};

// =====================================================================================================================
// Represents the state of a GPU memory binding to a PAL object.
class BoundGpuMemory
{
public:
    BoundGpuMemory()
        :
        m_pGpuMemory(nullptr),
        m_offset(0)
    {
    }

    ~BoundGpuMemory() { }

    void Update(
        IGpuMemory* pGpuMemory,
        gpusize     offset)
    {
        m_pGpuMemory = static_cast<GpuMemory*>(pGpuMemory);
        m_offset     = offset;
    }

    Result Map(void** ppMappedAddr)
    {
        Result result = Result::ErrorGpuMemoryNotBound;
        if (IsBound())
        {
            result = m_pGpuMemory->Map(ppMappedAddr);
            (*ppMappedAddr) = Util::VoidPtrInc(*ppMappedAddr, static_cast<size_t>(m_offset));
        }

        return result;
    }

    Result Unmap()
        { return m_pGpuMemory->Unmap(); }

    GpuMemory* Memory() const { return m_pGpuMemory; }
    gpusize Offset() const { return m_offset; }

    bool IsBound() const { return (m_pGpuMemory != nullptr); }

    gpusize GpuVirtAddr() const { return (m_pGpuMemory->Desc().gpuVirtAddr + m_offset); }

private:
    GpuMemory*  m_pGpuMemory;
    gpusize     m_offset;

    PAL_DISALLOW_COPY_AND_ASSIGN(BoundGpuMemory);
};

} // Pal
