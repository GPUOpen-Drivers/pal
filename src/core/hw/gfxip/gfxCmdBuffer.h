/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "gfxBarrier.h"
#include "palDeque.h"
#include "palHashMap.h"
#include "palQueryPool.h"

namespace Util
{
namespace HsaAbi
{
class CodeObjectMetadata;
}
}

namespace Pal
{

// Forward decl's
class BarrierMgr;
class BorderColorPalette;
class CmdAllocator;
class CmdStream;
class GfxCmdBuffer;
class GfxDevice;
class GfxImage;
class GpuMemory;
class Image;
class IndirectCmdGenerator;
class IPerfExperiment;
class PerfExperiment;
class Pipeline;

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
constexpr uint32 MaxUserDataEntries = 160;

// Wide-bitmask of one flag for every user-data entry.
constexpr uint32 UserDataEntriesPerMask = (sizeof(size_t) << 3);
constexpr uint32 NumUserDataFlagsParts = ((MaxUserDataEntries + UserDataEntriesPerMask - 1) / UserDataEntriesPerMask);
typedef size_t UserDataFlags[NumUserDataFlagsParts];

// Constant timestamp reset/set value used in waiting CS idle or EOP.
constexpr uint32 ClearedTimestamp   = 0x11111111;
constexpr uint32 CompletedTimestamp = 0x22222222;

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

union PipelineStateFlags
{
    struct
    {
        uint32 pipeline             :  1;
        uint32 borderColorPalette   :  1;
        uint32 reserved             : 30;
    };

    uint32 u32All;
};

// Represents GFXIP state which is currently active within a command buffer.
struct PipelineState
{
    const Pipeline*           pPipeline;
    uint64                    apiPsoHash;
    const BorderColorPalette* pBorderColorPalette;
    PipelineStateFlags        dirtyFlags;
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
    uint8*                   pKernelArguments;
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

// Tracks the state of a user-data table stored in GPU memory.  The table's contents are managed using embedded data
// and the CPU, or using GPU scratch memory.
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

union GfxCmdBufferStateFlags
{
    struct
    {
        uint32 clientPredicate           :  1;  // Track if client is currently using predication functionality.
        uint32 isGfxStatePushed          :  1;  // If CmdSaveGraphicsState was called without a matching
                                                // CmdRestoreGraphicsStateInternal.
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

        uint32 csBltDirectWriteMisalignedMdDirty   :  1; // Track if CS direct write to misaligned md may be dirty.
        uint32 csBltIndirectWriteMisalignedMdDirty :  1; // Track if CS indirect write to misaligned md may be dirty.
        uint32 gfxBltDirectWriteMisalignedMdDirty  :  1; // Track if GFX direct write to misaligned md may be dirty.
        uint32 reserved                            : 14;
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
    } fences;
};

enum ExecuteIndirectV2GlobalSpill
{
    NoExecuteIndirectV2               = 0,
    ContainsExecuteIndirectV2         = 1,
    ContainsExecuteIndirectV2WithTask = 2
};

// Required argument for WriteWaitEop() call.
struct WriteWaitEopInfo
{
    uint8 hwGlxSync;  // Opaque HWL glx cache sync flags.
    uint8 hwRbSync;   // Opaque HWL RB cache sync flags; ignored on compute cmd buffer.
    uint8 hwAcqPoint; // Opaque HWL acquire point; ignored on compute cmd buffer.
    bool  waitCpDma;  // If wait CpDma to be idle.
    bool  disablePws; // If disable PWS RELEASE_MEM/ACQUIRE_MEM packet.
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
    virtual Result Begin(const CmdBufferBuildInfo& info) override;
    virtual Result Init(const CmdBufferInternalCreateInfo& internalInfo) override;
    virtual Result Reset(ICmdAllocator* pCmdAllocator, bool returnGpuMemory) override;
    virtual Result End() override;

    virtual void CmdCopyMemoryByGpuVa(
        gpusize                 srcGpuVirtAddr,
        gpusize                 dstGpuVirtAddr,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions) override;

    virtual void CmdCopyMemory(
        const IGpuMemory&       srcGpuMemory,
        const IGpuMemory&       dstGpuMemory,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions) override;

    virtual void CmdUpdateMemory(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           dataSize,
        const uint32*     pData) override;

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

