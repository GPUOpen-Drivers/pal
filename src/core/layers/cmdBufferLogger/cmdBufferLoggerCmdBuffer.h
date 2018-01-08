/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palLinearAllocator.h"
#include "core/layers/decorators.h"

namespace Pal
{
namespace CmdBufferLogger
{

// Identifies a specific ICmdBuffer function call in a token stream.  One enum per interface in ICmdBuffer.
enum class CmdBufCallId : uint32
{
    Begin,
    End,
    CmdBindPipeline,
    CmdBindMsaaState,
    CmdBindColorBlendState,
    CmdBindDepthStencilState,
    CmdBindCompoundState,
    CmdBindIndexData,
    CmdBindTargets,
    CmdBindStreamOutTargets,
    CmdBindBorderColorPalette,
    CmdSetUserData,
    CmdSetIndirectUserData,
    CmdSetIndirectUserDataWatermark,
    CmdSetBlendConst,
    CmdSetInputAssemblyState,
    CmdSetTriangleRasterState,
    CmdSetPointLineRasterState,
    CmdSetDepthBiasState,
    CmdSetDepthBounds,
    CmdSetStencilRefMasks,
    CmdSetMsaaQuadSamplePattern,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 339
    CmdStoreMsaaQuadSamplePattern,
    CmdLoadMsaaQuadSamplePattern,
#endif
    CmdSetViewports,
    CmdSetScissorRects,
    CmdSetGlobalScissor,
    CmdBarrier,
    CmdWaitRegisterValue,
    CmdWaitMemoryValue,
    CmdWaitBusAddressableMemoryMarker,
    CmdDraw,
    CmdDrawIndexed,
    CmdDrawIndirectMulti,
    CmdDrawIndexedIndirectMulti,
    CmdDispatch,
    CmdDispatchIndirect,
    CmdDispatchOffset,
    CmdUpdateMemory,
    CmdUpdateBusAddressableMemoryMarker,
    CmdFillMemory,
    CmdCopyMemory,
    CmdCopyTypedBuffer,
    CmdCopyRegisterToMemory,
    CmdCopyImage,
    CmdScaledCopyImage,
    CmdColorSpaceConversionCopy,
    CmdCloneImageData,
    CmdCopyMemoryToImage,
    CmdCopyImageToMemory,
    CmdClearColorBuffer,
    CmdClearBoundColorTargets,
    CmdClearColorImage,
    CmdClearBoundDepthStencilTargets,
    CmdClearDepthStencil,
    CmdClearBufferView,
    CmdClearImageView,
    CmdResolveImage,
    CmdSetEvent,
    CmdResetEvent,
    CmdPredicateEvent,
    CmdMemoryAtomic,
    CmdResetQueryPool,
    CmdBeginQuery,
    CmdEndQuery,
    CmdResolveQuery,
    CmdSetPredication,
    CmdWriteTimestamp,
    CmdWriteImmediate,
    CmdLoadGds,
    CmdStoreGds,
    CmdUpdateGds,
    CmdFillGds,
    CmdLoadBufferFilledSizes,
    CmdSaveBufferFilledSizes,
    CmdLoadCeRam,
    CmdWriteCeRam,
    CmdDumpCeRam,
    CmdExecuteNestedCmdBuffers,
    CmdExecuteIndirectCmds,
    CmdIf,
    CmdElse,
    CmdEndIf,
    CmdWhile,
    CmdEndWhile,
    CmdFlglSync,
    CmdFlglEnable,
    CmdFlglDisable,
    CmdBeginPerfExperiment,
    CmdUpdatePerfExperimentSqttTokenMask,
    CmdEndPerfExperiment,
    CmdInsertTraceMarker,
    CmdInsertRgpTraceMarker,
    CmdSaveComputeState,
    CmdRestoreComputeState,
    CmdSetUserClipPlanes,
    CmdXdmaWaitFlipPending,
    CmdCopyMemoryToTiledImage,
    CmdCopyTiledImageToMemory,
    CmdStartGpuProfilerLogging,
    CmdStopGpuProfilerLogging,
    CmdCopyImageToPackedPixelImage,
    CmdSetViewInstanceMask,
    Count
};

union CmdBufferLoggerFlags
{
    struct
    {
        uint32 logCmdBarrier        :  1;
        uint32 logCmdDraws          :  1;
        uint32 logCmdDispatchs      :  1;
        uint32 logCmdWriteTimestamp :  1;
        uint32 logCmdBinds          :  1;
        uint32 logCmdSetUserData    :  1;
        uint32 logCmdSets           :  1;
        uint32 logCmdBlts           :  1;
        uint32 logMiscellaneous     :  1;
        uint32 reserved             : 23;
    };

