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

#include "core/cmdBuffer.h"
#include "core/dmaUploadRing.h"
#include "core/fence.h"
#include "core/perfExperiment.h"
#include "core/platform.h"
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

// Which engines are supported by this command buffer's CmdStreams.
enum CmdBufferEngineSupport : uint32
{
    Graphics = 0x1,
    Compute  = 0x2,
    CpDma    = 0x4,
};

// GPU memory alignment required for a piece of memory used in a predication operation.
constexpr uint32 PredicationAlign = 16;

// In order to make tracking user data entries easier, MaxUserDataEntries is the maximum possible number of user data
// entries (registers and spill memory) available to the client. This value should always be greater than or equal to
// the number returned to the client.
constexpr uint32 MaxUserDataEntries = 128;

// Wide-bitmask of one flag for every user-data entry.
constexpr uint32 UserDataEntriesPerMask = (sizeof(size_t) << 3);
constexpr uint32 NumUserDataFlagsParts = ((MaxUserDataEntries + UserDataEntriesPerMask - 1) / UserDataEntriesPerMask);
typedef size_t UserDataFlags[NumUserDataFlagsParts];

// Represents the user data entries for a particular shader stage.
struct UserDataEntries
{
    uint32         entries[MaxUserDataEntries];
    UserDataFlags  dirty;   // Bitmasks of which user data entries have been set since the last time the entries
                            // were written to hardware.
    // Bitmasks of which user data entries have been ever set within this command buffer. If a bit is set, then the
    // corresponding user-data entry was set at least once in this command buffer.
    UserDataFlags  touched;
};

// Represents GFXIP state which is currently active within a command buffer.
struct PipelineState
{
    const Pipeline*           pPipeline;
    uint64                    apiPsoHash;
    const BorderColorPalette* pBorderColorPalette;

    union
    {
        struct
        {
            uint32 pipelineDirty             :  1;
            uint32 borderColorPaletteDirty   :  1;
            uint32 reserved                  : 30;
        };

        uint32 u32All;
    } dirtyFlags;   // Tracks which part of command buffer state is dirty
};

// State active necessary for compute operations. Used by compute and universal command buffers.
struct ComputeState
{
    // If the command buffer is in HSA ABI mode or not. In HSA mode it's not legal to call CmdSetUserData and when not
    // in HSA mode it's not legal to call CmdSetKernelArguments. This state also controls compute state save/restore
    // of user-data and kernel arguments and is itself saved and restored.
    bool                     hsaAbiMode;
    PipelineState            pipelineState;     // Common pipeline state
    DynamicComputeShaderInfo dynamicCsInfo;     // Info used during pipeline bind.
    UserDataEntries          csUserDataEntries;
    gpusize                  dynamicLaunchGpuVa;
    uint8*                   pKernelArguments;
};

union GfxCmdBufferStateFlags
{
    struct
    {
        uint32 perfCounterStarted        :  1;  // Track if perfExperiment has started with perfCounter or spmTrace
        uint32 perfCounterStopped        :  1;  // Track if perfExperiment has stopped with perfCounter or spmTrace
        uint32 sqttStarted               :  1;  // Track if perfExperiment has started with SQ Thread Trace enabled
        uint32 sqttStopped               :  1;  // Track if perfExperiment has stopped with SQ Thread Trace enabled
        uint32 clientPredicate           :  1;  // Track if client is currently using predication functionality.
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
        uint32 reserved                  : 16;
    };

    uint32 u32All;
};

struct GfxCmdBufferState
{
    GfxCmdBufferStateFlags flags;

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

// Tracks the state of a user-data table stored in GPU memory.  The table's contents are managed using embedded data
// and the CPU, or using GPU scratch memory and CE RAM.
struct UserDataTableState
{
    gpusize  gpuVirtAddr;   // GPU virtual address where the current copy of the table data is stored.
    // CPU address of the embedded-data allocation storing the current copy of the table data.  This can be null if
    // the table has not yet been uploaded to embedded data.
    uint32*  pCpuVirtAddr;
    struct
    {
        uint32  sizeInDwords : 31; // Size of one full instance of the user-data table, in DWORD's.
        uint32  dirty        :  1; // Indicates that the CPU copy of the user-data table is more up to date than the
                                   // copy currently in GPU memory and should be updated before the next dispatch.
    };
};

// Structure for getting CmdChunks for the IndirectCmdGenerator.
struct ChunkOutput
{
    CmdStreamChunk* pChunk;
    uint32          commandsInChunk;
    gpusize         embeddedDataAddr;
    uint32          embeddedDataSize;
    uint32          chainSizeInDwords;
};

constexpr uint32 AcqRelFenceResetVal = 0;

// Acquire/release synchronization event types for supported pipeline event.
enum class AcqRelEventType : uint32
{
    Eop    = 0x0,
    PsDone = 0x1,
    CsDone = 0x2,
    Count
};

// Acquire/release synchronization token structure.
struct AcqRelSyncToken
{
    union
    {
        struct
        {
            uint32 fenceVal : 30;
            uint32 type     :  2;
        };

