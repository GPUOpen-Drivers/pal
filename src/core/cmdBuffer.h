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
            uint32 isInternal :  1;     // Whether this command buffer is created on an internal, hidden queue
            uint32 reserved   : 31;
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
//          * The sub-engine ID. This is used when a CE and DE combined command buffer is sent to the
//            universal queue, for example. The meaning of the sub-engine ID is specific to the engine
//            index and ASIC family.

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
    uint32      subEngineId;        // Sub-engine. (0 = DE, 1 = CE)
};

// Structure defining header for an IB2 buffer.
struct CmdBufferIb2DumpHeader
{
    uint32      size;               // Size of this structure in bytes.
    uint32      cmdBufferSize;      // Size of the command buffer in bytes.
    uint32      subEngineId;        // Sub-engine. (0 = DE, 1 = CE)
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
const uint32 GetSubEngineId(
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

// Differentiates between once-per-copy and once-per-chunk data in P2pBltWaInfo.
enum class P2pBltWaInfoType : uint32
{
    PerCopy  = 0,
    PerChunk = 1,
};

// Used to record information needed by the OS layers to implement a workaround for peer to peer copies required by
// some hardware.  The workaround requires splitting any P2P copies into small chunks, which unfortunately requires
// some parts of the workaround to be implemented in hardware independent portions of PAL.
//
// When building command buffers with P2P BLTs, a list of these structures will be built.  Each copy will cause one
// "PerCopy" entry to be inserted followed by one or more "PerChunk" entries.
struct P2pBltWaInfo
{
    P2pBltWaInfoType type;                // PerCopy/PerChunk - chooses which part of the union is valid.
    union
    {
        struct
        {
            const GpuMemory* pDstMemory;  // Destination of the copy.
            uint32           numChunks;   // Number of "PerChunk" entries this copy needs.  Each "chunk" refers to a
                                          // specific VA range that commands will write, there may be more than one BLT
                                          // command in a single chunk that all target the same small chunk of VA space.
        } perCopy;

        struct
        {
            gpusize cmdBufPatchGpuVa;     // GPU VA pointing into the command buffer memory where PAL has written NOPs
                                          // so that KMD can patch in command to modify the PCI BAR.
            gpusize startAddr;            // Starting VA of this chunk.
        } perChunk;
    };
};

// Convenience typedef for a vector of P2P BLT workaround structures.
typedef Util::Vector<P2pBltWaInfo, 1, Platform> P2pBltWaInfoVector;

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

    virtual uint32 GetEmbeddedDataLimit() const override
        { return m_pCmdAllocator->ChunkSize(EmbeddedDataAlloc) / sizeof(uint32); }

    virtual void CmdBarrier(const BarrierInfo& barrierInfo) override;

    virtual void OptimizeBarrierReleaseInfo(
        uint32       pipePointCount,
        HwPipePoint* pPipePoints,
        uint32*      pCacheMask) const override { }

    virtual void OptimizeAcqRelReleaseInfo(uint32* pStageMask, uint32* pAccessMask) const override { }

    virtual uint32 CmdRelease(const AcquireReleaseInfo& releaseInfo) override;

    virtual void CmdAcquire(
        const AcquireReleaseInfo& acquireInfo,
        uint32                    syncTokenCount,
        const uint32*             pSyncTokens) override;

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
        {PAL_NEVER_CALLED();}

    virtual void CmdRestoreGraphicsState() override
        {PAL_NEVER_CALLED();}

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

    virtual void CmdSetVertexBuffers(
        uint32                firstBuffer,
        uint32                bufferCount,
        const BufferViewInfo* pBuffers) override
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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 778
    virtual void CmdSetColorWriteMask(
        const ColorWriteMaskParams& params) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdSetRasterizerDiscardEnable(
        bool rasterizerDiscardEnable) override
        { PAL_NEVER_CALLED(); }
#endif

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

    virtual void CmdSetEvent(const IGpuEvent& gpuEvent, HwPipePoint setPoint) override
        { WriteEvent(gpuEvent, setPoint, GpuEvent::SetValue); }

    virtual void CmdResetEvent(const IGpuEvent& gpuEvent, HwPipePoint resetPoint) override
        { WriteEvent(gpuEvent, resetPoint, GpuEvent::ResetValue); }

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
        HwPipePoint       pipePoint,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset) override
        { PAL_NEVER_CALLED(); }

