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

#include "core/cmdAllocator.h"
#include "core/device.h"
#include "g_coreSettings.h"
#include "core/gpuEvent.h"
#include "palAssert.h"
#include "palCmdBuffer.h"
#include "palFile.h"
#include "palVector.h"

namespace Util { class VirtualLinearAllocator; }

namespace Pal
{

class      CmdStream;
enum class ShaderType : uint32;

// Defines additional information describing command buffer objects beyond what PAL clients are able to specify.
struct CmdBufferInternalCreateInfo
{
    union
    {
        struct
        {
            uint32 isInternal           :  1;   // Whether this command buffer is created on an internal, hidden queue
            uint32 reserved1            :  1;
            uint32 reserved             : 30;
        };                              // Command buffer create flags

        uint32 u32all;                  // Value of the bitfield
    } flags;
};

// The following structures are used in dumping command buffers to files.
//
// The general structure of the dump file is as follows:
//
//    * First comes a header (CmdBufDumpFileHeader).
//      * This includes versioning information and the ASIC family.
//      * Also includes the starting index of the IB2 chunks (0 if no IB2 chunks)
//    * Next, we read the file until we're out of data or reach an invalid header. Until that happens:
//      * Find a list header (CmdBufferListHeader). It contains:
//        * The engine index (universal, compute, SDMA, etc).
//          * What the engine index means might be dependent on ASIC family.
//        * The number of command buffer (chunks) that follow.
//        * For each command buffer (or chunk) there is another header (CmdBufferDumpHeader), which contains:
//          * The size of the command buffer, in bytes.
//          * The sub-engine ID. (0 - DE, 2 - CE preamble 3 - compute, 4 - SDMA, see GetSubEngineId() for details)

// Structure defining top of binary command buffer dump file.
struct CmdBufferDumpFileHeader
{
    uint32      size;               // Size of this structure in bytes.
    uint32      headerVersion;      // Version of header. Should be 1.
    uint32      asicFamily;         // ASIC family
    uint32      asicRevision;       // ASIC revision
    uint32      ib2Start;           // Chunk index of first IB2 (by dump order), 0 if there is no IB2
};

// Structure defining header for list of command buffers.
struct CmdBufferListHeader
{
    uint32      size;               // Size of this structure in bytes.
    uint32      engineIndex;        // Engine index for which this command buffer is destined
    uint32      count;              // Number of command buffers that follow.
};

// Structure defining header for each command buffer.
struct CmdBufferDumpHeader
{
    uint32      size;               // Size of this structure in bytes.
    uint32      cmdBufferSize;      // Size of the command buffer in bytes.
    uint32      subEngineId;        // Sub-engine.
};

// Structure defining header for an IB2 buffer.
struct CmdBufferIb2DumpHeader
{
    uint32      size;               // Size of this structure in bytes.
    uint32      cmdBufferSize;      // Size of the command buffer in bytes.
    uint32      subEngineId;        // Sub-engine.
    uint64      gpuVa;              // GPU virtual address of the IB2
};

// Structure holding information needed to dump IB2s
struct Ib2DumpInfo
{
    const uint32*       pCpuAddress;      // CPU address of the commands
    const uint32        ib2Size;          // Length of the dump in bytes
    const uint64        gpuVa;            // GPU virtual address of the commands
    const EngineType    engineType;       // Engine Type
    const SubEngineType subEngineType;    // SubEngine Type
};

// Gets the subEngineId to put in headers when dumping
extern uint32 GetSubEngineId(
    const SubEngineType subEngineType,
    const EngineType    engineType,
    const bool          isPreamble);

// The available states of command buffer recording.
enum class CmdBufferRecordState : uint32
{
    Building   = 0,    // CmdBuffer is actively recording
    Executable = 1,    // Recording has ended, ready to submit
    Reset      = 2,    // CmdBuffer has been reset and not re-begun
};

// =====================================================================================================================
// A command buffer can be executed by the GPU multiple times and recycled, provided the command buffer is not pending
// execution on the GPU when it is recycled.
//
// Command buffers are fully independent and there is no persistence of GPU state between submitted command buffers.
// When a new command buffer is recorded, the state is undefined.  All relevant state must be explicitly set by the
// client before state-dependent operations such as draws and dispatches.
class CmdBuffer : public ICmdBuffer
{
    // A useful shorthand for a vector of chunks.
    typedef ChunkVector<CmdStreamChunk*, 16, Platform> ChunkRefList;

