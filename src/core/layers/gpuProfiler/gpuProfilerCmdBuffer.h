/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/functionIds.h"

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
#include "core/layers/gpuProfiler/gpuProfilerPlatform.h"
#endif
#include "core/layers/gpuProfiler/gpuProfilerQueue.h"
#include "palLinearAllocator.h"

// Forward declarations.
namespace GpuUtil
{
    class      GpaSession;
}

namespace Pal
{
namespace GpuProfiler
{

class Device;
class TargetCmdBuffer;

// Used to track currently-bound compute/graphics pipeline/shader state during replay.
struct PipelineState
{
    PipelineInfo pipelineInfo;
    uint64       apiPsoHash;
};

// =====================================================================================================================
// GpuProfiler implementation of the ICmdBuffer interface.  Instead of passing commands on to the next layer, the
// various command buffer calls are tokenized and stored for later replay.  The Replay() interface will replay the
// tokenized sequence of calls made on this command buffer into a separate command buffer, while adding any
// instrumentation required for GPU profiling.  By this process, the same command buffer from the client/app perspective
// can be in flight multiple times simultanesouly while counters for the separate invocations are collected in
// difference sections of memory.
class CmdBuffer final : public CmdBufferDecorator
{
public:
    CmdBuffer(ICmdBuffer*                pNextCmdBuffer,
              Device*                    pDevice,
              const CmdBufferCreateInfo& createInfo,
              bool                       logPipeStats,
              bool                       enableSqThreadTrace);

    // This function will playback the commands recorded by this command buffer into the specified target command
    // buffer while instrumenting it with additional commands to gather timing, perf counters, etc.
    Result Replay(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuf, uint32 curFrame);

    bool ContainsPresent() const { return m_flags.containsPresent; }

    ICmdBuffer* NextLayer() { return GetNextLayer(); }
    const ICmdBuffer* NextLayer() const { return GetNextLayer(); }

    // Public ICmdBuffer interface methods.  Each one tokenizes the call and returns immediately.
    virtual Result Begin(
        const CmdBufferBuildInfo& info) override;
    virtual Result End() override;
    virtual Result Reset(ICmdAllocator* pCmdAllocator, bool returnGpuMemory) override;
    virtual void CmdBindPipeline(
        const PipelineBindParams& params) override;
    virtual void CmdBindMsaaState(
        const IMsaaState* pMsaaState) override;
    virtual void CmdBindColorBlendState(
        const IColorBlendState* pColorBlendState) override;
    virtual void CmdBindDepthStencilState(
        const IDepthStencilState* pDepthStencilState) override;
    virtual void CmdBindIndexData(
        gpusize gpuAddr, uint32 indexCount, IndexType indexType) override;
    virtual void CmdBindTargets(
        const BindTargetParams& params) override;
    virtual void CmdBindStreamOutTargets(
        const BindStreamOutTargetParams& params) override;
    virtual void CmdBindBorderColorPalette(
        PipelineBindPoint          pipelineBindPoint,
        const IBorderColorPalette* pPalette) override;
    virtual void CmdSetVertexBuffers(
        uint32 firstBuffer,
        uint32 bufferCount,
        const BufferViewInfo* pBuffers) override;
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
    virtual void CmdSetColorWriteMask(
        const ColorWriteMaskParams& params) override;
    virtual void CmdSetRasterizerDiscardEnable(
        bool rasterizerDiscardEnable) override;
    virtual void CmdBarrier(
        const BarrierInfo& barrierInfo) override;
    virtual void OptimizeBarrierReleaseInfo(
        uint32       pipePointWaitCount,
        HwPipePoint* pPipePoints,
        uint32*      pCacheMask) const override
    {
        // The optimization works in PAL core level. If any replay layer is enabled, this function should immediately
        // return the original pipe and cache info, and drop the call from propagating to next layer.
    }
    virtual void OptimizeAcqRelReleaseInfo(
        uint32* pStageMask,
        uint32* pAccessMask) const override
    {
        // The optimization works in PAL core level. If any replay layer is enabled, this function should immediately
        // return the original pipe and cache info, and drop the call from propagating to next layer.
    }
    virtual void CmdWaitRegisterValue(
        uint32      registerOffset,
        uint32      data,
        uint32      mask,
        CompareFunc compareFunc) override;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
    virtual uint32 CmdRelease(
        const AcquireReleaseInfo& releaseInfo) override;
    virtual void CmdAcquire(
        const AcquireReleaseInfo& acquireInfo,
        uint32                    syncTokenCount,
        const uint32*             pSyncTokens) override;
#endif