        uint32 u32All;
    };
};

// =====================================================================================================================
// Abstract class for executing basic hardware-specific functionality common to GFXIP universal and compute command
// buffers.
class GfxCmdBuffer : public CmdBuffer
{

    // A useful shorthand for a vector of chunks.
    typedef ChunkVector<CmdStreamChunk*, 16, Platform> ChunkRefList;

    // Alias for a vector of pointers to gfx images.
    using FceRefCountsVector = Util::Vector<uint32*, MaxNumFastClearImageRefs, Platform>;

public:
    virtual Result Init(const CmdBufferInternalCreateInfo& internalInfo) override;

    virtual Result Begin(const CmdBufferBuildInfo& info) override;
    virtual Result End() override;
    virtual Result Reset(ICmdAllocator* pCmdAllocator, bool returnGpuMemory) override;

    virtual void CmdBindPipeline(
        const PipelineBindParams& params) override;

    virtual void CmdCopyMemoryByGpuVa(
        gpusize                 srcGpuVirtAddr,
        gpusize                 dstGpuVirtAddr,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions) override;

    virtual void CmdCopyImage(
        const IImage&          srcImage,
        ImageLayout            srcImageLayout,
        const IImage&          dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        const Rect*            pScissorRect,
        uint32                 flags) override;

    virtual void CmdCopyMemoryToImage(
        const IGpuMemory&            srcGpuMemory,
        const IImage&                dstImage,
        ImageLayout                  dstImageLayout,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions) override;

    virtual void CmdCopyImageToMemory(
        const IImage&                srcImage,
        ImageLayout                  srcImageLayout,
        const IGpuMemory&            dstGpuMemory,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions) override;

    virtual void CmdCopyMemoryToTiledImage(
        const IGpuMemory&                 srcGpuMemory,
        const IImage&                     dstImage,
        ImageLayout                       dstImageLayout,
        uint32                            regionCount,
        const MemoryTiledImageCopyRegion* pRegions) override;

    virtual void CmdCopyTiledImageToMemory(
        const IImage&                     srcImage,
        ImageLayout                       srcImageLayout,
        const IGpuMemory&                 dstGpuMemory,
        uint32                            regionCount,
        const MemoryTiledImageCopyRegion* pRegions) override;

    virtual void CmdCopyTypedBuffer(
        const IGpuMemory&            srcGpuMemory,
        const IGpuMemory&            dstGpuMemory,
        uint32                       regionCount,
        const TypedBufferCopyRegion* pRegions) override;

    virtual void CmdScaledCopyImage(
        const ScaledCopyInfo& copyInfo) override;

    virtual void CmdGenerateMipmaps(
        const GenMipmapsInfo& genInfo) override;

    virtual void CmdColorSpaceConversionCopy(
        const IImage&                     srcImage,
        ImageLayout                       srcImageLayout,
        const IImage&                     dstImage,
        ImageLayout                       dstImageLayout,
        uint32                            regionCount,
        const ColorSpaceConversionRegion* pRegions,
        TexFilter                         filter,
        const ColorSpaceConversionTable&  cscTable) override;

    virtual void CmdFillMemory(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           fillSize,
        uint32            data) override;

    virtual void CmdClearColorBuffer(
        const IGpuMemory& gpuMemory,
        const ClearColor& color,
        SwizzledFormat    bufferFormat,
        uint32            bufferOffset,
        uint32            bufferExtent,
        uint32            rangeCount = 0,
        const Range*      pRanges    = nullptr) override;

    virtual void CmdClearBoundColorTargets(
        uint32                          colorTargetCount,
        const BoundColorTarget*         pBoundColorTargets,
        uint32                          regionCount,
        const ClearBoundTargetRegion*   pClearRegions) override;

