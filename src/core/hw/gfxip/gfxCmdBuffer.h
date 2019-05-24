/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/fence.h"
#include "core/platform.h"
#include "palDeque.h"
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
constexpr uint32 UserDataEntriesPerMask = (sizeof(uint16) << 3);
constexpr uint32 NumUserDataFlagsParts  = (MaxUserDataEntries / UserDataEntriesPerMask);
typedef uint16 UserDataFlags[NumUserDataFlagsParts];

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
    PipelineState            pipelineState;     // Common pipeline state
    DynamicComputeShaderInfo dynamicCsInfo;     // Info used during pipeline bind.
    UserDataEntries          csUserDataEntries;
};

struct GfxCmdBufferState
{
    union
    {
        struct
        {
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
            uint32 reserved                  : 22;
        };

        uint32 u32All;
    } flags;
};

// Internal flags for CmdScaledCopyImage.
union ScaledCopyInternalFlags
{
    struct
    {
        uint32 srcSrgbAsUnorm :  1; // Treat the source image's SRGB data as UNORM data.
        uint32 reserved       : 31;
    };
    uint32 u32All;
};

// Tracks the state of a user-data table stored in GPU memory.  The table's contents are managed using embedded data
// and the CPU, or using GPU scratch memory and CE RAM.
struct UserDataTableState
{
    gpusize  gpuVirtAddr;   // GPU virtual address where the current copy of the table data is stored.
    // CPU address of the embedded-data allocation storing the current copy of the table data.  This can be null if
    // the table has not yet been uploaded to embedded data.
    uint32*  pCpuVirtAddr;
    // Offset into CE RAM (in bytes!) where the staging area is located. This can be zero if the table is being
    // managed using CPU updates instead of the constant engine.
    uint32   ceRamOffset;
    struct
    {
        uint32  sizeInDwords : 31; // Size of one full instance of the user-data table, in DWORD's.
        uint32  dirty        :  1; // Indicates that the CPU copy of the user-data table is more up to date than the
                                   // copy currently in GPU memory and should be updated before the next dispatch.
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

    virtual void CmdCopyImage(
        const IImage&          srcImage,
        ImageLayout            srcImageLayout,
        const IImage&          dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
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
        const ScaledCopyInfo&        copyInfo) override;

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
        float                           depth,
        uint8                           stencil,
        uint32                          samples,
        uint32                          fragments,
        DepthStencilSelectFlags         flag,
        uint32                          regionCount,
        const ClearBoundTargetRegion*   pClearRegions) override;

    virtual void CmdClearDepthStencil(
        const IImage&      image,
        ImageLayout        depthLayout,
        ImageLayout        stencilLayout,
        float              depth,
        uint8              stencil,
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
        const ImageResolveRegion* pRegions) override;

    void CmdCopyImageToPackedPixelImage(
        const IImage&          srcImage,
        const IImage&          dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        Pal::PackedPixelType   packPixelType) override;

    void CmdPresentBlt(
        const IImage&   srcImage,
        const IImage&   dstImage,
        const Offset3d& dstOffset);

    virtual void CmdSaveComputeState(uint32 stateFlags) override;
    virtual void CmdRestoreComputeState(uint32 stateFlags) override;

    virtual bool IsQueryAllowed(QueryPoolType queryPoolType) const = 0;
    virtual void AddQuery(QueryPoolType queryPoolType, QueryControlFlags flags) = 0;
    virtual void RemoveQuery(QueryPoolType queryPoolType) = 0;

    gpusize TimestampGpuVirtAddr() const { return m_timestampGpuVa; }
    GpuEvent* GetInternalEvent() { return m_pInternalEvent; }

    virtual void PushGraphicsState() = 0;
    virtual void PopGraphicsState()  = 0;

    const ComputeState& GetComputeState() const { return m_computeState; }

    // Returns a pointer to the command stream associated with the specified engine type
    virtual CmdStream* GetCmdStreamByEngine(uint32 engineType) = 0;

    GfxCmdBufferState GetGfxCmdBufState() const { return m_gfxCmdBufState; }

