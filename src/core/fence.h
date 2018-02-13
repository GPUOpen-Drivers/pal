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

#include "palFence.h"
#include <climits>

namespace Pal
{

class Device;
class Platform;
class SubmissionContext;

// =====================================================================================================================
// Represents a command buffer fence the client can use for coarse-level synchronization between the GPU and CPU.
//
// Fences can be specified when calling IQueue::Submit() and will be signaled when all commands in that submit have
// completed.  The status of the fence can be queried by the client to determine when the GPU work of interest has
// completed.
//
// NOTE: The Windows and Linux flavors of this object are so similar, that it seemed excessive to create muliple child
// classes just to override the GetStatus() method and to determine whether or not the Fence contained an Event object
// (which is only needed on Windows).
class Fence : public IFence
{
public:
    Fence();
    virtual ~Fence();

    virtual Result Init(
        const FenceCreateInfo& createInfo,
        bool                   needsEvent);

    // NOTE: Part of the public IDestroyable interface.
    virtual void Destroy() override;

    void DestroyInternal(Platform* pPlatform);

    // NOTE: Part of the public IFence interface.
    virtual Result GetStatus() const override;

    virtual Result OpenHandle(const FenceOpenInfo& openInfo);

    virtual OsExternalHandle GetHandle() const override
    {
        return -1;
    }

    // Fence association is split into two steps:
    // - Associate with a submission context, which must be done as soon as the queue is known.
    // - Associate with the submission context's last timestamp, which can only be done post-queue-batching.
    void AssociateWithContext(SubmissionContext* pContext);
    virtual Result AssociateWithLastTimestampOrSyncobj();

    virtual Result ResetAssociatedSubmission();

    // Associates the Fence object with private screen present, i.e. The WaitForFences() only needs to check the event.
    void AssociateWithPrivateScreen() { m_fenceState.privateScreenPresentUsed = 1; }

    bool InitialState() const                { return (m_fenceState.initialSignalState != 0); }
    bool WasNeverSubmitted() const           { return (m_fenceState.neverSubmitted != 0); }
    bool WasPrivateScreenPresentUsed() const { return (m_fenceState.privateScreenPresentUsed != 0); }
    bool IsBatched() const                   { return (m_timestamp == BatchedTimestamp); }
    bool IsOpened() const                    { return (m_fenceState.isOpened != 0);}

    uint64 Timestamp() const { return m_timestamp; }

    // True if the fence has been reset, meaning it isn't associated with a private screen present or a regular
    // command buffer submission.
    bool IsReset() const { return (WasPrivateScreenPresentUsed() == false) && (m_pContext == nullptr); }

    virtual Result WaitForFences(
        const Device&      device,
        uint32             fenceCount,
        const Fence*const* ppFenceList,
        bool               waitAll,
        uint64             timeout) const;

protected:
    //state flag for an fence object.
    union
    {
        struct
        {
            uint32 initialSignalState       : 1;  // Tracks if this fence is signaled because it was created signaled.
            uint32 neverSubmitted           : 1;  // Tracks if this fence has never been submitted by the client.
            uint32 privateScreenPresentUsed : 1;  // Tracks if this fence has been used in private screen present
            uint32 isOpened                 : 1;  // Tracks if this fence is opened.
            uint32 reserved                 : 28;
        };
        uint32 flags;
    } m_fenceState;

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

    PAL_DISALLOW_COPY_AND_ASSIGN(Fence);
};

} // Pal
