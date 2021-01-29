/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_BUILD_NULL_DEVICE

#include "core/platform.h"
#include "core/os/nullDevice/ndDevice.h"
#include "core/os/nullDevice/ndGpuMemory.h"
#include "palSysMemory.h"
#include "palInlineFuncs.h"

namespace Pal
{
namespace NullDevice
{
// =====================================================================================================================
NdGpuMemory::NdGpuMemory(
    Device* pDevice)
    :
    Pal::GpuMemory(pDevice)
{
}

// =====================================================================================================================
NdGpuMemory::~NdGpuMemory()
{
    Pal::Platform*  pPlatform = m_pDevice->GetPlatform();

    PAL_DELETE_ARRAY(m_pMemory, pPlatform);
}

// =====================================================================================================================
// Performs OS-specific initialization for allocating real, pinned or virtual memory objects.
Result NdGpuMemory::AllocateOrPinMemory(
    gpusize                 baseVirtAddr,
    uint64*                 pPagingFence,
    VirtualGpuMemAccessMode virtualAccessMode,
    uint32                  multiDeviceGpuMemoryCount,
    IDevice*const*          ppDevice,
    Pal::Image*const*       ppImage)
{
    const GpuMemoryDesc&  desc      = Desc();
    auto*                 pNdDevice = static_cast<Device*>(m_pDevice);
    auto*                 pPlatform = m_pDevice->GetPlatform();
    Result                result    = Result::Success;

    if (IsGpuVaPreReserved())
    {
        PAL_NOT_IMPLEMENTED();
        result = Result::Unsupported;
    }
    else if (IsPageTableBlock())
    {
        // Page table block allocations use the baseVirtAddr as their GPU virtual address. This will normally be
        // nonzero except for the first page table block in the address range.
        m_desc.gpuVirtAddr = baseVirtAddr;
    }
    else if (IsPageDirectory() == false)
    {
        // Anything else which isn't the Page Directory gets assigned a GPU virtual address through VAM. Note that this
        // call will overrwrite baseVirtAddr with the assigned base address.
        result = pNdDevice->AssignVirtualAddress(*this, &m_desc.gpuVirtAddr, VaPartition::Default);
    }

    // Round the address up to alignment.
    m_pMemory = PAL_NEW_ARRAY(uint8, static_cast<uint32>(desc.size + m_desc.alignment), pPlatform, Util::AllocInternal);
    m_pMemory = reinterpret_cast<uint8*>(Util::VoidPtrAlign(m_pMemory, static_cast<uint32>(m_desc.alignment)));
    if (m_pMemory == nullptr)
    {
        result = Result::ErrorOutOfMemory;
    }

    return result;
}

#if  PAL_AMDGPU_BUILD
// =====================================================================================================================

OsExternalHandle NdGpuMemory::ExportExternalHandle(
    const GpuMemoryExportInfo& exportInfo) const
{
#if defined(__unix__)
    return 0;
#else
    return nullptr;
#endif
}
#endif

// =====================================================================================================================
// Performs OS-specific initialization for allocating peer memory objects.
Result NdGpuMemory::OpenPeerMemory()
{
    // This is not expected to ever be called because there's only one null device in the system.
    PAL_NEVER_CALLED();

    return Result::Success;
}

// =====================================================================================================================
// Performs OS-specific initialization for allocating shared memory objects. In this context, a "shared" memory object
// refers to a GPU memory object residing in a non-local heap which can be accessed (shared between) two or more GPU's
// without requiring peer memory transfers.
Result NdGpuMemory::OpenSharedMemory(
    OsExternalHandle handle)
{
    // This is not expected to ever be called because there's only one null device in the system.
    PAL_NEVER_CALLED();

    return Result::Success;
}

// =====================================================================================================================
// Maps the GPU memory allocation into CPU address space.
Result NdGpuMemory::OsMap(
    void** ppData)
{
    *ppData = m_pMemory;

    return Result::Success;
}

// =====================================================================================================================
// Changes the allocation's priority. This is only supported for "real" allocations.
Result NdGpuMemory::OsSetPriority(
    GpuMemPriority       priority,
    GpuMemPriorityOffset priorityOffset)
{
    return Result::Success;
}

// =====================================================================================================================
// Unmaps the GPU memory allocation out of CPU address space.
Result NdGpuMemory::OsUnmap()
{
    // Nothing to do here!
    return Result::Success;
}

} // NullDevice
} // Pal

#endif
