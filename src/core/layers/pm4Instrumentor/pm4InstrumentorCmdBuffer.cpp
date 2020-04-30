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

#if PAL_BUILD_PM4_INSTRUMENTOR

#include "core/layers/pm4Instrumentor/pm4InstrumentorCmdBuffer.h"
#include "core/layers/pm4Instrumentor/pm4InstrumentorDevice.h"
#include "core/layers/pm4Instrumentor/pm4InstrumentorPlatform.h"
#include "core/layers/pm4Instrumentor/pm4InstrumentorQueue.h"
#include "palVectorImpl.h"

using namespace Util;

namespace Pal
{
namespace Pm4Instrumentor
{

// =====================================================================================================================
CmdBuffer::CmdBuffer(
    ICmdBuffer*                pNextCmdBuffer,
    Device*                    pDevice,
    const CmdBufferCreateInfo& createInfo)
    :
    CmdBufferFwdDecorator(pNextCmdBuffer, pDevice),
    m_shRegs(static_cast<Platform*>(pDevice->GetPlatform())),
    m_ctxRegs(static_cast<Platform*>(pDevice->GetPlatform()))
{
    ResetStatistics();

    m_funcTable.pfnCmdSetUserData[static_cast<uint32>(PipelineBindPoint::Compute)]  = &CmdSetUserDataDecoratorCs;
    m_funcTable.pfnCmdSetUserData[static_cast<uint32>(PipelineBindPoint::Graphics)] = &CmdSetUserDataDecoratorGfx;
    m_funcTable.pfnCmdDraw                     = CmdDrawDecorator;
    m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaqueDecorator;
    m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexedDecorator;
    m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMultiDecorator;
    m_funcTable.pfnCmdDrawIndexedIndirectMulti = CmdDrawIndexedIndirectMultiDecorator;
    m_funcTable.pfnCmdDispatch                 = CmdDispatchDecorator;
    m_funcTable.pfnCmdDispatchIndirect         = CmdDispatchIndirectDecorator;
    m_funcTable.pfnCmdDispatchOffset           = CmdDispatchOffsetDecorator;
}

// =====================================================================================================================
void CmdBuffer::ResetStatistics()
{
    memset(&m_stats, 0, sizeof(m_stats));
    memset(&m_validationData, 0, sizeof(m_validationData));

    m_shRegs.Clear();
    m_ctxRegs.Clear();

    m_shRegBase  = 0;
    m_ctxRegBase = 0;
}

// =====================================================================================================================
void CmdBuffer::PreCall()
{
    m_stats.commandBufferSize = GetNextLayer()->GetUsedSize(CmdAllocType::CommandDataAlloc);
}

// =====================================================================================================================
void CmdBuffer::PreDispatchCall()
{
    PreCall();
    memset(&m_validationData, 0, sizeof(m_validationData));
}

// =====================================================================================================================
void CmdBuffer::PreDrawCall()
{
    PreCall();
    memset(&m_validationData, 0, sizeof(m_validationData));
}

// =====================================================================================================================
void CmdBuffer::PostCall(
    CmdBufCallId callId)
{
    const gpusize currentLen = GetNextLayer()->GetUsedSize(CmdAllocType::CommandDataAlloc);

    ++m_stats.call[static_cast<uint32>(callId)].count;
    m_stats.call[static_cast<uint32>(callId)].cmdSize += (currentLen - m_stats.commandBufferSize);
}

// =====================================================================================================================
void CmdBuffer::PostDispatchCall(
    CmdBufCallId callId)
{
    PostCall(callId);

    if (m_validationData.miscCmdSize > 0)
    {
        constexpr uint32 Id = static_cast<uint32>(InternalEventId::MiscDispatchValidation);

        ++m_stats.internalEvent[Id].count;
        m_stats.internalEvent[Id].cmdSize += m_validationData.miscCmdSize;
    }

    if (m_validationData.userDataCmdSize > 0)
    {
        constexpr uint32 Id = static_cast<uint32>(InternalEventId::UserDataValidationCs);

        ++m_stats.internalEvent[Id].count;
        m_stats.internalEvent[Id].cmdSize += m_validationData.userDataCmdSize;
    }

    if (m_validationData.pipelineCmdSize > 0)
    {
        constexpr uint32 Id = static_cast<uint32>(InternalEventId::PipelineValidationCs);

        ++m_stats.internalEvent[Id].count;
        m_stats.internalEvent[Id].cmdSize += m_validationData.pipelineCmdSize;
    }
}

// =====================================================================================================================
void CmdBuffer::PostDrawCall(
    CmdBufCallId callId)
{
    PostCall(callId);

    if (m_validationData.miscCmdSize > 0)
    {
        constexpr uint32 Id = static_cast<uint32>(InternalEventId::MiscDrawValidation);

        ++m_stats.internalEvent[Id].count;
        m_stats.internalEvent[Id].cmdSize += m_validationData.miscCmdSize;
    }

    if (m_validationData.userDataCmdSize > 0)
    {
        constexpr uint32 Id = static_cast<uint32>(InternalEventId::UserDataValidationGfx);

        ++m_stats.internalEvent[Id].count;
        m_stats.internalEvent[Id].cmdSize += m_validationData.userDataCmdSize;
    }

    if (m_validationData.pipelineCmdSize > 0)
    {
        constexpr uint32 Id = static_cast<uint32>(InternalEventId::PipelineValidationGfx);

        ++m_stats.internalEvent[Id].count;
        m_stats.internalEvent[Id].cmdSize += m_validationData.pipelineCmdSize;
    }
}

// =====================================================================================================================
void CmdBuffer::NotifyDrawDispatchValidation(
    const Developer::DrawDispatchValidationData& data)
{
    PAL_ASSERT(this == data.pCmdBuffer);
    m_validationData = data;
}

// =====================================================================================================================
void CmdBuffer::UpdateOptimizedRegisters(
    const Developer::OptimizedRegistersData& data)
{
    PAL_ASSERT(this == data.pCmdBuffer);

    if (m_shRegs.Reserve(data.shRegCount) == Result::Success)
    {
        for (uint32 i = 0; i < data.shRegCount; ++i)
        {
            RegisterInfo info;
            info.setPktTotal = data.pShRegSeenSets[i];
            info.setPktKept  = data.pShRegKeptSets[i];

            m_shRegs.PushBack(info);
        }

        m_shRegBase = data.shRegBase;
    }

    if (m_ctxRegs.Reserve(data.ctxRegCount) == Result::Success)
    {
        for (uint32 i = 0; i < data.ctxRegCount; ++i)
        {
            RegisterInfo info;
            info.setPktTotal = data.pCtxRegSeenSets[i];
            info.setPktKept  = data.pCtxRegKeptSets[i];

            m_ctxRegs.PushBack(info);
        }

        m_ctxRegBase = data.ctxRegBase;
    }
}

// =====================================================================================================================
Result CmdBuffer::Begin(
    const CmdBufferBuildInfo& buildInfo)
{
    ResetStatistics();

    PreCall();
    Result result = GetNextLayer()->Begin(buildInfo);
    if (result == Result::Success)
    {
        PostCall(CmdBufCallId::Begin);
    }

    return result;
}

// =====================================================================================================================
Result CmdBuffer::Reset(
    ICmdAllocator* pCmdAllocator,
    bool           returnGpuMemory)
{
    ResetStatistics();
    return GetNextLayer()->Reset(NextCmdAllocator(pCmdAllocator), returnGpuMemory);
}

// =====================================================================================================================
Result CmdBuffer::End()
{
    PreCall();
    Result result = GetNextLayer()->End();
    if (result == Result::Success)
    {
        PostCall(CmdBufCallId::End);

        m_stats.commandBufferSize = GetNextLayer()->GetUsedSize(CmdAllocType::CommandDataAlloc);
        m_stats.embeddedDataSize  = GetNextLayer()->GetUsedSize(CmdAllocType::EmbeddedDataAlloc);
        m_stats.gpuScratchMemSize = GetNextLayer()->GetUsedSize(CmdAllocType::GpuScratchMemAlloc);
    }

    return result;
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdSetUserDataDecoratorCs(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    CmdBuffer*const  pThis = static_cast<CmdBuffer*>(pCmdBuffer);
    ICmdBuffer*const pNext = pThis->GetNextLayer();

    pThis->PreCall();
    pNext->CmdSetUserData(PipelineBindPoint::Compute, firstEntry, entryCount, pEntryValues);
    pThis->PostCall(CmdBufCallId::CmdSetUserData);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdSetUserDataDecoratorGfx(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    CmdBuffer*const  pThis = static_cast<CmdBuffer*>(pCmdBuffer);
    ICmdBuffer*const pNext = pThis->GetNextLayer();

    pThis->PreCall();
    pNext->CmdSetUserData(PipelineBindPoint::Graphics, firstEntry, entryCount, pEntryValues);
    pThis->PostCall(CmdBufCallId::CmdSetUserData);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawDecorator(
    ICmdBuffer* pCmdBuffer,
    uint32      firstVertex,
    uint32      vertexCount,
    uint32      firstInstance,
    uint32      instanceCount)
{
    CmdBuffer*const  pThis = static_cast<CmdBuffer*>(pCmdBuffer);
    ICmdBuffer*const pNext = pThis->GetNextLayer();

    pThis->PreDrawCall();
    pNext->CmdDraw(firstVertex, vertexCount, firstInstance, instanceCount);
    pThis->PostDrawCall(CmdBufCallId::CmdDraw);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawOpaqueDecorator(
    ICmdBuffer*   pCmdBuffer,
    gpusize       streamOutFilledSizeVa,
    uint32        streamOutOffset,
    uint32        stride,
    uint32        firstInstance,
    uint32        instanceCount)
{
    CmdBuffer*const  pThis = static_cast<CmdBuffer*>(pCmdBuffer);
    ICmdBuffer*const pNext = pThis->GetNextLayer();

    pThis->PreDrawCall();
    pNext->CmdDrawOpaque(streamOutFilledSizeVa, streamOutOffset, stride, firstInstance, instanceCount);
    pThis->PostDrawCall(CmdBufCallId::CmdDrawOpaque);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndexedDecorator(
    ICmdBuffer* pCmdBuffer,
    uint32      firstIndex,
    uint32      indexCount,
    int32       vertexOffset,
    uint32      firstInstance,
    uint32      instanceCount)
{
    CmdBuffer*const  pThis = static_cast<CmdBuffer*>(pCmdBuffer);
    ICmdBuffer*const pNext = pThis->GetNextLayer();

    pThis->PreDrawCall();
    pNext->CmdDrawIndexed(firstIndex, indexCount, vertexOffset, firstInstance, instanceCount);
    pThis->PostDrawCall(CmdBufCallId::CmdDrawIndexed);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndirectMultiDecorator(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    CmdBuffer*const  pThis = static_cast<CmdBuffer*>(pCmdBuffer);
    ICmdBuffer*const pNext = pThis->GetNextLayer();

    pThis->PreDrawCall();
    pNext->CmdDrawIndirectMulti(*NextGpuMemory(&gpuMemory), offset, stride, maximumCount, countGpuAddr);
    pThis->PostDrawCall(CmdBufCallId::CmdDrawIndirectMulti);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndexedIndirectMultiDecorator(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    CmdBuffer*const  pThis = static_cast<CmdBuffer*>(pCmdBuffer);
    ICmdBuffer*const pNext = pThis->GetNextLayer();

    pThis->PreDrawCall();
    pNext->CmdDrawIndexedIndirectMulti(*NextGpuMemory(&gpuMemory), offset, stride, maximumCount, countGpuAddr);
    pThis->PostDrawCall(CmdBufCallId::CmdDrawIndexedIndirectMulti);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchDecorator(
    ICmdBuffer* pCmdBuffer,
    uint32      xDim,
    uint32      yDim,
    uint32      zDim)
{
    CmdBuffer*const  pThis = static_cast<CmdBuffer*>(pCmdBuffer);
    ICmdBuffer*const pNext = pThis->GetNextLayer();

    pThis->PreDispatchCall();
    pNext->CmdDispatch(xDim, yDim, zDim);
    pThis->PostDispatchCall(CmdBufCallId::CmdDispatch);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchIndirectDecorator(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset)
{
    CmdBuffer*const  pThis = static_cast<CmdBuffer*>(pCmdBuffer);
    ICmdBuffer*const pNext = pThis->GetNextLayer();

    pThis->PreDispatchCall();
    pNext->CmdDispatchIndirect(*NextGpuMemory(&gpuMemory), offset);
    pThis->PostDispatchCall(CmdBufCallId::CmdDispatchIndirect);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchOffsetDecorator(
    ICmdBuffer* pCmdBuffer,
    uint32      xOffset,
    uint32      yOffset,
    uint32      zOffset,
    uint32      xDim,
    uint32      yDim,
    uint32      zDim)
{
    CmdBuffer*const  pThis = static_cast<CmdBuffer*>(pCmdBuffer);
    ICmdBuffer*const pNext = pThis->GetNextLayer();

    pThis->PreDispatchCall();
    pNext->CmdDispatchOffset(xOffset, yOffset, zOffset, xDim, yDim, zDim);
    pThis->PostDispatchCall(CmdBufCallId::CmdDispatchOffset);
}

// =====================================================================================================================
void CmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    PreCall();
    CmdBufferFwdDecorator::CmdBindPipeline(params);
    PostCall(CmdBufCallId::CmdBindPipeline);
}

// =====================================================================================================================
void CmdBuffer::CmdBindMsaaState(
    const IMsaaState* pMsaaState)
{
    PreCall();
    CmdBufferFwdDecorator::CmdBindMsaaState(pMsaaState);
    PostCall(CmdBufCallId::CmdBindMsaaState);
}

// =====================================================================================================================
void CmdBuffer::CmdBindColorBlendState(
    const IColorBlendState* pColorBlendState)
{
    PreCall();
    CmdBufferFwdDecorator::CmdBindColorBlendState(pColorBlendState);
    PostCall(CmdBufCallId::CmdBindColorBlendState);
}

// =====================================================================================================================
void CmdBuffer::CmdBindDepthStencilState(
    const IDepthStencilState* pDepthStencilState)
{
    PreCall();
    CmdBufferFwdDecorator::CmdBindDepthStencilState(pDepthStencilState);
    PostCall(CmdBufCallId::CmdBindDepthStencilState);
}

// =====================================================================================================================
void CmdBuffer::CmdBindIndexData(
    gpusize   gpuAddr,
    uint32    indexCount,
    IndexType indexType)
{
    PreCall();
    CmdBufferFwdDecorator::CmdBindIndexData(gpuAddr, indexCount, indexType);
    PostCall(CmdBufCallId::CmdBindIndexData);
}

// =====================================================================================================================
void CmdBuffer::CmdSetVertexBuffers(
    uint32                firstBuffer,
    uint32                bufferCount,
    const BufferViewInfo* pBuffers)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetVertexBuffers(firstBuffer, bufferCount, pBuffers);
    PostCall(CmdBufCallId::CmdSetVertexBuffers);
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 473
// =====================================================================================================================
void CmdBuffer::CmdSetIndirectUserData(
    uint16      tableId,
    uint32      dwordOffset,
    uint32      dwordSize,
    const void* pSrcData)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetIndirectUserData(tableId, dwordOffset, dwordSize, pSrcData);
    PostCall(CmdBufCallId::CmdSetIndirectUserData);
}

// =====================================================================================================================
void CmdBuffer::CmdSetIndirectUserDataWatermark(
    uint16 tableId,
    uint32 dwordLimit)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetIndirectUserDataWatermark(tableId, dwordLimit);
    PostCall(CmdBufCallId::CmdSetIndirectUserDataWatermark);
}
#endif

// =====================================================================================================================
void CmdBuffer::CmdBindTargets(
    const BindTargetParams& params)
{
    PreCall();
    CmdBufferFwdDecorator::CmdBindTargets(params);
    PostCall(CmdBufCallId::CmdBindTargets);
}

// =====================================================================================================================
void CmdBuffer::CmdBindStreamOutTargets(
    const BindStreamOutTargetParams& params)
{
    PreCall();
    CmdBufferFwdDecorator::CmdBindStreamOutTargets(params);
    PostCall(CmdBufCallId::CmdBindStreamOutTargets);
}

// =====================================================================================================================
void CmdBuffer::CmdSetBlendConst(
    const BlendConstParams& params)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetBlendConst(params);
    PostCall(CmdBufCallId::CmdSetBlendConst);
}

// =====================================================================================================================
void CmdBuffer::CmdSetInputAssemblyState(
    const InputAssemblyStateParams& params)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetInputAssemblyState(params);
    PostCall(CmdBufCallId::CmdSetInputAssemblyState);
}

// =====================================================================================================================
void CmdBuffer::CmdSetTriangleRasterState(
    const TriangleRasterStateParams& params)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetTriangleRasterState(params);
    PostCall(CmdBufCallId::CmdSetTriangleRasterState);
}

// =====================================================================================================================
void CmdBuffer::CmdSetPointLineRasterState(
    const PointLineRasterStateParams& params)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetPointLineRasterState(params);
    PostCall(CmdBufCallId::CmdSetPointLineRasterState);
}

// =====================================================================================================================
void CmdBuffer::CmdSetLineStippleState(
    const LineStippleStateParams& params)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetLineStippleState(params);
    PostCall(CmdBufCallId::CmdSetLineStippleState);
}

// =====================================================================================================================
void CmdBuffer::CmdSetDepthBiasState(
    const DepthBiasParams& params)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetDepthBiasState(params);
    PostCall(CmdBufCallId::CmdSetDepthBiasState);
}

// =====================================================================================================================
void CmdBuffer::CmdSetDepthBounds(
    const DepthBoundsParams& params)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetDepthBounds(params);
    PostCall(CmdBufCallId::CmdSetDepthBounds);
}

// =====================================================================================================================
void CmdBuffer::CmdSetStencilRefMasks(
    const StencilRefMaskParams& params)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetStencilRefMasks(params);
    PostCall(CmdBufCallId::CmdSetStencilRefMasks);
}

// =====================================================================================================================
void CmdBuffer::CmdSetMsaaQuadSamplePattern(
    uint32                       numSamplesPerPixel,
    const MsaaQuadSamplePattern& quadSamplePattern)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetMsaaQuadSamplePattern(numSamplesPerPixel, quadSamplePattern);
    PostCall(CmdBufCallId::CmdSetMsaaQuadSamplePattern);
}

