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

#pragma once

#include "core/vamMgr.h"
#include "core/os/lnx/lnxPlatform.h"
#include "palMutex.h"

namespace Pal
{

namespace Linux
{

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

    virtual Result EarlyInit(
        AmdgpuVaRangeAlloc pfnAlloc,
        AmdgpuVaRangeFree  pfnFree);

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

    virtual void GetVaRangeInfo(
        uint32       partIndex,
        VaRangeInfo* pVaRange);
    virtual bool IsAllocated() { return m_allocated; }
protected:
    void*  AllocPageTableBlock(VAM_VIRTUAL_ADDRESS ptbBaseVirtAddr);
    void   FreePageTableBlock(VAM_PTB_HANDLE hPtbAlloc);

    // DRM Methods of VA management.
    AmdgpuVaRangeAlloc m_pfnAlloc;
    AmdgpuVaRangeFree  m_pfnFree;
    AmdgpuVaRangeQuery m_pfnQuery;

    // Handle of each allocated VAs.
    amdgpu_va_handle   m_allocatedVa[static_cast<uint32>(VaPartition::Count)];
    // Size and start address of each allocated VA.
    VaRangeInfo        m_vaRangeInfo[static_cast<uint32>(VaPartition::Count)];
    bool               m_allocated;

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

    size_t  m_maxPtbIndex;      // Maximum possible PTB index.
    size_t  m_ptbIndexShift;    // Bits to right-shift when converting a VA to a PTB index.

    PAL_DISALLOW_COPY_AND_ASSIGN(VamMgr);
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
    static void Init(const DrmLoaderFuncs& drmFuncs);

    static Result InitVaRangesAndFinalizeVam(
        Pal::Device* pDevice);

    static Result AssignVirtualAddress(
        Pal::Device*              pDevice,
        const VirtAddrAssignInfo& vaInfo,
        gpusize*                  pGpuVirtAddr);

    static void FreeVirtualAddress(
        Pal::Device*          pDevice,
        const Pal::GpuMemory& gpuMemory);

private:
    static Util::Mutex        s_mutex;
    static volatile uint32    s_refCount;
    static VamMgr             s_vammgr;
};

} // Linux

} // Pal