    virtual void CmdReleaseEvent(
        const AcquireReleaseInfo& releaseInfo,
        const IGpuEvent*          pGpuEvent) override;
    virtual void CmdAcquireEvent(
        const AcquireReleaseInfo& acquireInfo,
        uint32                    gpuEventCount,
        const IGpuEvent* const*   ppGpuEvents) override;

    virtual void CmdReleaseThenAcquire(const AcquireReleaseInfo& barrierInfo) override;
    virtual void CmdWaitMemoryValue(
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            data,
        uint32            mask,
        CompareFunc       compareFunc) override;
    virtual void CmdPrimeGpuCaches(
        uint32                    rangeCount,
        const PrimeGpuCacheRange* pRanges) override;
    virtual void CmdWaitBusAddressableMemoryMarker(
        const IGpuMemory& gpuMemory,
        uint32            data,
        uint32            mask,
        CompareFunc       compareFunc) override;
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
    virtual void CmdCopyMemory(
        const IGpuMemory&       srcGpuMemory,
        const IGpuMemory&       dstGpuMemory,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions) override;
    virtual void CmdCopyTypedBuffer(
        const IGpuMemory&            srcGpuMemory,
        const IGpuMemory&            dstGpuMemory,
        uint32                       regionCount,
        const TypedBufferCopyRegion* pRegions) override;
    virtual void CmdCopyRegisterToMemory(
        uint32            srcRegisterOffset,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset) override;
    virtual void CmdCopyImage(
        const IImage&          srcImage,
        ImageLayout            srcImageLayout,
        const IImage&          dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        const Rect*            pScissorRect,
        uint32                 flags) override;
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
    virtual void CmdCloneImageData(
        const IImage& srcImage,
        const IImage& dstImage) override;
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
    virtual void CmdClearColorBuffer(
        const IGpuMemory& gpuMemory,
        const ClearColor& color,
        SwizzledFormat    bufferFormat,
        uint32            bufferOffset,
        uint32            bufferExtent,
        uint32            rangeCount,
        const Range*      pRanges) override;
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
        uint32            rangeCount,
        const Range*      pRanges) override;
    virtual void CmdClearImageView(
        const IImage&     image,
        ImageLayout       imageLayout,
        const ClearColor& color,
        const void*       pImageViewSrd,
        uint32            rectCount,
        const Rect*       pRects) override;
    virtual void CmdResolveImage(
        const IImage&             srcImage,
        ImageLayout               srcImageLayout,
        const IImage&             dstImage,
        ImageLayout               dstImageLayout,
        ResolveMode               resolveMode,
        uint32                    regionCount,
        const ImageResolveRegion* pRegions,
        uint32                    flags) override;
    virtual void CmdSetEvent(
        const IGpuEvent& gpuEvent,
        HwPipePoint      setPoint) override;
    virtual void CmdResetEvent(
        const IGpuEvent& gpuEvent,
        HwPipePoint      resetPoint) override;
    virtual void CmdPredicateEvent(
        const IGpuEvent& gpuEvent) override;
    virtual void CmdMemoryAtomic(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        uint64            srcData,
        AtomicOp          atomicOp) override;
    virtual void CmdResetQueryPool(
        const IQueryPool& queryPool,
        uint32            startQuery,
        uint32            queryCount) override;
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
    virtual void CmdSetPredication(
        IQueryPool*         pQueryPool,
        uint32              slot,
        const IGpuMemory*   pGpuMemory,
        gpusize             offset,
        PredicateType       predType,
        bool                predPolarity,
        bool                waitResults,
        bool                accumulateData) override;
    virtual void CmdSuspendPredication(
        bool suspend) override;
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
    virtual void CmdLoadCeRam(
        const IGpuMemory& srcGpuMemory,
        gpusize           memOffset,
        uint32            ramOffset,
        uint32            dwordSize) override;
    virtual void CmdWriteCeRam(
        const void* pSrcData,
        uint32      ramOffset,
        uint32      dwordSize) override;
    virtual void CmdDumpCeRam(
        const IGpuMemory& dstGpuMemory,
        gpusize           memOffset,
        uint32            ramOffset,
        uint32            dwordSize,
        uint32            currRingPos,
        uint32            ringSize) override;
    virtual uint32 GetEmbeddedDataLimit() const override;
    virtual uint32* CmdAllocateEmbeddedData(
        uint32   sizeInDwords,
        uint32   alignmentInDwords,
        gpusize* pGpuAddress) override;
    virtual Result AllocateAndBindGpuMemToEvent(
        IGpuEvent* pGpuEvent) override;
    virtual void CmdExecuteNestedCmdBuffers(
        uint32            cmdBufferCount,
        ICmdBuffer*const* ppCmdBuffers) override;
    virtual void CmdExecuteIndirectCmds(
        const IIndirectCmdGenerator& generator,
        const IGpuMemory&            gpuMemory,
        gpusize                      offset,
        uint32                       maximumCount,
        gpusize                      countGpuAddr) override;
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
        uint32 markerData) override;
    virtual void CmdInsertRgpTraceMarker(
        uint32      numDwords,
        const void* pData) override;
    virtual void CmdSaveComputeState(
        uint32 stateFlags) override;
    virtual void CmdRestoreComputeState(
        uint32 stateFlags) override;
    virtual void CmdUpdateHiSPretests(
        const IImage*      pImage,
        const HiSPretests& pretests,
        uint32             firstMip,
        uint32             numMips) override;
    virtual void CmdFlglSync() override;
    virtual void CmdFlglEnable() override;
    virtual void CmdFlglDisable() override;
    virtual void CmdCommentString(
        const char* pComment) override;
    virtual void CmdNop(
        const void* pPayload,
        uint32      payloadSize) override;
    virtual uint32 CmdInsertExecutionMarker() override;
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
    virtual void CmdStartGpuProfilerLogging() override;
    virtual void CmdStopGpuProfilerLogging() override;