    virtual void CmdScaledCopyTypedBufferToImage(
        const IGpuMemory&                       srcGpuMemory,
        const IImage&                           dstImage,
        ImageLayout                             dstImageLayout,
        uint32                                  regionCount,
        const TypedBufferImageScaledCopyRegion* pRegions) override;

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
        const IImage&         image,
        ImageLayout           imageLayout,
        const ClearColor&     color,
        const SwizzledFormat& clearFormat,
        uint32                rangeCount,
        const SubresRange*    pRanges,
        uint32                boxCount,
        const Box*            pBoxes,
        uint32                flags) override;

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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 910
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
#endif

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

    virtual void CmdPostProcessFrame(
        const CmdPostProcessFrameInfo& postProcessInfo,
        bool*                          pAddedGpuWork) override;

    virtual void CmdPresentBlt(
        const IImage&   srcImage,
        const IImage&   dstImage,
        const Offset3d& dstOffset);

    virtual void CmdBindPipeline(const PipelineBindParams& params) override;

    virtual void CmdSaveGraphicsState() override;
    virtual void CmdRestoreGraphicsStateInternal(bool trackBltActiveFlags = true) override;

    virtual void CmdSaveComputeState(uint32 stateFlags) override;
    virtual void CmdRestoreComputeStateInternal(uint32 stateFlags, bool trackBltActiveFlags = true) override;

    virtual bool IsQueryAllowed(QueryPoolType queryPoolType) const = 0;
    virtual void AddQuery(QueryPoolType queryPoolType, QueryControlFlags flags) = 0;
    virtual void RemoveQuery(QueryPoolType queryPoolType) = 0;

    GpuEvent* GetInternalEvent() { return m_pInternalEvent; }

    // Returns a pointer to the command stream associated with the specified engine type
    virtual CmdStream* GetCmdStreamByEngine(CmdBufferEngineSupport engineType) = 0;

    // CmdDispatch on the ACE CmdStream for Gfx10+ UniversalCmdBuffer only when multi-queue is supported by the engine.
    virtual void CmdDispatchAce(DispatchDims size) { PAL_NEVER_CALLED(); }

    virtual void CopyMemoryCp(gpusize dstAddr, gpusize srcAddr, gpusize numBytes) = 0;

    bool IsComputeSupported() const  { return Util::TestAnyFlagSet(m_engineSupport, CmdBufferEngineSupport::Compute); }
    bool IsCpDmaSupported() const    { return Util::TestAnyFlagSet(m_engineSupport, CmdBufferEngineSupport::CpDma); }
    bool IsGraphicsSupported() const { return Util::TestAnyFlagSet(m_engineSupport, CmdBufferEngineSupport::Graphics); }

    bool IsComputeStateSaved() const { return (m_computeStateFlags != 0); }

    virtual void CmdOverwriteColorExportInfoForBlits(SwizzledFormat format, uint32 targetIndex) = 0;

    // Allows the queue to query the MALL perfmon info for this command buffer and
    // add it to the CmdBufInfo if need be.
    const DfSpmPerfmonInfo* GetDfSpmPerfmonInfo() const { return m_pDfSpmPerfmonInfo; }
    PerfExperimentFlags     PerfTracesEnabled()   const { return m_cmdBufPerfExptFlags; }

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

    virtual uint32 GetUsedSize(CmdAllocType type) const override;

    bool PerfCounterStarted() const { return m_cmdBufState.flags.perfCounterStarted; }
    bool PerfCounterClosed()  const { return m_cmdBufState.flags.perfCounterStopped; }
    bool SqttStarted() const { return m_cmdBufState.flags.sqttStarted; }
    bool SqttClosed()  const { return m_cmdBufState.flags.sqttStopped; }

    static bool IsAnyUserDataDirty(const UserDataEntries* pUserDataEntries);

    virtual void CmdBindPipelineWithOverrides(
        const PipelineBindParams& params,
        SwizzledFormat            swizzledFormat,
        uint32                    targetIndex) {}

    virtual void CmdDuplicateUserData(PipelineBindPoint source, PipelineBindPoint dest) override;

    virtual void CmdSuspendPredication(bool suspend) override { m_cmdBufState.flags.packetPredicate = suspend ? 0 : 1; }

