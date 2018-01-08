/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/vamMgr.h"
#include "core/os/lnx/lnxPlatform.h"
#include "palMutex.h"
#include "palSysMemory.h"

namespace Pal
{

namespace Linux
{

class Device;

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
class VamMgr : public Pal::VamMgr
{
public:
    VamMgr();
    virtual ~VamMgr();

    virtual Result LateInit(
        Pal::Device*const pDevice) override;

    virtual Result Finalize(
        Pal::Device*const pDevice) override;

    virtual Result AssignVirtualAddress(
        Pal::Device*const         pDevice,
        const VirtAddrAssignInfo& vaInfo,
        gpusize*                  pGpuVirtAddr) override;

    virtual void FreeVirtualAddress(
        Pal::Device*const     pDevice,
        const Pal::GpuMemory& gpuMemory) override;

    virtual Result Cleanup(
        Pal::Device*const pDevice) override;

    bool IsAllocated() const { return m_allocated; }

protected:
    void*  AllocPageTableBlock(VAM_VIRTUAL_ADDRESS ptbBaseVirtAddr);
    void   FreePageTableBlock(VAM_PTB_HANDLE hPtbAlloc);

    // VAM callbacks.
    static void*             VAM_STDCALL AllocSysMemCb(VAM_CLIENT_HANDLE hPal, uint32 sizeInBytes);
    static VAM_RETURNCODE    VAM_STDCALL FreeSysMemCb(VAM_CLIENT_HANDLE hPal, void* pAddress);
    static VAM_RETURNCODE    VAM_STDCALL AcquireSyncObjCb(VAM_CLIENT_HANDLE hPal, VAM_ACQSYNCOBJ_INPUT* pAcqSyncObjIn);
    static void              VAM_STDCALL ReleaseSyncObjCb(VAM_CLIENT_HANDLE hPal, VAM_SYNCOBJECT_HANDLE hSyncObj);
    static VAM_PTB_HANDLE    VAM_STDCALL AllocPtbCb(VAM_CLIENT_HANDLE hPal, VAM_VIRTUAL_ADDRESS ptbBaseVirtAddr);
    static VAM_RETURNCODE    VAM_STDCALL FreePtbCb(VAM_CLIENT_HANDLE hPal, VAM_PTB_HANDLE hPtbAlloc);
    static VAM_VIDMEM_HANDLE VAM_STDCALL AllocVidMemCb(VAM_CLIENT_HANDLE hPal, VAM_ALLOCVIDMEM_INPUT* pAllocVidMemIn);
    static VAM_RETURNCODE    VAM_STDCALL FreeVidMemCb(VAM_CLIENT_HANDLE hPal, VAM_VIDMEM_HANDLE hVidMem);
    static VAM_RETURNCODE    VAM_STDCALL OfferVidMemCb(VAM_CLIENT_HANDLE hPal, VAM_VIDMEM_HANDLE hVidMem);
    static VAM_RETURNCODE    VAM_STDCALL ReclaimVidMemCb(VAM_CLIENT_HANDLE hPal, VAM_VIDMEM_HANDLE hVidMem);
    static VAM_RETURNCODE    VAM_STDCALL NeedPtbCb();

private:
    bool    m_allocated;

    PAL_DISALLOW_COPY_AND_ASSIGN(VamMgr);
};

// =====================================================================================================================
// ReservedVaRangeInfo holds information about reserved ranges on the physical GPU device. New logical devices can
// retrieve this information without extra reservations.
struct ReservedVaRangeInfo
{
    gpusize          baseVirtualAddr[static_cast<uint32>(VaPartition::Count)];  // Virtual base address of the range
    amdgpu_va_handle allocatedVa[static_cast<uint32>(VaPartition::Count)];      // Handles of each allocated VAs
    uint32           devCounter;                                                // Number of allocated logical devices
};

// =====================================================================================================================
// VamMgrSingleton is a global container of VamMgr.
// All Pal devices must share VAs, otherwise the VAs will be used up in the beginning
// since each device will allocate two dedicated VAs for descriptor and shadow descriptor.
// VamMgrSingleton keeps one global VamMgr instance, manage its life cycle and provide thread-safe access.
class VamMgrSingleton
{
public:
    static void Cleanup();
    static void Init();

    static Result InitVaRangesAndFinalizeVam(
        Pal::Linux::Device* pDevice);

    static Result AssignVirtualAddress(
        Pal::Device*              pDevice,
        const VirtAddrAssignInfo& vaInfo,
        gpusize*                  pGpuVirtAddr);

    static void FreeVirtualAddress(
        Pal::Device*          pDevice,
        const Pal::GpuMemory& gpuMemory);

    static Result GetReservedVaRange(
        const DrmLoaderFuncs& drmFuncs,
        amdgpu_device_handle  devHandle,
        bool                  isDtifEnabled,
        GpuMemoryProperties*  memoryProperties);

    static void FreeReservedVaRange(
        const DrmLoaderFuncs& drmFuncs,
        amdgpu_device_handle  devHandle);

private:
    typedef Util::HashMap<amdgpu_device_handle, ReservedVaRangeInfo, GenericAllocatorAuto> ReservedVaMap;
    static constexpr uint32     InitialGpuNumber = 32;
    static GenericAllocatorAuto s_mapAllocator;
    static ReservedVaMap        s_reservedVaMap;
    static Util::Mutex          s_vaMapLock;
    static Util::Mutex          s_mutex;
    static volatile uint32      s_refCount;
    static VamMgr               s_vammgr;
};

} // Linux

} // Pal
