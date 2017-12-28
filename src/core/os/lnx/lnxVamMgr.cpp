/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/os/lnx/lnxVamMgr.h"
#include "core/os/lnx/lnxDevice.h"
#include "palSysUtil.h"

using namespace Util;

namespace Pal
{

namespace Linux
{

constexpr gpusize _1GB = (1uLL << 30u);
constexpr gpusize _4GB = (1ull << 32u);

// Indicates a type of VA partition and the VA size required.
struct VaSchema
{
    VaPartition vaType;
    gpusize     vaSize;
};

// =====================================================================================================================
// Note that this constructor is invoked before settings have been committed.
VamMgr::VamMgr()
    :
    Pal::VamMgr(),
    m_pfnAlloc(nullptr),
    m_pfnFree(nullptr),
    m_allocated(false)
{
    memset(m_allocatedVa, 0, sizeof(m_allocatedVa));
    memset(m_vaRangeInfo, 0, sizeof(m_vaRangeInfo));
}

// =====================================================================================================================
VamMgr::~VamMgr()
{
// Note: OCL API doesn't provide explicit device destruction
    // The VAM instance must be destroyed by calling Cleanup().
    PAL_ASSERT(m_hVamInstance == nullptr);
}

// =====================================================================================================================
Result VamMgr::EarlyInit(
    AmdgpuVaRangeAlloc pfnAlloc,
    AmdgpuVaRangeFree  pfnFree)
{
    m_pfnAlloc = pfnAlloc;
    m_pfnFree  = pfnFree;
    return Result::Success;
}

// =====================================================================================================================
// Performs any late-stage initialization that can only be done after settings have been committed.
// - Starts up the VAM library.
Result VamMgr::LateInit(
    Pal::Device*const pDevice)
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
Result VamMgr::Finalize(
    Pal::Device*const pDevice)
{
    Device* pLinuxDevice = static_cast<Linux::Device*>(pDevice);
    Result  result       = Result::Success;

    // The default schema of pre-allocated VA partitions.
    VaSchema VaDefault[static_cast<uint32>(VaPartition::Count)] = {
                                                                    {VaPartition::Default, 0},
                                                                    {VaPartition::DefaultBackup, 0},
                                                                    {VaPartition::DescriptorTable, _4GB},
                                                                    {VaPartition::ShadowDescriptorTable, _4GB},
                                                                    {VaPartition::Svm, 0}
                                                                   };
    if (m_allocated == false)
    {
        int32 ret = 0;
        for (uint32 partIndex = 0; partIndex < static_cast<uint32>(VaPartition::Count); partIndex++)
        {
            if (VaDefault[partIndex].vaSize > 0)
            {
                ret |= m_pfnAlloc(
                           pLinuxDevice->DeviceHandle(),
                           amdgpu_gpu_va_range_general,
                           VaDefault[partIndex].vaSize,
                           _4GB,
                           0,
                           &m_vaRangeInfo[static_cast<uint32>(VaDefault[partIndex].vaType)].baseVirtAddr,
                           &m_allocatedVa[static_cast<uint32>(VaDefault[partIndex].vaType)],
                           0);
                m_vaRangeInfo[static_cast<uint32>(VaDefault[partIndex].vaType)].size = VaDefault[partIndex].vaSize;
	    }
	}
	if (ret != 0)
        {
            for (uint32 partIndex = 0; partIndex < static_cast<uint32>(VaPartition::Count); partIndex++)
            {
                if (m_allocatedVa[partIndex] != nullptr)
                {
                    m_pfnFree(m_allocatedVa[partIndex]);
                    m_allocatedVa[partIndex] = nullptr;
                }
            }
            memset(&m_vaRangeInfo, 0, sizeof(m_vaRangeInfo));
            result = Result::ErrorOutOfMemory;
        }
    }

    if (result == Result::Success)
    {
        // Set VA configuration back to Pal Device.
        for (uint32 partIndex = 0; partIndex < static_cast<uint32>(VaPartition::Count); partIndex++)
        {
            pLinuxDevice->SetVaRangeInfo(partIndex, &m_vaRangeInfo[partIndex]);
        }
        result = Pal::VamMgr::Finalize(pDevice);
    }

    m_allocated = (result == Result::Success);

    return result;
}

// =====================================================================================================================
Result VamMgr::Cleanup(
    Pal::Device*const pDevice)
{
    Pal::VamMgr::Cleanup(pDevice);

    for (uint32 partIndex = 0; partIndex < static_cast<uint32>(VaPartition::Count); partIndex++)
    {
        if (m_allocatedVa[partIndex] != nullptr)
        {
            m_pfnFree(m_allocatedVa[partIndex]);
            m_allocatedVa[partIndex] = nullptr;
        }
    }
    memset(&m_vaRangeInfo, 0, sizeof(m_vaRangeInfo));
    m_allocated = false;
    return Result::Success;
}

// =====================================================================================================================
void VamMgr::GetVaRangeInfo(
    uint32       partIndex,
    VaRangeInfo* pVaRange)
{
    PAL_ASSERT(m_allocated && (pVaRange != nullptr) && (partIndex < static_cast<uint32>(VaPartition::Count)));
    *pVaRange = m_vaRangeInfo[partIndex];
}

// =====================================================================================================================
// Assigns a GPU virtual address for the specified allocation.
Result VamMgr::AssignVirtualAddress(
    Pal::Device*const         pDevice,
    const VirtAddrAssignInfo& vaInfo,
    gpusize*                  pGpuVirtAddr)  // [in/out] In: Zero, or the desired VA. Out: The assigned VA.
{
    Result result = Result::ErrorInvalidFlags;

    VAM_ALLOC_INPUT  vamAllocIn  = { };
    VAM_ALLOC_OUTPUT vamAllocOut = { };

    vamAllocIn.virtualAddress = *pGpuVirtAddr;
    vamAllocIn.sizeInBytes    = vaInfo.size;
    vamAllocIn.alignment      = Max(LowPart(vaInfo.alignment), MinVamAllocAlignment);

    // VAM takes a 32-bit alignment so the high part needs to be zero.
    PAL_ASSERT(HighPart(vaInfo.alignment) == 0);

    const VaPartition partition = pDevice->ChooseVaPartition(vaInfo.range);
    vamAllocIn.hSection = m_hSection[static_cast<uint32>(partition)];
    PAL_ASSERT(vamAllocIn.hSection != nullptr);

    if (VAMAlloc(m_hVamInstance, &vamAllocIn, &vamAllocOut) == VAM_OK)
    {
        result = Result::Success;
    }
    else
    {
        result = Result::ErrorOutOfGpuMemory;
    }

    if (result == Result::Success)
    {
        // Applications are expected to size-align their allocations to the largest size-alignment amongst the
        // heaps they want the allocation to go into.
        PAL_ASSERT(vamAllocOut.actualSize == vamAllocIn.sizeInBytes);

        // If the caller had a particular VA in mind we should make sure VAM gave it to us.
        PAL_ASSERT((*pGpuVirtAddr == 0) || (*pGpuVirtAddr == vamAllocOut.virtualAddress));

        (*pGpuVirtAddr) = vamAllocOut.virtualAddress;
    }

    return result;
}

// =====================================================================================================================
// Unmaps a previously-allocated GPU virtual address described by the associated GPU memory object. This is called when
// allocations are destroyed.
//
// On Linux, since we don't use an unmap-info buffer, we ask VAM to free the unmapped address immediately.
void VamMgr::FreeVirtualAddress(
    Pal::Device*const     pDevice,
    const Pal::GpuMemory& gpuMemory)
{
    VAM_FREE_INPUT vamFreeIn = { };

    vamFreeIn.virtualAddress = gpuMemory.Desc().gpuVirtAddr;
    vamFreeIn.actualSize     = gpuMemory.Desc().size;

    for (uint32 i = 0; i < static_cast<uint32>(VaPartition::Count); ++i)
    {
        const auto& vaRange = pDevice->MemoryProperties().vaRange[i];

        if ((vaRange.baseVirtAddr <= vamFreeIn.virtualAddress) &&
            ((vaRange.baseVirtAddr + vaRange.size) >= (vamFreeIn.virtualAddress + vamFreeIn.actualSize)))
        {
            vamFreeIn.hSection = m_hSection[i];
            break;
        }
    }

    if (VAMFree(m_hVamInstance, &vamFreeIn) != VAM_OK)
    {
        PAL_ASSERT_ALWAYS();
    }
}

// =====================================================================================================================
// Creates a GPU memory object for a page table block.  This method is protected by VAM's use of m_vamSyncObj.
void* VamMgr::AllocPageTableBlock(
    VAM_VIRTUAL_ADDRESS ptbBaseVirtAddr)  // Base GPU VA the new PTB will map.
{
    // On Linux, kernel allocates and manages the PTB and PD allocations, so we don't need to allocate anything here.
    // Just give VAM back a dummy pointer so it doesn't complain about a null return value.
    return this;
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
    VAM_CLIENT_HANDLE   hClient,
    VAM_VIRTUAL_ADDRESS ptbBaseVirtAddr)
{
    VamMgr*const pVamMgr = static_cast<VamMgr*>(hClient);
    PAL_ASSERT(pVamMgr != nullptr);

    // This is called by VAM to tell the client to allocate a single PTB in GPU memory.  The client knows the PTB size
    // and alignment.  ptbBaseVA is the starting GPU virtual address which the new PTB will map.

    // A pointer to the PTB GPU memory object is returned to VAM as a handle.
    void*const pPtbGpuMem = pVamMgr->AllocPageTableBlock(ptbBaseVirtAddr);
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

Util::Mutex          VamMgrSingleton::s_mutex;
volatile uint32      VamMgrSingleton::s_refCount    = 0;
VamMgr               VamMgrSingleton::s_vammgr;

// =====================================================================================================================
// Cleanup global VAM manager when one device is destroyed.
void VamMgrSingleton::Cleanup()
{
    s_mutex.Lock();
    s_refCount--;
    if (s_refCount == 0)
    {
        s_vammgr.Cleanup(nullptr);
    }
    s_mutex.Unlock();
}

// =====================================================================================================================
// Allocate VA from base driver and initialize global VAM manager.
Result VamMgrSingleton::InitVaRangesAndFinalizeVam(
    Pal::Device* const pDevice)
{
    Result result = Result::Success;
    Device* pLinuxDevice = static_cast<Linux::Device*>(pDevice);
    s_mutex.Lock();
    if (s_vammgr.IsAllocated())
    {
        // Set VA configuration back to Pal Device.
        for (uint32 partIndex = 0; partIndex < static_cast<uint32>(VaPartition::Count); partIndex++)
        {
            VaRangeInfo vaRange;
            s_vammgr.GetVaRangeInfo(partIndex, &vaRange);
            pLinuxDevice->SetVaRangeInfo(partIndex, &vaRange);
        }
    }
    else
    {
        result = s_vammgr.LateInit(pDevice);
        if (result == Result::Success)
        {
            result = s_vammgr.Finalize(pDevice);
        }
    }
    s_mutex.Unlock();
    return result;
}

// =====================================================================================================================
// Initialize global VAM manager when one device is created.
void VamMgrSingleton::Init(
    const DrmLoaderFuncs& drmFuncs)
{
    // One time initialization of global variables.
    static uint32 s_initialized = 0;
    if (AtomicCompareAndSwap(&s_initialized, 0, 1) == 0)
    {
        s_mutex.Init();
        MemoryBarrier();
        s_initialized ++;
    }
    // s_initialized is 2 indicates the one time initialization is finished.
    while (s_initialized != 2)
    {
        YieldThread();
    }
    s_mutex.Lock();
    s_refCount++;
    if (s_refCount == 1)
    {
        AmdgpuVaRangeAlloc pfnAlloc = drmFuncs.pfnAmdgpuVaRangeAlloc;
        AmdgpuVaRangeFree  pfnFree  = drmFuncs.pfnAmdgpuVaRangeFree;
        s_vammgr.EarlyInit(pfnAlloc, pfnFree);
    }
    s_mutex.Unlock();
}

// =====================================================================================================================
// Thread safe VA allocate function.
Result VamMgrSingleton::AssignVirtualAddress(
    Pal::Device*const              pDevice,
    const Pal::VirtAddrAssignInfo& vaInfo,
    gpusize*                       pGpuVirtAddr)
{
    s_mutex.Lock();
    Result result = s_vammgr.AssignVirtualAddress(pDevice, vaInfo, pGpuVirtAddr);
    s_mutex.Unlock();
    return result;
}

// =====================================================================================================================
// Thread safe VA free function.
void VamMgrSingleton::FreeVirtualAddress(
    Pal::Device*const     pDevice,
    const Pal::GpuMemory& gpuMemory)
{
    s_mutex.Lock();
    s_vammgr.FreeVirtualAddress(pDevice, gpuMemory);
    s_mutex.Unlock();
}

} // Linux

} // Pal