    uint32 u32All;
};

class Device;

// =====================================================================================================================
// CmdBufferLogger implementation of the ICmdBuffer interface.  In addition to passing commands on to the next layer,
// the various command buffer calls are annotated with the PAL API inputs directly within the command data.
//
// NOTE: This class inherits from CmdBufferDecorator. This is to get around a bug with multiple layers that inherit
//       straight from ICmdBuffer (related to the Next* functions). Any functions added to ICmdBuffer should be added
//       here as well.
class CmdBuffer: public CmdBufferDecorator
{
public:
    CmdBuffer(ICmdBuffer*                pNextCmdBuffer,
              const Device*              pDevice,
              const CmdBufferCreateInfo& createInfo);
    Result Init();

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
    virtual void CmdSetIndirectUserData(
        uint16      tableId,
        uint32      dwordOffset,
        uint32      dwordSize,
        const void* pSrcData) override;
    virtual void CmdSetIndirectUserDataWatermark(
        uint16 tableId,
        uint32 dwordLimit) override;
    virtual void CmdSetBlendConst(
        const BlendConstParams& params) override;
    virtual void CmdSetInputAssemblyState(
        const InputAssemblyStateParams& params) override;
    virtual void CmdSetTriangleRasterState(
        const TriangleRasterStateParams& params) override;
    virtual void CmdSetPointLineRasterState(
        const PointLineRasterStateParams& params) override;
    virtual void CmdSetDepthBiasState(
        const DepthBiasParams& params) override;
    virtual void CmdSetDepthBounds(
        const DepthBoundsParams& params) override;
    virtual void CmdSetStencilRefMasks(
        const StencilRefMaskParams& params) override;
    virtual void CmdSetMsaaQuadSamplePattern(
        uint32                       numSamplesPerPixel,
        const MsaaQuadSamplePattern& quadSamplePattern) override;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 339
    virtual void CmdStoreMsaaQuadSamplePattern(
        const IGpuMemory&            dstGpuMemory,
        gpusize                      dstMemOffset,
        uint32                       numSamplesPerPixel,
        const MsaaQuadSamplePattern& quadSamplePattern) override;
    virtual void CmdLoadMsaaQuadSamplePattern(
        const IGpuMemory* pSrcGpuMemory,
        gpusize           srcMemOffset) override;
#endif
    virtual void CmdSetViewports(
        const ViewportParams& params) override;
    virtual void CmdSetScissorRects(
        const ScissorRectParams& params) override;
    virtual void CmdSetGlobalScissor(
        const GlobalScissorParams& params) override;
    virtual void CmdBarrier(
        const BarrierInfo& barrierInfo) override;
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
    virtual void CmdUpdateMemory(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           dataSize,
        const uint32*     pData) override;
    virtual void CmdUpdateBusAddressableMemoryMarker(
        const IGpuMemory& dstGpuMemory,
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
        uint32                 flags) override;
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
        const ImageResolveRegion* pRegions) override;
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 311
    virtual void CmdSetPredication(
        IQueryPool*       pQueryPool,
        uint32            slot,
        const IGpuMemory* pGpuMemory,
        gpusize           offset,
        PredicateType     predType,
        bool              predPolarity,
        bool              waitResults,
        bool              accumulateData) override;
#else
    virtual void CmdSetPredication(
        IQueryPool*   pQueryPool,
        uint32        slot,
        gpusize       gpuVirtAddr,
        PredicateType predType,
        bool          predPolarity,
        bool          waitResults,
        bool          accumulateData) override;
#endif
    virtual void CmdWriteTimestamp(
        HwPipePoint       pipePoint,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset) override;
    virtual void CmdWriteImmediate(
        HwPipePoint        pipePoint,
        uint64             data,
        ImmediateDataWidth dataSize,
        gpusize            address) override;
    virtual void CmdLoadGds(
        HwPipePoint       pipePoint,
        uint32            dstGdsOffset,
        const IGpuMemory& srcGpuMemory,
        gpusize           srcMemOffset,
        uint32            size) override;
    virtual void CmdStoreGds(
        HwPipePoint       pipePoint,
        uint32            srcGdsOffset,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstMemOffset,
        uint32            size,
        bool              waitForWC) override;
    virtual void CmdUpdateGds(
        HwPipePoint       pipePoint,
        uint32            gdsOffset,
        uint32            dataSize,
        const uint32*     pData) override;
    virtual void CmdFillGds(
        HwPipePoint       pipePoint,
        uint32            gdsOffset,
        uint32            fillSize,
        uint32            data) override;
    virtual void CmdLoadBufferFilledSizes(
        const gpusize (&gpuVirtAddr)[MaxStreamOutTargets]) override;
    virtual void CmdSaveBufferFilledSizes(
        const gpusize (&gpuVirtAddr)[MaxStreamOutTargets]) override;
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
    virtual void CmdFlglSync() override;
    virtual void CmdFlglEnable() override;
    virtual void CmdFlglDisable() override;
    virtual void CmdBeginPerfExperiment(
        IPerfExperiment* pPerfExperiment) override;
    virtual void CmdUpdatePerfExperimentSqttTokenMask(
        IPerfExperiment* pPerfExperiment,
        uint32           sqttTokenMask) override;
    virtual void CmdEndPerfExperiment(
        IPerfExperiment* pPerfExperiment) override;
    virtual void CmdInsertTraceMarker(
        PerfTraceMarkerType markerType,
        uint32              markerData) override;
    virtual void CmdInsertRgpTraceMarker(
        uint32      numDwords,
        const void* pData) override;
    virtual void CmdSaveComputeState(
        uint32 stateFlags) override;
    virtual void CmdRestoreComputeState(
        uint32 stateFlags) override;
    virtual void CmdCommentString(
        const char* pComment) override;
    virtual void CmdStartGpuProfilerLogging() override;
    virtual void CmdStopGpuProfilerLogging() override;