    virtual void CmdBarrier(const BarrierInfo& barrierInfo) override;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 885
    virtual uint32 CmdRelease(const AcquireReleaseInfo& releaseInfo) override;
#else
    virtual ReleaseToken CmdRelease(const AcquireReleaseInfo& releaseInfo) override;
#endif

    virtual void CmdAcquire(
        const AcquireReleaseInfo& acquireInfo,
        uint32                    syncTokenCount,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 885
        const uint32*             pSyncTokens) override;
#else
        const ReleaseToken*       pSyncTokens) override;
#endif

    virtual void CmdReleaseEvent(
        const AcquireReleaseInfo& releaseInfo,
        const IGpuEvent*          pGpuEvent) override;

    virtual void CmdAcquireEvent(
        const AcquireReleaseInfo& acquireInfo,
        uint32                    gpuEventCount,
        const IGpuEvent* const*   ppGpuEvents) override;

    virtual void CmdReleaseThenAcquire(const AcquireReleaseInfo& barrierInfo) override;

    virtual void CmdResolveQuery(
        const IQueryPool& queryPool,
        QueryResultFlags  flags,
        QueryType         queryType,
        uint32            startQuery,
        uint32            queryCount,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           dstStride) override;

    gpusize GetAcqRelFenceValBaseGpuVa() const { return m_acqRelFenceValGpuVa; }
    void SetAcqRelFenceValBaseGpuVa(gpusize addr) { m_acqRelFenceValGpuVa = addr; }

    gpusize AcqRelFenceValGpuVa(ReleaseTokenType type) const
    {
        PAL_ASSERT(m_acqRelFenceValGpuVa != 0);
        return (m_acqRelFenceValGpuVa + sizeof(uint32) * static_cast<uint32>(type));
    }

    uint32 GetCurAcqRelFenceVal(ReleaseTokenType type) const { return m_acqRelFenceVals[type]; }

    uint32 GetNextAcqRelFenceVal(ReleaseTokenType type)
    {
        m_acqRelFenceVals[type]++;

        // Make sure no overflow in fence value.
        const ReleaseToken maxFenceToken = { .u32All = UINT32_MAX };
        PAL_ASSERT(m_acqRelFenceVals[type] <= maxFenceToken.fenceValue);

        return m_acqRelFenceVals[type];
    }

    uint32 GetRetiredAcqRelFenceVal(ReleaseTokenType tokenType) const { return m_retiredAcqRelFenceVals[tokenType]; }

    void UpdateRetiredAcqRelFenceVal(ReleaseTokenType tokenType, uint32 fenceVal)
    {
        if (fenceVal > m_retiredAcqRelFenceVals[tokenType])
        {
            m_retiredAcqRelFenceVals[tokenType] = fenceVal;
        }
    }

    const ComputeState& GetComputeState() const { return m_computeState; }

    const GfxCmdBufferState& GetCmdBufState() const { return m_cmdBufState; }

    uint32 GetPacketPredicate() const { return m_cmdBufState.flags.packetPredicate; }

    // Note that this function only checks if BLT stall has been completed but not cache flushed.
    bool AnyBltActive() const { return (m_cmdBufState.flags.cpBltActive | m_cmdBufState.flags.csBltActive |
                                        m_cmdBufState.flags.gfxBltActive) != 0; }

    void SetGfxBltState(bool gfxBltActive) { m_cmdBufState.flags.gfxBltActive = gfxBltActive; }
    void SetCsBltState(bool csBltActive)   { m_cmdBufState.flags.csBltActive  = csBltActive; }
    void SetCpBltState(bool cpBltActive)
    {
        m_cmdBufState.flags.cpBltActive = cpBltActive;
        if (cpBltActive == false)
        {
            UpdateRetiredAcqRelFenceVal(ReleaseTokenCpDma, GetCurAcqRelFenceVal(ReleaseTokenCpDma));
        }
    }
    void SetGfxBltWriteCacheState(bool gfxWriteCacheDirty)
        { m_cmdBufState.flags.gfxWriteCachesDirty = gfxWriteCacheDirty; }
    void SetCsBltWriteCacheState(bool csWriteCacheDirty)
        { m_cmdBufState.flags.csWriteCachesDirty = csWriteCacheDirty; }
    void SetCpBltWriteCacheState(bool cpWriteCacheDirty)
        { m_cmdBufState.flags.cpWriteCachesDirty = cpWriteCacheDirty; }
    void SetCpMemoryWriteL2CacheStaleState(bool cpMemoryWriteDirty)
        { m_cmdBufState.flags.cpMemoryWriteL2CacheStale = cpMemoryWriteDirty; }

