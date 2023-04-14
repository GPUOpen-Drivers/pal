/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/os/amdgpu/amdgpuHeaders.h"

namespace Pal
{
namespace Amdgpu
{

class Device;

// All of the flags supplementally describe the traits of an amdgpu GPU memory.
union GpuMemoryFlags
{
    struct
    {
        uint32 isVmAlwaysValid     :  1; // If the virtual memory is always valid.
        uint32 isShared            :  1; // If this gpu memory object has been added into shared bo map.
        uint32 reserved            : 30;
    };
    uint32 u32All;
};

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

    Result ImportMemory(amdgpu_bo_handle_type handleType, OsExternalHandle handle);

    virtual void Destroy() override;

    virtual OsExternalHandle ExportExternalHandle(const GpuMemoryExportInfo& exportInfo) const override;

    amdgpu_bo_handle_type GetSharedExternalHandleType() const { return m_externalHandleType; }

    void             SetSurfaceHandle(amdgpu_bo_handle hBuffer) { m_hSurface = hBuffer; }
    amdgpu_bo_handle SurfaceHandle() const { return m_hSurface; }
    void             SetVaRangeHandle(amdgpu_va_handle hVaRange) { m_hVaRange = hVaRange; }
    amdgpu_va_handle VaRangeHandle() const { return m_hVaRange; }

    void             SetSurfaceKmsHandle(uint32 hBufferKms) { m_hSurfaceKms = hBufferKms; }
    uint32           SurfaceKmsHandle() const { return m_hSurfaceKms; }

    void             SetMarkerHandle(amdgpu_bo_handle hBuffer) { m_hMarker = hBuffer; }
    amdgpu_bo_handle MarkerHandle() const { return m_hMarker; }
    void             SetMarkerVaRangeHandle(amdgpu_va_handle hVaRange) { m_hMarkerVa = hVaRange; }
    amdgpu_va_handle MarkerVaRangeHandle() const { return m_hMarkerVa; }

    void             SetOffset(uint64 offset) { m_offset = offset; }
    uint64           Offset() const { return m_offset; }

    void GetHeapsInfo(uint32* pHeapCount, GpuHeap** ppHeaps) const;

    bool IsVmAlwaysValid() const { return m_amdgpuFlags.isVmAlwaysValid; }

    Result QuerySdiBusAddress();

    // Set SDI remote surface bus address and marker bus address.
    virtual Result SetSdiRemoteBusAddress(gpusize surfaceBusAddr, gpusize markerBusAddr) override;

protected:
    virtual Result AllocateOrPinMemory(
        gpusize                 baseVirtAddr,
        uint64*                 pPagingFence,
        VirtualGpuMemAccessMode virtualAccessMode,
        uint32                  multiDeviceGpuMemoryCount,
        IDevice*const*          ppDevice,
        Pal::Image*const*       ppImage) override;
    virtual Result OpenSharedMemory(OsExternalHandle handle) override;
    virtual Result OpenPeerMemory() override;

    virtual Result OsSetPriority(GpuMemPriority priority, GpuMemPriorityOffset priorityOffset) override;
    virtual Result OsMap(void** ppData) override;
    virtual Result OsUnmap() override;

    virtual Result AllocateSvmVirtualAddress(
        gpusize baseVirtAddr,
        gpusize size,
        gpusize align,
        bool commitCpuVa) override;
    virtual Result FreeSvmVirtualAddress() override;

private:
    amdgpu_bo_handle m_hSurface;  // Handle of allocated memory.
    amdgpu_va_handle m_hVaRange;  // Handle of allocated va range.

    uint32           m_hSurfaceKms; // KMS handle of allocated memory.

    amdgpu_bo_handle m_hMarker;   // Handle of marker.
    amdgpu_va_handle m_hMarkerVa; // Handle of marker va range.

    uint64           m_offset;    // Offset in buffer object bound. It's only meaningful when it's a virtual gpu memroy.

    mutable Amdgpu::GpuMemoryFlags m_amdgpuFlags; // amdgpu specific flags

    enum amdgpu_bo_handle_type  m_externalHandleType; // Handle type such as GEM global names or dma-buf fd.

    PAL_DISALLOW_DEFAULT_CTOR(GpuMemory);
    PAL_DISALLOW_COPY_AND_ASSIGN(GpuMemory);
};

} // Amdgpu
} // Pal
