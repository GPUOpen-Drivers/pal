/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/queue.h"
#include "palVector.h"
#include "palLiterals.h"

namespace Pal
{

class Device;
class GpuMemory;
class ICmdBuffer;
class IFence;
class IQueue;
class IQueueSemaphore;

// How many command streams CmdUploadRing can upload from a command buffer.
constexpr uint32 MaxUploadedCmdStreams = 3;

// Flags we must track for each uploaded command stream.
union UploadedStreamFlags
{
    struct
    {
        uint32 isPreemptionEnabled :  1; // If the stream can be preempted.
        uint32 dropIfSameContext   :  1; // If the stream can be dropped if the prior submit was PAL's.
        uint32 reserved            : 30;
    };
    uint32 u32All;
};

// All information needed to launch a single uploaded command stream.
struct UploadedStreamInfo
{
    UploadedStreamFlags flags;
    EngineType          engineType;    // The engine type targeted by this stream.
    SubEngineType       subEngineType; // The sub engine type targeted by this stream.
    const GpuMemory*    pGpuMemory;    // The GPU memory backing the stream or null if the stream is empty.
    gpusize             launchSize;    // The size of the first command block in the stream.
};

// All information needed to launch the uploaded command streams from a set of command buffers.
struct UploadedCmdBufferInfo
{
    uint32             uploadedCmdBuffers;                // The number of command buffers uploaded.
    uint32             uploadedCmdStreams;                // The number of command streams that must be launched.
    UploadedStreamInfo streamInfo[MaxUploadedCmdStreams]; // The uploaded command streams.
    IQueueSemaphore*   pUploadComplete;                   // The caller must wait on this before executing.
    IQueueSemaphore*   pExecutionComplete;                // The caller must signal this when done executing.
};

// Gfxip-independent information provided by the creator.
struct CmdUploadRingCreateInfo
{
    EngineType engineType;
    uint32     numCmdStreams;
};

// =====================================================================================================================
// Uploads gfxip command buffers to rafts of GPU memory in the local heap using a DMA queue. Intended to optimize the
// submit overhead of a list of non-exclusive-submit command buffers.
class CmdUploadRing
{
    struct Copy;
    struct Raft;
    struct UploadState;

public:
    void DestroyInternal();

    uint32 PredictBatchSize(
        uint32                  cmdBufferCount,
        const ICmdBuffer*const* ppCmdBuffers) const;

    Result UploadCmdBuffers(
        uint32                  cmdBufferCount,
        const ICmdBuffer*const* ppCmdBuffers,
        UploadedCmdBufferInfo*  pUploadInfo);

    const IQueue* UploadQueue() const { return m_pQueue; }

protected:
    static size_t GetPlacementSize(const Device& device);

    CmdUploadRing(
        const CmdUploadRingCreateInfo& createInfo,
        Device*                        pDevice,
        uint32                         minPostambleBytes,
        gpusize                        maxStreamBytes);

    virtual ~CmdUploadRing();

    Result Init(void* pPlacementAddr);

    const CmdUploadRingCreateInfo m_createInfo;

    const bool    m_trackMemoryRefs;   // True if we must track per-submit memory references while uploading.
    const uint32  m_addrAlignBytes;    // Required command stream address alignment.
    const uint32  m_sizeAlignBytes;    // Required command stream size alignment.
    const uint32  m_minPostambleBytes; // Gfxip-specific minimum postamble size (chain plus necessary padding).
    const gpusize m_maxStreamBytes;    // Gfxip-specific command stream max size.
    Device*const  m_pDevice;

private:
    Raft* NextRaft();
    Copy* NextCopy();

    void EndCurrentIb(
        const IGpuMemory& raftMemory,
        ICmdBuffer*       pCopyCmdBuffer,
        UploadState*      pState);

    // Updates the copy command buffer to write commands into the raft memory at the postamble offset such that the
    // postamble is completely filled by NOPs followed by one chain packet which points at the chain destination.
    // If the chain address is zero the postamble is completely filled with NOPs.
    virtual void UploadChainPostamble(
        const IGpuMemory& raftMemory,
        ICmdBuffer*       pCopyCmdBuffer,
        gpusize           postambleOffset,
        gpusize           postambleBytes,
        gpusize           chainDestAddr,
        gpusize           chainDestBytes,
        bool              isConstantEngine,
        bool              isPreemptionEnabled) = 0;

    // A raft of GPU memory for a single upload plus the state needed to synchronize access to the memory.
    struct Raft
    {
        GpuMemory*       pGpuMemory[MaxUploadedCmdStreams]; // One GPU memory per uploaded command stream type.
        IQueueSemaphore* pStartCopy;                        // Signaled when the caller is done with prior reading.
        IQueueSemaphore* pEndCopy;                          // Signaled when the upload queue is done copying commands.
    };

    // A command buffer and fence used for a single upload operation. Uploads can be pipelined using queue semaphores
    // so we expect to have many more of these objects than memory rafts.
    struct Copy
    {
        ICmdBuffer* pCmdBuffer;
        IFence*     pFence;
    };

    // Some information we need to track per-command-stream while building upload commands.
    struct UploadState
    {
        UploadedStreamFlags flags;         // Most of these are taken from the first command stream uploaded.
        EngineType          engineType;    // Also from the first command stream.
        SubEngineType       subEngineType; // Also from the first command stream.

        gpusize raftFreeOffset;        // Where the next byte of free space is in the raft.
        gpusize prevIbPostambleOffset; // Zero, or the offset to the previous IB's chain postamble.
        gpusize prevIbPostambleSize;   // The size of the previous IB's chain postamble, including padding, in bytes.
        gpusize curIbOffset;           // Where the current IB started.
        gpusize curIbSizeBytes;        // The size of the current IB, not including the postamble.
        gpusize curIbFreeBytes;        // Remaining free space in the current IB.
        gpusize launchBytes;           // The size of the first uploaded IB (the size of the IB the KMD will launch).
    };

    static constexpr gpusize RaftMemBytes = 256 * Util::OneKibibyte; // The size of each raft's GPU memory object.
    static constexpr uint32  RaftRingSize = 2;
    static constexpr uint32  CopyRingSize = 4;

    IQueue* m_pQueue;             // All commands will be uploaded on this queue.
    Raft    m_raft[RaftRingSize];
    Copy    m_copy[CopyRingSize];
    uint32  m_prevRaft;           // These are the indices of the previously used items in each ring.
    uint32  m_prevCopy;

    // We must keep track of which command chunk allocations will be read by the upload queue.
    Util::Vector<GpuMemoryRef, 32, Platform> m_chunkMemoryRefs;

    PAL_DISALLOW_DEFAULT_CTOR(CmdUploadRing);
    PAL_DISALLOW_COPY_AND_ASSIGN(CmdUploadRing);
};

} // Pal