    virtual void CmdWriteImmediate(
        HwPipePoint        pipePoint,
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
        const IGpuMemory& gpuMemory,
        gpusize           offset,
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

    virtual void CmdLoadCeRam(
        const IGpuMemory& srcGpuMemory,
        gpusize           memOffset,
        uint32            ramOffset,
        uint32            dwordSize) override { PAL_NEVER_CALLED(); }

    virtual void CmdDumpCeRam(
        const IGpuMemory& dstGpuMemory,
        gpusize           memOffset,
        uint32            ramOffset,
        uint32            dwordSize,
        uint32            currRingPos,
        uint32            ringSize) override { PAL_NEVER_CALLED(); }

    virtual void CmdWriteCeRam(
        const void* pSrcData,
        uint32      ramOffset,
        uint32      dwordSize) override { PAL_NEVER_CALLED(); }

    virtual uint32* CmdAllocateEmbeddedData(
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

    virtual void CmdRestoreComputeState(
        uint32 stateFlags) override { PAL_NEVER_CALLED(); }

    virtual void CmdExecuteIndirectCmds(
        const IIndirectCmdGenerator& generator,
        const IGpuMemory&            gpuMemory,
        gpusize                      offset,
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

    virtual void CmdXdmaWaitFlipPending() override { PAL_NEVER_CALLED(); }

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
    virtual uint32 NumCmdStreams() const = 0;

    // Returns a pointer to the command stream specified by "cmdStreamIdx".
    virtual const CmdStream* GetCmdStream(uint32 cmdStreamIdx) const = 0;

    // Special sub-queue index representing the "main" sub-queue.
    static constexpr int32 MainSubQueueIdx = -1;

    // Returns the number of command streams associated with this command buffer, for the specified ganged
    // sub-queue index.  An index of MainSubQueueIdx indicates the "main" sub-queue.
    virtual uint32 NumCmdStreamsInSubQueue(int32 subQueueIndex) const
        { PAL_ASSERT(subQueueIndex == MainSubQueueIdx); return NumCmdStreams(); }

    // Returns a pointer to the command stream specified by the given ganged sub-queue index and command stream
    // index.  A sub-queue index of MainSubQueueIdx indicates the "main" sub-queue.
    virtual const CmdStream* GetCmdStreamInSubQueue(int32 subQueueIndex, uint32 cmdStreamIndex) const
        { PAL_ASSERT(subQueueIndex == MainSubQueueIdx); return GetCmdStream(cmdStreamIndex); }

    CmdBufferRecordState RecordState() const { return m_recordState; }

    QueueType       GetQueueType()      const { return m_createInfo.queueType; }
    QueuePriority   GetQueuePriority()  const { return m_createInfo.queuePriority; }
    EngineType      GetEngineType()     const { return m_engineType; }

    bool IsNested()               const { return (m_createInfo.flags.nested               != 0); }
    bool IsRealtimeComputeUnits() const { return (m_createInfo.flags.realtimeComputeUnits != 0); }
    bool UsesDispatchTunneling()  const { return (m_createInfo.flags.dispatchTunneling    != 0); }

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

    // Called once before initiating a copy that will target a peer memory object where the P2P BLT BAR workaround
    // is required.  It should not be called if the workaround is not requied.
    virtual void P2pBltWaCopyBegin(const GpuMemory* pDstMemory, uint32 regionCount, const gpusize* pChunkAddrs);

    // Having called P2pBltWaCopyBegin(); this function should called before each individual chunk.
    virtual void P2pBltWaCopyNextRegion(gpusize chunkAddr) { PAL_NEVER_CALLED(); }

    // Bookend to P2pBltWaCopyEnd(); should be called once all chunk BLTs have been inserted.
    virtual void P2pBltWaCopyEnd() { }

    const P2pBltWaInfoVector& GetP2pBltWaInfoVec() const { return m_p2pBltWaInfo; }

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
    bool IsUsedInEndTrace() const { return (m_flags.usedInEndTrace == 1); }
    void SetEndTraceFlag(uint32 value) { m_flags.usedInEndTrace = value; }

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
    virtual Result AddPreamble() = 0;

    // Default implementation for command streams that don't require a postamble.  Over-ride implementations are free
    // to add as much data as needed to their respective command streams.
    virtual Result AddPostamble() { return Result::_Success; }

    // CmdStream::Begin needs to call Begin on all command streams part-way through. Note that m_pMemAllocator will be
    // valid when this is called so it can be used to allocate temporary state objects.
    virtual Result BeginCommandStreams(CmdStreamBeginFlags cmdStreamFlags, bool doReset);

    // Resets and initializes all internal state that this command buffer uses to build commands. This must not interact
    // with the command allocator and is intended to be called during Begin().
    virtual void ResetState() { }

    virtual void WriteEventCmd(const BoundGpuMemory& boundMemObj, HwPipePoint pipePoint, uint32 data) = 0;

    // Helper function for switching the CmdSetUserData callback for a specific pipeline type.
    void SwitchCmdSetUserDataFunc(
        PipelineBindPoint  bindPoint,
        CmdSetUserDataFunc pfnCallback)
        { m_funcTable.pfnCmdSetUserData[static_cast<uint32>(bindPoint)] = pfnCallback; }

    // Get a chunk for embedded data.
    CmdStreamChunk* GetEmbeddedDataChunk(
        uint32 numDwords)
        { return GetDataChunk(EmbeddedDataAlloc, &m_embeddedData, numDwords); }

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

    // Helper called by public P2pBltWaCopyNextRegion that does the heavy lifting once the derived class provides the
    // appropriate command stream.
    void P2pBltWaCopyNextRegion(CmdStream* pCmdStream, gpusize chunkAddr);

    static void PAL_STDCALL CmdDispatchInvalid(
        ICmdBuffer*  pCmdBuffer,
        DispatchDims size);
    static void PAL_STDCALL CmdDispatchIndirectInvalid(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset);
    static void PAL_STDCALL CmdDispatchOffsetInvalid(
        ICmdBuffer*  pCmdBuffer,
        DispatchDims offset,
        DispatchDims launchSize,
        DispatchDims logicalSize);
    static void PAL_STDCALL CmdDispatchDynamicInvalid(
        ICmdBuffer*  pCmdBuffer,
        gpusize      gpuVa,
        DispatchDims size);

    static void PAL_STDCALL CmdDispatchMeshInvalid(
        ICmdBuffer*  pCmdBuffer,
        DispatchDims size);
    static void PAL_STDCALL CmdDispatchMeshIndirectMultiInvalid(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);

    // Utility function for determing if command buffer dumping has been enabled.
    bool IsDumpingEnabled() const { return m_device.Settings().cmdBufDumpMode == CmdBufDumpModeRecordTime; }

    Util::File* DumpFile() { return &m_file; }
    uint32      UniqueId() const { return m_uniqueId; }
    uint32      NumBegun() const { return m_numCmdBufsBegun; }

    virtual void CmdNop(
        const void* pPayload,
        uint32      payloadSize) override
        { PAL_NEVER_CALLED(); }

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
    ChunkData          m_gpuScratchMem;
    uint32             m_gpuScratchMemAllocLimit;

    // Latest GPU memory paging fence seen across this command buffer and all nested command buffers called by this
    // command buffer.
    uint64  m_lastPagingFence;

    P2pBltWaInfoVector m_p2pBltWaInfo;           // List of P2P BLT info that is required by the KMD-assisted PCI BAR
                                                 // workaround.
    gpusize            m_p2pBltWaLastChunkAddr;  // Scratch variable to avoid starting a new chunk if the starting
                                                 // address of a chunk matches the last chunk.

    // Some flags to track internal command buffer state.
    union
    {
        struct
        {
            uint32 internalMemAllocator     : 1;  // True if m_pMemAllocator is owned internally by PAL.
            uint32 hasHybridPipeline        : 1;  // True if this command buffer has a hybrid pipeline bound.
            uint32 autoMemoryReuse          : 1;  // True if the command buffer uses autoMemoryReuse.
#if PAL_BUILD_RDF
            uint32 usedInEndTrace           : 1;  // True if this is a cmdBuffer used during ending a PAL trace. Clients
                                                  // might submit their own GPU work as part of this cmdBuffer
#else
            uint32 placeholder3             : 1;
#endif
            uint32 reserved                 : 28;
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
    void WriteEvent(const IGpuEvent& gpuEvent, HwPipePoint pipePoint, uint32 data);

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