    bool IsBltWriteMisalignedMdDirty() const
        { return (m_cmdBufState.flags.csBltDirectWriteMisalignedMdDirty   |
                  m_cmdBufState.flags.csBltIndirectWriteMisalignedMdDirty |
                  m_cmdBufState.flags.gfxBltDirectWriteMisalignedMdDirty) != 0; }

    // For internal RPM BLT with misaligned metadata write access, we need track the detailed access mode. This helps
    // determine the optimal misaligned metadata workaround which requires a GL2 flush and invalidation.
    void SetCsBltDirectWriteMisalignedMdState(bool dirty)
        { m_cmdBufState.flags.csBltDirectWriteMisalignedMdDirty |= dirty;}
    void SetCsBltIndirectWriteMisalignedMdState(bool dirty)
        { m_cmdBufState.flags.csBltIndirectWriteMisalignedMdDirty |= dirty;}
    void SetGfxBltDirectWriteMisalignedMdState(bool dirty)
        { m_cmdBufState.flags.gfxBltDirectWriteMisalignedMdDirty |= dirty;}

    // When a GL2 flush and invalidation is issued, can clear the tracking flags to avoid duplicated GL2 flush and
    // invalidation. Note that can clear the flag safely only if related BLT is done (xxxBltActive = 0).
    void ClearBltWriteMisalignMdState()
    {
        GfxCmdBufferStateFlags* pFlags = &m_cmdBufState.flags;

        pFlags->csBltDirectWriteMisalignedMdDirty   &= pFlags->csBltActive;
        pFlags->csBltIndirectWriteMisalignedMdDirty &= pFlags->csBltActive;
        pFlags->gfxBltDirectWriteMisalignedMdDirty  &= pFlags->gfxBltActive;
    }

    // Execution fence value is updated at every BLT. Set it to the next event because its completion indicates all
    // prior BLTs have completed.
    void UpdateGfxBltExecEopFence()
    {
        m_cmdBufState.fences.gfxBltExecEopFenceVal = GetCurAcqRelFenceVal(ReleaseTokenEop) + 1;
    }
    void UpdateCsBltExecFence()
    {
        m_cmdBufState.fences.csBltExecEopFenceVal    = GetCurAcqRelFenceVal(ReleaseTokenEop) + 1;
        m_cmdBufState.fences.csBltExecCsDoneFenceVal = GetCurAcqRelFenceVal(ReleaseTokenCsDone) + 1;
    }
    // Cache write-back fence value is updated at every release event. Completion of current event indicates the cache
    // synchronization has completed too, so set it to current event fence value.
    void UpdateGfxBltWbEopFence(uint32 fenceVal) { m_cmdBufState.fences.gfxBltWbEopFenceVal = fenceVal; }

    virtual void CmdBeginPerfExperiment(IPerfExperiment* pPerfExperiment) override;
    virtual void CmdUpdatePerfExperimentSqttTokenMask(
        IPerfExperiment*              pPerfExperiment,
        const ThreadTraceTokenConfig& sqttTokenConfig) override;
    virtual void CmdEndPerfExperiment(IPerfExperiment* pPerfExperiment) override;

    virtual void CmdSetKernelArguments(
        uint32            firstArg,
        uint32            argCount,
        const void*const* ppValues) override;

    // Helper functions
    void AddFceSkippedImageCounter(GfxImage* pGfxImage);

    void SetPrevCmdBufInactive() { m_cmdBufState.flags.prevCmdBufActive = 0; }

    // Obtains a fresh command stream chunk from the current command allocator, for use as the target of GPU-generated
    // commands. The chunk is inserted onto the generated-chunks list so it can be recycled by the allocator after the
    // GPU is done with it.
    virtual void GetChunkForCmdGeneration(
        const Pal::IndirectCmdGenerator& generator,
        const Pipeline&                  pipeline,
        uint32                           maxCommands,
        uint32                           numChunkOutputs,
        ChunkOutput*                     pChunkOutputs) { PAL_NOT_IMPLEMENTED(); }

    gpusize TimestampGpuVirtAddr();

