/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/gpuProfiler/gpuProfilerCmdBuffer.h"
#include "core/layers/gpuProfiler/gpuProfilerDevice.h"
#include "core/layers/gpuProfiler/gpuProfilerPlatform.h"
#include "core/layers/gpuProfiler/gpuProfilerQueue.h"
#include "palAutoBuffer.h"
#include "palGpaSession.h"

// These includes are required because we need the definition of the D3D12DDI_PRESENT_0003 struct in order to make a
// copy of the data in it for the tokenization.

using namespace Util;

namespace Pal
{
namespace GpuProfiler
{

// =====================================================================================================================
CmdBuffer::CmdBuffer(
    ICmdBuffer*                pNextCmdBuffer,
    Device*                    pDevice,
    const CmdBufferCreateInfo& createInfo,
    bool                       logPipeStats,
    bool                       enableSqThreadTrace)
    :
    CmdBufferDecorator(pNextCmdBuffer, pDevice),
    m_pDevice(pDevice),
    m_queueType(createInfo.queueType),
    m_engineType(createInfo.engineType),
#if (PAL_COMPILE_TYPE == 32)
    m_tokenAllocator(4 * 1024 * 1024),
#else
    m_tokenAllocator(16 * 1024 * 1024),
#endif
    m_pTokenStream(nullptr),
    m_pTokenRdPtr(nullptr),
    m_disableDataGathering(false),
    m_forceDrawGranularityLogging(false),
    m_curLogFrame(0)
{
    PAL_ASSERT(NextLayer() == pNextCmdBuffer);

    m_funcTable.pfnCmdSetUserData[static_cast<uint32>(PipelineBindPoint::Compute)]  = &CmdBuffer::CmdSetUserDataCs;
    m_funcTable.pfnCmdSetUserData[static_cast<uint32>(PipelineBindPoint::Graphics)] = &CmdBuffer::CmdSetUserDataGfx;

    m_funcTable.pfnCmdDraw                     = CmdDraw;
    m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque;
    m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed;
    m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti;
    m_funcTable.pfnCmdDrawIndexedIndirectMulti = CmdDrawIndexedIndirectMulti;
    m_funcTable.pfnCmdDispatch                 = CmdDispatch;
    m_funcTable.pfnCmdDispatchIndirect         = CmdDispatchIndirect;
    m_funcTable.pfnCmdDispatchOffset           = CmdDispatchOffset;

    memset(&m_flags,                0, sizeof(m_flags));
    memset(&m_computePipelineInfo,  0, sizeof(m_computePipelineInfo));
    memset(&m_graphicsPipelineInfo, 0, sizeof(m_graphicsPipelineInfo));
    memset(&m_cmdBufLogItem,        0, sizeof(m_cmdBufLogItem));
    memset(&m_loopLogItem,          0, sizeof(m_loopLogItem));

    m_flags.nested              = createInfo.flags.nested;
    m_flags.logPipeStats        = logPipeStats;
    m_flags.enableSqThreadTrace = enableSqThreadTrace;
}

// =====================================================================================================================
Result CmdBuffer::Init()
{
    Result result = m_tokenAllocator.Init();

    if (result == Result::Success)
    {
        m_pTokenStream = m_tokenAllocator.Current();
    }

    return result;
}

// =====================================================================================================================
Result CmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    m_flags.containsPresent = 0;

    // Rewind the allocator to the beginning, overwriting any tokens stored from the last time this command buffer was
    // recorded.
    m_tokenAllocator.Rewind(m_pTokenStream, false);

    InsertToken(CmdBufCallId::Begin);
    InsertToken(info);
    if (info.pInheritedState != nullptr)
    {
        InsertToken(*info.pInheritedState);
    }

    Pal::Result result = Pal::Result::Success;

    // Note that Begin() is immediately forwarded to the next layer.  This is only necessary in order to support clients
    // that use CmdAllocateEmbeddedData().  They immediately need a CPU address corresponding to GPU memory with the
    // lifetime of this command buffer, so it is easiest to just let it go through the normal path.  The core layer's
    // command buffer will be filled entirely with embedded data.
    //
    // This is skipped for command buffers based on VideoEncodeCmdBuffers because those command buffers do not
    // reset their state (or even really build the command buffer) until that command buffer is submitted.  The GPU
    // profiler layer instead internally replaces and submits a different command buffer which leaves this one
    // permanently in Building state the next time Begin() is called on it.
    {
        result = NextLayer()->Begin(NextCmdBufferBuildInfo(info));
    }