// =====================================================================================================================
void CmdBuffer::CmdSetViewports(
    const ViewportParams& params)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetViewports(params);
    PostCall(CmdBufCallId::CmdSetViewports);
}

// =====================================================================================================================
void CmdBuffer::CmdSetScissorRects(
    const ScissorRectParams& params)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetScissorRects(params);
    PostCall(CmdBufCallId::CmdSetScissorRects);
}

// =====================================================================================================================
void CmdBuffer::CmdSetGlobalScissor(
    const GlobalScissorParams& params)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetGlobalScissor(params);
    PostCall(CmdBufCallId::CmdSetGlobalScissor);
}

// =====================================================================================================================
void CmdBuffer::CmdBarrier(
    const BarrierInfo& barrierInfo)
{
    PreCall();
    CmdBufferFwdDecorator::CmdBarrier(barrierInfo);
    PostCall(CmdBufCallId::CmdBarrier);
}

// =====================================================================================================================
void CmdBuffer::CmdRelease(
    const AcquireReleaseInfo& releaseInfo,
    const IGpuEvent*          pGpuEvent)
{
    PreCall();
    CmdBufferFwdDecorator::CmdRelease(releaseInfo, pGpuEvent);
    PostCall(CmdBufCallId::CmdRelease);
}

// =====================================================================================================================
void CmdBuffer::CmdAcquire(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    gpuEventCount,
    const IGpuEvent*const*    ppGpuEvents)
{
    PreCall();
    CmdBufferFwdDecorator::CmdAcquire(acquireInfo, gpuEventCount, ppGpuEvents);
    PostCall(CmdBufCallId::CmdAcquire);
}

