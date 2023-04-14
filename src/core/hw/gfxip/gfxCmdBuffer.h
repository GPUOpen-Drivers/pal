/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

union PipelineStateFlags
{
    struct
    {
        uint32 pipeline             :  1;
        uint32 dynamicState         :  1;
        uint32 borderColorPalette   :  1;
        uint32 reserved             : 29;
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
    gpusize                  dynamicLaunchGpuVa;
    uint8*                   pKernelArguments;
};

union GfxCmdBufferStateFlags
{
    struct
    {
        uint32 clientPredicate  :  1;  // Track if client is currently using predication functionality.
        uint32 isGfxStatePushed :  1;  // If CmdSaveGraphicsState was called without a matching CmdRestoreGraphicsState.
        uint32 reserved         : 30;
    };

    uint32 u32All;
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

// =====================================================================================================================
// Abstract class for executing basic hardware-specific functionality common to GFXIP universal and compute command
// buffers in PM4 and PUP.
class GfxCmdBuffer : public CmdBuffer
{
    // A useful shorthand for a vector of chunks.
    typedef ChunkVector<CmdStreamChunk*, 16, Platform> ChunkRefList;

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

    virtual void CmdPostProcessFrame(
        const CmdPostProcessFrameInfo& postProcessInfo,
        bool*                          pAddedGpuWork) override;

    virtual void CmdPresentBlt(
        const IImage&   srcImage,
        const IImage&   dstImage,
        const Offset3d& dstOffset);

    virtual void CmdSaveGraphicsState() override;
    virtual void CmdRestoreGraphicsState() override;

    virtual void CmdSaveComputeState(uint32 stateFlags) override;
    virtual void CmdRestoreComputeState(uint32 stateFlags) override;

    virtual bool IsQueryAllowed(QueryPoolType queryPoolType) const = 0;
    virtual void AddQuery(QueryPoolType queryPoolType, QueryControlFlags flags) = 0;
    virtual void RemoveQuery(QueryPoolType queryPoolType) = 0;

    GpuEvent* GetInternalEvent() { return m_pInternalEvent; }

    // Returns a pointer to the command stream associated with the specified engine type
    virtual CmdStream* GetCmdStreamByEngine(uint32 engineType) = 0;

    const GfxCmdBufferStateFlags& GetGfxCmdBufStateFlags() const { return m_gfxCmdBufStateFlags; }

    // CmdDispatch on the ACE CmdStream for Gfx10+ UniversalCmdBuffer only when multi-queue is supported by the engine.
    virtual void CmdDispatchAce(DispatchDims size) { PAL_NEVER_CALLED(); }

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

    // Allows the queue to query the MALL perfmon info for this command buffer and
    // add it to the CmdBufInfo if need be.
    const DfSpmPerfmonInfo* GetDfSpmPerfmonInfo() const
    {
        return m_pDfSpmPerfmonInfo;
    }
    PerfExperimentFlags PerfTracesEnabled() const { return m_cmdBufPerfExptFlags; }

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

    virtual bool PerfCounterStarted() const = 0;
    virtual bool PerfCounterClosed() const = 0;
    virtual bool SqttStarted() const = 0;
    virtual bool SqttClosed() const = 0;

    static bool IsAnyUserDataDirty(const UserDataEntries* pUserDataEntries);

    virtual void CmdBindPipelineWithOverrides(
        const PipelineBindParams& params,
        SwizzledFormat            swizzledFormat,
        uint32                    targetIndex) {}

protected:
    GfxCmdBuffer(
        const GfxDevice&           device,
        const CmdBufferCreateInfo& createInfo);
    virtual ~GfxCmdBuffer();

    virtual Result BeginCommandStreams(CmdStreamBeginFlags cmdStreamFlags, bool doReset) override;

    virtual void ResetState() override;

    void DescribeDispatch(Developer::DrawDispatchType cmdType, DispatchDims size);
    void DescribeDispatchOffset(DispatchDims offset, DispatchDims launchSize, DispatchDims logicalSize);
    void DescribeDispatchIndirect();

    CmdBufferEngineSupport GetPerfExperimentEngine() const;

    static bool FilterSetUserData(UserDataArgs*        pUserDataArgs,
                                  const uint32*        pEntries,
                                  const UserDataFlags& userDataFlags);

    uint32                  m_engineSupport;       // Indicates which engines are supported by the command buffer.
                                                   // Populated by the GFXIP-specific layer.
    GfxCmdBufferStateFlags  m_gfxCmdBufStateFlags;

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

private:
    void ReturnGeneratedCommandChunks(bool returnGpuMemory);

    const GfxDevice& m_device;

    PAL_DISALLOW_COPY_AND_ASSIGN(GfxCmdBuffer);
    PAL_DISALLOW_DEFAULT_CTOR(GfxCmdBuffer);
};
} // Pal
