/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palInlineFuncs.h"
#include "palQueue.h"

namespace Pal
{

// Identifies a specific ICmdBuffer function call.  One enum per interface in ICmdBuffer.
enum class CmdBufCallId : uint32
{
    Begin,
    End,
    CmdBindPipeline,
    CmdBindMsaaState,
    CmdBindColorBlendState,
    CmdBindDepthStencilState,
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
    CmdSetViewports,
    CmdSetScissorRects,
    CmdSetGlobalScissor,
    CmdBarrier,
    CmdWaitRegisterValue,
    CmdWaitMemoryValue,
    CmdWaitBusAddressableMemoryMarker,
    CmdDraw,
    CmdDrawOpaque,
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
    CmdCommentString,
    CmdXdmaWaitFlipPending,
    CmdCopyMemoryToTiledImage,
    CmdCopyTiledImageToMemory,
    CmdCopyImageToPackedPixelImage,
    CmdStartGpuProfilerLogging,
    CmdStopGpuProfilerLogging,
    CmdSetViewInstanceMask,
    CmdSetHiSCompareState0,
    CmdSetHiSCompareState1,
    Count
};

// Table converting CmdBufCallId enums to strings.
static const char* CmdBufCallIdStrings[] =
{
    "Begin()",
    "End()",
    "CmdBindPipeline()",
    "CmdBindMsaaState()",
    "CmdBindColorBlendState()",
    "CmdBindDepthStencilState()",
    "CmdBindIndexData()",
    "CmdBindTargets()",
    "CmdBindStreamOutTargets()",
    "CmdBindBorderColorPalette()",
    "CmdSetUserData()",
    "CmdSetIndirectUserData()",
    "CmdSetIndirectUserDataWatermark()",
    "CmdSetBlendConst()",
    "CmdSetInputAssemblyState()",
    "CmdSetTriangleRasterState()",
    "CmdSetPointLineRasterState()",
    "CmdSetDepthBiasState()",
    "CmdSetDepthBounds()",
    "CmdSetStencilRefMasks()",
    "CmdSetMsaaQuadSamplePattern()",
    "CmdSetViewports()",
    "CmdSetScissorRects()",
    "CmdSetGlobalScissor()",
    "CmdBarrier()",
    "CmdWaitRegisterValue()",
    "CmdWaitMemoryValue()",
    "CmdWaitBusAddressableMemoryMarker()",
    "CmdDraw()",
    "CmdDrawOpaque()",
    "CmdDrawIndexed()",
    "CmdDrawIndirectMulti()",
    "CmdDrawIndexedIndirectMulti()",
    "CmdDispatch()",
    "CmdDispatchIndirect()",
    "CmdDispatchOffset()",
    "CmdUpdateMemory()",
    "CmdUpdateBusAddressableMemoryMarker()",
    "CmdFillMemory()",
    "CmdCopyMemory()",
    "CmdCopyTypedBuffer()",
    "CmdCopyRegisterToMemory()",
    "CmdCopyImage()",
    "CmdScaledCopyImage()",
    "CmdColorSpaceConversionCopy()",
    "CmdCloneImageData()",
    "CmdCopyMemoryToImage()",
    "CmdCopyImageToMemory()",
    "CmdClearColorBuffer()",
    "CmdClearBoundColorTargets()",
    "CmdClearColorImage()",
    "CmdClearBoundDepthStencilTargets()",
    "CmdClearDepthStencil()",
    "CmdClearBufferView()",
    "CmdClearImageView()",
    "CmdResolveImage()",
    "CmdSetEvent()",
    "CmdResetEvent()",
    "CmdPredicateEvent()",
    "CmdMemoryAtomic()",
    "CmdResetQueryPool()",
    "CmdBeginQuery()",
    "CmdEndQuery()",
    "CmdResolveQuery()",
    "CmdSetPredication()",
    "CmdWriteTimestamp()",
    "CmdWriteImmediate()",
    "CmdLoadGds()",
    "CmdStoreGds()",
    "CmdUpdateGds()",
    "CmdFillGds()",
    "CmdLoadBufferFilledSizes()",
    "CmdSaveBufferFilledSizes()",
    "CmdLoadCeRam()",
    "CmdWriteCeRam()",
    "CmdDumpCeRam()",
    "CmdExecuteNestedCmdBuffers()",
    "CmdExecuteIndirectCmds()",
    "CmdIf()",
    "CmdElse()",
    "CmdEndIf()",
    "CmdWhile()",
    "CmdEndWhile()",
    "CmdFlglSync()",
    "CmdFlglEnable()",
    "CmdFlglDisable()",
    "CmdBeginPerfExperiment()",
    "CmdUpdatePerfExperimentSqttTokenMask()",
    "CmdEndPerfExperiment()",
    "CmdInsertTraceMarker()",
    "CmdInsertRgpTraceMarker()",
    "CmdSaveComputeState()",
    "CmdRestoreComputeState()",
    "CmdSetUserClipPlanes()",
    "CmdCommentString()",
    "CmdXdmaWaitFlipPending()",
    "CmdCopyMemoryToTiledImage()",
    "CmdCopyTiledImageToMemory()",
    "CmdCopyImageToPackedPixelImage()",
    "CmdStartGpuProfilerLogging()",
    "CmdStopGpuProfilerLogging()",
    "CmdSetViewInstanceMask()",
    "CmdSetHiSCompareState0()",
    "CmdSetHiSCompareState1()",
};

static_assert(Util::ArrayLen(CmdBufCallIdStrings) == static_cast<uint32>(CmdBufCallId::Count),
              "Missing entry in CmdBufCallIdStrings.");

// Identifies a specific IQueue function call.  One enum per interface in IQueue.
enum class QueueCallId : uint32
{
    Submit,
    WaitIdle,
    SignalQueueSemaphore,
    WaitQueueSemaphore,
    PresentDirect,
    PresentSwapChain,
    Delay,
    RemapVirtualMemoryPages,
    CopyVirtualMemoryPageMappings,
    Count
};

// Table converting QueueCallId enums to strings.
static const char* QueueCallIdStrings[] =
{
    "Submit()",
    "WaitIdle()",
    "SignalQueueSemaphore()",
    "WaitQueueSemaphore()",
    "PresentDirect()",
    "PresentSwapChain()",
    "Delay()",
    "RemapVirtualMemoryPages()",
    "CopyVirtualMemoryPageMappings()"
};

static_assert(Util::ArrayLen(QueueCallIdStrings) == static_cast<uint32>(QueueCallId::Count),
              "Missing entry in QueueCallIdStrings.");

} // Pal
