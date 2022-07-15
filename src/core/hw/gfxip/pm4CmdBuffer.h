/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/dmaUploadRing.h"
#include "core/fence.h"
#include "core/perfExperiment.h"
#include "core/platform.h"
#include "gfxCmdBuffer.h"
#include "palDeque.h"
#include "palHashMap.h"
#include "palQueryPool.h"

namespace Pal
{

// Forward decl's
class BorderColorPalette;
class CmdAllocator;
class CmdStream;
class GfxCmdBuffer;
class GfxDevice;
class GpuMemory;
class IndirectCmdGenerator;
class Pipeline;
class PerfExperiment;
class IPerfExperiment;

// Tracks the state of a user-data table stored in GPU memory.  The table's contents are managed using embedded data
// and the CPU, or using GPU scratch memory and CE RAM.
struct UserDataTableState
{
    gpusize  gpuVirtAddr;   // GPU virtual address where the current copy of the table data is stored.
    // CPU address of the embedded-data allocation storing the current copy of the table data.  This can be null if
    // the table has not yet been uploaded to embedded data.
    uint32* pCpuVirtAddr;
    struct
    {
        uint32  sizeInDwords : 31; // Size of one full instance of the user-data table, in DWORD's.
        uint32  dirty        : 1;  // Indicates that the CPU copy of the user-data table is more up to date than the
                                   // copy currently in GPU memory and should be updated before the next dispatch.
    };
};

// Acquire/release synchronization event types for supported pipeline event.
enum class AcqRelEventType : uint32
{
    Eop    = 0x0,
    PsDone = 0x1,
    CsDone = 0x2,
    Count,

    Invalid = Count
};

// Acquire/release synchronization token structure.
union AcqRelSyncToken
{
    struct
    {
        uint32 fenceVal : 30;
        uint32 type     :  2;
    };

    uint32 u32All;
};

union Pm4CmdBufferStateFlags
{
    struct
    {
        uint32 perfCounterStarted        :  1;  // Track if perfExperiment has started with perfCounter or spmTrace
        uint32 perfCounterStopped        :  1;  // Track if perfExperiment has stopped with perfCounter or spmTrace
        uint32 sqttStarted               :  1;  // Track if perfExperiment has started with SQ Thread Trace enabled
        uint32 sqttStopped               :  1;  // Track if perfExperiment has stopped with SQ Thread Trace enabled
        uint32 packetPredicate           :  1;  // Track if command buffer packets are currently using predication.
        uint32 gfxBltActive              :  1;  // Track if there are potentially any GFX Blt in flight.
        uint32 gfxWriteCachesDirty       :  1;  // Track if any of the GFX Blt write caches may be dirty.
        uint32 csBltActive               :  1;  // Track if there are potentially any CS Blt in flight.
        uint32 csWriteCachesDirty        :  1;  // Track if any of the CS Blt write caches may be dirty.
        uint32 cpBltActive               :  1;  // Track if there are potentially any CP Blt in flight. A CP Blt is
                                                // an asynchronous CP DMA operation acting as a PAL Blt.
        uint32 cpWriteCachesDirty        :  1;  // Track if any of the CP Blt write caches may be dirty.
        uint32 cpMemoryWriteL2CacheStale :  1;  // Track if a CP memory write has occurred and the L2 cache could be
                                                // stale.
        uint32 prevCmdBufActive          :  1;  // Set if it's possible work from a previous command buffer
                                                // submitted on this queue may still be active.  This flag starts
                                                // set and will be cleared if/when an EOP wait is inserted in this
                                                // command buffer.
        uint32 reserved1                 :  1;
        uint32 rasterKillDrawsActive     :  1;  // Track if there are any rasterization kill draws in flight.
        uint32 reserved                  : 17;
    };

    uint32 u32All;
};

struct Pm4CmdBufferState
{
    Pm4CmdBufferStateFlags flags;

