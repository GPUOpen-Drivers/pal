/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/fence.h"
#include "core/os/amdgpu/amdgpuQueue.h"
#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuPlatform.h"
#include "util/lnx/lnxTimeout.h"
#include "palInlineFuncs.h"
#include "palMutex.h"
#include "palAutoBuffer.h"

namespace Pal
{
namespace Amdgpu
{

// =====================================================================================================================
// This version of Fence is based on timestamp only. It cannot be shared among processes. This Fence is for legacy
// Linux only.
class TimestampFence : public Pal::Fence
{
public:
    TimestampFence();
    virtual ~TimestampFence();

    virtual Result Init(
        const FenceCreateInfo& createInfo) override;

    // NOTE: Part of the public IFence interface.
    virtual Result GetStatus() const override;

    virtual Result OpenHandle(const FenceOpenInfo& openInfo) override
        { return Result::Unsupported; }

    virtual OsExternalHandle ExportExternalHandle(const FenceExportInfo& exportInfo) const override
        { return -1; }

    // Fence association is split into two steps:
    // - Associate with a submission context, which must be done as soon as the queue is known.
    // - Associate with the submission context's last timestamp, which can only be done post-queue-batching.
    virtual void AssociateWithContext(Pal::SubmissionContext* pContext) override;
    virtual Result AssociateWithLastTimestamp();

    virtual Result Reset() override;

    uint64 Timestamp() const { return m_timestamp; }

    virtual Result WaitForFences(
        const Pal::Device&      device,
        uint32                  fenceCount,
        const Pal::Fence*const* ppFenceList,
        bool                    waitAll,
        uint64                  timeout) const override;

    bool IsBatched() const { return m_timestamp == BatchedTimestamp; }

protected:
    SubmissionContext* m_pContext;

private:
    // A Fence can be associated with a submission either at submission time or afterwards; the submission may be
    // batched or already submitted to the OS. A fence can only be associated with a single Queue submission at a time.
    // These members track the Queue and OS-specific timestamp for the current associated submission.
    //
    // The maximum timestamp has been reserved; it indicates that the associated submission has been batched. Note that
    // the timestamp may be modified asynchronously to normal fence operation when a batched submission is unrolled.
    static constexpr uint64 BatchedTimestamp = UINT64_MAX;

    volatile uint64    m_timestamp;

    PAL_DISALLOW_COPY_AND_ASSIGN(TimestampFence);
};

} // Amdgpu
} // Pal