    // A useful shorthand for a vector of IB2 Infos
    typedef Util::Vector<Ib2DumpInfo, 4, Platform> Ib2DumpInfoVec;

public:
    // NOTE: Part of the public IDestroyable interface.
    virtual void Destroy() override
    {
        Reset(nullptr, true);
        this->~CmdBuffer();
    }

    void DestroyInternal();

    virtual Result Init(const CmdBufferInternalCreateInfo& internalInfo);

    virtual Result Begin(const CmdBufferBuildInfo& info) override;
    virtual Result End() override;
    virtual Result Reset(ICmdAllocator* pCmdAllocator, bool returnGpuMemory) override;

    //size is in units of DWORDs
    virtual uint32 GetEmbeddedDataLimit() const override
        { return m_pCmdAllocator->ChunkSize(EmbeddedDataAlloc) / sizeof(uint32); }

    virtual uint32 GetLargeEmbeddedDataLimit() const override
        { return m_pCmdAllocator->ChunkSize(LargeEmbeddedDataAlloc) / sizeof(uint32); }

    virtual void CmdNop(
        const void* pPayload,
        uint32      payloadSize) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdBarrier(const BarrierInfo& barrierInfo) override;

    virtual bool OptimizeAcqRelReleaseInfo(
        BarrierType   barrierType,
        const IImage* pImage,
        uint32*       pSrcStageMask,
        uint32*       pSrcAccessMask,
        uint32*       pDstStageMask,
        uint32*       pDstAccessMask) const override { return false; }

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

    virtual void CmdBindPipeline(
        const PipelineBindParams& params) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdBindMsaaState(const IMsaaState* pMsaaState) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSaveGraphicsState() override
        { PAL_NEVER_CALLED(); }

    // For PAL internal: should call CmdRestoreGraphicsStateInternal() instead of CmdRestoreComputeState(); otherwise
    //                   there may be potential issue due to miss tracking blt active flags for barriers.
    virtual void CmdRestoreGraphicsStateInternal(bool trackBltActiveFlags = true) { PAL_NEVER_CALLED(); }

    virtual void CmdRestoreGraphicsState() override { CmdRestoreGraphicsStateInternal(false); }

