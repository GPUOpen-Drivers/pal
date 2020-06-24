/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_BUILD_PM4_INSTRUMENTOR

#include "palCmdBuffer.h"
#include "palDeveloperHooks.h"
#include "core/layers/pm4Instrumentor/pm4InstrumentorQueue.h"

namespace Pal
{
namespace Pm4Instrumentor
{

class Device;

// =====================================================================================================================
// Pm4Instrumentor layer implementation of ICmdBuffer.  Accumulates statistics while recording commands.
class CmdBuffer : public CmdBufferFwdDecorator
{
public:
    CmdBuffer(ICmdBuffer*                pNextCmdBuffer,
              Device*                    pDevice,
              const CmdBufferCreateInfo& createInfo);

    virtual Result Begin(const CmdBufferBuildInfo& buildInfo) override;
    virtual Result Reset(ICmdAllocator* pCmdAllocator, bool returnGpuMemory) override;
    virtual Result End() override;

    virtual void CmdBindPipeline(
        const PipelineBindParams& params) override;

    virtual void CmdBindMsaaState(
        const IMsaaState* pMsaaState) override;
    virtual void CmdBindColorBlendState(
        const IColorBlendState* pColorBlendState) override;
    virtual void CmdBindDepthStencilState(
        const IDepthStencilState* pDepthStencilState) override;

    virtual void CmdBindIndexData(
        gpusize   gpuAddr,
        uint32    indexCount,
        IndexType indexType) override;
    virtual void CmdSetVertexBuffers(
        uint32                firstBuffer,
        uint32                bufferCount,
        const BufferViewInfo* pBuffers) override;

    virtual void CmdBindTargets(
        const BindTargetParams& params) override;
    virtual void CmdBindStreamOutTargets(
        const BindStreamOutTargetParams& params) override;

    virtual void CmdSetBlendConst(
        const BlendConstParams& params) override;
    virtual void CmdSetInputAssemblyState(
        const InputAssemblyStateParams& params) override;
    virtual void CmdSetTriangleRasterState(
        const TriangleRasterStateParams& params) override;
    virtual void CmdSetPointLineRasterState(
        const PointLineRasterStateParams& params) override;
    virtual void CmdSetLineStippleState(
        const LineStippleStateParams& params) override;
    virtual void CmdSetDepthBiasState(
        const DepthBiasParams& params) override;
    virtual void CmdSetDepthBounds(
        const DepthBoundsParams& params) override;
    virtual void CmdSetStencilRefMasks(
        const StencilRefMaskParams& params) override;
    virtual void CmdSetMsaaQuadSamplePattern(
        uint32                       numSamplesPerPixel,
        const MsaaQuadSamplePattern& quadSamplePattern) override;

    virtual void CmdSetViewports(
        const ViewportParams& params) override;
    virtual void CmdSetScissorRects(
        const ScissorRectParams& params) override;
    virtual void CmdSetGlobalScissor(
        const GlobalScissorParams& params) override;

    virtual void CmdBarrier(const BarrierInfo& barrierInfo) override;
    virtual void CmdRelease(
        const AcquireReleaseInfo& releaseInfo,
        const IGpuEvent*          pGpuEvent) override;
    virtual void CmdAcquire(
        const AcquireReleaseInfo& acquireInfo,
        uint32                    gpuEventCount,
        const IGpuEvent*const*    ppGpuEvents) override;
    virtual void CmdReleaseThenAcquire(const AcquireReleaseInfo& barrierInfo) override;

