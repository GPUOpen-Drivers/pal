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

#pragma once

#include "palMutex.h"
#include "core/device.h"
#include "vaminterface.h"

namespace Pal
{

class Device;
class GpuMemory;

// Flags reporting status of page directory resize operations.  This comes into play on GPUs like Carrizo where the
// carve-out memory is small - the page directory will start in the carve-out for best performance, but may need to be
// relocated if the process allocates too much GPU memory.
union PageDirResizeFlags
{
    struct
    {
        uint32 pageDirectoryEnlarged     :  1; // Set if the page directory has ever been successfully enlarged.
        uint32 adjustVmRangeEscapeCalled :  1; // Set if an AjustVmRange escape call has ever been successfully called.
        uint32 reserved                  : 30;
    };
    uint32 value;
};

// Contains all of the necessary information for assigning a virtual address.
struct VirtAddrAssignInfo
{
    gpusize     size;      // Size of the VA range to assign. Must be a multiple of the Device's virtual memory
                           // allocation granularity.
    gpusize     alignment; // Alignment of the VA range to assign. Must be a multiple of the Device's virtual memory
                           // allocation granularity.
    VaPartition partition; // Virtual address partition to acquire an address from.
};

// =====================================================================================================================
// VamMgr provides a clean interface between PAL and the VAM library, which is used to allocate and free GPU virtual
// address space for video memory allocations on Windows WDDM1.  In WDDM2, the Windows OS provides a different mechanism
// for VA managements.
//
// Some commonly used abbreviations throughout the implementation of this class:
//     - VA:  Virtual address
//     - PD:  Page directory
//     - PDE: Page directory entry
//     - PTB: Page table block
//     - PTE: Page table entry
//     - UIB: Unmap info buffer
class VamMgr
{
public:
    VamMgr();
    virtual ~VamMgr();

    virtual Result EarlyInit();
    virtual Result LateInit(Device* pDevice) = 0;
    virtual Result Finalize(Device* pDevice);
    virtual Result Cleanup(Device* pDevice);

    virtual Result AssignVirtualAddress(
        Device*                   pDevice,
        const VirtAddrAssignInfo& vaInfo,
        gpusize*                  pGpuVirtAddr) = 0;

    virtual Result FreeVirtualAddress(
        Device*          pDevice,
        const GpuMemory* pGpuMemory) = 0;

    static constexpr uint32 MinVamAllocAlignment = 0;

protected:
    virtual void*  AllocPageTableBlock(VAM_VIRTUAL_ADDRESS ptbBaseVirtAddr) = 0;
    virtual void   FreePageTableBlock(VAM_PTB_HANDLE hPtbAlloc) = 0;
    virtual Result QueryPendingUnmaps(bool force) { return Result::Success; }
    virtual bool   IsVamPartition(VaPartition vaPartition) const { return true; }

    gpusize CalcPtbSize(Device* pDevice) const;

    // One page table entry maps one page's worth of video memory.
    static constexpr size_t SpaceMappedPerPte = 4 * 1024;

    VAM_HANDLE  m_hVamInstance;
    // VAM section corresponding to each usable virtual address range on the GPU.
    VAM_SECTION_HANDLE m_hSection[static_cast<uint32>(VaPartition::Count)];
    gpusize m_ptbSize;          // Size of a page table block, in bytes.

    PAL_DISALLOW_COPY_AND_ASSIGN(VamMgr);
};

} // Pal