    virtual void CmdXdmaWaitFlipPending() override;

    virtual void CmdCopyImageToPackedPixelImage(
        const IImage&          srcImage,
        const IImage&          dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        Pal::PackedPixelType   packPixelType) override;

    virtual void CmdSetViewInstanceMask(uint32 mask) override;

    virtual void CmdSetPerDrawVrsRate(const VrsRateParams&  rateParams) override;

    virtual void CmdSetVrsCenterState(const VrsCenterState&  centerState) override;

    virtual void CmdBindSampleRateImage(const IImage*  pImage) override;

    virtual uint32 GetUsedSize(CmdAllocType type) const override
    {
        // Always return 0 used size for GpuProfiler mode as build CMD calls are recorded and may be not replayed yet.
        return 0;
    }

    virtual void CmdResolvePrtPlusImage(
        const IImage&                    srcImage,
        ImageLayout                      srcImageLayout,
        const IImage&                    dstImage,
        ImageLayout                      dstImageLayout,
        PrtPlusResolveType               resolveType,
        uint32                           regionCount,
        const PrtPlusImageResolveRegion* pRegions) override;

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        ICmdBuffer* pNextLayer = m_pNextLayer;
        this->~CmdBuffer();
        pNextLayer->Destroy();
    }

private:
    virtual ~CmdBuffer();

    static void PAL_STDCALL CmdSetUserDataCs(
        ICmdBuffer*   pCmdBuffer,
        uint32        firstEntry,
        uint32        entryCount,
        const uint32* pEntryValues);
    static void PAL_STDCALL CmdSetUserDataGfx(
        ICmdBuffer*   pCmdBuffer,
        uint32        firstEntry,
        uint32        entryCount,
        const uint32* pEntryValues);
    static void PAL_STDCALL CmdDraw(
        ICmdBuffer* pCmdBuffer,
        uint32      firstVertex,
        uint32      vertexCount,
        uint32      firstInstance,
        uint32      instanceCount,
        uint32      drawId);
    static void PAL_STDCALL CmdDrawOpaque(
        ICmdBuffer* pCmdBuffer,
        gpusize     streamOutFilledSizeVa,
        uint32      streamOutOffset,
        uint32      stride,
        uint32      firstInstance,
        uint32      instanceCount);
    static void PAL_STDCALL CmdDrawIndexed(
        ICmdBuffer* pCmdBuffer,
        uint32      firstIndex,
        uint32      indexCount,
        int32       vertexOffset,
        uint32      firstInstance,
        uint32      instanceCount,
        uint32      drawId);

    static void PAL_STDCALL CmdDrawIndirectMulti(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);
    static void PAL_STDCALL CmdDrawIndexedIndirectMulti(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);
    static void PAL_STDCALL CmdDispatch(
        ICmdBuffer* pCmdBuffer,
        uint32      x,
        uint32      y,
        uint32      z);
    static void PAL_STDCALL CmdDispatchIndirect(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset);
    static void PAL_STDCALL CmdDispatchOffset(
        ICmdBuffer* pCmdBuffer,
        uint32      xOffset,
        uint32      yOffset,
        uint32      zOffset,
        uint32      xDim,
        uint32      yDim,
        uint32      zDim);
    static void PAL_STDCALL CmdDispatchMesh(
        ICmdBuffer* pCmdBuffer,
        uint32      xDim,
        uint32      yDim,
        uint32      zDim);
    static void PAL_STDCALL CmdDispatchMeshIndirectMulti(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);

    void* AllocTokenSpace(size_t numBytes, size_t alignment);

    // Insert a copy of the specified value into the token stream.
    template <typename T> void InsertToken(const T& token)
    {
        T*const pDst = static_cast<T*>(AllocTokenSpace(sizeof(T), __alignof(T)));
        if (pDst != nullptr)
        {
            *pDst = token;
        }
    }

    // Insert a copy of an array of values into the token stream.
    template <typename T> void InsertTokenArray(const T* pData, uint32 count)
    {
        InsertToken(count);
        if (count > 0)
        {
            void*const pDst = AllocTokenSpace(sizeof(T) * count, __alignof(T));
            if (pDst != nullptr)
            {
                memcpy(pDst, pData, sizeof(T) * count);
            }
        }
    }

    // Retrieves the value of the next item in the token stream then advances the read pointer.  Complement of
    // InsertToken().
    template <typename T> const T& ReadTokenVal()
    {
        PAL_ASSERT(m_tokenStreamResult == Result::Success);
        m_tokenReadOffset = Util::Pow2Align(m_tokenReadOffset, __alignof(T));
        const T& val = *static_cast<T*>(Util::VoidPtrInc(m_pTokenStream, m_tokenReadOffset));
        m_tokenReadOffset += sizeof(T);
        return val;
    }

    // Retrieves a pointer to the next array of value(s) in the token stream then advances the read pointer.  Returns
    // the number of items stored in the array.  Complement of InsertTokenArray().
    template <typename T> uint32 ReadTokenArray(T** ppToken)
    {
        uint32 count = ReadTokenVal<uint32>();
        if (count != 0)
        {
            m_tokenReadOffset = Util::Pow2Align(m_tokenReadOffset, __alignof(T));
            *ppToken = static_cast<T*>(Util::VoidPtrInc(m_pTokenStream, m_tokenReadOffset));
            m_tokenReadOffset += sizeof(T) * count;
        }
        else
        {
            *ppToken = nullptr;
        }
        return count;
    }

    // Helper methods for each ICmdBuffer entry point that replay the recorded tokens into the specified target
    // command buffer.
    void ReplayBegin(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayEnd(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdBindPipeline(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdBindMsaaState(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdBindColorBlendState(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdBindDepthStencilState(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdBindIndexData(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdBindTargets(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdBindStreamOutTargets(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdBindBorderColorPalette(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdPrimeGpuCaches(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetUserData(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetVertexBuffers(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetPerDrawVrsRate(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetVrsCenterState(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdBindSampleRateImage(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdResolvePrtPlusImage(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetBlendConst(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetInputAssemblyState(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetTriangleRasterState(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetPointLineRasterState(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetLineStippleState(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetDepthBiasState(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetDepthBounds(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetStencilRefMasks(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetMsaaQuadSamplePattern(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetViewports(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetScissorRects(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetGlobalScissor(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetColorWriteMask(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetRasterizerDiscardEnable(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdBarrier(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
    void ReplayCmdRelease(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdAcquire(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
#endif
    void ReplayCmdReleaseEvent(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdAcquireEvent(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdReleaseThenAcquire(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdWaitRegisterValue(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdWaitMemoryValue(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdWaitBusAddressableMemoryMarker(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdDraw(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdDrawOpaque(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdDrawIndexed(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdDrawIndirectMulti(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdDrawIndexedIndirectMulti(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdDispatch(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdDispatchIndirect(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdDispatchOffset(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdDispatchMesh(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdDispatchMeshIndirectMulti(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdUpdateMemory(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdUpdateBusAddressableMemoryMarker(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdFillMemory(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdCopyMemory(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdCopyTypedBuffer(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdCopyRegisterToMemory(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdCopyImage(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdScaledCopyImage(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdGenerateMipmaps(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdColorSpaceConversionCopy(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdCloneImageData(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdCopyMemoryToImage(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdCopyImageToMemory(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdCopyMemoryToTiledImage(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdCopyTiledImageToMemory(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdClearColorBuffer(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdClearBoundColorTargets(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdClearColorImage(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdClearBoundDepthStencilTargets(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdClearDepthStencil(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdClearBufferView(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdClearImageView(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdResolveImage(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetEvent(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdResetEvent(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdPredicateEvent(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdMemoryAtomic(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdResetQueryPool(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdBeginQuery(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdEndQuery(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdResolveQuery(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetPredication(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSuspendPredication(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdWriteTimestamp(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdWriteImmediate(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdLoadBufferFilledSizes(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSaveBufferFilledSizes(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetBufferFilledSize(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdLoadCeRam(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdWriteCeRam(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdDumpCeRam(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdExecuteNestedCmdBuffers(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdExecuteIndirectCmds(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdIf(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdElse(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdEndIf(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdWhile(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdEndWhile(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdBeginPerfExperiment(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdUpdatePerfExperimentSqttTokenMask(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdUpdateSqttTokenMask(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdEndPerfExperiment(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdInsertTraceMarker(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdInsertRgpTraceMarker(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSaveComputeState(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdRestoreComputeState(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdCommentString(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdNop(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdInsertExecutionMarker(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdPostProcessFrame(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetUserClipPlanes(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetClipRects(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdXdmaWaitFlipPending(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdCopyImageToPackedPixelImage(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdUpdateHiSPretests(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdFlglSync(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdFlglEnable(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdFlglDisable(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdStartGpuProfilerLogging(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdStopGpuProfilerLogging(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);
    void ReplayCmdSetViewInstanceMask(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer);

    void LogPreTimedCall(
        Queue*           pQueue,
        TargetCmdBuffer* pTgtCmdBuffer,
        LogItem*         pLogItem,
        CmdBufCallId     callId);

    void LogPostTimedCall(Queue* pQueue, TargetCmdBuffer* pTgtCmdBuffer, LogItem* pLogItem);

    Device*const     m_pDevice;
    const QueueType  m_queueType;
    const EngineType m_engineType;

    // The token stream is a single block of memory that doubles in size each time it runs out of space.
    void*            m_pTokenStream;      // Storage for tokenized commands. Rewind here on command buffer reset.
    size_t           m_tokenStreamSize;   // The size of the token stream buffer in bytes.
    size_t           m_tokenWriteOffset;  // Write the next token at this offset within the token stream.
    size_t           m_tokenReadOffset;   // Read the next token at this offset within the token stream.
    Result           m_tokenStreamResult; // This must be Success unless an error occured during AllocTokenSpace.

    struct
    {
        uint32 logPipeStats        :  1;  // Pipeline stats should be collected based on specified data granularity.
        uint32 enableSqThreadTrace :  1;  // Thread traces should be collected based on specified data granularity.
        uint32 containsPresent     :  1;  // A CmdPresent() call is made in this command buffer.
        uint32 nested              :  1;  // This is a nested command buffer.
        uint32 reserved            : 28;
    } m_flags;

    union
    {
        struct
        {
            uint8 sqThreadTraceActive : 1;
            uint8 reserved            : 7;
        };
        uint8 u8All;
    } m_sampleFlags;

    // Track current bound compute pipeline/shader state during replay.
    PipelineState m_cpState;

    // Track current bound graphics pipeline/shader state during replay.
    PipelineState m_gfxpState;

    // State for disabling data gathering (e.g., timings, counters, etc.) for calls made in a larger timed scope.  For
    // example, commands in a while loop may play back multiple times, so we gather data for the entire while loop, and
    // disable gathering counters, etc. for calls made inside the while loop.
    bool m_disableDataGathering;

    // Scratch log items used for cases where a timed range of a command buffer spans multiple calls, like While()/
    // EndWhile(), or Begin()/End().
    LogItem m_cmdBufLogItem;
    LogItem m_loopLogItem;

    bool m_forceDrawGranularityLogging;

    uint32 m_curLogFrame;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
    // List of release tokens that are used to handle acquire/release interface through this layer's replay mechanism.
    uint32                             m_numReleaseTokens;
    Util::Vector<uint32, 16, Platform> m_releaseTokenList;
#endif
    PAL_DISALLOW_DEFAULT_CTOR(CmdBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(CmdBuffer);
};

// =====================================================================================================================
// Another GpuProfiler implementation of the ICmdBuffer interface. The default CmdBuffer implementation replays its
// commands into a TargetCmdBuffer, which is then forwarded to the next layer. This command buffer implementation
// allocates memory for comments as well, which are valid until this command buffer is restarted.
class TargetCmdBuffer : public CmdBufferFwdDecorator
{
public:
    TargetCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        ICmdBuffer*                pNextCmdBuffer,
        const DeviceDecorator*     pNextDevice,
        uint32                     subQueueIdx);

    Result Init();

    virtual Result Begin(const CmdBufferBuildInfo& info) override;

    void ResetBarrierString();
    void AddBarrierString(const char* pString);
    const char* GetBarrierString() const { return m_pCurrentBarrierComment; }

    virtual void UpdateCommentString(Developer::BarrierData* pData) override;

    void BeginSample(Queue* pQueue, LogItem* pLogItem, bool pipeStats, bool perfExp);
    void EndSample(Queue* pQueue, const LogItem* pLogItem);

    Result BeginGpaSession(Queue* pQueue);
    Result EndGpaSession(LogItem* pLogItem);
    GpuUtil::GpaSession* GetGpaSession() { return m_pGpaSession; }

    Result GetLastResult() const { return m_result; }
    void SetLastResult(Result result);

    bool IsFromMasterSubQue() const { return m_subQueueIdx == 0; }
    uint32 GetSubQueueIdx() const { return m_subQueueIdx; }

protected:
    virtual ~TargetCmdBuffer() {}

private:
    Util::VirtualLinearAllocator m_allocator;
    void*                        m_pAllocatorStream; // Base address of m_allocator. Rewind here on reset.

    // Track the current comment string for the barrier call.
    char*                        m_pCurrentBarrierComment;
    size_t                       m_currentCommentSize;

    const QueueType              m_queueType;         // Universal, compute, etc.
    const EngineType             m_engineType;
    bool                         m_supportTimestamps; // This command buffer (based on engine type) supports timestamps.

    GpuUtil::GpaSession*         m_pGpaSession;

    Result                       m_result; // Result from attempted operations.

    // The subQueue to which this tgtCmdBuf belongs. This won't change onced the tgtCmdBuf is created.
    const uint32                 m_subQueueIdx;

    PAL_DISALLOW_DEFAULT_CTOR(TargetCmdBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(TargetCmdBuffer);
};

} // GpuProfiler
} // Pal