    return result;
}

// =====================================================================================================================
void CmdBuffer::ReplayBegin(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto info = ReadTokenVal<CmdBufferBuildInfo>();

    InheritedStateParams inheritedState = {};
    if (info.pInheritedState != nullptr)
    {
        inheritedState = ReadTokenVal<InheritedStateParams>();
        info.pInheritedState = &inheritedState;
    }
    // We must remove the client's external allocator because PAL can only use it during command building from the
    // client's perspective. By batching and replaying command building later on we're breaking that rule. The good news
    // is that we can replace it with our queue's command buffer replay allocator because replaying is thread-safe with
    // respect to each queue.
    info.pMemAllocator = pQueue->ReplayAllocator();

    pTgtCmdBuffer->Begin(NextCmdBufferBuildInfo(info));

    // Reset any per command buffer state we're tracking.
    memset(&m_computePipelineInfo,  0, sizeof(m_computePipelineInfo));
    memset(&m_graphicsPipelineInfo, 0, sizeof(m_graphicsPipelineInfo));

    if (m_pDevice->LoggingEnabled(GpuProfilerGranularityDraw) ||
        m_pDevice->LoggingEnabled(GpuProfilerGranularityCmdBuf))
    {
        memset(&m_cmdBufLogItem, 0, sizeof(m_cmdBufLogItem));
        m_cmdBufLogItem.type              = CmdBufferCall;
        m_cmdBufLogItem.frameId           = m_curLogFrame;
        m_cmdBufLogItem.cmdBufCall.callId = CmdBufCallId::Begin;

        // Begin a GPA session.
        pTgtCmdBuffer->BeginGpaSession(pQueue);

        if (m_flags.nested == false)
        {
            bool enablePerfExp   = false;
            bool enablePipeStats = false;

            if (m_pDevice->LoggingEnabled(GpuProfilerGranularityCmdBuf))
            {
                enablePerfExp    = (m_pDevice->NumGlobalPerfCounters() > 0)    ||
                                   (m_pDevice->NumStreamingPerfCounters() > 0) ||
                                   (m_flags.enableSqThreadTrace != 0);
                enablePipeStats  = m_flags.logPipeStats;
            }

            pTgtCmdBuffer->BeginSample(pQueue, &m_cmdBufLogItem, enablePipeStats, enablePerfExp);
        }
        else
        {
            m_cmdBufLogItem.pGpaSession = pTgtCmdBuffer->GetGpaSession();
        }
        pQueue->AddLogItem(m_cmdBufLogItem);
    }
}

// =====================================================================================================================
Result CmdBuffer::End()
{
    InsertToken(CmdBufCallId::End);

    // See CmdBuffer::Begin() for comment on why Begin()/End() are immediately passed to the next layer.
    return NextLayer()->End();
}

// =====================================================================================================================
void CmdBuffer::ReplayEnd(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    if (m_pDevice->LoggingEnabled(GpuProfilerGranularityDraw) ||
        m_pDevice->LoggingEnabled(GpuProfilerGranularityCmdBuf))
    {
        if (m_flags.nested == false)
        {
            pTgtCmdBuffer->EndSample(pQueue, &m_cmdBufLogItem);
        }
        pTgtCmdBuffer->EndGpaSession(&m_cmdBufLogItem);

        LogItem logItem = { };
        logItem.type              = CmdBufferCall;
        logItem.frameId           = m_curLogFrame;
        logItem.cmdBufCall.callId = CmdBufCallId::End;
        pQueue->AddLogItem(logItem);
    }

    pTgtCmdBuffer->End();
}

// =====================================================================================================================
Result CmdBuffer::Reset(
    ICmdAllocator* pCmdAllocator,
    bool           returnGpuMemory)
{
    return NextLayer()->Reset(NextCmdAllocator(pCmdAllocator), returnGpuMemory);
}

// =====================================================================================================================
void CmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    InsertToken(CmdBufCallId::CmdBindPipeline);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindPipeline(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const PipelineBindParams params            = ReadTokenVal<PipelineBindParams>();
    const PipelineBindPoint  pipelineBindPoint = params.pipelineBindPoint;
    const IPipeline*         pPipeline         = params.pPipeline;

    // Update currently bound pipeline and shader hashes.
    if (pipelineBindPoint == PipelineBindPoint::Compute)
    {
        if (pPipeline != nullptr)
        {
            m_computePipelineInfo = pPipeline->GetInfo();
        }
        else
        {
            memset(&m_computePipelineInfo, 0, sizeof(m_computePipelineInfo));
        }
    }
    else
    {
        PAL_ASSERT(pipelineBindPoint == PipelineBindPoint::Graphics);

        if (pPipeline != nullptr)
        {
            m_graphicsPipelineInfo = pPipeline->GetInfo();
        }
        else
        {
            memset(&m_graphicsPipelineInfo, 0, sizeof(m_graphicsPipelineInfo));
        }
    }

    pTgtCmdBuffer->CmdBindPipeline(params);

    if (m_pDevice->LoggingEnabled(GpuProfilerGranularity::GpuProfilerGranularityFrame))
    {
        GpuUtil::GpaSession* pGpaSession = pQueue->GetPerFrameGpaSession();

        if (pGpaSession != nullptr)
        {
            pGpaSession->RegisterPipeline(pPipeline);
        }
    }
}

// =====================================================================================================================
void CmdBuffer::CmdBindMsaaState(
    const IMsaaState* pMsaaState)
{
    InsertToken(CmdBufCallId::CmdBindMsaaState);
    InsertToken(pMsaaState);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindMsaaState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdBindMsaaState(ReadTokenVal<IMsaaState*>());
}

// =====================================================================================================================
void CmdBuffer::CmdBindColorBlendState(
    const IColorBlendState* pColorBlendState)
{
    InsertToken(CmdBufCallId::CmdBindColorBlendState);
    InsertToken(pColorBlendState);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindColorBlendState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdBindColorBlendState(ReadTokenVal<IColorBlendState*>());
}

// =====================================================================================================================
void CmdBuffer::CmdBindDepthStencilState(
    const IDepthStencilState* pDepthStencilState)
{
    InsertToken(CmdBufCallId::CmdBindDepthStencilState);
    InsertToken(pDepthStencilState);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindDepthStencilState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdBindDepthStencilState(ReadTokenVal<IDepthStencilState*>());
}

// =====================================================================================================================
void CmdBuffer::CmdBindIndexData(
    gpusize   gpuAddr,
    uint32    indexCount,
    IndexType indexType)
{
    InsertToken(CmdBufCallId::CmdBindIndexData);
    InsertToken(gpuAddr);
    InsertToken(indexCount);
    InsertToken(indexType);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindIndexData(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto gpuAddr    = ReadTokenVal<gpusize>();
    auto indexCount = ReadTokenVal<uint32>();
    auto indexType  = ReadTokenVal<IndexType>();

    pTgtCmdBuffer->CmdBindIndexData(gpuAddr, indexCount, indexType);
}

// =====================================================================================================================
void CmdBuffer::CmdBindTargets(
    const BindTargetParams& params)
{
    InsertToken(CmdBufCallId::CmdBindTargets);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindTargets(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdBindTargets(ReadTokenVal<BindTargetParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdBindStreamOutTargets(
    const BindStreamOutTargetParams& params)
{
    InsertToken(CmdBufCallId::CmdBindStreamOutTargets);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindStreamOutTargets(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdBindStreamOutTargets(ReadTokenVal<BindStreamOutTargetParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdBindBorderColorPalette(
    PipelineBindPoint          pipelineBindPoint,
    const IBorderColorPalette* pPalette)
{
    InsertToken(CmdBufCallId::CmdBindBorderColorPalette);
    InsertToken(pipelineBindPoint);
    InsertToken(pPalette);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindBorderColorPalette(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pipelineBindPoint = ReadTokenVal<PipelineBindPoint>();
    auto pPalette          = ReadTokenVal<IBorderColorPalette*>();

    pTgtCmdBuffer->CmdBindBorderColorPalette(pipelineBindPoint, pPalette);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdSetUserDataCs(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    auto* pCmdBuf = static_cast<CmdBuffer*>(pCmdBuffer);

    pCmdBuf->InsertToken(CmdBufCallId::CmdSetUserData);
    pCmdBuf->InsertToken(PipelineBindPoint::Compute);
    pCmdBuf->InsertToken(firstEntry);
    pCmdBuf->InsertTokenArray(pEntryValues, entryCount);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdSetUserDataGfx(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    auto* pCmdBuf = static_cast<CmdBuffer*>(pCmdBuffer);

    pCmdBuf->InsertToken(CmdBufCallId::CmdSetUserData);
    pCmdBuf->InsertToken(PipelineBindPoint::Graphics);
    pCmdBuf->InsertToken(firstEntry);
    pCmdBuf->InsertTokenArray(pEntryValues, entryCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetUserData(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto          pipelineBindPoint = ReadTokenVal<PipelineBindPoint>();
    auto          firstEntry        = ReadTokenVal<uint32>();
    const uint32* pEntryValues      = nullptr;
    auto          entryCount        = ReadTokenArray(&pEntryValues);

    pTgtCmdBuffer->CmdSetUserData(pipelineBindPoint, firstEntry, entryCount, pEntryValues);
}

// =====================================================================================================================
void CmdBuffer::CmdSetIndirectUserData(
    uint16      tableId,
    uint32      dwordOffset,
    uint32      dwordSize,
    const void* pSrcData)
{
    InsertToken(CmdBufCallId::CmdSetIndirectUserData);
    InsertToken(tableId);
    InsertToken(dwordOffset);
    InsertTokenArray(static_cast<const uint32*>(pSrcData), dwordSize);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetIndirectUserData(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto          tableId     = ReadTokenVal<uint16>();
    auto          dwordOffset = ReadTokenVal<uint32>();
    const uint32* pSrcData    = nullptr;
    auto          dwordSize   = ReadTokenArray(&pSrcData);

    pTgtCmdBuffer->CmdSetIndirectUserData(tableId, dwordOffset, dwordSize, pSrcData);
}

// =====================================================================================================================
void CmdBuffer::CmdSetIndirectUserDataWatermark(
    uint16 tableId,
    uint32 dwordLimit)
{
    InsertToken(CmdBufCallId::CmdSetIndirectUserDataWatermark);
    InsertToken(tableId);
    InsertToken(dwordLimit);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetIndirectUserDataWatermark(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto tableId    = ReadTokenVal<uint16>();
    auto dwordLimit = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdSetIndirectUserDataWatermark(tableId, dwordLimit);
}

// =====================================================================================================================
void CmdBuffer::CmdSetBlendConst(
    const BlendConstParams& params)
{
    InsertToken(CmdBufCallId::CmdSetBlendConst);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetBlendConst(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetBlendConst(ReadTokenVal<BlendConstParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetInputAssemblyState(
    const InputAssemblyStateParams& params)
{
    InsertToken(CmdBufCallId::CmdSetInputAssemblyState);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetInputAssemblyState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetInputAssemblyState(ReadTokenVal<InputAssemblyStateParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetTriangleRasterState(
    const TriangleRasterStateParams& params)
{
    InsertToken(CmdBufCallId::CmdSetTriangleRasterState);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetTriangleRasterState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetTriangleRasterState(ReadTokenVal<TriangleRasterStateParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetPointLineRasterState(
    const PointLineRasterStateParams& params)
{
    InsertToken(CmdBufCallId::CmdSetPointLineRasterState);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetPointLineRasterState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetPointLineRasterState(ReadTokenVal<PointLineRasterStateParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetDepthBiasState(
    const DepthBiasParams& params)
{
    InsertToken(CmdBufCallId::CmdSetDepthBiasState);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetDepthBiasState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetDepthBiasState(ReadTokenVal<DepthBiasParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetDepthBounds(
    const DepthBoundsParams& params)
{
    InsertToken(CmdBufCallId::CmdSetDepthBounds);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetDepthBounds(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetDepthBounds(ReadTokenVal<DepthBoundsParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetStencilRefMasks(
    const StencilRefMaskParams& params)
{
    InsertToken(CmdBufCallId::CmdSetStencilRefMasks);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetStencilRefMasks(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetStencilRefMasks(ReadTokenVal<StencilRefMaskParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetMsaaQuadSamplePattern(
    uint32                       numSamplesPerPixel,
    const MsaaQuadSamplePattern& quadSamplePattern)
{
    InsertToken(CmdBufCallId::CmdSetMsaaQuadSamplePattern);
    InsertToken(numSamplesPerPixel);
    InsertToken(quadSamplePattern);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetMsaaQuadSamplePattern(
    Queue*            pQueue,
    TargetCmdBuffer*  pTgtCmdBuffer)
{
    auto numSamplesPerPixel = ReadTokenVal<uint32>();
    auto quadSamplePattern  = ReadTokenVal<MsaaQuadSamplePattern>();

    pTgtCmdBuffer->CmdSetMsaaQuadSamplePattern(numSamplesPerPixel, quadSamplePattern);
}

// =====================================================================================================================
void CmdBuffer::CmdSetViewports(
    const ViewportParams& params)
{
    InsertToken(CmdBufCallId::CmdSetViewports);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetViewports(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetViewports(ReadTokenVal<ViewportParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetScissorRects(
    const ScissorRectParams& params)
{
    InsertToken(CmdBufCallId::CmdSetScissorRects);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetScissorRects(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetScissorRects(ReadTokenVal<ScissorRectParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetGlobalScissor(
    const GlobalScissorParams& params)
{
    InsertToken(CmdBufCallId::CmdSetGlobalScissor);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetGlobalScissor(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetGlobalScissor(ReadTokenVal<GlobalScissorParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdBarrier(
    const BarrierInfo& barrierInfo)
{
    InsertToken(CmdBufCallId::CmdBarrier);
    InsertToken(barrierInfo.flags);

    InsertToken(barrierInfo.waitPoint);
    InsertTokenArray(barrierInfo.pPipePoints, barrierInfo.pipePointWaitCount);
    InsertTokenArray(barrierInfo.ppGpuEvents, barrierInfo.gpuEventWaitCount);
    InsertTokenArray(barrierInfo.ppTargets, barrierInfo.rangeCheckedTargetWaitCount);
    InsertTokenArray(barrierInfo.pTransitions, barrierInfo.transitionCount);
    InsertToken(barrierInfo.pSplitBarrierGpuEvent);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBarrier(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    BarrierInfo barrierInfo;

    barrierInfo.flags                       = ReadTokenVal<BarrierFlags>();
    barrierInfo.waitPoint                   = ReadTokenVal<HwPipePoint>();
    barrierInfo.pipePointWaitCount          = ReadTokenArray(&barrierInfo.pPipePoints);
    barrierInfo.gpuEventWaitCount           = ReadTokenArray(&barrierInfo.ppGpuEvents);
    barrierInfo.rangeCheckedTargetWaitCount = ReadTokenArray(&barrierInfo.ppTargets);
    barrierInfo.transitionCount             = ReadTokenArray(&barrierInfo.pTransitions);
    barrierInfo.pSplitBarrierGpuEvent       = ReadTokenVal<const IGpuEvent*>();

    pTgtCmdBuffer->ResetBarrierString();

    // We can only log the parameters of one transition at a time.
    // TODO: Expand batched barrier calls into calls with one transition each when the profiler is enabled so we
    // can log the parameters of each individual transition.
    LogItem logItem = { };
    logItem.cmdBufCall.flags.barrier    = 1;
    logItem.cmdBufCall.barrier.pComment = nullptr;
    for (uint32 i = 0; i < barrierInfo.transitionCount; i++)
    {
        const BarrierTransition& transition = barrierInfo.pTransitions[i];

        char commentString[MaxCommentLength] = {};
        Snprintf(&commentString[0], MaxCommentLength,
                 "SrcCacheMask: 0x%08x\n"
                 "DstCacheMask: 0x%08x\n"
                 "OldLayout: 0x%08x\n"
                 "NewLayout: 0x%08x",
                 transition.srcCacheMask, transition.dstCacheMask,
                 *reinterpret_cast<const uint32*>(&transition.imageInfo.oldLayout),
                 *reinterpret_cast<const uint32*>(&transition.imageInfo.newLayout));
        pTgtCmdBuffer->AddBarrierString(&commentString[0]);
    }

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdBarrier);

    pTgtCmdBuffer->CmdBarrier(barrierInfo);

    logItem.cmdBufCall.barrier.pComment = pTgtCmdBuffer->GetBarrierString();
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdWaitRegisterValue(
    uint32      registerOffset,
    uint32      data,
    uint32      mask,
    CompareFunc compareFunc)
{
    InsertToken(CmdBufCallId::CmdWaitRegisterValue);
    InsertToken(registerOffset);
    InsertToken(data);
    InsertToken(mask);
    InsertToken(compareFunc);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdWaitRegisterValue(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto registerOffset = ReadTokenVal<uint32>();
    auto data           = ReadTokenVal<uint32>();
    auto mask           = ReadTokenVal<uint32>();
    auto compareFunc    = ReadTokenVal<CompareFunc>();

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdWaitRegisterValue);
    pTgtCmdBuffer->CmdWaitRegisterValue(registerOffset, data, mask, compareFunc);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdWaitMemoryValue(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    InsertToken(CmdBufCallId::CmdWaitMemoryValue);
    InsertToken(&gpuMemory);
    InsertToken(offset);
    InsertToken(data);
    InsertToken(mask);
    InsertToken(compareFunc);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdWaitMemoryValue(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuMemory  = ReadTokenVal<IGpuMemory*>();
    auto offset      = ReadTokenVal<gpusize>();
    auto data        = ReadTokenVal<uint32>();
    auto mask        = ReadTokenVal<uint32>();
    auto compareFunc = ReadTokenVal<CompareFunc>();

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdWaitMemoryValue);
    pTgtCmdBuffer->CmdWaitMemoryValue(*pGpuMemory, offset, data, mask, compareFunc);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdWaitBusAddressableMemoryMarker(
    const IGpuMemory& gpuMemory,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    InsertToken(CmdBufCallId::CmdWaitBusAddressableMemoryMarker);
    InsertToken(&gpuMemory);
    InsertToken(data);
    InsertToken(mask);
    InsertToken(compareFunc);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdWaitBusAddressableMemoryMarker(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto data = ReadTokenVal<uint32>();
    auto mask = ReadTokenVal<uint32>();
    auto compareFunc = ReadTokenVal<CompareFunc>();

    LogItem logItem = {};

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdWaitBusAddressableMemoryMarker);
    pTgtCmdBuffer->CmdWaitBusAddressableMemoryMarker(*pGpuMemory, data, mask, compareFunc);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDraw(
    ICmdBuffer* pCmdBuffer,
    uint32      firstVertex,
    uint32      vertexCount,
    uint32      firstInstance,
    uint32      instanceCount)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertToken(CmdBufCallId::CmdDraw);
    pThis->InsertToken(firstVertex);
    pThis->InsertToken(vertexCount);
    pThis->InsertToken(firstInstance);
    pThis->InsertToken(instanceCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDraw(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto firstVertex   = ReadTokenVal<uint32>();
    auto vertexCount   = ReadTokenVal<uint32>();
    auto firstInstance = ReadTokenVal<uint32>();
    auto instanceCount = ReadTokenVal<uint32>();

    LogItem logItem = { };
    logItem.cmdBufCall.flags.draw         = 1;
    logItem.cmdBufCall.draw.vertexCount   = vertexCount;
    logItem.cmdBufCall.draw.instanceCount = instanceCount;

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdDraw);
    pTgtCmdBuffer->CmdDraw(firstVertex, vertexCount, firstInstance, instanceCount);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawOpaque(
    ICmdBuffer* pCmdBuffer,
    gpusize     streamOutFilledSizeVa,
    uint32      streamOutOffset,
    uint32      stride)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertToken(CmdBufCallId::CmdDrawOpaque);
    pThis->InsertToken(streamOutFilledSizeVa);
    pThis->InsertToken(streamOutOffset);
    pThis->InsertToken(stride);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDrawOpaque(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto streamOutFilledSizeVa = ReadTokenVal<gpusize>();
    auto streamOutOffset       = ReadTokenVal<uint32>();
    auto stride                = ReadTokenVal<uint32>();

    LogItem logItem = { };
    logItem.cmdBufCall.flags.draw         = 1;
    logItem.cmdBufCall.draw.vertexCount   = 0;
    logItem.cmdBufCall.draw.instanceCount = 1;

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdDraw);
    pTgtCmdBuffer->CmdDrawOpaque(streamOutFilledSizeVa, streamOutOffset, stride);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndexed(
    ICmdBuffer* pCmdBuffer,
    uint32      firstIndex,
    uint32      indexCount,
    int32       vertexOffset,
    uint32      firstInstance,
    uint32      instanceCount)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertToken(CmdBufCallId::CmdDrawIndexed);
    pThis->InsertToken(firstIndex);
    pThis->InsertToken(indexCount);
    pThis->InsertToken(vertexOffset);
    pThis->InsertToken(firstInstance);
    pThis->InsertToken(instanceCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDrawIndexed(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto firstIndex    = ReadTokenVal<uint32>();
    auto indexCount    = ReadTokenVal<uint32>();
    auto vertexOffset  = ReadTokenVal<int32>();
    auto firstInstance = ReadTokenVal<uint32>();
    auto instanceCount = ReadTokenVal<uint32>();

    LogItem logItem = { };
    logItem.cmdBufCall.flags.draw         = 1;
    logItem.cmdBufCall.draw.vertexCount   = indexCount;
    logItem.cmdBufCall.draw.instanceCount = instanceCount;

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdDrawIndexed);
    pTgtCmdBuffer->CmdDrawIndexed(firstIndex, indexCount, vertexOffset, firstInstance, instanceCount);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndirectMulti(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertToken(CmdBufCallId::CmdDrawIndirectMulti);
    pThis->InsertToken(&gpuMemory);
    pThis->InsertToken(offset);
    pThis->InsertToken(stride);
    pThis->InsertToken(maximumCount);
    pThis->InsertToken(countGpuAddr);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDrawIndirectMulti(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuMemory   = ReadTokenVal<IGpuMemory*>();
    auto offset       = ReadTokenVal<gpusize>();
    auto stride       = ReadTokenVal<uint32>();
    auto maximumCount = ReadTokenVal<uint32>();
    auto countGpuAddr = ReadTokenVal<gpusize>();

    LogItem logItem = { };
    logItem.cmdBufCall.flags.draw = 1;

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdDrawIndirectMulti);
    pTgtCmdBuffer->CmdDrawIndirectMulti(*pGpuMemory, offset, stride, maximumCount, countGpuAddr);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndexedIndirectMulti(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertToken(CmdBufCallId::CmdDrawIndexedIndirectMulti);
    pThis->InsertToken(&gpuMemory);
    pThis->InsertToken(offset);
    pThis->InsertToken(stride);
    pThis->InsertToken(maximumCount);
    pThis->InsertToken(countGpuAddr);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDrawIndexedIndirectMulti(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuMemory   = ReadTokenVal<IGpuMemory*>();
    auto offset       = ReadTokenVal<gpusize>();
    auto stride       = ReadTokenVal<uint32>();
    auto maximumCount = ReadTokenVal<uint32>();
    auto countGpuAddr = ReadTokenVal<gpusize>();

    LogItem logItem = { };
    logItem.cmdBufCall.flags.draw = 1;

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdDrawIndexedIndirectMulti);
    pTgtCmdBuffer->CmdDrawIndexedIndirectMulti(*pGpuMemory, offset, stride, maximumCount, countGpuAddr);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatch(
    ICmdBuffer* pCmdBuffer,
    uint32      x,
    uint32      y,
    uint32      z)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertToken(CmdBufCallId::CmdDispatch);
    pThis->InsertToken(x);
    pThis->InsertToken(y);
    pThis->InsertToken(z);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDispatch(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto x = ReadTokenVal<uint32>();
    auto y = ReadTokenVal<uint32>();
    auto z = ReadTokenVal<uint32>();

    LogItem logItem = { };
    logItem.cmdBufCall.flags.dispatch            = 1;
    logItem.cmdBufCall.dispatch.threadGroupCount = x * y * z;

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdDispatch);
    pTgtCmdBuffer->CmdDispatch(x, y, z);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchIndirect(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertToken(CmdBufCallId::CmdDispatchIndirect);
    pThis->InsertToken(&gpuMemory);
    pThis->InsertToken(offset);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDispatchIndirect(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto offset     = ReadTokenVal<gpusize>();

    LogItem logItem = { };
    logItem.cmdBufCall.flags.dispatch = 1;

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdDispatchIndirect);
    pTgtCmdBuffer->CmdDispatchIndirect(*pGpuMemory, offset);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchOffset(
    ICmdBuffer* pCmdBuffer,
    uint32      xOffset,
    uint32      yOffset,
    uint32      zOffset,
    uint32      xDim,
    uint32      yDim,
    uint32      zDim)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertToken(CmdBufCallId::CmdDispatchOffset);
    pThis->InsertToken(xOffset);
    pThis->InsertToken(yOffset);
    pThis->InsertToken(zOffset);
    pThis->InsertToken(xDim);
    pThis->InsertToken(yDim);
    pThis->InsertToken(zDim);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDispatchOffset(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto xOffset = ReadTokenVal<uint32>();
    auto yOffset = ReadTokenVal<uint32>();
    auto zOffset = ReadTokenVal<uint32>();
    auto xDim    = ReadTokenVal<uint32>();
    auto yDim    = ReadTokenVal<uint32>();
    auto zDim    = ReadTokenVal<uint32>();

    LogItem logItem = { };
    logItem.cmdBufCall.flags.dispatch            = 1;
    logItem.cmdBufCall.dispatch.threadGroupCount = xDim * yDim * zDim;

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdDispatchOffset);
    pTgtCmdBuffer->CmdDispatchOffset(xOffset, yOffset, zOffset, xDim, yDim, zDim);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           dataSize,
    const uint32*     pData)
{
    InsertToken(CmdBufCallId::CmdUpdateMemory);
    InsertToken(&dstGpuMemory);
    InsertToken(dstOffset);
    InsertTokenArray(pData, static_cast<uint32>(dataSize / sizeof(uint32)));
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdUpdateMemory(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto          pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto          dstOffset     = ReadTokenVal<gpusize>();
    const uint32* pData         = nullptr;
    auto          dataSize      = ReadTokenArray(&pData) * sizeof(uint32);

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdUpdateMemory);
    pTgtCmdBuffer->CmdUpdateMemory(*pDstGpuMemory, dstOffset, dataSize, pData);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateBusAddressableMemoryMarker(
    const IGpuMemory& dstGpuMemory,
    gpusize           offset,
    uint32            value)
{
    InsertToken(CmdBufCallId::CmdUpdateBusAddressableMemoryMarker);
    InsertToken(&dstGpuMemory);
    InsertToken(offset);
    InsertToken(value);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdUpdateBusAddressableMemoryMarker(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto          pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto          offset = ReadTokenVal<uint32>();
    auto          value = ReadTokenVal<uint32>();

    LogItem logItem = {};

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdUpdateBusAddressableMemoryMarker);
    pTgtCmdBuffer->CmdUpdateBusAddressableMemoryMarker(*pDstGpuMemory, offset, value);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdFillMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           fillSize,
    uint32            data)
{
    InsertToken(CmdBufCallId::CmdFillMemory);
    InsertToken(&dstGpuMemory);
    InsertToken(dstOffset);
    InsertToken(fillSize);
    InsertToken(data);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdFillMemory(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto dstOffset     = ReadTokenVal<gpusize>();
    auto fillSize      = ReadTokenVal<gpusize>();
    auto data          = ReadTokenVal<uint32>();

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdFillMemory);
    pTgtCmdBuffer->CmdFillMemory(*pDstGpuMemory, dstOffset, fillSize, data);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyMemory(
    const IGpuMemory&       srcGpuMemory,
    const IGpuMemory&       dstGpuMemory,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions)
{
    InsertToken(CmdBufCallId::CmdCopyMemory);
    InsertToken(&srcGpuMemory);
    InsertToken(&dstGpuMemory);
    InsertTokenArray(pRegions, regionCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyMemory(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                    pSrcGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto                    pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    const MemoryCopyRegion* pRegions      = nullptr;
    auto                    regionCount   = ReadTokenArray(&pRegions);

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdCopyMemory);
    pTgtCmdBuffer->CmdCopyMemory(*pSrcGpuMemory, *pDstGpuMemory, regionCount, pRegions);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyTypedBuffer(
    const IGpuMemory&            srcGpuMemory,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const TypedBufferCopyRegion* pRegions)
{
    InsertToken(CmdBufCallId::CmdCopyTypedBuffer);
    InsertToken(&srcGpuMemory);
    InsertToken(&dstGpuMemory);
    InsertTokenArray(pRegions, regionCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyTypedBuffer(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                         pSrcGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto                         pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    const TypedBufferCopyRegion* pRegions      = nullptr;
    auto                         regionCount   = ReadTokenArray(&pRegions);

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdCopyTypedBuffer);
    pTgtCmdBuffer->CmdCopyTypedBuffer(*pSrcGpuMemory, *pDstGpuMemory, regionCount, pRegions);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyRegisterToMemory(
    uint32            srcRegisterOffset,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    InsertToken(CmdBufCallId::CmdCopyRegisterToMemory);
    InsertToken(srcRegisterOffset);
    InsertToken(&dstGpuMemory);
    InsertToken(dstOffset);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyRegisterToMemory(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto srcRegisterOffset = ReadTokenVal<uint32>();
    auto pDstGpuMemory     = ReadTokenVal<IGpuMemory*>();
    auto dstOffset         = ReadTokenVal<gpusize>();

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdCopyRegisterToMemory);
    pTgtCmdBuffer->CmdCopyRegisterToMemory(srcRegisterOffset, *pDstGpuMemory, dstOffset);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
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
    InsertToken(CmdBufCallId::CmdCopyImage);
    InsertToken(&srcImage);
    InsertToken(srcImageLayout);
    InsertToken(&dstImage);
    InsertToken(dstImageLayout);
    InsertTokenArray(pRegions, regionCount);
    InsertToken(flags);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                   pSrcImage      = ReadTokenVal<IImage*>();
    auto                   srcImageLayout = ReadTokenVal<ImageLayout>();
    auto                   pDstImage      = ReadTokenVal<IImage*>();
    auto                   dstImageLayout = ReadTokenVal<ImageLayout>();
    const ImageCopyRegion* pRegions       = nullptr;
    auto                   regionCount    = ReadTokenArray(&pRegions);
    auto                   flags          = ReadTokenVal<uint32>();

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdCopyImage);
    pTgtCmdBuffer->CmdCopyImage(*pSrcImage, srcImageLayout, *pDstImage, dstImageLayout, regionCount, pRegions, flags);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdScaledCopyImage(
    const ScaledCopyInfo&        copyInfo)
{
    InsertToken(CmdBufCallId::CmdScaledCopyImage);
    InsertToken(copyInfo.pSrcImage);
    InsertToken(copyInfo.srcImageLayout);
    InsertToken(copyInfo.pDstImage);
    InsertToken(copyInfo.dstImageLayout);
    InsertTokenArray(copyInfo.pRegions, copyInfo.regionCount);
    InsertToken(copyInfo.filter);
    InsertToken(copyInfo.rotation);
    InsertToken(copyInfo.flags);
    if (copyInfo.flags.srcColorKey || copyInfo.flags.dstColorKey)
    {
        InsertTokenArray(copyInfo.pColorKey,1);
    }
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdScaledCopyImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    ScaledCopyInfo copyInfo = {};

    copyInfo.pSrcImage      = ReadTokenVal<IImage*>();
    copyInfo.srcImageLayout = ReadTokenVal<ImageLayout>();
    copyInfo.pDstImage      = ReadTokenVal<IImage*>();
    copyInfo.dstImageLayout = ReadTokenVal<ImageLayout>();
    copyInfo.regionCount    = ReadTokenArray(&copyInfo.pRegions);
    copyInfo.filter         = ReadTokenVal<TexFilter>();
    copyInfo.rotation       = ReadTokenVal<ImageRotation>();
    copyInfo.flags          = ReadTokenVal<ScaledCopyFlags>();
    if (copyInfo.flags.srcColorKey || copyInfo.flags.dstColorKey)
    {
        ReadTokenArray(&copyInfo.pColorKey);
    }
    else
    {
        copyInfo.pColorKey = nullptr;
    }

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdScaledCopyImage);
    pTgtCmdBuffer->CmdScaledCopyImage(copyInfo);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
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
    InsertToken(CmdBufCallId::CmdColorSpaceConversionCopy);
    InsertToken(&srcImage);
    InsertToken(srcImageLayout);
    InsertToken(&dstImage);
    InsertToken(dstImageLayout);
    InsertTokenArray(pRegions, regionCount);
    InsertToken(filter);
    InsertToken(cscTable);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdColorSpaceConversionCopy(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                              pSrcImage      = ReadTokenVal<IImage*>();
    auto                              srcImageLayout = ReadTokenVal<ImageLayout>();
    auto                              pDstImage      = ReadTokenVal<IImage*>();
    auto                              dstImageLayout = ReadTokenVal<ImageLayout>();
    const ColorSpaceConversionRegion* pRegions       = nullptr;
    auto                              regionCount    = ReadTokenArray(&pRegions);
    auto                              filter         = ReadTokenVal<TexFilter>();
    auto                              cscTable       = ReadTokenVal<ColorSpaceConversionTable>();

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdColorSpaceConversionCopy);
    pTgtCmdBuffer->CmdColorSpaceConversionCopy(*pSrcImage,
                                               srcImageLayout,
                                               *pDstImage,
                                               dstImageLayout,
                                               regionCount,
                                               pRegions,
                                               filter,
                                               cscTable);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdCloneImageData(
    const IImage& srcImage,
    const IImage& dstImage)
{
    InsertToken(CmdBufCallId::CmdCloneImageData);
    InsertToken(&srcImage);
    InsertToken(&dstImage);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCloneImageData(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pSrcImage = ReadTokenVal<IImage*>();
    auto pDstImage = ReadTokenVal<IImage*>();

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdCloneImageData);
    pTgtCmdBuffer->CmdCloneImageData(*pSrcImage, *pDstImage);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyMemoryToImage(
    const IGpuMemory&            srcGpuMemory,
    const IImage&                dstImage,
    ImageLayout                  dstImageLayout,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions)
{
    InsertToken(CmdBufCallId::CmdCopyMemoryToImage);
    InsertToken(&srcGpuMemory);
    InsertToken(&dstImage);
    InsertToken(dstImageLayout);
    InsertTokenArray(pRegions, regionCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyMemoryToImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                         pSrcGpuMemory  = ReadTokenVal<IGpuMemory*>();
    auto                         pDstImage      = ReadTokenVal<IImage*>();
    auto                         dstImageLayout = ReadTokenVal<ImageLayout>();
    const MemoryImageCopyRegion* pRegions       = nullptr;
    auto                         regionCount    = ReadTokenArray(&pRegions);

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdCopyMemoryToImage);
    pTgtCmdBuffer->CmdCopyMemoryToImage(*pSrcGpuMemory, *pDstImage, dstImageLayout, regionCount, pRegions);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyImageToMemory(
    const IImage&                srcImage,
    ImageLayout                  srcImageLayout,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions)
{
    InsertToken(CmdBufCallId::CmdCopyImageToMemory);
    InsertToken(&srcImage);
    InsertToken(srcImageLayout);
    InsertToken(&dstGpuMemory);
    InsertTokenArray(pRegions, regionCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyImageToMemory(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                         pSrcImage      = ReadTokenVal<IImage*>();
    auto                         srcImageLayout = ReadTokenVal<ImageLayout>();
    auto                         pDstGpuMemory  = ReadTokenVal<IGpuMemory*>();
    const MemoryImageCopyRegion* pRegions       = nullptr;
    auto                         regionCount    = ReadTokenArray(&pRegions);

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdCopyImageToMemory);
    pTgtCmdBuffer->CmdCopyImageToMemory(*pSrcImage, srcImageLayout, *pDstGpuMemory, regionCount, pRegions);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyMemoryToTiledImage(
    const IGpuMemory&                 srcGpuMemory,
    const IImage&                     dstImage,
    ImageLayout                       dstImageLayout,
    uint32                            regionCount,
    const MemoryTiledImageCopyRegion* pRegions)
{
    InsertToken(CmdBufCallId::CmdCopyMemoryToTiledImage);
    InsertToken(&srcGpuMemory);
    InsertToken(&dstImage);
    InsertToken(dstImageLayout);
    InsertTokenArray(pRegions, regionCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyMemoryToTiledImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pSrcGpuMemory                         = ReadTokenVal<IGpuMemory*>();
    auto pDstImage                             = ReadTokenVal<IImage*>();
    auto dstImageLayout                        = ReadTokenVal<ImageLayout>();
    const MemoryTiledImageCopyRegion* pRegions = nullptr;
    auto                         regionCount = ReadTokenArray(&pRegions);

    LogItem logItem = {};

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdCopyMemoryToTiledImage);
    pTgtCmdBuffer->CmdCopyMemoryToTiledImage(*pSrcGpuMemory, *pDstImage, dstImageLayout, regionCount, pRegions);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyTiledImageToMemory(
    const IImage&                     srcImage,
    ImageLayout                       srcImageLayout,
    const IGpuMemory&                 dstGpuMemory,
    uint32                            regionCount,
    const MemoryTiledImageCopyRegion* pRegions)
{
    InsertToken(CmdBufCallId::CmdCopyTiledImageToMemory);
    InsertToken(&srcImage);
    InsertToken(srcImageLayout);
    InsertToken(&dstGpuMemory);
    InsertTokenArray(pRegions, regionCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyTiledImageToMemory(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pSrcImage                             = ReadTokenVal<IImage*>();
    auto srcImageLayout                        = ReadTokenVal<ImageLayout>();
    auto pDstGpuMemory                         = ReadTokenVal<IGpuMemory*>();
    const MemoryTiledImageCopyRegion* pRegions = nullptr;
    auto regionCount                           = ReadTokenArray(&pRegions);

    LogItem logItem = {};

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdCopyTiledImageToMemory);
    pTgtCmdBuffer->CmdCopyTiledImageToMemory(*pSrcImage, srcImageLayout, *pDstGpuMemory, regionCount, pRegions);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
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
    InsertToken(CmdBufCallId::CmdClearColorBuffer);
    InsertToken(&gpuMemory);
    InsertToken(color);
    InsertToken(bufferFormat);
    InsertToken(bufferOffset);
    InsertToken(bufferExtent);
    InsertTokenArray(pRanges, rangeCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdClearColorBuffer(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto         pGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto         color      = ReadTokenVal<ClearColor>();
    auto         format     = ReadTokenVal<SwizzledFormat>();
    auto         offset     = ReadTokenVal<uint32>();
    auto         extent     = ReadTokenVal<uint32>();
    const Range* pRanges    = nullptr;
    auto         rangeCount = ReadTokenArray(&pRanges);

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdClearColorBuffer);
    pTgtCmdBuffer->CmdClearColorBuffer(*pGpuMemory, color, format, offset, extent, rangeCount, pRanges);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdClearBoundColorTargets(
    uint32                          colorTargetCount,
    const BoundColorTarget*         pBoundColorTargets,
    uint32                          regionCount,
    const ClearBoundTargetRegion*   pClearRegions)
{
    InsertToken(CmdBufCallId::CmdClearBoundColorTargets);
    InsertTokenArray(pBoundColorTargets, colorTargetCount);
    InsertTokenArray(pClearRegions, regionCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdClearBoundColorTargets(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const BoundColorTarget*         pBoundColorTargets      = nullptr;
    auto                            colorTargetCount        = ReadTokenArray(&pBoundColorTargets);
    const ClearBoundTargetRegion*   pClearRegions           = nullptr;
    auto                            regionCount             = ReadTokenArray(&pClearRegions);

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdClearBoundColorTargets);
    pTgtCmdBuffer->CmdClearBoundColorTargets(colorTargetCount,
                                             pBoundColorTargets,
                                             regionCount,
                                             pClearRegions);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
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
    InsertToken(CmdBufCallId::CmdClearColorImage);
    InsertToken(&image);
    InsertToken(imageLayout);
    InsertToken(color);
    InsertTokenArray(pRanges, rangeCount);
    InsertTokenArray(pBoxes, boxCount);
    InsertToken(flags);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdClearColorImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto               pImage      = ReadTokenVal<IImage*>();
    auto               imageLayout = ReadTokenVal<ImageLayout>();
    auto               color       = ReadTokenVal<ClearColor>();
    const SubresRange* pRanges     = nullptr;
    auto               rangeCount  = ReadTokenArray(&pRanges);
    const Box*         pBoxes      = nullptr;
    auto               boxCount    = ReadTokenArray(&pBoxes);
    auto               flags         = ReadTokenVal<uint32>();

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdClearColorImage);
    pTgtCmdBuffer->CmdClearColorImage(*pImage, imageLayout, color, rangeCount, pRanges, boxCount, pBoxes, flags);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdClearBoundDepthStencilTargets(
    float                           depth,
    uint8                           stencil,
    uint32                          samples,
    uint32                          fragments,
    DepthStencilSelectFlags         flag,
    uint32                          regionCount,
    const ClearBoundTargetRegion*   pClearRegions)
{
    InsertToken(CmdBufCallId::CmdClearBoundDepthStencilTargets);
    InsertToken(depth);
    InsertToken(stencil);
    InsertToken(samples);
    InsertToken(fragments);
    InsertToken(flag);
    InsertTokenArray(pClearRegions, regionCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdClearBoundDepthStencilTargets(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                            depth         = ReadTokenVal<float>();
    auto                            stencil       = ReadTokenVal<uint8>();
    auto                            samples       = ReadTokenVal<uint32>();
    auto                            fragments     = ReadTokenVal<uint32>();
    auto                            flag          = ReadTokenVal<DepthStencilSelectFlags>();
    const ClearBoundTargetRegion*   pClearRegions = nullptr;
    auto                            regionCount   = ReadTokenArray(&pClearRegions);

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdClearBoundDepthStencilTargets);
    pTgtCmdBuffer->CmdClearBoundDepthStencilTargets(depth,
                                                    stencil,
                                                    samples,
                                                    fragments,
                                                    flag,
                                                    regionCount,
                                                    pClearRegions);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdClearDepthStencil(
    const IImage&      image,
    ImageLayout        depthLayout,
    ImageLayout        stencilLayout,
    float              depth,
    uint8              stencil,
    uint32             rangeCount,
    const SubresRange* pRanges,
    uint32             rectCount,
    const Rect*        pRects,
    uint32             flags)
{
    InsertToken(CmdBufCallId::CmdClearDepthStencil);
    InsertToken(&image);
    InsertToken(depthLayout);
    InsertToken(stencilLayout);
    InsertToken(depth);
    InsertToken(stencil);
    InsertTokenArray(pRanges, rangeCount);
    InsertTokenArray(pRects, rectCount);
    InsertToken(flags);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdClearDepthStencil(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto               pImage        = ReadTokenVal<IImage*>();
    auto               depthLayout   = ReadTokenVal<ImageLayout>();
    auto               stencilLayout = ReadTokenVal<ImageLayout>();
    auto               depth         = ReadTokenVal<float>();
    auto               stencil       = ReadTokenVal<uint8>();
    const SubresRange* pRanges       = nullptr;
    auto               rangeCount    = ReadTokenArray(&pRanges);
    const Rect*        pRects        = nullptr;
    auto               rectCount     = ReadTokenArray(&pRects);
    auto               flags         = ReadTokenVal<uint32>();

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdClearDepthStencil);
    pTgtCmdBuffer->CmdClearDepthStencil(*pImage,
                                        depthLayout,
                                        stencilLayout,
                                        depth,
                                        stencil,
                                        rangeCount,
                                        pRanges,
                                        rectCount,
                                        pRects,
                                        flags);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdClearBufferView(
    const IGpuMemory& gpuMemory,
    const ClearColor& color,
    const void*       pBufferViewSrd,
    uint32            rangeCount,
    const Range*      pRanges)
{
    InsertToken(CmdBufCallId::CmdClearBufferView);
    InsertToken(&gpuMemory);
    InsertToken(color);
    InsertTokenArray(static_cast<const uint32*>(pBufferViewSrd), m_pDevice->BufferSrdDwords());
    InsertTokenArray(pRanges, rangeCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdClearBufferView(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto          pGpuMemory     = ReadTokenVal<IGpuMemory*>();
    auto          color          = ReadTokenVal<ClearColor>();
    const uint32* pBufferViewSrd = nullptr;
    ReadTokenArray(&pBufferViewSrd);
    const Range*  pRanges        = nullptr;
    auto          rangeCount     = ReadTokenArray(&pRanges);

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdClearBufferView);
    pTgtCmdBuffer->CmdClearBufferView(*pGpuMemory, color, pBufferViewSrd, rangeCount, pRanges);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
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
    InsertToken(CmdBufCallId::CmdClearImageView);
    InsertToken(&image);
    InsertToken(imageLayout);
    InsertToken(color);
    InsertTokenArray(static_cast<const uint32*>(pImageViewSrd), m_pDevice->ImageSrdDwords());
    InsertTokenArray(pRects, rectCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdClearImageView(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto          pImage        = ReadTokenVal<IImage*>();
    auto          imageLayout   = ReadTokenVal<ImageLayout>();
    auto          color         = ReadTokenVal<ClearColor>();
    const uint32* pImageViewSrd = nullptr;
    ReadTokenArray(&pImageViewSrd);
    const Rect*   pRects        = nullptr;
    auto          rectCount     = ReadTokenArray(&pRects);

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdClearImageView);
    pTgtCmdBuffer->CmdClearImageView(*pImage, imageLayout, color, pImageViewSrd, rectCount, pRects);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
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
    InsertToken(CmdBufCallId::CmdResolveImage);
    InsertToken(&srcImage);
    InsertToken(srcImageLayout);
    InsertToken(&dstImage);
    InsertToken(dstImageLayout);
    InsertToken(resolveMode);
    InsertTokenArray(pRegions, regionCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdResolveImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                      pSrcImage      = ReadTokenVal<IImage*>();
    auto                      srcImageLayout = ReadTokenVal<ImageLayout>();
    auto                      pDstImage      = ReadTokenVal<IImage*>();
    auto                      dstImageLayout = ReadTokenVal<ImageLayout>();
    auto                      resolveMode    = ReadTokenVal<ResolveMode>();
    const ImageResolveRegion* pRegions       = nullptr;
    auto                      regionCount    = ReadTokenArray(&pRegions);

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdResolveImage);
    pTgtCmdBuffer->CmdResolveImage(*pSrcImage,
                                   srcImageLayout,
                                   *pDstImage,
                                   dstImageLayout,
                                   resolveMode,
                                   regionCount,
                                   pRegions);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdSetEvent(
    const IGpuEvent& gpuEvent,
    HwPipePoint      setPoint)
{
    InsertToken(CmdBufCallId::CmdSetEvent);
    InsertToken(&gpuEvent);
    InsertToken(setPoint);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetEvent(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuEvent = ReadTokenVal<IGpuEvent*>();
    auto setPoint  = ReadTokenVal<HwPipePoint>();

    pTgtCmdBuffer->CmdSetEvent(*pGpuEvent, setPoint);
}

// =====================================================================================================================
void CmdBuffer::CmdResetEvent(
    const IGpuEvent& gpuEvent,
    HwPipePoint      resetPoint)
{
    InsertToken(CmdBufCallId::CmdResetEvent);
    InsertToken(&gpuEvent);
    InsertToken(resetPoint);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdResetEvent(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuEvent  = ReadTokenVal<IGpuEvent*>();
    auto resetPoint = ReadTokenVal<HwPipePoint>();

    pTgtCmdBuffer->CmdResetEvent(*pGpuEvent, resetPoint);
}

// =====================================================================================================================
void CmdBuffer::CmdPredicateEvent(
    const IGpuEvent& gpuEvent)
{
    InsertToken(CmdBufCallId::CmdPredicateEvent);
    InsertToken(&gpuEvent);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdPredicateEvent(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuEvent  = ReadTokenVal<IGpuEvent*>();

    pTgtCmdBuffer->CmdPredicateEvent(*pGpuEvent);
}

// =====================================================================================================================
void CmdBuffer::CmdMemoryAtomic(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    uint64            srcData,
    AtomicOp          atomicOp)
{
    InsertToken(CmdBufCallId::CmdMemoryAtomic);
    InsertToken(&dstGpuMemory);
    InsertToken(dstOffset);
    InsertToken(srcData);
    InsertToken(atomicOp);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdMemoryAtomic(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto dstOffset     = ReadTokenVal<gpusize>();
    auto srcData       = ReadTokenVal<uint64>();
    auto atomicOp      = ReadTokenVal<AtomicOp>();

    pTgtCmdBuffer->CmdMemoryAtomic(*pDstGpuMemory, dstOffset, srcData, atomicOp);
}

// =====================================================================================================================
void CmdBuffer::CmdResetQueryPool(
    const IQueryPool& queryPool,
    uint32            startQuery,
    uint32            queryCount)
{
    InsertToken(CmdBufCallId::CmdResetQueryPool);
    InsertToken(&queryPool);
    InsertToken(startQuery);
    InsertToken(queryCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdResetQueryPool(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pQueryPool = ReadTokenVal<IQueryPool*>();
    auto startQuery = ReadTokenVal<uint32>();
    auto queryCount = ReadTokenVal<uint32>();

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdResetQueryPool);
    pTgtCmdBuffer->CmdResetQueryPool(*pQueryPool, startQuery, queryCount);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdBeginQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot,
    QueryControlFlags flags)
{
    InsertToken(CmdBufCallId::CmdBeginQuery);
    InsertToken(&queryPool);
    InsertToken(queryType);
    InsertToken(slot);
    InsertToken(flags);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBeginQuery(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pQueryPool = ReadTokenVal<IQueryPool*>();
    auto queryType  = ReadTokenVal<QueryType>();
    auto slot       = ReadTokenVal<uint32>();
    auto flags      = ReadTokenVal<QueryControlFlags>();

    pTgtCmdBuffer->CmdBeginQuery(*pQueryPool, queryType, slot, flags);
}

// =====================================================================================================================
void CmdBuffer::CmdEndQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot)
{
    InsertToken(CmdBufCallId::CmdEndQuery);
    InsertToken(&queryPool);
    InsertToken(queryType);
    InsertToken(slot);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdEndQuery(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pQueryPool = ReadTokenVal<IQueryPool*>();
    auto queryType  = ReadTokenVal<QueryType>();
    auto slot       = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdEndQuery(*pQueryPool, queryType, slot);
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
    InsertToken(CmdBufCallId::CmdResolveQuery);
    InsertToken(&queryPool);
    InsertToken(flags);
    InsertToken(queryType);
    InsertToken(startQuery);
    InsertToken(queryCount);
    InsertToken(&dstGpuMemory);
    InsertToken(dstOffset);
    InsertToken(dstStride);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdResolveQuery(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pQueryPool    = ReadTokenVal<IQueryPool*>();
    auto flags         = ReadTokenVal<QueryResultFlags>();
    auto queryType     = ReadTokenVal<QueryType>();
    auto startQuery    = ReadTokenVal<uint32>();
    auto queryCount    = ReadTokenVal<uint32>();
    auto pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto dstOffset     = ReadTokenVal<gpusize>();
    auto dstStride     = ReadTokenVal<gpusize>();

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdResolveQuery);
    pTgtCmdBuffer->CmdResolveQuery(*pQueryPool,
                                   flags,
                                   queryType,
                                   startQuery,
                                   queryCount,
                                   *pDstGpuMemory,
                                   dstOffset,
                                   dstStride);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
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
    InsertToken(CmdBufCallId::CmdSetPredication);
    InsertToken(pQueryPool);
    InsertToken(slot);
    InsertToken(pGpuMemory);
    InsertToken(offset);
    InsertToken(predType);
    InsertToken(predPolarity);
    InsertToken(waitResults);
    InsertToken(accumulateData);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetPredication(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pQueryPool     = ReadTokenVal<IQueryPool*>();
    auto slot           = ReadTokenVal<uint32>();
    auto pGpuMemory     = ReadTokenVal<IGpuMemory*>();
    auto offset         = ReadTokenVal<gpusize>();
    auto predType       = ReadTokenVal<PredicateType>();
    auto predPolarity   = ReadTokenVal<bool>();
    auto waitResults    = ReadTokenVal<bool>();
    auto accumData      = ReadTokenVal<bool>();

    pTgtCmdBuffer->CmdSetPredication(pQueryPool, slot, pGpuMemory, offset, predType, predPolarity,
                                     waitResults, accumData);
}

// =====================================================================================================================
void CmdBuffer::CmdWriteTimestamp(
    HwPipePoint       pipePoint,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    InsertToken(CmdBufCallId::CmdWriteTimestamp);
    InsertToken(pipePoint);
    InsertToken(&dstGpuMemory);
    InsertToken(dstOffset);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdWriteTimestamp(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pipePoint     = ReadTokenVal<HwPipePoint>();
    auto pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto dstOffset     = ReadTokenVal<gpusize>();

    pTgtCmdBuffer->CmdWriteTimestamp(pipePoint, *pDstGpuMemory, dstOffset);
}

// =====================================================================================================================
void CmdBuffer::CmdWriteImmediate(
    HwPipePoint        pipePoint,
    uint64             data,
    ImmediateDataWidth dataSize,
    gpusize            address)
{
    InsertToken(CmdBufCallId::CmdWriteImmediate);
    InsertToken(pipePoint);
    InsertToken(data);
    InsertToken(dataSize);
    InsertToken(address);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdWriteImmediate(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pipePoint = ReadTokenVal<HwPipePoint>();
    auto data      = ReadTokenVal<uint64>();
    auto dataSize  = ReadTokenVal<ImmediateDataWidth>();
    auto address   = ReadTokenVal<gpusize>();

    pTgtCmdBuffer->CmdWriteImmediate(pipePoint, data, dataSize, address);
}

// =====================================================================================================================
void CmdBuffer::CmdLoadGds(
    HwPipePoint       pipePoint,
    uint32            dstGdsOffset,
    const IGpuMemory& srcGpuMemory,
    gpusize           srcMemOffset,
    uint32            size)
{
    InsertToken(CmdBufCallId::CmdLoadGds);
    InsertToken(pipePoint);
    InsertToken(dstGdsOffset);
    InsertToken(&srcGpuMemory);
    InsertToken(srcMemOffset);
    InsertToken(size);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdLoadGds(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto          pipePoint         = ReadTokenVal<HwPipePoint>();
    auto          dstGdsOffset      = ReadTokenVal<uint32>();
    auto          pSrcGpuMemory     = ReadTokenVal<IGpuMemory*>();
    auto          srcMemOffset      = ReadTokenVal<gpusize>();
    auto          size              = ReadTokenVal<uint32>();

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdLoadGds);
    pTgtCmdBuffer->CmdLoadGds(pipePoint, dstGdsOffset, *pSrcGpuMemory, srcMemOffset, size);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdStoreGds(
    HwPipePoint       pipePoint,
    uint32            srcGdsOffset,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstMemOffset,
    uint32            size,
    bool              waitForWC)
{
    InsertToken(CmdBufCallId::CmdStoreGds);
    InsertToken(pipePoint);
    InsertToken(srcGdsOffset);
    InsertToken(&dstGpuMemory);
    InsertToken(dstMemOffset);
    InsertToken(size);
    InsertToken(waitForWC);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdStoreGds(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto          pipePoint         = ReadTokenVal<HwPipePoint>();
    auto          srcGdsOffset      = ReadTokenVal<uint32>();
    auto          pDstGpuMemory     = ReadTokenVal<IGpuMemory*>();
    auto          dstMemOffset      = ReadTokenVal<gpusize>();
    auto          size              = ReadTokenVal<uint32>();
    auto          waitForWC         = ReadTokenVal<bool>();

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdStoreGds);
    pTgtCmdBuffer->CmdStoreGds(pipePoint, srcGdsOffset, *pDstGpuMemory, dstMemOffset, size, waitForWC);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateGds(
    HwPipePoint       pipePoint,
    uint32            gdsOffset,
    uint32            dataSize,
    const uint32*     pData)
{
    InsertToken(CmdBufCallId::CmdUpdateGds);
    InsertToken(pipePoint);
    InsertToken(gdsOffset);
    InsertToken(dataSize);
    InsertTokenArray(pData, static_cast<uint32>(dataSize / sizeof(uint32)));
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdUpdateGds(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto          pipePoint         = ReadTokenVal<HwPipePoint>();
    auto          gdsOffset         = ReadTokenVal<uint32>();
    auto          dataSize          = ReadTokenVal<uint32>();
    const uint32* pData             = nullptr;
    ReadTokenArray(&pData);

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdUpdateGds);
    pTgtCmdBuffer->CmdUpdateGds(pipePoint, gdsOffset, dataSize, pData);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdFillGds(
    HwPipePoint       pipePoint,
    uint32            gdsOffset,
    uint32            fillSize,
    uint32            data)
{
    InsertToken(CmdBufCallId::CmdFillGds);
    InsertToken(pipePoint);
    InsertToken(gdsOffset);
    InsertToken(fillSize);
    InsertToken(data);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdFillGds(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto          pipePoint         = ReadTokenVal<HwPipePoint>();
    auto          gdsOffset         = ReadTokenVal<uint32>();
    auto          fillSize          = ReadTokenVal<uint32>();
    auto          data              = ReadTokenVal<uint32>();

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdFillGds);
    pTgtCmdBuffer->CmdFillGds(pipePoint, gdsOffset, fillSize, data);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdLoadBufferFilledSizes(
    const gpusize (&gpuVirtAddr)[MaxStreamOutTargets])
{
    InsertToken(CmdBufCallId::CmdLoadBufferFilledSizes);
    for (uint32 i = 0; i < MaxStreamOutTargets; i++)
    {
        InsertToken(gpuVirtAddr[i]);
    }
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdLoadBufferFilledSizes(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    gpusize gpuVirtAddrs[MaxStreamOutTargets];

    for (uint32 i = 0; i < MaxStreamOutTargets; i++)
    {
        gpuVirtAddrs[i] = ReadTokenVal<gpusize>();
    }

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdLoadBufferFilledSizes);
    pTgtCmdBuffer->CmdLoadBufferFilledSizes(gpuVirtAddrs);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdSaveBufferFilledSizes(
    const gpusize (&gpuVirtAddr)[MaxStreamOutTargets])
{
    InsertToken(CmdBufCallId::CmdSaveBufferFilledSizes);
    for (uint32 i = 0; i < MaxStreamOutTargets; i++)
    {
        InsertToken(gpuVirtAddr[i]);
    }
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSaveBufferFilledSizes(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    gpusize gpuVirtAddrs[MaxStreamOutTargets];

    for (uint32 i = 0; i < MaxStreamOutTargets; i++)
    {
        gpuVirtAddrs[i] = ReadTokenVal<gpusize>();
    }

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdSaveBufferFilledSizes);
    pTgtCmdBuffer->CmdSaveBufferFilledSizes(gpuVirtAddrs);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdLoadCeRam(
    const IGpuMemory& srcGpuMemory,
    gpusize           memOffset,
    uint32            ramOffset,
    uint32            dwordSize)
{
    InsertToken(CmdBufCallId::CmdLoadCeRam);
    InsertToken(&srcGpuMemory);
    InsertToken(memOffset);
    InsertToken(ramOffset);
    InsertToken(dwordSize);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdLoadCeRam(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pSrcGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto memOffset     = ReadTokenVal<gpusize>();
    auto ramOffset     = ReadTokenVal<uint32>();
    auto dwordSize     = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdLoadCeRam(*pSrcGpuMemory, memOffset, ramOffset, dwordSize);
}

// =====================================================================================================================
void CmdBuffer::CmdWriteCeRam(
    const void* pSrcData,
    uint32      ramOffset,
    uint32      dwordSize)
{
    InsertToken(CmdBufCallId::CmdWriteCeRam);
    InsertTokenArray(static_cast<const uint32*>(pSrcData), dwordSize);
    InsertToken(ramOffset);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdWriteCeRam(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const uint32* pSrcData  = nullptr;
    auto          dwordSize = ReadTokenArray(&pSrcData);
    auto          ramOffset = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdWriteCeRam(pSrcData, ramOffset, dwordSize);
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
    InsertToken(CmdBufCallId::CmdDumpCeRam);
    InsertToken(&dstGpuMemory);
    InsertToken(memOffset);
    InsertToken(ramOffset);
    InsertToken(dwordSize);
    InsertToken(currRingPos);
    InsertToken(ringSize);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDumpCeRam(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto memOffset     = ReadTokenVal<gpusize>();
    auto ramOffset     = ReadTokenVal<uint32>();
    auto dwordSize     = ReadTokenVal<uint32>();
    auto currRingPos   = ReadTokenVal<uint32>();
    auto ringSize      = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdDumpCeRam(*pDstGpuMemory, memOffset, ramOffset, dwordSize, currRingPos, ringSize);
}

// =====================================================================================================================
uint32 CmdBuffer::GetEmbeddedDataLimit() const
{
    return NextLayer()->GetEmbeddedDataLimit();
}

// =====================================================================================================================
uint32* CmdBuffer::CmdAllocateEmbeddedData(
    uint32   sizeInDwords,
    uint32   alignmentInDwords,
    gpusize* pGpuAddress)
{
    return NextLayer()->CmdAllocateEmbeddedData(sizeInDwords, alignmentInDwords, pGpuAddress);
}

// =====================================================================================================================
void CmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32            cmdBufferCount,
    ICmdBuffer*const* ppCmdBuffers)
{
    InsertToken(CmdBufCallId::CmdExecuteNestedCmdBuffers);
    InsertTokenArray(ppCmdBuffers, cmdBufferCount);
}

// =====================================================================================================================
// Nested command buffers are treated similarly to root-level command buffers.  The recorded commands are replayed
// (plus profiling) into queue-owned command buffers and those command buffers are the ones inserted into the final
// command stream.
void CmdBuffer::ReplayCmdExecuteNestedCmdBuffers(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    if (m_pDevice->LoggingEnabled(GpuProfilerGranularityDraw))
    {
        LogItem logItem = { };
        logItem.type              = CmdBufferCall;
        logItem.frameId           = m_curLogFrame;
        logItem.cmdBufCall.callId = CmdBufCallId::CmdExecuteNestedCmdBuffers;
        pQueue->AddLogItem(logItem);
    }

    ICmdBuffer*const* ppCmdBuffers   = nullptr;
    const uint32      cmdBufferCount = ReadTokenArray(&ppCmdBuffers);
    auto*const        pPlatform      = static_cast<Platform*>(m_pDevice->GetPlatform());

    AutoBuffer<ICmdBuffer*, 32, Platform> tgtCmdBuffers(cmdBufferCount, pPlatform);

    if (tgtCmdBuffers.Capacity() < cmdBufferCount)
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        for (uint32 i = 0; i < cmdBufferCount; i++)
        {
            auto*const pNestedCmdBuffer    = static_cast<CmdBuffer*>(ppCmdBuffers[i]);
            auto*const pNestedTgtCmdBuffer = pQueue->AcquireNestedCmdBuf();

            tgtCmdBuffers[i] = pNestedTgtCmdBuffer;
            pNestedCmdBuffer->Replay(pQueue, pNestedTgtCmdBuffer, m_curLogFrame);
        }

        pTgtCmdBuffer->CmdExecuteNestedCmdBuffers(cmdBufferCount, &tgtCmdBuffers[0]);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdExecuteIndirectCmds(
    const IIndirectCmdGenerator& generator,
    const IGpuMemory&            gpuMemory,
    gpusize                      offset,
    uint32                       maximumCount,
    gpusize                      countGpuAddr)
{
    InsertToken(CmdBufCallId::CmdExecuteIndirectCmds);
    InsertToken(&generator);
    InsertToken(&gpuMemory);
    InsertToken(offset);
    InsertToken(maximumCount);
    InsertToken(countGpuAddr);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdExecuteIndirectCmds(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const auto*const pGenerator   = ReadTokenVal<const IIndirectCmdGenerator*>();
    const auto*const pGpuMemory   = ReadTokenVal<const IGpuMemory*>();
    const gpusize    offset       = ReadTokenVal<gpusize>();
    const uint32     maximumCount = ReadTokenVal<uint32>();
    const gpusize    countGpuAddr = ReadTokenVal<gpusize>();

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdExecuteIndirectCmds);
    pTgtCmdBuffer->CmdExecuteIndirectCmds(*pGenerator, *pGpuMemory, offset, maximumCount, countGpuAddr);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdIf(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    InsertToken(CmdBufCallId::CmdIf);
    InsertToken(&gpuMemory);
    InsertToken(offset);
    InsertToken(data);
    InsertToken(mask);
    InsertToken(compareFunc);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdIf(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuMemory  = ReadTokenVal<IGpuMemory*>();
    auto offset      = ReadTokenVal<gpusize>();
    auto data        = ReadTokenVal<uint64>();
    auto mask        = ReadTokenVal<uint64>();
    auto compareFunc = ReadTokenVal<CompareFunc>();

    pTgtCmdBuffer->CmdIf(*pGpuMemory, offset, data, mask, compareFunc);
}

// =====================================================================================================================
void CmdBuffer::CmdElse()
{
    InsertToken(CmdBufCallId::CmdElse);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdElse(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdElse();
}

// =====================================================================================================================
void CmdBuffer::CmdEndIf()
{
    InsertToken(CmdBufCallId::CmdEndIf);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdEndIf(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdEndIf();
}

// =====================================================================================================================
void CmdBuffer::CmdWhile(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    InsertToken(CmdBufCallId::CmdWhile);
    InsertToken(&gpuMemory);
    InsertToken(offset);
    InsertToken(data);
    InsertToken(mask);
    InsertToken(compareFunc);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdWhile(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuMemory  = ReadTokenVal<IGpuMemory*>();
    auto offset      = ReadTokenVal<gpusize>();
    auto data        = ReadTokenVal<uint64>();
    auto mask        = ReadTokenVal<uint64>();
    auto compareFunc = ReadTokenVal<CompareFunc>();

    // Note that the entire while loop clause will be timed as one item.  If timestamps were written inside a while
    // loop, the last iteration would be the only one visible for logging.  The corresponding LogPostTimedCall() is made
    // in ReplayCmdEndWhile().
    memset(&m_loopLogItem, 0, sizeof(m_loopLogItem));
    LogPreTimedCall(pQueue, pTgtCmdBuffer, &m_loopLogItem, CmdBufCallId::CmdWhile);
    pTgtCmdBuffer->CmdWhile(*pGpuMemory, offset, data, mask, compareFunc);

    m_disableDataGathering = true;
}

// =====================================================================================================================
void CmdBuffer::CmdEndWhile()
{
    InsertToken(CmdBufCallId::CmdEndWhile);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdEndWhile(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdEndWhile();

    m_disableDataGathering = false;

    // Note that the entire while loop clause will be timed as one item.  See the comment in ReplayCmdWhile().
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &m_loopLogItem);
}

// =====================================================================================================================
void CmdBuffer::CmdSetHiSCompareState0(
    CompareFunc compFunc,
    uint32      compMask,
    uint32      compValue,
    bool        enable)
{
    InsertToken(CmdBufCallId::CmdSetHiSCompareState0);
    InsertToken(compFunc);
    InsertToken(compMask);
    InsertToken(compValue);
    InsertToken(enable);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetHiSCompareState0(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const CompareFunc compFunc  = ReadTokenVal<CompareFunc>();
    const uint32      compMask  = ReadTokenVal<uint32>();
    const uint32      compValue = ReadTokenVal<uint32>();
    const bool        enable    = ReadTokenVal<bool>();

    pTgtCmdBuffer->CmdSetHiSCompareState0(compFunc, compMask, compValue, enable);
}

// =====================================================================================================================
void CmdBuffer::CmdSetHiSCompareState1(
    CompareFunc compFunc,
    uint32      compMask,
    uint32      compValue,
    bool        enable)
{
    InsertToken(CmdBufCallId::CmdSetHiSCompareState1);
    InsertToken(compFunc);
    InsertToken(compMask);
    InsertToken(compValue);
    InsertToken(enable);

}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetHiSCompareState1(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const CompareFunc compFunc  = ReadTokenVal<CompareFunc>();
    const uint32      compMask  = ReadTokenVal<uint32>();
    const uint32      compValue = ReadTokenVal<uint32>();
    const bool        enable    = ReadTokenVal<bool>();

    pTgtCmdBuffer->CmdSetHiSCompareState1(compFunc, compMask, compValue, enable);
}

// =====================================================================================================================
void CmdBuffer::CmdFlglSync()
{
    InsertToken(CmdBufCallId::CmdFlglSync);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdFlglSync(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdFlglSync();
}

// =====================================================================================================================
void CmdBuffer::CmdFlglEnable()
{
    InsertToken(CmdBufCallId::CmdFlglEnable);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdFlglEnable(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
     pTgtCmdBuffer->CmdFlglEnable();
}

// =====================================================================================================================
void  CmdBuffer::CmdFlglDisable()
{
    InsertToken(CmdBufCallId::CmdFlglDisable);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdFlglDisable(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdFlglDisable();
}

// =====================================================================================================================
void CmdBuffer::CmdBeginPerfExperiment(
    IPerfExperiment* pPerfExperiment)
{
    InsertToken(CmdBufCallId::CmdBeginPerfExperiment);
    InsertToken(pPerfExperiment);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBeginPerfExperiment(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdBeginPerfExperiment(ReadTokenVal<IPerfExperiment*>());
}

// =====================================================================================================================
void CmdBuffer::CmdUpdatePerfExperimentSqttTokenMask(
    IPerfExperiment* pPerfExperiment,
    uint32           sqttTokenMask)
{
    InsertToken(CmdBufCallId::CmdUpdatePerfExperimentSqttTokenMask);
    InsertToken(pPerfExperiment);
    InsertToken(sqttTokenMask);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdUpdatePerfExperimentSqttTokenMask(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdUpdatePerfExperimentSqttTokenMask(ReadTokenVal<IPerfExperiment*>(), ReadTokenVal<uint32>());
}

// =====================================================================================================================
void CmdBuffer::CmdEndPerfExperiment(
    IPerfExperiment* pPerfExperiment)
{
    InsertToken(CmdBufCallId::CmdEndPerfExperiment);
    InsertToken(pPerfExperiment);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdEndPerfExperiment(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdEndPerfExperiment(ReadTokenVal<IPerfExperiment*>());
}

// =====================================================================================================================
void CmdBuffer::CmdInsertTraceMarker(
    PerfTraceMarkerType markerType,
    uint32              markerData)
{
    InsertToken(CmdBufCallId::CmdInsertTraceMarker);
    InsertToken(markerType);
    InsertToken(markerData);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdInsertTraceMarker(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto markerType = ReadTokenVal<PerfTraceMarkerType>();
    auto markerData = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdInsertTraceMarker(markerType, markerData);
}

// =====================================================================================================================
void CmdBuffer::CmdInsertRgpTraceMarker(
    uint32      numDwords,
    const void* pData)
{
    InsertToken(CmdBufCallId::CmdInsertRgpTraceMarker);
    InsertTokenArray(static_cast<const uint32*>(pData), numDwords);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdInsertRgpTraceMarker(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const uint32* pData = nullptr;
    uint32 numDwords = ReadTokenArray(&pData);

    pTgtCmdBuffer->CmdInsertRgpTraceMarker(numDwords, pData);
}

// =====================================================================================================================
void CmdBuffer::CmdSaveComputeState(
    uint32 stateFlags)
{
    InsertToken(CmdBufCallId::CmdSaveComputeState);
    InsertToken(stateFlags);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSaveComputeState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSaveComputeState(ReadTokenVal<uint32>());
}

// =====================================================================================================================
void CmdBuffer::CmdRestoreComputeState(
    uint32 stateFlags)
{
    InsertToken(CmdBufCallId::CmdRestoreComputeState);
    InsertToken(stateFlags);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdRestoreComputeState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdRestoreComputeState(ReadTokenVal<uint32>());
}

// =====================================================================================================================
void CmdBuffer::CmdCommentString(
    const char* pComment)
{
    InsertToken(CmdBufCallId::CmdCommentString);
    InsertTokenArray(pComment, static_cast<uint32>(strlen(pComment)) + 1);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCommentString(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const char* pComment = nullptr;
    uint32 commentLength = ReadTokenArray(&pComment);

    if (m_pDevice->LoggingEnabled(GpuProfilerGranularityDraw))
    {
        LogItem logItem = { };
        logItem.type                     = CmdBufferCall;
        logItem.frameId                  = m_curLogFrame;
        logItem.cmdBufCall.callId        = CmdBufCallId::CmdCommentString;
        logItem.cmdBufCall.flags.comment = 1;

        // Copy as much of the comment as possible, leaving one character at the end for a null terminator.
        // We zero-inited the LogItem so we shouldn't need to explicitly write a null terminator.
        const size_t copySize = sizeof(char) * Min<size_t>(commentLength, MaxCommentLength - 1);
        memcpy(logItem.cmdBufCall.comment.string, pComment, copySize);

        pQueue->AddLogItem(logItem);
    }

    pTgtCmdBuffer->CmdCommentString(pComment);
}

// =====================================================================================================================
void CmdBuffer::CmdSetUserClipPlanes(
    uint32               firstPlane,
    uint32               planeCount,
    const UserClipPlane* pPlanes)
{
    InsertToken(CmdBufCallId::CmdSetUserClipPlanes);
    InsertToken(firstPlane);
    InsertTokenArray(pPlanes, planeCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetUserClipPlanes(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const UserClipPlane* pPlanes    = nullptr;
    auto                 firstPlane = ReadTokenVal<uint32>();
    auto                 planeCount = ReadTokenArray(&pPlanes);

    pTgtCmdBuffer->CmdSetUserClipPlanes(firstPlane, planeCount, pPlanes);
}

// =====================================================================================================================
void CmdBuffer::CmdStartGpuProfilerLogging()
{
    InsertToken(CmdBufCallId::CmdStartGpuProfilerLogging);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdStartGpuProfilerLogging(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    m_forceDrawGranularityLogging = true;
}

// =====================================================================================================================
void CmdBuffer::CmdStopGpuProfilerLogging()
{
    InsertToken(CmdBufCallId::CmdStopGpuProfilerLogging);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdStopGpuProfilerLogging(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    m_forceDrawGranularityLogging = false;
}

// =====================================================================================================================
void CmdBuffer::CmdXdmaWaitFlipPending()
{
    InsertToken(CmdBufCallId::CmdXdmaWaitFlipPending);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdXdmaWaitFlipPending(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdXdmaWaitFlipPending);
    pTgtCmdBuffer->CmdXdmaWaitFlipPending();
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
// Replays the commands that were recorded on this command buffer into a separate, target command buffer while adding
// additional commands for GPU profiling purposes.
void CmdBuffer::Replay(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer,
    uint32           curFrame)
{
    typedef void (CmdBuffer::* ReplayFunc)(Queue*, TargetCmdBuffer*);

    constexpr ReplayFunc ReplayFuncTbl[] =
    {
        &CmdBuffer::ReplayBegin,
        &CmdBuffer::ReplayEnd,
        &CmdBuffer::ReplayCmdBindPipeline,
        &CmdBuffer::ReplayCmdBindMsaaState,
        &CmdBuffer::ReplayCmdBindColorBlendState,
        &CmdBuffer::ReplayCmdBindDepthStencilState,
        &CmdBuffer::ReplayCmdBindIndexData,
        &CmdBuffer::ReplayCmdBindTargets,
        &CmdBuffer::ReplayCmdBindStreamOutTargets,
        &CmdBuffer::ReplayCmdBindBorderColorPalette,
        &CmdBuffer::ReplayCmdSetUserData,
        &CmdBuffer::ReplayCmdSetIndirectUserData,
        &CmdBuffer::ReplayCmdSetIndirectUserDataWatermark,
        &CmdBuffer::ReplayCmdSetBlendConst,
        &CmdBuffer::ReplayCmdSetInputAssemblyState,
        &CmdBuffer::ReplayCmdSetTriangleRasterState,
        &CmdBuffer::ReplayCmdSetPointLineRasterState,
        &CmdBuffer::ReplayCmdSetDepthBiasState,
        &CmdBuffer::ReplayCmdSetDepthBounds,
        &CmdBuffer::ReplayCmdSetStencilRefMasks,
        &CmdBuffer::ReplayCmdSetMsaaQuadSamplePattern,
        &CmdBuffer::ReplayCmdSetViewports,
        &CmdBuffer::ReplayCmdSetScissorRects,
        &CmdBuffer::ReplayCmdSetGlobalScissor,
        &CmdBuffer::ReplayCmdBarrier,
        &CmdBuffer::ReplayCmdWaitRegisterValue,
        &CmdBuffer::ReplayCmdWaitMemoryValue,
        &CmdBuffer::ReplayCmdWaitBusAddressableMemoryMarker,
        &CmdBuffer::ReplayCmdDraw,
        &CmdBuffer::ReplayCmdDrawOpaque,
        &CmdBuffer::ReplayCmdDrawIndexed,
        &CmdBuffer::ReplayCmdDrawIndirectMulti,
        &CmdBuffer::ReplayCmdDrawIndexedIndirectMulti,
        &CmdBuffer::ReplayCmdDispatch,
        &CmdBuffer::ReplayCmdDispatchIndirect,
        &CmdBuffer::ReplayCmdDispatchOffset,
        &CmdBuffer::ReplayCmdUpdateMemory,
        &CmdBuffer::ReplayCmdUpdateBusAddressableMemoryMarker,
        &CmdBuffer::ReplayCmdFillMemory,
        &CmdBuffer::ReplayCmdCopyMemory,
        &CmdBuffer::ReplayCmdCopyTypedBuffer,
        &CmdBuffer::ReplayCmdCopyRegisterToMemory,
        &CmdBuffer::ReplayCmdCopyImage,
        &CmdBuffer::ReplayCmdScaledCopyImage,
        &CmdBuffer::ReplayCmdColorSpaceConversionCopy,
        &CmdBuffer::ReplayCmdCloneImageData,
        &CmdBuffer::ReplayCmdCopyMemoryToImage,
        &CmdBuffer::ReplayCmdCopyImageToMemory,
        &CmdBuffer::ReplayCmdClearColorBuffer,
        &CmdBuffer::ReplayCmdClearBoundColorTargets,
        &CmdBuffer::ReplayCmdClearColorImage,
        &CmdBuffer::ReplayCmdClearBoundDepthStencilTargets,
        &CmdBuffer::ReplayCmdClearDepthStencil,
        &CmdBuffer::ReplayCmdClearBufferView,
        &CmdBuffer::ReplayCmdClearImageView,
        &CmdBuffer::ReplayCmdResolveImage,
        &CmdBuffer::ReplayCmdSetEvent,
        &CmdBuffer::ReplayCmdResetEvent,
        &CmdBuffer::ReplayCmdPredicateEvent,
        &CmdBuffer::ReplayCmdMemoryAtomic,
        &CmdBuffer::ReplayCmdResetQueryPool,
        &CmdBuffer::ReplayCmdBeginQuery,
        &CmdBuffer::ReplayCmdEndQuery,
        &CmdBuffer::ReplayCmdResolveQuery,
        &CmdBuffer::ReplayCmdSetPredication,
        &CmdBuffer::ReplayCmdWriteTimestamp,
        &CmdBuffer::ReplayCmdWriteImmediate,
        &CmdBuffer::ReplayCmdLoadGds,
        &CmdBuffer::ReplayCmdStoreGds,
        &CmdBuffer::ReplayCmdUpdateGds,
        &CmdBuffer::ReplayCmdFillGds,
        &CmdBuffer::ReplayCmdLoadBufferFilledSizes,
        &CmdBuffer::ReplayCmdSaveBufferFilledSizes,
        &CmdBuffer::ReplayCmdLoadCeRam,
        &CmdBuffer::ReplayCmdWriteCeRam,
        &CmdBuffer::ReplayCmdDumpCeRam,
        &CmdBuffer::ReplayCmdExecuteNestedCmdBuffers,
        &CmdBuffer::ReplayCmdExecuteIndirectCmds,
        &CmdBuffer::ReplayCmdIf,
        &CmdBuffer::ReplayCmdElse,
        &CmdBuffer::ReplayCmdEndIf,
        &CmdBuffer::ReplayCmdWhile,
        &CmdBuffer::ReplayCmdEndWhile,
        &CmdBuffer::ReplayCmdFlglSync,
        &CmdBuffer::ReplayCmdFlglEnable,
        &CmdBuffer::ReplayCmdFlglDisable,
        &CmdBuffer::ReplayCmdBeginPerfExperiment,
        &CmdBuffer::ReplayCmdUpdatePerfExperimentSqttTokenMask,
        &CmdBuffer::ReplayCmdEndPerfExperiment,
        &CmdBuffer::ReplayCmdInsertTraceMarker,
        &CmdBuffer::ReplayCmdInsertRgpTraceMarker,
        &CmdBuffer::ReplayCmdSaveComputeState,
        &CmdBuffer::ReplayCmdRestoreComputeState,
        &CmdBuffer::ReplayCmdSetUserClipPlanes,
        &CmdBuffer::ReplayCmdCommentString,
        &CmdBuffer::ReplayCmdXdmaWaitFlipPending,
        &CmdBuffer::ReplayCmdCopyMemoryToTiledImage,
        &CmdBuffer::ReplayCmdCopyTiledImageToMemory,
        &CmdBuffer::ReplayCmdCopyImageToPackedPixelImage,
        &CmdBuffer::ReplayCmdStartGpuProfilerLogging,
        &CmdBuffer::ReplayCmdStopGpuProfilerLogging,
        &CmdBuffer::ReplayCmdSetViewInstanceMask,
        &CmdBuffer::ReplayCmdSetHiSCompareState0,
        &CmdBuffer::ReplayCmdSetHiSCompareState1,
    };

    static_assert(ArrayLen(ReplayFuncTbl) == static_cast<size_t>(CmdBufCallId::Count),
                  "Replay table must be updated!");

    m_pTokenRdPtr = m_pTokenStream;

    CmdBufCallId callId;

    m_curLogFrame = curFrame;

    do
    {
        callId = ReadTokenVal<CmdBufCallId>();

        (this->*ReplayFuncTbl[static_cast<uint32>(callId)])(pQueue, pTgtCmdBuffer);
    } while (callId != CmdBufCallId::End);
}

// =====================================================================================================================
// Perform initial setup of a log item and insert pre-call events into the target command buffer (i.e., begin queries,
// issue pre-call timestamp, etc.). Adds this log item to the queue for processing if LogPostTimedCall will not be
// called.
void CmdBuffer::LogPreTimedCall(
    Queue*            pQueue,
    TargetCmdBuffer*  pTgtCmdBuffer,
    LogItem*          pLogItem,
    CmdBufCallId      callId)
{
    if (m_pDevice->LoggingEnabled(GpuProfilerGranularityDraw) || m_forceDrawGranularityLogging)
    {
        pLogItem->type              = CmdBufferCall;
        pLogItem->frameId           = m_curLogFrame;
        pLogItem->cmdBufCall.callId = callId;

        // Should we enable SQ thread traces for this call?
        bool enableSqThreadTrace = false;

        // Log currently bound pipeline/shader state.
        if (pLogItem->cmdBufCall.flags.draw)
        {
            pLogItem->cmdBufCall.draw.pipelineInfo = m_graphicsPipelineInfo;

            if (m_flags.enableSqThreadTrace &&
                m_pDevice->SqttEnabledForPipeline(m_graphicsPipelineInfo, PipelineBindPoint::Graphics))
            {
                if ((m_pDevice->GetSqttMaxDraws() == 0) ||
                    (m_pDevice->GetSqttCurDraws() < m_pDevice->GetSqttMaxDraws()))
                {
                    m_pDevice->AddSqttCurDraws();
                    enableSqThreadTrace = true;
                }
            }
        }
        else if (pLogItem->cmdBufCall.flags.dispatch)
        {
            pLogItem->cmdBufCall.dispatch.pipelineInfo = m_computePipelineInfo;

            if (m_flags.enableSqThreadTrace &&
                m_pDevice->SqttEnabledForPipeline(m_computePipelineInfo, PipelineBindPoint::Compute))
            {
                if ((m_pDevice->GetSqttMaxDraws() == 0) ||
                    (m_pDevice->GetSqttCurDraws() < m_pDevice->GetSqttMaxDraws()))
                {
                    m_pDevice->AddSqttCurDraws();
                    enableSqThreadTrace = true;
                }
            }
        }

        if (m_disableDataGathering == false)
        {
            const bool enablePerfExp   = (m_pDevice->NumGlobalPerfCounters() > 0)    ||
                                         (m_pDevice->NumStreamingPerfCounters() > 0) ||
                                         enableSqThreadTrace;
            const bool enablePipeStats = m_flags.logPipeStats;

            pTgtCmdBuffer->BeginSample(pQueue, pLogItem, enablePipeStats, enablePerfExp);
        }
    }
}

// =====================================================================================================================
// Insert post-call events into the target command buffer (i.e., end queries, issue post-call timestamp, etc.), then
// add this log item to the queue for processing once the corresponding submit completes.
void CmdBuffer::LogPostTimedCall(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer,
    LogItem*         pLogItem)
{
    if (m_pDevice->LoggingEnabled(GpuProfilerGranularityDraw) || m_forceDrawGranularityLogging)
    {
        pTgtCmdBuffer->EndSample(pQueue, pLogItem);

        // Add this log item to the queue for processing once the corresponding submit is idle.
        pQueue->AddLogItem(*pLogItem);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCopyImageToPackedPixelImage(
    const IImage&          srcImage,
    const IImage&          dstImage,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    Pal::PackedPixelType   packPixelType)
{
    InsertToken(CmdBufCallId::CmdCopyImageToPackedPixelImage);
    InsertToken(&srcImage);
    InsertToken(&dstImage);
    InsertTokenArray(pRegions, regionCount);
    InsertToken(packPixelType);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyImageToPackedPixelImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                   pSrcImage      = ReadTokenVal<IImage*>();
    auto                   pDstImage      = ReadTokenVal<IImage*>();
    const ImageCopyRegion* pRegions       = nullptr;
    auto                   regionCount    = ReadTokenArray(&pRegions);
    auto                   packPixelType  = ReadTokenVal<Pal::PackedPixelType>();

    LogItem logItem = { };

    LogPreTimedCall(pQueue, pTgtCmdBuffer, &logItem, CmdBufCallId::CmdCopyImageToPackedPixelImage);
    pTgtCmdBuffer->CmdCopyImageToPackedPixelImage(*pSrcImage,
                                                  *pDstImage,
                                                  regionCount,
                                                  pRegions,
                                                  packPixelType);
    LogPostTimedCall(pQueue, pTgtCmdBuffer, &logItem);
}

// =====================================================================================================================
void CmdBuffer::CmdSetViewInstanceMask(
    uint32 mask)
{
    InsertToken(CmdBufCallId::CmdSetViewInstanceMask);
    InsertToken(mask);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetViewInstanceMask(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto mask = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdSetViewInstanceMask(mask);
}

// =====================================================================================================================
TargetCmdBuffer::TargetCmdBuffer(
    const CmdBufferCreateInfo& createInfo,
    ICmdBuffer*                pNextCmdBuffer,
    const DeviceDecorator*     pNextDevice)
    :
    CmdBufferFwdDecorator(pNextCmdBuffer, pNextDevice),
    m_allocator(2 * 1024 * 1024),
    m_pAllocatorStream(nullptr),
    m_pCurrentBarrierComment(nullptr),
    m_currentCommentSize(0),
    m_queueType(createInfo.queueType),
    m_engineType(createInfo.engineType),
    m_supportTimestamps(false),
    m_pGpaSession(nullptr)
{
}

// =====================================================================================================================
Result TargetCmdBuffer::Init()
{
    Result result = m_allocator.Init();

    if (result == Result::Success)
    {
        m_pAllocatorStream = m_allocator.Current();
    }

    DeviceProperties info;
    if (result == Result::Success)
    {
        result = m_pDevice->GetProperties(&info);
    }

    if (result == Result::Success)
    {
        m_supportTimestamps = info.engineProperties[m_engineType].flags.supportsTimestamps ? true : false;
    }

    return result;
}

// =====================================================================================================================
Result TargetCmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    // Rewind the allocator to the beginning, overwriting any data stored from the last time this command buffer was
    // recorded.
    m_allocator.Rewind(m_pAllocatorStream, false);
    ResetBarrierString();

    return CmdBufferFwdDecorator::Begin(info);
}

// =====================================================================================================================
void TargetCmdBuffer::ResetBarrierString()
{
    m_pCurrentBarrierComment = nullptr;
    m_currentCommentSize     = 0;
}

// =====================================================================================================================
void TargetCmdBuffer::AddBarrierString(
    const char* pString)
{
    // The space we append to the current string must fit the contents of pString plus a newline and a null terminator.
    size_t newStrLen           = strlen(pString);
    size_t newStringLenToAlloc = newStrLen + 2;
    size_t currentStringLength = 0;
    if (m_pCurrentBarrierComment != nullptr)
    {
        currentStringLength  = strlen(m_pCurrentBarrierComment);
        // A null terminator is already counted when allocate the 1st comment string
        newStringLenToAlloc -= 1;
    }

    if (newStringLenToAlloc > m_allocator.Remaining())
    {
        // Do nothing if this string won't fit in the linear allocator; this is better than crashing on release builds.
        // Increase the size of the linear allocator to see all of the strings.
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        // Note that this calls the Alloc() method directly instead of using PAL_MALLOC() for two reasons:
        // 1. PAL_MALLOC() aligns all allocations to 64 bytes, which is undesirable here
        // 2. it would be unseemly to use PAL_MALLOC() without corresponding PAL_FREE() calls, and tracking addresses
        //    where each free would be needed would be painful.
#if PAL_MEMTRACK
        const AllocInfo info(newStringLenToAlloc, 1, false, AllocInternal, MemBlkType::Malloc, nullptr, 0);
#else
        const AllocInfo info(newStringLenToAlloc, 1, false, AllocInternal);
#endif

        char* pBarrierComment = static_cast<char*>(m_allocator.Alloc(info));
        if (m_pCurrentBarrierComment == nullptr)
        {
            m_pCurrentBarrierComment = pBarrierComment;
        }
        else
        {
            PAL_ASSERT(static_cast<size_t>(VoidPtrDiff(pBarrierComment, m_pCurrentBarrierComment)) ==
                      (currentStringLength + 1));
        }

        m_currentCommentSize = currentStringLength + newStrLen + 2;

        Snprintf(m_pCurrentBarrierComment + currentStringLength, newStrLen + 2, "%s\n", pString);
    }
}

// =====================================================================================================================
static const char* FormatToString(
    ChNumFormat format)
{
    const char* FormatStrings[] =
    {
        "Undefined",
        "X1_Unorm",
        "X1_Uscaled",
        "X4Y4_Unorm",
        "X4Y4_Uscaled",
        "L4A4_Unorm",
        "X4Y4Z4W4_Unorm",
        "X4Y4Z4W4_Uscaled",
        "X5Y6Z5_Unorm",
        "X5Y6Z5_Uscaled",
        "X5Y5Z5W1_Unorm",
        "X5Y5Z5W1_Uscaled",
        "X1Y5Z5W5_Unorm",
        "X1Y5Z5W5_Uscaled",
        "X8_Unorm",
        "X8_Snorm",
        "X8_Uscaled",
        "X8_Sscaled",
        "X8_Uint",
        "X8_Sint",
        "X8_Srgb",
        "A8_Unorm",
        "L8_Unorm",
        "P8_Uint",
        "X8Y8_Unorm",
        "X8Y8_Snorm",
        "X8Y8_Uscaled",
        "X8Y8_Sscaled",
        "X8Y8_Uint",
        "X8Y8_Sint",
        "X8Y8_Srgb",
        "L8A8_Unorm",
        "X8Y8Z8W8_Unorm",
        "X8Y8Z8W8_Snorm",
        "X8Y8Z8W8_Uscaled",
        "X8Y8Z8W8_Sscaled",
        "X8Y8Z8W8_Uint",
        "X8Y8Z8W8_Sint",
        "X8Y8Z8W8_Srgb",
        "U8V8_Snorm_L8W8_Unorm",
        "X10Y11Z11_Float",
        "X11Y11Z10_Float",
        "X10Y10Z10W2_Unorm",
        "X10Y10Z10W2_Snorm",
        "X10Y10Z10W2_Uscaled",
        "X10Y10Z10W2_Sscaled",
        "X10Y10Z10W2_Uint",
        "X10Y10Z10W2_Sint",
        "X10Y10Z10W2Bias_Unorm",
        "U10V10W10_Snorm_A2_Unorm",
        "X16_Unorm",
        "X16_Snorm",
        "X16_Uscaled",
        "X16_Sscaled",
        "X16_Uint",
        "X16_Sint",
        "X16_Float",
        "L16_Unorm",
        "X16Y16_Unorm",
        "X16Y16_Snorm",
        "X16Y16_Uscaled",
        "X16Y16_Sscaled",
        "X16Y16_Uint",
        "X16Y16_Sint",
        "X16Y16_Float",
        "X16Y16Z16W16_Unorm",
        "X16Y16Z16W16_Snorm",
        "X16Y16Z16W16_Uscaled",
        "X16Y16Z16W16_Sscaled",
        "X16Y16Z16W16_Uint",
        "X16Y16Z16W16_Sint",
        "X16Y16Z16W16_Float",
        "X32_Uint",
        "X32_Sint",
        "X32_Float",
        "X32Y32_Uint",
        "X32Y32_Sint",
        "X32Y32_Float",
        "X32Y32Z32_Uint",
        "X32Y32Z32_Sint",
        "X32Y32Z32_Float",
        "X32Y32Z32W32_Uint",
        "X32Y32Z32W32_Sint",
        "X32Y32Z32W32_Float",
        "D16_Unorm_S8_Uint",
        "D32_Float_S8_Uint",
        "X9Y9Z9E5_Float",
        "Bc1_Unorm",
        "Bc1_Srgb",
        "Bc2_Unorm",
        "Bc2_Srgb",
        "Bc3_Unorm",
        "Bc3_Srgb",
        "Bc4_Unorm",
        "Bc4_Snorm",
        "Bc5_Unorm",
        "Bc5_Snorm",
        "Bc6_Ufloat",
        "Bc6_Sfloat",
        "Bc7_Unorm",
        "Bc7_Srgb",
        "Etc2X8Y8Z8_Unorm",
        "Etc2X8Y8Z8_Srgb",
        "Etc2X8Y8Z8W1_Unorm",
        "Etc2X8Y8Z8W1_Srgb",
        "Etc2X8Y8Z8W8_Unorm",
        "Etc2X8Y8Z8W8_Srgb",
        "Etc2X11_Unorm",
        "Etc2X11_Snorm",
        "Etc2X11Y11_Unorm",
        "Etc2X11Y11_Snorm",
        "AstcLdr4x4_Unorm",
        "AstcLdr4x4_Srgb",
        "AstcLdr5x4_Unorm",
        "AstcLdr5x4_Srgb",
        "AstcLdr5x5_Unorm",
        "AstcLdr5x5_Srgb",
        "AstcLdr6x5_Unorm",
        "AstcLdr6x5_Srgb",
        "AstcLdr6x6_Unorm",
        "AstcLdr6x6_Srgb",
        "AstcLdr8x5_Unorm",
        "AstcLdr8x5_Srgb",
        "AstcLdr8x6_Unorm",
        "AstcLdr8x6_Srgb",
        "AstcLdr8x8_Unorm",
        "AstcLdr8x8_Srgb",
        "AstcLdr10x5_Unorm",
        "AstcLdr10x5_Srgb",
        "AstcLdr10x6_Unorm",
        "AstcLdr10x6_Srgb",
        "AstcLdr10x8_Unorm",
        "AstcLdr10x8_Srgb",
        "AstcLdr10x10_Unorm",
        "AstcLdr10x10_Srgb",
        "AstcLdr12x10_Unorm",
        "AstcLdr12x10_Srgb",
        "AstcLdr12x12_Unorm",
        "AstcLdr12x12_Srgb",
        "AstcHdr4x4_Float",
        "AstcHdr5x4_Float",
        "AstcHdr5x5_Float",
        "AstcHdr6x5_Float",
        "AstcHdr6x6_Float",
        "AstcHdr8x5_Float",
        "AstcHdr8x6_Float",
        "AstcHdr8x8_Float",
        "AstcHdr10x5_Float",
        "AstcHdr10x6_Float",
        "AstcHdr10x8_Float",
        "AstcHdr10x10_Float",
        "AstcHdr12x10_Float",
        "AstcHdr12x12_Float",
        "X8Y8_Z8Y8_Unorm",
        "X8Y8_Z8Y8_Uscaled",
        "Y8X8_Y8Z8_Unorm",
        "Y8X8_Y8Z8_Uscaled",
        "AYUV",
        "UYVY",
        "VYUY",
        "YUY2",
        "YVY2",
        "YV12",
        "NV11",
        "NV12",
        "NV21",
        "P016",
        "P010",
    };

    static_assert(ArrayLen(FormatStrings) == static_cast<size_t>(ChNumFormat::Count),
                  "The number of formats has changed!");

    return FormatStrings[static_cast<size_t>(format)];
}

// =====================================================================================================================
static const char* ImageAspectToString(
    ImageAspect aspect)
{
    const char* ImageAspectStrings[] =
    {
        " - COLOR",
        " - DEPTH",
        " - STENCIL",
        " - Y",
        " - CbCr",
        " - Cb",
        " - Cr",
        " - YCbCr",
        "",
    };

    static_assert((ArrayLen(ImageAspectStrings) == static_cast<size_t>(ImageAspect::Count)),
                  "The number of image aspects has changed!");

    return ImageAspectStrings[static_cast<size_t>(aspect)];
}

// =====================================================================================================================
// Updates the current comment string for the executing barrier. This function is called from the layer callback and
// expects to only be called while a CmdBarrier call is executing in the lower layers.
void TargetCmdBuffer::UpdateCommentString(
    Developer::BarrierData* pData)
{
    char newBarrierComment[MaxCommentLength] = {};
    if (pData->hasTransition)
    {
        const ImageCreateInfo& imageInfo = pData->transition.imageInfo.pImage->GetImageCreateInfo();

        Snprintf(&newBarrierComment[0], MaxCommentLength,
            "Barrier: %ux%u %s - %s:",
            imageInfo.extent.width, imageInfo.extent.height,
            FormatToString(imageInfo.swizzledFormat.format),
            ImageAspectToString(pData->transition.imageInfo.subresRange.startSubres.aspect));

        AddBarrierString(&newBarrierComment[0]);
    }
    if (pData->operations.layoutTransitions.u16All != 0)
    {
        Snprintf(&newBarrierComment[0], MaxCommentLength, "Layout Transitions:");
        AddBarrierString(&newBarrierComment[0]);

        const char* LayoutTransitionStrings[] =
        {
            "Depth Stencil Expand",
            "HTile HiZ Range Expand",
            "Depth Stencil Resummarize",
            "DCC Decompress",
            "FMask Decompress",
            "Fast Clear Eliminate",
            "Fmask Color Expand",
            "Init Mask Ram",
            "Reserved",
            "Reserved",
            "Reserved",
            "Reserved",
            "Reserved",
            "Reserved",
            "Reserved",
            "Reserved",
        };

        static_assert(ArrayLen(LayoutTransitionStrings) ==
                      (sizeof(((Developer::BarrierOperations*)0)->layoutTransitions) * 8),
                      "Number of layout transitions has changed!");

        uint32 data      = pData->operations.layoutTransitions.u16All;
        uint32 lowSetBit = 0;
        while (BitMaskScanForward(&lowSetBit, data))
        {
            data &= ~(1 << lowSetBit);
            const char* pString = LayoutTransitionStrings[lowSetBit];
            Snprintf(&newBarrierComment[0], MaxCommentLength, " - %s", pString);
            AddBarrierString(&newBarrierComment[0]);
        }
    }
    if (pData->operations.pipelineStalls.u16All != 0)
    {
        Snprintf(&newBarrierComment[0], MaxCommentLength, "Pipeline Stalls:");
        AddBarrierString(&newBarrierComment[0]);

        const char* PipelineStallsStrings[] =
        {
            "Wait on EOP TS Bottom of Pipe",
            "VS Partial Flush",
            "PS Partial Flush",
            "CS Partial Flush",
            "PFP Sync ME",
            "Sync CPDMA",
            "Reserved",
            "Reserved",
            "Reserved",
            "Reserved",
            "Reserved",
            "Reserved",
            "Reserved",
            "Reserved",
            "Reserved",
            "Reserved",
        };

        static_assert(ArrayLen(PipelineStallsStrings) ==
                      (sizeof(((Developer::BarrierOperations*)0)->pipelineStalls) * 8),
                      "Number of pipeline stalls has changed!");

        uint32 data      = pData->operations.pipelineStalls.u16All;
        uint32 lowSetBit = 0;
        while (BitMaskScanForward(&lowSetBit, data))
        {
            data &= ~(1 << lowSetBit);
            const char* pString = PipelineStallsStrings[lowSetBit];
            Snprintf(&newBarrierComment[0], MaxCommentLength, " - %s", pString);
            AddBarrierString(&newBarrierComment[0]);
        }
    }
    if (pData->operations.caches.u16All != 0)
    {
        Snprintf(&newBarrierComment[0], MaxCommentLength, "Caches:");
        AddBarrierString(&newBarrierComment[0]);

        const char* CachesStrings[] =
        {
            "Invalidate TCP (L1 vector caches)",
            "Invalidate SQI$ (SQ instruction caches)",
            "Invalidate SQK$ (SQ constant caches - L1 scalar caches)",
            "Flush TCC (L2)",
            "Invalidate TCC (L2)",
            "Flush CB",
            "Invalidate CB",
            "Flush DB",
            "Invalidate DB",
            "Invalidate CB Metadata",
            "Flush CB Metadata",
            "Invalidate DB Metadata",
            "Flush DB Metadata",
            "Reserved",
            "Reserved",
            "Reserved",
        };

        static_assert(ArrayLen(CachesStrings) ==
                      (sizeof(((Developer::BarrierOperations*)0)->caches) * 8),
                      "Number of caches has changed!");

        uint32 data = pData->operations.caches.u16All;
        uint32 lowSetBit = 0;
        while (BitMaskScanForward(&lowSetBit, data))
        {
            data &= ~(1 << lowSetBit);
            const char* pString = CachesStrings[lowSetBit];
            Snprintf(&newBarrierComment[0], MaxCommentLength, " - %s", pString);
            AddBarrierString(&newBarrierComment[0]);
        }
    }
}

// =====================================================================================================================
// Issue commands on a target command buffer needed to begin a section of work to be profiled.
void TargetCmdBuffer::BeginSample(
    Queue*   pQueue,
    LogItem* pLogItem,
    bool     pipeStats,
    bool     perfExp)
{
    const GpuUtil::GpaSampleConfig& config = pQueue->GetGpaSessionSampleConfig();

    pLogItem->pGpaSession   = m_pGpaSession;            // Save the session for later end it.
    pLogItem->gpaSampleId   = GpuUtil::InvalidSampleId; // Initialize sample id.
    pLogItem->gpaSampleIdTs = GpuUtil::InvalidSampleId; // Initialize sample id.
    pLogItem->gpaSampleIdQuery = GpuUtil::InvalidSampleId; // Initialize sample id.

    // If requested, surround this universal/compute queue operation a pipeline stats query.
    if (pipeStats)
    {
        if ((m_queueType == QueueTypeUniversal) || (m_queueType == QueueTypeCompute))
        {
            GpuUtil::GpaSampleConfig queryConfig = {};
            queryConfig.type                     = GpuUtil::GpaSampleType::Query;
            pLogItem->gpaSampleIdQuery           = m_pGpaSession->BeginSample(this, queryConfig);
        }
        else
        {
            // Pipeline stats queries are not currently supported on anything but the Universal/compute engine.
            pLogItem->errors.pipeStatsUnsupported = 1;
        }
    }

    if (perfExp)
    {
        if ((m_queueType == QueueTypeUniversal) || (m_queueType == QueueTypeCompute))
        {
            pLogItem->gpaSampleId = m_pGpaSession->BeginSample(this, config);
        }
        else
        {
            // Perf experiments are not currently supported on anything but the Universal/compute engine.
            pLogItem->errors.perfExpUnsupported = 1;
        }
    }

    if (m_supportTimestamps)
    {
        GpuUtil::GpaSampleConfig tsConfig = {};
        tsConfig.type                     = GpuUtil::GpaSampleType::Timing;
        tsConfig.timing.preSample         = config.timing.preSample;
        tsConfig.timing.postSample        = config.timing.postSample;
        pLogItem->gpaSampleIdTs           = m_pGpaSession->BeginSample(this, tsConfig);
    }
}

// =====================================================================================================================
// Issue commands on a target command buffer needed to end a section of work to be profiled.
void TargetCmdBuffer::EndSample(
    Queue*         pQueue,
    const LogItem* pLogItem)
{
    // End the timestamp sample.
    if (pQueue->HasValidGpaSample(pLogItem, GpuUtil::GpaSampleType::Timing))
    {
        pLogItem->pGpaSession->EndSample(this, pLogItem->gpaSampleIdTs);
    }

    // End the counter/trace sample.
    if (pQueue->HasValidGpaSample(pLogItem, GpuUtil::GpaSampleType::Cumulative))
    {
        pLogItem->pGpaSession->EndSample(this, pLogItem->gpaSampleId);
    }

    // End the query sample.
    if (pQueue->HasValidGpaSample(pLogItem, GpuUtil::GpaSampleType::Query))
    {
        pLogItem->pGpaSession->EndSample(this, pLogItem->gpaSampleIdQuery);
    }
}

// =====================================================================================================================
// Begin a GpaSession for the current target command buffer.
Result TargetCmdBuffer::BeginGpaSession(
    Queue* pQueue)
{
    Result result = Result::Success;

    // Get an unused GPA session
    result = pQueue->AcquireGpaSession(&m_pGpaSession);
    if (result == Result::Success)
    {
        GpuUtil::GpaSessionBeginInfo info = {};
        result = m_pGpaSession->Begin(info);
    }

    return result;
}

// =====================================================================================================================
// End the GpaSession for current target command buffer.
Result TargetCmdBuffer::EndGpaSession(
    LogItem* pLogItem)
{
    return pLogItem->pGpaSession->End(this);
}

} // GpuProfiler
} // Pal
