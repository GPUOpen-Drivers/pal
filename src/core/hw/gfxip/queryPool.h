/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palCmdBuffer.h"
#include "palQueryPool.h"
#include "core/gpuMemory.h"

namespace Pal
{

class CmdStream;
class Device;
class GfxCmdBuffer;

// =====================================================================================================================
// Represents a set of queries that can be used to retrieve detailed info about the GPU's execution of a particular
// range of a command buffer.
class QueryPool : public IQueryPool
{
public:
    virtual ~QueryPool();

    // NOTE: Part of the IDestroyable interface.
    virtual void Destroy() override { this->~QueryPool(); }

    // NOTE: Part of the IGpuMemoryBindable interface.
    virtual void GetGpuMemoryRequirements(GpuMemoryRequirements* pGpuMemReqs) const override;

    // NOTE: Part of the IGpuMemoryBindable interface.
    virtual Result BindGpuMemory(IGpuMemory* pGpuMemory, gpusize offset) override;

    // NOTE: Part of the IQueryPool interface.
    virtual Result GetResults(
        QueryResultFlags flags,
        QueryType        queryType,
        uint32           startQuery,
        uint32           queryCount,
        const void*      pMappedGpuAddr,
        size_t*          pDataSize,
        void*            pData,
        size_t           stride
        ) override;

    virtual Result Reset(
        uint32  startQuery,
        uint32  queryCount,
        void*   pMappedCpuAddr) override
        { PAL_NEVER_CALLED(); return Result::Unsupported; }

    virtual void Begin(
        GfxCmdBuffer*     pCmdBuffer,
        CmdStream*        pCmdStream,
        CmdStream*        pHybridCmdStream,
        QueryType         queryType,
        uint32            slot,
        QueryControlFlags flags) const = 0;

    virtual void End(
        GfxCmdBuffer* pCmdBuffer,
        CmdStream*    pCmdStream,
        CmdStream*    pHybridCmdStream,
        QueryType     queryType,
        uint32        slot) const = 0;

    void Reset(
        GfxCmdBuffer* pCmdBuffer,
        CmdStream*    pCmdStream,
        uint32        startQuery,
        uint32        queryCount) const;

    // Writes commands to pCmdStream to wait until the given query slots are full of valid data. This will hang the GPU
    // if it was not preceded by a pair of calls to Begin and End.
    virtual void WaitForSlots(
        GfxCmdBuffer* pCmdBuffer,
        CmdStream*    pCmdStream,
        uint32        startQuery,
        uint32        queryCount) const = 0;

    const GpuMemory& GpuMemory() const { return *m_gpuMemory.Memory(); }

    Result GetQueryGpuAddress(uint32 slot, gpusize* pGpuAddr) const;
    Result GetTimestampGpuAddress(uint32 slot, gpusize* pGpuAddr) const;
    gpusize GetTimestampOffset(uint32 slot) const;

    gpusize GetQueryOffset(uint32 slot) const { return m_gpuMemory.Offset() + GetGpuResultSizeInBytes(slot); }
    const QueryPoolCreateInfo& CreateInfo() const { return m_createInfo; }

    size_t GetGpuResultSizeInBytes(uint32 queryCount) const
        { return static_cast<size_t>(m_gpuResultSizePerSlotInBytes * queryCount); }

    bool HasTimestamps() const { return (m_timestampSizePerSlotInBytes != 0); }

    virtual bool HasForcedQueryResult() const { return false; }
    virtual uint32 GetForcedQueryResult() const { return 0; }

    // Checks if this query pool requires any samples to be taken on the ganged-ACE queue of a Universal
    // command buffer.  This should not be called on Compute command buffers!
    virtual bool RequiresSamplingFromGangedAce() const { return false; }

    // Performs any necessary sampling of query data from the ganged-ACE queue of a Universal command
    // buffer.  This should not be called on Compute command buffers!
    virtual uint32* DeferredBeginOnGangedAce(
        GfxCmdBuffer* pCmdBuffer,
        uint32*       pCmdSpace,
        uint32        slot) const { return nullptr; }

    Result ValidateSlot(uint32 slot) const;

protected:
    QueryPool(const Device&              device,
              const QueryPoolCreateInfo& createInfo,
              gpusize                    alignment,
              gpusize                    querySizeInBytes,
              gpusize                    tsSizeInBytes);

    // Reset query via PM4 commands on a PM4-supported command buffer.
    virtual void NormalReset(
        GfxCmdBuffer* pCmdBuffer,
        CmdStream*    pCmdStream,
        uint32        startQuery,
        uint32        queryCount) const = 0;

    // Reset query using DMA, when NormalReset() can't be used or the command buffer does not support PM4.
    virtual void DmaEngineReset(
        GfxCmdBuffer* pCmdBuffer,
        CmdStream*    pCmdStream,
        uint32        startQuery,
        uint32        queryCount) const = 0;

    virtual size_t GetResultSizeForOneSlot(QueryResultFlags flags) const = 0;
    virtual bool ComputeResults(
        QueryResultFlags flags,
        QueryType        queryType,
        uint32           queryCount,
        size_t           stride,
        const void*      pGpuData,
        void*            pData) = 0;

    Result DoReset(
        uint32      startQuery,
        uint32      queryCount,
        void*       pMappedCpuAddr,
        gpusize     resetDataSizeInBytes,
        const void* pResetData);

    const QueryPoolCreateInfo m_createInfo;
    BoundGpuMemory            m_gpuMemory;

    const gpusize m_alignmentInBytes;            // Per-slot alignment of any memory bound to this pool
    const gpusize m_gpuResultSizePerSlotInBytes; // Amount of memory per slot the GPU to report all results
    const gpusize m_timestampSizePerSlotInBytes; // Amount of memory used for timestamp per slot
    const gpusize m_boundSizeInBytes;            // minimum size of any memory bound to pool (accomodates all slots)

private:
    const Device& m_device;
    const gpusize m_timestampStartOffset;        // Start offset of the timestamp. The timestamps are located at the end of
                                                 // all the query slots. QueryTimestampEnd is written to the timestamp
                                                 // address when the End() is called. And in WaitForSlots() we wait for
                                                 // this timestamp.

    PAL_DISALLOW_COPY_AND_ASSIGN(QueryPool);
    PAL_DISALLOW_DEFAULT_CTOR(QueryPool);
};

} // Pal
