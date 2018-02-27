/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/gpuMemory.h"
#include "palPlatform.h"

namespace Pal
{
namespace NullDevice
{

// =====================================================================================================================
// Represents a null device GPU memory object.
class NdGpuMemory : public GpuMemory
{
public:
    explicit NdGpuMemory(Device* pDevice);
    virtual ~NdGpuMemory();

    virtual Result AllocateOrPinMemory(
        gpusize                 baseVirtAddr,
        uint64*                 pPagingFence,
        VirtualGpuMemAccessMode virtualAccessMode,
        uint32                  multiDeviceGpuMemoryCount,
        IDevice*const*          ppDevice) override;

    virtual OsExternalHandle GetSharedExternalHandle() const override;

    virtual Result OpenPeerMemory() override;

    virtual Result OpenSharedMemory() override;

    virtual Result OsMap(void** ppData) override;

    virtual Result OsSetPriority(GpuMemPriority priority, GpuMemPriorityOffset priorityOffset) override;

    virtual Result OsUnmap() override;

    // SVM is not supported on this path
    virtual Result AllocateSvmVirtualAddress(
        gpusize baseVirtAddr,
        gpusize size,
        gpusize align,
        bool    commitCpuVa) override { return Result::ErrorUnavailable; }
    virtual Result FreeSvmVirtualAddress() override { return Result::ErrorUnavailable; }

protected:

private:
    PAL_DISALLOW_DEFAULT_CTOR(NdGpuMemory);
    PAL_DISALLOW_COPY_AND_ASSIGN(NdGpuMemory);

    uint8*  m_pMemory;
}; // NdGpuMemory

} // NullDevice
} // Pal