    virtual void CmdClearColorImage(
        const IImage&      image,
        ImageLayout        imageLayout,
        const ClearColor&  color,
        uint32             rangeCount,
        const SubresRange* pRanges,
        uint32             boxCount,
        const Box*         pBoxes,
        uint32             flags) override;

    virtual void CmdClearBoundDepthStencilTargets(
        float                         depth,
        uint8                         stencil,
        uint8                         stencilWriteMask,
        uint32                        samples,
        uint32                        fragments,
        DepthStencilSelectFlags       flag,
        uint32                        regionCount,
        const ClearBoundTargetRegion* pClearRegions) override;

    virtual void CmdClearDepthStencil(
        const IImage&      image,
        ImageLayout        depthLayout,
        ImageLayout        stencilLayout,
        float              depth,
        uint8              stencil,
        uint8              stencilWriteMask,
        uint32             rangeCount,
        const SubresRange* pRanges,
        uint32             rectCount,
        const Rect*        pRects,
        uint32             flags) override;

    virtual void CmdClearBufferView(
        const IGpuMemory& gpuMemory,
        const ClearColor& color,
        const void*       pBufferViewSrd,
        uint32            rangeCount = 0,
        const Range*      pRanges    = nullptr) override;

    virtual void CmdClearImageView(
        const IImage&     image,
        ImageLayout       imageLayout,
        const ClearColor& color,
        const void*       pImageViewSrd,
        uint32            rectCount = 0,
        const Rect*       pRects    = nullptr) override;

    virtual void CmdResolveImage(
        const IImage&             srcImage,
        ImageLayout               srcImageLayout,
        const IImage&             dstImage,
        ImageLayout               dstImageLayout,
        ResolveMode               resolveMode,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        uint32                    flags) override;

    virtual void CmdResolvePrtPlusImage(
        const IImage&                    srcImage,
        ImageLayout                      srcImageLayout,
        const IImage&                    dstImage,
        ImageLayout                      dstImageLayout,
        PrtPlusResolveType               resolveType,
        uint32                           regionCount,
        const PrtPlusImageResolveRegion* pRegions) override;

    void CmdCopyImageToPackedPixelImage(
        const IImage&          srcImage,
        const IImage&          dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        Pal::PackedPixelType   packPixelType) override;

    virtual void CmdPostProcessFrame(
        const CmdPostProcessFrameInfo& postProcessInfo,
        bool*                          pAddedGpuWork) override;

    void CmdPresentBlt(
        const IImage&   srcImage,
        const IImage&   dstImage,
        const Offset3d& dstOffset);

    virtual void CmdSuspendPredication(bool suspend) override
        { m_gfxCmdBufState.flags.packetPredicate = suspend ? 0 : 1; }

    virtual void CmdSaveComputeState(uint32 stateFlags) override;
    virtual void CmdRestoreComputeState(uint32 stateFlags) override;

    virtual bool IsQueryAllowed(QueryPoolType queryPoolType) const = 0;
    virtual void AddQuery(QueryPoolType queryPoolType, QueryControlFlags flags) = 0;
    virtual void RemoveQuery(QueryPoolType queryPoolType) = 0;

    gpusize TimestampGpuVirtAddr() const { return m_timestampGpuVa; }

    gpusize AcqRelFenceValBaseGpuVa() const { return m_acqRelFenceValGpuVa; }
    gpusize AcqRelFenceValGpuVa(AcqRelEventType type) const
        { return (m_acqRelFenceValGpuVa + sizeof(uint32) * static_cast<uint32>(type)); }

    uint32 GetNextAcqRelFenceVal(AcqRelEventType type)
    {
        m_acqRelFenceVals[static_cast<uint32>(type)]++;
        return m_acqRelFenceVals[static_cast<uint32>(type)];
    }

    GpuEvent* GetInternalEvent() { return m_pInternalEvent; }

    const ComputeState& GetComputeState() const { return m_computeState; }

    // Returns a pointer to the command stream associated with the specified engine type
    virtual CmdStream* GetCmdStreamByEngine(uint32 engineType) = 0;

    const GfxCmdBufferState& GetGfxCmdBufState() const { return m_gfxCmdBufState; }

