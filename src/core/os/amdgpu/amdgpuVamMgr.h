/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/os/amdgpu/amdgpuPlatform.h"
#include "core/vamMgr.h"
#include "palMutex.h"
#include "palSysMemory.h"

namespace Pal
{

class Device;

namespace Amdgpu
{

class Device;

// =====================================================================================================================
// ReservedVaRangeInfo holds information about reserved ranges on the physical GPU device. New logical devices can
// retrieve this information without extra reservations.
struct ReservedVaRangeInfo
{
    gpusize          baseVirtualAddr;  // Virtual base address of the range
    gpusize          size;             // Size of each allocated va range
    amdgpu_va_handle allocatedVa;      // Handles of each allocated VAs
};

// =====================================================================================================================
// SharedBoInfo holds information about shared buffer object between different PAL based drivers in single process
struct SharedBoInfo
{
    uint32           refCount;      // Reference count of the shared buffer object
    gpusize          gpuVirtAddr;   // Gpu virtual address of the shared buffer object
    amdgpu_va_handle hVaRange;      // Va range of the shared buffer object
};

// =====================================================================================================================
// VamMgr provides a clean interface between PAL and the VAM library, which is used to allocate and free GPU virtual
// address space for video memory allocations.
//
// Some commonly used abbreviations throughout the implementation of this class:
//     - VA:  Virtual address
//     - PD:  Page directory
//     - PDE: Page directory entry
//     - PTB: Page table block
//     - PTE: Page table entry
//     - UIB: Unmap info buffer
class VamMgr final : public Pal::VamMgr
{
public:
    VamMgr();
    virtual ~VamMgr();

    virtual Result EarlyInit() override;

    virtual Result Finalize(
        Pal::Device* pDevice) override;

    virtual Result AssignVirtualAddress(
        Pal::Device*              pDevice,
        const VirtAddrAssignInfo& vaInfo,
        gpusize*                  pGpuVirtAddr) override;

    virtual Result FreeVirtualAddress(
        Pal::Device*          pDevice,
        const Pal::GpuMemory* pGpuMemory) override;

    virtual bool IsVamPartition(
        VaPartition vaPartition) const override;

    virtual Result Cleanup(
        Pal::Device* pDevice) override;

    Result AllocateVaRange(
        const Device* pDevice,
        VaPartition   vaPartition,
        gpusize       vaStart,
        gpusize       vaSize);

    virtual Result LateInit(
        Pal::Device* pDevice) override;

    bool AddToSharedBoMap(
        amdgpu_bo_handle hBuffer,
        amdgpu_va_handle hVaRange,
        gpusize          gpuVirtAddr);

    bool RemoveFromSharedBoMap(
        amdgpu_bo_handle hBuffer);

    amdgpu_va_handle SearchSharedBoMap(
        amdgpu_bo_handle hBuffer,
        gpusize*         pGpuVirtAddr);

protected:
    Result AllocPageTableBlock(VAM_VIRTUAL_ADDRESS ptbBaseVirtAddr, VAM_PTB_HANDLE* hPtbAlloc) override;
    void   FreePageTableBlock(VAM_PTB_HANDLE hPtbAlloc) override;

    void FreeReservedVaRanges(
        Device* pDevice);

    // VAM callbacks.
    static void*             VAM_STDCALL AllocSysMemCb(VAM_CLIENT_HANDLE hPal, uint32 sizeInBytes);
    static VAM_RETURNCODE    VAM_STDCALL FreeSysMemCb(VAM_CLIENT_HANDLE hPal, void* pAddress);
    static VAM_RETURNCODE    VAM_STDCALL AcquireSyncObjCb(VAM_CLIENT_HANDLE hPal, VAM_ACQSYNCOBJ_INPUT* pAcqSyncObjIn);
    static void              VAM_STDCALL ReleaseSyncObjCb(VAM_CLIENT_HANDLE hPal, VAM_SYNCOBJECT_HANDLE hSyncObj);
    static VAM_PTB_HANDLE    VAM_STDCALL AllocPtbCb(
        VAM_CLIENT_HANDLE    hPal,
        VAM_VIRTUAL_ADDRESS  ptbBaseVirtAddr,
        VAM_RETURNCODE*const pResult);
    static VAM_RETURNCODE    VAM_STDCALL FreePtbCb(VAM_CLIENT_HANDLE hPal, VAM_PTB_HANDLE hPtbAlloc);
    static VAM_VIDMEM_HANDLE VAM_STDCALL AllocVidMemCb(VAM_CLIENT_HANDLE hPal, VAM_ALLOCVIDMEM_INPUT* pAllocVidMemIn);
    static VAM_RETURNCODE    VAM_STDCALL FreeVidMemCb(VAM_CLIENT_HANDLE hPal, VAM_VIDMEM_HANDLE hVidMem);
    static VAM_RETURNCODE    VAM_STDCALL OfferVidMemCb(VAM_CLIENT_HANDLE hPal, VAM_VIDMEM_HANDLE hVidMem);
    static VAM_RETURNCODE    VAM_STDCALL ReclaimVidMemCb(VAM_CLIENT_HANDLE hPal, VAM_VIDMEM_HANDLE hVidMem);
    static VAM_RETURNCODE    VAM_STDCALL NeedPtbCb();

    PAL_DISALLOW_COPY_AND_ASSIGN(VamMgr);

protected:
    typedef Util::HashMap<amdgpu_bo_handle, SharedBoInfo, Util::GenericAllocatorTracked> SharedBoMap;

    ReservedVaRangeInfo           m_vaRangeInfo[static_cast<uint32>(VaPartition::Count)];
    Util::Mutex                   m_mutex;
    Util::GenericAllocatorTracked m_mapAllocator;
    SharedBoMap                   m_sharedBoMap;

    static constexpr uint32 InitialBoCount = 8;
};

// =====================================================================================================================
// VamMgrInfo holds information of VamMgr on the physical GPU device.
// The virtual address management should be per physical device.
struct VamMgrInfo
{
    VamMgr* pVamMgr;                      // handle of va manager
    uint32  deviceRefCount;               // Number of logical devices
};

// =====================================================================================================================
// VamMgrSingleton is a global container of VamMgr.
// All Pal devices must share VAs, otherwise the VAs will be used up in the beginning
// since each device will allocate two dedicated VAs for descriptor and shadow descriptor.
// VamMgrSingleton keeps one global VamMgr instance, manage its life cycle and provide thread-safe access.
class VamMgrSingleton
{
public:
    static Result Init();

    static void Cleanup(
        Device* pDevice);

    static bool IsVamPartition(
        VaPartition vaPartition);

    static Result GetVamMgr(
        Device*  pDevice,
        VamMgr** ppVamMgr);

    ~VamMgrSingleton();
private:
    VamMgrSingleton();

    typedef Util::HashMap<amdgpu_device_handle, VamMgrInfo, Util::GenericAllocatorTracked> VamMgrMap;

    Util::GenericAllocatorTracked m_mapAllocator;
    VamMgrMap                     m_vamMgrMap;
    Util::Mutex                   m_mutex;

    PAL_DISALLOW_COPY_AND_ASSIGN(VamMgrSingleton);
};

} // Amdgpu
} // Pal