    struct
    {
        uint32 gfxBltExecEopFenceVal;       // Earliest EOP fence value that can confirm all GFX BLTs are complete.
        uint32 gfxBltWbEopFenceVal;         // Earliest EOP fence value that can confirm all GFX BLT destination data is
                                            // written back to L2.
        uint32 csBltExecEopFenceVal;        // Earliest EOP fence value that can confirm all CS BLTs are complete.
        uint32 csBltExecCsDoneFenceVal;     // Earliest CS_DONE fence value that can confirm all CS BLTs are complete.
        uint32 rasterKillDrawsExecFenceVal; // Earliest EOP fence value that can confirm all rasterization kill
                                            // draws are complete.
    } fences;
};

// =====================================================================================================================
// Abstract class for executing basic hardware-specific functionality common to GFXIP universal and compute command
// buffers in PM4.
class Pm4CmdBuffer : public GfxCmdBuffer
{
    // A useful shorthand for a vector of chunks.
    typedef ChunkVector<CmdStreamChunk*, 16, Platform> ChunkRefList;

    // Alias for a vector of pointers to gfx images.
    using FceRefCountsVector = Util::Vector<uint32*, MaxNumFastClearImageRefs, Platform>;

public:
    virtual Result Begin(const CmdBufferBuildInfo& info) override;
    virtual Result Reset(ICmdAllocator* pCmdAllocator, bool returnGpuMemory) override;
    virtual Result End() override;

    virtual void CmdSaveComputeState(uint32 stateFlags) override;
    virtual void CmdRestoreComputeState(uint32 stateFlags) override;

    virtual void CmdDuplicateUserData(
        PipelineBindPoint source,
        PipelineBindPoint dest) override;

    virtual void CmdSuspendPredication(bool suspend) override
        { m_pm4CmdBufState.flags.packetPredicate = suspend ? 0 : 1; }

    uint32 GetNextAcqRelFenceVal(AcqRelEventType type)
    {
        m_acqRelFenceVals[static_cast<uint32>(type)]++;
        return m_acqRelFenceVals[static_cast<uint32>(type)];
    }

    const ComputeState& GetComputeState() const { return m_computeState; }

    const Pm4CmdBufferState& GetPm4CmdBufState() const { return m_pm4CmdBufState; }

    // Helper functions
    void OptimizePipePoint(HwPipePoint* pPipePoint) const;
    void OptimizeSrcCacheMask(uint32* pCacheMask) const;
    virtual void OptimizePipeAndCacheMaskForRelease(uint32* pStageMask, uint32* pAccessMask) const;

    void SetPm4CmdBufGfxBltState(bool gfxBltActive) { m_pm4CmdBufState.flags.gfxBltActive = gfxBltActive; }
    void SetPm4CmdBufCsBltState(bool csBltActive) { m_pm4CmdBufState.flags.csBltActive = csBltActive; }
    void SetPm4CmdBufCpBltState(bool cpBltActive) { m_pm4CmdBufState.flags.cpBltActive = cpBltActive; }
    void SetPm4CmdBufRasterKillDrawsState(bool rasterKillDrawsActive)
        { m_pm4CmdBufState.flags.rasterKillDrawsActive = rasterKillDrawsActive; }
    void SetPm4CmdBufGfxBltWriteCacheState(bool gfxWriteCacheDirty)
        { m_pm4CmdBufState.flags.gfxWriteCachesDirty = gfxWriteCacheDirty; }
    void SetPm4CmdBufCsBltWriteCacheState(bool csWriteCacheDirty)
        { m_pm4CmdBufState.flags.csWriteCachesDirty = csWriteCacheDirty; }
    void SetPm4CmdBufCpBltWriteCacheState(bool cpWriteCacheDirty)
        { m_pm4CmdBufState.flags.cpWriteCachesDirty = cpWriteCacheDirty; }
    void SetPm4CmdBufCpMemoryWriteL2CacheStaleState(bool cpMemoryWriteDirty)
        { m_pm4CmdBufState.flags.cpMemoryWriteL2CacheStale = cpMemoryWriteDirty; }