    // Helper functions
    void OptimizePipePoint(HwPipePoint* pPipePoint) const;
    void OptimizeSrcCacheMask(uint32* pCacheMask) const;
    virtual void OptimizePipeAndCacheMaskForRelease(uint32* pStageMask, uint32* pAccessMask) const;
    void SetGfxCmdBufGfxBltState(bool gfxBltActive) { m_gfxCmdBufState.flags.gfxBltActive = gfxBltActive; }
    void SetGfxCmdBufCsBltState(bool csBltActive) { m_gfxCmdBufState.flags.csBltActive = csBltActive; }
    void SetGfxCmdBufCpBltState(bool cpBltActive) { m_gfxCmdBufState.flags.cpBltActive = cpBltActive; }
    void SetGfxCmdBufRasterKillDrawsState(bool rasterKillDrawsActive)
        { m_gfxCmdBufState.flags.rasterKillDrawsActive = rasterKillDrawsActive; }
    void SetGfxCmdBufGfxBltWriteCacheState(bool gfxWriteCacheDirty)
        { m_gfxCmdBufState.flags.gfxWriteCachesDirty = gfxWriteCacheDirty; }
    void SetGfxCmdBufCsBltWriteCacheState(bool csWriteCacheDirty)
        { m_gfxCmdBufState.flags.csWriteCachesDirty = csWriteCacheDirty; }
    void SetGfxCmdBufCpBltWriteCacheState(bool cpWriteCacheDirty)
        { m_gfxCmdBufState.flags.cpWriteCachesDirty = cpWriteCacheDirty; }
    void SetGfxCmdBufCpMemoryWriteL2CacheStaleState(bool cpMemoryWriteDirty)
        { m_gfxCmdBufState.flags.cpMemoryWriteL2CacheStale = cpMemoryWriteDirty; }
    void SetPrevCmdBufInactive() { m_gfxCmdBufState.flags.prevCmdBufActive = 0; }
    uint32 GetCurAcqRelFenceVal(AcqRelEventType type) const { return m_acqRelFenceVals[static_cast<uint32>(type)]; }

    // Execution fence value is updated at every BLT. Set it to the next event because its completion indicates all
    // prior BLTs have completed.
    void UpdateGfxCmdBufGfxBltExecEopFence()
    {
        m_gfxCmdBufState.fences.gfxBltExecEopFenceVal = GetCurAcqRelFenceVal(AcqRelEventType::Eop) + 1;
    }
    void UpdateGfxCmdBufCsBltExecFence()
    {
        m_gfxCmdBufState.fences.csBltExecEopFenceVal    = GetCurAcqRelFenceVal(AcqRelEventType::Eop) + 1;
        m_gfxCmdBufState.fences.csBltExecCsDoneFenceVal = GetCurAcqRelFenceVal(AcqRelEventType::CsDone) + 1;
    }
    // Execution fence value is updated at every draw when rasterization kill state is dirtied.
    // - If rasterization kill is enabled, set it UINT32_MAX to make rasterKillDrawsActive never to be cleared.
    // - If rasterization kill is disabled, set it to the next EOP event because its completion indicates all
    //   prior rasterization kill draws have completed.
    void UpdateGfxCmdBufRasterKillDrawsExecEopFence(bool setMaxFenceVal)
    {
        m_gfxCmdBufState.fences.rasterKillDrawsExecFenceVal =
            setMaxFenceVal ? UINT32_MAX : GetCurAcqRelFenceVal(AcqRelEventType::Eop) + 1;
    }
    // Cache write-back fence value is updated at every release event. Completion of current event indicates the cache
    // synchronization has completed too, so set it to current event fence value.
    void UpdateGfxCmdBufGfxBltWbEopFence(uint32 fenceVal)
    {
        m_gfxCmdBufState.fences.gfxBltWbEopFenceVal = fenceVal;
    }

    // Obtains a fresh command stream chunk from the current command allocator, for use as the target of GPU-generated
    // commands. The chunk is inserted onto the generated-chunks list so it can be recycled by the allocator after the
    // GPU is done with it.
    virtual void GetChunkForCmdGeneration(
        const IndirectCmdGenerator& generator,
        const Pipeline&             pipeline,
        uint32                      maxCommands,
        uint32                      numChunkOutputs,
        ChunkOutput*                pChunkOutputs) = 0;

    // CmdDispatch on the ACE CmdStream for Gfx10+ UniversalCmdBuffer only when multi-queue is supported by the engine.
    virtual void CmdDispatchAce(
        uint32      xDim,
        uint32      yDim,
        uint32      zDim)
    { PAL_NEVER_CALLED(); }

