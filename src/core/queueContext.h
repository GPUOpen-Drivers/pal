/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace Pal
{

class  CmdStream;
class  Device;
struct InternalSubmitInfo;
class  Queue;

// =====================================================================================================================
// A "QueueContext" is responsible for managing any Device or hardware-layer state which needs to potentially be updated
// or re-validated prior to any of the operations which the IQueue interface exposes. The default implementation in this
// base class is just a set of empty functions, which is useful for Queue types or hardware layers which don't require
// any per-Queue submission bookkeeping. Presently, the only IQueue operation which requires such preprocessing or
// postprocessing is the Submit() method.
class QueueContext
{
public:
    QueueContext(Device* pDevice) : m_pDevice(pDevice), m_pParentQueue(nullptr), m_needWaitIdleOnRingResize(false) { }

    // Queue contexts should only be created in placed memory and must always be destroyed explicitly.
    void Destroy() { this->~QueueContext(); }

    // Performs preprocessing or validation which needs to occur before the Queue is "ready" to receive a set of
    // command buffers for submission. The base implementation is intentionally a no-op.
    virtual Result PreProcessSubmit(InternalSubmitInfo* pSubmitInfo, uint32 cmdBufferCount);

    // Performs postprocessing which needs to occur after the Queue has either submitted or batched a set of command
    // buffers from the client. The base implementation is intentionally a no-op.
    virtual void PostProcessSubmit() { }

    // Performs any required processing on the first submission to the queue.
    // Returns Success if the submission is required, and Unsupported otherwise.
    virtual Result ProcessInitialSubmit(InternalSubmitInfo* pSubmitInfo) { return Result::Unsupported; }

    void SetParentQueue(Queue* pQueue) { m_pParentQueue   = pQueue; }
    void SetWaitForIdleOnRingResize(bool doWait) { m_needWaitIdleOnRingResize = doWait; }

    virtual gpusize ShadowMemVa() const { return 0; }

protected:
    virtual ~QueueContext();

    Result CreateTimestampMem(bool needWaitForIdleMem);

    Device*const   m_pDevice;
    Queue*         m_pParentQueue;

    // All QueueContext subclasses require at least one 32-bit timestamp in local GPU memory.
    BoundGpuMemory m_exclusiveExecTs; // This TS prevents independent submissions from running at the same time.
    BoundGpuMemory m_waitForIdleTs;   // This TS implements a full wait-for-idle.

    bool           m_needWaitIdleOnRingResize;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(QueueContext);
};

// Make sure that QueueContext has nonzero size (this should always be the case due to the v-table.
static_assert(sizeof(QueueContext) > 0, "Unexpected size for QueueContext!");

} // Pal
