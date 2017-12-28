/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "gpuUtil/sqtt_file_format.h"
#include "palDequeImpl.h"
#include "palGpaSession.h"

using namespace Util;

namespace Pal
{
namespace GpuProfiler
{

// Translation table for obtaining Sqtt thread trace version given the Pal::GfxIpLevel.
static const SqttVersion GfxipToSqttVersionTranslation[static_cast<uint32>(Pal::GfxIpLevel::Count)] =
{
    SQTT_VERSION_NONE,
    SQTT_VERSION_2_0,  // Gfxip 6
    SQTT_VERSION_2_1,  // Gfxip 7
    SQTT_VERSION_2_2,  // Gfxip 8
    SQTT_VERSION_2_2,  // Gfxip 8.1
    SQTT_VERSION_2_3   // Gfxip 9
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 339
    "CmdStoreMsaaQuadSamplePattern()",
    "CmdLoadMsaaQuadSamplePattern()",
#endif
    "CmdSetViewports()",
    "CmdSetScissorRects()",
    "CmdSetGlobalScissor()",
    "CmdBarrier()",
    "CmdWaitRegisterValue()",
    "CmdWaitMemoryValue()",
    "CmdWaitBusAddressableMemoryMarker()",
    "CmdDraw()",
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
    "CmdBeginPerfExperiment()",
    "CmdUdpatePerfExperiment()",
    "CmdEndPerfExperiment()",
    "CmdInsertTraceMarker()",
    "CmdInsertRgpTraceMarker()",
    "CmdSetUserClipPlanes()",
    "CmdCommentString()",
    "CmdXdmaWaitFlipPending()",
    "CmdCopyMemoryToTiledImage()",
    "CmdCopyTiledImageToMemory()",
    "CmdCopyImageToPackedPixelImage()",
    "CmdFlglSync()",
    "CmdFlglEnable()",
    "CmdFlglDisable()",
    "CmdStartGpuProfilerLogging()",
    "CmdStopGpuProfilerLogging()",
    "CmdSetViewInstanceMask()",
};

static_assert((sizeof(CmdBufCallIdStrings)/sizeof(CmdBufCallIdStrings[0])) == static_cast<uint32>(CmdBufCallId::Count),
              "Missing entry in CmdBufCallIdStrings.");

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

static_assert((sizeof(QueueCallIdStrings)/sizeof(QueueCallIdStrings[0])) == static_cast<uint32>(QueueCallId::Count),
              "Missing entry in QueueCallIdStrings.");

// =====================================================================================================================
// Writes .csv entries to file corresponding to the first count items in the m_logItems deque.  The caller guarantees
// that all of these calls are idle.
void Queue::OutputLogItemsToFile(
    size_t count)
{
    PAL_ASSERT(count <= m_logItems.NumElements());

    // Log items from a nested command buffer are flattened so that they appear the same as regular command buffer
    // calls.  activeCmdBufs tracks how many "open" command buffers there are - 0 during queue calls, 1 inside a submit,
    // and 2 inside a nested command buffer.  m_curLogCmdBufIdx is used to log which command buffer we are logging out
    // of the root-level command buffers submitted during a particular frame.  This is incremented anytime a root-level
    // command buffer is ended.
    uint32 activeCmdBufs = 0;

    for (uint32 i = 0; i < count; i++)
    {
        LogItem logItem = { };
        m_logItems.PopFront(&logItem);

        // The fence bundled to this submit wave should promise GpaSession ready.
        PAL_ASSERT((logItem.pGpaSession == nullptr) || logItem.pGpaSession->IsReady());

        if (logItem.type == CmdBufferCall)
        {
            if (logItem.cmdBufCall.callId == CmdBufCallId::Begin)
            {
                PAL_ASSERT(activeCmdBufs <= 1);
                activeCmdBufs++;

                m_curLogSqttIdx = 0;
            }

            // Add a "- " before command buffer calls made in a nested command buffer to differentiate them from calls
            // made in the root command buffer.
            const char* pNestedCmdBufPrefix = (activeCmdBufs == 2) ? "- " : "";

            // If we have received a command buffer call without having received a queue call for this frame,
            // we are using the dynamic start/stop of GPU profiling.  Open a new log file in this case.
            if ((m_logFile.IsOpen() == false) || (m_curLogFrame != logItem.frameId))
            {
                OpenLogFile(logItem.frameId);
                m_curLogFrame = logItem.frameId;
                m_curLogCmdBufIdx = 0;
            }

            OutputCmdBufCallToFile(logItem, pNestedCmdBufPrefix);

            if (logItem.cmdBufCall.callId == CmdBufCallId::End)
            {
                PAL_ASSERT((activeCmdBufs > 0) && (activeCmdBufs <= 2));
                m_curLogCmdBufIdx += (--activeCmdBufs == 0) ? 1 : 0;
            }
        }
        else if (logItem.type == QueueCall)
        {
            // If this is the first queue call for a new frame, open a new log file.
            if ((m_logFile.IsOpen() == false) || (m_curLogFrame != logItem.frameId))
            {
                OpenLogFile(logItem.frameId);
                m_curLogFrame = logItem.frameId;
                m_curLogCmdBufIdx = 0;
            }

            OutputQueueCallToFile(logItem);
        }
        else if (logItem.type == Frame)
        {
            m_curLogFrame = logItem.frameId;
            OutputFrameToFile(logItem);
        }
    }

    // Flush any buffered log writes to disk.  This is helpful for examining log files while an app is running or
    // dealing with app/driver crashes after the captured frame.
    m_logFile.Flush();
}

// =====================================================================================================================
// Opens and initializes a log file for the specified frame.
void Queue::OpenLogFile(
    uint32 frameId)
{
    const auto& settings = m_pDevice->ProfilerSettings();

    m_logFile.Close();

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 303
    const char* pEngineTypeStrings[] =
    {
        "Gfx",
        "Ace",
        "XAce",
        "Dma",
        "Timer",
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 315
        "HpUniversal",
        "HpGfxOnly",
#endif
    };
#else
    const char* pEngineTypeStrings[] = { "Gfx", "Ace", "XAce", "Dma", "Timer", "Vce", "Uvd" };
#endif
    static_assert((sizeof(pEngineTypeStrings) / sizeof(pEngineTypeStrings[0])) == EngineTypeCount,
                 "Missing entry in pEngineTypeStrings.");

    // Build a file name for this frame's log file.  It will have the pattern frameAAAAAADevBEngCD-EE.csv, where:
    //     - AAAAAA: Frame number.
    //     - B:      Device index (mostly relevant when profiling MGPU systems).
    //     - C:      Engine type (U = universal, C = compute, D = DMA, and T = timer).
    //     - D:      Engine index (for cases like compute/DMA where there are multiple instances of the same engine).
    //     - EE:     Queue ID (there can be multiple IQueue objects created for the same engine instance).
    char tempString[512];
    Snprintf(&tempString[0],
             sizeof(tempString),
             "%s/%s/frame%06uDev%uEng%s%u-%02u.csv",
             settings.gpuProfilerLogDirectory,
             static_cast<const Platform*>(m_pDevice->GetPlatform())->LogDirName(),
             frameId,
             m_pDevice->Id(),
             pEngineTypeStrings[static_cast<uint32>(m_engineType)],
             m_engineIndex,
             m_queueId);

    Result result = m_logFile.Open(&tempString[0], FileAccessWrite);
    PAL_ASSERT(result == Result::Success);

    // Write the CSV column headers to the newly opened file.
    const char* pCsvHeader = "Queue Call, CmdBuffer Index, CmdBuffer Call, Start Clock, End Clock, Time (us) "
                             "[Frequency: %llu], PipelineHash, CompilerHash, VS/CS, HS, DS, GS, PS, "
                             "Verts/ThreadGroups, Instances, Comments, ";
    Snprintf(&tempString[0], sizeof(tempString), pCsvHeader, m_pDevice->TimestampFreq());
    m_logFile.Write(&tempString[0], strlen(&tempString[0]));

    // Add some additional column headers based on enabled profiling features.
    if (settings.gpuProfilerRecordPipelineStats)
    {
        const char* pCsvPipelineStatsHeader = "IaVertices, IaPrimitives, VsInvocations, GsInvocations, "
                                              "GsPrimitives, CInvocations, CPrimitives, PsInvocations, "
                                              "HsInvocations, DsInvocations, CsInvocations, ";
        m_logFile.Write(pCsvPipelineStatsHeader, strlen(pCsvPipelineStatsHeader));
    }

    const uint32 numGlobalPerfCounters = m_pDevice->NumGlobalPerfCounters();
    if (numGlobalPerfCounters > 0)
    {
        for (uint32 i = 0; i < numGlobalPerfCounters; i++)
        {
            const auto& counter = m_pDevice->GlobalPerfCounters()[i];
            if (settings.gpuProfilerGlobalPerfCounterPerInstance)
            {
                for (uint32 j = 0; j < counter.instanceCount; j++)
                {
                    m_logFile.Printf("%s_INSTANCE%d, ", &counter.name[0], j);
                }
            }
            else
            {
                m_logFile.Printf("%s, ", &counter.name[0]);
            }
        }
    }

    if (m_pDevice->GetProfilerMode() > GpuProfilerSqttOff)
    {
        m_logFile.Printf("ThreadTraceId, ");
    }

    m_logFile.Printf("\n");
}

// =====================================================================================================================
// Opens and initializes a SQ thread trace file.
void Queue::OpenSqttFile(
    uint32         shaderEngineId,
    uint32         computeUnitId,
    uint32         traceId,
    File*          pFile,
    const LogItem& logItem)
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 303
    const char* pEngineTypeStrings[] =
    {
        "Gfx",
        "Ace",
        "XAce",
        "Dma",
        "Timer",
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 315
        "HpUniversal",
        "HpGfxOnly",
#endif
    };
#else
    const char* pEngineTypeStrings[] = { "Gfx", "Ace", "XAce", "Dma", "Timer", "Vce", "Uvd" };
#endif
    static_assert((sizeof(pEngineTypeStrings) / sizeof(pEngineTypeStrings[0])) == EngineTypeCount,
        "Missing entry in pEngineTypeStrings.");

