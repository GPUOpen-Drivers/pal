/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/os/lnx/lnxHeaders.h"
#include "core/os/lnx/lnxDevice.h"
#include "core/os/lnx/lnxQueue.h"
#include "core/fence.h"
#include <climits>

namespace Pal
{
namespace Linux
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
class SyncobjFence : public Fence
{
public:
    SyncobjFence(const Device& device);
    ~SyncobjFence();

    virtual Result Init(
        const FenceCreateInfo& createInfo,
        bool                   needsEvent) override;

    virtual Result OpenHandle(
        const FenceOpenInfo& openInfo) override;

    virtual Result WaitForFences(
        const Pal::Device& device,
        uint32             fenceCount,
        const Fence*const* ppFenceList,
        bool               waitAll,
        uint64             timeout) const override;

    virtual Result AssociateWithLastTimestampOrSyncobj() override;

    virtual Result ResetAssociatedSubmission() override;

    virtual Result GetStatus() const override;

    amdgpu_syncobj_handle GetFenceSyncObject() const { return m_fenceSyncObject; }

private:
    bool IsSyncobjSignaled(
        amdgpu_syncobj_handle    syncObj) const;

    amdgpu_syncobj_handle        m_fenceSyncObject;
    const Device&                m_device;

    PAL_DISALLOW_COPY_AND_ASSIGN(SyncobjFence);
};

} // Linux
} // Pal
