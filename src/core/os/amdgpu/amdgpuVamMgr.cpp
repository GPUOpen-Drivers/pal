/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/os/amdgpu/amdgpuVamMgr.h"
#include "core/os/amdgpu/amdgpuDevice.h"
#include "palSysUtil.h"
#include "palHashMapImpl.h"

#include <atomic>

using namespace Util;

namespace Pal
{
namespace Amdgpu
{

// =====================================================================================================================
// Note that this constructor is invoked before settings have been committed.
VamMgr::VamMgr()
    :
    Pal::VamMgr(),
    m_mutex(),
    m_mapAllocator(),
    m_sharedBoMap(InitialBoCount, &m_mapAllocator)
{
    for (uint32 partIndex = 0; partIndex < static_cast<uint32>(VaPartition::Count); partIndex++)
    {
        m_vaRangeInfo[partIndex].allocatedVa     = nullptr;
        m_vaRangeInfo[partIndex].baseVirtualAddr = 0;
        m_vaRangeInfo[partIndex].size            = 0;
    }
}

// =====================================================================================================================
VamMgr::~VamMgr()
{
// Note: OCL API doesn't provide explicit device destruction
    // The VAM instance must be destroyed by calling Cleanup().
    PAL_ASSERT(m_hVamInstance == nullptr);
}

// =====================================================================================================================
// Performs any early-stage initialization.
Result VamMgr::EarlyInit()
{
    return m_sharedBoMap.Init();
}

// =====================================================================================================================
// Performs any late-stage initialization that can only be done after settings have been committed.
// - Starts up the VAM library.
Result VamMgr::LateInit(
    Pal::Device* pDevice)
{
    const auto& memProps = pDevice->MemoryProperties();
    Result      result   = Result::Success;

    m_ptbSize = CalcPtbSize(pDevice);

    VAM_CREATE_INPUT vamCreateIn = { };
    vamCreateIn.size          = sizeof(VAM_CREATE_INPUT);
    vamCreateIn.version.major = VAM_VERSION_MAJOR;
    vamCreateIn.version.minor = VAM_VERSION_MINOR;

    vamCreateIn.flags.useUIB = (memProps.uibVersion > 0);
    vamCreateIn.uibVersion   = memProps.uibVersion;

    vamCreateIn.callbacks.allocSysMem    = AllocSysMemCb;
    vamCreateIn.callbacks.freeSysMem     = FreeSysMemCb;
    vamCreateIn.callbacks.acquireSyncObj = AcquireSyncObjCb;
    vamCreateIn.callbacks.releaseSyncObj = ReleaseSyncObjCb;
    vamCreateIn.callbacks.allocPTB       = AllocPtbCb;
    vamCreateIn.callbacks.freePTB        = FreePtbCb;
    vamCreateIn.callbacks.allocVidMem    = AllocVidMemCb;
    vamCreateIn.callbacks.freeVidMem     = FreeVidMemCb;
    vamCreateIn.callbacks.offerVidMem    = OfferVidMemCb;
    vamCreateIn.callbacks.reclaimVidMem  = ReclaimVidMemCb;
    vamCreateIn.callbacks.needPTB        = NeedPtbCb;

    vamCreateIn.VARangeStart = memProps.vaStart;
    vamCreateIn.VARangeEnd   = memProps.vaEnd;
    vamCreateIn.bigKSize     = static_cast<uint32>(memProps.fragmentSize);
    vamCreateIn.PTBSize      = static_cast<uint32>(m_ptbSize);
    vamCreateIn.hSyncObj     = nullptr;

    // Create the VAM library instance.
    m_hVamInstance = VAMCreate(this, &vamCreateIn);
    if (m_hVamInstance == nullptr)
    {
        PAL_ALERT_ALWAYS();
        result = Result::ErrorInitializationFailed;
    }

    return result;
}

// =====================================================================================================================
// Vam manager finalize
Result VamMgr::Finalize(
    Pal::Device* pDevice)
{
    Result result = Result::Success;

    MutexAuto lock(&m_mutex);

    if (m_hVamInstance == nullptr)
    {
        result = LateInit(pDevice);

        if (result == Result::Success)
        {
            result = Pal::VamMgr::Finalize(pDevice);

            if (result != Result::Success)
            {
                Pal::VamMgr::Cleanup(pDevice);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Assigns a GPU virtual address for the specified allocation.
Result VamMgr::AssignVirtualAddress(
    Pal::Device*               pDevice,
    const VirtAddrAssignInfo&  vaInfo,
    gpusize*                   pGpuVirtAddr)  // [in/out] In: Zero, or the desired VA. Out: The assigned VA.
{
    Result result = Result::Success;

    VAM_ALLOC_INPUT  vamAllocIn  = { };
    VAM_ALLOC_OUTPUT vamAllocOut = { };

    vamAllocIn.virtualAddress = *pGpuVirtAddr;
    vamAllocIn.sizeInBytes    = vaInfo.size;
    vamAllocIn.alignment      = Max(LowPart(vaInfo.alignment), MinVamAllocAlignment);

    // VAM takes a 32-bit alignment so the high part needs to be zero.
    PAL_ASSERT(HighPart(vaInfo.alignment) == 0);

    vamAllocIn.hSection = m_hSection[static_cast<uint32>(vaInfo.partition)];
    PAL_ASSERT(vamAllocIn.hSection != nullptr);

    MutexAuto lock(&m_mutex);

    if (VAMAlloc(m_hVamInstance, &vamAllocIn, &vamAllocOut) == VAM_OK)
    {
        // Applications are expected to size-align their allocations to the largest size-alignment amongst the
        // heaps they want the allocation to go into.
        PAL_ASSERT(vamAllocOut.actualSize == vamAllocIn.sizeInBytes);

        // If the caller had a particular VA in mind we should make sure VAM gave it to us.
        PAL_ASSERT((*pGpuVirtAddr == 0) || (*pGpuVirtAddr == vamAllocOut.virtualAddress));

        *pGpuVirtAddr = vamAllocOut.virtualAddress;
    }
    else
    {
        result = Result::ErrorOutOfGpuMemory;
    }

    return result;
}

// =====================================================================================================================
// Unmaps a previously-allocated GPU virtual address described by the associated GPU memory object. This is called when
// allocations are destroyed.
//
// On Linux, since we don't use an unmap-info buffer, we ask VAM to free the unmapped address immediately.
Result VamMgr::FreeVirtualAddress(
    Pal::Device*          pDevice,
    const Pal::GpuMemory* pGpuMemory)
{
    Result result = Result::Success;

    PAL_ASSERT((pGpuMemory != nullptr) &&
               IsVamPartition(pGpuMemory->VirtAddrPartition()));

    VAM_FREE_INPUT vamFreeIn = { };
    vamFreeIn.virtualAddress = pGpuMemory->Desc().gpuVirtAddr;
    vamFreeIn.actualSize     = pGpuMemory->Desc().size;
    vamFreeIn.hSection       = m_hSection[static_cast<uint32>(pGpuMemory->VirtAddrPartition())];

    MutexAuto lock(&m_mutex);

    if (VAMFree(m_hVamInstance, &vamFreeIn) != VAM_OK)
    {
        PAL_ASSERT_ALWAYS();
        result = Result::ErrorOutOfGpuMemory;
    }

    return result;
}

// =====================================================================================================================
// Creates a GPU memory object for a page table block.  This method is protected by VAM's use of m_vamSyncObj.
Result VamMgr::AllocPageTableBlock(
    VAM_VIRTUAL_ADDRESS ptbBaseVirtAddr, // Base GPU VA the new PTB will map.
    VAM_PTB_HANDLE*     phPtbAlloc)
{
    // On Linux, kernel allocates and manages the PTB and PD allocations, so we don't need to allocate anything here.
    // Just give VAM back a dummy pointer so it doesn't complain about a null return value.
    *phPtbAlloc = this;
    return Result::Success;
}

// =====================================================================================================================
// Destroys the specified page table block GPU memory allocation.  This method is protected by VAM's use of
// m_vamSyncObj.
void VamMgr::FreePageTableBlock(
    VAM_PTB_HANDLE hPtbAlloc)
{
    // On Linux, CMM/QS allocates and manages the PTB and PD allocations, so we don't need to deallocate anything here.
    // Just make sure the handle VAM is giving us is the "dummy" pointer we returned from AllocPageTableBlock().
    PAL_ASSERT(hPtbAlloc == this);
}

// =====================================================================================================================
// Returns true if the VA partition may be managed by VAM.
bool VamMgr::IsVamPartition(
    VaPartition vaPartition
    ) const
{
    // Call the singleton, which controls the VA partition reservations
    return VamMgrSingleton::IsVamPartition(vaPartition);
}

// =====================================================================================================================
// Vam manager object cleanup.
Result VamMgr::Cleanup(
    Pal::Device* pDevice)
{
    FreeReservedVaRanges(static_cast<Device*>(pDevice));

    return Pal::VamMgr::Cleanup(pDevice);
}

// =====================================================================================================================
// VAM system memory allocation callback.
void* VAM_STDCALL VamMgr::AllocSysMemCb(
    VAM_CLIENT_HANDLE hClient,
    uint32            sizeInBytes)
{
    Util::AllocCallbacks allocCb = {};
    GetDefaultAllocCb(&allocCb);
    return allocCb.pfnAlloc(hClient, sizeInBytes, PAL_DEFAULT_MEM_ALIGN, AllocInternal);
}

// =====================================================================================================================
// VAM system memory free callback.
VAM_RETURNCODE VAM_STDCALL VamMgr::FreeSysMemCb(
    VAM_CLIENT_HANDLE hClient,
    void*             pAddress)
{
    Util::AllocCallbacks allocCb = {};
    GetDefaultAllocCb(&allocCb);
    allocCb.pfnFree(hClient, pAddress);
    return VAM_OK;
}

// =====================================================================================================================
// VAM callback to enter the specified critical section.
VAM_RETURNCODE VAM_STDCALL VamMgr::AcquireSyncObjCb(
    VAM_CLIENT_HANDLE     hClient,
    VAM_ACQSYNCOBJ_INPUT* pAcqSyncObjIn)
{
    return VAM_OK;
}

// =====================================================================================================================
// VAM callback to leave the specified critical section.
void VAM_STDCALL VamMgr::ReleaseSyncObjCb(
    VAM_CLIENT_HANDLE     hClient,
    VAM_SYNCOBJECT_HANDLE hSyncObj)
{
}

// =====================================================================================================================
// VAM callback to allocate GPU memory for a page table block.
VAM_PTB_HANDLE VAM_STDCALL VamMgr::AllocPtbCb(
    VAM_CLIENT_HANDLE    hClient,
    VAM_VIRTUAL_ADDRESS  ptbBaseVirtAddr,
    VAM_RETURNCODE*const pResult)
{
    VamMgr*const pVamMgr = static_cast<VamMgr*>(hClient);
    PAL_ASSERT(pVamMgr != nullptr);

    // This is called by VAM to tell the client to allocate a single PTB in GPU memory.  The client knows the PTB size
    // and alignment.  ptbBaseVA is the starting GPU virtual address which the new PTB will map.

    // A pointer to the PTB GPU memory object is returned to VAM as a handle.
    VAM_PTB_HANDLE pPtbGpuMem = nullptr;
    Result res = pVamMgr->AllocPageTableBlock(ptbBaseVirtAddr, &pPtbGpuMem);
    switch (res)
    {
    case Result::Success:
        *pResult = VAM_OK;
        break;
    case Result::ErrorOutOfMemory:
        *pResult = VAM_OUTOFMEMORY;
        break;
    case Result::ErrorOutOfGpuMemory:
        *pResult = VAM_PTBALLOCFAILED;
        break;
    default:
        *pResult = VAM_ERROR;
        break;
    }
    return static_cast<VAM_PTB_HANDLE>(pPtbGpuMem);
}

// =====================================================================================================================
// VAM callback to free GPU memory for a page table block.
VAM_RETURNCODE VAM_STDCALL VamMgr::FreePtbCb(
    VAM_CLIENT_HANDLE hClient,
    VAM_PTB_HANDLE    hPtbAlloc)
{
    VamMgr*const pVamMgr = static_cast<VamMgr*>(hClient);
    PAL_ASSERT(pVamMgr != nullptr);

    pVamMgr->FreePageTableBlock(hPtbAlloc);
    return VAM_OK;
}

// =====================================================================================================================
// VAM callback to allocate GPU memory for a raft block.  Suballocation is not supported by PAL, so this is never
// expected to be called.
VAM_VIDMEM_HANDLE VAM_STDCALL VamMgr::AllocVidMemCb(
    VAM_CLIENT_HANDLE      hClient,
    VAM_ALLOCVIDMEM_INPUT* pAllocVidMemIn)
{
    PAL_NEVER_CALLED();
    return nullptr;
}

// =====================================================================================================================
// VAM callback to free GPU memory for a raft block.  Suballocation is not supported by PAL, so this is never expected
// to be called.
VAM_RETURNCODE VAM_STDCALL VamMgr::FreeVidMemCb(
    VAM_CLIENT_HANDLE hClient,
    VAM_VIDMEM_HANDLE hVidMem)
{
    PAL_NEVER_CALLED();
    return VAM_ERROR;
}

// =====================================================================================================================
// VAM callback to offer a raft block's GPU memory.  Suballocation is not supported by PAL, so no raft blocks should
// never be offered or reclaimed.
VAM_RETURNCODE VAM_STDCALL VamMgr::OfferVidMemCb(
    VAM_CLIENT_HANDLE hClient,
    VAM_VIDMEM_HANDLE hVidMem)
{
    PAL_NEVER_CALLED();
    return VAM_ERROR;
}

// =====================================================================================================================
// VAM callback to reclaim a raft block's GPU memory.  Suballocation is not supported by PAL, so no raft blocks should
// never be offered or reclaimed.
VAM_RETURNCODE VAM_STDCALL VamMgr::ReclaimVidMemCb(
    VAM_CLIENT_HANDLE hClient,
    VAM_VIDMEM_HANDLE hVidMem)
{
    PAL_NEVER_CALLED();
    return VAM_ERROR;
}

// =====================================================================================================================
// Callback function to check if PTB management is needed. Returns VAM_OK if PTB management is needed.
VAM_RETURNCODE VAM_STDCALL VamMgr::NeedPtbCb()
{
    return VAM_OK;
}

// =====================================================================================================================
// Free va range if it's not null.
void VamMgr::FreeReservedVaRanges(
    Device* pDevice)
{
    for (uint32 partIndex = 0; partIndex < static_cast<uint32>(VaPartition::Count); partIndex++)
    {
        if (m_vaRangeInfo[partIndex].allocatedVa != nullptr)
        {
            pDevice->FreeVaRange(m_vaRangeInfo[partIndex].allocatedVa);
            m_vaRangeInfo[partIndex].allocatedVa     = nullptr;
            m_vaRangeInfo[partIndex].baseVirtualAddr = 0;
            m_vaRangeInfo[partIndex].size            = 0;
        }
    }
}

// =====================================================================================================================
// Allocate va range if it shall be shared across amdgpu drm device
Result VamMgr::AllocateVaRange(
    const Device* pDevice,
    VaPartition   vaPartition,
    gpusize       vaStart,
    gpusize       vaSize)
{
    Result result = Result::Success;

    PAL_ASSERT(IsVamPartition(vaPartition));

    MutexAuto    lock(&m_mutex);
    const uint32 index = static_cast<uint32>(vaPartition);

    if (m_vaRangeInfo[index].allocatedVa != nullptr)
    {
        if (m_vaRangeInfo[index].baseVirtualAddr != vaStart)
        {
            result = Result::ErrorOutOfGpuMemory;
        }
        else
        {
            PAL_ASSERT(m_vaRangeInfo[index].size == vaSize);
        }
    }
    else
    {
        result = pDevice->AllocVaRange(vaSize,
                                       vaStart,
                                       &m_vaRangeInfo[index].baseVirtualAddr,
                                       &m_vaRangeInfo[index].allocatedVa);
        if (result == Result::Success)
        {
            if (vaStart == m_vaRangeInfo[index].baseVirtualAddr)
            {
                m_vaRangeInfo[index].size = vaSize;
            }
            else
            {
                PAL_ASSERT_ALWAYS();
                result = Result::ErrorOutOfGpuMemory;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Add buffer object information into shared bo map.
bool VamMgr::AddToSharedBoMap(
    amdgpu_bo_handle hBuffer,
    amdgpu_va_handle hVaRange,
    gpusize          gpuVirtAddr)
{
    bool added = false;

    MutexAuto     lock(&m_mutex);
    SharedBoInfo* pInfo = m_sharedBoMap.FindKey(hBuffer);

    if (pInfo != nullptr)
    {
        // It's possible that single buffer object contains multiple va ranges
        // Shared bo map only records one va range for one buffer object
        if (pInfo->hVaRange == hVaRange)
        {
            PAL_ASSERT(pInfo->gpuVirtAddr == gpuVirtAddr);
            pInfo->refCount++;
            added = true;
        }
    }
    else
    {
        SharedBoInfo info = {};
        info.refCount    = 1;
        info.gpuVirtAddr = gpuVirtAddr;
        info.hVaRange    = hVaRange;

        if (m_sharedBoMap.Insert(hBuffer, info) == Result::Success)
        {
            added = true;
        }
    }
    return added;
}

// =====================================================================================================================
// Remove buffer object from shared bo map.
bool VamMgr::RemoveFromSharedBoMap(
    amdgpu_bo_handle hBuffer)
{
    bool removed = false;

    MutexAuto     lock(&m_mutex);
    SharedBoInfo* pInfo = m_sharedBoMap.FindKey(hBuffer);

    PAL_ASSERT(pInfo != nullptr);

    pInfo->refCount--;

    if (pInfo->refCount == 0)
    {
        m_sharedBoMap.Erase(hBuffer);
        removed = true;
    }

    return removed;
}

// =====================================================================================================================
// Search bo handle in shared bo map. Increase reference count if the bo handle is already in hash map.
amdgpu_va_handle VamMgr::SearchSharedBoMap(
    amdgpu_bo_handle hBuffer,
    gpusize*         pGpuVirtAddr)
{
    amdgpu_va_handle hVaRange = nullptr;

    MutexAuto     lock(&m_mutex);
    SharedBoInfo* pInfo = m_sharedBoMap.FindKey(hBuffer);

    if (pInfo != nullptr)
    {
        pInfo->refCount++;

        *pGpuVirtAddr = pInfo->gpuVirtAddr;
        hVaRange      = pInfo->hVaRange;
    }
    return hVaRange;
}

constexpr uint32        InitialGpuNumber = 32;
static VamMgrSingleton* pVamMgrSingleton = nullptr;

// =====================================================================================================================
VamMgrSingleton::VamMgrSingleton()
    :
    m_mapAllocator(),
    m_vamMgrMap(InitialGpuNumber, &m_mapAllocator),
    m_mutex()
{
}

// =====================================================================================================================
VamMgrSingleton::~VamMgrSingleton()
{
}

// =====================================================================================================================
// This function is called once before lib is unloaded from process
void  __attribute__((destructor)) palExit(void)
{
    if (pVamMgrSingleton)
    {
        GenericAllocator genericAllocator;
        PAL_DELETE(pVamMgrSingleton, &genericAllocator);
        pVamMgrSingleton = nullptr;
    }
}

// =====================================================================================================================
// Cleanup global VAM manager when one device is destroyed.
void VamMgrSingleton::Cleanup(
    Device* pDevice)
{
    PAL_ASSERT(pVamMgrSingleton != nullptr);
    MutexAuto   lock(&pVamMgrSingleton->m_mutex);
    VamMgrInfo* pVamMgrInfo = pVamMgrSingleton->m_vamMgrMap.FindKey(pDevice->DeviceHandle());

    if (pVamMgrInfo != nullptr)
    {
        if (--pVamMgrInfo->deviceRefCount == 0)
        {
            GenericAllocator genericAllocator;

            pVamMgrInfo->pVamMgr->Cleanup(pDevice);
            PAL_DELETE(pVamMgrInfo->pVamMgr, &genericAllocator);
            pVamMgrSingleton->m_vamMgrMap.Erase(pDevice->DeviceHandle());
        }
    }
}

// =====================================================================================================================
// Get vam manager object for given device
Result VamMgrSingleton::GetVamMgr(
    Device*  pDevice,
    VamMgr** ppVamMgr)
{
    Result result = Result::Success;

    PAL_ASSERT(pVamMgrSingleton != nullptr);
    MutexAuto lock(&pVamMgrSingleton->m_mutex);

    amdgpu_device_handle devHandle   = pDevice->DeviceHandle();
    VamMgrInfo*          pVamMgrInfo = pVamMgrSingleton->m_vamMgrMap.FindKey(devHandle);

    if (pVamMgrInfo == nullptr)
    {
        GenericAllocator genericAllocator;

        VamMgrInfo vamMgrInfo = {};
        VamMgr*    pVamMgr    = PAL_NEW(VamMgr, &genericAllocator, AllocInternal);

        if (pVamMgr != nullptr)
        {
            result = pVamMgr->EarlyInit();

            if (result == Result::Success)
            {
                vamMgrInfo.pVamMgr        = pVamMgr;
                vamMgrInfo.deviceRefCount = 1;
                result = pVamMgrSingleton->m_vamMgrMap.Insert(devHandle, vamMgrInfo);
            }

            if (result == Result::Success)
            {
                *ppVamMgr = pVamMgr;
            }
            else
            {
                PAL_DELETE(pVamMgr, &genericAllocator);
            }
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }
    else
    {
        pVamMgrInfo->deviceRefCount++;
        *ppVamMgr = pVamMgrInfo->pVamMgr;
    }

    return result;
}

// =====================================================================================================================
// Initialize global VAM manager when one device is created.
Result VamMgrSingleton::Init()
{
    Result result = Result::Success;

    // One time initialization of global variables.
    static uint32 s_initialized = 0;
    if (AtomicCompareAndSwap(&s_initialized, 0, 1) == 0)
    {
        if (pVamMgrSingleton == nullptr)
        {
            GenericAllocator genericAllocator;
            pVamMgrSingleton = PAL_NEW(VamMgrSingleton, &genericAllocator, AllocInternal);
            if(pVamMgrSingleton == nullptr)
            {
                result = Result::ErrorInitializationFailed;
            }
        }
        if (result == Result::Success)
        {
            pVamMgrSingleton->m_vamMgrMap.Init();
        }
        if (result == Result::Success)
        {
            std::atomic_thread_fence(std::memory_order_acq_rel);
            s_initialized ++;
        }
    }
    if (result == Result::Success)
    {
        // s_initialized is 2 indicates the one time initialization is finished.
        while (s_initialized != 2)
        {
            YieldThread();
        }
    }
    return result;
}

// =====================================================================================================================
// Return true for the partitions that may be reserved by VamMgrSingleton
bool VamMgrSingleton::IsVamPartition(
    VaPartition vaPartition)
{
    return ((vaPartition == VaPartition::DescriptorTable)       ||
            (vaPartition == VaPartition::ShadowDescriptorTable) ||
            (vaPartition == VaPartition::CaptureReplay));
}

} // Amdgpu
} // Pal