    // CRC Info
    constexpr uint32 CrcInfoSize = 256;
    char crcInfo[CrcInfoSize];
    memset(crcInfo, 0, CrcInfoSize);
    if (CmdBufferCall == logItem.type)
    {
        if (logItem.cmdBufCall.flags.draw == 1)
        {
            Snprintf(crcInfo, CrcInfoSize, "_DRAW_PIPELINE%.16I64x",
                     logItem.cmdBufCall.draw.pipelineInfo.compilerHash);
        }
        else if (logItem.cmdBufCall.flags.dispatch == 1)
        {
            Snprintf(crcInfo, CrcInfoSize, "_DISPATCH_PIPELINE%.16I64x",
                     logItem.cmdBufCall.dispatch.pipelineInfo.compilerHash);
        }
    }

    // frameAAAAAADevBEngCD-EE.SqttCmdBufFTraceGSeHCuUI.out, where:
    //     - AAAAAA: Frame number.
    //     - B:      Device index (mostly relevant when profiling MGPU systems).
    //     - C:      Engine type (U = universal, C = compute, D = DMA, and T = timer).
    //     - D:      Engine index (for cases like compute/DMA where there are multiple instances of the same engine).
    //     - EE:     Queue ID (there can be multiple IQueue objects created for the same engine instance).
    //     - F:      Command buffer ID.
    //     - G:      Thread-trace ID for correlation between per-Draw output and SQTT logs.
    //     - H:      Shader engine ID.
    //     - U:      Compute unit ID.
    //     - I:      Concatenation of shader IDs bound (for draw/dispatch calls only).
    char logFilePath[512];
    Snprintf(&logFilePath[0],
             sizeof(logFilePath),
             "%s/%s/frame%06uDev%uEng%s%u-%02u.SqttCmdBuf%uTrace%uSe%uCu%u%s.out",
             m_pDevice->ProfilerSettings().gpuProfilerLogDirectory,
             static_cast<const Platform*>(m_pDevice->GetPlatform())->LogDirName(),
             m_curLogFrame,
             m_pDevice->Id(),
             pEngineTypeStrings[static_cast<uint32>(m_engineType)],
             m_engineIndex,
             m_queueId,
             m_curLogCmdBufIdx,
             traceId,
             shaderEngineId,
             computeUnitId,
             crcInfo);

