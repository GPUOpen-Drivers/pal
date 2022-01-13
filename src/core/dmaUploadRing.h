/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "pal.h"
#include "palMutex.h"
#include "palInlineFuncs.h"

namespace Pal
{

class Queue;
class GpuMemory;
class Device;
class CmdBuffer;
class IFence;
class ICmdBuffer;

// Describes a token which can be waited-on to wait for a previously-submitted upload to // finish.
typedef uint64 UploadFenceToken;

// Describes a slot in the upload ring where work can be recorded.
typedef uint32 UploadRingSlot;

constexpr uint32 RingInitEntries = 512;  ///< Max number of entries in DmaUploadRing.

// =====================================================================================================================
class DmaUploadRing
{
public:
    explicit DmaUploadRing(Device* pDevice);
    virtual ~DmaUploadRing();
    Result Init();
    Result AcquireRingSlot(UploadRingSlot* pSlotId);
    Result Submit(
        UploadRingSlot    slotId,
        UploadFenceToken* pCompletionFence,
        uint64            pagingFenceVal);

    virtual Result WaitForPendingUpload(
        Pal::Queue*      pWaiter,
        UploadFenceToken fenceValue) = 0;

    // Records DMA upload commands from embedded data to the destination.  Will only copy
    // up to the embedded data limit. Actual bytes copied are returned.  Caller must
    // initialize the embedded data buffer returned through ppEmbeddedData after this
    // returns.
    size_t UploadUsingEmbeddedData(
        UploadRingSlot  slotId,
        Pal::GpuMemory* pDst,
        gpusize         dstOffset,
        size_t          bytes,
        void**          ppEmbeddedData);

protected:
    Pal::Device* m_pDevice;
    Pal::Queue*  m_pDmaQueue;

private:
    struct Entry
    {
        ICmdBuffer* pCmdBuf;
        IFence*     pFence;
    };
    // Initialize each item of the ring from m_firstEntryFree to the end of the ring.
    Result InitRingItem(uint32 slotIdx);
    Result CreateInternalCopyQueue();
    Result CreateInternalCopyCmdBuffer(CmdBuffer** ppCmdBuffer);
    Result CreateInternalFence(IFence** ppFence);
    Result ResizeRing();
    Result FreeFinishedSlots();

    Entry* m_pRing;
    uint32 m_ringCapacity;
    uint32 m_firstEntryInUse;
    uint32 m_firstEntryFree;
    uint32 m_numEntriesInUse;
};

}
