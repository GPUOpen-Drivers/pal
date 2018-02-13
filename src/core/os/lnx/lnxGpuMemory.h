/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/gpuMemory.h"
#include "core/os/lnx/lnxHeaders.h"

namespace Pal
{
namespace Linux
{

class Device;

// =====================================================================================================================
// Unmaps the allocation out of CPU address space.
class GpuMemory : public Pal::GpuMemory
{
public:
    explicit GpuMemory(Device* pDevice);
    virtual ~GpuMemory();

    virtual Result Init(
        const GpuMemoryCreateInfo&         createInfo,
        const GpuMemoryInternalCreateInfo& internalInfo) override;

    virtual void Destroy() override;

    virtual OsExternalHandle GetSharedExternalHandle() const override;

    void             SetSurfaceHandle(amdgpu_bo_handle hBuffer) { m_hSurface = hBuffer; }
    amdgpu_bo_handle SurfaceHandle() const { return m_hSurface; }
    void             SetVaRangeHandle(amdgpu_va_handle hVaRange) { m_hVaRange = hVaRange; }
    amdgpu_va_handle VaRangeHandle() const { return m_hVaRange; }

    void             SetMarkerHandle(amdgpu_bo_handle hBuffer) { m_hMarker = hBuffer; }
    amdgpu_bo_handle MarkerHandle() const { return m_hMarker; }
    void             SetMarkerVaRangeHandle(amdgpu_va_handle hVaRange) { m_hMarkerVa = hVaRange; }
    amdgpu_va_handle MarkerVaRangeHandle() const { return m_hMarkerVa; }

    void             SetOffset(uint64 offset) { m_offset = offset; }
    uint64           Offset() const { return m_offset; }

    void GetHeapsInfo(uint32* pHeapCount, GpuHeap** ppHeaps) const;

    bool IsVmAlwaysValid() const { return m_isVmAlwaysValid; }

    Result QuerySdiBusAddress();

protected:
    virtual Result AllocateOrPinMemory(
        gpusize                 baseVirtAddr,
        uint64*                 pPagingFence,
        VirtualGpuMemAccessMode virtualAccessMode) override;
    virtual Result OpenSharedMemory() override;
    virtual Result OpenPeerMemory() override;

    virtual Result OsSetPriority(GpuMemPriority priority, GpuMemPriorityOffset priorityOffset) override;
    virtual Result OsMap(void** ppData) override;
    virtual Result OsUnmap() override;

    // SVM is not supported on PAL Linux platform
    virtual Result AllocateSvmVirtualAddress(
        gpusize baseVirtAddr,
        gpusize size,
        gpusize align,
        bool commitCpuVa) override { return Result::ErrorUnavailable; }
    virtual Result FreeSvmVirtualAddress() override { return Result::ErrorUnavailable; }

private:
    amdgpu_bo_handle m_hSurface; // Handle of allocated memory.
    amdgpu_va_handle m_hVaRange; // Handle of allocated va range.

    amdgpu_bo_handle m_hMarker; // Handle of marker.
    amdgpu_va_handle m_hMarkerVa; // Handle of marker va range.

    uint64           m_offset;   // Offset in buffer object bound. It's only meaningful when it's a virtual gpu memroy.

    bool             m_isVmAlwaysValid; // If the virtual memory is always valid.

    PAL_DISALLOW_DEFAULT_CTOR(GpuMemory);
    PAL_DISALLOW_COPY_AND_ASSIGN(GpuMemory);
};

} // Linux
} // Pal
