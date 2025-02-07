/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/fence.h"
#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuQueue.h"

#include <climits>

namespace Pal
{
namespace Amdgpu
{

class Device;
class Platform;
class SubmissionContext;

// =====================================================================================================================
// Represents a command buffer SyncobjFence the client can use for coarse-level synchronization between GPU and CPU.
//
// SyncObjFences is implemented on Sync Object.  Instead using timestamp to reference the underlying dma-fence,
// Sync Object contains the pointer to the fence.  Beyond the ordinary fence wait operation, Fence import/export
// are supported with sync object.
class SyncobjFence final : public Fence
{
public:
    SyncobjFence(const Device& device);
    ~SyncobjFence();

    virtual Result Init(
        const FenceCreateInfo& createInfo) override;

    virtual Result OpenHandle(
        const FenceOpenInfo& openInfo) override;

    virtual OsExternalHandle ExportExternalHandle(
        const FenceExportInfo& exportInfo) const override;

    virtual Result WaitForFences(
        const Pal::Device&       device,
        uint32                   fenceCount,
        const Pal::Fence*const*  ppFenceList,
        bool                     waitAll,
        std::chrono::nanoseconds timeout) const override;

    virtual void AssociateWithContext(Pal::SubmissionContext* pContext) override;

    virtual Result Reset() override;

    virtual Result GetStatus() const override;

    amdgpu_syncobj_handle SyncObjHandle() const { return m_fenceSyncObject; }

private:
    bool IsSyncobjSignaled(
        amdgpu_syncobj_handle    syncObj) const;

    amdgpu_syncobj_handle        m_fenceSyncObject;
    const Device&                m_device;

    PAL_DISALLOW_COPY_AND_ASSIGN(SyncobjFence);
};

} // Amdgpu
} // Pal