    Result result = pFile->Open(&logFilePath[0], FileAccessWrite);
    PAL_ASSERT(result == Result::Success);
}

// =====================================================================================================================
// Opens and initializes an SQ thread trace file for consumption by RGP (Radeon GPU Profiler).
void Queue::OutputRgpFile(
    const GpuUtil::GpaSession& gpaSession,
    uint32                     sampleId)
{
    // Open a binary file named like frame018479.rgp.
    char logFilePath[512];
    Snprintf(&logFilePath[0],
             sizeof(logFilePath),
             "%s/%s/frame%06u.rgp",
             m_pDevice->ProfilerSettings().gpuProfilerLogDirectory,
             static_cast<const Platform*>(m_pDevice->GetPlatform())->LogDirName(),
             m_curLogFrame);

    File file;
    Result result = file.Open(&logFilePath[0], FileAccessBinary | FileAccessWrite);

    void*  pResult  = nullptr;
    size_t dataSize = 0;
    if (result == Result::Success)
    {
        result = gpaSession.GetResults(sampleId, &dataSize, pResult);
        PAL_ASSERT(dataSize != 0);
    }

    if (result == Result::Success)
    {
        pResult = static_cast<void*>(PAL_MALLOC(dataSize, m_pDevice->GetPlatform(), AllocInternal));
        if (pResult == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    if (result == Result::Success)
    {
        result = gpaSession.GetResults(sampleId, &dataSize, pResult);
    }

    if (result == Result::Success)
    {
        file.Write(pResult, dataSize);
    }

    PAL_SAFE_FREE(pResult, m_pDevice->GetPlatform());

    file.Close();
}

// =====================================================================================================================
// Outputs details of a single queue call to the log file.
void Queue::OutputQueueCallToFile(
    const LogItem& logItem)
{
    PAL_ASSERT(logItem.type == QueueCall);

    m_logFile.Printf("%s, , , , , , , , , , , , , , , , ",
                     QueueCallIdStrings[static_cast<uint32>(logItem.queueCall.callId)]);

    if (m_pDevice->ProfilerSettings().gpuProfilerRecordPipelineStats)
    {
        m_logFile.Printf(", , , , , , , , , , , ");
    }

    for (uint32 i = 0; i < m_numReportedPerfCounters; i++)
    {
        m_logFile.Printf(", ");
    }

    m_logFile.Printf("\n");
}

//======================================================================================================================
// Outputs details of a single command buffer call to the log file.
void Queue::OutputCmdBufCallToFile(
    const LogItem& logItem,
    const char*    pNestedCmdBufPrefix)
{
    PAL_ASSERT(logItem.type == CmdBufferCall);
    PAL_ASSERT(m_logFile.IsOpen());

    constexpr uint32 CsIdx = static_cast<uint32>(ShaderType::Compute);
    constexpr uint32 VsIdx = static_cast<uint32>(ShaderType::Vertex);
    constexpr uint32 HsIdx = static_cast<uint32>(ShaderType::Hull);
    constexpr uint32 DsIdx = static_cast<uint32>(ShaderType::Domain);
    constexpr uint32 GsIdx = static_cast<uint32>(ShaderType::Geometry);
    constexpr uint32 PsIdx = static_cast<uint32>(ShaderType::Pixel);

    const auto& cmdBufItem = logItem.cmdBufCall;

    m_logFile.Printf(", %d, %s%s, ",
                     m_curLogCmdBufIdx,
                     pNestedCmdBufPrefix,
                     CmdBufCallIdStrings[static_cast<uint32>(cmdBufItem.callId)]);

    OutputTimestampsToFile(logItem);

    // Print any draw/dispatch specific info (shader hashes, etc.).
    if (cmdBufItem.flags.draw)
    {
        m_logFile.Printf("0x%016llx, 0x%016llx, 0x%016llx%016llx, 0x%016llx%016llx, 0x%016llx%016llx,"
                         " 0x%016llx%016llx, 0x%016llx%016llx, %u, %u, , ",
                         cmdBufItem.draw.pipelineInfo.pipelineHash,
                         cmdBufItem.draw.pipelineInfo.compilerHash,
                         cmdBufItem.draw.pipelineInfo.shader[VsIdx].hash.upper,
                         cmdBufItem.draw.pipelineInfo.shader[VsIdx].hash.lower,
                         cmdBufItem.draw.pipelineInfo.shader[HsIdx].hash.upper,
                         cmdBufItem.draw.pipelineInfo.shader[HsIdx].hash.lower,
                         cmdBufItem.draw.pipelineInfo.shader[DsIdx].hash.upper,
                         cmdBufItem.draw.pipelineInfo.shader[DsIdx].hash.lower,
                         cmdBufItem.draw.pipelineInfo.shader[GsIdx].hash.upper,
                         cmdBufItem.draw.pipelineInfo.shader[GsIdx].hash.lower,
                         cmdBufItem.draw.pipelineInfo.shader[PsIdx].hash.upper,
                         cmdBufItem.draw.pipelineInfo.shader[PsIdx].hash.lower,
                         cmdBufItem.draw.vertexCount,
                         cmdBufItem.draw.instanceCount);
    }
    else if (cmdBufItem.flags.dispatch)
    {
        m_logFile.Printf("0x%016llx, 0x%016llx, 0x%016llx%016llx, , , , , %u, , , ",
                         cmdBufItem.dispatch.pipelineInfo.pipelineHash,
                         cmdBufItem.dispatch.pipelineInfo.compilerHash,
                         cmdBufItem.draw.pipelineInfo.shader[CsIdx].hash.upper,
                         cmdBufItem.draw.pipelineInfo.shader[CsIdx].hash.lower,
                         cmdBufItem.dispatch.threadGroupCount);
    }
    else if (cmdBufItem.flags.barrier)
    {
        m_logFile.Printf(", , , , , , , , ,\"%s\", ",
                         (cmdBufItem.barrier.pComment != nullptr) ? cmdBufItem.barrier.pComment : "");
    }
    else if (cmdBufItem.flags.comment)
    {
        m_logFile.Printf(", , , , , , , , ,\"%s\", ", cmdBufItem.comment.string);
    }
    else
    {
        m_logFile.Printf(", , , , , , , , , , ");
    }

    OutputPipelineStatsToFile(logItem);
    OutputGlobalPerfCountersToFile(logItem);
    OutputSqThreadTraceToFile(logItem);

    m_logFile.Printf("\n");
}

//======================================================================================================================
// Outputs details of a frame to the log file (used only for frame-granularity profiling).
void Queue::OutputFrameToFile(
    const LogItem& logItem)
{
    const auto& settings = m_pDevice->ProfilerSettings();

    if (m_logFile.IsOpen() == false)
    {
        // Build a file name for this frame's log file.
        char tempString[512];
        Snprintf(&tempString[0],
                 sizeof(tempString),
                 "%s/%s/frameLog.csv",
                 settings.gpuProfilerLogDirectory,
                 static_cast<const Platform*>(m_pDevice->GetPlatform())->LogDirName());

        Result result = m_logFile.Open(&tempString[0], FileAccessWrite);
        PAL_ASSERT(result == Result::Success);

        // Write the CSV column headers to the newly opened file.
        const char* pCsvHeader = "Frame #, Start Clock, End Clock, Time (us) [Frequency: %llu], ";
        Snprintf(&tempString[0], sizeof(tempString), pCsvHeader, m_pDevice->TimestampFreq());
        m_logFile.Write(&tempString[0], strlen(&tempString[0]));
        const uint32 numGlobalPerfCounters = m_pDevice->NumGlobalPerfCounters();
        if (numGlobalPerfCounters > 0)
        {
            for (uint32 i = 0; i < numGlobalPerfCounters; i++)
            {
                const auto& counter = m_pDevice->GlobalPerfCounters()[i];
                if (settings.gpuProfilerGlobalPerfCounterPerInstance)
                {
                    for (uint32 j = 0; j < counter.instanceCount; j++)
                    {
                        m_logFile.Printf("%s_INSTANCE%d, ", &counter.name[0], j);
                    }
                }
                else
                {
                    m_logFile.Printf("%s, ", &counter.name[0]);
                }
            }
        }

        if (m_pDevice->GetProfilerMode() > GpuProfilerSqttOff)
        {
            m_logFile.Printf("ThreadTraceId, ");
        }

        m_logFile.Printf("\n");
    }

    m_logFile.Printf("%u, ", logItem.frameId);

    OutputTimestampsToFile(logItem);
    OutputGlobalPerfCountersToFile(logItem);
    OutputSqThreadTraceToFile(logItem);

    m_logFile.Printf("\n");
    m_logFile.Flush();
}

// =====================================================================================================================
// Output the portion of a .csv with the start/end clock values and time elapsed.  Shared code by all profile
// granularities.
void Queue::OutputTimestampsToFile(
    const LogItem& logItem)
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 355
    if (HasValidGpaSample(&logItem, true))
#else
    if (HasValidGpaSample(&logItem, GpuUtil::GpaSampleType::Timing))
#endif
    {
        uint64 pResult[2] = {};
        Result result     = logItem.pGpaSession->GetResults(logItem.gpaSampleIdTs,
                                                            nullptr,
                                                            pResult);

        m_logFile.Printf("%llu, %llu, ", pResult[0], pResult[1]);

        bool hideElapsedTime = (m_pDevice->ProfilerSettings().gpuProfilerGranularity == GpuProfilerGranularityDraw) &&
                               (logItem.type == LogItemType::CmdBufferCall) &&
                               (logItem.cmdBufCall.callId == CmdBufCallId::Begin);

        // Print the elapsed time for this call if pre-call/post-call timestamps were inserted.
        if (!hideElapsedTime)
        {
            const double tsDiff   = static_cast<double>(pResult[1] - pResult[0]);
            const double timeInUs = 1000000 * tsDiff / m_pDevice->TimestampFreq();

            m_logFile.Printf("%.2lf, ", timeInUs);
        }
        else
        {
            m_logFile.Printf(", ");
        }
    }
    else
    {
        m_logFile.Printf(", , , ");
    }
}

// =====================================================================================================================
// Output pipeline stats to file.  Only supported by draw/cmdbuf granularities.
void Queue::OutputPipelineStatsToFile(
    const LogItem& logItem)
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 355
    if (logItem.pPipeStatsQuery != nullptr)
#else
    if (HasValidGpaSample(&logItem, GpuUtil::GpaSampleType::Query))
#endif
    {
        uint64 pipelineStats[11] = {};
        size_t pipelineStatsSize = sizeof(pipelineStats);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 355
        QueryResultFlags flags = static_cast<QueryResultFlags>(QueryResult64Bit | QueryResultWait);
        const Result result = logItem.pPipeStatsQuery->GetResults(flags,
                                                                  QueryType::PipelineStats,
                                                                  0,
                                                                  1,
                                                                  &pipelineStatsSize,
                                                                  &pipelineStats[0],
                                                                  0);
#else
        const Result result = logItem.pGpaSession->GetResults(logItem.gpaSampleIdQuery,
                                                              &pipelineStatsSize,
                                                              pipelineStats);
#endif
        PAL_ASSERT(result == Result::Success);
        PAL_ASSERT(pipelineStatsSize == sizeof(pipelineStats));

        // PAL hardcodes the layout of the return pipeline stats values based on the client, leading to different
        // versions of this code to a uniform log layout.
        m_logFile.Printf("%llu, %llu, %llu, %llu, %llu, %llu, %llu, %llu, %llu, %llu, %llu, ", pipelineStats[0],
                         pipelineStats[1], pipelineStats[2], pipelineStats[3], pipelineStats[4], pipelineStats[5],
                         pipelineStats[6], pipelineStats[7], pipelineStats[8], pipelineStats[9], pipelineStats[10]);
    }
    else if (m_pDevice->ProfilerSettings().gpuProfilerRecordPipelineStats)
    {
        m_logFile.Printf(", , , , , , , , , , , ");
    }
}

// =====================================================================================================================
// Dump the enabled global perf counters to file.  Shared code between draw/cmdbuf and per-frame profile granularities.
void Queue::OutputGlobalPerfCountersToFile(
    const LogItem& logItem)
{
    const auto&              settings              = m_pDevice->ProfilerSettings();
    const GlobalPerfCounter* pGlobalPerfCounters   = m_pDevice->GlobalPerfCounters();
    const uint32             numGlobalPerfCounters = m_pDevice->NumGlobalPerfCounters();

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 355
    if ((numGlobalPerfCounters > 0) && HasValidGpaSample(&logItem, false))
#else
    if ((numGlobalPerfCounters > 0) && HasValidGpaSample(&logItem, GpuUtil::GpaSampleType::Cumulative))
#endif
    {
        void*  pResult  = nullptr;
        size_t dataSize = 0;

        Result result = logItem.pGpaSession->GetResults(logItem.gpaSampleId,
                                                        &dataSize,
                                                        pResult);
        PAL_ASSERT(dataSize != 0);

        if (result == Result::Success)
        {
            pResult = static_cast<void*>(PAL_MALLOC(dataSize, m_pDevice->GetPlatform(), AllocInternal));
            if (pResult == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
        }

        if (result == Result::Success)
        {
            result = logItem.pGpaSession->GetResults(logItem.gpaSampleId,
                                                             &dataSize,
                                                             pResult);
        }

        if (result == Result::Success)
        {
            PAL_ASSERT(m_pGlobalPerfCounterValues != nullptr);

            // Zero out the reported value for each counter.  The results from each instance of that counter will be
            // accumulated into this array.
            memset(m_pGlobalPerfCounterValues, 0, sizeof(uint64) * m_numReportedPerfCounters);

            PerfExperimentProperties perfExpProps;
            result = m_pDevice->GetPerfExperimentProperties(&perfExpProps);

            // Determine which user-specified counter this sample corresponds to.
            uint32 instanceIndex = 0;
            if (settings.gpuProfilerGlobalPerfCounterPerInstance)
            {
               for (uint32 i = 0; i < m_numReportedPerfCounters; i++)
               {
                    m_pGlobalPerfCounterValues[i] += static_cast<uint64*>(pResult)[instanceIndex++];
               }
            }
            else
            {
               for (uint32 i = 0; i < m_numReportedPerfCounters; i++)
               {
                   const auto& blockProps = perfExpProps.blocks[static_cast<uint32>(pGlobalPerfCounters[i].block)];

                   for (uint32 j = 0; j < blockProps.instanceCount; j++)
                   {
                       m_pGlobalPerfCounterValues[i] += static_cast<uint64*>(pResult)[instanceIndex++];
                   }
               }
            }

            PAL_SAFE_FREE(pResult, m_pDevice->GetPlatform());

            // Output into .csv file.
            for (uint32 i = 0; i < m_numReportedPerfCounters; i++)
            {
                m_logFile.Printf("%llu, ", m_pGlobalPerfCounterValues[i]);
            }
        }
    }
    else
    {
        for (uint32 i = 0; i < m_numReportedPerfCounters; i++)
        {
            m_logFile.Printf(", ");
        }
    }
}

// =====================================================================================================================
// Dumps the SQ thread trace data from this experiment out to file.
void Queue::OutputSqThreadTraceToFile(
    const LogItem& logItem)
{
    const auto& settings = m_pDevice->ProfilerSettings();

    if (m_pDevice->GetProfilerMode() > GpuProfilerSqttOff)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 355
        if (HasValidGpaSample(&logItem, false))
#else
        if (HasValidGpaSample(&logItem, GpuUtil::GpaSampleType::Trace))
#endif
        {
            if (m_pDevice->GetProfilerMode() == GpuProfilerSqttRgp)
            {
                if (settings.gpuProfilerGranularity == GpuProfilerGranularityFrame)
                {
                    OutputRgpFile(*logItem.pGpaSession, logItem.gpaSampleId);
                    m_logFile.Printf("%u, ", m_curLogFrame);
                }
                else
                {
                    m_logFile.Printf("USE FRAME-GRANULARITY FOR RGP, ");
                }
            }
            else
            {
                void*  pResult     = nullptr;
                void*  pResultBase = nullptr;
                size_t dataSize    = 0;
                Result result      = logItem.pGpaSession->GetResults(logItem.gpaSampleId,
                                                                     &dataSize,
                                                                     pResult);
                PAL_ASSERT(dataSize != 0);

                if (result == Result::Success)
                {
                    pResult = static_cast<void*>(PAL_MALLOC(dataSize, m_pDevice->GetPlatform(), AllocInternal));
                    if (pResult == nullptr)
                    {
                        result = Result::ErrorOutOfMemory;
                    }
                    else
                    {
                        pResultBase = pResult;
                    }
                }

                if (result == Result::Success)
                {
                    result = logItem.pGpaSession->GetResults(logItem.gpaSampleId,
                                                                     &dataSize,
                                                                     pResult);
                }

                // Below crack open the .rgp blob in the GpuProfiler to translate it to ThreadTraceView format
                if (result == Result::Success)
                {
                    SqttFileHeader* pFileHeader = static_cast<SqttFileHeader*>(pResult);
                    pResult = Util::VoidPtrInc(pResult, pFileHeader->chunkOffset);

                    SqttFileChunkCpuInfo* pCpuInfo = static_cast<SqttFileChunkCpuInfo*>(pResult);
                    pResult = Util::VoidPtrInc(pResult, pCpuInfo->header.sizeInBytes);

                    SqttFileChunkAsicInfo* pGpuInfo = static_cast<SqttFileChunkAsicInfo*>(pResult);
                    pResult = Util::VoidPtrInc(pResult, pGpuInfo->header.sizeInBytes);

                    SqttFileChunkApiInfo* pApiInfo = static_cast<SqttFileChunkApiInfo*>(pResult);
                    pResult = Util::VoidPtrInc(pResult, pApiInfo->header.sizeInBytes);

                    for (uint32 i = 0; i < m_shaderEngineCount; i++)
                    {
                        SqttFileChunkSqttDesc* pDesc = static_cast<SqttFileChunkSqttDesc*>(pResult);
                        pResult = Util::VoidPtrInc(pResult, pDesc->header.sizeInBytes);

                        SqttFileChunkSqttData* pData = static_cast<SqttFileChunkSqttData*>(pResult);
                        pResult = Util::VoidPtrInc(pResult, sizeof(*pData));

                        File logFile;
                        const uint32 shaderEngine = pDesc->shaderEngineIndex;
                        const uint32 computeUnit = pDesc->v1.computeUnitIndex;
                        OpenSqttFile(shaderEngine, computeUnit, m_curLogSqttIdx, &logFile, logItem);

                        // The ThreadTraceView app expects the raw data to be dumped with one 16-bit hex per line.
                        const uint32 tokenCount = pData->size / sizeof(uint16);

                        uint16* pRawData = static_cast<uint16*>(pResult);
                        for (uint32 j = 0; j < tokenCount; j++)
                        {
                            logFile.Printf("%04x\n", *pRawData++);
                        }

                        logFile.Close();

                        pResult = Util::VoidPtrInc(pResult, pData->size);
                    }

                    m_logFile.Printf("%u, ", m_curLogSqttIdx++);
                }

                PAL_SAFE_FREE(pResultBase, m_pDevice->GetPlatform());
            }
        }
        else if (logItem.errors.perfExpOutOfMemory != 0)
        {
            // TODO: this error is set under none case yet.
            // GpaSession::BeginSample hits an ASSERT if this error happens.
            m_logFile.Printf("ERROR: OUT OF MEMORY, ");
        }
        else if (logItem.errors.perfExpUnsupported != 0)
        {
            m_logFile.Printf("ERROR: THREAD TRACE UNSUPPORTED, ");
        }
        else
        {
            m_logFile.Printf(", ");
        }
    }
}

} // GpuProfiler
} // Pal