    // Execution fence value is updated at every BLT. Set it to the next event because its completion indicates all
    // prior BLTs have completed.
    void UpdatePm4CmdBufGfxBltExecEopFence()
    {
        m_pm4CmdBufState.fences.gfxBltExecEopFenceVal = GetCurAcqRelFenceVal(AcqRelEventType::Eop) + 1;
    }
    void UpdatePm4CmdBufCsBltExecFence()
    {
        m_pm4CmdBufState.fences.csBltExecEopFenceVal    = GetCurAcqRelFenceVal(AcqRelEventType::Eop) + 1;
        m_pm4CmdBufState.fences.csBltExecCsDoneFenceVal = GetCurAcqRelFenceVal(AcqRelEventType::CsDone) + 1;
    }
    // Execution fence value is updated at every draw when rasterization kill state is dirtied.
    // - If rasterization kill is enabled, set it UINT32_MAX to make rasterKillDrawsActive never to be cleared.
    // - If rasterization kill is disabled, set it to the next EOP event because its completion indicates all
    //   prior rasterization kill draws have completed.
    void UpdatePm4CmdBufRasterKillDrawsExecEopFence(bool setMaxFenceVal)
    {
        m_pm4CmdBufState.fences.rasterKillDrawsExecFenceVal =
            setMaxFenceVal ? UINT32_MAX : GetCurAcqRelFenceVal(AcqRelEventType::Eop) + 1;
    }
    // Cache write-back fence value is updated at every release event. Completion of current event indicates the cache
    // synchronization has completed too, so set it to current event fence value.
    void UpdatePm4CmdBufGfxBltWbEopFence(uint32 fenceVal) { m_pm4CmdBufState.fences.gfxBltWbEopFenceVal = fenceVal; }

    virtual void CmdBeginPerfExperiment(IPerfExperiment* pPerfExperiment) override;
    virtual void CmdUpdatePerfExperimentSqttTokenMask(
        IPerfExperiment*              pPerfExperiment,
        const ThreadTraceTokenConfig& sqttTokenConfig) override;
    virtual void CmdEndPerfExperiment(IPerfExperiment* pPerfExperiment) override;

    virtual bool PerfCounterStarted() const override
        { return m_pm4CmdBufState.flags.perfCounterStarted; }

    virtual bool PerfCounterClosed() const override
        { return m_pm4CmdBufState.flags.perfCounterStopped; }

    virtual bool SqttStarted() const override
        { return m_pm4CmdBufState.flags.sqttStarted; }

    virtual bool SqttClosed() const override
        { return m_pm4CmdBufState.flags.sqttStopped; }

    virtual void CmdSetKernelArguments(
        uint32            firstArg,
        uint32            argCount,
        const void*const* ppValues) override;

    // Helper functions
    uint32 GetCurAcqRelFenceVal(AcqRelEventType type) const { return m_acqRelFenceVals[static_cast<uint32>(type)]; }

    virtual void CmdOverwriteRbPlusFormatForBlits(
        SwizzledFormat format,
        uint32         targetIndex) = 0;

    virtual void CmdOverwriteDisableViewportClampForBlits(
        bool disableViewportClamp) = 0;

    void AddFceSkippedImageCounter(GfxImage* pGfxImage);

    static void SetBarrierOperationsRbCacheSynced(Developer::BarrierOperations* pOperations)
    {
        pOperations->caches.flushCb = 1;
        pOperations->caches.invalCb = 1;
        pOperations->caches.flushDb = 1;
        pOperations->caches.invalDb = 1;
        pOperations->caches.flushCbMetadata = 1;
        pOperations->caches.invalCbMetadata = 1;
        pOperations->caches.flushDbMetadata = 1;
        pOperations->caches.invalDbMetadata = 1;
    }

    void SetPrevCmdBufInactive() { m_pm4CmdBufState.flags.prevCmdBufActive = 0; }

    gpusize AcqRelFenceValBaseGpuVa() const { return m_acqRelFenceValGpuVa; }
    gpusize AcqRelFenceValGpuVa(AcqRelEventType type) const
        { return (m_acqRelFenceValGpuVa + sizeof(uint32) * static_cast<uint32>(type)); }

protected:
    Pm4CmdBuffer(
        const GfxDevice&           device,
        const CmdBufferCreateInfo& createInfo);
    virtual ~Pm4CmdBuffer();

    virtual Result BeginCommandStreams(CmdStreamBeginFlags cmdStreamFlags, bool doReset) override;

    virtual void ResetState() override;

    void LeakPerPipelineStateChanges(
        const Pal::PipelineState& leakedPipelineState,
        const UserDataEntries&    leakedUserDataEntries,
        Pal::PipelineState*       pDestPipelineState,
        UserDataEntries*          pDestUserDataEntries);