// =====================================================================================================================
void CmdBuffer::CmdReleaseThenAcquire(
    const AcquireReleaseInfo& barrierInfo)
{
    PreCall();
    CmdBufferFwdDecorator::CmdReleaseThenAcquire(barrierInfo);
    PostCall(CmdBufCallId::CmdReleaseThenAcquire);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyMemory(
    const IGpuMemory&       srcGpuMemory,
    const IGpuMemory&       dstGpuMemory,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions)
{
    PreCall();
    CmdBufferFwdDecorator::CmdCopyMemory(srcGpuMemory, dstGpuMemory, regionCount, pRegions);
    PostCall(CmdBufCallId::CmdCopyMemory);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyImage(
    const IImage&          srcImage,
    ImageLayout            srcImageLayout,
    const IImage&          dstImage,
    ImageLayout            dstImageLayout,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    uint32                 flags)
{
    PreCall();
    CmdBufferFwdDecorator::CmdCopyImage(srcImage,
                                        srcImageLayout,
                                        dstImage,
                                        dstImageLayout,
                                        regionCount,
                                        pRegions,
                                        flags);
    PostCall(CmdBufCallId::CmdCopyImage);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyMemoryToImage(
    const IGpuMemory&            srcGpuMemory,
    const IImage&                dstImage,
    ImageLayout                  dstImageLayout,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions)
{
    PreCall();
    CmdBufferFwdDecorator::CmdCopyMemoryToImage(srcGpuMemory, dstImage, dstImageLayout, regionCount, pRegions);
    PostCall(CmdBufCallId::CmdCopyMemoryToImage);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyImageToMemory(
    const IImage&                srcImage,
    ImageLayout                  srcImageLayout,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions)
{
    PreCall();
    CmdBufferFwdDecorator::CmdCopyImageToMemory(srcImage, srcImageLayout, dstGpuMemory, regionCount, pRegions);
    PostCall(CmdBufCallId::CmdCopyImageToMemory);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyMemoryToTiledImage(
    const IGpuMemory&                 srcGpuMemory,
    const IImage&                     dstImage,
    ImageLayout                       dstImageLayout,
    uint32                            regionCount,
    const MemoryTiledImageCopyRegion* pRegions)
{
    PreCall();
    CmdBufferFwdDecorator::CmdCopyMemoryToTiledImage(srcGpuMemory, dstImage, dstImageLayout, regionCount, pRegions);
    PostCall(CmdBufCallId::CmdCopyMemoryToTiledImage);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyTiledImageToMemory(
    const IImage&                     srcImage,
    ImageLayout                       srcImageLayout,
    const IGpuMemory&                 dstGpuMemory,
    uint32                            regionCount,
    const MemoryTiledImageCopyRegion* pRegions)
{
    PreCall();
    CmdBufferFwdDecorator::CmdCopyTiledImageToMemory(srcImage, srcImageLayout, dstGpuMemory, regionCount, pRegions);
    PostCall(CmdBufCallId::CmdCopyTiledImageToMemory);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyTypedBuffer(
    const IGpuMemory&            srcGpuMemory,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const TypedBufferCopyRegion* pRegions)
{
    PreCall();
    CmdBufferFwdDecorator::CmdCopyTypedBuffer(srcGpuMemory, dstGpuMemory, regionCount, pRegions);
    PostCall(CmdBufCallId::CmdCopyTypedBuffer);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyRegisterToMemory(
    uint32            srcRegisterOffset,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    PreCall();
    CmdBufferFwdDecorator::CmdCopyRegisterToMemory(srcRegisterOffset, dstGpuMemory, dstOffset);
    PostCall(CmdBufCallId::CmdCopyRegisterToMemory);
}

// =====================================================================================================================
void CmdBuffer::CmdScaledCopyImage(
    const ScaledCopyInfo& copyInfo)
{
    PreCall();
    CmdBufferFwdDecorator::CmdScaledCopyImage(copyInfo);
    PostCall(CmdBufCallId::CmdScaledCopyImage);
}

// =====================================================================================================================
void CmdBuffer::CmdColorSpaceConversionCopy(
    const IImage&                     srcImage,
    ImageLayout                       srcImageLayout,
    const IImage&                     dstImage,
    ImageLayout                       dstImageLayout,
    uint32                            regionCount,
    const ColorSpaceConversionRegion* pRegions,
    TexFilter                         filter,
    const ColorSpaceConversionTable&  cscTable)
{
    PreCall();
    CmdBufferFwdDecorator::CmdColorSpaceConversionCopy(srcImage,
                                                       srcImageLayout,
                                                       dstImage,
                                                       dstImageLayout,
                                                       regionCount,
                                                       pRegions,
                                                       filter,
                                                       cscTable);
    PostCall(CmdBufCallId::CmdColorSpaceConversionCopy);
}

// =====================================================================================================================
void CmdBuffer::CmdCloneImageData(
    const IImage& srcImage,
    const IImage& dstImage)
{
    PreCall();
    CmdBufferFwdDecorator::CmdCloneImageData(srcImage, dstImage);
    PostCall(CmdBufCallId::CmdCloneImageData);
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           dataSize,
    const uint32*     pData)
{
    PreCall();
    CmdBufferFwdDecorator::CmdUpdateMemory(dstGpuMemory, dstOffset, dataSize, pData);
    PostCall(CmdBufCallId::CmdUpdateMemory);
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateBusAddressableMemoryMarker(
    const IGpuMemory& dstGpuMemory,
    gpusize           offset,
    uint32            value)
{
    PreCall();
    CmdBufferFwdDecorator::CmdUpdateBusAddressableMemoryMarker(dstGpuMemory, offset, value);
    PostCall(CmdBufCallId::CmdUpdateBusAddressableMemoryMarker);
}

// =====================================================================================================================
void CmdBuffer::CmdFillMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           fillSize,
    uint32            data)
{
    PreCall();
    CmdBufferFwdDecorator::CmdFillMemory(dstGpuMemory, dstOffset, fillSize, data);
    PostCall(CmdBufCallId::CmdFillMemory);
}

// =====================================================================================================================
void CmdBuffer::CmdClearColorBuffer(
    const IGpuMemory& gpuMemory,
    const ClearColor& color,
    SwizzledFormat    bufferFormat,
    uint32            bufferOffset,
    uint32            bufferExtent,
    uint32            rangeCount,
    const Range*      pRanges)
{
    PreCall();
    CmdBufferFwdDecorator::CmdClearColorBuffer(gpuMemory,
                                               color,
                                               bufferFormat,
                                               bufferOffset,
                                               bufferExtent,
                                               rangeCount,
                                               pRanges);
    PostCall(CmdBufCallId::CmdClearColorBuffer);
}

// =====================================================================================================================
void CmdBuffer::CmdClearBoundColorTargets(
    uint32                          colorTargetCount,
    const BoundColorTarget*         pBoundColorTargets,
    uint32                          regionCount,
    const ClearBoundTargetRegion*   pClearRegions)
{
    PreCall();
    CmdBufferFwdDecorator::CmdClearBoundColorTargets(colorTargetCount, pBoundColorTargets, regionCount, pClearRegions);
    PostCall(CmdBufCallId::CmdClearBoundColorTargets);
}

// =====================================================================================================================
void CmdBuffer::CmdClearColorImage(
    const IImage&      image,
    ImageLayout        imageLayout,
    const ClearColor&  color,
    uint32             rangeCount,
    const SubresRange* pRanges,
    uint32             boxCount,
    const Box*         pBoxes,
    uint32             flags)
{
    PreCall();
    CmdBufferFwdDecorator::CmdClearColorImage(image,
                                              imageLayout,
                                              color,
                                              rangeCount,
                                              pRanges,
                                              boxCount,
                                              pBoxes,
                                              flags);
    PostCall(CmdBufCallId::CmdClearColorImage);
}

// =====================================================================================================================
void CmdBuffer::CmdClearBoundDepthStencilTargets(
    float                         depth,
    uint8                         stencil,
    uint8                         stencilWriteMask,
    uint32                        samples,
    uint32                        fragments,
    DepthStencilSelectFlags       flags,
    uint32                        regionCount,
    const ClearBoundTargetRegion* pClearRegions)
{
    PreCall();
    CmdBufferFwdDecorator::CmdClearBoundDepthStencilTargets(depth,
                                                            stencil,
                                                            stencilWriteMask,
                                                            samples,
                                                            fragments,
                                                            flags,
                                                            regionCount,
                                                            pClearRegions);
    PostCall(CmdBufCallId::CmdClearBoundDepthStencilTargets);
}

// =====================================================================================================================
void CmdBuffer::CmdClearDepthStencil(
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
    uint32             flags)
{
    PreCall();
    CmdBufferFwdDecorator::CmdClearDepthStencil(image,
                                                depthLayout,
                                                stencilLayout,
                                                depth,
                                                stencil,
                                                stencilWriteMask,
                                                rangeCount,
                                                pRanges,
                                                rectCount,
                                                pRects,
                                                flags);
    PostCall(CmdBufCallId::CmdClearDepthStencil);
}

// =====================================================================================================================
void CmdBuffer::CmdClearBufferView(
    const IGpuMemory& gpuMemory,
    const ClearColor& color,
    const void*       pBufferViewSrd,
    uint32            rangeCount,
    const Range*      pRanges)
{
    PreCall();
    CmdBufferFwdDecorator::CmdClearBufferView(gpuMemory, color, pBufferViewSrd, rangeCount, pRanges);
    PostCall(CmdBufCallId::CmdClearBufferView);
}

// =====================================================================================================================
void CmdBuffer::CmdClearImageView(
    const IImage&     image,
    ImageLayout       imageLayout,
    const ClearColor& color,
    const void*       pImageViewSrd,
    uint32            rectCount,
    const Rect*       pRects)
{
    PreCall();
    CmdBufferFwdDecorator::CmdClearImageView(image,
                                             imageLayout,
                                             color,
                                             pImageViewSrd,
                                             rectCount,
                                             pRects);
    PostCall(CmdBufCallId::CmdClearImageView);
}

// =====================================================================================================================
void CmdBuffer::CmdResolveImage(
    const IImage&             srcImage,
    ImageLayout               srcImageLayout,
    const IImage&             dstImage,
    ImageLayout               dstImageLayout,
    ResolveMode               resolveMode,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions)
{
    PreCall();
    CmdBufferFwdDecorator::CmdResolveImage(srcImage,
                                           srcImageLayout,
                                           dstImage,
                                           dstImageLayout,
                                           resolveMode,
                                           regionCount,
                                           pRegions);
    PostCall(CmdBufCallId::CmdResolveImage);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyImageToPackedPixelImage(
    const IImage&          srcImage,
    const IImage&          dstImage,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    Pal::PackedPixelType   packPixelType)
{
    PreCall();
    CmdBufferFwdDecorator::CmdCopyImageToPackedPixelImage(srcImage, dstImage, regionCount, pRegions, packPixelType);
    PostCall(CmdBufCallId::CmdCopyImageToPackedPixelImage);
}

// =====================================================================================================================
void CmdBuffer::CmdSetEvent(
    const IGpuEvent& gpuEvent,
    HwPipePoint      setPoint)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetEvent(gpuEvent, setPoint);
    PostCall(CmdBufCallId::CmdSetEvent);
}

// =====================================================================================================================
void CmdBuffer::CmdResetEvent(
    const IGpuEvent& gpuEvent,
    HwPipePoint      resetPoint)
{
    PreCall();
    CmdBufferFwdDecorator::CmdResetEvent(gpuEvent, resetPoint);
    PostCall(CmdBufCallId::CmdResetEvent);
}

// =====================================================================================================================
void CmdBuffer::CmdPredicateEvent(
    const IGpuEvent& gpuEvent)
{
    PreCall();
    CmdBufferFwdDecorator::CmdPredicateEvent(gpuEvent);
    PostCall(CmdBufCallId::CmdPredicateEvent);
}

// =====================================================================================================================
void CmdBuffer::CmdMemoryAtomic(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        uint64            srcData,
        AtomicOp          atomicOp)
{
    PreCall();
    CmdBufferFwdDecorator::CmdMemoryAtomic(dstGpuMemory, dstOffset, srcData, atomicOp);
    PostCall(CmdBufCallId::CmdMemoryAtomic);
}

// =====================================================================================================================
void CmdBuffer::CmdBeginQuery(
        const IQueryPool& queryPool,
        QueryType         queryType,
        uint32            slot,
        QueryControlFlags flags)
{
    PreCall();
    CmdBufferFwdDecorator::CmdBeginQuery(queryPool, queryType, slot, flags);
    PostCall(CmdBufCallId::CmdBeginQuery);
}

// =====================================================================================================================
void CmdBuffer::CmdEndQuery(
        const IQueryPool& queryPool,
        QueryType         queryType,
        uint32            slot)
{
    PreCall();
    CmdBufferFwdDecorator::CmdEndQuery(queryPool, queryType, slot);
    PostCall(CmdBufCallId::CmdEndQuery);
}

// =====================================================================================================================
void CmdBuffer::CmdResolveQuery(
    const IQueryPool& queryPool,
    QueryResultFlags  flags,
    QueryType         queryType,
    uint32            startQuery,
    uint32            queryCount,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           dstStride)
{
    PreCall();
    CmdBufferFwdDecorator::CmdResolveQuery(queryPool,
                                           flags,
                                           queryType,
                                           startQuery,
                                           queryCount,
                                           dstGpuMemory,
                                           dstOffset,
                                           dstStride);
    PostCall(CmdBufCallId::CmdResolveQuery);
}

// =====================================================================================================================
void CmdBuffer::CmdResetQueryPool(
    const IQueryPool& queryPool,
    uint32            startQuery,
    uint32            queryCount)
{
    PreCall();
    CmdBufferFwdDecorator::CmdResetQueryPool(queryPool, startQuery, queryCount);
    PostCall(CmdBufCallId::CmdResetQueryPool);
}

// =====================================================================================================================
void CmdBuffer::CmdWriteTimestamp(
    HwPipePoint       pipePoint,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    PreCall();
    CmdBufferFwdDecorator::CmdWriteTimestamp(pipePoint, dstGpuMemory, dstOffset);
    PostCall(CmdBufCallId::CmdWriteTimestamp);
}

// =====================================================================================================================
void CmdBuffer::CmdWriteImmediate(
    HwPipePoint        pipePoint,
    uint64             data,
    ImmediateDataWidth dataSize,
    gpusize            address)
{
    PreCall();
    CmdBufferFwdDecorator::CmdWriteImmediate(pipePoint, data, dataSize, address);
    PostCall(CmdBufCallId::CmdWriteImmediate);
}

// =====================================================================================================================
void CmdBuffer::CmdLoadBufferFilledSizes(
    const gpusize (&gpuVirtAddr)[MaxStreamOutTargets])
{
    PreCall();
    CmdBufferFwdDecorator::CmdLoadBufferFilledSizes(gpuVirtAddr);
    PostCall(CmdBufCallId::CmdLoadBufferFilledSizes);
}

// =====================================================================================================================
void CmdBuffer::CmdSaveBufferFilledSizes(
    const gpusize (&gpuVirtAddr)[MaxStreamOutTargets])
{
    PreCall();
    CmdBufferFwdDecorator::CmdSaveBufferFilledSizes(gpuVirtAddr);
    PostCall(CmdBufCallId::CmdSaveBufferFilledSizes);
}

// =====================================================================================================================
void CmdBuffer::CmdSetBufferFilledSize(
    uint32  bufferId,
    uint32  offset)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetBufferFilledSize(bufferId, offset);
    PostCall(CmdBufCallId::CmdSetBufferFilledSize);
}

// =====================================================================================================================
void CmdBuffer::CmdBindBorderColorPalette(
    PipelineBindPoint          pipelineBindPoint,
    const IBorderColorPalette* pPalette)
{
    PreCall();
    CmdBufferFwdDecorator::CmdBindBorderColorPalette(pipelineBindPoint, pPalette);
    PostCall(CmdBufCallId::CmdBindBorderColorPalette);
}

// =====================================================================================================================
void CmdBuffer::CmdSetPredication(
    IQueryPool*         pQueryPool,
    uint32              slot,
    const IGpuMemory*   pGpuMemory,
    gpusize             offset,
    PredicateType       predType,
    bool                predPolarity,
    bool                waitResults,
    bool                accumulateData)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetPredication(pQueryPool,
                                             slot,
                                             pGpuMemory,
                                             offset,
                                             predType,
                                             predPolarity,
                                             waitResults,
                                             accumulateData);
    PostCall(CmdBufCallId::CmdSetPredication);
}

// =====================================================================================================================
void CmdBuffer::CmdIf(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    PreCall();
    CmdBufferFwdDecorator::CmdIf(gpuMemory, offset, data, mask, compareFunc);
    PostCall(CmdBufCallId::CmdIf);
}

// =====================================================================================================================
void CmdBuffer::CmdElse()
{
    PreCall();
    CmdBufferFwdDecorator::CmdElse();
    PostCall(CmdBufCallId::CmdElse);
}

// =====================================================================================================================
void CmdBuffer::CmdEndIf()
{
    PreCall();
    CmdBufferFwdDecorator::CmdEndIf();
    PostCall(CmdBufCallId::CmdEndIf);
}

// =====================================================================================================================
void CmdBuffer::CmdWhile(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    PreCall();
    CmdBufferFwdDecorator::CmdWhile(gpuMemory, offset, data, mask, compareFunc);
    PostCall(CmdBufCallId::CmdWhile);
}

// =====================================================================================================================
void CmdBuffer::CmdEndWhile()
{
    PreCall();
    CmdBufferFwdDecorator::CmdEndWhile();
    PostCall(CmdBufCallId::CmdEndWhile);
}

// =====================================================================================================================
void CmdBuffer::CmdWaitRegisterValue(
    uint32      registerOffset,
    uint32      data,
    uint32      mask,
    CompareFunc compareFunc)
{
    PreCall();
    CmdBufferFwdDecorator::CmdWaitRegisterValue(registerOffset, data, mask, compareFunc);
    PostCall(CmdBufCallId::CmdWaitRegisterValue);
}

// =====================================================================================================================
void CmdBuffer::CmdWaitMemoryValue(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    PreCall();
    CmdBufferFwdDecorator::CmdWaitMemoryValue(gpuMemory, offset, data, mask, compareFunc);
    PostCall(CmdBufCallId::CmdWaitMemoryValue);
}

// =====================================================================================================================
void CmdBuffer::CmdWaitBusAddressableMemoryMarker(
    const IGpuMemory& gpuMemory,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    PreCall();
    CmdBufferFwdDecorator::CmdWaitBusAddressableMemoryMarker(gpuMemory, data, mask, compareFunc);
    PostCall(CmdBufCallId::CmdWaitBusAddressableMemoryMarker);
}

// =====================================================================================================================
void CmdBuffer::CmdBeginPerfExperiment(
    IPerfExperiment* pPerfExperiment)
{
    PreCall();
    CmdBufferFwdDecorator::CmdBeginPerfExperiment(pPerfExperiment);
    PostCall(CmdBufCallId::CmdBeginPerfExperiment);
}

// =====================================================================================================================
void CmdBuffer::CmdUpdatePerfExperimentSqttTokenMask(
    IPerfExperiment*              pPerfExperiment,
    const ThreadTraceTokenConfig& sqttTokenConfig)
{
    PreCall();
    CmdBufferFwdDecorator::CmdUpdatePerfExperimentSqttTokenMask(pPerfExperiment, sqttTokenConfig);
    PostCall(CmdBufCallId::CmdUpdatePerfExperimentSqttTokenMask);
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateSqttTokenMask(
    const ThreadTraceTokenConfig& sqttTokenConfig)
{
    PreCall();
    CmdBufferFwdDecorator::CmdUpdateSqttTokenMask(sqttTokenConfig);
    PostCall(CmdBufCallId::CmdUpdateSqttTokenMask);
}

// =====================================================================================================================
void CmdBuffer::CmdEndPerfExperiment(
    IPerfExperiment* pPerfExperiment)
{
    PreCall();
    CmdBufferFwdDecorator::CmdEndPerfExperiment(pPerfExperiment);
    PostCall(CmdBufCallId::CmdEndPerfExperiment);
}

// =====================================================================================================================
void CmdBuffer::CmdInsertTraceMarker(
    PerfTraceMarkerType markerType,
    uint32              markerData)
{
    PreCall();
    CmdBufferFwdDecorator::CmdInsertTraceMarker(markerType, markerData);
    PostCall(CmdBufCallId::CmdInsertTraceMarker);
}

// =====================================================================================================================
void CmdBuffer::CmdInsertRgpTraceMarker(
    uint32      numDwords,
    const void* pData)
{
    PreCall();
    CmdBufferFwdDecorator::CmdInsertRgpTraceMarker(numDwords, pData);
    PostCall(CmdBufCallId::CmdInsertRgpTraceMarker);
}

// =====================================================================================================================
void CmdBuffer::CmdLoadCeRam(
    const IGpuMemory& srcGpuMemory,
    gpusize           memOffset,
    uint32            ramOffset,
    uint32            dwordSize)
{
    PreCall();
    CmdBufferFwdDecorator::CmdLoadCeRam(srcGpuMemory, memOffset, ramOffset, dwordSize);
    PostCall(CmdBufCallId::CmdLoadCeRam);
}

// =====================================================================================================================
void CmdBuffer::CmdDumpCeRam(
    const IGpuMemory& dstGpuMemory,
    gpusize           memOffset,
    uint32            ramOffset,
    uint32            dwordSize,
    uint32            currRingPos,
    uint32            ringSize)
{
    PreCall();
    CmdBufferFwdDecorator::CmdDumpCeRam(dstGpuMemory,
                                        memOffset,
                                        ramOffset,
                                        dwordSize,
                                        currRingPos,
                                        ringSize);
    PostCall(CmdBufCallId::CmdDumpCeRam);
}

// =====================================================================================================================
void CmdBuffer::CmdWriteCeRam(
    const void* pSrcData,
    uint32      ramOffset,
    uint32      dwordSize)
{
    PreCall();
    CmdBufferFwdDecorator::CmdWriteCeRam(pSrcData, ramOffset, dwordSize);
    PostCall(CmdBufCallId::CmdWriteCeRam);
}

// =====================================================================================================================
void CmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32            cmdBufferCount,
    ICmdBuffer*const* ppCmdBuffers)
{
    PreCall();
    CmdBufferFwdDecorator::CmdExecuteNestedCmdBuffers(cmdBufferCount, ppCmdBuffers);
    PostCall(CmdBufCallId::CmdExecuteNestedCmdBuffers);
}

// =====================================================================================================================
void CmdBuffer::CmdSaveComputeState(
    uint32 stateFlags)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSaveComputeState(stateFlags);
    PostCall(CmdBufCallId::CmdSaveComputeState);
}

// =====================================================================================================================
void CmdBuffer::CmdRestoreComputeState(
    uint32 stateFlags)
{
    PreCall();
    CmdBufferFwdDecorator::CmdRestoreComputeState(stateFlags);
    PostCall(CmdBufCallId::CmdRestoreComputeState);
}

// =====================================================================================================================
void CmdBuffer::CmdExecuteIndirectCmds(
    const IIndirectCmdGenerator& generator,
    const IGpuMemory&            gpuMemory,
    gpusize                      offset,
    uint32                       maximumCount,
    gpusize                      countGpuAddr)
{
    PreCall();
    CmdBufferFwdDecorator::CmdExecuteIndirectCmds(generator, gpuMemory, offset, maximumCount, countGpuAddr);
    PostCall(CmdBufCallId::CmdExecuteIndirectCmds);
}

// =====================================================================================================================
void CmdBuffer::CmdPostProcessFrame(
    const CmdPostProcessFrameInfo& postProcessInfo,
    bool*                          pAddedGpuWork)
{
    PreCall();
    CmdBufferFwdDecorator::CmdPostProcessFrame(postProcessInfo, pAddedGpuWork);
    PostCall(CmdBufCallId::CmdPostProcessFrame);
}

// =====================================================================================================================
void CmdBuffer::CmdSetUserClipPlanes(
    uint32               firstPlane,
    uint32               planeCount,
    const UserClipPlane* pPlanes)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetUserClipPlanes(firstPlane, planeCount, pPlanes);
    PostCall(CmdBufCallId::CmdSetUserClipPlanes);
}

// =====================================================================================================================
void CmdBuffer::CmdSetClipRects(
    uint16      clipRule,
    uint32      rectCount,
    const Rect* pRectList)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetClipRects(clipRule, rectCount, pRectList);
    PostCall(CmdBufCallId::CmdSetClipRects);
}

// =====================================================================================================================
void CmdBuffer::CmdFlglSync()
{
    PreCall();
    CmdBufferFwdDecorator::CmdFlglSync();
    PostCall(CmdBufCallId::CmdFlglSync);
}

// =====================================================================================================================
void CmdBuffer::CmdFlglEnable()
{
    PreCall();
    CmdBufferFwdDecorator::CmdFlglEnable();
    PostCall(CmdBufCallId::CmdFlglEnable);
}

// =====================================================================================================================
void CmdBuffer::CmdFlglDisable()
{
    PreCall();
    CmdBufferFwdDecorator::CmdFlglDisable();
    PostCall(CmdBufCallId::CmdFlglDisable);
}

// =====================================================================================================================
void CmdBuffer::CmdXdmaWaitFlipPending()
{
    PreCall();
    CmdBufferFwdDecorator::CmdXdmaWaitFlipPending();
    PostCall(CmdBufCallId::CmdXdmaWaitFlipPending);
}

// =====================================================================================================================
void CmdBuffer::CmdStartGpuProfilerLogging()
{
    PreCall();
    CmdBufferFwdDecorator::CmdStartGpuProfilerLogging();
    PostCall(CmdBufCallId::CmdStartGpuProfilerLogging);
}

// =====================================================================================================================
void CmdBuffer::CmdStopGpuProfilerLogging()
{
    PreCall();
    CmdBufferFwdDecorator::CmdStopGpuProfilerLogging();
    PostCall(CmdBufCallId::CmdStopGpuProfilerLogging);
}

// =====================================================================================================================
void CmdBuffer::CmdSetViewInstanceMask(
    uint32 mask)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetViewInstanceMask(mask);
    PostCall(CmdBufCallId::CmdSetViewInstanceMask);
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 509
// =====================================================================================================================
void CmdBuffer::CmdSetHiSCompareState0(
    CompareFunc compFunc,
    uint32      compMask,
    uint32      compValue,
    bool        enable)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetHiSCompareState0(compFunc, compMask, compValue, enable);
    PostCall(CmdBufCallId::CmdSetHiSCompareState0);
}

// =====================================================================================================================
void CmdBuffer::CmdSetHiSCompareState1(
    CompareFunc compFunc,
    uint32      compMask,
    uint32      compValue,
    bool        enable)
{
    PreCall();
    CmdBufferFwdDecorator::CmdSetHiSCompareState1(compFunc, compMask, compValue, enable);
    PostCall(CmdBufCallId::CmdSetHiSCompareState1);
}
#endif

// =====================================================================================================================
void CmdBuffer::CmdUpdateHiSPretests(
    const IImage*      pImage,
    const HiSPretests& pretests,
    uint32             firstMip,
    uint32             numMips)
{
    PreCall();
    CmdBufferFwdDecorator::CmdUpdateHiSPretests(pImage, pretests, firstMip, numMips);
    PostCall(CmdBufCallId::CmdUpdateHiSPretests);
}

} // Pm4Instrumentor
} // Pal

#endif