    // hwGlxSync/hwRbSync: opaque HWL cache sync flags. hwRbSync will be ignored for compute cmd buffer.
    virtual uint32* WriteWaitEop(WriteWaitEopInfo info, uint32* pCmdSpace) = 0;

    virtual uint32* WriteWaitCsIdle(uint32* pCmdSpace) = 0;

    const GfxDevice& GetGfxDevice() const { return m_device; }

    void SetExecuteIndirectV2GlobalSpill(bool hasTask);

    ExecuteIndirectV2GlobalSpill ExecuteIndirectV2NeedsGlobalSpill() const { return m_executeIndirectV2GlobalSpill; }

    void CopyHsaKernelArgsToMem(
        const DispatchDims&                     offset,
        const DispatchDims&                     threads,
        const DispatchDims&                     logicalSize,
        gpusize*                                pKernArgsGpuVa,
        uint32*                                 pLdsSize,
        const Util::HsaAbi::CodeObjectMetadata& metadata);

protected:
    GfxCmdBuffer(
        const GfxDevice&           device,
        const CmdBufferCreateInfo& createInfo,
        const GfxBarrierMgr*       pBarrierMgr);
    virtual ~GfxCmdBuffer();

    virtual Result BeginCommandStreams(CmdStreamBeginFlags cmdStreamFlags, bool doReset) override;

    virtual void ResetState() override;

    virtual void DescribeDraw(Developer::DrawDispatchType cmdType, bool includedGangedAce = false);
    void DescribeExecuteIndirectCmds(GfxCmdBuffer* pCmdBuf, uint32 genType);
    void DescribeDispatch(
        Developer::DrawDispatchType cmdType,
        DispatchDims                size,
        DispatchInfoFlags           infoFlags);
    void DescribeDispatchOffset(DispatchDims offset, DispatchDims launchSize, DispatchDims logicalSize);
    void DescribeDispatchIndirect();

    CmdBufferEngineSupport GetPerfExperimentEngine() const;

    void SetComputeState(const ComputeState& newComputeState, uint32 stateFlags);

    void LeakPerPipelineStateChanges(
        const Pal::PipelineState& leakedPipelineState,
        const UserDataEntries&    leakedUserDataEntries,
        Pal::PipelineState*       pDestPipelineState,
        UserDataEntries*          pDestUserDataEntries);

    virtual void InheritStateFromCmdBuf(const GfxCmdBuffer* pCmdBuffer)
        { SetComputeState(pCmdBuffer->GetComputeState(), ComputeStateAll); }

    virtual bool OptimizeAcqRelReleaseInfo(
        BarrierType   barrierType,
        const IImage* pImage,
        uint32*       pSrcStageMask,
        uint32*       pSrcAccessMask,
        uint32*       pDstStageMask,
        uint32*       pDstAccessMask) const override;