    // Helper functions
    HwPipePoint OptimizeHwPipePostBlit() const;
    uint32 ConvertToInternalPipelineStageMask(uint32 stageMask) const;
    void SetGfxCmdBufGfxBltState(bool gfxBltActive) { m_gfxCmdBufState.flags.gfxBltActive = gfxBltActive; }
    void SetGfxCmdBufCsBltState(bool csBltActive) { m_gfxCmdBufState.flags.csBltActive = csBltActive; }
    void SetGfxCmdBufCpBltState(bool cpBltActive) { m_gfxCmdBufState.flags.cpBltActive = cpBltActive; }
    void SetGfxCmdBufGfxBltWriteCacheState(bool gfxWriteCacheDirty)
        { m_gfxCmdBufState.flags.gfxWriteCachesDirty = gfxWriteCacheDirty; }
    void SetGfxCmdBufCsBltWriteCacheState(bool csWriteCacheDirty)
        { m_gfxCmdBufState.flags.csWriteCachesDirty = csWriteCacheDirty; }
    void SetGfxCmdBufCpBltWriteCacheState(bool cpWriteCacheDirty)
        { m_gfxCmdBufState.flags.cpWriteCachesDirty = cpWriteCacheDirty; }
    void SetGfxCmdBufCpMemoryWriteL2CacheStaleState(bool cpMemoryWriteDirty)
        { m_gfxCmdBufState.flags.cpMemoryWriteL2CacheStale = cpMemoryWriteDirty; }
    void SetPrevCmdBufInactive() { m_gfxCmdBufState.flags.prevCmdBufActive = 0; }

    // Obtains a fresh command stream chunk from the current command allocator, for use as the target of GPU-generated
    // commands. The chunk is inserted onto the generated-chunks list so it can be recycled by the allocator after the
    // GPU is done with it.
    virtual CmdStreamChunk* GetChunkForCmdGeneration(
        const IndirectCmdGenerator& generator,
        const Pipeline&             pipeline,
        uint32                      maxCommands,
        uint32*                     pCommandsInChunk,
        gpusize*                    pEmbeddedDataAddr,
        uint32*                     pEmbeddedDataSize) = 0;

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

    virtual void CmdOverwriteRbPlusFormatForBlits(
        SwizzledFormat format,
        uint32         targetIndex) = 0;

    void EnableSpmTrace() { m_spmTraceEnabled = true; }
    bool SpmTraceEnabled() const { return m_spmTraceEnabled; }

    Result AddFceSkippedImageCounter(GfxImage* pGfxImage);

protected:
    GfxCmdBuffer(
        const GfxDevice&           device,
        const CmdBufferCreateInfo& createInfo);
    virtual ~GfxCmdBuffer();

    virtual Result BeginCommandStreams(CmdStreamBeginFlags cmdStreamFlags, bool doReset) override;

    virtual void ResetState() override;

    virtual Pal::PipelineState* PipelineState(PipelineBindPoint bindPoint) = 0;

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
        const uint32*       pSrcData);

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

    GpuEvent*  m_pInternalEvent;    // Internal Event for Release/Acquire based barrier.  CPU invisible.
    gpusize    m_timestampGpuVa;    // GPU virtual address of memory used for cache flush & inv timestamp events.

    uint32  m_computeStateFlags;       // The flags that CmdSaveComputeState was called with.
    bool    m_spmTraceEnabled;         // Used to indicate whether Spm Trace has been enabled through this command
                                       // buffer so that appropriate submit-time operations can be done.

    FceRefCountsVector m_fceRefCountVec;

    PAL_DISALLOW_COPY_AND_ASSIGN(GfxCmdBuffer);
    PAL_DISALLOW_DEFAULT_CTOR(GfxCmdBuffer);
};

// =====================================================================================================================
// Helper function for resetting a user-data table which is managed using embdedded data or CE RAM at the beginning of
// a command buffer.
void PAL_INLINE ResetUserDataTable(
    UserDataTableState* pTable)
{
    pTable->pCpuVirtAddr = nullptr;
    pTable->gpuVirtAddr  = 0;
    pTable->dirty        = 0;
}

} // Pal