    virtual void CmdBeginPerfExperiment(IPerfExperiment* pPerfExperiment) override;
    virtual void CmdUpdatePerfExperimentSqttTokenMask(
        IPerfExperiment*              pPerfExperiment,
        const ThreadTraceTokenConfig& sqttTokenConfig) override;
    virtual void CmdEndPerfExperiment(IPerfExperiment* pPerfExperiment) override;

    virtual void AddPerPresentCommands(
        gpusize frameCountGpuAddr,
        uint32  frameCntReg) = 0;

    virtual void CpCopyMemory(gpusize dstAddr, gpusize srcAddr, gpusize numBytes) = 0;

    bool IsComputeSupported() const
        { return Util::TestAnyFlagSet(m_engineSupport, CmdBufferEngineSupport::Compute); }

    bool IsCpDmaSupported() const
        { return Util::TestAnyFlagSet(m_engineSupport, CmdBufferEngineSupport::CpDma); }

    bool IsGraphicsSupported() const
        { return Util::TestAnyFlagSet(m_engineSupport, CmdBufferEngineSupport::Graphics); }

    bool IsComputeStateSaved() const { return (m_computeStateFlags != 0); }

    virtual void CmdOverwriteRbPlusFormatForBlits(
        SwizzledFormat format,
        uint32         targetIndex) = 0;

    virtual void CmdOverwriteDisableViewportClampForBlits(
        bool disableViewportClamp) = 0;

    virtual uint32 GetUsedSize(CmdAllocType type) const override;

    PerfExperimentFlags PerfTracesEnabled() const { return m_cmdBufPerfExptFlags; }

    bool PerfCounterStarted() const
        { return m_gfxCmdBufState.flags.perfCounterStarted; }

    bool PerfCounterClosed() const
        { return m_gfxCmdBufState.flags.perfCounterStopped; }

    bool SqttStarted() const
        { return m_gfxCmdBufState.flags.sqttStarted; }

    bool SqttClosed() const
        { return m_gfxCmdBufState.flags.sqttStopped; }

    // Allows the queue to query the MALL perfmon info for this command buffer and
    // add it to the CmdBufInfo if need be.
    const DfSpmPerfmonInfo* GetDfSpmPerfmonInfo() const
        { return m_pDfSpmPerfmonInfo; }

    void AddFceSkippedImageCounter(GfxImage* pGfxImage);

    // Other Cmd* functions may call this function to notify our VRS copy state tracker of changes to VRS resources.
    // Provide a NOP default implementation, it should only be implemented on gfx9 universal command buffers.
    //
    // We take care to never overwrite HTile VRS data in universal command buffers (even in InitMaskRam) so only HW
    // bugs should overwrite the HTile VRS data. It's OK that DMA command buffers will clobber HTile VRS data on Init
    // because we'll redo the HTile update the first time the image is bound in a universal command buffer. Thus we
    // only need to call DirtyVrsDepthImage when a certain HW bug is triggered.
    virtual void DirtyVrsDepthImage(const IImage* pDepthImage) { }

    UploadFenceToken GetMaxUploadFenceToken() const { return m_maxUploadFenceToken; }

    virtual gpusize GetMeshPipeStatsGpuAddr() const
    {
        // Mesh/task shader pipeline stats not supported.
        PAL_ASSERT_ALWAYS();
        return 0;
    }

    virtual void CmdSetKernelArguments(
        uint32            firstArg,
        uint32            argCount,
        const void*const* ppValues) override;

protected:
    GfxCmdBuffer(
        const GfxDevice&           device,
        const CmdBufferCreateInfo& createInfo);
    virtual ~GfxCmdBuffer();

    virtual Result BeginCommandStreams(CmdStreamBeginFlags cmdStreamFlags, bool doReset) override;

    virtual void ResetState() override;

    virtual Pal::PipelineState* PipelineState(PipelineBindPoint bindPoint) = 0;

    void DescribeDispatch(Developer::DrawDispatchType cmdType, uint32 xDim, uint32 yDim, uint32 zDim);
    void DescribeDispatchOffset(uint32 xOffset, uint32 yOffset, uint32 zOffset, uint32 xDim, uint32 yDim, uint32 zDim);
    void DescribeDispatchIndirect();

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