    virtual void CmdBindColorBlendState(const IColorBlendState* pColorBlendState) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdBindDepthStencilState(const IDepthStencilState* pDepthStencilState) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetBlendConst(const BlendConstParams& params) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetDepthBounds(
        const DepthBoundsParams& params) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetInputAssemblyState(const InputAssemblyStateParams& params) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetStencilRefMasks(const StencilRefMaskParams& params) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdDuplicateUserData(
        PipelineBindPoint source,
        PipelineBindPoint dest) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetKernelArguments(
        uint32            firstArg,
        uint32            argCount,
        const void*const* ppValues) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetVertexBuffers(const VertexBufferViews& bufferViews) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdBindIndexData(
        gpusize   gpuAddr,
        uint32    indexCount,
        IndexType indexType) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdBindTargets(
        const BindTargetParams& params) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdBindStreamOutTargets(
        const BindStreamOutTargetParams& params) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetTriangleRasterState(
        const TriangleRasterStateParams& params) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetPointLineRasterState(
        const PointLineRasterStateParams& params) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetLineStippleState(
        const LineStippleStateParams& params) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetDepthBiasState(
        const DepthBiasParams& params) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetMsaaQuadSamplePattern(
        uint32                       numSamplesPerPixel,
        const MsaaQuadSamplePattern& quadSamplePattern) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetViewports(
        const ViewportParams& params) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetScissorRects(
        const ScissorRectParams& params) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetGlobalScissor(
        const GlobalScissorParams& params) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdCopyMemory(
        const IGpuMemory&       srcGpuMemory,
        const IGpuMemory&       dstGpuMemory,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdCopyMemoryByGpuVa(
        gpusize                 srcGpuVirtAddr,
        gpusize                 dstGpuVirtAddr,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdCopyImage(
        const IImage&          srcImage,
        ImageLayout            srcImageLayout,
        const IImage&          dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        const Rect*            pScissorRect,
        uint32                 flags) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdCopyMemoryToImage(
        const IGpuMemory&            srcGpuMemory,
        const IImage&                dstImage,
        ImageLayout                  dstImageLayout,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdCopyImageToMemory(
        const IImage&                srcImage,
        ImageLayout                  srcImageLayout,
        const IGpuMemory&            dstGpuMemory,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdCopyMemoryToTiledImage(
        const IGpuMemory&                 srcGpuMemory,
        const IImage&                     dstImage,
        ImageLayout                       dstImageLayout,
        uint32                            regionCount,
        const MemoryTiledImageCopyRegion* pRegions) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdCopyTiledImageToMemory(
        const IImage&                     srcImage,
        ImageLayout                       srcImageLayout,
        const IGpuMemory&                 dstGpuMemory,
        uint32                            regionCount,
        const MemoryTiledImageCopyRegion* pRegions) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdCopyTypedBuffer(
        const IGpuMemory&            srcGpuMemory,
        const IGpuMemory&            dstGpuMemory,
        uint32                       regionCount,
        const TypedBufferCopyRegion* pRegions) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdScaledCopyTypedBufferToImage(
        const IGpuMemory&                       srcGpuMemory,
        const IImage&                           dstImage,
        ImageLayout                             dstImageLayout,
        uint32                                  regionCount,
        const TypedBufferImageScaledCopyRegion* pRegions) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdCopyRegisterToMemory(
        uint32            srcRegisterOffset,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdScaledCopyImage(
        const ScaledCopyInfo& copyInfo) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdGenerateMipmaps(
        const GenMipmapsInfo& genInfo) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdColorSpaceConversionCopy(
        const IImage&                     srcImage,
        ImageLayout                       srcImageLayout,
        const IImage&                     dstImage,
        ImageLayout                       dstImageLayout,
        uint32                            regionCount,
        const ColorSpaceConversionRegion* pRegions,
        TexFilter                         filter,
        const ColorSpaceConversionTable&  cscTable) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdCloneImageData(
        const IImage& srcImage,
        const IImage& dstImage) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdUpdateMemory(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           dataSize,
        const uint32*     pData) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdUpdateBusAddressableMemoryMarker(
        const IGpuMemory& dstGpuMemory,
        gpusize           offset,
        uint32            value) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdFillMemory(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           dataSize,
        uint32            data) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdClearColorBuffer(
        const IGpuMemory& gpuMemory,
        const ClearColor& color,
        SwizzledFormat    bufferFormat,
        uint32            bufferOffset,
        uint32            bufferExtent,
        uint32            rangeCount = 0,
        const Range*      pRanges    = nullptr) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdClearBoundColorTargets(
        uint32                          colorTargetCount,
        const BoundColorTarget*         pBoundColorTargets,
        uint32                          regionCount,
        const ClearBoundTargetRegion*   pClearRegions) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdClearColorImage(
        const IImage&         image,
        ImageLayout           imageLayout,
        const ClearColor&     color,
        const SwizzledFormat& clearFormat,
        uint32                rangeCount,
        const SubresRange*    pRanges,
        uint32                boxCount,
        const Box*            pBoxes,
        uint32                flags) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdClearBoundDepthStencilTargets(
        float                         depth,
        uint8                         stencil,
        uint8                         stencilWriteMask,
        uint32                        samples,
        uint32                        fragments,
        DepthStencilSelectFlags       flag,
        uint32                        regionCount,
        const ClearBoundTargetRegion* pClearRegions) override
        { PAL_NEVER_CALLED(); }

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
        uint32             flags) override
        { PAL_NEVER_CALLED(); }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 910
    virtual void CmdClearBufferView(
        const IGpuMemory& gpuMemory,
        const ClearColor& color,
        const void*       pBufferViewSrd,
        uint32            rangeCount = 0,
        const Range*      pRanges    = nullptr) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdClearImageView(
        const IImage&     image,
        ImageLayout       imageLayout,
        const ClearColor& color,
        const void*       pImageViewSrd,
        uint32            rectCount = 0,
        const Rect*       pRects    = nullptr) override
        { PAL_NEVER_CALLED(); }
#endif

    virtual void CmdResolveImage(
        const IImage&             srcImage,
        ImageLayout               srcImageLayout,
        const IImage&             dstImage,
        ImageLayout               dstImageLayout,
        ResolveMode               resolveMode,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        uint32                    flags) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdResolvePrtPlusImage(
        const IImage&                    srcImage,
        ImageLayout                      srcImageLayout,
        const IImage&                    dstImage,
        ImageLayout                      dstImageLayout,
        PrtPlusResolveType               resolveType,
        uint32                           regionCount,
        const PrtPlusImageResolveRegion* pRegions) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetEvent(const IGpuEvent& gpuEvent, uint32 stageMask) override
        { WriteEvent(gpuEvent, stageMask, GpuEvent::SetValue); }

    virtual void CmdResetEvent(const IGpuEvent& gpuEvent, uint32 stageMask) override
        { WriteEvent(gpuEvent, stageMask, GpuEvent::ResetValue); }

    virtual void CmdPredicateEvent(const IGpuEvent& gpuEvent) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdMemoryAtomic(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        uint64            srcData,
        AtomicOp          atomicOp) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdBeginQuery(
        const IQueryPool& queryPool,
        QueryType         queryType,
        uint32            slot,
        QueryControlFlags flags) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdEndQuery(
        const IQueryPool& queryPool,
        QueryType         queryType,
        uint32            slot) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdResolveQuery(
        const IQueryPool& queryPool,
        QueryResultFlags  flags,
        QueryType         queryType,
        uint32            startQuery,
        uint32            queryCount,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           dstStride) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdResetQueryPool(
        const IQueryPool& queryPool,
        uint32            startQuery,
        uint32            queryCount) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdWriteTimestamp(
        uint32            stageMask,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdWriteImmediate(
        uint32             stageMask,
        uint64             data,
        ImmediateDataWidth dataSize,
        gpusize            address) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdLoadBufferFilledSizes(
        const gpusize (&gpuVirtAddr)[MaxStreamOutTargets]) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSaveBufferFilledSizes(
        const gpusize (&gpuVirtAddr)[MaxStreamOutTargets]) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetBufferFilledSize(
        uint32  bufferId,
        uint32  offset) override
     { PAL_NEVER_CALLED(); }

    virtual void CmdBindBorderColorPalette(
        PipelineBindPoint          pipelineBindPoint,
        const IBorderColorPalette* pPalette) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetPredication(
        IQueryPool*         pQueryPool,
        uint32              slot,
        const IGpuMemory*   pGpuMemory,
        gpusize             offset,
        PredicateType       predType,
        bool                predPolarity,
        bool                waitResults,
        bool                accumulateData) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSuspendPredication(
        bool suspend) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdIf(
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint64            data,
        uint64            mask,
        CompareFunc       compareFunc) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdElse() override { PAL_NEVER_CALLED(); }

    virtual void CmdEndIf() override { PAL_NEVER_CALLED(); }

    virtual void CmdWhile(
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint64            data,
        uint64            mask,
        CompareFunc       compareFunc) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdEndWhile() override { PAL_NEVER_CALLED(); }

    virtual void CmdWaitRegisterValue(
        uint32      registerOffset,
        uint32      data,
        uint32      mask,
        CompareFunc compareFunc) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdWaitMemoryValue(
        gpusize           gpuVirtAddr,
        uint32            data,
        uint32            mask,
        CompareFunc       compareFunc) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdWaitBusAddressableMemoryMarker(
        const IGpuMemory& gpuMemory,
        uint32            data,
        uint32            mask,
        CompareFunc       compareFunc) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdBeginPerfExperiment(IPerfExperiment* pPerfExperiment) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdUpdatePerfExperimentSqttTokenMask(
        IPerfExperiment*              pPerfExperiment,
        const ThreadTraceTokenConfig& sqttTokenConfig) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdUpdateSqttTokenMask(
        const ThreadTraceTokenConfig& sqttTokenConfig) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdEndPerfExperiment(IPerfExperiment* pPerfExperiment) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdInsertTraceMarker(PerfTraceMarkerType markerType, uint32 markerData) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdInsertRgpTraceMarker(
        RgpMarkerSubQueueFlags subQueueFlags,
        uint32                 numDwords,
        const void*            pData) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdCopyDfSpmTraceData(
        const IPerfExperiment& perfExperiment,
        const IGpuMemory&      dstGpuMemory,
        gpusize                dstOffset) override;

    virtual uint32* CmdAllocateEmbeddedData(
        uint32   sizeInDwords,
        uint32   alignmentInDwords,
        gpusize* pGpuAddress) override final;

    virtual uint32* CmdAllocateLargeEmbeddedData(
        uint32   sizeInDwords,
        uint32   alignmentInDwords,
        gpusize* pGpuAddress) override final;

    virtual Result AllocateAndBindGpuMemToEvent(
        IGpuEvent* pGpuEvent) override;

    virtual void CmdPrimeGpuCaches(
        uint32                    rangeCount,
        const PrimeGpuCacheRange* pRanges) override { PAL_NEVER_CALLED(); }

    virtual void CmdExecuteNestedCmdBuffers(
        uint32            cmdBufferCount,
        ICmdBuffer*const* ppCmdBuffers) override { PAL_NEVER_CALLED(); }

    void TrackIb2DumpInfoFromExecuteNestedCmds(const Pal::CmdStream& targetStream);

    virtual void CmdSaveComputeState(
        uint32 stateFlags) override { PAL_NEVER_CALLED(); }

    // For PAL internal: should call CmdRestoreGraphicsStateInternal() instead of CmdRestoreComputeState(); otherwise
    //                   there may be potential issue due to miss tracking blt active flags for barriers.
    virtual void CmdRestoreComputeStateInternal(
        uint32 stateFlags,
        bool   trackBltActiveFlags = true) { PAL_NEVER_CALLED(); }

    virtual void CmdRestoreComputeState(
        uint32 stateFlags) override { CmdRestoreComputeStateInternal(stateFlags, false); }

    virtual void CmdExecuteIndirectCmds(
        const IIndirectCmdGenerator& generator,
        gpusize                      gpuMemVirtAddr,
        uint32                       maximumCount,
        gpusize                      countGpuAddr) override { PAL_NEVER_CALLED(); }

    virtual void CmdPostProcessFrame(
        const CmdPostProcessFrameInfo& postProcessInfo,
        bool*                          pAddedGpuWork) override
    {
        if (pAddedGpuWork != nullptr)
        {
            *pAddedGpuWork = false;
        }
    }

    virtual void CmdSetUserClipPlanes(
        uint32               firstPlane,
        uint32               planeCount,
        const UserClipPlane* pPlanes) override { PAL_NEVER_CALLED(); }

    virtual void CmdSetClipRects(
        uint16      clipRule,
        uint32      rectCount,
        const Rect* pRectList) override { PAL_NEVER_CALLED(); }

    virtual void CmdStartGpuProfilerLogging() override  { PAL_NEVER_CALLED(); }

    virtual void CmdStopGpuProfilerLogging() override  { PAL_NEVER_CALLED(); }

    virtual void CmdSetViewInstanceMask(uint32 mask) override { PAL_NEVER_CALLED(); }

    virtual void CmdCommentString(
        const char* pComment) override { PAL_NEVER_CALLED(); }

    virtual void CmdUpdateHiSPretests(
       const IImage*      pImage,
       const HiSPretests& pretests,
       uint32             firstMip,
       uint32             numMips) override { PAL_NEVER_CALLED(); }

    virtual uint32 GetUsedSize(CmdAllocType type) const override;

    // Maximum length of a filename allowed for command buffer dumps
    static constexpr uint32 MaxFilenameLength = 32;

    // Pre-processes the command buffer before submission (potentially generating the commands at submit time).
    virtual Result PreSubmit() { return Result::Success; }

    // Increments the submit-count of the command stream(s) contained in this command buffer.
    virtual void IncrementSubmitCount() = 0;

    // This function allows us end all CmdStream provided and dump them into a file.
    virtual void EndCmdBufferDump(const CmdStream** ppCmdStreams, uint32 cmdStreamsNum);

    // This function allows us to dump the contents of this command buffer to a file at submission time.
    virtual void DumpCmdStreamsToFile(Util::File* pFile, CmdBufDumpFormat mode) const = 0;

    // This function gets the directory from device settings and dump the file to the right directory.
    void OpenCmdBufDumpFile(const char* pFilename);

    void GetCmdBufDumpFilename(char* pOutput, size_t outputBufSize) const;

    // Dumps the Ib2s to a file.  Should be called after the rest of the dumping is done.
    // Even though not all command buffers can have IB2s, dumping from Pal::Queue is much more straightforward if the
    // dumpInfo is stored in all types of command buffers, and just empty in ones that don't use it.
    void DumpIb2s(Util::File* pFil, CmdBufDumpFormat mode);
    Ib2DumpInfoVec* GetIb2DumpInfos() { return &m_ib2DumpInfos; }

    // Inserts Ib2 to the vector, but only if there isn't an ib2 with the same gpuVA already
    void InsertIb2DumpInfo(const Ib2DumpInfo& dumpInfo);

    // Returns the number of command streams associated with this command buffer.
    virtual uint32 NumCmdStreams() const { return 1; }

    // Returns a pointer to the command stream specified by "cmdStreamIdx".
    virtual const CmdStream* GetCmdStream(uint32 cmdStreamIdx) const = 0;

    // Special sub-queue index representing the "main" sub-queue.
    static constexpr int32 MainSubQueueIdx = -1;

    // Returns a pointer to the command stream specified by the given ganged sub-queue index and command stream
    // index.  A sub-queue index of MainSubQueueIdx indicates the "main" sub-queue.
    virtual const CmdStream* GetCmdStreamInSubQueue(int32 subQueueIndex) const
        { PAL_ASSERT(subQueueIndex == MainSubQueueIdx); return GetCmdStream(0); }

    CmdBufferRecordState RecordState() const { return m_recordState; }

    const Device&   GetDevice()         const { return m_device; }
    QueueType       GetQueueType()      const { return m_createInfo.queueType; }
    QueuePriority   GetQueuePriority()  const { return m_createInfo.queuePriority; }
    EngineType      GetEngineType()     const { return m_engineType; }

    bool IsNested()               const { return (m_createInfo.flags.nested               != 0); }
    bool IsRealtimeComputeUnits() const { return (m_createInfo.flags.realtimeComputeUnits != 0); }
    bool UsesDispatchTunneling()  const { return (m_createInfo.flags.dispatchTunneling    != 0); }
#if PAL_BUILD_GFX12
    bool DispatchPongPongWalk()   const { return (m_createInfo.flags.dispatchPingPongWalk != 0); }
#endif

    bool IsExclusiveSubmit() const { return (m_buildFlags.optimizeExclusiveSubmit    != 0); }
    bool IsOneTimeSubmit()   const { return (m_buildFlags.optimizeOneTimeSubmit      != 0); }
    bool AllowLaunchViaIb2() const { return (m_buildFlags.disallowNestedLaunchViaIb2 == 0); }

    bool IsTmzEnabled() const { return (m_buildFlags.enableTmz != 0); }

    uint64 LastPagingFence() const { return m_lastPagingFence; }
    void UpdateLastPagingFence(uint64 pagingFence)
        { m_lastPagingFence = Util::Max(pagingFence, m_lastPagingFence); }

    // Note that this is not a general-purpose allocator. It is only valid during command building and its allocations
    // must follow special life-time rules. Read the CmdBufferBuildInfo documentation for more information.
    Util::VirtualLinearAllocator* Allocator() { return m_pMemAllocator; }

    // Command building error management:
    void NotifyAllocFailure();
    void SetCmdRecordingError(Result error);

    bool HasAddressDependentCmdStream() const;

    gpusize AllocateGpuScratchMem(
        uint32 sizeInDwords,
        uint32 alignmentInDwords)
    {
        gpusize    offset  = 0;
        GpuMemory* pGpuMem = nullptr;
        return AllocateGpuScratchMem(sizeInDwords, alignmentInDwords, &pGpuMem, &offset);
    }

    // Allocated embedded data for physical engines. The function returns the memory object and offset.
    uint32* CmdAllocateEmbeddedData(
        uint32      sizeInDwords,
        uint32      alignmentInDwords,
        GpuMemory** ppGpuMem,
        gpusize*    pOffset);

    uint32* CmdAllocateLargeEmbeddedData(
        uint32      sizeInDwords,
        uint32      alignmentInDwords,
        GpuMemory** ppGpuMem,
        gpusize*    pOffset);

    virtual void CmdSetPerDrawVrsRate(
        const VrsRateParams&  rateParams) override;

    virtual void CmdSetVrsCenterState(
        const VrsCenterState&  centerState) override;

    virtual void CmdBindSampleRateImage(
        const IImage*  pImage) override;

    // True if a Hybrid pipeline was bound to this command buffer or if any of the task/mesh draw functions were
    // invoked.
    bool HasHybridPipeline() const { return (m_flags.hasHybridPipeline == 1); }
    void ReportHybridPipelineBind() { m_flags.hasHybridPipeline = 1; }
#if PAL_BUILD_RDF
    bool IsUsedInEndTrace() const { return (m_flags.usedInEndTrace == 1); }
    void SetEndTraceFlag(uint32 value) { m_flags.usedInEndTrace = value; }
#else
    bool IsUsedInEndTrace() const { return false; }
    void SetEndTraceFlag(uint32 value) {}
#endif

    uint32 ImplicitGangedSubQueueCount() const { return m_implicitGangSubQueueCount; }
    void EnableImplicitGangedSubQueueCount(uint32 count)
        { if (count > m_implicitGangSubQueueCount) { m_implicitGangSubQueueCount = count; } }

    // Get the cmd allocator currently associated with this cmd buffer
    CmdAllocator* GetCmdAllocator() { return m_pCmdAllocator; }
    const CmdAllocator* GetCmdAllocator() const { return m_pCmdAllocator; }

    // Checks if command buffer can be preempted.
    // Default state is permissive and assumes most command buffers contain work that is indifferent to preemption
    virtual bool IsPreemptable() const { return true; }

protected:
    CmdBuffer(const Device&              device,
              const CmdBufferCreateInfo& createInfo);
    virtual ~CmdBuffer();

    // Responsible for adding all the commands needed by the preamble
    virtual void AddPreamble() = 0;

    // Default implementation for command streams that don't require a postamble.  Over-ride implementations are free
    // to add as much data as needed to their respective command streams.
    virtual void AddPostamble() {}

    // CmdStream::Begin needs to call Begin on all command streams part-way through. Note that m_pMemAllocator will be
    // valid when this is called so it can be used to allocate temporary state objects.
    virtual Result BeginCommandStreams(CmdStreamBeginFlags cmdStreamFlags, bool doReset);

    // Resets and initializes all internal state that this command buffer uses to build commands. This must not interact
    // with the command allocator and is intended to be called during Begin().
    virtual void ResetState()
    {
        // This hasHybridPipeline flag needs to be reset otherwise reuse of this cmdBuffer would cause issue on
        // gang-submit related tests.
        m_flags.hasHybridPipeline = 0;
    }

    virtual void WriteEventCmd(const BoundGpuMemory& boundMemObj, uint32 stageMask, uint32 data) = 0;

    // Helper function for switching the CmdSetUserData callback for a specific pipeline type.
    void SwitchCmdSetUserDataFunc(
        PipelineBindPoint  bindPoint,
        CmdSetUserDataFunc pfnCallback)
        { m_funcTable.pfnCmdSetUserData[static_cast<uint32>(bindPoint)] = pfnCallback; }

    // Get a chunk for embedded data.
    CmdStreamChunk* GetEmbeddedDataChunk(
        uint32 numDwords)
        { return GetDataChunk(EmbeddedDataAlloc, &m_embeddedData, numDwords); }

    // Get a chunk for embedded data. A large one, that is.
    CmdStreamChunk* GetLargeEmbeddedDataChunk(
        uint32 numDwords)
        { return GetDataChunk(LargeEmbeddedDataAlloc, &m_largeEmbeddedData, numDwords); }

    gpusize AllocateGpuScratchMem(
        uint32      sizeInDwords,
        uint32      alignmentInDwords,
        GpuMemory** ppGpuMem,
        gpusize*    pOffset);

    // Inserts the specified number of dwords of NOPs into the "main" command stream of the command buffer (DE for
    // universal command buffers).
    virtual uint32* WriteNops(uint32* pCmdSpace, uint32 numDwords) const
    {
        PAL_NEVER_CALLED();
        return nullptr;
    }

    static void PAL_STDCALL CmdDispatchInvalid(
        ICmdBuffer*       pCmdBuffer,
        DispatchDims      size,
        DispatchInfoFlags infoFlags);
    static void PAL_STDCALL CmdDispatchIndirectInvalid(
        ICmdBuffer* pCmdBuffer,
        gpusize     gpuVirtAddr);
    static void PAL_STDCALL CmdDispatchOffsetInvalid(
        ICmdBuffer*  pCmdBuffer,
        DispatchDims offset,
        DispatchDims launchSize,
        DispatchDims logicalSize);

    static void PAL_STDCALL CmdDispatchMeshInvalid(
        ICmdBuffer*  pCmdBuffer,
        DispatchDims size);
    static void PAL_STDCALL CmdDispatchMeshIndirectMultiInvalid(
        ICmdBuffer*          pCmdBuffer,
        GpuVirtAddrAndStride gpuVirtAddrAndStride,
        uint32               maximumCount,
        gpusize              countGpuAddr);

    // Utility function for determing if command buffer dumping has been enabled.
    bool IsDumpingEnabled() const { return m_device.Settings().cmdBufDumpMode == CmdBufDumpModeRecordTime; }

    Util::File* DumpFile() { return &m_file; }
    virtual uint32 UniqueId() const override { return m_uniqueId; }
    uint32      NumBegun() const { return m_numCmdBufsBegun; }

    virtual uint32 CmdInsertExecutionMarker(
        bool         isBegin,
        uint8        sourceId,
        const char*  pMarkerName,
        uint32       markerNameSize) override { return 0; }

    const CmdBufferCreateInfo     m_createInfo;
    CmdBufferInternalCreateInfo   m_internalInfo;
    CmdBufferBuildFlags           m_buildFlags;
    const EngineType              m_engineType;

    CmdAllocator*                 m_pCmdAllocator;

    Util::VirtualLinearAllocator* m_pMemAllocator;
    void*                         m_pMemAllocatorStartPos;
    Result                        m_status; // Remembers if we encountered an error while recording commands.

    gpusize                       m_executionMarkerAddr;
    uint32                        m_executionMarkerCount;

    struct ChunkData
    {
        ChunkData(Platform* pAllocator) : chunkList(pAllocator), retainedChunks(pAllocator), chunkDwordsAvailable(0)
        { }

        ChunkRefList chunkList;            // List of allocated data chunks.
        ChunkRefList retainedChunks;       // List of data chunks that have been retained between resets
        uint32       chunkDwordsAvailable; // Number of unused DWORD's in the tail of the chunk list.
    };

    ChunkData          m_embeddedData;
    ChunkData          m_largeEmbeddedData;
    ChunkData          m_gpuScratchMem;
    uint32             m_gpuScratchMemAllocLimit;

    // Latest GPU memory paging fence seen across this command buffer and all nested command buffers called by this
    // command buffer.
    uint64  m_lastPagingFence;

    // Some flags to track internal command buffer state.
    union
    {
        struct
        {
            uint32 internalMemAllocator : 1;  // True if m_pMemAllocator is owned internally by PAL.
            uint32 hasHybridPipeline    : 1;  // True if this command buffer has a hybrid pipeline bound.
            uint32 autoMemoryReuse      : 1;  // True if the command buffer uses autoMemoryReuse.
#if PAL_BUILD_RDF
            uint32 usedInEndTrace       : 1;  // True if this is a cmdBuffer used during ending a PAL trace. Clients
                                              // might submit their own GPU work as part of this cmdBuffer
#else
            uint32 placeholder3         : 1;
#endif
            uint32 reserved             : 28;
        };

        uint32     u32All;
    } m_flags;

    // Number of implicit ganged sub-queues.
    uint32 m_implicitGangSubQueueCount;

    Ib2DumpInfoVec m_ib2DumpInfos; // Vector holding information needed to dump IB2s

private:
    CmdStreamChunk* GetNextDataChunk(
        CmdAllocType type,
        ChunkData*   pData,
        uint32       numDwords);

    CmdStreamChunk* GetDataChunk(
        CmdAllocType type,
        ChunkData*   pData,
        uint32       numDwords);

    void WriteEvent(const IGpuEvent& gpuEvent, uint32 stageMask, uint32 data);

    void ReturnDataChunks(ChunkData* pData, CmdAllocType type, bool returnGpuMemory);
    void ReturnLinearAllocator();

    void VerifyBarrierTransitions(const AcquireReleaseInfo& releaseInfo) const;

    const Device&          m_device;
    CmdBufferRecordState   m_recordState;

    // These member variables are only for command buffer dumping support.
    static uint32  s_numCreated[QueueTypeCount]; // Number of created CmdBuffers of each type.
    Util::File     m_file;
    uint32         m_uniqueId;
    uint32         m_numCmdBufsBegun;

    PAL_DISALLOW_COPY_AND_ASSIGN(CmdBuffer);
    PAL_DISALLOW_DEFAULT_CTOR(CmdBuffer);
};

} // Pal
