/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
class Fence : public IFence
{
public:
    Fence();
    virtual ~Fence() {};

    virtual Result Init(
        const FenceCreateInfo& createInfo) = 0;

    // NOTE: Part of the public IDestroyable interface.
    virtual void Destroy() override;

    void DestroyInternal(Platform* pPlatform);

    virtual Result OpenHandle(const FenceOpenInfo& openInfo) = 0;

    // Fence association is split into two steps:
    // - Associate with a submission context, which must be done as soon as the queue is known.
    // - Associate with the submission context's last timestamp, which can only be done post-queue-batching.
    virtual void AssociateWithContext(SubmissionContext* pContext) = 0;

    virtual Result Reset() = 0;

    bool InitialState() const                { return (m_fenceState.initialSignalState != 0); }
    bool WasNeverSubmitted() const           { return (m_fenceState.neverSubmitted != 0); }
    bool WasPrivateScreenPresentUsed() const { return (m_fenceState.privateScreenPresentUsed != 0); }
    bool IsOpened() const                    { return (m_fenceState.isOpened != 0);}

    virtual Result WaitForFences(
        const Device&      device,
        uint32             fenceCount,
        const Fence*const* ppFenceList,
        bool               waitAll,
        uint64             timeout) const = 0;

protected:
    // NOTE: winFence does not need initialSignalState and neverSubmitted after ErrorFenceNeverSubmitted is removed.
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

    PAL_DISALLOW_COPY_AND_ASSIGN(Fence);
};

} // Pal