    void UpdateUserDataTableCpu(
        UserDataTableState* pTable,
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

    // Helper function for resetting a user-data table which is managed using embdedded data at the beginning of
    // a command buffer.
    inline void ResetUserDataTable(UserDataTableState* pTable)
    {
        pTable->pCpuVirtAddr = nullptr;
        pTable->gpuVirtAddr  = 0;
        pTable->dirty        = 0;
    }

    static void UpdateUserData(
        UserDataEntries* pState,
        uint32           firstEntry,
        uint32           entryCount,
        const uint32*    pEntryValues);

    static void PAL_STDCALL CmdUpdateUserDataCs(
        ICmdBuffer*   pCmdBuffer,
        uint32        firstEntry,
        uint32        entryCount,
        const uint32* pEntryValues);

    static void SetUserData(
        uint32           firstEntry,
        uint32           entryCount,
        UserDataEntries* pEntries,
        const uint32*    pEntryValues);

    static void PAL_STDCALL CmdSetUserDataCs(
        ICmdBuffer*   pCmdBuffer,
        uint32        firstEntry,
        uint32        entryCount,
        const uint32* pEntryValues);

    static bool FilterSetUserData(
        UserDataArgs*        pUserDataArgs,
        const uint32*        pEntries,
        const UserDataFlags& userDataFlags);

    CmdStreamChunk* GetNextGeneratedChunk();
    CmdStreamChunk* GetNextLargeGeneratedChunk();

    uint32                  m_engineSupport;       // Indicates which engines are supported by the command buffer.
                                                   // Populated by the GFXIP-specific layer.

    // This list of command chunks contains all of the command chunks containing commands which were generated on the
    // GPU using a compute shader. This list of chunks is associated with the command buffer, but won't contain valid
    // commands until after the command buffer has been executed by the GPU.
    ChunkRefList            m_generatedChunkList;
    ChunkRefList            m_retainedGeneratedChunkList;

    const PerfExperiment*   m_pCurrentExperiment;  // Current performance experiment.
    const GfxIpLevel        m_gfxIpLevel;

    UploadFenceToken        m_maxUploadFenceToken;

    const DfSpmPerfmonInfo* m_pDfSpmPerfmonInfo;   // Cached pointer to the DF SPM perfmon info for the DF SPM perf
                                                   // experiment.
    PerfExperimentFlags     m_cmdBufPerfExptFlags; // Flags that indicate which Performance Experiments are ongoing in
                                                   // this CmdBuffer.

    uint32                  m_computeStateFlags;   // The flags that CmdSaveComputeState was called with.
    GpuEvent*               m_pInternalEvent;      // Internal Event for Release/Acquire based barrier.  CPU invisible.

    FceRefCountsVector      m_fceRefCountVec;

    GfxCmdBufferState       m_cmdBufState;         // Common gfx command buffer states.

    ComputeState            m_computeState;        // Currently bound compute command buffer state.
    ComputeState            m_computeRestoreState; // State saved by the previous call to CmdSaveComputeState.

    // DX12 requires that the command-stream chunks generated by indirect command generators honor the command
    // buffer's predication state. Since we cannot predicate the chain packet used to launch the indirect command
    // chunks, we need to save the predicate values to a location in embedded data to check when executing a call
    // to CmdExecuteIndirectCmds().
    // Vulkan similarly requires predication state for predicating compute workload discard when doing gang submit.
    // NOTE: SET_PREDICATION is not supported on compute queue so we emulate it using COND_EXEC
    // NOTE: m_cmdBufState.flags.clientPredicate and m_cmdBufState.flags.packetPredicate bits are 0 when:
    //       - app disables/resets predication
    //       - driver forces them to 0 when a new command buffer begins
    // NOTE: m_gfxCmdBuff.packetPredicate is also temporarily overridden by the driver during some operations
    gpusize                 m_predGpuAddr;
    gpusize                 m_inheritedPredGpuAddr; // Holds pred GPU address for nested cmdbuff execution

    gpusize                 m_globalInternalTableAddr; // If non-zero, the low 32-bits of the
                                                       // global internal table were written here.

    const GfxBarrierMgr*    m_pBarrierMgr; // Manager of all barrier calls.

private:
    void ReturnGeneratedCommandChunks(bool returnGpuMemory);
    void ResetFastClearReferenceCounts();

    const GfxDevice& m_device;

    const bool m_splitBarriers;    // If need split barriers with multiple planes into barriers with single plane.

    gpusize m_timestampGpuVa;      // GPU virtual address of memory used for cache flush & inv timestamp events.

    // Number of active queries in this command buffer.
    uint32 m_numActiveQueries[size_t(QueryPoolType::Count)];

    // False if DeactivateQuery() has been called on a particular query type, true otherwise.
    // Specifically used for when Push/Pop state has been called. We only want to have a query active on code
    // executed by a client.
    bool m_queriesActive[size_t(QueryPoolType::Count)];

    gpusize m_acqRelFenceValGpuVa; // GPU virtual address of 3-dwords memory used for acquire/release pipe event sync.
    uint32  m_acqRelFenceVals[ReleaseTokenCount];        // Outstanding released acquire release fence values per type.
    uint32  m_retiredAcqRelFenceVals[ReleaseTokenCount]; // The latest retired fence values acquired at ME/PFP.

    // Indicates that this CmdBuffer contains an ExecuteIndirectV2 PM4 which will have multiple instances of
    // SpilledUserData Tables that will use the Global SpillTable Buffer.
    ExecuteIndirectV2GlobalSpill m_executeIndirectV2GlobalSpill;

    PAL_DISALLOW_COPY_AND_ASSIGN(GfxCmdBuffer);
    PAL_DISALLOW_DEFAULT_CTOR(GfxCmdBuffer);
};
} // Pal