    void UpdateUserDataTableCpu(
        UserDataTableState* pTable,
        uint32              dwordsNeeded,
        uint32              offsetInDwords,
        const uint32*       pSrcData,
        uint32              alignmentInDwords = 1);

    static void PAL_STDCALL CmdSetUserDataCs(
        ICmdBuffer*   pCmdBuffer,
        uint32        firstEntry,
        uint32        entryCount,
        const uint32* pEntryValues);

    void LeakPerPipelineStateChanges(
        const Pal::PipelineState& leakedPipelineState,
        const UserDataEntries&    leakedUserDataEntries,
        Pal::PipelineState*       pDestPipelineState,
        UserDataEntries*          pDestUserDataEntries);

    CmdStreamChunk* GetNextGeneratedChunk();

    void SetComputeState(const ComputeState& newComputeState, uint32 stateFlags);

    virtual void InheritStateFromCmdBuf(const GfxCmdBuffer* pCmdBuffer) = 0;

    virtual bool SupportsExecutionMarker() override { return true; }

    virtual void OptimizeBarrierReleaseInfo(
        uint32       pipePointCount,
        HwPipePoint* pPipePoints,
        uint32*      pCacheMask) const override;

    virtual void OptimizeAcqRelReleaseInfo(uint32* pStageMask, uint32* pAccessMask) const override;

    uint32            m_engineSupport;       // Indicates which engines are supported by the command buffer.
                                             // Populated by the GFXIP-specific layer.
    ComputeState      m_computeState;        // Currently bound compute command buffer state.
    ComputeState      m_computeRestoreState; // State saved by the previous call to CmdSaveCompputeState.
    GfxCmdBufferState m_gfxCmdBufState;      // Common gfx command buffer states.

    // This list of command chunks contains all of the command chunks containing commands which were generated on the
    // GPU using a compute shader. This list of chunks is associated with the command buffer, but won't contain valid
    // commands until after the command buffer has been executed by the GPU.
    ChunkRefList m_generatedChunkList;
    ChunkRefList m_retainedGeneratedChunkList;

    const PerfExperiment* m_pCurrentExperiment; // Current performance experiment.
    const GfxIpLevel      m_gfxIpLevel;

    UploadFenceToken m_maxUploadFenceToken;

private:
    void ReturnGeneratedCommandChunks(bool returnGpuMemory);
    CmdBufferEngineSupport GetPerfExperimentEngine() const;
    void ResetFastClearReferenceCounts();

    const GfxDevice&  m_device;

    // False if DeactivateQuery() has been called on a particular query type, true otherwise.
    // Specifically used for when Push/Pop state has been called. We only want to have a query active on code
    // executed by a client.
    bool  m_queriesActive[static_cast<size_t>(QueryPoolType::Count)];

    // Number of active queries in this command buffer.
    uint32  m_numActiveQueries[static_cast<size_t>(QueryPoolType::Count)];

    gpusize m_acqRelFenceValGpuVa; // GPU virtual address of 3-dwords memory used for acquire/release pipe event sync.
    uint32 m_acqRelFenceVals[static_cast<uint32>(AcqRelEventType::Count)];

    GpuEvent*  m_pInternalEvent;   // Internal Event for Release/Acquire based barrier.  CPU invisible.

    gpusize    m_timestampGpuVa;   // GPU virtual address of memory used for cache flush & inv timestamp events.

    uint32  m_computeStateFlags;   // The flags that CmdSaveComputeState was called with.

    FceRefCountsVector m_fceRefCountVec;

    const DfSpmPerfmonInfo* m_pDfSpmPerfmonInfo; // Cached pointer to the DF SPM perfmon info for the DF SPM perf
                                                 // experiment.

    PerfExperimentFlags m_cmdBufPerfExptFlags; // Flags that indicate which Performance Experiments are ongoing in
                                               // this CmdBuffer.

    PAL_DISALLOW_COPY_AND_ASSIGN(GfxCmdBuffer);
    PAL_DISALLOW_DEFAULT_CTOR(GfxCmdBuffer);
};

// =====================================================================================================================
// Helper function for resetting a user-data table which is managed using embdedded data or CE RAM at the beginning of
// a command buffer.
inline void ResetUserDataTable(
    UserDataTableState* pTable)
{
    pTable->pCpuVirtAddr = nullptr;
    pTable->gpuVirtAddr  = 0;
    pTable->dirty        = 0;
}

} // Pal