    virtual void CmdCopyMemory(
        const IGpuMemory&       srcGpuMemory,
        const IGpuMemory&       dstGpuMemory,
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
    virtual void CmdCopyRegisterToMemory(
        uint32            srcRegisterOffset,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset) override;
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

    virtual void CmdCloneImageData(
        const IImage& srcImage,
        const IImage& dstImage) override;

    virtual void CmdUpdateMemory(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           dataSize,
        const uint32*     pData) override;

    virtual void CmdUpdateBusAddressableMemoryMarker(
        const IGpuMemory& dstGpuMemory,
        gpusize           offset,
        uint32            value) override;

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

    virtual void CmdCopyImageToPackedPixelImage(
        const IImage&          srcImage,
        const IImage&          dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        Pal::PackedPixelType   packPixelType) override;

    virtual void CmdSetEvent(
        const IGpuEvent& gpuEvent, HwPipePoint setPoint) override;
    virtual void CmdResetEvent(
        const IGpuEvent& gpuEvent, HwPipePoint resetPoint) override;
    virtual void CmdPredicateEvent(
        const IGpuEvent& gpuEvent) override;

    virtual void CmdMemoryAtomic(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        uint64            srcData,
        AtomicOp          atomicOp) override;

    virtual void CmdBeginQuery(
        const IQueryPool& queryPool,
        QueryType         queryType,
        uint32            slot,
        QueryControlFlags flags) override;
    virtual void CmdEndQuery(
        const IQueryPool& queryPool,
        QueryType         queryType,
        uint32            slot) override;
    virtual void CmdResolveQuery(
        const IQueryPool& queryPool,
        QueryResultFlags  flags,
        QueryType         queryType,
        uint32            startQuery,
        uint32            queryCount,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           dstStride) override;
    virtual void CmdResetQueryPool(
        const IQueryPool& queryPool,
        uint32            startQuery,
        uint32            queryCount) override;

    virtual void CmdWriteTimestamp(
        HwPipePoint       pipePoint,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset) override;
    virtual void CmdWriteImmediate(
        HwPipePoint        pipePoint,
        uint64             data,
        ImmediateDataWidth dataSize,
        gpusize            address) override;

    virtual void CmdLoadBufferFilledSizes(
        const gpusize (&gpuVirtAddr)[MaxStreamOutTargets]) override;
    virtual void CmdSaveBufferFilledSizes(
        const gpusize (&gpuVirtAddr)[MaxStreamOutTargets]) override;
    virtual void CmdSetBufferFilledSize(
        uint32  bufferId,
        uint32  offset) override;

    virtual void CmdBindBorderColorPalette(
        PipelineBindPoint          pipelineBindPoint,
        const IBorderColorPalette* pPalette) override;

    virtual void CmdSetPredication(
        IQueryPool*         pQueryPool,
        uint32              slot,
        const IGpuMemory*   pGpuMemory,
        gpusize             offset,
        PredicateType       predType,
        bool                predPolarity,
        bool                waitResults,
        bool                accumulateData) override;

    virtual void CmdIf(
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint64            data,
        uint64            mask,
        CompareFunc       compareFunc) override;
    virtual void CmdElse() override;
    virtual void CmdEndIf() override;
    virtual void CmdWhile(
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint64            data,
        uint64            mask,
        CompareFunc       compareFunc) override;
    virtual void CmdEndWhile() override;

    virtual void CmdWaitRegisterValue(
        uint32      registerOffset,
        uint32      data,
        uint32      mask,
        CompareFunc compareFunc) override;
    virtual void CmdWaitMemoryValue(
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            data,
        uint32            mask,
        CompareFunc       compareFunc) override;
    virtual void CmdWaitBusAddressableMemoryMarker(
        const IGpuMemory& gpuMemory,
        uint32            data,
        uint32            mask,
        CompareFunc       compareFunc) override;

    virtual void CmdBeginPerfExperiment(
        IPerfExperiment* pPerfExperiment) override;
    virtual void CmdUpdatePerfExperimentSqttTokenMask(
        IPerfExperiment*              pPerfExperiment,
        const ThreadTraceTokenConfig& sqttTokenConfig) override;
    virtual void CmdUpdateSqttTokenMask(
        const ThreadTraceTokenConfig& sqttTokenConfig) override;
    virtual void CmdEndPerfExperiment(
        IPerfExperiment* pPerfExperiment) override;

    virtual void CmdInsertTraceMarker(
        PerfTraceMarkerType markerType,
        uint32              markerData) override;
    virtual void CmdInsertRgpTraceMarker(
        uint32      numDwords,
        const void* pData) override;

    virtual void CmdLoadCeRam(
        const IGpuMemory& srcGpuMemory,
        gpusize           memOffset,
        uint32            ramOffset,
        uint32            dwordSize) override;
    virtual void CmdDumpCeRam(
        const IGpuMemory& dstGpuMemory,
        gpusize           memOffset,
        uint32            ramOffset,
        uint32            dwordSize,
        uint32            currRingPos,
        uint32            ringSize) override;
    virtual void CmdWriteCeRam(
        const void* pSrcData,
        uint32      ramOffset,
        uint32      dwordSize) override;

    virtual void CmdExecuteNestedCmdBuffers(
        uint32            cmdBufferCount,
        ICmdBuffer*const* ppCmdBuffers) override;

    virtual void CmdSaveComputeState(
        uint32 stateFlags) override;
    virtual void CmdRestoreComputeState(
        uint32 stateFlags) override;

    virtual void CmdExecuteIndirectCmds(
        const IIndirectCmdGenerator& generator,
        const IGpuMemory&            gpuMemory,
        gpusize                      offset,
        uint32                       maximumCount,
        gpusize                      countGpuAddr) override;

    virtual void CmdPostProcessFrame(
        const CmdPostProcessFrameInfo& postProcessInfo,
        bool*                          pAddedGpuWork) override;

    virtual void CmdSetUserClipPlanes(
        uint32               firstPlane,
        uint32               planeCount,
        const UserClipPlane* pPlanes) override;
    virtual void CmdSetClipRects(
        uint16      clipRule,
        uint32      rectCount,
        const Rect* pRectList) override;

    virtual void CmdFlglSync() override;
    virtual void CmdFlglEnable() override;
    virtual void CmdFlglDisable() override;

    virtual void CmdXdmaWaitFlipPending() override;

    virtual void CmdStartGpuProfilerLogging() override;
    virtual void CmdStopGpuProfilerLogging() override;

    virtual void CmdSetViewInstanceMask(uint32 mask) override;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 509
    virtual void CmdSetHiSCompareState0(
        CompareFunc compFunc,
        uint32      compMask,
        uint32      compValue,
        bool        enable) override;
    virtual void CmdSetHiSCompareState1(
        CompareFunc compFunc,
        uint32      compMask,
        uint32      compValue,
        bool        enable) override;
#endif

    virtual void CmdUpdateHiSPretests(
        const IImage*      pImage,
        const HiSPretests& pretests,
        uint32             firstMip,
        uint32             numMips) override;

    void NotifyDrawDispatchValidation(
        const Developer::DrawDispatchValidationData& data);
    void UpdateOptimizedRegisters(
        const Developer::OptimizedRegistersData& data);

    const Pm4Statistics& Statistics() const { return m_stats; }

    const RegisterInfoVector& ShRegs() const { return m_shRegs; }
    const RegisterInfoVector& CtxRegs() const { return m_ctxRegs; }

    uint16 ShRegBase() const { return m_shRegBase; }
    uint16 CtxRegBase() const { return m_ctxRegBase; }

private:
    virtual ~CmdBuffer() { }

    void ResetStatistics();

    void PreCall();
    void PostCall(CmdBufCallId callId);

    void PreDispatchCall();
    void PostDispatchCall(CmdBufCallId callId);

    void PreDrawCall();
    void PostDrawCall(CmdBufCallId callId);

    static void PAL_STDCALL CmdSetUserDataDecoratorCs(
        ICmdBuffer*   pCmdBuffer,
        uint32        firstEntry,
        uint32        entryCount,
        const uint32* pEntryValues);
    static void PAL_STDCALL CmdSetUserDataDecoratorGfx(
        ICmdBuffer*   pCmdBuffer,
        uint32        firstEntry,
        uint32        entryCount,
        const uint32* pEntryValues);

    static void PAL_STDCALL CmdDrawDecorator(
        ICmdBuffer* pCmdBuffer,
        uint32      firstVertex,
        uint32      vertexCount,
        uint32      firstInstance,
        uint32      instanceCount);
    static void PAL_STDCALL CmdDrawOpaqueDecorator(
        ICmdBuffer*   pCmdBuffer,
        gpusize       streamOutFilledSizeVa,
        uint32        streamOutOffset,
        uint32        stride,
        uint32        firstInstance,
        uint32        instanceCount);
    static void PAL_STDCALL CmdDrawIndexedDecorator(
        ICmdBuffer* pCmdBuffer,
        uint32      firstIndex,
        uint32      indexCount,
        int32       vertexOffset,
        uint32      firstInstance,
        uint32      instanceCount);
    static void PAL_STDCALL CmdDrawIndirectMultiDecorator(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);
    static void PAL_STDCALL CmdDrawIndexedIndirectMultiDecorator(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);

    static void PAL_STDCALL CmdDispatchDecorator(
        ICmdBuffer* pCmdBuffer,
        uint32      xDim,
        uint32      yDim,
        uint32      zDim);
    static void PAL_STDCALL CmdDispatchIndirectDecorator(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset);
    static void PAL_STDCALL CmdDispatchOffsetDecorator(
        ICmdBuffer* pCmdBuffer,
        uint32      xOffset,
        uint32      yOffset,
        uint32      zOffset,
        uint32      xDim,
        uint32      yDim,
        uint32      zDim);

    Pm4Statistics  m_stats;

    RegisterInfoVector  m_shRegs;
    RegisterInfoVector  m_ctxRegs;

    uint16  m_shRegBase;
    uint16  m_ctxRegBase;

    Developer::DrawDispatchValidationData  m_validationData;

    PAL_DISALLOW_DEFAULT_CTOR(CmdBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(CmdBuffer);
};

} // Pm4Instrumentor
} // Pal

#endif