    virtual void InheritStateFromCmdBuf(const Pm4CmdBuffer* pCmdBuffer) = 0;

    virtual void OptimizeBarrierReleaseInfo(
        uint32       pipePointCount,
        HwPipePoint* pPipePoints,
        uint32*      pCacheMask) const override;

    virtual void OptimizeAcqRelReleaseInfo(uint32* pStageMask, uint32* pAccessMask) const override;

    void UpdateUserDataTableCpu(
        UserDataTableState * pTable,
        uint32              dwordsNeeded,
        uint32              offsetInDwords,
        const uint32*       pSrcData,
        uint32              alignmentInDwords = 1);

    // Returns the number of queries associated with this command buffer that have yet to "end"
    uint32 NumActiveQueries(QueryPoolType queryPoolType) const
        { return m_numActiveQueries[static_cast<size_t>(queryPoolType)]; }

    // NOTE: We need to be conservative if this is a nested command buffer: the calling command buffer may have
    // enabled one or more queries before calling this command buffer, so we need to assume that it did, because
    // we have no way of knowing for sure.  Returning uint32 as zero vs. non-zero to avoid some branches.
    uint32 MayHaveActiveQueries() const
        { return  (m_createInfo.flags.nested | NumActiveQueries(QueryPoolType::Occlusion)); }

    virtual void ActivateQueryType(QueryPoolType queryPoolType)
        { m_queriesActive[static_cast<size_t>(queryPoolType)] = true; }

    virtual void DeactivateQueryType(QueryPoolType queryPoolType)
        { m_queriesActive[static_cast<size_t>(queryPoolType)] = false; }

    void DeactivateQueries();
    void ReactivateQueries();

    bool IsQueryActive(QueryPoolType queryPoolType) const
        { return m_queriesActive[static_cast<size_t>(queryPoolType)]; }

    // Returns true if the client is beginning the first query of the specified type on this command buffer. Note that
    // this function has the side-effect of changing the number of active queries begin tracked. For general-status
    // queries, call NumActiveQueries() instead to not modify the current state.
    bool IsFirstQuery(QueryPoolType queryPoolType)
    {
        m_numActiveQueries[static_cast<uint32>(queryPoolType)]++;
        return (NumActiveQueries(queryPoolType) == 1);
    }

    // Returns true if the client is ending the last active query of the specified type on this command buffer. Note
    // that this function has the side-effect of changing the number of active queries begin tracked. For
    // general-status queries, call NumActiveQueries() instead to not modify the current state.
    bool IsLastActiveQuery(QueryPoolType queryPoolType)
    {
        PAL_ASSERT(NumActiveQueries(queryPoolType) != 0);
        m_numActiveQueries[static_cast<uint32>(queryPoolType)]--;
        return (NumActiveQueries(queryPoolType) == 0);
    }

    // Helper function for resetting a user-data table which is managed using embdedded data or CE RAM at the beginning of
    // a command buffer.
    inline void ResetUserDataTable(
        UserDataTableState* pTable)
    {
        pTable->pCpuVirtAddr = nullptr;
        pTable->gpuVirtAddr  = 0;
        pTable->dirty        = 0;
    }

    CmdStreamChunk* GetNextGeneratedChunk();

    FceRefCountsVector m_fceRefCountVec;

    Pm4CmdBufferState  m_pm4CmdBufState;      // Common pm4 command buffer states.

private:
    void ResetFastClearReferenceCounts();

    const GfxDevice&  m_device;

    gpusize m_acqRelFenceValGpuVa; // GPU virtual address of 3-dwords memory used for acquire/release pipe event sync.

    // Number of active queries in this command buffer.
    uint32 m_numActiveQueries[static_cast<size_t>(QueryPoolType::Count)];

    // False if DeactivateQuery() has been called on a particular query type, true otherwise.
    // Specifically used for when Push/Pop state has been called. We only want to have a query active on code
    // executed by a client.
    bool m_queriesActive[static_cast<size_t>(QueryPoolType::Count)];

    uint32 m_acqRelFenceVals[static_cast<uint32>(AcqRelEventType::Count)];

    PAL_DISALLOW_COPY_AND_ASSIGN(Pm4CmdBuffer);
    PAL_DISALLOW_DEFAULT_CTOR(Pm4CmdBuffer);
};
} // Pal