    virtual void CmdSetUserClipPlanes(
        uint32               firstPlane,
        uint32               planeCount,
        const UserClipPlane* pPlanes) override;

    virtual void CmdXdmaWaitFlipPending() override;

    virtual void CmdCopyImageToPackedPixelImage(
        const IImage&          srcImage,
        const IImage&          dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        Pal::PackedPixelType   packPixelType) override;

    virtual void CmdSetViewInstanceMask(uint32 mask) override;

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        ICmdBuffer* pNextLayer = m_pNextLayer;
        this->~CmdBuffer();
        pNextLayer->Destroy();
    }

    void DescribeBarrier(
        const Developer::BarrierData* pData);

    Util::VirtualLinearAllocator* Allocator()    { return &m_allocator; }
    CmdBufferLoggerFlags          Flags()        { return m_flags;      }
    const Device*                 LoggerDevice() { return m_pDevice;    }

private:
    virtual ~CmdBuffer() { }

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
        uint32      instanceCount);
    static void PAL_STDCALL CmdDrawIndexed(
        ICmdBuffer* pCmdBuffer,
        uint32      firstIndex,
        uint32      indexCount,
        int32       vertexOffset,
        uint32      firstInstance,
        uint32      instanceCount);
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

    const Device*const           m_pDevice;
    Util::VirtualLinearAllocator m_allocator;       // Temp storage for argument translation.
    CmdBufferLoggerFlags         m_flags;

    PAL_DISALLOW_DEFAULT_CTOR(CmdBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(CmdBuffer);
};

} // CmdBufferLogger
} // Pal
